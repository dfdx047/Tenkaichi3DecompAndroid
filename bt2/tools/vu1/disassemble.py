"""Lists a VU1 microprogram of a program census as text, for reading it.

Usage: disassemble.py CENSUS_FILE

One line per instruction pair: its index, the upper instruction and the lower instruction, in the
order the unit runs them. Covers the instructions the VU1 model implements (runtime/vu/vu1.cpp);
anything else is shown as a word. The output describes the game's own program: it is for local
reading and is never committed.
"""
import struct, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from translate_programs import read_program, imm11

FIELDS = 'xyzw'


def dest(word):
    mask = (word >> 21) & 15
    return ''.join(f for n, f in enumerate(FIELDS) if mask & (8 >> n))


def upper(word):
    ft, fs, fd, op = (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31, word & 0x3F
    d = dest(word)
    if op < 0x3C:
        broadcast = {0x00: 'ADD', 0x04: 'SUB', 0x08: 'MADD', 0x0C: 'MSUB', 0x10: 'MAX', 0x14: 'MINI', 0x18: 'MUL'}
        if op & 0x3C in broadcast and op < 0x1C: return f'{broadcast[op & 0x3C]}{FIELDS[op & 3]}.{d} vf{fd}, vf{fs}, vf{ft}{FIELDS[op & 3]}'
        other = {0x1C: ('MULq', 'Q'), 0x1D: ('MAXi', 'I'), 0x1E: ('MULi', 'I'), 0x1F: ('MINIi', 'I'), 0x20: ('ADDq', 'Q'), 0x21: ('MADDq', 'Q'),
                 0x22: ('ADDi', 'I'), 0x23: ('MADDi', 'I'), 0x24: ('SUBq', 'Q'), 0x25: ('MSUBq', 'Q'), 0x26: ('SUBi', 'I'), 0x27: ('MSUBi', 'I')}
        if op in other: return f'{other[op][0]}.{d} vf{fd}, vf{fs}, {other[op][1]}'
        vector = {0x28: 'ADD', 0x29: 'MADD', 0x2A: 'MUL', 0x2B: 'MAX', 0x2C: 'SUB', 0x2D: 'MSUB', 0x2E: 'OPMSUB', 0x2F: 'MINI'}
        if op in vector: return f'{vector[op]}.{d} vf{fd}, vf{fs}, vf{ft}'
        return f'.word 0x{word:08X}'
    code = (fd << 2) | (op & 3)
    broadcast = {0x00: 'ADDA', 0x04: 'SUBA', 0x08: 'MADDA', 0x0C: 'MSUBA', 0x18: 'MULA'}
    if code & 0x7C in broadcast and code < 0x1C and code & 0x7C != 0x10 and code & 0x7C != 0x14:
        return f'{broadcast[code & 0x7C]}{FIELDS[code & 3]}.{d} ACC, vf{fs}, vf{ft}{FIELDS[code & 3]}'
    if 0x10 <= code < 0x18: return f"{'ITOF' if code < 0x14 else 'FTOI'}{(0, 4, 12, 15)[code & 3]}.{d} vf{ft}, vf{fs}"
    other = {0x1C: ('MULAq', 'Q'), 0x1E: ('MULAi', 'I'), 0x20: ('ADDAq', 'Q'), 0x21: ('MADDAq', 'Q'), 0x22: ('ADDAi', 'I'), 0x23: ('MADDAi', 'I'),
             0x24: ('SUBAq', 'Q'), 0x25: ('MSUBAq', 'Q'), 0x26: ('SUBAi', 'I'), 0x27: ('MSUBAi', 'I')}
    if code in other: return f'{other[code][0]}.{d} ACC, vf{fs}, {other[code][1]}'
    vector = {0x28: 'ADDA', 0x29: 'MADDA', 0x2A: 'MULA', 0x2C: 'SUBA', 0x2D: 'MSUBA', 0x2E: 'OPMULA'}
    if code in vector: return f'{vector[code]}.{d} ACC, vf{fs}, vf{ft}'
    if code == 0x1D: return f'ABS.{d} vf{ft}, vf{fs}'
    if code == 0x1F: return f'CLIP vf{fs}, vf{ft}w'
    if code == 0x2F: return 'NOP'
    return f'.word 0x{word:08X}'


def lower(word, at):
    op, it, is_, d = word >> 25, (word >> 16) & 31, (word >> 11) & 31, dest(word)
    imm, target = imm11(word), (at + 1 + imm11(word)) & 0x7FF
    simple = {0x00: f'LQ.{d} vf{it}, {imm}(vi{is_})', 0x01: f'SQ.{d} vf{is_}, {imm}(vi{it})', 0x04: f'ILW.{d} vi{it}, {imm}(vi{is_})',
              0x05: f'ISW.{d} vi{it}, {imm}(vi{is_})', 0x12: f'FCAND vi1, 0x{word & 0xFFFFFF:06X}', 0x13: f'FCOR vi1, 0x{word & 0xFFFFFF:06X}',
              0x1C: f'FCGET vi{it}', 0x20: f'B {target}', 0x21: f'BAL vi{it}, {target}', 0x24: f'JR vi{is_}',
              0x28: f'IBEQ vi{it}, vi{is_}, {target}', 0x29: f'IBNE vi{it}, vi{is_}, {target}', 0x2C: f'IBLTZ vi{is_}, {target}',
              0x2D: f'IBGTZ vi{is_}, {target}', 0x2E: f'IBLEZ vi{is_}, {target}', 0x2F: f'IBGEZ vi{is_}, {target}'}
    if op in simple: return simple[op]
    if op in (0x08, 0x09): return f"{'IADDIU' if op == 8 else 'ISUBIU'} vi{it}, vi{is_}, {((word >> 10) & 0x7800) | (word & 0x7FF)}"
    if op != 0x40: return f'.word 0x{word:08X}'
    function, id_ = word & 0x3F, (word >> 6) & 31
    if function < 0x3C:
        integer = {0x30: 'IADD', 0x31: 'ISUB', 0x34: 'IAND', 0x35: 'IOR'}
        if function in integer: return f'{integer[function]} vi{id_}, vi{is_}, vi{it}'
        if function == 0x32: return f'IADDI vi{it}, vi{is_}, {id_ - ((id_ & 16) << 1)}'
        return f'.word 0x{word:08X}'
    code = (id_ << 2) | (word & 3)
    special = {0x30: 'NOP' if not it else f'MOVE.{d} vf{it}, vf{is_}', 0x31: f'MR32.{d} vf{it}, vf{is_}', 0x34: f'LQI.{d} vf{it}, (vi{is_}++)',
               0x35: f'SQI.{d} vf{is_}, (vi{it}++)', 0x36: f'LQD.{d} vf{it}, (--vi{is_})', 0x37: f'SQD.{d} vf{is_}, (--vi{it})',
               0x38: f'DIV Q, vf{is_}{FIELDS[(word >> 21) & 3]}, vf{it}{FIELDS[(word >> 23) & 3]}', 0x3B: 'WAITQ',
               0x3E: f'ILWR.{d} vi{it}, (vi{is_})', 0x3F: f'ISWR.{d} vi{it}, (vi{is_})', 0x68: f'XTOP vi{it}', 0x69: f'XITOP vi{it}', 0x6C: f'XGKICK vi{is_}'}
    return special.get(code, f'.word 0x{word:08X}')


def lines(start, pairs):
    """The program as text lines; the start address is marked."""
    result = []
    for at in sorted(pairs):
        low, high = pairs[at]
        flags = ('E' if high & 0x40000000 else ' ') + ('>' if at == start else ' ')
        if high >> 31: text = f'{upper(high):<34} I = {struct.unpack("<f", struct.pack("<I", low))[0]!r}'
        else: text = f'{upper(high):<34} {lower(low, at)}'
        result.append(f'{at:4d} {flags} {text}')
    return result


if __name__ == '__main__':
    if len(sys.argv) != 2: sys.exit(__doc__)
    print('\n'.join(lines(*read_program(sys.argv[1]))))
