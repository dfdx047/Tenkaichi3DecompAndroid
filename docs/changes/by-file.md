# Mudanças por arquivo

Gerado por `python3 port/tools/change_map.py`: não edite à mão. Commits do mais novo ao mais velho, de `e1b9207~200` até o HEAD, no máximo 25 por arquivo. Merges ficam de fora: os commits deles aparecem com o autor original.

## `.gitattributes`

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -0: Add native Tag Team modes

## `.github/annotate.sh` ([build-ci](build-ci.md))

- `23d6efa5` 2026-10-08 dfdx047, +8 -0: Android app in English and Portuguese; CI shows build failures as annotations

## `.github/workflows/android.yml` ([build-ci](build-ci.md))

- `473b81df` 2026-10-25 Regulus CiberSec, +11 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +2 -1: BT2 no Android: motor importado pelo app, fora do GitHub
- `0f729094` 2026-10-24 Regulus CiberSec, +44 -2: BT2 Recomp para Android: motor libbt2.so
- `3950958c` 2026-10-19 Ariel R Rodrigues, +1 -1: Reverte android.yml ao da main
- `0b9e1c2a` 2026-10-19 Ariel R Rodrigues, +1 -1: Dragon Net: servidores A/B, Ranked no C, chat desligado, menos polling
- `c9fee92d` 2026-10-09 Claude, +18 -16: Release builds only, versioned tags, README update
- `20f13aed` 2026-10-08 dfdx047, +1 -0: Android: settings overlay sized for phones and handhelds, controller navigation; README status; CI nightly fix
- `547544df` 2026-10-08 dfdx047, +22 -3: CI: the latest build of main as the android-nightly pre-release
- `23d6efa5` 2026-10-08 dfdx047, +7 -4: Android app in English and Portuguese; CI shows build failures as annotations
- `dce2c134` 2026-10-08 dfdx047, +22 -3: Engine on Android arm64: build, loader and game activity
- `4d0ce356` 2026-10-08 dfdx047, +3 -5: android CI: use the runner's Android SDK, newer checkout and setup-java
- `750273dd` 2026-10-08 dfdx047, +103 -0: Add Dragon Rage, the Android launcher app, and its CI

## `.github/workflows/release.yml` ([build-ci](build-ci.md))

- `0914c6d2` 2026-10-27 z3xox, +21 -2: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.
- `0ca19719` 2026-10-27 z3xox, +4 -1: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `032e32ba` 2026-10-14 jllacchua, +1 -1: UI: clean chat rows, add chat size setting, and finalize upstream PR readiness
- `06f9c28b` 2026-10-14 jllacchua, +1 -1: Online chat: reset message history between sessions and record sender name
- `9cdcdcbd` 2026-10-14 z3xox, +1 -1: Licence: GNU AGPL v3 from here on
- `5158b9e7` 2026-10-13 jllacchua, +1 -1: ci: restore release workflow branch trigger
- `4be6435d` 2026-10-13 jllacchua, +1 -1: ci: trigger release workflow on feature branches
- `5b0ad7ed` 2026-10-13 z3xox, +6 -1: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `c9fee92d` 2026-10-09 Claude, +2 -4: Release builds only, versioned tags, README update
- `932d9b3a` 2026-10-08 dfdx047, +1 -1: CI toolchain: package the sysroot's libraries, clang runtime and glslc
- `750273dd` 2026-10-08 dfdx047, +1 -1: Add Dragon Rage, the Android launcher app, and its CI
- `434b8581` 2026-10-06 z3xox, +5 -5: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `cb63faa3` 2026-10-06 z3xox, +1 -1: Licence: MIT for the port's own code
- `ef51c9e8` 2026-10-06 z3xox, +66 -0: GitHub workflow for the release archives; README and notes for the disc-free build

## `.github/workflows/toolchain.yml` ([build-ci](build-ci.md))

- `2d6d0531` 2026-10-25 dfdevx2, +4 -1: Lite mode: dr_lite setting, SDL descriptor sets kept
- `8a22a0f0` 2026-10-08 dfdx047, +2 -0: CI toolchain: glslc's libc++
- `dc3c0c05` 2026-10-08 dfdx047, +21 -1: CI toolchain: SDL3 3.4.12 for Android arm64
- `932d9b3a` 2026-10-08 dfdx047, +16 -11: CI toolchain: package the sysroot's libraries, clang runtime and glslc
- `cbf6eb8d` 2026-10-08 dfdx047, +42 -0: CI: package the NDK arm64 sysroot as a build input

## `.gitignore`

- `344aa16c` 2026-10-31 dfdevx2, +1 -0: Tag Team as a setting of the app; the team and the licences in the app
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +1 -0: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -0: Add native Tag Team modes
- `c9e77091` 2026-10-23 dfdevx2, +1 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `3ca419c7` 2026-10-19 dfdevx2, +1 -0: Dragon Rage 0.8.1
- `0a4b5935` 2026-10-15 dfdevx2, +1 -0: Remove patch script [skip ci]
- `7b99f8ad` 2026-10-15 dfdevx2, +2 -0: Dragon Rage 0.6.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +1 -0: WIP: patch 1 and touch artwork
- `d8b10f7c` 2026-10-09 Claude, +1 -0: Ignore the generated engine relocation list (built by android.py, like the .dat)
- `dce2c134` 2026-10-08 dfdx047, +5 -0: Engine on Android arm64: build, loader and game activity
- `4d108fa3` 2026-10-07 z3xox, +5 -0: gitignore: the stages, songs and textures folders next to the game
- `434b8581` 2026-10-06 z3xox, +1 -0: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `35eb9563` 2026-10-06 z3xox, +1 -0: Linux release built in an Ubuntu 22.04 container (baseline x86-64, glibc 2.35, SDL3 from source); portability check in package.py
- `17e53ac8` 2026-10-06 z3xox, +3 -0: Setup: install.py (fresh copy + disc image -> playable build) and the bt3-setup window
- `2d923df4` 2026-10-06 z3xox, +3 -0: Remove the game screenshots and the built test program from the repository (kept out by .gitignore; build.sh makes test.elf)
- `c3084bcf` 2026-10-06 z3xox, +1 -0: Port: settings saved to bt3_settings.txt

## `CHANGELOG.es.md`

- `b38202f4` 2026-10-31 dfdevx2, +1 -1: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `8005f0bd` 2026-10-31 dfdevx2, +53 -96: Changelogs: 1.0, what changes for players
- `2d6d0531` 2026-10-25 dfdevx2, +3 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `49b9b4d4` 2026-10-25 dfdevx2, +16 -0: Docs: fullscreen and texture packs in the 0.9.0 report
- `8d9d5fb3` 2026-10-25 dfdevx2, +73 -26: Release 0.9.0: version, README, changelog
- `3ca419c7` 2026-10-19 dfdevx2, +30 -89: Dragon Rage 0.8.1
- `176d77e8` 2026-10-18 dfdevx2, +7 -7: Dragon Rage 0.8.0: Dragon Net
- `95ff74da` 2026-10-18 dfdevx2, +87 -19: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +20 -35: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +38 -0: Dragon Rage 0.7.3

## `CHANGELOG.md`

- `b38202f4` 2026-10-31 dfdevx2, +1 -1: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `8005f0bd` 2026-10-31 dfdevx2, +50 -89: Changelogs: 1.0, what changes for players
- `2d6d0531` 2026-10-25 dfdevx2, +3 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `49b9b4d4` 2026-10-25 dfdevx2, +15 -0: Docs: fullscreen and texture packs in the 0.9.0 report
- `8d9d5fb3` 2026-10-25 dfdevx2, +69 -26: Release 0.9.0: version, README, changelog
- `3ca419c7` 2026-10-19 dfdevx2, +30 -85: Dragon Rage 0.8.1
- `176d77e8` 2026-10-18 dfdevx2, +7 -6: Dragon Rage 0.8.0: Dragon Net
- `95ff74da` 2026-10-18 dfdevx2, +82 -19: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +20 -32: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +35 -0: Dragon Rage 0.7.3

## `CHANGELOG.pt.md`

- `b38202f4` 2026-10-31 dfdevx2, +2 -1: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `8005f0bd` 2026-10-31 dfdevx2, +50 -89: Changelogs: 1.0, what changes for players
- `2d6d0531` 2026-10-25 dfdevx2, +3 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `49b9b4d4` 2026-10-25 dfdevx2, +16 -0: Docs: fullscreen and texture packs in the 0.9.0 report
- `8d9d5fb3` 2026-10-25 dfdevx2, +68 -25: Release 0.9.0: version, README, changelog
- `3ca419c7` 2026-10-19 dfdevx2, +29 -90: Dragon Rage 0.8.1
- `176d77e8` 2026-10-18 dfdevx2, +7 -7: Dragon Rage 0.8.0: Dragon Net
- `95ff74da` 2026-10-18 dfdevx2, +90 -21: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +20 -35: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +38 -0: Dragon Rage 0.7.3

## `LICENSE`

- `7b99f8ad` 2026-10-15 dfdevx2, +656 -17: Dragon Rage 0.6.0 [skip ci]
- `9cdcdcbd` 2026-10-14 z3xox, +656 -17: Licence: GNU AGPL v3 from here on
- `cc003257` 2026-10-06 z3xox, +21 -674: Revert "Licence: GPL-3.0-or-later for the port's own code (replaces MIT)"
- `1a3ccd87` 2026-10-06 z3xox, +674 -21: Licence: GPL-3.0-or-later for the port's own code (replaces MIT)
- `cb63faa3` 2026-10-06 z3xox, +22 -0: Licence: MIT for the port's own code

## `LICENSE-MIT`

- `7b99f8ad` 2026-10-15 dfdevx2, +22 -0: Dragon Rage 0.6.0 [skip ci]
- `9cdcdcbd` 2026-10-14 z3xox, +22 -0: Licence: GNU AGPL v3 from here on

## `NATIVE_TAG_ONLINE.md`

- `02d1e354` 2026-10-26 Tag Team Mod contributors, +24 -68: Simplify the native online menu
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +1 -1: Add disc-based setup and simplify launchers
- `101de958` 2026-10-24 Tag Team Mod contributors, +215 -0: Add native Tag Team modes

## `NATIVE_TAG_TEAM.md`

- `02d1e354` 2026-10-26 Tag Team Mod contributors, +3 -4: Simplify the native online menu
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +10 -12: Add disc-based setup and simplify launchers
- `101de958` 2026-10-24 Tag Team Mod contributors, +616 -0: Add native Tag Team modes

## `Play.cmd`

- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +5 -0: Add disc-based setup and simplify launchers

## `Play.sh`

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +4 -0: Add Linux setup and improve native menus and music

## `README.es.md`

- `fafbb446` 2026-10-31 dfdevx2, +161 -17: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report
- `344aa16c` 2026-10-31 dfdevx2, +9 -4: Tag Team as a setting of the app; the team and the licences in the app
- `49b9b4d4` 2026-10-25 dfdevx2, +1 -1: Docs: fullscreen and texture packs in the 0.9.0 report
- `8d9d5fb3` 2026-10-25 dfdevx2, +10 -3: Release 0.9.0: version, README, changelog
- `f11e5f18` 2026-10-18 dfdevx2, +4 -2: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +4 -2: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +4 -2: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +5 -4: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +8 -2: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +174 -0: Dragon Rage 0.6.0 [skip ci]

## `README.md`

- `fafbb446` 2026-10-31 dfdevx2, +160 -16: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report
- `344aa16c` 2026-10-31 dfdevx2, +8 -3: Tag Team as a setting of the app; the team and the licences in the app
- `d4e61322` 2026-10-28 z3xox, +17 -15: Docs: 0.1.39, lobbies replace rooms. docs/port/README.md, a chapter: the list without Create Room, New Lobby, Join without Watch, by address as a lobby, Dragon Net Battle on lobbies, NET_VERSION 6 and how a 0.1.38 game is told apart both ways, FP_PEER_REQUIRED, the update's version compare, MIN_RELEASE 139 after publishing, and what was tested (a lobby by code with two matches and a spectator, by address, a 4-entrant tournament, quick match, 0.1.38.9 against 0.1.39 by address both ways, by code and in the quick match queue, a member without a fingerprint, the regular picture and speed checks); notes in the earlier chapters where they send players to Create Room or Watch. README.md: online play with lobbies. port/release/README.md: MIN_RELEASE after a release that changes NET_VERSION.
- `a3cf1192` 2026-10-28 z3xox, +11 -7: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `ffc63800` 2026-10-27 z3xox, +9 -1: Docs: the automatic update. docs/port/README.md, a chapter "Automatic updates": based on PR #107 by JosephLlacchua (what it brought and what was added on top of it), the version from the tag, the flow (the check, the line over the menus, Update now / Later / What's new, the download while playing, online play held, Restart now with its two steps of the rename trick, the undoing, the cleanup), the files never written, the signature and its key, the test switches and what was tested (and not). port/release/README.md (new): making a release, its two secrets, and the update signing key once (update_key.py generate, the secret UPDATE_SIGNING_KEY, the public key into port/src/update_key.h), with the credit to PR #107. README.md: updates for players; translations.md: the update's lines in the port catalog.
- `d01e8270` 2026-10-14 jllacchua, +3 -2: Auto-updater: in-game release download, progress bar, extraction and restart
- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +3 -2: Fix giant grabs, ring-outs and fallen fighter recovery
- `47b4d577` 2026-10-26 Tag Team Mod contributors, +1 -1: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `02d1e354` 2026-10-26 Tag Team Mod contributors, +2 -2: Simplify the native online menu
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +19 -12: Add Linux setup and improve native menus and music
- `49b9b4d4` 2026-10-25 dfdevx2, +1 -1: Docs: fullscreen and texture packs in the 0.9.0 report
- `8d9d5fb3` 2026-10-25 dfdevx2, +9 -2: Release 0.9.0: version, README, changelog
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +18 -8: Add disc-based setup and simplify launchers
- `101de958` 2026-10-24 Tag Team Mod contributors, +92 -166: Add native Tag Team modes
- `f11e5f18` 2026-10-18 dfdevx2, +4 -2: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +3 -2: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +4 -3: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +5 -4: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +7 -2: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +79 -79: Dragon Rage 0.6.0 [skip ci]
- `555adc01` 2026-10-15 z3xox, +2 -0: README: Discord badge with the server invite
- `9cdcdcbd` 2026-10-14 z3xox, +7 -2: Licence: GNU AGPL v3 from here on
- `06e46712` 2026-10-14 z3xox, +7 -0: README: the official source, and no browser sites are ours
- `313084b9` 2026-10-13 jllacchua, +4 -4: UI: improve online chat aesthetic, tab toggle, controls binding, and mouse copy support
- `b330baf9` 2026-10-13 jllacchua, +4 -0: Online chat: in-game text overlay between players with T key
- ... e mais 37 (`git log -- README.md`)

## `README.pt.md`

- `fafbb446` 2026-10-31 dfdevx2, +161 -16: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report
- `344aa16c` 2026-10-31 dfdevx2, +9 -4: Tag Team as a setting of the app; the team and the licences in the app
- `49b9b4d4` 2026-10-25 dfdevx2, +1 -1: Docs: fullscreen and texture packs in the 0.9.0 report
- `8d9d5fb3` 2026-10-25 dfdevx2, +9 -2: Release 0.9.0: version, README, changelog
- `f11e5f18` 2026-10-18 dfdevx2, +4 -2: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +4 -2: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +4 -2: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +5 -4: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +8 -2: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +172 -0: Dragon Rage 0.6.0 [skip ci]

## `Setup.cmd`

- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +9 -0: Add disc-based setup and simplify launchers

## `Setup.sh`

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +7 -0: Add Linux setup and improve native menus and music

## `android/.gitignore` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +9 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/build.gradle.kts` ([launcher](launcher.md))

- `3ea03f80` 2026-10-31 dfdevx2, +1 -1: BT2 online: its screens in the three languages
- `2d6d0531` 2026-10-25 dfdevx2, +1 -5: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +5 -1: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +4 -0: BT2 no Android: motor importado pelo app, fora do GitHub
- `d6467b24` 2026-10-25 dfdevx2, +1 -0: Texture packs: used on Android, in both games; more zips read
- `8d9d5fb3` 2026-10-25 dfdevx2, +2 -2: Release 0.9.0: version, README, changelog
- `00aada9d` 2026-10-24 Regulus CiberSec, +8 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações
- `3ca419c7` 2026-10-19 dfdevx2, +2 -2: Dragon Rage 0.8.1
- `f11e5f18` 2026-10-18 dfdevx2, +2 -2: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +2 -2: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +2 -2: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +2 -2: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +7 -2: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +2 -2: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +2 -2: Dragon Rage 0.5.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +7 -2: Dragon Rage 0.4.0 [skip ci]
- `52232f3a` 2026-10-08 dfdx047, +8 -0: Android: the game's data list was shifted; a fixed key for test builds
- `dce2c134` 2026-10-08 dfdx047, +5 -0: Engine on Android arm64: build, loader and game activity
- `750273dd` 2026-10-08 dfdx047, +74 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/proguard-rules.pro` ([launcher](launcher.md))

- `d6467b24` 2026-10-25 dfdevx2, +5 -0: Texture packs: used on Android, in both games; more zips read
- `dce2c134` 2026-10-08 dfdx047, +2 -1: Engine on Android arm64: build, loader and game activity
- `750273dd` 2026-10-08 dfdx047, +2 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/debug/AndroidManifest.xml` ([launcher](launcher.md))

- `cab206e6` 2026-10-25 Regulus CiberSec, +7 -0: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)

## `android/app/src/debug/res/xml/bt2_dev_network.xml` ([launcher](launcher.md))

- `cab206e6` 2026-10-25 Regulus CiberSec, +7 -0: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)

## `android/app/src/main/AndroidManifest.xml` ([launcher](launcher.md))

- `424283db` 2026-10-27 Regulus CiberSec, +7 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `d4467759` 2026-10-26 Regulus CiberSec, +2 -0: BT2 online: pareamento entre redes (STUN esperado, endereços em ordem, falha de verdade) e registro da rede no log
- `9693e97e` 2026-10-24 Regulus CiberSec, +12 -0: App: Budokai Tenkaichi 2 ao lado do BT3
- `e3df9aa1` 2026-10-20 dfdevx2, +11 -0: BT4 bridge step 1
- `c3b7c86c` 2026-10-11 dfdevx2, +10 -0: Dragon Rage 0.4.0 [skip ci]
- `b5ba41b7` 2026-10-09 Claude, +12 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About
- `23d6efa5` 2026-10-08 dfdx047, +1 -0: Android app in English and Portuguese; CI shows build failures as annotations
- `dce2c134` 2026-10-08 dfdx047, +22 -1: Engine on Android arm64: build, loader and game activity
- `750273dd` 2026-10-08 dfdx047, +29 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/assets/bt2/menu-item/gohan.txt` ([launcher](launcher.md))

- `3c19f7d9` 2026-10-26 Regulus CiberSec, +1 -0: BT2 online: título que flutua, Gohan em vídeo em loop, 16 ganchos e retorno ao Modo Online

## `android/app/src/main/assets/bt2/unlock/BASLUS-21441DBZT2/entries.txt` ([launcher](launcher.md))

- `98b4cedc` 2026-10-25 Regulus CiberSec, +4 -0: BT2 no Android: save "tudo desbloqueado" dentro do app

## `android/app/src/main/cpp/loader.c` ([bt3-engine](bt3-engine.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +14 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `344aa16c` 2026-10-31 dfdevx2, +9 -2: Tag Team as a setting of the app; the team and the licences in the app
- `d54095dd` 2026-10-29 Regulus CiberSec, +13 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `2d6d0531` 2026-10-25 dfdevx2, +5 -1: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +82 -1: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `5707f85b` 2026-10-22 dfdevx2, +74 -0: Fixes from logs, touch pause menu, BT4 menu
- `95ff74da` 2026-10-18 dfdevx2, +6 -0: Dragon Rage 0.8.0: Dragon Net
- `7b99f8ad` 2026-10-15 dfdevx2, +12 -0: Dragon Rage 0.6.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +3 -2: WIP: patch 1 and touch artwork
- `f19cbe5c` 2026-10-11 dfdevx2, +118 -0: feat(android): add customizable game-specific touch controls
- `0ab41362` 2026-10-09 Claude, +157 -15: Android: engine loadable anywhere below 4 GB; registers and stack movable
- `0e56e90c` 2026-10-08 Claude, +59 -1: Android: claim the engine's fixed addresses when libmain.so loads
- `87db39ee` 2026-10-08 Claude, +33 -0: Android: custom GPU drivers (adrenotools), bug report export, region message
- `4edb0841` 2026-10-08 Claude, +76 -4: Android loader: safe watchdog peeks (off by default), crash report in the log
- `68aa7428` 2026-10-08 Claude, +27 -0: Android: on-screen controller with layout editor, macros and three looks
- `d285f5d6` 2026-10-08 Claude, +13 -0: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `bec469bc` 2026-10-08 dfdx047, +41 -0: Android loader: game variables in the watchdog's report (BT3_PEEK)
- `1eb77ecc` 2026-10-08 dfdx047, +114 -0: Android loader: a watchdog that logs where every thread is
- `def0472b` 2026-10-08 dfdx047, +16 -0: Android: heartbeat in the log, full screen, bt3_env.txt for test switches
- `69457fad` 2026-10-08 dfdx047, +2 -1: Android: the engine at 0x03000000 and the port's region below the Java heap
- `6ca73cbb` 2026-10-08 dfdx047, +1 -0: Android: unlock-all from the app, back button opens the settings; toolchain fix
- `dce2c134` 2026-10-08 dfdx047, +214 -0: Engine on Android arm64: build, loader and game activity

## `android/app/src/main/java/com/dfdx047/dragonrage/AppViewModel.kt` ([launcher](launcher.md))

- `4a72a23c` 2026-10-31 dfdevx2, +14 -1: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `8cbd9d5a` 2026-10-31 dfdevx2, +5 -1: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `e87e44d9` 2026-10-26 Regulus CiberSec, +1 -1: Mescla 0.9.0: ajustes de compilação (install com report nomeado, import do viewModels)
- `2d6d0531` 2026-10-25 dfdevx2, +13 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `d6467b24` 2026-10-25 dfdevx2, +2 -0: Texture packs: used on Android, in both games; more zips read
- `d138e3a5` 2026-10-25 dfdevx2, +1 -1: Launcher: log the BT4 renderer on start
- `0c7ed754` 2026-10-25 dfdevx2, +2 -1: Dragon Net: back off on HTTP 429
- `00aada9d` 2026-10-24 Regulus CiberSec, +2 -27: App: abas BT3 \| BT2, cada jogo com as próprias configurações
- `9693e97e` 2026-10-24 Regulus CiberSec, +36 -4: App: Budokai Tenkaichi 2 ao lado do BT3
- `e3df9aa1` 2026-10-20 dfdevx2, +24 -11: BT4 bridge step 1
- `176d77e8` 2026-10-18 dfdevx2, +11 -0: Dragon Rage 0.8.0: Dragon Net
- `95ff74da` 2026-10-18 dfdevx2, +90 -18: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +11 -1: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +59 -4: Dragon Rage 0.7.3
- `1867006b` 2026-10-17 dfdevx2, +25 -0: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +84 -0: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +22 -1: Dragon Rage 0.5.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +19 -0: Dragon Rage 0.4.0 [skip ci]
- `6e471376` 2026-10-09 Claude, +5 -1: Fix launch crash: start the update check after _update is initialised
- `b5ba41b7` 2026-10-09 Claude, +126 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About
- `87db39ee` 2026-10-08 Claude, +46 -0: Android: custom GPU drivers (adrenotools), bug report export, region message
- `68aa7428` 2026-10-08 Claude, +13 -0: Android: on-screen controller with layout editor, macros and three looks
- `23d6efa5` 2026-10-08 dfdx047, +45 -27: Android app in English and Portuguese; CI shows build failures as annotations
- `dce2c134` 2026-10-08 dfdx047, +4 -1: Engine on Android arm64: build, loader and game activity
- `750273dd` 2026-10-08 dfdx047, +243 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/Bt2ViewModel.kt` ([bt2](bt2.md))

- `d9618feb` 2026-10-31 dfdevx2, +19 -47: BT2: only the ISO: the engine in the APK, the image read where it is
- `424283db` 2026-10-27 Regulus CiberSec, +2 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +47 -2: BT2 no Android: motor importado pelo app, fora do GitHub
- `5a417d3f` 2026-10-24 Regulus CiberSec, +28 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +259 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/java/com/dfdx047/dragonrage/MainActivity.kt` ([launcher](launcher.md))

- `297c8a00` 2026-10-31 dfdevx2, +0 -11: Launcher: controllers navigate it again
- `e87e44d9` 2026-10-26 Regulus CiberSec, +1 -0: Mescla 0.9.0: ajustes de compilação (install com report nomeado, import do viewModels)
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +2 -3: BT2 no Android: motor importado pelo app, fora do GitHub
- `fda328fb` 2026-10-25 Regulus CiberSec, +14 -0: BT2 no Android: controle só dentro do jogo, como um de PS2
- `f3557974` 2026-10-25 dfdevx2, +11 -7: Launcher: the side rail scrolls (BT3's nine pages on 1080p)
- `5a417d3f` 2026-10-24 Regulus CiberSec, +3 -1: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +52 -12: App: abas BT3 \| BT2, cada jogo com as próprias configurações
- `e3df9aa1` 2026-10-20 dfdevx2, +41 -16: BT4 bridge step 1
- `2d8a5a72` 2026-10-18 dfdevx2, +2 -0: Dragon Rage 0.7.3
- `1867006b` 2026-10-17 dfdevx2, +3 -1: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +52 -6: Dragon Rage 0.6.0 [skip ci]
- `b5ba41b7` 2026-10-09 Claude, +3 -1: App: in-app update from GitHub releases, texture pack download, GitHub button in About
- `68aa7428` 2026-10-08 Claude, +4 -0: Android: on-screen controller with layout editor, macros and three looks
- `23d6efa5` 2026-10-08 dfdx047, +10 -8: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +148 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/PlayShortcutActivity.kt` ([launcher](launcher.md))

- `c3b7c86c` 2026-10-11 dfdevx2, +23 -0: Dragon Rage 0.4.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/data/ActivityLog.kt` ([launcher](launcher.md))

- `95ff74da` 2026-10-18 dfdevx2, +69 -0: Dragon Rage 0.8.0: Dragon Net

## `android/app/src/main/java/com/dfdx047/dragonrage/data/AppUpdates.kt` ([launcher](launcher.md))

- `2d6d0531` 2026-10-25 dfdevx2, +5 -7: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +7 -5: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `2d8a5a72` 2026-10-18 dfdevx2, +5 -4: Dragon Rage 0.7.3
- `7b99f8ad` 2026-10-15 dfdevx2, +1 -1: Dragon Rage 0.6.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +10 -2: Dragon Rage 0.4.0 [skip ci]
- `b5ba41b7` 2026-10-09 Claude, +99 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2Drivers.kt` ([bt2](bt2.md))

- `00aada9d` 2026-10-24 Regulus CiberSec, +25 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2MenuItem.kt` ([bt2](bt2.md))

- `cfac7c3c` 2026-10-25 Regulus CiberSec, +31 -0: BT2 no Android: imagens do item Online Mode vêm dentro do app, desenhadas pelo projeto

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2Online.kt` ([bt2](bt2.md))

- `3ea03f80` 2026-10-31 dfdevx2, +19 -12: BT2 online: its screens in the three languages
- `f0bd6511` 2026-10-31 dfdevx2, +2 -0: Requests: 2 s polls, backoff on 429, none in the background; the relay socket checks the host
- `6340a148` 2026-10-25 Regulus CiberSec, +18 -5: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2
- `473b81df` 2026-10-25 Regulus CiberSec, +11 -8: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `cab206e6` 2026-10-25 Regulus CiberSec, +108 -0: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2Renderer.kt` ([bt2](bt2.md))

- `90e4838b` 2026-10-25 Regulus CiberSec, +6 -1: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `5a417d3f` 2026-10-24 Regulus CiberSec, +83 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2Settings.kt` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +47 -5: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +83 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2Unlock.kt` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +9 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `5a417d3f` 2026-10-24 Regulus CiberSec, +44 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Bt4Installer.kt` ([bt4](bt4.md))

- `d9618feb` 2026-10-31 dfdevx2, +1 -1: BT2: only the ISO: the engine in the APK, the image read where it is
- `f689660f` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `22c21670` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `9ecce978` 2026-10-23 dfdevx2, +42 -10: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +3 -2: Fixes from logs, touch pause menu, BT4 menu
- `e3df9aa1` 2026-10-20 dfdevx2, +102 -0: BT4 bridge step 1

## `android/app/src/main/java/com/dfdx047/dragonrage/data/DriverSources.kt` ([launcher](launcher.md))

- `9ecce978` 2026-10-23 dfdevx2, +28 -5: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `7b99f8ad` 2026-10-15 dfdevx2, +47 -0: Dragon Rage 0.6.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/data/EngineSettings.kt` ([launcher](launcher.md))

- `4a72a23c` 2026-10-31 dfdevx2, +2 -1: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `8cbd9d5a` 2026-10-31 dfdevx2, +1 -1: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `344aa16c` 2026-10-31 dfdevx2, +2 -0: Tag Team as a setting of the app; the team and the licences in the app
- `2d6d0531` 2026-10-25 dfdevx2, +2 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `1bc64e27` 2026-10-25 dfdevx2, +1 -0: BT4: ARMSX2 by default, 60 fps fallback
- `c9e77091` 2026-10-23 dfdevx2, +5 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +4 -0: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `1867006b` 2026-10-17 dfdevx2, +1 -0: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +5 -0: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +2 -0: Dragon Rage 0.5.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +1 -0: Dragon Rage 0.4.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +3 -1: WIP: patch 1 and touch artwork
- `c9fee92d` 2026-10-09 Claude, +5 -1: Release builds only, versioned tags, README update
- `23d6efa5` 2026-10-08 dfdx047, +1 -1: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +75 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/data/FileUtils.kt` ([launcher](launcher.md))

- `d6467b24` 2026-10-25 dfdevx2, +27 -3: Texture packs: used on Android, in both games; more zips read
- `7b99f8ad` 2026-10-15 dfdevx2, +1 -1: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +8 -2: Dragon Rage 0.5.0 [skip ci]
- `23d6efa5` 2026-10-08 dfdx047, +2 -2: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +99 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Game.kt` ([launcher](launcher.md))

- `e3df9aa1` 2026-10-20 dfdevx2, +17 -0: BT4 bridge step 1

## `android/app/src/main/java/com/dfdx047/dragonrage/data/GameDataInstaller.kt` ([launcher](launcher.md))

- `d9618feb` 2026-10-31 dfdevx2, +54 -35: BT2: only the ISO: the engine in the APK, the image read where it is
- `00aada9d` 2026-10-24 Regulus CiberSec, +3 -1: App: abas BT3 \| BT2, cada jogo com as próprias configurações
- `9693e97e` 2026-10-24 Regulus CiberSec, +61 -1: App: Budokai Tenkaichi 2 ao lado do BT3
- `5707f85b` 2026-10-22 dfdevx2, +33 -1: Fixes from logs, touch pause menu, BT4 menu
- `1867006b` 2026-10-17 dfdevx2, +25 -0: Dragon Rage 0.7.0
- `87db39ee` 2026-10-08 Claude, +3 -1: Android: custom GPU drivers (adrenotools), bug report export, region message
- `d285f5d6` 2026-10-08 Claude, +31 -44: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `23d6efa5` 2026-10-08 dfdx047, +13 -16: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +204 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/data/GamePaths.kt` ([launcher](launcher.md))

- `d9618feb` 2026-10-31 dfdevx2, +3 -1: BT2: only the ISO: the engine in the APK, the image read where it is
- `9693e97e` 2026-10-24 Regulus CiberSec, +21 -1: App: Budokai Tenkaichi 2 ao lado do BT3
- `e3df9aa1` 2026-10-20 dfdevx2, +8 -2: BT4 bridge step 1
- `750273dd` 2026-10-08 dfdx047, +39 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/data/GpuDrivers.kt` ([launcher](launcher.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +7 -2: Dragon Rage 0.6.0 [skip ci]
- `87db39ee` 2026-10-08 Claude, +83 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `android/app/src/main/java/com/dfdx047/dragonrage/data/GpuProfiles.kt` ([launcher](launcher.md))

- `2d6d0531` 2026-10-25 dfdevx2, +16 -1: Lite mode: dr_lite setting, SDL descriptor sets kept
- `84b31f2e` 2026-10-25 dfdevx2, +13 -2: GPU profiles: measured G57 MC4 and Adreno 650
- `7b99f8ad` 2026-10-15 dfdevx2, +96 -0: Dragon Rage 0.6.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/data/IsoImage.kt` ([launcher](launcher.md))

- `9693e97e` 2026-10-24 Regulus CiberSec, +3 -0: App: Budokai Tenkaichi 2 ao lado do BT3
- `23d6efa5` 2026-10-08 dfdx047, +2 -2: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +83 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/data/LastRun.kt` ([launcher](launcher.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +2 -1: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `f689660f` 2026-10-26 Regulus CiberSec, +13 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `22c21670` 2026-10-26 Regulus CiberSec, +13 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `2d8a5a72` 2026-10-18 dfdevx2, +13 -3: Dragon Rage 0.7.3
- `1867006b` 2026-10-17 dfdevx2, +33 -0: Dragon Rage 0.7.0

## `android/app/src/main/java/com/dfdx047/dragonrage/data/LogExport.kt` ([launcher](launcher.md))

- `95ff74da` 2026-10-18 dfdevx2, +35 -0: Dragon Rage 0.8.0: Dragon Net
- `87db39ee` 2026-10-08 Claude, +68 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Mods.kt` ([launcher](launcher.md))

- `23d6efa5` 2026-10-08 dfdx047, +10 -10: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +198 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Net.kt` ([launcher](launcher.md))

- `b5ba41b7` 2026-10-09 Claude, +72 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About

## `android/app/src/main/java/com/dfdx047/dragonrage/data/OnlineStatus.kt` ([online](online.md))

- `fe6cf04e` 2026-10-31 dfdevx2, +1 -1: Online: 429 match, relay during a match, lobby timeout, banner version, getenv
- `0c7ed754` 2026-10-25 dfdevx2, +23 -4: Dragon Net: back off on HTTP 429
- `176d77e8` 2026-10-18 dfdevx2, +3 -5: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +28 -5: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +12 -0: Dragon Rage 0.7.3
- `1867006b` 2026-10-17 dfdevx2, +70 -0: Dragon Rage 0.7.0

## `android/app/src/main/java/com/dfdx047/dragonrage/data/PerfReport.kt` ([launcher](launcher.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +62 -0: Dragon Rage 0.6.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/data/Renderer.kt` ([launcher](launcher.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +4 -4: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +75 -0: Dragon Rage 0.5.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/data/TexturePacks.kt` ([launcher](launcher.md))

- `d6467b24` 2026-10-25 dfdevx2, +23 -1: Texture packs: used on Android, in both games; more zips read
- `7b99f8ad` 2026-10-15 dfdevx2, +5 -2: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +55 -11: Dragon Rage 0.5.0 [skip ci]
- `b5ba41b7` 2026-10-09 Claude, +11 -6: App: in-app update from GitHub releases, texture pack download, GitHub button in About
- `23d6efa5` 2026-10-08 dfdx047, +8 -8: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +84 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/data/UiPrefs.kt` ([launcher](launcher.md))

- `e3df9aa1` 2026-10-20 dfdevx2, +12 -1: BT4 bridge step 1
- `750273dd` 2026-10-08 dfdx047, +31 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2Bridge.kt` ([bt2](bt2.md))

- `dfe15ac2` 2026-10-24 Regulus CiberSec, +3 -0: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +32 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2GameActivity.kt` ([bt2](bt2.md))

- `128c1003` 2026-10-31 dfdevx2, +8 -0: BT2: the build needs the kernel code; an error is said, not a black screen; output scale fills the screen
- `d9618feb` 2026-10-31 dfdevx2, +23 -11: BT2: only the ISO: the engine in the APK, the image read where it is
- `424283db` 2026-10-27 Regulus CiberSec, +21 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `d4467759` 2026-10-26 Regulus CiberSec, +17 -0: BT2 online: pareamento entre redes (STUN esperado, endereços em ordem, falha de verdade) e registro da rede no log
- `cf0af222` 2026-10-26 Regulus CiberSec, +18 -0: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `cab206e6` 2026-10-25 Regulus CiberSec, +5 -0: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)
- `cfac7c3c` 2026-10-25 Regulus CiberSec, +2 -0: BT2 no Android: imagens do item Online Mode vêm dentro do app, desenhadas pelo projeto
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +12 -0: BT2 no Android: motor importado pelo app, fora do GitHub
- `90e4838b` 2026-10-25 Regulus CiberSec, +5 -1: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +14 -0: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +50 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +11 -2: App: abas BT3 \| BT2, cada jogo com as próprias configurações
- `9693e97e` 2026-10-24 Regulus CiberSec, +34 -0: App: Budokai Tenkaichi 2 ao lado do BT3

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2Menu.kt` ([bt2](bt2.md))

- `90e4838b` 2026-10-25 Regulus CiberSec, +12 -0: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +3 -2: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +347 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2OnlineOverlay.kt` ([bt2](bt2.md))

- `3ea03f80` 2026-10-31 dfdevx2, +21 -21: BT2 online: its screens in the three languages
- `f0bd6511` 2026-10-31 dfdevx2, +21 -4: Requests: 2 s polls, backoff on 429, none in the background; the relay socket checks the host
- `424283db` 2026-10-27 Regulus CiberSec, +60 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `d4467759` 2026-10-26 Regulus CiberSec, +52 -9: BT2 online: pareamento entre redes (STUN esperado, endereços em ordem, falha de verdade) e registro da rede no log
- `6340a148` 2026-10-25 Regulus CiberSec, +19 -0: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2
- `473b81df` 2026-10-25 Regulus CiberSec, +36 -7: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `cab206e6` 2026-10-25 Regulus CiberSec, +310 -0: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2PerfOverlay.kt` ([bt2](bt2.md))

- `dfe15ac2` 2026-10-24 Regulus CiberSec, +123 -0: BT2 no Android: personagens com cores, painel de desempenho e otimizações

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2RelaunchActivity.kt` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +22 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt4Activity.kt` ([launcher](launcher.md))

- `4a72a23c` 2026-10-31 dfdevx2, +7 -0: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `1bc64e27` 2026-10-25 dfdevx2, +27 -1: BT4: ARMSX2 by default, 60 fps fallback
- `c9e77091` 2026-10-23 dfdevx2, +34 -3: BT4: ARMSX2 renderer (libbt4gsx)
- `5707f85b` 2026-10-22 dfdevx2, +2 -0: Fixes from logs, touch pause menu, BT4 menu
- `e3df9aa1` 2026-10-20 dfdevx2, +39 -0: BT4 bridge step 1

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/EngineBridge.kt` ([launcher](launcher.md))

- `d9618feb` 2026-10-31 dfdevx2, +2 -4: BT2: only the ISO: the engine in the APK, the image read where it is
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +6 -1: BT2 no Android: motor importado pelo app, fora do GitHub
- `9693e97e` 2026-10-24 Regulus CiberSec, +9 -8: App: Budokai Tenkaichi 2 ao lado do BT3
- `e3df9aa1` 2026-10-20 dfdevx2, +5 -2: BT4 bridge step 1
- `dce2c134` 2026-10-08 dfdx047, +6 -7: Engine on Android arm64: build, loader and game activity
- `750273dd` 2026-10-08 dfdx047, +22 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/GameActivity.kt` ([launcher](launcher.md))

- `344aa16c` 2026-10-31 dfdevx2, +4 -0: Tag Team as a setting of the app; the team and the licences in the app
- `d54095dd` 2026-10-29 Regulus CiberSec, +26 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `2d6d0531` 2026-10-25 dfdevx2, +8 -1: Lite mode: dr_lite setting, SDL descriptor sets kept
- `79ce6851` 2026-10-25 dfdevx2, +21 -0: Game: hide the system bars before the surface is made
- `0c7ed754` 2026-10-25 dfdevx2, +3 -1: Dragon Net: back off on HTTP 429
- `8e838c71` 2026-10-25 dfdevx2, +12 -0: Game menu: START + SELECT closes it too
- `a38b7c1f` 2026-10-24 dfdevx2, +23 -0: Touch pad hidden at start when a stick controller is connected
- `5a417d3f` 2026-10-24 Regulus CiberSec, +16 -6: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +9 -2: App: abas BT3 \| BT2, cada jogo com as próprias configurações
- `9693e97e` 2026-10-24 Regulus CiberSec, +12 -7: App: Budokai Tenkaichi 2 ao lado do BT3
- `9ecce978` 2026-10-23 dfdevx2, +9 -0: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +60 -11: Fixes from logs, touch pause menu, BT4 menu
- `e3df9aa1` 2026-10-20 dfdevx2, +7 -4: BT4 bridge step 1
- `7b99f8ad` 2026-10-15 dfdevx2, +63 -6: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +14 -3: Dragon Rage 0.5.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +30 -6: Dragon Rage 0.4.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +26 -1: WIP: patch 1 and touch artwork
- `0ab41362` 2026-10-09 Claude, +10 -6: Android: engine loadable anywhere below 4 GB; registers and stack movable
- `87db39ee` 2026-10-08 Claude, +6 -1: Android: custom GPU drivers (adrenotools), bug report export, region message
- `68aa7428` 2026-10-08 Claude, +146 -0: Android: on-screen controller with layout editor, macros and three looks
- `d285f5d6` 2026-10-08 Claude, +17 -0: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `dce2c134` 2026-10-08 dfdx047, +39 -0: Engine on Android arm64: build, loader and game activity

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/GameMenu.kt` ([launcher](launcher.md))

- `4a72a23c` 2026-10-31 dfdevx2, +3 -3: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `8e7dac04` 2026-10-28 Regulus CiberSec, +9 -3: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `9285f417` 2026-10-27 Androidxxt01, +3 -2: Show server beside Android lobby code
- `a2deb142` 2026-10-27 Androidxxt01, +3 -2: Show server beside Android lobby code
- `cf2c0fb3` 2026-10-27 Regulus CiberSec, +57 -2: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `c86573f2` 2026-10-27 Regulus CiberSec, +3 -3: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `a96bd6f8` 2026-10-27 Regulus CiberSec, +57 -2: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `9cd11a1b` 2026-10-27 Regulus CiberSec, +3 -3: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `1bc64e27` 2026-10-25 dfdevx2, +13 -8: BT4: ARMSX2 by default, 60 fps fallback
- `c9e77091` 2026-10-23 dfdevx2, +9 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +17 -0: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `744447a6` 2026-10-22 dfdevx2, +4 -0: Menu text colour, fight load crash fallback
- `5707f85b` 2026-10-22 dfdevx2, +814 -0: Fixes from logs, touch pause menu, BT4 menu

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/MenuBridge.kt` ([launcher](launcher.md))

- `8e7dac04` 2026-10-28 Regulus CiberSec, +1 -0: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `cf2c0fb3` 2026-10-27 Regulus CiberSec, +5 -0: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `a96bd6f8` 2026-10-27 Regulus CiberSec, +5 -0: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `5707f85b` 2026-10-22 dfdevx2, +26 -0: Fixes from logs, touch pause menu, BT4 menu

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/RelaySocket.kt` ([online](online.md))

- `f0bd6511` 2026-10-31 dfdevx2, +5 -2: Requests: 2 s polls, backoff on 429, none in the background; the relay socket checks the host
- `d54095dd` 2026-10-29 Regulus CiberSec, +139 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `android/app/src/main/java/com/dfdx047/dragonrage/engine/TouchBridge.kt` ([touch-input](touch-input.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +14 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `bbd4c2b5` 2026-10-11 dfdevx2, +5 -1: Dragon Rage 0.5.0 [skip ci]
- `f19cbe5c` 2026-10-11 dfdevx2, +18 -0: feat(android): add customizable game-specific touch controls
- `68aa7428` 2026-10-08 Claude, +16 -0: Android: on-screen controller with layout editor, macros and three looks

## `android/app/src/main/java/com/dfdx047/dragonrage/touch/ClassicPadView.kt` ([launcher](launcher.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +22 -2: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `81e16c65` 2026-10-25 dfdevx2, +17 -1: Touch: classic pad fits square screens
- `5707f85b` 2026-10-22 dfdevx2, +17 -3: Fixes from logs, touch pause menu, BT4 menu
- `c3b7c86c` 2026-10-11 dfdevx2, +777 -0: Dragon Rage 0.4.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/touch/TouchConfig.kt` ([launcher](launcher.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +10 -1: Dragon Rage 0.6.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +16 -0: Dragon Rage 0.4.0 [skip ci]
- `f19cbe5c` 2026-10-11 dfdevx2, +43 -23: feat(android): add customizable game-specific touch controls
- `07280401` 2026-10-08 Claude, +3 -3: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `68aa7428` 2026-10-08 Claude, +197 -0: Android: on-screen controller with layout editor, macros and three looks

## `android/app/src/main/java/com/dfdx047/dragonrage/touch/TouchPadView.kt` ([launcher](launcher.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +21 -2: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `5a417d3f` 2026-10-24 Regulus CiberSec, +14 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `5707f85b` 2026-10-22 dfdevx2, +17 -3: Fixes from logs, touch pause menu, BT4 menu
- `7b99f8ad` 2026-10-15 dfdevx2, +335 -71: Dragon Rage 0.6.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +43 -33: Dragon Rage 0.4.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +90 -50: WIP: patch 1 and touch artwork
- `f19cbe5c` 2026-10-11 dfdevx2, +656 -73: feat(android): add customizable game-specific touch controls
- `68aa7428` 2026-10-08 Claude, +749 -0: Android: on-screen controller with layout editor, macros and three looks

## `android/app/src/main/java/com/dfdx047/dragonrage/touch/TouchSurface.kt` ([launcher](launcher.md))

- `5707f85b` 2026-10-22 dfdevx2, +37 -0: Fixes from logs, touch pause menu, BT4 menu
- `c3b7c86c` 2026-10-11 dfdevx2, +27 -0: Dragon Rage 0.4.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/components/Common.kt` ([launcher](launcher.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +41 -0: Dragon Rage 0.6.0 [skip ci]
- `750273dd` 2026-10-08 dfdx047, +137 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/components/DragonBall.kt` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +105 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/components/Markdown.kt` ([launcher](launcher.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +1 -1: Dragon Rage 0.7.3
- `c3b7c86c` 2026-10-11 dfdevx2, +141 -0: Dragon Rage 0.4.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/components/SkinLegend.kt` ([launcher](launcher.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +41 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/CheatsScreen.kt` ([launcher](launcher.md))

- `23d6efa5` 2026-10-08 dfdx047, +12 -13: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +94 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/ControlsScreen.kt` ([launcher](launcher.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +4 -1: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `7b99f8ad` 2026-10-15 dfdevx2, +12 -0: Dragon Rage 0.6.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +45 -17: Dragon Rage 0.4.0 [skip ci]
- `f19cbe5c` 2026-10-11 dfdevx2, +3 -5: feat(android): add customizable game-specific touch controls
- `68aa7428` 2026-10-08 Claude, +247 -0: Android: on-screen controller with layout editor, macros and three looks

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/DebugScreen.kt` ([launcher](launcher.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +136 -0: Dragon Rage 0.6.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/DriverAndLogCards.kt` ([launcher](launcher.md))

- `dc9f17f3` 2026-10-24 dfdevx2, +7 -4: Drivers: the whole row picks the driver
- `87db39ee` 2026-10-08 Claude, +85 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/DriversScreen.kt` ([launcher](launcher.md))

- `dc9f17f3` 2026-10-24 dfdevx2, +7 -4: Drivers: the whole row picks the driver
- `9ecce978` 2026-10-23 dfdevx2, +14 -4: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `7b99f8ad` 2026-10-15 dfdevx2, +164 -0: Dragon Rage 0.6.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/GpuOfferDialog.kt` ([launcher](launcher.md))

- `2f3ff7b2` 2026-10-25 dfdevx2, +5 -1: Lite: report checks, GPU dialog scrolls
- `2d6d0531` 2026-10-25 dfdevx2, +1 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `7b99f8ad` 2026-10-15 dfdevx2, +51 -0: Dragon Rage 0.6.0 [skip ci]

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/HomeScreen.kt` ([launcher](launcher.md))

- `b38202f4` 2026-10-31 dfdevx2, +2 -1: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `0c7ed754` 2026-10-25 dfdevx2, +1 -0: Dragon Net: back off on HTTP 429
- `00aada9d` 2026-10-24 Regulus CiberSec, +0 -51: App: abas BT3 \| BT2, cada jogo com as próprias configurações
- `9693e97e` 2026-10-24 Regulus CiberSec, +51 -0: App: Budokai Tenkaichi 2 ao lado do BT3
- `e3df9aa1` 2026-10-20 dfdevx2, +17 -12: BT4 bridge step 1
- `95ff74da` 2026-10-18 dfdevx2, +13 -4: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +3 -2: Dragon Rage 0.8.0: Dragon Net
- `1867006b` 2026-10-17 dfdevx2, +41 -6: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +145 -11: Dragon Rage 0.6.0 [skip ci]
- `c3b7c86c` 2026-10-11 dfdevx2, +12 -0: Dragon Rage 0.4.0 [skip ci]
- `d285f5d6` 2026-10-08 Claude, +12 -1: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `23d6efa5` 2026-10-08 dfdx047, +23 -23: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +189 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/LastRunDialog.kt` ([launcher](launcher.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +8 -1: Dragon Rage 0.7.3
- `1867006b` 2026-10-17 dfdevx2, +45 -0: Dragon Rage 0.7.0

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/ModsScreen.kt` ([launcher](launcher.md))

- `23d6efa5` 2026-10-08 dfdx047, +27 -26: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +212 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/SettingsScreen.kt` ([launcher](launcher.md))

- `4a72a23c` 2026-10-31 dfdevx2, +1 -1: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `344aa16c` 2026-10-31 dfdevx2, +26 -0: Tag Team as a setting of the app; the team and the licences in the app
- `2d6d0531` 2026-10-25 dfdevx2, +3 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `1bc64e27` 2026-10-25 dfdevx2, +9 -3: BT4: ARMSX2 by default, 60 fps fallback
- `c9e77091` 2026-10-23 dfdevx2, +12 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +45 -22: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `e3df9aa1` 2026-10-20 dfdevx2, +11 -9: BT4 bridge step 1
- `176d77e8` 2026-10-18 dfdevx2, +0 -18: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +11 -0: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +19 -0: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +1 -1: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +6 -0: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +3 -0: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +21 -4: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +20 -0: Dragon Rage 0.5.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +30 -2: WIP: patch 1 and touch artwork
- `b5ba41b7` 2026-10-09 Claude, +22 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About
- `c9fee92d` 2026-10-09 Claude, +22 -4: Release builds only, versioned tags, README update
- `87db39ee` 2026-10-08 Claude, +3 -0: Android: custom GPU drivers (adrenotools), bug report export, region message
- `68aa7428` 2026-10-08 Claude, +3 -18: Android: on-screen controller with layout editor, macros and three looks
- `23d6efa5` 2026-10-08 dfdx047, +44 -33: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +193 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/TexFailDialog.kt` ([launcher](launcher.md))

- `95ff74da` 2026-10-18 dfdevx2, +4 -3: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +70 -0: Dragon Rage 0.7.3

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/TextureDownloadDialog.kt` ([launcher](launcher.md))

- `b5ba41b7` 2026-10-09 Claude, +86 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/TexturesScreen.kt` ([launcher](launcher.md))

- `d6467b24` 2026-10-25 dfdevx2, +3 -1: Texture packs: used on Android, in both games; more zips read
- `2d8a5a72` 2026-10-18 dfdevx2, +15 -0: Dragon Rage 0.7.3
- `b5ba41b7` 2026-10-09 Claude, +19 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About
- `23d6efa5` 2026-10-08 dfdx047, +14 -13: Android app in English and Portuguese; CI shows build failures as annotations
- `750273dd` 2026-10-08 dfdx047, +135 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/UpdateDialog.kt` ([launcher](launcher.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +1 -1: Dragon Rage 0.7.3
- `c3b7c86c` 2026-10-11 dfdevx2, +64 -13: Dragon Rage 0.4.0 [skip ci]
- `b5ba41b7` 2026-10-09 Claude, +74 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/WelcomeScreen.kt` ([launcher](launcher.md))

- `e3df9aa1` 2026-10-20 dfdevx2, +114 -0: BT4 bridge step 1

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/bt2/Bt2ControlsScreen.kt` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +7 -1: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +281 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/bt2/Bt2HomeScreen.kt` ([bt2](bt2.md))

- `b38202f4` 2026-10-31 dfdevx2, +2 -2: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `2e034b59` 2026-10-31 dfdevx2, +3 -4: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `d9618feb` 2026-10-31 dfdevx2, +8 -50: BT2: only the ISO: the engine in the APK, the image read where it is
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +50 -5: BT2 no Android: motor importado pelo app, fora do GitHub
- `8881bf0a` 2026-10-25 Regulus CiberSec, +33 -0: App: cartão Dragon Net 2 (em breve) na tela inicial do BT2
- `00aada9d` 2026-10-24 Regulus CiberSec, +161 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/bt2/Bt2Pages.kt` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +26 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +140 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/bt2/Bt2SettingsDebug.kt` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +25 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +98 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/theme/Color.kt` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +46 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/theme/Theme.kt` ([launcher](launcher.md))

- `e3df9aa1` 2026-10-20 dfdevx2, +35 -2: BT4 bridge step 1
- `750273dd` 2026-10-08 dfdx047, +85 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/java/com/dfdx047/dragonrage/ui/theme/Type.kt` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +33 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/drawable/ic_launcher_background.xml` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +13 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/drawable/ic_launcher_foreground.xml` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +27 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/drawable/ic_launcher_monochrome.xml` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +6 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +6 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +6 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/values-es/strings.xml` ([launcher](launcher.md))

- `b38202f4` 2026-10-31 dfdevx2, +1 -0: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `4a72a23c` 2026-10-31 dfdevx2, +3 -2: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `d9618feb` 2026-10-31 dfdevx2, +9 -6: BT2: only the ISO: the engine in the APK, the image read where it is
- `f689660f` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `22c21670` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `2d6d0531` 2026-10-25 dfdevx2, +4 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +1 -2: BT2 no Android: motor importado pelo app, fora do GitHub
- `5dab1213` 2026-10-25 dfdevx2, +1 -1: Textures: help says what to do with a .rar or .7z pack and the room it needs
- `d6467b24` 2026-10-25 dfdevx2, +4 -2: Texture packs: used on Android, in both games; more zips read
- `0c7ed754` 2026-10-25 dfdevx2, +2 -0: Dragon Net: back off on HTTP 429
- `1bc64e27` 2026-10-25 dfdevx2, +7 -5: BT4: ARMSX2 by default, 60 fps fallback
- `9693e97e` 2026-10-24 Regulus CiberSec, +13 -0: App: Budokai Tenkaichi 2 ao lado do BT3
- `c9e77091` 2026-10-23 dfdevx2, +3 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +12 -2: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +3 -0: Fixes from logs, touch pause menu, BT4 menu
- `e3df9aa1` 2026-10-20 dfdevx2, +13 -0: BT4 bridge step 1
- `3ca419c7` 2026-10-19 dfdevx2, +5 -5: Dragon Rage 0.8.1
- `176d77e8` 2026-10-18 dfdevx2, +0 -4: Dragon Rage 0.8.0: Dragon Net
- `95ff74da` 2026-10-18 dfdevx2, +15 -3: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +13 -4: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +14 -0: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +1 -1: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +1 -0: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +24 -0: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +342 -0: Dragon Rage 0.6.0 [skip ci]

## `android/app/src/main/res/values-es/strings_bt2.xml` ([launcher](launcher.md))

- `2e034b59` 2026-10-31 dfdevx2, +3 -4: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `d9618feb` 2026-10-31 dfdevx2, +3 -15: BT2: only the ISO: the engine in the APK, the image read where it is
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +14 -0: BT2 no Android: motor importado pelo app, fora do GitHub
- `8881bf0a` 2026-10-25 Regulus CiberSec, +5 -0: App: cartão Dragon Net 2 (em breve) na tela inicial do BT2
- `90e4838b` 2026-10-25 Regulus CiberSec, +4 -0: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +6 -2: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +36 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +59 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/res/values-pt/strings.xml` ([launcher](launcher.md))

- `b38202f4` 2026-10-31 dfdevx2, +1 -0: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `4a72a23c` 2026-10-31 dfdevx2, +3 -2: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `d9618feb` 2026-10-31 dfdevx2, +9 -6: BT2: only the ISO: the engine in the APK, the image read where it is
- `f689660f` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `22c21670` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `2d6d0531` 2026-10-25 dfdevx2, +4 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +1 -2: BT2 no Android: motor importado pelo app, fora do GitHub
- `5dab1213` 2026-10-25 dfdevx2, +1 -1: Textures: help says what to do with a .rar or .7z pack and the room it needs
- `d6467b24` 2026-10-25 dfdevx2, +4 -2: Texture packs: used on Android, in both games; more zips read
- `0c7ed754` 2026-10-25 dfdevx2, +2 -0: Dragon Net: back off on HTTP 429
- `1bc64e27` 2026-10-25 dfdevx2, +7 -5: BT4: ARMSX2 by default, 60 fps fallback
- `9693e97e` 2026-10-24 Regulus CiberSec, +13 -0: App: Budokai Tenkaichi 2 ao lado do BT3
- `c9e77091` 2026-10-23 dfdevx2, +3 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +12 -2: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +3 -0: Fixes from logs, touch pause menu, BT4 menu
- `e3df9aa1` 2026-10-20 dfdevx2, +13 -0: BT4 bridge step 1
- `3ca419c7` 2026-10-19 dfdevx2, +5 -5: Dragon Rage 0.8.1
- `176d77e8` 2026-10-18 dfdevx2, +0 -4: Dragon Rage 0.8.0: Dragon Net
- `95ff74da` 2026-10-18 dfdevx2, +15 -3: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +13 -4: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +14 -0: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +1 -1: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +1 -0: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +24 -0: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +74 -1: Dragon Rage 0.6.0 [skip ci]
- ... e mais 10 (`git log -- android/app/src/main/res/values-pt/strings.xml`)

## `android/app/src/main/res/values-pt/strings_bt2.xml` ([launcher](launcher.md))

- `2e034b59` 2026-10-31 dfdevx2, +3 -4: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `d9618feb` 2026-10-31 dfdevx2, +3 -15: BT2: only the ISO: the engine in the APK, the image read where it is
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +14 -0: BT2 no Android: motor importado pelo app, fora do GitHub
- `8881bf0a` 2026-10-25 Regulus CiberSec, +5 -0: App: cartão Dragon Net 2 (em breve) na tela inicial do BT2
- `90e4838b` 2026-10-25 Regulus CiberSec, +4 -0: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +6 -2: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +36 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +59 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/res/values/colors.xml` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +4 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/values/strings.xml` ([launcher](launcher.md))

- `b38202f4` 2026-10-31 dfdevx2, +1 -0: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `4a72a23c` 2026-10-31 dfdevx2, +3 -2: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `d9618feb` 2026-10-31 dfdevx2, +9 -6: BT2: only the ISO: the engine in the APK, the image read where it is
- `f689660f` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `22c21670` 2026-10-26 Regulus CiberSec, +1 -1: Logs de 12 celulares: o aviso de textura só vale com pacote instalado; recusa do BT4 diz o que achou
- `2d6d0531` 2026-10-25 dfdevx2, +5 -1: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +1 -1: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +1 -2: BT2 no Android: motor importado pelo app, fora do GitHub
- `5dab1213` 2026-10-25 dfdevx2, +1 -1: Textures: help says what to do with a .rar or .7z pack and the room it needs
- `d6467b24` 2026-10-25 dfdevx2, +4 -2: Texture packs: used on Android, in both games; more zips read
- `0c7ed754` 2026-10-25 dfdevx2, +2 -0: Dragon Net: back off on HTTP 429
- `1bc64e27` 2026-10-25 dfdevx2, +7 -5: BT4: ARMSX2 by default, 60 fps fallback
- `9693e97e` 2026-10-24 Regulus CiberSec, +13 -0: App: Budokai Tenkaichi 2 ao lado do BT3
- `c9e77091` 2026-10-23 dfdevx2, +3 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +12 -2: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +3 -0: Fixes from logs, touch pause menu, BT4 menu
- `e3df9aa1` 2026-10-20 dfdevx2, +13 -0: BT4 bridge step 1
- `3ca419c7` 2026-10-19 dfdevx2, +5 -5: Dragon Rage 0.8.1
- `176d77e8` 2026-10-18 dfdevx2, +0 -4: Dragon Rage 0.8.0: Dragon Net
- `95ff74da` 2026-10-18 dfdevx2, +15 -3: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +13 -4: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +14 -0: Dragon Rage 0.7.3
- `02338589` 2026-10-17 dfdevx2, +1 -1: Dragon Rage 0.7.2
- `70441e5f` 2026-10-17 dfdevx2, +1 -0: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +24 -0: Dragon Rage 0.7.0
- ... e mais 12 (`git log -- android/app/src/main/res/values/strings.xml`)

## `android/app/src/main/res/values/strings_bt2.xml` ([launcher](launcher.md))

- `2e034b59` 2026-10-31 dfdevx2, +3 -4: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `d9618feb` 2026-10-31 dfdevx2, +3 -16: BT2: only the ISO: the engine in the APK, the image read where it is
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +14 -0: BT2 no Android: motor importado pelo app, fora do GitHub
- `8881bf0a` 2026-10-25 Regulus CiberSec, +5 -0: App: cartão Dragon Net 2 (em breve) na tela inicial do BT2
- `90e4838b` 2026-10-25 Regulus CiberSec, +4 -0: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +6 -2: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +36 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `00aada9d` 2026-10-24 Regulus CiberSec, +61 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/app/src/main/res/values/themes.xml` ([launcher](launcher.md))

- `dce2c134` 2026-10-08 dfdx047, +4 -0: Engine on Android arm64: build, loader and game activity
- `750273dd` 2026-10-08 dfdx047, +8 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/xml/data_extraction_rules.xml` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +9 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/app/src/main/res/xml/file_paths.xml` ([launcher](launcher.md))

- `b5ba41b7` 2026-10-09 Claude, +5 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About

## `android/app/src/main/res/xml/locales_config.xml` ([launcher](launcher.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +1 -0: Dragon Rage 0.6.0 [skip ci]
- `23d6efa5` 2026-10-08 dfdx047, +5 -0: Android app in English and Portuguese; CI shows build failures as annotations

## `android/app/src/main/res/xml/shortcuts.xml` ([launcher](launcher.md))

- `c3b7c86c` 2026-10-11 dfdevx2, +14 -0: Dragon Rage 0.4.0 [skip ci]

## `android/app/src/sideBySide/res/values/strings.xml` ([launcher](launcher.md))

- `00aada9d` 2026-10-24 Regulus CiberSec, +5 -0: App: abas BT3 \| BT2, cada jogo com as próprias configurações

## `android/build.gradle.kts` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +5 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/gradle.properties` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +4 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/gradle/libs.versions.toml` ([launcher](launcher.md))

- `d6467b24` 2026-10-25 dfdevx2, +2 -0: Texture packs: used on Android, in both games; more zips read
- `750273dd` 2026-10-08 dfdx047, +28 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/gradle/wrapper/gradle-wrapper.properties` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +7 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/gradlew` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +251 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/gradlew.bat` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +94 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/settings.gradle.kts` ([launcher](launcher.md))

- `750273dd` 2026-10-08 dfdx047, +16 -0: Add Dragon Rage, the Android launcher app, and its CI

## `android/texture-packs.json` ([launcher](launcher.md))

- `b5ba41b7` 2026-10-09 Claude, +3 -0: App: in-app update from GitHub releases, texture pack download, GitHub button in About

## `bt2/.editorconfig` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +5 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/.gitattributes` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/.gitignore` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +75 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/.vscode/settings.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +24 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/AGENTS.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +72 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/CMakeLists.txt` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +306 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/LICENSE` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +232 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/README.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +155 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/android/CMakeLists.txt` ([bt2](bt2.md))

- `128c1003` 2026-10-31 dfdevx2, +3 -0: BT2: the build needs the kernel code; an error is said, not a black screen; output scale fills the screen
- `424283db` 2026-10-27 Regulus CiberSec, +1 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +1 -0: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade
- `ad9c36e9` 2026-10-26 Regulus CiberSec, +1 -0: BT2 online: troca de entradas por campo (lockstep) testada no PC, e o que o BT3 faz para manter dois jogos em passo
- `cf0af222` 2026-10-26 Regulus CiberSec, +2 -1: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `6340a148` 2026-10-25 Regulus CiberSec, +2 -1: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2
- `473b81df` 2026-10-25 Regulus CiberSec, +13 -2: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `3ff439ea` 2026-10-25 Regulus CiberSec, +2 -0: BT2 no Android: item "Online Mode" no menu principal do jogo, com tela e música
- `5a417d3f` 2026-10-24 Regulus CiberSec, +21 -2: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `048cbd8d` 2026-10-24 Regulus CiberSec, +12 -0: BT2 no Android: geração no Linux, kernel, desempenho e opções da GPU
- `0f729094` 2026-10-24 Regulus CiberSec, +122 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/android/build.py` ([bt2](bt2.md))

- `1e7ea3d8` 2026-10-31 dfdevx2, +1 -1: BT2: the relay bounds the user name; the default build makes the online library too
- `d9618feb` 2026-10-31 dfdevx2, +1 -64: BT2: only the ISO: the engine in the APK, the image read where it is
- `473b81df` 2026-10-25 Regulus CiberSec, +18 -1: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +64 -1: BT2 no Android: motor importado pelo app, fora do GitHub
- `0f729094` 2026-10-24 Regulus CiberSec, +105 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/android/cmake/game-activity-config.cmake` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +6 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/android/cmake/games-controller-config.cmake` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +6 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/android/cmake/games-frame-pacing-config.cmake` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +6 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/cmake/bt2_sources.cmake` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +136 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/config/game_patches.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/config/ghidra.lock.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +19 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/config/parallel_gs.lock.json` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +2 -1: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +5 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/config/ps2recomp.lock.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +11 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/config/retail_revision.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +17 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/config/startup_abi.sources.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +363 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/config/xxhash.lock.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +3 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/PROG.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +63 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/guides/ANDROID.md` ([bt2](bt2.md))

- `d9618feb` 2026-10-31 dfdevx2, +12 -21: BT2: only the ISO: the engine in the APK, the image read where it is
- `bcd7fa70` 2026-10-25 Regulus CiberSec, +20 -1: BT2 no Android: motor importado pelo app, fora do GitHub
- `048cbd8d` 2026-10-24 Regulus CiberSec, +15 -0: BT2 no Android: geração no Linux, kernel, desempenho e opções da GPU
- `0f729094` 2026-10-24 Regulus CiberSec, +102 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/guides/BUILDING.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +141 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/guides/RUNTIME_LAYOUT.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +30 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/guides/TOOLS.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +745 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/disc/CDVD_STREAMING_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +15 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/disc/DISC_COMMAND_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/disc/DISC_SEARCH_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +9 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_CACHE_TRANSFER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +28 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_FILEIO_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +46 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_INTERRUPT_LOAD_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +39 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_OVERLAY_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_RPC_SCHEDULING_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +36 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_SOUND_SCHEDULING_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_THREAD_START_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +39 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/EE_TIMER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +27 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/PERFORMANCE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +115 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/RUNTIME_GENERALIZATION_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +56 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/STORY_MODE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/ee/WHOLE_GAME_COVERAGE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +50 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/FRAME_GIF_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +47 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GIF_DMA_BRINGUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +23 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GPU_RENDERING_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +47 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GS_BRINGUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +31 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GS_INTERRUPT_TIMING_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +25 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GS_READBACK_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +37 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GS_RENDERER_BRINGUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +24 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GS_TEXTURE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +80 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/GS_TEXTURE_STARTUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +28 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/HD_TEXTURES_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +87 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +31 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/IMAGE_QUALITY_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/LIVE_WINDOW_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +30 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/VIF_STARTUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +24 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/graphics/VU1_PIPELINE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +48 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/input/CONTROLLER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +48 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/input/DBCMAN_DMA_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +27 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/input/DBCMAN_RPC_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +31 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/input/SIO2_BRINGUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_HARDWARE_TIMER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +20 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_HEAP_CAPACITY_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_HEAP_STARTUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +30 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_IOMAN_DRIVER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +25 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_LOADFILE_BRIDGE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +24 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_NATIVE_EXECUTION_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_SIF_RPC_SERVER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_TIMED_WAIT_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/IOP_VBLANK_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +25 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/LOADFILE_IOP_MODULE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +23 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/MODULE_BUFFER_STARTUP_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/NATIVE_SERVICES_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +70 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/POST_RESET_IOP_SERVICES_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/iop/POST_RESET_SIF_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +26 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/launcher/LAUNCHER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +146 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/memory-card/MCSERV_RPC_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +30 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/memory-card/MEMORY_CARD_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/memory-card/SIO2_CARD_DMA_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +23 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/online/LOCKSTEP_PLAN.md` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +44 -12: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `ad9c36e9` 2026-10-26 Regulus CiberSec, +67 -0: BT2 online: troca de entradas por campo (lockstep) testada no PC, e o que o BT3 faz para manter dois jogos em passo

## `bt2/docs/plans/online/ONLINE_PLAN.md` ([bt2](bt2.md))

- `3c19f7d9` 2026-10-26 Regulus CiberSec, +11 -0: BT2 online: título que flutua, Gohan em vídeo em loop, 16 ganchos e retorno ao Modo Online
- `cffcb461` 2026-10-26 Regulus CiberSec, +6 -0: BT2 online: menu de Duelo com 1P VS 2P acesa ao conectar, queda do oponente e mudo na transição
- `751d478b` 2026-10-26 Regulus CiberSec, +13 -0: BT2 online: cobertura sobre o menu principal na troca para o Duelo e regras da partida
- `aa4e918e` 2026-10-26 Regulus CiberSec, +17 -0: BT2 online: o que o BT3 faz depois de conectar, e a chave de teste dos modos
- `4accbb51` 2026-10-26 Regulus CiberSec, +8 -0: BT2 online: tabela de modos lida do disco; a seleção de personagens está dentro do Dueling
- `5e2c25de` 2026-10-26 Regulus CiberSec, +16 -0: BT2 online: plano da etapa 3: como o jogo chega ao versus de dois jogadores
- `0476d5d5` 2026-10-25 Regulus CiberSec, +2 -2: BT2 online: a conexão direta entre o celular e o PC foi testada e funciona
- `473b81df` 2026-10-25 Regulus CiberSec, +25 -7: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `cab206e6` 2026-10-25 Regulus CiberSec, +63 -0: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)

## `bt2/docs/plans/sound/CRI_AUTODMA_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +31 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/CRI_RPC_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +56 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/CRI_SECOND_CHANNEL_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +35 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/CRI_THREAD_START_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +26 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/IOP_SPU2_TRANSFER_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +28 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/SDRDRV_RPC_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +26 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/SOUND_OUTPUT_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +130 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/SPU2_INITIALIZATION_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +25 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/sound/SPU2_VOICE_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/video/IPU_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/plans/video/SOFDEC_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +69 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/progress/2026-10.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2810 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/progress/ACTIVE_PASS.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +52 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/progress/VALIDATION.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +65 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/progress/components.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +54 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/progress/dashboard.svg` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +179 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/progress/latest.json` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +35 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/project/IMPLEMENTATION_PLAN.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +56 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/project/LEGAL.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/project/PROGRESS_TRACKING.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +74 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/reference/REFERENCE_ARCHITECTURE.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +327 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/docs/reference/REVERSE_ENGINEERING.md` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +327 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/plugin/bt2_plugin.h` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +12 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `3c19f7d9` 2026-10-26 Regulus CiberSec, +1 -1: BT2 online: título que flutua, Gohan em vídeo em loop, 16 ganchos e retorno ao Modo Online
- `473b81df` 2026-10-25 Regulus CiberSec, +85 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)

## `bt2/plugin/online/fight_session.cpp` ([bt2](bt2.md))

- `2e034b59` 2026-10-31 dfdevx2, +5 -0: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `424283db` 2026-10-27 Regulus CiberSec, +190 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC

## `bt2/plugin/online/fight_session.h` ([bt2](bt2.md))

- `2e034b59` 2026-10-31 dfdevx2, +1 -0: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `424283db` 2026-10-27 Regulus CiberSec, +92 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC

## `bt2/plugin/online/link.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +12 -3: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +28 -5: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade
- `d4467759` 2026-10-26 Regulus CiberSec, +61 -6: BT2 online: pareamento entre redes (STUN esperado, endereços em ordem, falha de verdade) e registro da rede no log
- `cffcb461` 2026-10-26 Regulus CiberSec, +14 -1: BT2 online: menu de Duelo com 1P VS 2P acesa ao conectar, queda do oponente e mudo na transição
- `cf0af222` 2026-10-26 Regulus CiberSec, +13 -0: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `6340a148` 2026-10-25 Regulus CiberSec, +32 -8: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2
- `473b81df` 2026-10-25 Regulus CiberSec, +166 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)

## `bt2/plugin/online/link.h` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +5 -1: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +14 -1: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade
- `d4467759` 2026-10-26 Regulus CiberSec, +9 -0: BT2 online: pareamento entre redes (STUN esperado, endereços em ordem, falha de verdade) e registro da rede no log
- `cffcb461` 2026-10-26 Regulus CiberSec, +5 -0: BT2 online: menu de Duelo com 1P VS 2P acesa ao conectar, queda do oponente e mudo na transição
- `cf0af222` 2026-10-26 Regulus CiberSec, +7 -1: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `6340a148` 2026-10-25 Regulus CiberSec, +13 -3: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2
- `473b81df` 2026-10-25 Regulus CiberSec, +46 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)

## `bt2/plugin/online/lockstep.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +3 -5: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `ad9c36e9` 2026-10-26 Regulus CiberSec, +151 -0: BT2 online: troca de entradas por campo (lockstep) testada no PC, e o que o BT3 faz para manter dois jogos em passo

## `bt2/plugin/online/lockstep.h` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +1 -1: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +1 -0: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade
- `ad9c36e9` 2026-10-26 Regulus CiberSec, +101 -0: BT2 online: troca de entradas por campo (lockstep) testada no PC, e o que o BT3 faz para manter dois jogos em passo

## `bt2/plugin/online/online_plugin.cpp` ([bt2](bt2.md))

- `2e034b59` 2026-10-31 dfdevx2, +23 -11: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `424283db` 2026-10-27 Regulus CiberSec, +134 -8: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `d4467759` 2026-10-26 Regulus CiberSec, +23 -5: BT2 online: pareamento entre redes (STUN esperado, endereços em ordem, falha de verdade) e registro da rede no log
- `17888f34` 2026-10-26 Regulus CiberSec, +33 -24: BT2 online: caminhada automática do Dueling até a seleção de personagens (em teste)
- `3c19f7d9` 2026-10-26 Regulus CiberSec, +90 -17: BT2 online: título que flutua, Gohan em vídeo em loop, 16 ganchos e retorno ao Modo Online
- `cffcb461` 2026-10-26 Regulus CiberSec, +130 -10: BT2 online: menu de Duelo com 1P VS 2P acesa ao conectar, queda do oponente e mudo na transição
- `751d478b` 2026-10-26 Regulus CiberSec, +52 -1: BT2 online: cobertura sobre o menu principal na troca para o Duelo e regras da partida
- `aa4e918e` 2026-10-26 Regulus CiberSec, +22 -2: BT2 online: o que o BT3 faz depois de conectar, e a chave de teste dos modos
- `cf0af222` 2026-10-26 Regulus CiberSec, +10 -4: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `6340a148` 2026-10-25 Regulus CiberSec, +19 -1: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2

## `bt2/plugin/online/opening_music.cpp` ([bt2](bt2.md))

- `d9618feb` 2026-10-31 dfdevx2, +18 -6: BT2: only the ISO: the engine in the APK, the image read where it is

## `bt2/plugin/online/relay.cpp` ([bt2](bt2.md))

- `1e7ea3d8` 2026-10-31 dfdevx2, +2 -1: BT2: the relay bounds the user name; the default build makes the online library too
- `6340a148` 2026-10-25 Regulus CiberSec, +343 -0: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2

## `bt2/plugin/online/relay.h` ([bt2](bt2.md))

- `6340a148` 2026-10-25 Regulus CiberSec, +68 -0: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2

## `bt2/plugin/online/session.cpp` ([bt2](bt2.md))

- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +94 -0: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade

## `bt2/plugin/online/session.h` ([bt2](bt2.md))

- `2e034b59` 2026-10-31 dfdevx2, +1 -1: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +50 -0: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade

## `bt2/plugin/online/stun.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +34 -34: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `d4467759` 2026-10-26 Regulus CiberSec, +7 -4: BT2 online: pareamento entre redes (STUN esperado, endereços em ordem, falha de verdade) e registro da rede no log
- `cf0af222` 2026-10-26 Regulus CiberSec, +135 -0: BT2 online: STUN para jogar entre redes; preset para GPUs fracas

## `bt2/plugin/online/stun.h` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +6 -6: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `cf0af222` 2026-10-26 Regulus CiberSec, +43 -0: BT2 online: STUN para jogar entre redes; preset para GPUs fracas

## `bt2/plugin/tests/fight_tests.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +132 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC

## `bt2/plugin/tests/lockstep_tests.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +1 -1: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `ad9c36e9` 2026-10-26 Regulus CiberSec, +205 -0: BT2 online: troca de entradas por campo (lockstep) testada no PC, e o que o BT3 faz para manter dois jogos em passo

## `bt2/plugin/tests/session_tests.cpp` ([bt2](bt2.md))

- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +136 -0: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade

## `bt2/plugin/tests/stack_tests.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +1 -1: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `b2f2dbb5` 2026-10-27 Regulus CiberSec, +127 -0: BT2 online: sessão (configuração e encontro) e mensagens na linha UDP, testadas em sockets de verdade

## `bt2/runtime/android/android_main.cpp` ([bt2](bt2.md))

- `128c1003` 2026-10-31 dfdevx2, +10 -2: BT2: the build needs the kernel code; an error is said, not a black screen; output scale fills the screen
- `d9618feb` 2026-10-31 dfdevx2, +18 -7: BT2: only the ISO: the engine in the APK, the image read where it is
- `424283db` 2026-10-27 Regulus CiberSec, +12 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `473b81df` 2026-10-25 Regulus CiberSec, +21 -3: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `cab206e6` 2026-10-25 Regulus CiberSec, +6 -3: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)
- `3ff439ea` 2026-10-25 Regulus CiberSec, +2 -0: BT2 no Android: item "Online Mode" no menu principal do jogo, com tela e música
- `90e4838b` 2026-10-25 Regulus CiberSec, +12 -1: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +44 -4: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +150 -1: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `048cbd8d` 2026-10-24 Regulus CiberSec, +9 -0: BT2 no Android: geração no Linux, kernel, desempenho e opções da GPU
- `0f729094` 2026-10-24 Regulus CiberSec, +200 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/android/engine_abi.h` ([bt2](bt2.md))

- `bcd7fa70` 2026-10-25 Regulus CiberSec, +7 -0: BT2 no Android: motor importado pelo app, fora do GitHub

## `bt2/runtime/ee/dma_chain.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/dma_chain.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_execution_owner.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_gs_privileged.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +50 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_gs_readback.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +92 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_interpreter.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +174 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_interpreter_handlers.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +17 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_runtime.h` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +5 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `90e4838b` 2026-10-25 Regulus CiberSec, +5 -3: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `0f729094` 2026-10-24 Regulus CiberSec, +978 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_sound_dispatch.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +201 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ee_thread_scheduler.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +289 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/frame_profile.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +17 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/game_patches.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/game_patches.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/guest_interface.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +39 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/intc_checkpoint.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +34 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_block.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +208 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_block.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +34 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_csc.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +26 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_csc.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +12 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_device.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +214 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_device.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +59 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_input.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +82 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_input.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +44 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_vlc.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +71 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ipu_vlc.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +19 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/logical_timer1.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +62 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/logical_timer2.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +45 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/overlay_image.h` ([bt2](bt2.md))

- `fe2770e0` 2026-10-26 Regulus CiberSec, +56 -0: BT2: o código do overlay (DBZP.BIN) vem da ISO do jogador, não de dentro do motor

## `bt2/runtime/ee/post_reset_sif.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +107 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_fpu.cpp` ([bt2](bt2.md))

- `dfe15ac2` 2026-10-24 Regulus CiberSec, +46 -8: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_fpu.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +13 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_runtime.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +91 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_runtime_dispatch.cpp` ([bt2](bt2.md))

- `dfe15ac2` 2026-10-24 Regulus CiberSec, +30 -3: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +145 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_runtime_macros.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +213 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_runtime_memory.cpp` ([bt2](bt2.md))

- `bd17534e` 2026-10-27 Regulus CiberSec, +4 -1: BT2: menos trabalho repetido por pacote e por lista (fila do renderizador, lista do VIF1)
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +25 -6: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +488 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_runtime_services.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +375 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_runtime_syscalls.cpp` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +9 -1: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +528 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_stubs.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/ps2_syscalls.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/retained_ee_stack.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +260 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/retained_ee_stack.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +20 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/state_hash.cpp` ([bt2](bt2.md))

- `473b81df` 2026-10-25 Regulus CiberSec, +1 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `0f729094` 2026-10-24 Regulus CiberSec, +62 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/state_hash.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +34 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/stopped_dma_channels.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +49 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/ee/vu0_macro.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +106 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/game/game_main.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +160 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/game/startup_inputs.cpp` ([bt2](bt2.md))

- `d9618feb` 2026-10-31 dfdevx2, +2 -3: BT2: only the ISO: the engine in the APK, the image read where it is
- `0f729094` 2026-10-24 Regulus CiberSec, +172 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/game/startup_inputs.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +36 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/cpu/gs_command_sink.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +225 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/cpu/gs_command_sink.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +124 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gif_dma_staging.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +130 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/gpu_gs_backend.cpp` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +113 -7: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +341 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/gpu_gs_backend.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +7 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +81 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/gpu_gs_mirror.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/gpu_gs_mirror.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +1 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +36 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/gpu_window_presenter.cpp` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +15 -2: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +225 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/gpu_window_presenter.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +5 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/hd_texture_image.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +6 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/hd_texture_pack.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +133 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/hd_texture_pack.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +57 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/in_between_frames.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +143 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/gpu/in_between_frames.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +92 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/hw/bt3/gl_translate.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +104 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/gs_core.c` ([bt2](bt2.md))

- `dfe15ac2` 2026-10-24 Regulus CiberSec, +154 -11: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +1723 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/gs_draw.c` ([bt2](bt2.md))

- `cf0af222` 2026-10-26 Regulus CiberSec, +32 -0: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `fa6bb69c` 2026-10-25 Regulus CiberSec, +15 -5: BT2 no Android: registro dos passes de GPU e menos leituras de ambiente por desenho
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +108 -2: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +4605 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/gs_draw.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +170 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/gs_gl.c` ([bt2](bt2.md))

- `90e4838b` 2026-10-25 Regulus CiberSec, +90 -3: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `5a417d3f` 2026-10-24 Regulus CiberSec, +1417 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/gs_gpu.c` ([bt2](bt2.md))

- `cf0af222` 2026-10-26 Regulus CiberSec, +3 -0: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `fa6bb69c` 2026-10-25 Regulus CiberSec, +34 -0: BT2 no Android: registro dos passes de GPU e menos leituras de ambiente por desenho
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +88 -1: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +1933 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/gs_internal.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +95 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/gs_texpack.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +30 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/menu_labels.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +47 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/menu_text.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +82 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/menu_translation.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +47 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/port_addr.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +31 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/alphakey.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +18 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/dclut.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +33 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/ffx/LICENSE-FidelityFX-FSR.txt` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +19 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/ffx/LICENSE-Snapdragon-GSR.txt` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +29 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/ffx/ffx_a.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +2656 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/ffx/ffx_fsr1.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +1199 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/fsr_easu.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +25 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/fsr_rcas.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +23 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/fx.vert` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +8 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/gs.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +160 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/gs.vert` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +13 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/outline.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +31 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/present.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +56 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/sgsr.frag` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +122 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/vu0.vert` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +62 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/vu4.vert` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +32 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/shaders/vu6.vert` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +35 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/tbo_art.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +52 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3/ui.h` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +112 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/bt3_host.c` ([bt2](bt2.md))

- `3ff439ea` 2026-10-25 Regulus CiberSec, +41 -4: BT2 no Android: item "Online Mode" no menu principal do jogo, com tela e música
- `5a417d3f` 2026-10-24 Regulus CiberSec, +293 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/hw_gs_mirror.cpp` ([bt2](bt2.md))

- `bd17534e` 2026-10-27 Regulus CiberSec, +17 -7: BT2: menos trabalho repetido por pacote e por lista (fila do renderizador, lista do VIF1)
- `3ff439ea` 2026-10-25 Regulus CiberSec, +2 -0: BT2 no Android: item "Online Mode" no menu principal do jogo, com tela e música
- `90e4838b` 2026-10-25 Regulus CiberSec, +78 -6: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +10 -1: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +56 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/hw/hw_gs_mirror.h` ([bt2](bt2.md))

- `90e4838b` 2026-10-25 Regulus CiberSec, +6 -1: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +5 -0: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +31 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/runtime/gs/texture_gif_startup.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +52 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/textures/gs_texture_names.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +448 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/textures/gs_texture_names.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +124 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/gs/vif_startup.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/audio_device.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +83 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/display_frame.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/dualsense_report.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/frame_pacer.h` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +10 -4: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +11 -1: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +14 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `048cbd8d` 2026-10-24 Regulus CiberSec, +11 -2: BT2 no Android: geração no Linux, kernel, desempenho e opções da GPU
- `0f729094` 2026-10-24 Regulus CiberSec, +118 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/game_folders.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +116 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/gamepad.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/host_input.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +128 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/input_mapping.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +40 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/launcher.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +155 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/logo_image.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +52 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/menu_host.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +207 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/menu_input.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +72 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/menu_model.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +95 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/menu_renderer.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +482 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/overlay.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +37 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/player_session.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +106 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/sample_profiler.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +101 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/sdl/android_performance.h` ([bt2](bt2.md))

- `048cbd8d` 2026-10-24 Regulus CiberSec, +77 -0: BT2 no Android: geração no Linux, kernel, desempenho e opções da GPU

## `bt2/runtime/host/sdl/sdl_audio_device.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +50 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/sdl/sdl_host_input.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +84 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/sdl/sdl_player_session.h` ([bt2](bt2.md))

- `fda328fb` 2026-10-25 Regulus CiberSec, +6 -0: BT2 no Android: controle só dentro do jogo, como um de PS2
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +5 -1: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +16 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +90 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/sdl/sdl_viewer_window.h` ([bt2](bt2.md))

- `90e4838b` 2026-10-25 Regulus CiberSec, +13 -1: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `5a417d3f` 2026-10-24 Regulus CiberSec, +6 -1: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +149 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/sdl/touch_pad.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +39 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/settings.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +124 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/utf8.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +23 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/viewer_window.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +227 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/wav_writer.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/host/window_presenter.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +20 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/main.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +7 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/LICENSE.sse2neon` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/emmintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/immintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/nmmintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/pmmintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/smmintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/sse2neon.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +11222 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/sse_on_neon.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/tmmintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/wmmintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/x86intrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/platform/sse/xmmintrin.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/plugin/plugin_host.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +24 -0: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `3c19f7d9` 2026-10-26 Regulus CiberSec, +1 -1: BT2 online: título que flutua, Gohan em vídeo em loop, 16 ganchos e retorno ao Modo Online
- `473b81df` 2026-10-25 Regulus CiberSec, +135 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)

## `bt2/runtime/plugin/plugin_host.h` ([bt2](bt2.md))

- `473b81df` 2026-10-25 Regulus CiberSec, +15 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)

## `bt2/runtime/probe/checkpoint_checks.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +594 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/entry_probe.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +2 -2: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `473b81df` 2026-10-25 Regulus CiberSec, +42 -4: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)
- `3ff439ea` 2026-10-25 Regulus CiberSec, +8 -0: BT2 no Android: item "Online Mode" no menu principal do jogo, com tela e música
- `90e4838b` 2026-10-25 Regulus CiberSec, +12 -4: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +29 -6: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `5a417d3f` 2026-10-24 Regulus CiberSec, +44 -6: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +791 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/game_starter.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +644 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/game_starter.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +49 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/generated_probe.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +61 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/interpreter_handlers_none.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +6 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/overlay_bindings.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +12 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/probe_inputs.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +104 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/probe_reports.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +440 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/probe_state.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +61 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/rpc_bind_probe.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +61 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/selected_bindings.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +12 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/probe/startup_bindings.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +110 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/audio_mixer.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +81 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/card_folder.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +298 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/cri_audio.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +154 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/cri_service.cpp` ([bt2](bt2.md))

- `3ff439ea` 2026-10-25 Regulus CiberSec, +8 -1: BT2 no Android: item "Online Mode" no menu principal do jogo, com tela e música
- `0f729094` 2026-10-24 Regulus CiberSec, +100 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/cri_stream.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +57 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/devices/disc_image.h` ([bt2](bt2.md))

- `d9618feb` 2026-10-31 dfdevx2, +50 -0: BT2: only the ISO: the engine in the APK, the image read where it is

## `bt2/runtime/services/devices/disc_metadata.h` ([bt2](bt2.md))

- `d9618feb` 2026-10-31 dfdevx2, +15 -3: BT2: only the ISO: the engine in the APK, the image read where it is
- `90e4838b` 2026-10-25 Regulus CiberSec, +12 -0: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `0f729094` 2026-10-24 Regulus CiberSec, +67 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/devices/disc_sectors.h` ([bt2](bt2.md))

- `d9618feb` 2026-10-31 dfdevx2, +9 -0: BT2: only the ISO: the engine in the APK, the image read where it is
- `90e4838b` 2026-10-25 Regulus CiberSec, +27 -5: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/devices/dualshock2.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +86 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/devices/pad_input.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +11 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/devices/pad_script.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +55 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/disc_service.cpp` ([bt2](bt2.md))

- `90e4838b` 2026-10-25 Regulus CiberSec, +1 -1: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `0f729094` 2026-10-24 Regulus CiberSec, +75 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/fileio_service.cpp` ([bt2](bt2.md))

- `90e4838b` 2026-10-25 Regulus CiberSec, +5 -3: BT2 no Android: renderizador em thread própria, leitura do disco e Mali
- `0f729094` 2026-10-24 Regulus CiberSec, +96 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/iop_heap.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +37 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/iop_memory_service.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +57 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/memory_card_info.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/memory_card_service.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +132 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/module_service.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/pad_block.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +25 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/pad_service.cpp` ([bt2](bt2.md))

- `424283db` 2026-10-27 Regulus CiberSec, +29 -6: BT2 online: a luta (segundo controle, partida em motor novo, encontro na seleção, lockstep por campo), compilada e testada no PC
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +6 -3: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +105 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/sif_rpc.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +127 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/sif_rpc.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/sound_bank.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +92 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/sound_sampler.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +134 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/sound_service.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +133 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/services/spu_adpcm.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +36 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/bootstrap_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +2008 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/buffer_fill_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/card_folder_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +129 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/compare_bytes_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +44 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/copy_handler_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/disc_metadata_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +31 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/disc_sector_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/dma_chain_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +44 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/game_patches_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +36 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/gpu_gs_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +93 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/gs_command_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +240 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/host_input_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +78 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/ipu_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +197 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/kernel_handler_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +65 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/menu_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +227 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/native_services_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +319 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/post_reset_sif_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +48 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/retained_ee_stack_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/startup_inputs_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +93 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/texture_names_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +159 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/timer_write_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/vif1_blend_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +65 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/tests/vu1_tests.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +381 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vif1.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +106 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vif1.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +40 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vif1_blend.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +163 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vif1_blend.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +42 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1.cpp` ([bt2](bt2.md))

- `46357425` 2026-10-25 Regulus CiberSec, +32 -124: BT2 no Android: instruções do VU1 inline e NEON direto
- `dfe15ac2` 2026-10-24 Regulus CiberSec, +38 -1: BT2 no Android: personagens com cores, painel de desempenho e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +446 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1.h` ([bt2](bt2.md))

- `46357425` 2026-10-25 Regulus CiberSec, +7 -1: BT2 no Android: instruções do VU1 inline e NEON direto
- `0f729094` 2026-10-24 Regulus CiberSec, +116 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_handwritten_helpers.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +84 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_handwritten_registry.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +26 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_lower.h` ([bt2](bt2.md))

- `46357425` 2026-10-25 Regulus CiberSec, +120 -0: BT2 no Android: instruções do VU1 inline e NEON direto

## `bt2/runtime/vu/vu1_native_none.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +5 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_program.h` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_simd.h` ([bt2](bt2.md))

- `46357425` 2026-10-25 Regulus CiberSec, +20 -2: BT2 no Android: instruções do VU1 inline e NEON direto
- `0f729094` 2026-10-24 Regulus CiberSec, +40 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_skinned_vertex_programs.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +257 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_triangle_clipper.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +203 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/runtime/vu/vu1_triangle_strip_program.cpp` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +118 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/audio/compare_audio.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +97 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/audio/test_compare_audio.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/build/msvc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +37 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/build/test_msvc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +18 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_card_alarm.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_card_fifo.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +55 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_cri_low_read.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +76 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_cri_thread.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +98 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_dbc_dma.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_disc_api.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +54 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_disc_dma.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_disc_type.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +37 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_ee.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +1271 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_ee_reverse_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +48 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_iop_delay.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_iop_ee_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +46 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_pad_poll.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_sio2_dma.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +47 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_sio2_flow.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +98 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_sio2_start.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +52 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_spu2_initialized.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +62 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_spu2_interrupts.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_spu2_key_readback.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +66 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_spu2_status.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +62 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/capture_spu2_transfer.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +77 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/read_cri_stream_log.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +23 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/read_sounds_log.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +22 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_capture_cri_thread.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_capture_ee.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +469 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_capture_pad_poll.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +24 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_capture_spu2_status.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +17 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_capture_spu2_transfer.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +27 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_dbzp_load.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_disc_command.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +54 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_disc_startup.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +19 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_display_reset.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_frame_gs_reset.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +13 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_mc_sio2_flow.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +18 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_mc_timed.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +15 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_overlay_dispenv.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +28 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_queued_sound_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +34 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_sound_bank.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +25 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_startup_workers.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +50 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_timer2.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +28 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_cd_init.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +47 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_gif.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +95 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_gs.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +70 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_loadfile.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +42 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_post_cd.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +102 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_post_sif.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_rpc_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +82 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_scmd_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/test_verify_sif_reset.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +127 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_cd_init.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +102 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_cri_autodma.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +81 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_cri_preemption.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +59 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_cri_rpc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +53 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_cri_second_channel.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +54 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_dbc_dma.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_dbc_rpc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +55 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_dbzp_load.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +31 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_disc_command.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +62 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_disc_read.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_disc_startup.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +71 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_display_reset.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_ee_cache.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_ee_main_status.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +66 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_ee_sound_dispatch.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +38 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_ee_thread.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +45 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_frame_gs_reset.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +45 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_frame_vsync.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_gif.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +95 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_gs.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +54 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_heap_allocation.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_heap_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +42 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_heap_copy.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +36 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_loadfile.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +30 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_mc_frame_query.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +36 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_mc_rpc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_mc_sio2.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_mc_sio2_flow.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_mc_timed.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +40 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_module_buffer.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +44 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_overlay_dispenv.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +45 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_post_cd.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +54 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_post_sif.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +37 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_rpc_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +119 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_scmd_bind.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +86 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_second_heap.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +24 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_sif_reset.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +106 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_sound_bank.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_sound_rpc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +92 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_spu2_initialization.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +44 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_spu2_transfer.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_startup_workers.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +67 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_texture_startup.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +32 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_timer2.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +39 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_vblank_wake.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +59 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/capture_ee/verify_vif_startup.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +27 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/dev_server/peer.py` ([bt2](bt2.md))

- `cf0af222` 2026-10-26 Regulus CiberSec, +40 -2: BT2 online: STUN para jogar entre redes; preset para GPUs fracas
- `6340a148` 2026-10-25 Regulus CiberSec, +4 -2: BT2 online: relay TURN na biblioteca, serviço próprio na Cloudflare e endereço padrão do BT2
- `473b81df` 2026-10-25 Regulus CiberSec, +145 -0: BT2 no Android: menu e online numa biblioteca do APK, e a conexão direta (em andamento)

## `bt2/tools/dev_server/serve.mjs` ([bt2](bt2.md))

- `cab206e6` 2026-10-25 Regulus CiberSec, +52 -0: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)

## `bt2/tools/docs/export_startup_progress.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +66 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/docs/render_status.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +97 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/docs/sync_progress.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +42 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/gate/battle_run.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +69 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/gate/pad-fight.txt` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +1 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/gate/run_gate.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/gate/test_run_gate.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +29 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/DiscoverBT2Prologues.java` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +53 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/DiscoverBT2Target.java` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +81 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/ExportBT2Candidates.java` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +153 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/analyze_game.py` ([bt2](bt2.md))

- `048cbd8d` 2026-10-24 Regulus CiberSec, +2 -1: BT2 no Android: geração no Linux, kernel, desempenho e opções da GPU
- `0f729094` 2026-10-24 Regulus CiberSec, +95 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/audit_startup.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +77 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/review_entry.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +121 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/review_entry_targets.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +39 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ghidra/test_review_entry.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +51 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/graphics/gpu/fetch_parallel_gs.py` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +13 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações
- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/graphics/gpu/granite_failed_pipelines.patch` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +47 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/tools/graphics/gpu/granite_vulkan11_extensions.patch` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +96 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/tools/graphics/gpu/parallel_gs_android_options.patch` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +123 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/tools/graphics/gpu/parallel_gs_android_timeline_wait.patch` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +25 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/tools/graphics/gpu/parallel_gs_frame_boundaries.patch` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +76 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/graphics/gpu/parallel_gs_no_8bit_storage.patch` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +315 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/tools/graphics/gpu/parallel_gs_texture_replacement.patch` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +133 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/graphics/gpu/parallel_gs_vram_backup.patch` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +63 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/graphics/gpu/rebuild_gs_shaders.py` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +77 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/tools/graphics/hw/make_bt3_shaders.py` ([bt2](bt2.md))

- `5a417d3f` 2026-10-24 Regulus CiberSec, +63 -0: BT2 no Android: renderizador do BT3, menu no jogo e otimizações

## `bt2/tools/graphics/textures/fetch_xxhash.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +17 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/graphics/textures/test_texture_name_check.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/graphics/textures/texture_name_check.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +91 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/inspect_elf/inspect_elf.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +103 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/inspect_elf/test_inspect_elf.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +71 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/iop/inspect_image.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +96 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/iop/inspect_irx.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +159 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/iop/test_inspect_image.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +55 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/iop/test_inspect_irx.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +58 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/launcher/cut_logo.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +50 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/menu_item/make_fighter_video.py` ([bt2](bt2.md))

- `3c19f7d9` 2026-10-26 Regulus CiberSec, +101 -0: BT2 online: título que flutua, Gohan em vídeo em loop, 16 ganchos e retorno ao Modo Online

## `bt2/tools/menu_item/make_menu_item.py` ([bt2](bt2.md))

- `3c19f7d9` 2026-10-26 Regulus CiberSec, +15 -8: BT2 online: título que flutua, Gohan em vídeo em loop, 16 ganchos e retorno ao Modo Online
- `cffcb461` 2026-10-26 Regulus CiberSec, +14 -1: BT2 online: menu de Duelo com 1P VS 2P acesa ao conectar, queda do oponente e mudo na transição
- `cab206e6` 2026-10-25 Regulus CiberSec, +22 -10: BT2 no Android: Online Mode com sala por código e partida rápida (etapa 2)
- `cfac7c3c` 2026-10-25 Regulus CiberSec, +366 -0: BT2 no Android: imagens do item Online Mode vêm dentro do app, desenhadas pelo projeto

## `bt2/tools/perf/profile_lines.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +58 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/perf/profile_report.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +57 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/perf/test_profile_report.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +42 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/battle_sweep.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +134 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/build_tools.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +61 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/compare_state_hash.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/discovery_loop.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +227 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/expand_startup.py` ([bt2](bt2.md))

- `fe2770e0` 2026-10-26 Regulus CiberSec, +21 -8: BT2: o código do overlay (DBZP.BIN) vem da ISO do jogador, não de dentro do motor
- `048cbd8d` 2026-10-24 Regulus CiberSec, +2 -1: BT2 no Android: geração no Linux, kernel, desempenho e opções da GPU
- `0f729094` 2026-10-24 Regulus CiberSec, +316 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/generate_game.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +131 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/generate_interpreter.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +181 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/generate_kernel.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +173 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/inspect_blocker.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +68 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/map_gaps.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +276 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/menu_explorer.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +100 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/overlay_programs.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +113 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/prepare_map.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +694 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/prepare_runtime.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +378 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/probe_kernel.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +39 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/run_pipeline.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +629 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/run_startup_probe.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +48 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/runtime_census.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +347 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/screen_run.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +66 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/smoke_test.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +47 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/story_scenes.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +164 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/story_sweep.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +138 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_battle_sweep.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +33 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_compare_state_hash.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +21 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_discovery_loop.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +43 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_expand_startup.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +50 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_game_inputs.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +18 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_generate_game.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +51 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_generate_interpreter.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +54 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_generate_kernel.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +41 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_inspect_blocker.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_map_entries.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +160 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_menu_explorer.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +34 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_overlay_programs.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +64 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_prepare_map.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +364 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_prepare_runtime.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +252 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_probe_kernel.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +13 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_run_pipeline.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +358 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_run_startup_probe.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +20 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_runtime_census.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +53 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_screen_run.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +23 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_story_scenes.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +65 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/ps2recomp/test_story_sweep.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +32 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/verify_game/test_verify_disc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +46 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/verify_game/verify_disc.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +116 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/verify_game/verify_game.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +78 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/vu1/disassemble.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +91 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/vu1/test_disassemble.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +52 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/vu1/test_translate_programs.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +65 -0: BT2 Recomp para Android: motor libbt2.so

## `bt2/tools/vu1/translate_programs.py` ([bt2](bt2.md))

- `0f729094` 2026-10-24 Regulus CiberSec, +242 -0: BT2 Recomp para Android: motor libbt2.so

## `docs/DRAGON_NET.md`

- `176d77e8` 2026-10-18 dfdevx2, +6 -4: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +119 -0: Dragon Rage 0.8.0: Dragon Net

## `docs/NATIVE_BUILD.md`

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +9 -0: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +22 -4: Add Linux setup and improve native menus and music
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +23 -3: Add disc-based setup and simplify launchers
- `101de958` 2026-10-24 Tag Team Mod contributors, +78 -0: Add native Tag Team modes

## `docs/README.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -4: Fix giant grabs, ring-outs and fallen fighter recovery
- `0153b616` 2026-10-06 z3xox, +1 -0: Songs added to the music select: manifest, script, and the name overlay
- `28e86145` 2026-10-06 z3xox, +1 -0: Stages from outside the disc: manifest, install script, styled menu names

## `docs/agent_rules.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -72: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/briefs_action_handlers.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -78: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/briefs_cleanup.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -43: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/briefs_effects_stage.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -127: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/briefs_menu_overlay.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -117: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/briefs_remaining_main.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -46: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/changes/README.md`

- `96895d21` 2026-10-31 dfdevx2, +49 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/audio.md`

- `c45067ba` 2026-10-31 dfdevx2, +26 -0: Change map: the history from 0.9.0 back, by area
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/bt2.md`

- `2e034b59` 2026-10-31 dfdevx2, +7 -0: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `3ea03f80` 2026-10-31 dfdevx2, +1 -1: BT2 online: its screens in the three languages
- `128c1003` 2026-10-31 dfdevx2, +14 -0: BT2: the build needs the kernel code; an error is said, not a black screen; output scale fills the screen
- `c45067ba` 2026-10-31 dfdevx2, +4 -0: Change map: the history from 0.9.0 back, by area
- `d9618feb` 2026-10-31 dfdevx2, +7 -0: BT2: only the ISO: the engine in the APK, the image read where it is
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/bt3-engine.md`

- `8005f0bd` 2026-10-31 dfdevx2, +1 -1: Changelogs: 1.0, what changes for players
- `8cbd9d5a` 2026-10-31 dfdevx2, +14 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `c45067ba` 2026-10-31 dfdevx2, +90 -0: Change map: the history from 0.9.0 back, by area
- `caeeffd0` 2026-10-31 dfdevx2, +7 -0: Low memory: a negative size from the disc says modified data, not out of memory
- `ac51a1ad` 2026-10-31 dfdevx2, +7 -0: Game files: say why a read failed, read again through a new file object; keep the old model
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/bt4-armsx2.md`

- `4a72a23c` 2026-10-31 dfdevx2, +6 -0: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `c45067ba` 2026-10-31 dfdevx2, +61 -0: Change map: the history from 0.9.0 back, by area
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/bt4.md`

- `c45067ba` 2026-10-31 dfdevx2, +90 -0: Change map: the history from 0.9.0 back, by area
- `a974f119` 2026-10-31 dfdevx2, +9 -0: BT4: shim symbols for 0.1.39; tag link: buffers really out of .data
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/build-ci.md`

- `fafbb446` 2026-10-31 dfdevx2, +7 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report
- `b38202f4` 2026-10-31 dfdevx2, +7 -0: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `8005f0bd` 2026-10-31 dfdevx2, +7 -0: Changelogs: 1.0, what changes for players
- `3441216c` 2026-10-31 dfdevx2, +8 -1: BT4: the ARMSX2 library again opens on every build; the build refuses a library the APK does not carry
- `1e7ea3d8` 2026-10-31 dfdevx2, +7 -0: BT2: the relay bounds the user name; the default build makes the online library too
- `8cbd9d5a` 2026-10-31 dfdevx2, +7 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `c45067ba` 2026-10-31 dfdevx2, +54 -0: Change map: the history from 0.9.0 back, by area
- `3ce20c60` 2026-10-31 dfdevx2, +7 -0: Lab tools: the pixel compare; logscan counts a crash once
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/launcher.md`

- `b38202f4` 2026-10-31 dfdevx2, +7 -0: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts
- `8cbd9d5a` 2026-10-31 dfdevx2, +7 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `c45067ba` 2026-10-31 dfdevx2, +90 -0: Change map: the history from 0.9.0 back, by area
- `344aa16c` 2026-10-31 dfdevx2, +7 -0: Tag Team as a setting of the app; the team and the licences in the app
- `297c8a00` 2026-10-31 dfdevx2, +7 -0: Launcher: controllers navigate it again
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/merges-pc.md`

- `3ea03f80` 2026-10-31 dfdevx2, +14 -1: BT2 online: its screens in the three languages
- `c45067ba` 2026-10-31 dfdevx2, +78 -0: Change map: the history from 0.9.0 back, by area
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/online.md`

- `2e034b59` 2026-10-31 dfdevx2, +7 -0: BT2 online: the match opens and walks itself to the character select; the card says where it is
- `3ea03f80` 2026-10-31 dfdevx2, +7 -0: BT2 online: its screens in the three languages
- `1e7ea3d8` 2026-10-31 dfdevx2, +7 -0: BT2: the relay bounds the user name; the default build makes the online library too
- `c45067ba` 2026-10-31 dfdevx2, +75 -0: Change map: the history from 0.9.0 back, by area
- `f0bd6511` 2026-10-31 dfdevx2, +7 -0: Requests: 2 s polls, backoff on 429, none in the background; the relay socket checks the host
- `fe6cf04e` 2026-10-31 dfdevx2, +7 -0: Online: 429 match, relay during a match, lobby timeout, banner version, getenv
- `96895d21` 2026-10-31 dfdevx2, +14 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/regression-tests.md`

- `fafbb446` 2026-10-31 dfdevx2, +4 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report
- `3ea03f80` 2026-10-31 dfdevx2, +22 -0: BT2 online: its screens in the three languages
- `8cbd9d5a` 2026-10-31 dfdevx2, +50 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `docs/changes/renderer-gs.md`

- `91be0b33` 2026-10-31 dfdevx2, +7 -0: BT4: the frame buffer being drawn is the picture, not a work buffer: no noise over the far scenery
- `fea06c41` 2026-10-31 dfdevx2, +6 -0: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump
- `c45067ba` 2026-10-31 dfdevx2, +118 -0: Change map: the history from 0.9.0 back, by area
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/tag-team.md`

- `00244a0d` 2026-10-31 dfdevx2, +7 -0: Tag Team online: the costume table is read through the file cache, as the Android install keeps it in the archive
- `fea06c41` 2026-10-31 dfdevx2, +14 -0: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump
- `8cbd9d5a` 2026-10-31 dfdevx2, +28 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `c45067ba` 2026-10-31 dfdevx2, +4 -0: Change map: the history from 0.9.0 back, by area
- `344aa16c` 2026-10-31 dfdevx2, +7 -0: Tag Team as a setting of the app; the team and the licences in the app
- `c98c4c6b` 2026-10-31 dfdevx2, +7 -0: Clash prompts: not in a match of the normal game
- `a974f119` 2026-10-31 dfdevx2, +8 -5: BT4: shim symbols for 0.1.39; tag link: buffers really out of .data
- `96895d21` 2026-10-31 dfdevx2, +12 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/changes/touch-input.md`

- `c45067ba` 2026-10-31 dfdevx2, +61 -0: Change map: the history from 0.9.0 back, by area
- `d4842ec9` 2026-10-31 dfdevx2, +7 -0: Pad mappings: read them where they are written on Android
- `96895d21` 2026-10-31 dfdevx2, +4 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `docs/decomp_guide.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +1 -1: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/device-lab-2026-10.md`

- `2649a870` 2026-10-25 dfdevx2, +79 -33: Docs: device lab, the Cube's results
- `8b921046` 2026-10-25 dfdevx2, +265 -0: Docs: device lab report, October 2026

## `docs/images/1.0/perf_draw_en.svg`

- `fafbb446` 2026-10-31 dfdevx2, +7 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `docs/images/1.0/perf_draw_es.svg`

- `fafbb446` 2026-10-31 dfdevx2, +7 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `docs/images/1.0/perf_draw_pt.svg`

- `fafbb446` 2026-10-31 dfdevx2, +7 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `docs/images/1.0/perf_speed_en.svg`

- `fafbb446` 2026-10-31 dfdevx2, +7 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `docs/images/1.0/perf_speed_es.svg`

- `fafbb446` 2026-10-31 dfdevx2, +7 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `docs/images/1.0/perf_speed_pt.svg`

- `fafbb446` 2026-10-31 dfdevx2, +7 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `docs/known_bugs.md`

- `928b2413` 2026-10-11 dfdevx2, +68 -0: WIP: patch 1 and touch artwork
- `6e36f906` 2026-10-10 z3xox, +2 -1: known_bugs: the cross-fade seen by the user under both back ends
- `8ed3d38a` 2026-10-10 z3xox, +6 -0: The cross-fade keeps the old shot at the resolution it was drawn in (a copy on the card; read back and uploaded it was the PS2's 512 x 448, blocky)
- `60a8a3ed` 2026-10-10 z3xox, +4 -1: The cross-fade's picture keeps the full width in a wide picture (its strips were narrowed as 2D art)
- `10b86ac0` 2026-10-10 z3xox, +7 -2: The cross-fade in the GPU back ends: the frame the game read back is drawn from the depth page, where every draw was dropped
- `021fc2fc` 2026-10-10 z3xox, +20 -0: The cross-fade between the shots of a fight's intro: the game's read-back of the frame is answered (software renderer and both GPU back ends)
- `6a7b9557` 2026-10-10 z3xox, +1 -1: known_bugs: the replay list fix confirmed by the user
- `322b51eb` 2026-10-10 z3xox, +12 -0: The replay list no longer stops the game: pow, the game's one double maths call, goes through the software-float bridge
- `066ebd60` 2026-10-09 z3xox, +3 -2: known_bugs: the tournament crash fix confirmed by the player who reported it; 0.1.17
- `bf793603` 2026-10-09 z3xox, +20 -0: A tournament (and training, Sim Dragon) match could crash in a 64-bit build: BattleSetup_SetSide's eighth argument, a pointer, was declared s32 in three menu headers, so the caller wrote 32 bits of it on the stack

## `docs/lite-notes.md`

- `2d6d0531` 2026-10-25 dfdevx2, +11 -8: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +66 -0: Lite: measurements on a simulated Mali-G52 MC1 (WIP)

## `docs/logs-1.0.md`

- `8cbd9d5a` 2026-10-31 dfdevx2, +186 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `docs/netplay_notes.md`

- `928b2413` 2026-10-11 dfdevx2, +84 -0: WIP: patch 1 and touch artwork
- `036c74d1` 2026-10-09 z3xox, +26 -1: Relay: channels (ChannelBind, the match's packets as channel data), '(relay)' beside the ping on the meter, packet counts in BT3_RELAY_LOG; notes: two networks played by the user, and the crawl of a relayed match between two copies behind one router, measured and not explained
- `3a361aa6` 2026-10-09 z3xox, +59 -0: Online play: a relay for room codes. The host gets an address at Cloudflare's TURN service and posts it with its others; the one who joins tries it when the direct ways stay silent
- `0e49f1b0` 2026-10-07 z3xox, +21 -0: Input delay chosen from the line's ping when a match starts (automatic by default); the rollback limit starts at 8
- `71824065` 2026-10-07 z3xox, +22 -0: A meter (frame rate, game speed; ping, rollbacks, waits in a match); ping in the input packets; a version in the greeting
- `3c5691ab` 2026-10-07 z3xox, +15 -0: docs: the memory card checked on this branch against the main line
- `2f14974f` 2026-10-07 z3xox, +20 -0: Net window: rollback and input delay (host's choices sent to the joiner); goodbye packet, 6 s time-out, a line over the picture when a match ends
- `3adaa24a` 2026-10-07 z3xox, +7 -0: docs: rollback tried on a real Windows by hand
- `bbf72ff9` 2026-10-07 z3xox, +13 -0: Rollback: the copy that runs ahead slows down until the two agree (ahead value in the input packets)
- `28e4dc06` 2026-10-07 z3xox, +1 -1: docs: the rollback counts of the two-window run
- `ff06e5a4` 2026-10-07 z3xox, +28 -0: Rollback netcode (BT3_NET_ROLLBACK=<n>): guess the other player's input, go back and re-run when it was wrong; BT3_NET_LATENCY for testing
- `59e6bec5` 2026-10-07 z3xox, +17 -0: Re-run frames: vertex programs not run, vertices not collected (uploads and texture-memory sprites kept); 1.27 -> 1.13 ms
- `9d7f0cd7` 2026-10-07 z3xox, +25 -0: Roll ring: a state save per blank that keeps only the pages written (write faults on Linux, write watch on Windows); saving 0.74 -> 0.08 ms
- `99dabe03` 2026-10-07 z3xox, +6 -0: undefined.py: BT3_PORT_CFLAGS for profiling builds; docs: where the arithmetic comes from now
- `bb212093` 2026-10-07 z3xox, +31 -0: Add and multiply by the processor where that is exact (plat_fastvec.c): single operations and the matrix-times-vector kernel, checked bit for bit against the soft-float code; a re-run frame 30% cheaper
- `1f10d21b` 2026-10-07 z3xox, +22 -0: docs: what a re-run frame costs; the two experiments (skip draw lists: -25%; quick exact floats in soft-float files: slower)
- `1cbfa922` 2026-10-07 z3xox, +6 -0: docs: Linux against a real Windows, by hand
- `401fb60d` 2026-10-07 z3xox, +15 -0: Release package carries the list of the game's variables; without it sessions restart the program as before
- `fbf59c17` 2026-10-07 z3xox, +11 -0: docs: Linux against the Windows program (Wine): fight values in step for the whole session
- `b16d39a5` 2026-10-07 z3xox, +3 -2: docs: the in-program session checked by hand
- `3fbf12d1` 2026-10-07 z3xox, +40 -0: Online session inside the running program: the state at the first blank is kept, a session puts the game back to it and leaving brings the player's own state back (no restart); GS memory kept with it; sound made to fit the state
- `7e5ec7d6` 2026-10-07 z3xox, +4 -2: docs: the Windows rewind test on the session, run from the right folder
- `dfd64826` 2026-10-07 z3xox, +19 -0: State save / restore on Windows (builtin setjmp, the port's stack switch); one region allocator on both systems; the rewind test passes on the Windows program under Wine
- `0df4b247` 2026-10-07 z3xox, +3 -0: docs: the session flow tried by hand
- `de855229` 2026-10-07 z3xox, +21 -0: Online session flow: lobby (Host / Join), restart into a synchronised session that opens on the versus character select with everything unlocked, leaving back to the normal game
- ... e mais 12 (`git log -- docs/netplay_notes.md`)

## `docs/open_questions.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -197: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/port/README.md`

- `d4e61322` 2026-10-28 z3xox, +83 -7: Docs: 0.1.39, lobbies replace rooms. docs/port/README.md, a chapter: the list without Create Room, New Lobby, Join without Watch, by address as a lobby, Dragon Net Battle on lobbies, NET_VERSION 6 and how a 0.1.38 game is told apart both ways, FP_PEER_REQUIRED, the update's version compare, MIN_RELEASE 139 after publishing, and what was tested (a lobby by code with two matches and a spectator, by address, a 4-entrant tournament, quick match, 0.1.38.9 against 0.1.39 by address both ways, by code and in the quick match queue, a member without a fingerprint, the regular picture and speed checks); notes in the earlier chapters where they send players to Create Room or Watch. README.md: online play with lobbies. port/release/README.md: MIN_RELEASE after a release that changes NET_VERSION.
- `a3cf1192` 2026-10-28 z3xox, +71 -4: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `ef26bc43` 2026-10-28 z3xox, +22 -2: Next: the three branches made one. A lobby is made or joined only when no update is being downloaded or waiting to be installed (net_lobby.c online_ok, with the rooms' line), and Port_LobbyStart says so before it holds a start for the fingerprint, so the update's "online off" and the data policy's "wait for your fingerprint" agree for lobbies and tournaments as for rooms. LOBBY_VERSION is 3, a single step from 0.1.38.9's 2 for everything the tournaments add (it had gone to 4 on the tournament branch, never released). Docs: a chapter on how the three meet.
- `ffc63800` 2026-10-27 z3xox, +84 -2: Docs: the automatic update. docs/port/README.md, a chapter "Automatic updates": based on PR #107 by JosephLlacchua (what it brought and what was added on top of it), the version from the tag, the flow (the check, the line over the menus, Update now / Later / What's new, the download while playing, online play held, Restart now with its two steps of the rename trick, the undoing, the cleanup), the files never written, the signature and its key, the test switches and what was tested (and not). port/release/README.md (new): making a release, its two secrets, and the update signing key once (update_key.py generate, the secret UPDATE_SIGNING_KEY, the public key into port/src/update_key.h), with the credit to PR #107. README.md: updates for players; translations.md: the update's lines in the port catalog.
- `277d09eb` 2026-10-28 z3xox, +10 -0: Tournaments: the owner chooses the winner when the two games disagree twice. A draw is still played again, but when the two players' games name different winners for the same match a second time there is no more replay: the owner's bracket page asks it to choose ("The two games disagree on this match: choose the winner", the two names as buttons), the members read that the owner is choosing, no match starts meanwhile, and the owner's choice is final (TOUR_CHOOSE; the match marked as chosen). All the lines are in port_tbo.c for the translators. BT3_TEST_RESULT_LIE makes a member report the other winner and BT3_LOBBY_DO has choose <side>. Tested with two entrants and a lying member: the first disagreement replayed, the second asked the owner, whose choice made the champion.
- `504064a3` 2026-10-27 z3xox, +44 -4: Tournaments: an entrant whose game drops gets back in. Leaving on purpose is still a walkover at once; an entrant whose game falls silent (a lost connection, a crash) keeps its bracket place for 90 s, shown on its box as DISCONNECTED and the time left, with a lobby note, and its next match waits. Joining with the lobby's code within the 90 s puts it back in its place, and the match the drop broke is played again from the start; after 90 s the walkover stands. It is known by a secret rejoin token, not its name: the owner draws 128 bits from the system's random source for each entrant and sends it to that member alone (TOKEN, a new packet type 41), which keeps it in memory and in lobby_rejoin.txt beside its settings file so that a crash or a restart keeps it; its greeting to the lobby of that code carries it. A rejoin gets a new token, so a copied old one is refused; a greeting without a valid token is an ordinary member, a spectator while the tournament is on. The owner forgets a token when its entrant is out, leaves, runs out of time, rejoins, or the tournament ends, and tells the member to forget it; the member also removes its file when the lobby closes, when it leaves, when its lobby does not answer, and at start when the file is over an hour old. The member now keeps the code it asked the service for, since the service's answer to a member carries none. STATE carries each held place's seconds left (616 bytes) and HELLO the token (80 bytes): LOBBY_VERSION 4. Tested offscreen: an entrant killed mid-match and back within 90 s (place restored, match replayed, spectators identical), one back too late, a stranger with its name, a copied old token, the token files removed, a stale file ignored.
- `b84d3e3b` 2026-10-27 z3xox, +13 -6: Tournaments: an Entrants rule decides who plays. In Tournament the battle's row (a tournament's matches are single battles anyway) is Entrants, 2 to 8, the owner's and fixed while a tournament is on, and the places on the left follow it: PLAYERS with the places 1..N, then SPECTATORS with the other 8 - N; Versus keeps 1P, 2P and six spectators. The entrants are whoever sits in the player places when the owner starts the tournament (at least two; empty places are left out, with byes as before), placed with Move; with fewer entrants, those in the places that are no longer the players' are spectators there. Ready is no longer shown or asked for in Tournament, since the places say who plays. The rule travels in STATE (byte 3 of word 96). The user found it unclear who was spectating and who was taking part. Tested: the lobby page with 2, 4 (after 5 and back) and 8 player places, the brackets drawn from them, a two-entrant tournament to its champion.
- `3ea60f57` 2026-10-27 z3xox, +81 -0: Tournaments in lobbies. The lobby's owner switches the lobby between Versus and Tournament (a new first rule) and draws a knockout bracket of the places marked ready (2 to 8 humans, the owner too if it is ready): the order is shuffled from a number the owner draws from the system's random source, with byes shared out between the halves when the entrants are not 2, 4 or 8. The owner's game holds the bracket and sends it with the lobby (STATE, now 608 bytes; LOBBY_VERSION 3, so a 0.1.38.9 game is told the versions differ, both ways). Each match is an ordinary lobby match of single battles: the owner's game puts the two in 1P and 2P and starts it, everyone else watches. Every copy in the match notes how its first battle ended (the battle sequence past the fight, held 0.3 s so that a rolled-back K.O. is not taken; Port_BattleOutcome in headless.c), a player says it with each ALIVE, and the owner, when the two agree on a winner, marks it and ends the match; a draw, two different results, one result alone for 20 s or a match ended any other way is played again. An entrant who leaves loses by walkover. A new page, the bracket (TB_BRACKET in port_tbo.c), shows the two halves meeting at the final with gold lines for winners, the next match glowing, byes greyed and the champion's box, and the owner's Start Next Match / New Tournament; it comes up when a tournament is drawn and after each of its matches, and the lobby's page has a Bracket button. All text is in port_tbo.c for the translators. Tested offscreen through a local Worker: 4 and 5 entrants (byes) to a champion with every spectator identical to its host in every match, a walkover, an old release joining either way; Versus lobbies, quick match and rooms of two unchanged (docs/port/README.md, "Tournaments").
- `79fb9436` 2026-10-27 z3xox, +54 -2: Online: whose data may meet. A quick match (strangers) needs the original disc's data, in every menu: Port_LobbyStartQuick refuses it before the matchmaking service is asked ("Quick match needs the original disc's data, and your game data are not the disc's (what differs). With friends ... you can play when both have the same data."), Dragon Net Battle's Quick match tab says so before the search and greys its button, and Tenkaichi Budokai Online's "Cannot Go Online" is now "No Quick Match", for Quick Match only. With friends (a room code, a lobby, an address, watching) the same data on both sides is enough, modified or not: Tenkaichi Budokai Online's Create Room, Join Room and Lobby no longer refuse modified data. No game connects before its own gameplay fingerprint is known: a start asked for earlier waits (lobby state 6, "Checking your game data..."; a lobby's create and join commands stay queued; net_start holds the picture), and a run that computes none (BT3_GS=none, or online play asked before the data check began) starts it (Port_DataFingerprintNeed; PortFp_Run runs once). A peer that sends no fingerprint: 0.1.37 cannot be told from 0.1.37.1 to 0.1.38.9 by its greetings (NET_VERSION 5, the same capability word; rel 137 at the service), so FP_PEER_REQUIRED decides: 0 for now (MIN_RELEASE is 137), 1 with MIN_RELEASE 138, which refuses such a player, spectator or lobby member. BT3_TEST_FPRINT now acts in plat_fprint.c; BT3_TEST_FPRINT=off sends none (as 0.1.37); BT3_TEST_FP_REQUIRED=1; BT3_UI_NET_TAB opens the online window on a tab. Tested with the local Worker: quick match original against original (BT3_LOBBY and Tenkaichi Budokai Online's screens), a claimed fingerprint refused before the service (both menus show it); by address and room code the same fake fingerprint plays and different ones are refused, after waiting for the fingerprint at the first blank; spectators and lobby members with other data refused, with the same let in; with FP_PEER_REQUIRED a player and a member without a fingerprint refused; 0.1.38.9 by address both ways. Windows builds.
- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +3 -3: Fix giant grabs, ring-outs and fallen fighter recovery
- `6a8d1429` 2026-10-26 z3xox, +18 -5: Quick match: a chosen region is a preference that widens, no longer a lock. A pair is made when each side takes the other, the wait being the longer of the two: Auto takes players within 2,500 km at once and anyone from 25 s; a region takes players placed in it at once and anyone from 40 s (REGION_WIDEN_SECONDS); Anywhere and older games take anyone. So for the first 25 s Auto is nearby only and a region its players only, until 40 s Auto takes everyone but those who chose another region, and after 40 s everyone queuing meets, the nearest first. A region's search is told widen 40; its screens say "Players there first, everyone after 40 s" and, past it, "NO ONE THERE: SEARCHING EVERYWHERE", and the hosts' line says the same. No change to the database. Tested: test.mjs 185 checks (Auto in the Middle East against South America chosen: not at 30 s either way, matched at 40); through the screens with the local Worker, Dubai on Auto and Sao Paulo on South America put together at 40 s.
- `cbde53ab` 2026-10-26 z3xox, +61 -0: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `82070dbe` 2026-10-26 z3xox, +20 -0: Online: the goodbye says why a match ended. The other game's goodbye (T_BYE) carried only 0 or the desync word, so "The other player left the match." could mean its player left or that it had stopped hearing this game (a user's quick match ended so after five fights at 300 ms and the log could not tell). It now says BYE_LEFT (the menus), BYE_QUIT (the program closed: sent at exit, so the other side knows at once instead of after six seconds of silence) or BYE_LOST (it stopped hearing this one), and the screen says which. Older releases show any of these as "left the match", as before. Tested with two copies by address: leaving, a closed program, a one-way failure (new test switch BT3_NET_MUTE_AT), and against the 0.1.38.7 and 0.1.38.8 releases both ways; Windows builds
- `b6315e28` 2026-10-26 z3xox, +8 -5: Lobbies: a member connecting takes the owner's first answer only when it carries the nonce of the member's greeting (STATE carries it now; NO did), from whatever address it comes, instead of from any address of the owner's machine or port; the nonce is from the system's random source (/dev/urandom, rand_s on Windows); a knock no longer adds an address to greet. LOBBY_VERSION 2: a game of 09ba09c is told the versions differ. Tested: an owner behind BT3_TEST_FIREWALL joined ten times out of ten; forged answers (no nonce, a wrong nonce, a NO with a wrong one) from another socket not taken; a lobby match and back, rooms of two, quick matches and spectators by code as before.
- `09ba09c8` 2026-10-26 z3xox, +10 -0: Lobbies: a member connecting takes the owner's answer from any address of the owner's machines or with the port of its socket (an answer from another address of the machine, or through its router with another port, was dropped: a Windows owner and a Linux member on one network never met though the owner had taken the member), greets the owner wherever the owner's knocks come from, and logs where it greets and what it did not take; when nothing answers it says a firewall on the owner's PC may be blocking the game instead of guessing a release. BT3_TEST_FIREWALL=1 (testing): a socket that takes only what comes from where it has sent, as a stateful firewall. Tested: an owner behind it, a member by code, ten times through a local Worker and twice through the live one; rooms of two, quick matches and spectators by code as before.
- `79cc5d6d` 2026-10-26 z3xox, +11 -0: Spectators run closer behind the match: 24 blanks behind the host at first instead of a second, 12 more each time this copy runs out of the host's blanks (at most the second), 2 fewer after each quiet 900 blanks (down to 18); further behind than the lead and 12, one blank in sixteen runs without picture until it is back. Still only the host's confirmed input. The log says the lead every 1,800 blanks. Measured offscreen at 114% with the spectator drawing: its pictures as even as before (99th percentile 30.0 ms, none over 36 ms), 27 blanks behind (0.4 s) with 5% of its packets lost, the host unchanged, fight values identical.
- `6f99aff2` 2026-10-26 z3xox, +19 -2: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `a581c3f7` 2026-10-26 z3xox, +20 -12: A spectator's own pause menu is the battle's own pause menu: its box from the battle's sprite sheet, drawn by its engine (BtlMenu_Draw, src/battle/hud_0_b.c, through BtlMenu_PortDrawMenu), its colours, sounds and six-frame growing box, and the versus pause menu's own "EXIT ?" Yes / No box before leaving (PauseMenu_PortConfirmBox, src/battle/hud_0.c), instead of a Tenkaichi Budokai Online panel. Its items (port/src/spec_menu.c): the note "Your screen only: the fight goes on" in the title's row, Continue, View: Player 1 / Player 2 / Split Screen with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching; their texts reach the engine through BtlMenu_GetItemText (numbers 0x6FFF and on) and are translated by the online menus' file (tbo_strings.py lists the file). The engine's update is not run and the battle's menus are not touched: the file moves the cursor and the box itself and reads the PC's pad (Port_PadRead); in a fight only (gPortBtlMenuLive). Tested by code with an owner and a member watching and every item used with the pad: the fight values of both spectators and both players identical, no check value differed. All game-side changes under PORT: the PS2 build is unchanged.
- `b4a6f9ef` 2026-10-26 z3xox, +82 -23: Lobbies: "The screens" in docs/port/README.md (the page, chat, getting there, the spectator's line and pause menu, text, test switches, what was tested), the Lobbies and Spectators chapters brought up to date (the page instead of the window), docs/port/menus.md (drawing over the game's own screens) and docs/port/translations.md (%s in lines with a name).
- `d9a688c2` 2026-10-26 z3xox, +114 -0: Lobbies: the chapter in docs/port/README.md (the window, a match and who watches whom, back to the lobby and leaving, the packets, the relay and what was not tested against Cloudflare, the test switches, what was tested and the players' pacing) and the Worker's part in port/matchmaking/README.md (the lobby's room, the answers to old games, the requests an hour, and the deploy steps: one ALTER TABLE and the new worker.js).
- `086d9073` 2026-10-26 z3xox, +108 -0: Spectators: the chapter in docs/port/README.md (how, the packets, limits, the test switches, what was tested) and the Worker's part in port/matchmaking/README.md (what /v1/watch and the poll's new fields do, the table watchers, what to deploy). BT3_TEST_SPEC_APART=<blank>: a spectator claims a wrong check value once, for testing that it follows the match again from the start.
- `088ec8c7` 2026-10-21 z3xox, +8 -0: Songs: the .adx an earlier release converted from an MP3 (the converter's mark) is not listed while that MP3 is there, which is now played itself (a player who had added MP3s saw each one twice); a songs.txt line naming such a copy plays the MP3; the folder's note says so, and that a file that cannot be read is silent (it is not left out)
- `c46e640d` 2026-10-20 z3xox, +7 -0: Vibration: a controller that keeps the game waiting for its motor command (some generic pads through DirectInput, some Bluetooth links: a player's game froze for seconds whenever it vibrated) is sent fewer, one per ten times its longest call, and after two of 50 ms none, with a notice; the test controller can be slow (BT3_RUMBLE_SLOW) and while it is attached the real ones are left out
- `109cfbe5` 2026-10-19 Renzo, +42 -0: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion
- `559f85b2` 2026-10-19 z3xox, +35 -0: Online: a desync is a warning and the match goes on (F1 setting to end it, as before); waits for the other player and slow frames in the log with what caused them, the meter says which; Windows writes bt3_log.txt next to the program
- ... e mais 132 (`git log -- docs/port/README.md`)

## `docs/port/adding_songs.md`

- `a3cf1192` 2026-10-28 z3xox, +1 -1: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `109cfbe5` 2026-10-19 Renzo, +35 -48: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion
- `7b99f8ad` 2026-10-15 dfdevx2, +39 -0: Dragon Rage 0.6.0 [skip ci]
- `19011dcb` 2026-10-13 z3xox, +31 -5: Added songs: the texts and the notes for the MP3 conversion as it is now
- `d67cd22c` 2026-10-12 Valentin Vivaldi, +13 -0: Added songs: an .mp3 in the songs folder is converted to ADX at start, once (one that cannot be read is left out, with a notice; the game goes on)
- `928b2413` 2026-10-11 dfdevx2, +37 -1: WIP: patch 1 and touch artwork
- `ca55bb2c` 2026-10-10 z3xox, +19 -0: Added songs: offered in team and DP battles too (the team select's own music list had been left without them); a song without loop points is played round like the disc's music
- `3e2b773d` 2026-10-08 z3xox, +18 -1: Added songs: file numbers past the archive's end (the 5th and 6th were the announcer's first two lines); up to 40
- `d3bccc25` 2026-10-07 z3xox, +21 -0: Names of added stages and songs in the game's own lettering, cut from the disc's name pictures at run time
- `39db4ebd` 2026-10-07 z3xox, +14 -0: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `9a2185c4` 2026-10-07 z3xox, +8 -0: docs: how the added stages and songs differ from the pull request (save, online, split screen)
- `ec0853df` 2026-10-06 z3xox, +76 -77: docs: the added stages/songs documents in English
- `0153b616` 2026-10-06 z3xox, +137 -0: Songs added to the music select: manifest, script, and the name overlay

## `docs/port/adding_stages.md`

- `a3cf1192` 2026-10-28 z3xox, +1 -1: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `928b2413` 2026-10-11 dfdevx2, +17 -0: WIP: patch 1 and touch artwork
- `e112aff2` 2026-10-10 z3xox, +7 -0: Added stages: offered in team and DP battles too (the team select's own stage list had been left without them)
- `030cecdf` 2026-10-08 z3xox, +10 -0: docs: the added stages' file numbers
- `153b81a3` 2026-10-08 z3xox, +8 -0: Stage select: room for all 26 added stages (24 were listed with every disc stage unlocked)
- `755986ea` 2026-10-07 z3xox, +25 -0: Added stages: file ids that are no disc file's (0.1.8 gave a disc stage's model to an added stage's sound bank); stages larger than the game's buffer
- `5bae1068` 2026-10-07 z3xox, +2 -0: docs: a downloaded custom map loaded from the stages folder
- `d3bccc25` 2026-10-07 z3xox, +21 -0: Names of added stages and songs in the game's own lettering, cut from the disc's name pictures at run time
- `39db4ebd` 2026-10-07 z3xox, +14 -0: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `9a2185c4` 2026-10-07 z3xox, +10 -0: docs: how the added stages and songs differ from the pull request (save, online, split screen)
- `ec0853df` 2026-10-06 z3xox, +95 -95: docs: the added stages/songs documents in English
- `28e86145` 2026-10-06 z3xox, +155 -0: Stages from outside the disc: manifest, install script, styled menu names

## `docs/port/macos_plan.md`

- `7b99f8ad` 2026-10-15 dfdevx2, +197 -0: Dragon Rage 0.6.0 [skip ci]
- `8c3bdb43` 2026-10-13 Valentin Vivaldi, +5 -0: macOS: a game address passed as an integer to a callback that takes a pointer reaches it (the memory card's new save crashed)
- `c722c17f` 2026-10-12 Valentin Vivaldi, +4 -0: macOS plan: a stale bt3_mac.dat from a blank-tables build crashes a later build at start (SIGBUS in Port_MapMemory)
- `b7f3f877` 2026-10-12 Valentin Vivaldi, +3 -2: macOS: README and plan say how to install on a Mac (install.py, about 2 minutes)
- `ed929815` 2026-10-12 Valentin Vivaldi, +2 -0: macOS: marked experimental, built from source only (README, AGENTS.md, the plan)
- `cfeee61c` 2026-10-12 Valentin Vivaldi, +4 -3: macOS: a build without game data fetches it from the disc like the Linux release (same demo-fight state on all 4428 frames)
- `81bc5508` 2026-10-12 Valentin Vivaldi, +11 -5: macOS: rollback and the state list (rewind test: the whole demo fight, 0 differences at depth 1 and 8)
- `660c1a38` 2026-10-12 Valentin Vivaldi, +178 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal

## `docs/port/menu_font.md`

- `9bed1539` 2026-10-27 Regulus CiberSec, +5 -4: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `979210ac` 2026-10-27 victorgonl, +1 -1: Expand translation extraction to story mode text
- `5d3eea11` 2026-10-27 Regulus CiberSec, +5 -4: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `2d8a5a72` 2026-10-18 dfdevx2, +78 -24: Dragon Rage 0.7.3
- `134975b3` 2026-10-17 victorgonl, +3 -2: Show language selection screen only if more than one language is available
- `4822563f` 2026-10-16 victorgonl, +1 -1: Support for ui bundled translations
- `52ebfd61` 2026-10-16 victorgonl, +77 -24: Add and update documentations related to font and translations
- `7b99f8ad` 2026-10-15 dfdevx2, +189 -0: Dragon Rage 0.6.0 [skip ci]
- `cb2b930d` 2026-10-15 z3xox, +7 -4: fonts.tsv: the use column one number again (1 Menu, 2 Dialogue / UI, 4 Battle, added up; none = 3), so lines players added keep their meaning; Fira first again (the TBO screens on Original)
- `440aa24f` 2026-10-15 victorgonl, +1 -0: Refine battle font outlines, shadows, and subtle digit relief
- `a103b16f` 2026-10-15 victorgonl, +45 -47: Add support for battle texts fonts
- `2e97fe78` 2026-10-14 z3xox, +5 -2: TBO: Fira Sans Condensed Bold (OFL) first of the Menu fonts: the nearest of the open fonts to the game menu lettering
- `dd2cf63b` 2026-10-13 victorgonl, +5 -7: Keep DejaVu Sans Condensed as the only bundled menu font
- `33ffd60c` 2026-10-13 victorgonl, +34 -9: Add font style support for menu headers
- `f86a4f39` 2026-10-13 z3xox, +42 -24: Notes: the list of menu labels made by the game
- `9ec49151` 2026-10-13 z3xox, +61 -39: Menu fonts: the notes say what the two settings do and where the files are
- `e1e19d47` 2026-10-12 victorgonl, +31 -2: Add text styles
- `58b7887c` 2026-10-12 victorgonl, +2 -2: Remove generation of PNG previews
- `2e93dc19` 2026-10-12 victorgonl, +5 -2: Add support for shared fonts in dialogue and UI (text not editable)
- `2950c9d2` 2026-10-12 victorgonl, +89 -0: Add native and editable menu text with selectable fonts

## `docs/port/menus.md`

- `a581c3f7` 2026-10-26 z3xox, +5 -3: A spectator's own pause menu is the battle's own pause menu: its box from the battle's sprite sheet, drawn by its engine (BtlMenu_Draw, src/battle/hud_0_b.c, through BtlMenu_PortDrawMenu), its colours, sounds and six-frame growing box, and the versus pause menu's own "EXIT ?" Yes / No box before leaving (PauseMenu_PortConfirmBox, src/battle/hud_0.c), instead of a Tenkaichi Budokai Online panel. Its items (port/src/spec_menu.c): the note "Your screen only: the fight goes on" in the title's row, Continue, View: Player 1 / Player 2 / Split Screen with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching; their texts reach the engine through BtlMenu_GetItemText (numbers 0x6FFF and on) and are translated by the online menus' file (tbo_strings.py lists the file). The engine's update is not run and the battle's menus are not touched: the file moves the cursor and the box itself and reads the PC's pad (Port_PadRead); in a fight only (gPortBtlMenuLive). Tested by code with an owner and a member watching and every item used with the pad: the fight values of both spectators and both players identical, no check value differed. All game-side changes under PORT: the PS2 build is unchanged.
- `b4a6f9ef` 2026-10-26 z3xox, +7 -0: Lobbies: "The screens" in docs/port/README.md (the page, chat, getting there, the spectator's line and pause menu, text, test switches, what was tested), the Lobbies and Spectators chapters brought up to date (the page instead of the window), docs/port/menus.md (drawing over the game's own screens) and docs/port/translations.md (%s in lines with a name).
- `7b99f8ad` 2026-10-15 dfdevx2, +180 -0: Dragon Rage 0.6.0 [skip ci]
- `6070f0bf` 2026-10-15 z3xox, +4 -3: TBO: its pictures in the repository (port/tbo/ui, with their credits: the announcer by MarshallPunk, the buttons by pluantoine under CC BY 4.0), read there in a source checkout and packaged as tbo/ui; tbo_sizes.py makes tbo_sizes.h (the new background size)
- `d398327d` 2026-10-15 z3xox, +30 -30: docs: menus.md, the port screen mode and the Ps_ kit
- `e44f77b6` 2026-10-15 z3xox, +159 -185: docs: menus.md as a reference (how menus work, an entry, the screen behind it, limits); the lessons to the README
- `c1f1df72` 2026-10-15 z3xox, +34 -0: docs: menus.md, a step by step recipe and the checks; the new item has no line in the box (open)
- `8e0f2c7a` 2026-10-15 z3xox, +7 -2: docs: menus.md, icons shared and labels not, by the plate code
- `4a0d7d23` 2026-10-15 z3xox, +166 -0: docs: menus.md, adding a main-menu entry and the screen behind it (what TBO did), and what is possible for mods

## `docs/port/translations.md`

- `9bed1539` 2026-10-27 Regulus CiberSec, +11 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `979210ac` 2026-10-27 victorgonl, +35 -11: Expand translation extraction to story mode text
- `5d3eea11` 2026-10-27 Regulus CiberSec, +11 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `ffc63800` 2026-10-27 z3xox, +1 -1: Docs: the automatic update. docs/port/README.md, a chapter "Automatic updates": based on PR #107 by JosephLlacchua (what it brought and what was added on top of it), the version from the tag, the flow (the check, the line over the menus, Update now / Later / What's new, the download while playing, online play held, Restart now with its two steps of the rename trick, the undoing, the cleanup), the files never written, the signature and its key, the test switches and what was tested (and not). port/release/README.md (new): making a release, its two secrets, and the update signing key once (update_key.py generate, the secret UPDATE_SIGNING_KEY, the public key into port/src/update_key.h), with the credit to PR #107. README.md: updates for players; translations.md: the update's lines in the port catalog.
- `b4a6f9ef` 2026-10-26 z3xox, +2 -1: Lobbies: "The screens" in docs/port/README.md (the page, chat, getting there, the spectator's line and pause menu, text, test switches, what was tested), the Lobbies and Spectators chapters brought up to date (the page instead of the window), docs/port/menus.md (drawing over the game's own screens) and docs/port/translations.md (%s in lines with a name).
- `c49f5e0f` 2026-10-23 z3xox, +19 -0: The online menus can be translated: translations/port/<language>.json takes their lines (ids tbo/..., looked up by their English), where the menu draws and measures a line (tbo_text.c), joins pieces (Ps_Copy, which also feeds the screen reader) and formats a number (Tb_Fmt). A translation that loses a line's %d parts or {name} is not used, and a lone % in a format line is refused, so a file cannot make the game misread. The talk box decodes UTF-8 (accents were two wrong letters), and the frame count has its own plural line. port/tools/tbo_strings.py makes a file for translators with the English as placeholder (303 lines; --bt4 without BT3's fighter names) and keeps what is translated when run again. Accents checked on screen in Spanish
- `3f922112` 2026-10-18 victorgonl, +2 -2: Original opening music extraction using European disc
- `de8da43b` 2026-10-18 victorgonl, +8 -4: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +151 -0: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +35 -40: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `35db3fd4` 2026-10-18 z3xox, +1 -1: Online: a line on the screen while a match starts (Connecting to your rival..., then Your rival "NAME" is here! The match begins.)
- `d290443b` 2026-10-17 z3xox, +18 -9: Translations: the battle HUD and the pause menu from the European disc, and its reworded lines by position
- `aa3289ee` 2026-10-17 z3xox, +36 -0: Translations: the European disc's menu art for its languages (the painted headings, menu items, buttons and title prompt)
- `134975b3` 2026-10-17 victorgonl, +3 -2: Show language selection screen only if more than one language is available
- `4822563f` 2026-10-16 victorgonl, +9 -8: Support for ui bundled translations
- `52ebfd61` 2026-10-16 victorgonl, +109 -0: Add and update documentations related to font and translations

## `docs/port/upstream_README.md`

- `928b2413` 2026-10-11 dfdevx2, +31 -5: WIP: patch 1 and touch artwork
- `750273dd` 2026-10-08 dfdx047, +106 -0: Add Dragon Rage, the Android launcher app, and its CI

## `docs/release-0.9.0.md`

- `1c4a9a16` 2026-10-25 dfdevx2, +1 -1: Docs: commit id in the Lite section
- `2f3ff7b2` 2026-10-25 dfdevx2, +74 -6: Lite: report checks, GPU dialog scrolls
- `2d6d0531` 2026-10-25 dfdevx2, +56 -0: Lite mode: dr_lite setting, SDL descriptor sets kept
- `49b9b4d4` 2026-10-25 dfdevx2, +98 -1: Docs: fullscreen and texture packs in the 0.9.0 report
- `6dabc200` 2026-10-25 dfdevx2, +325 -0: Docs: release 0.9.0 report

## `docs/release-1.0.md`

- `fafbb446` 2026-10-31 dfdevx2, +302 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `docs/roadmap.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -115: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/status.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +0 -707: Fix giant grabs, ring-outs and fallen fighter recovery
- `697d9e08` 2026-10-06 z3xox, +10 -2: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `docs/systems/audio.md`

- `109cfbe5` 2026-10-19 Renzo, +5 -0: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion

## `docs/systems/combat.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +8 -8: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/systems/fighter.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +5 -5: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/systems/graphics.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +1 -1: Fix giant grabs, ring-outs and fallen fighter recovery

## `docs/systems/math.md`

- `697d9e08` 2026-10-06 z3xox, +2 -2: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `docs/systems/menu_overlay.md`

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +2 -2: Fix giant grabs, ring-outs and fallen fighter recovery

## `include/battle/bobj_a.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -1: Add native Tag Team modes

## `include/battle/btl_ai.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -1: Add native Tag Team modes

## `include/battle/btl_ai_act.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -1: Add native Tag Team modes

## `include/battle/btl_ai_int.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -0: Add native Tag Team modes

## `include/battle/btl_ai_think.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -1: Add native Tag Team modes

## `include/battle/btl_cam.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -0: Add native Tag Team modes

## `include/battle/btl_char_flag.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +19 -10: Add native Tag Team modes

## `include/battle/btl_demo_cam.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -0: Add native Tag Team modes

## `include/battle/btl_effect_owner.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `include/battle/btl_obj.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +12 -7: Add native Tag Team modes

## `include/battle/btl_pool.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -0: Add native Tag Team modes

## `include/battle/btl_seq.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +11 -0: Add native Tag Team modes

## `include/battle/btl_tech_a.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -3: Add native Tag Team modes

## `include/battle/cpu_level.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `include/battle/eft_aa.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -7: Add native Tag Team modes

## `include/battle/eft_ab.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -0: Add native Tag Team modes

## `include/battle/eft_ae.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -5: Add native Tag Team modes

## `include/battle/eft_det_a.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -4: Add native Tag Team modes

## `include/battle/eft_disc_capacity.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `include/battle/eft_e.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -0: Add native Tag Team modes

## `include/battle/eft_fighter_capacity.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `include/battle/eft_o.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -0: Add native Tag Team modes

## `include/battle/eft_p.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -3: Add native Tag Team modes

## `include/battle/eft_r.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -1: Add native Tag Team modes

## `include/battle/eft_v_ext.h` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -25: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `include/battle/eft_z.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -2: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -10: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `include/battle/hud_0.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -0: Add native Tag Team modes

## `include/battle/stg_a.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -4: Add native Tag Team modes

## `include/battle/tag_assist.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +14 -0: Add native Tag Team modes

## `include/battle/tag_assist_profiles.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +36 -0: Add native Tag Team modes

## `include/battle/tag_balance.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +10 -0: Add native Tag Team modes

## `include/battle/tag_capacity.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `include/battle/tag_challenge.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +28 -0: Add native Tag Team modes

## `include/battle/tag_clash.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +17 -0: Add native Tag Team modes

## `include/battle/tag_clash_sequence.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +31 -0: Add native Tag Team modes

## `include/battle/tag_cpu.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -0: Add native Tag Team modes

## `include/battle/tag_fusion.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +19 -0: Add native Tag Team modes

## `include/battle/tag_giant.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +11 -0: Add native Tag Team modes

## `include/battle/tag_hud_data.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +36 -0: Add native Tag Team modes

## `include/battle/tag_lobby.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +23 -0: Add native Tag Team modes

## `include/battle/tag_lobby_rules.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +118 -0: Add native Tag Team modes

## `include/battle/tag_options.h` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +2 -1: Fix giant grabs, ring-outs and fallen fighter recovery
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +37 -7: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +125 -0: Add native Tag Team modes

## `include/battle/tag_position.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +13 -0: Add native Tag Team modes

## `include/battle/tag_post_view.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +20 -0: Add native Tag Team modes

## `include/battle/tag_revive.h` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +9 -0: Fix giant grabs, ring-outs and fallen fighter recovery

## `include/battle/tag_session.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +33 -0: Add native Tag Team modes

## `include/battle/tag_stage.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +7 -0: Add native Tag Team modes

## `include/battle/tag_team.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +62 -0: Add native Tag Team modes

## `include/battle/tag_view_layout.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +46 -0: Add native Tag Team modes

## `include/battle/view_a.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +3 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -2: Add native Tag Team modes
- `28e86145` 2026-10-06 z3xox, +5 -0: Stages from outside the disc: manifest, install script, styled menu names

## `include/battle/view_b.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -0: Add native Tag Team modes

## `include/common.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +7 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `include/menu/menu_a.h` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +6 -0: Dragon Rage 0.6.0 [skip ci]
- `178b177d` 2026-10-15 z3xox, +2 -0: Port screens: a progress mode of their own (80, PortScreen_Main by gPortScreen); TBO through it, Duel_Main the game own again
- `2574a9df` 2026-10-13 Ahmed, +4 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens

## `include/menu/menu_d.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -1: Add native Tag Team modes
- `928b2413` 2026-10-11 dfdevx2, +5 -0: WIP: patch 1 and touch artwork
- `3e2b773d` 2026-10-08 z3xox, +5 -0: Added songs: file numbers past the archive's end (the 5th and 6th were the announcer's first two lines); up to 40
- `28e86145` 2026-10-06 z3xox, +8 -0: Stages from outside the disc: manifest, install script, styled menu names

## `include/menu/menu_e.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +11 -0: Add native Tag Team modes

## `include/menu/menu_h.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls

## `include/menu/menu_i.h` ([bt3-engine](bt3-engine.md))

- `928b2413` 2026-10-11 dfdevx2, +1 -1: WIP: patch 1 and touch artwork
- `bf793603` 2026-10-09 z3xox, +1 -1: A tournament (and training, Sim Dragon) match could crash in a 64-bit build: BattleSetup_SetSide's eighth argument, a pointer, was declared s32 in three menu headers, so the caller wrote 32 bits of it on the stack

## `include/menu/menu_l.h` ([bt3-engine](bt3-engine.md))

- `928b2413` 2026-10-11 dfdevx2, +1 -1: WIP: patch 1 and touch artwork
- `bf793603` 2026-10-09 z3xox, +1 -1: A tournament (and training, Sim Dragon) match could crash in a 64-bit build: BattleSetup_SetSide's eighth argument, a pointer, was declared s32 in three menu headers, so the caller wrote 32 bits of it on the stack

## `include/menu/menu_m.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls

## `include/menu/menu_n.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls

## `include/menu/menu_q.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls
- `928b2413` 2026-10-11 dfdevx2, +1 -1: WIP: patch 1 and touch artwork
- `bf793603` 2026-10-09 z3xox, +1 -1: A tournament (and training, Sim Dragon) match could crash in a 64-bit build: BattleSetup_SetSide's eighth argument, a pointer, was declared s32 in three menu headers, so the caller wrote 32 bits of it on the stack

## `include/menu/menu_r.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls

## `include/menu/menu_z.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -2: Fix disc resources, character previews and HUD controls

## `include/menu/port_screen.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -0: Add native Tag Team modes
- `2d8a5a72` 2026-10-18 dfdevx2, +5 -0: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +5 -0: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +75 -0: Dragon Rage 0.6.0 [skip ci]
- `101712ea` 2026-10-15 z3xox, +54 -0: Port screens: the drawing kit (pictures, shapes, letters, buttons, prompts, the page) out of port_tbo.c into port_screen.c as Ps_*, unchanged
- `178b177d` 2026-10-15 z3xox, +21 -0: Port screens: a progress mode of their own (80, PortScreen_Main by gPortScreen); TBO through it, Duel_Main the game own again

## `include/menu/tag_controllers.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +29 -0: Add native Tag Team modes

## `include/menu/tag_kit.h` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +350 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `include/menu/tag_labels.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +22 -0: Add native Tag Team modes

## `include/menu/tag_outfit.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +50 -0: Add native Tag Team modes

## `include/menu/tag_setup.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -0: Add native Tag Team modes

## `include/menu/tag_tournament.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +19 -0: Add native Tag Team modes

## `include/menu/tbo_art.h` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +7 -1: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `4dc3964c` 2026-10-27 Regulus CiberSec, +1 -0: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +1 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +1 -0: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `5d3eea11` 2026-10-27 Regulus CiberSec, +1 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `de8da43b` 2026-10-18 victorgonl, +1 -4: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +8 -0: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +8 -0: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +44 -0: Dragon Rage 0.6.0 [skip ci]
- `76fa4afb` 2026-10-14 z3xox, +6 -3: Tenkaichi Budokai Online: letters from the font list, the keyboard on its keypads, Copy Code
- `2574a9df` 2026-10-13 Ahmed, +41 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens

## `include/menu/tbo_sizes.h` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +6 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `4dc3964c` 2026-10-27 Regulus CiberSec, +1 -0: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +3 -2: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +1 -0: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `5d3eea11` 2026-10-27 Regulus CiberSec, +3 -2: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `7b99f8ad` 2026-10-15 dfdevx2, +29 -0: Dragon Rage 0.6.0 [skip ci]
- `6070f0bf` 2026-10-15 z3xox, +2 -2: TBO: its pictures in the repository (port/tbo/ui, with their credits: the announcer by MarshallPunk, the buttons by pluantoine under CC BY 4.0), read there in a source checkout and packaged as tbo/ui; tbo_sizes.py makes tbo_sizes.h (the new background size)
- `76fa4afb` 2026-10-14 z3xox, +6 -44: Tenkaichi Budokai Online: letters from the font list, the keyboard on its keypads, Copy Code
- `2574a9df` 2026-10-13 Ahmed, +67 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens

## `include/port/vu0_a.h` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -156: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `include/port/vu0_b.h` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -180: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `include/sys/dma.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -0: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -21: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `include/sys/file.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +10 -0: Add native Tag Team modes

## `include/sys/gfxm_e.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes

## `include/sys/math3d.h` ([bt3-engine](bt3-engine.md))

- `07280401` 2026-10-08 Claude, +9 -0: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown

## `include/sys/movie.h` ([bt3-engine](bt3-engine.md))

- `3f922112` 2026-10-18 victorgonl, +1 -0: Original opening music extraction using European disc

## `include/sys/pad.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -1: Add native Tag Team modes

## `include/sys/snd.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +7 -3: Add native Tag Team modes

## `include/sys/spu_heap.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -0: Add native Tag Team modes

## `include/sys/vu1_packet.h` ([bt3-engine](bt3-engine.md))

- `f1619b6b` 2026-10-06 z3xox, +6 -0: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed

## `install.py`

- `2d8a5a72` 2026-10-18 dfdevx2, +20 -0: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +20 -0: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `7b99f8ad` 2026-10-15 dfdevx2, +49 -9: Dragon Rage 0.6.0 [skip ci]
- `32fc2370` 2026-10-13 z3xox, +9 -33: Menu fonts: the Python tool only applies an edited labels.json; the install step is gone
- `95d0e94e` 2026-10-13 z3xox, +14 -24: Menu fonts: the label step needs no packages, cannot fail the installation, and checks the disc
- `11ed03f6` 2026-10-13 z3xox, +3 -2: macOS: its changes to the shared build tools apply to the macOS build only (eeconst, ptr32, link, make_dat, the bsdtar reader)
- `2d312424` 2026-10-12 Valentin Vivaldi, +46 -9: macOS: install.py does the whole build from the disc image (Homebrew tools; the disc read with the system's bsdtar)
- `2950c9d2` 2026-10-12 victorgonl, +43 -7: Add native and editable menu text with selectable fonts
- `dd6b20ce` 2026-10-06 z3xox, +2 -2: Release program opens its window by default and works from its own folder (double click)
- `7fd459c6` 2026-10-06 z3xox, +0 -1: Built-in MPEG-2 intra decoder for the two movies (no ffmpeg); verified against ffmpeg on all 7,010 pictures
- `17e53ac8` 2026-10-06 z3xox, +409 -0: Setup: install.py (fresh copy + disc image -> playable build) and the bt3-setup window

## `port/bt4/CMakeLists.txt` ([bt4](bt4.md))

- `9ecce978` 2026-10-23 dfdevx2, +9 -3: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +128 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/README.md` ([bt4](bt4.md))

- `1bc64e27` 2026-10-25 dfdevx2, +2 -2: BT4: ARMSX2 by default, 60 fps fallback
- `c9e77091` 2026-10-23 dfdevx2, +20 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +9 -3: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +55 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/build_bt4.py` ([bt4](bt4.md))

- `3441216c` 2026-10-31 dfdevx2, +10 -0: BT4: the ARMSX2 library again opens on every build; the build refuses a library the APK does not carry
- `2d6d0531` 2026-10-25 dfdevx2, +1 -5: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +5 -1: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `b94a6faf` 2026-10-25 dfdevx2, +30 -1: BT4: ARMSX2 shader cache, link check
- `4c8794ef` 2026-10-24 dfdevx2, +1 -0: BT4: ARMSX2 readback, pad and env fixes
- `950764a1` 2026-10-23 dfdevx2, +4 -2: BT4: ARMSX2 renderer for 4 KB pages
- `c9e77091` 2026-10-23 dfdevx2, +65 -0: BT4: ARMSX2 renderer (libbt4gsx)
- `5707f85b` 2026-10-22 dfdevx2, +186 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/gsx/bt4_gsx.cpp` ([bt4](bt4.md))

- `b94a6faf` 2026-10-25 dfdevx2, +3 -1: BT4: ARMSX2 shader cache, link check
- `4c8794ef` 2026-10-24 dfdevx2, +1 -0: BT4: ARMSX2 readback, pad and env fixes
- `950764a1` 2026-10-23 dfdevx2, +6 -0: BT4: ARMSX2 renderer for 4 KB pages
- `c9e77091` 2026-10-23 dfdevx2, +385 -0: BT4: ARMSX2 renderer (libbt4gsx)

## `port/bt4/gsx/bt4gsx.cmake` ([bt4](bt4.md))

- `c9e77091` 2026-10-23 dfdevx2, +45 -0: BT4: ARMSX2 renderer (libbt4gsx)

## `port/bt4/gsx/cache/README.md` ([bt4](bt4.md))

- `b94a6faf` 2026-10-25 dfdevx2, +21 -0: BT4: ARMSX2 shader cache, link check

## `port/bt4/gsx/merge_cache.py` ([bt4](bt4.md))

- `b94a6faf` 2026-10-25 dfdevx2, +69 -0: BT4: ARMSX2 shader cache, link check

## `port/bt4/gsx/ndk/sources/android/cpufeatures/cpu-features.c` ([bt4](bt4.md))

- `c9e77091` 2026-10-23 dfdevx2, +5 -0: BT4: ARMSX2 renderer (libbt4gsx)

## `port/bt4/gsx/ndk/sources/android/cpufeatures/cpu-features.h` ([bt4](bt4.md))

- `c9e77091` 2026-10-23 dfdevx2, +16 -0: BT4: ARMSX2 renderer (libbt4gsx)

## `port/bt4/host/android_compat.h` ([bt4](bt4.md))

- `5707f85b` 2026-10-22 dfdevx2, +32 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/host/android_makecontext.cpp` ([bt4](bt4.md))

- `5707f85b` 2026-10-22 dfdevx2, +21 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/host/android_ucontext.S` ([bt4](bt4.md))

- `5707f85b` 2026-10-22 dfdevx2, +80 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/host/android_ucontext.h` ([bt4](bt4.md))

- `5707f85b` 2026-10-22 dfdevx2, +18 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/host/bt3gl_android.cpp` ([bt4](bt4.md))

- `5707f85b` 2026-10-22 dfdevx2, +496 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/host/bt4_main.cpp` ([bt4](bt4.md))

- `4a72a23c` 2026-10-31 dfdevx2, +52 -2: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose
- `91be0b33` 2026-10-31 dfdevx2, +3 -0: BT4: the frame buffer being drawn is the picture, not a work buffer: no noise over the far scenery
- `8cbd9d5a` 2026-10-31 dfdevx2, +13 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `d6467b24` 2026-10-25 dfdevx2, +37 -2: Texture packs: used on Android, in both games; more zips read
- `1bc64e27` 2026-10-25 dfdevx2, +53 -3: BT4: ARMSX2 by default, 60 fps fallback
- `0cabeef6` 2026-10-25 dfdevx2, +8 -1: BT4: read the disc image through its descriptor
- `069c31a6` 2026-10-24 dfdevx2, +2 -0: BT4: count frames for the texture cache
- `4c8794ef` 2026-10-24 dfdevx2, +17 -1: BT4: ARMSX2 readback, pad and env fixes
- `950764a1` 2026-10-23 dfdevx2, +16 -0: BT4: ARMSX2 renderer for 4 KB pages
- `c9e77091` 2026-10-23 dfdevx2, +238 -10: BT4: ARMSX2 renderer (libbt4gsx)
- `9ecce978` 2026-10-23 dfdevx2, +65 -0: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +616 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/host/bt4_shim.c` ([bt4](bt4.md))

- `91be0b33` 2026-10-31 dfdevx2, +1 -0: BT4: the frame buffer being drawn is the picture, not a work buffer: no noise over the far scenery
- `8cbd9d5a` 2026-10-31 dfdevx2, +2 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `a974f119` 2026-10-31 dfdevx2, +8 -0: BT4: shim symbols for 0.1.39; tag link: buffers really out of .data
- `6e57077d` 2026-10-25 dfdevx2, +6 -0: BT4: shim symbols, seam width scale
- `9ecce978` 2026-10-23 dfdevx2, +29 -5: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +109 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/host/glad_null.cpp` ([bt4](bt4.md))

- `5707f85b` 2026-10-22 dfdevx2, +124 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/bt4/patches/armsx2-bt4.patch` ([bt4](bt4.md))

- `d6467b24` 2026-10-25 dfdevx2, +25 -0: Texture packs: used on Android, in both games; more zips read
- `eefe5d9b` 2026-10-25 dfdevx2, +52 -12: BT4: ARMSX2's FPS meter counts the game's pictures
- `4c8794ef` 2026-10-24 dfdevx2, +12 -2: BT4: ARMSX2 readback, pad and env fixes
- `c9e77091` 2026-10-23 dfdevx2, +271 -0: BT4: ARMSX2 renderer (libbt4gsx)

## `port/bt4/patches/bt3recomp-android.patch` ([bt4](bt4.md))

- `0cabeef6` 2026-10-25 dfdevx2, +143 -0: BT4: read the disc image through its descriptor
- `4c8794ef` 2026-10-24 dfdevx2, +41 -0: BT4: ARMSX2 readback, pad and env fixes
- `5707f85b` 2026-10-22 dfdevx2, +111 -0: Fixes from logs, touch pause menu, BT4 menu

## `port/data/cod_002FF180.sbss.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +111 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_002FF280.bss.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +18 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_002FF300.bss.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +711 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1C3380.data.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1C3388.data.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +7 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1C3398.data.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +9 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1C3400.data.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +4445 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1EB354.data.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +10 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1EB380.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +9 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1EC248.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +28 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1ECB60.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +9 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1EE7BC.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +15 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1F2168.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +8 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1F2198.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +8 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1F3488.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +31 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1F36F8.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +245 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1F44C0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +2818 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FC2A0.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +21 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FC2CC.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +7 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FC328.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +10 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FC3FC.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +22 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FC728.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +7 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FC744.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +13 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE408.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +103 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE4BC.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +16 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE7AC.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +10 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE820.lit4.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +75 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE880.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +60 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE958.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +74 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE9DC.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +21 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FE9F0.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +27 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEA18.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +52 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEA58.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +101 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEAE0.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +51 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEB28.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +22 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEB78.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +42 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEBB0.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +75 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEC20.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +15 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FEC44.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +11 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FF05C.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +58 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FF11C.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +40 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/cod_1FF170.sdata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/dbzp_07C204.data.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +9 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EC6E0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EC8C0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EC8D0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ECE90.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ECEA0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ED360.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ED3E0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ED510.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ED550.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ED590.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002ED5D0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDA70.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDC70.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDCE0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDD50.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDDC0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDDD8.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +7 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDE10.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDEB0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002EDEE8.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +7 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002F1B90.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002F2420.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002F33D0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002F33E0.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002F3410.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_D_002F3440.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_gEftShotFxRows.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +9 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/inc_gEftShotFxSegs.rodata.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +6 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/index.txt` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +71 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/data/vu1_micro.data.s` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +30 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/dragonnet/README.md` ([bt3-engine](bt3-engine.md))

- `176d77e8` 2026-10-18 dfdevx2, +8 -2: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +45 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/chat-schema.sql` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +11 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/chat-test.mjs` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +33 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/chat.js` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +37 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/dragonnet-test.mjs` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +48 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/presence-schema.sql` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +8 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/presence-test.mjs` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +45 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/presence.js` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +39 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/ranked-migrate-rematches.sql` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +9 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/ranked-migrate-rules.sql` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +4 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/ranked-migrate-version.sql` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +4 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/ranked-rematch-test.mjs` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +64 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/ranked-schema.sql` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +25 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/ranked-test.mjs` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +73 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/ranked.js` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +190 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/schema.sql` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +2 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/test.mjs` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +331 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/versions.js` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +24 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/worker.js` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +684 -0: Dragon Rage 0.8.0: Dragon Net

## `port/dragonnet/wrangler-ranked.example.jsonc` ([bt3-engine](bt3-engine.md))

- `f11e5f18` 2026-10-18 dfdevx2, +20 -0: Dragon Rage 0.8.0: Dragon Net

## `port/include/plat_game.h` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +37 -0: Add native Tag Team modes

## `port/include/plat_songs.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes
- `109cfbe5` 2026-10-19 Renzo, +3 -3: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion
- `7b99f8ad` 2026-10-15 dfdevx2, +1 -0: Dragon Rage 0.6.0 [skip ci]
- `b5160626` 2026-10-13 z3xox, +1 -0: Added songs: MP3 files are converted in the background, the start does not wait; a song joins the list when a select screen is next set up (at start in a recorded or re-run session, never in an online one)
- `928b2413` 2026-10-11 dfdevx2, +10 -1: WIP: patch 1 and touch artwork
- `ca55bb2c` 2026-10-10 z3xox, +1 -0: Added songs: offered in team and DP battles too (the team select's own music list had been left without them); a song without loop points is played round like the disc's music
- `3e2b773d` 2026-10-08 z3xox, +9 -1: Added songs: file numbers past the archive's end (the 5th and 6th were the announcer's first two lines); up to 40
- `0153b616` 2026-10-06 z3xox, +26 -0: Songs added to the music select: manifest, script, and the name overlay

## `port/include/plat_stages.h` ([bt3-engine](bt3-engine.md))

- `28e86145` 2026-10-06 z3xox, +23 -0: Stages from outside the disc: manifest, install script, styled menu names

## `port/include/port_compat.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +15 -0: Add native Tag Team modes

## `port/include/port_libm.h` ([bt3-engine](bt3-engine.md))

- `928b2413` 2026-10-11 dfdevx2, +2 -0: WIP: patch 1 and touch artwork
- `322b51eb` 2026-10-10 z3xox, +2 -0: The replay list no longer stops the game: pow, the game's one double maths call, goes through the software-float bridge

## `port/include/port_tbo_label.h` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +615 -0: Dragon Rage 0.6.0 [skip ci]
- `2574a9df` 2026-10-13 Ahmed, +615 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens

## `port/include/tag_text.h` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +11 -0: Add native Tag Team modes

## `port/lang/a11y_es.txt` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +523 -0: Dragon Rage 0.6.0 [skip ci]
- `6ea3abf0` 2026-10-13 z3xox, +6 -0: Screen reader: the question before the game closes is said
- `13154262` 2026-10-13 z3xox, +517 -0: Screen reader: every language through a11y_<code>.txt, Spanish too

## `port/licenses/SDL.txt` ([bt3-engine](bt3-engine.md))

- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +17 -0: Add disc-based setup and simplify launchers

## `port/matchmaking/README.md` ([bt3-engine](bt3-engine.md))

- `a3cf1192` 2026-10-28 z3xox, +3 -3: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `6a8d1429` 2026-10-26 z3xox, +19 -9: Quick match: a chosen region is a preference that widens, no longer a lock. A pair is made when each side takes the other, the wait being the longer of the two: Auto takes players within 2,500 km at once and anyone from 25 s; a region takes players placed in it at once and anyone from 40 s (REGION_WIDEN_SECONDS); Anywhere and older games take anyone. So for the first 25 s Auto is nearby only and a region its players only, until 40 s Auto takes everyone but those who chose another region, and after 40 s everyone queuing meets, the nearest first. A region's search is told widen 40; its screens say "Players there first, everyone after 40 s" and, past it, "NO ONE THERE: SEARCHING EVERYWHERE", and the hosts' line says the same. No change to the database. Tested: test.mjs 185 checks (Auto in the Middle East against South America chosen: not at 30 s either way, matched at 40); through the screens with the local Worker, Dubai on Auto and Sao Paulo on South America put together at 40 s.
- `cbde53ab` 2026-10-26 z3xox, +44 -1: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `d9a688c2` 2026-10-26 z3xox, +36 -2: Lobbies: the chapter in docs/port/README.md (the window, a match and who watches whom, back to the lobby and leaving, the packets, the relay and what was not tested against Cloudflare, the test switches, what was tested and the players' pacing) and the Worker's part in port/matchmaking/README.md (the lobby's room, the answers to old games, the requests an hour, and the deploy steps: one ALTER TABLE and the new worker.js).
- `086d9073` 2026-10-26 z3xox, +41 -3: Spectators: the chapter in docs/port/README.md (how, the packets, limits, the test switches, what was tested) and the Worker's part in port/matchmaking/README.md (what /v1/watch and the poll's new fields do, the table watchers, what to deploy). BT3_TEST_SPEC_APART=<blank>: a spectator claims a wrong check value once, for testing that it follows the match again from the start.
- `7b99f8ad` 2026-10-15 dfdevx2, +184 -1: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +133 -1: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `d2b34265` 2026-10-11 z3xox, +10 -0: A relay login is asked for when a match needs one, not taken with every room (quick match's searches used up the day's logins and the relay was off for everyone)
- `c927b17d` 2026-10-11 z3xox, +5 -0: The online screen says when this release is too old for the matchmaking service, or when a newer one exists (asked once as the screen opens)
- `19360c64` 2026-10-11 z3xox, +13 -0: The matchmaking service can refuse releases older than one its keeper names (MIN_RELEASE); the game says which release it is (port_release.h, raised with each release)
- `7779c68a` 2026-10-11 z3xox, +23 -0: Quick match: five queues by battle type (single, team, DP 10 / 15 / 20, no time limit); the service pairs whoever searches a queue with the one who waits longest, the online screen has a tab for it
- `928b2413` 2026-10-11 dfdevx2, +118 -0: WIP: patch 1 and touch artwork
- `3b975c0f` 2026-10-09 z3xox, +29 -1: Matchmaking Worker: a ceiling on the relay's use. It measures the use from Cloudflare's analytics every five minutes, ends a login that passes more than a match would, and past the month's limit ends all and gives no more; /v1/status for whoever runs it; fewer logins a day
- `74eb095c` 2026-10-09 z3xox, +5 -0: Matchmaking Worker: a test of its address rules and limits (node port/matchmaking/test.mjs)
- `4706927d` 2026-10-09 z3xox, +8 -0: Matchmaking Worker: an address of a private network is handed only to a player on the same network; wrong codes limited to 30 an hour for one address
- `c9351ebb` 2026-10-09 z3xox, +14 -0: Matchmaking Worker: limits against abuse (rooms an hour and relay logins a day for one address, relay logins a day in all; table 'used'); the host asks the service less often after the first minute
- `6e7bfe74` 2026-10-09 z3xox, +12 -0: Matchmaking Worker: access to Cloudflare's TURN service for the host (if a TURN key is set up), /v1/addrs for the host's relay address, the joining player's seen address
- `444f4fd9` 2026-10-09 z3xox, +14 -1: Matchmaking: the schema as one line without comments (the dashboard's console joins lines; a -- comment took the statement with it)
- `39171cf0` 2026-10-09 z3xox, +38 -0: Matchmaking: a Cloudflare Worker for room codes (create, join, poll, leave; a D1 table), with set-up steps

## `port/matchmaking/schema.sql` ([bt3-engine](bt3-engine.md))

- `cbde53ab` 2026-10-26 z3xox, +1 -1: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `84283eaa` 2026-10-26 z3xox, +1 -1: Lobbies, the service's part and the relay: a lobby's room code, and a relay that serves several members. /v1/host with "lobby": true makes a lobby's room (a new column rooms.lobby); a join or watch of its code from a game that says "lobby" is answered with the owner's addresses and {lobby: true}, and the member's addresses are kept in watchers, where the owner's polls find them as a spectator's. A game that does not say it (0.1.38.7 and older) is answered "old" and shows "Update available..."; a new game joining an ordinary room gets the answer it always had. The owner's poll with watch and keep keeps a lobby's room for as long as it asks, a member's poll with its key gets the owner's addresses again (the relay address that came later), and /v1/relay is the owner's once a member has come. A database without the column answers "no lobbies" and makes every other room as before. In the game, net_match.c makes a lobby's room (role 5: polls every four seconds, 900 requests an hour of lobby, and asks for a relay address when the lobby says a member cannot reach it), sends "lobby" with joins and watches, and says when a code is a lobby's. The TURN client (net_relay.c) now serves up to twelve addresses with a channel each and lets sixteen through, a ChannelBind's answer matched to its request by the transaction; a match of two uses one place as before. node port/matchmaking/test.mjs: 17 new checks.
- `85daa805` 2026-10-26 z3xox, +1 -0: Spectators with a room code. The Worker takes /v1/watch {v, code, name, addrs}: a spectator of any room of its version gets the host's addresses, and its own (with "seen=" for the host's relay) are kept two minutes in a new table `watchers` (schema.sql; at most 12 a room in that time). A host that sends `watch: true` with its poll is told them (`watchers`), and with `keep: true` as well a room someone has joined starts its ten minutes again, so that the room lives as long as the match: the game's host now keeps polling every five seconds during its session when it lets spectators in, and gives the room up when the session ends. Older games send neither and get the answers they always had (checked: two 0.1.38.7 copies play a match through the new Worker). In the game: "Watch the match" with a code on the Join tab, BT3_LOBBY=spectate:CODE; the host knocks at each new spectator's addresses for ten seconds and lets it through its relay, in its lobby (spectators who come before the other player are told to wait) and in the match. A spectator with a code of a 0.1.38.7 host is told why it found nobody (no answer, or no room once the match has begun); with a Worker that does not know /v1/watch yet, that the service does not take spectators yet. Room-code errors may now be up to 255 characters. node port/matchmaking/test.mjs: 13 new checks. The joiner's log says who watches.
- `7b99f8ad` 2026-10-15 dfdevx2, +1 -1: Dragon Rage 0.6.0 [skip ci]
- `7779c68a` 2026-10-11 z3xox, +1 -1: Quick match: five queues by battle type (single, team, DP 10 / 15 / 20, no time limit); the service pairs whoever searches a queue with the one who waits longest, the online screen has a tab for it
- `928b2413` 2026-10-11 dfdevx2, +2 -0: WIP: patch 1 and touch artwork
- `c9351ebb` 2026-10-09 z3xox, +1 -0: Matchmaking Worker: limits against abuse (rooms an hour and relay logins a day for one address, relay logins a day in all; table 'used'); the host asks the service less often after the first minute
- `444f4fd9` 2026-10-09 z3xox, +1 -14: Matchmaking: the schema as one line without comments (the dashboard's console joins lines; a -- comment took the statement with it)
- `39171cf0` 2026-10-09 z3xox, +14 -0: Matchmaking: a Cloudflare Worker for room codes (create, join, poll, leave; a D1 table), with set-up steps

## `port/matchmaking/test.mjs` ([bt3-engine](bt3-engine.md))

- `6a8d1429` 2026-10-26 z3xox, +92 -54: Quick match: a chosen region is a preference that widens, no longer a lock. A pair is made when each side takes the other, the wait being the longer of the two: Auto takes players within 2,500 km at once and anyone from 25 s; a region takes players placed in it at once and anyone from 40 s (REGION_WIDEN_SECONDS); Anywhere and older games take anyone. So for the first 25 s Auto is nearby only and a region its players only, until 40 s Auto takes everyone but those who chose another region, and after 40 s everyone queuing meets, the nearest first. A region's search is told widen 40; its screens say "Players there first, everyone after 40 s" and, past it, "NO ONE THERE: SEARCHING EVERYWHERE", and the hosts' line says the same. No change to the database. Tested: test.mjs 185 checks (Auto in the Middle East against South America chosen: not at 30 s either way, matched at 40); through the screens with the local Worker, Dubai on Auto and Sao Paulo on South America put together at 40 s.
- `cbde53ab` 2026-10-26 z3xox, +214 -3: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `84283eaa` 2026-10-26 z3xox, +46 -1: Lobbies, the service's part and the relay: a lobby's room code, and a relay that serves several members. /v1/host with "lobby": true makes a lobby's room (a new column rooms.lobby); a join or watch of its code from a game that says "lobby" is answered with the owner's addresses and {lobby: true}, and the member's addresses are kept in watchers, where the owner's polls find them as a spectator's. A game that does not say it (0.1.38.7 and older) is answered "old" and shows "Update available..."; a new game joining an ordinary room gets the answer it always had. The owner's poll with watch and keep keeps a lobby's room for as long as it asks, a member's poll with its key gets the owner's addresses again (the relay address that came later), and /v1/relay is the owner's once a member has come. A database without the column answers "no lobbies" and makes every other room as before. In the game, net_match.c makes a lobby's room (role 5: polls every four seconds, 900 requests an hour of lobby, and asks for a relay address when the lobby says a member cannot reach it), sends "lobby" with joins and watches, and says when a code is a lobby's. The TURN client (net_relay.c) now serves up to twelve addresses with a channel each and lets sixteen through, a ChannelBind's answer matched to its request by the transaction; a match of two uses one place as before. node port/matchmaking/test.mjs: 17 new checks.
- `85daa805` 2026-10-26 z3xox, +40 -1: Spectators with a room code. The Worker takes /v1/watch {v, code, name, addrs}: a spectator of any room of its version gets the host's addresses, and its own (with "seen=" for the host's relay) are kept two minutes in a new table `watchers` (schema.sql; at most 12 a room in that time). A host that sends `watch: true` with its poll is told them (`watchers`), and with `keep: true` as well a room someone has joined starts its ten minutes again, so that the room lives as long as the match: the game's host now keeps polling every five seconds during its session when it lets spectators in, and gives the room up when the session ends. Older games send neither and get the answers they always had (checked: two 0.1.38.7 copies play a match through the new Worker). In the game: "Watch the match" with a code on the Join tab, BT3_LOBBY=spectate:CODE; the host knocks at each new spectator's addresses for ten seconds and lets it through its relay, in its lobby (spectators who come before the other player are told to wait) and in the match. A spectator with a code of a 0.1.38.7 host is told why it found nobody (no answer, or no room once the match has begun); with a Worker that does not know /v1/watch yet, that the service does not take spectators yet. Room-code errors may now be up to 255 characters. node port/matchmaking/test.mjs: 13 new checks. The joiner's log says who watches.
- `7b99f8ad` 2026-10-15 dfdevx2, +213 -1: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +142 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `d2b34265` 2026-10-11 z3xox, +15 -0: A relay login is asked for when a match needs one, not taken with every room (quick match's searches used up the day's logins and the relay was off for everyone)
- `c927b17d` 2026-10-11 z3xox, +2 -0: The online screen says when this release is too old for the matchmaking service, or when a newer one exists (asked once as the screen opens)
- `19360c64` 2026-10-11 z3xox, +11 -0: The matchmaking service can refuse releases older than one its keeper names (MIN_RELEASE); the game says which release it is (port_release.h, raised with each release)
- `944e7cd4` 2026-10-11 z3xox, +3 -0: Quick match: the count of who waits is one query, asked every thirty seconds and only while the player is at the tab
- `7779c68a` 2026-10-11 z3xox, +40 -1: Quick match: five queues by battle type (single, team, DP 10 / 15 / 20, no time limit); the service pairs whoever searches a queue with the one who waits longest, the online screen has a tab for it
- `928b2413` 2026-10-11 dfdevx2, +108 -0: WIP: patch 1 and touch artwork
- `3b975c0f` 2026-10-09 z3xox, +42 -0: Matchmaking Worker: a ceiling on the relay's use. It measures the use from Cloudflare's analytics every five minutes, ends a login that passes more than a match would, and past the month's limit ends all and gives no more; /v1/status for whoever runs it; fewer logins a day
- `74eb095c` 2026-10-09 z3xox, +66 -0: Matchmaking Worker: a test of its address rules and limits (node port/matchmaking/test.mjs)

## `port/matchmaking/test_sign.py` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +134 -0: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +134 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets

## `port/matchmaking/worker.js` ([bt3-engine](bt3-engine.md))

- `6a8d1429` 2026-10-26 z3xox, +38 -29: Quick match: a chosen region is a preference that widens, no longer a lock. A pair is made when each side takes the other, the wait being the longer of the two: Auto takes players within 2,500 km at once and anyone from 25 s; a region takes players placed in it at once and anyone from 40 s (REGION_WIDEN_SECONDS); Anywhere and older games take anyone. So for the first 25 s Auto is nearby only and a region its players only, until 40 s Auto takes everyone but those who chose another region, and after 40 s everyone queuing meets, the nearest first. A region's search is told widen 40; its screens say "Players there first, everyone after 40 s" and, past it, "NO ONE THERE: SEARCHING EVERYWHERE", and the hosts' line says the same. No change to the database. Tested: test.mjs 185 checks (Auto in the Middle East against South America chosen: not at 30 s either way, matched at 40); through the screens with the local Worker, Dubai on Auto and Sao Paulo on South America put together at 40 s.
- `cbde53ab` 2026-10-26 z3xox, +219 -26: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `84283eaa` 2026-10-26 z3xox, +77 -9: Lobbies, the service's part and the relay: a lobby's room code, and a relay that serves several members. /v1/host with "lobby": true makes a lobby's room (a new column rooms.lobby); a join or watch of its code from a game that says "lobby" is answered with the owner's addresses and {lobby: true}, and the member's addresses are kept in watchers, where the owner's polls find them as a spectator's. A game that does not say it (0.1.38.7 and older) is answered "old" and shows "Update available..."; a new game joining an ordinary room gets the answer it always had. The owner's poll with watch and keep keeps a lobby's room for as long as it asks, a member's poll with its key gets the owner's addresses again (the relay address that came later), and /v1/relay is the owner's once a member has come. A database without the column answers "no lobbies" and makes every other room as before. In the game, net_match.c makes a lobby's room (role 5: polls every four seconds, 900 requests an hour of lobby, and asks for a relay address when the lobby says a member cannot reach it), sends "lobby" with joins and watches, and says when a code is a lobby's. The TURN client (net_relay.c) now serves up to twelve addresses with a channel each and lets sixteen through, a ChannelBind's answer matched to its request by the transaction; a match of two uses one place as before. node port/matchmaking/test.mjs: 17 new checks.
- `85daa805` 2026-10-26 z3xox, +57 -5: Spectators with a room code. The Worker takes /v1/watch {v, code, name, addrs}: a spectator of any room of its version gets the host's addresses, and its own (with "seen=" for the host's relay) are kept two minutes in a new table `watchers` (schema.sql; at most 12 a room in that time). A host that sends `watch: true` with its poll is told them (`watchers`), and with `keep: true` as well a room someone has joined starts its ten minutes again, so that the room lives as long as the match: the game's host now keeps polling every five seconds during its session when it lets spectators in, and gives the room up when the session ends. Older games send neither and get the answers they always had (checked: two 0.1.38.7 copies play a match through the new Worker). In the game: "Watch the match" with a code on the Join tab, BT3_LOBBY=spectate:CODE; the host knocks at each new spectator's addresses for ten seconds and lets it through its relay, in its lobby (spectators who come before the other player are told to wait) and in the match. A spectator with a code of a 0.1.38.7 host is told why it found nobody (no answer, or no room once the match has begun); with a Worker that does not know /v1/watch yet, that the service does not take spectators yet. Room-code errors may now be up to 255 characters. node port/matchmaking/test.mjs: 13 new checks. The joiner's log says who watches.
- `7b99f8ad` 2026-10-15 dfdevx2, +272 -33: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +133 -7: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `d2b34265` 2026-10-11 z3xox, +20 -2: A relay login is asked for when a match needs one, not taken with every room (quick match's searches used up the day's logins and the relay was off for everyone)
- `c927b17d` 2026-10-11 z3xox, +4 -0: The online screen says when this release is too old for the matchmaking service, or when a newer one exists (asked once as the screen opens)
- `19360c64` 2026-10-11 z3xox, +10 -1: The matchmaking service can refuse releases older than one its keeper names (MIN_RELEASE); the game says which release it is (port_release.h, raised with each release)
- `944e7cd4` 2026-10-11 z3xox, +8 -3: Quick match: the count of who waits is one query, asked every thirty seconds and only while the player is at the tab
- `7779c68a` 2026-10-11 z3xox, +106 -29: Quick match: five queues by battle type (single, team, DP 10 / 15 / 20, no time limit); the service pairs whoever searches a queue with the one who waits longest, the online screen has a tab for it
- `928b2413` 2026-10-11 dfdevx2, +420 -0: WIP: patch 1 and touch artwork
- `3b975c0f` 2026-10-09 z3xox, +120 -3: Matchmaking Worker: a ceiling on the relay's use. It measures the use from Cloudflare's analytics every five minutes, ends a login that passes more than a match would, and past the month's limit ends all and gives no more; /v1/status for whoever runs it; fewer logins a day
- `4706927d` 2026-10-09 z3xox, +80 -6: Matchmaking Worker: an address of a private network is handed only to a player on the same network; wrong codes limited to 30 an hour for one address
- `c9351ebb` 2026-10-09 z3xox, +40 -1: Matchmaking Worker: limits against abuse (rooms an hour and relay logins a day for one address, relay logins a day in all; table 'used'); the host asks the service less often after the first minute
- `afc27c34` 2026-10-09 z3xox, +10 -7: Matchmaking Worker: says why there is no relay (which secret is missing, what the TURN service answered); takes the TURN service's answer in either shape
- `6e7bfe74` 2026-10-09 z3xox, +49 -4: Matchmaking Worker: access to Cloudflare's TURN service for the host (if a TURN key is set up), /v1/addrs for the host's relay address, the joining player's seen address
- `39171cf0` 2026-10-09 z3xox, +142 -0: Matchmaking: a Cloudflare Worker for room codes (create, join, poll, leave; a D1 table), with set-up steps

## `port/release/Dockerfile` ([bt3-engine](bt3-engine.md))

- `bc80dfe4` 2026-10-06 z3xox, +12 -7: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone
- `35eb9563` 2026-10-06 z3xox, +33 -0: Linux release built in an Ubuntu 22.04 container (baseline x86-64, glibc 2.35, SDL3 from source); portability check in package.py

## `port/release/README.md` ([bt3-engine](bt3-engine.md))

- `d4e61322` 2026-10-28 z3xox, +7 -0: Docs: 0.1.39, lobbies replace rooms. docs/port/README.md, a chapter: the list without Create Room, New Lobby, Join without Watch, by address as a lobby, Dragon Net Battle on lobbies, NET_VERSION 6 and how a 0.1.38 game is told apart both ways, FP_PEER_REQUIRED, the update's version compare, MIN_RELEASE 139 after publishing, and what was tested (a lobby by code with two matches and a spectator, by address, a 4-entrant tournament, quick match, 0.1.38.9 against 0.1.39 by address both ways, by code and in the quick match queue, a member without a fingerprint, the regular picture and speed checks); notes in the earlier chapters where they send players to Create Room or Watch. README.md: online play with lobbies. port/release/README.md: MIN_RELEASE after a release that changes NET_VERSION.
- `ffc63800` 2026-10-27 z3xox, +50 -0: Docs: the automatic update. docs/port/README.md, a chapter "Automatic updates": based on PR #107 by JosephLlacchua (what it brought and what was added on top of it), the version from the tag, the flow (the check, the line over the menus, Update now / Later / What's new, the download while playing, online play held, Restart now with its two steps of the rename trick, the undoing, the cleanup), the files never written, the signature and its key, the test switches and what was tested (and not). port/release/README.md (new): making a release, its two secrets, and the update signing key once (update_key.py generate, the secret UPDATE_SIGNING_KEY, the public key into port/src/update_key.h), with the credit to PR #107. README.md: updates for players; translations.md: the update's lines in the port catalog.

## `port/release/build.sh` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +1 -1: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.
- `0ca19719` 2026-10-27 z3xox, +3 -1: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `7b99f8ad` 2026-10-15 dfdevx2, +3 -1: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +3 -1: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `434b8581` 2026-10-06 z3xox, +1 -1: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `bc80dfe4` 2026-10-06 z3xox, +12 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/release/inside.sh` ([bt3-engine](bt3-engine.md))

- `0ca19719` 2026-10-27 z3xox, +2 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `7b99f8ad` 2026-10-15 dfdevx2, +2 -0: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +2 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `434b8581` 2026-10-06 z3xox, +1 -1: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `8d8ba102` 2026-10-06 z3xox, +4 -3: Release build: Windows section padding in make_dat; failures stop the build
- `bc80dfe4` 2026-10-06 z3xox, +27 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/run.sh` ([bt3-engine](bt3-engine.md))

- `66e2a7b1` 2026-10-06 z3xox, +2 -1: Port: 64-bit build (game pointers stay 4 bytes: ptr32.py, irfix.py, low memory); replay and heap match the 32-bit build
- `d26776bc` 2026-10-06 z3xox, +1 -1: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `port/setup/build.sh` ([bt3-engine](bt3-engine.md))

- `928b2413` 2026-10-11 dfdevx2, +2 -2: WIP: patch 1 and touch artwork
- `a7843dd4` 2026-10-08 z3xox, +2 -2: Setup window: OpenGL when the GPU layer cannot start (no Vulkan, an old graphics chip), and BT3_GPU_API=gl as for the game
- `434b8581` 2026-10-06 z3xox, +4 -4: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `35eb9563` 2026-10-06 z3xox, +1 -1: Linux release built in an Ubuntu 22.04 container (baseline x86-64, glibc 2.35, SDL3 from source); portability check in package.py
- `f1619b6b` 2026-10-06 z3xox, +28 -6: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `0c841750` 2026-10-06 z3xox, +3 -3: Release zip: native installer in the setup window (ISO reader, AFS split, self-test), package.py, windowless --install
- `17e53ac8` 2026-10-06 z3xox, +12 -0: Setup: install.py (fresh copy + disc image -> playable build) and the bt3-setup window

## `port/setup/native.cpp` ([bt3-engine](bt3-engine.md))

- `979210ac` 2026-10-27 victorgonl, +4 -3: Expand translation extraction to story mode text
- `3f922112` 2026-10-18 victorgonl, +22 -3: Original opening music extraction using European disc
- `de8da43b` 2026-10-18 victorgonl, +22 -5: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +99 -12: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +99 -12: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `7a15c277` 2026-10-06 z3xox, +23 -0: Setup: the self-test's log says how the game exited (exit code, crash report)
- `dd6b20ce` 2026-10-06 z3xox, +2 -1: Release program opens its window by default and works from its own folder (double click)
- `434b8581` 2026-10-06 z3xox, +3 -3: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `f1619b6b` 2026-10-06 z3xox, +7 -2: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `0c841750` 2026-10-06 z3xox, +429 -0: Release zip: native installer in the setup window (ISO reader, AFS split, self-test), package.py, windowless --install

## `port/setup/native.h` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +4 -1: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +4 -1: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `0c841750` 2026-10-06 z3xox, +14 -0: Release zip: native installer in the setup window (ISO reader, AFS split, self-test), package.py, windowless --install

## `port/setup/setup.cpp` ([bt3-engine](bt3-engine.md))

- `3f922112` 2026-10-18 victorgonl, +3 -3: Original opening music extraction using European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +82 -13: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +82 -13: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `7b99f8ad` 2026-10-15 dfdevx2, +50 -0: Dragon Rage 0.6.0 [skip ci]
- `32fc2370` 2026-10-13 z3xox, +1 -2: Menu fonts: the Python tool only applies an edited labels.json; the install step is gone
- `ba9b0f67` 2026-10-13 z3xox, +47 -0: Setup, Windows: the disc image's path is taken as UTF-8 from the command line
- `b548b8d8` 2026-10-13 z3xox, +3 -0: Setup: the keyboard reaches the buttons (Tab, arrows, Space or Enter)
- `95d0e94e` 2026-10-13 z3xox, +1 -1: Menu fonts: the label step needs no packages, cannot fail the installation, and checks the disc
- `2950c9d2` 2026-10-12 victorgonl, +2 -1: Add native and editable menu text with selectable fonts
- `928b2413` 2026-10-11 dfdevx2, +111 -12: WIP: patch 1 and touch artwork
- `a7843dd4` 2026-10-08 z3xox, +111 -12: Setup window: OpenGL when the GPU layer cannot start (no Vulkan, an old graphics chip), and BT3_GPU_API=gl as for the game
- `434b8581` 2026-10-06 z3xox, +13 -11: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `f1619b6b` 2026-10-06 z3xox, +2 -2: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `8f58c889` 2026-10-06 z3xox, +2 -1: Setup window: text for the release folder; notes on the window test of the native path
- `0c841750` 2026-10-06 z3xox, +123 -15: Release zip: native installer in the setup window (ISO reader, AFS split, self-test), package.py, windowless --install
- `17e53ac8` 2026-10-06 z3xox, +635 -0: Setup: install.py (fresh copy + disc image -> playable build) and the bt3-setup window

## `port/src/a11y_game.c` ([bt3-engine](bt3-engine.md))

- `0ca19719` 2026-10-27 z3xox, +6 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -4: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +3603 -0: Dragon Rage 0.6.0 [skip ci]
- `3d8b2901` 2026-10-15 z3xox, +7 -7: Main menu: allocated at its PC size (TBO made it 4 bytes longer than the 0x188 allocated); the screen reader reads its 12 items again, Tenkaichi Budokai Online by name
- `178b177d` 2026-10-15 z3xox, +2 -1: Port screens: a progress mode of their own (80, PortScreen_Main by gPortScreen); TBO through it, Duel_Main the game own again
- `13154262` 2026-10-13 z3xox, +246 -291: Screen reader: every language through a11y_<code>.txt, Spanish too
- `e2ab5d99` 2026-10-13 z3xox, +16 -10: Screen reader: small fixes from the review
- `01e7ea01` 2026-10-13 z3xox, +17 -2: Screen reader: online, the pause menu of the other player is not read and does not quiet the cues
- `9348a847` 2026-10-13 z3xox, +44 -8: Screen reader: a fight is told from the roster, not from BattleWork.running alone
- `930e65b3` 2026-10-13 piapenia, +161 -150: port: community translations of the screen reader text (a11y_<code>.txt)
- `1604bb8b` 2026-10-13 piapenia, +87 -0: a11y: panned fight cues for a transformation or the Ultimate Blast within reach
- `71a7a193` 2026-10-13 piapenia, +2460 -109: a11y: the rest of the menus, the fight's gauges on request, one option at a time
- `715afc18` 2026-10-11 piapenia, +339 -13: a11y: quieter guides, no subtitles, screens without a reader of their own
- `0aa94971` 2026-10-11 piapenia, +815 -0: Screen reader: the menus speak through Prism, off unless prism.dll is beside the program

## `port/src/fight_panels.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -2: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +184 -0: Dragon Rage 0.6.0 [skip ci]
- `cc012f78` 2026-10-15 z3xox, +184 -0: Side bars: Fighter panels (a contributor's patch): in a 4:3 fight in a wide window, each side's portrait, name and form; fonts from the game's list (fonts.tsv), the portrait decoder bounded

## `port/src/gs/a11y.c` ([renderer-gs](renderer-gs.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +896 -0: Dragon Rage 0.6.0 [skip ci]
- `6ea3abf0` 2026-10-13 z3xox, +10 -2: Screen reader: the question before the game closes is said
- `13154262` 2026-10-13 z3xox, +140 -85: Screen reader: every language through a11y_<code>.txt, Spanish too
- `1e34830e` 2026-10-13 z3xox, +27 -7: Screen reader: room for a larger PrismConfig, and what of Prism is not guarded written down
- `e2ab5d99` 2026-10-13 z3xox, +23 -3: Screen reader: small fixes from the review
- `9348a847` 2026-10-13 z3xox, +13 -1: Screen reader: a fight is told from the roster, not from BattleWork.running alone
- `930e65b3` 2026-10-13 piapenia, +139 -36: port: community translations of the screen reader text (a11y_<code>.txt)
- `1604bb8b` 2026-10-13 piapenia, +95 -1: a11y: panned fight cues for a transformation or the Ultimate Blast within reach
- `71a7a193` 2026-10-13 piapenia, +97 -8: a11y: the rest of the menus, the fight's gauges on request, one option at a time
- `715afc18` 2026-10-11 piapenia, +25 -2: a11y: quieter guides, no subtitles, screens without a reader of their own
- `0aa94971` 2026-10-11 piapenia, +472 -0: Screen reader: the menus speak through Prism, off unless prism.dll is beside the program

## `port/src/gs/fight_panels.h` ([renderer-gs](renderer-gs.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +45 -0: Dragon Rage 0.6.0 [skip ci]
- `cc012f78` 2026-10-15 z3xox, +45 -0: Side bars: Fighter panels (a contributor's patch): in a 4:3 fight in a wide window, each side's portrait, name and form; fonts from the game's list (fonts.tsv), the portrait decoder bounded

## `port/src/gs/fight_panels_ui.cpp` ([renderer-gs](renderer-gs.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +7 -1: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +912 -0: Dragon Rage 0.6.0 [skip ci]
- `cc012f78` 2026-10-15 z3xox, +912 -0: Side bars: Fighter panels (a contributor's patch): in a 4:3 fight in a wide window, each side's portrait, name and form; fonts from the game's list (fonts.tsv), the portrait decoder bounded

## `port/src/gs/gl_translate.h` ([renderer-gs](renderer-gs.md))

- `1867006b` 2026-10-17 dfdevx2, +23 -3: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +4 -0: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +80 -0: Dragon Rage 0.5.0 [skip ci]

## `port/src/gs/gs_adpf.c` ([renderer-gs](renderer-gs.md))

- `9ecce978` 2026-10-23 dfdevx2, +91 -0: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers

## `port/src/gs/gs_astc.cpp` ([renderer-gs](renderer-gs.md))

- `1867006b` 2026-10-17 dfdevx2, +226 -0: Dragon Rage 0.7.0

## `port/src/gs/gs_core.c` ([renderer-gs](renderer-gs.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +7 -1: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `2d6d0531` 2026-10-25 dfdevx2, +3 -2: Lite mode: dr_lite setting, SDL descriptor sets kept
- `6e57077d` 2026-10-25 dfdevx2, +4 -2: BT4: shim symbols, seam width scale
- `2e894c0c` 2026-10-25 dfdevx2, +5 -2: GS: mark every page a 24-bit upload touches
- `3eca58d1` 2026-10-24 z3xox, +30 -1: A seam's triangles are drawn half a pixel larger. At their exact positions the seam and the parts beside it still left a pixel open here and there, where one's edge has a corner the other's has not: a few marks at 4x, about a dozen at 8x on Videl's shirt. Overlapping them a little closes those (no line of its own at 4x or 8x). Only triangles whose three corners came from the seam routine; BT3_SEAM_WIDEN=0: their own size. Unchanged: every screenshot of the recorded fights, uncapped speed (246 / 336 fps), input to picture 33.36 ms, pacing
- `5a2a000f` 2026-10-24 z3xox, +49 -0: A model's seam is drawn at the positions its routine works out, not rounded to the GS's 1/16 pixel. The seam (the strip joining two parts of a model: the back of Videl's shirt) is projected on the CPU and sent as plain GS triangles (XYZF2), while the parts beside it come from the vertex programs at full precision: their edges did not meet, and the black outline behind showed through, a dashed line down the seam (from a player's screenshot; at 4x about 15 marks, now 3 single pixels where the meshes do not share a corner). The seam routine leaves each vertex's exact position under its rounded one (its floats as bits: that file uses the game's software float); a GS vertex with that position and depth is drawn at it. BT3_SEAM_ROUND=1: as before. Unchanged: every screenshot of the recorded fights, uncapped speed (243 / 336 fps), input to picture 33.36 ms, pacing
- `01002cc3` 2026-10-24 z3xox, +12 -2: BT3_DRAW_LOG splits the list's time further: vertex programs (their draws in it), pictures written into GS memory, and all texture lookups. In the heaviest recorded frame (116 new textures): 12.1 ms in vertex programs, 13.7 in lookups, 0.4 writing into GS memory
- `9ecce978` 2026-10-23 dfdevx2, +4 -0: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `071dc055` 2026-10-23 z3xox, +5 -0: BT3_DRAW_LOG=<ms>: each picture whose drawing took longer, timed where it is drawn (the render thread or the game's), from the start of its list to the driver, in parts: walking the list (new textures decoded, pipelines made), uploads, issuing the draws, waiting for the screen's buffer, showing, handing over. The per-second lines could not tell what made 1 drawing in 10 take 10 to 20 ms instead of 2 on a laptop, and with the render thread they see none of the drawing
- `e21b3737` 2026-10-22 z3xox, +22 -0: native-fx: with the render thread, no more input lag than without it when the game has time to spare. The game read the pads right after handing its list over while the thread drew it, so what it read was older by the drawing's time when its picture came: 35.8 ms from the pad read to its picture instead of 33.4 (2x, an integrated chip). Now the pad read waits for the thread to have drawn the list and shown its first picture (Gs_RenderListWait; with Smooth motion not for the pictures after it) when the last vertical blank was waited for; a game that is behind (or uncapped) does not wait, and keeps the work side by side. 33.4 ms with and without the thread; uncapped at 1x 337 pictures a second against 256 as before; Smooth motion as even
- `925d75f7` 2026-10-22 z3xox, +23 -2: native-fx: the render thread is stopped at the program's end. A battle's end (or any exit from the game's thread) took the window and the device down while the thread still showed the tick's last pictures or took the window's events: a crash on the way out (found with a real window). render_stop (atexit): the list being drawn is finished, the thread leaves its loop and is waited for; from the render thread itself there is nothing to wait for
- `153b847c` 2026-10-22 z3xox, +39 -4: native-fx: the render thread owns the window's events and becomes a setting. Presenting has to be done by the thread that made the window, and that is the render thread; two threads taking the window's events is not safe. So the render thread takes them (PortInput_Pump) every millisecond it waits, for the game's next list or for a Smooth-motion picture's time, and the game's thread no longer does (PortInput_Fresh); the state of the controllers and keys it reads is at most a millisecond old. F1 > Video, "Render on a second processor core" (setting render_thread, off by default, after a restart; BT3_GS_THREAD=0/1 over it); never on macOS, not with OpenGL chosen or with two logical processors or fewer. If Vulkan does not start on the thread and OpenGL does, the game's own reads of the picture and the held picture online step aside (OpenGL's context is the thread's). The recording the same with the thread (28 of 28, the game values), Smooth motion as even (spread 0.57 ms); the Windows build runs it (Wine)
- `fb85a2bc` 2026-10-22 z3xox, +31 -3: native-fx: the render thread with the rest of the renderer and with Smooth motion. Gs_RenderIdle: the game's thread waits for the thread before it touches what the thread uses (state keeping, the cross-fade's read of the picture, which no longer returns black with the thread, the picture held while waiting online); not on the thread itself (the settings window calls one of them from there). Smooth motion: the tick's pictures after the first are the thread's (GsGpu_InterpRun), each when due, while the game computes the next tick; a list handed over early has the real picture shown at once (GsGpu_InterpHurry, for the list being run only). The wall patches the game hands over are written and read under a lock. BT3_PRESENT_LOG writes whole lines. With the thread: the recording the same (28 of 28 shots, the game values), Smooth motion at 60 as even as without it (16.68 ms, spread 0.60 against 0.61), at 120 too; one shot of 28 differs by 24 pixels on the edge of an animated effect (the real picture without the thread was drawn after the next tick's textures had come)
- `dd849fd1` 2026-10-22 z3xox, +69 -8: native-fx: the render thread (BT3_GS_THREAD=1) works from a copy of the frame's list. The game goes on with the next frame while the thread draws and wrote over data the list refers to (BT3_GS_RACE: the block of the view the fighters are drawn with, a jumping camera); every block the list takes, tag included, is now copied on the game's thread at the hand-over (freeze_block) and the thread runs the copy (run_frozen). The same pictures as without the thread (346 of 346, every fifth frame of a fight) and the same fight values; a fight uncapped at 1x on an integrated chip 6.4 s instead of 10.2. Still off by default: the window is the thread's while the game's thread pumps its events before reading the pads (late input), the cross-fade's read, the state keeping and Smooth motion do not wait for the thread yet
- `559f85b2` 2026-10-19 z3xox, +12 -6: Online: a desync is a warning and the match goes on (F1 setting to end it, as before); waits for the other player and slow frames in the log with what caused them, the meter says which; Windows writes bt3_log.txt next to the program
- `7b99f8ad` 2026-10-15 dfdevx2, +16 -15: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +14 -13: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `074f44ca` 2026-10-11 z3xox, +2 -2: Frame skipping: a tick's picture is left out when the game is behind its grid, instead of the game slowing down (setting frame_skip, on by default)
- `928b2413` 2026-10-11 dfdevx2, +77 -1: WIP: patch 1 and touch artwork
- `8ed3d38a` 2026-10-10 z3xox, +16 -0: The cross-fade keeps the old shot at the resolution it was drawn in (a copy on the card; read back and uploaded it was the PS2's 512 x 448, blocky)
- `021fc2fc` 2026-10-10 z3xox, +61 -1: The cross-fade between the shots of a fight's intro: the game's read-back of the frame is answered (software renderer and both GPU back ends)
- `5ddecbfd` 2026-10-08 z3xox, +35 -1: Drawing: strips as indexed triangles, command data used in place, getenv remembered per place, a faster page hash
- `59e6bec5` 2026-10-07 z3xox, +5 -0: Re-run frames: vertex programs not run, vertices not collected (uploads and texture-memory sprites kept); 1.27 -> 1.13 ms
- `3fbf12d1` 2026-10-07 z3xox, +43 -0: Online session inside the running program: the state at the first blank is kept, a session puts the game back to it and leaving brings the player's own state back (no restart); GS memory kept with it; sound made to fit the state
- `6130d57b` 2026-10-07 z3xox, +3 -1: Frames that are only re-run draw nothing, show nothing and start no sound (gPortResim); the state test's undone pass runs that way and still leaves the same state and picture
- ... e mais 9 (`git log -- port/src/gs/gs_core.c`)

## `port/src/gs/gs_draw.c` ([renderer-gs](renderer-gs.md))

- `91be0b33` 2026-10-31 dfdevx2, +11 -3: BT4: the frame buffer being drawn is the picture, not a work buffer: no noise over the far scenery
- `fea06c41` 2026-10-31 dfdevx2, +2 -1: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump
- `f8b96695` 2026-10-28 Regulus CiberSec, +17 -13: Dragon Net Online: letras não ficam mais distorcidas ao navegar pelos menus
- `e84c359f` 2026-10-28 Regulus CiberSec, +17 -13: Dragon Net Online: letras não ficam mais distorcidas ao navegar pelos menus
- `8b8f611d` 2026-10-28 Regulus CiberSec, +139 -15: Dragon Net Online: texto novo fora do quadro (fim da queda ao navegar rápido) e crash de textura apagada
- `6e26f543` 2026-10-28 Regulus CiberSec, +139 -15: Dragon Net Online: texto novo fora do quadro (fim da queda ao navegar rápido) e crash de textura apagada
- `8e7dac04` 2026-10-28 Regulus CiberSec, +113 -30: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `4dc3964c` 2026-10-27 Regulus CiberSec, +72 -56: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +141 -1: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +72 -56: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `5d3eea11` 2026-10-27 Regulus CiberSec, +141 -1: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `47b4d577` 2026-10-26 Tag Team Mod contributors, +32 -2: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `2d6d0531` 2026-10-25 dfdevx2, +10 -108: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +109 -1: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `101de958` 2026-10-24 Tag Team Mod contributors, +35 -3: Add native Tag Team modes
- `52b675c3` 2026-10-25 z3xox, +8 -3: A 256-colour table is reused as a texture only when every entry is the same, not by its hash alone. The object glow's table and the glare's ramp differ only in alpha, and with teen Gohan's third costume on Namek their hashes were the same: the fighters' glow was drawn through the glare's ramp, which marks every pixel, and the picture went white (seen by a player; glow 100% and 200%). The hash now folds in the top bits too; the copies of the tables are kept apart from the slots the lookup goes through. Unchanged: every screenshot of the recorded fights, input to picture 33.36 ms, pacing, uncapped speed (1x, four runs each against port: 237 / 333 fps, port 238 / 334)
- `4d11e2bf` 2026-10-24 z3xox, +35 -4: A frame takes in at most 4 MB of a texture pack's pictures (BT3_PACK_FRAME_MB; 0: no limit); past that a texture keeps its original for a frame or two more, as one whose file is still being read does. With a large pack (18 597 DXT5 pictures with their smaller copies) a fight's new effects brought up to 34 MB into one frame. In the recorded fights at 8x with that pack read from the disk: the worst upload 7.9 ms instead of 21.7, the worst hand-over to the driver 7.6 instead of 18.1, drawings over 15 ms 10 instead of 22, 1% low 25.1 fps instead of 24.0
- `01002cc3` 2026-10-24 z3xox, +4 -2: BT3_DRAW_LOG splits the list's time further: vertex programs (their draws in it), pictures written into GS memory, and all texture lookups. In the heaviest recorded frame (116 new textures): 12.1 ms in vertex programs, 13.7 in lookups, 0.4 writing into GS memory
- `5b7189b5` 2026-10-24 z3xox, +4 -4: Spare images for new textures, made ahead in frames with time to spare: a fight's first big effects asked for up to 122 new images in one frame, 28 ms of it to make them on an NVIDIA card. Of each size up to 256 x 256 32 kept once it is asked for, of larger ones the most one frame has asked for and 4 more, 96 MB at most; made at the end of a frame drawn in under 10 ms, 1 ms at most. In the recorded fights at 8x: the heaviest frame 47.5 ms instead of 86.9 (105 spares, 0.2 ms making images), the next 26.8 instead of 34.6. BT3_TEX_POOL=0: none. BT3_DRAW_LOG also times making images and counts the spares taken
- `f6715c5c` 2026-10-24 z3xox, +1 -0: A new texture's label hash (byte by byte over all of it) is worked out once for both checks that need it, the battle text one and MenuText_Render with a menu font chosen, instead of in each. New-texture work in the recorded two fights: 254 ms in all instead of 312, 13.1 ms in its worst frame instead of 15.4 (629 and 30.7 before this branch); every texture the same byte for byte, the menu font's labels included
- `341f07da` 2026-10-24 z3xox, +97 -7: A large new texture (16384 pixels or more) is decoded by worker threads together with the caller, row by row, while the caller waits: they only read GS memory. Decoding new textures in the recorded two fights: 312 ms in all instead of 398, 15.4 ms in its worst frame instead of 20.4; every texture the same byte for byte. BT3_TEX_THREADS=<n> (0: none; by default the logical processors less two, at most 7)
- `e2e40089` 2026-10-24 z3xox, +19 -13: A palette texture's colours are read and expanded once per texture, not again for every pixel: in a recorded session of two fights with ultimates, decoding new textures took 398 ms in all instead of 629 and 20.4 ms in its worst frame instead of 30.7 (the frames where a fight's first effects come, which a slower machine fills past its 33 ms and leaves pictures out). Every decoded texture the same, byte for byte (BT3_TEX_DUMP, frame skipping off)
- `9ecce978` 2026-10-23 dfdevx2, +17 -1: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `01cf18d0` 2026-10-23 z3xox, +4 -4: BT3_DRAW_LOG also counts each picture's graphics-chip images made and released and the bytes uploaded: whether a slow hand-over to the driver follows new images (a pool would help) or big uploads (spreading them would)
- `071dc055` 2026-10-23 z3xox, +14 -0: BT3_DRAW_LOG=<ms>: each picture whose drawing took longer, timed where it is drawn (the render thread or the game's), from the start of its list to the driver, in parts: walking the list (new textures decoded, pipelines made), uploads, issuing the draws, waiting for the screen's buffer, showing, handing over. The per-second lines could not tell what made 1 drawing in 10 take 10 to 20 ms instead of 2 on a laptop, and with the render thread they see none of the drawing
- ... e mais 95 (`git log -- port/src/gs/gs_draw.c`)

## `port/src/gs/gs_draw.h` ([renderer-gs](renderer-gs.md))

- `91be0b33` 2026-10-31 dfdevx2, +1 -0: BT4: the frame buffer being drawn is the picture, not a work buffer: no noise over the far scenery
- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -1: Add native Tag Team modes
- `968794bf` 2026-10-21 z3xox, +1 -1: Glow: its pages at the PS2's resolution only while the glow chain runs (GfxPost_DrawGlow marks its start and end in the display list: markers 0x20 and 0x21); the fighters' shadow picture, the lens flare's, the heat haze's and the stage blur's use of the same pages at the internal resolution again; F1 > Effects > Sharp stage blur (on by default): those passes between work buffers sampled at the internal resolution as before, off as the PS2 samples them
- `14c996fd` 2026-10-21 z3xox, +3 -0: Glow: its work buffers (pages 0x150, 0x170, 0x1F6, 0x1F8) drawn at the PS2's own resolution, one texel per GS pixel in the corner of their textures (positions and scissor not scaled up, texture coordinates scaled down; the vertex programs' draws into them through the uniforms' target scale, after the in-between pictures' camera): the filter is the GS's and those passes have SCALE x SCALE fewer pixels; a fight on an integrated chip at 2x 128 pictures a second (0.1.37.1: 124), at 6x 16 (14 to 15); BT3_GLOW_NATIVE=0 as before
- `618ee826` 2026-10-21 z3xox, +1 -1: Glow and glare: their own strengths in F1 > Effects (the glow of characters and objects, the sky's glare); the chain the game runs for both is told apart by its tint back up into the 128 x 128 page (the glare's grey has alpha 0); a saved glow strength is the glare's too until it has its own
- `1867006b` 2026-10-17 dfdevx2, +1 -0: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +4 -0: Dragon Rage 0.6.0 [skip ci]
- `de981ad7` 2026-10-12 z3xox, +1 -0: Performance: the HUD's alpha copies cover only what their draws read, not the whole 1024 x 1024 target (a fifth more pictures a second on integrated graphics)
- `c39c5c25` 2026-10-11 z3xox, +1 -0: In-between pictures: the wall at the edge of the map is drawn from the game's own patch, a surface keeps its identity when its texture changes (a waterfall jumped a step to the side); what the game transforms itself leads by two depth steps (the fall's filled part was a speckle in motion)
- `161a7632` 2026-10-11 z3xox, +2 -0: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)
- `928b2413` 2026-10-11 dfdevx2, +4 -0: WIP: patch 1 and touch artwork
- `195402b4` 2026-10-10 z3xox, +2 -0: The game says when its data is not the original disc's: a background check of every data file against a table of checksums, then the window title, the log, a notice and the crash report carry it
- `8ed3d38a` 2026-10-10 z3xox, +1 -0: The cross-fade keeps the old shot at the resolution it was drawn in (a copy on the card; read back and uploaded it was the PS2's 512 x 448, blocky)
- `021fc2fc` 2026-10-10 z3xox, +1 -0: The cross-fade between the shots of a fight's intro: the game's read-back of the frame is answered (software renderer and both GPU back ends)
- `5ad3f1b9` 2026-10-09 Claude, +7 -5: GPU: 512x512 render targets (a quarter of the memory and tile traffic)
- `5ddecbfd` 2026-10-08 z3xox, +6 -0: Drawing: strips as indexed triangles, command data used in place, getenv remembered per place, a faster page hash
- `684d0c16` 2026-10-06 z3xox, +12 -2: Texture packs: wire the replacements into the split renderer (Vulkan and OpenGL)
- `75bf61a4` 2026-10-06 z3xox, +142 -0: GS: an OpenGL back end beside the Vulkan one

## `port/src/gs/gs_gl.c` ([renderer-gs](renderer-gs.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +2 -1: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `c2b26ba6` 2026-10-24 dfdevx2, +63 -13: GS: depthless passes, shadow refs, Adreno fixes
- `9ecce978` 2026-10-23 dfdevx2, +98 -0: ARMSX2 lab: drivers, BT4 16:9 and 60 fps, ADPF, GL persistent buffers
- `5707f85b` 2026-10-22 dfdevx2, +1 -1: Fixes from logs, touch pause menu, BT4 menu
- `6dbb460b` 2026-10-21 z3xox, +5 -3: Glow: the GS's 1x sampling of a work buffer in a shader of its own (gswb.frag, pipeline key bit 25), only for draws from one work buffer into another, one read per GS texel at 2x (in the shader of every draw and on the passes over the whole picture it took a fight at 2x on an integrated chip from 124 pictures a second to 90; now 118, 122 without it: BT3_GLOW_BLOCK=0)
- `f40dc865` 2026-10-20 z3xox, +2 -1: Outline: the same thickness at any aspect ratio (widescreen stretches the PS2's 512 pixels across the picture, and the line one PS2 pixel wide on the left and right of every shape grew with it: 8.2 screen pixels at 21:9 against 4.7 at 4:3; across, the width is narrowed by the stretch and measured in texels, a fraction drawn as a lighter last texel)
- `2d8a5a72` 2026-10-18 dfdevx2, +5 -1: Dragon Rage 0.7.3
- `70441e5f` 2026-10-17 dfdevx2, +8 -3: Dragon Rage 0.7.1
- `1867006b` 2026-10-17 dfdevx2, +39 -5: Dragon Rage 0.7.0
- `9af92af5` 2026-10-17 z3xox, +5 -1: Blending: a source alpha above 0x80 counts as on the PS2 (the Solar Flare's whiteout)
- `7b99f8ad` 2026-10-15 dfdevx2, +162 -19: Dragon Rage 0.6.0 [skip ci]
- `f2b014d9` 2026-10-14 victorgonl, +1 -1: Show GPU being used
- `7bb64094` 2026-10-14 victorgonl, +43 -6: Fix character shadow on view model
- `df22e6f8` 2026-10-14 z3xox, +0 -8: White dots, done properly: the depth the shader writes is never above the depth it came from (the step below where the division rounds up); the depth bias of the first try is gone (it let layers one step behind show through)
- `2080bfeb` 2026-10-14 z3xox, +8 -0: White dots on shiny characters (Perfect Cell): a layer drawn without the shader depth lost its depth test by the last bit of a float; those draws get a depth bias of two such bits
- `0aa94971` 2026-10-11 piapenia, +1 -0: Screen reader: the menus speak through Prism, off unless prism.dll is beside the program
- `de981ad7` 2026-10-12 z3xox, +4 -1: Performance: the HUD's alpha copies cover only what their draws read, not the whole 1024 x 1024 target (a fifth more pictures a second on integrated graphics)
- `40f5f520` 2026-10-12 z3xox, +2 -1: Menus in 4:3: an option to show the menus at their own shape in a wider picture instead of stretched (fights unchanged)
- `e16805d3` 2026-10-12 z3xox, +8 -3: Performance: the per-pixel depth of 0.1.25 only where it changes something (it cost a quarter of the frame rate on integrated graphics)
- `bbd4c2b5` 2026-10-11 dfdevx2, +215 -118: Dragon Rage 0.5.0 [skip ci]
- `c39c5c25` 2026-10-11 z3xox, +1 -1: In-between pictures: the wall at the edge of the map is drawn from the game's own patch, a surface keeps its identity when its texture changes (a waterfall jumped a step to the side); what the game transforms itself leads by two depth steps (the fall's filled part was a speckle in motion)
- `161a7632` 2026-10-11 z3xox, +15 -4: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)
- `928b2413` 2026-10-11 dfdevx2, +54 -1: WIP: patch 1 and touch artwork
- `80e0dd42` 2026-10-11 z3xox, +1 -1: The settings window opens from a controller: START + SELECT, and a key and a button of its own in the Controls list; the window is worked with the controller and keeps it from the game while open
- `195402b4` 2026-10-10 z3xox, +1 -0: The game says when its data is not the original disc's: a background check of every data file against a table of checksums, then the window title, the log, a notice and the crash report carry it
- ... e mais 17 (`git log -- port/src/gs/gs_gl.c`)

## `port/src/gs/gs_gpu.c` ([renderer-gs](renderer-gs.md))

- `f0bd6511` 2026-10-31 dfdevx2, +4 -0: Requests: 2 s polls, backoff on 429, none in the background; the relay socket checks the host
- `47b4d577` 2026-10-26 Tag Team Mod contributors, +2 -3: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `a7e5238c` 2026-10-26 z3xox, +9 -2: Spectators: a third game (and up to six) can watch a match, by the host's address. A session is computed from its first blank out of nothing but the two players' input, so the host sends a spectator the match's set-up (rules and speed, seed, both players' options and names) and then the input of both players for every blank, as it has it confirmed (never a guess): the spectator's copy computes the same match, character select, battles and rematches included, about a second behind (it waits for a second's worth when it runs out, and runs blanks without picture when it is more than two seconds behind). The host keeps the whole match's input (36 bytes a blank, a byte for 128 unchanged blanks in the packets), so a spectator can come in mid-match and catch up from the start. Nothing waits for spectators: the host sends what is due with its looks at the socket, at most two catch-up packets every 16 ms between them all, and forgets one that is silent for 8 s or still a minute behind after a minute. The host's check values travel with the input and the spectator compares them with its own; one that comes apart says so and follows the match again from the start, once. New packet types 20 to 28, which a 0.1.38.7 ignores: an old host never answers, and the spectator says what that may mean after 10 s; nothing between the two players changed (NET_VERSION 5). The view is the spectator's: player 1's, player 2's, or both halves as the game draws its two-player battle (V or R1; it is drawing only: the fight's values stay the same). In the online window: "Watch the match" on the Join tab, "Let others watch the match" on the Host tab (setting net_spectators); the players see who watches, the spectator a line with the match, the view and the keys, and F1 has Change view / Stop watching. Tests: BT3_LOBBY=spectate:<address>:<port>, BT3_NET_SPECTATE with BT3_NET_SESSION, BT3_LOBBY_AT, BT3_SPEC_VIEWS, BT3_SPEC_LEAVE, BT3_NET_SAVES (the session's save folder), BT3_SHOT_DIR (screenshots of several copies, named by blank too). A program started as the session (tests) now exits when it ends instead of loading a state it never kept.
- `2d6d0531` 2026-10-25 dfdevx2, +1 -73: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +158 -0: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `79ce6851` 2026-10-25 dfdevx2, +6 -3: Game: hide the system bars before the surface is made
- `23fa0efe` 2026-10-25 dfdevx2, +11 -0: GS: no window left behind when Vulkan cannot start
- `101de958` 2026-10-24 Tag Team Mod contributors, +35 -13: Add native Tag Team modes
- `c2b26ba6` 2026-10-24 dfdevx2, +150 -23: GS: depthless passes, shadow refs, Adreno fixes
- `b8956f98` 2026-10-24 z3xox, +46 -24: A frame's textures go up through one upload buffer, each level at its own offset, mapped once, instead of a buffer made, mapped and let go for every level (a pack's picture has up to 11). With a large texture pack (18 597 DXT5 pictures) read from the disk, in the recorded fights at 8x: pictures over 36 ms 0.2% instead of 2.4% (3.1% in 0.1.38.5), 1% low 27.9 fps instead of 25.1 (24.0), as without a pack (27.7); the slowest 5% of drawings 3.4 ms instead of 6.9. The buffer stays and grows; SDL hands a fresh one while the last frame's is still read (cycle). Every screenshot the same
- `14c46704` 2026-10-24 z3xox, +58 -4: Spare render targets, made in quiet frames before the game needs them: a target is the PS2 screen's size times the internal resolution in three images (about 130 MB at 8x) and took 6.5 ms to make; a fight's first big effect asked for four new ones in one frame. As many as a session has needed at most and are not there yet (the setting gpu_targets_peak, written when it rises), 4 at most, one a frame after a frame drawn in under 6 ms. In the recorded fights at 8x: the heaviest frame 13.9 ms instead of 40.4; on a machine made slower by 15 ms a frame 30 pictures skipped instead of 43, 1% low 27.7 fps instead of 26.4. Every screenshot the same. BT3_TARGET_SPARES=0: none
- `01002cc3` 2026-10-24 z3xox, +3 -3: BT3_DRAW_LOG splits the list's time further: vertex programs (their draws in it), pictures written into GS memory, and all texture lookups. In the heaviest recorded frame (116 new textures): 12.1 ms in vertex programs, 13.7 in lookups, 0.4 writing into GS memory
- `5b7189b5` 2026-10-24 z3xox, +102 -5: Spare images for new textures, made ahead in frames with time to spare: a fight's first big effects asked for up to 122 new images in one frame, 28 ms of it to make them on an NVIDIA card. Of each size up to 256 x 256 32 kept once it is asked for, of larger ones the most one frame has asked for and 4 more, 96 MB at most; made at the end of a frame drawn in under 10 ms, 1 ms at most. In the recorded fights at 8x: the heaviest frame 47.5 ms instead of 86.9 (105 spares, 0.2 ms making images), the next 26.8 instead of 34.6. BT3_TEX_POOL=0: none. BT3_DRAW_LOG also times making images and counts the spares taken
- `01cf18d0` 2026-10-23 z3xox, +16 -4: BT3_DRAW_LOG also counts each picture's graphics-chip images made and released and the bytes uploaded: whether a slow hand-over to the driver follows new images (a pool would help) or big uploads (spreading them would)
- `071dc055` 2026-10-23 z3xox, +26 -0: BT3_DRAW_LOG=<ms>: each picture whose drawing took longer, timed where it is drawn (the render thread or the game's), from the start of its list to the driver, in parts: walking the list (new textures decoded, pipelines made), uploads, issuing the draws, waiting for the screen's buffer, showing, handing over. The per-second lines could not tell what made 1 drawing in 10 take 10 to 20 ms instead of 2 on a laptop, and with the render thread they see none of the drawing
- `b500f3de` 2026-10-23 z3xox, +9 -1: The log names the graphics chip and its driver beside the renderer (as F1 > Video does): a laptop's logs did not say whether its Intel chip or its NVIDIA one drew the game
- `e50d6137` 2026-10-22 z3xox, +51 -1: BT3_LAG_LOG measures more of the way from a press to the screen: "lag P" with how old a press is when the game first reads it (an event watch notes each button or key press's own time), and "lag G" when the graphics chip has finished a frame handed over (its fence, waited for by a thread of its own). With the render thread and without it, offscreen at 2x on an integrated chip: pad read to its picture 33.36 ms, the chip done 7.3 ms later, the same in both
- `041cc5c4` 2026-10-22 z3xox, +68 -0: native-fx: measuring what each kind of pass costs. BT3_FX_SKIP=<bits> leaves out the draws into other buffers (1), the effects put back onto the picture (2), the vertex programs' 3D (4), the GS-vertex sprites (8); BT3_FX_STAT=<n> counts them. Desert - Noon, demo battle, integrated Radeon: at 1x 3.7 ms a frame, of it 3D 1.0, everything else 0.3 (the rest is the CPU: game code 1.1, the display list 2.5, of it issuing the draws 1.1); at 3x 19.9 ms, sprites 6.6, 3D 4.3, other buffers 2.3, effects put back 2.2
- `5707f85b` 2026-10-22 dfdevx2, +1 -1: Fixes from logs, touch pause menu, BT4 menu
- `cd8a3372` 2026-10-21 z3xox, +131 -6: The alpha bytes are written only where something reads them later. A draw writes the GS's alpha byte into its own buffer next to the colour; it is read only through the copies a native 4 (outline, see-through tint) and a native 5 (destination alpha test) take, each of a known part of the buffer. aux_plan walks the frame's list backwards and gives each draw the rectangle where its byte is still read; a draw outside it is drawn with a pipeline that leaves the byte alone (key bit 26), one partly inside is split by scissor. An ultimate's flash, rays and aura (some fifty additive layers over the whole picture in the heaviest frames) come after the last copy of the whole screen. Only for a buffer whose frame begins with the clear, this frame and the one before. The same bit for bit (every 150 frames at 2x, every frame of an ultimate at 4x, Smooth motion at 3x). A fight full of ultimates on an integrated chip: 4x 34.1 pictures a second (31.8), 6x 14.7 (13.9), the heaviest stretches 13% faster. BT3_AUX_ALL=1: every draw writes it, as before
- `0ee1fc74` 2026-10-21 z3xox, +45 -1: The copy of the alpha bytes for the table passes, the outline and the see-through tint: only the part those passes read before the next copy (their scissors, 8 texels and two PS2 pixels more on every side), not the whole 2048 x 2048 buffer at 2x. A fight on an integrated chip at 2x 145 pictures a second (142), at 6x 18.4 (17.8); the same bit for bit. BT3_AUX_COPY_WHOLE=1: the whole buffer
- `89ff6558` 2026-10-21 z3xox, +99 -0: The frame's clear as the render pass's: the game's untextured black rectangle over the whole screen, written at one depth whatever the test (the first draw of a fight's picture), was a pass of colour, alpha byte and depth through the shader that writes its own depth; the pass now clears to the same values (the depth rounded as gs.frag rounds it). Only a clear of the whole screen (a view of the split screen keeps its own). A fight on an integrated chip at 2x 142 pictures a second (138); colour, alpha bytes and depth on the screen the same bit for bit, every 300 ticks, odd ticks, smooth motion, 6x. BT3_FAST_CLEAR=0: drawn as before
- `6dbb460b` 2026-10-21 z3xox, +4 -3: Glow: the GS's 1x sampling of a work buffer in a shader of its own (gswb.frag, pipeline key bit 25), only for draws from one work buffer into another, one read per GS texel at 2x (in the shader of every draw and on the passes over the whole picture it took a fight at 2x on an integrated chip from 124 pictures a second to 90; now 118, 122 without it: BT3_GLOW_BLOCK=0)
- `f40dc865` 2026-10-20 z3xox, +2 -1: Outline: the same thickness at any aspect ratio (widescreen stretches the PS2's 512 pixels across the picture, and the line one PS2 pixel wide on the left and right of every shape grew with it: 8.2 screen pixels at 21:9 against 4.7 at 4:3; across, the width is narrowed by the stretch and measured in texels, a fraction drawn as a lighter last texel)
- `2d8a5a72` 2026-10-18 dfdevx2, +6 -2: Dragon Rage 0.7.3
- ... e mais 72 (`git log -- port/src/gs/gs_gpu.c`)

## `port/src/gs/gs_input.c` ([touch-input](touch-input.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +3 -2: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `d4842ec9` 2026-10-31 dfdevx2, +14 -0: Pad mappings: read them where they are written on Android
- `d54095dd` 2026-10-29 Regulus CiberSec, +29 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `8e7dac04` 2026-10-28 Regulus CiberSec, +109 -0: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `4dc3964c` 2026-10-27 Regulus CiberSec, +9 -2: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +9 -2: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `6b69eb72` 2026-10-28 z3xox, +1 -1: 0.1.39: lobbies replace rooms. Tenkaichi Budokai Online has no Create Room: the list is Quick Match, Ranked Match, Lobby, Join (Join Room renamed, the one place for a code), the SOON ones and Profile. Lobby opens New Lobby: Open Lobby (a code) or Host by Address (Port_LobbyCreate on the port net_port, L1 / R1 by a hundred); the lobby's page shows the port and this PC's LAN address where a code would be, square copies "address:port". A new lobby starts with the rules its owner last set on a lobby's page (Tb_LobbyKeepRules, in Create Room's settings: battle with DP 10 / 15 / 20, time, game speed, rollback, delay), so nothing that could be chosen is lost. No Watch on Join or Join by Address: a spectator joins the lobby; JOIN on Join by Address is Port_LobbyJoin and opens the lobby's page at once. Dragon Net Battle (F1 > Online > Advanced) opens and joins lobbies too (Host: a lobby with a code or on its port and the tab's rules; Join: a code or an address), no room of two and no Watch; F1 > Online loses "Let others watch your matches" (rooms only). PORT_RELEASE 139, NET_VERSION 6: a 0.1.38 game is never paired (the service's queues and codes are kept apart by it); by address a member joining a typed address that has not answered for two seconds also sends a spectator's greeting too short to be one, which a 0.1.38 host of two answers with SPEC_NO_VERSION ("The game at that address is a different version"), and a lobby's owner refusing another version notes it once a minute. FP_PEER_REQUIRED is 1: a peer, spectator or member without a fingerprint is refused. Update checks: v0.1.39 and v0.1.39.1 newer than 0.1.38.9 (44 checks).
- `a3cf1192` 2026-10-28 z3xox, +32 -2: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `cf2c0fb3` 2026-10-27 Regulus CiberSec, +2 -0: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `a96bd6f8` 2026-10-27 Regulus CiberSec, +2 -0: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `47b4d577` 2026-10-26 Tag Team Mod contributors, +19 -4: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `a7e5238c` 2026-10-26 z3xox, +2 -1: Spectators: a third game (and up to six) can watch a match, by the host's address. A session is computed from its first blank out of nothing but the two players' input, so the host sends a spectator the match's set-up (rules and speed, seed, both players' options and names) and then the input of both players for every blank, as it has it confirmed (never a guess): the spectator's copy computes the same match, character select, battles and rematches included, about a second behind (it waits for a second's worth when it runs out, and runs blanks without picture when it is more than two seconds behind). The host keeps the whole match's input (36 bytes a blank, a byte for 128 unchanged blanks in the packets), so a spectator can come in mid-match and catch up from the start. Nothing waits for spectators: the host sends what is due with its looks at the socket, at most two catch-up packets every 16 ms between them all, and forgets one that is silent for 8 s or still a minute behind after a minute. The host's check values travel with the input and the spectator compares them with its own; one that comes apart says so and follows the match again from the start, once. New packet types 20 to 28, which a 0.1.38.7 ignores: an old host never answers, and the spectator says what that may mean after 10 s; nothing between the two players changed (NET_VERSION 5). The view is the spectator's: player 1's, player 2's, or both halves as the game draws its two-player battle (V or R1; it is drawing only: the fight's values stay the same). In the online window: "Watch the match" on the Join tab, "Let others watch the match" on the Host tab (setting net_spectators); the players see who watches, the spectator a line with the match, the view and the keys, and F1 has Change view / Stop watching. Tests: BT3_LOBBY=spectate:<address>:<port>, BT3_NET_SPECTATE with BT3_NET_SESSION, BT3_LOBBY_AT, BT3_SPEC_VIEWS, BT3_SPEC_LEAVE, BT3_NET_SAVES (the session's save folder), BT3_SHOT_DIR (screenshots of several copies, named by blank too). A program started as the session (tests) now exits when it ends instead of loading a state it never kept.
- `101de958` 2026-10-24 Tag Team Mod contributors, +203 -22: Add native Tag Team modes
- `1e55403b` 2026-10-24 z3xox, +1 -0: Four settings are written to the file when they change: the Square clash's worth per press (from a player: it did not keep after closing the game), Low input lag, the game speed (when its slider is let go, and Reset) and Music follows game speed. They were only marked changed, and kept only when another setting was written after them in the same run
- `c9e77091` 2026-10-23 dfdevx2, +4 -1: BT4: ARMSX2 renderer (libbt4gsx)
- `b151618c` 2026-10-22 z3xox, +5 -1: BT3_LAG_LOG: a press's age is logged once, at the first read that has it. A direction from a stick sets a new bit without a press of its own, and was given the age of the last button press (28 of 243 lines over 200 ms in a real fight)
- `e50d6137` 2026-10-22 z3xox, +31 -0: BT3_LAG_LOG measures more of the way from a press to the screen: "lag P" with how old a press is when the game first reads it (an event watch notes each button or key press's own time), and "lag G" when the graphics chip has finished a frame handed over (its fence, waited for by a thread of its own). With the render thread and without it, offscreen at 2x on an integrated chip: pad read to its picture 33.36 ms, the chip done 7.3 ms later, the same in both
- `153b847c` 2026-10-22 z3xox, +9 -0: native-fx: the render thread owns the window's events and becomes a setting. Presenting has to be done by the thread that made the window, and that is the render thread; two threads taking the window's events is not safe. So the render thread takes them (PortInput_Pump) every millisecond it waits, for the game's next list or for a Smooth-motion picture's time, and the game's thread no longer does (PortInput_Fresh); the state of the controllers and keys it reads is at most a millisecond old. F1 > Video, "Render on a second processor core" (setting render_thread, off by default, after a restart; BT3_GS_THREAD=0/1 over it); never on macOS, not with OpenGL chosen or with two logical processors or fewer. If Vulkan does not start on the thread and OpenGL does, the game's own reads of the picture and the held picture online step aside (OpenGL's context is the thread's). The recording the same with the thread (28 of 28, the game values), Smooth motion as even (spread 0.57 ms); the Windows build runs it (Wine)
- `c46e640d` 2026-10-20 z3xox, +66 -7: Vibration: a controller that keeps the game waiting for its motor command (some generic pads through DirectInput, some Bluetooth links: a player's game froze for seconds whenever it vibrated) is sent fewer, one per ten times its longest call, and after two of 50 ms none, with a notice; the test controller can be slow (BT3_RUMBLE_SLOW) and while it is attached the real ones are left out
- `2d8a5a72` 2026-10-18 dfdevx2, +6 -6: Dragon Rage 0.7.3
- `c934aab4` 2026-10-17 z3xox, +6 -6: Clashes, the Square option: a press can count 2.25 (9 quarters), and 2.25 is what it counts unless another is chosen (the stick's pace is nearer: tested by the user)
- `7b99f8ad` 2026-10-15 dfdevx2, +546 -15: Dragon Rage 0.6.0 [skip ci]
- `ce39ef7a` 2026-10-14 z3xox, +42 -1: TBO: a PASTE key on Join Room: the room code of the copied text in place of what is typed
- `76fa4afb` 2026-10-14 z3xox, +113 -2: Tenkaichi Budokai Online: letters from the font list, the keyboard on its keypads, Copy Code
- `313084b9` 2026-10-13 jllacchua, +3 -2: UI: improve online chat aesthetic, tab toggle, controls binding, and mouse copy support
- ... e mais 16 (`git log -- port/src/gs/gs_input.c`)

## `port/src/gs/gs_internal.h` ([renderer-gs](renderer-gs.md))

- `153b847c` 2026-10-22 z3xox, +1 -0: native-fx: the render thread owns the window's events and becomes a setting. Presenting has to be done by the thread that made the window, and that is the render thread; two threads taking the window's events is not safe. So the render thread takes them (PortInput_Pump) every millisecond it waits, for the game's next list or for a Smooth-motion picture's time, and the game's thread no longer does (PortInput_Fresh); the state of the controllers and keys it reads is at most a millisecond old. F1 > Video, "Render on a second processor core" (setting render_thread, off by default, after a restart; BT3_GS_THREAD=0/1 over it); never on macOS, not with OpenGL chosen or with two logical processors or fewer. If Vulkan does not start on the thread and OpenGL does, the game's own reads of the picture and the held picture online step aside (OpenGL's context is the thread's). The recording the same with the thread (28 of 28, the game values), Smooth motion as even (spread 0.57 ms); the Windows build runs it (Wine)
- `fb85a2bc` 2026-10-22 z3xox, +7 -0: native-fx: the render thread with the rest of the renderer and with Smooth motion. Gs_RenderIdle: the game's thread waits for the thread before it touches what the thread uses (state keeping, the cross-fade's read of the picture, which no longer returns black with the thread, the picture held while waiting online); not on the thread itself (the settings window calls one of them from there). Smooth motion: the tick's pictures after the first are the thread's (GsGpu_InterpRun), each when due, while the game computes the next tick; a list handed over early has the real picture shown at once (GsGpu_InterpHurry, for the list being run only). The wall patches the game hands over are written and read under a lock. BT3_PRESENT_LOG writes whole lines. With the thread: the recording the same (28 of 28 shots, the game values), Smooth motion at 60 as even as without it (16.68 ms, spread 0.60 against 0.61), at 120 too; one shot of 28 differs by 24 pixels on the edge of an animated effect (the real picture without the thread was drawn after the next tick's textures had come)
- `7b99f8ad` 2026-10-15 dfdevx2, +4 -0: Dragon Rage 0.6.0 [skip ci]
- `074f44ca` 2026-10-11 z3xox, +4 -0: Frame skipping: a tick's picture is left out when the game is behind its grid, instead of the game slowing down (setting frame_skip, on by default)
- `5ddecbfd` 2026-10-08 z3xox, +3 -1: Drawing: strips as indexed triangles, command data used in place, getenv remembered per place, a faster page hash
- `6130d57b` 2026-10-07 z3xox, +6 -0: Frames that are only re-run draw nothing, show nothing and start no sound (gPortResim); the state test's undone pass runs that way and still leaves the same state and picture
- `04964a3d` 2026-10-06 z3xox, +2 -0: Texture packs: PCSX2-named DDS replacements from the textures folder, with a switch in the settings window
- `f7811048` 2026-10-06 z3xox, +9 -0: Windows speed: settings lookups remembered (getenv cost about half of a frame there); frame pacing and timing statistic fixed
- `301c8a7b` 2026-10-06 z3xox, +1 -0: Port: destination alpha test (TEST.DATE) and FBA, for the masks of the HUD bars

## `port/src/gs/gs_sony.c` ([renderer-gs](renderer-gs.md))

- `91be0b33` 2026-10-31 dfdevx2, +1 -0: BT4: the frame buffer being drawn is the picture, not a work buffer: no noise over the far scenery
- `7b99f8ad` 2026-10-15 dfdevx2, +2 -1: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +2 -1: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal

## `port/src/gs/gs_texpack.c` ([renderer-gs](renderer-gs.md))

- `d6467b24` 2026-10-25 dfdevx2, +18 -2: Texture packs: used on Android, in both games; more zips read
- `11059a6d` 2026-10-21 z3xox, +159 -19: Texture pack files are read by a thread of their own and kept in memory. A replacement was read from disk inside the frame that first used it: 30 to 150 ms when an ultimate's effects first appear, and the same file again every time the game uploaded its texture again (671 files read 2924 times in a session of two fights), which on a machine whose memory does not keep an 11 GB pack cached is a disk read inside a frame every time (reported: 60 to 35 pictures a second in ultimates with the pack, 60 without). Now TexPack_Get queues the file for the reader thread and the original texture is drawn until it is in, the way PCSX2 loads them asynchronously; the texture is made again from the replacement at its first use in a frame, and the one it replaces is released four frames later (Smooth motion still draws the last frame). What was read stays in memory up to 1 GB (BT3_PACK_MB), the least recently used files leave first. With the pack's files out of the OS cache, the whole recording: 2 slow frames, the longest 51 ms, both where the game makes 38 and 114 textures at once (the menu, a fight's first frame) (5, the longest 158 ms; a first run also had 35 and 40 ms inside ultimates). BT3_PACK_SYNC=1: read inside the frame, as before; the same picture bit for bit (28 shots every 150 frames).
- `bbd4c2b5` 2026-10-11 dfdevx2, +127 -0: Dragon Rage 0.5.0 [skip ci]
- `90b6aa80` 2026-10-06 z3xox, +70 -4: Texture packs: PNG files too
- `04964a3d` 2026-10-06 z3xox, +303 -0: Texture packs: PCSX2-named DDS replacements from the textures folder, with a switch in the settings window

## `port/src/gs/gs_texpack.h` ([renderer-gs](renderer-gs.md))

- `11059a6d` 2026-10-21 z3xox, +8 -6: Texture pack files are read by a thread of their own and kept in memory. A replacement was read from disk inside the frame that first used it: 30 to 150 ms when an ultimate's effects first appear, and the same file again every time the game uploaded its texture again (671 files read 2924 times in a session of two fights), which on a machine whose memory does not keep an 11 GB pack cached is a disk read inside a frame every time (reported: 60 to 35 pictures a second in ultimates with the pack, 60 without). Now TexPack_Get queues the file for the reader thread and the original texture is drawn until it is in, the way PCSX2 loads them asynchronously; the texture is made again from the replacement at its first use in a frame, and the one it replaces is released four frames later (Smooth motion still draws the last frame). What was read stays in memory up to 1 GB (BT3_PACK_MB), the least recently used files leave first. With the pack's files out of the OS cache, the whole recording: 2 slow frames, the longest 51 ms, both where the game makes 38 and 114 textures at once (the menu, a fight's first frame) (5, the longest 158 ms; a first run also had 35 and 40 ms inside ultimates). BT3_PACK_SYNC=1: read inside the frame, as before; the same picture bit for bit (28 shots every 150 frames).
- `bbd4c2b5` 2026-10-11 dfdevx2, +2 -0: Dragon Rage 0.5.0 [skip ci]
- `04964a3d` 2026-10-06 z3xox, +28 -0: Texture packs: PCSX2-named DDS replacements from the textures folder, with a switch in the settings window

## `port/src/gs/gs_vu1.c` ([renderer-gs](renderer-gs.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -1: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +1 -1: Dragon Rage 0.6.0 [skip ci]
- `074f44ca` 2026-10-11 z3xox, +1 -1: Frame skipping: a tick's picture is left out when the game is behind its grid, instead of the game slowing down (setting frame_skip, on by default)
- `59e6bec5` 2026-10-07 z3xox, +7 -0: Re-run frames: vertex programs not run, vertices not collected (uploads and texture-memory sprites kept); 1.27 -> 1.13 ms
- `3fbf12d1` 2026-10-07 z3xox, +13 -0: Online session inside the running program: the state at the first blank is kept, a session puts the game back to it and leaving brings the player's own state back (no restart); GS memory kept with it; sound made to fit the state
- `41760bb6` 2026-10-06 z3xox, +39 -0: Speed: vertex unpack as a copy, table-driven texture uploads, faster page hash, float core inlined into the vector library (fight frame work more than halved)
- `69c9ff21` 2026-10-06 z3xox, +40 -0: Port: vertex program 1 (second fighter pass, e.g. Raditz's scouter lens) as a shader
- `85f9e475` 2026-10-06 z3xox, +41 -0: Port: vertex programs 7 (debris) and 8 (animated stage objects) as shaders
- `22c7ee0d` 2026-10-06 z3xox, +15 -0: Port: program 8 upload length (stages with animated objects), hashed texture cache, target reuse

## `port/src/gs/menu_labels.c` ([renderer-gs](renderer-gs.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +22 -4: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `101de958` 2026-10-24 Tag Team Mod contributors, +160 -20: Add native Tag Team modes
- `65a836e1` 2026-10-24 z3xox, +7 -4: labels.tsv v13: each label's fingerprint (64 of its pixels, MenuLabels_Quick) as its last column, and a new texture of a label's size is hashed in full only when a label of that size has its fingerprint. The hash over all of a texture's bytes, one at a time, was most of a fight's big new textures' work. New-texture work in the recorded two fights: 145 ms in all instead of 254, 8.3 ms in its worst frame instead of 13.1, none over 10 (629 and 30.7 before this branch). Every texture the same byte for byte, the menu font's labels included. An older list is made again from the disc at the next start (0.3 s), as for any new layout; a row without a fingerprint is always hashed. port/tools/menu_font.py writes v13 and keeps the fingerprints it finds in labels.tsv, by texture hash
- `de8da43b` 2026-10-18 victorgonl, +61 -0: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +656 -187: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +189 -99: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `d290443b` 2026-10-17 z3xox, +12 -4: Translations: the battle HUD and the pause menu from the European disc, and its reworded lines by position
- `aa3289ee` 2026-10-17 z3xox, +273 -0: Translations: the European disc's menu art for its languages (the painted headings, menu items, buttons and title prompt)
- `5b3f71f3` 2026-10-16 victorgonl, +285 -187: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +1911 -0: Dragon Rage 0.6.0 [skip ci]
- `440aa24f` 2026-10-15 victorgonl, +18 -17: Refine battle font outlines, shadows, and subtle digit relief
- `a103b16f` 2026-10-15 victorgonl, +211 -39: Add support for battle texts fonts
- `98d09510` 2026-10-14 z3xox, +125 -15: TBO: the main menu item follows the Menu font setting, and the three labels on its sheet again (the list hashes the sheet with the item written in)
- `c19b9004` 2026-10-14 z3xox, +143 -32: TBO: the main menu item lettered from the disc: its letters cut from the other labels (and the stage names, for the k)
- `33ffd60c` 2026-10-13 victorgonl, +121 -19: Add font style support for menu headers
- `0325bca5` 2026-10-13 z3xox, +1415 -0: Menu fonts: the game makes the list of labels itself, in a thread at start

## `port/src/gs/menu_labels.h` ([renderer-gs](renderer-gs.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -0: Add native Tag Team modes
- `65a836e1` 2026-10-24 z3xox, +18 -1: labels.tsv v13: each label's fingerprint (64 of its pixels, MenuLabels_Quick) as its last column, and a new texture of a label's size is hashed in full only when a label of that size has its fingerprint. The hash over all of a texture's bytes, one at a time, was most of a fight's big new textures' work. New-texture work in the recorded two fights: 145 ms in all instead of 254, 8.3 ms in its worst frame instead of 13.1, none over 10 (629 and 30.7 before this branch). Every texture the same byte for byte, the menu font's labels included. An older list is made again from the disc at the next start (0.3 s), as for any new layout; a row without a fingerprint is always hashed. port/tools/menu_font.py writes v13 and keeps the fingerprints it finds in labels.tsv, by texture hash
- `de8da43b` 2026-10-18 victorgonl, +6 -0: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +12 -1: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +8 -7: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `aa3289ee` 2026-10-17 z3xox, +10 -0: Translations: the European disc's menu art for its languages (the painted headings, menu items, buttons and title prompt)
- `5b3f71f3` 2026-10-16 victorgonl, +1 -1: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +36 -0: Dragon Rage 0.6.0 [skip ci]
- `a103b16f` 2026-10-15 victorgonl, +2 -2: Add support for battle texts fonts
- `98d09510` 2026-10-14 z3xox, +10 -5: TBO: the main menu item follows the Menu font setting, and the three labels on its sheet again (the list hashes the sheet with the item written in)
- `c19b9004` 2026-10-14 z3xox, +7 -0: TBO: the main menu item lettered from the disc: its letters cut from the other labels (and the stage names, for the k)
- `33ffd60c` 2026-10-13 victorgonl, +2 -3: Add font style support for menu headers
- `0325bca5` 2026-10-13 z3xox, +25 -0: Menu fonts: the game makes the list of labels itself, in a thread at start

## `port/src/gs/menu_text.c` ([renderer-gs](renderer-gs.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +14 -1: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `8b8f611d` 2026-10-28 Regulus CiberSec, +24 -4: Dragon Net Online: texto novo fora do quadro (fim da queda ao navegar rápido) e crash de textura apagada
- `6e26f543` 2026-10-28 Regulus CiberSec, +24 -4: Dragon Net Online: texto novo fora do quadro (fim da queda ao navegar rápido) e crash de textura apagada
- `101de958` 2026-10-24 Tag Team Mod contributors, +94 -0: Add native Tag Team modes
- `65a836e1` 2026-10-24 z3xox, +41 -6: labels.tsv v13: each label's fingerprint (64 of its pixels, MenuLabels_Quick) as its last column, and a new texture of a label's size is hashed in full only when a label of that size has its fingerprint. The hash over all of a texture's bytes, one at a time, was most of a fight's big new textures' work. New-texture work in the recorded two fights: 145 ms in all instead of 254, 8.3 ms in its worst frame instead of 13.1, none over 10 (629 and 30.7 before this branch). Every texture the same byte for byte, the menu font's labels included. An older list is made again from the disc at the next start (0.3 s), as for any new layout; a row without a fingerprint is always hashed. port/tools/menu_font.py writes v13 and keeps the fingerprints it finds in labels.tsv, by texture hash
- `f6715c5c` 2026-10-24 z3xox, +31 -7: A new texture's label hash (byte by byte over all of it) is worked out once for both checks that need it, the battle text one and MenuText_Render with a menu font chosen, instead of in each. New-texture work in the recorded two fights: 254 ms in all instead of 312, 13.1 ms in its worst frame instead of 15.4 (629 and 30.7 before this branch); every texture the same byte for byte, the menu font's labels included
- `c8d28cd8` 2026-10-22 victorgonl, +17 -2: Fix custom stage and song name rendering
- `109cfbe5` 2026-10-19 Renzo, +1 -1: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion
- `2d8a5a72` 2026-10-18 dfdevx2, +187 -10: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +31 -11: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `aa3289ee` 2026-10-17 z3xox, +42 -0: Translations: the European disc's menu art for its languages (the painted headings, menu items, buttons and title prompt)
- `7f8692d4` 2026-10-16 z3xox, +42 -0: Fonts found when the game is started from another folder: fonts/ next to the program, or the checkout from port/build (only Original was offered)
- `5b3f71f3` 2026-10-16 victorgonl, +83 -10: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +2135 -0: Dragon Rage 0.6.0 [skip ci]
- `7f3048e7` 2026-10-15 z3xox, +3 -1: Menu text: a row is drawn in the font it is asked for again (since the Battle font, every row but a battle one was drawn from the Menu font, which is empty on Original: the PC screens headings read outside it)
- `cc012f78` 2026-10-15 z3xox, +26 -0: Side bars: Fighter panels (a contributor's patch): in a 4:3 fight in a wide window, each side's portrait, name and form; fonts from the game's list (fonts.tsv), the portrait decoder bounded
- `cb2b930d` 2026-10-15 z3xox, +17 -14: fonts.tsv: the use column one number again (1 Menu, 2 Dialogue / UI, 4 Battle, added up; none = 3), so lines players added keep their meaning; Fira first again (the TBO screens on Original)
- `440aa24f` 2026-10-15 victorgonl, +17 -11: Refine battle font outlines, shadows, and subtle digit relief
- `053be7ac` 2026-10-15 victorgonl, +49 -3: Fix battle announcement in widescreen
- `a103b16f` 2026-10-15 victorgonl, +320 -67: Add support for battle texts fonts
- `98d09510` 2026-10-14 z3xox, +4 -23: TBO: the main menu item follows the Menu font setting, and the three labels on its sheet again (the list hashes the sheet with the item written in)
- `c19b9004` 2026-10-14 z3xox, +77 -0: TBO: the main menu item lettered from the disc: its letters cut from the other labels (and the stage names, for the k)
- `5ffd592f` 2026-10-14 z3xox, +75 -15: Tenkaichi Budokai Online: the titles in the main menu's heading style
- `76fa4afb` 2026-10-14 z3xox, +143 -0: Tenkaichi Budokai Online: letters from the font list, the keyboard on its keypads, Copy Code
- `33ffd60c` 2026-10-13 victorgonl, +121 -69: Add font style support for menu headers
- ... e mais 7 (`git log -- port/src/gs/menu_text.c`)

## `port/src/gs/menu_text.h` ([renderer-gs](renderer-gs.md))

- `f6715c5c` 2026-10-24 z3xox, +1 -0: A new texture's label hash (byte by byte over all of it) is worked out once for both checks that need it, the battle text one and MenuText_Render with a menu font chosen, instead of in each. New-texture work in the recorded two fights: 254 ms in all instead of 312, 13.1 ms in its worst frame instead of 15.4 (629 and 30.7 before this branch); every texture the same byte for byte, the menu font's labels included
- `2d8a5a72` 2026-10-18 dfdevx2, +3 -0: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +3 -0: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +79 -0: Dragon Rage 0.6.0 [skip ci]
- `cc012f78` 2026-10-15 z3xox, +4 -0: Side bars: Fighter panels (a contributor's patch): in a 4:3 fight in a wide window, each side's portrait, name and form; fonts from the game's list (fonts.tsv), the portrait decoder bounded
- `053be7ac` 2026-10-15 victorgonl, +6 -2: Fix battle announcement in widescreen
- `a103b16f` 2026-10-15 victorgonl, +8 -2: Add support for battle texts fonts
- `5ffd592f` 2026-10-14 z3xox, +6 -0: Tenkaichi Budokai Online: the titles in the main menu's heading style
- `76fa4afb` 2026-10-14 z3xox, +8 -0: Tenkaichi Budokai Online: letters from the font list, the keyboard on its keypads, Copy Code
- `0325bca5` 2026-10-13 z3xox, +7 -0: Menu fonts: the game makes the list of labels itself, in a thread at start
- `f9c3f935` 2026-10-13 z3xox, +11 -7: Menu fonts: a font file is checked before use, drawing is several times faster, "Original" is as before
- `2e93dc19` 2026-10-12 victorgonl, +12 -0: Add support for shared fonts in dialogue and UI (text not editable)
- `2950c9d2` 2026-10-12 victorgonl, +28 -0: Add native and editable menu text with selectable fonts

## `port/src/gs/menu_translation.c` ([renderer-gs](renderer-gs.md))

- `4dc3964c` 2026-10-27 Regulus CiberSec, +1 -1: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +32 -3: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +1 -1: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `5d3eea11` 2026-10-27 Regulus CiberSec, +32 -3: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `0ca19719` 2026-10-27 z3xox, +6 -1: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `cbde53ab` 2026-10-26 z3xox, +11 -2: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `18d6fd06` 2026-10-26 z3xox, +3 -2: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `455d74c7` 2026-10-26 z3xox, +2 -1: An online session begun without starting the program again no longer stops at the language question. The question at the start (no language chosen yet, or "Always ask" on) was left out of a session only when the program was started as one (BT3_NET_SESSION), not when the game was put back to its first moment from the lobby window: such a session's unseen start-up waited for an answer that nothing gives, and never reached the character select (seen in tests with a new settings file; a player with "Always ask" on would have had the same). It is now left out of every session.
- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -2: Add native Tag Team modes
- `c49f5e0f` 2026-10-23 z3xox, +124 -2: The online menus can be translated: translations/port/<language>.json takes their lines (ids tbo/..., looked up by their English), where the menu draws and measures a line (tbo_text.c), joins pieces (Ps_Copy, which also feeds the screen reader) and formats a number (Tb_Fmt). A translation that loses a line's %d parts or {name} is not used, and a lone % in a format line is refused, so a file cannot make the game misread. The talk box decodes UTF-8 (accents were two wrong letters), and the frame count has its own plural line. port/tools/tbo_strings.py makes a file for translators with the English as placeholder (303 lines; --bt4 without BT3's fighter names) and keeps what is translated when run again. Accents checked on screen in Spanish
- `de8da43b` 2026-10-18 victorgonl, +1 -55: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +919 -0: Dragon Rage 0.7.3
- `06754063` 2026-10-18 z3xox, +9 -1: Translations: a JSON string gets a buffer of its own length (Windows refused the catalogs: the memory card box in English)
- `33d6750b` 2026-10-18 z3xox, +40 -6: Translations: the bundled catalogs found next to the program too (started from another folder, a language failed to load)
- `35db3fd4` 2026-10-18 z3xox, +4 -2: Online: a line on the screen while a match starts (Connecting to your rival..., then Your rival "NAME" is here! The match begins.)
- `134975b3` 2026-10-17 victorgonl, +1 -1: Show language selection screen only if more than one language is available
- `4822563f` 2026-10-16 victorgonl, +9 -6: Support for ui bundled translations
- `71b1f103` 2026-10-16 victorgonl, +193 -67: Add support for port content text translations (need documentation)
- `6bf0ad56` 2026-10-16 victorgonl, +4 -0: Option to always show the language selector at startup like in Europe ROM
- `316b567c` 2026-10-16 victorgonl, +88 -6: Add support for dialogue and ui texts translations (no documentation yet)
- `5b3f71f3` 2026-10-16 victorgonl, +660 -0: Add support for menu translations (documentation not updated)

## `port/src/gs/menu_translation.h` ([renderer-gs](renderer-gs.md))

- `0ca19719` 2026-10-27 z3xox, +7 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `cbde53ab` 2026-10-26 z3xox, +4 -0: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `18d6fd06` 2026-10-26 z3xox, +2 -0: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `c49f5e0f` 2026-10-23 z3xox, +2 -0: The online menus can be translated: translations/port/<language>.json takes their lines (ids tbo/..., looked up by their English), where the menu draws and measures a line (tbo_text.c), joins pieces (Ps_Copy, which also feeds the screen reader) and formats a number (Tb_Fmt). A translation that loses a line's %d parts or {name} is not used, and a lone % in a format line is refused, so a file cannot make the game misread. The talk box decodes UTF-8 (accents were two wrong letters), and the frame count has its own plural line. port/tools/tbo_strings.py makes a file for translators with the English as placeholder (303 lines; --bt4 without BT3's fighter names) and keeps what is translated when run again. Accents checked on screen in Spanish
- `de8da43b` 2026-10-18 victorgonl, +0 -2: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +47 -0: Dragon Rage 0.7.3
- `35db3fd4` 2026-10-18 z3xox, +3 -0: Online: a line on the screen while a match starts (Connecting to your rival..., then Your rival "NAME" is here! The match begins.)
- `71b1f103` 2026-10-16 victorgonl, +5 -1: Add support for port content text translations (need documentation)
- `5b3f71f3` 2026-10-16 victorgonl, +40 -0: Add support for menu translations (documentation not updated)

## `port/src/gs/namefont.c` ([renderer-gs](renderer-gs.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +13 -24: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +13 -24: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +5 -4: Dragon Rage 0.6.0 [skip ci]
- `ebe07a68` 2026-10-13 z3xox, +1 -1: Names of added stages: a name row with a line of no fill no longer divides by zero
- `660c1a38` 2026-10-12 Valentin Vivaldi, +4 -3: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `d3bccc25` 2026-10-07 z3xox, +1160 -0: Names of added stages and songs in the game's own lettering, cut from the disc's name pictures at run time

## `port/src/gs/namefont.h` ([renderer-gs](renderer-gs.md))

- `d3bccc25` 2026-10-07 z3xox, +23 -0: Names of added stages and songs in the game's own lettering, cut from the disc's name pictures at run time

## `port/src/gs/net.c` ([online](online.md))

- `d747606b` 2026-10-27 Androidxxt01, +1 -0: Retry Dragon Net lobby codes on peer server
- `6b69eb72` 2026-10-28 z3xox, +14 -8: 0.1.39: lobbies replace rooms. Tenkaichi Budokai Online has no Create Room: the list is Quick Match, Ranked Match, Lobby, Join (Join Room renamed, the one place for a code), the SOON ones and Profile. Lobby opens New Lobby: Open Lobby (a code) or Host by Address (Port_LobbyCreate on the port net_port, L1 / R1 by a hundred); the lobby's page shows the port and this PC's LAN address where a code would be, square copies "address:port". A new lobby starts with the rules its owner last set on a lobby's page (Tb_LobbyKeepRules, in Create Room's settings: battle with DP 10 / 15 / 20, time, game speed, rollback, delay), so nothing that could be chosen is lost. No Watch on Join or Join by Address: a spectator joins the lobby; JOIN on Join by Address is Port_LobbyJoin and opens the lobby's page at once. Dragon Net Battle (F1 > Online > Advanced) opens and joins lobbies too (Host: a lobby with a code or on its port and the tab's rules; Join: a code or an address), no room of two and no Watch; F1 > Online loses "Let others watch your matches" (rooms only). PORT_RELEASE 139, NET_VERSION 6: a 0.1.38 game is never paired (the service's queues and codes are kept apart by it); by address a member joining a typed address that has not answered for two seconds also sends a spectator's greeting too short to be one, which a 0.1.38 host of two answers with SPEC_NO_VERSION ("The game at that address is a different version"), and a lobby's owner refusing another version notes it once a minute. FP_PEER_REQUIRED is 1: a peer, spectator or member without a fingerprint is refused. Update checks: v0.1.39 and v0.1.39.1 newer than 0.1.38.9 (44 checks).
- `41870d9c` 2026-10-27 Androidxxt01, +1 -0: Retry Dragon Net lobby codes on peer server
- `a3cf1192` 2026-10-28 z3xox, +39 -1: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `0ca19719` 2026-10-27 z3xox, +17 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `504064a3` 2026-10-27 z3xox, +3 -1: Tournaments: an entrant whose game drops gets back in. Leaving on purpose is still a walkover at once; an entrant whose game falls silent (a lost connection, a crash) keeps its bracket place for 90 s, shown on its box as DISCONNECTED and the time left, with a lobby note, and its next match waits. Joining with the lobby's code within the 90 s puts it back in its place, and the match the drop broke is played again from the start; after 90 s the walkover stands. It is known by a secret rejoin token, not its name: the owner draws 128 bits from the system's random source for each entrant and sends it to that member alone (TOKEN, a new packet type 41), which keeps it in memory and in lobby_rejoin.txt beside its settings file so that a crash or a restart keeps it; its greeting to the lobby of that code carries it. A rejoin gets a new token, so a copied old one is refused; a greeting without a valid token is an ordinary member, a spectator while the tournament is on. The owner forgets a token when its entrant is out, leaves, runs out of time, rejoins, or the tournament ends, and tells the member to forget it; the member also removes its file when the lobby closes, when it leaves, when its lobby does not answer, and at start when the file is over an hour old. The member now keeps the code it asked the service for, since the service's answer to a member carries none. STATE carries each held place's seconds left (616 bytes) and HELLO the token (80 bytes): LOBBY_VERSION 4. Tested offscreen: an entrant killed mid-match and back within 90 s (place restored, match replayed, spectators identical), one back too late, a stranger with its name, a copied old token, the token files removed, a stale file ignored.
- `79fb9436` 2026-10-27 z3xox, +202 -11: Online: whose data may meet. A quick match (strangers) needs the original disc's data, in every menu: Port_LobbyStartQuick refuses it before the matchmaking service is asked ("Quick match needs the original disc's data, and your game data are not the disc's (what differs). With friends ... you can play when both have the same data."), Dragon Net Battle's Quick match tab says so before the search and greys its button, and Tenkaichi Budokai Online's "Cannot Go Online" is now "No Quick Match", for Quick Match only. With friends (a room code, a lobby, an address, watching) the same data on both sides is enough, modified or not: Tenkaichi Budokai Online's Create Room, Join Room and Lobby no longer refuse modified data. No game connects before its own gameplay fingerprint is known: a start asked for earlier waits (lobby state 6, "Checking your game data..."; a lobby's create and join commands stay queued; net_start holds the picture), and a run that computes none (BT3_GS=none, or online play asked before the data check began) starts it (Port_DataFingerprintNeed; PortFp_Run runs once). A peer that sends no fingerprint: 0.1.37 cannot be told from 0.1.37.1 to 0.1.38.9 by its greetings (NET_VERSION 5, the same capability word; rel 137 at the service), so FP_PEER_REQUIRED decides: 0 for now (MIN_RELEASE is 137), 1 with MIN_RELEASE 138, which refuses such a player, spectator or lobby member. BT3_TEST_FPRINT now acts in plat_fprint.c; BT3_TEST_FPRINT=off sends none (as 0.1.37); BT3_TEST_FP_REQUIRED=1; BT3_UI_NET_TAB opens the online window on a tab. Tested with the local Worker: quick match original against original (BT3_LOBBY and Tenkaichi Budokai Online's screens), a claimed fingerprint refused before the service (both menus show it); by address and room code the same fake fingerprint plays and different ones are refused, after waiting for the fingerprint at the first blank; spectators and lobby members with other data refused, with the same let in; with FP_PEER_REQUIRED a player and a member without a fingerprint refused; 0.1.38.9 by address both ways. Windows builds.
- `cbde53ab` 2026-10-26 z3xox, +86 -18: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `82070dbe` 2026-10-26 z3xox, +53 -2: Online: the goodbye says why a match ended. The other game's goodbye (T_BYE) carried only 0 or the desync word, so "The other player left the match." could mean its player left or that it had stopped hearing this game (a user's quick match ended so after five fights at 300 ms and the log could not tell). It now says BYE_LEFT (the menus), BYE_QUIT (the program closed: sent at exit, so the other side knows at once instead of after six seconds of silence) or BYE_LOST (it stopped hearing this one), and the screen says which. Older releases show any of these as "left the match", as before. Tested with two copies by address: leaving, a closed program, a one-way failure (new test switch BT3_NET_MUTE_AT), and against the 0.1.38.7 and 0.1.38.8 releases both ways; Windows builds
- `09ba09c8` 2026-10-26 z3xox, +46 -0: Lobbies: a member connecting takes the owner's answer from any address of the owner's machines or with the port of its socket (an answer from another address of the machine, or through its router with another port, was dropped: a Windows owner and a Linux member on one network never met though the owner had taken the member), greets the owner wherever the owner's knocks come from, and logs where it greets and what it did not take; when nothing answers it says a firewall on the owner's PC may be blocking the game instead of guessing a release. BT3_TEST_FIREWALL=1 (testing): a socket that takes only what comes from where it has sent, as a stateful firewall. Tested: an owner behind it, a member by code, ten times through a local Worker and twice through the live one; rooms of two, quick matches and spectators by code as before.
- `79cc5d6d` 2026-10-26 z3xox, +34 -6: Spectators run closer behind the match: 24 blanks behind the host at first instead of a second, 12 more each time this copy runs out of the host's blanks (at most the second), 2 fewer after each quiet 900 blanks (down to 18); further behind than the lead and 12, one blank in sixteen runs without picture until it is back. Still only the host's confirmed input. The log says the lead every 1,800 blanks. Measured offscreen at 114% with the spectator drawing: its pictures as even as before (99th percentile 30.0 ms, none over 36 ms), 27 blanks behind (0.4 s) with 5% of its packets lost, the host unchanged, fight values identical.
- `6f99aff2` 2026-10-26 z3xox, +19 -1: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `ef2b70e2` 2026-10-26 z3xox, +302 -22: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.
- `086d9073` 2026-10-26 z3xox, +7 -1: Spectators: the chapter in docs/port/README.md (how, the packets, limits, the test switches, what was tested) and the Worker's part in port/matchmaking/README.md (what /v1/watch and the poll's new fields do, the table watchers, what to deploy). BT3_TEST_SPEC_APART=<blank>: a spectator claims a wrong check value once, for testing that it follows the match again from the start.
- `85daa805` 2026-10-26 z3xox, +145 -16: Spectators with a room code. The Worker takes /v1/watch {v, code, name, addrs}: a spectator of any room of its version gets the host's addresses, and its own (with "seen=" for the host's relay) are kept two minutes in a new table `watchers` (schema.sql; at most 12 a room in that time). A host that sends `watch: true` with its poll is told them (`watchers`), and with `keep: true` as well a room someone has joined starts its ten minutes again, so that the room lives as long as the match: the game's host now keeps polling every five seconds during its session when it lets spectators in, and gives the room up when the session ends. Older games send neither and get the answers they always had (checked: two 0.1.38.7 copies play a match through the new Worker). In the game: "Watch the match" with a code on the Join tab, BT3_LOBBY=spectate:CODE; the host knocks at each new spectator's addresses for ten seconds and lets it through its relay, in its lobby (spectators who come before the other player are told to wait) and in the match. A spectator with a code of a 0.1.38.7 host is told why it found nobody (no answer, or no room once the match has begun); with a Worker that does not know /v1/watch yet, that the service does not take spectators yet. Room-code errors may now be up to 255 characters. node port/matchmaking/test.mjs: 13 new checks. The joiner's log says who watches.
- `04120651` 2026-10-26 z3xox, +8 -0: Spectators: a lobby asking to watch a match that already has six spectators is told so at once, not only when its session has begun.
- `a7e5238c` 2026-10-26 z3xox, +1067 -24: Spectators: a third game (and up to six) can watch a match, by the host's address. A session is computed from its first blank out of nothing but the two players' input, so the host sends a spectator the match's set-up (rules and speed, seed, both players' options and names) and then the input of both players for every blank, as it has it confirmed (never a guess): the spectator's copy computes the same match, character select, battles and rematches included, about a second behind (it waits for a second's worth when it runs out, and runs blanks without picture when it is more than two seconds behind). The host keeps the whole match's input (36 bytes a blank, a byte for 128 unchanged blanks in the packets), so a spectator can come in mid-match and catch up from the start. Nothing waits for spectators: the host sends what is due with its looks at the socket, at most two catch-up packets every 16 ms between them all, and forgets one that is silent for 8 s or still a minute behind after a minute. The host's check values travel with the input and the spectator compares them with its own; one that comes apart says so and follows the match again from the start, once. New packet types 20 to 28, which a 0.1.38.7 ignores: an old host never answers, and the spectator says what that may mean after 10 s; nothing between the two players changed (NET_VERSION 5). The view is the spectator's: player 1's, player 2's, or both halves as the game draws its two-player battle (V or R1; it is drawing only: the fight's values stay the same). In the online window: "Watch the match" on the Join tab, "Let others watch the match" on the Host tab (setting net_spectators); the players see who watches, the spectator a line with the match, the view and the keys, and F1 has Change view / Stop watching. Tests: BT3_LOBBY=spectate:<address>:<port>, BT3_NET_SPECTATE with BT3_NET_SESSION, BT3_LOBBY_AT, BT3_SPEC_VIEWS, BT3_SPEC_LEAVE, BT3_NET_SAVES (the session's save folder), BT3_SHOT_DIR (screenshots of several copies, named by blank too). A program started as the session (tests) now exits when it ends instead of loading a state it never kept.
- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes
- `559f85b2` 2026-10-19 z3xox, +138 -35: Online: a desync is a warning and the match goes on (F1 setting to end it, as before); waits for the other player and slow frames in the log with what caused them, the meter says which; Windows writes bt3_log.txt next to the program
- `19610dcc` 2026-10-19 z3xox, +11 -0: Online: sound effects and Solar Flare with rollback (an effect that only the run again of a blank started is heard, a few blanks late: the other player's vanish or counter was often silent; one heard on a wrong guess and not started again is stopped, and a voice still playing takes the handle the run again gave it: a ki charge's loop went on the whole fight; the blinded player's overlay is drawn in the online full-screen view, where the game drew none)
- `01b94696` 2026-10-19 z3xox, +91 -4: Online: two games with different game data are not matched (the gameplay fingerprint goes to the other side with the greetings; a modified disc image against another one ended as a desync a moment into the fight)
- `f11e5f18` 2026-10-18 dfdevx2, +8 -0: Dragon Rage 0.8.0: Dragon Net
- `2d8a5a72` 2026-10-18 dfdevx2, +200 -3: Dragon Rage 0.7.3
- `aed84ef0` 2026-10-18 z3xox, +5 -1: Online: the profile name no longer erased by Quick Match (Linux)
- `35db3fd4` 2026-10-18 z3xox, +66 -0: Online: a line on the screen while a match starts (Connecting to your rival..., then Your rival "NAME" is here! The match begins.)
- ... e mais 42 (`git log -- port/src/gs/net.c`)

## `port/src/gs/net_chat.c` ([online](online.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +255 -0: Dragon Rage 0.6.0 [skip ci]
- `3772be72` 2026-10-14 z3xox, +26 -6: Online chat: keep copying a line, the mute, and the chat as it was by default
- `06f9c28b` 2026-10-14 jllacchua, +20 -0: Online chat: reset message history between sessions and record sender name
- `b330baf9` 2026-10-13 jllacchua, +215 -0: Online chat: in-game text overlay between players with T key

## `port/src/gs/net_chat.h` ([online](online.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +36 -0: Dragon Rage 0.6.0 [skip ci]
- `06f9c28b` 2026-10-14 jllacchua, +2 -0: Online chat: reset message history between sessions and record sender name
- `b330baf9` 2026-10-13 jllacchua, +34 -0: Online chat: in-game text overlay between players with T key

## `port/src/gs/net_dragon.c` ([online](online.md))

- `97fdfe8e` 2026-10-27 Regulus CiberSec, +0 -26: Ranked: net_dragon.c volta a compilar (restos de uma mescla da fila de resultados)
- `89a39bb8` 2026-10-25 dfdevx2, +2 -0: Dragon Net: busy notice on Ranked, 429 test env
- `0c7ed754` 2026-10-25 dfdevx2, +42 -11: Dragon Net: back off on HTTP 429
- `5fbafc1c` 2026-10-23 Regulus CiberSec, +56 -4: Ranked: trava na fila de resultados
- `fb438773` 2026-10-22 Regulus CiberSec, +40 -3: Ranked: fila de resultados, sem sobrescrever pendentes
- `5707f85b` 2026-10-22 dfdevx2, +8 -0: Fixes from logs, touch pause menu, BT4 menu
- `0b9e1c2a` 2026-10-19 Ariel R Rodrigues, +16 -11: Dragon Net: servidores A/B, Ranked no C, chat desligado, menos polling
- `f11e5f18` 2026-10-18 dfdevx2, +849 -0: Dragon Rage 0.8.0: Dragon Net

## `port/src/gs/net_dragon.h` ([online](online.md))

- `0c7ed754` 2026-10-25 dfdevx2, +3 -1: Dragon Net: back off on HTTP 429
- `0b9e1c2a` 2026-10-19 Ariel R Rodrigues, +6 -0: Dragon Net: servidores A/B, Ranked no C, chat desligado, menos polling
- `176d77e8` 2026-10-18 dfdevx2, +4 -3: Dragon Rage 0.8.0: Dragon Net
- `f11e5f18` 2026-10-18 dfdevx2, +77 -0: Dragon Rage 0.8.0: Dragon Net

## `port/src/gs/net_lobby.c` ([online](online.md))

- `fe6cf04e` 2026-10-31 dfdevx2, +8 -4: Online: 429 match, relay during a match, lobby timeout, banner version, getenv
- `6b69eb72` 2026-10-28 z3xox, +25 -1: 0.1.39: lobbies replace rooms. Tenkaichi Budokai Online has no Create Room: the list is Quick Match, Ranked Match, Lobby, Join (Join Room renamed, the one place for a code), the SOON ones and Profile. Lobby opens New Lobby: Open Lobby (a code) or Host by Address (Port_LobbyCreate on the port net_port, L1 / R1 by a hundred); the lobby's page shows the port and this PC's LAN address where a code would be, square copies "address:port". A new lobby starts with the rules its owner last set on a lobby's page (Tb_LobbyKeepRules, in Create Room's settings: battle with DP 10 / 15 / 20, time, game speed, rollback, delay), so nothing that could be chosen is lost. No Watch on Join or Join by Address: a spectator joins the lobby; JOIN on Join by Address is Port_LobbyJoin and opens the lobby's page at once. Dragon Net Battle (F1 > Online > Advanced) opens and joins lobbies too (Host: a lobby with a code or on its port and the tab's rules; Join: a code or an address), no room of two and no Watch; F1 > Online loses "Let others watch your matches" (rooms only). PORT_RELEASE 139, NET_VERSION 6: a 0.1.38 game is never paired (the service's queues and codes are kept apart by it); by address a member joining a typed address that has not answered for two seconds also sends a spectator's greeting too short to be one, which a 0.1.38 host of two answers with SPEC_NO_VERSION ("The game at that address is a different version"), and a lobby's owner refusing another version notes it once a minute. FP_PEER_REQUIRED is 1: a peer, spectator or member without a fingerprint is refused. Update checks: v0.1.39 and v0.1.39.1 newer than 0.1.38.9 (44 checks).
- `ef26bc43` 2026-10-28 z3xox, +22 -6: Next: the three branches made one. A lobby is made or joined only when no update is being downloaded or waiting to be installed (net_lobby.c online_ok, with the rooms' line), and Port_LobbyStart says so before it holds a start for the fingerprint, so the update's "online off" and the data policy's "wait for your fingerprint" agree for lobbies and tournaments as for rooms. LOBBY_VERSION is 3, a single step from 0.1.38.9's 2 for everything the tournaments add (it had gone to 4 on the tournament branch, never released). Docs: a chapter on how the three meet.
- `277d09eb` 2026-10-28 z3xox, +54 -12: Tournaments: the owner chooses the winner when the two games disagree twice. A draw is still played again, but when the two players' games name different winners for the same match a second time there is no more replay: the owner's bracket page asks it to choose ("The two games disagree on this match: choose the winner", the two names as buttons), the members read that the owner is choosing, no match starts meanwhile, and the owner's choice is final (TOUR_CHOOSE; the match marked as chosen). All the lines are in port_tbo.c for the translators. BT3_TEST_RESULT_LIE makes a member report the other winner and BT3_LOBBY_DO has choose <side>. Tested with two entrants and a lying member: the first disagreement replayed, the second asked the owner, whose choice made the champion.
- `504064a3` 2026-10-27 z3xox, +371 -27: Tournaments: an entrant whose game drops gets back in. Leaving on purpose is still a walkover at once; an entrant whose game falls silent (a lost connection, a crash) keeps its bracket place for 90 s, shown on its box as DISCONNECTED and the time left, with a lobby note, and its next match waits. Joining with the lobby's code within the 90 s puts it back in its place, and the match the drop broke is played again from the start; after 90 s the walkover stands. It is known by a secret rejoin token, not its name: the owner draws 128 bits from the system's random source for each entrant and sends it to that member alone (TOKEN, a new packet type 41), which keeps it in memory and in lobby_rejoin.txt beside its settings file so that a crash or a restart keeps it; its greeting to the lobby of that code carries it. A rejoin gets a new token, so a copied old one is refused; a greeting without a valid token is an ordinary member, a spectator while the tournament is on. The owner forgets a token when its entrant is out, leaves, runs out of time, rejoins, or the tournament ends, and tells the member to forget it; the member also removes its file when the lobby closes, when it leaves, when its lobby does not answer, and at start when the file is over an hour old. The member now keeps the code it asked the service for, since the service's answer to a member carries none. STATE carries each held place's seconds left (616 bytes) and HELLO the token (80 bytes): LOBBY_VERSION 4. Tested offscreen: an entrant killed mid-match and back within 90 s (place restored, match replayed, spectators identical), one back too late, a stranger with its name, a copied old token, the token files removed, a stale file ignored.
- `b84d3e3b` 2026-10-27 z3xox, +29 -13: Tournaments: an Entrants rule decides who plays. In Tournament the battle's row (a tournament's matches are single battles anyway) is Entrants, 2 to 8, the owner's and fixed while a tournament is on, and the places on the left follow it: PLAYERS with the places 1..N, then SPECTATORS with the other 8 - N; Versus keeps 1P, 2P and six spectators. The entrants are whoever sits in the player places when the owner starts the tournament (at least two; empty places are left out, with byes as before), placed with Move; with fewer entrants, those in the places that are no longer the players' are spectators there. Ready is no longer shown or asked for in Tournament, since the places say who plays. The rule travels in STATE (byte 3 of word 96). The user found it unclear who was spectating and who was taking part. Tested: the lobby page with 2, 4 (after 5 and back) and 8 player places, the brackets drawn from them, a two-entrant tournament to its champion.
- `3ea60f57` 2026-10-27 z3xox, +519 -13: Tournaments in lobbies. The lobby's owner switches the lobby between Versus and Tournament (a new first rule) and draws a knockout bracket of the places marked ready (2 to 8 humans, the owner too if it is ready): the order is shuffled from a number the owner draws from the system's random source, with byes shared out between the halves when the entrants are not 2, 4 or 8. The owner's game holds the bracket and sends it with the lobby (STATE, now 608 bytes; LOBBY_VERSION 3, so a 0.1.38.9 game is told the versions differ, both ways). Each match is an ordinary lobby match of single battles: the owner's game puts the two in 1P and 2P and starts it, everyone else watches. Every copy in the match notes how its first battle ended (the battle sequence past the fight, held 0.3 s so that a rolled-back K.O. is not taken; Port_BattleOutcome in headless.c), a player says it with each ALIVE, and the owner, when the two agree on a winner, marks it and ends the match; a draw, two different results, one result alone for 20 s or a match ended any other way is played again. An entrant who leaves loses by walkover. A new page, the bracket (TB_BRACKET in port_tbo.c), shows the two halves meeting at the final with gold lines for winners, the next match glowing, byes greyed and the champion's box, and the owner's Start Next Match / New Tournament; it comes up when a tournament is drawn and after each of its matches, and the lobby's page has a Bracket button. All text is in port_tbo.c for the translators. Tested offscreen through a local Worker: 4 and 5 entrants (byes) to a champion with every spectator identical to its host in every match, a walkover, an old release joining either way; Versus lobbies, quick match and rooms of two unchanged (docs/port/README.md, "Tournaments").
- `79fb9436` 2026-10-27 z3xox, +19 -1: Online: whose data may meet. A quick match (strangers) needs the original disc's data, in every menu: Port_LobbyStartQuick refuses it before the matchmaking service is asked ("Quick match needs the original disc's data, and your game data are not the disc's (what differs). With friends ... you can play when both have the same data."), Dragon Net Battle's Quick match tab says so before the search and greys its button, and Tenkaichi Budokai Online's "Cannot Go Online" is now "No Quick Match", for Quick Match only. With friends (a room code, a lobby, an address, watching) the same data on both sides is enough, modified or not: Tenkaichi Budokai Online's Create Room, Join Room and Lobby no longer refuse modified data. No game connects before its own gameplay fingerprint is known: a start asked for earlier waits (lobby state 6, "Checking your game data..."; a lobby's create and join commands stay queued; net_start holds the picture), and a run that computes none (BT3_GS=none, or online play asked before the data check began) starts it (Port_DataFingerprintNeed; PortFp_Run runs once). A peer that sends no fingerprint: 0.1.37 cannot be told from 0.1.37.1 to 0.1.38.9 by its greetings (NET_VERSION 5, the same capability word; rel 137 at the service), so FP_PEER_REQUIRED decides: 0 for now (MIN_RELEASE is 137), 1 with MIN_RELEASE 138, which refuses such a player, spectator or lobby member. BT3_TEST_FPRINT now acts in plat_fprint.c; BT3_TEST_FPRINT=off sends none (as 0.1.37); BT3_TEST_FP_REQUIRED=1; BT3_UI_NET_TAB opens the online window on a tab. Tested with the local Worker: quick match original against original (BT3_LOBBY and Tenkaichi Budokai Online's screens), a claimed fingerprint refused before the service (both menus show it); by address and room code the same fake fingerprint plays and different ones are refused, after waiting for the fingerprint at the first blank; spectators and lobby members with other data refused, with the same let in; with FP_PEER_REQUIRED a player and a member without a fingerprint refused; 0.1.38.9 by address both ways. Windows builds.
- `b6315e28` 2026-10-26 z3xox, +44 -29: Lobbies: a member connecting takes the owner's first answer only when it carries the nonce of the member's greeting (STATE carries it now; NO did), from whatever address it comes, instead of from any address of the owner's machine or port; the nonce is from the system's random source (/dev/urandom, rand_s on Windows); a knock no longer adds an address to greet. LOBBY_VERSION 2: a game of 09ba09c is told the versions differ. Tested: an owner behind BT3_TEST_FIREWALL joined ten times out of ten; forged answers (no nonce, a wrong nonce, a NO with a wrong one) from another socket not taken; a lobby match and back, rooms of two, quick matches and spectators by code as before.
- `09ba09c8` 2026-10-26 z3xox, +33 -4: Lobbies: a member connecting takes the owner's answer from any address of the owner's machines or with the port of its socket (an answer from another address of the machine, or through its router with another port, was dropped: a Windows owner and a Linux member on one network never met though the owner had taken the member), greets the owner wherever the owner's knocks come from, and logs where it greets and what it did not take; when nothing answers it says a firewall on the owner's PC may be blocking the game instead of guessing a release. BT3_TEST_FIREWALL=1 (testing): a socket that takes only what comes from where it has sent, as a stateful firewall. Tested: an owner behind it, a member by code, ten times through a local Worker and twice through the live one; rooms of two, quick matches and spectators by code as before.
- `6f99aff2` 2026-10-26 z3xox, +39 -1: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `18d6fd06` 2026-10-26 z3xox, +37 -0: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `ef2b70e2` 2026-10-26 z3xox, +1739 -0: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.

## `port/src/gs/net_lobby.h` ([online](online.md))

- `277d09eb` 2026-10-28 z3xox, +5 -3: Tournaments: the owner chooses the winner when the two games disagree twice. A draw is still played again, but when the two players' games name different winners for the same match a second time there is no more replay: the owner's bracket page asks it to choose ("The two games disagree on this match: choose the winner", the two names as buttons), the members read that the owner is choosing, no match starts meanwhile, and the owner's choice is final (TOUR_CHOOSE; the match marked as chosen). All the lines are in port_tbo.c for the translators. BT3_TEST_RESULT_LIE makes a member report the other winner and BT3_LOBBY_DO has choose <side>. Tested with two entrants and a lying member: the first disagreement replayed, the second asked the owner, whose choice made the champion.
- `504064a3` 2026-10-27 z3xox, +2 -1: Tournaments: an entrant whose game drops gets back in. Leaving on purpose is still a walkover at once; an entrant whose game falls silent (a lost connection, a crash) keeps its bracket place for 90 s, shown on its box as DISCONNECTED and the time left, with a lobby note, and its next match waits. Joining with the lobby's code within the 90 s puts it back in its place, and the match the drop broke is played again from the start; after 90 s the walkover stands. It is known by a secret rejoin token, not its name: the owner draws 128 bits from the system's random source for each entrant and sends it to that member alone (TOKEN, a new packet type 41), which keeps it in memory and in lobby_rejoin.txt beside its settings file so that a crash or a restart keeps it; its greeting to the lobby of that code carries it. A rejoin gets a new token, so a copied old one is refused; a greeting without a valid token is an ordinary member, a spectator while the tournament is on. The owner forgets a token when its entrant is out, leaves, runs out of time, rejoins, or the tournament ends, and tells the member to forget it; the member also removes its file when the lobby closes, when it leaves, when its lobby does not answer, and at start when the file is over an hour old. The member now keeps the code it asked the service for, since the service's answer to a member carries none. STATE carries each held place's seconds left (616 bytes) and HELLO the token (80 bytes): LOBBY_VERSION 4. Tested offscreen: an entrant killed mid-match and back within 90 s (place restored, match replayed, spectators identical), one back too late, a stranger with its name, a copied old token, the token files removed, a stale file ignored.
- `b84d3e3b` 2026-10-27 z3xox, +4 -2: Tournaments: an Entrants rule decides who plays. In Tournament the battle's row (a tournament's matches are single battles anyway) is Entrants, 2 to 8, the owner's and fixed while a tournament is on, and the places on the left follow it: PLAYERS with the places 1..N, then SPECTATORS with the other 8 - N; Versus keeps 1P, 2P and six spectators. The entrants are whoever sits in the player places when the owner starts the tournament (at least two; empty places are left out, with byes as before), placed with Move; with fewer entrants, those in the places that are no longer the players' are spectators there. Ready is no longer shown or asked for in Tournament, since the places say who plays. The rule travels in STATE (byte 3 of word 96). The user found it unclear who was spectating and who was taking part. Tested: the lobby page with 2, 4 (after 5 and back) and 8 player places, the brackets drawn from them, a two-entrant tournament to its champion.
- `3ea60f57` 2026-10-27 z3xox, +28 -5: Tournaments in lobbies. The lobby's owner switches the lobby between Versus and Tournament (a new first rule) and draws a knockout bracket of the places marked ready (2 to 8 humans, the owner too if it is ready): the order is shuffled from a number the owner draws from the system's random source, with byes shared out between the halves when the entrants are not 2, 4 or 8. The owner's game holds the bracket and sends it with the lobby (STATE, now 608 bytes; LOBBY_VERSION 3, so a 0.1.38.9 game is told the versions differ, both ways). Each match is an ordinary lobby match of single battles: the owner's game puts the two in 1P and 2P and starts it, everyone else watches. Every copy in the match notes how its first battle ended (the battle sequence past the fight, held 0.3 s so that a rolled-back K.O. is not taken; Port_BattleOutcome in headless.c), a player says it with each ALIVE, and the owner, when the two agree on a winner, marks it and ends the match; a draw, two different results, one result alone for 20 s or a match ended any other way is played again. An entrant who leaves loses by walkover. A new page, the bracket (TB_BRACKET in port_tbo.c), shows the two halves meeting at the final with gold lines for winners, the next match glowing, byes greyed and the champion's box, and the owner's Start Next Match / New Tournament; it comes up when a tournament is drawn and after each of its matches, and the lobby's page has a Bracket button. All text is in port_tbo.c for the translators. Tested offscreen through a local Worker: 4 and 5 entrants (byes) to a champion with every spectator identical to its host in every match, a walkover, an old release joining either way; Versus lobbies, quick match and rooms of two unchanged (docs/port/README.md, "Tournaments").
- `79fb9436` 2026-10-27 z3xox, +2 -0: Online: whose data may meet. A quick match (strangers) needs the original disc's data, in every menu: Port_LobbyStartQuick refuses it before the matchmaking service is asked ("Quick match needs the original disc's data, and your game data are not the disc's (what differs). With friends ... you can play when both have the same data."), Dragon Net Battle's Quick match tab says so before the search and greys its button, and Tenkaichi Budokai Online's "Cannot Go Online" is now "No Quick Match", for Quick Match only. With friends (a room code, a lobby, an address, watching) the same data on both sides is enough, modified or not: Tenkaichi Budokai Online's Create Room, Join Room and Lobby no longer refuse modified data. No game connects before its own gameplay fingerprint is known: a start asked for earlier waits (lobby state 6, "Checking your game data..."; a lobby's create and join commands stay queued; net_start holds the picture), and a run that computes none (BT3_GS=none, or online play asked before the data check began) starts it (Port_DataFingerprintNeed; PortFp_Run runs once). A peer that sends no fingerprint: 0.1.37 cannot be told from 0.1.37.1 to 0.1.38.9 by its greetings (NET_VERSION 5, the same capability word; rel 137 at the service), so FP_PEER_REQUIRED decides: 0 for now (MIN_RELEASE is 137), 1 with MIN_RELEASE 138, which refuses such a player, spectator or lobby member. BT3_TEST_FPRINT now acts in plat_fprint.c; BT3_TEST_FPRINT=off sends none (as 0.1.37); BT3_TEST_FP_REQUIRED=1; BT3_UI_NET_TAB opens the online window on a tab. Tested with the local Worker: quick match original against original (BT3_LOBBY and Tenkaichi Budokai Online's screens), a claimed fingerprint refused before the service (both menus show it); by address and room code the same fake fingerprint plays and different ones are refused, after waiting for the fingerprint at the first blank; spectators and lobby members with other data refused, with the same let in; with FP_PEER_REQUIRED a player and a member without a fingerprint refused; 0.1.38.9 by address both ways. Windows builds.
- `18d6fd06` 2026-10-26 z3xox, +10 -0: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `ef2b70e2` 2026-10-26 z3xox, +97 -0: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.

## `port/src/gs/net_match.c` ([online](online.md))

- `f0bd6511` 2026-10-31 dfdevx2, +7 -0: Requests: 2 s polls, backoff on 429, none in the background; the relay socket checks the host
- `fe6cf04e` 2026-10-31 dfdevx2, +6 -2: Online: 429 match, relay during a match, lobby timeout, banner version, getenv
- `d54095dd` 2026-10-29 Regulus CiberSec, +21 -1: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `d747606b` 2026-10-27 Androidxxt01, +53 -1: Retry Dragon Net lobby codes on peer server
- `41870d9c` 2026-10-27 Androidxxt01, +53 -1: Retry Dragon Net lobby codes on peer server
- `0ca19719` 2026-10-27 z3xox, +4 -98: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `d01e8270` 2026-10-14 jllacchua, +36 -4: Auto-updater: in-game release download, progress bar, extraction and restart
- `cbde53ab` 2026-10-26 z3xox, +64 -3: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `84283eaa` 2026-10-26 z3xox, +92 -7: Lobbies, the service's part and the relay: a lobby's room code, and a relay that serves several members. /v1/host with "lobby": true makes a lobby's room (a new column rooms.lobby); a join or watch of its code from a game that says "lobby" is answered with the owner's addresses and {lobby: true}, and the member's addresses are kept in watchers, where the owner's polls find them as a spectator's. A game that does not say it (0.1.38.7 and older) is answered "old" and shows "Update available..."; a new game joining an ordinary room gets the answer it always had. The owner's poll with watch and keep keeps a lobby's room for as long as it asks, a member's poll with its key gets the owner's addresses again (the relay address that came later), and /v1/relay is the owner's once a member has come. A database without the column answers "no lobbies" and makes every other room as before. In the game, net_match.c makes a lobby's room (role 5: polls every four seconds, 900 requests an hour of lobby, and asks for a relay address when the lobby says a member cannot reach it), sends "lobby" with joins and watches, and says when a code is a lobby's. The TURN client (net_relay.c) now serves up to twelve addresses with a channel each and lets sixteen through, a ChannelBind's answer matched to its request by the transaction; a match of two uses one place as before. node port/matchmaking/test.mjs: 17 new checks.
- `85daa805` 2026-10-26 z3xox, +123 -3: Spectators with a room code. The Worker takes /v1/watch {v, code, name, addrs}: a spectator of any room of its version gets the host's addresses, and its own (with "seen=" for the host's relay) are kept two minutes in a new table `watchers` (schema.sql; at most 12 a room in that time). A host that sends `watch: true` with its poll is told them (`watchers`), and with `keep: true` as well a room someone has joined starts its ten minutes again, so that the room lives as long as the match: the game's host now keeps polling every five seconds during its session when it lets spectators in, and gives the room up when the session ends. Older games send neither and get the answers they always had (checked: two 0.1.38.7 copies play a match through the new Worker). In the game: "Watch the match" with a code on the Join tab, BT3_LOBBY=spectate:CODE; the host knocks at each new spectator's addresses for ten seconds and lets it through its relay, in its lobby (spectators who come before the other player are told to wait) and in the match. A spectator with a code of a 0.1.38.7 host is told why it found nobody (no answer, or no room once the match has begun); with a Worker that does not know /v1/watch yet, that the service does not take spectators yet. Room-code errors may now be up to 255 characters. node port/matchmaking/test.mjs: 13 new checks. The joiner's log says who watches.
- `89a39bb8` 2026-10-25 dfdevx2, +1 -1: Dragon Net: busy notice on Ranked, 429 test env
- `0c7ed754` 2026-10-25 dfdevx2, +118 -4: Dragon Net: back off on HTTP 429
- `d0b4bb8c` 2026-10-19 Ariel R Rodrigues, +1 -1: Dragon Net: servidor B temporario (DBT-NX)
- `c55cabac` 2026-10-19 Ariel R Rodrigues, +9 -0: Dragon Net: embute URL do servidor C (ranked) e campo do B
- `0b9e1c2a` 2026-10-19 Ariel R Rodrigues, +128 -24: Dragon Net: servidores A/B, Ranked no C, chat desligado, menos polling
- `f11e5f18` 2026-10-18 dfdevx2, +57 -21: Dragon Rage 0.8.0: Dragon Net
- `7b99f8ad` 2026-10-15 dfdevx2, +456 -34: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +114 -13: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `6e08d7c5` 2026-10-12 z3xox, +75 -3: An update notice: the newest release is asked of GitHub once per start; a line over the picture and a button to the release page in the settings window ("Check for updates" turns it off)
- `bbd4c2b5` 2026-10-11 dfdevx2, +38 -0: Dragon Rage 0.5.0 [skip ci]
- `d2b34265` 2026-10-11 z3xox, +89 -4: A relay login is asked for when a match needs one, not taken with every room (quick match's searches used up the day's logins and the relay was off for everyone)
- `c927b17d` 2026-10-11 z3xox, +43 -3: The online screen says when this release is too old for the matchmaking service, or when a newer one exists (asked once as the screen opens)
- `19360c64` 2026-10-11 z3xox, +10 -6: The matchmaking service can refuse releases older than one its keeper names (MIN_RELEASE); the game says which release it is (port_release.h, raised with each release)
- `7779c68a` 2026-10-11 z3xox, +117 -7: Quick match: five queues by battle type (single, team, DP 10 / 15 / 20, no time limit); the service pairs whoever searches a queue with the one who waits longest, the online screen has a tab for it
- `928b2413` 2026-10-11 dfdevx2, +594 -0: WIP: patch 1 and touch artwork
- ... e mais 5 (`git log -- port/src/gs/net_match.c`)

## `port/src/gs/net_match.h` ([online](online.md))

- `d747606b` 2026-10-27 Androidxxt01, +1 -0: Retry Dragon Net lobby codes on peer server
- `41870d9c` 2026-10-27 Androidxxt01, +1 -0: Retry Dragon Net lobby codes on peer server
- `0ca19719` 2026-10-27 z3xox, +0 -4: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `d01e8270` 2026-10-14 jllacchua, +2 -0: Auto-updater: in-game release download, progress bar, extraction and restart
- `cbde53ab` 2026-10-26 z3xox, +3 -0: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `84283eaa` 2026-10-26 z3xox, +7 -1: Lobbies, the service's part and the relay: a lobby's room code, and a relay that serves several members. /v1/host with "lobby": true makes a lobby's room (a new column rooms.lobby); a join or watch of its code from a game that says "lobby" is answered with the owner's addresses and {lobby: true}, and the member's addresses are kept in watchers, where the owner's polls find them as a spectator's. A game that does not say it (0.1.38.7 and older) is answered "old" and shows "Update available..."; a new game joining an ordinary room gets the answer it always had. The owner's poll with watch and keep keeps a lobby's room for as long as it asks, a member's poll with its key gets the owner's addresses again (the relay address that came later), and /v1/relay is the owner's once a member has come. A database without the column answers "no lobbies" and makes every other room as before. In the game, net_match.c makes a lobby's room (role 5: polls every four seconds, 900 requests an hour of lobby, and asks for a relay address when the lobby says a member cannot reach it), sends "lobby" with joins and watches, and says when a code is a lobby's. The TURN client (net_relay.c) now serves up to twelve addresses with a channel each and lets sixteen through, a ChannelBind's answer matched to its request by the transaction; a match of two uses one place as before. node port/matchmaking/test.mjs: 17 new checks.
- `85daa805` 2026-10-26 z3xox, +6 -2: Spectators with a room code. The Worker takes /v1/watch {v, code, name, addrs}: a spectator of any room of its version gets the host's addresses, and its own (with "seen=" for the host's relay) are kept two minutes in a new table `watchers` (schema.sql; at most 12 a room in that time). A host that sends `watch: true` with its poll is told them (`watchers`), and with `keep: true` as well a room someone has joined starts its ten minutes again, so that the room lives as long as the match: the game's host now keeps polling every five seconds during its session when it lets spectators in, and gives the room up when the session ends. Older games send neither and get the answers they always had (checked: two 0.1.38.7 copies play a match through the new Worker). In the game: "Watch the match" with a code on the Join tab, BT3_LOBBY=spectate:CODE; the host knocks at each new spectator's addresses for ten seconds and lets it through its relay, in its lobby (spectators who come before the other player are told to wait) and in the match. A spectator with a code of a 0.1.38.7 host is told why it found nobody (no answer, or no room once the match has begun); with a Worker that does not know /v1/watch yet, that the service does not take spectators yet. Room-code errors may now be up to 255 characters. node port/matchmaking/test.mjs: 13 new checks. The joiner's log says who watches.
- `0c7ed754` 2026-10-25 dfdevx2, +1 -0: Dragon Net: back off on HTTP 429
- `f11e5f18` 2026-10-18 dfdevx2, +2 -0: Dragon Rage 0.8.0: Dragon Net
- `7b99f8ad` 2026-10-15 dfdevx2, +16 -0: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +1 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `6e08d7c5` 2026-10-12 z3xox, +2 -0: An update notice: the newest release is asked of GitHub once per start; a line over the picture and a button to the release page in the settings window ("Check for updates" turns it off)
- `d2b34265` 2026-10-11 z3xox, +3 -0: A relay login is asked for when a match needs one, not taken with every room (quick match's searches used up the day's logins and the relay was off for everyone)
- `c927b17d` 2026-10-11 z3xox, +2 -0: The online screen says when this release is too old for the matchmaking service, or when a newer one exists (asked once as the screen opens)
- `7779c68a` 2026-10-11 z3xox, +8 -0: Quick match: five queues by battle type (single, team, DP 10 / 15 / 20, no time limit); the service pairs whoever searches a queue with the one who waits longest, the online screen has a tab for it
- `928b2413` 2026-10-11 dfdevx2, +20 -0: WIP: patch 1 and touch artwork
- `b2f73a79` 2026-10-09 z3xox, +20 -0: Online play: room codes. The lobby can host and join with a short code through the matchmaking Worker (STUN for the public address, both sides send to each other's addresses)

## `port/src/gs/net_relay.c` ([online](online.md))

- `fe6cf04e` 2026-10-31 dfdevx2, +5 -1: Online: 429 match, relay during a match, lobby timeout, banner version, getenv
- `84283eaa` 2026-10-26 z3xox, +97 -46: Lobbies, the service's part and the relay: a lobby's room code, and a relay that serves several members. /v1/host with "lobby": true makes a lobby's room (a new column rooms.lobby); a join or watch of its code from a game that says "lobby" is answered with the owner's addresses and {lobby: true}, and the member's addresses are kept in watchers, where the owner's polls find them as a spectator's. A game that does not say it (0.1.38.7 and older) is answered "old" and shows "Update available..."; a new game joining an ordinary room gets the answer it always had. The owner's poll with watch and keep keeps a lobby's room for as long as it asks, a member's poll with its key gets the owner's addresses again (the relay address that came later), and /v1/relay is the owner's once a member has come. A database without the column answers "no lobbies" and makes every other room as before. In the game, net_match.c makes a lobby's room (role 5: polls every four seconds, 900 requests an hour of lobby, and asks for a relay address when the lobby says a member cannot reach it), sends "lobby" with joins and watches, and says when a code is a lobby's. The TURN client (net_relay.c) now serves up to twelve addresses with a channel each and lets sixteen through, a ChannelBind's answer matched to its request by the transaction; a match of two uses one place as before. node port/matchmaking/test.mjs: 17 new checks.
- `928b2413` 2026-10-11 dfdevx2, +603 -0: WIP: patch 1 and touch artwork
- `036c74d1` 2026-10-09 z3xox, +61 -1: Relay: channels (ChannelBind, the match's packets as channel data), '(relay)' beside the ping on the meter, packet counts in BT3_RELAY_LOG; notes: two networks played by the user, and the crawl of a relayed match between two copies behind one router, measured and not explained
- `3a361aa6` 2026-10-09 z3xox, +543 -0: Online play: a relay for room codes. The host gets an address at Cloudflare's TURN service and posts it with its others; the one who joins tries it when the direct ways stay silent

## `port/src/gs/net_relay.h` ([online](online.md))

- `928b2413` 2026-10-11 dfdevx2, +31 -0: WIP: patch 1 and touch artwork
- `3a361aa6` 2026-10-09 z3xox, +31 -0: Online play: a relay for room codes. The host gets an address at Cloudflare's TURN service and posts it with its others; the one who joins tries it when the direct ways stay silent

## `port/src/gs/net_sign.c` ([online](online.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +253 -0: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +253 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets

## `port/src/gs/net_sign.h` ([online](online.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +24 -0: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +24 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets

## `port/src/gs/net_update.c` ([online](online.md))

- `30873d9b` 2026-10-27 z3xox, +33 -31: Automatic update: the list of .old files to remove at the next start is written and read through SDL (the folder's path may have any letters; fopen took it in the system's code page on Windows) with '\n' alone, and read line by line whatever ends them: on Windows fopen's text mode wrote '\r\n' and no .old file was ever removed; a line removed also left the rest of its text to be read as another. Found by running the install under wine: a 0.1.38.9 Windows folder whose running Tenkaichi3Decomp.exe (a driver of net_update.c's calls) and loaded SDL3.dll are renamed, the 0.1.38.10 zip in place, the player's files unchanged, every .old gone at the next start; with BT3_UPDATE_FAIL_AT=8 everything back as it was, the running program included.
- `0914c6d2` 2026-10-27 z3xox, +62 -8: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.
- `0ca19719` 2026-10-27 z3xox, +1190 -229: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `d01e8270` 2026-10-14 jllacchua, +340 -0: Auto-updater: in-game release download, progress bar, extraction and restart

## `port/src/gs/net_update.h` ([online](online.md))

- `0ca19719` 2026-10-27 z3xox, +40 -34: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `d01e8270` 2026-10-14 jllacchua, +44 -0: Auto-updater: in-game release download, progress bar, extraction and restart

## `port/src/gs/shaders/ffx/LICENSE-FidelityFX-FSR.txt` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +19 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/ffx/LICENSE-Snapdragon-GSR.txt` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +29 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/ffx/ffx_a.h` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +2656 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/ffx/ffx_fsr1.h` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +1199 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/fsr_easu.frag` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +25 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/fsr_rcas.frag` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +23 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/gs.frag` ([renderer-gs](renderer-gs.md))

- `6dbb460b` 2026-10-21 z3xox, +9 -0: Glow: the GS's 1x sampling of a work buffer in a shader of its own (gswb.frag, pipeline key bit 25), only for draws from one work buffer into another, one read per GS texel at 2x (in the shader of every draw and on the passes over the whole picture it took a fight at 2x on an integrated chip from 124 pictures a second to 90; now 118, 122 without it: BT3_GLOW_BLOCK=0)
- `71e6e574` 2026-10-21 z3xox, +18 -1: Glow: a work buffer read with the bilinear filter is sampled as the GS samples it at 1x, each GS texel the average of its block (at a higher internal resolution the glow chain's blur steps, half a GS texel to the side, blended two nearly equal texels and only moved the picture: the glow was a sharp copy of the character beside it, reported on a Super Saiyan's hair); 1x is the same to the pixel, OpenGL as Vulkan
- `2d8a5a72` 2026-10-18 dfdevx2, +3 -1: Dragon Rage 0.7.3
- `9af92af5` 2026-10-17 z3xox, +3 -1: Blending: a source alpha above 0x80 counts as on the PS2 (the Solar Flare's whiteout)
- `7b99f8ad` 2026-10-15 dfdevx2, +25 -0: Dragon Rage 0.6.0 [skip ci]
- `df22e6f8` 2026-10-14 z3xox, +12 -1: White dots, done properly: the depth the shader writes is never above the depth it came from (the step below where the division rounds up); the depth bias of the first try is gone (it let layers one step behind show through)
- `e16805d3` 2026-10-12 z3xox, +7 -0: Performance: the per-pixel depth of 0.1.25 only where it changes something (it cost a quarter of the frame rate on integrated graphics)
- `161a7632` 2026-10-11 z3xox, +7 -0: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)
- `928b2413` 2026-10-11 dfdevx2, +18 -1: WIP: patch 1 and touch artwork
- `9f4b7e9b` 2026-10-11 z3xox, +8 -2: Texture packs: a replacement drawn as 2D art no longer picks its texel on the boundary (broken letters, stepped logos) and follows a gliding piece smoothly in both 2D filtering settings (the main menu's text tore while it moved)
- `8132ee13` 2026-10-10 z3xox, +11 -0: The smooth 2D experiment: the game's own art takes the plain coordinate of each output pixel inside the piece's rectangle (the replacement's rule chose between two formulas per pixel and flickered: comb stripes in small text); BT3_2D_SMOOTH=2 is everything, 1 the fight's display
- `7427cc37` 2026-10-09 Claude, +6 -0: GPU: shader without discard for draws that never discard; split frame submission
- `04964a3d` 2026-10-06 z3xox, +46 -7: Texture packs: PCSX2-named DDS replacements from the textures folder, with a switch in the settings window
- `6474cf76` 2026-10-06 z3xox, +11 -1: Port: 2D sprites sampled at the GS's sample points (no bleeding from neighbouring pictures of a sheet); notes
- `301c8a7b` 2026-10-06 z3xox, +12 -2: Port: destination alpha test (TEST.DATE) and FBA, for the masks of the HUD bars

## `port/src/gs/shaders/outline.frag` ([renderer-gs](renderer-gs.md))

- `58faf782` 2026-10-22 z3xox, +11 -1: The outline writes only where it draws a line. Its pass covers the whole picture and subtracted 0 from almost every pixel (the pipeline: picture minus this, colour only), reading and writing each back; now a pixel with nothing to subtract is discarded, and one with no object (the table's 0: stage, sky) does not read its neighbours. The same picture (the recording at 3x, 28 of 28); on an integrated chip at 3x Mount Paozu 71.0 pictures a second instead of 66.0, Desert at noon 52.9 instead of 50.8
- `6af08452` 2026-10-22 z3xox, +11 -1: The outline writes only where it draws a line. Its pass covers the whole picture and subtracted 0 from almost every pixel (the pipeline: picture minus this, colour only), reading and writing each back; now a pixel with nothing to subtract is discarded, and one with no object (the table's 0: stage, sky) does not read its neighbours. The same picture (the recording at 3x, 28 of 28); on an integrated chip at 3x Mount Paozu 71.0 pictures a second instead of 66.0, Desert at noon 52.9 instead of 50.8
- `f40dc865` 2026-10-20 z3xox, +15 -5: Outline: the same thickness at any aspect ratio (widescreen stretches the PS2's 512 pixels across the picture, and the line one PS2 pixel wide on the left and right of every shape grew with it: 8.2 screen pixels at 21:9 against 4.7 at 4:3; across, the width is narrowed by the stretch and measured in texels, a fraction drawn as a lighter last texel)

## `port/src/gs/shaders/present.frag` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +56 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/sgsr.frag` ([renderer-gs](renderer-gs.md))

- `6484b228` 2026-10-09 Claude, +122 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions

## `port/src/gs/shaders/vu0.vert` ([renderer-gs](renderer-gs.md))

- `14c996fd` 2026-10-21 z3xox, +3 -1: Glow: its work buffers (pages 0x150, 0x170, 0x1F6, 0x1F8) drawn at the PS2's own resolution, one texel per GS pixel in the corner of their textures (positions and scissor not scaled up, texture coordinates scaled down; the vertex programs' draws into them through the uniforms' target scale, after the in-between pictures' camera): the filter is the GS's and those passes have SCALE x SCALE fewer pixels; a fight on an integrated chip at 2x 128 pictures a second (0.1.37.1: 124), at 6x 16 (14 to 15); BT3_GLOW_NATIVE=0 as before
- `7b99f8ad` 2026-10-15 dfdevx2, +6 -2: Dragon Rage 0.6.0 [skip ci]
- `c39c5c25` 2026-10-11 z3xox, +3 -3: In-between pictures: the wall at the edge of the map is drawn from the game's own patch, a surface keeps its identity when its texture changes (a waterfall jumped a step to the side); what the game transforms itself leads by two depth steps (the fall's filled part was a speckle in motion)
- `161a7632` 2026-10-11 z3xox, +4 -0: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)
- `69c9ff21` 2026-10-06 z3xox, +9 -2: Port: vertex program 1 (second fighter pass, e.g. Raditz's scouter lens) as a shader
- `85f9e475` 2026-10-06 z3xox, +7 -3: Port: vertex programs 7 (debris) and 8 (animated stage objects) as shaders

## `port/src/gs/shaders/vu4.vert` ([renderer-gs](renderer-gs.md))

- `14c996fd` 2026-10-21 z3xox, +4 -1: Glow: its work buffers (pages 0x150, 0x170, 0x1F6, 0x1F8) drawn at the PS2's own resolution, one texel per GS pixel in the corner of their textures (positions and scissor not scaled up, texture coordinates scaled down; the vertex programs' draws into them through the uniforms' target scale, after the in-between pictures' camera): the filter is the GS's and those passes have SCALE x SCALE fewer pixels; a fight on an integrated chip at 2x 128 pictures a second (0.1.37.1: 124), at 6x 16 (14 to 15); BT3_GLOW_NATIVE=0 as before
- `7b99f8ad` 2026-10-15 dfdevx2, +5 -1: Dragon Rage 0.6.0 [skip ci]
- `c39c5c25` 2026-10-11 z3xox, +2 -2: In-between pictures: the wall at the edge of the map is drawn from the game's own patch, a surface keeps its identity when its texture changes (a waterfall jumped a step to the side); what the game transforms itself leads by two depth steps (the fall's filled part was a speckle in motion)
- `161a7632` 2026-10-11 z3xox, +4 -0: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)

## `port/src/gs/shaders/vu6.vert` ([renderer-gs](renderer-gs.md))

- `14c996fd` 2026-10-21 z3xox, +4 -1: Glow: its work buffers (pages 0x150, 0x170, 0x1F6, 0x1F8) drawn at the PS2's own resolution, one texel per GS pixel in the corner of their textures (positions and scissor not scaled up, texture coordinates scaled down; the vertex programs' draws into them through the uniforms' target scale, after the in-between pictures' camera): the filter is the GS's and those passes have SCALE x SCALE fewer pixels; a fight on an integrated chip at 2x 128 pictures a second (0.1.37.1: 124), at 6x 16 (14 to 15); BT3_GLOW_NATIVE=0 as before
- `7b99f8ad` 2026-10-15 dfdevx2, +5 -1: Dragon Rage 0.6.0 [skip ci]
- `c39c5c25` 2026-10-11 z3xox, +2 -2: In-between pictures: the wall at the edge of the map is drawn from the game's own patch, a surface keeps its identity when its texture changes (a waterfall jumped a step to the side); what the game transforms itself leads by two depth steps (the fall's filled part was a speckle in motion)
- `161a7632` 2026-10-11 z3xox, +4 -0: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)

## `port/src/gs/snd_adx.c` ([renderer-gs](renderer-gs.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +14 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +102 -1: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +40 -3: Add native Tag Team modes
- `4481148c` 2026-10-21 z3xox, +6 -3: Online with the second voice set: the fight's streamed effects (0x8D30 and on: the clash, a transformation's, the moves') no longer replaced by a menu guide's English line (the guides' range is 0x87D4..0x8D2F, not 0x87F2..0x8D4D); a fighter's voice bank recognised by its whole header table (Goku (End) and Turles Great Ape share the first 64 bytes)
- `109cfbe5` 2026-10-19 Renzo, +86 -8: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion
- `7b99f8ad` 2026-10-15 dfdevx2, +196 -13: Dragon Rage 0.6.0 [skip ci]
- `715afc18` 2026-10-11 piapenia, +5 -1: a11y: quieter guides, no subtitles, screens without a reader of their own
- `d48b19ba` 2026-10-13 z3xox, +59 -8: Online: how long a voice line lasts is counted from a table of the original disc's streams, not from the player's own file (a voice mod of another length started the fight on another blank)
- `aa36212f` 2026-10-12 z3xox, +13 -0: Audio: "Music follows game speed", off by default: looping music plays as it is at any game speed
- `8f70bbb3` 2026-10-12 z3xox, +108 -11: Sound at another game speed keeps its pitch: streams are stretched in time, effects play as they are
- `188e9740` 2026-10-11 z3xox, +6 -2: Cheats: a game speed of one's own, 50% to 200%, outside online play (an online match never reads it)
- `c6e5f78d` 2026-10-11 z3xox, +9 -4: Volume: voices and stage sounds get their own sliders, apart from the sound effects
- `86110097` 2026-10-11 z3xox, +6 -0: Online matches at 120% speed, the competitive standard: every quick match, and the host's choice with a room (the pace of the vertical blanks and of the sound; only when both copies know it)
- `161a7632` 2026-10-11 z3xox, +6 -3: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)
- `928b2413` 2026-10-11 dfdevx2, +64 -0: WIP: patch 1 and touch artwork
- `ca55bb2c` 2026-10-10 z3xox, +17 -0: Added songs: offered in team and DP battles too (the team select's own music list had been left without them); a song without loop points is played round like the disc's music
- `9e1d9679` 2026-10-09 z3xox, +47 -0: Online play: each player hears the voice set chosen in their own save
- `d285f5d6` 2026-10-08 Claude, +14 -5: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `3fbf12d1` 2026-10-07 z3xox, +31 -3: Online session inside the running program: the state at the first blank is kept, a session puts the game back to it and leaving brings the player's own state back (no restart); GS memory kept with it; sound made to fit the state
- `6130d57b` 2026-10-07 z3xox, +5 -0: Frames that are only re-run draw nothing, show nothing and start no sound (gPortResim); the state test's undone pass runs that way and still leaves the same state and picture
- `8a0dc162` 2026-10-07 z3xox, +55 -2: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences
- `d5ac0274` 2026-10-06 z3xox, +11 -1: Port: settings overlay (F1) with live resolution, aspect, full screen, effects and volumes

## `port/src/gs/snd_ma.c` ([renderer-gs](renderer-gs.md))

- `b98a5f08` 2026-10-21 z3xox, +10 -0: Songs played by miniaudio: its device is stopped as the program ends, so its thread is out of the decoders before exit() closes the files (Windows' C library closes them all while that thread still ran; tested: quitting during a song)
- `109cfbe5` 2026-10-19 Renzo, +285 -0: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion

## `port/src/gs/snd_se.c` ([renderer-gs](renderer-gs.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +38 -13: Add native Tag Team modes
- `4481148c` 2026-10-21 z3xox, +9 -4: Online with the second voice set: the fight's streamed effects (0x8D30 and on: the clash, a transformation's, the moves') no longer replaced by a menu guide's English line (the guides' range is 0x87D4..0x8D2F, not 0x87F2..0x8D4D); a fighter's voice bank recognised by its whole header table (Goku (End) and Turles Great Ape share the first 64 bytes)
- `19610dcc` 2026-10-19 z3xox, +121 -4: Online: sound effects and Solar Flare with rollback (an effect that only the run again of a blank started is heard, a few blanks late: the other player's vanish or counter was often silent; one heard on a wrong guess and not started again is stopped, and a voice still playing takes the handle the run again gave it: a ki charge's loop went on the whole fight; the blinded player's overlay is drawn in the online full-screen view, where the game drew none)
- `7b99f8ad` 2026-10-15 dfdevx2, +34 -7: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +29 -3: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `8f70bbb3` 2026-10-12 z3xox, +0 -9: Sound at another game speed keeps its pitch: streams are stretched in time, effects play as they are
- `188e9740` 2026-10-11 z3xox, +2 -2: Cheats: a game speed of one's own, 50% to 200%, outside online play (an online match never reads it)
- `c6e5f78d` 2026-10-11 z3xox, +5 -4: Volume: voices and stage sounds get their own sliders, apart from the sound effects
- `86110097` 2026-10-11 z3xox, +9 -0: Online matches at 120% speed, the competitive standard: every quick match, and the host's choice with a room (the pace of the vertical blanks and of the sound; only when both copies know it)
- `928b2413` 2026-10-11 dfdevx2, +116 -3: WIP: patch 1 and touch artwork
- `9e1d9679` 2026-10-09 z3xox, +116 -3: Online play: each player hears the voice set chosen in their own save
- `6130d57b` 2026-10-07 z3xox, +3 -0: Frames that are only re-run draw nothing, show nothing and start no sound (gPortResim); the state test's undone pass runs that way and still leaves the same state and picture
- `66e2a7b1` 2026-10-06 z3xox, +4 -3: Port: 64-bit build (game pointers stay 4 bytes: ptr32.py, irfix.py, low memory); replay and heap match the 32-bit build
- `d5ac0274` 2026-10-06 z3xox, +2 -1: Port: settings overlay (F1) with live resolution, aspect, full screen, effects and volumes

## `port/src/gs/stage_scale.cpp` ([renderer-gs](renderer-gs.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +372 -0: Add native Tag Team modes

## `port/src/gs/state.c` ([renderer-gs](renderer-gs.md))

- `a7e5238c` 2026-10-26 z3xox, +10 -0: Spectators: a third game (and up to six) can watch a match, by the host's address. A session is computed from its first blank out of nothing but the two players' input, so the host sends a spectator the match's set-up (rules and speed, seed, both players' options and names) and then the input of both players for every blank, as it has it confirmed (never a guess): the spectator's copy computes the same match, character select, battles and rematches included, about a second behind (it waits for a second's worth when it runs out, and runs blanks without picture when it is more than two seconds behind). The host keeps the whole match's input (36 bytes a blank, a byte for 128 unchanged blanks in the packets), so a spectator can come in mid-match and catch up from the start. Nothing waits for spectators: the host sends what is due with its looks at the socket, at most two catch-up packets every 16 ms between them all, and forgets one that is silent for 8 s or still a minute behind after a minute. The host's check values travel with the input and the spectator compares them with its own; one that comes apart says so and follows the match again from the start, once. New packet types 20 to 28, which a 0.1.38.7 ignores: an old host never answers, and the spectator says what that may mean after 10 s; nothing between the two players changed (NET_VERSION 5). The view is the spectator's: player 1's, player 2's, or both halves as the game draws its two-player battle (V or R1; it is drawing only: the fight's values stay the same). In the online window: "Watch the match" on the Join tab, "Let others watch the match" on the Host tab (setting net_spectators); the players see who watches, the spectator a line with the match, the view and the keys, and F1 has Change view / Stop watching. Tests: BT3_LOBBY=spectate:<address>:<port>, BT3_NET_SPECTATE with BT3_NET_SESSION, BT3_LOBBY_AT, BT3_SPEC_VIEWS, BT3_SPEC_LEAVE, BT3_NET_SAVES (the session's save folder), BT3_SHOT_DIR (screenshots of several copies, named by blank too). A program started as the session (tests) now exits when it ends instead of loading a state it never kept.
- `7b99f8ad` 2026-10-15 dfdevx2, +83 -7: Dragon Rage 0.6.0 [skip ci]
- `392ca031` 2026-10-12 Valentin Vivaldi, +18 -4: state.c: Linux and Windows keep their original code exactly (the macOS forms only under __APPLE__)
- `81bc5508` 2026-10-12 Valentin Vivaldi, +56 -12: macOS: rollback and the state list (rewind test: the whole demo fight, 0 differences at depth 1 and 8)
- `660c1a38` 2026-10-12 Valentin Vivaldi, +4 -3: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `52f2861a` 2026-10-11 z3xox, +17 -0: The list of the game's variables says which program it is for: a list left over from another release (a Windows copy with only the .exe replaced) is refused with a message instead of crashing the first online match
- `928b2413` 2026-10-11 dfdevx2, +82 -8: WIP: patch 1 and touch artwork
- `2f14974f` 2026-10-07 z3xox, +1 -1: Net window: rollback and input delay (host's choices sent to the joiner); goodbye packet, 6 s time-out, a line over the picture when a match ends
- `ff06e5a4` 2026-10-07 z3xox, +17 -0: Rollback netcode (BT3_NET_ROLLBACK=<n>): guess the other player's input, go back and re-run when it was wrong; BT3_NET_LATENCY for testing
- `9d7f0cd7` 2026-10-07 z3xox, +385 -4: Roll ring: a state save per blank that keeps only the pages written (write faults on Linux, write watch on Windows); saving 0.74 -> 0.08 ms
- `401fb60d` 2026-10-07 z3xox, +13 -1: Release package carries the list of the game's variables; without it sessions restart the program as before
- `3fbf12d1` 2026-10-07 z3xox, +99 -0: Online session inside the running program: the state at the first blank is kept, a session puts the game back to it and leaving brings the player's own state back (no restart); GS memory kept with it; sound made to fit the state
- `dfd64826` 2026-10-07 z3xox, +30 -2: State save / restore on Windows (builtin setjmp, the port's stack switch); one region allocator on both systems; the rewind test passes on the Windows program under Wine
- `6a65ae7a` 2026-10-07 z3xox, +27 -1: Netcode, first step: two copies in lockstep over UDP (gs/net.c), tested on one machine with delay, loss and lag; timing of re-run blanks
- `6130d57b` 2026-10-07 z3xox, +9 -1: Frames that are only re-run draw nothing, show nothing and start no sound (gPortResim); the state test's undone pass runs that way and still leaves the same state and picture
- `c5519c34` 2026-10-07 z3xox, +36 -13: State test rewinding several blanks at a time (BT3_SYNCTEST_DEPTH): passes; after a pad recording ends the pads stay idle (the real controllers were read: the run-to-run difference)
- `8a0dc162` 2026-10-07 z3xox, +7 -0: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences
- `54548c55` 2026-10-06 z3xox, +175 -0: State save / restore (64-bit Linux) and BT3_SYNCTEST, which re-runs every frame from its saved state: it works and shows the file and sound layers are not rewindable yet
- `3d60111b` 2026-10-06 z3xox, +11 -3: Fight checksum in the BT3_HASH log (health, positions, clock, generator) and compare_hash.py --fight
- `4cae46f6` 2026-10-06 z3xox, +139 -0: State checksum (BT3_HASH) with tools to find what differs; the port's low memory at fixed addresses below the program: runs of one program are now identical

## `port/src/gs/tag_hud.cpp` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +9 -1: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +42 -19: Fix disc resources, character previews and HUD controls
- `47b4d577` 2026-10-26 Tag Team Mod contributors, +1 -1: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `101de958` 2026-10-24 Tag Team Mod contributors, +539 -0: Add native Tag Team modes

## `port/src/gs/tag_hud_marker.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +36 -0: Add native Tag Team modes

## `port/src/gs/tag_link.c` ([tag-team](tag-team.md))

- `f0bd6511` 2026-10-31 dfdevx2, +11 -1: Requests: 2 s polls, backoff on 429, none in the background; the relay socket checks the host
- `a974f119` 2026-10-31 dfdevx2, +2 -1: BT4: shim symbols for 0.1.39; tag link: buffers really out of .data
- `96895d21` 2026-10-31 dfdevx2, +2 -1: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan
- `d54095dd` 2026-10-29 Regulus CiberSec, +1069 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/src/gs/tag_link.h` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +33 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/src/gs/tag_net.c` ([tag-team](tag-team.md))

- `fea06c41` 2026-10-31 dfdevx2, +0 -5: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump
- `18dad4b3` 2026-10-29 Regulus CiberSec, +20 -5: Tag Team online: aceita jogos de outra versão do APK ou outra ISO de BT3
- `d54095dd` 2026-10-29 Regulus CiberSec, +98 -3: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +2 -1: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +1303 -0: Add native Tag Team modes

## `port/src/gs/tag_net.h` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +86 -0: Add native Tag Team modes

## `port/src/gs/tag_net_data.c` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +6 -1: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `101de958` 2026-10-24 Tag Team Mod contributors, +125 -0: Add native Tag Team modes

## `port/src/gs/tag_net_ui.cpp` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +10 -1: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +25 -13: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +79 -0: Add native Tag Team modes

## `port/src/gs/tag_text.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +23 -0: Add native Tag Team modes

## `port/src/gs/tag_text_es.inc` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +26 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `3f9ad0fc` 2026-10-28 Regulus CiberSec, +5 -0: Tag Team: polimento do menu do SELECT com as peças do Dragon Net Online
- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +2 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +37 -0: Fix disc resources, character previews and HUD controls
- `02d1e354` 2026-10-26 Tag Team Mod contributors, +2 -2: Simplify the native online menu
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +1 -0: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +732 -0: Add native Tag Team modes

## `port/src/gs/tag_text_pt.inc` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +26 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `3f9ad0fc` 2026-10-28 Regulus CiberSec, +5 -0: Tag Team: polimento do menu do SELECT com as peças do Dragon Net Online

## `port/src/gs/tbo_text.c` ([renderer-gs](renderer-gs.md))

- `8b8f611d` 2026-10-28 Regulus CiberSec, +56 -15: Dragon Net Online: texto novo fora do quadro (fim da queda ao navegar rápido) e crash de textura apagada
- `6e26f543` 2026-10-28 Regulus CiberSec, +56 -15: Dragon Net Online: texto novo fora do quadro (fim da queda ao navegar rápido) e crash de textura apagada
- `c49f5e0f` 2026-10-23 z3xox, +14 -0: The online menus can be translated: translations/port/<language>.json takes their lines (ids tbo/..., looked up by their English), where the menu draws and measures a line (tbo_text.c), joins pieces (Ps_Copy, which also feeds the screen reader) and formats a number (Tb_Fmt). A translation that loses a line's %d parts or {name} is not used, and a lone % in a format line is refused, so a file cannot make the game misread. The talk box decodes UTF-8 (accents were two wrong letters), and the frame count has its own plural line. port/tools/tbo_strings.py makes a file for translators with the English as placeholder (303 lines; --bt4 without BT3's fighter names) and keeps what is translated when run again. Accents checked on screen in Spanish
- `7b99f8ad` 2026-10-15 dfdevx2, +208 -0: Dragon Rage 0.6.0 [skip ci]
- `5ffd592f` 2026-10-14 z3xox, +25 -7: Tenkaichi Budokai Online: the titles in the main menu's heading style
- `76fa4afb` 2026-10-14 z3xox, +190 -0: Tenkaichi Budokai Online: letters from the font list, the keyboard on its keypads, Copy Code

## `port/src/gs/ui.cpp` ([renderer-gs](renderer-gs.md))

- `8e7dac04` 2026-10-28 Regulus CiberSec, +15 -0: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `f12f3fef` 2026-10-27 Androidxxt01, +2 -1: Show matchmaking server with lobby code
- `4dc3964c` 2026-10-27 Regulus CiberSec, +1 -1: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +1 -1: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `6b69eb72` 2026-10-28 z3xox, +59 -123: 0.1.39: lobbies replace rooms. Tenkaichi Budokai Online has no Create Room: the list is Quick Match, Ranked Match, Lobby, Join (Join Room renamed, the one place for a code), the SOON ones and Profile. Lobby opens New Lobby: Open Lobby (a code) or Host by Address (Port_LobbyCreate on the port net_port, L1 / R1 by a hundred); the lobby's page shows the port and this PC's LAN address where a code would be, square copies "address:port". A new lobby starts with the rules its owner last set on a lobby's page (Tb_LobbyKeepRules, in Create Room's settings: battle with DP 10 / 15 / 20, time, game speed, rollback, delay), so nothing that could be chosen is lost. No Watch on Join or Join by Address: a spectator joins the lobby; JOIN on Join by Address is Port_LobbyJoin and opens the lobby's page at once. Dragon Net Battle (F1 > Online > Advanced) opens and joins lobbies too (Host: a lobby with a code or on its port and the tab's rules; Join: a code or an address), no room of two and no Watch; F1 > Online loses "Let others watch your matches" (rooms only). PORT_RELEASE 139, NET_VERSION 6: a 0.1.38 game is never paired (the service's queues and codes are kept apart by it); by address a member joining a typed address that has not answered for two seconds also sends a spectator's greeting too short to be one, which a 0.1.38 host of two answers with SPEC_NO_VERSION ("The game at that address is a different version"), and a lobby's owner refusing another version notes it once a minute. FP_PEER_REQUIRED is 1: a peer, spectator or member without a fingerprint is refused. Update checks: v0.1.39 and v0.1.39.1 newer than 0.1.38.9 (44 checks).
- `d73b404d` 2026-10-27 Androidxxt01, +2 -1: Show matchmaking server with lobby code
- `a3cf1192` 2026-10-28 z3xox, +42 -5: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `0914c6d2` 2026-10-27 z3xox, +82 -81: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.
- `0ca19719` 2026-10-27 z3xox, +270 -123: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `d01e8270` 2026-10-14 jllacchua, +91 -19: Auto-updater: in-game release download, progress bar, extraction and restart
- `79fb9436` 2026-10-27 z3xox, +37 -8: Online: whose data may meet. A quick match (strangers) needs the original disc's data, in every menu: Port_LobbyStartQuick refuses it before the matchmaking service is asked ("Quick match needs the original disc's data, and your game data are not the disc's (what differs). With friends ... you can play when both have the same data."), Dragon Net Battle's Quick match tab says so before the search and greys its button, and Tenkaichi Budokai Online's "Cannot Go Online" is now "No Quick Match", for Quick Match only. With friends (a room code, a lobby, an address, watching) the same data on both sides is enough, modified or not: Tenkaichi Budokai Online's Create Room, Join Room and Lobby no longer refuse modified data. No game connects before its own gameplay fingerprint is known: a start asked for earlier waits (lobby state 6, "Checking your game data..."; a lobby's create and join commands stay queued; net_start holds the picture), and a run that computes none (BT3_GS=none, or online play asked before the data check began) starts it (Port_DataFingerprintNeed; PortFp_Run runs once). A peer that sends no fingerprint: 0.1.37 cannot be told from 0.1.37.1 to 0.1.38.9 by its greetings (NET_VERSION 5, the same capability word; rel 137 at the service), so FP_PEER_REQUIRED decides: 0 for now (MIN_RELEASE is 137), 1 with MIN_RELEASE 138, which refuses such a player, spectator or lobby member. BT3_TEST_FPRINT now acts in plat_fprint.c; BT3_TEST_FPRINT=off sends none (as 0.1.37); BT3_TEST_FP_REQUIRED=1; BT3_UI_NET_TAB opens the online window on a tab. Tested with the local Worker: quick match original against original (BT3_LOBBY and Tenkaichi Budokai Online's screens), a claimed fingerprint refused before the service (both menus show it); by address and room code the same fake fingerprint plays and different ones are refused, after waiting for the fingerprint at the first blank; spectators and lobby members with other data refused, with the same let in; with FP_PEER_REQUIRED a player and a member without a fingerprint refused; 0.1.38.9 by address both ways. Windows builds.
- `cf2c0fb3` 2026-10-27 Regulus CiberSec, +13 -0: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `c86573f2` 2026-10-27 Regulus CiberSec, +1 -0: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `a96bd6f8` 2026-10-27 Regulus CiberSec, +13 -0: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `9cd11a1b` 2026-10-27 Regulus CiberSec, +1 -0: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +0 -2: Fix disc resources, character previews and HUD controls
- `47b4d577` 2026-10-26 Tag Team Mod contributors, +11 -4: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `18d6fd06` 2026-10-26 z3xox, +17 -418: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `ef2b70e2` 2026-10-26 z3xox, +435 -7: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.
- `a7e5238c` 2026-10-26 z3xox, +138 -4: Spectators: a third game (and up to six) can watch a match, by the host's address. A session is computed from its first blank out of nothing but the two players' input, so the host sends a spectator the match's set-up (rules and speed, seed, both players' options and names) and then the input of both players for every blank, as it has it confirmed (never a guess): the spectator's copy computes the same match, character select, battles and rematches included, about a second behind (it waits for a second's worth when it runs out, and runs blanks without picture when it is more than two seconds behind). The host keeps the whole match's input (36 bytes a blank, a byte for 128 unchanged blanks in the packets), so a spectator can come in mid-match and catch up from the start. Nothing waits for spectators: the host sends what is due with its looks at the socket, at most two catch-up packets every 16 ms between them all, and forgets one that is silent for 8 s or still a minute behind after a minute. The host's check values travel with the input and the spectator compares them with its own; one that comes apart says so and follows the match again from the start, once. New packet types 20 to 28, which a 0.1.38.7 ignores: an old host never answers, and the spectator says what that may mean after 10 s; nothing between the two players changed (NET_VERSION 5). The view is the spectator's: player 1's, player 2's, or both halves as the game draws its two-player battle (V or R1; it is drawing only: the fight's values stay the same). In the online window: "Watch the match" on the Join tab, "Let others watch the match" on the Host tab (setting net_spectators); the players see who watches, the spectator a line with the match, the view and the keys, and F1 has Change view / Stop watching. Tests: BT3_LOBBY=spectate:<address>:<port>, BT3_NET_SPECTATE with BT3_NET_SESSION, BT3_LOBBY_AT, BT3_SPEC_VIEWS, BT3_SPEC_LEAVE, BT3_NET_SAVES (the session's save folder), BT3_SHOT_DIR (screenshots of several copies, named by blank too). A program started as the session (tests) now exits when it ends instead of loading a state it never kept.
- `0c7ed754` 2026-10-25 dfdevx2, +1 -0: Dragon Net: back off on HTTP 429
- `101de958` 2026-10-24 Tag Team Mod contributors, +38 -5: Add native Tag Team modes
- `1e55403b` 2026-10-24 z3xox, +5 -0: Four settings are written to the file when they change: the Square clash's worth per press (from a player: it did not keep after closing the game), Low input lag, the game speed (when its slider is let go, and Reset) and Music follows game speed. They were only marked changed, and kept only when another setting was written after them in the same run
- `b83477ac` 2026-10-22 z3xox, +2 -1: "Low input lag" tooltip: it works with Smooth motion too (since 1434251), it is only not used online. With the render thread as without it (2x, offscreen): the in-between picture 4.7 ms after the read, the tick's own 21.3, 16.68 ms apart
- `c8d28cd8` 2026-10-22 victorgonl, +11 -47: Fix custom stage and song name rendering
- ... e mais 114 (`git log -- port/src/gs/ui.cpp`)

## `port/src/gs/ui.h` ([renderer-gs](renderer-gs.md))

- `8e7dac04` 2026-10-28 Regulus CiberSec, +2 -0: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `47b4d577` 2026-10-26 Tag Team Mod contributors, +2 -1: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `d7bb04e8` 2026-10-22 z3xox, +2 -1: Settings, Effects, Weather: Full or Light. With Weather on, a choice below it: Light draws half of the haze's sprites (fx_off bit 128), a thinner haze for about half of what switching it off gains: Desert at noon at 3x on an integrated chip 57.2 pictures a second (Full 52.6, off 62.6), Rocky Area at noon 61.3 (56.8, 67.7). The fight the same either way
- `95fd1797` 2026-10-22 z3xox, +1 -1: Settings, Effects: Weather. The dust, snow and clouds some stages have drifting over them (the stage's weather record, EftWeather: Desert and Rocky Area at noon, Rocky Area in the evening, Kami's Lookout, Glacier, Muscle Tower, the Time Chamber) are 12 to 30 big soft sprites, up to 400 units across, blended over much of the picture: the reason those stages are heavier than the others. Off, the particles go on moving and are not drawn (fx_off bit 64). Desert at noon at 3x on an integrated chip 61 pictures a second instead of 50, Rocky Area at noon 65 instead of 54; the fight the same either way
- `236caf0a` 2026-10-22 z3xox, +1 -1: Settings, Effects: Weather. The dust, snow and clouds some stages have drifting over them (the stage's weather record, EftWeather: Desert and Rocky Area at noon, Rocky Area in the evening, Kami's Lookout, Glacier, Muscle Tower, the Time Chamber) are 12 to 30 big soft sprites, up to 400 units across, blended over much of the picture: the reason those stages are heavier than the others. Off, the particles go on moving and are not drawn (fx_off bit 64). Desert at noon at 3x on an integrated chip 61 pictures a second instead of 50, Rocky Area at noon 65 instead of 54; the fight the same either way
- `968794bf` 2026-10-21 z3xox, +1 -0: Glow: its pages at the PS2's resolution only while the glow chain runs (GfxPost_DrawGlow marks its start and end in the display list: markers 0x20 and 0x21); the fighters' shadow picture, the lens flare's, the heat haze's and the stage blur's use of the same pages at the internal resolution again; F1 > Effects > Sharp stage blur (on by default): those passes between work buffers sampled at the internal resolution as before, off as the PS2 samples them
- `618ee826` 2026-10-21 z3xox, +2 -1: Glow and glare: their own strengths in F1 > Effects (the glow of characters and objects, the sky's glare); the chain the game runs for both is told apart by its tint back up into the 128 x 128 page (the glare's grey has alpha 0); a saved glow strength is the glare's too until it has its own
- `7b99f8ad` 2026-10-15 dfdevx2, +33 -2: Dragon Rage 0.6.0 [skip ci]
- `a103b16f` 2026-10-15 victorgonl, +1 -0: Add support for battle texts fonts
- `f2b014d9` 2026-10-14 victorgonl, +1 -1: Show GPU being used
- `b330baf9` 2026-10-13 jllacchua, +3 -0: Online chat: in-game text overlay between players with T key
- `f6265282` 2026-10-13 z3xox, +15 -0: Controls: the game's vibration reaches the controller (on / off and strength per player), a round deadzone per stick ("Original" unless set), and the sticks and triggers shown live
- `2e93dc19` 2026-10-12 victorgonl, +1 -0: Add support for shared fonts in dialogue and UI (text not editable)
- `2950c9d2` 2026-10-12 victorgonl, +1 -0: Add native and editable menu text with selectable fonts
- `2c4a57ae` 2026-10-11 Renzo, +0 -7: Add QoL settings to skip intro and logos
- `96f490c5` 2026-10-11 z3xox, +1 -0: A fight's display in a wide picture: at the edges, at the edges with long health bars (pull request #28), or in the middle as in 4:3
- `c6e5f78d` 2026-10-11 z3xox, +1 -1: Volume: voices and stage sounds get their own sliders, apart from the sound effects
- `074f44ca` 2026-10-11 z3xox, +3 -0: Frame skipping: a tick's picture is left out when the game is behind its grid, instead of the game slowing down (setting frame_skip, on by default)
- `161a7632` 2026-10-11 z3xox, +6 -0: The settings window with a controller (back, tabs, any pad), Esc asks before the game closes, an Exit game button; Music no longer turns the voices down; a V-sync option; fewer in-between pictures on a machine that cannot show them in time; depth as whole numbers per pixel, as on the GS (a waterfall's missing triangle)
- `c3b7c86c` 2026-10-11 dfdevx2, +3 -0: Dragon Rage 0.4.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +6 -2: WIP: patch 1 and touch artwork
- `80e0dd42` 2026-10-11 z3xox, +4 -1: The settings window opens from a controller: START + SELECT, and a key and a button of its own in the Controls list; the window is worked with the controller and keeps it from the game while open
- `4b8e04b1` 2026-10-10 z3xox, +1 -1: The Effects tab: "Movement blur" switches the game's blur of the scenery while the view turns
- `dfed0616` 2026-10-10 z3xox, +1 -0: The setting "2D filtering": Sharp (console) or Smooth, smooth unless set
- `738157dd` 2026-10-09 Renzo, +7 -0: .
- ... e mais 10 (`git log -- port/src/gs/ui.h`)

## `port/src/gs/ui_android.inc` ([renderer-gs](renderer-gs.md))

- `8e7dac04` 2026-10-28 Regulus CiberSec, +17 -1: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `9285f417` 2026-10-27 Androidxxt01, +3 -1: Show server beside Android lobby code
- `a2deb142` 2026-10-27 Androidxxt01, +3 -1: Show server beside Android lobby code
- `cf2c0fb3` 2026-10-27 Regulus CiberSec, +61 -3: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `c86573f2` 2026-10-27 Regulus CiberSec, +3 -3: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `a96bd6f8` 2026-10-27 Regulus CiberSec, +61 -3: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `9cd11a1b` 2026-10-27 Regulus CiberSec, +3 -3: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `5707f85b` 2026-10-22 dfdevx2, +276 -1: Fixes from logs, touch pause menu, BT4 menu
- `f11e5f18` 2026-10-18 dfdevx2, +1 -336: Dragon Rage 0.8.0: Dragon Net
- `1867006b` 2026-10-17 dfdevx2, +379 -30: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +725 -0: Dragon Rage 0.6.0 [skip ci]

## `port/src/gs/ui_dragonnet.inc` ([online](online.md))

- `4dc3964c` 2026-10-27 Regulus CiberSec, +3 -3: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +3 -3: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `cf2c0fb3` 2026-10-27 Regulus CiberSec, +11 -10: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `c86573f2` 2026-10-27 Regulus CiberSec, +94 -9: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `a96bd6f8` 2026-10-27 Regulus CiberSec, +11 -10: BT3 Android: botões de lobby e espectador no menu de pausa, R1 do toque troca a câmera, escolhas salvas
- `9cd11a1b` 2026-10-27 Regulus CiberSec, +94 -9: Dragon Net Battle do Android com os recursos novos do PC 0.1.38.9
- `0c7ed754` 2026-10-25 dfdevx2, +8 -1: Dragon Net: back off on HTTP 429
- `3ca419c7` 2026-10-19 dfdevx2, +1 -1: Dragon Rage 0.8.1
- `f11e5f18` 2026-10-18 dfdevx2, +889 -0: Dragon Rage 0.8.0: Dragon Net

## `port/src/gs/ui_text_extract.c` ([renderer-gs](renderer-gs.md))

- `979210ac` 2026-10-27 victorgonl, +37 -13: Expand translation extraction to story mode text
- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes
- `2d8a5a72` 2026-10-18 dfdevx2, +434 -0: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +196 -12: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `d290443b` 2026-10-17 z3xox, +27 -2: Translations: the battle HUD and the pause menu from the European disc, and its reworded lines by position
- `316b567c` 2026-10-16 victorgonl, +225 -0: Add support for dialogue and ui texts translations (no documentation yet)

## `port/src/gs/ui_text_extract.h` ([renderer-gs](renderer-gs.md))

- `979210ac` 2026-10-27 victorgonl, +2 -2: Expand translation extraction to story mode text
- `2d8a5a72` 2026-10-18 dfdevx2, +15 -0: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +7 -0: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `316b567c` 2026-10-16 victorgonl, +8 -0: Add support for dialogue and ui texts translations (no documentation yet)

## `port/src/gs/ui_text_translate.c` ([renderer-gs](renderer-gs.md))

- `979210ac` 2026-10-27 victorgonl, +6 -5: Expand translation extraction to story mode text
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +3 -1: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -3: Add native Tag Team modes
- `2d8a5a72` 2026-10-18 dfdevx2, +409 -0: Dragon Rage 0.7.3
- `67253b22` 2026-10-18 z3xox, +1 -0: Translations: a UI catalog row that cannot be added (a duplicate ID) no longer frees its text twice
- `06754063` 2026-10-18 z3xox, +9 -1: Translations: a JSON string gets a buffer of its own length (Windows refused the catalogs: the memory card box in English)
- `39d7a5f8` 2026-10-17 z3xox, +21 -5: Translations: the translated lines no longer in the saved game state (an online pause menu read "! !" and "dd")
- `316b567c` 2026-10-16 victorgonl, +384 -0: Add support for dialogue and ui texts translations (no documentation yet)

## `port/src/gs/ui_text_translate.h` ([renderer-gs](renderer-gs.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +11 -0: Dragon Rage 0.7.3
- `316b567c` 2026-10-16 victorgonl, +11 -0: Add support for dialogue and ui texts translations (no documentation yet)

## `port/src/gs/update_crypto.c` ([renderer-gs](renderer-gs.md))

- `0914c6d2` 2026-10-27 z3xox, +12 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/src/gs/update_monocypher.c` ([renderer-gs](renderer-gs.md))

- `0914c6d2` 2026-10-27 z3xox, +2 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/src/gs/update_sign.c` ([renderer-gs](renderer-gs.md))

- `0ca19719` 2026-10-27 z3xox, +210 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.

## `port/src/gs/update_sign.h` ([renderer-gs](renderer-gs.md))

- `0ca19719` 2026-10-27 z3xox, +38 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.

## `port/src/headless.c` ([bt3-engine](bt3-engine.md))

- `a3cf1192` 2026-10-28 z3xox, +9 -2: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `3ea60f57` 2026-10-27 z3xox, +15 -0: Tournaments in lobbies. The lobby's owner switches the lobby between Versus and Tournament (a new first rule) and draws a knockout bracket of the places marked ready (2 to 8 humans, the owner too if it is ready): the order is shuffled from a number the owner draws from the system's random source, with byes shared out between the halves when the entrants are not 2, 4 or 8. The owner's game holds the bracket and sends it with the lobby (STATE, now 608 bytes; LOBBY_VERSION 3, so a 0.1.38.9 game is told the versions differ, both ways). Each match is an ordinary lobby match of single battles: the owner's game puts the two in 1P and 2P and starts it, everyone else watches. Every copy in the match notes how its first battle ended (the battle sequence past the fight, held 0.3 s so that a rolled-back K.O. is not taken; Port_BattleOutcome in headless.c), a player says it with each ALIVE, and the owner, when the two agree on a winner, marks it and ends the match; a draw, two different results, one result alone for 20 s or a match ended any other way is played again. An entrant who leaves loses by walkover. A new page, the bracket (TB_BRACKET in port_tbo.c), shows the two halves meeting at the final with gold lines for winners, the next match glowing, byes greyed and the champion's box, and the owner's Start Next Match / New Tournament; it comes up when a tournament is drawn and after each of its matches, and the lobby's page has a Bracket button. All text is in port_tbo.c for the translators. Tested offscreen through a local Worker: 4 and 5 entrants (byes) to a champion with every spectator identical to its host in every match, a walkover, an old release joining either way; Versus lobbies, quick match and rooms of two unchanged (docs/port/README.md, "Tournaments").
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -0: Fix disc resources, character previews and HUD controls
- `18d6fd06` 2026-10-26 z3xox, +23 -0: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `ef2b70e2` 2026-10-26 z3xox, +5 -0: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.
- `101de958` 2026-10-24 Tag Team Mod contributors, +85 -1: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +81 -4: Dragon Rage 0.6.0 [skip ci]
- `dab7e2ff` 2026-10-13 z3xox, +19 -0: Online: a seed per match, so that Random is not the same in every session
- `f5e637be` 2026-10-13 z3xox, +43 -0: Online: a session's start-up no longer follows "Skip logos" / "Skip intro video" (one side with either on reached the character select 537 blanks early and the two games never agreed again); the two copies now compare what they compute and end the match with a message when it differs; netcode version 4
- `660c1a38` 2026-10-12 Valentin Vivaldi, +5 -4: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `c5019a96` 2026-10-12 z3xox, +11 -0: The character reference's model viewer is shown with the wide view, not stretched like the menus around it
- `928b2413` 2026-10-11 dfdevx2, +23 -5: WIP: patch 1 and touch artwork
- `195402b4` 2026-10-10 z3xox, +4 -0: The game says when its data is not the original disc's: a background check of every data file against a table of checksums, then the window title, the log, a notice and the crash report carry it
- `18c32e30` 2026-10-09 z3xox, +19 -5: Online play: the host chooses the battle (single, team, DP battle 10 / 15 / 20) and the time limit in the lobby window; no versus menu in a session; netcode version 3
- `7e09775c` 2026-10-09 z3xox, +1 -6: Revert "Online play: the session opens on the game's own versus menu (single, team or DP battle, the DP limit, the battle settings); netcode version 3"
- `f7408a40` 2026-10-09 z3xox, +4 -4: Revert "Online play: the session opens on the battle type itself (single, team, DP battle); in the list behind it the two rows with COM are greyed and skipped"
- `a8a5d08e` 2026-10-09 z3xox, +4 -4: Online play: the session opens on the battle type itself (single, team, DP battle); in the list behind it the two rows with COM are greyed and skipped
- `df6337f9` 2026-10-09 z3xox, +6 -1: Online play: the session opens on the game's own versus menu (single, team or DP battle, the DP limit, the battle settings); netcode version 3
- `6ca73cbb` 2026-10-08 dfdx047, +21 -0: Android: unlock-all from the app, back button opens the settings; toolchain fix
- `dce2c134` 2026-10-08 dfdx047, +1 -1: Engine on Android arm64: build, loader and game activity
- `a16577da` 2026-10-07 z3xox, +13 -0: Name strips of added stages and songs: read from the game's data folder (BT3_DATA), and drawn under OpenGL too
- `0153b616` 2026-10-06 z3xox, +2 -0: Songs added to the music select: manifest, script, and the name overlay
- `28e86145` 2026-10-06 z3xox, +51 -1: Stages from outside the disc: manifest, install script, styled menu names
- `de855229` 2026-10-07 z3xox, +32 -3: Online session flow: lobby (Host / Join), restart into a synchronised session that opens on the versus character select with everything unlocked, leaving back to the normal game
- `3d60111b` 2026-10-06 z3xox, +23 -2: Fight checksum in the BT3_HASH log (health, positions, clock, generator) and compare_hash.py --fight
- ... e mais 11 (`git log -- port/src/headless.c`)

## `port/src/plat_adxlen_tab.h` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +2724 -0: Dragon Rage 0.6.0 [skip ci]
- `d48b19ba` 2026-10-13 z3xox, +2724 -0: Online: how long a voice line lasts is counted from a table of the original disc's streams, not from the player's own file (a voice mod of another length started the fight on another blank)

## `port/src/plat_crash.c` ([bt3-engine](bt3-engine.md))

- `559f85b2` 2026-10-19 z3xox, +20 -0: Online: a desync is a warning and the match goes on (F1 setting to end it, as before); waits for the other player and slow frames in the log with what caused them, the meter says which; Windows writes bt3_log.txt next to the program
- `7b99f8ad` 2026-10-15 dfdevx2, +59 -0: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +59 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `928b2413` 2026-10-11 dfdevx2, +11 -0: WIP: patch 1 and touch artwork
- `195402b4` 2026-10-10 z3xox, +11 -0: The game says when its data is not the original disc's: a background check of every data file against a table of checksums, then the window title, the log, a notice and the crash report carry it
- `dce2c134` 2026-10-08 dfdx047, +4 -0: Engine on Android arm64: build, loader and game activity
- `66b57ad0` 2026-10-06 z3xox, +2 -1: Windows: no relocation table (a system set to move every program moved it off its fixed address: crash before any output); a plain message if loaded elsewhere; crash report installed first
- `8084caf3` 2026-10-06 z3xox, +101 -22: Windows crash report: vectored handler, no C library, registers, modules and the last start-up step; output unbuffered; test switches
- `f1619b6b` 2026-10-06 z3xox, +44 -0: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed

## `port/src/plat_extras.c` ([bt3-engine](bt3-engine.md))

- `39db4ebd` 2026-10-07 z3xox, +82 -0: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional

## `port/src/plat_fastvec.c` ([bt3-engine](bt3-engine.md))

- `bb212093` 2026-10-07 z3xox, +229 -0: Add and multiply by the processor where that is exact (plat_fastvec.c): single operations and the matrix-times-vector kernel, checked bit for bit against the soft-float code; a re-run frame 30% cheaper

## `port/src/plat_fcache.c` ([bt3-engine](bt3-engine.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +10 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `ac51a1ad` 2026-10-31 dfdevx2, +38 -0: Game files: say why a read failed, read again through a new file object; keep the old model
- `5707f85b` 2026-10-22 dfdevx2, +13 -1: Fixes from logs, touch pause menu, BT4 menu
- `7b99f8ad` 2026-10-15 dfdevx2, +32 -24: Dragon Rage 0.6.0 [skip ci]
- `17dc72dd` 2026-10-13 z3xox, +32 -24: A gameplay fingerprint of the data, printed in the log: what a fight is computed from, never pictures or sound (step 1 of matching online players by their data)
- `d285f5d6` 2026-10-08 Claude, +79 -4: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `755986ea` 2026-10-07 z3xox, +9 -0: Added stages: file ids that are no disc file's (0.1.8 gave a disc stage's model to an added stage's sound bank); stages larger than the game's buffer
- `39db4ebd` 2026-10-07 z3xox, +5 -1: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `0153b616` 2026-10-06 z3xox, +2 -1: Songs added to the music select: manifest, script, and the name overlay
- `28e86145` 2026-10-06 z3xox, +15 -3: Stages from outside the disc: manifest, install script, styled menu names
- `9d7f0cd7` 2026-10-07 z3xox, +2 -0: Roll ring: a state save per blank that keeps only the pages written (write faults on Linux, write watch on Windows); saving 0.74 -> 0.08 ms
- `8a0dc162` 2026-10-07 z3xox, +89 -0: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences

## `port/src/plat_file.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -2: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +2 -0: Dragon Rage 0.6.0 [skip ci]
- `f5e637be` 2026-10-13 z3xox, +2 -0: Online: a session's start-up no longer follows "Skip logos" / "Skip intro video" (one side with either on reached the character select 537 blanks early and the two games never agreed again); the two copies now compare what they compute and end the match with a message when it differs; netcode version 4
- `d285f5d6` 2026-10-08 Claude, +39 -0: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `39db4ebd` 2026-10-07 z3xox, +6 -0: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `0153b616` 2026-10-06 z3xox, +12 -0: Songs added to the music select: manifest, script, and the name overlay
- `28e86145` 2026-10-06 z3xox, +35 -0: Stages from outside the disc: manifest, install script, styled menu names
- `8a0dc162` 2026-10-07 z3xox, +12 -19: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences
- `66e2a7b1` 2026-10-06 z3xox, +4 -2: Port: 64-bit build (game pointers stay 4 bytes: ptr32.py, irfix.py, low memory); replay and heap match the 32-bit build

## `port/src/plat_fprint.c` ([bt3-engine](bt3-engine.md))

- `79fb9436` 2026-10-27 z3xox, +53 -2: Online: whose data may meet. A quick match (strangers) needs the original disc's data, in every menu: Port_LobbyStartQuick refuses it before the matchmaking service is asked ("Quick match needs the original disc's data, and your game data are not the disc's (what differs). With friends ... you can play when both have the same data."), Dragon Net Battle's Quick match tab says so before the search and greys its button, and Tenkaichi Budokai Online's "Cannot Go Online" is now "No Quick Match", for Quick Match only. With friends (a room code, a lobby, an address, watching) the same data on both sides is enough, modified or not: Tenkaichi Budokai Online's Create Room, Join Room and Lobby no longer refuse modified data. No game connects before its own gameplay fingerprint is known: a start asked for earlier waits (lobby state 6, "Checking your game data..."; a lobby's create and join commands stay queued; net_start holds the picture), and a run that computes none (BT3_GS=none, or online play asked before the data check began) starts it (Port_DataFingerprintNeed; PortFp_Run runs once). A peer that sends no fingerprint: 0.1.37 cannot be told from 0.1.37.1 to 0.1.38.9 by its greetings (NET_VERSION 5, the same capability word; rel 137 at the service), so FP_PEER_REQUIRED decides: 0 for now (MIN_RELEASE is 137), 1 with MIN_RELEASE 138, which refuses such a player, spectator or lobby member. BT3_TEST_FPRINT now acts in plat_fprint.c; BT3_TEST_FPRINT=off sends none (as 0.1.37); BT3_TEST_FP_REQUIRED=1; BT3_UI_NET_TAB opens the online window on a tab. Tested with the local Worker: quick match original against original (BT3_LOBBY and Tenkaichi Budokai Online's screens), a claimed fingerprint refused before the service (both menus show it); by address and room code the same fake fingerprint plays and different ones are refused, after waiting for the fingerprint at the first blank; spectators and lobby members with other data refused, with the same let in; with FP_PEER_REQUIRED a player and a member without a fingerprint refused; 0.1.38.9 by address both ways. Windows builds.
- `25bdfba9` 2026-10-27 z3xox, +10 -0: The online screens no longer refuse a game whose data check has not finished. The data check's thread first looked at every one of the disc's ~68,600 files (their sizes and times) and only then computed the gameplay fingerprint, and until then the fingerprint's state was "off", which the Tenkaichi Budokai Online screens take as "could not be checked": on a USB hard disk (exFAT) that pass took long enough that a player opening the online screens got "Cannot Go Online" with the original disc's data (their log had no fingerprint line yet). The fingerprint is now computed first (it reads only the files the fights are computed from, or nothing with its cache), and from the moment the thread is started its state is "being computed", so the screens say to wait. Seen in a start-up log: the gameplay data line now comes before the check of all the files. Windows builds
- `01b94696` 2026-10-19 z3xox, +2 -1: Online: two games with different game data are not matched (the gameplay fingerprint goes to the other side with the greetings; a modified disc image against another one ended as a desync a moment into the fight)
- `7b99f8ad` 2026-10-15 dfdevx2, +1530 -0: Dragon Rage 0.6.0 [skip ci]
- `17dc72dd` 2026-10-13 z3xox, +1530 -0: A gameplay fingerprint of the data, printed in the log: what a fight is computed from, never pictures or sound (step 1 of matching online players by their data)

## `port/src/plat_fprint_tab.h` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +2013 -0: Dragon Rage 0.6.0 [skip ci]
- `17dc72dd` 2026-10-13 z3xox, +2013 -0: A gameplay fingerprint of the data, printed in the log: what a fight is computed from, never pictures or sound (step 1 of matching online players by their data)

## `port/src/plat_game.c` ([bt3-engine](bt3-engine.md))

- `00244a0d` 2026-10-31 dfdevx2, +7 -3: Tag Team online: the costume table is read through the file cache, as the Android install keeps it in the archive
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +6 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +337 -0: Add native Tag Team modes

## `port/src/plat_gs.c` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +2 -1: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +2 -1: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `0ab41362` 2026-10-09 Claude, +5 -0: Android: engine loadable anywhere below 4 GB; registers and stack movable

## `port/src/plat_libm.c` ([bt3-engine](bt3-engine.md))

- `928b2413` 2026-10-11 dfdevx2, +5 -0: WIP: patch 1 and touch artwork
- `322b51eb` 2026-10-10 z3xox, +5 -0: The replay list no longer stops the game: pow, the game's one double maths call, goes through the software-float bridge
- `5ddecbfd` 2026-10-08 z3xox, +4 -4: Drawing: strips as indexed triangles, command data used in place, getenv remembered per place, a faster page hash
- `40a2fa36` 2026-10-06 z3xox, +1 -0: Port: game code builds with clang (BT3_CC=clang), replay and heap match the gcc build

## `port/src/plat_mc.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -0: Add native Tag Team modes
- `9d7f0cd7` 2026-10-07 z3xox, +2 -0: Roll ring: a state save per blank that keeps only the pages written (write faults on Linux, write watch on Windows); saving 0.74 -> 0.08 ms
- `8a0dc162` 2026-10-07 z3xox, +54 -14: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences
- `f1619b6b` 2026-10-06 z3xox, +4 -0: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed

## `port/src/plat_mem.c` ([bt3-engine](bt3-engine.md))

- `caeeffd0` 2026-10-31 dfdevx2, +4 -0: Low memory: a negative size from the disc says modified data, not out of memory
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +14 -2: Add disc-based setup and simplify launchers
- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -3: Add native Tag Team modes
- `2d8a5a72` 2026-10-18 dfdevx2, +27 -0: Dragon Rage 0.7.3
- `7b99f8ad` 2026-10-15 dfdevx2, +146 -17: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +8 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `660c1a38` 2026-10-12 Valentin Vivaldi, +138 -17: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `928b2413` 2026-10-11 dfdevx2, +4 -0: WIP: patch 1 and touch artwork
- `0ab41362` 2026-10-09 Claude, +45 -3: Android: engine loadable anywhere below 4 GB; registers and stack movable
- `0e56e90c` 2026-10-08 Claude, +17 -2: Android: claim the engine's fixed addresses when libmain.so loads
- `d285f5d6` 2026-10-08 Claude, +6 -1: Android: fast install, modified ISOs, pause menu with touch, Odin back button
- `69457fad` 2026-10-08 dfdx047, +16 -0: Android: the engine at 0x03000000 and the port's region below the Java heap
- `dce2c134` 2026-10-08 dfdx047, +52 -16: Engine on Android arm64: build, loader and game activity
- `755986ea` 2026-10-07 z3xox, +30 -0: Added stages: file ids that are no disc file's (0.1.8 gave a disc stage's model to an added stage's sound bank); stages larger than the game's buffer
- `9d7f0cd7` 2026-10-07 z3xox, +13 -4: Roll ring: a state save per blank that keeps only the pages written (write faults on Linux, write watch on Windows); saving 0.74 -> 0.08 ms
- `dfd64826` 2026-10-07 z3xox, +87 -82: State save / restore on Windows (builtin setjmp, the port's stack switch); one region allocator on both systems; the rewind test passes on the Windows program under Wine
- `8a0dc162` 2026-10-07 z3xox, +16 -5: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences
- `54548c55` 2026-10-06 z3xox, +25 -0: State save / restore (64-bit Linux) and BT3_SYNCTEST, which re-runs every frame from its saved state: it works and shows the file and sound layers are not rewindable yet
- `4cae46f6` 2026-10-06 z3xox, +99 -10: State checksum (BT3_HASH) with tools to find what differs; the port's low memory at fixed addresses below the program: runs of one program are now identical
- `66b57ad0` 2026-10-06 z3xox, +19 -0: Windows: no relocation table (a system set to move every program moved it off its fixed address: crash before any output); a plain message if loaded elsewhere; crash report installed first
- `8084caf3` 2026-10-06 z3xox, +15 -0: Windows crash report: vectored handler, no C library, registers, modules and the last start-up step; output unbuffered; test switches
- `dd6b20ce` 2026-10-06 z3xox, +29 -0: Release program opens its window by default and works from its own folder (double click)
- `434b8581` 2026-10-06 z3xox, +2 -2: Release names: Tenkaichi3Decomp for the program, the setup, the archives and the window titles
- `f1619b6b` 2026-10-06 z3xox, +172 -12: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `97a17db3` 2026-10-06 z3xox, +120 -0: Release program without the game's data tables: strip_data.py and the loader that fetches them from the user's disc
- ... e mais 1 (`git log -- port/src/plat_mem.c`)

## `port/src/plat_movie.c` ([bt3-engine](bt3-engine.md))

- `3fbf12d1` 2026-10-07 z3xox, +2 -1: Online session inside the running program: the state at the first blank is kept, a session puts the game back to it and leaving brings the player's own state back (no restart); GS memory kept with it; sound made to fit the state
- `7fd459c6` 2026-10-06 z3xox, +19 -28: Built-in MPEG-2 intra decoder for the two movies (no ffmpeg); verified against ffmpeg on all 7,010 pictures
- `f1619b6b` 2026-10-06 z3xox, +12 -0: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `fe7e90c3` 2026-10-06 z3xox, +2 -1: Port: alpha-test copy refreshed for the shader draw paths too (strip behind READY/FIGHT); movie runs during input playback; notes

## `port/src/plat_mpeg2.c` ([bt3-engine](bt3-engine.md))

- `7fd459c6` 2026-10-06 z3xox, +655 -0: Built-in MPEG-2 intra decoder for the two movies (no ffmpeg); verified against ffmpeg on all 7,010 pictures

## `port/src/plat_settings.c` ([bt3-engine](bt3-engine.md))

- `344aa16c` 2026-10-31 dfdevx2, +1 -1: Tag Team as a setting of the app; the team and the licences in the app
- `2d8a5a72` 2026-10-18 dfdevx2, +1 -1: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +1 -1: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +61 -3: Dragon Rage 0.6.0 [skip ci]
- `f6265282` 2026-10-13 z3xox, +2 -1: Controls: the game's vibration reaches the controller (on / off and strength per player), a round deadzone per stick ("Original" unless set), and the sticks and triggers shown live
- `421adbbe` 2026-10-12 z3xox, +58 -1: Online: the player name is remembered between runs (the settings file takes text)
- `aa36212f` 2026-10-12 z3xox, +1 -1: Audio: "Music follows game speed", off by default: looping music plays as it is at any game speed
- `188e9740` 2026-10-11 z3xox, +1 -1: Cheats: a game speed of one's own, 50% to 200%, outside online play (an online match never reads it)
- `96f490c5` 2026-10-11 z3xox, +1 -1: A fight's display in a wide picture: at the edges, at the edges with long health bars (pull request #28), or in the middle as in 4:3
- `c6e5f78d` 2026-10-11 z3xox, +1 -1: Volume: voices and stage sounds get their own sliders, apart from the sound effects
- `fd1d6497` 2026-10-06 z3xox, +26 -12: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment
- `c3084bcf` 2026-10-06 z3xox, +80 -0: Port: settings saved to bt3_settings.txt

## `port/src/plat_sndstate.c` ([audio](audio.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +5 -0: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes
- `3fbf12d1` 2026-10-07 z3xox, +3 -0: Online session inside the running program: the state at the first blank is kept, a session puts the game back to it and leaving brings the player's own state back (no restart); GS memory kept with it; sound made to fit the state
- `8a0dc162` 2026-10-07 z3xox, +31 -0: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences

## `port/src/plat_songs.c` ([bt3-engine](bt3-engine.md))

- `088ec8c7` 2026-10-21 z3xox, +46 -3: Songs: the .adx an earlier release converted from an MP3 (the converter's mark) is not listed while that MP3 is there, which is now played itself (a player who had added MP3s saw each one twice); a songs.txt line naming such a copy plays the MP3; the folder's note says so, and that a file that cannot be read is silent (it is not left out)
- `109cfbe5` 2026-10-19 Renzo, +30 -45: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion
- `7b99f8ad` 2026-10-15 dfdevx2, +64 -10: Dragon Rage 0.6.0 [skip ci]
- `b5160626` 2026-10-13 z3xox, +42 -11: Added songs: MP3 files are converted in the background, the start does not wait; a song joins the list when a select screen is next set up (at start in a recorded or re-run session, never in an online one)
- `a06b5a5d` 2026-10-13 z3xox, +7 -1: Added songs: the ADX made from an MP3 says so in its header (the MP3 size and time), so that a changed MP3 is converted again whatever the dates, an ADX made by hand is never written over, and an MP3 that cannot be decoded is tried once (mp3-failed.txt); names that give the same ADX, too long, .Mp3 in songs.txt
- `d67cd22c` 2026-10-12 Valentin Vivaldi, +18 -1: Added songs: an .mp3 in the songs folder is converted to ADX at start, once (one that cannot be read is left out, with a notice; the game goes on)
- `928b2413` 2026-10-11 dfdevx2, +24 -3: WIP: patch 1 and touch artwork
- `ca55bb2c` 2026-10-10 z3xox, +14 -0: Added songs: offered in team and DP battles too (the team select's own music list had been left without them); a song without loop points is played round like the disc's music
- `3e2b773d` 2026-10-08 z3xox, +10 -3: Added songs: file numbers past the archive's end (the 5th and 6th were the announcer's first two lines); up to 40
- `39db4ebd` 2026-10-07 z3xox, +63 -20: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `0153b616` 2026-10-06 z3xox, +103 -0: Songs added to the music select: manifest, script, and the name overlay

## `port/src/plat_stages.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -0: Add native Tag Team modes
- `928b2413` 2026-10-11 dfdevx2, +7 -3: WIP: patch 1 and touch artwork
- `a3d30848` 2026-10-08 z3xox, +7 -3: Added stages: file numbers past the archive's end (30000.. are the training guide's voice lines)
- `755986ea` 2026-10-07 z3xox, +87 -7: Added stages: file ids that are no disc file's (0.1.8 gave a disc stage's model to an added stage's sound bank); stages larger than the game's buffer
- `39db4ebd` 2026-10-07 z3xox, +39 -8: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `28e86145` 2026-10-06 z3xox, +142 -0: Stages from outside the disc: manifest, install script, styled menu names

## `port/src/plat_stub.c` ([bt3-engine](bt3-engine.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +1 -0: Add Linux setup and improve native menus and music
- `a7e5238c` 2026-10-26 z3xox, +2 -1: Spectators: a third game (and up to six) can watch a match, by the host's address. A session is computed from its first blank out of nothing but the two players' input, so the host sends a spectator the match's set-up (rules and speed, seed, both players' options and names) and then the input of both players for every blank, as it has it confirmed (never a guess): the spectator's copy computes the same match, character select, battles and rematches included, about a second behind (it waits for a second's worth when it runs out, and runs blanks without picture when it is more than two seconds behind). The host keeps the whole match's input (36 bytes a blank, a byte for 128 unchanged blanks in the packets), so a spectator can come in mid-match and catch up from the start. Nothing waits for spectators: the host sends what is due with its looks at the socket, at most two catch-up packets every 16 ms between them all, and forgets one that is silent for 8 s or still a minute behind after a minute. The host's check values travel with the input and the spectator compares them with its own; one that comes apart says so and follows the match again from the start, once. New packet types 20 to 28, which a 0.1.38.7 ignores: an old host never answers, and the spectator says what that may mean after 10 s; nothing between the two players changed (NET_VERSION 5). The view is the spectator's: player 1's, player 2's, or both halves as the game draws its two-player battle (V or R1; it is drawing only: the fight's values stay the same). In the online window: "Watch the match" on the Join tab, "Let others watch the match" on the Host tab (setting net_spectators); the players see who watches, the spectator a line with the match, the view and the keys, and F1 has Change view / Stop watching. Tests: BT3_LOBBY=spectate:<address>:<port>, BT3_NET_SPECTATE with BT3_NET_SESSION, BT3_LOBBY_AT, BT3_SPEC_VIEWS, BT3_SPEC_LEAVE, BT3_NET_SAVES (the session's save folder), BT3_SHOT_DIR (screenshots of several copies, named by blank too). A program started as the session (tests) now exits when it ends instead of loading a state it never kept.
- `1e55403b` 2026-10-24 z3xox, +2 -0: Four settings are written to the file when they change: the Square clash's worth per press (from a player: it did not keep after closing the game), Low input lag, the game speed (when its slider is let go, and Reset) and Music follows game speed. They were only marked changed, and kept only when another setting was written after them in the same run
- `b84a5461` 2026-10-23 z3xox, +7 -1: Windows: CreateWaitableTimerExW is looked up at run time, as SetProcessInformation already is: the release build's older headers do not declare it (the 0.1.38.2 build stopped there), and Windows before 10 does not have it (then the wait is as before)
- `e50d6137` 2026-10-22 z3xox, +20 -4: BT3_LAG_LOG measures more of the way from a press to the screen: "lag P" with how old a press is when the game first reads it (an event watch notes each button or key press's own time), and "lag G" when the graphics chip has finished a frame handed over (its fence, waited for by a thread of its own). With the render thread and without it, offscreen at 2x on an integrated chip: pad read to its picture 33.36 ms, the chip done 7.3 ms later, the same in both
- `e21b3737` 2026-10-22 z3xox, +10 -0: native-fx: with the render thread, no more input lag than without it when the game has time to spare. The game read the pads right after handing its list over while the thread drew it, so what it read was older by the drawing's time when its picture came: 35.8 ms from the pad read to its picture instead of 33.4 (2x, an integrated chip). Now the pad read waits for the thread to have drawn the list and shown its first picture (Gs_RenderListWait; with Smooth motion not for the pictures after it) when the last vertical blank was waited for; a game that is behind (or uncapped) does not wait, and keeps the work side by side. 33.4 ms with and without the thread; uncapped at 1x 337 pictures a second against 256 as before; Smooth motion as even
- `f8ce6e00` 2026-10-22 z3xox, +45 -7: Windows: the game asks not to be throttled and waits for the blanks with the high-resolution timer. Windows 11 throttles a process it takes for background work (EcoQoS): on processors with efficiency cores its threads may be put there, and its timer requests are ignored while its window is covered; reported: the efficiency cores at 100% while the game stuttered (an i7-12700KF). The process opts out of both (SetProcessInformation, ProcessPowerThrottling, looked up at run time). The wait for a blank used Sleep to within 1.5 ms and Sleep(0) for the rest, a core busy for a tenth of every blank; now the high-resolution waitable timer (Windows 10 1803 and later) to within 0.4 ms and a spin with the processor's pause, the old way where the timer is missing. Under Wine: the blanks 16.67 to 16.69 ms apart
- `2d8a5a72` 2026-10-18 dfdevx2, +14 -2: Dragon Rage 0.7.3
- `1434251c` 2026-10-15 z3xox, +14 -2: Low input lag with Smooth motion too: the owed blanks show the tick's in-between pictures (measured at 60: own picture 50 -> 20 ms after the read, first picture 33 -> 4 ms; the same-game check not finished)
- `7b99f8ad` 2026-10-15 dfdevx2, +166 -7: Dragon Rage 0.6.0 [skip ci]
- `cc012f78` 2026-10-15 z3xox, +4 -0: Side bars: Fighter panels (a contributor's patch): in a 4:3 fight in a wide window, each side's portrait, name and form; fonts from the game's list (fonts.tsv), the portrait decoder bounded
- `b1f33b98` 2026-10-14 z3xox, +82 -1: Low input lag (experimental, off by default): the blanks a tick waits for are waited for before its pad read, not after its computing; BT3_LAG_LOG measures a tick
- `55250225` 2026-10-13 piapenia, +17 -1: port: controller vibration through SDL
- `0aa94971` 2026-10-11 piapenia, +4 -0: Screen reader: the menus speak through Prism, off unless prism.dll is beside the program
- `f6265282` 2026-10-13 z3xox, +11 -1: Controls: the game's vibration reaches the controller (on / off and strength per player), a round deadzone per stick ("Original" unless set), and the sticks and triggers shown live
- `660c1a38` 2026-10-12 Valentin Vivaldi, +2 -1: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `40f5f520` 2026-10-12 z3xox, +17 -0: Menus in 4:3: an option to show the menus at their own shape in a wider picture instead of stretched (fights unchanged)
- `188e9740` 2026-10-11 z3xox, +28 -2: Cheats: a game speed of one's own, 50% to 200%, outside online play (an online match never reads it)
- `86110097` 2026-10-11 z3xox, +8 -3: Online matches at 120% speed, the competitive standard: every quick match, and the host's choice with a room (the pace of the vertical blanks and of the sound; only when both copies know it)
- `ba71e7f6` 2026-10-11 z3xox, +5 -1: The picture's shape is the player's setting, not game state: it was put back with the game when an online session started (a match reset the screen to 4:3)
- `074f44ca` 2026-10-11 z3xox, +7 -0: Frame skipping: a tick's picture is left out when the game is behind its grid, instead of the game slowing down (setting frame_skip, on by default)
- `928b2413` 2026-10-11 dfdevx2, +63 -5: WIP: patch 1 and touch artwork
- `021fc2fc` 2026-10-10 z3xox, +16 -2: The cross-fade between the shots of a fight's intro: the game's read-back of the frame is answered (software renderer and both GPU back ends)
- `089995e1` 2026-10-10 z3xox, +18 -0: The fight's views follow the picture's shape when it is changed during a fight (their projection was only made at the fight's start: the picture was stretched until the next one)
- `3a361aa6` 2026-10-09 z3xox, +4 -0: Online play: a relay for room codes. The host gets an address at Cloudflare's TURN service and posts it with its others; the one who joins tries it when the direct ways stay silent
- ... e mais 26 (`git log -- port/src/plat_stub.c`)

## `port/src/plat_verify.c` ([bt3-engine](bt3-engine.md))

- `25bdfba9` 2026-10-27 z3xox, +10 -6: The online screens no longer refuse a game whose data check has not finished. The data check's thread first looked at every one of the disc's ~68,600 files (their sizes and times) and only then computed the gameplay fingerprint, and until then the fingerprint's state was "off", which the Tenkaichi Budokai Online screens take as "could not be checked": on a USB hard disk (exFAT) that pass took long enough that a player opening the online screens got "Cannot Go Online" with the original disc's data (their log had no fingerprint line yet). The fingerprint is now computed first (it reads only the files the fights are computed from, or nothing with its cache), and from the moment the thread is started its state is "being computed", so the screens say to wait. Seen in a start-up log: the gameplay data line now comes before the check of all the files. Windows builds
- `101de958` 2026-10-24 Tag Team Mod contributors, +15 -8: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +30 -5: Dragon Rage 0.6.0 [skip ci]
- `17dc72dd` 2026-10-13 z3xox, +30 -5: A gameplay fingerprint of the data, printed in the log: what a fight is computed from, never pictures or sound (step 1 of matching online players by their data)
- `928b2413` 2026-10-11 dfdevx2, +275 -0: WIP: patch 1 and touch artwork
- `bc047812` 2026-10-10 z3xox, +31 -2: The data check: a size-only pass first, and the first difference is known at once (a romhack's data stopped the game a second after its start, before the count ended); a crash report during the check says so
- `195402b4` 2026-10-10 z3xox, +239 -0: The game says when its data is not the original disc's: a background check of every data file against a table of checksums, then the window title, the log, a notice and the crash report carry it

## `port/src/plat_verify_tab.h` ([bt3-engine](bt3-engine.md))

- `928b2413` 2026-10-11 dfdevx2, +11441 -0: WIP: patch 1 and touch artwork
- `195402b4` 2026-10-10 z3xox, +11441 -0: The game says when its data is not the original disc's: a background check of every data file against a table of checksums, then the window title, the log, a notice and the crash report carry it

## `port/src/port_addr.h` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +31 -0: Dragon Rage 0.6.0 [skip ci]
- `570c097c` 2026-10-13 z3xox, +4 -6: macOS: Port_Ptr and Port_Addr are the plain casts themselves on Linux and Windows (macros: the same code as before the names)
- `660c1a38` 2026-10-12 Valentin Vivaldi, +33 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal

## `port/src/port_host.h` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +4 -0: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +4 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `8a0dc162` 2026-10-07 z3xox, +8 -0: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences

## `port/src/port_release.h` ([bt3-engine](bt3-engine.md))

- `6b69eb72` 2026-10-28 z3xox, +1 -1: 0.1.39: lobbies replace rooms. Tenkaichi Budokai Online has no Create Room: the list is Quick Match, Ranked Match, Lobby, Join (Join Room renamed, the one place for a code), the SOON ones and Profile. Lobby opens New Lobby: Open Lobby (a code) or Host by Address (Port_LobbyCreate on the port net_port, L1 / R1 by a hundred); the lobby's page shows the port and this PC's LAN address where a code would be, square copies "address:port". A new lobby starts with the rules its owner last set on a lobby's page (Tb_LobbyKeepRules, in Create Room's settings: battle with DP 10 / 15 / 20, time, game speed, rollback, delay), so nothing that could be chosen is lost. No Watch on Join or Join by Address: a spectator joins the lobby; JOIN on Join by Address is Port_LobbyJoin and opens the lobby's page at once. Dragon Net Battle (F1 > Online > Advanced) opens and joins lobbies too (Host: a lobby with a code or on its port and the tab's rules; Join: a code or an address), no room of two and no Watch; F1 > Online loses "Let others watch your matches" (rooms only). PORT_RELEASE 139, NET_VERSION 6: a 0.1.38 game is never paired (the service's queues and codes are kept apart by it); by address a member joining a typed address that has not answered for two seconds also sends a spectator's greeting too short to be one, which a 0.1.38 host of two answers with SPEC_NO_VERSION ("The game at that address is a different version"), and a lobby's owner refusing another version notes it once a minute. FP_PEER_REQUIRED is 1: a peer, spectator or member without a fingerprint is refused. Update checks: v0.1.39 and v0.1.39.1 newer than 0.1.38.9 (44 checks).
- `0ca19719` 2026-10-27 z3xox, +4 -3: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `d01e8270` 2026-10-14 jllacchua, +1 -0: Auto-updater: in-game release download, progress bar, extraction and restart
- `aec1f8af` 2026-10-21 z3xox, +1 -1: Release 0.1.38
- `2d8a5a72` 2026-10-18 dfdevx2, +1 -1: Dragon Rage 0.7.3
- `ec3c85a9` 2026-10-18 z3xox, +1 -1: Release 0.1.37
- `1867006b` 2026-10-17 dfdevx2, +1 -1: Dragon Rage 0.7.0
- `3449af26` 2026-10-17 z3xox, +1 -1: Release 0.1.36
- `7b99f8ad` 2026-10-15 dfdevx2, +11 -0: Dragon Rage 0.6.0 [skip ci]
- `79da0420` 2026-10-14 z3xox, +1 -1: Release 0.1.35
- `c1bd045b` 2026-10-13 z3xox, +1 -1: Release 0.1.34
- `420e055d` 2026-10-12 z3xox, +1 -1: Release 0.1.33
- `6e08d7c5` 2026-10-12 z3xox, +4 -0: An update notice: the newest release is asked of GitHub once per start; a line over the picture and a button to the release page in the settings window ("Check for updates" turns it off)
- `c5d29170` 2026-10-12 z3xox, +1 -1: Release 0.1.32
- `fff151eb` 2026-10-12 z3xox, +1 -1: Release 0.1.31
- `597fd7ce` 2026-10-12 z3xox, +1 -1: Release 0.1.30
- `19360c64` 2026-10-11 z3xox, +7 -0: The matchmaking service can refuse releases older than one its keeper names (MIN_RELEASE); the game says which release it is (port_release.h, raised with each release)

## `port/src/quit_dialog.c` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +37 -6: Dragon Rage 0.7.3
- `71b1f103` 2026-10-16 victorgonl, +37 -6: Add support for port content text translations (need documentation)
- `7b99f8ad` 2026-10-15 dfdevx2, +159 -0: Dragon Rage 0.6.0 [skip ci]
- `f2b701ac` 2026-10-15 z3xox, +6 -2: Exit question: its animation at the menus speed in a fight too (a fight frame lasts two blanks: the window played at half speed)
- `65086467` 2026-10-14 victorgonl, +155 -0: Quit game dialog using the game’s UI

## `port/src/quit_dialog.h` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +26 -0: Dragon Rage 0.6.0 [skip ci]
- `f2b701ac` 2026-10-15 z3xox, +1 -1: Exit question: its animation at the menus speed in a fight too (a fight frame lasts two blanks: the window played at half speed)
- `65086467` 2026-10-14 victorgonl, +26 -0: Quit game dialog using the game’s UI

## `port/src/softfloat_ps2.c` ([bt3-engine](bt3-engine.md))

- `41760bb6` 2026-10-06 z3xox, +3 -149: Speed: vertex unpack as a copy, table-driven texture uploads, faster page hash, float core inlined into the vector library (fight frame work more than halved)

## `port/src/softfloat_ps2_inl.h` ([bt3-engine](bt3-engine.md))

- `bb212093` 2026-10-07 z3xox, +21 -0: Add and multiply by the processor where that is exact (plat_fastvec.c): single operations and the matrix-times-vector kernel, checked bit for bit against the soft-float code; a re-run frame 30% cheaper
- `41760bb6` 2026-10-06 z3xox, +163 -0: Speed: vertex unpack as a copy, table-driven texture uploads, faster page hash, float core inlined into the vector library (fight frame work more than halved)

## `port/src/spec_menu.c` ([bt3-engine](bt3-engine.md))

- `6f99aff2` 2026-10-26 z3xox, +7 -2: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `a581c3f7` 2026-10-26 z3xox, +428 -0: A spectator's own pause menu is the battle's own pause menu: its box from the battle's sprite sheet, drawn by its engine (BtlMenu_Draw, src/battle/hud_0_b.c, through BtlMenu_PortDrawMenu), its colours, sounds and six-frame growing box, and the versus pause menu's own "EXIT ?" Yes / No box before leaving (PauseMenu_PortConfirmBox, src/battle/hud_0.c), instead of a Tenkaichi Budokai Online panel. Its items (port/src/spec_menu.c): the note "Your screen only: the fight goes on" in the title's row, Continue, View: Player 1 / Player 2 / Split Screen with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching; their texts reach the engine through BtlMenu_GetItemText (numbers 0x6FFF and on) and are translated by the online menus' file (tbo_strings.py lists the file). The engine's update is not run and the battle's menus are not touched: the file moves the cursor and the box itself and reads the PC's pad (Port_PadRead); in a fight only (gPortBtlMenuLive). Tested by code with an owner and a member watching and every item used with the pad: the fight values of both spectators and both players identical, no check value differed. All game-side changes under PORT: the PS2 build is unchanged.

## `port/src/tagteam_trace.c` ([tag-team](tag-team.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +5 -2: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +4 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +5 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +225 -0: Add native Tag Team modes

## `port/src/update_key.h` ([bt3-engine](bt3-engine.md))

- `fe01dd57` 2026-10-28 z3xox, +1 -1: Automatic update: the release signing key's public half. Updates are installed only when their zip is signed with the matching private key (the repository secret UPDATE_SIGNING_KEY)
- `0914c6d2` 2026-10-27 z3xox, +29 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/src/vu0_names.h` ([bt3-engine](bt3-engine.md))

- `08321034` 2026-10-06 z3xox, +1 -0: ObjSeam_TransformVtx written (instruction-for-instruction, with a check against the original instruction words): characters with seam models no longer stop the game

## `port/tagnet-worker/README.md` ([bt3-engine](bt3-engine.md))

- `41a8fa3d` 2026-10-29 Regulus CiberSec, +11 -2: Tag Team Online: o serviço publicado (endereço, banco, relay por Durable Object) e como republicar
- `d54095dd` 2026-10-29 Regulus CiberSec, +38 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/tagnet-worker/schema.sql` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +27 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/tagnet-worker/test.mjs` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +60 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/tagnet-worker/worker.js` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +289 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/tagnet-worker/wrangler.example.jsonc` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +11 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/tbo/ui/CREDITS.md` ([bt3-engine](bt3-engine.md))

- `9bed1539` 2026-10-27 Regulus CiberSec, +8 -1: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `5d3eea11` 2026-10-27 Regulus CiberSec, +8 -1: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `7b99f8ad` 2026-10-15 dfdevx2, +12 -0: Dragon Rage 0.6.0 [skip ci]
- `6070f0bf` 2026-10-15 z3xox, +12 -0: TBO: its pictures in the repository (port/tbo/ui, with their credits: the announcer by MarshallPunk, the buttons by pluantoine under CC BY 4.0), read there in a source checkout and packaged as tbo/ui; tbo_sizes.py makes tbo_sizes.h (the new background size)

## `port/tests/bt4_animation.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +16 -1: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +45 -0: Add native Tag Team modes

## `port/tests/bt4_custom_save.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +50 -0: Add native Tag Team modes

## `port/tests/bt4_model_body.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +30 -0: Add native Tag Team modes

## `port/tests/bt4_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +149 -0: Add native Tag Team modes

## `port/tests/cpu_difficulty_memory_runtime.py` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +98 -0: Fix disc resources, character previews and HUD controls

## `port/tests/disc_menu_resources_runtime.py` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +76 -0: Fix disc resources, character previews and HUD controls

## `port/tests/effect_vram.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +79 -0: Add native Tag Team modes

## `port/tests/evolution_z_runtime.py` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +72 -0: Fix disc resources, character previews and HUD controls

## `port/tests/fullscreen_runtime.py` ([bt3-engine](bt3-engine.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +106 -0: Add branded Play launcher, fullscreen controls and Mod Settings tab

## `port/tests/menu_font/fuzz.c` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +10 -3: Dragon Rage 0.7.3
- `316b567c` 2026-10-16 victorgonl, +2 -1: Add support for dialogue and ui texts translations (no documentation yet)
- `5b3f71f3` 2026-10-16 victorgonl, +8 -2: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +324 -0: Dragon Rage 0.6.0 [skip ci]
- `cb2b930d` 2026-10-15 z3xox, +1 -1: fonts.tsv: the use column one number again (1 Menu, 2 Dialogue / UI, 4 Battle, added up; none = 3), so lines players added keep their meaning; Fira first again (the TBO screens on Original)
- `a103b16f` 2026-10-15 victorgonl, +10 -7: Add support for battle texts fonts
- `33ffd60c` 2026-10-13 victorgonl, +2 -2: Add font style support for menu headers
- `0325bca5` 2026-10-13 z3xox, +3 -1: Menu fonts: the game makes the list of labels itself, in a thread at start
- `637d1bd8` 2026-10-13 z3xox, +319 -0: Menu fonts: damaged font files against the font check, with the sanitizers

## `port/tests/menu_font/labels.c` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +3 -1: Dragon Rage 0.7.3
- `316b567c` 2026-10-16 victorgonl, +1 -1: Add support for dialogue and ui texts translations (no documentation yet)
- `5b3f71f3` 2026-10-16 victorgonl, +3 -1: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +159 -0: Dragon Rage 0.6.0 [skip ci]
- `01650240` 2026-10-13 z3xox, +159 -0: Menu fonts: a test of the maker of the list, with damaged archives

## `port/tests/menu_font/ui_text_extract.py` ([bt3-engine](bt3-engine.md))

- `979210ac` 2026-10-27 victorgonl, +135 -0: Expand translation extraction to story mode text

## `port/tests/menu_font/ui_translation.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +5 -2: Fix disc resources, character previews and HUD controls
- `2d8a5a72` 2026-10-18 dfdevx2, +62 -0: Dragon Rage 0.7.3
- `39d7a5f8` 2026-10-17 z3xox, +4 -13: Translations: the translated lines no longer in the saved game state (an online pause menu read "! !" and "dd")
- `316b567c` 2026-10-16 victorgonl, +71 -0: Add support for dialogue and ui texts translations (no documentation yet)

## `port/tests/native_music_runtime.py` ([bt3-engine](bt3-engine.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +61 -0: Add Linux setup and improve native menus and music

## `port/tests/native_play.py` ([bt3-engine](bt3-engine.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +52 -0: Add Linux setup and improve native menus and music

## `port/tests/native_regions_import.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +70 -0: Add native Tag Team modes

## `port/tests/native_regions_runtime.py` ([bt3-engine](bt3-engine.md))

- `02d1e354` 2026-10-26 Tag Team Mod contributors, +1 -1: Simplify the native online menu
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +10 -4: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +114 -0: Add native Tag Team modes

## `port/tests/native_setup.py` ([bt3-engine](bt3-engine.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +6 -2: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +45 -5: Add Linux setup and improve native menus and music
- `4fedaa1b` 2026-10-24 Tag Team Mod contributors, +13 -0: Keep the language selected during setup
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +53 -0: Add disc-based setup and simplify launchers

## `port/tests/tag_camera_options_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +31 -0: Add native Tag Team modes

## `port/tests/tag_challenge_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +37 -0: Add native Tag Team modes

## `port/tests/tag_cinema_matrix_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +21 -0: Add native Tag Team modes

## `port/tests/tag_cinematic_safety_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +73 -0: Add native Tag Team modes

## `port/tests/tag_clash_qte_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +21 -0: Add native Tag Team modes

## `port/tests/tag_clash_sequence.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +29 -0: Add native Tag Team modes

## `port/tests/tag_corpse_safety_runtime.py` ([bt3-engine](bt3-engine.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +39 -0: Fix giant grabs, ring-outs and fallen fighter recovery

## `port/tests/tag_feedback_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +21 -0: Add native Tag Team modes

## `port/tests/tag_ffa_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +63 -0: Add native Tag Team modes

## `port/tests/tag_fusion_camera_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +33 -0: Add native Tag Team modes

## `port/tests/tag_fusion_matrix_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +46 -0: Add native Tag Team modes

## `port/tests/tag_gauntlet_rematch_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +45 -0: Add native Tag Team modes

## `port/tests/tag_giant_grab_runtime.py` ([bt3-engine](bt3-engine.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +63 -0: Fix giant grabs, ring-outs and fallen fighter recovery

## `port/tests/tag_hud_render.cpp` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +96 -0: Fix disc resources, character previews and HUD controls

## `port/tests/tag_intro_lips_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +89 -0: Add native Tag Team modes

## `port/tests/tag_loading_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +69 -0: Add native Tag Team modes

## `port/tests/tag_lobby_rules.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +171 -0: Add native Tag Team modes

## `port/tests/tag_lobby_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +31 -0: Add native Tag Team modes

## `port/tests/tag_menu_runtime.py` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +6 -2: Fix disc resources, character previews and HUD controls
- `02d1e354` 2026-10-26 Tag Team Mod contributors, +1 -1: Simplify the native online menu
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +28 -22: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +314 -0: Add native Tag Team modes

## `port/tests/tag_mode_rush_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +23 -0: Add native Tag Team modes

## `port/tests/tag_online_menu_runtime.py` ([bt3-engine](bt3-engine.md))

- `02d1e354` 2026-10-26 Tag Team Mod contributors, +13 -7: Simplify the native online menu
- `101de958` 2026-10-24 Tag Team Mod contributors, +66 -0: Add native Tag Team modes

## `port/tests/tag_online_roles.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +144 -0: Add native Tag Team modes

## `port/tests/tag_online_room.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -0: Add native Tag Team modes

## `port/tests/tag_online_runtime.py` ([bt3-engine](bt3-engine.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +8 -3: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +188 -0: Add native Tag Team modes

## `port/tests/tag_post_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +90 -0: Add native Tag Team modes

## `port/tests/tag_ringout_runtime.py` ([bt3-engine](bt3-engine.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +13 -5: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +19 -0: Add native Tag Team modes

## `port/tests/tag_spawn_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +44 -0: Add native Tag Team modes

## `port/tests/tag_special_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +42 -0: Add native Tag Team modes

## `port/tests/tag_stage_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +45 -0: Add native Tag Team modes

## `port/tests/tag_status_hud_runtime.py` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +43 -0: Fix disc resources, character previews and HUD controls

## `port/tests/tag_team_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +324 -0: Add native Tag Team modes

## `port/tests/tag_tournament_runtime.py` ([bt3-engine](bt3-engine.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +10 -1: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +43 -0: Add native Tag Team modes

## `port/tests/tag_training_reentry_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +54 -0: Add native Tag Team modes

## `port/tests/tag_view_layout.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +43 -0: Add native Tag Team modes

## `port/tests/tag_weather_runtime.py` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +85 -0: Add native Tag Team modes

## `port/tests/update/run.sh` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +7 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/tests/update/test_update.c` ([bt3-engine](bt3-engine.md))

- `6b69eb72` 2026-10-28 z3xox, +4 -0: 0.1.39: lobbies replace rooms. Tenkaichi Budokai Online has no Create Room: the list is Quick Match, Ranked Match, Lobby, Join (Join Room renamed, the one place for a code), the SOON ones and Profile. Lobby opens New Lobby: Open Lobby (a code) or Host by Address (Port_LobbyCreate on the port net_port, L1 / R1 by a hundred); the lobby's page shows the port and this PC's LAN address where a code would be, square copies "address:port". A new lobby starts with the rules its owner last set on a lobby's page (Tb_LobbyKeepRules, in Create Room's settings: battle with DP 10 / 15 / 20, time, game speed, rollback, delay), so nothing that could be chosen is lost. No Watch on Join or Join by Address: a spectator joins the lobby; JOIN on Join by Address is Port_LobbyJoin and opens the lobby's page at once. Dragon Net Battle (F1 > Online > Advanced) opens and joins lobbies too (Host: a lobby with a code or on its port and the tab's rules; Join: a code or an address), no room of two and no Watch; F1 > Online loses "Let others watch your matches" (rooms only). PORT_RELEASE 139, NET_VERSION 6: a 0.1.38 game is never paired (the service's queues and codes are kept apart by it); by address a member joining a typed address that has not answered for two seconds also sends a spectator's greeting too short to be one, which a 0.1.38 host of two answers with SPEC_NO_VERSION ("The game at that address is a different version"), and a lobby's owner refusing another version notes it once a minute. FP_PEER_REQUIRED is 1: a peer, spectator or member without a fingerprint is refused. Update checks: v0.1.39 and v0.1.39.1 newer than 0.1.38.9 (44 checks).
- `0914c6d2` 2026-10-27 z3xox, +79 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/third_party/astcenc/LICENSE.txt` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +175 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/README.txt` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +896 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_averages_and_directions.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +948 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_block_sizes.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1218 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_color_quantize.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +2149 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_color_unquantize.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1023 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_compress_symbolic.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1458 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_compute_variance.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +558 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_decompress_symbolic.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +620 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_diagnostic_trace.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +254 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_diagnostic_trace.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +227 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_entry.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1558 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_find_best_partitioning.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +781 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_ideal_endpoints_and_weights.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1652 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_image.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +573 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_integer_sequence.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +739 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_internal.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +2230 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_internal_entry.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +346 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_mathlib.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +48 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_mathlib.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +615 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_mathlib_softfloat.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +358 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_partition_tables.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +497 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_percentile_tables.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1251 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_pick_best_endpoint_format.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1359 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_quantization.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +903 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_symbolic_physical.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +556 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +632 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib_avx2_8.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1228 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib_common_4.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +421 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib_neon_4.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1129 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib_none_4.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1228 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib_rvv_n.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1230 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib_sse_4.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1350 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_vecmathlib_sve_8.h` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +1105 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_weight_align.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +500 -0: Dragon Rage 0.7.0

## `port/third_party/astcenc/astcenc_weight_quant_xfer_tables.cpp` ([bt3-engine](bt3-engine.md))

- `1867006b` 2026-10-17 dfdevx2, +147 -0: Dragon Rage 0.7.0

## `port/third_party/fonts/anton/LICENSE` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +93 -0: Dragon Rage 0.6.0 [skip ci]
- `a103b16f` 2026-10-15 victorgonl, +93 -0: Add support for battle texts fonts

## `port/third_party/fonts/dejavu/LICENSE` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +44 -0: Dragon Rage 0.6.0 [skip ci]
- `2950c9d2` 2026-10-12 victorgonl, +44 -0: Add native and editable menu text with selectable fonts

## `port/third_party/fonts/fira/OFL.txt` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +92 -0: Dragon Rage 0.6.0 [skip ci]
- `2e97fe78` 2026-10-14 z3xox, +92 -0: TBO: Fira Sans Condensed Bold (OFL) first of the Menu fonts: the nearest of the open fonts to the game menu lettering

## `port/third_party/fonts/fonts.tsv` ([bt3-engine](bt3-engine.md))

- `9bed1539` 2026-10-27 Regulus CiberSec, +1 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `5d3eea11` 2026-10-27 Regulus CiberSec, +1 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `7b99f8ad` 2026-10-15 dfdevx2, +8 -0: Dragon Rage 0.6.0 [skip ci]
- `cb2b930d` 2026-10-15 z3xox, +4 -3: fonts.tsv: the use column one number again (1 Menu, 2 Dialogue / UI, 4 Battle, added up; none = 3), so lines players added keep their meaning; Fira first again (the TBO screens on Original)
- `a103b16f` 2026-10-15 victorgonl, +3 -2: Add support for battle texts fonts
- `2e97fe78` 2026-10-14 z3xox, +1 -0: TBO: Fira Sans Condensed Bold (OFL) first of the Menu fonts: the nearest of the open fonts to the game menu lettering
- `dd2cf63b` 2026-10-13 victorgonl, +1 -2: Keep DejaVu Sans Condensed as the only bundled menu font
- `33ffd60c` 2026-10-13 victorgonl, +1 -1: Add font style support for menu headers
- `f9c3f935` 2026-10-13 z3xox, +6 -0: Menu fonts: a font file is checked before use, drawing is several times faster, "Original" is as before

## `port/third_party/fonts/kanit/OFL.txt` ([bt3-engine](bt3-engine.md))

- `9bed1539` 2026-10-27 Regulus CiberSec, +93 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `5d3eea11` 2026-10-27 Regulus CiberSec, +93 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela

## `port/third_party/imgui/LICENSE.txt` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +21 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imconfig.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +150 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui.cpp` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +18628 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +4300 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_draw.cpp` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +6811 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_impl_opengl3.cpp` ([bt3-engine](bt3-engine.md))

- `9bfa7cfc` 2026-10-07 z3xox, +1173 -0: Overlay under OpenGL (ImGui's OpenGL3 backend, one build path for both renderers); Renderer choice in the video settings; the other renderer is tried when the chosen one cannot start

## `port/third_party/imgui/imgui_impl_opengl3.h` ([bt3-engine](bt3-engine.md))

- `9bfa7cfc` 2026-10-07 z3xox, +81 -0: Overlay under OpenGL (ImGui's OpenGL3 backend, one build path for both renderers); Renderer choice in the video settings; the other renderer is tried when the chosen one cannot start

## `port/third_party/imgui/imgui_impl_opengl3_loader.h` ([bt3-engine](bt3-engine.md))

- `9bfa7cfc` 2026-10-07 z3xox, +958 -0: Overlay under OpenGL (ImGui's OpenGL3 backend, one build path for both renderers); Renderer choice in the video settings; the other renderer is tried when the chosen one cannot start

## `port/third_party/imgui/imgui_impl_sdl3.cpp` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +890 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_impl_sdl3.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +54 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_impl_sdlgpu3.cpp` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +706 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_impl_sdlgpu3.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +62 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_impl_sdlgpu3_shaders.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +406 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_internal.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +4119 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_tables.cpp` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +4905 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imgui_widgets.cpp` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +11041 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imstb_rectpack.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +627 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imstb_textedit.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +1527 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/imgui/imstb_truetype.h` ([bt3-engine](bt3-engine.md))

- `fd1d6497` 2026-10-06 z3xox, +5085 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/third_party/libadrenotools/.gitmodules` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +4 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/CMakeLists.txt` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +37 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/LICENSE` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +24 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/README.md` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +30 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/adrenotools.pc.in` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +13 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/build_asm.sh` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +10 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/gen/bcenabler_patch.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +23 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/include/adrenotools/bcenabler.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +41 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/include/adrenotools/driver.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +71 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/include/adrenotools/priv.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +27 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/lib/linkernsbypass/CMakeLists.txt` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +19 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/lib/linkernsbypass/LICENSE` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +24 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/lib/linkernsbypass/README.md` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +9 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/lib/linkernsbypass/android_linker_ns.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +192 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/lib/linkernsbypass/android_linker_ns.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +81 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/lib/linkernsbypass/elf_soname_patcher.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +67 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/lib/linkernsbypass/elf_soname_patcher.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +21 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/bcenabler.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +135 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/bcenabler_patch.s` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +83 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/driver.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +210 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/CMakeLists.txt` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +27 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/file_redirect_hook.c` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +5 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/gsl_alloc_hook.c` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +13 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/hook_impl.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +229 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/hook_impl.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +30 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/hook_impl_params.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +31 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/kgsl.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +2124 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/src/hook/main_hook.c` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +9 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/ADPKG.md` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +96 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/README.md` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +7 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/acc-shim/README.md` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +12 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/acc-shim/vk_acc_shim.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +108 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/blob-patcher.py` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +67 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/.clang-format` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +9 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/Android.bp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +37 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/README.md` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +19 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/QtiGrallocDefs.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +173 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/common.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +86 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/gr_adreno_info.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +344 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/gr_adreno_info.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +234 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/gr_priv_handle.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +190 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/gr_utils.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +662 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/gr_utils.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +156 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/gralloc_priv.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +103 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/mapper.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +183 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/mapperextensions.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +224 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/types/common.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +6744 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/types/mapper.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +250 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/ext/types/mapperextensions.h` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +707 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/libadrenotools/tools/qtimapper-shim/shim.cpp` ([bt3-engine](bt3-engine.md))

- `87db39ee` 2026-10-08 Claude, +472 -0: Android: custom GPU drivers (adrenotools), bug report export, region message

## `port/third_party/miniaudio/README.txt` ([bt3-engine](bt3-engine.md))

- `109cfbe5` 2026-10-19 Renzo, +20 -0: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion

## `port/third_party/miniaudio/miniaudio.h` ([bt3-engine](bt3-engine.md))

- `109cfbe5` 2026-10-19 Renzo, +95864 -0: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion

## `port/third_party/miniz/LICENSE` ([bt3-engine](bt3-engine.md))

- `d01e8270` 2026-10-14 jllacchua, +22 -0: Auto-updater: in-game release download, progress bar, extraction and restart

## `port/third_party/miniz/miniz.c` ([bt3-engine](bt3-engine.md))

- `d01e8270` 2026-10-14 jllacchua, +7922 -0: Auto-updater: in-game release download, progress bar, extraction and restart

## `port/third_party/miniz/miniz.h` ([bt3-engine](bt3-engine.md))

- `d01e8270` 2026-10-14 jllacchua, +1510 -0: Auto-updater: in-game release download, progress bar, extraction and restart

## `port/third_party/monocypher/LICENCE.md` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +167 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/third_party/monocypher/README.txt` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +5 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/third_party/monocypher/monocypher-ed25519.c` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +500 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/third_party/monocypher/monocypher-ed25519.h` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +140 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/third_party/monocypher/monocypher.c` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +2956 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/third_party/monocypher/monocypher.h` ([bt3-engine](bt3-engine.md))

- `0914c6d2` 2026-10-27 z3xox, +321 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/third_party/pl_mpeg/LICENSE` ([bt3-engine](bt3-engine.md))

- `9bed1539` 2026-10-27 Regulus CiberSec, +23 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `5d3eea11` 2026-10-27 Regulus CiberSec, +23 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela

## `port/third_party/pl_mpeg/pl_mpeg.h` ([bt3-engine](bt3-engine.md))

- `9bed1539` 2026-10-27 Regulus CiberSec, +4439 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `5d3eea11` 2026-10-27 Regulus CiberSec, +4439 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela

## `port/third_party/sdl3-patches/vulkan-descriptor-sets.patch` ([renderer-gs](renderer-gs.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +155 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/third_party/xxhash/LICENSE` ([bt3-engine](bt3-engine.md))

- `04964a3d` 2026-10-06 z3xox, +34 -0: Texture packs: PCSX2-named DDS replacements from the textures folder, with a switch in the settings window

## `port/third_party/xxhash/xxhash.h` ([bt3-engine](bt3-engine.md))

- `04964a3d` 2026-10-06 z3xox, +7238 -0: Texture packs: PCSX2-named DDS replacements from the textures folder, with a switch in the settings window

## `port/tools/add_song.py` ([build-ci](build-ci.md))

- `39db4ebd` 2026-10-07 z3xox, +4 -2: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `0153b616` 2026-10-06 z3xox, +283 -0: Songs added to the music select: manifest, script, and the name overlay

## `port/tools/add_stage.py` ([build-ci](build-ci.md))

- `39db4ebd` 2026-10-07 z3xox, +4 -2: Stages and songs from folders next to the game: a file dropped in is used, the list and the name strip are optional
- `28e86145` 2026-10-06 z3xox, +283 -0: Stages from outside the disc: manifest, install script, styled menu names

## `port/tools/android.py` ([build-ci](build-ci.md))

- `344aa16c` 2026-10-31 dfdevx2, +12 -0: Tag Team as a setting of the app; the team and the licences in the app
- `2d8a5a72` 2026-10-18 dfdevx2, +3 -0: Dragon Rage 0.7.3
- `1867006b` 2026-10-17 dfdevx2, +10 -0: Dragon Rage 0.7.0
- `7b99f8ad` 2026-10-15 dfdevx2, +35 -12: Dragon Rage 0.6.0 [skip ci]
- `bbd4c2b5` 2026-10-11 dfdevx2, +1 -1: Dragon Rage 0.5.0 [skip ci]
- `928b2413` 2026-10-11 dfdevx2, +54 -0: WIP: patch 1 and touch artwork
- `6484b228` 2026-10-09 Claude, +5 -0: Screen filters (sharp bilinear, FXAA, FSR 1, Snapdragon GSR) and quarter-step resolutions
- `7427cc37` 2026-10-09 Claude, +7 -0: GPU: shader without discard for draws that never discard; split frame submission
- `0ab41362` 2026-10-09 Claude, +83 -3: Android: engine loadable anywhere below 4 GB; registers and stack movable
- `87db39ee` 2026-10-08 Claude, +46 -4: Android: custom GPU drivers (adrenotools), bug report export, region message
- `da357474` 2026-10-08 Claude, +1 -1: Android: work around LLVM 21 AArch64 dropping truncation on ptr32 stores
- `52232f3a` 2026-10-08 dfdx047, +12 -2: Android: the game's data list was shifted; a fixed key for test builds
- `69457fad` 2026-10-08 dfdx047, +2 -2: Android: the engine at 0x03000000 and the port's region below the Java heap
- `dce2c134` 2026-10-08 dfdx047, +375 -0: Engine on Android arm64: build, loader and game activity

## `port/tools/android_abicheck.py` ([build-ci](build-ci.md))

- `07280401` 2026-10-08 Claude, +87 -0: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown

## `port/tools/android_toolchain.sh` ([build-ci](build-ci.md))

- `c9fee92d` 2026-10-09 Claude, +1 -1: Release builds only, versioned tags, README update
- `6ca73cbb` 2026-10-08 dfdx047, +1 -2: Android: unlock-all from the app, back button opens the settings; toolchain fix
- `23d6efa5` 2026-10-08 dfdx047, +1 -1: Android app in English and Portuguese; CI shows build failures as annotations
- `dce2c134` 2026-10-08 dfdx047, +50 -0: Engine on Android arm64: build, loader and game activity

## `port/tools/build_native_launcher.py` ([build-ci](build-ci.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +21 -0: Add branded Play launcher, fullscreen controls and Mod Settings tab

## `port/tools/build_native_table_recipes.py` ([build-ci](build-ci.md))

- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +90 -0: Add disc-based setup and simplify launchers

## `port/tools/buildkey.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +29 -0: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +29 -0: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets

## `port/tools/change_map.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +86 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/compare_hash.py` ([build-ci](build-ci.md))

- `3d60111b` 2026-10-06 z3xox, +13 -1: Fight checksum in the BT3_HASH log (health, positions, clock, generator) and compare_hash.py --fight
- `4cae46f6` 2026-10-06 z3xox, +86 -0: State checksum (BT3_HASH) with tools to find what differs; the port's low memory at fixed addresses below the program: runs of one program are now identical

## `port/tools/db_assets.py` ([build-ci](build-ci.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +135 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas

## `port/tools/db_menu_prototype.py` ([build-ci](build-ci.md))

- `c6a37406` 2026-10-29 Regulus CiberSec, +164 -0: Tag Team: teste da conexão online no PC (Worker local + tag_link.c) e o protótipo aprovado dos menus

## `port/tools/eeconst.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +8 -2: Dragon Rage 0.6.0 [skip ci]
- `11ed03f6` 2026-10-13 z3xox, +5 -4: macOS: its changes to the shared build tools apply to the macOS build only (eeconst, ptr32, link, make_dat, the bsdtar reader)
- `660c1a38` 2026-10-12 Valentin Vivaldi, +6 -1: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal

## `port/tools/extract_bt4.py` ([build-ci](build-ci.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +133 -0: Add native Tag Team modes

## `port/tools/extract_disc.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +6 -3: Dragon Rage 0.6.0 [skip ci]
- `11ed03f6` 2026-10-13 z3xox, +1 -1: macOS: its changes to the shared build tools apply to the macOS build only (eeconst, ptr32, link, make_dat, the bsdtar reader)
- `2d312424` 2026-10-12 Valentin Vivaldi, +6 -3: macOS: install.py does the whole build from the disc image (Homebrew tools; the disc read with the system's bsdtar)

## `port/tools/extract_eu.py` ([build-ci](build-ci.md))

- `979210ac` 2026-10-27 victorgonl, +6 -4: Expand translation extraction to story mode text
- `3f922112` 2026-10-18 victorgonl, +41 -9: Original opening music extraction using European disc
- `de8da43b` 2026-10-18 victorgonl, +44 -7: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +123 -0: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +123 -0: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself

## `port/tools/fastvec_check.c` ([build-ci](build-ci.md))

- `bb212093` 2026-10-07 z3xox, +62 -0: Add and multiply by the processor where that is exact (plat_fastvec.c): single operations and the matrix-times-vector kernel, checked bit for bit against the soft-float code; a re-run frame 30% cheaper

## `port/tools/gen_data.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +5 -2: Dragon Rage 0.6.0 [skip ci]
- `2d312424` 2026-10-12 Valentin Vivaldi, +5 -2: macOS: install.py does the whole build from the disc image (Homebrew tools; the disc read with the system's bsdtar)

## `port/tools/glprobe.c` ([build-ci](build-ci.md))

- `75bf61a4` 2026-10-06 z3xox, +507 -0: GS: an OpenGL back end beside the Vulkan one

## `port/tools/import_native_disc.py` ([build-ci](build-ci.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +171 -0: Add native Tag Team modes

## `port/tools/import_tag_gauntlets.py` ([build-ci](build-ci.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +35 -0: Add native Tag Team modes

## `port/tools/irfix.py` ([build-ci](build-ci.md))

- `0ab41362` 2026-10-09 Claude, +23 -0: Android: engine loadable anywhere below 4 GB; registers and stack movable
- `da357474` 2026-10-08 Claude, +36 -1: Android: work around LLVM 21 AArch64 dropping truncation on ptr32 stores
- `f1619b6b` 2026-10-06 z3xox, +9 -0: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `66e2a7b1` 2026-10-06 z3xox, +107 -0: Port: 64-bit build (game pointers stay 4 bytes: ptr32.py, irfix.py, low memory); replay and heap match the 32-bit build

## `port/tools/lab/ab_bt4.sh` ([build-ci](build-ci.md))

- `fafbb446` 2026-10-31 dfdevx2, +16 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `port/tools/lab/ab_low.sh` ([build-ci](build-ci.md))

- `3ea03f80` 2026-10-31 dfdevx2, +15 -0: BT2 online: its screens in the three languages

## `port/tools/lab/auto_cube.sh` ([build-ci](build-ci.md))

- `4a72a23c` 2026-10-31 dfdevx2, +14 -0: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose

## `port/tools/lab/auto_rp5.sh` ([build-ci](build-ci.md))

- `4a72a23c` 2026-10-31 dfdevx2, +11 -0: BT4: the renderer by device: Dragon Rage where ARMSX2 is not held, unless the player chose

## `port/tools/lab/base_low.sh` ([build-ci](build-ci.md))

- `3ea03f80` 2026-10-31 dfdevx2, +20 -0: BT2 online: its screens in the three languages

## `port/tools/lab/bench.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +118 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/bt2boot.txt` ([build-ci](build-ci.md))

- `fea06c41` 2026-10-31 dfdevx2, +1 -0: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump

## `port/tools/lab/bt2key.py` ([build-ci](build-ci.md))

- `fea06c41` 2026-10-31 dfdevx2, +13 -0: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump

## `port/tools/lab/bt2room.sh` ([build-ci](build-ci.md))

- `2e034b59` 2026-10-31 dfdevx2, +10 -0: BT2 online: the match opens and walks itself to the character select; the card says where it is

## `port/tools/lab/bt2run.sh` ([build-ci](build-ci.md))

- `fea06c41` 2026-10-31 dfdevx2, +10 -0: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump

## `port/tools/lab/bt4cube.sh` ([build-ci](build-ci.md))

- `3441216c` 2026-10-31 dfdevx2, +7 -5: BT4: the ARMSX2 library again opens on every build; the build refuses a library the APK does not carry
- `8cbd9d5a` 2026-10-31 dfdevx2, +25 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/bt4fight.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +20 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/compare_env.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +9 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/dense.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +10 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/finals.sh` ([build-ci](build-ci.md))

- `b38202f4` 2026-10-31 dfdevx2, +20 -0: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts

## `port/tools/lab/finals2.sh` ([build-ci](build-ci.md))

- `b38202f4` 2026-10-31 dfdevx2, +12 -0: Launcher: the ISO notice on the BT3 online card; card text readable; final measuring scripts

## `port/tools/lab/kpad.py` ([build-ci](build-ci.md))

- `fafbb446` 2026-10-31 dfdevx2, +1 -1: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report
- `8cbd9d5a` 2026-10-31 dfdevx2, +13 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/lab.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +88 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/lowend.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +7 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/nettable.py` ([build-ci](build-ci.md))

- `3441216c` 2026-10-31 dfdevx2, +2 -1: BT4: the ARMSX2 library again opens on every build; the build refuses a library the APK does not carry
- `8cbd9d5a` 2026-10-31 dfdevx2, +17 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/numbers_armsx2.sh` ([build-ci](build-ci.md))

- `3441216c` 2026-10-31 dfdevx2, +16 -0: BT4: the ARMSX2 library again opens on every build; the build refuses a library the APK does not carry

## `port/tools/lab/numbers_cube.sh` ([build-ci](build-ci.md))

- `3441216c` 2026-10-31 dfdevx2, +20 -0: BT4: the ARMSX2 library again opens on every build; the build refuses a library the APK does not carry

## `port/tools/lab/numbers_rp5.sh` ([build-ci](build-ci.md))

- `3441216c` 2026-10-31 dfdevx2, +15 -0: BT4: the ARMSX2 library again opens on every build; the build refuses a library the APK does not carry

## `port/tools/lab/online10.sh` ([build-ci](build-ci.md))

- `3ea03f80` 2026-10-31 dfdevx2, +35 -0: BT2 online: its screens in the three languages

## `port/tools/lab/pad.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +51 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/ppmcmp.py` ([build-ci](build-ci.md))

- `3ce20c60` 2026-10-31 dfdevx2, +59 -0: Lab tools: the pixel compare; logscan counts a crash once

## `port/tools/lab/rec4.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +8 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/res.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +30 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/series_base.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +26 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/series_bt4_cube.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +13 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/series_bt4_rp5.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +8 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/series_low.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +12 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/series_lowend.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +20 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/tagoff.sh` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +9 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/tagroom.sh` ([build-ci](build-ci.md))

- `00244a0d` 2026-10-31 dfdevx2, +13 -0: Tag Team online: the costume table is read through the file cache, as the Android install keeps it in the archive

## `port/tools/lab/threads.sh` ([build-ci](build-ci.md))

- `fafbb446` 2026-10-31 dfdevx2, +9 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `port/tools/lab/ui.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +46 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/lab/waitfor.py` ([build-ci](build-ci.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +14 -0: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs

## `port/tools/link.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +43 -3: Dragon Rage 0.6.0 [skip ci]
- `11ed03f6` 2026-10-13 z3xox, +3 -3: macOS: its changes to the shared build tools apply to the macOS build only (eeconst, ptr32, link, make_dat, the bsdtar reader)
- `1296198e` 2026-10-12 Valentin Vivaldi, +4 -4: link.py: the macOS comment wrapped like the rest of the file
- `81bc5508` 2026-10-12 Valentin Vivaldi, +1 -3: macOS: rollback and the state list (rewind test: the whole demo fight, 0 differences at depth 1 and 8)
- `660c1a38` 2026-10-12 Valentin Vivaldi, +46 -4: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `928b2413` 2026-10-11 dfdevx2, +4 -2: WIP: patch 1 and touch artwork
- `c72ed98d` 2026-10-08 JulioHeroZ, +3 -1: Fix native Windows build argument limits and SDL runtime packaging
- `db3730c8` 2026-10-09 z3xox, +1 -1: Matchmaking client: the system's own HTTPS (WinHTTP on Windows, the curl library on Linux, the curl program only as a last resort); the host gives its room up as soon as the other player is known
- `6a65ae7a` 2026-10-07 z3xox, +1 -1: Netcode, first step: two copies in lockstep over UDP (gs/net.c), tested on one machine with delay, loss and lag; timing of re-run blanks
- `4cae46f6` 2026-10-06 z3xox, +6 -0: State checksum (BT3_HASH) with tools to find what differs; the port's low memory at fixed addresses below the program: runs of one program are now identical
- `66b57ad0` 2026-10-06 z3xox, +1 -1: Windows: no relocation table (a system set to move every program moved it off its fixed address: crash before any output); a plain message if loaded elsewhere; crash report installed first
- `f7811048` 2026-10-06 z3xox, +1 -1: Windows speed: settings lookups remembered (getenv cost about half of a frame there); frame pacing and timing statistic fixed
- `8d8ba102` 2026-10-06 z3xox, +4 -0: Release build: Windows section padding in make_dat; failures stop the build
- `bc80dfe4` 2026-10-06 z3xox, +4 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone
- `35eb9563` 2026-10-06 z3xox, +2 -1: Linux release built in an Ubuntu 22.04 container (baseline x86-64, glibc 2.35, SDL3 from source); portability check in package.py
- `f1619b6b` 2026-10-06 z3xox, +22 -12: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `97a17db3` 2026-10-06 z3xox, +2 -1: Release program without the game's data tables: strip_data.py and the loader that fetches them from the user's disc
- `66e2a7b1` 2026-10-06 z3xox, +8 -4: Port: 64-bit build (game pointers stay 4 bytes: ptr32.py, irfix.py, low memory); replay and heap match the 32-bit build
- `40a2fa36` 2026-10-06 z3xox, +7 -4: Port: game code builds with clang (BT3_CC=clang), replay and heap match the gcc build
- `fd1d6497` 2026-10-06 z3xox, +1 -1: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/tools/logscan.py` ([build-ci](build-ci.md))

- `3ce20c60` 2026-10-31 dfdevx2, +20 -9: Lab tools: the pixel compare; logscan counts a crash once
- `96895d21` 2026-10-31 dfdevx2, +348 -0: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan

## `port/tools/macho.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +24 -0: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +24 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal

## `port/tools/macwin.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +51 -0: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +51 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal

## `port/tools/macwin/macwin.cpp` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +735 -0: Dragon Rage 0.6.0 [skip ci]
- `8c3bdb43` 2026-10-13 Valentin Vivaldi, +47 -0: macOS: a game address passed as an integer to a callback that takes a pointer reaches it (the memory card's new save crashed)
- `660c1a38` 2026-10-12 Valentin Vivaldi, +688 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal

## `port/tools/make_adxlen.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +55 -0: Dragon Rage 0.6.0 [skip ci]
- `d48b19ba` 2026-10-13 z3xox, +55 -0: Online: how long a voice line lasts is counted from a table of the original disc's streams, not from the player's own file (a voice mod of another length started the fight on another blank)

## `port/tools/make_dat.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +44 -6: Dragon Rage 0.6.0 [skip ci]
- `11ed03f6` 2026-10-13 z3xox, +1 -1: macOS: its changes to the shared build tools apply to the macOS build only (eeconst, ptr32, link, make_dat, the bsdtar reader)
- `cfeee61c` 2026-10-12 Valentin Vivaldi, +44 -6: macOS: a build without game data fetches it from the disc like the Linux release (same demo-fight state on all 4428 frames)
- `8d8ba102` 2026-10-06 z3xox, +1 -1: Release build: Windows section padding in make_dat; failures stop the build
- `bc80dfe4` 2026-10-06 z3xox, +81 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/tools/make_fingerprint.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +618 -0: Dragon Rage 0.6.0 [skip ci]
- `17dc72dd` 2026-10-13 z3xox, +618 -0: A gameplay fingerprint of the data, printed in the log: what a fight is computed from, never pictures or sound (step 1 of matching online players by their data)

## `port/tools/make_skeleton.py` ([build-ci](build-ci.md))

- `bc80dfe4` 2026-10-06 z3xox, +157 -0: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone

## `port/tools/make_state.py` ([build-ci](build-ci.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -1: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +51 -6: Dragon Rage 0.6.0 [skip ci]
- `81bc5508` 2026-10-12 Valentin Vivaldi, +40 -5: macOS: rollback and the state list (rewind test: the whole demo fight, 0 differences at depth 1 and 8)
- `52f2861a` 2026-10-11 z3xox, +11 -1: The list of the game's variables says which program it is for: a list left over from another release (a Windows copy with only the .exe replaced) is refused with a message instead of crashing the first online match
- `dfd64826` 2026-10-07 z3xox, +8 -3: State save / restore on Windows (builtin setjmp, the port's stack switch); one region allocator on both systems; the rewind test passes on the Windows program under Wine
- `8a0dc162` 2026-10-07 z3xox, +1 -1: Rewindable file, memory card and stream-player layers; PORT_HOST; the state test passes the replay fight and a whole recorded session with zero differences
- `54548c55` 2026-10-06 z3xox, +12 -2: State save / restore (64-bit Linux) and BT3_SYNCTEST, which re-runs every frame from its saved state: it works and shows the file and sound layers are not rewindable yet
- `4cae46f6` 2026-10-06 z3xox, +65 -0: State checksum (BT3_HASH) with tools to find what differs; the port's low memory at fixed addresses below the program: runs of one program are now identical

## `port/tools/make_tag_text.py` ([build-ci](build-ci.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +44 -0: Add native Tag Team modes

## `port/tools/make_verify.py` ([build-ci](build-ci.md))

- `928b2413` 2026-10-11 dfdevx2, +91 -0: WIP: patch 1 and touch artwork
- `195402b4` 2026-10-10 z3xox, +91 -0: The game says when its data is not the original disc's: a background check of every data file against a table of checksums, then the window title, the log, a notice and the crash report carry it

## `port/tools/mc_extract.py` ([build-ci](build-ci.md))

- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +32 -6: Add disc-based setup and simplify launchers

## `port/tools/menu_font.py` ([build-ci](build-ci.md))

- `65a836e1` 2026-10-24 z3xox, +13 -2: labels.tsv v13: each label's fingerprint (64 of its pixels, MenuLabels_Quick) as its last column, and a new texture of a label's size is hashed in full only when a label of that size has its fingerprint. The hash over all of a texture's bytes, one at a time, was most of a fight's big new textures' work. New-texture work in the recorded two fights: 145 ms in all instead of 254, 8.3 ms in its worst frame instead of 13.1, none over 10 (629 and 30.7 before this branch). Every texture the same byte for byte, the menu font's labels included. An older list is made again from the disc at the next start (0.3 s), as for any new layout; a row without a fingerprint is always hashed. port/tools/menu_font.py writes v13 and keeps the fingerprints it finds in labels.tsv, by texture hash
- `2d8a5a72` 2026-10-18 dfdevx2, +31 -42: Dragon Rage 0.7.3
- `316b567c` 2026-10-16 victorgonl, +4 -2: Add support for dialogue and ui texts translations (no documentation yet)
- `5b3f71f3` 2026-10-16 victorgonl, +28 -41: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +532 -0: Dragon Rage 0.6.0 [skip ci]
- `cb2b930d` 2026-10-15 z3xox, +6 -5: fonts.tsv: the use column one number again (1 Menu, 2 Dialogue / UI, 4 Battle, added up; none = 3), so lines players added keep their meaning; Fira first again (the TBO screens on Original)
- `440aa24f` 2026-10-15 victorgonl, +3 -3: Refine battle font outlines, shadows, and subtle digit relief
- `a103b16f` 2026-10-15 victorgonl, +102 -31: Add support for battle texts fonts
- `33ffd60c` 2026-10-13 victorgonl, +269 -11: Add font style support for menu headers
- `32fc2370` 2026-10-13 z3xox, +108 -600: Menu fonts: the Python tool only applies an edited labels.json; the install step is gone
- `95d0e94e` 2026-10-13 z3xox, +160 -251: Menu fonts: the label step needs no packages, cannot fail the installation, and checks the disc
- `e1e19d47` 2026-10-12 victorgonl, +77 -9: Add text styles
- `b26bdd08` 2026-10-12 victorgonl, +9 -2: Add missing Character Customize menu text
- `58b7887c` 2026-10-12 victorgonl, +10 -73: Remove generation of PNG previews
- `2e93dc19` 2026-10-12 victorgonl, +10 -7: Add support for shared fonts in dialogue and UI (text not editable)
- `667ec7a8` 2026-10-12 victorgonl, +7 -1: Add missing Character Reference menu text
- `cac847fb` 2026-10-12 victorgonl, +2 -0: Add missing "END" label to font script
- `2950c9d2` 2026-10-12 victorgonl, +762 -0: Add native and editable menu text with selectable fonts

## `port/tools/native_launcher.c` ([build-ci](build-ci.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +81 -0: Add branded Play launcher, fullscreen controls and Mod Settings tab

## `port/tools/native_launcher.rc` ([build-ci](build-ci.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +22 -0: Add branded Play launcher, fullscreen controls and Mod Settings tab

## `port/tools/native_play.py` ([build-ci](build-ci.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +25 -11: Add Linux setup and improve native menus and music
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +54 -0: Add disc-based setup and simplify launchers

## `port/tools/native_save.py` ([build-ci](build-ci.md))

- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +70 -0: Add disc-based setup and simplify launchers

## `port/tools/native_setup.py` ([build-ci](build-ci.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +7 -28: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +35 -7: Add Linux setup and improve native menus and music
- `4fedaa1b` 2026-10-24 Tag Team Mod contributors, +1 -1: Keep the language selected during setup
- `a2e12c1f` 2026-10-24 Tag Team Mod contributors, +5 -3: Separate save instructions and clear idle progress
- `1a723f81` 2026-10-24 Tag Team Mod contributors, +10 -4: Clarify the choice between save sources
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +230 -0: Add disc-based setup and simplify launchers

## `port/tools/native_table_recipes.json` ([build-ci](build-ci.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +1 -1: Fix giant grabs, ring-outs and fallen fighter recovery
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls
- `02d1e354` 2026-10-26 Tag Team Mod contributors, +1 -1: Simplify the native online menu
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +1 -1: Add Linux setup and improve native menus and music
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +1 -0: Add disc-based setup and simplify launchers

## `port/tools/native_tables.py` ([build-ci](build-ci.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +19 -0: Add Linux setup and improve native menus and music
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +75 -0: Add disc-based setup and simplify launchers

## `port/tools/package.py` ([build-ci](build-ci.md))

- `9bed1539` 2026-10-27 Regulus CiberSec, +2 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `5d3eea11` 2026-10-27 Regulus CiberSec, +2 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `a3cf1192` 2026-10-28 z3xox, +2 -2: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `0914c6d2` 2026-10-27 z3xox, +3 -1: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.
- `0ca19719` 2026-10-27 z3xox, +6 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `d01e8270` 2026-10-14 jllacchua, +2 -0: Auto-updater: in-game release download, progress bar, extraction and restart
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +5 -4: Add disc-based setup and simplify launchers
- `088ec8c7` 2026-10-21 z3xox, +3 -2: Songs: the .adx an earlier release converted from an MP3 (the converter's mark) is not listed while that MP3 is there, which is now played itself (a player who had added MP3s saw each one twice); a songs.txt line naming such a copy plays the MP3; the folder's note says so, and that a file that cannot be read is silent (it is not left out)
- `109cfbe5` 2026-10-19 Renzo, +10 -13: Implement miniaudio library for native high quality audio, remove MP3 to ADX conversion
- `de8da43b` 2026-10-18 victorgonl, +1 -8: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +25 -0: Dragon Rage 0.7.3
- `add09861` 2026-10-18 z3xox, +4 -1: Setup: an optional European disc image for Spanish, German, French and Italian; the game makes the languages itself
- `4822563f` 2026-10-16 victorgonl, +3 -2: Support for ui bundled translations
- `71b1f103` 2026-10-16 victorgonl, +7 -5: Add support for port content text translations (need documentation)
- `5b3f71f3` 2026-10-16 victorgonl, +19 -0: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +69 -4: Dragon Rage 0.6.0 [skip ci]
- `cb2b930d` 2026-10-15 z3xox, +2 -2: fonts.tsv: the use column one number again (1 Menu, 2 Dialogue / UI, 4 Battle, added up; none = 3), so lines players added keep their meaning; Fira first again (the TBO screens on Original)
- `a103b16f` 2026-10-15 victorgonl, +6 -5: Add support for battle texts fonts
- `6070f0bf` 2026-10-15 z3xox, +11 -0: TBO: its pictures in the repository (port/tbo/ui, with their credits: the announcer by MarshallPunk, the buttons by pluantoine under CC BY 4.0), read there in a source checkout and packaged as tbo/ui; tbo_sizes.py makes tbo_sizes.h (the new background size)
- `9cdcdcbd` 2026-10-14 z3xox, +8 -0: Licence: GNU AGPL v3 from here on
- `bbe13fcb` 2026-10-13 z3xox, +7 -5: Package: the README says the Menu font setting is there
- `13154262` 2026-10-13 z3xox, +14 -0: Screen reader: every language through a11y_<code>.txt, Spanish too
- `2db5e22b` 2026-10-13 z3xox, +18 -0: Release: the fonts and their licences are in the package
- `19011dcb` 2026-10-13 z3xox, +6 -3: Added songs: the texts and the notes for the MP3 conversion as it is now
- `d67cd22c` 2026-10-12 Valentin Vivaldi, +9 -4: Added songs: an .mp3 in the songs folder is converted to ADX at start, once (one that cannot be read is left out, with a notice; the game goes on)
- ... e mais 14 (`git log -- port/tools/package.py`)

## `port/tools/package_native_setup.py` ([build-ci](build-ci.md))

- `47b4d577` 2026-10-26 Tag Team Mod contributors, +9 -2: Add branded Play launcher, fullscreen controls and Mod Settings tab
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +52 -20: Add Linux setup and improve native menus and music
- `6f5f62c8` 2026-10-24 Tag Team Mod contributors, +80 -0: Add disc-based setup and simplify launchers

## `port/tools/perf_chart.py` ([build-ci](build-ci.md))

- `fafbb446` 2026-10-31 dfdevx2, +102 -0: README: everything new in 1.0, in three languages, with pictures and before/after numbers; the 1.0 report

## `port/tools/portsrc.py` ([build-ci](build-ci.md))

- `1ffcdf72` 2026-10-13 z3xox, +1 -17: Menu fonts: the glyph table hook is an #ifdef PORT in col_c.c, not a patch of the source at build time
- `2e93dc19` 2026-10-12 victorgonl, +17 -1: Add support for shared fonts in dialogue and UI (text not editable)

## `port/tools/proto_check.py` ([build-ci](build-ci.md))

- `928b2413` 2026-10-11 dfdevx2, +48 -0: WIP: patch 1 and touch artwork
- `bf793603` 2026-10-09 z3xox, +48 -0: A tournament (and training, Sim Dragon) match could crash in a 64-bit build: BattleSetup_SetSide's eighth argument, a pointer, was declared s32 in three menu headers, so the caller wrote 32 bits of it on the stack

## `port/tools/ptr32.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +11 -0: Dragon Rage 0.6.0 [skip ci]
- `11ed03f6` 2026-10-13 z3xox, +2 -1: macOS: its changes to the shared build tools apply to the macOS build only (eeconst, ptr32, link, make_dat, the bsdtar reader)
- `660c1a38` 2026-10-12 Valentin Vivaldi, +10 -0: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `f1619b6b` 2026-10-06 z3xox, +50 -4: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `66e2a7b1` 2026-10-06 z3xox, +188 -0: Port: 64-bit build (game pointers stay 4 bytes: ptr32.py, irfix.py, low memory); replay and heap match the 32-bit build

## `port/tools/strip_data.py` ([build-ci](build-ci.md))

- `f1619b6b` 2026-10-06 z3xox, +19 -14: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `97a17db3` 2026-10-06 z3xox, +138 -0: Release program without the game's data tables: strip_data.py and the loader that fetches them from the user's disc

## `port/tools/taglink_test/README.md` ([build-ci](build-ci.md))

- `c6a37406` 2026-10-29 Regulus CiberSec, +14 -0: Tag Team: teste da conexão online no PC (Worker local + tag_link.c) e o protótipo aprovado dos menus

## `port/tools/taglink_test/harness.c` ([build-ci](build-ci.md))

- `c6a37406` 2026-10-29 Regulus CiberSec, +85 -0: Tag Team: teste da conexão online no PC (Worker local + tag_link.c) e o protótipo aprovado dos menus

## `port/tools/taglink_test/server.mjs` ([build-ci](build-ci.md))

- `c6a37406` 2026-10-29 Regulus CiberSec, +16 -0: Tag Team: teste da conexão online no PC (Worker local + tag_link.c) e o protótipo aprovado dos menus

## `port/tools/tbo_sizes.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +55 -0: Dragon Rage 0.6.0 [skip ci]
- `6070f0bf` 2026-10-15 z3xox, +55 -0: TBO: its pictures in the repository (port/tbo/ui, with their credits: the announcer by MarshallPunk, the buttons by pluantoine under CC BY 4.0), read there in a source checkout and packaged as tbo/ui; tbo_sizes.py makes tbo_sizes.h (the new background size)

## `port/tools/tbo_strings.py` ([build-ci](build-ci.md))

- `a3cf1192` 2026-10-28 z3xox, +1 -1: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `3ea60f57` 2026-10-27 z3xox, +7 -1: Tournaments in lobbies. The lobby's owner switches the lobby between Versus and Tournament (a new first rule) and draws a knockout bracket of the places marked ready (2 to 8 humans, the owner too if it is ready): the order is shuffled from a number the owner draws from the system's random source, with byes shared out between the halves when the entrants are not 2, 4 or 8. The owner's game holds the bracket and sends it with the lobby (STATE, now 608 bytes; LOBBY_VERSION 3, so a 0.1.38.9 game is told the versions differ, both ways). Each match is an ordinary lobby match of single battles: the owner's game puts the two in 1P and 2P and starts it, everyone else watches. Every copy in the match notes how its first battle ended (the battle sequence past the fight, held 0.3 s so that a rolled-back K.O. is not taken; Port_BattleOutcome in headless.c), a player says it with each ALIVE, and the owner, when the two agree on a winner, marks it and ends the match; a draw, two different results, one result alone for 20 s or a match ended any other way is played again. An entrant who leaves loses by walkover. A new page, the bracket (TB_BRACKET in port_tbo.c), shows the two halves meeting at the final with gold lines for winners, the next match glowing, byes greyed and the champion's box, and the owner's Start Next Match / New Tournament; it comes up when a tournament is drawn and after each of its matches, and the lobby's page has a Bracket button. All text is in port_tbo.c for the translators. Tested offscreen through a local Worker: 4 and 5 entrants (byes) to a champion with every spectator identical to its host in every match, a walkover, an old release joining either way; Versus lobbies, quick match and rooms of two unchanged (docs/port/README.md, "Tournaments").
- `cbde53ab` 2026-10-26 z3xox, +1 -1: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `a581c3f7` 2026-10-26 z3xox, +4 -4: A spectator's own pause menu is the battle's own pause menu: its box from the battle's sprite sheet, drawn by its engine (BtlMenu_Draw, src/battle/hud_0_b.c, through BtlMenu_PortDrawMenu), its colours, sounds and six-frame growing box, and the versus pause menu's own "EXIT ?" Yes / No box before leaving (PauseMenu_PortConfirmBox, src/battle/hud_0.c), instead of a Tenkaichi Budokai Online panel. Its items (port/src/spec_menu.c): the note "Your screen only: the fight goes on" in the title's row, Continue, View: Player 1 / Player 2 / Split Screen with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching; their texts reach the engine through BtlMenu_GetItemText (numbers 0x6FFF and on) and are translated by the online menus' file (tbo_strings.py lists the file). The engine's update is not run and the battle's menus are not touched: the file moves the cursor and the box itself and reads the PC's pad (Port_PadRead); in a fight only (gPortBtlMenuLive). Tested by code with an owner and a member watching and every item used with the pad: the fight values of both spectators and both players identical, no check value differed. All game-side changes under PORT: the PS2 build is unchanged.
- `18d6fd06` 2026-10-26 z3xox, +13 -0: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `c49f5e0f` 2026-10-23 z3xox, +191 -0: The online menus can be translated: translations/port/<language>.json takes their lines (ids tbo/..., looked up by their English), where the menu draws and measures a line (tbo_text.c), joins pieces (Ps_Copy, which also feeds the screen reader) and formats a number (Tb_Fmt). A translation that loses a line's %d parts or {name} is not used, and a lone % in a format line is refused, so a file cannot make the game misread. The talk box decodes UTF-8 (accents were two wrong letters), and the frame count has its own plural line. port/tools/tbo_strings.py makes a file for translators with the English as placeholder (303 lines; --bt4 without BT3's fighter names) and keeps what is translated when run again. Accents checked on screen in Spanish

## `port/tools/tbo_translations.py` ([build-ci](build-ci.md))

- `4dc3964c` 2026-10-27 Regulus CiberSec, +71 -7: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +396 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +71 -7: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `5d3eea11` 2026-10-27 Regulus CiberSec, +396 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela

## `port/tools/toolchain.py` ([build-ci](build-ci.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +22 -6: Dragon Rage 0.6.0 [skip ci]
- `660c1a38` 2026-10-12 Valentin Vivaldi, +22 -6: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `f1619b6b` 2026-10-06 z3xox, +30 -0: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed

## `port/tools/undefined.py` ([build-ci](build-ci.md))

- `0914c6d2` 2026-10-27 z3xox, +6 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.
- `0ca19719` 2026-10-27 z3xox, +7 -0: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +5 -1: Fix giant grabs, ring-outs and fallen fighter recovery
- `6dbb460b` 2026-10-21 z3xox, +6 -1: Glow: the GS's 1x sampling of a work buffer in a shader of its own (gswb.frag, pipeline key bit 25), only for draws from one work buffer into another, one read per GS texel at 2x (in the shader of every draw and on the passes over the whole picture it took a fight at 2x on an integrated chip from 124 pictures a second to 90; now 118, 122 without it: BT3_GLOW_BLOCK=0)
- `7b99f8ad` 2026-10-15 dfdevx2, +118 -25: Dragon Rage 0.6.0 [skip ci]
- `5b0ad7ed` 2026-10-13 z3xox, +3 -1: Online: a build key (official releases sign their matchmaking requests, and the Worker can be told to let only those into quick match, or into everything); off until the keeper sets the secrets
- `660c1a38` 2026-10-12 Valentin Vivaldi, +90 -9: macOS: native Apple Silicon build (BT3_CC=mac64): the demo fight gives the same state as Linux on all 4428 frames, drawn with Metal
- `4c566107` 2026-10-12 z3xox, +5 -1: Side bars: Black or Orange (the owner's art, two pieces that fit a bar of any width)
- `e16805d3` 2026-10-12 z3xox, +8 -2: Performance: the per-pixel depth of 0.1.25 only where it changes something (it cost a quarter of the frame rate on integrated graphics)
- `928b2413` 2026-10-11 dfdevx2, +4 -1: WIP: patch 1 and touch artwork
- `c72ed98d` 2026-10-08 JulioHeroZ, +4 -1: Fix native Windows build argument limits and SDL runtime packaging
- `7427cc37` 2026-10-09 Claude, +12 -0: GPU: shader without discard for draws that never discard; split frame submission
- `75bf61a4` 2026-10-06 z3xox, +9 -0: GS: an OpenGL back end beside the Vulkan one
- `99dabe03` 2026-10-07 z3xox, +2 -0: undefined.py: BT3_PORT_CFLAGS for profiling builds; docs: where the arithmetic comes from now
- `bb212093` 2026-10-07 z3xox, +1 -1: Add and multiply by the processor where that is exact (plat_fastvec.c): single operations and the matrix-times-vector kernel, checked bit for bit against the soft-float code; a re-run frame 30% cheaper
- `bc80dfe4` 2026-10-06 z3xox, +17 -3: Build without a disc: blank data tables (port/data), make_skeleton.py, make_dat.py; release container builds Linux and Windows archives from the repository alone
- `35eb9563` 2026-10-06 z3xox, +8 -5: Linux release built in an Ubuntu 22.04 container (baseline x86-64, glibc 2.35, SDL3 from source); portability check in package.py
- `f1619b6b` 2026-10-06 z3xox, +19 -18: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `66e2a7b1` 2026-10-06 z3xox, +43 -6: Port: 64-bit build (game pointers stay 4 bytes: ptr32.py, irfix.py, low memory); replay and heap match the 32-bit build
- `40a2fa36` 2026-10-06 z3xox, +23 -3: Port: game code builds with clang (BT3_CC=clang), replay and heap match the gcc build
- `fd1d6497` 2026-10-06 z3xox, +14 -0: Port: settings window with Dear ImGui (mouse), rebindable keys and controller buttons, controller assignment

## `port/tools/update_key.py` ([build-ci](build-ci.md))

- `0914c6d2` 2026-10-27 z3xox, +225 -0: Automatic update, on top of PR #107 by JosephLlacchua: releases are signed, and the game installs only what the release key signed. Monocypher 4.0.2 (port/third_party/monocypher, BSD-2-Clause or CC0, its licence into the release's licenses/monocypher.txt) checks an Ed25519 signature (RFC 8032) of the whole zip, the .sig beside it in the release (128 hex digits), against the public key compiled in from port/src/update_key.h (gs/update_crypto.c, update_monocypher.c: the two parts of the library in two files, they share static names). The key in update_key.h is a PLACEHOLDER of zeros: a build with it says an update exists and shows its page, but installs nothing, as does any build for a release without a .sig ("The release is not signed..."); a signature that is not right: nothing is installed, the reason and the release page are shown. The size and GitHub's SHA-256 are still checked first, for a clear message. port/tools/update_key.py (plain Python, nothing to install; checked against the RFC's vectors and the cryptography package): `generate` makes the key pair once, the private half into a file (mode 600, never on the screen) for the repository secret UPDATE_SIGNING_KEY, the public half printed for update_key.h; `sign`, `verify`, `public`. The release workflow, for a tag, signs both zips with that secret and attaches the .sig files to the draft release; without the secret it warns in the log and the release is made unsigned; a secret whose public half is not the one in update_key.h stops it (no game could check those signatures). BT3_UPDATE_TEST_KEY=<hex> at build time (undefined.py, testing only; the release container is never given it) builds with a throw-away key instead. port/tests/update: 34 checks (release numbers: 0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39, "dev" and "0.1.39-rc1" never compared; SHA-256 against FIPS 180-4; the RFC 8032 signature, and a changed message, signature or key refused). F1's update rows: the state on one line, its buttons under it (they ran out of the window). Tested on Linux with a local stand-in of GitHub (BT3_UPDATE_API) and a scratch install of a 0.1.38.9 built so: Update now, download, verify, Restart now: the new 0.1.38.10 runs from the folder, says it, removes the .old files; the player's files unchanged byte for byte (fonts.tsv: the new list and the player's line), the six entries of theirs put into the zip on purpose not written, the programs executable; a zip signed with another key, a release without .sig, a read-only folder (step a), a swap failing in step b (BT3_UPDATE_FAIL_AT=6, after the program itself was swapped) and a zip with a ../ path: each refused or put back, every file as before, the old version going on.

## `port/translations/labels/en.json` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +2031 -0: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +2031 -0: Add support for menu translations (documentation not updated)

## `port/translations/labels/es.json` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2031 -0: Add native Tag Team modes

## `port/translations/port/en.json` ([bt3-engine](bt3-engine.md))

- `4dc3964c` 2026-10-27 Regulus CiberSec, +2 -2: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +2 -2: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `0ca19719` 2026-10-27 z3xox, +22 -2: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `cbde53ab` 2026-10-26 z3xox, +37 -1: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `18d6fd06` 2026-10-26 z3xox, +6 -2: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `2d8a5a72` 2026-10-18 dfdevx2, +23 -0: Dragon Rage 0.7.3
- `35db3fd4` 2026-10-18 z3xox, +9 -1: Online: a line on the screen while a match starts (Connecting to your rival..., then Your rival "NAME" is here! The match begins.)
- `71b1f103` 2026-10-16 victorgonl, +15 -0: Add support for port content text translations (need documentation)

## `port/translations/port/es.json` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +23 -0: Add native Tag Team modes

## `port/translations/tag/es.tsv` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +26 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `3f9ad0fc` 2026-10-28 Regulus CiberSec, +5 -0: Tag Team: polimento do menu do SELECT com as peças do Dragon Net Online
- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +2 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +37 -0: Fix disc resources, character previews and HUD controls
- `02d1e354` 2026-10-26 Tag Team Mod contributors, +2 -2: Simplify the native online menu
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +1 -0: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +731 -0: Add native Tag Team modes

## `port/translations/tag/pt.tsv` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +26 -0: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `3f9ad0fc` 2026-10-28 Regulus CiberSec, +5 -0: Tag Team: polimento do menu do SELECT com as peças do Dragon Net Online

## `port/translations/tbo/es.json` ([bt3-engine](bt3-engine.md))

- `4dc3964c` 2026-10-27 Regulus CiberSec, +482 -278: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +1087 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +482 -278: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `5d3eea11` 2026-10-27 Regulus CiberSec, +1087 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela

## `port/translations/tbo/pt.json` ([bt3-engine](bt3-engine.md))

- `4dc3964c` 2026-10-27 Regulus CiberSec, +482 -278: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +1087 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +482 -278: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `5d3eea11` 2026-10-27 Regulus CiberSec, +1087 -0: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela

## `src/battle/battle.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +63 -12: Add native Tag Team modes

## `src/battle/battle_load.c` ([bt3-engine](bt3-engine.md))

- `fea06c41` 2026-10-31 dfdevx2, +6 -0: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +13 -0: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +85 -37: Add native Tag Team modes
- `744447a6` 2026-10-22 dfdevx2, +6 -0: Menu text colour, fight load crash fallback
- `f11e5f18` 2026-10-18 dfdevx2, +6 -0: Dragon Rage 0.8.0: Dragon Net
- `1867006b` 2026-10-17 dfdevx2, +14 -0: Dragon Rage 0.7.0
- `9ed9f8c9` 2026-10-17 z3xox, +14 -0: Online: no fighter has items (the Random item-set glitch)
- `7b99f8ad` 2026-10-15 dfdevx2, +15 -0: Dragon Rage 0.6.0 [skip ci]
- `fe4d05ac` 2026-10-11 z3xox, +15 -0: Online: each side of a match gets its player's own camera type and screen shake, read from his save and told to the other copy in the greeting (with 0.1.25 or earlier: the defaults, as before)
- `928b2413` 2026-10-11 dfdevx2, +12 -1: WIP: patch 1 and touch artwork
- `3e2b773d` 2026-10-08 z3xox, +8 -0: Added songs: file numbers past the archive's end (the 5th and 6th were the announcer's first two lines); up to 40
- `a3d30848` 2026-10-08 z3xox, +4 -1: Added stages: file numbers past the archive's end (30000.. are the training guide's voice lines)
- `755986ea` 2026-10-07 z3xox, +61 -6: Added stages: file ids that are no disc file's (0.1.8 gave a disc stage's model to an added stage's sound bank); stages larger than the game's buffer
- `28e86145` 2026-10-06 z3xox, +14 -0: Stages from outside the disc: manifest, install script, styled menu names

## `src/battle/battle_work.c` ([bt3-engine](bt3-engine.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +4 -1: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -2: Add native Tag Team modes
- `928b2413` 2026-10-11 dfdevx2, +7 -0: WIP: patch 1 and touch artwork
- `3e2b773d` 2026-10-08 z3xox, +7 -0: Added songs: file numbers past the archive's end (the 5th and 6th were the announcer's first two lines); up to 40

## `src/battle/bobj_a.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +117 -8: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -9: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/btl_act_a.c` ([bt3-engine](bt3-engine.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +16 -0: Fix giant grabs, ring-outs and fallen fighter recovery

## `src/battle/btl_act_c.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `src/battle/btl_act_d.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -0: Add native Tag Team modes

## `src/battle/btl_act_e.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +16 -6: Add native Tag Team modes
- `928b2413` 2026-10-11 dfdevx2, +3 -0: WIP: patch 1 and touch artwork
- `39d6c916` 2026-10-08 z3xox, +3 -0: BT3_CLASH_LOG=1: what a clash counts from input, and a beam clash's total

## `src/battle/btl_act_f.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -2: Add native Tag Team modes
- `928b2413` 2026-10-11 dfdevx2, +6 -0: WIP: patch 1 and touch artwork
- `39d6c916` 2026-10-08 z3xox, +6 -0: BT3_CLASH_LOG=1: what a clash counts from input, and a beam clash's total

## `src/battle/btl_act_h_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +46 -7: Add native Tag Team modes

## `src/battle/btl_act_j.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +50 -3: Add native Tag Team modes

## `src/battle/btl_ai_act.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +32 -22: Add native Tag Team modes

## `src/battle/btl_ai_cond.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +13 -13: Add native Tag Team modes

## `src/battle/btl_ai_mgr.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +28 -15: Add native Tag Team modes

## `src/battle/btl_ai_seq.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +14 -13: Add native Tag Team modes

## `src/battle/btl_cam.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +185 -3: Add native Tag Team modes
- `928b2413` 2026-10-11 dfdevx2, +26 -0: WIP: patch 1 and touch artwork
- `089995e1` 2026-10-10 z3xox, +26 -0: The fight's views follow the picture's shape when it is changed during a fight (their projection was only made at the fight's start: the picture was stretched until the next one)
- `7827a83d` 2026-10-07 z3xox, +11 -0: Online: each copy shows its own player's view full screen (the game's own priority-view mechanism); BT3_VIEW for testing
- `d26776bc` 2026-10-06 z3xox, +7 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/btl_capi_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -0: Add native Tag Team modes

## `src/battle/btl_char_action.c` ([bt3-engine](bt3-engine.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +5 -3: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +16 -1: Add native Tag Team modes

## `src/battle/btl_char_api.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -0: Add native Tag Team modes

## `src/battle/btl_char_cam_cut.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -8: Add native Tag Team modes

## `src/battle/btl_char_cam_modes.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +16 -3: Add native Tag Team modes

## `src/battle/btl_char_coll_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +21 -7: Add native Tag Team modes

## `src/battle/btl_char_ctl_d.c` ([bt3-engine](bt3-engine.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +38 -17: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +25 -8: Add native Tag Team modes

## `src/battle/btl_char_flag.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +22 -0: Add native Tag Team modes

## `src/battle/btl_char_flag_clash.c` ([bt3-engine](bt3-engine.md))

- `c98c4c6b` 2026-10-31 dfdevx2, +13 -5: Clash prompts: not in a match of the normal game
- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +2 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +234 -116: Add native Tag Team modes

## `src/battle/btl_char_flag_opp.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +10 -6: Add native Tag Team modes

## `src/battle/btl_char_flag_snd.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +61 -25: Add native Tag Team modes

## `src/battle/btl_char_fx.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -3: Add native Tag Team modes
- `07280401` 2026-10-08 Claude, +2 -2: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `697d9e08` 2026-10-06 z3xox, +0 -35: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/btl_char_fx_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -2: Fix disc resources, character previews and HUD controls

## `src/battle/btl_char_get.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +12 -2: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +7 -0: Dragon Rage 0.6.0 [skip ci]
- `f6265282` 2026-10-13 z3xox, +7 -0: Controls: the game's vibration reaches the controller (on / off and strength per player), a round deadzone per stick ("Original" unless set), and the sticks and triggers shown live

## `src/battle/btl_char_hit.c` ([bt3-engine](bt3-engine.md))

- `fea06c41` 2026-10-31 dfdevx2, +3 -0: Normal fights: the clash and the online random music as on the disc; a duplicate block; a filter for the texture dump
- `101de958` 2026-10-24 Tag Team Mod contributors, +32 -12: Add native Tag Team modes

## `src/battle/btl_char_member.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -4: Add native Tag Team modes

## `src/battle/btl_char_mgr.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +23 -8: Add native Tag Team modes

## `src/battle/btl_char_move.c` ([bt3-engine](bt3-engine.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +6 -0: Fix giant grabs, ring-outs and fallen fighter recovery

## `src/battle/btl_char_status_anim.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -2: Add native Tag Team modes

## `src/battle/btl_demo_cam.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +66 -0: Add native Tag Team modes

## `src/battle/btl_input.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +23 -2: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +13 -0: Dragon Rage 0.6.0 [skip ci]
- `d60fcab8` 2026-10-13 z3xox, +13 -0: Online: the pause menu no longer stops the fight

## `src/battle/btl_obj.c` ([bt3-engine](bt3-engine.md))

- `ac51a1ad` 2026-10-31 dfdevx2, +28 -1: Game files: say why a read failed, read again through a new file object; keep the old model
- `101de958` 2026-10-24 Tag Team Mod contributors, +59 -25: Add native Tag Team modes
- `744447a6` 2026-10-22 dfdevx2, +26 -0: Menu text colour, fight load crash fallback
- `5707f85b` 2026-10-22 dfdevx2, +34 -0: Fixes from logs, touch pause menu, BT4 menu

## `src/battle/btl_pool.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +58 -0: Add native Tag Team modes

## `src/battle/btl_replay.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -0: Add native Tag Team modes

## `src/battle/btl_scene.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +37 -1: Add native Tag Team modes
- `744447a6` 2026-10-22 dfdevx2, +13 -0: Menu text colour, fight load crash fallback
- `7b99f8ad` 2026-10-15 dfdevx2, +15 -0: Dragon Rage 0.6.0 [skip ci]
- `66682403` 2026-10-11 z3xox, +15 -0: Online: only the view's own fighter counts as in view, as in the halves of a split screen (the opponent's aura drew its dark layer in front of whatever he stood behind)

## `src/battle/btl_script.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +3 -3: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -4: Add native Tag Team modes

## `src/battle/btl_script_cmd.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -4: Fix disc resources, character previews and HUD controls
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/btl_seq.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +158 -11: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +37 -3: Dragon Rage 0.6.0 [skip ci]
- `d60fcab8` 2026-10-13 z3xox, +37 -3: Online: the pause menu no longer stops the fight

## `src/battle/btl_tech_a.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +28 -0: Add native Tag Team modes

## `src/battle/btl_tech_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +28 -0: Add native Tag Team modes

## `src/battle/col_c.c` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +15 -0: Dragon Rage 0.7.3
- `316b567c` 2026-10-16 victorgonl, +15 -0: Add support for dialogue and ui texts translations (no documentation yet)
- `7b99f8ad` 2026-10-15 dfdevx2, +14 -0: Dragon Rage 0.6.0 [skip ci]
- `1ffcdf72` 2026-10-13 z3xox, +8 -0: Menu fonts: the glyph table hook is an #ifdef PORT in col_c.c, not a patch of the source at build time
- `715afc18` 2026-10-11 piapenia, +6 -0: a11y: quieter guides, no subtitles, screens without a reader of their own

## `src/battle/eft_a.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +82 -4: Add native Tag Team modes
- `5864f1d8` 2026-10-06 z3xox, +32 -0: Port: in-place polygon clippers work on a copy with room (demo fight crash); notes
- `d26776bc` 2026-10-06 z3xox, +12 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_aa.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +57 -6: Add native Tag Team modes
- `19610dcc` 2026-10-19 z3xox, +13 -0: Online: sound effects and Solar Flare with rollback (an effect that only the run again of a blank started is heard, a few blanks late: the other player's vanish or counter was often silent; one heard on a wrong guess and not started again is stopped, and a voice still playing takes the handle the run again gave it: a ki charge's loop went on the whole fight; the blinded player's overlay is drawn in the online full-screen view, where the game drew none)

## `src/battle/eft_ab.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_ab_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +82 -7: Add native Tag Team modes

## `src/battle/eft_ab_c.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -4: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_ad.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -2: Add native Tag Team modes

## `src/battle/eft_ad_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +7 -7: Add native Tag Team modes
- `5864f1d8` 2026-10-06 z3xox, +16 -0: Port: in-place polygon clippers work on a copy with room (demo fight crash); notes

## `src/battle/eft_ae.c` ([bt3-engine](bt3-engine.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +14 -2: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `101de958` 2026-10-24 Tag Team Mod contributors, +64 -44: Add native Tag Team modes
- `5864f1d8` 2026-10-06 z3xox, +8 -0: Port: in-place polygon clippers work on a copy with room (demo fight crash); notes

## `src/battle/eft_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -6: Add native Tag Team modes
- `d26776bc` 2026-10-06 z3xox, +8 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/eft_c.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +57 -2: Add native Tag Team modes
- `d7bb04e8` 2026-10-22 z3xox, +3 -0: Settings, Effects, Weather: Full or Light. With Weather on, a choice below it: Light draws half of the haze's sprites (fx_off bit 128), a thinner haze for about half of what switching it off gains: Desert at noon at 3x on an integrated chip 57.2 pictures a second (Full 52.6, off 62.6), Rocky Area at noon 61.3 (56.8, 67.7). The fight the same either way
- `95fd1797` 2026-10-22 z3xox, +7 -0: Settings, Effects: Weather. The dust, snow and clouds some stages have drifting over them (the stage's weather record, EftWeather: Desert and Rocky Area at noon, Rocky Area in the evening, Kami's Lookout, Glacier, Muscle Tower, the Time Chamber) are 12 to 30 big soft sprites, up to 400 units across, blended over much of the picture: the reason those stages are heavier than the others. Off, the particles go on moving and are not drawn (fx_off bit 64). Desert at noon at 3x on an integrated chip 61 pictures a second instead of 50, Rocky Area at noon 65 instead of 54; the fight the same either way
- `236caf0a` 2026-10-22 z3xox, +7 -0: Settings, Effects: Weather. The dust, snow and clouds some stages have drifting over them (the stage's weather record, EftWeather: Desert and Rocky Area at noon, Rocky Area in the evening, Kami's Lookout, Glacier, Muscle Tower, the Time Chamber) are 12 to 30 big soft sprites, up to 400 units across, blended over much of the picture: the reason those stages are heavier than the others. Off, the particles go on moving and are not drawn (fx_off bit 64). Desert at noon at 3x on an integrated chip 61 pictures a second instead of 50, Rocky Area at noon 65 instead of 54; the fight the same either way
- `07280401` 2026-10-08 Claude, +4 -4: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `5864f1d8` 2026-10-06 z3xox, +24 -0: Port: in-place polygon clippers work on a copy with room (demo fight crash); notes
- `697d9e08` 2026-10-06 z3xox, +0 -15: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_d.c` ([bt3-engine](bt3-engine.md))

- `5864f1d8` 2026-10-06 z3xox, +8 -0: Port: in-place polygon clippers work on a copy with room (demo fight crash); notes

## `src/battle/eft_d_b.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -4: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_det_a.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +57 -16: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -9: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_det_b_c.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes

## `src/battle/eft_e.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +22 -21: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +12 -0: Dragon Rage 0.6.0 [skip ci]
- `781b52d7` 2026-10-11 z3xox, +12 -0: A water ring that has grown past any map is not drawn: cut off at a 16:9 view's edges its corners came out behind the eye, a flat blue triangle across the picture after leaving the water
- `5864f1d8` 2026-10-06 z3xox, +8 -0: Port: in-place polygon clippers work on a copy with room (demo fight crash); notes

## `src/battle/eft_g.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +15 -9: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +16 -0: Dragon Rage 0.6.0 [skip ci]
- `c39c5c25` 2026-10-11 z3xox, +16 -0: In-between pictures: the wall at the edge of the map is drawn from the game's own patch, a surface keeps its identity when its texture changes (a waterfall jumped a step to the side); what the game transforms itself leads by two depth steps (the fall's filled part was a speckle in motion)
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_h.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -4: Add native Tag Team modes
- `07280401` 2026-10-08 Claude, +2 -2: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `697d9e08` 2026-10-06 z3xox, +0 -15: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_i.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -2: Add native Tag Team modes
- `07280401` 2026-10-08 Claude, +26 -26: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `697d9e08` 2026-10-06 z3xox, +0 -74: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_j.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +33 -3: Add native Tag Team modes
- `07280401` 2026-10-08 Claude, +2 -2: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_k.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +30 -5: Add native Tag Team modes
- `07280401` 2026-10-08 Claude, +2 -2: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `697d9e08` 2026-10-06 z3xox, +0 -10: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_l_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes

## `src/battle/eft_l_c.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes

## `src/battle/eft_l_d.c` ([bt3-engine](bt3-engine.md))

- `07280401` 2026-10-08 Claude, +4 -4: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `697d9e08` 2026-10-06 z3xox, +0 -10: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_n.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -6: Add native Tag Team modes

## `src/battle/eft_o_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -1: Add native Tag Team modes

## `src/battle/eft_o_c.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +14 -6: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_p.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +33 -31: Add native Tag Team modes

## `src/battle/eft_q.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -8: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_r.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +84 -95: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -10: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_s.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -3: Add native Tag Team modes
- `5864f1d8` 2026-10-06 z3xox, +8 -0: Port: in-place polygon clippers work on a copy with room (demo fight crash); notes
- `697d9e08` 2026-10-06 z3xox, +0 -9: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_t_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -1: Add native Tag Team modes

## `src/battle/eft_t_c.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -4: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_u_b.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -4: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_v_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -1: Add native Tag Team modes

## `src/battle/eft_w.c` ([bt3-engine](bt3-engine.md))

- `07280401` 2026-10-08 Claude, +4 -4: Android: fix argument passing of effect vectors (beam clash crash); L3/R3 shown
- `d26776bc` 2026-10-06 z3xox, +8 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window
- `697d9e08` 2026-10-06 z3xox, +0 -10: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/eft_x.c` ([bt3-engine](bt3-engine.md))

- `d26776bc` 2026-10-06 z3xox, +8 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/eft_z.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -3: Add native Tag Team modes

## `src/battle/hud_0.c` ([bt3-engine](bt3-engine.md))

- `4c222dcc` 2026-10-26 z3xox, +3 -1: Lobbies: the after-battle menu of the player who is not the lobby's owner keeps its title row. The lobby builds that menu from the game's own entries it wants (Fight Again, Return to Character Select), and left out the title picture, a row with a negative text number; BtlMenu_Draw starts the words one row down for it, so they sat a row low and Return to Character Select was drawn below the box (seen by the user). The pause menu was built by leaving entries out and kept it. The PS2 build is unchanged (PORT only)
- `6f99aff2` 2026-10-26 z3xox, +8 -3: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `a581c3f7` 2026-10-26 z3xox, +29 -0: A spectator's own pause menu is the battle's own pause menu: its box from the battle's sprite sheet, drawn by its engine (BtlMenu_Draw, src/battle/hud_0_b.c, through BtlMenu_PortDrawMenu), its colours, sounds and six-frame growing box, and the versus pause menu's own "EXIT ?" Yes / No box before leaving (PauseMenu_PortConfirmBox, src/battle/hud_0.c), instead of a Tenkaichi Budokai Online panel. Its items (port/src/spec_menu.c): the note "Your screen only: the fight goes on" in the title's row, Continue, View: Player 1 / Player 2 / Split Screen with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching; their texts reach the engine through BtlMenu_GetItemText (numbers 0x6FFF and on) and are translated by the online menus' file (tbo_strings.py lists the file). The engine's update is not run and the battle's menus are not touched: the file moves the cursor and the box itself and reads the PC's pad (Port_PadRead); in a fight only (gPortBtlMenuLive). Tested by code with an owner and a member watching and every item used with the pad: the fight values of both spectators and both players identical, no check value differed. All game-side changes under PORT: the PS2 build is unchanged.
- `ef2b70e2` 2026-10-26 z3xox, +71 -1: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.
- `101de958` 2026-10-24 Tag Team Mod contributors, +47 -0: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +63 -0: Dragon Rage 0.6.0 [skip ci]
- `083a6f2c` 2026-10-13 z3xox, +17 -0: Dragon History: Fight Again in the pause menu
- `982035e5` 2026-10-13 z3xox, +4 -3: Online: no "View Replay Data" at the end of a battle either
- `0ea678e5` 2026-10-13 z3xox, +32 -0: Online: no "Return to Vs Menu" and "Save Replay Data" at the end of a battle
- `d60fcab8` 2026-10-13 z3xox, +13 -0: Online: the pause menu no longer stops the fight
- `d26776bc` 2026-10-06 z3xox, +6 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/hud_0_b.c` ([bt3-engine](bt3-engine.md))

- `6f99aff2` 2026-10-26 z3xox, +3 -2: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `a581c3f7` 2026-10-26 z3xox, +66 -1: A spectator's own pause menu is the battle's own pause menu: its box from the battle's sprite sheet, drawn by its engine (BtlMenu_Draw, src/battle/hud_0_b.c, through BtlMenu_PortDrawMenu), its colours, sounds and six-frame growing box, and the versus pause menu's own "EXIT ?" Yes / No box before leaving (PauseMenu_PortConfirmBox, src/battle/hud_0.c), instead of a Tenkaichi Budokai Online panel. Its items (port/src/spec_menu.c): the note "Your screen only: the fight goes on" in the title's row, Continue, View: Player 1 / Player 2 / Split Screen with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching; their texts reach the engine through BtlMenu_GetItemText (numbers 0x6FFF and on) and are translated by the online menus' file (tbo_strings.py lists the file). The engine's update is not run and the battle's menus are not touched: the file moves the cursor and the box itself and reads the PC's pad (Port_PadRead); in a fight only (gPortBtlMenuLive). Tested by code with an owner and a member watching and every item used with the pad: the fight values of both spectators and both players identical, no check value differed. All game-side changes under PORT: the PS2 build is unchanged.
- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -4: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +30 -0: Dragon Rage 0.6.0 [skip ci]
- `2574a9df` 2026-10-13 Ahmed, +30 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens

## `src/battle/hud_a.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +337 -196: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +18 -2: Dragon Rage 0.6.0 [skip ci]
- `053be7ac` 2026-10-15 victorgonl, +11 -2: Fix battle announcement in widescreen
- `2574a9df` 2026-10-13 Ahmed, +7 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens
- `928b2413` 2026-10-11 dfdevx2, +11 -0: WIP: patch 1 and touch artwork
- `92b6e745` 2026-10-08 z3xox, +11 -0: Clashes: with the Square option on, the clash prompt is the game's own mash prompt with Square (not the turning stick)
- `a232ce90` 2026-10-06 z3xox, +5 -1: Port: any aspect ratio (BT3_ASPECT, ultrawide), captions anchored by side
- `d26776bc` 2026-10-06 z3xox, +17 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/hud_a_b.c` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +2 -0: Dragon Rage 0.6.0 [skip ci]
- `ae803c6f` 2026-10-11 Mikay, +2 -0: Matching build: HUD_SCR on the PS2 side, stage name UV order in CharSel_Draw
- `928b2413` 2026-10-11 dfdevx2, +9 -2: WIP: patch 1 and touch artwork
- `5d4e9bc9` 2026-10-09 z3xox, +9 -2: Online play: the joining player's own side of the HUD is on the left (gauges, team, prompts, counters, technique names); the players' names under the gauges
- `d26776bc` 2026-10-06 z3xox, +12 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/hud_a_d.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +14 -2: Add native Tag Team modes

## `src/battle/hud_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +52 -86: Fix disc resources, character previews and HUD controls
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +27 -17: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +115 -5: Add native Tag Team modes
- `5707f85b` 2026-10-22 dfdevx2, +20 -0: Fixes from logs, touch pause menu, BT4 menu
- `7b99f8ad` 2026-10-15 dfdevx2, +124 -11: Dragon Rage 0.6.0 [skip ci]
- `ae803c6f` 2026-10-11 Mikay, +2 -0: Matching build: HUD_SCR on the PS2 side, stage name UV order in CharSel_Draw
- `96f490c5` 2026-10-11 z3xox, +2 -1: A fight's display in a wide picture: at the edges, at the edges with long health bars (pull request #28), or in the middle as in 4:3
- `928b2413` 2026-10-11 dfdevx2, +14 -5: WIP: patch 1 and touch artwork
- `640d2024` 2026-10-10 victorgonl, +121 -11: Extend widescreen life bars toward the center timer
- `1ca2857d` 2026-10-09 z3xox, +4 -2: HUD swap: a fighter's portrait is the picture drawn for its place on the screen (each face sheet has one for the left and one for the right)
- `5d4e9bc9` 2026-10-09 z3xox, +10 -3: Online play: the joining player's own side of the HUD is on the left (gauges, team, prompts, counters, technique names); the players' names under the gauges
- `d26776bc` 2026-10-06 z3xox, +12 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/hud_c_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +18 -0: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +2 -0: Dragon Rage 0.6.0 [skip ci]
- `ae803c6f` 2026-10-11 Mikay, +2 -0: Matching build: HUD_SCR on the PS2 side, stage name UV order in CharSel_Draw
- `928b2413` 2026-10-11 dfdevx2, +20 -7: WIP: patch 1 and touch artwork
- `fe216754` 2026-10-11 z3xox, +6 -0: Widescreen: each side's technique name is placed with its own panel (player 1's was narrowed about the right edge: both names are drawn after both combo trees)
- `38909b09` 2026-10-09 z3xox, +1 -1: HUD swap: the skill and technique names are aligned by the side's place on the screen too (they ran off the edge)
- `5d4e9bc9` 2026-10-09 z3xox, +13 -6: Online play: the joining player's own side of the HUD is on the left (gauges, team, prompts, counters, technique names); the players' names under the gauges
- `d26776bc` 2026-10-06 z3xox, +12 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/hud_e.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -2: Fix disc resources, character previews and HUD controls

## `src/battle/hud_e_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +7 -0: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +2 -0: Dragon Rage 0.6.0 [skip ci]
- `ae803c6f` 2026-10-11 Mikay, +2 -0: Matching build: HUD_SCR on the PS2 side, stage name UV order in CharSel_Draw
- `928b2413` 2026-10-11 dfdevx2, +19 -9: WIP: patch 1 and touch artwork
- `fe216754` 2026-10-11 z3xox, +3 -0: Widescreen: each side's technique name is placed with its own panel (player 1's was narrowed about the right edge: both names are drawn after both combo trees)
- `38909b09` 2026-10-09 z3xox, +1 -1: HUD swap: the skill and technique names are aligned by the side's place on the screen too (they ran off the edge)
- `5d4e9bc9` 2026-10-09 z3xox, +15 -8: Online play: the joining player's own side of the HUD is on the left (gauges, team, prompts, counters, technique names); the players' names under the gauges
- `d26776bc` 2026-10-06 z3xox, +12 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window

## `src/battle/hud_e_d.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +11 -1: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +7 -0: Dragon Rage 0.6.0 [skip ci]
- `d60fcab8` 2026-10-13 z3xox, +7 -0: Online: the pause menu no longer stops the fight

## `src/battle/stg_a.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +19 -0: Add native Tag Team modes
- `d26776bc` 2026-10-06 z3xox, +12 -0: Port: widescreen (BT3_WIDE): wider view and culling in the game's copy, 2D kept in proportion with HUD panels anchored, aspect-correct window
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/stg_a_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +82 -1: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -5: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/battle/stg_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -1: Add native Tag Team modes
- `80ed18e4` 2026-10-21 z3xox, +13 -0: Distance blur at higher internal resolutions: StgFog_Draw marks its passes (0x20 / 0x21, as the glow chain), so page 0x150 holds the half-size copy at the PS2's resolution and stretching it back blurs as on the console (at 6x it took a twelfth of the detail out of distant scenery, now as at 1x); the picture shrunk into such a page is read as the GS reads it at 1x, each GS texel the average of its block (the copy showed the leaves as squares); 1x unchanged, the glow within 2 of 255
- `928b2413` 2026-10-11 dfdevx2, +3 -5: WIP: patch 1 and touch artwork
- `021fc2fc` 2026-10-10 z3xox, +3 -5: The cross-fade between the shots of a fight's intro: the game's read-back of the frame is answered (software renderer and both GPU back ends)

## `src/battle/stg_c.c` ([bt3-engine](bt3-engine.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +10 -6: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `101de958` 2026-10-24 Tag Team Mod contributors, +44 -22: Add native Tag Team modes

## `src/battle/stg_d_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -0: Add native Tag Team modes

## `src/battle/stgm_a.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -0: Add native Tag Team modes

## `src/battle/tag_assist.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +349 -0: Add native Tag Team modes

## `src/battle/tag_aura_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +18 -0: Add native Tag Team modes

## `src/battle/tag_balance.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +119 -0: Add native Tag Team modes

## `src/battle/tag_beam_test.c` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +69 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +247 -0: Add native Tag Team modes

## `src/battle/tag_blind_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +30 -0: Add native Tag Team modes

## `src/battle/tag_bodyfx_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +18 -0: Add native Tag Team modes

## `src/battle/tag_bubble_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +25 -0: Add native Tag Team modes

## `src/battle/tag_camera_test.c` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +71 -2: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +136 -0: Add native Tag Team modes

## `src/battle/tag_challenge.c` ([tag-team](tag-team.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +3 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +367 -0: Add native Tag Team modes

## `src/battle/tag_cinema_matrix_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +277 -0: Add native Tag Team modes

## `src/battle/tag_clash_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +81 -0: Add native Tag Team modes

## `src/battle/tag_controls_test.c` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +130 -6: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +316 -0: Add native Tag Team modes

## `src/battle/tag_cpu.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +145 -0: Add native Tag Team modes

## `src/battle/tag_cpu_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +88 -0: Add native Tag Team modes

## `src/battle/tag_dust_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +20 -0: Add native Tag Team modes

## `src/battle/tag_effect_slots_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +43 -0: Add native Tag Team modes

## `src/battle/tag_effects_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +100 -0: Add native Tag Team modes

## `src/battle/tag_fusion.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +304 -0: Add native Tag Team modes

## `src/battle/tag_fusion_matrix_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +114 -0: Add native Tag Team modes

## `src/battle/tag_fusion_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +197 -0: Add native Tag Team modes

## `src/battle/tag_gauntlets.inc` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +865 -0: Add native Tag Team modes

## `src/battle/tag_giant.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +61 -0: Add native Tag Team modes

## `src/battle/tag_giant_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +80 -0: Add native Tag Team modes

## `src/battle/tag_hud.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +172 -0: Add native Tag Team modes

## `src/battle/tag_hud_borders.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +32 -0: Add native Tag Team modes

## `src/battle/tag_lobby.c` ([tag-team](tag-team.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +341 -0: Add native Tag Team modes

## `src/battle/tag_lobby_rules.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +283 -0: Add native Tag Team modes

## `src/battle/tag_online.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +117 -0: Add native Tag Team modes

## `src/battle/tag_online_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +139 -0: Add native Tag Team modes

## `src/battle/tag_position.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +180 -0: Add native Tag Team modes

## `src/battle/tag_revive.c` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +36 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +142 -0: Add native Tag Team modes

## `src/battle/tag_spawn.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +246 -0: Add native Tag Team modes

## `src/battle/tag_special_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +103 -0: Add native Tag Team modes

## `src/battle/tag_stage.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +24 -0: Add native Tag Team modes

## `src/battle/tag_stage_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +207 -0: Add native Tag Team modes

## `src/battle/tag_team.c` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +3 -0: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +854 -0: Add native Tag Team modes

## `src/battle/tag_team_test.c` ([tag-team](tag-team.md))

- `4d01ef54` 2026-10-27 Tag Team Mod contributors, +9 -1: Fix giant grabs, ring-outs and fallen fighter recovery
- `101de958` 2026-10-24 Tag Team Mod contributors, +226 -0: Add native Tag Team modes

## `src/battle/tag_transform_fx_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +34 -0: Add native Tag Team modes

## `src/battle/tag_water_test.c` ([tag-team](tag-team.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +15 -0: Add native Tag Team modes

## `src/battle/view_a_b.c` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +51 -0: Dragon Rage 0.6.0 [skip ci]
- `ddf2ddfc` 2026-10-15 z3xox, +37 -23: TBO: the hosts line in the game box itself, in the game font, by its text box (the screen reader reads it as the game lines)
- `a864caad` 2026-10-14 z3xox, +37 -0: TBO: hosts line where the game writes in its box; heading window without its words

## `src/battle/view_a_c.c` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +10 -0: Dragon Rage 0.6.0 [skip ci]
- `17a96294` 2026-10-14 z3xox, +3 -1: TBO: the heading window without the Duel menu emblem as well
- `a864caad` 2026-10-14 z3xox, +8 -0: TBO: hosts line where the game writes in its box; heading window without its words

## `src/battle/view_a_d.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +40 -2: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -2: Add native Tag Team modes

## `src/battle/view_a_e.c` ([bt3-engine](bt3-engine.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +19 -1: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +56 -7: Add native Tag Team modes

## `src/battle/view_b.c` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +8 -0: Dragon Rage 0.7.3
- `316b567c` 2026-10-16 victorgonl, +8 -0: Add support for dialogue and ui texts translations (no documentation yet)

## `src/battle/view_b_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -2: Add native Tag Team modes

## `src/battle/view_b_c.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +25 -0: Add native Tag Team modes

## `src/battle/view_b_d.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls

## `src/menu/menu_a.c` ([bt3-engine](bt3-engine.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +224 -47: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `3f9ad0fc` 2026-10-28 Regulus CiberSec, +26 -7: Tag Team: polimento do menu do SELECT com as peças do Dragon Net Online
- `4dc3964c` 2026-10-27 Regulus CiberSec, +6 -2: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +6 -2: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `a3cf1192` 2026-10-28 z3xox, +3 -2: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `02d1e354` 2026-10-26 Tag Team Mod contributors, +8 -9: Simplify the native online menu
- `18d6fd06` 2026-10-26 z3xox, +12 -0: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `101de958` 2026-10-24 Tag Team Mod contributors, +277 -4: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +108 -13: Dragon Rage 0.6.0 [skip ci]
- `3d8b2901` 2026-10-15 z3xox, +5 -0: Main menu: allocated at its PC size (TBO made it 4 bytes longer than the 0x188 allocated); the screen reader reads its 12 items again, Tenkaichi Budokai Online by name
- `178b177d` 2026-10-15 z3xox, +5 -9: Port screens: a progress mode of their own (80, PortScreen_Main by gPortScreen); TBO through it, Duel_Main the game own again
- `dbc8e227` 2026-10-14 z3xox, +3 -0: TBO: listed once on the main menu (item 11 was also listed at the end); the sparring sign without the crest, its padding even
- `c19b9004` 2026-10-14 z3xox, +25 -3: TBO: the main menu item lettered from the disc: its letters cut from the other labels (and the stage names, for the k)
- `2574a9df` 2026-10-13 Ahmed, +82 -13: Tenkaichi Budokai Online: a main-menu entry with its own online screens
- `de855229` 2026-10-07 z3xox, +7 -0: Online session flow: lobby (Host / Join), restart into a synchronised session that opens on the versus character select with everything unlocked, leaving back to the normal game
- `84fae11b` 2026-10-06 z3xox, +2 -1: Dragon Net Battle entry: the switch that hides it is read on the port's side (a pointer from getenv in the game's file crashed the 64-bit build)
- `d5c835a5` 2026-10-06 z3xox, +11 -3: Dragon Net Battle is listed in the main menu and opens the port's online screen (a shell: host / join fields, nothing sent yet)
- `75dc0835` 2026-10-06 z3xox, +8 -0: Main menu: BT3_MENU_ITEM4 lists the hidden Dragon Net Battle entry; notes

## `src/menu/menu_a_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -2: Fix disc resources, character previews and HUD controls
- `ef2b70e2` 2026-10-26 z3xox, +10 -0: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.
- `101de958` 2026-10-24 Tag Team Mod contributors, +3 -0: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +19 -0: Dragon Rage 0.6.0 [skip ci]
- `178b177d` 2026-10-15 z3xox, +9 -3: Port screens: a progress mode of their own (80, PortScreen_Main by gPortScreen); TBO through it, Duel_Main the game own again
- `2574a9df` 2026-10-13 Ahmed, +13 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens

## `src/menu/menu_a_c.c` ([bt3-engine](bt3-engine.md))

- `7b99f8ad` 2026-10-15 dfdevx2, +30 -1: Dragon Rage 0.6.0 [skip ci]
- `65086467` 2026-10-14 victorgonl, +18 -1: Quit game dialog using the game’s UI
- `f5e637be` 2026-10-13 z3xox, +4 -2: Online: a session's start-up no longer follows "Skip logos" / "Skip intro video" (one side with either on reached the character select 537 blanks early and the two games never agreed again); the two copies now compare what they compute and end the match with a message when it differs; netcode version 4
- `75f80aa9` 2026-10-12 z3xox, +10 -0: Skip copyright page: a third setting on the UI tab (the 300-frame still at the start of the title screen)

## `src/menu/menu_a_d.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +7 -7: Fix disc resources, character previews and HUD controls

## `src/menu/menu_b_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +9 -9: Fix disc resources, character previews and HUD controls

## `src/menu/menu_b_d.c` ([bt3-engine](bt3-engine.md))

- `928b2413` 2026-10-11 dfdevx2, +10 -0: WIP: patch 1 and touch artwork
- `1e16092e` 2026-10-08 z3xox, +10 -0: Story mode: a won battle that opens the first episode of a new saga always lists the saga (an original read of uninitialised stack; on PC it could keep the Frieza Saga off the list after the Vegeta fight); saves that have it are repaired when the saga select opens

## `src/menu/menu_c_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -2: Fix disc resources, character previews and HUD controls
- `928b2413` 2026-10-11 dfdevx2, +8 -0: WIP: patch 1 and touch artwork
- `1e16092e` 2026-10-08 z3xox, +8 -0: Story mode: a won battle that opens the first episode of a new saga always lists the saga (an original read of uninitialised stack; on PC it could keep the Frieza Saga off the list after the Vegeta fight); saves that have it are repaired when the saga select opens

## `src/menu/menu_c_e.c` ([bt3-engine](bt3-engine.md))

- `6f99aff2` 2026-10-26 z3xox, +23 -0: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +6 -6: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +49 -20: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +32 -6: Dragon Rage 0.6.0 [skip ci]
- `b5160626` 2026-10-13 z3xox, +3 -1: Added songs: MP3 files are converted in the background, the start does not wait; a song joins the list when a select screen is next set up (at start in a recorded or re-run session, never in an online one)
- `d0e044ba` 2026-10-12 z3xox, +24 -2: "Random" in the duel / training and team selects also draws the stages and songs added from outside the disc
- `ae803c6f` 2026-10-11 Mikay, +5 -3: Matching build: HUD_SCR on the PS2 side, stage name UV order in CharSel_Draw
- `928b2413` 2026-10-11 dfdevx2, +9 -6: WIP: patch 1 and touch artwork
- `3e2b773d` 2026-10-08 z3xox, +9 -6: Added songs: file numbers past the archive's end (the 5th and 6th were the announcer's first two lines); up to 40
- `153b81a3` 2026-10-08 z3xox, +6 -3: Stage select: room for all 26 added stages (24 were listed with every disc stage unlocked)
- `e73fb408` 2026-10-07 z3xox, +10 -2: Added stage's and song's names: only on the screen whose movie has the name clip
- `d3bccc25` 2026-10-07 z3xox, +11 -1: Names of added stages and songs in the game's own lettering, cut from the disc's name pictures at run time
- `4000ea6c` 2026-10-07 z3xox, +15 -2: Added song's name: its place from whichever of the two name clips the menu's state has
- `a16577da` 2026-10-07 z3xox, +4 -0: Name strips of added stages and songs: read from the game's data folder (BT3_DATA), and drawn under OpenGL too
- `590bce70` 2026-10-07 z3xox, +17 -0: Added songs and stages: the game's own name clips come back when the list returns to an entry of the disc
- `0153b616` 2026-10-06 z3xox, +106 -2: Songs added to the music select: manifest, script, and the name overlay
- `2902d41b` 2026-10-07 z3xox, +17 -10: Added stages: nothing written to the save, none in an online session, the game's own grid when none is installed
- `28e86145` 2026-10-06 z3xox, +146 -3: Stages from outside the disc: manifest, install script, styled menu names

## `src/menu/menu_e_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +12 -1: Fix disc resources, character previews and HUD controls
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +4 -3: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +426 -40: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +25 -2: Dragon Rage 0.6.0 [skip ci]
- `b5160626` 2026-10-13 z3xox, +3 -1: Added songs: MP3 files are converted in the background, the start does not wait; a song joins the list when a select screen is next set up (at start in a recorded or re-run session, never in an online one)
- `d0e044ba` 2026-10-12 z3xox, +22 -1: "Random" in the duel / training and team selects also draws the stages and songs added from outside the disc
- `928b2413` 2026-10-11 dfdevx2, +236 -1: WIP: patch 1 and touch artwork
- `e112aff2` 2026-10-10 z3xox, +119 -2: Added stages: offered in team and DP battles too (the team select's own stage list had been left without them)
- `7043a3e4` 2026-10-10 z3xox, +4 -0: Added songs in the team select: the song chosen from the list is started with its own file number too (it was only when the screen opened)
- `ca55bb2c` 2026-10-10 z3xox, +114 -0: Added songs: offered in team and DP battles too (the team select's own music list had been left without them); a song without loop points is played round like the disc's music
- `28e86145` 2026-10-06 z3xox, +14 -0: Stages from outside the disc: manifest, install script, styled menu names

## `src/menu/menu_g.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -1: Add native Tag Team modes

## `src/menu/menu_g_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls
- `6f99aff2` 2026-10-26 z3xox, +6 -7: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `ef2b70e2` 2026-10-26 z3xox, +11 -4: Lobbies: up to eight games meet at the game of the one who made the lobby, two play and up to six watch. Dragon Net Battle's new Lobby tab creates one without asking anything (a room code, or a port); others join with the code or the address on the Join tab, and the answer says it is a lobby. The lobby's window shows the two player places and six spectator places (the owner picks a name, then a place, and the two swap; Out removes someone), the rules as rows of the game's menus with left / right (battle type and DP, time limit, speed, rollback, input delay: what the Host tab offers; they cannot change during a match), Ready, Start, Leave, and a chat for everyone. The owner's game keeps the lobby (port/src/gs/net_lobby.c, packets 30 to 40 that older releases ignore) and tells each member its part in a match: the game in 1P hosts it, the one in 2P joins it, everyone else watches it as a spectator of the owner, whose game passes the match on whatever its own part is (host, the one who joined, or a spectator itself), so members only ever talk to the owner. The match is the usual session on the lobby's own socket, which comes back to the lobby when it ends. Only the owner goes back to the lobby: the game's pause menu and its menu after a battle have "Return to Vs Menu" for the owner's side and lose it and "Return to Main Menu" for the other player's (src/battle/hud_0.c, chosen by the menu's pad, the owner's side travelling in the rules word so that every copy agrees; src/menu/menu_a_b.c ends the session there; going back from the character select opens it again, src/menu/menu_g_b.c), F1 has "Back to the lobby", and a spectating owner its own pause menu. A spectator's pause menu (START) is its own: change view, leave, and for the owner back to the lobby. A spectator leaving changes nothing; a player leaving ends the match and everyone is back with "NAME left the lobby"; the owner leaving closes the lobby for all; a ninth game is told the lobby is full; a 0.1.38.7 knocking by address is answered as by a host of another release. The spectator's "Joining the match as a spectator..." no longer stays on its screen, and the test line's queue (BT3_NET_LATENCY) no longer carries packets from one session into the next. Testing: BT3_LOBBY=lobby / lobby:<port> / lobbyjoin:<address>:<port>, BT3_LOBBY_DO (start, back, leave, kick, move, rule, ready, say, waits), BT3_SPEC_PAUSE_AT. All game-side changes are under PORT: the PS2 build is unchanged.
- `101de958` 2026-10-24 Tag Team Mod contributors, +12 -0: Add native Tag Team modes
- `178b177d` 2026-10-15 z3xox, +0 -30: Port screens: a progress mode of their own (80, PortScreen_Main by gPortScreen); TBO through it, Duel_Main the game own again
- `eed0c971` 2026-10-14 z3xox, +7 -1: TBO: a match found from its screens goes to the character select (the session cleared the screens role); the early break of Duel_Main is PC code only
- `2574a9df` 2026-10-13 Ahmed, +24 -0: Tenkaichi Budokai Online: a main-menu entry with its own online screens
- `928b2413` 2026-10-11 dfdevx2, +19 -0: WIP: patch 1 and touch artwork
- `18c32e30` 2026-10-09 z3xox, +19 -0: Online play: the host chooses the battle (single, team, DP battle 10 / 15 / 20) and the time limit in the lobby window; no versus menu in a session; netcode version 3

## `src/menu/menu_g_c.c` ([bt3-engine](bt3-engine.md))

- `7e09775c` 2026-10-09 z3xox, +0 -10: Revert "Online play: the session opens on the game's own versus menu (single, team or DP battle, the DP limit, the battle settings); netcode version 3"
- `f7408a40` 2026-10-09 z3xox, +6 -30: Revert "Online play: the session opens on the battle type itself (single, team, DP battle); in the list behind it the two rows with COM are greyed and skipped"
- `a8a5d08e` 2026-10-09 z3xox, +30 -6: Online play: the session opens on the battle type itself (single, team, DP battle); in the list behind it the two rows with COM are greyed and skipped
- `df6337f9` 2026-10-09 z3xox, +10 -0: Online play: the session opens on the game's own versus menu (single, team or DP battle, the DP limit, the battle settings); netcode version 3

## `src/menu/menu_h_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +10 -5: Fix disc resources, character previews and HUD controls
- `7b99f8ad` 2026-10-15 dfdevx2, +12 -0: Dragon Rage 0.6.0 [skip ci]
- `c5019a96` 2026-10-12 z3xox, +12 -0: The character reference's model viewer is shown with the wide view, not stretched like the menus around it

## `src/menu/menu_h_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +2 -2: Fix disc resources, character previews and HUD controls

## `src/menu/menu_i_c.c` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +6 -0: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +6 -0: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +21 -2: Dragon Rage 0.6.0 [skip ci]
- `f5e637be` 2026-10-13 z3xox, +6 -1: Online: a session's start-up no longer follows "Skip logos" / "Skip intro video" (one side with either on reached the character select 537 blanks early and the two games never agreed again); the two copies now compare what they compute and end the match with a message when it differs; netcode version 4
- `4c200231` 2026-10-12 z3xox, +14 -12: Skip logos / skip intro (PR #50) in the port's style: the checks inside PORT, the two boxes on the UI tab, no new tab or header
- `2c4a57ae` 2026-10-11 Renzo, +11 -5: Add QoL settings to skip intro and logos
- `738157dd` 2026-10-09 Renzo, +11 -5: .

## `src/menu/menu_i_d.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +8 -4: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +6 -2: Add native Tag Team modes

## `src/menu/menu_k_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +20 -34: Add native Tag Team modes

## `src/menu/menu_k_f.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +24 -4: Add native Tag Team modes

## `src/menu/menu_l_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +6 -2: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -2: Add native Tag Team modes

## `src/menu/menu_m_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +2 -2: Add native Tag Team modes

## `src/menu/menu_n_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls

## `src/menu/menu_n_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls

## `src/menu/menu_o_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls
- `7b99f8ad` 2026-10-15 dfdevx2, +5 -0: Dragon Rage 0.6.0 [skip ci]
- `094a365b` 2026-10-12 z3xox, +5 -0: Disc Fusion: both earlier discs count as recognised (there is no drive, and the game takes nothing from them)

## `src/menu/menu_o_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +16 -16: Fix disc resources, character previews and HUD controls

## `src/menu/menu_o_d.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls

## `src/menu/menu_r_d.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls

## `src/menu/menu_u_g.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +9 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +5 -5: Add native Tag Team modes

## `src/menu/menu_u_h.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +11 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +9 -9: Add native Tag Team modes

## `src/menu/menu_v_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +15 -15: Add native Tag Team modes

## `src/menu/menu_w_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls

## `src/menu/menu_x_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls

## `src/menu/menu_x_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +1 -1: Fix disc resources, character previews and HUD controls

## `src/menu/menu_z_b.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -4: Fix disc resources, character previews and HUD controls

## `src/menu/menu_za_c.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +7 -3: Fix disc resources, character previews and HUD controls

## `src/menu/menu_za_d.c` ([bt3-engine](bt3-engine.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +4 -0: Fix disc resources, character previews and HUD controls

## `src/menu/port_language.c` ([bt3-engine](bt3-engine.md))

- `de8da43b` 2026-10-18 victorgonl, +3 -4: Language selector artwork extraction from European disc
- `2d8a5a72` 2026-10-18 dfdevx2, +201 -0: Dragon Rage 0.7.3
- `6bf0ad56` 2026-10-16 victorgonl, +7 -0: Option to always show the language selector at startup like in Europe ROM
- `5b3f71f3` 2026-10-16 victorgonl, +194 -0: Add support for menu translations (documentation not updated)

## `src/menu/port_screen.c` ([bt3-engine](bt3-engine.md))

- `9bed1539` 2026-10-27 Regulus CiberSec, +6 -6: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `5d3eea11` 2026-10-27 Regulus CiberSec, +6 -6: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `101de958` 2026-10-24 Tag Team Mod contributors, +16 -0: Add native Tag Team modes
- `c49f5e0f` 2026-10-23 z3xox, +8 -0: The online menus can be translated: translations/port/<language>.json takes their lines (ids tbo/..., looked up by their English), where the menu draws and measures a line (tbo_text.c), joins pieces (Ps_Copy, which also feeds the screen reader) and formats a number (Tb_Fmt). A translation that loses a line's %d parts or {name} is not used, and a lone % in a format line is refused, so a file cannot make the game misread. The talk box decodes UTF-8 (accents were two wrong letters), and the frame count has its own plural line. port/tools/tbo_strings.py makes a file for translators with the English as placeholder (303 lines; --bt4 without BT3's fighter names) and keeps what is translated when run again. Accents checked on screen in Spanish
- `2d8a5a72` 2026-10-18 dfdevx2, +33 -7: Dragon Rage 0.7.3
- `5b3f71f3` 2026-10-16 victorgonl, +33 -7: Add support for menu translations (documentation not updated)
- `7b99f8ad` 2026-10-15 dfdevx2, +606 -0: Dragon Rage 0.6.0 [skip ci]
- `101712ea` 2026-10-15 z3xox, +568 -0: Port screens: the drawing kit (pictures, shapes, letters, buttons, prompts, the page) out of port_tbo.c into port_screen.c as Ps_*, unchanged
- `178b177d` 2026-10-15 z3xox, +38 -0: Port screens: a progress mode of their own (80, PortScreen_Main by gPortScreen); TBO through it, Duel_Main the game own again

## `src/menu/port_tbo.c` ([online](online.md))

- `96895d21` 2026-10-31 dfdevx2, +0 -4: Tag link: buffers out of .data; Tb_Vanilla: dead branch; change map; logscan
- `8e7dac04` 2026-10-28 Regulus CiberSec, +14 -2: BT3 Android: resoluções fracionárias inteiras, Dragon Net Online mais leve e configuração automática de controle sem fio
- `4dc3964c` 2026-10-27 Regulus CiberSec, +566 -153: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `9bed1539` 2026-10-27 Regulus CiberSec, +161 -82: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `c12ebe2a` 2026-10-27 Regulus CiberSec, +566 -153: Dragon Net Online: as telas novas no lugar do Dragon Net Battle, Classificatória, Raditz em vídeo e toque mais fluido
- `6b69eb72` 2026-10-28 z3xox, +193 -420: 0.1.39: lobbies replace rooms. Tenkaichi Budokai Online has no Create Room: the list is Quick Match, Ranked Match, Lobby, Join (Join Room renamed, the one place for a code), the SOON ones and Profile. Lobby opens New Lobby: Open Lobby (a code) or Host by Address (Port_LobbyCreate on the port net_port, L1 / R1 by a hundred); the lobby's page shows the port and this PC's LAN address where a code would be, square copies "address:port". A new lobby starts with the rules its owner last set on a lobby's page (Tb_LobbyKeepRules, in Create Room's settings: battle with DP 10 / 15 / 20, time, game speed, rollback, delay), so nothing that could be chosen is lost. No Watch on Join or Join by Address: a spectator joins the lobby; JOIN on Join by Address is Port_LobbyJoin and opens the lobby's page at once. Dragon Net Battle (F1 > Online > Advanced) opens and joins lobbies too (Host: a lobby with a code or on its port and the tab's rules; Join: a code or an address), no room of two and no Watch; F1 > Online loses "Let others watch your matches" (rooms only). PORT_RELEASE 139, NET_VERSION 6: a 0.1.38 game is never paired (the service's queues and codes are kept apart by it); by address a member joining a typed address that has not answered for two seconds also sends a spectator's greeting too short to be one, which a 0.1.38 host of two answers with SPEC_NO_VERSION ("The game at that address is a different version"), and a lobby's owner refusing another version notes it once a minute. FP_PEER_REQUIRED is 1: a peer, spectator or member without a fingerprint is refused. Update checks: v0.1.39 and v0.1.39.1 newer than 0.1.38.9 (44 checks).
- `5d3eea11` 2026-10-27 Regulus CiberSec, +161 -82: Tenkaichi Budokai Online no Android: imagens que apareciam brancas, fundo em vídeo, menu novo e idioma na tela
- `a3cf1192` 2026-10-28 z3xox, +633 -103: Tenkaichi Budokai Online does everything Dragon Net Battle did, and the window is put away. Join Room has a second row of buttons, BY ADDRESS and WATCH (WATCH under JOIN; the cursor still goes to JOIN when the code is full): WATCH watches the room's match (Port_LobbySpectateCode), with "The match has not begun yet" while the host has none, the host's refusal (full, nobody may watch, other data, another version, no answer) said in the hosts' box (Tb_Say; the lines in sTbNetLines for the translation file), no Train While Waiting; a lobby's code under WATCH joins the lobby, as JOIN does, and the hosts say so. Join by Address (TB_ADDR): the host's address and :port (7000 when none) on a keypad of digits, . : - and small letters (blank keys skipped), the PC keyboard and PASTE (the clipboard's last word, PortTbo_PasteText), then JOIN (Port_LobbyStart) or WATCH (Port_LobbySpectate); the address is kept (net_join_addr). Create Room: DP battles of 10 / 15 / 20 DP, a Game Speed row (net_speed_host, the lobby's steps) and Host by Address (port net_port, L1 / R1 by a hundred): the waiting page shows the port, this PC's address on its network (Port_NetLanAddress) and Copy Address. The screens ask the service "hello" when they open, as the window did. The main menu lists Dragon Net Battle only with the setting net_dnb_menu (BT3_MENU_ITEM4 for tests); F1 has a tab Online (Let others watch your matches, the desync and chat settings moved from Misc, and Advanced: Open Dragon Net Battle, List it in the main menu). Texts that sent players to the window name the screens now. Tested through the pages with a local Worker: a room watched by code (the spectator identical to the host over 10,507 blanks), host / join / watch by address (fight values 8,226, spectator 11,426), the lobby, WATCH with a lobby's code, a 4-entrant tournament, a quick match.
- `0ca19719` 2026-10-27 z3xox, +9 -2: Automatic update, on top of PR #107 by JosephLlacchua: the player chooses when, and an install that can be undone. Each build knows its full number: undefined.py writes PORT_VERSION into the generated port_version.h from BT3_VERSION, which the release workflow takes from the tag (v0.1.38.10 -> 0.1.38.10); a local build is "dev" and is never offered anything. The check compares that with the newest release's tag as dotted numbers (0.1.38.7 < 0.1.38.9 < 0.1.38.10 < 0.1.39), so patch releases are announced at last (the old check knew only PORT_RELEASE, 0.1.38, and told a 0.1.38.7 player "the newest is 0.1.38, this is 0.1.38"); PORT_SUBRELEASE goes. gs/net_update.c (the PR's file, rewritten): a few seconds after start one GET of releases/latest, its notes and its list of files read with a small JSON reader (this system's zip by name, its size and GitHub's SHA-256 digest); newer: a line over the main menu and the Tenkaichi Budokai Online screens ("Update 0.1.38.10 available. F1 to update.", through the port catalog: update/available, downloading, ready, done, online_off), and at the top of F1 Update now, Later (no line until the next start), What's new (the release's notes), Release page. Update now downloads into update/ in a thread (WinHTTP / the curl library / the curl program, with the PR's progress bar), checks the size and SHA-256, and the button becomes Restart now; offline play goes on, while no new online session starts (Port_OnlineBlocked: net.c's Port_LobbyStart and the screens' Tb_Vanilla refuse with the line; the Dragon Net Battle window says it). Restart now is never offered during an online session or a lobby. It installs with the PR's rename trick, generalised and made undoable, every path from the program's own folder (the PR's were relative to the working folder, wrong from a shortcut with another "start in"): every file the zip ships is first written beside its place as <name>.new, then each one in place is renamed to <name>.old and the .new put in its place (a running program and its loaded libraries can be renamed); a failure in either step puts every file back and the old version goes on, with the reason in F1. Never written, even if a zip had them: saves/, gamedata/ (mods in it), mods/, update/, bars/, net_session/, bt3_settings.txt, bt3_pipelines.txt, online_name.txt, bt3_log*/bt3_crash*, a11y_*_own.txt and the screen reader's logs, prism.dll / libprism.so, and in textures/, songs/, stages/ everything but the README.txt a release has there; fonts/fonts.tsv takes the new list with the player's own font lines added back; nothing is deleted. The zip must be the game's folder with this system's program in it, no path may leave the folder; Linux modes come from the zip, the programs stay executable. Then update/result.txt and update/cleanup.txt are written, the new version is started from the same folder and this one closes; it says "Updated to 0.1.38.10" and removes the .old files by that list. BT3_UPDATE_API (a local stand-in of GitHub), BT3_UPDATE_AUTO and BT3_UPDATE_FAIL_AT are for testing. The old update check in net_match.c is gone; F1 > Misc shows the version running, as the PR did.
- `277d09eb` 2026-10-28 z3xox, +48 -10: Tournaments: the owner chooses the winner when the two games disagree twice. A draw is still played again, but when the two players' games name different winners for the same match a second time there is no more replay: the owner's bracket page asks it to choose ("The two games disagree on this match: choose the winner", the two names as buttons), the members read that the owner is choosing, no match starts meanwhile, and the owner's choice is final (TOUR_CHOOSE; the match marked as chosen). All the lines are in port_tbo.c for the translators. BT3_TEST_RESULT_LIE makes a member report the other winner and BT3_LOBBY_DO has choose <side>. Tested with two entrants and a lying member: the first disagreement replayed, the second asked the owner, whose choice made the champion.
- `504064a3` 2026-10-27 z3xox, +32 -8: Tournaments: an entrant whose game drops gets back in. Leaving on purpose is still a walkover at once; an entrant whose game falls silent (a lost connection, a crash) keeps its bracket place for 90 s, shown on its box as DISCONNECTED and the time left, with a lobby note, and its next match waits. Joining with the lobby's code within the 90 s puts it back in its place, and the match the drop broke is played again from the start; after 90 s the walkover stands. It is known by a secret rejoin token, not its name: the owner draws 128 bits from the system's random source for each entrant and sends it to that member alone (TOKEN, a new packet type 41), which keeps it in memory and in lobby_rejoin.txt beside its settings file so that a crash or a restart keeps it; its greeting to the lobby of that code carries it. A rejoin gets a new token, so a copied old one is refused; a greeting without a valid token is an ordinary member, a spectator while the tournament is on. The owner forgets a token when its entrant is out, leaves, runs out of time, rejoins, or the tournament ends, and tells the member to forget it; the member also removes its file when the lobby closes, when it leaves, when its lobby does not answer, and at start when the file is over an hour old. The member now keeps the code it asked the service for, since the service's answer to a member carries none. STATE carries each held place's seconds left (616 bytes) and HELLO the token (80 bytes): LOBBY_VERSION 4. Tested offscreen: an entrant killed mid-match and back within 90 s (place restored, match replayed, spectators identical), one back too late, a stranger with its name, a copied old token, the token files removed, a stale file ignored.
- `b84d3e3b` 2026-10-27 z3xox, +52 -28: Tournaments: an Entrants rule decides who plays. In Tournament the battle's row (a tournament's matches are single battles anyway) is Entrants, 2 to 8, the owner's and fixed while a tournament is on, and the places on the left follow it: PLAYERS with the places 1..N, then SPECTATORS with the other 8 - N; Versus keeps 1P, 2P and six spectators. The entrants are whoever sits in the player places when the owner starts the tournament (at least two; empty places are left out, with byes as before), placed with Move; with fewer entrants, those in the places that are no longer the players' are spectators there. Ready is no longer shown or asked for in Tournament, since the places say who plays. The rule travels in STATE (byte 3 of word 96). The user found it unclear who was spectating and who was taking part. Tested: the lobby page with 2, 4 (after 5 and back) and 8 player places, the brackets drawn from them, a two-entrant tournament to its champion.
- `3ea60f57` 2026-10-27 z3xox, +572 -51: Tournaments in lobbies. The lobby's owner switches the lobby between Versus and Tournament (a new first rule) and draws a knockout bracket of the places marked ready (2 to 8 humans, the owner too if it is ready): the order is shuffled from a number the owner draws from the system's random source, with byes shared out between the halves when the entrants are not 2, 4 or 8. The owner's game holds the bracket and sends it with the lobby (STATE, now 608 bytes; LOBBY_VERSION 3, so a 0.1.38.9 game is told the versions differ, both ways). Each match is an ordinary lobby match of single battles: the owner's game puts the two in 1P and 2P and starts it, everyone else watches. Every copy in the match notes how its first battle ended (the battle sequence past the fight, held 0.3 s so that a rolled-back K.O. is not taken; Port_BattleOutcome in headless.c), a player says it with each ALIVE, and the owner, when the two agree on a winner, marks it and ends the match; a draw, two different results, one result alone for 20 s or a match ended any other way is played again. An entrant who leaves loses by walkover. A new page, the bracket (TB_BRACKET in port_tbo.c), shows the two halves meeting at the final with gold lines for winners, the next match glowing, byes greyed and the champion's box, and the owner's Start Next Match / New Tournament; it comes up when a tournament is drawn and after each of its matches, and the lobby's page has a Bracket button. All text is in port_tbo.c for the translators. Tested offscreen through a local Worker: 4 and 5 entrants (byes) to a champion with every spectator identical to its host in every match, a walkover, an old release joining either way; Versus lobbies, quick match and rooms of two unchanged (docs/port/README.md, "Tournaments").
- `79fb9436` 2026-10-27 z3xox, +23 -19: Online: whose data may meet. A quick match (strangers) needs the original disc's data, in every menu: Port_LobbyStartQuick refuses it before the matchmaking service is asked ("Quick match needs the original disc's data, and your game data are not the disc's (what differs). With friends ... you can play when both have the same data."), Dragon Net Battle's Quick match tab says so before the search and greys its button, and Tenkaichi Budokai Online's "Cannot Go Online" is now "No Quick Match", for Quick Match only. With friends (a room code, a lobby, an address, watching) the same data on both sides is enough, modified or not: Tenkaichi Budokai Online's Create Room, Join Room and Lobby no longer refuse modified data. No game connects before its own gameplay fingerprint is known: a start asked for earlier waits (lobby state 6, "Checking your game data..."; a lobby's create and join commands stay queued; net_start holds the picture), and a run that computes none (BT3_GS=none, or online play asked before the data check began) starts it (Port_DataFingerprintNeed; PortFp_Run runs once). A peer that sends no fingerprint: 0.1.37 cannot be told from 0.1.37.1 to 0.1.38.9 by its greetings (NET_VERSION 5, the same capability word; rel 137 at the service), so FP_PEER_REQUIRED decides: 0 for now (MIN_RELEASE is 137), 1 with MIN_RELEASE 138, which refuses such a player, spectator or lobby member. BT3_TEST_FPRINT now acts in plat_fprint.c; BT3_TEST_FPRINT=off sends none (as 0.1.37); BT3_TEST_FP_REQUIRED=1; BT3_UI_NET_TAB opens the online window on a tab. Tested with the local Worker: quick match original against original (BT3_LOBBY and Tenkaichi Budokai Online's screens), a claimed fingerprint refused before the service (both menus show it); by address and room code the same fake fingerprint plays and different ones are refused, after waiting for the fingerprint at the first blank; spectators and lobby members with other data refused, with the same let in; with FP_PEER_REQUIRED a player and a member without a fingerprint refused; 0.1.38.9 by address both ways. Windows builds.
- `6a8d1429` 2026-10-26 z3xox, +13 -8: Quick match: a chosen region is a preference that widens, no longer a lock. A pair is made when each side takes the other, the wait being the longer of the two: Auto takes players within 2,500 km at once and anyone from 25 s; a region takes players placed in it at once and anyone from 40 s (REGION_WIDEN_SECONDS); Anywhere and older games take anyone. So for the first 25 s Auto is nearby only and a region its players only, until 40 s Auto takes everyone but those who chose another region, and after 40 s everyone queuing meets, the nearest first. A region's search is told widen 40; its screens say "Players there first, everyone after 40 s" and, past it, "NO ONE THERE: SEARCHING EVERYWHERE", and the hosts' line says the same. No change to the database. Tested: test.mjs 185 checks (Auto in the Middle East against South America chosen: not at 30 s either way, matched at 40); through the screens with the local Worker, Dubai on Auto and Sao Paulo on South America put together at 40 s.
- `16e55137` 2026-10-26 z3xox, +11 -1: Tenkaichi Budokai Online: the found match's page is shown before the match starts. The session was asked for in the same frame the rival was found, and it holds the last picture shown while it starts, so that picture was always the search's ("SEARCHING..."), never "YOUR RIVAL IS HERE!" (in 0.1.38.8 as well, and in every release since the screens came). The session is now asked for four frames later. Tested: two copies by quick match through the screens, the held picture is the found page with the rival's region, and both reach the character select together.
- `cbde53ab` 2026-10-26 z3xox, +81 -18: Quick match prefers nearby players. The matchmaking Worker places each search by Cloudflare's request.cf and keeps, for a quick-match room's life only, its host's position rounded to whole degrees, its region and its choice (column loc), and the joiner's region (join_region); nothing of it is logged, and the other player is told the region only. A new game (it sends `waited` and `area`) chooses on Quick Match's new Region row: Auto (the default) takes a room within 2,500 km at once and any room, the nearest first, once it or the room has waited 25 seconds; a region (North America, South America, Europe, Middle East, Africa, Asia, Oceania; the Middle East by country, Egypt and Turkey with it) matches only players placed there who chose Auto or that region, never widened; Anywhere takes whoever waits longest. A waiting new game now searches again from its room every ten seconds (not counted against the searches an hour), so two far waiters meet; the room is held meanwhile, and one that was just joined is answered as joined, never lost. The start-up line says where the rival is and, once connected, the measured round trip; the search screen says when Auto looks everywhere, or that a region's wait can be longer. Games of 0.1.38.8 and older are answered as before (they pass a room of one region by), and their rooms are taken by new games as of unknown place. The D1 database needs two columns (port/matchmaking/README.md); without them rooms are matched after the widening. Tested: test.mjs 180 checks (56 new); a local Worker over SQLite with faked places: Dubai and Riyadh matched at once, Dubai and Frankfurt at 30 s with "NO ONE NEARBY", a Europe-only search left alone, 0.1.38.8 against this build both ways.
- `bc8e44f8` 2026-10-26 z3xox, +15 -5: Lobbies: the hosts who leave as the lobby's page comes in go behind the box at the foot of the screen, as on every page, and in 16 frames (a page's coming in); READY sits inside its bar's round end.
- `22aecbaf` 2026-10-26 z3xox, +61 -6: Lobbies: the page's rules have their names inside their bars as the places' names are; going into the page the last page's hosts walk off to the right in front of it while its windows come in from the left, as the other pages come in (from the list, from Join Room's search, and once a lobby is reached); the windows over the page (leaving, the owner's choices over a place) slide in from the left and out again.
- `1641f1f7` 2026-10-26 z3xox, +10 -9: Lobbies: the yes / no window before leaving or closing the lobby, and the window while it is reached or after it closed, have their title in the heading style (Ps_Heading 0.9, as "Cannot Go Online") and the line under it smaller (Ps_Msg 0.88); the title was in small capitals under a larger line.
- `6f99aff2` 2026-10-26 z3xox, +9 -8: Lobbies, from the user's first test: the page loses its Close / Leave button (triangle leaves, with its yes / no; the owner's Start Match and Ready with Chat across the row under them, a member's Ready and Chat in one row). Going back from a lobby match's character select is the owner's way back to the lobby for everyone, as Back to the Lobby in a fight (src/menu/menu_c_e.c, CharSel_PortLobbyHold: the other player's triangle does nothing there; src/menu/menu_g_b.c: the session ends); before, the select opened again. The players' pause menu and the menu after a battle read "Back to the Lobby" for the owner's way back (Return to Vs Menu's item with the port's text, number 0x6FFE in BtlMenu_GetItemText, translated). A game that ends in a lobby (window closed, game quit: atexit) closes it for everyone if it is the owner's and leaves it otherwise, three times on the socket it has, in a match too: the others know at once instead of after the silence. The player's name is the setting net_name (as Dragon Net Battle writes it; Profile now writes it too) before online_name.txt, which two copies with their own settings files shared. BT3_LOBBY_DO quit (the program ends). Tested by code, owner + 2: the new texts, the character select's back for both players, the owner quitting in the lobby, at the character select and mid-fight, a player quitting mid-fight; fight values identical. All game-side changes under PORT: the PS2 build is unchanged.
- `a581c3f7` 2026-10-26 z3xox, +10 -297: A spectator's own pause menu is the battle's own pause menu: its box from the battle's sprite sheet, drawn by its engine (BtlMenu_Draw, src/battle/hud_0_b.c, through BtlMenu_PortDrawMenu), its colours, sounds and six-frame growing box, and the versus pause menu's own "EXIT ?" Yes / No box before leaving (PauseMenu_PortConfirmBox, src/battle/hud_0.c), instead of a Tenkaichi Budokai Online panel. Its items (port/src/spec_menu.c): the note "Your screen only: the fight goes on" in the title's row, Continue, View: Player 1 / Player 2 / Split Screen with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching; their texts reach the engine through BtlMenu_GetItemText (numbers 0x6FFF and on) and are translated by the online menus' file (tbo_strings.py lists the file). The engine's update is not run and the battle's menus are not touched: the file moves the cursor and the box itself and reads the PC's pad (Port_PadRead); in a fight only (gPortBtlMenuLive). Tested by code with an owner and a member watching and every item used with the pad: the fight values of both spectators and both players identical, no check value differed. All game-side changes under PORT: the PS2 build is unchanged.
- `18d6fd06` 2026-10-26 z3xox, +1421 -16: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `c49f5e0f` 2026-10-23 z3xox, +37 -13: The online menus can be translated: translations/port/<language>.json takes their lines (ids tbo/..., looked up by their English), where the menu draws and measures a line (tbo_text.c), joins pieces (Ps_Copy, which also feeds the screen reader) and formats a number (Tb_Fmt). A translation that loses a line's %d parts or {name} is not used, and a lone % in a format line is refused, so a file cannot make the game misread. The talk box decodes UTF-8 (accents were two wrong letters), and the frame count has its own plural line. port/tools/tbo_strings.py makes a file for translators with the English as placeholder (303 lines; --bt4 without BT3's fighter names) and keeps what is translated when run again. Accents checked on screen in Spanish
- `7b99f8ad` 2026-10-15 dfdevx2, +2064 -0: Dragon Rage 0.6.0 [skip ci]
- ... e mais 34 (`git log -- src/menu/port_tbo.c`)

## `src/menu/tag_lobby_menu.c` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +12 -10: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `0773df3a` 2026-10-26 Tag Team Mod contributors, +3 -0: Fix disc resources, character previews and HUD controls
- `101de958` 2026-10-24 Tag Team Mod contributors, +64 -0: Add native Tag Team modes

## `src/menu/tag_online_menu.c` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +28 -13: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `02d1e354` 2026-10-26 Tag Team Mod contributors, +1 -1: Simplify the native online menu
- `101de958` 2026-10-24 Tag Team Mod contributors, +598 -0: Add native Tag Team modes

## `src/menu/tag_session.c` ([tag-team](tag-team.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +2 -2: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +152 -0: Add native Tag Team modes

## `src/menu/tag_setup.c` ([tag-team](tag-team.md))

- `d54095dd` 2026-10-29 Regulus CiberSec, +85 -71: Tag Team: menus no estilo Dragon Ball, toque, online pela internet e correção do fechamento nas lutas
- `3f9ad0fc` 2026-10-28 Regulus CiberSec, +25 -11: Tag Team: polimento do menu do SELECT com as peças do Dragon Net Online
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +11 -6: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +204 -0: Add native Tag Team modes

## `src/menu/tag_tournament.c` ([tag-team](tag-team.md))

- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +3 -1: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +195 -0: Add native Tag Team modes

## `src/port/vu0_a.c` ([bt3-engine](bt3-engine.md))

- `5a2a000f` 2026-10-24 z3xox, +6 -0: A model's seam is drawn at the positions its routine works out, not rounded to the GS's 1/16 pixel. The seam (the strip joining two parts of a model: the back of Videl's shirt) is projected on the CPU and sent as plain GS triangles (XYZF2), while the parts beside it come from the vertex programs at full precision: their edges did not meet, and the black outline behind showed through, a dashed line down the seam (from a player's screenshot; at 4x about 15 marks, now 3 single pixels where the meshes do not share a corner). The seam routine leaves each vertex's exact position under its rounded one (its floats as bits: that file uses the game's software float); a GS vertex with that position and depth is drawn at it. BT3_SEAM_ROUND=1: as before. Unchanged: every screenshot of the recorded fights, uncapped speed (243 / 336 fps), input to picture 33.36 ms, pacing
- `08321034` 2026-10-06 z3xox, +321 -0: ObjSeam_TransformVtx written (instruction-for-instruction, with a check against the original instruction words): characters with seam models no longer stop the game
- `41760bb6` 2026-10-06 z3xox, +10 -0: Speed: vertex unpack as a copy, table-driven texture uploads, faster page hash, float core inlined into the vector library (fight frame work more than halved)
- `697d9e08` 2026-10-06 z3xox, +0 -2111: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/port/vu0_b.c` ([bt3-engine](bt3-engine.md))

- `bb212093` 2026-10-07 z3xox, +9 -0: Add and multiply by the processor where that is exact (plat_fastvec.c): single operations and the matrix-times-vector kernel, checked bit for bit against the soft-float code; a re-run frame 30% cheaper
- `41760bb6` 2026-10-06 z3xox, +12 -0: Speed: vertex unpack as a copy, table-driven texture uploads, faster page hash, float core inlined into the vector library (fight frame work more than halved)
- `697d9e08` 2026-10-06 z3xox, +0 -1816: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/adx.c` ([audio](audio.md))

- `0773df3a` 2026-10-26 Tag Team Mod contributors, +6 -0: Fix disc resources, character previews and HUD controls
- `4d7664c0` 2026-10-26 Tag Team Mod contributors, +6 -0: Add Linux setup and improve native menus and music
- `101de958` 2026-10-24 Tag Team Mod contributors, +37 -31: Add native Tag Team modes

## `src/sys/dialog.c` ([bt3-engine](bt3-engine.md))

- `2d8a5a72` 2026-10-18 dfdevx2, +9 -3: Dragon Rage 0.7.3
- `316b567c` 2026-10-16 victorgonl, +9 -3: Add support for dialogue and ui texts translations (no documentation yet)

## `src/sys/dma.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +22 -3: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -9: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/file.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +60 -3: Add native Tag Team modes
- `5707f85b` 2026-10-22 dfdevx2, +31 -0: Fixes from logs, touch pause menu, BT4 menu
- `697d9e08` 2026-10-06 z3xox, +0 -2: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/gfx.c` ([bt3-engine](bt3-engine.md))

- `18d6fd06` 2026-10-26 z3xox, +3 -0: Lobbies in the game's own menus: the lobby is a page of Tenkaichi Budokai Online (src/menu/port_tbo.c, TB_LOBBY) instead of an ImGui window, drawn with the same kit as the other pages (the grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font) and worked with the pad alone. Left the eight places (the owner's mark, YOU, who is ready); right the code (square copies it), the rules with left / right, and Start Match, Ready, Chat, Leave / Close Lobby (a yes / no window; triangle too). Cross on a place gives the owner Move (then the place: the two swap), Remove from the Lobby, Back; L1 / R1 switch sides. The box at the foot of the screen has the last three things that happened, notes and chat (net_lobby.c keeps them in order for the view: logCount, logTotal). Chat is the name's keypad (and the PC keyboard), sent with Done or START. The hub's new Lobby makes one asking nothing (Create Room's rules); Join Room with a lobby's code opens the page; the screens and the main menu go to the page whenever a lobby is on (Port_LobbyWantsScreen, src/menu/menu_a.c), so a match asked for there comes back there. Dragon Net Battle loses its Lobby tab and says where the lobby's screen is when its Join tab finds one (port text net/lobby_joined). A spectator's line over the match and its own pause menu (START or Esc: Continue, View with left / right, Back to the Lobby for the owner, Leave / Close the Lobby or Stop Watching with a yes / no) are the game's too, drawn over every screen of the match from Gfx_EndFrame (src/sys/gfx.c, PortTbo_SpecDraw): the menu reads the PC's pad itself (Port_PadRead) and keeps its variables out of the game's state (section .porthst); its drawing goes through the game's draw lists, so a spectator showing the line differs from its host in those bytes only (the fight's values and check values are the same). BT3_SPEC_LINE=0 leaves the line out (tests compare whole states); BT3_TBO=1 opens Tenkaichi Budokai Online instead of the title; BT3_SPEC_PAUSE_AT opens the game's menu. Every new line is in port_tbo.c for port/tools/tbo_strings.py; the lobby's English notes are translated where shown (whole lines, "%s joined the lobby" and the like by their end, "Match %d: %s vs %s"). All game-side changes are under PORT: the PS2 build is unchanged.
- `7b99f8ad` 2026-10-15 dfdevx2, +5 -0: Dragon Rage 0.6.0 [skip ci]
- `f2b701ac` 2026-10-15 z3xox, +2 -2: Exit question: its animation at the menus speed in a fight too (a fight frame lasts two blanks: the window played at half speed)
- `65086467` 2026-10-14 victorgonl, +5 -0: Quit game dialog using the game’s UI

## `src/sys/gfxm_a.c` ([bt3-engine](bt3-engine.md))

- `2d6d0531` 2026-10-25 dfdevx2, +2 -2: Lite mode: dr_lite setting, SDL descriptor sets kept
- `dd4581d7` 2026-10-25 dfdevx2, +6 -0: Lite: measurements on a simulated Mali-G52 MC1 (WIP)
- `101de958` 2026-10-24 Tag Team Mod contributors, +60 -8: Add native Tag Team modes
- `968794bf` 2026-10-21 z3xox, +11 -0: Glow: its pages at the PS2's resolution only while the glow chain runs (GfxPost_DrawGlow marks its start and end in the display list: markers 0x20 and 0x21); the fighters' shadow picture, the lens flare's, the heat haze's and the stage blur's use of the same pages at the internal resolution again; F1 > Effects > Sharp stage blur (on by default): those passes between work buffers sampled at the internal resolution as before, off as the PS2 samples them
- `928b2413` 2026-10-11 dfdevx2, +14 -0: WIP: patch 1 and touch artwork
- `4b8e04b1` 2026-10-10 z3xox, +14 -0: The Effects tab: "Movement blur" switches the game's blur of the scenery while the view turns
- `40a2fa36` 2026-10-06 z3xox, +11 -0: Port: game code builds with clang (BT3_CC=clang), replay and heap match the gcc build
- `697d9e08` 2026-10-06 z3xox, +0 -8: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/gfxm_b.c` ([bt3-engine](bt3-engine.md))

- `8cbd9d5a` 2026-10-31 dfdevx2, +9 -7: Tag Team off by default; its drawing changes only in its battles; labels read the archive; test CPUs
- `101de958` 2026-10-24 Tag Team Mod contributors, +44 -18: Add native Tag Team modes
- `697d9e08` 2026-10-06 z3xox, +0 -8: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/gfxm_c.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -0: Add native Tag Team modes

## `src/sys/gfxm_d_b.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +8 -4: Add native Tag Team modes

## `src/sys/gfxm_e_b.c` ([bt3-engine](bt3-engine.md))

- `ac51a1ad` 2026-10-31 dfdevx2, +8 -0: Game files: say why a read failed, read again through a new file object; keep the old model
- `101de958` 2026-10-24 Tag Team Mod contributors, +1 -1: Add native Tag Team modes
- `5707f85b` 2026-10-22 dfdevx2, +9 -0: Fixes from logs, touch pause menu, BT4 menu

## `src/sys/heap.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +7 -1: Add native Tag Team modes
- `755986ea` 2026-10-07 z3xox, +12 -1: Added stages: file ids that are no disc file's (0.1.8 gave a disc stage's model to an added stage's sound bank); stages larger than the game's buffer

## `src/sys/loading.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +11 -0: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +36 -4: Dragon Rage 0.6.0 [skip ci]
- `40f5f520` 2026-10-12 z3xox, +2 -2: Menus in 4:3: an option to show the menus at their own shape in a wider picture instead of stretched (fights unchanged)
- `6a3d188f` 2026-10-11 victorgonl, +36 -4: Anchor loading animations to screen edges beyond 4:3

## `src/sys/mathf.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -6: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/mcflow_a.c` ([bt3-engine](bt3-engine.md))

- `04339c17` 2026-10-20 z3xox, +36 -12: Quick saving: a new game's first save on a card with no save goes through without the window (the question to create the save file answered yes, the card icon in the corner); after a save was loaded, its questions are shown as the game has them (the save on the card would be lost) and the pages after yes go without the waits
- `0d7c4662` 2026-10-20 z3xox, +56 -16: Quick saving: Load in the options asks its question as the game does, then the checking and loading pages go as soon as the card answers and "Load successful" stays half a second (about 85 frames after yes against 400); an error page is the game's own
- `3274502b` 2026-10-20 z3xox, +124 -0: Quick saving: a save goes through without the memory card window (the game keeps every page on screen two seconds and the PC card answers at once), a card icon in the corner says it is saving and saved; the start-up load reads the save with no pages and goes straight to the logos; an error, no card or a full card still show the game's window; off online (both games run the start-up frame for frame), F1 > Misc to turn it off

## `src/sys/movie.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +19 -0: Add native Tag Team modes
- `3f922112` 2026-10-18 victorgonl, +11 -0: Original opening music extraction using European disc
- `4c200231` 2026-10-12 z3xox, +1 -1: Skip logos / skip intro (PR #50) in the port's style: the checks inside PORT, the two boxes on the UI tab, no new tab or header
- `738157dd` 2026-10-09 Renzo, +1 -1: .
- `f1619b6b` 2026-10-06 z3xox, +8 -0: Windows: bt3.exe (BT3_CC=win64, cross-built with mingw-w64), setup wizard and release zip; full-width pointers for parameters and results; address masks removed
- `697d9e08` 2026-10-06 z3xox, +0 -9: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/pad.c` ([touch-input](touch-input.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +28 -16: Add native Tag Team modes

## `src/sys/randf.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +0 -4: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/snd.c` ([audio](audio.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +33 -2: Add native Tag Team modes
- `7b99f8ad` 2026-10-15 dfdevx2, +11 -0: Dragon Rage 0.6.0 [skip ci]
- `d60fcab8` 2026-10-13 z3xox, +11 -0: Online: the pause menu no longer stops the fight

## `src/sys/spu_heap.c` ([bt3-engine](bt3-engine.md))

- `101de958` 2026-10-24 Tag Team Mod contributors, +4 -1: Add native Tag Team modes

## `src/sys/vu0_a_c.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +1 -1: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/vu0_a_c_b.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +1 -1: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/vu0_a_c_c.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +1 -1: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/vu0_b_c.c` ([bt3-engine](bt3-engine.md))

- `697d9e08` 2026-10-06 z3xox, +68 -68: Decomp holds PS2 code only: PC-port guards and the PC reference vector library removed (they live in bt3-port)

## `src/sys/vu1_packet.c` ([bt3-engine](bt3-engine.md))

- `22c7ee0d` 2026-10-06 z3xox, +6 -0: Port: program 8 upload length (stages with animated objects), hashed texture cache, target reuse
