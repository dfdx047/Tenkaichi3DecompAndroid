#!/usr/bin/env python3
"""Build the menu and battle texture/layout index from labels.json. See docs/port/menu_font.md.

The game makes the list of labels itself, from the disc data, the first time it starts (port/src/gs/menu_labels.c):
gamedata/game-text/labels.tsv and name-styles.tsv, which it reads, and labels.json, the editable layout list.
This tool turns an edited labels.json into labels.tsv and name-styles.tsv again; translated wording stays in
game-text/translations/labels/<locale>.json:

    python3 port/tools/menu_font.py                      gamedata/game-text of this checkout
    python3 port/tools/menu_font.py --out <folder>       another game-text folder (a release's gamedata/game-text)

For applying edits it reads labels.json. Standard library only.
The optional --compare-styles command also reads the disc, and needs Pillow and a C compiler.
To start over, delete labels.json and labels.tsv: the game writes both again at its next start."""
import argparse
import ctypes
import json
import os
import pathlib
import re
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
FONT_MENU, FONT_DIALOGUE, FONT_BATTLE = 1, 2, 3

# The first lines of the two files, matching port/src/gs/menu_labels.c (ml_tsv).
LABELS_HEAD = ('# BT3 menu text v13: hash width height y row_height cap_height baseline center margin width_milli x row_width '
               'art_width top bottom middle_rgb has_middle outline italic outline_milli outer_milli outer_rgb shadow '
               'outline_bottom bevel_milli bevel_light bevel_dark depth_milli mid_gradient font_face label_id quick')
STYLES_HEAD = '# kind (0 song, 1 selected song, 2 stage) cap_height baseline margin width_milli top bottom outline'


def font_chars(path):
    """The characters a TrueType file has glyphs for (its Unicode character map, formats 4 and 12)."""
    chars = set()
    try:
        d = path.read_bytes()
        tables = {d[12 + 16 * i:16 + 16 * i]: struct.unpack_from('>I', d, 20 + 16 * i)[0] for i in range(struct.unpack_from('>H', d, 4)[0])}
        cmap, best = tables[b'cmap'], None
        for i in range(struct.unpack_from('>H', d, cmap + 2)[0]):
            platform, encoding, offset = struct.unpack_from('>HHI', d, cmap + 4 + 8 * i)
            kind = struct.unpack_from('>H', d, cmap + offset)[0]
            if (platform == 0 or (platform == 3 and encoding in (1, 10))) and kind in (4, 12) and (best is None or kind > best[0]):
                best = (kind, cmap + offset)
        if best is None:
            return chars
        kind, at = best
        if kind == 4:
            segs = struct.unpack_from('>H', d, at + 6)[0] // 2
            ends, starts = at + 14, at + 16 + 2 * segs
            deltas, offsets = starts + 2 * segs, starts + 4 * segs
            for i in range(segs):
                end, start, delta, offset = (struct.unpack_from('>H', d, base + 2 * i)[0] for base in (ends, starts, deltas, offsets))
                for c in range(start, min(end, 0xFFFE) + 1):
                    glyph = c if offset == 0 else struct.unpack_from('>H', d, offsets + 2 * i + offset + 2 * (c - start))[0]
                    if glyph and (glyph + delta) & 0xFFFF:
                        chars.add(c)
        else:
            for i in range(min(struct.unpack_from('>I', d, at + 12)[0], 100000)):
                start, end, glyph = struct.unpack_from('>III', d, at + 16 + 12 * i)
                if end < 0x110000 and end - start < 0x20000:
                    chars.update(range(start, end + 1))
    except (OSError, KeyError, struct.error, IndexError):
        pass
    return chars


def font_entries(fonts):
    """The valid rows of fonts.tsv, in the same order the game numbers them."""
    entries = []
    for line in (fonts / 'fonts.tsv').read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):
            continue
        fields = line.split('\t')
        if len(fields) not in (2, 3):
            continue
        path, name = fields[0], fields[1].strip()
        use = fields[2].strip() if len(fields) == 3 else '3'  # (the sum of 1 Menu, 2 Dialogue / UI, 4 Battle)
        if (not path or len(path) >= 256 or path.startswith('/') or '\\' in path or ':' in path or
                path == '..' or path.startswith('../') or '/../' in path or
                not name or len(name) >= 256 or not use.isdigit() or not 1 <= int(use) <= 7):
            continue
        bits = int(use)
        entries.append((path, name, {role for role, bit in ((FONT_MENU, 1), (FONT_DIALOGUE, 2), (FONT_BATTLE, 4)) if bits & bit}))
    return entries


