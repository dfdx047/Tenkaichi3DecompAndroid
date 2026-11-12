begin;
create table if not exists bt4_private.online_presence (
  user_id uuid primary key references auth.users(id) on delete cascade,
  expires_at timestamptz not null
);
create index if not exists online_presence_expiry on bt4_private.online_presence(expires_at);
alter table bt4_private.online_presence enable row level security;
revoke all on bt4_private.online_presence from public,anon,authenticated;

create or replace function bt4_private.presence(p_active boolean) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; total bigint;
begin
  who:=bt4_private.budget('presence',6);
  if p_active is null then raise exception 'BAD_REQUEST'; end if;
  delete from bt4_private.online_presence where expires_at<=clock_timestamp();
  if p_active then
    insert into bt4_private.online_presence values(who,clock_timestamp()+interval '6 minutes')
    on conflict(user_id) do update set expires_at=excluded.expires_at;
  else
    delete from bt4_private.online_presence where user_id=who;
  end if;
  select count(*) into total from bt4_private.online_presence where expires_at>clock_timestamp();
  return jsonb_build_object('version',1,'online',total,'refresh_seconds',300);
end $$;
revoke all on function bt4_private.presence(boolean) from public,anon,authenticated;

/* Preserve every existing room/voice operation and its validation/budget.
 * The counter uses one bounded HTTP request per five minutes, no broadcasts. */
do $migration$
begin
  if to_regprocedure('public.bt4_api_rooms_v4(text,jsonb)') is null then
    alter function public.bt4_api(text,jsonb) rename to bt4_api_rooms_v4;
  end if;
end $migration$;
revoke all on function public.bt4_api_rooms_v4(text,jsonb) from public,anon,authenticated;
create or replace function public.bt4_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
begin
  if p_op is distinct from 'presence' then return public.bt4_api_rooms_v4(p_op,p_args); end if;
  if p_args is null or jsonb_typeof(p_args)<>'object' or p_args-'active'<>'{}'::jsonb
     or jsonb_typeof(p_args->'active') is distinct from 'boolean' then
    return jsonb_build_object('version',1,'error','BAD_REQUEST');
  end if;
  begin
    return bt4_private.presence((p_args->>'active')::boolean);
  exception when others then
    return jsonb_build_object('version',1,'error',case when SQLERRM in ('AUTH_REQUIRED','RATE_LIMITED') then SQLERRM else 'INTERNAL_ERROR' end);
  end;
end $$;
revoke all on function public.bt4_api(text,jsonb) from public,anon;
grant execute on function public.bt4_api(text,jsonb) to authenticated;
commit;
