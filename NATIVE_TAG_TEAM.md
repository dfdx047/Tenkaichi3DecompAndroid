# Native Tag Team prototype

This native prototype is based on
[Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp), at commit
`ec3c85a98cffd9cd22c22ac14cd62fb584d23664` (upstream 0.1.37).

The game loads and updates up to ten real fighters, with their own models, AI,
health, animations, effects and audio. Two, three or four local players have
separate views with the original game's health/ki panels. Teammates can revive
one another. It runs as a native Windows or Linux program.

**This is not the complete Tag Team Mod.** Treat it as a playable engineering
prototype. It is maintained separately from the PCSX2 installer.

## Playing

For the online prototype with up to ten players, CPUs and read-only spectators,
see [Native Tag Team Online](NATIVE_TAG_ONLINE.md). Its BT3 and BT4 launchers are
**Play native online.cmd** and **Play native BT4 online.cmd**. It is also available
from **Select > Modded Online** in the normal game menu. **Select > Online Lobby**
opens the persistent room: guests practise while the host chooses modes, then
everyone stays connected through team setup, matches and returns to practice.

Open **Play native prototype.cmd** in this folder. At the main menu, press
**Select** to open Modded Modes. Team Battle, Free For All, Modded Training,
Gauntlet and World Tournament,
player counts, team assignments and Mod Settings use the original animated
game menu and lettering. Character, outfit, equipment and stage selection
then use the game's normal screens. It uses your imported disc resources and
does not launch PCSX2. See [Building](docs/NATIVE_BUILD.md) for initial setup.

For the English **BT4 B14 REV2** disc, open **Play native BT4 prototype.cmd**.
It uses the same native engine and Modded Modes, with BT4's roster, costumes,
stages, music, voices, portraits and fusion recipes. Its saves and settings are
separate from BT3, under `port/build/native-bt4-prototype-saves` and
`port/build/native-bt4-prototype-settings.txt`. It does not modify either ISO.

BT4's outfit selector uses its own stacked number plates, arrows and outfit
names, including characters with more than four outfits. Battle portrait faces
use BT4's frame offsets. Planet destruction follows BT4's replacement-stage
table and places the roster on safe terrain after the shared cinematic.

The local outfit sweep loads all 1,510 selectable intact/damaged outfit cases
into short actual battles. It checks the loaded model identity, not just the
menu selection. Native effect allocations also cover ordinary duels, where
BT4 costume emitters could overflow the original fixed arenas. One B14 outfit
has corrupt body-volume data; a fingerprinted in-memory repair restores the
shape exactly to its intact counterpart on the same disc. Disc files stay
unchanged. These checks do not replace playing through every character's moves.

The following disc imports are supported locally. Each uses only the assets
from that disc; Europe and Japan do not need a second USA disc.

| Disc | Local launcher | Private data folder |
| --- | --- | --- |
| BT3 USA, SLUS-21678 | Play native prototype.cmd | `gamedata` |
| BT3 Europe/Australia, SLES-54945 | Play native BT3 Europe.cmd | `gamedata-eu` |
| BT3 Japan, SLPS-25815 | Play native BT3 Japan.cmd | `gamedata-jp` |
| BT4 B14 REV2 English | Play native BT4 prototype.cmd | `gamedata-bt4` |
| BT4 B14 REV2 Spanish | Play native BT4 Spanish.cmd | `gamedata-bt4-es` |

To import your own disc, install `pycdlib` for Python and run the shared importer,
using the matching data folder from the table:

```text
python -m pip install pycdlib
python port/tools/import_native_disc.py "path/to/your-disc.iso" gamedata-eu
```

Use a new or empty output directory. The importer checks the executable and
menu layout before extracting; unknown revisions still require their own
profile. It leaves existing data alone. Imported assets are private and must
not be included in source or release packages. These launchers keep separate
saves and settings for each new disc import under `port/build`.

Choose **Español** on the first language screen, or use **F1 → UI → Language**
and restart. The mod's modes, team setup, settings and battle indicators use
Spanish. The European disc supplies its original Spanish menu and HUD assets;
Spanish BT4 uses its own artwork and character names, and initially highlights
Spanish in the language picker. The Japanese disc retains its Japanese game
assets and voices; both voice-language choices use its sole Japanese voice set.
Changing the interface language does not add voice recordings to a disc.

