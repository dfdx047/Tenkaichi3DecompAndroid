# Notes for deterministic netplay

The end goal is a PC port with new online play. This collects what the decompilation has shown
that bears on keeping two machines in sync. **Verified** means confirmed by C that compiles to
the original bytes; **inferred** means read from disassembly or deduced.

## The simulation step

- Fixed 30 Hz step: every loop ends a frame with a two-vblank wait, timers count
  `seconds * 30`, and the battle clock advances 100 ms per three ticks. (verified)
- One battle frame is `Battle_Loop`'s body (systems/battle.md). Input is sampled once per
  fighter in `BtlChars_SampleInput`; the simulation is the AI update, the fighter phases, the
  effect scene, the stage (including rigid-body debris), the cameras and the sequence; drawing
  follows. (verified order)
- Fighters are updated pass by pass, fighter 0 first in each pass; the hit-stop loop has an
  early exit that favours roster order. A port must keep the order. (verified)
- A fight starts from a clean state: `BtlChar_ResetAll` zeroes both fighters and rebuilds them
  from the setup, at init, on restart, and again on the first Ready/Fight frame. (verified)
- Pause does not stop the loop: it is a flag each subsystem checks. Both peers must agree on
  it. (verified)

## Input: what to synchronise

- Fights use a per-fighter record, not the menu input word. (verified, `btl_input.c`)
- **The game's own replay records exactly `{buttons, stickX, stickY}` per fighter per
  input-taking frame**, after key config and double-tap detection, before the ring and the
  gating. The command word is recomputed. This is the minimal input a peer needs.
  (verified, `btl_replay.c`)
- The 8-entry ring per fighter never delays (pushed and popped in the same call; its public
  wrappers have no callers). It is the natural place to add input delay. (verified)
- CPU input is injected through fields on the fighter; the same path could carry remote input.
  CPU fighters are recorded in replays as inputs. (verified)
- Key config is applied before recording, so peers with different layouts exchange the same
  button word. (verified)
- A track index is not a frame number: nothing is recorded on paused frames or when the
  fighter is not taking input. (verified)
- Gating after the record (neutral or masked input in non-fight states) depends on battle and
  fighter flags, which must therefore be in sync. (verified)
- Menus read `gPad` fields directly from about 110 functions. (counted from disassembly)

## The replay system as evidence

A replay is the 0x5A8-byte setup plus two 9000-frame input tracks and nothing else: no seed,
no frame counter, no result. (verified) It reproduces a fight because:

1. the fighters' own generator and the frame counter both reset with the fighters at fight
   start (verified);
2. the CPU's decisions are captured as input (verified);
3. everything else that is random was judged not to matter (inferred).

A replay is therefore a ready-made desync test for a port, with one known weakness: paths
driven by `rand()` are not reproduced, so a replayed double KO can resolve differently from
the original. (inferred from the verified code)

## Sources of randomness

| Generator | State | Seeded / reset | Used by |
|---|---|---|---|
| `BtlChar_Rand` | roster +0x18 | zeroed by `BtlChar_ResetAll` | clash C only (9 sites): stage path, point on it, exchanges before moving. Feeds fighter placement. Plus two float users |
| `BtlChar_FrameMod` | roster frame counter | zeroed by `BtlChar_ResetAll` | fighter picks, e.g. voice lines (39 sites) |
| `BtlScene_Rand` | scene +randState | zeroed by `BtlScene_Reset` (battle start, restart, and some mid-battle sites) | effect scene |
| libc `rand()` | C library | **boot only**, from a hardware timer | about 490 direct call sites, almost all effect tasks; camera shake; the battle sequence's voice choice and **double-KO tie-break**; `Rand_IntRange` (34 sites) |
| VU0 R register (`Rand_Float01`, `Rand_FloatRange`) | vector unit | **boot only**, from a constant | effect code (about 160 sites) |
| `Rand_*` (broken-refill MT19937) | 624 words + index | **boot only**, from `rand()` | the AI (49 sites), the menus (about 198 sites), a few others |
| second MT19937 at 0x252F68 | own state | per call | a menu codec only |

(Generators and reset points verified; "boot only" is from a grep of every caller; the
per-generator user lists are by address range, not traced call by call.)

Consequences:
- The first three reset themselves; both peers only need to reach the first Ready frame on
  the same frame.
- **Draw code consumes libc `rand()`**: the depth haze (`StgHaze_Step`, called from a draw
  pass) draws 2 values per mesh vertex per view, about 2000 per drawn frame, on stages with
  that feature; the count depends on split screen, the demo camera and pause. A port must give
  it its own generator. (verified)
