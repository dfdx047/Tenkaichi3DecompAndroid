-- Authenticated fight liveness and idempotent, fixed-point forfeits.
begin;
alter table bt4_private.ranked_matches
 add column started_a timestamptz,
 add column started_b timestamptz,
 add column seen_a timestamptz,
 add column seen_b timestamptz,
 add column forfeit_by uuid,
 add column abandonment_void boolean not null default false;
create or replace function bt4_private.ranked_record_result(p_match uuid,p_winner uuid,p_digest text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare m bt4_private.ranked_matches%rowtype; a integer; b integer; gain integer; outcome numeric;
begin
 if p_digest is null or p_digest !~ '^[a-f0-9]{64}$' then raise exception 'BAD_DIGEST'; end if;
 if not exists(select 1 from bt4_private.ranked_config where singleton and enabled) then raise exception 'RANKED_NOT_ENABLED'; end if;
 perform pg_advisory_xact_lock(5454,4001);
 select * into m from bt4_private.ranked_matches where id=p_match for update;
 if not found then raise exception 'MATCH_NOT_ADMITTED'; end if;
 if m.completed_at is not null then
  if m.winner is not distinct from p_winner and m.result_digest=p_digest then
   return jsonb_build_object('state','already_applied');
  end if;
  raise exception 'CONFLICTING_RESULT';
 end if;
 if p_winner is not null and p_winner not in (m.player_a,m.player_b) then raise exception 'BAD_WINNER'; end if;
 if not exists(select 1 from bt4_private.ranked_seasons s where s.id=m.season
  and m.admitted_at>=s.starts_at and m.admitted_at<s.ends_at) then raise exception 'BAD_SEASON'; end if;
 if (select count(*) from bt4_private.ranked_profiles where season=m.season and ruleset=m.ruleset
  and user_id in (m.player_a,m.player_b))<>2 then raise exception 'MATCH_NOT_ADMITTED'; end if;
 select rating into strict a from bt4_private.ranked_players where ruleset=m.ruleset and user_id=m.player_a for update;
 select rating into strict b from bt4_private.ranked_players where ruleset=m.ruleset and user_id=m.player_b for update;
 outcome:=case when p_winner=m.player_a then 1 when p_winner=m.player_b then 0 else 0.5 end;
 if m.forfeit_by is not null then
  gain:=case when p_winner=m.player_a then 16 else -16 end;
 else
  gain:=round(32*(outcome-1/(1+power(10::numeric,(b-a)::numeric/400))))::integer;
 end if;
 gain:=greatest(-a,b-100000,least(b,100000-a,gain));
 update bt4_private.ranked_players set rating=rating+gain,updated_at=now(),
  wins=wins+case when p_winner=m.player_a then 1 else 0 end,
  losses=losses+case when p_winner=m.player_b then 1 else 0 end
  where ruleset=m.ruleset and user_id=m.player_a;
 update bt4_private.ranked_players set rating=rating-gain,updated_at=now(),
  wins=wins+case when p_winner=m.player_b then 1 else 0 end,
  losses=losses+case when p_winner=m.player_a then 1 else 0 end
  where ruleset=m.ruleset and user_id=m.player_b;
 update bt4_private.ranked_profiles set rating=a+gain,
  wins=wins+case when p_winner=m.player_a then 1 else 0 end,
  losses=losses+case when p_winner=m.player_b then 1 else 0 end
  where season=m.season and ruleset=m.ruleset and user_id=m.player_a;
 update bt4_private.ranked_profiles set rating=b-gain,
  wins=wins+case when p_winner=m.player_b then 1 else 0 end,
  losses=losses+case when p_winner=m.player_a then 1 else 0 end
  where season=m.season and ruleset=m.ruleset and user_id=m.player_b;
 insert into bt4_private.ranked_rating_history values(p_match,m.player_a,a,gain,a+gain),(p_match,m.player_b,b,-gain,b-gain);
 update bt4_private.ranked_matches set winner=p_winner,result_digest=p_digest,completed_at=now() where id=p_match;
 return jsonb_build_object('state','applied');
end $$;


alter function public.bt4_ranked_api(text,jsonb) rename to bt4_ranked_gameplay;
revoke all on function public.bt4_ranked_gameplay(text,jsonb) from public,anon,authenticated;
create function public.bt4_ranked_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); m bt4_private.ranked_matches%rowtype; chosen uuid; quitter uuid;
 stamp timestamptz:=clock_timestamp(); reply jsonb; result_hash text;
