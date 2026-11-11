"""The macOS build's step between clang and llc for the game's code (undefined.py): applies the window memory model
(port/tools/macwin/macwin.cpp, an LLVM pass plugin built here on first use with Homebrew's LLVM) to an .ll file in
place. See the top of macwin.cpp."""
import os, pathlib, re, subprocess, threading
from toolchain import ROOT, llvm

SRC = ROOT / "port/tools/macwin/macwin.cpp"
PLUGIN = ROOT / "port/build/macwin.dylib"
# clang writes a plain bitcast between a 4-byte and an ordinary pointer in a conditional expression that mixes the
# two, which the IR reader rejects (irfix.py does the same fix for the Linux and Windows builds)
BITCAST = re.compile(r'^(\s*%[\w.$-]+ = )bitcast (ptr(?: addrspace\(27[12]\))? \S+) to (ptr(?: addrspace\(27[12]\))?)(?=,|$)', re.M)

_plugin_lock = threading.Lock()

def plugin():
    # Compiler workers share this plugin. Build it once and publish it atomically
    # so another worker cannot load a missing or partially linked dylib.
    with _plugin_lock:
        PLUGIN.parent.mkdir(parents=True, exist_ok=True)
        if os.name == "posix":
            import fcntl
            with PLUGIN.with_suffix(".lock").open("a+b") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                try:
                    return _plugin_locked()
                finally:
                    fcntl.flock(lock, fcntl.LOCK_UN)
        return _plugin_locked()

def _plugin_locked():
    if PLUGIN.exists() and PLUGIN.stat().st_mtime >= SRC.stat().st_mtime:
        return PLUGIN
    flags = subprocess.run([llvm("llvm-config"), "--cxxflags", "--ldflags"], capture_output=True, text=True, check=True).stdout.split()
    PLUGIN.parent.mkdir(parents=True, exist_ok=True)
    temporary = PLUGIN.with_suffix(".tmp.dylib")
    try:
        subprocess.run([llvm("clang++"), "-shared", "-fPIC", "-O2", "-isysroot", subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True).stdout.strip(),
                        "-undefined", "dynamic_lookup", str(SRC), "-o", str(temporary)] + flags, check=True)
        os.replace(temporary, PLUGIN)
    finally:
        temporary.unlink(missing_ok=True)
    return PLUGIN

# GNU ld's --wrap and --defsym of link.py's `common` flags, applied by the pass (ld64 has neither)
WRAP = ["Progress_Main"]
ABS = {"D_3BE71C": 0x3BE71C}

SIGS = ROOT / "port/build/mac_sigs.txt"  # every function the program defines, with its kinds (the top of macwin.cpp)

def opt(ll, passes, opts):
    return subprocess.run([llvm("opt"), "-load-pass-plugin", str(plugin()), "-passes=" + passes] + opts, capture_output=True, text=True)

def defs(ll):
    """First step, for every module: the functions it defines, into ll.defs (made into SIGS by sigs())."""
    text = ll.read_text()
    fixed = BITCAST.sub(r"\1addrspacecast \2 to \3", text)
    if fixed != text:
        ll.write_text(fixed)
    return opt(ll, "bt3-window", ["-bt3-wrap=" + ",".join(WRAP), "-bt3-defs=" + str(ll.with_suffix(".defs")), "-disable-output", str(ll)])

def sigs(lls):
    SIGS.write_text("".join(pathlib.Path(ll).with_suffix(".defs").read_text() for ll in lls))

def run(ll, port=False):
    """Second step, once SIGS lists the whole program: the pass, then LLVM's optimisations, on ll in place."""
    opts = ["-bt3-wrap=" + ",".join(WRAP), "-bt3-abs=" + ",".join(f"{k}={v:#x}" for k, v in ABS.items()), "-bt3-sigs=" + str(SIGS)] + (["-bt3-port"] if port else [])
    if os.environ.get("BT3_MACWIN_REPORT"):  # (bring-up: list host pointers made into integers)
        opts.append("-bt3-report")
    r = opt(ll, "bt3-window,default<O2>", opts + ["-S", str(ll), "-o", str(ll)])
    if os.environ.get("BT3_MACWIN_REPORT"):
        with open(ROOT / "port/build/macwin_report.txt", "a") as f:
            f.write(r.stderr)
    return r