- More draws from shared generators by non-simulation code (verified): stage ambience sound
  draws libc `rand()` on some stages; screen shock waves draw 20 values from the VU0 register
  per spawn; visual particle modules draw libc `rand()` in proportion to live particles; water
  trails only outside split screen. See systems/effects_stage.md.
- **libc `rand()` must be synchronised**: it reaches the result through the double-KO
  tie-break, and its call count depends on camera shake and effects.
- **The VU0 register and the Mersenne Twister must be synchronised** if anything that uses
  them can affect the simulation: effects (which can hit) and any CPU-controlled fighter.
- The twister is shared with the menus; nothing outside the simulation may draw from it
  during a fight, or the AI needs its own stream.
- The roster's "time stopped" word (+0x274) freezes the two fighter sources, the scene
  generator and input. Its writer is `BtlChange_Update` (see the load-completion row below).
- Generators that reach projectile paths (verified, eft_r / eft_det_a / eft_l_b): the fighter
  generator (ki blast launch jitter, 3 per shot; deflect and reflect direction, 2 each), the
  VU0 register (bomb throw direction, 3 per bomb), and the scene generator (volley spread,
  ring-shot placement). **The VU0 register is therefore simulation state**, and it is also
  drawn by purely visual particle modules at pool-dependent rates: a port must separate them.
- Effect modules draw from the fighter generator too: `BtlCharApi_GetDeflectDir` (two
  `BtlChar_RandF` per call, callers 0x1764E8 and 0x178630). So its sequence depends on effect
  update order. (verified)
- The fighter core itself (actions, movement, hits, collision, members, stats, flags) calls no
  generator other than `BtlChar_FrameMod` and, in clash C, `BtlChar_Rand`. (verified)

## Non-simulation state that reaches the simulation

Each of these is a way two peers could diverge with identical inputs.

| What | How it reaches the simulation | Status |
|---|---|---|
| **Load completion** | A transformation, fusion or member switch pushes a character-change request; **time is stopped (roster +0x274) on every frame that ends with a request active**, i.e. for as long as the model takes to load. Fighter generators, the frame counter, gauges and input are frozen meanwhile. The change itself is applied in `BtlChars_OnModelLoaded` when the load job ends; battle flags 0x800 / 0x1000 / 0x2000 also suspend updates while loads run | verified |
| **Load completion** | The self-destruct technique pushes a character-change request and waits on `BtlChange_IsLoadedFor` before firing; the partner object (second model for fusion / team techniques) exists only once its load job ends and handlers branch on it | verified |
| **Load completion** | The AI skips any frame on which an object load job is running | verified |
| **Controller removal** | `PadWatch` debounces pad presence; the battle pause check sets the pause flag when a required pad is missing (not in mode 7) | watcher verified; pause path read from disassembly |
| **Voice playback** | The battle sequence waits on `Voice_IsStopped` (with a 10 s timeout) in the intro and win talk; story scripts wait on voices too | verified |
| **Per-player camera option** | One of the per-side options from the save gates camera shake, and shake calls `rand()` five times per frame while active | gating verified; option source inferred |
| **Screen mode** | `Battle_IsSplitScreen()` changes the lock-on camera pose; the pose can reach fighter state through `cam->side` on certain cuts | pose dependence verified; consequence inferred, medium confidence |
| **Which side is human** | `BtlCam_GetDefaultView` depends on side control and feeds an "is this camera on screen" test used by the effect scene | verified code; consequence not traced |
| **Replay viewer** | Pad 0 picks the watched side during playback; it writes nothing in the simulation but feeds the same default-view test | verified |

A port has to make each of these identical on both peers, or remove the dependency (for
example by loading models ahead of time and resolving loads on a fixed frame).

## Fighter order (verified unless marked)

- The collision step tests and applies fighter 0 first; applying fighter 0's hit writes
  fighter 1's reaction and health before fighter 1's own hit is applied.
- The hit-stop loop's early exit favours roster order.
- `BtlChar_PlaceOnPath` places by player index.
- Movement, push-out and most cross-fighter reads are order-independent: they use per-pass
  position snapshots and last-frame flag and action values.
- (inferred) Effect hits resolve in the order of the effect scene's record list.

Both peers must agree on who is fighter 0.

## Story battles (verified)

Scripts wait on non-simulation state: the voice stream (`Talk`, `PlayVoice`, line triggers),
the music stream, and a raw pad-0 button wait. `Talk` also starts and stops lip movement (a
fighter object sub-state) from the stream's status. Online story battles would need fixed
durations in place of those waits. Versus modes run no scripts.

