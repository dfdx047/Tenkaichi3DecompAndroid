#!/bin/sh
# Runs INSIDE the release container (port/release/Dockerfile), in the repository's top folder: builds both release
# archives from the repository alone. No disc and no game data are involved: the game's data tables are built blank
# (port/data) and the finished program fetches their values from the user's disc at start.
#   -> port/build/Tenkaichi3Decomp-linux-x64.zip, port/build/Tenkaichi3Decomp-windows-x64.zip
# port/release/build.sh starts this in the container; the GitHub workflow does the same.
set -e
export BT3_SKELETON=1
run() { # the build of one variant; a compile failure must stop the release (undefined.py only reports it)
    python3 port/tools/undefined.py > port/build/compile_$BT3_CC.txt 2>&1 || { tail -5 port/build/compile_$BT3_CC.txt; exit 1; }
    if grep -q "^FAILED" port/build/compile_$BT3_CC.txt; then grep "^FAILED" port/build/compile_$BT3_CC.txt | head -20; exit 1; fi
    python3 port/tools/link.py > port/build/link_$BT3_CC.txt 2>&1 || { cat port/build/link_$BT3_CC.txt; exit 1; }
    cat port/build/link_$BT3_CC.txt
}
mkdir -p port/build
# the build key (port/tools/buildkey.py reads it from the environment; it is never printed)
if [ -n "$BT3_BUILD_KEY" ]; then echo "build key: given"; else echo "build key: none (the programs do not sign their matchmaking requests)"; fi
# the full release number (undefined.py writes it into the program for the automatic update): the tag's, else "dev"
echo "version: ${BT3_VERSION:-dev}"
echo "== Linux"
export BT3_CC=clang64
run
sh port/setup/build.sh
python3 port/tools/package.py > port/build/package_linux.txt 2>&1 || { cat port/build/package_linux.txt; exit 1; }
cat port/build/package_linux.txt
grep -q "^portable: yes" port/build/package_linux.txt
echo "== Windows"
export BT3_CC=win64
run
sh port/setup/build.sh win
python3 port/tools/package.py
ls -la port/build/*.zip