begin
 if who is null then return jsonb_build_object('error','AUTH_REQUIRED'); end if;
 if p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>2048 then
  return jsonb_build_object('error','BAD_REQUEST'); end if;
 if p_op is null then return jsonb_build_object('error','BAD_REQUEST');end if;
 if p_op not in ('heartbeat','forfeit') then return public.bt4_ranked_gameplay(p_op,p_args); end if;
 if not exists(select 1 from bt4_private.ranked_config c join bt4_private.route_config r on r.singleton
  where c.singleton and c.enabled and c.authority_backend=r.backend_id) then
  return jsonb_build_object('error','RANKED_UNAVAILABLE'); end if;
 perform bt4_private.budget('ranked_liveness',40);
 perform pg_advisory_xact_lock(5454,4001);
 select * into m from bt4_private.ranked_matches where id=(p_args->>'match')::uuid for update;
 if not found or who not in(m.player_a,m.player_b) then return jsonb_build_object('error','NOT_A_PARTICIPANT'); end if;
 if m.completed_at is not null then
  return jsonb_build_object('state','already_applied')||coalesce((select jsonb_build_object('rating',h.after,'delta',h.delta)
   from bt4_private.ranked_rating_history h where h.match_id=m.id and h.user_id=who),'{}'::jsonb);
 end if;
 if m.abandonment_void or m.admitted_at<stamp-interval '90 minutes' then return jsonb_build_object('state','expired'); end if;
 -- Both clients disappearing is not evidence that either one won. Do not revive
 -- a stale match later to collect a false forfeit.
 if m.started_a is not null and m.started_b is not null and m.seen_a<stamp-interval '45 seconds' and m.seen_b<stamp-interval '45 seconds' then
  update bt4_private.ranked_matches set abandonment_void=true where id=m.id;
  return jsonb_build_object('state','expired');
 end if;
 if p_op='heartbeat' then
  update bt4_private.ranked_matches set
   started_a=case when who=player_a then coalesce(started_a,stamp) else started_a end,
   started_b=case when who=player_b then coalesce(started_b,stamp) else started_b end,
   seen_a=case when who=player_a then stamp else seen_a end,
   seen_b=case when who=player_b then stamp else seen_b end
   where id=m.id returning * into m;
 else
  -- A client may concede only its own admitted fight, not blame its opponent.
  update bt4_private.ranked_matches set forfeit_by=coalesce(forfeit_by,who),
   started_a=case when who=player_a then coalesce(started_a,stamp) else started_a end,
   started_b=case when who=player_b then coalesce(started_b,stamp) else started_b end
   where id=m.id returning * into m;
 end if;
 -- A failed room/UDP/character-select handshake cannot change anyone's rating.
 if m.started_a is null or m.started_b is null then return jsonb_build_object('state','waiting'); end if;
 quitter:=m.forfeit_by;
 if quitter is null then
  if who=m.player_a and m.seen_b<stamp-interval '45 seconds' then quitter:=m.player_b;
  elsif who=m.player_b and m.seen_a<stamp-interval '45 seconds' then quitter:=m.player_a;
  end if;
 end if;
 if quitter is null then return jsonb_build_object('state','waiting'); end if;
 -- A surviving player's confirmed win can coexist with a missing loser report.
 -- A contradictory terminal report requires reconciliation, not a timeout award.
 if exists(select 1 from bt4_private.ranked_reports where match_id=m.id and
  outcome<>case when quitter=m.player_a then 1 else 0 end) then
  return jsonb_build_object('state','waiting'); end if;
 chosen:=case when quitter=m.player_a then m.player_b else m.player_a end;
 update bt4_private.ranked_matches set forfeit_by=quitter where id=m.id;
 result_hash:=md5(m.id::text||quitter::text)||md5('forfeit-v1:'||m.id::text);
 reply:=bt4_private.ranked_record_result(m.id,chosen,result_hash);
 return reply||coalesce((select jsonb_build_object('rating',h.after,'delta',h.delta)
   from bt4_private.ranked_rating_history h where h.match_id=m.id and h.user_id=who),'{}'::jsonb);
exception when invalid_text_representation or numeric_value_out_of_range then
 return jsonb_build_object('error','BAD_REQUEST');
