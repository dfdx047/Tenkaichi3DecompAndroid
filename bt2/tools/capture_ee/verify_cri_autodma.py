"""Validate initial CRI loop programming/API return against original local execution."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from capture_cri_thread import BASE,START,WAIT,SCRATCH,CODE,start_hook,wait_hook,program_hook,transfer_hook,irq_hook,volume_hook

def require(ok,message):
    if not ok:raise ValueError(message)
def words(b,a,n):return struct.unpack_from('<'+str(n)+'I',b,a)
def verify(root,native):
    observations={};environment=None
    for mode,off,observer in [('program',0x2E38,program_hook),('transfer',None,None),('irq',0x27E8,irq_hook),('volume',0x36E4,volume_hook)]:
        f=root/'work/pcsx2-capture'/('rpc-cri-third-before-absent-cards-cri-thread-'+mode)
        m=json.loads((f/'cri-thread.json').read_text(encoding='utf-8'));ram=(f/'cri-thread-iop.bin').read_bytes();ee=(f/'cri-thread-ee.bin').read_bytes()
        require(m['mode']==mode and m['source_elf_sha256']==EXPECTED_SHA and len(ram)==2*1024*1024 and len(ee)==32*1024*1024,'AutoDMA capture identity differs')
        require(hashlib.sha256(ram).hexdigest()==m['iop_sha256'] and hashlib.sha256(ee).hexdigest()==m['ee_sha256'],'AutoDMA capture hashes differ')
        current=[m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]
        if environment is None:environment=current
        require(current==environment,'AutoDMA capture environment differs')
        for address,code in [(START,start_hook(m['targets'][0],False)),(WAIT,wait_hook(m['targets'][1]))]:
            dest=CODE if address==START else CODE+0x100
            require(words(ram,address,2)==(0x08000000|(dest>>2),0) and ram[dest:dest+len(code)]==code,'Original thread observer differs')
        address=BASE+0xC434 if mode=='transfer' else 0x82C30+off
        code=transfer_hook(m['targets'][2]) if mode=='transfer' else observer()
        require(words(ram,address,2)==(0x08000000|((CODE+0x140)>>2),0) and ram[CODE+0x140:CODE+0x140+len(code)]==code,'AutoDMA observer differs')
        v=m['values'];require(words(ram,SCRATCH,len(v))==tuple(v) and v[:13]==[1,0xFA0037,0,0xBD94C,0xCDD20,0x1EFCE8,0xFA0E15,0xBD800,0xCDD20,0x1EEDB8,0,0xCDD20,0x1EFCE8],'Original first-wait/parent publication differs')
        observations[mode]=(v,ram)
    v,ram=observations['program']
    require(v[13:]==[0x1000201,0xC6EF0,0x800,0x800,0x859EC,0xC6EF0,0x200010,0x200B31E1],'Actual initial DMA programming differs')
    require((v[19]>>16)*(v[19]&0xFFFF)*4==2048 and ram[0xC6EF0:0xC7EF0]==bytes(4096),'Initial buffer/word sizing differs')
    v,_=observations['transfer'];require(v[13:]==[0,0x10,0xC6EF0,0x1000,0xBDDD8,0xCDD20,0x1EFCB8,0x800,0xCDD20,0x1EFCB8],'Original block-transfer arguments/return differ')
    v,_=observations['irq'];require(v[13]==0x87F58 and v[17:]==[0xC76F0,0x200010,0x201,0x1600],'Original first DMA interrupt entry differs')
    v,_=observations['volume'];require(v[13:]==[2,0xBF90076C,0x7FFF,0,0xBDE10,0xBF90076E,0x7FFF,0,0xBDE2C],'Actual direct-input parameter stores differ')
    captures={}
    for point in ('rpc-cri-fourth-before','rpc-cri-fifth-before','rpc-cri-sixth-before'):
        f=root/'work/pcsx2-capture'/(point+'-absent-cards');m=json.loads((f/'capture.json').read_text(encoding='utf-8'));ee=(f/'ee-ram.bin').read_bytes()
        a,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and ee[a:a+8]==trap
            and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'Following CRI capture differs')
        require([m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]==environment,'Following capture environment differs')
        captures[point]=ee
    b=captures['rpc-cri-fourth-before']
    require(words(b,0x39DD80,3)==(0,1,0) and words(b,MARKER+24,9)==(0x355C70,0x422,0,0x39DC80,12,0x39DD80,4,0,0x2FD1F8),'Retail 0x40C result/next ABI differs')
    require(words(b,0x3AC8C0,1)==(0x41040,) and words(b,0x3AC880,1)==(0x150600,) and words(b,0x39DC80,3)==(1,0x150640,0x100),'Fourth heap/0x422 input differs')
    b=captures['rpc-cri-fifth-before'];require(words(b,0x39DD80,1)==(0xCC970,) and words(b,MARKER+24,9)==(0x355C70,0x400,0,0x39DC80,16,0x39DD80,4,0,0x2FD1F8),'Retail 0x422 result/next ABI differs')
    b=captures['rpc-cri-sixth-before']
    require(words(b,0x39DD80,1)==(0xCD4B0,) and words(b,0x39DC80,3)==(1,0x154740,0x100),'First 0x400 result/second 0x422 input differs')
    import capture_ee as capture_module
    batches={}
    for index in (10,36):
        f=root/'work/pcsx2-capture'/('rpc-cri-nth-before-absent-cards-'+str(index))
        m=json.loads((f/'capture.json').read_text(encoding='utf-8'));ee=(f/'ee-ram.bin').read_bytes();ram=(f/'iop-ram.bin').read_bytes()
        previous=capture_module.CRI_CALL_INDEX;capture_module.CRI_CALL_INDEX=index
        try:stub=texture_stub('rpc-cri-nth-before')
        finally:capture_module.CRI_CALL_INDEX=previous
        a,trap,marker=capture_trap('rpc-cri-nth-before')
        require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256']
            and hashlib.sha256(ram).hexdigest()==m['iop_capture_sha256'] and ee[a:a+8]==trap
            and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,)
            and words(ee,MARKER+60,1)==(index,),'Indexed original CRI capture differs')
        require([m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]==environment,'Indexed CRI environment differs')
        batches[index]=(ee,ram)
    b,_=batches[36]
    require(words(b,MARKER+24,9)==(0x355C70,2,0,0x39DC80,16,0x39DD80,4,0,0x2FCB20)
        and words(b,0x39DC80,4)==(1,0x35FD00,0x191740,0x880) and words(b,0x39DD80,1)==(0xCD5DC,),
        'Original sixteen-pair batch/next channel handoff differs')
    require(words(b,0x3AC8C0,1)==(0x8D0,) and words(b,0x3AC880,1)==(0x191700,),'Next retail heap differs')
    r=json.loads(native.read_text(encoding='utf-8'));ne=Path(str(native)+'.ee.bin').read_bytes()
    # Later phases add the game's own cache scans (whole 128-line passes, one or both ways).
    require(r['cri_rpc_calls']>=3 and r['indexed_cache_operations']==r['indexed_cache_tag_reads'] and (r['indexed_cache_operations']==(512+256*r.get('cri_stream_transfers',0) if r['cri_rpc_calls']>=36 else 256) or (r.get('load_poll_idles',0) and r['indexed_cache_operations']>=512 and r['indexed_cache_operations']%128==0)),'Native prior commands incomplete')
    base=0x1A7000 # Native CRI module base; its server handles are fixed host constants (no IOP module state).
    require(words(ne,0x355C84,1)==(base+0xE7C0,) and words(ne,0x355C94,1)==(base+0xED30,),'Native CRI client ownership differs')
    if r['cri_rpc_calls']==3:require(words(ne,0x39DD80,3)==(0,1,0) and words(ne,0x355C70,1)==(0,),'Native original 0x40C result/cleanup differs')
    if r['cri_rpc_calls']==4:require(r['heap_calls']==4 and words(ne,0x3AC880,1)==(0x150600,) and words(ne,0x39DD80,1)==(base+0x14A40,),'Native fourth heap/0x422 result differs')
    if r['cri_rpc_calls']>=35:
        require(r['heap_calls']>=4, 'Native first-channel heap incomplete')
        if r['cri_rpc_calls']==35:require(words(ne,0x3AC880,1)==(0x150600,) and words(ne,0x39DD80,1)==(base+0x15580+15*0x14,), 'Native first-channel last return differs')
        require(words(ne,0x355C70,1)==(0,) or (r['selector']==0x77 and words(ne,0x3A97DC,1)==(0x355C70,)),'Native full batch cleanup incomplete') # Or the next CRI submission.
    print('Verified original AutoDMA setup/API return, constructor batch and owned cleanup. Native CRI is a host service: IOP constructor objects, DMA, IRQ/refill and playback are not compared.')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args();verify(Path(__file__).resolve().parents[2],a.native)
