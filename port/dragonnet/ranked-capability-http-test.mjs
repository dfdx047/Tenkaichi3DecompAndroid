// Disposable local Worker/D1 only. SPDX-License-Identifier: MIT
import assert from 'node:assert/strict';
const base=process.env.DBT_TEST_SERVER??'http://127.0.0.1:8792';
assert(base.startsWith('http://127.0.0.1:'));
async function post(path,auth={},extra={}){
 const response=await fetch(base+'/v1/ranked/'+path,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({v:40001,...auth,...extra})});
 const body=await response.json();assert(response.ok,`${path}: ${response.status} ${body.error}`);return body;
}
const host=await post('register',{}, {name:'ResumeHost'}),guest=await post('register',{}, {name:'ResumeGuest'});
const waiting=await post('queue',host,{rules:'solo-inf-v1'});
assert.equal(waiting.state,'waiting');
assert.equal((await post('queue',host,{rules:'solo-inf-v1',rematches:1})).match,waiting.match);
// Give this host a strictly older timestamp, independent of UUID ordering.
await new Promise(resolve=>setTimeout(resolve,1100));
const paired=await post('queue',guest,{rules:'solo-inf-v1',rematches:1});
assert.equal(paired.match,waiting.match);assert.equal(paired.rematches,1);
assert.equal((await post('queue',host,{rules:'solo-inf-v1',rematches:1})).rematches,1);
for(const [round,score] of [[1,2],[2,0]]){
 const body={match:paired.match,round,score,digest:'11223344'};
 await post('result',host,body);assert.equal((await post('result',guest,body)).state,'final');
}
assert.deepEqual(await Promise.all([host,guest].map(async p=>{
 const profile=await post('profile',p,{name:p===host?'ResumeHost':'ResumeGuest'});return [profile.wins,profile.losses];
})),[[1,1],[1,1]]);
const legacy=await post('queue',host,{rules:'solo-inf-v1'});
await new Promise(resolve=>setTimeout(resolve,1100));
const legacyPeer=await post('queue',guest,{rules:'solo-inf-v1'});
assert.equal(legacyPeer.match,legacy.match);assert.equal(legacyPeer.rematches,0);
for(const who of [host,guest])assert.equal((await post('queue',who,{rules:'solo-inf-v1',rematches:1})).rematches,0);
console.log('PASS real local Worker/D1: resumed waiting search upgrades capability, both clients score a rematch, paired legacy capability remains frozen.');
