"""What the game's code can ask of the runtime, read from the code (WHOLE_GAME_COVERAGE_PLAN.md, step 4).

For the mapped code of every program it lists
- the system calls: every SYSCALL site with the number placed in v1 before it;
- the hardware registers: every address in the EE register window (0x10000000-0x1000FFFF), the VU
  memories (0x11000000-0x1100FFFF) and the GS privileged registers (0x12000000-0x12001FFF), uncached
  aliases folded, that mapped code loads from or stores to through a base register it built with LUI
  (and ADDIU or ORI), or builds as a pointer;
- the VU1 microcode of the executable's .vutext section, against the instructions the VU1 model
  implements (tools/vu1/disassemble.py shows anything else as a word).
and compares the first two with the runtime's sources (runtime/): numbers and addresses it mentions at
all, and those it mentions only in conditions on the guest pc, a startup phase, a call count or the
stack offset (handlers that accept a request from one place only). The result is the work list for
step 5. Requests built at run time (VIF codes, GS packets, service commands) are not covered; the
coverage files of step 6 report those.

    python tools/ps2recomp/runtime_census.py [--json PATH]

The report names counts and public register and system call names; the per-site lists stay in the
ignored JSON (default work/bt2-generation/runtime-census.json).
"""
import argparse, collections, json, re, struct, sys
from pathlib import Path
import overlay_programs
from map_gaps import program_view

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/vu1'))

# Public EE kernel system call names (the PS2SDK kernel API), by number; a negative number calls the
# interrupt-context form of the same table entry.
SYSCALLS = {
    0x02: 'GsSetCrt', 0x04: 'Exit', 0x06: 'LoadExecPS2', 0x07: 'ExecPS2', 0x0D: 'SetVTLBRefillHandler', 0x0E: 'SetVCommonHandler',
    0x10: 'AddIntcHandler', 0x11: 'RemoveIntcHandler', 0x12: 'AddDmacHandler', 0x13: 'RemoveDmacHandler', 0x14: '_EnableIntc',
    0x15: '_DisableIntc', 0x16: '_EnableDmac', 0x17: '_DisableDmac', 0x18: 'SetAlarm', 0x19: 'ReleaseAlarm', 0x1A: '_iEnableIntc',
    0x1B: '_iDisableIntc', 0x1C: '_iEnableDmac', 0x1D: '_iDisableDmac', 0x1E: 'iSetAlarm', 0x1F: 'iReleaseAlarm', 0x20: 'CreateThread',
    0x21: 'DeleteThread', 0x22: 'StartThread', 0x23: 'ExitThread', 0x24: 'ExitDeleteThread', 0x25: 'TerminateThread',
    0x26: 'iTerminateThread', 0x27: 'DisableDispatchThread', 0x28: 'EnableDispatchThread', 0x29: 'ChangeThreadPriority',
    0x2A: 'iChangeThreadPriority', 0x2B: 'RotateThreadReadyQueue', 0x2C: 'iRotateThreadReadyQueue', 0x2D: 'ReleaseWaitThread',
    0x2E: 'iReleaseWaitThread', 0x2F: 'GetThreadId', 0x30: 'ReferThreadStatus', 0x31: 'iReferThreadStatus', 0x32: 'SleepThread',
    0x33: 'WakeupThread', 0x34: 'iWakeupThread', 0x35: 'CancelWakeupThread', 0x36: 'iCancelWakeupThread', 0x37: 'SuspendThread',
    0x38: 'iSuspendThread', 0x39: 'ResumeThread', 0x3A: 'iResumeThread', 0x3B: 'JoinThread', 0x3C: 'SetupThread', 0x3D: 'SetupHeap',
    0x3E: 'EndOfHeap', 0x40: 'CreateSema', 0x41: 'DeleteSema', 0x42: 'SignalSema', 0x43: 'iSignalSema', 0x44: 'WaitSema',
    0x45: 'PollSema', 0x46: 'iPollSema', 0x47: 'ReferSemaStatus', 0x48: 'iReferSemaStatus', 0x4A: 'SetOsdConfigParam',
    0x4B: 'GetOsdConfigParam', 0x4C: 'GetGsHParam', 0x4D: 'GetGsVParam', 0x4E: 'SetGsHParam', 0x4F: 'SetGsVParam',
    0x63: 'GetCop0', 0x64: 'FlushCache', 0x68: 'iFlushCache', 0x6B: 'SetSyscall (kernel call)', 0x70: 'GsGetIMR', 0x71: 'GsPutIMR',
    0x73: 'SetVSyncFlag', 0x74: 'SetSyscall', 0x76: 'SifDmaStat', 0x77: 'SifSetDma', 0x78: 'SifSetDChain', 0x79: 'SifSetReg',
    0x7A: 'SifGetReg', 0x7C: 'Deci2Call', 0x7E: 'MachineType', 0x7F: 'GetMemorySize',
}

