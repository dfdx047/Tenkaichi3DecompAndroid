# macOS port: plan

The macOS port is EXPERIMENTAL: built from source on the Mac, no release archive.

Status 2026-10-12: native arm64 build (`BT3_CC=mac64`) links and runs the attract-demo fight headless to the end
("battle finished"), and its fight state (`BT3_HASH`: health, positions, clock, random generator, and their
checksum) equals the Linux clang64 build's on all 4428 vertical blanks (the whole-memory checksum differs, as between
any two builds: it holds addresses). It draws through SDL_GPU's Metal back end, with sound (CoreAudio). Rollback works: `BT3_SYNCTEST=1` (depth 1, and
depth 8 with the roll ring, `BT3_SYNCTEST_ROLL=1`) runs the whole demo fight with 0 differences; per blank about
1 ms of game, 0.45 ms to save, 0.3 ms to restore. This file is the
plan and the record of what was checked; update it as phases land.

## Ground rule

Players on Linux and Windows see no change. Every macOS change sits behind `__APPLE__` or the `mac64` variant, the
Linux and Windows builds keep their toolchain (stock LLVM; a patched LLVM, if one is needed, is for macOS only),
and the Linux demo-fight state hashes are compared before and after each change. The macOS build must give the
same hashes, so that replays and online matches against Linux and Windows work.

Decision 2026-10-12: aim for native arm64 directly, starting with the memory-model spike (phase 2 below); the
x86-64/Rosetta route (phase 1) is the fallback, and the platform work is shared by both.

## Why it is not a recompile

The 64-bit build keeps every pointer of the game's code 4 bytes wide (`port/tools/ptr32.py`: clang's
`__ptr32 __uptr`, address space 271) and puts the PS2's memory at its original addresses below 4 GB
(`port/src/plat_mem.c`). Both are x86-64 features of the platform, not of the source code.

BT3-Recomp ran on macOS because its memory model is different: PS2 RAM is a buffer (`uint8_t *rdram`) and every
access is `rdram + address`, so the PS2 addresses never have to exist in the host's address space.

## Checked on this Mac (M-series, macOS 26, Apple clang 17, Homebrew LLVM 22.1)

| Check | x86-64 Mach-O (Rosetta 2) | arm64 Mach-O (native) |
|---|---|---|
| `sizeof(int * __ptr32 __uptr)` | 4 | **8** (`__ptr32` ignored: every game struct changes layout) |
| `mmap` MAP_FIXED at 0x003BE000, 0x02000000, 0x10000000, 0x13000000, 0x70000000 | all succeed | **all fail** (4 GB `__PAGEZERO` is mandatory) |
| link with `-no_pie -pagezero_size 0x1000 -image_base 0x20000000` and a `.long _g` (32-bit absolute) in data, run | works, reads the right value | n/a |

So x86-64 under Rosetta keeps the port's memory model unchanged; arm64 needs a new one.

## Phase 1: x86-64 build under Rosetta (the route to a playable game)

Done when `BT3_CC=mac64` builds `port/build/bt3_mac`, the demo fight runs headless with the same state hashes as
Linux (`port/tools/compare_hash.py`), and the game plays in a window with the OpenGL renderer.

1. **Toolchain variant.** Add `mac64` to `port/tools/toolchain.py`: `--target=x86_64-apple-macos11`, Homebrew
   LLVM's `clang`/`llc` (Apple clang has no `llc`), `pip install libclang` for `ptr32.py`. Port C/C++ files
   built with `-arch x86_64`.
2. **SDL3 for x86-64.** Homebrew's is arm64. Build SDL3 from source with `CMAKE_OSX_ARCHITECTURES=x86_64` into
   `port/build/mac/` (same idea as `port/build/win` for mingw).
3. **Assembly and symbol names.** Mach-O prefixes C symbols with `_`, and has no `.section .data`, `.type`,
   `.size`. Teach the generators of `port/data/*.s` (`make_skeleton.py`, `gen_data.py`, `strip_data.py`) and the
   stub generator in `link.py` to emit Mach-O directives and `_` names when the variant is `mac64`.
