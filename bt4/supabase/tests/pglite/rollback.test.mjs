// The ranked migrations 20261122000001/2 are reversible with nothing lost, and a failure in the middle of one leaves the database as it was.
//   npm test   (runs after the SQL and contract tests)
import {boot} from './harness.mjs';
import {readFileSync} from 'node:fs';
let fails=0;const ok=(c,w,x='')=>{console.log((c?'ok   ':'FAIL ')+w+(c?'':'  '+x));if(!c)fails++;};
const MIG=new URL('../../migrations/',import.meta.url).pathname,DOWN=new URL('../../migrations_down/',import.meta.url).pathname;
const up1=readFileSync(MIG+'20261122000001_ranked_pair_day_cap.sql','utf8'),up2=readFileSync(MIG+'20261122000002_ranked_farm_review.sql','utf8');
const down1=readFileSync(DOWN+'20261122000001_ranked_pair_day_cap.down.sql','utf8'),down2=readFileSync(DOWN+'20261122000002_ranked_farm_review.down.sql','utf8');

const {db,failed}=await boot({skip:['20261122']});            // the database as it is BEFORE these two migrations
if(failed.length){console.log('boot failed',failed);process.exit(2);}
const q=async(s,p)=>(await db.query(s,p)).rows;
const fnSrc=async()=>(await q(`select md5(prosrc) h from pg_proc where proname='ranked_record_result'`))[0].h;
const cols=async()=>(await q(`select column_name c from information_schema.columns where table_schema='bt4_private' and table_name in('ranked_matches','ranked_config') order by 1`)).map(r=>r.c).join(',');
const before={fn:await fnSrc(),cols:await cols()};

// 1. a failure in the middle of the migration changes nothing (it is one transaction)
let failedAsExpected=false;
try{await db.exec(up1.replace(/commit;\s*$/,'select 1/0; commit;'));}catch(e){failedAsExpected=true;try{await db.exec('rollback');}catch{}}
ok(failedAsExpected&&await fnSrc()===before.fn&&await cols()===before.cols&&(await q(`select to_regclass('bt4_private.ranked_pair_day') t`))[0].t===null,'a migration that fails half way leaves function, columns and tables exactly as they were');
await db.exec(up1);await db.exec(up2);
ok(await fnSrc()!==before.fn&&(await q(`select pair_day_cap::int c from bt4_private.ranked_config`))[0].c===0,'the migration applied: new function, and the new switch is 0 (off)');

// the scenario on a database that has the migration and the switch ON: 6 battles of one pair (4 score, 2 capped)
const api=async(u,op,args)=>{await db.exec(`select set_config('request.jwt.claim.sub','${u}',false)`);return (await q('select public.bt4_ranked_api($1,$2::jsonb) r',[op,JSON.stringify(args)]))[0].r;};
await db.exec(`update bt4_private.ranked_config set enabled=true,authority_backend=1,pair_day_cap=4; update bt4_private.route_config set backend_id=1,admission_open=true; select bt4_private.ranked_rollover();`);
const sid=(await q(`select (bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date s`))[0].s;
const ua=(await q('insert into auth.users default values returning id'))[0].id,ub=(await q('insert into auth.users default values returning id'))[0].id;
for(const [u,n] of [[ua,'A'],[ub,'B']]){await q(`insert into bt4_private.ranked_players(ruleset,user_id,name) values('solo',$1,$2)`,[u,n+u.slice(0,6)]);await q(`insert into bt4_private.ranked_profiles(season,ruleset,user_id,name,rating) select $1,'solo',$2,name,rating from bt4_private.ranked_players where ruleset='solo' and user_id=$2`,[sid,u]);}
const D='a'.repeat(64);
const battle=async()=>{const m=crypto.randomUUID();await q(`insert into bt4_private.ranked_matches(id,season,ruleset,player_a,player_b) values($1,$2,'solo',$3,$4)`,[m,sid,ua,ub]);await api(ua,'report',{match:m,outcome:0,digest:D});return {m,r:await api(ub,'report',{match:m,outcome:0,digest:D})};};
const ids=[];for(let i=0;i<6;i++)ids.push(await battle());
const players=async()=>JSON.stringify((await q(`select user_id=$1 isa,rating,wins,losses from bt4_private.ranked_players order by user_id=$1 desc`,[ua])));
const state0=await players();
ok(ids.slice(0,4).every(x=>!x.r.capped)&&ids.slice(4).every(x=>x.r.capped===true),'6 battles with the switch on: 4 score, 2 capped');

// 2. rollback: farm view first, then the cap; nothing lost
await db.exec(down2);await db.exec(down1);
ok(await fnSrc()===before.fn&&await cols()===before.cols,'after the rollback the function and the columns are exactly the ones from before the migration');
ok(await players()===state0,'ratings, wins and losses are untouched by the rollback');
const arch=(await q(`select (select count(*)::int from bt4_private.ranked_pair_day_archive) pd,(select rated from bt4_private.ranked_pair_day_archive) pr,(select battles from bt4_private.ranked_pair_day_archive) pb,(select count(*)::int from bt4_private.ranked_matches_meta_archive where capped) cp,(select v from bt4_private.ranked_rollback_config where k='pair_day_cap') cfg`))[0];
ok(arch.pd===1&&arch.pr===4&&arch.pb===6&&arch.cp===2&&arch.cfg==='4','the pair counter (4 rated of 6), the 2 capped matches and the switch (4) are archived, not dropped',JSON.stringify(arch));
const legacy=await battle(); // with the old function: no cap exists, the 7th battle scores as it did before the migration
ok(legacy.r.state==='applied'&&!legacy.r.capped,'with the rollback in place the pair scores as before the migration (no cap)',JSON.stringify(legacy.r));

// 3. the migration again: what was archived comes back
await db.exec(up1);await db.exec(up2);
const back=(await q(`select (select pair_day_cap::int from bt4_private.ranked_config) cfg,(select rated from bt4_private.ranked_pair_day) pr,(select count(*)::int from bt4_private.ranked_matches where capped) cp`))[0];
ok(back.cfg===4&&back.pr===4&&back.cp===2,'running the migration again restores the switch, the counter and the capped flags',JSON.stringify(back));
const after=await battle();
ok(after.r.capped===true,'and the limit holds again at once (the pair already used its 4 today)',JSON.stringify(after.r));
console.log(fails?fails+' FAILED':'rollback: reversible, nothing lost');process.exit(fails?1:0);
