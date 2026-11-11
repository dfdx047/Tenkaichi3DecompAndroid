-- BT4 discovery and ICE signaling only. Never send gameplay or audio here.
begin;
create schema if not exists bt4_private;
revoke all on schema bt4_private from public, anon, authenticated;

create table bt4_private.rooms (
  id uuid primary key default gen_random_uuid(),
  code text not null unique check (code ~ '^[A-HJ-NP-Z2-9]{6}$'),
  owner uuid not null references auth.users(id) on delete cascade,
  generation uuid not null default gen_random_uuid(),
  mode text not null check (mode in ('duel','tag','lobby','tournament')),
  wire text not null check (length(wire) between 1 and 64),
  content text not null check (content ~ '^[a-f0-9]{16,64}$'),
  rules_key text not null check (length(rules_key) between 1 and 64),
  capacity integer not null check (capacity between 2 and 16),
  next_peer integer not null default 2,
  joinable boolean not null default true,
  listed boolean not null default false,
  expires_at timestamptz not null default clock_timestamp()+interval '5 minutes',
  created_at timestamptz not null default clock_timestamp(),
  unique(owner)
);
create index rooms_listing on bt4_private.rooms(mode,wire,content,created_at,id) where listed and joinable;
create table bt4_private.members (
  room_id uuid not null references bt4_private.rooms(id) on delete cascade,
  user_id uuid not null references auth.users(id) on delete cascade,
  peer integer not null check (peer between 1 and 2147483646),
  name text not null check (octet_length(name) between 1 and 19),
  joined_at timestamptz not null default clock_timestamp(),
  primary key(room_id,user_id), unique(room_id,peer), unique(user_id)
);
create table bt4_private.queue (
  user_id uuid primary key references auth.users(id) on delete cascade,
  mode text not null check(mode in ('duel','tag')),
  wire text not null,
  content text not null,
  rules_key text not null,
  name text not null,
  joined_at timestamptz not null default clock_timestamp(),
  expires_at timestamptz not null default clock_timestamp()+interval '2 minutes'
);
create index queue_matching on bt4_private.queue(mode,wire,content,rules_key,joined_at);
create table bt4_private.budgets (
  user_id uuid not null references auth.users(id) on delete cascade,
  operation text not null,
  window_at timestamptz not null,
  used integer not null,
  primary key(user_id,operation)
);
alter table bt4_private.rooms enable row level security;
alter table bt4_private.members enable row level security;
alter table bt4_private.queue enable row level security;
alter table bt4_private.budgets enable row level security;
revoke all on all tables in schema bt4_private from public, anon, authenticated;

create function bt4_private.budget(op text, maximum integer) returns uuid
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); n integer;
begin
  if who is null then raise exception 'AUTH_REQUIRED' using errcode='28000'; end if;
  insert into bt4_private.budgets values(who,op,clock_timestamp(),1)
  on conflict(user_id,operation) do update set
    used=case when bt4_private.budgets.window_at<clock_timestamp()-interval '1 minute' then 1 else bt4_private.budgets.used+1 end,
    window_at=case when bt4_private.budgets.window_at<clock_timestamp()-interval '1 minute' then clock_timestamp() else bt4_private.budgets.window_at end
  returning used into n;
  if n>maximum then raise exception 'RATE_LIMITED' using errcode='54000'; end if;
  return who;
end $$;

create function bt4_private.validate_profile(p_mode text,p_wire text,p_content text,p_rules text,p_name text) returns void
language plpgsql set search_path='' as $$
begin
  if p_mode not in ('duel','tag','lobby','tournament') or p_mode is null
     or p_wire is null or length(p_wire) not between 1 and 64
     or p_content is null or p_content !~ '^[a-f0-9]{16,64}$'
     or p_rules is null or length(p_rules) not between 1 and 64
     or p_name is null or octet_length(p_name) not between 1 and 19
     or p_name ~ '[[:cntrl:]]' then
    raise exception 'BAD_PROFILE' using errcode='22023';
  end if;
