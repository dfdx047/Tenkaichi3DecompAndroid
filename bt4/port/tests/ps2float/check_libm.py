#!/usr/bin/env python3
"""Compares the PC build's maths library with the game's own routines as run in PCSX2 (libm_test.c).
Needs the port's objects (port/tools/undefined.py). Usage: port/tests/ps2float/check_libm.py"""
import pathlib, subprocess
D = pathlib.Path(__file__).resolve().parent
ROOT = D.parents[2]
O = ROOT / "port/build/obj"
exe = ROOT / "port/build/ps2libm_model"
soft = ["-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double", "-fno-builtin", "-include", "math.h", "-include", "stdlib.h",
        "-include", "port_libm.h"]
objs = [str(p) for p in sorted(O.glob("nl_*.o"))] + [str(O / n) for n in ("pc_softfloat_ps2.o", "pc_vu0_a.o", "pc_plat_libm.o")]
subprocess.run(["gcc", "-m32", "-O1", "-w", f"-I{ROOT / 'port/include'}"] + soft + ["-o", str(exe), str(D / "libm_model.c")] + objs +
               ["-lm"], check=True)
ref = [l.split() for l in (D / "pcsx2_libm_game.txt").read_text().splitlines()]
mine = [l.split() for l in subprocess.run([str(exe)], capture_output=True, text=True).stdout.splitlines()]
cols = "a b c sinf cosf tanf atanf atan2f asinf acosf sqrtf powf floorf".split()
print(f"{len(ref)} cases")
for c, name in enumerate(cols):
    bad = [(r, m) for r, m in zip(ref, mine) if len(r) > c and r[c] != m[c]]
    ex = "" if not bad else f"   e.g. a {bad[0][0][0]} b {bad[0][0][1]} c {bad[0][0][2]}: game {bad[0][0][c]}, port {bad[0][1][c]}"
    print(f"  {name:7s} {len(bad):5d} differ{ex}")
