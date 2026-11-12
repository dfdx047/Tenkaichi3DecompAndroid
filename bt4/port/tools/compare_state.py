#!/usr/bin/env python3
"""Compares the PC build's state with a console memory dump taken at the same battle tick.

    port/tools/compare_state.py <tick> <eeMemory.bin> [-v] [name-filter]

Needs port/build/heap_<tick>.bin and glob_<tick>.bin (run the build with BT3_AT=<tick>). The heap uses the same
addresses on both sides and is compared byte by byte. Global variables live at different addresses: they are
matched by NAME (PC address and size from `nm` of port/build/bt3, PS2 address from config/symbols) and compared
word by word; a word that is a pointer to the same named global on both sides counts as equal."""
import bisect, pathlib, re, struct, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
B = ROOT / "port/build"
HEAP0, HEAP1 = 0x3BE730, 0x1EFB014

def main():
    tick, eepath = sys.argv[1], sys.argv[2]
    verbose = "-v" in sys.argv
    filt = next((a for a in sys.argv[3:] if not a.startswith("-")), None)
    ee = pathlib.Path(eepath).read_bytes()
    heap = (B / f"heap_{tick}.bin").read_bytes()
    g = (B / f"glob_{tick}.bin").read_bytes()
    gbase, g = struct.unpack_from("<I", g)[0], g[4:]
    ps2 = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        if not p.name.startswith("menu"):
            for l in p.read_text().splitlines():
                m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)", l)
                if m:
                    ps2[m.group(1)] = int(m.group(2), 16)
    pc = {}
    syms = []
    for l in subprocess.run(["nm", "-n", str(B / "bt3")], capture_output=True, text=True).stdout.splitlines():
        f = l.split()
        if len(f) == 3 and f[1] in "BDbdC":
            syms.append((int(f[0], 16), f[2]))
    ps2_sorted = sorted(set(ps2.values()))
    for k, (addr, name) in enumerate(syms):
        a = ps2.get(name) or (int(name[2:], 16) if re.fullmatch(r"D_[0-9A-F]{8}", name) else None)
        if not a:
            continue
        # most globals come from assembly data and carry no size: up to the next symbol on either side
        nxt_pc = next((x for x, _ in syms[k + 1:] if x > addr), addr + 4)
        i = bisect.bisect_right(ps2_sorted, a)
        nxt_ps2 = ps2_sorted[i] if i < len(ps2_sorted) else a + 4
        pc[name] = (addr, min(nxt_pc - addr, nxt_ps2 - a), a)
    pc_by_addr = {v[0]: k for k, v in pc.items()}
    for l in subprocess.run(["nm", str(B / "bt3")], capture_output=True, text=True).stdout.splitlines():
        f = l.split()  # functions too: tables of function pointers
        if len(f) == 3 and f[1] in "Tt":
            pc_by_addr.setdefault(int(f[0], 16), f[2])
            m = re.fullmatch(r"func_([0-9A-F]{8})", f[2])
            if m:
                ps2.setdefault(f[2], int(m.group(1), 16))
    same = diff = 0
    rows = []
    for name, (pa, size, sa) in sorted(pc.items(), key=lambda kv: kv[1][2]):
        if not (0 <= pa - gbase and pa - gbase + size <= len(g)) or sa + size > len(ee) or size > 0x40000:
            continue
        a, b = g[pa - gbase:pa - gbase + size], ee[sa:sa + size]
        bad = []
        for o in range(0, size - size % 4, 4):
            x, y = struct.unpack_from("<I", a, o)[0], struct.unpack_from("<I", b, o)[0]
            if x != y and not (pc_by_addr.get(x) is not None and ps2.get(pc_by_addr[x]) == y):
                bad.append((o, y, x))
        if size % 4 and a[size - size % 4:] != b[size - size % 4:]:
            bad.append((size - size % 4, 0, 0))
        if bad:
            diff += 1
            rows.append((name, sa, size, bad))
        else:
            same += 1
    hd = sum(1 for i in range(len(heap)) if heap[i] != ee[HEAP0 + i])
    print(f"tick {tick}: heap {hd} of {len(heap)} bytes differ ({100 * hd / len(heap):.2f}%); "
          f"globals: {same} equal, {diff} differ")
    for name, sa, size, bad in rows:
        if filt and filt not in name:
            continue
        if verbose or filt:
            print(f"  {name} (PS2 {sa:#x}, {size:#x} bytes): {len(bad)} words differ; first: " +
                  ", ".join(f"+{o:#x} console {y:08x} pc {x:08x}" for o, y, x in bad[:3]))
    if not verbose and not filt:
        print("  " + " ".join(f"{n}({len(b)})" for n, _, _, b in rows))

main()