Disc support does not bypass online game-data matching: players still need
matching gameplay resources. Different regional imports are not automatically
interchangeable in one online match.

The default keyboard binding for Select is **Right Shift**. Setup and Mod
Settings stay on the original animated main-menu screen, with its scrolling
panels, highlighting and menu sounds. In team setup, directions move between
player and rule cards. Press **Cross** to edit a card, use Left/Right to change
its value, then Cross to confirm or Triangle to cancel that edit. **Start** or
**Choose Fighters** continues. Choose up to five fighters on each side in the
normal character screen; choose **End** whenever that side is complete. A 4v2
match simply means selecting four fighters, then two. Unclaimed fighters are
CPUs. Each team needs enough slots for its assigned players. Free For All treats
every selected fighter as an opponent and scatters their starting positions
across the arena, with terrain clearance and space between fighters. Tournament
stages keep those starting points inside the ring. Triangle goes back, and Select at the
main menu returns to normal modes.

The battle modes also offer **CPU Only**. Choose both rosters normally, then
watch their fight. The team setup screen includes **Duel Time** (60, 90, 180,
240 seconds or no limit) and **COM Level** (Very Weak through Very Strong).
COM Level supplies the default difficulty. After choosing a CPU fighter's
costume, choose its individual difficulty on the next page; human fighters skip
that page. These choices remain in effect for Fight Again and
returning to character selection. They do not overwrite the base game's saved
battle rules. Training starts with no time limit but can use a chosen limit;
health recovery, full ki and full blast stocks are separately configurable.

**Gauntlet** offers the authored BT3 and BT4 wave presets. Choose your team,
arena and health refill between waves. The Music card in team setup defaults to
Random; select it with Cross, change the song with Left/Right, then confirm with
Cross. Your song stays through the waves and rematches. The countdown replaces
the battle timer; you can move during the interval without repeated lock-on
searches. Defeated opponents remain visible for the first three seconds of the
break, then disappear before the next wave's models load. They cannot be
targeted or revived during the break. Each new wave gets voiced fighter entrances while the other fighters
pause. Press Start to skip them. Clearing the gauntlet shows each wave's time
and damage taken.

**World Tournament** uses the original tournament bracket, announcer, crowd
introduction and fighter entrances, with whole teams fighting together. Enter
up to four player teams; other entrants are generated CPU teams. You can watch
or skip CPU-only matches. Results return to the bracket for the next round.
Rounds have no duel timer. Ring-outs leave that fighter permanently down;
remaining teammates continue, and players can take over a living CPU teammate.

**Combat → Clash Button Prompts** optionally replaces mashing and stick spinning
with successive button prompts. A correct press immediately shows the next
button; a wrong press briefly blocks scoring. It is off by default and applies
to beam, melee and vanish clashes.

**HUD** has separate switches for the combo hit counter, damage counter and
the entire base-game HUD. Mod indicators retain their own switches.

- **F1:** the native port's graphics and controller/keyboard bindings.
- **C / L3:** tap to target someone attacking you, even with right-stick targeting
  enabled. If nobody else is attacking, select a target nearest the centre of
  your view. Hold for half a second to lock off. Downed teammates
  can also be targeted while revival is enabled. Cinematic participants retain
  their targets until the move finishes.
- **Flick the right stick:** switch targets in that direction on your screen,
  following the camera instead of the order in the roster.
  Targeting and Views also offers requiring L3 while flicking, or disabling stick
  targeting. While locked off, the stick remains available for the camera.
- **Enter / Start:** pause.
- **Esc:** the native port's exit confirmation.

The launchers use their own settings and saves under
`port/build/native-prototype-settings.txt` and
`port/build/native-prototype-saves`. Controls can be rebound in F1.

**Watch native 2v2.cmd** starts an all-CPU fight and closes when it finishes.
**Play native split-screen.cmd** opens the same menus. Select two, three or
four players there, then assign them to either team. Bind each player's keyboard/controller in F1;
Players 3 and 4 inherit the in-game button layouts of Players 1 and 2 respectively,
but have independent physical-device bindings. Three-player matches default to
two views across the top and a full-width Player 3 view below. **Mod Settings →
Targeting and Views → Three Player Layout** also offers the previous centered bottom
view, the same size as each upper view.

