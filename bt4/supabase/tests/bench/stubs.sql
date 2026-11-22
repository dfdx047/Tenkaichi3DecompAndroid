create role anon nologin; create role authenticated nologin; create role service_role nologin;
create schema auth; create table auth.users(id uuid primary key default gen_random_uuid(), created_at timestamptz default now(), raw_user_meta_data jsonb default '{}', is_anonymous boolean default true, email text);
create function auth.uid() returns uuid language sql stable as $$ select nullif(current_setting('request.jwt.claim.sub',true),'')::uuid $$;
create schema realtime; create function realtime.send(payload jsonb, event text, topic text, private boolean default true) returns void language sql as $$ select $$;
create table realtime.messages(id bigint generated always as identity primary key, topic text, extension text, payload jsonb, event text, private boolean, inserted_at timestamptz default now());
create function realtime.topic() returns text language sql as $$ select current_setting('realtime.topic',true) $$;
create schema extensions; create schema cron; create table cron.job(jobid serial primary key, jobname text, schedule text, command text);
create function cron.schedule(n text, s text, c text) returns bigint language sql as $$ insert into cron.job(jobname,schedule,command) values(n,s,c) returning jobid::bigint $$;
create function cron.unschedule(n text) returns boolean language sql as $$ delete from cron.job where jobname=n returning true $$;
