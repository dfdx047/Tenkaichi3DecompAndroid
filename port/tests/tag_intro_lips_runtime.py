"""Check native intro lip tracks on loaded fighters, including gauntlet arrivals.

Uses isolated saves and the real voice durations. Does not substitute models,
animations or a save state. BT3_TAG_FACE_TRACE observes the regular face update.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
INTRO = re.compile(r'tagintro actor=(\d+) chara=(\d+) line=(\d+) state=(\d+)')
FACE = re.compile(r'tagface frame=(\d+) obj=([0-9a-f]+) mode=(\d+) line=(\d+) '
                  r'time=(\d+) shape=(\d+) first=(-?\d+) count=(\d+) jaw=([-\d.]+)')


def check_lines(text, log, expected_count=None, skipped=False):
    active = None
    completed = []
    for line in text.splitlines():
        intro = INTRO.fullmatch(line)
        face = FACE.fullmatch(line)
        if intro:
            actor, chara, voice, state = map(int, intro.groups())
            if state == 0:
                # A skipped wave starts its next speaker without a completed line.
                assert active is None or skipped, (active, log)
                active = (actor, chara, voice, set(), set(), set())
            else:
                assert state == 1 and active is not None, (line, log)
                assert active[:3] == (actor, chara, voice), (active, line, log)
                _, _, _, shapes, jaws, times = active
                assert max(times, default=0) > 4, ('Lip track did not advance', active, log)
                assert len(shapes) > 1 or len(jaws) > 1, ('Mouth did not move', active, log)
                completed.append(active[:3])
                active = None
        elif face and active:
            frame, obj, mode, voice, time, shape, first, count, jaw = face.groups()
            if int(mode) and int(voice) == active[2] and int(count) > 1:
                active[3].add(int(shape))
                active[4].add(float(jaw))
                active[5].add(int(time))
    assert completed, ('No complete spoken entrances', log)
    if expected_count is not None:
        assert len(completed) == expected_count, (completed, log)
        assert sorted(a for a, _, _ in completed) == list(range(expected_count)), log
    return len(completed)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bt4', action='store_true')
    parser.add_argument('--exe', default='port/build/bt3.exe' if os.name == 'nt' else 'port/build/bt3_64')
    parser.add_argument('--output', default='port/build/intro-lips')
    args = parser.parse_args()
    out = ROOT / args.output / ('bt4' if args.bt4 else 'bt3')
    out.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_GS='none', BT3_DEMO='1', BT3_SETTINGS='', BT3_TAG_TEAM='1', BT3_TAG_PLAY='4',
               BT3_TAG_TRACE='1', BT3_TAG_FACE_TRACE='1', BT3_SOUND_TICKS='1',
               BT3_UPDATE_CHECK='0', SDL_AUDIO_DRIVER='dummy')
    if args.bt4:
        env['BT3_DATA'] = str(ROOT / 'gamedata-bt4')
    cases = [('ten-fighters', dict(BT3_TAG_ROSTER='5,5', BT3_TEST_FRAMES='2'), 10, False),
             ('gauntlet', dict(BT3_TAG_ROSTER='2,2', BT3_TAG_TEST_CHALLENGE='1', BT3_TAG_MENU_TEST='main'), None, False),
             ('gauntlet-skip', dict(BT3_TAG_ROSTER='2,2', BT3_TAG_TEST_CHALLENGE='1', BT3_TAG_MENU_TEST='main',
                                   BT3_TAG_TEST_WAVE_SKIP='1'), None, True)]
    if args.bt4:
        cases += [(f'added-fighter-{c}', dict(BT3_TAG_ROSTER='2,2', BT3_TEST_FRAMES='2', BT3_CHARA=str(c)), 4, False)
                  for c in (34, 151)]
    for name, values, count, skipped in cases:
        log = out / (name + '.log')
        with log.open('w') as stream:
            result = subprocess.run([str(ROOT / args.exe)], cwd=ROOT,
                env=env | values | {'BT3_SAVES': str(out / name / 'saves')},
                stdout=stream, stderr=subprocess.STDOUT, timeout=180)
        text = log.read_text(errors='replace')
        assert result.returncode == 0, (result.returncode, log)
        spoken = check_lines(text, log, count, skipped)
        if count is None:
            assert 'PASS check=99' in text, log
        if skipped:
            assert all(f'PASS check={n}' in text for n in (2152, 2153, 2154)), log
        print(f'PASS {name}: {spoken} entrances with moving mouths', flush=True)


if __name__ == '__main__':
    main()
