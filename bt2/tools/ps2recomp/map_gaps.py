"""Code the map does not cover although the programs themselves name it: what a run would stop at.

For the main executable and each overlay program, lists the addresses that are not function starts
of the map but are
- targets of JAL or J from mapped code (in unmapped code, or inside another function),
- data words that point at unmapped code right after a completed function,
- addresses mapped code builds (LUI with ADDIU or ORI) that lie in unmapped code,
- code in the gaps between functions that follows completed code although nothing names it,
- addresses inside a function that code builds or data holds and that have no label in the generated
  code (entry hints: the generator makes them labels, and the runtime registers labels as entries).
Kernel-mode code (exception handlers ending in ERET) is reported and never mapped.

`--write` adds the entries to the program's `entries.json`, which prepare_map.py turns into functions
(see its add_entries), and the hints to its `entry-hints.json`, which generate_game.py passes to the
generator; then regenerate and build, for example with
    python tools/ps2recomp/discovery_loop.py --rounds 0 --regenerate
    python tools/ps2recomp/discovery_loop.py --rounds 0 --regenerate dbzp
Without arguments it only reports. Everything it reads and writes is local and ignored.
"""
import argparse, bisect, csv, json, re, struct, sys
from pathlib import Path
import overlay_programs
ROOT = Path(__file__).resolve().parents[2]
FOLDER = ROOT / 'work/bt2-generation'


def find_gaps(rows, word, code_range, data_ranges):
    """(direct, inside, pointers): sorted addresses. `rows`: (start, end) of the map; `word(address)`
    reads a word or returns None; `code_range`: where code lies; `data_ranges`: where to look for pointers."""
    starts = sorted(s for s, e in rows); ends = dict(rows)
    lo, hi = code_range
    def owner(address):
        at = bisect.bisect_right(starts, address) - 1
        return starts[at] if at >= 0 and address < ends[starts[at]] else None
    direct, inside = set(), set()
    for s, e in rows:
        for at in range(s, e, 4):
            ins = word(at)
            if ins is None or ins >> 26 not in (2, 3):
                continue
            target = ((at + 4) & 0xF0000000) | ((ins & 0x03FFFFFF) << 2)
            if not lo <= target < hi or word(target) is None:
                continue
            holder = owner(target)
            if holder is None:
                direct.add(target)
            elif target != holder and (ins >> 26 == 3 or holder != owner(at)):
                inside.add(target) # A call into a function's middle, or a jump there from another function.
    def after_completed(address):
        # The code before it ended: a transfer and its delay slot, then at most three NOPs.
        at = address
        for _ in range(3):
            if word(at - 4) == 0 and word(at - 8) != 0x03E00008 and (word(at - 8) or 0) >> 26 != 2:
                at -= 4
        transfer = word(at - 8)
        return transfer is not None and (transfer == 0x03E00008 or transfer >> 26 == 2)
    pointers = set()
    for a, b in data_ranges:
        for at in range(a, b, 4):
            value = word(at)
            if value is not None and lo <= value < hi and not value & 3 and owner(value) is None and word(value) and after_completed(value):
                pointers.add(value)
    return sorted(direct), sorted(inside), sorted(pointers - direct)


JR_RA, ERET = 0x03E00008, 0x42000018


def transfer(ins):
    """A transfer after which code does not run on: JR RA, J, or the unconditional branch BEQ zero, zero."""
    return ins is not None and (ins == JR_RA or ins >> 26 == 2 or ins >> 16 == 0x1000)


def preceded_by(word, address, lowest):
    """What ends the code before `address`, padding NOPs skipped: 'transfer' (a transfer and its delay
    slot), 'eret' (ERET: kernel-mode code, which the port does not run) or None (code that runs on)."""
    at = address - 4
    while at >= lowest and word(at) == 0:
        at -= 4
    if at < lowest:
        return None
    # The last non-zero word is a delay slot after its transfer, or the transfer itself with a NOP
    # delay slot between it and the address. ERET has no delay slot.
    if word(at) == ERET or word(at - 4) == ERET:
        return 'eret'
    return 'transfer' if transfer(word(at - 4)) or (transfer(word(at)) and address >= at + 8) else None


def routine_end(word, entry, limit):
    """End of the routine at `entry` as prepare_map.add_entries builds it: after the first JR RA or J,
    with its delay slot, that no earlier forward branch reaches beyond; None if it does not end before
    `limit`."""
    sign = lambda v: v - 0x10000 if v & 0x8000 else v
    reach = entry
    for at in range(entry, limit, 4):
        ins = word(at)
        if ins is None:
            return None
        op = ins >> 26
        if op in (4, 5, 6, 7, 20, 21, 22, 23) or (op == 1 and (ins >> 16) & 31 in (0, 1, 2, 3, 16, 17, 18, 19)):
            reach = max(reach, at + 4 + sign(ins & 0xFFFF) * 4)
        if (ins == JR_RA or op == 2) and at + 8 > reach:
            return at + 8 if at + 8 <= limit else None
    return None


