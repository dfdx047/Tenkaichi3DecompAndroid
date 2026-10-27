"""Native catches, scaled hand attachments, arena limits and simultaneous grabs.

Uses the actual disc models, grab collision, animation and release handlers.
The opt-in fixture places a receiver in the grab volume to isolate attachment
from approach/flight. Settings and saves are isolated from normal play.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT / 'gamedata')
    parser.add_argument('--bt4', action='store_true')
    parser.add_argument('--exe', type=Path, default=ROOT / ('port/build/bt3.exe' if os.name == 'nt' else 'port/build/bt3_64'))
    parser.add_argument('--case', action='append')
    parser.add_argument('--renderer', choices=['none', 'gpu'], default='none')
    args = parser.parse_args()
    models = (12, 30, 137, 143, 168, 169, 170, 171, 172, 173, 195, 211) if args.bt4 else (12, 30, 72, 74, 76, 78, 81, 122, 124, 139, 143)
    cases = [(f'giant-{ch}', ch, 200, 5) for ch in models]
    cases += [('stock-size', 30, 100, 5), ('maximum-size', 30, 400, 5),
              ('simultaneous', 30, 200, 6), ('ordinary-simultaneous', 0, 100, 6)]
    base = ROOT / 'port/build/giant-grab-tests' / args.data.name
    failed = []
    for name, character, size, mode in cases:
        if args.case and name not in args.case:
            continue
        out = base / name
        out.mkdir(parents=True, exist_ok=True)
        settings = out / 'settings.txt'
        settings.write_text(f'tag_extra_intros=0\ntag_controller_check=0\n'
                            f'tag_giants=1\ntag_giant_size={size}\nmusic=0\n')
        env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
        env.update(BT3_GS=args.renderer, BT3_DATA=str(args.data.resolve()), BT3_DEMO='1',
                   BT3_TAG_TEAM='1', BT3_TAG_PLAY='4', BT3_TAG_VIEWS='4', BT3_TAG_ROSTER='2,2',
                   BT3_CHARA=str(character), BT3_TAG_TEST_CAMERAS=str(mode), BT3_TAG_TRACE='1',
                   BT3_SETTINGS=str(settings), BT3_SAVES=str(out / 'saves'),
                   BT3_TEST_STAGE='28', BT3_UPDATE_CHECK='0', BT3_UNCAPPED='1', SDL_AUDIO_DRIVER='dummy')
        if args.renderer == 'gpu':
            (out / 'frames').mkdir(exist_ok=True)
            env.update(BT3_WINDOW='1280x720', BT3_SHOT='20', BT3_SHOT_FROM='400',
                       BT3_SHOT_TO='1500', BT3_SHOT_DIR=str(out / 'frames'))
        log = out / 'runtime.log'
        with log.open('w') as stream:
            result = subprocess.run([str(args.exe.resolve())], cwd=ROOT, env=env,
                                    stdout=stream, stderr=subprocess.STDOUT, timeout=120)
        text = log.read_text(errors='replace')
        passed = result.returncode == 0 and 'PASS check=99' in text and 'PASS check=472' in text
        if mode == 6:
            passed = passed and 'PASS check=473' in text
        print(('PASS ' if passed else 'FAIL ') + name, flush=True)
        if not passed:
            failed.append(str(log))
    if failed:
        raise SystemExit('Grab checks failed: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
