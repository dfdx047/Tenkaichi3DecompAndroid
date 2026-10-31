"""Actual grabs, shared transformations and whole-roster destruction transitions."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', required=True,
                        choices=['grab-hit', 'grab-shared', 'ape', 'planet', 'planet-ultimate'])
    parser.add_argument('--exe', default='port/build/bt3.exe')
    parser.add_argument('--data', type=Path, help='Extracted native disc directory')
    parser.add_argument('--renderer', choices=['none', 'gpu'], default='none')
    parser.add_argument('--shot', type=int, help='Capture one GPU frame')
    parser.add_argument('--sequence', action='store_true', help='Capture GPU frames 600 through 1600')
    parser.add_argument('--views', choices=['2', '3', '4'], default='4')
    parser.add_argument('--roster', default='2,2')
    parser.add_argument('--stage', default='2')
    parser.add_argument('--scale', default='100')
    parser.add_argument('--character', help='Defaults to Scouter Vegeta for transformation, Kid Buu for destruction')
    parser.add_argument('--contact', action='store_true', help='Check protection on the exact native catch update')
    args = parser.parse_args()
    if (args.shot or args.sequence) and args.renderer != 'gpu':
        parser.error('Frame captures require --renderer gpu')

    name = (args.data.name+'-' if args.data else '') + f'{args.case}-{args.views}-{args.roster.replace(",", "v")}-{args.stage}-{args.scale}'
    if args.character: name += '-'+args.character
    out = ROOT / 'port/build/oct20-cinematics' / name
    out.mkdir(parents=True, exist_ok=True)
    settings = out / 'settings.txt'
    settings.write_text(f'tag_extra_intros=0\ntag_map_size={args.scale}\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_GS=args.renderer, BT3_DEMO='1', BT3_TAG_TEAM='1', BT3_TAG_PLAY=args.views,
               BT3_TAG_ROSTER=args.roster, BT3_SETTINGS=str(settings), BT3_SAVES=str(out / 'saves'),
               BT3_UPDATE_CHECK='0', SDL_AUDIO_DRIVER='dummy', BT3_TEST_STAGE=args.stage)
    if args.data: env['BT3_DATA'] = str(args.data.resolve())
    if args.contact: env['BT3_TAG_TEST_CONTACT'] = '1'
    if args.case.startswith('grab'):
        env['BT3_TAG_TEST_CAMERAS'] = '3' if args.case == 'grab-hit' else '4'
    else:
        env['BT3_TAG_TEST_CINEMA'] = {'ape': '10', 'planet': '11', 'planet-ultimate': '13'}[args.case]
    if args.case in ('ape', 'planet-ultimate'):
        env['BT3_TAG_SPECIAL_CHARACTER'] = args.character or ('29' if args.case=='ape' else '117')
    if args.sequence:
        env.update(BT3_SHOT='20', BT3_SHOT_FROM='600', BT3_SHOT_TO='1600',
                   BT3_WINDOW='960x720', BT3_UNCAPPED='1')
    if args.shot:
        shot = (out / f'{args.shot}.ppm').relative_to(ROOT).as_posix()
        env.update(BT3_UI_SHOT=f'{args.shot}:{shot}', BT3_WINDOW='960x720')

    log = out / 'runtime.log'
    started = time.time_ns()
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run([str(ROOT / args.exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding='utf-8', errors='replace')
    assert result.returncode == 0 and 'PASS check=99' in text, (result.returncode, log)
    if args.contact: assert 'PASS check=465' in text, log
    if args.sequence:
        for frame in range(600, 1601, 20):
            source = ROOT / 'port/build/shots' / f'gpu_{frame:05d}.ppm'
            if source.exists() and source.stat().st_mtime_ns >= started:
                shutil.copy2(source, out / source.name)
    print('PASS', args.case, args.views, args.roster, args.stage, args.scale)


if __name__ == '__main__':
    main()
