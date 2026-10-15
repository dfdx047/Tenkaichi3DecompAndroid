#!/usr/bin/env python3
"""The gameplay fingerprint of a data folder, and port/src/plat_fprint_tab.h (the original disc's values).

    python3 port/tools/make_fingerprint.py [--root gamedata] [--write] [--diff] [--list ID]

The game computes the same thing at start (port/src/plat_fprint.c) and prints it in the log; this is the second,
independent implementation, and the one that writes the table of the original disc's values the game compares
with. The definition is in docs/port/README.md ("The gameplay fingerprint"); both programs follow it to the byte.

  --root DIR   the data folder (pzs3us0..2 inside it; a file in DIR/mods/<same path> replaces one, as at load)
  --write      write port/src/plat_fprint_tab.h from DIR (DIR must be the unmodified disc: run make_verify's
               check, or the game with BT3_VERIFY=1, first)
  --diff       name every file and item that differs from the table in port/src/plat_fprint_tab.h
  --list ID    the items of one game file (its id, e.g. 0x590) with their values

The table holds hashes only, no game data.
"""
import bisect, hashlib, os, re, struct, sys
from pathlib import Path

FP_VERSION = 1
COMPONENTS = ["tables", "characters", "animations", "stages", "effects", "scripts", "menus"]
CHARS = 161
STAGES = 35
PART1 = 3399          # files of the second archive (pzs3us1): game file id n is pzs3us1/(n - 1)
MAX_DEPTH = 4
LIPS = 0x100          # item ids above the entry numbers
ST_NODES, ST_COLL, ST_BOUNDS, ST_OBJS = 0x101, 0x102, 0x103, 0x104
WHOLE = 0xFFFF        # the file (or entry) could not be read as what it should be: hashed whole


def u32(d, o):
    return struct.unpack_from("<I", d, o)[0]


def s32(d, o):
    return struct.unpack_from("<i", d, o)[0]


def P32(x):
    return struct.pack("<I", x & 0xFFFFFFFF)


def digest(stream):
    return int.from_bytes(hashlib.sha256(stream).digest()[:8], "big")


# ---- the three shapes -------------------------------------------------------------------------------------------

SECTOR = 2048
OUT = None            # an entry whose offset lies outside the file


def pack_lenient(d):
    """A file the game reads as a pack: [(flag, bytes | b"" | OUT)] or None. Word 0 = count N, words 1..N+1 =
    offsets w; the game takes entry k at w[k] & ~3 and calls it empty when w[k+1] == w[k], and that is all it
    asks of the table. So nothing more is asked here (packs rebuilt by modding tools have offsets that point
    past the end, or a last word that is zero): an entry runs from its offset to the next larger offset of the
    table, or to the end of the file."""
    n = len(d)
    if n < 12:
        return None
    N = u32(d, 0)
    if N < 1 or N > 4096 or 4 * (N + 2) > n:
        return None
    w = struct.unpack_from("<%dI" % (N + 1), d, 4)
    starts = sorted(set(x & ~3 for x in w if (x & ~3) < n)) + [n]
    out = []
    for i in range(N):
        o = w[i] & ~3
        if w[i + 1] == w[i]:
            out.append((w[i] & 3, b""))
        elif o >= n:
            out.append((w[i] & 3, OUT))
        else:
            out.append((w[i] & 3, d[o:starts[bisect.bisect_right(starts, o)]]))
    return out


def pack_strict_table(d):
    """[(start, end, flag)] of a well-formed pack (offsets in order, inside the block), or None."""
    n = len(d)
    if n < 12:
        return None
    N = u32(d, 0)
    if N < 1 or N > 4096 or 4 * (N + 2) > n:
        return None
    w = struct.unpack_from("<%dI" % (N + 1), d, 4)
    prev = 4 * (N + 2)
    for x in w:
        if (x & ~3) < prev or (x & ~3) > n:
            return None
        prev = x & ~3
    return [(w[i] & ~3, w[i + 1] & ~3, w[i] & 3) for i in range(N)]


