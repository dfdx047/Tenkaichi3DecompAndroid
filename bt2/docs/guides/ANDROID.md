# BT2 Recomp on Android (Dragon Rage)

This folder (`bt2/`) is BT2 Recomp inside the Dragon Rage repository (Tenkaichi3DecompAndroid), so one APK
plays Budokai Tenkaichi 2 and Budokai Tenkaichi 3: the player picks the disc image of either game in the app,
and the app tells them apart by their serial (SLUS-21441, SLUS-21678). Copied from BT2-Recomp at commit
`d6c8b51` (2026-10-23); the Windows build is unchanged.

## How the two games share the APK

| | BT3 | BT2 |
| --- | --- | --- |
| Engine | `libbt3.so` (decompilation, `port/tools/android.py`) | `libbt2.so` (recompilation, `bt2/android/build.py`) |
| Started by | `GameActivity`, process `:game`, through `libmain.so` | `Bt2GameActivity`, process `:game_bt2`, SDL calls `libbt2.so` directly |
| Game folder | the app's folder (`gamedata/`, `saves/`, ...) | `<app folder>/bt2` (`data/BT2.iso`, `saves/`, `startup/`, `user/`) |
| Disc | unpacked by the app | copied whole; the runtime reads the image and makes its startup inputs from it |

Each game has its own process: BT3's loader reserves fixed low addresses as soon as it is loaded, and SDL
keeps its state in statics. Both use the app's SDL3, its on-screen controller (`TouchBridge`) and its
"quit to launcher".

## The app: a BT3 | BT2 switch

A switch at the top of Dragon Rage's pages chooses the game. BT3's pages are the app's own, unchanged; BT2's
are in `android/app/.../ui/screens/bt2` with their own view model (`Bt2ViewModel`): home (its disc image,
playing), HD textures, mods (what works: data-only mods of an image with the original program), its own
on-screen controller layout (`bt2/touch_controls.json`), patches (widescreen, 60 fps fights), its own GPU driver
choice (`bt2/user/driver`), its own logs and settings. Nothing set on the BT2 tab reaches BT3: BT2's settings
are `bt2/user/settings` (the engine's file) and `bt2/user/android` (Android-only options such as
`gs_hacks`), its log is `bt2/user/activity.log` with each run's engine log kept in `bt2/user/runs`.
`./gradlew assembleDebug -PsideBySide` builds a test app ("Dragon Rage (teste)") that installs beside the
player's own.

On Android the game thread reports each field's work to the system's performance hints (ADPF, Android 13 and
later) and is kept off the efficiency cores (`runtime/host/sdl/android_performance.h`).

## What changed for Android

