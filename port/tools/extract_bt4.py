#!/usr/bin/env python3
"""Extract a player's BT4 B14 REV2 disc for the native port, separately from BT3.

BT4 volumes are sector streams, not AFS archives. Their index lives in DBZ4.BIN.
Requires pycdlib. Extracted data is private game data and must not be distributed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

import pycdlib


def extract_movies(iso, output):
    """Keep the disc's named opening/ending streams alongside its archives."""
    folder = Path(output) / 'disc' / 'DATA'
    folder.mkdir(parents=True, exist_ok=True)
    for stem in ('ZS4USOP', 'ZS4USED'):
        for suffix in ('PSS', 'ADX'):
            name = f'{stem}.{suffix}'
            target = folder / name
            member = f'/DATA/{name};1'
            size = iso.get_record(iso_path=member).data_length
            if target.exists() and target.stat().st_size == size:
                continue
            temporary = target.with_suffix(target.suffix + '.tmp')
            with iso.open_file_from_iso(iso_path=member) as source, temporary.open('wb') as dest:
                import shutil
                shutil.copyfileobj(source, dest)
            if temporary.stat().st_size != size:
                raise ValueError(f'Truncated movie: {name}')
            temporary.replace(target)


def write_catalog(output):
    """Read names from this disc; never use BT3's names for reassigned IDs."""
    from menu_font import section, unpack
    data = Path(output) / 'pzs4us1' / '00450.bin'
    screen = unpack(section(data.read_bytes(), 2))
    names, forms = section(screen, 30), section(screen, 31)
    if struct.unpack_from('<I', names)[0] != 250 or struct.unpack_from('<I', forms)[0] != 250:
        raise ValueError('Unsupported BT4 roster labels')

    def label(table, index):
        raw = section(table, index + 1)
        if not raw.startswith(b'\xff\xfe'):
            raise ValueError('Invalid character label')
        text = []
        for (code,) in struct.iter_unpack('<H', raw[2:len(raw) // 2 * 2]):
            if not (32 <= code <= 126 or 192 <= code <= 255):
                break
            text.append(chr(code))
        return ''.join(text).strip()

    lines = [f'{i}\t{label(names, i)}\t{label(forms, i)}\n' for i in range(250)]
    (Path(output) / 'native-roster.tsv').write_text(''.join(lines), encoding='utf-8')


def extract(iso_path, output):
    output = Path(output)
    marker = output / 'native-disc.json'
    if output.exists() and any(output.iterdir()):
        raise ValueError('This destination already contains a disc; choose an empty destination.')
    iso = pycdlib.PyCdlib()
    iso.open(str(iso_path))
    try:
        def read(name):
            with iso.open_file_from_iso(iso_path=name) as stream:
                return stream.read()
        exe = read('/SLUS_219.78;1')
        addon = read('/BIN/DBZ4.BIN;1')
        menu = read('/BIN/DBZP.BIN;1')
        if not exe.startswith(b'\x7fELF'):
            raise ValueError('Missing BT4 executable')
        # Match the complete set, never a mixture of programs from two revisions.
        known = {
            ('6c38c69f17e852ce9146b0576afbcd45ebe13208921c06e088b712c01dd84d3a',
             '80471637383e6c6a00a222a5808b5ee241e286b60bcce5b47cdddb8ebf4e5bd6',
             '0562c337d27adbb141030ddd616326118b181cee381a512351bc4421b8a35707'): 'en',
            ('224525c5639c00e01c2604c4685d307bc739c041000dcf8c69a4f884f465e8a4',
             '7527f44b4bf84f377cb5a70279d859c1d170af4b0fc979b61dfaf696a454695b',
             '6639aa78a1ab63a392ac2cbccd70350307d6bc5ecb57dc3d60a3671a2df022fd'): 'es',
        }
        language = known.get(tuple(hashlib.sha256(data).hexdigest() for data in (exe, addon, menu)))
        if language is None:
            raise ValueError('Unrecognized BT4 revision. Supported: B14 REV2 English and Spanish.')
        volumes = []
        for volume in range(4):
            matches = list(re.finditer(f'pzs4us{volume}.afs'.encode(), addon))
            if len(matches) != 1:
                raise ValueError('Unrecognized BT4 volume index')
            base = matches[0].start() - 16
            _, size, count, duplicate = struct.unpack_from('<4I', addon, base)
            if not 0 < count <= 100000 or count != duplicate or size != 282 + 2 * count:
                raise ValueError('Invalid BT4 volume index')
            lengths = struct.unpack_from(f'<{count}H', addon, base + 282)
            member = f'/DATA/PZS4US{volume}.AFS;1'
            if sum(lengths) * 2048 > iso.get_record(iso_path=member).data_length:
                raise ValueError('Truncated BT4 volume')
            volumes.append((member, lengths))
        for name, data in [('SLUS_219.78', exe), ('BIN/DBZ4.BIN', addon), ('BIN/DBZP.BIN', menu)]:
            target = output / 'disc' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        for volume, (member, lengths) in enumerate(volumes):
            folder = output / f'pzs4us{volume}'
            folder.mkdir(parents=True, exist_ok=True)
            with iso.open_file_from_iso(iso_path=member) as stream:
                for index, sectors in enumerate(lengths):
                    data = stream.read(sectors * 2048)
                    if len(data) != sectors * 2048:
                        raise ValueError(f'Truncated resource {volume}:{index}')
                    (folder / f'{index:05d}.bin').write_bytes(data)
            print(f'Volume {volume}: {len(lengths)} resources', flush=True)
        extract_movies(iso, output)
        write_catalog(output)
        marker.write_text(json.dumps(dict(schema=1, game='bt4', revision='b14-rev2', language=language,
            executable='SLUS_219.78', executable_sha256=hashlib.sha256(exe).hexdigest(),
            menu_sha256=hashlib.sha256(menu).hexdigest(), addon_sha256=hashlib.sha256(addon).hexdigest(),
            volumes=[len(v[1]) for v in volumes]), indent=2) + '\n')
    finally:
        iso.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    extract(args.iso, args.output)
