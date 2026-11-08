"""Validate fresh CD service captures; proprietary RAM remains local."""
import hashlib,json,struct
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,fresh_bind_stub
SCMD_CALLER_SHA='7c870c275aab738fedf09c35f92d5aadd38c91965c79e32974ae89590a57b4b9'
CALLER_SHA='2849705860af8c8f510c3ec74b6e88f2e4dbc48fe481c6018d94ed6356daca88'
POINTS=('post-cd-bind-after','post-cd-bind-success','post-cd-after')
def verify(folder,environment):
    ram={};manifests={}
    for point in POINTS:
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'))
        ee=path.with_name('ee-ram.bin').read_bytes();iop=path.with_name('iop-ram.bin').read_bytes()
        address,trap,marker=capture_trap(point);p=m['patch']
        if len(ee)!=32*1024*1024 or len(iop)!=2*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap) or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap)) or p['marker_address']!=MARKER or p['marker_value']!=marker or ee[address:address+len(trap)]!=trap or struct.unpack_from('<I',ee,MARKER)[0]!=marker:
            raise ValueError('Fresh CD trap/identity differs')
        if hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or hashlib.sha256(iop).hexdigest()!=m['iop_capture_sha256']:raise ValueError('Fresh CD RAM hash differs')
        if any(m.get(k)!=environment[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')):raise ValueError('Fresh CD reference environment differs')
        if hashlib.sha256(ee[0x1003E8:0x1003F0]).hexdigest()!=CALLER_SHA:raise ValueError('Fresh CD unique caller differs')
        if point!='post-cd-after':
            stub=fresh_bind_stub(point.endswith('success'))
            if ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or m['authored_extra_segment']!={'address':FRESH_BIND_STUB,'words':list(struct.unpack('<'+str(len(stub)//4)+'I',stub))}:raise ValueError('Fresh CD conditional stub differs')
        expected=(1,0,0x1FFED40) if point=='post-cd-after' else (0,0,0x1FFEC90)
        if struct.unpack_from('<3I',ee,MARKER+8)!=expected:raise ValueError('Fresh CD return/SP differs')
        if struct.unpack_from('<4I',iop,0x1E700)!=(0x1E640,0x1E6C0,0x3A94C0,0x1E730):raise ValueError('Fresh CD transport ownership differs')
        if struct.unpack_from('<3I',iop,0x43388)!=(0x80000592,0x3E36C,0x441B8) or struct.unpack_from('<I',iop,0x43388+64)[0]!=0x43358 or struct.unpack_from('<I',iop,0x43358+8)[0]!=0x43388:raise ValueError('Fresh CD service/queue differs')
        server=0 if point=='post-cd-bind-after' else 0x43388;buffer=0 if not server else 0x441B8
        if struct.unpack_from('<I',ee,0x3A87A8)[0]!=0 or struct.unpack_from('<I',ee,0x3A87A8+36)[0]!=server or struct.unpack_from('<I',ee,0x3A87A8+20)[0]!=buffer:raise ValueError('Fresh CD client differs')
        reply=struct.unpack_from('<16I',ee,0x3A94C0)
        if point=='post-cd-after':
            if reply!=(0x1000,0x3695C0,0x80000008,0,0,0,0,0x3A87A8,0x8000000A,0,0,0,0,0,0,0) or struct.unpack_from('<4I',ee,0x3695C0)!=(1,0x225,0x225,0):raise ValueError('Fresh CD call result differs')
        elif reply!=(0,0,0x80000008,0,0,0x203A97C0,0,0x3A87A8,0x80000009,server,buffer,0,0,0,0,0):raise ValueError('Fresh CD bind reply differs')
        ram[point]=ee;manifests[point]=m
    bundle=b''.join(ram[p][0x3A94C0:0x3A9500] for p in POINTS)+ram['post-cd-after'][0x3695C0:0x3695D0]
    return bundle,{'captures':manifests,'limitations':'One logical not-ready bind, later successful bind and CD-init reply. Physical retry counts/timing are not replayed; no native IOP boot/service instructions or general disc I/O.'}
def verify_scmd(folder,environment):
    replies=[];manifests={}
    for point in ('post-scmd-bind-success','post-mmode-after'):
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'));ee=path.with_name('ee-ram.bin').read_bytes();iop=path.with_name('iop-ram.bin').read_bytes()
        addr,trap,marker=capture_trap(point);p=m['patch'];bind=point=='post-scmd-bind-success'
        if len(ee)!=32*1024*1024 or len(iop)!=2*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA or p['address']!=addr or p['size']!=len(trap) or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap)) or ee[addr:addr+len(trap)]!=trap or struct.unpack_from('<I',ee,MARKER)[0]!=marker or p['marker_value']!=marker or p['marker_address']!=MARKER or not m['capture_point'].startswith(point+':'):raise ValueError('Fresh SCMD identity/trap differs')
        if hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or hashlib.sha256(iop).hexdigest()!=m['iop_capture_sha256'] or any(m.get(k)!=environment[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')):raise ValueError('Fresh SCMD hash/environment differs')
        if hashlib.sha256(ee[0x1003F0:0x1003F8]).hexdigest()!=SCMD_CALLER_SHA:raise ValueError('Fresh SCMD caller differs')
        if bind:
            stub=fresh_bind_stub(True,True)
            if ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or m['authored_extra_segment']!={'address':FRESH_BIND_STUB,'words':list(struct.unpack('<'+str(len(stub)//4)+'I',stub))}:raise ValueError('Fresh SCMD conditional stub differs')
        if struct.unpack_from('<3I',ee,MARKER+8)!=((0,0,0x1FFECB0) if bind else (1,0,0x1FFED40)):raise ValueError('Fresh SCMD publication differs')
        if struct.unpack_from('<4I',iop,0x1E700)!=(0x1E640,0x1E6C0,0x3A94C0,0x1E730) or struct.unpack_from('<3I',iop,0x434F0)!=(0x80000593,0x41BC8,0x439A8) or struct.unpack_from('<I',iop,0x434F0+64)[0]!=0x43358 or struct.unpack_from('<I',iop,0x43388+56)[0]!=0x43460 or struct.unpack_from('<I',iop,0x43460+56)[0]!=0x434F0:raise ValueError('Fresh SCMD service ownership differs')
        client=struct.unpack_from('<10I',ee,0x369F40)
        if client[0]!=0 or client[2]!=7 or client[5]!=0x439A8 or client[9]!=0x434F0:raise ValueError('Fresh SCMD client differs')
        reply=struct.unpack_from('<16I',ee,0x3A94C0)
        expected=(0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x434F0,0x439A8,0,0,0,0,0) if bind else (0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0)
        if reply!=expected:raise ValueError('Fresh SCMD reply differs')
        if not bind and (struct.unpack_from('<I',ee,0x3695C0)[0]!=1 or struct.unpack_from('<I',ee,0x369A00)[0]!=2 or struct.unpack_from('<I',iop,0x439A8)[0]!=2):raise ValueError('Fresh SCMD media-mode effect differs')
        replies.append(ee[0x3A94C0:0x3A9500]);manifests[point]=m
    return b''.join(replies)+struct.pack('<I',1),{'captures':manifests,'limitations':'Fresh service bind and one mode-2 request/reply; no general CDVD backend or physical timing.'}
