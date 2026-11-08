"""Validate the locally reached second CD service bind; no general service backend."""
import hashlib
import json
import struct
from pathlib import Path
from capture_ee import EXPECTED_SHA, MARKER, capture_trap


def validate_state(ee, iop):
    if len(ee) != 32*1024*1024 or len(iop) != 2*1024*1024:
        raise ValueError('Capture sizes differ.')
    def words(data, address, count):
        return struct.unpack_from('<' + str(count) + 'I', data, address)
    if words(ee, MARKER, 3) != (0xB713, 0, 0x1FFECB0):
        raise ValueError('SCMD bind publication differs.')
    if words(ee, 0x1FFEC40, 6) != (0x370000, 1, 0, 0, 2, 0x3838D8):
        raise ValueError('SCMD semaphore descriptor differs.')
    if words(ee, 0x369F40, 10) != (0, 4, 7, 0, 0, 0x3F6D8, 0, 0, 0, 0x3F690):
        raise ValueError('SCMD completed client differs.')
    request = (0, 0, 0x80000009, 0, 5, 0x203A97C0, 4, 0x369F40,
               0x80000593, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0)
    if words(iop, 0x19600, 16) != request:
        raise ValueError('SCMD bind request differs.')
    freed = list(request); freed[0], freed[4], freed[6] = 64, 4, 0
    if words(ee, 0x3A97C0, 16) != tuple(freed):
        raise ValueError('SCMD freed packet differs.')
    if words(ee, 0x3A94C0, 16) != (0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,
                                  0x80000009,0x3F690,0x3F6D8,0,0,0,0,0):
        raise ValueError('SCMD reply differs.')
    server = words(iop, 0x3F690, 17)
    if server[:7] != (0x80000593,0x3D4E8,0x3F6D8,0,0,0,0) or server[16] != 0x3F558:
        raise ValueError('SCMD server differs.')
    if words(iop, 0x3F648, 17)[14] != 0x3F690 or words(iop, 0x3F570, 17)[14] != 0x3F648:
        raise ValueError('SCMD queue linkage differs.')


def verify(path):
    manifest=json.loads(path.read_text(encoding='utf-8'))
    ee=path.with_name('ee-ram.bin').read_bytes(); iop=path.with_name('iop-ram.bin').read_bytes()
    address, trap, marker=capture_trap('scmd-bind-after'); patch=manifest['patch']
    if (manifest['source_elf_sha256'] != EXPECTED_SHA
        or not manifest['capture_point'].startswith('scmd-bind-after:')
        or patch['address'] != address or patch['size'] != len(trap)
        or patch['authored_words'] != list(struct.unpack('<8I',trap))
        or patch['marker_address'] != MARKER or patch['marker_value'] != marker
        or hashlib.sha256(ee).hexdigest() != manifest['capture_sha256']
        or hashlib.sha256(iop).hexdigest() != manifest['iop_capture_sha256']
        or ee[address:address+len(trap)] != trap):
        raise ValueError('SCMD capture provenance differs.')
    validate_state(ee,iop)
    return ee[0x1FFEC40:0x1FFEC58]+ee[0x3A94C0:0x3A9500], manifest


def validate_mmode_state(ee, iop):
    if len(ee) != 32*1024*1024 or len(iop) != 2*1024*1024:
        raise ValueError('Capture sizes differ.')
    def words(data,address,count): return struct.unpack_from('<'+str(count)+'I',data,address)
    if words(ee,MARKER,3)!=(0xB714,0,0x1FFECF0): raise ValueError('Media-mode publication differs.')
    if words(ee,0x1FFEC30,6)!=(0,1,0,0,0x370000,0x3838E8): raise ValueError('Media-mode semaphore differs.')
    if words(ee,0x369F40,10)!=(0,5,7,0,0,0x3F6D8,0x38BDF0,0,0,0x3F690): raise ValueError('Media-mode client differs.')
    request=(0x400,0x3F6D8,0x8000000A,0,5,0x203A97C0,5,0x369F40,34,4,0x3695C0,4,1,0x3F690,0,0)
    if words(iop,0x19600,16)!=request: raise ValueError('Media-mode request differs.')
    freed=list(request);freed[0],freed[4],freed[6]=0x440,4,0
    if words(ee,0x3A97C0,16)!=tuple(freed): raise ValueError('Media-mode packet differs.')
    if words(ee,0x3A94C0,16)!=(0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0):
        raise ValueError('Media-mode reply differs.')
    if words(ee,0x369A00,1)!=(2,) or words(iop,0x3F6D8,1)!=(2,) or words(ee,0x3695C0,1)!=(0,):
        raise ValueError('Media-mode input/output differs.')
    server=words(iop,0x3F690,17)
    if server[:13]!=(0x80000593,0x3D4E8,0x3F6D8,4,0,0,0,0x369F40,0x203A97C0,34,0x3695C0,4,1) or server[16]!=0x3F558:
        raise ValueError('Media-mode service differs.')


def verify_mmode(path):
    manifest=json.loads(path.read_text(encoding='utf-8'))
    ee=path.with_name('ee-ram.bin').read_bytes();iop=path.with_name('iop-ram.bin').read_bytes()
    address,trap,marker=capture_trap('scmd-mmode-after');patch=manifest['patch']
    if (manifest['source_elf_sha256']!=EXPECTED_SHA or not manifest['capture_point'].startswith('scmd-mmode-after:')
        or patch['address']!=address or patch['size']!=len(trap)
        or patch['authored_words']!=list(struct.unpack('<8I',trap))
        or patch['marker_address']!=MARKER or patch['marker_value']!=marker
        or hashlib.sha256(ee).hexdigest()!=manifest['capture_sha256']
        or hashlib.sha256(iop).hexdigest()!=manifest['iop_capture_sha256'] or ee[address:address+len(trap)]!=trap):
        raise ValueError('Media-mode capture provenance differs.')
    validate_mmode_state(ee,iop)
    return ee[0x1FFEC30:0x1FFEC48]+ee[0x3A94C0:0x3A9500]+ee[0x369A00:0x369A04]+ee[0x3695C0:0x3695C4],manifest
