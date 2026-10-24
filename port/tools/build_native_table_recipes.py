"""Build offset-only startup recipes using privately owned reference discs.

Pass --usa-data, one or more --manifest paths produced by strip_data.py, and
--disc ID SERIAL ISO for each supported disc. Generated copies are verified.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import pycdlib


def flat_elf(blob):
    shoff = struct.unpack_from('<I', blob, 32)[0]
    entsize, count = struct.unpack_from('<HH', blob, 46)
    out = bytearray(0x1ff180)
    for i in range(count):
        _, kind, flags, address, offset, length = struct.unpack_from('<6I', blob, shoff + i * entsize)
        if flags & 2 and kind != 8 and 0x100000 <= address and address + length <= 0x2ff180:
            out[address - 0x100000:address - 0x100000 + length] = blob[offset:offset + length]
    return out


def recipe(data, blobs):
    ops, position = [], 0
    while position < len(data):
        rest = data[position:]
        best = None
        for source, blob in enumerate(blobs):
            low, high, found = 4, len(rest), -1
            while low <= high:
                length = (low + high) // 2
                at = blob.find(rest[:length])
                if at >= 0:
                    found = at
                    low = length + 1
                else:
                    high = length - 1
            if found >= 0 and (best is None or high > best[2]):
                best = [source, blob.find(rest[:high]), high]
        if best is None:
            # Differences are canonical addresses, archive names and numeric
            # constants. Four bytes at most; review these when adding a disc.
            length = min(4, len(rest))
            best = [-1, rest[:length].hex(), length]
        ops.append(best)
        position += best[2]
    return ops


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--usa-data', type=Path, required=True)
    p.add_argument('--manifest', type=Path, action='append', required=True)
    p.add_argument('--disc', nargs=3, action='append', required=True, metavar=('ID', 'SERIAL', 'ISO'))
    p.add_argument('--output', type=Path, default=Path(__file__).with_name('native_table_recipes.json'))
    args = p.parse_args()
    sources = [flat_elf((args.usa_data / 'disc/SLUS_216.78').read_bytes()),
               (args.usa_data / 'disc/BIN/DBZP.BIN').read_bytes()]
    ranges = set()
    for path in args.manifest:
        ranges.update((w, o, n) for a, w, o, n in struct.iter_unpack('<4I', path.read_bytes()[16:]))
    result = dict(schema=1, sizes=list(map(len, sources)), profiles=[])
    for ident, serial, filename in args.disc:
        iso = pycdlib.PyCdlib()
        iso.open(filename)
        names = [f'/{serial};1', '/BIN/DBZP.BIN;1']
        if serial == 'SLUS_219.78':
            names.append('/BIN/DBZ4.BIN;1')
        blobs = []
        for name in names:
            with iso.open_file_from_iso(iso_path=name) as stream:
                blobs.append(stream.read())
        iso.close()
        mapped = []
        for which, offset, length in sorted(ranges):
            data = bytes(sources[which][offset:offset + length])
            mapped.append([which, offset, length, hashlib.sha256(data).hexdigest(), recipe(data, blobs)])
        fixes = sum(op[2] for r in mapped for op in r[4] if op[0] == -1)
        if fixes > 1024:
            raise ValueError(f'{ident}: too many compatibility fixups ({fixes})')
        result['profiles'].append(dict(id=ident, serial=serial, files=names,
            sha256=[hashlib.sha256(b).hexdigest() for b in blobs], ranges=mapped))
        print(f'{ident}: {len(mapped)} ranges, {fixes} compatibility bytes', flush=True)
    args.output.write_text(json.dumps(result, separators=(',', ':')) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
