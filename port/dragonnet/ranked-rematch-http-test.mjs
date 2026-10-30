// Run only against a disposable local Worker/D1. SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
const base=process.env.DBT_TEST_SERVER??'http://127.0.0.1:8792';
assert(base.startsWith('http://127.0.0.1:'));
async function post(path,auth={},extra={}){const r=await fetch(base+'/v1/ranked/'+path,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({v:40001,...auth,...extra})});const j=await r.json();assert(r.ok,`${path}: ${r.status} ${j.error}`);return j;}
const a=await post('register',{}, {name:'RematchAlpha'}),b=await post('register',{}, {name:'RematchBeta'});
let av=await post('queue',a,{rules:'solo-inf-v1',rematches:1}),bv=await post('queue',b,{rules:'solo-inf-v1',rematches:1});
for(let i=0;i<5;i++){av=await post('queue',a,{rules:'solo-inf-v1',rematches:1});bv=await post('queue',b,{rules:'solo-inf-v1',rematches:1});if(av.match===bv.match && av.state==='paired')break;}
assert.equal(av.match,bv.match);assert.equal(av.rematches,1);assert.equal(bv.rematches,1);
const host=av.role===1?a:b,guest=av.role===1?b:a,match=av.match;
for(const [round,score] of [[1,2],[2,2],[3,0],[4,1]]){
 const body={match,round,score,digest:round.toString(16).padStart(8,'0')};
 await post('result',host,body);
 const replies=await Promise.all([post('result',guest,body),post('result',guest,body)]);assert(replies.every(r=>r.state==='final'));
}
let hp=await post('profile',host,{name:'RematchHost'}),gp=await post('profile',guest,{name:'RematchGuest'});
assert.deepEqual([hp.wins,hp.losses,hp.draws],[2,1,0]);assert.deepEqual([gp.wins,gp.losses,gp.draws],[1,2,0]);
const quits=await Promise.all([post('forfeit',guest,{match,round:5}),post('forfeit',guest,{match,round:5})]);assert(quits.every(r=>r.state==='final'));
hp=await post('profile',host,{name:'RematchHost'});gp=await post('profile',guest,{name:'RematchGuest'});
assert.deepEqual([hp.wins,hp.losses],[3,1]);assert.deepEqual([gp.wins,gp.losses],[1,3]);assert.equal(hp.rating+gp.rating,2000);
await post('forfeit',host,{match,round:5});assert.equal((await post('profile',host,{name:'RematchHost'})).losses,1);
const next=await post('queue',host,{rules:'solo-inf-v1',rematches:1});assert.notEqual(next.match,match);
await post('forfeit',guest,{match,round:5});assert.equal((await post('queue',host,{rules:'solo-inf-v1',rematches:1})).match,next.match);
let paired=await post('queue',guest,{rules:'solo-inf-v1',rematches:1}),other;
for(let i=0;i<5;i++){other=await post('queue',host,{rules:'solo-inf-v1',rematches:1});paired=await post('queue',guest,{rules:'solo-inf-v1',rematches:1});if(other.match===paired.match && paired.state==='paired')break;}
assert.equal(other.match,paired.match);assert.equal(paired.state,'paired');
const simultaneous=await Promise.all([post('forfeit',host,{match:paired.match,round:1}),post('forfeit',guest,{match:paired.match,round:1})]);assert(simultaneous.every(r=>r.state==='final'));
hp=await post('profile',host,{name:'RematchHost'});gp=await post('profile',guest,{name:'RematchGuest'});
assert.equal(hp.wins+gp.wins,5);assert.equal(hp.losses+gp.losses,5);assert.equal(hp.rating+gp.rating,2000);
console.log('PASS real local Worker/D1: rematch capabilities, repeated wins/loss, no draw totals, concurrent result and surrender duplicates, finished-menu exit harmless, next search preserved.');
