#!/usr/bin/env python3
"""Assembler and disassembler for the game's VU1 microprograms (src/vu1/*.vsm).

  vuasm.py -o out.bin a.vsm b.vsm ...      assemble, concatenated in the order given
  vuasm.py --disasm blob.bin start end      print a listing of blob[start:end] (a VIF packet with MPG codes)

Listing syntax (one VU instruction per line, both halves; `;` starts a comment):
    label:
        upper | lower [flags]        flags: any of I E M D T (bits 31..27 of the upper word)
    .vif 0x........                  a raw 32-bit VIF code (the packets start with a VIF NOP and are padded with them)
    .mpg <addr>                      VIF MPG code; its instruction count is everything up to the next directive
Branch operands are labels; a label's value is the instruction's address in VU memory (the .mpg address plus
its position). With the I flag the lower word is a float constant: `loi 0x3F800000`.
`raw 0x........` in either half gives the word as is (for an encoding the mnemonics do not reproduce).
Only the instructions these programs use are known."""
import re
import struct
import sys

XYZW = "xyzw"
BC = {0x00: "add", 0x04: "sub", 0x08: "madd", 0x0C: "msub", 0x10: "max", 0x14: "mini", 0x18: "mul"}
QI = {0x1C: "mulq", 0x1D: "maxi", 0x1E: "muli", 0x1F: "minii", 0x20: "addq", 0x21: "maddq", 0x22: "addi", 0x23: "maddi",
      0x24: "subq", 0x25: "msubq", 0x26: "subi", 0x27: "msubi"}
N3 = {0x28: "add", 0x29: "madd", 0x2A: "mul", 0x2B: "max", 0x2C: "sub", 0x2D: "msub", 0x2E: "opmsub", 0x2F: "mini"}
ABC = {0x00: "adda", 0x04: "suba", 0x08: "madda", 0x0C: "msuba", 0x18: "mula"}
CV = {0x10: "itof0", 0x11: "itof4", 0x12: "itof12", 0x13: "itof15", 0x14: "ftoi0", 0x15: "ftoi4", 0x16: "ftoi12", 0x17: "ftoi15",
      0x1D: "abs"}
ACC = {0x1C: "mulaq", 0x1E: "mulai", 0x20: "addaq", 0x22: "addai", 0x28: "adda", 0x29: "madda", 0x2A: "mula", 0x2C: "suba",
       0x2D: "msuba", 0x2E: "opmula"}
FC = {0x10: "fceq", 0x11: "fcset", 0x12: "fcand", 0x13: "fcor"}
BR2 = {0x28: "ibeq", 0x29: "ibne"}
BR1 = {0x2C: "ibltz", 0x2D: "ibgtz", 0x2E: "iblez", 0x2F: "ibgez"}
INT3 = {0x30: "iadd", 0x31: "isub", 0x34: "iand", 0x35: "ior"}
LSP = {0x30: "move", 0x31: "mr32", 0x34: "lqi", 0x35: "sqi", 0x36: "lqd", 0x37: "sqd", 0x38: "div", 0x39: "sqrt", 0x3A: "rsqrt",
       0x3B: "waitq", 0x3C: "mtir", 0x3D: "mfir", 0x3E: "ilwr", 0x3F: "iswr", 0x68: "xtop", 0x69: "xitop", 0x6C: "xgkick"}


def dest(d):
    return "".join(c for i, c in enumerate(XYZW) if d >> (3 - i) & 1)


def inv(table):
    return {v: k for k, v in table.items()}


# ------------------------------------------------------------------------------------------------ disassembly

def dis_upper(hi):
    d, ft, fs, fd, op, bc = hi >> 21 & 15, hi >> 16 & 31, hi >> 11 & 31, hi >> 6 & 31, hi & 63, hi & 3
    D = dest(d)
    if op < 0x1C:
        return f"{BC[op & ~3]}{XYZW[bc]}.{D} vf{fd}, vf{fs}, vf{ft}{XYZW[bc]}" if (op & ~3) in BC else None
    if op in QI:
        return f"{QI[op]}.{D} vf{fd}, vf{fs}"
    if op in N3:
        return f"{N3[op]}.{D} vf{fd}, vf{fs}, vf{ft}"
    if op < 0x3C:
        return None
    sp = fd << 2 | bc
    if sp == 0x2F:
        return "nop"
    if (sp < 0x10 or 0x18 <= sp < 0x1C) and (sp & ~3) in ABC:
        return f"{ABC[sp & ~3]}{XYZW[bc]}.{D} acc, vf{fs}, vf{ft}{XYZW[bc]}"
    if sp in CV:
        return f"{CV[sp]}.{D} vf{ft}, vf{fs}"
    if sp == 0x1F:
        return f"clip.{D} vf{fs}, vf{ft}"
    if sp in ACC:
        return f"{ACC[sp]}.{D} acc, vf{fs}, vf{ft}"
    return None


