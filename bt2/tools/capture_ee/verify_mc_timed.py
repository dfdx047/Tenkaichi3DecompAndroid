"""Independent evidence for the bounded timed absent-card query, not timing equivalence."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA
from capture_sio2_dma import CODE,SCRATCH,pack
from verify_sound_rpc import require,words,completed_card_query

def verify(root,native):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    def load(folder,name):
        p=root/'work/pcsx2-capture'/folder;m=json.loads((p/(name+'.json')).read_text(encoding='utf-8'));b=(p/(name+'-iop.bin')).read_bytes()
        require(m['source_elf_sha256']==EXPECTED_SHA and all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Timed capture identity differs')
        require(hashlib.sha256(b).hexdigest()==m['iop_sha256'],'Timed capture RAM identity differs')
        settings=(p/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8')
        require('Slot1_Enable = false' in settings and 'Slot2_Enable = false' in settings,'Timed capture enables a card')
        return m,b
    from capture_iop_delay import hook as delay_hook
    delay,b=load('rpc-mc-query-before-iop-delay','iop-delay')
    require(delay['delay_us']==300 and delay['return']==0 and delay['status_before']==delay['status_after']==0x401,'Actual delay return/status differs')
    require(b[CODE:CODE+len(delay_hook(delay['target']))]==delay_hook(delay['target']),'Delay hook differs')
    from capture_card_alarm import hook as alarm_hook
    alarm,b=load('rpc-mc-query-before-card-alarm','card-alarm')
    require(alarm['clock']==[3686,0] and alarm['return']==0 and alarm['arguments']==[0x4D030+0x13F18,0x4D030+0x9F84,0],'Actual alarm differs')
    require(b[CODE:CODE+len(alarm_hook(alarm['target']))]==alarm_hook(alarm['target']),'Alarm hook differs')
    from capture_card_fifo import hook as fifo_hook,IMPORT as FIFO_IMPORT
    fifo,b=load('rpc-mc-query-before-card-fifo','card-fifo')
    require(b[CODE:CODE+len(fifo_hook())]==fifo_hook() and b[FIFO_IMPORT:FIFO_IMPORT+8]==pack([0x08000000|(CODE>>2),0]),'FIFO capture hook differs')
    require(fifo['transmit']==[0x81,0x52,0] and fifo['reply']==[255]*3 and fifo['command']==0xC0342 and fifo['status']==0x1D100,'Actual absent-card FIFO differs')
    r=json.loads(native.read_text(encoding='utf-8'));require(completed_card_query(r),'Native query lifecycle differs')
    # Only the requested primary result is defined for absent cards. Do not compare undefined stack fields.
    p=root/'work/pcsx2-capture/rpc-mc-callback-before-absent-cards';m=json.loads((p/'capture.json').read_text(encoding='utf-8'));ee=(p/'ee-ram.bin').read_bytes()
    require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Callback environment differs')
    settings=(p/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8')
    require('Slot1_Enable = false' in settings and 'Slot2_Enable = false' in settings,'Callback capture enables a card')
    require(r['mc_result_words'][0]==words(ee,0x3A8540,1)[0]==0xFFFFFFF5,'Actual absent-card primary result differs')
    require(words(ee,0x3A94C0,16)==(0x400,0x3A8540,0x80000008,0,0,0,0,0x3A6F80,0x8000000A,0,0,0,0,0,0,0),'Actual async reply packet differs')
    print('Five timed probes, actual delay/alarm/FIFO, native EE query completion and absent-card primary result verified. Physical scheduling/undefined secondary fields are not equated.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path,default=Path('work/bt2-generation/entry-stop.json'));a=p.parse_args();verify(a.root,a.native)
