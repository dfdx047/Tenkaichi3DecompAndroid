// Real SQLite transactions; no production players or scores. SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {rankedFetch,eloDelta} from './ranked.js';
const sql=new DatabaseSync(':memory:');
for(const file of ['schema.sql','ranked-schema.sql'])sql.exec(fs.readFileSync(new URL(file,import.meta.url),'utf8'));
sql.exec(fs.readFileSync(new URL('ranked-flow-schema.sql',import.meta.url),'utf8'));
const DB={prepare(text){return {values:[],bind(...v){return {...this,values:v};},async first(){return sql.prepare(text).get(...this.values)??null;},async run(){return {meta:{changes:Number(sql.prepare(text).run(...this.values).changes)}};},async all(){return {results:sql.prepare(text).all(...this.values)};}};},async batch(statements){sql.exec('BEGIN');try{const out=[];for(const st of statements)out.push(await st.run());sql.exec('COMMIT');return out;}catch(e){sql.exec('ROLLBACK');throw e;}}};
let now=1800000000;
async function api(path,auth={},data={}){const response=await rankedFetch(null,{DB,RANKED_ENABLED:'1'},path,{v:40001,...auth,...data},now,'198.51.100.1');return {status:response.status,...await response.json()};}
async function players(prefix){return [await api('register',{}, {name:prefix+'A'}),await api('register',{}, {name:prefix+'B'})];}
async function pair(a,b,cap=1){const first=await api('queue',a,{rules:'team3-inf-v1',rematches:1});now++;const second=await api('queue',b,{rules:'team3-inf-v1',rematches:cap});assert.equal(first.match,second.match);const host=await api('queue',a,{rules:'team3-inf-v1',rematches:1});assert.equal(host.rematches,cap);return first.match;}
const [a,b]=await players('Round'),m=await pair(a,b);
const result=(p,round,score,digest='12345678')=>api('result',p,{match:m,round,score,digest});
assert.equal((await result(a,2,2)).state,'cancelled'); // cannot award before first fight
await result(a,1,2);assert.equal((await result(b,1,2)).state,'final');
await result(a,2,2);assert.equal((await result(a,3,0)).state,'pending');
assert.equal((await result(a,2,0)).status,409); // immutable own report
const duplicate=await Promise.all([result(b,2,2),result(b,2,2),result(a,2,2)]);
assert(duplicate.every(r=>r.state==='final'));
let hp=await api('profile',a,{name:'RoundA'}),gp=await api('profile',b,{name:'RoundB'});
assert.equal(hp.rating,1016+eloDelta(1016,984,2));assert.equal(hp.wins,2);assert.equal(gp.losses,2);assert.equal(hp.rating+gp.rating,2000);
for(const score of [0,1,2]){now+=60;const round=score===0?3:score===1?4:5;const rating=(await api('profile',a,{name:'RoundA'})).rating;await result(a,round,score);assert.equal((await result(b,round,score)).state,'final');if(score===1)assert.equal((await api('profile',a,{name:'RoundA'})).rating,rating);}
hp=await api('profile',a,{name:'RoundA'});gp=await api('profile',b,{name:'RoundB'});
assert.deepEqual([hp.wins,hp.losses,hp.draws],[3,1,0]);assert.deepEqual([gp.wins,gp.losses,gp.draws],[1,3,0]);
const before=hp.rating;await result(a,2,2);assert.equal((await api('profile',a,{name:'RoundA'})).rating,before);
const outsider=await api('register',{}, {name:'Outsider'});
assert.equal((await api('result',outsider,{match:m,round:6,score:2,digest:'12345678'})).status,404);
assert.equal((await result(a,257,2)).status,400);assert.equal((await result(a,2.5,2)).status,400);
await result(a,6,2);assert.equal((await result(b,6,0)).state,'disputed');
assert.equal((await result(a,7,2)).state,'cancelled');assert.equal((await api('profile',a,{name:'RoundA'})).rating,before);
now+=60;
const [x,y]=await players('Mixed'),mixed=await pair(x,y,0);
await api('result',x,{match:mixed,score:2,digest:'11111111'});await api('result',y,{match:mixed,score:2,digest:'11111111'});
assert.equal((await api('result',x,{match:mixed,round:2,score:2,digest:'11111111'})).state,'cancelled');
assert.equal((await api('profile',x,{name:'MixedA'})).wins,1);
now+=60;
const [q,z]=await players('Forfeit'),concede=await pair(q,z);
const concedeResult=(who,round)=>api('forfeit',who,{match:concede,round});
assert.equal((await concedeResult(outsider,1)).status,404);
const quits=await Promise.all([concedeResult(q,1),concedeResult(q,1)]);assert(quits.every(r=>r.state==='final'));
assert.equal((await api('profile',q,{name:'ForfeitA'})).losses,1);
assert.equal((await api('profile',z,{name:'ForfeitB'})).wins,1);
assert.equal((await concedeResult(z,1)).state,'final'); // leaving a finished fight adds nothing
assert.equal((await api('profile',z,{name:'ForfeitB'})).losses,0);
assert.equal((await concedeResult(z,2)).state,'final');
assert.equal((await api('profile',z,{name:'ForfeitB'})).losses,1);
assert.equal((await api('profile',q,{name:'ForfeitA'})).wins,1);
// A delayed engine result after the concession must not award it again.
await api('result',q,{match:concede,round:2,score:2,digest:'12345678'});
assert.equal((await api('profile',q,{name:'ForfeitA'})).wins,1);
now+=60;
const [u,v]=await players('NewSearch'),other=await pair(u,v);
await api('result',u,{match:other,score:2,digest:'11111111'});await api('result',v,{match:other,score:2,digest:'11111111'});
await api('queue',u,{rematches:1});assert.equal((await api('result',v,{match:other,round:2,score:2,digest:'11111111'})).state,'cancelled');
// Migration from exact pre-rematch schema preserves credentials, ratings,
// history and legacy capability defaults without replaying old results.
const old=new DatabaseSync(':memory:');let schema=fs.readFileSync(new URL('ranked-schema.sql',import.meta.url),'utf8');
schema=schema.replace(', rematch_caps INTEGER NOT NULL DEFAULT 0','').split('CREATE TABLE IF NOT EXISTS ranked_rounds')[0];old.exec(schema);
old.exec("INSERT INTO ranked_players(id,token_hash,name,created,rating,wins) VALUES('old','secret','Old',1,1277,19);INSERT INTO ranked_matches(id,host_id,made,state,delta) VALUES('game','old',1,'final',16)");
old.exec(fs.readFileSync(new URL('ranked-migrate-rematches.sql',import.meta.url),'utf8'));
assert.deepEqual({...old.prepare("SELECT rating,wins,token_hash FROM ranked_players WHERE id='old'").get()},{rating:1277,wins:19,token_hash:'secret'});
assert.deepEqual({...old.prepare("SELECT state,delta,rematch_caps FROM ranked_matches WHERE id='game'").get()},{state:'final',delta:16,rematch_caps:0});
console.log('PASS rematches: consecutive wins/losses/draw, current-rating Elo, duplicate concurrency, immutable reports, sequential rounds, dispute, outsider, old client fallback, new-search guard, additive migration.');
