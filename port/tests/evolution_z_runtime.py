"""Exercise Evolution Z from the main menu using each disc's actual assets.

Uses isolated saves/settings. Visits the last roster row (BT4-only fighters),
equipment and item menus, backs out, and enters Customize again.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def run(exe, data):
    out = ROOT / 'port/build/evolution-z-fix' / data.name
    out.mkdir(parents=True, exist_ok=True)
    settings = out / 'settings.txt'
    settings.write_text('menu_lang="en"\n')
    shot = out / 'customize.ppm'
    shot.unlink(missing_ok=True)
    pad = ('150:down,210:down,270:down,330:down,390:cross,600:cross,'
           '800:right,860:left,920:up,970:right,1020:up,1100:cross,1220:cross,1340:cross,'
           '1460:down,1520:triangle,1640:triangle,1760:triangle,1880:triangle,'
           '2100:cross,2300:down,2420:cross,2540:triangle')
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_GS='gpu', BT3_DATA=str(data), BT3_MENU_LANG='en',
               BT3_TAG_TEAM='1', BT3_TAG_MENU_TEST='main', BT3_MENU_ITEM4='0',
               BT3_UPDATE_CHECK='0', BT3_SETTINGS=str(settings),
               BT3_SAVES=str(out / 'saves'), BT3_PAD=pad,
               BT3_UI_SHOT=f'1000:{shot.as_posix()}', SDL_AUDIO_DRIVER='dummy')
    log = out / 'runtime.log'
    with log.open('w') as stream:
        proc = subprocess.Popen([str(exe)], cwd=ROOT, env=env, stdout=stream, stderr=stream)
        try:
            deadline = time.monotonic() + 110
            while time.monotonic() < deadline:
                text = log.read_text(errors='replace')
                assert 'crashed:' not in text, log
                if re.search(r'tagmenu frame=25\d\d page=49 state=0,', text):
                    break
                assert proc.poll() is None, f'Exited {proc.returncode}: {log}'
                time.sleep(.1)
            else:
                raise AssertionError(f'Menu navigation timed out: {log}')
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=10)
    states = [(int(t), *map(int, s.split(','))) for t, s in
              re.findall(r'tagmenu frame=(\d+) page=49 state=([-\d,]+)', text)]
    assert {0, 1, 3, 10}.issubset({s[1] for s in states}), states
    assert any(1900 < s[0] < 2300 and s[1] == 0 for s in states), 'Did not re-enter Customize'
    assert any(s[5] == 4 and s[2] != 0 for s in states), 'Portrait navigation did not complete'
    marker = data / 'native-disc.json'
    if marker.exists() and json.loads(marker.read_text()).get('game') == 'bt4':
        assert any(s[2] > 164 and s[5] == 4 for s in states), 'Did not draw BT4-only portrait'
    assert shot.exists(), 'No Customize picture'
    assert 'file not found' not in text.lower(), log
    print(json.dumps({'passed': True, 'data': data.name, 'log': str(log)}), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT / ('port/build/bt3.exe' if sys.platform == 'win32' else 'port/build/bt3_64'))
    parser.add_argument('--data', type=Path, action='append', required=True)
    args = parser.parse_args()
    for data in args.data:
        run(args.exe.resolve(), data.resolve())
