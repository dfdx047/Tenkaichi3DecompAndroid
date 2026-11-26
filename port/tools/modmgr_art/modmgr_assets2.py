"""The pictures of the Mod Manager in the game's own menu style (second look, after the user's mockup
gamedata/modmgr/src/mockup2.webp): gamedata/modmgr/ui.bin and include/menu/port_modart.h.

  0  the backdrop, 512 x 512 (448 lines used): a bright sky over Capsule Corporation's domes, cloud curls in the corners
  1  the title, 512 x 128: the four-star ball and "Mod Manager" (shown 219 x 64 on screen)
  2  the letters, 512 x 512: Tahoma Bold, ASCII 32..126 - the letters themselves in the upper half, their outlines
     (with a soft shadow) in the lower half, both white with the shape in the alpha: the colours are the drawer's
  3  the pieces, 512 x 512: bars, panels, buttons, arrows, the platform (see PIECES)

Everything is drawn at twice the screen's 512 x 448, and the screen's pixels are 7:6 wide, so a piece that is to
look w x h pixels of a square grid is made (w * 6 / 7 * 2) x (h * 2).

    modmgr_assets2.py           writes both files and modmgr_assets2.png (what the file holds)
"""
import math, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gsmem
from modmgr_assets import entry, write
import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')).replace('\\', '/')  # the repository


OUT = ROOT + '/gamedata/modmgr'
HDR = ROOT + '/include/menu/port_modart.h'
BOLD = 'C:/Windows/Fonts/tahomabd.ttf'
PAR = 6.0 / 7.0   # a square thing is this much narrower in the screen's pixels
SS = 4            # drawn this much larger, then shrunk


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def vgrad(w, h, stops):
    """stops: [(t, (r, g, b, a))...] top to bottom"""
    col = np.zeros((h, 4))
    for y in range(h):
        t = y / max(h - 1, 1)
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                k = (t - t0) / max(t1 - t0, 1e-9)
                col[y] = [c0[i] + (c1[i] - c0[i]) * k for i in range(4)]
                break
    return Image.fromarray(np.repeat(col[:, None, :], w, axis=1).astype(np.uint8), 'RGBA')


def masked(img, mask):
    out = img.copy()
    a = ImageChops.multiply(out.getchannel('A'), mask)
    out.putalpha(a)
    return out


# ------------------------------------------------------------------------------------------------------ the backdrop
def cloud_curl(d, cx, cy, s, flip=1):
    """A cream cloud of the game's corners: lobes with a darker rim and a curl line."""
    cream, rim = (250, 240, 205, 255), (214, 190, 130, 255)
    lobes = [(0, 0, 60), (70 * flip, 12, 50), (-64 * flip, 18, 44), (130 * flip, 30, 40), (30 * flip, 44, 46), (-20 * flip, -30, 40), (96 * flip, -18, 36)]
    for dx, dy, r in lobes:
        d.ellipse([cx + (dx - r) * s - 3, cy + (dy - r) * s - 3, cx + (dx + r) * s + 3, cy + (dy + r) * s + 3], fill=rim)
    for dx, dy, r in lobes:
        d.ellipse([cx + (dx - r) * s, cy + (dy - r) * s, cx + (dx + r) * s, cy + (dy + r) * s], fill=cream)
    for dx, dy, r in ((0, 4, 30), (70 * flip, 16, 24), (-60 * flip, 20, 20)):
        d.arc([cx + (dx - r) * s, cy + (dy - r) * s, cx + (dx + r) * s, cy + (dy + r) * s], 200 if flip > 0 else 250, 430 if flip > 0 else 480, fill=rim, width=max(2, int(3 * s)))


def dome(d, cx, base, rx, ry, label=None):
    body, shade, line = (240, 232, 200, 255), (214, 200, 160, 255), (150, 140, 110, 255)
    d.pieslice([cx - rx, base - ry, cx + rx, base + ry], 180, 360, fill=body, outline=line, width=3)
    d.pieslice([cx - rx * 0.96, base - ry * 0.2, cx + rx * 0.96, base + ry * 0.9], 180, 360, fill=shade)
    d.rectangle([cx - rx, base - 2, cx + rx, base + 3], fill=line)
    # a row of round windows and a door
    for k in (-0.55, -0.2, 0.2, 0.55):
        d.ellipse([cx + k * rx - rx * 0.07, base - ry * 0.42, cx + k * rx + rx * 0.07, base - ry * 0.26], fill=(120, 190, 235, 255), outline=line)
    d.rectangle([cx - rx * 0.12, base - ry * 0.2, cx + rx * 0.12, base], fill=(170, 200, 225, 255), outline=line, width=2)
    d.arc([cx - rx * 0.8, base - ry * 0.92, cx + rx * 0.2, base + ry * 0.2], 200, 260, fill=(255, 252, 235, 255), width=5)


