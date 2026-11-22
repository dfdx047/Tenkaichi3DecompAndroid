# Ranked SQL checks on an in-process Postgres

    cd supabase/tests/pglite && npm install && npm test

Applies all 16 migrations of `supabase/migrations` and exercises the real `bt4_ranked_api` (report, heartbeat, forfeit) as different users:
one scoring per match, idempotent repeats, disputes, strangers, malformed input, forfeit-after-result, quitting a match that never started,
the 45 s abandonment rule, and a timeout that cannot override a contradicting report. This is NOT a load or concurrency test (one connection).
