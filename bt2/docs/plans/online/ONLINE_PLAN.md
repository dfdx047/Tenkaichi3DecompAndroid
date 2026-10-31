# BT2 online play (Dragon Net 2)

Written 2026-10-25. Android only. Names stay neutral; addresses are BT2's (USA disc, SLUS-21441).

## What is decided

- **The app does everything that is not the game's memory.** The player's name, the room code, the search, the requests to the
  matchmaking service and their errors are Kotlin (`android/.../data/Bt2Online.kt`, `engine/Bt2OnlineOverlay.kt`). The engine
  (`libbt2.so`, a signed package outside GitHub) keeps what touches the game: the menu pages, the pictures drawn over the frame, and
  later the exchange of the pads and the stepping of the game. Anything that can live in the app goes there: an app update reaches
  the players with the APK, an engine update needs a new motor package.
- **The same matchmaking service as BT3** (`port/dragonnet/worker.js`, run by its owner), with a netcode version of its own:
  `Bt2Online.VERSION = 6001` (BT3: 5001). The service pairs only equal versions, so a BT2 player never meets a BT3 one, and BT3 is not
  touched. The service's `ALLOWED_VERSIONS` must list 6001 (its owner's change) before the official service takes BT2.
- **No BT2-against-BT3 play, and none against the Switch or the PC port**: different games, different fights.
- **Development without the official service:** `node bt2/tools/dev_server/serve.mjs` runs `worker.js` unchanged on the developer's
  computer (in-memory database, `adb reverse tcp:8787 tcp:8787`, `dragonnet_url.txt` in `<root>/bt2/user`). Debug builds accept
  cleartext to 127.0.0.1 only (`android/app/src/debug`).

## The stages

1. **Menus** (done, tested on a phone): Online Mode item in the main menu, its pages (Online / Ranked; Create room / Join room / Quick
   match) and the name box.
2. **Pairing** (done, tested against the development service): create a room (a code), join with a code, quick match; the opponent's
   name is shown. The app talks to the service; the game's library draws the screens and goes by the app's word, files in
   `<root>/bt2/user`: `online_peer` (the opponent: name, room code, his addresses, a line each), `online_back`; and the library
   tells the app through `online_local` (the addresses of its socket) and `online_link` (`rtt=<ms>`). The state is the fourth count
   of `Bt2Bridge.nativeStats()`: 0 closed, 1 Online/Ranked, 2 the three ways in, 11/12/13 create/join/quick in progress, 15 saying
   hello to the other game, 14 the line is up, 16 it could not be made.
2b. **The library** (done): everything of the menu and of the online play that does not need the game's code is `libbt2online.so`
   (`bt2/plugin/`, API in `bt2_plugin.h`), built by the public CI (`build.py plugin`, non-blocking step in `android.yml`) and carried
   by the APK; the engine loads it (`runtime/plugin/plugin_host.cpp`). Quick loop: `build.py plugin` takes 15 s.
2c. **The direct line** (done, tested 2026-10-26 on the same Wi-Fi: phone shows "Conectado! Atraso: 38 ms", PC measures 22-32 ms): `bt2/plugin/online/link.{h,cpp}`, UDP "hello" to every address the service
   gave, "ping" for the round trip, 15 s to give up. Test with one phone: `bt2/tools/dev_server/peer.py` plays the other player on the
   computer (same Wi-Fi).
3. **The fight** (not started, the hard part).

## Next, in this order

