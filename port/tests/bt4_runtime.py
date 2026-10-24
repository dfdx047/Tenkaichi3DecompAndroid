"""Exercise a privately imported BT4 disc through the native game, not a save state.

The roster, costumes and playable stages come from the player's own disc.
Run from the repository root; logs and isolated saves stay in port/build.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from threading import Lock

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'port/tools'))
from menu_font import unpack
from tag_stage_runtime import run as stage_run


def part(blob,index):
    count=struct.unpack_from('<I',blob)[0]
    assert 1<=index<=count and 4+4*count<=len(blob)
    start=struct.unpack_from('<I',blob,index*4)[0]&~3
    ends=[struct.unpack_from('<I',blob,i*4)[0]&~3 for i in range(1,count+1)]
    end=min((v for v in ends if v>start),default=len(blob))
    assert 4+4*count<=start<=end<=len(blob)
    return blob[start:end]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,default=ROOT/'gamedata-bt4')
    parser.add_argument('--exe',type=Path,default=ROOT/'port/build/bt3.exe')
    parser.add_argument('--case',choices=['roster','costumes','stages','fusions','mechanics','giants'],required=True)
    parser.add_argument('--character',type=int,action='append')
    parser.add_argument('--stage',type=int,action='append')
    parser.add_argument('--mechanic',action='append')
    parser.add_argument('--frames',type=int,default=1800)
    parser.add_argument('--output',type=Path,help='Separate results directory for this run')
    parser.add_argument('--jobs',type=int,choices=range(1,5),default=1)
    parser.add_argument('--resume',action='store_true',help='Keep completed cases in this local run')
    args=parser.parse_args(); data=args.data.resolve()
    assert json.loads((data/'native-disc.json').read_text())['game']=='bt4'
    out=args.output or ROOT/'port/build/bt4-runtime'/args.case;out.mkdir(parents=True,exist_ok=True)
    fingerprint=dict(exe=hashlib.sha256(args.exe.resolve().read_bytes()).hexdigest(),
                     memory=hashlib.sha256(Path(str(args.exe.resolve())+'.mem').read_bytes()).hexdigest(),
                     disc=json.loads((data/'native-disc.json').read_text()),frames=args.frames,case=args.case)
    record=out/'build.json'
    if args.resume and (out/'results.json').exists():
        assert record.exists() and json.loads(record.read_text())==fingerprint, \
            'Results belong to a different build or test setup; use a new output directory'
    record.write_text(json.dumps(fingerprint,indent=2)+'\n')
    settings=out/'settings.txt';settings.write_text('tag_extra_intros=0\n')
    env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_DATA=str(data),BT3_GS='none',BT3_DEMO='1',BT3_SETTINGS=str(settings),
               BT3_SAVES=str(out/'saves'),BT3_UPDATE_CHECK='0',SDL_AUDIO_DRIVER='dummy')
    results=json.loads((out/'results.json').read_text()) if args.resume and (out/'results.json').exists() else []
    result_lock=Lock()

    def battle(name,values,check=None):
        if args.resume and any(r.get('case')==name and r.get('passed') for r in results):return True
        log=out/(name+'.log')
        with log.open('w') as f:
            try:
                p=subprocess.run([str(args.exe.resolve())],cwd=ROOT,env=env|values|{'BT3_SAVES':str(out/'saves'/name)},
                                 stdout=f,stderr=subprocess.STDOUT,timeout=60)
                code=p.returncode
            except subprocess.TimeoutExpired:
                code=124
        text=log.read_text(errors='replace')
        passed=code==0 and 'crashed:' not in text and (check is None or check in text)
        with result_lock:
            results[:]=[r for r in results if r.get('case')!=name]
            results.append(dict(case=name,passed=passed,exit=code))
            (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        print(('PASS ' if passed else 'FAIL ')+name,flush=True)
        return passed

    if args.case in ('roster','costumes'):
        table=part((data/'pzs4us1/00003.bin').read_bytes(),1)
        cases=[]
        for c in args.character or range(250):
            outfits=struct.unpack_from('<H',table,c*60+40)[0]
            if not outfits or 161<=c<=164:continue
            # +40 counts physical resources, including Time Patroller's
            # internal costume-switch fragments. +10 is the actual outfit
            # selector limit; those internal fragments are not playable skins.
            selectable=table[c*60+10]
            assert 1<=selectable<=outfits,(c,selectable,outfits)
            for outfit in range(selectable if args.case=='costumes' else 1):
                for damaged in range(2 if args.case=='costumes' else 1):
                    values=dict(BT3_CHARA=str(c),BT3_COSTUME=str(outfit),BT3_TEST_FRAMES=str(args.frames),BT3_LOAD_TRACE='1')
                    if damaged:values['BT3_VARIANT']='1'
                    cases.append((f'{c}-{outfit}-{damaged}',values,
                        f'model load: actor=0 kind=-1 character={c} costume={outfit} variant={damaged}'))
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            list(pool.map(lambda case:battle(*case),cases))
    elif args.case=='stages':
        menu=unpack(part((data/'pzs4us1/00448.bin').read_bytes(),2))
        grid=part(menu,39);count=struct.unpack_from('<I',grid)[0]
        stages=list(struct.unpack_from(f'<{count}I',grid,16))
        for stage in args.stage or sorted(set(stages)-{35}):
            try:
                a=stage_run(args.exe.resolve(),stage,100,data)
                b=stage_run(args.exe.resolve(),stage,200,data)
                assert all(abs(x*2-y)<.1 for x,y in zip(a,b)),(a,b)
                result=dict(stage=stage,passed=True,normal=a,expanded=b)
            except Exception as exc:result=dict(stage=stage,passed=False,error=str(exc))
            results.append(result)
            print(('PASS ' if result['passed'] else 'FAIL ')+f'stage {stage}',flush=True)
            (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    elif args.case=='mechanics':
        common=dict(BT3_TAG_TEAM='1',BT3_TAG_PLAY='4',BT3_TAG_VIEWS='4',BT3_TAG_ROSTER='2,2')
        for name,values in [
            ('revival',dict(BT3_TAG_TEST_REVIVE='1')),
            ('targeting',dict(BT3_TAG_TEST_CONTROLS='1',BT3_TAG_PLAY='1')),
            ('spectating',dict(BT3_TAG_TEST_CONTROLS='2',BT3_TAG_PLAY='0',BT3_TAG_VIEWS='0')),
            ('clashes',dict(BT3_TAG_TEST_CLASH='1')),
            ('beam-assist',dict(BT3_TAG_TEST_BEAMS='1',BT3_TAG_TEST_BEAM_ASSIST='1')),
            ('cpu-targets',dict(BT3_TAG_TEST_CPU='1',BT3_TAG_PLAY='1')),
            ('voices',dict(BT3_TAG_TEST_AUDIO='1',BT3_TAG_ROSTER='5,5',BT3_SOUND='1')),
        ]:
            if args.mechanic and name not in args.mechanic:continue
            battle(name,common|values,'PASS check=99')
    elif args.case=='giants':
        table=part((data/'pzs4us1/00003.bin').read_bytes(),1)
        for c in args.character or range(250):
            if 161<=c<=164 or not struct.unpack_from('<H',table,c*60+40)[0]:continue
            params=part((data/'pzs4us2'/f'{750+c:05d}.bin').read_bytes(),2)
            if params[2]!=4:continue
            battle(f'giant-{c}',dict(BT3_CHARA=str(c),BT3_TAG_TEAM='1',BT3_TAG_PLAY='4',
                BT3_TAG_ROSTER='2,2',BT3_TAG_TEST_GIANTS='1'),'PASS check=99')
    else:
        pairs=[('kefla',118,198,104),('kefla-ss2',198,135,154),('zamasu',34,157,76),
               ('gogeta',3,31,85),('super-gogeta',31,3,53),('vegito',31,3,51),
               ('gotenks',46,44,48),('gotenks-ss3',45,47,50),('ss4-gogeta',10,38,54)]
        for name,a,b,fused in pairs:
            for players in (1,4):
                battle(f'{name}-{players}p',dict(BT3_TAG_TEAM='1',BT3_TAG_PLAY=str(players),
                    BT3_TAG_VIEWS='4',BT3_TAG_ROSTER='2,2',BT3_TAG_TEST_FUSION='2',
                    BT3_TAG_FUSION_PAIR=f'{a},{b},0',BT3_TAG_FUSION_RESULT=str(fused),
                    BT3_TAG_TRACE='1'),'PASS check=99')
    raise SystemExit(0 if results and all(r['passed'] for r in results) else 1)


if __name__=='__main__':main()