def supported_chars(fonts):
    """The characters some font of the "Menu font" list has (fonts.tsv in the fonts folder). None: no list found."""
    if not (fonts / 'fonts.tsv').is_file():
        return None
    chars = set()
    for path, _, roles in font_entries(fonts):
        if FONT_MENU in roles:
            chars.update(font_chars(fonts / path))
    return chars


def rgb(value, what):
    if not isinstance(value, list) or len(value) != 3 or any(type(c) != int or not 0 <= c <= 255 for c in value):
        raise ValueError(f'{what}: expected three RGB values (0-255)')
    return ''.join(f'{c:02x}' for c in value)


def apply_labels(out, fonts):
    """labels.json into the bounded TSV files read by the game's renderer (port/src/gs/menu_text.c)."""
    config = json.loads((out / 'labels.json').read_text(encoding='utf-8'))
    palettes = config.get('color_styles')
    if not isinstance(palettes, dict) or not isinstance(config.get('labels'), list):
        raise ValueError('labels.json has no "color_styles" or no "labels" (delete it and labels.tsv, '
                         'and the game writes both again at its next start)')
    required = ('notice-blue/0/0', 'combo-red-digits/0/0', 'timer-white/0/0')
    present = {row.get('id') for row in config['labels'] if isinstance(row, dict)}
    if any(row_id not in present for row_id in required):
        raise ValueError('labels.json is missing battle labels; delete labels.json and labels.tsv, then start the game to regenerate them')

    def palette(row):
        name = row.get('color_style')
        if name not in palettes:
            raise ValueError(f"{row['id']}: color_style {name!r} is not one of color_styles")
        colors = palettes[name]
        return colors, [rgb(colors.get(key), f'color_styles.{name}.{key}') for key in ('top', 'bottom', 'outline')]

    def decorations(colors):
        widths = []
        for key, default in (('outline_width', None), ('outer_outline_width', 0), ('bevel_width', 0)):
            value = colors.get(key, default)
            if value is None and key == 'outline_width':
                widths.append(-1)  # the renderer's own width
            elif type(value) not in (int, float) or not 0 <= value <= (4 if key == 'bevel_width' else 8):
                raise ValueError(f'{key}: expected a width from 0 to {4 if key == "bevel_width" else 8}')
            else:
                widths.append(round(value * 1000))
        shadow = colors.get('shadow', True)
        if type(shadow) != bool and shadow not in ('hard', 'translucent-hard'):
            raise ValueError('shadow must be true, false, "hard", or "translucent-hard"')
        depth = colors.get('depth', 0)
        midpoint = colors.get('mid_gradient', False)
        if type(depth) not in (int, float) or not 0 <= depth <= 8:
            raise ValueError('depth must be from 0 to 8 pixels')
        if type(midpoint) != bool:
            raise ValueError('mid_gradient must be true or false')
        outer_color = colors.get('outer_outline', [0, 0, 0])
        depth_color = outer_color if depth else colors['outline']
        return [widths[0], widths[1], rgb(outer_color, 'outer_outline'),
                3 if shadow == 'translucent-hard' else 2 if shadow == 'hard' else int(shadow),
                rgb(colors.get('outline_bottom', depth_color), 'outline_bottom'), widths[2],
                rgb(colors.get('bevel_light', colors['top']), 'bevel_light'),
                rgb(colors.get('bevel_dark', depth_color), 'bevel_dark'), round(depth * 1000), int(midpoint)]

    lines = [LABELS_HEAD]
    # Each texture's fingerprint (the game's MenuLabels_Quick: 64 of its pixels) as the game wrote it, by the texture's
    # hash: labels.json has no pixels to make it from. A row without one is checked by the full hash.
    quick = {}
    old = out / 'labels.tsv'
    if old.exists():
        for line in old.read_text(encoding='utf-8', errors='replace').splitlines():
            parts = line.split('\t')
            if not line.startswith('#') and len(parts) >= 32 and re.fullmatch(r'[0-9a-fA-F]{16}', parts[-1]):
                quick[parts[0].lower()] = parts[-1]
    # The overlay uses these same styles for names of external songs and stages.
    styles = [STYLES_HEAD]
    for selected, id in ((0, 'song/0/0'), (1, 'song-highlighted/0/0'), (2, 'stage/0/0')):
        template = next((r for r in config['labels'] if r['id'] == id), None)
        if template:
            values = [selected, template['cap_height'], template['baseline'], template['margin'], round(template.get('width_scale', 1) * 1000), *palette(template)[1]]
            styles.append('\t'.join(map(str, values)))
    count = 0
    for row in config['labels']:
        if not re.fullmatch(r'[0-9a-fA-F]{16}', row['hash']):
            raise ValueError(f"{row['id']}: invalid texture hash")
        texture_width = row.get('texture_width', 512)
        if texture_width < 8 or texture_width > 512 or texture_width & (texture_width - 1):
            raise ValueError(f"{row['id']}: unsupported texture width")
        x = row.get('x', 0)
        row_width = row.get('width', texture_width)
        if not (0 <= x < texture_width and 1 <= row_width <= texture_width - x):
            raise ValueError(f"{row['id']}: invalid horizontal bounds")
        if not (1 <= row['texture_height'] <= 512 and 1 <= row['height'] <= 64 and
                0 <= row['y'] <= row['texture_height'] - row['height'] and 1 <= row['cap_height'] <= 64 and
                0 <= row['baseline'] <= (64 if row.get('original') == '∞' and row['id'].startswith('timer-') and
                                         row['height'] == 32 and row_width == 64 else row['height']) and
                0 <= row['margin'] < row_width / 2 and row['align'] in ('left', 'center')):
            raise ValueError(f"{row['id']}: invalid label bounds or style")
        width_scale = row.get('width_scale', 1.0)
        if not isinstance(width_scale, (int, float)) or not 0.5 <= width_scale <= 2.0:
            raise ValueError(f"{row['id']}: width_scale must be between 0.5 and 2")
        art_width = row.get('art_width', 0)
        if type(art_width) != int or not 0 <= art_width <= row_width:
            raise ValueError(f"{row['id']}: invalid art_width")
        if not row.get('enabled', True) or not row.get('original'):
            continue
        colors, fill = palette(row)
        italic = row.get('italic', False)
        if type(italic) != bool:
            raise ValueError(f"{row['id']}: italic must be true or false")
        # Only the unchanged original label has a measured artwork width.
        font_face = row.get('font_face', 'selected')
        if font_face not in ('selected', 'battle'):
            raise ValueError(f"{row['id']}: font_face must be selected or battle")
        middle = colors.get('middle')
        middle_rgb = rgb(middle, f"color_styles.{row['color_style']}.middle") if middle is not None else '000000'
        fields = [row['hash'], texture_width, row['texture_height'], row['y'], row['height'], row['cap_height'], row['baseline'],
                  int(row['align'] == 'center'), row['margin'], round(width_scale * 1000), x, row_width,
                  art_width, fill[0], fill[1], middle_rgb, int(middle is not None), fill[2],
                  int(italic), *decorations(colors), int(font_face == 'battle'), row['id']]
        if row['hash'].lower() in quick:
            fields.append(quick[row['hash'].lower()])
        lines.append('\t'.join(map(str, fields)))
        count += 1
    if count > 640:
        raise ValueError('The renderer supports at most 640 label rows')
    # each file whole or not at all; labels.tsv last, as the game writes them
    (out / 'name-styles.tsv.tmp').write_text('\n'.join(styles) + '\n', encoding='utf-8')
    (out / 'name-styles.tsv.tmp').replace(out / 'name-styles.tsv')
    (out / 'labels.tsv.tmp').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    (out / 'labels.tsv.tmp').replace(out / 'labels.tsv')
    print(f'{count} labels ready. Restart the game to load {out}/labels.tsv')


