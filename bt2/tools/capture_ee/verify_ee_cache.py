"""Compare original indexed cache scan with the coherent native RAM contract."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,texture_stub,capture_trap

def require(ok,message):
    if not ok:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)

def verify(root,native):
    snapshots=[]
    for point,hooks in (('rpc-cache-after',(0x32F4CC,0x32F508)),('rpc-cache-invalidate-after',(0x32F60C,0x32F648))):
        folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
        m=json.loads((folder/'capture.json').read_text(encoding='utf-8'))
        ee=(folder/'ee-ram.bin').read_bytes()
        require(len(ee)==32*1024*1024 and m['source_elf_sha256']==EXPECTED_SHA,'Cache identity differs')
        require(hashlib.sha256(ee).hexdigest()==m['capture_sha256'],'Cache capture hash differs')
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(ee[address:address+8]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+0x800]==stub[:0x800],'Cache instrumentation differs')
        for hook,offset in ((hooks[0],0x100),(hooks[1],0x180)):
            require(words(ee,hook,2)==(0x08000000|((FRESH_BIND_STUB+offset)>>2),0),'TagLo hook differs')
        require(words(ee,FRESH_BIND_STUB+0x800,128)==(0,)*128,'TagLo observations differ or sentinel remains')
        require(words(ee,MARKER,1)==(marker,) and words(ee,MARKER+24,4)==(0x365BC0,0x366400,0x1000,0xFFFFF000),'Original scan return differs')
        require(words(ee,MARKER+40,1)[0]&0x10001==0x10001,'Original interrupt restoration differs')
        snapshots.append(ee)
    require(snapshots[0][0x365BC0:0x366440]==snapshots[1][0x365BC0:0x366440],'Original cache helpers changed range')
    r=json.loads(native.read_text(encoding='utf-8'))
    require(r['indexed_cache_operations']>=256 and r['indexed_cache_tag_reads']==r['indexed_cache_operations'] and r['indexed_cache_tag_reads']%128==0,'Native indexed scan incomplete')
    require(r['cri_rpc_calls']>=2 and not r.get('native_iop_error'),'Native cache caller/control cleanup incomplete')
    owned=Path(str(native)+'.ee.bin').read_bytes()
    if r.get('cri_stream_transfers',0):
        # Later helper fills reuse this exact 0x880-byte stream buffer; its owned IOP copy must match.
        iop=Path(str(native)+'.iop.bin').read_bytes()
        sent,held=iop[0x14FD40:0x14FD40+0x880],owned[0x365BC0:0x366440]
        # Either the EE has re-uploaded the buffer, or the stop falls after the VBlank echo returned it
        # (identical except the sequence counter, advanced by one) and before the next upload.
        echoed=sent[:-4]==held[:-4] and int.from_bytes(held[-4:],'little')==int.from_bytes(sent[-4:],'little')+1
        require(sent==held or echoed,'Transferred stream buffer differs')
    else:require(owned[0x365BC0:0x366440]==ee[0x365BC0:0x366440],'Coherent cache range differs')
    print('Verified both indexed ways, original caller return and coherent native range; physical cache timing remains unvalidated.')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',required=True,type=Path);args=parser.parse_args()
    verify(Path(__file__).resolve().parents[2],args.native)
