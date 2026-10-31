# PC port: working notes

This repository is the PC port. It was cloned from the matching decompilation at tag
`port-base-2026-10-05`; the decompilation stays a separate repository and is the remote
`decomp` here. `git fetch decomp && git merge decomp/master` brings in later matching work.

## Rules of the layout

- `src/`, `include/`, `config/`, `scripts/`: the decompilation's files. Do not edit them here.
  When a game source needs a change to build on PC, make it in the decompilation behind
  `#ifdef PORT`, check there that the matching build is still byte-identical, then merge.
- `port/`: everything that exists only for the PC build (compatibility header, platform
  layer, tools, build files). `port/build/` is output and is ignored.
- `port/include/port_compat.h` is forced into every game source (`-include`). It defines
  `PORT`, takes over the base types (`long` is 64-bit on the PS2, so `s64 / u64` become
  `long long`) and turns the assembly-inclusion macros into nothing.

## State on 2026-10-05 (first day)

Verified by running the tools below:
- All 204 game sources of the main executable and all 69 menu-overlay sources pass the host
  compiler's front end as 32-bit code (`port/tools/survey.py`). Three `#ifdef PORT` guards
  were needed: the 128-bit typedefs (include/sys/dma.h), one 128-bit store (Dma_BeginDirect),
  and five cast macros that are assigned to.
- 196 of 204 compile to objects (`port/tools/undefined.py`). The eight that do not contain
  MIPS inline assembly: dma.c, file.c, mathf.c, randf.c, vu0_a_c.c, vu0_a_c_b.c, vu0_a_c_c.c,
  vu0_b_c.c (the vector library has exact C references in src/port/).
- What a link of those 196 objects still needs (list in port/build/undefined.txt):
  423 data symbols (data the decompilation still keeps in assembly chunks: to be extracted
  from the original executable by a tool, the user's own copy, not committed), 179 game
  functions (the INCLUDE_ASM functions whose C sits in `#if 0`, the hand-written assembly
  routines, and the functions of the eight files above), 69 library functions (Sony SDK,
  CRI sound, libc: the platform layer), 12 others (menu-overlay entry points and host
  compiler helpers).

## Decisions so far

- 32-bit host build first (`-m32`): the game stores pointers in 32-bit fields everywhere.
  A 64-bit build (needed for Android) is a later clean-up, not a blocker for validation.
- First milestone (docs/roadmap.md stage 2): the fight simulation headless, fed by a replay
  recorded on the original, compared frame by frame.

## State at the end of 2026-10-05

Tools (run from the repository root, in this order): `port/tools/gen_data.py` (the game's
remaining assembly data as host assembly, from the decompilation's generated asm/),
then `port/tools/undefined.py` (compiles everything to port/build/obj and lists what a link
still needs in port/build/undefined.txt). `port/tools/portsrc.py` is their shared module: it
lists the sources of the PC build and switches on the C of functions that the matching build
still takes from assembly.

Verified by running them: 202 of 204 game sources of the main executable build to 32-bit
objects (the four vu0 files are replaced by src/port/vu0_a.c + vu0_b.c compiled under the
game's names through port/src/vu0_names.h). Still failing: mathf.c and randf.c (COP1 / VU0
assembly: need PC versions on the PS2 float model, including the VU0 random register).
A link still needs: 21 data symbols (VU1 microprograms and a few tables), 39 game functions
(mathf, randf, three StgVu_Rotate routines, ObjSeam_TransformVtx, the entry point, and
library names in the game range), about 190 library functions (Sony SDK, CRI, libc; many are
referenced only by library data tables that the PC build can drop).

NOT verified: that every vector-library reference has the same parameter list as the game's
callers expect (the callers declare these functions locally); the maths harness will show it.

## Update 2026-10-05 (late): every game source builds

All 204 game sources of the main executable build to 32-bit objects. mathf.c / randf.c: their
five assembly functions (Mathf_WrapAngle, Mathf_SinFast, Mathf_Sqrt, Rand_SeedFloat,
Rand_Float01) are guarded by `#ifndef PORT` and supplied by port/src/mathf_pc.c, operation
by operation on the PS2 float model; the VU0 random register is `gPortVu0R` (simulation
state: it must be saved and restored with the rest for netplay). Sanity-checked only
(sine within 0.002 of libm, square roots, value range of the generator), not against a console.
Game routines still missing: StgVu_RotateX / Y / Z, ObjSeam_TransformVtx (hand-written VU0
assembly outside the vector library) and the entry point.

Open points for bit-exact simulation, none solved yet:
- The game's ordinary C float arithmetic is compiled natively here (IEEE, round to nearest);
  the PS2 truncates and has no denormals / infinities. Decide: software float for simulation
  code, or prove where it matters.
- libm (sinf, tanf, asinf, acosf, atanf, atan2f) and libc rand() are the SDK's newlib
  versions on the PS2; the host's give different bits. The PC build needs its own exact copies.

## First link (2026-10-05, late)

`port/tools/link.py` links all objects with generated stubs for the 238 symbols nobody
provides yet (a stub prints its name and exits) into port/build/bt3, a 32-bit Linux
executable. It links without duplicate or undefined symbols and runs the game's own `main`
up to its first SDK call (`sceSifInitRpc`). C library and libm calls go to the host for now.
Build order: gen_data.py, undefined.py, link.py. Running the executable and implementing
whatever stub it names next is the working loop for the platform layer.
The symbols game code itself needs (not only library data tables) are about 130: CRI ADX
(31), Sony file / CD / SIF / kernel (about 35), GS and MPEG (about 20), pad and vibration
(8), libc / libm (about 25), the nine VU1 microprograms and a few tables, the menu overlay's
entry points (the overlay is not part of this link yet), four VU0 assembly routines.

## Start-up runs (2026-10-05, night)

Verified by running port/build/bt3: `Game_Main` completes its whole initialisation list
(Sys_RebootIop ... PadWatch_Init: heap, graphics, sound, files, movie, common data, save,
jobs, vector unit, DMA buffers, pad, memory card, fonts, fade) and stops at `Overlay_Load`,
where the PS2 reads the menu overlay to a fixed address. Files come from loose folders.

Platform layer so far (port/src):
- plat_mem.c: memory at PS2 addresses (heap 0x00400000..0x02000000 served to the game's one
  malloc; hardware register pages; scratchpad).
- plat_file.c: CRI ADXF over folders made by port/tools/extract_disc.py (`gamedata/`, or
  BT3_DATA); `gamedata/mods/<same path>` overrides a file.
- plat_gs.c: DMA channel control registers (a started transfer completes at the next access;
  the data is dropped in the headless build).
- plat_sys.c, plat_stub.c, plat_mc.c: system calls, sound, GS, pads, movies (do nothing),
  memory card ("no card").
- port/tools/where.sh: where the program is stuck or crashed.

Changes this needed in the shared sources (all made in the decompilation, matching build
unchanged): DMA control register macros go through `Port_DmaChcr` under PORT; the 68 integer
literals with an `L` / `UL` suffix became `LL` / `ULL` (`1UL << 34` is wrong on a 32-bit
host); guards around five assembly maths / random functions and three hardware waits.

Not looked at yet: other uses of the PS2's 64-bit `long` semantics that are not literals
(none found by grep: the sources use s64 / u64), struct layouts that depend on 64-bit
alignment (u64 members are 8-byte aligned on the PS2 and 4-byte aligned on 32-bit x86: must
be checked, `-malign-double` is the likely fix), and float behaviour.

## The fight loop starts (2026-10-05, night)

port/src/headless.c stands in for the menu overlay (`Progress_Main`): it sets up the game's
own attract-demo battle (CPU against CPU), or loads a replay save file named by BT3_REPLAY
(0x1AC00 bytes) through `BattleReplay_Load`, and ends the program when the battle is over.
Verified by running it: the demo battle loads completely (sound banks, stage, both character
models and parameter files, from loose files) and `Battle_Loop` runs; 186 vertical blanks in,
the first CPU update crashes in `BtlObjAnim_QueryEvent`.

Cause, and a general lesson: PROTOTYPE DISAGREEMENTS. Game sources declare the functions
they call locally. On the PS2 `s32` against `u64`, the order of float against integer
parameters and "struct by value" against pointer all produce the same code; on 32-bit x86 the
stack layout differs. `port/tools/check_protos.py` compares every declaration with the
definition by ABI class: 127 of 14,887 declarations disagree (86 functions; list in
port/build/protos.txt). They are being fixed in the decompilation. The same check shows the
vector-library references agree with all their callers except `Vec3_ScaleAdd`.

Other checks added: `port/tools/check_layout.py` (14,862 documented PS2 member offsets
against the host compiler: all agree with `-malign-double`, 22 differ without it; one
remaining report, EftTask.unk4 in eft_c.h, not examined).

The headless build has no real-time pacing: a vertical blank happens whenever the game waits
for one (`gPortVBlanks` counts them).

## A recorded fight plays through (2026-10-05, late night)

Verified by running it: `BT3_REPLAY=gamedata/validation/replay01.bin port/build/bt3` (a replay
the user recorded in PCSX2: versus, character 0 on the pad against character 13 as CPU,
stage 0) loads, runs the intro states, the whole fight and reaches the battle sequence's last
state. Headless it takes a few seconds.

Comparison with the console (the user's PCSX2 save state taken at the result screen of the
same fight, gamedata/validation/state01/eeMemory.bin):

| | Console | PC build |
|---|---|---|
| address of the sequence object (`gBtlSeq`) | 0x187A420 | 0x187A420 |
| battle clock at the end | 0x83F ticks, 1:10.366 | 0x843 ticks, 1:10.500 |
| time left | 170 s | 170 s |
| pad fighter's health | 23050 | 23400 |
| CPU fighter's health | 0 (knocked out) | 7490 |
| heap bytes identical at the end | | 92.1 % of 28.5 MB |

So the heap layout is the console's, the fight follows the recording closely, and it DIVERGES
somewhere: the PC fight ends when the recording runs out, not by the knock-out. Not yet known
where it first differs. Known candidates, none examined: the host's libm (sinf, atan2f, ...)
and libc rand() in place of the PS2's; details of the float model (never checked against a
console); the unmatched functions' C; remaining PS2-only assumptions like the ones below.

What it took after the first link (lessons, each a class of PS2-versus-PC difference):
1. PROTOTYPE DISAGREEMENTS (check_protos.py): fixed in the decompilation, 127 declarations.
2. CALLS THAT RELY ON A LEFTOVER REGISTER: a callback called with no arguments whose target
   reads its first argument (`HudNode_Update`), a call with two arguments to a function that
   uses three (`Res_RelocateOffsets`). The checker cannot see calls through pointers; more
   may exist.
3. FLOAT SEMANTICS: with host floats the first battle frame produced NaNs (the PS2 has none)
   and an endless angle wrap. All game code is now compiled `-msoft-float -mno-sse -mno-mmx`
   and port/src/softfloat_ps2.c implements every float operation with the PS2 model
   (src/port/vu0_a.c): floats travel as bit patterns in integer registers, nothing of the
   host's float unit is used. port/src/plat_libm.c (the only hard-float file) bridges libm and
   double arithmetic. This also removed the 16-byte alignment faults (no SSE moves).
4. DATA THE C FILES TAKE FROM ASSEMBLY (INCLUDE_RODATA) and the VU1 microprograms: gen_data.py.
5. PAD: an all-zero pad packet means every button held (buttons are active-low): the headless
   pads are connected and idle.
6. `long` is 64 bits on the PS2: literals respelled LL / ULL in the decompilation.

Tools added: port/tools/try.sh (test uncommitted decompilation edits here), where.sh,
mc_extract.py (PS2 memory card image reader), BT3_TRACE=<n> (fight state every n vertical
blanks), BT3_DUMP=<file> (heap dump at the end of the battle, for comparison with a console
memory dump: both use the same addresses).

## Hunting the divergence (2026-10-05 / 06)

Console reference: four PCSX2 save states the user made while PLAYING BACK the replay
(gamedata/validation/play01..04: 55 frames into the countdown, battle ticks 319, 906, 1506)
and state01 (result screen of the original fight). Tools: `BT3_AT=<ticks>` dumps heap and
globals at those ticks; `port/tools/compare_state.py <tick> <eeMemory.bin>` compares the
heap byte by byte and the global variables by name (pointer-aware);
`port/tools/intro_metric.py` reads a `BT3_TRACE=1` trace and reports how close the PC is to
play01 (the fighters already move during the countdown, so this is the earliest check).

Verified:
- Health is identical at tick 319 (38030 / 26510) and different at 906 and later.
- A replay stores no random seed; the game's generators differ between any two runs
  (docs/netplay_notes.md in the decompilation), so they are not expected to match.
- 55 frames into the countdown the fighter positions already differ from the console in the
  last few bits (e.g. x 0x43119222 against 0x4311922A): the divergence is ARITHMETIC and starts
  at once; everything later is amplification.

Float model, as far as it is established (port/src/softfloat_ps2.c; PCSX2 is the reference
because the save states come from it):
- FPU add / subtract: truncating, and the smaller operand loses its low bits first (exponent
  difference d of 1..24 clears d - 1 bits, 25 or more leaves only the sign). CONFIRMED: with
  it a decaying animation value matches the console bit for bit at tick 319 (0xB5C0554B);
  with plain truncation it was 3 units low.
- FPU division: to nearest. Evidence: 1.0f / 30.0f in gFade.
- FPU multiply: truncating (assumed). Vector unit add / multiply / divide: truncating
  (assumed). Square root: the vector unit's truncates, the FPU's / libm's rounds to nearest
  (assumed).
- The integer routines agree with the host float unit on 4,000,000 random cases per
  operation in both rounding modes, so what remains is the choice of model, not its coding.
- libm is the PS2's own (newlib 1.10.0 float sources, unmodified, in
  port/third_party/newlib_libm, compiled with software float); libc rand is newlib's
  64-bit generator (read from the executable).
- Experiment switches (environment): BT3_VU_NEAREST, BT3_FPU_NEAREST, BT3_VU_ADDHACK,
  BT3_FPU_NOADDHACK. None of the six combinations tried makes the countdown positions exact
  (differences stay around 0.001 units summed over six coordinates).

Not known: which operation still differs. Candidates: a vector-unit operation PCSX2 treats
differently from the assumption, an instruction the original uses where our C compiles to
something else (multiply-add, the FPU square root's handling of edge cases), or a logic
difference that only shows in the last bits.
The decisive tool would be a small PS2 test program run in PCSX2 that prints the result bits
of every operation class for fixed inputs (the maths harness planned in docs/roadmap.md).

## The replay reproduces the console's fight (2026-10-05, evening)

Verified by running it, against the user's PCSX2 save states:

| Battle tick | Console health (pad / CPU) | PC build |
|---|---|---|
| 319 | 38030 / 26510 | 38030 / 26510 |
| 906 | 33880 / 16320 | 33880 / 16320 |
| 1506 | 26110 / 6970 | 26110 / 6970 |
| end | 23050 / 0 at tick 2111, clock 1:10.366 | 23050 / 0 at tick 2111, clock 1:10.366 |

and 55 frames into the countdown both fighters' positions are bit-identical to the console.
NOT yet identical: at tick 319 the pad fighter's x differs in the last bits (0x43E00E88 /
0x43E00E7F) and 166 of 2816 fighter words differ; not examined (candidates: effects and
camera fed by the game's random generators, which a replay does not reproduce, or a
remaining small arithmetic difference).

How the cause was found (ground-truth tests, port/tests/ps2float, run in PCSX2 2.7.303 with
`PCSX2.AppImage -nogui -fastboot -logfile <log> -- <elf>`; output arrives on the EE serial
port 0x1000F180 in the log):
1. test.c / check.py: every float operation class (FPU and vector unit: add, subtract,
   multiply, divide, square root, multiply-add, int <-> float) on 1500 inputs: the PC model
   agrees on all of them. So the model was right; the difference was above it.
2. libm_test.c / pack_libm.py / check_libm.py: a driver packed into one ELF with the game
   executable calls the game's ORIGINAL sinf, cosf, tanf, atanf, atan2f, asinf, acosf, sqrtf,
   powf, floorf. Against the port's newlib build: 3 to 59 of 1500 results one unit off.
3. Cause: THE PS2 COMPILER'S CONSTANTS. ee-gcc 2.96 converts decimal float constants by
   truncation (0.1f = 0x3DCCCCCC; newlib's S1 = 0xBE2AAAAA where the source comment says
   0xBE2AAAAB), doubles too, and its compile-time folding truncates (1.0f / 30.0f =
   0x3D088888, (int)(0.1f * 100.0f) = 9). A host compiler rounds to nearest. This affected
   every inexact float constant in the game's C, not only libm.
4. Fix: port/tools/eeconst.py rewrites preprocessed source: constants become hexadecimal
   constants with the truncated value, constant arithmetic the grammar exposes is folded
   with truncation, a double constant initialising a float object is truncated. undefined.py
   builds every software-float source through it (not the vector-library references, which
   were written for a host compiler). After it: libm 0 differences on all ten functions,
   and the table above.
Known gap: constant arithmetic that only appears after the compiler propagates variables is
folded by the host with rounding to nearest; not measured.

## Renderer, step 1: software reference of the GS (2026-10-05, night)

Decision (user): the renderer uses SDL3's GPU API; start with the 2D layer.

port/src/gs/gs_soft.c interprets what the unchanged game code sends: the frame's DMA source
chain on VIF1 (`Dma_Flush` -> D1_TADR, chain mode, tag transfer on) -> VIF1 commands (DIRECT
carries GIF data; a command word often sits in the DMA tag with its data in the next run) ->
GIF packets (PACKED / REGLIST / IMAGE) -> GS registers, texture uploads and primitives -> a
software rasteriser (sprites, triangles, strips, fans, lines; texture functions, alpha
blending, alpha and depth test, scissor, both contexts). It is the dependency-free reference
for tests and for the GPU renderer, which takes over at the "registers and primitives" stage.

GS memory is emulated with its real layout (4 MB; page / block / column order per pixel
format). This is required, not a nicety: the game uploads its 4-bit textures as 32-bit
images whose bytes are pre-arranged for the 4-bit layout (the loading screen: a 64 x 32
32-bit upload drawn as a 128 x 128 4-bit texture), and frame buffers are read back as
textures. The 8-bit and 4-bit column tables are generated from their first column.

Use: `BT3_GS=1 BT3_SHOT=<n>` writes port/build/shots/frame_NNNNN.ppm every n frames
(port/tools/ppm2png.py converts); `BT3_GS_DUMP=<frame>` lists that frame's primitives and
uploads; `BT3_GS_VERBOSE=1` prints per-frame counts. Compiled with hardware float (it is not
simulation code).

Verified by looking at the output: the loading screen (the sword-field
mini-game, character sprite and 30 swords, correct colours and transparency) and the game
logo overlay during the battle intro. The 3D scene is black: models go through the VU1
vertex programs, whose data the VIF1 interpreter currently skips. About 8 frames per second
(per-pixel C, no optimisation).

Not done: VU1 programs (stage, characters, most effects), bilinear filtering, fog,
mip-mapping, GS -> host transfers, the display registers (the shown buffer is guessed as the
one that received most pixels), GIF data on the GIF channel itself (`Dma_SendGif`, used for
some uploads: not yet routed to the interpreter).

SDL3: the 64-bit library is installed (3.4.12); the port is a 32-bit program and needs
`lib32-sdl3` (multilib repository), not installed yet.

## Renderer, step 2: SDL3 GPU back end for the same primitives (2026-10-05, night)

port/src/gs/gs_gpu.c (+ shaders/gs.vert, gs.frag, compiled with glslc and embedded by
undefined.py). gs_core.c keeps decoding and GS memory and hands every primitive, with the GS
registers current at that moment, to `GsGpu_Draw`; `GsGpu_FrameEnd` uploads the frame's
vertices and newly decoded textures in one copy pass, replays the draw list (one render pass
per run of draws to the same frame-buffer address), shows the buffer that received most
draws, and paces to 30 frames per second. `BT3_GS=gpu` selects it (`port/run.sh`).
- Render targets: one per GS frame-buffer address, 1024 x 1024 GS pixels at SCALE = 2, with a
  D32 depth buffer; GS z is scaled to 0..1 and compared GREATER / GREATER_OR_EQUAL as on the GS.
- Textures: decoded from GS memory to RGBA and cached by TEX0, TEXA and the upload generation
  of the pages they occupy. A frame buffer used as a texture is sampled directly, unless
  something was uploaded over it since it was drawn.
- Blending: ((A - B) * C >> 7) + D turned into source / destination factors (coefficients
  0, 1, C, -C, 1 - C; 1 + C is approximated by 1). Alpha is carried with 1.0 = 0x80.
- Texture function and alpha test are in the fragment shader.
- SDL GPU's clip space has +Y up on every back end: the vertex shader flips y.
Verified by reading the render target back (`BT3_SHOT=<n>` writes
port/build/shots/gpu_NNNNN.ppm): the loading screen and the logo overlay are drawn correctly
at twice the PS2's resolution, at full speed.
The 32-bit SDL3 GPU device works on the development machine (Vulkan, NVIDIA; lib32-sdl3,
lib32-nvidia-utils).

Known gaps of the GPU back end: a buffer sampled while it is the draw target (skipped);
region clamp / repeat; destination alpha; frame-buffer masks; the texture cache keys on
upload generations, so a texture re-uploaded every frame (the loading screen does this for
every sprite) is decoded again each time (correct, wasteful: hash the contents instead);
no keyboard / pad input yet; the window shows nothing of the 3D scene (VU1 programs).

## Renderer, step 3: the vertex unit (VU1) as an interpreter (2026-10-05 / 06)

