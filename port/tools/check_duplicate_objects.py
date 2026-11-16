#!/usr/bin/env python3
"""Report symbols defined by more than one Android Engine object file.

Run after ``android.py compile``; this needs nm but no Android link/toolchain.
"""
import collections
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / "port/build/android"
NM = sys.argv[1] if len(sys.argv) > 1 else "nm"
objects = sorted((BUILD / "obj").glob("*.o")) + sorted((BUILD / "data").glob("*.o"))
if not objects:
    raise SystemExit(f"no objects found under {BUILD}; compile the Engine first")

defined = collections.defaultdict(list)
for obj in objects:
    result = subprocess.run([NM, "--defined-only", str(obj)], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise SystemExit(f"{NM} failed for {obj}: {result.stderr.strip()}")
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[-2] not in ("U", "u"):
            defined[fields[-1]].append(obj)

duplicates = {symbol: paths for symbol, paths in defined.items() if len(paths) > 1}
if duplicates:
    for symbol, paths in sorted(duplicates.items()):
        print(f"{symbol}: " + ", ".join(str(p.relative_to(ROOT)) for p in paths))
    raise SystemExit(f"{len(duplicates)} symbols have multiple object definitions")
print(f"no duplicate defined symbols across {len(objects)} objects")
