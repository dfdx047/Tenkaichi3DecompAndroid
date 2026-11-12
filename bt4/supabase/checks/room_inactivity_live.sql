begin;
set local statement_timeout='30s';
update bt4_private.route_config set admission_open=true,admission_until=now()+interval '1 hour';
do $$
declare a uuid; mode_name text; args jsonb; reply jsonb; rid uuid; epoch_id integer;
begin
 select epoch into epoch_id from bt4_private.route_config where singleton;
 foreach mode_name in array array['duel','lobby','tournament','tag'] loop
  a:=gen_random_uuid();insert into auth.users(id) values(a);
  perform set_config('request.jwt.claim.sub',a::text,true);
  args:=jsonb_build_object('mode',mode_name,'wire','lease-'||replace(gen_random_uuid()::text,'-',''),
   'content','aaaaaaaaaaaaaaaa','rules','lease-test','name','Lease check','capacity',case when mode_name='duel' then 2 else 4 end,
   '_client',jsonb_build_object('version','0.1.39.5-R5.1','capability',1,'epoch',epoch_id));
  reply:=public.bt4_api('create',args);
  if reply->>'room' is null then raise exception 'ROOM_CREATE_FAILED: %',reply;end if;
  rid:=(reply->>'room')::uuid;
  if (select expires_at<clock_timestamp()+interval '29 minutes 50 seconds' from bt4_private.rooms where id=rid) then
   raise exception 'ROOM_LEASE_TOO_SHORT';end if;
  update bt4_private.rooms set expires_at=clock_timestamp()+interval '1 minute' where id=rid;
  reply:=public.bt4_api('lease','{}');
  if reply->>'room' is distinct from rid::text then raise exception 'LEASE_RENEW_FAILED: %',reply;end if;
  if (select expires_at<clock_timestamp()+interval '29 minutes 50 seconds' from bt4_private.rooms where id=rid) then
   raise exception 'RENEWED_LEASE_TOO_SHORT';end if;
  update bt4_private.rooms set expires_at=clock_timestamp()-interval '1 second' where id=rid;
  reply:=public.bt4_api('lease','{}');
  if reply->>'error' is null then raise exception 'EXPIRED_ROOM_REVIVED: %',reply;end if;
 end loop;
end $$;
rollback;
