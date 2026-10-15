#!/usr/bin/env python3
"""Setup wizard of the PC port: from a fresh copy of this repository and YOUR OWN disc image to a playable build.

    python3 install.py                       asks for the disc image
    python3 install.py --iso <disc.iso>      no questions
    python3 install.py --check               only checks what is installed

The repository contains no game data. Everything the game needs (models, sounds, text, and the data tables of the
executable itself) is taken from your disc image of the USA release (SLUS-21678) and stays on your machine.

Steps (each is skipped when its result is already there; --force redoes them):
  1. requirements       compilers and tools, with what to install when one is missing
  2. disc image         found, read, and checked against the known checksum of the release
  3. game data          unpacked into gamedata/ as loose files
  4. executable data    the data tables of the game's two programs, split out for the build
  5. build              the port itself (64-bit; --32 for the 32-bit build)
  6. self-test          the game's own demo fight, run without a window
(The list of menu labels of the "Menu font" setting was a step here. The game makes it itself now, the first time
it starts with a window: port/src/gs/menu_labels.c, docs/port/menu_font.md.)
Everything the tools print goes to install.log.
Standard library only; Linux, and macOS on Apple Silicon (experimental: the native arm64 build, BT3_CC=mac64,
docs/port/macos_plan.md)."""
import argparse, hashlib, os, pathlib, platform, shutil, struct, subprocess, sys, threading, time

ROOT = pathlib.Path(__file__).resolve().parent
LOG = ROOT / "install.log"
VENV = ROOT / ".venv"
ROM_SHA1 = "caee6c2269bba89fc51eea9e7beac3adbec9dc52"   # SLUS_216.78 as a flat image (config/SLUS_216.78.yaml)
DBZP_SHA1 = "4f910969e05d9b25c83af7642b60da9949a4348b"  # BIN/DBZP.BIN (config/DBZP.yaml)
ROM_BASE, ROM_END = 0x100000, 0x2FF180
SPLAT = "splat64[mips]==0.50.0"
MAC = sys.platform == "darwin"
LIBCLANG = "libclang==18.1.1"  # (macOS: the python bindings of clang for ptr32.py, in .venv; Linux uses the system's)
LLVM = pathlib.Path(os.environ.get("BT3_LLVM", "/opt/homebrew/opt/llvm/bin"))  # (macOS: Homebrew's LLVM, as toolchain.py)

TTY = sys.stdout.isatty()
MACHINE = False  # --machine: one line per event for the setup window (port/setup/setup.cpp), see say()


def say(event, text=""):
    """A line of the protocol the setup window reads: `@step 3 6 Game data`, `@note ...`, `@ok ...`, `@skip ...`,
    `@fail`, `@missing name|purpose|package`, `@stopped message`, `@done launcher`. Line breaks inside a text are
    sent as the two characters backslash, n."""
    print(f"@{event} {text}".rstrip().replace("\n", "\\n"), flush=True)
def paint(code, s):
    return f"\033[{code}m{s}\033[0m" if TTY else s
OK, BAD, DIM, BOLD = (lambda s: paint("32", s)), (lambda s: paint("31", s)), (lambda s: paint("2", s)), (lambda s: paint("1", s))


class Failed(Exception):
    """A step could not be completed; the message says what to do."""


def log(text):
    with open(LOG, "a") as f:
        f.write(text if text.endswith("\n") else text + "\n")


def run(cmd, env=None, cwd=ROOT):
    """Runs a tool quietly; its output goes to install.log. Returns (exit code, output)."""
    log(f"\n$ {' '.join(str(c) for c in cmd)}")
    r = subprocess.run([str(c) for c in cmd], cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                       errors="replace")
    log(r.stdout)
    return r.returncode, r.stdout


