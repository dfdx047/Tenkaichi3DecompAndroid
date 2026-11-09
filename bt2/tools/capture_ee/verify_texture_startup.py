"""Validate user-local GIF configuration evidence and prepare an ignored probe bundle."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub

def verify(root,output=None,native=None):
    baseline=json.loads((root/'work/pcsx2-capture/graph-startup-after/capture.json').read_text(encoding='utf-8'))
    data=[];packet=None
    for point in ['texture-idle','texture-path-idle','texture-poll','texture-complete','texture-after']:
        folder=root/'work/pcsx2-capture'/point;m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        if len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA or hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or any(m[k]!=baseline[k] for k in ['pcsx2_sha256','bios_sha256','disc_sha256']):raise ValueError('Texture capture identity/environment differs')
        if not m['capture_point'].startswith(point+':') or ee[address:address+len(trap)]!=trap or ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or struct.unpack_from('<I',ee,MARKER)[0]!=marker:raise ValueError('Texture instrumentation differs')
        pub=ee[MARKER+8:MARKER+40];words=struct.unpack('<8I',pub)
        if words[2]!=(0x1FFED40 if point=='texture-after' else 0x1FFECE0):raise ValueError('Texture capture stack differs')
        current=ee[0x1FFECF0:0x1FFED30]
        if current!=ee[0x376908:0x376948] or (packet is not None and current!=packet):raise ValueError('Texture source packets differ')
        packet=current;data.append(pub)
    bundle=b''.join(data)+packet
    if output:output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(bundle)
    if native:
        r=json.loads(native.read_text(encoding='utf-8'));w=struct.unpack('<8Q',packet)
        if any(r.get('ee_pool_pointers',[])):
            point='ee-pools-after';folder=root/'work/pcsx2-capture'/point;m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes();address,trap,marker=capture_trap(point);stub=texture_stub(point)
            if hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or any(m[k]!=baseline[k] for k in ['source_elf_sha256','pcsx2_sha256','bios_sha256','disc_sha256']) or ee[address:address+len(trap)]!=trap or ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or struct.unpack_from('<I',ee,MARKER)[0]!=marker or r['ee_pool_pointers']!=list(struct.unpack_from('<4I',ee,0x38A628)):raise ValueError('Native EE pool pointers differ from independent return capture')
        if r.get('main_thread_priority')==2:
            point='main-priority-after';folder=root/'work/pcsx2-capture'/point;m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes();address,trap,marker=capture_trap(point);stub=texture_stub(point)
            if hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or any(m[k]!=baseline[k] for k in ['source_elf_sha256','pcsx2_sha256','bios_sha256','disc_sha256']) or ee[address:address+len(trap)]!=trap or ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or struct.unpack_from('<I',ee,MARKER)[0]!=marker or struct.unpack_from('<4I',ee,MARKER+8)!=(1,0,0x1FFED40,0):raise ValueError('Priority return capture differs')
        if [r[k] for k in ['texture_gif_phase','texture_gif_starts','texture_gif_completions','texture_gif_polls']]!=[8,1,1,2] or r['tex0']!=[w[4],w[6]] or r['pabe']!=(w[2]&1):raise ValueError('Native texture startup differs')
    print('Independent GIF configuration source/loads/return verified'+(' against native execution.' if native else '.'))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--output',type=Path);p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.output,a.native)
