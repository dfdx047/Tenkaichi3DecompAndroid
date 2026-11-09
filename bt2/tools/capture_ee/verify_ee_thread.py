"""Validate the reached EE sound thread query/start; keep evidence bundles local."""
import argparse,json,struct
from pathlib import Path
from verify_sound_rpc import capture,words,require,queued_sound_bind_completed,SERVERS
from capture_ee import MARKER

def verify(root,output=None,native=None):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    before,_=capture(root,'sound-ee-thread-before',baseline)
    query,_=capture(root,'sound-ee-status-after',baseline)
    after,_=capture(root,'sound-ee-thread-after',baseline)
    require(words(before,MARKER+40,1)==(0x70030C11,) and words(after,MARKER+40,1)==(0x70030C11,),'Thread caller Status differs')
    require(words(query,MARKER+8,2)==(0x10,0) and words(query,MARKER+16,1)==(0x1FFEC60,) and words(query,MARKER+40,1)==(0x70020C11,),'Thread status-query return differs')
    info=words(query,0x1FFEC60,12)
    require(info==(0x10,0x315380,0x3A5CC0,0x1000,0x38BDF0,5,5,0,0,0,0,0),'Dormant sound thread descriptor differs')
    require(words(after,MARKER+8,2)==(3,0) and words(after,0x3681D4,1)==(3,),'StartThread return/ID differs')
    frame=0x3A5CC0+0x1000-0x2A0
    require(words(after,frame+28*16,1)==(0x38BDF0,) and words(after,frame+29*16,1)==(0x3A6CA0,) and words(after,frame+31*16,1)==(0x100250,),'Saved sound thread context differs')
    if output:
        output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(struct.pack('<14I',0x70030C11,3,*info))
    if native:
        r=json.loads(native.read_text(encoding='utf-8'))
        require(r['sound_thread_starts']==1 and ((r['ee_sound_context']==[3,1,0x315380,0x38BDF0,0x3A6CA0,0x100250,0,0x10001] and not r.get('queued_sound_submissions',0)) or queued_sound_bind_completed(r)),'Native ready/owned parked EE context differs')
        # After main's sound-bank load the SOUNDS client carries later calls (live ID, call semaphore).
        later=r.get('display_reset_phase',0)==13 and r.get('sound_rpc_calls',1)>1
        require(r['sounds_rpc_binds']==1 and (r['sounds_client_words'][:5]==[0,30,7,0,0] or (later and r['sounds_client_words'][1]>30))
            and r['sounds_client_words'][5] and r['sounds_client_words'][9],'Native SOUNDS owned bind differs')
    completed,_=capture(root,'sound-service-call-after',baseline)
    require(words(completed,MARKER+8,4)==(0,0,0x1FFED10,0),'SOUNDS first caller return differs')
    require(words(completed,0x635180,16)==(0,)*16 and words(completed,0x3A97E0,4)==(16,256,0x635180,64),'SOUNDS first command/result differs')
    if native:
        descriptor,buffer=SERVERS[0x02000004]
        # Later SOUNDS commands reuse the reply buffer (verify_sound_bank.py compares the last one).
        require((r['sounds_rpc_calls']==1 and r['sounds_result_words']==list(words(completed,0x635180,16)))
            or (r.get('display_reset_phase',0)==13 and r['sounds_rpc_calls']>1),'Native SOUNDS response differs')
        require(r['sounds_client_words'][5]==buffer and r['sounds_client_words'][9]==descriptor,'Native SOUNDS server ownership differs')
    dbc,dbc_iop=capture(root,'rpc-dbc-bind-after',baseline)
    dbc_client=words(dbc,0x3A03C0,10)
    require(words(dbc,MARKER+8,4)==(0,0,0x1FFEC40,0) and dbc_client==(0,31,7,0,0,0x7AB38,0,0,0,0x7AA10),'DBCMAN forward bind return differs')
    require(words(dbc_iop,dbc_client[9],1)==(0x80001300,),'DBCMAN forward registration differs')
    print('EE dormant query, IE/EIE publication, saved context and StartThread return validated'+(' against native '+('retained first-wait ownership.' if queued_sound_bind_completed(r) else 'ready-state transition.') if native else '.'))
    print('Startup context and retained first wait do not establish general scheduling timing or sound callbacks.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--output',type=Path);p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.output,a.native)
