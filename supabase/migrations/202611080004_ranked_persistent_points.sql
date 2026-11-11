-- Ratings persist between weekly seasons. Weekly records/snapshots stay separate.
begin;
create table bt4_private.ranked_players (
 ruleset text not null check(ruleset in ('solo','teams')),
 user_id uuid not null references auth.users(id),
 name text not null check(length(name) between 1 and 19),
 rating integer not null default 1000 check(rating between 0 and 100000),
 wins integer not null default 0 check(wins>=0),
 losses integer not null default 0 check(losses>=0),
 updated_at timestamptz not null default now(),
 primary key(ruleset,user_id)
);
alter table bt4_private.ranked_players enable row level security;
revoke all on bt4_private.ranked_players from public,anon,authenticated;
with latest as (
 select distinct on (ruleset,user_id) ruleset,user_id,name,rating
 from bt4_private.ranked_profiles order by ruleset,user_id,season desc
), totals as (
 select ruleset,user_id,sum(wins)::integer wins,sum(losses)::integer losses
 from bt4_private.ranked_profiles group by ruleset,user_id
)
insert into bt4_private.ranked_players(ruleset,user_id,name,rating,wins,losses)
select p.ruleset,p.user_id,p.name,p.rating,t.wins,t.losses from latest p
join totals t using(ruleset,user_id);
create index ranked_history_user_match on bt4_private.ranked_rating_history(user_id,match_id);
create index ranked_matches_week_rules on bt4_private.ranked_matches(season,ruleset,id);

create or replace function bt4_private.ranked_record_result(p_match uuid,p_winner uuid,p_digest text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare m bt4_private.ranked_matches%rowtype; a integer; b integer; gain integer; outcome numeric;
begin
 if p_digest is null or p_digest !~ '^[a-f0-9]{64}$' then raise exception 'BAD_DIGEST'; end if;
 if not exists(select 1 from bt4_private.ranked_config where singleton and enabled) then raise exception 'RANKED_NOT_ENABLED'; end if;
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
 if (select count(*) from bt4_private.ranked_profiles where season=m.season and ruleset=m.ruleset
  and user_id in (m.player_a,m.player_b))<>2 then raise exception 'MATCH_NOT_ADMITTED'; end if;
 select rating into strict a from bt4_private.ranked_players where ruleset=m.ruleset and user_id=m.player_a for update;
 select rating into strict b from bt4_private.ranked_players where ruleset=m.ruleset and user_id=m.player_b for update;
 outcome:=case when p_winner=m.player_a then 1 when p_winner=m.player_b then 0 else 0.5 end;
 gain:=round(32*(outcome-1/(1+power(10::numeric,(b-a)::numeric/400))))::integer;
 gain:=greatest(-a,b-100000,least(b,100000-a,gain));
 update bt4_private.ranked_players set rating=rating+gain,updated_at=now(),
  wins=wins+case when p_winner=m.player_a then 1 else 0 end,
  losses=losses+case when p_winner=m.player_b then 1 else 0 end
  where ruleset=m.ruleset and user_id=m.player_a;
 update bt4_private.ranked_players set rating=rating-gain,updated_at=now(),
  wins=wins+case when p_winner=m.player_b then 1 else 0 end,
  losses=losses+case when p_winner=m.player_a then 1 else 0 end
  where ruleset=m.ruleset and user_id=m.player_b;
 update bt4_private.ranked_profiles set rating=a+gain,
  wins=wins+case when p_winner=m.player_a then 1 else 0 end,
  losses=losses+case when p_winner=m.player_b then 1 else 0 end
  where season=m.season and ruleset=m.ruleset and user_id=m.player_a;
 update bt4_private.ranked_profiles set rating=b-gain,
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
  from (
   select a.name,a.rating,p.wins,p.losses,p.user_id from bt4_private.ranked_profiles p
   join bt4_private.ranked_players a on a.ruleset=p.ruleset and a.user_id=p.user_id
   where p.season=current_season and p.ruleset=format and exists(
    select 1 from bt4_private.ranked_rating_history h join bt4_private.ranked_matches m on m.id=h.match_id
    where h.user_id=p.user_id and m.season=current_season and m.ruleset=format)
   order by a.rating desc,p.user_id limit 100
  ) p
  on conflict(season,ruleset) do update set updated_at=excluded.updated_at,entries=excluded.entries;
 end loop;
end $$;

create or replace function public.bt4_ranked_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); sid date; profile bt4_private.ranked_profiles%rowtype;
 player bt4_private.ranked_players%rowtype; snapshot bt4_private.ranked_snapshots%rowtype; active boolean; format text;
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
  select * into player from bt4_private.ranked_players where ruleset=format and user_id=who;
  return jsonb_build_object('enabled',coalesce(active,false),'season',sid,'rating',coalesce(player.rating,0),
   'wins',coalesce(profile.wins,0),'losses',coalesce(profile.losses,0),'points_persist',true);
 elsif p_op='top' then
  select * into snapshot from bt4_private.ranked_snapshots where season=sid and ruleset=format;
  return jsonb_build_object('season',sid,'updated_at',snapshot.updated_at,'entries',coalesce(snapshot.entries,'[]'::jsonb));
 end if;
 return jsonb_build_object('error','RANKED_NOT_ENABLED');
end $$;
revoke all on function public.bt4_ranked_api(text,jsonb) from public,anon;
grant execute on function public.bt4_ranked_api(text,jsonb) to authenticated;
revoke all on function bt4_private.ranked_record_result(uuid,uuid,text),bt4_private.ranked_refresh_top() from public,anon,authenticated;
grant execute on function bt4_private.ranked_record_result(uuid,uuid,text) to service_role;
commit;
