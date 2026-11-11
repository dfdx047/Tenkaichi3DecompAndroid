#!/bin/sh
# Runs INSIDE the release container (port/release/Dockerfile), in the repository's top folder: builds the Windows release
# archive from the repository alone. No disc and no game data are involved: the game's data tables are built blank
# (port/data) and the finished program fetches their values from the user's disc at start.
#   -> port/build/BT4decomp-windows-x64.zip
# port/release/build.sh starts this in the container; the GitHub workflow does the same.
set -e
export BT3_SKELETON=1
run() { # the build of one variant; a compile failure must stop the release (undefined.py only reports it)
    python3 port/tools/undefined.py > port/build/compile_$BT3_CC.txt 2>&1 || { tail -5 port/build/compile_$BT3_CC.txt; exit 1; }
    if grep -q "^FAILED" port/build/compile_$BT3_CC.txt; then grep -A8 "^FAILED" port/build/compile_$BT3_CC.txt | head -80; exit 1; fi
    python3 port/tools/link.py > port/build/link_$BT3_CC.txt 2>&1 || { cat port/build/link_$BT3_CC.txt; exit 1; }
    cat port/build/link_$BT3_CC.txt
}
mkdir -p port/build
python3 port/tests/spectator/run.py
python3 port/tests/mods/run.py
python3 port/tests/custom_content/run.py
# the build key (port/tools/buildkey.py reads it from the environment; it is never printed)
if [ -n "$BT3_BUILD_KEY" ]; then echo "build key: given"; else echo "build key: none (the programs do not sign their matchmaking requests)"; fi
# the full release number (undefined.py writes it into the program for the automatic update): the tag's, else "dev"
echo "version: ${BT3_VERSION:-dev}"
echo "== Windows"
export BT3_CC=win64
clang --version | head -1
llc --version | head -1
run
sh port/setup/build.sh win
# Preserve the exact RTC source bundle and receipt produced in the pinned build image.
mkdir -p port/build
cp /usr/local/share/licenses/BT4-RTC/libdatachannel-build-source.zip port/build/libdatachannel-build-source.zip
cp /usr/local/share/licenses/BT4-RTC/BUILD.json port/build/libdatachannel-BUILD.json
python3 port/tools/prepare_windows_runtime.py
python3 port/tools/package.py
ls -la port/build/*.zip
