import test from 'node:test';
import assert from 'node:assert/strict';
import {webcrypto} from 'node:crypto';
import {VoiceHub,VoiceDuelRoom} from '../voice.mjs';

if(!globalThis.crypto)globalThis.crypto=webcrypto;
class Socket {
    constructor(){this.messages=[];this.listeners=new Map();this.closed=false;this.accepted=false;this.attachment=null;}
    accept(){this.accepted=true;}
    addEventListener(type,callback){this.listeners.set(type,callback);}
    send(data){if(this.closed)throw new Error('closed');this.messages.push(data instanceof ArrayBuffer?data.slice(0):data);}
    close(){this.closed=true;}
    serializeAttachment(value){const text=JSON.stringify(value);assert.ok(Buffer.byteLength(text)<2048);this.attachment=JSON.parse(text);}
    deserializeAttachment(){return structuredClone(this.attachment);}
}
globalThis.WebSocketPair=class {
    constructor(){return {0:new Socket(),1:new Socket()};}
};
globalThis.Response=class {
    constructor(body=null,init={}){this.body=body;this.status=init.status??200;this.webSocket=init.webSocket;}
};
globalThis.WebSocketRequestResponsePair=class {
    constructor(request,response){this.request=request;this.response=response;}
};
class State {
    constructor(){this.sockets=[];this.auto=null;this.events=0;this.autoResponses=0;}
    acceptWebSocket(ws){ws.accepted=true;this.sockets.push(ws);}
    getWebSockets(){return this.sockets.filter(ws=>!ws.closed);}
    setWebSocketAutoResponse(pair){this.auto=pair;}
    receive(room,ws,message){
        if(this.auto&&message===this.auto.request){++this.autoResponses;ws.send(this.auto.response);return;}
        ++this.events;room.webSocketMessage(ws,message);
    }
}
const capability='a'.repeat(64);
async function request(seat=0,cap=capability,name='Player',path=null){
    const digest=[...new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(cap)))].map(b=>b.toString(16).padStart(2,'0')).join('');
    const headers=new Headers({Authorization:'Bearer '+cap,'X-BT4-Name':encodeURIComponent(name)});
    if(seat!==null)headers.set('X-BT4-Seat',String(seat));
    return {url:'https://existing-worker/v3/voice/duel/'+(path??digest),headers};
}
async function connect(room,state,seat,name='Player'){
    const result=await room.fetch(await request(seat,capability,name));assert.equal(result.status,101);
    return state.getWebSockets().find(ws=>room.voice.peers.get(ws)?.seat===seat);
}
function audio(sequence=9){
    const bytes=new ArrayBuffer(20),v=new DataView(bytes);
    v.setUint32(0,0x31563442,true);v.setUint32(4,0xffffffff,true);v.setUint16(8,sequence,true);
    v.setUint16(10,4,true);v.setUint16(12,3,true);new Uint8Array(bytes,16).set([11,22,33,44]);return bytes;
}
function exchange(room,state,left,right){
    left.messages=[];right.messages=[];
    const signal={type:'signal',to:2,signal:{type:'offer',sdp:'unchanged SDP'}};
    state.receive(room,left,JSON.stringify(signal));
    assert.deepEqual(JSON.parse(right.messages.at(-1)),{type:'signal',from:1,signal:signal.signal});
    const candidate={type:'signal',to:1,signal:{type:'candidate',candidate:'candidate:local',mid:'0'}};
    state.receive(room,right,JSON.stringify(candidate));
    assert.deepEqual(JSON.parse(left.messages.at(-1)),{type:'signal',from:2,signal:candidate.signal});
    state.receive(room,left,audio());
    const relayed=right.messages.at(-1);assert.ok(relayed instanceof ArrayBuffer);
    assert.equal(new DataView(relayed).getUint32(4,true),1,'Sender identity remains server-owned');
    assert.equal(new DataView(relayed).getUint16(8,true),9);
    assert.deepEqual([...new Uint8Array(relayed,16)],[11,22,33,44]);
}

test('duel signaling and Opus fallback survive two real constructor restorations',async()=>{
    const state=new State();let room=new VoiceDuelRoom(state,{});
    const left=await connect(room,state,0,'Left'),right=await connect(room,state,1,'Right');
    assert.equal(left.listeners.size,0,'Hibernating sockets use class event handlers');
    exchange(room,state,left,right);
    const born=room.born;
    for(let i=0;i<2;++i){
        room=new VoiceDuelRoom(state,{});
        assert.equal(room.born,born);assert.equal(room.members.size,2);assert.equal(room.voice.peers.size,2);
        assert.equal(room.members.get(1).name,'Left');assert.equal(room.members.get(2).name,'Right');
        exchange(room,state,left,right);
    }
    for(const ws of [left,right]){
        const saved=ws.deserializeAttachment();assert.equal(saved.v,1);assert.equal(saved.alive,true);
        assert.ok(!JSON.stringify(saved).includes(capability));assert.ok(!('iceServers' in saved));
    }
});