Decision: run the game's nine ORIGINAL vertex programs in an interpreter first
(port/src/gs/gs_vu1.c), instead of rewriting each as a shader straight away. Nothing was
known about what the programs do (the decompilation's notes say so), and an interpreter
gives the correct 3D scene for all nine at once and a reference to check shader versions
against later. Its output is GIF packets (XGKICK), so both back ends draw it unchanged.

- VIF1 side (gs_core.c `vif`): STCYCL, OFFSET, BASE, ITOP, STMOD, STMASK, STROW, STCOL, MPG,
  UNPACK (all formats, masks, modes, skipping and filling cycles, TOPS-relative addresses),
  MSCAL / MSCALF / MSCNT with the TOP / TOPS double-buffer flip.
- Interpreter: the instruction subset found by surveying the 1946 instruction pairs of the
  microcode block (upper: add / sub / madd / mul with broadcast, Q and I forms, the ACC
  forms, itof0, ftoi0, ftoi4, abs, clip; lower: lq, sq, lqi, sqi, sqd, ilwr, iswr, isw,
  integer add / and, branches, bal, jr, div, waitq, mr32, move, xtop, xgkick, fcand, fcor,
  fcget) plus neighbours; unknown instructions are counted and reported. Upper and lower
  halves of an instruction read their operands before either writes; Q has its latency;
  the clipping flags have their four-instruction delay. MAC / status flags are not modelled
  (no program reads them).
- A fact that cost a debugging round: BAL stores an instruction NUMBER and JR jumps to one
  (no division by 8).
- port/tools/vudis.py disassembles microcode (the used subset); a run that does not end
  dumps port/build/vu1_micro.bin and vu1_mem.bin.

How the game drives it (verified from the command stream of a battle frame): MPG uploads a
program at 0 (two commands, 399 instructions for the first battle program); UNPACK of 12
quadwords of constants at 0; BASE 141, OFFSET 436; MSCALF 0 (initialisation, ends at the
first E bit); then per batch UNPACK header (3 quadwords at TOPS) + vertices (3 quadwords each
at TOPS + 3), MSCNT. The program's main loop: transform by the matrix in vf1..vf4 and a
second one in vf5..vf8, divide, ftoi4, clip test with a branch to a clipper, XGKICK.

Verified by looking at the GPU read-back: both fighters are drawn
correctly (textured, cel-shaded, with their after-images) during the battle intro, and the
sky is drawn.
Wrong or missing: the sky shows as vertical stripes (first_stage_sky.png), the stage itself
is black in the frames looked at, the picture is slower than real time (about 15 frames per
second: the interpreter, thousands of small draws, and textures decoded again whenever
their upload generation changes).

## Renderer, step 4: the whole scene in the software reference (2026-10-06)

Verified by looking at it: the software reference
draws a complete battle frame: sky, the plains stage, both fighters, the logo overlay.

What was missing, found by drawing only the first N vertices of a frame (`BT3_GS_STOP`):
the stage was drawn correctly and then wiped by the game's post-processing. Facts about how
the game uses the GS, all needed for a correct picture:
- GS memory map in battle: frame buffer at block 0 (fbp 0x000, 512 x 448, 32-bit), second
  buffer at fbp 0x070, DEPTH BUFFER at fbp 0x0E0 (ZBUF 0xE0, 24-bit: PSM 0x31), work buffers
  at fbp 0x150 (256 x 256), 0x170 (128 x 128), 0x1F6 / 0x1F8 (64 x 64): a down-sampling
  chain (glare / bloom), textures streamed to block 0x2A00 with palettes at 0x32C0.
- The stage is textured by STREAMING: each material's texture is uploaded to the same
  address (0x2A00) immediately before the triangles that use it, dozens of times per frame.
- The depth buffer is cleared by drawing sprites INTO it as a frame buffer (FRAME fbp 0xE0),
  and it is read as a texture (TEX0 tbp 0x1C00 with the depth formats 0x31 / 0x32 or as
  PSMT8H, i.e. its top byte as an 8-bit index). So depth must live in GS memory with the
  real layout: the depth formats are the colour formats with the block index XOR 0x18.
- FRAME.FBMSK (per-bit write mask) is used constantly: 0x00FFFFFF to write only the alpha
  byte, 0xFF000000 to protect it, and 0x3FFF with a 16-BIT view of the 32-bit frame buffer
  (512 x 896, drawn in 8-pixel strips with the texture shifted by 8 pixels: the "channel
  shuffle" that moves one half of every 32-bit pixel into the other).
  Ignoring the mask made that pass overwrite the whole picture.
- Depth clear between stage and fighters: 16 untextured strips, alpha 0, blending on (colour
  unchanged), depth test ALWAYS.
- DMA address registers: the game masks addresses to 28 bits (`& 0x0FFFFFFF`), which cuts PC
  stack addresses: `DMA_PHYS` in the decompilation keeps the pointer on PC. The GIF channel
  (`Dma_SendGif`, used for uploads outside the frame list) is routed to the front end too.

Two artefacts fixed afterwards (reference_full_scene.png shows the result):
- SHADOW drawn as a black shape near the horizon: the game switches PRMODECONT to 0 for its
  shadow passes, so the primitive attributes come from PRMODE (0x48: Gouraud, blended,
  untextured) and PRIM only gives the type. With that honoured, the fighter is first drawn
  from above into the 256 x 256 work buffer (fbp 0x150, VU1 program of 101 instructions) and
  the shadow then lies on the ground under it.
- DARK SPECKLES over the fighters: the black outline hull is drawn at almost the same depth
  as the body. The rasteriser's float weights summed to just under 1, so a flat triangle's
  depth came out one unit low on some pixels and the hull won. Weights are now double
  precision and depth / colour are rounded.
- The last pass of the frame draws the depth-buffer memory (cleared to 0 by then) as a
  24-bit texture over the picture with alpha 0x19: a uniform darkening (the intro's fade).
`BT3_GS_STOP=<n>` (with BT3_GS_FROM / BT3_GS_DUMP on the same frame) draws only the first n
primitives of that frame: the way these were located.

The GPU back end does not have depth-in-memory, depth as a texture, or FBMSK yet, so its
picture is behind the reference's (stripes, black stage). The post-processing chain reads
and writes single channels of the same memory in several formats; on the GPU those passes
need either the same tricks expressed with colour masks and format-converting shaders, or
native replacements of the effects (depth tint, glare, blur).

## Renderer, step 5: the GPU back end draws the whole scene (2026-10-06)

Decision (user): full-screen effects that depend on PS2 memory tricks get NATIVE versions;
everything else stays on the generic path. Step 1 of that, done:
- PRMODECONT / PRMODE: the GPU path reads the effective attributes (gGs.prim), so the shadow
  passes work as in the reference.
- FRAME.FBMSK in its whole-channel forms becomes the pipeline's colour write mask (alpha
  only, everything but alpha, ...). Partial masks cannot be expressed and are ignored.
- PS2-only passes are DROPPED in GsGpu_Draw (counted; `BT3_GS_VERBOSE` prints the number):
  drawing through a 16-bit view of the frame buffer; drawing into the depth buffer's memory;
  sampling the depth buffer's memory or a buffer's top byte as an 8-bit index; sprites that
  copy one frame buffer into another or into itself. About 240 primitives per battle frame.
  Triangles textured with a buffer are kept (the projected shadow).
Verified by reading the render target back: the same
frame as the reference's, at twice the resolution: sky, stage, both fighters with their
outlines, the shadow on the ground, the logo overlay.
Missing on purpose until their native versions exist, so the GPU picture is brighter and
sharper than the reference's: depth tint, glare, the intro's fade, any blur.
Speed is unchanged (about half real time): see the profile above, not addressed yet.

## Renderer fixes after the user looked at the window (2026-10-06)

- SMEARS TRAILING BEHIND CAMERA PANS ("a trail of textures"), stage picture stuck on the
  nearest geometry ever drawn: nothing cleared the depth buffer between frames. The game
  leaves the per-frame clear of colour and depth to Sony's library: `sceGsSetDefDBuff`
  builds a "drawing environment + clear sprite" GIF packet per buffer inside sceGsDBuff and
  `sceGsSwapDBuff` sends it every frame (Gfx_EndFrame step 8). Both were empty stand-ins.
  Implemented in port/src/gs/gs_sony.c from the documented layout.
- TREES WITHOUT LEAVES: the leaves are alpha-tested with a result exactly at the threshold
  (texture alpha 0x80 times vertex alpha 0x7F = 0x7F, test "alpha >= 0x7F"). The shader
  compared floats; it now rounds to the GS's integer first.
- TEXTURE CACHE: keyed on a content hash of the GS memory pages a texture and its palette
  occupy (the game streams stage textures to one address hundreds of times per frame; the
  old key changed on every upload). About 80 to 200 live textures instead of 2,048 churning,
  and the window now runs close to full speed.
- `BT3_GS_PROBE=x,y` (with BT3_GS_DUMP): the reference lists every primitive that reaches a
  pixel, with its state. This is how the leaves were diagnosed.
- CHARACTER OUTLINES ARE MISSING in the GPU picture, as expected after dropping the PS2-only
  passes: the outline is a full-screen effect (`ObjOutline_Draw`, src/sys/gfxm_a.c), not
  geometry. It reads a per-pixel object id that the models leave in the frame's alpha (kept
  in the depth page's spare top byte), finds where neighbouring pixels have different ids
  (two shifted, masked, subtracting copies), writes the edges into the frame's alpha and
  blends a dark rectangle (0x64 per channel) by it. First native effect to write.
  Design note for it: the render targets currently store alpha rescaled (1.0 = 0x80,
  clamped), which destroys ids above 0x80; the GS alpha byte needs its own exact channel
  (a second colour attachment written unblended under the alpha part of FBMSK).

## First native effect: the outline (2026-10-06)

Mechanism for native effects, used here for the first time:
- MARKER. The game builds the frame's display list first and the renderer runs it later, so
  the place of an effect travels in the list: `Port_GsMarker(effect)` (port/src/gs_marker.c)
  queues a one-register GIF packet writing the effect number to "register 0x7F", which the
  GS does not have. The game's effect function calls it under `#ifdef PORT`
  (`ObjOutline_Draw`: PORT_FX_OUTLINE = 1). gs_core.c passes it to `GsGpu_Native`; the
  software reference ignores it and keeps drawing the PS2 passes, which the GPU path drops.
- EXACT ALPHA BYTE. Every render target has a second colour attachment (R8, "aux") that
  receives the GS alpha byte unblended, under the alpha part of FBMSK; the colour texture
  keeps alpha rescaled for blending. Object numbers live in that byte.
- NATIVE PASS. In the replay, a native entry ends the current pass and runs its own pass on
  the colour texture alone (so the aux texture can be sampled), then normal drawing resumes.

The outline itself (shaders/outline.frag): the PS2 pass (verified reading of
`ObjOutline_Draw` / `ObjOutline_BuildClut`) maps the id byte through a table (id * 8, or
0x80 when that is 0; id 0xFF = no colour), subtracts the same image shifted by one pixel in
each axis so only positive differences remain, and blends a dark rectangle (0x64 per
channel) through the result. The native shader reads the id of the pixel and its four
neighbours one PS2 pixel away, applies the same table, and subtracts 0x64 / 255 where the
pixel's value is larger than a neighbour's. Where the ids come from (verified on a frame):
the frame's alpha is set to 0xFF, the fighters are drawn with their part numbers as alpha,
and that alpha is what the pass reads (on the PS2 through a copy in the depth page's top
byte). `BT3_FX_DEBUG=1` shows the numbers instead of the lines.
Verified by looking at the read-back: a dark line around
both fighters and between body parts with different table values.
Not matched exactly: the PS2 line's thickness and strength (it draws the edge image three
times, one line up, one down and centred, with different alphas); decoded textures keep
alpha rescaled, so an id passes through a rounding step (looked right, not proven exact).

## Frame time (2026-10-06)

Measured (`BT3_GS_VERBOSE` prints "time:" every 60 frames; `perf record` for the split), on
the development machine, replay01 in the window:
- The picture is CPU-bound on one thread; the graphics card is idle. In the fight a frame
  takes about 23 to 32 ms of work against a budget of 33.4 ms, so 30 frames per second is
  held most of the time and missed on a quarter to a third of the frames in the heavier
  stretches.
- Where it goes (after the fixes below): the VU1 interpreter about 45 % plus UNPACK 9 %,
  GS front end (GIF decoding, page hashes, GS memory writes for uploads) about 15 %, the GPU
  back end's recording 4 %, the game with its software float the rest.
Done: texture cache by content (was 30 %), last-texture shortcut, everything compiled -O2
and without PIC (the replay still reproduces the console's fight exactly), the upper and
lower NOPs of the vertex programs skipped, present mode MAILBOX (VSYNC added a wait for the
display on top of the game's own 30 Hz pacing). Pacing and timing now sit in gs_core.c.
Tried and switched off: a render thread (BT3_GS_THREAD=1). It is how the PS2 works (DMA
executes the list while the CPU runs the next frame), but here the list is still being
interpreted when the game changes data the list refers to; the user saw the camera jump.
It also gained little, because the renderer's own time (about 25 ms) is what sets the pace.
Next: the interpreter itself (pre-decoding the microcode when it is uploaded, whole-vector
operations for the full-mask cases, a straight copy for unmasked V4-32 UNPACK), and later
the shader versions of the programs, which remove this cost altogether.

## Vertex programs as shaders: program 0 (2026-10-06)

Decision (user): no interpreter tuning; go straight to shader versions. The interpreter
stays as the fallback for programs without a shader, as the reference to compare with
(`BT3_VU_INTERP=1` forces it), and for the software renderer.

Who costs what (measured, `BT3_GS_VERBOSE`; programs are told apart by uploaded size):
| uploaded size | program in the executable | used for | share of interpreted instructions |
|---|---|---|---|
| 399 | 4 (402) | stage and most static geometry | 27 to 39 % |
| 127 | 0 (128) | the fighters' models, two layers | 30 to 36 % |
| 101 | 3 (102) or 2 (100): not determined | the fighters again for the shadow buffer | 29 to 35 % |
| 451 | 6 (454) | a few runs per frame | under 1 % |

PROGRAM 0, read from its disassembly (port/tools/vudis.py) and confirmed on memory dumps
(`BT3_VU_DUMP=<frame>`):
- Constants, VU memory quadwords: 0..3 bone matrix A, 4..7 bone matrix B (columns), 8 / 9
  their pivots, 10..13 a matrix of which only the x components are set (the light direction:
  x' = light . v), 14..17 the matrix to GS screen coordinates, 18..21 the clip-test matrix,
  22 / 23 the two layers' colours (0..255 as floats). MSCALF 0 only precomputes 26..33
  (10..13 times A and B).
- A batch (one MSCNT): +0 a GIF tag "one A+D register", +1 TEX0_1 (texture layer), +2 TEX0_2
  (toon layer), +3 / +4 the primitive tags of the two layers (NLOOP = vertex count, PRIM in
  the tag: 0x5C and 0x25C, registers ST, RGBAQ, XYZF2), +5 the vertices: position with the
  blend weight in w, normal, (s, t, 1). One batch is ONE triangle strip.
- Per vertex: p = mix(B (pos - pivotB), A (pos - pivotA), weight); screen = C p, divided by
  w, to 12.4 fixed point with the integer depth in XYZF2; layer 0 gets (s, t) and colour 22;
  layer 1 gets u = 0.5 + 0.5 (light . normal'), v = 0 and colour 23: cel shading through a
  ramp texture. A triangle with a vertex outside the guard volume is dropped (ADC bit), there
  is no real clipping.
- Output: both layers in one kick, the second packet right behind the first.
Shader version: gs_vu1.c `hle_program0` sets each layer's TEX0 and PRIM through the normal
register path and passes the raw vertices and constants to `GsGpu_DrawVu0`;
shaders/vu0.vert does the skinning, outputs clip-space positions (the GPU clips and
interpolates with perspective; x, y, depth mapped as for GS vertices) and the two layers'
colour / coordinates; the fragment shader is the common one. Strips become triangle lists;
consecutive strips with the same state and constants are one draw call.
Verified: the same frame rendered with the interpreter and with the shader differs clearly in
587 of 917,504 pixels (0.06 %, edges); `work per frame` went from about 30 ms to about 24 ms
with a third of the interpreter's work gone.

## Next steps, in order

1. DONE: data (gen_data.py). 2. DONE except mathf.c / randf.c. 3. DONE (portsrc.py).
4. Platform layer for the 69 library functions: files from loose folders, memory, pad,
   stubs for sound and the GS for the headless build.
5. Link, boot to the battle loop, replay validation.

## 2026-10-05 (evening): fight camera inside a cliff, see-through fighter

- **Camera going through terrain (verified against console state `play07`, tick 666).** `StgCol_TraceSphere`
  handed `ColCapsule_GetLongSegDir` a 16-byte vector for a 32-byte segment. On PS2 the overflow lands in the next
  local (which the decomp had named `unused`); on PC the stack order differs and it overwrote the query's bounding
  box, so the camera's sweep never tested any terrain. Fixed in the decomp by declaring the scratch as two vectors
  (still matching). Lesson: a local that is "unused" next to an out-parameter is a hint of an undersized buffer;
  the camera of the pad fighter now equals the console's at that tick.
- **Stale work buffers.** A work buffer whose filling pass was dropped is no longer sampled (blocks of noise).
- **Vertex program 4 (stage) as a shader** (`vu4.vert`): strips go to the GPU unclipped, the program's own clipper
  is not needed.
- **See-through fighter ("alpha key", `GfxAlphaKey_Draw`)**: native pass (`alphakey.frag`, marker 2). Three things
  were needed: the pass itself; rectangles drawn into palette memory (the game paints alpha 0xF9 over the fighter
  palettes) must be executed in GS memory; texture alpha above 0x80 must survive (it was clamped at upload, now
  the shader rescales).
- Tools: `BT3_SHOT_FROM` / `BT3_SHOT_TO`, `port/tools/montage.py`, `BT3_CAMCHECK=<vblank>`.

## 2026-10-05 (night): VU1 listings, silhouette program as a shader

- The nine VU1 microprograms are documented assembly in the decomp now (`src/vu1/prog*.vsm`,
  `docs/systems/vu1/README.md`): 0 / 1 fighters, 2a / 2b flat-colour fighter (flat shadow / silhouette into the
  shadow page), 4 stage, 5 unused, 6 ground shadow, 7 debris, 8 animated stage objects.
- Measured per program: the interpreter's work in a fight frame was 92% program 2b (1,092 runs) and 7% program 6.
- Programs 2a / 2b go through program 0's shader (`hle_program2` in `gs_vu1.c`: the same batch, constants in
  other places, first layer only). Frame work about 21 ms -> about 13 ms.
  First version lost the shadows (my check used frames with both fighters in the air; the user noticed). Cause:
  the shadow camera's matrix is orthographic with a negative constant w (-862 measured); the PS2 only divides,
  a GPU clips it as behind the eye. The shader negates the whole position when w < 0. Also: the context comes
  from the PRIM in force (the shadow passes use PRMODE), and depth is clamped, not clipped. Checked on the first
  frames of the fight (fighters on the ground) against the interpreter: shadows equal by eye. Left in the interpreter: program 6 (6 runs per frame), 1, 7, 8.
- Not carried over from the originals: program 4's 0.5% enlargement of clipped triangles (see the decomp notes).
- **Program 6 (ground shadow) as a shader** (`vu6.vert`, `hle_program6`, `GsGpu_DrawVu6`): position through the
  screen matrix, s, t generated from the shadow camera matrix and the scale, flagged strip triangles skipped on
  the CPU, the texture coordinates rescaled for a frame buffer used as a texture. Checked on the first frames of
  the fight against the interpreter (shadows equal by eye). The interpreter now runs 0 times in replay01's first
  1,440 frames (48 s; the timing line is printed every 60 frames = 2 s, not every second as first written);
  frame work about 10 ms (mean of the 60-frame averages). Three single frames over the 33.4 ms
  budget remain (75.7 ms near frame 121, about 36 ms near frames 361 and 1261): not examined.
- **Slow single frames = pipeline creation.** `BT3_GS_VERBOSE` now prints a `slow:` line for every frame over
  budget (game / display list split, new textures and pipelines, time in each). Measured: the three slow frames
  were 46 ms, 21 ms, 21 ms of pipeline creation (10, 5, 2 pipelines), texture decoding only 2 to 9 ms. Fix: a
  pipeline is built from its key alone (`pipeline_create`), keys are appended to `bt3_pipelines.txt`
  (`BT3_PIPELINES` overrides the path; git-ignored) and all known ones are created at start (22 in 101 ms).
  Whole replay with the file present, two runs: no frame over budget, mean 10.8 ms, worst 29.1 ms. The first run
  on a machine (no file) still has the hitches; shipping a list with the game would remove that.

## 2026-10-05 (late): the menu overlay is part of the PC build

- `src/menu/*.c` (the DBZP.BIN overlay, 69 files) is compiled and linked with the rest: all files compiled
  unchanged. `port/tools/portsrc.py` lists them; `gen_data.py` also converts the overlay's one assembly data
  chunk and now assembles the data objects itself (that step was done by hand before and not scripted).
- `Progress_Main` exists twice (the real one and the replay / demo stand-in in `headless.c`): linked with
  `--wrap=Progress_Main`. `BT3_REPLAY` or `BT3_DEMO` take the stand-in; otherwise the real menus run
  (`port/run.sh menu`). The overlay load (`Overlay_Load`) reads nothing.
- Fixes needed to reach the title screen: `UNCACHED()` in movie.c under PORT; `StgVu_RotateZ / X / Y` (hand-written
  VU0 code in the stage module) added to the reference vector library. Movies are skipped (MPEG stand-ins).
- Verified by screenshots: memory card check, publisher logos, legal screen, title screen with "Press START".
- **Input** (`port/src/gs/gs_input.c`): keyboard and SDL gamepads into the game's pad buffer; mapping in the
  file's header. The user confirmed a controller works.
- **Pacing**: the vertical blank is the clock with a window (`Port_VBlank` waits on a 59.94 Hz grid): menus run
  at 60 frames per second, battles at 30, as the game asks. `BT3_UNCAPPED=1` removes the wait.
- `port/src/plat_crash.c`: a fatal signal writes a backtrace to the terminal and `bt3_crash.txt`.
- **Memory card** (`port/src/plat_mc.c`): slot 1 is a formatted card backed by the folder `saves/card1/`
  (`BT3_SAVES` overrides `saves`), slot 2 is empty. Card paths are used as they are, so a save has the console's
  files. Result codes as libmc gives them (listed in the file). The user confirmed: a save can be created and a
  duel started from the menus (2026-10-05). The earlier "closes when creating a save" was with the "no card"
  layer and an executable built without the pacing code; not seen again, cause not established.
- Build tool fix: `eeconst.py` did not know octal literals and stopped `undefined.py` part-way (objects after
  the failing file stayed stale while the link still said OK). Lesson: read the whole output of the build step.

## 2026-10-06: sound, part 1: ADX streams

- `port/src/gs/snd_adx.c` replaces the ADXT stand-ins: players, start from an archive file or a loose file, stop,
  pause, volume (0.1 dB units), pan per input channel, mono switch, status (stopped / playing / play end), loops
  from the version 4 header. One SDL audio stream per player; decoding on SDL's audio thread.
- All 65,201 files of the third archive and both movie tracks are plain ADX (type 3, 18-byte frames, version 4,
  not encrypted; mono or stereo; 16, 24 or 48 kHz). Music files loop from sample 0 to a loop end in the header.
- Checked: the decoding formula (as a Python copy) against ffmpeg's ADX decoder: identical on a short stereo clip;
  up to 91 / 32768 apart over 3 s of music, from the rounding of the two filter coefficients.
- Not checked by ear by me (no way to listen): the user has to confirm music and voices.
- Headless runs open no device and every player reports "stopped", as before: the replay validation is unchanged.
  With a window the status is real, so anything in the game that waits for a voice line now really waits.
- `BT3_NOSOUND=1` turns the sound off.
- 2026-10-06: the user confirmed the opening movie plays (picture; ffmpeg pipe, `port/src/plat_movie.c`) and the
  music plays. "No sound" during testing had two causes outside the code: the system's default output was the
  Sunshine host's virtual sink while Sunshine ran (started by me earlier in the session), and the user was
  expecting sound effects, which are not implemented yet. A movie picture has to be placed in the frame's draw
  order (after the buffer clear): `GsGpu_FbUpload`.

## 2026-10-06: sound, part 2: sound effects

- `port/src/gs/snd_se.c` stands in for the game's IOP driver SOUNDS.IRX behind `sceSifCallRpc`, `sceSifSetDma` and
  `sceSdRemote`: bank capture (sample data into a 2 MB "sound chip memory", the two tables as they are), the
  per-frame command queue (play / stop by number / stop by handle), pause, stop bank, reset, mono, bank volume.
- SOUNDS.IRX used Sony's SE sequencer (modsesq2) and synthesizer (modhsyn); its debug strings name the calls
  (`SesqPlay port_num, sesq, loop, vol, pan, pitch, cpu_id`). Measured over all 361 bank files (18,924 effects):
  every effect is one note-on (two also stop after 2000 ticks), timbre i uses sample i, and all timbres have the
  same 0x30 parameter bytes. So a sampler is enough: PS2 ADPCM decode, pitch in cents, constant-power pan, 48
  voices, one SDL stream at 48 kHz. Sample rates in the banks: 8000 to 24000 Hz, almost all 16000; 3 looping.
- Table layouts (Vagi, Setb, Sesq) are in the file's header. Not reproduced: the envelope release (8 ms fade),
  the chip's own interpolation, reverb.
- Loudness: 0.25 of full level (`BT3_SE_GAIN=<percent>`), from the console's chain (three volumes of 100/127 and
  the chip's half-scale voice volume); the user found 0.55 too loud and 0.25 fine against the music.
- Verified by the user by ear: effects play in menus and fights. Not verified: individual effects against the
  console, pitch accuracy, the two timed effects, the looping ones.

## 2026-10-06: full-screen effects, part 1: the generic table pass and the switches

- The game's depth effects share one primitive, `GfxPost_DrawDepthClut`: the spare top byte of each depth word,
  drawn over the screen as an 8-bit texture through a 256-colour table. The byte holds either a fog value made
  from the depth (`GfxDepthFog_Draw`) or a copy of the frame's alpha, i.e. the object ids
  (`GfxPost_CopyAlphaToDepth`). The GPU back end now recognises that draw (sprite, PSMT8H texture at the depth
  page) and runs it as one full-screen pass (`dclut.frag`, `depth_clut` in gs_gpu.c) with the game's own table,
  blend mode and write mask. It tracks which of the two meanings the byte has from the two passes that fill it,
  and keeps its own copy of the ids taken at the moment the game copies them (later passes overwrite the live
  alpha; reading the live one lost the outline).
- Through it: the depth tint, the see-through tint (the hand-written `alphakey.frag` pass is no longer used: the
  table comes from the game now), and the alpha writes that feed the glare and the object glow. The outline keeps
  its own native pass; the game's outline passes through this primitive are dropped.
- Fog value used: 0 for depth >= 0xFFA8, else 255 - depth / 256. This is read from the code, NOT confirmed: the
  software reference's depth page at that moment gave a byte that matches this in the lower half of the picture
  and is off by about +-24 in bands in the middle (either the reference's 16-bit view of the depth page is not
  exact, or the console really bands; not settled). On the test stage the tint makes no visible difference
  (compared on / off), so this stage does not prove it either way.
- 32 of `BT3_FX_OFF` / `fx_off` ("Movement blur" in the settings window, 2026-10-10): the game's blur of the scenery
  while the view turns (StgPanBlur, src/sys/gfxm_a.c). Left out where the game draws it (`Port_FxOff(32)` in
  `StgPanBlur_Draw`: the views are still kept up to date), not in the renderer: it is several passes through work
  buffers. Checked without a window: with it off the scenery of a turning view is sharp, and the fight is the same.
- Switches: `BT3_FX_OFF=<mask>` or F1..F4 while running: 1 outline, 2 see-through tint, 4 depth tint, 8 glow
  (glare and object glow: reserved, the glow pass itself is not written yet).
- Still dropped: the glow pass (shrink, blur, add), pan blur, haze, stage blur, water wobble, lens. (The cross-fade
  is drawn since 2026-10-10: the game's read-back of the frame is answered, see docs/known_bugs.md.)
- **Buffer-to-buffer passes are drawn** (glare, object glow, blur of distant things; `draw_state` in gs_gpu.c):
  textured sprites from one render target into another, with sampling held inside the buffer's own area (the GS
  CLAMP register done by hand, since each buffer is a corner of a larger texture). Not drawable and dropped: a
  buffer sampled while it is the target, and 16-bit views.
- **Half-pixel rule for those passes.** The GS evaluates texture coordinates at whole pixel positions, a GPU at
  pixel centres. The glow chain's blur steps sample one texel apart on purpose; at pixel centres they did not blur
  and moved the picture one texel per round, which the user saw as a displaced ghost of the nearest hill in the
  sky during the intro (I first explained it away as the distance blur: wrong; the frames showed it at once).
  Primitives textured from a render target are moved half a GS pixel. Other primitives are left as they were:
  at 2x, nearest-sampled sprites would pick the neighbouring texel for half their sub-pixels.
- Switches: F4 glare and glow (the additive pass back to the frame), F5 blur of distant things (the pass mixed in
  by destination alpha); `BT3_FX_OFF` bits 8 and 16.
- `BT3_SHOT_VBLANK=<n>`: a screenshot at a vertical blank, to compare with a console save state's tick. At the
  slot 7 tick, region averages (grass, trees, hill) are within a few units of the console screenshot.
- The direct replay mode starts no music: the menus that start it are skipped.
- Glare and glow strength: `BT3_GLOW=<percent>`, or the settings overlay (F1) in steps of 10; default 60 (the user's choice; 100 is what
  the game's passes give, and the GPU and software pictures agree on that level, but it was not compared with a
  console at a peak of the glare). The half-pixel rule is applied to the texture coordinates of sprites textured
  from a render target (moving the sprite left the first row and column undrawn).

## 2026-10-06: HUD

- **Bars did not shrink**: the gauges are cut to length with a mask (src/battle/hud_a_d.c): context 2 writes only
  alpha with FBA (bit 7 set) where the mask sprite's alpha is not 0, context 1 draws the bar with TEST.DATE where
  bit 7 is clear. Added: the FBA register, DATE / DATM in gs.frag against a copy of the alpha bytes taken at the
  start of each run of DATE draws (`native` 5). Confirmed by the user in a duel.
- **Slivers of other bars' colours, grey lines under the panels**: at 2x each output pixel sampled the sprite sheet
  at its own centre, a quarter or three quarters of a texel past where the GS samples, which at a sprite's edge
  is the neighbouring picture. 2D sprites now take the coordinate at the whole GS pixel (gs.frag, misc.w), so
  2D art looks as on the console (2x point-scaled), not sharper. Not yet confirmed by the user.
  **Since 2026-10-10 this is the setting "2D filtering"** (F1, `smooth_2d`, `BT3_2D_SMOOTH=0/1`): "Sharp
  (console)" is the above; "Smooth", the default, samples the game's own 2D art at every output pixel, held inside
  the piece's rectangle of its sheet by the middles of its edge texels (the rectangle worked out for a texture
  pack's replacement, gs_draw.c; the rule in gs.frag is its own: the plain coordinate). The replacement's rule,
  tried first, picks one of two formulas per pixel by a derivative and flickered on art near one texel a pixel:
  comb stripes in small print at 6x, which the user saw as blurred text, a line in a letter, a pop-up cut short
  and tearing on moving pieces. Seen by the user: the fight's display, the title, the main menu. Not seen by
  anyone: every other screen.

## 2026-10-06: repository rule, widescreen

- **The decomp holds PS2 code only** (the user's decision). The 82 `#ifdef PORT` blocks and `src/port/` were
  removed from the decomp; that commit is recorded here with `git merge -s ours`. PC-only changes to game code are
  made in THIS repository's copy of `src/` and `include/`, inside `#ifdef PORT` so the PS2 text stays visible.
  Later merges from the decomp can conflict in those files: keep the PC lines.
- **Widescreen** (`BT3_WIDE=1`): 16:9 with the same height of view and more to the sides.
  - Game side (all `#ifdef PORT`): `View_SetProjection` multiplies the aspect by `Port_WideFactor()` (4/3); the
    game's hard-coded 4:3 pieces get the same factor: the stage's culling frustum (`StgFrustum_Build`), the effect
    clipper's side planes (`EftGfx_UpdateClipPlanes`: the sky stopped short of the edges without it), and the
    pixel aspect the effect billboards use (four `Vec4_Set(&aspect, 1, 7/6, 1, 1)`).
  - 2D art is laid out for 4:3: the renderer narrows each 2D piece drawn into the picture to 3/4 about a fixed
    point (`GsGpu_Draw`): the screen's middle in the menus; the left edge, right edge or middle for the HUD's
    left panel, right panel and centre parts (display-list markers 0x10..0x13 from the HUD's side selection and
    node draw); a sprite's own middle otherwise. Not touched: draws into work buffers (effects, shadow page),
    full-screen fills, in a fight screen-wide sprites and 2D triangles outside the HUD.
  - The window shows the picture at 4:3 or 16:9 whatever its own shape, with black borders.
  - The fight's outcome is the same with widescreen on (tick 2111: 23050 / 0). Checked by picture: three replay
    frames (sky to the edges, fighters and logo not stretched, shadow intact). Not checked: HUD panels at the
    edges, the menus, other stages, split screen.
- **Any aspect ratio from 4:3 up** (`Port_AspectMilli` in plat_stub.c): `BT3_ASPECT=21:9`, `BT3_WIDE=1` (16:9), or
  a `BT3_WINDOW=WxH` wider than 3:2 (its own shape). The 2D narrowing and the window follow it. Fight outcome
  unchanged at 4:3, 16:9, 21:9, 32:9. The window opens at the picture's shape and keeps it on resize.
- Captions (technique names; the replay mark) are anchored to the side they are drawn on (marker 0x14).
- 2D layout in widescreen cannot be checked from the replay (it hides the HUD): only the user has seen it.

## 2026-10-06: recorded input; the strip behind READY / FIGHT

- **Recorded controller input** (`BT3_PAD_REC=<file>` / `BT3_PAD_PLAY=<file>`, plat_stub.c): every pad read of a
  session, 18 bytes each. Playback reproduces the user's menus and fight exactly, with or without a window, given
  the same save folder to start from (record with an empty `BT3_SAVES` folder). The movie has to run during
  playback even without a window (its loop reads the pad every frame). `gamedata`-like file, not committed:
  `port/build/session1.pad` (title screen to a duel through READY / FIGHT; READY at vertical blank 1909).
  This is the way to see the HUD in test runs: replays and the demo fight do not show it.
- **Strip of different-looking scenery where an announcement's band was.** Some of the stage's own layers are
  drawn with the destination alpha test (TEST 0x54000: 2,479 triangles at the start of the frame). The copy of the
  alpha bytes that test reads was only refreshed for primitives recorded through `GsGpu_Draw`, not for the
  vertex-program shader paths, so those layers tested against the copy the HUD had made the frame before: the
  band's outline. `date_snapshot` is now called from every draw entry point. Found by: the user's video, then a
  playback of their recording, a difference picture between two frames, and listing the frame's draws that use
  the test. Wrong guesses on the way: post effects (the strip stayed with them off), the band or the word drawing
  colour (their write masks and corners were right).
- Test aids: `BT3_GPU_TAIL=<frame>` prints the frame's last draws as recorded (pipeline key, modes, corners).
- Seen and not handled: the software reference has no destination alpha test (its READY shows at once).
- **In-place polygon clipping overruns its caller's buffer** (found through the demo fight crashing in
  `EftRibbon_DrawStrip`): thirteen draw functions (`EftGfx_DrawPoly*`, `EftSurf_Draw*Clipped`,
  `EftSprAnim_DrawTriClip`, `EftMesh_Draw*TriClip`, `EftWater_DrawClippedFan`, `EftRay_DrawClipped`) clip a
  triangle against five planes in the caller's array, which has room for three corners; the result can have
  eight. On the PS2 the extra corners overwrite the caller's other locals; on PC they reach the return address.
  Under `#ifdef PORT` each now works on a 16-corner copy. The demo fight (`port/run.sh demo`) runs to its end
  without a window; the replay result is unchanged. Any effect near the screen edge could have hit this in a
  normal fight.
- The user confirmed READY / FIGHT in widescreen (bands keep the full width).
- **Stages with animated objects broke the whole picture** (user's `session2.pad`, island stage with water):
  `Vu1Pkt_LoadProg8` measures program 8 as `D_002C3380 - D_002C3080`, and `D_002C3380` is the first symbol of the
  next data file, which the PC linker places elsewhere. The DMA reference got a nonsense length and the list ran
  into unrelated memory (163,000 VIF codes per frame, garbage primitives, 7,000 textures). Fixed under
  `#ifdef PORT` with the real length (0x300 bytes). The other seven programs are measured between symbols of one
  file and are fine. Lesson: a length taken between two data symbols is only valid inside one generated data file.
- While chasing that: texture cache now hashed (8192 entries, palettes hashed per palette instead of per page),
  render targets 24 with reuse of ones idle for two seconds, frame dump prints DMA tags, `BT3_VU_CALLS=1` lists
  program entry points. Program 8 (94 instructions) runs in the interpreter on that stage: 57 runs per frame.
- **Programs 7 (debris) and 8 (animated stage objects) as shaders** (`hle_program78` in gs_vu1.c, layers 4 and 3 of
  `vu0.vert`): 8 is the fighter skinning with per-vertex colour; 7 is rigid (the same matrix for both halves, weight
  forced to 1), alpha from the mesh, and a flagged vertex does not complete a triangle. Checked on the user's
  recording with destructibles (`session2.pad`, frames 2460..2760) against the interpreter (`BT3_VU_INTERP=1`):
  same picture side by side; the pixel diff is not exact because the two runs' screenshots land a frame apart.
  On that stage no call is interpreted any more (4416 served by shaders); work per frame 32 ms -> 19 ms in the
  debris-heavy second. Program 1 (fighter fade) is the only one still interpreted; no recording shows it yet.
- **Display settings**: `BT3_SCALE=1..8` internal resolution multiplier (default 2; render targets are 1024 x 1024
  GS pixels times it, 9 / 36 / 144 MB each at 1x / 2x / 4x), `BT3_DISPLAY=n` (n-th display, listed at start-up),
  `BT3_FULLSCREEN=1` and F11 (borderless full screen, picture centred with its own shape). The window's start size
  no longer depends on the multiplier (896 lines). Checked with the user's recording at 1x and 4x (screenshots
  512 x 448 and 2048 x 1792, same picture, work per frame about the same on an RTX 5080). Not checked by eye:
  full screen and the display choice (started without errors only). The fighters' outline is one target pixel
  wide, so it gets thinner as the multiplier goes up.
- **Settings overlay** (F1; `overlay_*` in gs_gpu.c, font from `port/tools/gen_font.py` -> `overlay_font.h`): Up / Down
  pick a line, Left / Right change it, all live: resolution multiplier (targets dropped and remade between two
  frames), aspect ratio (4:3, 16:10, 16:9, 21:9, 32:9; `Port_SetAspectMilli`, the window follows), full screen, the
  five effect switches, glow strength, music and effects volume (`gPortMusicPercent`, `gPortSePercent`). The
  keyboard does not reach the game while it is open. It replaces the F1..F7 keys (F11 stays). Drawn over the
  finished picture, so screenshots (`BT3_SHOT`) never contain it. Testing hooks: `BT3_KEYS=<frame>:<key>,...`
  presses overlay keys at given frames, `BT3_OV_DUMP=<file.ppm>` writes the panel as painted. Checked that way:
  resolution 2x -> 3x -> 1x -> 4x and 4:3 -> 16:9 during a fight (screenshot sizes and picture). Not checked: the
  panel's appearance in the window itself (no screen capture taken), the volumes by ear. Settings are not saved
  between runs. Gamepad cannot operate it yet.
- **Settings file** (`port/src/plat_settings.c`): `bt3_settings.txt` in the current directory (`BT3_SETTINGS=<path>`,
  or empty for none; git-ignored), lines `name=number`: scale, aspect_milli, fullscreen, fx_off, glow, music,
  effects. Written by the overlay on every change and by F11; read at start, where an environment variable still
  wins. Checked with scripted keys and a scratch file: changed values written, next start came up at the saved
  resolution. Test runs should set `BT3_SETTINGS=` so the player's file neither changes them nor is changed.
- **Settings window** (F1; replaces the text overlay of the entry above, whose font tool and `BT3_KEYS` /
  `BT3_OV_DUMP` hooks are gone): `port/src/gs/ui.cpp` with Dear ImGui 1.92.9b (MIT, `port/third_party/imgui`, SDL3
  + SDL_GPU back ends; the build now has C++: `g++ -m32` in undefined.py, `-lstdc++` in link.py, the library only
  recompiled when it changes). Mouse-driven; tabs Video (resolution, aspect, full screen, display for the next
  start), Effects, Audio, Controls. `ui.h` is the C interface: `GsGpu_GetSettings` / `GsGpu_SetSettings`
  (`PortVideo`) and the bindings of gs_input.c.
- **Bindings** (gs_input.c): per player one key per action (16 buttons + 8 stick directions), one controller
  button or trigger per pad button, and a controller slot (n-th connected controller or none); sticks pass
  through. Saved as `key_p1_cross`, `pad_p1_cross`, `padslot_p1` ... in the settings file. Click a binding, press
  the key or button; Esc keeps, Delete clears. While the window is open the keyboard does not reach the game;
  while it waits for a binding nothing does. Esc closes the window if it is open, else quits as before.
- Testing hooks: `BT3_UI_OPEN=<tab>` opens the window at start on a tab, `BT3_UI_SHOT=<frame>:<file.ppm>` writes the
  window's picture with the settings window on it. Checked that way: all four tabs draw. NOT checked (needs a
  person): clicking, rebinding a key and a controller button, controller assignment with real controllers, the
  second player on the keyboard.
- **Split screen: blocks of noise over both views** (user's `session3.pad`; GPU renderer only, 4:3 and 16:9; the
  software reference was clean). Found with the new `BT3_GPU_CUT=<frame>:<n>` (only the first n draws of a
  frame) and a bisection: the outline's own PS2 passes. `ObjOutline_Draw` builds its edge image in work buffer
  0x150 through a 16-bit view (dropped here, the outline is native), then copies it into the frame's alpha and
  darkens by that alpha. Buffer 0x150 shares its address with the stage textures (tbp 0x2A00); the later passes
  were only recognised as "reads the buffer" if something had drawn into it since the last texture upload. In a
  single view the glare does; in split screen nothing had, so the copy read stage texture data as the mask. Fix:
  a dropped 16-bit-view pass now marks the buffer as written (and stale), so its readers are dropped too.
  Confirmed by the user in play; the replay check is unchanged. Not re-captured by me afterwards.
- **Program 1 as a shader** (`hle_program1`, layer 5 of `vu0.vert`; uniform gained `light2`): the last program on
  the interpreter. It is not a fade seen rarely: it runs every frame for fighters with a part flagged for it
  (user's `session4.pad`: Raditz, 78 runs per frame; on screen it is the scouter's lens). Layer 0 = program 0's
  layer 0; layer 1 = texture from constant 26, coordinates 0.5 + 0.5 * (M * (normal, 1)).xy with M = constants
  10..13; batches whose first normal has 0 in the low 16 bits of its 4th word are skipped. Checked against the
  interpreter (`BT3_VU_INTERP=1`) on that recording: same picture in a close crop of the fighter; frames differ by
  a few thousand pixels at most where the two runs' captures line up. With this, no vertex program runs on the
  interpreter in any recording so far (programs 0, 1, 2a, 2b, 4, 6, 7, 8 are shaders; 5 is unused by the game).

## 64-bit: scoping (2026-10-06, nothing converted yet)

Measured on the port's copy of the game sources (269 files):
- 1,813 pointer members in structures (171 files), 41 of them function pointers; 1,872 pointers inside the game's
  static data (relocations in `port/build/obj_data`); 157 calls of `Res_RelocateOffsets` plus a dozen other
  fix-up functions that turn offsets in loaded files into pointers in place (4-byte slots); 69 allocations with
  literal sizes; 231 integer <-> pointer casts the compiler reports (41 files). No file fails to parse as 64-bit.
- The exact arithmetic depends on `gcc -m32 -msoft-float`, which sends float operations to our PS2-accurate
  routines. gcc cannot do that for x86-64 ("SSE register return with SSE disabled"); **clang can**
  (`-mno-sse -mno-mmx -mno-x87 -msoft-float` emits `__addsf3` etc.). ARM64 has no such mode in either compiler.
- Tried and working with clang 22 on x86-64 Linux: `T * __ptr32 __uptr member` (with `-fms-extensions`) keeps a
  pointer member 4 bytes wide inside a 64-bit program and dereferences transparently; a test structure kept its
  PS2 size. A call *through* such a function pointer crashes the compiler's back end; calling through a normal
  local pointer works. The same declaration is accepted for Windows x64 and ARM64 targets (syntax only).

Approach this points to ("PS2 memory model in a 64-bit program"), not started: compile the game code with clang as
64-bit; declare every pointer that is stored in memory (structure members, globals, tables) 4 bytes wide through
one macro, by script; keep all game memory below 4 GB (non-PIE link, heap and game-thread stack from a low
mapping); leave locals and parameters as ordinary pointers. Layouts, file formats, static data and literal sizes
then stay as they are. Checks available: every structure's size compared between the 32-bit and 64-bit builds,
and the replay result. Open: clang instead of gcc for the game code (does the replay still match?), the
function-pointer call sites, ARM64's arithmetic, low memory on macOS and Windows.
- **64-bit step 1: the game code built with clang, still 32-bit** (`BT3_CC=clang` for undefined.py and link.py;
  objects in `port/build/obj_clang`, program `port/build/bt3_clang`; the gcc build is unchanged and stays the
  default). Result: the replay check prints the same line (`tick 2111 ... hp 23050 0`), and the whole heap at
  that tick (7.1 million words) differs from the gcc build's in 399 words: 246 are addresses (of the program
  image or of host allocations); the other 153 are one field of an array with stride 0xD0 at heap offset
  0x15D149C.., which holds leftover-looking values in the gcc build (0x008D5F71, ...) and small numbers in the
  clang build (0x7D..0x80): a field written from something uninitialised, different per compiler. Not traced
  yet; it does not affect the fight's outcome here, but it is exactly the kind of thing that breaks online
  determinism, so it has to be found before netplay. Needed for it: preprocessed input passed as `-x cpp-output`; `-mno-x87`;
  `-Wno-error=return-mismatch` (three old-style functions); `__unorddf2` (plat_libm.c); and in `StgPanBlur_DrawView`
  two float -> u64 conversions written through u32 under `PORT`, because clang's 32-bit software-float code
  generator stops with an internal error on any float -> 64-bit integer conversion (the 64-bit target compiles
  them). Only the windowless replay was run with the clang build; it has not been played.
- **64-bit build works** (`BT3_CC=clang64 python3 port/tools/undefined.py && BT3_CC=clang64 python3 port/tools/link.py`
  -> `port/build/bt3_64`; `BT3_64=1 port/run.sh ...` starts it; the 32-bit gcc build is unchanged and still the
  default). The replay check prints the same line as the 32-bit build, the heap at that tick differs in 397 words
  (245 addresses, 152 the uninitialised field noted above), and a recorded session (menus, island stage, fight
  with debris and a beam) plays in the window with the same picture, at about 14.5 ms of work per frame where
  the 32-bit build takes about 19. How it is done (nothing in the game sources was edited for it):
  - `port/tools/ptr32.py` rewrites the preprocessed text of the game's files with libclang, three passes:
    (1) every `*` of a declarator or type name becomes `* __ptr32 __uptr` (45,000 lines), so every structure,
    global and local of the game keeps its PS2 size (24,234 records, typedefs and globals compared with the 32-bit
    build: no game type differs); (2) calls through function pointers and pointers in variable argument lists get
    casts; (3) **call arguments are evaluated right to left**, as the game's compiler and 32-bit gcc do and clang
    does not (1,174 calls with side effects in their arguments). Without (3) `BtlKiBlast_GetSpeed`
    (`f(chr, kind, Get(chr, &kind)->speed)`) read `kind` before it was set and clang dropped the rest of the
    function. Text of the port's own files and of system headers is not rewritten.
  - `port/tools/irfix.py` repairs the LLVM IR between `clang -emit-llvm` and `llc`: memory intrinsics and calls on
    4-byte pointers, and addresses in static data (written as `i32 ptrtoint`), which LLVM 22's x86-64 code
    generator or assembly printer reject.
  - Memory (plat_mem.c): program linked at 0x20000000; the game's `main` runs on a thread whose stack is mapped at
    0x60000000; `Port_LowAlloc` (mmap below 2 GB) for what the port hands to the game by address (file handles,
    sound-bank memory); malloc kept in the break area. The game's data objects are assembled again with `as --64`.
  - Port code that read game memory with its own 8-byte pointers: `SifDmaData` (snd_se.c), `sceSifBindRpc`
    (plat_stub.c), `gBtlSeq` (headless.c). `port/build/scope/globals.py`-style checks found no others; more may
    exist on paths not run yet.
  Not done / not checked: sound in the 64-bit build (runs were silent), the movie, saving, the settings window,
  split screen, long play; evaluation order of *operators* (only call arguments are forced); Windows, macOS, ARM64.

## Open: the seam routine (noted 2026-10-06, to do later)

`ObjSeam_TransformVtx` (0x10FFD0, hand-written VU0 assembly; maths described in the header of `src/sys/gfxm_d_c.c`)
has no C version, so in the port it is a stub: a model with seam vertices makes the port print "not implemented"
and exit when it is drawn. It is the only game function without code in the port (`port/tools/undefined.py -v`).
- Scan of the disc (models are entry 2 of each character pack, magic `pmdl`, `seamOfs` at +0x70): 73 of 742 model
  files have a seam section with vertices (12..120 vertices), in 23 character slots. Offsets from file 1423:
  31 35 41 45 51 55 61 65 70 71 74 75 80 81 84 85 90 91 94 95 171 181 191 200 201 204 205 262 266 550 551 554 555
  560 561 562 564 565 566 570 571 572 574 575 576 600 601 604 605 610 611 614 615 690 691 694 695 1011 1015 1020
  1024 1100 1101 1104 1105 1180 1181 1184 1185 1340 1341 1344 1345 (ten files per character: slot = offset / 10;
  `battle_load.c`: model = chara * 10 + costume + BTL_FILE_CHARA, +4 for the variant).
- Not done: tying slots to character names (not plain text in the data; whether the port's file numbers are
  0- or 1-based against BTL_FILE_CHARA = 0x590 is unchecked, so the slot numbers may be off by one file), and what
  seams are for (only 10% of models have them, so "every joint" was wrong). Characters played without the stub
  firing: Goku (Early), Kid Gohan, Raditz.
- To do: write the portable routine from the notes, find one affected character, compare with the software
  reference renderer.

## Setup (2026-10-06)

- `install.py` (repository root, standard library only, Linux): the whole way from a fresh copy of the repository
  and the user's own disc image to a playable build, in six steps that are skipped when already done:
  requirements (with the package to install for each missing tool), disc image (7z; `SLUS_216.78` flattened in
  Python and checked against the sha1 of config/SLUS_216.78.yaml, `DBZP.BIN` against config/DBZP.yaml), game data
  (extract_disc.py), executable data (splat64 0.50.0 from pip into `.venv`, run on both yamls; this is what
  gen_data.py needs and what used to come from the decomp checkout's `asm/`), build (gen_data, undefined, link;
  64-bit by default, `--32`), self-test (the attract demo without a window must print "battle finished"). Writes
  `play.sh`. Output of every tool goes to `install.log`. `--machine` prints one event per line for the window.
- `bt3-setup` (`port/setup/setup.cpp`, built by `port/setup/build.sh`; SDL3 + the vendored Dear ImGui): the same as
  a window. Pick or drop the disc image, a list of the six steps with a spinner, check marks and times, then Play;
  on a failure the reason, the table of missing packages, and a button for the log. It only runs install.py and
  shows its events. Test hooks: `BT3_SETUP_ISO`, `BT3_SETUP_SHOT`, `BT3_SETUP_QUIT`, `BT3_SETUP_ARGS`.
- Checked: install.py on a fresh `git clone` of this repository with the disc image, start to finish (68,611
  files unpacked, 64-bit build, demo fight ran); the window on that clone (all steps, Ready page) and with a file
  that is not a disc image (failure page), by pictures written by the test hook. Not checked: clicking (Browse,
  drag and drop, Play, Cancel), a machine with a requirement missing, other distributions.
- What it is not yet: an installer for people without compilers. It builds from source, so it needs gcc, clang,
  llvm, glslc, SDL3, Python and 7z. A download-and-run release needs the program prebuilt, and the program
  contains the game's own data tables (linked in from the user's executable), so a prebuilt program cannot be
  shipped as it is: the tables would have to be loaded from the user's disc at start instead.
- **A program that can be given away** (`port/tools/strip_data.py`, `Port_LoadGameData` in plat_mem.c): the linked
  program carries 219,504 bytes in 69 data objects taken from the user's executable (the data tables of both
  programs and the VU1 microprograms). The tool finds them through the linker's map (link.py now writes
  `<program>.map`) and symbols with known PS2 addresses, zeroes every run of bytes that equals the user's
  executable (178,906 bytes in 272 runs; what stays are addresses the linker filled in, and zeros), sets
  `gPortDataStripped` to 1 and writes `port/build/release/bt3` and `bt3.dat` (the list of runs and a checksum, 4 KB,
  numbers only). At start, before anything else, the program copies the runs back from
  `gamedata/disc/SLUS_216.78` and `gamedata/disc/BIN/DBZP.BIN` and checks the checksum. Checked: the stripped
  program gives the replay result of the other builds; without `bt3.dat` and without the game data it stops with a
  plain message. A developer's build (no `.dat` next to it, flag 2) is unchanged. The decompiled code itself,
  including tables written as C, is of course still in the program.
- **Release zip** (`python3 port/tools/package.py` -> `port/build/bt3-port-linux-x64.zip`, 4.9 MB): `bt3` + `bt3.dat`
  (stripped as above), `bt3-setup`, `lib/libSDL3.so.0` (both programs are linked with rpath `$ORIGIN/lib`),
  `play.sh`, README, licences. No game data, no build tools needed by the user.
- **The setup window installs natively in a release folder** (`port/setup/native.cpp`): when `bt3` and `bt3.dat`
  are next to it, it reads the disc image itself (ISO 9660), checks the SHA-1 of both programs, writes the loose
  files and splits the AFS archives into `gamedata/` (the layout of extract_disc.py; a marker `gamedata/.installed`
  at the end), and runs the demo fight. Three steps instead of six; no Python, no 7z. In a source checkout it
  still runs install.py. Started again when everything is installed, it opens on the Ready page with Play.
  `bt3-setup --install <disc.iso>` does the same without a window and prints the event lines.
- Checked (2026-10-06, windowless because the desktop reported no displays at that hour): the zip unpacked into an
  empty folder, `bt3-setup --install` with the disc image: 8 seconds, 68,611 files, identical file by file
  (`diff -rq`) to the extraction made by extract_disc.py; the demo fight ran; the replay check from that folder
  prints the same line as the other builds. Later the same night, with the display back: the window on the native path from a
  freshly unzipped folder (first page, the three steps to the Ready page in 8 s, started again when installed,
  and the failure page for a file that is not a disc image), by pictures from the test hook. NOT checked:
  clicking (Browse, drag and drop, Play, Cancel); another machine or distribution.
- Fixed on the way: the developer's heap dump of `BT3_AT` wrote through a NULL file where `port/build` does not
  exist (crash in a release folder); the last event line of the native installer could be dropped.

## Windows (2026-10-06)

