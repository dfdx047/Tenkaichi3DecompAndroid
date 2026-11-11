"""Manual outage recovery, scoped to the backup project. No route/data changes."""
import json,os,time,sys
from urllib.request import Request,urlopen
REF='houhjsfukkkicjejvett'
BASE='https://api.supabase.com/v1/projects/'+REF
HEADERS={'Authorization':'Bearer '+os.environ['SUPABASE_SECONDARY_MANAGEMENT_TOKEN'],'Content-Type':'application/json'}
def call(path,body=None):
    with urlopen(Request(BASE+path,headers=HEADERS,data=None if body is None else json.dumps(body).encode()),timeout=35) as r:
        raw=r.read()
        return json.loads(raw) if raw.strip() else {}
def healthy():
    h=call('/health?services=auth,rest,db')
    print('Secondary services:',json.dumps([{k:x.get(k) for k in ('name','healthy','status')} for x in h]),flush=True)
    return bool(h) and all(x.get('healthy') is True for x in h)
if '--observe' in sys.argv:
    for attempt in range(18):
        try:
            if healthy():
                assert call('/database/query',{'query':'select 1 as responsive'})==[{'responsive':1}]
                print('Secondary service recovery and SQL confirmed',flush=True)
                break
        except Exception as exc:print('Observe:',type(exc).__name__,flush=True)
        time.sleep(10)
    else:raise RuntimeError('Secondary recovery has not completed; no extra restart performed')
elif healthy():
    print('Already healthy: no restart performed',flush=True)
else:
    time.sleep(8)
    if not healthy():
        # One restart only, after two unhealthy independent service checks.
        call('/restart',{})
        print('Secondary restart accepted. Primary and routing were not changed.',flush=True)
        for attempt in range(12):
            time.sleep(10)
            try:
                if healthy():
                    rows=call('/database/query',{'query':'select 1 as responsive'})
                    assert rows==[{'responsive':1}],rows
                    print('Secondary Auth, REST and DB recovered; SQL probe passed',flush=True)
                    break
            except Exception as exc:
                print('Recovery check:',type(exc).__name__,flush=True)
        else:raise RuntimeError('Secondary service did not recover within the bounded recovery window')