def backdrop():
    W, H = 512 * 2, 448 * 2
    im = vgrad(W, H, [(0, (40, 150, 240, 255)), (0.45, (110, 196, 250, 255)), (0.70, (190, 232, 255, 255)), (0.70, (214, 230, 240, 255)), (1, (160, 190, 220, 255))])
    d = ImageDraw.Draw(im)
    hor = int(H * 0.70)
    # far soft clouds
    soft = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    ds = ImageDraw.Draw(soft)
    rng = np.random.RandomState(3)
    for _ in range(26):
        x, y, r = rng.uniform(0, W), rng.uniform(H * 0.28, hor - 30), rng.uniform(30, 90)
        ds.ellipse([x - r * 1.8, y - r * 0.5, x + r * 1.8, y + r * 0.5], fill=(255, 255, 255, 120))
    im.alpha_composite(soft.filter(ImageFilter.GaussianBlur(14)))
    # the domes, hazy with distance
    far = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    df = ImageDraw.Draw(far)
    dome(df, 150, hor + 6, 190, 150)
    dome(df, 470, hor + 6, 120, 96)
    dome(df, 900, hor + 6, 230, 190)
    dome(df, 690, hor + 6, 90, 70)
    far = far.filter(ImageFilter.GaussianBlur(1.6))
    haze = Image.new('RGBA', (W, H), (150, 210, 250, 70))
    far2 = far.copy(); far2.alpha_composite(masked(haze, far.getchannel('A')))
    im.alpha_composite(far2)
    # the paved ground: lines running to the horizon and across
    d = ImageDraw.Draw(im)
    d.rectangle([0, hor + 6, W, H], fill=(205, 222, 236, 255))
    g = vgrad(W, H - hor - 6, [(0, (222, 236, 246, 255)), (1, (150, 182, 214, 255))])
    im.paste(g, (0, hor + 6))
    d = ImageDraw.Draw(im)
    for k in range(-14, 15):
        d.line([(W / 2 + k * 60, hor + 6), (W / 2 + k * 260, H)], fill=(170, 196, 220, 255), width=2)
    y = hor + 6.0; step = 10.0
    while y < H:
        d.line([(0, y), (W, y)], fill=(170, 196, 220, 255), width=2)
        y += step; step *= 1.45
    # the light where the model stands
    glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([W * 0.75 - 250, H * 0.42 - 330, W * 0.75 + 250, H * 0.42 + 330], fill=(255, 255, 255, 70))
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(70)))
    # the corners' clouds
    d = ImageDraw.Draw(im)
    cloud_curl(d, 40, 60, 1.25, 1)
    cloud_curl(d, W - 60, 28, 1.25, -1)
    im = im.filter(ImageFilter.GaussianBlur(0.8)).resize((512, 448), Image.LANCZOS)
    full = Image.new('RGB', (512, 512), (160, 190, 220))
    full.paste(im.convert('RGB'), (0, 0))
    return full


# --------------------------------------------------------------------------------------------------- the four-star ball
def ball(size):
    s = size * SS
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float64)
    dx, dy = (xx - s / 2) / (s / 2), (yy - s / 2) / (s / 2)
    rr = np.sqrt(dx ** 2 + dy ** 2)
    lit = np.clip(1.2 - np.sqrt((dx + 0.35) ** 2 + (dy + 0.42) ** 2), 0, 1)
    col = np.zeros((s, s, 4))
    col[..., 0] = 236 + 19 * lit; col[..., 1] = 110 + 120 * lit; col[..., 2] = 10 + 90 * lit ** 3
    edge = np.clip((rr - 0.80) / 0.12, 0, 1)
    col[..., :3] = col[..., :3] * (1 - edge[..., None]) + np.array([150, 60, 10.0]) * edge[..., None]
    rim = (rr > 0.88)
    col[rim, 0], col[rim, 1], col[rim, 2] = 70, 40, 60
    col[..., 3] = np.clip((1 - rr) * s / 2 / 1.5, 0, 1) * 255
    im = Image.fromarray(col.astype(np.uint8), 'RGBA')
    d = ImageDraw.Draw(im)
    for sx, sy in ((-0.2, -0.2), (0.22, -0.18), (-0.2, 0.22), (0.2, 0.22)):
        pts = []
        for k in range(10):
            ang = -math.pi / 2 + k * math.pi / 5
            rad = (0.19 if k % 2 == 0 else 0.08) * s / 2
            pts.append((s / 2 * (1 + sx) + rad * math.cos(ang), s / 2 * (1 + sy) + rad * math.sin(ang)))
        d.polygon(pts, fill=(214, 40, 24, 255))
    hi = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(hi).ellipse([s * 0.2, s * 0.12, s * 0.5, s * 0.3], fill=(255, 255, 255, 150))
    im.alpha_composite(masked(hi.filter(ImageFilter.GaussianBlur(s * 0.03)), im.getchannel('A')))
    return im.resize((size, size), Image.LANCZOS)


# ------------------------------------------------------------------------------------------------------------ the title
def title():
    W, H = 512, 128
    big = Image.new('RGBA', (W * 2, H * 2), (0, 0, 0, 0))
    font = ImageFont.truetype(BOLD, 132)
    text = 'Mod Manager'
    wide = Image.new('L', (2400, big.size[1]), 0)
    ImageDraw.Draw(wide).text((40, 38), text, font=font, fill=255)
    box = wide.getbbox()
    # squeeze the word to the room it has
    room = min(W * 2 - 262, box[2] - box[0])
    word = wide.crop(box).resize((room, box[3] - box[1]), Image.LANCZOS)
    mask = Image.new('L', big.size, 0); mask.paste(word, (246, box[1]))
    box = mask.getbbox()
    fat = mask.filter(ImageFilter.MaxFilter(13))
    fatter = mask.filter(ImageFilter.MaxFilter(21)).filter(ImageFilter.GaussianBlur(7))
    glow = Image.new('RGBA', big.size, (255, 236, 220, 0)); glow.putalpha(fatter.point(lambda v: int(v * 0.85)))
    big.alpha_composite(glow)
    dark = Image.new('RGBA', big.size, (120, 16, 10, 255)); dark.putalpha(fat)
    big.alpha_composite(dark)
    grad = vgrad(big.size[0], big.size[1], [(0, (255, 236, 60, 255)), (box[1] / big.size[1], (255, 236, 60, 255)),
                                            ((box[1] + (box[3] - box[1]) * 0.5) / big.size[1], (255, 150, 20, 255)),
                                            (box[3] / big.size[1], (238, 50, 16, 255)), (1, (238, 50, 16, 255))])
    big.alpha_composite(masked(grad, mask))
    # a light line inside the top of the letters
    inner = ImageChops.subtract(mask, ImageChops.offset(mask, 0, 5))
    shine = Image.new('RGBA', big.size, (255, 255, 220, 0)); shine.putalpha(inner.point(lambda v: int(v * 0.55)))
    big.alpha_composite(masked(shine, mask))
    # the rule under it
    d = ImageDraw.Draw(big)
    d.rounded_rectangle([170, 232, W * 2 - 6, 240], radius=4, fill=(150, 150, 236, 230))
    d.rounded_rectangle([170, 232, W * 2 - 6, 235], radius=2, fill=(214, 214, 255, 230))
    # the ball in its ring
    ring = Image.new('RGBA', (232, 232), (0, 0, 0, 0))
    dr = ImageDraw.Draw(ring)
    dr.ellipse([0, 0, 231, 231], fill=(40, 60, 120, 255))
    dr.ellipse([8, 8, 223, 223], fill=(120, 170, 240, 255))
    dr.ellipse([18, 18, 213, 213], fill=(30, 40, 90, 255))
    ring.alpha_composite(ball(184), (24, 24))
    big.alpha_composite(ring, (4, 8))
    return big.resize((W, H), Image.LANCZOS)