def built_addresses(rows, word):
    """Values built in mapped code by LUI followed by ADDIU or ORI on its register, in address order
    within each function (a later write to the register forgets the LUI)."""
    found = set()
    sign = lambda v: v - 0x10000 if v & 0x8000 else v
    for s, e in rows:
        high = {}
        for at in range(s, e, 4):
            ins = word(at)
            if ins is None:
                continue
            op, rs, rt = ins >> 26, (ins >> 21) & 31, (ins >> 16) & 31
            if op == 15:
                high[rt] = (ins & 0xFFFF) << 16
                continue
            if op in (9, 13) and rs in high:
                found.add((high[rs] + sign(ins & 0xFFFF)) & 0xFFFFFFFF if op == 9 else high[rs] | (ins & 0xFFFF))
            if 8 <= op <= 14 or 24 <= op <= 25 or 32 <= op <= 39:
                high.pop(rt, None) # An immediate operation or a load wrote rt.
            elif op == 0:
                high.pop((ins >> 11) & 31, None)
    return found


def find_code(rows, word, code_range):
    """(built, unreferenced, kernel): sorted addresses of code no function holds.

    built: addresses built in mapped code (built_addresses) that lie in unmapped code right after
    completed code. unreferenced: in the gaps between functions, each non-zero word that follows
    completed code (padding skipped); nothing names it as a constant, but it is code (small leaf
    routines reached through tables built at run time). kernel: such code after an ERET, or running
    into one before any JR RA: exception handlers the game installs for the kernel, which the port
    does not run; reported, never mapped."""
    lo, hi = code_range
    starts = sorted(s for s, e in rows); ends = dict(rows)
    def owner(address):
        at = bisect.bisect_right(starts, address) - 1
        return starts[at] if at >= 0 and address < ends[starts[at]] else None
    def runs_into_eret(address, end):
        for at in range(address, end, 4):
            if word(at) == JR_RA:
                return False
            if word(at) == ERET:
                return True
        return False
    gaps, previous = [], lo
    for s, e in sorted(rows) + [(hi, hi)]:
        if s > previous:
            gaps.append((previous, s))
        previous = max(previous, e)
    unreferenced, kernel = set(), set()
    for a, b in gaps:
        at = a
        while at < b:
            ending = preceded_by(word, at, lo) if word(at) else None
            if ending == 'eret' or (ending == 'transfer' and runs_into_eret(at, b)):
                # Kernel-mode code runs on to the gap's end (handlers and their return paths).
                kernel.add(at); break
            end = routine_end(word, at, b) if ending == 'transfer' else None
            if end is None:
                at += 4; continue
            unreferenced.add(at); at = end
    built = {v for v in built_addresses(rows, word)
             if lo <= v < hi and not v & 3 and word(v) and owner(v) is None and preceded_by(word, v, lo) == 'transfer' and v not in kernel}
    return sorted(built), sorted(unreferenced - built), sorted(kernel)


def without_label(rows, word, code_range, data_ranges, labels):
    """Addresses inside a function, other than its start, that mapped code builds or data holds and that
    have no label (`labels`: interior entries and local switch targets of the generated code). Each is
    given to the generator as an entry hint, so that it becomes a label of its function."""
    lo, hi = code_range
    starts = sorted(s for s, e in rows); ends = dict(rows)
    def owner(address):
        at = bisect.bisect_right(starts, address) - 1
        return starts[at] if at >= 0 and address < ends[starts[at]] else None
    values = built_addresses(rows, word)
    for a, b in data_ranges:
        values |= {word(at) for at in range(a, b, 4)} - {None}
    return sorted(v for v in values if lo <= v < hi and not v & 3 and word(v) is not None
                  and owner(v) not in (None, v) and v not in labels)


def generated_labels(folder):
    """Interior entry labels and local switch targets of the generated sources in `folder`."""
    labels = set()
    for path in folder.glob('FUN_*.cpp'):
        text = path.read_text(encoding='utf-8')
        for block in re.findall(r'switch \((?:ctx->pc|jumpTarget)\) \{(.*?)default: break;', text, re.S):
            labels |= {int(a, 16) for a in re.findall(r'case 0x([0-9a-fA-F]+)u:', block)}
    return labels


