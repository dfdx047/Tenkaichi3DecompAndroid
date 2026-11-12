import test, {afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {createSessionClass} from '../session.mjs';

const PROTOCOL=0x54540009,MAGIC=0x314b4c54;
const liveFixtures=new Set();
afterEach(()=>{for(const f of liveFixtures)f.clean();liveFixtures.clear();});
class Socket {
    constructor(){this.messages=[];this.closed=false;this.listeners=new Map();}
    accept(){}
    addEventListener(type,callback){this.listeners.set(type,callback);}
    send(message){if(this.closed)throw new Error('closed');this.messages.push(message instanceof ArrayBuffer?message.slice(0):JSON.parse(message));}
    close(){this.closed=true;}
    clear(){this.messages=[];}
}
function request(command,epoch=7){
    const packet=new ArrayBuffer(512),v=new DataView(packet);
    v.setUint32(0,PROTOCOL,true);v.setUint32(4,command,true);v.setUint32(8,epoch,true);return packet;
}
function fixture(ids=[101,102,103]) {
    const state={phase:4,epoch:7,host:101,start:0,input:0,deadline:0,leave:0,abort:0,pacing:0};
    const memory={buffer:new ArrayBuffer(65536)},input=new Uint8Array(memory.buffer,0,512),output=new Uint8Array(memory.buffer,512,512);
    const api={memory,Kernel_Input:()=>0,Kernel_Output:()=>512,
        Kernel_Phase:()=>state.phase,Kernel_Epoch:()=>state.epoch,Kernel_Host:()=>state.host,
        Kernel_Seat:id=>ids.indexOf(id),Kernel_Seed(){},Kernel_Pacing:on=>{state.pacing=on;},
        Kernel_Command:id=>{
            const v=new DataView(input.buffer),cmd=v.getUint32(4,true);
            if(cmd===9){assert.equal(id,state.host);assert.equal(state.phase,4);++state.start;state.phase=5;}
            if(cmd===11)++state.input;
            if(cmd===18){state.phase=3;++state.epoch;}
            return 0;
        },
        Kernel_DeadlineFrame(){++state.deadline;},Kernel_TakeFrame:()=>0,
        Kernel_SyncSerial:()=>0,Kernel_SyncKind:()=>0,
        Kernel_Leave(id){++state.leave;if(id===state.host)state.phase=6;},
        Kernel_Abort(){++state.abort;state.phase=6;},
        Kernel_Snapshot(member,result){
            output.fill(0);const v=new DataView(output.buffer,512,512);
            v.setUint32(0,PROTOCOL,true);v.setUint32(4,100,true);v.setUint32(8,state.epoch,true);
            v.setUint32(12,member,true);v.setUint32(16,result,true);v.setUint32(24,state.host,true);v.setUint32(28,state.phase,true);
            v.setUint32(32,2,true);v.setUint32(40,1,true);v.setUint32(472,ids.length,true);
            for(const [seat,id]of ids.entries()){
                const o=72+seat*40;v.setUint32(o,id,true);v.setUint32(o+24,seat,true);v.setUint32(o+32,7,true);
                output.set(new TextEncoder().encode('Player'+id),o+4);
            }
        }};
    const original=WebAssembly.Instance;WebAssembly.Instance=class{constructor(){this.exports=api;}};
    let session;try{const Session=createSessionClass(null);session=new Session({},{});}finally{WebAssembly.Instance=original;}
    session.code='ABCDEF';const game=new Map(),link=new Map();
    for(const id of ids){
        const ws=new Socket();game.set(id,ws);session.peers.set(ws,{id,joined:true,features:7,window:Date.now(),messages:0,chats:0,ack:0});
        const wire=new Socket();link.set(id,wire);session.tagLink.peers.set(wire,{id,window:Date.now(),messages:0,controls:0,ready:null,hello:''});
    }
    session.tagLink.refresh();
    function ready(id){
        const h=session.tagLink,peers=h.members.filter(p=>p.id!==id&&(id===h.host||p.id===h.host)).map(p=>({id:p.id,rttMs:3}));
        h.message(link.get(id),JSON.stringify({type:'ready',version:1,epoch:h.epoch,generation:h.generation,peers}));
    }
    function json(id,type,fields={}){
        const h=session.tagLink;h.message(link.get(id),JSON.stringify({type,version:1,epoch:h.epoch,generation:h.generation,...fields}));
    }
    function binary(id,command,frame=0,fields={}){
        const h=session.tagLink,packet=new ArrayBuffer(528),v=new DataView(packet);
        v.setUint32(0,MAGIC,true);v.setUint32(4,parseInt(h.generation.slice(8),16),true);v.setUint32(8,parseInt(h.generation.slice(0,8),16),true);
        v.setUint32(12,999999,true);v.setUint32(16,PROTOCOL,true);v.setUint32(20,command,true);v.setUint32(24,fields.epoch??h.epoch,true);
        v.setUint32(28,frame,true);if(fields.value!==undefined)v.setUint32(40,fields.value,true);
        h.message(link.get(id),packet);return packet;
    }
    async function prepare(){
        for(const id of ids)ready(id);
        session.message(game.get(101),request(9));
        // WebCrypto completes on a worker, not necessarily by the next event
        // loop turn on a loaded CI runner. Await the protocol event we assert.
        const deadline=Date.now()+1000;
        while(!session.tagLink.pending?.digest&&Date.now()<deadline)
            await new Promise(resolve=>setTimeout(resolve,2));
        const pending=session.tagLink.pending;assert.ok(pending?.digest);return pending;
    }
    async function commit(){const pending=await prepare();for(const id of ids)json(id,'prepared',{digest:pending.digest});assert.ok(session.tagLink.active);return pending;}
    function clear(){for(const ws of [...game.values(),...link.values()])ws.clear();}
    function clean(){session.stopClock();session.tagLink.failPending('test-completed',false);}
    const f={session,state,game,link,ready,json,binary,prepare,commit,clear,clean};
    liveFixtures.add(f);return f;
}

test('game tickets are separate, feature-gated and checked against current membership',async()=>{
    const f=fixture(),ws=f.game.get(102),peer=f.session.peers.get(ws);f.clear();peer.features=3;
    f.session.message(ws,request(23));assert.equal(ws.messages.length,0);
    peer.features=7;f.session.message(ws,request(23));
    assert.equal(new DataView(ws.messages[0]).getUint32(4,true),105);
    const token=new TextDecoder().decode(new Uint8Array(ws.messages[0],16,64));assert.match(token,/^[a-f0-9]{64}$/);
    f.session.message(ws,request(20));assert.notEqual(peer.voiceToken,token);
    let accepted=0;f.session.tagLink.accept=async(_request,id)=>{accepted=id;return new Response('ok');};
    const req=cap=>new Request('https://example.test/v4/taglink/ABCDEF',{headers:{Upgrade:'websocket',Authorization:'Bearer '+cap}});
    assert.equal((await f.session.fetch(req('0'.repeat(64)))).status,403);
    assert.equal((await f.session.fetch(req(token))).status,200);assert.equal(accepted,102);
    peer.features=3;assert.equal((await f.session.fetch(req(token))).status,403);f.clean();
});

test('signaling is star-only, generation-bound and derives sender from TLS membership',()=>{
    const f=fixture();f.clear();
    f.json(102,'signal',{to:103,from:101,signal:{type:'offer',sdp:'sdp'}});assert.equal(f.link.get(103).messages.length,0);
    f.json(102,'signal',{to:101,from:103,signal:{type:'offer',sdp:'sdp'}});
    assert.equal(f.link.get(101).messages[0].from,102);
    f.json(101,'signal',{to:102,signal:{type:'answer',sdp:'sdp'}});assert.equal(f.link.get(102).messages[0].from,101);
    const before=f.link.get(101).messages.length;
    f.json(102,'signal',{epoch:6,to:101,signal:{type:'candidate',candidate:'candidate',mid:'game'}});
    f.json(102,'signal',{generation:'0000000000000000',to:101,signal:{type:'offer',sdp:'sdp'}});
    f.json(102,'signal',{to:101,signal:{type:'offer',sdp:'x'.repeat(14001)}});
    assert.equal(f.link.get(101).messages.length,before);f.clean();
});

test('auxiliary accept announces destination first and then lets the host offer',async()=>{
    const f=fixture(),h=f.session.tagLink,host=f.link.get(101),generation=h.generation;
    h.peers.delete(f.link.get(102));h.peers.delete(f.link.get(103));h.refresh();
    assert.deepEqual(host.messages.at(-1).peers,[]);
    f.json(101,'ready',{peers:[]});assert.equal(h.peers.get(host).ready,null); // Full game roster is still required.
    let target;
    const ResponseOriginal=globalThis.Response,PairOriginal=globalThis.WebSocketPair;
    globalThis.Response=class{constructor(_body,options){Object.assign(this,options);}};
    globalThis.WebSocketPair=class{constructor(){target=new Socket();return {0:new Socket(),1:target};}};
    const sendOriginal=host.send.bind(host);let offered=false;
    host.send=message=>{
        sendOriginal(message);const m=JSON.parse(message);
        if(!offered&&m.type==='hello'&&m.peers.some(p=>p.id===102)){
            offered=true;f.json(101,'signal',{to:102,signal:{type:'offer',sdp:'sdp'}});
        }
    };
    try{
        const response=await h.accept(new Request('https://example.test/v4/taglink/ABCDEF'),102);
        assert.equal(response.status,101);assert.equal(offered,true);
        assert.equal(target.messages[0].type,'hello');assert.equal(target.messages[0].self,102);
        assert.equal(target.messages[1].type,'signal');assert.equal(target.messages[1].from,101);
        assert.deepEqual(host.messages.at(-1).peers.map(p=>p.id),[102]);assert.equal(h.generation,generation);
    }finally{globalThis.Response=ResponseOriginal;globalThis.WebSocketPair=PairOriginal;f.clean();}
});

test('precommit link loss withdraws readiness and peer announcement without changing generation',()=>{
    const f=fixture(),h=f.session.tagLink,generation=h.generation;for(const id of [101,102,103])f.ready(id);
    h.close(f.link.get(102));
    assert.equal(h.peers.get(f.link.get(101)).ready,null);assert.equal(h.peers.get(f.link.get(103)).ready,null);
    assert.deepEqual(f.link.get(101).messages.at(-1).peers.map(p=>p.id),[103]);assert.equal(h.generation,generation);f.clean();
});

test('server advertises direct support before commit without selecting the route',()=>{
    const f=fixture();f.clear();const packet=request(22);new DataView(packet).setUint32(12,7,true);
    f.session.message(f.game.get(102),packet);
    const capabilities=f.game.get(102).messages.find(m=>m instanceof ArrayBuffer&&new DataView(m).getUint32(4,true)===104);
    const v=new DataView(capabilities);assert.equal(v.getUint32(44,true),4);assert.equal(v.getUint32(24,true),0);
    // An older server leaves +44 zero; a client can keep the existing path and
    // avoid sending command 23 to a kernel which does not implement it.
    assert.equal(new DataView(new ArrayBuffer(512)).getUint32(44,true)&4,0);f.clean();
});

test('prepare snapshot is canonical SHA256 and all members commit exactly one kernel start',async()=>{
    const f=fixture(),pending=await f.prepare();
    assert.equal(f.state.start,0);assert.equal(f.session.clockTimer,null);
    const prepare=f.link.get(101).messages.find(m=>m.type==='prepare');
    const bytes=Buffer.from(prepare.snapshot,'hex');assert.equal(bytes.length,512);
    assert.equal(bytes.readUInt32LE(12),0);assert.equal(bytes.readUInt32LE(508),0);assert.equal(bytes.readUInt32LE(28),4);
    assert.equal(createHash('sha256').update(bytes).digest('hex'),prepare.digest);
    f.json(101,'prepared',{digest:pending.digest});f.json(102,'prepared',{digest:'0'.repeat(64)});assert.equal(f.state.start,0);
    f.json(102,'prepared',{digest:pending.digest});assert.equal(f.state.start,0);
    f.json(103,'prepared',{digest:pending.digest});assert.equal(f.state.start,1);assert.equal(f.session.tagLink.active,true);
    f.json(103,'prepared',{digest:pending.digest});assert.equal(f.state.start,1);
    assert.equal(f.session.clockTimer,null);f.session.startClock();f.session.clockTick();assert.equal(f.state.deadline,0);assert.equal(f.session.clockTimer,null);
    const capabilities=f.game.get(102).messages.filter(m=>m instanceof ArrayBuffer&&new DataView(m).getUint32(4,true)===104).at(-1);
    const caps=new DataView(capabilities);assert.equal(caps.getUint32(12,true),1);assert.equal(caps.getUint32(24,true),1);
    assert.equal(caps.getUint32(28,true),101);assert.equal(caps.getUint32(40,true),7);
    assert.equal(caps.getUint32(44,true),4);
    f.session.message(f.game.get(102),request(11));assert.equal(f.state.input,0);f.clean();
});

test('missing direct readiness keeps the original DO start and paced clock',()=>{
    const f=fixture();f.ready(101);f.ready(102);
    f.session.message(f.game.get(101),request(9));assert.equal(f.state.start,1);assert.equal(f.session.tagLink.active,false);
    assert.ok(f.session.clockTimer);assert.equal(f.session.paced,true);f.clean();
});

test('repeated START during prepare cannot start the DO clock or commit early',async()=>{
    const f=fixture(),pending=await f.prepare();
    for(let i=0;i<3;++i)f.session.message(f.game.get(101),request(9));
    assert.equal(f.session.tagLink.pending,pending);assert.equal(f.state.start,0);assert.equal(f.session.clockTimer,null);
    f.json(101,'prepared',{digest:pending.digest});f.json(102,'prepared',{digest:pending.digest});
    f.session.message(f.game.get(101),request(9));
    assert.equal(f.state.start,0);assert.equal(f.session.clockTimer,null);
    f.json(103,'prepared',{digest:pending.digest});assert.equal(f.state.start,1);assert.equal(f.session.tagLink.active,true);
    assert.equal(f.session.clockTimer,null);f.clean();
});

test('stale readiness falls back while renewed readiness can negotiate',async()=>{
    const stale=fixture();for(const id of [101,102,103])stale.ready(id);
    stale.session.tagLink.peers.get(stale.link.get(102)).ready.at=Date.now()-2500;
    stale.session.message(stale.game.get(101),request(9));
    assert.equal(stale.state.start,1);assert.equal(stale.session.tagLink.active,false);assert.ok(stale.session.clockTimer);stale.clean();
    const fresh=fixture();for(const id of [101,102,103])fresh.ready(id);
    fresh.session.tagLink.peers.get(fresh.link.get(102)).ready.at=Date.now()-2500;fresh.ready(102);
    await fresh.prepare();
    assert.ok(fresh.session.tagLink.pending?.digest);assert.equal(fresh.state.start,0);assert.equal(fresh.session.clockTimer,null);fresh.clean();
});

test('link failure before commit cancels negotiation then starts DO once',async()=>{
    const f=fixture(),pending=await f.prepare();f.session.tagLink.close(f.link.get(103));
    await new Promise(resolve=>setImmediate(resolve));assert.equal(f.state.start,1);assert.equal(f.session.tagLink.active,false);assert.ok(f.session.clockTimer);
    f.json(101,'prepared',{digest:pending.digest});f.json(102,'prepared',{digest:pending.digest});assert.equal(f.state.start,1);
    assert.ok(f.link.get(101).messages.some(m=>m.type==='cancel'&&m.reason==='link-disconnected'));f.clean();
});

test('negotiation timeout falls back without a second direct attempt in the epoch',async()=>{
    const f=fixture();await f.prepare();
    await new Promise(resolve=>setTimeout(resolve,3050));
    assert.equal(f.state.start,1);assert.equal(f.session.tagLink.active,false);assert.equal(f.session.tagLink.attemptedEpoch,7);
    assert.ok(f.link.get(101).messages.some(m=>m.type==='cancel'&&m.reason==='negotiation-timeout'));f.clean();
});

test('binary fallback authenticates input sender and accepts host frames only once',async()=>{
    const f=fixture();await f.commit();f.clear();
    f.binary(102,11,0);const input=f.link.get(101).messages.at(-1);assert.equal(new DataView(input).getUint32(12,true),102);
    assert.equal(f.link.get(103).messages.length,0);
    f.binary(102,107,77);assert.equal(new DataView(f.link.get(101).messages.at(-1)).getUint32(20,true),107);
    f.binary(101,108,102);assert.equal(new DataView(f.link.get(102).messages.at(-1)).getUint32(20,true),108);
    f.clear();f.binary(102,101,0);assert.equal(f.link.get(103).messages.length,0);
    f.binary(101,101,0);assert.equal(f.link.get(103).messages.length,1);
    f.binary(101,101,0);assert.equal(f.link.get(103).messages.length,1);
    f.binary(101,101,2);assert.equal(f.link.get(103).messages.length,1);
    f.binary(101,101,1,{epoch:6});assert.equal(f.link.get(103).messages.length,1);
    f.binary(101,101,1);assert.equal(f.link.get(103).messages.length,2);
    const before=f.link.get(101).messages.length;
    f.binary(102,11,100000);f.binary(102,11,1,{epoch:6});assert.equal(f.link.get(101).messages.length,before);
    f.clean();
});

test('conflicting host retransmission aborts explicitly instead of switching authority',async()=>{
    const f=fixture();await f.commit();f.binary(101,101,0);f.binary(101,101,0,{value:1});
    assert.equal(f.state.abort,1);assert.equal(f.state.phase,6);assert.equal(f.session.tagLink.active,false);assert.equal(f.session.clockTimer,null);f.clean();
});

test('link reconnect preserves the generation; game member loss preserves active host',async()=>{
    const f=fixture();await f.commit();const generation=f.session.tagLink.generation;
    f.session.tagLink.close(f.link.get(102));f.session.tagLink.refresh();
    assert.equal(f.session.tagLink.generation,generation);assert.equal(f.session.tagLink.active,true);
    assert.ok(f.link.get(101).messages.at(-1).peers.some(p=>p.id===102));
    f.session.close(f.game.get(103));assert.equal(f.state.leave,1);assert.equal(f.state.abort,0);
    assert.equal(f.session.tagLink.active,true);assert.equal(f.session.tagLink.generation,generation);f.clean();
});

test('game host departure aborts the active round and clears the DO clock',async()=>{
    const f=fixture();await f.commit();f.session.close(f.game.get(101));
    assert.equal(f.state.abort,1);assert.equal(f.state.leave,0);assert.equal(f.state.phase,6);
    assert.equal(f.session.tagLink.active,false);assert.equal(f.session.clockTimer,null);f.clean();
});

test('next-round control returns to room authority with a new generation',async()=>{
    const f=fixture();await f.commit();const generation=f.session.tagLink.generation;
    f.session.message(f.game.get(101),request(18));assert.equal(f.state.epoch,8);assert.equal(f.session.tagLink.active,false);
    assert.notEqual(f.session.tagLink.generation,generation);assert.equal(f.state.pacing,0);assert.equal(f.session.clockTimer,null);f.clean();
});

test('a stale prepare cannot commit after the room changes epoch',async()=>{
    const f=fixture(),pending=await f.prepare();f.state.epoch=8;f.state.phase=2;f.session.broadcast();
    for(const id of [101,102,103])f.json(id,'prepared',{epoch:7,generation:pending.generation,digest:pending.digest});
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(f.state.start,0);assert.equal(f.session.tagLink.active,false);assert.equal(f.session.tagLink.pending,null);f.clean();
});

test('frame replay history remains bounded and an old stale frame is ignored',async()=>{
    const f=fixture();await f.commit();const h=f.session.tagLink,host=f.link.get(101),peer=h.peers.get(host);
    for(let frame=0;frame<515;++frame){
        const packet=new ArrayBuffer(528),v=new DataView(packet);
        v.setUint32(0,MAGIC,true);v.setUint32(4,parseInt(h.generation.slice(8),16),true);v.setUint32(8,parseInt(h.generation.slice(0,8),16),true);
        v.setUint32(16,PROTOCOL,true);v.setUint32(20,101,true);v.setUint32(24,h.epoch,true);v.setUint32(28,frame,true);
        h.binary(host,peer,packet);
    }
    assert.equal(h.history.size,512);assert.equal(h.lastFrame,514);assert.equal(h.history.keys().next().value,3);
    f.binary(101,101,0,{value:123});assert.equal(f.state.abort,0);assert.equal(h.lastFrame,514);f.clean();
});

test('room shutdown closes auxiliary links and cannot leave a prepare timeout alive',async()=>{
    const f=fixture();await f.prepare();f.session.abort('Room expired');
    await new Promise(resolve=>setImmediate(resolve));assert.equal(f.state.start,0);
    assert.equal(f.session.tagLink.peers.size,0);assert.equal(f.session.tagLink.pending,null);assert.equal(f.session.peers.size,0);f.clean();
});

test('only the active authenticated host can resume relay history in the same generation',async()=>{
    const f=fixture();await f.commit();f.binary(101,101,0);const h=f.session.tagLink,generation=h.generation,digest=h.digest;
    f.clear();f.json(102,'resume',{firstFrame:900});
    f.json(101,'resume',{generation:'0000000000000000',firstFrame:900});
    f.json(101,'resume',{epoch:6,firstFrame:900});
    for(const firstFrame of [-1,.5,0x80000000,'900',null])f.json(101,'resume',{firstFrame});
    assert.equal(h.lastFrame,0);assert.equal(h.history.size,1);assert.equal(f.link.get(101).messages.length,0);
    f.json(101,'resume',{firstFrame:900});
    assert.equal(h.lastFrame,899);assert.equal(h.history.size,0);assert.equal(h.active,true);
    assert.equal(h.generation,generation);assert.equal(h.digest,digest);assert.equal(h.epoch,7);
    assert.equal(f.session.clockTimer,null);assert.equal(f.state.start,1);assert.equal(f.state.deadline,0);
    assert.deepEqual(f.link.get(101).messages.at(-1),{type:'resume-ack',version:1,epoch:7,generation,nextFrame:900});
    f.binary(101,101,900);assert.equal(h.lastFrame,900);assert.equal(h.history.size,1);
    f.json(101,'resume',{firstFrame:100});assert.equal(h.lastFrame,900);assert.equal(h.history.size,1);
    assert.equal(f.link.get(101).messages.at(-1).nextFrame,901);f.clean();
});

test('authenticated auxiliary replacement closes the zombie before hello and preserves active RTC membership',async()=>{
    const f=fixture();await f.commit();f.binary(101,101,0);
    const h=f.session.tagLink,old=f.link.get(102),generation=h.generation,digest=h.digest,order=[];
    const ResponseOriginal=globalThis.Response,PairOriginal=globalThis.WebSocketPair;let replacement;
    const closeOriginal=old.close.bind(old);old.close=()=>{order.push('old-close');closeOriginal();};
    globalThis.Response=class{constructor(_body,options){Object.assign(this,options);}};
    globalThis.WebSocketPair=class{constructor(){
        replacement=new Socket();const send=replacement.send.bind(replacement);
        replacement.send=message=>{if(typeof message==='string')order.push(JSON.parse(message).type);send(message);};
        return {0:new Socket(),1:replacement};
    }};
    try{
        const response=await h.accept(new Request('https://example.test/v4/taglink/ABCDEF'),102);
        assert.equal(response.status,101);assert.deepEqual(order.slice(0,3),['old-close','hello','commit']);
        assert.equal(h.connection(102)[0],replacement);assert.equal(h.peers.has(old),false);
        h.close(old);assert.equal(h.connection(102)[0],replacement);
        assert.equal(h.generation,generation);assert.equal(h.digest,digest);assert.equal(h.active,true);assert.equal(h.lastFrame,0);
        assert.equal(f.session.clockTimer,null);assert.equal(f.state.start,1);
        assert.ok(replacement.messages.at(-1) instanceof ArrayBuffer);assert.equal(replacement.messages.at(-1).byteLength,528);
    }finally{globalThis.Response=ResponseOriginal;globalThis.WebSocketPair=PairOriginal;f.clean();}
});

test('auxiliary replacement before commit invalidates readiness and falls back from pending preparation',async()=>{
    const f=fixture();await f.prepare();const h=f.session.tagLink,generation=h.generation;
    const ResponseOriginal=globalThis.Response,PairOriginal=globalThis.WebSocketPair;let replacement;
    globalThis.Response=class{constructor(_body,options){Object.assign(this,options);}};
    globalThis.WebSocketPair=class{constructor(){replacement=new Socket();return {0:new Socket(),1:replacement};}};
    try{
        await h.accept(new Request('https://example.test/v4/taglink/ABCDEF'),102);
        await new Promise(resolve=>setImmediate(resolve));
        assert.equal(h.connection(102)[0],replacement);assert.equal(h.pending,null);assert.equal(h.active,false);
        assert.equal(h.generation,generation);assert.equal(f.state.start,1);assert.ok(f.session.clockTimer);
        for(const [,p]of h.peers)assert.equal(p.ready,null);
    }finally{globalThis.Response=ResponseOriginal;globalThis.WebSocketPair=PairOriginal;f.clean();}
});

test('malformed binary size and message flooding close only the offending link',()=>{
    const f=fixture();f.session.tagLink.message(f.link.get(102),new ArrayBuffer(527));
    assert.equal(f.link.get(102).closed,true);assert.equal(f.link.get(101).closed,false);
    const ws=f.link.get(103),p=f.session.tagLink.peers.get(ws);p.messages=240;p.window=Date.now();
    f.session.tagLink.message(ws,'{}');assert.equal(ws.closed,true);assert.equal(f.link.get(101).closed,false);f.clean();
});


test('connection maintenance expires only missing traffic and releases the final room',()=>{
    const f=fixture(),now=Date.now();
    const first=f.game.get(101),second=f.game.get(102),third=f.game.get(103);
    f.session.peers.get(first).lastMessage=now-120001;
    f.session.peers.get(second).lastMessage=now;
    f.session.peers.get(third).lastMessage=now-100000;
    f.session.reapConnections();
    assert.equal(first.closed,true);assert.equal(f.session.peers.size,2);
    assert.equal(second.closed,false);assert.equal(third.closed,false);
    f.session.close(second);f.session.close(third);
    assert.equal(f.session.peers.size,0);assert.equal(f.session.clockTimer,null);
    assert.equal(f.session.leaseTimer,null);assert.equal(f.session.tagLink.peers.size,0);
});

test('room abort closes voice and auxiliary sockets as well as game sockets',()=>{
    const f=fixture();const voice=new Socket();
    f.session.voice.peers.set(voice,{id:101});
    f.session.abort('Room expired');
    assert.equal(voice.closed,true);assert.equal(f.session.voice.peers.size,0);
    assert.equal(f.session.tagLink.peers.size,0);assert.equal(f.session.peers.size,0);
    assert.equal(f.session.clockTimer,null);assert.equal(f.session.leaseTimer,null);
});

test('unjoined handshakes expire earlier without disconnecting active members',()=>{
    const f=fixture(),ws=f.game.get(102),peer=f.session.peers.get(ws);
    peer.joined=false;peer.lastMessage=Date.now()-15001;
    f.session.reapConnections();assert.equal(ws.closed,true);assert.equal(f.session.peers.size,2);
});
