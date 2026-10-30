// Single authoritative D1 for C/D: existing settlement/flow:2 implementations.
// Private business API only, never arbitrary SQL. SPDX-License-Identifier: MIT
import service from './worker.js';
import {json} from './ranked_gateway.js';
import {currentRoute,promoteD,retireUnstartedC,matchRoute} from './ranked_cd_route.js';
import {versionAllowed} from './versions.js';
const paths=new Set(['host','join','poll','addrs','leave','relay','hello',
 ...['capabilities','register','profile','leaderboard','queue','match','room','cancel','result','forfeit','lifecycle'].map(x=>'ranked/'+x)]);
async function equalSecret(a,b){
 if(typeof a!=='string'||typeof b!=='string'||b.length<32||a.length>256)return false;
 const digest=s=>crypto.subtle.digest('SHA-256',new TextEncoder().encode(s));
 const [x,y]=await Promise.all([digest(a),digest(b)]);const u=new Uint8Array(x),v=new Uint8Array(y);let difference=0;
 for(let i=0;i<u.length;i++)difference|=u[i]^v[i];return difference===0;
}
async function cUnavailable(env){
 try{
  const r=await fetch(env.URL_C+'/gateway-health',{signal:AbortSignal.timeout(2500),redirect:'error'});
  if(r.status>=500||r.status===429)return true;
  if(r.status!==200)return false; // Misconfiguration is not quota exhaustion.
  return false;
 }catch{return true;}
}
export default {
 async fetch(request,env){
  if(request.method!=='POST'||new URL(request.url).pathname!=='/_ranked/dispatch')return json({error:'not found'},404);
  const raw=await request.text();if(raw.length>4096)return json({error:'bad request'},413);
  let call;try{call=JSON.parse(raw);}catch{return json({error:'bad request'},400);}
  if(!call||!['C','D'].includes(call.gateway))return json({error:'unauthorized'},401);
  const secret=call.gateway==='C'?env.GATEWAY_C_KEY:env.GATEWAY_D_KEY;
  if(!await equalSecret(request.headers.get('authorization')?.replace(/^Bearer /,''),secret))return json({error:'unauthorized'},401);
  if(!paths.has(call.path)||!call.body||typeof call.body!=='object'||Array.isArray(call.body)||typeof call.ip!=='string'||call.ip.length>64)return json({error:'bad request'},400);
  if(String(env.MIGRATION_COMPLETE)!=='1'||!env.DB||!env.AUTHORITY_ID||!env.URL_C||!env.URL_D)return json({error:'migration incomplete'},503);
  const now=Math.floor(Date.now()/1000);
  try{
   let route=await currentRoute(env.DB,now);
   if(call.gateway==='D'&&route.backend==='C'&&await cUnavailable(env))route=await promoteD(env.DB,now);
   await retireUnstartedC(env.DB,now,route);
   const metadata={cd:1,migration_complete:1,authority:env.AUTHORITY_ID,url_c:env.URL_C,url_d:env.URL_D,backend:route.backend,route_epoch:route.epoch};
   if(call.path==='ranked/capabilities')return versionAllowed(env,call.body.v)?json({...metadata,lifecycle:2,connect_timeout_ms:7000,rematch_timeout_ms:15000}):json({error:'version',...metadata},409);
   const inner=new Request('https://authority.internal/v1/'+call.path,{method:'POST',headers:{'content-type':'application/json','cf-connecting-ip':call.ip},body:JSON.stringify(call.body)});
   const response=await service.fetch(inner,{...env,RANKED_ENABLED:'1',CHAT_ENABLED:'0'});
   let result;try{result=await response.json();}catch{return json({error:'ranked unavailable'},503);}
   if(result.match){
    const assigned=await matchRoute(env.DB,result.match,now);
    if(assigned){metadata.backend=assigned.backend;metadata.route_epoch=assigned.epoch;}
   }
   // A route may have changed concurrently while queue/room work was in flight.
   await retireUnstartedC(env.DB,now);
   if(result.match&&['waiting','paired'].includes(result.state)){
    const row=await env.DB.prepare('SELECT state FROM ranked_matches WHERE id=?').bind(result.match).first();
    if(row?.state==='cancelled'){result.state='cancelled';result.code='';}
   }
   return json({...result,...metadata},response.status);
  }catch{return json({error:'ranked unavailable'},503);}
 }
};
