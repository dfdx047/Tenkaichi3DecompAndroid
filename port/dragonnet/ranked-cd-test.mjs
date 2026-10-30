// Actual gateway -> authority -> Worker -> SQLite. No public traffic/secrets.
// SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {AsyncLocalStorage} from 'node:async_hooks';
import authority from './ranked_authority.js';
import gateway from './ranked_gateway.js';
const sql=new DatabaseSync(':memory:');
for(const file of ['schema.sql','ranked-schema.sql','ranked-flow-schema.sql','ranked-cd-schema.sql'])sql.exec(fs.readFileSync(new URL(file,import.meta.url),'utf8'));
// Execute each batch synchronously, as D1 does atomically.
const accounting=new AsyncLocalStorage();let maxQueries=0;
function count(){const scope=accounting.getStore();if(scope)scope.queries++;}
const DB={prepare(text){return {values:[],bind(...v){return {...this,values:v};},first(){count();return sql.prepare(text).get(...this.values)??null;},run(){count();const r=sql.prepare(text).run(...this.values);return {meta:{changes:Number(r.changes)}};},all(){count();return {results:sql.prepare(text).all(...this.values)};}};},async batch(statements){sql.exec('BEGIN');try{const out=statements.map(s=>s.run());sql.exec('COMMIT');return out;}catch(e){sql.exec('ROLLBACK');throw e;}}};
const E={DB,AUTHORITY_ID:'test-authority-unique-00001',URL_C:'https://c.invalid',URL_D:'https://d.invalid',MIGRATION_COMPLETE:'1',ALLOWED_VERSIONS:'5001',DP20_ENABLED:'1',GATEWAY_C_KEY:'c'.repeat(64),GATEWAY_D_KEY:'d'.repeat(64)};
const C={GATEWAY_ID:'C',AUTHORITY_ID:E.AUTHORITY_ID,AUTHORITY_URL:'https://e.invalid',AUTHORITY_KEY:E.GATEWAY_C_KEY};
const D={...C,GATEWAY_ID:'D',AUTHORITY_KEY:E.GATEWAY_D_KEY};
let now=Math.floor(1800000000000/86400000)*86400000+86300000,cDown=false,eDown=false,probes=0,originalNow=Date.now;
Date.now=()=>now;
const originalFetch=globalThis.fetch;
globalThis.fetch=async (url,options)=>{
 const request=url instanceof Request?url:new Request(url,options),u=new URL(request.url);
 if(u.hostname==='e.invalid'){if(eDown)throw Error('simulated E outage');return authority.fetch(request,E);}
 if(u.hostname==='c.invalid'&&u.pathname==='/gateway-health'){probes++;return cDown?new Response('Cloudflare 1027',{status:530}):gateway.fetch(request,C);}
 throw Error('Unexpected network access: '+request.url);
};
async function api(g,path,auth={},extra={},ip='198.51.100.10'){
 const request=new Request('https://'+g.GATEWAY_ID.toLowerCase()+'.invalid/v1/'+path,{method:'POST',headers:{'content-type':'application/json','cf-connecting-ip':ip},body:JSON.stringify({v:5001,...auth,...extra})});
 return accounting.run({queries:0},async()=>{
  const r=await gateway.fetch(request,g),{queries}=accounting.getStore();maxQueries=Math.max(maxQueries,queries);
  assert(queries<=50,`${path}: ${queries} exceeds Workers Free 50-query budget`);return {status:r.status,...await r.json()};
 });
}
async function pair(a,b){
 const h=await api(C,'ranked/queue',a,{flow:2,rules:'dp20-inf-v1'});now+=1000;
 const g=await api(D,'ranked/queue',b,{flow:2,rules:'dp20-inf-v1'});
 assert.equal(h.match,g.match);assert.equal(g.backend,h.backend);
 const hv=await api(C,'ranked/match',a,{match:h.match});assert.equal(hv.state,'paired');assert.equal(hv.flow,2);
 return hv;
}
try{
 const denied=await authority.fetch(new Request('https://e.invalid/_ranked/dispatch',{method:'POST',body:JSON.stringify({gateway:'D',path:'ranked/profile',body:{},ip:''})}),E);
 assert.equal(denied.status,401);
 assert.equal((await api({...D,AUTHORITY_KEY:E.GATEWAY_C_KEY},'ranked/capabilities')).status,401);
 E.MIGRATION_COMPLETE='0';assert.equal((await api(C,'ranked/capabilities')).status,503);E.MIGRATION_COMPLETE='1';
 assert.equal((await api(C,'ranked/capabilities')).authority,E.AUTHORITY_ID);
 assert.equal((await api(D,'ranked/capabilities')).backend,'C');
 const a=await api(C,'ranked/register',{}, {name:'Host'}),b=await api(D,'ranked/register',{}, {name:'Guest'});
 assert(a.id&&b.id);let first=await pair(a,b);
 assert.equal(first.backend,'C');
 const room=await api(C,'host',{}, {name:'Host',addrs:['198.51.100.10:7000'],rel:135});assert(room.code);
 assert.equal((await api(C,'ranked/room',a,{match:first.match,code:room.code})).code,room.code);
 assert((await api(D,'join',{}, {name:'Guest',code:room.code,addrs:['198.51.100.10:7001'],rel:135})).peer);
 for(const p of [a,b])await api(C,'ranked/lifecycle',p,{match:first.match,action:'ready'});
 await api(C,'ranked/lifecycle',a,{match:first.match,action:'started'});
 const x=await api(C,'ranked/result',a,{match:first.match,score:2,digest:'11223344'});assert.equal(x.state,'paired');
 assert.equal((await api(D,'ranked/result',b,{match:first.match,score:2,digest:'11223344'})).state,'final');
 await Promise.all([api(D,'ranked/result',b,{match:first.match,score:2,digest:'11223344'}),api(C,'ranked/result',b,{match:first.match,score:2,digest:'11223344'})]);
 assert.equal((await api(D,'ranked/profile',a,{name:'Host'})).wins,1);
 assert.equal((await api(C,'ranked/profile',b,{name:'Guest'})).losses,1);
 await Promise.all([api(C,'ranked/lifecycle',a,{match:first.match,action:'accept'}),api(D,'ranked/lifecycle',b,{match:first.match,action:'accept'})]);
 const rematch=await api(D,'ranked/lifecycle',b,{match:first.match,action:'status'});
 assert.equal(rematch.state,'paired');assert.notEqual(rematch.match,first.match);assert.equal(rematch.backend,'C');
 assert.equal((await api(C,'ranked/lifecycle',a,{match:first.match,action:'status'})).match,rematch.match);
 const readyHost=await api(C,'ranked/register',{}, {name:'ReadyHost'}),readyGuest=await api(C,'ranked/register',{}, {name:'ReadyGuest'});
 const ready=await pair(readyHost,readyGuest);
 for(const p of [readyHost,readyGuest])await api(C,'ranked/lifecycle',p,{match:ready.match,action:'ready'});
 // C quota failure -> authoritative promotion, shared cancellation before any
 // started marker. Both players can queue again, no accidental loss.
 cDown=true;const promoted=await api(D,'ranked/capabilities');assert.equal(promoted.backend,'D');
 assert.equal((await api(D,'ranked/match',readyHost,{match:ready.match})).state,'paired');
 assert.equal((await api(D,'ranked/match',a,{match:rematch.match})).state,'cancelled');
 assert.equal(sql.prepare('SELECT active_match FROM ranked_players WHERE id=?').get(b.id).active_match,null);
 const second=await pair(a,b);assert.equal(second.backend,'D');assert(second.route_epoch>first.route_epoch);
 const guest=await api(D,'ranked/match',b,{match:second.match});assert.equal(guest.backend,second.backend);assert.equal(guest.route_epoch,second.route_epoch);
 for(const p of [a,b])await api(D,'ranked/lifecycle',p,{match:second.match,action:'ready'});
 await api(D,'ranked/lifecycle',a,{match:second.match,action:'started'});
 // E outage never settles locally or creates a second profile.
 eDown=true;assert.equal((await api(D,'ranked/result',a,{match:second.match,score:0,digest:'55667788'})).status,503);
 assert.equal(sql.prepare('SELECT wins FROM ranked_players WHERE id=?').get(a.id).wins,1);eDown=false;
 cDown=false;now=(Math.floor(now/86400000)+1)*86400000;
 assert.equal((await api(C,'ranked/capabilities')).backend,'C');
 // A live assigned D fight stays at D across UTC reset, even through C.
 assert.equal((await api(C,'ranked/match',a,{match:second.match})).backend,'D');
 await api(C,'ranked/result',a,{match:second.match,score:0,digest:'55667788'});
 await api(D,'ranked/result',b,{match:second.match,score:0,digest:'55667788'});
 await api(C,'ranked/result',a,{match:second.match,score:0,digest:'55667788'});
 assert.equal((await api(D,'ranked/profile',a,{name:'Host'})).losses,1);
 assert.equal((await api(C,'ranked/profile',b,{name:'Guest'})).wins,1);
 assert.equal(sql.prepare('SELECT COUNT(*) AS n FROM ranked_players').get().n,4);
 // No fallback to another local D1 or direct public SQL, no chat/quick.
 assert.equal((await api(D,'chat/sync')).status,404);assert.equal((await api(D,'quick')).status,404);
 assert.equal((await api(D,'ranked/capabilities',{}, {v:5})).status,409);
 const bad=await authority.fetch(new Request('https://e.invalid/sql',{method:'POST',body:'SELECT *'}),E);assert.equal(bad.status,404);
 console.log('PASS C/D: real gateway/authority/SQLite, separate keys, migration gate, shared profiles, DP20, rooms, flow:2 fresh rematch, quota promotion, both-peer cancellation, pinned routes, UTC reset, durable retry exactly-once points, E outage, no public SQL/chat/quick.');
 console.log('Maximum D1 queries/invocation in exercised flows:',maxQueries,'(Free limit: 50).');
}finally{globalThis.fetch=originalFetch;Date.now=originalNow;sql.close();}
