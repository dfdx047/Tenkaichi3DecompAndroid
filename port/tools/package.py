#!/usr/bin/env python3
"""Makes the release folder and zip: the finished port without any game data, plus the setup window.

    BT3_CC=clang64 python3 port/tools/undefined.py && BT3_CC=clang64 python3 port/tools/link.py
    port/setup/build.sh
    python3 port/tools/package.py          -> port/build/release/ and port/build/Tenkaichi3Decomp-linux-x64.zip
    BT3_CC=win64 python3 port/tools/package.py   (after the win64 build and `port/setup/build.sh win`)
                                           -> port/build/release-win/ and port/build/Tenkaichi3Decomp-windows-x64.zip

Contents:  Tenkaichi3Decomp, Tenkaichi3Decomp.dat   the game without the data tables of the original programs (strip_data.py)
           Tenkaichi3Decomp-setup       the setup window: reads the user's own disc image and unpacks it into gamedata/
           lib/            SDL3, so that it does not have to be installed
           README.txt, licenses/
The user needs nothing but their disc image. Nothing from the disc is in the zip."""
import os, pathlib, shutil, subprocess, sys, zipfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

ROOT = pathlib.Path(__file__).resolve().parents[2]
NAME = "Tenkaichi3Decomp"  # the program's name in a release: NAME, NAME.dat, NAME-setup
OUT = ROOT / "port/build/release"
ZIP = ROOT / "port/build/Tenkaichi3Decomp-linux-x64.zip"

README = """Dragon Ball Z: Budokai Tenkaichi 3 - PC port (Linux, 64-bit)

1. Start  Tenkaichi3Decomp-setup
2. Choose your own disc image of the USA release (SLUS-21678), an .iso file.
   The setup checks it and unpacks the game's data next to this file. About 4 GB are needed.
3. Press Play. Later, start  Tenkaichi3Decomp-setup  again and press Play, or run  ./play.sh

In the game, F1 opens the settings: resolution, widescreen, controls and sound.
Saves are kept in saves/. A file placed in gamedata/mods/<same path as the original> replaces the original.
Texture packs (PCSX2 naming, .dds or .png) go into textures/: see the note there.

This package contains no game data. The game's data comes from your disc and stays on your computer.
"""

WIN_README = README.replace("(Linux, 64-bit)", "(Windows, 64-bit)").replace("Start  Tenkaichi3Decomp-setup\n", "Start  Tenkaichi3Decomp-setup.exe\n") \
    .replace("start  Tenkaichi3Decomp-setup  again and press Play, or run  ./play.sh", "start  Tenkaichi3Decomp-setup.exe  again and press Play, or run  play.bat")

TEXTURES_NOTE = """Texture packs go here.

Put a pack's replacement textures into this folder, in subfolders or not: the game looks through all of it when it
starts. The files have to be named the way the PCSX2 emulator names texture replacements for this game
(for example 1dd4c76113969303-56e3d4469d2ad392-00005e54.dds), so a pack made for PCSX2 can be copied in as it is.
Read: .dds files (DXT1, DXT3, DXT5 or plain 32-bit) and .png files.

In the game, F1 > Video has a switch for the pack.
"""

