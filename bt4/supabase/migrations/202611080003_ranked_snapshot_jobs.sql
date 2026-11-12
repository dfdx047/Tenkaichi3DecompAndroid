-- Snapshot maintenance does not enable ranked admission or result submission.
begin;
do $$begin
 if coalesce(current_setting('cron.timezone',true),'GMT') not in ('GMT','UTC','Etc/UTC') then
  raise exception 'Ranked weekly schedule requires UTC pg_cron timezone';
 end if;
end $$;
select cron.schedule('bt4-ranked-hourly','0 * * * *',
 'select bt4_private.ranked_refresh_top()');
-- Sunday 23:59 America/Argentina/Buenos_Aires is Monday 02:59 UTC.
select cron.schedule('bt4-ranked-weekly','59 2 * * 1',
 'select bt4_private.ranked_rollover(); select bt4_private.ranked_refresh_top()');
select bt4_private.ranked_refresh_top();
commit;
