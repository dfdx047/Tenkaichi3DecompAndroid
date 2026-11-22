truncate bt4_private.ranked_reports, bt4_private.ranked_rating_history;
update bt4_private.ranked_matches set completed_at=null, winner=null, result_digest=null, disputed=false where id in (select id from bench_matches);
update bt4_private.ranked_players set rating=1000,wins=0,losses=0;
update bt4_private.ranked_profiles set rating=1000,wins=0,losses=0;
vacuum analyze bt4_private.ranked_matches, bt4_private.ranked_players, bt4_private.ranked_profiles;