end $$;
revoke all on function public.bt4_ranked_api(text,jsonb) from public,anon;
grant execute on function public.bt4_ranked_api(text,jsonb) to authenticated;
revoke all on function bt4_private.ranked_record_result(uuid,uuid,text) from public,anon,authenticated;

-- Retain bounded, authenticated Tag Team negotiation messages for a reconnecting
-- inbox. This carries SDP/ICE only, never gameplay, audio or player inputs.
create table bt4_private.rtc_signals (
 id bigint generated always as identity primary key,
 room_id uuid not null references bt4_private.rooms(id) on delete cascade,
 sender uuid not null, recipient uuid not null, packet jsonb not null,
 fingerprint text not null, expires_at timestamptz not null default clock_timestamp()+interval '2 minutes',
 unique(room_id,sender,recipient,fingerprint)
);
create index rtc_signals_recipient_id on bt4_private.rtc_signals(recipient,id);
create index rtc_signals_expiry on bt4_private.rtc_signals(expires_at);
alter table bt4_private.rtc_signals enable row level security;
revoke all on bt4_private.rtc_signals from public,anon,authenticated;
alter function public.bt4_signal(uuid,integer,text,text,jsonb) rename to bt4_signal_broadcast;
revoke all on function public.bt4_signal_broadcast(uuid,integer,text,text,jsonb) from public,anon,authenticated;
create function public.bt4_signal(p_generation uuid,p_to integer,p_channel text,p_kind text,p_body jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare reply jsonb; rid uuid; sender_peer integer; target uuid;
begin
 -- Canonical validation, authenticated membership, generation and recipient checks.
 reply:=public.bt4_signal_broadcast(p_generation,p_to,p_channel,p_kind,p_body);
 if p_channel='tag' and reply->>'state'='sent' then
  select r.id,m.peer into rid,sender_peer from bt4_private.rooms r join bt4_private.members m on m.room_id=r.id
   where r.generation=p_generation and m.user_id=auth.uid();
  select user_id into target from bt4_private.members where room_id=rid and peer=p_to;
  delete from bt4_private.rtc_signals where expires_at<clock_timestamp();
  if (select count(*) from bt4_private.rtc_signals where room_id=rid)<128 then
   insert into bt4_private.rtc_signals(room_id,sender,recipient,fingerprint,packet)
    values(rid,auth.uid(),target,md5(p_kind||p_body::text),jsonb_build_object('generation',p_generation,
      'from',sender_peer,'to',p_to,'channel',p_channel,'kind',p_kind,'body',p_body)) on conflict do nothing;
  end if;
 end if;
 return reply;
end $$;
revoke all on function public.bt4_signal(uuid,integer,text,text,jsonb) from public,anon,authenticated;
alter function public.bt4_api(text,jsonb) rename to bt4_room_api;
revoke all on function public.bt4_room_api(text,jsonb) from public,anon,authenticated;
create function public.bt4_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); rid uuid; item bt4_private.rtc_signals%rowtype; cursor_id bigint;
begin
 if p_op is distinct from 'get_signals' then return public.bt4_room_api(p_op,p_args); end if;
 if who is null then return jsonb_build_object('error','AUTH_REQUIRED');end if;
 if p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>512 then
  return jsonb_build_object('error','BAD_REQUEST');end if;
 perform bt4_private.budget('get_signals',90);
 select r.id into rid from bt4_private.rooms r join bt4_private.members m on m.room_id=r.id
  where m.user_id=who and r.generation=(p_args->>'generation')::uuid and r.expires_at>clock_timestamp();
 if rid is null then return jsonb_build_object('error','NOT_A_MEMBER');end if;
 cursor_id:=coalesce((p_args->>'after')::bigint,0);
 select * into item from bt4_private.rtc_signals where room_id=rid and recipient=who and id>cursor_id
  and expires_at>clock_timestamp() order by id limit 1;
 if not found then return jsonb_build_object('signals','[]'::jsonb,'cursor',cursor_id);end if;
 return jsonb_build_object('signals',jsonb_build_array(item.packet),'cursor',item.id);
exception when invalid_text_representation or numeric_value_out_of_range then
 return jsonb_build_object('error','BAD_REQUEST');
end $$;
revoke all on function public.bt4_api(text,jsonb) from public,anon;
grant execute on function public.bt4_api(text,jsonb) to authenticated;

notify pgrst,'reload schema';
commit;
