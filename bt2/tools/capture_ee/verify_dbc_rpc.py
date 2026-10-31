"""Validate the reached DBCMAN request group against independent local captures."""
import argparse,json,sys
from pathlib import Path
from verify_sound_rpc import capture,words,require,SERVERS
from capture_ee import MARKER

def verify(root,native=None):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    before,_=capture(root,'rpc-dbc-command-before',baseline)
    after,iop=capture(root,'rpc-dbc-command-after',baseline)
    binds,bind_iop=capture(root,'rpc-dbc-binds-after',baseline)
    sema,_=capture(root,'rpc-dbc-sema-after',baseline)
    require(words(before,0x3A04C0,36)==(0,)*36,'DBCMAN input differs')
    require(words(after,MARKER+8,4)==(0,0,0x1FFEC10,0),'DBCMAN command return differs')
    require(words(after,0x3A04C0,36)==(0x310,)+(0,)*35,'DBCMAN result differs')
    addresses=(0x3A03C0,0x3A0410,0x3A0438,0x3A0460)
    services=(0x80001300,0x8000131C,0x8000131E,0x8000131F)
    clients=[words(binds,a,10) for a in addresses]
    for index,(client,service) in enumerate(zip(clients,services)):
        require(client[:5]==(0,32+index,7,0,0) and client[5] and client[9],'DBCMAN completed client differs')
        require(words(bind_iop,client[9],1)==(service,),'DBCMAN registration differs')
    require(words(sema,MARKER+8,4)==(7,0,0x1FFEC40,0),'DBCMAN semaphore return differs')
    require(words(sema,0x1FFEC40,6)==(0,127,1,0,0,0x381C28),'DBCMAN semaphore descriptor differs')
    setup_before,setup_before_iop=capture(root,'rpc-dbc-setup-before',baseline)
    setup,setup_iop=capture(root,'rpc-dbc-setup-after',baseline)
    require(words(setup_before,0x3A04C0,36)==(0x310,0x3A27C0)+(0,)*34,'DBCMAN setup input differs')
    require(words(setup,0x3A04C0,36)==(1,0x3A27C0)+(0,)*34,'DBCMAN setup result differs')
    require(words(setup_iop,0x77C30+0x2774,1)==(0x3A27C0,),'DBCMAN retained EE destination differs')
    sys.path.insert(0,str(root/'tools/iop'))
    from inspect_irx import inspect,relocate
    module=(root/'rom/IRX/DBCMAN.IRX').read_bytes();report=inspect(module)
    refbase=0x77C30;image=relocate(module,report,refbase)
    require(all(iop[refbase+a:refbase+b]==image[a:b] for a,b in report['code_ranges']),'Retail DBCMAN code differs')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'))
        require(r['dbc_rpc_binds']==4 and r['dbc_rpc_calls']==4,'Native DBCMAN request group incomplete')
        next_before,_=capture(root,'rpc-dbc-next-before',baseline)
        next_after,_=capture(root,'rpc-dbc-next-after',baseline)
        port_before,_=capture(root,'rpc-dbc-port1-before',baseline)
        slots_after,_=capture(root,'rpc-dbc-slots-after',baseline)
        for slot,source in enumerate((next_before,port_before)):
            expected=[0]*36;expected[0]=3;expected[1]=1;expected[2]=slot
            expected[10]=0x3B0B00+slot*0x180;expected[11]=expected[10]+0x80
            require(list(words(source,0x3A04C0,36))==expected,'Captured related request differs')
            expected[9]=slot
            require(list(words((next_after,slots_after)[slot],0x3A04C0,36))==expected,'Captured related result differs')
        require(r['dbc_result_words']==list(words(slots_after,0x3A04C0,36)),'Native completed related result differs')
        for index,service in enumerate(services):
            descriptor,buffer=SERVERS[service];client=r['dbc_client_words'][index]
            require(client[:5]==([0,38,8,0,0] if index==0 else list(clients[index][:5])) and client[5]==buffer and client[9]==descriptor,'Native DBCMAN client ownership differs')
    print('DBCMAN four binds and bounded four-request group verified'+(' against native execution.' if native else '.'))
    print('No recurring transfer, callback delivery, input device behavior or hardware timing inferred.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
