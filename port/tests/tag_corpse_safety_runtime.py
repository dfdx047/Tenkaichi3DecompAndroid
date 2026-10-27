"""KO catcher and real revival on floating stages, including expanded arenas."""
import argparse
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--bt4', action='store_true')
p.add_argument('--stage', type=int, choices=(5, 16), default=5)
p.add_argument('--scale', type=int, choices=(100, 200), default=100)
p.add_argument('--headless', action='store_true')
p.add_argument('--capture', action='store_true')
p.add_argument('--views', type=int, choices=(1, 2, 3, 4), default=1)
p.add_argument('--exe', default='port/build/bt3.exe' if os.name == 'nt' else 'port/build/bt3_64')
a = p.parse_args()
out = root/'port/build/oct26/corpse-safety'/('bt4' if a.bt4 else 'bt3')/f'{a.stage}-{a.scale}-{a.views}views'
out.mkdir(parents=True, exist_ok=True)
settings = out/'settings.txt'
settings.write_text(f'tag_map_size={a.scale}\ntag_extra_intros=0\n')
env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_GS='none' if a.headless else 'gpu', BT3_UNCAPPED='1', BT3_WINDOW='800x600',
           BT3_SETTINGS=str(settings), BT3_SAVES=str(out/'saves'), BT3_DEMO='1',
           BT3_TAG_TEAM='1', BT3_TAG_PLAY='4', BT3_TAG_VIEWS=str(a.views), BT3_TAG_ROSTER='2,2',
           BT3_TEST_STAGE=str(a.stage), BT3_TAG_TEST_CONTROLS='5', BT3_TAG_TRACE='1',
           BT3_UPDATE_CHECK='0', SDL_AUDIO_DRIVER='dummy')
if a.bt4:
    env['BT3_DATA'] = str(root/'gamedata-bt4')
if a.capture:
    shots = out/'shots'
    shots.mkdir(exist_ok=True)
    env.update(BT3_SHOT='30', BT3_SHOT_FROM='1000', BT3_SHOT_TO='1800', BT3_SHOT_DIR=str(shots))
log = out/'runtime.log'
with log.open('w') as stream:
    result = subprocess.run([str(root/a.exe)], cwd=root, env=env, stdout=stream,
                            stderr=subprocess.STDOUT, timeout=120)
text = log.read_text(errors='replace')
assert result.returncode == 0 and all(f'PASS check={i}' in text for i in (*range(400, 416), 99)), (result.returncode, log)
print('PASS KO settles, catcher toggle, permanent ring-out exclusion, revival at stage boundary', log)
