# Building and testing on Windows

This project is in early development. The normal host executable builds without game data and prints a startup message. The separate developer probe executes recompiled BT2 startup code using your game files and locally prepared analysis/capture inputs. A playable game build is not available yet.

## 1. Install the build tools

- Windows 10 or 11, x64.
- Visual Studio 2022 with **Desktop development with C++**, MSVC and a Windows SDK.
- CMake 3.21 or newer, Git and Python 3.10 or newer, available in your terminal.

Open a Developer PowerShell for Visual Studio 2022. Clone the repository into a location you choose:

```powershell
git clone https://github.com/oric212/BT2-Recomp.git
cd BT2-Recomp
```

If you already cloned it, open a terminal in that checkout. All relative paths below start at the repository root; none refer to the maintainer's computer.

## 2. Build the foundation host

No game files, BIOS, Ghidra or PS2Recomp installation are needed for this step.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\Debug\bt2-port.exe
```

Expected output:

```text
BT2 Native Recompilation Project
No game data loaded.
```

This confirms the Windows build works. It does not load or boot BT2. Build output is stored in the ignored `build/` directory.

## 3. Prepare your own game files before recompilation

The supported initial target is **Dragon Ball Z: Budokai Tenkaichi 2, North American PS2 release, SLUS-21441**. Other regions or modified executables are not validated targets.

1. Create a disc image from your own legally obtained copy. The project does not supply or download game files.
2. Open that image with an ISO-capable archive tool and extract the original **`SLUS_214.41`** executable and **`SYSTEM.CNF`** from the disc root. Preserve their contents and names. The executable is a separate file; do not pass the whole ISO to the ELF tools.
3. For the examples below, create a `rom` directory inside your checkout and place those two files there. Keep your ISO there too, named `BT2-USA.iso` for these examples. These are local, Git-ignored input locations, not folders supplied by GitHub. If you use another ISO name/location, pass its actual path to commands.
4. Preserve the original disc's `IRX` directory and assets for later developer work. The advanced IOP tools need original modules extracted from this same disc; this first validation step does not require unpacking every asset.

Example local layout after you supply the files:

```text
BT2-Recomp/
  rom/                 # created locally; ignored by Git
    BT2-USA.iso        # your disc image
    SLUS_214.41        # extracted original executable
    SYSTEM.CNF         # extracted boot configuration