# ---------------------------------------------------------------------------------------------------------- the letters
CELL = 42          # a row of the sheet
PAD = 5            # room around a letter for its outline


def letters():
    font = ImageFont.truetype(BOLD, 32)
    asc, desc = font.getmetrics()
    fill = Image.new('L', (512, 256), 0)
    line = Image.new('L', (512, 256), 0)
    table = []
    x, y = 0, 0
    top = 6   # the font's own blank lines above its capitals, cut off
    for code in range(32, 127):
        ch = chr(code)
        adv = font.getlength(ch)
        w = int(math.ceil(adv * PAR)) + PAD * 2
        if x + w > 512:
            x = 0; y += CELL
        g = Image.new('L', (int(math.ceil(adv)) + 40, asc + desc + 20), 0)
        ImageDraw.Draw(g).text((20, 10), ch, font=font, fill=255)
        # the outline: the letter grown, and a soft shadow below right
        grown = g.filter(ImageFilter.MaxFilter(5))
        shadow = ImageChops.offset(g.filter(ImageFilter.MaxFilter(3)), 1, 3).filter(ImageFilter.GaussianBlur(1.6)).point(lambda v: int(v * 0.6))
        o = ImageChops.lighter(grown, shadow).filter(ImageFilter.GaussianBlur(0.6))
        gw = int(round(g.size[0] * PAR))
        g2 = g.resize((gw, g.size[1]), Image.LANCZOS)
        o2 = o.resize((gw, g.size[1]), Image.LANCZOS)
        ox = PAD - int(round(20 * PAR))
        box = (-ox, 10 + top, -ox + w, 10 + top + CELL)
        fill.paste(g2.crop(box), (x, y))
        line.paste(o2.crop(box), (x, y))
        table.append((x, y, w, adv * PAR))
        x += w
    assert y + CELL <= 256, y
    sheet = Image.new('L', (512, 512), 0)
    sheet.paste(fill, (0, 0)); sheet.paste(line, (0, 256))
    return sheet, table


# ----------------------------------------------------------------------------------------------------------- the pieces
def canvas(w, h):
    """a piece that takes w x h of the screen: drawn square-true at SS times, shrunk to (2w, 2h) at the end"""
    return Image.new('RGBA', (int(round(w / PAR * 2 * SS)), h * 2 * SS), (0, 0, 0, 0))


def done(im, w, h):
    return im.resize((w * 2, h * 2), Image.LANCZOS)


def pill(w, h, top, bottom, rim, zig=None, glow=None):
    im = canvas(w, h)
    W, H = im.size
    m = int(H * 0.08)
    d = ImageDraw.Draw(im)
    if glow is not None:
        g = Image.new('RGBA', im.size, (0, 0, 0, 0))
        ImageDraw.Draw(g).rounded_rectangle([m * 0.3, m * 0.3, W - m * 0.3, H - m * 0.3], radius=H / 2, fill=glow)
        im.alpha_composite(g.filter(ImageFilter.GaussianBlur(m * 0.5)))
        d = ImageDraw.Draw(im)
    d.rounded_rectangle([m, m, W - m, H - m], radius=(H - 2 * m) / 2, fill=(40, 40, 70, 150))                  # dark outer line
    d.rounded_rectangle([m * 1.5, m * 1.5, W - m * 1.5, H - m * 1.5], radius=(H - 3 * m) / 2, fill=rim)        # the rim
    body = Image.new('L', im.size, 0)
    ImageDraw.Draw(body).rounded_rectangle([m * 2.6, m * 2.6, W - m * 2.6, H - m * 2.6], radius=(H - 5.2 * m) / 2, fill=255)
    g = vgrad(W, H, [(0, top), (1, bottom)])
    im.paste(g, (0, 0), body)
    if zig is not None:   # the faint zigzag of the game's bars
        z = Image.new('RGBA', im.size, (0, 0, 0, 0))
        dz = ImageDraw.Draw(z)
        step = H * 0.42
        x = -step
        while x < W + step:
            dz.polygon([(x, H * 0.72), (x + step / 2, H * 0.3), (x + step, H * 0.72)], fill=zig)
            x += step
        im.alpha_composite(masked(z, body))
    sheen = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(sheen).rounded_rectangle([m * 4, m * 3, W - m * 4, H * 0.42], radius=H * 0.2, fill=(255, 255, 255, 70))
    im.alpha_composite(masked(sheen.filter(ImageFilter.GaussianBlur(m * 0.6)), body))
    return done(im, w, h)


