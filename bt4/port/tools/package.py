#!/usr/bin/env python3
"""Makes the release folder and zip: the finished port without any game data, plus the setup window.

    BT3_CC=clang64 python3 port/tools/undefined.py && BT3_CC=clang64 python3 port/tools/link.py
    port/setup/build.sh
    python3 port/tools/package.py          -> port/build/release/ and port/build/BT4decomp-linux-x64.zip
    BT3_CC=win64 python3 port/tools/package.py   (after the win64 build and `port/setup/build.sh win`)
                                           -> port/build/release-win/ and port/build/BT4decomp-windows-x64.zip

Contents:  BT4decomp, BT4decomp.dat   the game without the data tables of the original programs (strip_data.py)
           BT4decomp-setup       the setup window: reads the user's own disc image and unpacks it into gamedata/
           lib/            SDL3, so that it does not have to be installed
           a11y_<code>.txt the screen reader's lines in other languages (port/lang; its own labels, no game text)

           fonts/          the fonts of F1 > UI with their licences (port/third_party/fonts as it is)
           translations/labels/   menu and battle language catalogs
           translations/ui/       optional bundled game UI catalogs
           translations/port/     text added by the PC port
           tbo/ui/         the pictures of the Tenkaichi Budokai Online screens (port/tbo/ui), their credits in licenses/
           README.txt, licenses/
The user needs nothing but their disc image. Nothing from the disc is in the zip."""
import hashlib, json, os, pathlib, re, shutil, subprocess, sys, zipfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

ROOT = pathlib.Path(__file__).resolve().parents[2]
NAME = "BT4decomp"  # the program's name in a release: NAME, NAME.dat, NAME-setup
OUT = ROOT / "port/build/release"
ZIP = ROOT / "port/build/BT4decomp-linux-x64.zip"

README = """DBZ BT4 Tag Team Decomp (Linux, 64-bit)
BT4 B14 Rev2. The installed revision is recorded in BT4_VERSION.txt.

Extract the complete ZIP before starting setup. Keep the setup application and
BT4decomp together in the installation folder.

1. Start  BT4decomp-setup
2. Choose the setup language and the game language or languages.
3. Select your own Spanish LATINO or English BT4 B14 Rev2 ISO (SLUS-21978).
4. Complete setup, then start  BT4decomp  itself.

A single-language installation starts in that language. A bilingual installation
downloads the second language package and shows the language selector at startup.
Setup requests GitHub authorization when access to that package requires it.
Do not use a BT3 USA or European ISO for this BT4 build.

F1 opens video, effects, audio, UI and control settings. In-game menus use the
controls configured for the game. The pause menu can hide or restore the HUD.

Saves are stored in saves/. Optional custom stages go in stages/ and custom music
in songs/. A compatible stage thumbnail uses the same filename stem as its .unk
model, with a .png extension. See each folder's README for supported files.

When a newer signed release is available from this project's GitHub repository,
the updater downloads it in the background and verifies its hash and signature.
Installation is deferred to the main menu outside an active online session.
Existing game data, saves, settings and custom content are preserved. Launchers
that predate automatic updates need one manual installation of an updater build.

This package contains no ISO or extracted game data. Setup reads the player's ISO.
The exact BT4 build identity is recorded in BT4_VERSION.txt.

Source and releases: https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp
Upstream PC port: https://github.com/z3xox/Tenkaichi3Decomp
See licenses/ for the AGPL, MIT and bundled component licenses.
"""

WIN_README = README.replace("(Linux, 64-bit)", "(Windows, 64-bit)").replace("Start  BT4decomp-setup\n", "Start  BT4decomp-setup.exe\n") \
    .replace("start  BT4decomp  itself", "start  BT4decomp.exe  itself")

TEXTURES_NOTE = """Texture packs go here.

Copy a texture pack ZIP into this folder, or add its extracted PNG/DDS files. ZIP packs are read directly:
no extraction or second copy is needed. Restart the game after adding or removing a pack.

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

Added stages are not written to your save, and they are not offered in an online match (Tenkaichi Budokai Online).
"""

SONGS_NOTE = """Music from outside the disc.

Put a song as an .mp3, .wav, .flac or .adx file (ADX is the game's own music format) into this folder. It is added
to the music choice of the stage select, before "Random", named after the file. Up to 40 songs. An .mp3, .wav or
.flac is played as it is (miniaudio decodes it; there is no conversion pass and no .adx written next to it), so a
new song is in the list from the next start. An .adx is played by the game's own decoder. A file that cannot be
read stays in the list and is silent (the log says why). An .adx that an earlier version wrote next to an .mp3 is
not listed while the .mp3 is there (it can be deleted). Anything else (ogg, m4a...) has to be converted first
(ffmpeg can write the formats above, or ADX: ffmpeg -i song.ogg -ar 24000 -ac 2 -c:a adpcm_adx song.adx).

Optional: a file songs.txt here, one line per song, "file|Name In The Menu", gives other names and an order.

Added songs are not written to your save, and they are not offered in an online match (Tenkaichi Budokai Online).
"""


