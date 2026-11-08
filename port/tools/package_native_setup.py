"""Package a self-contained native installer from an already stripped build.

Requires PyInstaller and pycdlib in the packaging Python. No disc assets are
included. Run strip_data.py first and regenerate recipes if its ranges change.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import zipfile
import package as upstream
import native_tables

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=('windows', 'linux'), default='windows' if sys.platform == 'win32' else 'linux')
    parser.add_argument('--portable', type=Path, default=ROOT / 'port/build/installer-portable')
    parser.add_argument('--sdl', type=Path)
    parser.add_argument('--launcher', type=Path, default=ROOT / 'port/build/Play.exe')
    parser.add_argument('--output', type=Path, default=ROOT / 'port/build/native-setup')
    args = parser.parse_args()
    windows = args.platform == 'windows'
    if windows != (sys.platform == 'win32'):
        raise ValueError('Freeze Setup on the same operating system as the release.')
    dat = (args.portable / 'bt3.dat').read_bytes()
    native_tables.check_recipe_coverage(dat)
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError('Choose an empty packaging output folder.')
    output.mkdir(parents=True, exist_ok=True)
    payload = output / 'payload'
    engine = payload / 'engine'
    engine.mkdir(parents=True, exist_ok=True)
    if windows:
        if not args.launcher.is_file():
            raise ValueError('Build Play.exe with port/tools/build_native_launcher.py first.')
        shutil.copy2(args.launcher, payload / 'Play.exe')
    shutil.copy2(ROOT / 'docs/assets/tag-team-logo.png', payload / 'Tag Team Mod Logo.png')
    binary = 'TagTeam.exe' if windows else 'TagTeam'
    source_binary = 'bt3.exe' if windows else 'bt3'
    mem = args.portable / (source_binary + '.mem')
    if not mem.is_file():
        mem = ROOT / ('port/build/bt3.exe.mem' if windows else 'port/build/bt3_64.mem')
    sdl = args.sdl or (ROOT / 'port/build/SDL3.dll' if windows else Path('/usr/local/lib/libSDL3.so.0'))
    for src, name in [(args.portable / source_binary, binary), (mem, binary + '.mem'),
                      (sdl, 'SDL3.dll' if windows else 'lib/libSDL3.so.0')]:
        (engine / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, engine / name)
    if not windows:
        (engine / binary).chmod(0o755)
    if dat[:4] != b'BT3D':
        raise ValueError('Use the manifest from strip_data.py')
    (engine / 'TagTeam.dat').write_bytes(b'TTMD' + dat[4:])
    licenses = payload / 'licenses'
    licenses.mkdir(exist_ok=True)
    upstream.take_translations(payload)
    upstream.take_lang(payload)
    upstream.take_fonts(payload, licenses)
    upstream.take_tbo_art(payload, licenses)
    for source, name in [('LICENSE', 'AGPL-3.0.txt'), ('LICENSE-MIT', 'upstream-MIT.txt'),
        ('port/third_party/imgui/LICENSE.txt', 'dear-imgui.txt'),
        ('port/third_party/newlib_libm/COPYING.NEWLIB', 'newlib.txt'),
        ('port/third_party/xxhash/LICENSE', 'xxhash.txt'), ('port/third_party/dr_libs/LICENSE', 'dr_mp3.txt')]:
        shutil.copy2(ROOT / source, licenses / name)
    shutil.copy2(ROOT / 'port/licenses/SDL.txt', licenses / 'SDL.txt')
    launcher = 'Play.exe' if windows else 'Play.sh'
    (payload / 'Read me.txt').write_text(
        f'Tag Team Mod — Native Beta .2\n\nStart {launcher}. Press Select at the main menu for Modded Modes.\n'
        'Double-click the game picture or press Alt+Enter to switch fullscreen. F11 also works.\n'
        'F1 opens graphics, controller, language and mod settings. Online modes are in the game menu.\n'
        'Your saves are in saves/. Setup copies imported saves; it never changes the original.\n'
        'English and Spanish mod menus are included. Disc audio comes from your selected disc.\n'
        'BT3: USA, Europe/Australia, Japan. BT4: B14 REV2 English and Spanish.\n\n'
        'Source: https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-\n', encoding='utf-8')
    manifest = {p.relative_to(payload).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(payload.rglob('*')) if p.is_file() and p.name != 'manifest.json'}
    (payload / 'release.json').write_text(json.dumps(dict(version='beta.2', platform=args.platform+'-x86_64'))+'\n')
    # Include release metadata in the same integrity manifest as every asset.
    manifest['release.json']=hashlib.sha256((payload/'release.json').read_bytes()).hexdigest()
    (payload / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--windowed',
        '--name', 'Setup', '--distpath', str(output), '--workpath', str(ROOT / ('port/build/setup-freeze-'+args.platform)),
        '--specpath', str(ROOT / ('port/build/setup-freeze-'+args.platform)), '--paths', str(ROOT / 'port/tools'),
        '--add-data', str(ROOT / 'port/tools/native_table_recipes.json') + os.pathsep + '.',
        str(ROOT / 'port/tools/native_setup.py')], check=True)
    setup = 'Setup.exe' if windows else './Setup.sh'
    (output / 'Read me.txt').write_text(f'Extract the whole archive, then run {setup}.\n'
        'Choose your disc image, English or Spanish, and an optional existing save.\n'
        'Setup needs no separate Python installation. Keep the payload folder beside Setup.\n'
        f'After installation, open {launcher} in the folder you chose.\n\n'
        f'Extrae todo el archivo y ejecuta {setup}. Elige el disco, el idioma y una partida existente si la tienes.\n'
        'No hace falta instalar Python. Conserva la carpeta payload junto a Setup.\n'+
        ('' if windows else '\nLinux: x86-64, glibc 2.35 or newer (Ubuntu 22.04+), and a working Vulkan graphics driver.\nRun Setup.sh and Play.sh as your normal user, without sudo.\n'), encoding='utf-8')
    archive = output.parent / ('Tag-Team-Recomp-Beta.2-Windows.zip' if windows else 'Tag-Team-Recomp-Beta.2-Linux-x86_64.tar.gz')
    if windows:
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for p in sorted(output.rglob('*')):
                if p.is_file():z.write(p, p.relative_to(output))
    else:
        (output/'Setup.sh').write_text('#!/bin/sh\nset -eu\ncd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"\nexec "$PWD/Setup" "$@"\n')
        (output/'Setup.sh').chmod(0o755)
        with tarfile.open(archive, 'w:gz') as tar:
            def public_metadata(info):
                info.uid=info.gid=0;info.uname=info.gname=''
                return info
            for p in sorted(output.iterdir()):tar.add(p, arcname=p.name, filter=public_metadata)
    print(archive)


if __name__ == '__main__':
    main()
