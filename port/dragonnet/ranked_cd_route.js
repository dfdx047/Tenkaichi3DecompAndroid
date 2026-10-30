// Central routing only; gameplay, delay and UDP stay unchanged. SPDX-License-Identifier: MIT
const stmt=(db,s,...v)=>db.prepare(s).bind(...v);
export async function currentRoute(db,now){
 const day=Math.floor(now/86400);
 const current=await stmt(db,'SELECT * FROM ranked_service_route WHERE id=1').first();
 if(current&&current.day>=day)return current;
 await db.batch([
  stmt(db,"INSERT OR IGNORE INTO ranked_service_route(id,day,backend,epoch) VALUES(1,?,'C',1)",day),
  stmt(db,"UPDATE ranked_service_route SET day=?,backend='C',epoch=epoch+1 WHERE id=1 AND day<?",day,day)
 ]);
 return stmt(db,'SELECT * FROM ranked_service_route WHERE id=1').first();
}
export async function promoteD(db,now){
 const route=await currentRoute(db,now);
 await stmt(db,"UPDATE ranked_service_route SET backend='D',epoch=epoch+1 WHERE id=1 AND day=? AND backend='C'",route.day).run();
 return stmt(db,'SELECT * FROM ranked_service_route WHERE id=1').first();
}
export async function retireUnstartedC(db,now,knownRoute=null){
 const route=knownRoute??await currentRoute(db,now);if(route.backend!=='D')return;
 // One transaction: both peers are released together, never score a network
 // outage as a loss. A fight with either a start marker or a report survives.
 await db.batch([
  stmt(db,`UPDATE ranked_matches SET state='cancelled' WHERE state IN ('waiting','paired')
   AND host_score IS NULL AND guest_score IS NULL
   AND id IN (SELECT match_id FROM ranked_match_route WHERE backend='C')
   AND NOT EXISTS(SELECT 1 FROM ranked_flow f WHERE f.match_id=ranked_matches.id AND (f.started_mask<>0 OR f.ready_mask=3))`),
  stmt(db,`UPDATE ranked_flow SET closed=1 WHERE match_id IN
   (SELECT m.id FROM ranked_matches m JOIN ranked_match_route r ON r.match_id=m.id WHERE m.state='cancelled' AND r.backend='C')`),
  stmt(db,`UPDATE ranked_players SET active_match=NULL WHERE active_match IN
   (SELECT m.id FROM ranked_matches m JOIN ranked_match_route r ON r.match_id=m.id WHERE m.state='cancelled' AND r.backend='C')`)
 ]);
}
export async function matchRoute(db,match,now){
 await currentRoute(db,now);
 await stmt(db,`INSERT OR IGNORE INTO ranked_match_route(match_id,backend,epoch)
  SELECT ?,backend,epoch FROM ranked_service_route WHERE id=1
  AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)`,match,match).run();
 return stmt(db,'SELECT backend,epoch FROM ranked_match_route WHERE match_id=?',match).first();
}
