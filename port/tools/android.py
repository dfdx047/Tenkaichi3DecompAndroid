#!/usr/bin/env python3
"""Builds the engine for Android arm64 (Dragon Rage): the same sources and the same steps as the 64-bit PC build
(undefined.py + link.py with BT3_CC=clang64), with clang's AArch64 target instead of x86-64.

    port/tools/android.py [compile] [link] [loader] [install]        (all four when none is named)
        -> port/build/android/libbt3.so  (+ libbt3.so.dat, the list of where the game's data comes from)

What is different from the PC build:
  - the game's 4-byte pointers (`__ptr32 __uptr`, ptr32.py) need clang/LLVM 20 or newer on AArch64; irfix.py does
    the same repairs as for x86-64
  - the PS2's float arithmetic: software float through the soft ABI (`-mabi=aapcs-soft -mgeneral-regs-only`,
    clang 19 or newer), so every float operation still goes to port/src/softfloat_ps2.c
  - the program: linked at 0x03000000 (the PC's is at 0x20000000, where an Android app has its Java heap), without a program interpreter, and marked as a shared object
    afterwards, so Android's own loader can load it into a region the app reserved at that address (the Android
    side: android/app/src/main/cpp). Code is position-independent (GOT access to the system libraries' data) but
    the link is not: the game's 32-bit addresses of symbols in its data tables are fixed at link time.

Environment (defaults: the folders port/tools/android_toolchain.sh unpacks):
    BT3_LLVM      folder of an LLVM 20+ installation (bin/clang, bin/llc, lib/libclang.so)
    BT3_SYSROOT   the NDK's sysroot (usr/include, usr/lib/aarch64-linux-android)
    BT3_CLANG_RT  folder with libclang_rt.builtins-aarch64-android.a
    BT3_SDL3      folder with SDL3's include/ and lib/libSDL3.so for Android arm64
    BT3_GLSLC     the shader compiler (glslc)
    BT3_JOBS      parallel compiles (default: the number of processors)
    BT3_SKELETON  1: the game's data tables blank (always for a release); otherwise port/build/gen/data if it exists
"""
import concurrent.futures, os, pathlib, re, shutil, struct, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TC = pathlib.Path(os.environ.get("BT3_TOOLCHAIN", str(ROOT / "port/build/android-toolchain")))
LLVM = pathlib.Path(os.environ.get("BT3_LLVM", str(TC / "llvm")))
SYSROOT = pathlib.Path(os.environ.get("BT3_SYSROOT", str(TC / "ndk/sysroot")))
CLANG_RT = pathlib.Path(os.environ.get("BT3_CLANG_RT", str(TC / "ndk/clang_rt")))
SDL3 = pathlib.Path(os.environ.get("BT3_SDL3", str(TC / "sdl3")))
GLSLC = os.environ.get("BT3_GLSLC", str(TC / "ndk/bin/glslc"))
API = os.environ.get("BT3_ANDROID_API", "29")

# the Python side of libclang for ptr32.py: the installation's own if it has one, else what the toolchain unpacked
for p in (LLVM / "lib/python3/site-packages", TC / "clang-bindings"):
    if p.exists():
        sys.path.insert(0, str(p))
os.environ["LD_LIBRARY_PATH"] = os.pathsep.join([str(LLVM / "lib"), str(pathlib.Path(GLSLC).parent), os.environ.get("LD_LIBRARY_PATH", "")])
sys.path.insert(0, str(HERE))
import portsrc, eeconst, irfix  # noqa: E402
try:
    import clang.cindex as _ci
    if (LLVM / "lib/libclang.so").exists():
        _ci.Config.set_library_file(str(LLVM / "lib/libclang.so"))
except ImportError:
    pass
import ptr32  # noqa: E402

OUT = ROOT / "port/build/android"
OBJ = OUT / "obj"
DATA = OUT / "obj_data"
GEN = OUT / "gen"
EXE = OUT / "libbt3.so"
BASE = int(os.environ.get("BT3_ANDROID_BASE", "0x03000000"), 0)  # below an app's Java heap (0x12C00000 up): plat_mem.c

