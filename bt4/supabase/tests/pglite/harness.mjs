import {PGlite} from '@electric-sql/pglite';
// Runs EVERY real migration in order on an in-process Postgres (PGlite) with stand-ins for what Supabase provides (auth.users, auth.uid(),
// roles, realtime, cron). No network, no Docker. `npm install` in this folder first.
import {readdirSync,readFileSync} from 'node:fs';
const MIG=new URL('../../migrations/',import.meta.url).pathname;
export async function boot(opts={}){
  const db=new PGlite();
  await db.exec(`
    create role anon nologin; create role authenticated nologin; create role service_role nologin;
    create schema auth; create table auth.users(id uuid primary key default gen_random_uuid(), created_at timestamptz default now(), raw_user_meta_data jsonb default '{}', is_anonymous boolean default true, email text);
    create function auth.uid() returns uuid language sql stable as $$ select nullif(current_setting('request.jwt.claim.sub',true),'')::uuid $$;
    create schema realtime; create function realtime.send(payload jsonb, event text, topic text, private boolean default true) returns void language sql as $$ select $$;
    create schema extensions; create schema cron; create table cron.job(jobid serial primary key, jobname text, schedule text, command text); create function cron.schedule(n text, s text, c text) returns bigint language sql as $$ insert into cron.job(jobname,schedule,command) values(n,s,c) returning jobid::bigint $$; create function cron.unschedule(n text) returns boolean language sql as $$ delete from cron.job where jobname=n returning true $$; create table realtime.messages(id bigint generated always as identity primary key, topic text, extension text, payload jsonb, event text, private boolean, inserted_at timestamptz default now()); create function realtime.topic() returns text language sql as $$ select current_setting('realtime.topic',true) $$;
  `);
  const files=readdirSync(MIG).filter(f=>f.endsWith('.sql')&&!(opts.skip||[]).some(x=>f.includes(x))).sort();
  const failed=[];
  for(const f of files){
    try{await db.exec(readFileSync(MIG+f,'utf8'));}catch(e){failed.push([f,e.message]);try{await db.exec('rollback');}catch{}}
  }
  return {db,files,failed};
}
if(process.argv[1].endsWith('harness.mjs')){
  const {files,failed}=await boot();
  console.log(files.length,'migrations;','failed:',JSON.stringify(failed,null,1));
}
