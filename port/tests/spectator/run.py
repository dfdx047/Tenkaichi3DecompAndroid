"""Camera math and host-state regression tests without desktop input or audio."""
from pathlib import Path
import os,subprocess,shutil,shlex
root=Path(__file__).resolve().parents[3]
out=root/'port/build/test_spectator';out.mkdir(parents=True,exist_ok=True)
exe=out/('camera.exe' if os.name=='nt' else 'camera')
prefix=['rtk','proxy'] if shutil.which('rtk') else []
subprocess.run(prefix+[os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Werror',
                       str(Path(__file__).with_name('test_camera.c')),'-lm','-o',str(exe)],check=True)
subprocess.run(prefix+[str(exe)],check=True)
collision=out/('camera-collision.exe' if os.name=='nt' else 'camera-collision')
subprocess.run(prefix+[os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Wno-unused-function',
                       '-I'+str(root/'include'),str(Path(__file__).with_name('test_collision.c')),'-lm','-o',str(collision)],check=True)
subprocess.run(prefix+[str(collision)],check=True)
env=dict(os.environ)
if env.get('BT3_SDL3'):
    sdl=Path(env['BT3_SDL3']);flags=['-I'+str(sdl/'include'),'-L'+str(sdl/'lib'),'-lSDL3']
    if os.name=='nt':
        shutil.copy2(sdl/'bin/SDL3.dll',out/'SDL3.dll')
    elif os.uname().sysname=='Darwin':
        flags+=['-Wl,-rpath,'+str(sdl/'lib')]
else:flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],text=True))
host=out/('camera-host.exe' if os.name=='nt' else 'camera-host')
subprocess.run(prefix+[os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Wno-unused-function',
                       str(Path(__file__).with_name('test_host.c')),*flags,'-lm','-o',str(host)],check=True)
subprocess.run(prefix+[str(host)],check=True)