- **x86 intrinsics on NEON:** `runtime/platform/sse` stands in for `immintrin.h` and the other x86 headers
  with sse2neon (v1.9.1, MIT, precise variants on). Only the ARM build puts it on the include path; the
  runtime and the generated code are unchanged. PS2Recomp's own Android runner takes the same approach.
  `_mm_setcsr` (the FPU's round toward zero, `ee/ps2_fpu.cpp`) becomes FPCR's rounding mode.
- **Retained EE stacks without fibers:** `ee/retained_ee_stack.cpp` keeps the Win32 fibers on Windows and
  elsewhere maps its own stacks (with a guard page) and switches them with a few instructions (AArch64,
  x86-64). Bionic has no `ucontext`.
- **Host layer on SDL3** (`runtime/host/sdl`, `BT2_SDL`): window (fullscreen, Vulkan), audio stream, gamepad
  and keyboard, the on-screen controller (`touch_pad.h`), the frame clock and the player's session. The
  existing host headers include them when `BT2_SDL` is set; on Windows they are as before.
- **GPU presenter:** `gs/gpu/gpu_window_presenter.cpp` makes its Vulkan surface through SDL
  (`VK_KHR_android_surface` on a phone) and lets its swapchain go when the app goes to the background
  (`WindowPresenter::lost`), making a new one at the next picture.
- **Entry:** `runtime/android/android_main.cpp` is `SDL_main`: it does what `runtime/game/game_main.cpp` does
  before it starts the runtime (folders, settings, startup inputs, the player's environment) and calls the
  entry probe's main, built as `bt2_entry_main`.
- **Startup inputs without captures** (`runtime/game/startup_inputs.*`): the 22 inputs the entry probe
  starts from are made from the player's disc image. 19 are fully set by the runtime's own checks (each
  `supply*` accepts one value only), `guest-memory.bin` is the program's PT_LOAD image as
  `prepare_runtime.py` builds it, and the IOP memory bundles hold only the words the runtime reads. The three
  GS packets the game builds itself are left zero and, with `BT2_SELF_STARTUP`, taken from the game
  (`gs/gif_dma_staging.h`, `gs/texture_gif_startup.h`). The kernel snapshot is zero (no BIOS); in that mode
  each word the game reads from it is logged. `tests/startup_inputs_tests.cpp` passes the made files
  through the runtime's own reading and checks. Without `BT2_SELF_STARTUP` nothing changes.
- **GPUs:** the backend names the GPU and driver in the log and checks paraLLEl-GS's requirements first,
  naming what is missing (Vulkan 1.3 with subgroup size control: Adreno 600 and later, Mali Valhall and
  later; not Mali Bifrost such as G52/G72/G76). A custom driver chosen in the app (Turnip on Adreno) is
  loaded through libadrenotools (BT3's copy, `port/third_party/libadrenotools`) and used for drawing and
  for the surface, which Android's build makes from the native window itself. `BT2_GS_HACKS` turns on
  paraLLEl-GS's speed-for-accuracy options (off by default until measured on phones).
- **Generation on Linux:** the PS2Recomp tools build with Ninja or Make outside Windows (`build_tools.py`,
  one `recompiler()` path for every script), and `tools/ghidra/analyze_game.py` finds Java on Linux.
- **Portability fixes found on the way:** `services/cri_audio.h` (a deduced return type GCC and Clang refuse
  before its definition), `probe/entry_probe.cpp` (the card folder read with Win32 only).
- **Build:** `cmake/bt2_sources.cmake` holds the source lists both builds use (the Windows build sees the same
  180 files as before); `android/CMakeLists.txt` builds `libbt2.so` with the NDK, paraLLEl-GS and SDL3;
  `android/build.py` configures, builds and installs it into the app.

## Building

The game's code must be generated first, with the user's own disc image, by the pipeline of
[BUILDING.md](BUILDING.md) (`tools/ps2recomp`; its generation steps also run on Linux). The startup inputs
are not needed at build time: libbt2.so makes them on the phone. Then, with the Android NDK and BT3's toolchain (`port/tools/android_toolchain.sh`, for
SDL3):

```sh
ANDROID_NDK_HOME=/path/to/ndk python3 bt2/android/build.py      # libbt2.so and the startup inputs into the app
cd android && ./gradlew assembleRelease                          # the APK with both games
```

The Android workflow (`.github/workflows/android.yml`) runs `build.py` only when
`bt2/work/bt2-generation/guest-memory.bin` is there. That never happens in the public repository:
`libbt2.so`, the generated code and the startup inputs are made from the game's program and stay out of it
([LEGAL.md](../project/LEGAL.md); the app's `.gitignore` covers `assets/bt2/` and `jniLibs/`). A public
build therefore has no BT2 engine of its own: the players import it.

## The motor package (the public APKs)

The public APK leaves `libbt2.so` out; the players download a motor package shared outside GitHub (the Dragon
Rage Discord) and import it on the BT2 tab's home ("Import motor"). Made where the game's code was generated:

```sh
python3 bt2/android/build.py key        # once: the signing key, ~/.config/dragonrage/bt2-engine-key.pem
ANDROID_NDK_HOME=/path/to/ndk python3 bt2/android/build.py configure build package
                                        # build/android/bt2-motor-<date>-<commit>.zip
```

The package holds the stripped `libbt2.so`, `manifest.json` (version, ABI, size, SHA-256) and `manifest.sig`,
the manifest's ECDSA P-256 signature. The app (`data/Bt2Engine.kt`) takes it only when the signature matches the
public key it carries (`Bt2Engine.PUBLIC_KEY`, printed by `key`), the library matches the manifest and the ABI is
its own (`BT2_ENGINE_ABI`, `runtime/android/engine_abi.h`; raise it when `SDL_main`'s arguments or the JNI
functions change, so an app update asks for the matching motor instead of starting one that does not fit). It
keeps the engine in its own folder (`files/engines/bt2`) and `Bt2GameActivity` loads it from there. The private
key stays out of every repository; a lost key means a new one and an app update carrying its public half.

## Status

Verified (2026-10-23, Linux x86-64):

- `android/build.py check` with NDK 29: the handwritten runtime, paraLLEl-GS with Granite, and
  libadrenotools compile for Android arm64 (Granite's Android application layer is not built; its Game
  Development Kit packages are satisfied by empty stand-ins in `android/cmake`);
- the whole handwritten runtime compiles for arm64 with GCC 13 and sse2neon;
- BT2's retained stack tests pass on arm64 under QEMU and on x86-64, at -O0 and -O2;
- the startup inputs test passes; BT2's pipeline tests pass with the Linux recompiler path;
- the PS2Recomp tools build on Linux; Dragon Rage's app builds with the BT2 changes.

Not verified yet: generating the game's code on Linux (Ghidra analysis and PS2Recomp with a disc image),
linking libbt2.so with it, the first run (whether the BIOS-less kernel baseline is enough), paraLLEl-GS on
phone GPUs, and the speed of a fight on a phone.

Not there yet:

- the in-game menu (`host/overlay.h` draws with GDI): on Android, back pressed twice returns to the app;
- the frame rate text on the picture (written to the log instead);
- OpenGL ES: paraLLEl-GS is Vulkan only (the CPU renderer was removed upstream in `0b92f0b`);
- fractional resolutions: paraLLEl-GS scales by samples per pixel (1x, 2x; 4x and more draw at twice the
  resolution);
- the sample profiler outside Windows.
