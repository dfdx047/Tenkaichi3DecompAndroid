#!/usr/bin/env python3
"""Package reviewable R4 database changes, without secrets or production data."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[2]
MIGRATION = 'supabase/migrations/202611060001_online_route_gate.sql'


def files():
    body = (ROOT / MIGRATION).read_text(encoding='utf-8').strip()
    assert body.startswith('begin;') and body.endswith('commit;')
    body = body[len('begin;'):-len('commit;')]
    preflight = '''begin;
-- Run only on an existing BT4 signaling project. No base-schema reinstall.
do $preflight$ begin
  if to_regprocedure('public.bt4_api(text,jsonb)') is null
     or to_regclass('bt4_private.online_presence') is null
     or to_regprocedure('public.bt4_room_join_v2(text,text,text,text,text)') is null then
    raise exception 'The current six-migration BT4 signaling contract is required';
  end if;
end $preflight$;
'''
    result = {}
    for project, backend in (('PRIMARY', 0), ('RESERVE', 1)):
        initial = f'''
-- Initial preparation only. Re-running never resets an activated epoch/lease.
update bt4_private.route_config set backend_id={backend}{',admission_open=false' if backend else ''}
where singleton and epoch=1 and not require_update and admission_until is null;
do $check$ begin
  if not exists(select 1 from bt4_private.route_config where singleton and backend_id={backend}) then
    raise exception 'Wrong backend identity: inspect the project before applying';
  end if;
end $check$;
commit;
select public.bt4_route_health();
'''
        result['APPLY_R4_'+project+'.sql'] = (preflight+body+initial).encode('utf-8')
    paths = [MIGRATION, 'supabase/R4-DEPLOYMENT.md', 'supabase/online-routing.json',
             'port/tools/online_authority.py', 'port/tools/update_key.py',
             'port/src/update_key.h', '.github/workflows/online-routing.yml']
    for name in paths:
        result[name] = (ROOT/name).read_bytes()
    result['README.txt'] = (
        'BT4decomp 0.1.39.5 R4: online deployment handoff\n\n'
        'Prepared for review. No production migration or minimum-version cutover\n'
        'is implied by this archive. Read supabase/R4-DEPLOYMENT.md first.\n'
        'PRIMARY and RESERVE scripts contain the same additive migration plus\n'
        'guarded initial backend setup. Apply each only to its named project.\n'
        'The route configuration remains disabled until explicit post-release\n'
        'activation. Do not run either script in the game installer.\n'
        'No passwords, management tokens, user sessions or private keys included.\n'
    ).encode()
    result['SHA256SUMS.json'] = json.dumps({name: hashlib.sha256(data).hexdigest()
        for name, data in sorted(result.items())}, indent=2).encode()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.out, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in files().items():
            archive.writestr('BT4decomp-R4-Supabase/'+name, data)
    print(args.out)


if __name__ == '__main__':
    main()
