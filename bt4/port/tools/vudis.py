#!/usr/bin/env python3
"""Disassembles VU1 microcode (the subset the game's nine vertex programs use).
Usage: vudis.py <file> [first-instruction [count]]   (file: raw microcode, 8 bytes per instruction)"""
import struct, sys
XYZW = "xyzw"
def dest(d): return "".join(c for i, c in enumerate(XYZW) if d >> (3 - i) & 1)
def upper(hi):
    d, ft, fs, fd, op, bc = hi >> 21 & 15, hi >> 16 & 31, hi >> 11 & 31, hi >> 6 & 31, hi & 63, hi & 3
    D = dest(d)
    names3 = {0x28: "add", 0x29: "madd", 0x2A: "mul", 0x2B: "max", 0x2C: "sub", 0x2D: "msub", 0x2E: "opmsub", 0x2F: "mini"}
    bcn = {0x00: "add", 0x04: "sub", 0x08: "madd", 0x0C: "msub", 0x10: "max", 0x14: "mini", 0x18: "mul"}
    qi = {0x1C: "mulq", 0x1D: "maxi", 0x1E: "muli", 0x1F: "minii", 0x20: "addq", 0x21: "maddq", 0x22: "addi", 0x23: "maddi",
          0x24: "subq", 0x25: "msubq", 0x26: "subi", 0x27: "msubi"}
    if op < 0x1C: return f"{bcn[op & ~3]}{XYZW[bc]}.{D} vf{fd}, vf{fs}, vf{ft}{XYZW[bc]}"
    if op in qi: return f"{qi[op]}.{D} vf{fd}, vf{fs}"
    if op in names3: return f"{names3[op]}.{D} vf{fd}, vf{fs}, vf{ft}"
    sp = fd << 2 | bc
    if sp == 0x2F: return "nop"
    a = {0x00: "adda", 0x04: "suba", 0x08: "madda", 0x0C: "msuba", 0x18: "mula"}
    if sp < 0x10 or 0x18 <= sp < 0x1C: return f"{a[sp & ~3]}{XYZW[bc]}.{D} acc, vf{fs}, vf{ft}{XYZW[bc]}"
    cv = {0x10: "itof0", 0x11: "itof4", 0x12: "itof12", 0x13: "itof15", 0x14: "ftoi0", 0x15: "ftoi4", 0x16: "ftoi12", 0x17: "ftoi15", 0x1D: "abs"}
    if sp in cv: return f"{cv[sp]}.{D} vf{ft}, vf{fs}"
    if sp == 0x1F: return f"clip vf{fs}, vf{ft}w"
    o = {0x1C: "mulaq", 0x1E: "mulai", 0x20: "addaq", 0x22: "addai", 0x28: "adda", 0x29: "madda", 0x2A: "mula", 0x2C: "suba", 0x2D: "msuba", 0x2E: "opmula"}
    return f"{o.get(sp, 'up?%02x' % sp)}.{D} acc, vf{fs}, vf{ft}"
def lower(lo, pc):
    op, it, is_, id_, d = lo >> 25, lo >> 16 & 31, lo >> 11 & 31, lo >> 6 & 31, lo >> 21 & 15
    imm11 = (lo & 0x7FF) - (0x800 if lo & 0x400 else 0)
    imm15 = (lo >> 10 & 0x7800) | (lo & 0x7FF)
    D = dest(d)
    if op == 0x00: return f"lq.{D} vf{it}, {imm11}(vi{is_})"
    if op == 0x01: return f"sq.{D} vf{is_}, {imm11}(vi{it})"
    if op == 0x04: return f"ilw.{D} vi{it}, {imm11}(vi{is_})"
    if op == 0x05: return f"isw.{D} vi{it}, {imm11}(vi{is_})"
    if op == 0x08: return f"iaddiu vi{it}, vi{is_}, {imm15:#x}"
    if op == 0x09: return f"isubiu vi{it}, vi{is_}, {imm15:#x}"
    if op in (0x10, 0x11, 0x12, 0x13): return f"{['fceq', 'fcset', 'fcand', 'fcor'][op - 0x10]} vi1, {lo & 0xFFFFFF:#x}"
    if op == 0x1C: return f"fcget vi{it}"
    if op == 0x20: return f"b {pc + 1 + imm11}"
    if op == 0x21: return f"bal vi{it}, {pc + 1 + imm11}"
    if op == 0x24: return f"jr vi{is_}"
    if op == 0x25: return f"jalr vi{it}, vi{is_}"
    br = {0x28: "ibeq", 0x29: "ibne"}
    if op in br: return f"{br[op]} vi{it}, vi{is_}, {pc + 1 + imm11}"
    bz = {0x2C: "ibltz", 0x2D: "ibgtz", 0x2E: "iblez", 0x2F: "ibgez"}
    if op in bz: return f"{bz[op]} vi{is_}, {pc + 1 + imm11}"
    if op == 0x40:
        f = lo & 63
        if f == 0x30: return f"iadd vi{id_}, vi{is_}, vi{it}"
        if f == 0x31: return f"isub vi{id_}, vi{is_}, vi{it}"
        if f == 0x32: return f"iaddi vi{it}, vi{is_}, {(lo >> 6 & 31) - (32 if lo & 0x400 else 0)}"
        if f == 0x34: return f"iand vi{id_}, vi{is_}, vi{it}"
        if f == 0x35: return f"ior vi{id_}, vi{is_}, vi{it}"
        sp = id_ << 2 | (lo & 3)
        fsf, ftf = XYZW[lo >> 21 & 3], XYZW[lo >> 23 & 3]
        t = {0x30: f"move.{D} vf{it}, vf{is_}", 0x31: f"mr32.{D} vf{it}, vf{is_}", 0x34: f"lqi.{D} vf{it}, (vi{is_}++)",
             0x35: f"sqi.{D} vf{is_}, (vi{it}++)", 0x36: f"lqd.{D} vf{it}, (--vi{is_})", 0x37: f"sqd.{D} vf{is_}, (--vi{it})",
             0x38: f"div q, vf{is_}{fsf}, vf{it}{ftf}", 0x39: f"sqrt q, vf{it}{ftf}", 0x3A: f"rsqrt q, vf{is_}{fsf}, vf{it}{ftf}",
             0x3B: "waitq", 0x3C: f"mtir vi{it}, vf{is_}{fsf}", 0x3D: f"mfir.{D} vf{it}, vi{is_}", 0x3E: f"ilwr.{D} vi{it}, (vi{is_})",
             0x3F: f"iswr.{D} vi{it}, (vi{is_})", 0x68: f"xtop vi{it}", 0x69: f"xitop vi{it}", 0x6C: f"xgkick vi{is_}"}
        s = t.get(sp, "lo?%02x" % sp)
        return "nop" if s.startswith("move.") and it == 0 and is_ == 0 else s
    return "lo?op%02x" % op
def main():
    d = open(sys.argv[1], "rb").read()
    first = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0
    count = int(sys.argv[3], 0) if len(sys.argv) > 3 else len(d) // 8 - first
    for pc in range(first, first + count):
        lo, hi = struct.unpack_from("<II", d, pc * 8)
        fl = "".join(c for b, c in ((31, "I"), (30, "E"), (29, "M"), (28, "D"), (27, "T")) if hi >> b & 1)
        l = ("loi %.9g" % struct.unpack("<f", struct.pack("<I", lo))[0]) if hi >> 31 & 1 else lower(lo, pc)
        print(f"{pc:5d}: {upper(hi):34s} | {l:34s} {('[' + fl + ']') if fl else ''}")
main()