def pack_strict(d):
    """The same for a block that is only GUESSED to be a pack: the bytes outside its entries must be zero, so
    that taking something else for a pack cannot hide data."""
    p = pack_strict_table(d)
    if p is None:
        return None
    N = len(p)
    if any(d[4 * (N + 2):p[0][0]]) or any(d[p[-1][1]:]):
        return None
    return p


def is_tex(d):
    """A texture file (what Res_RelocateOffsets takes): count, entry table offset in words, 0x40-byte entries with
    the pixel and palette packets' offsets (words) and sizes; each packet starts with a VIF DIRECT code that
    spans it."""
    n = len(d)
    if n < 0x60:
        return False
    cnt, eo = struct.unpack_from("<II", d, 0)
    if cnt < 1 or cnt > 1024 or eo < 4 or eo > 0x1000 or eo * 4 + cnt * 0x40 > n:
        return False
    lo = eo * 4 + cnt * 0x40
    for i in range(cnt):
        e = eo * 4 + i * 0x40
        po, co, ps, cs = struct.unpack_from("<IIii", d, e)
        if po == 0 and co == 0:
            return False
        for o, s in ((po, ps), (co, cs)):
            if o == 0:
                continue
            if o > 0x4000000 or s < 0x20 or s % 16 or o * 4 < lo or o * 4 + s > n:
                return False
            a, b, c, v = struct.unpack_from("<IIII", d, o * 4)
            if a or b or c or (v >> 24) != 0x50 or (v & 0xFFFF) != s // 16 - 1:
                return False
    return True


def G(d, depth):
    """The generic walk: an empty block, a texture file (its content does not count), a pack of blocks, or data."""
    if d is OUT:
        return b"X"
    if len(d) == 0:
        return b"E"
    if is_tex(d):
        return b"T"
    if depth < MAX_DEPTH:
        p = pack_strict(d)
        if p is not None:
            out = [b"P", P32(len(p))]
            for a, b, f in p:
                out.append(bytes([f]))
                out.append(G(d[a:b], depth + 1))
            return b"".join(out)
    return b"D" + P32(len(d)) + d


def D(d):
    if d is OUT:
        return b"X"
    return b"D" + P32(len(d)) + d if len(d) else b"E"


def header_item(p):
    return b"H" + P32(len(p)) + bytes(f for f, e in p)


# ---- the kinds of file -------------------------------------------------------------------------------------------

def whole(d):
    return [(WHOLE, b"W" + P32(len(d)) + d)]


def items_tables(d, fid):
    p = pack_lenient(d)
    if p is None:
        return whole(d)
    out = [(0, header_item(p))]
    lips = [b"L"]
    for n, (f, e) in enumerate(p, 1):
        if fid == 2 and n == 1:      # the fighters' light texture
            out.append((n, b"T" if e is not OUT and is_tex(e) else D(e)))
        elif fid == 2 and 14 <= n <= 213:
            lips.append(D(e))
        else:
            out.append((n, D(e)))
    if fid == 2:
        out.append((LIPS, b"".join(lips)))
    return out


def skeleton(e):
    """Entry 3 of a model file: the header's first 0x28 bytes and, of every record of the node chain, what places
    the node (bytes 4..0x40) and its bounds (0x40..0x60, records longer than 0x40). Not the records' lengths,
    the vertex streams behind them, the texture slots or the seam section."""
    n = len(e)
    if n < 0x70 or e[:4] != b"pmdl":
        return None
    o = u32(e, 0x6C)
    out = [b"M", e[:0x28]]
    recs = []
    for _ in range(4096):
        if o < 0x70 or o + 0x40 > n:
            return None
        nxt = s32(e, o)
        last = struct.unpack_from("<H", e, o + 6)[0]
        if nxt > 0x40:
            if nxt < 0x60 or o + 0x60 > n:
                return None
            recs.append(b"\1" + e[o + 4:o + 0x60])
        elif nxt == 0x40 or (last and nxt >= 0):
            recs.append(b"\0" + e[o + 4:o + 0x40])
        else:
            return None
        if last:
            out.append(P32(len(recs)))
            out.extend(recs)
            return b"".join(out)
        o += nxt
    return None