## Faces and voice language (verified, bobj_b)

Every battle object with a face draws libc `rand()` for blinks and talk patterns during
`BtlObj_UpdateAll` (unpaused frames). Which lip tracks play depends on a save-data flag
(inferred: voice language), so two peers with different settings consume `rand()` differently.
Faces need their own generator, or the setting must be synchronised.

## Simulation inside draw callbacks (verified)

`EftTechEvtTask_Draw`, a draw callback of the effect scene, steps the technique fire and end
timers and sets fighter flag 0xA7 (the flag that ends a technique's charge loop). It is guarded
to run once per frame, but a frame that does not call `BtlScene_Draw` does not advance it. A
headless or rollback build must run that step from the update. See systems/effects_stage.md.

## Camera

- Movement is relative to the fighter camera's `yaw` (fighter +0x4A0), not to the camera
  position. `yaw` depends only on opponent direction, fighter flags, the fighter's input record
  and facing. The movement code reads no other camera field. (verified on both sides:
  `btl_char_cam*.c` and `btl_char_move.c`)
- The camera's `side` value (+0x4A8) chooses between two camera cuts for some attacks
  (`BtlAct_PrepareAttack`). Cuts raise fighter flags and have their own durations, so `side`
  must be treated as simulation state. Whether an attack's two variants actually differ is in
  a data file and not checked. (verified code; consequence open)
- No pad is read by the fighter camera or by the battle camera module. The two pad-reading
  camera functions found earlier are dead debug code and a viewer screen. (verified code;
  reachability from the absence of callers)
- Camera cuts raise fighter flags, and the demo camera advances inside
  `BtlCam_UpdateOverride`, which the battle sequence waits on. Camera updates therefore cannot
  be skipped on re-simulated frames. (verified)

## Floating point

All game maths is single-precision. Three things need exact reproduction:

- `Mathf_WrapAngle` (behind `Mathf_Sin` / `Mathf_Cos`, about 180 call sites) pushes every
  angle below 2 pi up by 2 pi and brings it back, which quantises small angles. Skip it for
  in-range angles and results change. (verified code; quantisation inferred)
- `Mathf_SinFast` / `Mathf_CosFast` are a polynomial evaluated on the vector unit, and return
  different bits from the libm-based pair. (verified)
- The PS2's FPU and vector unit do not follow IEEE exactly. Two PCs running the same build
  agree with each other; matching PS2 results bit-for-bit (for replays recorded on a PS2, or
  cross-play) is a separate, harder problem.

## Display offset in positions (verified)

`BtlCharApi_GetPos`, used by the AI and by effects, returns the fighter position plus the
display offset (hover bob and camera-independent shake at pose +0x20). That offset is therefore
simulation input wherever those callers act on it.

## State to save for rollback (inferred from the verified structure)

The 0x280 roster, the two 0x1600 fighters and the two per-side arrays; the battle objects each
fighter drives (pose is copied both ways several times per frame); `gBattleWork`; the sequence
block; the effect scene and its tasks; the stage's rigid bodies; the AI block; the script
tasks in story battles; and the state of every generator in the table above.

## Open items

- Character-change loads: the fighter side waits only on `BtlChange_IsLoadedFor(player)` (see
  combat.md, "Character changes"). Making that true a fixed number of frames after the push
  (with the models preloaded) removes the dependency without touching the handlers. The same
  call gates the KO member switch (0xF6) and the self-destruct technique.
- Check in the attack and cut data whether left / right cut variants differ in flags or length.
  Readers of the camera `side` are now all known: `BtlAct_PrepareAttack` and actions 0x41, 0x45,
  0x46 and 0xBE; each only chooses between two cut ids.
- Decompile the pause check `func_0022F9F8` and confirm the controller-removal path.
- Decompile the fighter state machine, movement and hit detection (the bulk of the
  simulation), and the effect tasks that call `rand()`.
- The Wii build has online play; its game-side netcode has not been examined and would show
  what the developers themselves synchronised.

## HUD consumes libc rand() (found 2026-10-04)

The gauge part of the HUD (`HudGauge_UpdateHpTrail`, `HudGauge_UpdateAura`; src/battle/hud_b.c)
draws from libc `rand()` every unpaused frame: at least 30 draws per side for the aura sparks
plus 2 per shaking node. It runs from `Hud_Draw`. Since libc `rand()` also reaches simulation
code, the HUD is one more visual consumer that shifts a simulation stream; see
docs/systems/hud.md.

## Correction (2026-10-04): the "second MT19937 at 0x252F68"

It is not a generator: there are two instances (0x252F68 and 0x253ED8), each the key stream of
the character password codec (docs/systems/save_data.md). The state is seeded per call, read
and never twisted, and touches nothing shared. The only live draw in that range is
`ChrPass_Encode`, which takes two libc `rand()` values (menu only).

## The Wii build's online mode: first look (2026-10-06, from the text in its main.dol only)

Read from the readable strings of `sys/main.dol` of the USA Wii disc (no symbols; nothing disassembled yet).

- Networking is Nintendo's DWC library (Nintendo Wi-Fi Connection) on GameSpy's services: login, friend lists
  (GP), matchmaking ("ConnectToAnybody", "ConnectToFriends", server browsing), and GT2 for the connection between
  the two consoles, with "SendUnreliable" present: the consoles talk to each other directly; the servers only
  introduce them. Save file of the mode: `nocopy/DBZT3_WIFI`.
- Screens, from the names of the menu's objects (`mc_*` / `fl_*`): a Wi-Fi menu with a guide character; a friend
  list with add / delete, entry of a friend code on an on-screen keypad and a "my code" page; a friend match
  lobby; a matchmaking screen with both players' names, win counts and battle points and a "revenge" (rematch)
  marker; time counters on the selection screens; choice of map and music; a VS screen with a countdown; a
  ranking list; a results list with win percentages; and the "ability limit" / item cost fields of custom
  characters among the same objects.
- NOT known from this: what the two consoles exchange during a fight (inputs only or more), whether there is an
  input delay, and what happens on a mismatch or a lost connection. That needs the callers of the library's send
  and receive functions found in the PowerPC code.

## The PS2 main menu's hidden entry: "Dragon Net Battle" (2026-10-06)

- `MainMenu_Init` (src/menu/menu_a.c) lists items 0..10 and skips item 4 (`if (i == 4) continue;`); every other
  item has a mode (menu_overlay.md), item 4 has none. Listed with `BT3_MENU_ITEM4=1` (port only), it appears
  between Duel and Dragon World Tour as **"Dragon Net Battle"**, with its plate, label and icon from the PS2
  disc's own menu pictures (seen in a screenshot).
- The user's check in the running game: the guide speaks a line when it is highlighted; confirming it plays the
  confirm sound and nothing else happens (no hang).
- Plan: this is the entry point of online play. Work on it happens on the branch `netplay`.

## What is left of Dragon Net Battle on the PS2 disc, and the Wii's screens (2026-10-06)

Two read-only investigations (by subagents; the points marked "checked" I looked at myself).

**PS2 leftovers**
- `MainMenu_Input` (src/menu/menu_a.c) has `case 4: break;` (checked): no mode is chosen, the code falls through to
  the "fl_ok" plate animation and the confirm sound. No mode number is reserved for it; nothing network-related
  is in either program (no strings, no modules, no uncalled functions that fit).
- Main-menu pack `pzs3us1/00449.bin`: the four guides' lines for item 4 are message lines 32..35 ("You can compete
  with players from around the world. That's amazing." and three more), voice files 0x8719..0x871C; the label
  "Dragon Net Battle" is row 4 of two 512 x 256 sheets (off / on), the icon a spaceship, still and animated.
- `pzs3us1/00478.bin` (baseFile + 0x1E, which no loader in the source reads): a complete 44-line guide script of
  the online mode with Pan and Giru. Lines 0..5 are a "not available" scene ("Whoops, the spaceship's run out of
  energy... Return to the Main menu and have fun with a different mode, okay?"), lines 6..43 the online mode's
  own lines (Wi-Fi data, Friend Code, friend list, ranking, sending fighters and replays, searching).
- None of the Wii's online screens (movies, pictures) is on the PS2 disc.

**The Wii's online mode** (USA disc, `wzs3us1.afs` entry 461 "DragonNe...", three BPE-compressed sections; the
same pack / movie / text formats as the PS2 with the byte order swapped; pictures decoded: checked on two sheets)
- Top: Nintendo WFC Battle / Manage Friends / User Settings. Guides: Pan and Giru.
- Battle menu: Custom Battle (anyone, custom characters allowed) / Normal Battle (anyone, normal characters only)
  / Friend Battle / Ranking Battle / View Ranking / Battle Record. No other rule options exist (no time, rounds
  or handicap text).
- Matchmaking: "Search For Opponent" / "Search From Limited Opponents"; two player plates with name, Fighting
  Points, battles / wins / losses and Connection Errors; a countdown.
- Setup: the ordinary versus screens (character reel with DP totals, Normal / Custom 1-3, colour, Map Select, BGM
  Select, VS) with a two-digit countdown. Afterwards: win plate, points up or down, Rematch Request / End Battle.
- Friends: roster (battles / wins / losses per friend), enter a 12-digit Friend Code, show one's own. User
  Settings: a player name (on-screen keyboard), initialise the mode's data. Ranking: My Area / Top 10. Battle
  Record: per mode, wins, losses, consecutive wins, connection errors.
- Messages include "Searching for opponent...", "Opponent found!", "Waiting for opponent's input...", "Match
  interrupted.", "No activity for 60 seconds".
- The screen order and what the countdown does are inferred; main.dol was not analysed (how a fight is kept in
  sync is still unknown). The decoded sheets and the text are kept outside the repositories (game data).

## Stage 1: the state checksum (2026-10-06, branch netplay)

- `port/src/gs/state.c`: `BT3_HASH=<file>` writes one checksum per vertical blank (XXH3 over the game's global
  variables, its heap and the scratchpad); `BT3_HASH_AT=<n>` adds, at that blank, a checksum per 4 KB page and a
  dump of the regions. `port/tools/compare_hash.py` finds the first differing blank of two logs and, from two
  dumps, the differing bytes with the variable they are in. Which addresses are the game's own globals comes from
  the linker's map (`port/tools/make_state.py` -> `<program>.mem`, run at link time): about 440 KB in 5 ranges.
- Checksums are comparable between runs of the SAME program only: the state holds addresses of functions and
  variables, which differ between builds. Comparing a Linux and a Windows machine needs something else (a
  checksum of chosen values, or addresses normalised): open.
- **First findings, both fixed:**
  1. `Port_LowAlloc` on 64-bit Linux took its blocks from wherever mmap put them; the addresses end up in the
     game's memory (file handles, sound buffers: gSndRpcBuf, gFileReq, heap), so two runs differed from the third
     blank on. Now one region at a fixed address, blocks handed out in order and reused by size.
  2. That region (first at 0x30000000) and the game thread's stack (0x60000000) were at fixed addresses ABOVE the
     program, where the C library's heap starts at a random place within a gigabyte: the region was taken in
     about a third of the windowed runs, and the stack's address must have been taken now and then too (the
     program then stops at start with "cannot reserve": not seen reported, a few percent by the arithmetic).
     Both are below the program now (stack 0x02000000, region 0x13000000). This second fix belongs on the main
     line as well.
