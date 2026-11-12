export {VoiceDuelRoom} from './voice.mjs';
import kernelModule from './kernel.wasm';
import {createSessionClass} from './session.mjs';
import lobby from './lobby.js';
export class TagSession extends createSessionClass(kernelModule) {}
export default {
    async fetch(request,env,ctx) {
        const url=new URL(request.url);
        if(url.pathname==='/health')return Response.json({service:'BT4 Tag Team',build:'0.1.39.2-R8',maxPlayers:10,transport:'wss',protocol:0x54540009,compatibility:'BT4-B14-Rev2-sim2',syncDetail:2,pacedInput:true,tagSpeed:114,directInput:true,directSignalVersion:1});
        if(url.pathname.startsWith('/v3/voice/')){
            if(request.headers.get('Upgrade')?.toLowerCase()!=='websocket')return new Response('WebSocket required',{status:426});
            const match=url.pathname.match(/^\/v3\/voice\/(tag|duel)\/([A-HJ-NP-Z2-9]{6}|[a-f0-9]{64})$/);
            if(!match)return new Response('Invalid voice room',{status:400});
            const [,mode,code]=match;
            const minute=Math.floor(Date.now()/60000),ip=request.headers.get('CF-Connecting-IP')||'local';
            const count=await env.DB.prepare('INSERT INTO used(what,slot,n) VALUES(?,?,1) ON CONFLICT(what,slot) DO UPDATE SET n=n+1 RETURNING n').bind('voice-ws:'+ip,minute).first();
            if(count.n>60)return new Response('Too many voice connection attempts',{status:429});
            if(mode==='tag'){if(code.length!==6)return new Response('Invalid Tag room',{status:400});return env.TAG_SESSIONS.get(env.TAG_SESSIONS.idFromName(code)).fetch(request);}
            if(code.length!==64)return new Response('Invalid duel room',{status:400});return env.VOICE_DUELS.get(env.VOICE_DUELS.idFromName(code)).fetch(request);
        }
        if(url.pathname.startsWith('/v2/room/')||url.pathname.startsWith('/v4/taglink/')){
            const prefix=url.pathname.startsWith('/v4/taglink/')?'/v4/taglink/':'/v2/room/';
            const code=url.pathname.slice(prefix.length);
            if(!/^[A-HJ-NP-Z2-9]{6}$/.test(code))return new Response('Invalid room code',{status:400});
            if(request.headers.get('Upgrade')?.toLowerCase()!=='websocket')return new Response('WebSocket required',{status:426});
            const minute=Math.floor(Date.now()/60000),ip=request.headers.get('CF-Connecting-IP')||'local';
            const count=await env.DB.prepare('INSERT INTO used(what,slot,n) VALUES(?,?,1) ON CONFLICT(what,slot) DO UPDATE SET n=n+1 RETURNING n').bind('tag-ws:'+ip,minute).first();
            if(count.n>40)return new Response('Too many connection attempts. Try again shortly.',{status:429});
            await env.DB.prepare("DELETE FROM used WHERE what LIKE 'tag-ws:%' AND slot<?").bind(minute-60).run();
            const id=env.TAG_SESSIONS.idFromName(code);return env.TAG_SESSIONS.get(id).fetch(request);
        }
        return lobby.fetch(request,env,ctx);
    },
    async scheduled(event,env,ctx){return lobby.scheduled(event,env,ctx);}
};
