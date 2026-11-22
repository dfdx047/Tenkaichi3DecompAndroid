import {boot} from './harness.mjs';
const {db,failed}=await boot();
if(failed.length){console.log('migration failures',failed);process.exit(2);}
let fails=0;const ok=(c,w,x='')=>{console.log((c?'ok   ':'FAIL ')+w+(c?'':'  '+x));if(!c)fails++;};
const q=async(s,p)=>(await db.query(s,p)).rows;
const as=async(u)=>{await db.exec(`select set_config('request.jwt.claim.sub','${u}',false)`);};
const api=async(u,op,args)=>{await as(u);return (await q('select public.bt4_ranked_api($1,$2::jsonb) r',[op,JSON.stringify(args)]))[0].r;};
const user=async()=>(await q('insert into auth.users default values returning id'))[0].id;
await db.exec(`update bt4_private.ranked_config set enabled=true,authority_backend=1; update bt4_private.route_config set backend_id=1,admission_open=true;`);
await db.exec(`select bt4_private.ranked_rollover()`);
const sid=(await q(`select (bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date s`))[0].s;
const admit=async(a,b,id)=>{ // what admission writes: the two players' ledger rows and the match
  for(const [u,n] of [[a,'A'],[b,'B']]){
    await q(`insert into bt4_private.ranked_players(ruleset,user_id,name) values('solo',$1,$2) on conflict do nothing`,[u,n+u.slice(0,6)]);
    await q(`insert into bt4_private.ranked_profiles(season,ruleset,user_id,name,rating) select $1,'solo',$2,name,rating from bt4_private.ranked_players where ruleset='solo' and user_id=$2 on conflict do nothing`,[sid,u]);
  }
  await q(`insert into bt4_private.ranked_matches(id,season,ruleset,player_a,player_b) values($1,$2,'solo',$3,$4)`,[id,sid,a,b]);
};
const rating=async(u)=>(await q(`select rating,wins,losses from bt4_private.ranked_players where ruleset='solo' and user_id=$1`,[u]))[0];
const hist=async(m)=>(await q(`select count(*)::int n from bt4_private.ranked_rating_history where match_id=$1`,[m]))[0].n;
const D='a'.repeat(64);

