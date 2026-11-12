# Battle

Sources: `src/battle/battle.c`, `btl_seq.c`, `battle_work.c`, `battle_load.c`, `btl_pool.c`. Layouts: `include/battle/*.h`.

`include/battle/battle.h` is the single description of the battle block (`BattleWork`,
`BattleSetup`, `BattleSide`, `BattleMember`, `BattleResult`, `BattleEvents`, the flag bits and
the mode / control / result constants). The loader jobs and the setup accessors are one file,
`battle_load.c`, because they were one object in the original: their read-only data only lines
up together.

## Entry (verified)

`Battle_Main` = `Battle_Init`, `Battle_Loop`, `Battle_Term`. `gBattleWork` is a static
0x1A00-byte block at 0x331DC8.

## One battle frame (verified)

1. If flag 0x8000: `Battle_Restart()`, clear the bit.
2. `Job_Run()`.
3. `Gfx_BeginFrame()`.
4. Unless flag 0x100: `Gsc_Update` (script engine), `BtlScript_Update` (story triggers).
5. `BtlChars_CheckStart` (resets the roster on the first Ready/Fight frame).
6. **`Pad_Update()`**: the only pad read of the frame.
7. `Snd_Update()`, `Snd_SendFighters()`.
8. `BtlGame_PreUpdate()` (HUD, then `BtlSeq_PreUpdate`).
9. `Battle_UpdateWork()`.
10. `BtlAiMgr_Update` (the CPU opponent), then `BtlChars_SampleInput` (battle input).
11. `Battle_Update()`:
    - fighter phases `BtlChars_UpdateInput`, `BtlChars_UpdateMain` (see fighter.md)
    - effect scene `BtlScene_Update`, then `func_001AF9C0`, then `BtlScene_PostUpdate`
    - fighter phase `BtlChars_PostScene`, then `BtlScene_CheckStageChange`
    - loader polls `BtlLoad_PollCharaRequest`, `BtlLoad_PollObjectRequest`
    - stage update `func_00243568`
    - cameras: select + update for views 0 and 1, always both
    - `singleView = BtlCam_UpdateOverride()`
    - final fighter phase `BtlChars_EndFrame`, then `BtlObj_UpdateAll`
    - visibility lists per view
    - returns `singleView == 0`
12. `done = BtlGame_Update()` (tail call of `BtlSeq_Update`).
13. Draw: `Battle_DrawSplit()` if split-screen and step 11 returned 1, else `Battle_Draw()`.
14. `Gfx_EndFrame(2)`.
15. `Dma_Flush()`. Loop while `!done`.

For a port or netplay: input is sampled at step 6, the simulation is steps 8 to 12, and state
reset is `Battle_Restart`.

## Flags (`gBattleWork + 0x19F0`, 64-bit)

| Bit | Meaning | Status |
|---|---|---|
| 0x100 | pause: each subsystem tests it itself (126 read sites); the loop keeps running | verified in the loop, pause meaning from the sequence code |
| 0x200 | set by every non-fight sequence state, cleared when fighters are released | verified |
| 0x400 | set only during the 2 s of the Ready state | verified |
| 0x800 | character load job running | verified |
| 0x1000 | object load job running | verified |
| 0x2000 | stage load jobs running; fighters, stage and cameras are not updated or drawn | verified |
| 0x4000 | pause menu open (set together with 0x100) | verified |
| 0x8000 | restart requested | verified |

## Sequence: how a match progresses (verified behaviour; state names are guesses)

