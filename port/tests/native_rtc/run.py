"""Compile a host C probe and test the exact RTC library shipped to players."""
from pathlib import Path
import argparse,json,os,shlex,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[3]

def run(library=None):
    mac=os.uname().sysname=='Darwin' if os.name!='nt' else False
    prefix=Path(os.environ.get('BT3_RTC','/usr/local'))
    if library is None:library=prefix/'lib'/('libdatachannel.dylib' if mac else 'libdatachannel.so')
    library=library.resolve();assert library.is_file(),str(library)
    env=dict(os.environ)
    with tempfile.TemporaryDirectory(prefix='bt4-rtc-test-') as directory:
        out=Path(directory);exe=out/('rtc-probe.exe' if os.name=='nt' else 'rtc-probe')
        source=Path(__file__).with_name('datachannel_probe.c')
        cc=os.environ.get('CC','cc')
        if os.name=='nt':
            sdl=Path(os.environ['BT3_SDL3'])
            flags=['-I'+str(sdl/'include'),'-L'+str(sdl/'lib'),'-lSDL3','-static-libgcc']
            shutil.copy2(sdl/'bin/SDL3.dll',out/'SDL3.dll')
            env['PATH']=str(library.parent)+os.pathsep+str(sdl/'bin')+os.pathsep+env['PATH']
        else:
            if os.environ.get('BT3_SDL3'):env['PKG_CONFIG_PATH']=os.environ['BT3_SDL3']+'/lib/pkgconfig'+os.pathsep+env.get('PKG_CONFIG_PATH','')
            flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],env=env,text=True))
            if mac and os.environ.get('BT3_SDL3'):flags+=['-Wl,-rpath,'+os.environ['BT3_SDL3']+'/lib']
        hidden={'creationflags':0x08000000} if os.name=='nt' else {}
        subprocess.run([cc,'-std=c11','-O2','-I'+str(ROOT/'port/third_party/voice'),str(source),'-o',str(exe),*flags],env=env,check=True,**hidden)
        reports=[]
        for mode in ['reliable','unreliable']:
            result=subprocess.run([str(exe),str(library),mode],env=env,capture_output=True,text=True,timeout=20,**hidden)
            if result.returncode:raise RuntimeError(result.stdout+result.stderr)
            report=json.loads(result.stdout.strip().splitlines()[-1]);report['library']=str(library)
            reports.append(report);print(json.dumps(report),flush=True)
        return reports

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--library',type=Path)
    run(parser.parse_args().library)
