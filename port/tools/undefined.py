#!/usr/bin/env python3
"""Compile the game sources to 32-bit host objects and list what a link would still need, by kind.
Usage: port/tools/undefined.py [-v]   (objects go to port/build/obj)"""
import collections, concurrent.futures, os, pathlib, re, shutil, subprocess, sys
import portsrc
import eeconst
import buildkey

ROOT = pathlib.Path(__file__).resolve().parents[2]
# The build variants (BT3_CC): see toolchain.py.
from toolchain import VARIANT, WIN, MAC, BITS64, PREFIX, CLANG_TARGET, OBJ, DATA, sdl3, llvm

# macOS / arm64: x86 code-generation flags have no meaning there. Software float is not a front-end option on arm64:
# the IR is made as usual and the code generator is told there is no floating-point unit (llc -mattr, below), which
# turns every float operation into the same calls (__addsf3 ...) and passes floats in integer registers, as
# -msoft-float does on x86. No vectorizing, as with -mno-sse on x86: the IR keeps one float operation per source one.
MAC_DROP = {"-m32", "-m64", "-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double", "-fno-pic", "-msse2", "-mfpmath=sse", "-mno-x87"}
MAC_FLAGS = ["-fno-vectorize", "-fno-slp-vectorize", "-Wno-error=return-mismatch", "-Wno-error=incompatible-function-pointer-types",
             "-Wno-error=int-conversion", "-Wno-error=incompatible-pointer-types", "-Wno-error=implicit-function-declaration", "-Wno-error=implicit-int"]
MAC_IR = ["-Xclang", "-disable-llvm-passes"]  # the window pass sees the code as written ((u32)p is still a ptrtoint and a
# trunc, not folded into a shift or a mask); macwin.run optimises after it
MAC_LLC = ["-mtriple=arm64-apple-macos11", "-mattr=-fp-armv8,-neon", "-relocation-model=pic"]

def variant(cmd):
    """The command for this build variant: clang for everything built with software float, 64-bit or Windows flags."""
    if MAC:
        soft = "-msoft-float" in cmd
        cc = llvm("clang++" if cmd[0] == "g++" else "clang") if soft else ("clang++" if cmd[0] == "g++" else "clang")
        target = CLANG_TARGET if soft else ["-arch", "arm64", "-mmacosx-version-min=11.0", f"-I{os.environ.get('BT3_SDL3', '/opt/homebrew/opt/sdl3')}/include"]
        return [cc] + target + [a for a in cmd[1:] if a not in MAC_DROP] + (["-fms-extensions"] + MAC_FLAGS if soft else [])
    if BITS64:
        cmd = ["-m64" if a == "-m32" else a for a in cmd if a != "-malign-double"]
    if WIN:
        cmd = [a for a in cmd if a != "-m64"]
        inc = [f"-I{sdl3() / 'include'}"] if sdl3() else []
        if cmd[0] in ("gcc", "g++") and "-msoft-float" not in cmd:  # the renderer, the settings window: the Windows gcc
            return [PREFIX + cmd[0]] + cmd[1:] + inc
    if VARIANT == "gcc" or "-msoft-float" not in cmd:
        return cmd
    extra = ["-fms-extensions"] if BITS64 else []
    if WIN:  # GCC's structure layout, not Microsoft's bit-field rules: the game's structures are the PS2's
        extra += ["-mno-ms-bitfields", "-D__CRT__NO_INLINE"]  # (no inline maths of the headers: they use long double) + ([f"-I{sdl3() / 'include'}"] if sdl3() else [])
    return ["clang"] + CLANG_TARGET + cmd[1:] + ["-mno-x87", "-Wno-error=return-mismatch"] + extra

def run(cmd, **kw):
    if MAC and "-msoft-float" in cmd:  # software float on arm64 is a code-generator option: through IR and llc
        return mac_soft(variant(cmd), **kw)
    return subprocess.run(variant(cmd), **kw)

def mac_soft(cmd, **kw):
    """`clang ... src -o obj` for the macOS build's software-float code that is not the game's (the vector-library
    references, src/port): IR, the window pass (port mode: no 4-byte pointers there, only the no-FPU attributes),
    then llc without a floating-point unit."""
    import macwin
    o = pathlib.Path(cmd[cmd.index("-o") + 1])
    ll = o.with_suffix(".ll")
    c = [a for a in cmd if a != "-c"]
    c[c.index("-o") + 1] = str(ll)
    r = subprocess.run(c + ["-S", "-emit-llvm"] + MAC_IR, **kw)
    return r if r.returncode else mac_pending(ll, o, True)

