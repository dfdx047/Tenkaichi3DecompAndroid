"""Verify owned CRI bind/create against independent local retail captures."""
import argparse,json,hashlib,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub

def require(ok,message):
    if not ok:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)
def verify(root,native):
    observations={};baseline=None
    for point in ('rpc-cri-bind-sema','rpc-cri-bind-after','rpc-cri-command-before','rpc-cri-command-after','rpc-cri-second-before','rpc-cri-second-after','rpc-cri-call-sema'):
        folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
        m=json.loads((folder/'capture.json').read_text(encoding='utf-8'))
        ee=(folder/'ee-ram.bin').read_bytes();iop=(folder/'iop-ram.bin').read_bytes()
        require(m['source_elf_sha256']==EXPECTED_SHA and len(ee)==32*1024*1024 and len(iop)==2*1024*1024,'CRI capture identity/size differs')
        require(hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and hashlib.sha256(iop).hexdigest()==m['iop_capture_sha256'],'CRI capture hashes differ')
        env=[m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]
        if baseline is None:baseline=env
        require(env==baseline and m['capture_point'].startswith(point+':'),'CRI capture environment differs')
        ini=(folder/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8')
        require('Slot1_Enable = false' in ini and 'Slot2_Enable = false' in ini,'CRI card profile differs')
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(ee[address:address+len(trap)]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'CRI hook differs')
        observations[point]=(ee,iop)
    b,_=observations['rpc-cri-bind-sema'];sp=words(b,MARKER+16,1)[0];d=words(b,sp,6)
    require(sp==0x1FFEC00 and words(b,MARKER+8,2)==(10,0) and (d[1],d[2],d[5])==(1,0,0x3838D8),'CRI bind semaphore differs')
    b,_=observations['rpc-cri-bind-after']
    require(words(b,MARKER+8,2)==(0,0) and words(b,0x355C70,10)==(0,52,10,0,0,0xC66F0,0,0,0,0xC6C60),'CRI retail bind differs')
    b,_=observations['rpc-cri-command-before']
    require(words(b,MARKER+24,9)==(0x355C70,2,0,0x39DC80,16,0x39DD80,4,0,0x2FCB20),'CRI first command ABI differs')
    require(words(b,0x39DC80,4)==(0,0x365BC0,0x14FD40,0x880),'CRI first input differs')
    b,iop=observations['rpc-cri-command-after']
    require(words(b,MARKER+8,2)==(0,0) and words(b,0x39DD80,1)==(0xC67F0,),'CRI retail create return differs')
    b,_=observations['rpc-cri-second-before']
    require(words(b,MARKER+24,9)==(0x355C70,0x403,0,0x39DC80,0,0x39DD80,0,0,0x2FD1F8) and words(b,MARKER+60,1)==(2,),'CRI second ABI differs')
    b,_=observations['rpc-cri-second-after'];require(words(b,MARKER+8,2)==(0,0),'CRI control return differs')
    b,_=observations['rpc-cri-call-sema'];sp=words(b,MARKER+16,1)[0];d=words(b,sp,6)
    require(sp==0x1FFEB60 and (d[1],d[2],d[5])==(1,0,0x3838E8),'CRI call semaphore differs')
    r=json.loads(native.read_text(encoding='utf-8'))
    require(r.get('cri_rpc_binds')==1 and r.get('cri_rpc_calls') in (1,2) and r.get('native_iop_error')=='','Native CRI lifecycle differs')
    ee=Path(str(native)+'.ee.bin').read_bytes();ni=Path(str(native)+'.iop.bin').read_bytes()
    pending_third=(r.get('cri_rpc_calls')==2 and r.get('selector')==0x40 and r.get('pc')==0x32EC68
        and r.get('packet_words',[0]*9)[6:12]==[56,0x355C70,0x40C,12,0x39DD80,12])
    require(words(ee,0x355C70,1)==((0x203A97C0 if pending_third else 0),) and words(ee,0x355C84,1)==(0x1767C0,) and words(ee,0x355C94,1)==(0x176D30,),'Native CRI bind ownership differs')
    require(words(ee,0x39DD80,1)==(0x1768C0,),'Native CRI slot differs')
    # Compare stable slot fields; internal function pointers relocate with the module.
    retail=list(words(iop,0xC67F0,12));actual=list(words(ni,0x1768C0,12))
    for n in (8,10):retail[n]+=0x168000-0xB7F30
    require(actual==retail,'Native original CRI slot fields differ')
    print('Verified original CRI bind, command 2 allocation, owned slot and EE cleanup.')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',type=Path,required=True);args=parser.parse_args()
    verify(Path(__file__).resolve().parents[2],args.native)
