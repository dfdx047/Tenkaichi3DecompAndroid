"""Validate local first-bind evidence and prepare an isolated RPC_END probe image."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from capture_ee import ROOT, EXPECTED_SHA, MARKER, capture_trap


def validate_state(ee, iop):
    if len(ee) != 32 * 1024 * 1024 or len(iop) != 2 * 1024 * 1024:
        raise ValueError('Expected bounded EE and IOP captures.')
    def words(data, address, count):
        return struct.unpack_from('<' + str(count) + 'I', data, address)
    if words(ee, MARKER, 3) != (0xB70F, 0x801, 0x1FFEC20):
        raise ValueError('Bind return/publication differs.')
    if words(iop, 0x19600, 16) != (0, 0, 0x80000009, 0, 5, 0x203A97C0,
                                   2, 0x3A87A8, 0x80000592, 0, 0, 0, 0, 0, 0, 0):
        raise ValueError('Consumed bind request differs.')
    if words(iop, 0x196C0, 3) != (0x19600, 0x19680, 0x3A94C0):
        raise ValueError('IOP receive table differs.')
    if words(ee, 0x3A94C0, 16) != (0, 0, 0x80000008, 0, 0, 0x203A97C0,
                                    0, 0x3A87A8, 0x80000009, 0x3F570, 0x3FB20, 0, 0, 0, 0, 0):
        raise ValueError('Consumed RPC_END response differs.')
    # Public SifRpcServerData layout; check response pointers against local data.
    server = words(iop, 0x3F570, 17)
    if server[:7] != (0x80000592, 0x39534, 0x3FB20, 0, 0, 0, 0) or server[16] != 0x3F558:
        raise ValueError('Registered CD initialization service differs.')
    if words(iop, 0x3F558, 3)[2] != 0x3F570:
        raise ValueError('Service is not linked to its RPC queue.')
    if words(ee, 0x3A87A8, 10) != (0, 2, 3, 0, 0, 0x3FB20, 0, 0, 0, 0x3F570):
        raise ValueError('Completed EE client differs.')
    packet = words(ee, 0x3A97C0, 16)
    if packet != (64, 0, 0x80000009, 0, 4, 0x203A97C0, 0,
                  0x3A87A8, 0x80000592, 0, 0, 0, 0, 0, 0, 0):
        raise ValueError('Freed request packet differs.')
    for command, callback in ((8, 0x3322E8), (9, 0x3326C0), (10, 0x3328E0), (12, 0x3323F8)):
        if words(ee, 0x3A95C0 + command * 12, 3) != (callback, 0x3AAFC0, 0x38BDF0):
            raise ValueError('Registered EE RPC callback differs.')


def verify(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    ee = path.with_name('ee-ram.bin').read_bytes()
    iop = path.with_name('iop-ram.bin').read_bytes()
    address, trap, marker = capture_trap('sif-bind-after')
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != EXPECTED_SHA
            or not manifest['capture_point'].startswith('sif-bind-after:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != list(struct.unpack('<8I', trap))
            or patch['marker_address'] != MARKER or patch['marker_value'] != marker
            or manifest['observed_sif_dma_result'] != 0x801
            or manifest['observed_sif_dma_sp'] != 0x1FFEC20
            or hashlib.sha256(ee).hexdigest() != manifest['capture_sha256']
            or hashlib.sha256(iop).hexdigest() != manifest['iop_capture_sha256']
            or ee[address:address + len(trap)] != trap):
        raise ValueError('Bind capture provenance/instrumentation differs.')
    validate_state(ee, iop)
    return ee, manifest


def verify_call_semaphore(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    ee = path.with_name('ee-ram.bin').read_bytes()
    address, trap, marker = capture_trap('rpc-call-sema')
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != EXPECTED_SHA
            or not manifest['capture_point'].startswith('rpc-call-sema:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != list(struct.unpack('<8I', trap))
            or patch['marker_address'] != MARKER or patch['marker_value'] != marker
            or len(ee) != 32 * 1024 * 1024
            or hashlib.sha256(ee).hexdigest() != manifest['capture_sha256']
            or ee[address:address + len(trap)] != trap
            or struct.unpack_from('<III', ee, MARKER) != (marker, 3, 0x1FFEBD0)
            or struct.unpack_from('<6I', ee, 0x1FFEBD0) != (0, 1, 0, 0, 0, 0x3838E8)):
        raise ValueError('RPC call semaphore capture differs.')
    return ee[0x1FFEBD0:0x1FFEBD0 + 24] + struct.pack('<I', 3), manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--run-probe', action='store_true', help='Verify generated provenance and run the built isolated callback probe.')
    parser.add_argument('--call-sema-capture', type=Path, help='Optional checked first RPC call semaphore result.')
    args = parser.parse_args()
    ee, manifest = verify(args.manifest)
    folder = ROOT / 'work/bt2-generation'
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'rpc-bind-callback.bin').write_bytes(ee)
    iop = args.manifest.with_name('iop-ram.bin').read_bytes()
    bundle = iop + ee[0x3A94C0:0x3A9500] + struct.pack('<I', 0x801)
    call = verify_call_semaphore(args.call_sema_capture) if args.call_sema_capture else None
    if call:
        if any(call[1][key] != manifest[key] for key in ('pcsx2_sha256', 'bios_sha256')):
            raise ValueError('RPC bind and call semaphore reference environment differ.')
        bundle += call[0]
    (folder / 'sif-bind.bin').write_bytes(bundle)
    (folder / 'rpc-bind-callback.json').write_text(json.dumps({
        'provenance': manifest, 'call_semaphore_provenance': call[1] if call else None,
        'service_id': '0x80000592',
        'service_role': 'CD initialization (public PS2SDK registration)',
        'scope': 'Isolated generated RPC_END callback; no DMA or interrupt scheduling.'
    }, indent=2) + '\n', encoding='utf-8')
    print('Validated local RPC bind request, service queue, response and completed EE client.')
    if args.run_probe:
        sys.path.insert(0, str(ROOT / 'tools/ps2recomp'))
        from run_pipeline import verify_generated
        verify_generated()
        subprocess.run([str(ROOT / 'build/probe/Debug/bt2-rpc-bind-probe.exe'),
                        str(folder / 'rpc-bind-callback.bin')], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
