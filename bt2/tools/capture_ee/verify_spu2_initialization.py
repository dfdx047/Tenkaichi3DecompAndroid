"""Verify ignored SPU2 captures and the native initialization checkpoint independently."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_spu2_status import hook,HOOK,CODE,SCRATCH,REGISTERS
from capture_spu2_interrupts import hook as irq_hook,HOOK as IRQ_HOOK

def verify(root,native,disc):
    expected_source=json.loads((root/'config/retail_revision.json').read_text(encoding='utf-8'))['executable']['sha256']
    with disc.open('rb') as stream:expected_disc=hashlib.file_digest(stream,'sha256').hexdigest()
    captures=root/'work/pcsx2-capture'
    status=json.loads((captures/'sounds-module-after-spu2-status/spu2-status.json').read_text())
    irq=json.loads((captures/'sounds-module-after-spu2-interrupts/spu2-interrupts.json').read_text())
    for folder,metadata,prefix,trampoline,location in [
        ('sounds-module-after-spu2-status',status,'spu2-status',hook(),HOOK),
        ('sounds-module-after-spu2-interrupts',irq,'spu2-interrupt',irq_hook(irq['target']),IRQ_HOOK)]:
        assert metadata['source_sha256']==expected_source and metadata['disc_sha256']==expected_disc,'Input provenance differs'
        ram=(captures/folder/(prefix+'-iop.bin')).read_bytes()
        ee=(captures/folder/(prefix+'-ee.bin')).read_bytes()
        assert len(ram)==2*1024*1024 and len(ee)==32*1024*1024
        assert hashlib.sha256(ram).hexdigest()==metadata['iop_sha256'] and hashlib.sha256(ee).hexdigest()==metadata['ee_sha256']
        assert ram[CODE:CODE+len(trampoline)]==trampoline and hashlib.sha256(trampoline).hexdigest()==metadata['hook_sha256']
        assert ram[location:location+8]==struct.pack('<2I',0x08000000|(CODE>>2),0)
        if metadata is status:
            assert struct.unpack_from('<3I',ram,SCRATCH)==(1,0xBF900344,0)
            registers=dict(zip(REGISTERS,struct.unpack_from('<13I',ram,SCRATCH+12)))
            assert metadata['address']==0xBF900344 and metadata['value']==0
            assert {hex(p):v for p,v in registers.items()}==metadata['registers']
        else:
            assert struct.unpack_from('<I',ram,SCRATCH)[0]==3
            records=[dict(zip(['irq','result_pointer','return','old_mask'],struct.unpack_from('<4I',ram,SCRATCH+16+i*16))) for i in range(3)]
            assert records==metadata['records'] and [r['irq'] for r in records]==[36,40,9]
            assert all(r['return']==r['old_mask']==0xFFFFFF99 for r in records)
    report=json.loads(native.read_text())
    config={r['address']:r['value'] for r in report['spu2_configuration']}
    for address,value in status['registers'].items():
        if int(address,16)!=0x744:assert config[0x1F900000+int(address,16)]==value
    assert report['spu2_reset_status_reads']==2 and report['spu2_pending_key_off']==[0xFFFFFF]*2
    assert report['pc']==0x940CC and report['steps']==16244 and not report['entry_returned']
    assert report['blocked_address']==0x1F9001AC and report['access_kind']=='write' and report['access_width']==2 and report['access_value']==0x700
    print('SPU2 capture provenance, hooks, publications and native reset/FIFO boundary verified.')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,default=Path.cwd());parser.add_argument('--native',type=Path,required=True);parser.add_argument('--disc',type=Path,required=True)
    args=parser.parse_args();verify(args.root,args.native,args.disc)
