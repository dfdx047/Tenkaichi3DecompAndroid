#!/usr/bin/env python3
"""Builds libbt4.so, the Budokai Tenkaichi 4 engine of Dragon Rage (Team BT4's mod, SLUS-219.78 Beta 14 Rev2), and
puts it next to libbt3.so in the app (android/app/src/main/jniLibs/arm64-v8a). See port/bt4/README.md.

    python3 port/tools/android.py                            (first: the toolchain, SDL3 and the GS shaders)
    python3 port/bt4/build_bt4.py --data <folder>            (the first time: the game's code is generated)
    python3 port/bt4/build_bt4.py                            (later: rebuilds with the code already generated)
    python3 port/bt4/build_bt4.py --armsx2                   (also ARMSX2's renderer for the game: libbt4gsx.so)

<folder> holds the mod's SLUS_219.78 and BIN/DBZP.BIN: the app copies them out of the image to
Android/data/com.dfdx047.dragonrage/files/bt4/runtime/data (copy that folder to the PC), or extract them from the ISO.
Nothing of the game is in this repository: its code is generated here from your copy.

Everything goes to port/build/bt4/ (BT3-Recomp, dbz-bt4-recomp, FFmpeg, the generated code, the build). Needs, on the
PC: git, cmake, ninja, make, a C/C++ compiler (the recompiler runs here) and Python 3.
"""
import argparse
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
import types
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
HERE = ROOT / "port/bt4"
WORK = ROOT / "port/build/bt4"
TC = pathlib.Path(os.environ.get("BT3_TOOLCHAIN", str(ROOT / "port/build/android-toolchain")))
LLVM = pathlib.Path(os.environ.get("BT3_LLVM", str(TC / "llvm")))
SYSROOT = pathlib.Path(os.environ.get("BT3_SYSROOT", str(TC / "ndk/sysroot")))
SDL3 = pathlib.Path(os.environ.get("BT3_SDL3", str(TC / "sdl3")))
GEN = ROOT / "port/build/android/gen/gs"
API = 29

BT3R_URL, BT3R_REV = "https://github.com/z3xox/BT3-Recomp", "e22a34b"
BT4R_URL, BT4R_REV = "https://github.com/recomp-code/dbz-bt4-recomp", "a5771ed"
FFMPEG_URL, FFMPEG_TAG = "https://github.com/FFmpeg/FFmpeg", "n7.1"
ELF_SHA256 = "224525c5639c00e01c2604c4685d307bc739c041000dcf8c69a4f884f465e8a4"  # SLUS_219.78, Beta 14 Rev2

ARMSX2_URL, ARMSX2_REV = "https://github.com/ARMSX2/ARMSX2", "90126bdfea7a3ffb1fd288b2f7f0ae0b033af2d6"
ARMSX2 = WORK / "ARMSX2"
# what the APK puts beside libbt4gsx.so (resources/, cache/): a new value has Bt4Activity copy them out again
GSX_FILES = "files2"
ARMSX2_PATCH = HERE / "patches/armsx2-bt4.patch"
GSX = HERE / "gsx"

BT3R = WORK / "BT3-Recomp"
BT4R = WORK / "dbz-bt4-recomp"
OURS = HERE / "patches/bt3recomp-android.patch"


def run(cmd, cwd=None, env=None):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], cwd=cwd, env=env, check=True)


