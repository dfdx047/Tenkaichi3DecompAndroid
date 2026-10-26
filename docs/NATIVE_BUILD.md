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

Keep `bt3.exe` and `bt3.exe.mem` together. Open `Play.cmd` from Windows and choose
one of your imported discs. BT3 and BT4 keep separate saves and settings under
`port/build/`.

The inherited upstream release-container scripts build blank executable tables
for the upstream USA installer. They are not a regional Tag Team installer.

## Native installers

Strip a freshly linked Windows build before packaging it:

```sh
BT3_CC=win64 .venv/bin/python port/tools/strip_data.py --data gamedata --out port/build/installer-portable
```

On Windows, use an isolated Python environment with `pyinstaller` and `pycdlib`,
then run `python port/tools/package_native_setup.py --platform windows`. This
produces `port/build/Tag-Team-Recomp-Beta.2-Windows.zip`. It contains Setup.exe,
a readme and one payload folder, with no extracted game assets or saves.

For Linux, build in Ubuntu 22.04 or an equivalent glibc 2.35 environment. Compile
SDL 3.4.12 there too; copying a library from a newer distribution can raise the
minimum system requirement. Keep the CPU target at generic x86-64. Then:

```sh
BT3_CC=clang64 python3 port/tools/strip_data.py --data gamedata --out port/build/installer-portable-linux
cp port/build/bt3_64.mem port/build/installer-portable-linux/bt3.mem
python3 port/tools/package_native_setup.py --platform linux \
  --portable port/build/installer-portable-linux \
  --sdl /usr/local/lib/libSDL3.so.0 --output port/build/native-setup-linux
```

Freeze each installer on its own operating system, using a Python environment
with PyInstaller, pycdlib and Tk installed. Packaging requires an empty output
folder. The Linux archive is `Tag-Team-Recomp-Beta.2-Linux-x86_64.tar.gz`; its
Setup.sh opens the bundled installer. It installs Play.sh, the game, SDL and the
chosen disc's data. Neither installer needs a system Python installation.

`native_table_recipes.json` reconstructs the stripped startup tables from each
supported disc. Regenerate it with `build_native_table_recipes.py` when a build
needs new ranges; pass the stripped Windows and Linux manifests and privately
owned reference discs. Setup checks every reconstructed range and the build's
combined checksum before installing. Never package a build that reports
unmatched original data in `strip_data.py`.
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
