"""Run the real clash handlers with successive prompts enabled on both discs."""
import os
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
exe=root/('port/build/bt3.exe' if os.name=='nt' else 'port/build/bt3_64')
for bt4 in (False,True):
    out=root/'port/build/oct22/clash-qte'/('bt4' if bt4 else 'bt3')
    out.mkdir(parents=True,exist_ok=True)
    settings=out/'settings.txt'
    settings.write_text('tag_clash_qte=1\ntag_extra_intros=0\n')
    env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
    env.update(BT3_GS='none',BT3_DEMO='1',BT3_SETTINGS=str(settings),BT3_SAVES=str(out/'saves'),
               BT3_TAG_TEAM='1',BT3_TAG_PLAY='4',BT3_TAG_ROSTER='2,2',BT3_TAG_TEST_CLASH='1')
    if bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
    log=out/'runtime.log'
    with log.open('w') as stream:
        r=subprocess.run([str(exe)],cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    assert r.returncode==0 and 'PASS check=99' in log.read_text(errors='replace'),log
    print('PASS QTE melee, vanish, beam and interruption:', 'BT4' if bt4 else 'BT3',flush=True)