4. **Linker.** In `link.py`, a `mac64` branch with ld64 equivalents:
   `-Ttext-segment=0x20000000` → `-no_pie -pagezero_size 0x1000 -image_base 0x20000000`;
   `-Map` → `-map`; `-rdynamic` → `-export_dynamic`; `$ORIGIN/lib` → `@executable_path/lib`;
   `--defsym=D_3BE71C=0x3BE71C` → an absolute symbol in a small `.s` (`.globl _D_3BE71C; .set _D_3BE71C, 0x3BE71C`);
   `--wrap=main` and `--wrap=Progress_Main` → ld64 has no `--wrap`: rename the game's definitions at compile
   time (`-Dmain=__real_main` style, only on game objects) and keep the port's `__wrap_*` as the real names.
   The undefined-symbol scan parses GNU ld's message (`undefined reference to`); ld64 prints
   `Undefined symbols ... "_name", referenced from`.
5. **Memory (`plat_mem.c`).** A `__APPLE__` branch beside the Linux one: `MAP_FIXED_NOREPLACE` → check the range
   is free (`mach_vm_region`) then `MAP_FIXED`, or `mach_vm_allocate` with a fixed address; drop `mallopt`
   (glibc) and confirm the system malloc never hands out the low regions (reserve them first thing in start-up).
   Audit every `#if defined(__x86_64__) && !defined(_WIN32)`: today it means "Linux".
6. **The rest of the platform layer.** Audit `plat_crash.c` (signal context registers differ: `uc_mcontext->__ss`),
   `plat_sys.c`, `plat_file.c` (case-insensitive file systems, paths next to the executable),
   `plat_fastvec.c` / `state.c` / `headless.c` (Rosetta has SSE4.2 and, from macOS 15, AVX2; nothing beyond),
   and the network code (`gs/net*.c`: BSD sockets, the HTTP call to the matchmaking service).
7. **Renderer.** SDL_GPU's Metal backend with MSL shaders (see "Prior work" below); OpenGL (`BT3_GPU_API=gl`,
   macOS caps at 4.1 core) as the fallback while the MSL shaders are not ready.
8. **Install path.** `install.py` stops on non-Linux (`sys.platform != "linux"`): allow `darwin` and skip the
   steps that do not apply. Then `port/setup/build.sh` for the setup window.
9. **Package.** An `.app` bundle with SDL3 in `Contents/Frameworks`, ad-hoc signed (`codesign -s -`).

Risks: speed (softfloat for every game float, translated by Rosetta; measure the demo fight early, step 5 is
enough to run headless), and Rosetta's lifetime (Apple has announced it is reduced after macOS 27 to a subset
kept for games; check what that subset allows before investing in packaging).

## arm64 spike results (2026-10-12, scratch programs, not yet the game)

- **Layout: solved by Homebrew LLVM 22.** Its arm64 datalayout has `p270:32:32-p271:32:32`, so
  `sizeof(int * __ptr32 __uptr) == 4` and game structs keep their PS2 layout; `llc` selects loads and stores
  through address space 271 (as a zero extension, i.e. still an absolute low address). Apple clang 17 ignores
  `__ptr32` on arm64 (pointer 8 bytes): the arm64 build must use Homebrew LLVM. The static-initializer error
  (`addrspacecast` in a global) is the one `irfix.py` already handles.
- **Window: works.** arm64 is always PIE (`-image_base` ignored), but `-segaddr` is honoured for every segment
  and slid as a whole. With `__TEXT` at 0x320000000 (`__DATA_CONST`, `__DATA` right after it) the window
  `W = &__mh_execute_header - 0x20000000` is free: W+0x003BC000, +0x02000000, +0x10000000, +0x12000000,
  +0x13000000, +0x70000000 all map with `MAP_FIXED`, 50 of 50 runs (the same at 0x620000000). With `__TEXT`
  at 0x120000000 it fails every time: malloc's first zones sit just above `__PAGEZERO`, and the dyld shared
  cache sits at 0x18xxxxxxx.
- **Static 4-byte pointers: work.** `.long _g - __mh_execute_header + 0x20000000` is resolved by ld64 and
  stays right under any slide; the game's globals keep offsets from 0x20000000 as on Linux.
- **16 KB pages.** Apple Silicon pages are 16 KB: `MAP_FIXED` at 0x003BE000 is refused (EINVAL). Map from
  0x003BC000 (16 KB-aligned) and keep `HEAP_FIRST` 0x003BE730 as the first address handed to the game.

