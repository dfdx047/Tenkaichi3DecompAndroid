"""Verify the actual boot FILEIO heap-service bind; no file reads or allocations supplied."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'iop'))
from inspect_image import inspect as inspect_image,disc_image
from inspect_irx import inspect,relocate

def verify(root,folder,disc,output):
    def require(ok,why):
        if not ok:raise ValueError(why)
    m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes();iop=(folder/'iop-ram.bin').read_bytes()
    require(len(ee)==32*1024*1024 and len(iop)==2*1024*1024,'RAM extent differs')
    require(m['source_elf_sha256']==EXPECTED_SHA and m['capture_point'].startswith('fileio-bind-after:'),'Identity/point differs')
    require(hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and hashlib.sha256(iop).hexdigest()==m['iop_capture_sha256'],'RAM hashes differ')
    with disc.open('rb') as stream:require(hashlib.file_digest(stream,'sha256').hexdigest()==m['disc_sha256'],'Disc differs')
    address,trap,marker=capture_trap('fileio-bind-after')
    require(ee[address:address+len(trap)]==trap and struct.unpack_from('<I',ee,MARKER)[0]==marker,'Unique caller trap differs')
    require(struct.unpack_from('<3I',ee,MARKER+8)==(0,0,0x1FFED40),'Return result/stack differs')
    image=(root/'rom/IRX/IOPRP300.IMG').read_bytes()
    require(image==disc_image(disc,b'IOPRP300.IMG;1'),'IOPRP identity differs')
    row=next(r for r in inspect_image(image)['entries'] if r['name']=='FILEIO');module=image[row['offset']:row['offset']+row['size']]
    report=inspect(module);base=0x44430;linked=relocate(module,report,base)
    require(report['name']=='FILEIO_service' and report['code_end']==0x2AF0,'Boot module differs')
    require(struct.unpack_from('<4I',iop,base-32)==(base,base+report['gp'],base,0x2E80),'Module ownership differs')
    require(all(iop[base+a:base+b]==linked[a:b] for a,b in report['code_ranges']),'Relocated FILEIO code differs')
    server=0x48438;queue=0x48420;buffer=0x48480
    require(struct.unpack_from('<3I',iop,server)==(0x80000003,0x46C04,buffer) and struct.unpack_from('<I',iop,server+64)[0]==queue,'Heap server differs')
    handle,_,registered=struct.unpack_from('<3I',iop,queue);record=((handle>>7)<<2)+8
    require(registered==server and struct.unpack_from('<I',iop,record)[0]==(((handle>>1)&63)<<16)|0x7F01,'Worker tag/queue differs')
    require(struct.unpack_from('<I',iop,record+4)[0]==0x600004 and struct.unpack_from('<I',iop,record+20)[0]==1,'Worker priority/sleep differs')
    require(struct.unpack_from('<4I',iop,record+48)==(0x46D3C,0x1FA600,0x800,base+report['gp']),'Worker entry/stack/GP differs')
    require(struct.unpack_from('<4I',iop,0x1E700)==(0x1E640,0x1E6C0,0x3A94C0,0x1E730),'Transport differs')
    require(struct.unpack_from('<10I',ee,0x3AC840)==(0,22,7,0,0,buffer,0,0,0,server),'Heap client differs')
    expected=(0,0,0x80000008,0,0,0x203A97C0,0,0x3AC840,0x80000009,server,buffer,0,0,0,0,0)
    require(struct.unpack_from('<16I',ee,0x3A94C0)==expected,'Heap bind reply differs')
    output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(ee[0x3A94C0:0x3A9500])
    output.with_suffix('.json').write_text(json.dumps({'verified':'IOPRP FILEIO code/ownership, heap RPC worker, client and bind return','capture':m,'module_sha256':report['sha256'],'limitations':'Reference-backed lookup only; no native boot FILEIO worker, heap allocation or file read executed.'},indent=2)+'\n',encoding='utf-8')
    print('Local heap bind, relocated boot FILEIO code and owned reference worker verified.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--capture',type=Path,required=True);p.add_argument('--disc',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();verify(a.root,a.capture,a.disc,a.output)
