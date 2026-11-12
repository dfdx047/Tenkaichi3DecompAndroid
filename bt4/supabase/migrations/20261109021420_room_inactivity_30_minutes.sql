-- Room lease follows communication activity, not total tournament duration.
begin;
alter table bt4_private.rooms alter column expires_at set default clock_timestamp()+interval '30 minutes';
create or replace function public.bt4_room_lease(p_joinable boolean default null) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); rid uuid;
begin
 if who is null then raise exception 'AUTH_REQUIRED' using errcode='42501';end if;
 update bt4_private.rooms set expires_at=clock_timestamp()+interval '30 minutes',joinable=coalesce(p_joinable,joinable)
  where owner=who and expires_at>clock_timestamp() returning id into rid;
 if rid is null then raise exception 'ROOM_NOT_FOUND';end if;
 return bt4_private.descriptor(rid,who);
end $$;
revoke all on function public.bt4_room_lease(boolean) from public,anon,authenticated;
-- Existing, still-live rooms receive the same grace; expired rooms stay expired.
update bt4_private.rooms set expires_at=greatest(expires_at,clock_timestamp()+interval '30 minutes')
 where expires_at>clock_timestamp();
notify pgrst,'reload schema';
commit;
