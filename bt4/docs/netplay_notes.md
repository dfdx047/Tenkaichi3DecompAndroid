# Deterministic netplay reference

This reference records findings from the original BT3 decompilation that affect
deterministic simulation, input exchange and rollback. **Verified** means confirmed
by C that compiles to the original bytes; **inferred** means read from disassembly
or deduced.

For the implemented BT4 Tag Team protocol and its validation scope, see the
[BT4 compatibility profile](../BT4_PROFILE.md) and [room service guide](../cloudflare/README.md).

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
