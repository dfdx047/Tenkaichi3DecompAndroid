import {VoiceHub} from './voice.mjs';
import {TagLinkHub} from './tag_link.mjs';
// One serialized Durable Object per match. C kernel owns room authority and barriers.
export class SessionKernel {
    constructor(module) {
        this.api=new WebAssembly.Instance(module,{}).exports;
        this.input=new Uint8Array(this.api.memory.buffer,this.api.Kernel_Input(),512);
        this.output=new Uint8Array(this.api.memory.buffer,this.api.Kernel_Output(),512);
    }
    command(id,packet){this.input.set(new Uint8Array(packet));return this.api.Kernel_Command(id);}
    snapshot(id,result,code){
        const ptr=this.api.Kernel_Input();this.input.fill(0);this.input.set(new TextEncoder().encode(code));
        this.api.Kernel_Snapshot(id,result,ptr);return this.output.slice().buffer;
    }
    take(){return this.api.Kernel_TakeFrame()?this.output.slice().buffer:null;}
}
export function createSessionClass(module) {
    return class {
        constructor(ctx,env) {
            this.ctx=ctx;this.env=env;this.kernel=new SessionKernel(module);this.peers=new Map();this.serial=100;this.code='';this.born=Date.now();this.voice=new VoiceHub(id=>this.voiceMember(id),false,env);
            this.syncSerial=0;this.syncKind=0;this.clockTimer=null;this.paced=false;this.clockNext=0;
            this.tagLink=new TagLinkHub(this,env);
            this.leaseTimer=null;
        }
        voiceMember(id){
            const peer=[...this.peers.values()].find(p=>p.id===id);if(!peer?.joined)return null;
            const seat=this.kernel.api.Kernel_Seat(id);if(seat<0)return null;
            const b=this.kernel.snapshot(id,0,this.code),v=new DataView(b),o=72+seat*40;
            return {id,seat,team:(v.getUint32(o+32,true)>>4)&1,host:id===this.kernel.api.Kernel_Host(),name:new TextDecoder().decode(new Uint8Array(b,o+4,20)).split('\0')[0]};
        }
        async fetch(request) {
            if(request.headers.get('Upgrade')?.toLowerCase()!=='websocket')return new Response('WebSocket required',{status:426});
            if(new URL(request.url).pathname.startsWith('/v4/taglink/')){
                const token=request.headers.get('Authorization')?.replace(/^Bearer /,'');
                if(!/^[a-f0-9]{64}$/.test(token||''))return new Response('Invalid game link membership',{status:403});
                const peer=[...this.peers.values()].find(p=>p.joined&&(p.features&4)&&p.linkToken===token);
                return peer?this.tagLink.accept(request,peer.id):new Response('Invalid game link membership',{status:403});
            }
            if(new URL(request.url).pathname.startsWith('/v3/voice/tag/')){
                const token=request.headers.get('Authorization')?.replace(/^Bearer /,'');
                const peer=[...this.peers.values()].find(p=>p.voiceToken&&p.voiceToken===token);
                return peer?this.voice.accept(request,peer.id):new Response('Invalid voice membership',{status:403});
            }
            if(this.peers.size>=10)return new Response('Room full',{status:409});
            if(Date.now()-this.born>3*3600000)return new Response('Room expired',{status:410});
            this.code=new URL(request.url).pathname.split('/').pop();
            const [client,server]=Object.values(new WebSocketPair());
            const id=++this.serial;server.accept();
            this.peers.set(server,{id,joined:false,window:Date.now(),messages:0,chats:0,ack:0,lastMessage:Date.now()});
            this.watchConnections();
            server.addEventListener('message',event=>this.message(server,event.data));
            server.addEventListener('close',()=>{try{server.close(1000,'');}catch{}this.close(server);});
            server.addEventListener('error',()=>this.close(server));
            server.send(this.kernel.snapshot(id,0,this.code));
            return new Response(null,{status:101,webSocket:client});
        }
        broadcast(result=0,only=null){
            this.voice.refresh();
            this.tagLink.refresh();
            for(const [ws,p] of this.peers)if(!only||ws===only){try{const packet=this.kernel.snapshot(p.id,result,this.code);new DataView(packet).setUint32(508,p.ack,true);ws.send(packet);}catch{this.close(ws);}}
        }
        message(ws,data){
            const peer=this.peers.get(ws);if(!peer)return;
            peer.lastMessage=Date.now();
            if(Date.now()-this.born>3*3600000){this.abort('Room expired');return;}
            if(!(data instanceof ArrayBuffer)||data.byteLength!==512){ws.close(1008,'Invalid packet');return;}
            const now=Date.now();if(now-peer.window>=1000){peer.window=now;peer.messages=0;peer.chats=0;}
            if(++peer.messages>180){ws.close(1008,'Rate limit');return;}
            const view=new DataView(data),cmd=view.getUint32(4,true);
            peer.ack=view.getUint32(508,true);
            if(cmd===22){
                if(view.getUint32(0,true)!==0x54540009||this.kernel.api.Kernel_Seat(peer.id)<0)return;
                peer.features=view.getUint32(12,true)&7;this.tagLink.refresh();this.sendCapabilities(ws,peer);return;
            }
            if(cmd===23){
                if(view.getUint32(0,true)!==0x54540009||!peer.joined||!(peer.features&4)||this.kernel.api.Kernel_Seat(peer.id)<0)return;
                if(peer.linkAt&&now-peer.linkAt<1000)return;peer.linkAt=now;
                if(!peer.linkToken)peer.linkToken=[...crypto.getRandomValues(new Uint8Array(32))].map(b=>b.toString(16).padStart(2,'0')).join('');
                const packet=new Uint8Array(512),out=new DataView(packet.buffer);out.setUint32(0,0x54540009,true);out.setUint32(4,105,true);
                packet.set(new TextEncoder().encode(peer.linkToken),16);ws.send(packet.buffer);return;
            }
            if(cmd===20){
                if(this.kernel.api.Kernel_Seat(peer.id)<0)return;
                if(peer.voiceAt&&now-peer.voiceAt<1000)return;peer.voiceAt=now;
                if(!peer.voiceToken)peer.voiceToken=[...crypto.getRandomValues(new Uint8Array(32))].map(b=>b.toString(16).padStart(2,'0')).join('');
                const packet=new Uint8Array(512),out=new DataView(packet.buffer);out.setUint32(0,0x54540009,true);out.setUint32(4,102,true);
                packet.set(new TextEncoder().encode(peer.voiceToken),16);ws.send(packet.buffer);return;
            }
            if(cmd===16){
                if(this.kernel.api.Kernel_Seat(peer.id)<0 || ++peer.chats>2)return;
                const packet=new Uint8Array(data.slice(0));const out=new DataView(packet.buffer);out.setUint32(12,peer.id,true);
                // Text only, maximum 160 UTF-8 bytes; sender identity cannot be forged.
                packet.fill(0,176);
                for(const [other] of this.peers){try{other.send(packet.buffer);}catch{this.close(other);}}return;
            }
            let result;
            // After COMMIT the native host is the sole input/frame authority.
            // Regular WSS remains available for room control, hashes and results.
            if(cmd===11&&this.tagLink.active)return;
            if(cmd===9&&this.tagLink.tryStart(ws,data,peer.id))return;
            try{result=this.kernel.command(peer.id,data);}catch{this.abort('Invalid session state');return;}
            if(cmd===17){peer.detailedSync=view.getUint32(24,true)===0x32535442;this.syncDiagnostics();}
            if(result){this.broadcast(result,this.kernel.api.Kernel_Phase()===6?null:ws);return;}
            if(cmd===1){this.kernel.api.Kernel_Seed(crypto.getRandomValues(new Uint32Array(1))[0]||1);peer.joined=true;}
            if(cmd===2)peer.joined=true;
            if(cmd===18&&view.getUint32(12,true)<2)this.kernel.api.Kernel_Seed(crypto.getRandomValues(new Uint32Array(1))[0]||1);
            if(cmd===11){if(!this.paced)this.frames();}
            else if(cmd===17)return;
            else if(cmd===9){this.startClock();this.broadcast();if(!this.paced)this.frames();}
            else if(cmd===10)this.broadcast(0,ws);
            else this.broadcast();
        }
        sendCapabilities(ws,peer){
            if(!peer.features)return;
            const packet=new ArrayBuffer(512),v=new DataView(packet);
            v.setUint32(0,0x54540009,true);v.setUint32(4,104,true);
            v.setUint32(12,this.paced?1:0,true);v.setUint32(16,114,true);v.setUint32(20,34200,true);
            v.setUint32(44,4,true); // Server supports the independent native-host game link.
            if(this.tagLink.active){
                v.setUint32(24,1,true);v.setUint32(28,this.tagLink.host,true);
                v.setUint32(32,parseInt(this.tagLink.generation.slice(8),16),true);
                v.setUint32(36,parseInt(this.tagLink.generation.slice(0,8),16),true);v.setUint32(40,this.tagLink.epoch,true);
            }
            try{ws.send(packet);}catch{this.close(ws);}
        }
        commitTagLinkStart(pending){
            if(!this.tagLink.validPending(pending))return false;
            let result;try{result=this.kernel.command(pending.id,pending.packet);}catch{return false;}
            if(result){this.broadcast(result,pending.ws);return false;}
            this.stopClock();this.paced=true;this.kernel.api.Kernel_Pacing(1);
            this.tagLink.activate(pending);
            for(const [ws,p]of this.peers)this.sendCapabilities(ws,p);
            this.broadcast();return true;
        }
        tagLinkEnded(){
            this.stopClock();this.paced=false;this.kernel.api.Kernel_Pacing(0);
            for(const [ws,p]of this.peers)this.sendCapabilities(ws,p);
        }
        abortTagLink(reason){
            this.stopClock();this.kernel.api.Kernel_Abort();this.tagLink.stopActive(reason);this.broadcast();
        }
        startClock(){
            this.stopClock();
            if(this.tagLink.active)return;
            const players=[...this.peers.values()].filter(p=>p.joined);
            this.paced=players.length>1&&players.every(p=>(p.features&1)!==0);
            this.kernel.api.Kernel_Pacing(this.paced?1:0);
            for(const [ws,p]of this.peers)this.sendCapabilities(ws,p);
            if(this.paced){this.clockNext=Date.now()+120;this.clockTimer=setTimeout(()=>this.clockTick(),120);}
        }
        stopClock(){if(this.clockTimer!==null)clearTimeout(this.clockTimer);this.clockTimer=null;}
        clockTick(){
            this.clockTimer=null;
            if(this.tagLink.active)return;
            if(!this.paced||this.kernel.api.Kernel_Phase()!==5)return;
            const now=Date.now(),step=1000/34.2;let burst=0;
            while(now>=this.clockNext&&burst++<4){
                this.kernel.api.Kernel_DeadlineFrame();const packet=this.kernel.take();
                if(packet)this.sendFrame(packet);
                this.clockNext+=step;
            }
            // A delayed server callback must not emit an unbounded backlog in one event.
            if(this.clockNext<now-step*4)this.clockNext=now;
            this.clockTimer=setTimeout(()=>this.clockTick(),Math.max(1,this.clockNext-Date.now()));
        }
        syncDiagnostics(){
            const a=this.kernel.api,serial=a.Kernel_SyncSerial();if(serial===this.syncSerial)return;this.syncSerial=serial;
            const kind=a.Kernel_SyncKind(),previous=this.syncKind;this.syncKind=kind;
            if(kind!==2&&previous!==2)return;
            const row={event:'tag-sync',epoch:a.Kernel_Epoch(),frame:a.Kernel_SyncFrame(),kind,streak:a.Kernel_SyncStreak(),
                actor:a.Kernel_SyncActor(),field:a.Kernel_SyncField(),seatA:a.Kernel_SyncSeatA(),seatB:a.Kernel_SyncSeatB(),
                valueA:a.Kernel_SyncValueA(),valueB:a.Kernel_SyncValueB()};
            console.warn(JSON.stringify(row));
            const packet=new ArrayBuffer(512),v=new DataView(packet);v.setUint32(0,0x54540009,true);v.setUint32(4,103,true);v.setUint32(8,row.epoch,true);
            [row.frame,kind,row.streak,row.actor,row.field,row.seatA,row.seatB,row.valueA,row.valueB].forEach((x,i)=>v.setUint32(12+i*4,x>>>0,true));
            // Old clients do not understand notice packets. Send only to advertised/new clients.
            for(const [ws,p]of this.peers)if(p.detailedSync||(p.features&2)){try{ws.send(packet);}catch{this.close(ws);}}
        }
        sendFrame(packet){for(const [ws]of this.peers){try{ws.send(packet);}catch{this.close(ws);}}}
        frames(){let packet;while((packet=this.kernel.take()))this.sendFrame(packet);}
        watchConnections(){
            if(this.leaseTimer!==null||!this.peers.size)return;
            this.leaseTimer=setTimeout(()=>{this.leaseTimer=null;this.reapConnections();this.watchConnections();},30000);
        }
        reapConnections(){
            const now=Date.now();
            if(now-this.born>3*3600000){this.abort('Room expired');return;}
            // The existing clients send a heartbeat at least once per second.
            // Only missing traffic expires a connection; gameplay never waits
            // for this maintenance task. A quiet but connected lobby is kept.
            for(const [ws,p]of this.peers)if(Number.isFinite(p.lastMessage)&&now-p.lastMessage>(p.joined?120000:15000))this.close(ws);
        }
        close(ws){
            const peer=this.peers.get(ws);if(!peer)return;this.peers.delete(ws);
            try{ws.close(1000,'');}catch{}
            this.voice.drop(peer.id);
            if(!this.peers.size){
                this.stopClock();this.tagLink.shutdown();
                if(this.leaseTimer!==null)clearTimeout(this.leaseTimer);this.leaseTimer=null;
            }
            if(peer.joined){
                if(this.tagLink.active&&peer.id===this.tagLink.host){this.abortTagLink('Host disconnected');return;}
                this.kernel.api.Kernel_Leave(peer.id);this.broadcast();
            }
        }
        abort(why){
            this.stopClock();if(this.leaseTimer!==null)clearTimeout(this.leaseTimer);this.leaseTimer=null;
            this.tagLink.shutdown();
            for(const [ws]of this.voice.peers)this.voice.close(ws);
            for(const [ws]of this.peers){try{ws.close(1011,why);}catch{}}this.peers.clear();
        }
    };
}
