# Combat: actions, movement, hits, damage, gauges, stats

Sources (all in `src/battle/`): `btl_char_action.c`, `btl_char_move.c`, `btl_char_hit.c`,
`btl_char_coll*.c`, `btl_char_member.c`, `btl_char_status.c`, `btl_char_status_anim.c`,
`btl_char_ctl*.c`, `btl_char_flag*.c`. Layouts are in the matching headers under
`include/battle/` (each file still has its own partial view of the fighter).

**Verified** = confirmed by C that compiles to the original bytes. **Inferred** = read from
disassembly or a judgement about meaning. Game terms such as "guard", "rush", "blast",
"throw" are inferred from the flags and gauges involved; the mechanics are verified. At the
time of writing these files match per function but are being linked.

## Fighter flags (verified)

317 flags per fighter, in two kinds. A **held** flag stays up until cleared. A **plain** flag
drops by itself after one frame. `BtlChar_TestFlag` returns held | plain.

Storage: four 0x28-byte bit arrays at fighter +0x1085 (held), +0x10AD (plain), +0x10D5 and
+0x10FD (their previous values), and one "stamp" byte per flag at +0x1125 recording the stage
in which it was last written. Fighter +0x1084 is the current stage (1..12) of the frame.

`BtlChar_SetStage(chr, n)` promotes every flag stamped n: previous := current, plain cleared.
So a plain flag raised in stage S is visible until stage S begins again a frame later, whatever
the pass order; "previous" means "as of when the writer's stage last began".
`BtlChar_TestPrevFlag`, `BtlChar_IsFlagRaised` and `BtlChar_IsFlagDropped` read the previous
arrays.

Cross-fighter reads are made order-independent the same way: `BtlOpp_GetSeenAction` returns the
opponent's last-frame action until both fighters have reached stage 6, and stage 6 begins with
`BtlOpp_MirrorFlags`, which copies a fixed set of opponent flags into own flags 0xAC..0xB9.

Quirks: flags written before the first `SetStage` are never promoted; `ClearFlag` does not
refresh the previous plain bit; when stage 1 is the first stage of a frame its plain flags lose
their previous bit.

## Actions (verified)

Fighter +0x948 is `{current, request, prev, queue[4]}`; `request` is -1 when nothing is
requested; `prev` has no writer anywhere (it reads 0). +0x964 counts frames in the action;
+0x3D0 is 0x50 bytes of per-action scratch, zeroed on every switch.

The handler table `gBtlActHandlers` (0x2C4980) has 0x13C entries served by 115 functions:
`handler(chr, phase)` with phase 0 enter, 1 run, 2 decide, 3 leave.

One frame (`BtlAct_Update`):
1. `handler[current](chr, 2)` (decide);
2. `BtlAct_CheckForced`;
3. if a request is pending: leave the old action, switch, pop the queue, clear the per-action
   state and several flags, enter the new action;
4. `handler[current](chr, 1)` (run): an action entered on a frame also runs on it;
5. `BtlAct_UpdateGauges`, `BtlAct_UpdateTimers`.

Forced actions come only from fighter flags, in a fixed priority order (table in
`btl_char_action.h`): side steps from flags 0x74..0x76, guard reactions from 0x68..0x6B, hit
reactions through the pending reaction id at +0xFB0, clash actions from 0x61 / 0x62, the
finisher victim from 0xAB, scripted actions from 0xEF / 0xF1 / 0xF2. A handler can block the
check for one frame by setting +0x970.

Action id families (handlers read from disassembly unless decompiled): 1..6 scripted and
utility; 0xB the neutral state; 0xC..0x35 movement and basic actions; 0x36..0x46 guard and
related; 0x47..0x6F more moves; **0x70..0xAD table-driven attacks** (one generic handler; the
attack record is roster +0x2C + (id - 0x70) * 0x18, prepared into fighter +0xD1C and latched
to +0xCF8 at action start); 0xCB..0xEB the airborne family; 0xEC..0xF0 transformation;
0xF1 / 0xF2 fusion; 0xF3..0xF8 member switch; 0xF9 round reset; 0xFA..0xFC clashes;
0xFD..0x102 skill (Blast 1) actions; 0x105..0x132 technique (Blast 2 / Ultimate) actions in
triples whose position gives class 2 / 3 / 4; 0x12D..0x12F and 0x139..0x13B rush sequences;
0x130..0x132 clash A. Hit reactions are 0xB8..0xE0 (see the reaction table below).

### Input conditions (verified)

`BtlInput_TestAction(chr, id, want)` is the single "is input condition N happening" query: 114
numbered conditions over the battle button and command words (held, pressed, released, with or
without a direction, with timing windows of 6, 7, 12 or 30 frames). The full table is in
`include/battle/btl_char_ctl.h`. It has 166 call sites in the action code and records every
query in two per-frame bit sets at fighter +0x918. Conditions 108..114 can be forced by fighter
flags 0x11F..0x124 (how scripts trigger actions). What each condition means in game terms comes
from the handlers that use them.

## Animation (verified)

Fighter +0x974 is the current animation ("motion") id, +0x978 the requested one, +0x980 a
second layer. Roster `tbl[0]` is one flag word per animation id (0x19E entries); bit 0x10000
marks charging motions, 0x8000 airborne ones.

The player lives in the battle object: length +0xB44, frame +0xC78, previous frame +0xC7C,
step +0xC80, blend counter +0xC84 (all floats). Step = length / (seconds * 30).

Actions drive it once per frame: `BtlAnim_Advance` (returns 1 at the end), `BtlAnim_AdvanceThen`
(requests a follow-up), `BtlAnim_AdvanceLoop`. A change is immediate (`BtlAnim_Play`, raises
plain flag 0x2B) or requested (held flag 0x2D) and flushed by the action dispatcher. Frame
events are tested against the interval between the previous and current frame, so a large step
skips nothing.

## Movement (verified)

Per fighter, in the pose block at fighter +0x10: position +0x00, rotation +0x10 (y = model
yaw), impulse +0x70, unit travel direction +0x80, heading pitch +0x90, heading yaw +0x94, speed
+0x98, vertical speed +0x9C (positive = down), two lean angles +0xA0 / +0xA4.

- `BtlMove_Advance`: speed approaches a target by an acceleration; `pos += dir * speed`. There
  is no time step: one call is one 30 Hz frame and all speeds are per frame.
- `BtlMove_Step` (142 call sites in 123 handlers): turn the heading, let the model yaw follow
  at max(1.8 deg, 0.3 * difference) capped at 36 deg per frame, build the direction, advance.
  Movement integration happens inside the action handlers, not in a global step.
- Stick to heading: `WrapAngle(atan2f(stickX, -stickY) + camYaw)`, used only while a direction
  is held.
- **The only camera field movement reads is the yaw at fighter +0x4A0**, in two places.
- Gravity: without flag 0xE (in flight), vertical speed += 50 km/h per frame up to 3000 km/h.
  With flag 0xE it is braked to 0, so there is no gravity in flight.
- Impulse (knock-back): added to position each frame; its length drops by a fixed amount per
  frame; a new impulse replaces the old one only if longer.
