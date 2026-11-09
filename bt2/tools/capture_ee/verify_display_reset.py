"""Compare main's repeated clear, embedded reload and display setup with the original.

Original observer points: `rpc-display-init-after` (main returns from display setup at
0x100658) and `rpc-timer-seed-after` (main returns from its system-time read). Native
evidence is the normal startup report. VBlank and field timing are logical; the field
sample is compared because main stores it.
"""
import argparse,json
from pathlib import Path
from verify_vblank_wake import original
from verify_sound_rpc import require,words

# Marker payload: +16 sp, +48 gp. Display setup ends with EnableIntc(2) (a0 = 2).
DISPLAY={16:0x1FFED40,24:2,48:0x38BDF0}
DBUFF=0x3B0850;DBUFF_SIZE=0x240;GS_STATE=0x368090;FIELD_FLAG=0x3B0A80

def native_scope(r):
    require(r['display_reset_phase']==13 and r['polled_vblanks']==2 and r['vblank_handler_registrations']==2
        and r['vblank_enable_calls']==2 and r['vblank_disable_calls']==1 and r['heap_calls']>=7 and r['heap_frees']==2
        and r['native_module_loads']==15 and r['cri_rpc_calls']==42 and not r.get('native_iop_error'),'Native display setup scope differs')

def verify(root,native):
    r=json.loads(native.read_text(encoding='utf-8'));native_scope(r)
    later=r.get('frame_vsync_deliveries',0)>0
    ee=original(root,'rpc-frame-end',{16:0x1FFECF0,48:0x38BDF0}) if later else original(root,'rpc-display-init-after',DISPLAY)
    iop=(root/'work/pcsx2-capture/rpc-display-init-after-absent-cards/iop-ram.bin').read_bytes()
    ne=Path(str(native)+'.ee.bin').read_bytes();ni=Path(str(native)+'.iop.bin').read_bytes()
    require(ne[DBUFF:DBUFF+DBUFF_SIZE]==ee[DBUFF:DBUFF+DBUFF_SIZE],'Double-buffer environment differs')
    o=words(ee,GS_STATE,8);n=words(ne,GS_STATE,8)
    # Kernel handler IDs are model-local: native IDs run one below the original (4 -> 5 there, 3 -> 4 here).
    require(o[:3]==n[:3] and o[4:]==n[4:] and n[3]+1==o[3],'GS library state differs')
    require(words(ne,FIELD_FLAG,1)==words(ee,FIELD_FLAG,1)==((1,) if later else (0,)),'Field sample differs')
    # The sound-bank blocks later reuse this placement (verify_sound_bank.py compares them).
    require(r['heap_calls']>7 or (ni[0x196100:0x196460]==iop[0x196100:0x196460] and ni[0x196530:0x196570]==iop[0x196530:0x196570]),'Embedded reload placement differs')
    # Later heap calls reuse the client words; the reload's free result is compared while it is the last call.
    require((r['heap_calls']>7 or (words(ne,0x3AC8C0,1)==words(ee,0x3AC8C0,1)==(0x196100,) and words(ne,0x3AC880,1)==words(ee,0x3AC880,1)==(0,)))
        and words(ne,0x3ACA04,1)==words(ee,0x3ACA04,1)==(1,),'Embedded reload result/free differs')
    print('Verified the repeated clear field sample, embedded reload placement/result/free, double-buffer environment and GS library state.'
        ' VBlank/field timing is logical; kernel handler and module IDs are model-local.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args()
    verify(Path(__file__).resolve().parents[2],a.native)
