#!/usr/bin/env python3
"""Writes port/src/plat_adxlen_tab.h: how long every stream of the original disc lasts, in vertical blanks.

    python3 port/tools/make_adxlen.py [<data folder of the original disc, default gamedata>]

The game's logic waits for voice lines to end (the fight's intro and win talk, the menus' guides). In an online
match that wait must be the same on both machines whatever sound files each player has (another voice set, a
voice mod under mods/), so there it is counted from this table and not from the local file
(port/src/gs/snd_adx.c, view_start). Lengths only, no game data: one number per file of the third archive
(0 = not a stream, -1 = it loops) and the two loose movie tracks. The formula is adx_blanks' of snd_adx.c: with
the original files the table says exactly what the file's own header says."""
import struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOOSE = ["ZS3USED.ADX", "ZS3USOP.ADX"]


def blanks(path):
    try:
        h = path.open("rb").read(0x40)
    except OSError:
        return 0
    if len(h) < 0x40 or struct.unpack_from(">H", h, 0)[0] != 0x8000:
        return 0
    header = struct.unpack_from(">H", h, 2)[0] + 4
    rate, total = struct.unpack_from(">II", h, 8)
    if h[0x12] == 4 and header >= 0x38 and any(h[0x24:0x28]):
        return -1
    return 0 if rate == 0 else (total * 60000 + rate * 1001 - 1) // (rate * 1001) + 1


def main():
    data = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "gamedata"
    files = sorted((data / "pzs3us2").glob("[0-9][0-9][0-9][0-9][0-9].bin"))
    count = int(files[-1].stem) + 1
    tab = [0] * count
    for f in files:
        tab[int(f.stem)] = blanks(f)
    loose = [blanks(data / "disc" / "DATA" / n) for n in LOOSE]
    assert max(tab) < 32768 and min(tab) >= -1, "a length does not fit a short"
    out = ["/* Made by port/tools/make_adxlen.py from the data of SLUS-21678: how long each stream lasts in vertical blanks",
           "   (59.94 a second), by file number of the third archive (0 = not a stream, -1 = it loops), and the two loose",
           "   movie tracks. Lengths only. */",
           f"enum {{ ADXLEN_COUNT = {count} }};",
           "static const struct { const char *name; int blanks; } kAdxLenLoose[%d] = { %s };" % (len(LOOSE), ", ".join('{"%s", %d}' % p for p in zip(LOOSE, loose))),
           "static const short kAdxLen[ADXLEN_COUNT] = {"]
    for i in range(0, count, 24):
        out.append("    " + ",".join(str(v) for v in tab[i:i + 24]) + ",")
    out.append("};")
    (ROOT / "port/src/plat_adxlen_tab.h").write_text("\n".join(out) + "\n")
    print(f"{count} files: {sum(1 for v in tab if v > 0)} streams with an end, {sum(1 for v in tab if v < 0)} that loop, the longest {max(tab)} blanks; loose {loose}")


main()
