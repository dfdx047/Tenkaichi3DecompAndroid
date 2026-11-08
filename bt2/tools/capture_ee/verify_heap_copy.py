"""Verify the first heap copy and completed DMA poll; no hardware timing claim."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
from verify_heap_allocation import verify as verify_allocation

def verify(root,folder,disc,output,native=None):
    def require(ok,message):
        if not ok:raise ValueError(message)
    verify_allocation(root,folder.parent/'heap-alloc-after',disc,output)
    metadata=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes();iop=(folder/'iop-ram.bin').read_bytes()
    previous=json.loads((folder.parent/'heap-alloc-after/capture.json').read_text(encoding='utf-8'))
    require(len(ee)==32*1024*1024 and len(iop)==2*1024*1024,'RAM extent differs')
    require(metadata['source_elf_sha256']==EXPECTED_SHA and metadata['capture_point'].startswith('heap-copy-after:'),'Identity/point differs')
    require(all(metadata[k]==previous[k] for k in ['pcsx2_sha256','bios_sha256','disc_sha256']),'Environment differs')
    require(hashlib.sha256(ee).hexdigest()==metadata['capture_sha256'] and hashlib.sha256(iop).hexdigest()==metadata['iop_capture_sha256'],'RAM hash differs')
    address,trap,marker=capture_trap('heap-copy-after')
    require(ee[address:address+len(trap)]==trap and struct.unpack_from('<I',ee,MARKER)[0]==marker,'Copy trap differs')
    require(struct.unpack_from('<3I',ee,MARKER+8)==(0xFFFFFFFF,0xFFFFFFFF,0x1FFECD0),'Completed poll/stack differs')
    descriptor=(0x369F90,0xCE500,0x360,0)
    require(struct.unpack_from('<4I',ee,0x1FFECD0)==descriptor,'Copy descriptor differs')
    source=ee[descriptor[0]:descriptor[0]+descriptor[2]]
    require(source==iop[descriptor[1]:descriptor[1]+descriptor[2]] and source[:4]==b'\x7fELF','Destination/payload differs')
    # Fresh prepared ELF image independently proves this source has no runtime mutation.
    require(source==(root/'work/bt2-generation/guest-memory.bin').read_bytes()[descriptor[0]:descriptor[0]+descriptor[2]],'Prepared source differs')
    if native:
        report=json.loads(native.read_text(encoding='utf-8'));ram=Path(str(native)+'.iop.bin').read_bytes()
        require(report['heap_binds']==report['heap_calls']==report['heap_transfers']==report['heap_dma_polls']==1,'Native heap phases differ')
        require(report['heap_client_words']==[0,23,7,0,0,0x48480,0x38BDF0,0,0,0x48438],'Native heap client differs')
        require(len(ram)==2*1024*1024 and ram[descriptor[1]:descriptor[1]+descriptor[2]]==source,'Native transferred payload differs')
    output.write_bytes(output.read_bytes()+struct.pack('<5I',*descriptor,0xFFFFFFFF))
    output.with_suffix('.json').write_text(json.dumps({'verified':'First owned EE-to-IOP heap copy and completion boundary','source_sha256':hashlib.sha256(source).hexdigest(),'capture':metadata,'native_compared':bool(native),'limitations':'Synchronous logical copy only; no physical DMA scheduling, token equality or general transfer/loader behavior.'},indent=2)+'\n',encoding='utf-8')
    print('Heap copy source/destination and completion verified'+(' against native RAM.' if native else '.'))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--capture',type=Path,required=True);p.add_argument('--disc',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--native',type=Path)
    a=p.parse_args();verify(a.root,a.capture,a.disc,a.output,a.native)
