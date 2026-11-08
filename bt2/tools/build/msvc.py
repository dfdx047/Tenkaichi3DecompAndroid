"""Run a command inside the MSVC x64 build environment (what vcvars64.bat sets up).

The Ninja build needs cl.exe, its headers and libraries in the environment:
    python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-entry-probe
"""
import os,subprocess,sys
from pathlib import Path
NINJA='Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'
def choose_installation(roots):
    """BT2_VS_ROOT if set; otherwise the first installation with the C++ tools that bundles Ninja, else the first with the tools."""
    usable=[r for r in roots if (Path(r)/'VC/Auxiliary/Build/vcvars64.bat').is_file()]
    if not usable:raise SystemExit('No Visual Studio installation with the C++ x64 tools was found')
    return next((r for r in usable if (Path(r)/NINJA/'ninja.exe').is_file()),usable[0])
def installation():
    if os.environ.get('BT2_VS_ROOT'):return choose_installation([os.environ['BT2_VS_ROOT']])
    installer=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    if not installer.is_file():raise SystemExit('vswhere.exe not found; install Visual Studio with the C++ workload')
    out=subprocess.run([str(installer),'-all','-products','*','-property','installationPath'],capture_output=True,text=True,check=True).stdout
    return choose_installation([line.strip() for line in out.splitlines() if line.strip()])
def parse_environment(text):
    env={}
    for line in text.splitlines():
        name,sep,value=line.partition('=')
        if sep and name:env[name]=value
    return env
def environment():
    root=Path(installation())
    out=subprocess.run(f'"{root/"VC/Auxiliary/Build/vcvars64.bat"}" >nul && set',shell=True,capture_output=True,text=True,check=True).stdout
    env=parse_environment(out)
    if 'INCLUDE' not in env:raise SystemExit('The MSVC environment has no INCLUDE path')
    key=next((k for k in env if k.upper()=='PATH'),'PATH')
    if (root/NINJA).is_dir():env[key]=env.get(key,'')+os.pathsep+str(root/NINJA) # Visual Studio's own Ninja.
    return env
def main():
    if len(sys.argv)<2:raise SystemExit(__doc__)
    sys.exit(subprocess.run(sys.argv[1:],env=environment()).returncode)
if __name__=='__main__':main()
