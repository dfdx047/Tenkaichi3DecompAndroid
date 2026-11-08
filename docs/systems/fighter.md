# Fighters

Sources: `src/battle/btl_char_mgr.c` (roster, reset, per-frame phases), `btl_char_get.c`
(accessors, fighter generator, vibration, voice), `btl_char_api.c` (by-object-id interface),
`btl_input.c` (input, see input.md), `btl_replay.c`, and the fighter camera in
`btl_char_cam.c`, `btl_char_cam_cut.c` and `btl_char_cam_modes.c` (one module, three files
because the unmatched cut evaluator owns constants in the middle of the float pool).

Each file declares its own partial view of the fighter object (`BtlMgrChr`, `BtlCharGetChr`,
`BtlCharApiChr`, `BtlInputChr`, `ChrCamChr`, ...). A single unified header does not exist yet;
the table below is the merged picture.

The action state machine, movement, hit detection, gauges and effects are NOT decompiled: the
phase order below is verified, but what each still-unnamed callee does is inferred.

## Roster `gBtlChars` (0x280 bytes, heap; verified)

| Offset | Field |
|---|---|
| 0x00 | count (2 in a battle) |
| 0x04 | fighters: count x 0x1600 bytes |
| 0x08, 0x0C | two per-side arrays of 0x34 bytes; +0xC holds four live sound handles per side |
| 0x10 | flags, bit 0 = fight started |
| 0x14 | frame counter (30 bits), ticked only when started and not time-stopped |
| 0x18 | fighter random generator state |
| 0x1C | stage timer, counts down |
| 0x20..0x34 | six table pointers into common file 2; `tbl[0]` is indexed by fighter +0x974 |
| 0x24 | (tbl[1]) the common camera cut table |
| 0x28 | (tbl[2]) the voice table `{s16 first, s16 count, f32 interval}` |
| 0xA0 | four positional sound requests, count at +0x120 |
| 0x130, 0x134 | replay viewer row (always 0) and watched side |
| 0x274 | "time stopped" word: freezes both simple generators, the frame counter and input; writer not found |

## Fighter object (0x1600 bytes)

Offsets verified by matching code in at least one file; meanings as marked.

| Offset | Type | Meaning | Status |
|---|---|---|---|
| 0x0000 | s32 | player: roster index, replay track index, key-config index | verified |
| 0x0004 | s32 | pad index | verified |
| 0x0008 | s32 | side | verified |
| 0x000C | s32 | battle object id | verified |
| 0x0010 | 0xF0 bytes | pose block; world position at +0x10, facing yaw at +0xA4 | verified |
| 0x0100 | 0xF0 bytes | last frame's pose | verified |
| 0x0420 | 0xA0 bytes | fighter camera (`ChrCam`): eye, pos +0x430, rot +0x440, target, shake +0x470, trace result, distMode +0x49C, **yaw +0x4A0**, side +0x4A8 | verified offsets |
| 0x04C0 | 0xB0 bytes | camera cut state | verified offsets |
| 0x0570 | 0x3D8 bytes | input block (`BtlCharInput`): raw sample, mask table +0x578, held/pressed/released +0x73C, ring +0x8C0, record +0x938 | verified |
| 0x0948 / 0x0950 | s32 | action id / previous action | verified |
| 0x0974 | s32 | state id indexing roster `tbl[0]` | verified use, meaning unknown |
| 0x0994 / 0x0998 | s32 | active member index / member count | verified |
| 0x09A4 | 5 x 0xA4 | member entries (below) | verified |
| 0x0D40, 0x0D44 | s32 | running maxima copied to the result block | verified |
| 0x0E5C, 0x0E60 | s32 | counters, full at 3 and 5 | verified |
| 0x0F30 | Vec4 | written by the opponent's post-scene pass | verified |
| 0x0F50 | 4 x 0x18 | status table owned by the next module | inferred |
| 0x1084 | u8 | per-frame stage number (1..12) | inferred |
| 0x1278 | s32 | input is injected (CPU) | verified |
| 0x127C..0x1284 | u32, f32, f32 | injected buttons, stick x, y | verified |
| 0x12D0.. | s32 | chara / costume of the form being changed into | verified |
| 0x1320..0x1328 | s32 | hit-stop: freeze counter, pending length, delay | verified |
| 0x1330 | s32 | a loaded resource to release | verified |
| 0x1370 / 0x1454 | s32[57] | last voice line / voice cooldown per kind | verified |
| 0x1538 | u64 | per-frame bit set | verified |
| 0x15D0..0x15E4 | | vibration: enabled, power, frames, small-motor frames, phase | verified |
| 0x15E8 | 3 pointers | tables inside the object's three files | verified |

