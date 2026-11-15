// Local HTTP adapter for the production ranked Worker. Termux/Node 22+ only.
// Bind to a private LAN address when testing with a second phone. SPDX-License-Identifier: MIT
import {createServer} from 'node:http';
import {randomBytes} from 'node:crypto';
import {DatabaseSync} from 'node:sqlite';
import fs from 'node:fs';
import path from 'node:path';
import worker from './worker.js';

const root=path.dirname(new URL(import.meta.url).pathname);
const dbPath=path.resolve(process.env.RANKED_LOCAL_DB||path.join(root,'ranked-local.sqlite'));
const sql=new DatabaseSync(dbPath);
for(const file of ['schema.sql','ranked-schema.sql','ranked-flow-schema.sql','chat-schema.sql','presence-schema.sql'])
 sql.exec(fs.readFileSync(path.join(root,file),'utf8'));
const DB={
 prepare(text){let values=[];return {bind(...args){values=args;return this;},async first(){return sql.prepare(text).get(...values)??null;},async run(){const r=sql.prepare(text).run(...values);return {meta:{changes:Number(r.changes),last_row_id:Number(r.lastInsertRowid??0)}};},async all(){return {results:sql.prepare(text).all(...values)};}};},
 async batch(statements){sql.exec('BEGIN');try{const result=[];for(const s of statements)result.push(await s.run());sql.exec('COMMIT');return result;}catch(e){sql.exec('ROLLBACK');throw e;}}
};
sql.exec(`CREATE TABLE IF NOT EXISTS debug_forced_results(match_id TEXT PRIMARY KEY,host_score INTEGER NOT NULL,digest TEXT NOT NULL,remaining INTEGER NOT NULL DEFAULT 2);`);
const env={DB,RANKED_ENABLED:'1',CHAT_ENABLED:'1',ALLOWED_VERSIONS:'5001'};
const host=process.env.RANKED_LOCAL_ADDR||'0.0.0.0',port=Number(process.env.RANKED_LOCAL_PORT||8787);
const adminKey=process.env.RANKED_DEBUG_KEY||randomBytes(18).toString('hex');
const send=(res,status,obj)=>{const data=JSON.stringify(obj);res.writeHead(status,{'content-type':'application/json','cache-control':'no-store','access-control-allow-origin':'*','access-control-allow-headers':'content-type'});res.end(data);};
async function readBody(req){let raw='';for await(const chunk of req)raw+=chunk;if(raw.length>8192)throw Error('body too large');return raw?JSON.parse(raw):{};}
function admin(req,res,body){if(body.key!==adminKey){send(res,403,{error:'debug key'});return false;}return true;}
const server=createServer(async(req,res)=>{
 const started=Date.now();let logged=false;
 const logRequest=()=>{if(logged)return;logged=true;console.log(`${new Date().toISOString()} ${req.method} ${req.url} -> ${res.statusCode} ${Date.now()-started}ms from ${req.socket.remoteAddress||'unknown'}`);};
 res.once('finish',logRequest);res.once('close',logRequest);
 try{
  const url=new URL(req.url,`http://${req.headers.host||'localhost'}`);
  if(req.method==='OPTIONS'){res.writeHead(204,{'access-control-allow-origin':'*','access-control-allow-methods':'GET,POST,OPTIONS','access-control-allow-headers':'content-type'});return res.end();}
  if(url.pathname==='/__debug/state'&&req.method==='GET'){
   if(url.searchParams.get('key')!==adminKey)return send(res,403,{error:'debug key'});
   const matches=sql.prepare(`SELECT m.id,m.state,m.host_id,m.guest_id,m.series_id,m.host_score,m.guest_score,
    COALESCE((SELECT SUM(host_wins) FROM ranked_series_results r WHERE r.series_id=COALESCE(m.series_id,m.id)),0) host_wins,
    COALESCE((SELECT SUM(guest_wins) FROM ranked_series_results r WHERE r.series_id=COALESCE(m.series_id,m.id)),0) guest_wins
    FROM ranked_matches m ORDER BY m.made DESC LIMIT 12`).all();
   return send(res,200,{matches});
  }
  if(url.pathname.startsWith('/__debug/')){
   const body=await readBody(req);if(!admin(req,res,body))return;
   if(url.pathname==='/__debug/seed-series'){
    const m=sql.prepare("SELECT * FROM ranked_matches WHERE id=? AND state='paired' AND host_score IS NULL AND guest_score IS NULL").get(body.match);
    if(!m)return send(res,404,{error:'paired match required'});
    const hw=Number(body.host_wins),gw=Number(body.guest_wins);
    if(!Number.isInteger(hw)||!Number.isInteger(gw)||hw<0||gw<0||hw>1||gw>1||hw+gw>2)return send(res,400,{error:'wins must be 0 or 1 each'});
    const series=`debug-${m.id}`;sql.exec('BEGIN');try{
     sql.prepare('UPDATE ranked_matches SET series_id=? WHERE id=?').run(series,m.id);
     sql.prepare('DELETE FROM ranked_series_results WHERE series_id=?').run(series);
     for(let i=0;i<hw;i++)sql.prepare('INSERT INTO ranked_series_results(match_id,series_id,host_wins,guest_wins,made) VALUES(?,?,1,0,?)').run(`debug:${m.id}:h:${i}`,series,Math.floor(Date.now()/1000));
     for(let i=0;i<gw;i++)sql.prepare('INSERT INTO ranked_series_results(match_id,series_id,host_wins,guest_wins,made) VALUES(?,?,0,1,?)').run(`debug:${m.id}:g:${i}`,series,Math.floor(Date.now()/1000));
     sql.exec('COMMIT');
    }catch(e){sql.exec('ROLLBACK');throw e;}
    console.log(`DEBUG series seeded ${m.id.slice(0,8)} = ${hw}-${gw}`);return send(res,200,{ok:true,series,host_wins:hw,guest_wins:gw});
   }
   if(url.pathname==='/__debug/force-result'){
    const m=sql.prepare("SELECT id FROM ranked_matches WHERE id=? AND state='paired'").get(body.match);
    if(!m)return send(res,404,{error:'paired match required'});
    if(![0,1,2].includes(body.host_score))return send(res,400,{error:'host_score must be 0, 1, or 2'});
    sql.prepare('INSERT INTO debug_forced_results(match_id,host_score,digest,remaining) VALUES(?,?,?,2) ON CONFLICT(match_id) DO UPDATE SET host_score=excluded.host_score,digest=excluded.digest,remaining=2').run(m.id,body.host_score,randomBytes(4).toString('hex'));
    console.log(`DEBUG next reports for ${m.id.slice(0,8)} forced to host score ${body.host_score}`);return send(res,200,{ok:true,match:m.id,host_score:body.host_score});
   }
   return send(res,404,{error:'unknown debug endpoint'});
  }
  if(req.method!=='POST'||!url.pathname.startsWith('/v1/'))return send(res,404,{error:'bad request'});
  let raw=await readBody(req);
  if(url.pathname==='/v1/ranked/result'&&typeof raw.match==='string'){
   const f=sql.prepare('SELECT * FROM debug_forced_results WHERE match_id=? AND remaining>0').get(raw.match);
   if(f){raw={...raw,score:f.host_score,digest:f.digest};sql.prepare('UPDATE debug_forced_results SET remaining=remaining-1 WHERE match_id=?').run(f.match_id);console.log(`DEBUG forced ranked result ${f.match_id.slice(0,8)} host=${f.host_score}`);}
  }
  const request=new Request(`http://${req.headers.host||'127.0.0.1'}${url.pathname}`,{method:'POST',headers:{'content-type':'application/json','cf-connecting-ip':req.socket.remoteAddress||'127.0.0.1'},body:JSON.stringify(raw)});
  const answer=await worker.fetch(request,env);res.writeHead(answer.status,Object.fromEntries(answer.headers));res.end(await answer.text());
 }catch(e){console.error('local ranked server:',e);if(!res.headersSent)send(res,500,{error:'local server error',detail:String(e.message||e)});else res.end();}
});
server.listen(port,host,()=>{
 console.log(`Local Ranked mock listening on http://${host}:${port}`);
 console.log(`Admin key: ${adminKey}`);
 console.log(`SQLite: ${dbPath}`);
 console.log('Use only the Debug APK and a private network. Ctrl+C stops the server.');
});
