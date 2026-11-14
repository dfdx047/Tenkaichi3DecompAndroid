# Test Ranked revanche with a local Termux server

The local server runs the production `worker.js` and ranked modules against a local SQLite database. The debug APK can
point at it over loopback/private LAN HTTP; release builds keep cleartext HTTP disabled. A second game device is needed
for the actual peer-to-peer match. The server can seed series wins and override the two result reports for a paired
match, so the desired series result does not depend on who won the fight on screen.

## Build and start

From the repository in Termux:

```sh
cd ~/Tenkaichi3DecompAndroid
ANDROID_NDK_HOME=/data/data/com.termux/files/home/Android/Sdk/ndk/28.2.13676358 sh port/tools/android_toolchain.sh
python3 port/tools/android.py
cd android
ANDROID_HOME=/data/data/com.termux/files/home/Android/Sdk ./gradlew :app:assembleDebug
```

The first toolchain setup downloads LLVM and SDL3 if they are not already present. Install
`android/app/build/outputs/apk/debug/app-debug.apk` on both phones. Put both phones on the same Wi-Fi. On the phone
running Termux, from another Termux session:

```sh
cd ~/Tenkaichi3DecompAndroid
RANKED_LOCAL_ADDR=0.0.0.0 RANKED_LOCAL_PORT=8787 node port/dragonnet/ranked-local-server.mjs
```

Keep this process running. It prints an **Admin key** and the SQLite file path. The default database is
`port/dragonnet/ranked-local.sqlite`; it persists between server restarts. Stop it with Ctrl+C.

Find the Termux phone's Wi-Fi IPv4 address with:

```sh
ip -4 addr show wlan0
```

In each APK open **Debug → Local Ranked server (debug APK)**. On the Termux phone enter
`http://127.0.0.1:8787`; on the other phone enter `http://<TERMUX_WIFI_IP>:8787` (for example
`http://192.168.1.42:8787`). Tap **Apply**, then fully stop and reopen the game on both phones. The URL is saved in
the app's game data, so no shell access to Android's private `Android/data` directory is needed. To return to production,
clear the field and restart the game.

Both players then open **Dragon Net → Ranked** and search with the same mode. Wait until they are paired and the game
reaches character/map selection. At this point query the local match list from Termux, replacing `ADMIN_KEY` with the
key printed at startup:

```sh
curl 'http://127.0.0.1:8787/__debug/state?key=ADMIN_KEY'
```

Copy the newest paired match ID. Seed the prior series score and install a forced host-relative result before ending
the fight. The two phones can finish the fight normally; the local server will make both ranked result reports agree
with the forced score. Use these values:

| Final series | Seed before this match (`host_wins`, `guest_wins`) | Forced result (`host_score`) | Expected screen |
| --- | ---: | ---: | --- |
| 2-0 | 1, 0 | 2 | No revanche; server logs `series-decided` |
| 0-2 | 0, 1 | 0 | No revanche; server logs `series-decided` |
| 1-1 | 1, 0 | 0 | Revanche screen appears; a third match is available |
| 2-1 | 1, 1 | 2 | No revanche; server logs `series-decided` |

For each row, run the two debug commands below with the current `MATCH_ID`, the printed `ADMIN_KEY`, and that row's
values. Then let the match end on both phones:

```sh
curl -s -X POST http://127.0.0.1:8787/__debug/seed-series -H 'Content-Type: application/json' -d '{"key":"ADMIN_KEY","match":"MATCH_ID","host_wins":1,"guest_wins":0}'
curl -s -X POST http://127.0.0.1:8787/__debug/force-result -H 'Content-Type: application/json' -d '{"key":"ADMIN_KEY","match":"MATCH_ID","host_score":2}'
```

Change the numeric values to match the table. `host_score=2` means host wins, `0` means guest wins. These controls
only exist on the local HTTP server. After the match, the **1-1** case should let both players accept; acceptance
creates a different match ID, and both clients return to character/map selection for the third game. The other scores
close the series and skip the offer. The server's `/__debug/state` output shows the authoritative totals.

The debug URL accepts only `localhost`, `127/8`, and RFC1918 private IPv4 addresses, with an explicit port. The APK
also keeps local identity, result queue and session files under `.debug` names, separate from the production Ranked
profile. The local mock does not emulate the fighting simulation: the second phone still supplies the actual opponent.
