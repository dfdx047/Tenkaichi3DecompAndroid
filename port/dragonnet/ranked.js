// DBT-NX community ranked prototype. SPDX-License-Identifier: MIT
// Passwordless device credentials; never a Nintendo account or platform proof.
// Dragon Net: a search keeps the netcode version it was made with (ranked_matches.version), and pairs only with a
// search of the same version: a Switch of 40001 never meets an Android of 5001. Profiles and ratings are shared.
import {versionAllowed} from "./versions.js";
import {flowQueue,flowView,flowExpire,flowRequest} from './ranked_flow.js';
const TTL=21600, WAIT_TTL=120;
const json=(body,status=200)=>new Response(JSON.stringify(body),{status,headers:{"content-type":"application/json","cache-control":"no-store"}});
const id=()=>crypto.randomUUID().replaceAll("-","");
const clean=s=>String(s??"Player").replace(/[^a-zA-Z0-9 _.-]/g,"").slice(0,19)||"Player";
const hex=s=>typeof s==="string" && /^[a-f0-9]{32}$/.test(s);
async function hash(s){return [...new Uint8Array(await crypto.subtle.digest("SHA-256",new TextEncoder().encode(s)))].map(x=>x.toString(16).padStart(2,"0")).join("");}
const stmt=(db,s,...a)=>db.prepare(s).bind(...a);
const one=(db,s,...a)=>stmt(db,s,...a).first();
async function limit(db,key,slot,max){
 const row=await one(db,"INSERT INTO used(what,slot,n) VALUES(?,?,1) ON CONFLICT(what,slot) DO UPDATE SET n=n+1 RETURNING n",key,slot);
 return row.n<=max;
}
export function rankName(r){return r<900?"Bronce":r<1100?"Plata":r<1300?"Oro":r<1500?"Platino":r<1700?"Diamante":"Maestro";}
export function eloDelta(a,b,score){return Math.round(32*(score/2-1/(1+10**((b-a)/400))));}
async function view(db,p){
 const row=await one(db,"SELECT COUNT(*)+1 AS pos FROM ranked_players WHERE rating>? OR (rating=? AND id<?)",p.rating,p.rating,p.id);
 return {name:p.name,rating:p.rating,rank:rankName(p.rating),wins:p.wins,losses:p.losses,draws:p.draws,position:row.pos};
}
async function matchView(db,m,player){
 const host=m.host_id===player;
 const peer=await one(db,"SELECT name FROM ranked_players WHERE id=?",host?m.guest_id:m.host_id);
 return {match:m.id,state:m.state,role:host?1:2,code:m.room_code??"",peer:peer?.name??"",rules:m.rule_set,rematches:m.rematch_caps===3?1:0,flow:await flowView(db,m),delta:m.delta===null?0:host?m.delta:-m.delta};
}
async function rematchResult(db,m,p,body,now,forfeit=false){
 if(forfeit)body={...body,score:m.host_id===p.id?0:2,digest:'f0fe1700'};
 const n=body.round;
 if(!Number.isInteger(n) || n<2 || n>256 || ![0,1,2].includes(body.score) || !/^[a-f0-9]{8}$/.test(body.digest??""))return json({error:"round result"},400);
 let r=await one(db,"SELECT * FROM ranked_rounds WHERE match_id=? AND round_no=?",m.id,n);
 if(!r){
  if(m.state!=="final" || m.rematch_caps!==3 || m.made<now-TTL)return json({state:"cancelled",round:n});
  if(n>2){
   const previous=await one(db,"SELECT state FROM ranked_rounds WHERE match_id=? AND round_no=?",m.id,n-1);
   if(previous?.state!=="final")return json({state:previous && previous.state!=="paired"?"cancelled":"pending",round:n});
  }
  // Snapshot current ratings only when neither player has started another
  // search. A late retry of an existing round remains readable/idempotent.
  await db.batch([stmt(db,`INSERT OR IGNORE INTO ranked_rounds(match_id,round_no,made,host_rating,guest_rating)
   SELECT ?,?,?,h.rating,g.rating FROM ranked_players h,ranked_players g
   WHERE h.id=? AND g.id=? AND h.active_match IS NULL AND g.active_match IS NULL`,m.id,n,now,m.host_id,m.guest_id),
   stmt(db,"UPDATE ranked_players SET active_match=? WHERE id IN (?,?) AND active_match IS NULL AND EXISTS(SELECT 1 FROM ranked_rounds WHERE match_id=? AND round_no=? AND state='paired')",m.id,m.host_id,m.guest_id,m.id,n)]);
  r=await one(db,"SELECT * FROM ranked_rounds WHERE match_id=? AND round_no=?",m.id,n);
  if(!r)return json({state:"cancelled",round:n});
 }
 const side=m.host_id===p.id?"host":"guest";
 if(forfeit && r.state==="paired"){
  await stmt(db,"UPDATE ranked_rounds SET host_score=?,guest_score=?,host_digest=?,guest_digest=?,settlement=? WHERE match_id=? AND round_no=? AND state='paired' AND settlement IS NULL",body.score,body.score,body.digest,body.digest,'forfeit:'+p.id,m.id,n).run();
  r=await one(db,"SELECT * FROM ranked_rounds WHERE match_id=? AND round_no=?",m.id,n);
  body={...body,score:r.host_score,digest:r.host_digest};
 }
 if(r.state==="paired"){
  if(r[side+"_score"]!==null && (r[side+"_score"]!==body.score || r[side+"_digest"]!==body.digest))return json({error:"result locked"},409);
  await stmt(db,`UPDATE ranked_rounds SET ${side}_score=?,${side}_digest=? WHERE match_id=? AND round_no=? AND state='paired' AND ${side}_score IS NULL`,body.score,body.digest,m.id,n).run();
  r=await one(db,"SELECT * FROM ranked_rounds WHERE match_id=? AND round_no=?",m.id,n);
  if(r.host_score!==null && r.guest_score!==null){
   if(r.host_score!==r.guest_score || r.host_digest!==r.guest_digest){
    await stmt(db,"UPDATE ranked_rounds SET state='disputed' WHERE match_id=? AND round_no=? AND state='paired' AND host_score=? AND guest_score=? AND host_digest=? AND guest_digest=?",m.id,n,r.host_score,r.guest_score,r.host_digest,r.guest_digest).run();
   }else {
    const delta=r.host_score===1?0:eloDelta(r.host_rating,r.guest_rating,r.host_score),nonce=id();
    const hw=r.host_score===2?1:0,gw=r.host_score===0?1:0,draw=0;
    await db.batch([
     stmt(db,"UPDATE ranked_rounds SET state='final',delta=?,settlement=? WHERE match_id=? AND round_no=? AND state='paired' AND host_score=? AND guest_score=? AND host_digest=? AND guest_digest=?",delta,nonce,m.id,n,r.host_score,r.guest_score,r.host_digest,r.guest_digest),
     stmt(db,"UPDATE ranked_players SET rating=rating+?,wins=wins+?,losses=losses+?,draws=draws+? WHERE id=? AND EXISTS(SELECT 1 FROM ranked_rounds WHERE match_id=? AND round_no=? AND settlement=?)",delta,hw,gw,draw,m.host_id,m.id,n,nonce),
     stmt(db,"UPDATE ranked_players SET rating=rating-?,wins=wins+?,losses=losses+?,draws=draws+? WHERE id=? AND EXISTS(SELECT 1 FROM ranked_rounds WHERE match_id=? AND round_no=? AND settlement=?)",delta,gw,hw,draw,m.guest_id,m.id,n,nonce)
    ]);
   }
  }
 }
 r=await one(db,"SELECT * FROM ranked_rounds WHERE match_id=? AND round_no=?",m.id,n);
 if(r.state!=="paired")await stmt(db,"UPDATE ranked_players SET active_match=NULL WHERE active_match=? AND NOT EXISTS(SELECT 1 FROM ranked_rounds WHERE match_id=? AND state='paired')",m.id,m.id).run();
 return json({round:n,state:r.state,delta:r.delta===null?0:side==="host"?r.delta:-r.delta});
}
async function expire(db,p,now){
 if(!p.active_match)return;
 await flowExpire(db,await one(db,'SELECT * FROM ranked_matches WHERE id=?',p.active_match),now);
 await db.batch([
  stmt(db,"UPDATE ranked_rounds SET state='expired' WHERE match_id=? AND state='paired' AND made<?",p.active_match,now-TTL),
  stmt(db,"UPDATE ranked_matches SET state='expired' WHERE id=? AND (made<? OR (state='waiting' AND made<?)) AND state IN ('waiting','paired')",p.active_match,now-TTL,now-WAIT_TTL),
  stmt(db,"UPDATE ranked_players SET active_match=NULL WHERE active_match=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND state NOT IN ('waiting','paired')) AND NOT EXISTS(SELECT 1 FROM ranked_rounds WHERE match_id=? AND state='paired')",p.active_match,p.active_match,p.active_match)
 ]);
}
async function settle(db,m){
 if(m.state!=="paired" || m.host_score===null || m.guest_score===null)return;
 if(m.host_score!==m.guest_score || m.host_digest!==m.guest_digest){
  await db.batch([
   stmt(db,"UPDATE ranked_matches SET state='disputed' WHERE id=? AND state='paired' AND host_score=? AND guest_score=? AND host_digest=? AND guest_digest=?",m.id,m.host_score,m.guest_score,m.host_digest,m.guest_digest),
   stmt(db,"UPDATE ranked_players SET active_match=NULL WHERE active_match=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND state='disputed')",m.id,m.id)
  ]);return;
 }
 const delta=m.host_score===1?0:eloDelta(m.host_rating,m.guest_rating,m.host_score),nonce=id();
 const hwin=m.host_score===2?1:0,gwin=m.host_score===0?1:0,draw=0;
 await db.batch([
  stmt(db,"UPDATE ranked_matches SET state='final',delta=?,settlement=? WHERE id=? AND state='paired' AND host_score=? AND guest_score=? AND host_digest=? AND guest_digest=?",delta,nonce,m.id,m.host_score,m.guest_score,m.host_digest,m.guest_digest),
  stmt(db,"UPDATE ranked_players SET rating=rating+?,wins=wins+?,losses=losses+?,draws=draws+?,active_match=NULL WHERE id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND settlement=?)",delta,hwin,gwin,draw,m.host_id,m.id,nonce),
  stmt(db,"UPDATE ranked_players SET rating=rating-?,wins=wins+?,losses=losses+?,draws=draws+?,active_match=NULL WHERE id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND settlement=?)",delta,gwin,hwin,draw,m.guest_id,m.id,nonce)
 ]);
}
export async function rankedFetch(request,env,path,body,now,ip){
 if(String(env.RANKED_ENABLED)!=="1")return json({error:"ranked unavailable"},503);
 if(!versionAllowed(env,body.v))return json({error:"version"},409);
 const db=env.DB;
 if(path==='capabilities')return json({lifecycle:2,connect_timeout_ms:7000,rematch_timeout_ms:15000});
 if(path==="register"){
  if(!await limit(db,"ranked-register:"+ip,now-now%3600,10))return json({error:"busy"},429);
  const player=id(),token=id()+id();
  await stmt(db,"INSERT INTO ranked_players(id,token_hash,name,created) VALUES(?,?,?,?)",player,await hash(token),clean(body.name),now).run();
  return json({id:player,token});
 }
 if(!hex(body.id) || !/^[a-f0-9]{64}$/.test(body.token??""))return json({error:"identity"},401);
 let p=await one(db,"SELECT * FROM ranked_players WHERE id=?",body.id);
 if(!p || p.token_hash!==await hash(body.token))return json({error:"identity"},401);
 if(!await limit(db,"ranked-api:"+p.id,now-now%60,45))return json({error:"busy"},429);
 if(path==="profile"){
  await stmt(db,"UPDATE ranked_players SET name=? WHERE id=?",clean(body.name),p.id).run();p.name=clean(body.name);
  return json(await view(db,p));
 }
 if(path==="leaderboard"){
  const rows=await stmt(db,"SELECT name,rating,wins,losses,draws FROM ranked_players ORDER BY rating DESC,id LIMIT 10").all();
  return json({rows:rows.results.map((r,i)=>({...r,position:i+1,rank:rankName(r.rating)}))});
 }
 await expire(db,p,now);p=await one(db,"SELECT * FROM ranked_players WHERE id=?",p.id);
 if(path==="queue"){
  const rules=body.rules??'legacy';
  if(!['legacy','solo-inf-v1','team3-inf-v1'].includes(rules)&&!(rules==='dp20-inf-v1'&&String(env.DP20_ENABLED)==='1'&&body.flow===2))return json({error:'rules'},400);
  if(!p.active_match){
   if(!await limit(db,"ranked-start:"+p.id,now-now%3600,30))return json({error:"busy"},429);
   const mine=id();
   await db.batch([
    stmt(db,"INSERT INTO ranked_matches(id,host_id,made,host_rating,rule_set,rematch_caps,version) SELECT ?,?,?,?,?,?,? WHERE (SELECT active_match FROM ranked_players WHERE id=?) IS NULL",mine,p.id,now,p.rating,rules,body.rematches===1?1:0,body.v,p.id),
    stmt(db,"UPDATE ranked_players SET active_match=? WHERE id=? AND active_match IS NULL AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)",mine,p.id,mine),
    stmt(db,"INSERT OR IGNORE INTO ranked_flow(match_id,host_cap) SELECT ?,? WHERE EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)",mine,body.flow===2?2:0,mine)
   ]);
   p=await one(db,"SELECT * FROM ranked_players WHERE id=?",p.id);
  }
  let m=await one(db,"SELECT * FROM ranked_matches WHERE id=?",p.active_match);
  if(m.rule_set!==rules || m.version!==body.v)return json({error:'cancel current search first'},409);
  // A waiting assignment can survive a client update/restart. Refresh the
  // authenticated host's capability before pairing; after pairing it is
  // frozen, because either peer may already have handed the session over.
  if(m.state==="waiting" && body.rematches===1 && !(m.rematch_caps&1)){
   await stmt(db,"UPDATE ranked_matches SET rematch_caps=rematch_caps|1 WHERE id=? AND host_id=? AND state='waiting'",m.id,p.id).run();
   m=await one(db,"SELECT * FROM ranked_matches WHERE id=?",m.id);
  }
  if(m?.state==="waiting"){
   // FIFO within a widening Elo window. The older room is always the host.
   const window=200+Math.min(800,Math.floor((now-m.made)/15)*100);
   const other=await one(db,"SELECT m.* FROM ranked_matches m JOIN ranked_players p ON p.id=m.host_id AND p.active_match=m.id WHERE m.state='waiting' AND m.rule_set=? AND m.version=? AND m.id<>? AND m.made>=? AND ABS(m.host_rating-?)<=? ORDER BY m.made,m.id LIMIT 1",rules,body.v,m.id,now-WAIT_TTL,p.rating,window);
   if(other && (other.made<m.made || (other.made===m.made && other.id<m.id))){
    await db.batch([
     stmt(db,"UPDATE ranked_matches SET guest_id=?,guest_rating=?,state='paired',rematch_caps=rematch_caps|? WHERE id=? AND state='waiting' AND rule_set=? AND EXISTS(SELECT 1 FROM ranked_players WHERE id=? AND active_match=?) AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND state='waiting' AND rule_set=?)",p.id,p.rating,body.rematches===1?2:0,other.id,rules,p.id,m.id,m.id,rules),
     stmt(db,"UPDATE ranked_players SET active_match=? WHERE id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND guest_id=? AND state='paired')",other.id,p.id,other.id,p.id),
     stmt(db,"UPDATE ranked_matches SET state='superseded' WHERE id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND guest_id=?)",m.id,other.id,p.id),
     stmt(db,"UPDATE ranked_flow SET guest_cap=?,paired_at=? WHERE match_id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND guest_id=?)",body.flow===2?2:0,now,other.id,other.id,p.id)
    ]);
    p=await one(db,"SELECT * FROM ranked_players WHERE id=?",p.id);
   }
  }
  m=await one(db,"SELECT * FROM ranked_matches WHERE id=?",p.active_match);
  await flowQueue(db,m,p,body);
  return json(await matchView(db,m,p.id));
 }
 if(!hex(body.match))return json({error:"match"},400);
 let m=await one(db,"SELECT * FROM ranked_matches WHERE id=?",body.match);
 if(!m || (m.host_id!==p.id && m.guest_id!==p.id))return json({error:"match"},404);
 await flowExpire(db,m,now);m=await one(db,'SELECT * FROM ranked_matches WHERE id=?',m.id);
 if(path==='lifecycle'){const r=await flowRequest(db,m,p,body,now,matchView);return json(r.body,r.status??200);}
 if(path==="cancel"){
  if(m.state==='final'&&await flowView(db,m)===2)await flowRequest(db,m,p,{action:'leave'},now,matchView);
  // Leaving a screen after a legitimate finish must not erase a report
  // already submitted by either player. Await the other report or expiry.
  await db.batch([
   stmt(db,"UPDATE ranked_matches SET state='cancelled' WHERE id=? AND state IN ('waiting','paired') AND host_score IS NULL AND guest_score IS NULL AND NOT EXISTS(SELECT 1 FROM ranked_flow WHERE match_id=? AND started_mask<>0)",m.id,m.id),
   stmt(db,"UPDATE ranked_players SET active_match=NULL WHERE active_match=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND state NOT IN ('waiting','paired')) AND NOT EXISTS(SELECT 1 FROM ranked_rounds WHERE match_id=? AND state='paired')",m.id,m.id,m.id)
  ]);
 }else if(path==="room"){
  if(m.state!=="paired" || m.host_id!==p.id || !/^[A-Z2-9]{6}$/.test(body.code??""))return json({error:"room"},400);
  // The publisher must be on the network which created this live room.
  const room=await one(db,"SELECT * FROM rooms WHERE code=? AND made>=?",body.code,now-600);
  if(!room || room.ip!==ip || room.version!==m.version || room.join_key!==null)return json({error:"room"},409);
  await stmt(db,"UPDATE ranked_matches SET room_code=? WHERE id=? AND state='paired' AND room_code IS NULL",body.code,m.id).run();
 }else if(path==="forfeit"){
  const round=body.round??1;
  if(!Number.isInteger(round) || round<1 || round>256)return json({error:"round"},400);
  if(round>1)return rematchResult(db,m,p,{...body,round},now,true);
  // Only the authenticated participant can concede their OWN loss. A peer
  // cannot turn a timeout or desync into an accusation against the other.
  const score=m.host_id===p.id?0:2;
  await stmt(db,"UPDATE ranked_matches SET host_score=?,guest_score=?,host_digest='f0fe1700',guest_digest='f0fe1700',settlement=? WHERE id=? AND state='paired' AND settlement IS NULL",score,score,'forfeit:'+p.id,m.id).run();
  await settle(db,await one(db,"SELECT * FROM ranked_matches WHERE id=?",m.id));
 }else if(path==="result"){
  if(body.round!==undefined && body.round!==1)return rematchResult(db,m,p,body,now);
  if(![0,1,2].includes(body.score) || !/^[a-f0-9]{8}$/.test(body.digest??"") || m.state!=="paired")return m.state==="final"?json(await matchView(db,m,p.id)):json({error:"result"},409);
  const side=m.host_id===p.id?"host":"guest";
  // Column choice comes only from the authenticated participant, never input.
  if(m[side+"_score"]!==null && (m[side+"_score"]!==body.score || m[side+"_digest"]!==body.digest))return json({error:"result locked"},409);
  await stmt(db,`UPDATE ranked_matches SET ${side}_score=?,${side}_digest=? WHERE id=? AND state='paired' AND ${side}_score IS NULL`,body.score,body.digest,m.id).run();
  m=await one(db,"SELECT * FROM ranked_matches WHERE id=?",m.id);await settle(db,m);
 }else if(path!=="match")return json({error:"bad request"},404);
 m=await one(db,"SELECT * FROM ranked_matches WHERE id=?",m.id);
 return json(await matchView(db,m,p.id));
}
