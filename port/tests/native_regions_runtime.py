"""Native regional-disc smoke tests using the player's private extracted data."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def match_pad(bt4):
    events=['150:select','210:cross','270:cross','450:start']
    tick=540
    def press(key):
        nonlocal tick
        events.append(f'{tick}:{key}')
        tick+=60
    for side in range(2):
        for member in range(2):
            if member:press('cross')
            # Fighter, optional form, equipment, colour, then CPU difficulty.
            for _ in range(3+bool(bt4)+(side!=0 or member!=0)):press('cross')
        press('right');press('cross')
    tick+=80
    press('cross')
    return ','.join(events)


def run(data, locale, picture, mechanic=None, match=False, executable=None):
    out = ROOT / 'port/build/regional-runtime' / (data.name + '-' + locale)
    out.mkdir(parents=True, exist_ok=True)
    settings = out / 'settings.txt'
    settings.write_text('menu_lang="' + locale + '"\ntag_controller_check=0\n', encoding='utf-8')
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_DATA=str(data), BT3_SETTINGS=str(settings), BT3_SAVES=str(out/'saves'),
               BT3_TAG_TEAM='1', BT3_UPDATE_CHECK='0', BT3_MENU_LANG=locale, SDL_AUDIO_DRIVER='dummy')
    name = picture or mechanic or ('match' if match else 'battle')
    if picture:
        pads = dict(modes='150:select', setup='150:select,210:cross,270:cross',
                    fighters='150:select,210:cross,270:cross,450:start',
                    settings='150:select,180:down,210:down,240:down,270:down,300:down,330:down,360:down,400:cross,460:cross')
        frame = dict(modes=200, setup=380, fighters=520, settings=540)[picture]
        shot = out / (picture+'.ppm')
        shot.unlink(missing_ok=True)
        env.update(BT3_GS='gpu', BT3_TAG_MENU_TEST='main', BT3_PAD=pads[picture],
                   BT3_UI_SHOT=f'{frame}:{shot.as_posix()}', BT3_TAG_TRACE='1', BT3_MENU_ITEM4='0')
    elif match:
        marker=data/'native-disc.json'
        bt4=marker.exists() and json.loads(marker.read_text())['game']=='bt4'
        env.update(BT3_GS='gpu',BT3_TAG_MENU_TEST='main',BT3_PAD=match_pad(bt4),
                   BT3_TAG_TRACE='1',BT3_MENU_ITEM4='0',BT3_LOAD_TRACE='1')
    elif mechanic:
        values={
            'voices':dict(BT3_TAG_TEST_AUDIO='1',BT3_TAG_ROSTER='5,5',BT3_SOUND='1'),
            'revival':dict(BT3_TAG_TEST_REVIVE='1'),
            'assists':dict(BT3_TAG_TEST_BEAMS='1',BT3_TAG_TEST_BEAM_ASSIST='1'),
            'fusion':dict(BT3_TAG_TEST_FUSION='1'),
        }[mechanic]
        marker=data/'native-disc.json'
        if mechanic=='fusion' and marker.is_file() and json.loads(marker.read_text()).get('game')=='bt4':
            # BT4 has different forms/recipes; use its authored Goku/Vegeta pair.
            values=dict(BT3_TAG_TEST_FUSION='2',BT3_TAG_FUSION_PAIR='3,31,0',BT3_TAG_FUSION_RESULT='85')
        env.update(BT3_GS='none',BT3_DEMO='1',BT3_TAG_PLAY='4',BT3_TAG_VIEWS='4',
                   BT3_TAG_ROSTER='2,2',BT3_TEST_STAGE='6')
        env.update(values)
    else:
        env.update(BT3_GS='none', BT3_DEMO='1', BT3_TAG_ROSTER='5,5', BT3_TAG_TRACE='1', BT3_TEST_FRAMES='1800')
    log = out / (name+'.log')
    with log.open('w', encoding='utf-8') as stream:
        p = subprocess.Popen([str(executable or ROOT/('port/build/bt3.exe' if sys.platform=='win32' else 'port/build/bt3_64'))], cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+100
            while time.monotonic()<deadline:
                if match and re.search(r'tagintro actor=3 .*state=1',log.read_text(encoding='utf-8',errors='replace')):
                    break
                if picture and shot.exists() and shot.stat().st_size>100000:
                    time.sleep(.5)
                    break
                if p.poll() is not None:
                    assert p.returncode==0, f'{data.name}: game exited {p.returncode}; {log}'
                    assert not picture, f'{data.name}: no picture; {log}'
                    break
                time.sleep(.1)
            else:
                raise AssertionError(f'{data.name}: timed out; {log}')
        finally:
            if p.poll() is None:p.terminate()
            p.wait(timeout=10)
    text=log.read_text(encoding='utf-8', errors='replace')
    assert 'crashed:' not in text and 'file not found' not in text.lower(), log
    if mechanic:
        assert 'PASS check=99' in text and 'FAIL check=' not in text,text[-1000:]
    elif match:
        assert {int(a) for a in re.findall(r'tagintro actor=(\d+)',text)}==set(range(4))
        assert 'page=80' not in text,'Standalone setup page was entered'
    elif not picture:
        actors=set(map(int,re.findall(r'tagteam frame=\d+ fighter=(\d+)',text)))
        assert actors==set(range(10)),actors
        assert 'test frames complete' in text,text[-300:]
    else:
        from PIL import Image
        Image.open(shot).save(shot.with_suffix('.png'))
    result=dict(data=data.name, locale=locale, case=name, passed=True)
    (out/(name+'.json')).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,required=True)
    parser.add_argument('--locale',default='en')
    parser.add_argument('--picture',choices=['modes','setup','fighters','settings'])
    parser.add_argument('--mechanic',choices=['voices','revival','assists','fusion'])
    parser.add_argument('--match',action='store_true')
    parser.add_argument('--exe',type=Path)
    args=parser.parse_args()
    run(args.data.resolve(),args.locale,args.picture,args.mechanic,args.match,args.exe)
