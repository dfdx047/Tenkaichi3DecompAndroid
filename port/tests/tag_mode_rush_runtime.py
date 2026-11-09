"""Native authored rush cameras in real tournament/gauntlet matches."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--bt4',action='store_true');p.add_argument('--mode',choices=['team','gauntlet','tournament'],default='gauntlet');p.add_argument('--players',type=int,default=1);p.add_argument('--shared',action='store_true');p.add_argument('--contact',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2]
out=root/'port/build/oct22/mode-rush'/f'{a.mode}-{"bt4" if a.bt4 else "bt3"}-{a.players}-{int(a.shared)}';out.mkdir(parents=True,exist_ok=True)
settings=out/'settings.txt';settings.write_text('tag_controller_check=0\ntag_extra_intros=0\n')
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='gpu',BT3_UNCAPPED='1',BT3_MENU_LANG='en',BT3_WINDOW='800x600',BT3_SETTINGS=str(settings),BT3_SAVES=str(out/'saves'),BT3_UPDATE_CHECK='0',BT3_TAG_TEAM='1',BT3_TAG_TEST_MODE_RUSH=str(2 if a.shared else 1),BT3_TAG_TRACE='1',SDL_AUDIO_DRIVER='dummy')
if a.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
if a.contact:env['BT3_TAG_TEST_CONTACT']='1'
if a.mode=='tournament':
 buttons=[(150,'select')]+[(i,'down') for i in (210,270,330,390)]+[(510,'cross'),(600,'cross'),(690,'cross'),(900,'start')]+[(i,'cross') for i in range(1020,16000,90)]
 env.update(BT3_TAG_MENU_TEST='main',BT3_PAD=','.join(f'{t}:{b}' for t,b in buttons))
else:
 env.update(BT3_DEMO='1',BT3_TAG_TEST_CINEMA='5',BT3_TAG_PLAY=str(a.players),BT3_TAG_ROSTER=f'{max(2,a.players)},2')
 if a.mode=='gauntlet':env['BT3_TAG_TEST_CHALLENGE']='1'
log=out/'runtime.log'
with log.open('w') as f:r=subprocess.run([str(root/'port/build/bt3.exe')],cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
s=log.read_text(errors='replace');assert r.returncode==0 and 'PASS check=99' in s,(r.returncode,log)
assert 'PASS check=2256' in s,log
if a.contact:assert 'PASS check=465' in s,log
print('PASS',out.name,flush=True)
