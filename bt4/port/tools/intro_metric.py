#!/usr/bin/env python3
"""How close is the PC build to the console during the pre-fight countdown? Reads a BT3_TRACE=1 trace on stdin and
prints the frame whose two fighter positions are closest to those in the console state gamedata/validation/play01
(taken 55 frames into the countdown), with the summed absolute difference; 0 with "EXACT" means bit-identical."""
import struct, sys
ee = open("gamedata/validation/play01/eeMemory.bin", "rb").read()
c = struct.unpack_from("<3I", ee, 0x18706D0) + struct.unpack_from("<3I", ee, 0x1871CD0)
f = lambda u: struct.unpack("<f", struct.pack("<I", u))[0]
best = None
for l in sys.stdin:
    t = l.split()
    if "p0" not in t or t[3] not in ("2", "3"):
        continue
    i, j = t.index("p0"), t.index("p1")
    p = [int(x, 16) for x in t[i + 1:i + 4] + t[j + 1:j + 4]]
    d = sum(abs(f(a) - f(b)) for a, b in zip(p, c))
    if best is None or d < best[0]:
        best = (d, t[1], sum(a != b for a, b in zip(p, c)))
print(f"closest frame: vblank {best[1]}, difference {best[0]:.6f}, {6 - best[2]} of 6 words identical" + (" EXACT" if best[2] == 0 else ""))
