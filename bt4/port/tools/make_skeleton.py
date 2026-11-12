#!/usr/bin/env python3
"""Writes the game's data tables WITHOUT THEIR VALUES: port/data/*.s and port/data/index.txt.

The PC program needs the built-in data tables of the game's two programs (and the VU1 microprograms). Their values
are game data and come from the user's own disc: at start the program copies them in (Port_LoadGameData in
port/src/plat_mem.c). What a build needs is only their shape, and that is what this writes, from a build made
with the disc (gen_data.py's real tables, assembled in port/build/obj_data64):

    port/data/<name>.s    for every table file: its labels, and for every run of bytes one of
                              .space N         N bytes whose values come from the disc
                              .zero N          N bytes that are zero on PC (padding; addresses of PS2 code)
                              .long symbol     an address the linker fills in
    port/data/index.txt   for every file: which of the user's files its values come from and where
                          (0 = SLUS_216.78 as a flat image from 0x100000, 1 = BIN/DBZP.BIN), and a checksum of all
                          the values in order, so that a wrong disc is noticed at start

These files contain no value of the game's data and are committed; with them the port builds without a disc
(undefined.py uses them when port/build/gen/data does not exist, or with BT3_SKELETON=1) and make_dat.py writes
the list the program reads at start. Run this again after a change to gen_data.py or to the decompilation's
data split:      python3 port/tools/gen_data.py && BT3_CC=clang64 python3 port/tools/undefined.py
                 python3 port/tools/make_skeleton.py"""
import pathlib, re, struct, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
OBJ = ROOT / "port/build/obj_data64"
OUT = ROOT / "port/data"
ROM_BASE, ROM_END, DBZP_BASE = 0x100000, 0x2FF180, 0x334C00

