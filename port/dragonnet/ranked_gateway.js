// Identical public gateway on C and D; no local ratings/database.
// SPDX-License-Identifier: MIT
const paths=new Set(['host','join','poll','addrs','leave','relay','hello',
 ...['capabilities','register','profile','leaderboard','queue','match','room','cancel','result','forfeit','lifecycle'].map(x=>'ranked/'+x)]);
export const json=(body,status=200)=>new Response(JSON.stringify(body),{status,headers:{'content-type':'application/json','cache-control':'no-store'}});
export function gatewayConfig(env){
 return ['C','D'].includes(env.GATEWAY_ID)&&/^https:\/\/[a-z0-9.-]+$/.test(env.AUTHORITY_URL??'')&&
 /^[a-zA-Z0-9_-]{16,64}$/.test(env.AUTHORITY_ID??'')&&String(env.AUTHORITY_KEY??'').length>=32;
}
export default {
 async fetch(request,env){
  const url=new URL(request.url);
  if(request.method==='GET'&&url.pathname==='/gateway-health')return json({gateway:env.GATEWAY_ID??'',authority:env.AUTHORITY_ID??''},gatewayConfig(env)?200:503);
  if(request.method==='GET'&&url.pathname==='/')return new Response('Dragon Net: pasarela Ranked C/D. Servicio no oficial; requiere una copia ORIGINAL del juego obtenida legalmente. Fuentes y licencias: /source.\n');
  if(request.method==='GET'&&url.pathname==='/source')return Response.redirect('https://gitlab.com/alexyt2021es/DBT-NX',302);
  const path=url.pathname.startsWith('/v1/')?url.pathname.slice(4):'';
  if(request.method!=='POST'||!paths.has(path))return json({error:'not found'},404);
  if(!gatewayConfig(env))return json({error:'ranked unavailable'},503);
  const raw=await request.text();if(raw.length>2048)return json({error:'bad request'},413);
  let body;try{body=JSON.parse(raw);}catch{return json({error:'bad request'},400);}
  if(!body||typeof body!=='object'||Array.isArray(body))return json({error:'bad request'},400);
  try{
   // Ignore caller-provided forwarded IP/authorization/gateway headers.
   const r=await fetch(env.AUTHORITY_URL+'/_ranked/dispatch',{
    method:'POST',headers:{'content-type':'application/json','authorization':'Bearer '+env.AUTHORITY_KEY},
    body:JSON.stringify({gateway:env.GATEWAY_ID,path,body,ip:request.headers.get('cf-connecting-ip')??''}),signal:AbortSignal.timeout(9000)
   });
   // Do not leak authority service headers or secret diagnostics.
   return new Response(r.body,{status:r.status,headers:{'content-type':'application/json','cache-control':'no-store'}});
  }catch{return json({error:'ranked unavailable'},503);}
 }
};
