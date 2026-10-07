# Tenkaichi3Decomp

A native PC port of **Dragon Ball Z: Budokai Tenkaichi 3** (PlayStation 2, USA release, SLUS-21678), built on
the matching decompilation in **BT3-Decompiled**. It is the game's own code compiled for PC, with a new
renderer, sound and input layer underneath; it is not an emulator.

This repository contains **no game data**. You need your own disc image of that release: the setup reads the
game's data from it, and it stays on your computer.

## What works

- The whole game loop: boot, opening movie, menus, saving, fights, split screen.
- A GPU renderer (Vulkan) with the PS2's effects (outline, glow, depth tint, distance blur), each switchable.
  An OpenGL 3.3 renderer draws the same picture for machines without a working Vulkan (F1, Video, Renderer; it
  is also tried by itself when Vulkan cannot start). It is slower than Vulkan.
- Internal resolution from 1x to 8x; 4:3, 16:9, 21:9 and wider without stretching the fight.
- Music, voices and sound effects; keyboard and controllers, fully rebindable; two players.
- Texture packs made for PCSX2 (`.dds` or `.png`): copied into the `textures` folder next to the game, they are used as they are.
- Stages and music from outside the disc: a stage model (`.unk`) dropped into the `stages` folder next to the
  game, or a song (`.adx`) into `songs`, is added to the stage select, its name written in the game's own
  lettering (cut from the disc's name pictures while the game runs). They are not written to the save and are
  not offered in an online match. See [docs/port/adding_stages.md](docs/port/adding_stages.md) and
  [adding_songs.md](docs/port/adding_songs.md).
- In the game, **F1** opens the settings. Its Cheats tab has "Unlock everything" (a debug function the game's
  developers left in: all characters, stages, music, items and Zenni).

- **Online play, experimental** (from release 0.1.8): the main menu's hidden "Dragon Net Battle"
  entry is back and opens a small window. One player hosts on a port, the other joins with the host's address;
  both get the same roster with everything unlocked, each sees their own fighter's view full screen, and leaving
  the match puts each player back where they were with their own save. Linux and Windows play against each
  other. With rollback (the host chooses how many frames, and the input delay) your own moves come out at once
  and the game corrects itself when the other player's buttons arrive.

Online play is new: it has been played between two machines on one local network, not yet over the internet,
and the host's port has to be reachable from the other player (port forwarding; there is no relay or
match-making). The 32-bit Linux build has no rollback. [docs/netplay_notes.md](docs/netplay_notes.md) is the
working log of how it is built and what was checked.

See [docs/port/README.md](docs/port/README.md) for the running log of what is done, what is verified and what is not.

## Requirements

- A 64-bit x86 processor and a graphics card with Vulkan support. Most of the work is on the processor (the
  game's arithmetic is reproduced exactly as on the PS2); a recent mid-range CPU is recommended.
- Linux (glibc 2.35 or newer) or Windows 10 / 11.
- About 4 GB of disc space, and your disc image as an `.iso` file.

## Installing a release

Unpack the release, start `Tenkaichi3Decomp-setup` (`Tenkaichi3Decomp-setup.exe` on Windows), choose your disc image, press Play. The
setup checks that the image is the unmodified USA release and unpacks the game's data next to the program.

## Building from source

```
python3 install.py --iso <your disc image>
```

does everything on Linux: checks the tools, reads the disc, builds the 64-bit program and runs the game's demo
fight as a test. `python3 install.py --check` lists what is missing. `port/setup/build.sh && ./Tenkaichi3Decomp-setup` is
the same with a window.

The release archives for Linux and Windows are built from the repository alone, with no disc involved:
`port/release/build.sh` (needs only docker) or the GitHub workflow in `.github/workflows/release.yml`. The
repository carries the shape of the game's built-in data tables without their values (`port/data`); the
finished program fetches the values from your disc when it starts.

## Layout

| Path | Contents |
|---|---|
| `src/`, `include/` | The game's code, from BT3-Decompiled; changes for PC are inside `#ifdef PORT` |
| `port/src/` | The PC side: memory, files, memory card, movies; `gs/` renderer, sound, input, settings window |
| `port/tools/` | The build: pointer rewriting for 64-bit, packaging, extraction |
| `port/data/` | The game's built-in data tables as shape only (labels, sizes, pointer slots), no values |
| `port/release/` | The container and scripts that build the release archives |
| `port/setup/` | The setup window |
| `port/third_party/` | Dear ImGui (MIT), the maths library of newlib |
| `docs/` | Notes on the game (from the decompilation) and `docs/port/` on the port |

This project was made with the help of AI.

## Licence

The port's own code (everything under `port/` except `port/third_party/`, the setup, the build tools, and the
PC changes inside `#ifdef PORT`) is under the [MIT licence](LICENSE). The libraries in `port/third_party/`
keep their own licences (Dear ImGui: MIT; newlib's maths library: its BSD-style terms, in that folder).
The game's code in `src/` and `include/` comes from the decompilation and is not ours to license: it belongs
to the game's rights holders.

## Legal

This project is not affiliated with or endorsed by the game's developers, publishers or rights holders. It
exists for preservation, study and interoperability, and distributes none of the game's assets. All rights to
the game belong to their owners. You need your own legally obtained copy of the game to use it.
