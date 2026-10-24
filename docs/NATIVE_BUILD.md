# Building Native Tag Team

Use a separate build folder or checkout when working on a different target.
Windows and Linux builds share some generated files and should run sequentially.
The modded modes are intended for the 64-bit targets.

## Linux

The upstream installer can prepare a source build from your own unmodified USA
BT3 disc. It creates the executable data tables locally and builds the game:

```sh
python3 install.py --check
python3 install.py --iso "path/to/BT3-USA.iso"
```

The current development toolchain uses Clang/LLVM 22, matching Python Clang
bindings, SDL 3.4.12, shaderc and SPIRV-Cross. The upstream installer reports
missing dependencies. Its disc extraction step expects the USA executable;
use `port/tools/import_native_disc.py` separately for regional or BT4 resources.

For later source changes, with the same environment and generated data:

```sh
export BT3_CC=clang64
.venv/bin/python port/tools/undefined.py
.venv/bin/python port/tools/link.py
```

Stop if compilation reports `FAILED`, even if the compiler driver exits zero.
The link must report `link OK`. Run from the repository root:

```sh
BT3_TAG_TEAM=1 BT3_GS=gpu BT3_UPDATE_CHECK=0 \
BT3_DATA=gamedata BT3_SAVES=port/build/native-saves \
BT3_SETTINGS=port/build/native-settings.txt ./port/build/bt3_64
```

Change `BT3_DATA` to the appropriate imported folder. Use separate saves and
settings for BT3 and BT4.

## Windows from Linux or WSL

Prepare the local executable tables as above. Install MinGW-w64 and SDL3's
MinGW development package, then build with:

```sh
export BT3_CC=win64
export BT3_SDL3=/path/to/SDL3/x86_64-w64-mingw32
.venv/bin/python port/tools/undefined.py
.venv/bin/python port/tools/link.py
cp "$BT3_SDL3/bin/SDL3.dll" port/build/
```

Keep `bt3.exe` and `bt3.exe.mem` together. Open the matching `Play native*.cmd`
launcher from Windows after importing that disc's resources. Each launcher keeps
its own saves and settings under `port/build/`.

The inherited upstream release-container scripts build blank executable tables
for the upstream USA installer. They are not a regional Tag Team installer.
The workflow is kept in `.github/workflow-examples/release.yml` as a disabled
example. This repository does not publish those archives automatically.

## Checks

Run these on Windows with the native executable and your own imported game data:

```sh
python port/tests/native_regions_import.py
python port/tests/tag_team_runtime.py
python port/tests/bt4_runtime.py --case mechanics
python port/tests/bt4_runtime.py --case fusions
python port/tests/tag_ffa_runtime.py --case combat
python port/tests/tag_online_runtime.py --players 10 --spectators 10 --lag --input --frames 1800
```

The online runner starts separate local processes. It does not contact a relay
or matchmaking service. Results and temporary profiles stay under `port/build/`.
