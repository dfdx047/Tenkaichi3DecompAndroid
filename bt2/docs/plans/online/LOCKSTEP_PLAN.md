# BT2 online: the fight, from what BT3 does (2026-10-27)

Read together with [ONLINE_PLAN.md](ONLINE_PLAN.md) (stage 3). This is what BT3's netplay does, what of it BT2 can have and how, and where the work stands.

## What the games have

| | Online play |
|---|---|
| BT3 | Complete: `port/src/gs/net.c`, `net_relay.c`, `net_match.c`, `state.c`; the design is in `docs/netplay_notes.md`. |
| BT4 | None. Its engine answers "no" to every network question (`port/bt4/host/bt4_shim.c`: `Port_NetActive`, `Port_NetSession` return 0; `port/bt4/README.md`: online is the BT3 engine's). |
| BT2 | The line between two phones (pairing through the service, STUN, hole punch, UDP, optional relay) and the screens. The fight is what is left. |

BT3 and BT4 are not touched by anything below.

## How BT3 keeps two copies in step

1. **Same start.** Two copies only agree if they start from the same state. A session is a fresh start of the program on both sides
   (first by relaunching with `BT3_NET_SESSION=1`, later by putting the game back to a snapshot kept at the first blank), with a save folder
   of the session's own and everything unlocked, so the roster is the same. The start-up runs with nothing pressed, without picture or sound and
   not held to real time, to the character select; the two meet there (`T_ARRIVED`).
2. **Lockstep.** One player per copy (the host is player 1). Every vertical blank a copy sends its player's input for that blank, the last 16
   in every packet, and does not go on without the other's input for it. An input takes effect `delay` blanks after it was made (default 2;
   automatic from the measured round trip). A waiting copy sends its latest packet again every few milliseconds.
3. **Randomness.** The simulation's own generators reset with the fighters; the ones fed from a timer at boot are the problem (notes,
   "Sources of randomness"). Anything the game waits for that depends on real time (voice playback, model loading) must be made the same.
