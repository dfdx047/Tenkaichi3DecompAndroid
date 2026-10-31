"""Validate original DVD read contracts and optionally native read/completion evidence."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_disc_api import hook,BASE,CODE
from verify_disc_startup import require,words
from verify_disc_command import ENV
from verify_sound_rpc import completed_directory_diagnostics

def verify(root,disc,sector_bytes=None,native=None,workspace=0x1B0000):
    base=root/'work/pcsx2-capture'
    baseline=json.loads((base/'rpc-mc-callback-before-absent-cards/capture.json').read_text(encoding='utf-8'))
    captures={}
    for kind in ('read-rv','read-callback','read-callback-after','read-transfer','read-transfer-poll','read-alarm','read-alarm-cancel','sync','sync-after','error','read-disable','read-tail','read-completion','read-final','search-afs','search-afs2','search-op','search-ed','search-logo'):
        f=base/('rpc-ncmd-before-absent-cards-disc-api-'+kind);m=json.loads((f/'disc-api.json').read_text(encoding='utf-8'));b=(f/'disc-api-iop.bin').read_bytes()
        require(len(b)==2*1024*1024 and all(m[k]==baseline[k] for k in ENV) and hashlib.sha256(b).hexdigest()==m['iop_sha256'],'Read capture identity differs')
        ini=(f/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8');require('Slot1_Enable = false' in ini and 'Slot2_Enable = false' in ini,'Read evidence card profile differs')
        code=hook(m['target'],kind);require(b[CODE:CODE+len(code)]==code and words(b,0x1D0000,1)==(1,) and list(words(b,0x1D0020,4))==m['arguments'],'Read observer/publication differs')
        if kind=='read-callback-after':require(words(b,CODE+0x100,4)==(0x27BDFFE8,0x3C040004,0x08000000|((BASE+0xB08)>>2),0),'Original callback trampoline differs')
        captures[kind]=(m,b)
    m,b=captures['read-rv'];sp=words(b,0x1D001C,1)[0]
    require(m['arguments']==[0xE2A,2,0x34138,0x43220] and m['result']==1 and words(b,sp+16,2)==(2,BASE+0xB00),'Actual RV read contract differs')
    require(b[0x43220:0x43224]==bytes([0,1,0,0]),'Actual DVD read mode differs')
    with disc.open('rb') as f:f.seek(0xE2A*2048);payload=f.read(4096)
    require(len(payload)==4096,'Local payload truncated')
    for kind in ('read-callback','read-callback-after'):
        m,b=captures[kind];raw=b[0x34138:0x34138+4128]
        for sector in range(2):
            record=raw[sector*2064:(sector+1)*2064];expected_id=(0x20030000+0xE2A+sector).to_bytes(4,'big')
            require(record[:4]==expected_id and record[4:12]==bytes(8) and record[12:2060]==payload[sector*2048:(sector+1)*2048] and record[2060:]==bytes(4),'Observed DVD framing/payload differs')
        require(m['result']==(None if kind=='read-callback' else 0),'Callback entry/return publication differs')
        require(words(b,BASE+0x5550,1)==(0x7B9A09,) and words(b,BASE+0x5568,1)==(0x3D380,),'Captured callback ownership differs')
    m,b=captures['read-transfer'];require(m['arguments'][:2]==[0x43110,2] and m['result']>0,'Actual transfer submission differs')
    require(words(b,0x43110,8)==(0x34144,0x39DE80,2048,0,0x34954,0x39E680,2048,0),'Actual two-source DMA differs')
    require(b[0x34144:0x34944]==payload[:2048] and b[0x34954:0x35154]==payload[2048:],'Actual transfer source payload differs')
    for kind,result in [('read-alarm',0),('read-alarm-cancel',0),('sync',0),('sync-after',0),('error',0),('read-disable',0xFFFFFF99),('read-transfer-poll',0)]:
        require(captures[kind][0]['result']==result,'Actual read service return differs: '+kind)
    m,b=captures['read-alarm'];require(m['arguments'][:3]==[BASE+0x5548,BASE+0x8C8,BASE+0x5548] and words(b,BASE+0x5548,2)==(552960000,0),'Read timeout ownership differs')
    require(captures['read-alarm-cancel'][0]['arguments'][:2]==[BASE+0x8C8,BASE+0x5548],'Read timeout cancellation differs')
    require(captures['sync'][0]['arguments'][0]==32 and captures['sync-after'][0]['arguments'][0]==5,'Read synchronization modes differ')
    m,b=captures['read-disable'];require(m['arguments'][0]==35 and words(b,m['arguments'][1],1)==(0xFFFFFF99,),'Completed DMA3 state differs')
    for kind,value in [('read-tail',0),('read-final',4096)]:
        m,b=captures[kind];require(m['arguments'][:2]==[BASE+0x5508,1] and words(b,BASE+0x5508,4)==(BASE+0x5818,0x369580,4,0) and words(b,BASE+0x5818,1)==(value,),'Progress submission differs: '+kind)
    m,b=captures['read-completion'];require(m['arguments'][:2]==[BASE+0x54F8,1] and words(b,BASE+0x54F8,4)==(BASE+0x5580,0x3694C0,144,0) and words(b,BASE+0x5580,36)==(0,0,0x39DE80,0x39EE80)+tuple([0]*32),'Read completion buffer differs')
    if sector_bytes is not None:require(sector_bytes.read_bytes()==payload,'Native read-only backend bytes differ')
    m,b=captures['search-afs'];require(m['arguments'][:3]==[0x441C8,0x441EC,0] and m['result']==1 and words(b,0x441C8,2)==(0x1FC26,0x419B9800) and b[0x441EC:0x441EC+20]==b"\\DATA\\ZS2US_1.AFS;1\0",'Actual first archive lookup differs')
    for kind,name,lba,size in [('search-afs2','ZS2US_2.AFS',0xA2F9A,0x60014000),('search-op','ZS2USOP.SFD',0x14245,0x5CF0800),('search-ed','ZS2USED.SFD',0xE2B,0x9A0D000),('search-logo','LOGO_O.SFD',0x8B1,0x2BC800)]:
        m,b=captures[kind];path=('\\DATA\\'+name+';1\0').encode();require(m['arguments'][:3]==[0x441C8,0x441EC,0] and m['result']==1 and words(b,0x441C8,2)==(lba,size) and b[0x441EC:0x441EC+len(path)]==path,'Directory metadata batch differs: '+kind)
    m,b=captures['search-afs']
    f=base/'rpc-ncmd-before-absent-cards-disc-api-search-afs';retail_ee=(f/'disc-api-ee.bin').read_bytes();require(hashlib.sha256(retail_ee).hexdigest()==m['ee_sha256'],'Archive lookup EE evidence hash differs')
    tokenized=retail_ee[0x39DE80:0x39EE80];require(tokenized==bytes(0 if n in (11,24,37,50,62,63) else value for n,value in enumerate(payload)),'Retail directory tokenization differs')
    if native is not None:
        r=json.loads(native.read_text(encoding='utf-8'));require(r.get('ncmd_read_calls')==1 and r.get('disc_read_calls')==1 and r.get('disc_read_callbacks')==1 and r.get('disc_read_dma_submissions')==4 and r.get('native_iop_error')=='','Native read/completion frontier differs')
        if r.get('disc_rpc_calls')==6:
            require(completed_directory_diagnostics(r),'Completed directory diagnostics differ')
            executable=(root/'rom/SLUS_214.41').read_bytes();require(b'DVCI: "%s" found.\n\0' in executable and b'DVCI: Total %d files.\n\0' in executable,'Original directory diagnostic formats differ')
        ee=Path(str(native)+'.ee.bin').read_bytes();iop=Path(str(native)+'.iop.bin').read_bytes()
        require(len(ee)==32*1024*1024 and ee[0x39DE80:0x39EE80]==tokenized,'Generated directory tokenization differs from local ISO')
        require(words(ee,0x369580,1)==(4096,) and words(ee,0x3683F4,1)==(0,) and words(ee,0x3694C0,36)==(0,0,0x39DE80,0x39EE80)+tuple([0]*32),'Native progress/generated callback effects differ')
        require(words(ee,0x369590,10)==(0,46,0xFFFFFFFF,0,0,0x43DB8,0x38BDF0,0x31ECF8,0x3694C0,0x43418),'Native async cleanup differs')
        require(iop[workspace+0x2254:workspace+0x2254+2048]==payload[:2048] and iop[workspace+0x2A64:workspace+0x2A64+2048]==payload[2048:],'Native owned source buffer differs')
        print('Native original IOP read, ISO bytes, ordered secondary transfers and generated EE callback/cleanup independently verified.')
    print('Original read acceptance, callback return, framing, timeout, synchronization and transport independently verified.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--disc',type=Path,required=True);p.add_argument('--sector-bytes',type=Path);p.add_argument('--native',type=Path);p.add_argument('--workspace',type=lambda x:int(x,0),default=0x1B0000);a=p.parse_args();verify(a.root,a.disc,a.sector_bytes,a.native,a.workspace)