# Register blocks of the EE register window, the VU memories and the GS (public hardware layout).
BLOCKS = [
    (0x10000000, 0x10000040, 'Timer 0'), (0x10000800, 0x10000840, 'Timer 1'), (0x10001000, 0x10001040, 'Timer 2'),
    (0x10001800, 0x10001840, 'Timer 3'), (0x10002000, 0x10002040, 'IPU'), (0x10003000, 0x10003100, 'GIF'),
    (0x10003800, 0x10003A00, 'VIF0'), (0x10003C00, 0x10003E00, 'VIF1'), (0x10004000, 0x10004010, 'VIF0 FIFO'),
    (0x10005000, 0x10005010, 'VIF1 FIFO'), (0x10006000, 0x10006010, 'GIF FIFO'), (0x10007000, 0x10007020, 'IPU FIFO'),
    (0x10008000, 0x10008100, 'DMA 0 VIF0'), (0x10009000, 0x10009100, 'DMA 1 VIF1'), (0x1000A000, 0x1000A100, 'DMA 2 GIF'),
    (0x1000B000, 0x1000B100, 'DMA 3 fromIPU'), (0x1000B400, 0x1000B500, 'DMA 4 toIPU'), (0x1000C000, 0x1000C100, 'DMA 5 SIF0'),
    (0x1000C400, 0x1000C500, 'DMA 6 SIF1'), (0x1000C800, 0x1000C900, 'DMA 7 SIF2'), (0x1000D000, 0x1000D100, 'DMA 8 fromSPR'),
    (0x1000D400, 0x1000D500, 'DMA 9 toSPR'), (0x1000E000, 0x1000E100, 'DMAC'), (0x1000F000, 0x1000F020, 'INTC'),
    (0x1000F100, 0x1000F200, 'SIO'), (0x1000F200, 0x1000F300, 'SIF registers'), (0x1000F400, 0x1000F600, 'EE misc'),
    (0x1000F500, 0x1000F600, 'DMAC enable'), (0x11000000, 0x11001000, 'VU0 micro memory'), (0x11004000, 0x11005000, 'VU0 data memory'),
    (0x11008000, 0x1100C000, 'VU1 micro memory'), (0x1100C000, 0x11010000, 'VU1 data memory'),
    (0x12000000, 0x12001000, 'GS privileged (display)'), (0x12001000, 0x12002000, 'GS privileged (CSR, IMR, BUSDIR, SIGLBLID)'),
]
HARDWARE = ((0x10000000, 0x10010000), (0x11000000, 0x11010000), (0x12000000, 0x12002000))
LOADS = {32: ('lb', 1), 33: ('lh', 2), 34: ('lwl', 4), 35: ('lw', 4), 36: ('lbu', 1), 37: ('lhu', 2), 38: ('lwr', 4), 39: ('lwu', 4),
         55: ('ld', 8), 30: ('lq', 16), 26: ('ldl', 8), 27: ('ldr', 8), 49: ('lwc1', 4), 54: ('lqc2', 16)}
STORES = {40: ('sb', 1), 41: ('sh', 2), 42: ('swl', 4), 43: ('sw', 4), 46: ('swr', 4), 63: ('sd', 8), 31: ('sq', 16),
          44: ('sdl', 8), 45: ('sdr', 8), 57: ('swc1', 4), 62: ('sqc2', 16)}
GATED = re.compile(r'(?:target|source|resume) ?== ?0x[0-9A-Fa-f]{5}|pc ?== ?0x|->pc ?== ?0x|_phase ?== ?\d|\.phase ?== ?\d|_calls ?== ?\d|== ?initial_sp|initial_sp ?- ?\d+ ?==|_count ?== ?\d')


def physical(address):
    """Uncached and cached aliases of main memory and registers folded to the physical address."""
    return address & 0x1FFFFFFF if 0x80000000 <= address < 0xC0000000 else address


def hardware(address):
    return any(a <= address < b for a, b in HARDWARE)


