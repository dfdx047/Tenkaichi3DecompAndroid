#!/bin/sh
# Sets up what port/tools/android.py needs, in port/build/android-toolchain (or $BT3_TOOLCHAIN):
#   llvm/            upstream LLVM 21 (clang, llc, lld, libclang) from the LLVM project's release on GitHub
#   clang-bindings/  libclang's Python module, for port/tools/ptr32.py (from the same release's clang sources)
#   ndk/             the NDK's arm64 sysroot, clang runtime and glslc: from $ANDROID_NDK_HOME when it is set (CI),
#                    else this repository's "toolchain-ndk" release (.github/workflows/toolchain.yml)
#   sdl3/            SDL3 for Android arm64 (headers, libSDL3.so, its Java side), from the same release
# Needs curl, xz, tar. About 2 GB is downloaded once for LLVM (only the needed files are kept).
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TC=${BT3_TOOLCHAIN:-$ROOT/port/build/android-toolchain}
LLVM_VERSION=${BT3_LLVM_VERSION:-21.1.0}
REPO=${BT3_REPO:-dfdx047/Tenkaichi3DecompAndroid}
mkdir -p "$TC"
cd "$TC"

if [ ! -x llvm/bin/clang ]; then
    echo "== LLVM $LLVM_VERSION"
    curl -fsSL "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/LLVM-$LLVM_VERSION-Linux-X64.tar.xz" \
        | xz -dc | tar -x --wildcards "*/bin/clang-*" "*/bin/clang" "*/bin/clang++" "*/bin/lld" "*/bin/ld.lld" "*/bin/llc" \
            "*/bin/llvm-nm" "*/bin/llvm-objdump" "*/bin/llvm-ar" "*/lib/clang/*/include/*" "*/lib/libclang.so*" "*/lib/libclang-cpp.so*"
    rm -rf llvm && mv "LLVM-$LLVM_VERSION-Linux-X64" llvm
fi
if [ ! -f clang-bindings/clang/cindex.py ]; then
    echo "== libclang's Python module"
    curl -fsSL "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/clang-$LLVM_VERSION.src.tar.xz" \
        | xz -dc | tar -x --wildcards "*/bindings/python/clang/*"
    rm -rf clang-bindings && mv "clang-$LLVM_VERSION.src/bindings/python" clang-bindings && rm -rf "clang-$LLVM_VERSION.src"
fi
if [ ! -d ndk/sysroot ]; then
    if [ -n "$ANDROID_NDK_HOME" ] && [ -d "$ANDROID_NDK_HOME/toolchains" ]; then
        echo "== NDK from $ANDROID_NDK_HOME"
        NTC="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"
        mkdir -p ndk/clang_rt ndk/bin
        ln -sfn "$NTC/sysroot" ndk/sysroot
        find "$NTC/lib/clang" -name "libclang_rt.builtins-aarch64-android.a" -exec cp {} ndk/clang_rt/ \;
        find "$NTC/lib/clang" -path "*aarch64*" -name "libunwind.a" -exec cp {} ndk/clang_rt/ \;
        ln -sfn "$ANDROID_NDK_HOME/shader-tools/linux-x86_64/glslc" ndk/bin/glslc
    else
        echo "== NDK sysroot from $REPO"
        curl -fsSL "https://github.com/$REPO/releases/download/toolchain-ndk/ndk-arm64-sysroot.tar.xz" | xz -dc | tar -x
    fi
fi
if [ ! -f sdl3/lib/libSDL3.so ]; then
    echo "== SDL3 from $REPO"
    curl -fsSL "https://github.com/$REPO/releases/download/toolchain-ndk/sdl3-android-arm64.tar.xz" | xz -dc | tar -x
fi
echo "toolchain ready in $TC"
"$TC/llvm/bin/clang" --version | head -1
