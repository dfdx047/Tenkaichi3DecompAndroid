"""Refresh one unpublished release by ID, including drafts with temporary tags."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
from urllib.parse import urlencode, urlsplit
from urllib.request import Request, urlopen


def select_draft(releases, tag, title):
    names = {title, title.removeprefix('BT4decomp ')}
    matches = [r for r in releases if r['tag_name'] == tag or r['name'] in names]
    if len(matches) > 1:
        raise ValueError('Multiple releases match this candidate; refusing to duplicate it')
    if matches and not matches[0]['draft']:
        raise ValueError('Refusing to replace a published release')
    return matches[0] if matches else None


def request(method, url, data=None, binary=False):
    headers = {'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
               'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28'}
    if data is not None:
        if binary:
            headers['Content-Type'] = 'application/octet-stream'
        else:
            headers['Content-Type'] = 'application/json'
            data = json.dumps(data).encode()
    with urlopen(Request(url, data=data, headers=headers, method=method), timeout=180) as response:
        body = response.read()
    return json.loads(body) if body else None


def refresh(repository, tag, title, commit, notes, assets, api=request):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository):
        raise ValueError('Invalid repository')
    base = 'https://api.github.com/repos/' + repository
    releases = []
    for page in range(1, 11):
        batch = api('GET', base + '/releases?' + urlencode({'per_page': 100, 'page': page}))
        releases.extend(batch)
        if len(batch) < 100:
            break
    else:
        raise ValueError('Release listing exceeded its review limit')
    old = select_draft(releases, tag, title)
    fields = dict(tag_name=tag, name=title, target_commitish=commit, body=notes, draft=True)
    if old:
        fields.pop('draft')  # Do not change an existing release's publication status.
    release = api('PATCH' if old else 'POST',
                  base + '/releases' + ('/' + str(old['id']) if old else ''), fields)
    if not release['draft'] or (old and release['id'] != old['id']):
        raise ValueError('Release identity or draft status changed')
    release_url = base + '/releases/' + str(release['id'])
    existing = api('GET', release_url)['assets']
    expected = {p.name: p for p in assets}
    if len(expected) != len(assets) or len({a['name'] for a in existing}) != len(existing):
        raise ValueError('Duplicate asset names')
    if any(a['name'] not in expected for a in existing):
        raise ValueError('Unexpected assets require review before replacing this draft')
    upload = release['upload_url'].split('{', 1)[0]
    parts = urlsplit(upload)
    if parts.scheme != 'https' or parts.netloc != 'uploads.github.com' or parts.path != '/repos/' + repository + '/releases/' + str(release['id']) + '/assets':
        raise ValueError('Unexpected asset upload endpoint')
    for name, path in expected.items():
        content = path.read_bytes()
        digest = 'sha256:' + hashlib.sha256(content).hexdigest()
        found = next((a for a in existing if a['name'] == name), None)
        if found and found.get('digest') == digest and found.get('state') == 'uploaded':
            continue
        if not api('GET', release_url)['draft']:
            raise ValueError('Release was published during upload; stopping')
        if found:
            api('DELETE', base + '/releases/assets/' + str(found['id']))
        result = api('POST', upload + '?' + urlencode({'name': name}), content, binary=True)
        if result.get('digest') != digest or result['size'] != len(content):
            raise ValueError('Uploaded asset digest or size differs: ' + name)
    final = api('GET', release_url)
    if not final['draft'] or final['name'] != title or {a['name'] for a in final['assets']} != set(expected):
        raise ValueError('Incomplete draft release')
    for asset in final['assets']:
        if asset['digest'] != 'sha256:' + hashlib.sha256(expected[asset['name']].read_bytes()).hexdigest():
            raise ValueError('Final asset differs: ' + asset['name'])
    return final


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--notes', type=Path, required=True)
    parser.add_argument('--title', required=True)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    names = ['BT4decomp-' + platform + '.zip' + suffix
             for platform in ('windows-x64', 'linux-x64', 'macos-arm64') for suffix in ('', '.sig')]
    assets = [args.assets / name for name in names]
    if not all(p.is_file() and p.stat().st_size > 0 for p in assets):
        raise SystemExit('Missing release assets')
    result = refresh(os.environ['GITHUB_REPOSITORY'], args.tag, args.title, args.commit,
                     args.notes.read_text(encoding='utf8'), assets)
    print(json.dumps({'release_id': result['id'], 'draft': result['draft'],
                      'assets': len(result['assets']), 'url': result['html_url']}))
