"""Validate the original 0x40C request and its current native preemption blocker."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub

def require(ok,message):
    if not ok:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)
def verify(root,native):
    captured={}
    for point in ('rpc-cri-third-before','rpc-cri-fourth-before'):
        folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
        m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
        require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'],'CRI command capture identity differs')
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(ee[address:address+8]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'CRI command hook differs')
        captured[point]=ee
    before=captured['rpc-cri-third-before'];after=captured['rpc-cri-fourth-before']
    require(words(before,MARKER+24,9)==(0x355C70,0x40C,0,0x39DC80,12,0x39DD80,12,0,0x2FD1F8),'Retail 0x40C request differs')
    require(words(before,0x39DC80,3)==(0xBB80,0xFFFF,0xFFFF),'Retail 0x40C input differs')
    require(words(after,0x39DD80,3)==(0,1,0) and words(after,MARKER+28,1)==(0x422,),'Retail subsequent command/result differs')
    r=json.loads(native.read_text(encoding='utf-8'));ee=Path(str(native)+'.ee.bin').read_bytes();iop=Path(str(native)+'.iop.bin').read_bytes()
    require(r['selector']==0x77 and r['pc']==0x32F028 and r['cri_rpc_calls']==2,'Native 0x40C boundary differs')
    if r['native_iop_error']=='Higher-priority nested IOP start needs preemption':
        require(r['native_iop_pc']==0x1745F0,'Historical preemption rejection differs')
    else:
        from capture_cri_thread import START,WAIT,SCRATCH,CODE,start_hook,wait_hook,dma_hook,adma_hook
        env=None
        for mode in ('start','dma','adma'):
            folder=root/'work/pcsx2-capture'/('rpc-cri-third-before-absent-cards-cri-thread-'+mode)
            m=json.loads((folder/'cri-thread.json').read_text(encoding='utf-8'));ram=(folder/'cri-thread-iop.bin').read_bytes()
            image=(folder/'cri-thread-ee.bin').read_bytes()
            require(m['mode']==mode and m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ram).hexdigest()==m['iop_sha256']
                and hashlib.sha256(image).hexdigest()==m['ee_sha256'],'CRI thread capture identity differs')
            current=[m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]
            if env is None:env=current
            require(current==env,'CRI thread capture environment differs')
            require(words(ram,START,2)==(0x08000000|(CODE>>2),0) and words(ram,WAIT,2)==(0x08000000|((CODE+0x100)>>2),0),'CRI API observer differs')
            original_start=start_hook(m['targets'][0],mode=='start');original_wait=wait_hook(m['targets'][1])
            require(ram[CODE:CODE+len(original_start)]==original_start and ram[CODE+0x100:CODE+0x100+len(original_wait)]==original_wait,'CRI original-call observer differs')
            v=m['values'];require(words(ram,SCRATCH,len(v))==tuple(v),'CRI publication differs')
            require(v[0]==1 and v[1]!=0 and v[2]==0 and v[3]==0xBD94C and v[4]==v[8]==v[11]==0xCDD20
                and v[5]==v[12] and v[6]!=0 and v[7]==0xBD800 and v[10]==0,'Original parent/first-wait context differs')
            if mode=='dma':require(v[13:19]==[0,0xBF8010C8,0,0xC000,0xC6EF0,0x1000],'Original initial DMA request differs')
            if mode=='adma':require(v[13:15]==[1,0xBF9001B0],'Original AutoDMA enable differs')
        require(r['native_iop_error']=='Unimplemented IOP memory/hardware access' and r['native_iop_pc']==0x92DB8
            and r['native_iop_blocked_address']==0x1F9001B0 and r['native_iop_access_width']==2
            and r['native_iop_access_kind']=='write' and r['native_iop_access_value']==1,'Native AutoDMA rejection differs')
        child=r['cri_thread_words'];require(child[1:9]==[0x16D890,0x17DDF0,0x800,39,0x2000000,0,1,0]
            and child[9]!=0 and child[10:12]==[0x17463C,0x16D8D0] and words(iop,0x175A84,1)==(child[9],),'Native child did not park at original first wait')
        require(r['spu2_dma_idle_reads']==1,'Native initial DMA-control observation differs')
        require(iop[0x176FC0:0x177FC0]==ram[0xC6EF0:0xC7EF0]==bytes(4096),'Observed initial CRI source buffer differs')
    require(r['indexed_cache_tag_reads']==256 and r['indexed_cache_operations']==256,'Native prior cache scans incomplete')
    require(words(ee,0x39DC80,3)==(0xBB80,0xFFFF,0xFFFF) and words(iop,0x1767C0,3)==(0xBB80,0xFFFF,0xFFFF),'Original handler input transfer differs')
    require(r['packet_words'][6:12]==[56,0x355C70,0x40C,12,0x39DD80,12],'Native request ABI differs')
    print('Verified original CRI request, parent return/child wait, initial DMA control and reached AutoDMA boundary; no native 0x40C completion claimed.')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',required=True,type=Path);args=parser.parse_args()
    verify(Path(__file__).resolve().parents[2],args.native)