class Step:
    """One line of the wizard: `[3/6] Game data ... done (41 s)` with a running clock while it works."""
    count, total = 0, 6

    def __init__(self, title):
        Step.count += 1
        self.head = f"{DIM(f'[{Step.count}/{Step.total}]')} {title}"
        self.t0 = time.time()
        self._note = ""
        self.done = threading.Event()
        log(f"\n===== {title} =====")
        if MACHINE:
            say("step", f"{Step.count} {Step.total} {title}")
        elif TTY:
            self.thread = threading.Thread(target=self.tick, daemon=True)
            self.thread.start()
        else:
            print(f"[{Step.count}/{Step.total}] {title} ...", flush=True)

    @property
    def note(self):
        return self._note

    @note.setter
    def note(self, text):
        self._note = text
        if MACHINE:
            say("note", text)

    def tick(self):
        while not self.done.wait(0.5):
            extra = f"  {self.note}" if self.note else ""
            sys.stdout.write(f"\r\033[K{self.head} {DIM(f'... {int(time.time() - self.t0)} s{extra}')}")
            sys.stdout.flush()

    def end(self, mark, text, event="ok", plain=None):
        self.done.set()
        if MACHINE:
            say(event, text if plain is None else plain)
            return
        if TTY:
            self.thread.join()
            sys.stdout.write("\r\033[K")
        print(f"{self.head}  {mark} {text}" if TTY else f"    {text}", flush=True)

    def ok(self, text="done"):
        secs = int(time.time() - self.t0)
        self.end(OK("ok"), text + (DIM(f"  ({secs} s)") if secs >= 2 else ""), "ok", text)

    def skip(self, text):
        self.end(OK("ok"), DIM(text), "skip", text)

    def fail(self, text):
        self.end(BAD("failed"), "", "fail")
        raise Failed(text)


# ------------------------------------------------------------------------------------------------ 1. requirements

def works(cmd, stdin=None):
    try:
        return subprocess.run(cmd, input=stdin, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True, timeout=60).returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def requirements_mac():
    """macOS: Apple's command line tools (the renderer, the link), Homebrew's LLVM (the game code: Apple's clang has no
    4-byte pointers on arm64), SDL3 and the shader tools from Homebrew. The disc is read with the system's bsdtar."""
    have = lambda tool: shutil.which(tool) is not None
    sdl = pathlib.Path(os.environ.get("BT3_SDL3", "/opt/homebrew/opt/sdl3"), "lib", "libSDL3.dylib").exists()
    return [
        ("Apple Silicon", platform.machine() == "arm64", "the macOS build is native arm64 only", "a Mac with an M-series processor"),
        ("Command Line Tools", works(["xcrun", "--show-sdk-path"]) and have("clang"), "compiles the renderer and links the game", "xcode-select --install"),
        ("LLVM (Homebrew)", all((LLVM / t).exists() for t in ("clang", "llc", "opt", "llvm-config")), "compiles the game code", "brew install llvm"),
        ("SDL3", sdl, "window, input and sound", "brew install sdl3"),
        ("glslc", have("glslc"), "compiles the shaders", "brew install shaderc"),
        ("spirv-cross", have("spirv-cross"), "translates the shaders for Metal", "brew install spirv-cross"),
        ("disc reader", have("7z") or have("bsdtar"), "reads the disc image", "brew install p7zip"),
        ("python venv", works([sys.executable, "-c", "import venv, ensurepip"]), "holds the tools that split the executable", "python3 from python.org or brew install python"),
    ]


def requirements(want32):
    """[(name, present, what it is for, how to get it)] and the build variant that can be made."""
    if MAC:
        return requirements_mac()
    hello = "int main(void){return 0;}\n"
    have = lambda tool: shutil.which(tool) is not None
    sdl = works(["pkg-config", "--exists", "sdl3"]) or any(pathlib.Path(p, "libSDL3.so").exists() for p in ("/usr/lib", "/usr/lib64", "/usr/lib/x86_64-linux-gnu"))
    clang_py = works([sys.executable, "-c", "import clang.cindex as c; c.Index.create()"])
    rows = [
        ("7z", have("7z"), "reads the disc image", "p7zip / 7zip"),
        ("gcc, g++", have("gcc") and have("g++") and works(["gcc", "-x", "c", "-", "-o", os.devnull], hello), "compiles the renderer and links the game", "gcc"),
        ("as", have("as"), "assembles the game's data tables", "binutils"),
        ("glslc", have("glslc"), "compiles the shaders", "shaderc"),
        ("SDL3", sdl, "window, input and sound", "sdl3"),
        ("python venv", works([sys.executable, "-c", "import venv, ensurepip"]), "holds the tool that splits the executable", "python3-venv"),
    ]
    if want32:
        rows.append(("gcc -m32", works(["gcc", "-m32", "-x", "c", "-", "-o", os.devnull], hello), "the 32-bit build", "gcc-multilib and the 32-bit SDL3 (lib32-sdl3)"))
    else:
        rows += [
            ("clang", have("clang"), "compiles the game code (64-bit)", "clang"),
            ("llc", have("llc"), "code generation for the game code", "llvm"),
            ("python clang module", clang_py, "rewrites the game's pointers for 64-bit", "python bindings of clang (python-clang, or `pip install libclang`)"),
        ]
    return rows


