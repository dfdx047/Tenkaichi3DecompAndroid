# Throughput of the ranked `report` path on a real Postgres (and what the global advisory lock costs)

    docker run -d --name bt4pg --cpus=2 -e POSTGRES_PASSWORD=pw postgres:16-alpine
    docker exec -i bt4pg psql -U postgres -q < stubs.sql                      # roles/auth/realtime/cron stand-ins (Supabase provides them)
    for f in ../../migrations/*.sql; do docker exec -i bt4pg psql -U postgres -q -v ON_ERROR_STOP=1 < $f; done
    docker exec -i bt4pg psql -U postgres -q < setup.sql                      # 60 000 users, 30 000 admitted matches
    docker cp report.pgb bt4pg:/tmp/
    docker exec -i bt4pg psql -U postgres -q < reset.sql                      # before EACH run: matches back to "not scored", ratings 1000
    docker exec bt4pg pgbench -U postgres -n -f /tmp/report.pgb -c 8 -j 8 -T 12 postgres

One pgbench transaction = both players' reports of one match (two SQL transactions). To measure without the global lock, `create or replace` the three functions that call
`pg_advisory_xact_lock(5454,4001)` (`bt4_private.ranked_record_result`, `public.bt4_ranked_api`, `public.bt4_ranked_gameplay_before_solo`) with the call removed: for MEASUREMENT only (two matches
with the same players in opposite order can then deadlock in ranked_record_result's row locks). Results and limits: docs/AUDITORIA-ONLINE-PARTIDA.md.

## Concurrency proof of the pair's daily limit

    ./cap_race.sh      # Postgres 16 in Docker; 8 sessions settle 24 matches of ONE pair at once; exit 0 = exactly 4 scored, points conserved, with and without the global lock

Two runs: with the lock and without it (the three functions that call `pg_advisory_xact_lock(5454,4001)` recreated without the call: a race-test variant, not a proposal), both ended with 4 rated,
20 capped, history rows 8, ratings summing 2000, no deadlock and no error. That the limit holds without the lock shows it rests on the counter row's upsert (`ranked_pair_day`), not on the lock.
It does NOT prove that removing the lock is safe: a deadlock between settlements with opposite player order was not reproduced in these runs, which is not evidence that it cannot happen.
