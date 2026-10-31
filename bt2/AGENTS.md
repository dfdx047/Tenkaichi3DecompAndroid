# BT2 project working instructions

Use this repository as the project root.

## Start of a session

1. Read `docs/PROG.md` (current state), `docs/progress/ACTIVE_PASS.md` (handoff and how to run) and `docs/project/IMPLEMENTATION_PLAN.md` (order of work).
2. Before architecture or unfamiliar PS2 subsystem work, consult `docs/reference/REFERENCE_ARCHITECTURE.md` and its linked sources, and the subsystem's plan in `docs/plans/`.

## Goal and architecture

BT2-Recomp is a native Windows x64 port of *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441) with a minimal compatibility layer, not a PS2 emulator.

- **Recompiled game code is the base.** EE code runs as generated native C++ (the main executable and the `DBZP.BIN` overlay). Replace individual functions with hand-written C++ only where it pays off. No full decompilation or full-system emulation. An EE interpreter runs only as a logged bridge for code the map does not hold yet: what it runs is compiled natively at the next build, and the gate runs interpret nothing ([whole-game coverage plan](docs/plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), decided 2026-10-18).
- **Native services over emulation.** PS2 services are answered by host code at the highest boundary BT2's evidence supports: SIF RPC services (disc, files, sound, controller, memory card, CRI) instead of IOP modules and hardware. No IOP code runs. Emulate only what the game drives directly: EE DMA, GIF and VIF packets, VU1 microprograms, the IPU, the EE kernel.
- **Graphics:** the game's own GIF/VIF output is rendered on the GPU through paraLLEl-GS. It is the only renderer (the CPU renderer was removed 2026-10-21, the user's decision); the GS command sink checks the packets and follows the GS registers and transfers.
- **Sound:** the game decodes its streams itself; the port mixes what the native services receive. Sound effects come from a native sampler behind the SOUNDS service. The probe writes the mix to a WAV file or plays it on the audio device (`docs/plans/sound/`).
- **Smallest layer.** Implement only what BT2 reaches, in the simplest native form. One implementation per behavior; generalize verified special cases instead of adding handlers gated on guest PCs, stack offsets or call counts; remove code a replacement makes obsolete.
- **Fail explicitly.** Unsupported requests stop at a checked boundary that names them; never fake success.
- **References:** adapt behavior, never copy implementations 1:1, and never copy decompiled game code (for example Tenkaichi3Decomp's `src/`). Keep unknown function names neutral.

## Correctness and status

- **Game-visible results must match the original:** EE memory, files, replies, frames, sound. Validate with tests and PCSX2 comparisons; add a test for each service request type and generalized behavior.
- **Compare where it is exact.** Prefer byte-exact comparisons of memory, requests and register values over judging the final picture or sound. For sound, the working check is tests plus the user's listening; the comparison with the original is an optional final step (`docs/plans/sound/SOUND_OUTPUT_PLAN.md`, Verification) and does not block progress.
- **The user verifies by inspection.** They know how the game looks and sounds: save frames to `work/frames/` and audio to `work/audio/` and ask. Record what they confirmed, and keep it distinct from a comparison with the original.
- **Honest status.** Keep Verified separate from Operational or untested. Say what was measured, on which run. Never infer gameplay performance from startup progress; measure Release builds on representative workloads.

## Meaning of "continue"

A plain "continue" (any spelling or case) runs a goal-driven pass, automatically and across sessions. Work through "Current next step" in the implementation plan, in order, toward the next functional milestone: inspect -> gather evidence (captures, tooling, references) -> implement the native replacement or general behavior -> tests and original comparisons -> Release rerun -> next item. A new execution boundary is input to the plan: decide whether it belongs behind a native service or needs general EE-side behavior, and implement it at subsystem scale.

- **Scope follows the request.** By default finishing one item or fix is not a stopping point. When the user asks for a small pass, do one item and stop at its verified, committed checkpoint. When the user says not to start a pass, do only what was asked.
- **Stop only when** a new major subsystem needs its own plan (write it in `docs/plans/`; if actionable, follow it and continue), required information or access is unavailable in this session, or the next change would be speculative after consulting the evidence. State the condition and the evidence.
- **End of every pass:** run the gate, update `docs/PROG.md`, the handoff and the monthly history, review the diff and staged files, commit and push.
- Reduce pass scope for the usage allowance only when the actual remaining usage is low.

## Content rules

Never download or commit proprietary game/BIOS content, retail executables, disc images, extracted assets, captures, recordings or proprietary-derived generated code. Local content (`rom/`, `game_data/`), analysis output (`work/`, `research/`), generated code (`game/generated/`) and build output stay ignored. Documents record numbers and descriptions only. `README.md` stays at the root; documentation stays in `docs/`.

## Builds and runs

- **Narrow checks first:** unit tests, offline tools, targeted builds. Batch related changes. Every pass completes the full gate before commit.
- **Build with Ninja:** `python tools/build/msvc.py cmake --build build/probe --config <Debug|Release> [--target bt2-entry-probe]`. Keep the precompiled headers; no unity builds; never merge generated units.
- **Gate:** `python tools/gate/run_gate.py --disc <iso>` builds Debug and Release and runs the unit tests, the Python suites, the Debug probe runs with the original comparisons, and the Release battle run (the scripted fight against its recorded baseline, `tools/gate/battle_run.py`).
- **Long runs use Release:** `run_startup_probe.py --config Release --frames <n> --pad-script <script>`. Pad scripts count fields; snapshots count flips.
- **New code the game reaches:** `python tools/ps2recomp/discovery_loop.py` runs, discovers, regenerates and rebuilds until the run passes or a stop needs a decision.
- **Headers reached by generated code** (`ps2_runtime.h`, `ps2_runtime_macros.h`, `ps2_fpu.h`) hold only the CPU context and the entry points generated code calls; an edit recompiles every generated unit (about 6 minutes). Runtime state goes in `ee_runtime.h` (`EeRuntime`), implementation in focused `.cpp` files. A new entry point for generated code is declared in `ps2_runtime.h` and forwarded in `guest_interface.cpp`.
- **One at a time:** no concurrent native builds or probe runs. Exception: a sweep tool's own parallel runs (`--jobs`, the user's decision of 2026-10-19), with nothing else building or running, and at most 6 at a time: the user keeps using the PC (4 of its 10 cores stay free).
- **Long-running commands:** make them write a timestamped log, and each time one starts give the user a working command to follow it, with the absolute path, for PowerShell and for cmd.
- **Writing files from a shell:** shell heredocs in this environment corrupt backslash escapes. Write or edit files that contain backslashes with the file tools, or with a script file created that way.
- **After a pass,** keep only intentional build outputs and required diagnostic artifacts; delete temporary residue, stale logs, abandoned builds, old dumps and backups. Never delete source inputs, probe inputs, reference data or test artifacts without verifying they are unused.

