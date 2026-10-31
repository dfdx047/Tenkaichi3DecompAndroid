"""Capture native status frames on real disc assets, with isolated settings.

The four status flags are a HUD-only fixture: attack/defense/ki buffs and ki
sickness. No texture or position is fabricated by this test. Inspect the PNG
alongside the source disc's HudGauge_UpdateStatIcons/Init layout.
"""
import argparse
import os
from pathlib import Path
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--disc',default='gamedata-bt4')
    p.add_argument('--views',type=int,default=2,choices=(1,2,3,4))
    p.add_argument('--hide',action='store_true')
    p.add_argument('--exe',default='port/build/bt3.exe' if os.name=='nt' else 'port/build/bt3_64')
    p.add_argument('--out',default='port/build/hud-status-fix')
    args=p.parse_args()
    out=ROOT/args.out/(args.disc+f'-{args.views}p'+('-hidden' if args.hide else ''))
    out.mkdir(parents=True,exist_ok=True)
    settings=out/'settings.txt'
    settings.write_text('tag_extra_intros=0\n'+('tag_hide_all_hud=1\n' if args.hide else ''))
    shot=out/'hud.ppm';shot.unlink(missing_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_GS='gpu',BT3_DEMO='1',BT3_WINDOW='1280x720',BT3_FULLSCREEN='0',
        BT3_PACED='1',BT3_SETTINGS=str(settings),BT3_SAVES=str(out/'saves'),
        BT3_TAG_TEAM='1',BT3_TAG_PLAY=str(args.views),BT3_TAG_VIEWS=str(args.views),
        BT3_TAG_ROSTER='2,2',BT3_DATA=str(ROOT/args.disc),BT3_NOSOUND='1',
        BT3_UPDATE_CHECK='0',BT3_TEST_STATUS_ICONS='1',BT3_TEST_FRAMES='700',
        BT3_UI_SHOT='650:'+str(shot))
    with (out/'runtime.log').open('w') as log:
        result=subprocess.run([str(ROOT/args.exe)],cwd=ROOT,env=env,
            stdout=log,stderr=subprocess.STDOUT,timeout=120)
    assert result.returncode==0,(result.returncode,out)
    assert shot.is_file(),out
    with Image.open(shot) as im:im.save(out/'hud.png')
    print(out/'hud.png',flush=True)

if __name__=='__main__':main()
