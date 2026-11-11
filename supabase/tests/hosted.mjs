import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {SupabaseSignal} from '../client.mjs';

const config=JSON.parse(await readFile(process.env.BT4_SUPABASE_CONFIG,'utf8'));
const cachePath=process.env.BT4_SUPABASE_TEST_SESSIONS;
let cache={};try{cache=JSON.parse(await readFile(cachePath,'utf8'));}catch{}
const clients=[];const checks=[];
async function actor(label){
  const c=new SupabaseSignal(config.url,config.publishable_key,cache[label]);
  await c.authenticate();cache[label]=c.session;
  await writeFile(cachePath,JSON.stringify(cache),{mode:0o600});clients.push(c);return c;
}
const profile={mode:'tag',wire:'bt4-p2p-1',content:'5454000754540008',rules:'2v2-114',name:'Host',capacity:4};
const checked=(name)=>{checks.push(name);console.log('PASS '+name);};
try{
  const host=await actor('host'), guest=await actor('guest'), outsider=await actor('outsider');
  for(const c of [host,guest,outsider])await c.rpc('leave');
  await host.connect();await guest.connect();
  const room=await host.rpc('create',profile);assert.ok(room.code,JSON.stringify(room));
  assert.equal(room.self,1);assert.equal(room.host,1);assert.equal(room.members.length,1);
  checked('create-private-tag-room');
  const hidden=await outsider.rpc('list',profile);assert.deepEqual(hidden,[]);checked('private-room-not-listed');
  const wrong=await guest.rpc('join',{code:room.code,wire:profile.wire,content:'ffffffffffffffff',name:'Guest'});
  assert.equal(wrong.error,'INCOMPATIBLE_DATA');checked('incompatible-content-rejected');
  const member=await guest.rpc('join',{code:room.code,wire:profile.wire,content:profile.content,name:'Guest'});
  assert.equal(member.room,room.room);assert.equal(member.self,2);assert.equal(member.members.length,2);
  await host.wait('bt4.room',x=>x.members?.length===2);checked('same-room-membership-and-realtime-notification');
  const signal={generation:room.generation,to:2,channel:'tag',kind:'candidate',body:{candidate:'candidate:1 1 UDP 1 127.0.0.1 26010 typ host',mid:'0'}};
  const sent=await host.rpc('signal',signal);assert.equal(sent.state,'sent',JSON.stringify(sent));
  const received=await guest.wait('bt4.signal',x=>x.kind==='candidate');assert.equal(received.from,1);assert.equal(received.to,2);
  assert.equal(received.generation,room.generation);assert.deepEqual(received.body,signal.body);checked('directed-candidate-delivery');
  const voice={...signal,channel:'voice',body:{...signal.body,revision:1}};
  assert.equal((await host.rpc('signal',voice)).state,'sent');
  const media=await guest.wait('bt4.signal',x=>x.channel==='voice');assert.equal(media.body.revision,1);checked('versioned-voice-negotiation');
  assert.ok((await host.rpc('signal',{...voice,body:signal.body})).error);checked('unversioned-voice-negotiation-rejected');
  assert.equal((await host.rpc('signal',{...voice,kind:'restart',body:{}})).state,'sent');
  assert.ok((await host.rpc('signal',{...voice,channel:'tag',kind:'restart',body:{}})).error);checked('restart-limited-to-voice');
  const forbidden=await outsider.rpc('signal',signal);assert.ok(forbidden.error);checked('non-member-cannot-signal');
  for(const kind of ['input','frame','audio','snapshot']){
    const rejected=await host.rpc('signal',{...signal,kind,body:{data:'forbidden'}});
    assert.equal(rejected.error,'SIGNAL_KIND_NOT_ALLOWED');
  }checked('gameplay-and-voice-relay-rejected');
  const forged=await host.rpc('signal',{...signal,body:{...signal.body,from:999}});assert.ok(forged.error);checked('extra-signal-fields-rejected');
  await assert.rejects(outsider.connect(host.session.user.id),/PRIVATE_CHANNEL_DENIED/);checked('another-players-inbox-denied');
  await host.rpc('lease',{joinable:false});
  const busy=await outsider.rpc('join',{code:room.code,wire:profile.wire,content:profile.content,name:'Other'});
  assert.equal(busy.error,'ROOM_BUSY');checked('closed-admission-rejected');
  await guest.rpc('leave');await host.rpc('leave');
  const gone=await guest.rpc('get');assert.equal(gone.state,'none');checked('leave-and-room-cleanup');
  const duel=await host.rpc('create',{...profile,mode:'duel',capacity:8});
  const observer=await guest.rpc('join',{code:duel.code,wire:profile.wire,content:profile.content,name:'Observer',role:'spectator'});
  assert.equal(observer.members.find(x=>x.peer===observer.self).role,'spectator');
  const fighter=await outsider.rpc('join',{code:duel.code,wire:profile.wire,content:profile.content,name:'Fighter'});
  assert.equal(fighter.members.filter(x=>x.role==='player').length,2);checked('spectator-does-not-occupy-second-fighter');
  await host.rpc('leave',{generation:room.generation});
  assert.equal((await host.rpc('get')).room,duel.room);checked('stale-teardown-cannot-close-a-new-room');
  await host.rpc('leave');await guest.rpc('leave');await outsider.rpc('leave');
  await host.rpc('queue',{...profile,mode:'duel',name:'Host'});
  const match=await guest.rpc('queue',{...profile,mode:'duel',name:'Guest'});
  assert.equal(match.mode,'duel');assert.equal(match.members.length,2);assert.equal(match.self,2);
  const paired=await host.wait('bt4.room',x=>x.room===match.room);assert.equal(paired.self,1);
  checked('quick-match-pairs-compatible-clients');
  await host.rpc('leave');await guest.rpc('leave');
  // Invalid guesses must consume their budget even though the operation fails.
  for(let i=0;i<31;i++)await outsider.rpc('join',{code:'AAAAAA',wire:profile.wire,content:profile.content,name:'Other'});
  const limited=await outsider.rpc('join',{code:'AAAAAA',wire:profile.wire,content:profile.content,name:'Other'});
  assert.equal(limited.error,'RATE_LIMITED');checked('invalid-code-guesses-are-rate-limited');
  console.log(JSON.stringify({checks,pass:checks.length,hosted:true,gameplayTested:false},null,2));
  if(process.env.BT4_SUPABASE_REPORT)await writeFile(process.env.BT4_SUPABASE_REPORT,JSON.stringify({checks,pass:checks.length,hosted:true,gameplayTested:false},null,2));
}finally{
  for(const c of clients){try{await c.rpc('leave');}catch{}c.disconnect();}
}