- Stage bounds (`BtlMove_ClampToStage`): a horizontal radius and a top and bottom height, with
  held flags 0x14 / 0x15 / 0x16 when clamped. Stages 4 and 27 add a ring: y limit and +-185 on
  x and z.
- Ground (`BtlColl_UpdateGround`): snap to the ground when close and moving towards it; landing
  zeroes vertical speed and holds flag 0xF. Stage contact bits deal 200 / 600 / 1000 damage.
  On stages 4 and 27 certain contacts hold flag 7, which the end-of-battle check treats as a
  loss (ring-out, inferred).
- Push-out between fighters (`BtlMove_PushOut`, stage 7): a plane between them offset by both
  radii; the correction is split by how far each moved towards the other, 50/50 when neither
  did. It reads the opponent only through position snapshots.
- Snapshots: `BtlChars_Snapshot(n)` stores every fighter's pose six times per frame (fighter
  +0x2E0 positions, +0x350 rotations, +0x3C0 mask), which is what lets each fighter see the
  other at a consistent instant.
- Placement: `BtlChar_Place` is the only teleport (it also resets the fighter camera's eye and
  yaw). Seven flag-driven placement requests run in a fixed order each frame (0xF3, 0xF4, 0xF6,
  0xF8, 0xF7, 0xF5, 0xFC / 0xFD). The 0xF8 placement puts player 0 and the other player on
  opposite sides of a stage path point.

Units (inferred, strong): the source wrote speeds in km/h through a macro
(`x * 1000 / 3600 / 30` metres per frame) and angles in degrees; six constants reproduce bit
for bit as 10, 50, 100, 500, 800 and 3000 km/h.

## Gauges and units (verified)

In the active member's gauge block (fighter + 0x9A4 + member * 0xA4 + 0x40):

| Offset | Gauge | Unit |
|---|---|---|
| +0x00 / +0x04 | health / maximum | 10000 per bar |
| +0x0C / +0x10 | ki / maximum | 0..100000 for every character |
| +0x14 / +0x18 | blast stock / maximum | 100000 per stock |
| +0x1C | "max power" (name inferred) | 0..30000 |

Base values come from a 16-byte record per character in the common data (roster +0x30):
health maximum, starting ki, an unused word, number of blast stocks. Item abilities adjust them
at load: health +-10000..30000 (never below 10000), starting ki variants, a full blast stock.

Per frame (`BtlAct_UpdateGauges`), skipped while time is stopped or during hit-stop: charging
motions add ki (four times as fast while a face button is mashed in one state); several
abilities add fixed amounts once per second; benched team members recover +100 health (up to
the top of their current bar) and +1000 ki per second; modes 5 and 6 refill health and gauges
every frame. Nothing else regenerates.

Stun (fighter +0xFE0) only counts down in a charging motion, and each face-button press removes
three more frames.

Member switch during a fight: right stick up / down double tap (input bits 25 / 26), needs
three or more members, not in mode 1.

## Stats (verified numbers; stat meanings inferred)

Four stats (read as attack, defence, ki, blast). Fighter +0xF50 holds four 0x18-byte modifier
records with different lifetimes: kept, timed, until an action group ends, two ended
explicitly, a penalty ignored in the powered-up mode (flag 6), and a one-frame value.

Effective level = clamp(item bonus + clamp(modifier sum + ability extras, -20, 20), -20, 80).
Thirteen derived quantities come from one table (`gBtlStatCurve`, 0x2EE7C0), linear between its
values at -20, 0 and 80; for example the damage-taken multiplier runs from 1.1875 at -20 to
0.25 at +80. Ability extras: +3 levels per lost health bar (abilities 0x58 / 0x71 / 0x72 /
0x73, one per stat); stage bonuses of +15, +10 or +5 from abilities 0x59 / 0x5B / 0x5A.

## Hits between fighters (verified)

There is no hit object. A hit is: the attacker's current attack id, the 0x30-byte attack record
it selects (a table of 0xA3 records reached through the pointer at battle object +0x920;
an id outside 0..0xA2 reads record 0), the attacker's pending target (fighter +0xF40),
and the defender's reaction block (fighter +0xFB0).

Per frame (`BtlColl_Update`), fighter 0 first in each loop:
1. `BtlHit_BeginFrame` per fighter;
2. `BtlHit_CheckClash` and `BtlHit_CheckRush`: either can end the step;
3. `BtlHit_CheckProximityAll`;
4. `BtlHit_TestHit` per fighter: needs active hit volumes and the opponent within 90 degrees of
   the facing; then, in order, invulnerable / dodge / armour / repel (whiff), guard, throw
   break; otherwise the opponent becomes the pending target;
5. `BtlHit_ResolveTrades`: when both target each other, the attack with the higher priority
   byte wins; a tie cancels both;
6. `BtlHit_ApplyHit` per fighter: choose the reaction from the attack record by the defender's
   animation flags, fill the reaction block, request shake and vibration, apply damage.

While anyone is in hit-stop, only the pending targets are cleared.

The attack record (field offsets read from accessor disassembly): flags, damage, guard damage,
ki the defender pays on a guard, attacker ki gain, push speed on hit and on guard, launch
angles, shake power and time, reaction id per defender state, trade priority, guard result per
guard kind, armour levels ignored. About 25 flag bits are identified in `btl_char_hit.h`.

Guard: the guard kind comes from the defender's animation (ids 0xEC..0xF7); guarding fails from
behind ("behind" = the hit's yaw within 90 degrees of the defender's model yaw). A guard costs
resources, not health: one result spends the defender's ki, another gives blast gauge; guard
damage is a separate, smaller value.

Armour: a level summed from both fighters' parameter bits, abilities and powered-up skills,
minus what the attack ignores. When it absorbs a hit there is no flinch and the defender pays
2000 ki.

Damage modifiers here: +20% from behind (unless the attack or the defender's animation opts
out); a small family of attacks scales with a level on both fighters instead.

## Hits from blasts and techniques (verified; calling order inferred)

The effect scene walks its hit records and, for a record overlapping its target, calls in
order: `BtlColl_TryDodge`, `TryDeflect` (+5000 blast gauge), `TryReflect`, `TryAbsorb` (the hit
becomes ki; overflow becomes "max power"), `TryGuard`, and only then `BtlColl_Hit`.

`BtlColl_Hit` dispatches by record type: blast, technique slot 0/1 ("strike"), slot 2..4
("rush"). Blasts pick a reaction from one of four tables by the defender's state and use the
same armour score; strikes can stun (halved by ability 0x3A on the defender, x1.5 by 0x3B on
the attacker); rush hits can catch or throw. Fighter flags 0x44 / 0x45 / 0x46 make a fighter
immune to blasts / strikes / rush attacks. A throw fills a 0x68-byte block on both fighters and
sets flag 0x128 on the attacker.

## Damage (`BtlMember_Damage(chr, amount, flags)`)

