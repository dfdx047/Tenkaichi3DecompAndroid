"""Validate the unique local VIF setup return/configuration; no VU execution claim."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,vif_startup_stub,graph_startup_stub,graph_csr_stub

def verify(root,native=None):
    folder=root/'work/pcsx2-capture/vif-startup-after';m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
    previous=json.loads((folder.parent/'dmac-startup-after/capture.json').read_text(encoding='utf-8'))
    if len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA or not m['capture_point'].startswith('vif-startup-after:') or hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or any(m[k]!=previous[k] for k in ['pcsx2_sha256','bios_sha256','disc_sha256']):raise ValueError('VIF capture identity/environment differs')
    address,trap,marker=capture_trap('vif-startup-after');stub=vif_startup_stub()
    if ee[address:address+len(trap)]!=trap or ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or struct.unpack_from('<I',ee,MARKER)[0]!=marker or struct.unpack_from('<I',ee,MARKER+16)[0]!=0x1FFED40:raise ValueError('VIF hook/publication differs')
    if ee[0x3680A0:0x3680C0]!=(root/'work/bt2-generation/guest-memory.bin').read_bytes()[0x3680A0:0x3680C0]:raise ValueError('VIF source data differs')
    registers=struct.unpack_from('<18I',ee,MARKER+24)
    if registers!=(0,0,2,0,0x404,0,0,0,0x04000000,0,0,0,0,0,0,0,0,0):raise ValueError('VIF/GIF/VU readback differs')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'))
        if [r[k] for k in ['vif_phase','vif_commands','vif_words','vif_fifo_writes','vu_resets','gif_resets']]!=[6,7,8,2,1,1] or r['vif_registers']!=[2,0x404,0,0,0x04000000,0,0,0,0]:raise ValueError('Native VIF configuration differs')
        if r.get('graph_startup_phase')==4:
            for point,stub_factory in [('graph-csr-after',graph_csr_stub),('graph-startup-after',graph_startup_stub)]:
                sub=folder.parent/point;sm=json.loads((sub/'capture.json').read_text(encoding='utf-8'));se=(sub/'ee-ram.bin').read_bytes()
                address,trap,marker=capture_trap(point);stub=stub_factory()
                if hashlib.sha256(se).hexdigest()!=sm['capture_sha256'] or any(m[k]!=sm[k] for k in ['source_elf_sha256','pcsx2_sha256','bios_sha256','disc_sha256']) or se[address:address+len(trap)]!=trap or se[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or struct.unpack_from('<I',se,MARKER)[0]!=marker:raise ValueError('Second graph setup capture differs')
                expected=(0x551B4000,0,0x1FFECF0) if point=='graph-csr-after' else (0,0,0x1FFED40)
                if struct.unpack_from('<3I',se,MARKER+8)!=expected:raise ValueError('Second graph setup return/read differs')
    print('VIF setup packets and independent register readbacks verified'+(' against native execution.' if native else '.'))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
