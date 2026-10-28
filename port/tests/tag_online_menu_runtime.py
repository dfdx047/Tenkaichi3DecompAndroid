"""Drive the actual native online menu with ordinary pad inputs and disc artwork."""
import argparse, os, re, subprocess, time
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--bt4',action='store_true');p.add_argument('--case',default='room');p.add_argument('--window',default='800x600')
p.add_argument('--exe',type=Path,default=root/'port/build/bt3.exe');p.add_argument('--locale',default='en');a=p.parse_args()
cases={
 'home':('',400,False),
 'host':('150:cross,210:cross',450,False),
 'address':('150:down,210:cross',400,False),
 'lobby-entry':('150:select,210:down,250:down,290:down,330:down,350:down,390:cross',650,False),
 'settings-entry':('150:select,210:down,250:down,290:down,330:down,350:down,390:down,430:cross',690,False),
 'modes-bottom':('150:select,210:down,250:down,290:down,330:down,350:down',450,False),
 'main-back':('150:select,210:down,250:down,290:down,330:down,350:down,390:down,430:down,480:cross',690,False),
 'lobby-host-settings':('300:down,350:down,400:down,450:down,500:down,560:cross',750,True),
 'room':('',400,True),
 'fighter':('150:cross,210:right',320,True),
 'cpu-difficulty':('150:down,210:cross,270:cross,330:cross,390:cross,450:cross,510:down',550,True),
 'z-hard':('150:down,210:cross,270:cross,330:cross,390:cross,450:cross,510:down,570:down',610,True),
 'z-hard-save':('150:down,210:cross,270:cross,330:cross,390:cross,450:cross,510:down,570:down,630:cross',730,True),
 'cpu-save':('150:down,210:cross,270:cross,330:cross,390:cross,450:cross,510:down,570:cross',680,True),
 'fighter-save':('150:cross,210:right,270:cross,330:cross,390:cross,450:cross',550,True),
 'stage':('150:down,190:down,230:down,270:down,310:down,370:cross',470,True),
 'music':('150:down,190:down,230:down,270:down,310:down,350:right,410:cross,470:down,530:cross',600,True),
 'z-hard-rules':('150:right,190:down,230:down,270:down,310:down,350:down,410:cross,470:right,530:cross,590:right,650:right',730,True),
 'rules':('150:right,190:down,230:down,270:down,310:down,350:down,410:cross',500,True),
 'options':('150:right,190:down,230:down,270:down,310:down,350:down,410:cross,470:down,530:cross,590:cross,650:cross,710:right',800,True),
 'roles':('150:down,210:square',400,True),
 'return':('150:triangle,210:left,270:cross,350:triangle,450:cross',600,True),
}
pad,end,host=cases[a.case]
out=root/'port/build/tag-online/native-menus'/('bt4' if a.bt4 else 'bt3')/(a.case+'-'+a.window);out.mkdir(parents=True,exist_ok=True)
shot=out/'screen.ppm';shot.unlink(missing_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='gpu',BT3_WINDOW=a.window,BT3_FULLSCREEN='0',BT3_TAG_TEAM='1',BT3_TAG_MENU_TEST='main',BT3_MENU_LANG=a.locale,BT3_UPDATE_CHECK='0',BT3_NOSOUND='1',BT3_PACED='1',BT3_SAVES=str(out/'saves'),BT3_SETTINGS=str(out/'settings.txt'),BT3_PAD=pad,BT3_UI_SHOT=f'{end}:{shot}')
if a.case not in ('settings-entry','lobby-entry','modes-bottom','main-back'):env['BT3_TAG_ONLINE']='1'
if host:env.update(BT3_TAG_HOST='17031',BT3_TAG_TEST_SIZE='5,5')
if a.case=='lobby-host-settings':env['BT3_TAG_PARTY']='1'
if a.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
path=out/'runtime.log'
with path.open('w') as log:
 proc=subprocess.Popen([str(a.exe.resolve())],cwd=root,env=env,stdout=log,stderr=log)
 try:
  deadline=time.monotonic()+90
  while time.monotonic()<deadline:
   assert proc.poll() is None,'Game exited: '+str(path)
   if shot.exists() and shot.stat().st_size>100000:
    try:Image.open(shot).save(out/'screen.png');break
    except OSError:pass
   time.sleep(.1)
  else:raise AssertionError('Menu capture timed out: '+str(path))
 finally:
  if proc.poll() is None:proc.terminate()
  try:proc.wait(timeout=10)
  except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=10)
text=path.read_text(errors='replace')
assert 'crashed:' not in text
expected={'lobby-host-settings':(4,4),'lobby-entry':(81,0),'settings-entry':(4,4),'modes-bottom':(4,1),'main-back':(4,0),'home':(81,0),'host':(81,2),'address':(81,1),
 'z-hard':(82,5),'z-hard-save':(81,2),'cpu-difficulty':(82,5),'cpu-save':(81,2),'room':(81,2),'fighter':(82,0),'fighter-save':(81,2),'stage':(81,4),
 'music':(81,5),'z-hard-rules':(81,6),'rules':(81,6),'options':(81,8),'roles':(81,2),'return':(4,2)}
states=[tuple(map(int,m)) for m in re.findall(r'page=(\d+) state=(\d+),',text)]
assert states[-1]==expected[a.case], (a.case,states[-1],expected[a.case])
if a.case=='z-hard-save':assert 'state=6,-1,4,' in text, text[-2500:]
if a.case=='roles':assert 'state=2,1,2,0,3' in text
if a.case=='fighter-save':assert re.search(r'page=81 state=2,0,2,0,3',text)
with Image.open(shot) as img:
 requested=tuple(map(int,a.window.split('x')))
 # The native window rounds its aspect ratio to thousandths (1280 can be 1279).
 assert img.height==requested[1] and abs(img.width-requested[0])<=1,img.size
print('\n'.join(l for l in text.splitlines() if 'tagmenu' in l or 'tagnet:' in l))
print(out/'screen.png')
