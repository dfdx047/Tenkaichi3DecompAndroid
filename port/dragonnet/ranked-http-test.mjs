import assert from 'node:assert/strict';
const base=process.env.DBT_TEST_SERVER??'http://127.0.0.1:8791';
assert(base.startsWith('http://127.0.0.1:'),'This test must only use a local Worker/database.');
async function post(path,auth={},extra={}){const r=await fetch(base+'/v1/'+path,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({v:40001,...auth,...extra})});const j=await r.json();assert(r.ok,`${path}: ${r.status} ${j.error}`);return j;}
const a=await post('ranked/register',{}, {name:'TestAlpha'}),b=await post('ranked/register',{}, {name:'TestBeta'});
const searches=await Promise.all([post('ranked/queue',a),post('ranked/queue',b),post('ranked/queue',a)]);
let av=await post('ranked/queue',a),bv=await post('ranked/queue',b);
for(let i=0;i<4 && av.match!==bv.match;i++){av=await post('ranked/queue',a);bv=await post('ranked/queue',b);}
assert.equal(av.match,bv.match);assert.equal(av.state,'paired');assert.equal(av.role+bv.role,3);
const host=av.role===1?a:b,guest=av.role===1?b:a,match=av.match;
const room=await post('host',{}, {name:'TestHost',rel:135,addrs:['127.0.0.1:7000']});
await post('ranked/room',host,{match,code:room.code});
assert.equal((await post('ranked/match',guest,{match})).code,room.code);
await post('ranked/result',host,{match,score:2,digest:'11111111'});
assert.equal((await post('ranked/cancel',guest,{match})).state,'paired');
assert.equal((await post('ranked/profile',host,{name:'TestHost'})).rating,1000);
const responses=await Promise.all([post('ranked/result',guest,{match,score:2,digest:'11111111'}),post('ranked/result',guest,{match,score:2,digest:'11111111'})]);
assert(responses.every(x=>x.state==='final'));
assert.equal((await post('ranked/profile',host,{name:'TestHost'})).rating,1016);
assert.equal((await post('ranked/profile',guest,{name:'TestGuest'})).rating,984);
await post('leave',{}, {code:room.code,key:room.key});
// The same two identities can immediately obtain a fresh match after a
// settled round. No old room/result may be reused for its next score.
await post('ranked/queue',a);av=await post('ranked/queue',b);
for(let i=0;i<5;i++){av=await post('ranked/queue',a);bv=await post('ranked/queue',b);if(av.match===bv.match && av.state==='paired')break;}
assert.equal(av.match,bv.match);assert.notEqual(av.match,match);assert.equal(av.state,'paired');
const h2=av.role===1?a:b,g2=av.role===1?b:a;
await post('ranked/result',h2,{match:av.match,score:1,digest:'22222222'});
assert.equal((await post('ranked/result',g2,{match:av.match,score:1,digest:'22222222'})).state,'final');
assert.equal((await post('ranked/profile',a,{name:'TestAlpha'})).draws,0);
assert.equal((await post('ranked/profile',b,{name:'TestBeta'})).draws,0);
const c=await post('ranked/register',{}, {name:'SoloOnly'}),d=await post('ranked/register',{}, {name:'TeamOnly'});
let sv=await post('ranked/queue',a,{rules:'solo-inf-v1'}),tv=await post('ranked/queue',b,{rules:'team3-inf-v1'});
assert.notEqual(sv.match,tv.match);
await post('ranked/queue',c,{rules:'solo-inf-v1'});await post('ranked/queue',d,{rules:'team3-inf-v1'});
for(let i=0;i<5;i++){
 sv=await post('ranked/queue',a,{rules:'solo-inf-v1'});tv=await post('ranked/queue',b,{rules:'team3-inf-v1'});
 if(sv.state==='paired' && tv.state==='paired')break;
 await post('ranked/queue',c,{rules:'solo-inf-v1'});await post('ranked/queue',d,{rules:'team3-inf-v1'});
}
assert.equal(sv.state,'paired');assert.equal(tv.state,'paired');
assert.equal(sv.rules,'solo-inf-v1');assert.equal(tv.rules,'team3-inf-v1');
assert.equal((await post('ranked/queue',c,{rules:'solo-inf-v1'})).match,sv.match);
assert.equal((await post('ranked/queue',d,{rules:'team3-inf-v1'})).match,tv.match);
console.log('PASS real local Worker+D1: concurrent queue, role assignment, existing room bridge, peer confirmation, concurrent duplicate settlement, exactly-once points');