{ // S1: two agreeing reports count once; repeats and a late third call change nothing
  const a=await user(),b=await user(),m=crypto.randomUUID();await admit(a,b,m);
  const r1=await api(a,'report',{match:m,outcome:0,digest:D});
  ok(r1.state==='waiting','S1 first report waits',JSON.stringify(r1));
  const r2=await api(b,'report',{match:m,outcome:0,digest:D});
  ok(r2.state==='applied','S1 second agreeing report applies',JSON.stringify(r2));
  const r3=await api(a,'report',{match:m,outcome:0,digest:D}),r4=await api(b,'report',{match:m,outcome:0,digest:D});
  ok(r3.state==='already_applied'&&r4.state==='already_applied','S1 repeats are idempotent',JSON.stringify([r3,r4]));
  const ra=await rating(a),rb=await rating(b);
  ok(ra.rating===1016&&rb.rating===984&&ra.wins===1&&rb.losses===1&&await hist(m)===2,'S1 points applied exactly once (1016 / 984, 2 history rows)',JSON.stringify([ra,rb]));
  const lie=await api(b,'report',{match:m,outcome:1,digest:D});
  ok(lie.error==='RESULT_CONFLICT'||lie.state==='already_applied','S1 a changed claim after the result is refused or ignored',JSON.stringify(lie));
  ok((await rating(a)).rating===1016,'S1 the late contradicting claim did not move points');
}
{ // S2: contradicting reports award nothing; the match stays unscored
  const a=await user(),b=await user(),m=crypto.randomUUID();await admit(a,b,m);
  await api(a,'report',{match:m,outcome:0,digest:D});
  const d=await api(b,'report',{match:m,outcome:1,digest:D});
  ok(d.state==='disputed'&&(await rating(a)).rating===1000&&(await rating(b)).rating===1000,'S2 disputed: nobody scores',JSON.stringify(d));
}
{ // S3: a stranger cannot report a match
  const a=await user(),b=await user(),c=await user(),m=crypto.randomUUID();await admit(a,b,m);
  const r=await api(c,'report',{match:m,outcome:0,digest:D});
  ok(r.error==='NOT_A_PARTICIPANT','S3 a non-participant is refused',JSON.stringify(r));
}
{ // S4: a report of a match nobody admitted, and bad inputs
  const a=await user();
  ok((await api(a,'report',{match:crypto.randomUUID(),outcome:0,digest:D})).error==='NOT_A_PARTICIPANT','S4 unknown match refused');
  ok((await api(a,'report',{match:crypto.randomUUID(),outcome:7,digest:D})).error==='BAD_RESULT','S4 outcome out of range refused');
  ok((await api(a,'report',{match:crypto.randomUUID(),outcome:0,digest:'zz'})).error==='BAD_DIGEST','S4 bad digest refused');
  ok((await api(a,'report',{match:'not-a-uuid',outcome:0,digest:D})).error==='BAD_REQUEST','S4 malformed id refused');
}
{ // S5: a result in a match that has already been scored cannot be re-scored by the abandonment path
  const a=await user(),b=await user(),m=crypto.randomUUID();await admit(a,b,m);
  await api(a,'report',{match:m,outcome:0,digest:D});await api(b,'report',{match:m,outcome:0,digest:D});
  const f=await api(b,'forfeit',{match:m,reason:'local_quit'});
  ok(f.state==='already_applied'&&(await rating(a)).rating===1016&&await hist(m)===2,'S5 a forfeit after the result does not score again',JSON.stringify(f));
}
{ // S6: an explicit quit before the opponent ever started (a failed handshake) changes no rating
  const a=await user(),b=await user(),m=crypto.randomUUID();await admit(a,b,m);
  const f=await api(a,'forfeit',{match:m,reason:'local_quit'});
  ok(f.state==='waiting'&&(await rating(a)).rating===1000&&(await rating(b)).rating===1000,'S6 quitting a never-started match is free',JSON.stringify(f));
  const hb=await api(b,'heartbeat',{match:m});
  ok(hb.state==='waiting'&&(await rating(a)).rating===1000,'S6 and the opponent starting later does not punish the early quitter',JSON.stringify(hb));
}
{ // S7: both started, one stops heartbeating; the survivor claiming peer_gone wins only after 45 s of silence
  const a=await user(),b=await user(),m=crypto.randomUUID();await admit(a,b,m);
  await api(a,'heartbeat',{match:m});await api(b,'heartbeat',{match:m});
  const early=await api(a,'heartbeat',{match:m,peer_gone:true});
  ok(early.state==='waiting','S7 peer_gone right away is not enough',JSON.stringify(early));
  await q(`update bt4_private.ranked_matches set seen_b=now()-interval '60 seconds' where id=$1`,[m]);
  const late=await api(a,'heartbeat',{match:m,peer_gone:true});
  ok(late.state==='applied'&&(await rating(a)).rating===1016,'S7 after 60 s of silence the survivor wins the fixed 16',JSON.stringify(late));
}
{ // S8: the survivor of a match whose opponent has an honest, contradicting report cannot take a timeout award
  const a=await user(),b=await user(),m=crypto.randomUUID();await admit(a,b,m);
  await api(a,'heartbeat',{match:m});await api(b,'heartbeat',{match:m});
  await api(b,'report',{match:m,outcome:1,digest:D}); // B says B won
  await q(`update bt4_private.ranked_matches set seen_b=now()-interval '60 seconds' where id=$1`,[m]);
  const t=await api(a,'heartbeat',{match:m,peer_gone:true});
  ok(t.state==='waiting'&&(await rating(a)).rating===1000,'S8 a timeout cannot override the opponent\'s contradicting report',JSON.stringify(t));
}
{ // S8b: the cap ships OFF: with the switch at 0 ten battles of one pair all score, exactly as before the migration, and no counter is touched
  const a=await user(),b=await user();let scored=0;
  for(let i=0;i<10;i++){const m=crypto.randomUUID();await admit(a,b,m);await api(a,'report',{match:m,outcome:0,digest:D});const r=await api(b,'report',{match:m,outcome:0,digest:D});if(r.state==='applied'&&!r.capped)scored++;}
  ok((await q(`select pair_day_cap::int c from bt4_private.ranked_config`))[0].c===0,'S8b the switch is 0 after the migration (new settings ship off)');
  ok(scored===10&&(await q(`select count(*)::int n from bt4_private.ranked_pair_day`))[0].n===0,'S8b with the switch off 10 of 10 battles of one pair score and no pair counter exists',String(scored));
}
await db.exec("update bt4_private.ranked_config set pair_day_cap=4"); // from here on the limit is switched on (validation)
{ // S9: the pair's daily limit (4) is enforced in the database; the first four score, the rest are recorded and change nothing
  const a=await user(),b=await user();const states=[];
  for(let i=0;i<10;i++){const m=crypto.randomUUID();
    if(i%2===0)await admit(a,b,m);else await admit(b,a,m); // the pair in either order shares one counter
    const p1=i%2===0?a:b,p2=i%2===0?b:a;
    await api(p1,'report',{match:m,outcome:0,digest:D});const r=await api(p2,'report',{match:m,outcome:0,digest:D});states.push(r.capped?'capped':r.state);}
  const rated=states.filter(x=>x==='applied').length;
  ok(rated===4&&states.slice(4).every(x=>x==='capped'),'S9 10 battles of one pair in one day: 4 score, 6 are capped',states.join(','));
  const ra=await rating(a),rb=await rating(b);
  ok(ra.wins+ra.losses===4&&rb.wins+rb.losses===4,'S9 wins/losses count only the 4 rated battles',JSON.stringify([ra,rb]));
  ok(ra.rating+rb.rating===2000,'S9 ratings still conserve points (2000 together)',JSON.stringify([ra,rb]));
  ok((await q(`select count(*)::int n from bt4_private.ranked_rating_history h join bt4_private.ranked_matches m on m.id=h.match_id where h.user_id=$1 and m.player_a in ($1,$2) and m.player_b in ($1,$2)`,[a,b]))[0].n===4,'S9 history holds only the rated battles (capped ones are not eligibility)');
  // S10: another pair is not limited; the next (Brasilia) day starts the counter again; replays and contradictions of a capped match behave
  const c=await user(),m2=crypto.randomUUID();await admit(c,a,m2);await api(c,'report',{match:m2,outcome:0,digest:D});
  ok((await api(a,'report',{match:m2,outcome:0,digest:D})).state==='applied','S10 another pair the same day is unaffected');
  await q(`update bt4_private.ranked_pair_day set day=day-1 where pa=$1 and pb=$2`,[a<b?a:b,a<b?b:a]);
  const m3=crypto.randomUUID();await admit(a,b,m3);await api(a,'report',{match:m3,outcome:0,digest:D});
  ok((await api(b,'report',{match:m3,outcome:0,digest:D})).state==='applied','S10 a new day starts the pair\'s counter again');
  // a capped match replayed / contradicted
  const d=await user(),e=await user();let capMatch;
  for(let i=0;i<5;i++){const m=crypto.randomUUID();await admit(d,e,m);await api(d,'report',{match:m,outcome:0,digest:D});await api(e,'report',{match:m,outcome:0,digest:D});capMatch=m;}
  const rep1=await api(d,'report',{match:capMatch,outcome:0,digest:D}),rep2=await api(e,'report',{match:capMatch,outcome:1,digest:D});
  ok(rep1.state==='already_applied'&&(rep2.error==='RESULT_CONFLICT'||rep2.state==='already_applied'),'S10 a capped match replayed is idempotent and a contradiction is refused',JSON.stringify([rep1,rep2]));
  ok((await q(`select rated,capped from bt4_private.ranked_matches where id=$1`,[capMatch]))[0].capped===true,'S10 the capped match is marked capped and not rated');
}
{ // S11: a forfeit counts as a battle of the pair's day (the fixed 16 points obey the same limit)
  const a=await user(),b=await user();
  for(let i=0;i<4;i++){const m=crypto.randomUUID();await admit(a,b,m);await api(a,'report',{match:m,outcome:0,digest:D});await api(b,'report',{match:m,outcome:0,digest:D});}
  const m=crypto.randomUUID();await admit(a,b,m);await api(a,'heartbeat',{match:m});await api(b,'heartbeat',{match:m});
  const before=await rating(a);
  const f=await api(b,'forfeit',{match:m,reason:'local_quit'});
  ok(f.capped===true&&(await rating(a)).rating===before.rating,'S11 a 5th battle of the day by forfeit scores nothing',JSON.stringify(f));
}
{ // S12: duration telemetry: measured by the server from the later first heartbeat; absent when no heartbeat; nothing gates on it
  const a=await user(),b=await user(),c=await user(),d=await user(),m=crypto.randomUUID(),m2=crypto.randomUUID();
  await admit(a,b,m);await api(a,'heartbeat',{match:m});await api(b,'heartbeat',{match:m});
  await q(`update bt4_private.ranked_matches set started_a=now()-interval '95 seconds',started_b=now()-interval '90 seconds' where id=$1`,[m]);
  await api(a,'report',{match:m,outcome:0,digest:D});await api(b,'report',{match:m,outcome:0,digest:D});
  const ms=(await q(`select duration_ms from bt4_private.ranked_matches where id=$1`,[m]))[0].duration_ms;
  ok(ms>=89000&&ms<=95000,'S12 the duration is measured from the later of the two first heartbeats (about 90 s)',String(ms));
  await admit(c,d,m2);await api(c,'report',{match:m2,outcome:0,digest:D});await api(d,'report',{match:m2,outcome:0,digest:D});
  ok((await q(`select duration_ms from bt4_private.ranked_matches where id=$1`,[m2]))[0].duration_ms===null&&(await rating(c)).rating===1016,'S12 with no heartbeat the duration is null and the result still scores (no minimum is enforced)');
}
{ // S13: the farm review aid flags a farm (one account winning its 4-a-day from 3 accounts for 2 days) and does not flag organic play (12 wins from 12 different rivals)
  const farmer=await user(),sy=[await user(),await user(),await user()];
  for(let day=0;day<2;day++){
    for(const s2 of sy){for(let i=0;i<4;i++){const m=crypto.randomUUID();await admit(farmer,s2,m);await api(farmer,'report',{match:m,outcome:0,digest:D});await api(s2,'report',{match:m,outcome:0,digest:D});}}
    await q(`update bt4_private.ranked_pair_day set day=day-1`); // the next day: the counters start again
  }
  const organic=await user();for(let i=0;i<12;i++){const o=await user(),m=crypto.randomUUID();await admit(organic,o,m);await api(organic,'report',{match:m,outcome:0,digest:D});await api(o,'report',{match:m,outcome:0,digest:D});}
  const flagged=(await q(`select account,rated_wins,opponents,top3_share from bt4_private.ranked_farm_review`));
  const f=flagged.find(r=>r.account===farmer),o=flagged.find(r=>r.account===organic);
  ok(f&&f.rated_wins===24&&f.opponents===3&&Number(f.top3_share)===1,'S13 the farmer (24 rated wins from 3 rivals) is flagged',JSON.stringify(f));
  ok(!o,'S13 12 wins from 12 different rivals is not flagged',JSON.stringify(o));
}
console.log(fails?fails+' FAILED':'all passed');process.exit(fails?1:0);