- **LLVM 22 bug: narrow stores through address space 271 on AArch64 are 32-bit.** `p->charField = v` through
  a `__ptr32` pointer compiles to `str w1, [x8]` and overwrites the next three bytes (reproduced here;
  llvm/llvm-project#228782, fixed 2026-10-07 by commit ac2f555, in no release yet). The rewrite removes every
  271 memory operation before instruction selection, which avoids it; the rewrite must fail the build if any
  load, store or memory intrinsic on `addrspace(271)` is left.
- **Window placement vs the shared cache.** XNU's arm64 shared region is 0x180000000 + 0x180000000, so it can
  reach 0x300000000; `__TEXT` at 0x320000000 puts W just above it. 0x620000000 also passed and leaves margin.
  (Note: a research pass claimed `-segaddr` is ignored for PIE; here it is honoured, the image loaded at
  0x32xxxxxxx and 0x62xxxxxxx. Only `-image_base` is ignored.)

What remains for the memory model is the rewrite below (step 2): turning `addrspacecast` 271 → 0 into
`W + zext(offset)` and 0 → 271 into `p - W`. W is a link-time constant relative to the code (`adrp` of
`__mh_execute_header`), so it costs no memory load.

## Prior work found (research 2026-10-12)

- No existing LLVM pass lowers a 32-bit address space to base + offset; ours is new. Closest models: LLVM LFI
  (https://llvm.org/docs/LFI.html: base in a register, accesses as `[xBase, wOff, uxtw]`), V8 pointer
  compression, N64Recomp / XenonRecomp (base + 32-bit guest address in C). Every console port that keeps the
  original layouts on arm64 uses base + offset; none uses fixed low addresses.
- Apple DTS (https://developer.apple.com/forums/thread/845976, new and undocumented): the entitlement
  `com.apple.developer.cross-architecture-support` allows mapping below 4 GB. Needs a provisioning profile, so
  it does not fit users building from source; noted, not relied on. Without it, `-pagezero_size 0x4000` is
  killed before `main`.
- Rosetta 2 is general-purpose only through macOS 27; from macOS 28 a reduced subset for old games. Phase 1
  is a fallback for testing, not a release target.
- Linker: ld64 / ld-prime have no `--wrap`: `llvm-objcopy --redefine-sym` works on Mach-O objects, or rename in
  the IR rewrite. `--defsym` to an absolute value: `.set` in a small `.s` (`-alias` only aliases a symbol).
- Renderer: the game draws through SDL_GPU (`port/src/gs/gs_gpu.c`, SPIR-V shaders), which has a native Metal
  backend on macOS. No MoltenVK needed: compile the shaders in `port/src/gs/shaders/` to MSL as well
  (SDL_shadercross / spirv-cross) and request `SDL_GPU_SHADERFORMAT_MSL` on Apple. Uses only triangle, line and
  point lists and D32_FLOAT depth, all available on Metal. The setup window already accepts MSL.

## Native arm64: how the build works (as built)

`python3 install.py --iso <image>` does it all on a Mac (about 2 minutes on an M-series): checks the Homebrew tools,
reads the disc with the system's `bsdtar`, puts `libclang` 18 (for `ptr32.py`) in `.venv`, builds with `BT3_CC=mac64`
(`port/tools/undefined.py`, `link.py` -> `port/build/bt3_mac`), runs the demo fight, writes `play.sh`.

- **Game code**: preprocess + `eeconst.py` + `ptr32.py` as on Linux, then Homebrew clang `-emit-llvm` with
  `-Xclang -disable-llvm-passes`, the window pass (`port/tools/macwin/macwin.cpp`, an `opt` plugin built on first
  use by `macwin.py`), `default<O2>`, then `llc` with `-mattr=-fp-armv8,-neon` (software float: the game's floats
  are `__addsf3`-style calls, floats in integer registers, as `-msoft-float` on x86). The pass runs BEFORE the
  optimizer so that it sees `(u32)p` as written (ptrtoint + trunc), not folded into shifts and masks.
- **Two steps over the whole program** (`undefined.py`: `mac_pending`, then `mac_finish`): first every module's IR
  and its list of definitions (`-bt3-defs`), merged into `port/build/mac_sigs.txt`; then the pass with that list
  (`-bt3-sigs`). The game's sources often declare a function with `s32` where its definition has a pointer (or the
  reverse; about a hundred such pairs), or as variadic when it is not; each such call is converted to the
  definition's terms.
- **Port C files** of the soft-float kind (`src/port/vu0_*.c`, `port/src/*.c`) take the same path with `-bt3-port`
  (no game-address rules); the renderer and the C++ (`port/src/gs`, ImGui) are ordinary Apple clang arm64.
- **Data tables** (`.s`): `port/tools/macho.py` (Mach-O names, `.long _g - __mh_execute_header + 0x20000000`).
- **Link** (`link.py`, `link_mac`): ld64, entry `__wrap_main`, every segment moved with `-segaddr` so that
  `__TEXT` starts at 1 TB + 0x20000000. `make_state.py` and `make_dat.py` read ld64's map (symbols, not input sections; game addresses; a table's
  length from its object, since a symbol's size in the map runs on to the next one); `make_dat.py` signs the marked
  program again (ad hoc). A `BT3_SKELETON=1` build fetches its tables from the disc and gives the same fight state.

Memory model: W = `&__mh_execute_header - 0x20000000` = 1 TB. A game pointer (4 bytes) is an offset from W; the PS2
regions are mapped at W + their address (`plat_mem.c`, Apple branch); the program's own globals are in the window
too, at the same offsets as the Linux addresses. Port code converts with `Port_Ptr` / `Port_Addr`
(`port/src/port_addr.h`; plain casts elsewhere). Pointers cross every call as host pointers (W + offset).

Lessons (each was a crash; keep them):
- **W on a 4 GB boundary.** The game passes integers through `void *` parameters and back (a task's argument is a
  pointer for some tasks, a character number for others: `EftChar_Init(task, s32 chr)` called through a table).
  With W at 1 TB the low 32 bits of W + n are n, so these round-trip as on Linux. ASLR would slide W off the
  boundary: `window_aligned()` in `plat_mem.c` restarts the program in the same process with
  `_POSIX_SPAWN_DISABLE_ASLR` when it is not aligned.
- **Integers for pointers through a function pointer.** The reverse case: `McFlowCb` is `void (*)(s32)`, the title
  screen stores its `s32 *result` as that s32, and `Title_OnCardDone(s32 *)` got the bare offset (crash in the
  memory card's new-save screen). A function whose address is taken takes a pointer parameter whose high 32 bits
  are 0 as a game address (W + it; nothing in the window has them), and an indirect call's pointer result too
  (`fixCalledThroughPointer`, `fixIndirectResult` in `macwin.cpp`).
- **Load W, build pointers with inttoptr.** W as a constant expression of `__mh_execute_header` let alias analysis
  conclude that game memory is that one symbol (stores dropped in `Ot_Init`); a GEP on a W loaded at function
  entry has W's provenance, and LLVM concluded it could not reach a local whose address is taken later (the store
  of `head` in an inlined `StgRigidList_Last` was dropped). `host()` is `inttoptr(W + offset)`.
- **Program placement.** macOS 26's malloc (xzm) reserves zones at 64 GB / 256 GB and sometimes 24 GB: a program
  there crashed in malloc's start-up. 1 TB and 4 TB were 200 of 200.
- **Variadic ABI.** Apple arm64 passes variadic arguments on the stack. `func_002967C0` (the game declares it
  `(s32, s32, ...)`, the port defines fixed parameters) has an Apple variadic definition in `snd_se.c`; calls from
  game code to game definitions are fixed by the signature list.
- **Pages are 16 KB**: regions are mapped from the 16 KB page holding their first address.
- **lldb** needs Developer Mode (`sudo DevToolsSecurity -enable`), otherwise it waits for an authorization prompt.
  `plat_crash.c` prints registers (pc, lr, sp, x0-x28) and the game address of the fault instead.
- **A stale `bt3_mac.dat` crashes at start** (SIGBUS in `Port_MapMemory`, a write into `__TEXT`): it is written
  for a build from the blank data tables (install.py) and lists addresses of that link. A later build with the
  data linked in (`gen_data.py --asm`, no `SKELETON`) does not remove it, and the program still applies it. Delete
  it when changing between the two.
- `BT3_MACWIN_REPORT=1` during the build lists, in `port/build/macwin_report.txt`, every host pointer made into an
  integer outside the window arithmetic (pointer differences are expected there).

State saving on macOS (`state.c`): `_setjmp`/`_longjmp` and the port's stack switch instead of ucontext, 16 KB pages
for the written-page tracking, and SIGBUS as well as SIGSEGV (macOS reports a write to a read-only page as SIGBUS).
The program's path comes from `_NSGetExecutablePath` (there is no /proc).

Still to do: an online match Mac against Linux (the attract demo with `BT3_NET_HOST` crashes on Linux too: not a
test; needs a pad recording, `BT3_PAD_TABLE`, played with `BT3_NET_SCRIPT`); sound, input, netplay checks; `.app` package. Calls from hard-float port code (Apple clang) to game
functions are not covered by the signature list.