- **Result:** the same program, the same input, the same settings: 3 headless runs of the replay fight identical
  for 13,378 blanks; 10 windowed runs of session5 (menus and a split-screen fight, played from the pad recording,
  `BT3_MENU_ITEM4=0`) identical for 6,973 blanks and 5 for 12,956.
- **Differences still to look at** (same recording): sound on against `BT3_NOSOUND=1` differs from blank 1477;
  4:3 against 16:9 from blank 2112. Not yet compared: the frame limiter on against off, the render thread, the
  32-bit program, the Windows program.
- **A fight checksum** (the user's point: two players' machines legitimately differ in everything that belongs to
  the view, so what has to match is the fight): `BT3_HASH` lines carry, during a fight, a checksum of both
  fighters' health and position, the battle clock and the C library generator's state, and the values
  themselves; `compare_hash.py a b --fight` compares only those. session5's split-screen fight (about 9,500 blanks;
  nobody is hit in it, the fighters move a little, 4,700 clock ticks): identical between 4:3, 16:9 and 21:9 and
  between sound on and off, the generator's state included. To come: a fight with hits, one view against two
  views, a stage with the haze effect (which draws from the same generator, see above), ki and the fighters'
  action states in the checksum.
- **session6** (the user's recording with hits: health 40000 / 30000 -> 37420 / 16770 in 29 steps, split screen,
  played with `BT3_MENU_ITEM4=0 BT3_NOMOVIE=1`): the fight values are identical for about 16,000 blanks of fighting
  between 4:3, 16:9 and 21:9 and between sound on and off.
