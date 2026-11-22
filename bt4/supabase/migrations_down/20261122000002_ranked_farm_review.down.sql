-- ROLLBACK of 20261122000002_ranked_farm_review.sql: a read-only view, nothing to preserve.
begin;
drop view if exists bt4_private.ranked_farm_review;
commit;