# Optional disc artwork comparison. Pillow is imported only by compare_styles().

def u32(data, offset=0):
    return struct.unpack_from('<I', data, offset)[0]


def section(data, index):
    return data[u32(data, index * 4) & ~3:]


def unpack(data):
    """The game's byte-pair compression, src/sys/bpe.c (no game code is executed)."""
    size, packed = struct.unpack_from('<II', data)
    if size > 64 * 1024 * 1024 or packed > len(data) - 8:
        raise ValueError('Invalid BPE header')
    p, out = 8, bytearray()
    while p < packed + 8:
        left, right = list(range(256)), [0] * 256
        c = 0
        while c < 256:
            count = data[p]; p += 1
            if count > 127:
                c += count - 127
                count = 0
            if c == 256:
                break
            for _ in range(count + 1):
                left[c] = data[p]; p += 1
                if c != left[c]:
                    right[c] = data[p]; p += 1
                c += 1
        block = data[p] * 256 + data[p + 1]; p += 2
        for value in data[p:p + block]:
            stack = [value]
            while stack:
                value = stack.pop()
                if left[value] == value:
                    out.append(value)
                else:
                    stack.extend((right[value], left[value]))
                if len(out) > size or len(stack) > 256:
                    raise ValueError('Invalid BPE block')
        p += block
    if p != packed + 8 or len(out) != size:
        raise ValueError('BPE size mismatch')
    return bytes(out)