def items_model(d):
    p = pack_lenient(d)
    if p is None:
        return whole(d)
    out = [(0, header_item(p))]
    lips = [b"L"]
    for n, (f, e) in enumerate(p, 1):
        if n == 3:
            s = skeleton(e) if e is not OUT and len(e) else None
            out.append((3, s if s is not None else D(e)))
        elif n == 2 or 0x12 <= n <= 0x2C:
            out.append((n, D(e)))
        elif 0x35 <= n <= 0xFC:
            lips.append(D(e))
        # 1, 4..0x11: face, mouth, eye and body textures; 0x2D..0x34: the skill names; past 0xFC: nothing reads
    out.append((LIPS, b"".join(lips)))
    return out


def items_anim(d):
    p = pack_lenient(d)
    if p is None:
        return whole(d)
    return [(0, b"P" + P32(len(p)) + b"".join(bytes([f]) + D(e) for f, e in p))]


def items_generic(d):
    """Effect packs: every entry by the generic walk."""
    p = pack_lenient(d)
    if p is None:
        return whole(d)
    return [(0, header_item(p))] + [(n, G(e, 1)) for n, (f, e) in enumerate(p, 1)]


class Bad(Exception):
    pass


def stage_object(e):
    """Entry 1 of a stage file, the part the simulation reads, without anything that says WHERE in the entry a
    table lies (every offset is in words from the entry's start and moves when the texture file in the middle of
    the entry changes size). None if the entry does not have the shape."""
    n = len(e)

    def at(off, size):
        if off < 0 or size < 0 or off * 4 + size > n:
            raise Bad()
        return off * 4

    def W(o):
        return s32(e, o)

    try:
        if n < 0x60:
            raise Bad()
        timerCount, timers = u32(e, 0x10), W(0x14)
        nodeCount, nodes = u32(e, 0x30), W(0x34)
        zoneCount, zones = u32(e, 0x38), W(0x3C)
        bounds = W(0x40)
        objCount, objs = u32(e, 0x44), W(0x48)
        if max(timerCount, nodeCount, zoneCount, objCount) > 0x10000:
            raise Bad()

        s = [b"N", P32(timerCount)]
        t = at(timers, timerCount * 0x10)
        for i in range(timerCount):
            s.append(e[t + i * 0x10:t + i * 0x10 + 4])
            s.append(e[t + i * 0x10 + 8:t + i * 0x10 + 0x10])
        s.append(P32(nodeCount))
        nb = at(nodes, nodeCount * 0x50)
        for i in range(nodeCount):
            o = nb + i * 0x50
            keyCount, keys = W(o + 0x40), W(o + 0x44)
            s.append(e[o:o + 0x44])
            s.append(e[o + 0x48:o + 0x50])
            if keyCount > 0 and keys != 0:
                if keyCount > 0x10000:
                    raise Bad()
                k = at(keys, keyCount * 0x20)
                s.append(e[k:k + keyCount * 0x20])
        item_nodes = b"".join(s)

        def mesh(off):
            if off == 0:
                return b"\0"
            m = at(off, 0x18)
            nodeN, polyN = W(m), W(m + 4)
            if nodeN < 0 or polyN < 0 or nodeN > 0x100000 or polyN > 0x100000:
                raise Bad()
            no = at(W(m + 0xC), nodeN * 0x20)
            po = at(W(m + 0x10), polyN * 0x20)
            top = -1
            for i in range(polyN):
                a, b, c = struct.unpack_from("<iii", e, po + i * 0x20 + 4)
                if a < 0 or b < 0 or c < 0:
                    raise Bad()
                top = max(top, a, b, c)
            vo = at(W(m + 0x14), (top + 1) * 0x10)
            return b"".join([b"\1", e[m:m + 0xC], e[no:no + nodeN * 0x20], e[po:po + polyN * 0x20], P32(top + 1),
                             e[vo:vo + (top + 1) * 0x10]])

        s = [b"C", P32(zoneCount)]
        zb = at(zones, zoneCount * 0x40)
        for i in range(zoneCount):
            o = zb + i * 0x40
            nearCount, near, recCount, recs = W(o + 8), W(o + 0xC), W(o + 0x14), W(o + 0x18)
            if not (0 <= nearCount <= 0x10000 and 0 <= recCount <= 0x10000):
                raise Bad()
            s += [e[o:o + 4], P32(nearCount), e[o + 0x10:o + 0x18], e[o + 0x1C:o + 0x40]]
            if nearCount:
                q = at(near, nearCount * 4)
                s.append(e[q:q + nearCount * 4])
            s.append(mesh(W(o + 4)))
            if recCount:
                r = at(recs, recCount * 0x10)
                for k in range(recCount):
                    s += [e[r + k * 0x10:r + k * 0x10 + 8], e[r + k * 0x10 + 0xC:r + k * 0x10 + 0x10],
                          mesh(W(r + k * 0x10 + 8))]
        item_coll = b"".join(s)

        b = at(bounds, 12)
        item_bounds = b"B" + e[b:b + 12]

        def part(off):
            if off == 0:
                return b"\0"
            p = at(off, 0x20)
            node = W(p + 8)
            index = -1
            if node != 0:
                rel = (node - nodes) * 4
                if rel < 0 or rel % 0x50 or rel // 0x50 >= nodeCount:
                    raise Bad()
                index = rel // 0x50
            return b"\1" + e[p + 2:p + 4] + P32(index) + e[p + 0xC:p + 0x20]

        s = [b"O", P32(objCount)]
        ob = at(objs, objCount * 0x50)
        for i in range(objCount):
            o = ob + i * 0x50
            s.append(e[o:o + 0xC])
            for k in range(6):
                s.append(part(W(o + 0xC + k * 4)))
            pieceCount, pieces = u32(e, o + 0x24), W(o + 0x28)
            if pieceCount > 0x10000:
                raise Bad()
            s.append(P32(pieceCount))
            if pieceCount:
                q = at(pieces, pieceCount * 4)
                for k in range(pieceCount):
                    s.append(part(W(q + k * 4)))
            s.append(e[o + 0x2C:o + 0x50])
        item_objs = b"".join(s)
    except (Bad, struct.error):
        return None
    return [(ST_NODES, item_nodes), (ST_COLL, item_coll), (ST_BOUNDS, item_bounds), (ST_OBJS, item_objs)]