Verified in a scratch build; the function is one instruction off until its neighbour is in the
same file. In order:
1. halved if the defender's animation has flag 0x800;
2. combo scaling from the fifth hit: 3% less per hit, floor 50% (2% and 70% with ability 0x5D);
3. multiplied by a per-character value and by the defence stat curve;
4. drain: ability 0x3F cancels it, 0x3E halves it; ability 0x76: no damage at all;
5. nothing is applied unless the fighter is in the fight state;
6. amounts of 10 or more are rounded up to a multiple of 10;
7. modes 5 and 6, drain, and one flag leave at least 1 health;
8. combo counters: hit count, total, and a timer of 30 + total * 0.003 frames, at most 90;
9. KO raises battle events; at 0 health the other gauges are zeroed.

## Clashes (verified; "beam struggle" inferred)

`BtlClash_Update` runs once per frame after the fighter passes (roster +0x40 is its state):

- **A**: both fighters in actions 0x130..0x132. For about 106 frames a lead moves by comparing
  the two fighters' counters (+0xE4C); a tie is broken by `BtlChar_FrameMod(2)`. The loser takes
  the winner's damage plus half its own, or is thrown.
- **B**: both in 0xFA. 76 frames; the higher counter (+0xE50) wins.
- **C**: both in 0xFB. A series of exchanges; after seven or more with unequal counters one
  side wins. Between exchanges `BtlChar_Rand` picks a stage path, a point on it and how many
  exchanges before moving. Those feed `BtlChar_PlaceOnPath`, so the fighter generator decides
  positions.

## Character changes and time stop (verified)

Roster +0x138 is a queue of character-change requests (8 entries of 0x24 bytes). A fighter that
transforms, fuses or switches pushes a request; `BtlChange_Update` (end of every frame, also
while paused) pops it and it then moves through states: popped, taken by the loader, files
loaded, fighter ready, done.

**Roster +0x274, the "time stopped" word, is 1 on every frame that ends with a request
active.** While it is set the fighter generators, the frame counter, gauges, most timers and
input are frozen. So the fight is frozen for as long as the new model takes to load. See
netplay_notes.md.

## Lock-on and sight (verified)

Flags 5 and 0x13 are raised when the opponent is within the fighter's reach and certain flags
are down. Flag 0xBA on both fighters means the segment between their heads is blocked by the
stage (the last part inferred). Head tracking turns the head and neck toward the opponent with
per-character limits; it uses only fighter and object state.

## Order dependence (verified unless marked)

- The collision step tests and applies fighter 0 first; applying fighter 0's hit writes
  fighter 1's reaction and health before fighter 1's own hit is applied.
- The hit-stop loop's early exit favours roster order.
- `BtlChar_PlaceOnPath` places by player index.
- (inferred) Effect hits resolve in the order of the effect scene's record list.
- Movement, push-out and most flag reads are order-independent by construction (snapshots and
  last-frame values).

## Random numbers in the fighter core (verified)

No call to libc `rand`, the Mersenne Twister or the vector-unit generator anywhere in the
action core, movement, hit, collision, member, status or flag code. The only sources are:
- `BtlChar_FrameMod`: side-step direction when none is held, throw variant, attack camera-cut
  variant, one of six random forms, clash tie-breaks, sound variants;
- `BtlChar_Rand`: clash C only.
(Camera shake requested by hits does call `rand()`; see netplay_notes.md.)

## Fighter effect layer (`btl_char_fx*.c`, 0x1D00D8..0x1D3B40)

64 of 67 functions match per function (three left in assembly); linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). What each
effect looks like is not known: names carrying a request or event number mean the trigger is
verified and the visual is not.

- (verified) It turns two inputs into effect-scene calls, sounds, camera shake and vibration:
  the 72 one-frame effect request bits the action code sets (fighter +0x1262), and the
  animation event bits of the playing animation (`BtlAnim_TestAttr(chr, mask)`, a 64-bit mask).
- (verified) It runs at several points of the frame, the main one being `BtlFx_UpdateAll` in
  the camera stage: 37 steps in a fixed order (aura, charge effect, damage sparks, impact
  events, swing sound, power-up look, screen filter, speed lines, ground and water effects...).
- (verified) **It writes simulation state**, so it cannot be skipped on re-simulated frames:
  fighter flag 0x30 is set and cleared by animation events; held flags 0x9F and 0x12E; a
  backward push while flag 0x9F lasts; request bits 0x12, 0x19, 0x1B. Animation event data is
  therefore simulation input.
- (verified) Animation events: 0x80 / 0x100 are heavy / light impacts (flash, camera shake of
  10 for 0.2 s or 5 for 0.1 s, sound, voice); 4 spawns damage sparks; bit 45 vibrates; bit 46
  shakes the camera.
- (read from disassembly) The only random draws are three `BtlChar_RandF` per damage spark for
  position jitter (the fighter generator). No `rand()`, twister or vector-unit generator here.
- (verified) **Partner object**: a second character model attached to a fighter (fighter
  +0x1330: active, resource slot, object id), created by the object load job. It follows the
  fighter, plays animation ids from 0x19E up, and plays voice lines. `BtlPartner_StepAnim`
  returns "finished" when there is no partner, and action handlers branch on it, so two peers
  must attach the partner on the same frame (another load-completion dependency). That it is
  the second character of fusion and team techniques is inferred.
- (verified) Evidence of the original file: `BtlFx_UpdateGroundFx` only matches when the effect
  request-bit helpers (in `btl_char_member.c`) are in the same translation unit, and the float
  pools are contiguous.

## Controls: what the special inputs do (`btl_act_j.c`, 0x203168..0x204E78; verified)

The decision helpers never request an action directly: they fill the four-slot queue with
`BtlAct_SetQueue` and return the slot count; the caller requests the head. Input numbers are
the conditions of `BtlInput_TestAction` (table in `btl_char_ctl.h`).

