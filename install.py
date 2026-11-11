#!/usr/bin/env python3
"""Build BT4decomp on Linux x86-64 or macOS Apple Silicon and install a BT4 ISO.

    python3 install.py --iso /path/to/BT4.iso
    python3 install.py --iso /path/to/BT4.iso --language both
    python3 install.py --check

Builds use the repository's blank tables, never BT3 disc data. The same native
installer shipped in release packages validates and extracts the player's BT4
B14 Rev2 LATINO or English ISO. Nothing from the disc is redistributed.
"""
from pathlib import Path
import argparse,hashlib,os,platform,re,shutil,subprocess,sys,time

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'port/tools'))
from bt4_disc import DiscError,identify
LOG=ROOT/'install.log'
MAC=sys.platform=='darwin'

class Failed(Exception):pass

def run(command,env=None,cwd=ROOT):
 with LOG.open('a',encoding='utf8') as log:
  log.write('\n$ '+' '.join(map(str,command))+'\n');log.flush()
  result=subprocess.run(list(map(str,command)),cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
 if result.returncode:raise Failed('Command failed. See install.log: '+str(command[0]))

def available(command):
 try:return subprocess.run(command,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30).returncode==0
 except (OSError,subprocess.TimeoutExpired):return False

def requirements():
 llvm=Path(os.environ.get('BT3_LLVM','/opt/homebrew/opt/llvm/bin'))
 rows=[('C/C++ compiler',bool(shutil.which('clang' if MAC else 'gcc')) and bool(shutil.which('clang++' if MAC else 'g++')),
        'xcode-select --install' if MAC else 'install build-essential'),
       ('LLVM',all((llvm/t).is_file() for t in ('clang','llc')) if MAC else bool(shutil.which('clang')) and bool(shutil.which('llc')),
        'brew install llvm' if MAC else 'install clang, llvm and matching python3-clang bindings'),
       ('SDL3',available(['pkg-config','--exists','sdl3']),'brew install sdl3 pkgconf' if MAC else 'install SDL3 development headers'),
       ('libcurl WebSockets',available(['pkg-config','--atleast-version=7.86','libcurl']),
        'brew install curl' if MAC else 'install a libcurl development build with WebSocket and TLS support'),
       ('zlib',available(['pkg-config','--exists','zlib']),'brew install zlib' if MAC else 'install zlib1g-dev'),
       ('shader compiler',bool(shutil.which('glslc') or shutil.which('glslangValidator')),'brew install shaderc' if MAC else 'install glslang-tools or shaderc')]
 if MAC:rows.append(('Metal shader translator',bool(shutil.which('spirv-cross')),'brew install spirv-cross'))
 return rows

def python_environment():
 if available([sys.executable,'-c','import clang.cindex as c;c.Index.create()']):return Path(sys.executable)
 venv=ROOT/'.venv';python=venv/'bin/python'
 if not python.exists():run([sys.executable,'-m','venv','--system-site-packages',venv])
 if not available([str(python),'-c','import clang.cindex as c;c.Index.create()']):
  run([python,'-m','pip','install','--disable-pip-version-check','libclang==18.1.1'])
 return python

def stage_runtime(package):
 """Copy generated package files, retaining player data and backing up replaced files."""
 runtime=ROOT/'BT4decomp-runtime';backup=ROOT/'port/build/install-backups'/time.strftime('%Y%m%d-%H%M%S')
 runtime.mkdir(exist_ok=True)
 for source in sorted(package.rglob('*')):
  if not source.is_file():continue
  rel=source.relative_to(package);target=runtime/rel
  if rel.parts[0] in {'gamedata','saves','update'}:continue
  if target.exists():
   if rel.parts[0] in {'stages','songs','textures'} or rel.as_posix()=='fonts/fonts.tsv':continue
   if hashlib.sha256(source.read_bytes()).digest()==hashlib.sha256(target.read_bytes()).digest():continue
   old=backup/rel;old.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,old)
  target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
 for name in ('BT4decomp','BT4decomp-setup'):(runtime/name).chmod(0o755)
 return runtime

