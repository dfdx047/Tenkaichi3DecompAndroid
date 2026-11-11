"""Stage and activate nickname registration separately from client publication."""

import argparse
import base64
import json
import os
import re
import tempfile
import time
from pathlib import Path

import online_authority as authority
import update_key

ROOT = Path(__file__).resolve().parents[2]
MIGRATION = ROOT / 'supabase/migrations/20261109183000_online_nicknames_and_ranked_top.sql'
VERSION = re.search(r'^#define BT4_CANONICAL_VERSION "([^"]+)"$',
                    (ROOT / 'port/include/bt4_version.h').read_text(), re.M)[1]


def query(project, sql):
    secret = os.environ.get(project['secret'])
    if not secret:
        raise RuntimeError('Missing management credential for ' + project['id'])
    status, raw = authority.request(f"https://api.supabase.com/v1/projects/{project['ref']}/database/query",
                                    {'query': sql}, headers={'Authorization': 'Bearer ' + secret}, timeout=40)
    if not 200 <= status < 300:
        raise RuntimeError(f"{project['id']} management query returned HTTP {status}")
    rows = json.loads(raw)
    if not isinstance(rows, list):
        raise RuntimeError('Unexpected database response')
    return rows


def staged(project):
    return query(project, "select to_regclass('bt4_private.online_nickname_config') is not null as ready")[0]['ready']


def verify(project):
    rows = query(project, """select enforce,solo_only,
      (select count(*) from bt4_private.online_nicknames) as reservations,
      (select count(*) from bt4_private.ranked_players) as ranked_records,
      exists(select 1 from cron.job where jobname='bt4-ranked-hourly' and active and schedule='*/10 * * * *') as ten_minute_top,
      exists(select 1 from cron.job where jobname='bt4-ranked-weekly' and active and schedule='59 2 * * 1') as weekly_top,
      has_function_privilege('authenticated','public.bt4_api(text,jsonb)','execute') as api_access,
      has_table_privilege('authenticated','bt4_private.online_nicknames','select') as table_access
      from bt4_private.online_nickname_config where singleton""")
    if len(rows) != 1 or not all(rows[0][key] for key in ('ten_minute_top', 'weekly_top', 'api_access')) or rows[0]['table_access']:
        raise RuntimeError('Nickname schema or schedule verification failed on ' + project['id'])
    print(project['id'], json.dumps(rows[0], sort_keys=True), flush=True)
    return rows[0]


def verify_client_admission():
    status, release = authority.github('releases/latest')
    if status != 200 or release['tag_name'] != 'v' + VERSION or release['draft'] or not release['published_at']:
        raise RuntimeError('The compatible game release must be public and latest')
    status, envelope = authority.github('contents/policy.json?ref=online-routing')
    if status != 200:
        raise RuntimeError('The signed route is unavailable')
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / 'policy.json'
        path.write_bytes(base64.b64decode(envelope['content']))
        policy = authority.read_policy(path, update_key.header_key())
    if policy['minimum'] != VERSION or not policy['require_update'] or policy['expires_at'] <= time.time():
        raise RuntimeError('The current signed minimum must point to the compatible game release')
    project = authority.PROJECTS[1]
    status, health = authority.service_health(project)
    if (status != 200 or policy['active'] != 'secondary' or health['epoch'] != policy['epoch'] or
        health['minimum'] != VERSION or not health['admission_open'] or not health['require_update']):
        raise RuntimeError('The Ranked authority has not confirmed the compatible client floor')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('stage', 'activate', 'verify', 'disable'))
    action = parser.parse_args().action
    for project in authority.PROJECTS:
        if authority.service_health(project)[0] != 200 and action == 'stage':
            raise RuntimeError('Project unhealthy; deferring the migration on both routes')
        ready = staged(project)
        if action != 'stage' and not ready:
            raise RuntimeError('Run the staging phase before activation')
    if action == 'stage':
        for project in authority.PROJECTS:
            if not staged(project):
                query(project, MIGRATION.read_text(encoding='utf-8'))
            verify(project)
        return
    if action in ('activate', 'disable'):
        if action == 'activate':
            verify_client_admission()
        switch = 'true' if action == 'activate' else 'false'
        rows = query(authority.PROJECTS[1], f"""update bt4_private.online_nickname_config
          set enforce={switch},solo_only={switch} where singleton returning enforce,solo_only""")
        if rows != [{'enforce': action == 'activate', 'solo_only': action == 'activate'}]:
            raise RuntimeError('Nickname and Ranked admission switches were not confirmed')
    for project in authority.PROJECTS:
        verify(project)


if __name__ == '__main__':
    main()
