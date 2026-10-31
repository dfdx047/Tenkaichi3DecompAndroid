#!/usr/bin/env python3
"""Builds libbt2.so, BT2 Recomp for Android arm64, and puts it into Dragon Rage's app next to BT3's engine.

    python3 bt2/android/build.py [configure] [build] [install]        (all three when none is named)
    python3 bt2/android/build.py check                               (no game code needed)
    python3 bt2/android/build.py plugin                              (libbt2online.so, no game code needed)
    python3 bt2/android/build.py package                             (the motor .zip players import)
    python3 bt2/android/build.py key                                 (makes the signing key, once)

Needs the game's generated code (tools/ps2recomp/run_pipeline.py: game/generated, work/bt2-generation), the
Android NDK (ANDROID_NDK_HOME or ANDROID_NDK_LATEST_HOME), CMake and SDL3 for Android arm64 (BT2_SDL3_DIR;
default: the folder BT3's port/tools/android_toolchain.sh unpacks). paraLLEl-GS and xxHash are fetched at
their pinned revisions when missing.

install copies libbt2.so into android/app/src/main/jniLibs/arm64-v8a/ (beside libbt3.so). It is made from
the game's program: it stays out of the repository (.gitignore) and goes only into an APK built where the
game's code was generated. The runtime's startup inputs are not part of the APK: libbt2.so makes them from
the player's disc image (runtime/game/startup_inputs.h).

package makes build/android/bt2-motor-<version>.zip for the APKs that leave libbt2.so out (the public ones): the
stripped libbt2.so, manifest.json (version, BT2_ENGINE_ABI from runtime/android/engine_abi.h, size, SHA-256) and
manifest.sig, the manifest's ECDSA P-256 signature. It is shared outside GitHub (the Discord); the app imports it
(BT2 tab, data/Bt2Engine.kt) only when the signature is the one of the key whose public half the app carries and the
ABI is its own. The private key (BT2_ENGINE_KEY, default ~/.config/dragonrage/bt2-engine-key.pem) never goes into a
repository; key makes it when missing and prints the public half for Bt2Engine.PUBLIC_KEY.

check compiles the handwritten runtime and paraLLEl-GS for Android arm64 without the game's code
(android/CMakeLists.txt, BT2_RUNTIME_CHECK), in build/android-check: what the public CI can verify.
"""
import base64
import datetime
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

BT2 = pathlib.Path(__file__).resolve().parents[1]
APP = BT2.parent / "android/app/src/main"
BUILD = BT2 / "build/android"
SDL3 = pathlib.Path(os.environ.get("BT2_SDL3_DIR", str(BT2.parent / "port/build/android-toolchain/sdl3")))
API = os.environ.get("BT2_ANDROID_API", "29")
KEY = pathlib.Path(os.environ.get("BT2_ENGINE_KEY", str(pathlib.Path.home() / ".config/dragonrage/bt2-engine-key.pem")))


def ndk():
    for name in ("ANDROID_NDK_HOME", "ANDROID_NDK_LATEST_HOME", "ANDROID_NDK_ROOT"):
        path = os.environ.get(name)
        if path and (pathlib.Path(path) / "build/cmake/android.toolchain.cmake").exists():
            return pathlib.Path(path)
    sys.exit("No Android NDK: set ANDROID_NDK_HOME.")


def run(*command):
    print("+", " ".join(str(c) for c in command), flush=True)
    subprocess.run([str(c) for c in command], check=True)


def dependencies():
    if not (BT2 / "work/dependencies").is_dir() or not any((BT2 / "work/dependencies").glob("parallel-gs-*")):
        run(sys.executable, BT2 / "tools/graphics/gpu/fetch_parallel_gs.py")
    if not any((BT2 / "work/dependencies").glob("xxhash-*")):
        run(sys.executable, BT2 / "tools/graphics/textures/fetch_xxhash.py")


def cmake_configure(build_dir, *extra):
    generator = ["-G", "Ninja"] if shutil.which("ninja") else []
    run("cmake", "-S", BT2 / "android", "-B", build_dir, *generator,
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_TOOLCHAIN_FILE={ndk() / 'build/cmake/android.toolchain.cmake'}",
        "-DANDROID_ABI=arm64-v8a", f"-DANDROID_PLATFORM=android-{API}", "-DANDROID_STL=c++_static",
        f"-DBT2_SDL3_DIR={SDL3}", *extra)


def jobs():
    return os.environ.get("BT2_JOBS", str(os.cpu_count() or 2))


