#!/usr/bin/env python3
"""Links the PC build: every object from undefined.py, plus generated stubs for whatever is still missing.
A stub prints its name and stops the program, so running the result shows what the game needs next.
Usage: port/tools/undefined.py && port/tools/link.py     -> port/build/bt3"""
import os, pathlib, re, subprocess

import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from toolchain import ROOT, VARIANT, WIN, MAC, PREFIX, OBJ, DATA, EXE, sdl3

B = ROOT / "port/build"

# macOS: where the program starts (1 TB + 0x20000000). The game's addresses are offsets in the 4 GB window that begins
# 0x20000000 below it (port/src/port_addr.h), at 1 TB: on a 4 GB boundary (plat_mem.c, window_aligned). arm64
# programs are always position-independent, but each segment can be given an address (-segaddr; the system may then
# move the whole program, see window_aligned), and up there nothing else is placed: lower, macOS 26's malloc reserves
# its zones (64 GB and 256 GB failed every time, 24 GB a third of the time) and the shared libraries sit at 6 to 12 GB.
MAC_BASE = 0x10000000000 + 0x20000000

def mac_segments(exe):
    """[(segment, address, size)] of a linked program, from otool."""
    out, seg = [], None
    for l in subprocess.run(["otool", "-l", str(exe)], capture_output=True, text=True).stdout.splitlines():
        l = l.split()
        if l[:1] == ["segname"]:
            seg = l[1]
        elif l[:1] == ["vmaddr"] and seg:
            addr = int(l[1], 16)
        elif l[:1] == ["vmsize"] and seg:
            out.append((seg, addr, int(l[1], 16)))
            seg = None
    return out

