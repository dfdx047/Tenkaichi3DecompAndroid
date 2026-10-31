#!/usr/bin/env python3
"""A table for BT3_NET_SCRIPT: 36 bytes per blank (pad 1, pad 2), the buttons pressed at random. nettable.py <out> <blanks> <seed>"""
import random, sys
out, blanks, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
r = random.Random(seed)
BTN = [0x0010, 0x0020, 0x0040, 0x0080, 0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0400, 0x0800, 0x0008]  # d-pad, shoulders, face, start
def pad(state):
    if state["left"] == 0:
        state["btn"] = r.choice(BTN) if r.random() < 0.8 else 0
        state["left"] = r.randint(3, 25)
    state["left"] -= 1
    word = 0xFFFF ^ state["btn"]
    return bytes([word & 0xFF, word >> 8, 0x80, 0x80, 0x80, 0x80]) + bytes(12)
a, b = {"left": 0, "btn": 0}, {"left": 0, "btn": 0}
with open(out, "wb") as f:
    for _ in range(blanks):
        f.write(pad(a) + pad(b))