end $$;

create function bt4_private.prune() returns void
language sql security definer set search_path='' as $$
  delete from bt4_private.rooms where expires_at<clock_timestamp();
  delete from bt4_private.queue where expires_at<clock_timestamp();
  delete from bt4_private.budgets where window_at<clock_timestamp()-interval '1 day';
$$;

create function bt4_private.inbox(who uuid,event_name text,body jsonb) returns void
language plpgsql security definer set search_path='' as $$
begin
  perform realtime.send(body,event_name,'bt4:inbox:'||who::text,true);
end $$;

create function bt4_private.descriptor(room_id uuid,who uuid) returns jsonb
language sql stable security definer set search_path='' as $$
  select jsonb_build_object('version',1,'room',r.id,'code',r.code,'generation',r.generation,
    'mode',r.mode,'wire',r.wire,'content',r.content,'rules',r.rules_key,
    'host',1,'self',m.peer,'capacity',r.capacity,'joinable',r.joinable,
    'expires_at',r.expires_at,'members',coalesce((select jsonb_agg(jsonb_build_object('peer',p.peer,'name',p.name) order by p.peer)
      from bt4_private.members p where p.room_id=r.id),'[]'::jsonb))
  from bt4_private.rooms r join bt4_private.members m on m.room_id=r.id
  where r.id=$1 and m.user_id=$2;
$$;

create function bt4_private.changed(room_id uuid) returns void
language plpgsql security definer set search_path='' as $$
declare m record;
begin
  for m in select user_id from bt4_private.members where members.room_id=changed.room_id loop
    perform bt4_private.inbox(m.user_id,'bt4.room',bt4_private.descriptor(room_id,m.user_id));
  end loop;
end $$;

create function bt4_private.new_room(who uuid,p_mode text,p_wire text,p_content text,p_rules text,p_name text,p_capacity integer,p_listed boolean) returns uuid
language plpgsql security definer set search_path='' as $$
declare rid uuid; candidate text; bytes bytea; alphabet constant text:='ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; i integer;
begin
  if p_capacity is null or p_capacity not between 2 and 16 or (p_mode='tag' and p_capacity>10) then
    raise exception 'BAD_CAPACITY' using errcode='22023';
  end if;
  if exists(select 1 from bt4_private.members where user_id=who) then
    raise exception 'ALREADY_IN_ROOM' using errcode='23505';
  end if;
  for attempt in 1..10 loop
    bytes:=extensions.gen_random_bytes(6); candidate:='';
    for i in 0..5 loop candidate:=candidate||substr(alphabet,(get_byte(bytes,i)%32)+1,1); end loop;
    begin
      insert into bt4_private.rooms(code,owner,mode,wire,content,rules_key,capacity,listed)
        values(candidate,who,p_mode,p_wire,p_content,p_rules,p_capacity,coalesce(p_listed,false)) returning id into rid;
      exit;
    exception when unique_violation then
      if exists(select 1 from bt4_private.rooms where owner=who) then raise exception 'ALREADY_IN_ROOM'; end if;
    end;
  end loop;
  if rid is null then raise exception 'CODE_EXHAUSTED'; end if;
  insert into bt4_private.members(room_id,user_id,peer,name) values(rid,who,1,p_name);
  return rid;
end $$;

create function public.bt4_room_create(p_mode text,p_wire text,p_content text,p_rules text,p_name text,p_capacity integer default 2,p_listed boolean default false) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; rid uuid;
begin
  who:=auth.uid();
  perform bt4_private.validate_profile(p_mode,p_wire,p_content,p_rules,p_name);
  perform bt4_private.prune();
  delete from bt4_private.queue where user_id=who;
  rid:=bt4_private.new_room(who,p_mode,p_wire,p_content,p_rules,p_name,p_capacity,p_listed);
  return bt4_private.descriptor(rid,who);