def step_requirements(args):
    s = Step("Requirements")
    rows = requirements(args.bits32)
    missing = [r for r in rows if not r[1] and "optional" not in r[0]]
    for name, present, why, pkg in rows:
        log(f"{'ok     ' if present else 'MISSING'} {name}: {why} ({pkg})")
    if missing:
        s.end(BAD("missing"), ", ".join(r[0] for r in missing), "fail")
        if not MACHINE:
            print()
        for name, _, why, pkg in missing:
            if MACHINE:
                say("missing", f"{name}|{why}|{pkg}")
            else:
                print(f"    {BOLD(name):<32} {why}\n    {'':<24} install: {pkg}")
        raise Failed("Install what is listed above " + ("(Homebrew: https://brew.sh)" if MAC else "with your system's package manager") + " and run this again.")
    optional = [r[0] for r in rows if not r[1]]
    s.ok("all present" + (f"  (not found: {', '.join(optional)})" if optional else ""))


# ---------------------------------------------------------------------------------------------- 2. the disc image

def find_iso(args):
    if args.iso:
        p = pathlib.Path(args.iso).expanduser()
        if not p.is_file():
            raise Failed(f"No such file: {p}")
        return p
    here = sorted(ROOT.glob("*.iso")) + sorted(ROOT.parent.glob("*.iso"))
    if len(here) == 1:
        return here[0]
    if not sys.stdin.isatty():
        raise Failed("Give the disc image with --iso <file>.")
    print(f"\n    The port needs your own disc image of {BOLD('Dragon Ball Z: Budokai Tenkaichi 3')} (USA, SLUS-21678), as an .iso file.")
    while True:
        try:
            line = input("    Path to the disc image (you can drag the file here): ").strip().strip("'\"")
        except EOFError:
            raise Failed("No disc image given.")
        p = pathlib.Path(line).expanduser()
        if line and p.is_file():
            print()
            return p
        print(f"    {BAD('not a file:')} {line or '(nothing)'}")


def flat_image(elf):
    """The executable's loaded sections as one image from 0x100000 (what `objcopy -O binary --pad-to` writes)."""
    b = elf.read_bytes()
    if b[:4] != b"\x7fELF":
        raise Failed("SLUS_216.78 on this disc is not a PS2 executable.")
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", b, 0x2E)
    out = bytearray(ROM_END - ROM_BASE)
    for i in range(shnum):
        _, kind, flags, addr, off, size = struct.unpack_from("<6I", b, shoff + i * shentsize)
        if flags & 2 and kind != 8 and size and ROM_BASE <= addr and addr + size <= ROM_END:
            out[addr - ROM_BASE:addr - ROM_BASE + size] = b[off:off + size]
    return bytes(out)


def step_disc(args):
    """The two programs of the game, checked: disc/SLUS_216.78.rom and disc/BIN/DBZP.BIN."""
    disc = ROOT / "disc"
    rom, dbzp = disc / "SLUS_216.78.rom", disc / "BIN" / "DBZP.BIN"
    def good():
        return rom.is_file() and dbzp.is_file() and hashlib.sha1(rom.read_bytes()).hexdigest() == ROM_SHA1 and \
            hashlib.sha1(dbzp.read_bytes()).hexdigest() == DBZP_SHA1
    if not args.force and good():
        Step("Disc image").skip("already read and checked")
        return None
    iso = find_iso(args)
    s = Step("Disc image")
    bsdtar = MAC and shutil.which("7z") is None and shutil.which("bsdtar") is not None  # (macOS only: the system's own reader)
    code, listing = run(["bsdtar", "-tf", iso] if bsdtar else ["7z", "l", iso])
    if code != 0 or "SLUS_216.78" not in listing:
        s.fail(f"{iso.name} is not a disc image of the USA release (no SLUS_216.78 on it).\n"
               "    Other regions and the Wii version have different programs and are not supported.")
    if bsdtar:
        disc.mkdir(exist_ok=True)  # (7z makes its output folder itself)
    code, _ = run(["bsdtar", "-xf", iso, "-C", disc, "SLUS_216.78", "BIN"] if bsdtar else ["7z", "x", "-y", f"-o{disc}", iso, "SLUS_216.78", "BIN"])
    if code != 0 or not (disc / "SLUS_216.78").is_file() or not dbzp.is_file():
        s.fail("The disc image could not be read (see install.log).")
    rom.write_bytes(flat_image(disc / "SLUS_216.78"))
    if not good():
        s.fail("The game's programs on this disc do not have the expected checksums.\n"
               "    The port is built from the unmodified USA release (SLUS-21678); a patched or damaged image will not work.")
    s.ok("the USA release, checksums match")
    return iso


