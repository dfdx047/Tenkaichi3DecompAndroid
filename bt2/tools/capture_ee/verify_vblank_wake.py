"""Compare VBLANK_START delivery, helper wakes and the first asynchronous load with captures.

Original observer points: `rpc-vblank-first` (first handler entry), `rpc-load-poll-idle`
(main's first idle call in its request poll) and `rpc-load-poll-done` (main leaves the poll).
Native evidence is the normal startup report after the load. VBlank timing is logical: one
delivery per idle call; the original count is recorded but not equated.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_sound_rpc import require,words

# Marker payload: +8 v0, +16 sp, +24 a0, +28 a1, +48 gp.
HANDLER={16:0x81FC0,24:2,48:0x38BDF0}
IDLE={8:2,16:0x1FFED20,48:0x38BDF0}
DONE={8:3,16:0x1FFED20,48:0x38BDF0}
GP=0x38BDF0;BUFFER_SLOT=GP-16096;LOADED=0x6880

def original(root,point,fields):
    folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
    m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point);stub=texture_stub(point)
    require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'],f'{point} capture identity differs')
    require(ee[address:address+8]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),f'{point} observer differs')
    for offset,value in fields.items():require(words(ee,MARKER+offset,1)==(value,),f'{point} field +{offset} differs')
    return ee

def native_scope(r):
    v=r['vblank_deliveries']
    # Main's display setup removes and re-adds the same handler (phase 13); no delivery happens there.
    # Each frame-phase GS reset (verify_frame_gs_reset.py) removes and re-adds it once more.
    registrations=(2 if r.get('display_reset_phase',0)>=13 else 1)+r.get('frame_gs_resets',0)
    require(r['vblank_handler_registrations']==registrations and r['vblank_enable_calls']==registrations and 2<=v<=21001 and r['load_poll_idles']+r.get('file_load_idles',0)+r.get('asset_poll_idles',0)+r.get('timer1_vblanks',int(r.get('timer1_stops',0)>0))+r.get('frame_vsync_deliveries',0)==v-1,
        'VBlank delivery/idle accounting differs')
    # Each delivery wakes sleeping helpers 5 and 6; each wake is resumed once and re-parks.
    # A boundary at helper 6's pending NCMD read: its readiness query completed, the read did not.
    pending=r.get('selector')==0x77 and r['ncmd_ready_calls']==r['ncmd_read_calls']+1
    require(r['worker_wakes']==2*v and r['worker_resumes']==r['worker_wakes']==r['worker_sleep_returns'],'Helper wake ownership differs')
    require(r['ncmd_read_calls']>=2 and r['ncmd_ready_calls']==r['ncmd_read_calls']+pending and r['scmd_calls']==r['ncmd_read_calls']-1,
        'Disc readiness/read/error-query sequence differs')
    require(not r.get('native_iop_error'),'Native IOP error present')

def verify(root,native):
    first=original(root,'rpc-vblank-first',HANDLER)
    idle=original(root,'rpc-load-poll-idle',IDLE)
    done=original(root,'rpc-load-poll-done',DONE)
    # One VBlank precedes the request; by the first idle call both helpers ran twice.
    require(words(first,0x34AED8,1)==(0,) and words(first,0x367408,1)==(0,) and words(first,0x3A0370,1)==(0,),'Original first VBlank phase differs')
    require(words(idle,0x34AED8,1)==(1,) and words(idle,0x34BBD0,1)==(2,) and words(idle,0x34BBD8,1)==(2,) and words(idle,0x366A58,1)==(0x201,),'Original idle phase differs')
    require(words(done,0x34AED8,1)==(120,),'Original load duration differs')
    r=json.loads(native.read_text(encoding='utf-8'));native_scope(r)
    ee=Path(str(native)+'.ee.bin').read_bytes()
    ob,nb=words(done,BUFFER_SLOT,1)[0],words(ee,BUFFER_SLOT,1)[0]
    require(ob==nb==0x635500 and ee[nb:nb+LOADED]==done[ob:ob+LOADED],'Loaded archive table differs from original')
    print(f"Verified first VBLANK_START before the request, {r['vblank_deliveries']} logical deliveries with owned helper wakes, "
          f"{r['ncmd_read_calls']} original IOP reads and a byte-identical first archive load (original: 120 VBlanks; timing not equated).")

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args();verify(Path(__file__).resolve().parents[2],a.native)