def take_program(exe, out):
    """The program without the game's data, and its list, into the release folder. A program built from the blank
    tables (port/data) is that already; one built with the tables' values has them taken out by strip_data.py."""
    import toolchain
    out.mkdir(parents=True)
    name = NAME + ".exe" if exe.suffix == ".exe" else NAME
    # where the game's own variables lie in the program (make_state.py, at link time): the program reads it from
    # beside itself to save and restore its state, which an online session is built on
    mem = pathlib.Path(str(exe) + ".mem")
    if not mem.exists():
        sys.exit(f"missing {mem.name}: link.py makes it")
    shutil.copy2(mem, out / (name + ".mem"))
    if (toolchain.DATA / "SKELETON").exists():
        dat = pathlib.Path(str(exe)[:-4] + ".dat" if exe.suffix == ".exe" else str(exe) + ".dat")
        if not dat.exists():
            sys.exit(f"missing {dat.name}: link.py makes it")
        shutil.copy2(exe, out / name)
        shutil.copy2(dat, out / (NAME + ".dat"))
        print("program built from the blank data tables (no disc involved)")
        return
    r = subprocess.run([sys.executable, str(ROOT / "port/tools/strip_data.py"), "--exe", str(exe), "--out", str(out)], capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode or "NOT matched" in r.stdout:
        sys.exit("strip_data.py failed or left game data in the program: not packaging\n" + r.stderr)
    (out / ("bt3.exe" if exe.suffix == ".exe" else "bt3")).rename(out / name)  # strip_data.py writes bt3 and bt3.dat
    (out / "bt3.dat").rename(out / (NAME + ".dat"))

def main_win():
    """BT3_CC=win64: bt3.exe (cross-built or built in MSYS2), Tenkaichi3Decomp-setup.exe (port/setup/build.sh win), SDL3.dll."""
    import toolchain
    out, zpath = ROOT / "port/build/release-win", ROOT / "port/build/Tenkaichi3Decomp-windows-x64.zip"
    exe, setup = ROOT / "port/build/bt3.exe", ROOT / "port/build/Tenkaichi3Decomp-setup.exe"
    dll = next((p for p in [toolchain.sdl3() / "bin/SDL3.dll"] if p.exists()), None) if toolchain.sdl3() else None
    if dll is None:
        dll = next((pathlib.Path(d) / "SDL3.dll" for d in os.environ.get("PATH", "").split(os.pathsep) if (pathlib.Path(d) / "SDL3.dll").exists()), None)
    for need in (exe, setup, pathlib.Path(str(exe) + ".map")):
        if not need.exists():
            sys.exit(f"missing {need.relative_to(ROOT)}: build first (BT3_CC=win64 undefined.py and link.py; port/setup/build.sh win)")
    if dll is None:
        sys.exit("SDL3.dll not found")
    if out.exists():
        shutil.rmtree(out)
    take_program(exe, out)
    shutil.copy2(setup, out / "Tenkaichi3Decomp-setup.exe")
    shutil.copy2(dll, out / "SDL3.dll")
    (out / "README.txt").write_text(WIN_README.replace("\n", "\r\n"))
    (out / "play.bat").write_text('@echo off\r\nrem Starts the game from its menus.\r\ncd /d "%~dp0"\r\nset BT3_GS=gpu\r\nstart "" Tenkaichi3Decomp.exe\r\n')
    lic = out / "licenses"
    lic.mkdir()
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    (out / "textures").mkdir()
    (out / "textures/README.txt").write_text(TEXTURES_NOTE.replace("\n", "\r\n"))
    (lic / "sdl3.txt").write_text("SDL3 (SDL3.dll) is distributed under the zlib license: https://www.libsdl.org/license.php\n")
    for f in (out / "Tenkaichi3Decomp.exe", out / "Tenkaichi3Decomp-setup.exe"):
        subprocess.run([toolchain.PREFIX + "strip", "--strip-debug", str(f)], check=False)
    if zpath.exists():
        zpath.unlink()
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(out.rglob("*")):
            if p.is_file():
                z.write(p, "Tenkaichi3Decomp/" + str(p.relative_to(out)))
    print(f"{zpath.relative_to(ROOT)}: {zpath.stat().st_size / 1e6:.1f} MB")
    for p in sorted(out.rglob("*")):
        if p.is_file():
            print(f"  {p.stat().st_size:>10,}  {p.relative_to(out)}")

def main():
    exe = ROOT / "port/build/bt3_64"
    setup = ROOT / "Tenkaichi3Decomp-setup"
    for need in (exe, setup, pathlib.Path(str(exe) + ".map")):
        if not need.exists():
            sys.exit(f"missing {need.relative_to(ROOT)}: build first (see the top of this file)")
    if OUT.exists():
        shutil.rmtree(OUT)
    take_program(exe, OUT)
    shutil.copy2(setup, OUT / "Tenkaichi3Decomp-setup")
    (OUT / "lib").mkdir()
    lib = next((p for d in ("/usr/local/lib", "/usr/lib", "/usr/lib64", "/usr/lib/x86_64-linux-gnu") for p in sorted(pathlib.Path(d).glob("libSDL3.so.0"))), None)
    if lib is None:
        sys.exit("libSDL3.so.0 not found")
    shutil.copy2(lib.resolve(), OUT / "lib/libSDL3.so.0")
    (OUT / "README.txt").write_text(README)
    (OUT / "play.sh").write_text('#!/bin/sh\n# Starts the game from its menus.\ncd "$(dirname "$0")" && BT3_GS=gpu exec ./Tenkaichi3Decomp\n')
    (OUT / "play.sh").chmod(0o755)
    lic = OUT / "licenses"
    lic.mkdir()
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    (OUT / "textures").mkdir()
    (OUT / "textures/README.txt").write_text(TEXTURES_NOTE)
    (lic / "sdl3.txt").write_text("SDL3 (lib/libSDL3.so.0) is distributed under the zlib license: https://www.libsdl.org/license.php\n")
    for f in (OUT / "Tenkaichi3Decomp", OUT / "Tenkaichi3Decomp-setup"):
        subprocess.run(["strip", "--strip-debug", str(f)], check=False)  # the symbol names stay (crash reports use them)
    if ZIP.exists():
        ZIP.unlink()
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(OUT.rglob("*")):
            if p.is_file():
                info = zipfile.ZipInfo.from_file(p, "Tenkaichi3Decomp/" + str(p.relative_to(OUT)))
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, p.read_bytes(), compresslevel=9)
    print(f"{ZIP.relative_to(ROOT)}: {ZIP.stat().st_size / 1e6:.1f} MB")
    for p in sorted(OUT.rglob("*")):
        if p.is_file():
            print(f"  {p.stat().st_size:>10,}  {p.relative_to(OUT)}")
    portable(OUT)

