#!/usr/bin/env python3
"""fightcmp.py a/hash.txt b/hash.txt: do two runs of the same recorded fight (BT3_HASH) simulate the same fight?
Compares the fight's own values (the third column: hp, positions, clock, random state) blank by blank, from the
first blank of the fight in each run, so a different loading time before it does not count. The whole-state
checksum (second column) is left out: a build with new variables has another memory layout."""
import sys


def fight(path):
    out = []
    for ln in open(path, errors="replace"):
        f = ln.split()
        if len(f) >= 3 and f[2] != "0000000000000000":
            out.append((int(f[0]), f[2], " ".join(f[3:])))
    return out


a, b = fight(sys.argv[1]), fight(sys.argv[2])
n = min(len(a), len(b))
print(f"fight blanks: {len(a)} and {len(b)}; first blank {a[0][0] if a else '-'} and {b[0][0] if b else '-'}")
for i in range(n):
    if a[i][1] != b[i][1]:
        print(f"DIFFER at fight blank {i} (blanks {a[i][0]} / {b[i][0]}):\n  {a[i][2]}\n  {b[i][2]}")
        sys.exit(1)
print(f"SAME over {n} fight blanks" + ("" if len(a) == len(b) else " (one run is longer)"))
