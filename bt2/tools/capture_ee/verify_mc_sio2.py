"""Verify initial card slice programming; the native run is checked only for EE-side query completion."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from capture_ee import EXPECTED_SHA
from capture_sio2_dma import SCRATCH,CODE,IMPORT,hook,pack
from verify_sound_rpc import require,words

def verify(root,native=None):
    folder=root/'work/pcsx2-capture/rpc-mc-query-before-sio2-slice'
    m=json.loads((folder/'sio2-slice.json').read_text(encoding='utf-8'));ram=(folder/'sio2-slice-iop.bin').read_bytes()
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    require(m['source_elf_sha256']==EXPECTED_SHA and all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Slice capture environment differs')
    require(hashlib.sha256(ram).hexdigest()==m['iop_sha256'],'Slice RAM hash differs')
    channel,source,size,count,direction=m['arguments']
    require((channel,size,count,direction)==(11,36,1,1) and m['return']==1 and m['registers']==[source,0x10024,0x201],'Slice API/register result differs')
    payload=bytes([0x81,0x11])+bytes(142)
    require(ram[source:source+144]==payload and hashlib.sha256(payload).hexdigest()==m['payload_sha256'],'Slice actual source differs')
    require(ram[CODE:CODE+len(hook(m['target']))]==hook(m['target']) and ram[IMPORT:IMPORT+8]==pack([0x08000000|(CODE>>2),0]),'Slice hook differs')
    require(words(ram,SCRATCH,7)==(1,channel,source,size,count,direction,1),'Slice publication differs')
    sys.path.insert(0,str(root/'tools/iop'));from inspect_irx import inspect,relocate
    module=(root/'rom/IRX/MCMAN.IRX').read_bytes();meta=inspect(module);base=source-0x12020;image=relocate(module,meta,base)
    require(all(ram[base+a:base+b]==image[a:b] for a,b in meta['code_ranges']),'Slice source module ownership differs')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'))
        from verify_sound_rpc import completed_card_query
        require(completed_card_query(r),'Native completed card query differs') # The native card service has no SIO2 transfer state.
    print('Actual initial card slice API, source and register readbacks verified; the native card service completes the EE query.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
