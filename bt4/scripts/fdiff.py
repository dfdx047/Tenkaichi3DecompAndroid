#!/usr/bin/env python3
"""Compiles one C file and compares each of its functions against the original binary.

Usage: scripts/fdiff.py src/sys/heap.c [function ...]

Relocated fields (jump targets, %hi/%lo/%gp_rel immediates) are masked out, so a function can be
checked before the file is linked in. The linked build's byte comparison is the final word.
Target addresses come from the symbol files in config/, so a newly added name works without
re-running configure. The original bytes are read from disc/, not from build/.
"""

import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BINUTILS = "tools/binutils/mips-ps2-decompals-"
CC = "tools/ee-gcc2.96/bin/ee-gcc"
ELFS = ["build/SLUS_216.78.elf", "build/DBZP.elf"]
OVERLAY_BASE = 0x334C00  # DBZP.BIN load address

# Bits of an instruction word that a relocation fills in.
RELOC_MASK = {
    "R_MIPS_26": 0x03FFFFFF,
    "R_MIPS_HI16": 0xFFFF,
    "R_MIPS_LO16": 0xFFFF,
    "R_MIPS_GPREL16": 0xFFFF,
    "R_MIPS_LITERAL": 0xFFFF,
}


def sh(*cmd):
    return subprocess.run(cmd, cwd=ROOT, check=True, capture_output=True, text=True).stdout


def gflag(src):
    return "-G0" if any(d in str(src) for d in ("src/cri/", "src/menu/", "src/sys/late_a")) else "-G8"


def scratch(src):
    """Per-source scratch folder, so several runs can go in parallel."""
    return ROOT / "build" / "fdiff" / "_".join(Path(src).with_suffix("").parts)


def compile_c(src):
    out = scratch(src) / Path(src).with_suffix(".o").name
    out.parent.mkdir(parents=True, exist_ok=True)
    asm = out.with_suffix(".s")
    g = gflag(src)
    r = subprocess.run([CC, "-O2", "-fno-strict-aliasing", "-Iinclude", g, "-S", str(src), "-o", str(asm)],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)
    if r.stderr:
        print(r.stderr, file=sys.stderr)
    # Assembler-only exception, as AS_G_FLAGS in configure.py.
    if "src/menu/menu_t_b" in str(src):
        g = "-G8"
    sh(BINUTILS + "as", "-EL", "-march=r5900", "-mabi=o64", "-Iinclude", g, "-mno-pdr",
       "include/gcc_prelude.inc", str(asm), "-o", str(out))
    return out


def functions(obj):
    """name -> (offset, size) for functions defined in the object's .text."""
    funcs = {}
    for line in sh(BINUTILS + "readelf", "-sW", str(obj)).splitlines():
        p = line.split()
        if len(p) == 8 and p[3] == "FUNC" and p[6] != "UND":
            funcs[p[7]] = (int(p[1], 16), int(p[2]))
    return funcs


def reloc_masks(obj):
    """.text offset -> mask of relocated bits."""
    masks = {}
    in_text = False
    for line in sh(BINUTILS + "readelf", "-rW", str(obj)).splitlines():
        if line.startswith("Relocation section"):
            in_text = "'.rel.text'" in line
            continue
        p = line.split()
        if in_text and len(p) >= 3 and p[2] in RELOC_MASK:
            masks[int(p[0], 16)] = RELOC_MASK[p[2]]
    return masks


def text_bytes(obj):
    raw = obj.parent / "text.bin"
    sh(BINUTILS + "objcopy", "-O", "binary", "-j", ".text", str(obj), str(raw))
    return raw.read_bytes()


def targets():
    """name -> (address, elf) from the symbol files, falling back to the linked ELFs' symbols."""
    found = {}
    files = sorted((ROOT / "config").glob("symbol_addrs*.txt")) + \
        sorted((ROOT / "config" / "symbols").glob("*.txt"))
    for path in files:
        for line in path.read_text().splitlines():
            m = re.match(r"(\w+) = 0x([0-9A-Fa-f]+);", line)
            if m:
                addr = int(m.group(2), 16)
                found[m.group(1)] = (addr, ELFS[1] if addr >= OVERLAY_BASE else ELFS[0])
    for elf in ELFS:
        if not (ROOT / elf).exists():
            continue
        for line in sh(BINUTILS + "nm", elf).splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in "Tt":
                found.setdefault(p[2], (int(p[0], 16), elf))
    return found


def disasm(elf_or_obj, start, size, extra=()):
    out = sh(BINUTILS + "objdump", "-d", "-z", *extra, f"--start-address={start:#x}",
             f"--stop-address={start + size:#x}", str(elf_or_obj))
    lines = []
    for line in out.splitlines():
        if "\t" in line and ":" in line.split("\t")[0]:
            lines.append(" ".join(line.split("\t")[2:]))
    return lines


# Original images and their load addresses. Read from disc/ so the tool does not depend on the
# state of build/ (another process may be relinking).
ORIGINALS = {ELFS[0]: ("disc/SLUS_216.78.rom", 0x100000), ELFS[1]: ("disc/BIN/DBZP.BIN", OVERLAY_BASE)}


def original_words(tmp, elf, addr, size):
    path, base = ORIGINALS[elf]
    with open(ROOT / path, "rb") as f:
        f.seek(addr - base)
        data = f.read(size)
    return struct.unpack(f"<{len(data) // 4}I", data)


def original_disasm(tmp, elf, addr, size):
    path, base = ORIGINALS[elf]
    return disasm(ROOT / path, addr, size,
                  ("-D", "-b", "binary", "-m", "mips:5900", "-EL", f"--adjust-vma={base:#x}"))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = Path(sys.argv[1])
    wanted = sys.argv[2:]
    obj = compile_c(src)
    funcs = functions(obj)
    masks = reloc_masks(obj)
    text = text_bytes(obj)
    known = targets()

    ok = True
    for name, (off, size) in sorted(funcs.items(), key=lambda kv: kv[1]):
        if wanted and name not in wanted:
            continue
        if name not in known:
            print(f"{name}: no symbol with this name in the original; add it to the symbol files")
            ok = False
            continue
        addr, elf = known[name]
        mine = struct.unpack(f"<{size // 4}I", text[off:off + size])
        orig = original_words(obj.parent, elf, addr, size)
        bad = [i for i, (m, o) in enumerate(zip(mine, orig))
               if (m ^ o) & ~masks.get(off + i * 4, 0) & 0xFFFFFFFF]
        if not bad:
            print(f"{name}: OK ({size:#x} bytes)")
            continue
        ok = False
        print(f"{name}: {len(bad)} of {size // 4} instructions differ")
        a = original_disasm(obj.parent, elf, addr, size)
        b = disasm(obj, off, size)
        for i in range(max(len(a), len(b))):
            left = a[i] if i < len(a) else ""
            right = b[i] if i < len(b) else ""
            mark = "!" if i in bad else " "
            print(f"  {mark} {i * 4:4x}  {left:<34} | {right}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
