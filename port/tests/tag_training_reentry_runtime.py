"""Three real logical pads: select, leave Training, re-enter, select again."""
from pathlib import Path
import argparse
import re
import tag_menu_runtime as menus

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check-log',type=Path,help='Check an already captured real input replay')
parser.add_argument('--return-characters',action='store_true',help='Keep the roster inside the same battle session')
args=parser.parse_args()
p1='150:select,180:down,210:down,270:cross,330:down,390:down,450:cross,570:start'
p1+=',660:cross,720:cross,780:cross,840:cross,1140:right,1200:cross,1260:cross,1620:right,1680:cross,1860:cross,1920:cross'
p1+=',3700:start,3760:down,3800:down,3830:down,3860:cross,3920:up,3980:cross'
p1+=',4200:down,4260:down,4320:cross,4380:down,4440:down,4500:cross,4620:start'
p1+=',4800:cross,4860:cross,4920:cross,4980:cross,5340:right,5400:cross,5460:cross,5700:right,5760:cross'
p2='1320:cross,1380:cross,1440:cross,1500:right,1560:cross,5520:cross,5580:cross,5640:cross'
p3='900:cross,960:cross,1020:cross,1080:cross,5100:cross,5160:cross,5220:cross,5280:cross'
name='training-three-reentry'
if args.return_characters:
    name='training-three-return-characters'
    p1=p1.replace(',3830:down','')
    p1=p1.replace(',4200:down,4260:down,4320:cross,4380:down,4440:down,4500:cross,4620:start','')
    # Once P3 confirms, CPU slots and End belong to P1. P3 cannot advance them.
    p3+=',5300:right,5320:cross'
menus.CASES[name]={
    'pad':p1,'env':{'BT3_PAD_2':p2,'BT3_PAD_3':p3,'BT3_UNCAPPED':'1','BT3_WINDOW':'640x480'},
    'end':r'frame=\d+ page=40 state=1,8,5,','after':5760,
    'settings':'tag_extra_intros=0\n',
    'states':[(4,3),(48,0),(40,0),(90,0)]+([(48,2),(40,0)] if args.return_characters else [(4,1),(4,3),(48,0),(40,0)]),
}
if args.check_log:
    text=args.check_log.read_text()
else:
    result=menus.run(name,menus.ROOT/'port/build/bt3.exe','gpu')
    text=Path(menus.ROOT/result['log']).read_text()
entries=re.findall(r'page=48 state=([-\d,]+)',text)
assert entries==['0,0,0,3,2','2,1,0,3,2' if args.return_characters else '0,0,0,3,2'],entries
# Input scripts use rendered frames; menu traces use simulation VBlanks. They
# diverge during combat, so compare each visit's complete ownership transitions.
visits=re.split(r'tagmenu frame=\d+ page=48 state=[-\d,]+',text)[1:]
expected=[(0,0,0),(0,1,2),(0,2,0),(0,5,0),(1,0,1),(1,1,0),(1,5,0)]
for visit in visits:
    rows=[tuple(map(int,m)) for m in re.findall(r'page=40 state=(\d+),(\d+),(\d+),(\d+),(\d+)',visit)]
    actual=[]
    for side,state,slot,flags,owner in rows:
        key=(side,slot,owner)
        if not actual or actual[-1]!=key:actual.append(key)
    assert actual==expected,actual
    assert any(side==1 and state==8 and owner==0 for side,state,slot,flags,owner in rows)
destination=3 if args.return_characters else 4
assert f'page=90 state=0,0,{destination},{destination},0' in visits[0], 'Did not exit the actual battle through its pause menu'
if not args.return_characters:
    assert 'page=4 state=1,0,1,1,0' in visits[0], 'Did not return to the mode menu'
print('PASS',name,': P1/P2/P3 picks, P1 CPU and End control on both visits')
