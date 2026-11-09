"""Independently compare guarded local LIBSD transfer/initialization captures."""
import argparse, hashlib, json, struct
from pathlib import Path
import capture_spu2_transfer as transfer
import capture_spu2_initialized as initialized
import capture_spu2_key_readback as key
import capture_cri_low_read as low

def verify(root, disc):
    expected=json.loads((root/'config/retail_revision.json').read_text(encoding='utf-8'))['executable']['sha256']
    with disc.open('rb') as stream: disc_hash=hashlib.file_digest(stream,'sha256').hexdigest()
    def capture(folder,prefix,module,location=None):
        directory=root/'work/pcsx2-capture'/folder
        metadata=json.loads((directory/(prefix+'.json')).read_text(encoding='utf-8'))
        ram=(directory/(prefix+'-iop.bin')).read_bytes()
        ee=(directory/(prefix+'-ee.bin')).read_bytes()
        assert len(ram)==2*1024*1024 and len(ee)==32*1024*1024
        assert metadata['source_sha256']==expected and metadata['disc_sha256']==disc_hash
        assert hashlib.sha256(ram).hexdigest()==metadata['iop_sha256']
        assert hashlib.sha256(ee).hexdigest()==metadata['ee_sha256']
        code=module.hook()
        assert hashlib.sha256(code).hexdigest()==metadata['hook_sha256']
        assert ram[module.CODE:module.CODE+len(code)]==code
        address=module.HOOK if location is None else location
        assert ram[address:address+8]==struct.pack('<2I',0x08000000|(module.CODE>>2),0)
        assert struct.unpack_from('<I',ram,module.SCRATCH)[0]==1
        return metadata,ram
    t,ram=capture('sounds-module-after-spu2-transfer','spu2-transfer',transfer)
    assert struct.unpack_from('<3I',ram,transfer.SCRATCH)==(1,t['address'],t['value'])
    assert t['address']==0xBF900344 and t['value']==0x80
    assert list(struct.unpack_from('<8I',ram,transfer.SCRATCH+28))==t['source_words']
    assert list(struct.unpack_from('<8I',ram,transfer.SCRATCH+60))==t['sound_ram_readback']==t['source_words']
    assert list(struct.unpack_from('<8H',ram,t['source_pointer']))==t['source_words']
    assert {hex(p):v for p,v in zip(transfer.REGISTERS,struct.unpack_from('<3I',ram,transfer.SCRATCH+12))}==t['registers']
    i,ram=capture('sounds-module-after-spu2-initialized','spu2-initialized',initialized)
    assert struct.unpack_from('<I',ram,initialized.SCRATCH+4)[0]==i['result']==0
    assert {hex(p):v for p,v in zip(initialized.REGISTERS,struct.unpack_from('<1024I',ram,initialized.SCRATCH+16))}==i['registers']
    checkpoint=json.loads((root/'work/iop/libsd-initialized-native.json').read_text())
    dynamic={0x340,0x342,0x740,0x742}
    stable=[r for r in checkpoint['spu2_configuration'] if r['address']-0x1F900000 not in dynamic]
    assert len(stable)==394
    for r in stable: assert i['registers'][hex(r['address']-0x1F900000)]==r['value']
    assert [r['value'] for r in checkpoint['spu2_sound_ram']]==t['source_words']
    for kind,offset in [('on',0x4278),('off',0x42B8)]:
        k,ram=capture('sounds-module-after-spu2-key-readback-'+kind,'spu2-key-readback',key,key.BASE+offset)
        assert k['kind']==kind and k['value']==0xFFFF
        assert struct.unpack_from('<3I',ram,key.SCRATCH)==(1,k['address'],k['value'])
        assert {hex(p):v for p,v in zip(key.REGISTERS,struct.unpack_from('<10I',ram,key.SCRATCH+12))}==k['registers']
    path=root/'work/pcsx2-capture/sounds-module-after-cri-low-read/cri-low-read.json'
    l=json.loads(path.read_text())
    l,ram=capture(path.parent.name,'cri-low-read',low,l['hook_address'])
    assert l['address']==0 and l['words']==list(struct.unpack_from('<3I',ram,0))==[0,0,0]
    assert l['module_base']==low.BASE and l['hook_address']==low.HOOK
    assert list(struct.unpack_from('<3I',ram,low.SCRATCH+8))==l['arguments']==[0,0,0]
    loading=bytearray(low.loading_hook())
    struct.pack_into('<2I',loading,7*4,0x08000000|((low.SCRATCH+0x100+9*4)>>2),0)
    assert ram[low.SCRATCH+0x100:low.SCRATCH+0x100+len(loading)]==loading
    assert struct.unpack_from('<I',ram,low.SCRATCH+4)[0]==0
    assert list(struct.unpack_from('<3I',ram,low.SCRATCH+20))==l['words']
    final=json.loads((root/'work/iop/cri-adxi-stop.json').read_text())
    assert final['entry_returned'] and final['spu2_io_transfer_completed']
    assert final['spu2_fifo_halfwords']==8 and final['spu2_io_status_reads']==1
    assert any(r['name']=='libsd:4' and r['value']==0 for r in final['native_export_results'])
    print('Transfer, 394 stable initialization registers, immediate key reads and scoped low-memory ABI verified.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--root',type=Path,default=Path.cwd()); parser.add_argument('--disc',type=Path,required=True)
    args=parser.parse_args(); verify(args.root,args.disc)
