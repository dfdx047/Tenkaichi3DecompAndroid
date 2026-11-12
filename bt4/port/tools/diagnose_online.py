"""Print aggregate project diagnostics without raw log events or SQL text."""
import json,os,re
from datetime import datetime,timedelta,timezone
from urllib.parse import urlencode
from urllib.request import Request,urlopen
from urllib.error import HTTPError

now=datetime.now(timezone.utc)
window={'iso_timestamp_start':(now-timedelta(minutes=40)).strftime('%Y-%m-%dT%H:%M:%SZ'),'iso_timestamp_end':now.strftime('%Y-%m-%dT%H:%M:%SZ')}
for name,ref,key in [('secondary','houhjsfukkkicjejvett','SUPABASE_SECONDARY_MANAGEMENT_TOKEN'),
                     ('primary','mhgksaiitqoijnatpurf','SUPABASE_PRIMARY_MANAGEMENT_TOKEN')]:
    headers={'Authorization':'Bearer '+os.environ[key],'Content-Type':'application/json'}
    logs="""select source, multiIf(
      positionCaseInsensitive(event_message,'statement timeout')>0,'statement_timeout',
      positionCaseInsensitive(event_message,'deadline exceeded')>0,'deadline_exceeded',
      positionCaseInsensitive(event_message,'too many database timeouts')>0,'database_pool_timeouts',
      positionCaseInsensitive(event_message,'timeout')>0,'other_timeout',
      'other_error') as category,count(*) as occurrences
      from logs
      where source in ('postgres_logs','auth_logs','realtime_logs','postgrest_logs')
      and (positionCaseInsensitive(event_message,'error')>0
      or positionCaseInsensitive(event_message,'fatal')>0
      or positionCaseInsensitive(event_message,'timeout')>0)
      group by source,category order by occurrences desc"""
    probes=[('project','',None),('health','/health?services=auth,rest,realtime,db',None),('disk','/config/disk/util',None),('logs','/analytics/endpoints/logs?'+urlencode({**window,'sql':logs}),None),('database','/database/query',{'query':"select state,wait_event_type,count(*) from pg_stat_activity group by state,wait_event_type;"})]
    if name=='secondary':
        probes += [
            ('ranked_settlements','/database/query',{'query':"""with recent as (
              select m.*, (select count(*) from bt4_private.ranked_reports r where r.match_id=m.id) reports
              from bt4_private.ranked_matches m where m.admitted_at>now()-interval '12 hours'
              ) select case when completed_at is not null and forfeit_by is not null then 'forfeit'
                  when completed_at is not null then 'result' when abandonment_void then 'void'
                  when disputed then 'disputed' else 'open' end settlement,
              (started_a is not null)::int+(started_b is not null)::int started,reports,count(*) as matches,
              count(*) filter(where forfeit_by=winner) forfeiter_won,
              count(*) filter(where forfeit_by is not null and
                (case when forfeit_by=player_a then seen_a-seen_b else seen_b-seen_a end)>interval '2 seconds') as forfeiter_seen_later,
              count(*) filter(where completed_at-admitted_at<interval '45 seconds') as quick_settlements
              from recent group by 1,2,3 order by 1,2,3"""}),
            ('metrics','/analytics/endpoints/metrics',None),
            ('hot_queries','/database/query',{'query':"""select queryid,calls,
              round(total_exec_time::numeric) total_ms,round(mean_exec_time::numeric,2) mean_ms,
              rows,shared_blks_read,temp_blks_written,
              case when query like '%"bt4_ranked_api"%' then 'ranked'
                   when query like '%"bt4_api"%' then 'discovery'
                   when query like '%realtime.authorize%' then 'realtime_authorization'
                   else 'other' end as category
              from extensions.pg_stat_statements order by total_exec_time desc limit 12"""}),
            ('active_work','/database/query',{'query':"""select state,wait_event_type,wait_event,
              count(*) as connections,round(max(extract(epoch from clock_timestamp()-query_start))) as oldest_seconds
              from pg_stat_activity where pid<>pg_backend_pid() and state is distinct from 'idle'
              and query<>'' group by state,wait_event_type,wait_event order by connections desc limit 12"""})
        ]
    for label,path,body in probes:
        request=Request('https://api.supabase.com/v1/projects/'+ref+path,
                        data=json.dumps(body).encode() if body else None,headers=headers)
        try:
            with urlopen(request,timeout=25) as response:
                if label=='metrics':
                    prefixes=('node_load','node_memory_MemAvailable','node_memory_MemTotal',
                              'node_memory_Swap','node_pressure_','node_cpu_seconds','node_disk_io_time')
                    result=[line for line in response.read().decode().splitlines() if line.startswith(prefixes)]
                else:result=json.load(response)
            if label=='project':result={k:result.get(k) for k in ('id','name','status','region')}
            if label=='health':result=[{k:row.get(k) for k in ('name','healthy','status')} for row in result]
            if label=='logs':result={'result':result.get('result',[]),'query_error':bool(result.get('error'))}
            safe=json.dumps(result)
            safe=re.sub(r'[\w.+-]+@[\w.-]+','[email]',safe)
            safe=re.sub(r'\b(?:\d{1,3}\.){3}\d{1,3}\b','[ip]',safe)
            safe=re.sub(r'eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+','[jwt]',safe)
            print(name,label,safe[:18000],flush=True)
        except HTTPError as exc:
            print(name,label,'HTTP '+str(exc.code),flush=True)
        except Exception as exc:print(name,label,type(exc).__name__,flush=True)
