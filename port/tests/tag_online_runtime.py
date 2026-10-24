"""Native rooms using actual player and spectator processes; no external server."""
import os, pathlib, subprocess, time, argparse, re
p=argparse.ArgumentParser()
p.add_argument('--bt4', action='store_true')
p.add_argument('--exe',type=pathlib.Path,help='Native executable to exercise')
p.add_argument('--data',type=pathlib.Path,help='Imported disc folder (including regional profiles)')
p.add_argument('--party', action='store_true', help='Persistent practice lobby transport')
p.add_argument('--late-join', action='store_true', help='Wait for the first practice frame 1200 before the third connection')
p.add_argument('--rejoin', action='store_true', help='Replace the first guest after their practice fighter change')
p.add_argument('--join-in-match', action='store_true', help='Connect the third member during a match, then check the practice return')
p.add_argument('--party-mode',type=int,default=1,choices=range(5))
p.add_argument('--spectators', type=int, default=0)
p.add_argument('--players', type=int, default=2, choices=range(1,11))
p.add_argument('--host-spectator', action='store_true')
p.add_argument('--swap-roles', action='store_true', help='Host and first spectator exchange seats, then swap back after a round')
p.add_argument('--high-fusion', action='store_true', help='Give the fusion pair to Players 9 and 10')
p.add_argument('--frames', type=int, default=600)
p.add_argument('--timeout', type=int, default=90)
p.add_argument('--startup-timeout',type=int,default=600,help='Allow a first-run BT4 content fingerprint to finish')
p.add_argument('--audit', action='store_true')
p.add_argument('--input', action='store_true')
p.add_argument('--lag', action='store_true')
p.add_argument('--size', help='Team sizes; cinematic fixtures default to their authored 2v2 setup, other cases to 5v5')
p.add_argument('--case', type=int, default=0)
p.add_argument('--label', default='latest')
p.add_argument('--rounds', type=int, default=0)
p.add_argument('--watch-camera', action='store_true')
p.add_argument('--gpu', action='store_true')
p.add_argument('--gpu-client', help='Render only this client (for example player9); other clients run headless')
p.add_argument('--capture-ms', type=int, default=0, help='Save each GPU window at this time after startup')
p.add_argument('--sound', action='store_true', help='Exercise the real audio backend')
p.add_argument('--disconnect', choices=['host','guest','watch0'])
p.add_argument('--kick', action='store_true', help='Host removes the first practice guest; other members continue')
p.add_argument('--port', type=int, default=17001)
p.add_argument('--load-delay', type=int, default=0, help='Delay only the guest loading check by this many milliseconds')
a=p.parse_args()
if a.size is None:a.size='2,2' if a.case in (6,7) else '5,5'
root=pathlib.Path(__file__).resolve().parents[2]
exe=(a.exe or root/'port/build/bt3.exe').resolve()
out=root/'port/build/tag-online'/a.label
out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_TAG_TEAM='1', BT3_TAG_ONLINE='1', BT3_TAG_AUTO_READY='1',
           BT3_TAG_TEST_PLAYERS=str(a.players),
           BT3_DATA=str((a.data or root/('gamedata-bt4' if a.bt4 else 'gamedata')).resolve()),
           BT3_NOSOUND='1', BT3_UPDATE_CHECK='0', BT3_TEST_FRAMES='0')
if a.disconnect or a.kick:env['BT3_TAG_SAVE_CHECK']='1'
if a.kick:
    assert a.party and a.players>=3
    env['BT3_TAG_TEST_KICK']='1'
if a.audit:env['BT3_TAG_NET_AUDIT']='1'
if a.party:env['BT3_TAG_PARTY']='1'
if a.late_join or a.join_in_match:env['BT3_TAG_TEST_PLAYERS']='2'
if a.party and a.rounds:env['BT3_TAG_PARTY_FLOW']=str(a.party_mode)
if a.party and a.rounds:env['BT3_TAG_TEST_ROUNDS']=str(a.rounds)
if a.input:env['BT3_TAG_TEST_INPUT']='1'
if a.lag:env['BT3_TAG_TEST_LAG']='1'
if a.swap_roles:env['BT3_TAG_TEST_ROLE_SWAP']='1'
if a.high_fusion:env['BT3_TAG_TEST_HIGH_FUSION']='1'
if a.watch_camera:env['BT3_TAG_NET_WATCH_TEST']='1'
if a.gpu:env['BT3_GS']='gpu'
if a.sound:env.pop('BT3_NOSOUND',None)
env['BT3_TAG_TEST_SIZE']=a.size
if a.case:env['BT3_TAG_NET_CASE']=str(a.case)
if a.case==3:env['BT3_TAG_TEST_BEAM_ASSIST']='1'
if a.case==4:env['BT3_TAG_TEST_MULTI_BEAMS']='1'
if a.case in (6,7):env['BT3_TAG_TEST_CINEMA']=str(a.case-5)
jobs=[];completed=False;disconnected=False;disconnect_at=None;rejoined=False
expected={1:['PASS 105'],2:['PASS 206'],3:['PASS check=99'],4:['PASS check=99'],6:['PASS check=99'],7:['PASS check=99'],8:['PASS 801','PASS 802','PASS 803'],10:['PASS 2510'],12:['PASS 2512']}
if a.party and a.rounds:expected[9]=['PASS 2500','PASS 2501']

