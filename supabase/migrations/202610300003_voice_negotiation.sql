begin;
create or replace function public.bt4_signal(p_generation uuid,p_to integer,p_channel text,p_kind text,p_body jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; sender integer; recipient uuid; item jsonb; k text;
begin
  who:=auth.uid();
  if p_channel is null or p_channel not in ('duel','tag','voice') or p_body is null or jsonb_typeof(p_body)<>'object'
    or octet_length(p_body::text)>16000 then raise exception 'BAD_SIGNAL' using errcode='22023'; end if;
  if p_kind in ('offer','answer') then
    if p_channel='duel' or jsonb_typeof(p_body->'sdp')<>'string' or length(p_body->>'sdp') not between 1 and 14000
      or p_body-'sdp'<>(case when p_channel='voice' then jsonb_build_object('revision',p_body->'revision') else '{}'::jsonb end) then raise exception 'BAD_SIGNAL'; end if;
  elsif p_kind='candidate' then
    if p_channel='duel' or jsonb_typeof(p_body->'candidate')<>'string' or length(p_body->>'candidate') not between 1 and 2048
      or jsonb_typeof(p_body->'mid')<>'string' or length(p_body->>'mid')>32 or p_body-'candidate'-'mid'<>(case when p_channel='voice' then jsonb_build_object('revision',p_body->'revision') else '{}'::jsonb end) then raise exception 'BAD_SIGNAL'; end if;
  elsif p_kind='restart' then
    if p_channel<>'voice' or p_body<>'{}'::jsonb then raise exception 'BAD_SIGNAL'; end if;
  elsif p_kind='udp' then
    if p_channel<>'duel' or jsonb_typeof(p_body->'addresses')<>'array' or jsonb_array_length(p_body->'addresses') not between 1 and 4
      or p_body-'addresses'-'nonce'<>'{}'::jsonb or coalesce(p_body->>'nonce','')!~'^[a-f0-9]{32,64}$' then raise exception 'BAD_SIGNAL'; end if;
    for item in select value from jsonb_array_elements(p_body->'addresses') loop
      if jsonb_typeof(item)<>'string' or length(item#>>'{}')>96 or (item#>>'{}')!~'^[a-fA-F0-9.:\[\]]+$' then raise exception 'BAD_SIGNAL'; end if;
    end loop;
  else raise exception 'SIGNAL_KIND_NOT_ALLOWED';
  end if;
  if p_channel='voice' and p_kind<>'restart' then
    if not p_body ? 'revision' or jsonb_typeof(p_body->'revision')<>'number' or (p_body->>'revision')::bigint not between 1 and 2147483647 then raise exception 'BAD_SIGNAL'; end if;
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
commit;
