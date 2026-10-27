"""Open the real disc menus with isolated saves and render their assets.

The fixture skips the logos and parent choice only. The normal mode handlers,
archive loading, texture relocation, audio and frame loops still run.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
SCREENS = {4: 'main', 6: 'history', 14: 'mission100', 17: 'survival',
           20: 'simulation', 26: 'ranking', 28: 'courses', 33: 'tournament',
           34: 'tournament-selector', 48: 'evolution', 49: 'equipment',
           53: 'data-center', 54: 'password', 55: 'saved-characters',
           56: 'replays', 60: 'character-reference'}


def run(exe, data, screen, output, pad='', frame=220):
    out = output / data.name / SCREENS[screen]
    out.mkdir(parents=True, exist_ok=True)
    settings = out / 'settings.txt'
    settings.write_text('menu_lang="en"\n')
    shot = out / 'menu.ppm'
    shot.unlink(missing_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_DATA=str(data), BT3_SETTINGS=str(settings),
               BT3_SAVES=str(out / 'saves'), BT3_UPDATE_CHECK='0',
               BT3_TAG_TEAM='1', BT3_TAG_MENU_TEST=f'screen:{screen}',
               BT3_GS='gpu', BT3_UI_SHOT=f'{frame}:{shot.as_posix()}', BT3_PAD=pad,
               SDL_AUDIO_DRIVER='dummy')
    log = out / 'runtime.log'
    with log.open('w') as stream:
        proc = subprocess.Popen([str(exe)], cwd=ROOT, env=env,
                                stdout=stream, stderr=stream)
        try:
            deadline = time.monotonic() + 50
            while not shot.exists():
                assert proc.poll() is None, f'{data.name}/{screen}: exited {proc.returncode}: {log}'
                assert time.monotonic() < deadline, f'{data.name}/{screen}: stalled: {log}'
                time.sleep(.1)
            time.sleep(.3)
            assert proc.poll() is None, f'{data.name}/{screen}: crashed after drawing: {log}'
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=10)
    text = log.read_text(errors='replace').lower()
    for error in ('crashed:', 'file not found', 'could not load', 'segmentation fault'):
        assert error not in text, f'{error}: {log}'
    assert shot.stat().st_size > 100000, f'No rendered menu: {shot}'
    result = dict(data=data.name, screen=screen, name=SCREENS[screen], passed=True)
    print(json.dumps(result), flush=True)
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe', type=Path, default=ROOT / ('port/build/bt3.exe' if sys.platform == 'win32' else 'port/build/bt3_64'))
    p.add_argument('--data', type=Path, action='append', required=True)
    p.add_argument('--screen', type=int, action='append', choices=SCREENS)
    p.add_argument('--jobs', type=int, default=1)
    p.add_argument('--pad', default='')
    p.add_argument('--frame', type=int, default=220)
    p.add_argument('--output', type=Path, default=ROOT / 'port/build/disc-menu-resources')
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    jobs = [(args.exe.resolve(), d.resolve(), s, args.output.resolve(), args.pad, args.frame)
            for d in args.data for s in (args.screen or SCREENS)]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda task: run(*task), jobs))
    (args.output / 'results.json').write_text(json.dumps(results, indent=2))