def items_stage(d):
    p = pack_lenient(d)
    if p is None:
        return whole(d)
    out = [(0, header_item(p))]
    for n, (f, e) in enumerate(p, 1):
        if n == 1:
            s = stage_object(e) if e is not OUT else None
            out += s if s is not None else [(1, D(e))]
        else:
            out.append((n, G(e, 1)))
    return out


def items_transition(d):
    """The stage-change scene: entry 1 is its camera animation (the load waits for it); 2 is a model and 3 its
    textures."""
    p = pack_lenient(d)
    if p is None:
        return whole(d)
    return [(0, header_item(p)), (1, D(p[0][1]))]


def bpe_decode(src):
    """Philip Gage's byte pair "expand" as src/sys/bpe.c does it; None when the data does not decode."""
    if len(src) < 8:
        return None
    size, packed = struct.unpack_from("<ii", src, 0)
    if size < 0 or size > 0x4000000 or packed < 0 or 8 + packed > len(src):
        return None
    out = bytearray()
    i, end = 8, 8 + packed
    try:
        while i < end:
            left = list(range(256))
            right = [0] * 256
            count = src[i]
            i += 1
            c = 0
            while True:
                if count > 127:
                    c += count - 127
                    count = 0
                if c == 256:
                    break
                for _ in range(count + 1):
                    left[c] = src[i]
                    i += 1
                    if c != left[c]:
                        right[c] = src[i]
                        i += 1
                    c += 1
                if c == 256:
                    break
                count = src[i]
                i += 1
            block = (src[i] << 8) + src[i + 1]
            i += 2
            stop = i + block
            if stop > end:
                return None
            stack = []
            while True:
                if stack:
                    c = stack.pop()
                elif i < stop:
                    c = src[i]
                    i += 1
                else:
                    break
                if c == left[c]:
                    out.append(c)
                    if len(out) > size:
                        return None
                else:
                    if len(stack) > 4096:
                        return None
                    stack.append(right[c])
                    stack.append(left[c])
    except IndexError:
        return None
    return bytes(out) if len(out) == size else None


