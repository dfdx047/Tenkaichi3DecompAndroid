-- Ranked: at most RANKED_PER_PAIR_DAY (4) battles of one pair of accounts score in a day, enforced IN THE DATABASE and atomically, SWITCHED OFF until validated
-- (ranked_config.pair_day_cap = 0: the function behaves exactly as before this migration; set it to 4 to enforce the limit), plus the measured
-- duration of every settled match (telemetry only: nothing is gated by it). The Worker (port/bt4online-worker/profile.js, RANKED_PER_PAIR_DAY) has the same rule:
-- the first 4 battles of a pair in a (Brasilia) day move ratings, wins, losses and history; later ones are recorded as capped and change nothing.
-- Elo, K, the fixed 16-point forfeit, medals and seasons are untouched.
begin;
set local lock_timeout = '2s';

-- 0 = off (the rules before this migration), 1..4 = battles of one pair per day that move ratings. New switches ship OFF.
alter table bt4_private.ranked_config add column if not exists pair_day_cap smallint not null default 0 check(pair_day_cap between 0 and 4);

alter table bt4_private.ranked_matches
 add column if not exists rated boolean not null default true,
 add column if not exists capped boolean not null default false,
 add column if not exists duration_ms integer;

-- One row per (day, ruleset, pair); pa<pb so (A,B) and (B,A) share the counter. (pa/pb: plain a/b would collide with the rating variables of the function.)
create table if not exists bt4_private.ranked_pair_day (
 day date not null, ruleset text not null check(ruleset in ('solo','teams')),
 pa uuid not null references auth.users(id), pb uuid not null references auth.users(id),
 battles integer not null default 0 check(battles>=0),
 rated integer not null default 0 check(rated between 0 and 4),
 primary key(day,ruleset,pa,pb), check(pa<pb)
);
alter table bt4_private.ranked_pair_day enable row level security;
revoke all on bt4_private.ranked_pair_day from public,anon,authenticated;

create or replace function bt4_private.ranked_record_result(p_match uuid,p_winner uuid,p_digest text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare m bt4_private.ranked_matches%rowtype; a integer; b integer; gain integer; outcome numeric;
 pair_day date; pair_rated integer; is_rated boolean; took_ms integer; cap smallint;
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
 -- Measured by the server's clock from the later of the two first heartbeats (null when a client never sent one). Telemetry: no rule reads it.
 took_ms:=case when m.started_a is not null and m.started_b is not null
  then least(2147483647,greatest(0,(extract(epoch from clock_timestamp()-greatest(m.started_a,m.started_b))*1000)::bigint))::integer end;
 -- The daily limit of this pair, ONLY when the switch is on: the counter's row is created or locked by the upsert and raised only while it is under the cap, so two
 -- settlements of the same pair can never both take the last place, with or without the global lock above. Off (0): no counter is touched, the match scores as before.
 select pair_day_cap into cap from bt4_private.ranked_config where singleton;
 is_rated:=true;
 if coalesce(cap,0)>0 then
  pair_day:=(clock_timestamp() at time zone 'America/Sao_Paulo')::date;
  insert into bt4_private.ranked_pair_day(day,ruleset,pa,pb,battles,rated)
   values(pair_day,m.ruleset,least(m.player_a,m.player_b),greatest(m.player_a,m.player_b),1,1)
   on conflict(day,ruleset,pa,pb) do update set battles=bt4_private.ranked_pair_day.battles+1,rated=bt4_private.ranked_pair_day.rated+1
    where bt4_private.ranked_pair_day.rated<cap
   returning rated into pair_rated;
  is_rated:=pair_rated is not null;
 end if;
 if not is_rated then
  update bt4_private.ranked_pair_day set battles=battles+1
   where day=pair_day and ruleset=m.ruleset and pa=least(m.player_a,m.player_b) and pb=greatest(m.player_a,m.player_b);
  update bt4_private.ranked_matches set winner=p_winner,result_digest=p_digest,completed_at=now(),rated=false,capped=true,duration_ms=took_ms where id=p_match;
  return jsonb_build_object('state','applied','capped',true);
 end if;
 select rating into strict a from bt4_private.ranked_players where ruleset=m.ruleset and user_id=m.player_a for update;
 select rating into strict b from bt4_private.ranked_players where ruleset=m.ruleset and user_id=m.player_b for update;
 outcome:=case when p_winner=m.player_a then 1 when p_winner=m.player_b then 0 else 0.5 end;
 if m.forfeit_by is not null then
  gain:=case when p_winner=m.player_a then 16 else -16 end;
 else
  gain:=round(32*(outcome-1/(1+power(10::numeric,(b-a)::numeric/400))))::integer;
 end if;
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
 update bt4_private.ranked_matches set winner=p_winner,result_digest=p_digest,completed_at=now(),duration_ms=took_ms where id=p_match;
 return jsonb_build_object('state','applied');
end $$;
revoke all on function bt4_private.ranked_record_result(uuid,uuid,text) from public,anon,authenticated;
grant execute on function bt4_private.ranked_record_result(uuid,uuid,text) to service_role;

-- What the real durations look like, for calibrating a minimum later (nothing enforces one): percentiles of the matches that had both clients' heartbeats.
create or replace view bt4_private.ranked_duration_stats as
 select ruleset,count(*) matches,
  percentile_cont(0.01) within group(order by duration_ms) p01_ms,
  percentile_cont(0.05) within group(order by duration_ms) p05_ms,
  percentile_cont(0.50) within group(order by duration_ms) p50_ms,
  percentile_cont(0.95) within group(order by duration_ms) p95_ms
 from bt4_private.ranked_matches where duration_ms is not null and completed_at is not null and not coalesce(abandonment_void,false) and forfeit_by is null
 group by ruleset;
revoke all on bt4_private.ranked_duration_stats from public,anon,authenticated;

-- Run again after a rollback (migrations_down/): what the rollback archived comes back (the counters, the per-match flags, the switch). The archives stay until someone drops them.
do $restore$
begin
 if to_regclass('bt4_private.ranked_pair_day_archive') is not null then
  insert into bt4_private.ranked_pair_day select * from bt4_private.ranked_pair_day_archive on conflict do nothing;
 end if;
 if to_regclass('bt4_private.ranked_matches_meta_archive') is not null then
  update bt4_private.ranked_matches m set rated=a.rated,capped=a.capped,duration_ms=a.duration_ms
   from bt4_private.ranked_matches_meta_archive a where a.match_id=m.id;
 end if;
 if to_regclass('bt4_private.ranked_rollback_config') is not null then
  update bt4_private.ranked_config set pair_day_cap=coalesce((select v::smallint from bt4_private.ranked_rollback_config where k='pair_day_cap'),pair_day_cap) where singleton;
 end if;
end $restore$;
commit;