def configure():
    if not (BT2 / "game/generated/bt2").is_dir():
        sys.exit("No generated BT2 code (game/generated/bt2): run tools/ps2recomp/run_pipeline.py with your disc image first.")
    dependencies()
    cmake_configure(BUILD)


def check():
    dependencies()
    build_dir = BT2 / "build/android-check"
    cmake_configure(build_dir, "-DBT2_RUNTIME_CHECK=ON")
    run("cmake", "--build", build_dir, "--target", "bt2-runtime-check", "parallel-gs", "adrenotools", "--parallel", jobs())
    print("check: the handwritten runtime and paraLLEl-GS compile for Android arm64")


def plugin():
    """libbt2online.so (plugin/): Dragon Rage's own part, built without the game's code; into the app's jniLibs."""
    dependencies()
    build_dir = BT2 / "build/android-check"
    cmake_configure(build_dir, "-DBT2_RUNTIME_CHECK=ON")
    run("cmake", "--build", build_dir, "--target", "bt2online", "--parallel", jobs())
    library = build_dir / "libbt2online.so"
    jni = APP / "jniLibs/arm64-v8a"
    jni.mkdir(parents=True, exist_ok=True)
    shutil.copy(library, jni / library.name)
    strip = next(ndk().glob("toolchains/llvm/prebuilt/*/bin/llvm-strip"), None)
    if strip:
        run(strip, "--strip-unneeded", jni / library.name)
    print("plugin: installed into", jni.relative_to(BT2.parent))


def build():
    run("cmake", "--build", BUILD, "--target", "bt2-entry-probe", "--parallel", jobs())


def install():
    library = BUILD / "libbt2.so"
    if not library.exists():
        sys.exit(f"{library} is missing: build first.")
    jni = APP / "jniLibs/arm64-v8a"
    jni.mkdir(parents=True, exist_ok=True)
    shutil.copy(library, jni / library.name)
    strip = next(ndk().glob("toolchains/llvm/prebuilt/*/bin/llvm-strip"), None)
    if strip:
        run(strip, "--strip-debug", jni / library.name)
    print("installed into", jni.relative_to(BT2.parent))


def stripped(library, out):
    shutil.copy(library, out)
    strip = next(ndk().glob("toolchains/llvm/prebuilt/*/bin/llvm-strip"), None)
    if not strip:
        sys.exit("No llvm-strip in the NDK.")
    run(strip, "--strip-debug", out)


def key():
    if not KEY.exists():
        KEY.parent.mkdir(parents=True, exist_ok=True)
        run("openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", KEY)
        KEY.chmod(0o600)
        print("made", KEY, "- keep a copy of it: without it no motor the published apps accept can be made")
    public = subprocess.run(["openssl", "ec", "-in", str(KEY), "-pubout", "-outform", "DER"],
                            check=True, capture_output=True).stdout
    print("public key (Bt2Engine.PUBLIC_KEY):", base64.b64encode(public).decode())


def package():
    library = BUILD / "libbt2.so"
    if not library.exists():
        sys.exit(f"{library} is missing: build first.")
    if not KEY.exists():
        sys.exit(f"No signing key at {KEY}: run the key step (or set BT2_ENGINE_KEY).")
    abi = re.search(r"^#define BT2_ENGINE_ABI (\d+)", (BT2 / "runtime/android/engine_abi.h").read_text(), re.M)
    commit = subprocess.run(["git", "-C", str(BT2), "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()
    version = datetime.date.today().strftime("%Y.%m.%d") + (f"-{commit}" if commit else "")
    with tempfile.TemporaryDirectory() as work:
        work = pathlib.Path(work)
        lib = work / "libbt2.so"
        stripped(library, lib)
        data = lib.read_bytes()
        manifest = json.dumps({
            "game": "SLUS-21441", "version": version, "abi": int(abi.group(1)),
            "library": lib.name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        }, indent=2).encode()
        (work / "manifest.json").write_bytes(manifest)
        run("openssl", "dgst", "-sha256", "-sign", KEY, "-out", work / "manifest.sig", work / "manifest.json")
        out = BUILD / f"bt2-motor-{version}.zip"
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for name in ("manifest.json", "manifest.sig", lib.name):
                z.write(work / name, name)
    print(f"packaged {out.relative_to(BT2.parent)} ({out.stat().st_size // (1 << 20)} MB, ABI {abi.group(1)})")


def main():
    steps = sys.argv[1:] or ["configure", "build", "install"]
    for step in steps:
        {"configure": configure, "build": build, "install": install, "check": check, "plugin": plugin, "package": package, "key": key}[step]()


if __name__ == "__main__":
    main()
