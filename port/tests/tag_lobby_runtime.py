"""Exercise the actual 15-body practice simulation and native recovery paths."""
import argparse
import os
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--bt4',action='store_true')
p.add_argument('--gpu',action='store_true')
p.add_argument('--close',action='store_true')
p.add_argument('--exe',default='port/build/bt3.exe' if os.name=='nt' else 'port/build/bt3_64')
args=p.parse_args()
out=root/'port/build/online-lobby'/('bt4' if args.bt4 else 'bt3')
out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='gpu' if args.gpu else 'none',BT3_WINDOW='960x720',BT3_DEMO='1',
           BT3_SETTINGS='',BT3_SAVES=str(out/'saves'),BT3_TAG_TEAM='1',
           BT3_TAG_ROSTER='2,2',BT3_TAG_TEST_LOBBY='1',BT3_LOAD_TRACE='1',
           BT3_TAG_TRACE='1',BT3_UPDATE_CHECK='0',BT3_NOSOUND='1')
if args.close:env['BT3_TAG_TEST_LOBBY']='2'
if args.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
if args.gpu:env['BT3_UI_SHOT_MS']=f'12000:{out/"practice.ppm"}'
log=out/'runtime.log'
with log.open('w') as stream:
    run=subprocess.run([str(root/args.exe)],cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
text=log.read_text(errors='replace')
assert run.returncode==0 and 'PASS check=99' in text,(run.returncode,log)
assert all(f'PASS check={i}' in text for i in range(2400,2423)),log
assert 'FAIL' not in text,log
print('PASS: native practice map, 15 fighters, no host actor, PvP KO/recovery, passive and active dummies, PvP-only score',log)
