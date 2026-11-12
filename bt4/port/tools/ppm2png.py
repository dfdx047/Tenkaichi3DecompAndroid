#!/usr/bin/env python3
"""Converts the reference renderer's screenshots (port/build/shots/*.ppm) to PNG. Usage: ppm2png.py [files...]"""
import glob, os, struct, sys, zlib
def png(path, w, h, rgb):
    raw = b"".join(b"\0" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def ch(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                           ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))
for f in sys.argv[1:] or sorted(glob.glob("port/build/shots/*.ppm")):
    d = open(f, "rb").read()
    hdr = d.split(b"\n", 3)
    w, h = map(int, hdr[1].split())
    png(f[:-4] + ".png", w, h, hdr[3])
    print(os.path.basename(f), "lit" if any(hdr[3][i] for i in range(0, len(hdr[3]), 991)) else "black")