- **Open: the checksum of everything is not always the same between runs with the same settings.** Seen on
  session6: the runs fall into groups. Runs made up to about 22:49 agree with each other (one of them leaves the
  others at blank 13,518); runs made from about 22:55 agree with each other and differ from the first group from
  blank 4,658 on. The fight values are identical in all of them. Ruled out: the save file (unchanged, and a fresh
  copy per run changes nothing), a busy against an idle machine (same result), the controllers' state (constant
  in the port), a clock (nothing reads one). Not found: what changed between the groups. To do: keep a memory
  dump at blank 4,658 of every run so that the next time two groups appear they can be compared byte by byte.
  This matters for rollback (the whole memory is restored and re-run), not for the comparison between players.

## Stage 2: saving and restoring the state, first version (2026-10-06, branch netplay, 64-bit Linux only)

- `port/src/gs/state.c`: a snapshot is the checksum's regions, the port's own state memory (`Port_StateExtra`: the
  blocks of `Port_LowAlloc` and its allocator), the used part of the game thread's stack and the registers
  (`getcontext`); restoring copies it back from another stack and continues at the save point (`setcontext`).
  Which of the port's own files count as state is in `port/tools/make_state.py` (`PORT_STATE`: headless, the
  memory / file / memory card / system layers, the float and vector code); about 570 KB of variables now.
