#!/usr/bin/env python3
"""Turn the data the decompilation still keeps as assembly into host assembly for the PC build.

The matching build links some data sections straight from files that splat generates out of the user's own
SLUS_216.78 (asm/data/...). Those files keep pointers symbolic (`.word SomeFunc`), which is what a PC build needs:
the linker fills in the PC addresses. This tool rewrites them for the host assembler:
`.word` -> `.long` (a "word" is 2 bytes on x86), power-of-two `.align` -> `.p2align`, references to labels inside
assembly functions (jump tables of code the PC build does not use) -> 0, and each chunk starts at the same offset
modulo 16 as on the PS2, so data that must be 16-byte aligned stays aligned.

Usage: port/tools/gen_data.py [--asm DIR]     DIR = the decompilation's generated asm/ (default ../bt3/asm)
Output: port/build/gen/data/*.s and the assembled port/build/obj_data/*.o  (generated from the game; never commit)"""
import argparse, collections, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
KINDS = ("data", "sdata", "rodata", "lit4", "lit8", "bss", "sbss")
SECTION = {"data": ".data", "sdata": ".data", "rodata": ".data", "lit4": ".data", "lit8": ".data",
           "bss": ".bss", "sbss": ".bss"}  # everything writable: the original rodata is patched in places

def chunks(yaml, base=0x100000):
    """(kind, name, PS2 address) of every data chunk the matching build takes from assembly."""
    out = []
    for l in yaml.read_text().splitlines():
        m = re.match(r"\s+- \[0x([0-9A-Fa-f]+), (\w+), \"?([\w/]+)\"?\]", l)
        if m and m.group(2) in KINDS:
            out.append((m.group(2), m.group(3), int(m.group(1), 16) + base))
        m = re.match(r"\s+- \{ type: (\w+), vram: 0x([0-9A-Fa-f]+), name: ([\w/]+) \}", l)
        if m and m.group(1) in KINDS:
            out.append((m.group(1), m.group(3), int(m.group(2), 16)))
    return out

def symbols():
    """name -> PS2 address, from the symbol files and from D_XXXXXXXX names."""
    out = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        if not p.name.startswith("menu"):
            for l in p.read_text().splitlines():
                m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)", l)
                if m:
                    out[m.group(1)] = int(m.group(2), 16)
    return out

def convert(text, kind, addr):
    out = [f".section {SECTION[kind]}", ".p2align 4"]
    if addr & 15:
        out.append(f".space {addr & 15}")
    if not addr:
        m = re.search(r"dlabel D_([0-9A-F]{8})", text)
        addr = int(m.group(1), 16) if m else 0
        out = [f".section {SECTION[kind]}", ".p2align 4"] + ([f".space {addr & 15}"] if addr & 15 else [])
    for l in text.splitlines():
        l = re.sub(r"/\*.*?\*/", "", l).rstrip()
        s = l.strip()
        if not s or s.startswith((".include", ".section", "nonmatching", "enddlabel")):
            continue
        m = re.match(r"dlabel (\S+)", s)
        if m:
            out += [f".globl {m.group(1)}", f"{m.group(1)}:"]
        elif s.startswith(".word"):
            arg = s[5:].strip()
            out.append("    .long 0" if arg.startswith(".L") else f"    .long {arg}")
        elif s.startswith(".align"):
            out.append(f"    .p2align {s[6:].strip()}")
        elif s.startswith((".byte", ".short", ".float", ".asciz", ".ascii", ".space", ".double")):
            out.append("    " + s)
        else:
            raise SystemExit(f"unhandled line: {s}")
    return "\n".join(out) + "\n"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--asm", default=str(ROOT.parent / "bt3/asm"))
    a = ap.parse_args()
    dst = ROOT / "port/build/gen/data"
    dst.mkdir(parents=True, exist_ok=True)
    for old in dst.glob("*.s"):
        old.unlink()
    n = 0
    for kind, name, addr in chunks(ROOT / "config/SLUS_216.78.yaml"):
        src = pathlib.Path(a.asm) / "data" / f"{name}.{kind}.s"
        if not src.exists():
            raise SystemExit(f"missing {src}: run the decompilation's configure.py first")
        (dst / f"{name.replace('/', '_')}.{kind}.s").write_text(convert(src.read_text(), kind, addr))
        n += 1
    # The menu overlay (DBZP.BIN, loaded at 0x334C00 on the PS2): the same for its one assembly data chunk.
    for kind, name, addr in chunks(ROOT / "config/DBZP.yaml", 0x334C00):
        src = pathlib.Path(a.asm) / "dbzp/data" / f"{name}.{kind}.s"
        if not src.exists():
            raise SystemExit(f"missing {src}: run the decompilation's configure.py first")
        (dst / f"dbzp_{name.replace('/', '_')}.{kind}.s").write_text(convert(src.read_text(), kind, addr))
        n += 1
    # Tables that C files take from assembly with INCLUDE_RODATA (the PC build turns that macro into nothing).
    addr_of = symbols()
    m = 0
    for c in [ROOT / "src/main.c"] + sorted((ROOT / "src/sys").glob("*.c")) + sorted((ROOT / "src/battle").glob("*.c")):
        for folder, name in re.findall(r'^INCLUDE_RODATA\("([^"]+)", (\w+)\);', c.read_text(), re.M):
            if name.startswith("jtbl_"):
                continue  # jump table of an assembly function: the PC build compiles that function from C
            src = pathlib.Path(a.asm).parent / folder / f"{name}.s"
            (dst / f"inc_{name}.rodata.s").write_text(convert(src.read_text(), "rodata", addr_of.get(name, 0)))
            m += 1
    # The VU1 microprograms: one binary block with entry labels inside it.
    for c in sorted((ROOT / "src").rglob("*.c")) + sorted((ROOT / "include").rglob("*.h")):
        for h in re.findall(r"\bD_([0-9A-F]{8})\b", c.read_text()):
            addr_of.setdefault("D_" + h, int(h, 16))
    blob = pathlib.Path(a.asm).parent / "assets/cod/1BF6B0.textbin.bin"
    start, size = 0x2BF6B0, blob.stat().st_size
    cuts = sorted({v for v in addr_of.values() if start <= v < start + size} | {start})
    out = [".section .data", ".p2align 4"]
    names = collections.defaultdict(list)
    for k, v in addr_of.items():
        names[v].append(k)
    for i, c in enumerate(cuts):
        for k in sorted(names[c]) or [f"D_{c:08X}"]:
            out += [f".globl {k}", f"{k}:"]
        end = cuts[i + 1] if i + 1 < len(cuts) else start + size
        out.append(f'    .incbin "{blob}", {c - start}, {end - c}')
    (dst / "vu1_micro.data.s").write_text("\n".join(out) + "\n")
    print(f"{n} data chunks, {m} included tables and the VU1 microprograms written to {dst.relative_to(ROOT)}")
    # assemble them for the link (port/build/obj_data: the 32-bit Linux build's; the other builds assemble the
    # sources themselves, undefined.py, and macOS has no 32-bit x86 assembler)
    import subprocess
    if sys.platform == "darwin":
        return
    obj = ROOT / "port/build/obj_data"
    obj.mkdir(parents=True, exist_ok=True)
    for old in obj.glob("*.o"):
        old.unlink()
    for f in sorted(dst.glob("*.s")):
        r = subprocess.run(["as", "--32", str(f), "-o", str(obj / (f.name[:-2] + ".o"))], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(f"{f.name}: {r.stderr[:300]}")
    print(f"{len(list(obj.glob('*.o')))} data objects assembled")

main()
