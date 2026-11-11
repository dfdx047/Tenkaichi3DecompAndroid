"""Signed, fenced matchmaking routing. Defaults to a read-only plan.

No database credentials or user sessions are stored in the policy/state files.
The two management tokens are separate Actions secrets for the two accounts.
Only admission configuration is changed. Existing rooms and P2P are untouched.
"""
from __future__ import annotations
import argparse,base64,hashlib,json,os,pathlib,re,sys,time,urllib.error,urllib.request
from datetime import datetime
import update_key

ROOT=pathlib.Path(__file__).resolve().parents[2]
REPOSITORY='NauTaro/DBZ-BT4-Tag-Team-Decomp'
PROJECTS=(
 dict(id='primary',ref='mhgksaiitqoijnatpurf',key='sb_publishable_T2WjPRgHLMAVgRC6juKLqg_BoKiSgww',secret='SUPABASE_PRIMARY_MANAGEMENT_TOKEN'),
 dict(id='secondary',ref='houhjsfukkkicjejvett',key='sb_publishable_JZJl6DjcnWM6M3FyqF8Ojw_Gn-HHHPN',secret='SUPABASE_SECONDARY_MANAGEMENT_TOKEN'),
)
DOMAIN=b'BT4-ONLINE-ROUTE-V1\n'
PLATFORMS=('windows-x64','linux-x64','macos-arm64')

class AuthorityError(RuntimeError):pass

def request(url,body=None,headers=None,limit=65536,timeout=12):
    req=urllib.request.Request(url,data=None if body is None else json.dumps(body).encode(),
        headers={'User-Agent':'BT4 online authority','Content-Type':'application/json',**(headers or {})})
    # Credentials are never included in exception text or request diagnostics.
    try:
        with urllib.request.urlopen(req,timeout=timeout) as response:
            data=response.read(limit+1)
            if len(data)>limit:raise AuthorityError('Response exceeded its size bound')
            return response.status,data
    except urllib.error.HTTPError as e:return e.code,b''
    except (OSError,TimeoutError):return 0,b''

def global_failure(status):
    # A per-user limit, authentication rejection or 403 is never route evidence.
    return status==0 or status==402 or 500<=status<=599

def config(path):
    c=json.loads(path.read_text())
    if set(c)!={'schema','enabled','minimum','release_tag','require_update','capability','policy_seconds','admission_seconds','automatic_return'}:raise AuthorityError('Invalid authority configuration fields')
    if c['schema']!=1 or type(c['enabled']) is not bool or type(c['require_update']) is not bool or type(c['automatic_return']) is not bool:raise AuthorityError('Invalid authority switches')
    if not re.fullmatch(r'[0-9]{1,4}(?:\.[0-9]{1,4}){2,3}-R[0-9]{1,4}(?:\.[0-9]{1,4})?',c['minimum']):raise AuthorityError('Invalid minimum version')
    if c['release_tag']!='v'+c['minimum']:raise AuthorityError('Release tag must match the minimum')
    if c['capability']!=1 or not 300<=c['policy_seconds']<c['admission_seconds']<=86400:raise AuthorityError('Invalid compatibility or lease bounds')
    return c

def read_policy(path,public):
    if not path.exists():return None
    raw=path.read_bytes()
    if len(raw)>8192:raise AuthorityError('Oversized prior policy')
    env=json.loads(raw);payload=env['payload'].encode();sig=bytes.fromhex(env['signature'])
    if not update_key.verify(public,DOMAIN+payload,sig):raise AuthorityError('Prior policy signature is invalid')
    p=json.loads(payload)
    if set(p)!={'schema','epoch','issued_at','expires_at','capability','active','minimum','require_update'} or p['schema']!=1 or p['active'] not in ('primary','secondary'):raise AuthorityError('Invalid prior policy')
    if type(p['epoch']) is not int or not 1<=p['epoch']<=2147483647:raise AuthorityError('Invalid prior epoch')
    return p

def health(project):
    status,raw=request(f"https://{project['ref']}.supabase.co/rest/v1/rpc/bt4_route_health",{},headers={'apikey':project['key']})
    if status!=200:return status,None
    try:
        h=json.loads(raw)
        if h['schema']!=1 or type(h['epoch']) is not int or h['backend_id']!=PROJECTS.index(project):return 422,None
        return 200,h
    except (ValueError,KeyError,TypeError):return 422,None

def service_health(project):
    status,route=health(project)
    if status!=200:return status,route
    auth,_=request(f"https://{project['ref']}.supabase.co/auth/v1/health",headers={'apikey':project['key']})
    return auth,route

