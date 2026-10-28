"""One developer launcher for every imported disc. Online is selected in-game."""
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def compatible_games(root):
    """Only imports in this launcher's installation, never sibling installs."""
    choices = []
    for folder in sorted(Path(root).glob('gamedata*')):
        if not folder.is_dir():
            continue
        marker = folder / 'native-disc.json'
        try:
            info = json.loads(marker.read_text(encoding='utf-8')) if marker.is_file() else {}
        except (OSError, ValueError):
            continue
        game = info.get('game')
        if game == 'bt4' and info.get('revision') == 'b14-rev2' and (folder / 'pzs4us1/00003.bin').is_file():
            choices.append(('BT4 — ' + info.get('language', 'en').upper(), folder, game))
        elif game == 'bt3' and info.get('region') in ('us', 'eu', 'jp') and (folder / 'pzs3us1/00000.bin').is_file():
            choices.append(('BT3 — ' + info['region'].upper(), folder, game))
        elif not marker.exists() and (folder / 'disc/SLUS_216.78').is_file() and (folder / 'pzs3us1/00000.bin').is_file():
            choices.append(('BT3 — USA', folder, 'bt3'))
    return choices


def main():
    exe = ROOT / ('port/build/bt3.exe' if sys.platform == 'win32' else 'port/build/bt3_64')
    if not exe.is_file():
        raise SystemExit('Build the native game first; see NATIVE_TAG_TEAM.md.')
    choices = compatible_games(ROOT)
    if not choices:
        raise SystemExit('Run ' + ('Setup.cmd' if sys.platform == 'win32' else 'Setup.sh') + ' to install from your disc, or import it using port/tools/import_native_disc.py.')
    selected = [choices[0]]
    if len(choices) > 1:
        import tkinter as tk
        from tkinter import ttk
        window = tk.Tk()
        window.title('Tag Team Mod')
        frame = ttk.Frame(window, padding=24)
        frame.pack(fill='both', expand=True)
        ttk.Label(frame, text='Choose your game', font=('Segoe UI', 16, 'bold')).pack(anchor='w', pady=(0, 12))
        combo = ttk.Combobox(frame, values=[c[0] for c in choices], state='readonly', width=30)
        combo.current(0)
        combo.pack(fill='x', pady=(0, 14))
        selected.clear()
        def play():
            selected.append(choices[combo.current()])
            window.destroy()
        ttk.Button(frame, text='Play', command=play).pack(anchor='e')
        window.mainloop()
    if not selected:
        return
    _, folder, game = selected[0]
    env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
    stem = 'native-bt4-prototype' if game == 'bt4' else 'native-prototype'
    env.update(BT3_TAG_TEAM='1', BT3_GS='gpu', BT3_DATA=str(folder), BT3_MENU_ITEM4='0', BT3_UPDATE_CHECK='0',
        BT3_SAVES=str(ROOT / f'port/build/{stem}-saves'), BT3_SETTINGS=str(ROOT / f'port/build/{stem}-settings.txt'))
    raise SystemExit(subprocess.call([str(exe)], cwd=ROOT, env=env))


if __name__ == '__main__':
    main()
