# Effects and stage

Sources: `src/battle/eft_*.c`, `src/battle/stg_*.c` with headers under `include/battle/`.
Everything in the module table is linked and byte-identical. At the second integration eft_t.c went into eft_s.c, eft_u.c
into eft_t_c.c, eft_v.c into eft_u_b.c, eft_ac.c into eft_ab_c.c, and eft_y.c's tail into
eft_z.c; `BtlFx_SpawnDamageSparks` became `BtlFx_FireKiBlast` and the eft_a.h task flags got
their real names (GUARDED, HIT_STAGE, LOST_CLASH, ABSORBED, DEFLECTED, REFLECTED, STRUGGLE,
MULTI_CONTACT). Original module names in the table correspond to these source mappings:
at integration eft_e.c's transition half moved into eft_d_b.c, eft_f.c into eft_e.c, eft_l.c
into eft_k.c, and stg_a.c was split into stg_a.c + stg_a_b.c around three assembly-only VU0
functions. "Match" means per function. **Verified** = matching C;
**inferred** = read from disassembly or a judgement about meaning.

This reference distinguishes: which of this code affects the fight
(simulation) and which only draws.

## Screen effects (`stg_c.c`, 0x245F58..0x248F28): visual only

66 of 68 functions match (`StgHaze_Draw` and `StgBlur_Draw`, both GS packet code, stay in
assembly). Despite its address this is not stage geometry: it is the battle's full-screen post
effects and the manager that creates, resets and draws them. No function touches fighters, hit
records, battle objects, rigid bodies or collision. (verified)

| Group | What |
|---|---|
| `StgCurve_*` | a 3-key Hermite curve sampled into 256 entries (used by fog and haze) |
| `StgHaze_*` | depth haze / heat shimmer, one per view, on stages with feature 9 |
| `StgTint_*` | four full-screen colour layers with linear ramps |
| `StgBlur_*` | radial zoom blur, one per view |
| `StgFx_*` | the group manager: init, term, reset, four draw passes |

- Draw order in a battle frame: stage, `StgFx_DrawPre`, (0x10FF40), effect scene per view,
  `StgFx_DrawPost` (white stage-change fade, blur), HUD, `StgFx_DrawOverlay` (cross-fade,
  front tints). The group has no update call: **its timers advance once per drawn frame**.
- Fighter effect request 0x12 is the stage darkening: tint slot 0, black at the stage file's
  alpha, over 0.2 s, ramped back when no fighter has the request.
- Tint slots 0..2 freeze while paused; slot 3 does not.
- **The haze draws libc `rand()` from inside a draw call**: 2 per mesh vertex per view (2000
  per frame with default parameters), only on stages with feature 9 and not while paused; the
  count depends on split screen and the demo camera. Appearance only, but it consumes the
  shared libc stream that reaches the double-KO tie-break (netplay_notes.md).
- Original bug (inferred): `StgHaze_SetParams` writes the depth curve only into view 0, so
  view 1 has an all-zero curve on stages with a haze section.
- Stage file sections identified: fog, tint colours (two RGBA), haze parameters.

Still without an owner: the stage's rigid bodies, debris and destructibles (0x22FDA0..0x230AA0
and the stage update at 0x243568).

## Module reference

