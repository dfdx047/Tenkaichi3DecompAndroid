-- Local fixture. Run after installing the schema with the two legacy profiles
-- from test-most-wanted-db.mjs. All test writes are rolled back.
begin;
set local statement_timeout='20s';
do $$
declare a uuid:='10000000-0000-4000-8000-000000000001';
 b uuid:='10000000-0000-4000-8000-000000000002'; c uuid:=gen_random_uuid();
 args jsonb; reply jsonb; match_id uuid; epoch_id integer;
begin
 insert into auth.users(id) values(c);
 if not exists(select 1 from bt4_private.online_nicknames where user_id=a and name='Nova') or
    exists(select 1 from bt4_private.online_nicknames where user_id=b) then raise exception 'BACKFILL_OWNER'; end if;
 if has_table_privilege('authenticated','bt4_private.online_nicknames','select') then raise exception 'PUBLIC_NICKNAME_TABLE'; end if;
 if (select schedule from cron.job where name='bt4-ranked-hourly') is distinct from '*/10 * * * *' or
    (select schedule from cron.job where name='bt4-ranked-weekly') is distinct from '59 2 * * 1' then
  raise exception 'RANKED_SCHEDULE'; end if;
 update bt4_private.ranked_config set enabled=true;
 select epoch into epoch_id from bt4_private.route_config where singleton;
 args:=jsonb_build_object('mode','duel','ruleset','teams','rules','T','name','Fixture',
  'wire','wanted-fixture','content','ee003456789abcde','capacity',2,
  '_client',jsonb_build_object('version','0.1.39.5-R5.2','capability',1,'epoch',epoch_id));
 perform set_config('request.jwt.claim.sub',a::text,true);
 reply:=public.bt4_api('nickname_get','{}');
 if reply->>'name' is distinct from 'Nova' then raise exception 'EXISTING_NAME: %',reply; end if;
 reply:=public.bt4_ranked_api('queue',args);
 if reply->>'state' is distinct from 'waiting' then raise exception 'LEGACY_WAIT: %',reply; end if;
 perform set_config('request.jwt.claim.sub',b::text,true);
 reply:=public.bt4_api('nickname_get','{}');
 if reply->>'state' is distinct from 'optional' then raise exception 'STAGED_COLLISION: %',reply; end if;
 reply:=public.bt4_api('nickname_set','{"name":" NOVA "}');
 if reply->>'error' is distinct from 'NICKNAME_TAKEN' then raise exception 'CASE_COLLISION: %',reply; end if;
 reply:=public.bt4_api('nickname_set','{"name":" Blue Phoenix "}');
 if reply->>'name' is distinct from 'Blue Phoenix' then raise exception 'RESERVE_NAME: %',reply; end if;
 reply:=public.bt4_ranked_api('queue',args);
 if reply->>'ranked' is distinct from 'true' then raise exception 'LEGACY_PAIR: %',reply; end if;
 match_id:=(reply->>'generation')::uuid;

 update bt4_private.online_nickname_config set enforce=true,solo_only=true;
 reply:=public.bt4_ranked_api('queue',args);
 if reply->>'error' is distinct from 'BAD_RULESET' then raise exception 'TEAM_QUEUE: %',reply; end if;
 reply:=public.bt4_ranked_api('report',jsonb_build_object('match',match_id,'outcome',1,'digest',repeat('e',64)));
 if reply->>'state' is distinct from 'waiting' then raise exception 'LEGACY_REPORT_WAIT: %',reply; end if;
 perform set_config('request.jwt.claim.sub',a::text,true);
 reply:=public.bt4_ranked_api('report',jsonb_build_object('match',match_id,'outcome',1,'digest',repeat('e',64)));
 if reply->>'state' is distinct from 'applied' then raise exception 'LEGACY_DRAIN: %',reply; end if;
 perform public.bt4_api('leave','{}');
 perform set_config('request.jwt.claim.sub',b::text,true); perform public.bt4_api('leave','{}');
 if (select rating from bt4_private.ranked_players where user_id=b and ruleset='solo') is distinct from 1502 then
  raise exception 'SOLO_PROGRESS_LOST'; end if;

 args:=args||'{"ruleset":"solo","rules":"S"}';
 perform set_config('request.jwt.claim.sub',c::text,true);
 reply:=public.bt4_api('nickname_get','{}');
 if reply->>'state' is distinct from 'required' then raise exception 'REQUIRED_PROMPT: %',reply; end if;
 reply:=public.bt4_ranked_api('queue',args);
 if reply->>'error' is distinct from 'NICKNAME_REQUIRED' then raise exception 'NICKNAME_GATE: %',reply; end if;
 perform set_config('request.jwt.claim.sub',a::text,true);
 reply:=public.bt4_ranked_api('queue',args);
 if reply->>'state' is distinct from 'waiting' then raise exception 'SOLO_WAIT: %',reply; end if;
 perform set_config('request.jwt.claim.sub',b::text,true);
 reply:=public.bt4_ranked_api('queue',args);
 if reply->>'ranked' is distinct from 'true' or reply#>>'{members,1,name}' is distinct from 'Blue Phoenix' then
  raise exception 'SOLO_CANONICAL_NAME: %',reply; end if;
 match_id:=(reply->>'generation')::uuid;
 perform public.bt4_ranked_api('report',jsonb_build_object('match',match_id,'outcome',1,'digest',repeat('d',64)));
 perform set_config('request.jwt.claim.sub',a::text,true);
 reply:=public.bt4_ranked_api('report',jsonb_build_object('match',match_id,'outcome',1,'digest',repeat('d',64)));
 if reply->>'state' is distinct from 'applied' then raise exception 'SOLO_RESULT: %',reply; end if;
 perform bt4_private.ranked_refresh_top();
 reply:=public.bt4_ranked_api('top','{"ruleset":"solo"}');
 if jsonb_array_length(reply->'entries')<>2 or reply#>>'{entries,0,name}' is distinct from 'Nova' or
    reply#>>'{entries,1,name}' is distinct from 'Blue Phoenix' then raise exception 'TOP_ORDER: %',reply; end if;

 update bt4_private.route_config set backend_id=0;
 perform set_config('request.jwt.claim.sub',c::text,true);
 reply:=public.bt4_api('nickname_get','{}');
 if reply->>'state' is distinct from 'disabled' then raise exception 'FAILOVER_PROFILE: %',reply; end if;
 reply:=public.bt4_api('create',args-'ruleset');
 if reply ? 'error' or not(reply ? 'generation') then raise exception 'FAILOVER_CASUAL: %',reply; end if;
end $$;
rollback;