- `BT3_SYNCTEST=1`: every vertical blank the frame is run, rewound to the saved state and run again, and the two
  results compared, with the differing places printed. The mechanism works: frames are re-run from the saved
  state. (The first try stopped at once with "battle finished": the port's own counter of battles was not in
  the state and counted the re-run as a second battle. Hence PORT_STATE.)
- **What it shows on the replay fight, from the third blank: the port's file and sound layers cannot be rewound.**
  1. Files (`plat_file.c`): a handle given to the game holds the host's `FILE *`, and a read continues from the
     host file's own position. Re-running a frame opens the file again (another `FILE *`, the first one lost) or
     reads on from where the first run stopped (the heap then holds zeros where the first run had data), and a
     file closed in the first run is closed again: the test ends in an abort after five blanks.
  2. Sound (`gs/snd_adx.c`): the players handed to the game are slots of the sound code's own table, which is not
     part of the state: the re-run frame gets the next slot (`gAdxPlayerTbl` differs).
- Next: (a) a file handle that holds only what the game may see (which file, size, position) with the host's files
  kept beside it and every read positioned by the handle; (b) the sound code's bookkeeping that the game can see
  (which players exist, what they report) moved into the state, the audio itself left outside; then the memory
  card layer; then the test again until a whole fight passes.

## Stage 2, continued: the port's layers made rewindable; the test passes a whole session (2026-10-07)

Changes, each found by `BT3_SYNCTEST` and each leaving normal play as it was (the replay's result on all three
programs, and session6's fight values, are unchanged):
- **Files** (`plat_file.c`, new `plat_fcache.c`): a handle holds the file's relative path, size, position and
  status and nothing of the host; the host's `FILE`s are in a cache outside the state (32 files, least recently
  used closed), every read positioned from the handle.
- **Memory card** (`plat_mc.c`): the same split (open / mode / path / position are state; the host `FILE` beside
  it is reopened when it does not fit the state). The write path was not exercised by any test here (saving a
  game: to be tried by hand).
- **Stream players** (`gs/snd_adx.c`, new `plat_sndstate.c`): how many players exist is state. With
  `BT3_SYNCTEST` or `BT3_SOUND_TICKS=1` what a player reports (playing / played to the end) is counted in vertical
  blanks from the stream's length instead of taken from the sound device: the same on every machine. Normal play
  keeps the device's answer. On session6 without a sound device the two give the same fight values.
