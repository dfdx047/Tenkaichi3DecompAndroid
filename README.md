<p align="center">
  <img src="android/art/banner.png" alt="Dragon Rage" width="100%">
</p>

<p align="center">
  <b>Dragon Ball Z: Budokai Tenkaichi 3, running natively on Android.</b><br>
  An Android port of <a href="https://github.com/z3xox/Tenkaichi3Decomp">Tenkaichi3Decomp</a>, the native PC port built on the BT3 decompilation.
</p>

<p align="center">
  <a href="../../releases"><img alt="Release" src="https://img.shields.io/github/v/release/dfdx047/Tenkaichi3DecompAndroid?include_prereleases&label=release&color=ff8a1f"></a>
  <a href="../../actions/workflows/android.yml"><img alt="Android build" src="https://github.com/dfdx047/Tenkaichi3DecompAndroid/actions/workflows/android.yml/badge.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Android%2010%2B%20·%20arm64-1e4fa8">
</p>

---

> [!IMPORTANT]
> **Status: the game runs on an AYN Odin 3 (Snapdragon 8 Elite, Android 15): menus and full battles, smooth, at
> the game's full 30 fps.** The whole game (332 source files: the decompiled game, the PC port's platform layer,
> renderer, sound) runs natively on Android arm64. Now: on-screen controls, other GPUs (MediaTek, low-end
> Snapdragon), modified ISOs, then 60 fps and picture options. See [Roadmap](#roadmap).

This repository contains **no game data**. You need your own copy of the **USA release (SLUS-21678)** as an
`.iso`. The app reads it on your device; nothing is downloaded or uploaded.

## What it is

The PC port is not an emulator: it is the game's own code, decompiled and compiled for PC, with a new renderer,
sound and input layer underneath. Dragon Rage brings that to Android in two parts:

| Part | Where | State |
|---|---|---|
| **Launcher app** (Kotlin, Jetpack Compose, Material 3; English and Portuguese) | [`android/`](android) | ✅ Done |
| **Game engine** (the port's C code, built for arm64 as `libbt3.so`) | `src/`, `include/`, `port/` | ✅ Runs on the Odin 3; other devices being tested |

## The app

<table>
<tr><td><b>Home</b></td><td>Pick your ISO. The app checks it is the USA release (the same SHA-1 checks as the PC setup; a translated or modded image installs too, with a warning) and copies the game data in the layout the engine expects. Play button.</td></tr>
<tr><td><b>Textures</b></td><td>Import a texture pack as a <code>.zip</code>. Packs made for PCSX2 (<code>.dds</code> / <code>.png</code>) work as they are. Turn each pack on or off, or all of them at once.</td></tr>
<tr><td><b>Mods</b></td><td>File-replacement mods as <code>.zip</code> (with a priority order), extra stages (<code>.unk</code>) and extra songs (<code>.adx</code>), with the name shown in the game's menus editable.</td></tr>
<tr><td><b>Cheats</b></td><td><i>Unlock everything</i>: every character, stage, song and item, and max Zeni. This is the debug function the developers left in the game. It is applied the next time the game starts.</td></tr>
<tr><td><b>Controls</b></td><td>The on-screen controller for playing without a gamepad: PlayStation, Xbox or Nintendo look, layout editor (move, resize, hide), macros (combos, several buttons at once, turbo), opacity, size, vibration, floating stick. Also reachable in game.</td></tr>
<tr><td><b>Settings</b></td><td>Internal resolution 1x–8x, aspect ratio (4:3 to 32:9), each of the five PS2 effects (outline, see-through tint, depth tint, glow, distance blur), glow strength, music / voice volume, on-screen controls, theme, Material You colours.</td></tr>
</table>

Dark theme with Dragon Ball colours (Goku's gi orange and blue, dragon-ball amber, Super Saiyan gold) by default,
or your wallpaper's colours with Material You. On wide screens (handhelds like the AYN Odin) the navigation moves
to a side rail.

### Install

Download the APK from [Releases](../../releases) and install it, or build it yourself:

```sh
cd android
./gradlew installDebug        # needs JDK 17+ and the Android SDK (compileSdk 35)
```

### Where files go

Everything lives in `Android/data/com.dfdx047.dragonrage/files/`, in **the same layout the PC port uses next to
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

New keys in `bt3_settings.txt` for the Android side of the engine to read: `dr_unlock_all` (1 = run the unlock once
at start, then reset to 0), `dr_fps60` (reserved). The on-screen controller's settings, layout and macros are in
`touch_controls.json`.

## Roadmap

### Done
- [x] Launcher app: game-data install from ISO with checksum verification, texture packs, mods, extra
      stages and songs, unlock-all cheat, all engine settings, Material 3 theme, English and Portuguese
- [x] CI: every push builds the APK ([android-nightly](../../releases/tag/android-nightly)); an `android-v*` tag
      publishes a release
- [x] Engine builds for arm64 (332 of 332 sources), loads below the Java heap, and **boots and plays**: memory card
      check, logos, opening movie, menus, character select and battles on the AYN Odin 3 (Snapdragon 8 Elite), at
      the game's full 30 fps, about 2–3 W at 4x resolution
- [x] Worked around an LLVM 21 AArch64 bug: byte/halfword stores through 32-bit pointers were emitted as 4/8-byte
      stores (`irfix.py` routes them through an ordinary pointer)

### Now (testing on devices)
- [x] **Fast install**: the AFS archives are copied whole and read through their tables (no more ~20 000 small files)
- [x] **Modified ISOs** (translations such as PT-BR, mods such as "Tenkaichi 4"): installed with a warning; the
      engine takes their data. Changes a mod made to the game's *code* do not apply (the engine is the original
      program, rebuilt)
- [x] In-game menu: the game **pauses** while it is open, works with **touch** (tap, drag to scroll), fills the screen,
      Resume button; the **Odin's back button** opens it
- [x] **On-screen controller**: d-pad, sticks (optional floating stick), face buttons (slide between them), shoulders,
      START/SELECT; **PlayStation, Xbox and Nintendo looks**; **layout editor** (move, resize, hide, reset) in the
      app and in game; **macros** (combos, several buttons at once, turbo); opacity, size, vibration; hides when a
      physical controller is used
- [ ] Texture packs from a .zip: check on a device
- [ ] **MediaTek** GPUs (Mali): debug and fix
- [ ] **Low-end devices** (Snapdragon 665 / Adreno 610): profile and optimise
- [ ] In-game menu in Portuguese; a cleaner layout

### Next
- [ ] **60 fps mode.** The game runs its logic at 30 fps (a fight frame waits two vertical blanks); a real 60 fps needs
      the simulation's timing changed (or frame interpolation)
- [ ] Picture: bilinear / sharp-bilinear and other output filters, anisotropic filtering, **FSR 1** and
      **Snapdragon Game Super Resolution** upscaling, post-processing shaders
- [ ] Quality-of-life features of a "definitive edition" (the game's code is all here to change)
- [ ] Performance on mobile GPUs; DDS (BC1–BC3) texture packs on GPUs without BC support
- [ ] Pause/resume when the app goes to the background; save states on arm64 (`port/src/gs/state.c` uses
      `getcontext`, which Android lacks)
- [ ] Online play (the PC port has rollback netplay)

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
- Made with the help of AI.

## Licence

The Android app (`android/`) and the PC port's own code (`port/` except `port/third_party/`) are under the
[MIT licence](LICENSE). The libraries in `port/third_party/` keep their own licences. The game's code in `src/` and
`include/` comes from the decompilation and belongs to the game's rights holders.

## Legal

This project is not affiliated with or endorsed by the game's developers, publishers or rights holders. It exists
for preservation, study and interoperability, and distributes none of the game's assets. Dragon Ball and all
related names belong to their owners. You need your own legally obtained copy of the game.