def panel(w, h, kind):
    im = canvas(w, h)
    W, H = im.size
    d = ImageDraw.Draw(im)
    u = H / h / 2.0 * 2   # one screen line
    if kind == 'teal':    # the game's settings window: notched corners, teal with a light metal rim
        n = 7 * u

        def shape(inset):
            a = inset
            return [(n + a, a), (W - n - a, a), (W - n - a, n * 0.5 + a), (W - a, n * 0.5 + a), (W - a, H - n * 0.5 - a), (W - n - a, H - n * 0.5 - a),
                    (W - n - a, H - a), (n + a, H - a), (n + a, H - n * 0.5 - a), (a, H - n * 0.5 - a), (a, n * 0.5 + a), (n + a, n * 0.5 + a)]
        d.polygon(shape(0), fill=(30, 50, 60, 190))
        d.polygon(shape(u * 0.7), fill=(190, 214, 220, 255))
        d.polygon(shape(u * 1.7), fill=(60, 96, 104, 255))
        body = Image.new('L', im.size, 0)
        ImageDraw.Draw(body).polygon(shape(u * 2.4), fill=255)
        g = vgrad(W, H, [(0, (26, 110, 116, 226)), (1, (14, 78, 92, 232))])
        im.paste(g, (0, 0), body)
    else:                 # the game's dialogue box: deep blue, a double line and fret corners
        d.rectangle([0, 0, W, H], fill=(16, 40, 104, 214))
        g = vgrad(W, H, [(0, (26, 62, 140, 214)), (1, (10, 28, 84, 224))])
        im.paste(g, (0, 0))
        d = ImageDraw.Draw(im)
        for a, c in ((u * 1.2, (236, 244, 255, 255)), (u * 2.6, (130, 180, 240, 255))):
            d.rectangle([a, a, W - a, H - a], outline=c, width=max(1, int(u * 0.7)))
        k = 6 * u
        for cx, cy in ((0, 0), (W, 0), (0, H), (W, H)):   # the fret: a small square knot on each corner
            sx = 1 if cx == 0 else -1; sy = 1 if cy == 0 else -1
            d.rectangle(sorted_box(cx, cy, cx + sx * k, cy + sy * k), fill=(16, 40, 104, 255), outline=(236, 244, 255, 255), width=max(1, int(u * 0.7)))
            d.rectangle(sorted_box(cx + sx * k * 0.35, cy + sy * k * 0.35, cx + sx * k * 0.8, cy + sy * k * 0.8), outline=(130, 180, 240, 255), width=max(1, int(u * 0.7)))
    return done(im, w, h)


def sorted_box(x0, y0, x1, y1):
    return [min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)]


def chevron(w, h):
    im = canvas(w, h)
    W, H = im.size
    t = H * 0.42
    pts = [(W * 0.06, H * 0.92), (W * 0.5, H * 0.08), (W * 0.94, H * 0.92), (W * 0.94 - t * 0.6, H * 0.92 + 0), (W * 0.5, H * 0.08 + t), (W * 0.06 + t * 0.6, H * 0.92)]
    m = Image.new('L', im.size, 0)
    ImageDraw.Draw(m).polygon(pts, fill=255)
    fat = m.filter(ImageFilter.MaxFilter(int(H * 0.12) | 1))
    dark = Image.new('RGBA', im.size, (150, 70, 10, 255)); dark.putalpha(fat)
    im.alpha_composite(dark)
    im.alpha_composite(masked(vgrad(W, H, [(0, (255, 226, 110, 255)), (1, (250, 140, 20, 255))]), m))
    return done(im, w, h)


def button(kind, size=16):
    im = canvas(size, size)
    W, H = im.size
    d = ImageDraw.Draw(im)
    d.ellipse([H * 0.04, H * 0.04, W - H * 0.04, H * 0.96], fill=(200, 210, 230, 255))
    d.ellipse([H * 0.10, H * 0.10, W - H * 0.10, H * 0.90], fill=(34, 40, 70, 255))
    hi = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(hi).ellipse([W * 0.2, H * 0.12, W * 0.8, H * 0.5], fill=(255, 255, 255, 46))
    im.alpha_composite(hi)
    d = ImageDraw.Draw(im)
    cx, cy, r, t = W / 2, H / 2, H * 0.24, int(H * 0.085)
    if kind == 0:      # cross
        c = (120, 170, 255, 255)
        d.line([(cx - r, cy - r), (cx + r, cy + r)], fill=c, width=t); d.line([(cx + r, cy - r), (cx - r, cy + r)], fill=c, width=t)
    elif kind == 1:    # square
        d.rectangle([cx - r, cy - r, cx + r, cy + r], outline=(255, 130, 210, 255), width=t)
    elif kind == 2:    # triangle
        c = (90, 230, 170, 255)
        pts = [(cx, cy - r * 1.15), (cx + r * 1.1, cy + r * 0.8), (cx - r * 1.1, cy + r * 0.8)]
        d.line(pts + [pts[0]], fill=c, width=t, joint='curve')
    elif kind == 3:    # left / right
        c = (255, 220, 90, 255)
        d.polygon([(cx - r * 1.3, cy), (cx - r * 0.2, cy - r), (cx - r * 0.2, cy + r)], fill=c)
        d.polygon([(cx + r * 1.3, cy), (cx + r * 0.2, cy - r), (cx + r * 0.2, cy + r)], fill=c)
    else:              # circle
        d.ellipse([cx - r * 1.1, cy - r * 1.1, cx + r * 1.1, cy + r * 1.1], outline=(255, 110, 100, 255), width=t)
    return done(im, size, size)


def keycap(w, h):
    im = canvas(w, h)
    W, H = im.size
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([H * 0.04, H * 0.04, W - H * 0.04, H * 0.96], radius=H * 0.26, fill=(200, 210, 230, 255))
    d.rounded_rectangle([H * 0.12, H * 0.12, W - H * 0.12, H * 0.88], radius=H * 0.2, fill=(52, 58, 84, 255))
    d.rounded_rectangle([H * 0.2, H * 0.16, W - H * 0.2, H * 0.48], radius=H * 0.14, fill=(84, 92, 124, 255))
    return done(im, w, h)


