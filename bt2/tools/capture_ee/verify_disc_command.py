"""Check independent first NCMD bind/readiness/request/readback evidence."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_disc_startup import require,words,disc_search_completed
from capture_disc_api import hook as api_hook,POINTS
from capture_disc_type import hook as type_hook

ENV=('source_elf_sha256','pcsx2_sha256','bios_sha256','disc_sha256')
def first_read_request(b):
    return words(b,0x3684C0,6)==(0xE2A,2,0x39DE80,0x100,0x3694C0,0x369580)
def ready_completed(r):
    return (disc_search_completed(r) and r.get('ncmd_rpc_binds')==1 and r.get('ncmd_bind_semaphores')==1
            and r.get('ncmd_ready_calls',0)>=1) # Later readiness queries precede later archive reads.
def verify(root,native,disc):
    base=root/'work/pcsx2-capture'
    baseline=json.loads((base/'rpc-mc-callback-before-absent-cards/capture.json').read_text(encoding='utf-8'))
    obs={};iops={}
    for point in ('rpc-ncmd-bind-sema','rpc-ncmd-bind-after','rpc-ncmd-call-sema','rpc-ncmd-before','rpc-ncmd-after'):
        f=base/(point+'-absent-cards');m=json.loads((f/'capture.json').read_text(encoding='utf-8'))
        b=(f/'ee-ram.bin').read_bytes();i=(f/'iop-ram.bin').read_bytes()
        require(all(m[k]==baseline[k] for k in ENV) and m['source_elf_sha256']==EXPECTED_SHA,'NCMD environment differs')
        require(len(b)==32*1024*1024 and len(i)==2*1024*1024,'NCMD memory size differs')
        require(hashlib.sha256(b).hexdigest()==m['capture_sha256'] and hashlib.sha256(i).hexdigest()==m['iop_capture_sha256'],'NCMD capture hashes differ')
        ini=(f/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8')
        require('Slot1_Enable = false' in ini and 'Slot2_Enable = false' in ini,'NCMD card profile differs')
        a,t,marker=capture_trap(point);stub=texture_stub(point)
        require(b[a:a+len(t)]==t and b[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(b,MARKER,1)==(marker,),'NCMD observer differs')
        obs[point]=b;iops[point]=i
    for point,sp,option in [('rpc-ncmd-bind-sema',0x1FFEB00,0x3838D8),('rpc-ncmd-call-sema',0x1FFEAF0,0x3838E8)]:
        b=obs[point];require(words(b,MARKER+8,2)==(10,0) and words(b,MARKER+16,1)==(sp,),'NCMD semaphore return differs')
        d=words(b,sp,6);require((d[1],d[2],d[5])==(1,0,option),'NCMD semaphore descriptor differs')
    b=obs['rpc-ncmd-bind-after'];i=iops['rpc-ncmd-bind-after']
    require(words(b,0x369590,10)==(0,44,10,0,0,0x43DB8,0,0,0,0x43418),'NCMD client differs')
    require(words(i,0x43418,3)==(0x80000595,0x417EC,0x43DB8),'NCMD server/handler/buffer differs')
    require(words(b,0x3A94C0,16)==(0,0,0x80000008,0,0,0x203A97C0,0,0x369590,0x80000009,0x43418,0x43DB8,0,0,0,0,0),'NCMD bind reply differs')
    (root/'work/bt2-generation/ncmd-bind.bin').write_bytes(b[0x3A94C0:0x3A9500])
    before=obs['rpc-ncmd-before'];after=obs['rpc-ncmd-after']
    require(first_read_request(before) and words(before,MARKER+24,4)==(0x369590,1,1,0x3684C0),'First sector request/mode differs')
    require(words(before,0x368440,1)==(2,) and words(before,0x3A94C0,9)==(0x400,0x368440,0x80000008,0,0,0,0,0x369590,0x8000000A),'Independent readiness result differs')
    require(words(after,MARKER+8,2)==(0,0) and words(after,0x3683F4,1)==(0,),'Read transport/completion flag differs')
    require(words(after,0x369590,10)==(0,46,0xFFFFFFFF,0,0,0x43DB8,0x38BDF0,0x31ECF8,0x3694C0,0x43418),'Actual async read client differs')
    with disc.open('rb') as f:f.seek(0xE2A*2048);payload=f.read(4096)
    require(len(payload)==4096 and after[0x39DE80:0x39EE80]==payload and before[0x39DE80:0x39EE80]!=payload,'Actual sector readback differs')
    for point,ready in [('rpc-ncmd-before',False),('rpc-ncmd-call-sema',True)]:
        f=base/(point+'-absent-cards-disc-type');m=json.loads((f/'disc-type.json').read_text(encoding='utf-8'));i=(f/'disc-type-iop.bin').read_bytes()
        require(all(m[k]==baseline[k] for k in ENV) and hashlib.sha256(i).hexdigest()==m['iop_sha256'],'Disc-register provenance differs')
        code=type_hook(ready);require(i[0x1D0100:0x1D0100+len(code)]==code and words(i,0x1D0000,3)==(1,0xBF40200F,0x14),'Disc-type observer/value differs')
        if ready:require(words(i,0x1D000C,2)==(0xBF402005,0x4C),'Readiness status differs')
    for point,kind,result in [('rpc-ncmd-call-sema','sc-read2',0),('rpc-ncmd-call-sema','sc-media',1),('rpc-ncmd-before','sc-key',0),('rpc-ncmd-before','sc-dual',1),('rpc-ncmd-before','sc-lsn',0xE2A),('rpc-ncmd-before','sc-timeout',15000)]:
        f=base/(point+'-absent-cards-disc-api-'+kind);m=json.loads((f/'disc-api.json').read_text(encoding='utf-8'));i=(f/'disc-api-iop.bin').read_bytes()
        require(all(m[k]==baseline[k] for k in ENV) and hashlib.sha256(i).hexdigest()==m['iop_sha256'],'Disc API provenance differs')
        code=api_hook(m['target'],kind);off,ordinal=POINTS[kind]
        require(i[0x1D0100:0x1D0100+len(code)]==code and words(i,0x3DD30+off,2)==(0x08000000|(0x1D0100>>2),0),'Disc API observer/import differs')
        require(m['kind']==kind and m['result']==result and words(i,0x1D0000,1)==(1,) and words(i,0x1D0008,1)==(result,) and list(words(i,0x1D0020,4))==m['arguments'],'Actual API return differs')
    r=json.loads(native.read_text(encoding='utf-8'));require(disc_search_completed(r) and r.get('ncmd_rpc_binds')==1 and r.get('ncmd_bind_semaphores')==1,'Native NCMD bind differs')
    if r.get('ncmd_ready_calls'):
        require(ready_completed(r),'Native readiness completion differs')
        ee=Path(str(native)+'.ee.bin').read_bytes();require(words(ee,0x368440,1)==(2,),'Native readiness result (EE) differs')
    print('NCMD bind, readiness hardware/API/result and actual two-sector readback independently verified. Native sector completion is not claimed.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path,default=Path('work/bt2-generation/entry-stop.json'));p.add_argument('--disc',type=Path,required=True);a=p.parse_args();verify(a.root,a.native,a.disc)