Member entry (0xA4): chara, costume, present, `bonus[7]`, `ability[4]`, cpuLevel +0x38,
aiType +0x3C, gauge block at +0x40.

Gauge block: health / health max at +0 / +4 (10000 units per bar), two more gauges with maxima
at +0xC / +0x10 and +0x14 / +0x18 ("ki" and "blast" are guesses), +0x1C = 30000 together with
flag 6, low-health flags, a mode flag at +0x30.

## Lifecycle (verified)

- `BtlChar_AllocAll(2)` from `Battle_Init`; `BtlChar_FreeAll` from `Battle_Term`.
- `BtlChar_ResetAll` (init, restart, and the first Ready/Fight frame): zeroes every fighter and
  the roster's working areas, then per fighter binds the battle object and runs
  `BtlChar_Reset`. **Nothing in a fighter survives a restart.**
- `BtlChar_Reset` copies from the battle setup: member count, each member's chara, costume,
  cpuLevel, aiType, `bonus[1..7]`, `ability[0..3]`, health (through a parameter loader), the
  per-side camera options, whether the side is CPU (`+0x1278`), and the vibration setting from
  the save. It then resets input, places the fighter and rewinds the replay track.
  Not copied here: `BattleMember.items` and `bonus[0]`.

## One simulated frame (call order verified; callee purposes inferred)

Fighters are updated pass by pass, fighter 0 first within each pass. Each per-fighter body
returns immediately while the fighter is frozen (hit-stop).

1. **`BtlChars_CheckStart`** (before the pad read): when the sequence first reaches Ready or
   Fight, reset the whole roster once more (not in mode 1) and set "started".
2. **`BtlChars_SampleInput`** (skipped when paused or loading): `BtlInput_Sample` per fighter.
3. **`BtlChars_UpdateInput`** (skipped when paused): tick the frame counter; round-reset check
   (fighter flag 0xF9); hit-stop; per fighter save last pose, clear per-frame fields, tick
   voice timers, `BtlInput_Update`; set one held flag 1..4 from the sequence state.
4. **`BtlChars_UpdateMain`** (skipped when paused or loading), seven roster passes with global
   steps between: action state machine; push-out between fighters; movement; stage and ground
   queries; camera control; then gauges, hit detection and damage (or a reduced path while
   anyone is frozen); effect spawning.
5. (effect scene runs here, from `Battle_Update`)
6. **`BtlChars_PostScene`**: fighter camera pose (`ChrCam_Update`), low-health flags, and a
   vector written into the *opponent*.
7. **`BtlChars_EndFrame`**: late update, object flag update, queued sounds, then per fighter
   `BtlChar_RaiseEvents` and `BtlChar_UpdateVibration`.

## Hit-stop (verified)

Level per fighter: 2 with flag 0x125, 1 with flag 0x126. If any level is non-zero, every
fighter is scheduled to freeze except those at the highest level (and, at level 1, those with
flag 0x127). At level 2 the loop stops at the first exempt fighter, so roster order matters.
A frozen fighter skips every phase, including input sampling.

## Character changes (verified)

`BtlChars_OnModelLoaded(side)` runs when the character load job finishes, by current action:
- 0xEC..0xF0 transformation: new chara/costume, parameters reloaded, health keeps its ratio.
- 0xF1, 0xF2 fusion: bonuses summed and clamped to -20..40, abilities OR-ed, health and
  maximum summed, the partner marked absent.
- 0xF3..0xF8 member switch.
Parameter byte 0xAD adds +5000 / +10000 health or fills a gauge.

## Events raised per frame (verified)

`BtlChar_RaiseEvents` raises battle events from fighter state: 0x16..0x1C for health lost in
steps of 10000 (only the highest), 0x1D / 0x1E for full gauges, others keyed by action id,
technique id or flag edges. The full table is in the header comment of `btl_char_mgr.c`.
Under the fight state it also keeps `BattleResult.health[side]` as a team health percentage.

## Random numbers used by fighters (verified)

- `BtlChar_Rand`: roster+0x18 = `(state * 714025 + 4096) % 150889`, zeroed by `ResetAll`.
- `BtlChar_FrameMod(n)`: frame counter % n, used as a deterministic pick (39 call sites).
Both are frozen while roster+0x274 is set.

## Voice (verified)

57 voice kinds per fighter with a per-kind cooldown; the line is picked with `FrameMod`,
avoiding the last one. In mode 1, kinds 14..16 and 34 are suppressed.

