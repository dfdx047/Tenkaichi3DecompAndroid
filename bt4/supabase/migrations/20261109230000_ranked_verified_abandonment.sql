begin;
set local lock_timeout = '500ms';
set local statement_timeout = '10s';

alter table bt4_private.ranked_matches
 add column quit_a timestamptz,
 add column quit_b timestamptz;

create or replace function public.bt4_ranked_api(p_op text,p_args jsonb default '{}'::jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); m bt4_private.ranked_matches%rowtype;
 stamp timestamptz:=clock_timestamp(); reply jsonb; result_hash text;
 quitter uuid; winner uuid; other_seen timestamptz; explicit_quit boolean;
begin
 if who is null then return jsonb_build_object('error','AUTH_REQUIRED'); end if;
 if p_args is null or jsonb_typeof(p_args)<>'object' or octet_length(p_args::text)>2048 or p_op is null then
  return jsonb_build_object('error','BAD_REQUEST');
 end if;
 if p_op not in ('heartbeat','forfeit') then return public.bt4_ranked_gameplay(p_op,p_args); end if;
 if not exists(select 1 from bt4_private.ranked_config c join bt4_private.route_config r on r.singleton
  where c.singleton and c.enabled and c.authority_backend=r.backend_id) then
  return jsonb_build_object('error','RANKED_UNAVAILABLE');
 end if;
 perform bt4_private.budget('ranked_liveness',40);
 perform pg_advisory_xact_lock(5454,4001);
 select * into m from bt4_private.ranked_matches where id=(p_args->>'match')::uuid for update;
 if not found or who not in(m.player_a,m.player_b) then return jsonb_build_object('error','NOT_A_PARTICIPANT'); end if;
 if m.completed_at is not null then
  return jsonb_build_object('state','already_applied')||coalesce((select jsonb_build_object('rating',h.after,'delta',h.delta)
   from bt4_private.ranked_rating_history h where h.match_id=m.id and h.user_id=who),'{}'::jsonb);
 end if;
 if m.abandonment_void or m.admitted_at<stamp-interval '90 minutes' then return jsonb_build_object('state','expired'); end if;

 explicit_quit:=p_op='forfeit' and coalesce(p_args->>'reason','')='local_quit';
 if p_op='forfeit' and not explicit_quit then
  -- An older game could report the same synchronized pause-menu exit on both peers.
  -- Keep the two claims separate. An opposing claim is never evidence of a win.
  update bt4_private.ranked_matches set
   quit_a=case when who=player_a then coalesce(quit_a,stamp) else quit_a end,
   quit_b=case when who=player_b then coalesce(quit_b,stamp) else quit_b end
   where id=m.id returning * into m;
  if m.quit_a is not null and m.quit_b is not null then
   update bt4_private.ranked_matches set abandonment_void=true where id=m.id;
   return jsonb_build_object('state','expired');
  end if;
  return jsonb_build_object('state','waiting');
 end if;

 if not explicit_quit and m.seen_a<stamp-interval '45 seconds' and m.seen_b<stamp-interval '45 seconds' then
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
 end if;
 if m.started_a is null or m.started_b is null then return jsonb_build_object('state','waiting'); end if;

 if explicit_quit then
  quitter:=who;
 elsif who=m.player_a and m.quit_b<stamp-interval '15 seconds' and m.quit_a is null then
  quitter:=m.player_b;
 elsif who=m.player_b and m.quit_a<stamp-interval '15 seconds' and m.quit_b is null then
  quitter:=m.player_a;
 elsif m.quit_a is null and m.quit_b is null then
  other_seen:=case when who=m.player_a then m.seen_b else m.seen_a end;
  -- Without a reported loss of transport, a missing HTTP heartbeat can still be a live fight.
  -- Legacy clients send an authenticated room leave before polling the settlement.
  if other_seen<stamp-interval '45 seconds' and (p_args->'peer_gone'='true'::jsonb or
   (not (p_args ? 'peer_gone') and not exists(
    select 1 from bt4_private.rooms r join bt4_private.members rm on rm.room_id=r.id
    where r.generation=m.id and rm.user_id=who))) then
   quitter:=case when who=m.player_a then m.player_b else m.player_a end;
  end if;
 end if;
 if quitter is null then return jsonb_build_object('state','waiting'); end if;
 if not explicit_quit and exists(select 1 from bt4_private.ranked_reports where match_id=m.id and
  outcome<>case when quitter=m.player_a then 1 else 0 end) then
  return jsonb_build_object('state','waiting');
 end if;
 winner:=case when quitter=m.player_a then m.player_b else m.player_a end;
 update bt4_private.ranked_matches set forfeit_by=quitter where id=m.id;
 result_hash:=md5(m.id::text||quitter::text)||md5('forfeit-v1:'||m.id::text);
 reply:=bt4_private.ranked_record_result(m.id,winner,result_hash);
 return reply||coalesce((select jsonb_build_object('rating',h.after,'delta',h.delta)
  from bt4_private.ranked_rating_history h where h.match_id=m.id and h.user_id=who),'{}'::jsonb);
exception when invalid_text_representation or numeric_value_out_of_range then
 return jsonb_build_object('error','BAD_REQUEST');
end $$;

commit;
