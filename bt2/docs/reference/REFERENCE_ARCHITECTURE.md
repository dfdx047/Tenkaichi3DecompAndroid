# Reference architecture review

Inspected on 2026-10-03, without building either reference or downloading game binaries/assets. BT3-Recomp is the primary architectural reference; Chronicle supplies selected validation practices. Since 2026-10-11, Tenkaichi3Decomp is the primary reference for the native layer (services, audio, movies) and paraLLEl-GS for GPU rendering. No BT3/Dark Cloud addresses, patches, or game implementations are used in BT2.

## BT3-Recomp

Snapshot: `e22a34beb564e2d0f32bd60b02f0a95d9645ddcc`. These are source observations, not independent verification of gameplay claims.

| Area | Observed approach | BT2 decision |
| --- | --- | --- |
| Function maps | Main and overlay CSVs use Name/Start/End/Size and exclusive ends. An oversized-map splitter deals with overlapping analyzer blobs and compiler limits. | Automate provisional neutral EE rows, quarantine noncontiguous/overlapping/oversized bodies, report exclusions. Do not copy BT3 boundaries or guess chunk cuts. [Map preparation](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/split_functions.py). |
| Configuration | Local ELF/map/output paths; per-file generation; syscall instruction handling; address-qualified game stubs and selective COP0/cache patches. | Local configuration and output; no BT3 stub list or instruction patches. [Config](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/config.toml.in). |
| Pipeline | Validate input, prepare map, build recompiler, generate, apply authored patches, install generated sources/headers into runtime, build runner. | One repeatable map/generate/build/run/diagnose command, hashed output provenance, separate host and optional generated probe. [Setup](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/setup.py). |
| Overlays | Separately wraps local gameplay binary as ELF, generates separate maps/tables and adds known re-entry registrations. | Treat BT2's unmapped call as a future execution diagnostic. Do not assume BT2 uses BT3's overlay format or base. [Overlay tooling](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/gen_overlay.py). |
| Runtime/EE | Generated ABI is memory/context/runtime; context has 128-bit GPRs and VU state, address registration and branch dispatch. | Start with a clearly scoped authored ABI adapter for one generated routine; next implement missing entry ABI surface from compiler failures. [Runtime API](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/ps2xRuntime/include/ps2_runtime.h). |
| VU | Manifest identifies local microprogram segments/hashes; setup extracts and translates them. Runtime retains an interpreter fallback. | Keep VU discovery separate from the EE `.text` map; defer until execution needs it. [VU tooling](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/vu1_programs.py). |
| GS/rendering | GPU renderer records commands on GIF/DMA side and replays on the presentation thread. Native engine-seam rendering is a separate evolving path. | Rendering is a later compatibility boundary; no graphics dependency needed for the first generated execution. [GPU interface](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/ps2xRuntime/include/runtime/ps2_gs_gpu_renderer.h), [native seam design](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/docs/NATIVE-RENDER-SEAM.md). |
| Patches/build | Authored post-generation patches use anchors and report missing matches; Windows setup delegates to Python and selects ClangCL. | Keep any future BT2 patch deterministic, evidence-based and checked. Retain our MSVC/C++20 target. [Patches](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/apply_patches.py), [Windows workflow](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/scripts/build-windows.ps1). |

## Tenkaichi3Decomp (native port reference)

[Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp) (snapshot `80e0dd42f751f26214843c7c0f4a281ea0b66cfb`) is a native PC port of *Budokai Tenkaichi 3* (SLUS-21678), built from the BT3-Decompiled matching decompilation: "the game's own code compiled for PC, with a new renderer, sound and input layer underneath; it is not an emulator." Its port code (`port/`) is MIT; the decompiled game code (`src/`, `include/`) belongs to the game's rights holders and must not be copied. BT3 shares BT2's engine lineage, so its notes are the closest available evidence for how this engine uses the hardware.

| Area | Tenkaichi3Decomp approach | BT2 use |
| --- | --- | --- |
| Graphics | A software interpreter decodes the DMA chain, VIF1, GIF packets and GS registers, feeding an SDL3 GPU back end (Vulkan, OpenGL 3.3 fallback). The nine VU1 vertex programs ran in an interpreter first; most were then replaced by shaders. Some full-screen effects are native passes placed by a display-list marker. | Confirms that GS packet consumption plus a GPU back end works for this engine, and that VU1 programs are few enough to replace one by one. |
| Sound | `SOUNDS.IRX` is replaced, not emulated: a native sampler for effects and ADX players for music and voices on SDL audio streams. | The shape of our sound output ([plan](../plans/sound/SOUND_OUTPUT_PLAN.md)). Its port log (`docs/port/README.md`, sources `port/src/gs/snd_se.c` and `snd_adx.c`) reports Sony's sequencer and synthesizer modules behind SOUNDS, effects that are single note-ons with one sample each, and an effects level explained by volumes of 100/127 and the chip's half-scale voice volume. BT2's disc carries the same module and chunk names (checked 2026-10-13); everything else is to be established on BT2. |
| Movies | A port-specific MPEG-2 decoder (`plat_mpeg2.c`) for the opening and ending movies; intra pictures only, checked against ffmpeg. | Evidence for the video plan: a small native decoder instead of IPU emulation. |
| Kernel, SIF, CD, files, memory card | Stubbed by `plat_sys.c`, `plat_stub.c` and `plat_mc.c`; files are read from the extracted disc. | Model for a minimal service layer in place of emulated IOP modules. |
| Memory model | Pointers stay 32-bit: first a `-m32` build, later 64-bit with clang `__ptr32` pointer members and game memory below 4 GB. | Not needed for recompiled code, which already addresses guest RAM by offset; relevant only if decompiled C is introduced. |

## PS2Recomp runtime (architectural reference)