def block(address):
    for a, b, name in BLOCKS:
        if a <= address < b:
            return name
    return 'other register'


def syscall_number(word, start, at):
    """The number placed in v1 before the SYSCALL at `at` (ADDIU/ORI v1, zero, n within the eight
    preceding instructions of the function, no other write of v1 between), or None."""
    for back in range(at - 4, max(start, at - 32) - 4, -4):
        ins = word(back)
        if ins is None:
            return None
        op, rs, rt = ins >> 26, (ins >> 21) & 31, (ins >> 16) & 31
        if op in (9, 13) and rt == 3:
            if rs != 0:
                return None
            imm = ins & 0xFFFF
            return imm - 0x10000 if op == 9 and imm & 0x8000 else imm
        writes = (rt if op in (8, 10, 11, 12, 14, 15, 24, 25) or 32 <= op <= 39 else (ins >> 11) & 31 if op == 0 else None)
        if writes == 3:
            return None
        if op in (2, 3) or (op == 0 and (ins & 63) in (8, 9)):
            return None
    return None


def scan_program(rows, word):
    """(syscalls, accesses): {number: [sites]} and {physical address: {kind: [sites]}} of one program."""
    syscalls, accesses = collections.defaultdict(list), collections.defaultdict(lambda: collections.defaultdict(list))
    sign = lambda v: v - 0x10000 if v & 0x8000 else v
    for start, end in rows:
        values = {}
        for at in range(start, end, 4):
            ins = word(at)
            if ins is None:
                continue
            op, rs, rt = ins >> 26, (ins >> 21) & 31, (ins >> 16) & 31
            if ins & 0xFC00003F == 0x0C:
                syscalls[syscall_number(word, start, at)].append(at)
            if op in LOADS or op in STORES:
                if rs in values:
                    address = physical((values[rs] + sign(ins & 0xFFFF)) & 0xFFFFFFFF)
                    if hardware(address):
                        name, width = LOADS.get(op) or STORES.get(op)
                        accesses[address][('store ' if op in STORES else 'load ') + name].append(at)
                if op in LOADS and op not in (49, 54):
                    values.pop(rt, None)
                continue
            if op == 15:
                values[rt] = (ins & 0xFFFF) << 16
                continue
            if op in (9, 13) and rs in values:
                value = (values[rs] + sign(ins & 0xFFFF)) & 0xFFFFFFFF if op == 9 else values[rs] | (ins & 0xFFFF)
                if hardware(physical(value)):
                    accesses[physical(value)]['pointer built'].append(at)
                values[rt] = value
                continue
            target = rt if 8 <= op <= 14 or 24 <= op <= 25 else (ins >> 11) & 31 if op == 0 else None
            if target is not None:
                values.pop(target, None)
            if op in (2, 3) or (op == 0 and (ins & 63) in (8, 9)):
                values.clear() # A call or jump: the registers are not known after it.
    return syscalls, accesses


def references(programs):
    """{address: count} of how often the code names each address: JAL/J targets, data words, and
    addresses built with LUI and ADDIU or ORI."""
    from map_gaps import built_addresses
    counts = collections.Counter()
    for rows, word, code, data_ranges in programs:
        for start, end in rows:
            for at in range(start, end, 4):
                ins = word(at)
                if ins is not None and ins >> 26 in (2, 3):
                    counts[((at + 4) & 0xF0000000) | ((ins & 0x03FFFFFF) << 2)] += 1
        for a, b in data_ranges:
            for at in range(a, b, 4):
                value = word(at)
                if value is not None and code[0] <= value < code[1]:
                    counts[value] += 1
        counts.update(built_addresses(rows, word))
    return counts


def installed_syscalls(programs, setsyscall_stubs):
    """Numbers the code installs its own handlers for: the constant in a0 at each call of a SetSyscall
    stub (ADDIU/ORI a0, zero, n in the delay slot or within the eight instructions before the call)."""
    installed = set()
    for rows, word, _, _ in programs:
        for start, end in rows:
            for at in range(start, end, 4):
                ins = word(at)
                if ins is None or ins >> 26 != 3 or ((at + 4) & 0xF0000000) | ((ins & 0x03FFFFFF) << 2) not in setsyscall_stubs:
                    continue
                for back in [at + 4] + list(range(at - 4, max(start, at - 32) - 4, -4)):
                    value = word(back)
                    if value is not None and value >> 26 in (9, 13) and (value >> 16) & 31 == 4:
                        if (value >> 21) & 31 == 0:
                            installed.add(value & 0xFFFF)
                        break
    return installed