1. (done) the line over the same Wi-Fi works; not tested across networks or on mobile data.
2. **The relay (TURN)**, so that any two phones can play. BT3 does it in `port/src/gs/net_relay.c` (RFC 5766 over UDP: Allocate with
   long-term credentials, CreatePermission, Refresh, Send/Data indications, ChannelBind). The service gives the host `turn`
   {server, username, credential} in the answer of `/v1/host` (only when it has `TURN_KEY_ID`/`TURN_KEY_TOKEN`; the development
   copy has none, so test the relay with a local TURN server such as coturn, or with the owner's keys); the host publishes
   `relay=ip:port` with `/v1/addrs`; the other player's addresses carry `seen=ip`. In the library: a small TURN client next to `link.cpp`;
   `Link::start` already skips `relay=` / `seen=` entries.
3. The repeatability check (stage 3, step 1 below), the second pad, lockstep.
## Stage 3: what BT3 does, and what BT2 lacks

BT3 (`port/src/gs/net.c`, `state.c`, `net_relay.c`; notes in `docs/netplay_notes.md`): both copies run the same game from the first
vertical blank, each with one local player (the host is player 1); every blank a copy sends its player's input for that blank over UDP
(the last 16 blanks in each packet, so a lost one is covered) and does not go on without the other's input; an input delay of a few
blanks gives the packet time to arrive; on top of that, rewinding (the state of the game as one number, saved and restored) lets a copy
guess the other's input and go back when wrong. When two players cannot reach each other, the host asks the service for a TURN relay
(Cloudflare) and publishes its address.

What BT2 has: the pad service is one place (`services/pad_service.cpp`), so the other player's input can enter there; `ee/state_hash.cpp`
hashes EE RAM, the scratchpad and the VU memories at the end of every field; fields are counted (one per vertical blank).

What BT2 lacks, in the order it must be settled:

1. **Is a run repeatable?** The same pad input from the same start must give the same states, field by field. This is the question
   nothing else can be built without. Test: two runs with the same input (the state hash tool, `tools/ps2recomp/compare_state_hash.py`).
   Sources to look at first: the game's random generators seeded from a timer at boot, the disc's read timing, the host-side threads of
   the recompiled EE (`ee/retained_ee_stack.cpp`), the sound and video services' timing. A first attempt (2026-10-25) recorded the
   player's input and replayed it, hashing every field; the hash slowed the game from 60 to about 48 fields a second, so a check must be
   made cheaper (hash every N fields, or hash only the fight's own values) before it can run in a normal session.
2. **A second pad.** The pad service answers "no device" for port 1; a two-player fight needs both. The other player's input is written
   as port 1 (the host: port 0 local, port 1 remote; the one who joins: reversed or mirrored).
3. **Starting both from the same state.** BT3 keeps the state at the first blank and puts the game back to it at a session's start.
   BT2 would need the whole machine's state saved and restored: EE RAM, scratchpad, VU memories, registers, the recompiled threads'
   stacks, the services' own state (cards, sound, GS). Alternative: both games reach the same point by running the same input from a
   fresh start, if (1) holds.
4. **Lockstep** over UDP with the redundancy and delay above, then rewinding only if lockstep alone is too slow.
5. **The connection**: addresses from the service (`addrs`, "ip:port"), a hole punch, and the host's TURN relay when needed. The app
   now reports this phone's addresses with a placeholder port; the engine's socket will report the real one.

## Release

Stage 2 needs the app and the engine updated together (the app's overlay reads the engine's fourth count; the engine's pages need the
app's pictures). Stage 3 will also raise `BT2_ENGINE_ABI` if the app and engine start to exchange more than counts and files.

## Stage 3, step 1: how the game reaches a two-player versus (found 2026-10-26, from the project notes; not yet run)

- The main menu's entries set the progress structure's mode (`+0x1C` of the structure whose pointer is at `0x3876A4`): Dueling is **mode 30**.
  The path of the gate's scripted fight (`bt2/tools/gate/pad-fight.txt`): main menu, Dueling, a versus choice (1P VS COM there), Single
  Battle, **character selection** (one screen: mode, colour, pick), map selection, loading, fight. After the menus the main loop (`0x100680`)
  runs the battle routine `0x103000` with the setup the menus left: `0x101A60(kind, music, ..., stage)` (kind 0 versus), `0x101AB0(side, ...)`
  control (byte `+452`: 1 = CPU), `0x101AF8` a fighter (character * 9 + variant).
- So the online match can use the game's own two-player versus screens: after "Connected", the library takes the game to mode 30 and
  presses the menu's buttons itself (as the gate's script does) to the two-player versus, and the other player's pad is port 1.
- The layout asked for (player 1's picture above, player 2's below) is not what the game's selection screen is: it is one screen with
  both sides. To decide with the user: use the game's own screen (both pickers live on it), or build the two-picture layout (much more work).
- Order of work: (1) the library leaves the Online screen into mode 30 and walks the menus to the two-player versus (a menu-walking script
  from the library, hook on the pad); (2) pad port 1 answers with the other player's buttons; (3) exchange of the buttons each field
  over the line, waiting when the other's are late; (4) the roles (player 1 and 2 drawn by one side and told to the other) and player 1's
  map and music; (5) the names under the health bars.


- Mode table read from the disc (2026-10-26; DBZP.BIN loaded at 0x3B3100, table at 0x421950, one handler pointer per mode 0..63): modes that share a handler are one
  family. Handler 0x3B8360 serves modes 29..34 (Dueling is 30, so 29..34 are Dueling's variants: which one is the two-player versus is not known), 0x3B8350 modes 9..15,
  0x3B8340 modes 23..26, 0x3B8330 modes 5..7, 0x3B8370 modes 52,53,55..57, 0x3B8380 modes 60..63, 0x3B8398 modes 47..50, 0x3B83A8 modes 42..46, 0x3B83B8 modes 37..40, 0x3B83C0 = the
  default (the rest). The character selection is not a mode of its own: it is inside the family's handler (a sub-state), so the way in is the Dueling menus, not a mode number.
  Next experiment (phone, needs the user to look): from the Online screen set progress +0x1C to 29, 31, 32, 33, 34 in turn (file `online_mode_test` in user/) and see which screen
  comes; look for the one that is "1P VS 2P" and its way to the character selection (then walk the menu with pad presses the way tools/gate/pad-fight.txt does).

## What BT3 does after two players connect (read 2026-10-26: port/src/gs/net.c, "warp_tick", NET_VERSION 5 note), and what BT2 will copy

BT3 does not walk menus and does not jump into a mode. When a session starts it puts the game back to its first blank (a saved state) and
**both copies run the game's own start-up again from there, with nothing pressed** (logos, card check, up to the character select), without
picture or sound and without waiting for real time or for each other ("warp": Port_NetWarp). Both arrive at the character select in
exactly the same state, at their own speed; a message (T_ARRIVED) tells each the other is there; from the character select on the
real lockstep begins (inputs each blank, check values compared). The host is player 1; random numbers are seeded the same on both.

BT2 has no saved state of the whole machine, so the same idea in BT2's terms: **a session = a fresh start of the game** (the app restarts
the engine with a "session" flag and the same seed on both phones), the start-up fast-forwarded without picture or sound with a fixed
script of presses (the probe already walks boot -> title -> menus -> Dueling -> versus -> character selection with scripted presses,
tools/gate/pad-fight.txt, and that scripted run gives the same VU1 hash every time), arriving at the character select identical on both;
then the two meet (arrived message) and the lockstep starts. Needed: a pad script from the library (a hook on frame_pad 0x28B090 that
adds buttons to pad+0x144 held / +0x134 pressed), the session flag and seed from the app, a fast-forward without picture/sound, and the
repeatability proof (sync_test). The "Dueling menu has 1P VS COM, 1P VS 2P, COM VS COM, Battle Settings" is confirmed on the phone: the
second item is the one to pick.

## The online match's flow, as the user specified it (2026-10-26)

1. When the two players are connected, the game goes from the Online Mode screen straight to the Dueling menu (no main menu seen: a cover over the
   frame during the switch). Nothing is pressed for the player: he picks in the menus himself.
2. In Dueling, **1P VS 2P is available only while someone is connected**; with an opponent connected it is the only entry open. The host (player 1,
   drawn at random) drives the menus; only player 1 chooses the map and the music.
3. The connection stays open for the whole match (character selection and fight, shown live as the game shows it). The names given on the
   Online screen are shown under each health bar.
4. If the opponent leaves or the line drops (no answer for a few seconds): the screen goes black for a moment with a message that the opponent
   left or was disconnected, and the game goes back to the Online Mode screen.
Needed: the line kept open after "Connected" (not closed by endWait), a drop detector on it, the Dueling menu's entries (find its pad/draw routines
like the main menu's), the second pad, the exchange of buttons, the shared seed and choices at the start of a fight, the names under the bars.

## Next after the Dueling menu works (user, 2026-10-26)
- Pressing "1P VS 2P" opens the character selection of both players in real time, on a split screen. If either player has not yet taken the option in
  the Dueling menu, the selection loads for the other one with "Aguardando Oponente" written at the bottom (each side waits, the line tells who has
  pressed). Needs: a message "I pressed 1P VS 2P" over the line, the second pad (port 1) fed with the other phone's buttons, the exchange of buttons per
  field once both are in, and the waiting text drawn over the selection (a picture, like the others).

## The Online Mode screens: the title and the fighter (2026-10-26)

- The title band ("Online Mode") is its own picture (title.png) that the plugin draws floating (a slow sine of a few pixels), over the screens' pictures.
- The fighter is a looping clip, not a still: `bt2/tools/menu_item/make_fighter_video.py <clip.mp4>` cuts the clip's frames (GStreamer; the sound is not used) into
  gohan_NN.png (8-bit palette, a few tens of KB each) and gohan.txt ("frames fps width height"); `make_menu_item.py --bust <first frame> --animated` then draws the
  screens with only the glow behind it. The plugin draws the frame of the moment (its slot's version changes with the frame, so the renderer takes the new pixels).
- For tries on a phone, pictures in `<root>/bt2/user/extra/` go before the APK's (the app rewrites menu-item/ when it updates).
- Hook slots of the engine: 16 (an engine of an earlier version has 8 and ignores the others).
- Development triggers in the plugin (files in the user folder): online_dump (the game's memory to dump_N.bin), online_press (a button name), online_walk (a script of
  presses), online_mode_test (the mode to go to after "Connected").
