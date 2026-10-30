import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {rankedFetch,eloDelta} from './ranked.js';
const sql=new DatabaseSync(':memory:');
sql.exec(fs.readFileSync(new URL('./schema.sql',import.meta.url),'utf8'));
sql.exec(fs.readFileSync(new URL('./ranked-schema.sql',import.meta.url),'utf8'));
sql.exec(fs.readFileSync(new URL('./ranked-flow-schema.sql',import.meta.url),'utf8'));
const DB={prepare(text){return {values:[],bind(...v){return {...this,values:v};},async first(){return sql.prepare(text).get(...this.values)??null;},async run(){const r=sql.prepare(text).run(...this.values);return {meta:{changes:Number(r.changes)}};},async all(){return {results:sql.prepare(text).all(...this.values)};}};},async batch(statements){sql.exec('BEGIN');try{const out=[];for(const st of statements)out.push(await st.run());sql.exec('COMMIT');return out;}catch(e){sql.exec('ROLLBACK');throw e;}}};
let now=1800000000;
const env={DB,RANKED_ENABLED:'1'};
async function api(path,auth={},data={},ip='198.51.100.1'){
 const response=await rankedFetch(null,env,path,{v:40001,...auth,...data},now,ip);
 return {status:response.status,...await response.json()};
}
const a=await api('register',{}, {name:'Alpha'}),b=await api('register',{}, {name:'Beta'}),c=await api('register',{}, {name:'Gamma'});
for(const p of [a,b,c])assert(p.id && p.token);
assert.equal((await api('profile',{id:a.id,token:b.token})).status,401);
assert.equal((await api('profile',a,{name:'Alpha'})).rating,1000);
async function pair(x,y){let m=await api('queue',x);now++;let n=await api('queue',y);assert.equal(n.match,m.match);assert.equal(n.role,2);m=await api('queue',x);assert.equal(m.state,'paired');return m.match;}
const m=await pair(a,b);
assert.equal((await api('match',c,{match:m})).status,404);
assert.equal((await api('room',b,{match:m,code:'ABC234'})).status,400);
sql.prepare('INSERT INTO rooms(code,made,version,ip,host_key,host_name,host_addrs) VALUES(?,?,?,?,?,?,?)').run('ABC234',now,40001,'198.51.100.1','secret','Alpha','[]');
assert.equal((await api('room',a,{match:m,code:'ABC234'},'203.0.113.2')).status,409);
assert.equal((await api('room',a,{match:m,code:'ABC234'})).code,'ABC234');
assert.equal((await api('match',b,{match:m})).code,'ABC234');
let r=await api('result',a,{match:m,score:2,digest:'12345678'});assert.equal(r.state,'paired');
assert.equal((await api('cancel',b,{match:m})).state,'paired');
assert.equal(sql.prepare('SELECT active_match FROM ranked_players WHERE id=?').get(b.id).active_match,m);
assert.equal((await api('profile',a,{name:'Alpha'})).rating,1000);
assert.equal((await api('result',a,{match:m,score:0,digest:'12345678'})).status,409);
r=await api('result',b,{match:m,score:2,digest:'12345678'});assert.equal(r.state,'final');assert.equal(r.delta,-16);
assert.equal((await api('profile',a,{name:'Alpha'})).rating,1016);
assert.equal((await api('profile',b,{name:'Beta'})).rating,984);
for(let i=0;i<3;i++)await api('result',b,{match:m,score:2,digest:'12345678'});
assert.equal((await api('profile',b,{name:'Beta'})).rating,984);
const disputed=await pair(a,b);await api('result',a,{match:disputed,score:2,digest:'00000001'});
r=await api('result',b,{match:disputed,score:0,digest:'00000002'});assert.equal(r.state,'disputed');
assert.equal((await api('profile',a,{name:'Alpha'})).rating,1016);
const cancelled=await pair(a,c);await api('cancel',c,{match:cancelled});
assert.equal((await api('result',a,{match:cancelled,score:2,digest:'00000001'})).status,409);
const stale=await api('queue',a);now+=121;const fresh=await api('queue',a);assert.notEqual(fresh.match,stale.match);
assert.equal(sql.prepare('SELECT state FROM ranked_matches WHERE id=?').get(stale.match).state,'expired');
assert.equal(eloDelta(1000,1000,2),16);assert.equal(eloDelta(1000,1000,0),-16);assert.equal(eloDelta(1000,1000,1),0);
const board=await api('leaderboard',a);assert.equal(board.rows[0].name,'Alpha');assert(!JSON.stringify(board).includes(a.token));
// Multiple simultaneous polls for one identity retain one active room.
await api('cancel',a,{match:fresh.match});
const attempts=await Promise.all([api('queue',a),api('queue',a),api('queue',a)]);
assert(attempts.every(x=>x.match===attempts[0].match));
// New rules never mix solo, team-three, or legacy timed builds. Ratings remain
// attached to the existing player; changing a queue cannot hijack a live match.
now+=60;
const d=await api('register',{}, {name:'Solo1'}),e=await api('register',{}, {name:'Team1'}),f=await api('register',{}, {name:'Team2'}),g=await api('register',{}, {name:'Solo2'});
const solo=await api('queue',d,{rules:'solo-inf-v1'});now++;
const team=await api('queue',e,{rules:'team3-inf-v1'});assert.notEqual(solo.match,team.match);
assert.equal((await api('queue',d,{rules:'team3-inf-v1'})).status,409);
assert.equal((await api('queue',d,{rules:'invalid'})).status,400);
now++;const secondTeam=await api('queue',f,{rules:'team3-inf-v1'});assert.equal(secondTeam.match,team.match);assert.equal(secondTeam.rules,'team3-inf-v1');
now++;const secondSolo=await api('queue',g,{rules:'solo-inf-v1'});assert.equal(secondSolo.match,solo.match);assert.equal(secondSolo.rules,'solo-inf-v1');
// Unlimited in-game time must not lose reports at the previous 30-minute TTL.
now+=3600;
assert.equal((await api('result',e,{match:team.match,score:2,digest:'aabbccdd'})).state,'paired');
assert.equal((await api('result',f,{match:team.match,score:2,digest:'aabbccdd'})).state,'final');
assert.equal((await api('profile',e,{name:'Team1'})).wins,1);
assert.equal((await api('profile',f,{name:'Team2'})).losses,1);
const oldDb=new DatabaseSync(':memory:');
const oldSchema=fs.readFileSync(new URL('./ranked-schema.sql',import.meta.url),'utf8').replace(/,\s*rule_set TEXT NOT NULL DEFAULT 'legacy'/,'').replace(/^CREATE INDEX IF NOT EXISTS ranked_rules_waiting.*$/m,'').replace(/^CREATE INDEX IF NOT EXISTS ranked_version_waiting.*$/m,'');
oldDb.exec(oldSchema);
oldDb.exec("INSERT INTO ranked_players(id,token_hash,name,created,rating,wins) VALUES('old','hash','Existing',1,1379,12); INSERT INTO ranked_matches(id,host_id,made,state) VALUES('oldmatch','old',1,'paired');");
oldDb.exec(fs.readFileSync(new URL('./ranked-migrate-rules.sql',import.meta.url),'utf8'));
assert.deepEqual({...oldDb.prepare("SELECT rating,wins FROM ranked_players WHERE id='old'").get()},{rating:1379,wins:12});
assert.equal(oldDb.prepare("SELECT rule_set FROM ranked_matches WHERE id='oldmatch'").get().rule_set,'legacy');
console.log('PASS ranked: credentials, participant authorization, live room ownership, FIFO pairing, dual reports, exactly-once Elo, disputes, cancel, expiry, concurrent queue, safe leaderboard');