def items_menu(d):
    """The versus menus' archive: section 2 (the character / team select's resources) unpacked, every entry by the
    generic walk, so its tables and animations count and its pictures do not."""
    p = pack_lenient(d)
    if p is None or len(p) < 2:
        return whole(d)
    sec = p[1][1]
    if sec is OUT:
        return whole(d)
    raw = bpe_decode(sec)
    q = pack_lenient(raw) if raw is not None else None
    if q is None:
        return [(WHOLE, b"W" + P32(len(sec)) + sec)]
    return [(0, header_item(q))] + [(n, G(e, 1)) for n, (f, e) in enumerate(q, 1)]


# ---- the list of files, in the order they are hashed ------------------------------------------------------------

def file_list():
    """[(component index, game file id, kind)]"""
    out = [(0, 2, "tables"), (0, 4, "tables")]
    for c in range(CHARS):
        out += [(1, 0x590 + c * 10 + k, "model") for k in range(8)]
    out += [(1, i, "model") for i in range(0xD1C, 0xD48)]
    out += [(2, 0x598 + c * 10, "anim") for c in range(CHARS)]
    out += [(3, 0x171 + s, "stage") for s in range(STAGES)]
    out += [(3, 0x198 + s, "stage") for s in range(STAGES)]
    out += [(3, 0x1BF, "transition"), (3, 0x1C0, "transition")]
    out += [(4, 3, "generic")]
    out += [(4, 0x599 + c * 10, "generic") for c in range(CHARS)]
    out += [(5, i, "script") for i in range(0x1FF, 0x231)]
    out += [(6, 0x1C1, "menu")]
    return out


KINDS = {
    "tables": None, "model": items_model, "anim": items_anim, "stage": items_stage, "transition": items_transition,
    "generic": items_generic, "script": lambda d: [(0, b"S" + P32(len(d)) + d)], "menu": items_menu,
}


def rel_of(fid):
    return "pzs3us1/%05d.bin" % (fid - 1)


def read_file(root, rel):
    """What the game would load: mods/ first; in whole sectors, the rest of the last one zero (ADXF_ReadNw)."""
    for p in (os.path.join(root, "mods", rel), os.path.join(root, rel)):
        try:
            with open(p, "rb") as f:
                d = f.read()
                return d + bytes(-len(d) % SECTOR)
        except OSError:
            pass
    return None


def file_items(root, fid, kind):
    d = read_file(root, rel_of(fid))
    if d is None:
        return None
    its = items_tables(d, fid) if kind == "tables" else KINDS[kind](d)
    return [(i, digest(s)) for i, s in its]


def component_digest(files):
    """files: [(fid, items or None)] in list order."""
    h = hashlib.sha256()
    for fid, its in files:
        h.update(P32(fid))
        if its is None:
            h.update(P32(0xFFFFFFFF))
            continue
        h.update(P32(len(its)))
        for i, dg in its:
            h.update(P32(i) + dg.to_bytes(8, "big"))
    return int.from_bytes(h.digest()[:8], "big")


def main_digest(comps):
    h = hashlib.sha256(b"BT3FP" + P32(FP_VERSION))
    for c in comps:
        h.update(c.to_bytes(8, "big"))
    return int.from_bytes(h.digest()[:8], "big")


