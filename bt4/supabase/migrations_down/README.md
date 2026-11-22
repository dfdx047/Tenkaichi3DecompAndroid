# Rollbacks of the ranked migrations

Not applied by the Supabase CLI (they are outside `migrations/`). Run in the REVERSE order of the migrations, each in its own transaction:

    psql … -f migrations_down/20261122000002_ranked_farm_review.down.sql
    psql … -f migrations_down/20261122000001_ranked_pair_day_cap.down.sql

Nothing is lost: the pair counters, the per-match flags (`rated`, `capped`, `duration_ms`) and the value of `ranked_config.pair_day_cap` are copied to
`bt4_private.ranked_pair_day_archive`, `ranked_matches_meta_archive` and `ranked_rollback_config` before anything is dropped, and running the migration again restores them
(the archives stay until someone drops them). Ratings, wins, losses, history, medals and seasons are never touched. Battles settled while the rollback is in place are not
counted by the pair counter (the old function has none): after a long rollback, re-enable the switch on a quiet day. Tested: `supabase/tests/pglite/rollback.test.mjs`
(a migration that fails half way leaves the database as it was; up, rollback, up again). PGlite is Postgres compiled to WebAssembly: the same SQL, but not the hosted project.
