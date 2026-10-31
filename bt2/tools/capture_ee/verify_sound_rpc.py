"""Compare the bounded SDRDRV bind/request against independent local evidence."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub

def completed_directory_diagnostics(r):
    names=['ZS2US_1.AFS','ZS2US_2.AFS','ZS2USOP.SFD','ZS2USED.SFD','LOGO_O.SFD']
    expected=['DVCI: "'+name+'" found.\n' for name in names]+['DVCI: Total 5 files.\n']
    # Every EE disc request (search, NCMD ready/read, SCMD) reaches the native disc service exactly once.
    return (r.get('console_messages')==len(r.get('console_messages_text',[])) and r.get('console_messages_text',[])[:6]==expected and r.get('disc_rpc_calls')==6 and r.get('ncmd_read_calls',0)>=1
        and r.get('native_disc_requests')==r['disc_rpc_calls']+r.get('ncmd_ready_calls',0)+r['ncmd_read_calls']+r.get('scmd_calls',0) and r.get('native_iop_error','')=='')

def require(condition,message):
    if not condition:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)
def capture(root,point,baseline):
    f=root/'work/pcsx2-capture'/point;m=json.loads((f/'capture.json').read_text(encoding='utf-8'));ee=(f/'ee-ram.bin').read_bytes();iop=(f/'iop-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point);stub=texture_stub(point)
    require(len(ee)==32*1024*1024 and len(iop)==2*1024*1024,'Sound capture lengths differ')
    require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and hashlib.sha256(iop).hexdigest()==m['iop_capture_sha256'],'Sound capture identity/hash differs')
    require(all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')) and m['capture_point'].startswith(point+':'),'Sound capture environment differs')
    require(ee[address:address+len(trap)]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'Sound capture hook/publication differs')
    return ee,iop

# Native host services answer IOP requests; only EE-visible state and EE-side counters are compared.
SERVERS={0x02000004:(0x1E5D90,0x1D16E0),0x80000400:(0x536F8,0x532B0),0x80001300:(0x62DE0,0x62F08),0x8000131C:(0x62E28,0x62F98),
    0x8000131E:(0x62E70,0x65028),0x8000131F:(0x62EB4,0x650B8),0x80000701:(0xA15E0,0xA1C30),0x90000200:(0x1B5D30,0x1B57C0),
    0x80000597:(0x433D0,0x441C8),0x80000595:(0x43418,0x43DB8),0x80000001:(0x47788,0x477D0),0x80000003:(0x48438,0x48480)}

def queued_sound_bind_completed(r):
    """Recognize the owned earlier bind/retained child, independent of later boundary."""
    return (r.get('queued_sound_submissions')==1 and r.get('queued_sound_completions')==1 and r.get('sounds_rpc_binds')==1
        and r.get('sound_thread_waiting')==1 and r.get('sound_dispatch_bind_id')==29
        and r.get('sound_dispatch_wait_ra')==0x332874 and r.get('sound_dispatch_wait_sp')==0x1FFECB0
        and r.get('ee_sound_probe_attempted')==0 and r.get('ee_sound_probe_waits')==0
        and r.get('ee_sound_probe_parent_pc')==0
        and r.get('ee_sound_context')==[3,0,0x32EB88,0x38BDF0,0x3A6C80,0x333098,0,0x10001]
        and r.get('ee_sound_probe_frontier')==[0x32EB88,0x38BDF0,0x3A6C80,0x333098,0x3A6D00,0x10001])

def completed_card_query(r):
    # MCSERV init plus the startup query, then one absent-card query per frame-phase query; each
    # EE MCSERV request reaches the native card service once and returns the absent-card result (-11).
    result=r.get('mc_result_words')
    return (r.get('mc_rpc_binds')==1 and r.get('mc_rpc_calls')==2
        and r.get('native_card_requests')==2+r.get('mc_frame_queries',0)
        and (result is None or result[:1]==[0xFFFFFFF5]) and queued_sound_bind_completed(r))

def later_iop_boundary(r):return completed_card_query(r)

def verify(root,native=None):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    bind,_=capture(root,'sound-bind-after',baseline);before,_=capture(root,'sound-init-before',baseline);after,iop=capture(root,'sound-init-after',baseline)
    require(words(bind,MARKER+8,4)==(0,0,0x1FFECF0,0),'Sound bind return differs')
    require(words(bind,0x3A5C80,10)==(0,27,7,0,0,0x8A260,0,0,0,0x89C10),'Sound bind client differs')
    require(words(before,MARKER+16,3)==(0x1FFECD0,0,1) and words(before,MARKER+28,1)==(0xE620,),'Sound request caller differs')
    require(words(after,MARKER+8,4)==(0,0,0x1FFECD0,0) and words(after,0x3A5C40,4)==(0,0,0,0),'Sound response differs')
    require(words(after,0x3A5C80,10)==(0,28,7,0,0,0x8A260,0x38BDF0,0,0x3A5C40,0x89C10),'Sound completed client differs')
    require(words(after,0x3A97E0,6)==(0xE620,64,0x3A5C40,16,1,0x89C10),'Sound request fields differ')
    sys.path.insert(0,str(root/'tools/iop'));from inspect_irx import inspect,relocate
    module=(root/'rom/IRX/SDRDRV.IRX').read_bytes();report=inspect(module);refbase=0x88630;owned=0xA0000
    image=relocate(module,report,refbase)
    require(all(iop[refbase+a:refbase+b]==image[a:b] for a,b in report['code_ranges']),'Reference SDRDRV code differs')
    handle=words(iop,refbase+0x14CC,1)[0];p=((handle>>7)<<2)+8
    require(p<=len(iop)-72 and words(iop,p,1)==((((handle>>1)&63)<<16)|0x7F01,),'Sound-created thread ownership differs')
    expected=(words(iop,p+48,1)[0]-refbase,words(iop,p+60,1)[0]-refbase,words(iop,p+56,1)[0],words(iop,p+4,1)[0]>>16,words(iop,p+64,1)[0])
    require(expected==(0xFF0,report['gp'],0x800,24,0x2000000) and words(iop,p+4,1)[0]&255==1,'Sound-created started helper thread parameters/state differ')
    second,second_iop=capture(root,'sound-service-bind-after',baseline)
    require(words(second,MARKER+8,4)==(0,0,0x1FFED20,0),'SOUNDS bind return differs')
    client=words(second,0x6351C0,10)
    require(client[:5]==(0,29,7,0,0) and client[6:9]==(0,0,0) and client[5] and client[9],'SOUNDS bind client differs')
    require(words(second,0x3A97DC,2)==(0x6351C0,0x02000004) and words(second_iop,client[9],1)==(0x02000004,),'Retail SOUNDS registration differs')
    thread_before,_=capture(root,'sound-ee-thread-before',baseline)
    thread,_=capture(root,'sound-ee-thread-after',baseline)
    require(words(thread,MARKER+8,2)==(3,0) and words(thread,0x3681D4,1)==(3,),'Retail EE StartThread return differs')
    require(words(thread_before,MARKER+40,1)==(0x70030C11,) and words(thread,MARKER+40,1)==(0x70030C11,),'Retail thread CP0 Status differs')
    console,_=capture(root,'deci2-after',baseline)
    require(words(console,MARKER+8,2)==(0xFFFFFFFF,0xFFFFFFFF) and words(console,MARKER+24,1)==(16,),'DECI2 return/subfunction differs')
    param=words(console,MARKER+28,1)[0];text=words(console,param,1)[0];end=console.find(b'\0',text,text+4096)
    require(end>=text,'DECI2 text is not terminated')
    require(console[text:end].decode('ascii'),'Empty DECI2 diagnostic')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'))
        require((r.get('sound_thread_starts')==1 or (r['target']==0x32FDA0 and r['source_pc']==0x315210)) and (r['console_messages']==0 or completed_directory_diagnostics(r)),'Native checked EE thread-start state differs')
        later=r.get('display_reset_phase',0)==13 # Main's later sound-driver commands follow the first call.
        require(r['sound_rpc_binds']==1 and (r['sound_rpc_calls']==1 or (later and r['sound_rpc_calls']>1)) and r.get('native_iop_error','')=='','Native sound lifecycle differs')
        descriptor,buffer=SERVERS[0x80000701]
        require(r['sound_client_words'][5]==buffer and r['sound_client_words'][9]==descriptor,'Native sound server mapping differs')
    print('SDRDRV bind, first command/result and started helper thread verified'+(' against the native EE client.' if native else '.'))
    print('Native SDRDRV is a host service: IOP code, helper thread, queue timing and audio playback are not compared.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
