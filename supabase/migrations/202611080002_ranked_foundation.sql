-- Ranked storage/read foundation. Admission and scoring start disabled.
-- This script deliberately starts disabled. The result writer is server-only.
begin;
create table if not exists bt4_private.ranked_config (
 singleton boolean primary key default true check(singleton),
 enabled boolean not null default false
);
insert into bt4_private.ranked_config(singleton) values(true) on conflict do nothing;
create table if not exists bt4_private.ranked_seasons (
 id date primary key, starts_at timestamptz not null, ends_at timestamptz not null,
 check(ends_at>starts_at)
);
create table if not exists bt4_private.ranked_profiles (
 season date not null references bt4_private.ranked_seasons(id),
 ruleset text not null check(ruleset in ('solo','teams')),
 user_id uuid not null references auth.users(id), name text not null check(length(name) between 1 and 19),
 rating integer not null default 1000 check(rating between 0 and 100000),
 wins integer not null default 0 check(wins>=0), losses integer not null default 0 check(losses>=0),
 primary key(season,ruleset,user_id)
);
create index if not exists ranked_profiles_order on bt4_private.ranked_profiles(season,ruleset,rating desc,user_id);
create table if not exists bt4_private.ranked_matches (
 id uuid primary key, season date not null references bt4_private.ranked_seasons(id),
 ruleset text not null check(ruleset in ('solo','teams')),
 player_a uuid not null references auth.users(id), player_b uuid not null references auth.users(id),
 admitted_at timestamptz not null default now(), completed_at timestamptz,
 winner uuid, result_digest text,
 check(player_a<>player_b), check(winner is null or winner in (player_a,player_b))
);
create table if not exists bt4_private.ranked_rating_history (
 match_id uuid not null references bt4_private.ranked_matches(id), user_id uuid not null references auth.users(id),
 before integer not null, delta integer not null, after integer not null,
 primary key(match_id,user_id), check(after=before+delta)
);
create table if not exists bt4_private.ranked_snapshots (
 season date not null references bt4_private.ranked_seasons(id),
 ruleset text not null check(ruleset in ('solo','teams')),
 updated_at timestamptz not null, entries jsonb not null check(jsonb_typeof(entries)='array'),
 primary key(season,ruleset)
);
alter table bt4_private.ranked_config enable row level security;
alter table bt4_private.ranked_seasons enable row level security;
alter table bt4_private.ranked_profiles enable row level security;
alter table bt4_private.ranked_matches enable row level security;
alter table bt4_private.ranked_rating_history enable row level security;
alter table bt4_private.ranked_snapshots enable row level security;
revoke all on bt4_private.ranked_config,bt4_private.ranked_seasons,bt4_private.ranked_profiles,
 bt4_private.ranked_matches,bt4_private.ranked_rating_history,bt4_private.ranked_snapshots from public,anon,authenticated;

-- Sunday at 23:59 Argentina. Boundary computation also works before the cron runs.
create or replace function bt4_private.ranked_season_at(t timestamptz) returns timestamptz
language sql immutable set search_path='' as $$
 select (date_trunc('week',(t at time zone 'America/Argentina/Buenos_Aires')+interval '1 minute')
         -interval '1 minute') at time zone 'America/Argentina/Buenos_Aires'
$$;
create or replace function bt4_private.ranked_rollover() returns void
language plpgsql security definer set search_path='' as $$
declare boundary timestamptz:=bt4_private.ranked_season_at(now());
begin
 insert into bt4_private.ranked_seasons(id,starts_at,ends_at)
 values((boundary at time zone 'America/Argentina/Buenos_Aires')::date,boundary,boundary+interval '7 days')
 on conflict do nothing;
end $$;

