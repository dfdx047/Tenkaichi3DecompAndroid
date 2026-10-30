// Real SQLite, production service, fake clock. No accounts or public requests.
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {rankedFetch} from './ranked.js';
const sql=new DatabaseSync(':memory:');
sql.exec(fs.readFileSync(new URL('./schema.sql',import.meta.url),'utf8'));
sql.exec(fs.readFileSync(new URL('./ranked-schema.sql',import.meta.url),'utf8'));
sql.exec(fs.readFileSync(new URL('./ranked-flow-schema.sql',import.meta.url),'utf8'));
const DB={prepare(text){return {values:[],bind(...v){return {...this,values:v};},async first(){return sql.prepare(text).get(...this.values)??null;},sync(){const r=sql.prepare(text).run(...this.values);return {meta:{changes:Number(r.changes)}};},async run(){return this.sync();},async all(){return {results:sql.prepare(text).all(...this.values)};}};},async batch(stmts){sql.exec('BEGIN');try{const r=stmts.map(s=>s.sync());sql.exec('COMMIT');return r;}catch(e){sql.exec('ROLLBACK');throw e;}}};
const env={DB,RANKED_ENABLED:'1',ALLOWED_VERSIONS:'5001'};let now=1800000000;
async function api(path,p={},extra={}){const r=await rankedFetch(null,env,path,{v:5001,...p,...extra},now,'198.51.100.1');return {status:r.status,...await r.json()};}
const a=await api('register',{}, {name:'SwitchA'}),b=await api('register',{}, {name:'SwitchB'});
const row=id=>sql.prepare('SELECT * FROM ranked_players WHERE id=?').get(id);
async function pair(flowA=2,flowB=2){
 await api('queue',a,{rules:'solo-inf-v1',flow:flowA});now++;
 const guest=await api('queue',b,{rules:'solo-inf-v1',flow:flowB});
 const host=await api('queue',a,{rules:'solo-inf-v1',flow:flowA});assert.equal(host.match,guest.match);return host;
}
const capabilities=await api('capabilities');assert.equal(capabilities.lifecycle,2);
let m=await pair();assert.equal(m.flow,2);
await api('lifecycle',a,{match:m.match,action:'connect'});
await api('lifecycle',b,{match:m.match,action:'connect'});
await api('lifecycle',a,{match:m.match,action:'ready'});
now+=7;
assert.equal((await api('match',b,{match:m.match})).state,'cancelled');
assert.equal(row(a.id).active_match,null);assert.equal(row(b.id).active_match,null);
assert.equal(row(a.id).wins+row(a.id).losses+row(b.id).wins+row(b.id).losses,0);
m=await pair();now+=90;
assert.equal((await api('match',a,{match:m.match})).state,'cancelled');
assert.equal(row(a.id).active_match,null);assert.equal(row(b.id).active_match,null);
m=await pair();
await api('lifecycle',a,{match:m.match,action:'connect'});await api('lifecycle',b,{match:m.match,action:'connect'});
await api('lifecycle',a,{match:m.match,action:'ready'});
assert.equal((await api('lifecycle',b,{match:m.match,action:'ready'})).state,'connected');
now+=15;assert.equal((await api('match',a,{match:m.match})).state,'paired');
const ids=new Set([m.match]);
for(let fight=0;fight<4;fight++){
 await api('lifecycle',a,{match:m.match,action:'started'});
 assert.equal((await api('cancel',b,{match:m.match})).state,'paired');
 const score=fight%2?0:2,digest='1234abcd';
 await api('result',a,{match:m.match,score,digest});
 // A pending legitimate result is never erased by cancel.
 assert.equal((await api('cancel',b,{match:m.match})).state,'paired');
 assert.equal((await api('result',b,{match:m.match,score,digest})).state,'final');
 assert.equal(row(a.id).wins+row(a.id).losses,fight+1);
 const rating=row(a.id).rating;
 await Promise.all([api('result',a,{match:m.match,score,digest}),api('result',b,{match:m.match,score,digest})]);
 assert.equal(row(a.id).rating,rating);assert.equal(row(a.id).wins+row(a.id).losses,fight+1);
 if(fight<3){
  assert.equal((await api('lifecycle',a,{match:m.match,action:'accept'})).state,'waiting');
  const [x,y]=await Promise.all([api('lifecycle',b,{match:m.match,action:'accept'}),api('lifecycle',a,{match:m.match,action:'accept'})]);
  assert.equal(x.state,'paired');assert.equal(x.match,y.match);assert(!ids.has(x.match));ids.add(x.match);
  assert.equal((await api('lifecycle',a,{match:m.match,action:'status'})).match,x.match);
  m=x;now++;
 }
}
assert.equal(row(a.id).wins,2);assert.equal(row(a.id).losses,2);assert.equal(row(b.id).wins,2);assert.equal(row(b.id).losses,2);
await api('lifecycle',a,{match:m.match,action:'accept'});
assert.equal((await api('lifecycle',b,{match:m.match,action:'leave'})).state,'declined');
assert.equal((await api('lifecycle',a,{match:m.match,action:'status'})).state,'declined');
assert.equal(row(a.id).active_match,null);assert.equal(row(b.id).active_match,null);
m=await pair();await api('result',a,{match:m.match,score:2,digest:'abcdef12'});await api('result',b,{match:m.match,score:2,digest:'abcdef12'});
await api('lifecycle',a,{match:m.match,action:'accept'});now+=15;
assert.equal((await api('lifecycle',b,{match:m.match,action:'accept'})).state,'declined');
m=await pair(2,0);assert.equal(m.flow,0);
assert.equal((await api('queue',b,{rules:'solo-inf-v1',flow:2})).flow,0); // frozen at pairing
assert.equal((await api('lifecycle',a,{match:m.match,action:'accept'})).status,409);
await api('cancel',a,{match:m.match});
console.log('PASS Ranked lifecycle: bilateral 7s connection release, no pre-fight penalty, pending result protection, 4 separate same-peer scored fights, idempotent retry/concurrent rematch IDs, reject/15s timeout and legacy capability isolation.');