CLANG = str(LLVM / "bin/clang")
LLC = str(LLVM / "bin/llc")
TARGET = [f"--target=aarch64-linux-android{API}", f"--sysroot={SYSROOT}"]
SOFT = ["-mabi=aapcs-soft", "-mgeneral-regs-only"]
# the game's code: the flags of undefined.py's CC, for this target
GAME = [CLANG] + TARGET + ["-std=gnu89", "-c", "-O2", "-g1", "-fno-strict-aliasing", "-ffp-contract=off", "-fcommon", "-w",
                           "-fPIE", "-fno-stack-protector", "-fno-builtin", "-fms-extensions", "-Wno-error=return-mismatch"] + SOFT + \
       ["-include", "port_libm.h", "-Iinclude", "-Iport/include", "-include", "port_compat.h"]


def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, capture_output=True, text=True, **kw)


def compile_ee(cmd, src, o, game_text=True):
    """undefined.compile_ee for this target: preprocess, the PS2 compiler's float constants (eeconst), 4-byte
    pointers in the game's text (ptr32), LLVM IR, irfix, llc."""
    pre = [a for a in cmd if a != "-c"] + ["-E", str(src)]
    r = run(pre)
    if r.returncode:
        return r
    i = pathlib.Path(str(o)[:-2] + ".i")
    i.write_text(eeconst.transform(r.stdout))
    std = next((a for a in cmd if a.startswith("-std=")), "-std=gnu89")
    if game_text:
        try:
            text = ptr32.transform(str(i), ["-x", "cpp-output"] + TARGET + [std, "-fms-extensions", "-w", "-Wno-error=return-mismatch"] + SOFT)
        except RuntimeError as e:
            return subprocess.CompletedProcess(cmd, 1, "", f"ptr32: {e}\n")
        i.write_text(text, encoding="latin-1")
    c2, skip = [], False
    for a in cmd:  # the forced includes are already in the preprocessed text
        if skip:
            skip = False
        elif a == "-include":
            skip = True
        else:
            c2.append(a)
    ll = pathlib.Path(str(o)[:-2] + ".ll")
    r = run(c2 + ["-S", "-emit-llvm", "-x", "cpp-output", str(i), "-o", str(ll)])
    if r.returncode:
        return r
    text, _ = irfix.fix(ll.read_text(), stores=True)
    ll.write_text(text)
    r = run([LLC, "-O2", "-filetype=obj", "-relocation-model=pic", str(ll), "-o", str(o)])
    if r.returncode == 0 and not os.environ.get("BT3_KEEP"):
        i.unlink(missing_ok=True)
        ll.unlink(missing_ok=True)
    return r


def game_source(f):
    o = OBJ / (str(f.relative_to(ROOT / "src")).replace("/", "_")[:-2] + ".o")
    src, done, missing = portsrc.prepare(f)
    r = compile_ee(GAME + [f"-I{f.parent}"], src, o)
    return (o if r.returncode == 0 else None), f.name, r.stderr


