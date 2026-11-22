#!/bin/bash
# Concurrency proof of the pair's daily limit on a REAL Postgres 16 (Docker): 8 sessions settle 24 matches of the SAME pair at once.
#   ./cap_race.sh            exits 0 when exactly 4 battles scored with the global lock, and also WITHOUT it (the limit does not depend on that lock)
set -u
# (the per-account report budget is 30 a window: more matches than that would fail on the rate limit, not on the cap)
cd "$(dirname "$0")"; N=${N:-24}; W=${W:-8}; C=bt4race
docker rm -f $C >/dev/null 2>&1
docker run -d --name $C --cpus=2 -e POSTGRES_PASSWORD=pw postgres:16-alpine >/dev/null
for i in $(seq 1 40); do docker exec $C pg_isready -U postgres >/dev/null 2>&1 && break; sleep 1; done
sleep 2
P="docker exec -i $C psql -U postgres -q -v ON_ERROR_STOP=1"
$P < stubs.sql >/dev/null || exit 2
for f in ../../migrations/*.sql; do $P < $f >/dev/null 2>/tmp/cap_mig_err.txt || { echo "migration failed: $f"; cat /tmp/cap_mig_err.txt | head -3; exit 2; }; done
$P >/dev/null <<'SQL'
update bt4_private.ranked_config set enabled=true,authority_backend=1,pair_day_cap=4; update bt4_private.route_config set backend_id=1,admission_open=true;
select bt4_private.ranked_rollover();
SQL
setup() { # two users, N admitted matches of that pair, alternating who is player_a
 $P >/dev/null <<SQL
truncate bt4_private.ranked_reports, bt4_private.ranked_rating_history, bt4_private.ranked_pair_day, bt4_private.ranked_matches cascade;
delete from bt4_private.ranked_profiles; delete from bt4_private.ranked_players; delete from auth.users;
create temp table _u as select gen_random_uuid() id, g n from generate_series(1,2) g;
insert into auth.users(id) select id from _u;
insert into bt4_private.ranked_players(ruleset,user_id,name) select 'solo',id,'P'||n from _u;
insert into bt4_private.ranked_profiles(season,ruleset,user_id,name,rating) select (bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date,'solo',id,'P'||n,1000 from _u;
drop table if exists race_m; create table race_m as
 select g n, gen_random_uuid() id, (case when g%2=0 then (select id from _u where n=1) else (select id from _u where n=2) end) a,
        (case when g%2=0 then (select id from _u where n=2) else (select id from _u where n=1) end) b from generate_series(1,$N) g;
insert into bt4_private.ranked_matches(id,season,ruleset,player_a,player_b)
 select id,(bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date,'solo',a,b from race_m;
SQL
}
run() { # $1 label
 for w in $(seq 0 $((W-1))); do
  docker exec -i $C psql -U postgres -At -q -c "select format('select set_config(''request.jwt.claim.sub'',%L,false); select public.bt4_ranked_api(''report'',jsonb_build_object(''match'',%L,''outcome'',0,''digest'',repeat(''a'',64))); select set_config(''request.jwt.claim.sub'',%L,false); select public.bt4_ranked_api(''report'',jsonb_build_object(''match'',%L,''outcome'',0,''digest'',repeat(''a'',64)));', a, id, b, id) from race_m where n%$W=$w order by n" > /tmp/cap_w$w.sql
 done
 for w in $(seq 0 $((W-1))); do ( docker exec -i $C psql -U postgres -At -q < /tmp/cap_w$w.sql > /tmp/cap_w$w.out 2>/tmp/cap_w$w.err ) & done
 wait
 local dl=$(cat /tmp/cap_w*.err | grep -c "deadlock detected")
 local errs=$(cat /tmp/cap_w*.err | grep -c "ERROR")
 read rated capped open < <(docker exec $C psql -U postgres -tAc "select count(*) filter (where completed_at is not null and rated), count(*) filter (where capped), count(*) filter (where completed_at is null) from bt4_private.ranked_matches" | tr '|' ' ')
 read rsum hist pdr pdb < <(docker exec $C psql -U postgres -tAc "select (select sum(rating) from bt4_private.ranked_players), (select count(*) from bt4_private.ranked_rating_history), coalesce((select max(rated) from bt4_private.ranked_pair_day),0), coalesce((select max(battles) from bt4_private.ranked_pair_day),0)" | tr '|' ' ')
 echo "$1: rated=$rated capped=$capped not-completed=$open | ratings sum=$rsum history rows=$hist | counter rated=$pdr battles=$pdb | deadlocks=$dl errors=$errs"
 RATED=$rated; HIST=$hist; RSUM=$rsum
}
fail=0
setup; run "WITH the global lock (as is)"
[ "$RATED" = 4 ] && [ "$HIST" = 8 ] && [ "$RSUM" = 2000 ] || { echo "FAIL: the limit/points are wrong with the lock"; fail=1; }
# the same three functions without the global lock (a measurement variant: NOT a proposed change)
docker exec $C psql -U postgres -At -c "select pg_get_functiondef('bt4_private.ranked_record_result(uuid,uuid,text)'::regprocedure)||';'" -c "select pg_get_functiondef('public.bt4_ranked_api(text,jsonb)'::regprocedure)||';'" -c "select pg_get_functiondef('public.bt4_ranked_gameplay_before_solo(text,jsonb)'::regprocedure)||';'" | sed 's/perform pg_advisory_xact_lock(5454,4001);/-- no global lock (race test)/g' | $P >/dev/null
left=$(docker exec $C psql -U postgres -tAc "select count(*) from pg_proc where proname in ('ranked_record_result','bt4_ranked_api','bt4_ranked_gameplay_before_solo') and position('pg_advisory_xact_lock(5454,4001)' in prosrc)>0")
echo "functions still holding the global lock after the replacement: $left (must be 0)"; [ "$left" = 0 ] || { echo "FAIL: the lock was not removed for the race test"; fail=1; }
setup; run "WITHOUT the global lock (race test only)"
[ "$RATED" -le 4 ] && [ "$RSUM" = 2000 ] || { echo "FAIL: the cap or the conservation of points broke without the lock"; fail=1; }
docker rm -f $C >/dev/null 2>&1
[ $fail = 0 ] && echo "CAP RACE: OK" || echo "CAP RACE: FAILED"; exit $fail
