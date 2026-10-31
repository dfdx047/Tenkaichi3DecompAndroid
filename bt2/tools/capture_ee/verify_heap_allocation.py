"""Verify the reached local IOP heap allocation before enabling its native handler."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
from verify_heap_bind import verify as verify_bind

def verify(root,folder,disc,output):
    def require(ok,message):
        if not ok:raise ValueError(message)
    verify_bind(root,folder.parent/'fileio-bind-after',disc,output.with_name('heap-bind.bin'))
    m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes();iop=(folder/'iop-ram.bin').read_bytes()
    before=json.loads((folder.parent/'fileio-bind-after/capture.json').read_text(encoding='utf-8'))
    require(len(ee)==32*1024*1024 and len(iop)==2*1024*1024,'RAM extent differs')
    require(m['source_elf_sha256']==EXPECTED_SHA and m['capture_point'].startswith('heap-alloc-after:'),'Allocation identity differs')
    require(all(m[k]==before[k] for k in ['pcsx2_sha256','bios_sha256','disc_sha256']),'Environment differs')
    require(hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and hashlib.sha256(iop).hexdigest()==m['iop_capture_sha256'],'RAM hashes differ')
    address,trap,marker=capture_trap('heap-alloc-after')
    require(ee[address:address+len(trap)]==trap and struct.unpack_from('<I',ee,MARKER)[0]==marker,'Allocation trap differs')
    require(struct.unpack_from('<3I',ee,MARKER+8)==(0xCE500,0,0x1FFECD0),'Allocation return/stack differs')
    require(struct.unpack_from('<I',ee,0x3AC8C0)[0]==struct.unpack_from('<I',iop,0x48480)[0]==0x360,'Reached size differs')
    require(struct.unpack_from('<I',iop,0x48410)[0]==struct.unpack_from('<I',ee,0x3AC880)[0]==0xCE500,'Handler output/destination differs')
    require(struct.unpack_from('<10I',ee,0x3AC840)==(0,23,7,0,0,0x48480,0x38BDF0,0,0,0x48438),'Allocation client differs')
    reply=(0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0)
    require(struct.unpack_from('<16I',ee,0x3A94C0)==reply,'Allocation reply differs')
    old=(folder.parent/'fileio-bind-after/iop-ram.bin').read_bytes()
    require(iop[0x44430:0x46F20]==old[0x44430:0x46F20],'Boot FILEIO code changed')
    require(struct.unpack_from('<3I',iop,0x48438)==(0x80000003,0x46C04,0x48480),'Heap server changed')
    output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(struct.pack('<17I',*reply,0xCE500))
    output.with_suffix('.json').write_text(json.dumps({'verified':'First 864-byte heap request, handler result, client, reply and destination','capture':m,'limitations':'Initial allocated contents are undefined; no general allocator/free/file-loading behavior established.'},indent=2)+'\n',encoding='utf-8')
    print('Reached 864-byte heap allocation result and RPC effects verified.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--capture',type=Path,required=True);p.add_argument('--disc',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();verify(a.root,a.capture,a.disc,a.output)
