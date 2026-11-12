#!/usr/bin/env python3
"""Compares the PC float model with PCSX2's results for the same inputs (see test.c / model.c).
Usage: port/tests/ps2float/check.py [reference.txt]   (default: pcsx2_v2.7.303.txt)"""
import pathlib, subprocess, sys
D = pathlib.Path(__file__).resolve().parent
ROOT = D.parents[2]
ref = [l.split() for l in (D / (sys.argv[1] if len(sys.argv) > 1 else "pcsx2_v2.7.303.txt")).read_text().splitlines()]
exe = ROOT / "port/build/ps2float_model"
subprocess.run(["gcc", "-m32", "-O1", "-w", "-DREF_VU0_EXTERN_ARITH", f"-I{ROOT / 'include'}", "-o", str(exe), str(D / "model.c"),
                str(ROOT / "port/src/softfloat_ps2.c"), str(ROOT / "src/port/vu0_a.c"), "-lm"], check=True)
mine = [l.split() for l in subprocess.run([str(exe)], capture_output=True, text=True).stdout.splitlines()]
cols = "a b n fpu_add fpu_sub fpu_mul fpu_div fpu_sqrt fpu_madd fpu_itof fpu_ftoi vu_add vu_sub vu_mul vu_div vu_sqrt vu_madd vu_itof vu_ftoi".split()
print(f"{len(ref)} cases")
for c, name in enumerate(cols):
    bad = [(r, m) for r, m in zip(ref, mine) if len(r) > c and r[c] != m[c]]
    ex = "" if not bad else f"   e.g. a {bad[0][0][0]} b {bad[0][0][1]} n {bad[0][0][2]}: PCSX2 {bad[0][0][c]}, model {bad[0][1][c]}"
    print(f"  {name:9s} {len(bad):5d} differ{ex}")