end $$;

create function public.bt4_room_join(p_code text,p_wire text,p_content text,p_name text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; allocated integer;
begin
  who:=auth.uid();
  perform bt4_private.validate_profile('duel',p_wire,p_content,'join',p_name);
  if p_code is null or upper(p_code) !~ '^[A-HJ-NP-Z2-9]{6}$' then raise exception 'BAD_CODE' using errcode='22023'; end if;
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

create function public.bt4_room_get() returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; rid uuid;
begin
  who:=auth.uid();
  select r.id into rid from bt4_private.members m join bt4_private.rooms r on r.id=m.room_id
    where m.user_id=who and r.expires_at>clock_timestamp();
  if rid is null then return jsonb_build_object('version',1,'state','none'); end if;
  return bt4_private.descriptor(rid,who);
end $$;

create function public.bt4_room_lease(p_joinable boolean default null) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; rid uuid;
begin
  who:=auth.uid();
  update bt4_private.rooms set expires_at=clock_timestamp()+interval '5 minutes',joinable=coalesce(p_joinable,joinable)
    where owner=who and expires_at>clock_timestamp() returning id into rid;
  if rid is null then raise exception 'ROOM_NOT_FOUND'; end if;
  return bt4_private.descriptor(rid,who);
end $$;

create function public.bt4_room_leave() returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; m record;
begin
  who:=auth.uid();
  delete from bt4_private.queue where user_id=who;
  select r2.* into r from bt4_private.rooms r2 join bt4_private.members p on p.room_id=r2.id where p.user_id=who for update of r2;
  if not found then return jsonb_build_object('version',1,'state','left'); end if;
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

create function public.bt4_room_list(p_mode text,p_wire text,p_content text,p_after timestamptz default null,p_after_id uuid default null) returns jsonb
language plpgsql security definer set search_path='' as $$
begin
  if auth.uid() is null then raise exception 'AUTH_REQUIRED'; end if;
  perform bt4_private.validate_profile(p_mode,p_wire,p_content,'list','list');
  return coalesce((select jsonb_agg(item) from (select jsonb_build_object('code',r.code,'mode',r.mode,
    'name',m.name,'capacity',r.capacity,'players',(select count(*) from bt4_private.members p where p.room_id=r.id),
    'cursor',r.created_at,'cursor_id',r.id) item
    from bt4_private.rooms r join bt4_private.members m on m.room_id=r.id and m.peer=1
    where r.listed and r.joinable and r.expires_at>clock_timestamp() and r.mode=p_mode and r.wire=p_wire and r.content=p_content
    and (p_after is null or (r.created_at,r.id)>(p_after,coalesce(p_after_id,'00000000-0000-0000-0000-000000000000'::uuid)))
    order by r.created_at,r.id limit 20) page),'[]'::jsonb);
end $$;

create function public.bt4_queue_join(p_mode text,p_wire text,p_content text,p_rules text,p_name text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; candidate bt4_private.queue%rowtype; rid uuid;
begin
  who:=auth.uid();
  perform bt4_private.validate_profile(p_mode,p_wire,p_content,p_rules,p_name);
  if p_mode not in ('duel','tag') then raise exception 'BAD_QUEUE'; end if;
  perform bt4_private.prune();
  select room_id into rid from bt4_private.members where user_id=who;
  if found then return bt4_private.descriptor(rid,who); end if;
  -- Serialize compatible queue admission, including the empty-queue case.
  perform pg_advisory_xact_lock(hashtextextended(p_mode||':'||p_wire||':'||p_content||':'||p_rules,0));
  select * into candidate from bt4_private.queue q where q.user_id<>who and q.mode=p_mode and q.wire=p_wire
    and q.content=p_content and q.rules_key=p_rules and q.expires_at>clock_timestamp()
    order by q.joined_at for update skip locked limit 1;
  if not found then
    insert into bt4_private.queue(user_id,mode,wire,content,rules_key,name) values(who,p_mode,p_wire,p_content,p_rules,p_name)
    on conflict(user_id) do update set mode=p_mode,wire=p_wire,content=p_content,rules_key=p_rules,name=p_name,
      expires_at=clock_timestamp()+interval '2 minutes';
    return jsonb_build_object('version',1,'state','waiting','lease_seconds',120);
  end if;
  rid:=bt4_private.new_room(candidate.user_id,p_mode,p_wire,p_content,p_rules,candidate.name,case when p_mode='tag' then 10 else 2 end,false);
  insert into bt4_private.members values(rid,who,2,p_name,clock_timestamp());
  update bt4_private.rooms set next_peer=3 where id=rid;
  delete from bt4_private.queue where user_id in(who,candidate.user_id);
  perform bt4_private.changed(rid);
  return bt4_private.descriptor(rid,who);
end $$;

create function public.bt4_queue_cancel() returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid;
begin
  who:=auth.uid();
  delete from bt4_private.queue where user_id=who;
  return jsonb_build_object('version',1,'state','cancelled');
end $$;

create function public.bt4_signal(p_generation uuid,p_to integer,p_channel text,p_kind text,p_body jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; sender integer; recipient uuid; item jsonb; k text;
begin
  who:=auth.uid();
  if p_channel is null or p_channel not in ('duel','tag','voice') or p_body is null or jsonb_typeof(p_body)<>'object'
    or octet_length(p_body::text)>16000 then raise exception 'BAD_SIGNAL' using errcode='22023'; end if;
  if p_kind in ('offer','answer') then
    if p_channel='duel' or jsonb_typeof(p_body->'sdp')<>'string' or length(p_body->>'sdp') not between 1 and 14000
      or p_body-'sdp'<>'{}'::jsonb then raise exception 'BAD_SIGNAL'; end if;
  elsif p_kind='candidate' then
    if p_channel='duel' or jsonb_typeof(p_body->'candidate')<>'string' or length(p_body->>'candidate') not between 1 and 2048
      or jsonb_typeof(p_body->'mid')<>'string' or length(p_body->>'mid')>32 or p_body-'candidate'-'mid'<>'{}'::jsonb then raise exception 'BAD_SIGNAL'; end if;
  elsif p_kind='udp' then
    if p_channel<>'duel' or jsonb_typeof(p_body->'addresses')<>'array' or jsonb_array_length(p_body->'addresses') not between 1 and 4
      or p_body-'addresses'-'nonce'<>'{}'::jsonb or coalesce(p_body->>'nonce','')!~'^[a-f0-9]{32,64}$' then raise exception 'BAD_SIGNAL'; end if;
    for item in select value from jsonb_array_elements(p_body->'addresses') loop
      if jsonb_typeof(item)<>'string' or length(item#>>'{}')>96 or (item#>>'{}')!~'^[a-fA-F0-9.:\[\]]+$' then raise exception 'BAD_SIGNAL'; end if;
    end loop;
  else raise exception 'SIGNAL_KIND_NOT_ALLOWED';
  end if;
  -- Explicit checks also reject missing keys (SQL NULL must not bypass validation).
  if p_kind in ('offer','answer') and not p_body ? 'sdp' then raise exception 'BAD_SIGNAL'; end if;
  if p_kind='candidate' and not (p_body ? 'candidate' and p_body ? 'mid') then raise exception 'BAD_SIGNAL'; end if;
  if p_kind='udp' and not p_body ? 'addresses' then raise exception 'BAD_SIGNAL'; end if;
  select r2.* into r from bt4_private.rooms r2 join bt4_private.members m on m.room_id=r2.id
    where m.user_id=who and r2.generation=p_generation and r2.expires_at>clock_timestamp();
  if not found then raise exception 'NOT_A_MEMBER' using errcode='42501'; end if;
  select peer into sender from bt4_private.members where room_id=r.id and user_id=who;
  select user_id into recipient from bt4_private.members where room_id=r.id and peer=p_to and user_id<>who;
  if recipient is null then raise exception 'BAD_RECIPIENT' using errcode='42501'; end if;
  perform bt4_private.inbox(recipient,'bt4.signal',jsonb_build_object('version',1,'room',r.id,'generation',r.generation,
    'from',sender,'to',p_to,'channel',p_channel,'kind',p_kind,'body',p_body));
  return jsonb_build_object('version',1,'state','sent');
end $$;

-- Clients receive only their own server-authored inbox. There is no client
-- INSERT policy: neither a forged sender nor a frame/audio broadcast is accepted.
create policy bt4_inbox_receive on realtime.messages for select to authenticated
  using (realtime.topic()='bt4:inbox:'||(select auth.uid())::text and extension='broadcast');

revoke all on all functions in schema bt4_private from public,anon,authenticated;
revoke all on function public.bt4_room_create(text,text,text,text,text,integer,boolean),
  public.bt4_room_join(text,text,text,text),public.bt4_room_get(),public.bt4_room_lease(boolean),
  public.bt4_room_leave(),public.bt4_room_list(text,text,text,timestamptz,uuid),
  public.bt4_queue_join(text,text,text,text,text),public.bt4_queue_cancel(),
  public.bt4_signal(uuid,integer,text,text,jsonb) from public,anon,authenticated;
-- Only this bounded dispatcher is client-callable. Its outer budget survives
-- failed business operations; an exception in a directly called RPC would roll
-- the budget increment back and allow unlimited guesses of room codes.
create function public.bt4_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare maximum integer; answer jsonb;
begin
  maximum:=case p_op when 'create' then 6 when 'join' then 30 when 'get' then 30
    when 'lease' then 12 when 'leave' then 30 when 'list' then 12 when 'queue' then 12
    when 'cancel' then 12 when 'signal' then 240 else 0 end;
  if maximum=0 or p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>18000 then
    return jsonb_build_object('version',1,'error','BAD_REQUEST');
  end if;
  begin perform bt4_private.budget(p_op,maximum);
  exception when others then return jsonb_build_object('version',1,'error',SQLERRM); end;
  begin
    case p_op
      when 'create' then answer:=public.bt4_room_create(p_args->>'mode',p_args->>'wire',p_args->>'content',p_args->>'rules',p_args->>'name',coalesce((p_args->>'capacity')::integer,2),coalesce((p_args->>'listed')::boolean,false));
      when 'join' then answer:=public.bt4_room_join(p_args->>'code',p_args->>'wire',p_args->>'content',p_args->>'name');
      when 'get' then answer:=public.bt4_room_get();
      when 'lease' then answer:=public.bt4_room_lease((p_args->>'joinable')::boolean);
      when 'leave' then answer:=public.bt4_room_leave();
      when 'list' then answer:=public.bt4_room_list(p_args->>'mode',p_args->>'wire',p_args->>'content',(p_args->>'after')::timestamptz,(p_args->>'after_id')::uuid);
      when 'queue' then answer:=public.bt4_queue_join(p_args->>'mode',p_args->>'wire',p_args->>'content',p_args->>'rules',p_args->>'name');
      when 'cancel' then answer:=public.bt4_queue_cancel();
      when 'signal' then answer:=public.bt4_signal((p_args->>'generation')::uuid,(p_args->>'to')::integer,p_args->>'channel',p_args->>'kind',p_args->'body');
    end case;
  exception when others then
    return jsonb_build_object('version',1,'error',case when SQLSTATE like '22%' then 'BAD_REQUEST' else SQLERRM end);
  end;
  return answer;
end $$;
revoke all on function public.bt4_api(text,jsonb) from public,anon;
grant execute on function public.bt4_api(text,jsonb) to authenticated;
commit;