def portable(out):
    """Says whether the release would run on other people's machines: no CPU level above the baseline demanded
    or used, and no newer C library than Ubuntu 22.04's (2.35). Build with port/release/build_linux.sh to pass."""
    bad = []
    for f in ("Tenkaichi3Decomp", "Tenkaichi3Decomp-setup", "lib/libSDL3.so.0"):
        note = subprocess.run(["readelf", "-n", str(out / f)], capture_output=True, text=True).stdout
        need = next((l.split(":", 1)[1].strip() for l in note.splitlines() if "ISA needed" in l), "")
        if any(v in need for v in ("x86-64-v2", "x86-64-v3", "x86-64-v4")):
            bad.append(f"{f}: stamped as needing {need}")
        dis = subprocess.run(["objdump", "-d", str(out / f)], capture_output=True, text=True).stdout
        if f != "lib/libSDL3.so.0" and ("%ymm" in dis or "%zmm" in dis):
            bad.append(f"{f}: contains AVX instructions")
        if "%zmm" in dis:
            bad.append(f"{f}: contains AVX-512 instructions")
        syms = subprocess.run(["objdump", "-T", str(out / f)], capture_output=True, text=True).stdout
        vers = sorted({tuple(int(x) for x in v.split(".")) for v in __import__("re").findall(r"GLIBC_(\d+\.\d+)", syms)})
        if vers and vers[-1] > (2, 35):
            bad.append(f"{f}: needs glibc {vers[-1][0]}.{vers[-1][1]}")
    print("portable: yes (baseline x86-64, glibc <= 2.35)" if not bad else "NOT PORTABLE:\n  " + "\n  ".join(bad))

if os.environ.get("BT3_CC") == "win64":
    main_win()
else:
    main()