def route_config_matches(project,epoch,admission,until,c):
    status,h=health(project)
    if status!=200:return False
    try:
        actual_until=int(datetime.fromisoformat(h['admission_until']).timestamp())
        return (h['epoch']==epoch and h['backend_id']==PROJECTS.index(project) and
            h['admission_open'] is admission and h['require_update'] is c['require_update'] and
            h['minimum']==c['minimum'] and actual_until==int(until))
    except (ValueError,TypeError,KeyError):return False

def write_config(project,epoch,admission,until,c):
    token=os.environ.get(project['secret'],'')
    if not token:raise AuthorityError('A required project management secret is missing')
    # All interpolated SQL values have been bounded here or in config().
    if type(epoch) is not int or not 1<=epoch<=2147483647:raise AuthorityError('Invalid write epoch')
    index=PROJECTS.index(project)
    sql=f"""update bt4_private.route_config set backend_id={index},epoch={epoch},
      admission_open={'true' if admission else 'false'},admission_until=to_timestamp({int(until)}),
      require_update={'true' if c['require_update'] else 'false'},minimum_client='{c['minimum']}',minimum_capability=1
      where singleton and epoch<={epoch}
      returning epoch,backend_id,admission_open,extract(epoch from admission_until)::bigint admission_until"""
    status,raw=request(f"https://api.supabase.com/v1/projects/{project['ref']}/database/query",{'query':sql},
        headers={'Authorization':'Bearer '+token},timeout=20)
    # A committed SQL receipt can fence an endpoint whose public REST service
    # is down. Empty results and ambiguous management failures are insufficient.
    if not admission and 200<=status<300:
        try:
            rows=json.loads(raw)
            if (isinstance(rows,list) and len(rows)==1 and rows[0]['epoch']==epoch and
                rows[0]['backend_id']==index and rows[0]['admission_open'] is False and
                int(rows[0]['admission_until'])==int(until)):
                return True
        except (ValueError,KeyError,TypeError):pass
    return route_config_matches(project,epoch,admission,until,c)

def fence_previous(project,epoch,old_until,c,now):
    if type(old_until) is int and now>=old_until+30:
        return old_until
    if write_config(project,epoch,False,now,c):
        return now
    # If a verified SQL close was just published, allow its short clock margin
    # to expire. An older 24-hour lease is never shortened on an unknown result.
    if type(old_until) is int and 0<old_until+30-now<=30:
        time.sleep(old_until+30-now)
        return old_until
    raise AuthorityError('Previous route is not fenced; keeping standby admission closed')

def verify_release(c,public,fetch=request):
    headers={}
    if os.environ.get('GITHUB_TOKEN'):headers['Authorization']='Bearer '+os.environ['GITHUB_TOKEN']
    status,raw=fetch(f"https://api.github.com/repos/{REPOSITORY}/releases/tags/{c['release_tag']}",headers=headers)
    if status!=200:raise AuthorityError('The required release is not accessible')
    release=json.loads(raw)
    if release.get('draft') or release.get('prerelease') or release.get('tag_name')!=c['release_tag'] or not release.get('published_at'):raise AuthorityError('The required release must already be public and stable')
    assets={a['name']:a for a in release.get('assets',[])}
    checked={}
    for platform in PLATFORMS:
        name=f'BT4decomp-{platform}.zip';asset=assets.get(name);signature=assets.get(name+'.sig')
        if not asset or not signature or not 1024<asset.get('size',0)<250_000_000:raise AuthorityError('A required signed platform package is missing')
        # Re-download and verify the actual bytes, not merely the asset's name.
        archive_url=f'https://github.com/{REPOSITORY}/releases/download/{c["release_tag"]}/{name}'
        a,data=fetch(archive_url,limit=250_000_000,timeout=120)
        b,sig=fetch(archive_url+'.sig',limit=512)
        if a!=200 or b!=200 or len(data)!=asset['size']:raise AuthorityError('A public update package could not be fully downloaded')
        text=sig.decode('ascii').strip()
        if not re.fullmatch('[0-9a-fA-F]{128}',text) or not update_key.verify(public,data,bytes.fromhex(text)):raise AuthorityError('A public update package failed signature verification')
        digest=hashlib.sha256(data).hexdigest()
        if asset.get('digest') and asset['digest']!='sha256:'+digest:raise AuthorityError('A public package digest differs from GitHub metadata')
        checked[name]=digest
    return checked

def choose(previous,statuses,requested='auto'):
    active=previous['active'] if previous else 'primary'
    if requested in ('primary','secondary'):
        return requested if statuses[requested]==200 else active
    if statuses[active]==200:return active
    other='secondary' if active=='primary' else 'primary'
    if global_failure(statuses[active]) and statuses[other]==200:return other
    return active

