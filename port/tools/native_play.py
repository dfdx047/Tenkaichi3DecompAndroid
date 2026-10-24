"""One developer launcher for every imported disc. Online is selected in-game."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    exe = ROOT / 'port/build/bt3.exe'
    if not exe.is_file():
        raise SystemExit('Build the native game first; see NATIVE_TAG_TEAM.md.')
    choices = []
    for folder in sorted(ROOT.glob('gamedata*')):
        marker = folder / 'native-disc.json'
        if marker.is_file():
            info = json.loads(marker.read_text())
            label = 'BT4 — ' + info.get('language', 'en').upper() if info['game'] == 'bt4' else 'BT3 — ' + info.get('region', 'us').upper()
            choices.append((label, folder, info['game']))
        elif (folder / 'disc/SLUS_216.78').is_file():
            choices.append(('BT3 — USA', folder, 'bt3'))
    if not choices:
        raise SystemExit('Run Setup.cmd to install from your disc, or import it using port/tools/import_native_disc.py.')
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
