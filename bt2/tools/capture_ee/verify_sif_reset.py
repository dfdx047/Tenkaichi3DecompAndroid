"""Verify reached DMA-stop/query checkpoints; no IOP reset completion inferred."""
import hashlib
import json
import struct
from capture_ee import EXPECTED_SHA, MARKER, capture_trap

KERNEL_STOP_SHA = '6a47c4cee303ef9916bbf8229e556dcd2f84ccdd054239b4c10f3a6dadecb19e'


def validate_publication(ee, point):
    address, trap, marker = capture_trap(point)
    if len(ee) != 32 * 1024 * 1024:
        raise ValueError('Expected complete bounded EE capture.')
    result = 0 if point == 'sif-stop-after' else 0x19600
    if struct.unpack_from('<III', ee, MARKER) != (marker, result, 0x1FFEC80):
        raise ValueError('SIF stop/query publication differs.')
    if struct.unpack_from('<I', ee, 0x14EAC)[0] != 0x80006380:
        raise ValueError('SIF stop syscall table differs.')
    if hashlib.sha256(ee[0x6380:0x63AC]).hexdigest() != KERNEL_STOP_SHA:
        raise ValueError('SIF stop kernel implementation differs.')
    return struct.pack('<I',result)


def verify(path, point):
    manifest=json.loads(path.read_text(encoding='utf-8'))
    ee=path.with_name('ee-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point); patch=manifest['patch']
    if (manifest['source_elf_sha256'] != EXPECTED_SHA or not manifest['capture_point'].startswith(point+':')
        or patch['address']!=address or patch['size']!=len(trap)
        or patch['authored_words']!=list(struct.unpack('<8I',trap))
        or patch['marker_address']!=MARKER or patch['marker_value']!=marker
        or hashlib.sha256(ee).hexdigest()!=manifest['capture_sha256'] or ee[address:address+len(trap)]!=trap):
        raise ValueError('SIF stop/query provenance differs.')
    return validate_publication(ee,point),manifest


def validate_smflag_publication(ee,point):
    if len(ee) != 32*1024*1024: raise ValueError('Expected bounded EE capture.')
    marker=capture_trap(point)[2]
    expected=(marker,0 if point=='reset-smflag-before' else 0x30000,0x1FFEC80,0x30000)
    if struct.unpack_from('<4I',ee,MARKER)!=expected:
        raise ValueError('Reached SMFLAG publication differs.')


def verify_smflag_pair(before,after):
    manifests=[]
    for path,point in ((before,'reset-smflag-before'),(after,'reset-smflag-after')):
        m=json.loads(path.read_text(encoding='utf-8'));ee=path.with_name('ee-ram.bin').read_bytes()
        address,trap,marker=capture_trap(point);p=m['patch']
        if (m['source_elf_sha256']!=EXPECTED_SHA or not m['capture_point'].startswith(point+':')
            or p['address']!=address or p['size']!=len(trap) or p['authored_words']!=list(struct.unpack('<12I',trap))
            or p['marker_address']!=MARKER or p['marker_value']!=marker
            or hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or ee[address:address+len(trap)]!=trap):
            raise ValueError('SMFLAG pair provenance differs.')
        validate_smflag_publication(ee,point);manifests.append(m)
    if any(manifests[0][key]!=manifests[1][key] for key in ('source_elf_sha256','pcsx2_sha256','bios_sha256')):
        raise ValueError('SMFLAG pair environments differ.')
    return struct.pack('<III',0x30000,0x30000,0x30000),{'before':manifests[0],'after':manifests[1]}


def verify_submission_chain(folder, image_report):
    expected={'reset-submit-after':(0xB719,0xE01,None),
              'reset-cmd-clear':(0xB71D,0x20000,0x20000),
              'reset-flags-after':(0xB71A,0,0),
              'reset-rpc-clear':(0xB71B,0,0),'reset-subaddr-clear':(0xB71C,0,0)}
    manifests={}
    for point,(marker,result,flag) in expected.items():
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'))
        ee=path.with_name('ee-ram.bin').read_bytes();address,trap,_=capture_trap(point);p=m['patch']
        if (len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA
            or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap)
            or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap))
            or p['marker_address']!=MARKER or p['marker_value']!=marker
            or hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or ee[address:address+len(trap)]!=trap
            or struct.unpack_from('<III',ee,MARKER)!=(marker,result,0x1FFEC80)
            or (flag is not None and struct.unpack_from('<I',ee,MARKER+12)[0]!=flag)
            or m.get('disc_sha256')!=image_report['disc_sha256']):
            raise ValueError('Disc-backed reboot chain evidence differs.')
        if struct.unpack_from('<6I',ee,0x3ACC40)!=(104,0,0x80000003,0,36,0) or ee[0x3ACC58:0x3ACC7D]!=b'rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1\0':
            raise ValueError('Reboot packet/path differs.')
        manifests[point]=m
    first=next(iter(manifests.values()))
    if any(any(m[key]!=first[key] for key in ('pcsx2_sha256','bios_sha256','disc_sha256')) for m in manifests.values()):
        raise ValueError('Reboot chain environments differ.')
    return struct.pack('<5I',0xE01,0x20000,0,0,0),manifests


READY_BRANCH_SHA='94650c2e1225da69a0ae2025860eba1f6c95dd59455df4e74c94a1710c25f7bc'


def verify_ready(path, submission_manifests):
    m=json.loads(path.read_text(encoding='utf-8'));ee=path.with_name('ee-ram.bin').read_bytes()
    iop=path.with_name('iop-ram.bin').read_bytes();address,trap,marker=capture_trap('reset-ready');p=m['patch']
    if (len(ee)!=32*1024*1024 or len(iop)!=2*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA
        or not m['capture_point'].startswith('reset-ready:') or p['address']!=address or p['size']!=len(trap)
        or p['authored_words']!=list(struct.unpack('<8I',trap)) or p['marker_address']!=MARKER or p['marker_value']!=marker
        or ee[address:address+len(trap)]!=trap or hashlib.sha256(ee).hexdigest()!=m['capture_sha256']
        or hashlib.sha256(ee[0x338108:0x338128]).hexdigest()!=READY_BRANCH_SHA
        or hashlib.sha256(iop).hexdigest()!=m['iop_capture_sha256']
        or struct.unpack_from('<3I',ee,MARKER)!=(marker,0x40000,0x1FFED30)):
        raise ValueError('Ready-branch evidence differs.')
    first=submission_manifests['reset-submit-after']
    if any(m.get(key)!=first[key] for key in ('pcsx2_sha256','bios_sha256','disc_sha256')):
        raise ValueError('Ready-branch environment differs.')
    # Only the tested bit is established, not the complete SMFLAG register.
    return struct.pack('<I',0x40000),m
