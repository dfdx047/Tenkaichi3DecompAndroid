"""Deploy the scoring RPC without changing the route or restarting a service."""

import time

from rollout_online_nicknames import ROOT, authority, query


MIGRATION = ROOT / 'supabase/migrations/20261109230000_ranked_verified_abandonment.sql'
VERIFY = """select
  (select count(*) from information_schema.columns where table_schema='bt4_private' and table_name='ranked_matches'
   and column_name in ('quit_a','quit_b'))=2 as columns,
  position('local_quit' in pg_get_functiondef('public.bt4_ranked_api(text,jsonb)'::regprocedure))>0 as scorer,
  has_function_privilege('authenticated','public.bt4_ranked_api(text,jsonb)','execute') as authenticated"""


def main():
    migration = MIGRATION.read_text(encoding='utf-8')
    for project in authority.PROJECTS:
        before_status, before = authority.health(project)
        if before_status != 200:
            raise RuntimeError(f"{project['id']} health returned HTTP {before_status}")
        for attempt in range(3):
            if all(query(project, VERIFY)[0].values()):
                break
            try:
                query(project, migration)
            except RuntimeError:
                if attempt == 2:
                    raise
                time.sleep(2)
        checks = query(project, VERIFY)[0]
        if not all(checks.values()):
            raise RuntimeError(f"{project['id']} scorer verification failed")
        after_status, after = authority.health(project)
        if after_status != 200 or any(before[key] != after[key] for key in ('epoch', 'admission_open', 'minimum')):
            raise RuntimeError(f"{project['id']} route changed during rollout")
        print(project['id'], 'scorer verified', f"epoch={after['epoch']}", f"admission_open={after['admission_open']}", flush=True)


if __name__ == '__main__':
    main()