# ------------------------------------------------------------------------------------------------- 3. game data

def step_gamedata(args, iso):
    data = ROOT / "gamedata"
    have = all((data / d).is_dir() and any((data / d).iterdir()) for d in ("pzs3us0", "pzs3us1", "pzs3us2")) and (data / "disc" / "DATA").is_dir()
    if not args.force and have:
        Step("Game data").skip("already unpacked")
        return
    if iso is None:
        iso = find_iso(args)
    s = Step("Game data")
    code, out = run([sys.executable, ROOT / "port/tools/extract_disc.py", iso, data])
    if code != 0:
        s.fail("Unpacking the disc failed (see install.log). Is there enough free space? About 3.5 GB are needed.")
    files = sum(1 for _ in data.rglob("*") if _.is_file())
    s.ok(f"{files:,} files in gamedata/")


# -------------------------------------------------------------------------------------------- 4. executable data

def venv_python():
    return VENV / "bin" / "python"


def step_split(args):
    asm = ROOT / "asm"
    have = (asm / "data").is_dir() and (asm / "dbzp").is_dir() and any((asm / "data").iterdir())
    if not args.force and have:
        Step("Executable data").skip("already split")
        return
    s = Step("Executable data")
    py = venv_python()
    if not py.exists():
        s.note = "making the tool environment"
        code, _ = run([sys.executable, "-m", "venv", "--system-site-packages", VENV])
        if code != 0:
            s.fail("Could not create the Python environment .venv (see install.log).")
    if run([py, "-c", "import splat"])[0] != 0:
        s.note = "downloading the splitting tool"
        code, _ = run([py, "-m", "pip", "install", "--quiet", "--disable-pip-version-check", SPLAT])
        if code != 0:
            s.fail(f"Could not install {SPLAT} (see install.log). This step needs an internet connection once.")
    if MAC and run([py, "-c", "import clang.cindex as c; c.Index.create()"])[0] != 0:
        s.note = "downloading the python bindings of clang"
        code, _ = run([py, "-m", "pip", "install", "--quiet", "--disable-pip-version-check", LIBCLANG])
        if code != 0:
            s.fail(f"Could not install {LIBCLANG} (see install.log). This step needs an internet connection once.")
    for name in ("SLUS_216.78", "DBZP"):
        s.note = name
        code, _ = run([py, "-m", "splat", "split", ROOT / "config" / f"{name}.yaml"])
        if code != 0:
            s.fail(f"Splitting {name} failed (see install.log).")
    if not (asm / "data").is_dir():
        s.fail("The splitting tool ran but wrote no data (see install.log).")
    s.ok()


# ------------------------------------------------------------------------------------------------------ 5. build

def step_build(args):
    exe = ROOT / "port/build" / ("bt3_mac" if MAC else "bt3" if args.bits32 else "bt3_64")
    if not args.force and exe.is_file() and not args.rebuild:
        Step("Build").skip("already built" if MACHINE else f"{exe.relative_to(ROOT)} is there (--rebuild to build again)")
        return exe
    s = Step("Build")
    py = venv_python() if venv_python().exists() else pathlib.Path(sys.executable)
    env = dict(os.environ)
    if MAC:
        env["BT3_CC"] = "mac64"
    elif not args.bits32:
        env["BT3_CC"] = "clang64"
    s.note = "data tables"
    code, out = run([py, ROOT / "port/tools/gen_data.py", "--asm", ROOT / "asm"], env)
    if code != 0:
        s.fail("Preparing the game's data tables failed (see install.log).")
    s.note = "compiling (a few minutes)"
    code, out = run([py, ROOT / "port/tools/undefined.py"], env)
    failed = [l for l in out.splitlines() if l.startswith("FAILED")]
    if code != 0 or failed:
        s.fail("Compiling failed" + (f" in {len(failed)} files, first: {failed[0][7:120]}" if failed else "") + " (see install.log).")
    s.note = "linking"
    code, out = run([py, ROOT / "port/tools/link.py"], env)
    if code != 0 or "link OK" not in out or not exe.is_file():
        s.fail("Linking failed (see install.log).")
    s.ok(f"{exe.relative_to(ROOT)}, {exe.stat().st_size / 1e6:.0f} MB")
    return exe


