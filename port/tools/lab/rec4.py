#!/usr/bin/env python3
"""A pad recording of 2 ports per frame (0.9.0) made into one of 4 (1.0: src/sys/pad.c polls four). rec4.py <in> <out>"""
import sys
d = open(sys.argv[1], "rb").read()
idle = bytes.fromhex("ffff80808080") + bytes(12)
with open(sys.argv[2], "wb") as o:
    for i in range(0, len(d), 36):
        o.write(d[i:i + 36] + idle + idle)
