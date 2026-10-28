"""Copy a selected native/PCSX2 save; never alter the source card."""
from pathlib import Path
import re
from mc_extract import Card

CANONICAL = 'BASLUS-21678DBZT3'
SAVE_NAMES = re.compile(r'^(?:BASLUS-21678DBZT3|BESLES-54945DBZT3|BISLPS-25815DBZSM)$', re.I)


def collect(source):
    source = Path(source)
    if source.is_file() and source.suffix.lower() == '.ps2':
        card = Card(source)
        entries = list(card.walk())
        candidates = [(n, size, first) for n, size, first in entries
                      if SAVE_NAMES.fullmatch(Path(n).name) and size == 16384]
        if len(candidates) != 1:
            raise ValueError('Select a card with exactly one BT3/BT4 save, or select the individual save folder.')
        name, size, first = candidates[0]
        folder = str(Path(name).parent).replace('\\', '/')
        files = {Path(n).name: card.read(c, length) for n, length, c in entries
                 if str(Path(n).parent).replace('\\', '/') == folder and Path(n).name in (Path(name).name, 'icon.sys', 'dbzsm.ico')}
        files[CANONICAL] = files.pop(Path(name).name)
        return files, None
    if source.is_file():
        candidates = [source] if SAVE_NAMES.fullmatch(source.name) else []
        root = source.parent
    elif source.is_dir():
        # Only standard card/save locations, not an unbounded recursive scan.
        candidates = []
        for directory in [source, source / 'card1', source / 'saves', source / 'saves/card1']:
            if not directory.is_dir():
                continue
            for child in directory.iterdir():
                if child.is_file() and SAVE_NAMES.fullmatch(child.name):
                    candidates.append(child)
                elif child.is_dir() and SAVE_NAMES.fullmatch(child.name) and (child / child.name).is_file():
                    candidates.append(child / child.name)
        candidates = list({p.resolve(): p for p in candidates}.values())
        root = source
    else:
        raise ValueError('The selected save location does not exist.')
    if len(candidates) != 1 or candidates[0].stat().st_size != 16384:
        raise ValueError('Choose a BT3/BT4 save folder, its 16 KB save file, or a PCSX2 .ps2 memory card.')
    save = candidates[0]
    files = {CANONICAL: save.read_bytes()}
    for name, limit in [('icon.sys', 4096), ('dbzsm.ico', 1024 * 1024)]:
        path = save.parent / name
        if path.is_file() and path.stat().st_size <= limit:
            files[name] = path.read_bytes()
    custom = None
    for parent in [root, save.parent.parent, save.parent.parent.parent]:
        path = parent / 'native-bt4-custom.bin'
        if path.is_file() and path.stat().st_size == 8 + 153 * 56:
            data = path.read_bytes()
            if data[:8] == b'BT4CUST1':
                custom = data
                break
    return files, custom


def write(collected, target):
    files, custom = collected
    target = Path(target)
    folder = target / 'card1' / CANONICAL
    folder.mkdir(parents=True, exist_ok=False)
    for name, data in files.items():
        (folder / name).write_bytes(data)
    if custom is not None:
        (target / 'native-bt4-custom.bin').write_bytes(custom)
