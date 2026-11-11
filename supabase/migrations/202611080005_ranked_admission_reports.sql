-- Two isolated native queues and authenticated, agreeing result reports.
-- The secondary project (backend 1) is the single rating authority on both DBs.
begin;
alter table bt4_private.ranked_config add column authority_backend smallint not null default 1 check(authority_backend in (0,1));
update bt4_private.ranked_config set authority_backend=1,enabled=false where singleton;
alter table bt4_private.ranked_matches add column disputed boolean not null default false;
create table bt4_private.ranked_reports (
 match_id uuid not null references bt4_private.ranked_matches(id),
 user_id uuid not null references auth.users(id),
 outcome integer not null check(outcome between 0 and 2),
 digest text not null check(digest ~ '^[a-f0-9]{64}$'),
 reported_at timestamptz not null default now(),
 primary key(match_id,user_id)
);
alter table bt4_private.ranked_reports enable row level security;
revoke all on bt4_private.ranked_reports from public,anon,authenticated;
alter function public.bt4_ranked_api(text,jsonb) rename to bt4_ranked_reads;
revoke all on function public.bt4_ranked_reads(text,jsonb) from public,anon,authenticated;

create function public.bt4_ranked_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); cfg bt4_private.ranked_config%rowtype; route bt4_private.route_config%rowtype;
 format text; sid date; reply jsonb; args jsonb; rid uuid; generation_id uuid; first_player uuid; second_player uuid;
 m bt4_private.ranked_matches%rowtype; mine bt4_private.ranked_reports%rowtype; theirs bt4_private.ranked_reports%rowtype;
 selected_outcome integer; result_hash text; chosen uuid; profile record; match_wire text; initial_rating integer;