def program_view(name):
    """(rows, word, code range, data ranges, folder) of the main executable ('main') or an overlay."""
    folder = FOLDER if name == 'main' else FOLDER / name
    rows = sorted((int(r['Start'], 16), int(r['End'], 16)) for r in csv.DictReader(open(folder / 'candidates.csv', encoding='utf-8')))
    if name == 'main':
        elf = (ROOT / 'rom/SLUS_214.41').read_bytes()
        offset, count = struct.unpack_from('<I', elf, 0x1C)[0], struct.unpack_from('<H', elf, 0x2C)[0]
        loads = [struct.unpack_from('<6I', elf, offset + n * 32) for n in range(count)]
        loads = [(va, off, size) for kind, off, va, pa, size, memory in loads if kind == 1]
        def word(address):
            for va, off, size in loads:
                if va <= address <= va + size - 4:
                    return struct.unpack_from('<I', elf, off + address - va)[0]
            return None
        code = (rows[0][0], rows[-1][1])
        return rows, word, code, [(code[1], max(va + size for va, off, size in loads))], folder
    image, _ = overlay_programs.load(name)
    base = overlay_programs.PROGRAMS[name]['base']
    def word(address):
        return struct.unpack_from('<I', image, address - base)[0] if base <= address <= base + len(image) - 4 else None
    code = (rows[0][0], rows[-1][1])
    return rows, word, code, [(code[1], base + len(image))], folder


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--write', action='store_true', help='Add what is found to each program\'s entries.json.')
    parser.add_argument('--add', nargs='+', metavar='ADDRESS', help='Add these addresses (hexadecimal) to the entries of the program they lie in, with or without --write.')
    parser.add_argument('--check', action='store_true', help='Exit with status 1 when anything is not covered (the gate).')
    args = parser.parse_args()
    total = 0
    extra = [int(a, 16) for a in args.add or []]
    for name in ['main', *sorted(overlay_programs.PROGRAMS)]:
        rows, word, code, data_ranges, folder = program_view(name)
        direct, inside, pointers = find_gaps(rows, word, code, data_ranges)
        built, unreferenced, kernel = find_code(rows, word, code)
        mine = [a for a in extra if (overlay_programs.program_for(a) or 'main') == name]
        found = sorted(set(direct) | set(inside) | set(pointers) | set(built) | set(unreferenced))
        # Labels exist only after generation: hints are checked against the latest generated sources.
        output = ROOT / 'game/generated' / ('bt2' if name == 'main' else name)
        hints_path = folder / 'entry-hints.json'
        hints = set(json.loads(hints_path.read_text(encoding='utf-8'))) if hints_path.exists() else set()
        labels = generated_labels(output) if output.exists() else set()
        unlabelled = without_label(rows, word, code, data_ranges, labels) if output.exists() else []
        # Added addresses inside a function (where the interpreter ran, or a run stopped) are hints as
        # well: if the map builder cannot split the function there, the generator makes them labels.
        starts = sorted(s for s, e in rows); ends = dict(rows)
        def within(address):
            at = bisect.bisect_right(starts, address) - 1
            return at >= 0 and starts[at] != address and address < ends[starts[at]]
        unlabelled = sorted(set(unlabelled) | {a for a in mine if within(a) and not a & 3 and a not in labels})
        new_hints, not_taken = sorted(set(unlabelled) - hints), sorted(set(unlabelled) & hints)
        total += len(found) + len(new_hints) + len(not_taken)
        print(f'{name}: {len(rows)} functions; not covered: {len(direct)} call or jump targets in unmapped code, '
              f'{len(inside)} inside another function, {len(pointers)} pointed at from data, {len(built)} built in code, '
              f'{len(unreferenced)} code no one names; inside a function without a label: {len(new_hints)} new, '
              f'{len(not_taken)} given as hints and not labelled yet; kernel-mode code, not run: {len(kernel)}')
        for label, addresses in (('targets', direct), ('inside', inside), ('pointers', pointers), ('built', built),
                                 ('unreferenced', unreferenced), ('new hints', new_hints), ('hints not labelled', not_taken), ('kernel-mode', kernel)):
            if addresses:
                print(f'  {label}: ' + ' '.join(f'{a:08X}' for a in addresses))
        if args.write or mine:
            path = folder / 'entries.json'
            entries = set(json.loads(path.read_text(encoding='utf-8'))) if path.exists() else set()
            before = len(entries)
            entries |= set(mine) | (set(found) if args.write else set())
            path.write_text(json.dumps(sorted(entries)) + '\n', encoding='utf-8')
            print(f'  {path.name}: {before} -> {len(entries)} entries')
        if (args.write or mine) and new_hints:
            hints_path.write_text(json.dumps(sorted(hints | set(new_hints))) + '\n', encoding='utf-8')
            print(f'  {hints_path.name}: {len(hints)} -> {len(hints) + len(new_hints)} hints')
    print(f'{total} addresses not covered in all.' + ('' if args.write or not total else ' Run with --write, then regenerate, to map them.'))
    return 1 if args.check and total else 0


if __name__ == '__main__':
    sys.exit(main())