**Controller Setup** on the team screen asks each player to press Start in
player order. It prevents one controller from claiming multiple seats, names the
connected devices, and keeps assignments if a different controller disconnects.
Select leaves a seat; Player 1 confirms with Cross. Keyboard players can join
using their individual Start bindings, configured in F1. The check runs before
multiplayer character selection by default; its requirement and remembering
assignments for the current session are configurable.

The launchers hide the upstream online menu and disable its update check for
this experimental build.

To revive, stay inside a downed teammate's ring with two blast stocks for three
seconds. Moving and blocking are allowed; unblocked damage or leaving the ring
interrupts revival. Ring-outs are permanent. Mod Settings lets you change the
revival time, cost, range and restored health, plus the range ring's visibility.
The revival meter uses the same beveled native artwork as the fusion and
overhead health meters. **HUD → Styled Battle Indicators** switches between
that style and simple bars. The relevant settings show live examples in both
the game menu and F1.

**Cinematics → Keep Moves Nearby** stages attacks and transformations near
their starting location. Paired moves share one placement offset, and their
release positions are checked against the arena, floor and nearby walls.
When the local space cannot safely fit both fighters, the move uses its
original staging. This setting is off by default. Ultimates share their camera
and pause the other fighters by default; other cinematic contexts have their
own camera choices. Ordinary specials leave the other fighters visible and
moving unless their shared-camera option is enabled. The Special Move Pause
setting applies to those shared cinematics; normal impact hit-stop is preserved.

**Combat** includes beam assists and their power/range controls. Helpers move
beside the actual clash participant with clearance for both bodies and nearby
fighters. **Outnumbered** offers Balanced, Strong and Custom assistance;
it is off by default. **HUD → Enemy Attack Indicators** is also off by default.
It marks opponents targeting you and flashes while they attack.

A knocked-out player can press Square to take over a living CPU teammate.
While spectating after a KO or in a CPU-only battle, **L3** switches the watched
fighter across either team. The original health/ki panels follow that fighter.
Square takes over the watched CPU teammate only; it never takes an enemy or
another human's fighter. CPU-only battles remain CPU-only. Charging and ordinary
attacks do not prevent takeover, while paired cinematics and transformations
do. A revived player automatically returns to their own fighter. Watching
fallen fighters and displaying spectator hints are optional.
Mod Settings also has controls for ring-outs, CPU transformation likelihood,
fusion availability, the enemy HUD while locked off, and training recovery.
Camera options separately control shared views for ultimates, rushes, grabs,
ordinary beam/projectile specials, beam/melee/vanish clashes and transformations.
Only ultimates, transformations and beam clashes share everyone's view by
default. Normal combat camera distance and
special-move pausing are configurable too. The side ally-health list defaults
off; overhead health and the target marker have separate controls. These
settings are also available in F1.
The target marker can be an arrow, ring or corner reticle.
Corner player labels default off. Overhead health bars grow gently as fighters
come closer, and the extra battle indicators hide while the game is paused.

Fusion uses the original paired dance/Potara animations and native form loader.
Use the normal fusion input with a compatible living teammate; a human partner
accepts with R3, while a CPU partner accepts automatically. Human partners
take turns controlling the fused fighter every 20 seconds by default. The
Fusion settings page changes the interval or disables switching. The HUD shows
who has control, who takes over next and the remaining time. The handoff clock
stops during pause and special move cinematics. A human/CPU pair stays under
human control. Fusion Dance has an optional time limit
(80 seconds by default); Potara stays fused. Timed separation uses a native
power-down, restores both fighters' stats and controllers, and places them apart
inside the arena. Health is divided proportionally and spent stocks stay spent.
Temporary dance fusions show a remaining-time meter by default; permanent
Potara fusions do not. The meter, control indicator and countdown have
independent visibility settings.
In-battle mod text is doubled in size, with spacing to match. The fusion meter
is thicker while keeping its original length.

Fused human players share adjacent views. Two-player split becomes full screen.
In four-player split, P1/P2 or P3/P4 occupy a full row; P1/P3 or P2/P4 occupy a
full-height column. Diagonal pairs keep duplicate views. In three-player split,
the top pair shares the top row; a top/bottom pair gets a full-height column
beside the remaining player. Defusion restores the chosen original layout.
Native HUD panels, prompts, borders and world rendering follow these rectangles.

