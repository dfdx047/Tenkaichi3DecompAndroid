# Native Tag Team Online

The Online Lobby connects **up to ten people**. Guests can join the host's
next match or watch it. Unclaimed fighters are CPUs, and everyone fights on
the field together. Teams can be different sizes.

From the normal game menu, press **Select**, then choose **Online Lobby**.
The online room uses native game menu pages, character artwork and stage previews.

You can also open **Play.cmd** for BT3 USA, or **Play.cmd**
for English BT4 B14 REV2. These launch the native game, not PCSX2. They use
separate online saves and settings; your offline saves are unchanged.

## Persistent Online Lobby

Choose **Select > Online Lobby** in the normal BT3 or BT4 main menu.
The Windows and Linux launchers both include this menu entry.

An Online Lobby holds the host and up to nine guests. The host uses the normal
**Modded Modes** main menu, including its game artwork and challenge submenus.
The host does not load or simulate the practice map, so its map audio does not
play behind the menu. Guests enter a practice map with no Ready/Fight sequence
or time limit. BT3 uses Kami's Lookout and BT4 uses South City, both at 200% size.
Six Saibamen stand in a row, labelled **Training Dummy** and the game's five
difficulty levels (including **Z Hard** in BT4).

- The training dummy does not fight back. The other dummies engage after you
  attack them. When either fighter is knocked out, they recover after three
  seconds and the dummy returns to its station.
- Approach another guest and press **Square** to challenge them. They accept
  with **Square** nearby. Players cannot hurt each other before accepting.
  After a knockout, both recover and become neutral again. Only these player
  challenges count toward the lobby's wins, losses and draws.
- **Start** opens your local practice menu without stopping the world.
  **Change Fighter** uses the original character, form, equipment and costume
  screens, then replaces your fighter in the current map. Finish a challenge
  before changing fighters. If another guest is using the selector, your
  request waits for them to finish.
- The host chooses Duel, Free For All, Training, Gauntlet or World Tournament.
  Everyone moves to the shared team setup. **Start** claims an available slot;
  press it again to ready up. Select an available CPU slot to move there, or
  use **L1/R1** to change teams. **Triangle** releases your slot so you can watch.
  Gauntlet has five player slots; other modes allow ten. Unclaimed fighters
  are CPUs, and unclaimed guests can follow fighters or use free camera.
- World Tournament supports four entered teams. The host can watch or skip
  CPU-only bracket matches. Gauntlet has preset, music and Heal per Wave choices.
- When the session ends, the host returns to mode selection and guests return
  to practice, keeping their connections. The host can also cancel setup with
  **Triangle**. A guest leaving a match hands their fighter to the CPU. Losing
  the host ends the room.
- Connected players and their round-trip ping to the host appear in the corner
  of the host's mode menu and the guests' practice view. This roster does not
  appear during the selected match. The host can press **R3**, select a guest,
  then press **Square** to remove them; **Triangle** returns to choosing modes.
  Guests can
  join an ongoing practice map; if a match is already underway, they wait for
  its next room transition.

Gameplay settings come from the host for the duration of the room. Each guest
keeps their own HUD and other display preferences, and their saved offline
settings are not overwritten. All computers need the same build and disc data.
Separate-PC internet play still needs a human playtest.

Connections are direct to the host's address and TCP port. This feature does
not depend on a matchmaking or relay service. On a local network, use the
host's local IPv4 address. Across the internet, use an address reachable from
the other computers; the host may need to forward the chosen TCP port.

The local lobby checks cover ten members with delayed input, joining practice
in progress, replacing a disconnected guest after a fighter change, arriving
during a match, and returning to practice on the same connections. They also
cover a full tournament, five gauntlet players with two viewers, local picker
confirmation/cancellation, host loss, kicking a guest, and restoring offline
save data and BT4 equipment afterward. These used real native processes on
one computer; they are not a substitute for a multi-PC playtest.

## Hosting and joining

Everyone needs matching game data and the same native build. Windows and Linux
currently use separate online rooms. Choose **Host a Room**, confirm the TCP
port (7001 by default), and open the lobby. Guests choose **Join a Room** and
enter the host's reachable IPv4 address and port.

