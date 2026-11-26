"""Draws a main-menu plate label in the game's plate lettering (used by modmgr_label2.py for "Mod Manager").

The plate labels have no letter k and no free row, so a label is drawn new - Tahoma Bold, which is what the game's
plates are lettered in, sized and coloured from "Dragon History" of the same sheet - as palette indices of that sheet
(build(); TEXT is the words). The PC build writes it into a row of a third label sheet (src/menu/menu_a.c).
"""
import pathlib
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import bpe
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')).replace('\\', '/')  # the repository



class _Ml:   # modmgr_label.py's helpers (the file runs its main when imported, so only what stands above that)
    pass


_src = (HERE / 'modmgr_label.py').read_text(encoding='utf-8')
_ns = {'__file__': str(HERE / 'modmgr_label.py')}
exec(compile(_src[:_src.index('def main():')], 'modmgr_label.py', 'exec'), _ns)
ml = _Ml()
ml.load, ml.letters, ml.runs = _ns['load'], _ns['letters'], _ns['runs']

ROW_H = 32
TEXT = 'Mod Manager'


def build(pack, lst):
    texs = bpe.textures(pack, lst)
    src, spal = ml.load(texs[0])
    dst, dpal = ml.load(texs[1])
    rgba = spal[src]
    on = lst == 6
    segs, fill = ml.letters(rgba, 0, on)                 # "Dragon History"
    row = rgba[0:ROW_H].astype(np.int32)
    ys = np.nonzero(fill[:, segs[0][0]:segs[0][1] + 1].any(1))[0]   # the D: a capital's height
    top, bot = int(ys.min()), int(ys.max())
    solid = row[..., 3] >= 96
    line = solid & ~fill
    ftop = np.median(row[top:top + 3][fill[top:top + 3]], axis=0)
    fbot = np.median(row[bot - 2:bot + 1][fill[bot - 2:bot + 1]], axis=0)
    lcol = np.median(row[line], axis=0)
    x0 = segs[0][0]
    print(lst, 'capital', top, bot, 'starts at', x0, 'fill', ftop[:3], fbot[:3], 'line', lcol[:3])
    # the word, four times as large, then shrunk
    S = 4
    cap = (bot - top + 1) * S
    size = 10
    while True:
        font = ImageFont.truetype('C:/Windows/Fonts/tahomabd.ttf', size)
        box = font.getbbox('S')
        if box[3] - box[1] >= cap:
            break
        size += 1
    mask = Image.new('L', (512 * S, ROW_H * S), 0)
    d = ImageDraw.Draw(mask)
    base = font.getbbox('S')
    d.text((x0 * S, top * S - base[1]), TEXT, font=font, fill=255)
    bb = mask.getbbox()
    room = 244 * S - x0 * S
    if bb[2] - bb[0] > room:   # (the runs reach column 255)
        word = mask.crop(bb).resize((room, bb[3] - bb[1]), Image.LANCZOS)
        mask = Image.new('L', mask.size, 0)
        mask.paste(word, (x0 * S, bb[1]))
    fat = mask.filter(ImageFilter.MaxFilter(4 * S + 1))
    big = np.zeros((ROW_H * S, 512 * S, 4), dtype=np.float64)
    m = np.array(mask, dtype=np.float64) / 255.0
    f = np.array(fat, dtype=np.float64) / 255.0
    t = np.clip((np.arange(ROW_H * S) - top * S) / max(cap, 1), 0, 1)[:, None, None]
    fillc = ftop[None, None, :3] * (1 - t) + fbot[None, None, :3] * t
    big[..., :3] = lcol[None, None, :3] * (1 - m[..., None]) + fillc * m[..., None]
    big[..., 3] = f * 128.0
    small = big.reshape(ROW_H, S, 512, S, 4).mean(axis=(1, 3))
    out = np.clip(small, 0, 255).astype(np.uint8)
    dp = dpal.astype(np.int32)
    idx = np.zeros((ROW_H, 512), dtype=np.uint8)
    clear = int(np.argmin(dp[:, 3]))
    for y in range(ROW_H):
        for x in range(256):
            c = out[y, x].astype(np.int32)
            if c[3] < 10:
                idx[y, x] = clear
            else:
                dd = ((dp[:, :3] - c[:3]) ** 2).sum(1) + ((dp[:, 3] - c[3]) ** 2) * 3
                idx[y, x] = int(np.argmin(dd))
        idx[y, 256:] = clear
    return idx, dpal, rgba
