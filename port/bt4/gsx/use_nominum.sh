#!/bin/bash
# port/bt4/gsx/use_nominum.sh <libbt4gsx.so of Nominum Render> | --back
# Puts Nominum Render's libbt4gsx.so (its tools/build_android.sh: ARMSX2's GS from this tree's port/build/bt4/ARMSX2,
# the same revision and patch, plus Nominum's own backend) in the APK in place of Dragon Rage's, and marks it in the
# assets (bt4gsx/nominum.txt) so the launcher offers the renderer "Nominum" (bt4_renderer 2). The resources beside it
# (shaders, fonts) are ARMSX2's of the same revision: they stay. --back: Dragon Rage's library again (kept in
# port/build/bt4/libbt4gsx.dr.so; build_bt4.py --armsx2 also puts it back and clears the mark).
set -euo pipefail
cd "$(dirname "$0")/../../.."
J=android/app/src/main/jniLibs/arm64-v8a; A=android/app/src/main/assets/bt4gsx; K=port/build/bt4/libbt4gsx.dr.so
if [ "${1:-}" = "--back" ]; then
    cp "$K" $J/libbt4gsx.so; rm -f $A/nominum.txt
else
    src=${1:?usage: use_nominum.sh <libbt4gsx.so> | --back}
    port/build/android-toolchain/llvm/bin/llvm-nm -D --defined-only "$src" | grep -q " T nominum_create" || { echo "$src is not Nominum Render's"; exit 1; }
    [ -f $A/nominum.txt ] || cp $J/libbt4gsx.so "$K"
    cp "$src" $J/libbt4gsx.so
    echo "$(sha256sum "$src" | cut -c1-16) $(date +%F)" > $A/nominum.txt
fi
# a new version.txt: Bt4Activity copies the resources out again (and the cache made by the other library goes)
read -r rev _ files < $A/version.txt
echo "$rev $(sha256sum $J/libbt4gsx.so | cut -c1-16) $files" > $A/version.txt
echo "libbt4gsx.so: $( [ -f $A/nominum.txt ] && echo "Nominum Render's" || echo "Dragon Rage's")"