def newlib_source(f):
    o = OBJ / ("nl_" + f.stem + ".o")
    cmd = [CLANG] + TARGET + ["-std=gnu89", "-c", "-O2", "-g1", "-fPIE", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w"] + SOFT + \
          ["-Iport/include", f"-I{f.parent}", "-include", "newlib_shim.h"]
    r = compile_ee(cmd, f, o, game_text=False)
    return (o if r.returncode == 0 else None), f.name, r.stderr


HARD = [CLANG] + TARGET + ["-c", "-O2", "-g1", "-fPIE", "-fno-strict-aliasing", "-DBT3_ANDROID=1"]


def port_source(f):
    o = OBJ / ("pc_" + f.stem + ".o")
    names = ["-include", "vu0_names.h", "-DREF_VU0_EXTERN_ARITH"] if f.parent.name == "port" and f.parent.parent.name == "src" else []
    hard = f.name in ("plat_libm.c", "plat_fastvec.c")
    cmd = [CLANG] + TARGET + ["-std=gnu99", "-c", "-O2", "-g1", "-fPIE", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w",
                              "-fms-extensions", "-DBT3_ANDROID=1", "-Iinclude", "-Iport/include", "-Iport/src", f"-I{SDL3 / 'include'}"]
    if not hard:
        cmd += SOFT + ["-DSF_FAST_CALLS", "-include", "math.h", "-include", "stdlib.h", "-include", "port_libm.h"]
    cmd += names
    if not hard and not names:
        r = compile_ee(cmd, f, o)
    else:
        r = run(cmd + [str(f), "-o", str(o)])
    return (o if r.returncode == 0 else None), f.name, r.stderr


def gs_source(f):
    o = OBJ / ("gs_" + f.stem + ".o")
    r = run(HARD + ["-std=gnu99", "-Wall", "-Wno-unused", "-Wno-misleading-indentation", f"-I{GEN / 'gs'}", f"-I{SDL3 / 'include'}", str(f), "-o", str(o)])
    return (o if r.returncode == 0 else None), f.name, r.stderr


def cxx_source(f):
    o = OBJ / ("cxx_" + f.stem + ".o")
    imgui = ROOT / "port/third_party/imgui"
    r = run([str(LLVM / "bin/clang++")] + TARGET + ["-std=c++17", "-c", "-O2", "-g1", "-fPIE", "-fno-exceptions", "-fno-rtti", "-w", "-DBT3_ANDROID=1",
                                                   f"-I{imgui}", f"-I{ROOT / 'port/src/gs'}", f"-I{SDL3 / 'include'}", str(f), "-o", str(o)])
    return (o if r.returncode == 0 else None), f.name, r.stderr


def shaders():
    """port/src/gs/shaders as SPIR-V and as source text, in the header the renderer includes (as undefined.py)."""
    gen = GEN / "gs"
    gen.mkdir(parents=True, exist_ok=True)
    arrays = []
    for src in sorted((ROOT / "port/src/gs/shaders").glob("*.*")):
        stage = src.suffix[1:]
        name = "k" + src.stem.capitalize() + stage.capitalize() + "Spv"
        spv = gen / (src.name + ".spv")
        r = run([GLSLC, f"-fshader-stage={stage}", str(src), "-o", str(spv)])
        if r.returncode:
            print("FAILED shader", src.name, r.stderr[:300])
            continue
        arrays.append(f"static const unsigned char {name}[] = {{" + ",".join(str(b) for b in spv.read_bytes()) + "};\n")
    # gs.frag a second time without its discards (the draws that never discard; gs_gpu.c, key bit 23)
    spv = gen / "gs_nd.frag.spv"
    r = run([GLSLC, "-fshader-stage=frag", "-DGS_NO_DISCARD=1", str(ROOT / "port/src/gs/shaders/gs.frag"), "-o", str(spv)])
    if r.returncode:
        print("FAILED shader gs.frag (no discard)", r.stderr[:300])
    else:
        arrays.append("static const unsigned char kGsNdFragSpv[] = {" + ",".join(str(b) for b in spv.read_bytes()) + "};\n")
    for src in sorted((ROOT / "port/src/gs/shaders").glob("*")):
        if src.suffix in (".vert", ".frag"):
            name = "k" + src.stem.capitalize() + src.suffix[1:].capitalize() + "Glsl"
            data = src.read_bytes() + b"\0"
            arrays.append(f"static const unsigned char {name}[] = {{" + ",".join(str(b) for b in data) + "};\n")
    (gen / "shaders.h").write_text("/* generated from port/src/gs/shaders by port/tools/android.py */\n" + "".join(arrays))


def compile_all():
    OBJ.mkdir(parents=True, exist_ok=True)
    DATA.mkdir(parents=True, exist_ok=True)
    shaders()
    jobs = []
    jobs += [(game_source, f) for f in portsrc.sources()]
    jobs += [(newlib_source, f) for f in sorted((ROOT / "port/third_party/newlib_libm").glob("*.c"))]
    jobs += [(port_source, f) for f in [ROOT / "src/port/vu0_a.c", ROOT / "src/port/vu0_b.c"] + sorted((ROOT / "port/src").glob("*.c"))]
    jobs += [(gs_source, f) for f in sorted((ROOT / "port/src/gs").glob("*.c"))]
    jobs += [(cxx_source, f) for f in sorted((ROOT / "port/third_party/imgui").glob("*.cpp")) + sorted((ROOT / "port/src/gs").glob("*.cpp"))]
    only = os.environ.get("BT3_ONLY")  # a few files: BT3_ONLY=btl_obj,plat_mem
    if only:
        names = only.split(",")
        jobs = [j for j in jobs if any(n in j[1].name for n in names)]
    failed = []
    with concurrent.futures.ThreadPoolExecutor(int(os.environ.get("BT3_JOBS", os.cpu_count() or 2))) as ex:
        for o, name, err in ex.map(lambda j: j[0](j[1]), jobs):
            if o is None:
                failed.append(name)
                lines = [l for l in err.splitlines() if "error" in l or l.startswith("ptr32") or "Cannot select" in l]
                print("FAILED", name, "\n   ".join((lines or err.strip().splitlines() or ["?"])[:6])[:900], flush=True)
    # the game's data tables: blank (their values come from the user's disc at start) unless real ones were made
    real = sorted((ROOT / "port/build/gen/data").glob("*.s"))
    blank = os.environ.get("BT3_SKELETON") == "1" or not real
    for old in DATA.glob("*.o"):
        old.unlink()
    for f in (sorted((ROOT / "port/data").glob("*.s")) if blank else real):
        r = run([CLANG] + TARGET + ["-c", str(f), "-o", str(DATA / (f.name[:-2] + ".o"))])
        if r.returncode:
            failed.append(f.name)
            print("FAILED", f.name, r.stderr.strip()[:300])
    (DATA / "SKELETON").write_text("blank\n") if blank else (DATA / "SKELETON").unlink(missing_ok=True)
    print(f"compiled: {len(jobs) - len([n for n in failed if not n.endswith('.s')])} of {len(jobs)} sources; {len(failed)} failed")
    return not failed


def rtdir():
    """clang's runtime folder for this target, filled from the NDK's files (the upstream LLVM has none for Android)."""
    out = pathlib.Path(run([CLANG] + TARGET + ["-print-resource-dir"]).stdout.strip()) / f"lib/aarch64-unknown-linux-android{API}"
    if not (out / "libclang_rt.builtins.a").exists():
        out.mkdir(parents=True, exist_ok=True)
        shutil.copy(CLANG_RT / "libclang_rt.builtins-aarch64-android.a", out / "libclang_rt.builtins.a")
        shutil.copy(CLANG_RT / "libunwind.a", out / "libunwind.a")
    return out


def link_cmd(objs, extra, out=None, base=None):
    out = out or EXE
    base = BASE if base is None else base
    return [str(LLVM / "bin/clang++")] + TARGET + [
        "-fuse-ld=lld", "-no-pie", "-nostartfiles", "-o", str(out)] + extra + objs + [
        str(SYSROOT / f"usr/lib/aarch64-linux-android/{API}/crtbegin_so.o"),
        f"-L{SDL3 / 'lib'}", f"-L{rtdir()}", "-lSDL3", "-llog", "-landroid", "-lm", "-ldl",
        "-static-libstdc++",
        str(SYSROOT / f"usr/lib/aarch64-linux-android/{API}/crtend_so.o"),
        f"-Wl,--image-base={base:#x}", "-Wl,--no-dynamic-linker", "-Wl,--export-dynamic", "-Wl,--hash-style=both",
        "-Wl,-z,max-page-size=16384", "-Wl,--wrap=main", "-Wl,--wrap=Progress_Main", "-Wl,--defsym=D_3BE71C=0x3BE71C",
        "-Wl,--unresolved-symbols=report-all", "-Wl,--error-limit=0", f"-Wl,-Map={EXE}.map", "-Wl,--build-id=sha1"]


def link():
    objs = [str(o) for o in sorted(OBJ.glob("*.o")) + sorted(DATA.glob("*.o"))]
    r = run(link_cmd(objs, []))
    need = sorted(set(re.findall(r"undefined symbol: (\w+)", r.stderr)))
    lines = ['/* Generated by port/tools/android.py: one stub per symbol the Android build does not provide yet. */',
             '#include <android/log.h>', 'extern void abort(void);',
             'static void missing(const char *n) { __android_log_print(6, "bt3", "not implemented yet: %s", n); abort(); }']
    lines += [f'void {s}(void) {{ missing("{s}"); }}' for s in need]
    GEN.mkdir(parents=True, exist_ok=True)
    (GEN / "stubs.c").write_text("\n".join(lines) + "\n")
    (OUT / "stubs.txt").write_text("\n".join(need) + "\n")
    stub_o = GEN / "stubs.o"
    if need:
        s = run([CLANG] + TARGET + ["-c", "-O2", "-fPIE", str(GEN / "stubs.c"), "-o", str(stub_o)])
        if s.returncode:
            print(s.stderr[:600])
            return False
    r = run(link_cmd(objs, [str(stub_o)] if need else []))
    errs = [l for l in r.stderr.splitlines() if "error" in l]
    print(f"{len(need)} stubs; link {'OK -> ' + str(EXE.relative_to(ROOT)) if r.returncode == 0 else 'FAILED'}")
    for l in errs[:20]:
        print("  " + l[-200:])
    if r.returncode:
        return False
    # the same program linked RELOC_DELTA higher: the words that differ by exactly that are the program's own
    # addresses, which the loader moves when the program cannot be loaded at BASE (relocations())
    alt = OUT / "libbt3.alt.so"
    r = run(link_cmd(objs, [str(stub_o)] if need else [], out=alt, base=BASE + RELOC_DELTA))
    if r.returncode:
        print("second link FAILED", r.stderr[-600:])
        return False
    if not relocations(EXE, alt):
        return False
    alt.unlink()
    finish()
    return True


RELOC_DELTA = 0x10000000
SKIP_SECTIONS = {".dynsym", ".gnu.version", ".gnu.version_r", ".gnu.version_d", ".gnu.hash", ".hash", ".dynstr", ".rela.dyn",
                 ".rela.plt", ".dynamic", ".note.android.ident", ".note.gnu.build-id"}


def elf_sections(data):
    shoff, = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    raw = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shentsize) for i in range(shnum)]
    strtab = raw[shstrndx][4]
    out = {}
    for sh in raw:
        name = data[strtab + sh[0]:data.index(b"\0", strtab + sh[0])].decode()
        out[name] = {"type": sh[1], "flags": sh[2], "addr": sh[3], "off": sh[4], "size": sh[5]}
    return out


