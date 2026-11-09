"""Names the places of a sample profile (BT2_SAMPLE_PROFILE) with the linker's map file.

Usage: profile_report.py PROFILE MAP [--top N] [--module NAME]

The profile holds one line per place: module file name, offset in the module (hex), samples and,
for a place in another module, the offset of the program's code that called into it (0: unknown). The
map file (linked with /MAP) lists each function's address; an offset belongs to the function with
the highest address not above it. Places in other modules are summed per module and caller.
"""
import argparse, bisect, collections, re
from pathlib import Path


def read_map(path):
    """Returns (preferred load address, sorted [(address, name)]) of a MSVC map file."""
    base = None; symbols = []
    for line in Path(path).read_text(errors='replace').splitlines():
        if base is None:
            m = re.search(r'Preferred load address is ([0-9a-fA-F]+)', line)
            if m: base = int(m.group(1), 16)
            continue
        # " 0001:00000000       name                       0000000140001000 f   object"
        m = re.match(r'\s+[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s', line)
        if m and int(m.group(2), 16) >= base: symbols.append((int(m.group(2), 16) - base, m.group(1)))
    if base is None: raise ValueError('Not a linker map file: no preferred load address')
    symbols.sort()
    return symbols


def report(profile, symbols, module):
    """Returns (total samples, Counter by function or module)."""
    lines = Path(profile).read_text().splitlines()
    total = int(lines[0].split()[1]); counts = collections.Counter(); addresses = [a for a, _ in symbols]
    def named(offset):
        at = bisect.bisect_right(addresses, offset) - 1
        return symbols[at][1] if at >= 0 else '[before the first symbol]'
    for line in lines[1:]:
        name, offset, samples, caller = line.rsplit(' ', 3)
        if name.lower() != module.lower():
            counts['[' + name + ']' + (' <- ' + named(int(caller, 16)) if int(caller, 16) else '')] += int(samples); continue
        counts[named(int(offset, 16))] += int(samples)
    return total, counts


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('profile'); p.add_argument('map'); p.add_argument('--top', type=int, default=40)
    p.add_argument('--module', default='bt2-entry-probe.exe')
    a = p.parse_args()
    total, counts = report(a.profile, read_map(a.map), a.module)
    print(f'{total} samples')
    for name, samples in counts.most_common(a.top):
        print(f'{samples / total * 100:6.2f}%  {samples:7d}  {name[:110]}')


if __name__ == '__main__':
    main()
