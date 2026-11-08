"""Translates the VU1 microprograms of a program census into C++ (native translations).

Usage: translate_programs.py CENSUS_FOLDER OUTPUT.cpp

The census (BT2_VU1_CENSUS) holds one file per program a run started: the pairs reachable from its
start address. The output defines one function per program and `Vu1::nativePrograms()`. It is derived
from the game's own programs: it stays in the ignored game/generated/ and is never committed.

A translated program does exactly what the interpreter (runtime/vu/vu1.cpp) does pair by pair. The
frequent instructions are written out with their operands as constants (arithmetic, loads and
stores); every other instruction calls the interpreter's own `upper` and `lower`, so its behaviour
is the interpreter's by construction. Control flow becomes labels and gotos. A program the
translator cannot follow exactly is left out and stays interpreted; the reason is printed.
"""
import argparse, struct, sys
from pathlib import Path

MASK = 0x7FF
BRANCHES = {0x20, 0x21, 0x24, 0x28, 0x29, 0x2C, 0x2D, 0x2E, 0x2F}
ADD, SUB, MADD, MSUB, MUL = range(5)
VECTOR, FIELD, REG_Q, REG_I = range(4)


class Untranslatable(Exception):
    pass


def read_program(path):
    """Returns (start, {address: (low, high)}) of a census file."""
    data = Path(path).read_bytes()
    start, count = struct.unpack_from('<II', data, 0)
    if len(data) != 8 + count * 12: raise ValueError(f'{path}: size does not match its pair count')
    pairs = {}
    for n in range(count):
        address, low, high = struct.unpack_from('<III', data, 8 + n * 12)
        pairs[address] = (low, high)
    return start, pairs


def imm11(word):
    return (word & 0x7FF) - ((word & 0x400) << 1)


def shared_arithmetic(code):
    """The codes 0x1C-0x2D shared by the register and accumulator forms: (operation, source) or None."""
    if code == 0x1C: return MUL, REG_Q
    if code == 0x1E: return MUL, REG_I
    if code == 0x2A: return MUL, VECTOR
    if code in (0x20, 0x21, 0x24, 0x25): source = REG_Q
    elif code in (0x22, 0x23, 0x26, 0x27): source = REG_I
    elif code in (0x28, 0x29, 0x2C, 0x2D): source = VECTOR
    else: return None
    return (SUB if code & 4 else ADD) + (2 if code & 1 else 0), source


def decode(low, high):
    """What the run loop needs of a pair, as the interpreter's decoder derives it."""
    d = {'low': low, 'high': high, 'i_bit': bool(high >> 31), 'ending': bool((high >> 30) & 1), 'debug': bool(high & 0x18000000),
         'upper': 'slow', 'target': 0, 'lower': 'slow', 'branch': False}
    ft, fs, fd, op = (high >> 16) & 31, (high >> 11) & 31, (high >> 6) & 31, high & 0x3F
    d.update(fs=fs, ft=ft, field=high & 3, mask=(high >> 21) & 15)
    arithmetic = None
    if op < 0x3C:
        arithmetic = (op >> 2, FIELD) if op < 0x10 else (MUL, FIELD) if 0x18 <= op < 0x1C else shared_arithmetic(op)
        if arithmetic: d.update(upper='register', reg=fd, target=fd)
    else:
        code = (((high >> 6) & 0x1F) << 2) | (high & 3)
        if code == 0x2F: d['upper'] = 'nop'
        elif 0x10 <= code < 0x18 or code == 0x1D: d['target'] = ft
        elif code != 0x1F:
            arithmetic = (code >> 2, FIELD) if code < 0x10 else (MUL, FIELD) if 0x18 <= code < 0x1C else shared_arithmetic(code)
            if arithmetic: d['upper'] = 'accumulator'
    if arithmetic and d['upper'] != 'slow': d['operation'], d['source'] = arithmetic
    if d['i_bit']: return d
    lop, it, is_ = low >> 25, (low >> 16) & 31, (low >> 11) & 31
    lower_code = (((low >> 6) & 31) << 2) | (low & 3) if lop == 0x40 and (low & 0x3F) >= 0x3C else 0x100
    writes_float = lop == 0 or lower_code in (0x30, 0x31, 0x34, 0x36)
    d['skip_lower'] = bool(d['target'] and writes_float and it == d['target'])
    d.update(lower_mask=(low >> 21) & 15, lower_imm=imm11(low))
    if lop == 0x00 and is_ < 16: d.update(lower='lq', lower_vf=it, lower_vi=is_)
    elif lop == 0x01 and it < 16: d.update(lower='sq', lower_vf=is_, lower_vi=it)
    elif lower_code == 0x34 and is_ < 16: d.update(lower='lqi', lower_vf=it, lower_vi=is_)
    elif lower_code == 0x35 and it < 16: d.update(lower='sqi', lower_vf=is_, lower_vi=it)
    elif lower_code == 0x30 and not it: d['lower'] = 'nop'
    d['branch'] = lop in BRANCHES and not d['skip_lower']
    d['lop'] = lop
    return d