[PS2Recomp](https://github.com/ran-j/PS2Recomp) is our pinned recompiler (`config/ps2recomp.lock.json`). Its runtime, `ps2xRuntime`, is the architectural reference for runtime structure (local checkout: `work/dependencies/ps2recomp-<revision>`). PS2SDK and PCSX2 remain the behavioral references. Each pattern is reused where a BT2 blocker needs it, without replacing working, verified BT2 code.

| Area | PS2Recomp structure | BT2 use |
| --- | --- | --- |
| GS | GIF parsing (`gs_frontend`) separate from a CPU rasterizer (`gs_cpu_backend`); GS memory and one swizzle module per pixel format (`ps2_gs_psmct32/16`, `psmt8`, `psmt4`); a texture page cache. | Our `gs_command_sink` combines parsing, memory and rasterization. Split along these lines when GS work next grows (new formats, VU1 output), keeping the validated layouts and the transaction rollback. |
| GIF paths | `ps2_gif_arbiter` arbitrates PATH1 (VU1 XGKICK), PATH2 (VIF1 DIRECT) and PATH3 (GIF DMA). | Needed once VU1 XGKICK output must interleave with VIF1 DIRECT and GIF DMA; until then frames use PATH2/3 in issue order. |
| VIF1 | `ps2_vif1_interpreter`: a code-by-code VIF1 interpreter (UNPACK, MSCAL, DIRECT and friends). | Our frame VIF1 path walks chains and accepts only NOP/FLUSH/DIRECT. Extend it code by code as BT2 reaches UNPACK, MSCAL and others. |
| VU1 | Interpreter split into `ps2_vu1_core`, `_upper` and `_lower`, with a `ps2_vu1.h` state header. | Follow this split when BT2 first executes a VU1 microprogram; VU discovery stays separate from the EE map (see BT3 above). |
| Kernel/syscalls | One stub file per subsystem (GS, DMA, Pad, SIF, MemoryCard and so on) and a dispatcher. | Matches our focused files (`runtime/ee`, `runtime/iop/providers`); keep BT2's evidence-checked handlers. |

## paraLLEl-GS (GS rendering reference)

[paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs) (snapshot `cc6184af7e0c03da603045ca371ffa5dae9b0655`, LGPL-3.0) is a PS2 Graphics Synthesizer emulator by Hans-Kristian Arntzen (Arntzen Software). It runs on Vulkan compute shaders, written from scratch rather than derived from GSdx. It aims for software-renderer accuracy with optional super-sampling, and handles de-interlacing (weave, CRTC field blending, progressive promotion when FFMD is 0). It is the reference for GS rendering behavior and for a future GPU renderer; PCSX2 remains the behavioral reference for the rest of the hardware.

| Area | paraLLEl-GS | BT2 use |
| --- | --- | --- |
| GIF and register state | GIF payloads consumed per path (`gs_interface`: `gif_transfer`); context registers (TEX0/TEX1, TEXCLUT, TEXA) and privileged registers (PMODE, SMODE, DISPFB, DISPLAY, CSR). | Reference for our GIF parser and GS register file as new packet forms and registers are reached. |
| Rendering accuracy | Software-renderer accuracy as the target; varying interpolation not bit-exact with hardware. | Check our CPU rasterizer's primitive, texture and blending rules against it as GS features are reached. |
| GPU renderer | GS emulation in Vulkan compute (`gs/`, `ubershader.comp`), with Granite as its Vulkan layer. | Architecture reference if the CPU rasterizer (about 75% of a Release run) is replaced by a GPU path. |
| Presentation | Weave de-interlacer, CRTC field blending, FFMD progressive promotion, optional FSR1 upscaling. | Reference for the live window's interlaced presentation and later scaling. |

Adapt its approach; do not copy code. Its Vulkan/Granite dependency is not part of our current build.

## Chronicle

Snapshot: `05fd37be122cd5833cfeeceb83475933959d938f`. This is a matching decompilation and port, rather than our PS2Recomp pipeline. Region configs associate hashes, symbol maps, and section layout; its validation tools compare built/reference images. Borrow explicit identity, module/symbol provenance and reproducible comparison, without importing its symbols or assuming byte-matching requirements for native recompilation. [NTSC config](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/config/ntsc/main.yaml), [validator](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/scripts/build/verify.py).

Its PCSX2 launcher boots a user's local ISO on the host. Use that separation for future behavioral comparisons with recorded version, input hash, scenario and observations. Its PC platform separation is useful, but the documented Linux/C++26/Vulkan setup is not our Windows/MSVC target. [PCSX2 launcher](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/scripts/host/pcsx2.sh), [PC notes](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/docs/PC.md).

## Whole-game coverage (2026-10-18)

Consulted for the [whole-game coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md): BT3-Recomp at `e22a34beb564e2d0f32bd60b02f0a95d9645ddcc` (its latest revision, the same as above) and Tenkaichi3Decomp at `ec3c85a`, of which only `docs/`, `port/` and `scripts/` were checked out; its decompiled game code (`src/`, `include/`) was not downloaded. No code copied.

| Area | Reference approach | BT2 use |
| --- | --- | --- |
| Code the map misses | BT3-Recomp: a MIPS interpreter (`ps2xRuntime/src/lib/ps2_interp.cpp`, `interpretUntil`) runs code without a recompiled function and calls recompiled functions natively when it reaches them; `PS2X_COVERAGE` logs each interpreted address. | Adopted as a logged bridge: what it runs is compiled at the next build; the gate runs interpret nothing. |
| Progressive recompilation | BT3-Recomp's orchestrator (`tools/orchestrator/`) keeps a corpus of logged units, discards those seen before and plans the next, hottest first. It depends on natural play to find units. | Same corpus model, fed by sweeps of game states started directly, as well as by play. |
| Entries inside functions | BT3-Recomp inserts labels and function-table slots for callback and jump-table targets inside a function (`games/bt3/gen_overlay.py`, `REENTRY_LABELS`), found one by one. | Re-entry labels for every such target a static scan finds. |
| Kernel | BT3-Recomp answers system calls by number (`ps2xRuntime/src/lib/Kernel/Syscalls/`) and SDK calls with stubs; its runtime also holds many fixed game addresses. | Layout reference for general handlers by number and address; the fixed game addresses are not adopted. |
| What the game needs | Tenkaichi3Decomp: `port/tools/link.py` creates a stub for every missing symbol (about 130 at the first link), the work list of its native layer. | A list of the system calls, register addresses, RPC requests and VU1 instructions the game's code uses, against what the runtime handles. |
| Reaching game states | Tenkaichi3Decomp: `port/src/headless.c` stands in for the menu routine, fills the battle setup (`BT3_CHARA`, costume, stage) and starts the CPU-vs-CPU demo battle or a recorded replay; a leftover debug routine unlocks everything. Its notes (`docs/systems/boot_and_frame.md`) describe the overlay's entry choosing screens by a mode field through a jump table. | A tool-only starter through BT2's own state fields, established on BT2's code; sweeps over characters, maps, screens and episodes without input. |
| Exact checks | Tenkaichi3Decomp: a state hash per vertical blank (`BT3_HASH`, `compare_hash.py`) names the first differing blank; `compare_state.py` compares memory at a battle tick with a PCSX2 save state. | A state hash per field guards refactors and the interpreter. |

## Working strategy

Discover provisionally, generate, compile/link, execute, inspect the actual blocker, implement a narrow runtime fix or checked patch, and repeat. Manual reverse engineering follows failing paths. Since 2026-10-18 the [whole-game coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md) replaces finding blockers one run at a time: the map is closed statically, what the game asks of the runtime is listed from its code, and game states are started directly. Existing entry/control audits validate tooling; exhaustive manual boundary approval is not a prerequisite for experimental local generation.

## Reached IOP boot boundary (2026-10-04)

Before unfamiliar subsystem/architecture work, consult this review and its linked sources first. Verify every adapted pattern against BT2; keep game-specific behavior separate.

The pinned [BT3 IOP notes](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/tools/iop/README.md) describe separate R3000 module translation, import providers, loader, IOP memory/scheduling and RPC routing. Treat those as architectural guidance; reported module behavior and permissive fallback returns are not BT2 evidence. The inspected README SHA-256 is `59da0bf2c59bfb017bca594c46274b02066fe38a126848cbfca8d27baadc6414`.

BT2 now requests `rom0:UDNL cdrom0:\IRX\IOPRP300.IMG;1` in command 0x80000003. Next validate that actual local image and collect disc-backed boot evidence using isolated reference settings, following Chronicle's user-local disc workflow. Establish a new IOP boot/service lifetime; neither a BT3 inventory nor BT2's earlier standalone CD snapshot establishes reset completion. General IOP scheduling/recompilation remains a separate implementation decision after this evidence.

### Local image validation and submission evidence

The pinned [BT3 IOPRP tooling](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/tools/iop/ioprp.py) supplied ROMDIR inventory guidance. Our authored validator derives offsets from all directory entries and checks metadata/ISO bounds, rather than locating the first ELF as an offset shortcut. BT2's local image matches the disc and contains 16 ELF payloads (inventory/hashes remain ignored). This establishes identity and header metadata, not successful loading/import resolution.

Five independent disc-backed EE checkpoints establish submission acknowledgement and ordered register clears. A new pending IOP lifetime invalidates old service state. Fresh post-reset IOP evidence is still needed before readiness, loader/import providers, scheduling or service routing can be implemented. No reference game implementation was copied.

### Reached GS setup boundary

The pinned GPU interface was consulted again for the GS boundary (SHA-256 `2e70e2cdaad179218e55cd365b54c2308cf6ce46dafd5d9fe9d820f44cd741aa`). It separates command recording from presentation. BT2's next mapped helper includes GS register access at 0x12001000; [the scoped plan](../plans/graphics/GS_BRINGUP_PLAN.md) starts with actual register evidence before rendering integration. No renderer implementation copied.

### GS setup and new interrupt boundary

Consulted pinned PCSX2 GS.cpp for CSR reset behavior and Hw.h for register addresses, and pinned Play PS2OS.cpp for GS syscall ABI. Source hashes/provenance are in config/startup_abi.sources.json. Local captures determine returned values; Play's void-style handlers do not establish BT2 BIOS return values. The authored adapter records only exact reached requests with explicit reference readback checkpoints, without copying reset/rendering implementations.

The next stop is INTC_STAT acknowledgement followed by polling and a later CSR field read. An initial working hypothesis incorrectly labeled 0x1000F000 as DMA control; the pinned hardware map resolves it as INTC_STAT (DMAC_CTRL is 0x1000E000). No runtime DMA behavior was added from that hypothesis. Follow GS_INTERRUPT_TIMING_PLAN.md before introducing event sources or synchronization.

### Interrupt ordering and GIF DMA boundary

Pinned [PCSX2 HwWrite.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/HwWrite.cpp) establishes status acknowledgement; [Hw.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Hw.cpp) separates status latching from mask-gated interrupt delivery. [Counters.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Counters.cpp) separates Vblank start, later GS field update, and Vblank end. Pinned PS2SDK kernel.h identifies Vblank-start source 2, hence bit 4. Hashes are in config/startup_abi.sources.json. The authored diagnostic sequence uses validated BT2 samples without copying these implementations or claiming cycle accuracy.

The reached GIF DMA channel read now needs [its separate plan](../plans/graphics/GIF_DMA_BRINGUP_PLAN.md). Use BT3's recording/presentation separation as architectural guidance; validate actual BT2 transfers before acknowledging or decoding them.

### First GIF request staging and drawing packet

Consulted pinned [Dmac.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Dmac.cpp) for channel programming/enable gating, [Gif.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Gif.cpp) for normal transfers, and [Gif_Unit.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Gif_Unit.h) for tag layout. [GSRegs.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSRegs.h) identifies XYZ2, sprite primitive and the three single-bit control registers. Source hashes/roles are recorded in config/startup_abi.sources.json. No implementations copied.

The first reached BT2 packet includes drawing, so channel staging alone cannot justify transfer completion. The [renderer plan](../plans/graphics/GS_RENDERER_BRINGUP_PLAN.md) follows BT3's separation of ordered command consumption and presentation, scoped initially to observed BT2 behavior.

### Scoped GS software command consumption

Consulted pinned [GSLocalMemory.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSLocalMemory.h) and [GSTables.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSTables.cpp) for 64 by 32 PSMCT32/PSMZ24 pages, 8 by 8 blocks, pixel bit layout and depth block XOR 0x18. The authored sink expresses addressing through bit placement, preserves unknown VRAM bits, and consumes validated packets transactionally. Public register definitions establish ZBUF PSM field 1 as PSMZ24 and TEST ZTST 1 as ALWAYS; ZMSK is bit 32, so both BT2 clears write depth. Source hashes are in config/startup_abi.sources.json. No implementation copied.

The first DMA synchronization is backed by separate before/poll/after, VU-idle and helper-return captures; the second same-form source packet is captured at the latter return. Completion remains a bounded logical diagnostic, independent of physical DMA/video timing. Main startup next requests a new [post-reset SIF transport lifetime](../plans/iop/POST_RESET_SIF_PLAN.md); old channel/service evidence must not be reused automatically.

Pinned [GSDrawingEnvironment.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSDrawingEnvironment.cpp) and [GSDrawingContext.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSDrawingContext.cpp) were also consulted: drawing reset establishes SCANMSK and FBA defaults. The reached CSR reset explicitly initializes those scoped sink defaults while preserving unknown VRAM; full GS reset/presentation effects remain incomplete.

### Fresh SIF transport and IOP service lifetime

Consulted pinned PS2SDK [EE sifcmd.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/src/sifcmd.c) and [IOP sifcmd.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sifcmd/src/sifcmd.c) for INIT_CMD opt 0/1 and command ownership. Local BT2 before/after captures establish fresh buffer addresses and SET_SREG index 0/value 1. Source hashes/roles are in config/startup_abi.sources.json. No implementation copied.

The separate diagnostic adapter completes transport initialization without executing an IOP boot or validating RPC services. BT3's native IOP module/import/scheduler separation guides the [new service ownership scope](../plans/iop/POST_RESET_IOP_SERVICES_PLAN.md). Live IOP service records are evidence to validate against a fresh reply, not a reason to reuse old pointers.

### Post-reset service replies and loader boundary

Pinned public PS2SDK sifrpc.c/sifrpc-common.h establish synchronous bind/call headers, packet freeing, client pointers and semaphore signaling. BT2 captures independently validate a null CD bind, later successful bind, CD initialization, SCMD bind and mode-2 return. The authored epoch-owned adapter executes the generated EE RPC_END callback and keeps prior service state invalid; it does not execute native IOP service instructions or reproduce physical retry counts.

Pinned [loadfile.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/src/loadfile.c) identifies the reached service 0x80000006 as LOADFILE. BT3's distinction between module mapping, relocation/import resolution, entry execution and scheduling guides [the loader plan](../plans/iop/LOADFILE_IOP_MODULE_PLAN.md). Confirm our toolchain's actual capabilities and BT2 requests before implementation. Source hash is recorded in config/startup_abi.sources.json; no implementation copied.

### LOADFILE and actual pinned IOP capability

Inspected pinned BT3 [IRX discovery](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/tools/iop/irx_functions.py), SHA-256 `83465ff5c1a6e167fa5e6fe8ef485784ad09f034c05e026e3114c70f5d41e1c0`, and pinned PS2SDK [irx.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/kernel/include/irx.h), SHA-256 `2afc986bdf65970f599f635ed3c5c73353f8ed6a83f2ca186c285c927bdddff7`, for table layout, ordinal stubs and map seeds. The authored inspector checks bounds/stub encodings and preserves code between inline tables; it does not assume that the first table ends all executable code. No implementation copied.

Audited our pinned PS2Recomp `RecompilerConfig` in `types.h` (SHA-256 `f33027cc08de16084ef27ca1b2593e4736a0734031cd619b62e4981a774041d3`) and `function_emitter.cpp` (`a1e3267a8f81d032a29ab7a2d260b2f501425b6bbcb4d244c3315be6d5e34c34`). No R3000 architecture selector/import dispatcher is present; emission uses the EE context. BT3's added IOP backend therefore needs a [separate native execution scope](../plans/iop/IOP_NATIVE_EXECUTION_PLAN.md).

Local SIO2MAN metadata and private relocation preparation are compared against reference loaded-module ownership described by public [loadcore.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/loadcore/include/loadcore.h). Matching relocated code does not establish entry execution, library registration or native scheduling.

### Isolated native IOP backend and first library

Reviewed pinned BT3 instruction_translator.cpp, function_emitter.cpp and ps2xRuntime/src/lib/ps2_iop_module.cpp: its R3000 guards/import dispatch and separate module lifetime informed the authored backend. Its context coupling and permissive import defaults were not adopted. The pinned EE dependency remains unchanged.

Public PS2SDK [modload.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/modload/src/modload.c) establishes argc including the module path, argv termination and GP initialization. [loadcore.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/loadcore/src/loadcore.c) and irx.h establish export registration/layout and linkage mutation. The authored provider supports only validated first registration in an owned lifetime; no copied implementation or automatic successful imports.

Pinned PCSX2 [IOP writes](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/ps2/Iop/IopHwWrite.cpp), [SioTypes.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/SIO/SioTypes.h) and [Sio2.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/SIO/Sio2.cpp) distinguish control reset value 0x3BC from START_TRANSFER bit 0 and IRQ effects. They establish the next hardware scope; neither transfer completion nor always-ready behavior follows from reset alone. Synthetic execution verifies scoped CPU semantics, not physical timing equivalence.

### Reached SIO2MAN kernel startup

Consulted pinned public PS2SDK intrman.h/intrman.c, dmacman.h/dmacman.c, thbase.h/thbase.c, thevent.h/thevent.c, thsemap.h and threadman thcommon.c/include/thcommon.h at ac92a9f657d2e531dd8f060250b07f2a5ac6dea5. Source paths are under iop/system/. Parameter layouts, scoped DMA priority/enable, enabled kernel startup, retained GP/stack and event mask/OR/CLEAR behavior guide authored providers. No source implementation was copied. The observed retail kernel record layout differs from SDK thcommon.h; the verifier labels its scenario-specific offsets and checks object tag/ID, state, module entry/GP and wait ownership before comparison. Opaque options are not interpreted. See [SIO2 scope](../plans/input/SIO2_BRINGUP_PLAN.md) and [persistent EE bridge](../plans/iop/IOP_LOADFILE_BRIDGE_PLAN.md).

### Persistent native LOADFILE and MCMAN imports

The authored shared service follows the previously reviewed BT3 separation of compiled module dispatch and owned runtime lifetime. Consulted pinned PS2SDK sysclib.h (memset ordinal 14), secrman.h (void callback setters 4/5), modload.h/modload.c (void SetCheckKelfPathCallback ordinal 13), and ioman.h/ioman.c (driver init/deinit lifecycle) at ac92a9f657d2e531dd8f060250b07f2a5ac6dea5. BT3's ordinal-13 description does not establish BT2 behavior; the public SDK identifies a callback setter rather than module loading. Only reached callbacks are retained, not invoked or counted as executed. See [IOMAN plan](../plans/iop/IOP_IOMAN_DRIVER_PLAN.md) for links and adaptation limits.

### IOMAN lifecycle and MCMAN removable resident result

Adapted public pinned PS2SDK ioman.c/ioman.h: 16-slot driver registry, first-match deletion, native init/deinit callbacks and removal after negative init. BT3's permissive IOMAN/security defaults were not adopted. Public modload.c retains entry results with low bits 0 or 2 and reports the actual entry result; reached MCMAN returns 2. Scoped runtime accepts only the observed exact 0/2 values. MCMAN normalized local return/code/data/BSS comparison is separate from physical timing/device equivalence.

Pinned [sifman.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sifman/include/sifman.h) identifies sceSifCheckInit ordinal 29. Existing pinned IOP sifcmd.c startup checks SIFMAN before command/RPC setup. This informs the separate [native SIF/RPC server plan](../plans/iop/IOP_SIF_RPC_SERVER_PLAN.md); EE initialization snapshots do not justify automatic native IOP initialized flags.

### Bounded native RPC startup and reached Vblank wait

Consulted pinned PS2SDK sifman.c/sifman.h, IOP sifrpc.c/sifcmd.h and common sifrpc-common.h for initialized-flag queries, idempotent RPC init, 24-byte queue / 68-byte server descriptors, list linkage and empty-queue sleep. Authored compatibility providers attach to the validated fresh EE epoch and retain actual compiled worker contexts. MCSERV/DBCMAN startup is compared against separate local unique-caller captures; no physical SIF transfer or successful client request is implied.

Pinned VBLANK ordinal 5 is WaitVblankEnd (not handler registration). vblank.c implements an OR wait on an end-event flag. The reached SIO2D worker requires an owned event/phase producer; no immediate-success result is justified. See [Vblank timing scope](../plans/iop/IOP_VBLANK_PLAN.md).

### Native exports, event wake and SIO2 transfer scope

Consulted pinned SDK thsemap.c/thsemap.h (WaitSema ordinal 8 consumes a positive count), sysclib.h (memcpy ordinal 12), thevent.c/thevent.h (SetEventFlag 6, ClearEventFlag 8, FIFO satisfaction, result publication, CLEAR and ready dispatch), and VBLANK source for absent-end waiting. Adapted scoped owned state and compiled export dispatch with retained registration GP; no default success or source implementation copied. Pinned PCSX2 Sio2.cpp and IopHwWrite.cpp establish retained control and command/port register writes, and the reached byte FIFO boundary. No controller replies, timing or interrupt completion are inferred from those configuration writes.

### Scoped first controller polls and cross-module callbacks

Consulted pinned PCSX2 Sio2.cpp, SioTypes.h, IopHwWrite.cpp and PadDualshock2.cpp for queued packet boundaries, control START/IRQ behavior, status and digital poll response. Direct local first-five/first-ten FIFO captures independently validate BT2's actual initial polls and status. The authored model supports only those observed initial profiles. Native IRQ execution, checked cross-module callback dispatch, owned ReferThreadStatus (PS2SDK thbase.h/thbase.c ordinal 22) and WaitVblankStart (vblank ordinal 4) adapt the previously consulted module/scheduler structure. No implementation copied or later controller/audio behavior inferred.


### Reached sound-module registration

Consulted pinned PS2SDK [FREESD startup](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/sound/libsd/src/freesd.c) for bus/DPCR setup guidance and [sysmem header](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sysmem/include/sysmem.h)/[implementation](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sysmem/src/sysmem.c) for Kprintf ordinal 14 and zero return without an installed callback. The authored adapter retains reached literal diagnostics; formatted calls remain a checked boundary. Retail LIBSD bus writes and baseline DPCR values were established separately from reached native operations and local register captures. All four sound-module resident checkpoints were compared to the supplied game's local PCSX2 execution. No voice engine, synthesis, physical DMA, playback or successful sound RPC processing is inferred from library registration.


### Disc-streaming resident and actual sound initialization

Pinned SDK [CDVDMAN service commands](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/cdvd/cdvdman/src/cdvdman.c) and [CDVDSTM startup](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/cdvd/cdvdstm/src/cdvdstm.c) establish SC(-11)'s interrupt-event-handle query. Local pre/post checkpoints establish retail event attributes/options/bits and resident result; no default successful sectors or event transitions inferred. SOUNDS' actual stack-owned worker/RPC descriptors are independently compared to local reference evidence.

Consulted pinned [stdio.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/stdio/src/stdio.c), [loadcore.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/loadcore/include/loadcore.h) and [LIBSD exports](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/sound/libsd/src/exports.tab) for reached printf/count behavior, void FlushDcache and sceSdInit ordinal 4. The authored native adapter has coherent owned RAM and bounded observed formatting, with unsupported cases rejected. Reached SPU2 MMIO is the next separate hardware scope; no public/reference audio implementation copied.


### Reached SPU2 reset and interrupt teardown

Consulted pinned SDK [SPU2 register definitions](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/sound/libsd/include/spu2regs.h), [FREESD](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/sound/libsd/src/freesd.c), [intrman](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/intrman/src/intrman.c), and [kernel errors](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/kernel/include/kerr.h). DisableIntr publishes KE_INTRDISABLE (-103) when already disabled; ReleaseIntrHandler returns KE_NOTFOUND_HANDLER (-105) when absent. Providers retain owned state and reject unhandled IRQs. The reached bzero ordinal 17 clears checked owned RAM and preserves V0 for its void API.

Pinned PCSX2 [register table](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/SPU2/RegTable.cpp) and [SPU2 handling](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/SPU2/spu2sys.cpp) establish retained SPDIF/configuration, ATTR/status context and key-off behavior. Key-off's first halfword is the low mask; SDK HI/LO macro names here are misleading and were checked against PCSX2 storage and reached retail writes. The adapter retains pending release masks without claiming envelopes/sample processing. Effects and transfer start addresses retain their written halves without transferring sound RAM.

Guarded isolated local captures independently publish the first actual status LHU and three linked DisableIntr calls. Exact reset configuration and the two observed ordered status polls bound the native zero status values. No transfer-ready status, arbitrary reset values, successful CRI residency or audio playback is inferred. [Initialization scope](../plans/sound/SPU2_INITIALIZATION_PLAN.md) ends at the first sound-data FIFO write; sound RAM transactions need separate completion evidence.


### Bounded LIBSD initialization and CRI residency

Consulted pinned PS2SDK FREESD/SPU2 definitions, intrman/thbase/sysclib contracts and PCSX2 SPU2 register, IOP hardware read and DMA handling. Adapted only storage/address progression, owned interrupt state and cooperative lower-priority worker startup; guarded BT2 captures establish reached values and final effects. No implementation copied. Initial FIFO transfer, LIBSD return 0 and CRI resident result 2 are independently compared; general sound DMA, envelopes, effects and playback remain unimplemented. The next generated call binds server 0x80000003 and requires the separate IOP_HEAP_STARTUP_PLAN.md service/protocol evidence.


### IOP heap service correction and bind evidence

Pinned PS2SDK [IOP FILEIO source](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/fs/fileio/src/fileio.c) registers the heap service as 0x80000003; ordinary file I/O is 0x80000001. The previous server classification was incorrect. Local IOPRP FILEIO code relocation, module ownership, tagged sleeping worker, service queue and unique EE bind return establish this specific BT2 service. An authored reference-backed bind adapter executes the already generated RPC_END callback; it does not recompile/execute FILEIO startup or grant successful heap/file requests. Adapted the existing checked RPC path without copying SDK/reference code. Follow IOP_HEAP_STARTUP_PLAN.md for reached requests.


### First heap allocation and owned transfer

Pinned FILEIO heap handler and [sysmem allocation](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sysmem/src/sysmem.c) establish allocation mode 0 and 256-byte rounding. Local captures establish BT2's first size 864, returned block 0xCE500, RPC result and exact subsequent copy. The authored generator extracts only user-local disc-matched FILEIO and reuses our translator; its real handler executes against a scoped owned allocation provider. No public implementation copied. The worker/boot entry is not run or counted as another native resident. First copy is synchronous logical transport with its own token, independently compared against reference bytes; hardware DMA timing and general allocation are not claimed. The copied payload is an embedded IRX, now reaching the distinct module-buffer loader boundary.


### Embedded entry, nonresident cleanup and global DMAC setup

Consulted pinned PS2SDK LOADFILE operation-6 layout, modload start/result cleanup, FILEIO operation-2 handler and CDVDMAN SC command -9. Adapted existing checked RPC/native generation and ownership machinery; no public implementation copied. The local `ck01` entry compares the queried version with 1: equality returns 5, inequality returns 1. Disc-matched IOPRP CDVDMAN metadata is 0x225; a retail unique caller capture returns module result 1. SDK modload result & 3 == 1 unlinks/frees, so native ownership is released and resident count stays thirteen. The translated FILEIO handler then calls checked FreeSysMemory for the separately owned source block.

Pinned PCSX2 `pcsx2/Dmac.cpp` confirms CTRL register storage and STAT low-half clear-on-one / upper-half toggle-on-one semantics. Reached global initialization writes CTRL 1, PCR 0x83FF0002 and STAT 0; zero STAT preserves the existing interrupt mask. A unique retail caller-return capture validates this path; no channel start, completion event or hardware DMA scheduler is inferred. The next routine reaches VIF1 reset and FIFO commands; follow VIF_STARTUP_PLAN.md before adding those effects. Existing BT3 VU interfaces are architectural references, not evidence that BT2's packets or microprograms match.

### Bounded VIF configuration and second GS setup

Consulted BT3's pinned VIF decoder/VU state interfaces and PCSX2 [VIF command handling](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Vif_Codes.cpp), [VIF reset](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Vif.cpp), [VU control](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/VUmicro.cpp) and [GIF handling](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Gif.cpp). Adapted configuration state and streaming command/payload separation. BASE stores the base; OFFSET clears DBF and establishes TOPS from BASE. STMASK consumes its next word, including zero, rather than interpreting it as another opcode. The prepared VU control hook preserves reset pulses before the generated persistent-bit mask discards them. No source implementation copied, VU program executed or generic DMA scheduler supplied.

BT2's unique return capture independently matches the reached VIF register configuration. Actual second CSR load and graph caller return captures establish repeated setup; CSR reset clears scoped drawing state while retaining VRAM. Reading 0x12001010 with the diagnostic LD produced a CSR-like value and is not evidence for IMR; native IMR remains bounded by the reached syscall arguments and prior verified contract. The next packet's PABE/TEX0 configuration requires the separate GS_TEXTURE_STARTUP_PLAN.md decoder/state scope.

### Third GIF packet, EE heap and IOP layout conflict

Consulted the existing pinned BT3 renderer/GS state interfaces and PCSX2 [GS register definitions](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSRegs.h), [GS handlers](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSState.cpp) and counter mode handling. Adapted register-state/command-consumption separation. PABE retains bit 0; TEX0 is retained per context only when CLD is zero, with no draw, upload or CLUT inference. Unsupported effects reject transactionally. Normal GIF staging consumes checked source bytes before logical completion; original idle/busy/clear-STR loads and caller return are captured separately. No timing scheduler or public implementation copied.

Pinned PS2SDK kernel.h declares EndOfHeap (0x3E); previously consulted Play! SetupHeap/EndOfHeap returns the current thread's initialized heap boundary. The authored main-thread query reuses existing owned bounds rather than returning a guessed pointer. Generated EE allocator routines execute, and independent retail pool pointers match. A structural map filter admits only isolated zero padding after unconditional BEQ and a non-control delay slot; this validates a discovery pattern, not function purpose or all control-flow boundaries.

The next retail IOP allocation reuses 0xCE500 for 0x81800 bytes. Its interval overlaps our translated FILEIO helper, even though the original retail FILEIO at 0x44430 does not overlap it. This is an authored layout constraint, not evidence of broken retail logic. IOP_HEAP_CAPACITY_PLAN.md requires an ownership inventory and consistent translation placement before enabling the new request. The initial scoped heap provider stays unchanged.


### Second heap capacity and native layout

Consulted the pinned BT3 IOP notes/module ownership model and local pinned public PS2SDK sysmem/FILEIO contracts. Native translation addresses are authored placements, while EE-visible heap pointers retain captured BT2 values. Five overlapping images were regenerated into disjoint owned ranges rather than overwriting code or remapping the retail heap result. Relocated resident startup state still passes independent normalized comparisons. Two captured allocations reuse the released block; arbitrary allocations remain unsupported. The next SDRDRV bind needs owned native server/worker request delivery, as described in the reference's separate RPC routing model; registration alone is not command execution.


### SDRDRV request and EE thread-start evidence

Consulted the already pinned PS2SDK SIFRPC source for request metadata, handler invocation and retained queue-tail semantics. The owned native worker context/GP/buffer pattern follows the pinned BT3 IOP notes without copying implementation. Public kernel thread status constants distinguish RUNNING (1), READY (2), WAIT (4) and DORMANT (0x10); retail running and native ready are not asserted equivalent.

The official pinned [PS2SDK DECI2 source](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/src/deci2.c) identifies kputs as subfunction 16 with a pointer in the first argument word. Local capture establishes this BIOS return separately. Console logging does not establish disc-file reads. The valid native path stops at the new [EE thread-start scope](../plans/ee/EE_THREAD_START_PLAN.md), before its unvalidated Status/context transition.


### EE sound thread readiness and SOUNDS command

Consulted the pinned BT3 runtime scheduler/fiber interfaces for ownership and explicit blocking/dispatch boundaries, and the pinned Play StartThread/ReferThreadStatus behavior plus PS2SDK kernel.h for the public 48-byte descriptor. Adapted only owned state and lower-priority ready transition, not thread/fiber implementations. Independent ID-3 query, caller Status and saved-context captures establish this BT2 scope. The child's exit is the supplied SetupThread handler, not the reference BIOS constant used by the older dormant bootstrap profile. IE/EIE handoff is explicitly capture-backed; continuous interrupt timing is unimplemented.

Native SOUNDS command 0x10 reuses the checked registered-worker request contract and executes actual translated code. Its result/input/metadata match independent captures; no public handler implementation copied. The next DBCMAN service has an owned server but unknown EE command effects. Its request group requires [separate protocol evidence](../plans/input/DBCMAN_RPC_PLAN.md); service IDs do not establish subsystem capabilities.

### DBCMAN request group and reverse SIF DMA

Reused the previously reviewed pinned SIFRPC contracts and BT3 owned worker/GP/stack architecture for four binds and real handler delivery. Public PS2SDK [sifman.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sifman/include/sifman.h) identifies ordinal 7 as sceSifSetDma, ordinal 8 as sceSifDmaStat and the four-word transfer descriptor. Pinned sifman.c distinguishes submission tokens from completion/poll state. No implementation copied. BT2's second command reaches a 128-byte IOP-to-EE transfer; existing EE-to-IOP boot copy and live retail return snapshots do not validate this reverse transfer. Follow DBCMAN_DMA_PLAN.md before completing it.

The initial reverse SIF transfer now follows the pinned descriptor/token/poll contracts with exact BT2 caller and buffer gates. Retain ownership of native source bytes and a submission-time diagnostic record rather than equating mutable device buffers across captures. Retail verification explicitly freezes and redirects a diagnostic source before the real transfer; native completion is logical, not hardware scheduling equivalence. Both subsequent captured buffer-registration requests execute actual native handlers. MCSERV protocol work remains a separate scope.

### Bounded MCSERV startup

Reviewed the local pinned BT3 IOP notes for cross-space request delivery on the actual registered worker GP/stack, and pinned [PS2SDK libmc.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/rpc/memorycard/src/libmc.c) for service 0x80000400, initialization command 0xFE and the 12-byte version result. These identify public protocol structure; they do not establish BT2's buffers, return values or card state. Independent BT2 captures establish those. Deliver the real handler through the existing owned RPC transport and preserve generated cleanup; no public implementation copied, no virtual card filesystem supplied.

### MCSERV-to-SIO2 preemption and initial slice programming

Reused the reviewed PS2SDK event semantics and BT3 owned worker/import separation for bounded priority-16 SIO2 preemption from the priority-104 MCSERV handler. Preserve the calling thread on both yield and exception; unrelated nested wake remains rejected. Pinned [dmacman.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/dmacman/src/dmacman.c) and dmacman.h distinguish ordinal-28 slice programming/return 1 from ordinal-32 START. Direct BT2 retail API arguments, first canonical command source and physical register readbacks independently validate the native configuration. No implementation copied and no transferred bytes, device reply, IRQ or completion inferred. The actual card transport requires SIO2_CARD_DMA_PLAN.md.

### Absent-card DMA transport and timed IOP boundary

Consulted pinned PCSX2 [IopDma.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/IopDma.cpp), locally pinned Sio2.cpp/SioTypes.h and PS2SDK dmacman API contracts. Adapted words-to-bytes sizing, retained channel state and separate START/completion; no implementation copied. Explicitly disabled local card slots establish five probe packets, one F3 fallback and six FF bytes/zero padding. Full observer preserves actual control writes/readbacks and real API calls. Native uses bounded logical DMA/IRQ delivery; hardware timing and whole-system control counts are not equated. MCMAN now reaches a real 300-microsecond DelayThread; the active borrowed RPC context and ready sound helper require [timed scheduling design](../plans/iop/IOP_TIMED_WAIT_PLAN.md).

### Bounded timed query dispatch (2026-10-05)

The pinned BT3 IOP notes and public PS2SDK thread/RPC contracts informed ownership and cooperative dispatch. BT2 retains the live borrowed RPC/export CPU frames on the host stack while separately retained ready worker contexts run by priority. Independent local delay, clock/alarm, FIFO and absent-card callback captures verify reached behavior. The outgoing SDRDRV EE bind is retained as pending because the native EE server is not registered; the diagnostic retail bind result is not substituted. The current scope does not claim general EE/IOP scheduling or physical timing. See [timed IOP scope](../plans/iop/IOP_TIMED_WAIT_PLAN.md).

The later generated diagnostic formatter reaches EE PSUBW. [PCSX2's MMI interpreter](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/MMI.cpp#L290) confirms four independent word subtractions; the authored ABI helper uses unsigned 32-bit lane arithmetic and tests wraparound/no cross-lane borrow. No reference implementation was copied.

### First disc-search service boundary (2026-10-06)

Pinned PS2SDK [EE libcdvd.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/rpc/cdvd/src/libcdvd.c) identifies 0x80000597 as the CDVDFSV file-search service. This identifies the public protocol, not a completed native file lookup. Reached generated BT2 code uses client 0x3A8780 and RPC ID 42. Independent absent-card retail captures publish initialization, actual bind semaphore creation and bind return. The generated bind helper initializes max/init/option, leaving attr and other output fields as stack residue; the scoped native semaphore profile validates ownership/caller and those actual inputs rather than equating undefined fields. No reference implementation copied. Disc-search request delivery, results, reverse transfer and subsequent sector reads require a separate compatibility design; earlier CD initialization and CDVDSTM residency do not supply them.

### First owned disc metadata request

The pinned BT3 boot-service/translated-handler pattern informs the CDVDFSV adapter. Every relocated code range, the captured server/handler and retained GP match the local disc's module. Public PS2SDK CDVDMAN SC(-10) acquires/clears a file-search event bit for a nonzero parameter and releases it for zero, returning that parameter. The adapter models one serialized owned request and restores earlier providers afterward; it does not execute the boot entry or register a resident CDVDFSV worker. No reference implementation copied.

Original BT2/CDVDFSV code selects layer-zero search ordinal 84 from its 300-byte request. Its accepted SIF submission sends 36 bytes; on this path it does not call sceSifDmaStat. Independent submission and EE-return captures verify the metadata and result separately. The host supplies bounded ISO9660 metadata only. Unused filename padding differs legitimately between fresh native RAM and retail residue. Sector data, repeated search and asynchronous disc timing remain unsupported.


### Reached EE indexed cache scans and CRI child-start boundary

Reviewed pinned BT3 coherent native-memory/cache configuration and the pinned PCSX2 CACHE implementation linked in EE_CACHE_TRANSFER_PLAN.md. Adapted explicit invalid logical tags and checked indexed ordering to owned coherent RAM; original BT2 translated branches remain intact. Two independent full scans publish both ways and caller returns. PCSX2's flags-only TagLo behavior cannot validate physical PS2 tag addresses or cache timing. No reference implementation was copied.

Original CRI 0x40C now exposes a priority-39 child start from its priority-48 borrowed RPC worker. Existing bounded MCSERV/SIO2 event preemption informs parent ownership, but does not establish the new child-start lifecycle or first wait. CRI_THREAD_START_PLAN.md records the evidence and required scheduling design; the runtime explicitly rejects this transition.


## CRI child start and AutoDMA handoff (2026-10-06)

Consulted the pinned BT3 cooperative IOP notes, public PS2SDK StartThread/WaitSema ownership, intrman.c/intrman.h and kerr.h for nested suspend state and KE_CPUDI (-102). Adapted the existing bounded retained-parent event-preemption pattern to the observed CRI child start. Independent original StartThread/WaitSema return captures verify the parent/child handoff. Pinned PCSX2 RegTable maps ADMAS to retained AutoDMACtrl; Dma/ReadInput identify a separate AutoDMA ownership path. The initial CHCR read and requested ADMAS write are captured independently. No reference implementation was copied; active transfer/playback requires CRI_AUTODMA_PLAN.md.


## First CRI AutoDMA transaction and constructor batch (2026-10-06)

Applied the previously inspected pinned PCSX2 DMA/ReadInput and PS2SDK block-transfer ownership patterns to independent BT2 register/API observations. Implemented only accepted initial-half programming and immutable pending ownership; no reference code copied or completion inferred. Original CRI constructs sixteen object pairs. The fifth heap request now exposes broader address-space capacity work; see CRI_SECOND_CHANNEL_PLAN.md before relocating further images.


## Second CRI channel ownership layout (2026-10-06)

Reconsulted the retained pinned BT3 native IOP notes (per-image placements, translated imports and owned RPC delivery) and pinned PS2SDK sysmem.c (256-byte rounding). Adapted placement into disjoint authored gaps, preserving captured BT2 heap results and 2 MiB bounds. The native diagnostic kernel uses host providers; it does not load the retail low-memory kernel into those gaps. Both relocated resident images pass normalized original startup comparisons. No reference implementation copied. Second-channel inputs/results come from independent local BT2 captures, not BT3.

## Retained EE execution and reverse-bind diagnostic (2026-10-06)

Reconsulted the pinned BT3 runtime's fiber/guest-wait interfaces (single host thread, explicit park/dispatch and owned thread lifetime) and the local pinned PS2SDK IOP sifrpc.c RPC_END/client/packet ownership and thbase SleepThread ordinal 24. Windows [CreateFiberEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfiberex) and [ConvertThreadToFiberEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-convertthreadtofiberex) provide the host stack mechanism. Authored frame retention, cancellation/unwind, failure propagation and per-dispatch return ownership are independently tested; no reference implementation copied.

BT2 captures establish actual registration, blocking reverse-bind return/client and subsequent first sleep. They also show the sound child runs during the earlier SOUNDS bind. Our late probe validates generated code and retained stacks; enabling it as the normal scheduler would assume an ordering contradicted by the original. Queued EE/IOP completion and earlier dispatch need the separate EE_RPC_SCHEDULING_PLAN.md design.

## Queued sound ownership checkpoint

The adapted runtime now retains a generated EE sound invocation across its real SleepThread and completes the queued original RPC at the earlier main bind wait. This is a bounded verified ownership path, not a general scheduler. Reference patterns guide the architecture; BT2 registrations, request identity and captured waits define admission. Helper priority transitions still require implementation. Distinguish EE RAM flag 0x34BC84 from GS BGCOLOR 0x120000E0; no CSR read was observed on that helper path.

## SPU2 position and VSync flag evidence (2026-10-07)

Consulted pinned PCSX2 [Mixer.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/SPU2/Mixer.cpp) alongside the existing register/KON references: eight halfwords per ADPCM block, 28 source samples, 4.12 pitch and end/repeat/loop-start flags. The authored model advances positions and ENDX on logical IOP timer intervals; it does not decode audio or emulate ADSR. Missing sound RAM or parameters stay checked. The consulted Mixer source SHA-256 is `94a43b48c65cd724661ffa29668428d00041bc7e55fd81934315a9dd82220791`.

The pinned Play! [`SetVSyncFlag` / `CheckVBlankFlag` implementation](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ee/PS2OS.cpp) and PS2SDK declaration establish one-shot flag/CSR pointer registration. Three local original captures around the first frame's wait confirm preserved V0 on registration and the subsequent stack publication. Native delivery shares the existing logical VBlank handler/worker path; physical timing and FIELD phase are excluded from comparison.
