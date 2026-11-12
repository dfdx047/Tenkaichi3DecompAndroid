#!/usr/bin/env python3
"""Finds calls that pass arguments differently from what the called function expects.

Many game sources declare the functions they call themselves, with their own idea of the types. The PS2 passes
every scalar in a 64-bit register, so `s32` against `u64`, or a missing argument, was harmless there; on a 32-bit
PC the stack layout changes and the callee reads garbage. This compares every declaration with the definition
(the compiler's own -aux-info dump) by ABI class: W = 32-bit integer or pointer, L = 64-bit integer, F = float,
D = double, V = void, other names = passed by value as written. Reported: different return class, different
class of a parameter, fewer parameters declared than defined.
Usage: port/tools/check_protos.py [-v]"""
import collections, concurrent.futures, pathlib, re, subprocess, sys
import portsrc

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/build/gen/protos"
W = set("uint_least32_t s8 u8 s16 u16 s32 u32 int char short long unsigned signed size_t uint32_t int32_t uint8_t uint16_t".split())
L = {"s64", "u64", "uint64_t", "int64_t"}

def klass(t):
    t = re.sub(r"\b(const|volatile|register|extern|static|struct|union|enum)\b", " ", t).strip()
    if "*" in t or "[" in t or "(" in t:
        return "W"
    words = t.split()
    if not words or words == ["void"]:
        return "V"
    if "long" in words and words.count("long") == 2:
        return "L"
    if all(w in W for w in words):
        return "W"
    if words[-1] in L:
        return "L"
    if words[-1] in ("f32", "float"):
        return "F"
    if words[-1] in ("ADXF", "ADXT_HN") or re.search(r"(Cb|Func|Fn|Callback|Handler)$", words[-1]):
        return "W"  # pointer typedefs
    return {"double": "D"}.get(words[-1], words[-1])

def split_params(p):
    out, depth, cur = [], 0, ""
    for c in p:
        if c == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            depth += c == "("
            depth -= c == ")"
            cur += c
    return out + [cur]

LINE = re.compile(r"/\* (\S+?):(\d+):([NO])([CF]) \*/ (.*?)\b(\w+) \((.*)\);")

def dump(f):
    src, _, _ = portsrc.prepare(f)
    x = OUT / (str(f.relative_to(ROOT)).replace("/", "_") + ".X")
    ref = f.parent == ROOT / "src/port"  # the vector-library references, under the game's names
    subprocess.run(["gcc", "-m32", "-std=gnu99" if ref else "-std=gnu89", "-fsyntax-only", "-w", "-Iinclude",
                    "-Iport/include", "-Iport/src", "-include", "port_compat.h"] +
                   (["-include", "vu0_names.h"] if ref else []) + [f"-I{f.parent}", str(src), "-aux-info", str(x)],
                   cwd=ROOT, capture_output=True)
    rows = []
    for l in x.read_text().splitlines() if x.exists() else []:
        l = l.split("; /*")[0] + ";"
        m = LINE.match(l)
        if not m or m.group(3) == "O" and m.group(4) == "C":
            continue  # old-style declarations say nothing about the parameters
        where, line, _, kind, ret, name, params = m.groups()
        params = [p.strip() for p in split_params(params)]
        var = params[-1:] == ["..."]
        ps = [klass(re.sub(r"\b\w+$", "", p) if kind == "F" and not p.endswith("*") and len(p.split()) > 1 else p)
              for p in params if p != "..."]
        ps = [] if ps == ["V"] else ps
        rows.append((name, kind, klass(ret), tuple(ps), var, f"{where}:{line}"))
    return rows

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    fs = portsrc.sources() + sorted((ROOT / "port/src").glob("*.c")) + \
         [ROOT / "src/port/vu0_a.c", ROOT / "src/port/vu0_b.c"]
    defs, decls = {}, collections.defaultdict(set)
    with concurrent.futures.ThreadPoolExecutor(16) as ex:
        for rows in ex.map(dump, fs):
            for name, kind, ret, ps, var, where in rows:
                if kind == "F":
                    defs.setdefault(name, (ret, ps, var, where))
                else:
                    decls[name].add((ret, ps, var, where))
    bad = []
    for name, (ret, ps, var, where) in sorted(defs.items()):
        for dret, dps, dvar, dwhere in sorted(decls.get(name, ())):
            why = []
            if dret != ret and "V" not in (dret,):
                why.append(f"returns {dret}, defined {ret}")
            if len(dps) < len(ps) and not dvar:
                why.append(f"{len(dps)} parameters, defined {len(ps)}")
            for i, (a, b) in enumerate(zip(dps, ps)):
                if a != b:
                    why.append(f"parameter {i + 1} {a}, defined {b}")
            if why:
                bad.append((name, where, dwhere, "; ".join(why)))
    print(f"{len(defs)} functions defined, {sum(len(v) for v in decls.values())} declarations seen, "
          f"{len(bad)} declarations disagree ({len({b[0] for b in bad})} functions)")
    (ROOT / "port/build/protos.txt").write_text("".join(f"{n}\t{w}\t{d}\t{y}\n" for n, w, d, y in bad))
    if "-v" in sys.argv:
        for n, w, d, y in bad[:60]:
            print(f"  {n} ({w}) <- {d}: {y}")

main()