def sha256(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def clone(url, rev, dst):
    if not (dst / ".git").exists():
        run(["git", "clone", "--quiet", url, dst])
    if subprocess.run(["git", "-C", dst, "cat-file", "-e", rev + "^{commit}"]).returncode != 0:
        run(["git", "-C", dst, "fetch", "--quiet", "origin"])


def sources():
    """BT3-Recomp at its revision with the mod's patch and ours (applied again whenever ours changed)."""
    WORK.mkdir(parents=True, exist_ok=True)
    clone(BT3R_URL, BT3R_REV, BT3R)
    clone(BT4R_URL, BT4R_REV, BT4R)
    run(["git", "-C", BT4R, "checkout", "--quiet", BT4R_REV])
    mark = BT3R / ".dragonrage-patched"
    want = sha256(OURS) + " " + BT3R_REV + " " + BT4R_REV
    if mark.exists() and mark.read_text().strip() == want:
        return
    print("== BT3-Recomp: the mod's patch and Dragon Rage's", flush=True)
    run(["git", "-C", BT3R, "checkout", "--quiet", "--force", BT3R_REV])
    run(["git", "-C", BT3R, "reset", "--quiet", "--hard", BT3R_REV])
    # (clean, but keep the generated game code and the VU1 programs: they come from the ELF, not from git)
    run(["git", "-C", BT3R, "clean", "--quiet", "-fdx", "-e", "ps2xRuntime/src/runner", "-e", "ps2xRuntime/src/runner_overlay",
         "-e", "ps2xRuntime/include/ps2_recompiled_functions.h", "-e", "ps2xRuntime/include/ps2_recompiled_stubs.h",
         "-e", "ps2xRuntime/include/ps2_overlay_functions.h",
         "-e", "games/bt3/work", "-e", "ps2xRuntime/src/lib/vu1_jit_gen.inc"])
    run(["git", "-C", BT3R, "apply", "--whitespace=nowarn", "--exclude=ps2xRuntime/third_party/parallel-gs",
         BT4R / "patches/bt3recomp-bt4mod-full.patch"])
    run(["git", "-C", BT3R, "apply", "--whitespace=nowarn", OURS])
    mark.write_text(want + "\n")


def toolchain_file():
    f = WORK / "android.cmake"
    f.write_text(f"""set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSROOT "{SYSROOT}")
set(CMAKE_C_COMPILER "{LLVM / 'bin/clang'}")
set(CMAKE_CXX_COMPILER "{LLVM / 'bin/clang++'}")
set(CMAKE_ASM_COMPILER "{LLVM / 'bin/clang'}")
set(CMAKE_C_COMPILER_TARGET aarch64-linux-android{API})
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-android{API})
set(CMAKE_ASM_COMPILER_TARGET aarch64-linux-android{API})
set(CMAKE_C_FLAGS_INIT "-fPIC")
set(CMAKE_CXX_FLAGS_INIT "-fPIC -stdlib=libc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld -rtlib=compiler-rt --unwindlib=none -L{TC / 'ndk/clang_rt'}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
""")
    return f


def ffmpeg():
    """FFmpeg for Android (the game's videos: MPEG-2), static, decoders only."""
    out = WORK / "ffmpeg"
    if (out / "lib/libavcodec.a").exists():
        return out
    src = WORK / "ffmpeg-src"
    if not (src / "configure").exists():
        run(["git", "clone", "--quiet", "--depth", "1", "-b", FFMPEG_TAG, FFMPEG_URL, src])
    cc = f"{LLVM / 'bin/clang'} --target=aarch64-linux-android{API}"
    ar = LLVM / "bin/llvm-ar"
    ranlib = LLVM / "bin/llvm-ranlib"
    if not ranlib.exists():
        ranlib = shutil.which("llvm-ranlib") or "true"  # (llvm-ar writes the index itself)
    run(["./configure", f"--prefix={out}", "--target-os=android", "--arch=aarch64", "--enable-cross-compile", f"--cc={cc}",
         f"--sysroot={SYSROOT}", f"--ar={ar}", f"--ranlib={ranlib}", f"--nm={LLVM / 'bin/llvm-nm'}", f"--strip={LLVM / 'bin/llvm-strip'}",
         "--enable-static", "--disable-shared", "--disable-programs", "--disable-doc", "--disable-avdevice", "--disable-avfilter",
         "--disable-network", "--disable-everything", "--enable-decoder=mpeg2video,mpeg1video,av1,pcm_s16le,adpcm_*",
         "--enable-parser=mpegvideo,av1", "--enable-demuxer=mpegps,mpegvideo,ivf,matroska,mov,wav", "--enable-protocol=file",
         "--enable-swscale", "--enable-swresample", "--enable-pic", "--disable-asm"], cwd=src)
    run(["make", f"-j{os.cpu_count() or 4}"], cwd=src)
    run(["make", "install"], cwd=src)
    return out


def generate(data):
    """The game's code from the mod's ELF and DBZP.BIN, with BT3-Recomp's own generator (games/bt3/setup.py, stage 3
    steps 2b-6: the VU1 programs, the function maps of the mod, the recompiler, the overlay)."""
    data = pathlib.Path(data)
    elf = next((p for p in (data / "SLUS_219.78", data / "SLUS_216.78") if p.exists()), None)
    dbzp = data / "BIN/DBZP.BIN"
    if elf is None or not dbzp.exists():
        sys.exit(f"{data} needs SLUS_219.78 and BIN/DBZP.BIN (the app's bt4/runtime/data folder, or the ISO's files)")
    if sha256(elf) != ELF_SHA256:
        sys.exit(f"{elf} is not the ELF of Budokai Tenkaichi 4 Beta 14 Rev2 (SLUS-219.78): the code is generated for that one")
    work = BT3R / "games/bt3/work"
    (work / "BIN").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(elf, work / "SLUS_216.78")
    shutil.copyfile(dbzp, work / "BIN/DBZP.BIN")
    # the recompiler, for this PC
    rb = WORK / "recomp-build"
    run(["cmake", "-S", BT3R, "-B", rb, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DPS2X_BUILD_RUNTIME=OFF",
         "-DPS2X_BUILD_ANALYZER=OFF"])
    run(["cmake", "--build", rb, "--target", "ps2_recomp"])
    recomp = next(rb.rglob("ps2_recomp"), None)
    if recomp is None:
        sys.exit("the recompiler (ps2_recomp) was not built")
    sys.path.insert(0, str(BT3R / "games/bt3"))
    import setup as bt3setup  # BT3-Recomp's games/bt3/setup.py (its functions, not its main)
    ctx = types.SimpleNamespace(elf=work / "SLUS_216.78", args=types.SimpleNamespace(skip_setup=False, gen_only=True))
    bt3setup.gen_vu1(ctx)
    bt3setup.generate_runner(ctx, recomp)


def check_symbols(lib):
    """Every symbol libbt4.so leaves to the system is one the system has: one of BT3's port the GS asks for and nothing
    here answers (bt4_shim.c) is otherwise only found on the device, as "dlopen failed: cannot locate symbol" (0.9.0's
    merge of the PC's 0.1.38 did that). The unwinder's _Unwind_* come from the device's libc, not the NDK's stubs."""
    def names(path, flags):
        r = subprocess.run([LLVM / "bin/llvm-nm", "-D", *flags, path], capture_output=True, text=True) # (a linker script: none)
        return {l.split()[-1].split("@")[0] for l in r.stdout.splitlines() if l.strip()}
    have = names(SDL3 / "lib/libSDL3.so", ["--defined-only"])
    for so in sorted((SYSROOT / f"usr/lib/aarch64-linux-android/{API}").glob("*.so")):
        have |= names(so, ["--defined-only"])
    want = {l.split()[-1].split("@")[0] for l in subprocess.run([LLVM / "bin/llvm-nm", "-D", "--undefined-only", lib],
            capture_output=True, text=True, check=True).stdout.splitlines() if l.split()[0] == "U"}
    missing = sorted(w for w in want - have if not w.startswith("_Unwind_"))
    if missing:
        sys.exit("libbt4.so needs symbols nothing defines (add them to host/bt4_shim.c): " + ", ".join(missing))


def build():
    out = WORK / "build"
    run(["cmake", "-S", HERE, "-B", out, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file()}",
         f"-DBT3R={BT3R}", f"-DDR={ROOT}", f"-DDRGEN={GEN}", f"-DSDL3={SDL3}", f"-DFFMPEG={ffmpeg()}"])
    run(["cmake", "--build", out, "--target", "bt4"])
    lib = out / "libbt4.so"
    check_symbols(lib)
    dst = ROOT / "android/app/src/main/jniLibs/arm64-v8a"
    dst.mkdir(parents=True, exist_ok=True)
    run([LLVM / "bin/llvm-strip", "--strip-debug", "-o", dst / "libbt4.so", lib])
    print(f"== libbt4.so ({(dst / 'libbt4.so').stat().st_size >> 20} MB) installed into {dst}")