-- Only an independently trusted result verifier may call this function.
-- Match admission must have inserted the participants before play starts.
create or replace function bt4_private.ranked_record_result(p_match uuid,p_winner uuid,p_digest text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare m bt4_private.ranked_matches%rowtype; a integer; b integer; gain integer; outcome numeric;
begin
 if p_digest is null or p_digest !~ '^[a-f0-9]{64}$' then raise exception 'BAD_DIGEST'; end if;
 if not exists(select 1 from bt4_private.ranked_config where singleton and enabled) then raise exception 'RANKED_NOT_ENABLED'; end if;
 -- Consistent serialization prevents opposite participant lock order deadlocks.
 perform pg_advisory_xact_lock(5454,4001);
 select * into m from bt4_private.ranked_matches where id=p_match for update;
 if not found then raise exception 'MATCH_NOT_ADMITTED'; end if;
 if m.completed_at is not null then
  if m.winner is not distinct from p_winner and m.result_digest=p_digest then
   return jsonb_build_object('state','already_applied');
  end if;
  raise exception 'CONFLICTING_RESULT';
 end if;
 if p_winner is not null and p_winner not in (m.player_a,m.player_b) then raise exception 'BAD_WINNER'; end if;
 if not exists(select 1 from bt4_private.ranked_seasons s where s.id=m.season
  and m.admitted_at>=s.starts_at and m.admitted_at<s.ends_at) then raise exception 'BAD_SEASON'; end if;
 select rating into strict a from bt4_private.ranked_profiles where season=m.season and ruleset=m.ruleset and user_id=m.player_a for update;
 select rating into strict b from bt4_private.ranked_profiles where season=m.season and ruleset=m.ruleset and user_id=m.player_b for update;
 outcome:=case when p_winner=m.player_a then 1 when p_winner=m.player_b then 0 else 0.5 end;
 gain:=round(32*(outcome-1/(1+power(10::numeric,(b-a)::numeric/400))))::integer;
 -- Clamp the shared transfer so one player's floor cannot manufacture points.
 gain:=greatest(-a,b-100000,least(b,100000-a,gain));
 update bt4_private.ranked_profiles set rating=rating+gain,
  wins=wins+case when p_winner=m.player_a then 1 else 0 end,
  losses=losses+case when p_winner=m.player_b then 1 else 0 end
  where season=m.season and ruleset=m.ruleset and user_id=m.player_a;
 update bt4_private.ranked_profiles set rating=rating-gain,
  wins=wins+case when p_winner=m.player_b then 1 else 0 end,
  losses=losses+case when p_winner=m.player_a then 1 else 0 end
  where season=m.season and ruleset=m.ruleset and user_id=m.player_b;
 insert into bt4_private.ranked_rating_history values(p_match,m.player_a,a,gain,a+gain),(p_match,m.player_b,b,-gain,b-gain);
 update bt4_private.ranked_matches set winner=p_winner,result_digest=p_digest,completed_at=now() where id=p_match;
 return jsonb_build_object('state','applied');
end $$;

create or replace function bt4_private.ranked_refresh_top() returns void
language plpgsql security definer set search_path='' as $$
declare current_season date; format text;
begin
 perform bt4_private.ranked_rollover();
 current_season:=(bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date;
 foreach format in array array['solo','teams'] loop
 insert into bt4_private.ranked_snapshots(season,ruleset,updated_at,entries)
 select current_season,format,now(),coalesce(jsonb_agg(jsonb_build_object('name',p.name,'rating',p.rating,'wins',p.wins,'losses',p.losses)
  order by p.rating desc,p.user_id),'[]'::jsonb)
 from (select name,rating,wins,losses,user_id from bt4_private.ranked_profiles
  where season=current_season and ruleset=format order by rating desc,user_id limit 100) p
 on conflict(season,ruleset) do update set updated_at=excluded.updated_at,entries=excluded.entries;
 end loop;
end $$;

create or replace function public.bt4_ranked_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); sid date; profile bt4_private.ranked_profiles%rowtype;
 snapshot bt4_private.ranked_snapshots%rowtype; active boolean; format text;
begin
 if who is null then return jsonb_build_object('error','AUTH_REQUIRED'); end if;
 if p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>512 then
  return jsonb_build_object('error','BAD_REQUEST'); end if;
 format:=coalesce(p_args->>'ruleset','solo');
 if format not in ('solo','teams') then return jsonb_build_object('error','BAD_RULESET'); end if;
 sid:=(bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date;
 select enabled into active from bt4_private.ranked_config where singleton;
 if p_op='status' then
  select * into profile from bt4_private.ranked_profiles where season=sid and ruleset=format and user_id=who;
  return jsonb_build_object('enabled',coalesce(active,false),'season',sid,'rating',coalesce(profile.rating,0),
   'wins',coalesce(profile.wins,0),'losses',coalesce(profile.losses,0));
 elsif p_op='top' then
  select * into snapshot from bt4_private.ranked_snapshots where season=sid and ruleset=format;
  return jsonb_build_object('season',sid,'updated_at',snapshot.updated_at,'entries',coalesce(snapshot.entries,'[]'::jsonb));
 end if;
 return jsonb_build_object('error','RANKED_NOT_ENABLED');
end $$;
revoke all on function public.bt4_ranked_api(text,jsonb) from public,anon;
grant execute on function public.bt4_ranked_api(text,jsonb) to authenticated;
revoke all on function bt4_private.ranked_season_at(timestamptz),bt4_private.ranked_rollover(),
 bt4_private.ranked_record_result(uuid,uuid,text),bt4_private.ranked_refresh_top() from public,anon,authenticated;
grant execute on function bt4_private.ranked_record_result(uuid,uuid,text) to service_role;
grant usage on schema bt4_private to service_role;
-- pg_cron uses UTC here: Sunday 23:59 Argentina is Monday 02:59 UTC.
-- Add these jobs only after applying this draft and reviewing cron.job:
-- select cron.schedule('bt4-ranked-hourly','0 * * * *','select bt4_private.ranked_refresh_top()');
-- select cron.schedule('bt4-ranked-weekly','59 2 * * 1','select bt4_private.ranked_rollover(); select bt4_private.ranked_refresh_top()');
commit;
