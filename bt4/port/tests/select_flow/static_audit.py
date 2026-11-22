#!/usr/bin/env python3
"""Static audit: the screen (new or classic) must not reach the game's deterministic state. Reads the sources only.

  python3 static_audit.py [path to bt4]        exit 0 = every rule holds, 1 = a rule failed (a line to review is printed)

Rules (each is a way a LOCAL preference could change what both online copies compute):
  R1  PortCharSel_On() (which screen) is used in the select logic only to print or to feed PortSelFlow_StageFirst.
  R2  the settings the select logic reads are only the ones named here (select_stage_first, through PortSelFlow_StageFirst).
  R3  the new screen (port_charsel.c) never writes the game's selection/progress/pad state and draws no random number.
  R4  every call from the select logic into the new screen is one of a reviewed list; a new call fails until it is reviewed.
  R5  the way to the battle (CinemaStart) and the map-first calls are reachable only in the map-first order, which is never the online order.
  R6  the random generators the viewer's world could touch lie inside the regions the world saves and puts back (needs the build's .map/.mem).
"""
import os, re, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
fails = 0


def fail(rule, where, text):
    global fails
    fails += 1
    print("FAIL %s %s: %s" % (rule, where, text.strip()))


def lines(rel):
    with open(os.path.join(ROOT, rel), errors="replace") as f:
        return f.read().split("\n")


LOGIC = ["src/menu/menu_c_e.c", "src/menu/menu_e_b.c"]

# R1, R2, R4
VIEW_ONLY = {"Shown", "Draw", "DrawItems", "DrawEnd", "MuteOn", "MuteOff", "Frame", "CineBlank_draw"}
REVIEWED_LOGIC = {  # name: why it is safe for the shared state
    "Begin": "resets the screen's own variables",
    "End": "tears the screen's own world down",
    "Abandon": "same, after a session was left",
    "On": "R1: only into PortSelFlow_StageFirst / diagnostics",
    "NetSide": "the same value on a given machine whichever screen: Port_NetMe() & 1, or the caller's fallback",
    "SetStage": "map-first only (R5); the viewer's stage, drawing",
    "CinemaStart": "map-first only (R5)",
    "InCinema": "a counter that is 0 unless CinemaStart ran (R5)",
    "CinemaSkip": "reads both pads, a counter; only after CinemaStart",
    "CineBlank": "a counter; only after CinemaStart",
    "CinemaDone": "a counter; only after CinemaStart",
    "CineBlank": "a counter; only after CinemaStart",
    "FormRowSize": "pure of its arguments",
    "FormOtherRow": "pure of its arguments",
    "CineFrame": "drawing",
    "CineBattle": "0 unless CinemaStart ran (R5)",
    "CineBattleEnd": "clears it",
}
for rel in LOGIC:
    L = lines(rel)
    for n, t in enumerate(L, 1):
        code = t.split("/*")[0]
        for m in re.finditer(r"PortCharSel_(\w+)\s*\(", code):
            name = m.group(1)
            if name in VIEW_ONLY or name in REVIEWED_LOGIC:
                pass
            else:
                fail("R4", "%s:%d" % (rel, n), "PortCharSel_%s is not in the reviewed list: %s" % (name, t))
            if name == "On" and not ("PortSelFlow_StageFirst" in code or "PortSelDiag_Flow" in code or "PortSelFlow_StageFirst" in L[n - 2] or
                                     "PortSelDiag_Flow" in code or "PortCharSel_On()," in code or "PortCharSel_On())" in code and "PortSelDiag_Flow" in code):
                fail("R1", "%s:%d" % (rel, n), "PortCharSel_On() outside the flow decision: %s" % t)
        for m in re.finditer(r'Port_Setting\s*\(\s*"([^"]+)"', code):
            if m.group(1) != "select_stage_first":
                fail("R2", "%s:%d" % (rel, n), "a setting read in the select logic (%s): %s" % (m.group(1), t))

# R3
CHARSEL = "src/menu/port_charsel.c"
L = lines(CHARSEL)
for n, t in enumerate(L, 1):
    code = t.split("//")[0]
    code = re.sub(r"/\*.*?\*/", "", code)
    if re.search(r"\b(gCharSel|gTeamSel|gProgress|gTsProgress|CSPROG|gPad)\b[^;]*?(?<![=!<>])=(?!=)", code) and not re.search(r"\b(const|==)\b", code.split("=")[0]):
        if not re.match(r"\s*(Pad|CharSelSide|CharSel|TeamSel|TsTeam|s32|f32|u32)\s*\*?\s*\w+\s*=\s*&?g", code) and "*pad =" not in code and "Pad *pad" not in code:
            fail("R3", "%s:%d" % (CHARSEL, n), "writes game state? %s" % t)
    if re.search(r"\b(Rand_\w+|rand|srand|Port_Rand|Port_Srand)\s*\(", code):
        fail("R3", "%s:%d" % (CHARSEL, n), "draws a random number: %s" % t)

# R5: CinemaStart / SetStage / the cinema only after sPortStageFirst reached 2 or 3 (the map-first order)
for rel in LOGIC:
    L = lines(rel)
    for n, t in enumerate(L, 1):
        code = t.split("/*")[0]
        if re.search(r"PortCharSel_(CinemaStart|SetStage)\s*\(", code):
            window = "\n".join(L[max(0, n - 30):n])
            if "sPortStageFirst == 2" not in window and "sPortStageFirst == 3" not in window:
                fail("R5", "%s:%d" % (rel, n), "reachable without the map-first order: %s" % t)

# the only value sPortStageFirst can start with is PortSelFlow_StageFirst's, and it becomes 2/3 only from 1
for rel in LOGIC:
    s = "\n".join(lines(rel))
    starts = re.findall(r"sPortStageFirst\s*(?<![=!<>])=(?!=)\s*([^;]+);", s)
    for v in starts:
        v = v.strip()
        if not (v.startswith("PortSelFlow_StageFirst") or v in ("0", "2", "3", "1")):
            fail("R5", rel, "sPortStageFirst assigned from something else: %s" % v)

# R6
import glob
maps = glob.glob(os.path.join(ROOT, "port/build/android/libbt3.so.map"))
mem = os.path.join(ROOT, "port/build/android/libbt3.so.mem")
if maps and os.path.exists(mem):
    regions = []
    for l in open(mem):
        p = l.split()
        if len(p) >= 2:
            a = int(p[0], 16)
            if a >= 2:
                regions.append((a, int(p[1], 16)))
    text = open(maps[0], errors="replace").read()
    for sym in ("gPortRandNext", "gRandIndex", "gRandState", "gRandMag01"):
        m = re.search(r"^\s*([0-9a-f]{8,16})\s+[0-9a-f]+\s+\d+\s+\d+\s+%s\s*$" % sym, text, re.M)
        if not m:
            print("note R6: %s not in the map (skipped)" % sym)
            continue
        addr = int(m.group(1), 16)
        inside = any(s <= addr - off < s + n for s, n in regions for off in (0, 0x10000000))
        print("R6 %-14s %s" % (sym, "inside a saved region" if inside else "OUTSIDE"))
        if not inside:
            fail("R6", sym, "not in a region the world saves: the viewer could change it")
else:
    print("R6 skipped: no Android build map/mem in this tree (build first)")

print("static audit: %s (%d failures)" % ("OK" if fails == 0 else "FAILED", fails))
sys.exit(1 if fails else 0)