def read_signed(path,public,domain):
    if not path.exists():return None
    env=json.loads(path.read_text());payload=env['payload'].encode()
    if len(payload)>8192 or not update_key.verify(public,domain+payload,bytes.fromhex(env['signature'])):raise AuthorityError('An authority record has an invalid signature')
    return json.loads(payload)

def save_signed(path,value,seed,domain):
    payload=json.dumps(value,sort_keys=True,separators=(',',':'))
    envelope={'payload':payload,'signature':update_key.sign(seed,domain+payload.encode()).hex()}
    path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_suffix('.tmp');temp.write_text(json.dumps(envelope,indent=2)+'\n');temp.replace(path)

def github(path,body=None,method=None):
    token=os.environ.get('GITHUB_TOKEN','')
    if not token:raise AuthorityError('Repository token is missing')
    req=urllib.request.Request(f'https://api.github.com/repos/{REPOSITORY}/'+path,
        data=None if body is None else json.dumps(body).encode(),method=method,
        headers={'User-Agent':'BT4 online authority','Authorization':'Bearer '+token,'Content-Type':'application/json','Accept':'application/vnd.github+json'})
    try:
        with urllib.request.urlopen(req,timeout=15) as response:return response.status,json.load(response)
    except urllib.error.HTTPError as e:
        if e.code==404:return 404,{}
        raise AuthorityError('Routing branch API request failed') from None
    except (OSError,ValueError):raise AuthorityError('Routing branch API is unavailable') from None