```

Validate the files before analysis or generation:

```powershell
python tools/verify_game/verify_game.py rom/SLUS_214.41 --system-cnf rom/SYSTEM.CNF
python tools/verify_game/verify_disc.py rom/BT2-USA.iso --executable rom/SLUS_214.41
python tools/inspect_elf/inspect_elf.py rom/SLUS_214.41
```

The first command checks the executable header and boot reference. The second checks the disc against the recorded trusted revision and verifies that your extracted executable matches the one in that image; hashing the full disc can take time. The third reports ELF metadata. These commands validate/read existing files; they do not extract the image, modify the game, recompile it or boot it. If validation fails, resolve the mismatch before proceeding.

Never commit your ISO, executable, BIOS, extracted assets, captures or proprietary-derived generated code. See [content and license rules](../project/LEGAL.md).

## 4. Prepare the developer toolchain

This is a development workflow, not a player installation wizard. See [the tooling guide](TOOLS.md) for the complete commands and limitations.

- Build the pinned PS2Recomp tools with `python tools/ps2recomp/build_tools.py`.
- Check out the pinned xxHash with `python tools/graphics/textures/fetch_xxhash.py` (the runtime's texture names need it; the probe build stops with that instruction when it is missing) and, for the GPU renderer, paraLLEl-GS with `python tools/graphics/gpu/fetch_parallel_gs.py` (it applies the project's patches).
- Install the Ghidra version recorded in `config/ghidra.lock.json`, its matching R5900 extension and a compatible JDK. Pass your own installation locations to `tools/ghidra/analyze_game.py` using `--ghidra-home` and `--java-home`.
- Analyze your extracted executable, export provisional candidates and prepare the function map. Generated code and analysis output remain local and ignored.
- Run `python tools/ps2recomp/run_pipeline.py` for the baseline generation/build/probe workflow. `--reuse-generated` reuses output only after provenance checks.
- Run `python tools/ps2recomp/generate_interpreter.py` once after generation (the discovery loop reruns it with `--reuse` after map changes): it makes the EE interpreter's handlers in ignored `game/generated/ee-interpreter/`, and the next CMake configure builds them in. Without them, code the map does not hold stops the run instead of running interpreted.
- Advanced startup probes also require original IOP modules and validated local capture bundles. Preparing these uses your own PCSX2 installation, locally supplied BIOS and game image. Follow the capture/checkpoint sections in the tooling guide.

The current deeper startup checkpoint is not reproducible from a fresh clone with one ISO command. Its complete input chain must be prepared locally. Missing inputs must be resolved; capture bundles are not downloaded or fabricated.

## 5. Run the prepared startup checkpoint

Once your generated build and required capture bundles exist, and your checkout includes the startup runner:

```powershell
python tools/build/msvc.py cmake --build build/probe --config Debug --target bt2-entry-probe
python tools/ps2recomp/run_startup_probe.py --disc rom/BT2-USA.iso
```

The probe build directory uses **Ninja** (`run_pipeline.py` configures it with `-G "Ninja Multi-Config"`). `tools/build/msvc.py` runs a command inside the MSVC x64 environment, so a Developer PowerShell is not needed; it uses the Visual Studio installation that bundles Ninja (override with `BT2_VS_ROOT`). A `build/probe` folder made earlier with a Visual Studio generator must be deleted first.

The probe executes generated EE and translated IOP code, performs reads against your ISO and reports the execution boundary, distinct entries, call/return events and sound/heap counters. It stops at an unsupported or checked runtime boundary; reaching that boundary is not a full game boot.

**Build times.** Measured on a 10-core / 20-thread machine on 2026-10-13, with all 10,650 mapped functions compiled in (about 10,600 generated files):
- **Full Release build of the entry probe:** about 6 minutes (Debug: about 8 minutes). Needed after a change to a header generated code includes (`ps2_runtime.h`, `ps2_runtime_macros.h`, `ps2_fpu.h`).
- **Edit to the runtime header** (`ee_runtime.h`): about 16 s. Generated code does not include it.
- **Edit to one runtime source or one generated file:** about 12 s.
- **Nothing changed:** about 5 s.

Before that date the build used the Visual Studio generator and one runtime header for everything: a runtime header edit was a full rebuild of about 14 minutes and a one-file change about 1 minute. See [the runtime layout](RUNTIME_LAYOUT.md) for the header split.

**Optimized probe.** For long runs and selection loops, build and run the Release configuration, which is about 7× faster (16 s instead of 112 s for the current checkpoint). Its report and RAM/GS dumps are identical to Debug:

```powershell
cmake --build build/probe --config Release --target bt2-entry-probe
python tools/ps2recomp/run_startup_probe.py --config Release --disc rom/BT2-USA.iso
```

The verification gate uses Debug:

```powershell
python tools/gate/run_gate.py --disc rom/BT2-USA.iso
```

It builds Debug, then runs CTest, the Python suites and the Debug probe runs concurrently, and finally the original comparisons (about 80 s when the build is up to date). Debug compiles the generated code and the runtime's hot sources optimized; the rest of the runtime, the probe and the tests keep the full Debug settings.

**Controller input.** Runs press the controller buttons from a deterministic script. By default the script answers the no-memory-card prompt with "Yes" (`--pad-script`, steps `FIELD:buttons` in logical IOP fields; an empty value disables input). Runs stop after 1,000 display flips (`--frames` overrides).

The private report is written to `work/bt2-generation/entry-stop.json`. RAM sidecars and generated outputs also stay ignored. A completed entry means at least one invocation returned normally; it does not establish full function correctness or gameplay. See [current progress](../PROG.md) and [the active-pass handoff](../progress/ACTIVE_PASS.md) for the last verified checkpoint and pending work.
### Diagnostic sound-worker path

After preparing the developer checkpoint above, run:

```powershell
python tools/ps2recomp/run_startup_probe.py --disc "PATH_TO_YOUR_BT2_USA_ISO" --sound-worker
```

This executes the generated sound worker through registration/first wait and the translated IOP reverse-bind continuation through its first sleep. It writes a separate ignored `sound-worker-stop.json` report. Main's priority request remains pending; the diagnostic EE stack is cancelled at the boundary. It does not establish correct startup scheduling, callback completion, audio playback or a presented frame. The normal command keeps its own report and measurement.

## Startup helper scheduling

Normal startup now admits main's ChangeThreadPriority(1, 24) and runs the ready startup helpers before main resumes; the former `--startup-workers` diagnostic flag was retired. Compare that scope with the original helper captures after a normal run:

```powershell
python tools/capture_ee/verify_startup_workers.py --native work/bt2-generation/entry-stop.json
```

Owned helper wakes and logical VBlank delivery now drive the first archive-table load. Physical timing, IOP stream acknowledgement and full startup remain unverified; see the current progress page for the measured boundary.
