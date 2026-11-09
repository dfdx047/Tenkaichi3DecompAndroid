"""Independent absent-card packet, owned transport and timed-wait verification."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from capture_ee import EXPECTED_SHA
from capture_sio2_start import BASE,SCRATCH,CODE,IMPORT
from capture_sio2_flow import hook,control_hook,send_hook,count_hook,fifo_hook,pack
from verify_sound_rpc import require,words

def verify(root,native):
    folder=root/'work/pcsx2-capture/rpc-mc-query-before-sio2-flow'
    m=json.loads((folder/'sio2-flow.json').read_text(encoding='utf-8'));ram=(folder/'sio2-flow-iop.bin').read_bytes()
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    require(m['source_elf_sha256']==EXPECTED_SHA and all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Card flow environment differs')
    require(hashlib.sha256(ram).hexdigest()==m['iop_sha256'] and m['device_profile']=='both memory-card slots disabled','Card flow identity/profile differs')
    settings=(folder/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8')
    require('Slot1_Enable = false' in settings and 'Slot2_Enable = false' in settings,'Card profile enables a slot')
    require(ram[CODE:CODE+len(hook(m['target']))]==hook(m['target']) and ram[m['import_address']:m['import_address']+8]==pack([0x08000000|(CODE>>2),0]),'Card flow terminal hook differs')
    require(ram[BASE:BASE+8]==pack([0x08000000|((CODE+0x200)>>2),0]) and ram[CODE+0x200:CODE+0x200+len(control_hook())]==control_hook(),'Control observer differs')
    require(ram[IMPORT:IMPORT+8]==pack([0x08000000|((CODE+0x400)>>2),0]) and ram[CODE+0x400:CODE+0x400+len(send_hook(m['start_target']))]==send_hook(m['start_target']),'Send observer differs')
    require(m['delay_count']==5 and m['delay_argument']==300 and m['fifo']==[0x81,0x52,0]*5,'Timed FIFO observer result differs')
    require(ram[CODE+0x600:CODE+0x600+len(count_hook(m['delay_target']))]==count_hook(m['delay_target']), 'Delay observer code differs')
    require(ram[CODE+0x680:CODE+0x680+len(fifo_hook())]==fifo_hook() and ram[BASE+0x144:BASE+0x14C]==pack([0x08000000|((CODE+0x680)>>2),0]),'FIFO observer code differs')
    require(ram[0x4D030+0x11B40:0x4D030+0x11B48]==pack([0x08000000|((CODE+0x600)>>2),0]),'Delay observer import differs')
    require(m['sends']==[[0x1181,0,0x100472]]*5+[[0xF381,0,0x140572]],'Actual card send sequence differs')
    require(m['controls'] and all(x==[0x3BD,0x3BD] for x in m['controls']),'Actual control values/readbacks differ')
    source,send_bcr,send_chcr,destination,recv_bcr,recv_chcr=m['final_dma_registers']
    require((send_bcr,send_chcr,recv_bcr,recv_chcr)==(0x10024,0x201,0x10024,0x40000200),'Final DMA registers differ')
    base=source-144-0x12020
    require(destination-144==base+0x12650,'Receive source ownership differs')
    tx=bytes([0x81,0xF3])+bytes(142);rx=bytes([0xFF])*6+bytes(138)
    require(ram[source-144:source]==tx and ram[destination-144:destination]==rx,'Actual final owned buffers differ')
    sys.path.insert(0,str(root/'tools/iop'));from inspect_irx import inspect,relocate
    module=(root/'rom/IRX/MCMAN.IRX').read_bytes();meta=inspect(module);image=relocate(module,meta,base)
    require(all(ram[base+a:base+b]==image[a:b] for a,b in meta['code_ranges']),'Card source module code differs')
    require(words(ram,m['descriptor_pointer'],4)==tuple(m['descriptor']) and m['descriptor'][1:]==[0x3A7480,192,0],'Actual pending secondary descriptor differs')
    if native: # The native card service has no SIO2 state; only EE-side query completion is compared.
        from verify_sound_rpc import completed_card_query
        r=json.loads(native.read_text(encoding='utf-8'));require(completed_card_query(r),'Native card query lifecycle differs')
    print('Six absent-card DMA pairs, five timed FIFO probes'+(' and native EE query completion' if native else '')+' verified. Control counts/scheduling are not equated.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