def dis_lower(lo, pc, label):
    op, it, is_, id_, d = lo >> 25, lo >> 16 & 31, lo >> 11 & 31, lo >> 6 & 31, lo >> 21 & 15
    imm11 = (lo & 0x7FF) - (0x800 if lo & 0x400 else 0)
    imm15 = (lo >> 10 & 0x7800) | (lo & 0x7FF)
    D = dest(d)
    is_branch = op in (0x20, 0x21) or op in BR2 or op in BR1
    tgt = label(pc + 1 + imm11) if is_branch else None   # only a branch names a target (labels are collected through this)
    if op == 0x00: return f"lq.{D} vf{it}, {imm11}(vi{is_})"
    if op == 0x01: return f"sq.{D} vf{is_}, {imm11}(vi{it})"
    if op == 0x04: return f"ilw.{D} vi{it}, {imm11}(vi{is_})"
    if op == 0x05: return f"isw.{D} vi{it}, {imm11}(vi{is_})"
    if op == 0x08: return f"iaddiu vi{it}, vi{is_}, {imm15:#x}"
    if op == 0x09: return f"isubiu vi{it}, vi{is_}, {imm15:#x}"
    if op in FC: return f"{FC[op]} vi1, {lo & 0xFFFFFF:#x}"
    if op == 0x1C: return f"fcget vi{it}"
    if op == 0x20: return f"b {tgt}"
    if op == 0x21: return f"bal vi{it}, {tgt}"
    if op == 0x24: return f"jr vi{is_}"
    if op == 0x25: return f"jalr vi{it}, vi{is_}"
    if op in BR2: return f"{BR2[op]} vi{it}, vi{is_}, {tgt}"
    if op in BR1: return f"{BR1[op]} vi{is_}, {tgt}"
    if op != 0x40:
        return None
    f = lo & 63
    if f in INT3: return f"{INT3[f]} vi{id_}, vi{is_}, vi{it}"
    if f == 0x32: return f"iaddi vi{it}, vi{is_}, {(lo >> 6 & 31) - (32 if lo & 0x400 else 0)}"
    if f < 0x3C:
        return None
    sp = id_ << 2 | (lo & 3)
    fsf, ftf = XYZW[lo >> 21 & 3], XYZW[lo >> 23 & 3]
    if lo == 0x8000033C: return "nop"
    n = LSP.get(sp)
    if n in ("move", "mr32"): return f"{n}.{D} vf{it}, vf{is_}"
    if n in ("lqi",): return f"lqi.{D} vf{it}, (vi{is_}++)"
    if n == "sqi": return f"sqi.{D} vf{is_}, (vi{it}++)"
    if n == "lqd": return f"lqd.{D} vf{it}, (--vi{is_})"
    if n == "sqd": return f"sqd.{D} vf{is_}, (--vi{it})"
    if n == "div": return f"div q, vf{is_}{fsf}, vf{it}{ftf}"
    if n == "sqrt": return f"sqrt q, vf{it}{ftf}"
    if n == "rsqrt": return f"rsqrt q, vf{is_}{fsf}, vf{it}{ftf}"
    if n == "waitq": return "waitq"
    if n == "mtir": return f"mtir vi{it}, vf{is_}{fsf}"
    if n == "mfir": return f"mfir.{D} vf{it}, vi{is_}"
    if n in ("ilwr", "iswr"): return f"{n}.{D} vi{it}, (vi{is_})"
    if n in ("xtop", "xitop"): return f"{n} vi{it}"
    if n == "xgkick": return f"xgkick vi{is_}"
    return None


# --------------------------------------------------------------------------------------------------- assembly

def reg(tok, kind):
    m = re.fullmatch(kind + r"(\d+)", tok)
    if not m or int(m.group(1)) > 31:
        raise ValueError(f"expected {kind} register, got '{tok}'")
    return int(m.group(1))


def regc(tok):
    """vfNc -> (N, component index)"""
    m = re.fullmatch(r"vf(\d+)([xyzw])", tok)
    if not m:
        raise ValueError(f"expected vfNc, got '{tok}'")
    return int(m.group(1)), XYZW.index(m.group(2))


