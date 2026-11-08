# Implementation plan

Target: *Dragon Ball Z: Budokai Tenkaichi 2*, North American PS2 release SLUS-21441, running the original game logic natively on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20 and CMake.

**Goal:** a native Windows x64 port with a minimal compatibility layer, not a PS2 emulator. The game's EE code is statically recompiled to C++; the PS2 hardware underneath is replaced by native services (disc, sound, controller, memory card) and GPU rendering through paraLLEl-GS. Then optional enhancements that never change how the game plays.

## Direction

| Area | Decision |
| --- | --- |
| Game code | The recompiled EE code is the base. Hand-written C++ only where it pays off (hardware-facing code, hot loops). No full decompilation. An EE interpreter only as a logged bridge for code the map does not hold yet, compiled natively at the next build (2026-10-18, [plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)). |
| IOP side | Native RPC services answer the game's SIF RPC requests (disc, files and heap, controller, memory card, sounds, CRI ADX); no IOP code runs (2026-10-11). |
| Graphics | The game's own GIF and VIF packets, rendered on the GPU by [paraLLEl-GS](../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference); VU1 programs native, the heaviest hand-written. The only renderer; the GS command sink checks the packets (the CPU renderer was removed 2026-10-21). |
| Films and sound | The game's Sofdec library decodes the films through a native IPU; ADX streams and a native effects sampler are mixed by the port. |
| Enhancements | Off by default, settings in the launcher and the in-game menu. They must leave the fight's speed, physics and results exactly as in the original (the user, 2026-10-20), and the port must not be held back by the processor or the graphics card. |

## Architecture rules

- **Native service over emulation**, at the highest boundary BT2's evidence supports; emulate only what the game drives directly (EE DMA, GIF and VIF packets, VU1, the IPU, the EE kernel).
- **Implement what BT2 requires,** in the simplest native form; **one implementation per behavior**; generalize verified special cases instead of handlers gated on guest PCs, stack offsets or call counts; remove what a replacement makes obsolete.
- **Game-visible results must match** the original (EE memory, files, replies, frames, sound); compare byte-exactly where possible. **Fail explicitly** at a checked boundary; never fabricate success.
- **Tests** for each service request type and each generalized behavior; status in the vocabulary of [PROG.md](../PROG.md).
- **Performance:** measure Release builds on representative workloads; report time per frame or share of run time, never projected gameplay performance.

## Milestones

| Milestone | Status |
| --- | --- |
| Foundation, generation, startup to the frame loop, controller and card checks | ✅ Verified |
| Films, title, menus, all game modes | 🟢 Operational |
| Versus battle with a controller, sound, real-time speed | 🟢 Operational |
| Story mode (all parts and towns without input) | 🟢 Operational; not played through by the user |
| Launcher, settings, in-game menu, fast forward | 🟢 Operational |
| Saving | 🔧 Incomplete: not tried in the game |
| Enhancements: HD textures, super-sampling, widescreen | 🟢 Operational |
| Enhancements: 60 fps battles, FSR 1 scaling, fighters' HD bodies | ⏳ In progress |
| Final validation: exhaustive battle sweep, comparisons of 3D frames | ⏳ Not reached |

## Current next step

Done, in order: native services; GPU rendering; films; the 3D pipeline; menus and battle setup; sound; controller; launcher, settings and overlay; whole-game coverage (all modes without input); performance (fights at 30, menus at 60); HD textures, super-sampling and widescreen. Details in the [October 2026 history](../progress/2026-10.md).

Next, in order (the user, 2026-10-20):

1. **60 fps battles** ([plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md)): in-between pictures drawn from the game's commands on a second renderer, shown between two fight frames; the setting in the launcher and the overlay; checked against the 30 fps fight (identical game).
2. **Picture** ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md)): FSR 1 scaling with sharpening, an edge filter (FXAA or SMAA); the fighters' HD bodies and the button art choice ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md)).
3. **The user's play-testing** of saving, the enhancements and the fixes; stops are mapped with `discovery_loop.py --player-stop`.
4. **Final validation:** the exhaustive battle sweep; comparisons of 3D frames with the original.
5. **At the very end:** the runtime generalization plan's groups 4 to 6 ([plan](../plans/ee/RUNTIME_GENERALIZATION_PLAN.md)), once the game boots and everything works.

## Working method

- **"Continue"** runs a goal-driven pass as [AGENTS.md](../../AGENTS.md) defines: work through "Current next step" in order: inspect → evidence → native replacement or general behavior → tests and comparisons → Release rerun → next item. A new subsystem gets its plan in `docs/plans/<subsystem>/` first.
- **Evidence:** local captures, the tools and the [reference architecture](../reference/REFERENCE_ARCHITECTURE.md); adapt reference behavior, never copy implementations or decompiled game code. Record confirmed facts, hypotheses and unknowns separately; keep unknown names neutral.
- **Content:** game content in ignored `rom/` or `game_data/`, analysis in `work/` or `research/`, generated code in `game/generated/`; never commit proprietary content.
- **End of a pass:** the gate, [PROG.md](../PROG.md), the handoff and the monthly history, a review of the staged files, commit and push.