`BtlSeq` (0x12C bytes at `gBtlSeq`): state, a 0x100-byte handler context (not cleared between
states; its first word is the state's poll callback), the state table, the battle clock, a
second clock, and an end-check-off flag. A table has 7 entries of
`{enter, preUpdate, update, exit, extra, poll}`.

| # | Name | What it does | Next |
|---|---|---|---|
| 0 | StageIntro | three stage camera cuts | 1 |
| 1 | IntroTalk | two voice lines, each ending when the voice stops or after 10 s | 2 |
| 2 | Ready | waits 0.8 s, shows announcement 0, waits 2.0 s; exit shows announcement 1 | 3 |
| 3 | Fight | pause logic, then `BtlSeq_CheckBattleEnd` | 4, or 6 on mode 7 / abort |
| 4 | Finish | waits 3.5 s and shows an announcement chosen by the end reason | 5 or 6 |
| 5 | WinTalk | winner's or loser's line, waits for the voice or 10 s, holds 1.5 s | 6 |
| 6 | End | in mode 0, runs the result menu; then fade out and wait 1.2 s | 99 |

State 99 is not a table entry: `BtlSeq_Update` calls the old state's exit and returns 0 after
setting the restart flag if a rematch was requested (reason bits 0x18000, or mode 6 with reason
bit 2), else 1 to leave the battle.

Tables by mode: mode 1 uses states 1-6; modes 5, 6, 7 use states 2, 3, 4, 6; everything else
uses all seven. All poll callbacks test pad bit 0x1000 (skip / pause).

### End of battle

`BtlSeq_CheckBattleEnd` (verified):

1. return 1 if a winner is already set;
2. return 0 without ticking if end checks are off, the battle is paused, stage jobs are running,
   or any character has flag 0x128;
3. tick both clocks;
4. return 0 in mode 1 (clocks run, no win check);
5. time up: reason 2, judge by health;
6. character flag 7 on both sides: reason 4, judge; on one side: the other side wins;
7. all characters of a side at health <= 0, on both sides: reason 1, judge; one side: the other
   wins;
8. a replay that has run out of recorded input: winner 0x10, reason 0x40000.

`BtlSeq_JudgeByHealth` (verified): the higher health wins. On equal health, in order: rule flag
0x3C of side 0 gives P1; of side 1 gives P2; a draw only when the reason is time-up and the mode
is 0; P2 in mode 8; otherwise `rand() & 1`.

Winner bits: 1 P1, 2 P2, 4 draw, 8 abort, 0x10. Reason bits: 1 KO, 2 time up, 4 "character flag
7", 0x18000 restart, 0x40000.

### Clock

`BtlClock`: ticks, hours, minutes, seconds, ms, time left. Each tick adds 34, 32, 34 ms in turn;
it stops at 9:59:59.999. The time limit compares `minutes * 60 + seconds` with the limit table
(hours ignored).

## Setup block (verified unless marked)

The first 0x5A8 bytes of `gBattleWork` are one unit, cleared, filled and copied whole.

| Offset | Field | Notes |
|---|---|---|
| 0x00 | `magic[4]` = "btls" | |
| 0x04 | `version` = 7 | |
| 0x08 | `mode` | 0..9, see below |
| 0x0C | music index | inferred (`Bgm_Play(0x10B16 + n)`) |
| 0x10 | time-limit index | table {none, 60, 90, 180, 240, 45} s |
| 0x14 | announcer 0..7 | "announcer voice" is inferred |
| 0x1C | starting stage (0..34) | |
| 0x24 | screen mode, 1 = split | |
| 0x28 | current stage | |
| 0x2C | 0x90-byte option block | filled from the save |
| 0xBC | script + 1 | script file `0x1FF + n` |
| 0xC0 | `sides[2]`, 0x270 each | |
| 0x5A8 | opponent pool: `{cur, count, members[50]}` | mode 3 |
| 0x1938 | result block, 0x48 bytes | |
| 0x1980 | event work, 0x70 bytes | |
| 0x19F0 | flags (u64) | |
| 0x19F8 | running | |

Side (0x270): member count at +0, up to five members of 0x64 bytes from +4, lead member index
at +0x1F8, controlling pad at +0x204, control type at +0x208 (0 = pad, 2 = CPU), a character
bit array at +0x210, the starting and current `{chara, costume, variant}` at +0x250 / +0x25C.

Member (0x64): `chara`, `costume`, `variant`, `cpuLevel` (0..29 or -1), `health` (float; 100.0
from the menus, percentage inferred), `items[8]` (16-bit, 1-based), `bonus[8]` (clamped to
-20..20, or -20..60), `aiType`, `ability[4]`, data pointers.

CPU difficulty: the five menu levels map to internal levels 0, 6, 13, 21, 29.

Setup sequence: `BattleSetup_Clear`, `SetRule`, `SetSide` twice, `SetMember` per member,
`Finish`. `BattleSetup_FixForMode` and `BattleSetup_FinishEx` then adjust the setup per mode,
fill the options from the save, and copy the setup into the replay block.

## Modes (setters verified; meanings inferred from callers)

| Mode | Set by | What it appears to be |
|---|---|---|
| 0 | versus menus | versus; split-screen possible; result menu at the end |
| 1 | script commands | scripted (story) battle: pad vs CPU; no win check in the fight state |
| 2 | three menu sites | pad vs CPU teams |
| 3 | two menu sites | one pad member vs a pool of up to 50 opponents |
| 4 | one menu site | one member per side; split-screen allowed |
| 5 | one menu site | no time limit, announcer 7, one member each |
| 6 | versus menu in one progress state | training-like: no time limit, dummy CPU (level -1) |
| 7 | main executable | attract demo: 9 fixed pairs, 45 s, CPU vs CPU |
| 8, 9 | no setter found | presumably only via a loaded replay; 9 becomes 6 with both sides CPU |

## Replay block (verified code; "replay" meaning inferred)

`gBattleReplay` at 0x301268, 0x1ABA8 bytes. The setup is copied into it at the end of setup; the
memory card code loads and saves it; input, camera and HUD code test whether it is active. A
block is accepted only if its size is 0x1ABA8 and its header is "btls" / 7. How inputs are
recorded into it is not decompiled yet.

## Loading (verified)

`Battle_Load` pushes `BtlLoad_StepInitial` on the job queue and runs the loading screen.

1. Optional script file.
2. Sound banks: 0x14A, the stage bank, one voice bank per side.
3. Allocate the stage buffer (0x6CB800), the bank buffer (0x90800), two 0x1000 blocks per
   member.
4. Request the stage model (a different file for split-screen) and each member's parameter file.
5. Relocate each member block.

Character models are loaded by a separate job (`BtlLoad_StepChara`) whenever a side's current
character differs from its starting one; this is also the path for mid-battle changes. Jobs come
from a pool of 8 (`gBtlJobPool`). File ids are in files_and_assets.md.

## Events (verified)

Event work at +0x1980: per side, 128-bit sets `now`, `prev` and `held`.
`BtlEvent_Raise(side, ev)` sets a bit; `BtlEvent_Update` (each unpaused frame) rotates now into
prev and accumulates held, and raises time events 0..18 when the second clock passes
{10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, 90, 100, 120, 140, 160, 180}.
`BtlEvent_IsNew` is a rising edge; `BtlEvent_WasRaised` reads held. Events 0x4C..0x4F drive
script interrupts. What most event ids mean is not known.

## Battle event script and its facade (verified code; meanings inferred)

A command interpreter at 0x259070..0x25BE58 (operand fetch 0x2585C8, a 30-way command switch at
0x25B5B8) runs battle scripts: story battles and scripted events. It is stepped each frame from
`Battle_Loop` (the calls gated by the pause flag) and it drives the battle only through
`src/battle/btl_facade.c`: 63 small wrappers (0x12BD58..0x12C9F0) for the scripted camera,
fighter position / movement / control flags, member health and two gauges, forced actions, the
sequence, the result block and script waits. Seven wrappers have no caller. The interpreter
itself is not decompiled.

## Effect scene (verified code; purposes inferred)

`src/battle/btl_scene.c` (0x12C9F0..0x12DD80), state `gBtlScene` (0x34 bytes). It is not a
general scene manager: it owns the task tree that the fighter-attached effect modules
(0x12DD80..0x1AE200) run in, plus their shared services:

- the `BtlPool` arenas (initialised and released only from here);
- a root task with a group of five "layer" tasks selected by a mask;
- a list of up to 64 blast records of 0x190 bytes, emptied each unpaused frame;
- a per-character table of rates and body scale (object +0xFF4 / 19.35);
- a private random generator: `(state * 714025 + 4096) % 150889`;
- the "is time stopped for this object" predicates the tasks call;
- the stage-change trigger.

Frame order: `BtlScene_Update`, `BtlScene_PostUpdate`, `BtlScene_CheckStageChange`,
`BtlScene_SetSingleView`, then `BtlScene_Draw(first)` per view. `BtlScene_Reset(mode)` resets
all tasks (0), also rebuilds the layers (1), or only one character's tasks (2, 3).

`BtlScene_CheckStageChange` runs while fighting, outside modes 4..7, with the time limit off or
at least 10: for the first active type-1 blast record, if a rule word is on and the opponent is
below half health, it requests a stage change (the stage-destruction transition, inferred).

The menu overlay also runs its own instance of this scene.

## Camera (verified code; all names are guesses)

Sources: `src/battle/btl_cam.c`, `btl_demo_cam.c`, `orbit_cam.c`.

- `gBtlCam` (0xC50): three `View` layout templates (full, left half, right half) and two
  `BtlCamView`s. A `View` (0x260) holds the world-to-view and projection matrices, the scissor,
  and projection parameters: aspect 7/6, screen distance 433, near 0.3, far 65536.
- The camera pose is not computed here. Each frame `BtlCam_UpdateView(i)` copies fighter i's own
  camera pose (`func_00207DD0`, fighter +0x430 / +0x440) into the view and builds the matrices.
  The fighter camera itself is maintained at 0x1C69C8 (not decompiled).
- `BtlCam_UpdateOverride` returns 1 when the frame is drawn once, full screen: always outside
  split-screen or during a replay; in split-screen only when one view has priority.
- Demo camera: `DemoCam_PlayStageAnim(0..2)` plays the stage-intro cuts from a camera animation
  file (channels of keys `{flags, time, value}`); it advances 2.0 per unpaused update, inside
  `BtlCam_UpdateOverride`. The battle sequence waits on `DemoCam_IsActive`.
- Camera shake calls the C library `rand()` five times per update while a shake is active.
- The camera module never writes to a fighter. Of about 140 readers of the views, none is in
  the fighter-logic address range except one that turns the view into sound volume and pan
  (classified by address only).

## Pause-menu skill list (verified code; names are guesses)

The start of `btl_seq.c` (0x215420..0x216AC0) draws the pause menu's skill list from a UTF-16
script: tags for page titles, entries with three icon digits, detail lines and notes. A line
can be gated on a character-unlock bit in the save and is drawn at half alpha when locked.

## Battle objects (verified; field meanings partly inferred)

Source: `src/battle/btl_obj.c`. Layout: `include/battle/btl_obj.h`.

- A battle object is one animated model. Up to 12 exist, 0x1670 bytes each, in one heap block
  of 0x6B740 bytes. An object id is its index, 0..11. `gBtlObjTbl[id]` is rebuilt every frame
  from the used list.
- A fighter object gets a 0x1A0C0-byte work buffer; there is room for two (the code allows a
  third, which would overlap the next pool).
- A model resource slot is three files: the model and two more (animations, inferred). A
  battle preallocates two fixed slots (model 0xCE000, 0x160800 and 0xCE800 bytes) and one
  spare model buffer.
- A character change (`BtlRes_Reload`) reads the new model into the spare buffer, then swaps
  it with the live one. Only one reload can be pending.
- `BattleSide + 0x26C` is the resource slot number; `+ 0x268` is the object id.
- Per frame `BtlObj_UpdateAll` (not while paused) runs animation and pose passes per object
  and rebuilds its world bounding box.
- Per view: an object is culled when all 8 box corners are past the same scissor edge or
  behind the camera; models can fade with distance (near 40, far 120 by default); a fighter
  whose body box contains the camera is moved to the later draw pass. The draw list is built
  in three passes, with the view's own fighter last.
- One light: direction, a derived half vector, colour; copied from the stage when ready.

## Stage rigid bodies (verified code; purpose inferred)

Source: `src/sys/rigid.c`. A sphere rigid-body integrator (explicit Euler, penalty contact,
friction, a rest test) stepped only by the battle stage update, 10 sub-steps per frame, not
while paused. The bodies look like stage objects or debris; no fighter code calls it. +Y is
down (gravity is +9.8 on Y).

## Not decompiled yet

The individual action handlers and technique code (0x1E3158..0x2129C8, in progress), the effect
tasks (0x12DD80..0x1AE200), the stage, and HUD internals. The fighter core is described in
fighter.md and combat.md. About 50 functions called from the frame loop are still unnamed, with a
first-read description in the header comment of `battle.c`.

## Pause / result menu (0x2129C8..0x215420; src/battle/hud_0.c, hud_0_b.c, hud_0_c.c, linked; names in config/symbols/hud_0.txt)
Linked layout: hud_0.c `.rodata` 0x2F19D0 (0x5C, two jump tables), hud_0_b.c `.rodata` 0x2F1A30 (0x14).

63 of 64 functions match; `BtlText_PutSprite` (0x215140) is INCLUDE_ASM, registers only. The
stem is a misnomer: nothing here is HUD. Final names: the first three functions join the
`BtlGame_*` group (`BtlGame_IsFighting` 0x2129F0, `BtlGame_IsReplay` 0x212A08); pause_menu.c
(`PauseMenu_*`, 0x212A10..0x213238: the battle's menu trees and callbacks); btl_menu.c
(`BtlMenu_*`, 0x213238..0x215010: a generic menu engine); 0x215010.. is the head of btl_seq.c's
`BtlText_*` part. Layouts in include/battle/hud_0.h.

Verified by matching C:
- Called from `BtlSeqFight_Update` (`PauseMenu_Update(pad, 0)` while `BATTLE_FLAG_PAUSE_MENU`)
  and `BtlSeqEnd_Update` (`PauseMenu_Update(pad, 1)`, the result menu); drawn from
  `BtlGame_Draw` after `Hud_Draw`.
- Menu trees are static data at 0x2C4E70..0x2C6070, chosen by battle mode and, in mode 0, by
  replay state; only modes 0 and 1 have a result menu.
- Pad (`gPad[pad].gamePressed`): up 8, down 4, cross 0x200 confirms, triangle 0x400 closes,
  start 0x1000 also closes the top-level pause menu; the skill list uses `gameRepeat`.
- **Writes to the simulation**: (1) a picked result item calls `BattleResult_Set(8, reason)`,
  one reason bit per item id (table in the source; inferred: 0x8000 rematch, 0x10000 watch
  replay); (2) closing the pause menu clears `BATTLE_FLAG_PAUSE` and resumes paused voices;
  (3) the mode 6 pause menu's CPU level sub menu calls `BtlAiMgr_SetLevel(1, level)` every
  frame it is open. Nothing else. For netplay / replays these are inputs outside the fighter
  pad stream.
- No random draw, clock or camera read. Opening / closing take 6 frames each.
- Original quirk: the code tests `pad` for -1 yet still indexes `gPad[pad]` with it.
