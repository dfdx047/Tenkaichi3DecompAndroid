begin;
alter table bt4_private.rooms add column if not exists voice_token text unique;
create or replace function bt4_private.voice_rendezvous(p_token text,p_seat integer,p_name text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; r bt4_private.rooms%rowtype; rid uuid;
begin
 who:=auth.uid();
 if p_token is null or p_token!~'^[a-f0-9]{64}$' or p_seat is null or p_seat not in (0,1) then raise exception 'BAD_REQUEST'; end if;
 perform bt4_private.validate_profile('duel','bt4-voice-direct-1',p_token,'voice-direct',p_name);
 perform pg_advisory_xact_lock(hashtextextended(p_token,764));
 perform bt4_private.prune();
 select * into r from bt4_private.rooms where voice_token=p_token and expires_at>clock_timestamp() for update;
 if not found then
   if p_seat=1 then return jsonb_build_object('version',1,'state','waiting'); end if;
   if exists(select 1 from bt4_private.members where user_id=who) then raise exception 'ALREADY_IN_ROOM'; end if;
   rid:=bt4_private.new_room(who,'duel','bt4-voice-direct-1',p_token,'voice-direct',p_name,2,false);
   update bt4_private.rooms set voice_token=p_token where id=rid;
   return bt4_private.descriptor(rid,who);
 end if;
 if exists(select 1 from bt4_private.members where room_id=r.id and user_id=who and peer=p_seat+1) then return bt4_private.descriptor(r.id,who); end if;
 if p_seat=0 or r.owner=who then raise exception 'NOT_OWNER'; end if;
 return public.bt4_room_join_v2(r.code,r.wire,r.content,p_name,'player');
end $$;
revoke all on function bt4_private.voice_rendezvous(text,integer,text) from public,anon,authenticated;
create or replace function public.bt4_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare maximum integer; answer jsonb;
begin
  maximum:=case p_op when 'create' then 6 when 'join' then 30 when 'get' then 30
    when 'lease' then 12 when 'leave' then 30 when 'list' then 12 when 'queue' then 12
    when 'cancel' then 12 when 'signal' then 240 when 'counts' then 6 when 'voice' then 18 else 0 end;
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
      when 'voice' then answer:=bt4_private.voice_rendezvous(p_args->>'token',(p_args->>'seat')::integer,p_args->>'name');
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
commit;
