"""Compare admitted main priority and helper 5/6 first waits with original captures.

Original observer points supply the main request/previous priority, each helper's first
critical request/return, restore request and first SleepThread stack. Native evidence is the
normal startup report. Later wake, callback and timing behavior are not established here.
"""
import argparse,hashlib,json
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_sound_rpc import queued_sound_bind_completed,require,words

# Marker payload: +8 v0, +16 sp, +24 a0, +28 a1, +48 gp.
EXPECTED={
    'rpc-main-priority-after':{8:2,16:0x1FFECD0,24:1,28:24},
    'rpc-helper5-critical-before':{16:0x34E370,24:5,28:1,48:0x38BDF0},
    'rpc-helper5-critical-after':{8:16,16:0x34E370,48:0x38BDF0},
    'rpc-helper5-restore-before':{8:5,28:16,48:0x38BDF0},
    'rpc-helper5-first-wait':{16:0x34E430,48:0x38BDF0},
    'rpc-helper6-critical-before':{16:0x34F390,24:6,28:1,48:0x38BDF0},
    'rpc-helper6-critical-after':{8:18,16:0x34F390,48:0x38BDF0},
    'rpc-helper6-restore-before':{8:6,28:18,48:0x38BDF0},
    'rpc-helper6-first-wait':{16:0x34F430,48:0x38BDF0},
}

def original(root,point):
    folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
    m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point);stub=texture_stub(point)
    require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'],f'{point} capture identity differs')
    require(ee[address:address+8]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),f'{point} observer differs')
    for offset,value in EXPECTED[point].items():
        require(words(ee,MARKER+offset,1)==(value,),f'{point} field +{offset} differs')
    return ee

def native_scope(r):
    """Normal-startup scheduler outcome, independent of the later boundary."""
    require(queued_sound_bind_completed(r),'Normal startup lost queued sound ownership')
    require(r['main_priority_admissions']==1 and r['main_priority_previous']==2 and r['startup_scheduler_active']==0
        and r['startup_dispatch_parent_pc']==0x32EAF8,'Main priority admission differs')
    # Later owned VBlank wakes resume the parked helpers; each resume ends in another first-site sleep.
    # A boundary at a helper's pending NCMD read leaves that helper resumed but not yet re-parked.
    pending=r.get('selector')==0x77 and r.get('ncmd_ready_calls',0)==r.get('ncmd_read_calls',0)+1
    # In the frame phase main resumes worker 7 once (third dispatch); it then parks in its first sleep.
    worker7=r['startup_worker_dispatches']==3
    require(r['startup_worker_dispatches'] in (2,3) and r['startup_worker_waits']==r['startup_worker_dispatches']+r.get('worker_resumes',0)-pending,'Helper dispatch/wait count differs')
    # Every critical enter has its restore and every resume of helper 4 its suspend.
    # A later boundary may fall inside main's critical section: one resume/raise not yet undone.
    open_section=r['scheduled_resumes']-r['scheduled_suspends']
    require(open_section in (0,1) and r['scheduled_priority_changes']==2*r['scheduled_suspends']+open_section
        and (not open_section or (r['main_thread_priority']==1 and r.get('load_poll_idles',0))),'Critical-section ownership is unbalanced')
    require(r['cri_stream_transfers']>=2,'Helper stream submissions differ')
    state={t[0]:t for t in r['startup_worker_state']}
    require(state[4][2:5]==([1,0,8] if open_section else [0,1,8]) and (state[7][2:5]==[0,0,25] and state[7][8]==1 if worker7 else state[7][2:5]==[0,1,25]),
        'Suspended helper state differs') # ID 4 is resumed inside an open section; worker 7 waits once dispatched.
    require(state[5]==[5,0x2EDF48,0,0,16,0x38BDF0,0x34E430,0x2EE00C,1],'Helper 5 first wait differs')
    # At a pending NCMD read helper 6 is the running owned stack (ready, not waiting).
    require(state[6]==[6,0x2EE050,0,0,18,0x38BDF0,0x34F430,0x2EE100,1] or (pending and state[6][:6]==[6,0x2EE050,1,0,18,0x38BDF0] and state[6][8]==0),
        'Helper 6 first wait differs')

def verify(root,native):
    for point in EXPECTED:original(root,point)
    native_scope(json.loads(native.read_text(encoding='utf-8')))
    print('Verified admitted main priority 2->24 and helper 5/6 critical/restore ownership to their captured first waits. '
          'Later VBlank wakes are compared by verify_vblank_wake.py; timing is not equated.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args();verify(Path(__file__).resolve().parents[2],a.native)