def main():
 parser=argparse.ArgumentParser(description='Build and install BT4decomp from a BT4 B14 Rev2 ISO.')
 parser.add_argument('--iso',type=Path,help='BT4 B14 Rev2 LATINO or English ISO (SLUS-21978)')
 parser.add_argument('--language',choices=('auto','es','en','both'),default='auto',help='game language; auto uses the ISO language')
 parser.add_argument('--check',action='store_true',help='check build requirements without installing')
 parser.add_argument('--rebuild',action='store_true',help='rebuild the game and installer')
 parser.add_argument('--force',action='store_true',help='rebuild; native setup still verifies existing extracted files')
 parser.add_argument('--skip-test',action='store_true',help=argparse.SUPPRESS)
 parser.add_argument('--machine',action='store_true',help=argparse.SUPPRESS)
 args=parser.parse_args()
 if sys.platform not in ('linux','darwin'):
  print('Use the Windows release package on Windows. This source installer supports Linux x86-64 and macOS Apple Silicon.');return 2
 if (MAC and platform.machine()!='arm64') or (not MAC and platform.machine().lower() not in ('x86_64','amd64')):
  print('This build supports Linux x86-64 and macOS Apple Silicon (arm64).');return 2
 LOG.write_text('BT4decomp source setup\n',encoding='utf8')
 print('BT4decomp setup',flush=True)
 try:
  rows=requirements();missing=[x for x in rows if not x[1]]
  for name,present,instruction in rows:print(('OK  ' if present else 'MISSING  ')+name+('' if present else ': '+instruction),flush=True)
  if missing:raise Failed('Install the missing build requirements and run this command again.')
  if args.check:return 0
  iso=args.iso
  if not iso:
   if not sys.stdin.isatty():raise Failed('Pass your BT4 ISO with --iso /path/to/BT4.iso.')
   iso=Path(input('BT4 B14 Rev2 ISO path: ').strip().strip('\"\''))
  iso=iso.expanduser().resolve()
  print('Checking BT4 disc profile...',flush=True);language=identify(iso)
  if args.language not in ('auto','both',language):raise Failed('The selected language does not match the ISO. Select its language or both.')
  print('Verified BT4 B14 Rev2 '+('English' if language=='en' else 'LATINO'),flush=True)
  python=python_environment();env=dict(os.environ,BT3_CC='mac64' if MAC else 'clang64',BT3_SKELETON='1')
  if MAC:
   env['BT3_CURL']=os.environ.get('BT3_CURL','/opt/homebrew/opt/curl')
   env['CPPFLAGS']=(env.get('CPPFLAGS','')+' -I'+env['BT3_CURL']+'/include').strip()
  marker=(ROOT/'port/release/BT4_VERSION.txt').read_text(encoding='utf8')
  match=re.search(r'BT4decomp\s+(\d+(?:\.\d+){1,3})(?: R(\d+))?',marker)
  if match:env['BT3_VERSION']=match[1]+('-R'+match[2] if match[2] else '')
  exe=ROOT/'port/build'/('bt3_mac' if MAC else 'bt3_64')
  if args.force or args.rebuild or not exe.exists():
   print('Building the native runtime...',flush=True)
   run([python,ROOT/'port/tools/undefined.py'],env)
   compile_log=LOG.read_text(encoding='utf8',errors='replace')
   if re.search(r'^FAILED',compile_log,re.M):raise Failed('One or more source files failed to compile. See install.log.')
   run([python,ROOT/'port/tools/link.py'],env)
  print('Building setup and preparing the portable folder...',flush=True)
  run(['sh',ROOT/'port/setup/build.sh','mac' if MAC else 'linux'],env)
  run([python,ROOT/'port/tools/package.py'],env)
  runtime=stage_runtime(ROOT/'port/build'/('release-mac' if MAC else 'release'))
  selected=language if args.language=='auto' else args.language
  print('Installing and verifying BT4 game data. Progress is written to install.log.',flush=True)
  run([runtime/'BT4decomp-setup','--install',iso,'--language',selected],env,cwd=runtime)
  launcher=ROOT/'play.sh';launcher.write_text('#!/bin/sh\ncd "$(dirname "$0")/BT4decomp-runtime" || exit 1\nexec ./BT4decomp "$@"\n',encoding='utf8');launcher.chmod(0o755)
  print('Ready. Start ./play.sh. Game files and saves are in BT4decomp-runtime/.',flush=True)
  return 0
 except (Failed,DiscError,OSError) as error:
  print('Setup stopped: '+str(error)+'\nDetails: install.log',file=sys.stderr);return 1

if __name__=='__main__':sys.exit(main())
