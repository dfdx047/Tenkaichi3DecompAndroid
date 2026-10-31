"""Exercise split-screen camera choices on both native discs using real moves."""
import os
import subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[2]
for bt4 in (False,True):
    for name,flags in (
        ('beam',{'BT3_TAG_TEST_BEAMS':'1'}),
        ('clashes',{'BT3_TAG_TEST_CLASH':'1'}),
        ('transform',{'BT3_TAG_TEST_CINEMA':'10','BT3_TAG_SPECIAL_CHARACTER':'29'}),
        ('planet',{'BT3_TAG_TEST_CINEMA':'11'}),
    ):
        out=root/'port/build/oct22/camera-options'/('bt4' if bt4 else 'bt3')/name
        out.mkdir(parents=True,exist_ok=True)
        settings=out/'settings.txt';settings.write_text('tag_extra_intros=0\n')
        env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
        env.update(BT3_GS='gpu',BT3_DEMO='1',BT3_UNCAPPED='1',BT3_WINDOW='800x600',
                   BT3_SETTINGS=str(settings),BT3_SAVES=str(out/'saves'),BT3_TAG_TEAM='1',
                   BT3_TAG_PLAY='4',BT3_TAG_ROSTER='2,2',BT3_UPDATE_CHECK='0',SDL_AUDIO_DRIVER='dummy')
        env.update(flags)
        if bt4:env['BT3_DATA']=str(root/'gamedata-bt4')
        log=out/'runtime.log'
        with log.open('w') as f:
            result=subprocess.run([str(root/'port/build/bt3.exe')],cwd=root,env=env,
                                  stdout=f,stderr=subprocess.STDOUT,timeout=120)
        text=log.read_text(errors='replace')
        assert result.returncode==0 and 'PASS check=99' in text,(result.returncode,log)
        if name=='beam':assert all(f'PASS check={n}' in text for n in (340,341,342,343,344,345)),log
        if name=='clashes':assert all(f'PASS check={n}' in text for n in (65,66,67,68)),log
        print('PASS','BT4' if bt4 else 'BT3',name,flush=True)
