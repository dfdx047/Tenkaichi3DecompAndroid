"""Validate local LOADFILE bind/version evidence; never claim module execution."""
import hashlib,json,struct
from capture_ee import EXPECTED_SHA,MARKER,LOADFILE_POINTS,capture_trap

CALLER_SHA='631de72f8f76ebe8a18bc3ef8d41ada660ffbd3947b868a67a97faa4dd334900'

def verify(folder,environment):
    replies=[];manifests={}
    for point in ('loadfile-bind-after','loadfile-version-after','loadfile-module-before'):
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'))
        ee=path.with_name('ee-ram.bin').read_bytes();iop=path.with_name('iop-ram.bin').read_bytes()
        address,trap,marker=capture_trap(point);p=m['patch']
        if len(ee)!=32*1024*1024 or len(iop)!=2*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap) or p['authored_words']!=list(struct.unpack('<8I',trap)) or p['marker_address']!=MARKER or p['marker_value']!=marker or ee[address:address+len(trap)]!=trap or struct.unpack_from('<I',ee,MARKER)[0]!=marker:raise ValueError('LOADFILE trap/identity differs')
        if hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or hashlib.sha256(iop).hexdigest()!=m['iop_capture_sha256'] or any(m.get(k)!=environment[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')):raise ValueError('LOADFILE hash/environment differs')
        if struct.unpack_from('<4I',iop,0x1E700)!=(0x1E640,0x1E6C0,0x3A94C0,0x1E730) or struct.unpack_from('<3I',iop,0x22568)!=(0x80000006,0x20DAC,0x225B0) or struct.unpack_from('<I',iop,0x22568+64)[0]!=0x22550 or struct.unpack_from('<I',iop,0x22550+8)[0]!=0x22568:raise ValueError('LOADFILE ownership differs')
        if hashlib.sha256(ee[0x1003E0:0x100414]).hexdigest()!=CALLER_SHA:raise ValueError('LOADFILE unique startup caller differs')
        client=struct.unpack_from('<10I',ee,0x3ACC00)
        if client[0]!=0 or client[2]!=7 or client[5]!=0x225B0 or client[9]!=0x22568:raise ValueError('LOADFILE client differs')
        if point=='loadfile-module-before':
            if struct.unpack_from('<I',ee,MARKER+16)[0]!=0x1FFECA0 or struct.unpack_from('<I',ee,0x3ACA00)[0]!=0 or ee[0x3ACA08:0x3ACA22]!=b'cdrom0:\\IRX\\SIO2MAN.IRX;1\0':raise ValueError('LOADFILE first module request differs')
        else:
            if struct.unpack_from('<3I',ee,MARKER+8)!=(0,0,0x1FFEC50):raise ValueError('LOADFILE return/SP differs')
            expected=(0,0,0x80000008,0,0,0x203A97C0,0,0x3ACC00,0x80000009,0x22568,0x225B0,0,0,0,0,0) if point=='loadfile-bind-after' else (0x400,0x3ACA00,0x80000008,0,0,0,0,0x3ACC00,0x8000000A,0,0,0,0,0,0,0)
            if struct.unpack_from('<16I',ee,0x3A94C0)!=expected:raise ValueError('LOADFILE reply differs')
            replies.append(ee[0x3A94C0:0x3A9500])
            if point=='loadfile-version-after':
                if ee[0x3ACA00:0x3ACA04]!=b'3000':raise ValueError('LOADFILE version differs')
                replies.append(ee[0x3ACA00:0x3ACA04])
        manifests[point]=m
    return b''.join(replies),{'captures':manifests,'limitations':'Fresh bind and version query only; first requested module identified, no module ID/start success or IOP execution supplied.'}