def platform(w, h):
    im = canvas(w, h)
    W, H = im.size
    g = Image.new('RGBA', im.size, (0, 0, 0, 0))
    dg = ImageDraw.Draw(g)
    dg.ellipse([W * 0.14, H * 0.2, W * 0.86, H * 0.8], fill=(120, 230, 255, 200))
    dg.ellipse([W * 0.24, H * 0.3, W * 0.76, H * 0.7], fill=(255, 255, 255, 255))
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(H * 0.07)))
    d = ImageDraw.Draw(im)
    t = int(H * 0.07)
    for a0 in (20, 200):   # two gold arcs with arrow heads: it turns
        d.arc([W * 0.04, H * 0.08, W * 0.96, H * 0.92], a0, a0 + 120, fill=(250, 190, 60, 255), width=t)
        ang = math.radians(a0 + 120)
        ex, ey = W / 2 + W * 0.46 * math.cos(ang), H / 2 + H * 0.42 * math.sin(ang)
        tx, ty = -math.sin(ang), math.cos(ang) * H / W
        n = math.hypot(tx, ty); tx, ty = tx / n, ty / n
        s = H * 0.16
        d.polygon([(ex + tx * s * 1.4, ey + ty * s * 1.4), (ex - ty * s, ey + tx * s * 0.5), (ex + ty * s, ey - tx * s * 0.5)], fill=(250, 190, 60, 255))
    return done(im, w, h)


def spark(size):
    """the small mark at the left of the chosen row: a little four-star ball"""
    return ball(size * 2 * SS).resize((int(round(size * 2 * PAR)), size * 2), Image.LANCZOS)


TEAL = dict(top=(46, 150, 160, 214), bottom=(20, 96, 120, 224), rim=(150, 220, 226, 255), zig=(255, 255, 255, 18))
GOLD = dict(top=(255, 244, 150, 255), bottom=(244, 176, 40, 255), rim=(255, 255, 240, 255), zig=(255, 255, 255, 60), glow=(255, 240, 150, 150))
PERI = dict(top=(140, 150, 236, 224), bottom=(84, 92, 196, 232), rim=(204, 214, 255, 255), zig=(255, 255, 255, 22))
STEEL = dict(top=(120, 140, 170, 240), bottom=(70, 86, 120, 244), rim=(204, 214, 230, 255))
AMBER = dict(top=(252, 214, 90, 255), bottom=(214, 140, 20, 255), rim=(255, 250, 220, 255))
DARK = dict(top=(30, 44, 96, 232), bottom=(12, 22, 60, 240), rim=(120, 150, 220, 255))

# name, picture, how it is cut when stretched: 0 not, 3 = left cap | middle | right cap, 9 = with corners of 8 screen pixels
PIECES = [
    ('ROW', lambda: pill(48, 26, **TEAL), 3), ('ROW_SEL', lambda: pill(48, 26, **GOLD), 3), ('TAB', lambda: pill(48, 22, **PERI), 3),
    ('OFF', lambda: pill(40, 16, **STEEL), 3), ('ON', lambda: pill(40, 16, **AMBER), 3), ('SLOT', lambda: pill(40, 16, **DARK), 3),
    ('PANEL', lambda: panel(48, 48, 'teal'), 9), ('TALK', lambda: panel(48, 48, 'talk'), 9),
    ('UP', lambda: chevron(18, 10), 0), ('DOWN', lambda: chevron(18, 10).transpose(Image.FLIP_TOP_BOTTOM), 0),
    ('CROSS', lambda: button(0), 0), ('SQUARE', lambda: button(1), 0), ('TRIANGLE', lambda: button(2), 0), ('LEFTRIGHT', lambda: button(3), 0),
    ('CIRCLE', lambda: button(4), 0), ('KEY', lambda: keycap(24, 14), 3), ('PLATFORM', lambda: platform(140, 44), 0), ('SPARK', lambda: spark(12), 0),
    ('CELL', lambda: cell(), 0), ('CELL_RIM', lambda: cell_rim(), 0), ('CURSOR', lambda: cursor(), 0), ('CAP_ON', lambda: capsule(True), 0), ('CAP_OFF', lambda: capsule(False), 0),
    ('BALL', lambda: big_ball(30), 0), ('NAMEPLATE', lambda: nameplate(250, 62), 0),
]


def pieces():
    sheet = Image.new('RGBA', (512, 512), (0, 0, 0, 0))
    table = []
    x, y, rowh = 2, 2, 0
    for name, make, cut in PIECES:
        im = make()
        if x + im.size[0] + 2 > 512:
            x = 2; y += rowh + 4; rowh = 0
        sheet.alpha_composite(im, (x, y))
        table.append((name, x, y, im.size[0], im.size[1], cut))
        x += im.size[0] + 4
        rowh = max(rowh, im.size[1])
    return sheet, table