def spawn(name):
    e=env.copy();e['BT3_SAVES']=str(out/('saves-'+name));e['BT3_SETTINGS']=str(out/(name+'.settings'))
    # Real hosts run at display speed. An uncapped headless host otherwise
    # creates replay history as quickly as a replacement can consume it.
    if name=='host' and (a.rejoin or a.late_join):e['BT3_PACED']='1'
    if a.gpu_client==name:e['BT3_GS']='gpu'
    if a.capture_ms and (not a.gpu_client or a.gpu_client==name):e['BT3_UI_SHOT_MS']=str(a.capture_ms)+':'+str(out/(name+'.ppm'))
    pathlib.Path(e['BT3_SAVES']).mkdir(exist_ok=True)
    if name=='guest' and a.load_delay:e['BT3_TAG_TEST_LOAD_DELAY_MS']=str(a.load_delay)
    if name=='host' and a.host_spectator:e['BT3_TAG_TEST_HOST_SPECTATOR']='1'
    e['BT3_TAG_HOST' if name=='host' else 'BT3_TAG_WATCH' if name.startswith('watch') else 'BT3_TAG_JOIN']=str(a.port) if name=='host' else '127.0.0.1:'+str(a.port)
    log=out/(('bt4-' if a.bt4 else 'bt3-')+name+'.log')
    f=log.open('w');proc=subprocess.Popen([str(exe)],cwd=root,env=e,stdout=f,stderr=f)
    jobs.append((name,proc,f,log))
    return proc,log

def return_complete(content):
    if not a.party:return content.count('Back in the room.')>=a.rounds
    # Wait for a functioning practice map after the final transition, not
    # merely the message printed before loading starts.
    return (content.count('Loading the match...')>=a.rounds*2+1 and
            'frame 600,' in content.rsplit('Loading the match...',1)[-1])

try:
    roles=['host']+['watch'+str(i) for i in range(a.spectators)]+(['guest'] if a.players>1 or a.host_spectator else [])
    roles+=['player'+str(i) for i in range(2,a.players+int(a.host_spectator))]
    for name in roles:
        if (a.late_join or a.join_in_match) and name=='player2':
            until=time.time()+90
            def join_point(log):
                content=log.read_text(errors='replace')
                return ('frame 600,' in content.rsplit('Loading the match...',1)[-1] and content.count('Loading the match...')==2) if a.join_in_match else 'frame 1200,' in content
            while time.time()<until and not all(join_point(log) for _,_,_,log in jobs):time.sleep(.1)
            assert all(join_point(log) for _,_,_,log in jobs), 'Initial clients did not reach the join point'
        proc,log=spawn(name);time.sleep(0.6)
        if name=='host':
            wait=time.time()+a.startup_timeout
            while proc.poll() is None and time.time()<wait and 'Room open.' not in log.read_text(errors='replace'):time.sleep(.2)
            if proc.poll() is not None or 'Room open.' not in log.read_text(errors='replace'):raise RuntimeError('Host did not open room')
    end=time.time()+a.timeout
    while any(proc.poll() is None for _,proc,_,_ in jobs) and time.time()<end:
        if a.rejoin and not rejoined and all((name=='host' or 'PASS 2510' in log.read_text(errors='replace')) and 'frame 2400,' in log.read_text(errors='replace') for name,_,_,log in jobs):
            next(proc for name,proc,_,_ in jobs if name=='guest').terminate()
            until=time.time()+10
            while time.time()<until and 'A guest left the practice map.' not in jobs[0][3].read_text(errors='replace'):time.sleep(.1)
            assert 'A guest left the practice map.' in jobs[0][3].read_text(errors='replace')
            spawn('replacement');rejoined=True
        party_done=a.party and not a.rounds and not a.disconnect and all('frame 1200,' in log.read_text(errors='replace') for _,_,_,log in jobs)
        # Online battles enter through the room, not the attract demo. Its
        # BT3_TEST_FRAMES counter measures battle-clock changes, accumulates
        # across rounds, and can exit before a room return completes. The
        # harness owns this deadline and waits for every client's actual
        # confirmed frames and the requested gameplay checks instead.
        if not a.party and not a.rounds and not a.disconnect:
            contents=[log.read_text(errors='replace') for _,_,_,log in jobs]
            samples=[re.findall(r'frame (\d+), confirmed (\d+),',s) for s in contents]
            if all(rows and int(rows[-1][1])>=max(600,a.frames) for rows in samples) and all(all(marker in s for marker in expected.get(a.case,[])) for s in contents):
                completed=True;break
        if a.rejoin:party_done=party_done and rejoined
        if party_done and a.case==10:party_done=all((name=='host' or 'PASS 2510' in log.read_text(errors='replace')) and 'frame 2400,' in log.read_text(errors='replace') for name,_,_,log in jobs)
        if party_done and a.case==12:party_done=all((name=='host' or 'PASS 2512' in log.read_text(errors='replace')) and 'frame 1800,' in log.read_text(errors='replace') for name,_,_,log in jobs)
        if party_done and (a.late_join or a.rejoin):
            caught=re.findall(r'caught up to practice frame (\d+)',jobs[0][3].read_text(errors='replace'))
            party_done=bool(caught) and all(max(map(int,re.findall(r'frame (\d+),',log.read_text(errors='replace')) or ['0']))>int(caught[-1])+600 for name,_,_,log in jobs if not(a.rejoin and name=='guest'))
        if party_done:completed=True;break
        if a.rounds and all(return_complete(log.read_text(errors='replace')) for name,_,_,log in jobs if not(a.join_in_match and name=='player2')):
            if not a.join_in_match or 'frame 600,' in jobs[-1][3].read_text(errors='replace'):completed=True;break
        if a.disconnect and not disconnected and all('frame 600,' in log.read_text(errors='replace') for _,_,_,log in jobs):
            victim=next(proc for name,proc,_,_ in jobs if name==a.disconnect);victim.terminate();disconnected=True;disconnect_at=time.time()
        if disconnected and a.disconnect=='watch0' and time.time()-disconnect_at>8:
            assert all(proc.poll() is None for name,proc,_,_ in jobs if name!='watch0'), 'Spectator departure stopped a player'
            completed=True;break
        if disconnected and a.party and a.disconnect!='host' and all('frame 1800,' in log.read_text(errors='replace') for name,_,_,log in jobs if name!=a.disconnect):completed=True;break
        if a.kick and all('frame 1800,' in log.read_text(errors='replace') for name,_,_,log in jobs if name!='guest') and next(proc for name,proc,_,_ in jobs if name=='guest').poll() is not None:completed=True;break
        time.sleep(.2)
