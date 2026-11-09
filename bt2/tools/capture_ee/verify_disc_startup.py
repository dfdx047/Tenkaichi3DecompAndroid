"""Validate the first disc-search bind boundary, without granting file lookup success."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_mc_timed import completed_card_query

def require(condition,message):
    if not condition:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)

def disc_bind_boundary(r):
    return (r.get('disc_bind_semaphores')==1 and r.get('selector')==0x77 and r.get('pc')==0x32F028
        and r.get('target')==0 and r.get('native_iop_error','')==''
        and r.get('packet_words',[])[6:9]==[42,0x3A8780,0x80000597])

def disc_search_completed(r):
    return (r.get('disc_rpc_binds')==1 and r.get('disc_rpc_calls') in range(1,7)
        and r.get('disc_result')==1 and r.get('native_iop_error','')=='')

def verify(root,native):
    baseline=json.loads((root/'work/pcsx2-capture/rpc-mc-callback-before-absent-cards/capture.json').read_text(encoding='utf-8'))
    observations={}
    for point in ('rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after','rpc-disc-command-before','rpc-disc-command-after'):
        f=root/'work/pcsx2-capture'/(point+'-absent-cards')
        m=json.loads((f/'capture.json').read_text(encoding='utf-8'));b=(f/'ee-ram.bin').read_bytes();i=(f/'iop-ram.bin').read_bytes()
        require(m['capture_point'].startswith(point+':') and m['source_elf_sha256']==EXPECTED_SHA and len(b)==32*1024*1024 and len(i)==2*1024*1024,'Disc capture identity/size differs')
        require(hashlib.sha256(b).hexdigest()==m['capture_sha256'] and hashlib.sha256(i).hexdigest()==m['iop_capture_sha256'],'Disc capture hashes differ')
        require(all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Disc environment differs')
        ini=(f/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8')
        require('Slot1_Enable = false' in ini and 'Slot2_Enable = false' in ini,'Disc capture card profile differs')
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(b[address:address+len(trap)]==trap and b[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(b,MARKER,1)==(marker,),'Disc hook/publication differs')
        observations[point]=b
    b=observations['rpc-disc-bind-sema'];sp=words(b,MARKER+16,1)[0]
    require(sp==0x1FFEB10 and words(b,MARKER+8,2)==(10,0),'Actual disc semaphore return differs')
    descriptor=words(b,sp,6)
    require((descriptor[1],descriptor[2],descriptor[5])==(1,0,0x3838D8),'Actual disc semaphore input differs')
    b=observations['rpc-disc-bind-after']
    require(words(b,MARKER+8,2)==(0,0) and words(b,0x3A8780,10)==(0,42,10,0,0,0x441C8,0,0,0,0x433D0),'Actual disc-search bind client differs')
    r=json.loads(native.read_text(encoding='utf-8'))
    require(completed_card_query(r) and r.get('disc_bind_semaphores')==1,'Native prior query/disc semaphore lifecycle differs')
    require(disc_bind_boundary(r) or (r.get('disc_rpc_binds')==1 and r.get('native_iop_error','')==''),'Native disc bind transition differs')
    reply=b[0x3A94C0:0x3A9500]
    (root/'work/bt2-generation/disc-bind.bin').write_bytes(reply)
    before=observations['rpc-disc-command-before'];after=observations['rpc-disc-command-after']
    require(before[0x3A8624:0x3A8636]==b'\\DATA\\ZS2US.DIR;1\0' and words(before,0x3A8724,2)==(0x3A8600,0),'First search request/path differs')
    require(words(after,MARKER+8,2)==(0,0) and words(after,0x3A8740,1)==(1,),'Actual metadata result differs')
    require(words(after,0x3A8600,2)==(0xE2A,64) and after[0x3A8608:0x3A8614]==b'ZS2US.DIR;1\0','Actual directory index metadata differs')
    f=root/'work/pcsx2-capture/rpc-disc-command-before-absent-cards-disc-dma'
    m=json.loads((f/'disc-dma.json').read_text(encoding='utf-8'));i=(f/'disc-dma-iop.bin').read_bytes();ee=(f/'disc-dma-ee.bin').read_bytes()
    require(all(m[k]==baseline[k] for k in ('source_elf_sha256','pcsx2_sha256','bios_sha256','disc_sha256')),'DMA environment differs')
    require(hashlib.sha256(i).hexdigest()==m['iop_sha256'] and hashlib.sha256(ee).hexdigest()==m['ee_sha256'],'DMA hashes differ')
    require(m['descriptor']==[0x441C8,0x3A8600,36,0] and m['count']==1 and m['token']>0 and m['diagnostic_source_redirect'] is None,'Actual metadata DMA differs')
    source=i[0x1D0800:0x1D0824]
    require(hashlib.sha256(source).hexdigest()==m['source_sha256'] and ee[0x3A8600:0x3A8624]==source,'Actual secondary metadata copy differs')
    from capture_disc_dma import submit_hook
    require(i[0x1D0300:0x1D0300+len(submit_hook(m['targets'][0]))]==submit_hook(m['targets'][0]),'DMA observer differs')
    if r.get('disc_rpc_calls'):
        require(disc_search_completed(r),'Native original handler/transport differs')
        ne=Path(str(native)+'.ee.bin').read_bytes()
        if r.get('disc_rpc_calls',0)>1:
            kind=['search-afs','search-afs2','search-op','search-ed','search-logo'][r['disc_rpc_calls']-2]
            f=root/('work/pcsx2-capture/rpc-ncmd-before-absent-cards-disc-api-'+kind);m=json.loads((f/'disc-api.json').read_text(encoding='utf-8'));b=(f/'disc-api-iop.bin').read_bytes()
            require(hashlib.sha256(b).hexdigest()==m['iop_sha256'] and m['result']==1,'Repeated metadata evidence differs')
            source=b[0x441C8:0x441EC]
        for lo,hi in ((0,8),(8,20),(24,36)):
            require(ne[0x3A8600+lo:0x3A8600+hi]==source[lo:hi],'Native defined search metadata (EE copy) differs')
    print('Actual disc initialization/semaphore/bind evidence verified. First search request/result verified independently; native completion is checked when present. No sector success claimed.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path,default=Path('work/bt2-generation/entry-stop.json'));a=p.parse_args();verify(a.root,a.native)
