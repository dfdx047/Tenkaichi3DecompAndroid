"""Native installer checks without game data, windows, audio or input devices."""
from pathlib import Path
import os,shlex,subprocess,sys,tempfile

ROOT=Path(__file__).resolve().parents[3]
mac=sys.platform=='darwin'
prefix=Path(os.environ.get('BT3_SDL3','/usr/local'))
env=dict(os.environ)
env['PKG_CONFIG_PATH']=str(prefix/'lib/pkgconfig')+os.pathsep+env.get('PKG_CONFIG_PATH','')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],env=env,text=True))
flags+=['-Wl,-rpath,'+str(prefix/'lib')]
objects=ROOT/'port/build'/('setup_mac' if mac else 'setup')
cxx=os.environ.get('CXX','clang++' if mac else 'g++')
with tempfile.TemporaryDirectory(prefix='bt4-setup-check-') as directory:
    out=Path(directory)
    common=[cxx,'-std=c++17','-O2','-DIMGUI_ENABLE_TEST_ENGINE','-I'+str(ROOT/'port/third_party/imgui'),'-I'+str(ROOT/'port/src/gs')]
    core=[str(objects/(n+'.o')) for n in ['native','miniz','update_sign']]
    gui=[str(p) for p in objects.glob('imgui*.o')]+[str(objects/(n+'.o')) for n in ['ui_a11y','a11y','plat_settings']]
    libs=['-lz','-lpthread']+([] if mac else ['-ldl'])
    sources=[('ui',Path(__file__).with_name('ui_probe.cpp'),core+gui,[]),
             ('events',ROOT/'tools/review/test_setup_events.cpp',core+gui,[]),
             ('download',Path(__file__).with_name('download_probe.cpp'),[str(objects/(n+'.o')) for n in ['miniz','update_sign']],[str(out/'download')])]
    for name,source,extra,args in sources:
        exe=out/(name+'-probe')
        subprocess.run(common+[str(source)]+extra+['-o',str(exe)]+flags+libs,env=env,check=True)
        subprocess.run([str(exe)]+args,env=env,cwd=out,check=True,timeout=240)
