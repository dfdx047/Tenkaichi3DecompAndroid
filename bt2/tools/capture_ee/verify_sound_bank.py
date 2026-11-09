"""Compare main's first sound-bank load (0x297A60) with the original.

Original observer point: `rpc-sound-bank-after` (main returns from the loader at its loop head,
0x100680, first entry). The native checkpoint may stop inside the loader, so the pieces it has
completed are compared: the uploaded IOP sound block, both bank blocks at their captured heap
placements, the SOUNDS request record and the loader's slot table. SPU2 sound RAM is not part of
the captures and is not compared.
"""
import argparse,json,struct
from pathlib import Path
from verify_vblank_wake import original
from verify_sound_rpc import require

BANK={16:0x1FFED40,48:0x38BDF0}
BLOCK=(0xCE500,0x16640);FIRST=(0x196100,0x2C0);SECOND=(0x196400,0x180)
REQUEST=(0x3B0E00,12);TABLE=0x635080

def native_scope(r):
    require(r['display_reset_phase']==13 and r['iop_uploads']>=3 and r['heap_calls']>=9 and r['sound_rpc_calls']>=3
        and not r.get('native_iop_error'),'Native sound-bank scope differs')

def verify(root,native):
    ee=original(root,'rpc-sound-bank-after',BANK)
    iop=(root/'work/pcsx2-capture/rpc-sound-bank-after-absent-cards/iop-ram.bin').read_bytes()
    r=json.loads(native.read_text(encoding='utf-8'));native_scope(r)
    ne=Path(str(native)+'.ee.bin').read_bytes();ni=Path(str(native)+'.iop.bin').read_bytes()
    for name,(address,size) in (('sound block',BLOCK),('first bank block',FIRST),('second bank block',SECOND)):
        require(ni[address:address+size]==iop[address:address+size],f'IOP {name} differs')
    a,n=REQUEST;require(ne[a:a+n]==ee[a:a+n],'SOUNDS request record differs')
    if r['sounds_rpc_calls']>=2:
        # SOUNDS command 0x20 reply: word 8 is a model-local thread ID; word 13 is a SOUNDS address
        # (native module base 0x1C0000, retail 0xA3430).
        o=list(struct.unpack_from('<16I',ee,0x635180));n=list(r['sounds_result_words'])
        require(len(n)==16 and all(o[k]==n[k] for k in range(16) if k not in (8,13)) and n[8] and n[13]-0x1C0000==o[13]-0xA3430,
            'SOUNDS command reply differs')
    require(ne[TABLE:TABLE+4]==ee[TABLE:TABLE+4] and ne[TABLE+32:TABLE+36]==ee[TABLE+32:TABLE+36]
        and ne[TABLE+64:TABLE+68]==ee[TABLE+64:TABLE+68],'Loader slot table differs')
    print('Verified the IOP sound block, both bank blocks at their captured placements, the SOUNDS request record'
        ' and the loader slot table. SPU2 sound RAM and playback are not compared.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args()
    verify(Path(__file__).resolve().parents[2],a.native)
