#!/usr/bin/env python3
"""Lists or extracts the files of a PS2 memory card image (PCSX2 .ps2, with or without the ECC bytes).
Usage: port/tools/mc_extract.py <card.ps2> [out_dir]     (no out_dir: list only)"""
import pathlib, struct, sys

class Card:
    def __init__(self, path):
        self.d = pathlib.Path(path).read_bytes()
        sb = self.d[:0x154]
        if not sb.startswith(b"Sony PS2 Memory Card Format "):
            raise SystemExit("not a PS2 memory card image")
        self.page, self.ppc = struct.unpack_from("<HH", sb, 0x28)
        (self.clusters,) = struct.unpack_from("<I", sb, 0x30)
        self.alloc, _, self.root = struct.unpack_from("<III", sb, 0x34)
        self.ifc = struct.unpack_from("<32I", sb, 0x50)
        self.csize = self.page * self.ppc
        pages = self.clusters * self.ppc
        self.raw = self.page + 16 if len(self.d) >= pages * (self.page + 16) else self.page
        self.per = self.csize // 4

    def cluster(self, n):
        return b"".join(self.d[(n * self.ppc + i) * self.raw:(n * self.ppc + i) * self.raw + self.page]
                        for i in range(self.ppc))

    def fat(self, n):
        ind = struct.unpack_from(f"<{self.per}I", self.cluster(self.ifc[n // (self.per * self.per)]))
        tbl = struct.unpack_from(f"<{self.per}I", self.cluster(ind[(n // self.per) % self.per]))
        return tbl[n % self.per]

    def read(self, first, length):
        out, c = b"", first
        while len(out) < length:
            out += self.cluster(self.alloc + c)
            e = self.fat(c)
            if e == 0xFFFFFFFF or not e & 0x80000000:
                break
            c = e & 0x7FFFFFFF
        return out[:length]

    def entries(self, first, count):
        raw = self.read(first, count * 512)
        for i in range(len(raw) // 512):
            e = raw[i * 512:(i + 1) * 512]
            mode, length, cl = struct.unpack_from("<H2xI8xI", e, 0)
            name = e[0x40:0x60].split(b"\0")[0].decode("ascii", "replace")
            if mode & 0x8000 and name not in (".", ".."):
                yield mode, length, cl, name

    def walk(self, first=None, count=None, prefix=""):
        if first is None:
            first = self.root
            (count,) = struct.unpack_from("<I", self.cluster(self.alloc + first), 4)
        for mode, length, cl, name in self.entries(first, count):
            if mode & 0x20:
                yield from self.walk(cl, length, prefix + name + "/")
            else:
                yield prefix + name, length, cl

def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    card = Card(sys.argv[1])
    out = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None
    for name, length, cl in card.walk():
        print(f"{length:8d}  {name}")
        if out:
            p = out / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(card.read(cl, length))

main()
