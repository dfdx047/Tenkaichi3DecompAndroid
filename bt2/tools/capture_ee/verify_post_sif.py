"""Validate fresh transport evidence without adopting an old service lifetime."""
import hashlib,json,struct
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
POINTS={'post-sif-before':(0x1003D8,0x1003E0,'603110c1b731c552095023bfad9d649621fae18222de2db7349ff6ad76d1f579'),
        'post-sif-after':(0x1003E0,0x1003E8,'8987e736b71beba5a5b1123f323edefb3ded8547c880824d44ae1522fc322182')}

def verify(folder,environment):
    manifests={};ram={};iop={};publications=[]
    for point,(start,end,prefix) in POINTS.items():
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'))
        ee=path.with_name('ee-ram.bin').read_bytes();ir=path.with_name('iop-ram.bin').read_bytes()
        address,trap,marker=capture_trap(point);p=m['patch']
        if (len(ee)!=32*1024*1024 or len(ir)!=2*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA
            or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap)
            or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap))
            or p['marker_address']!=MARKER or p['marker_value']!=marker
            or ee[address:address+len(trap)]!=trap or hashlib.sha256(ee).hexdigest()!=m['capture_sha256']
            or hashlib.sha256(ir).hexdigest()!=m['iop_capture_sha256']
            or hashlib.sha256(ee[start:end]).hexdigest()!=prefix
            or struct.unpack_from('<I',ee,MARKER)[0]!=marker):raise ValueError('Post-reset SIF provenance differs')
        if any(m.get(k)!=environment[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')):
            raise ValueError('Post-reset SIF reference environment differs')
        expected=(0,0,0x1FFED40,0x800044,0x84,0x3A9500,0x70000,0,0x1E640) if point.endswith('before') else (1,0,0x1FFED40,0xA00044,0x184,0x3A94E0,0x70000,0,0x1E640)
        pub=ee[MARKER+8:MARKER+44]
        if struct.unpack('<9I',pub)!=expected:raise ValueError('Post-reset SIF register/publication differs')
        owned=(0x1E640,0x1E6C0,0 if point.endswith('before') else 0x3A94C0,0x1E730)
        if struct.unpack_from('<4I',ir,0x1E700)!=owned:raise ValueError('Fresh IOP command ownership differs')
        owner=(0x203A94C0,0x203A9540,0x19600 if point.endswith('before') else 0x1E640,0x3A95C0,32,0,0,0x3A9740)
        if struct.unpack_from('<8I',ee,0x3A9598)!=owner:raise ValueError('Fresh EE command ownership differs')
        if point.endswith('after') and struct.unpack_from('<4I',ee,0x3A9800)!=(16,0,0x80000002,1):
            raise ValueError('Fresh INIT_CMD opt-1 packet differs')
        publications.append(pub);manifests[point]=m;ram[point]=ee;iop[point]=ir
    before,after=ram['post-sif-before'],ram['post-sif-after']
    reply=after[0x3A94C0:0x3A94D8]
    if struct.unpack('<6I',reply)!=(0,0,0x80000001,0,0,1) or struct.unpack_from('<I',before,0x3A9740)[0]!=0 or struct.unpack_from('<I',after,0x3A9740)[0]!=1:
        raise ValueError('Fresh SET_SREG response differs')
    return b''.join(publications)+iop['post-sif-before'][0x1E700:0x1E710]+iop['post-sif-after'][0x1E700:0x1E710]+reply+before[0x3A9740:0x3A9744]+after[0x3A9740:0x3A9744], {'captures':manifests,'limitations':'Transport checkpoint/INIT_CMD and SET_SREG effects only. No IOP execution, service bind or cycle-accurate DMA delivery.'}
