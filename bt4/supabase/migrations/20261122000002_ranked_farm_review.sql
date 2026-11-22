-- Read-only REVIEW AID for several-account farming (nothing is enforced and no rating changes): the accounts whose rated wins of the last 7 days come almost all from a handful of rivals.
-- A flagged account is a question for a human (friends who only play each other look the same), not a verdict. Thresholds are the view's, change them there.
begin;
create or replace view bt4_private.ranked_farm_review as
 with w as (
  select m.ruleset,m.winner as account,case when m.winner=m.player_a then m.player_b else m.player_a end as opponent
  from bt4_private.ranked_matches m
  where m.completed_at>now()-interval '7 days' and m.rated and m.winner is not null),
 per_opp as (select ruleset,account,opponent,count(*) wins_vs from w group by ruleset,account,opponent),
 ranked_opp as (select *,row_number() over(partition by ruleset,account order by wins_vs desc,opponent) rk from per_opp),
 agg as (select ruleset,account,sum(wins_vs)::integer rated_wins,count(*)::integer opponents,
   coalesce(sum(wins_vs) filter(where rk<=3),0)::integer top3_wins from ranked_opp group by ruleset,account)
 select ruleset,account,rated_wins,opponents,top3_wins,round(top3_wins::numeric/rated_wins,2) top3_share
 from agg where rated_wins>=12 and top3_wins::numeric/rated_wins>=0.8;
revoke all on bt4_private.ranked_farm_review from public,anon,authenticated;
commit;
