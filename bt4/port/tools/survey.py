#!/usr/bin/env python3
"""Compile every game source with the host compiler (32-bit, syntax only) and tally what stops it.
Usage: port/tools/survey.py [dir ...]   (default: src/main.c src/sys src/battle)"""
import collections, concurrent.futures, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CC = ["gcc", "-m32", "-std=gnu89", "-fsyntax-only", "-fno-strict-aliasing", "-w", "-fmax-errors=0",
      "-fdiagnostics-plain-output", "-Iinclude", "-Iport/include", "-include", "port_compat.h"]

def files(args):
    for a in args or ["src/main.c", "src/sys", "src/battle"]:
        p = ROOT / a
        yield from sorted(p.rglob("*.c")) if p.is_dir() else [p]

def run(f):
    r = subprocess.run(CC + [str(f.relative_to(ROOT))], cwd=ROOT, capture_output=True, text=True)
    return f, [l for l in r.stderr.splitlines() if " error: " in l]

def klass(msg):
    msg = msg.split(" error: ", 1)[1]
    msg = re.sub(r"'[^']*'", "'X'", msg)
    return re.sub(r"\d+", "N", msg)[:90]

def main():
    fs = list(files(sys.argv[1:]))
    kinds, clean, per = collections.Counter(), 0, []
    with concurrent.futures.ThreadPoolExecutor(16) as ex:
        for f, errs in ex.map(run, fs):
            clean += not errs
            per.append((len(errs), f))
            kinds.update(klass(e) for e in errs)
    print(f"{clean} of {len(fs)} files compile; {sum(n for n, _ in per)} errors")
    for k, n in kinds.most_common(25):
        print(f"{n:6d}  {k}")
    print("worst files:", ", ".join(f"{f.name}({n})" for n, f in sorted(per, reverse=True)[:10] if n))

main()
