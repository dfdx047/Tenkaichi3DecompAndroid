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
- Switches: `BT3_FX_OFF=<mask>` or F1..F4 while running: 1 outline, 2 see-through tint, 4 depth tint, 8 glow
  (glare and object glow: reserved, the glow pass itself is not written yet).
- Still dropped: the glow pass (shrink, blur, add), pan blur, haze, stage blur, water wobble, lens, cross-fade.
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
