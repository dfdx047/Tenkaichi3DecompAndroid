#!/usr/bin/env python3
"""Writes <program>.mem: where the GAME's own variables lie in the linked program.

The game's memory is its heap, the scratchpad and its global variables. The first two are fixed regions; the
globals are spread through the program's .data and .bss between the port's own (the renderer's buffers, caches,
counters), which are not game state. This reads the linker's map and lists the address ranges that come from the
game's objects (everything in the object folder except the port's gs_* and C++ files and some of its pc_* files,
see PORT_STATE; plus the data tables),
merged where they touch. port/src/gs/state.c reads the list: it is what a checksum of the game state covers, and
later what saving and restoring the state copies.

Usage: port/tools/make_state.py  (run by link.py after a successful link; BT3_CC selects the build as everywhere)"""
import bisect, itertools, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from toolchain import OBJ, DATA, EXE, MAC

PORT_STATE = {"headless", "mathf_pc", "plat_file", "plat_libm", "plat_mc", "plat_mem", "plat_sndstate", "plat_stub", "plat_sys", "softfloat_ps2",
              "vu0_a", "vu0_b", "gs_marker"}
WRITABLE = (".data", ".bss", ".sdata", ".sbss", ".ldata", ".lbss", ".tbss", ".tdata")

def is_game(obj):
    p = pathlib.Path(obj.split("(")[0])
    if p.parent == DATA:
        return True
    if p.parent != OBJ or p.name.startswith(("gs_", "cxx_", "ui", "imgui")):
        return False
    if p.name.startswith("pc_"):
        # the port's own files: those whose variables are part of what the game goes through (the vector unit's
        # registers and the C library generator, the memory, file and memory card layers, the start of a battle),
        # not the ones that only show or configure (crash report, movies, settings, the GS glue)
        return p.stem[3:] in PORT_STATE
    return True

def mac_map(text):
    """ld64's map (the macOS build) lists symbols, not input sections: "address size [file] name", the files numbered
    at the top. A game variable is a symbol of a game object in __DATA's __data, __common or __bss (not __porthst,
    the port's PORT_HOST variables). Addresses become game addresses: offsets in the window that starts 0x20000000
    below the program (port/src/port_addr.h). Returns (game ranges, the port's ranges, the map's text for the marks)."""
    files = {int(m.group(1)): m.group(2) for m in re.finditer(r"^\[\s*(\d+)\] (.*)$", text, re.M)}
    sect = text[text.index("# Sections:"):text.index("# Symbols:")]
    writable = []
    for m in re.finditer(r"^0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+(\S+)\s+(\S+)$", sect, re.M):
        if m.group(3) == "__DATA" and m.group(4) in ("__data", "__common", "__bss"):
            a = int(m.group(1), 16)
            writable.append((a, a + int(m.group(2), 16)))
    syms = text[text.index("# Symbols:"):]
    syms = syms[:syms.index("# Dead Stripped Symbols:")] if "# Dead Stripped Symbols:" in syms else syms
    window = int(re.search(r"^0x([0-9A-Fa-f]+)\s+0x0+\s+\[\s*\d+\] __mh_execute_header$", syms, re.M).group(1), 16) - 0x20000000
    ranges, others, marks = [], [], {}
    for m in re.finditer(r"^0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+\[\s*(\d+)\] (\S+)$", syms, re.M):
        a, size = int(m.group(1), 16), int(m.group(2), 16)
        if m.group(4) in ("_gPortResim", "_gPortVBlanks"):
            marks[m.group(4)[1:]] = a - window
        if size == 0 or not any(x <= a < y for x, y in writable):
            continue
        (ranges if is_game(files.get(int(m.group(3)), "")) else others).append((a - window, a - window + size))
    return ranges, others, "".join(f"   0x{v:x} {k}\n" for k, v in marks.items())

def linux_map(text):
    """GNU ld's map: input sections, each with its object."""
    text = text[text.index("Linker script and memory map"):]
    out = None
    ranges = []
    others = []
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
        if size and addr:
            (ranges if is_game(obj) else others).append((addr, addr + size))
    return ranges, others, text

def main():
    text = pathlib.Path(str(EXE) + ".map").read_text(errors="replace")
    if MAC:
        ranges, others, text = mac_map(text)
    else:
        ranges, others, text = linux_map(text)
    ranges.sort()
    others.sort()
    starts = [x for x, _ in others]
    reach = list(itertools.accumulate((y for _, y in others), max))  # the furthest end among the port's ranges so far
    merged = []
    for a, b in ranges:
        # Two game ranges are joined over the alignment gap between them, but not over anything of the port's
        # own that lies there (a small variable of a file that is not state sat in such a gap and was saved and
        # restored with the game: on Windows a lock's address, which then differed after a restore).
        if merged and a <= merged[-1][1] + 64 and not ((n := bisect.bisect_left(starts, a)) and reach[n - 1] > merged[-1][1]):
            merged[-1][1] = max(merged[-1][1], b)
        else:
            merged.append([a, b])
    # Which program the list belongs to: where two of its variables lie (lines "0 <address>" and "1 <address>").
    # state.c compares them with its own. A list left over from another release next to a new program (seen on
    # Windows: only the .exe was replaced) named addresses that are code in the new one, and the first restored
    # state was written over it.
    marks = []
    for k, name in enumerate(("gPortResim", "gPortVBlanks")):
        m = re.search(r"^\s+0x([0-9a-fA-F]+)\s+" + name + r"\s*$", text, re.M)
        if m is None:
            sys.exit(f"make_state: {name} is not in the map")
        marks.append(f"{k:x} {int(m.group(1), 16):x}\n")
    path = pathlib.Path(str(EXE) + ".mem")
    path.write_text("".join(marks) + "".join(f"{a:x} {b - a:x}\n" for a, b in merged))
    print(f"{path.name}: {len(merged)} ranges of game variables, {sum(b - a for a, b in merged):,} bytes")

if __name__ == "__main__":
    main()