def followed_pairs(start, pairs):
    """The addresses whose successors belong to the program (vu1_program.h's rule), in address order."""
    followed, work, returns, indirect = set(), [start & MASK], [], False
    followed.add(start & MASK)
    def visit(address):
        address &= MASK
        if address not in followed and address in pairs: followed.add(address); work.append(address)
    done = 0
    while True:
        if done == len(work):
            before = len(work)
            if indirect:
                for r in returns: visit(r)
            if len(work) == before: break
        at = work[done]; done += 1
        low, high = pairs[at]
        if (high >> 30) & 1 or high >> 31:
            if not (high >> 30) & 1: visit(at + 1)
            continue
        op = low >> 25; target = at + 1 + imm11(low)
        if op == 0x20: visit(target)
        elif op == 0x21: visit(target); returns.append((at + 2) & MASK)
        elif op == 0x24: indirect = True
        elif op in (0x28, 0x29, 0x2C, 0x2D, 0x2E, 0x2F): visit(target); visit(at + 2)
        else: visit(at + 1)
    return sorted(followed)


def emit_pair(at, d, out, indent='        '):
    """The code of one pair: the body of the interpreter's loop for it, without the control flow."""
    w = lambda text: out.append(indent + text)
    low, high = d['low'], d['high']
    if d['debug']: raise Untranslatable(f'D/T bit at {at}')
    w(f'{{ // {at}: {high:08X} {low:08X}')
    w('if(vu.q_pending_ || vu.clip_count_)vu.settle(false);')
    if d['i_bit']: w(f'vu.i=0x{low:08X}u;')
    commit = None
    if d['upper'] in ('register', 'accumulator'):
        second = {VECTOR: f'lanes(vu.vf[{d["ft"]}])', FIELD: f'_mm_castsi128_ps(_mm_set1_epi32(int(vu.vf[{d["ft"]}][{d["field"]}])))',
                  REG_Q: '_mm_castsi128_ps(_mm_set1_epi32(int(vu.readQ())))', REG_I: '_mm_castsi128_ps(_mm_set1_epi32(int(vu.i)))'}[d['source']]
        w(f'{{const __m128 a=clamp(lanes(vu.vf[{d["fs"]}])),b=clamp({second});')
        if d['operation'] in (ADD, SUB, MUL):
            w(f' up=_mm_castps_si128(clamp(_mm_{("add", "sub", None, None, "mul")[d["operation"]]}_ps(a,b)));}}')
        else:
            w(f' up=_mm_castps_si128(clamp(_mm_{"add" if d["operation"] == MADD else "sub"}_ps(clamp(lanes(vu.acc)),clamp(_mm_mul_ps(a,b)))));}}')
        if d['upper'] == 'accumulator': commit = f'storeMasked(vu.acc.data(),up,{d["mask"]});'
        elif d['reg']: commit = f'storeMasked(vu.vf[{d["reg"]}].data(),up,{d["mask"]});'
    elif d['upper'] == 'slow':
        w(f'const auto result{at}=vu.upper(0x{high:08X}u,{at});')
        commit = (f'if(result{at}.target==Vu1::Result::Register && result{at}.reg)storeMasked(vu.vf[result{at}.reg].data(),load4(result{at}.value.data()),result{at}.mask);'
                  f'else if(result{at}.target==Vu1::Result::Accumulator)storeMasked(vu.acc.data(),load4(result{at}.value.data()),result{at}.mask);')
    if not d['i_bit'] and not d['skip_lower']:
        kind = d['lower']
        vf, vi, mask, imm = d.get('lower_vf'), d.get('lower_vi'), d.get('lower_mask'), d.get('lower_imm')
        if kind == 'lq':
            if vf: w(f'storeMasked(vu.vf[{vf}].data(),load4(vu.cell(std::uint32_t(vu.vi[{vi}]+({imm})))),{mask});')
        elif kind == 'sq': w(f'storeMasked(vu.cell(std::uint32_t(vu.vi[{vi}]+({imm}))),load4(vu.vf[{vf}].data()),{mask});')
        elif kind == 'lqi':
            if vf: w(f'storeMasked(vu.vf[{vf}].data(),load4(vu.cell(vu.vi[{vi}])),{mask});')
            w(f'vu.setVi({vi},std::uint16_t(vu.vi[{vi}]+1));')
        elif kind == 'sqi':
            w(f'storeMasked(vu.cell(vu.vi[{vi}]),load4(vu.vf[{vf}].data()),{mask});')
            w(f'vu.setVi({vi},std::uint16_t(vu.vi[{vi}]+1));')
        elif kind == 'slow': w(f'vu.lower(0x{low:08X}u,{at},kick);')
    if commit: w(commit)
    w('++vu.cycle_;++vu.instructions;++executed;}')