def mac_pending(ll, o, port):
    """The macOS build's first step is done for this file (IR, its definitions listed): mac_finish does the rest
    once every file is there."""
    r = __import__("macwin").defs(ll)
    r.pending = (str(ll), str(o), port)
    return r

def mac_finish(item):
    import macwin
    ll, o, port = item
    r = macwin.run(pathlib.Path(ll), port=port)
    if r.returncode == 0:
        r = subprocess.run([llvm("llc"), "-O2", "-filetype=obj"] + MAC_LLC + [ll, "-o", o], cwd=ROOT, capture_output=True, text=True)
    return o, r.returncode, r.stderr

CC = ["gcc", "-m32", "-std=gnu89", "-c", "-O2", "-g", "-fno-strict-aliasing", "-ffp-contract=off", "-fcommon", "-w", "-fno-pic", "-fno-stack-protector", "-fno-builtin", "-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double", "-include", "port_libm.h",
      "-Iinclude", "-Iport/include", "-include", "port_compat.h"]
TEXT_END, GAME_END = 0x2BF6B0, 0x273CF0  # end of all code; end of game code (libraries follow)

def compile_ee(cmd, src, o):
    """Compiles src with the PS2 compiler's float constants: preprocess, eeconst.transform, compile the result."""
    cmd = variant(cmd)
    pre = [a for a in cmd if a != "-c"] + ["-E", str(src)]
    r = subprocess.run(pre, cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        return r
    i = pathlib.Path(str(o)[:-2] + ".i")
    i.write_text(eeconst.transform(r.stdout))
    if BITS64 and (cmd[0] == "clang" or MAC) and not o.name.startswith("nl_"):  # the game's pointers: 4 bytes (not the maths library: no game text)
        import ptr32
        std = next((a for a in cmd if a.startswith("-std=")), "-std=gnu89")
        try:
            text = ptr32.transform(str(i), ["-x", "cpp-output"] + (CLANG_TARGET or ["-m64"]) + [std, "-fms-extensions", "-w", "-Wno-error=return-mismatch"] +
                                   (["-mno-ms-bitfields"] if WIN else []) +
                                   (["-Wno-error=return-type"] if MAC else []))  # (pip's libclang 18: the older name of return-mismatch)
        except RuntimeError as e:
            return subprocess.CompletedProcess(cmd, 1, "", f"ptr32: {e}\n")
        i.write_text(text, encoding="latin-1")
    if cmd[0] == "clang" or MAC:  # no -fpreprocessed: the input is named as preprocessed, and the forced includes are dropped
        c2, skip = [], False
        for a in cmd:
            if skip:
                skip = False
            elif a == "-include":
                skip = True
            else:
                c2.append(a)
        if MAC:  # through LLVM IR: the game's addresses become offsets in the window (macwin), then arm64 code, no FPU
            ll = pathlib.Path(str(o)[:-2] + ".ll")
            r = subprocess.run(c2 + ["-S", "-emit-llvm"] + MAC_IR + ["-x", "cpp-output", str(i), "-o", str(ll)], cwd=ROOT, capture_output=True, text=True)
            return r if r.returncode else mac_pending(ll, o, o.name.startswith(("pc_", "nl_")))
        if BITS64:  # through LLVM IR: irfix.py repairs what the code generator cannot select for 4-byte pointers
            import irfix
            ll = pathlib.Path(str(o)[:-2] + ".ll")
            r = subprocess.run(c2 + ["-S", "-emit-llvm", "-x", "cpp-output", str(i), "-o", str(ll)], cwd=ROOT, capture_output=True, text=True)
            if r.returncode:
                return r
            text, _ = irfix.fix(ll.read_text())
            ll.write_text(text)
            return subprocess.run(["llc", "-O2", "-filetype=obj", "-relocation-model=static", str(ll), "-o", str(o)], cwd=ROOT, capture_output=True, text=True)
        return subprocess.run(c2 + ["-x", "cpp-output", str(i), "-o", str(o)], cwd=ROOT, capture_output=True, text=True)
    return subprocess.run(cmd + ["-fpreprocessed", str(i), "-o", str(o)], cwd=ROOT, capture_output=True, text=True)

def cc(f):
    o = OBJ / (str(f.relative_to(ROOT / "src")).replace("/", "_")[:-2] + ".o")
    src, done, missing = portsrc.prepare(f)
    r = compile_ee(CC + [f"-I{f.parent}"], src, o)
    if missing:
        print("no C for:", f.name, " ".join(missing))
    return o if r.returncode == 0 else None, f, r.stderr, getattr(r, "pending", None)

def main():
    OBJ.mkdir(parents=True, exist_ok=True)
    key = buildkey.flags()  # (first: a key that cannot be used stops the build before anything is compiled)
    fs = portsrc.sources()
    with concurrent.futures.ProcessPoolExecutor(16) as ex:
        res = list(ex.map(cc, fs))
    pending = [p for _, _, _, p in res if p]  # (the macOS build: files waiting for mac_finish)
    bad = [(f, e) for o, f, e, _ in res if o is None]
    for f, e in bad:
        print("FAILED", f.name, next((l for l in e.splitlines() if "rror" in l or l.startswith("ptr32")), e.strip()[:150])[:200])
    objs = [str(o) for o, _, _, _ in res if o]
    if BITS64:
        # The game's data tables, assembled for this target: with their values (gen_data.py's sources, made from the
        # user's disc) when those exist, else as blank tables of the same shape (port/data, make_skeleton.py), whose
        # values the program fetches from the disc at start. BT3_SKELETON=1 takes the blank ones in any case; that
        # is how releases are built, so that a build needs no disc.
        real = sorted((ROOT / "port/build/gen/data").glob("*.s"))
        blank = os.environ.get("BT3_SKELETON") == "1" or not real
        DATA.mkdir(exist_ok=True)
        for old in DATA.glob("*.o"):
            old.unlink()
        (DATA / "SKELETON").unlink(missing_ok=True)
        if blank:
            (DATA / "SKELETON").write_text("built from port/data: finish with make_dat.py\n")
        for f in (sorted((ROOT / "port/data").glob("*.s")) if blank else real):
            if MAC:  # Mach-O names and window offsets for the tables' pointers (macho.py)
                import macho
                m = DATA / f.name
                m.write_text(macho.asm(f.read_text()))
                r = subprocess.run([llvm("clang")] + CLANG_TARGET + ["-c", "-x", "assembler", str(m), "-o", str(DATA / (f.name[:-2] + ".o"))],
                                   cwd=ROOT, capture_output=True, text=True)
            else:
                r = subprocess.run([PREFIX + "as"] + ([] if WIN else ["--64"]) + [str(f), "-o", str(DATA / (f.name[:-2] + ".o"))], capture_output=True, text=True)
            if r.returncode:
                print("FAILED", f.name, r.stderr.splitlines()[0][:150])
        objs += [str(o) for o in sorted(DATA.glob("*.o"))]
    else:
        objs += [str(o) for o in sorted((ROOT / "port/build/obj_data").glob("*.o"))]  # from gen_data.py
    # PC-only code: the vector-library references under the game's names, and port/src.
    for f in sorted((ROOT / "port/third_party/newlib_libm").glob("*.c")):  # the PS2's maths library, software float
        o = OBJ / ("nl_" + f.stem + ".o")
        r = compile_ee(["gcc", "-m32", "-std=gnu89", "-c", "-O2", "-g", "-fno-pic", "-msoft-float", "-mno-sse", "-mno-mmx",
                        "-malign-double", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w",
                        "-Iport/include", f"-I{f.parent}", "-include", "newlib_shim.h"], f, o)
        if r.returncode:
            print("FAILED", f.name, r.stderr.splitlines()[0][:150])
        else:
            objs.append(str(o))
            if getattr(r, "pending", None):
                pending.append(r.pending)
    # renderer: ordinary host code, hardware float. Its shaders are compiled to SPIR-V and embedded.
    gen = ROOT / "port/build/gen/gs"
    gen.mkdir(parents=True, exist_ok=True)
    arrays = []
    # gsnz.frag: gs.frag without its depth output (NO_FRAG_DEPTH), for the draws that do not need it
    nz = gen / "gsnz.frag"
    nz_text = (ROOT / "port/src/gs/shaders/gs.frag").read_text().replace("\n", "\n#define NO_FRAG_DEPTH\n", 1)
    if not nz.exists() or nz.read_text() != nz_text:
        nz.write_text(nz_text)
    shader_sources = sorted((ROOT / "port/src/gs/shaders").glob("*.*")) + [nz]
    for src in shader_sources:  # gs.vert -> kGsVertSpv, outline.frag -> kOutlineFragSpv
        stage = src.suffix[1:]
        name = "k" + src.stem.capitalize() + stage.capitalize() + "Spv"
        spv = gen / (src.name + ".spv")
        if shutil.which("glslc") is None and spv.exists() and spv.stat().st_mtime >= src.stat().st_mtime:
            pass  # no shader compiler here (the release container): the compiled shader that was put in place is current
        else:
            if shutil.which("glslc") is not None:
                r = subprocess.run(["glslc", f"-fshader-stage={stage}", str(src), "-o", str(spv)], capture_output=True, text=True)
            else:  # the reference compiler of the same language (package glslang-tools)
                r = subprocess.run(["glslangValidator", "-V", "-S", stage, str(src), "-o", str(spv)], capture_output=True, text=True)
            if r.returncode:
                print("FAILED shader", src.name, r.stderr[:300])
                continue
        arrays.append(f"static const unsigned char {name}[] = {{" + ",".join(str(b) for b in spv.read_bytes()) + "};\n")
        if MAC:  # SDL_GPU's Metal back end takes MSL: the same shader translated (bindings as numbered: SDL's Metal slots)
            msl = gen / (src.name + ".msl")
            r = subprocess.run(["spirv-cross", "--msl", "--msl-version", "20100", "--msl-decoration-binding", str(spv), "--output", str(msl)],
                               capture_output=True, text=True)
            if r.returncode:
                print("FAILED shader (MSL)", src.name, r.stderr[:300])
                continue
            arrays.append(f"static const unsigned char {name[:-3]}Msl[] = {{" + ",".join(str(b) for b in msl.read_bytes() + b"\0") + "};\n")
    # The GL back end (gs_gl.c) compiles the GLSL at run time (it translates Vulkan GLSL to GL 3.3 the way
    # port/tools/glprobe.c proved), so the sources are embedded here too, NUL-terminated.
    for src in shader_sources:
        if src.suffix not in (".vert", ".frag"):
            continue
        stage = src.suffix[1:]
        name = "k" + src.stem.capitalize() + stage.capitalize() + "Glsl"
        data = src.read_bytes() + b"\0"
        arrays.append(f"static const unsigned char {name}[] = {{" + ",".join(str(b) for b in data) + "};\n")
    # the side bars' two pictures (port/assets/bars), as bytes in the program: nothing to ship beside it
    bars = "".join(f"static const unsigned char kBars{n.capitalize()}Png[] = {{" + ",".join(str(b) for b in (ROOT / "port/assets/bars" / (n + ".png")).read_bytes()) + "};\n"
                   for n in ("trim", "fill"))
    (gen / "bars_png.h").write_text("/* generated from port/assets/bars by port/tools/undefined.py */\n" + bars)
    (gen / "shaders.h").write_text("/* generated from port/src/gs/shaders by port/tools/undefined.py */\n" + "".join(arrays))
    for f in sorted((ROOT / "port/src/gs").glob("*.c")):
        o = OBJ / ("gs_" + f.stem + ".o")
        r = run(["gcc", "-m32", "-std=gnu99", "-c", "-O2", "-g", "-fno-pic", "-msse2", "-mfpmath=sse", "-fno-strict-aliasing",
                            "-Wall", "-Wno-unused", "-Wno-misleading-indentation", f"-I{gen}"] + (key if f.name == "net_sign.c" else []) + [str(f), "-o", str(o)], cwd=ROOT,
                           capture_output=True, text=True)
        if r.returncode:
            print("FAILED", f.name, "\n".join(l for l in r.stderr.splitlines() if "error" in l)[:400])
        else:
            objs.append(str(o))
    # C++: the settings window (port/src/gs/ui.cpp) and Dear ImGui. The library is only compiled when it changed.
    imgui = ROOT / "port/third_party/imgui"
    for f in sorted(imgui.glob("*.cpp")) + sorted((ROOT / "port/src/gs").glob("*.cpp")):
        o = OBJ / ("cxx_" + f.stem + ".o")
        if f.parent == imgui and o.exists() and o.stat().st_mtime > f.stat().st_mtime:
            objs.append(str(o))
            continue
        r = run(["g++", "-m32", "-std=c++17", "-c", "-O2", "-g", "-fno-pic", "-msse2", "-mfpmath=sse", "-fno-exceptions",
                            "-fno-rtti", "-w", f"-I{imgui}", f"-I{ROOT / 'port/src/gs'}", f"-I{gen}", str(f), "-o", str(o)], cwd=ROOT,
                           capture_output=True, text=True)
        if r.returncode:
            print("FAILED", f.name, "\n".join(l for l in r.stderr.splitlines() if "error" in l)[:600])
        else:
            objs.append(str(o))
    for f in [ROOT / "src/port/vu0_a.c", ROOT / "src/port/vu0_b.c"] + sorted((ROOT / "port/src").glob("*.c")):
        o = OBJ / ("pc_" + f.stem + ".o")
        names = ["-include", "vu0_names.h", "-DREF_VU0_EXTERN_ARITH"] if f.parent.name == "port" and f.parent.parent.name == "src" else []
        soft = [] if f.name in ("plat_libm.c", "plat_fastvec.c") else ["-msoft-float", "-mno-sse", "-mno-mmx", "-DSF_FAST_CALLS", "-include", "math.h", "-include", "stdlib.h", "-include", "port_libm.h"]
        cmd = ["gcc", "-m32", "-std=gnu99", "-c", "-O2", "-g", "-fno-pic", "-malign-double", "-fno-strict-aliasing",
               "-ffp-contract=off", "-fno-builtin", "-w", "-Iinclude", "-Iport/include", "-Iport/src"] + soft + names
        if os.environ.get("BT3_PORT_CFLAGS"):  # profiling: e.g. -fno-omit-frame-pointer
            cmd += os.environ["BT3_PORT_CFLAGS"].split()
        if soft and not names:  # the port's own soft-float code: PS2 constants; the vector references are written for the host
            r = compile_ee(cmd, f, o)
        else:
            r = run(cmd + [str(f), "-o", str(o)], cwd=ROOT, capture_output=True, text=True)
        if r.returncode:
            print("FAILED", f.name, r.stderr.splitlines()[0][:150])
        else:
            objs.append(str(o))
            if getattr(r, "pending", None):
                pending.append(r.pending)
    if pending:  # the macOS build's second step, now that every definition of the program is listed (macwin.py)
        import macwin
        macwin.sigs([ll for ll, _, _ in pending])
        with concurrent.futures.ProcessPoolExecutor(16) as ex:
            for o, rc, err in ex.map(mac_finish, pending):
                if rc:
                    print("FAILED", pathlib.Path(o).name, next((l for l in err.splitlines() if "rror" in l or "macwin" in l), err.strip()[:150])[:300])
                    objs.remove(o)
    defined, undef = set(), collections.Counter()
    # Native Windows has a 32767-character CreateProcess limit; absolute object paths exceed it.
    argsfile = ROOT / "port/build/nm-objects.rsp"
    argsfile.write_text("\n".join('"' + p.replace('\\', '/') + '"' for p in objs))
    out = subprocess.run([llvm("llvm-nm") if MAC else PREFIX + "nm", "-A", "@" + str(argsfile)], capture_output=True, text=True, check=True).stdout
    for l in out.splitlines():
        m = re.match(r"\S+:\s*([0-9a-f]*)\s+(\w)\s+(\S+)$", l)
        if not m:
            continue
        if MAC:  # Mach-O: C names with a leading underscore
            m = re.match(r"(\S+:\s*[0-9a-f]*\s+\w\s+)_?(\S+)$", l)
            m = re.match(r"\S+:\s*([0-9a-f]*)\s+(\w)\s+(\S+)$", m.group(1) + m.group(2))
        if m.group(2) == "U":
            undef[m.group(3)] += 1
        elif m.group(2) not in "tdbr":
            defined.add(m.group(3))
    addr = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        if p.name.startswith("menu"):
            continue
        for l in p.read_text().splitlines():
            m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)", l)
            if m:
                addr[m.group(1)] = int(m.group(2), 16)
    kinds = collections.defaultdict(list)
    for s in undef:
        if s in defined:
            continue
        a = addr.get(s)
        if a is None:
            m = re.match(r"(?:func|D|jtbl)_([0-9A-F]{8})$", s)
            a = int(m.group(1), 16) if m else None
        k = ("no address (host library or compiler helper)" if a is None else "game function" if a < GAME_END
             else "library function (SDK / CRI / libc)" if a < TEXT_END else "data")
        kinds[k].append(s)
    print(f"{len(objs)} objects, {len(defined)} global symbols defined")
    for k, v in sorted(kinds.items()):
        print(f"{len(v):5d}  {k}")
        if "-v" in sys.argv:
            print("       " + " ".join(sorted(v)))
    (ROOT / "port/build/undefined.txt").write_text(
        "".join(f"{k}\t{s}\n" for k, v in sorted(kinds.items()) for s in sorted(v)))

if __name__ == "__main__":
    main()