def armsx2():
    """libbt4gsx.so: the renderer "ARMSX2" of the game (port/bt4/gsx): ARMSX2's PCSX2 core (its Android build, all its
    libraries from its own tree) with gsrunner as the host, reading the game's live GS stream. Its shaders and fonts go
    to the app's assets (bt4gsx/), which Bt4Activity copies out. Long the first time (the whole core and shaderc)."""
    src = ARMSX2
    if not (src / ".git").exists():
        run(["git", "clone", "--quiet", "--filter=blob:none", ARMSX2_URL, src])
    if subprocess.run(["git", "-C", src, "cat-file", "-e", ARMSX2_REV + "^{commit}"]).returncode != 0:
        run(["git", "-C", src, "fetch", "--quiet", "origin"])
    mark = src / ".dragonrage-patched"
    want = sha256(ARMSX2_PATCH) + " " + ARMSX2_REV
    if not mark.exists() or mark.read_text().strip() != want:
        print("== ARMSX2: its revision and Dragon Rage's patch (live GS stream, the app's window)", flush=True)
        run(["git", "-C", src, "checkout", "--quiet", "--force", ARMSX2_REV])
        run(["git", "-C", src, "reset", "--quiet", "--hard", ARMSX2_REV])
        run(["git", "-C", src, "apply", "--whitespace=nowarn", ARMSX2_PATCH])
        mark.write_text(want + "\n")
    cpp = src / "platforms/android/app/src/main/cpp"
    if not (cpp / "3rdparty/shaderc/third_party/spirv-tools").exists():
        print("== shaderc's sources (SPIRV-Tools, glslang...)", flush=True)
        run([sys.executable, "utils/git-sync-deps"], cwd=cpp / "3rdparty/shaderc")
    # The core's Android build, configured as a Linux aarch64 build with this toolchain, then told it is Android
    # (CMAKE_PROJECT_INCLUDE: after every project(), LINUX off and ANDROID on) - the NDK's own CMake files are not
    # in the toolchain android.py makes. libwebp wants the NDK's cpu-features: a small one is in gsx/ndk.
    # The core is built for one memory page size and refuses to start on another ("Page size mismatch"): 4 KB, what
    # phones use (ARMSX2 ships a 4 KB and a 16 KB core); on a 16 KB device the game keeps Dragon Rage's renderer.
    out = WORK / "armsx2-build"
    out.mkdir(parents=True, exist_ok=True)
    after = out / "after_project.cmake"
    after.write_text("set(LINUX FALSE)\nset(ANDROID TRUE)\n")
    tcf = out / "android.cmake"
    tcf.write_text(toolchain_file().read_text() + f"""set(CMAKE_SHARED_LINKER_FLAGS_INIT "${{CMAKE_SHARED_LINKER_FLAGS_INIT}} -static-libstdc++")
set(CMAKE_LIBRARY_PATH "{SYSROOT / f'usr/lib/aarch64-linux-android/{API}'}")
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-{API})
set(CMAKE_PROJECT_INCLUDE "{after}")
""")
    run(["cmake", "-S", cpp, "-B", out, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_TOOLCHAIN_FILE={tcf}",
         "-DANDROID=true", "-DANDROID_STL=c++_static", "-DARMSX2_ANDROID_HOST_PAGE_SIZE=0x1000",
         # (off whatever this PC has: with Rust's Android target installed it builds a liblibrashader_capi.so that does
         # not go into the APK, the library no longer opens, and the game falls silently to Dragon Rage's renderer)
         "-DARMSX2_DISABLE_LIBRASHADER=ON",
         "-DARMSX2_EMUCORE_LIBRARY_NAME=emucore", "-DHAVE_WL_VERSION_SCRIPT=1", "-DCMAKE_HAVE_LIBC_PTHREAD=1", f"-DANDROID_NDK={GSX / 'ndk'}",
         f"-DARMSX2_EXTRA_CMAKE={GSX / 'bt4gsx.cmake'}", f"-DBT4GSX_DIR={GSX}",
         f"-DBT4GSX_RING_DIR={BT3R / 'ps2xRuntime/src/lib'}"])
    run(["cmake", "--build", out, "--target", "bt4gsx"])
    dst = ROOT / "android/app/src/main/jniLibs/arm64-v8a"
    dst.mkdir(parents=True, exist_ok=True)
    run([LLVM / "bin/llvm-strip", "--strip-debug", "-o", dst / "libbt4gsx.so", out / "libbt4gsx.so"])
    needed = re.findall(r"NEEDED\s+(\S+)", subprocess.run([LLVM / "bin/llvm-objdump", "-p", dst / "libbt4gsx.so"],
                                                           capture_output=True, text=True).stdout)
    alien = [n for n in needed if n not in ("libandroid.so", "libEGL.so", "libGLESv2.so", "libGLESv1_CM.so", "libOpenSLES.so",
                                            "liblog.so", "libdl.so", "libz.so", "libm.so", "libc.so")]
    if alien:
        sys.exit(f"libbt4gsx.so needs {alien}, not in the system or the APK: it would not open on the device")
    # its resources: the shaders it compiles at run time, its fonts, the game index
    res = src / "bin/resources"
    assets = ROOT / "android/app/src/main/assets/bt4gsx"
    shutil.rmtree(assets, ignore_errors=True)
    for part in ("shaders/common", "shaders/opengl", "shaders/vulkan", "fonts"):
        shutil.copytree(res / part, assets / "resources" / part)
    shutil.copyfile(res / "GameIndex.yaml", assets / "resources/GameIndex.yaml")
    # an empty patches.zip: the game's own patches are the recompiled game's business (this core only draws its GS
    # stream); without one ARMSX2 says on screen at every start that it could not open it
    zipfile.ZipFile(assets / "resources/patches.zip", "w").close()
    # Its Vulkan shaders already compiled (gsx/cache, see the README there): without them the first start spends
    # 20 to 25 s compiling SPIR-V on the CPU before the first picture (85 shaders on the RG Cube).
    for f in sorted((GSX / "cache").glob("vulkan_shaders_*.*")):
        (assets / "cache").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(f, assets / "cache" / f.name)
    (assets / "version.txt").write_text(ARMSX2_REV + " " + sha256(dst / "libbt4gsx.so")[:16] + " " + GSX_FILES + "\n")
    print(f"== libbt4gsx.so ({(dst / 'libbt4gsx.so').stat().st_size >> 20} MB, for 4 KB memory pages) and its resources installed")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", help="folder with the mod's SLUS_219.78 and BIN/DBZP.BIN (generates the game's code)")
    ap.add_argument("--armsx2", action="store_true", help="also build ARMSX2's renderer for the game (libbt4gsx.so)")
    args = ap.parse_args()
    if not (LLVM / "bin/clang").exists() or not (SDL3 / "lib/libSDL3.so").exists() or not (GEN / "shaders.h").exists():
        sys.exit("Run python3 port/tools/android.py first (the toolchain, SDL3 and the GS shaders come from it).")
    sources()
    if args.data:
        generate(args.data)
    elif not any((BT3R / "ps2xRuntime/src/runner").glob("*.cpp")):
        sys.exit("No game code yet: run once with --data <folder with SLUS_219.78 and BIN/DBZP.BIN>.")
    build()
    if args.armsx2:
        armsx2()


if __name__ == "__main__":
    main()