def routing_files(directory,publish=False):
    names=('policy.json','state.json','release-receipt.json')
    if not publish:
        directory.mkdir(parents=True,exist_ok=True)
        for name in names:
            status,value=github(f'contents/{name}?ref=online-routing')
            if status==404:continue
            if value.get('size',100000)>16384:raise AuthorityError('Oversized routing record')
            data=base64.b64decode(value['content'])
            (directory/name).write_bytes(data)
        return
    public=update_key.header_key()
    read_policy(directory/'policy.json',public)
    for name,domain in [('state.json',b'BT4-ONLINE-LEASES-V1\n'),('release-receipt.json',b'BT4-ONLINE-RELEASE-V1\n')]:
        if read_signed(directory/name,public,domain) is None:raise AuthorityError('Missing signed routing record')
    status,ref=github('git/ref/heads/online-routing');parent=ref.get('object',{}).get('sha') if status==200 else None
    tree=[]
    for name in names:
        raw=(directory/name).read_bytes()
        if len(raw)>16384:raise AuthorityError('Oversized routing record')
        _,blob=github('git/blobs',{'content':base64.b64encode(raw).decode(),'encoding':'base64'})
        tree.append({'path':name,'mode':'100644','type':'blob','sha':blob['sha']})
    _,created_tree=github('git/trees',{'tree':tree})
    identity={'name':'NauTaro','email':'117424539+NauTaro@users.noreply.github.com'}
    _,commit=github('git/commits',{'message':'Refresh signed online routing policy','tree':created_tree['sha'],
        'parents':[parent] if parent else [],'author':identity,'committer':identity})
    if parent:github('git/refs/heads/online-routing',{'sha':commit['sha'],'force':False},method='PATCH')
    else:github('git/refs',{'ref':'refs/heads/online-routing','sha':commit['sha']})

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--config',type=pathlib.Path,default=ROOT/'supabase/online-routing.json')
    ap.add_argument('--state-dir',type=pathlib.Path,required=True)
    mode=ap.add_mutually_exclusive_group()
    mode.add_argument('--prepare',action='store_true')
    mode.add_argument('--activate-published',action='store_true')
    mode.add_argument('--download-state',action='store_true')
    mode.add_argument('--publish-prepared',action='store_true')
    ap.add_argument('--route',choices=('auto','primary','secondary'),default='auto')
    args=ap.parse_args();c=config(args.config);public=update_key.header_key()
    if args.download_state:
        routing_files(args.state_dir);print('Downloaded routing state.');return
    if args.publish_prepared:
        if not c['enabled'] or not c['require_update']:raise AuthorityError('Routing activation is disabled')
        routing_files(args.state_dir,True);print('Published signed routing policy and lease bounds.');return
    prior=read_policy(args.state_dir/'policy.json',public);now=int(time.time())
    if not c['enabled']:
        print('Routing is disabled; no database writes or admission restriction applied.');return
    if not c['require_update']:raise AuthorityError('Coordinated routing requires the capability gate in both projects')
    state_domain=b'BT4-ONLINE-LEASES-V1\n';receipt_domain=b'BT4-ONLINE-RELEASE-V1\n'
    state=read_signed(args.state_dir/'state.json',public,state_domain) or {'schema':1,'admission_until':{}}
    if args.activate_published:
        if not prior or now>=prior['expires_at'] or prior['minimum']!=c['minimum']:raise AuthorityError('A current published policy is required')
        # The workflow pushes these signed lease bounds BEFORE any database write.
        # A failed later commit can therefore never hide a renewed admission lease.
        expected_state=(args.state_dir/'state.json').read_bytes().strip()
        expected_policy=(args.state_dir/'policy.json').read_bytes().strip()
        published=False
        # Read the just-published records from GitHub's authenticated Contents
        # API. The raw-content CDN can continue serving the prior branch version
        # after the write-ahead commit has completed.
        for attempt in range(15):
            status,state_record=github('contents/state.json?ref=online-routing')
            status2,policy_record=github('contents/policy.json?ref=online-routing')
            remote=base64.b64decode(state_record.get('content','')) if status==200 else b''
            remote_policy=base64.b64decode(policy_record.get('content','')) if status2==200 else b''
            if status==200 and status2==200 and remote.strip()==expected_state and remote_policy.strip()==expected_policy:
                published=True
                break
            if attempt<14:time.sleep(2)
        if not published:raise AuthorityError('The write-ahead policy and lease bounds are not published yet')
        selected=prior['active'];epoch=prior['epoch'];other=next(p for p in PROJECTS if p['id']!=selected)
        old_until=state['admission_until'].get(other['id'])
        # Never presume that an unreachable old database stopped accepting rooms.
        # Either its management endpoint acknowledges closure, or its durable
        # published upper lease bound plus clock tolerance must have passed.
        fence_previous(other,epoch,old_until,c,now)
        until=state['admission_until'].get(selected)
        if type(until) is not int or not now<until<=prior['issued_at']+c['admission_seconds']:raise AuthorityError('Invalid published admission lease')
        target=next(p for p in PROJECTS if p['id']==selected)
        if not write_config(target,epoch,True,until,c):raise AuthorityError('Selected route did not acknowledge admission configuration')
        print('Published policy activated; existing rooms were not changed.');return
    active=prior['active'] if prior else 'primary';statuses={p['id']:None for p in PROJECTS};observations={}
    project=next(p for p in PROJECTS if p['id']==active)
    status,h=service_health(project);statuses[active]=status;observations[active]=h
    if global_failure(status):
        for _ in range(2):
            time.sleep(2);status,h=service_health(project);statuses[active]=status;observations[active]=h
            if not global_failure(status):break
    # The reserve is not polled continuously when the active endpoint is healthy.
    if statuses[active]!=200 or args.route!='auto' or prior is None:
        other=next(p for p in PROJECTS if p['id']!=active)
        statuses[other['id']],observations[other['id']]=service_health(other)
    selected=choose(prior,statuses,args.route)
    if statuses[selected]!=200:raise AuthorityError('No verified healthy route is available; no policy published')
    if args.prepare:
        seed=update_key.load_seed(args)
        if update_key.public_key(seed)!=public:raise AuthorityError('Authority signing key does not match released clients')
        # Check published archives before the very first compatibility cutover.
        receipt_path=args.state_dir/'release-receipt.json'
        receipt=read_signed(receipt_path,public,receipt_domain) or {}
        if receipt.get('tag')!=c['release_tag']:
            receipt={'tag':c['release_tag'],'checked_at':now,'sha256':verify_release(c,public)}
        epoch=max([prior['epoch'] if prior else 1]+[h['epoch'] for h in observations.values() if h])
        switching=prior is None or selected!=active
        if switching:epoch+=1
        leases=state.get('admission_until',{}).copy()
        if switching:
            old=next(p for p in PROJECTS if p['id']!=selected)
            leases[old['id']]=fence_previous(old,epoch,leases.get(old['id']),c,int(time.time()))
        now=int(time.time())
        until=now+c['admission_seconds']
        args.state_dir.mkdir(parents=True,exist_ok=True)
        leases[selected]=until
        save_signed(args.state_dir/'state.json',{'schema':1,'admission_until':leases},seed,state_domain)
        save_signed(receipt_path,receipt,seed,receipt_domain)
        save_signed(args.state_dir/'policy.json',dict(schema=1,epoch=epoch,issued_at=now,expires_at=now+c['policy_seconds'],capability=1,
            active=selected,minimum=c['minimum'],require_update=True),seed,DOMAIN)
        print('Signed policy and write-ahead lease bounds prepared; publish them before activation.')
    else:print(json.dumps({'read_only':True,'active':active,'proposed':selected,'http_statuses':statuses}))

if __name__=='__main__':
    try:main()
    except (AuthorityError,ValueError,KeyError,TypeError) as e:sys.exit('Authority stopped: '+str(e))