# The same GS layout tables as namefont.c. Map each decoded texel to its uploaded byte once.
BLOCK = ((0,1,4,5,16,17,20,21),(2,3,6,7,18,19,22,23),(8,9,12,13,24,25,28,29),(10,11,14,15,26,27,30,31))
COL = ((0,1,4,5,8,9,12,13),(2,3,6,7,10,11,14,15),(16,17,20,21,24,25,28,29),(18,19,22,23,26,27,30,31),
       (32,33,36,37,40,41,44,45),(34,35,38,39,42,43,46,47),(48,49,52,53,56,57,60,61),(50,51,54,55,58,59,62,63))
COL8 = ((0,4,16,20,32,36,48,52,2,6,18,22,34,38,50,54),(8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62),
        (33,37,49,53,1,5,17,21,35,39,51,55,3,7,19,23),(41,45,57,61,9,13,25,29,43,47,59,63,11,15,27,31))
_MAPPING = {}


BLOCK16 = ((0,2,8,10),(1,3,9,11),(4,6,12,14),(5,7,13,15),(16,18,24,26),(17,19,25,27),(20,22,28,30),(21,23,29,31))
COL4 = ((0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110),
        (16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126),
        (65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47),
        (81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63))


def texture(data, index, palette_index=None):
    """Decode indexed menu textures using their actual GS upload format and pitch."""
    e = data[u32(data,4)*4+index*64:][:64]
    if len(e)<64:
        return None
    tex0 = struct.unpack_from('<Q',e,48)[0]
    w,h = 1<<((tex0>>26)&15),1<<((tex0>>30)&15)
    psm = (tex0>>20)&63
    if w>512 or h>512 or psm not in (19,20) or u32(e,24)>>8 != 0:
        return None
    pixels = u32(e)*4+96
    palette_entry = e if palette_index is None else data[u32(data,4)*4+palette_index*64:][:64]
    if len(palette_entry) < 64:
        return None
    palette = u32(palette_entry,4)*4+96
    reg = struct.unpack_from('<Q',data,pixels-48)[0]
    uw,uh = reg&4095,(reg>>32)&4095
    dbw,tbw = u32(e,24)&63,(tex0>>14)&63
    key=(w,h,psm,uw,uh,dbw,tbw)
    if key not in _MAPPING:
        mem=[None]*(512*512*4)
        for y in range(uh):
            for x in range(uw):
                a=(((y&~31)*dbw+((x>>1)&~31)+BLOCK[(y>>3)&3][(x>>3)&7])<<6)+COL[y&7][x&7]
                at=(y*uw+x)*4
                mem[a*4:a*4+4]=range(at,at+4)
        mapping=[]
        for y in range(h):
            for x in range(w):
                col=(y&15)>>2;sx=x^(4 if col&1 else 0)
                if psm==19:
                    a=((((y>>1)&~31)*(tbw>>1)+((x>>2)&~31)+BLOCK[(y>>4)&3][(x>>4)&7])<<8)+COL8[y&3][sx&15]+col*64
                    address,shift=a,0
                else:
                    a=((((y>>2)&~31)*(tbw>>1)+((x>>2)&~31)+BLOCK16[(y>>4)&7][(x>>5)&3])<<9)+COL4[y&3][sx&31]+col*128
                    address,shift=a>>1,(a&1)*4
                if mem[address] is None:
                    return None  # dimensions outside the uploaded rectangle
                mapping.append((mem[address],shift))
        _MAPPING[key]=mapping
    count=256 if psm==19 else 16
    colors=[]
    for i in range(count):
        c=(i&0xE7)|((i&8)<<1)|((i&16)>>1) if psm==19 else i
        colors.append(data[palette+c*4:palette+c*4+4])
    mask=count-1
    return b''.join(colors[(data[pixels+a]>>shift)&mask] for a,shift in _MAPPING[key])