finally:
    for name,proc,f,log in jobs:
        timed=proc.poll() is None
        if timed and not completed:print('HARNESS DEADLINE:',name,flush=True)
        if timed:proc.terminate()
        proc.wait();f.close()
        print(name,proc.returncode,'ROUNDS COMPLETE' if completed else 'TIMEOUT' if timed else '',flush=True)
        print('\n'.join(l for l in log.read_text(errors='replace').splitlines() if not l.startswith('audit'))[-3000:],flush=True)

# A zero exit alone is not a pass: a cleanly reported disconnect/desync also exits zero.
for name,proc,_,log in jobs:
    content=log.read_text(errors='replace')
    assert 'FAIL' not in content and 'divergence' not in content and 'went out of sync' not in content, log
    if name==a.disconnect or (a.rejoin and name=='guest'):continue
    if a.kick and name=='guest':
        assert 'The host removed you from the lobby.' in content and 'PASS 2600' in content and proc.returncode==0,log
        continue
    if a.join_in_match and name=='player2':
        assert 'A match is in progress.' in content and 'frame 600,' in content, log
        continue
    if a.rounds:
        assert return_complete(content), log
        for marker in expected.get(a.case,[]):
            if a.party and name=='host' and marker=='PASS 2500':continue
            assert marker in content, (log,marker)
    elif a.disconnect and a.party and a.disconnect!='host':assert 'frame 1800,' in content, log
    elif a.disconnect and a.disconnect!='watch0':assert 'match has stopped' in content or 'Match stopped' in content, log
    elif not a.disconnect:
        assert 'frame 600,' in content, log
        for marker in expected.get(a.case,[]):
            if a.party and name=='host' and marker in ('PASS 2510','PASS 2512'):continue
            assert marker in content, (log,marker)
        if a.lag and a.input and not name.startswith('watch') and not(name=='host' and (a.host_spectator or a.party)):
            assert any(int(v)>0 for v in re.findall(r'rewinds (\d+)',content)), log
    assert proc.returncode==0 or completed or name==a.disconnect, (log,proc.returncode)
    if a.party and name=='host':
        assert 'host remains on native mode menu; no practice battle loaded' in content,log
        first_practice=content.split('host remains on native mode menu; no practice battle loaded',1)[1].split('Choose teams.',1)[0]
        assert 'tagstage: scale=' not in first_practice, 'Host loaded the practice stage'
    if a.disconnect=='host':assert 'PASS 2600' in content,log
if a.disconnect:assert disconnected, 'Disconnect point was not reached'
print('PASS: room clients completed the requested checks',flush=True)
