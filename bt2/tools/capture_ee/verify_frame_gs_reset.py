"""Compare the frame-phase GS reset (ResetGraph(0,1,2,0) from 0x24B800) with the original at its return.

The original is captured interactively (a person answers the no-memory-card prompt with Yes), halting at
the return address 0x24B81C. The native side is the phase stop at the same return (BT2_PHASE_STOP=24b81c).
"""
import argparse,hashlib,json
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_sound_rpc import words,require

POINT='rpc-frame-gs-reset-after'
GS_STATE=0x368090 # GS library state block (FUN_0030FF50): interlace, output mode, field mode, revision, VSync handler.


def load(root,baseline):
    f=root/'work/pcsx2-capture'/(POINT+'-absent-cards');m=json.loads((f/'capture.json').read_text(encoding='utf-8'));ee=(f/'ee-ram.bin').read_bytes()
    a,trap,marker=capture_trap(POINT);stub=texture_stub(POINT)
    require(len(ee)==32*1024*1024 and m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'],'Frame GS reset capture identity differs')
    require(all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Frame GS reset capture environment differs')
    require(ee[a:a+len(trap)]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'Frame GS reset capture hook/publication differs')
    return ee


def state(ee):
    # Halfwords: interlace, output mode, field mode, GS revision; words: VSync handler flag and ID.
    half=[int.from_bytes(ee[GS_STATE+2*n:GS_STATE+2*n+2],'little') for n in range(4)]
    return half,words(ee,GS_STATE+8,2)


def verify(root,native):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    ee=load(root,baseline)
    half,handler=state(ee)
    require(half[:2]==[1,2] and half[3]==0x1B,'Original GS reset mode/revision differs')
    require(handler==(0,0),'Original reset left a VSync handler registered')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'));n=Path(str(native)+'.ee.bin').read_bytes()
        require(r.get('frame_gs_resets')==1 and r.get('frame_gs_reset_step')==0 and r.get('frame_gs_crt_request')==[1,2,0],'Native frame GS reset sequence differs')
        require(r.get('pc')==0x24B81C,'Native report is not at the reset return (BT2_PHASE_STOP=24b81c)')
        require(state(n)==(half,handler),'Native GS library state differs from the original')
    print('Frame-phase GS reset verified: interlace/NTSC/field mode, GS revision and cleared VSync handler match the original at the return.')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
