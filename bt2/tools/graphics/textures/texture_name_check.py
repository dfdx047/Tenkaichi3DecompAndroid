"""Texture name check (docs/plans/graphics/HD_TEXTURES_PLAN.md, step 1): how many of the textures a run drew
have a replacement in each pack.

A run with BT2_TEXTURE_NAMES=<names file> writes the PCSX2 name of each texture drawn ("<name> <count>" lines,
a "#" line with what the names' copy of GS memory did not follow). For each pack folder this prints how many
names it has, by the pack's subfolder, and how many match on the texture hash alone (the first part), which
separates a wrong texture hash from a wrong palette hash.

    python tools/graphics/textures/texture_name_check.py work/coverage/names.txt "work/HD_TEXTURES/SLUS-21441-texture pack 2"
"""
import argparse
import collections
from pathlib import Path


def read_names(path):
    names, notes = {}, []
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        if line.startswith('#'):
            notes.append(line[1:].strip())
        elif line.strip():
            name, count = line.split()
            names[name] = int(count)
    return names, notes


def normalized(name):
    """A pack's file name as PCSX2 reads it: the format word's bit 14 (TCC in older PCSX2 versions, which
    packs made with them still carry) is cleared."""
    parts = name.split('-')
    try:
        parts[-1] = f'{int(parts[-1], 16) & ~0x4000:08x}'
    except ValueError:
        return name
    return '-'.join(parts)


def pack_index(folder):
    """Replacement names of a pack, normalized: under its 'replacements' folder if it has one."""
    folder = Path(folder)
    root = folder / 'replacements' if (folder / 'replacements').is_dir() else folder
    index = {}
    for f in root.rglob('*'):
        if f.suffix.lower() in ('.dds', '.png'):
            index.setdefault(normalized(f.stem), f.parent.relative_to(root).as_posix() or '.')
    return index


def lookup(name, index):
    """The pack's entry for a drawn name: the name itself, or a replacement made without the palette part
    (it stands for every palette), as PCSX2 looks them up. A logged name may give two forms, "<used
    entries' name>/<whole palette's name>": the first found counts."""
    if '/' in name:
        for form in name.split('/'):
            found = lookup(form, index)
            if found is not None:
                return found
        return None
    if name in index:
        return index[name]
    parts = name.split('-')
    return index.get(f'{parts[0]}-{parts[-1]}') if len(parts) == 3 else None


def check(names, index):
    found = {n: lookup(n, index) for n in names if lookup(n, index) is not None}
    texture_hashes = {n.split('-')[0] for n in index}
    by_texture = sum(1 for n in names if n.split('-')[0] in texture_hashes)
    return found, by_texture


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('names')
    p.add_argument('packs', nargs='+')
    a = p.parse_args()
    names, notes = read_names(a.names)
    print(f'{len(names)} texture names drawn')
    for note in notes:
        print(' ', note)
    for pack in a.packs:
        index = pack_index(pack)
        found, by_texture = check(names, index)
        print(f'{Path(pack).name}: {len(index)} replacements; {len(found)} of the drawn names found, '
              f'{by_texture} by the texture hash alone')
        for folder, count in collections.Counter(found.values()).most_common():
            print(f'  {folder}: {count}')


if __name__ == '__main__':
    main()
