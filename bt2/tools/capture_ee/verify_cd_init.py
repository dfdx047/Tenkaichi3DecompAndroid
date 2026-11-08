"""Validate the first local standalone-ELF CD initialization RPC completion."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from capture_ee import ROOT, EXPECTED_SHA, MARKER, capture_trap


def validate_state(ee, iop):
    if len(ee) != 32 * 1024 * 1024 or len(iop) != 2 * 1024 * 1024:
        raise ValueError('Expected bounded EE/IOP captures.')
    def words(data, address, count):
        return struct.unpack_from('<' + str(count) + 'I', data, address)
    request = (0x400, 0x3FB20, 0x8000000A, 0, 5, 0x203A97C0, 3, 0x3A87A8,
               0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0)
    response = (0x1000, 0x3695C0, 0x80000008, 0, 0, 0, 0, 0x3A87A8,
                0x8000000A, 0, 0, 0, 0, 0, 0, 0)
    freed = list(request)
    freed[0], freed[4], freed[6] = 0x440, 4, 0
    if (words(ee, MARKER, 3) != (0xB711, 0x902, 0x1FFEBD0)
            or words(iop, 0x19600, 16) != request
            or words(ee, 0x3A94C0, 16) != response
            or words(ee, 0x3A97C0, 16) != tuple(freed)
            or words(ee, 0x3A8800, 1) != (0,)
            or words(iop, 0x3FB20, 1) != (0,)
            or words(ee, 0x3695C0, 4) != (1, 0, 0, 0)
            or words(iop, 0x196C0, 3) != (0x19600, 0x19680, 0x3A94C0)):
        raise ValueError('CD-init request/response/data effects differ.')
    server = words(iop, 0x3F570, 17)
    if (server[:4] != (0x80000592, 0x39534, 0x3FB20, 4)
            or server[7:14] != (0x3A87A8, 0x203A97C0, 0, 0x3695C0, 16, 1, 0)
            or server[16] != 0x3F558 or words(iop, 0x3F558 + 8, 1) != (0x3F570,)
            or words(ee, 0x3A87A8, 3) != (0, 3, 3)
            or words(ee, 0x3A87BC, 1) != (0x3FB20,)
            or words(ee, 0x3A87C4, 2) != (0, 0)
            or words(ee, 0x3A87CC, 1) != (0x3F570,)):
        raise ValueError('CD-init server/client linkage differs.')


def verify(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    ee = path.with_name('ee-ram.bin').read_bytes()
    iop = path.with_name('iop-ram.bin').read_bytes()
    address, trap, marker = capture_trap('cd-init-after')
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != EXPECTED_SHA
            or not manifest['capture_point'].startswith('cd-init-after:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != list(struct.unpack('<8I', trap))
            or patch['marker_address'] != MARKER or patch['marker_value'] != marker
            or manifest['observed_sif_dma_result'] != 0x902
            or manifest['observed_sif_dma_sp'] != 0x1FFEBD0
            or hashlib.sha256(ee).hexdigest() != manifest['capture_sha256']
            or hashlib.sha256(iop).hexdigest() != manifest['iop_capture_sha256']
            or ee[address:address + len(trap)] != trap):
        raise ValueError('CD-init capture provenance/instrumentation differs.')
    validate_state(ee, iop)
    return struct.pack('<II', 0, 0x902) + ee[0x3A94C0:0x3A9500] + ee[0x3695C0:0x3695D0], manifest


def verify_semaphores(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    ee = path.with_name('ee-ram.bin').read_bytes()
    address, trap, marker = capture_trap('cd-semaphores')
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != EXPECTED_SHA
            or not manifest['capture_point'].startswith('cd-semaphores:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != list(struct.unpack('<8I', trap))
            or patch['marker_address'] != MARKER or patch['marker_value'] != marker
            or len(ee) != 32 * 1024 * 1024
            or hashlib.sha256(ee).hexdigest() != manifest['capture_sha256']
            or ee[address:address + len(trap)] != trap
            or struct.unpack_from('<III', ee, MARKER) != (marker, 6, 0x1FFEC30)
            or struct.unpack_from('<6I', ee, 0x1FFEC30) != (0x370000, 1, 0, 0, 0x370000, 0x3825E8)
            or struct.unpack_from('<3I', ee, 0x3683E8) != (3, 4, 5)):
        raise ValueError('CD semaphore setup capture differs.')
    return ee[0x1FFEC30:0x1FFEC48] + struct.pack('<4I', 3, 4, 5, 6), manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--semaphores-capture', type=Path)
    args = parser.parse_args()
    bundle, manifest = verify(args.manifest)
    semaphores = verify_semaphores(args.semaphores_capture) if args.semaphores_capture else None
    if semaphores:
        if any(manifest[key] != semaphores[1][key] for key in ('pcsx2_sha256', 'bios_sha256')):
            raise ValueError('CD-init and semaphore reference environments differ.')
        bundle += semaphores[0]
    folder = ROOT / 'work/bt2-generation'
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'cd-init.bin').write_bytes(bundle)
    (folder / 'cd-init.json').write_text(json.dumps({'cd_init': manifest,
        'semaphores': semaphores[1] if semaphores else None}, indent=2) + '\n', encoding='utf-8')
    print('Validated first CD-init mode-0 request/reply; no disc access established.')


if __name__ == '__main__':
    main()
