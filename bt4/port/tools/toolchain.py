"""Which build this is and which tools it uses; shared by undefined.py, link.py and strip_data.py.

    BT3_CC=gcc       (default) 32-bit, gcc                      -> port/build/bt3
    BT3_CC=clang     32-bit, the game code through clang        -> port/build/bt3_clang
    BT3_CC=clang64   64-bit Linux                               -> port/build/bt3_64
    BT3_CC=win64     64-bit Windows                             -> port/build/bt3.exe
                     On Linux: cross-compiled (mingw-w64: x86_64-w64-mingw32-gcc, clang with the mingw target; SDL3's
                     mingw development package unpacked under port/build/win or named by BT3_SDL3).
                     On Windows: in the MSYS2 CLANG64 or UCRT64/MINGW64 shell, with that shell's own tools.
    BT3_CC=mac64     64-bit macOS on Apple Silicon (arm64)      -> port/build/bt3_mac
                     Homebrew's LLVM (`brew install llvm`; BT3_LLVM names another bin/ folder): Apple's clang drops
                     the 4-byte pointers on arm64. The game's addresses are offsets in a 4 GB window instead of
                     low addresses (macOS keeps the low 4 GB unmapped): see docs/port/macos_plan.md."""
import os, pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
VARIANT = os.environ.get("BT3_CC", "gcc")
WIN = VARIANT == "win64"
MAC = VARIANT == "mac64"
BITS64 = VARIANT in ("clang64", "win64", "mac64")
ON_WINDOWS = os.name == "nt" or sys.platform in ("msys", "cygwin")
PREFIX = "x86_64-w64-mingw32-" if WIN and not ON_WINDOWS else ""
MAC_TRIPLE = "arm64-apple-macos11"
LLVM = pathlib.Path(os.environ.get("BT3_LLVM", "/opt/homebrew/opt/llvm/bin")) if MAC else None

def llvm(tool):
    """An LLVM tool by name: Homebrew's for the macOS build, the one on PATH otherwise."""
    return str(LLVM / tool) if MAC else tool

def mac_sdk():
    return subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True).stdout.strip()

CLANG_TARGET = ["--target=x86_64-w64-mingw32"] if WIN else [f"--target={MAC_TRIPLE}", "-isysroot", mac_sdk()] if MAC else []
OBJ = ROOT / ("port/build/obj" if VARIANT == "gcc" else "port/build/obj_" + VARIANT)
DATA = ROOT / ("port/build/obj_data" if not BITS64 else "port/build/obj_data_win64" if WIN else
               "port/build/obj_data_mac64" if MAC else "port/build/obj_data64")
EXE = ROOT / "port/build" / {"gcc": "bt3", "clang64": "bt3_64", "win64": "bt3.exe", "mac64": "bt3_mac"}.get(VARIANT, "bt3_" + VARIANT)

def sdl3():
    """The folder with SDL3's include/ and lib/ for the Windows build, or None when the compiler finds it itself."""
    if not WIN:
        return None
    if os.environ.get("BT3_SDL3"):
        return pathlib.Path(os.environ["BT3_SDL3"])
    found = sorted((ROOT / "port/build/win").glob("SDL3-*/x86_64-w64-mingw32"))
    return found[-1] if found else None