def link_mac(objs, extra):
    """ld64 through clang: SDL3 from Homebrew, the C++ library for the settings window, entry point __wrap_main
    (plat_mem.c: what --wrap=main does elsewhere); --wrap=Progress_Main and --defsym are done by macwin.py."""
    sdl = os.environ.get("BT3_SDL3", "/opt/homebrew/opt/sdl3")
    cmd = ["clang", "-arch", "arm64", "-mmacosx-version-min=11.0", "-w", "-o", str(EXE)] + extra + objs + \
          [f"-L{sdl}/lib", "-lSDL3", "-lc++", f"-Wl,-rpath,{sdl}/lib", "-Wl,-rpath,@executable_path/lib",
           f"-L{os.environ.get('BT3_CURL', '/opt/homebrew/opt/curl')}/lib", "-lcurl", "-Wl,-headerpad_max_install_names", "-Wl,-e,___wrap_main", "-Wl,-export_dynamic", f"-Wl,-map,{EXE}.map"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        return r
    # again, with every segment moved up to MAC_BASE in the same order and spacing
    segs = [s for s in mac_segments(EXE) if s[0] != "__PAGEZERO"]
    first = min(a for _, a, _ in segs)
    moved = [f"-Wl,-segaddr,{s},{MAC_BASE + a - first:#x}" for s, a, _ in segs if s != "__LINKEDIT"]
    r = subprocess.run(cmd + moved, capture_output=True, text=True)
    if r.returncode == 0:
        mac_pagezero(EXE)
    return r

def mac_pagezero(exe):
    """A page zero of 4 GB in the program's header. Some linkers (Xcode 16's on GitHub's Mac) leave it 0 bytes when
    every segment is moved with -segaddr, -pagezero_size or not, and macOS on Apple Silicon kills such a program
    before it starts, without a word (found by josehs08). Nothing else lies below 4 GB here (the program is at 1 TB),
    so the header's vmsize is set and the program signed again (ad hoc)."""
    import struct
    data = bytearray(exe.read_bytes())
    magic, _, _, _, ncmds = struct.unpack_from("<IiiII", data, 0)
    if magic != 0xFEEDFACF:
        return
    off = 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", data, off)
        if cmd == 0x19 and data[off + 8:off + 24].rstrip(b"\0") == b"__PAGEZERO":  # LC_SEGMENT_64
            vmaddr, vmsize = struct.unpack_from("<QQ", data, off + 24)
            if vmaddr == 0 and vmsize < 0x100000000:
                struct.pack_into("<Q", data, off + 32, 0x100000000)
                exe.write_bytes(data)
                subprocess.run(["codesign", "--force", "--sign", "-", str(exe)], check=True, capture_output=True)
                print(f"__PAGEZERO {vmsize:#x} -> 4 GB")
            return
        off += size

def link(objs, extra):
    if MAC:
        return link_mac(objs, extra)
    common = ["-Wl,--defsym=D_3BE71C=0x3BE71C", "-Wl,--wrap=Progress_Main", f"-Wl,-Map={EXE}.map"]
    if WIN:
        # Keep the supplied game icon in every Windows build, including rebuilds.
        icon_dir = ROOT / "port/release"
        icon_obj = B / "game_icon.res.o"
        icon_inputs = [icon_dir / "game_icon.rc", icon_dir / "icon_game.ico", icon_dir / "app.manifest"]
        if not all(p.is_file() for p in icon_inputs):
            raise RuntimeError("The BT4 Windows icon resources are missing")
        if not icon_obj.exists() or any(p.stat().st_mtime_ns > icon_obj.stat().st_mtime_ns for p in icon_inputs):
            subprocess.run([PREFIX + "windres", "-O", "coff", "-i", "game_icon.rc", "-o", str(icon_obj)], cwd=icon_dir, check=True)
        objs = [*objs, str(icon_obj)]
        # Windows: the program at a fixed address below 4 GB (no relocation: the game's 4-byte pointers and the
        # 32-bit addresses in its data tables depend on it), the game's main() wrapped for a low stack (plat_mem.c),
        # the compiler's own libraries linked in, SDL3 as SDL3.dll next to the program.
        lib = [f"-L{sdl3() / 'lib'}"] if sdl3() else []
        cmd = [PREFIX + "gcc", "-w", "-o", str(EXE)] + extra + objs + lib + \
              ["-lSDL3", "-lm", "-lwinmm", "-lws2_32", "-lwinhttp", "-ladvapi32", "-static-libgcc", "-Wl,-Bstatic", "-lstdc++", "-lwinpthread", "-Wl,-Bdynamic",
               "-Wl,--image-base,0x20000000", "-Wl,--disable-dynamicbase", "-Wl,--disable-reloc-section", "-Wl,--disable-high-entropy-va", "-Wl,--wrap=main",
               "-Wl,--stack,0x1000000", "-mwindows"] + common
        argsfile = B / "link.rsp"
        argsfile.write_text("\n".join('"' + a.replace('\\', '/').replace('"', '\\"') + '"' for a in cmd[1:]))
        return subprocess.run([cmd[0], "@" + str(argsfile)], capture_output=True, text=True)
    m = "-m64" if VARIANT == "clang64" else "-m32"
    # 64-bit: the program below 4 GB and clear of the PS2 regions (plat_mem.c); the game's main() on a low stack
    base64 = ["-Wl,-Ttext-segment=0x20000000", "-Wl,--wrap=main"] if VARIANT == "clang64" else []
    # -rpath $ORIGIN/lib: a release folder carries its own SDL3 in lib/
    return subprocess.run(["gcc", m, "-no-pie", "-w", "-o", str(EXE)] + extra + objs +
                          ["-static-libstdc++", "-static-libgcc", "-Wl,-Bstatic", "-lstdc++", "-Wl,-Bdynamic", "-lm", "-lpthread", "-lSDL3", "-lcurl", "-rdynamic",
                           "-Wl,-rpath,$ORIGIN/lib"] + common + base64, capture_output=True, text=True)

def main():
    objs = [str(o) for o in sorted(OBJ.glob("*.o")) + sorted(DATA.glob("*.o"))]
    r = link(objs, [])  # first pass: ask the linker what nobody provides (the host C library included)
    need = sorted(set(re.findall(r'^\s+"_(\w+)", referenced from', r.stderr, re.M) if MAC else  # (ld64; GNU ld)
                      re.findall(r"undefined reference to `(\w+)'", r.stderr)))
    missing_menu = [name for name in need if name.startswith("Tb_")]
    if missing_menu:
        raise SystemExit("Incomplete online menu integration: " + ", ".join(missing_menu))
    lines = ['/* Generated by port/tools/link.py: one stub per symbol the PC build does not provide yet. */',
             'extern int printf(const char *, ...);', 'extern void exit(int);',
             'static void missing(const char *n) { printf("bt3: not implemented yet: %s\\n", n); exit(3); }']
    lines += [f'void {s}(void) {{ missing("{s}"); }}' for s in need]
    (B / "gen").mkdir(exist_ok=True)
    (B / "gen/stubs.c").write_text("\n".join(lines) + "\n")
    (B / "stubs.txt").write_text("\n".join(need) + "\n")
    r = link(objs, [str(B / "gen/stubs.c")])
    errs = [l for l in r.stderr.splitlines() if "multiple definition" in l or "undefined reference" in l or "rror" in l or (MAC and "duplicate symbol" in l)]
    print(f"{len(need)} stubs; link {'OK -> ' + str(EXE.relative_to(ROOT)) if r.returncode == 0 else 'FAILED'}; {len(errs)} error lines")
    for l in errs[:12]:
        print("  " + l[-160:])
    if r.returncode == 0:
        # where the game's own variables lie in this program (for the state checksum; port/src/gs/state.c)
        d = subprocess.run([sys.executable, str(ROOT / "port/tools/make_state.py")], capture_output=True, text=True)
        if d.returncode:
            print((d.stdout + d.stderr).strip()[-300:])
            sys.exit(1)
    if r.returncode == 0 and (DATA / "SKELETON").exists():
        # built from the blank data tables: the program needs the list of where their values are on the user's disc
        d = subprocess.run([sys.executable, str(ROOT / "port/tools/make_dat.py")], capture_output=True, text=True)
        print((d.stdout + d.stderr).strip()[-300:])
        if d.returncode:
            sys.exit(1)
    if r.returncode:
        sys.exit(1)

main()
