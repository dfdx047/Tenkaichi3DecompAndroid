"""The Mod Manager's pictures at full resolution, drawn by the renderer like Tenkaichi Budokai Online's: tbo/ui/mm_*.png
next to the program (include/menu/tbo_art.h lists them after TBO's) and include/menu/mm_sizes.h (their sizes and the
corners that keep their shape).

The art comes from Google Flow (modmgr_design/art_src), on a flat green screen: the green is keyed out and the pieces
of a sheet are told apart by the green between them (scipy's labels).

  bg_b.jpg      the background (16:9, as it is)
  bars_a.jpg    bars, tabs, windows, the button bar, the path bar, the key badge, the gold card
  bars_b.jpg    the teal window
  icons_b.jpg   the emblem, the platform, folders, keys, the face cell, arrows, the ball, the lock, the stock pips

    mm_art.py --list    a preview with every piece's number (modmgr_design/art_src/*_numbered.png)
    mm_art.py           writes the pictures and the header
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')).replace('\\', '/')  # the repository


SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'art_src')
OUTS = [ROOT + '/tbo/ui', ROOT + '/tbo/ui']
HDR = ROOT + '/include/menu/mm_sizes.h'



def key_green(im):
    """The flat green screen of the Flow pictures keyed out (its spill taken off the edges)."""
    a = np.array(im.convert('RGB')).astype(np.float64)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    spill = g - np.maximum(r, b)
    alpha = np.clip(1.0 - (spill - 40.0) / 80.0, 0.0, 1.0)
    alpha[(g > 140) & (spill > 90)] = 0.0
    g2 = np.minimum(g, np.maximum(r, b) + 20.0)
    return Image.fromarray(np.dstack([r, g2, b, alpha * 255.0]).clip(0, 255).astype(np.uint8), 'RGBA')


def pieces(name):
    """The pieces of a sheet, in reading order (rows by their middle, then left to right): (box, keyed picture)."""
    im = key_green(Image.open(os.path.join(SRC, name)))
    a = np.array(im.getchannel('A')) > 24
    lab, n = ndimage.label(ndimage.binary_dilation(a, iterations=3))
    boxes = []
    for sl in ndimage.find_objects(lab):
        y0, y1, x0, x1 = sl[0].start, sl[0].stop, sl[1].start, sl[1].stop
        if (x1 - x0) * (y1 - y0) < 400:
            continue
        boxes.append((x0, y0, x1, y1))
    boxes.sort(key=lambda b: (round(((b[1] + b[3]) / 2) / 40), b[0]))
    return im, boxes


def numbered(name):
    im, boxes = pieces(name)
    pic = Image.new('RGBA', im.size, (40, 40, 60, 255))
    pic.alpha_composite(im)
    d = ImageDraw.Draw(pic)
    for i, b in enumerate(boxes):
        d.rectangle(b, outline=(255, 0, 0, 255), width=2)
        d.text((b[0] + 3, b[1] + 2), str(i), fill=(255, 255, 0, 255))
    pic.convert('RGB').save(os.path.join(SRC, name.split('.')[0] + '_numbered.png'))
    return boxes


if __name__ == '__main__' and '--list' in sys.argv:
    for s in ('bars_a.jpg', 'bars_b.jpg', 'icons_b.jpg'):
        for i, b in enumerate(numbered(s)):
            print(s, i, b, (b[2] - b[0], b[3] - b[1]))

# (sheet, piece number, name, the corner that keeps its shape, in the picture's pixels: a bar's round end, a window's
# corner; 0 for a picture drawn whole)
ART = [
    ('bg_sky.png', 'sky', 'mm_bg', 0),
    ('bars_a.jpg', 0, 'mm_bar', 54),
    ('bars_a.jpg', 3, 'mm_bar_sel', 54),
    ('bars_a.jpg', 6, 'mm_tab', 46),
    ('bars_a.jpg', 7, 'mm_tab_sel', 46),
    ('bars_a.jpg', 5, 'mm_bar_grey', 41),
    ('bars_a.jpg', 1, 'mm_navbar', 44),
    ('bars_a.jpg', 2, 'mm_field', 44),
    ('bars_a.jpg', 4, 'mm_key', 30),
    ('bars_a.jpg', 9, 'mm_window_dark', 44),
    ('bars_a.jpg', 8, 'mm_card', 56),
    ('bars_b.jpg', 5, 'mm_window', 48),
    ('icons_b.jpg', 3, 'mm_emblem', 0),
    ('icons_b.jpg', 4, 'mm_platform', 0),
    ('icons_b.jpg', 0, 'mm_folder', 0),
    ('icons_b.jpg', 1, 'mm_folder_mod', 0),
    ('icons_b.jpg', 2, 'mm_folder_batch', 0),
    ('icons_b.jpg', 5, 'mm_zip', 0),
    ('icons_b.jpg', 6, 'mm_keycap', 34),
    ('icons_b.jpg', 7, 'mm_keycap_sel', 34),
    ('icons_b.jpg', 9, 'mm_cell', 30),
    ('icons_b.jpg', 11, 'mm_up', 0),
    ('icons_b.jpg', 12, 'mm_down', 0),
    ('icons_b.jpg', 13, 'mm_left', 0),
    ('icons_b.jpg', 14, 'mm_right', 0),
    ('icons_b.jpg', 16, 'mm_ball', 0),
    ('icons_b.jpg', 17, 'mm_lock', 0),
    ('icons_b.jpg', 18, 'mm_pip', 0),
    ('icons_b.jpg', 19, 'mm_pip_off', 0),
    ('bg_b.jpg', 'dusk', 'mm_bg_dusk', 0),
]


def background(name, how):
    """The backdrop, a little out of focus (the screens' windows read over it, as in the mockups); 'dusk': the same
    place at dusk, for the character select's mod grid (mockup 16's purple and pink sky)."""
    from PIL import ImageFilter, ImageEnhance
    pic = Image.open(os.path.join(SRC, name)).convert('RGB')
    if how == 'sky':  # Ahmed's sky (square): covers the 16:9 page, from its middle, a little brighter
        w, h = pic.size
        k = max(1376 / w, 768 / h)
        pic = pic.resize((round(w * k), round(h * k)), Image.LANCZOS)
        x = (pic.width - 1376) // 2
        y = (pic.height - 768) // 2
        return pic.crop((x, y, x + 1376, y + 768)).convert('RGBA')
    pic = pic.filter(ImageFilter.GaussianBlur(1.4))
    if how == 'dusk':
        a = np.asarray(pic).astype(np.float32) / 255.0
        lum = a @ np.array([0.3, 0.55, 0.15], np.float32)
        h = a.shape[0]
        t = np.linspace(0.0, 1.0, h, dtype=np.float32)[:, None]
        top = np.array([0.36, 0.20, 0.62], np.float32)     # violet above,
        mid = np.array([0.98, 0.58, 0.74], np.float32)     # pink at the horizon,
        low = np.array([0.52, 0.36, 0.66], np.float32)     # lilac on the ground
        sky = np.where(t[..., None] < 0.55, top + (mid - top) * (t[..., None] / 0.55), mid + (low - mid) * ((t[..., None] - 0.55) / 0.45))
        out = sky * (0.35 + 0.95 * lum[..., None])
        out = np.clip(out * 0.92 + a * 0.12, 0.0, 1.0)
        pic = Image.fromarray((out * 255).astype(np.uint8))
    else:
        pic = ImageEnhance.Brightness(pic).enhance(1.03)
    return pic.convert('RGBA')


def main():
    cache = {}
    lines = ['/* Generated by port/build/ml/mm_art.py: the Mod Manager\'s pictures (tbo/ui/mm_*.png, listed in tbo_art.h after',
             '   Tenkaichi Budokai Online\'s): width, height, the corner that keeps its shape (pixels of the picture). */',
             '#ifndef MM_SIZES_H', '#define MM_SIZES_H', 'static const u16 sMmArtSize[][3] = {']
    for sheet, num, name, corner in ART:
        if num is None or num in ('dusk', 'sky'):
            pic = background(sheet, num)
        else:
            if sheet not in cache:
                cache[sheet] = pieces(sheet)
            im, boxes = cache[sheet]
            x0, y0, x1, y1 = boxes[num]
            pic = im.crop((x0, y0, x1, y1))
            bb = pic.getchannel('A').point(lambda v: 255 if v > 12 else 0).getbbox()
            pic = pic.crop(bb)
        for out in OUTS:
            os.makedirs(out, exist_ok=True)
            pic.save(os.path.join(out, name + '.png'))
        lines.append('    {%d, %d, %d}, /* %s */' % (pic.width, pic.height, corner, name))
        print(name, pic.size)
    lines += ['};', '#endif', '']
    open(HDR, 'w', newline='\n').write('\n'.join(lines))
    print('wrote', HDR)


if __name__ == '__main__' and '--list' not in sys.argv:
    main()
