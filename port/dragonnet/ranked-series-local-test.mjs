// Local SQLite/mock-opponent coverage for ranked series. No network or ranked service.
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {rankedFetch} from './ranked.js';

const sql=new DatabaseSync(':memory:');
for(const file of ['schema.sql','ranked-schema.sql','ranked-flow-schema.sql'])sql.exec(fs.readFileSync(new URL(file,import.meta.url),'utf8'));
const DB={};
DB.prepare=text=>({values:[],bind(...v){return {...this,values:v,sql:text};},async first(){return sql.prepare(text).get(...this.values)??null;},async run(){return {meta:{changes:Number(sql.prepare(text).run(...this.values).changes)}};},async all(){return {results:sql.prepare(text).all(...this.values)};},sync(){return sql.prepare(text).run(...this.values);}});
DB.batch=async stmts=>{sql.exec('BEGIN');try{const out=stmts.map(s=>s.sync());sql.exec('COMMIT');return out;}catch(e){sql.exec('ROLLBACK');throw e;}};
let now=1800000000;const env={DB,RANKED_ENABLED:'1',ALLOWED_VERSIONS:'5001'};
async function api(path,p={},extra={}){const r=await rankedFetch(null,env,path,{v:5001,...p,...extra},now,'127.0.0.1');return {status:r.status,...await r.json()};}
async function series(label,scores,expected){
 const a=await api('register',{}, {name:label+'A'}),b=await api('register',{}, {name:label+'B'});
 async function pair(){await api('queue',a,{rules:'solo-inf-v1',flow:2});now++;const g=await api('queue',b,{rules:'solo-inf-v1',flow:2});return api('queue',a,{rules:'solo-inf-v1',flow:2}).then(h=>{assert.equal(h.match,g.match);return h;});}
 let match=await pair();const ids=[];
 for(let i=0;i<scores.length;i++){
  const score=scores[i],digest=String(i+1).padStart(8,'0');ids.push(match.match);
  await api('result',a,{match:match.match,score,digest});assert.equal((await api('result',b,{match:match.match,score,digest})).state,'final');
  if(i<scores.length-1){
   await api('lifecycle',a,{match:match.match,action:'accept'});
   const next=await api('lifecycle',b,{match:match.match,action:'accept'});
   assert.equal(next.state,'paired');assert.notEqual(next.match,match.match);match=next;now++;
  }
 }
 const status=await api('lifecycle',a,{match:match.match,action:'status'});
 if(expected.offer){assert.notEqual(status.state,'series_decided');}
 else {assert.equal(status.state,'series_decided');assert.equal(status.host_wins,expected.host);assert.equal(status.guest_wins,expected.guest);}
 assert.equal(new Set(ids).size,ids.length);
 const results=sql.prepare('SELECT count(*) n FROM ranked_series_results WHERE series_id=?').get(ids[0]);
 assert.equal(results.n,scores.length);
 console.log(`PASS ${label}: ${expected.host}-${expected.guest}, ${expected.offer?'third match offered':'series closed'}, unique match IDs and settled results`);
}
await series('HostSweep',[2,2],{host:2,guest:0,offer:false});
await series('GuestSweep',[0,0],{host:0,guest:2,offer:false});
await series('Decider',[2,0],{host:1,guest:1,offer:true});
await series('ThreeGames',[2,0,2],{host:2,guest:1,offer:false});
