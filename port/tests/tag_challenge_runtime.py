"""Load authored native gauntlet waves through the real fighter/resource loader."""
import argparse
import os
from pathlib import Path
import re
import subprocess

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--bt4',action='store_true');p.add_argument('--all',action='store_true');p.add_argument('--gpu',action='store_true');p.add_argument('--skip-intros',action='store_true');p.add_argument('--index',type=int)
p.add_argument('--exe',default='port/build/bt3.exe' if os.name=='nt' else 'port/build/bt3_64')
p.add_argument('--departure-shots',action='store_true',help='Capture the first wave ending with the GPU renderer')
p.add_argument('--output-root',default='port/build/oct22/gauntlets');args=p.parse_args()
names=re.findall(r'^\{"([^"\n]+)",([01]),\d+,\d+,gauntlet_', (root/'src/battle/tag_gauntlets.inc').read_text(), re.MULTILINE)
names=[name for name,game in names if int(game)==int(args.bt4)]
assert names, 'No gauntlets found; refusing an empty run'
out=root/args.output_root/('bt4' if args.bt4 else 'bt3')/('skip' if args.skip_intros else 'full');out.mkdir(parents=True,exist_ok=True)
indices=[args.index] if args.index is not None else range(len(names)) if args.all else range(min(3,len(names)))
for index in indices:
    env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_GS='gpu' if args.gpu or args.departure_shots else 'none',BT3_WINDOW='960x720',BT3_DEMO='1',BT3_SETTINGS='',
        BT3_SAVES=str(out/'saves'),BT3_TAG_TEAM='1',BT3_TAG_ROSTER='2,2',BT3_TAG_TEST_CHALLENGE=str(index+1),
        BT3_TAG_MENU_TEST='main',BT3_LOAD_TRACE='1',BT3_TAG_TRACE='1',BT3_UPDATE_CHECK='0')
    if args.skip_intros:env['BT3_TAG_TEST_WAVE_SKIP']='1'
    if args.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
    if args.departure_shots:
        shots=out/f'{index}-departure';shots.mkdir(exist_ok=True)
        env.update(BT3_SHOT='15',BT3_SHOT_FROM='1500',BT3_SHOT_TO='1800',BT3_SHOT_DIR=str(shots),BT3_UNCAPPED='1')
    if args.gpu:env['BT3_UI_SHOT']=f'1050:{out/(str(index)+".ppm")}'
    log=out/f'{index}.log'
    with log.open('w') as stream:
        result=subprocess.run([str(root/args.exe)],cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
    text=log.read_text(errors='replace')
    assert result.returncode==0 and 'PASS check=99' in text, (names[index],result.returncode,log)
    assert all(f'PASS check={c}' in text for c in (2150,2151,2155,2156,2157,2170,2171,2172,2173)),log
    if args.skip_intros:
        assert all(f'PASS check={c}' in text for c in (2152,2153,2154)),log
    print('PASS',names[index],flush=True)
