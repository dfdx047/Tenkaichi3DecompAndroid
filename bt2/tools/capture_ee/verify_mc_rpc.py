"""Validate only the independently captured initial MCSERV exchange."""
import argparse,json,sys
from pathlib import Path
from verify_sound_rpc import capture,words,require,completed_card_query,SERVERS

def verify(root,native):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    before,_=capture(root,'rpc-mc-command-before',baseline)
    after,iop=capture(root,'rpc-mc-command-after',baseline)
    require(words(before,0x3A7000,12)==(0,)*12,'MCSERV input differs')
    require(words(after,0x3A8540,3)==(0,0x210,0x226),'MCSERV response differs')
    b=words(before,0x3A6F80,10);a=words(after,0x3A6F80,10)
    require(b[:5]==(0,39,10,0,0) and a[:5]==(0,40,10,0,0),'MCSERV cleanup differs')
    require(b[5]==a[5] and b[9]==a[9] and words(iop,a[9],1)==(0x80000400,),'MCSERV ownership differs')
    sys.path.insert(0,str(root/'tools/iop'))
    from inspect_irx import inspect,relocate
    module=(root/'rom/IRX/MCSERV.IRX').read_bytes();meta=inspect(module)
    base=a[9]-0x36F8;image=relocate(module,meta,base)
    require(all(iop[base+x:base+y]==image[x:y] for x,y in meta['code_ranges']),'Retail MCSERV code differs')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'))
        require(r['mc_rpc_binds']==1 and (completed_card_query(r) or (r['mc_rpc_calls']==1 and r['mc_result_words']==list(words(after,0x3A8540,3)))),'Native MCSERV exchange incomplete')
        descriptor,buffer=SERVERS[0x80000400];client=r['mc_client_words']
        pending=r['mc_rpc_calls']==1 and r['packet_words'][6:10]==[41,0x3A6F80,1,48]
        require(client[:5]==([0x203A97C0,41,0xFFFFFFFF,0,0] if pending else ([0,41,0xFFFFFFFF,0,0] if completed_card_query(r) else [0,40,10,0,0])) and client[5]==buffer and client[9]==descriptor,'Native MCSERV ownership/cleanup differs')
        if pending:
            query,_=capture(root,'rpc-mc-query-before',baseline)
            require(r['mc_input_words']==list(words(query,0x3A7000,12)),'Native pending query input differs')
            require(r['packet_words'][10:14]==[0x3A8540,4,1,descriptor] and client[7]==0x31DD60 and client[8]==0x3A7480,'Native async endpoint/callback differs')

    print('Initial MCSERV bind and real initialization/version response verified; card operations and timing unverified.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
