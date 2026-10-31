"""Authenticate a local BT2 ISO against recorded Redump metadata and its local ELF."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]


def hash_stream(stream, size=None):
    hashes = {name: hashlib.new(name) for name in ('md5', 'sha1', 'sha256')}
    remaining = size
    while remaining is None or remaining:
        chunk = stream.read(8 * 1024 * 1024 if remaining is None else min(remaining, 8 * 1024 * 1024))
        if not chunk:
            if remaining:
                raise ValueError('Truncated disc extent.')
            break
        for digest in hashes.values():
            digest.update(chunk)
        if remaining is not None:
            remaining -= len(chunk)
    return {name: digest.hexdigest() for name, digest in hashes.items()}


def root_executable(stream, disc_size):
    def read(offset, size):
        if offset < 0 or size < 0 or offset + size > disc_size:
            raise ValueError('ISO extent outside disc.')
        stream.seek(offset)
        data = stream.read(size)
        if len(data) != size:
            raise ValueError('Truncated ISO metadata.')
        return data
    def extent(record):
        if len(record) < 34:
            raise ValueError('Truncated ISO directory record.')
        lba, = struct.unpack_from('<I', record, 2)
        size, = struct.unpack_from('<I', record, 10)
        if struct.unpack_from('>I', record, 6)[0] != lba or struct.unpack_from('>I', record, 14)[0] != size:
            raise ValueError('ISO endian-paired extent fields differ.')
        if record[1] or record[26] or record[27]:
            raise ValueError('Extended/interleaved ISO records unsupported.')
        if lba * 2048 + size > disc_size:
            raise ValueError('ISO extent outside disc.')
        return lba * 2048, size
    pvd = read(16 * 2048, 2048)
    if pvd[:7] != b'\x01CD001\x01' or pvd[128:132] != b'\x00\x08\x08\x00':
        raise ValueError('Expected ISO9660 primary volume with 2048-byte blocks.')
    start, size = extent(pvd[156:156 + pvd[156]])
    if size > 16 * 1024 * 1024:
        raise ValueError('Unreasonable root directory size.')
    directory = read(start, size)
    matches = []
    pos = 0
    while pos < len(directory):
        length = directory[pos]
        if not length:
            pos = (pos // 2048 + 1) * 2048
            continue
        if length < 34 or pos + length > len(directory) or pos % 2048 + length > 2048:
            raise ValueError('Invalid ISO directory record size.')
        record = directory[pos:pos + length]
        name_size = record[32]
        if 33 + name_size > length:
            raise ValueError('Invalid ISO filename length.')
        if record[33:33 + name_size] == b'SLUS_214.41;1':
            if record[25] & (2 | 128):
                raise ValueError('Expected one regular executable extent.')
            matches.append(extent(record))
        pos += length
    if len(matches) != 1:
        raise ValueError('Expected exactly one root SLUS_214.41;1 record.')
    return matches[0]


def validate(iso, executable, reference):
    size = iso.stat().st_size
    if size != reference['disc']['size']:
        raise ValueError('Disc size differs from trusted reference.')
    with iso.open('rb') as stream:
        hashes = hash_stream(stream)
        for name in ('md5', 'sha1'):
            if hashes[name] != reference['disc'][name]:
                raise ValueError(f'Disc {name} differs from trusted reference.')
        offset, elf_size = root_executable(stream, size)
        stream.seek(offset)
        elf_hashes = hash_stream(stream, elf_size)
    with executable.open('rb') as stream:
        local_hashes = hash_stream(stream)
    if executable.stat().st_size != elf_size or local_hashes['sha256'] != elf_hashes['sha256']:
        raise ValueError('Local executable differs from the authenticated disc extent.')
    if local_hashes['sha256'] != reference['executable']['sha256']:
        raise ValueError('Executable differs from the recorded target revision.')
    return {'status': 'trusted retail revision confirmed', 'reference_source': reference['source'],
            'disc_size': size, 'disc_hashes': hashes, 'executable_size': elf_size,
            'executable_sha256': local_hashes['sha256'], 'disc_executable_offset': offset,
            'limitations': 'Matches the cited Redump datfile entry and binds the local ELF to that disc; authenticity depends on reference metadata.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso', type=Path)
    parser.add_argument('--executable', type=Path, default=ROOT / 'rom/SLUS_214.41')
    args = parser.parse_args()
    reference = json.loads((ROOT / 'config/retail_revision.json').read_text(encoding='utf-8'))
    report = validate(args.iso, args.executable, reference)
    folder = ROOT / 'work/disc-validation'
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'retail-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
