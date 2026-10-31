"""Replay the real native three-player fusion with framebuffer captures."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--seats',default='0,1')
p.add_argument('--views',default='3')
p.add_argument('--renderer',default='gpu')
p.add_argument('--local',choices=['0','1'],default='1')
a=p.parse_args()
out=ROOT/'port/build/oct20-cinematics'/f'fusion-{a.views}-{a.seats.replace(",","-")}-local{a.local}'
out.mkdir(parents=True,exist_ok=True)
settings=out/'settings.txt'
settings.write_text(f'tag_extra_intros=0\ntag_local_cinematics={a.local}\n')
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS=a.renderer,BT3_DEMO='1',BT3_SETTINGS=str(settings),BT3_SAVES=str(out/'saves'),
           BT3_TAG_TEAM='1',BT3_TAG_PLAY=a.views,BT3_TAG_VIEWS=a.views,BT3_TAG_ROSTER='2,2',
           BT3_TAG_FUSION_SEATS=a.seats,BT3_TAG_TEST_FUSION='3',BT3_TAG_TRACE='1',
           BT3_WINDOW='960x720',BT3_UNCAPPED='1',SDL_AUDIO_DRIVER='dummy',BT3_UPDATE_CHECK='0')
if a.renderer=='gpu':env.update(BT3_SHOT='20',BT3_SHOT_FROM='600',BT3_SHOT_TO='1300')
started=time.time_ns()
with (out/'runtime.log').open('w') as f:
    result=subprocess.run([str(ROOT/'port/build/bt3.exe')],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
if a.renderer=='gpu':
    for src in (ROOT/'port/build/shots').glob('gpu_*.ppm'):
        if src.stat().st_mtime_ns>=started:shutil.copy2(src,out/src.name)
assert result.returncode==0 and 'PASS check=99' in (out/'runtime.log').read_text(),out/'runtime.log'
print('PASS fusion',a.views,a.seats)