## Fighter camera (matches per function, not linked; names are guesses)

`ChrCam` at fighter +0x420. Two phases per frame: control (cut start, demo triggers, yaw and
input) from the main update, and pose from the post-scene pass.

- Pose priority: frozen (flag 0xCC) > cut > fixed (0xDC/0xB8) > lock-on (flag 5) > free. Then
  shake, clamp to the stage radius, smoothing, keep-behind-head, stage trace.
- SELECT cycles three distance presets. In lock-on the yaw follows the opponent at 25% per
  frame and the right stick picks the side the camera sits on.
- **Movement is relative to `yaw` (+0x4A0)**, which depends only on opponent direction, fighter
  flags, the fighter's input record and facing. (Writers verified; the movement-side readers
  were read from disassembly.)
- No pad is read; all input comes from the fighter's input block.
- Cuts are 0x24-byte records (per character at object +0x938, common at roster +0x24) that
  orbit one model node and look at another; requested from 31 places in the fighter code.
- Non-simulation inputs: `Battle_IsSplitScreen()` changes the lock-on pose, and a per-side
  option gates camera shake (and therefore `rand()` calls). See netplay_notes.md.

## Fighter flags seen (numbers verified, meanings inferred)

1..4 sequence state; 5 lock-on / basic controlled state; 6 a powered-up mode; 0xD3 this
fighter's camera takes the full screen; 0x125..0x127 hit-stop; 0x128 holds off the
end-of-battle check; 0xF9 round-reset request; 0x135 time stopped; 0xAB hit by a finishing
technique. Longer lists are in the reports summarised in the headers.

## Scope note

`btl_char_api.c` and `btl_char_get.c` are slices of larger original files (about 330 functions
from 0x204E78, and the object around 0x1DBAD0..0x1DCB88). They link on their own only because
they own no shared constants.

## Corrections and additions from the fighter-core batch

See combat.md for the mechanics. Corrections to the tables above (all verified by matching C):

- +0x0974 / +0x0978 / +0x097C / +0x0980 are the current / requested / previous / second-layer
  **animation id**, not a "state id"; roster `tbl[0]` is the per-animation flag word table.
- +0x0948 is the action state `{current, request, prev, queue[4]}`; +0x094C is the requested
  action (-1 = none), and nothing ever writes `prev` at +0x0950.
- +0x00A4 is the **heading** yaw; the model yaw is the rotation y at +0x0024 and follows it.
- +0x0D40 / +0x0D44 are the combo damage total and combo hit count; +0x0D48 the combo timer.
- +0x0F40 / +0x0F44 / +0x0F48 are hit status (pending target, last hit number, contact
  counter); +0x0F50 is the stat modifier table (4 x 0x18).
- +0x0FB0 is the reaction block filled by the hit code (reaction id, push yaw, launch angles,
  stun at +0xFE0, deferred damage at +0xFEC).
- +0x1085..+0x1261 are the flag arrays and stamps; +0x1262 / +0x126B are 72 one-frame effect
  request bits and last frame's copy.
- +0x02E0 / +0x0350 / +0x03C0 are per-pass position and rotation snapshots and their mask.
- +0x0918 holds two per-frame bit sets of the input conditions queried this frame.
- +0x15A0 is the head-tracking state.
- Roster +0x08 / +0x0C are the per-side one-shot and looping sound slot sets; +0x40 is the
  clash state machine; +0x138..+0x278 is the character-change queue; **+0x274 is its
  "time stopped" word, written by `BtlChange_Update`** (the writer earlier listed as not found).
- The manager's step named `BtlChars_UpdateCollision` calls `BtlColl_Update`; the step labelled
  "push-out" in the phase list is a hold attachment for grabs (`BtlChars_UpdateHold`), and the
  real push-out is `BtlMove_PushOut` in the stage-7 pass. The step labelled "gauges" only
  applies queued hits and drains (`BtlMembers_UpdateQueuedDamage`); gauges are updated by
  `BtlAct_UpdateGauges` inside each fighter's action update.

## Fighter API, second part (`btl_capi_b.c`, 0x208430..0x20BA80)

All 136 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). Full tables are in
`include/battle/btl_capi_b.h`. Four groups (verified code; names partly guessed):

- **By object id** (66 functions, used by the AI): gauges, current action, whether the fighter
  can transform / fuse / switch, the opponent's current technique kind and class, clash
  counters, stun timer, an armour level, and the incoming-blast test. A blast counts as
  incoming when it is not moving away and `distance - body radius - 10 <= last step * 5`
  (1.9 for technique records); each is reported once (blast record +0x180).
