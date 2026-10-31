#!/usr/bin/env python3
"""Import a supported BT3 or BT4 ISO into an empty native game-data folder.

Only the supplied disc is read. Regional BT3 resources are assigned the native
engine's logical file numbers; no USA disc, executable patch or emulator is needed.
Requires pycdlib. Imported game assets must not be redistributed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct

import pycdlib

PROFILES = {
    'SLUS_216.78': ('us', (1, 3399, 65201),
        '811188ba9b416500d921cd4d9514df0cbf42f3a41a99cf5aac5a3da37171bf99',
        '30f61f9c78c3859e5dd4fcfd8df6753b0f2d2c64beb924b92da5b2eaaa5e09cc'),
    'SLES_549.45': ('eu', (2, 3645, 65201),
        '98ff35e7962da9533ae7aa0b337ee316bc24527678a897c0e946db424153f228',
        '81d029a54c1cf8ceb1232db698594a39dc42f8c3d7533d43c849b51bf262523b'),
    'SLPS_258.15': ('jp', (1, 3317, 63773),
        'bde07b4d5986a7fc9af40eb18381cc0ae1db33ccf07f70ac4bc955de68453011',
        'd857f8ae629d4a082224ed7d561c7b878e47185781799e5e7198ed1ba983d36d'),
}
COUNTS = (1, 3399, 65201)


def source_index(region, volume, index):
    """Canonical native ID -> index on the input disc (zero based).

    JP has one menu/announcer voice language: both voice selectors alias that
    language. It also has one HUD/story text bank instead of the USA disc's two.
    EU's extra language banks are retained separately for the language selector.
    """
    if not 0 <= volume < 3 or not 0 <= index < COUNTS[volume]:
        raise ValueError('Invalid logical resource')
    if region == 'eu' and volume == 1:
        return index if index < 7 else index + (3 if index < 510 else 96 if index < 658 else 246)
    if region == 'jp':
        if volume == 1:
            if index <= 5:
                return index
            if index == 6:
                return 5
            if index < 479:
                return index - 1
            if index < 608:
                return index - 32
            if index < 610:
                return 576  # empty story slots, identical on both discs
            if index < 658:
                return index - 82  # Japanese story text replaces second language
            return index - 82
        if volume == 2:
            if index < 31372:
                return index
            if index < 32744:
                return index - 1372  # second menu voice bank -> sole JP bank
            if index < 65060:
                return index - 1372
            if index < 65116:
                return index - 1372 - 56  # second announcer -> sole JP announcer
            return index - 1428
    if region not in ('us', 'eu', 'jp'):
        raise ValueError('Unknown disc region')
    return index


def read(iso, member):
    with iso.open_file_from_iso(iso_path=member) as stream:
        return stream.read()


def archive_table(iso, member, expected):
    size = iso.get_record(iso_path=member).data_length
    with iso.open_file_from_iso(iso_path=member) as stream:
        head = stream.read(8)
        if len(head)!=8 or head[:4] != b'AFS\0' or struct.unpack_from('<I', head, 4)[0] != expected:
            raise ValueError(f'Unexpected archive layout: {member}')
        raw = stream.read(expected * 8)
    if len(raw) != expected * 8:
        raise ValueError(f'Truncated archive: {member}')
    table = list(struct.iter_unpack('<II', raw))
    if any(offset < 8 + expected * 8 or offset + length > size for offset, length in table):
        raise ValueError(f'Invalid resource bounds: {member}')
    return table


def alias(source, destination):
    try:
        os.link(source, destination)
    except OSError:
        shutil.copyfile(source, destination)


def extract(iso_path, output):
    output = Path(output)
    if output.exists() and any(output.iterdir()):
        raise ValueError('Choose an empty destination; existing game data will not be replaced.')
    iso = pycdlib.PyCdlib()
    iso.open(str(iso_path))
    opened = True
    try:
        members = {r.file_identifier().decode().split(';')[0] for r in iso.list_children(iso_path='/')}
        if 'SLUS_219.78' in members:
            iso.close()
            opened = False
            from extract_bt4 import extract as extract_bt4
            return extract_bt4(iso_path, output)
        serial = next((s for s in PROFILES if s in members), None)
        if serial is None:
            raise ValueError('Supported discs: BT3 USA, Europe/Australia, Japan; BT4 B14 REV2 English or Spanish.')
        region, counts, exe_hash, menu_hash = PROFILES[serial]
        exe = read(iso, f'/{serial};1')
        menu = read(iso, '/BIN/DBZP.BIN;1')
        if hashlib.sha256(exe).hexdigest() != exe_hash or hashlib.sha256(menu).hexdigest() != menu_hash:
            raise ValueError('This revision has a different executable or menu layout; it needs its own disc profile.')
        tables = [archive_table(iso, f'/DATA/PZS3{region.upper()}{v}.AFS;1', n) for v, n in enumerate(counts)]
        output.mkdir(parents=True, exist_ok=True)
        for volume, table in enumerate(tables):
            folder = output / f'pzs3us{volume}'
            folder.mkdir()
            extracted = {}
            with iso.open_file_from_iso(iso_path=f'/DATA/PZS3{region.upper()}{volume}.AFS;1') as stream:
                for index in range(COUNTS[volume]):
                    source = source_index(region, volume, index)
                    target = folder / f'{index:05d}.bin'
                    if source in extracted:
                        alias(extracted[source], target)
                    else:
                        offset, length = table[source]
                        stream.seek(offset)
                        data = stream.read(length)
                        if len(data) != length:
                            raise ValueError(f'Truncated resource {volume}:{source}')
                        target.write_bytes(data)
                        extracted[source] = target
            print(f'Volume {volume}: {COUNTS[volume]} native resources', flush=True)
        if region == 'eu':
            from extract_eu import extract as extract_languages
            extract_languages(iso_path, output)
        # Runtime movie names remain canonical; their content comes from this disc.
        movies = output / 'disc' / 'DATA'
        movies.mkdir(parents=True)
        for ending in ('OP', 'ED'):
            for suffix in ('ADX', 'PSS'):
                member = f'/DATA/ZS3{region.upper()}{ending}.{suffix};1'
                with iso.open_file_from_iso(iso_path=member) as source, (movies / f'ZS3US{ending}.{suffix}').open('wb') as dest:
                    shutil.copyfileobj(source, dest)
        (output / 'native-disc.json').write_text(json.dumps(dict(schema=1, game='bt3', region=region,
            resource_layout='bt3-us-canonical-v1', executable=serial, executable_sha256=exe_hash,
            menu_sha256=menu_hash, volumes=COUNTS, source_volumes=counts), indent=2) + '\n')
        print(f'Imported BT3 {region.upper()}: {output}', flush=True)
    finally:
        if opened:
            iso.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        extract(args.iso, args.output)
    except (ValueError, OSError) as error:
        parser.exit(1, f'Import stopped: {error}\n')
