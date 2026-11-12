"""Rebuild the bundled room kernel with Clang/LLD supporting wasm32."""
from pathlib import Path
import os,shutil,subprocess

root=Path(__file__).resolve().parent
source=root.parent
cc=os.environ.get('CLANG') or shutil.which('clang')
if not cc:
    raise SystemExit('Install Clang with wasm32 and LLD support, or set CLANG to its executable.')
command=[cc,'--target=wasm32','-std=c99','-O2','-ffreestanding','-fno-builtin','-nostdlib',
         '-I'+str(root/'libc'),str(root/'kernel.c'),str(source/'port/src/tag_team.c'),
         str(source/'port/src/tag_team_wire.c'),'-Wl,--no-entry','-Wl,--export-all',
         '-Wl,--initial-memory=524288','-Wl,--max-memory=524288','-o',str(root/'kernel.wasm')]
subprocess.run(command,check=True)
print('Built',root/'kernel.wasm')