def dmask(s):
    if s is None:
        return 0
    if not re.fullmatch(r"x?y?z?w?", s):
        raise ValueError(f"bad destination mask '{s}'")
    return sum(1 << (3 - XYZW.index(c)) for c in s)


def split_ins(text):
    text = text.strip()
    m = re.match(r"(\S+)\s*(.*)", text)
    name, rest = m.group(1), m.group(2)
    mask = None
    if "." in name:
        name, mask = name.split(".", 1)
    ops = [o.strip() for o in rest.split(",")] if rest else []
    return name, mask, ops


def asm_upper(text):
    name, mask, o = split_ins(text)
    if name == "raw":
        return int(o[0], 0)
    if name == "nop":
        return 0x2FF
    d = dmask(mask) << 21
    if name in inv(QI):
        return d | reg(o[1], "vf") << 11 | reg(o[0], "vf") << 6 | inv(QI)[name]
    if name in inv(CV):
        sp = inv(CV)[name]
        return d | reg(o[0], "vf") << 16 | reg(o[1], "vf") << 11 | (sp >> 2) << 6 | 0x3C | (sp & 3)
    if name == "clip":
        return d | reg(o[1], "vf") << 16 | reg(o[0], "vf") << 11 | (0x1F >> 2) << 6 | 0x3C | 3
    if o and o[0] == "acc":
        if name in inv(ACC) and re.fullmatch(r"vf\d+", o[2]):
            sp = inv(ACC)[name]
            return d | reg(o[2], "vf") << 16 | reg(o[1], "vf") << 11 | (sp >> 2) << 6 | 0x3C | (sp & 3)
        if name[:-1] in inv(ABC) and name[-1] in XYZW:
            ft, bc = regc(o[2])
            if bc != XYZW.index(name[-1]):
                raise ValueError("component of the mnemonic and of the operand differ")
            sp = inv(ABC)[name[:-1]] | bc
            return d | ft << 16 | reg(o[1], "vf") << 11 | (sp >> 2) << 6 | 0x3C | (sp & 3)
        raise ValueError(f"unknown upper instruction '{text}'")
    if name in inv(N3) and len(o) == 3 and re.fullmatch(r"vf\d+", o[2]):
        return d | reg(o[2], "vf") << 16 | reg(o[1], "vf") << 11 | reg(o[0], "vf") << 6 | inv(N3)[name]
    if name[:-1] in inv(BC) and name[-1] in XYZW:
        ft, bc = regc(o[2])
        if bc != XYZW.index(name[-1]):
            raise ValueError("component of the mnemonic and of the operand differ")
        return d | ft << 16 | reg(o[1], "vf") << 11 | reg(o[0], "vf") << 6 | inv(BC)[name[:-1]] | bc
    raise ValueError(f"unknown upper instruction '{text}'")


def mem(tok):
    m = re.fullmatch(r"(-?\d+)\(vi(\d+)\)", tok)
    if not m:
        raise ValueError(f"expected imm(viN), got '{tok}'")
    imm = int(m.group(1))
    if not -1024 <= imm < 1024:
        raise ValueError("offset out of range")
    return imm & 0x7FF, int(m.group(2))


