begin;
-- Additive and initially permissive. Publication/activation is a separate step.
create table if not exists bt4_private.route_config (
  singleton boolean primary key default true check(singleton),
  backend_id smallint not null default 0 check(backend_id in (0,1)),
  epoch bigint not null default 1 check(epoch between 1 and 2147483647),
  admission_open boolean not null default true,
  admission_until timestamptz,
  require_update boolean not null default false,
  minimum_client text not null default '0.1.39.5-R4',
  minimum_capability integer not null default 1 check(minimum_capability between 1 and 100)
);
insert into bt4_private.route_config(singleton) values(true) on conflict do nothing;
alter table bt4_private.route_config enable row level security;
revoke all on bt4_private.route_config from public,anon,authenticated;
alter table bt4_private.rooms add column if not exists route_epoch bigint not null default 1;
alter table bt4_private.rooms drop constraint if exists rooms_code_check;
alter table bt4_private.rooms add constraint rooms_code_check
  check(code ~ '^([A-HJ-NP-Z2-9]{6}|[01][A-HJ-NP-Z2-9]{5})$');

create or replace function bt4_private.client_version(v text) returns integer[]
language plpgsql immutable set search_path='' as $$
declare parts text[]; answer integer[];
begin
  if v is null or length(v)>40 then return null; end if;
  parts:=regexp_match(v,'^([0-9]{1,4})\.([0-9]{1,4})\.([0-9]{1,4})(?:\.([0-9]{1,4}))?-R([0-9]{1,4})(?:\.([0-9]{1,4}))?$');
  if parts is null then return null; end if;
  answer:=array[parts[1]::integer,parts[2]::integer,parts[3]::integer,
                coalesce(parts[4],'0')::integer,parts[5]::integer,coalesce(parts[6],'0')::integer];
  return answer;
end $$;
revoke all on function bt4_private.client_version(text) from public,anon,authenticated;
-- A malformed administrative minimum must fail the transaction, not turn a
-- NULL version comparison into an admission bypass.
alter table bt4_private.route_config drop constraint if exists route_minimum_version_check;
alter table bt4_private.route_config add constraint route_minimum_version_check
  check(bt4_private.client_version(minimum_client) is not null);

do $migration$
begin
  if to_regprocedure('public.bt4_api_before_route_gate(text,jsonb)') is null then
    alter function public.bt4_api(text,jsonb) rename to bt4_api_before_route_gate;
  end if;
end $migration$;
revoke all on function public.bt4_api_before_route_gate(text,jsonb) from public,anon,authenticated;

create or replace function public.bt4_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare cfg bt4_private.route_config%rowtype; meta jsonb; version integer[]; capability integer; epoch bigint;
begin
  if auth.uid() is null then return jsonb_build_object('version',1,'error','AUTH_REQUIRED'); end if;
  if p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>19000 then
    return jsonb_build_object('version',1,'error','BAD_REQUEST');
  end if;
  meta:=p_args->'_client';
  -- Gate admission only. Established rooms may drain, signal, renew and leave.
  if p_op in ('create','join','queue','list') then
    select * into cfg from bt4_private.route_config where singleton;
    if not found then return jsonb_build_object('version',1,'error','SIGNALING_UNAVAILABLE'); end if;
    if cfg.require_update then
      if jsonb_typeof(meta) is distinct from 'object' or octet_length(meta::text)>512 then
        return jsonb_build_object('version',1,'error','UPDATE_REQUIRED','minimum',cfg.minimum_client);
      end if;
      version:=bt4_private.client_version(meta->>'version');
      if version is null or version<bt4_private.client_version(cfg.minimum_client)
         or coalesce(meta->>'capability','') !~ '^[0-9]{1,3}$' then
        return jsonb_build_object('version',1,'error','UPDATE_REQUIRED','minimum',cfg.minimum_client);
      end if;
      capability:=(meta->>'capability')::integer;
      if capability<cfg.minimum_capability then
        return jsonb_build_object('version',1,'error','UPDATE_REQUIRED','minimum',cfg.minimum_client);
      end if;
    end if;
    if not cfg.admission_open or (cfg.admission_until is not null and cfg.admission_until<=clock_timestamp()) then return jsonb_build_object('version',1,'error','ROUTE_CHANGED','epoch',cfg.epoch); end if;
    if jsonb_typeof(meta)='object' then
      if coalesce(meta->>'epoch','') !~ '^[0-9]{1,10}$' then return jsonb_build_object('version',1,'error','BAD_REQUEST'); end if;
      epoch:=(meta->>'epoch')::bigint;
      if epoch<>cfg.epoch then return jsonb_build_object('version',1,'error','ROUTE_CHANGED','epoch',cfg.epoch); end if;
    end if;
    if p_op='join' and exists(select 1 from bt4_private.rooms where code=upper(p_args->>'code') and route_epoch<>cfg.epoch) then
      return jsonb_build_object('version',1,'error','ROOM_ROUTE_EXPIRED');
    end if;
  end if;
  return public.bt4_api_before_route_gate(p_op,p_args-'_client');
end $$;
revoke all on function public.bt4_api(text,jsonb) from public,anon;
grant execute on function public.bt4_api(text,jsonb) to authenticated;

-- Bounded monitoring endpoint: no identities, rooms, history, tokens or writes.
create or replace function public.bt4_route_health() returns jsonb
language sql stable security definer set search_path='' as $$
  select jsonb_build_object('schema',1,'epoch',epoch,'admission_open',admission_open,
                           'require_update',require_update,'minimum',minimum_client,'backend_id',backend_id,
                           'admission_until',admission_until)
  from bt4_private.route_config where singleton
