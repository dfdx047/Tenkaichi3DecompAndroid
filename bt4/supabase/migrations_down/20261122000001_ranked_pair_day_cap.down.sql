-- ROLLBACK of 20261122000001_ranked_pair_day_cap.sql. Nothing is lost: the pair counters, the per-match flags and the switch's value are copied to archive tables
-- (bt4_private.*_archive) BEFORE anything is dropped, and the next run of the migration restores them. Run 20261122000002_ranked_farm_review.down.sql FIRST (that view reads rated).
-- Ratings, wins, losses, history, medals and seasons are not touched by this script; matches that were capped keep their result (no history rows, as when they were settled).
begin;
set local lock_timeout = '2s';
drop view if exists bt4_private.ranked_farm_review;
drop view if exists bt4_private.ranked_duration_stats;

create table if not exists bt4_private.ranked_pair_day_archive (like bt4_private.ranked_pair_day);
insert into bt4_private.ranked_pair_day_archive select * from bt4_private.ranked_pair_day on conflict do nothing;
create table if not exists bt4_private.ranked_matches_meta_archive (
 match_id uuid primary key, rated boolean not null, capped boolean not null, duration_ms integer, saved_at timestamptz not null default now());
insert into bt4_private.ranked_matches_meta_archive(match_id,rated,capped,duration_ms)
 select id,rated,capped,duration_ms from bt4_private.ranked_matches where capped or not rated or duration_ms is not null
 on conflict(match_id) do update set rated=excluded.rated,capped=excluded.capped,duration_ms=excluded.duration_ms,saved_at=now();
create table if not exists bt4_private.ranked_rollback_config (k text primary key, v text not null, saved_at timestamptz not null default now());
insert into bt4_private.ranked_rollback_config(k,v) select 'pair_day_cap',pair_day_cap::text from bt4_private.ranked_config where singleton
 on conflict(k) do update set v=excluded.v,saved_at=now();
alter table bt4_private.ranked_pair_day_archive enable row level security;
alter table bt4_private.ranked_matches_meta_archive enable row level security;
alter table bt4_private.ranked_rollback_config enable row level security;
revoke all on bt4_private.ranked_pair_day_archive,bt4_private.ranked_matches_meta_archive,bt4_private.ranked_rollback_config from public,anon,authenticated;

-- the function as it was before the migration (20261108224635_ranked_abandonment.sql, verbatim)
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
 update bt4_private.ranked_matches set winner=p_winner,result_digest=p_digest,completed_at=now() where id=p_match;
 return jsonb_build_object('state','applied');
end $$;
revoke all on function bt4_private.ranked_record_result(uuid,uuid,text) from public,anon,authenticated;
grant execute on function bt4_private.ranked_record_result(uuid,uuid,text) to service_role;

drop table if exists bt4_private.ranked_pair_day;
alter table bt4_private.ranked_matches drop column if exists rated, drop column if exists capped, drop column if exists duration_ms;
alter table bt4_private.ranked_config drop column if exists pair_day_cap;
commit;