test('native idle heartbeat is answered without dispatching a JavaScript event',async()=>{
    const state=new State(),room=new VoiceDuelRoom(state,{}),ws=await connect(room,state,0);
    assert.equal(state.auto.request,'{"type":"ping"}');assert.equal(state.auto.response,'{"type":"pong"}');
    const before=ws.deserializeAttachment();
    for(let i=0;i<10;++i)state.receive(room,ws,'{"type":"ping"}');
    assert.equal(state.events,0);assert.equal(state.autoResponses,10);
    assert.equal(ws.messages.at(-1),'{"type":"pong"}');assert.deepEqual(ws.deserializeAttachment(),before);
    state.receive(room,ws,'{ "type": "ping" }');
    assert.equal(state.events,1);assert.equal(ws.messages.at(-1),'{"type":"pong"}');
});

test('capability, room hash and explicit native seat remain required before replacement',async()=>{
    const state=new State(),room=new VoiceDuelRoom(state,{}),original=await connect(room,state,0);
    assert.equal((await room.fetch(await request(0,'bad'))).status,403);
    assert.equal((await room.fetch(await request(0,capability,'Player','b'.repeat(64)))).status,403);
    for(const seat of [null,'',2,-1,'0.0','0x0'])assert.equal((await room.fetch(await request(seat))).status,400);
    assert.equal(original.closed,false);assert.equal(room.voice.peers.size,1);
});

test('authenticated same-seat reconnect replaces a zombie and old close cannot evict it',async()=>{
    const state=new State();let room=new VoiceDuelRoom(state,{});
    const old=await connect(room,state,0,'Before'),right=await connect(room,state,1,'Other');
    const replacement=await connect(room,state,0,'After');
    assert.notEqual(replacement,old);assert.equal(old.closed,true);assert.equal(old.attachment.alive,false);
    room.webSocketClose(old);room.webSocketError(old);
    assert.equal(room.members.get(1).name,'After');assert.equal(room.voice.peers.size,2);
    room=new VoiceDuelRoom(state,{});exchange(room,state,replacement,right);
    room.webSocketClose(replacement);
    assert.equal(room.members.has(1),false);assert.equal(room.voice.peers.size,1);
    const newest=await connect(room,state,0,'Rejoined');assert.ok(newest);assert.equal(room.members.size,2);
});

test('birth time survives idle rehydration and expired room closes existing members',async()=>{
    const originalNow=Date.now;let now=originalNow();Date.now=()=>now;
    try{
        const state=new State();let room=new VoiceDuelRoom(state,{});const ws=await connect(room,state,0);
        const born=room.born;now+=3*3600000+1;room=new VoiceDuelRoom(state,{});
        assert.equal(room.born,born);
        assert.equal((await room.fetch(await request())).status,410);
        assert.equal(ws.closed,true);assert.equal(room.members.size,0);assert.equal(room.voice.peers.size,0);
    }finally{Date.now=originalNow;}
});

test('rate windows survive rehydration and malformed metadata cannot restore a member',async()=>{
    const originalNow=Date.now,now=originalNow();Date.now=()=>now;
    try{
        const state=new State();let room=new VoiceDuelRoom(state,{});const ws=await connect(room,state,0);
        for(let i=0;i<160;++i)state.receive(room,ws,'{"type":"channel"}');
        room=new VoiceDuelRoom(state,{});assert.equal(room.voice.peers.get(ws).controls,160);
        state.receive(room,ws,'{"type":"channel"}');assert.equal(ws.closed,true);assert.equal(room.members.size,0);
        const invalid=new Socket();invalid.accepted=true;invalid.serializeAttachment({v:1,alive:true,id:4,seat:0});state.sockets.push(invalid);
        room=new VoiceDuelRoom(state,{});assert.equal(room.members.size,0);assert.equal(invalid.closed,true);
    }finally{Date.now=originalNow;}
});

test('Tag voice keeps its standard socket transport and existing relay semantics',async()=>{
    const members=new Map([[7,{id:7,seat:0,team:0,host:true,name:'Tag Left'}],[8,{id:8,seat:1,team:1,host:false,name:'Tag Right'}]]);
    const hub=new VoiceHub(id=>members.get(id),false,{});
    assert.equal((await hub.accept({},7)).status,101);assert.equal((await hub.accept({},8)).status,101);
    const left=[...hub.peers.keys()][0],right=[...hub.peers.keys()][1];
    assert.equal(left.accepted,true);assert.equal(left.listeners.has('message'),true);assert.equal(left.attachment,null);
    hub.message(left,audio());assert.equal(new DataView(right.messages.at(-1)).getUint32(4,true),7);
    assert.equal((await hub.accept({},7)).status,409,'Tag duplicate policy is unchanged');
});

test('duel fixtures without hibernation APIs still accept and remove standard sockets',async()=>{
    const room=new VoiceDuelRoom({},{});
    assert.equal((await room.fetch(await request())).status,101);
    const ws=[...room.voice.peers.keys()][0];assert.equal(ws.listeners.has('message'),true);
    room.voice.message(ws,'{"type":"ping"}');assert.equal(ws.messages.at(-1),'{"type":"pong"}');
    room.voice.close(ws);assert.equal(room.members.size,0);assert.equal(room.voice.peers.size,0);
});

test('failed roster delivery clears saved membership and informs remaining peers',async()=>{
    const state=new State(),room=new VoiceDuelRoom(state,{});
    const left=await connect(room,state,0),right=await connect(room,state,1);
    right.closed=true;room.voice.peers.get(right).roster='';room.voice.refresh();
    assert.equal(room.members.has(2),false);assert.equal(room.voice.peers.size,1);
    assert.equal(right.attachment.alive,false);
    assert.deepEqual(JSON.parse(left.messages.at(-1)).peers,[]);
});
