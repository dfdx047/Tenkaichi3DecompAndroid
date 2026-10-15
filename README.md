<p align="center">
  <img src="android/art/banner.png" alt="Dragon Rage" width="100%">
</p>

# Tenkaichi3Decomp

[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white)](https://discord.gg/RsPFr4pkaN)

A native PC port of **Dragon Ball Z: Budokai Tenkaichi 3** (PlayStation 2, USA release, SLUS-21678), built on
the matching decompilation in **BT3-Decompiled**. It is the game's own code compiled for PC, with a new
renderer, sound and input layer underneath; it is not an emulator.

This repository contains **no game data**. You need your own disc image of that release: the setup reads the
game's data from it, and it stays on your computer.

> **Official source.** This project lives only at
> [github.com/z3xox/Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp), and it never ships the game:
> you need your own disc. We do not make, run or endorse any website that lets you play Budokai Tenkaichi 3 in a
> browser, with or without ads; such sites are not affiliated with this project. A site that runs the game
> without your own disc is handing out the game's copyrighted code and data. Our releases are free and have no
> ads, accounts or payments.

## What works

- The whole game loop: boot, opening movie, menus, saving, fights, split screen.
- A GPU renderer (Vulkan) with the PS2's effects (outline, glow, depth tint, distance blur), each switchable.
  An OpenGL 3.3 renderer draws the same picture for machines without a working Vulkan (F1, Video, Renderer; it
  is also tried by itself when Vulkan cannot start). It is slower than Vulkan.
- Internal resolution from 1x to 8x; 4:3, 16:9, 21:9 and wider without stretching the fight.
- Smooth motion (experimental, off by default; F1, Video, or F9): the game computes a fight 30 times a second, as
  on the console; with this the renderer draws pictures in between, for 60, 120 or 240 a second. The fight itself,
  replays and online play are unchanged, and the two players of an online match need not agree on it. The newest
  picture reaches the screen 17 to 29 ms later than without. Known limits: pictures are left out at camera cuts
  and where the camera would pass through scenery; 240 is uneven; little tested with OpenGL. Works in both halves
  of a split screen.
- Music, voices and sound effects; keyboard and controllers, fully rebindable; two players.
- Clashes: Square counts as a direction (off by default; F1, Controls): in a clash of two beams or two rushes the
  game counts newly pressed directions, which is why the stick is turned; with this each press of Square counts as
  one, as a tap of the d-pad does. It changes only your own pad, so it can be used online. A press can be made to
  count for 1, 1.5, 1.75 or 2 (1.75 unless set: the d-pad has four buttons to tap in turn, Square is one).
- Texture packs made for PCSX2 (`.dds` or `.png`): copied into the `textures` folder next to the game, they are used as they are.
- Stages and music from outside the disc: a stage model (`.unk`) dropped into the `stages` folder next to the
  game, or a song (`.mp3`, converted in the background when the game starts, or `.adx`) into `songs`, is added to the stage select, its name written in the game's own
  lettering (cut from the disc's name pictures while the game runs). They are not written to the save and are
  not offered in an online match. See [docs/port/adding_stages.md](docs/port/adding_stages.md) and
  [adding_songs.md](docs/port/adding_songs.md).
- In the game, **F1** opens the settings, or **START + SELECT** on a controller (which also works the window: d-pad and
  Cross, Circle to leave, L1 / R1 for the tabs). Controls has a row to choose another key or button. **Esc** asks
  before it closes the game, and the window has an "Exit game" button. Its Cheats tab has "Unlock everything" (a debug function the game's
  developers left in: all characters, stages, music, items and Zenni).

- **Online play, experimental** (from release 0.1.8): the main menu's hidden "Dragon Net Battle"
  entry is back and opens a small window. One player hosts and gets a six-character room code, the other joins
  with the code: no port forwarding (from 0.1.16; hosting on a port and joining by address is still there);
  both get the same roster with everything unlocked, each sees their own fighter's view full screen, and leaving
  the match puts each player back where they were with their own save. Linux and Windows play against each
  other. With rollback (the host chooses how many frames, and the input delay) your own moves come out at once
  and the game corrects itself when the other player's buttons arrive.
  The host also chooses the battle (single, team, or DP battle with a limit of 10, 15 or 20) and the time limit.
  Each player sees their own gauges on the left with the players' names under them, and hears the voice language
  chosen in their own save.
  The two players can write to each other: Tab opens a line, Enter sends it, Esc or Tab puts it away (the keyboard
  does not play while a line is typed; a controller does; the key can be changed under Controls). Lines show in the
  lower left corner for a few seconds. F1, Misc turns the chat off; the line also has "Mute rival". It is not
  part of the game's state and a copy without it simply sees nothing.

Online play is new. It has been played over the internet between a Linux and a Windows machine. The input delay
is chosen from the connection when a match starts (or set by the host), both players need the same release (the
game says so when they differ; 0.1.10 does not play against 0.1.8 / 0.1.9, and 0.1.15 does not play against
0.1.14 and earlier). F1, Video has a meter for the frame rate and, in a match, the ping, rollbacks and waits. The 32-bit
Linux build has no rollback. [docs/netplay_notes.md](docs/netplay_notes.md) is the working log of how it is built
and what was checked.

Room codes go through a small matchmaking service (a Cloudflare Worker; its whole source is in
[port/matchmaking](port/matchmaking), and `BT3_MATCH_URL` points the game at another copy of it). The game tells it
the name typed in the window and the addresses it can be reached at; the service keeps them with the room for at
most ten minutes and hands them to the player who enters the code. The two games then send to each other
directly. Where the routers do not allow that, the match goes through a relay (Cloudflare's TURN service), which
passes the match's packets (the buttons pressed) on and keeps nothing. Without a room code none of this is used.

See [docs/port/README.md](docs/port/README.md) for the running log of what is done, what is verified and what is not.

## Requirements

- A 64-bit x86 processor and a graphics card with Vulkan support. Most of the work is on the processor (the
  game's arithmetic is reproduced exactly as on the PS2); a recent mid-range CPU is recommended.
- Linux (glibc 2.35 or newer) or Windows 10 / 11.
- macOS on Apple Silicon: **experimental**, built from source only (see below).
- About 4 GB of disc space, and your disc image as an `.iso` file.

## Installing a release

Unpack the release, start `Tenkaichi3Decomp-setup` (`Tenkaichi3Decomp-setup.exe` on Windows), choose your disc image, press Play. The
setup checks that the image is the unmodified USA release and unpacks the game's data next to the program.

On a machine without a working Vulkan the setup window and the game use OpenGL by themselves. To ask for OpenGL
from the start (a Vulkan driver that starts but is incomplete, as on Intel Ivy Bridge), set `BT3_GPU_API=gl`:
`BT3_GPU_API=gl ./Tenkaichi3Decomp-setup`, and the same for `./Tenkaichi3Decomp`. The setup also runs without a
window: `./Tenkaichi3Decomp-setup --install <disc image>`.

## Building from source

<p align="center">
  <b>Dragon Ball Z: Budokai Tenkaichi 3, running natively on Android.</b><br>
  An Android port of <a href="https://github.com/z3xox/Tenkaichi3Decomp">Tenkaichi3Decomp</a>, the native PC port built on the BT3 decompilation.
</p>

<p align="center">
  <a href="../../releases"><img alt="Release" src="https://img.shields.io/github/v/release/dfdx047/Tenkaichi3DecompAndroid?label=release&color=ff8a1f"></a>
  <a href="../../actions/workflows/android.yml"><img alt="Android build" src="https://github.com/dfdx047/Tenkaichi3DecompAndroid/actions/workflows/android.yml/badge.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Android%2010%2B%20·%20arm64-1e4fa8">
</p>

---

> [!IMPORTANT]
> **Status: the whole game runs natively on Android arm64**: menus and full battles at the game's full 30 fps on
> Snapdragon (AYN Odin 3, Odin 2 Mini, Snapdragon 8s Gen 4) and on recent Mali GPUs (Dimensity 8400, Unisoc T820).
> Online play with room codes, smooth motion (60/120), HD texture packs (`.dds` and `.png`), Vulkan or OpenGL ES
> (automatic for Mali), an on-screen controller made for this game, screen filters (FXAA, AMD FSR 1, Snapdragon GSR)
> and custom Adreno drivers are in. Next: weaker GPUs, quality-of-life features. See [Roadmap](#roadmap).

This repository contains **no game data**. You need your own copy of the **USA release (SLUS-21678)** as an
`.iso`. The app reads it on your device; nothing is downloaded or uploaded.

## What it is

The PC port is not an emulator: it is the game's own code, decompiled and compiled for PC, with a new renderer,
sound and input layer underneath. Dragon Rage brings that to Android in two parts:

| Part | Where | State |
|---|---|---|
| **Launcher app** (Kotlin, Jetpack Compose, Material 3; English and Portuguese) | [`android/`](android) | ✅ Done |
| **Game engine** (the port's C code, built for arm64 as `libbt3.so`) | `src/`, `include/`, `port/` | ✅ Runs on Snapdragon and Mali phones and handhelds; weaker GPUs being tuned |

## The app

<table>
<tr><td><b>Home</b></td><td>Pick your ISO. The app checks it is the USA release (the same SHA-1 checks as the PC setup; a translated or modded image installs too, with a warning) and copies the game data in the layout the engine expects. Play button, and a <b>Play shortcut</b> for the home screen that starts the game directly.</td></tr>
<tr><td><b>Textures</b></td><td>Import a texture pack as a <code>.zip</code>, or download one from inside the app (a list kept in <a href="android/texture-packs.json"><code>android/texture-packs.json</code></a>, or any direct link to a <code>.zip</code>), or copy its folder into <code>textures/</code>. Packs made for PCSX2 (<code>.dds</code> / <code>.png</code>) work as they are: <code>.dds</code> files are unpacked for phones' GPUs, which cannot read DXT. Turn each pack on or off, or all of them at once.</td></tr>
<tr><td><b>Mods</b></td><td>File-replacement mods as <code>.zip</code> (with a priority order), extra stages (<code>.unk</code>) and extra songs (<code>.adx</code>), with the name shown in the game's menus editable.</td></tr>
<tr><td><b>Cheats</b></td><td><i>Unlock everything</i>: every character, stage, song and item, and max Zeni. This is the debug function the developers left in the game. It is applied the next time the game starts.</td></tr>
<tr><td><b>Controls</b></td><td>The on-screen controller for playing without a gamepad: a <b>layout made for this game</b> (its own actions: target, Ki, melee, guard, dash, blasts, ultimate, team swap) with a menu mode that switches <b>by itself</b> between menus and fights, or the <b>classic</b> console pad; PlayStation, Xbox or Nintendo look, layout editor (move, resize, hide), macros (combos, several buttons at once, turbo), opacity, size, vibration, floating stick. <b>One button in clashes</b> (mash Square instead of turning the stick). Also reachable in game.</td></tr>
<tr><td><b>Settings</b></td><td>Renderer (<b>Vulkan</b> or <b>OpenGL ES</b>; automatic: OpenGL ES on Mali), internal resolution 1x–8x in fine steps (1.25x, 1.5x, 1.75x … for weaker GPUs), screen filter (bilinear, sharp bilinear, FXAA, AMD FSR 1, Snapdragon GSR), picture shape (the screen's own, or 4:3 to 32:9), <b>smooth motion</b> (60 or 120 pictures a second), present mode (FIFO, Mailbox, Immediate), each of the five PS2 effects (outline, see-through tint, depth tint, glow, distance blur), glow strength, music / voice volume, custom GPU driver for Adreno (Turnip and others, through AdrenoTools), <i>Save log to Downloads</i> for bug reports, theme, Material You colours.</td></tr>
<tr><td><b>In game</b></td><td>The game's own menu (back button, START + SELECT, or the on-screen MENU button): every setting above, <b>Dragon Net Battle</b> (online play with a room code), and <b>Quit to launcher</b>.</td></tr>
</table>

Dark theme with Dragon Ball colours (Goku's gi orange and blue, dragon-ball amber, Super Saiyan gold) by default,
or your wallpaper's colours with Material You. On wide screens (handhelds like the AYN Odin) the navigation moves
to a side rail.

### Install

Download `DragonRage-<version>.apk` from the [latest release](../../releases/latest) and install it. A newer
version installs over the older one and keeps your game data, settings and saves.

- **Releases** (`android-vX.Y.Z` tags): tested versions.
- **[Nightly](../../releases/tag/android-nightly)**: the latest build of `main`, for testing; may be broken.

The app checks GitHub when it opens and offers new versions: it downloads the APK and hands it to Android's installer (it asks for "install unknown apps" the first time). A test build follows the nightly, a normal build follows the releases.

To report a problem: *Settings → Save log to Downloads*, then send the `DragonRage-log-….txt` file.

Build it yourself:

```sh
sh port/tools/android_toolchain.sh   # LLVM 21, the NDK's sysroot, SDL3 (once)
python3 port/tools/android.py        # the engine -> android/app/src/main/jniLibs
cd android && ./gradlew assembleRelease   # needs JDK 17+ and the Android SDK (compileSdk 35)
```

### Where files go

### macOS (experimental)

Apple Silicon Macs, native arm64, built from source on the Mac itself; there is no macOS release archive. The
attract-demo fight gives the same state as the Linux program on every frame, the rewind test (rollback) passes,
and the game draws through Metal with sound; online play against Linux / Windows has not been tried on two machines
yet. With [Homebrew](https://brew.sh):

```
xcode-select --install                       # (once: Apple's command line tools)
brew install llvm sdl3 shaderc spirv-cross
python3 install.py --iso <your disc image>   # then ./play.sh
```

`python3 install.py --check` lists what is missing. How the build works and what is left:
[docs/port/macos_plan.md](docs/port/macos_plan.md).

The release archives for Linux and Windows are built from the repository alone, with no disc involved:
`port/release/build.sh` (needs only docker) or the GitHub workflow in `.github/workflows/release.yml`. The
repository carries the shape of the game's built-in data tables without their values (`port/data`); the
finished program fetches the values from your disc when it starts.

**Android:** Everything lives in `Android/data/com.dfdx047.dragonrage/files/`, in **the same layout the PC port uses next to
its program**, so the engine will run with that folder as its working directory without Android-specific paths:

```
gamedata/            the disc's files (disc/SLUS_216.78, disc/BIN/, disc/DATA/ with the AFS archives, .installed)
gamedata/mods/       replaced files (rebuilt by the app from modlib/)
textures/            active texture packs (any depth)      textures_off/   packs switched off
stages/  songs/      extra stages and songs (maps.txt / songs.txt hold the menu names)
saves/               memory card
bt3_settings.txt     engine settings (name=number lines)
modlib/              the app's mod library
```

New keys in `bt3_settings.txt` for the Android side: `dr_unlock_all` (1 = run the unlock once at start, then reset to
0), `dr_renderer` (0 automatic, 1 Vulkan, 2 OpenGL ES; the app writes the engine's `gpu_api` from it). The on-screen
controller's settings, layout and macros are in `touch_controls.json`. An optional `match_url.txt` (one line,
`https://...`) names another matchmaking service for online play (see `port/matchmaking/`).

## Roadmap

### Done
- [x] Launcher app: game-data install from ISO with checksum verification, texture packs, mods, extra
      stages and songs, unlock-all cheat, all engine settings, Material 3 theme, English and Portuguese
- [x] CI: every push builds the APK ([android-nightly](../../releases/tag/android-nightly)); an `android-vX.Y.Z` tag
      publishes a versioned release
- [x] Engine builds for arm64 (332 of 332 sources), loads below the Java heap, and **boots and plays**: memory card
      check, logos, opening movie, menus, character select and battles on the AYN Odin 3 (Snapdragon 8 Elite), at
      the game's full 30 fps, about 2–3 W at 4x resolution
- [x] Worked around an LLVM 21 AArch64 bug: byte/halfword stores through 32-bit pointers were emitted as 4/8-byte
      stores (`irfix.py` routes them through an ordinary pointer)

### Since then
- [x] **Fast install**: the AFS archives are copied whole and read through their tables (no more ~20 000 small files)
- [x] **Modified ISOs** (translations such as PT-BR, mods such as "Tenkaichi 4"): installed with a warning; the
      engine takes their data. Changes a mod made to the game's *code* do not apply (the engine is the original
      program, rebuilt)
- [x] In-game menu: the game **pauses** while it is open, works with **touch**, fills the screen, every setting of the
      app, **Quit to launcher**; the back button and **START + SELECT** open it
- [x] **On-screen controller** made for this game (0.3.0), automatic menu / fight layout, the classic pad as an option,
      one button in clashes, layout editor, macros, three looks (0.4.0)
- [x] **Mali GPUs**: Vulkan without the optional features Mali lacks, shaders without `discard` where nothing is
      discarded, render targets a quarter of the size; **OpenGL ES 3.2** renderer, automatic on Mali (0.5.0)
- [x] **Engine loads anywhere in the low 4 GB** (relocatable), for devices whose Java heap sits where the engine used
      to go (Moto g100 and others)
- [x] **Custom GPU drivers** for Adreno (Turnip, newer Qualcomm drivers) through AdrenoTools
- [x] **Screen filters**: bilinear, sharp bilinear, FXAA, AMD FSR 1 (EASU + RCAS), Snapdragon Game Super Resolution;
      internal resolution in fine steps
- [x] **In-app update** from GitHub releases with the changelog, and **texture pack download** inside the app
- [x] **Smooth motion** (frame interpolation to 60/120 pictures a second), **present mode** selector, picture shaped to
      the screen (0.4.0)
- [x] **Online play** with room codes and rollback, from the PC port (0.4.0)
- [x] **HD texture packs** in `.dds` on phones' GPUs (unpacked on the CPU), a memory budget for the cache, a sturdier
      import (0.5.0)
- [x] Back from the background without a black screen; the log says the speed and where the frame time goes (0.5.0)

### Next
- [ ] **Weaker GPUs** (Mali-G52, Mali-G68, Adreno 610): fewer render passes and copies in the renderer; compare Vulkan
      and OpenGL ES on them
- [ ] Online play: our own matchmaking service (the PC port's has daily limits), Android to PC
- [ ] In-game menu in Portuguese
- [ ] Picture: anisotropic filtering, SMAA, more post-processing shaders
- [ ] Quality-of-life features of a "definitive edition" (the game's code is all here to change)
- [ ] Pause the game while the app is in the background

## Repository layout

| Path | Contents |
|---|---|
| `android/` | **Dragon Rage**, the Android app (Gradle project) |
| `src/`, `include/` | The game's code, from the decompilation; PC changes are inside `#ifdef PORT` |
| `port/` | The PC port: platform layer, renderer, sound, input, build tools |
| `docs/` | Notes on the game and on the PC port ([the PC port's original README](docs/port/upstream_README.md)) |

## Credits

- **[Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp)** by z3xox: the native PC port this is built on,
  and the matching decompilation BT3-Decompiled under it. Contributors of the PC port are listed in its README.
- Dragon Rage (Android app and port): [dfdx047](https://github.com/dfdx047).
- [AMD FidelityFX Super Resolution 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR) (MIT),
  [Snapdragon Game Super Resolution](https://github.com/SnapdragonStudios/snapdragon-gsr) (BSD-3-Clause),
  [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD-2-Clause), [SDL3](https://libsdl.org) (zlib).
- Made with the help of AI.

## Licence

The port's own code (everything under `port/` except `port/third_party/`, the setup, the build tools, and the
PC changes inside `#ifdef PORT`) is under the [GNU Affero General Public License, version 3](LICENSE)
(AGPL-3.0-only) in every commit after `06e4671` (2026-10-14). Up to and including that commit it was under the
[MIT licence](LICENSE-MIT), and that code, as it was then, stays available under MIT; contributions made under
MIT keep their MIT notice inside the AGPL whole. Whoever passes the program on, changed or not, or runs a
changed version for others over a network (a lobby or relay server, a web page), has to offer its source under
the same licence. The libraries in `port/third_party/` keep their own licences (Dear ImGui: MIT; newlib's maths
library: its BSD-style terms, in that folder).
The game's code in `src/` and `include/` comes from the decompilation and is not ours to license: it belongs
to the game's rights holders.

The Android app (`android/`) is under the [MIT licence](LICENSE-MIT).

## Legal

This project is not affiliated with or endorsed by the game's developers, publishers or rights holders. It exists
for preservation, study and interoperability, and distributes none of the game's assets. Dragon Ball and all
related names belong to their owners. You need your own legally obtained copy of the game.