The CPU demonstration uses the native attract-demo roster plus additional
fighters. The normal Play launchers let you choose a roster. Player 1 can always
use Triangle to go back during selection, including while another player is
choosing. Moving over another player's slot does not transfer the selecting
controller. An outlined label above the portraits identifies the selecting
player without covering the character grid. The complete roster is loaded and constructed during the original
game loading screen; there is no separate preparation screen.
The same fighter can be chosen more than once on a team.

CPU target choices use the PCSX2 mod's scoring: recent attackers, distance,
opportunities and how many other CPUs are already engaging each opponent.
CPUs commit to a target for at least two seconds while it remains valid, then
reconsider periodically or in response to damage. Grudges fade over time.
Target changes wait for queued damage, grabs, clashes and transformations to
finish. CPUs never target allies, and this policy never changes a human
player's chosen target.

## What has been exercised

The actual Windows and Linux executables have loaded and completed CPU matches
in these configurations: **2v2, 1v2, 2v1, 1v3, 3v1, 4v2, 2v4, 5v5, 5v1 and 1v5**. The original 1v1 demo
also completes without enabling Tag Team. These runs exercise the real disc
loader, combat code, AI, damage, timer and battle cleanup; they do not replay
emulator savestates.

The ten-fighter work expands model, shadow, effect, audio, file-request and display-list
capacity. Fighter contacts use their own table instead of a packed four-slot
mask.

The Windows Vulkan renderer has drawn two-, three- and four-view matches with
complete fighter models and the original health/ki panels. The native
scripted-input path has cycled targets and triggered attacks independently on
all four pads. Ready/Fight renders over the split borders. This is not a
four-controller human playtest or a claim that every move works.

The controller check-in test uses four SDL virtual controllers in shuffled order,
then checks duplicate joins, leaving/rejoining, button and stick routing,
disconnects, and cancellation. A native grab test catches someone who had a
different opponent selected, checks both participants' camera ownership and the
shared-camera toggle, and follows them back into normal play. Spawn checks cover
all pairs in a ten-fighter match. The full-width bottom view has also been drawn
in Vulkan with the native HUD; stage and effect clipping follow each viewport's
actual projection.

Camera-following weather particles have separate banks and camera tracking for
every view. Drawing Players 3 and 4 no longer advances Player 2's canyon dust
again. Each bank moves once per game update and keeps its own particles while
paused. The GPU regression in `port/tests/tag_weather_runtime.py` checks
three- and four-player matches through pause and resume.

The Windows and Linux fusion fixture runs the actual Goku/Vegeta dance, model
change, consent, timed power-down and separation across every four-player pair,
both initiating-seat orders, and two-/three-player layouts. It checks restored
view ownership, placement, health limits, resources, alternating physical-pad
input and restoration of each original controller. The geometry test also
covers two simultaneous fused pairs and both three-player layout settings.
The real Vegito/Potara case stays fused beyond the dance time limit, switches
controllers, and respects turning automatic control switching off and on.
This is not yet a human check of every native fusion character or costume.

Scripted controller runs also open Players 3 and 4's own skill lists and resume
the match independently. Team Battle, Free For All and Modded Training have
been launched through the native selection screens; pause-menu exits return
to character selection or Modded Modes. These checks are in
`port/tests/tag_menu_runtime.py`.

The revised selector has also launched 4v2 and 5v5 rosters, including the normal
form picker and optional End entries. All ten selected fighters receive their
own entrance, and both pause-menu destinations have been checked again.

A four-fighter Windows Vulkan loading comparison on this PC reached the stage
intro in 1.81 seconds, versus 4.88 seconds with the original file-queue cadence.
That includes the normal loading fade and actor creation. It measures the start
of the stage intro, not the end of the voiced fighter introductions, and will
vary by computer. `port/tests/tag_loading_runtime.py --renderer gpu` repeats
that comparison using the extracted disc data.

An opt-in fixture runs real KO, revival and recovery code, including movement,
guarded and unguarded damage, stock cost and permanent ring-out elimination.
Another loads the actual voice/sample banks and checks SDL's decoded output from
ten voices, ten fighter sound banks and ten streamed effects on both platforms. It checks pause,
resume, music continuing during pause, stopping one actor without stopping the
others, and battle voice cleanup. Automated audio runs use SDL's silent dummy
device; they are not a subjective listening comparison.