$$;
revoke all on function public.bt4_route_health() from public;
grant execute on function public.bt4_route_health() to anon,authenticated;
create or replace function bt4_private.new_room(who uuid,p_mode text,p_wire text,p_content text,p_rules text,p_name text,p_capacity integer,p_listed boolean) returns uuid
language plpgsql security definer set search_path='' as $$
declare rid uuid; candidate text; bytes bytea; alphabet constant text:='ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; i integer; cfg bt4_private.route_config%rowtype;
begin
  select * into strict cfg from bt4_private.route_config where singleton;
  if p_capacity is null or p_capacity not between 2 and 16 or (p_mode='tag' and p_capacity>10) then
    raise exception 'BAD_CAPACITY' using errcode='22023';
  end if;
  if exists(select 1 from bt4_private.members where user_id=who) then
    raise exception 'ALREADY_IN_ROOM' using errcode='23505';
  end if;
  for attempt in 1..10 loop
    if cfg.backend_id=0 and not cfg.require_update then
      bytes:=extensions.gen_random_bytes(6); candidate:='';
      for i in 0..5 loop candidate:=candidate||substr(alphabet,(get_byte(bytes,i)%32)+1,1); end loop;
    else
      bytes:=extensions.gen_random_bytes(5); candidate:=case cfg.backend_id when 0 then '1' else '0' end;
      for i in 0..4 loop candidate:=candidate||substr(alphabet,(get_byte(bytes,i)%32)+1,1); end loop;
    end if;
    begin
      insert into bt4_private.rooms(code,owner,mode,wire,content,rules_key,capacity,listed,route_epoch)
        values(candidate,who,p_mode,p_wire,p_content,p_rules,p_capacity,coalesce(p_listed,false),cfg.epoch) returning id into rid;
      exit;
    exception when unique_violation then
      if exists(select 1 from bt4_private.rooms where owner=who) then raise exception 'ALREADY_IN_ROOM'; end if;
    end;
  end loop;
  if rid is null then raise exception 'CODE_EXHAUSTED'; end if;
  insert into bt4_private.members(room_id,user_id,peer,name) values(rid,who,1,p_name);
  return rid;
end $$;

create or replace function public.bt4_room_join(p_code text,p_wire text,p_content text,p_name text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; allocated integer;
begin
  who:=auth.uid();
  perform bt4_private.validate_profile('duel',p_wire,p_content,'join',p_name);
  if p_code is null or upper(p_code) !~ '^([A-HJ-NP-Z2-9]{6}|[01][A-HJ-NP-Z2-9]{5})$' then raise exception 'BAD_CODE' using errcode='22023'; end if;
  select * into r from bt4_private.rooms where code=upper(p_code) for update;
  if not found or r.expires_at<clock_timestamp() then raise exception 'ROOM_NOT_FOUND' using errcode='P0002'; end if;
  if r.wire<>p_wire or r.content<>p_content then raise exception 'INCOMPATIBLE_DATA' using errcode='22023'; end if;
  if exists(select 1 from bt4_private.members where room_id=r.id and user_id=who) then return bt4_private.descriptor(r.id,who); end if;
  if not r.joinable then raise exception 'ROOM_BUSY'; end if;
  if exists(select 1 from bt4_private.members where user_id=who) then raise exception 'ALREADY_IN_ROOM'; end if;
  if (select count(*) from bt4_private.members where room_id=r.id)>=r.capacity then raise exception 'ROOM_FULL'; end if;
  allocated:=r.next_peer;
  update bt4_private.rooms set next_peer=next_peer+1 where id=r.id;
  insert into bt4_private.members values(r.id,who,allocated,p_name,clock_timestamp());
  delete from bt4_private.queue where user_id=who;
  perform bt4_private.changed(r.id);
  return bt4_private.descriptor(r.id,who);
end $$;
create or replace function public.bt4_room_join_v2(p_code text,p_wire text,p_content text,p_name text,p_role text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; allocated integer;
begin
  who:=auth.uid();
  if p_role is null or p_role not in ('player','spectator') then raise exception 'BAD_PROFILE'; end if;
  perform bt4_private.validate_profile('duel',p_wire,p_content,'join',p_name);
  if p_code is null or upper(p_code) !~ '^([A-HJ-NP-Z2-9]{6}|[01][A-HJ-NP-Z2-9]{5})$' then raise exception 'BAD_CODE' using errcode='22023'; end if;
  select * into r from bt4_private.rooms where code=upper(p_code) for update;
  if not found or r.expires_at<clock_timestamp() then raise exception 'ROOM_NOT_FOUND' using errcode='P0002'; end if;
  if r.wire<>p_wire or r.content<>p_content then raise exception 'INCOMPATIBLE_DATA' using errcode='22023'; end if;
  if exists(select 1 from bt4_private.members where room_id=r.id and user_id=who) then return bt4_private.descriptor(r.id,who); end if;
  if r.mode='tag' and p_role<>'player' then raise exception 'BAD_PROFILE'; end if;
  if r.mode='duel' and p_role='player' and (select count(*) from bt4_private.members where room_id=r.id and role='player')>=2 then raise exception 'ROOM_FULL'; end if;
  if not r.joinable then raise exception 'ROOM_BUSY'; end if;
  if exists(select 1 from bt4_private.members where user_id=who) then raise exception 'ALREADY_IN_ROOM'; end if;
  if (select count(*) from bt4_private.members where room_id=r.id)>=r.capacity then raise exception 'ROOM_FULL'; end if;
  allocated:=r.next_peer;
  update bt4_private.rooms set next_peer=next_peer+1 where id=r.id;
  insert into bt4_private.members values(r.id,who,allocated,p_name,clock_timestamp(),p_role);
  delete from bt4_private.queue where user_id=who;
  perform bt4_private.changed(r.id);
  return bt4_private.descriptor(r.id,who);
end $$;
commit;
