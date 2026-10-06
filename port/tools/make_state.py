#!/usr/bin/env python3
"""Writes <program>.mem: where the GAME's own variables lie in the linked program.

The game's memory is its heap, the scratchpad and its global variables. The first two are fixed regions; the
globals are spread through the program's .data and .bss between the port's own (the renderer's buffers, caches,
counters), which are not game state. This reads the linker's map and lists the address ranges that come from the
game's objects (everything in the object folder except the port's pc_*, gs_* and C++ files, plus the data tables),
merged where they touch. port/src/gs/state.c reads the list: it is what a checksum of the game state covers, and
later what saving and restoring the state copies.

Usage: port/tools/make_state.py  (run by link.py after a successful link; BT3_CC selects the build as everywhere)"""
import pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from toolchain import OBJ, DATA, EXE

WRITABLE = (".data", ".bss", ".sdata", ".sbss", ".ldata", ".lbss", ".tbss", ".tdata")

def is_game(obj):
    p = pathlib.Path(obj.split("(")[0])
    if p.parent == DATA:
        return True
    return p.parent == OBJ and not p.name.startswith(("pc_", "gs_", "cxx_", "ui", "imgui"))

def main():
    text = pathlib.Path(str(EXE) + ".map").read_text(errors="replace")
    text = text[text.index("Linker script and memory map"):]
    out = None
    ranges = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        l = lines[i]
        i += 1
        m = re.match(r"^(\.[A-Za-z0-9_.]+|COMMON)\b", l)
        if m and not l.startswith(" "):
            out = m.group(1)          # an output section starts
            continue
        if not l.startswith(" ") or l.startswith("  "):
            continue
        # an input section: " name addr size object", or the name alone with the rest on the next line
        parts = l.split()
        if len(parts) == 1 and i < len(lines):
            parts += lines[i].split()
            i += 1
        if len(parts) < 4 or not parts[1].startswith("0x") or not parts[2].startswith("0x"):
            continue
        if out is None or not out.startswith(WRITABLE):
            continue
        addr, size, obj = int(parts[1], 16), int(parts[2], 16), " ".join(parts[3:])
        if size and addr and is_game(obj):
            ranges.append((addr, addr + size))
    ranges.sort()
    merged = []
    for a, b in ranges:
        if merged and a <= merged[-1][1] + 64:   # alignment gaps between two game objects belong to neither side
            merged[-1][1] = max(merged[-1][1], b)
        else:
            merged.append([a, b])
    path = pathlib.Path(str(EXE) + ".mem")
    path.write_text("".join(f"{a:x} {b - a:x}\n" for a, b in merged))
    print(f"{path.name}: {len(merged)} ranges of game variables, {sum(b - a for a, b in merged):,} bytes")

if __name__ == "__main__":
    main()
