begin;
create or replace function public.bt4_queue_join(p_mode text,p_wire text,p_content text,p_rules text,p_name text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid; candidate record; group_ids uuid[]; rid uuid; needed integer; peer_id integer:=1;
begin
  who:=auth.uid();
  perform bt4_private.validate_profile(p_mode,p_wire,p_content,p_rules,p_name);
  if p_mode='duel' then
    if p_rules not in ('S','T','D10','D15','D20') then raise exception 'BAD_QUEUE'; end if;
    needed:=2;
  elsif p_mode='tag' and p_wire='bt4-p2p-2' then
    needed:=case p_rules when '2v2-114' then 4 else 0 end;
    if needed=0 then raise exception 'BAD_QUEUE'; end if;
  else raise exception 'BAD_QUEUE'; end if;
  perform bt4_private.prune();
  select m.room_id into rid from bt4_private.members m join bt4_private.rooms r on r.id=m.room_id
    where m.user_id=who and r.mode=p_mode and r.wire=p_wire and r.content=p_content and r.rules_key=p_rules;
  if found then return bt4_private.descriptor(rid,who); end if;
  if exists(select 1 from bt4_private.members where user_id=who) then raise exception 'ALREADY_IN_ROOM'; end if;
  perform pg_advisory_xact_lock(hashtextextended(p_mode||':'||p_wire||':'||p_content||':'||p_rules,0));
  insert into bt4_private.queue(user_id,mode,wire,content,rules_key,name)
    values(who,p_mode,p_wire,p_content,p_rules,p_name)
  on conflict(user_id) do update set mode=p_mode,wire=p_wire,content=p_content,rules_key=p_rules,name=p_name,
    expires_at=clock_timestamp()+interval '2 minutes';
  select array_agg(q.user_id order by q.joined_at,q.user_id) into group_ids from
    (select user_id,joined_at from bt4_private.queue where mode=p_mode and wire=p_wire and content=p_content
      and rules_key=p_rules and expires_at>clock_timestamp() order by joined_at,user_id for update limit needed) q;
  if coalesce(array_length(group_ids,1),0)<needed then
    return jsonb_build_object('version',1,'state','waiting','lease_seconds',120);
  end if;
  for candidate in select * from bt4_private.queue where user_id=any(group_ids) order by joined_at,user_id loop
    if peer_id=1 then
      rid:=bt4_private.new_room(candidate.user_id,p_mode,p_wire,p_content,p_rules,candidate.name,needed,false);
    else
      insert into bt4_private.members(room_id,user_id,peer,name,role) values(rid,candidate.user_id,peer_id,candidate.name,'player');
    end if;
    peer_id:=peer_id+1;
  end loop;
  update bt4_private.rooms set next_peer=peer_id where id=rid;
  delete from bt4_private.queue where user_id=any(group_ids);
  perform bt4_private.changed(rid);
  return bt4_private.descriptor(rid,who);
end $$;

create or replace function public.bt4_queue_counts(p_wire text,p_content text) returns jsonb
language sql stable security definer set search_path='' as $$
  select jsonb_build_object('S',count(*) filter(where rules_key='S'),'T',count(*) filter(where rules_key='T'),
    'D10',count(*) filter(where rules_key='D10'),'D15',count(*) filter(where rules_key='D15'),'D20',count(*) filter(where rules_key='D20'),
    'F6',count(*) filter(where rules_key='ffa6-114'),'T2',count(*) filter(where rules_key='2v2-114'),'T5',count(*) filter(where rules_key='5v5-114'))
  from bt4_private.queue where wire=p_wire and content=p_content and expires_at>clock_timestamp();
$$;
revoke all on function public.bt4_queue_join(text,text,text,text,text), public.bt4_queue_counts(text,text) from public,anon,authenticated;
commit;
