"""Validate scoped reference GS register/syscall publications; no renderer state restored."""
import hashlib,json,struct
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
PREFIX_SHA="632dae42262ff96c6c3df20b70ce804822a254acf7cad589bec615488ab08e8b"

EXTRA_PREFIXES={'gs-imr-after':(0x30FE64,0x30FE6C,'643facf39f44ead03e627a715a39fd4dd1288a6994ca78951a5a329ff5c674f1'),
                'gs-crt-after':(0x10026C,0x100274,'3ce7cacfef060753b37dd991b2bbee15bc77c56c04d033ac05055cda928dd7fe'),
 'gs-intc-after':(0x3104D4,0x3104DC,'9bdf3bfb2fe439ac5da2b31a6b72e245e9144e2fbf8099e28a91cbfe7689e760'),
 'gs-csr-field':(0x3104EC,0x3104F8,'346895ca2476c4fdd2636b8993f550f89ed54388844cc0163a68a34c4c83266a')}

def verify(path,ready_manifest,point='gs-csr-reset'):
    value,sp={'gs-csr-reset':(0x551B4000,0x1FFEAB0),'gs-imr-after':(0xFF00,0x1FFEAB0),'gs-crt-after':(0,0x1FFEB00),'gs-intc-after':(8,0x1FFEAE0),'gs-csr-field':(0x551B6004,0x1FFEAE0)}[point]
    start,end,prefix=(0x30FE34,0x30FE54,PREFIX_SHA) if point=='gs-csr-reset' else EXTRA_PREFIXES[point]
    m=json.loads(path.read_text(encoding='utf-8'));ee=path.with_name('ee-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point);p=m['patch']
    if (len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA
        or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap)
        or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap)) or p['marker_address']!=MARKER or p['marker_value']!=marker
        or ee[address:address+len(trap)]!=trap or hashlib.sha256(ee).hexdigest()!=m['capture_sha256']
        or hashlib.sha256(ee[start:end]).hexdigest()!=prefix
        or struct.unpack_from('<I',ee,MARKER)[0]!=marker
        or struct.unpack_from('<Q',ee,MARKER+8)[0]!=value
        or struct.unpack_from('<I',ee,MARKER+16)[0]!=sp):
        raise ValueError('GS CSR reset/readback evidence differs.')
    if any(m.get(key)!=ready_manifest[key] for key in ('pcsx2_sha256','bios_sha256','disc_sha256')):
        raise ValueError('GS checkpoint environment differs.')
    return struct.pack('<Q',value),m

INTERRUPT_POINTS={
 'gs-intc-before':(0x32F100,0x32F10C,'2c4af947b906d39895813949116eb3421716893fde29af41553833810e75c6f5',12,1),
 'gs-intc-cleared':(0x32F100,0x32F114,'ef4d49a5b8d26983d3c301188fcac49b5b9b65335e79901046cc7fa9cfa5283a',8,1),
 'gs-intc-event':(0x32F128,0x32F14C,'6d3fd08e79d1ca7c0564d7b492b35f4a5ea53540fe95e036b0c32be5961bd4a8',12,4),
 'gs-intc-acknowledged':(0x32F154,0x32F168,'ef4d49a5b8d26983d3c301188fcac49b5b9b65335e79901046cc7fa9cfa5283a',8,1)}

def verify_interrupts(folder,environment):
    """Independent reached-point evidence, not a continuous elapsed-cycle trace."""
    bundle=bytearray();manifests={}
    for point,(start,end,prefix,status,result) in INTERRUPT_POINTS.items():
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'))
        ee=path.with_name('ee-ram.bin').read_bytes();address,trap,marker=capture_trap(point);p=m['patch']
        publication=struct.pack('<4I',status,0x1802,0x1FFEAD0,result)
        if (len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA
            or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap)
            or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap))
            or p['marker_address']!=MARKER or p['marker_value']!=marker
            or ee[address:address+len(trap)]!=trap or hashlib.sha256(ee).hexdigest()!=m['capture_sha256']
            or hashlib.sha256(ee[start:end]).hexdigest()!=prefix
            or ee[MARKER:MARKER+4]!=struct.pack('<I',marker)
            or ee[MARKER+8:MARKER+24]!=publication):
            raise ValueError('INTC publication/prefix differs: '+point)
        if any(m.get(key)!=environment[key] for key in ('pcsx2_sha256','bios_sha256','disc_sha256')):
            raise ValueError('INTC checkpoint environment differs.')
        bundle+=publication;manifests[point]=m
    return bytes(bundle),manifests
