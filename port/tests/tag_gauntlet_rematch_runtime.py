"""Choose Gauntlet in the real menu, finish its waves and choose Fight Again."""
import argparse,os,subprocess,re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--bt4',action='store_true');p.add_argument('--music',action='store_true');a=p.parse_args()
out=root/'port/build/oct22/gauntlet-rematch'/(('bt4' if a.bt4 else 'bt3')+('-music' if a.music else ''));out.mkdir(parents=True,exist_ok=True)
settings=out/'settings.txt';settings.write_text('tag_controller_check=0\ntag_extra_intros=0\n')
pad=[(150,'select')]+[(i,'down') for i in (210,270,330)]+[(570,'cross'),(660,'cross'),(750,'cross'),(900,'start')]
pad += [(i,'cross') for i in range(1020,3600,60)]
for i in range(4000,20000,360):pad.extend(((i,'cross'),(i+120,'up'),(i+240,'cross')))
if a.music:
    # Team setup: edit Music, cancel back to Random, then choose and confirm.
    pad=[(t,b) for t,b in pad if t<900 or t>=4000]
    pad.extend(((840,'down'),(900,'down'),(960,'cross'),(1020,'right'),(1080,'triangle'),
                (1140,'cross'),(1200,'right'),(1260,'cross'),(1320,'start')))
    stage=(2760 if a.bt4 else 2460)+420
    pad.extend((t,'cross') for t in range(1440,stage,60))
    # Square must no longer open a second music picker at stage selection.
    pad.extend(((stage,'square'),(stage+120,'cross')))
    pad.sort()
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='gpu',BT3_UNCAPPED='1',BT3_WINDOW='800x600',BT3_TAG_TEAM='1',BT3_TAG_MENU_TEST='main',BT3_MENU_LANG='en',
 BT3_SETTINGS=str(settings),BT3_SAVES=str(out/'saves'),BT3_UPDATE_CHECK='0',BT3_TAG_TEST_CHALLENGE='1',
 BT3_TAG_TEST_WAVE_FINISH='1',BT3_PAD=','.join(f'{t}:{b}' for t,b in pad),BT3_LOAD_TRACE='1')
if a.bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
log=out/'runtime.log'
with log.open('w') as f:
 r=subprocess.run([str(root/'port/build/bt3.exe')],cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
text=log.read_text(errors='replace')
assert r.returncode==0 and 'PASS check=2160' in text and 'PASS check=99' in text,log
assert 'page=90 state=0,1,1,' in text and text.count('page=85 state=0,')>=2, ('No native Fight Again transition',log)
rows=[tuple(map(int,m)) for m in re.findall(r'page=91 state=(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)',text)]
waves=[tuple(map(int,m)) for m in re.findall(r'page=92 state=(\d+),(\d+),0,0,0',text)]
if a.music:
    edits=[tuple(map(int,m)) for m in re.findall(r'page=94 state=(\d+),(\d+),(\d+),0,0',text)]
    assert len(edits)==3 and edits[1][1]==24 and edits[-1][1]!=24,('Music edit/cancel/confirm failed',edits,log)
    picked=edits[-1][1]
    assert rows and all(r[0]==9 and r[3]==picked for r in rows),('Stage picker changed music',rows,log)
    assert picked>=0 and len(waves)>=4 and all(song==picked for _,song in waves),(picked,waves,log)
    print('PASS selected music retained across every wave and Fight Again',picked)
else:
    assert rows and all(r[3]==24 for r in rows),('Gauntlet did not default to Random',rows,log)
    assert len(waves)>=4 and all(song not in (24,25) for _,song in waves),(waves,log)
    print('PASS Random default resolves to playable music')
print('PASS gauntlet selection, final result, Fight Again and first wave restoration',log)