| Input | Does | Cost / requirements | Action ids |
|---|---|---|---|
| 99..103: R3 tap, neutral or with left / up / right / down | transformation (neutral = the character's default target; directions = targets 0..3) | blast stocks (100000 each); target usable for the side; one kind needs a stage flag (inferred: the moon for Great Ape forms) | 0xEC..0xF0 |
| 104..106: R3 held 12 frames + left / up / right | fusion 0..2 | blast stocks; a listed partner present on the team, alive | 0xF1 / 0xF2 |
| 107: L3+R3 | member switch | fighter +0x99C gauge full (100000); two or more members alive | 0xF3 |
| 108..110: charge + blast, neutral / up / down | technique slot 2 / 3 / 4 | ki; lock-on (flag 5); slot 4 also needs the powered-up mode (flag 6) | 0x106..0x126 in blocks of three per technique kind; 0x105 first for slot 4 |
| 113, 114: charge + guard, neutral / up | skill slot 0 / 1 | blast stocks | 0xFD..0x102 |
| 80 (blast pressed) in the powered-up mode | combo finisher, by the last attack and a per-character mode byte | | 0x115 / 0x116, 0x118 / 0x119, 0x127 / 0x128 |

Inferred: slot 4 is the ultimate, slots 2 / 3 the two blast-2 techniques, skill slots 0 / 1 the
blast-1 skills.

Recovery from a knock-down (`BtlAct_CheckRecoveryInput`): a direction picks one of four
recovery actions 0xE2..0xE5, mirrored by whether the model faces the same way as the fighter
camera yaw (+0x4A0); a face button alone gives 0xE1.

Hit reaction to action (`BtlAct_QueueReaction`, called each frame with the pending reaction id
at fighter +0xFB0): the full table is in `btl_act_j.h`. Reactions 23..25 and 26..28 also set
the stun timer (+0xFE0) to 15 / 30 / 45 frames. A light reaction taken twice in a row becomes a
knock-back (reaction 0x10); most reactions on a dead fighter become reaction 0xF. A per-reaction
counter at fighter +0x1004 saturates at 100.

Story battles force actions through fighter flags (`BtlAct_CheckStoryForced`, mode 1 only):
0xFA plays a scripted animation (action 4); 0x110..0x115 queue actions 5..10; 0x116..0x119
force a transformation, 0x11A..0x11C a fusion, 0x11D a switch. Forced changes skip every cost
and availability check.

No random draw, pad read, sound status or player-0 test anywhere in this range.

Character parameter block (object +0x91C; offsets read from accessor disassembly): +0x98[4]
transformation targets, +0x9C[4] their blast cost, +0xA0[4] sequence, +0xA4[4] kind, +0xAC
default index, +0xAE[3] fusion cost, +0xB1[3] fusion sequence, +0xB4[3] fusion result,
+0xBA[3][4] fusion partners.

## Technique actions, attacker side (`btl_act_f.c`, 0x1F5460..0x1F8C00; verified unless marked)

Correction: actions 0x106..0x11A are **not hit reactions**. They are the attacker's actions for
the class 2..4 techniques (the two Blast 2 moves and the Ultimate), three ids per handler, the
class being the position in the triple. `BtlAct_IsTechniqueId` (0x105..0x132) is therefore
misnamed: it means "is a technique action id". The hit-reaction actions are the ones in the
reaction table above (0xB8..0xE0 and 0x131..0x139). Handler names and the game terms below are
guesses from the animation and flag sequences; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly).

| Actions | Handler | Technique type |
|---|---|---|
| 0x106..0x108 | beam | 0, 7 |
| 0x109..0x10B | warp beam (teleports in front of the opponent before firing) | 9 |
| 0x10C..0x10E | long beam (two firing stages) | 1 |
| 0x10F..0x111 | charge (fires on button release or when the charge time runs out) | 2 |
| 0x112..0x114 | repeat fire (each shot costs health, cannot kill the user) | 3 |
| 0x115..0x117, 0x118..0x11A | quick forms: the powered-up combo finishers, 30% damage, no ki cost (inferred) | |

- Animations: 0x22 per class from id 0x105: +0 start, +1 charge loop, +2 fire, +3 firing loop,
  +4 / +5 second stage, +6 end, +7 / +14 aimed up / down variants, +0x17.. rush chain steps.
- Loops end on held flags 0xA7 (fire), 0xA8 (beam over), 0xA9 (second stage over). The normal
  setter is outside the fighter code (inferred: the effect modules); the handler raises them
  itself after 150 frames, so no loop lasts more than 5 s.
- **Hit-stop**: flags 0x125 / 0x126 are one-frame requests; the other fighter is frozen one
  frame per request. Level 2 (0x125) on every frame of the start animation and charge loop;
  level 1 (0x126) during the fire animation up to its 0x400 event. The charge and quick forms
  never request it. Flag 0x127 (exempt at level 1) is raised every frame by the user; flag
  0x128 accompanies every request.
- A beam that catches moves to other actions: attacker flag 0x73 (set by
  `BtlColl_ApplyRushHit`) requests `class + 0x128`; 0x72 (`BtlColl_StartThrow`) requests
  `class + 0x12B`.
- Rush damage (`BtlSuper_SetupRushDamage`): per hit = total / (hits + 5), capped at 15000,
  rounded up to 10; +50% with flag 0xA0.
- Control returns when the end animation finishes (request action 0xB, or 0xD9 by a flag).
- On leaving, fighter +0xE40 is set to a per-technique cooldown in frames (blocks technique
  input while positive).
- **Load dependency**: the self-destruct variant pushes a character-change request and its
  charge loop waits on `BtlChange_IsLoadedFor` before firing.
- No random draws; no player-0 dependence.
- `BtlAnim_AdvanceThen` takes `(chr, next, f32 blend, s32 flags)`; `btl_char_status.h` declares
  the last two the other way round (same registers, so harmless until fixed).
- "Strike" / "rush" in `btl_char_coll.h` for slots 0..1 / 2..4 is loose: slots 2..4 include
  plain beams. Fighter +0x1594 is the class whose technique button is watched this frame, not
  a switch prompt.

## Neutral state and movement actions (`btl_act_d.c`, 0x1EE058..0x1F1930; verified unless marked)

All 18 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). Handler names are guesses.

**Action 0xB, neutral.** Every fighter returns here.
- Enter: leave flight mode (flag 0xE) if within a small height of the ground; play the idle
  motion (ground 0 / 1, flight 0x26 / 0x27, 0x18B for a non-flyer in the air).
- Run: flight-mode upkeep, loop the animation, brake to a stop, gravity, flags 0xC9 / 0x1C /
  0x25, look at the opponent.
- Decide, in this order: idle timeout (more than 90 counted frames with flag 3 set requests
  action 0x43; not in mode 1); `BtlDecide_Main(chr, 0x22FCBFEB)`; the attack decision
  `func_00201E18(chr, 0x0800054F)`; `BtlDecide_Common(chr, 0xF)`; then request queue[0]. Each
  writes queue slot 0, so later ones override: forced transitions > attacks > the main table.
  The mask argument selects which groups of inputs are live in the calling action.

| Action | What (names inferred) | Notes |
|---|---|---|
| 0xC | idle once | 1 s, no decisions |
| 0xD | free move | yaw from the stick; side-lean layers weighted by the model yaw relative to the camera yaw |
| 0xE | close move | used when near the opponent (flag 0x13); faces the opponent |
| 0xF | dash | DASH held; each turn-around adds 0.1 to a speed multiplier, up to 1.5 |
| 0x10 / 0x12 | jump take-off (0x12 from a dash) | launch at 30% of the motion |
| 0x11 / 0x13 | airborne | lands when fewer than 3 frames from the ground |
| 0x14 / 0x15 | ascend / descend | |
| 0x16 | hop | the ascend of a character that cannot fly (inferred) |
| 0x17 / 0x18 | fast ascend / descend | drains ki per frame; ends when ki runs out |
| 0x19 | homing dash | steers at the opponent; drains ki; stops when blocked, slow for 16 frames, or lock-on is lost |
| 0xB2 / 0xB3 | ki blast / charged ki blast (inferred) | aim vector at fighter +0xDD0 |
| 0x95 | ki volley (inferred) | ten short motions, ki per motion |

- Speeds come from a per-character table by kind (`func_00210940(chr, kind)`): 0/1 move, 2/3
  close move, 4/5 dash, 6 homing dash, 9 fast vertical, 0xD jump, 0x10/0x11 ascend / descend;
  the odd kind of each pair applies in water (flag 0x11, inferred).
- Flags (inferred meanings): 0xE flight mode, 0xF on the ground, 0x11 in water, 0x13 close to
  the opponent, 5 locked on, 0x16 / 0x17 forced flight.
- No random draws, no pad reads, no player-0 dependence. The camera yaw only picks animation
  layers here. A heavy landing requests camera shake (which draws `rand()`).