- **By player index, `BtlCtrl_*`** (43 functions, used by the battle sequence and the story
  script): entrance / win / lose poses (one-frame flags 0xEF..0xF2), scripted motions, warps,
  aura and charge effects, hide / show, gauge edits, forced transformation / fusion / switch /
  technique, `BtlCtrl_CanAct`, `BtlCtrl_IsInterruptible`.
- **Loader side**: wrappers the model loader uses to drive the character-change queue.
- **By side, `BtlSide_*`** (used by the HUD, inferred): gauges, switch target, combo read-out
  (taken from the opponent's combo counters at +0xD40).

Script units (verified): health in percent of the member's maximum; ki in units of 20000;
blast in stocks of 100000. "Raise" means at least, "Lower" at most. Every forced action clears
flag 0xBE and zeroes the stun countdown.

`BtlCtrl_IsMotionPlaying` is confirmed inverted: it returns 1 while a one-shot motion is still
playing. A list of proposed renames for the `BtlCtrl_*` and `BtlFacade_*` placeholders
(PlayMotion, Transform, Fuse, UseTechnique, SetMaxPower, aura and charge effects, Ki / Blast
gauge names) is in the agent's symbol file comments and [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/status.md) follow-ups.

The code assumes a fighter's object id equals its side / player index.

Random draws: two `Rand_Range` users (`PickFusionSlot`, `PickTransformSlot`), both without a
caller in either binary. No pad, camera or sound reads; nothing depends on player 0.

## Fighter API, first part (`btl_capi_a.c`, 0x204E78..0x207020)

79 of 81 functions match per function (two small ki-blast-type loops differ in block layout
and stay in assembly); linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). Tables are in `include/battle/btl_capi_a.h`.

- (verified) **Object +0xFF4 is the character's height** in world units, copied from the model
  header with three more floats (+0xFF8..+0x1000). Body centre = `pos.y - height * 0.5`; look
  height = `height * 0.7`; the scene's "body scale" is `height / 19.35`.
- (verified) Body radius = parameter float +0xC when positive, else object +0xFF0 x 1.8.
- (verified) `BtlCharApi_GetPos`, the position the AI and effects use, is the pose position
  **plus the display offset** (pose +0x20: hover bob and shake); `GetBasePos` is without it.
- (verified) A model node's world position is the translation of its matrix; `GetNodePos` has
  232 callers, all effect modules.
- (verified) Action groups tested by the interface: 0xFD..0x102 skill; 0x105..0x132 technique;
  0x12D..0x12F and 0x139..0x13B rush sequence; 0x130..0x132 clash A; 0xFA / 0xFC clash B / C;
  0xEC..0xF8, 0x103, 0x104 changing form or member; 0x33 circle dash.
- (verified) `BtlCharApi_CalcAimDir` builds the ki blast aim: toward the opponent's node, pitch
  clamped by the caller, mirrored if it would point away.
- (verified) **`BtlCharApi_GetDeflectDir` draws twice from the fighter generator
  (`BtlChar_RandF`) and is called from effect modules**: effect code advances the fighter
  generator, in effect update order. (inferred) It is the direction of a deflected ki blast.
- (inferred) Fighter +0xFFC / 90 is a 0..1 level shown as a full-screen overlay (a blinding
  effect) for object 0, and for object 1 only in split screen.
- Flag meanings (inferred from setters): 8 action switched this frame; 9 fast vertical flight;
  0xA dash; 0xC charging ki; 0x8E charging a ki blast; 0x9A skill applied; 0xA1 rush technique
  connected; 0x12B bound to a new model. 0x11 = in water is verified by its setter.
- Corrections: the field at fighter +0 is the roster index (player), not the side;
  `BtlCharApi_TestFlag0F` / `TestFlag05` duplicate `IsOnGround` / `IsLockedOn`.

## Battle object: faces, parts, nodes, secondary motion (`bobj_b.c`, `bobj_b_b.c`, 0x24F1F0..0x2527B0)

43 of 49 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). Two of the six misses need the previous
range's face helpers in the same file; the two large chain step functions are read from
disassembly only. Layouts are in `include/battle/bobj_b.h`.

- (verified) **Node table**: object +0xD6C maps node id to a 0xE0-byte node: world matrix at
  +0x10 (row 3 = world position), parent's world matrix at +0x50, local rotation quaternion at
  +0xA0, last snapshot position at +0xD0.
