#!/usr/bin/env python3
"""Makes the program that can be given to other people: the game's own data taken out of it.

The linked program (link.py) carries the data tables of the game's two programs, assembled from the user's own
executable (gen_data.py), and the VU1 microprograms. This tool finds those bytes in the linked program, sets them to
zero, and writes a list of where they came from, so that the program can fetch them again from the user's disc
when it starts (Port_LoadGameData in port/src/plat_mem.c):

    port/tools/strip_data.py [--exe port/build/bt3_64] [--out port/build/release]
        -> <out>/bt3        the program without the game's data
           <out>/bt3.dat    the list: (address in the program, source file, offset in it, length) and a checksum

How: the linker's map says where each data object landed; a symbol with a known PS2 address inside it gives the
distance between program address and PS2 address; every run of bytes equal to the user's executable at that PS2
address is game data. Bytes that differ are addresses the linker filled in (they belong to this build, not to the
game) and stay. The list contains no game data, only numbers.
Sources: 0 = SLUS_216.78 as a flat image from 0x100000, 1 = BIN/DBZP.BIN (loaded at 0x334C00 on the PS2)."""
import argparse, pathlib, re, struct, subprocess, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import toolchain
from toolchain import ROOT, PREFIX
ROM_BASE, ROM_END, DBZP_BASE = 0x100000, 0x2FF180, 0x334C00
MIN_RUN = 4  # shorter runs of equal bytes are left (a coincidence inside an address)

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

def fnv(data, h=0xCBF29CE484222325):
    for i in range(0, len(data), 1 << 16):  # same result as byte by byte, done in chunks to keep Python quick
        for byte in data[i:i + (1 << 16)]:
            h = ((h ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(toolchain.EXE if toolchain.BITS64 else ROOT / "port/build/bt3_64"))
    ap.add_argument("--out", default=str(ROOT / ("port/build/release-win" if toolchain.WIN else "port/build/release")))
    ap.add_argument("--data", default=str(ROOT / "gamedata"))
    a = ap.parse_args()
    exe = pathlib.Path(a.exe)
    image = bytearray(exe.read_bytes())
    src = [flat_image(pathlib.Path(a.data) / "disc/SLUS_216.78"), (pathlib.Path(a.data) / "disc/BIN/DBZP.BIN").read_bytes()]
    base = [ROM_BASE, DBZP_BASE]
    # loaded sections of the program: address -> file offset
    secs = []
    # (objdump -h reads both kinds of program: ELF and Windows PE. Columns: index, name, size, address, load
    # address, file offset; a section with CONTENTS in the next line has bytes in the file.)
    lines = subprocess.run([PREFIX + "objdump", "-h", str(exe)], capture_output=True, text=True).stdout.splitlines()
    for k, l in enumerate(lines):
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", l)
        if m and k + 1 < len(lines) and "CONTENTS" in lines[k + 1]:
            secs.append((int(m.group(3), 16), int(m.group(5), 16), int(m.group(2), 16)))
    def file_off(addr):
        for va, off, size in secs:
            if va <= addr < va + size:
                return off + addr - va
        return None
    # symbols with a known PS2 address: D_XXXXXXXX names and the decompilation's symbol files
    known = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        for l in p.read_text().splitlines():
            m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)", l)
            if m:
                known[m.group(1)] = int(m.group(2), 16)
    syms = []
    for l in subprocess.run([PREFIX + "nm", str(exe)], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3:
            m = re.fullmatch(r"D_([0-9A-F]{8})", p[2])
            ps2 = int(m.group(1), 16) if m else known.get(p[2])
            if ps2:
                syms.append((int(p[0], 16), ps2))
    syms.sort()
    # where each data object landed
    pieces = []
    for l in open(str(exe) + ".map"):
        m = re.match(r"^\s*(\.\S+)?\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\S*(obj_data\w*/\S+\.o)\s*$", l)
        if m and m.group(1) in (".data", ".rodata") and int(m.group(3), 16):
            pieces.append((int(m.group(2), 16), int(m.group(3), 16), m.group(4)))
    records, taken, total, odd = [], bytearray(), 0, []
    for start, size, name in pieces:
        total += size
        inside = [(h, p) for h, p in syms if start <= h < start + size]
        deltas = {h - p for h, p in inside}
        m = re.search(r"/cod_([0-9A-F]{6,8})\.", name)  # the object's name holds its place: offset in the file, or address
        if m:
            ps2 = int(m.group(1), 16) + (ROM_BASE if len(m.group(1)) == 6 else 0)
            deltas.add(start + (ps2 & 15) - ps2)  # gen_data.py pads a piece to its PS2 offset within 16 bytes
        best = None
        for delta in deltas:  # one piece comes from one place: the distance most of its bytes agree with
            which = 1 if start - delta >= DBZP_BASE else 0
            so = start - delta - base[which]
            if so < 0 or so + size > len(src[which]):
                continue
            off = file_off(start)
            same = sum(1 for i in range(size) if image[off + i] == src[which][so + i])
            if best is None or same > best[0]:
                best = (same, which, so, off)
        if best is None or (best[0] < size // 2 and size > 64):  # a small piece is mostly its own padding
            odd.append((name, size, best[0] if best else -1))
            continue
        _, which, so, off = best
        i = 0
        while i < size:
            if image[off + i] != src[which][so + i]:
                i += 1
                continue
            j = i
            while j < size and image[off + j] == src[which][so + j]:
                j += 1
            run = bytes(image[off + i:off + j])
            if j - i >= MIN_RUN and any(run):
                records.append((start + i, which, so + i, j - i))
                taken += run
                image[off + i:off + j] = bytes(j - i)
            i = j
    flag = next((int(l.split()[0], 16) for l in subprocess.run([PREFIX + "nm", str(exe)], capture_output=True, text=True).stdout.splitlines()
                 if l.endswith(" gPortDataStripped")), None)
    if flag is None or file_off(flag) is None:
        raise SystemExit("gPortDataStripped not found in the program's data: is this a current build?")
    image[file_off(flag):file_off(flag) + 4] = struct.pack("<I", 1)  # the program now insists on its list
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    prog = out / ("bt3.exe" if exe.suffix == ".exe" else "bt3")
    prog.write_bytes(image)
    prog.chmod(0o755)
    head = struct.pack("<4sIQ", b"BT3D", len(records), fnv(bytes(taken)))
    (out / "bt3.dat").write_bytes(head + b"".join(struct.pack("<IIII", *r) for r in records))
    print(f"{len(pieces)} data objects, {total} bytes; {len(taken)} bytes of game data taken out in {len(records)} runs "
          f"({total - len(taken)} bytes stay: addresses of this build and zeros)")
    for name, size, same in odd:
        print(f"  NOT matched to the game's executable: {name} ({size} bytes, {same} equal)")
    print(f"wrote {prog} and {out / 'bt3.dat'} ({(out / 'bt3.dat').stat().st_size} bytes)")

main()
