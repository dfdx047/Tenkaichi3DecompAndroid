// Contract test between the two authorities of the ranked rules: the SAME scenario is played on the Worker (D1, port/bt4online-worker) and on the SQL (Postgres via PGlite) and the
// ratings and counters must agree battle by battle. Each layer keeps its own database (Android on D1, PC on Supabase); this proves the RULES are the same, not that the data are shared.
//   cd supabase/tests/pglite && npm test        (runs after ranked_sql.test.mjs)
import {boot} from './harness.mjs';
import {boot as bootWorker} from '../../../port/bt4online-worker/tests/harness.mjs';
let fails=0;const ok=(c,w,x='')=>{console.log((c?'ok   ':'FAIL ')+w+(c?'':'  '+x));if(!c)fails++;};

// the scenario: one pair, 7 battles in one day; the winner side per battle (0 = first player). Battles 1-4 score, 5-7 are capped (the day's limit).
const winners=[0,0,1,0,1,0,0];

// ---- Worker
const hw=bootWorker();
const regs=async(n)=>hw.register(n+Math.floor(Math.random()*1e6).toString(36),'198.51.100.'+(1+Math.floor(Math.random()*200)));
const A=await regs('ContrA'),B=await regs('ContrB');
const code=await hw.rankedRoom(A,B,true);
const workerSeq=[];
for(let i=0;i<winners.length;i++){
  const n=i+1,w=winners[i];
  await hw.rep(A,{rkey:hw.rk(),code,n,me:A.name,opp:B.name,side:0,winner:w,ranked:true});hw.clock.advance(11000);
  const r=await hw.rep(B,{rkey:hw.rk(),code,n,me:B.name,opp:A.name,side:1,winner:w,ranked:true});hw.clock.advance(11000);
  const ma=await hw.post('me',hw.auth(A)),mb=await hw.post('me',hw.auth(B));
  workerSeq.push({ra:ma.rating,rb:mb.rating,wa:ma.rwins,wb:mb.rwins,capped:!!r.capped});
}

// ---- SQL
const {db}=await boot();
const q=async(s,p)=>(await db.query(s,p)).rows;
await db.exec(`update bt4_private.ranked_config set enabled=true,authority_backend=1,pair_day_cap=4; update bt4_private.route_config set backend_id=1,admission_open=true; select bt4_private.ranked_rollover();`);
const sid=(await q(`select (bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date s`))[0].s;
const ua=(await q('insert into auth.users default values returning id'))[0].id,ub=(await q('insert into auth.users default values returning id'))[0].id;
for(const [u,n] of [[ua,'A'],[ub,'B']]){
  await q(`insert into bt4_private.ranked_players(ruleset,user_id,name) values('solo',$1,$2)`,[u,n+u.slice(0,6)]);
  await q(`insert into bt4_private.ranked_profiles(season,ruleset,user_id,name,rating) select $1,'solo',$2,name,rating from bt4_private.ranked_players where ruleset='solo' and user_id=$2`,[sid,u]);
}
const api=async(u,op,args)=>{await db.exec(`select set_config('request.jwt.claim.sub','${u}',false)`);return (await q('select public.bt4_ranked_api($1,$2::jsonb) r',[op,JSON.stringify(args)]))[0].r;};
const D='a'.repeat(64);const sqlSeq=[];
for(let i=0;i<winners.length;i++){
  const m=crypto.randomUUID();
  await q(`insert into bt4_private.ranked_matches(id,season,ruleset,player_a,player_b) values($1,$2,'solo',$3,$4)`,[m,sid,ua,ub]);
  const outcome=winners[i]===0?0:1; // SQL: 0 = player_a wins, 1 = player_b wins (A is player_a)
  await api(ua,'report',{match:m,outcome,digest:D});const r=await api(ub,'report',{match:m,outcome,digest:D});
  const pa=(await q(`select rating,wins from bt4_private.ranked_players where ruleset='solo' and user_id=$1`,[ua]))[0],pb=(await q(`select rating,wins from bt4_private.ranked_players where ruleset='solo' and user_id=$1`,[ub]))[0];
  sqlSeq.push({ra:pa.rating,rb:pb.rating,wa:pa.wins,wb:pb.wins,capped:!!r.capped});
}

console.log('battle | worker (A rating, B rating, wins A/B, capped) | sql (same)');
for(let i=0;i<winners.length;i++){const w=workerSeq[i],s=sqlSeq[i];console.log(`  ${i+1}    | ${w.ra} ${w.rb} ${w.wa}/${w.wb} ${w.capped} | ${s.ra} ${s.rb} ${s.wa}/${s.wb} ${s.capped}`);}
for(let i=0;i<winners.length;i++){
  const w=workerSeq[i],s=sqlSeq[i];
  ok(w.ra===s.ra&&w.rb===s.rb&&w.wa===s.wa&&w.wb===s.wb&&w.capped===s.capped,`battle ${i+1}: Worker and SQL agree on ratings, wins and the daily limit`,JSON.stringify([w,s]));
}
ok(workerSeq.filter(x=>!x.capped).length===4&&sqlSeq.filter(x=>!x.capped).length===4,'both layers score exactly 4 of the 7 battles');
console.log(fails?fails+' FAILED':'contract: both layers agree');process.exit(fails?1:0);