TAB = Path(__file__).resolve().parents[1] / "src/plat_fprint_tab.h"


def read_table():
    """{fid: [(item, hash32)]} from the header."""
    text = TAB.read_text()
    out = {}
    for m in re.finditer(r"/\* (0x[0-9A-F]+) \*/((?: 0x[0-9A-F]+,0x[0-9A-F]+,)+)", text):
        nums = [int(x, 16) for x in re.findall(r"0x[0-9A-F]+", m.group(2))]
        out[int(m.group(1), 16)] = list(zip(nums[0::2], nums[1::2]))
    return out


def main():
    args = sys.argv[1:]
    root = "gamedata"
    if "--root" in args:
        root = args[args.index("--root") + 1]
    if "--list" in args:
        fid = int(args[args.index("--list") + 1], 0)
        kind = [k for c, f, k in file_list() if f == fid]
        if not kind:
            raise SystemExit("file 0x%X is not part of the fingerprint" % fid)
        for i, dg in file_items(root, fid, kind[0]) or []:
            print("0x%X item 0x%X: %016x" % (fid, i, dg))
        return
    per = [[] for _ in COMPONENTS]
    for comp, fid, kind in file_list():
        per[comp].append((fid, file_items(root, fid, kind)))
    comps = [component_digest(f) for f in per]
    fp = main_digest(comps)
    print("gameplay data: %08x (%s)" % (fp >> 32, " ".join("%s %08x" % (n, c >> 32) for n, c in zip(COMPONENTS, comps))))
    if "--diff" in args:
        tab = read_table()
        for comp, files in enumerate(per):
            for fid, its in files:
                want = tab.get(fid, [])
                got = [] if its is None else [(i, dg >> 32) for i, dg in its]
                if got != want:
                    w = dict(want)
                    bad = [i for i, v in got if w.get(i) != v] + [i for i in w if i not in dict(got)]
                    print("  %s: file 0x%X (%s)%s: items %s" % (COMPONENTS[comp], fid, rel_of(fid), " MISSING" if its is None else "",
                                                             " ".join("0x%X" % i for i in bad)))
    if "--write" in args:
        with open(TAB, "w") as o:
            o.write("/* Made by port/tools/make_fingerprint.py from the unmodified data of SLUS-21678: the gameplay fingerprint's\n"
                    "   expected values (plat_fprint.c). Hashes only. Per file: its game id, then (item, first 32 bits of the\n"
                    "   item's hash) pairs, in the order the files are hashed. */\n")
            o.write("#define FP_TAB_VERSION %d\n" % FP_VERSION)
            o.write("static const unsigned long long kFpMain = 0x%016Xull;\n" % fp)
            o.write("static const unsigned long long kFpComp[%d] = { %s };\n" % (len(comps), ", ".join("0x%016Xull" % c for c in comps)))
            flat, index = [], []
            for files in per:
                for fid, its in files:
                    if its is None:
                        raise SystemExit("file 0x%X is missing: not the original data" % fid)
                    index.append((fid, len(flat) // 2, len(its)))
                    for i, dg in its:
                        flat += [i, dg >> 32]
            o.write("static const unsigned int kFpFile[%d][3] = { /* game file id, first pair, pairs */\n" % len(index))
            for k in range(0, len(index), 8):
                o.write("    " + " ".join("{0x%X,%d,%d}," % x for x in index[k:k + 8]) + "\n")
            o.write("};\nstatic const unsigned int kFpItem[%d] = {\n" % len(flat))
            for fid, first, count in index:
                o.write("    /* 0x%X */ %s\n" % (fid, " ".join("0x%X,0x%08X," % (flat[(first + k) * 2], flat[(first + k) * 2 + 1]) for k in range(count))))
            o.write("};\n")
        print("%s: %d files, %d items" % (TAB, len(index), len(flat) // 2))


if __name__ == "__main__":
    main()
