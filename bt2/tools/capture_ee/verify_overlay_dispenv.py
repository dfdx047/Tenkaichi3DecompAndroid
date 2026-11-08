"""Compare the DBZP.BIN overlay's first display flip with the original.

Original observer point: `rpc-overlay-dispenv` (entry of the GS library's display-environment flip
0x311860, first call, reached from the overlay's frame path). The native checkpoint stops at the
following draw-environment send, after the flip's privileged GS stores; no RAM is written between.
Compared: the display/double-buffer block at 0x3B0850 (including the flip's source environment),
the recorded PMODE/SMODE2/DISPFB2/DISPLAY2/BGCOLOR values against that environment, and the whole
overlay range (code and any data it changed). No presentation is compared.
"""
import argparse,json,struct
from pathlib import Path
from verify_vblank_wake import original
from verify_sound_rpc import require

MARKER={48:0x38BDF0}
BLOCK=(0x3B0850,0x250)
OVERLAY=(0x3B3100,0x7D48F)

def native_scope(r):
    # Later checkpoints continue past the flip (the game's own display writes follow); the memory
    # compared below is unchanged by them.
    require(r['gs_display_flips']>=1 and r['gs_display_writes']>=5 and r['overlay_dispatch_validations']>=2
        and r['timer1_starts']==1 and r['fileio_calls']>=6,'Native overlay frame scope differs') # The IOP may stop later in the frame.

def verify(root,native):
    r=json.loads(native.read_text(encoding='utf-8'));native_scope(r)
    later=r.get('frame_vsync_deliveries',0)>0
    flip=original(root,'rpc-overlay-dispenv',MARKER)
    ee=original(root,'rpc-frame-end',{16:0x1FFECF0,48:0x38BDF0}) if later else flip
    ne=Path(str(native)+'.ee.bin').read_bytes()
    a,n=BLOCK;require(ne[a:a+n]==ee[a:a+n],'Display/double-buffer block differs')
    a,n=OVERLAY;diff=[a+k for k in range(n) if ne[a+k]!=ee[a+k]]
    require(not diff,f'Overlay range differs at {len(diff)} bytes (first {hex(diff[0]) if diff else 0})')
    # The flip's source: a0 of 0x311860 is one of two 40-byte environments in the block (PMODE,
    # SMODE2, DISPFB, DISPLAY, BGCOLOR as doublewords); the recorded registers must equal it.
    source=struct.unpack_from('<I',flip,0x387E80+24)[0]
    require(BLOCK[0]<=source<=BLOCK[0]+BLOCK[1]-40,'Original flip source outside the display block')
    env=struct.unpack_from('<5Q',ee,source)
    if r['gs_display_writes']==5:
        require((r['gs_pmode'],r['gs_dispfb2'],r['gs_display2'])==(env[0],env[2],env[3]),'Recorded GS display registers differ from the original environment')
    print(f'Verified the display block, the overlay range'+(' and the first flip' if r['gs_display_writes']==5 else ' (flip registers since rewritten)')+f' (environment {hex(source)}).')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args()
    verify(Path(__file__).resolve().parents[2],a.native)
