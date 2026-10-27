"""Confirm CPU difficulty carry-over and cancellation through the real picker."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def run(exe, data, online=False):
    bt4 = 'bt4' in data.name
    out = ROOT / 'port/build/cpu-difficulty-memory' / (data.name + ('-online' if online else '-offline'))
    out.mkdir(parents=True, exist_ok=True)
    settings = out / 'settings.txt'
    settings.write_text('menu_lang="en"\ntag_controller_check=0\n')
    if online:
        events = ['150:down', '210:cross']
        tick = 270
    else:
        events = ['150:select', '210:cross', '270:down', '310:down', '350:down', '390:down', '450:cross', '690:start']
        tick = 870

    def press(key):
        nonlocal tick
        events.append(f'{tick}:{key}')
        tick += 60

    expected = []
    for i in range(4):
        slot = 0 if online else (0, 1, 5, 6)[i]
        if online and i:
            press('cross')  # edit the same CPU again in the room
        elif not online and i in (1, 3):
            press('cross')  # next free team slot
        for _ in range(4 if online else 3 + bt4):
            press('cross')  # character, optional form, equipment, costume
        initial = (2, 1, 2, 2)[i]
        expected.append((0, slot, initial))
        if i == 0:
            press('up')
        elif i == 1:
            press('down')
        elif i == 2:
            press('down')
            press('triangle')
            expected.append((2, slot, 3))
            press('cross')  # re-open after cancelling the unconfirmed change
            expected.append((0, slot, 2))
        press('cross')
        expected.append((1, slot, 1 if i == 0 else 2))
        if not online and i == 1:
            press('right')
            press('cross')  # finish Team 1

    shot = out / 'screen.ppm'
    shot.unlink(missing_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_GS='gpu', BT3_DATA=str(data), BT3_TAG_TEAM='1', BT3_TAG_MENU_TEST='main',
               BT3_MENU_ITEM4='0', BT3_UPDATE_CHECK='0', BT3_MENU_LANG='en',
               BT3_SETTINGS=str(settings), BT3_SAVES=str(out/'saves'),
               BT3_PAD=','.join(events), BT3_UI_SHOT=f'{tick+40}:{shot.as_posix()}', SDL_AUDIO_DRIVER='dummy')
    if online:
        env.update(BT3_TAG_ONLINE='1', BT3_TAG_HOST='17032', BT3_TAG_TEST_SIZE='5,5')
    log = out / 'runtime.log'
    with log.open('w') as stream:
        proc = subprocess.Popen([str(exe)], cwd=ROOT, env=env, stdout=stream, stderr=stream)
        try:
            deadline = time.monotonic() + 110
            while time.monotonic() < deadline:
                text = log.read_text(errors='replace')
                assert 'crashed:' not in text, log
                if shot.exists() and shot.stat().st_size > 100000:
                    break
                assert proc.poll() is None, f'Exited {proc.returncode}: {log}'
                time.sleep(.1)
            else:
                raise AssertionError(f'Menu timed out: {log}')
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=10)
    actual = [tuple(map(int, row)) for row in re.findall(r'tagcpu prompt=(\d+) slot=(\d+) level=(\d+)', text)]
    assert actual == expected, (actual, expected, log)
    print(json.dumps({'passed': True, 'data': data.name, 'online': online, 'log': str(log)}), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT / ('port/build/bt3.exe' if sys.platform == 'win32' else 'port/build/bt3_64'))
    parser.add_argument('--data', type=Path, action='append', required=True)
    parser.add_argument('--online', action='store_true')
    args = parser.parse_args()
    for data in args.data:
        run(args.exe.resolve(), data.resolve(), args.online)
