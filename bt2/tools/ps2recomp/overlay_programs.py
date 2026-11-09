"""Runtime-loaded EE code segments (overlays) handled as separate generated programs.

Each overlay is extracted from the local disc, checked against a pinned identity and wrapped as a
minimal local ELF (one executable `.text` at its link address) so the existing analysis, map and
generation tools can process it. Every derived file stays under ignored work/ and game/generated/.
"""
import hashlib
import struct
import sys
from pathlib import Path
from build_tools import recompiler

ROOT = Path(__file__).resolve().parents[2]

# DBZP.BIN: raw R5900 code/data linked for 0x3B3100 (just past the executable's last section) and
# called directly by main after its first file load (EE_OVERLAY_PLAN.md).
PROGRAMS = {
    'dbzp': {'directory': b'BIN', 'filename': b'DBZP.BIN;1', 'base': 0x3B3100, 'size': 0x7D48F,
             'sha256': '04d36d81b0d9bc9e7f8d66acac243ccaec9747cda88d27e456e5047e89f4d065', 'entry': 0x3B7FA8},
}


def program_for(address):
    """Name of the overlay whose code range contains `address`, else None."""
    for name, p in PROGRAMS.items():
        if isinstance(address, int) and p['base'] <= address < p['base'] + p['size']:
            return name
    return None


def paths(name):
    folder = ROOT / 'work/overlay' / name
    return {'folder': folder, 'image': folder / f'{name}.bin', 'elf': folder / f'{name}.elf',
            'ghidra_project': f'BT2_{name.upper()}', 'exports': ROOT / 'work/ghidra' / f'exports-{name}',
            'generation': ROOT / 'work/bt2-generation' / name, 'output': ROOT / 'game/generated' / name,
            'runtime': ROOT / 'game/generated' / f'{name}-runtime'}


def wrap(image, base, entry):
    """Minimal ELF32 LE MIPS executable: header, one PT_LOAD and sections null/.text/.shstrtab."""
    names = b'\0.text\0.shstrtab\0'
    text_offset = 0x100
    names_offset = text_offset + len(image)
    section_offset = (names_offset + len(names) + 3) & ~3
    header = struct.pack('<16sHHIIIIIHHHHHH', b'\x7fELF\x01\x01\x01' + bytes(9), 2, 8, 1, entry, 52,
                         section_offset, 0x20924001, 52, 32, 1, 40, 3, 2)
    program = struct.pack('<8I', 1, text_offset, base, base, len(image), len(image), 5, 16)
    sections = bytes(40) + struct.pack('<10I', 1, 1, 6, base, text_offset, len(image), 0, 0, 16, 0) \
        + struct.pack('<10I', 7, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0)
    data = bytearray(section_offset + len(sections))
    data[:52] = header
    data[52:84] = program
    data[text_offset:names_offset] = image
    data[names_offset:names_offset + len(names)] = names
    data[section_offset:] = sections
    return bytes(data)


def load(name):
    """Return (image, wrapped ELF bytes) after checking the local extraction against its identity."""
    p = PROGRAMS[name]
    files = paths(name)
    image = files['image'].read_bytes()
    if len(image) != p['size'] or hashlib.sha256(image).hexdigest() != p['sha256']:
        raise ValueError(f'Local {name} overlay image differs from its pinned identity.')
    elf = files['elf'].read_bytes()
    if elf != wrap(image, p['base'], p['entry']):
        raise ValueError(f'Local {name} wrapped ELF differs; rerun overlay_programs.py --extract.')
    return image, elf


def verify_generated(name):
    """Overlay generation provenance: wrapped identity, map, tool lock/binary and raw outputs."""
    import json
    _, elf = load(name)
    files = paths(name)
    manifest = json.loads((files['generation'] / 'generation.manifest.json').read_text(encoding='utf-8'))
    if (manifest['executable_sha256'] != hashlib.sha256(elf).hexdigest() or manifest.get('program') != name
            or manifest['map_sha256'] != hashlib.sha256((files['generation'] / 'candidates.csv').read_bytes()).hexdigest()
            or manifest['lock_sha256'] != hashlib.sha256((ROOT / 'config/ps2recomp.lock.json').read_bytes()).hexdigest()
            or manifest['recompiler_sha256'] != hashlib.sha256(recompiler(ROOT).read_bytes()).hexdigest()):
        raise ValueError(f'{name} overlay generation provenance differs; rerun generation.')
    actual = {path.name for path in files['output'].iterdir() if path.is_file() and path.suffix in ('.cpp', '.h')}
    if actual != set(manifest['files']):
        raise ValueError(f'{name} overlay generated source file set changed.')
    for filename, expected in manifest['files'].items():
        if Path(filename).name != filename or hashlib.sha256((files['output'] / filename).read_bytes()).hexdigest() != expected:
            raise ValueError(f'{name} overlay generated source changed: {filename}')
    return manifest


def extract(name, disc):
    sys.path.insert(0, str(ROOT / 'tools/iop'))
    from inspect_image import disc_image
    p = PROGRAMS[name]
    image = disc_image(disc, p['filename'], p['directory'], limit=p['size'])
    if len(image) != p['size'] or hashlib.sha256(image).hexdigest() != p['sha256']:
        raise ValueError(f'{name} overlay on disc differs from its pinned identity.')
    files = paths(name)
    files['folder'].mkdir(parents=True, exist_ok=True)
    files['image'].write_bytes(image)
    files['elf'].write_bytes(wrap(image, p['base'], p['entry']))
    return files


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extract', type=Path, metavar='DISC', required=True, help='Local disc image')
    parser.add_argument('--program', choices=sorted(PROGRAMS), default='dbzp')
    args = parser.parse_args()
    files = extract(args.program, args.extract)
    print(f'Extracted and wrapped {args.program} under {files["folder"]}; output remains ignored.')
