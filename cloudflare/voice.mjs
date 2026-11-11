// Voice signaling and bounded Opus relay. Gameplay never waits for this channel.
// WebRTC media goes directly between authorized peers when ICE succeeds.
const MAGIC=0x31563442;
const safeName=value=>String(value||'Player').replace(/[\x00-\x1f\x7f]/g,'').slice(0,32);
export class VoiceHub {
    constructor(lookup,duel=false,env={},lifecycle=null){this.env=env;this.iceServers=[];this.iceUntil=0;this.lookup=lookup;this.duel=duel;this.peers=new Map();this.allowRoom=true;this.lifecycle=lifecycle;}
    allowed(a,b){return a.id!==b.id;}
    async prepareIce(){
        if(Date.now()<this.iceUntil)return;
        this.iceUntil=Date.now()+60000;
        if(!this.env.TURN_KEY_ID||!this.env.TURN_KEY_TOKEN)return;
        try{
            const response=await fetch(`https://rtc.live.cloudflare.com/v1/turn/keys/${this.env.TURN_KEY_ID}/credentials/generate-ice-servers`,{
                method:'POST',signal:AbortSignal.timeout(4000),headers:{Authorization:`Bearer ${this.env.TURN_KEY_TOKEN}`,'Content-Type':'application/json'},body:JSON.stringify({ttl:10800})});
            if(!response.ok)return;
            const body=await response.json(),list=Array.isArray(body.iceServers)?body.iceServers:body.iceServers?[body.iceServers]:[];
            this.iceServers=list.filter(x=>x&&typeof x==='object'&&x.urls).map(x=>({urls:x.urls,...(typeof x.username==='string'?{username:x.username,credential:x.credential}:{})}));
            this.iceUntil=Date.now()+9000000;
        }catch{}
    }
    async accept(request,id,icePrepared=false){
        let meta=this.lookup(id);if(!meta)return new Response('Not a room member',{status:403});
        if([...this.peers.values()].some(p=>p.id===id))return new Response('Voice already connected',{status:409});
        if(!icePrepared)await this.prepareIce();
        meta=this.lookup(id);if(!meta)return new Response('Not a room member',{status:403});
        if([...this.peers.values()].some(p=>p.id===id))return new Response('Voice already connected',{status:409});
        const [client,server]=Object.values(new WebSocketPair());
        if(this.lifecycle?.hibernating)this.lifecycle.state.acceptWebSocket(server);
        else server.accept();
        const peer={...meta,id,channel:'proximity',window:Date.now(),messages:0,controls:0,roster:'',opened:Date.now()};
        this.peers.set(server,peer);this.lifecycle?.save(server,peer);
        if(!this.lifecycle?.hibernating){
            server.addEventListener('message',e=>this.message(server,e.data));
            server.addEventListener('close',()=>this.close(server));server.addEventListener('error',()=>this.close(server));
        }
        this.refresh();return new Response(null,{status:101,webSocket:client});
    }
    forget(ws){const peer=this.peers.get(ws);if(!this.peers.delete(ws))return false;this.lifecycle?.closed(ws,peer);try{ws.close(1000,'');}catch{}return true;}
    close(ws){if(this.forget(ws))this.refresh();}
    drop(id){for(const [ws,p]of this.peers)if(p.id===id)this.close(ws);}
    refresh(){
        for(const [ws,p]of this.peers){const m=this.lookup(p.id);if(!m){this.forget(ws);continue;}Object.assign(p,m);}
        let removed=false;
        for(const [ws,p]of this.peers){
            const roster={type:'hello',iceServers:this.iceServers,self:p.id,seat:p.seat,host:!!p.host,allowRoom:this.allowRoom,channel:p.channel,
                peers:[...this.peers.values()].filter(q=>this.allowed(p,q)).map(q=>({id:q.id,seat:q.seat,team:q.team,name:safeName(q.name)}))};
            const text=JSON.stringify(roster);if(text!==p.roster){p.roster=text;try{ws.send(text);}catch{removed=this.forget(ws)||removed;}}
        }
        if(removed)this.refresh(); // Each retry removes a socket; the roster is bounded.
    }
    message(ws,data){
        const p=this.peers.get(ws);if(!p||!this.lookup(p.id)){this.close(ws);return;}
        const now=Date.now();if(now-p.window>=1000){p.window=now;p.messages=p.controls=0;}
        if(++p.messages>240){this.close(ws);return;}
        if(data instanceof ArrayBuffer){
            if(data.byteLength<17||data.byteLength>528){this.close(ws);return;}
            const v=new DataView(data);if(v.getUint32(0,true)!==MAGIC||v.getUint16(10,true)!==data.byteLength-16){this.close(ws);return;}
            const mask=v.getUint16(12,true);v.setUint32(4,p.id,true);
            for(const [other,q]of this.peers)if((mask&(1<<q.seat))&&this.allowed(p,q)){try{other.send(data);}catch{this.close(other);}}
            return;
        }
        if(typeof data!=='string'||data.length>16384||++p.controls>160){this.close(ws);return;}
        let m;try{m=JSON.parse(data);}catch{this.close(ws);return;}
        if(m.type==='ping'){ws.send('{"type":"pong"}');return;}
        if(m.type==='channel')return;
        if(m.type==='policy')return;
        if(m.type!=='signal'||!Number.isSafeInteger(m.to)||typeof m.signal!=='object'||m.signal===null)return;
        const s=m.signal;if(!['offer','answer','candidate'].includes(s.type))return;
        if(s.type==='candidate'&&(typeof s.candidate!=='string'||s.candidate.length>2048||typeof s.mid!=='string'||s.mid.length>32))return;
        if(s.type!=='candidate'&&(typeof s.sdp!=='string'||s.sdp.length>14000))return;
        for(const [other,q]of this.peers)if(q.id===m.to&&this.allowed(p,q)){
            try{other.send(JSON.stringify({type:'signal',from:p.id,signal:s}));}catch{this.close(other);}break;
        }
    }
}
export class VoiceDuelRoom {
    constructor(ctx,env){
        this.members=new Map();this.born=Date.now();this.state=ctx;
        this.hibernating=typeof ctx?.acceptWebSocket==='function'&&typeof ctx?.getWebSockets==='function';
        this.voice=new VoiceHub(id=>this.members.get(id),true,env,this);
        if(this.hibernating){
            for(const ws of ctx.getWebSockets())this.restore(ws);
            if(typeof ctx.setWebSocketAutoResponse==='function'&&typeof WebSocketRequestResponsePair==='function')
                ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"type":"ping"}','{"type":"pong"}'));
        }
    }
    save(ws,peer){
        if(!this.hibernating)return;
        // Only bounded server-authenticated metadata survives hibernation. No
        // capability, SDP, ICE credential or audio payload is persisted here.
        ws.serializeAttachment({v:1,alive:true,id:peer.id,seat:peer.seat,name:safeName(peer.name),born:this.born,
            opened:peer.opened,window:peer.window,messages:peer.messages,controls:peer.controls});
    }
    restore(ws){
        let saved;try{saved=ws.deserializeAttachment();}catch{}
        const now=Date.now();
        if(!saved||saved.v!==1||saved.alive!==true||!Number.isInteger(saved.seat)||saved.seat<0||saved.seat>1||saved.id!==saved.seat+1||
           typeof saved.name!=='string'||saved.name.length>32||!Number.isSafeInteger(saved.born)||saved.born<=0||saved.born>now||
           !Number.isSafeInteger(saved.opened)||saved.opened<saved.born||saved.opened>now||
           !Number.isSafeInteger(saved.window)||saved.window<saved.born||saved.window>now||
           !Number.isInteger(saved.messages)||saved.messages<0||saved.messages>241||!Number.isInteger(saved.controls)||saved.controls<0||saved.controls>161){
            try{ws.close(1008,'Invalid voice membership');}catch{}return;
        }
        this.born=Math.min(this.born,saved.born);
        const previous=[...this.voice.peers].find(([,peer])=>peer.id===saved.id);
        if(previous){
            if(previous[1].opened>saved.opened){try{ws.close(1000,'Replaced voice connection');}catch{}return;}
            this.voice.close(previous[0]);
        }
        const meta={id:saved.id,seat:saved.seat,team:0,host:saved.seat===0,name:safeName(saved.name)};
        this.members.set(meta.id,meta);
        this.voice.peers.set(ws,{...meta,channel:'proximity',window:saved.window,messages:saved.messages,controls:saved.controls,opened:saved.opened,roster:''});
    }
    closed(ws,peer){
        if(this.hibernating){try{ws.serializeAttachment({v:1,alive:false});}catch{}}
        if(peer&&![...this.voice.peers.values()].some(other=>other.id===peer.id))this.members.delete(peer.id);
    }
    expired(){
        if(Date.now()-this.born<=3*3600000)return false;
        for(const [ws]of this.voice.peers)this.voice.close(ws);
        this.members.clear();return true;
    }
    webSocketMessage(ws,data){
        if(this.expired())return;
        this.voice.message(ws,data);
        const peer=this.voice.peers.get(ws);if(peer)this.save(ws,peer);
    }
    webSocketClose(ws){this.voice.close(ws);}
    webSocketError(ws){this.voice.close(ws);}
    async fetch(request){
        if(this.expired())return new Response('Voice room expired',{status:410});
        const cap=request.headers.get('Authorization')?.replace(/^Bearer /,'')||'';
        if(!/^[a-f0-9]{64}$/.test(cap))return new Response('Invalid voice capability',{status:403});
        const hash=[...new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(cap)))].map(b=>b.toString(16).padStart(2,'0')).join('');
        if(new URL(request.url).pathname.split('/').pop()!==hash)return new Response('Invalid voice room',{status:403});
        const seatText=request.headers.get('X-BT4-Seat');
        if(seatText!=='0'&&seatText!=='1')return new Response('Invalid seat',{status:400});
        const seat=Number(seatText);
        let name='Player';try{name=decodeURIComponent(request.headers.get('X-BT4-Name')||'Player');}catch{}
        await this.voice.prepareIce();
        if(this.expired())return new Response('Voice room expired',{status:410});
        // A valid capability may replace its own half-open seat. A late close
        // from the old socket cannot remove the newly accepted membership.
        this.voice.drop(seat+1);
        this.members.set(seat+1,{id:seat+1,seat,team:0,host:seat===0,name:safeName(name)});
        return this.voice.accept(request,seat+1,true);
    }
}
