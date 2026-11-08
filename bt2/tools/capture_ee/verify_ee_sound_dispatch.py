"""Compare diagnostic generated EE registration/first wait and actual IOP reverse bind."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from capture_ee_reverse_bind import hook,IMPORT,CODE,SCRATCH
from verify_cri_autodma import require,words
from verify_sound_rpc import queued_sound_bind_completed

def verify(root,native,queued=False):
    env=None
    for suffix,terminal in [('ee-reverse-bind',True),('ee-reverse-bind-wait',False)]:
        folder=root/'work/pcsx2-capture'/('sound-init-before-absent-cards-'+suffix)
        m=json.loads((folder/'ee-reverse-bind.json').read_text(encoding='utf-8'));b=(folder/'ee-reverse-bind-iop.bin').read_bytes()
        require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(b).hexdigest()==m['iop_sha256'],'Reverse bind capture identity differs')
        current=[m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')];require(env is None or current==env,'Reverse bind environments differ');env=current
        require(b[CODE:CODE+len(hook(m['target'],terminal))]==hook(m['target'],terminal),'Actual bind observer differs')
        require(words(b,IMPORT,2)==(0x08000000|(CODE>>2),0),'Original BindRpc import hook differs')
        require(m['arguments']==[0x8A2D0,0x80000704,0] and m['return']==0 and m['client']==[0,2,0,0,0,0x3A6CC0,0,0,0,0x3A6D18],'Original reverse bind result/client differs')
        require(m['caller_ra']==0x8964C and m['gp']==0x91B10 and m['status_before']==m['status_after']==0x401,'Original bind caller context differs')
        if not terminal:require(m['wait_observed'] and m['wait_ra']==0x89554 and m['wait_gp']==m['gp'] and m['wait_sp']==m['sp']-64,'Original post-bind first sleep differs')
    folder=root/'work/pcsx2-capture/rpc-ee-server-wait-absent-cards';m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));b=(folder/'ee-ram.bin').read_bytes()
    require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(b).hexdigest()==m['capture_sha256'] and [m[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')]==env,'EE first-wait capture identity differs')
    address,trap,marker=capture_trap('rpc-ee-server-wait');stub=texture_stub('rpc-ee-server-wait')
    require(b[address:address+8]==trap and b[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(b,MARKER,1)==(marker,),'EE first-wait observer differs')
    r=json.loads(native.read_text(encoding='utf-8'));ee=Path(str(native)+'.ee.bin').read_bytes()
    if queued:
        require(queued_sound_bind_completed(r),'Queued earlier bind/retained sound ownership differs')
    else:
        require(not r.get('queued_sound_submissions',0) and not r.get('queued_sound_completions',0),'Queued execution supplied to diagnostic scope')
        require(r['ee_sound_probe_attempted']==1 and r['ee_sound_probe_waits']==1 and not r.get('native_iop_error',''),'Native sound handoff incomplete')
        require(r['selector']==0x32 and r['pc']==0x32EB88 and r['ee_sound_probe_parent_pc']==0x32EAF8 and r['main_thread_priority']==2,'Diagnostic parent/wait boundary differs')
    require(r['ee_sound_probe_frontier']==[0x32EB88,0x38BDF0,0x3A6C80,0x333098,0x3A6D00,0x10001],'Native first-wait context differs')
    for address,count in [(0x3A6D00,3),(0x3A6D18,3)]:require(words(ee,address,count)==words(b,address,count),'Generated queue/server registration differs')
    require(words(ee,0x3A6C90,1)==words(b,0x3A6C90,1)==(0x100250,),'Owned child saved exit differs')
    print('Verified '+('queued earlier bind/retained' if queued else 'diagnostic')+' generated sound registration/EE first wait, actual reverse bind return/client. The native sound service has no IOP thread; main scheduling/wake/callback completion remain unverified.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);p.add_argument('--queued',action='store_true',help='Validate earlier queued bind scope instead of late diagnostic');a=p.parse_args();verify(Path(__file__).resolve().parents[2],a.native,a.queued)
