begin;
set local lock_timeout = '1s';
set local statement_timeout = '25s';

-- Use the existing Ranked authority for the nickname namespace. The casual
-- failover route has its own Auth IDs and cannot reserve names independently.
create table bt4_private.online_nickname_config (
 singleton boolean primary key default true check(singleton),
 enforce boolean not null default false,
 solo_only boolean not null default false
);
insert into bt4_private.online_nickname_config default values;
create table bt4_private.online_nicknames (
 user_id uuid primary key references auth.users(id) on delete cascade,
 name text not null check(name=btrim(name) and octet_length(name) between 1 and 19
                          and lower(name)<>'player' and name !~ '[[:cntrl:]]'),
 name_key text generated always as (lower(name)) stored unique
);
alter table bt4_private.online_nickname_config enable row level security;
alter table bt4_private.online_nicknames enable row level security;
revoke all on bt4_private.online_nickname_config,bt4_private.online_nicknames from public,anon,authenticated;

-- Preserve the latest name from each known account. When names collide, the
-- oldest account keeps the reservation. Rankings remain keyed by Auth UUID.
with known as (
 select distinct on (p.user_id) p.user_id,btrim(p.name) as name,u.created_at
 from bt4_private.ranked_players p join auth.users u on u.id=p.user_id
 order by p.user_id,p.updated_at desc,p.ruleset desc
), owners as (
 select distinct on (lower(name)) user_id,name
 from known where octet_length(name) between 1 and 19 and lower(name)<>'player' and name !~ '[[:cntrl:]]'
 order by lower(name),created_at nulls last,user_id
)
insert into bt4_private.online_nicknames(user_id,name) select user_id,name from owners;

create function bt4_private.nickname_authority() returns boolean
language sql stable set search_path='' as $$
 select exists(select 1 from bt4_private.route_config r join bt4_private.ranked_config c on c.singleton
  where r.singleton and r.backend_id=c.authority_backend)
$$;

create function bt4_private.nickname_api(p_op text,p_args jsonb) returns jsonb
language plpgsql set search_path='' as $$
declare who uuid:=auth.uid(); chosen text;
begin
 if who is null then return jsonb_build_object('error','AUTH_REQUIRED'); end if;
 if p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>256 then
  return jsonb_build_object('error','BAD_REQUEST');
 end if;
 if not bt4_private.nickname_authority() then return jsonb_build_object('state','disabled'); end if;
 if p_op='nickname_get' then
  select name into chosen from bt4_private.online_nicknames where user_id=who;
  if chosen is null then return jsonb_build_object('state',case
   when (select enforce from bt4_private.online_nickname_config where singleton) then 'required'
   else 'optional' end); end if;
 else
  perform bt4_private.budget('nickname_set',10);
  if jsonb_typeof(p_args->'name') is distinct from 'string' then
   return jsonb_build_object('error','INVALID_NICKNAME');
  end if;
  chosen:=btrim(p_args->>'name');
  if octet_length(chosen) not between 1 and 19 or lower(chosen)='player' or chosen ~ '[[:cntrl:]]' then
   return jsonb_build_object('error','INVALID_NICKNAME');
  end if;
  insert into bt4_private.online_nicknames(user_id,name) values(who,chosen)
   on conflict(user_id) do update set name=excluded.name;
 end if;
 return jsonb_build_object('state','ready','name',chosen);
exception when unique_violation then return jsonb_build_object('error','NICKNAME_TAKEN');
end $$;

revoke all on function bt4_private.nickname_authority(),bt4_private.nickname_api(text,jsonb) from public,anon,authenticated;

alter function public.bt4_api(text,jsonb) rename to bt4_api_before_nicknames;
revoke all on function public.bt4_api_before_nicknames(text,jsonb) from public,anon,authenticated;
create function public.bt4_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare chosen text;
begin
 if p_op in ('nickname_get','nickname_set') then return bt4_private.nickname_api(p_op,p_args); end if;
 if p_op in ('create','join','queue') and jsonb_typeof(p_args)='object' and auth.uid() is not null
    and (select enforce from bt4_private.online_nickname_config where singleton)
    and bt4_private.nickname_authority() then
  select name into chosen from bt4_private.online_nicknames where user_id=auth.uid();
  if chosen is not null then
   p_args:=jsonb_set(p_args,'{name}',to_jsonb(chosen));
  else
   return jsonb_build_object('error','NICKNAME_REQUIRED');
  end if;
 end if;
 return public.bt4_api_before_nicknames(p_op,p_args);
end $$;
revoke all on function public.bt4_api(text,jsonb) from public,anon;
grant execute on function public.bt4_api(text,jsonb) to authenticated;

-- Gate new team searches separately so existing matches can report results.
alter function public.bt4_ranked_gameplay(text,jsonb) rename to bt4_ranked_gameplay_before_solo;
revoke all on function public.bt4_ranked_gameplay_before_solo(text,jsonb) from public,anon,authenticated;
create function public.bt4_ranked_gameplay(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql set search_path='' as $$
begin
 if p_op='queue' and auth.uid() is not null and p_args->>'ruleset'='teams'
    and (select solo_only from bt4_private.online_nickname_config where singleton) then
  return jsonb_build_object('error','BAD_RULESET');
 end if;
 return public.bt4_ranked_gameplay_before_solo(p_op,p_args);
end $$;
revoke all on function public.bt4_ranked_gameplay(text,jsonb) from public,anon,authenticated;

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
   select coalesce(n.name,a.name) as name,a.rating,p.wins,p.losses,p.user_id from bt4_private.ranked_profiles p
   join bt4_private.ranked_players a on a.ruleset=p.ruleset and a.user_id=p.user_id
   left join bt4_private.online_nicknames n on n.user_id=p.user_id
   where p.season=current_season and p.ruleset=format and exists(
    select 1 from bt4_private.ranked_rating_history h join bt4_private.ranked_matches m on m.id=h.match_id
    where h.user_id=p.user_id and m.season=current_season and m.ruleset=format)
   order by a.rating desc,p.user_id limit 100
  ) p
  on conflict(season,ruleset) do update set updated_at=excluded.updated_at,entries=excluded.entries;
 end loop;
end $$;

-- Update the existing named job. The separate Sunday rollover stays scheduled.
select cron.schedule('bt4-ranked-hourly','*/10 * * * *','select bt4_private.ranked_refresh_top()');
notify pgrst,'reload schema';
commit;
