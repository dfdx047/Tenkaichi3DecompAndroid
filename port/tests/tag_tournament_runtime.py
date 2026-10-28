"""Fill five-fighter teams in the original menus, finish two bracket matches,
and confirm the next match still has the full teams and controller mapping."""
import argparse, os, re, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--bt4',action='store_true');p.add_argument('--four-teams',action='store_true');p.add_argument('--skip',action='store_true');p.add_argument('--easy-opponents',action='store_true');a=p.parse_args()
out=root/'port/build/oct22/tournament'/(('bt4' if a.bt4 else 'bt3')+('-four' if a.four_teams else '')+('-skip' if a.skip else ''));out.mkdir(parents=True,exist_ok=True)
settings=out/'settings.txt';settings.write_text('tag_controller_check=0\ntag_extra_intros=0\n')
buttons=[(150,'select')]+[(i,'down') for i in (210,270,330,390)]+[(510,'cross'),(600,'cross'),(690,'cross'),(900,'start')]
buttons += [(i,'cross') for i in range(1020,30000,90)]
if a.easy_opponents:
 assert not a.four_teams
 # Opponent difficulty is its own card; selected teammates still default
 # to Normal on their individual difficulty pages after the outfit picker.
 buttons += [(750,'down'),(780,'cross'),(810,'left'),(840,'left'),(870,'cross')]
if a.four_teams:
 buttons=[(150,'select')]+[(i,'down') for i in (210,270,330,390)]+[(510,'cross'),(600,'down'),(660,'down'),(720,'down'),(780,'cross'),(870,'cross')]
 tick=960
 for player in (1,2,3):
  for key in ['down','cross']+['right']*player+['cross']:
   buttons.append((tick,key));tick+=60
 buttons.append((tick,'start'));tick+=180
 buttons += [(i,'cross') for i in range(tick,45000,90)]
if a.skip:
 # Finish the form/outfit/CPU-level confirmations before pressing Square.
 # BT4 adds a form page to each of the five selected fighters.
 start=12450 if a.four_teams else 3900 if a.bt4 else 3450
 buttons=[(t,b) for t,b in buttons if t<start or t>start+2400]
 buttons += [(i,'square') for i in range(start,start+2400,45)]
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='gpu',BT3_WINDOW='800x600',BT3_UNCAPPED='1',BT3_SETTINGS=str(settings),BT3_SAVES=str(out/'saves'),BT3_TAG_TEAM='1',
 BT3_TAG_MENU_TEST='main',BT3_MENU_LANG='en',BT3_UPDATE_CHECK='0',BT3_SOUND_TICKS='1',SDL_AUDIO_DRIVER='dummy',BT3_SND_VERBOSE='1',
 BT3_TAG_TEST_TOURNAMENT='1',BT3_PAD=','.join(f'{f}:{b}' for f,b in sorted(buttons)))
env['BT3_UI_SHOT']=f'{8420 if a.bt4 else 7250}:{out/"crowd.ppm"}'
if a.easy_opponents:env['BT3_UI_SHOT']=f'890:{out/"setup.ppm"}'
if a.four_teams:
 for p in (2,3,4):env[f'BT3_PAD_{p}']=','.join(f'{t}:{b}' for t,b in buttons if t>2000 and b=='cross')
if a.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
log=out/'runtime.log'
with log.open('w') as f:
 r=subprocess.run([str(root/'port/build/bt3.exe')],cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=480)
text=log.read_text(errors='replace')
assert r.returncode==0 and 'PASS check=99' in text,(r.returncode,log)
cuts=re.findall(r'page=84 state=4,4,(\d),0,0',text)
assert cuts==['0','1','2']*2, ('Missing tournament crowd introduction',cuts,log)
print('PASS native tournament menu, two round returns, next match',log)

if a.four_teams:assert 'page=87 state=0,0,4,0,4' in text,log
if a.skip:assert 'page=88 state=1,' in text,log
if a.easy_opponents:
 assert 'page=47 state=1,-1,3,0,1' in text,log
 assert 'PASS check=2202' in text and 'FAIL' not in text,log