# -------------------------------------------------------------------------------------------------- 6. self-test

def step_test(args, exe):
    if args.skip_test:
        Step("Self-test").skip("skipped")
        return
    s = Step("Self-test")
    s.note = "the game's demo fight, no window"
    env = dict(os.environ, BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(ROOT / "port/build/selftest_saves"), BT3_GS="none")
    for k in ("BT3_REPLAY", "BT3_PAD_PLAY", "BT3_PAD_REC"):
        env.pop(k, None)
    log(f"\n$ BT3_DEMO=1 {exe}")
    try:
        r = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", timeout=300)
        out, code = r.stdout, r.returncode
    except subprocess.TimeoutExpired as e:
        out, code = (e.stdout or b"").decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or ""), -1
    log(out)
    if "battle finished" not in out:
        last = next((l for l in reversed(out.splitlines()) if l.strip()), "no output")
        s.fail(f"The demo fight did not finish (exit code {code}). Last line: {last[:140]}\n    The full output is in install.log.")
    s.ok("the demo fight ran to its end")


def write_launcher(exe, bits32):
    play = ROOT / "play.sh"
    if MAC:
        play.write_text("#!/bin/sh\n# Starts the game from its menus. Written by install.py.\n"
                        'cd "$(dirname "$0")" && unset BT3_REPLAY BT3_DEMO && BT3_GS=gpu exec port/build/bt3_mac\n')
        play.chmod(0o755)
        return play
    play.write_text("#!/bin/sh\n# Starts the game from its menus. Written by install.py.\n"
                    f'cd "$(dirname "$0")" && {"" if bits32 else "BT3_64=1 "}exec port/run.sh menu\n')
    play.chmod(0o755)
    return play


def main():
    ap = argparse.ArgumentParser(description="Setup wizard of the Budokai Tenkaichi 3 PC port.", epilog="Run without arguments to be asked for the disc image.")
    ap.add_argument("--iso", help="your disc image of the USA release (SLUS-21678)")
    ap.add_argument("--32", dest="bits32", action="store_true", help="make the 32-bit build instead of the 64-bit one")
    ap.add_argument("--check", action="store_true", help="only check the requirements")
    ap.add_argument("--rebuild", action="store_true", help="build again even if the program is there")
    ap.add_argument("--force", action="store_true", help="redo every step")
    ap.add_argument("--skip-test", action="store_true", help="do not run the demo fight at the end")
    ap.add_argument("--machine", action="store_true", help=argparse.SUPPRESS)  # for the setup window
    args = ap.parse_args()
    global MACHINE, TTY
    MACHINE = args.machine
    TTY = TTY and not MACHINE
    if sys.platform not in ("linux", "darwin"):
        print("This wizard supports Linux and macOS (Apple Silicon).")
        return 2
    LOG.write_text(f"install.py, {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
    if not MACHINE:
        print(f"\n  {BOLD('Budokai Tenkaichi 3 PC port')}  {DIM('setup')}\n")
    if args.check:
        Step.total = 1
    t0 = time.time()
    try:
        step_requirements(args)
        if args.check:
            print()
            return 0
        iso = step_disc(args)
        step_gamedata(args, iso)
        step_split(args)
        exe = step_build(args)
        step_test(args, exe)
    except Failed as e:
        if MACHINE:
            say("stopped", str(e))
            return 1
        print(f"\n  {BAD('Setup stopped.')} {e}\n  {DIM('Details: install.log. Finished steps are kept; run this again to continue.')}\n")
        return 1
    except KeyboardInterrupt:
        print(f"\n\n  {DIM('Interrupted. Finished steps are kept; run this again to continue.')}\n")
        return 130
    play = write_launcher(exe, args.bits32)
    if MACHINE:
        say("done", str(play))
        return 0
    mins = (time.time() - t0) / 60
    print(f"\n  {OK('Ready.')}" + (DIM(f"  ({mins:.0f} min)") if mins >= 1 else ""))
    print(f"\n    Play:      {BOLD('./' + play.name)}")
    print("    Settings:  F1 in the game (resolution, widescreen, controls, sound)")
    print("    Saves:     saves/      Mods: gamedata/mods/<same path as the original file>\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