- **`BT3_CC=win64`** (toolchain.py holds the variants now): a 64-bit Windows program, `port/build/bt3.exe`.
  Cross-built on Linux with mingw-w64 (`x86_64-w64-mingw32-gcc/g++/as`, clang with `--target=x86_64-w64-mingw32`
  for the game code, SDL3's mingw development package unpacked under `port/build/win`), or built on Windows in an
  MSYS2 shell with that shell's tools. `port/setup/build.sh win` makes `bt3-setup.exe`;
  `BT3_CC=win64 python3 port/tools/package.py` makes `port/build/bt3-port-windows-x64.zip` (bt3.exe, bt3.dat,
  bt3-setup.exe, SDL3.dll, play.bat, README, licences; 4.6 MB, no game data).
- What Windows needed:
  - plat_mem.c: the hardware-register areas and the scratchpad reserved at their PS2 addresses with VirtualAlloc; the
    game heap at its PS2 address if free, else anywhere below 2 GB (a Windows process has its first stack and heap
    there); `Port_LowAlloc` from a low pool; `main` on a low stack if the process's own is above 4 GB (stack switch
    with the TEB's stack fields updated); the program linked at 0x20000000 without relocation
    (`--image-base`, `--disable-dynamicbase`), runtime libraries linked in, `-mwindows` (no console window; output
    goes to the parent terminal or the setup's pipe).
  - plat_crash.c (exception code, address, return addresses to bt3_crash.txt), plat_movie.c (`_popen`, binary),
    plat_mc.c (`_mkdir`), `-D__CRT__NO_INLINE` (the headers' inline maths uses long double, which the
    software-float mode forbids), `#undef __ptr32` in port files (mingw's headers define it away).
- **Two bugs Windows exposed, both real on every platform:**
  - A 4-byte pointer passed on the stack has garbage above it; Windows passes the fifth argument onwards there
    (crash in `Vec3_Add4`). ptr32.py now leaves the outermost pointer of every parameter and return type an
    ordinary 64-bit pointer (except a parameter whose address is taken or that is declared as an array), so
    arguments travel in full whoever is on the other side. irfix.py also turns clang's `bitcast` between the
    two kinds of pointer into `addrspacecast`.
  - `VU1_ADDR_MASK` (0x0FFFFFFF on the chain addresses of the models) and two masks in movie.c did nothing while
    the heap sat at its PS2 address and cut the addresses where it does not: nothing 3D was drawn (the user saw
    the black fight). Under `PORT` they are the pointer itself now.
- Checked under Wine 11: the replay check prints the same line as the Linux builds; the validation replay in the
  window shows the fight correctly (2,202 draws per frame, all vertex programs through shaders); the zip unpacked
  into an empty folder and `bt3-setup.exe --install <iso>`: 68,611 files, identical to the reference extraction,
  demo fight ran (284 s there: Wine's file access is slow); replay check from that folder; the setup window opens
  on the Ready page. NOT checked: real Windows (heap placement, the stack switch and the crash report run for
  the first time there), sound, the movie (needs ffmpeg.exe), the settings window, a gamepad.
- The recorded sessions (`*.pad`) do not replay under Wine: without ffmpeg the opening movie takes a different
  number of frames, so the inputs arrive at the wrong moments.

## A Linux release that runs on other machines (2026-10-06)

The zip made on the development machine did not: "CPU ISA level is lower than required" on another computer.
Three causes, all from building on a distribution compiled for one CPU family (CachyOS, znver4):
- the system's start-up object (`crt1.o`) is stamped "x86 ISA needed: x86-64-v4" and the stamp ends up in every
  program linked there, although our own code has no AVX instruction in it (0 uses of ymm / zmm registers);
- the bundled `lib/libSDL3.so.0` was the system's copy, which really is AVX-512 code (29,991 zmm uses);
- the programs asked for glibc 2.43.
`port/release/build_linux.sh` builds the release in a container instead (`port/release/Dockerfile`: Ubuntu 22.04,
glibc 2.35, clang/LLVM 22.1.8 from apt.llvm.org as on the host, SDL3 3.4.12 built from source there) into
`port/build_release/` -> `port/build_release/bt3-port-linux-x64.zip`. The container has no shader compiler and not
the decompilation's checkout, so the script hands it the compiled shaders and the data tables' assembly sources of
a host build. libstdc++ and libgcc are linked in. `package.py` ends with a check (`portable:`): no ISA stamp above
baseline, no AVX in our programs, no AVX-512 anywhere, glibc <= 2.35.
Checked: the three files need at most glibc 2.34 and carry no stamp; a fresh install from that zip and the replay
check on the host; the demo fight run inside a plain `ubuntu:22.04` container; the setup window and the renderer
start with the bundled SDL3. The Windows zip was never affected (0 AVX in our programs, the official SDL3.dll).
Rule: release zips come from `port/release/build_linux.sh` (and the mingw cross-build), never from a plain host build.

## The movies without ffmpeg (2026-10-06)

- `port/src/plat_mpeg2.c`: the port's own decoder for the opening and the ending (`DATA/ZS3USOP.PSS`,
  `ZS3USED.PSS`). Both are MPEG-2 program streams with one video stream, 512 x 448, 4:2:0, progressive, and
  **every picture is an intra picture** (2,810 and 4,200; made for the PS2's picture decoder), all with
  intra_dc_precision 0, q_scale_type 1, intra_vlc_format 1 (table B-15), zigzag scan, the default matrix, one
  slice per macroblock row. So the decoder is the intra part of ISO/IEC 13818-2 only: program-stream demux,
  headers, slices, DC and AC coefficient tables, inverse quantisation with mismatch control, the reference
  decoder's integer inverse DCT, BT.601 to RGB. Integer arithmetic only; a P or B picture would show as the
  previous one.
- Verified against ffmpeg, picture by picture, on both whole files (`-DMPEG2_TEST` builds a test program that
  writes planar 4:2:0): all 7,010 pictures differ by at most 1 in any sample (the usual difference between two
  inverse DCTs; mean 0.010 and 0.012). The opening decodes in about 3 seconds in total.
- `plat_movie.c` uses it in place of the pipe to the `ffmpeg` program. Seen in the game (64-bit Linux build,
  started from the menus without input): the opening plays after the logos. The sound was already separate (ADX).
- A consequence: the movie now takes the same number of frames everywhere, so recorded sessions (`*.pad`) replay
  under Wine as on Linux (the fight of `session2.pad` starts at vertical blank 2377 on both).

## Building without a disc (2026-10-06)

- `port/data/` (72 files, 424 KB, committed): the game's data tables as shape only. Made by
  `port/tools/make_skeleton.py` from a build with the disc: per table file its labels, `.space N` (N bytes whose
  values come from the disc: 210,773 in 2,388 runs), `.zero N` (zero on PC: padding, addresses of PS2 code,
  .bss), `.long symbol` (1,872 addresses for the linker), and `index.txt` (which of the user's two files each
  table comes from and where; a checksum of all values in order). No value of the game's data is in them.
- `undefined.py` assembles these in place of gen_data.py's real tables when `BT3_SKELETON=1` or when
  `port/build/gen/data` does not exist, and marks the object folder; `link.py` then runs `make_dat.py`, which
  writes `<program>.dat` from the linker's map and port/data and sets the program's "needs its list" flag. No
  comparison with the disc is involved (strip_data.py remains for a program built with the values).
- `port/release/build.sh` -> `port/release/inside.sh` in the container (now also with glslangValidator, the
  mingw-w64 cross-compiler with POSIX threads and SDL3's mingw package): both archives from the repository alone.
  Checked from a fresh `git clone` with no gamedata, asm or previous build: 81 seconds for both; the Linux
  archive installed with the disc image and passed the replay check; the Windows archive passed its self-test
  and the replay check under Wine.
- `.github/workflows/release.yml`: the same on GitHub's machines (artifacts on every push to main, a draft release
  on a `v*` tag). Written and syntax-checked; NOT run yet (it runs only once pushed).
- make_skeleton.py has to be run again (with the disc) when gen_data.py or the decompilation's data split changes;
  a stale port/data shows as a size mismatch in make_dat.py or as a checksum failure at start.

## Names (2026-10-06)

In a release the program is `Tenkaichi3Decomp` (`.exe` on Windows) with `Tenkaichi3Decomp.dat`, the setup is
`Tenkaichi3Decomp-setup`, the archives are `Tenkaichi3Decomp-linux-x64.zip` / `-windows-x64.zip` with a top folder
`Tenkaichi3Decomp/`, and both windows carry that name in their title (after the repository's name, at the
user's request). Inside the build the programs are still `port/build/bt3`, `bt3_64`, `bt3.exe`; messages keep the
`bt3:` prefix, and the files the game writes keep their names (`bt3_settings.txt`, `bt3_crash.txt`,
`bt3_pipelines.txt`).
- **Started by a double click:** a release program (flag `gPortDataStripped` = 1) opens its window when `BT3_GS` is
  not set, and if there is no `gamedata` in the current folder but there is next to the program it works from the
  program's folder (data, saves, settings). `BT3_GS=none` means no renderer; the setup's self-test and install.py
  set it. A developer's build (data linked in) still draws nothing by default. Checked: the Linux release
  started from `/` with nothing set opens the window and writes its files next to itself.

## First run on real Windows, and the getenv cost (2026-10-06)

- The user ran v0.1.1 in a Windows VM with `BT3_GS_VERBOSE=1` (log: `metrics.txt`): Vulkan, mailbox present mode,
  WASAPI sound, menus at 0.2 to 7 ms of work per frame, and a fight (2,233 draws per frame, no interpreter runs)
  at **29 to 31 ms on average**, with 8 of 60 frames over the 33.4 ms budget in the last second.
- Cause, found by running the same replay fight on the development machine with both programs: Linux 8 to 10 ms,
  the Windows program under Wine 16 to 25 ms. `GsVu1_Call` evaluates eight `getenv(...)` per call, about 40,000
  per frame; the C library's getenv is nearly free on Linux and slow on Windows. In the renderer's files getenv
  is now `Port_GetEnv` (gs_internal.h / gs_core.c): one real lookup per name, remembered. After: Windows under
  Wine 8 to 11 ms, Linux 7 to 10 ms.
- The log's "worst 18446744073709.5 ms" and the inflated "n of 60 over budget" in the menus were the statistic,
  not the game: it subtracted the sleep that was ASKED for, and Windows' sleep returns early. `vblank_wait`
  (plat_stub.c) now measures the wait, and on Windows asks for 1 ms timer resolution and waits the last
  millisecond and a half in `Sleep(0)` slices.
- **Pacing bug found through the VM's logs** (`metrics3.txt`: i5 12th gen, 10 logical processors, RTX 3070 passed
  through; fight at 18 to 20 ms of work per frame = game code 6 to 8 ms, display list 11 to 13 ms, submit 3 ms,
  each about 2 to 2.5 times this machine's figure). The new `pace:` line showed 30 of 60 vertical blanks "started
  late": a fight frame waits for two blanks, and when its work took longer than one period (16.7 ms) the first
  wait found its moment passed and `vblank_wait` started a NEW grid from then, so the frame lasted work + 16.7 ms
  (about 37 ms) and the fight ran at about 27 frames per second, unevenly, although the work was within the
  budget. Now a blank that is late by less than 0.1 s is not waited for and the grid is kept. Checked with
  `BT3_BURN_MS=12` (burns that much per frame): frames last 31.9 to 35.2 ms on Linux, 29.6 to 37.6 under Wine.
  This affected every machine that needs more than 16.7 ms per fight frame; this development machine never did.
- Verbose log additions: `time:   of the average: game code / display list (texture decoding, pipeline creation,
  ending and submitting)`, `time:   frames lasted A to B ms`, `pace:` (blank intervals, how many were reached late,
  new grids), and the processor count and memory at start.
- Open: why that VM is 2 to 2.5 times slower in every part alike (suspected: its processors run on the host's
  efficiency cores; not confirmed).

## Speed, first round (2026-10-06)

The development machine (Ryzen 7 9800X3D) had hidden how slow the port was on ordinary processors: the user's
i5 12th gen needs 17 to 21 ms of work per fight frame where this machine needed 8 to 10. Profiled with
`perf record` on the replay fight (uncapped); all measured, results checked identical (below).

| Where | What it was | Change |
|---|---|---|
| `GsVu1_Unpack` (26% of the time) | a general per-component loop for every vector | everything the game sends in a fight is V4-32 without mask or row mode: a plain copy (other unmasked formats get a tight loop) |
| `transfer_data` + `vram_rw` (18%) | each uploaded pixel through the general address function, with a division and a remainder | per-row loops over tables of the place inside a page (`sIn32/16/8/4`) for the formats textures and palettes use |
| `Gs_PageHash` (7%) | a byte-serial 32-bit hash of each touched 8 KB page | four 64-bit lanes |
| vector library (`src/port/vu0_a.c`, `vu0_b.c`) | built with `-fno-builtin`, so each float <-> bits `memcpy` of four bytes was a CALL into the C library (667 call sites), and each multiply three calls deep | `__builtin_memcpy`; the add / multiply core moved to `port/src/softfloat_ps2_inl.h` and inlined |

Replay fight, work per frame on this machine: 8.6 -> 3.6 ms (frame 721), 10.1 -> 4.7 ms (frame 1081); game code
2.7 -> 1.6 ms, display list 5.9 -> 2.0 ms. The Windows program under Wine: the same within 0.3 ms.

Checked: the replay's tick line on all three programs; screenshots every 150 frames of the replay fight (11) and
every 400 frames of `session2.pad` (destructible stage, 34) and `session4.pad` (33) byte-identical to the
program from before the changes.

What is left in the profile, largest first: the software float add and multiply themselves (about a quarter:
`f_mul`, `add_core`; the next step there would be the host's float instructions with the PS2's rounding, which
needs care for determinism), the GIF packet walk, the page hash, the graphics driver (8%).

## Render thread, re-tested (2026-10-06)

- `BT3_GS_THREAD=1` stays opt-in. (The note above blaming it for a camera jump was wrong: that was a separate bug,
  fixed since.) On the user's i5 VM at normal speed it ran menus and a fight correctly, the game's side of a frame
  dropping to 3.5 to 4.1 ms; frames it draws are byte-identical to unthreaded ones (replay fight and demo fights),
  except 4 of 121 demo-fight frames that differed in the rightmost one or two pixel columns.
- It is NOT safe yet: played back uncapped (`session5.pad`, split screen), the threaded program stops at frame
  2010, where the fight would begin, interpreting garbage as a vertex program. `BT3_GS_RACE=1` (new: hashes the
  list's blocks at hand-over and again after drawing) reports 12 frames between 489 and 2015 in which the game
  rewrote the list while it was drawn (the list "had N blocks when handed over and 1 after"), all in menus and
  loading, none in the fight frames it reached. At normal speed the drawing is done long before the game gets
  there; on a slow machine it would not be. Fix to try: hand the render thread its own copy of the list.
- The old pad recordings (session1..4) no longer replay: they stall at the title screen (start-up changed since;
  cause not found). `session5.pad` was recorded with `BT3_NOMOVIE=1` and is played back with it.
- The speed changes of v0.1.4 re-checked on session5 (menus and a split-screen fight): 62 of 62 screenshots
  byte-identical to the code before them.

## Speed: what is left for later (2026-10-06)

Speed is good enough for now (the user's i5 12th gen uses 8 to 10 ms of the 33.4 ms a fight frame has). Not
started; in the order I would take them:

1. **Render thread by default** (`BT3_GS_THREAD=1`): the largest gain left (the game's side of a frame fell to
   3.5 to 4.1 ms on the i5). Blocked by the race above; to try: hand the thread its own copy of the frame's list
   (guess: about 1 ms per frame, not measured). Afterwards also find the 4-of-121 edge-column difference.
2. **Draw order of the two-layer character pieces**: each piece is drawn twice back to back (base, then shading:
   pipelines 13 / 14 alternating), so nearly all of the 2,170 draws change pipeline, texture and matrix. Issuing
   them costs 1.7 to 2.3 ms on the i5 (0.5 to 0.8 here). Sending each layer as one run would cut that, but can
   change the picture where pieces overlap: only with screenshot comparison on every recording.
3. **Float add / multiply with the processor's instructions** (about a quarter of what is left, all game code):
   only after netplay's per-frame state hash exists to catch a mismatch between machines.
4. Smaller: the GIF packet walk, the page hash (hash only what a texture lookup needs).

Still to measure: a destructible stage and the big attack effects (no working recording), integrated graphics,
and a processor from before 2015.

## Seam models: first look at who has them (2026-10-06)

- New test switches: `BT3_DEMO=1 BT3_CHARA=<id> [BT3_COSTUME=<n>] [BT3_VARIANT=1]` puts that character on side 0 of
  the demo battle; `BT3_SEAM_SKIP=1` makes the missing routine return instead of stopping the game (the stub is
  now `port/src/plat_seam.c`).
- With them, the routine was reached for character ids 55, 56, 57, 110 and 134 (costume 0), which agrees with the
  scan's offsets 550.., 560.., 570.., 1100.., 1340.. read as `chara * 10 + costume` (0-based). Seen on screen:
  56 is Videl; 57 wears a cape and turban (Great Saiyaman, by the look: not confirmed by name).
- Not reached within 12 to 20 s of the demo for the other 18 ids of the scan (3..9, 17..20, 26, 60, 61, 69, 101,
  102, 118): cause unknown (the costume numbers I passed, or the seam is only drawn in some states). The demo's
  opponent is a Super Saiyan Goku, so screenshots alone do not tell those apart.

## The seam routine is written (2026-10-06)

- `ObjSeam_TransformVtx` (0x10FFD0) is now `Ref_ObjSeam_TransformVtx` in `src/port/vu0_a.c`: the 128 instructions
  translated one for one onto the library's per-instruction helpers (new: `op_vftoi`, `op_vmfir`). No game function
  is a stub any more. `port/src/plat_seam.c` and `BT3_SEAM_SKIP` are gone again.
- Check built in: `BT3_SEAM_CHECK=<path of SLUS_216.78>` executes every call a second time with a small interpreter
  from the instruction words of the original program and compares everything both write (the vertex, the work
  block, all vector registers, ACC, Q, the status flags). Demo battles with characters 55, 56 (Videl), 57, 110,
  134: 0 differences (56: over 40,000 vertices on each of the three programs; 57: over 20,000). The interpreter
  uses the library's arithmetic, so this proves the translation, not the number model.
- One thing the bytes do not say: `qmtc2 $v0, vf19` moves 128 bits and the upper 64 of `$v0` are whatever was there;
  both versions put 0 into vf19.z / w (only x and y are read here).
- Seen: Videl (first costume) is drawn completely in the demo battle. Not compared with the software reference
  renderer or with a console picture yet.
- **Cheats tab** of the settings window (2026-10-06), "Unlock everything..." with a confirmation: calls the game's
  leftover debug function `Save_UnlockAll` (0x266088; no caller in the game) on the save in memory: all characters,
  stages, music, items, unlock flags, maximum Zenni; it also empties the records list. Requested by the window,
  carried out at the next vertical blank on the game's side (`Port_Trace`). The user tried the function itself
  (first on a bare F9 key, removed again): it works.

## Windows: the program was movable (2026-10-06)

- Report from a second Windows machine (same hardware as the user's): the setup unpacked the data, the self-test
  failed, `install.log` empty. With the setup's new exit-code line: 0xC0000005, and no `bt3_crash.txt` even with the
  crash report as a vectored handler. So: a crash before any of the port's start-up code had installed anything.
- Cause (fits everything; the machine's setting itself was not confirmed): `bt3.exe` was linked with
  `--disable-dynamicbase` but still had a `.reloc` section, and a Windows set to "Force randomization for images
  (Mandatory ASLR)" moves any program that has one. The first write of `Port_LoadGameData` then goes to
  0x20000000-based addresses where nothing is. Linked with `--disable-reloc-section` now ("relocations stripped"):
  **with that build the same machine runs the setup and the game.**
- Also new: a check at start (`GetModuleHandle(NULL) != 0x20000000` -> a plain message naming the setting, exit 4);
  the crash report is a vectored handler written without the C library, installed with constructor priority 101,
  with registers, module names and `gPortStage` (the last start-up step); stdout / stderr unbuffered on Windows;
  the setup's `install.log` ends with the game's exit code and includes `bt3_crash.txt`.
  Test switches: `BT3_CRASH_TEST=1`, `BT3_STACK_TEST=1` (forces the move to a low stack).

## Texture packs: how PCSX2 names a replacement (found 2026-10-06, nothing implemented yet)

Worked out against a user's 18,700-file DDS pack for this game and a dump of the textures the port decodes
(menus and a fight, `session5.pad`): 320 of 347 paletted textures have a file in the pack under this rule.

File name: `<texture hash>-<palette hash>-<bits>.dds`, the hashes in hexadecimal WITHOUT leading zeros, bits as 8 digits.
- texture hash = XXH3 (64 bits, seed 0) over the texture's raw 256-byte GS memory BLOCKS, block after block in
  row-major block order over the texture's rectangle (a block is 16 x 16 pixels of 8 bits, 32 x 16 of 4 bits):
  the data as it lies in GS memory, not unswizzled. (Seen from two blank textures of different formats sharing
  one hash; unswizzled pixels match nothing but blank textures.)
- palette hash = XXH3 over the palette's 32-bit entries, in index order: for most files only from the lowest to
  the highest index the texture uses; for 63 of the 320 the whole palette (16 or 256 entries) matched and the
  range did not (which rule applies when is not worked out; both can simply be looked up).
- bits = PSM | log2(width) << 6 | log2(height) << 10 | TCC << 14 (the TEXA fields above are zero for these).
- One texture comes with many palettes in the pack (Goku: 127 files, few texture hashes): the game paints into
  the palettes (tint, flash, dimming), so a pack never has all of them: 3 of the 347 had a known texture hash
  with an unknown palette.
The replacements are DXT5 at 4 times the size (512 x 512 for a 128 x 128 original).

## Texture packs implemented (2026-10-06)

- `port/src/gs/gs_texpack.c` (index of the `textures` folder or `BT3_TEXTURES`, PCSX2 names, DDS reader with mip
  levels), xxHash vendored in `port/third_party/xxhash`; looked up in `texture_get` on a cache miss; switch in
  F1 > Video (`texture_pack` in the settings file). `BT3_TEX_LOG=1` prints each lookup as found / missing with the
  name it needs. Without a pack the picture is byte-identical to before (6 of 6 screenshots of session5).
- What a replacement needs that an original does not (each found on the user's pack, each seen fixed):
  1. **Filtering**: always sampled with filtering (the game asks for nearest on text and 2D art).
  2. **2D art**: the per-GS-pixel sampling of gs.frag (misc.w) would show it at the original's resolution. A
     replacement is sampled per output pixel instead (misc.w negative), limited to the rectangle of the sheet the
     piece shows on the console (worked out from the primitive's vertices in `GsGpu_Draw`; rect / orig uniforms).
     Limiting per original texel left steps on stretched logos; no limit brought back the lines along HUD panels.
     A mirrored piece starts exactly on the boundary to its neighbour: the boundary does not count as inside.
  3. **No mip levels for 2D art** (samplers 8..15 have them, used for 3D only): a smaller copy averages
     neighbouring pieces of a sheet.
  4. **Alpha test**: compression moves 0x7F to 0x7C..0x7E and filtering fades cut-out edges, so a test against the
     exact value dropped most of a tree's leaves: for a replacement the line is at half the reference (mode.x = 3).
  5. **Bit 7 of the written alpha** (the health bar): its layers are drawn with the destination alpha test, each
     writing its texture's alpha; the original stays at 0x7F, the pack's redrawn sheet reaches 0x80..0x8E near
     one end, so the first layer set bit 7 there and the next was not drawn (seen in 16:9, by luck not in 4:3).
     A replacement's alpha is kept at or below the ORIGINAL's largest (`TexPack_Lookup` returns it; orig.z).
- PNG is read too (SDL_LoadPNG; mip levels made by averaging), checked with nine of the HUD files converted from the DDS pack.
- Not done: textures smaller than one block; loading in the background (files are read when first needed);
  choosing between a pack's alternative folders (first name found wins); a pack on the Windows program has not
  been run.
- A user's PNG pack (1,501 files, 1.8 GB) run on session5 in 16:9: 1,497 indexed (4 with other names skipped), 17
  files used, picture correct. Loading is felt: 404 ms for a frame with 38 new textures, 231 ms for one with 20
  (PNG decoding and the mip levels, about 10 ms a texture). Loading in the background (the original shown until
  the replacement is ready) would remove that; **left as it is for now by the user's decision (2026-10-06)**: a
  short freeze instead of a visible swap.
- The Windows program under Wine, session5 in 16:9: the DDS pack (557 lookups found, 70 missing) and the PNG pack
  (55 found) both load and draw correctly; no crash.

## An OpenGL 3.3 renderer beside Vulkan (branch `opengl`, 2026-10-07)

From pull request #1 (its renderer commits only, on top of the main line with online play; its stages / songs work
is not in here). The draw stream is recorded by shared code (`gs/gs_draw.c`) and replayed by one of two back ends:
Vulkan through SDL GPU (`gs/gs_gpu.c`) or OpenGL 3.3 core (`gs/gs_gl.c`).

- Choice: F1 -> Video -> Renderer (setting `gpu_api`, 0 Vulkan / 1 OpenGL; from the next start). `BT3_GPU_API=
  vulkan|gl` wins. Without a name asked for, the other renderer is tried when the chosen one cannot start.
- The main line's changes carried into the refactor: the seven returns for frames that are only re-run
  (`gPortResim`) and `GsGpu_PagesMoved`, now in gs_draw.c; the overlay is built by one function for both
  (`frame_build` in ui.cpp: settings, the online window, the line over the picture), drawn by ImGui's SDL GPU or
  OpenGL3 backend.
- **Verified** (session6: menus and three moments of a split-screen fight, blanks 1500 / 2700 / 4500 / 6500):
  Vulkan on this branch writes the same screenshot files as the main line and runs as fast (9,210 and 9,131
  blanks in 30 s uncapped against 9,183 and 9,131). OpenGL writes the same four files as Vulkan, on Linux.
  The replay's result on all three programs. The overlay and the online window open under OpenGL (looked at).
- The pull request's OpenGL picture was darker in fights: its vertex wrapper clamped clip-space z before the
  divide and kept GL's -1..1 depth range, so stored depth was wrong, and the depth tint that feeds the glare and
  the distance blur with it. Fixed with `glClipControl` (or the same mapping by hand without the extension: a few
  hundred to a few thousand pixels then still differ) and depth clamp.
- The pull request's slower Vulkan (29% in fights) was not the renderer: its stages work loads the full stage
  model in split screen instead of the lighter split one (battle_load.c). Not taken.
- **OpenGL is slower**: 7,931 blanks in the same 30 s (14% fewer over the whole session, more in fights). A
  profile puts 47% of the processor time inside the graphics driver, against 14% with Vulkan: the cost is the
  number of GL calls per draw, not the graphics card.
- On a real Windows (the user's VM, archive of commit 2490fd5): "it opened fine" with OpenGL chosen. Under Wine the
  same program stops inside the creation of the OpenGL window (`BT3_GL_TRACE=1` shows how far it gets), so that
  is Wine's.
- **Not checked**: the picture on Windows against Vulkan's (by eye or by file); graphics cards other than the two
  NVIDIA ones.
  The line over the picture and a whole online match under OpenGL were not run. `BT3_SHOT_VBLANK` is not
  implemented in gs_gl.c (`BT3_SHOT` with `BT3_SHOT_FROM` / `_TO` is).

- 2026-10-07, from release 0.1.10: the archives no longer have `play.sh` / `play.bat`. They set `BT3_GS=gpu` and
  the current folder, and a release program does both itself (it opens its window without the variable, and
  works in its own folder when started from another: plat_mem.c). The program is started directly.

## Where a fight's frame goes, and four things taken out of it (2026-10-08)

Measured on the user's machine (Ryzen 7 9800X3D, RTX 5080) with the recorded two-player split-screen session
(`session6.pad`), uncapped, the port's own timing (`BT3_GS_VERBOSE`), frames 2400 to 5400:

| | before | after |
|---|---|---|
| Vulkan, work per frame | 4.84 ms (game 1.5, drawing 3.3 of which submitting 1.7) | 4.30 ms (game 1.5, drawing 2.8) |
| OpenGL, work per frame | 6.85 ms (game 1.5, drawing 5.3 of which submitting 3.7) | 6.32 ms (game 1.5, drawing 4.8) |
| nothing drawn (`BT3_GS=none`) | 1.6 ms per blank | not measured again |

The budget is 33.4 ms. One frame of the session is over it: 51 ms at the fight's start, where 116 textures are
made at once. Of the game's own code more than half is `Port_FastAdd` / `Port_FastMul` / `Port_FastMtxApply`.
OpenGL's extra time is all inside the driver (48% of the process's time against 14% with Vulkan).

Found with `perf` and with a preloaded `memcpy` that counts by caller (the call stacks of `perf` and of `gdb`
attached from outside were not available: `ptrace_scope` 1), and changed:

- The vertex programs' strips were stored as whole triangles, every vertex three times: 11 MB a frame, built
  with one 48-byte `memcpy` per vertex and then copied to the graphics card (half of all bytes copied in a
  frame). Now each vertex once, and the triangles as 32-bit indices (`gsVuIdx`; a draw's `first` / `count` are a
  range of indices; `SDL_DrawGPUIndexedPrimitives`, `glDrawElements`).
- `vif` copied every command's data to a buffer before handing it on (a fifth of all `memcpy` calls); now used
  where it lies when it is all there.
- `getenv` inside drawing: each place remembers its answer (the search in `Port_GetEnv` was 3%).
- The hash of a page of GS memory takes 16 bytes per multiplication where the compiler has 128-bit integers
  (it was 7%: the game sends megabytes of textures to the same addresses in every frame, and all is hashed).
- `plat_libm.c`: `__builtin_memcpy` (with `-fno-builtin` each 8-byte copy of the double helpers was a call into
  the C library, a third of all `memcpy` calls).

Checked: 16 screenshots through the session (`BT3_SHOT=400`) byte-identical to those of the program before the
changes, on Vulkan and on OpenGL, and from the 32-bit program; the replay check on the three programs; the
Windows program through the session on Vulkan under Wine (fight values as always).

Not done, in the order of what is left in the profile (Vulkan): the per-pixel loop that writes an uploaded
texture into GS memory (`Gs_Gif`, 7%); `draw_state` (4%); `feclearexcept` in libm, 2%, caller not found; the copy
of the vertices into the transfer buffer could be saved by recording straight into it.

## Is the data the original disc's? (2026-10-10)

- Why: a report of a broken skeleton turned out to come from a modified disc image (a romhack), which nothing on
  the screen said. The installer checks the two programs only; modified DATA installs and runs.
- `port/src/plat_verify.c`: at start a background thread compares every data file (the three archives' 68,601
  entries and the four streams of DATA/) with `port/src/plat_verify_tab.h` (size and CRC-32 of the original's,
  made by `port/tools/make_verify.py <disc image>`; checksums only). A file in `<root>/mods/` counts as replaced.
  When something differs: a log line with the numbers and the first four names, the window title becomes
  `Tenkaichi3Decomp [modified game data]`, the notice line is shown once, and a crash report carries the line.
- The result is kept in `<root>/.verified` with a signature of every file's size and time (since 2026-10-13 also
  the time of its `mods/` twin: an edited mod of the same size used to keep the old result); a later start only
  looks at those. Measured here: under 4 s for the full pass (files in the system's cache), under 1 s after.
- A quick pass first: sizes only, no reading. A replaced, missing or resized file is reported at once ("at least
  N files") and nothing is read; a romhack's data stopped the game in its first seconds, before the full pass
  would have ended. A crash report written while the check runs says "not checked yet".
- Texture packs, added songs, stages and characters are outside the data folder and do not count.
- `BT3_VERIFY=0` turns it off; without a window (`BT3_GS=none`) it runs only with `BT3_VERIFY=1`.
- Checked: the original data (clean); a copy with one byte changed in a file, another file longer and a third
  missing (3 of 68,605 named, both on the full pass and from the cache); the replay check unchanged. NOT seen:
  the title and the notice on a real window (screenshots of the offscreen renderer are taken under the overlay).

## In-between pictures: a shiny model's reflection flickered (2026-10-11)

- Seen by the user on characters with a reflection (Perfect Cell's black parts) with "interp" on; reproduced from
  a pad recording in the offscreen renderer, pictures written one by one (`BT3_SHOT=1`).
- The reflection is program 1's layer 1 (`misc.w == 5`), drawn over the same triangles as layer 0. In
  `interp_build` layers 0 and 1 got `interp_bone` (axes given back their length, the place moved around the
  camera) and every other layer the plain average of the matrices, so in the in-between pictures the reflection's
  triangles were not where the layer under them was: depth fighting, the shine there in one picture and gone or
  in shreds in the next. Layer 5 goes through `interp_bone` now.
- Checked: the same six consecutive pictures before and after; after, the shine is whole and the same in each.

## Texture packs: broken letters and logos, text that tears while it moves (2026-10-11)

Reported by a player on 0.1.22 with a pack: "some things are pixelated". Seen with the 18,704-file pack: the
publishers' logos had steps along their edges, the copyright text had letters with columns missing ("righ:s",
"SH.JEISHA"), and the main menu's entries tore while they glided. Without the pack the same screens were clean.
Two faults of the rule a replacement is sampled by as 2D art (gs.frag, mode 3):

- **The texel was chosen on its boundary.** The rule takes the original texel the GS pixel shows (`floor(at *
  size)`) and shows the matching part of the replacement. Where a piece has whole-texel coordinates, `at` lies ON
  the boundary between two texels, and its rounding chose one side or the other from pixel to pixel. Now a 64th of
  a texel in the piece's direction decides, as where the piece's rectangle is worked out (GsGpu_Draw).
- **The picture was held to the GS's pixel grid.** A piece that glides by parts of a GS pixel (the main menu's
  entries) moved smoothly as geometry while each letter's picture stepped a whole GS pixel at another moment. A
  replacement now takes the rule of the game's own smooth 2D art: the plain coordinate of each output pixel, held
  inside the piece's rectangle of its sheet. This does not depend on the setting "2D filtering": sharp means the
  game's own art as blocks of a GS pixel, which a replacement of several texels per GS pixel is not.
  `BT3_TEX_GRID=1` gives the grid rule back, to compare.

Checked: frames of the boot and the main menu with the pack, before and after, smooth and sharp, beside the
frames without the pack (windowless, the processor's own Radeon, Vulkan); the recorded fight ends as before. The
user saw on screen: the logos, the copyright text and the menu's motion right in both settings, the fight's HUD
unchanged in sharp. Not checked: widescreen with a pack; the OpenGL renderer with a pack.

## The settings window opens from a controller (2026-10-11)

Asked for by the user. Until now only F1 opened the settings window, and with it open only the keyboard was kept
from the game.

- **START + SELECT** of any connected controller opens and closes it (the controller's own two buttons, whatever
  the game's bindings are). `PortInput_Hotkey` (gs_input.c) decides for both renderers' event loops.
- **A row "Open this window"** heads the Controls list: a key (F1) and a controller button (none) of the settings
  file (`key_overlay`, `pad_overlay`). A cleared key is F1 again, so a keyboard always has a way in; a trigger
  is not taken as the button. START + SELECT works beside a bound button.
- **The window is worked with the controller** (Dear ImGui's gamepad navigation), and **nothing of a controller
  reaches the game while it is open**, as with the keyboard. The buttons that opened or closed it do not reach
  the game until they are let go (START would pause).
- **Found by the user: pressed in the same moment, the two did nothing.** Both "down" events come in one round and
  each already finds the other button held, so the window opened and closed again at once; only "hold one, press
  the other" worked. The pair counts once per press now (a latch, cleared when either is let go).

Known: START pressed a moment before SELECT reaches the game first (a pause in a fight). With the controller,
Cross on a binding starts waiting for a button and the next press is taken; Esc on the keyboard leaves it as it
was, and there is no way back from the controller yet (the user bound SELECT alone this way without meaning to).

Checked by the user on screen (Linux): START + SELECT opens and closes it, pressed together. Not checked by
anyone: Windows; a second controller; the online screen with a controller.

## A day of reports: the window with a controller, leaving the game, volumes, V-sync, slow machines, depth (2026-10-11)

**The settings window.**
- **Esc asks first.** Outside the window Esc closed the game at once; it now puts up "Close the game?" (Exit /
  Cancel, Cancel has the focus, Esc again takes it away). The window's own close button still closes at once.
  `Ui_QuitAsk` (ui.cpp), called from both renderers' event loops.
- **"Exit game"** on a last line of the window that every tab shows; the same question.
- **Worked with a controller:** Circle / B closes the window (and the online screen, as its close button),
  L1 / R1 go to the neighbouring tab. Any connected controller works it, one plugged in later too (the library
  only took the first one found at start: `ImGui_ImplSDL3_SetGamepadMode(AutoAll)`, and added / removed events
  are passed on while the window is closed).
- **A binding taken from a controller pressed its own row again:** the button that became the binding was also
  "choose" for the row it was bound in, so the row waited for a button a second time (how the user bound SELECT
  alone without meaning to). The controller does not work the window while a binding is waited for, nor until
  its buttons are let go (`sPadHold`). A binding waited for from a controller ends by itself after five seconds.
- The button that closed the window or answered the question does not reach the game (`PortInput_Swallow`).

**Music turned the voices down.** Every ADX stream took the Music slider: the songs, but also the voices, the
stage's background sound and the streamed effects, while "Sound effects and voices" only reached the short
effects. The game has one player per kind (`BGM`, `MAP`, `SE`, `SE`, `VIC`, `VIC`, docs/game_overview.md); seen
with BT3_SND_VERBOSE on a recorded session: intro, menu and fight songs start on player 0, the stage's sound on
1, effects on 2, a fighter's and the announcer's lines on 4 and 5. Player 0 takes Music, the others the other
slider, and moving that one reaches the streams that are playing at once. Not listened to by me.

**V-sync** (F1, Video; off by default, setting `vsync`). The port shows a picture without waiting for the display
(MAILBOX, else IMMEDIATE, which tears: some Windows drivers, the OpenGL renderer). The option asks for the wait.
Checked windowless: the setting switches the mode ("present mode vsync"), the game runs the same. Not checked: on
a display, by anyone.

**Smooth motion on a machine that cannot keep up.** Reported: "the fighters go 2D and glitch". The order of the
pictures does not change when a machine is slow (looked for and not found: the tick's real picture is always
shown and remembered). What happens: every in-between picture costs what a real one does, and they were shown
whatever they cost, so the game itself slowed down: on an integrated GPU at 8x and "240", a picture every 186 ms.
An in-between picture's faults (a part left behind, a blend of two far poses) are on screen for a few
milliseconds on a machine that keeps up, and for 186 there. Now `interp_cost` (gs_draw.c) measures what showing
a picture takes and allows as many per tick as fit in 70% of it (8, 4, 2 or none; more again when one step more
would take under half). Measured on that GPU with a recorded session: at 8x / 240 the in-between pictures stop
after the first tick (the fight then at about half speed: 8x is too much for it without them too); at 2x / 60
every tick keeps its in-between picture and the limit never acts. BT3_INTERP_NOCAP=1 switches it off. The
faults themselves are not fixed by this.

**A triangle of a waterfall missing, another one as the camera moves** (Ruined Earth, found by the user). The lower
fall is drawn by the game itself (vertices it transforms, whole-number depth) in the plane of a rock face that a
vertex program draws. On the PS2 both are whole numbers at every pixel: they tie, and the fall, drawn later,
passes the GEQUAL test. Here the rock's depth kept its fractions, so the rock was a hair in front for this
triangle or that one. Rounding only at the vertices (first try) was not enough: between them the fractions were
back, the triangle came back with the camera's movement and other stage textures flickered (found by the user).
Now as the GS does it: gs.frag writes the depth rounded down to a whole number (`extra.x` = the depth buffer's
largest value; not for 32 bits), and every vertex hands over its whole number plus a half (vu0 / vu4 / vu6.vert,
`put` in gs_draw.c) so that nothing interpolated sits on the edge between two. Checked on the user's recording,
windowless at 2x: the fall is whole in 57 samples of the stretch where the camera moves, and against the build
before, the only pixels that differ in the mapped frame are the repaired triangle. The user, on screen: "looks
like it's fixed". Not measured: the cost (the shader writes depth; declared `depth_less`, which the OpenGL 3.3 renderer
has not and drops).

**Not a fault: a glow cut in a straight line by the ground.** An explosive wave or a charge on the ground tints
the scenery up to a line at the fighter's feet; in the air there is no line. The tint is one flat, camera-facing
picture at the fighter's depth (seen with the new BT3_GPU_DUMP: six vertices of one depth, drawn with the depth
test on), and ground nearer than the fighter hides it. PCSX2 shows the same (checked by the user).

**Tools.** `BT3_GPU_DUMP=<frame>:<n>` prints every vertex of the frame's draw n (gs_gpu.c). A recorded session
(BT3_PAD_PLAY) replayed with `BT3_UNCAPPED=1` gave the same pictures as at real speed (two frames compared
byte for byte) and reaches a fight in six seconds: with BT3_GPU_CUT that makes a search for "the draw that
paints this" a matter of minutes.

## Windows: a crash when an online match starts, from files of two releases side by side (2026-10-11)

Reported by the user from a Windows copy of 0.1.24: joining crashes, "exception 0xC0000005 ... msvcrt.dll, writing
address 0x203364B0", the game running, an empty backtrace.

- **The address is code in 0.1.24** (.text is 0x20001000 to 0x20338ED8 there) **and is the first line of
  `Tenkaichi3Decomp.exe.mem` of releases 0.1.16 to 0.1.18** ("203364b0 1f4": checked in each release's Windows
  package; 0.1.24's own list begins at 0x20339500 and fits its program). So that copy had the new program next to
  an old list. The list says where the game's variables lie in the program (make_state.py, written at link time);
  a state put back (the start of a session, a rollback) is copied to those addresses, here over code, which
  Windows does not allow. Nothing reads the list before the first state is taken, so the copy played offline
  without a sign of it.
- **Now the list says which program it is for:** two first lines, "0 <address of gPortResim>" and
  "1 <address of gPortVBlanks>". state.c compares them with its own two addresses; if they differ (or are not
  there: an old list) the list is not used, the log and a line over the picture say that files of two releases
  are mixed, and online play goes on the way it does without a list (the program starts again for the session,
  no rollback).
- Checked on Linux, two copies on one machine through the room-code lobby: with the right list they connect with
  rollback; with a list whose first line was changed they connect without rollback and print the message, no
  crash. Not checked: on Windows, by anyone; how that copy came to have an old list (only the .exe replaced is
  the guess).

## Online: the opponent's aura showed on what he stood behind (2026-10-11)

Found by the user in online matches, and again offline with `BT3_VIEW=0` in a 1P vs 2P battle: while the opponent
charges behind a tree or a building, a soft dark-blue oval sits on it, where he is.

- **What it is:** one camera-facing sprite of the aura (the dark layer under its glow, subtracted from the
  picture), sent with the nearest depth there is (z = 0xFFFFFF: seen with BT3_GPU_DUMP). `EftBill_Draw`
  (src/battle/eft_z.c) draws a billboard that has EFT_BILL_FRONT in front of everything, and only "when its
  fighter is in view": `BtlScene_IsCharInView`. In a one-player battle that is the camera's fighter; in the two
  halves of a split screen, each half's own fighter. So the layer is only ever drawn around the fighter the
  camera stands behind.
- **Why online:** an online match is the two-player battle with the local player's view shown full screen
  (`BtlCam_UpdateOverride`). The game takes that for its own "one view of a split-screen battle" (a cinematic),
  where `BtlCharApi_IsCamShown` says yes for both fighters when neither has camera priority. The opponent's
  layer was drawn too, in front of whatever hid him. In a real split screen it is not (compared on the same
  recording).
- **The fix** (src/battle/btl_scene.c, `#ifdef PORT`): in that case, and only when the game itself gives no view
  priority, a fighter is in view if he is the view's own (`objId == Port_NetView()`), as in the two halves. All
  nine callers are drawing routines (EftBill, EftPtcl, EftQuad, EftChain, EftRay, EftSpdLine, EftBolt, EftGlow,
  EftAnimPart), so the two copies' game states are not touched by which view each shows.
- Checked on the user's recording, windowless: the oval is gone and 158 pixels changed, all of them the oval;
  the charging player's own view and the split-screen picture are the same byte for byte as before; the
  regression replay passes. Not checked: in a real online match, by anyone (a host and a joiner connect as
  before, but that test does not reach a fight).

## In-between pictures: the wall at the edge of the map, a waterfall a step to the side; depth again (2026-10-11)

**The wall at the edge of the map tore in in-between pictures** (found by the user, flying up and down along it).
It is not scenery: `EftBound_BuildWall` (src/battle/eft_g.c) builds it anew every tick as a patch of 6 x 4 points on
the map's cylinder, centred on the fighter's bearing and height, with a texture laid out in the world that also
scrolls, and its triangles reach the GS cut off at the edges of the view, differently every tick. As pieces moved
by the camera's change, some passed the questions asked of a piece and some did not. Two attempts on the pieces
were worse and were taken back (told by the user in one case, seen in captures in the other): "none of a kind
moves if one cannot" (the wall then stood still against what moved behind it) and "all move if most can" (the
pieces that fail are really computed wrong: parts of the wall flew off).
- Now the game hands the patch over (`Port_WallMesh`, called from EftBound_DrawMesh / DrawMeshCulled under
  `#ifdef PORT`: the 24 points, texture coordinates, colours, the quads, the view's matrix), and `interp_wall`
  (gs_draw.c) draws the patch part of the way between the two ticks', through the tick's matrix and the camera's
  change for that part (sInterpDt), cut at the view's edges as the game cuts it (the GS shades a triangle
  straight across the screen, so where its corners are decides the colours: left whole, the wall was brighter
  towards the top). The tick's own wall triangles are taken out of that picture.
- Patches come in a vertical blank before the list they belong to is shown, so they are kept in a ring and a
  picture's are found by the view's matrix.
- Two fighters at the same stretch: two patches in one surface, and in the game which one a pixel shows on top
  falls to the last bit of depth, triangle by triangle. Here each patch is drawn a few depth steps in front of the
  one before, and with in-between pictures on, the tick's REAL picture gets the wall drawn this way too
  (`interp_wall_real`; BT3_INTERP_WALLGAME=1: the game's own). So with Smooth motion on, the wall's shade where
  two patches overlap is not the original's (on Rocky Area up to 85 of 765 in a pixel, hard to see side by side);
  with one fighter at the wall a handful of pixels differ; with Smooth motion off nothing is touched.
- The test that was promised to the user: the patch drawn at the tick itself (BT3_INTERP_WALLTEST=1 with
  BT3_INTERP_ONLY=1 and BT3_INTERP_T=1.0) against the real picture shown (BT3_INTERP_REALONLY=1): the same in
  four frames of two stages (three identical, one with 2 pixels off). Checked by the user on screen.
- A side finding: saving a picture every frame made `interp_cost` think the machine slow and the in-between
  pictures stopped (captures of "in-between pictures" that were the real ones). BT3_INTERP_NOCAP=1 for captures.

**A distant waterfall a camera's step to the side for one in-between picture in about 25** (found by the user,
moving sideways; shown on a video of his screen, frame 47 of 100). The fall's water is a few textures shown in
turn. A piece's kind has its texture in it, so in the tick the texture changed the fall was a kind that had not
been there: nothing of it was found in the previous picture and all of it stayed where the tick has it.
`interp_kinds_follow` gives the previous picture's pieces of a kind that is gone the name of the new kind that is
drawn the same way but for the texture (only when there is exactly one such). Measured on his recording, 121
in-between pictures with the camera moving sideways: the fall out of step with the rock in 4 before, in none
after. Checked by the user on screen. Two wrong turns on the way, both from not having seen it: "the depth
change" (his test with BT3_DEPTH_FRAC=1 seemed to say so; that switch breaks in-between pictures by itself) and
"the stored depth's offset moves a far corner sideways" (corrected in `interp_reproject_with`, right in principle,
no measurable effect).

**Depth, third part.** With whole-number depth per pixel the fall's missing part was filled, but as a speckle that
changed every picture while the camera moved: the rock's whole number, computed by the GPU, comes out one higher
than the fall's on scattered pixels. What the game transforms itself now leads by two depth steps (`GS_Z_LEAD`,
put() in gs_draw.c; not past the nearest depth). On the user's recording the lead changes about 24 pixels a
frame of 917,504, nearly all at the fall. BT3_DEPTH_FRAC=1 turns the whole-number rule off, to compare (not
with in-between pictures).

Both renderers run with all of this (the user saw the same in each; an Nvidia card's OpenGL and an integrated
GPU's Vulkan gave the same pictures windowless, 50 pixels apart at most).

## A flat blue triangle across the picture after leaving the water, at 16:9 only (2026-10-11)

Found by the user at Kame House ("a vertex explosion for two seconds"); not in PCSX2 (checked by him), not at 4:3
here (checked on his recording), and in the game's own pictures, not the in-between ones.

- A ripple ring on the water (`EftWaterRing`, src/battle/eft_e.c) grows by a share of itself every tick
  (`sizeVel += sizeVel * sizeDamp`) and within two seconds its size is the largest number a float holds: the
  quad `EftWater_DrawGroundQuad` hands to the clipper then has corners at 1.7e38 (seen in a trace of the
  clipper's input: a five-cornered polygon with positions of 1e9 to 1e38 and "distances" to the planes that put
  kept corners far outside them).
- Cut off at the view's edges with such numbers, the result is where the rounding puts it. On the console and at
  4:3 that is off the screen and the triangles are dropped (`EftWater_IsClipped` for all three corners). With
  the side planes of a 16:9 view (`EftGfx_UpdateClipPlanes`, widened by Port_WideFactor) one corner is on the
  screen and two behind the eye: a triangle from the ring's middle to the far ends of the GS's coordinates.
- Under `#ifdef PORT`, a ring of 100,000 units or more (ten times a map) is not drawn. Checked on the recording:
  the three frames that showed it are clean, the ring while small is drawn as before, a 4:3 frame is the same
  pixel for pixel. Checked by the user on screen.
- The first fault that comes from the widescreen setting meeting an effect that grows without limit; others of
  the kind would look the same (a flat shape across the picture for a moment) and are found the same way: the
  draw by BT3_GPU_CUT, its vertices by BT3_GPU_DUMP (a corner at x = 2303.94 or -1792 is the GS's coordinate
  range), then the clipper's caller.

## Online: each player's own camera type and screen shake (2026-10-11)

Asked for by the user. A session starts from an empty save on both copies, so both sides of an online match had
the option menu's defaults whatever each player had chosen.

- The two options are save words 0x1694 and 0x1698 (`gOption->type` and `->value` in the option menu,
  menu_x_c.c); `BattleSetup_DefaultOptions` copies them to both sides of the battle setup, from where they reach
  each fighter (+0x49C; +0x4B8 `camShakeOn` = the second word is 0).
- Each copy reads its player's own two words from his save file (as it does for the voice set: `own_options` beside
  `own_voice_alt`, carried over the restart into a session in BT3_NET_OPTS) and sends them behind the name in the
  greeting (the joiner) and in its answers (the host), with a mark. Both copies then set side 0 to the host's and
  side 1 to the joiner's: the same on both, as it must be, the cameras being part of the game's state
  (`Port_NetSideOptions`, called from BattleSetup_DefaultOptions under `#ifdef PORT`).
- NET_VERSION is not raised. A copy of 0.1.25 or earlier neither sends nor reads the word; a new copy that gets
  none uses none, so with an older copy both sides keep the defaults (the log says so).
- Checked, two copies on one machine through the room-code lobby, saves made to differ (camera 1 shake 1 against
  camera 2 shake 0): both log the same four values; with scripted input both reach a battle, log that it was set
  up with them, and are at the same vertical blank with the same health at battle tick 400. Against the published
  0.1.25 in either role: they connect and the new copy says the defaults are used. Not checked: what the camera
  and the shake look like in the match (no picture was looked at), a match longer than that, Windows.
- What is read is the save FILE: an option changed in the menu and not yet saved is not the one sent.

## Ticking "V-sync" closed the game (0.1.25, the Vulkan renderer) (2026-10-11)

Found by the user an hour after 0.1.25 was published; his backtrace ends in `Ui_DrawAgain`, in the Nvidia driver.
The settings window is drawn inside a frame, after the swapchain's picture of that frame was acquired; the
checkbox called `SDL_SetGPUSwapchainParameters` right there, and the window was then drawn onto a picture of a
swapchain that was gone. `GsGpu_Vsync` now only notes the request; `frame_end` applies it at the start of the
next frame, after `SDL_WaitForGPUIdle`. (The OpenGL renderer sets a swap interval, which is safe at any time.)
The option had only been tried as a saved setting read at start; "ticking it while the game runs" was listed as
not tested in the release notes and should have been tried before release. Checked by the user on screen (Nvidia,
Vulkan): on and off, no crash.

## Frame skipping: full speed with pictures left out, instead of slowing down (2026-10-11)

Asked for by the user ("why does the game slow down instead of being choppy"). The game advances one step per
picture and has no other clock; a machine that needs longer than a tick to show a picture ran the game slowly.

- `skip_decide` (gs_draw.c), as each picture is finished: if the game is more than one vertical blank behind its
  grid (`Port_PaceBehindUs`, plat_stub.c), the NEXT tick is not drawn. Its drawing is left out the way a re-run
  frame's is (`GS_NO_PICTURE` = gPortResim or gsSkipFrame, in gs_core.c / gs_draw.c / gs_vu1.c: vertices are not
  collected, vertex programs not run, nothing recorded), and `GsGpu_FrameEnd` shows nothing for it. The grid then
  catches up by itself: a blank that is already due is not waited for (vblank_wait).
- At most four ticks in a row, so something is shown at least every fifth; slower than that, the game slows down
  as before. Not while the settings window is open, not with BT3_UNCAPPED or BT3_SHOT.
- No in-between picture is made across a skipped tick (the previous tick's lists are forgotten).
- Setting `frame_skip`, on by default: F1, Video, "Skip pictures instead of slowing down". BT3_FRAME_SKIP=0|1;
  BT3_SKIP_FORCE=1 leaves out every other tick whatever the clock says (testing; what "choppy" looks like).
- Measured on an integrated GPU at 8x in a stretch where a picture takes about 33 ms (a recorded session, 15
  seconds): off, 29.9 vertical blanks a second, half speed, 30 pictures a second; on, 60.1 blanks a second, full
  speed, 31.5 pictures a second. With every other tick forced out, the 31 pictures that are shown in a fight are
  the same, pixel for pixel, as those ticks drawn normally. The regression replay passes.
- It only helps where showing is what takes the time. Where computing the tick is too slow, nothing is gained.
  Not checked: on a display by anyone, in an online match, with Smooth motion on, on Windows.
- (Also: the "(F11)" beside "Full screen" had ended up beside "V-sync" in 0.1.25; put back.)

## Quick match: queues by battle type (2026-10-11)

Asked for by the user: "matchmaking where it searches for people queuing". Decided with him: five queues (single,
team, DP battle of 10 / 15 / 20 DP), all without a time limit; the longest-waiting player first; connected as rooms
with codes are (directly when the two can reach each other, through the relay when not), with a line in the window
saying that a direct connection lets the other player see one's address.

- **Service** (port/matchmaking/worker.js): `/v1/quick {v, name, addrs, queue, who}` joins the caller to the
  oldest unjoined room of that queue and netcode version, or makes it a room there; `/v1/queues` counts who waits.
  Two who search in the same moment both find nobody and both make a room: the one whose room came second (by
  rowid) sees the first, gives its own up and joins. The room table has two more columns (`queue`, `who`); the
  README there says how a Worker that runs is updated. Its tests (test.mjs, 11 new) pass under Node 24.
- **Game**: `Match_Begin(3, ...)` in net_match.c (the thread goes on as a host or reports the peer as a joiner; a
  room that runs out is given up and the search starts again, keeping its relay address), `Port_LobbyStartQuick`
  in net.c (the lobby takes host or joiner from the service's answer), the tab "Quick match" in the online
  screen (ui.cpp): the queues with how many search each, Search / Cancel. Whoever becomes the host uses its saved
  rollback and input delay; the rules are the queue's. BT3_LOBBY=quick:<queue> for tests.
- Checked on this machine with the Worker behind a local server and its test database (BT3_MATCH_URL): one copy
  searches DP 15 and waits, a second searching the same queue eight seconds later is put with it, they connect
  directly with rollback and both log "DP battle (15), time limit none". Against the Worker as deployed today
  (not updated yet): rooms with codes work as before, a search says the service does not know quick match yet.
- Not checked: the Worker on Cloudflare with its real database (the `rowid` tie-break and the two ALTER TABLE
  lines among it), the window itself (nobody has looked at the tab), two players on two networks, the relay path
  of a quick match, Windows.

**Quick match: the same rollback and input delay for everyone** (the same day, before 0.1.27 was published). A quick
match took the rollback and delay the host had saved on the Host tab, and who hosts is whoever searched first: a
stranger's choices, unseen. Now fixed for quick matches: rollback of up to 8 frames, the input delay measured from
the connection. Checked through the real service, two copies on one machine: both log "input delay 1 blanks,
rollback of up to 8".

**Quick match: a search ends after three minutes** (after 0.1.27; asked for by the user). The tab counts the time
down, then says nobody was searching that queue and offers "Search again". It bounds what one search can ask of
the service (about 60 requests) and replaces the open-ended wait, where a window left open asked for hours. Not
seen on screen by me.

**Quick match between two networks, seen once** (2026-10-11, the user, 0.1.27's code): while trying the search timer
he was put with another player who was searching; the match "felt fine at 180 ping" (rollback of up to 8 frames,
the measured input delay). Not known: whether that connection was direct or through the relay, and the delay chosen.

**The log printed the other player's address** (asked by the user: "does the log leak ip?"). The copy that joins
wrote "net: joining <address>:<port>" for every match, also when the address was the host's as the matchmaking
service gave it (a room code, a quick match): a stranger's public address in a log that gets pasted into bug
reports. In every release that has room codes, up to 0.1.27 (which one was the first was not looked up). Now the address is printed only when the player typed it (Join by
address); otherwise the line says the host was found by the service. Checked: both logs of a room-code match
between two copies hold no address at all. The crash report has none either (it never had). Not printed anywhere:
the player's own address.

## The matchmaking service can ask for a minimum release (2026-10-11)

Asked for by the user ("only the newest version can play"). The game had no idea which release it is; now
`port/src/port_release.h` says (129 = 0.1.29), **raised by hand in the commit a release is tagged on**, and
net_match.c sends it as `rel` with a new room, a join and a search. The Worker refuses those from a game below its
variable `MIN_RELEASE` (or that sends none: 0.1.28 and earlier) with the error "old", which a game of 0.1.29 or
later turns into "This release is too old for online play through the matchmaking service. Get the newest
release." Off unless the variable is set. What it does not do is in port/matchmaking/README.md: old copies can
still meet by typed address, and a changed copy can claim any number. Checked: the Worker's tests (5 new), and
that a game sending `rel` still makes and joins rooms on the Worker as deployed (which ignores it). Not checked:
the refusal against the deployed Worker (it has no such variable yet), the message on screen.

**"Update available. Online servers are unreachable until you update."** (the user's words; the same day). When
the online screen opens the game asks the service once (`/v1/hello`) for the oldest release it lets in and the
newest there is, and says so at the top of the screen: the line above with the two numbers when it is too old; a
milder "Update available: release 0.1.NN" when a newer one exists (`LATEST_RELEASE`, optional) and it is still let
in. A refused room, join or search answers with the same words. Checked against the Worker behind a local server
with a minimum above this build: host, join and quick match all stop with that line. Not seen: the banner in the
window. One request more per opening of the online screen; none while it is closed.

## Joining a match reset the picture to 4:3 (2026-10-11)

Reported to the user by a player. The shape of the picture (`sAspectMilli`, plat_stub.c) was an ordinary static of
a file whose variables count as the game's state (make_state.py: PORT_STATE has plat_stub), so it was saved and
put back with the game. An online session starts by going back to the state of the program's first vertical
blank; a shape chosen in the settings window during that run went back to what it had been at start (4:3 on a
first run, or whatever the settings file said then). The same at the session's end, and it sat in every rollback's
state too. Now PORT_HOST, like the other things of plat_stub.c that are the player's and not the game's. Checked:
before, the variable's address lies inside the ranges of the `.mem` list; after, outside; two copies still connect,
reach a scripted battle and agree at tick 400; the regression replay passes. Not checked: on screen, by anyone.

## The relay was off for everyone: a login was taken with every room (2026-10-11)

A player with a VPN reported that quick match failed with "could not reach each other, directly or through the
relay" while rooms with codes worked. My first reading (the VPN gives the one who joins no stable address) was
wrong: he then said joining by code works too. A test that forced the relay showed the cause: the service
answered "the day's relay logins are used up". The Worker handed a login out with every room, and since quick
match every search that waits makes a room; the day's 500 were gone, and every pair that could not connect
directly failed until midnight UTC. (My own tests of the day used some of them.)

- **Now a login is asked for when a match needs it.** net_match.c: the host, once the other player is known, waits
  four seconds for the lobby to connect (the lobby ends the search when it has); if not, it asks `/v1/relay`,
  allocates on the match's socket (the lobby keeps off it meanwhile: `Match_SocketBusy`), adds `relay=` to the
  room and raises `relayGen`, on which the lobby permits the other player's addresses again. The one who joined
  asks the room every two seconds from the third on and takes the new address (`addrGen`). The room is kept until
  the lobby is done. The knocking lasts 24 seconds instead of 20.
- The Worker gives a game that sends `rel` >= 129 no login with its room; older games as before.
- Checked against the Worker behind a local server (no relay key there): by code and by quick match, direct: the
  two connect, `/v1/relay` is never asked, the room is left at once. With BT3_RELAY_ONLY: the host asks after the
  direct ways stayed silent, is told why there is no relay, the one who joined asks the room eleven times, both
  give up. The Worker's tests (7 new) pass. **Not checked: a relay address actually taken late and a match played
  through it** (needs the Worker deployed with its key and logins left for the day).

## Online matches at 120% speed (2026-10-11)

Asked for by the user: "the whole match, that's the standard for competitive play".

- **What it is here:** the vertical blanks' pace. `BLANK_NS` in plat_stub.c is 16.68 ms divided by
  `Port_NetSpeedMilli()` / 1000 (gs/net.c: 1200 while a match is connected and the rules word has `RULE_FAST`, bit
  12; else 1000). Every step of the game is computed as always, 72 of them a second instead of 60: nothing of the
  game's state changes, so replays, rollback and the two copies' agreement are untouched. The in-between pictures'
  timing, the frame-skip threshold and the automatic input delay take the blank's real length.
- **Sound** goes with it: the game counts a stream's length in vertical blanks in a session, so streams and
  effects are played a fifth faster (`SDL_SetAudioStreamFrequencyRatio` in snd_adx.c / snd_se.c), and higher.
- **Both copies or neither.** The host's choice, sent with the rules. The one who joins says in its greeting that
  it knows the pace (a capability word behind the options, `CAP_MARK | 1`); a host that hears none (0.1.28 and
  earlier) takes the bit back before it answers, so the two never run at different paces.
- **Where it is chosen:** every quick match is at 120%; the Host tab has "Game speed" (100% / 120%, setting
  `net_speed`, 120% by default). BT3_NET_SPEED=120|100 for tests.
- Checked with two copies connected by address, without a window: both log "game speed 120%", their blanks come
  13.90 ms apart (16.68 at 100%), and with scripted input both are in the same battle with the same health at tick
  400; with a 0.1.25 as the one who joins, the host logs 100% and both keep 16.68 ms. The regression replay
  passes. Not checked: how it feels and sounds (nobody has played or heard it), a real match between two
  networks, rollback's behaviour at a high ping with the shorter blank, Windows.

## Four volume sliders (2026-10-11)

Asked for: voices apart from the sound effects, and a slider for the stages' ambient sound. The game keeps these
apart by itself, so the sliders follow its own grouping (snd_adx.c, snd_se.c):

| Slider | Stream players (include/sys/adx.h) | Effect bank slots (src/sys/snd.c) |
|---|---|---|
| Music | 0, "BGM" | |
| Sound effects | 2, 3 | 0 (common, loaded at boot), 2 (the battle's common bank), any other |
| Voices | 4, 5 | 4, 5 (the two fighters' banks: battle_load.c loads `sndChara[0..1]` there) |
| Stage sounds | 1, "MAP" (only stg_b.c plays there) | 3 (the stage's bank) |

Settings `voices` and `stage_sounds`; a settings file from before has neither, and both then start at what
`effects` is set to, so nobody's sound changes by updating. Streams follow a slider at once, a bank's sound with
the next one that starts.

Checked: it builds, the regression replay passes, and a replay with sound logging uses stream players 0, 1, 2, 4
and 5, as the table says. Not checked: by ear, at all. In particular what the fighters' banks hold besides voices
(a fighter's own effects would follow the Voices slider) and what a stage's bank holds was not looked at.

## A fight's display in a wide picture: three layouts (2026-10-11)

Setting `hud_layout` (F1, the UI tab: "HUD Style"; BT3_HUD=<n> for a test), `Port_HudLayout()`:

- **0, at the screen's edges**: as since widescreen, the default.
- **1, at the edges with long health bars**: pull request #28 (victor-g-lima), merged behind this setting. hud_b.c
  draws the bar's frame and the bar in pieces and stretches a straight middle piece by half the extra width, so
  the bars reach the timer as they do at 4:3; markers 0x15 / 0x16 (anchors 5 and 6) keep a piece wider than 480
  pixels from being taken for a full-screen flash.
- **2, in the middle, as in 4:3**: every part of the HUD is narrowed about the middle of the screen
  (GsGpu_Draw: the pivot is 256 whatever side the part is marked with), which is the 4:3 arrangement as it is,
  with the scene wider around it.

Checked from screenshots of a recorded fight (offscreen): the three at 16:9, the long bars and the middle at 21:9,
both sides, at full health. Not checked: a bar that is losing health (the moving mask over the longer bar),
several bars in reserve, a team's portraits, split screen, Survival's counter, other aspect ratios.

## The player's own game speed, outside online play (2026-10-11)

"Game speed" on the Cheats tab: 50% to 200%, setting `game_speed`, `Port_GameSpeed()` in plat_stub.c. The same
thing as an online match's 120%: the vertical blanks' pace and the pitch of the sound, nothing of the game's state.
**An online match never reads it** (asked for by the user): `Port_SpeedMilli()` is the match's speed whenever a
match is connected or a session runs, and the setting only otherwise.

Checked without a window: with the setting at 150% the blanks come 11.1 ms apart offline; two connected copies that
both have 150% in their settings run at 16.68 ms when the host chose 100% and at 13.9 ms when it chose 120%, and
agree at tick 400. The regression replay passes. Not checked: the slider on screen, the sound by ear at any speed.

## Sound at another game speed: stretched in time, not pitched (2026-10-12)

Reported after 0.1.29: "audio is pitched at 120". The first version played everything faster by the speed, which
is also higher. Now:

- **Streams** (music, a stage's background, spoken lines; snd_adx.c `stretch_feed`): played at their own pitch
  and shortened or lengthened in time, as emulators do at such speeds. Pieces of 40 ms, each faded over 8 ms into
  the one before, each taken 32 ms * speed after the last and moved by up to 15 ms to where it continues the
  previous piece best (the largest correlation). A stream still ends when the game, counting in vertical blanks,
  expects it to.
- **Effects and battle voices** (snd_se.c): at their own pitch and their own length. They are short, start when
  the game starts them, and nothing waits for their end.

BT3_SND_DUMP=<file> writes what the music player hands to the sound device. With it, at 120% and at 70%: two
thirds of the audible windows of the output are exact copies of the sound at 100% (the rest are the fades), so the
pitch is the original's, and the output moves through the original 1.200 and 0.701 samples per sample. Not
checked: by ear. A time stretch can be heard on some material (a faint flutter on long held notes, doubled
attacks on drums); how this one sounds on the game's music nobody has listened to yet.

## The online match speed is a number (2026-10-12)

Players reported 120% here as "a bit faster than Dolphin": the competitive standard is 120% of the PAL version
there, which is 60 pictures a second, where 120% of this (USA) version is 72. How fast the PAL game moves per
picture is not known (the USA code has no PAL handling to read it from; a search found one forum recollection
that the series runs "at the same speed with lower FPS" on PAL and nothing measured), so the right number on this
version's scale is somewhere from 100 to 120 and has to be found by the players.

So the speed is now a percentage in the rules word (bits 16..23; bit 12 stays "120%" for 0.1.29), the Host tab has
a slider from 100% to 150% (setting `net_speed_percent`), and `NET_SPEED_STANDARD` in net.c is the one number a
quick match runs at and the slider starts from: 120 until the players name another. Capability word: | 2 = knows
the number.

Checked with connected pairs, without a window, against the published 0.1.29 build: new and new at 110% both run
blanks 15.16 ms apart; a new host at 110% with a 0.1.29 joining: both 100%; a new host at 120% with a 0.1.29:
both 120%; a 0.1.29 hosting at 120% with a new one joining: both 120%. The regression replay passes.

## "Music follows game speed" (2026-10-12)

Audio tab, setting `music_follows_speed`, off by default (`gPortMusicFollows`, snd_adx.c). Off: a looping stream
on the music or the stage player is played as it is at any game speed; the game never waits for a loop's end, so
nothing has to fit. On: it is stretched with the game, as in 0.1.30. A stream that plays once (a jingle, a spoken
line) is always stretched: the game counts its length in vertical blanks.

Checked from the music player's output at a game speed of 120% (BT3_SND_DUMP, the menu's looping music): off, 99%
of it is an exact copy of the original, advancing 1.000 per sample; on, 70% and 1.201. Not checked: by ear; the
checkbox on screen.

## The online name is remembered (2026-10-12)

The settings file takes text besides numbers: `name="text"` lines, written after the numbers
(`Port_SettingText` / `Port_SettingTextSave`, plat_settings.c; a release from before reads past them, and drops
them when it writes the file). The first one is `net_name`: the "Player name" of the online screen, read when the
screen is first built and saved when the field is left after an edit and when a match is started. Checked: a
settings file with `net_name="Goku 99"` shows that name in the field (offscreen screenshot). Not checked on
screen: that typing a name writes it.

## The online names: with the HUD, and where (2026-10-12)

- **Only while the HUD is drawn.** The names were shown whenever a fight existed, so also over the intro.
  GsGpu_Draw notes the frame in which a visible piece of a side panel was drawn (a 2D piece between the HUD's
  left / right markers with a colour that is not fully transparent); `GsDraw_HudShown()` is true within 6 frames
  of that (frame skipping leaves frames out), and ui.cpp draws the names only then. BT3_HUD_LOG=1 prints the
  frame the HUD starts at. In a recorded fight: no HUD at the versus screen or in the fade, the log says frame
  2257, and the screenshot of 2262 has the HUD. Not seen: an online intro itself.
- **106 HUD pixels in from the outer edge** (was 62 picture pixels, which sat on the panel of a team's waiting
  fighters), narrowed and placed like the HUD in a wide picture, including the "in the middle" style. Not seen on
  screen yet.

## The competitive standard is 114% (2026-10-12)

The players tried the sliders of 0.1.30 and settled on 114% as what "120% on PAL in Dolphin" is on this version
(told by the maintainer). `NET_SPEED_STANDARD` is 114: what every quick match runs at and what the Host tab's
slider starts from (its setting has a new name, `net_speed_host`, so that a 120 saved by 0.1.30 does not stay).
Checked with connected pairs, without a window: a host at 114% with the new build or the published 0.1.30
joining: both run blanks 14.63 ms apart; with 0.1.29 joining: both 100%. A quick match hosted by a 0.1.30 still
runs at its 120% on both sides, so the standard is only uniform once the service asks for this release.

## The depth fix of 0.1.25 cost a quarter of the frame rate (2026-10-12)

Reported: "huge performance drop from 0.1.24 compared to 0.1.30". Measured with the published builds on an
integrated chip (offscreen, uncapped, 2x, 16:9, the same recorded fight): 0.1.24 draws 112 pictures a second,
0.1.30 draws 82. Taking the `gl_FragDepth` line out of gs.frag gives 112 again: a fragment shader that writes
depth keeps the chip from leaving out hidden pixels before it runs the shader, `layout(depth_less)` or not.

Which draws pay: the GS's own primitives (2D, effects, the full-screen passes: 82 -> 108 with only those spared),
not the vertex programs' (82 -> 84), which are the ones the fix was for.

**The fix:** a second fragment shader, gs.frag compiled with NO_FRAG_DEPTH (the build writes gsnz.frag; key bit
23; `sFsNz` in gs_gpu.c, `sMainProg[4..7]` in gs_gl.c). pipeline_get gives it to every draw that **does not write
depth and tests with "greater or equal" or not at all**. For those nothing changes: what is in the depth buffer
was written through the full shader, so it is whole numbers, and z >= N is floor(z) >= N for a whole N. Draws
that write depth, and the few that test with "greater", keep the full shader. BT3_NZ=0 turns the rule off.

Result: 103 pictures a second (82 before, 112 in 0.1.24). The pictures are the same bit for bit with the rule on
and off: three frames of a fight, three of the waterfall recording (the scene the depth fix was made for), on the
integrated chip's Vulkan, and one of the waterfall on an Nvidia card's OpenGL. Not done: the remaining 8%, which
is the draws that do write depth (sparing all of the GS's own primitives gives 110, but those then store
fractions, which is the state the waterfall fault came from); a run on a display; Windows.

## Disc Fusion is always unlocked (2026-10-12)

The Disc Fusion screen (mode 24, src/menu/menu_o_b.c) asks for the discs of the two earlier games. It takes
nothing from them: `Disc_Identify` (src/sys/file.c) looks for the boot file's name and the screen sets one bit per
disc in `gProgress->discFlags` (kept for the session, not in the save); the two modes behind it are on this disc.
Under PORT, `DiscFusion_UpdateHave` sets both bits, so both plates open their mode without the disc swap, which
cannot work without a drive. (Bit 8 of the same word is something else: the ladder's "challenged a higher place".)
Checked: it builds and the regression replay passes. Not checked: the screen itself, and what the two modes do
from there.

## Loading screens beyond 4:3, and skipping the logos and the opening (2026-10-12)

Two pull requests.

**#49 (Victorgonl).** The three loading screens are drawn while the menus run, where every 2D page is stretched to
the width, so their figures came out a third too wide at 16:9. `Load_DrawScreen` now brackets its drawing with
marker 0x17 (`gsAnchor` 7): the renderer narrows those sprites about the left edge even in menu mode, and the
scattered items of the third screen are drawn at positions spread over the wider page (`Load_ItemDrawX`). Only where
things are drawn changes; the stored positions and the random sequence are the game's. Checked offscreen at 4:3,
16:9 and 21:9 for all three screens (the one shown at the start of the game).

**#50 (Renzo-MGRR).** F1, UI: "Skip logos" and "Skip intro video" (settings `skip_logos`, `skip_intro`, both off).
`FirstRun_Main` leaves out `Logo_ShowAll` and `Movie_PlayOpening`. Neither loads anything: the third partition is
loaded beside the logos and waited for after them (`FirstRun_WaitPartition`). The pull request was merged and then
reworked: its checks were outside `#ifdef PORT` and called a port function from PS2 code, and it added a tab of
its own and a header in `include/`; the checks now sit behind a macro that is 1 in the PS2 build, and the two
boxes are on the UI tab. Checked offscreen: with the logos skipped the opening movie is the first thing shown, with
both skipped the copyright page and then the title; with neither, the regression run is unchanged.

**"Skip copyright page"** (same tab, setting `skip_copyright`, off). The page of legal text is not part of the
first-run sequence: it is the first loop of `Title_Run`, a still held for 300 frames between two fades, shown every
time the title screen is entered and not shortened by any button. With the setting on that loop is not entered.
Checked offscreen with all three on: the memory-card check, then the title's own animation.

## "Menus in 4:3" (2026-10-12)

F1, UI (setting `menus_43`, off). In a picture wider than 4:3 the menus' pages are stretched to the width (they are
composed for 4:3; see `Port_WideFactor`). With the setting on they are shown at 4:3 in the middle of the window,
black at the sides; a fight keeps the wide picture. It is only the present step: `Port_PageAspectMilli()`
(plat_stub.c) is 1333 while `gPortMenuMode` is set and the setting is on, otherwise the picture's shape, and both
renderers size the shown rectangle by it. The window keeps its shape. The loading screens go by the same value
(PR #49's narrowing and spreading are for a stretched page), so one shown inside the menus is the original 4:3
page and one shown on the way into a fight is the wide one.

Checked offscreen at 16:9 from the window's own picture (`BT3_UI_SHOT`): the memory-card check and the title screen
are 4:3 with bars, a fight from a recording fills the width. Not seen: the change from a menu to a fight and back
in one run, and the OpenGL renderer.

## Orange side bars (2026-10-12)

F1, UI: "Side bars", Black or Orange (setting `side_bars`, 0 by default). Wherever the picture is narrower than the
window (a 4:3 picture, or the menus with "Menus in 4:3", in a wide window or full screen) the two sides are black;
with Orange they are a gold panel with a key-pattern border along the picture. The art is the project owner's own,
two pieces in `port/assets/bars`: `trim.png` (34 x 720, the border) and `fill.png` (126 x 720, the plain panel).
`undefined.py` writes them into the program as bytes (`bars_png.h`), so nothing ships beside it; `ui.cpp` decodes
them with SDL's PNG reader on first use and draws them on ImGui's background list, which both renderers already
show: the border in proportion to the picture's height at its edge, the panel stretched over the rest, both
mirrored on the right. A bar of any width therefore keeps the border's shape. Only side bars are drawn, not bars
above and below.

Checked offscreen from the window's own picture at 16:9 and 21:9 with "Menus in 4:3" on (Vulkan). The OpenGL
renderer ran with it without fault but its window picture was not captured.

**The option is "Border", and the pictures can be replaced.** The choice reads Black / Border. A folder `bars` next
to the game (where `textures` goes) with a `trim.png` and / or a `fill.png` takes the place of the built-in gold
set; either file alone replaces only its piece. Any size: the border is drawn as high as the picture and at its own
width-to-height proportion, the panel is stretched. They are read once, when bars are first drawn, and the log says
`bt3: side bars: bars/trim.png, W x H`. Checked offscreen at 16:9 with a made-up pair.

## The model viewer is shown wide (2026-10-12)

The character reference's model viewer (`ChrView`, battle code run from the menu screen `CharRef_Run` while its
state is `CHARREF_ST_VIEW`) is a 3D scene, but it ran in menu mode, where the projection is the 4:3 one and the
page is stretched to the width: the model and the arena came out a third too wide at 16:9 (seen by the user).
`CharRef_Run` now calls `Port_MenuScene3D(state == VIEW)` every frame (headless.c): while it is on, the menu mode
is off, so `Port_WideFactor` gives the wide projection (the view's matrices follow it within the frame, see
`View_UpdateMatrices`) and the renderer treats 2D as in a fight. With "Menus in 4:3" the viewer fills the window
like a fight does. Checked offscreen at 16:9 by jumping into the viewer with a temporary hook (not in the code):
the model and the arena's columns have their proportions. Not seen: entering and leaving it by hand, and the
viewer's own 2D (button hints), which is now narrowed about the middle like a fight's messages.

## "Random" draws added stages and songs (2026-10-12)

Asked by the user. In the duel's select (`CharSel_Run`, which Ultimate Training uses too: progress mode 0x2D) and the
team select (`TeamSel_Run`), a chosen "random" was the game's own draw:
- the stage: `stageIds[Rand_Range(stageCount - 1)]`, drawn again while the id is 0x23 ("random") or more. Added
  stages are numbered from 0x24, and are listed after "random", so none could come out;
- the song: `Rand_Range(9) + 8`, one of nine battle songs by number, whatever the list holds.

With stages added (or replaced) the whole list is drawn from, and only "random" and the locked / empty markers
(0x3E, 0x3F in the PC build) are drawn again. With songs added the song is any entry of the list before "random"
(`CHARSEL_RANDOM_BGM` / `TS_RANDOM_BGM`), the disc's and the added alike. With nothing added, and in an online
session (where nothing is added to the lists), both draws are the game's, call for call: recordings and online
agreement are as before. The other modes' random stages (`Rand_Range(0x23)`) are left alone; added stages are not
listed there. Not checked in the game: reaching the select needs a hand on the controller. The regression run and
the matching build pass.

## "An update is available" (2026-10-12)

Asked by the user: players were only told of a newer release by the matchmaking service, and only once theirs was
too old to be let online.

Once per run, when the window is there, the game asks GitHub for the repository's newest published release:
one GET of `https://api.github.com/repos/<PORT_UPDATE_REPO>/releases/latest` (net_match.c, `update_thread`; drafts
and pre-releases are not "latest"). It goes out the way the matchmaking requests do: WinHTTP on Windows, the curl
library on Linux, the `curl` program failing those (`http_post_native` with no body is a GET, with a user agent,
which GitHub requires). Nothing is sent but the request. The tag `v0.a.b` becomes a number as `PORT_RELEASE`
counts (0.1.33 is 133). The log says `bt3: update check: the newest release is 0.1.32, this is 0.1.32`, or
`no answer`.

If the number is higher than this program's:
- a line over the picture for ten seconds, once: "Update available: 0.1.33. F1 for the download page.";
- at the top of the settings window, for the rest of the run, the same with a "Download page" button, which opens
  `https://github.com/<repo>/releases/latest` in the browser (`SDL_OpenURL`).
Nothing is downloaded or installed by the game. Updating is copying the new files over the old ones.

F1, Misc (a tab of its own, the seventh): "Check for updates" (setting `update_check`, on by default) turns the request off. It is not made under the
offscreen or dummy video drivers (test runs). `BT3_UPDATE_CHECK=1` / `0` overrides both; `BT3_UPDATE_LATEST=<n>`
is taken as the answer without asking. `PORT_UPDATE_REPO` (port_release.h) names the repository: a fork that
publishes its own releases changes it, or its players are told of this repository's.

GitHub allows 60 such requests an hour from one address without an account; one per start is far inside it, and a
refused request is only "no answer".

Checked on Linux: the real request by the library and by the program (both give 0.1.32); no request in an
offscreen run without the override; with a pretended 0.1.40, the line over the picture and the settings window's
row, from the window's own picture. Not checked: the WinHTTP path (written, not run), and the button's opening of
a browser.

**Where the options are (same day).** The settings window has a seventh tab, Misc: "Check for updates" and the
three skip options (logos, intro video, copyright page), which were on the UI tab. The UI tab keeps HUD Style,
"Menus in 4:3" and "Side bars". Earlier sections that say "F1, UI" for the skips mean this tab now.

## Where the time goes on a weak graphics chip: the HUD's alpha copies (2026-10-12)

Players with weak machines report low speed, and the user's own machine hides it. Measured on this machine's
integrated graphics chip (Radeon, in a Ryzen 7 9800X3D), a recorded fight, 16:9, uncapped, all effects on:

| resolution | pictures a second |
|---|---|
| 1x | 278 |
| 2x (the default) | 96 |
| 3x | 45 |
| 4x | 25 |

- The resolution multiplier decides nearly everything; the six effects together cost a tenth (96 with, 108 without).
- On the fast graphics card the same fight gives about 243 at every resolution: that is the processor's limit, one
  core (pinning the game to one core changes nothing). About 4 ms a frame on the fastest desktop processor there is.
- Where the graphics chip's time went at 3x, found by drawing only the first n draws of every frame: the first
  1,700 of 1,890 draws (the whole 3D scene) take 9 ms; the last 190 (the post effects and the HUD) take 13 ms.

The cause of most of that tail: **every render target is 1024 x 1024 GS pixels times the multiplier** (gs_draw.h;
the picture is 512 x 448 of it, a fifth), and the HUD's destination-alpha draws (TEST.DATE: a bar's fill cut to
length, a word through a band) each had the whole target's alpha bytes copied for them, nine times a frame in a
fight: 9.4 megapixels a copy at 3x. A copy of 8 x 8 pixels in its place gave the same speed as no copy at all, so
the cost is the copy's size, not the break in drawing.

**Fix:** `GsDraw_DateRegion` (gs_draw.c) gives the box around the run of draws that read the copy, and both back
ends copy only that. A run with a draw placed by a vertex program still copies the whole target.
`BT3_DATE_WHOLE=1` copies the whole target as before (to compare).

| resolution | before | after |
|---|---|---|
| 2x | 96 | 117 |
| 3x | 45 | 56 |

Pictures compared old against new, identical in every pixel: five frames of the bubble fight and two of the
waterfall fight (Vulkan), two of the bubble fight (OpenGL).

**Still there, not done:**
- The targets themselves: a fight uses 7 to 16 of them, 36 MB each at 2x and 81 MB at 3x, although the game draws
  into at most 640 x 448 of each, and into 64 x 64 of the two smallest. Sizing them by need would cut the video
  memory to a third or less; on a chip that shares a small machine's memory that may matter more than speed. It
  touches about fifty places in the two back ends (sizes, the viewport, the coordinates of a target read as a
  texture).
- The processor's side (profile of the same fight): reading the game's picture transfers `Gs_Gif` 16%, the PS2's
  float arithmetic done in software 18%, looking a pipeline up `pipe_get` 6%, hashing texture pages `Gs_PageHash`
  6%, the graphics driver 13%.

**Pipeline lookups remembered (same day).** `pipeline_get` asked the back end for a pipeline once per primitive,
and the back end went through every pipeline it had made: 6% of the processor's time in the profile. The answer
is now kept by key in gs_draw.c (pipelines are only ever added, so a number never goes stale). Where the processor
is the limit, the integrated chip at 1x: 278 pictures a second before, 296 with this alone, 299 with the smaller
copies too. No change on the fast card, whose limit is elsewhere. Three fight frames compared against the build
without it: identical.

**Looked at and left** (after the copy fix, at 3x, 17.9 ms a frame): the 3D scene 9.3 ms; the two table passes
(depth tint, see-through tint) 1.7 ms; the draws that read a buffer as a texture (glare, blurs) 2.5 ms; the
outline's copy of the ids 0.3 ms; the shadow buffer 1.1 ms. These are the effects themselves, which the Effects
tab already turns off. The driver's buffer compression is not the cost (`RADV_DEBUG=nodcc,nohiz` changes nothing
or is slower). Two measurements were thrown away: skipping "every buffer but the first" and "buffer 070" doubled
the speed only because the game shows buffers 000 and 070 in turn, so every other frame was not drawn at all.

## Online: two games that never agreed again after the start (2026-10-13)

**The report** (a player, 0.1.33): "if they got matched with a slower machine and they reached the character
select first, they become de-synced and never re-sync".

**How a session starts** (gs/net.c, gs/state.c, plat_stub.c, headless.c). Both sides connect (`net_start`: greeting,
the host's pings and choices), then put the game back to the state of the program's first vertical blank
(`Port_SessionPoll`, `sBoot`; or the program starts again, `relaunch`) and run from blank 1 as a session. Every
blank goes through `Port_NetBeginTick`: with rollback (`roll_tick`) a copy never runs more than the rollback limit
(8) ahead of the other's last known input, without it not at all. The start-up (first-run sequence, memory card
check) runs unseen and not held to real time (`Port_NetWarp`) until `Port_Trace` sees the first-run flag clear
(`Port_NetArrived`). File reads finish at once (plat_file.c), there is no loading thread, movies are not played and
sound is counted in blanks, so real time enters nowhere: a slow machine only makes both copies' blanks come later.

**The slow machine was not the cause, and could not be reproduced as one.** Test: two copies without a window over
127.0.0.1, each in its own folder with its own saves, a session asked for at blank 200
(`BT3_SESSION_TEST`) or from the start (`BT3_NET_HOST` / `BT3_NET_JOIN` with `BT3_NET_SESSION=1`), both players'
buttons from one table of random presses from blank 1 (`BT3_NET_SCRIPT`), rollback 8, automatic delay, `BT3_HASH`
logs compared blank by blank (the last line written for a blank counts), both copies showing the same view
(`BT3_VIEW=0 BT3_HUD_SWAP=1`) so that the checksum of the whole state can be compared and not only the fight's
values. One copy made slow with three new test variables (gs/net.c `Port_TestSlow`): `BT3_TEST_SLOW_READ=<ms>`
(every file read, plat_file.c `ADXF_ReadNw`), `BT3_TEST_SLOW_START=<ms>` (between the connection and the first
blank), `BT3_TEST_SLOW_BLANK=<ms>` (every blank). With 0.1.33's code, in real time (`BT3_PACED=1`):

| | start-up over at blank (host / joiner) | after connecting | the two copies |
|---|---|---|---|
| both fast | 778 / 778 | 1.7 s / 1.7 s | whole state the same, 4,584 blanks |
| joiner slow (read 20 ms, start 3 s, blank 3 ms) | 778 / 778 | 6.8 s / 7.0 s | the same, 4,273 blanks |
| host slow (the same) | 778 / 778 | 7.0 s / 6.8 s | the same, 4,273 blanks |
| joiner slow, the program started as the session | 778 / 778 | 6.8 s / 7.0 s | the same, 4,471 blanks |

The fast copy is held by the slow one and both reach the character select on the same blank, a fifth of a second
apart. Also the same: sessions asked for at different blanks of the two games (200 and 1500), and without real
time.

**The cause as found: "Skip logos" and "Skip intro video"** (0.1.32, the day before the report). `FirstRun_Main`
(src/menu/menu_i_c.c) leaves out `Logo_ShowAll` and `Movie_PlayOpening` when the player's settings say so
(`FIRSTRUN_SHOW`), and the first-run sequence is part of what a session computes from blank 1. With "Skip logos" on
one side only, 0.1.33:

| | start-up over at blank | after connecting | the two copies |
|---|---|---|---|
| host skips the logos | host 241, joiner 778 | host 0.9 s, joiner 9.7 s | whole state differs from blank 238; one copy is in a fight at blank 1497 and the other in the menus; they end on different fights |
| joiner skips the logos | host 778, joiner 241 | host 9.7 s, joiner 0.9 s | the same, mirrored |
| host skips the intro video | host 775, joiner 778 | 1.7 s both | whole state differs from blank 775 for good (the fight's values happened to stay the same in this run) |
| both skip everything | the same blank | | the same, 4,860 blanks |

The player with the setting is in the character select 537 blanks before the other and, being held to real time
from there, keeps the other 8 to 9 seconds in its unseen start-up: "they reached the character select first", and
the other machine looks slow. Nothing compared the two games, so they went on apart. ("Skip copyright page" is the
title screen's, which a session does not enter: no difference found.)

**The fix.** In a session the first-run sequence and the copyright page are the game's own whatever the settings
say: `Port_NetFixedStart()` (gs/net.c) in `FIRSTRUN_SHOW` and in `Title_Run`'s loop (src/menu/menu_a_c.c). It costs
nothing: that part of a session is not shown and not timed. Outside a session the settings work as before.

**The check that was missing** (gs/net.c `sync_note` / `sync_check`, headless.c `Port_NetDigest`). Every 32 blanks
each copy notes a number made of what must be the same on both whatever each player's settings, view and system
are: the blank its start-up ended at (`sNetArrivedAt`, a variable of the game's state, so it goes back with a
rollback), the menu mode, whether the first-run part still runs, and in a fight the sequence's state, the clock and
both fighters' health and position (what `BT3_HASH` logs as the fight's values, without the generator). A note
counts once every blank before it was run with the other player's real input (`sChecked`); the newest such note
travels in every input packet (12 bytes more in the head); a copy that has its own sure note for the same blank
compares. When they differ both sides log "the two games differ at blank N", show "The two games no longer agree
(desync): the match has ended." and go back to their own game (the goodbye packet says why, so the side that
hears it first says the same). `BT3_GS_VERBOSE` shows the count: about 470 comparisons per 600 blanks on a 120 ms
line with 10% loss, 599 without rollback. Tests: `BT3_TEST_OWN_START=1` (this copy follows its skip settings in
a session, as 0.1.33 did) with "Skip logos": ended on both sides at blanks 256 and 265 (the games part at 238);
with "Skip intro video": at 800 and 802; `BT3_TEST_APART=<blank>` (a false value sent once): both end at 3002.
No false alarm in any of the runs below.

**After**, the same runs: start-up over at blank 778 on both sides and the whole state the same in every case:
both fast (4,556 blanks), host / joiner skips the logos (4,528 / 4,556), host skips the intro (4,528), joiner slow
(4,245), host slow (4,245), started as the session with a slow joiner (4,443), a slow host that skips everything
(4,245). With each copy showing its own view (fight values compared): 60 ms each way and 10% loss (5,395 blanks),
no rollback (8,700), no rollback against rollback (6,842), one copy at 21:9 (6,728), sessions asked for at
different blanks (3,848), a slow joiner on an 80 ms line with loss (5,077), rollback 4 with delay 0 (6,417),
seven sessions one after another in one run. The regression replay is unchanged.

**The netcode's version is 4** (`NET_VERSION`; it was 3 since 0.1.15): both players need the new release. The
game computed in a session is 0.1.33's when its skip settings are off, but a 0.1.32 / 0.1.33 with either on still
computes another start-up, cannot be told from outside and sends no check values, so it is refused instead:
through the service a room code answers "The host's game is a different version", quick-match queues are per
version, and by address the host logs "a copy of another version tried to join" (checked both ways with the
0.1.33 program: no match starts). `MIN_RELEASE` of the matchmaking service can be raised when the release is out.

### Can a player's own sound files change a match? (the same day)

Asked with voice mods in quick match in mind. What a session does: the game's logic asks only one thing of the
sound, `ADXT_GetStat` of a stream player (src/sys/adx.c `Adx_GetStat` / `Adx_IsStopped`, behind `Voice_GetStat`,
`Voice_IsStopped`, `Bgm_IsStopped`, `StreamSe_GetStat`, and the movie player). In a versus session that is: the
fight's intro and win talk, which wait for each fighter's line to end (src/battle/btl_seq.c, one of the three
waits has no time limit), `btl_char_get.c` (a fighter flag while his line plays), src/battle/view_b_c.c, and the duel
/ character-select menus' announcer and guide lines (menu_g_c.c, menu_c_b.c and others). The sound effects
(gs/snd_se.c) answer nothing to the game. In a session the answer comes from plat_sndstate.c (`gPortAdxView`,
counted down once a blank by `Port_AdxTick`), and the count was set by gs/snd_adx.c `view_start` from the header
of the file this copy found for that line (`adx_blanks` of `Port_FilePath`, where a file under `mods/` wins).

- **The other voice language: no.** `voice_own_path` plays the other set's file, while the count is the default
  set's file's on both sides. Checked (one copy with `BT3_NET_VOICE=alt`): fight values the same, 0.1.33 and now.
- **A voice file of another length under `mods/` (or replaced in place, or missing): yes, until today.** Test: one
  copy with a data folder whose `mods/pzs3us2/` holds two fighters' intro lines replaced by other lines (216
  blanks for 105, 27 for 160). 0.1.33: the fight begins on another blank on that copy (fight values differ from
  blank 2167) and the two play different fights without a word. With the check above alone: the match ends on
  both sides at blank 2177 / 2179.
- **Fixed**: in a match the count is the ORIGINAL disc's file's, from a table in the program
  (port/src/plat_adxlen_tab.h, made by port/tools/make_adxlen.py with `adx_blanks`' formula: one number for each
  of the third archive's 65,201 files and the two movie tracks, 72 of them looping, the longest line 1,048
  blanks), whatever file the copy has there, also when it has none. What is heard is the player's own file; a
  longer line is cut where the original ends, as the other voice set's already is. The log says once "a sound
  stream here is not the original's length". Outside a match (`Port_NetActive` false) nothing changes: the
  device's own progress, or with `BT3_SOUND_TICKS` / `BT3_SYNCTEST` the local header.
- Checked: with the original files the table and the header agree for every stream started and the fight values
  are those of the build before the table (3,769 blanks of fighting, the fight beginning at blank 2168 in both);
  the modded copy against an original one: fight values the same for 8,441 blanks; a copy that LACKS the two
  files: the same; mod, other voice set, 16:9, 60 ms and loss together: the same (5,558 blanks). The local sound
  files were confirmed to be the disc's first (every size, 3,004 checksums against plat_verify_tab.h).

### Not covered

- No window, no sound device, one machine, Linux only: the Windows program was not built or run, and Linux against
  Windows was not repeated (the check value is made of numbers the earlier Linux / Wine comparison found equal,
  but that is the reason to believe it, not a test). A false alarm there would end matches: watch for the message.
- Real controllers; the room-code / quick-match path through the service and the relay (not contacted).
- The check sees what it is made of. Two copies whose menus differ in something else (a cursor) are only caught
  when that reaches the mode, the fight's start or the fight; the generator's state is left out on purpose.
- Other things of a player's own that reach the game were not audited one by one: `mods/` files that are not
  sound (models, parameters: they change the fight and the check will end such a match), the picture's shape (one
  run at 21:9 against 4:3: fight values the same), `Port_FxOff`, the HUD layout.
- Seen and left: with random buttons the start-up ends at blank 777 / 778 / 779 for input delay 0 / 1 / 2 (the
  logos take buttons); `sArrived` (picture on) is set from a blank that may still be run again, so a wrong guess
  at that very moment can show a few blanks of the start-up. On a line of 60 ms each way the unseen start-up takes
  5 to 8 seconds, because each copy may only run 8 blanks ahead of the other's input (1 to 2.5 s on a local
  line): a black wait, not a disagreement. If `net_start`'s exchange of the host's choices times out (3 s / 5 s)
  the two sides can start with different input delays; the check now ends such a match, the cause is not fixed.
- The connecting screen of pull request #40 was not taken: nothing here makes the wait before the two games
  start longer.


## The pause before an online match (2026-10-17)

Reported: when two players are paired, the picture stands still before the character select, longer the higher
the ping ("at most 5 seconds at 50 ms"). Cause: the session's unseen start-up (1,753 blanks from the program's first
blank to the character select, `Port_NetWarp`) went through the same exchange as play: every blank sent its input
and waited for the other's, so it ran at a few blanks per round trip. Nobody plays those blanks; each player's pad
was read and sent all the same.

Now (gs/net.c `warp_tick`, `meet`; NET_VERSION 5) both copies give every start-up blank nothing pressed for both
players and do not wait: they run the same blanks from the same first blank with the same input, at their own
speed. The check values are still noted and sent (a packet a tenth of a second, which also keeps the line open).
At the character select each copy says at which blank it arrived (`T_ARRIVED`; one that has arrived answers each
such message once, so a lost packet costs a resend) and waits for the other's, up to a minute for a slow machine;
two different blanks end the match as a desync. A copy of version 4 would wait for input that does not come: the
versions do not match and the greeting says so.

Measured: two copies over 127.0.0.1, `BT3_SESSION_TEST`, rollback 4, `BT3_NET_LATENCY` each way (the round trip is
twice it). From the session asked for to both at the character select:

| round trip | before (0.1.36's code) | now |
|---|---|---|
| 0 ms | 0.6 s | 0.6 s |
| 50 ms | 6.8 s | 0.53 s |
| 100 ms | 13 s | 0.62 s |
| 200 ms | | 0.59 s |
| 300 ms | | 0.77 s |

(About 0.4 s of it is the host's measuring of the line before the session, the start-up itself 0.08 to 0.12 s.)
Both copies arrive at blank 1754 in every run. A fight on a 100 ms line with both players' buttons from a table
of random presses (`BT3_NET_SCRIPT`), 9,600 blanks: 30 to 40 rollbacks per 600 blanks, about 550 comparisons of the
check values per 600 blanks, none different; and without rollback the same with about 618.
## Vibration, stick deadzones, and the sticks shown in Controls (2026-10-13)

A player asked for "Vibration and Deadzone settings, with the analog sticks showing in Controls".

**What the game does with the motors.** It drives both of the DualShock 2's motors itself. In a fight, hits given
and taken, clashes, charging and the like call `BtlChar_SetVibration` (large motor: a power 0..1 and a time),
`BtlChar_SetSmallVibration` (small motor: a time) or `BtlChar_Vibrate` (both); some eighty calls in
`src/battle/`. Once per fight step `BtlChar_UpdateVibration` (btl_char_get.c) turns that into this step's request,
`Pad_AddVibration(pad, small, power)`: the large motor's power swings between 50% and 100% of what was asked, six
times a second, and the small motor is on in every other step. `Pad_Update` (src/sys/pad.c) ends with
`Pad_SendActuator` for each port, which calls libvib's `sceVibSetActParam` with two bytes, bit 0 = small motor,
bits 1..7 = large motor 0..127, and clears the request: a motor stops by the game sending zero in the next step.
(`sceVibGetProfile` is asked once when a pad appears; its answer is only handed back with each command.) The game gives no vibration

- when its own switch is off: Options > Controller > Vibration, per player, bits 1 and 2 of the save's flags
  (`SAVE_FLAG_PAD_A`, on in a new save; read into the fighter at its reset);
- in a replay (`BattleReplay_IsActive`);
- for a side the computer plays.

Nothing outside a fight vibrates. Until now `sceVibSetActParam` was an empty function in the port.

**Vibration in the port.** `sceVibSetActParam` (plat_stub.c) hands the two values to `Port_PadVibrate`
(gs/gs_input.c), which calls `SDL_RumbleGamepad` on that player's controller: the large motor is SDL's
low-frequency motor (power * 65535 / 127), the small motor the high-frequency one (all or nothing), each times
the player's strength. A command that only repeats the last one is not sent again until half its 250 ms are over;
every command runs out after 250 ms by itself, so a controller is not left humming if the game stops coming.
Nothing is sent, and what runs is stopped: while the settings window is open, while the game's window is not the
one in use, with the setting off, in a frame that is only being re-run (rollback), and as the program ends
(`atexit`). Settings, per player, in the Controls tab:

| key | default | |
|---|---|---|
| `vibration_p1`, `vibration_p2` | 1 | "Vibration": on / off |
| `vibration_p1_strength`, `vibration_p2_strength` | 100 | "Strength", 0..100 % |

On at full strength is the default because that is what the game does on a console. The game's own switch still
works and comes first: with it off the game sends zero and there is nothing to pass on; the port's setting can
only take away from what the game sends. "Test" next to it gives half a second of both motors at the chosen
strength (the one thing felt while the window is open). "Reset to defaults" resets these and the deadzones too.

It cannot change a fight: the function reads two bytes, writes nothing the game can see and returns the constant
it always returned; its own state is outside the game's memory (not in a saved state, not rolled back). So a
replay, a pad recording and both sides of an online match compute the same with it on or off, and the two sides
need not have the same setting. Online, both ports' commands are computed on both sides; only this side's
(`Port_NetMe`) are passed on, to player 1's controller, the one that plays there.

**How the game reads a stick** (found while looking for "the current threshold"; the port had none of its own and
passed the controller's bytes through). `Pad_Update` takes each axis as a byte and gives every axis *by itself* a
deadzone of `PAD_STICK_DEADZONE` = 50 of 127.5, 39%: `value = (|byte - 127.5| - 50) / 77.5`, zero below. The pair
is brought inside the unit circle, and the stick counts as a direction when the larger value is beyond 0.5, which
is 69.6% of the way on an axis. So the zone of rest is a square, and a diagonal starts late (a stick pushed fully
into a diagonal reads 0.52, 0.52).

**Deadzone setting**, per player and stick: `deadzone_p1_ls`, `deadzone_p1_rs`, `deadzone_p2_ls`,
`deadzone_p2_rs`; default -1.

- -1, shown as "Original": the bytes are passed as before. This is today's behaviour exactly, and it is the
  default because the game's square zone cannot be written as a round one of some size: 39% round would change
  every diagonal.
- 0..90: a round deadzone of that many percent, in place of the game's. With `m` the stick's distance from the
  centre (0..1; the corners of a square gate count as 1) and `d` the setting: nothing while `m <= d`, else
  `out = (m - d) / (1 - d)` in the stick's own direction. The bytes handed to the game are the ones its own
  formula turns back into `out`: 0x80 for zero, else `127.5 +- (50 + |out| * 77.5)` per axis, to the nearest of
  the 78 steps a side (so values are exact to 1/155; anything outside the zone gives at least the first step,
  0.0065). The game code is not changed, and its direction test, 0.5 of `out`, falls at `d + (1 - d) / 2` of the
  way in every direction: half way between the zone's edge and full tilt. With d = 39 that is the same 69.6% on
  an axis as the original.

It is done to the pad's bytes before they reach the game or the other side of an online match, as the Square
option is, so the game sees an ordinary pad. The keyboard's stick keys are not touched (full tilt, as before).

**The Controls tab** has a new part between the controller list and the bindings, "Sticks, triggers and
vibration" (it folds away): for each stick of the chosen player's controller a circle with the stick as a white
dot, the deadzone shaded in the middle (a square for Original, a circle otherwise), what the game is given as an
orange ring and in numbers ("stick" and "game", x and y), and after the stick's name the direction the game takes
it for; the deadzone slider under the numbers; a bar for each trigger with its percent and "pressed" (a trigger
counts from 37%; the game is only told pressed or not); and the vibration switch, strength and test. A stick that
drifts shows as a dot resting off the centre: the deadzone to choose is one whose shade covers it. The list of
bindings already had the eight stick directions (a key each; on a controller they are the sticks themselves).
The window is 760 high instead of 600 where the picture has 800 lines or more, and the list of bindings keeps at
least a few rows (in a small window the tab scrolls as a whole).

**For testing without a controller or a person:**
- `BT3_STICK_TEST="lx,ly,rx,ry[,lt,rt]"` (sticks -32768..32767, triggers 0..32767) attaches a controller made by
  SDL (a virtual joystick, "Test controller") with its sticks held there; it goes through everything a real one
  does. It also prints the numbers once: what SDL reports, the bytes passed, what the game makes of them as
  computed in the port, and then what the game's own pad record holds.
- `BT3_RUMBLE_LOG=1` attaches the same controller and prints every motor command with the SDL call made, and what
  SDL then delivers to that controller's motors.
- `BT3_RUMBLE_TEST=1` lifts the game's rule "no vibration in a replay" (one line in `BtlChar_UpdateVibration`,
  under `#ifdef PORT`), so that a recorded fight can be used: the regression replay is a replay.
- `BT3_RUMBLE_SLOW=<ms>` makes the test controller keep each vibration command waiting that long, as some real
  ones do (a report: a friend's controller froze the game for seconds whenever it vibrated). While the test
  controller is attached, the real ones are left out, so a test is never felt on one.

**Verified, by numbers:**
- replay01 with `BT3_RUMBLE_TEST=1 BT3_RUMBLE_LOG=1`: the first hit, at vertical blank 1250, gives large motor
  95, 125, 113, 76 of 127 and then 0, the small motor on, off, on, off: the game's own curve for a power of 1.0
  (0.75, 0.988, 0.897, 0.603 times 127). The calls were `SDL_RumbleGamepad(49022, 65535, 250)`, `(64502, 0)`,
  `(58310, 65535)`, `(39217, 0)`, `(0, 0, 0)`, and the test controller received the same. With strength 50:
  `(24511, 32767)`, `(32251, 0)`, ... With vibration off, and with the settings window open: no call at all.
  Without `BT3_RUMBLE_TEST`: no command (the game's rule). 393 commands up to tick 500, all for player 1.
- A slow controller, replay01 to tick 500 (393 commands): 37.0 s of running with the test controller answering at
  once, 8 ms late (199 commands sent: one per 125 ms at most) and 60 ms late (3 sent, then vibration off for it,
  with the notice); the line `tick 500 ... hp 36430 24070` the same in all three. Sent every one, 60 ms each would
  have been 23.6 s of standing still.
- The regression line (`tick 2111 ... hp 23050 0`) is the same with and without `BT3_RUMBLE_TEST`, headless and in
  a window with the commands going to the test controller.
- Sticks, twelve runs (the port's computed value equalled the game's own record in every one). Original:
  `(1500, -900)` -> bytes 133 124 -> 0, 0; `(20000, 20000)` -> 206 206 -> 0.3677, 0.3677; full right -> 1, 0,
  "right"; the diagonal `(23170, -23170)` -> 0.5226, -0.5226, "up"; 69.3% on an axis no direction, 69.9% a
  direction. Round 10%: drift of 5% and 8% -> 0; 9% -> 0, 11% -> 0.0065; full right -> 1, 0; the same diagonal ->
  0.7032, -0.7032; 55% (the half-way point) -> 0.4968, no direction yet. Round 25%: a square gate's corner
  `(32767, 32767)` -> 0.7032, 0.7032; half tilt -> -0.3290 (exact: -0.3333); 62.5% -> 0.4968. Round 0%: half
  tilt -> 0.4968; `(300, -200)` -> 0.0065, -0.0065. Round 90%: 94.6% -> 0.4581 (exact: 0.461).
- With the settings window open the game's pad record stays at rest whatever the stick does (as before).

**Verified, by pictures** of the Controls tab (window never shown on a screen): drift under Original and under a
round 10%; just inside and just outside 10%; full tilt and a diagonal under 10% and under Original, with triggers
at 24% and 92%; a square gate's corner and half tilt under 25%; vibration off with the strength greyed; no
controller.

**Not verified:** nothing was felt and nothing was pressed by a person: no real controller was connected, so how
the two motors feel on a given controller (the small motor's on / off every 33 ms in particular), the "Test"
button, a stick moved by hand and the window worked by the controller alone were not tried (the new controls are
ImGui's ordinary header, sliders, checkbox and button, which the window's controller navigation already works).
Vibration stopping when the settings window is opened in the middle of an effect, or when the window loses the
focus, was not run (only: opened from the start, no call). No online match was played with it. The Windows
program was built (`BT3_CC=win64`) and not run. The window in a picture lower than 800 lines was not looked at.

## The gameplay fingerprint (2026-10-13)

Step 1 of matching online players by their data: the game now computes one number for the data a fight is
computed from and **prints it in the log**. Nothing is enforced, nothing is sent, no interface.

    bt3: gameplay data: afd9336a (tables 1b07c338 characters 39b40b5f animations dc20c449 stages 57279192 effects de456880 scripts 5c25c3bc menus b269586a): the original disc's
    bt3: gameplay data: 2c0f... (...): modified: tables, characters differ
    bt3: gameplay data:   tables: pzs3us1/00001.bin (fighter tables): entry 0x8
    bt3: gameplay data:   characters: pzs3us1/01423.bin (character 0 costume 0): entry 0x12, entry 0x13

**The rule.** A change that only alters what is seen or heard never changes the fingerprint: pictures of any
kind (HUD and announcement graphics, portraits, menu art, character skins, stage and effect textures), text,
music, voices, sound banks, texture packs, added songs. Only data that can change what the game computes counts.
The data check above ("is every file the original's?") asks another question and stays as it is.

**Why it can be trusted where it says "same".** Two things were checked beyond byte edits. (1) A run of the
replay check with 3.8 MB of the excluded data of every pack that fight loads inverted (all effect, stage, light
and model textures, the skill names): health, positions, clock and generator are identical on all 29,484
vertical blanks compared (`BT3_HASH`), while one changed health value differs from the fight's first blank.
(2) The stage layout below was used to write a stage file whose texture file is 128 KB longer, with all 831
offsets behind it rewritten: the game loads it and the same fight is identical on all 29,599 blanks.

### Definition

Code: `port/src/plat_fprint.c`; second implementation and maker of the table of the original's values
(`port/src/plat_fprint_tab.h`, hashes only): `port/tools/make_fingerprint.py [--root DIR] [--write] [--diff]
[--list ID]`. The two agree on the original data, on each of the 50 edits below and on the modified image.

- **What is read** is what the game would load: `Port_FileOpenHost` (plat_fcache.c), the function the loader
  itself now uses (a stage / song list's file, else `mods/<path>`, else the original). A file is taken in whole
  2048-byte sectors, the rest of the last one zero, as `ADXF_ReadNw` hands it to the game.
- **Hash**: SHA-256, the first 64 bits kept. An *item* is the hash of a byte stream built from one block of a
  file; a *file* is its list of (item id, hash); a *component* is the hash of, for each of its files in the
  order below, `id (u32), item count (u32; 0xFFFFFFFF = file missing), then per item id (u32) and hash (8 bytes,
  big-endian)`; the *fingerprint* is the hash of `"BT3FP", version (u32, = 1), the seven component hashes`.
  All u32 little-endian. The log shows the first 32 bits of each.
- **A pack** (word 0 = count N, words 1..N+1 = offsets w): entry k starts at `w[k] & ~3`, is empty when
  `w[k+1] == w[k]` (the game's own test, `BObjFile_GetEntry`) and runs to the next larger offset of the table or
  the end of the file. Offsets are never hashed, so an entry that moved is the same entry. An entry whose offset
  lies outside the file is `X` (packs rebuilt by modding tools have such entries, and a last word of zero).
- **Streams**: `E` empty; `D, length (u32), bytes` data; `T` a texture file; `X` outside; `P, N, then per entry
  its flag byte (w & 3) and its stream` a pack; `H, N, the flag bytes` a file's header item (id 0);
  `W, length, bytes` a file that is not a pack at all (item 0xFFFF).
- **The generic walk** (used where the entry map is by content, not by number): empty -> `E`; a *texture file*
  -> `T`; a *well-formed pack* (offsets in order and inside the block, every byte outside its entries zero), up
  to four levels deep -> `P` with each entry walked; anything else -> `D`. A texture file is what
  `Res_RelocateOffsets` takes: count 1..1024, entry table offset in words, 0x40-byte entries, and every pixel /
  palette packet inside the block, a multiple of 16 bytes, starting with three zero words and a VIF DIRECT code
  (0x50......) whose count spans the packet. Because the class is part of the stream, a definition block
  dressed up as a texture file changes the fingerprint like any other edit.

Files, in order (game file id n is `pzs3us1/(n-1)`); 1,780 files, 28,267 items, 334 MB hashed of 780 MB read:

| component | files | items |
|---|---|---|
| tables | 2; 4 | file 2: header; entry 1 `T` if it is a texture file (the fighters' light texture), else data; entries 2..13 and any past 213 as data (animation flags, AI data, camera cuts, voice table, attack records, base health / ki / blast stocks, reference pose ...); entries 14..213 together as item 0x100 (the lip tracks objects share). File 4: header, entries 1 (character table) and 2 (item table) |
| characters | 0x590 + c*10 + k, c = 0..160, k = 0..7; then 0xD1C..0xD47 (partner objects) | header; 2 (body volumes); 3 (skeleton, below); 0x12..0x2C each (parameters, attacks, ki blasts, movement, talk table, chains, supers, skills, AI, camera cuts, motions kept in the model file, camera animations); 0x35..0xFC together as item 0x100 (lip tracks). Not counted: 1 (HUD face), 4..0x11 (mouth, body, eye textures), 0x2D..0x34 (skill names) |
| animations | 0x598 + c*10 | one item: `P`, N, every motion as data |
| stages | 0x171 + s, then 0x198 + s (the two-player files an online match loads), s = 0..0x22; 0x1BF, 0x1C0 | header; entry 1 as four items (below); entries 2..N by the generic walk (parameter block `MAPD`, camera animations, object animations `EPCK`, AI way points, `MEF0`, path table `EVPp` count; the texture files 6, 0xA, 0xB, 0xE, 0x12, 0x14, 0x15 do not). Transition files: header and entry 1 (the camera animation the load waits for) |
| effects | 3; 0x599 + c*10 | header; every entry by the generic walk |
| scripts | 0x1FF..0x230 | one item: `S`, length, the file |
| menus | 0x1C1 | section 2 (the character select's and team select's resources, the screens a session runs) byte-pair decoded, then header and every entry by the generic walk. So the tables that say which character, stage and song a cursor position means count, the pictures do not, and neither does how the section is packed |

**Skeleton (model file entry 3, magic `pmdl`).** Stream `M`, the header's first 0x28 bytes (magic, flags, the
four sizes), the record count, then per record of the chain from `+0x6C` (`next` at +0, `last` at +6): `1` and
bytes 4..0x60 when `next > 0x40` (the record carries a mesh: bytes 0x40..0x60 are its centre and extent), else
`0` and bytes 4..0x40. Not hashed: `next` (it grows with the mesh), the vertex streams behind the records, the
header from 0x28 on (alpha, texture slots, GS block numbers), the seam section. If the entry does not walk
(no magic, a record outside it, a `next` between 0x41 and 0x5F) it is hashed whole. Measured on the 742 model
files: all walk; 44,027 records, 10,186 of them without a mesh (the 742 last records not counted in that).

### The two layouts that were open

**Stage file entry 1** (derived from `BtlStage_Relocate`, src/sys/gfxm_e_d.c, which is decompiled now, and the
structures of battle/stg_a.h, stgm_a.h, eft_det_b.h, col_a.h; checked on all 70 fight stage files). The entry is
the stage object; every offset in it is in WORDS from the entry's start. Header words:

| +0x10 / +0x14 | timers: count, table (0x10 each) |
|---|---|
| +0x18 / +0x1C | 7 draw groups -> materials -> meshes -> VIF chains |
| +0x20 / +0x24 | 9 lists of always-drawn parts |
| +0x28 / +0x2C | visibility table: size, bytes |
| +0x30 / +0x34 | transform nodes: count, table (0x50 each: matrix, key count, keys, end frame, body) |
| +0x38 / +0x3C | zones: count (100 on stage 0), table (0x40 each) |
| +0x40 | bounds: radius, top, bottom |
| +0x44 / +0x48 | destructible objects: count, table (0x50 each) |
| +0x4C, +0x50 | culling tree: root size, root node |
| +0x54 | the stage's texture file |

In the file the order is: nodes, keys, draw data, parts, visibility, part lists, **texture file** (word 0xC6AE0
of stage 0), culling tree, zones, collision meshes, objects, bounds. So the collision lies BEHIND the textures
and every one of its offsets changes with their size: nothing position dependent may be hashed. The four items:

- 0x101 nodes: `N`, timer count, per timer bytes 0..4 and 8..0x10; node count, per node bytes 0..0x44 and
  0x48..0x50, then its `keyCount` keys of 0x20 bytes.
- 0x102 collision: `C`, zone count, per zone bytes 0..4, `nearCount`, bytes 0x10..0x18 (with the record count),
  bytes 0x1C..0x40 (centre, half size), the neighbour list, the zone's mesh, then per collision record its
  `broken` and object words, its last word and its mesh. A mesh: `0` when the offset is 0, else `1`, its first
  three words (node count, polygon count), the nodes (0x20 each), the polygons (0x20 each), the vertex count
  (largest index used + 1) and that many vertices (0x10 each).
- 0x103 bounds: `B` and the 12 bytes.
- 0x104 objects: `O`, count, per object bytes 0..0xC (with its health), its six parts, the piece count, the
  pieces, bytes 0x2C..0x50. A part: `0`, or `1`, its flags, the INDEX of its node, and bytes 0xC..0x20 (radius,
  position).

Left out, all drawing: groups, materials, meshes, chains, texture animations, the visibility table, the part
lists, the culling tree, the texture file. If anything points outside the entry the whole entry is hashed as
data (item 1). The study's worry that a stage's vertex count sets the number of generator draws on haze stages
does not hold: `StgHaze_Step` draws for a fixed 20 x 50 grid.

**Effect packs** (common file 3: 637 entries; `0x599 + c*10`: entries 1..5 the packs of the technique slots,
6 the character pack, itself a pack of packs). There is no table of entry numbers that separates them: the
consumers bind by number per effect module, and the packs have another entry list for every technique. What
does separate them is the content, measured over all 162 files (102.8 MB):

| kind | share | counted |
|---|---|---|
| texture files (7,059) | 84.7 MB | no |
| headerless definition blocks, all under 4 KB: particle definitions (0x300), colour tracks (0x140, 0xC0), emitter sets, slot parameters | 13.0 MB | yes |
| key tracks `V000` | 3.1 MB | yes |
| camera animations `CMAn` | 1.5 MB | yes |
| models `pmdl` (25), `DBES` blocks | 0.4 MB | yes, whole |

All 7,059 texture files have their entry table at word 8 and are covered completely by header plus packets, and
no block over 4 KB is left that is not one of the named kinds, so nothing that carries hit radii, timers or
behaviour is taken for a picture.

### What the port adds, and what a session switches off

| addition | gameplay? | in the fingerprint |
|---|---|---|
| `mods/<path>` | as the file it replaces | yes: it is what is hashed |
| added stages (`stages/`, plat_stages.c) | yes, their collision | no: the stage selects do not offer them in a session (`extra = Port_NetSession() ? 0 : ...`, menu_c_e.c, menu_e_b.c), nor the stage swaps of `BT3_STAGE_REPLACE`, and the stage buffer has the game's own size. They are hashed like a stage and printed on a line of their own: `N added stage(s) <hash> (not offered online, not counted)` |
| added songs (`songs/`, plat_songs.c) | no | no; not offered in a session either |
| texture packs (gs_texpack.c) | no (replaced at upload, in the renderer) | no |
| movies | no; not played in a session (plat_movie.c) | no |
| `BT3_FILE_ALIAS` | - | reaches only the streams and movies (`Port_FilePath`); the pack loader does not read it (the comment in plat_stages.c that says so is out of date) |

Audio is out for good: since d48b19b on `port` a session takes every stream's length from a table of the
original disc's lengths built into the program, not from the player's file, so neither samples nor headers can
reach a fight. An "audio lengths" line was planned as an uncounted extra and has been dropped altogether (it
would have opened 65,201 files for a number that means nothing). The desync check added to net.c in the same
change is the other half: the fingerprint says BEFORE a match that two data sets compute the same, the check
says DURING one that they did not; a cause the fingerprint does not see (below) is caught there.

**Before a match (2026-10-19).** Each copy sends its fingerprint to the other one in a packet of its own (`T_FPRINT`,
net.c: `{MAGIC, 12, "1RPF", low half, high half}`) with the lobby's greeting and its answer (room codes, quick
match, an address) and again while the session connects. When both know theirs and the two differ, there is no
match: the lobby stops with "Your game data and the other player's are not the same..." (the host answers with its
own, so both sides say so), and a session that got that far goes back to the menu with the same line. Until then
two players with different discs (a modified image on one side) were matched, and the desync check ended the
match a moment into the fight. Two copies with the same modified image have the same fingerprint and are let in.
A copy that does not send it (0.1.37 and older) is let in as before; `NET_VERSION` stays. `BT3_TEST_FPRINT=<hex>`:
testing, a copy claims that value. `BT3_LOBBY=direct:<port>` / `direct:<address>:<port>`: the lobby by address
without its window (tests).

### Cache and cost

- `<root>/.fingerprint` (binary, 389 KB): per file its path, size and modification time (nanoseconds on Linux)
  and its items. A file is read again only when one of the three changed, a `mods/` file appearing or going
  included; the definition's version is in the header.
- It runs on the data check's thread, before the 3 GB comparison when that has to run; the game does not wait.
  `BT3_VERIFY=0` now turns off the comparison and its notice only: the fingerprint is still computed. Without a
  window (`BT3_GS=none`) nothing runs unless `BT3_VERIFY=1`, as before.
- Measured (this machine, NVMe): 1780 files read and hashed, files not in the system's cache 2.57 s; in the
  cache 1.04 s; every later start 9 ms for the whole program in `BT3_FINGERPRINT=1` mode (1,780 `stat` calls).
  The replay check running beside the cold pass printed its result before the fingerprint line.
- `BT3_FINGERPRINT=1`: compute, print with EVERY differing file named, and end (a tool).
- For the rest of the program: `int Port_DataFingerprint(unsigned long long *out)` (1 ready, 0 still being
  computed, -1 not computed in this run), `Port_DataFingerprintPart(part, &out)`,
  `Port_DataFingerprintOriginal()`, `Port_DataFingerprintText()`.

### Tests

Each edit on a copy of the file in the `mods/` folder of a scratch data root made of links; game and tool agree
on every line. "LARGER" = the entry made longer and the offset tables rewritten.

| edit | expected | got |
|---|---|---|
| character 0 health 40000 -> 50000 (file 2 entry 8) | changes | changes: tables, entry 0x8 |
| AI data byte (file 2 entry 4) | changes | changes: tables, entry 0x4 |
| attack record byte, an attack's damage (model file entry 0x13) | changes | changes: characters, entry 0x13 |
| damage-taken scale 1.0 -> 0.5 (entry 0x12) | changes | changes: characters, entry 0x12 |
| body volume offset (entry 2) | changes | changes: characters, entry 0x2 |
| skeleton rest vector (entry 3, a node record) | changes | changes: characters, skeleton |
| lip track byte (entry 0x35) | changes | changes: characters, lip tracks |
| animation: a byte of motion 1 (hit windows are in the motion) | changes | changes: animations |
| stage collision: a vertex of zone 0 (0x198 and 0x171) | changes | changes: stages, stage collision |
| stage bounds radius 1200 -> 1500 | changes | changes: stages, stage bounds |
| stage object health 10 -> 99 | changes | changes: stages, stage objects |
| stage parameter block (entry 2) | changes | changes: stages, entry 0x2 |
| item table value (file 4 entry 2) | changes | changes: tables, entry 0x2 |
| effect definition byte (0x599, pack 2 entry 6; file 3 entry 4) | changes | changes: effects |
| battle script byte | changes | changes: scripts |
| versus menu stage id list (section 2 entry 39) | changes | changes: menus, entry 0x27 |
| animation file cut in half | changes | changes: animations |
| stage change camera animation (0x1BF entry 1) | changes | changes: stages |
| character texture texels (entry 0xC) | same | same |
| mesh vertex (entry 3, vertex stream) | same | same |
| HUD face and mouth textures (entries 1, 4) | same | same |
| skill names (entry 0x2D) | same | same |
| music samples; voice samples; a voice stream's length in its header | same | same |
| member HUD face block (file 8) | same | same |
| fonts, HUD and announcement sheets ("FIGHT"; files 5, 6, 7) | same | same |
| fighter light texture (file 2 entry 1) | same | same |
| stage change model (0x1BF entry 2) | same | same |
| stage sprite texture (stage entry 0x14) | same | same |
| stage texture texels INSIDE entry 1; stage mesh data inside entry 1 | same | same |
| effect texture texels (0x599 pack 2 entry 5; file 3 entry 3) | same | same |
| battle sound bank sample; a character portrait (0x2F9) | same | same |
| versus menu art texels; the menu section packed another way with the same content | same | same |
| LARGER character texture file (+16 KB, entries 0xD.. move); LARGER HUD face (all 251 later entries move) | same | same |
| LARGER effect texture inside a nested pack (inner and outer entries move); LARGER file 3 entry 3 (634 entries move) | same | same |
| LARGER stage texture file, entry 6 (+64 KB) | same | same |
| LARGER texture file inside stage entry 1 (+128 KB; collision, zones, objects move, 831 offsets rewritten), 0x198 and 0x171 | same | same |
| a pack with its trailing padding cut off (file 4, 3 bytes shorter) | same | same |
| an added song and a texture pack | same | same |
| an added stage | same, own line | same; `1 added stage 8cdb321e (not offered online, not counted)` |

**A real modified disc** (a modified disc image the user supplied; unpacked into a scratch folder, laid over the
original as `mods/` would, removed afterwards). It is a full conversion: 2,599 of 3,401 entries of the second
archive and 37,348 streams differ. All seven components differ; of the 1,780 files of the list 1,736 differ on
disk and 1,733 in a counted item (two partner object files and one stage file differ only in textures). What
it showed:
- Its packs are not the tidy ones of the disc: 6 + 1 + 3 + 1 files end a few bytes short of their table's last
  offset (padding cut off), 253 model files have entries 0x30.. pointing past the end (unused text and lip
  entries left with old offsets), 35 have a last word smaller than the one before. The first version of the
  reader refused all of these (299 files "not readable as a pack", which made their pictures count); the pack
  rule above comes from them. One file is left that is no pack (it begins with zeros).
- Not counted and correctly so: each of the three texture-only files by itself gives the original's
  fingerprint; so do all 38,211 files of the image outside the list at once (HUD sheets, 322 member faces, 163
  portraits, stage pictures, the other menu archives, sound and voice banks, every stream); so do the original's
  versus menu archive, stage 0 and three model files with the image's pictures put into them (22 menu textures;
  up to 14 face / mouth / eye / body texture and text entries per model file, 64 KB longer). Inside its effect
  packs 2,365 texture files differ from the original's at the same place (29 MB) and none of that is hashed.
- Counted: in the model files every counted entry class differs in 590 to 1,293 files; the fighter table file
  differs in exactly one entry (8, base health / ki / blast stocks); 48 stage files have other collision, 52
  other bounds; a texture ENTRY that appears or disappears in a stage file counts (33 and 30 cases), which is
  intended: the stage effect tasks are created by whether the entry exists.

### Known false alarms (a visual or audible change that DOES change the fingerprint)

- Lip tracks (model file 0x35..0xFC, file 2 14..213). Counted on purpose: each mouth change the track commands
  draws the libc generator the fight uses (`BtlObj_SetSubState` -> `rand()`), so a re-timed lip track is a
  different fight. A dub that brings its own lip tracks changes the fingerprint.
- A texture entry added to or removed from a pack, or a texture file in a shape the recogniser refuses (no VIF
  DIRECT code at a packet, a packet outside the block).
- A re-meshed EFFECT model (25 `pmdl` blocks in the effect packs are hashed whole; character models are split).
- Stage file entries that are not texture files but may be purely visual or audible: 7 (holds a `DBES` block),
  0xD (animated surfaces), 0x16 / 0x17 (the boundary wall's colour and parameter blocks); the colour tracks and
  particle definitions of effect packs (they are definitions, but a recoloured effect is a changed block);
  the versus menus' animations and text entries.
- A nested pack rebuilt untidily (offsets out of order, non-zero padding) is hashed whole, pictures included;
  top-level packs are read by the lenient rule.
- A skin made as a model swap with another skeleton or other node bounds; a character file whose header flags
  changed.

### Open

- The fingerprint covers the DATA. The program's own tables are checked at start already (`Port_LoadGameData`)
  and the port's code is covered only by the version; environment overrides are not covered at all, and three
  of them are not switched off in a session: `BT3_TEST_STAGE`, `BT3_TEST_BGM`, `BT3_TEST_SCREEN` (every battle
  on a named stage / song / screen mode; with the first an added stage could be loaded in a session), and
  `BT3_STAGE_BUF`. The step that enforces should refuse them in a session.
- The menus component is the character / team select's pack only. The screens a session passes after a fight
  (result, rematch) and the first-run overlay were not traced for data that decides the next fight.
- Sound banks are left out whole. Battle code asks a bank one thing, `Snd_IsLoopSe`, which decides whether a
  handle is kept in a per-side set; no path from there to health, position or a timer was found, and none was
  proven absent. How the port clears a bank's `pending` flag in the first load was not read.
- Readers still not traced, all counted: file 2 entries 9 and 12; model file entries 0x1A, 0x25..0x28, 0x2C;
  stage entries 7, 0xD, 0x13, 0x16, 0x17.
- Stage ids: 35 fight stages (0..0x22) are hashed. Files 0x194..0x197 (scene backdrops) and 0x1BB..0x1BE are
  not; whether id 0x23 can be a fight stage was not settled.
- Tournament, story, mission and training set up opponents from menu packs that are not hashed (not reachable
  in a session today); the scripts component covers their battle scripts.
- Added characters (branch `charslots`) are outside the data folder; when they can be used online their files
  belong in the list.
- Windows: the code is plain C with `stat`; it was built and run on Linux only here. The cache's time is in
  seconds there.

## The build key: only official releases at the matchmaking service (2026-10-13)

Asked by the user: the project is open source, so anyone can build the game and point it at the same matchmaking
service, which spends the keeper's Cloudflare allowance (relay logins above all) and lets a changed build into
quick match. This is the code for closing that, and nothing is closed by it yet: everything is off until the keeper
sets the secrets. The keeper's steps, the table of stages and the format are in `port/matchmaking/README.md`
("The build key"); this is what was built and what was checked.

**What it is and is not.** A release is built with a secret of its own, the build key; the game signs each request
to the service with it; the service, told the keys, can refuse what is not signed. The key is in every copy of the
release, scrambled only so that `strings` does not list it. It can be taken out by anyone who takes the program
apart. So it is per release (a key that got out dies with its release), and it is not anti-cheat: it tells a
release from a build of the source, nothing more.

**In the game.**
- `port/src/gs/net_sign.c` (new): SHA-256 and HMAC (small textbook versions; the port had MD5 and SHA-1 in
  `net_relay.c` and a SHA-1 in the setup program, no SHA-256), the key's unscrambling, `Sign_Request`.
- The signed text is six lines joined by a line feed: `BT3-SIGN-1`, `POST`, the path (`/v1/quick`), the release
  (`PORT_RELEASE`), the time in seconds since 1970, and the SHA-256 of the body in hex. The signature is
  HMAC-SHA256 of that with the key, in hex. Sent as `X-BT3-Release`, `X-BT3-Time`, `X-BT3-Sign`.
- `net_match.c`: `http_post` adds the three headers on all three ways out (WinHTTP: in the header text beside
  Content-Type; the curl library: `CURLOPT_HTTPHEADER` with `curl_slist_append`; the `curl` program: `-H`). A
  program without a key adds nothing. The request to GitHub (the update check) is never signed.
- The clock: the service allows five minutes either way, and players' clocks are worse than that. The game keeps
  how far the service's clock is ahead of its own (`sClockAhead`): learnt from `/v1/hello`, whose answer now has
  `now`, and from a refusal for the time, after which the request is sent once more.
- A refusal (`{"error":"build key",...}`, status 403) is reported where the other refusals are, as the search's
  failure in the online window: "Quick match needs an official release of the game."; for a room code "Room codes
  need an official release of the game. Playing by address still works."; if the time was still refused after the
  second try, "This computer's clock is wrong: set its date and time, then try again."

**The key's way into the program.** `BT3_BUILD_KEY` in the build's environment. `port/tools/buildkey.py` (new)
checks its form (16 to 128 characters of letters, digits and `. _ - + / =`; anything else stops the build, without
showing it), scrambles it and gives it to the compilation of `net_sign.c` alone as two macros. It is written to no
file and printed nowhere. `port/release/build.sh` and the workflow hand the variable to the container by name
(`-e BT3_BUILD_KEY`: the value is on no command line); `inside.sh` prints `build key: given` or `build key: none`.
The workflow takes it from the repository secret `BT3_BUILD_KEY` **for a tag's build only**; a push to main, and
any fork, builds without.

**In the Worker** (`port/matchmaking/worker.js`): the secret `BUILD_KEYS`, JSON of release -> key; the variable
`REQUIRE_KEY`: not set / `off` (checked, logged and counted, nothing refused), `quick` (`/v1/quick` needs a
signature), `all` (everything but `/v1/hello`). Without `BUILD_KEYS` nothing is checked at all. The signature is
compared whole (no early exit), and checked before the time, so only a holder of the key learns that its time is
off. A key of a release below `MIN_RELEASE` is not accepted, and the header's release must be the body's `rel`
(else a retired release's key could be used under a newer number). `/v1/status` gains `buildKey`: the releases
that have a key, the stage, today's counts, and what is wrong with the settings if something is. No key and no
signature is logged or reported.

**Known limits.** A signed request can be repeated unchanged for five minutes (no record of signatures is kept).
Releases up to 0.1.33 have no key: when `REQUIRE_KEY` goes to `quick`, `MIN_RELEASE` should go to the first keyed
release with it, or those players read "The matchmaking service does not know quick match yet." instead of being
told to update.

**For testing:** `BT3_SIGN_TEST=1` prints the signed text and the signature of one fixed request (with a fixed
dummy key that is in the source, and with the program's own key if it has one; the key itself is never printed)
and ends before the game starts. `BT3_SIGN_TEST=post` also sends one `hello` the ordinary way, and only to this
computer: `BT3_MATCH_URL` must name `http://127.0.0.1` or `http://localhost`.
`port/matchmaking/test_sign.py <program>` uses both.

**Verified** (every key a made-up test value; nothing was sent beyond 127.0.0.1, and the deployed service was not
contacted):
- Linux, built without `BT3_BUILD_KEY`: `test_sign.py` 8 of 8: the text and the signature equal Python's `hmac` /
  `hashlib`; one request each through the curl library and the `curl` program, with none of the three headers.
  The whole request as the listener got it (request line, every header, body) is the same as from the keyed
  build but for the three headers.
- Linux, built with a dummy key: 17 of 17: the same, the signature with the built-in key equals Python's; the key
  is not in the program in the clear; on both ways the three headers arrive and verify, another key and a changed
  body do not; a listener whose clock is 100000 seconds ahead refuses once and gets the request again, signed for
  its clock.
- Windows (`BT3_CC=win64`, cross-built, with the dummy key), run under Wine with no display: the fixed request's
  lines are the same, and the WinHTTP way passes the same checks (headers, signature, the second try). Wine has no
  `curl.exe`, so the program way was not run there.
- A key of the wrong form stops the build with a message that does not show it.
- The regression line (`tick 2111 ... hp 23050 0`) is the same without and with a key.

**Not verified:** `port/matchmaking/test.mjs` was extended (the stages, wrong key, wrong release, stale time,
changed body, another path, `MIN_RELEASE`, an unreadable `BUILD_KEYS`, the status, the log, and the game's fixed
request given to the Worker) but **not run**: there is no Node on the machine this was written on, so the Worker's
new code has not been executed at all. Run `node port/matchmaking/test.mjs` before the Worker is deployed. Not run
either: the release build in its container and the workflow (so the secret's way through GitHub and docker is
written, not seen), a real Windows machine, and anything against Cloudflare.

## Online: the pause menu no longer stops the fight (2026-10-13)

Asked for by the user, against players who hold the other in a pause. Until now START in an online match was the
game's own pause: both copies run the same game from the same buttons, so the fight stopped for both for as long as
the one who pressed it liked.

The game keeps two flags: `BATTLE_FLAG_PAUSE_MENU` (the menu is open and run by the fight state,
`BtlSeqFight_Update`) and `BATTLE_FLAG_PAUSE` (everything that moves checks it). In an online match
(`Port_NetActive()`; offline nothing changes) only the first is set:

- `src/battle/btl_seq.c` `BtlSeqFight_Update`: START opens the menu without `BATTLE_FLAG_PAUSE` and without
  pausing the streams and sound group 4; the fight, its clock and its sound go on. If the battle ends under an open
  menu (knockout, time up), the menu is closed (`PauseMenu_Reset`); not when the menu itself ended the battle, where
  it stays up through the fade as in the original.
- `src/battle/btl_input.c` `BtlInput_Sample`: the fighter of the player whose menu it is takes no buttons and a
  centred stick while it is open (the buttons are the menu's).
- `src/battle/hud_0.c` `PauseMenu_Draw`: the pause menu is drawn only on the copy of the player who opened it
  (`BtlPause_GetMenuPad() == Port_NetMe()`). The result menu after a battle is drawn for both, as before.
- `src/sys/snd.c` `gPortSeSilent`: the menu's sounds are asked for on both copies, with volume 0 on the other
  player's.
- `src/battle/hud_e_d.c` `BtlPause_CheckOpen`: the pause for a pad pulled out is not taken online.

The menu and its state are part of the game on both copies as before (one is open at a time; the other player's
START does nothing while it is). Only drawing and volume differ between the two, which the game's state does not
read back: with both copies showing the same view the checksum of the whole state differs from the blank the menu
is first drawn, the fight's values do not, and the desync check (which notes the fight's values) stays quiet.

The versus pause menu has four entries: Continue Battle, View Skill List, Return to Character Select, Return to
Main Menu. There is no restart. The two "return" entries end the battle for both players as before (leaving a match
cannot be prevented anyway).

Tests: two copies without a window over 127.0.0.1, buttons from a table (as for the desync, above), START and the
menu's buttons put into one player's part of the table; `BT3_HASH` logs compared blank by blank.

| case | result |
|---|---|
| player 2 opens the menu for 300 blanks, closes it | clock 1832 -> 1982 while open (the build before: stays at 1833); player 2 is hit meanwhile; fight values the same on both copies to the end of the run |
| the same, each copy with its own view, pictures taken (GPU renderer, offscreen) | the menu ("2P PAUSE") on player 2's picture only; the same clock on both |
| Return to Character Select picked | the battle ends for both, the next battles run, values the same |
| menu held open until the time is up | the battle ends, the menu is gone, the next battles run, values the same |
| menu held open by the player who is then knocked out | the same |
| skill list opened, paged and left, by player 1 and by player 2, with 8 ms lag and 5% loss | values the same |

Not done / known:
- the button that closes the menu reaches the fighter as a new press on the next blank (Continue Battle with X);
- the title still says PAUSE;
- only one menu at a time: while one player's is open the other's START does nothing;
- not tried between two machines, nor with a transformation or a cut scene starting under an open menu by intent
  (the random tables may have done so, unchecked).
- Both players need this release: the fight is computed differently from before once START is pressed (it is in the
  release that carries `NET_VERSION` 4).

## Online: "Random" gave the same fighters in every session (2026-10-13)

Reported by a player: Random at the character select of an online match is always the same, Yajirobe, then Cooler.

Cause: a session is a fresh start of the program on both sides, and the port seeds the game's random numbers with a
fixed number at start-up (`func_002BB390`, plat_sys.c: 0x12345678, so that a run can be repeated). The session's
start-up is the same every time, so both of the game's generators (the C library's `rand`, and the twister of
`src/sys/rand.c` that `Rand_Range` draws from: the Random cell, the random stage, the random song) were in the same
state at the character select of every session.

Fix (port/src/gs/net.c, port/src/headless.c): the host draws a number when it starts (`seed_draw`), it travels
behind the host's other choices in `T_HELLO_ACK` and `T_CONFIG` (one more word at the end; no copy joins without
the first of the two), and both copies seed both generators with it where the session enters the versus mode, the
way `Rand_Init` does at start-up. `BT3_NET_SEED=<n>` on the host sets the number for tests; 0 leaves the generators
alone (the sequence every session had before).

Tests (two copies without a window, buttons from the same table, as above): two sessions with drawn numbers differ
from each other and from before (random numbers, when the first fight starts and how it goes), the two copies of
each agree in the whole state; with lag and loss too; with `BT3_NET_SEED=0` the session is blank for blank what it
was before the change. Not done: the Random cell itself was not driven (the tables do not go to it); what was
checked is that the generator it draws from differs from session to session.

## Online: three entries less in the menu at the end of a battle (2026-10-13)

Asked for by the user. The menu after a versus battle (`gPauseMenuResultVersus`, 0x2C5408) has six entries: Fight
Again (id 0x13), Return to Character Select (0xD), Return to Vs Menu (0xE), Return to Main Menu (0xF), View Replay
Data (0x18), Save Replay Data (0x17). Online, "Return to Vs Menu", "View Replay Data" and "Save Replay Data" are
taken out
(`src/battle/hud_0.c` `PauseMenu_Init`, `PauseMenu_PortRemove`): the entries behind move up and the count goes
down. The menus are data (a `BtlMenu` with its `BtlMenuItem`s); the engine finds entries by id and sizes the box by
the count, so any menu of any mode can be shortened this way, or an entry stepped over with its `disabled` word.
Done on both copies at every battle start; a session ends with a fresh start of the program, so offline has all six.

Test: two copies as above. The picture of both players' screens shows the three entries left; with random presses
through four battle ends (Fight Again, the character select) the two copies agree in the whole state.

## Dragon History: "Fight Again" in the pause menu (2026-10-13)

Asked for by a player: a way to start a story fight over without leaving it. The pause menu of battle mode 1
(`gPauseMenuMode1`, 0x2C5970) has Continue Battle, View Skill List and Surrender. The pause menu of mode 6 has an
entry that restarts the battle (id 0xC, with the yes / no box): `PauseMenu_Update` ends the battle with the reason
0x8000 for it, `BtlSeq_Update` raises `BATTLE_FLAG_RESTART` at the sequence's end and `Battle_Loop` calls
`Battle_Restart`. The port puts a copy of that entry into the menu of mode 1, in front of Surrender
(`src/battle/hud_0.c` `PauseMenu_Init`; a table of five entries of the port's own, as the game's has no room). In
mode 6 the entry reads "Reset standing position" (text 19); the copy takes text 3, "Fight Again" of the versus
result menu, so it is the game's own text in every language. The box behind it is the game's ("EXIT ?", yes / no).

Test (one copy without a window, pictures offscreen): a recording of presses from the start of the program into
the first fight of Dragon History (Goku against Raditz), with more presses put behind it. Restart right at the
start of the fight; and after a minute of fighting, with Goku's health almost gone and two of the fight's story
lines already spoken: the fight starts over with its opening scene, both health bars full, the story lines come
again at their places, and the fight goes on to its K.O. and the mode's result screen. Not tried: a fight in which
a fighter is exchanged by the story before the restart, and the other modes that may run as battle mode 1.


## macOS on Apple Silicon: pull request 68 merged (2026-10-13)

By valenvivaldi (pull request 68, eight commits, kept as they are). A native arm64 build, `BT3_CC=mac64` ->
`port/build/bt3_mac`, built on the Mac from source with Homebrew's LLVM (`install.py` does it; there is no macOS
release archive). How it works is in `docs/port/macos_plan.md`. In short: macOS keeps the low 4 GB unmapped, so a
game address is an offset in a 4 GB window that starts 0x20000000 below the program (an LLVM pass,
`port/tools/macwin/macwin.cpp`, rewrites the game's code for it; the port's own code says `Port_Ptr` /
`Port_Addr`, `port/src/port_addr.h`); software float is done by the code generator (`llc` without a floating-point
unit); the shaders are translated to MSL for Metal. The author reports the demo fight with the same state as Linux
on all 4428 frames and the rewind test without differences at depth 1 and 8.

The merge with the local work had no conflicts. With it the Mac build has the desync check of online play
(`Port_NetDigest`, `NET_VERSION` 4) like the other two: a Mac copy that computes another game than its opponent
is noticed.

Changed on top of the pull request, so that nothing of it reaches the Linux and Windows builds:

- `AGENTS.md` (instructions to coding agents, written by the contributor) is not taken. What it said is in this
  file, `docs/agent_rules.md` and `docs/port/macos_plan.md`.
- The shared build tools: what was changed for every build now applies to the macOS build alone. `eeconst.py`
  (a float after `introduced=`, `deprecated=`, `obsoleted=` left as text: only with `BT3_CC=mac64`),
  `ptr32.py` (a cursor kind the Python bindings do not know taken for an attribute: only on macOS; elsewhere it
  stops the tool as before), `link.py` (ld64's wording of a missing or doubled symbol: only for mac64),
  `make_dat.py` (the name with Mach-O's underscore: only for mac64), `extract_disc.py` and `install.py` (the
  disc read with `bsdtar` when there is no `7z`: only on macOS; Linux asks for `7z` as before).
- `Port_Ptr` and `Port_Addr` are macros with the plain casts on Linux and Windows (they were inline functions),
  so the compiler is given the code it was given before the names existed, at any optimisation level.
- Left as the pull request has it: `make_state.py` joins the ranges with a binary search in place of a pass over
  the whole list for each (the same condition, faster; the macOS map lists every symbol, so its list is long). It
  runs for every build.

Checked (on Linux, the two builds made from clean in the same folder before and after): the Linux program is the
same file as before the merge but for its debug information and build id (compared with both removed); the Windows
program is the same file with the debug information removed (the linker's time stamp aside); `bt3_64.mem`,
`bt3.exe.mem` and both `.dat` lists are the same files; the state log (`BT3_HASH`) of `replay01` is the same on
every vertical blank written (over 74000; tick 2111 is at 5431), and the replay ends as always (`hp 23050 0`).

Not tested here:

- The macOS build itself. No Mac was at hand: that part was read, not built or run. What the author measured was
  measured before the merge; the code the local work added since (the signed matchmaking requests
  `gs/net_sign.c`, the data fingerprint `plat_fprint.c`, the controller's rumble, the pause menu changes in
  `src/battle/hud_0.c`) has never been compiled for macOS.
- Online play between a Mac and Linux or Windows. A Mac build gives itself out as any other (`NET_VERSION`), and
  an opponent cannot tell. The demo fight's equal state says the arithmetic is the same; a match on two machines
  has not been played.
- On macOS the start-up test switches `BT3_SIGN_TEST` and `BT3_CRASH_TEST` are not there (its `__wrap_main`
  in `plat_mem.c` does not read them).
- The 32-bit Linux build (`BT3_CC=gcc`) was not built for this.

## Added songs from MP3 files: pull request 70, and what was changed in it (2026-10-13)

Merged: pull request 70 by valenvivaldi (his commit kept as it is). An `.mp3` in the `songs` folder is converted
to the ADX the game plays (`port/src/gs/song_mp3.c`: dr_mp3 decodes, an encoder of the port's own writes ADX type 3
at the MP3's rate and channels), the release's README texts, `docs/port/adding_songs.md`.

A review had named seven things. Each was looked up in the code again; all seven are there. What was done:

1. The start waited for the conversion (`PortSongs_Init` called it before listing the folder: in the test program
   below it came back after 4.3 s for an MP3 of an hour, now at once). Now a thread converts
   (`PortSongs_Mp3Start`); it only reads and writes files of the songs folder and counts what it finished. The song
   list stays the game thread's: made at start from the ADX files there are, and added to, at its end only, when a
   character select sets up its music list (`PortSongs_Refresh`, the two `#ifdef PORT` places in
   `src/menu/menu_c_e.c` and `menu_e_b.c` that read the list). Not in an online session (added songs are not
   offered there; the list is not touched). In a run with `BT3_PAD_REC`, `BT3_PAD_PLAY`, `BT3_PAD_TABLE` or
   `BT3_SYNCTEST` the conversion is done at start as before, so that a recording's menus depend on the folder and
   not on the conversion's speed. `BT3_REPLAY` and `BT3_DEMO` set up no select screen.
2. "Converted already" was `the ADX's date >= the MP3's date`: an MP3 replaced by a file with an earlier date was
   not converted again, and an ADX dated before its MP3 was converted at every start. Now the ADX written says in
   its header which MP3 it was made from (a mark, the MP3's size and time of last change; header 0x60 bytes instead
   of 0x28); another size or time: converted again. Dates are no longer compared.
3. An ADX made by hand with the name of an MP3 was written over whenever the MP3 was the newer file. Now an ADX
   without the mark is never written over (checked once more just before the new file takes the name); the log
   says the MP3 was not converted.
4. An MP3 that could not be decoded was tried, and the notice shown, at every start. Now it is written down in
   `mp3-failed.txt` in the folder with its size and time, and neither tried nor reported until the file changes.
   Failures to write are not written down (not the MP3's fault): tried and reported at every start.
5. `dr_mp3.h` is not a release: it is dr_libs' master at commit 51e61d3 ("v0.7.4 - TBD"). Its git blob id is the
   one GitHub reports for the file at that commit (asked from api.github.com for mackron/dr_libs, read only;
   nothing else was fetched and the file was not replaced). The last tag is mp3-0.7.3; the snapshot was kept because
   what it has more is an overflow fix in tag parsing, and the files read are whatever a player has. Written down
   in `port/third_party/dr_libs/README.txt`. The header itself is unchanged.
6. The whole MP3 was decoded into memory (an hour of music: 622 MB measured). Now 8192 samples at a time (14 MB
   for the same file, the test program whole); the sample count is written into the header at the end. The ADX
   data is byte for byte what the pull request wrote (three files compared, one of an hour).
7. Names. `.Mp3` in `songs.txt` was listed as a song file itself (only `.mp3` and `.MP3` were known there): any
   case now. `a.mp3` and `a.MP3` gave the same ADX, each start converting one over the other: the first by name
   wins, the other is left out with a line. Names of 128 bytes and more were converted and then not listed: left
   out. A song whose folder and name together are longer than the 192 bytes of the file layer's table was listed
   and silent (cut short; there before the pull request): left out with a line. Names with dots were right
   already. Windows: the port opens files by the system code page's names everywhere, so a name with characters
   outside it cannot be opened, as MP3 or as ADX; such an MP3 is now left out with a line saying to rename it
   (not changed: opening files by wide names, which would be a change to the whole file layer). The temporary
   file carries the process number (two copies started together each write their own); one left over by a copy
   closed mid-way is removed when a day old.

Tests. With a small program of the port's `plat_songs.c`, `plat_extras.c` and `song_mp3.c` and nothing else (a sine
tone made with ffmpeg as MP3, 44.1 kHz stereo and 22 kHz mono, in a folder of its own): first start converts;
second start converts nothing and the ADX is the same file; MP3 replaced by one dated 2001: converted again; the
ADX's date set after and before the MP3's: nothing; an ADX from ffmpeg with an MP3 of the same name and a later
date: kept, same bytes; a file of random bytes, one of 0 bytes and a text file named `.MP3`: one line and one
notice, then silence at the next start, the good file put in place of the bad one converted and its line gone;
`My.Song.v2.MP3`, a name with spaces and a quote, non-ASCII names, `a.mp3` with `a.MP3`, `.mp3` alone, a folder
named `dir.mp3`, 127 and 144 byte names, a folder name too long for the table; a folder without write permission
(reported at every start, nothing written down); left-over temporary files, three days old (removed) and new
(kept); a conversion made slow (the list at start without the song, with it after the refresh); the list in an
online session
(unchanged); the list with pad playback (whole at start). ffmpeg decodes the ADX written: the MP3's rate, channels
and length, the tone at 15.6 and 33.7 dB against ffmpeg's own decoding of the MP3 (the pull request's numbers,
with the same data). The same program built for Windows and run by wine without a display: converted, replaced
over an ADX that was there, hand-made ADX kept, a name with accents converted, a name outside the code page left
out with its line, `A.mp3` with `a.MP3` one left out.
With the game itself, without a window (`BT3_GS=none`, replay01): four MP3 files of which two bad, every
conversion made to wait 2 s (`BT3_TEST_SONGS_SLOW=2000`): the fight's tick 2111 is reached after 2.5 s as without
songs, the conversions end at 4, 6 and 8 s, the health at tick 2111 is the reference's in every run; second start:
"2 added", nothing converted, no line for the bad ones. Linux (`clang64`) and Windows (`win64`) link.

Not tested: a converted song HEARD, or played by the game's own player at all (the runs without a window start no
stream, and no recording of presses into a fight with the music select was at hand; what stands for it is that
the header meets every check of `snd_adx.c` `start()` and that ffmpeg plays the file); the song joining the list
in the real select screen (the list code was run by the test program, the two calls in the menus were compiled
and not run); the notice over the picture; the Windows program itself and a real Windows (wine only, and only the
test program); macOS, where the pull request was written; mp3-0.7.3 in place of the snapshot.

## Screen reader: speech and braille through Prism (2026-10-13)

Pull request #75, by a blind player (piapenia). The menus, the windows of the memory card, the pause and result
menus and, on request, the fight's gauges are said by the player's screen reader, and shown on a braille display
where it has one. Files: `port/src/a11y_game.c` (on the game's thread, once per blank from `Port_VBlank`: reads the
game's screens and says what changed), `port/src/gs/a11y.c` (the speaker: a thread of its own, the library, the
keys, the cues, the translations). Hooks: `Port_VBlank` (plat_stub.c), the two event loops (gs_gpu.c, gs_gl.c),
`ADXT_StartAfs` and `apply_volume` (snd_adx.c), and one in the game's code, `Font_NewCmd` (`src/battle/col_c.c`,
inside `#ifdef PORT`), which notes each string the game queues to draw.

It speaks through Prism (https://github.com/ethindp/prism, MPL-2.0), loaded at run time from `prism.dll`
(`libprism.so`) in the program's folder. The library is not in the repository nor in a release: the player puts it
there. Without it, and without `BT3_A11Y_LOG`, everything is off: no thread, no file, no key.

What is read: the title, the main menu, Dragon History (sagas, episodes), Ultimate Battle (missions, Sim Dragon,
survival, Disc Fusion), the World Tour, the duel menu and its settings, the character, team, stage and music
selects, training, the replay list, the character reference, the shop, the options, the memory card's windows, the
pause menu and the menu after a fight, item by item. A screen without a reader of its own has the strings the game
draws on it said together once they have stood still for 20 blanks; a line a voice speaks is left to the voice. In
a fight nothing is said by itself: F3 / F4 (or a click on the left / right half of a controller's touchpad) say that
player's health, ki, blast stock, powered-up mode and fighters left. Two short sounds, on the player's side of the
stereo picture, tell that one more transformation or the Ultimate Blast has come within reach (the game's own
checks, held for 15 blanks).

Settings (`bt3_settings.txt`; none of them in the settings window):

| key | meaning |
|---|---|
| `a11y=0` | off, even with the library there |
| `a11y_key_repeat` | the key that says the last message again, an SDL scancode (default F2); the touchpad click too, outside a fight |
| `a11y_key_gauges1`, `a11y_key_gauges2` | the keys for player 1's and player 2's gauges (default F3, F4) |
| `a11y_cue_volume` | the two cues, percent (default 60; 0: none) |
| `a11y_menu_voices`, `a11y_fight_voices` | the game's two voice channels, percent of their own volume, in the menus (default 50, so the reader is heard over the guides) and in a battle (default: the same) |
| `a11y_lang="pt"` | the language, a code of two or three letters (default: the system's) |
| `a11y_log=1` | what is said, into `a11y_log.txt` in the program's folder |
| `a11y_missing=1` | out of English, each line said without a translation, once, into `a11y_untranslated.txt` |

For tests, without a screen reader: `BT3_A11Y_LOG=<file>` turns it on with no library and writes what would be
said, one line each with its blank ("+" in front: queued behind the line before), and notes marked "#" (mode
changes, "fight", "gauges on screen", a cue). `BT3_A11Y_LANG=<code>`, `BT3_A11Y_MISSING=<file>`;
`BT3_A11Y_ASK=<n>` asks for the gauges every n blanks, the two players' in turn (no key can be pressed without a
window).

**How it stays out of the game's state.** It reads: nothing in a11y_game.c writes the game's memory or draws from
its random generators, and the game's functions it calls are getters (`BtlCharApi_GetHp` and the like,
`BtlAct_CanTransform`, `BtlChar_TestFlag`, `BtlPause_GetMenuPad`). Its own variables are the port's, outside the
ranges of the state list (`make_state.py`). A blank that is run again after a rollback says nothing and notes
nothing (`gPortResim`, in `PortA11y_GameTick`, `PortA11y_FontText` and `PortA11y_StreamStart`). What it changes
outside itself is sound only: the gain of the two voice streams and its own cue stream. Checked: the regression
replay with the reader off and on (log only, gauges asked every 300 blanks) gives the same `BT3_HASH` log blank for
blank; two connected copies (the pause-menu test above, `BT3_NET_SEED` fixed) give the same whole-state checksums
with the reader on as with it off, on both sides, over 40,000 blanks with rollbacks.

**Changed on top of the pull request:**

- Its controller vibration (`Port_PadRumble`) is left out: `Port_PadVibrate` (gs_input.c) was there already and
  does more (the strength setting, the online side, the window's focus). One difference in what the player feels:
  the pull request averaged the small motor over two frames, because the game switches it on in every other frame
  only (`BtlChar_UpdateVibration`, `src/battle/btl_char_get.c`: `small = toggle % 2 == 0`; `Pad_SendActuator`
  passes it on as it is). Ours passes on / off on as it comes. Not changed.
- A fight was told from `BattleWork.running` alone. A battle that a script sets up (Dragon History) has the whole
  battle work zeroed by `BtlScriptCmd_SetRule` -> `Battle_ClearWork` six blanks after `Battle_Main` set it, while
  the battle still loads: the reader said the result screen's name at once, and the gauges on request, the cues,
  the "not said in a fight" rule and the fight's voice volume were dead for the whole fight. Now `battle_watch`
  follows the battle: it starts with `running` (or the roster, `gBtlChars`), and is over when the roster has come
  and gone and `running` is 0. The HUD (`gHud`, which the game never sets back to NULL) is read only while this
  battle's roster and sequence (`gBtlSeq`) are there; before, the freed HUD of the last battle was read while the
  next one loaded.
- Online the pause menu belongs to the player who opened it and the fight goes on under it (above): the reader
  takes the other player's pause menu for no menu, as `PauseMenu_Draw` does, so it is not read and does not stop
  the cues and the touchpad's gauges.
- Prism's `PrismConfig` is returned by value and was declared by hand "from release 0.18.3": the memory given for
  it is 512 bytes now and no field of it is used but the first byte (its version, printed). Not guarded: the
  parameters of the eleven functions used, the feature bits, the ids of NVDA and JAWS; none of those functions
  tells the library's version, so there is no version check at load time. 0.18.3 is the release to use.
- Languages: the code has English only. The Spanish that was in a11y_game.c is `port/lang/a11y_es.txt`, in the
  same form as any other language's file: a line per label, the FNV-1a hash (64 bits) of the English, a tab, the
  line to say; the English is in a note above each line, so the file is also the list to translate from.
  `package.py` puts `port/lang/a11y_*.txt` beside the program; a program run from `port/build` finds them in
  `../lang`. A label that a language says two ways has a second key, "label|sense" (`trc`: "Random|song"); a file
  without it gets the plain key's line. `a11y_<code>_own.txt` holds a player's own lines, wins, and is not
  replaced by a release. A table of one player's renames and of a fan translation's abbreviations is out of the
  code: lines "~word<tab>words" in either file do that (in the game's own lines, whole words). None of the files
  holds text of the game: labels of the reader, and hashes.
- Small: an entry beyond a text file's table is not read; a mission's opponent index is bounded; `gSaveData` is
  checked before the options' volume is read; `a11y_log.txt` and `a11y_untranslated.txt` open by their UTF-8 path
  on Windows.
- Three faults of the port the author met: the question before the game closes (Esc) was a window the reader
  could not see, and is said now with its focused button and its keys (`build_quit`, gs/ui.cpp); the setup window
  had no keyboard navigation (`ImGuiConfigFlags_NavEnableKeyboard`: Tab reached the text field only); the setup's
  `--install <disc.iso>` and an image dropped on its icon failed on Windows for a path with letters outside ASCII
  (main's arguments are in the system's code page there; they are made from the wide command line now).

Tests (Linux, no window, `BT3_A11Y_LOG`): the recording of presses from the start of the program into the first
fight of Dragon History, with the gauges asked: before, "fight over" and the result screen's name six blanks into
the loading and every request "outside a fight"; now the gauges of both players all through the fight, the HUD's
technique names noted and not said, the result screen's name at the end. The regression replay (the versus path):
gauges and the menu after the fight as before. Two connected copies, player 2 opening the pause menu: said on
player 2's copy only, the gauges on both. Three seeded walks through the menus and the story fight in Spanish,
before and after the languages' change: the same lines, but the renames, which come back with the player's own
file. A stand-in library with a `PrismConfig` of 200 bytes: started, spoken through. The Windows builds link (the
game and the setup). The setup's `--install` under wine, without a display, for an image in a folder named with
accented and Japanese letters: before, "The disc image could not be opened" with the path in pieces; now the image
is opened and read.

Not tested: real speech and braille (no run with Prism and a screen reader was made here: the author's own, on
Windows with NVDA); the keys and the touchpad (the requests were made by `BT3_A11Y_ASK`); the exit question's
buttons (only its coming up was run); the setup's keyboard navigation (read in Dear ImGui's tabbing rule and built,
the window was not opened); anything on Windows itself (wine only); the Spanish of the three new lines of the exit
question (not the author's). Online only between two copies on one machine.

## Menu fonts: two settings that draw text from a TrueType font (pull request #64, 2026-10-13)

By Victorgonl (pull request #64), merged with fixes on top. Notes for users: `docs/port/menu_font.md`.

Two settings in F1 > UI, both "Original" unless chosen:

- "Menu font": the labels of the menus are pictures on the disc. Each label keeps its row in its texture, so the
  menus' animation, tint and layout go on working, and the row is drawn again from the font, several times the
  size (`port/src/gs/menu_text.c`, called where the renderer decodes a texture, `gs_draw.c` `texture_get`). Which
  texture has which label is found by a hash of the decoded texture in a list, `gamedata/game-text/labels.tsv`,
  which `port/tools/menu_font.py` makes from the user's own disc (397 labels; `labels.json` beside it is for
  editing them). The names of added stages and songs are drawn from the font too (`ui.cpp`).
- "Dialogue / UI font": the glyph cells of the game's own font sheets (`Font_Print`: dialogue, subtitles,
  ordinary text) are drawn again from the font. `Font_UploadTexture` (`src/battle/col_c.c`, under `#ifdef PORT`)
  hands the renderer the game's glyph table; the renderer packs the new glyphs into an atlas of its own and moves
  the sprite's texture coordinates there. The game's placement, line breaks, colours and shadows are untouched.

The bundled fonts (DejaVu Sans Condensed for Dialogue / UI and Condensed Bold for menus, with their licence; later
Fira Sans Condensed Bold for menus, under the SIL Open Font License: see the section on Tenkaichi Budokai Online) are in
`port/third_party/fonts`, with the list `fonts.tsv`; a release has them as `fonts/`.

What was changed on top of the pull request:

1. A damaged font file crashed the game at every start (reproduced: the pull request's build with a font cut to
   200,000 bytes ends in a segmentation fault before the title; the choice is saved, so every start after it too).
   stb_truetype reads what a file says without looking at the file's size. `menu_text.c` now checks a file before
   anything of it is used (`face_check`): the table directory, the tables a TrueType font needs, the glyph index,
   every glyph's outline the way stb walks it (composites to a depth of 4, no glyph inside itself), the widths, the
   character map, the kerning pairs. The character lookup is the port's own (`face_glyph`), kerning comes from the
   `kern` table only (`GPOS` is a tree of offsets stb follows unchecked), a glyph is drawn only if its box and its
   outline have a sane size, and stb's asserts are off. A font that fails is not used: the setting is back on
   "Original", and the log says once which file and why.
   Test (`port/tests/menu_font/fuzz.c`, built with the address and undefined-behaviour sanitizers): 36,030 damaged
   copies of the three fonts (cut short, flipped bits, fields set to 0 / 0x7FFF / 0x8000 / 0xFFFF, noise, tables
   exchanged, one glyph damaged, empty, larger than the limit), each put through everything the game does with a
   font; 18,211 were accepted, and all their glyphs drawn (88 million drawings); no report. All 232 `.ttf` files
   of this machine are accepted; a program, a picture, a text, an empty file, an `.otf`, a `.ttc` and a file over
   16 MB are turned down. (Built with `-fno-sanitize=shift`: stb builds 32-bit numbers as `p[0] << 24` in a
   signed int, in every font.)
2. The install step downloaded Pillow and fontTools and stopped the whole installation when anything failed.
   Pillow was not used; what fontTools did (a font's name and characters) is now a list file and a few lines of
   Python. The step needs Python and gcc, nothing from the network, and is skipped with one line saying why when
   it cannot be done (tried: an archive missing, an archive with one bit changed, no gcc). The menu archives are
   checked by size and CRC-32 before the USA disc's texture numbers are used. The unpacking and the hash moved
   into the C helper: 14 s -> 2 s, the output byte for byte the same.
3. The build no longer patches `src/battle/col_c.c` on the way (`portsrc.py` is what it was): the call is an
   `#ifdef PORT` in the file. The file preprocessed without `PORT` is the same bytes as before.
4. A release ships the fonts and licences (`package.py`), so "Dialogue / UI font" works there. "Menu font" needs
   the list made from the disc by the Python tool, so it is a feature of the source installation; a release
   offers only "Original" there. Nothing made from the disc is packaged.
5. Speed. A label is drawn only in the box its letters, outline and shadow can reach instead of its whole row;
   text is drawn at the internal resolution, at least 2x and at most 4x (it was at least 4x, up to 8x: one
   512 x 512 sheet was 64 MB, now 16 MB at most and 4 MB at the default 2x); a texture is hashed only when a font
   is chosen, it has a palette and its size is one a label has; drawn labels are kept (24 MB at most) for the
   next time their texture is decoded.
   Measured without a window (`BT3_FONT_LOG=1`: the time spent on text per decoded texture), a recording from the
   start through the title, the main menu, Dragon History, into a fight and its pause menu, both settings on, 21
   textures drawn:

   | | pull request | now |
   |---|---|---|
   | 2x, the four worst frames | 73 / 49 / 35 / 22 ms | 8.6 / 5.5 / 4.7 / 4.2 ms |
   | 2x, the whole recording | 304 ms (56 of it hashing textures that are no labels) | 56 ms (34) |
   | 6x, the four worst frames | 157 / 110 / 74 / 48 ms | 18 / 14 / 10 / 7 ms |
   | 6x, the whole recording | 602 ms | 104 ms |

   Frames over 33.4 ms at 2x: the pull request 4 in each of two runs (92, 70, 56, 40 ms: the title, the main
   menu, Dragon History, the pause menu); now 0, 3 and 5 in three runs, none of them long because of text (the
   longest, 172 ms, had 1.8 ms of texture work and 125 ms in submitting the frame; the same build without fonts
   and the `port` build have 0 or 1). The frame times of this set-up without a window scatter; at 6x a fight is
   over the budget on it with or without fonts, so nothing can be said there beyond the table.
6. "Original" is what it was. The texture cache's key had the internal resolution in it for every texture up to
   512 x 512 (changing the resolution decoded them all again); a font setting now adds to the key only for the
   textures it replaces, and nothing with "Original". The name of an added stage or song is in front of the
   windows again with "Original" (behind them with a font, where every name is drawn that way).
   Test: the same recording at 2x, a picture every 20 frames, the `port` build against this one with both
   settings on "Original": 140 of 140 pictures the same bytes. With the fonts on at 4x, this against the pull
   request's build: 112 of 113 the same bytes (the one, frame 1460, shows another hint line of the main menu in a
   fade: the game was at another moment there, the lettering is the same; two runs of one build differ in that
   frame too when several copies run at once).
7. DejaVu Sans Condensed was in the repository twice (once per setting); it is there once. It was listed for both
   settings at the time of this change and is now listed only for Dialogue / UI.
   The dead font-building code in the tool is gone. The notes said the game's ordinary text is unaffected; the
   second setting replaces exactly that, and they say so now.

Online: nothing here is game state. The game is only read (`Font_UploadTexture` passes its glyph table); what
`menu_text.c` keeps is in the renderer's object (`gs_menu_text.o`), which `make_state.py` leaves out of the list
of game variables: its variables lie outside every range of `bt3_64.mem` (checked in the map of the 64-bit
build), so a rollback neither saves nor restores them and the desync checksum does not cover them. Two players
with different fonts play the same match. Not run: an online session with a font on.

Left as it is: the regular DejaVu file is version 2.33 and the bold one 2.37 (the files of the pull request;
replacing one needs a download). Kerning no longer uses `GPOS`; in the compared pictures that changed nothing. A
label's texture stays in the renderer's cache like any texture. What the kept labels save was not measured on its
own: the recording hardly decodes a label's texture twice; the test program draws every texture a second time from
them.

Not tested: widescreen and "Menus in 4:3" with a font; the settings window itself (choosing a font there, the
lists); the OpenGL renderer; Windows at run time (the Windows build links); the menus the recording does not
reach (versus, options, the shop, the character select with added stages and songs); a release package made with
`package.py` (its font copy was run on its own, and the game was run with a `fonts/` folder beside it and no
labels: the dialogue font is used, the menus keep their lettering).

## Online: what a changed program on the other side can send (2026-10-13)

Asked by the user: can someone change the messages to send bytes that do something here? Read through
(port/src/gs/net.c `receive`, `net_recv`, the lobby's loop; net_relay.c `Relay_Unwrap`; net_match.c `json_str`):
a packet is read into 2048 bytes and never trusted for its own length (every field is read only when the size
received covers it; the inputs are copied 18 bytes at a time into a ring by `% RING`, as many as the packet really
holds; the check value's slot by `% SYNC_SLOTS`); in a match only the other player's address is listened to; the
name is 20 bytes with its end set, drawn as text and never used as a format; the host's delay and rollback are
taken only within 0..30, the rules within their ranges (`Port_NetRules`). Nothing received is a file, a path, a
length to allocate or a number of an entry in a table of the port. What a packet can do: put buttons in (which is
the game), end the match, or stall it (the same as leaving).

One thing was not held to its range: the other side's camera distance and screen shake (`sOpts`, a byte each,
anything up to 255 from a changed program) went into the battle's options as they came. The game's uses happen to
be safe (a `switch` with a default, and a comparison with 0), but `Port_NetSideOptions` now lets through only
what the options menu can set (0..2 and 0..1).

Not done: packets made at random thrown at the receiver (reading only).

## Menu fonts: the game makes the list of labels itself (2026-10-13)

The "Menu font" setting needs a list of which menu texture holds which label (`gamedata/game-text/labels.tsv`,
`name-styles.tsv`), made from the player's disc data. It was made by `port/tools/menu_font.py`, with a C helper
built by gcc while it ran, as a step of `install.py`: a release has neither Python nor a compiler, so there the
setting offered only "Original". The game makes the list now (`port/src/gs/menu_labels.c`), in a release too.
Notes for users: `docs/port/menu_font.md`.

- When. At start with a window, if `labels.tsv` is missing or its first line is not the current format's
  (`MENU_LABELS_VERSION`), and `fonts.tsv` has a font for the menus. The ten archives
  `gamedata/pzs3us1/00448.bin` ... are checked by size and CRC-32 as the tool checked them (the table is the
  tool's); any other disc: one line in the log, nothing made. `BT3_MENU_FONT_ANY_DISC=1` is the tool's
  `--any-disc`.
- Where it runs. In a thread of its own, started from `menu_text.c` the first time the font list is read
  (`GsGpu_Init`). The thread reads the archives and writes files; it touches nothing of the game's or the
  renderer's (the letter cutter `namefont.c`, which measures the name strips, is compiled into `menu_labels.c` a
  second time with its own variables, as the tool's helper had it). When it is done it sets one flag. The renderer
  calls `MenuText_Poll` between two frames (`gs_draw.c`, beside `scale_apply`): there, under `menu_text.c`'s
  lock, the fonts for the menus become available and the font named in the settings file is opened. The settings
  window reads and applies its values within one frame, so it never sees the change half way. While the list is
  awaited the window can only offer "Original" and applies that with every change of another setting: the font
  the settings file asked for is kept aside (`MenuText_FontIdToSave`), so it is not lost from the file.
  As `song_mp3.c` does for its songs, a run with `BT3_PAD_REC`, `BT3_PAD_PLAY`, `BT3_PAD_TABLE` or `BT3_SYNCTEST`
  makes the list at start instead, before anything is drawn (`BT3_TEST_LABELS_SLOW=<ms>` forces the thread, after
  a wait, for testing).
- The files are written under another name and renamed (`MoveFileExA` on Windows), `labels.tsv` last. A failure
  (another disc, a folder that cannot be written, an archive that is not what it should be) is one line in the
  log and leaves the setting on "Original".
- One catalogue. Which archive, section and texture a menu's labels are in, and what they say, is `ml_catalog` in
  `menu_labels.c` and nowhere else; the stage and song names are `namefont.c`'s tables (the tool read them out of
  that file with a regular expression). `port/tools/menu_font_extract.c` is gone, the step "Menu fonts" of
  `install.py` and its row in the setup window too (six steps). `port/tools/menu_font.py` is what is left of the
  tool, 200 lines of Python that need no compiler: it turns an edited `labels.json` into the two tsv files, and
  reads only that file (the colour styles too, which it used to have a table of). The game writes `labels.json`
  beside the list unless one is there, and never reads it.
- The release's README (`package.py`) says the setting is there.

Checked:

1. The same bytes. `python3 port/tools/menu_font.py --data gamedata --out <dir>` of the commit before, against
   what the new code writes from the same data: `labels.tsv` (397 labels), `name-styles.tsv` and `labels.json`
   are identical (`cmp`) from the test program built with gcc -O2, with clang -O2, with gcc and the sanitizers,
   with mingw-w64 run under wine, and from the game itself (the 64-bit clang build), made at start and made in
   the thread. The tsv files do not depend on the fonts installed: the tool looked at the fonts only to print
   which characters none of them has. The reduced tool applied to the game's `labels.json`, and to the one the
   old tool wrote, gives the same two tsv files again.
2. Time. 200 to 250 ms for the whole list (reading and checking 32 MB of archives included; the Python tool
   took 1.8 s). In the thread the start does not wait for it; in a recorded run the start is that much longer,
   once. With the list there, the start reads its first line.
3. Damaged archives (`port/tests/menu_font/labels.c`, gcc with the address and undefined-behaviour sanitizers,
   the disc check left out). 24 damaged copies of the archives (cut short, emptied, tables zeroed or filled, 1 to
   4096 bits flipped, table and size fields replaced): all turned down, nothing written. Damage after unpacking,
   which reaches the tables and texture headers: 3,600 rounds of bytes flipped anywhere and 4,800 rounds aimed at
   the fields the catalogue reads (over a quarter of those gave the list up, at eight different checks). One finding,
   in the first run: `namefont.c` divided by zero for a name row with a line of no fill between lines that have
   it (`style_build`, the normalising of the fill). The disc's sheets have no such row, but the stage select runs
   the same code on the sheets in the game's memory, so a replaced name sheet could have stopped the game there;
   fixed. Nothing after that.
4. In the game, without a window on screen, a recording from the start through the title and the main menu into
   a fight's pause menu, a font chosen in the settings file, a picture every 60 frames at 2x: started with the
   Python tool's list, and started with no `game-text` at all (the list made at start): 201 of 201 pictures the
   same bytes. With the list made in the thread (`BT3_TEST_LABELS_SLOW=1`): 201 of 201. With the thread held back
   6 seconds: the title and the main menu (frames 1140 to 1440, 6 pictures) have the disc's lettering, every
   picture after that is the same as with the tool's list. Without a recording: the list is made in the thread
   and the font taken; started again, the list is read; a list with an older first line is made again (a
   `labels.json` beside it is kept); one byte changed in `00452.bin`: one line in the log, "Original"; a folder
   that cannot be written to: the same.
5. Online. Nothing here is game state: the new file is `gs_menu_labels.o`, which `make_state.py` leaves out like
   every `gs_*` object; in the map of the 64-bit build its variables and `gs_menu_text.o`'s lie at
   0x20d11ae0 to 0x20d6eae8, outside every range of `bt3_64.mem`. The thread starts with the program, never
   later; `MenuText_Poll` is not called under an online session, so a list that gets finished during one waits
   for its end. Without a window nothing of it runs (the replay check runs so). Not run: an online session.

Not tested: Windows at run time beyond the maker under wine (the Windows build links; the thread, the hand-over
and the drawing were not run there); a release package made with `package.py` and its setup (the game was run
with `BT3_DATA` pointing at a folder like a release's `gamedata`); the settings window during the hand-over (read
in the code, not operated); the OpenGL renderer; macOS.

## White dots on a shiny character (2026-10-14)

Reported by the user: Perfect Cell's black, glossy parts had small white dots scattered over them. Replayed from
a recording of the user's presses (New Game, Duel, Goku against Perfect Cell) without a window: the dots are in
0.1.34's picture, not in the software reference's (`BT3_GS=1`), and not with `BT3_NZ=0`. So they came with the
rule of 0.1.31 that draws without the shader's own depth where "it changes nothing" (gs_draw.c, key bit 23).

Why it did change something: the full shader writes `floor(z * steps) / steps`, worked out in floats, and that can
come out one last bit of the float ABOVE the depth the chip interpolates for the very same triangle. A second
layer over the same triangles that does not write depth (the gloss) is then tested with the interpolated depth
against a stored value one bit larger, fails "greater or equal", and the pixel keeps the layer below: a dot.

Fix, first try (2080bfe, tagged v0.1.34.1 but never published): a depth bias of two float bits on the pipelines
of bit 23. Wrong: with 16 million depth steps a step is about one such bit near the far end, so the bias was a
step or two, and the user saw layers that should lose by one step win (shadow layers over clothes).

Fix as it is now, in the shader that writes depth (gs.frag): where the division came out above z, the step below
is written, so what is stored is never above the z it came from. Then "z >= stored" and "step of z >= stored" are
the same thing, which is what the rule of bit 23 assumed, and nothing is biased.

Checked with the recording, 60 pictures of the fight, every pixel: the default and `BT3_NZ=0` give the same
picture in all 60 (0.1.34: 45 of 60 differ, by up to 328 pixels; the bias: 12 of 60, by 1 or 2 pixels). Against
0.1.34's own full-shader picture, 8 of the 60 differ in one pixel each (the step below, where 0.1.34 wrote the
step above). The replay check is unchanged.
Not checked: Windows (Direct3D 12), macOS (Metal) and the OpenGL back end; other shiny characters; the scene in
which the user saw the shadow layers (not in the recording).

## Input lag: measured, and "Low input lag" (2026-10-14)

Asked by the user: how large is the input lag, can it be lowered, or is it fixed by the game?

`BT3_LAG_LOG=1` prints when things happen, in microseconds: `V` a vertical blank (after its wait), `W` the game
arriving at a blank's wait, `R` a pad read, `E` the window's events taken (a controller's state is renewed then),
`S` a picture handed to the window, `L` the owed blanks waited for (below). Measured with a recording of the user's
(a fight against Perfect Cell), replayed at real speed without a window on an integrated graphics chip:

- A fight's tick is two vertical blanks, 33.4 ms; a menu's is one.
- The game's own order in a tick: read the pads, compute, give the drawing commands, THEN wait for the blanks,
  then the picture is shown. Computing took 1.1 ms and showing 2 ms here, the wait the other 30.
- So from a pad read to its picture: 33.35 ms (median). The controller's state was 1.1 ms old when read.
- The game adds no tick of its own on top: with a recording cut off before a stick push and one that has it, the
  very first picture after the read differs (movement; other actions have their own start-up frames).

What is the game's: the 30 ticks a second of a fight (a press waits up to 33 ms, 17 on average, for the next
read) and each action's start-up. What was the port's to give: the 30 ms of waiting between computing a tick and
showing it. The console has the same wait and then draws and shows at the following blanks, so the port was already
ahead of it.

"Low input lag (experimental)" (F1 > Video; setting `late_input`; `BT3_LATE_INPUT=0/1`), off by default: a blank
the game asks for is not waited for there but owed (at most two) and waited for just before the next pad read
(plat_stub.c `late_wait`), where the controllers' state is renewed first (`PortInput_Fresh`). The tick is as long
as before; the pads are read at its end instead of its start. Nothing of the game's state changes: the same reads
and blanks in the same order. Not in an online session, and not with in-between pictures.

Measured the same way with it on: read to picture 3.0 ms (median; 99% 3.5 ms), the controller's state 0.01 ms
old, the tick 33.37 ms as before. The recording's pictures are the same with it on and off (seven compared), and
the replay check is unchanged.

Not checked: with a window on a real display, and by feel; the sound (the game's sound step runs at each blank,
and those now come two at once in a fight); V-sync on; Windows and macOS; a machine too slow to compute and show
a tick in time, where the picture then comes late instead of the game's start of the tick.


## Tenkaichi Budokai Online: lettered like the game's menus (2026-10-14)

The main menu's labels are not written in a font: every one is a picture on the disc (two label sheets per plate
state, 512 x 256 and 512 x 128, eight bits with a 256-colour table, eight rows of 32 on the first and three used of
four on the second). The PC's item "Tenkaichi Budokai Online" goes into the free fourth row of the second sheet of
each state (src/menu/menu_a.c).

Its label is now put together at start from the letters of the other ten labels of the player's own disc, with the
letter cutter the names of added stages use (port/src/gs/namefont.c, run on the files by menu_labels.c,
MenuLabels_MainItem). The main menu's labels have every letter it needs but the k; that one is lent from the stage
select's names (Rocky, Lookout) and brought to the menu's size, as namefont.c already does between the stage and the
song names. Read from archives 448 and 449 (size and CRC checked), the job takes about 60 ms in a thread of its own
started with the first frame; the main menu waits for it if it is not done. The picture is turned into the sheet's
own palette indices (the nearest colour of its table, alpha first) and written into the row. Composed again,
"Dragon Net Battle" cannot be told from the disc's. Nothing of the disc is in the repository: the letters exist in
memory only. With another edition of the game (archives that are not the USA release's) the label of
port_tbo_label.h is written instead, as before.

With a Menu font chosen, the item stayed in the disc's lettering, and so did the three labels on the same sheet
(Character Reference, Options, Shenron): menu_text.c finds a sheet by a hash of all its pixels, and the list of labels
had hashed the disc's sheet, without the item. (This was so from the start, with port_tbo_label.h too.) The list now
writes the item into that sheet as the game writes it (the same nearest colours of the sheet's table,
MenuLabels_ItemPixels, used by both), hashes that, and lists the item as the sheet's twelfth label; its version is
v8, so an older list is made again.

A room match found on these screens opened the screens again instead of the character select (seen by the user,
two copies on one machine: both logs say "the start-up is over ... the character select", then the hub, then the
session ended when it was left for the main menu). The session puts the game back to its first moment and goes to
mode 39 (headless.c), and Duel_Main runs these screens whenever gPortDuelRole is 6: the role, set when the item was
chosen and cleared only after the screens return, was still 6 in the session, so it is not among what the session
puts back. Duel_Main now clears it in a session.

The screen reader said nothing on these pages (reported by a tester with speech on): port/src/a11y_game.c reads each
of the game's own screens from its state, and these pages drew their text as pictures of their own. Now the hosts'
line is written by the game's own text box in the game's message window (battle/view_a_b.c, MsgWin_PortDrawString:
TextBox_AttachString, the game's font and place), which the reader already reads as the game's lines; one or two
lines start at the same height (the box's line offsets, MsgWin_PortSameTop), as before. And the pages say the rest
themselves (port_tbo.c, Tb_Speak): the title as a page opens, what the cursor is on with its value, a keypad's key,
the code (letter by letter) or name typed so far, the room's code while searching, the notes. Checked with
BT3_A11Y_LOG (what would be said, into a file; the user went through the hub, Quick Match, Create Room and Join
Room): the titles, every item with its value, the box's lines, the keys and the code as typed were all said. One fix
from it: what was typed and where the cursor went in the same blank are one line (two in one blank, the second cut
the first: the cursor's jump to JOIN after the sixth letter was lost). Not heard with a speech library here.

Also learnt on the way (how menus are made is in [menus.md](menus.md)): the message box is drawn after the
hosts and is see-through, so a host picture cut off at its edge shows the cut through the box (the announcer looked
legless until his picture was made to run down behind the box to the foot of the screen); and a game file that
fails to compile still lets the program link, with that file's old object (menu_labels.c once: "link OK", but one
stub more than before). Look for `error:` in the build's output, not only for "link OK".

The lines of the online screens themselves cannot be made that way: names, room codes and the hosts' lines need
every character, which the disc's labels do not have. They keep being written with the Menu font. Measured against
the disc's letters (cap height, widths of "Dragon Net Battle" and "Ultimate Training"), the game's lettering is a
bold, slightly condensed humanist sans, not a rounded one; of the open fonts tried (DejaVu Sans Condensed Bold, Fira
Sans Bold, Condensed Bold and Condensed ExtraBold, Noto Sans Bold), Fira Sans Condensed Bold is the nearest in
proportions and shapes. It is bundled (port/third_party/fonts/fira, SIL Open Font License 1.1, from Mozilla's Fira
release 4.301) and first in fonts.tsv, so it is the font of those screens while the Menu font setting is on
"Original". Saved choices are kept by the file's path, so adding it at the top moved no one's setting.

## Online: a desync no longer ends the match; waits and slow frames in the log (2026-10-19)

Players reported matches ending with "The two games no longer agree" (since 0.1.34, when the copies began to compare
what they compute) and freezes of up to half a second ("waited 500 ms/s" on the meter) while both only walked.

- **A desync is a warning.** When the copies' notes differ (`sync_check`) the player is told once ("your screen and
  the other player's may now show different fights"), the meter says "desync since frame N", and the match goes on
  as it did before 0.1.34. The log has the first differing blank and, as the session ends, how many comparisons
  differed. F1 > "End an online match on a desync" (setting `net_desync_end`, or `BT3_NET_DESYNC_END=1`) ends it
  instead, as 0.1.34 to 0.1.37 did. A copy of those releases still ends the match and says so to this one.
- **Waits in the log.** A wait of 100 ms or more for the other player's input (rollback past its limit, or every
  blank with rollback off) writes a line: how long, the blank, and what came meanwhile: input packets without the
  blank this copy needs ("their game is behind this one") or nothing for most of the wait ("the line held it
  back, or their game stood still"); at most 200 lines a session. The meter names the same per second ("the other
  player's game is behind" / "nothing arrived from the other player").
- **Slow frames in the log online.** In an online match a frame whose own work (the time waited for the other
  player left out: `gPortNetWaitNs`) is 100 ms or more is reported without `BT3_GS_VERBOSE`, with the new textures
  and pipelines it made. With both players' logs of one match, a wait on one side and a slow frame on the other at
  the same moment say which machine it was; a wait with nothing arriving and no slow frame on the other side is
  the line.
- **Windows: bt3_log.txt.** Started from the Explorer or a shortcut (no terminal, no pipe), the program writes what
  it says to `bt3_log.txt` next to it, the run before kept as `bt3_log_prev.txt`; nothing when that folder cannot
  be written. From a terminal or the setup's pipe, as before.

Tested (two copies on one machine, 40 ms each way, rollback 8): a desync claimed by one copy at blank 4000
(`BT3_TEST_APART`): noted once at 3968, the match went on; with `BT3_NET_DESYNC_END=1` it ended on both copies at
4005. One copy stopped twice for 0.6 s (`kill -STOP`): the other logged "waited 496 ms ... nothing came from them
for 613 ms" and "waited 478 ms ...", its slow-frame lines giving 497 and 479 ms of it as waiting. The Windows log
file is compiled (MinGW, syntax), not run.

The online pause (2026-10-13, above), the cases its notes left open: four matches with player 2 opening the pause
menu every 700 to 1100 blanks for 60 to 200 blanks (cursor moves, the skill list, sometimes Return to Main Menu)
under random input of player 1, about 33,000 fight blanks: no difference in about 29,000 comparisons. Whether a
transformation or an Ultimate Blast's cut scene began under an open menu was not checked.

## Songs in mp3, wav and flac played as they are: miniaudio (2026-10-19)

The conversion of dropped MP3s to ADX at start-up is gone (`port/src/gs/song_mp3.c`, and `port/third_party/dr_libs`
with it).
An added song in mp3, wav or flac is now decoded and mixed by **miniaudio** (`port/third_party/miniaudio`, 0.11.25;
`port/src/gs/snd_ma.c`) beside the SDL output the ADX streams use; the game's own files are untouched.

- Where it is intercepted: `port/src/gs/snd_adx.c` `start()`, the one place every BGM / map / voice / movie stream
  is opened. A file whose first two bytes are not 0x8000 (the ADX mark) and that `PortMa_Format` accepts is handed
  to `PortMa_Start`; everything else is decoded as before. No `Bgm_Play` call site changed, and an ADX (or a mod's
  `.bin` that is really ADX) keeps the old path exactly.
- Format: `PortMa_Format` accepts a name ending in `.mp3` / `.wav` / `.flac` (any case) or a file whose first bytes
  say RIFF, fLaC, ID3 or a bare MP3 frame, so a mod file under another name (a `.bin` in `mods/`) is recognized too.
- The player state is mirrored: volume (the same gain `snd_adx.c` computes for SDL, a11y percent and all), pan (one
  figure from the two channel pans), pause / resume, and "has it ended" (`ADXT_GetStat` polls `ma_sound_at_end`; a
  native stream loops when it is an added song or is on one of the two music players (0 BGM, 1 MAP), as the disc's
  own track did; a native voice line or streaming effect (players 2..5) reports `PLAYEND`, so the game goes on).
  `ADXT_SetOutputMono` is taken over at the next start (miniaudio cannot change a sounding stream's channel count
  in place).
- Volume (2026-10-19, reported: dropped-in songs sounded lighter than the game's own): both paths are given the same
  figure (`player_gain`, from `ADXT_SetOutVol`) and miniaudio passes it through unchanged — measured in the offline
  engine, a tone of 0.8000 in comes back 0.8000 out with pan 0 and spatialization off, a mono file too, and 1.5 gives
  1.1999 and 2 gives 1.5999 (exactly the multiplier). So the difference is the file: the game's ADX music is mastered
  loud and a file made outside the port usually is not. A native stream is lifted by `native_lift` (1.5 = +3.5 dB by
  default); `BT3_MA_GAIN=<multiplier>` sets it, and above about 2 a file already near full scale clips, as the engine
  does not compress.
- Build options (the manual, section 2.7), set in `snd_ma.c` before the include: `MA_NO_ENCODING`,
  `MA_NO_GENERATION`, `MA_NO_RESOURCE_MANAGER`. The last means a stream is decoded by the `ma_decoder` the port
  owns, created into the engine with `ma_sound_init_from_data_source`; the resource manager and its job thread are
  not built. The WAV, FLAC and MP3 decoders are kept. No extra link libraries (miniaudio loads its Windows and
  macOS backends at run time; on Linux it needs only `-lpthread -ldl -lm`, already linked).
- Song list: `plat_songs.c` scans the songs folder for `.adx`, `.mp3`, `.wav` and `.flac`, puts the four scans
  together and sorts once by name; `songs.txt` may name any of the four. `PortSongs_Refresh` is kept (`menu_c_e.c`,
  `menu_e_b.c` call it) but does nothing: nothing is converted in the background any more, so the list does not
  change while the program runs.
- The engine is made lazily, the first time a native stream starts, so a run with no native songs never opens a
  second device; without a device every native start fails, as the ADX path without SDL's device reports a stopped
  player.

Checked: `snd_ma.c` compiles clean with miniaudio 0.11.25 and the port's flags; `snd_adx.c` and `plat_songs.c`
pass the front end. Not yet run: a native song heard, the mixing of the two devices, and the release build.

Merged (2026-10-21) with one addition: the `.adx` an earlier release's converter wrote next to an MP3 (its header
has the mark `BT3:MP3` 1 at 0x30) is left out of the list while that MP3 is there, since the MP3 itself is now
played: a player who had added MP3s saw every one twice. A `songs.txt` line that names such a copy plays the MP3.
A copy whose MP3 is gone stays a song, and an ADX without the mark (made by hand, ffmpeg, add_song.py) is never
left out. The files are not touched. Counted at start, four folders: MP3 + its copy + WAV + FLAC gave 3 songs
(4 before); the copy alone 1; a hand-made ADX beside an MP3 of the same name 2; the list naming the copy 3. Heard
(Linux): WAV and FLAC songs in a fight, and the MP3 beside its old ADX copy.

## Spectators (2026-10-26)

The first stage of the lobbies: a third game, and up to six, can watch a one-on-one match. (Later an eight-place
lobby is to reuse it: two players, six spectators, the host in the middle.)

**How.** A session is computed from its first blank out of nothing but the two players' input (the chapters on
the session's start-up above: nothing else enters the game's state). So a spectator does not need to be sent
pictures or state: the host sends it the match's set-up, the same words the one who joins is told (rules and speed,
the seed, both players' camera and shake options, the names), and then the input of both players for every blank
from the session's first, as the host has it CONFIRMED: its own, and the other player's as it arrived, never a
guess (`spec_log_update`: every blank below the input known of both). The spectator's copy starts a session like
the players' (state put back to the first blank, or the program started as the session), runs the same unseen
start-up, and from the character select on reads both pads from what came (`Port_NetInput`): it computes the same
character select, battles and rematches, and its player-1 view is the host's picture.

- **Buffer.** It keeps about a second of the host's blanks in hand (`SPEC_BUFFER`, 60). With none left it waits
  until it has a second's worth again ("Waiting for the match..."); more than two seconds behind (it came in
  mid-match, or loaded slower) it runs blanks without picture, sound and pacing until it is back at one second
  ("Catching up with the match... N%", `Port_NetSpecSkip`, the way blanks after a rollback are run).
- **Whole match kept.** The host keeps every blank's input of the session (36 bytes a blank, 7.8 MB an hour; at
  most 1,000,000 blanks), so a spectator can come at any time and is sent the match from its first blank.
- **Packets** (`T_SPEC_*`, types 20 to 28, gs/net.c, the comment above `spec_pump`): greeting (with the
  fingerprint, the netcode version and a version of its own), wait, no (with why), set-up, data, ack, goodbye,
  the list of who watches (to the other player), ready (to a lobby's question). Data packets carry a run of blanks,
  a byte per step: `0x80|n` = the next n+1 blanks are the one before again, `1`/`2`/`3` = the pad(s) that changed
  follow; a packet's first blank is whole, so every packet stands by itself (a fight with random presses on both
  sides is about 15 bytes a blank, a menu nothing). After the blanks, the host's sure check values (`sync_check`,
  every 32 blanks) from the first not yet sent.
- **Unreliable line.** The spectator says every 50 ms how many blanks it has in a row, and at once when a packet
  came after a gap; the host sends again from there (after such a word, or after half a second without progress).
- **The players come first.** Nothing waits for a spectator. The host does its sending with its looks at the socket
  (`receive`), at most every 16 ms: a live spectator gets a packet every other blank or so (about 30 a second,
  3 to 4 KB), one with nothing new one every half second, and those that are catching up two packets of 1,150
  bytes every 16 ms between them all (about 140 KB a second at most, whatever their number). A spectator silent for
  8 s, or still a minute behind after a minute, is let go.
- **Same data, same release.** The greeting carries the gameplay fingerprint: different data are refused ("The host's
  game data are not the same as yours..."), so is another netcode or spectator version. A 0.1.38.7 host ignores
  every packet of a spectator: after 10 s without an answer the spectator says so ("No answer from the host...
  needs a release newer than 0.1.38.7 on the host as well"). Nothing the two players send each other changed:
  NET_VERSION stays 5, and the new build plays 0.1.38.7 both ways.
- **Desync.** The spectator compares the host's check values with its own. When they differ it says so ("this
  screen no longer shows the players' fight") and follows the match again from its first blank, once (a second
  difference is only noted). The players are never told or touched.
- **The view.** A session's battle is the game's two-player battle with both cameras computed on every copy; each
  player's copy shows its own one full screen (`BtlCam_UpdateOverride`, `Port_NetView`). The spectator chooses:
  player 1's (the default), player 2's, or both halves as the game draws them on one machine, V or R1 (and F1 >
  Change view). Only drawing: the fight's values are the same in every view (tested below; the game's whole state
  differs by the camera, as between the two players' copies).

**In the game.** The online window's Join tab has "Watch the match" beside "Join the match" (by address and port, or
by room code); the Host tab "Let others watch the match" (setting `net_spectators`, on by default; the host then
answers no). While the host has no match yet the spectator waits ("The host is there but its match has not begun
yet"). The players see "N watching: names" small in the top right corner; the spectator a line at the top
("Spectating A vs B, view: ...", "V or R1: change view, F1: stop watching"), and F1 has Change view and Stop
watching. (Later the same day the spectator's line and a pause menu of its own, START, became the game's: see
"Lobbies", "The screens".) Its controller and keyboard play nobody (and nothing vibrates). When the players leave, the spectator is
told and goes back to where it was.

**Room codes** (port/matchmaking/README.md, "Spectators"): the spectator asks the service for the room's host
(`/v1/watch`), the host learns the spectator's addresses with its polls (`watch: true`) and knocks at them for ten
seconds, letting them through its relay if it holds one. A host that lets spectators in keeps its room while the
match is on (`keep: true` every five seconds) and gives it up when the session ends; one that does not, gives it up
when the match begins, as before. Needs the Worker change deployed (a new table and the code).

**Limits.** Six spectators. A spectator behind a router that the host's knocking does not open, with a host that has
no relay address (the relay is only asked for when the two players needed it), cannot watch by code. Watching by
code needs the session without a restart on the host (the 64-bit programs with the state list). During a catch-up
the spectator sees the last picture and hears nothing. A match with a modified disc is watched only by a copy with
the same one.

**Testing.** `BT3_LOBBY=spectate:<address>:<port>` or `spectate:CODE` (the lobby without its window),
`BT3_NET_SPECTATE=<address>:<port>` with `BT3_NET_SESSION=1` (started as the session); `BT3_LOBBY_AT=<n>` (the lobby
begins at the n-th blank, as from the menus), `BT3_SPEC_VIEWS=<blank>:<view>|...` (0, 1, 2 = split),
`BT3_SPEC_LEAVE=<blank>`, `BT3_TEST_SPEC_APART=<blank>` (this spectator claims a wrong check value once),
`BT3_NET_LOSS` (a spectator drops that share of what it sends and receives), `BT3_NET_SPECTATORS=0|1` (the host's
setting), `BT3_NET_SAVES=<folder>` (the session's save folder, for several copies in one folder),
`BT3_SHOT_DIR=<folder>` (screenshots there, named by frame and blank: two copies' pictures of one blank compared).
A program started as the session now exits when the session ends (it kept no game to go back to; it used to crash).

Found on the way: a session begun from the lobby window (not by starting the program again) waited forever at the
language question of the start-up when no language had been chosen yet, or with "Always ask" on: it now never
asks in a session.

**Tested** (this machine, every copy offscreen and on 127.0.0.1; host and joiner fed one table of random presses
from the character select on, `BT3_NET_SCRIPT`, rollback 8, 20 ms each way, 60 s battles at 100%; `BT3_HASH` of
every blank compared, the last line of a blank counting):

| case | compared | result |
|---|---|---|
| two spectators from the start (one by the lobby, caught up 570 blanks; one started as the session) | 13,403 blanks, two battles and the rematch between | the whole state identical to the host's in every blank |
| a spectator coming mid-fight (at the host's blank 6,293; 5,151 blanks run without picture) | 13,942 blanks | identical |
| a spectator with 15% of its packets lost both ways | 13,998 blanks | identical (one wait of 0.74 s) |
| views 1 -> 2 -> split -> 1 -> split, and one that left at blank 10,000 | 4,380 and 5,893 fight blanks | the fight's values identical (the whole state differs from the first change: the camera); 312 check values compared, none differed; the host let it go at once |
| a spectator killed | | the host let it go after 8 s |
| a false check value at blank 4,000 | | noted, followed again from the start (2,597 blanks caught up), then 4,557 fight blanks identical |
| pictures: host and spectator, every 20th frame of 110 s | 169 pictures of the same blanks | 168 byte-identical; the first after the start-up differs in 58 pixels of its bottom-right corner |
| by room code through a local copy of the new Worker: one before the match (told to wait), one mid-match (1,851 caught up) | 4,798 blanks | identical; the joiner's log names them |
| seven spectators | | the seventh is told the match has six |

The players' pacing (host / joiner, `BT3_LAG_LOG`, 130 s): without spectators, pictures 33.26 ms apart on average,
99th percentile 34.7 / 37.6 ms, longest 38.0 / 38.1, no waits; with six spectators (without picture, so as not to
share this machine's graphics card) 33.26, 37.6 / 34.7, 38.8 / 37.8, no waits; rollbacks per 600 blanks within
the run-to-run spread of the random input (44 to 48). (With four spectators that draw too, the six pictures at 2x
on one graphics card made frames of up to 100 ms on the players' copies: this machine, not the netcode.)
0.1.38.7 against this build, host and joiner both ways: the fight's values identical over 4,283 fight blanks, no
desync; two 0.1.38.7 copies by room code through the new Worker: connected as before; a spectator of a 0.1.38.7
host: "No answer from the host..." after 10 s by address, the same or "No room with that code..." by code. The
Windows build compiles (mingw-w64).

## Lobbies (2026-10-26)

An eight-place lobby on top of the spectators (the chapter above): two players and up to six spectators meet at the
game of the one who made it, the owner. The owner puts people in the places, chooses the rules and starts the
matches; the two in the player places play, everyone else watches.

**In the game** (changed the same day: the lobby is one of the game's own screens now, no longer an ImGui window; see
"The screens" below). Tenkaichi Budokai Online's list has Lobby: it asks for nothing, opens a lobby with a room code
and shows its page at once, with the code to hand on. Others type that code on Join Room: the service's answer says
it is a lobby, and the game becomes a member and shows the same page. (By address, `BT3_LOBBY=lobbyjoin:` and the
Dragon Net Battle window's Join tab still work; the window then closes with a line saying that the lobby's screen
is in Tenkaichi Budokai Online, which the main menu goes to by itself.) The page (src/menu/port_tbo.c, `TB_LOBBY`):

- the places: 1P and 2P, then spectators 1 to 6, with the owner's mark, this game's own ("YOU") and who is ready.
  The owner chooses a name: Move (then the place: the two swap) or Remove from the Lobby. A newcomer takes the first
  free place (a player's while one is free and no match is on);
- the rules as rows with left / right: battle (single, team, DP 10 / 15 / 20), time limit, game speed (100 to 150 in
  steps, 114 marked as the standard), rollback, input delay. A new lobby starts from Create Room's choices. Everyone
  sees them change at once; while a match is on they cannot change;
- Start Match (the owner, with both player places taken; also START), Ready / Not Ready, Chat, leaving (triangle:
  Leave / Close the Lobby, with a yes / no window), the code (square copies it on a PC), and in the box at
  the foot of the screen the last three things that happened ("Vegeta joined the lobby", "Match 2: Goku vs Vegeta",
  the chat's lines).

**A match.** The game in 1P hosts it, the one in 2P joins it, as any online match (the character select, rematches,
everything as in a match from the Host tab), with the lobby's rules. Everyone else watches it as a spectator **of the
owner**: the owner's game passes the match on to them whatever its own part is (host; the one who joined: its own
confirmed input of both players; a spectator itself: what its host sent it, check values included). So the members
only ever talk to the owner, whom they can already reach, and the owner's view or part makes no difference to them.
The two players reach each other directly (when one of them is the owner, over the lobby's own connection); when
neither is, the host knocks at the other's address as the owner sees it and at its own public address (STUN), and the
one who joins greets both.

**Back to the lobby.** Only the owner, three ways: the game's pause menu mid-fight and its menu after a battle have
"Return to Vs Menu" for the owner's side (with the pause menu's yes / no box), which ends the battle on every copy and
the session with it (`src/battle/hud_0.c` builds the menus' two forms at a battle's start, the owner's with the entry
and the other player's without it and without "Return to Main Menu", and picks one each time a menu runs, by its pad;
the owner's side travels in bits 24..27 of the rules word, the same on every copy; `src/menu/menu_a_b.c` turns that
result into the main menu, which ends the session); F1 > "Back to the lobby"; and a spectating owner's own pause menu.
Every member's session ends on the owner's word (`T_LOBBY_BACK`) and all are on the lobby's page again, where the
rules can be changed and the next match started. Going back from the character select opens it again in a lobby's
match (`src/menu/menu_g_b.c`): only the owner ends it.

**A spectator's pause menu** (START, Esc or the key bound to START): only on its screen, nothing stops. Continue, View
(player 1's, player 2's, split screen, with left / right; also V / R1 outside the menu), Leave the Lobby (the owner:
Close the Lobby; a yes / no first), and for the owner Back to the Lobby. It is the game's (below).

**Leaving.** Anyone, at any time: the window's button, F1, a spectator's pause menu. A spectator leaving changes
nothing for the others (its place frees). A player leaving mid-fight ends the match: everyone is back in the lobby
window with "NAME left the lobby", and the owner fills the free place. The owner leaving closes the lobby for everyone
("The lobby was closed by its owner."). A member whose game falls silent for 15 s is dropped (a player's: the match
ends); a member that hears nothing from the owner for 15 s says the lobby is closed. Removed members read "The lobby's
owner removed you from the lobby." A full lobby answers "This lobby is full (8 places)."

**How** (port/src/gs/net_lobby.c; packets 30 to 40, ignored by older releases). The owner keeps the places, the
rules, who is ready; members greet it (`HELLO`: versions, gameplay fingerprint, name, own public address) and say
every second that they are there (`ALIVE`: ready, the match they were sent to, the match they are in, connected); the
owner sends the whole lobby every second and at each change (`STATE`, 368 bytes). `GO` tells a member its part in
match n (host, join an address, watch the owner) with the rules, rollback and delay; the session begins as one from
the online screens always does (the state put back to the first blank, gs/state.c) on the lobby's own socket, which the
session hands back when it ends (`Port_NetSessionEnd`: no close, no relay given up). `BACK`, `CLOSE`, `LEAVE`,
`CHAT` / `SAID`. Everything runs on the game's thread: outside a session from `Port_VBlank` (`Lobby_Pump`), in one
from the look at the socket (`receive`). The owner sends `GO` again to a spectator that is back on the lobby's page
while the match is on (a desync, its connection), and ends the match when a player's session has ended or the two
have not connected in 40 s. Nothing of the two players' traffic changed; a lobby is a release-after-0.1.38.7 thing.

Lobbies need the session without a start again (the 64-bit programs with their .mem list; otherwise the page says
so). The game is put back, after each match, to the moment its match was asked for: the lobby's page, where it was.

**Old releases.** A 0.1.38.7 with the code: the service answers "old" (it shows "Update available. Online servers
are unreachable until you update."). By address: the owner answers its greeting as a host of another release would ("The
host's game is a different version"). A new game greeting by address as for a match of two is told "lobby"
(`INVITE`) and becomes a member.

**The relay.** The owner holds the relay (net_relay.c now serves up to twelve addresses, each with a channel of its
own, and lets sixteen through): a member who joined with the code and has not reached the owner in eight seconds
makes it ask for one, once per lobby (`/v1/relay`, allowed to a lobby's owner once a member has come); members learn
its address by polling with their key. Cloudflare documents no limit on peers or channels per allocation that we
know of; TURN itself allows 4096 channels. **Not tested against Cloudflare's service** (no local TURN server here):
the multi-peer client is exercised only by code reading; a match of two through the relay uses one place as before.
The two players reach each other directly only (when neither is the owner): two players who cannot are told "The two
players could not connect to each other." after 40 s; routing their match through the owner is not done.

**The screens** (the same day, later: the lobby moved out of Dear ImGui into the game's own menus, with the elements
of Tenkaichi Budokai Online's pages; "the less we rely on ImGui the better").

- *The page* (src/menu/port_tbo.c, `TB_LOBBY`, drawn with the kit of port_screen.c like the other pages: the
  tournament grounds, the heading, windows, bars, the gold cursor bar, the pad's prompts, the Menu font). Left, the
  eight places in a window; right, the code (square: Copy Code), the rules and four buttons in two rows. Up / down
  move in a side, left / right go from the places to the right side and back (on a rule they change it; L1 / R1
  switch sides from anywhere). Cross on a place opens the owner's window (Move, then the place, the two swap;
  Remove from the Lobby; Back). Start Match, Ready / Not Ready, Chat, Leave / Close Lobby (a yes / no window; also
  triangle). The talk box at the foot shows the last three things that happened, each cut to a line, in the game's
  own font; before anything has, the hosts' hint. While the lobby is being reached a window says so (circle or
  triangle: cancel); when it is closed, why, and OK goes back to the list. The screen reader is told the page, what
  the cursor is on, what was typed and each new line of what happened.
- *Changed after the first test by the user* (the same day): the page has no Close / Leave button (triangle does it,
  with its yes / no): the owner's buttons are Start Match and Ready, with Chat across the row under them; a member's
  Ready and Chat, one row in the middle. Going back from a lobby match's character select is the owner's way back to
  the lobby for everyone (as "Back to the Lobby" in a fight: the session ends on every copy); the other player's
  triangle does nothing there (src/menu/menu_c_e.c, `CharSel_PortLobbyHold`; before, the select opened again). The
  players' pause menu and the menu after a battle say "Back to the Lobby" for the owner's way back (the item
  "Return to Vs Menu" kept, its text the port's: text number 0x6FFE, hud_0_b.c). A lobby's game that ends (its
  window closed, the game quit: atexit) sends what leaving does: the owner closes the lobby for everyone (CLOSE, three
  times, also in a match, from its socket then), a member leaves (LEAVE); the others know at once ("The lobby was
  closed by its owner." / "NAME left the lobby") instead of after 15 s of silence. And the player's name is the
  setting `net_name` (the settings file's, as Dragon Net Battle's window writes it; Profile writes it too) before
  online_name.txt: two copies with their own settings files had both read the shared file's "Player".
  Tested (by code, owner + 2): the player's pause and result menus with "Back to the Lobby"; the 2P player's
  triangle at the character select (nothing), then the owner's (everyone back on the page); the owner quitting in the
  lobby, at the character select and mid-fight (the members: "The lobby was closed by its owner." at once), the 2P
  player quitting mid-fight ("Vegeta left the lobby", everyone back); fight values identical in every match.
  `BT3_LOBBY_DO` has `quit` (the program ends, as when its window is closed).
- *A spectator's delay* (the same day, from the user's test with three windows): the spectator ran a fixed second
  behind the host (`SPEC_BUFFER`). It now runs 24 blanks behind at first (about 0.4 s at 114%), 12 more each time
  it runs out of the host's blanks (a stall of the line or of the host; at most the old second), and 2 fewer after
  each quiet 900 blanks (down to 18); a spectator further behind than its lead and 12 runs one blank in sixteen
  without picture until it is back (only beyond the lead and 60, as before, it runs without picture until caught
  up). Still only the host's confirmed blanks. Measured (offscreen, 114%, drawing, 100 s; `BT3_PRESENT_LOG`, the
  fight's pictures): the spectator's pictures 29.0 / 29.5 / 30.1 ms (5th / 95th / 99th percentile) before and
  29.1 / 29.4 / 30.0 after, no picture over 36 ms; 27 blanks behind the host's latest from the fight on, with 5% of
  its packets lost too; the host's pictures unchanged; fight values identical. The 20 frames a second of that test
  were the three windows at 8x with a texture pack on one graphics card (the players' too), and its 0.7 s pauses
  came in all three windows at once ("submitting the frame"): the card, not the netcode.
- *Reaching the owner* (from the user's test, a Windows owner and a Linux member on one network: the owner said the
  member had joined, the member never heard it): a member connecting takes the owner's first answer by the nonce of
  its greeting (a number from the system's random source, new for each attempt), which the owner's STATE now
  carries (LOBBY_VERSION 2: a game of the version before is told the versions differ), from whatever address it
  comes (an answer from another address of the owner's machine, or through its router with another port, was
  dropped); without it nothing is taken, and once in only the owner's address counts, as before. It says in the
  log where it greets the owner and what it did not take, and when nothing answers says
  so without guessing a version ("A firewall on the owner's PC may be blocking the game..."). The owner already
  knocks at the addresses of members the service tells of, as a room's host does, and asks for a relay after eight
  seconds. `BT3_TEST_FIREWALL=1`: this copy takes only what comes from addresses it has sent to (a stateful
  firewall). Tested: an owner with it, a member by code, ten times through a local Worker and twice through the
  live one: joined each time; forged answers (no nonce, a wrong one, a NO with a wrong one) from another socket of
  this machine: not taken. The user's case itself was not reproduced here.
- *Chat* is the name's keypad (letters, digits, -_.!?#&*+, space; the PC keyboard types any plain character but < and
  >), up to 60 characters, sent with Done or START; the lines are in the talk box. No ImGui is left in the lobby.
- *Getting there.* The hub's new Lobby (between Join Room and Friends; the list is ten bars now, 26 units apart
  instead of 27.5) makes one with Create Room's rules and the saved speed, asking nothing. Join Room with a lobby's
  code goes to the page instead of the search (`Port_LobbyPoll` 5). The screens opened while a lobby is on go to its
  page; so does the main menu (`Port_LobbyWantsScreen`, src/menu/menu_a.c), for a lobby joined from the Dragon Net
  Battle window (which no longer has a Lobby tab, and only says where the lobby's screen is) or by the test switches.
  The game is in the page when a match is asked for, so it is put back into the page when the match ends.
- *A spectator* (any spectator, of a lobby's match or of a match of two) has the line over the match at the foot of
  the screen ("Spectating A vs B", the view, R1: change, START: menu; catching up; a desync), drawn by the game over
  every screen of the match from `Gfx_EndFrame` (src/sys/gfx.c, `PortTbo_SpecDraw`) with the pages' kit; and, in a
  fight, its own pause menu (START, Esc, or the key bound to START) in the battle's own pause-menu box, drawn by the
  battle's own menu engine (port/src/spec_menu.c; `BtlMenu_Draw` of src/battle/hud_0_b.c, through
  `BtlMenu_PortDrawMenu`): the note "Your screen only: the fight goes on" where the battle's menus have their title
  picture, Continue, View: Player 1 / Player 2 / Split Screen (left / right), Back to the Lobby (the owner), Leave /
  Close the Lobby or Stop Watching, the latter with the battle's own "EXIT ?" Yes / No box (the versus pause menu's,
  `PauseMenu_PortConfirmBox`). The box grows and shrinks over six frames and plays the menu's sounds as the
  players' does. Nothing of the battle's own menus is touched: the engine's update is not run (the file moves the
  cursor and the box itself), its texts reach the engine through `BtlMenu_GetItemText` (numbers 0x6FFF and on), and
  the pad is read with `Port_PadRead` (the spectator's pad plays nobody). Outside a fight (the character select)
  there is no menu. Drawing goes through the game's own draw lists, so a spectator that shows the line or the menu
  differs from its host in those bytes of its state (the fight's values and the check values are the same: tested
  below); `BT3_SPEC_LINE=0` leaves the line out, and the whole state is the same again. The players' "N watching"
  corner and F1's entries stay as they were (ImGui).
- *Text.* Every line is in port_tbo.c, so `port/tools/tbo_strings.py` lists it for translators (ids `tbo/...`; notes
  for the new places). port/src/spec_menu.c is listed as well. The lobby's own lines come from the owner's game in English and are translated where they
  are shown: whole lines by the file, lines with a name by their end ("%s joined the lobby" and four more), "Match %d:
  %s vs %s" by its parts. The Dragon Net Battle window's one new line is port text `net/lobby_joined`.
- *Testing.* `BT3_TBO=1` opens Tenkaichi Budokai Online instead of the title (after the logos; with a lobby, its page);
  `BT3_PAD` (gs_input.c) drives the pages; `BT3_SPEC_PAUSE_AT` now opens the game's menu.

Tested (this machine, offscreen, 127.0.0.1, local Worker; players fed the random-press table):

| case | result |
|---|---|
| the pages with the pad alone: the owner takes Lobby on the list, two members type its code on Join Room's keypad (`BT3_PAD`, a fixed code from the local Worker) | lobby made and joined through the screens; a match, back, closed; the spectator's whole state identical to the host's over 6,972 blanks, the players' fight values over 7,030 |
| the owner's pad on the page: a rule changed twice, a member's window, Move and the place, the chat's keypad typing | the lobby logs the new speed and the swap; screenshots below |
| by address, owner + 3, two matches (the owner hosts and two watch; then the owner watches and passes the match on), `BT3_SPEC_LINE=0` | every spectator's whole state identical to its host's (5,731 to 5,782 blanks a match), the players' fight values identical; 179 / 180 check values each, none differed; all back on the page after each match |
| by code, owner + 3, the owner and a member watching; the pause menu (the battle's box) driven with the pad: every item (Continue, View to player 2 and split, Back to the Lobby, Close the Lobby then No, Leave the Lobby then Yes) | fight values of both spectators and of the two players identical over the match (1,290 to 1,365 fight blanks); 141 check values each, none differed; the owner's Back brought everyone to the page, the member left |
| a match of two: a spectator with the menu open, one with neither line nor menu | the first's fight values identical; the second's whole state identical (6,281 blanks) |
| by code, owner + 3: the owner a spectator, line and pause menus open (member and owner; first version of the menu) | fight values identical to the host's (4,048 / 4,050 fight blanks); 227 check values each, none differed; the owner back to the lobby (test switch): everyone on the page again with "The owner went back to the lobby." |
| by code: a spectator leaves mid-fight, the owner removes the 2P player mid-fight, a fifth joins and leaves, the owner closes | "Piccolo left the lobby" (the match goes on), everyone back with "Vegeta was removed from the lobby" (Vegeta: "The lobby's owner removed you from the lobby."), "Krillin joined / left", "The lobby was closed by its owner." |
| a match of two by code with a spectator by code (`BT3_SPEC_LINE=0`), a quick match | spectator identical over 10,106 blanks; fight values identical (6,933 and 7,447 fight blanks) |
| 0.1.38.7 against this build, both ways | fight values identical (4,283 / 4,282 fight blanks) |

Screenshots were looked at for each page (owner and member, a rule being changed, a member's window, moving, the
chat's keypad, a spectator's line, the member's and the owner's pause menu next to a player's own, a match of two's spectator, back on the
page after a match, removed, closed). The Windows build compiles (mingw-w64). Not done: a person with a controller.

**Testing switches.** `BT3_LOBBY=lobby` (with a code), `lobby:<port>`, `lobbyjoin:<address>:<port>`, and a code with
`join:<code>` or `spectate:<code>`. `BT3_LOBBY_DO=<s>:<command>|...` (each that many seconds after the one before was
done): `start`, `back`, `leave`, `kick <place>`, `move <place> <place>`, `rule <rule> <step>` (0 battle, 1 time,
2 speed, 3 rollback, 4 delay), `ready`, `say <text>`, `wait <members>`, `waitmatch`, `waitlobby`, `waitseq <state>`
(the battle sequence's state, 6 the result menu). `BT3_SPEC_PAUSE_AT=<s>`: a spectator's pause menu (the game's) opens
that long into its match (for screenshots).

**Found on the way.** The spectator's "Joining the match as a spectator..." stayed on its screen for the whole
match; a test line (`BT3_NET_LATENCY`) delivered what it held back from one session in the next.

**Tested** (this machine, offscreen, 127.0.0.1; players fed the random-press table, rollback 8, `BT3_NET_LATENCY=20`
where said; `BT3_HASH` of each copy compared session by session, the last line of a blank counting):

| case | result |
|---|---|
| owner + 4 by address, three matches: owner hosts (3 watch it); m2 hosts, m1 joins, the owner watches and passes it on to 2; m2 hosts, the owner joins and passes it on to 3 | every spectator's whole state identical to its host's in every blank (5,181 / 5,182 / 4,536 to 4,584 blanks a match), the two players' fight values identical; 1,399 check values compared, none differed |
| by room code through a local copy of the new Worker: owner + 3, two matches with players swapped, a fourth (`spectate:` with the code) joining mid-match | members in by the code; the late one caught up 359 blanks and was identical over 3,941 |
| 0.1.38.7 with the lobby's code / by address, before and during a match | "Update available..." / "The host's game is a different version" |
| a new game knocking by address with Join and with Watch, before and during a match | became members (the second watched the running match) |
| owner back mid-fight (F1 / the test switch), after a battle (at its result menu), with the game's pause menu ("Return to Vs Menu", Yes) | everyone back in the lobby window each time; the other player's pause menu has 3 entries, its result menu 2 |
| rules changed, players moved, started again | the members' copies log the same places and rules; the next match runs with them |
| a spectator leaves mid-fight; a player leaves mid-fight; a player removed mid-fight; the owner closes mid-fight and in the lobby | nothing / everyone back with "m1 left the lobby" / back with "m3 was removed from the lobby" (m3: "removed you") / everyone "The lobby was closed by its owner." |
| a ninth game | "This lobby is full (8 places)." |
| chat | every line reached every member once |

Pacing of the two players (owner 1P and a member 2P drawing, `BT3_LAG_LOG`, 150 s, 60 s battles): without spectators
33.28 / 33.28 ms between pictures on average, 99th percentile 35.1 / 33.9, longest 37.6 / 37.6, no waits; with six
spectators (not drawing, and computing the state checksum every blank) 33.06 / 33.21, 35.2 / 34.0, longest 43.1 /
42.9 (one picture over 40 ms each), no waits; rollbacks per 600 blanks the same (47.4 / 47.9). (Spectators that draw
on this machine's one graphics card stall the players' pictures by up to 1.9 s, with the spectator branch's own build
as much as with this one: this machine, not the netcode; the spectator tests above did not draw them either.)
Unchanged: a match of two with a spectator (whole state identical over 8,090 blanks), 0.1.38.7 against this build
both ways (fight values identical over 4,380 fight blanks), `node port/matchmaking/test.mjs` (17 new checks for lobbies, the earlier ones unchanged). The
Windows build compiles (mingw-w64).

## Online: the goodbye says why the match ended (2026-10-26)

A match that ended with "The other player left the match." could not be told apart: the other game says goodbye
(T_BYE) whenever its session ends in order, whether its player left for the menus or it had stopped hearing this one.
Seen by the user: a quick match ended so after five fights at a 300 ms round trip, and the log could not say which.

The goodbye's word (until now 0, or BYE_APART for a desync) says why: BYE_LEFT (the player left: the menus, or the
match ended for a reason of this side's), BYE_QUIT (the program was closed: an atexit goodbye, so the other side knows
at once instead of after QUIET_NS of silence; before, closing the window mid-match gave "The connection to the other
player was lost." six seconds later), BYE_LOST (this game stopped hearing the other one). The other side shows "left the
match", "The other player's game was closed", or "The other player's game lost contact with yours (a connection
problem on one side or between you)". When one direction fails both games soon wait for each other's input, so the one
that notices first says LOST; the message does not say on which side the problem was, because it cannot know.

Compatible both ways: 0.1.38.8 and older take any word but BYE_APART as "left the match", as before; a goodbye of 0
from them is shown as that too. Tested offscreen with two copies by address (`BT3_SESSION_TEST`): leaving
(`BT3_SESSION_LEAVE`), a killed program (SIGTERM: the atexit goodbye), and a one-way failure (new test switch
`BT3_NET_MUTE_AT=<blank>`: this game's packets stop reaching the other from that blank), and each against the 0.1.38.7
and 0.1.38.8 releases.

## Quick match: nearby players first, and a Region to choose (2026-10-26)

Players complained about the round trip of quick matches: the service put a searcher with whoever had waited longest
in the queue, wherever that was (a player in the Gulf got about 300 ms). Asked for by the user, and decided with him:
the nearest first, then everyone; a Region item for those who want only one region; the rival's region and the
measured round trip shown when the match is found.

**Where players are.** The Worker reads Cloudflare's placing of each request (`request.cf`: latitude, longitude,
continent, country) and keeps, for a quick-match room of a new game and only for the room's life (ten minutes at
most), the position rounded to whole degrees (about 100 km), the region and the host's choice (column `loc`); of the
one who joins, its region only (`join_region`). Nothing of it is logged, and the other player is told the region
only ("from Europe"). Seven regions: Cloudflare's continents (North America, South America, Europe, Africa, Asia,
Oceania), with the Middle East made of the countries AE, BH, CY, EG, IL, IQ, IR, JO, KW, LB, OM, PS, QA, SA, SY,
TR, YE (Egypt, Turkey and Cyprus with it, as games' regions usually have them; Antarctica belongs to none).

**The choice** (Quick Match's new Region row, left / right, kept as the setting `tbo_region`):

| choice | is put with |
|---|---|
| Auto (the default) | a room within 2,500 km (great-circle, `NEAR_KM`) at once, the nearest first; once the searcher or the room has waited 25 seconds (`WIDEN_SECONDS`), any room, the nearest first (rooms of older games, whose place is not known, last). The search screen then says "NO ONE NEARBY: SEARCHING EVERYWHERE" |
| a region | players placed in that region (by Cloudflare, not by their own choice) at once, the nearest first; anyone once 40 seconds (`REGION_WIDEN_SECONDS`) have gone by. The screens say "Players there first, everyone after 40 s", and after 40 s "NO ONE THERE: SEARCHING EVERYWHERE" |
| Anywhere | anyone: whoever waits longest, as before |

(Changed the same day, the user's decision: a region was at first a lock, never widened; it is now a preference.)
A pair is made only when each side takes the other, the wait being the longer of the two sides' (a room's: its
age): Auto takes near players at once and anyone from 25 s; a region players placed there at once and anyone from
40 s; Anywhere and older games anyone. So Auto in the Middle East and a player who chose South America meet at 40 s,
not at 25 (Auto would take them, South America would not yet).

2,500 km keeps the round trip near 60 ms or under (Frankfurt-Madrid 1,400 km, Dubai-Mumbai 1,900 km, New
York-Chicago 1,150 km); 25 seconds is short enough that a search does not stall for long when nobody near is there.

**Two who wait far apart still meet.** Before, a host never searched again: it only polled. A new game that waits in
its room now asks `/v1/quick` again every ten seconds (`again` in the service's answer), naming its room (code and
key) and how long it has searched (`waited`). The service holds the room while it looks (`join_key` set to "-" for
that moment: nobody joins it meanwhile), then either puts it with another room (its own is deleted) or lets it go
and says to wait on. A room someone joined just before is never lost: the answer is then `joined`, and the poll that
follows has the other player. These searches from one's own room are not counted against the 60 searches an hour.
A database without the two new columns still works (rooms are made without a place and matched by the widening).

**Shown to the player.** The line of the match's start says `Your rival "Riyadh" from the Middle East is here! The
match begins.` and, once the two are connected, `Round trip: ~2 ms` under it (the smaller of the host's setup pings
and the round trip of the input packets; it stays a moment over the character select). New lines in the port
catalog: `net/rival_from`, `net/round_trip`, `region/NA` ... `region/OC`; the screens' lines are port_tbo.c's (listed by
tbo_strings.py; the table of what is sent, `sTbRegionArea`, is left out).

**Older games** (0.1.38.8 and before) send no `waited`: they are answered exactly as before, whoever waits longest
(of the rooms that take them: a new Auto room far away after 25 s, a region's room outside it after 40 s), never
search again and are told nothing new; their rooms are taken by new games as rooms whose place is not known (Auto:
after 25 s; a region: after 40 s; Anywhere: at once). Testing: `BT3_QUICK_AREA=auto|any|NA|SA|EU|ME|AF|AS|OC` with `BT3_LOBBY=quick:<queue>`.

Tested (this machine, offscreen, a local stand-in of the Worker over SQLite with the schema, its places faked per
player name):

| case | result |
|---|---|
| `node port/matchmaking/test.mjs`: 56 new checks (near first, far only after the widening, the nearest of the far ones, two far waiters meeting by searching again, a search again after being joined, a room held while its host looks, regions / Anywhere / Auto and their mixes, older games with each, the Middle East by country, no place in any answer, a database without the columns) | all 180 pass |
| Dubai + Riyadh (870 km), through the screens | matched at once; the line "Your rival "Riyadh" from the Middle East is here! ... Round trip: ~2 ms" |
| Dubai + Frankfurt (4,800 km) | both wait; "NO ONE NEARBY: SEARCHING EVERYWHERE" at 0:25; matched by Dubai's search at 30 s |
| Paris choosing Europe while Dubai and Riyadh search | left alone; its screen "ONLY PLAYERS IN EUROPE / THE WAIT CAN BE LONGER" |
| 0.1.38.8 waiting, this build searching | taken by this build's search at 27 s (the room's wait past 25 s) |
| this build waiting, 0.1.38.8 searching | joined at once |

Both matches of the last two reached the character select together. The Windows build compiles (mingw-w64).
Not checked: the Worker on Cloudflare with its real `request.cf` and D1 (the SQL ran on SQLite here), two players on
two networks.

**The region as a preference** (the same day): `node port/matchmaking/test.mjs` passes, 185 checks (the region's,
Anywhere's and the older games' rewritten for the new rule; new: 30 / 40 s for a region against Auto both ways, a
region chosen from elsewhere taking an Auto room there at 25 s, two different regions at 40 s). Through the screens
with the local Worker: Dubai on Auto and São Paulo choosing South America both waited, São Paulo's screen saying
"PLAYERS IN SOUTH AMERICA FIRST, EVERYONE AFTER 40 S", and were put together by Dubai's search at 40 s; both reached
the character select. No change to the database.
