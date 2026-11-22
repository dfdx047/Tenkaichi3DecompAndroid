// Dragon Net: two netcode versions on one service (DBT-NX 40001 and Dragon Net 5001) never meet in Ranked; chat and
// presence take both; a version not in ALLOWED_VERSIONS is refused. node dragonnet-test.mjs
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {rankedFetch} from './ranked.js';
import {chatFetch} from './chat.js';
import {presenceFetch} from './presence.js';
import {newDatabase} from './new-database.mjs';
const sql=newDatabase(DatabaseSync); // (the schemas README.md lists for a new database)
const DB={prepare(text){return {values:[],bind(...v){return {...this,values:v};},async first(){return sql.prepare(text).get(...this.values)??null;},async run(){const r=sql.prepare(text).run(...this.values);return {meta:{changes:Number(r.changes)}};},async all(){return {results:sql.prepare(text).all(...this.values)};}};},async batch(statements){sql.exec('BEGIN');try{const out=[];for(const st of statements)out.push(await st.run());sql.exec('COMMIT');return out;}catch(e){sql.exec('ROLLBACK');throw e;}}};
let now=1800000000;
const env={DB,RANKED_ENABLED:'1',CHAT_ENABLED:'1',ALLOWED_VERSIONS:'40001,5001'};
async function api(path,v,auth={},data={},ip='198.51.100.1'){
 const r=await rankedFetch(null,env,path,{v,...auth,...data},now,ip);return {status:r.status,...await r.json()};
}
// a version that is not allowed
assert.equal((await api('register',4,{}, {name:'Old'})).status,409);
const sw=await api('register',40001,{}, {name:'Switch'}),an=await api('register',5001,{}, {name:'Android'}),an2=await api('register',5001,{}, {name:'Android2'});
// Switch (40001) searches first, then an Android (5001): they must not be paired
let s=await api('queue',40001,sw,{rules:'solo-inf-v1'});now++;
let a=await api('queue',5001,an,{rules:'solo-inf-v1'});
assert.notEqual(a.match,s.match);assert.equal(a.state,'waiting');
// a second Android pairs with the first one
now++;let b=await api('queue',5001,an2,{rules:'solo-inf-v1'});
assert.equal(b.match,a.match);assert.equal(b.role,2);
a=await api('queue',5001,an,{rules:'solo-inf-v1'});assert.equal(a.state,'paired');
// the Switch search is still waiting, alone
s=await api('queue',40001,sw,{rules:'solo-inf-v1'});assert.equal(s.state,'waiting');
// a search of one version cannot be continued with another
assert.equal((await api('queue',5001,sw,{rules:'solo-inf-v1'})).status,409);
// the host publishes a room of its own version only
sql.prepare('INSERT INTO rooms(code,made,version,ip,host_key,host_name,host_addrs) VALUES(?,?,?,?,?,?,?)').run('SWTCH2',now,40001,'198.51.100.1','k','Android','[]');
assert.equal((await api('room',5001,an,{match:a.match,code:'SWTCH2'})).status,409);
sql.prepare('INSERT INTO rooms(code,made,version,ip,host_key,host_name,host_addrs) VALUES(?,?,?,?,?,?,?)').run('ANDRD2',now,5001,'198.51.100.1','k','Android','[]');
assert.equal((await api('room',5001,an,{match:a.match,code:'ANDRD2'})).code,'ANDRD2');
assert.equal((await api('match',5001,an2,{match:a.match})).code,'ANDRD2');
// both report: the host won
assert.equal((await api('result',5001,an,{match:a.match,score:2,digest:'0badcafe'})).state,'paired');
assert.equal((await api('result',5001,an2,{match:a.match,score:2,digest:'0badcafe'})).state,'final');
assert.equal((await api('profile',5001,an,{name:'Android'})).rating,1016);
// chat and presence take both versions
for(const [who,v] of [[sw,40001],[an,5001]]){
 const r=await chatFetch(env,{v,id:who.id,token:who.token,after:0},now);assert.equal(r.status,200);
 const p=await presenceFetch(env,{v,id:who.id.slice(0,32)},now,'203.0.113.'+v%200);assert.equal(p.status,200);
}
assert.equal((await chatFetch(env,{v:4,id:sw.id,token:sw.token,after:0},now)).status,409);
console.log('PASS dragonnet: versions kept apart in Ranked and rooms, shared chat, presence and profiles');
