#!/usr/bin/env python3
"""Writes port/src/plat_verify_tab.h: the CRC-32 of every data file of the original disc.

    python3 port/tools/make_verify.py <disc image of SLUS-21678>

The game compares its data folder with this table at start (port/src/plat_verify.c) and says so when the data is
not the original disc's: a modified disc image looks like the port's fault in a screenshot otherwise. The table
holds checksums only, no game data. The entries are in the order the game checks them: every entry of the three
AFS archives (DATA/PZS3US0..2.AFS), then the loose streams of DATA/."""
import struct, sys, zlib
from pathlib import Path

SECTOR = 2048
LOOSE = ["DATA/ZS3USED.ADX", "DATA/ZS3USED.PSS", "DATA/ZS3USOP.ADX", "DATA/ZS3USOP.PSS"]


def iso_files(f):
    """{upper-case path: (offset, size)} of an ISO 9660 image."""
    out = {}
    f.seek(16 * SECTOR + 156)
    root = f.read(34)

    def walk(lba, size, prefix):
        f.seek(lba * SECTOR)
        data = f.read(size)
        p = 0
        while p < len(data):
            n = data[p]
            if n == 0:
                p = (p // SECTOR + 1) * SECTOR
                continue
            rec = data[p:p + n]
            p += n
            lba2, size2 = struct.unpack_from("<I", rec, 2)[0], struct.unpack_from("<I", rec, 10)[0]
            name = rec[33:33 + rec[32]]
            if name in (b"\0", b"\1"):
                continue
            name = name.decode("ascii").split(";")[0].upper()
            if rec[25] & 2:
                walk(lba2, size2, prefix + name + "/")
            else:
                out[prefix + name] = (lba2 * SECTOR, size2)

    walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], "")
    return out


def crc_range(f, offset, size):
    f.seek(offset)
    c = 0
    while size > 0:
        b = f.read(min(size, 1 << 20))
        if not b:
            raise SystemExit("the image ends inside a file")
        c = zlib.crc32(b, c)
        size -= len(b)
    return c


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    lines, counts = [], []
    with open(sys.argv[1], "rb") as f:
        files = iso_files(f)
        for part in range(3):
            off, size = files["DATA/PZS3US%d.AFS" % part]
            f.seek(off)
            magic, count = struct.unpack("<4sI", f.read(8))
            assert magic == b"AFS\0"
            table = struct.unpack("<%dI" % (count * 2), f.read(count * 8))
            counts.append(count)
            for i in range(count):
                lines.append((table[i * 2 + 1], crc_range(f, off + table[i * 2], table[i * 2 + 1])))
        for name in LOOSE:
            off, size = files[name]
            lines.append((size, crc_range(f, off, size)))
    out = Path(__file__).resolve().parents[1] / "src/plat_verify_tab.h"
    with open(out, "w") as o:
        o.write("/* Made by port/tools/make_verify.py from the disc image of SLUS-21678: (size, CRC-32) of every data file,\n"
                "   in the order plat_verify.c checks them. Checksums only. */\n")
        o.write("static const int kVerifyCount[3] = { %d, %d, %d };\n" % tuple(counts))
        o.write("static const char *const kVerifyLoose[%d] = { %s };\n" % (len(LOOSE), ", ".join('"disc/%s"' % n for n in LOOSE)))
        o.write("static const unsigned int kVerify[%d][2] = {\n" % len(lines))
        for i in range(0, len(lines), 6):
            o.write("    " + " ".join("{%u,0x%08X}," % l for l in lines[i:i + 6]) + "\n")
        o.write("};\n")
    print("%s: %d files" % (out, len(lines)))


main()