# ------------------------------------------------------------------------------------------- the mod grid's backdrop
def grid_backdrop():
    """The second page of the character select (the new fighters): a dusk sky, the light band the portrait stands in,
    the tray of the grid and the box of the buttons. What moves or is read is drawn over it by the game."""
    S = 2
    W, H = 512 * S, 448 * S
    im = vgrad(W, H, [(0, (112, 84, 150, 255)), (0.5, (160, 110, 160, 255)), (1, (206, 150, 170, 255))])
    soft = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    ds = ImageDraw.Draw(soft)
    rng = np.random.RandomState(11)
    for _ in range(34):
        x, y, r = rng.uniform(0, W), rng.uniform(H * 0.1, H), rng.uniform(40, 120)
        c = (232, 190, 200, 70) if rng.rand() < 0.5 else (90, 60, 130, 70)
        ds.ellipse([x - r * 2.2, y - r * 0.45, x + r * 2.2, y + r * 0.45], fill=c)
    im.alpha_composite(soft.filter(ImageFilter.GaussianBlur(16)))
    # the band of light, leaning, and the line that parts the page
    band = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    db = ImageDraw.Draw(band)
    db.polygon([(372 * S, 0), (W, 0), (W, H), (268 * S, H)], fill=(236, 214, 246, 120))
    db.polygon([(402 * S, 0), (470 * S, 0), (366 * S, H), (298 * S, H)], fill=(255, 250, 255, 70))
    im.alpha_composite(band)
    d = ImageDraw.Draw(im)
    d.line([(372 * S, -4), (268 * S, H + 4)], fill=(255, 255, 255, 255), width=10)
    d.line([(369 * S, -4), (265 * S, H + 4)], fill=(20, 10, 30, 255), width=8)
    cloud_curl(d, 60, 56, 1.0, 1)
    cloud_curl(d, W - 90, 60, 1.0, -1)
    # the rule under the title
    d.rounded_rectangle([0, 71 * S, 310 * S, 71 * S + 5], radius=2, fill=(204, 190, 255, 255))
    # the tray: an angular violet frame with warning stripes at its corners, a dark floor inside
    x0, y0, x1, y1 = GRID_X0 * S, GRID_Y0 * S, GRID_X1 * S, GRID_Y1 * S
    n = 12 * S

    def frame(a):
        return [(x0 + n + a, y0 + a), (x1 - a, y0 + a), (x1 - a, y1 - n - a), (x1 - n - a, y1 - a), (x0 + a, y1 - a), (x0 + a, y0 + n + a)]
    sh = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(sh).polygon([(x + 8, y + 10) for x, y in frame(0)], fill=(20, 0, 40, 130))
    im.alpha_composite(sh.filter(ImageFilter.GaussianBlur(6)))
    d = ImageDraw.Draw(im)
    d.polygon(frame(0), fill=(36, 18, 70, 255))
    d.polygon(frame(3), fill=(214, 180, 255, 255))
    d.polygon(frame(6), fill=(124, 72, 196, 255))
    d.polygon(frame(9 * S), fill=(40, 22, 76, 255))
    d.polygon(frame(9 * S + 3), fill=(58, 40, 96, 255))
    stripes = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    dst = ImageDraw.Draw(stripes)
    for (sx0, sx1, sy) in ((x0 + n + 6, x0 + 92 * S, y0 + 7), (x1 - 92 * S, x1 - n - 6, y1 - 9 * S + 1), (x1 - 92 * S, x1 - 8, y0 + 7), (x0 + 8, x0 + 92 * S, y1 - 9 * S + 1)):
        x = sx0
        while x + 17 < sx1:
            dst.polygon([(x, sy + 9 * S - 9), (x + 9, sy), (x + 17, sy), (x + 8, sy + 9 * S - 9)], fill=(22, 12, 40, 255))
            x += 20
    im.alpha_composite(stripes)
    # the box of the buttons: the game's dialogue box
    box = panel(BOX_X1 - BOX_X0, BOX_Y1 - BOX_Y0, 'talk')
    im.alpha_composite(box, (BOX_X0 * S, BOX_Y0 * S))
    im = im.resize((512, 448), Image.LANCZOS)
    full = Image.new('RGB', (512, 512), (40, 20, 60))
    full.paste(im.convert('RGB'), (0, 0))
    return full


GRID_X0, GRID_Y0, GRID_X1, GRID_Y1 = 24, 82, 264, 352   # the tray
CELL_X, CELL_Y, CELL_W, CELL_H, CELL_PX, CELL_PY = 41, 100, 36, 42, 42, 48
BOX_X0, BOX_Y0, BOX_X1, BOX_Y1 = 18, 358, 250, 440


def cell():
    """an empty place of the grid: a sunk dark square with the game's faint zigzag"""
    im = canvas(CELL_W, CELL_H)
    W, H = im.size
    d = ImageDraw.Draw(im)
    u = H / CELL_H
    d.rounded_rectangle([0, 0, W - 1, H - 1], radius=u * 3, fill=(26, 14, 50, 255))
    d.rounded_rectangle([u, u, W - 1 - u, H - 1 - u], radius=u * 2.4, fill=(96, 78, 130, 255))
    d.rounded_rectangle([u * 2, u * 2, W - 1 - u * 2, H - 1 - u * 2], radius=u * 2, fill=(64, 50, 96, 255))
    z = Image.new('RGBA', im.size, (0, 0, 0, 0))
    dz = ImageDraw.Draw(z)
    for k in range(4):
        yy = H * (0.2 + 0.2 * k)
        dz.line([(u * 4, yy + u * 3), (W * 0.27, yy), (W * 0.5, yy + u * 3), (W * 0.73, yy), (W - u * 4, yy + u * 3)], fill=(84, 68, 118, 255), width=int(u * 1.2))
    im.alpha_composite(z)
    return done(im, CELL_W, CELL_H)


def cell_rim():
    """the border of a face on the grid, hollow"""
    im = canvas(CELL_W, CELL_H)
    W, H = im.size
    d = ImageDraw.Draw(im)
    u = H / CELL_H
    d.rounded_rectangle([0, 0, W - 1, H - 1], radius=u * 3, outline=(22, 12, 40, 255), width=int(u * 1.4))
    d.rounded_rectangle([u * 1.2, u * 1.2, W - 1 - u * 1.2, H - 1 - u * 1.2], radius=u * 2.4, outline=(170, 160, 190, 255), width=int(u * 1.2))
    d.rounded_rectangle([u * 2.2, u * 2.2, W - 1 - u * 2.2, H - 1 - u * 2.2], radius=u * 2, outline=(40, 30, 60, 255), width=int(u * 1.0))
    return done(im, CELL_W, CELL_H)


