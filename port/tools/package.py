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
           a11y_<code>.txt the screen reader's lines in other languages (port/lang; its own labels, no game text)

           fonts/          the fonts of F1 > UI with their licences (port/third_party/fonts as it is)
           tbo/ui/         the pictures of the Tenkaichi Budokai Online screens (port/tbo/ui), their credits in licenses/
           README.txt, licenses/
The user needs nothing but their disc image. Nothing from the disc is in the zip."""
import os, pathlib, re, shutil, subprocess, sys, zipfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

ROOT = pathlib.Path(__file__).resolve().parents[2]
NAME = "Tenkaichi3Decomp"  # the program's name in a release: NAME, NAME.dat, NAME-setup
OUT = ROOT / "port/build/release"
ZIP = ROOT / "port/build/Tenkaichi3Decomp-linux-x64.zip"

README = """Dragon Ball Z: Budokai Tenkaichi 3 - PC port (Linux, 64-bit)

1. Start  Tenkaichi3Decomp-setup
2. Choose your own disc image of the USA release (SLUS-21678), an .iso file.
   The setup checks it and unpacks the game's data next to this file. About 4 GB are needed.
3. Press Play. Later, start  Tenkaichi3Decomp  itself (or the setup again, and press Play).

In the game, F1 opens the settings: resolution, widescreen, controls and sound.
Saves are kept in saves/. A file placed in gamedata/mods/<same path as the original> replaces the original.
Texture packs (PCSX2 naming, .dds or .png) go into textures/, stages and music from outside the disc into
stages/ and songs/: see the note in each folder.
F1 > UI has two font settings, both "Original" unless you choose: "Menu font" draws the menus' labels and
"Dialogue / UI font" the game's text from a font in fonts/ instead of the game's own lettering (the fonts and their
licences are in that folder; fonts.tsv there is the list, to which a .ttf of your own can be added). For the menus
the game makes a list of their labels from your disc data the first time it starts (gamedata/game-text/, a moment
in the background): the fonts appear under "Menu font" when it is done.

Screen reader: with Prism's library (prism.dll, libprism.so; release 0.18.3) placed next to the program, the menus
and the fight's gauges are spoken and shown in braille. a11y_es.txt is its Spanish; another language is a file
a11y_<code>.txt of the same kind, and lines of your own go into a11y_<code>_own.txt, which no update replaces.

This package contains no game data. The game's data comes from your disc and stays on your computer.

The port's own code is under the GNU AGPL, version 3 (licenses/tenkaichi3decomp-agpl.txt; code from before the
change of licence also under MIT, licenses/tenkaichi3decomp-mit.txt). Its source, this release's tag included:
https://github.com/z3xox/Tenkaichi3Decomp
"""

WIN_README = README.replace("(Linux, 64-bit)", "(Windows, 64-bit)").replace("Start  Tenkaichi3Decomp-setup\n", "Start  Tenkaichi3Decomp-setup.exe\n") \
    .replace("start  Tenkaichi3Decomp  itself", "start  Tenkaichi3Decomp.exe  itself")

TEXTURES_NOTE = """Texture packs go here.

Put a pack's replacement textures into this folder, in subfolders or not: the game looks through all of it when it
starts. The files have to be named the way the PCSX2 emulator names texture replacements for this game
(for example 1dd4c76113969303-56e3d4469d2ad392-00005e54.dds), so a pack made for PCSX2 can be copied in as it is.
Read: .dds files (DXT1, DXT3, DXT5 or plain 32-bit) and .png files.

In the game, F1 > Video has a switch for the pack.
"""

STAGES_NOTE = """Stages from outside the disc.

Put a stage's model file (.unk, the kind made for this game's mods) into this folder. It gets one more cell at the
end of the stage select, named after the file ("My_Map.unk" is shown as "My Map"). Up to 26 stages.

Optional: a file maps.txt here, one line per stage, "file.unk|Name In The Menu", gives other names and an order.

Added stages are not written to your save, and they are not offered in an online match (Dragon Net Battle).
"""

SONGS_NOTE = """Music from outside the disc.

Put a song as an .mp3 or an .adx file (CRI ADX, the game's own music format) into this folder. It is added to the
music choice of the stage select, before "Random", named after the file. Up to 40 songs. An .mp3 is converted to
.adx when the game starts, once (the .adx is written next to it, while the game is already running: a new song may
be missing from the first select screen and is there from the next one on). An .mp3 that is replaced by another is
converted again. An .adx you made yourself is never replaced: an .mp3 of the same name beside it is left alone. An
.mp3 that cannot be read is left out, with a notice, and noted in mp3-failed.txt here; it is tried again when the
file changes. Other formats have to be converted first (ffmpeg can write ADX: ffmpeg -i song.ogg -ar 24000 -ac 2
-c:a adpcm_adx song.adx).

Optional: a file songs.txt here, one line per song, "file.adx|Name In The Menu" (or the .mp3), gives other names
and an order.

Added songs are not written to your save, and they are not offered in an online match (Dragon Net Battle).
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

def take_lang(out):
    """The screen reader's lines in other languages (port/lang/a11y_<code>.txt, read by port/src/gs/a11y.c from beside
    the program). They hold the reader's own labels only, none of the game's text."""
    for p in sorted((ROOT / "port/lang").glob("a11y_*.txt")):
        if not p.name.endswith("_own.txt"):
            shutil.copy2(p, out / p.name)

def take_fonts(out, lic):
    """The fonts the settings offer, and their licences next to them and in licenses/. Only what is in the
    repository: the list of menu labels (gamedata/game-text) is made from the disc, by the game at its first start
    on the player's machine (port/src/gs/menu_labels.c), and never packaged."""
    src = ROOT / "port/third_party/fonts"
    for p in sorted(src.rglob("*")):
        if p.is_file():
            (out / "fonts" / p.relative_to(src)).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, out / "fonts" / p.relative_to(src))
            if p.name == "LICENSE":
                shutil.copy2(p, lic / f"{p.parent.name}-fonts.txt")

