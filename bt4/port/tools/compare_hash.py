#!/usr/bin/env python3
"""Compares two state checksum logs of the same program (BT3_HASH=<file>, port/src/gs/state.c).

  compare_hash.py a.txt b.txt            the first vertical blank at which the two runs differ
  compare_hash.py a.txt.pages b.txt.pages [program]
                                          the 4 KB pages that differ at the blank BT3_HASH_AT named, with the
                                          variables of the program that lie in them (from `nm`), for globals"""
import bisect, subprocess, sys

def read_dump(path):
    d = open(path, "rb").read()
    out, o = [], 0
    while o < len(d):
        e = d.index(b"\n", o)
        what, addr, size = d[o:e].split()
        addr, size = int(addr, 16), int(size, 16)
        out.append((what.decode(), addr, d[e + 1:e + 1 + size]))
        o = e + 1 + size
    return out

def dumps():
    """compare_hash.py a.txt.dump b.txt.dump [program]: every run of differing bytes, with the variable it is in"""
    syms = []
    if len(sys.argv) > 3:
        for l in subprocess.run(["nm", "-n", sys.argv[3]], capture_output=True, text=True).stdout.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in "bBdDCsS":
                syms.append((int(p[0], 16), p[2]))
    addrs = [s[0] for s in syms]
    total = 0
    for (what, addr, x), (_, _, y) in zip(read_dump(sys.argv[1]), read_dump(sys.argv[2])):
        if x == y:
            continue
        i, runs = 0, []
        n = len(x)
        while i < n:
            if x[i] != y[i]:
                j = i
                while j < n and (x[j] != y[j] or x[j:j + 8] != y[j:j + 8]):
                    j += 1
                runs.append((i, j))
                i = j
            else:
                i += 1
        total += len(runs)
        for i, j in runs[:25]:
            at = addr + i
            name = ""
            if what == "globals" and syms:
                k = bisect.bisect_right(addrs, at) - 1
                name = f"{syms[k][1]}+{at - syms[k][0]:#x}" if k >= 0 else ""
            print(f"  {what:10s} {at:#010x} {j - i:6d} bytes  {name}  {x[i:i + 12].hex()} / {y[i:i + 12].hex()}")
        if len(runs) > 25:
            print(f"  ... {len(runs) - 25} more runs in {what} at {addr:#x}")
    print(f"{total} runs of differing bytes")

def main():
    if sys.argv[1].endswith(".dump"):
        return dumps()
    a, b = (open(p).read().split("\n")[:-1] for p in sys.argv[1:3])  # (a line is complete once its newline is there)
    if "--fight" in sys.argv:
        # only the fight's own values (third column on): for two runs with different settings
        fa = [" ".join(l.split()[2:]) for l in a if len(l.split()) > 3]
        fb = [" ".join(l.split()[2:]) for l in b if len(l.split()) > 3]
        va = [l.split()[0] for l in a if len(l.split()) > 3]
        n = min(len(fa), len(fb))
        for i in range(n):
            if fa[i] != fb[i]:
                print(f"fight values differ from fight blank {i + 1} of {n} (blank {va[i]}):\n  {fa[i]}\n  {fb[i]}")
                return
        print(f"fight values identical for {n} blanks of fighting")
        return
    if not sys.argv[1].endswith(".pages"):
        n = min(len(a), len(b))
        for i in range(n):
            if a[i] != b[i]:
                print(f"first difference at line {i + 1}: {a[i]}  /  {b[i]}   ({n} lines compared)")
                return
        print(f"identical for {n} lines")
        return
    syms = []
    if len(sys.argv) > 3:
        for l in subprocess.run(["nm", "-n", sys.argv[3]], capture_output=True, text=True).stdout.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in "bBdDCsS":
                syms.append((int(p[0], 16), p[2]))
    addrs = [s[0] for s in syms]
    diff = [x.split() for x, y in zip(a, b) if x != y and x]
    print(f"{len(diff)} pages differ")
    for what, addr, _ in diff[:40]:
        at = int(addr, 16)
        names = ""
        if what == "globals" and syms:
            i = bisect.bisect_right(addrs, at) - 1
            names = " ".join(n for _, n in syms[max(i, 0):bisect.bisect_left(addrs, at + 4096)][:8])
        print(f"  {what:10s} {at:#010x}  {names}")

main()