- Open: two handlers branch on the previous action (fighter +0x950), for which no writer has
  been found. Either a writer exists (a block copy) or those branches are dead.
- `btl_char_action.h` has pose +0x90 / +0x94 as speed / facing; the movement header's layout
  (+0x90 pitch, +0x94 heading yaw, +0x98 speed, +0x9C fall speed) is the right one. Pose +0xD0
  is a bit word, not a float.

## Dashes, steps, vanishes, approach actions and clash actions (`btl_act_e.c`, 0x1F1930..0x1F5460; verified unless marked)

All 20 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). Names and game terms are guesses.

| Actions | What (inferred terms) | Notes |
|---|---|---|
| 0x1A | free-direction dash ("dragon dash") | drains ki per frame; chains into the homing dash 0x19 (needs lock-on) or itself |
| 0x1B..0x1D | step back / left / right | moves between 10% and 60% of the animation |
| 0x1E, 0x1F | short forward dash | 0x1F queues attacks 0x8F / 0x8D / 0x8E |
| 0x20..0x23 | vanishing step | seven frames, invulnerable (flags 0x42..0x46). Entered with flag 0x74 / 0x75 / 0x76 it is a successful dodge: flag 0x12C, event 0x41, +2 s on the stage timer |
| 0x24 | vanish behind the opponent | costs ki |
| 0x25..0x2A | recovery | costs ki; continues to 0xF, 0x19 or 0xB |
| 0x2B..0x32 | approach before a queued attack: vanish and snap, warp behind, warp ahead by the attack's lead time, four-frame slide to an offset point, rush, hop back | each ends by requesting queue[0] (or 0xB) |
| 0x33 | circle dash to the opponent's back | drains ki; requests a camera cut |
| 0x34 | rush dash, 1800 km/h (2200 and invulnerable for some characters) | |
| 0x35 | vanished dash, 2000 km/h, up to 13 frames | |
| 0xFA | clash B: mash | see below |
| 0xFB, 0xFC | clash C: start, one exchange | see below |

- The pending attack block (fighter +0xD1C) holds one motion per part; +0xD34 is the attack's
  lead time in frames (built by `BtlAct_PlayAttackPart`), +0xD38 its approach speed.
- Clash B counts per frame: an accepted press (input condition 0x33) gives one; otherwise an
  automatic one every 4 frames (5 / 8 / 28 with abilities 0x68 / 0x69 / 0x6A); extra counts
  every 5 / 11 / 17 frames with abilities 0x14 / 0x13 / 0x12, and every 15 when the clash came
  out of a technique action. The loser takes `opponent count * 60` damage (`* 600` in clash C).
- Clash C exchange: wait and strike length by clash level are 18 frames / 0.5 s, 15 / 0.4 s,
  12 / 0.3 s. The first physical face-button press is judged once against the prompted button
  (the answer is stored at fighter +0xE58 with the frame it was pressed).
- No random draws: the clash handlers use `actionFrame % n`.
- Player dependence: clash B plays a different animation for player 0 and player 1, and only
  player 0 triggers one effect and two sounds in clash C. Output only.
- A second degrees macro exists in the source: `x * pi / 180` (one bit different from
  `x / 180 * pi` for some values); the float pool comparison found it, fdiff could not.
- Handlers have mixed return types: some only match as `void` (their leave phase ends in a
  tail call), others as `int` with no return.

## Character parameters: ki blasts, movement, techniques, skills (`btl_tech_b.c`, 0x20F0E8..0x2129C8)

All 182 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). Arithmetic and layouts verified; game
terms inferred. Full field tables are in `include/battle/btl_tech_b.h`.

The file is pure readers of the character's parameter file through the battle object, with
abilities and stat curves applied:

| Object field | Table | Prefix |
|---|---|---|
| +0x91C | general parameters | `BtlParam_*` |
| +0x924 | 13 ki blast records of 0x34 bytes | `BtlKiBlast_*` |
| +0x928 | movement table | `BtlMoveParam_*` |
| +0x92C | techniques, slots 2..4 (structure of arrays indexed by slot - 2) | `BtlSuper_*` |
| +0x930 | skills, slots 0..1 (structure of arrays) | `BtlSkill_*` |

File units: speeds in 10 km/h; turn rates in degrees per second; times in seconds; damage in
health points; damage and knock-back stored as totals and divided by the hit count.

| Slot | Term (inferred) | Cost | Damage scaling |
|---|---|---|---|
| 0, 1 | Blast 1 skills | blast stocks (one less with ability 0x15, at least 1) | none |
| 2, 3 | Blast 2 | ki (halved by ability 0x2B) | curve row 9; +20% in the powered-up mode |
| 4 | Ultimate | ki | curve row 10; +20% with ability 0x6F |

Technique damage (`BtlSuper_GetDamage`), each step rounded up to 10: base x curve (/ hit count
for per-hit); quick form x0.3; powered-up +20% (not slot 4); flag 0x9C +10%; full-charge
scaling by fighter +0xE44; some per-technique-id special cases; ability 0x77 replaces it with
999999.

Skills carry: four stat levels, a lifetime kind, a duration in seconds, a health change as a
percent of maximum, a ki change.

Ki blasts: 13 kinds. 0, 4, 8 are the uncharged shots; 1..3, 5..7, 9..11 their charged forms by
the charge at fighter +0xDEC (under 0.3, under 0.9, above); 12 is the volley (action 0x95).
Damage = record x curve row 6, +50% with ability 0xA, halved with 0xB. Ki cost doubled by
ability 0x2A, halved by 0x29; an absorber gains three times the cost.

Movement: speed = table[kind] x 10 km/h x curve row 8, capped at 4000 km/h; the dash kinds are
also multiplied by the dash-turn bonus (pose +0xAC). Ki drain per frame = table value / 30.

Stat curve names now established (old -> meaning): `GetScale4` melee damage, `GetScale5` ki
cost of guarding, `GetScale6` ki blast damage, `GetScale8` movement speed, `GetScale9` Blast 2
damage, `GetScale10` Ultimate damage. So stat 0 is attack, stat 2 is speed and stat 3 is blast
(4 and 5 read from btl_tech_a.c, not verified here).

Fighter +0x15E8: three pointers (per technique slot) to cutscene placement tables,
`{x, y, z, yaw}` five per stage.

No random draws; nothing depends on player 0. `BtlGame_Init` reads the split-screen setting
(HUD only).

Suspected misname (inferred): `BtlFx_FireKiBlast` in btl_char_fx_b.c reads the current
ki blast's kind, type, lifetime, speed, turn rate and hit count: it looks like the ki blast
launcher, not hit sparks. If so its three `BtlChar_RandF` draws are the blast's spread, and
simulation-relevant.

## Guard, charge, search, knock-down, ki blasts (`btl_act_c.c`, 0x1EA5F8..0x1EE058; verified unless marked)

29 of 30 functions match per function; `BtlAct_GuardHandler` is two instructions off (an
instruction-order difference) and stays in assembly. Not linked yet. Names are guesses.

