#!/usr/bin/env python3
"""Checks that the host compiler lays the game's structs out as the PS2 compiler did.
The headers document each member's PS2 offset (`/* 0x24 */ type name;`). This turns every such comment of a
top-level member into a compile-time assertion and reports the ones that fail under the PC build's flags.
Usage: port/tools/check_layout.py [-v]"""
import concurrent.futures, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/build/gen/layout"
CC = ["gcc", "-m32", "-malign-double", "-std=gnu11", "-fsyntax-only", "-w", "-fmax-errors=0",
      "-fdiagnostics-plain-output", "-Iinclude", "-Iport/include", "-include", "port_compat.h"]
MEMBER = re.compile(r"\s*/\* (0x[0-9A-Fa-f]+) \*/\s+(?:(?:const|unsigned|signed|struct|union|volatile) )*\w+[\s\*]+(\w+)(?:\[[^;,]*\])*\s*[;,]")

def asserts(text):
    out, depth, start = [], 0, None
    for l in text.split("\n"):
        if depth == 0:
            if re.match(r"typedef struct\b[^;]*\{\s*$", l):
                depth, members, first, rel = 1, [], True, False
            continue
        opens, closes = l.count("{"), l.count("}")
        if depth == 1 and opens == 0 and closes == 0:
            m = MEMBER.match(l)
            if m:
                rel = rel or (first and int(m.group(1), 16) != 0)
                members.append((m.group(2), m.group(1)))
        if l.strip() and not l.strip().startswith(("/*", "*", "//")) or "/* 0x" in l:
            first = False
        depth += opens - closes
        if depth == 0:
            m = re.match(r"\}[^;]*?\b(\w+);", l)
            if m:
                base = members[0][1] if members and rel else "0"  # some views document offsets within a larger block
                out += [f'_Static_assert(__builtin_offsetof({m.group(1)}, {n}) == {o} - {base}, "{m.group(1)}.{n} {o}");'
                        for n, o in members]
    return out

def check(h):
    a = asserts(h.read_text())
    if not a:
        return h, 0, []
    c = OUT / (str(h.relative_to(ROOT / "include")).replace("/", "_") + ".c")
    c.write_text(f'#include "common.h"\n#include "{h.relative_to(ROOT / "include")}"\n' + "\n".join(a) + "\n")
    r = subprocess.run(CC + [str(c)], cwd=ROOT, capture_output=True, text=True)
    errs = [l for l in r.stderr.splitlines() if " error: " in l]
    return h, len(a), errs

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    hs = sorted(p for p in (ROOT / "include").rglob("*.h") if "port" not in p.parts)
    total = bad = other = 0
    with concurrent.futures.ThreadPoolExecutor(16) as ex:
        for h, n, errs in ex.map(check, hs):
            total += n
            fails = [e for e in errs if "static assertion failed" in e]
            rest = [e for e in errs if "static assertion failed" not in e]
            bad += len(fails)
            other += len(rest)
            if (fails or rest) and "-v" in sys.argv:
                for e in (fails + rest)[:6]:
                    print(f"{h.name}: {e.split(' error: ', 1)[1][:110]}")
    print(f"{total} member offsets checked in {len(hs)} headers: {bad} differ, {other} other compile errors")

main()