begin
 if who is null then return jsonb_build_object('error','AUTH_REQUIRED'); end if;
 if p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>2048 then
  return jsonb_build_object('error','BAD_REQUEST'); end if;
 select * into cfg from bt4_private.ranked_config where singleton;
 if not found then return jsonb_build_object('error','RANKED_UNAVAILABLE'); end if;
 select * into route from bt4_private.route_config where singleton;
 if not found then return jsonb_build_object('error','SIGNALING_UNAVAILABLE'); end if;
 -- A single ranking authority avoids independent scores during casual failover.
 if cfg.authority_backend<>route.backend_id then return jsonb_build_object('error','RANKED_UNAVAILABLE'); end if;
 if p_op in ('status','top') then
  reply:=public.bt4_ranked_reads(p_op,p_args);
  if p_op='status' and not (reply ? 'error') then
   select rating into initial_rating from bt4_private.ranked_players
    where ruleset=p_args->>'ruleset' and user_id=who;
   reply:=reply||jsonb_build_object('enabled',cfg.enabled and route.admission_open
    and (route.admission_until is null or route.admission_until>clock_timestamp()),
    'rating',coalesce(initial_rating,1000));
  end if;
  return reply;
 end if;
 if not cfg.enabled then return jsonb_build_object('error','RANKED_NOT_ENABLED'); end if;
 if p_op='queue' then
  perform bt4_private.budget('ranked_queue',20);
  format:=p_args->>'ruleset';
  if format is null or format not in ('solo','teams') then return jsonb_build_object('error','BAD_RULESET'); end if;
  if p_args->>'mode' is distinct from 'duel' or p_args->>'rules' is distinct from (case when format='solo' then 'S' else 'T' end)
   or length(coalesce(p_args->>'wire','')) not between 1 and 40 then return jsonb_build_object('error','BAD_REQUEST'); end if;
  match_wire:='ranked-v1:'||(p_args->>'wire');
  args:=(p_args-'ruleset')||jsonb_build_object('wire',match_wire,'capacity',2);
  -- Reuse canonical version, route, data, profile and queue validation.
  reply:=public.bt4_api('queue',args);
  if reply ? 'error' or reply->>'state'='waiting' then return reply; end if;
  rid:=(reply->>'room')::uuid;generation_id:=(reply->>'generation')::uuid;
  if not exists(select 1 from bt4_private.rooms r where r.id=rid and r.generation=generation_id
   and r.mode='duel' and r.wire=match_wire and r.rules_key=p_args->>'rules' and r.content=p_args->>'content') then
   return jsonb_build_object('error','ALREADY_IN_ROOM'); end if;
  select user_id into first_player from bt4_private.members where room_id=rid and peer=1;
  select user_id into second_player from bt4_private.members where room_id=rid and peer=2;
  if first_player is null or second_player is null or first_player=second_player then raise exception 'BAD_MATCH'; end if;
  perform bt4_private.ranked_rollover();
  sid:=(bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date;
  for profile in select user_id,name from bt4_private.members where room_id=rid and user_id in(first_player,second_player) loop
   insert into bt4_private.ranked_players(ruleset,user_id,name) values(format,profile.user_id,profile.name)
    on conflict(ruleset,user_id) do update set name=excluded.name;
   insert into bt4_private.ranked_profiles(season,ruleset,user_id,name,rating)
    select sid,format,a.user_id,a.name,a.rating from bt4_private.ranked_players a where a.ruleset=format and a.user_id=profile.user_id
    on conflict(season,ruleset,user_id) do update set name=excluded.name;
  end loop;
  insert into bt4_private.ranked_matches(id,season,ruleset,player_a,player_b)
   values(generation_id,sid,format,first_player,second_player) on conflict(id) do nothing;
  return reply||jsonb_build_object('ranked',true,'ruleset',format);
 elsif p_op='report' then
  perform bt4_private.budget('ranked_report',30);
  generation_id:=(p_args->>'match')::uuid;
  if coalesce(p_args->>'outcome','') !~ '^[012]$' then return jsonb_build_object('error','BAD_RESULT'); end if;
  selected_outcome:=(p_args->>'outcome')::integer;result_hash:=p_args->>'digest';
  if result_hash is null or result_hash !~ '^[a-f0-9]{64}$' then return jsonb_build_object('error','BAD_DIGEST'); end if;
  -- Match lock order matches the rating writer and makes concurrent reports idempotent.
  perform pg_advisory_xact_lock(5454,4001);
  select * into m from bt4_private.ranked_matches where id=generation_id for update;
  if not found or who not in (m.player_a,m.player_b) then return jsonb_build_object('error','NOT_A_PARTICIPANT'); end if;
  if m.completed_at is null and m.admitted_at<now()-interval '90 minutes' then return jsonb_build_object('error','MATCH_EXPIRED'); end if;
  select * into mine from bt4_private.ranked_reports where match_id=m.id and user_id=who;
  if found and (mine.outcome<>selected_outcome or mine.digest<>result_hash) then return jsonb_build_object('error','RESULT_CONFLICT'); end if;
  insert into bt4_private.ranked_reports(match_id,user_id,outcome,digest)
   values(m.id,who,selected_outcome,result_hash) on conflict do nothing;
  select * into theirs from bt4_private.ranked_reports where match_id=m.id and user_id<>who;
  if not found then return jsonb_build_object('state','waiting'); end if;
  if theirs.outcome<>selected_outcome or theirs.digest<>result_hash then
   update bt4_private.ranked_matches set disputed=true where id=m.id;
   return jsonb_build_object('state','disputed');
  end if;
  chosen:=case selected_outcome when 0 then m.player_a when 1 then m.player_b else null end;
  reply:=bt4_private.ranked_record_result(m.id,chosen,result_hash);
  return reply||coalesce((select jsonb_build_object('rating',h.after,'delta',h.delta)
   from bt4_private.ranked_rating_history h where h.match_id=m.id and h.user_id=who),'{}'::jsonb);
 end if;
 return jsonb_build_object('error','BAD_REQUEST');
exception when invalid_text_representation or numeric_value_out_of_range then
 return jsonb_build_object('error','BAD_REQUEST');
end $$;
revoke all on function public.bt4_ranked_api(text,jsonb) from public,anon;
grant execute on function public.bt4_ranked_api(text,jsonb) to authenticated;
notify pgrst,'reload schema';
commit;
