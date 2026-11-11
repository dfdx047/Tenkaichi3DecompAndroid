"""Exercise the real Lua mod loader and the supplied scripts without opening a window."""
from pathlib import Path
import os,sys,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
out=root/'port/build/test_mods';out.mkdir(parents=True,exist_ok=True)
exe=out/('mods_test.exe' if os.name=='nt' else 'mods_test')
flags=[] if os.name=='nt' else ['-fsanitize=address,undefined','-fno-sanitize-recover=all']
hidden={'creationflags':0x08000000} if os.name=='nt' else {}
build=subprocess.run([os.environ.get('CC','gcc' if os.name=='nt' else 'cc'),'-g','-O1',*flags,str(root/'port/tests/mods/test_mods.c'),str(root/'port/src/gs/mods_lua.c'),'-lm','-lpthread','-o',str(exe)],cwd=root,capture_output=True,text=True,**hidden)
assert build.returncode==0,build.stdout+build.stderr
with tempfile.TemporaryDirectory(prefix='bt4-mod-tests-') as directory:
    env=dict(os.environ,TMPDIR=directory,ASAN_OPTIONS='detect_leaks=0')
    run=subprocess.run([str(exe)],cwd=root,env=env,capture_output=True,text=True,timeout=90,**hidden)
    assert run.returncode==0 and 'mods OK' in run.stdout,run.stdout+run.stderr
    print(run.stdout.strip())
