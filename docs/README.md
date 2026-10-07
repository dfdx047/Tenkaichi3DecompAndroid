# Dragon Ball Z: Budokai Tenkaichi 3 (PS2, USA, SLUS-21678) decompilation notes

What has been learned about the game while decompiling it. The goal of the project is a PC port
with new online play, built on a matching decompilation of the PS2 build.

## How to read these docs

Every fact carries one of two labels:

- **Verified**: the C that states it compiles to the original bytes (checked per function with
  `scripts/fdiff.py`, and by the byte-identical linked build once the file is linked).
- **Inferred**: read from disassembly, deduced from callers, or a judgement about meaning.
  Behaviour can be verified while the *name* or *purpose* is still inferred; the docs say which.

Exact struct layouts live in the headers under `include/`; the docs explain and point to them.
Names marked `// guess` in `config/symbols/*.txt` are best guesses.

## Index

| Doc | Contents |
|---|---|
| [systems/boot_and_frame.md](systems/boot_and_frame.md) | Boot order, the top-level loop, the frame skeleton, timing |
| [systems/memory.md](systems/memory.md) | Heap, battle arena pool, lists and queues |
| [systems/files_and_assets.md](systems/files_and_assets.md) | Disc layout, file ids, the file layer, compression, loading screen |
| [systems/input.md](systems/input.md) | Pad state, menu input word, battle input path |
| [systems/audio.md](systems/audio.md) | Streamed audio (ADX) and sound effects (SOUNDS.IRX protocol) |
| [systems/graphics.md](systems/graphics.md) | Display lists, frame buffers, render state, ordering table, fades |
| [systems/battle.md](systems/battle.md) | Battle frame, sequence states, setup block, modes, loading, results, events, objects |
| [systems/fighter.md](systems/fighter.md) | Roster, fighter object, per-frame phases, hit-stop, character changes, fighter camera |
| [systems/combat.md](systems/combat.md) | Flags, actions, input conditions, movement, hits, damage, gauges, stats, clashes, time stop |
| [systems/effects_stage.md](systems/effects_stage.md) | Effect objects, screen effects, stage: what is simulation and what only draws |
| [systems/ai.md](systems/ai.md) | The CPU opponent |
| [systems/script.md](systems/script.md) | The GSC script engine and battle (story) scripts |
| [systems/save_data.md](systems/save_data.md) | The 0x4000-byte save block |
| [systems/math.md](systems/math.md) | Vector, quaternion and matrix conventions; random numbers |
| [roadmap.md](roadmap.md) | The agreed order of work |
| [status.md](status.md) | Handoff note: what is linked, what is waiting, what is running |
| [known_bugs.md](known_bugs.md) | Bugs and quirks in the original code that a port must reproduce or consciously fix |
| [netplay_notes.md](netplay_notes.md) | Everything relevant to deterministic netplay |
| [open_questions.md](open_questions.md) | What is not known yet |
| [decomp_guide.md](decomp_guide.md) | How to decompile a module; compiler and assembler quirks |
| [agent_rules.md](agent_rules.md) | Working rules for parallel decomp agents |
| [text_map.md](text_map.md) | Address ranges of the executable: game vs library code |
| [port/adding_stages.md](port/adding_stages.md) | Adding stages (maps) to the port: the manifest, the script and how it works |
| [port/adding_songs.md](port/adding_songs.md) | Adding songs to the music select: the manifest, the script and how it works |
| [game_overview.md](game_overview.md) | The first static analysis (partly superseded; corrections at its end) |

## Binaries

| File | What | Size |
|---|---|---|
| `SLUS_216.78` | main executable: engine, battle, libraries | 1.8 MB of code |
| `BIN/DBZP.BIN` | menu overlay ("PROGRESS"), loaded at 0x334C00 | 0.5 MB of code |
| `IRX/*.IRX` | I/O processor drivers (Sony, CRI, and the game's SOUNDS.IRX) | 0.7 MB |

Code breakdown of the main executable (inferred, from static analysis in `text_map.md`): about
81% Spike's game code (~6,400 functions), 8.5% CRI middleware, 7% Sony SDK, 3.5% C library and
compiler runtime. The overlay adds 737 game functions.

Compiler: Sony ee-gcc 2.96, `-O2 -fno-strict-aliasing`, `-G8` for game code and `-G0` for CRI
(verified: every decompiled function matches with these flags). The game code seen so far is C,
not C++.

Internal project name: `zs3` (from CVS metadata left on the Wii disc). The developers called the
menu overlay `PROGRESS`.

## Progress

Run `python3 scripts/progress.py` after a build for current figures.
