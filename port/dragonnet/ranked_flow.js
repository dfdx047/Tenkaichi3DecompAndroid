// Coordinated connection and a NEW match per accepted rematch. SPDX-License-Identifier: MIT
const stmt=(db,s,...a)=>db.prepare(s).bind(...a);
const one=(db,s,...a)=>stmt(db,s,...a).first();
const uuid=()=>crypto.randomUUID().replaceAll('-','');
export async function flowRow(db,match){return one(db,'SELECT * FROM ranked_flow WHERE match_id=?',match);}
export async function flowQueue(db,m,p,body){
 // Only explicitly upgraded clients use this protocol. Legacy games keep
 // their original simulation/service and never negotiate a rematch silently.
 if(body.flow!==2||m.state!=='waiting'||m.host_id!==p.id)return;
 const side=m.host_id===p.id?'host':'guest';
 await stmt(db,'INSERT OR IGNORE INTO ranked_flow(match_id) VALUES(?)',m.id).run();
 await stmt(db,`UPDATE ranked_flow SET ${side}_cap=2 WHERE match_id=? AND closed=0 AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND state='waiting')`,m.id,m.id).run();
}
export async function flowView(db,m){
 const f=await flowRow(db,m.id);return f&&f.host_cap===2&&f.guest_cap===2?2:0;
}
async function cancel(db,m,deadline=0){
 // Never discard a submitted result or cancel another match of either player.
 await db.batch([
  stmt(db,"UPDATE ranked_matches SET state='cancelled' WHERE id=? AND state IN ('waiting','paired') AND host_score IS NULL AND guest_score IS NULL AND NOT EXISTS(SELECT 1 FROM ranked_flow WHERE match_id=? AND started_mask<>0) AND (?=0 OR EXISTS(SELECT 1 FROM ranked_flow WHERE match_id=? AND ready_mask<>3 AND ((connect_at>0 AND connect_at<=?-7) OR (paired_at>0 AND paired_at<=?-90))))",m.id,m.id,deadline,m.id,deadline,deadline),
  stmt(db,"UPDATE ranked_flow SET closed=1 WHERE match_id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND state='cancelled')",m.id,m.id),
  stmt(db,"UPDATE ranked_players SET active_match=NULL WHERE active_match=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND state='cancelled')",m.id,m.id)
 ]);
}
export async function flowExpire(db,m,now){
 if(!m||m.state!=='paired')return;
 const f=await flowRow(db,m.id);
 if(f&&f.host_cap===2&&f.guest_cap===2&&f.ready_mask!==3&&((f.connect_at>0&&now>=f.connect_at+7)||(f.paired_at>0&&now>=f.paired_at+90)))await cancel(db,m,now);
}
export async function flowRequest(db,m,p,body,now,matchView){
 let f=await flowRow(db,m.id);
 if(!f||f.host_cap!==2||f.guest_cap!==2)return {status:409,body:{error:'lifecycle unsupported'}};
 const host=m.host_id===p.id,bit=host?1:2,side=host?'host':'guest',action=body.action;
 if(action==='started'){
  if(m.state!=='paired'||f.closed)return {body:{state:'cancelled'}};
  await stmt(db,'UPDATE ranked_flow SET started_mask=started_mask|? WHERE match_id=?',bit,m.id).run();
  return {body:{state:'started'}};
 }
 if(action==='connect'||action==='ready'){
  if(m.state!=='paired'||f.closed)return {body:{state:'cancelled'}};
  await stmt(db,"UPDATE ranked_flow SET connect_mask=connect_mask|?,ready_mask=ready_mask|? WHERE match_id=? AND closed=0",bit,action==='ready'?bit:0,m.id).run();
  await stmt(db,"UPDATE ranked_flow SET connect_at=? WHERE match_id=? AND connect_mask=3 AND connect_at=0",now,m.id).run();
  f=await flowRow(db,m.id);
  return {body:{state:f.ready_mask===3?'connected':'connecting',remaining:f.connect_at?Math.max(0,7-(now-f.connect_at)):7}};
 }
 if(action==='leave'){
  await stmt(db,`UPDATE ranked_flow SET ${side}_choice=2 WHERE match_id=?`,m.id).run();
  // A fresh rematch may have been reserved by a simultaneous accept. Release
  // THAT reservation only, before results; leave completed scores untouched.
  if(f.next_match){const next=await one(db,'SELECT * FROM ranked_matches WHERE id=?',f.next_match);if(next)await cancel(db,next);}
  return {body:{state:'declined'}};
 }
 if(!['accept','status'].includes(action))return {status:400,body:{error:'action'}};
 if(f.next_match){
  const next=await one(db,'SELECT * FROM ranked_matches WHERE id=?',f.next_match);
  if(!next||next.state!=='paired')return {body:{state:'declined'}};
  return {body:{...await matchView(db,next,p.id),flow:2,previous:m.id}};
 }
 if(f.host_choice===2||f.guest_choice===2||(f.choice_until&&now>=f.choice_until)){
  await stmt(db,'UPDATE ranked_flow SET closed=1 WHERE match_id=? AND next_match IS NULL',m.id).run();
  return {body:{state:'declined'}};
 }
 if(m.state!=='final')return {body:{state:m.state==='paired'?'confirming':'declined'}};
 if(action==='accept'){
  await stmt(db,`UPDATE ranked_flow SET ${side}_choice=1,choice_until=CASE WHEN choice_until=0 THEN ? ELSE choice_until END WHERE match_id=? AND closed=0 AND ${side}_choice=0`,now+15,m.id).run();
  f=await flowRow(db,m.id);
 }
 if(f.closed)return {body:{state:'declined'}};
 if(f.host_choice===1&&f.guest_choice===1){
  const next=uuid();
  // D1 batch is transactional. CAS chooses ONE UUID; only that winning
  // request inserts a new battle and snapshots the players' current ratings.
  await db.batch([
   stmt(db,`UPDATE ranked_flow SET next_match=? WHERE match_id=? AND next_match IS NULL AND closed=0 AND host_choice=1 AND guest_choice=1 AND choice_until>?
    AND EXISTS(SELECT 1 FROM ranked_players WHERE id=? AND active_match IS NULL)
    AND EXISTS(SELECT 1 FROM ranked_players WHERE id=? AND active_match IS NULL)`,next,m.id,now,m.host_id,m.guest_id),
   stmt(db,`INSERT INTO ranked_matches(id,host_id,guest_id,made,state,host_rating,guest_rating,rule_set,rematch_caps,version)
    SELECT ?,h.id,g.id,?,'paired',h.rating,g.rating,?,0,? FROM ranked_players h,ranked_players g
    WHERE h.id=? AND g.id=? AND EXISTS(SELECT 1 FROM ranked_flow WHERE match_id=? AND next_match=?)`,next,now,m.rule_set,m.version,m.host_id,m.guest_id,m.id,next),
   stmt(db,"INSERT INTO ranked_flow(match_id,host_cap,guest_cap,paired_at) SELECT ?,2,2,? WHERE EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)",next,now,next),
   stmt(db,"UPDATE ranked_players SET active_match=? WHERE id IN (?,?) AND active_match IS NULL AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)",next,m.host_id,m.guest_id,next)
  ]);
  f=await flowRow(db,m.id);
  if(f.next_match){const row=await one(db,'SELECT * FROM ranked_matches WHERE id=?',f.next_match);if(row)return {body:{...await matchView(db,row,p.id),flow:2,previous:m.id}};}
  return {body:{state:'declined'}};
 }
 return {body:{state:action==='accept'||f.host_choice||f.guest_choice?'waiting':'choice',remaining:f.choice_until?Math.max(0,f.choice_until-now):15}};
}
