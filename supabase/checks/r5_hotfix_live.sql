-- Isolated identities and wire format, always rolled back, including configuration.
begin;
set local statement_timeout='30s';
update bt4_private.route_config set backend_id=1,admission_open=true,admission_until=now()+interval '1 hour';
update bt4_private.ranked_config set enabled=true,authority_backend=1;
do $$
declare a uuid; b uuid; outsider uuid:=gen_random_uuid(); args jsonb; paired jsonb; reply jsonb;
 generation_id uuid; format text; scenario integer; epoch_id integer; expected_state text;
 wire text:='hf-'||replace(gen_random_uuid()::text,'-','');
begin
 insert into auth.users(id) values(outsider);
 select epoch into epoch_id from bt4_private.route_config where singleton;
 foreach format in array array['solo','teams'] loop
  for scenario in 1..14 loop
   a:=gen_random_uuid();b:=gen_random_uuid();insert into auth.users(id) values(a),(b);
   args:=jsonb_build_object('mode','duel','wire',wire,'content','aaaaaaaaaaaaaaaa',
    'rules',case format when 'solo' then 'S' else 'T' end,'ruleset',format,'name','Hotfix check','capacity',2,
    '_client',jsonb_build_object('version','0.1.39.5-R5.1','capability',1,'epoch',epoch_id));
   perform set_config('request.jwt.claim.sub',a::text,true);
   reply:=public.bt4_ranked_api('queue',args);
   if reply->>'state' is distinct from 'waiting' then raise exception 'QUEUE_A: %',reply;end if;
   perform set_config('request.jwt.claim.sub',b::text,true);
   paired:=public.bt4_ranked_api('queue',args);
   if paired->>'ranked' is distinct from 'true' then raise exception 'QUEUE_B: %',paired;end if;
   generation_id:=(paired->>'generation')::uuid;
   args:=jsonb_build_object('match',generation_id);
   if scenario<>2 then
    perform set_config('request.jwt.claim.sub',a::text,true);perform public.bt4_ranked_api('heartbeat',args);
    perform set_config('request.jwt.claim.sub',b::text,true);perform public.bt4_ranked_api('heartbeat',args);
   end if;
   if scenario=1 then
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('forfeit',args||'{"reason":"local_quit"}'::jsonb);expected_state:='applied';
    if reply->>'delta' is distinct from '-16' then raise exception 'FORFEIT_LOSS: %',reply;end if;
   elsif scenario=2 then
    -- Admission alone is not a started battle, even when one side leaves.
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('forfeit',args);expected_state:='waiting';
   elsif scenario in (3,7) then
    if scenario=7 then
     perform set_config('request.jwt.claim.sub',a::text,true);
     perform public.bt4_ranked_api('report',args||jsonb_build_object('outcome',0,'digest',repeat('c',64)));
    end if;
    update bt4_private.ranked_matches set seen_b=clock_timestamp()-interval '50 seconds' where id=generation_id;
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('heartbeat',args||'{"peer_gone":true}'::jsonb);expected_state:='applied';
    if reply->>'delta' is distinct from '16' then raise exception 'TIMEOUT_GAIN: %',reply;end if;
   elsif scenario=4 then
    update bt4_private.ranked_matches set seen_a=clock_timestamp()-interval '50 seconds',seen_b=clock_timestamp()-interval '50 seconds' where id=generation_id;
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('heartbeat',args);expected_state:='expired';
   elsif scenario=5 then
    reply:=public.bt4_ranked_api('heartbeat',args);expected_state:='waiting';
   elsif scenario=6 then
    perform set_config('request.jwt.claim.sub',outsider::text,true);
    reply:=public.bt4_ranked_api('forfeit',args);
    if reply->>'error' is distinct from 'NOT_A_PARTICIPANT' then raise exception 'UNAUTHORIZED_FORFEIT: %',reply;end if;
    expected_state:=null;
   elsif scenario=8 then
    -- The standard penalty does not vary with the participants' Elo gap.
    update bt4_private.ranked_players set rating=case when user_id=a then 800 else 1600 end where ruleset=format and user_id in(a,b);
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('forfeit',args||'{"reason":"local_quit"}'::jsonb);expected_state:='applied';
    if reply->>'delta' is distinct from '-16' then raise exception 'FIXED_PENALTY: %',reply;end if;
   elsif scenario=9 then
    -- Both old clients see the same pause abort. The first to post cannot take the other's points.
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('forfeit',args);
    if reply->>'state' is distinct from 'waiting' then raise exception 'EARLY_LEGACY_PENALTY: %',reply;end if;
    perform set_config('request.jwt.claim.sub',b::text,true);
    reply:=public.bt4_ranked_api('forfeit',args);expected_state:='expired';
   elsif scenario=10 then
    -- Continued fighting while the opponent's HTTP requests fail is not abandonment.
    update bt4_private.ranked_matches set seen_b=clock_timestamp()-interval '50 seconds' where id=generation_id;
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('heartbeat',args);expected_state:='waiting';
   elsif scenario=11 then
    -- Old-client hard quit, followed by the surviving client's authenticated room departure.
    update bt4_private.ranked_matches set seen_b=clock_timestamp()-interval '50 seconds' where id=generation_id;
    perform set_config('request.jwt.claim.sub',a::text,true);
    perform public.bt4_api('leave',jsonb_build_object('generation',generation_id));
    reply:=public.bt4_ranked_api('heartbeat',args);expected_state:='applied';
   elsif scenario=12 then
    -- A false legacy self-claim cannot beat the new client's authenticated local confirmation.
    perform set_config('request.jwt.claim.sub',a::text,true);
    perform public.bt4_ranked_api('forfeit',args);
    perform set_config('request.jwt.claim.sub',b::text,true);
    reply:=public.bt4_ranked_api('forfeit',args||'{"reason":"local_quit"}'::jsonb);expected_state:='applied';
   elsif scenario=13 then
    -- A single older concession is corroborated by the other player's subsequent heartbeat.
    perform set_config('request.jwt.claim.sub',a::text,true);
    perform public.bt4_ranked_api('forfeit',args);
    update bt4_private.ranked_matches set quit_a=clock_timestamp()-interval '20 seconds' where id=generation_id;
    perform set_config('request.jwt.claim.sub',b::text,true);
    reply:=public.bt4_ranked_api('heartbeat',args);expected_state:='applied';
   else
    -- The updated game explicitly declares an inconclusive exit, even after leaving the lobby.
    update bt4_private.ranked_matches set seen_b=clock_timestamp()-interval '50 seconds' where id=generation_id;
    perform set_config('request.jwt.claim.sub',a::text,true);
    perform public.bt4_api('leave',jsonb_build_object('generation',generation_id));
    reply:=public.bt4_ranked_api('heartbeat',args||'{"peer_gone":false}'::jsonb);expected_state:='waiting';
   end if;
   if expected_state is not null and reply->>'state' is distinct from expected_state then
    raise exception 'SCENARIO %/% FAILED: %',format,scenario,reply;end if;
   if expected_state='applied' then
    perform set_config('request.jwt.claim.sub',b::text,true);
    reply:=public.bt4_ranked_api('heartbeat',args);
    if reply->>'state' is distinct from 'already_applied' then raise exception 'RETRY_FAILED: %',reply;end if;
    if (reply->>'delta')::integer is distinct from (case when scenario in (3,7,11,12) then -16 else 16 end) then
     raise exception 'OTHER_PLAYER_DELTA %/%: %',format,scenario,reply;
    end if;
    perform set_config('request.jwt.claim.sub',a::text,true);
    reply:=public.bt4_ranked_api('forfeit',args);
    if reply->>'state' is distinct from 'already_applied' or
     (reply->>'delta')::integer is distinct from (case when scenario in (3,7,11,12) then 16 else -16 end) then
     raise exception 'FORFEIT_RETRY %/%: %',format,scenario,reply;
    end if;
    if (select count(*) from bt4_private.ranked_rating_history where match_id=generation_id)<>2 then raise exception 'DUPLICATE_SCORE';end if;
   elsif exists(select 1 from bt4_private.ranked_rating_history where match_id=generation_id) then raise exception 'FALSE_PENALTY';
   end if;
   perform set_config('request.jwt.claim.sub',a::text,true);perform public.bt4_api('leave','{}');
   perform set_config('request.jwt.claim.sub',b::text,true);perform public.bt4_api('leave','{}');
  end loop;
 end loop;
 -- Missed Tag Team SDP can be recovered only by its admitted recipient.
 a:=gen_random_uuid();b:=gen_random_uuid();insert into auth.users(id) values(a),(b);
 args:=jsonb_build_object('mode','tag','wire',wire,'content','bbbbbbbbbbbbbbbb','rules','tag-114','name','Recovery check','capacity',4,
  '_client',jsonb_build_object('version','0.1.39.5-R5.1','capability',1,'epoch',epoch_id));
 perform set_config('request.jwt.claim.sub',a::text,true);paired:=public.bt4_api('create',args);
 perform set_config('request.jwt.claim.sub',b::text,true);reply:=public.bt4_api('join',args||jsonb_build_object('code',paired->>'code'));
 if reply->>'room' is distinct from paired->>'room' then raise exception 'RECOVERY_JOIN: %',reply;end if;
 perform set_config('request.jwt.claim.sub',a::text,true);
 args:=jsonb_build_object('generation',paired->>'generation','to',2,'channel','tag','kind','offer','body',jsonb_build_object('sdp','v=0'));
 reply:=public.bt4_api('signal',args);
 if reply->>'state' is distinct from 'sent' then raise exception 'SIGNAL_SEND: %',reply;end if;
 perform public.bt4_api('signal',args); -- retries are deduplicated in retained storage
 args:=jsonb_build_object('generation',paired->>'generation','after',0);
 perform set_config('request.jwt.claim.sub',b::text,true);reply:=public.bt4_api('get_signals',args);
 if reply#>>'{signals,0,body,sdp}' is distinct from 'v=0' or octet_length(reply::text)>24576 then raise exception 'RECOVERY_READ: %',reply;end if;
 reply:=public.bt4_api('get_signals',args||jsonb_build_object('after',reply->'cursor'));
 if reply->'signals' is distinct from '[]'::jsonb then raise exception 'RECOVERY_CURSOR: %',reply;end if;
 perform set_config('request.jwt.claim.sub',a::text,true);reply:=public.bt4_api('get_signals',args);
 if reply->'signals' is distinct from '[]'::jsonb then raise exception 'SENDER_READ_RECIPIENT_PACKET';end if;
 perform set_config('request.jwt.claim.sub',outsider::text,true);reply:=public.bt4_api('get_signals',args);
 if reply->>'error' is distinct from 'NOT_A_MEMBER' then raise exception 'RECOVERY_MEMBERSHIP: %',reply;end if;
end $$;
rollback;