4. **Checks.** Every 32 blanks each side notes a number made of what must be equal (the fighters' health, position and the clock); the newest
   travels in every packet and a difference ends the match with a message.
5. **Rollback** (optional, off by default) needs the whole state saved per blank. **Clock balance**: each packet says how far the sender is
   beyond the other's input; the side that is ahead lets its blanks come a little later.
6. **Round trip and delay**: 12 pings at the start, the host chooses the delay (1 to 6) and tells the other side.

## What BT2 can have, and what is different

- BT2 is recompiled, not decompiled: there is no state to save, so **no rollback**; lockstep only, with the delay from the round trip. At 45 ms
  (the test across two networks) the delay is 3 fields and the game does not wait; a worse line makes both games wait, as BT3's first version did.
- The time base is the field (a vertical blank, 60 a second; a fight draws every second field). The input is the pad service's six bytes per field
  (buttons, four sticks), the format of `BT2_INPUT_RECORD`.
- Where input enters: the game's own pad routines (`frame_pad` 0x28B090, `frame_pad2` 0x28BDA0) work on one structure at 0x3B0B00 that holds both
  pads; the plugin already hooks `frame_pad` and writes `pad+0x144` (held) and `+0x134` (pressed). The second player's fields in that structure are not
  mapped yet (see "Next").

## Done: `plugin/online/lockstep.{h,cpp}`, tested

The exchange itself, with nothing of the network or the game in it: `pushLocal`, `ready`, `pair`, `done`, `makePacket`, `receive`, `noteDigest`, `delayFor`.
A packet holds every sample of its side the other has not confirmed (up to 64), the position up to which it holds the other's, `ahead`, the newest
check value and a sum over the whole packet.

`plugin/tests/lockstep_tests.cpp` (AddressSanitizer and UBSan on) simulates two sides on a line and checks that the game on both sees the same two
controllers at every field, the ones the players made `delay` fields before: clean lines at delay 0 to 16; 180 runs with 10 to 40% of packets lost,
15% repeated, reordered and 1 to 6 fields late; a 2.5 s and a 5 s tunnel; one clock slower than the other; damaged packets; a check value changed on
one side (found on both, at the right field); random bytes into `receive` (never taken). 892 checks, 0 failures.

The first run of the tests found a real defect: a side dropped its own samples as soon as its game was past them, while the other could still be
waiting for them (every lossy run hung). Samples are now kept until the other side confirms them.

## Built (2026-10-27), compiled and tested on a computer, NOT yet run in the game

The whole path is written. What is tested is everything that does not need the game (892 + 2144 + 36 + 41 checks, with sanitizers); what is not tested is
how the game takes it. In the order a match goes:

1. **Lobby** (the Online Mode screens, as before). When the line is up and both names are shown (5 s), the plugin writes `online_session` (role, the lobby socket's
   port, the room's code, both names, the other side's addresses, the time) and `online_restart` = `session`; the line stays up meanwhile.
2. **The app** (`Bt2OnlineOverlay.checkRestart` -> `Bt2GameActivity.restartEngine`) makes `user/online-card` (a fresh copy of the 100% save from the APK's assets: both
   sides start from the same bytes, never the player's own card), starts `Bt2RelaunchActivity` (main process) and ends the game's process; the relaunch opens
   `Bt2GameActivity` afresh. A start from the launcher deletes any old `online_session`.
3. **The match's engine** (`android_main.cpp`: the card, 60 fps off, the widescreen patch on, whatever the player chose; `plugin/online/online_plugin.cpp`:
   `startFightProcess`): the plugin finds `online_session` (renamed to `.last` at once; older than 5 minutes is ignored), turns on the second pad, fast forward
   (the pacer's new global switch) and silence, covers the picture, and `FightSession` opens the socket **on the lobby's port** and says hello to the other side's
   addresses for up to 90 s (the other engine starts a few seconds either way).
4. **The walk** (state-driven, not a script of fields): `start` and `cross` every 24 frames until the game's mode is the main menu's; then the lobby's own
   automation (`auto_pick` = Dueling, "1P VS 2P" lit by `padConnected`, the cursor set, `cross`, the one-step walk) takes it to the character select.
5. **The meeting** (`FightSession`, `SessionControl`): the host offers the delay (from the round trip), who is player 1 (drawn) and a seed and repeats until
   confirmed; each game says "arrived" with its count of fields; the first to arrive WAITS inside the pad service (its time stands still). Both counts are
   logged; if they differ the log says the two games may not be in the same state.
6. **The lockstep**, once per field from the pad service (`EeRuntime::nativePadVblank` -> `on_pad_exchange` -> the plugin's `pad_field`): the local controller goes
   out, the game gets both players' controllers (player 1's on port 0, player 2's on port 1, a real second DualShock 2 for the game's own pad library), waits when
   the other's is missing, and every 32 fields notes a check value of the memory (a sampled hash of 4 of the 32 MB; advisory: it is logged, and ends the match only
   with the file `online_strict`). Names go to the app (`online_match`) and are drawn under the health bars.
7. **The end**: the other side leaving (BYE), silence for 6 s, the line lost, or the player going back to the main menu ends it; the picture goes black with the
   "left" message for 4 s and the app starts the engine as it normally is.

**The second pad** (`services/pad_service.cpp`): the game's pad library copies the socket table and opens socket k only when word k of it is 1; the service sets
word 1 once detection ends, opens slot 1, answers the device queries and port requests of port 1 as port 0's, and writes the other player's block to slot 1's buffers
each field. This follows the traced behaviour of port 0 and the library's code (`FUN_00313760`, `FUN_003149F8`); port 1 itself was never traced on real hardware.

### What to look at in the first test

- the run log, lines `online:` (the socket on the lobby's port, `UP via`, `the host offers`, `at the character select after N fields`, `both are at the character select
  (fields A and B: the same / DIFFERENT)`, `the match ends: ...`) and `Pad: slot 1 opened`;
- if the game stops with an unsupported pad request: that is port 1's protocol differing from port 0's (the request is named in the log);
- if the walk does not reach the character select: the start-up's presses (`fightFrame`) or the lobby's automation need another step (files `online_walk`);
- if the two arrive at different fields, or the check values differ: the game is not repeatable between the two phones (see the first item below).

## Next

1. **Is a run repeatable across two phones?** `sync_test = 1` then `2` on one phone, then the match's own check values on two. Everything depends on it.
2. Anything the first test shows: the walk, the second pad, the arrival, the waiting.
3. The stage and the music chosen only by player 1 (player 2's controller muted during that screen), the clock balance (`ahead` is already sent), the meter, and a relay for the
   networks that do not let a direct line through.
