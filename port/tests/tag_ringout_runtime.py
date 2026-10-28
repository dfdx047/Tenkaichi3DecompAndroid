"""Real ground-collision ring-outs: permanent defeat, no attacks, CPU takeover."""
import argparse, os, subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--bt4',action='store_true');p.add_argument('--stage',type=int,default=4)
p.add_argument('--capture',action='store_true')
p.add_argument('--headless',action='store_true')
p.add_argument('--views',type=int,default=1,choices=(1,2,3,4))
p.add_argument('--team-size',type=int,default=2,choices=(2,5))
p.add_argument('--exe',default='port/build/bt3.exe' if os.name=='nt' else 'port/build/bt3_64');a=p.parse_args()
out=root/'port/build/oct26/ringout'/('bt4' if a.bt4 else 'bt3')/f'{a.stage}-{a.team_size}v{a.team_size}-{a.views}views';out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='none' if a.headless else 'gpu',BT3_UNCAPPED='1',BT3_WINDOW='800x600',BT3_SETTINGS='',BT3_SAVES=str(out/'saves'),
           BT3_DEMO='1',BT3_TAG_TEAM='1',BT3_TAG_PLAY='1',BT3_TAG_VIEWS=str(a.views),BT3_TAG_ROSTER=f'{a.team_size},{a.team_size}',
           BT3_TEST_STAGE=str(a.stage),BT3_TAG_TEST_CONTROLS='4',BT3_TAG_TRACE='1',BT3_UPDATE_CHECK='0',SDL_AUDIO_DRIVER='dummy')
if a.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
if a.capture:
    shots=out/'shots';shots.mkdir(exist_ok=True)
    env.update(BT3_SHOT='20',BT3_SHOT_FROM='1600',BT3_SHOT_TO='2200',BT3_SHOT_DIR=str(shots))
log=out/'runtime.log'
with log.open('w') as f:
    r=subprocess.run([str(root/a.exe)],cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
text=log.read_text(errors='replace')
checks=(*range(370,390),99)
assert r.returncode==0 and all(f'PASS check={c}' in text for c in checks),(r.returncode,log)
print('PASS ring-out settles, stays eliminated, teammate takeover, finish and victory without tag-in/reset',log)
