#!/usr/bin/env python3
"""Finishes a program built from the blank data tables (port/data, see make_skeleton.py): writes the list of
where every value of those tables comes from on the user's disc, which the program reads at start.

    [BT3_CC=...] port/tools/make_dat.py [--exe <program>] [--out <folder>]
        -> the program marked as one that needs its list, and next to it <program>.dat:
           (address in the program, source file, offset in it, length) for every `.space` run of port/data/*.s,
           and the checksum of port/data/index.txt. link.py runs this by itself after linking such a program.

No disc is needed for this: the addresses come from the linker's map, the rest from port/data. (strip_data.py does
the same for a program that was built WITH the tables' values, by comparing it with the disc.)"""
import argparse, pathlib, re, struct, subprocess, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import toolchain
from toolchain import ROOT, PREFIX

def mac_placed(exe):
    """macOS: ld64's map lists symbols, not input sections. A table's place: its object's first symbol (the program's addresses made game addresses: what the program reads)."""
    text = pathlib.Path(str(exe) + ".map").read_text(errors="replace")
    files = {int(m.group(1)): m.group(2) for m in re.finditer(r"^\[\s*(\d+)\] (.*)$", text, re.M)}
    syms = text[text.index("# Symbols:"):]
    window = int(re.search(r"^0x([0-9A-Fa-f]+)\s+0x0+\s+\[\s*\d+\] __mh_execute_header$", syms, re.M).group(1), 16) - 0x20000000
    data = re.search(r"^0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+__DATA\s+__data$", text, re.M)  # (as on Linux: the .data tables only)
    d0, d1 = int(data.group(1), 16), int(data.group(1), 16) + int(data.group(2), 16)
    placed = {}
    for m in re.finditer(r"^0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+\[\s*(\d+)\] \S+$", syms, re.M):
        f = pathlib.Path(files.get(int(m.group(3)), ""))
        if f.parent == toolchain.DATA and d0 <= int(m.group(1), 16) < d1:
            a, e = int(m.group(1), 16) - window, int(m.group(1), 16) - window + int(m.group(2), 16)
            lo, hi = placed.get(f.stem, (a, e))
            placed[f.stem] = (min(lo, a), max(hi, e))
    # (a symbol's size in the map runs to the next symbol, past the end of its object: the length is the object's)
    out = {}
    for k, (lo, _) in placed.items():
        sizes = [int(s, 16) for s in re.findall(r"sectname __data\n\s+segname \S+\n\s+addr \S+\n\s+size (\S+)",
                                                 subprocess.run(["otool", "-l", str(toolchain.DATA / (k + ".o"))], capture_output=True, text=True).stdout)]
        if sizes and sizes[0]:
            out[k] = (lo, sizes[0])
    return out

def mac_sections(exe):
    """macOS: (address, file offset, size) of each section with contents, from otool -l."""
    secs, cur = [], {}
    for l in subprocess.run(["otool", "-l", str(exe)], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 2 and p[0] in ("sectname", "addr", "size", "offset"):
            cur[p[0]] = p[1]
            if p[0] == "offset" and int(p[1]) != 0:
                secs.append((int(cur["addr"], 16), int(p[1]), int(cur["size"], 16)))
    return secs

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(toolchain.EXE))
    ap.add_argument("--out", default=None, help="folder for bt3 + bt3.dat; without it the program is marked in place and gets <program>.dat next to it")
    a = ap.parse_args()
    exe = pathlib.Path(a.exe)
    image = bytearray(exe.read_bytes())
    where, checksum = {}, None
    for l in (ROOT / "port/data/index.txt").read_text().splitlines():
        p = l.split()
        if len(p) == 3 and not l.startswith("#"):
            where[p[0]] = (int(p[1]), int(p[2], 16))
        elif len(p) == 2 and p[0] == "checksum":
            checksum = int(p[1], 16)
    placed = mac_placed(exe) if toolchain.MAC else {}
    for l in open(str(exe) + ".map") if not toolchain.MAC else ():
        m = re.match(r"^\s*\.data\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\S*obj_data\w*/(\S+)\.o\s*$", l)
        if m and int(m.group(2), 16):
            placed[m.group(3)] = (int(m.group(1), 16), int(m.group(2), 16))
    records = []
    for name in sorted(where):
        if name not in placed:
            raise SystemExit(f"{name}: not in {exe.name}.map (was the program built from port/data?)")
        which, so = where[name]
        start, size = placed[name]
        off = 0
        for l in (ROOT / "port/data" / (name + ".s")).read_text().splitlines():
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
        if not off <= size < off + 16:  # (a Windows object's section is padded to its 16-byte alignment)
            raise SystemExit(f"{name}: {off} bytes in port/data, {size} in the program")
    # the mark that this program does not work without its list (gPortDataStripped, plat_mem.c)
    secs = mac_sections(exe) if toolchain.MAC else []
    lines = [] if toolchain.MAC else subprocess.run([PREFIX + "objdump", "-h", str(exe)], capture_output=True, text=True).stdout.splitlines()
    for k, l in enumerate(lines):
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", l)
        if m and k + 1 < len(lines) and "CONTENTS" in lines[k + 1]:
            secs.append((int(m.group(3), 16), int(m.group(5), 16), int(m.group(2), 16)))
    flag = next((int(l.split()[0], 16) for l in subprocess.run([toolchain.llvm("llvm-nm") if toolchain.MAC else PREFIX + "nm", str(exe)],
                                                              capture_output=True, text=True).stdout.splitlines()
                 if l.endswith(" _gPortDataStripped" if toolchain.MAC else " gPortDataStripped")), None)
    off = next((o + flag - va for va, o, size in secs if flag is not None and va <= flag < va + size), None)
    if off is None:
        raise SystemExit("gPortDataStripped not found in the program's data")
    image[off:off + 4] = struct.pack("<I", 1)
    if a.out is None:  # in place: bt3_64 + bt3_64.dat, bt3.exe + bt3.dat
        prog, dat = exe, pathlib.Path(str(exe)[:-4] + ".dat" if exe.suffix == ".exe" else str(exe) + ".dat")
    else:
        out = pathlib.Path(a.out)
        out.mkdir(parents=True, exist_ok=True)
        prog = out / ("bt3.exe" if exe.suffix == ".exe" else "bt3")
        dat = out / "bt3.dat"
    prog.write_bytes(image)
    prog.chmod(0o755)
    if toolchain.MAC:  # arm64 programs must be signed, and the mark changed their bytes: signed again (ad hoc)
        subprocess.run(["codesign", "--force", "--sign", "-", str(prog)], check=True, capture_output=True)
    dat.write_bytes(struct.pack("<4sIQ", b"BT3D", len(records), checksum) + b"".join(struct.pack("<IIII", *r) for r in records))
    print(f"{len(placed)} tables, {sum(r[3] for r in records)} bytes to fetch from the disc in {len(records)} runs; {dat.relative_to(ROOT) if dat.is_relative_to(ROOT) else dat}")

main()
