"""Real ground-collision ring-outs: permanent defeat, no attacks, CPU takeover."""
import argparse, os, subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--bt4',action='store_true');p.add_argument('--stage',type=int,default=4)
p.add_argument('--exe',default='port/build/bt3.exe' if os.name=='nt' else 'port/build/bt3_64');a=p.parse_args()
out=root/'port/build/oct22/ringout'/('bt4' if a.bt4 else 'bt3')/str(a.stage);out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='gpu',BT3_UNCAPPED='1',BT3_WINDOW='800x600',BT3_SETTINGS='',BT3_SAVES=str(out/'saves'),
           BT3_DEMO='1',BT3_TAG_TEAM='1',BT3_TAG_PLAY='1',BT3_TAG_VIEWS='1',BT3_TAG_ROSTER='2,2',
           BT3_TEST_STAGE=str(a.stage),BT3_TAG_TEST_CONTROLS='4',BT3_TAG_TRACE='1',BT3_UPDATE_CHECK='0',SDL_AUDIO_DRIVER='dummy')
if a.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
log=out/'runtime.log'
with log.open('w') as f:
    r=subprocess.run([str(root/a.exe)],cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
text=log.read_text(errors='replace')
assert r.returncode==0 and all(f'PASS check={c}' in text for c in (*range(370,384),99)),(r.returncode,log)
print('PASS ring-out stays down, no fighting/revival, teammate takeover, team elimination',log)
