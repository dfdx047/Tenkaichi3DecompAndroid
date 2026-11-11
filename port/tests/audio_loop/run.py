from pathlib import Path
import os,subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='bt4-adx-loop-') as folder:
    exe=Path(folder)/('loop.exe' if os.name=='nt' else 'loop')
    kw={'creationflags':0x08000000} if os.name=='nt' else {}
    prefix=['rtk','proxy'] if shutil.which('rtk') else []
    subprocess.run(prefix+[os.environ.get('CC','gcc' if os.name=='nt' else 'cc'),'-std=c99','-O2','-Wall','-Wextra','-Werror',str(root/'port/tests/audio_loop/test_loop.c'),'-o',str(exe)],check=True,**kw)
    subprocess.run(prefix+[str(exe)],check=True,**kw)
