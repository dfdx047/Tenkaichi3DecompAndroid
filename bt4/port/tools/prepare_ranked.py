"""Prepare R5 result reporting on both backends without opening Ranked admission."""
import json
import os
import time
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[2]
ACTIVATE = os.environ.get("ACTIVATE_RANKED", "").lower() == "true"
PROJECTS = (("primary", "mhgksaiitqoijnatpurf", "SUPABASE_PRIMARY_MANAGEMENT_TOKEN"),
            ("secondary", "houhjsfukkkicjejvett", "SUPABASE_SECONDARY_MANAGEMENT_TOKEN"))

def query(ref, secret, sql):
    request = Request("https://api.supabase.com/v1/projects/" + ref + "/database/query",
        data=json.dumps({"query": sql}).encode(),
        headers={"Authorization": "Bearer " + secret, "Content-Type": "application/json"}, method="POST")
    for attempt in range(3):
        try:
            with urlopen(request, timeout=60) as response:
                result = json.load(response)
            break
        except HTTPError as exc:
            # Read-only readiness probes can safely retry a transient gateway
            # failure. Never blindly replay a migration after an unknown result.
            if attempt<2 and sql.lstrip().lower().startswith('select ') and exc.code in (429,500,502,503,504,544):
                time.sleep(2**(attempt+1));continue
            raise RuntimeError("Management query failed with HTTP " + str(exc.code)) from None
    if not isinstance(result, list):
        raise RuntimeError("Unexpected Management response")
    return result

for name, ref, key in PROJECTS:
    secret = os.environ.get(key)
    if not secret:
        raise RuntimeError("Missing management credential for " + name)
    state = query(ref, secret, "select exists(select 1 from information_schema.columns where table_schema='bt4_private' and table_name='ranked_config' and column_name='authority_backend') as ready, to_regclass('bt4_private.ranked_players') is not null as foundation")
    if not state[0]["foundation"]:
        raise RuntimeError("Ranked foundation not installed on " + name)
    if not state[0]["ready"]:
        query(ref, secret, (ROOT / "supabase/migrations/202611080005_ranked_admission_reports.sql").read_text())
    hotfix = query(ref, secret, "select exists(select 1 from information_schema.columns where table_schema='bt4_private' and table_name='ranked_matches' and column_name='forfeit_by') as ready")
    if not hotfix[0]["ready"]:
        query(ref, secret, (ROOT / "supabase/migrations/20261108224635_ranked_abandonment.sql").read_text())
    query(ref, secret, (ROOT / "supabase/checks/r5_hotfix_live.sql").read_text())
    print(name + ": forfeits, handshake protection, timeout idempotence and retained ICE checks passed (rolled back)", flush=True)
    lease = query(ref, secret, "select position('30 minutes' in pg_get_functiondef('public.bt4_room_lease(boolean)'::regprocedure))>0 as ready")
    if not lease[0]["ready"]:
        query(ref, secret, (ROOT / "supabase/migrations/20261109021420_room_inactivity_30_minutes.sql").read_text())
    query(ref, secret, (ROOT / "supabase/checks/room_inactivity_live.sql").read_text())
    print(name + ": 30-minute room creation, lease renewal and expired-room checks passed (rolled back)", flush=True)
    result = query(ref, secret, "select enabled,authority_backend,to_regclass('bt4_private.ranked_reports') is not null as reports,has_function_privilege('authenticated','public.bt4_ranked_api(text,jsonb)','execute') as authenticated_api from bt4_private.ranked_config where singleton")
    if len(result) != 1 or result[0]["authority_backend"] != 1 or not result[0]["reports"] or not result[0]["authenticated_api"] or (result[0]["enabled"] and (not ACTIVATE or name != "secondary")):
        raise RuntimeError("Unexpected staged Ranked state on " + name)
    print(name + ": R5 reports ready, authority backend 1, admission disabled", flush=True)

if ACTIVATE:
    name,ref,key=PROJECTS[1];secret=os.environ[key]
    query(ref,secret,(ROOT/'supabase/checks/r5_ranked_live.sql').read_text())
    print('Live database: both formats, two-party scores and idempotent retries passed; fixture rolled back',flush=True)
    rows=query(ref,secret,"""update bt4_private.ranked_config set enabled=true where singleton and authority_backend=1
      and exists(select 1 from bt4_private.route_config where singleton and backend_id=1 and admission_open
      and (admission_until is null or admission_until>clock_timestamp())) returning enabled,authority_backend""")
    if len(rows)!=1 or rows[0]['enabled'] is not True:raise RuntimeError('Secondary route admission is not open; Ranked was not enabled')
    status=query(ref,secret,"""begin;
      select set_config('request.jwt.claim.sub',gen_random_uuid()::text,true);
      select public.bt4_ranked_api('status','{"ruleset":"solo"}') as solo,
             public.bt4_ranked_api('status','{"ruleset":"teams"}') as teams;
      commit;""")
    # Return the only SELECT object with both status values; API may include command rows.
    statuses=next((r for r in status if 'solo' in r and 'teams' in r),None)
    if not statuses or not statuses['solo'].get('enabled') or not statuses['teams'].get('enabled'):
        raise RuntimeError('Ranked status did not confirm both active queues')
    print('Secondary Ranked ACTIVE: solo and native teams, initial rating 1000',flush=True)
