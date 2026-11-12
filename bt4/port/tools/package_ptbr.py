#!/usr/bin/env python3
"""Build the versioned PT-BR overlay variant of the BT4 language container."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile


def build(source, destination):
    payload = io.BytesIO()
    paths = set()
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(payload, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as target:
        for name in sorted(original.namelist()):
            if name.endswith('/'):
                continue
            if name.startswith('fonts/'):
                output = 'PTBR/' + name
            elif name.startswith('textures/PTBR/'):
                output = 'PTBR/textures/' + name[len('textures/PTBR/'):]
            elif name == 'PTBR/fontes-sem-conflito.txt' or name.startswith(('PTBR/game-text/', 'PTBR/gamedata/mods/')):
                output = name
            else:
                continue
            path = Path(output)
            if path.is_absolute() or '..' in path.parts or ':' in output or '\\' in output:
                raise ValueError('Unsafe package path: ' + output)
            if path.suffix.lower() in ('.exe', '.dll', '.cmd', '.bat', '.ps1', '.vbs', '.js'):
                raise ValueError('Executable content is not a language resource: ' + output)
            if output.lower() in paths:
                raise ValueError('Duplicate package path: ' + output)
            paths.add(output.lower())
            info = zipfile.ZipInfo(output, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            target.writestr(info, original.read(name), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    data = payload.getvalue()
    header = struct.pack('<8sIIQ32s8s', b'BT4PTP01', 1, 64, len(data), hashlib.sha256(data).digest(), bytes(8))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(header + data)
    return {'name': destination.name, 'bytes': destination.stat().st_size,
            'sha256': hashlib.sha256(header + data).hexdigest(), 'payload_sha256': hashlib.sha256(data).hexdigest(),
            'format': 'BT4PTP01', 'version': 1, 'files': len(paths), 'executable_files': 0}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    arguments = parser.parse_args()
    print(json.dumps(build(arguments.source, arguments.destination), indent=2))
