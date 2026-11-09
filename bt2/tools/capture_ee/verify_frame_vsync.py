"""Compare the first frame's one-shot VSync stack publication; timing is logical."""
import argparse,json,struct
from pathlib import Path
from verify_vblank_wake import original
from verify_sound_rpc import require

def verify(root,native):
    before=original(root,'rpc-vsync-flags-after',{8:0x1006D8,16:0x1FFECA0,24:0x1FFECA0,28:0x1FFECA8,48:0x38BDF0})
    after=original(root,'rpc-frame-vsync-after',{16:0x1FFECC0,48:0x38BDF0})
    require(struct.unpack_from('<I',before,0x1FFECA0)[0]==0,'Original registration flag differs')
    flag,csr=struct.unpack_from('<I4xQ',after,0x1FFECA0)
    require(flag==1 and csr==struct.unpack_from('<Q',after,0x387E88)[0],'Original delivery differs')
    r=json.loads(native.read_text(encoding='utf-8'));ee=Path(str(native)+'.ee.bin').read_bytes()
    require(r['frame_vsync_deliveries']>=1 and r['timer1_stops']>=1,'Native frame/timer scope differs')
    native_flag,native_csr=struct.unpack_from('<I4xQ',ee,0x1FFECA0)
    require(native_flag==1 and (native_csr&~0x2000)==(csr&~0x2000),'Native one-shot publication differs')
    print('Verified first-frame VSync flag/CSR publication (FIELD phase and physical timing excluded).')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args()
    verify(Path(__file__).resolve().parents[2],a.native)