| Actions | What (inferred terms) | Notes |
|---|---|---|
| 0x36 | search for a lost opponent | a window grows each frame (half angle fighter +0xD58 from 0.5 to pi, range +0xD5C from 500); when the opponent is inside both, lock-on (flag 5) is restored |
| 0x37 | ki charge | adds ki per frame; at full ki with one blast stock it fills the +0x1C gauge; when that is full a stock is spent and the powered-up mode (held flag 6) begins. Clears the stat penalty at full ki |
| 0x38 | guard | three poses (animations 0xEF / 0xF1 / 0xF3) chosen by inputs 36 / 37, a pose change takes 3 frames; recoil = pose + 1; released when input 34 is no longer held |
| 0x39 | counter out of guard | queues the evasion attack |
| 0x3A..0x40 | guard-related moves | 0x3B / 0x3D look like blast deflection (low confidence); 0x3E / 0x3F a side step in the air / on the ground |
| 0x41 | reads the fighter camera `side` | picks camera cut 0x11 or 0x12 |
| 0x42 | deals 5000 damage to its own fighter (cannot kill) | |
| 0x43 | idle timeout action (taunt?) | counts in member +0xA0 |
| 0xAE / 0xAF | ki blast / charged ki blast | costs ki; aim limited to +-36 degrees under lock-on, else along the facing |
| 0xB0 / 0xB1 | ki blast / charged ki blast from a dash | animation and aim by facing relative to the camera yaw |
| 0xD3 / 0xE0 | stun on the ground / in the air | lasts while fighter +0xFE0 > 0 |
| 0xDA..0xDE | knocked down | while the member's "must mash" word is set, each face-button press decrements fighter +0x1000; recovery input is accepted once it reaches 0 |
| 0xE1..0xE5 | get up / recover | |
| 0xE6..0xEA | air recovery and landings | |
| 0xEB | waiting for a member switch | then 0xF6 |

- Random draws: one `BtlChar_FrameMod(2)` for a get-up voice. The charge handler requests a
  camera shake.
- Action 0x41 is a reader of the camera's `side` (+0x4A8): it selects between two cuts only.
- Characters that cannot fly sink at 100 km/h while airborne in guard and charge.

### The previous action (fighter +0x950) is never written (verified by search)

No instruction in the executable stores to offset 0x950 of any register, and the two places
that take a pointer to the action block (+0x948: `BtlAct_Update`, `BtlAct_CheckForced`, both
matching C) do not store to its third word. It is zeroed with the fighter and stays 0. So every
branch on `BtlAct_GetPrev` is constant in the shipped game, including: the alternating-hand ki
blast (always the first animation), the four strike animations of clash C (always the first),
the start-animation choices of the dashes, and the "came from action 0x44" test of the vanish
step. This looks like a store lost in development; a port can choose to restore it, but the
original behaviour is "previous action = 0".

## Attack ids, attack records, gauge rates (`btl_tech_a.c`, 0x20BA80..0x20F0E8; verified unless marked)

All 124 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). 102 names are guesses. Full tables
(attack id by action and animation, the 0x30-byte record, the parameter block) are in
`include/battle/btl_tech_a.h`.

- **Attack id** (`BtlAtk_GetId`) is a pure function of the action, the animation, the charge at
  fighter +0xD78 (weak below 0.3, medium below 0.99, else full), fighter +0xD68, flag 0x84, the
  object's hit counters and the frame of the animation's last hit event. Table-driven attack
  actions 0x70..0xAD take it from their row in roster +0x2C. Inferred: ids 0..10 are the rush
  combo, 0x13..0x21 the charged smash (5 directions x 3 charge levels), 0x55 / 0x56 throws.
- **Melee damage** = record damage x attack curve (x0.5 at level -20, x1 at 0, x3 at 80), then
  +-10% by abilities 4 / 5 (rush ids) or 6 / 7 (smash ids), x2 / x0.5 by abilities 8 / 9
  (throws), then divided by the animation's hit-event count when it has two or more. Integer
  arithmetic throughout. Guard damage follows the same chain from its own field.
- **Animation event data is simulation input**: damage, ki gain and several attack ids depend
  on the number and frame of hit events in the playing animation.
- Record units: push and launch speeds in 10 km/h, angles in whole degrees, shake times in
  tenths of a second. Reaction ids are stored per defender state with a default fallback;
  guard results per guard kind.
- **Gauge rates** (per frame at 30 fps; ki maximum 100000, max power 30000):
  ki charge = (parameter / 30 + stat curve + bonus / 30) x ability scale, at least 200;
  passive ki regeneration runs only below a per-character level; a faster recovery rate
  applies in the ki-exhausted state (flag 0xBE) and is x4 on a button press; blast gauge gain
  per frame from its own parameter and curve; max power fills in `param / curve` seconds and
  lasts `param + 0..8` seconds, both converted to an integer per-frame step (so real durations
  are rounded).
- Fixed costs: vanishing step 10000 ki, vanish behind 20000 (ability 0x23 halves, 0x24 frees).
- Stat curve rows: 0 ki charge bonus, 1 ki regeneration bonus, 2 ki recovery bonus, 3 blast
  gain bonus, 4 melee damage, 5 guard ki cost, 8 speed, 11 max power charge, 12 max power extra
  time (6, 9, 10: ki blast, Blast 2, Ultimate damage, from btl_tech_b.c).
- This file's km/h constants need `x * (1000.0f / 3600.0f) / 30.0f`; the form used in
  `btl_char_move.c` is one bit different. The source had more than one spelling.
- The first 29 functions (`BtlCtrl_*`) are by-side queries for one caller at 0x218D88 (HUD or
  tutorial, inferred): flags raised this frame, which button prompt to show. They read pad
  status and progress data: display only.
- No random draws.

## Melee: rush chain, charged smash, vanish attack (`btl_act_a.c`, 0x1E3158..0x1E6CC0; verified unless marked)

All 21 functions match per function; linked since (counts as first reported; [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). This file continues the object of
`btl_char_action.c`. Handler names and game terms are guesses.

| Actions | What (inferred terms) | Notes |
|---|---|---|
| 7..0xA | scripted wait stances | hold lock-on, face the opponent, end when flag 0xAF / 0xB0 drops |
| 0x44 | rush chain step | animation 0x37 + step (fighter +0xD60), ten steps in two halves of five; flag 0x86 or a powered skill jumps to the second half |
| 0x45 | automatic rush chain | forced by flag 0x7E; camera cut 0x11 / 0x12 by the camera `side` |
| 0x46 | rapid rush | five steps; re-queues itself on input 52; camera cut by `side` |
| 0x47..0x4C | charged smash, six directions | see charge below |
| 0x4D..0x52 | full smash | requests hit-stop (flags 0x125 + 0x12A) every frame of wind-up and a 0.2 s hold |
| 0x53..0x57 | smash out of a dash | spends ki per frame while charging; steers at the opponent |
| 0x58 / 0x59 | flying kick / lift strike | |
| 0x5A..0x5D | vanish attack | 8 frames, snap to the opponent, warp ahead by the attack's lead time, strike |
| 0x5E..0x66 | rush finishers | released on input 67; follow-up attack pairs per action |

- **Attack charge**: fighter +0xD78 level 0..1 (float), +0xD7C timer, +0xD84 frames at full.
  A full charge always takes 22.5 frames; the character's rate (two parameters blended by the
  gauge at +0xD80, floor 0.1) changes the level reached and the animation speed. Released on
  the first full frame, a smash holds flag 0x84 and lunges at 3500 km/h for four frames with
  the invulnerability flags 0x42..0x46.
