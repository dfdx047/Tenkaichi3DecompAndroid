"""The pictures of the Mod Manager screen (src/menu/port_modmgr.c): gamedata/modmgr/ui.bin, a texture file the game
loads like its own (count, entries of 0x40 bytes, upload packets; every texture 8 bits with its own CLUT).

  0  the backdrop, 512 x 512 (448 lines used): drawn here - a deep blue sky with slanted streaks and a glow where
     the model stands - with the two dragon balls of the mockup in its corners
  1  the title bar with "MOD MANAGER", 512 x 128: cut from the mockup (gamedata/modmgr/src/mockup.webp)

    modmgr_assets.py            writes the file and modmgr_assets.png (what it holds)
"""
import math, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))

import gsmem
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')).replace('\\', '/')  # the repository


OUT = ROOT + '/gamedata/modmgr'
MOCK = OUT + '/src/mockup.webp'
SX, SY = 512 / 1448.0, 448 / 1086.0


def backdrop(mock):
    w, h = 512, 448
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    top = np.array([10, 30, 92.0]); bot = np.array([3, 8, 30.0])
    t = (y / h)[..., None]
    img = top * (1 - t) + bot * t
    # the light where the model stands, and a column of light above it
    cx, cy = 388.0, 180.0
    d = np.sqrt(((x - cx) / 150.0) ** 2 + ((y - cy) / 190.0) ** 2)
    img += np.clip(1 - d, 0, 1)[..., None] ** 1.6 * np.array([20, 70, 150.0])
    col = np.clip(1 - np.abs(x - cx) / 46.0, 0, 1) * np.clip((262 - y) / 262.0, 0, 1) * (y < 262)
    img += col[..., None] * np.array([10, 40, 90.0])
    # slanted streaks
    rng = np.random.RandomState(7)
    for _ in range(26):
        x0 = rng.uniform(-100, 620); wd = rng.uniform(1.0, 5.0); amp = rng.uniform(6, 22)
        y0 = rng.uniform(0, 448); ln = rng.uniform(80, 260)
        along = (y - y0) / ln
        across = (x - (x0 - (y - y0) * 0.55)) / wd
        s = np.exp(-across ** 2) * np.clip(1 - np.abs(along * 2 - 1), 0, 1) * ((along > 0) & (along < 1))
        img += s[..., None] * np.array([0.25, 0.55, 1.0]) * amp
    im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).convert('RGBA')
    # the platform: rings of light on the floor
    d = ImageDraw.Draw(im, 'RGBA')
    for k, (rx, ry, c) in enumerate(((104, 17, (40, 110, 255, 70)), (86, 13, (90, 170, 255, 90)), (66, 10, (255, 170, 40, 150)),
                                     (60, 8, (20, 50, 140, 255)), (40, 5.5, (80, 160, 255, 120)), (20, 3, (200, 230, 255, 150)))):
        d.ellipse((cx - rx, 258 - ry, cx + rx, 258 + ry), fill=c)
    im = im.filter(ImageFilter.GaussianBlur(0.8))
    # a dragon ball in two corners, drawn here: an orange sphere with its stars, behind everything
    def ball(cx_, cy_, r_, stars):
        s4 = 4
        layer = Image.new('RGBA', (r_ * 2 * s4, r_ * 2 * s4), (0, 0, 0, 0))
        yy, xx = np.mgrid[0:r_ * 2 * s4, 0:r_ * 2 * s4].astype(np.float64)
        dx_, dy_ = (xx - r_ * s4) / (r_ * s4), (yy - r_ * s4) / (r_ * s4)
        rr = np.sqrt(dx_ ** 2 + dy_ ** 2)
        lit = np.clip(1.15 - np.sqrt((dx_ + 0.35) ** 2 + (dy_ + 0.4) ** 2), 0, 1)
        col = np.zeros(xx.shape + (4,))
        col[..., 0] = 150 + 105 * lit; col[..., 1] = 60 + 110 * lit; col[..., 2] = 8 + 40 * lit ** 3
        col[..., 3] = np.clip((1 - rr) * r_ * s4 / 3.0, 0, 1) * 215
        layer = Image.fromarray(col.astype(np.uint8), 'RGBA')
        d2 = ImageDraw.Draw(layer)
        for sx_, sy_ in stars:
            pts = []
            for k in range(10):
                ang = -math.pi / 2 + k * math.pi / 5
                rad = (0.17 if k % 2 == 0 else 0.07) * r_ * s4
                pts.append((r_ * s4 * (1 + sx_) + rad * math.cos(ang), r_ * s4 * (1 + sy_) + rad * math.sin(ang)))
            d2.polygon(pts, fill=(200, 30, 20, 230))
        layer = layer.resize((r_ * 2, r_ * 2), Image.LANCZOS)
        im.alpha_composite(layer, (cx_ - r_, cy_ - r_))
    im = im.convert('RGBA')
    canvas = Image.new('RGBA', (512 + 200, 448 + 200), (0, 0, 0, 0))
    big = Image.new('RGBA', canvas.size, (0, 0, 0, 0)); big.paste(im, (100, 100))
    im = big
    ball(100 + 500, 100 + 96, 52, [(-0.2, -0.22), (0.22, -0.2), (-0.22, 0.2), (0.2, 0.22)])
    ball(100 + 4, 100 + 440, 46, [(0.2, -0.3), (0.3, 0.1)])
    im = im.crop((100, 100, 612, 548))
    full = Image.new('RGB', (512, 512), (3, 8, 30))
    full.paste(im.convert('RGB'), (0, 0))
    return full


