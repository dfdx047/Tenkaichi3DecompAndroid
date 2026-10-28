"""Reconstruct portable startup tables from the player's supported disc.

The recipes contain offsets and small address/filename compatibility fixups,
not game assets. Every reconstructed range is checked before it is written.
"""
import hashlib
import json
from pathlib import Path
import struct
import pycdlib

RECIPES = Path(__file__).with_name('native_table_recipes.json')


def identify(path):
    recipes = json.loads(RECIPES.read_text(encoding='utf-8'))
    iso = pycdlib.PyCdlib()
    iso.open(str(path))
    try:
        members = {r.file_identifier().decode().split(';')[0] for r in iso.list_children(iso_path='/')}
        for profile in recipes['profiles']:
            if profile['serial'] not in members:
                continue
            blobs = []
            for name in profile['files']:
                with iso.open_file_from_iso(iso_path=name) as stream:
                    blobs.append(stream.read())
            if [hashlib.sha256(b).hexdigest() for b in blobs] == profile['sha256']:
                return profile, blobs, recipes['sizes']
    finally:
        iso.close()
    raise ValueError('Unsupported disc revision. Use BT3 USA, Europe/Australia or Japan, or BT4 B14 REV2 English/Spanish.')


def reconstruct(profile, blobs, sizes):
    output = [bytearray(n) for n in sizes]
    for which, offset, length, digest, operations in profile['ranges']:
        data = bytearray()
        for source, start, count in operations:
            if source == -1:
                data.extend(bytes.fromhex(start))
            else:
                data.extend(blobs[source][start:start + count])
        if len(data) != length or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('The disc startup tables could not be reconstructed.')
        output[which][offset:offset + length] = data
    return output


def prepare(path, output, dat=None):
    profile, blobs, sizes = identify(path)
    tables = reconstruct(profile, blobs, sizes)
    if dat is not None:
        verify(tables, Path(dat).read_bytes())
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    for i, data in enumerate(tables):
        (output / f'native-tables{i}.bin').write_bytes(data)
    return profile['id']


def verify(tables, dat):
    if len(dat) < 16 or dat[:4] not in (b'TTMD', b'BT3D'):
        raise ValueError('Invalid portable build table manifest')
    count, expected = struct.unpack_from('<IQ', dat, 4)
    if len(dat) != 16 + count * 16:
        raise ValueError('Truncated portable build table manifest')
    digest = 0xcbf29ce484222325
    for address, which, offset, length in struct.iter_unpack('<4I', dat[16:]):
        if which > 1 or offset + length > len(tables[which]):
            raise ValueError('Invalid portable table range')
        for byte in tables[which][offset:offset + length]:
            digest = ((digest ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    if digest != expected:
        raise ValueError('This setup does not contain the startup recipes required by the game build.')


def check_recipe_coverage(dat):
    """Reject a stale recipe file before an installer can be packaged."""
    if len(dat) < 16 or dat[:4] not in (b'TTMD', b'BT3D'):
        raise ValueError('Invalid portable build table manifest')
    count, = struct.unpack_from('<I', dat, 4)
    if len(dat) != 16 + count * 16:
        raise ValueError('Truncated portable build table manifest')
    for profile in json.loads(RECIPES.read_text(encoding='utf-8'))['profiles']:
        for _, which, offset, length in struct.iter_unpack('<4I', dat[16:]):
            end = offset + length
            for source, start, size, *_ in sorted(profile['ranges']):
                if source == which and start <= offset < start + size:
                    offset = max(offset, start + size)
                if offset >= end:
                    break
            if offset < end:
                raise ValueError(f"Rebuild native_table_recipes.json for this build ({profile['id']}).")