- Follow-ups (`BtlAct_DecideAttackFollow`): only while flag 0x5B is up (inferred: the attack
  connected); the attack block's flag bits open groups of inputs, and which follow-up table is
  read depends on the opponent's seen action, i.e. on its hit reaction.
- Function-local data: `BtlAct_FlyingKickHandler` owns 8 bytes of `.sdata` at 0x2FEB20.
- No random draws; no pad or camera reads other than the camera `side` for cut ids (0x45,
  0x46); nothing depends on player 0.

## The control scheme, skills, member switch (`btl_act_i.c`, 0x1FFAC0..0x203168; verified unless marked)

16 of 18 functions match per function as they stand; the other two differ only in "likely"
branch bits and match when two functions of `btl_act_h.c` are in the same translation unit
(so the original source file spans both). Not linked yet.

**Three decision functions are the control scheme**: `BtlDecide_Main` (movement, guard,
charge, vanishes, specials), `BtlDecide_Attack` (rush, smash, ki blasts, grabs, finishers) and
`BtlDecide_Common` (forced transitions: stun, ki-exhausted, fall, death). Each takes a mask
selecting which rows are live in the calling action, tests rows in a fixed order (first
satisfied row wins) and fills the queue. The complete tables, "mask + state + input id ->
action(s)", are in the header comment of `include/battle/btl_act_i.h`. Highlights:

| Input (condition id) | Requirement | Action |
|---|---|---|
| dash held (3) | | 0xF dash |
| dash + charge press, without / with direction (20 / 21) | ki >= 1000; lock-on for the homing one | 0x19 homing dash / 0x1A free dash |
| ascend / descend double tap (15 / 17) | | 0x17 / 0x18 fast vertical |
| ascend press on the ground (8) | | 0x10 / 0x12 jump |
| step inputs (25..28) | close to the opponent | 0x1B..0x1E |
| guard press + direction (29..32) | ki >= the vanishing step cost | 0x20..0x23 vanishing step |
| charge held (19) | not powered up | 0x37 ki charge |
| guard held (34) | | 0x38 guard |
| blast incoming within 90 frames (43) / 5 frames (39) | | 0x3B / 0x3D deflect, 0x3C |
| direction (1) | | 0xE close move if near, else 0xD |
| no input, lock-on lost | | 0x36 search |
| rush tap (52) | | 0x44 rush chain |
| rush held 6 frames + direction (53..57) | | 0x47..0x4C charged smash |
| blast tap (89) / held 6 frames (90) | ki cost; shots in a row below the character's limit | 0xAE / 0xB0 / 0xB2, 0xAF / 0xB1 / 0xB3 |
| grab (92) | | 0xB4 |

A locked-on fighter facing more than 1.7 rad away from the opponent turns first (action 0x3A)
before guarding or attacking.

`BtlDecide_QueueAttack(chr, id)` is the one place that turns an attack table record into queue
entries: the record carries the opponent state it requires (which hit reaction the opponent
must be in), the input it needs (conditions 79..87) and its lead-in (vanish, warp behind, warp
ahead, slide, rush, hop back: actions 0x2B..0x32).

**Skills** (`BtlSkill_Apply`): apply the four stat levels with the slot's lifetime kind, health
and ki changes, and per-skill special cases (stacking automatic evasions up to 3, a timed
state, a 3000-health cost, the powered-up mode by attribute 0x200), then spend the blast
stock. Three action shapes: with an animation (0xFD / 0xFE), teleporting behind the opponent
afterwards (0xFF / 0x100), instant (0x101 / 0x102). Skill timers stop while time is stopped.

**Member switch**: by input 0xF3 -> 0xF4 -> 0xF5 -> 0xB; after a defeat ... 0xEB -> 0xF6 ->
0xF7 -> 0xF8. **0xF6 waits on `BtlChange_IsLoadedFor`** (disc load time). The arriving member
flies in at 2500 km/h, lands, stands 30 frames, then raises fighter flag 0xF9. Arrivals add 2 s
to the stage timer.

Random draws: one `BtlChar_FrameMod(2)` choosing the side of a vanishing-step lead-in.
`BtlInput_TestAction` records each query in per-frame bit sets, so the order and
short-circuiting of the tables is observable state.

## The generic attack and the hit reactions (`btl_act_b.c`, 0x1E6CC0..0x1EA5F8; verified unless marked)

21 of 23 functions match per function; one more matches once `btl_act_a.c` is in the same
translation unit (same original source file), and one small helper (`BtlActB_TickMemberChange`)
is 14 instructions off. Not linked yet. All names are guesses.

**`BtlAct_AttackHandler`**: actions 0x70..0xAD except 0x92 / 0x9B / 0xAB (dash then strike),
0x99 (pulsed dash then strike) and 0x95.
- Enter: latch the pending attack block (fighter +0xD1C to +0xCF8), play its first motion,
  take the attacker's speed from the attack record, play the attack voice. **The camera cut is
  requested only when the opponent is not attacking or began its attack later**; of two
  attackers the earlier gets the cut, and on a tie neither does. The request raises fighter
  flag 0x27, so cuts are part of the simulation.
- Run: the motion chains through the block's parts. With a record speed the attacker homes on
  the opponent at that speed; contact flags stop it (0x5B at once; 0x5C / 0x5D brake at 100
  km/h per frame). Inferred: 0x5B = the hit landed, 0x5C = guarded, 0x5D = clashed.
- Decide: `BtlAct_DecideAttackFollow` (follow-ups, see the melee section).
- Latched attack block: +0 flags (0x40 end the cut on hit, 0x80, 0x400), +4 `s16 motion[2]`,
  +8 parts, +9 level, +0xA cut, +0x10 rate, +0x1C speed.

**Hit reactions** (the actions the reaction table selects):

| Actions | What (inferred terms) | Verified mechanics |
|---|---|---|
| 0xBD, 0xC0..0xCA | staggers | one animation each; variants cycle with a per-fighter counter (fighter +0xFB8), not randomly; leaving sets +0xFF0 = 15 |
| 0xCB, 0xCC, 0xCD | heavy stagger, ground bounce | animation speed follows the mash rate |
| 0xCE | launched | upward at (0.7 + 0.3 x strength) x 1200 km/h; recovery inputs from frame 9 |
| 0xCF..0xD1, 0xD4 | pushed back | 500 km/h; 700 km/h for strength x 15 frames |
| 0xD5 / 0xD6 / 0xDF | blown away | 50 / 15 / 32 frames at 1000 / 2000 / 2000 km/h. Hitting the floor costs 500 health and shakes nearby cameras; walls and the opponent give their own reactions |
| 0xD7 | recovery out of a blow-away | |
| 0xD8 | lying down | a dead fighter with a member left requests 0xF6 after 31 frames; otherwise gets up after 90 |
| 0xD9 | falling | |