Frame exports and test logs are local build output
under `port/build/`, not redistributable game assets.

## Current limits

| Area | Status |
| --- | --- |
| Simultaneous team combat | Up to five fighters per side, including unequal teams and free-for-all. Native character selection determines the actual roster. |
| Human controls | One to four independent pads/keyboards, configurable teams, 2/3/4 views. Native menus select the roster and each player's assignment. |
| Menus | Select toggles Modded Modes at the main menu. Setup and settings use native menu artwork; characters and stages use native selection. Return-to-menu routing retains the modded session. More human playtesting is still needed. |
| HUD | Original game health, ki, portrait, stock, combo and skill panels per view, plus target markers, revival and fusion indicators. Optional teammate/player labels leave room for native move names and counters. Revival captions and meters stay inside their viewport. |
| Revival | Configurable range, time and stock cost. Movement/blocking permitted; unblocked damage interrupts; ring-outs cannot revive. Eliminated players can take over a living CPU teammate. |
| Audio | Independent fighter sample banks, voice streams and streamed effects for all ten actors, with volume and pause controls; a direct match also opens the voice/music archive. |
| Targeting | Team-aware selection, L3 attacker targeting, camera-relative right-stick selection, hold-to-unlock, camera-centred relock and downed-ally targeting. |
| Collisions | Friendly hits are filtered; projectiles check living enemies. Clash pairs use the fighters that actually made contact. |
| Cinematics | All actors get voiced entrances. Melee/vanish exchanges and beam struggles have independent pairs; beam struggles use the actual beam owners and keep their effects alive. Further move and camera coverage is still needed. |
| Beam assists | Enabled by default. Press R3 near a struggling teammate with at least one blast stock. The helper uses their own model and firing pose; CPU teammates can assist too. Power and range can be changed in Mod Settings → Combat. |
| Giants | Optional larger bodies, collision volumes, slower ordinary attacks, melee damage, armor and follow-camera distance. Covers the eleven BT3 and twenty BT4 giant forms. Cinematic timing is unchanged. |
| Outnumbered assistance | Optional damage/reduction bonuses for the smaller living team, ordinary reaction recovery, brief get-up protection and combo escape. Specials and paired cinematics retain their normal behavior. |
| Stage effects | Water distortion, panorama blur and radial blur use each visible viewport, including the full-width third view and merged fusion views. Dust and weather advance once per game frame. |
| Expanded maps | Mod Settings → Stages → Map Size scales the arena, terrain, collision, water/lava, animated objects and intro cameras together, from 100% to 300%. It applies on the next stage load. The 35 selectable BT3 and 65 selectable BT4 stages have been loaded at 100% and 200%, including rematches. |
| Transformations and fusions | Native teammate fusion, human consent, timed dance separation and merged camera layouts. Real model-load checks cover Gogeta, Gotenks, SS4 Gogeta, Vegito, costumes, CPU partners, simultaneous fusions, knockouts and rematching while fused. |
| Online and replays | The separate Modded Online room supports players, CPUs and spectators; see NATIVE_TAG_ONLINE.md. The upstream online path and two-fighter replay format are separate and do not run Tag Team battles. |
| Game versions | BT3 USA, Europe/Australia and Japan, plus BT4 B14 REV2 English and Spanish. Regional resources are imported into the engine's logical layout; BT4 retains its own roster, parameters, voices and HUD offsets. Unknown revisions require another profile. |
| Gauntlets and tournaments | Authored wave presets, movement between waves, skippable arrivals and the original tournament bracket/crowd sequence. More human playtesting of complete runs and cancellations is still needed. |
| Other mod mechanics | Custom Potaras, tag attacks and custom scenarios still need native implementations. |

The upstream build also has 103 existing generated stubs. Linking successfully
does not establish that all of those paths work. None were added to hide a missing
Tag Team implementation.

## How this differs from the PCSX2 implementation

The mod changes the game's C code directly. A runtime actor list owns each
fighter and its team/target. Loading creates real engine objects and dedicated
resources for those actors. AI and combat run in the normal game update loop.
The battle ends based on the whole team's remaining fighters rather than just
its original leader.