MODS_NOTE = """Script mods.

Each mod is a folder here with a file mod.lua in it (a Lua script): mods/<name>/mod.lua. A mod can draw over the
game's picture, read the fight (health, ki, positions) and change how the fight camera is shown. It cannot change a
fight, read or write files or reach the network, so mods can be used in an online match.

In the game, F1 > Mods lists them: switch each on or off, change its settings, see why one stopped, and load them all
again after adding or changing one. Your choices are kept in bt3_mods.txt. A mod starts switched off.

The ones that come with the game: modern_hud (a HUD in the style of later games), modern_camera (inertia, tilt, hit
reactions, distance, height and zoom), modern_look (anime shading, rim light, colours, ambient shadows), skill_list
(your moves and their buttons while you hold charge), and two small examples to start from (example_hud,
example_camera).

How to write one: docs/port/mods.md in the source repository.
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

def take_translations(out):
    """Bundled label, UI, and port-text catalogs."""
    target = out / "translations"
    for kind in ("labels", "ui", "port"):
        (target / kind).mkdir(parents=True, exist_ok=True)
        for source in sorted((ROOT / "port/translations" / kind).glob("*.json")):
            if re.fullmatch(r"[a-z]{2,3}(?:-[A-Za-z]{2,3})?", source.stem):
                shutil.copy2(source, target / kind / source.name)
    bindings = ROOT / "port/translations/labels/pt-BR-native.tsv"
    if bindings.is_file():
        shutil.copy2(bindings, target / "labels" / bindings.name)

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

def take_example_mods(out):
    """The example script mods (port/examples/mods) into mods/, each in its own folder. A mod starts switched off, so
    they change nothing until the player switches one on in F1 > Mods. The reserved copy reaches older BT4
    updaters that protect mods/ entirely; startup installs missing scripts without overwriting player files."""
    src = ROOT / "port/examples/mods"
    for d in sorted(p for p in src.iterdir() if p.is_dir() and (p / "mod.lua").exists()):
        shutil.copytree(d, out / "mods" / d.name)
        shutil.copytree(d, out / "tbo/bundled-mods" / d.name)

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

def take_windows_runtime(out):
    source = ROOT / "port/build/windows-runtime"
    manifest = source / "manifest.json"
    if not manifest.exists():
        sys.exit("Windows voice and GitHub setup helpers are missing: run port/tools/prepare_windows_runtime.py before packaging")
    entries = json.loads(manifest.read_text())["files"]
    required = {"libdatachannel.dll", "libopus-0.dll", "libwinpthread-1.dll", "tbo/setup/gh.exe"}
    if not required <= set(entries):
        sys.exit("Incomplete Windows runtime dependency manifest")
    for name, expected in entries.items():
        path = pathlib.PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts:
            sys.exit("Invalid Windows runtime dependency path")
        data = (source / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            sys.exit("Windows runtime dependency checksum mismatch: " + name)
        target = out / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    shutil.copy2(manifest, out / "tbo/setup/runtime-manifest.json")


def main_win():
    """BT3_CC=win64: bt3.exe (cross-built or built in MSYS2), BT4decomp-setup.exe (port/setup/build.sh win), SDL3.dll."""
    import toolchain
    out, zpath = ROOT / "port/build/release-win", ROOT / "port/build/BT4decomp-windows-x64.zip"
    exe, setup = ROOT / "port/build/bt3.exe", ROOT / "port/build/BT4decomp-setup.exe"
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
    take_translations(out)
    shutil.copy2(ROOT / "port/release/BT4_VERSION.txt", out / "BT4_VERSION.txt")
    shutil.copy2(setup, out / "BT4decomp-setup.exe")
    shutil.copy2(dll, out / "SDL3.dll")
    (out / "README.txt").write_text(WIN_README.replace("\n", "\r\n"))
    lic = out / "licenses"
    lic.mkdir()
    bundle_sdl_dependencies(dll, out, lic, toolchain.PREFIX)
    take_windows_runtime(out)
    shutil.copy2(ROOT / "LICENSE", lic / "bt4decomp-agpl.txt")
    shutil.copy2(ROOT / "LICENSE-MIT", lic / "bt4decomp-mit.txt")
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    shutil.copy2(ROOT / "port/third_party/miniaudio/LICENSE", lic / "miniaudio.txt")
    shutil.copy2(ROOT / "port/third_party/miniz/LICENSE", lic / "miniz.txt")
    shutil.copy2(ROOT / "port/third_party/lua/LICENSE", lic / "lua.txt")  # (the script mods)
    shutil.copy2(ROOT / "port/third_party/monocypher/LICENCE.md", lic / "monocypher.txt")  # (the update's signature check)

    take_fonts(out, lic)
    take_tbo_art(out, lic)
    (out / "textures").mkdir()
    (out / "textures/README.txt").write_text(TEXTURES_NOTE.replace("\n", "\r\n"))
    for kind, note in (("stages", STAGES_NOTE), ("songs", SONGS_NOTE), ("mods", MODS_NOTE)):
        (out / kind).mkdir()
        (out / kind / "README.txt").write_text(note.replace("\n", "\r\n"))
    take_example_mods(out)
    (lic / "sdl3.txt").write_text("SDL3 (SDL3.dll) is distributed under the zlib license: https://www.libsdl.org/license.php\n")
    for f in (out / "BT4decomp.exe", out / "BT4decomp-setup.exe"):
        subprocess.run([toolchain.PREFIX + "strip", "--strip-debug", str(f)], check=False)
    # Keep existing desktop/setup entry points compatible with signed updates.
    shutil.copy2(out / "BT4decomp-setup.exe", out / "Install.exe")
    if zpath.exists():
        zpath.unlink()
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(out.rglob("*")):
            if p.is_file():
                z.write(p, "BT4decomp/" + str(p.relative_to(out)))
    print(f"{zpath.relative_to(ROOT)}: {zpath.stat().st_size / 1e6:.1f} MB")
    for p in sorted(out.rglob("*")):
        if p.is_file():
            print(f"  {p.stat().st_size:>10,}  {p.relative_to(out)}")

def main():
    exe = ROOT / "port/build/bt3_64"
    setup = ROOT / "BT4decomp-setup"
    for need in (exe, setup, pathlib.Path(str(exe) + ".map")):
        if not need.exists():
            sys.exit(f"missing {need.relative_to(ROOT)}: build first (see the top of this file)")
    if OUT.exists():
        shutil.rmtree(OUT)
    take_program(exe, OUT)
    take_lang(OUT)
    take_translations(OUT)
    shutil.copy2(ROOT / "port/release/BT4_VERSION.txt", OUT / "BT4_VERSION.txt")
    shutil.copy2(setup, OUT / "BT4decomp-setup")
    (OUT / "lib").mkdir()
    lib = next((p for d in ("/usr/local/lib", "/usr/lib", "/usr/lib64", "/usr/lib/x86_64-linux-gnu") for p in sorted(pathlib.Path(d).glob("libSDL3.so.0"))), None)
    if lib is None:
        sys.exit("libSDL3.so.0 not found")
    shutil.copy2(lib.resolve(), OUT / "lib/libSDL3.so.0")
    (OUT / "README.txt").write_text(README)
    lic = OUT / "licenses"
    lic.mkdir()
    shutil.copy2(ROOT / "LICENSE", lic / "bt4decomp-agpl.txt")
    shutil.copy2(ROOT / "LICENSE-MIT", lic / "bt4decomp-mit.txt")
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    shutil.copy2(ROOT / "port/third_party/miniaudio/LICENSE", lic / "miniaudio.txt")
    shutil.copy2(ROOT / "port/third_party/miniz/LICENSE", lic / "miniz.txt")
    shutil.copy2(ROOT / "port/third_party/lua/LICENSE", lic / "lua.txt")  # (the script mods)
    shutil.copy2(ROOT / "port/third_party/monocypher/LICENCE.md", lic / "monocypher.txt")  # (the update's signature check)

    take_fonts(OUT, lic)
    take_tbo_art(OUT, lic)
    (OUT / "textures").mkdir()
    (OUT / "textures/README.txt").write_text(TEXTURES_NOTE)
    for kind, note in (("stages", STAGES_NOTE), ("songs", SONGS_NOTE), ("mods", MODS_NOTE)):
        (OUT / kind).mkdir()
        (OUT / kind / "README.txt").write_text(note)
    take_example_mods(OUT)
    (lic / "sdl3.txt").write_text("SDL3 (lib/libSDL3.so.0) is distributed under the zlib license: https://www.libsdl.org/license.php\n")
    for f in (OUT / "BT4decomp", OUT / "BT4decomp-setup"):
        subprocess.run(["strip", "--strip-debug", str(f)], check=False)  # the symbol names stay (crash reports use them)
    if ZIP.exists():
        ZIP.unlink()
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(OUT.rglob("*")):
            if p.is_file():
                info = zipfile.ZipInfo.from_file(p, "BT4decomp/" + str(p.relative_to(OUT)))
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
    for f in ("BT4decomp", "BT4decomp-setup", "lib/libSDL3.so.0"):
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

if __name__ == "__main__":
    if os.environ.get("BT3_CC") == "win64":
        main_win()
    else:
        import package_native
        package_native.main(sys.modules[__name__], mac=os.environ.get("BT3_CC")=="mac64")