def asm_lower(text, pc, labels):
    name, mask, o = split_ins(text)
    if name == "raw" or name == "loi":
        return int(o[0], 0)
    if name == "nop":
        return 0x8000033C
    d = dmask(mask) << 21

    def rel(tok):
        if tok not in labels:
            raise ValueError(f"unknown label '{tok}'")
        v = labels[tok] - (pc + 1)
        if not -1024 <= v < 1024:
            raise ValueError("branch out of range")
        return v & 0x7FF

    def sp(code, it=0, is_=0):
        return 0x40 << 25 | d | it << 16 | is_ << 11 | (code >> 2) << 6 | 0x3C | (code & 3)

    if name in ("lq", "ilw"):
        imm, base = mem(o[1])
        return (0x00 if name == "lq" else 0x04) << 25 | d | reg(o[0], "vf" if name == "lq" else "vi") << 16 | base << 11 | imm
    if name == "sq":
        imm, base = mem(o[1])
        return 0x01 << 25 | d | base << 16 | reg(o[0], "vf") << 11 | imm
    if name == "isw":
        imm, base = mem(o[1])
        return 0x05 << 25 | d | reg(o[0], "vi") << 16 | base << 11 | imm
    if name in ("iaddiu", "isubiu"):
        imm = int(o[2], 0)
        if not 0 <= imm < 0x8000:
            raise ValueError("immediate out of range")
        return (0x08 if name == "iaddiu" else 0x09) << 25 | (imm & 0x7800) << 10 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11 | (imm & 0x7FF)
    if name in inv(FC):
        return inv(FC)[name] << 25 | (int(o[1], 0) & 0xFFFFFF)
    if name == "fcget":
        return 0x1C << 25 | reg(o[0], "vi") << 16
    if name == "b":
        return 0x20 << 25 | rel(o[0])
    if name == "bal":
        return 0x21 << 25 | reg(o[0], "vi") << 16 | rel(o[1])
    if name == "jr":
        return 0x24 << 25 | reg(o[0], "vi") << 11
    if name == "jalr":
        return 0x25 << 25 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11
    if name in inv(BR2):
        return inv(BR2)[name] << 25 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11 | rel(o[2])
    if name in inv(BR1):
        return inv(BR1)[name] << 25 | reg(o[0], "vi") << 11 | rel(o[1])
    if name in inv(INT3):
        return 0x40 << 25 | reg(o[2], "vi") << 16 | reg(o[1], "vi") << 11 | reg(o[0], "vi") << 6 | inv(INT3)[name]
    if name == "iaddi":
        imm = int(o[2], 0)
        if not -16 <= imm < 16:
            raise ValueError("immediate out of range")
        return 0x40 << 25 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11 | (imm & 31) << 6 | 0x32
    code = inv(LSP).get(name)
    if code is None:
        raise ValueError(f"unknown lower instruction '{text}'")
    if name in ("move", "mr32"):
        return sp(code, reg(o[0], "vf"), reg(o[1], "vf"))
    if name in ("lqi", "lqd", "sqi", "sqd"):
        m = re.fullmatch(r"\(vi(\d+)\+\+\)" if name[2] == "i" else r"\(--vi(\d+)\)", o[1])
        if not m:
            raise ValueError(f"bad address operand '{o[1]}'")
        vf, vi = reg(o[0], "vf"), int(m.group(1))
        return sp(code, vf, vi) if name[0] == "l" else sp(code, vi, vf)
    if name in ("div", "rsqrt"):
        (fs, fsf), (ft, ftf) = regc(o[1]), regc(o[2])
        return 0x40 << 25 | ftf << 23 | fsf << 21 | ft << 16 | fs << 11 | (code >> 2) << 6 | 0x3C | (code & 3)
    if name == "sqrt":
        ft, ftf = regc(o[1])
        return 0x40 << 25 | ftf << 23 | ft << 16 | (code >> 2) << 6 | 0x3C | (code & 3)
    if name == "waitq":
        return sp(code)
    if name == "mtir":
        fs, fsf = regc(o[1])
        return 0x40 << 25 | fsf << 21 | reg(o[0], "vi") << 16 | fs << 11 | (code >> 2) << 6 | 0x3C | (code & 3)
    if name == "mfir":
        return sp(code, reg(o[0], "vf"), reg(o[1], "vi"))
    if name in ("ilwr", "iswr"):
        m = re.fullmatch(r"\(vi(\d+)\)", o[1])
        return sp(code, reg(o[0], "vi"), int(m.group(1)))
    if name in ("xtop", "xitop"):
        return sp(code, reg(o[0], "vi"))
    if name == "xgkick":
        return sp(code, 0, reg(o[0], "vi"))
    raise ValueError(f"unknown lower instruction '{text}'")


FLAGS = {"I": 31, "E": 30, "M": 29, "D": 28, "T": 27}


