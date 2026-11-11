from pathlib import Path
import os, shutil, subprocess, tempfile

root = Path(__file__).resolve().parents[3]
prefix = ['rtk', 'proxy'] if shutil.which('rtk') else []
flags = {'creationflags': 0x08000000} if os.name == 'nt' else {}
with tempfile.TemporaryDirectory(prefix='bt4-health-palette-') as folder:
    for name in ('palette', 'health'):
        exe = Path(folder) / (name + ('.exe' if os.name == 'nt' else ''))
        sources = [str(root / f'port/tests/palette/test_{name}.c')]
        if name == 'health': sources.append(str(root / 'src/battle/hud_a_d.c'))
        subprocess.run(prefix + [os.environ.get('CC', 'gcc' if os.name == 'nt' else 'cc'),
            '-std=c99', '-O2', '-Wno-shift-count-overflow', '-I' + str(root / 'include'),
            *sources, '-o', str(exe)], check=True, **flags)
        subprocess.run(prefix + [str(exe)], check=True, **flags)
