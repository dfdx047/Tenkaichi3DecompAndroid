begin;
alter table bt4_private.members add column role text not null default 'player' check (role in ('player','spectator'));
create or replace function bt4_private.descriptor(room_id uuid,who uuid) returns jsonb
language sql stable security definer set search_path='' as $$
  select jsonb_build_object('version',1,'room',r.id,'code',r.code,'generation',r.generation,
    'mode',r.mode,'wire',r.wire,'content',r.content,'rules',r.rules_key,
    'host',1,'self',m.peer,'capacity',r.capacity,'joinable',r.joinable,
    'expires_at',r.expires_at,'members',coalesce((select jsonb_agg(jsonb_build_object('peer',p.peer,'name',p.name,'role',p.role) order by p.peer)
      from bt4_private.members p where p.room_id=r.id),'[]'::jsonb))
  from bt4_private.rooms r join bt4_private.members m on m.room_id=r.id
  where r.id=$1 and m.user_id=$2;
$$;
create or replace function public.bt4_room_join_v2(p_code text,p_wire text,p_content text,p_name text,p_role text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; allocated integer;
begin
  who:=auth.uid();
  if p_role is null or p_role not in ('player','spectator') then raise exception 'BAD_PROFILE'; end if;
  perform bt4_private.validate_profile('duel',p_wire,p_content,'join',p_name);
  if p_code is null or upper(p_code) !~ '^[A-HJ-NP-Z2-9]{6}$' then raise exception 'BAD_CODE' using errcode='22023'; end if;
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
create or replace function public.bt4_room_leave_v2(p_generation uuid) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; m record;
begin
  who:=auth.uid();
  if p_generation is null then delete from bt4_private.queue where user_id=who; end if;
  select r2.* into r from bt4_private.rooms r2 join bt4_private.members p on p.room_id=r2.id where p.user_id=who for update of r2;
  if not found then return jsonb_build_object('version',1,'state','left'); end if;
  if p_generation is not null and r.generation<>p_generation then return jsonb_build_object('version',1,'state','left'); end if;
  if r.owner=who then
    for m in select user_id from bt4_private.members where room_id=r.id and user_id<>who loop
      perform bt4_private.inbox(m.user_id,'bt4.closed',jsonb_build_object('room',r.id,'generation',r.generation));
    end loop;
    delete from bt4_private.rooms where id=r.id;
  else
    delete from bt4_private.members where room_id=r.id and user_id=who;
    perform bt4_private.changed(r.id);
  end if;
  return jsonb_build_object('version',1,'state','left');
end $$;
create or replace function public.bt4_queue_counts(p_wire text,p_content text) returns jsonb
language sql stable security definer set search_path='' as $$
  select coalesce(jsonb_object_agg(q.rules_key,q.n),'{}'::jsonb) from
    (select rules_key,count(*) n from bt4_private.queue where mode='duel' and wire=p_wire and content=p_content
      and expires_at>clock_timestamp() group by rules_key limit 64) q;
$$;
create or replace function public.bt4_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare maximum integer; answer jsonb;
begin
  maximum:=case p_op when 'create' then 6 when 'join' then 30 when 'get' then 30
    when 'lease' then 12 when 'leave' then 30 when 'list' then 12 when 'queue' then 12
    when 'cancel' then 12 when 'signal' then 240 when 'counts' then 6 else 0 end;
  if maximum=0 or p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>18000 then
    return jsonb_build_object('version',1,'error','BAD_REQUEST');
  end if;
  begin perform bt4_private.budget(p_op,maximum);
  exception when others then return jsonb_build_object('version',1,'error',SQLERRM); end;
  begin
    case p_op
      when 'create' then answer:=public.bt4_room_create(p_args->>'mode',p_args->>'wire',p_args->>'content',p_args->>'rules',p_args->>'name',coalesce((p_args->>'capacity')::integer,2),coalesce((p_args->>'listed')::boolean,false));
      when 'join' then answer:=public.bt4_room_join_v2(p_args->>'code',p_args->>'wire',p_args->>'content',p_args->>'name',coalesce(p_args->>'role','player'));
      when 'counts' then answer:=public.bt4_queue_counts(p_args->>'wire',p_args->>'content');
      when 'get' then answer:=public.bt4_room_get();
      when 'lease' then answer:=public.bt4_room_lease((p_args->>'joinable')::boolean);
      when 'leave' then answer:=public.bt4_room_leave_v2((p_args->>'generation')::uuid);
      when 'list' then answer:=public.bt4_room_list(p_args->>'mode',p_args->>'wire',p_args->>'content',(p_args->>'after')::timestamptz,(p_args->>'after_id')::uuid);
      when 'queue' then answer:=public.bt4_queue_join(p_args->>'mode',p_args->>'wire',p_args->>'content',p_args->>'rules',p_args->>'name');
      when 'cancel' then answer:=public.bt4_queue_cancel();
      when 'signal' then answer:=public.bt4_signal((p_args->>'generation')::uuid,(p_args->>'to')::integer,p_args->>'channel',p_args->>'kind',p_args->'body');
    end case;
  exception when others then
    return jsonb_build_object('version',1,'error',case when SQLERRM in ('AUTH_REQUIRED','RATE_LIMITED','BAD_CODE','ROOM_NOT_FOUND','INCOMPATIBLE_DATA','ROOM_BUSY','ALREADY_IN_ROOM','ROOM_FULL','BAD_CAPACITY','BAD_PROFILE','BAD_SIGNAL','SIGNAL_KIND_NOT_ALLOWED','BAD_RECIPIENT','NOT_A_MEMBER','NOT_OWNER') then SQLERRM when SQLSTATE like '22%' then 'BAD_REQUEST' else 'INTERNAL_ERROR' end);
  end;
  return answer;
end $$;
revoke all on function public.bt4_room_join_v2(text,text,text,text,text),public.bt4_room_leave_v2(uuid),public.bt4_queue_counts(text,text) from public,anon,authenticated;
commit;