- (read from disassembly, bobj_a's range) **Pose pass**: walks the skeleton with a matrix stack
  rooted at the object's world matrix: node world = parent x local. For fighters four node ids
  are overridden (a look rotation, two blended rotations, the jaw).
- (verified in btl_obj.c) **Per frame**, `BtlObj_UpdateAll` runs for every object unless the
  battle is paused: save node positions, animation, pose, secondary chains, pose again, bounds,
  face.
- (verified for the matched functions) **None of it reads a camera, a view or the visible
  flag: node matrices are the same on a drawn and an undrawn frame.** This is what the headless
  simulation needs.
- (verified) **Mouth and eyes**: `BtlObj_SetSubState` is the mouth mode (talk loops, fixed
  shapes, lip tracks of `{frame, code}` records advanced by 2.0 per frame); eyes blink unless a
  frame is forced. Starting a talk mode draws one libc `rand()`; (read) each blink draws two.
  So every object with a face advances the shared libc generator on unpaused frames.
- (verified) **A save-data flag selects which set of 100 lip tracks an object binds** (inferred:
  the voice language). Two peers with different settings play different tracks and consume
  `rand()` differently.
- (verified) **Secondary motion chains** (hair, cloth, tails; node ids from 0x47): up to 16 + 8
  chains per object, stepped in table order. They use a per-object noise source, a logistic map
  `x = 3.9999 * x * (1 - x)` reset to 0.5, not any shared generator. (read) They react to the
  owner's movement, a push, a sway and the stage's wind vector; they only write chain node
  rotations.
- (verified) Part visibility, colour modes (requests by priority), node visibility by object
  flags, node velocity since the last snapshot, push and sway with a 0.85 decay.
- State to save for rollback: face state, chain links, push / sway, node snapshots, the noise
  value.

## Battle object: the animation player (`bobj_a.c`, 0x24BBE8..0x24F1F0)

58 of 62 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). The four misses include the two pose
sampling functions (`BtlObjAnim_SamplePosRot`, `BtlObjAnim_SamplePose`), so the pose rules below
are read from disassembly; events, playback, hit volumes and the matrix walk are verified.
Layouts and the full object field table are in `include/battle/bobj_a.h`.

**Animation file** (verified): header with an event count, a length in frames, and one track
offset per model node (71 nodes). A track is rotation-only (packed quaternions and frame
numbers) or full keys of 0x18 bytes `{x, y, z, frame, quaternion}`. Between two keys:
translation linear, rotation slerp. Events are 0x10 bytes: a 64-bit attribute word, a frame, a
volume offset and an argument.

**Decoded motions are edited in place** (verified): fighter motions are BPE-compressed and
decoded into a per-fighter buffer (0xC000 bytes per layer), then retargeted to the character
(translations rescaled by height against a built-in reference skeleton), with optional
root-axis zeroing and root rebasing. The decoded buffers are simulation state.

**Playback** (verified): fighters are owner-stepped (the fighter code advances the frame; see
combat.md "Animation"); effect models step themselves by 2.0 per update, once or looping. Two
layers; layer 1 can be promoted to layer 0. A blend stores every node's pose and fades from it.

**Events in an interval** (verified): with `cur = (int)(frame + 1/60)` and
`prev = (int)(prevFrame + 1/60)` (forced to `cur - 1` when equal), an event fires when
`prev < f <= cur`. So a paused animation re-raises the events of its current frame every
update, and frame-0 events fire on the first update. At most 8 distinct attribute words per
frame. `BtlObjAnim_QueryEvent(obj, mask, layer, what)`: first frame, next frame, last frame,
count, counts ahead / reached, or the argument, for the events matching a 64-bit mask.

**Hit windows** (verified): event bit 0 opens a window (argument = mask of attack nodes 0..18;
bit 32 = spheres, else oriented boxes), bit 1 closes it. The open window supplies the volume
(offset, radius or half sizes) and the object's hit counters at +0xCAC / +0xCAD.
`BtlObjHit_BuildVolumes` then builds one sphere or box per attack node from the node matrices,
and the body part boxes; the body sphere sits on node 0 at node 0x11's height.

**Node matrices** (verified): `BtlObjPose_CalcMatrices` walks the skeleton parents first on
the VU0 matrix stack, world = local x parent. **No visibility, camera or draw dependence.** On
fighters the jaw node takes the face's rotation while the jaw is active, so a face animation
that draws libc `rand()` reaches one node matrix.

**Random draws** (verified): libc `rand()` only, all in the face code (jaw key choice, two per
blink, talk patterns).