- **`PORT_HOST`** (`port_host.h`): a variable of a state file that is not state (host file objects, the
  process's arguments) goes into the section `.porthst`, which `make_state.py` leaves out.
- **`Port_LowAlloc` clears new blocks**: after a restore the memory above the restored end of its region still
  held the undone frames' blocks.
- **The thread library's data at the top of the game thread's stack is not part of a snapshot** (the control
  block, thread-local variables, the C library's per-thread memory cache): restoring them corrupted the C
  library's heap (crashes at random places).
- Test harness: a pad recording opened during the re-run frame is rewound to its start.

**Result (64-bit Linux):**
- the replay fight without a window: 8,400+ frames each run twice from a saved state, 0 differences, the fight's
  result unchanged;
- session6 with the window (menus, loading, the split-screen fight with hits): 25,200 frames each run twice, 0
  differences; and against a normal run of the same session (15,504 blanks in both) the checksum of everything
  is the same at every blank.

Not covered: the render thread and sound effects (`gs/snd_se.c`) as far as the game can see them (no difference
showed, but with `BT3_NOSOUND=1`); the movie player; saving to the memory card; Windows and the 32-bit program
(the save / restore itself is written for 64-bit Linux only); rewinding more than one frame.

## Rewinding several blanks at a time; the run-to-run difference explained (2026-10-07)

- `BT3_SYNCTEST_DEPTH=<n>` (up to 64): save, run n blanks noting each one's checksum, restore, run the n again and
  compare each: what online play does when an input arrives late. Replay fight without a window, 8 and 20 blanks
  at a time: 15,000+ blanks each run twice, 0 differences, the fight's result unchanged. session6 with the window,
  8 and 20 at a time: 14,400 and 15,000 blanks each run twice, 0 differences, and against a normal run of the
  same program the checksum of everything is the same at every blank (14,714 and 15,032 compared).
- **The "open" run-to-run difference above (groups of runs differing from blank 4,658, one from 13,518) is
  explained: it was the test, not the game.** session6's recording ends at about blank 4,656. From there on a
  playback read the REAL keyboard and controllers, so runs differed by whatever a controller reported at the
  time (the groups by time of day: a controller awake or asleep; the one run at 13,518: an input event). The
  multi-blank rewind showed it: the stretch across the recording's end did not repeat, because the first pass
  had closed the recording. Now the pads are idle for the rest of a run once a recording has ended (what the
  comment always said), and the file stays open so a restored state can read its last part again.

## Stage 3: frames that are only re-run are neither seen nor heard (2026-10-07)

- `gPortResim` (gs/state.c): while it is set, the frame's list is still walked (it carries the texture uploads and
  vertex programs later frames count on) but no draw is recorded, nothing is shown, the picture's frame counter
  stands still, sound effects do not start, and a stream that is asked to start the file it is already playing
  plays on.
- `BT3_SYNCTEST` now runs the pass that gets undone in this mode and the second pass normally
  (`BT3_SYNCTEST_LOUD=1`: both with output, as before). session6 with the window, rewinding 1 and 8 blanks:
  13,200 and 16,200 blanks each run twice, 0 differences; the checksum of everything equals a normal run's at
  every blank (13,722 and 15,182 compared); and the picture at three fixed blanks (2600, 3401, 4000) is
  byte-identical to the normal run's. (Screenshots taken by frame number do not line up between the two runs:
  the runs count frames differently around loading. By blank they do.)
- Normal play: the flag is never set; the replay's result and session5's picture are unchanged.
- Not measured yet: what a silent frame costs (the list is still walked); sound with a device during rewinds
  (all runs had `BT3_NOSOUND=1`).

## Measured: what a re-run blank costs (2026-10-07, Ryzen 7 9800X3D, session6's split-screen fight)

`BT3_GS_VERBOSE=1` with `BT3_SYNCTEST`: per vertical blank, re-run without output 1.5 to 1.7 ms, with output 3.1
to 3.2 ms; saving the state 0.75 ms, restoring it 0.6 ms (about 30 MB each way). A fight frame is two blanks. So
undoing and re-running 4 frames costs about 13 ms here, and by the factor measured earlier about 2.2 times that on
the i5 12th gen: most of one 33 ms frame. Walking the frame's list is what a silent blank still pays for; to be
cut (skip the vertex work, keep the uploads) when rollback is in.

## Stage 4, first step: two copies in lockstep (2026-10-07)