No trainer attachment, external RAM writes, injected MIPS routines, actor-slot
swapping, or prepared savestates are used. The original serialized two-team
selection record stays intact; the additional actors live in a separate runtime
representation rather than changing the save format.

The native port itself still preserves many PS2-era structures and 32-bit game
pointers. This experiment has expanded the immediate per-fighter arrays and
replaced fixed effect arenas with owned native allocations. Fighter contacts
have a separate ten-actor table, and display-list writes are checked against
their reserved space. Remaining two-fighter interfaces still need attention
as more mechanics are ported.

## Source map

- `src/battle/tag_team.c`, `include/battle/tag_team.h`: actor roster, teams,
  targets, demo setup, lifecycle and whole-team victory decisions.
- `tag_revive.c`, `tag_hud.c`, `tag_hud_borders.c`, `port/src/gs/tag_hud.cpp`:
  revival, additional HUD indicators, correctly layered split borders and settings.
- `tag_assist.c`: teammate beam support, approach, firing poses, stock cost,
  power sharing and return to normal control.
- `hud_*.c`, `btl_cam.c`, `port/src/gs/gs_draw.c`: per-view original game HUD,
  four independent cameras and renderer capacity for ten fighters.
- `battle.c`, `battle_load.c`, `battle_work.c`, `btl_char_mgr.c`: allocation,
  loading, per-actor updates and hit-stop.
- `btl_ai_*.c`: expanded AI working sets and target-aware distance/sight queries.
- `tag_cpu.c`: the PCSX2 mod's opponent-scoring policy, with recent-damage
  retaliation, distance and crowding, minimum target commitment, and guards
  for grabs, clashes and queued cinematic damage. Human target selection
  stays under player control.
- `tag_stage.c`, `port/src/gs/stage_scale.cpp`: checked stage-resource scaling
  before native relocation. Unsupported layouts remain unchanged; partial
  scaling is never applied. The final two stages use their complete model
  packages and an existing ambient sound bank.
- `include/battle/tag_post_view.h`, `src/sys/gfxm_a.c`, `src/sys/gfxm_b.c`,
  `stg_c.c`: viewport-aware panorama, water and radial feedback.
- `btl_char_flag_opp.c`, `btl_char_hit.c`: actual rush participants and enemy
  filtering instead of the original fixed opposite slot.
- `btl_pool.c`, `eft_*.c`, `stg_a_b.c`, `stg_c.c`, `src/sys/heap.c`: independent
  native resource pools and per-actor stage effects.
- `src/sys/snd.c`, `src/sys/adx.c`, `src/sys/file.c`, `btl_char_flag_snd.c`,
  `port/src/gs/snd_se.c`, `snd_adx.c`: independent fighter audio, request queues,
  correct volume routing and the SDL mixer.
- `port/src/headless.c`, `port/src/gs/gs_input.c`: opt-in launch and target input.
- `port/tests/tag_team_runtime.py`: real-executable battle regression runner.

## Build and run tests

Use the prerequisites and disc extraction instructions in `docs/NATIVE_BUILD.md`.
The toolchain used here was Ubuntu 24.04 through WSL, Clang/LLVM 22 and matching
Python Clang bindings, SDL 3.4.12, and MinGW-w64 for the Windows executable.
Existing extracted `gamedata/` can be reused; there is no need to extract the disc
again for an incremental code change.

From this directory in the configured Linux environment:

```sh
export BT3_CC=clang64
.venv/bin/python port/tools/undefined.py
.venv/bin/python port/tools/link.py
.venv/bin/python port/tests/tag_team_runtime.py --exe port/build/bt3_64
```

For Windows, use the same upstream build scripts with `BT3_CC=win64` and
`BT3_SDL3` pointing to SDL's `x86_64-w64-mingw32` development directory. Copy its
`bin/SDL3.dll` next to `port/build/bt3.exe`, then run on Windows:

```powershell
python port/tests/tag_team_runtime.py --exe port/build/bt3.exe
```

Build the two targets sequentially: some generated build inputs are shared.
The test runner records separate Windows/Linux results under
`port/build/tagteam-runtime-tests/`. It requires the user's game data and limits
each test process to 120 seconds.

The additional Windows runners exercise the actual game executable and the
user's extracted disc data:

```powershell
python port/tests/tag_stage_runtime.py
python port/tests/tag_fusion_matrix_runtime.py
python port/tests/tag_cinema_matrix_runtime.py
python port/tests/tag_special_runtime.py
python port/tests/tag_spawn_runtime.py
python port/tests/tag_ffa_runtime.py --case effects
python port/tests/tag_ffa_runtime.py --case combat --renderer gpu
python port/tests/tag_ffa_runtime.py --case blind --renderer gpu
python port/tests/tag_menu_runtime.py --renderer gpu
python port/tests/tag_weather_runtime.py --views 3 --views 4
python port/tests/tag_post_runtime.py
python port/tests/bt4_runtime.py --case costumes --jobs 4 --frames 600
python port/tests/bt4_runtime.py --case fusions
python port/tests/bt4_runtime.py --case mechanics
```

The BT4 runners use `gamedata-bt4` by default. `--resume` only accepts results
from the same executable, memory image, disc profile and frame count.

The stage runner accepts repeated `--stage N` arguments for IDs 0–34 (35 is Random). Menu
scripts require the GPU renderer's input loop. The result cases use real team
knockouts before selecting Fight Again, Character Select or Main Menu on both
the win and loss screens. Cinematic cases include a native rush, Spirit Bomb
and Dragon Fist while an extra fighter transforms. These checks complement,
but do not replace, a human playtest of every move and costume.
The special-attack cases check ordinary beams, rushes, shared-camera options,
ultimates and concurrent attacks. The Free For All runner checks placements
for two to ten fighters, terrain clearance and rematches, including expanded
maps through `--scale 200` or `--scale 300`.

### Fighter-capacity fixes

The native build keeps independent disc lists, persistent body effects, sword
trails, ground dust, water wakes, splash cooldowns, bubbles and blindness for
all ten fighters. Aura and glow changes also accept every fighter. Discs
finishing out of order remove their own list entry, and each new disc receives
its own hit parameters. These changes address the extra-fighter disc corruption
that crashed ten-fighter Free For All on expanded Namek.

Transformation effects belong to individual fighters, including when a new
transformation starts before an old effect finishes fading. An unrelated hit
stop or rush does not stop another fighter's ordinary projectiles. Interrupted
charging clears the charging burst without removing the idle aura.

`tag_ffa_runtime.py --case effects` exercises these owners together in the real
game. The GPU blindness case checks that only the affected player's viewport
turns white. The combat case runs a ten-fighter Free For All on Namek at 200%.
Spawn checks cover dry starts, separation and rematches across all 35 stages.

Some two-entry structures intentionally remain: a move's attacker/victim pair,
the self/target panels in each viewport, the two authored teams and effect
geometry endpoints. They are not global two-fighter limits. Original story,
replay and online formats are outside this local Tag Team build's scope.

For a Linux playable quicklaunch after building:

```sh
BT3_TAG_TEAM=1 BT3_TAG_PLAY=1 BT3_TAG_ROSTER=2,2 BT3_DEMO=1 \
BT3_GS=gpu BT3_MENU_ITEM4=0 BT3_UPDATE_CHECK=0 \
BT3_SAVES=port/build/native-prototype-saves \
BT3_SETTINGS=port/build/native-prototype-settings.txt ./port/build/bt3_64
```

`BT3_TAG_ROSTER` accepts one to five fighters on each side, for example `4,2`
or `5,5`. Omit
`BT3_TAG_PLAY` for the CPU demo. `BT3_TAG_TRACE=1` enables periodic per-fighter
combat logging. Nothing dumps game memory by default.

## Latest local battle fixes

Revival rings use the PCSX2 mod's soft green bands and three traveling crests.
Opacity, wave height and wave speed are adjustable under Revival, with live
previews. The ring stays on the actual revival radius in each player's view;
it is hidden in Free For All and while paused.

L3 selects a recent attacker or another enemy currently attacking the player,
even while right-stick targeting is enabled. Without an attacker, it selects
relative to the centre of that player's camera. Stick flicks compare targets
in screen space, including vertical differences and a rotated camera. Brief
hit-stop retains an L3 click until the fighter can safely change target.

Ordinary special moves no longer send their camera files to the global intro
camera. Their native cuts belong to their participants, and the shared-camera
settings decide whether everyone else joins them. Camera-linked effect timing
stays with its owner too. Grabs use their actual attacker and victim for camera
midpoints and paired root movement; a catch clears leftover backstep momentum.