## Code organization

Small focused files with one clear purpose; explicit dependencies; check whether new code belongs in its own file before growing an existing one; no artificial splits; preserve existing behavior and generated-code boundaries. [docs/guides/RUNTIME_LAYOUT.md](docs/guides/RUNTIME_LAYOUT.md) maps the `runtime/` folders and where new code goes; [docs/guides/TOOLS.md](docs/guides/TOOLS.md) describes the tools and diagnostics. The probe is the runner for automated work. The player-facing programs (game, launcher, in-game overlay) follow `docs/plans/launcher/LAUNCHER_PLAN.md`; their files live in the ignored `user/` folder.

## Documentation

- **Layout:** `docs/PROG.md` (current progress; read by `tools/docs/sync_progress.py`, keep the path), `docs/progress/` (active handoff, monthly history, dashboard data), `docs/guides/` (BUILDING, TOOLS, RUNTIME_LAYOUT), `docs/project/` (implementation plan, progress tracking, legal), `docs/reference/` (reference architecture, reverse-engineering evidence), `docs/plans/<subsystem>/` (`ee`, `iop`, `graphics`, `sound`, `video`, `disc`, `input`, `memory-card`, `launcher`; add a folder only when none fits). Never leave a document loose in `docs/`.
- **Progress tracking:** follow `docs/project/PROGRESS_TRACKING.md`. Record pending changes and remaining verification in the handoff before an interruption. Export only allowlisted counters. Before a progress commit run `tools/docs/render_status.py` and `tools/docs/sync_progress.py`, each also with `--check`. Never count isolated service coverage as full startup coverage.
- **Links:** when a file moves, update every relative link and `docs/...` path to it (Markdown, code comments, CMake messages) and check that all links resolve. Historical entries change only in such paths.
- **Encoding:** read and write docs as UTF-8; check changed files for replacement characters and mojibake before committing.

## Commits

Review the staged files before each commit: no proprietary or generated content, no stray artifacts. Commit messages describe the change; no tool or assistant attribution lines.

## Parallel work

Use sub-agents only when the user asks for them, for independent investigation, reference comparison, capture analysis or test design, with clear file ownership. The primary agent coordinates scheduler, RPC and runtime state changes; avoid speculative parallel runtime edits.