def owner_of(rows, address):
    for start, end in rows:
        if start <= address < end:
            return start
    return None


def runtime_mentions(folder=ROOT / 'runtime'):
    """(numbers, addresses, ranges): the system call numbers and hardware addresses the runtime's
    sources name, each with whether every mention is gated (on the guest pc, a startup phase, a call
    count or the stack offset), and the address ranges it checks (address>=A && address<B)."""
    numbers, addresses, ranges = {}, {}, []
    for path in sorted(folder.rglob('*.cpp')) + sorted(folder.rglob('*.h')):
        if 'tests' in path.parts:
            continue
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
        for n, line in enumerate(lines):
            # The condition a mention is part of: this line and the following ones up to its block or statement.
            chunk = line
            for more in lines[n + 1:n + 4]:
                if '{' in chunk or ';' in chunk:
                    break
                chunk += more
            gated = bool(GATED.search(chunk))
            for value in re.findall(r'(?:selector|r\[3\]\.low\)?) ?[!=]= ?(0x[0-9A-Fa-f]+|\d+)', line):
                number = int(value, 0)
                number = number - (1 << 32) if number & 0x80000000 else number
                numbers[number] = numbers.get(number, True) and gated
            for value in re.findall(r'0x(1[012]0[0-9A-Fa-f]{5})\b', line):
                address = int(value, 16)
                if hardware(address):
                    addresses[address] = addresses.get(address, True) and gated
            for a, b in re.findall(r'address ?>= ?0x([0-9A-Fa-f]+)\s*&&\s*address ?< ?0x([0-9A-Fa-f]+)', line):
                ranges.append((int(a, 16), int(b, 16), gated))
    return numbers, addresses, ranges


def gated_conditions(folder=ROOT / 'runtime'):
    """{file: [line numbers]} of the runtime's conditions on the guest pc, a startup phase, a call count
    or the stack offset: the special cases step 5 replaces with general handling."""
    found = {}
    for path in sorted(folder.rglob('*.cpp')) + sorted(folder.rglob('*.h')):
        if 'tests' in path.parts or 'probe' in path.parts:
            continue
        lines = [n + 1 for n, line in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines())
                 if GATED.search(line) and not line.lstrip().startswith('//')]
        if lines:
            found[str(path.relative_to(ROOT)).replace('\\', '/')] = lines
    return found