Free For All disables revivals and their rings. Team-mode revival text appears
only inside a fallen teammate's revival radius, with progress or the number of
missing blast stocks. Lock-off hides the target arrow and, by default, the
enemy panel, including during hit-stop.

FFA starts prefer open, dry, durable terrain with room around each fighter,
rather than walls, submerged ground or props that can break during Ready.
The ten-fighter 200% placement/rematch checks cover all 35 stages.

Focused native tests are `tag_team_runtime.py --controls-only`,
`tag_team_runtime.py --cameras-only`, `tag_special_runtime.py`,
`tag_spawn_runtime.py --scale 200`, and `tag_feedback_runtime.py`, all under
`port/tests`. The grab replay includes a real interception during a backstep;
the special runner can select individual fighters and move slots, including
Nam's rush and Gotenks' Kablam Slicer. These tests use separate saves.

Fresh mode entries clear the previous roster and character-selection controller.
Returning to Character Select inside a match keeps the roster, but reconfirming
a human fighter still hands control to the next assigned player. CPU slots and
End return to P1; moving across another player's slot alone does not take control
away. The three-controller Training replay covers both menu paths.

Grab and rush participants reject outside body and projectile hits from the
successful catch update onward, including queued contacts. If the grabber was
already hit before catching the opponent, the grab cancels without leaving a
held victim behind. Normal contact resumes after the move. Shared cinematics freeze the other fighters;
the camera's owner and actual participants keep playing. Native fusion and
transformation cameras advance on simulation updates even while the duel clock
is stopped, and shared shots keep a full-screen projection when fusion changes
the split layout. Planet destruction uses one shared scene through the stage
load, then restores all views and places the full roster on the new terrain.

## Next pieces for a full mod port

New offline modes must include their complete native menu flow, not only a
battle launcher. Preserve the mode, teams, player assignments and rules when
returning to character selection; Fight Again must restart that setup. Pause,
results, cancellation and Return to Main Menu must lead to their corresponding
native screens without falling into an unrelated default mode. Exercise these
routes after wins, losses and interrupted setup as well as a normal start.

World Tournament, gauntlets and Modded Online now have local implementations.
Modded Scenarios and tag attacks remain separate future work.

Gauntlet and tournament presets are selected after player count, before team
setup. Tournament players can enter up to four teams; character selection asks
only for those teams. On the bracket, Cross watches a CPU match and Square skips
it. The tournament opening and bracket music use the selected disc's audio.
Tournament setup has no duel timer: rounds end when a team has no fighters left.
Ring-outs keep that fighter down permanently, with no recovery or revival; a
player can still take over a living CPU teammate.

Gauntlets allow movement between waves and play skippable entrances for the next
opponents. Wave titles and the five-second round-end message sit below the timer,
without a background panel. The clear screen lists each wave's time and damage
taken. Heal per Wave and Music are in team setup. Music starts on Random for
each new gauntlet setup. Fight Again resets the wave roster and the run's statistics.
Automatic opponent-search is suppressed during the break, so the hidden next
wave cannot cause repeated lock-on animations or sounds. Your chosen music
continues across waves and is kept for Fight Again.

Camera sharing choices apply in split-screen as well. Rush participants keep the
native authored shots even in a single view; enabling shared beam-clash cameras
promotes the clash camera to full screen and restores the player views afterward.

1. Continue human playtesting of controller assignment, selection, rematches and
   menu returns before creating the public fork.
2. Continue character/move coverage and longer four-controller sessions. Automated
   tests exercise ten simultaneous audio streams; busy-match listening and visual
   checks of every move are still separate acceptance steps.
3. Continue testing the online implementation across separate computers, then
   handle the remaining modes and tag attacks.
4. Extend native data adapters to the remaining discs supported by the PCSX2 mod.

Keep the upstream AGPL-3.0 license and attribution. The extracted disc, generated
game-data tables, saves, screenshots, and compiled local build artifacts are not
part of the source contribution and must not be published as game data.

BT4 COM Level and the individual CPU picker use the disc's five difficulties:
Easy, Normal, Hard, Very Hard and Z Hard. They map to the original BT4 CPU
levels (6, 13, 21, 29 and 99); BT3 retains its original five settings. Online
rooms keep the same difficulty indices as offline selection and save data.