def cursor():
    """the cursor: a bright yellow frame with light around it, hollow"""
    w, h = CELL_W + 12, CELL_H + 12
    im = canvas(w, h)
    W, H = im.size
    u = H / h
    g = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(g).rounded_rectangle([u * 2, u * 2, W - u * 2, H - u * 2], radius=u * 6, outline=(255, 236, 120, 255), width=int(u * 5))
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(u * 2.2)))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([u * 3.5, u * 3.5, W - u * 3.5, H - u * 3.5], radius=u * 4.5, outline=(120, 70, 10, 255), width=int(u * 4.4))
    d.rounded_rectangle([u * 4.2, u * 4.2, W - u * 4.2, H - u * 4.2], radius=u * 4, outline=(255, 232, 70, 255), width=int(u * 3))
    d.rounded_rectangle([u * 4.8, u * 4.8, W - u * 4.8, H - u * 4.8], radius=u * 3.6, outline=(255, 252, 200, 255), width=int(u * 1))
    return done(im, w, h)


def nameplate(w, h):
    """the plate of the fighter's two names: magenta to violet in a silver rim, its left end leaning"""
    im = canvas(w, h)
    W, H = im.size
    u = H / h
    lean = u * 12

    def shape(a):
        return [(lean + a, a), (W + 4, a), (W + 4, H - a), (a * 1.6, H - a)]
    sh = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(sh).polygon(shape(0), fill=(20, 0, 30, 150))
    im.alpha_composite(sh.filter(ImageFilter.GaussianBlur(u * 1.2)))
    d = ImageDraw.Draw(im)
    d.polygon(shape(u * 1.5), fill=(60, 40, 80, 255))
    d.polygon(shape(u * 2.4), fill=(232, 226, 240, 255))
    d.polygon(shape(u * 4.0), fill=(120, 110, 140, 255))
    body = Image.new('L', im.size, 0)
    ImageDraw.Draw(body).polygon(shape(u * 4.8), fill=255)
    im.paste(vgrad(W, H, [(0, (236, 80, 190, 255)), (0.5, (190, 50, 170, 255)), (1, (110, 40, 150, 255))]), (0, 0), body)
    sheen = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(sheen).polygon([(lean + u * 8, u * 6), (W, u * 6), (W, H * 0.42), (lean * 0.6 + u * 8, H * 0.42)], fill=(255, 255, 255, 46))
    im.alpha_composite(masked(sheen, body))
    return done(im, w, h)