def sheets(pack, part, width=512):
    data = section(pack, part)
    result = []
    for i in range(u32(data)):
        entry = u32(data,4)*4 + i*64
        tex0 = struct.unpack_from('<Q',data,entry+48)[0]
        if 1 << ((tex0 >> 26) & 15) == width:
            rgba = texture(data,i)
            if rgba is not None:
                result.append(rgba)
    return result


def compare_styles(data_root, labels_dir, fonts_dir, font_arg=None, output=None):
    """Show an original disc label and the selected font for every color style."""
    from PIL import Image, ImageDraw, ImageFont

    config = json.loads((labels_dir/'labels.json').read_text(encoding='utf-8'))
    rows = {row['id']: row for row in config['labels']}
    fonts = font_entries(fonts_dir)
    choices = [(index, path, name) for index, (path, name, roles) in enumerate(fonts, 1) if FONT_MENU in roles]
    if not choices:
        raise ValueError('No Menu font is listed in fonts.tsv')
    if font_arg:
        matches = [choice for choice in choices if font_arg in (choice[1], choice[2], pathlib.Path(choice[1]).name)]
        if len(matches) != 1:
            raise ValueError('Choose one Menu font by path or display name: ' + ', '.join(c[1] for c in choices))
        choice, path, name = matches[0]
    else:
        choice, path, name = choices[0]

    archive = (data_root/'pzs3us1'/'00449.bin').read_bytes()
    hud_archive = (data_root/'pzs3us1'/'00005.bin').read_bytes()
    timer = section(hud_archive, 1)
    notice = section(hud_archive, 4)
    combo = section(hud_archive, 5)
    main = unpack(section(archive, 2))
    title = unpack(section(archive, 1))
    examples = [
        ('purple', 'title-normal/0/0', lambda: texture(section(title, 8), 0)),
        ('main-menu', 'main-5/0/0', lambda: sheets(main, 5)[0]),
        ('main-heading', 'main-heading/0/0', lambda: texture(section(main, 23), 1)),
        ('orange', 'title-selected/0/0', lambda: texture(section(title, 8), 1)),
        ('title-prompt', 'title-press-start/0/0', lambda: texture(section(title, 6), 0)),
    ]
    battle = next(((index, path, name) for index, (path, name, roles) in enumerate(fonts, 1) if FONT_BATTLE in roles), None)
    battle_path = fonts_dir/battle[1] if battle else None
    if battle_path is not None and battle_path.is_file():
        examples += [
            ('notice-blue', 'notice-blue/0/0', lambda: texture(notice, 0)),
            ('notice-orange', 'notice-orange/0/0', lambda: texture(notice, 1)),
            ('notice-red', 'notice-red/0/0', lambda: texture(notice, 2)),
            ('notice-silver', 'notice-silver/0/0', lambda: texture(notice, 3)),
            ('notice-green', 'notice-green/0/0', lambda: texture(notice, 4)),
            ('notice-gold', 'notice-gold/0/0', lambda: texture(notice, 5)),
            ('notice-purple', 'notice-purple/0/0', lambda: texture(notice, 6)),
            ('combo-red-digits', 'combo-red-digits/0/8', lambda: texture(combo, 0)),
            ('combo-hits', 'combo-hits/0/0', lambda: texture(combo, 2)),
            ('combo-orange-digits', 'combo-orange-digits/0/8', lambda: texture(combo, 1)),
            ('combo-message', 'combo-counter/0/0', lambda: texture(combo, 3)),
            ('combo-message', 'combo-lock-on/0/0', lambda: texture(combo, 5)),
            ('combo-message', 'combo-first-attack/0/0', lambda: texture(combo, 6)),
            ('combo-message', 'combo-boost/0/0', lambda: texture(combo, 7)),
            ('combo-power', 'combo-max-power/0/0', lambda: texture(combo, 4)),
            ('timer-white', 'timer-white/0/8', lambda: texture(timer, 0, 0)),
            ('timer-yellow', 'timer-yellow/0/8', lambda: texture(timer, 0, 1)),
            ('timer-red', 'timer-red/0/8', lambda: texture(timer, 0, 2)),
            ('win-counter', 'win-counter/0/0', lambda: texture(timer, 4)),
            ('blast-stock-counter', 'blast-stock-counter/0/4', lambda: texture(timer, 14)),
        ]
    with tempfile.TemporaryDirectory() as temporary:
        library = pathlib.Path(temporary)/'menu_text.so'
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-DUI_TEXT_HOST_ALLOC', str(ROOT/'port/src/gs/menu_text.c'),
                        str(ROOT/'port/src/gs/menu_labels.c'), str(ROOT/'port/src/gs/menu_translation.c'),
                        str(ROOT/'port/src/gs/ui_text_extract.c'), str(ROOT/'port/src/plat_settings.c'),
                        str(ROOT/'port/src/gs/ui_text_translate.c'),
                        '-o', str(library), '-lm', '-lpthread'], check=True)
        prior = {key: os.environ.get(key) for key in ('BT3_MENU_FONT', 'BT3_FONTS', 'BT3_DATA', 'BT3_MENU_LANG')}
        os.environ['BT3_MENU_FONT'] = str(labels_dir.resolve())
        os.environ['BT3_FONTS'] = str(fonts_dir.resolve())
        os.environ['BT3_DATA'] = str(data_root.resolve())
        os.environ['BT3_MENU_LANG'] = config.get('locale', 'en')
        try:
            lib = ctypes.CDLL(str(library))
            lib.MenuText_SetFont.argtypes = [ctypes.c_int]
            lib.MenuText_SetBattleFont.argtypes = [ctypes.c_int]
            lib.MenuText_Render.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                            ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
            lib.MenuText_Render.restype = ctypes.c_void_p
            libc = ctypes.CDLL(None)
            libc.free.argtypes = [ctypes.c_void_p]
            lib.MenuText_SetFont(choice)
            if battle_path is not None and battle_path.is_file():
                lib.MenuText_SetBattleFont(battle[0])
                if lib.MenuText_GetBattleFont() != battle[0]:
                    raise ValueError(f'Could not load Battle font: {battle_path}')
            if lib.MenuText_GetFont() != choice:
                raise ValueError(f'Could not load Menu font: {name} ({path})')
            rendered_rows = []
            scale = 4
            for style, label_id, original_texture in examples:
                row = rows.get(label_id)
                if row is None or row.get('color_style') != style or not row.get('enabled', True):
                    raise ValueError(f'{label_id}: no enabled {style} label for comparison')
                source = original_texture()
                if source is None:
                    raise ValueError(f'{label_id}: disc texture could not be decoded')
                width = row.get('texture_width', 512)
                height = len(source)//(width*4)
                ow, oh = ctypes.c_int(), ctypes.c_int()
                source_buffer = ctypes.create_string_buffer(source)
                pointer = lib.MenuText_Render(source_buffer, width, height, 1, scale,
                                              ctypes.byref(ow), ctypes.byref(oh))
                if not pointer:
                    raise ValueError(f'{label_id}: renderer did not replace the original label')
                try:
                    changed = Image.frombytes('RGBA', (ow.value, oh.value),
                                              ctypes.string_at(pointer, ow.value*oh.value*4))
                finally:
                    libc.free(pointer)
                original = Image.frombytes('RGBA', (width, height), source).resize(
                    (width*scale, height*scale), Image.Resampling.NEAREST)
                x, y = row.get('x', 0)*scale, row['y']*scale
                box = (x, y, x+row.get('width', width)*scale, y+row['height']*scale)
                panels = []
                for item in (original.crop(box), changed.crop(box)):
                    alpha = item.getchannel('A').point(lambda value: min(255, value*2))
                    bounds = alpha.getbbox()
                    if bounds is None:
                        raise ValueError(f'{label_id}: comparison image is empty')
                    item.putalpha(alpha)
                    item = item.crop(bounds)
                    panel = Image.new('RGB', (item.width+24, item.height+24), '#518bd4')
                    panel.paste(item, (12, 12), item)
                    panels.append(panel)
                rendered_rows.append((style, row['original'], panels))
        finally:
            for key, value in prior.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

    annotation = ImageFont.truetype(str(fonts_dir/path), 28)
    title_font = ImageFont.truetype(str(fonts_dir/path), 36)
    column = max(panel.width for _, _, pair in rendered_rows for panel in pair)+32
    header = 128
    heights = [max(panel.height for panel in pair)+66 for _, _, pair in rendered_rows]
    comparison = Image.new('RGB', (column*2+48, header+sum(heights)+24), '#243c58')
    draw = ImageDraw.Draw(comparison)
    draw.text((24, 12), 'Menu text styles', font=title_font, fill='white')
    battle_font = battle[2] if battle_path is not None and battle_path.is_file() else 'Original (font missing)'
    draw.text((24, 62), f'Menu: {name}  |  Battle: {battle_font}', font=annotation, fill='#ffe8d0')
    draw.text((24, 98), 'Original game art', font=annotation, fill='white')
    draw.text((column+24, 98), 'Rendered replacement text', font=annotation, fill='white')
    top = header
    for (style, label, pair), row_height in zip(rendered_rows, heights):
        draw.text((24, top+4), f'{style}  —  {label}', font=annotation, fill='white')
        for index, panel in enumerate(pair):
            comparison.paste(panel, (24+column*index, top+44))
        top += row_height
    output = output or ROOT/'port/build/menu-font/style-comparison.png'
    output.parent.mkdir(parents=True, exist_ok=True)
    comparison.save(output)
    print(f'Style comparison: {output}\nMenu font: {name} ({path})\nBattle font: {battle_font}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--out', type=pathlib.Path, default=ROOT / 'gamedata/game-text', help='the game-text folder (labels.json is read there)')
    parser.add_argument('--fonts', type=pathlib.Path, default=None,
                        help='the fonts folder, to say which characters no font has (default: port/third_party/fonts, or fonts/ beside gamedata)')
    parser.add_argument('--apply', action='store_true', help='(what this tool does; kept so that the earlier command line works)')
    parser.add_argument('--compare-styles', action='store_true', help='save original-versus-rendered examples of every Menu font style')
    parser.add_argument('--font', help='Menu font path or display name for the comparison (default: first Menu font)')
    parser.add_argument('--output', type=pathlib.Path, help='PNG path for the comparison')
    parser.add_argument('--data', type=pathlib.Path, default=ROOT/'gamedata', help='extracted disc data for the comparison')
    args = parser.parse_args()
    if not (args.out / 'labels.json').is_file():
        raise SystemExit(f'No menu labels: {args.out}/labels.json is not there. Start the game once: it makes the list from the disc data '
                         '(USA release) and writes labels.json beside it.')
    fonts = args.fonts
    if fonts is None:
        beside = args.out.resolve().parent.parent / 'fonts'
        fonts = beside if (beside / 'fonts.tsv').is_file() else ROOT / 'port/third_party/fonts'
    apply_labels(args.out, fonts)
    if args.compare_styles:
        compare_styles(args.data, args.out, fonts, args.font, args.output)


if __name__ == '__main__':
    try:
        main()
    except ImportError as exc:
        raise SystemExit(f'{exc}. --compare-styles needs Pillow in the Python environment used for this tool.')
    except (ValueError, OSError, KeyError, IndexError, TypeError) as exc:
        raise SystemExit(f'No menu labels: {type(exc).__name__}: {exc}')
