"""Compare the EE timer library state and system-time seed with the original.

Original observer point: `rpc-timer-seed-after` (main returns from its system-time read at
0x100660; marker v0 holds the result). Native time is logical (one NTSC field per VBlank), so
overflow counts and the seed value are reported, not equated. The library structure, alarm
list and handler acknowledgement state are compared.
"""
import argparse,json,struct
from pathlib import Path
from verify_vblank_wake import original
from verify_sound_rpc import require,words

STATE=0x36BAD0;REENT_POINTER=0x36A600;SEED_OFFSET=168
SEED={16:0x1FFED40,48:0x38BDF0}

def native_scope(r):
    require(r['timer2_fields']==r['vblank_deliveries']+r['polled_vblanks'] and r['timer2_deliveries']==r['timer2_wraps']
        and 0<r['timer2_wraps']<=r['timer2_fields']*96096//655360+1 and r['display_reset_phase']==13,'Native Timer 2 accounting differs')

def verify(root,native):
    ee=original(root,'rpc-timer-seed-after',SEED)
    original_seed=struct.unpack_from('<Q',ee,0x387E80+8)[0]
    require(original_seed>>8 & 0xFFFF and original_seed%256==0,'Original system-time shape differs')
    r=json.loads(native.read_text(encoding='utf-8'));native_scope(r)
    ne=Path(str(native)+'.ee.bin').read_bytes()
    o=words(ee,STATE,10);n=words(ne,STATE,10)
    # +0 overflow count (logical timing), +8 kernel handler ID (model-local: native one above here).
    require(o[1]==n[1]==0 and o[3:]==n[3:] and n[2]==o[2]+1,'Timer library state differs')
    require(n[0]==r['timer2_deliveries'],'Native overflow count differs from delivered overflows')
    reent=words(ne,REENT_POINTER,1)[0];require(reent==words(ee,REENT_POINTER,1)[0],'C library state pointer differs')
    seed=struct.unpack_from('<Q',ne,reent+SEED_OFFSET)[0]
    # The seed was sampled earlier; later wraps only raise the current overflow count.
    require(seed!=1 and seed%256==0 and seed>>24<=n[0],'Native seed is not a Timer 2 system time')
    print(f'Verified timer library state, empty alarm list and acknowledged overflows ({n[0]} native, {o[0]} original);'
        f' native seed {seed:#x}, original {original_seed:#x}. Time is logical; values are not equated.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args()
    verify(Path(__file__).resolve().parents[2],a.native)