- Strength: fighter +0xFC8 (0..1), inferred to be the strength of the hit that caused the
  reaction.
- Mash-outs count presses of the face buttons: stagger animations run at step 1..4, +1 per
  press, decaying by 4/30 per frame; a blow-away is escaped after
  `4 + (int)(12 x health lost)` presses (needs a character skill bit); lying down needs the
  count at fighter +0x1000.
- On stages 4 and 27 several timers and speeds scale with health lost.
- No random draws. The floor crash calls `BtlCharApi_ShakeCamsNear` (camera shake draws
  `rand()`, gated by a per-side option).
- Three handlers branch on the previous action, which is never written (see above).

## Throws, transformation, fusion, switching out (`btl_act_h.c`, `btl_act_h_b.c`, 0x1FC2B0..0x1FFAC0; verified unless marked)

20 of 21 functions match per function; `BtlAct_GrabDash` is 18 instructions off (a branch the
compiler turns into a conditional move) and stays in assembly. Not linked yet. Handler names
are guesses.

**Throws** (0xB4..0xBF are throws, not character changes):

| Actions | What (inferred terms) | Verified mechanics |
|---|---|---|
| 0xB4..0xB6 | grab dash | flag 0x7A = missed (back to 0xB); flag 0x5B = caught: to 0xB7, 0xB9, or 0xBB by a character parameter flag |
| 0xB7 / 0xB9 | thrower | camera cut 7 or 8 by `BtlChar_FrameMod(2)`; the pair is turned round when near the stage edge |
| 0xB8 / 0xBA | thrown | on release takes the launch yaw and pitch from the reaction block, mirrored when grabbed from behind; then the reaction's action |
| 0xBB / 0xBC | slam throw: both dive at 1500 km/h | ends on the ground, a stage limit, or the 91st loop frame; the victim goes to 0xDE |
| 0xBE / 0xBF | drag down: head first at 2000 km/h | the victim takes 500 damage on landing. **0xBE picks camera cut 0x18 / 0x19 from the camera `side`** |

**Character changes** (0xEC..0xF0 transformation, 0xF1 / 0xF2 fusion, 0xF3 switch out):

1. Enter: animation, camera cut, save the placement, event 0x20.
2. The change request is pushed on the action's second frame (0xED: its first). Time stops at
   the end of that frame; the action frame counter then stays at 2.
3. Every run frame raises hit-stop level 2 (flag 0x125) and twelve more flags: the opponent is
   frozen for the whole action, not just the load.
4. The fighter loops in one specific animation per action and tests only
   `BtlChange_IsLoadedFor(player)`, once per frame. No timeout, no input.
5. On the first frame it is true: request the next animation, `BtlChange_SetReady`, and **pay
   the blast cost** (so the cost is paid when the model is in memory, not at the input).
6. The model swap happens at the next job run (not while paused).
7. `BtlChange_SetDone` on the first frame of the closing animation (0xEC, 0xED, 0xEE, 0xF0), at
   its end (0xEF and fusion, so time stays stopped through their closing animation), or in the
   arriving member's first action (switch).
8. End: event 0x5A; leave adds 2 s to the stage timer (4 s for fusion, none for 0xED), event
   0x4D.

0xEF and fusion queue two requests: an extra object (the partner model) and then the character.
Where each action waits, and its fixed lead-in, is tabulated in `include/battle/btl_act_h.h`
(for example the switch-out hides the fighter from the 16th loop frame and waits from the 46th;
0xEF loops at least 61 frames first).

(inferred) With an instant disc the sequence would be: push on frame P, loaded seen at P+3,
swap at P+4, time running from P+5. **A port can make `IsLoadedFor` become true a fixed number
of frames after the push and leave everything else untouched.**

Random draws: one `BtlChar_FrameMod(2)` (throw camera cut).

## Rush techniques, clash struggle (`btl_act_g.c`, 0x1F8C00..0x1FC2B0; verified unless marked)

13 of 14 functions match per function; `BtlAct_SuperRushDashHandler` is one instruction off
(a delay-slot choice) and stays in assembly. Not linked yet. Game terms are guesses. This file
continues `btl_act_f.c`; these are technique actions, not generic hit reactions.

| Actions | What (inferred terms) |
|---|---|
| 0x105 | pose before an Ultimate; freezes the battle (hit-stop level 2) until its animation event |
| 0x11B..0x120 | rush technique: start-up, then a homing (or straight) dash at the opponent |
| 0x121..0x123 | the same struck in place |
| 0x124..0x129 | player-steered rush dash, and its follow-up form |
| 0x12A..0x12C | attacker's closing animation |
| 0x12D..0x12F / 0x139..0x13B | the scripted hit sequence: attacker / victim, one handler for both |
| 0x133..0x135 | victim of a catch with no sequence |
| 0x136..0x138 | victim thrown straight up |
| 0x130..0x132 | clash A struggle (forced by flag 0xAA) |
| 0x104 | struggle loser, or hit by a finishing technique |
| 0x103 | the fighter is replaced by one of six fixed characters (`BtlChar_FrameMod(6)`) |

- The rush dash loop ends after 16 consecutive frames slower than 100 km/h, after the
  technique's time limit, on loss of lock-on, or on a contact flag. Flag 0x72 (caught) starts
  the sequence.
- The sequence is driven by the throw block at fighter +0xE90: up to five animation steps per
  side, the pair kept placed relative to each other, optional partner object, optional model
  reloads (the victim's damaged variant), the landing kind, and the victim's final turn. The
  field uses are listed in `include/battle/btl_act_g.h`.
- Damage goes through the damage queue at fighter +0xD94, filled on the attacker's entry and
  dealt on animation hit events. Level 1 hit-stop and the deferred reaction damage (+0xFEC) are
  not used here.
- **Clash A struggle score** (fighter +0xE4C): starts at a technique value / 20; +10 / +6 / +3
  with abilities 0x14 / 0x13 / 0x12; -10 / -6 / -3 with 0x6A / 0x69 / 0x68; +1 for each frame
  from frame 16 on that input condition 51 is true. The camera shakes every frame of it.
- **Load dependencies**: the sequence (partner object, model reloads), 0x104 and 0x103 wait on
  `BtlChange_IsLoadedFor`; 0x104 and 0x103 hold hit-stop level 2 meanwhile.
- Player dependence: 0x104 requests camera cut `0x21 + player`.
- (inferred) The original object boundary is probably 0x1FC008, not 0x1FC2B0: the two throw
  helpers at the end are only called from `btl_act_h.c`.

## Status of this file's sources (after the second integration)

Everything described above is linked and byte-identical. Renames applied at that point, so
older text may describe them by their previous meaning: `BtlAct_IsTechniqueId` (was
IsDamageId), `BtlCtrl_IsMotionPlaying` (was IsMoveDone, inverted), the stat curve getters
(`BtlStat_GetKiChargeBonus` ... `GetMaxPowerExtraTime`, were GetRate0..3 / GetScale4..12).
Files were merged where matching proved they were one original source file: the fx-bit helpers
+ `btl_char_fx.c`; `btl_act_a.c` (a + b); `btl_act_f.c` (f + g); `btl_act_h_b.c` (h_b + i).
Seven functions of this code stay in assembly ([upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md)).