- `port/src/gs/net.c`: `BT3_NET_HOST=<port>` / `BT3_NET_JOIN=<address>:<port>`; UDP; each copy has one local
  player (host = pad 1, joiner = pad 2); at every vertical blank a copy sends its player's input for that blank
  (the last 16 in every packet) and waits for the other's; the game's pad reads are answered from the exchange on
  both sides. `BT3_NET_DELAY` (default 2 blanks). Nothing predicted, nothing rewound. Both copies run the same
  game from the first blank (they connect before it). Test hooks: `BT3_PAD_TABLE` (an ordinary run writes its
  input by blank and pad), `BT3_NET_SCRIPT` (a copy plays its player's column of that), `BT3_NET_LOSS`,
  `BT3_NET_LAG`.
- Two copies on one machine (64-bit Linux, windows open, each with its own copy of the save folder), each playing
  one player's column of session6's input:
  | delay | packets dropped | extra lag | fight values of the two copies | final health |
  |---|---|---|---|---|
  | 0 | 0 | 0 | identical for 13,071 blanks of fighting | 37420 / 16770 |
  | 2 | 0 | 0 | identical for 11,297 | 37420 / 16770 |
  | 2 | 20% | 0 | identical for 11,353 | 37420 / 16770 |
  | 3 | 30% | 4 ms | identical for 15,720 | 37420 / 16770 |
  With delay 0 the host's fight values also equal the ordinary offline run's (9,794 blanks compared). The final
  health is the original session's in every case.
- The checksum of EVERYTHING differs between the two copies from blank 147 (where the memory card is read): the
  two had different save folders, and the path of an open card file is part of the state. Expected, not chased.
- Not done: real keyboards / controllers as the input (only the script), two machines, the Windows program (it
  builds), what happens when a copy is closed (the other stops after 15 s), a check of the two copies against
  each other while they run, both players' saves and settings (the copies had the same save).

## One view per window (2026-10-07)

- Online, both machines run the two-player battle with its two cameras (the split-screen game, the one tested
  above) and each SHOWS only its own player's view, full screen. The game has the mechanism already:
  `BtlCam_UpdateOverride` gives one view the whole screen while its camera "has priority" (close-ups). Under PORT
  the local player's view takes that place when the game gives none priority (`Port_NetView`, gs/net.c); when the
  game does, both players see that view, as in the split-screen game.
- `BT3_VIEW=0|1` does the same without a connection. session6 shown three ways (split, player 1's view, player
  2's view; 16:9): the fight values, the generator's state included, are identical for all 13,170 blanks of
  fighting. So what a machine shows does not reach the fight (on this session).
- Two connected copies (delay 2): each window shows its own fighter from behind, full screen, at the same blank;
  fight values identical for 15,693 blanks; the original session's final health.
- Still the split-screen game underneath: the stage model is the split-screen one (a different, presumably
  lighter file), and the effects that are per view are computed for both.

## The session flow behind "Dragon Net Battle" (2026-10-07)

- Lobby: the window behind the menu entry (ui.cpp) has working Host / Join buttons (`Port_Lobby*` in gs/net.c: a
  greeting and its answer over UDP). When the two have found each other, each side starts the program again as
  the session (`relaunch`: `BT3_NET_SESSION=1`, its role, `BT3_SAVES=net_session` emptied first,
  `BT3_SOUND_TICKS=1`), and the two connect again and run in lockstep from the first blank.
- The session opens on the versus mode's CHARACTER SELECT (mode 39), not on the versus menu: that menu (mode 38)
  leaves only three values behind (who plays, battle type, DP limit, at +0x620 / +0x624 / +0x630 of the progress
  record), which `__wrap_Progress_Main` sets itself (1P vs 2P, single battle). Everything is unlocked with the
  game's `Save_UnlockAll` on the default save, so both sides have the same roster. The start-up before that
  (logos, memory card check) runs without picture, sound or real-time pacing (`Port_NetWarp`). The user's check:
  the two windows come up in the character select.
- Leaving: back from the character select is the versus menu (the host can change the battle type); back from
  there the game enters the main menu, where `MainMenu_Run` ends the session (`Port_NetLeave`: the program
  starts once more as it normally is, with the player's own save). A player who does not answer for 15 seconds
  ends it the same way.
- Two session copies on one machine, no input: the checksum of everything is identical on both for all 2,715
  blanks run (after the frame limiter's clock values were taken out of the state: PORT_HOST).
- Not tried: the lobby window itself (the buttons were not clicked in a test: the session was started with the
  variables it sets), a fight from this flow with real controllers, leaving, the Windows program.
- The user's test of the whole flow by hand (two windows on one machine, 2026-10-07): Host / Join in the lobby,
  both restart into the character select, a fight that stays in step, and on leaving to the main menu both
  copies start again as the normal game. Works as built.

## Saving and restoring the state on Windows (2026-10-07)

- `gs/state.c` on Windows: the registers with the compiler's minimal `__builtin_setjmp` / `__builtin_longjmp`, the
  copy back on another stack through `Port_CallOnStack` (Linux keeps `getcontext` / `setcontext`).
- `plat_mem.c`: Windows now uses the same region allocator for `Port_LowAlloc` as Linux (address space reserved
  at 0x13000000 or, if that is taken, the first free place from 0x30000000; pages committed a megabyte at a time
  as it grows) instead of its own pools and single reservations: blocks are no longer given back to the system,
  which a restore to an earlier state needs. The region is taken once at the program's start on both systems
  (a state restored to before it existed took it a second time, elsewhere), and where it is counts as host data.
  The game's stack end is recorded on Windows too (`sGameStackTop`).
- `make_state.py`: two game ranges are no longer joined over a gap that holds a variable of a file that is not
  state (plat_settings.c's lock lay in one: on Windows its address was restored with the game).
- **Windows program under Wine** (`BT3_SYNCTEST=1 BT3_SYNCTEST_DEPTH=8`): the replay fight without a window,
  44,400 blanks each run twice, 0 differences, the fight's result unchanged; session6 with the window, 49,800
  blanks each run twice, 0 differences. Rewinding 1 at a time, the replay fight: 0 differences.
- Linux unchanged: session6 rewinding 8, 0 differences; the replay's result on all three programs; normal play's
  fight values as before.
- Not tried: a real Windows (only Wine); two Windows copies connected; Linux against Windows.