def relocations(exe, alt):
    """libbt3.so.rel: every 4-byte word of the program that holds one of its own addresses (32-bit game pointers and the
    low half of 64-bit ones: the program is below 4 GB), found by comparing two links; the loader adds the load bias to
    them. Also where the program's initialisers are: the loader calls them itself after moving (finish() hides them from
    Android's loader, which would call them before)."""
    a, b = exe.read_bytes(), alt.read_bytes()
    secs, secs_b = elf_sections(a), elf_sections(b)
    for name, sec in secs.items():  # (the allocated part must be laid out the same; names in .strtab may differ)
        if sec["flags"] & 2 and (name not in secs_b or secs_b[name]["off"] != sec["off"] or secs_b[name]["size"] != sec["size"]):
            print("relocations: the two links are laid out differently at", name)
            return False
    skip = set()
    for name in (".rela.dyn", ".rela.plt"):  # slots Android's loader writes itself
        sec = secs.get(name)
        if sec:
            for k in range(sec["size"] // 24):
                off, = struct.unpack_from("<Q", a, sec["off"] + k * 24)
                skip.add(off)
    words, odd = [], 0
    for name, sec in secs.items():
        if not (sec["flags"] & 2) or sec["type"] == 8 or name in SKIP_SECTIONS:  # allocated, with bytes in the file
            continue
        for k in range(0, sec["size"] - 3, 4):
            o = sec["off"] + k
            x, = struct.unpack_from("<I", a, o)
            y, = struct.unpack_from("<I", b, o)
            if x == y:
                continue
            va = sec["addr"] + k
            if (y - x) & 0xFFFFFFFF == RELOC_DELTA and va not in skip and va - 4 not in skip:
                words.append(va)
            elif va not in skip and va - 4 not in skip:
                odd += 1
    if odd:
        print(f"relocations: {odd} words differ by something else than the move (not moved)")
    init = secs.get(".init_array", {"addr": 0, "size": 0})
    rel = bytearray(b"BT3R" + struct.pack("<IIIII", 1, BASE, len(words), init["addr"], init["size"]))
    for w in words:
        rel += struct.pack("<I", w)
    pathlib.Path(str(exe) + ".rel").write_bytes(rel)
    print(f"relocations: {len(words)} words -> {exe.name}.rel")
    return True


def nm(path):
    syms = {}
    for l in run([LLVM / "bin/llvm-nm", "--defined-only", str(path)]).stdout.splitlines():
        p = l.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return syms


def file_offset(exe, addr):
    """Offset in the file of a virtual address, from the program headers."""
    data = exe.read_bytes()
    phoff, = struct.unpack_from("<Q", data, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
    for i in range(phnum):
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from("<IIQQQQ", data, phoff + i * phentsize)
        if p_type == 1 and p_vaddr <= addr < p_vaddr + p_filesz:
            return p_offset + addr - p_vaddr
    raise SystemExit(f"address {addr:#x} is not in the file")


def finish():
    """After the link: the list of the game's data (make_dat.py, from the symbol table instead of a GNU map) when the
    data tables are blank, and the ELF type changed to a shared object for Android's loader."""
    syms = nm(EXE)
    image = bytearray(EXE.read_bytes())
    if (DATA / "SKELETON").exists():
        where, checksum = {}, None
        for l in (ROOT / "port/data/index.txt").read_text().splitlines():
            p = l.split()
            if len(p) == 3 and not l.startswith("#"):
                where[p[0]] = (int(p[1]), int(p[2], 16))
            elif len(p) == 2 and p[0] == "checksum":
                checksum = int(p[1], 16)
        records = []
        for name in sorted(where):
            which, so = where[name]
            text = (ROOT / "port/data" / (name + ".s")).read_text().splitlines()
            # where the object's data starts: the first label's address less what the file puts before that label
            first, before = None, 0
            for l in text:
                if re.match(r"^[A-Za-z_.$][\w.$]*:$", l.strip()):
                    first = l.strip()[:-1]
                    break
                p = l.split()
                if p and p[0] in (".space", ".zero"):
                    before += int(p[1])
                elif p and p[0] == ".long":
                    before += 4
            if first is None or first not in syms:
                raise SystemExit(f"{name}: its first label is not in the program")
            start, off = syms[first] - before, 0
            for l in text:
                p = l.split()
                if not p or p[0].startswith("#"):
                    continue
                if p[0] == ".space":
                    records.append((start + off, which, so + off, int(p[1])))
                    off += int(p[1])
                elif p[0] == ".zero":
                    off += int(p[1])
                elif p[0] == ".long":
                    off += 4
        dat = bytearray(b"BT3D" + struct.pack("<IQ", len(records), checksum))
        for r in records:
            dat += struct.pack("<IIII", *r)
        pathlib.Path(str(EXE) + ".dat").write_bytes(dat)
        k = file_offset(EXE, syms["gPortDataStripped"])
        image[k:k + 4] = struct.pack("<I", 1)
        print(f"data list: {len(records)} records -> {EXE.name}.dat")
    dyn = elf_sections(bytes(image))[".dynamic"]
    for k in range(dyn["size"] // 16):  # DT_INIT_ARRAY(SZ) -> tags Android's loader ignores: the loader calls them (relocations())
        tag, = struct.unpack_from("<q", image, dyn["off"] + k * 16)
        if tag in (25, 27):
            struct.pack_into("<q", image, dyn["off"] + k * 16, 0x6FFFF000 + tag)
    struct.pack_into("<H", image, 0x10, 3)  # e_type: ET_EXEC -> ET_DYN (loaded at its own addresses, android/app/src/main/cpp)
    EXE.write_bytes(image)


APP = ROOT / "android/app/src/main"


ADRENO = ROOT / "port/third_party/libadrenotools"
ADRENO_HOOKS = ["hook_impl", "main_hook", "file_redirect_hook", "gsl_alloc_hook"]


def adrenotools():
    """libadrenotools (custom Vulkan drivers such as Turnip on Adreno GPUs): its objects for libmain.so and the hook
    libraries it loads from the app's native library folder."""
    obj = OUT / "adreno"
    obj.mkdir(parents=True, exist_ok=True)
    cxx = [CLANG.replace("/clang", "/clang++")] + TARGET + ["-c", "-O2", "-fPIC", "-std=c++17", "-w", f"-I{ADRENO}/include", f"-I{ADRENO}",
                                                          f"-I{ADRENO}/lib/linkernsbypass", "-fvisibility=hidden"]
    cc = [CLANG] + TARGET + ["-c", "-O2", "-fPIC", "-w", f"-I{ADRENO}/include", f"-I{ADRENO}/src/hook"]
    srcs = {"ns": "lib/linkernsbypass/android_linker_ns.cpp", "soname": "lib/linkernsbypass/elf_soname_patcher.cpp",
            "driver": "src/driver.cpp", "bcenabler": "src/bcenabler.cpp", "bcpatch": "src/bcenabler_patch.s",
            "hook_impl": "src/hook/hook_impl.cpp", "main_hook": "src/hook/main_hook.c",
            "file_redirect_hook": "src/hook/file_redirect_hook.c", "gsl_alloc_hook": "src/hook/gsl_alloc_hook.c"}
    for name, rel in srcs.items():
        src = ADRENO / rel
        comp = cxx if rel.endswith(".cpp") else cc
        r = run(comp + [str(src), "-o", str(obj / (name + ".o"))])
        if r.returncode:
            print("adrenotools: FAILED", rel, r.stderr[:1200])
            return False
    link = [CLANG.replace("/clang", "/clang++")] + TARGET + ["-shared", "-static-libstdc++", f"-L{rtdir()}", "-Wl,-z,max-page-size=16384"]
    r = run(link + ["-o", str(OUT / "libhook_impl.so"), str(obj / "hook_impl.o"), str(obj / "ns.o"), str(obj / "soname.o"),
                    "-llog", "-ldl", "-landroid"])
    if r.returncode:
        print("adrenotools: FAILED hook_impl", r.stderr[:1200])
        return False
    for h in ADRENO_HOOKS[1:]:
        r = run(link + ["-o", str(OUT / f"lib{h}.so"), str(obj / f"{h}.o"), "-Wl,-z,global", f"-L{OUT}", "-lhook_impl"])
        if r.returncode:
            print("adrenotools: FAILED", h, r.stderr[:1200])
            return False
    return True


def loader():
    """libmain.so, the part SDL's activity starts (android/app/src/main/cpp/loader.c), with libadrenotools in it."""
    if not adrenotools():
        return False
    obj = OUT / "adreno"
    r = run([CLANG.replace("/clang", "/clang++")] + TARGET + ["-shared", "-fPIC", "-O2", "-Wall", "-x", "c", str(APP / "cpp/loader.c"), "-x", "none",
                                f"-I{ADRENO}/include", str(obj / "driver.o"), str(obj / "bcenabler.o"), str(obj / "bcpatch.o"),
                                str(obj / "ns.o"), str(obj / "soname.o"), "-o", str(OUT / "libmain.so"), "-static-libstdc++",
                                f"-L{rtdir()}", "-llog", "-ldl", "-landroid", "-Wl,-z,max-page-size=16384", "-Wl,--build-id=sha1"])
    print("loader:", "OK" if r.returncode == 0 else "FAILED\n" + r.stderr[:1500])
    return r.returncode == 0


def install():
    """The engine's files into the app: the libraries for the APK, the data list as an asset."""
    jni = APP / "jniLibs/arm64-v8a"
    jni.mkdir(parents=True, exist_ok=True)
    for f in [EXE, OUT / "libmain.so", SDL3 / "lib/libSDL3.so"] + [OUT / f"lib{h}.so" for h in ADRENO_HOOKS]:
        shutil.copy(f, jni / f.name)
    assets = APP / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    if pathlib.Path(str(EXE) + ".dat").exists():
        shutil.copy(str(EXE) + ".dat", assets / "libbt3.so.dat")
    if pathlib.Path(str(EXE) + ".rel").exists():
        shutil.copy(str(EXE) + ".rel", assets / "libbt3.so.rel")
    java = SDL3 / "java/org"
    if java.exists():
        shutil.copytree(java, APP / "java/org", dirs_exist_ok=True)
    print("installed into", jni.relative_to(ROOT))
    return True


def main():
    args = sys.argv[1:] or ["compile", "link", "loader", "install"]
    ok = True
    if "compile" in args:
        ok = compile_all()
    if "link" in args and (ok or os.environ.get("BT3_LINK_ANYWAY")):
        ok = link()
    if "loader" in args and ok:
        ok = loader()
    if "install" in args and ok:
        ok = install()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