def flat_image(elf):
    b = elf.read_bytes()
    shoff, = struct.unpack_from("<I", b, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", b, 0x2E)
    out = bytearray(ROM_END - ROM_BASE)
    for i in range(shnum):
        _, kind, flags, addr, off, size = struct.unpack_from("<6I", b, shoff + i * shentsize)
        if flags & 2 and kind != 8 and size and ROM_BASE <= addr and addr + size <= ROM_END:
            out[addr - ROM_BASE:addr - ROM_BASE + size] = b[off:off + size]
    return bytes(out)

def fnv(data, h):
    for byte in data:
        h = ((h ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h

def read_object(path):
    """(section name, its bytes or None for .bss, size, [(offset, name, is global)], {offset: 'symbol+addend'})"""
    b = path.read_bytes()
    shoff, = struct.unpack_from("<Q", b, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", b, 0x3A)
    secs = [struct.unpack_from("<IIQQQQIIQQ", b, shoff + i * shentsize) for i in range(shnum)]
    strtab = lambda sec, off: b[secs[sec][4] + off:b.index(b"\0", secs[sec][4] + off)].decode()
    names = [strtab(shstrndx, s[0]) for s in secs]
    target = next((i for i, n in enumerate(names) if n == ".data" and secs[i][5]), None)
    if target is None:
        target = next(i for i, n in enumerate(names) if n == ".bss" and secs[i][5])
    symsec = names.index(".symtab")
    symbols, labels = [], []
    for k in range(secs[symsec][5] // 24):
        name, info, _, shndx, value, _ = struct.unpack_from("<IBBHQQ", b, secs[symsec][4] + k * 24)
        symbols.append(strtab(secs[symsec][6], name))
        if shndx == target and symbols[-1]:
            labels.append((value, symbols[-1], info >> 4 == 1))
    relocs = {}
    for i, n in enumerate(names):
        if n == ".rela" + names[target]:
            for k in range(secs[i][5] // 24):
                off, info, addend = struct.unpack_from("<QQq", b, secs[i][4] + k * 24)
                if info & 0xFFFFFFFF != 10:  # R_X86_64_32
                    raise SystemExit(f"{path.name}: relocation type {info & 0xFFFFFFFF} at {off:#x}")
                sym = symbols[info >> 32]
                if not sym:
                    raise SystemExit(f"{path.name}: relocation against a section at {off:#x}")
                relocs[off] = sym + (f"+{addend}" if addend > 0 else f"{addend}" if addend < 0 else "")
    data = b[secs[target][4]:secs[target][4] + secs[target][5]] if names[target] == ".data" else None
    return names[target], data, secs[target][5], sorted(labels), relocs

def main():
    known = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        for l in p.read_text().splitlines():
            m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)", l)
            if m:
                known[m.group(1)] = int(m.group(2), 16)
    src = [flat_image(ROOT / "gamedata/disc/SLUS_216.78"), (ROOT / "gamedata/disc/BIN/DBZP.BIN").read_bytes()]
    base = [ROM_BASE, DBZP_BASE]
    OUT.mkdir(exist_ok=True)
    for old in OUT.glob("*.s"):
        old.unlink()
    index, h, copied, zeroed, pointers = [], 0xCBF29CE484222325, 0, 0, 0
    for path in sorted(OBJ.glob("*.o")):
        sec, data, size, labels, relocs = read_object(path)
        name = path.name[:-2]
        out = [f"# {name}: the shape of this table, without its values (port/tools/make_skeleton.py)", f".section {sec}", ".p2align 4"]
        at = {}
        for off, label, is_global in labels:
            at.setdefault(off, []).append((label, is_global))
        which = so = None
        if data is not None:
            # where in the user's files this table is: from a label with a known PS2 address, or from the file's name
            cand = set()
            for off, label, _ in labels:
                m = re.fullmatch(r"D_([0-9A-F]{8})", label)
                ps2 = int(m.group(1), 16) if m else known.get(label)
                if ps2:
                    cand.add(ps2 - off)
            m = re.search(r"cod_([0-9A-F]{6,8})\.", path.name)
            if m:
                ps2 = int(m.group(1), 16) + (ROM_BASE if len(m.group(1)) == 6 else 0)
                cand.add(ps2 - (ps2 & 15))
            best = None
            for ps2 in cand:
                w = 1 if ps2 >= DBZP_BASE else 0
                o = ps2 - base[w]
                if o < 0 or o + size > len(src[w]):
                    continue
                same = sum(1 for i in range(size) if data[i] == src[w][o + i])
                if best is None or same > best[0]:
                    best = (same, w, o)
            if best is None or (best[0] < size // 2 and size > 64):
                raise SystemExit(f"{path.name}: not found in the game's programs")
            _, which, so = best
        i = 0
        while i < size or i in at:
            for label, is_global in at.get(i, []):
                out += ([f".globl {label}"] if is_global else []) + [f"{label}:"]
            if i >= size:
                break
            if i in relocs:
                out.append(f"    .long {relocs[i]}")
                pointers += 1
                i += 4
                continue
            # a run up to the next label or address, of one kind: values of the disc, or zero on PC
            copy = data is not None and data[i] == src[which][so + i]
            j = i
            while j < size and j not in relocs and (j == i or j not in at) and \
                    (data is not None and data[j] == src[which][so + j]) == copy:
                j += 1
            if copy:
                out.append(f"    .space {j - i}")
                h = fnv(data[i:j], h)
                copied += j - i
            else:
                if data is not None and any(data[i:j]):
                    raise SystemExit(f"{path.name}: bytes at {i:#x} are neither the disc's nor zero")
                out.append(f"    .zero {j - i}")
                zeroed += j - i
            i = j
        (OUT / (name + ".s")).write_text("\n".join(out) + "\n")
        if data is not None:
            index.append(f"{name} {which} {so:#x}")
    (OUT / "index.txt").write_text("# table file, source (0 SLUS_216.78 flat from 0x100000, 1 BIN/DBZP.BIN), offset in it of the table's first byte\n" +
                                   "\n".join(index) + f"\nchecksum {h:#018x}\n")
    print(f"{len(list(OUT.glob('*.s')))} table files in port/data: {copied} bytes come from the disc, {zeroed} are zero, {pointers} addresses; "
          f"checksum {h:#018x}")

main()
