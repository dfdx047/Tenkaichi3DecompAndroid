update bt4_private.ranked_config set enabled=true,authority_backend=1; update bt4_private.route_config set backend_id=1,admission_open=true;
select bt4_private.ranked_rollover();
drop table if exists bench_matches;
create table bench_users as select gen_random_uuid() id, g n from generate_series(1,60000) g;
insert into auth.users(id) select id from bench_users;
insert into bt4_private.ranked_players(ruleset,user_id,name) select 'solo',id,'P'||n from bench_users;
insert into bt4_private.ranked_profiles(season,ruleset,user_id,name,rating)
 select (bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date,'solo',id,'P'||n,1000 from bench_users;
create table bench_matches as select row_number() over() n, gen_random_uuid() id, a.id a, b.id b from
 (select id, row_number() over() r from bench_users where n%2=1) a join (select id, row_number() over() r from bench_users where n%2=0) b using(r);
insert into bt4_private.ranked_matches(id,season,ruleset,player_a,player_b)
 select id,(bt4_private.ranked_season_at(now()) at time zone 'America/Argentina/Buenos_Aires')::date,'solo',a,b from bench_matches;
create index on bench_matches(n);
analyze;
select count(*) matches from bench_matches;