def take_tbo_art(out, lic):
    """The pictures of the Tenkaichi Budokai Online screens, and who made them (port/tbo/ui/CREDITS.md)."""
    src = ROOT / "port/tbo/ui"
    (out / "tbo/ui").mkdir(parents=True, exist_ok=True)
    for p in sorted(src.glob("*.png")):
        shutil.copy2(p, out / "tbo/ui" / p.name)
    shutil.copy2(src / "CREDITS.md", lic / "tbo-ui-credits.txt")

def bundle_sdl_dependencies(dll, out, lic, prefix):
    """MSYS2 SDL can import libiconv; the official SDL archive is self-contained.
    Collect vendor DLLs from the SDL bin folder, never Windows system DLLs."""
    todo, seen = [dll], set()
    while todo:
        current = todo.pop()
        if current.name.lower() in seen:
            continue
        seen.add(current.name.lower())
        result = subprocess.run([prefix + "objdump", "-p", str(current)], capture_output=True, text=True, check=True)
        for name in re.findall(r"DLL Name:\s*(\S+)", result.stdout):
            source = dll.parent / name
            if source.exists() and name.lower() not in seen:
                shutil.copy2(source, out / name)
                todo.append(source)
            elif name.lower().startswith("lib") and not source.exists():
                sys.exit("missing SDL runtime dependency: " + name)
    if "libiconv-2.dll" in seen:
        notices = dll.parent.parent / "share/licenses/libiconv"
        shutil.copy2(notices / "COPYING.LIB", lic / "libiconv-lgpl.txt")
        shutil.copy2(notices / "README", lic / "libiconv-readme.txt")
        (lic / "libiconv-source.txt").write_text(
            "libiconv source: https://ftp.gnu.org/pub/gnu/libiconv/\n"
            "MSYS2 build recipe: https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-libiconv\n")

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
    take_lang(out)
    shutil.copy2(setup, out / "Tenkaichi3Decomp-setup.exe")
    shutil.copy2(dll, out / "SDL3.dll")
    (out / "README.txt").write_text(WIN_README.replace("\n", "\r\n"))
    lic = out / "licenses"
    lic.mkdir()
    bundle_sdl_dependencies(dll, out, lic, toolchain.PREFIX)
    shutil.copy2(ROOT / "LICENSE", lic / "tenkaichi3decomp-agpl.txt")
    shutil.copy2(ROOT / "LICENSE-MIT", lic / "tenkaichi3decomp-mit.txt")
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    shutil.copy2(ROOT / "port/third_party/dr_libs/LICENSE", lic / "dr_mp3.txt")

    take_fonts(out, lic)
    take_tbo_art(out, lic)
    (out / "textures").mkdir()
    (out / "textures/README.txt").write_text(TEXTURES_NOTE.replace("\n", "\r\n"))
    for kind, note in (("stages", STAGES_NOTE), ("songs", SONGS_NOTE)):
        (out / kind).mkdir()
        (out / kind / "README.txt").write_text(note.replace("\n", "\r\n"))
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
    take_lang(OUT)
    shutil.copy2(setup, OUT / "Tenkaichi3Decomp-setup")
    (OUT / "lib").mkdir()
    lib = next((p for d in ("/usr/local/lib", "/usr/lib", "/usr/lib64", "/usr/lib/x86_64-linux-gnu") for p in sorted(pathlib.Path(d).glob("libSDL3.so.0"))), None)
    if lib is None:
        sys.exit("libSDL3.so.0 not found")
    shutil.copy2(lib.resolve(), OUT / "lib/libSDL3.so.0")
    (OUT / "README.txt").write_text(README)
    lic = OUT / "licenses"
    lic.mkdir()
    shutil.copy2(ROOT / "LICENSE", lic / "tenkaichi3decomp-agpl.txt")
    shutil.copy2(ROOT / "LICENSE-MIT", lic / "tenkaichi3decomp-mit.txt")
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    shutil.copy2(ROOT / "port/third_party/dr_libs/LICENSE", lic / "dr_mp3.txt")

    take_fonts(OUT, lic)
    take_tbo_art(OUT, lic)
    (OUT / "textures").mkdir()
    (OUT / "textures/README.txt").write_text(TEXTURES_NOTE)
    for kind, note in (("stages", STAGES_NOTE), ("songs", SONGS_NOTE)):
        (OUT / kind).mkdir()
        (OUT / kind / "README.txt").write_text(note)
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
