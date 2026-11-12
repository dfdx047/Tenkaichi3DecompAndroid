// Authenticated star transport. The room kernel or the native host owns a round,
// never both. Voice and microphone settings do not participate in this channel.
import {VoiceHub} from './voice.mjs';

const PROTOCOL=0x54540009, MAGIC=0x314b4c54, BYTES=528;
const STATE=100, FRAME=101, INPUT=11, PING=107, PONG=108;
const ARMED=4, FIGHT=5, HISTORY=512;
const hex=bytes=>Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');
const same=(a,b)=>a.length===b.length&&a.every((v,i)=>v===b[i]);

export class TagLinkHub {
    constructor(session,env={}) {
        this.session=session;this.peers=new Map();this.ice=new VoiceHub(()=>null,false,env);
        this.epoch=-1;this.host=0;this.generation='';this.rosterKey='';this.members=[];
        this.pending=null;this.attemptedEpoch=-1;this.active=false;this.digest='';
        this.lastFrame=-1;this.history=new Map();
    }
    roster() {
        const s=this.session,a=s.kernel.api,packet=s.kernel.snapshot(0,0,s.code),v=new DataView(packet);
        return [...s.peers.values()].filter(p=>p.joined&&a.Kernel_Seat(p.id)>=0).map(p=>{
            const seat=a.Kernel_Seat(p.id),o=72+seat*40;
            return {id:p.id,seat,team:(v.getUint32(o+32,true)>>4)&1,
                name:new TextDecoder().decode(new Uint8Array(packet,o+4,20)).split('\0')[0],
                features:p.features||0};
        }).sort((a,b)=>a.id-b.id);
    }
    send(ws,message) {try{ws.send(typeof message==='string'||message instanceof ArrayBuffer?message:JSON.stringify(message));return true;}catch{this.close(ws);return false;}}
    sendControl(message) {for(const [ws]of this.peers)this.send(ws,message);}
    refresh() {
        const a=this.session.kernel.api,epoch=a.Kernel_Epoch(),host=a.Kernel_Host(),phase=a.Kernel_Phase(),members=this.roster();
        const key=JSON.stringify([epoch,host,members.map(p=>[p.id,p.seat,p.features])]);
        const changed=key!==this.rosterKey;
        if(this.active&&(epoch!==this.epoch||phase!==FIGHT||host!==this.host))this.stopActive('round-ended');
        if(this.pending&&changed)this.failPending('membership-changed',true);
        if(changed&&!this.active){
            this.generation=hex(crypto.getRandomValues(new Uint8Array(8)));
            for(const [,p]of this.peers)p.ready=null;
        }
        this.epoch=epoch;this.host=host;this.members=members;this.rosterKey=key;
        for(const [ws,p]of this.peers){
            if(!members.some(m=>m.id===p.id&&(m.features&4))){this.close(ws);continue;}
            this.hello(ws,p);
        }
    }
    hello(ws,p) {
        const message={type:'hello',version:1,self:p.id,host:this.host,epoch:this.epoch,generation:this.generation,
            iceServers:this.ice.iceServers,peers:this.members.filter(m=>m.id!==p.id&&(p.id===this.host||m.id===this.host)&&
                (this.active||this.connection(m.id)))
                .map(({id,seat,team,name})=>({id,seat,team,name})),active:this.active,nextFrame:this.lastFrame+1};
        const text=JSON.stringify(message);if(text!==p.hello){p.hello=text;this.send(ws,text);}
    }
    async accept(request,id) {
        this.refresh();
        if(!this.members.some(p=>p.id===id&&(p.features&4)))return new Response('Not an eligible room member',{status:403});
        await this.ice.prepareIce();
        this.refresh();
        if(!this.members.some(p=>p.id===id&&(p.features&4)))return new Response('Not a room member',{status:403});
        // The authenticated game ticket can replace a half-open auxiliary
        // socket. Its later close event cannot remove the replacement by ID.
        const previous=this.connection(id);if(previous)this.close(previous[0]);
        const [client,server]=Object.values(new WebSocketPair());server.accept();
        const p={id,window:Date.now(),messages:0,controls:0,ready:null,hello:''};this.peers.set(server,p);
        server.addEventListener('message',e=>this.message(server,e.data));
        server.addEventListener('close',()=>this.close(server));server.addEventListener('error',()=>this.close(server));
        if(!this.active)for(const [,peer]of this.peers)peer.ready=null;
        // The destination must receive its own identity/roster before another
        // peer learns it can send an offer. The room generation is unchanged.
        this.hello(server,p);
        for(const [other,peer]of this.peers)if(other!==server)this.hello(other,peer);
        if(this.active){
            this.send(server,{type:'commit',version:1,epoch:this.epoch,generation:this.generation,digest:this.digest});
            for(const packet of this.history.values())this.send(server,packet.slice(0));
        }
        return new Response(null,{status:101,webSocket:client});
    }
    close(ws) {
        const p=this.peers.get(ws);if(!p)return;this.peers.delete(ws);
        try{ws.close(1000,'');}catch{}
        // A link reconnect does not change the authenticated game membership,
        // epoch, host or generation. Existing RTC paths may still be usable.
        if(this.pending)this.failPending('link-disconnected',true);
        if(!this.active)for(const [other,peer]of this.peers){peer.ready=null;this.hello(other,peer);}
    }
    connection(id) {for(const [ws,p]of this.peers)if(p.id===id)return [ws,p];return null;}
    current(m) {return m.version===1&&m.epoch===this.epoch&&m.generation===this.generation;}
    ready(p,m) {
        if(this.active||!this.current(m)||!Array.isArray(m.peers)||m.peers.length>9)return;
        const wanted=this.members.filter(x=>x.id!==p.id&&(p.id===this.host||x.id===this.host)).map(x=>x.id).sort((a,b)=>a-b);
        const got=[];
        for(const peer of m.peers){
            if(!peer||!Number.isSafeInteger(peer.id)||!Number.isFinite(peer.rttMs)||peer.rttMs<0||peer.rttMs>30000)return;
            got.push(peer.id);
        }
        got.sort((a,b)=>a-b);
        if(JSON.stringify(wanted)!==JSON.stringify(got))return;
        p.ready={epoch:this.epoch,generation:this.generation,at:Date.now()};
    }
    resume(ws,p,m) {
        if(!this.active||p.id!==this.host||!this.current(m)||!Number.isSafeInteger(m.firstFrame)||m.firstFrame<0||m.firstFrame>0x7fffffff)return;
        if(m.firstFrame>this.lastFrame+1){
            // Only the WSS relay cache advances. The same native host kernel,
            // epoch, generation and clock continue producing authoritative data.
            this.history.clear();this.lastFrame=m.firstFrame-1;
        }
        this.send(ws,{type:'resume-ack',version:1,epoch:this.epoch,generation:this.generation,nextFrame:this.lastFrame+1});
    }
    canPrepare() {
        if(this.active||this.pending||this.session.kernel.api.Kernel_Phase()!==ARMED||this.members.length<2||this.attemptedEpoch===this.epoch)return false;
        return this.members.every(m=>{
            const c=this.connection(m.id),ready=c?.[1].ready;
            const age=ready?Date.now()-ready.at:Infinity;
            return (m.features&4)&&ready?.epoch===this.epoch&&ready?.generation===this.generation&&age>=0&&age<=2000;
        });
    }
    tryStart(ws,packet,id) {
        this.refresh();
        if(this.pending){
            const v=new DataView(packet);
            // Retrying START while PREPARE is in flight must not fall through to
            // the DO kernel/clock before the same members finish their barrier.
            return id===this.pending.host&&v.getUint32(0,true)===PROTOCOL&&
                v.getUint32(8,true)===this.pending.epoch&&this.validPending(this.pending);
        }
        if(id!==this.host||!this.canPrepare())return false;
        this.attemptedEpoch=this.epoch;
        const pending={ws,id,packet:packet.slice(0),epoch:this.epoch,generation:this.generation,host:this.host,
            members:this.members.map(p=>p.id),digest:'',prepared:new Set(),timer:null};
        this.pending=pending;
        pending.timer=setTimeout(()=>{if(this.pending===pending)this.failPending('negotiation-timeout',true);},3000);
        const snapshot=this.session.kernel.snapshot(0,0,this.session.code),v=new DataView(snapshot);
        v.setUint32(12,0,true);v.setUint32(508,0,true);
        if(v.getUint32(0,true)!==PROTOCOL||v.getUint32(4,true)!==STATE||v.getUint32(28,true)!==ARMED){this.failPending('invalid-snapshot',true);return true;}
        crypto.subtle.digest('SHA-256',snapshot).then(digest=>{
            if(this.pending!==pending)return;
            pending.digest=hex(new Uint8Array(digest));
            this.sendControl({type:'prepare',version:1,epoch:pending.epoch,generation:pending.generation,
                snapshot:hex(new Uint8Array(snapshot)),digest:pending.digest});
        }).catch(()=>{if(this.pending===pending)this.failPending('snapshot-failed',true);});
        return true;
    }
    validPending(p) {
        return this.pending===p&&this.epoch===p.epoch&&this.generation===p.generation&&this.host===p.host&&
            this.session.kernel.api.Kernel_Phase()===ARMED&&this.session.kernel.api.Kernel_Epoch()===p.epoch;
    }
    prepared(p,m) {
        const pending=this.pending;
        if(!pending||!this.current(m)||!pending.digest||m.digest!==pending.digest||!pending.members.includes(p.id)||!this.validPending(pending))return;
        pending.prepared.add(p.id);
        if(pending.prepared.size===pending.members.length){
            if(!this.session.commitTagLinkStart(pending))this.failPending('start-rejected',false);
        }
    }
    activate(p) {
        if(this.pending!==p)return false;
        clearTimeout(p.timer);this.pending=null;this.active=true;this.digest=p.digest;
        this.lastFrame=-1;this.history.clear();
        this.sendControl({type:'commit',version:1,epoch:this.epoch,generation:this.generation,digest:this.digest});
        return true;
    }
    failPending(reason,fallback) {
        const pending=this.pending;if(!pending)return;
        clearTimeout(pending.timer);this.pending=null;
        this.sendControl({type:'cancel',version:1,epoch:pending.epoch,generation:pending.generation,reason});
        if(fallback)queueMicrotask(()=>{
            const a=this.session.kernel.api;
            if(!this.active&&a.Kernel_Epoch()===pending.epoch&&a.Kernel_Host()===pending.host&&a.Kernel_Phase()===ARMED&&this.session.peers.has(pending.ws))
                this.session.message(pending.ws,pending.packet);
        });
    }
    stopActive(reason) {
        if(!this.active)return;this.active=false;
        this.sendControl({type:'cancel',version:1,epoch:this.epoch,generation:this.generation,reason});
        this.history.clear();this.lastFrame=-1;this.digest='';
        this.session.tagLinkEnded();
    }
    shutdown() {
        this.failPending('room-ended',false);this.stopActive('room-ended');
        for(const [ws]of this.peers)this.close(ws);
    }
    binary(ws,p,data) {
        if(data.byteLength!==BYTES){this.close(ws);return;}
        const v=new DataView(data),wire=new DataView(data,16),lo=parseInt(this.generation.slice(8),16),hi=parseInt(this.generation.slice(0,8),16);
        if(v.getUint32(0,true)!==MAGIC||v.getUint32(4,true)!==lo||v.getUint32(8,true)!==hi||
           wire.getUint32(0,true)!==PROTOCOL||wire.getUint32(8,true)!==this.epoch)return;
        const command=wire.getUint32(4,true);v.setUint32(12,p.id,true);
        if(command===PING){
            if(p.id===this.host)return;
            const host=this.connection(this.host);if(host)this.send(host[0],data);return;
        }
        if(command===PONG){
            if(p.id!==this.host)return;
            const id=wire.getUint32(12,true);
            if(id===this.host||!this.members.some(m=>m.id===id))return;
            const dest=this.connection(id);if(dest)this.send(dest[0],data);return;
        }
        if(!this.active||this.session.kernel.api.Kernel_Phase()!==FIGHT)return;
        const frame=wire.getUint32(12,true);
        if(command===INPUT){
            if(frame>0x7fffffff||frame>this.lastFrame+HISTORY||frame+64<this.lastFrame)return;
            const host=this.connection(this.host);if(host)this.send(host[0],data);return;
        }
        if(command!==FRAME||p.id!==this.host||frame>0x7fffffff)return;
        if(frame<=this.lastFrame){
            const previous=this.history.get(frame);
            if(previous&&!same(new Uint8Array(previous),new Uint8Array(data)))this.session.abortTagLink('Host frame conflict');
            return;
        }
        // WSS is reliable and ordered. The host duplicates every new frame onto
        // this path, so accepting a gap would hide a transport/programming fault.
        if(frame!==this.lastFrame+1)return;
        this.lastFrame=frame;this.history.set(frame,data.slice(0));
        while(this.history.size>HISTORY)this.history.delete(this.history.keys().next().value);
        for(const [other]of this.peers)this.send(other,data);
    }
    message(ws,data) {
        const p=this.peers.get(ws);if(!p)return;
        const member=this.session.peers;
        if(![...member.values()].some(m=>m.id===p.id&&m.joined&&(m.features&4))){this.close(ws);return;}
        const now=Date.now();if(now-p.window>=1000){p.window=now;p.messages=p.controls=0;}
        if(++p.messages>240){this.close(ws);return;}
        if(data instanceof ArrayBuffer){this.binary(ws,p,data);return;}
        if(typeof data!=='string'||data.length>16384||++p.controls>160){this.close(ws);return;}
        let m;try{m=JSON.parse(data);}catch{this.close(ws);return;}
        if(!m||typeof m!=='object'||Array.isArray(m))return;
        if(m.type==='ping'){this.send(ws,{type:'pong',version:1,epoch:this.epoch,generation:this.generation});return;}
        if(m.type==='resume'){this.resume(ws,p,m);return;}
        if(m.type==='ready'){this.ready(p,m);return;}
        if(m.type==='prepared'){this.prepared(p,m);return;}
        if(m.type!=='signal'||!this.current(m)||!Number.isSafeInteger(m.to)||!m.signal||typeof m.signal!=='object')return;
        if(m.to===p.id||(p.id!==this.host&&m.to!==this.host))return;
        const dest=this.connection(m.to);if(!dest||!this.members.some(x=>x.id===m.to))return;
        const s=m.signal;
        if(!['offer','answer','candidate'].includes(s.type))return;
        if(s.type==='candidate'&&(typeof s.candidate!=='string'||s.candidate.length>2048||typeof s.mid!=='string'||s.mid.length>32))return;
        if(s.type!=='candidate'&&(typeof s.sdp!=='string'||s.sdp.length>14000))return;
        this.send(dest[0],{type:'signal',version:1,epoch:this.epoch,generation:this.generation,from:p.id,signal:s});
    }
}