| Range | File | Module | Simulation? | Random draws | Match |
|---|---|---|---|---|---|
| 0x12DD80..0x12F550 | eft_a.c | **hit record list `EftHit_*`** | **yes** | none | (eft_a 80/90) |
| 0x12F550..0x12F810 | eft_a.c | technique camera cut `EftCam_*` (drives the demo camera) | camera only | none | |
| 0x12F810..0x131030 | eft_a.c | shared helpers: palette lighting, splines, `EftMath_WrapAngle`, **projectile aim and homing `EftAim_*`**, clip planes | aim and homing: **yes** | none | |
| 0x1312A8..0x132290 | eft_a.c | clipped polygon and sprite drawing `EftGfx_*` | no | none | |
| 0x14B108..0x14BC98 | eft_h.c | tail of the shot manager: slot parameter blocks, `EftShot_Start` (starts the technique event timeline) | **yes** (effect-side state, starts the event timers) | none | (eft_h 54/61) |
| 0x14BC98..0x14D000 (approx.) | eft_h.c | type -1 (no effect) and **type 1 `EftVolley*`: up to 30 shots** (tasks of the 0x16A400 module, which carry the hits) | **yes**: creates and steers shots, flag 0xA8, restarts the technique timer | none | |
| ..0x14F230 | eft_h.c | effect pack library `EftEmit_*`, first half: `EftEmit_LoadSet` and seven part spawners | no | none | |
| 0x14F230..0x151AD8 | eft_i.c | effect pack library `EftEmit_*` (continues eft_h): spawns, moves and kills the part objects of 11 part modules; six node slots per pack | drives parts; no hit records itself | libc `rand()`: 2 per started part with a spread (reaches the part's spawn position) | (eft_i 36/43) |
| 0x151AD8..0x152978 | eft_i.c | **technique effect type 3 `EftSweep*`: a sweeping beam** | **yes**: one hit record per frame; traces the stage; **destroys stage objects** (`BtlStage_DestroyObj`); sets fighter flag 0xA8 at its end | none | |
| 0x152978..0x1532A0 | eft_i.c | **technique effect type 7 `EftFollow*`: an effect on fighter node 3** | **yes**: a two-sphere hit record per frame; sets flag 0xA8 at its end. Also drives the stage blur light (visual) | none | |
| 0x1532A0..0x1533B0 | eft_j.c | tail of the type 7 follow effect | (see eft_i) | none | (eft_j 64/65) |
| 0x1533B0..0x1542A8 | eft_j.c | type 5 `EftMulti*`: fires up to ten pieces (separate tasks at 0x16D858.. that carry the hits) | **yes** (creates and steers pieces) | libc `rand()`: one per piece for effect id 0x202 (an angle handed to the piece; effect on the hit not verified) | |
| 0x1542A8..0x155588 | eft_j.c | type 6 `EftPropShot*`: one shot carrying a model | **yes**: hit record per frame; sets fighter flag 0xA8 and an object flag | `BtlScene_RandF`: two at init for id 0x165 (model bob, appearance) | |
| 0x155588..0x156450 | eft_j.c | **type 0 `EftBlast*`: the plain blast / beam** | **yes**: hit record per frame | none | |
| 0x156450..0x157398 | eft_j.c | type 2 `EftShotTech*` helpers: up to 14 blast objects (tasks at 0x16A7D0..) | **yes** (creates, retargets and stops them) | none | |
| 0x132290..0x1333C8 | eft_b.c | primitive helpers `EftPrim_*` (quads, triangles) | no | none | (eft_b 43/49) |
| 0x1333C8..0x135070 | eft_b.c | underwater bubbles `EftBubble_*` (pool of 100; layer 0 sub-task 7) | no | libc `rand()`; **count depends on camera pose** (per frame while a view's camera is under water), none in split screen; ambient body bubbles only for object 0 | |
| 0x135070..0x135610 | eft_b.c | scrolling stage sheet `EftStageScroll_*` (sub-task 0) | no | libc `rand()` once at load | |
| 0x135610..0x136760 | eft_b.c | geyser columns `EftGeyser_*` at stage-defined positions (sub-task 6) | no (its two emitters, in other files, not classified) | libc `rand()` once per column at creation | |
| 0x13A9D0..0x13C300 | eft_d.c | end of the stage surface module (triangle queueing) | no | none | 7/10 |
| 0x13C300..0x13EA00 | eft_d_b.c | stage-change transition, first half: 350 particles of six kinds, model draw | no | libc `rand()` at particle creation (14 per streak, re-created when it expires) | 21/22 |
| 0x13EA00..0x13F3D8 | eft_e.c | **stage-change transition `EftBurst_*` (scene layer 4)**: demo-camera animation, a model, 350 particles on a fixed schedule | no state writes, but **the stage swap waits on it** (150 unpaused frames) | libc `rand()` every unpaused frame | (eft_e 47/49) |
| 0x13F430..0x140338 | eft_e.c | stage particle emitters `EftSteam_*` (layer 0 sub-task 5; also used by the geysers) | no | VU0 register: 7 per new particle; emission is not gated by pause | |
| 0x140338..0x142CA0 | eft_e.c | water surface `EftWater_*`, first half (splashes, wakes; continues in eft_f) | one bit: sets flag 0x400 on a hit record's task (inferred private) | libc `rand()`: **one per eligible hit record per frame**, 61 / 44 per splash, 19 / 6 per frame per wake; **all creation is off in split screen** | |
| 0x157398..0x158438 | eft_k.c | type 2 `EftShotTech*` item and manager callbacks, target point by aim kind | **yes**: fires shots (eft_j), sets flag 0xA8 at its end, camera cut | none | (eft_k 63/63) |
| 0x158438..0x159130 | eft_k.c | **technique events `EftTechEvt*`** | **yes**: event bits for every technique module; **sets fighter flag 0xA7 ("fire")** | none | |
| 0x159130..0x15AB38 | eft_k.c | type 8 `EftObjTech*`: one thrown or held projectile with optional model and rings | **yes**: hit record per frame, homing, flag 0xA8, stops the technique timers on a hit | none | |
| 0x15AB38..0x15B550 | eft_k.c | first helpers of type 9 `EftRushShot*` (continues in eft_l) | hit record yes; models visual | libc `rand()`: 13 or 15 per model at placement, 4 at release (appearance) | |
| 0x136760..0x136CC0 | eft_c.c | geyser tail (starts its smoke and steam emitters) | no | libc `rand()`: 2 per geyser creation | (eft_c 68/70) |
| 0x136CC0..0x137BD0 | eft_c.c | weather particles `EftWeather_*` (30, camera-relative; 15 per view in split screen) | no | libc `rand()`: **3 per particle per drawn view, inside the draw callback** | |
| 0x137BD0..0x138178 | eft_c.c | **stage effect manager `EftStage_*` = scene layer 0**: creates a child task per kind the stage has | no | none | |
| 0x138178..0x13A9D0 | eft_c.c | animated stage surfaces `EftSurf_*` (water / lava meshes, palette-lit reflections; continues in eft_d) | no | none | |
| 0x15B550..0x15C728 | eft_l.c (same source file as eft_k.c) | type 9 `EftRushShot*`, rest: the projectile of a rush technique | **yes**: moves and homes, hit record per frame until the rush connects, then stops the technique timer; flag 0xA8 at its end. After connecting it is presentation driven by the victim's events | none | 17/18 standalone (18/18 in eft_k's unit) |
| 0x15C728..0x15E5D0 | eft_l_b.c | type 4 `EftRingShot*`: up to 20 blast objects placed on rings around a fighter, or fired as a volley | **yes**: creates, places, aims and delays blast objects; hit records; flags 0xA8 / 0xA9; restarts the technique timer | **`BtlScene_RandF`: one per shot, reaching the shot's launch position (rings) or direction (volley)** | 20/20 |
| 0x15E5D0..0x15EF18 | eft_l_c.c | `EftAbsorb*`: glow for drain and absorb (fighter requests 0x38 / 0x37) | no | none | 13/13 |
| 0x15EF18..0x15F728 | eft_l_d.c | speed-line spawners (head of eft_m's module) | no | libc `rand()`: 33+ per call on every frame a fighter has request 0xB; 90 per part burst | 2/2 |
| 0x1637A0..0x1655C8 (approx.) | eft_n.c | aura, second half: task, manager, commands (`EftAura_Command`: start, stop, burst, fade, kill) | no | none | (eft_n 41/47) |
| ..0x167E68 | eft_n.c | body lightning `EftBolt_*`, first half | no | libc `rand()` every unpaused frame per lightning task | |
| 0x167E68..0x168600 | eft_o.c | body lightning `EftBolt*`, second half (first half in eft_n) | no | none here | (eft_o.c 35/39) |
| 0x168600..0x1699D0 | eft_o.c | rays `EftRays*` (effect pack part kind 2) | no | libc `rand()`: 2 per ray at creation and per flicker period | |
| 0x1699D0..0x16AE78 | eft_o_b.c | **blast object `EftBlastObj*`** (60 per battle; fired by volley, shots and ring-shot techniques) | **yes** | none | 30/32 |
| 0x16AE78..0x16B4E0 | eft_o_c.c | body effect `EftBodyFx*` (fighter request 0x1A, common effect pack 0x23F) | no | none | (eft_o_c 17/18) |
| 0x16B4E0..0x16C2E0 | eft_o_c.c | **disc `EftDisc*`, first part** (ki blast discs and technique pieces; rest in eft_p) | **yes** | libc `rand()` once at creation (spin, appearance) | |
| 0x16C2E0..0x16DCA0 | eft_p.c | **disc `EftDisc*`, rest** (ki blast types 4 / 5 and the pieces of the multi-piece technique) | **yes**: hit record per flying frame; dies on a hit result | none here | 28/29 |
| 0x16DCA0..0x170A50 | eft_p_b.c | power-up glow particles `EftGlow*`, first part (fighter requests 4 / 5) | no | libc `rand()`: 34 sites, up to 14 spawns per fighter per frame | 18/22 |
| 0x170A50..0x1716A0 | eft_q.c | power-up glow `EftGlow_*`: tasks, manager, entry points | no | none | (eft_q 98/99) |
| 0x1716A0..0x171D80 | eft_q.c | rush burst `EftRushBurst_*` (fighter requests 0x28..0x2A) | no | none | |
| 0x171D80..0x1722E8 | eft_q.c | **scene layer 2: per-character effect root `EftChar*`** | infrastructure: decides which effect kinds (ki blasts included) a character has | none | |
| 0x172480..0x172D80 | eft_q.c | full-screen flash `EftFlash_*` | no (reads fight state; nothing reads it back) | none | |
| 0x172D80..0x174220 | eft_q.c | point trail `EftTrail_*`: dead code (no reference to its class) | no | none | |
| 0x174220..0x174A70 | eft_q.c | body effect of characters 0x37, 0x98, 0x99 | no | none | |
| 0x174AB8..0x175660 | eft_r.c | **beam struggle `EftStruggle_*`** | **yes**: writes both beams' hit position; on its end resets the loser's effect tasks | none | (eft_r 104/105) |
| 0x175660..0x175CA0 | eft_r.c | spark at the struggle point | no | none | |
| 0x175CA0..0x1763E8 | eft_r.c | charge aura (fighter requests 7 / 8) | no | VU0 through a shock wave (20) | |
| 0x1763E8..0x176F10 | eft_r.c | **ki blast `EftKiBlast_*`** | **yes** | fighter generator on deflect / reflect (2) | |
| 0x176F10..0x177548 | eft_r.c | glow while charging a blast | no | none | |
| 0x177548..0x178530 | eft_r.c | **ki blast type 3 `EftKiBomb_*`: thrown bouncing bomb** | **yes** | **VU0: 3 per bomb at launch (throw direction)**; libc `rand()` for spin and sound | |
| 0x178530..0x178AB0 | eft_r.c | ki blast type 2 `EftKiObj_*`, first half | **yes** | fighter generator on deflect / reflect; libc `rand()` for spin | |
| 0x178AB0..0x1793A8 | eft_s.c | ki blast type 2 `EftKiObj_*`, second half: a thrown model with gravity (first half in eft_r) | **yes**: motion, reacts to hit results (break, deflect / reflect), dies 15 frames after a hit | libc `rand()`: 20 per break (fragments, appearance) | (eft_s 27/31) |
| 0x1793A8..0x17CB40 | eft_s.c | chain / lightning ribbons `EftChain_*`, first half (16 strands, shared pool of 500 nodes) | no | libc `rand()` in update, count depends on pool occupancy | |
| 0x17CB40..0x17D290 | eft_t.c | chain module tail (part kind 18; same source file as eft_s's chain) | no | none | (eft_t.c 41/44) |
| 0x17D290..0x17EE68 | eft_t.c | ray burst `EftRay_*` (part kind 0; fighter requests 0x15 / 0x18): up to 48 quads around a point | no | libc `rand()`: 5..6 per ray at creation, 1 + count per frame in modes 1..3 | |
| 0x17EE68..0x1809C0 | eft_t_b.c | streak field `EftStreak_*` (pool of 360; started by technique modules) | no | VU0: 6 per rolled streak | 21/24 |
| 0x1809C0..0x180BF8 | eft_t_c.c | teleport lines head (same source file as eft_u.c) | no | none | 3/3 |
| 0x180BF8..0x182CE8 | eft_u.c | teleport lines (tail of eft_t's `EftShotFx`; fighter requests 0xC..0xF) | no | libc `rand()`, **count depends on the fighter's pose and height** | 16/17 |
| 0x182CE8..0x1853C8 | eft_u_b.c | particle emitter `EftPtcl_*`, head (effect pack part kind 5; pool of 500) | no | libc `rand()`: 25..30 per particle | 9/9 |
| 0x1853C8..0x1871A8 | eft_v.c | particle emitter `EftPtcl_*`, tail (same source file as eft_u_b.c) | no | libc `rand()`: 7..8 per emitter, 1 per emission | 29/31 |
| 0x1871A8..0x187C50 | eft_v_b.c | **impact effect `EftImpact_*`** (hit sparks and projectile impacts) | **no** (verified: no hit record, damage, shake or stage write) | none | 16/16 |
| 0x187C50..0x1895E8 | eft_v_c.c | sprite chain `EftLink_*`, first half (part kind 15) | no | VU0: 1 per sprite slot per frame (count follows fighter node positions) | 13/15 |
| 0x1895E8..0x18C190 | eft_w.c | sprite particles `EftLink_*`, second half (effect pack part kind 15; pool of 200) | no | VU0: 18 per sprite; libc `rand()`: up to 3 | (eft_w 39/44) |
| 0x18C190..0x18D618 | eft_w.c | ring particles `EftPart10*`, first half (part kind 10; 150 rings) | no | VU0: 2 per ring | |
| 0x18D618..0x190CC8 | eft_x.c | ring particles `EftPart10*`, second half | no | VU0: 18 per particle; libc `rand()`: up to 3 | 29/34 |
| 0x190CC8..0x190DA8 | eft_x_b.c | **scene layer 3 root**: creates the 31 common effect managers of table 0x2C3FB0 | no | none | 4/4 |
| 0x190DA8..0x191D28 | eft_x_c.c | quad emitter `EftQuad*`, first half (part kind 9; 200 quads) | no | VU0: 28 per quad; libc `rand()`: up to 5 | 11/11 |
| 0x191D28..0x195038 | eft_y.c | quad emitter `EftQuad_*`, second half (part kind 9) | no | none in this half | (eft_y 38/41) |
| 0x195038..0x195EE8 | eft_y.c | camera-facing strip `EftLine_*` helpers (part kind 16; rest in eft_z) | no | none | |
| 0x195EE8..0x196E40 | eft_z.c | camera-facing sprite, part kind 16 (same module as eft_y's `EftLine_*` helpers; named `EftBill_*` here) | no | libc `rand()`: 2 per sprite | (eft_z.c 55/57) |
| 0x196E40..0x199F28 | eft_z.c, eft_z_b.c, eft_z_c.c | ground dust `EftGndDust*`: six kinds (debris puff, slide, dash, burst, landing ring, **ground impact 0x1975A8**) | **no** (verified for the ground impact) | VU0: 8 per debris particle; libc `rand()`: 39 per landing ring, 13 per ground impact | 4/5, 8/9 |
| 0x199F28..0x19E0C0 | eft_aa.c | ground dust helpers; delayed sounds `EftDelaySe*`; per-fighter effect slots; weapon trail `EftBlade*` (eight character ids); **blinding overlay `EftBlind*`**; part kind 14 `EftAnimPart*` | no | VU0 and libc `rand()` in dust; 6 libc `rand()` at every scene init | 73/79 |
| 0x19E0C0..0x1A0020 | eft_ab.c | orb tail `EftOrbTail_*`: the aura-coloured ball of the shots technique with a node chain and streaks | no | VU0: 11 per streak; libc `rand()`: 1 at creation | 32/35 |
| 0x1A0020..0x1A0E58 | eft_ab_b.c | transformation / fusion effect `EftTransform_*` (fighter requests 0 / 1 / 2) | no (hides and shows the fighter model; stage tint; two shock waves) | VU0 through the shock waves (40) | 17/17 |
| 0x1A0E58..0x1A21A8 | eft_ab_c.c | ribbon `EftRibbon_*`, head (part kind 17; same source file as eft_ac.c) | no | libc `rand()`: 1 per ribbon with a pack flag | 7/12 |
| 0x1A21A8..0x1A3B00 | eft_ac.c | ribbon `EftRibbon_*`, tail (part kind 17; head in eft_ab; 40 tasks) | no | none in this half | (eft_ac 37/44 standalone; 3 more need eft_ab's ribbon helpers in the same file) |
| 0x1A3B00..0x1A62C8 | eft_ac.c | swirl lines `EftZap_*` (part kind 12; handle entries in eft_ad) | no | VU0: 23..24 per line; libc `rand()`: up to 7 | |
| 0x1A62C8..0x1A7018 | eft_ad.c | part kind 12 handle entries `EftZap_*` (module starts in eft_ac); fighter request 6 `EftShock_*` | no | VU0 through one shock wave (20) | 29/29 |
| 0x1A7608..0x1A9D90 | eft_ad_b.c | effect mesh renderer `EftMesh_*`; **effect model objects `EftObj_*` (wrappers of `BtlObj_Create`)** | no (draw state of battle objects) | none | 32/36 |
| 0x1A9D90..0x1AA7E8 | eft_ad_c.c | sprites `EftSpr_DrawFlat / DrawRot` | no | none | 0/2 |
| 0x1AA7E8..0x1AD138 | eft_ae.c | keyframed sprite animations `EftSprAnim_*` and their "V000" packs (played by part kind 14) | no | libc `rand()`: 2 per layer per step | (eft_ae 86/89) |
| 0x1AD138..0x1ADBA8 | eft_ae.c | **task tree `BtlTask*`** | **core: decides the update order of every effect task** | none | |
| 0x1ADBA8..0x1AE2A8 | eft_ae.c | per-frame VRAM allocation for effect textures, texture set loaders | no | none | |
| 0x1AE2A8..0x1AE5F8 | eft_det_a.c | texture set loaders, VRAM upload | no | none | (eft_det_a 40/40) |
| 0x1AE5F8..0x1AF508 | eft_det_a.c | **volley aim `EftVolleyAim_*`**: spread and steering of volley shots | **yes** | **`BtlScene_Rand*`: 0..3 per shot at fire (direction), 3..4 on one scripted frame per lobbed shot (target offset)** | |
| 0x1AF508..0x1AF7B8 | eft_det_a.c | **fighter strike volumes against the other fighter's body `BtlBodyHit_*`** | **yes** | none | |
| 0x1AF7B8..0x1B1260 | eft_det_a.c | **projectile hit detection `EftDet_*`** | **yes** | none | |
| 0x1B1260..0x1B16F0 | eft_det_a.c | **ground probe `StgGround_*`** | **yes** | none | |
| 0x1B16F0..0x1B3510 | eft_det_b.c | **stage collision queries `StgCol_*`** (fighter body sweep, segment trace, camera sweep, debris, shadow) | **yes** | none | 30/33 |
| 0x1B3510..0x1B3F78 | eft_det_b_b.c | stage way-point graph `StgNav_*` and path search (AI only) | yes (AI input) | none | 10/12 |
| 0x1B3F78..0x1B4140 | eft_det_b_c.c | head of the AI sequence object (`BtlAiSeq_Reset`, `PushRule`) | yes | none | 1/2 |
| 0x22FD10..0x230B38 | stg_d.c | stage debris rigid bodies `StgRigid_*` (pool of 128) | **no**: bodies touch nothing, not even the stage | libc `rand()` x6 and VU0 x4..7 per body launched (the VU0 count depends on the libc bits) | 20/20 |
| 0x115170..0x115478 | stg_d_b.c | stage object animations `StgModel_*` | no | VU0: one per animated object at every stage reset | 5/5 |
| 0x23FB20..0x242D28 | stg_a.c | **stage core `BtlStage_*`**: file binding, bounds, zones, start placements, paths, water level, destructible objects; plus frustum and fade helpers (visual) and an unreachable stage viewer | **yes** | libc `rand()`: one in `BtlStage_DestroyObj`, hidden-item case only | 70/78 |
| 0x242D28..0x2435C0 | stg_b.c | stage data readers `BtlStage_*`, stage timers, `BtlStage_Update` | timers and flags only (readers elsewhere) | none | (stg_b 84/88) |
| 0x2435C0..0x244170 | stg_b.c | stage ambience sound `StgAmb_*` (23 per-stage volume handlers) | no | libc `rand()` on stages 3, 4, 10, 15, 27 (random one-shot sounds) | |
| 0x244170..0x244890 | stg_b.c | screen cross-fade `ScrXfade_*` | no | none | |
| 0x244890..0x245878 | stg_b.c | screen shock waves `ScrWarp_*` (10 rings) | no | VU0 register: 20 per spawn | |
| 0x245878..0x245F58 | stg_b.c | depth blur `StgFog_*` (continues in stg_c) | no | none | |
| 0x245F58..0x248F28 | stg_c.c | screen effects (haze, tints, blur) | no | libc `rand()` in a draw pass (haze) | 66/68 |
| 0x15F728..0x1609C8 | eft_m.c | speed lines `EftSpdLine_*` (30 trails, 40 streaks; task table `D_002C3A18`) | no | libc `rand()` in update / spawn | 20/20 |
| 0x1609C8..0x1637A0 | eft_m.c | aura particles `EftAura_*`, first half (flames from 10 body parts, sparks from 12 emitters; pools shared by all fighters) | no | libc `rand()` in update / spawn, count depends on live particles | 30/30 |
| 0x142CA0..0x147050 | eft_f.c | water-surface particles, second half of the water module in eft_e.c (pools: 15 trails, 60 drops, 30 rings, 30 sprays, 30 mists) | no (reads a hit record's position, fighter height, water height) | libc `rand()`: 19 per blast trail, **not spawned in split screen** | 35/40 |
| 0x147050..0x147928 | eft_g.c | helpers (facing matrix, water clip, blast record class) | no | none | (eft_g 63/71) |
| 0x147928..0x148DF8 | eft_g.c | storm `EftStorm_*`: 4 lightning bolts, 64 rain drops (scene layer 0, sub-task 3) | no | libc `rand()`; **rain re-rolls inside the draw pass per view**, so the count depends on camera pose and split screen | |
| 0x148DF8..0x149818 | eft_g.c | stage smoke emitters `EftSmoke_*` (sub-task 4), 40 particles each | no | libc `rand()` and the VU0 register per particle | |
| 0x149818..0x14A828 | eft_g.c | stage boundary wall `EftBound_*` (sub-task 8), a mesh on the stage cylinder near each fighter | no (reads fighter state) | none | |
| 0x14A828..0x14B108 | eft_g.c | **shot slots `EftShot_*`: start of scene layer 1** | **yes** | none | |

## Notes common to effect modules

- (verified, eft_m) Effect task tables have the shape `{update, init, term, stub, 0, draw}` and
  are listed in `D_002C3FB0` as pairs `{table, 1}`.
- (verified, eft_m) The effect code's vector type is 16-byte aligned and passed by value with a
  callee copy (`ld` / `sd` pairs at function entry); `Vec4` from sys/math3d.h does not
  reproduce that, so effect files use a local aligned type.
- (verified) Visual modules read fighters only through the read-only `BtlCharApi_*` getters.
- (verified) Visual modules consume libc `rand()` at a rate that depends on how many particles
  are alive and on the pause flag. Since libc `rand()` also reaches simulation (double-KO
  tie-break, camera shake), a port must give visual effects their own generator.
- (inferred hazard, eft_m) `EftAura_StepFlames` revisits the same flame forever if the flame
  pool is exhausted when an expired flame tries to spawn its successor.
- (verified, eft_f / eft_e) Blast trails on water are not spawned in split screen, so the libc
  `rand()` call count depends on the screen mode: another reason for a separate generator.
- (verified, eft_f) Effect particles step by a fixed 1/30 s; durations are `seconds * 30`
  frames; updates do nothing while paused (battle flag 0x100).
- (evidence) eft_e.c and eft_f.c were one source file: `EftWaterRing_Update` matches only with
  `EftWater_GetSurfaceY` (0x140338) defined earlier in the same file.

## Shot layer (simulation; `eft_g.c` from 0x14A828, continues in eft_h)

- (verified) Scene layer 0 holds stage-wide effect sub-tasks registered in `D_002C3568` as
  `{class, index}` pairs. **Scene layer 1 is the shot layer**: `gEftShot` holds one 0x540-byte
  `EftShotChar` per character with 5 slots of 0x50 bytes and 5 definitions of 0x8C bytes. Slot
  kinds 0 and 1 are ki blasts, 2..4 techniques.
- (verified) A hit record's source pointer (+0x64) is a shot slot; the slot points at its pack
  data (+0x1C) and definition (+0x24).
- (verified) `EftShot_Request` (0x14AB90) is what the fighter calls to start a shot.
- (verified) **The effect code ends a beam's firing loop**: `EftShot_SetHeldFlagA8` / `A9` set
  the fighter's held flags 0xA8 / 0xA9, the "beam over" / "second stage over" flags the
  technique handlers wait on (combat.md). Seven callers further into the effect range.
- (verified) Each frame a character's current slot is cleared when the fighter is in neither a
  technique nor a skill. The code uses the character index as the fighter's object id.
- (verified hazard) `EftShotMgr_Init` always creates tasks for characters 0 and 1 and writes
  the second entry whatever the character count is.

## Hit records (simulation core; `eft_a.c`, verified unless marked)

`gEftHitList` is 64 records of 0x190 bytes plus a count; `gEftHitArena` is a 0x1800-byte
per-frame bump allocator for their shapes.

- **A record lives one frame.** `EftHit_BeginFrame` (start of `BtlScene_Update`) empties the
  list; projectile tasks then append records (`EftHit_GetNew`, fill, `EftHit_Add`), one or more
  per projectile per frame. Twelve task modules create records.
- A record has: owner object id, type (1 technique with a source slot and definition, 0 ki
  blast with blast parameters), `pos` / `prevPos` / `vel`, flags, a swept shape (two spheres or
  two capsules: the "box" shape is a segment plus a radius), and a pointer to its owning task.
- Feedback goes to the owning task as flags: bit 0 hit a fighter, 0x10 left the stage, 0x40
  dead, 0x200 impact effect spawned, 0x4000 multi-hit, 0x8000 one-shot. `EftHit_UpdateResults`
  (start of `BtlScene_PostUpdate`) condenses them into `task->result` (0, 1, 3, 4 hit,
  5 finished), which decides whether the projectile dies.
- Multi-hit techniques count hits in the task; record flag 0x10 marks the last hit.
- **Clash rule**: two blasts compare `level` (higher wins, equal is a draw); a blast loses to a
  technique; two techniques clash only for certain classes and kinds, and moving head-on gives
  the beam struggle (raised for both sides).
- Stage bound: a record further than stage radius - 100 from the origin gets "left the stage",
  is pulled back onto that radius (its collision shape is rewritten), and finishes.
- Aim: `BtlCharApi_CalcAimDir45` when locked on, else the fighter's yaw; more than 60 degrees
  off the facing falls back to straight ahead. Homing (`EftAim_Home`): target = opponent node
  0x11 + opponent frame movement x min(distance / speed, 15) x 0.5, turn capped per frame; no
  turn when the target is behind or lock-on is lost.
- No random draws anywhere in 0x12DD80..0x132290.

Hazards and order dependence (verified):
- **The list wraps destructively**: a 65th record in a frame resets the count to 0, dropping
  every earlier record of that frame. Which survive depends on task update order.
- `EftHit_GetNew` does not reserve its slot.
- Record index = creation order, and every pass walks it in order; when several records share
  a task the last one's result wins.
- `EftHit_Clash(a, b)` is not symmetric: the outcome can depend on which record is `a`.
- Homing leads the target with the opponent's frame movement, so it depends on whether the
  opponent has already moved this frame.
- While `BtlScene_IsTimeStopped()` (pause, or stage not ready) no record is added **and the
  list is not emptied**, so the last frame's records stay.

The hit detection that consumes the list is at 0x1AFDB0..0x1B10F0 (not decompiled yet).

Corrections to earlier notes: `BtlBlastRec.active` (+0xC) in btl_scene.h is the record type;
what btl_scene.c calls a record's "definition flags" is the owning task's event flags (so in
`BtlScene_CheckStageChange` the test means "the task hit a fighter").

## Technique effects (simulation; `eft_i.c`, verified unless marked)

- A technique effect type is a pair of task classes in `gEftShotClass` (0x2C3700, row = type +
  1): a group class and an instance class. Two types are in this file.
- Both are driven by the fighter's animation effect events, read with
  `EftShot_TestBits(objId, bit)`: 2 aim, 4 fire, 8 stop, 0x400 end now. Nothing runs while the
  character is stopped (`BtlScene_IsCharStopped`).
- **Type 3, sweeping beam**: the far end moves on a circle around the origin at about the
  opponent's distance (capped at 800), 1.08 degrees per frame, starting 11 steps before the
  opponent. While fired it publishes one hit record per frame (two spheres or two boxes). Each
  frame the beam segment is traced against the stage; on a hit the end is pulled back to the
  surface, a 10-frame impact mark is queued (15 at most; a 16th is dropped) and a hit stage
  object is destroyed.
- **Type 7, follow effect**: keeps a two-sphere hit record on fighter node 3 from the fire
  event until it ends.
- A hit result in the task flags stops the record. At their end both set the fighter's held
  flag 0xA8 ("beam over").
- The effect pack library (`EftEmit_*`) binds six node slots to model nodes the first time each
  event bit is seen and samples them every frame; event 0x400 freezes them.
- Order: `BtlStage_DestroyObj` is called from effect updates, so its order relative to the
  other fighter's effects is the task list order.

## Stage (from stg_b.c, verified)

- `BtlStage_Update` (0x243568): skipped while stage load jobs run; otherwise stage timers,
  stage objects (`BtlStage_UpdateObjs`), the ambience handler, then 0x115370 and 0x2309A8.
- Stage timers add 0.13333333 per unpaused frame and raise a one-frame flag when they pass
  their period (readers not found yet).
- Stage flag word: bits 1 / 2 / 4 enable three stage effect resources; bit 0x10 is what the
  transformation check tests (inferred: the stage has a moon).
- Stage change target comes from the stage info (1 gives stage 0xF, 2 gives stage 3).
- The collision and ground queries the fighters, camera and AI call are in the stg_a range
  (0x23FE70, 0x23FEB0, 0x23FEF8, 0x23FF38, 0x2427A0, 0x242668), not here.

## Blast and beam items (simulation; `eft_j.c`, verified)

`gEftShotClass` rows are {manager class, item class}; row = effect type + 1. Types so far:
0 blast / beam, 2 multiple blast objects, 3 sweeping beam, 5 multi-piece, 6 shot with a model,
7 follow effect.

- Item life (types 0 and 6): START event aims; FIRE puts the head at the muzzle node with
  velocity = aim direction x speed; each flying frame tail = head, optional homing
  (`EftAim_Home`), head += velocity, or head = the position the hit pass corrected; END or
  ABORT (0x400) starts the ending; the task dies after the emitter set's end frames.
- One hit record per frame while fired and not ending: two spheres at head and tail, or two
  boxes from the muzzle to head and to tail. **Radius = item radius x the emitter set's trail
  width animation**, so that "visual" animation track is simulation input and
  `EftEmit_UpdateTrailWidth` cannot be skipped.
- Blast size follows the fighter's charge (fighter +0xE44, and +0xE5C / +0xE60 for two specific
  techniques), which therefore scales the hit shapes.
- The hit pass reports back through the owning task: head corrected, stop, hit a fighter.
- Every update is gated by `BtlScene_IsCharStopped(objId)`.
- Effect code creates battle objects (`BtlObj_Create`) for shot models, consuming object ids.
- Stage blur is driven by effects (definition flag 0x200): visual.
- (verified, eft_b) Original bug: `EftBubble_RandDir` takes its output vector by value, so the
  burst bubbles' direction is uninitialised stack data.
- (verified, eft_b) The texture set effect modules load is 32 entries of 0x10 bytes
  `{u64 tex0; image pointer; pad}` plus a count at +0x200.
- (verified, eft_e) The stage-change transition counts 150 unpaused frames; the stage swap in
  battle_load.c waits on `EftBurst_IsBusy`. Its particles and sounds use libc `rand()`.
- (verified, eft_e) On water stages `EftWater_UpdateBlast` is called for every hit record each
  frame and draws `rand()` once per eligible record before testing anything, so the libc call
  count follows the number of live projectiles.
- (evidence) Source file boundaries: 0x13C300..0x13F3D8 (transition; `EftBurst_Update` needs
  three functions of eft_d.c in the same file) and 0x140338.. through eft_f (water).
- (prelude, to verify) `EftBurst_Update` also needs the HI/LO hazard handled: the compiler comparison showed a
  match with `.set mips64` instead of `.set mips4` in `__gp_forget` on a private prelude copy;
  it did not test other files with that change.

## Technique events and timers (simulation; `eft_k.c`, verified)

- `gEftTechEvt` holds one 16-byte entry per fighter, indexed by object id: an `events` word
  cleared every frame and a `requests` word. `EftShot_TestBits(objId, mask)` reads `events`;
  every technique module branches on it.
- A timeline task per technique in progress fills the events from the fighter's animation
  event attributes (0x200..0x4000 and 0x200000..0x800000 become events 2 start, 4 fire, 8 end,
  0x10 / 0x20 / 0x40 / 0x80 / 0x100 / 0x200 module-specific; 0x400 = aborted), or, for some
  definitions, from six frame numbers stored in the definition.
- A rush technique's timeline reads every fighter's animation attributes and writes each
  fighter's own events, so the victim's animation drives effects too.
- **Fighter flag 0xA7 ("fire", which ends a technique's charge loop) is set here**: a fire timer
  starts at event START and, when it runs out, the effect scene sets the flag. An end timer
  (definition life - 1) starts at event FIRE; while it is at 0 the END event is raised every
  frame. Other modules can STOP (freeze both timers, e.g. on a hit or a beam clash), RESTART or
  EXPIRE them.
- **Hazard for a headless or re-simulated frame: both timers are stepped, and flag 0xA7 is
  set, in the task's DRAW callback** (`EftTechEvtTask_Draw`; a guard bit makes it run once per
  frame however many views are drawn). A frame that skips `BtlScene_Draw` does not advance
  them. A port must call that step from the update.
- Order: the events task is first in the layer-1 list; each frame it clears all words, the
  timelines fill them, then character 0's technique tasks read them, then character 1's.
  `EftHit_ClashTech` stops the timers of objects 0 and 1 by literal id.
- The class table at 0x2C3700 is `{manager class, item class, 0}` by effect type + 1. Types
  known: 0 blast, 2 shots, 3 sweep, 5 multi, 6 prop shot, 7 follow, 8 object, 9 rush shot.

## Scene layer 0: stage effects (verified, eft_c)

Layer 0 is the stage effect manager. Its init creates one child task per entry of the table at
0x2C3568 whose stage test passes:

| Kind | Module | Exists when |
|---|---|---|
| 0 | scrolling stage sheet | stage pack entry 7 |
| 1 | animated surfaces | entry 0xD |
| 2 | weather | entry 0xB (and a stage flag) |
| 3 | storm (lightning, rain) | entry 0xE |
| 4 | smoke emitters | a stage effect list |
| 5 | steam emitters | a stage effect list |
| 6 | geysers | a stage effect list |
| 7 | water surface and bubbles | a stage flag and entry 0x12 |
| 8 | boundary wall | entries 0x15, 0x16, 0x17 |

Everything in layer 0 found so far is visual. Scene layer 1 is the shot layer (techniques and
blasts, simulation); layer 4 is the stage-change transition.

## Stage core (simulation; `stg_a.c`, verified unless marked)

- **Stage file**: a pack with a header of byte offsets; +0x08 is the parameter block, member 16
  an optional "MEF0" block, member 21 the path table. Bound once per load. Positions in the
  parameter block are stored with y and z negated and flipped at bind time; path points are
  not.
- **Bounds**: radius, top, bottom. Fighters are clamped to radius - 100. While the stage is not
  ready each getter returns the last value it read (and the inner radius comes back without
  the -100). The AI subtracts another 100, so its limit is radius - 200.
- **Zones**: rectangles on the ground plane with neighbour lists. A position's zone is the
  first rectangle containing it in array order, falling back to zone 0. Collision queries and
  fighter placement work per zone.
- **Ground probe** (`BtlStage_ProbeGround`): a small box around the point is handed to the
  collision query `func_001B14C0` (not decompiled; in the hit-detection range).
- **Start placements**: one (position, target) pair per player, an alternative pair, and a
  third; each is dropped onto the ground by probing from y = -700 and faces its target.
- **Water**: an environment flag and a level, cached while the stage is not ready.
- **Destructible objects**: 0x50 bytes each with hp, type bits and a parent index from the
  stage data. `BtlStage_DamageObj` subtracts hp; `BtlStage_DestroyObj` breaks the object.
  (read from disassembly) Breaking swaps the drawn model, disables the object's collision
  records, shakes cameras and rumbles by a size bit, and breaks every child object in index
  order. Debris then falls for 74 frames by default: keyed pieces by animation, single-key
  pieces as rigid bodies. Debris is read by no fighter code.
- What breaks objects: projectile collision (0x1B0E88), a fighter flying through (0x1B24B8,
  using the fighter's two "stage break" tests), and the sweeping beam.
- `BtlStage_Reset` restores everything whole. The stage change is a loader request.
- Random draws: one libc `rand()` in `BtlStage_DestroyObj`, only for the story-mode hidden item
  and only when the breaker's input is not injected (CPU or replay): it decides an item award,
  not fight state, but the draw itself depends on who controls the fighter.
- Ten functions with no caller are a development stage viewer.
- (verified, eft_l_b) **The scene generator reaches the simulation in the ring shot**:
  `BtlScene_RandF` decides each blast's bob phase (so where it is when launched) and the
  volley's spread direction. Drawn at creation, in creation order, so the generator's sequence
  depends on task update order between the two characters.
- (verified, eft_l) Common shape of technique effect modules: the owner's events 2 / 4 start,
  4 fires, 8 ends, 0x400 aborts; the hit pass writes results back into the task and the task's
  post-update reacts.
- (evidence) eft_l.c is the tail of eft_k.c's source file (`EftRushShot_UpdateAttached` needs
  `EftRushShot_UpdateModels` in the same file); eft_l_d.c + eft_m.c are the speed-line object.
- (verified) Original bugs: `EftSpdLine_SpawnBodyTrails` never resets its extra-trail count
  between nodes; `EftAbsorb_Init` aims the second hand glow from the first hand for one frame.
- (evidence, eft_d) Source file boundaries: 0x138178..0x13C300 (surfaces: eft_c + eft_d.c) and
  0x13C300..0x13F430 (transition: eft_d_b.c + the first part of eft_e.c; the two "tests without
  callers" at its end are that file's non-static inlines).
- (verified, eft_h) Shot slots 0 and 1 are the two skills, 2..4 the techniques (correcting the
  eft_g note). `EftShot_Start` sets the effect's life to the request's seconds x 30 (60 frames
  for a skill) and starts the event timeline with a lead time of 0.85 s (0.8 s for some
  techniques; per-id values for the ultimate), read from disassembly.

The module files were compared against the original binary.

## Stage debris and stage update order (verified, stg_d)

- `BtlStage_Update` order: stage timers, `BtlStage_UpdateObjs`, ambience, `StgModel_UpdateAnims`,
  `StgRigid_Update`. Not while paused.
- Debris bodies are visual only. The stage creates them without a hit buffer, which disables
  their triangle collection, contact forces, under-water drag and rest test: a body is a sphere
  in free fall with a small drag, kept inside the stage cylinder horizontally.
- They are stepped 10 fixed sub-steps of (1/60 + 0.00001) s per frame, i.e. 0.167 s of physics
  per 1/30 s frame, independent of real frame time.
- Each launched body draws 6 libc `rand()` and 4 to 7 VU0 values, so breaking a stage object
  advances both shared streams by an amount that depends on the libc bits.
- The object really starts at 0x22FC40 (two list helpers, still unnamed).

## Stage collision (simulation; `eft_det_b.c`, verified unless marked)

- **Data**: each zone has one static collision mesh and a list of records for destructible
  objects. An object has a "whole" record, tested while unbroken, and a "remains" record,
  tested once broken: breaking needs no collision update (correcting the stg_a note). Objects
  touched come back as a 64-bit mask (at most 64 colliding objects per stage); callers take the
  lowest set bit. Polygon flags mark non-solid polygons and ones the camera or shadow skip.
- **Fighter against the stage** (`StgCol_UpdateFighter`, once per fighter per frame, fighter 0
  first): the body sphere is swept from last frame's position in steps of half a radius; after
  each step every solid triangle touching it pushes it out, in mesh-walk order (the result
  depends on that order). Only the fighter's current zone is tested, never neighbours. The
  collision radius grows by 1 per frame up to the body radius. Contact bits last one frame.
- **Breaking objects by flying through them**: in each sub-step the lowest-index unbroken
  object touched is considered. With `BtlCharApi_TestStageBreakA` true it breaks only if its
  type has bit 0x100; otherwise it breaks if `TestStageBreakB` is true. Breaking sets contact
  bit 0x40, and the object's type picks the damage bit: 200, 600 or 1000 (applied by
  `BtlColl_UpdateGround` only while fighter flag 0xA is up).
- **Order dependence**: if both fighters would touch the same object in one frame, fighter 0
  breaks it and takes the damage; fighter 1 collides with the remains and takes none.
- **Segment trace** (`StgCol_TraceSegment`: lock-on sight, AI, approach point, sweeping beam):
  walks zones from the start point; the first zone with any hit ends the walk; the result is in
  one static block (not re-entrant, and overwritten by the debris update). (inferred hazard)
  The object index returned is the lowest-index unbroken object accepted as "nearest so far"
  during the walk, not necessarily the object of the final nearest hit; the sweeping beam
  destroys that index.
- **AI way-point graph** (`StgNav_*`): a per-stage graph of 0x30-byte nodes; path search by
  fewest links, at most 16 points. Original bug: sibling entries get increasing step counts, so
  it is not a true breadth-first search (still deterministic). Hazard: open and closed lists of
  256 entries with no bound check. The search ignores destructible objects.
- No random draws; no camera, pad or screen-mode input.
- Ring-out is decided in `BtlColl_UpdateGround`, not here.
- (verified, eft_x) Scene layer 3 is the root of the 31 "common effect" modules (table
  0x2C3FB0 of `{class, 1}` pairs, created in table order from pool slot 1).
- (verified hazard, eft_x) `EftQuad_Update` computes `age % interval`: a definition with
  interval 0 traps.

## Projectile hit detection (simulation; `eft_det_a.c`, all 40 functions match)

`EftDet_Update` runs between `BtlScene_Update` and `BtlScene_PostUpdate`, not while paused:
prepare every record, then record against record (clash), record against stage, record against
fighter. Every pass walks the list in creation order. `EftHit_CanHit` mode 0 = stage, 1 =
fighter, 2 = clash.

- **Results** go to the owning task through `EftHit_SetTaskFlag`: 1 hit a fighter, 2 guarded,
  4 hit the stage, 8 lost a clash, 0x20 absorbed, 0x40 deflected, 0x80 reflected, 0x100 beam
  struggle, 0x8000 multi-hit still in contact. (These correct the flag names in eft_a.h.)
- **Shapes**: type 0 is a sphere swept from the previous to the current position. Type 1 is a
  capsule: against fighters and other records the whole beam from the muzzle is tested every
  frame; against the stage only the newly covered piece. Types 2..6 never hit anything.
- **Against a fighter**: each record is tested against exactly one fighter, owner id ^ 1 (the
  code assumes object ids 0 and 1). The swept sphere is tested against every body part and the
  earliest contact wins. A fighter contact further away than the same record's stage contact
  is dropped, so walls shield. Then the fighter answers in this order: dodge, deflect, reflect,
  absorb, guard, hit. Multi-hit techniques hit at most once per interval frames of contact.
- **Order**: there is no fighter loop; the outcome depends on record order only (each record is
  fully resolved, fighter state included, before the next).
- **Clash**: all pairs with different owners; the earlier record of a pair can clash with
  several later ones in one frame. `EftHit_Clash` is called for every eligible pair, touching
  or not, and its side effect (stopping both technique timers for a head-on pair) is not gated
  by contact. The beam struggle starts only with line of sight, battle sequence state below 4
  and no struggle already running; it sets held flag 0xAA on fighters 0 and 1 by literal id.
- **Against the stage**: only the record's own zone is searched. Ki blasts do 1 damage to a
  stage object, techniques 999999. The object damaged is the lowest index touched, not the
  nearest. A projectile that breaks an object passes through it that frame. A stopping hit
  shakes nearby cameras and rumbles for techniques.
- No camera, view, screen mode, pad or sound input; no random draws in the detection.

### Fighter strikes (`BtlBodyHit_Update`, before `BtlColl_Update`)

Each fighter's attack spheres and volumes are tested against the other's body parts; a contact
sets bits in both objects' contact words (bits 24..31). It only sets bits, so the order of the
two tests does not matter. This is the geometric test behind the melee hit code in combat.md.

### Ground probe (`StgGround_Probe`)

The nearest upward-facing solid triangle under a point and below a height limit, searched in
one zone only; no result leaves the height at FLT_MAX. Each fighter probes once per frame and
may break a stage object it is inside.

### Volley aim (`EftVolleyAim_*`): the scene generator in the simulation

Each volley shot's direction is spread by pattern kind (cone, fan, alternating, ring, circle,
lob ...) with 0 to 3 draws from the scene generator at fire time, in shot creation order;
lobbed shots draw 3 or 4 more on one scripted frame for their target offset. The spread also
depends on the distance between fighters 0 and 1 (by literal id) and on the shooter's
altitude. Steering leads the target with the opponent's frame movement, so it depends on
whether the opponent has already moved this frame.
- (verified, eft_aa) The blinding overlay is display only: fighter +0xFFC / 90 becomes the alpha
  of a grey sprite; its getter has no other caller. It is keyed on object id 0: in single
  screen an object-1 blind is never shown, so a port where the local player is not object 0
  must change it. Not drawn in replays.
- (verified, eft_aa) 0x19B7F8 starts delayed sound tasks, not sparks (correcting eft_j / eft_l
  comments). Part kind 14's timers run in the update and feed `EftEmit_UpdateAlive`.

## Blast objects (simulation; `eft_o_b.c`, verified unless marked)

A blast object is a task with a 0x620-byte work block, in one list of 60 shared by both
fighters. Its creators (volley, shots, ring shot) keep the task pointer as a handle.

- Creation: position, direction, speed, scale and life (seconds x 30) from the argument; the
  previous position starts as the owner's node 0x11, so the first hit shape reaches back to
  the owner; delay starts at 1 frame.
- Each frame, unless the owner is stopped: refresh node slots; move (placed by the owner, or
  optional homing then position += velocity) unless delayed, held by the definition, stuck or
  frozen; count the delay and the life; the owner's ABORT event stops it; model and parts; then
  die, or publish one technique hit record (two spheres at position and previous position, or
  two boxes from the start point; radius = scale x the pack's trail width animation).
- Hit results from the task: bit 0 snaps the shot to the reported position and it never moves
  again (but keeps publishing records until its life ends); bit 2 stops it. A stopped shot
  publishes nothing on the next update and is killed on the one after.
- Entry points the creators use: stop, set target (the owner writes the position every frame),
  set previous position, set direction, freeze, no-hit, "held" (only sets record flag 0x80),
  mark last, set delay.
- For two specific volley techniques the shot's model animation releases it: one animation
  step clears "no hit" at the owner's FIRE event and the next clears "frozen" when it ends, so
  a model's animation length is simulation input there.
- Oddities: `SetDir` does not recompute the velocity (only the homing branch does); a handle is
  validated by task class only, so a stale handle to a reused slot passes.
- No random draws. Records are appended in task list order (creation order across both
  fighters).

## Ki blasts (simulation; `eft_r.c`, verified unless marked)

- **Launch**: `BtlFx_FireKiBlast` in btl_char_fx.c is the ki blast launcher (its name is
  wrong). On animation event 4 it fires the blast kind's shot count in one frame. In spread
  mode 0 it draws three values from the fighter generator per shot to jitter the direction.
  By blast type: plain blast, type 2 thrown object, type 3 bomb, types 4 / 5 discs.
- **The 0x50-byte launch block** (the same block the hit record's `atk` points at): direction,
  firer id (never changes), owner id, muzzle node, kind 0..12, type, lifetime in frames, speed
  per frame, homing turn per frame, radius, charge level 0..3 (= the clash level).
- **Plain blast**: starts at the muzzle node with "previous position" at the owner's node 0x11.
  Homing is enabled only if the owner is locked on at launch. Per frame: homing turn, move,
  lifetime, hit record (two spheres). Lists hold 20 plain blasts and 10 of the other types per
  character; (inferred) a full list drops the shot silently.
- **Freeze**: blast updates stop while paused, while any fighter is in hit-stop, or while
  either fighter is in a rush sequence.
- **Hit results**: hit, guarded, stage, lost a clash, left the stage, absorbed: the blast snaps
  to the reported position, stops and dies. **Deflected**: new direction from
  `BtlCharApi_GetDeflectDir` (two fighter-generator draws), owner flips, homing off; it can
  never hit again after a second deflection. **Reflected**: the same, but the lifetime
  restarts and homing now steers at the original firer; it can be reflected again.
- **Bomb (type 3)**: one shot creates two bombs, thrown with jitter from the VU0 generator,
  gravity 0.5 per frame, up to three bounces with speed halved; it explodes after 300 frames,
  near the opponent, on a hit, or once at rest; the explosion is a radius-28 hit record for 15
  frames.
- **Live blast count** (`BtlMove_CanFireBlast`): counts type 0 records whose original firer is
  the fighter, so a deflected blast still counts against its firer. (inferred hazard) Only
  blasts that published a record this frame count, so during hit-stop the count reads 0.
- **Beam struggle**: each unpaused frame the struggle point is placed between the two beams by
  the clash bias and written into both beam tasks' hit position. When it ends, the loser's
  technique timers expire and all of the loser's effect tasks are reset, live ki blasts
  included.
- Order dependence: owner flip is `id ^ 1`; when several blasts are deflected in one frame the
  fighter generator's sequence follows hit-list order.
- (verified, eft_v_b) The impact effect a projectile starts when it hits is purely visual: it
  plays one emitter set of the common pack at a point and writes nothing else. Explosions do
  no damage of their own.

## Effect model objects (verified, eft_ad_b)

An effect model (prop shot, object technique, shots with a model, rush shot swarm) is a battle
object of type 1 made by `BtlObj_Create` from a model already in memory inside the effect
pack: no file request, created in the item task's init and destroyed in its term. It is posed
with a world matrix, shown or hidden with one flag, and can play model animations.

- The object table has 12 ids with a FIFO free list. (inferred) Fighters hold 0 and 1, so
  effects share 10 ids, handed out in rotation; assignment follows task creation order.
- **Hazard**: nothing checks a failed create. Id -1 is then used for show / hide and pose,
  which writes through the table entry before the first one. One technique type can ask for 14
  models against 10 free ids. A port must guard it.
- `btl_pool.c` (linked, 0x1A7018..0x1A7608) sits inside this address range.
- (verified, eft_z) The ground impact a projectile starts (0x1975A8) is visual only: one dust
  task, two particles. With the impact effect (eft_v_b) this settles that nothing a projectile
  spawns on impact affects the fight.
- (naming, to unify at integration) Part kind 16 is called `EftLine_*` in eft_y and `EftBill_*`
  in eft_z; it is one module (and one source file: `EftBill_SetTexture` needs 0x195038 in its
  file).
- (verified hazard, eft_z) The dust module's per-fighter emitter slots are indexed by object id
  without a range check and hold two entries each.

## Discs (simulation; eft_o_c.c + eft_p.c, verified unless marked)

A spinning model projectile: ki blast types 4 / 5 (held in the hand, then thrown) and the
pieces of the multi-piece technique. One hit record per flying frame (two spheres). Ki blast
disc: radius = scale x 4.5. Technique piece: radius = the pack's trail width x 3.

- (inferred, from the update's disassembly) The libc `rand()` value drawn for a piece of
  technique 0x202, and at disc creation, only sets the model's roll: appearance.
- Hazards: held discs are kept in per-fighter lists indexed by object id 0 / 1; the term
  callback pops the list head whichever disc ends; ki blast parameters are copied into the
  fighter's first listed disc. (inferred) A second disc created while an older one lives keeps
  zeroed attack parameters: a possible original bug for overlapping disc blasts.
- (inferred by search, eft_n) The aura's sparks are spawned and stepped (consuming `rand()`) but
  their draw function has no caller, so they are never shown. Original bug in the lightning
  draw: one depth term uses the wrong vertex component.

## Collision primitive library (`col_a.c`, 0x230B38..0x236190; verified unless marked)

26 of 27 functions match; the 27th (`ColObb_Contact`, 0x4258 bytes, 77% of the range) is dead
code with no caller anywhere and stays in assembly. Not linked yet. Everything is compiled FPU
C: no VU0 code in the range, no random draws, no inputs. The object probably starts at
0x230B10 (`StgAabb_SetEmpty`).

- **Mesh format** (stage collision): one base pointer for all meshes, set at stage init. A mesh
  header gives word offsets to nodes, polygons and vertices. Nodes are 0x20 bytes: a box, then
  `left` and `right`; a leaf has `right == -1` and `left` = polygon index; node 0 is the root.
  Polygons are 0x20 bytes: flags, three vertex indices, plane. Vertices are 16-byte vectors.
- **Walk order** (`ColMesh_WalkBox`): depth first, `left` before `right`, subtrees pruned by a
  closed box overlap test, every overlapping leaf reported to a callback, results OR-ed, no
  early out. This is the "mesh-walk order" the fighter push-out depends on. The walk keeps its
  state in globals (not re-entrant) and recurses with no depth limit.
- **Box tests** are closed and tolerance-free (touching counts; a NaN never separates). The
  exception is `ColBox_ClipRay`, which accepts an entry point up to 0.001 outside a face; its
  ray is unbounded.
- **Oriented boxes** (`ColObb`, 0x190 bytes): the fighter body and attack volumes. Overlap is a
  fifteen-axis separating test with no epsilon; the exact operand order is in the C. One
  caller: the melee volume test.
- `ColBox_AddPoint` writes w = 1 into the point it is given (a side effect on the caller's
  data).
- Original bug in dead code: `Col_LineLineParams` has inverted guards and returns 0, 0 for unit
  directions.
- (read from callee disassembly) Dot products go through hand-written VU0 routines with the
  order (x*x' + y*y') + z*z'; a port must reproduce that order.

## Scene layers, complete picture (verified)

| Layer | What | Simulation? |
|---|---|---|
| 0 | stage effect manager and its per-stage kinds | no |
| 1 | shot layer: technique events, shot slots, technique effect types | yes |
| 2 | per-character effect root: one task per character with one manager per effect kind from table 0x2C3BE0 (0 charge, 1 / 2 ki blast, 3 blast charge, 4 shock, 5 rush burst, 7 body effect, 9 absorb) | holds the ki blasts |
| 3 | root of the 31 common effect managers (table 0x2C3FB0) | holds blast objects, discs, beam struggle |
| 4 | stage-change transition | timing only |

Which kinds a character gets comes from its character pack; type 2 / 3 ki blasts replace the
class of kind 1 / 2. When a character is streamed out, its layer-2 task is killed and
`BtlScene_Reset(2 or 3)` runs, (inferred) ending its effect tasks, ki blasts in flight
included.

## Collision primitives, second file (`col_b.c`, 0x236190..0x239BB0; all 42 functions match)

Stateless geometry: no globals, no static buffers, no random draws, no inline VU0. Not linked
yet. Exact contracts for every function are in `include/battle/col_b.h`; the ones the
simulation uses:

- **Shapes**: sphere = centre (w = 1), radius, radius squared. The hit record "box" is a
  capsule: two points and a radius. A sweep = a capsule plus its start sphere, delta, unit
  direction and length.
- **Projectile against fighter or projectile** (`ColSphere_SweepSphere`): relative-motion
  quadratic; an overlap at the start returns t = 0 (exact touching does not count); relative
  movement below 1.1754944e-36 (squared) returns no hit; t is not clamped (callers treat
  t <= 1e-6 or t >= 1 as "use the static test"). Static test: `ColSphere_TestSphere`
  (inclusive). Resolution: `ColSphere_SeparateSphere` (0.001 tolerance, 0.001 extra push).
- **Projectile or camera against the stage** (`ColSweep_TestTri`): back-facing or parallel
  triangles (dir . n > -0.001) are ignored; face, then corners, then edges; corners replace the
  best hit only if strictly nearer, edges also on a tie; the distance is not limited to the
  sweep length.
- **Fighter body against stage triangles** (`ColSphere_TestTri`): Magic Software
  point / triangle squared distance compared with the sphere's stored radius squared;
  degenerate triangles never hit.
- **Segment trace** (`ColSeg_TestTri`): front faces only, by the geometric normal, inclusive
  bounds, returns the fraction along the segment.
- **Fighter body volume** (`ColSphere_ContactObb`): sphere against an oriented box with a
  face / edge / corner classification. **Original bug, to be reproduced by a port**: only the
  x face returns a correct normal; the y and z faces return a vector built from one axis
  component, and the y face uses the wrong sign variable. It feeds the melee volume test.
- "No hit yet" is 0x7F7FFFFE, one bit below FLT_MAX.

### Vector routines (read from disassembly; hand-written VU0, at 0x121E28..)

A port must reproduce these exactly: `Vec3_Dot` = (x*x' + y*y') + VF3.x * (z*z'), where VF3 is
(1, 0, 0, 0) set once at boot; `Vec4_Add` / `Vec4_Sub` work on all four components (the
difference of two positions has w = 0); `Vec3_Normalize` takes a VU0 square root, then a VU0
reciprocal, then multiplies, and returns w = 0; `Vec3_Cross` gives w = 0. Several primitives
compute some dot products in plain FPU instead: which path each sum takes is fixed by the
matching C. `sqrtf` is the inline FPU instruction.

0x239BB0..0x239EA0 (`col_b_b.c`, 5/5) is the head of the text printer (see graphics.md).

## The effect task tree (verified, eft_ae)

- A task is 0x40 bytes: flag byte (bit 0 dead, bit 1 updated), index, tag word (+4), position
  (+0x10), list, child list, class, links, work pointer. A class is
  `{update, init(task, arg), term, postUpdate, reset, draw}`. Lists have a fixed task count and
  work size, allocated from the current battle pool.
- **Update order is list order, depth first; `AddTail` appends, so creation order is update
  order.** A task added after the current one during an update is updated in the same pass.
  Dead tasks are removed by Update and Reset only.
- A task is drawn only after it has been updated once.
- `BtlTaskList_Reset(list, mask)`: 0 resets every task; 0x800 tasks of object id 0; 0x1000
  tasks of any other object; 0x2000 a third group. The bits are in the task's tag word, which
  also receives the hit pass's result bits.
- (inferred hazard) `BtlTaskList_Update` saves the next pointer before updating a task's
  children; if a child kills its parent's next sibling the saved pointer is stale.
- Effect textures get VRAM per frame, in call order, from two cursors (128 textures at most);
  `BtlScene_Draw` uploads them and rewinds. Visual only.

With this file the whole effect range 0x12DD80..0x1B4140 has been decompiled.

## Stage model drawing (0x115478..0x116B98; src/battle/stgm_a.c, linked; names in config/symbols/stgm_a.txt)
Linked layout: `.rodata` 0x2EB9A0 (0xE0), `.lit4` 0x2FC2FC (one word). `gStgAnimTable` (0x2FE914) is defined in
stg_d_b.c (its object's only `.sdata` word).

All 13 functions match. Tail of the stage model code that starts at 0x114C60 (stg_d_b.c is
the file just before, not btl_seq.c); final name stg_model_draw.c. Layouts in
include/battle/stgm_a.h. **Visual only**: no pad, clock or random draw; nothing fight code
reads.

- (verified) Per view: `StgModel_Cull(view)` (0x115950) then `StgModel_Draw(view)` (0x115DE0),
  called by the battle draw and by the menu character viewer.
- (verified) Visibility is one byte per mesh, rebuilt for every view from an eight-child
  culling tree (cell half size = 2 x root size / 2^depth), plus an always-drawn part list,
  plus the pieces of falling objects. (inferred) frustum test result 0 / 1 / 2 = inside /
  crossing / outside.
- (verified) Seven draw groups, in the order: 0 (no depth writes), scrolling stage effect
  (`EftStageScroll_Draw`), 1, 2 (stage proper), animated objects (not culled; posed from the
  definition's position and rotation in degrees, ZXY; VU1 program 8), 3 / 4 / 5 (blended),
  6 (debris, VU1 program 7 with per-mesh alpha). Stage id 8 draws the debris before the
  blended groups; stage id 5 skips group 3.
- (verified) Debris alpha = `Stg_FadeByCamDist(pos, Stg_FadeRatio(frame, animEnd))` x 128,
  capped at 0x80. Stage textures upload one at a time to GS block 0x2A00 / CLUT 0x32C0.
- Original quirks: material texture animations advance once per `StgModel_Draw` call, so
  twice per frame in split screen; back-and-forth mode with one frame reads `frames[-1]`.

## Effect key tracks (part kinds 10 and 15; verified by matching C, 2026-10-05)

An emitter definition is two blocks (the pack entry's two resources). Every animated quantity
is a TRACK of three keys (start, middle, end), stored track by track: scalar `f32 k[3]`
(12 bytes), three-axis `f32 k[3][3]`, two-value `f32 k[3][2]` (a 2-D array, not an array of
pair structs), vector `Vec4 k[3]` (16-aligned). Most quantities come as a (value, range)
pair of tracks (the range reading is inferred from the spawn code's `Rand_FloatRange`).
Layouts: `EftPart10Def` / `EftPart10Def2` in include/battle/eft_x.h, `EftLinkDef` /
`EftLinkDef2` in include/battle/eft_v.h (member lists with offsets there; the second block
has the same 0x12C-byte shape in both: four Vec4 colour tracks, three two-value tracks,
three scalar tracks).
Runtime (same algorithm in both modules): `keyTime = def.keyTime * 30` frames,
`keyMid = keyTime * def.keyMid`; segment 1 runs from age 0, segment 2 once `keyMid <= age`
(a KEY2 flag); starting a segment stores `delta = key[s] - key[s-1]` per track; each frame
`cur = key[seg] + delta * t` with `t = age / keyMid` or `(age - keyMid) / (keyTime -
keyMid)`. Part kind 10 clamps t to 0..1, kind 15 does not. Original quirk: kind 10 scales
colours with `Vec3_Scale` then `Vec4_Add`, so the w of a colour takes whatever the scale left
in its temporary. `SetKey(i)` copies key i of every track without interpolation.
