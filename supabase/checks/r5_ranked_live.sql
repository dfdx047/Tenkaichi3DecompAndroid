-- Exercise the installed production API in one rollback-only transaction.
-- Random identities and a distinct wire/content prevent matching public users.
begin;
set local statement_timeout='30s';
do $$
declare a uuid:=gen_random_uuid(); b uuid:=gen_random_uuid(); c uuid:=gen_random_uuid(); d uuid:=gen_random_uuid();
 profile jsonb; result jsonb; reply jsonb; paired jsonb; format text; first_user uuid; second_user uuid;
 epoch integer; wire text:='r5check-'||replace(gen_random_uuid()::text,'-','');
begin
 if not exists(select 1 from bt4_private.route_config where singleton and backend_id=1 and admission_open
  and (admission_until is null or admission_until>clock_timestamp())) then raise exception 'SECONDARY_ADMISSION_CLOSED'; end if;
 select r.epoch into epoch from bt4_private.route_config r where singleton;
 update bt4_private.ranked_config set enabled=true where singleton and authority_backend=1;
 insert into auth.users(id) values(a),(b),(c),(d);
 foreach format in array array['solo','teams'] loop
  first_user:=case format when 'solo' then a else c end;
  second_user:=case format when 'solo' then b else d end;
  profile:=jsonb_build_object('mode','duel','wire',wire,'content','aaaaaaaaaaaaaaaa','rules',case format when 'solo' then 'S' else 'T' end,
   'ruleset',format,'name','R5 server check','capacity',2,'_client',jsonb_build_object('version','0.1.39.5-R5','capability',1,'epoch',epoch));
  perform set_config('request.jwt.claim.sub',first_user::text,true);
  reply:=public.bt4_ranked_api('queue',profile);
  if reply->>'state' is distinct from 'waiting' then raise exception 'FIRST_QUEUE_FAILED: %',reply;end if;
  perform set_config('request.jwt.claim.sub',second_user::text,true);
  paired:=public.bt4_ranked_api('queue',profile);
  if paired->>'ranked' is distinct from 'true' or paired->>'self' is distinct from '2' then raise exception 'PAIRING_FAILED: %',paired;end if;
  result:=jsonb_build_object('match',paired->>'generation','outcome',0,'digest',repeat('b',64));
  perform set_config('request.jwt.claim.sub',first_user::text,true);
  reply:=public.bt4_ranked_api('report',result);
  if reply->>'state' is distinct from 'waiting' then raise exception 'FIRST_REPORT_FAILED: %',reply;end if;
  perform set_config('request.jwt.claim.sub',second_user::text,true);
  reply:=public.bt4_ranked_api('report',result);
  if reply->>'state' is distinct from 'applied' or reply->>'rating' is distinct from '984' then raise exception 'RATING_FAILED: %',reply;end if;
  perform set_config('request.jwt.claim.sub',first_user::text,true);
  reply:=public.bt4_ranked_api('report',result);
  if reply->>'state' is distinct from 'already_applied' or reply->>'rating' is distinct from '1016' then raise exception 'RETRY_FAILED: %',reply;end if;
  reply:=public.bt4_ranked_api('report',result||'{"outcome":1}'::jsonb);
  if reply->>'error' is distinct from 'RESULT_CONFLICT' then raise exception 'CONFLICT_FAILED: %',reply;end if;
 end loop;
 if (select count(*) from bt4_private.ranked_rating_history where user_id in(a,b,c,d))<>4 then raise exception 'HISTORY_COUNT_FAILED';end if;
end $$;
rollback;