def assemble(path):
    """One listing -> bytes."""
    items = []   # ("vif", word) | ("mpg", addr) | ("ins", upper, lower, flags, line number)
    labels = {}
    pc = None
    for num, line in enumerate(open(path).read().splitlines(), 1):
        line = line.split(";", 1)[0].strip()
        while True:
            m = re.match(r"([A-Za-z_]\w*):\s*", line)
            if not m:
                break
            if pc is None:
                raise SystemExit(f"{path}:{num}: label outside a .mpg block")
            if m.group(1) in labels:
                raise SystemExit(f"{path}:{num}: label '{m.group(1)}' defined twice")
            labels[m.group(1)] = pc
            line = line[m.end():]
        if not line:
            continue
        if line.startswith(".vif"):
            items.append(("vif", int(line.split()[1], 0)))
            pc = None
        elif line.startswith(".mpg"):
            pc = int(line.split()[1], 0)
            items.append(("mpg", pc))
        else:
            if pc is None:
                raise SystemExit(f"{path}:{num}: instruction outside a .mpg block")
            flags = 0
            m = re.search(r"\[([IEMDT]+)\]\s*$", line)
            if m:
                flags = sum(1 << FLAGS[c] for c in m.group(1))
                line = line[:m.start()]
            if "|" not in line:
                raise SystemExit(f"{path}:{num}: expected 'upper | lower'")
            up, lo = line.split("|", 1)
            items.append(("ins", up, lo, flags, num, pc))
            pc += 1
    out = []
    i = 0
    while i < len(items):
        it = items[i]
        if it[0] == "vif":
            out.append(it[1])
            i += 1
            continue
        if it[0] == "mpg":
            j = i + 1
            while j < len(items) and items[j][0] == "ins":
                j += 1
            n = j - i - 1
            if not 1 <= n <= 256:
                raise SystemExit(f"{path}: a .mpg block has {n} instructions (1..256 allowed)")
            out.append(0x4A000000 | (n & 0xFF) << 16 | it[1])
            for _, up, lo, flags, num, pc in items[i + 1:j]:
                try:
                    out.append(asm_lower(lo, pc, labels) & 0xFFFFFFFF)
                    out.append((asm_upper(up) | flags) & 0xFFFFFFFF)
                except (ValueError, IndexError, AttributeError) as e:
                    raise SystemExit(f"{path}:{num}: {e}")
            i = j
    return struct.pack(f"<{len(out)}I", *out)


def disassemble(data, name):
    """A VIF packet (NOPs and MPG codes) -> listing text. Every instruction is assembled again and compared; a half
    that does not come back the same is written as `raw`."""
    words = struct.unpack(f"<{len(data) // 4}I", data)
    chunks, targets, i = [], set(), 0
    while i < len(words):
        c = words[i]
        if (c >> 24) & 0x7F == 0x4A:
            n = (c >> 16) & 0xFF or 256
            addr = c & 0xFFFF
            chunks.append(("mpg", addr, words[i + 1:i + 1 + n * 2]))
            i += 1 + n * 2
        else:
            chunks.append(("vif", c))
            i += 1
    label = lambda a: f"L{a:03d}"
    for ch in chunks:   # first pass: branch targets
        if ch[0] == "mpg":
            for k in range(len(ch[2]) // 2):
                lo, hi = ch[2][k * 2], ch[2][k * 2 + 1]
                if not hi >> 31 & 1:
                    dis_lower(lo, ch[1] + k, lambda a: targets.add(a) or label(a))
    labels = {label(a): a for a in targets}
    out = [f"; {name}", ""]
    raws = 0
    for ch in chunks:
        if ch[0] == "vif":
            out.append(f".vif 0x{ch[1]:08X}")
            continue
        out.append(f".mpg {ch[1]}")
        for k in range(len(ch[2]) // 2):
            lo, hi = ch[2][k * 2], ch[2][k * 2 + 1]
            pc = ch[1] + k
            flags = "".join(c for c, b in FLAGS.items() if hi >> b & 1)
            body = hi & 0x07FFFFFF
            up = dis_upper(body)
            try:
                if up is None or asm_upper(up) != body:
                    up = None
            except (ValueError, IndexError):
                up = None
            if up is None:
                up, raws = f"raw 0x{body:08X}", raws + 1
            if hi >> 31 & 1:
                low = f"loi 0x{lo:08X}"
                note = f"   ; {struct.unpack('<f', struct.pack('<I', lo))[0]:.9g}"
            else:
                note = ""
                low = dis_lower(lo, pc, label)
                try:
                    if low is None or asm_lower(low, pc, labels) != lo:
                        low = None
                except (ValueError, IndexError, AttributeError):
                    low = None
                if low is None:
                    low, raws = f"raw 0x{lo:08X}", raws + 1
            if pc in targets:
                out.append(f"{label(pc)}:")
            out.append(f"    {up:34s} | {low}{(' [' + flags + ']') if flags else ''}{note}".rstrip())
    return "\n".join(out) + "\n", raws


def main():
    a = sys.argv[1:]
    if a and a[0] == "--disasm":
        data = open(a[1], "rb").read()[int(a[2], 0):int(a[3], 0)]
        text, raws = disassemble(data, a[4] if len(a) > 4 else "")
        sys.stdout.write(text)
        print(f"{raws} raw words", file=sys.stderr)
    elif len(a) >= 3 and a[0] == "-o":
        open(a[1], "wb").write(b"".join(assemble(p) for p in a[2:]))
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
