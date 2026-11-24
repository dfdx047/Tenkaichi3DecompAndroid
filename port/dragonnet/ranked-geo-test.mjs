// Ranked pairs near players first (port/netgeo/match.js), and as before where no place is known.
// Run: node port/dragonnet/ranked-geo-test.mjs   (Node 22: node:sqlite; nothing is sent anywhere)
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import {rankedFetch} from './ranked.js';
function db(withLoc){
 const sql=new DatabaseSync(':memory:');
 for(const f of ['schema.sql','ranked-schema.sql','ranked-flow-schema.sql']){
  let text=fs.readFileSync(new URL('./'+f,import.meta.url),'utf8');
  if(!withLoc)text=text.replace(/,\s*loc TEXT\s*\)/,'\n)'); // (a database made before ranked-migrate-loc.sql)
  sql.exec(text);
 }
 assert.equal(sql.prepare("SELECT COUNT(*) AS n FROM pragma_table_info('ranked_matches') WHERE name='loc'").get().n,withLoc?1:0);
 return {sql,DB:{prepare(text){return {values:[],bind(...v){return {...this,values:v};},async first(){return sql.prepare(text).get(...this.values)??null;},async run(){const r=sql.prepare(text).run(...this.values);return {meta:{changes:Number(r.changes)}};},async all(){return {results:sql.prepare(text).all(...this.values)};}};},async batch(st){sql.exec('BEGIN');try{const o=[];for(const s of st)o.push(await s.run());sql.exec('COMMIT');return o;}catch(e){sql.exec('ROLLBACK');throw e;}}}};
}
const CF={
 SaoPaulo:{latitude:'-23.55',longitude:'-46.63',continent:'SA',country:'BR'},
 Rio:{latitude:'-22.91',longitude:'-43.17',continent:'SA',country:'BR'},
 Tokyo:{latitude:'35.68',longitude:'139.69',continent:'AS',country:'JP'},
 Lisbon:{latitude:'38.72',longitude:'-9.14',continent:'EU',country:'PT'},
};
async function run(withLoc){
 const {sql,DB}=db(withLoc);let now=1800000000;const env={DB,RANKED_ENABLED:'1'};
 const api=async(path,auth,data={},cf)=>{const r=await rankedFetch(cf?{cf}:null,env,path,{v:40001,...auth,...data},now,'198.51.100.'+(1+Math.floor(Math.random()*200)));return {status:r.status,...await r.json()};};
 const reg=async n=>api('register',{},{name:n});
 const jp=await reg('Japan'),br=await reg('Brazil'),br2=await reg('Rio'),pt=await reg('Portugal');
 const q=(p,cf)=>api('queue',{id:p.id,token:p.token},{},cf);
 const jm=await q(jp,CF.Tokyo);now+=2;
 const bm=await q(br,CF.SaoPaulo);now+=1;
 return {sql,now:()=>now,step:s=>{now+=s;},q,jp,br,br2,pt,jm,bm};
}
{
 const t=await run(true);
 assert.equal(t.bm.state,'waiting','Brazil is not put with Japan at once');
 assert.equal(t.sql.prepare('SELECT loc FROM ranked_matches WHERE id=?').get(t.bm.match).loc,'-24,-47,SA,,BR','the place kept: whole degrees, region, country');
 const r=await t.q(t.br2,CF.Rio);
 assert.equal(r.match,t.bm.match,'Rio is put with Sao Paulo (younger than Tokyo, nearer)');
 assert.equal(r.role,2);
 console.log('ok   Ranked: Brazil with Brazil, though Japan waited longer');
 const p=await t.q(t.pt,CF.Lisbon);
 assert.equal(p.state,'waiting','Lisbon is not put with Tokyo at once');
 t.step(35);
 const j=await t.q(t.jp,CF.Tokyo);
 const p2=await t.q(t.pt,CF.Lisbon);
 assert.equal(p2.match,t.jm.match,'after 30 s anyone: Lisbon with Tokyo');
 console.log('ok   Ranked: far players are paired once anyone is taken (30 s): nobody is left without a match');
}
{
 const t=await run(false);
 assert.equal(t.bm.match,t.jm.match,'without the column: as before, the oldest in the window at once');
 console.log('ok   Ranked: a database without ranked_matches.loc pairs as before (rating alone)');
}
{
 const {DB}=db(true);let now=1800000000;const env={DB,RANKED_ENABLED:'1'};
 const api=async(path,auth,data={})=>{const r=await rankedFetch(null,env,path,{v:40001,...auth,...data},now,'198.51.100.9');return r.json();};
 const a=await api('register',{},{name:'A'}),b=await api('register',{},{name:'B'});
 const m=await api('queue',a);now++;const n=await api('queue',b);
 assert.equal(n.match,m.match,'no place known (Cloudflare said nothing): as before');
 console.log('ok   Ranked: searches without a place pair as before');
}
console.log('PASS ranked-geo: near first, far after 30 s, unchanged without places or the column');
