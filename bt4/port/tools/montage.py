#!/usr/bin/env python3
"""Contact sheet of screenshots: montage.py out.png cols scale file...  (each tile is the image shrunk by `scale`,
labelled by position only: tiles are in file order, left to right, top to bottom)."""
import struct, sys, zlib
out, cols, scale, files = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4:]
tiles = []
for f in files:
    d = open(f, "rb").read()
    h = d.split(b"\n", 3)
    w, hh = map(int, h[1].split())
    b = h[3]
    tw, th = w // scale, hh // scale
    rows = [bytes(b[((y * scale) * w + x * scale) * 3 + c] for x in range(tw) for c in range(3)) for y in range(th)]
    tiles.append((tw, th, rows))
tw, th = tiles[0][0], tiles[0][1]
rows_n = (len(tiles) + cols - 1) // cols
W, H = cols * (tw + 4), rows_n * (th + 4)
img = [bytearray(b"\x40" * (W * 3)) for _ in range(H)]
for i, (_, _, rows) in enumerate(tiles):
    ox, oy = (i % cols) * (tw + 4), (i // cols) * (th + 4)
    for y, r in enumerate(rows):
        img[oy + y][ox * 3:ox * 3 + len(r)] = r
raw = b"".join(b"\0" + bytes(r) for r in img)
ch = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
open(out, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))
print(len(tiles), "tiles", W, "x", H)
