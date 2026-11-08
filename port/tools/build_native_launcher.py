"""Build the installed Windows Play.exe with MinGW-w64 (also works from WSL)."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    output = ROOT / 'port/build'
    output.mkdir(parents=True, exist_ok=True)
    resource = output / 'native_launcher.res.o'
    prefix = os.environ.get('MINGW_PREFIX', 'x86_64-w64-mingw32-')
    subprocess.run([prefix + 'windres', 'native_launcher.rc', '-O', 'coff', '-o', str(resource)],
                   cwd=ROOT / 'port/tools', check=True)
    subprocess.run([prefix + 'gcc', '-O2', '-Wall', '-Wextra', '-municode', '-mwindows', '-static', '-s',
                    str(ROOT / 'port/tools/native_launcher.c'), str(resource), '-o', str(output / 'Play.exe')], check=True)


if __name__ == '__main__':
    main()
