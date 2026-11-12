#!/usr/bin/env python3
"""Packs the libm test driver and the user's game executable into one PS2 ELF: the game's two load segments at
their own addresses (so its routines can be called), the driver at 0x01000000 as the entry point, with $gp set
to the game's value. Usage: pack_libm.py <SLUS_216.78> <driver.elf> <out.elf>"""
import struct, sys
game = open(sys.argv[1], "rb").read()
drv = open(sys.argv[2], "rb").read()

def loads(elf):
    phoff, = struct.unpack_from("<I", elf, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x2A)
    out = []
    for i in range(phnum):
        t, off, va, pa, fs, ms, fl, al = struct.unpack_from("<8I", elf, phoff + i * phentsize)
        if t == 1 and ms:
            out.append((va, elf[off:off + fs], ms))
    return out
segs = loads(game) + loads(drv)  # the driver is an ELF too: its own segments and entry point
entry, = struct.unpack_from("<I", drv, 0x18)
hdr = bytearray(game[:0x34])
struct.pack_into("<I", hdr, 0x18, entry)
struct.pack_into("<I", hdr, 0x1C, 0x34)                # program headers right behind the ELF header
struct.pack_into("<I", hdr, 0x20, 0)                   # no section headers
struct.pack_into("<HHHHH", hdr, 0x2A, 0x20, len(segs), 0x28, 0, 0)
out = bytearray(hdr) + bytearray(0x20 * len(segs))
for i, (va, data, ms) in enumerate(segs):
    while len(out) % 0x1000:
        out.append(0)
    struct.pack_into("<8I", out, 0x34 + i * 0x20, 1, len(out), va, va, len(data), ms, 7, 0x1000)
    out += data
open(sys.argv[3], "wb").write(out)