def capsule(lit):
    """a costume's marker: a small Capsule Corporation capsule, leaning"""
    w, h = 14, 20
    im = canvas(w, h)
    W, H = im.size
    big = Image.new('RGBA', (H, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    cw = H * 0.40
    x0 = (H - cw) / 2
    top, bot = ((255, 236, 120, 255), (240, 150, 30, 255)) if lit else ((150, 120, 200, 255), (84, 60, 140, 255))
    d.rounded_rectangle([x0 - 3, H * 0.04 - 3, x0 + cw + 3, H * 0.96 + 3], radius=cw / 2 + 3, fill=(30, 16, 50, 255))
    d.rounded_rectangle([x0, H * 0.04, x0 + cw, H * 0.96], radius=cw / 2, fill=bot)
    d.rounded_rectangle([x0, H * 0.04, x0 + cw, H * 0.5], radius=cw / 2, fill=top)
    d.rectangle([x0, H * 0.36, x0 + cw, H * 0.5], fill=top)
    d.line([(x0, H * 0.5), (x0 + cw, H * 0.5)], fill=(30, 16, 50, 255), width=max(2, int(H * 0.03)))
    d.rounded_rectangle([x0 + cw * 0.2, H * 0.12, x0 + cw * 0.42, H * 0.4], radius=cw * 0.1, fill=(255, 255, 255, 150))
    big = big.rotate(-28, resample=Image.BICUBIC)
    im.alpha_composite(big.resize((W, H), Image.LANCZOS))
    return done(im, w, h)


def big_ball(size):
    return ball(size * 2 * SS).resize((int(round(size * 2 * PAR)), size * 2), Image.LANCZOS)


# -------------------------------------------------------------------------------------------------------------- the file
def indexed_rgba(im, alpha_levels, colours=None):
    a = np.array(im.convert('RGBA'))
    colours = colours or 255 // alpha_levels
    # colours are chosen from what can be seen
    seen = Image.fromarray(a[..., :3][a[..., 3] > 8].reshape(-1, 1, 3)) if (a[..., 3] > 8).any() else Image.new('RGB', (1, 1))
    q = seen.quantize(colours, method=Image.MEDIANCUT, dither=Image.NONE)
    p = np.array(q.getpalette()[:colours * 3], dtype=np.int32).reshape(-1, 3)
    flat = a[..., :3].reshape(-1, 3).astype(np.int32)
    ci = np.zeros(len(flat), dtype=np.int32)
    for s in range(0, len(flat), 65536):
        dist = ((flat[s:s + 65536, None, :] - p[None, :, :]) ** 2).sum(axis=2)
        ci[s:s + 65536] = dist.argmin(axis=1)
    ci = ci.reshape(a.shape[:2])
    al = np.round(a[..., 3].astype(np.float64) / 255.0 * alpha_levels).astype(np.int32)
    idx = np.where(al == 0, 0, 1 + ci * alpha_levels + (al - 1))
    pal = np.zeros((256, 4), dtype=np.uint8)
    for c in range(len(p)):
        for l in range(alpha_levels):
            pal[1 + c * alpha_levels + l, :3] = p[c]
            pal[1 + c * alpha_levels + l, 3] = round((l + 1) / alpha_levels * 0x80)
    return idx.astype(np.uint8), pal


def indexed_rgb(im):
    q = im.convert('RGB').quantize(256, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG)
    p = np.array(q.getpalette()[:768], dtype=np.uint8).reshape(256, 3)
    pal = np.zeros((256, 4), dtype=np.uint8)
    pal[:, :3] = p; pal[:, 3] = 0x80
    return np.array(q, dtype=np.uint8), pal


def indexed_alpha(im):
    idx = np.array(im, dtype=np.uint8)
    pal = np.zeros((256, 4), dtype=np.uint8)
    pal[:, :3] = 255
    pal[:, 3] = np.round(np.arange(256) / 255.0 * 0x80)
    return idx, pal


if __name__ == '__main__':
    bg = backdrop()
    ti = title()
    font, glyphs = letters()
    sheet, table = pieces()
    gb = grid_backdrop()
    ents = [entry(*indexed_rgb(bg)), entry(*indexed_rgba(ti, 7)), entry(*indexed_alpha(font)), entry(*indexed_rgba(sheet, 7)), entry(*indexed_rgb(gb))]
    print('wrote', write(ents, OUT + '/ui.bin'), 'bytes')
    with open(HDR, 'w', newline='\n') as f:
        f.write('/* Generated by port/port/build/ml/modmgr_assets2.py: where things are in gamedata/modmgr/ui.bin. */\n')
        f.write('#ifndef PORT_MODART_H\n#define PORT_MODART_H\n\n')
        f.write('#define MA_TEX_BACK 0\n#define MA_TEX_TITLE 1\n#define MA_TEX_FONT 2\n#define MA_TEX_PIECES 3\n#define MA_TEX_GRID 4\n#define MA_TEX_COUNT 5\n\n')
        f.write('/* the letters (ASCII 32..126): x, y in the sheet, width there (with %d pixels of room each side), and how far\n   the next letter is, in sixteenths; a row is %d high; the outlines are 256 lines below */\n' % (PAD, CELL))
        f.write('#define MA_FONT_CELL %d\n#define MA_FONT_PAD %d\n' % (CELL, PAD))
        f.write('static const u16 sMaGlyph[95][4] = {\n')
        for x, y, w, adv in glyphs:
            f.write('    {%d, %d, %d, %d},\n' % (x, y, w, int(round(adv * 16))))
        f.write('};\n\n/* the pieces: x, y, width, height in the sheet (twice the screen\'s size), and how they stretch */\n')
        for i, (name, x, y, w, h, cut) in enumerate(table):
            f.write('#define MA_%s %d\n' % (name, i))
        f.write('static const u16 sMaPiece[%d][5] = {\n' % len(table))
        for name, x, y, w, h, cut in table:
            f.write('    {%d, %d, %d, %d, %d}, /* %s */\n' % (x, y, w, h, cut, name))
        f.write('};\n\n/* the mod grid: its first cell, a cell and the way to the next; the box of its buttons */\n')
        for k, v in (('CELL_X', CELL_X), ('CELL_Y', CELL_Y), ('CELL_W', CELL_W), ('CELL_H', CELL_H), ('CELL_PX', CELL_PX), ('CELL_PY', CELL_PY),
                     ('BOX_X', BOX_X0), ('BOX_Y', BOX_Y0)):
            f.write('#define MA_%s %d.0f\n' % (k, v))
        f.write('\n#endif\n')
    # what the file holds, on a checked ground
    prev = Image.new('RGBA', (1024, 448 + 512 + 8), (90, 90, 110, 255))
    prev.paste(bg.crop((0, 0, 512, 448)), (0, 0))
    over = Image.new('RGBA', (512, 448), (40, 120, 200, 255))
    over.alpha_composite(ti, (0, 0))
    f2 = Image.new('RGBA', (512, 512), (255, 255, 255, 0)); f2.putalpha(font)
    over.alpha_composite(f2.crop((0, 0, 512, 310)), (0, 132))
    prev.paste(over, (512, 0))
    chk = Image.new('RGBA', (512, 512), (60, 140, 220, 255))
    chk.alpha_composite(sheet)
    prev.paste(chk, (0, 456))
    dark = Image.new('RGBA', (512, 512), (20, 20, 40, 255)); dark.alpha_composite(f2)
    prev.paste(dark, (512, 456))
    prev.convert('RGB').save(os.path.join(HERE, '..', 'modmgr_assets2.png'))
    print('pieces:', [(t[0], t[3], t[4]) for t in table])
    look = Image.new('RGBA', (1024, 896), (0, 0, 0, 255))
    look.paste(gb.crop((0, 0, 512, 448)).resize((1024, 896), Image.BILINEAR), (0, 0))
    by = {t[0]: sheet.crop((t[1], t[2], t[1] + t[3], t[2] + t[4])) for t in table}
    for i in range(25):
        look.alpha_composite(by['CELL'], (int((CELL_X + CELL_PX * (i % 5)) * 2), int((CELL_Y + CELL_PY * (i // 5)) * 2)))
    for i in (0, 1, 5):
        look.alpha_composite(by['CELL_RIM'], (int((CELL_X + CELL_PX * (i % 5)) * 2), int((CELL_Y + CELL_PY * (i // 5)) * 2)))
    look.alpha_composite(by['CURSOR'], (int((CELL_X - 6 + CELL_PX) * 2), int((CELL_Y - 6) * 2)))
    look.alpha_composite(by['NAMEPLATE'], (262 * 2, 356 * 2))
    for k in range(4):
        look.alpha_composite(by['CAP_ON' if k == 1 else 'CAP_OFF'], ((440 + 16 * k) * 2, 388 * 2))
    look.alpha_composite(by['BALL'], (22 * 2, 26 * 2))
    look.convert('RGB').save(os.path.join(HERE, '..', 'modgrid_look.png'))