def title(mock):
    piece = mock.crop((0, 0, 800, 152))
    w, h = int(round(800 * SX)), int(round(152 * SY))
    piece = piece.resize((w, h), Image.LANCZOS).convert('RGBA')
    # the bar's own shape: its slanted right end and its lower edge
    mask = Image.new('L', (w * 4, h * 4), 0)
    ImageDraw.Draw(mask).polygon([(0, 0), (int(778 * SX * 4), 0), (int(778 * SX * 4), int(10 * SY * 4)),
                                  (int(694 * SX * 4), int(140 * SY * 4)), (0, int(146 * SY * 4))], fill=255)
    mask = mask.resize((w, h), Image.LANCZOS)
    piece.putalpha(mask)
    full = Image.new('RGBA', (512, 128), (0, 0, 0, 0))
    full.paste(piece, (0, 0))
    return full, (w, h)


def to_indexed(im, alpha_levels):
    """-> indices, palette (256, 4), alpha in the PS2's scale"""
    a = np.array(im.convert('RGBA'))
    if alpha_levels == 1:
        q = im.convert('RGB').quantize(256, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG)
        p = np.array(q.getpalette()[:768], dtype=np.uint8).reshape(256, 3)
        pal = np.zeros((256, 4), dtype=np.uint8)
        pal[:, :3] = p
        pal[:, 3] = 0x80
        return np.array(q, dtype=np.uint8), pal
    colours = 255 // alpha_levels
    q = Image.fromarray(a[..., :3]).quantize(colours, method=Image.MEDIANCUT, dither=Image.NONE)
    ci = np.array(q, dtype=np.int32)
    p = np.array(q.getpalette()[:colours * 3], dtype=np.uint8).reshape(-1, 3)
    p = np.vstack([p, np.zeros((colours - len(p), 3), dtype=np.uint8)])
    al = np.round(a[..., 3].astype(np.float64) / 255.0 * alpha_levels).astype(np.int32)
    idx = np.where(al == 0, 0, 1 + ci * alpha_levels + (al - 1))
    pal = np.zeros((256, 4), dtype=np.uint8)
    for c in range(colours):
        for l in range(alpha_levels):
            pal[1 + c * alpha_levels + l, :3] = p[c]
            pal[1 + c * alpha_levels + l, 3] = round((l + 1) / alpha_levels * 0x80)
    return idx.astype(np.uint8), pal


def entry(idx, pal):
    h, w = idx.shape
    data, upw, uph, dbw, tbw = gsmem.encode(idx, 8)
    pix = gsmem.image_packet(data, upw, uph)
    cdata = gsmem.csm1(pal).astype(np.uint8).tobytes()
    clut = gsmem.image_packet(cdata, 16, 16)
    tw, th = w.bit_length() - 1, h.bit_length() - 1
    tex0 = tbw << 14 | 0x13 << 20 | tw << 26 | th << 30 | 1 << 61
    return {'pix': pix, 'clut': clut, 'tstep': w * h // 256, 'cstep': 4, 'pblt': dbw, 'cblt': 0, 'tex0': tex0}


def write(ents, path):
    out = bytearray(struct.pack('<4I', len(ents), 8, 0, 0) + bytes(16))
    data = bytearray()
    base = 0x20 + 0x40 * len(ents)
    for e in ents:
        po = base + len(data); data += e['pix']
        co = base + len(data); data += e['clut']
        # every texture is uploaded on its own at the menus' block, its CLUT right behind its pixels
        out += struct.pack('<10I', po // 4, co // 4, len(e['pix']), len(e['clut']), e['tstep'], e['cstep'], e['pblt'], e['cblt'], 0, e['tstep'])
        out += bytes(8) + struct.pack('<Q', e['tex0']) + bytes(8)
    out += data
    open(path, 'wb').write(bytes(out))
    return len(out)


if __name__ == '__main__':
    mock = Image.open(MOCK).convert('RGB')
    bg = backdrop(mock)
    ti, tsize = title(mock)
    ents = [entry(*to_indexed(bg, 1)), entry(*to_indexed(ti, 5))]
    print('wrote', write(ents, OUT + '/ui.bin'), 'bytes; title piece', tsize)
    # read back
    d = open(OUT + '/ui.bin', 'rb').read()
    sheet = Image.new('RGBA', (512, 448 + 70), (60, 20, 60, 255))
    for i in range(2):
        e = d[0x20 + i * 0x40:0x60 + i * 0x40]
        f = struct.unpack_from('<10I', e, 0); tex0 = struct.unpack_from('<Q', e, 0x30)[0]
        pix = d[f[0] * 4:f[0] * 4 + f[2]]; clut = d[f[1] * 4:f[1] * 4 + f[3]]
        w = 1 << ((tex0 >> 26) & 15); h = 1 << ((tex0 >> 30) & 15)
        idx, psm = gsmem.decode(pix[0x60:], clut, tex0, w // 2, h // 2, f[6])
        pal = gsmem.csm1(np.frombuffer(clut[0x60:0x60 + 1024], dtype=np.uint8).reshape(256, 4))
        rgba = pal[idx].copy(); rgba[..., 3] = np.minimum(rgba[..., 3].astype(np.int32) * 2, 255)
        im = Image.fromarray(rgba, 'RGBA')
        sheet.alpha_composite(im.crop((0, 0, 512, 448 if i == 0 else 64)), (0, 0 if i == 0 else 448 + 4))
    sheet.convert('RGB').save(os.path.join(HERE, '..', 'modmgr_assets.png'))
