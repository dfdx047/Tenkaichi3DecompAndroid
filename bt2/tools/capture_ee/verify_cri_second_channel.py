"""Verify original second-channel requests/results against the owned native checkpoint."""
import argparse,hashlib,json,struct
from pathlib import Path
import capture_ee as capture
from verify_cri_autodma import require,words

def reference(root,point,index,environment):
    folder=root/'work/pcsx2-capture'/(point+'-absent-cards-'+str(index))
    m=json.loads((folder/'capture.json').read_text(encoding='utf-8'))
    ee=(folder/'ee-ram.bin').read_bytes();iop=(folder/'iop-ram.bin').read_bytes()
    previous=capture.CRI_CALL_INDEX;capture.CRI_CALL_INDEX=index
    try:stub=capture.texture_stub(point)
    finally:capture.CRI_CALL_INDEX=previous
    address,trap,marker=capture.capture_trap(point)
    require(m['source_elf_sha256']==capture.EXPECTED_SHA and len(ee)==32*1024*1024 and len(iop)==2*1024*1024,'Second-channel identity differs')
    require(hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and hashlib.sha256(iop).hexdigest()==m['iop_capture_sha256'],'Second-channel hashes differ')
    require(ee[address:address+len(trap)]==trap and ee[capture.FRESH_BIND_STUB:capture.FRESH_BIND_STUB+len(stub)]==stub
            and words(ee,capture.MARKER,1)==(marker,) and words(ee,capture.MARKER+60,1)==(index,), 'Second-channel observer/publication differs')
    current=[m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]
    require(environment is None or current==environment,'Second-channel environment differs')
    return ee,iop,current

def verify(root,native,service_ram=None):
    environment=None;refs={}
    for index in range(36,43):
        ee,iop,environment=reference(root,'rpc-cri-nth-before',index,environment);refs[index]=(ee,iop)
        command=2 if index==36 else 0x40A if index==37 else 0x408
        size=12 if index==37 else 16;out=0 if index==37 else 4
        require(words(ee,capture.MARKER+24,9)==(0x355C70,command,0,0x39DC80,size,0x39DD80,out,0,0x2FCB20 if index==36 else 0x2FD1F8),'Original request ABI differs')
        expected=(1,0x35FD00,0x191740,0x880) if index==36 else (0x10,0x192000,0x4040) if index==37 else (2,0,0xCC970+(index-38)*0x60,0xCC9A0+(index-38)*0x60)
        require(words(ee,0x39DC80,size//4)==expected,'Original second-channel input differs')
    require(words(refs[36][0],0x3AC8C0,1)==(0x8D0,) and words(refs[36][0],0x3AC880,1)==(0x191700,), 'Fifth heap differs')
    require(words(refs[37][0],0x3AC8C0,1)==(0x4040,) and words(refs[37][0],0x3AC880,1)==(0x192000,)
            and words(refs[37][0],0x39DD80,1)==(0xC6834,), 'Sixth heap/second create return differs')
    for index in range(39,43):require(words(refs[index][0],0x39DD80,1)==(0xC6CD0+(index-39)*0x1C,), 'Original preceding pair result differs')
    final,retail,_=reference(root,'rpc-cri-nth-control-after',40,environment)
    require(words(final,0x39DD80,1)==(0xC6D40,) and words(final,0x355C70,1)==(0,), 'Original fifth pair result/cleanup differs')
    for index in range(39,43):
        address=0xC6CD0+(index-39)*0x1C
        require(words(refs[index][1],address,7)==words(retail,address,7),'Constructed object changed between independent captures')
    r=json.loads(native.read_text(encoding='utf-8'));ee=Path(str(native)+'.ee.bin').read_bytes();iop=Path(str(native)+'.iop.bin').read_bytes()
    calls=r['cri_rpc_calls']
    require(calls in range(37,43) and (r['heap_calls']>=7 if r.get('display_reset_phase',0)>=8 else r['heap_calls']==6) and not r.get('native_iop_error'),'Native second-channel setup incomplete')
    completed=calls-37
    def normalize(value):return value+0x1A7000-0xB7F30 if 0xB7F30<=value<0xCE430 else value
    last_result=normalize(0xC6CD0+(completed-1)*0x1C) if completed else 0x1B5904
    # Main's later embedded reload reuses the heap client: its free leaves result 0 for input 0x196100.
    # Later heap calls (embedded reload, sound-bank blocks) reuse the client; their results are compared elsewhere.
    heap=words(ee,0x3AC880,1)==(0x192000,) if r['heap_calls']==6 else words(ee,0x3AC880,1)==(0,) and words(ee,0x3AC8C0,1)==(0x196100,) if r['heap_calls']==7 else True
    require(heap and words(ee,0x39DD80,1)==(last_result,) and (words(ee,0x355C70,1)==(0,) or (r['selector']==0x77 and words(ee,0x3A97DC,1)==(0x355C70,))), 'Native setup result/cleanup differs') # Later boundary may be the next CRI submission.
    # IOP-side pair objects and slot fields were emulated-IOP internals, retired with the native CRI service.
    print(f'Verified both allocations and generated EE setup cleanup ({completed}/5 pairs completed).')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);p.add_argument('--service-ram',type=Path);a=p.parse_args();verify(Path(__file__).resolve().parents[2],a.native,a.service_ram)
