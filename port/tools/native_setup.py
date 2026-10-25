"""Install the native Tag Team game from the player's own disc and optional save."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import sys
import tempfile
import threading

import native_save
import native_tables
from import_native_disc import extract

BASE = Path(sys.executable).parent if getattr(sys, 'frozen', False) else Path(__file__).resolve().parents[2]
PAYLOAD = BASE / 'payload' if getattr(sys, 'frozen', False) else BASE / 'port/build/native-setup/payload'

PLAY = '''@echo off
setlocal
cd /d "%~dp0"
for /f "tokens=1 delims==" %%V in ('set BT3_ 2^>nul') do set "%%V="
set "BT3_TAG_TEAM=1"
set "BT3_GS=gpu"
set "BT3_DATA=%~dp0gamedata"
set "BT3_SAVES=%~dp0saves"
set "BT3_SETTINGS=%~dp0settings.txt"
set "BT3_UPDATE_CHECK=0"
set "BT3_MENU_ITEM4=0"
start "" /wait "%~dp0engine\\TagTeam.exe"
if errorlevel 1 (
    echo The game stopped. Check bt3_crash.txt in this folder if it was created.
    pause
    exit /b 1
)
'''


def payload_files(payload):
    manifest = json.loads((payload / 'manifest.json').read_text(encoding='utf-8'))
    for name, expected in manifest.items():
        path = payload / name
        if path.resolve().parent != payload.resolve() and payload.resolve() not in path.resolve().parents:
            raise ValueError('Invalid setup manifest path')
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f'Setup file is missing or damaged: {name}. Extract the complete installer ZIP again.')
    for required in ('engine/TagTeam.exe', 'engine/TagTeam.exe.mem', 'engine/TagTeam.dat', 'engine/SDL3.dll'):
        if required not in manifest:
            raise ValueError('Incomplete setup payload')
    return manifest


def install(iso, target, language='en', save=None, payload=PAYLOAD, progress=print):
    iso, target, payload = Path(iso).resolve(), Path(target).resolve(), Path(payload).resolve()
    if language not in ('en', 'es'):
        raise ValueError('Choose English or Spanish.')
    if target == target.parent or target == payload or target in payload.parents or payload in target.parents:
        raise ValueError('Choose a separate installation folder.')
    if target.exists() and (not target.is_dir() or any(target.iterdir())):
        raise ValueError('Choose an empty folder. Existing games, settings and saves will not be replaced.')
    if not target.parent.is_dir():
        raise ValueError('Choose an installation folder whose parent exists.')
    progress('Checking disc / Comprobando disco...')
    profile, blobs, sizes = native_tables.identify(iso)
    tables = native_tables.reconstruct(profile, blobs, sizes)
    progress('Checking setup files / Comprobando archivos...')
    files = payload_files(payload)
    native_tables.verify(tables, (payload / 'engine/TagTeam.dat').read_bytes())
    imported = native_save.collect(save) if save else None
    required = iso.stat().st_size + 1024 ** 3
    if shutil.disk_usage(target.parent).free < required:
        raise ValueError(f'At least {required / 1024**3:.1f} GB of free space is needed.')
    stage = Path(tempfile.mkdtemp(prefix='.tag-team-install-', dir=target.parent))
    try:
        progress(f"Importing {profile['id'].upper()}. This may take a few minutes. / Importando el disco...")
        extract(iso, stage / 'gamedata')
        for i, data in enumerate(tables):
            (stage / 'gamedata' / f'native-tables{i}.bin').write_bytes(data)
        progress('Installing game / Instalando el juego...')
        for name in files:
            destination = stage / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(payload / name, destination)
        (stage / 'settings.txt').write_text(f'menu_lang={language}\nlanguage_prompt_always=0\n', encoding='utf-8')
        if imported:
            native_save.write(imported, stage / 'saves')
        else:
            (stage / 'saves').mkdir()
        (stage / 'Play.cmd').write_bytes(PLAY.replace('\n', '\r\n').encode('utf-8'))
        (stage / 'installation.json').write_text(json.dumps(dict(schema=1, profile=profile['id'], language=language,
            save_imported=bool(imported), files=files), indent=2) + '\n', encoding='utf-8')
        if target.exists():
            target.rmdir()  # Still empty; never remove an existing installation.
        stage.replace(target)
    finally:
        if stage.exists():
            # Only the unique temporary folder created by this invocation.
            assert stage.parent == target.parent and stage.name.startswith('.tag-team-install-')
            shutil.rmtree(stage)
    progress('Ready. Start Play.cmd. / Listo. Abre Play.cmd.')
    return target


def gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    window = tk.Tk()
    window.title('Tag Team Mod — Native Setup')
    window.geometry('760x460')
    window.minsize(660, 440)
    window.columnconfigure(0, weight=1)
    pane = ttk.Frame(window, padding=22)
    pane.grid(sticky='nsew')
    pane.columnconfigure(1, weight=1)
    lang, disc, destination, save = (tk.StringVar(value=v) for v in ('English', '', str(Path.home() / 'Tag Team Native'), ''))
    title = ttk.Label(pane, text='Tag Team Mod', font=('Segoe UI', 22, 'bold'))
    title.grid(row=0, column=0, columnspan=3, sticky='w', pady=(0, 6))
    subtitle = ttk.Label(pane, text='Native BT3 & BT4 • Choose your disc and start playing')
    subtitle.grid(row=1, column=0, columnspan=3, sticky='w', pady=(0, 18))
    labels, buttons = {}, []
    for row, key, variable in [(2, 'Language', lang), (3, 'Disc image', disc), (4, 'Install folder', destination), (5, 'Existing save (optional)', save)]:
        labels[key] = ttk.Label(pane, text=key)
        labels[key].grid(row=row, column=0, sticky='w', padx=(0, 12), pady=7)
        if row == 2:
            choice = ttk.Combobox(pane, textvariable=lang, values=('English', 'Español'), state='readonly', width=18)
            choice.grid(row=row, column=1, sticky='w')
        else:
            ttk.Entry(pane, textvariable=variable).grid(row=row, column=1, columnspan=2 if row == 5 else 1, sticky='ew')
    def choose_disc():
        name = filedialog.askopenfilename(filetypes=[('Disc image', '*.iso')])
        if name:
            disc.set(name)
    def choose_target():
        name = filedialog.askdirectory()
        if name:
            destination.set(str(Path(name) / 'Tag Team Native'))
    def choose_save(folder=False):
        name = filedialog.askdirectory(title='Recomp save folder / Carpeta de partida Recomp') if folder else filedialog.askopenfilename(
            title='PCSX2 memory card / Tarjeta de memoria de PCSX2', filetypes=[('PCSX2 memory card (.ps2)', '*.ps2')])
        if name:
            save.set(name)
    for row, action in [(3, choose_disc), (4, choose_target)]:
        button = ttk.Button(pane, text='Browse…', command=action)
        button.grid(row=row, column=2, padx=(8, 0))
        buttons.append(button)
    save_options = ttk.Frame(pane)
    save_options.grid(row=6, column=1, columnspan=2, sticky='ew', pady=(4, 0))
    save_options.columnconfigure(1, weight=1)
    card = ttk.Button(save_options, text='PCSX2 memory card (.ps2)…', command=choose_save)
    card.grid(row=0, column=0, sticky='w')
    save_or = ttk.Label(save_options, text='Or', anchor='center')
    save_or.grid(row=0, column=1, sticky='ew', padx=12)
    folder = ttk.Button(save_options, text='Recomp save folder…', command=lambda: choose_save(True))
    folder.grid(row=0, column=2, sticky='e')
    help_label = ttk.Label(pane, text='PCSX2: choose the .ps2 memory-card file.\nRecomp: choose the saves folder from an existing recomp installation.\nYour original save is kept.', wraplength=640, justify='left')
    help_label.grid(row=7, column=0, columnspan=3, sticky='ew', pady=14)
    status = ttk.Label(pane, text='', wraplength=640)
    status.grid(row=9, column=0, columnspan=3, sticky='ew', pady=10)
    bar = ttk.Progressbar(pane, mode='determinate', value=0)
    bar.grid(row=10, column=0, columnspan=3, sticky='ew')
    events, busy, installed = queue.Queue(), False, None
    def translate(*_):
        spanish = lang.get() == 'Español'
        names = ['Idioma', 'Imagen de disco', 'Carpeta de instalación', 'Partida existente (opcional)']
        for (key, label), text in zip(labels.items(), names):
            label.configure(text=text if spanish else key)
        for button in buttons:
            button.configure(text='Examinar…' if spanish else 'Browse…')
        card.configure(text='Tarjeta PCSX2 (.ps2)…' if spanish else 'PCSX2 memory card (.ps2)…')
        folder.configure(text='Carpeta de partida Recomp…' if spanish else 'Recomp save folder…')
        save_or.configure(text='o' if spanish else 'Or')
        start.configure(text='Instalar' if spanish else 'Install')
        help_label.configure(text='PCSX2: elige el archivo de tarjeta de memoria .ps2.\nRecomp: elige la carpeta saves de una instalación recomp existente.\nSe conserva la partida original.' if spanish else 'PCSX2: choose the .ps2 memory-card file.\nRecomp: choose the saves folder from an existing recomp installation.\nYour original save is kept.')
        subtitle.configure(text='BT3 y BT4 nativos • Elige tu disco y empieza a jugar' if spanish else 'Native BT3 & BT4 • Choose your disc and start playing')
    def begin():
        nonlocal busy
        if busy:
            return
        args = (disc.get(), destination.get(), 'es' if lang.get() == 'Español' else 'en', save.get() or None)
        if not args[0]:
            messagebox.showerror('Setup', 'Choose your disc image. / Elige tu imagen de disco.')
            return
        busy = True
        start.configure(state='disabled')
        bar.configure(mode='indeterminate', value=0)
        bar.start()
        def worker():
            try:
                result = install(*args, progress=lambda text: events.put(('status', text)))
                events.put(('done', result))
            except Exception as error:
                events.put(('error', str(error)))
        threading.Thread(target=worker, daemon=True).start()
    def poll():
        nonlocal busy, installed
        while not events.empty():
            kind, value = events.get_nowait()
            status.configure(text=str(value))
            if kind in ('done', 'error'):
                busy = False
                bar.stop()
                bar.configure(mode='determinate', value=100 if kind == 'done' else 0)
                start.configure(state='normal')
                if kind == 'done':
                    installed = value
                    status.configure(text=f'Ready / Listo: {value}')
                    start.configure(text='Play / Jugar', command=lambda: os.startfile(str(installed / 'Play.cmd')))
                else:
                    messagebox.showerror('Setup', value)
        window.after(100, poll)
    start = ttk.Button(pane, text='Install', command=begin)
    start.grid(row=8, column=2, sticky='e')
    choice.bind('<<ComboboxSelected>>', translate)
    def close():
        if busy:
            messagebox.showinfo('Setup', 'Please wait for installation to finish. / Espera a que termine la instalación.')
        else:
            window.destroy()
    window.protocol('WM_DELETE_WINDOW', close)
    window.after(100, poll)
    window.mainloop()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path)
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--language', choices=['en', 'es'], default='en')
    parser.add_argument('--save', type=Path)
    parser.add_argument('--payload', type=Path, default=PAYLOAD)
    args = parser.parse_args()
    if args.iso:
        if not args.destination:
            parser.error('--destination is required with --iso')
        install(args.iso, args.destination, args.language, args.save, args.payload)
    else:
        gui()