def vu1_microcode():
    """(pairs, unknown): the instruction pairs of .vutext and those with an instruction the VU1 model
    does not implement, as [(index, 'upper'|'lower', word)]."""
    from disassemble import upper, lower
    rows, word, _, _, _ = program_view('main')
    elf = (ROOT / 'rom/SLUS_214.41').read_bytes()
    shoff, shentsize, shnum, shstrndx = struct.unpack_from('<I', elf, 0x20)[0], *struct.unpack_from('<HHH', elf, 0x2E)
    sections = [struct.unpack_from('<10I', elf, shoff + n * shentsize) for n in range(shnum)]
    names = sections[shstrndx]
    name = lambda s: elf[names[4] + s[0]:elf.index(b'\0', names[4] + s[0])].decode()
    vutext = next(s for s in sections if name(s) == '.vutext')
    data = elf[vutext[4]:vutext[4] + vutext[5]]
    unknown, pairs = [], len(data) // 8
    for index in range(pairs):
        low, high = struct.unpack_from('<II', data, index * 8)
        if upper(high).startswith('.word'):
            unknown.append((index, 'upper', high))
        if not high >> 31 and lower(low, index).startswith('.word'): # With the I bit the lower word is a constant.
            unknown.append((index, 'lower', low))
    return pairs, unknown


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--json', type=Path, default=ROOT / 'work/bt2-generation/runtime-census.json')
    parser.add_argument('--report', type=Path, default=ROOT / 'work/bt2-generation/gate-battle.json',
                        help='A probe report whose installed_syscalls (numbers the game installed handlers for) count as answered (default: the battle run of the gate).')
    args = parser.parse_args()
    syscalls, accesses = collections.defaultdict(list), collections.defaultdict(lambda: collections.defaultdict(list))
    views, owners = [], {}
    for program in ['main', *sorted(overlay_programs.PROGRAMS)]:
        rows, word, code, data_ranges, _ = program_view(program)
        views.append((rows, word, code, data_ranges))
        s, a = scan_program(rows, word)
        for sites in s.values():
            for site in sites:
                owners[site] = owner_of(rows, site)
        for number, sites in s.items():
            syscalls[number] += sites
        for address, kinds in a.items():
            for kind, sites in kinds.items():
                accesses[address][kind] += sites
    numbers, addresses, ranges = runtime_mentions()
    named = references(views)
    # How often the code names the functions holding each system call: a library stub nobody calls is not a request.
    callers = {n: sum(named[owners[s]] for s in sites if owners.get(s) is not None) for n, sites in syscalls.items()}
    installed = installed_syscalls(views, {owners[s] for s in syscalls.get(0x74, []) if owners.get(s) is not None})
    # The game installs most of its own numbers from a table the code copies: a recorded run names them.
    if args.report.exists():
        installed |= set(json.loads(args.report.read_text(encoding='utf-8')).get('installed_syscalls', []))
    def address_status(address):
        if address in addresses:
            return 'gated' if addresses[address] else 'handled'
        inside = [gated for a, b, gated in ranges if a <= address < b]
        return ('gated' if all(inside) else 'handled') if inside else 'not named'
    def number_status(number):
        if number is None:
            return 'unknown number'
        found = [numbers[n] for n in (number, -number) if n in numbers]
        if found:
            return 'gated' if all(found) else 'handled'
        # The runtime runs the handlers the game installs with SetSyscall for any number.
        return 'installed by the game' if abs(number) in installed else 'not named'

    print('System calls the code reaches (number: sites, references to their functions; the runtime: handled / gated only / not named)')
    by_status = collections.Counter()
    for number in sorted(syscalls, key=lambda n: (n is None, abs(n) if n is not None else 0, n or 0)):
        status = number_status(number)
        if not callers[number]:
            by_status['unreferenced library stubs'] += 1
            continue
        by_status[status] += 1
        label = 'unknown' if number is None else f'{number:#x}' if number >= 0 else f'-{-number:#x}'
        print(f'  {label:>7} {SYSCALLS.get(abs(number), "?") if number is not None else "":28} {len(syscalls[number]):4} sites {callers[number]:5} references  {status}')
    print('  ' + ', '.join(f'{n} {s}' for s, n in sorted(by_status.items())))
    print('\nHardware registers (address: block, accesses; the runtime: handled / gated only / not named)')
    by_block = collections.defaultdict(list)
    for address in sorted(accesses):
        by_block[block(address)].append(address)
    totals = collections.Counter()
    for name, members in sorted(by_block.items(), key=lambda item: item[1][0]):
        statuses = collections.Counter(address_status(a) for a in members)
        totals.update(statuses)
        print(f'  {name:44} {len(members):3} addresses  ' + ', '.join(f'{n} {s}' for s, n in sorted(statuses.items())))
    print('  ' + ', '.join(f'{n} {s}' for s, n in sorted(totals.items())))
    gated = gated_conditions()
    print(f'\nRuntime conditions on the guest pc, a startup phase, a call count or the stack offset: '
          f'{sum(len(v) for v in gated.values())} lines')
    for path, lines in sorted(gated.items(), key=lambda item: -len(item[1])):
        print(f'  {path:52} {len(lines):4}')
    pairs, unknown = vu1_microcode()
    print(f'\nVU1 microcode (.vutext): {pairs} instruction pairs, {len(unknown)} instructions the VU1 model does not implement'
          + (' (' + ', '.join(f'{kind} 0x{word:08X}' for _, kind, word in unknown[:12]) + ')' if unknown else ''))
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps({
        'syscalls': {('unknown' if n is None else str(n)): {'sites': [f'{s:08X}' for s in sites], 'runtime': number_status(n), 'references': callers[n],
                                                             'name': SYSCALLS.get(abs(n), '') if n is not None else ''}
                     for n, sites in syscalls.items()},
        'registers': {f'{a:08X}': {'block': block(a), 'runtime': address_status(a), 'accesses': {k: [f'{s:08X}' for s in v] for k, v in kinds.items()}}
                      for a, kinds in sorted(accesses.items())},
        'gated_conditions': gated,
        'vu1': {'pairs': pairs, 'unknown': [{'pair': i, 'kind': k, 'word': f'{w:08X}'} for i, k, w in unknown]}}, indent=1) + '\n', encoding='utf-8')
    print(f'\nPer-site lists: {args.json} (local and ignored).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