The host chooses a mode from the normal Modded Modes menu. At team setup,
guests press **Start** to claim a place and choose their characters through
the game's normal selector. Guests without a slot watch the match. Loading
waits for the players before starting the entrances.

Each computer uses **Player 1's local input bindings**. Use **F1 > Controls**
to choose your keyboard/controller and bindings.

## Spectating

Spectators join **before the match starts** and stay connected across rematches.
They cannot send fighter inputs, ready up, change the roster, accept a fusion,
or take over a CPU. A spectator leaving does not stop the players.

- **L2 / R2**, or **[ / ]**: previous/next participant's view, including CPUs.
- **Select**, or **F**: toggle free camera.
- In free camera: left stick moves, right stick looks, L1/R1 change height.
  The keyboard equivalents use the local game bindings.
- The spectator panel also has clickable controls and **Leave match**.

The original health/ki panels follow the viewed fighter and their target.
Following a fused partner follows the shared fighter. Camera movement is local
to the spectator and does not affect the players' simulation.

## Results and connection problems

After a session ends, the host returns to mode selection and guests return to
the practice map on the same connection. A missing guest becomes a CPU during
a match; losing the host ends the room.

The compatibility check compares build and game-content fingerprints, not
installation paths. The first BT4 check reads its extracted resources and can
take a few minutes on a slow drive. Later launches reuse content hashes when
the files have not changed. This check happens before opening a room.

## Rollback and current coverage

Every player runs the complete battle locally, including the CPUs. Only agreed
inputs enter the simulation. Rollback saves the game heap, low-address arena,
scratch memory, stack and game globals, including all Tag Team modules and BT4
animation bookkeeping. It restores the whole battle before replaying late inputs;
it does not merely correct the human fighters' positions.

The input delay is two vertical blanks, with up to eight blanks of prediction.
Spectators run confirmed inputs without prediction. Shared random state,
fighter state, revival progress, fusion ownership/timers and assist state are
checked during play. A detected mismatch stops the match rather than continuing
with different outcomes. Network queues, spectator controls and local rendering
preferences stay outside rollback.

Local Windows tests have exercised actual socket connections with:

- Ten human input producers in BT3 and BT4, including a BT3 room with ten spectators.
- Two players plus CPUs, with the host and a spectator exchanging roles before
  the match and exchanging back before the rematch.
- Delayed inputs that force rollback; separate full-state rewind comparisons.
- BT3 and BT4 revivals, fusion/control handoff/defusion, beam assists,
  simultaneous beam clashes, and rush/ultimate cinematics overlapping a transformation.
- Two completed rounds with all ten spectators returning to the room.
- Remote pause-menu open/cancel and Return to room, including Player 10's
  rendered BT4 view and controls.
- A player loading 20 seconds later, and host/player/spectator disconnections.
- GPU-rendered matches with audio enabled, per-player health/ki panels, and
  spectator view changes/free camera.

These are same-PC tests. Separate PCs over the internet, high jitter/loss,
long sessions, and Linux online play still need testing. Build identity currently
requires identical binaries, so Windows/Linux cross-play is not enabled. The
current native disc support is BT3 USA and English BT4 B14 REV2; the PCSX2
mod's other regional adapters are separate.

The repeatable runner is `port/tests/tag_online_runtime.py`. Its default tests
use isolated saves/settings and real game processes on loopback. For example:

```text
python port/tests/tag_online_runtime.py --spectators 10 --input --lag
python port/tests/tag_online_runtime.py --players 10 --spectators 10 --input --lag
python port/tests/tag_online_runtime.py --bt4 --players 2 --spectators 1 --swap-roles --case 5 --rounds 2 --frames 5000
python port/tests/tag_online_runtime.py --bt4 --case 2 --spectators 1 --input --lag --frames 1850
python port/tests/tag_online_runtime.py --case 5 --spectators 10 --rounds 2 --frames 5000
```

Run one test at a time, or give parallel tests different `--port` values. The
runner closes only the processes it starts. Logs go under `port/build/tag-online/`.

`python port/tests/tag_online_room.py` checks a real host's character edits,
ready resets, player-slot ownership, spectator permissions and capacity,
role changes, build mismatch rejection, and replacement players joining an existing room.
