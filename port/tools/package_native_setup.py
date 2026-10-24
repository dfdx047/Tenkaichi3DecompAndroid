"""Package the Windows native installer from an already stripped build.

Requires PyInstaller and pycdlib in the packaging Python. No disc assets are
included. Run strip_data.py first and regenerate recipes if its ranges change.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
import package as upstream

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portable', type=Path, default=ROOT / 'port/build/installer-portable')
    parser.add_argument('--sdl', type=Path, default=ROOT / 'port/build/SDL3.dll')
    parser.add_argument('--output', type=Path, default=ROOT / 'port/build/native-setup')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    payload = output / 'payload'
    engine = payload / 'engine'
    engine.mkdir(parents=True, exist_ok=True)
    for src, name in [(args.portable / 'bt3.exe', 'TagTeam.exe'),
                      (ROOT / 'port/build/bt3.exe.mem', 'TagTeam.exe.mem'),
                      (args.sdl, 'SDL3.dll')]:
        shutil.copy2(src, engine / name)
    dat = (args.portable / 'bt3.dat').read_bytes()
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
    (payload / 'Read me.txt').write_text(
        'Tag Team Mod — Native\n\nStart Play.cmd. Press Select at the main menu for Modded Modes.\n'
        'F1 opens graphics, controller and language settings. Online modes are in the game menu.\n'
        'Your saves are in saves/. Setup copies imported saves; it never changes the original.\n'
        'English and Spanish mod menus are included. Disc audio comes from your selected disc.\n'
        'BT3: USA, Europe/Australia, Japan. BT4: B14 REV2 English and Spanish.\n\n'
        'Source: https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-\n', encoding='utf-8')
    manifest = {p.relative_to(payload).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(payload.rglob('*')) if p.is_file() and p.name != 'manifest.json'}
    (payload / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--windowed',
        '--name', 'Setup', '--distpath', str(output), '--workpath', str(ROOT / 'port/build/setup-freeze'),
        '--specpath', str(ROOT / 'port/build/setup-freeze'), '--paths', str(ROOT / 'port/tools'),
        '--add-data', str(ROOT / 'port/tools/native_table_recipes.json') + ';.',
        str(ROOT / 'port/tools/native_setup.py')], check=True)
    (output / 'Read me.txt').write_text('Extract the whole ZIP, then open Setup.exe.\n'
        'Choose your disc image, English or Spanish, and an optional existing save.\n'
        'Setup needs no separate Python installation. Keep the payload folder beside Setup.exe.\n'
        'After installation, open Play.cmd in the folder you chose.\n\n'
        'Extrae todo el ZIP y abre Setup.exe. Elige el disco, el idioma y una partida existente si la tienes.\n'
        'No hace falta instalar Python. Conserva la carpeta payload junto a Setup.exe.\n', encoding='utf-8')
    archive = output.parent / 'Tag Team Native - Setup.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in sorted(output.rglob('*')):
            if p.is_file():
                z.write(p, p.relative_to(output))
    print(archive)


if __name__ == '__main__':
    main()
