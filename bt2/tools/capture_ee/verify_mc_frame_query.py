"""Compare the first frame-phase MCSERV GetInfo (absent cards) with the original end callback."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_sound_rpc import words,require

POINT='rpc-mc-frame-callback'

def load(root,baseline):
    f=root/'work/pcsx2-capture'/(POINT+'-absent-cards');m=json.loads((f/'capture.json').read_text(encoding='utf-8'));ee=(f/'ee-ram.bin').read_bytes()
    a,trap,marker=capture_trap(POINT);stub=texture_stub(POINT)
    require(len(ee)==32*1024*1024 and m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'],'Frame card capture identity differs')
    require(all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Frame card capture environment differs')
    require(ee[a:a+len(trap)]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'Frame card capture hook/publication differs')
    return ee

def verify(root,native):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    ee=load(root,baseline)
    # Published by the halt stub: v0, SP, a0..a3, RA at the end-callback entry.
    published=words(ee,MARKER+8,9)
    require(published[4]==0x3A7480,'Original end-callback argument differs')
    query=words(ee,0x3A7000,12)
    require(query==(0,0,0,1,1,1,0,0x3A7480,0,0,0,0),'Original frame-phase GetInfo input differs')
    client=words(ee,0x3A6F80,10)
    require(client[2]==0xFFFFFFFF and client[7]==0x31DD60 and client[8]==0x3A7480,'Original no-wait endpoint differs')
    result=struct.unpack('<i',ee[0x3A8540:0x3A8544])[0];info=words(ee,0x3A7480,2)
    require(result==-11 and info==(0,0),'Original absent-card GetInfo result differs')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'));n=Path(str(native)+'.ee.bin').read_bytes()
        require(r.get('mc_frame_queries',0)>=1 and r.get('native_card_requests',0)==2+r['mc_frame_queries'],'Native frame-phase card query missing')
        require(words(n,0x3A7000,12)==query,'Native frame-phase GetInfo input differs')
        require(struct.unpack('<i',n[0x3A8540:0x3A8544])[0]==result and words(n,0x3A7480,2)==info,'Native absent-card GetInfo result differs')
    print('Frame-phase absent-card GetInfo input, end callback and result (-11, type 0, free 0) verified; per-query timing unverified.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
