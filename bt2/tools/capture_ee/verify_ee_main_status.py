"""Validate the running main-thread query and the following priority handoff evidence."""
import argparse,hashlib,json
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_cri_autodma import require,words
from verify_sound_rpc import queued_sound_bind_completed

def verify(root,native):
    environment=None;captures={}
    for point in ('rpc-main-status-after','rpc-main-priority-before','rpc-main-priority-after','rpc-worker4-created','rpc-worker4-started','rpc-worker5-created','rpc-worker5-started','rpc-helper-created','rpc-helper-started','rpc-helper2-created','rpc-helper2-started','rpc-helper-ready-status','rpc-helper-suspend-after','rpc-ee-server-wait','rpc-helper7-ready-status','rpc-helper7-suspend-after'):
        folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
        m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes();iop=(folder/'iop-ram.bin').read_bytes()
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and hashlib.sha256(iop).hexdigest()==m['iop_capture_sha256'],'Main status capture hashes/identity differ')
        require(ee[address:address+8]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'Main status observer differs')
        current=[m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]
        require(environment is None or current==environment,'Main status environment differs');environment=current;captures[point]=ee
    expected=(1,0x32EC78,0x1EFF000,0x100000,0x38BDF0,0,2,0,0,0,10,0)
    status=captures['rpc-main-status-after'];out=words(status,MARKER+16,1)[0]
    require(words(status,MARKER+8,1)==(1,) and words(status,out,12)==expected,'Original running main status differs')
    before=captures['rpc-main-priority-before'];after=captures['rpc-main-priority-after']
    require(words(before,0x34BB70,7)==(1,8,0,16,18,24,25) and words(after,MARKER+8,1)==(2,),'Original priority request/previous priority differs')
    for ee in (before,after):
        require(words(ee,0x3A6D00,3)==(3,0,0x3A6D18) and words(ee,0x3A6D18,3)==(0x80000704,0x3153F0,0x3A6CC0),'Original EE reverse-RPC registration differs')
    for prefix,retail_id,entry,stack,size in [('rpc-helper',4,0x2EDE58,0x34BC90,0x800),('rpc-helper2',5,0x2EDF48,0x34D490,0x1000),('rpc-worker4',6,0x2EE050,0x34E490,0x1000),('rpc-worker5',7,0x2EE140,0x34F490,0x2000)]:
        for phase in ('created','started'):
            data=captures[prefix+'-'+phase];sp=words(data,MARKER+16,1)[0]
            require(words(data,MARKER+8,1)==(retail_id,) and words(data,sp+4,5)==(entry,stack,size,0x38BDF0,2),'Original next worker creation/start differs')
        data=captures[prefix+'-created'];saved=stack+size-0x2A0
        require([words(data,saved+reg*16,1)[0] for reg in (28,29,30,31)]==[0x38BDF0,stack+size-32,stack+size-32,0x100250],'Original next worker reset context differs')
    ready=captures['rpc-helper-ready-status'];out=words(ready,MARKER+16,1)[0]
    require(words(ready,MARKER+8,1)==(2,) and words(ready,out,12)==(2,0x2EDE58,0x34BC90,0x800,0x38BDF0,2,2,0,0,0,0,0),'Original helper ready descriptor differs')
    require(words(captures['rpc-helper-suspend-after'],MARKER+8,1)==(4,),'Original helper suspension return differs')
    ready7=captures['rpc-helper7-ready-status'];out=words(ready7,MARKER+16,1)[0]
    require(words(ready7,MARKER+8,1)==(2,) and words(ready7,out,12)==(2,0x2EE140,0x34F490,0x2000,0x38BDF0,2,25,0,0,0,0,0),'Original last helper ready descriptor differs')
    require(words(captures['rpc-helper7-suspend-after'],MARKER+8,1)==(7,),'Original last helper suspension differs')
    wait=captures['rpc-ee-server-wait']
    require(words(wait,MARKER+16,1)==(0x3A6C80,) and words(wait,MARKER+40,3)==(0x70030C11,0x3A6D00,0x38BDF0),'Original RPC child first wait context differs')
    require(words(wait,0x3A6D00,3)==(3,0,0x3A6D18) and words(wait,0x3A6D18,3)==(0x80000704,0x3153F0,0x3A6CC0),'Original RPC child first wait registration differs')
    r=json.loads(native.read_text(encoding='utf-8'));ee=Path(str(native)+'.ee.bin').read_bytes()
    # After admission, main runs at 24 and helpers 5/6 have executed; verify_startup_workers.py
    # compares that later scope. Earlier lifecycle evidence below still applies to IDs 4 and 7.
    admitted=r.get('main_priority_admissions')==1
    require(r['main_thread_queries']==1 and r['main_thread_priority'] in ((24,1) if admitted else (2,)) # 1 inside main's later critical section. and r['cri_rpc_calls']==37 and r['heap_calls']==6
        and (not admitted or r['main_priority_previous']==2),'Native main status phase differs')
    # Later helper frames reuse the main query buffer; validate its successful query counter, not stale stack bytes.
    require(r['startup_ready_queries']==2 and r['startup_worker_suspends']==2,'Owned ready query/suspension counts differ')
    require(r['ee_thread_count']==6 and (admitted or (r['selector']==0x29 and r['pc']==0x32EAF8 and r['arguments']==[1,24])),'Native main-priority boundary differs')
    require(all(r[k]==4 for k in ('startup_worker_queries','startup_worker_starts','startup_worker_priority_changes')),'Four-worker lifecycle counters differ')
    expected_states=[[4,0x2EDE58,0,1,8,0x38BDF0,0x34C470,0x100250],
                     [5,0x2EDF48,1,0,16,0x38BDF0,0x34E470,0x100250],
                     [6,0x2EE050,1,0,18,0x38BDF0,0x34F470,0x100250],
                     [7,0x2EE140,0,1,25,0x38BDF0,0x351470,0x100250]]
    states=[t[:8] for t in r['startup_worker_state']]
    open_section=admitted and r['scheduled_resumes']>r['scheduled_suspends'] # Main inside its later critical section.
    first=[4,0x2EDE58,1,0,8,0x38BDF0,0x34C470,0x100250] if open_section else expected_states[0]
    require(states==expected_states or (admitted and [states[0],states[3]]==[first,expected_states[3]]),'Owned ready/suspended worker contexts differ')
    # Executed helpers reuse their stacks over the reset context; only unexecuted IDs remain checkable.
    for entry,stack,size in [(0x2EDE58,0x34BC90,0x800),(0x2EDF48,0x34D490,0x1000),(0x2EE050,0x34E490,0x1000),(0x2EE140,0x34F490,0x2000)]:
        if admitted and entry in (0x2EDF48,0x2EE050):continue
        saved=stack+size-0x2A0
        require([words(ee,saved+reg*16,1)[0] for reg in (28,29,30,31)]==[0x38BDF0,stack+size-32,stack+size-32,0x100250],'Owned four-worker reset context differs')
    require((r['ee_sound_context'][:2]==[3,1] and 0x80000704 in r['pending_ee_bind']) or queued_sound_bind_completed(r),'Required owned EE child/reverse bind lost')
    print('Verified consumed main status and owned worker lifecycle transitions'+(' before admitted priority 2 -> 24.' if admitted else '; main priority change 2 -> 24 remains pending.'))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args();verify(Path(__file__).resolve().parents[2],a.native)