def translate(start, pairs, key):
    """Returns the lines of the function for one program."""
    decoded = {a: decode(*pairs[a]) for a in pairs}
    followed = followed_pairs(start, pairs)
    labels = set(followed)
    out = [f'    static bool program_{key}(Vu1& vu,const Vu1::Kick& kick) {{',
           '        std::uint64_t executed=0;std::uint32_t target=0;__m128i up=_mm_setzero_si128();(void)up;',
           f'        if(vu.pc!={start})return false;',
           f'        goto L{start};']
    def go(address, indent):
        address &= MASK
        return f'{indent}goto L{address};' if address in labels else f'{indent}{{vu.pc={address};return false;}}'
    for index, at in enumerate(followed):
        d = decoded[at]
        out.append(f'    L{at}:')
        emit_pair(at, d, out)
        nxt = (at + 1) & MASK
        if d['ending'] or d['branch']:
            # The pair that follows runs before the program ends or the branch takes effect.
            if nxt not in pairs: raise Untranslatable(f'pair {at} is followed by a pair outside the program')
            delay = decoded[nxt]
            if d['branch']:
                if delay['branch']: raise Untranslatable(f'branch at {nxt} in the delay slot of {at}')
                if delay['ending']: raise Untranslatable(f'E bit at {nxt} in the delay slot of {at}')
                out.append(f'        {{const bool taken{at}=vu.branch_;target=vu.branch_target_;vu.branch_=false;')
                emit_pair(nxt, delay, out, '         ')
                after = (at + 2) & MASK
                if d['ending']:
                    out.append(f'         vu.pc=taken{at}?target:{after};return true;}}')
                    continue
                out.append(f'         if(taken{at}) {{')
                out.append(f'             if(executed>=Vu1::run_limit)throw std::runtime_error("VU1 microprogram does not end");')
                if d['lop'] == 0x24: out.append('             goto dispatch;')
                else: out.append(go(at + 1 + imm11(d['low']), '             '))
                out.append('         }')
                if d['lop'] in (0x20, 0x21, 0x24): out.append(f'         {{vu.pc={after};return false;}}}}')  # An unconditional branch not taken cannot happen; hand over if it does.
                else: out.append(go(after, '         ') + '}')
            else:
                emit_pair(nxt, delay, out)
                out.append(f'        vu.pc={(at + 2) & MASK};return true;')
            continue
        # Straight on: fall through when the next pair is emitted next, otherwise jump or hand over.
        if index + 1 < len(followed) and followed[index + 1] == nxt: continue
        out.append(go(nxt, '        '))
    out.append('    dispatch:')
    out.append('        switch(target) {')
    for at in followed: out.append(f'            case {at}:goto L{at};')
    out.append('            default:vu.pc=target;return false;')
    out.append('        }')
    out.append('    }')
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('census', type=Path); p.add_argument('output', type=Path)
    a = p.parse_args()
    files = sorted(a.census.glob('*.vu1'))
    if not files: sys.exit(f'No census files in {a.census}')
    lines = ['// Generated by tools/vu1/translate_programs.py from a VU1 program census. Derived from the game\'s own',
             '// microprograms: local only, never committed.',
             '#include "vu/vu1.h"', '#include "vu/vu1_simd.h"', '#include <stdexcept>', '', 'using namespace vu1_simd;', '',
             '#pragma warning(disable:4102) // Labels that only some programs jump to.', '',
             'struct Vu1Native {']
    done = []
    for path in files:
        key = path.stem
        start, pairs = read_program(path)
        try:
            lines += translate(start, pairs, key); done.append(key)
            print(f'{key}: translated, start {start}, {len(pairs)} pairs')
        except Untranslatable as reason:
            print(f'{key}: left to the interpreter ({reason})')
    lines += ['};', '', 'std::span<const Vu1::NativeEntry> Vu1::nativePrograms() {',
              '    static const Vu1::NativeEntry programs[]={']
    lines += [f'        {{0x{key}ull,&Vu1Native::program_{key}}},' for key in done]
    lines += ['    };', '    return programs;', '}', '']
    if not done: sys.exit('No program could be translated')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text('\n'.join(lines), encoding='utf-8', newline='\n')
    print(f'{len(done)} of {len(files)} programs written to {a.output}')


if __name__ == '__main__':
    main()
