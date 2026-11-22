#!/usr/bin/env python3
"""The Tag Team mod's new menu pictures (2026-11-21): the tournament arena behind the menus (db_arena.png), Goku cut out
of his video frame (db_goku.png), the gold TAG TEAM lettering (db_logo.png) and the blue glass of the panels
(db_glass.png). The inputs are the two clips the user made (a still of Goku on white, the arena with its ring).

    port/tools/tag_menu_art.py GOKU.mp4 ARENA.mp4 [goku arena logo glass plates]
"""
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "port/tbo/ui"
FONTS = ROOT / "port/third_party/fonts"


def frame(path, at):
    c = cv2.VideoCapture(path)
    n = int(c.get(cv2.CAP_PROP_FRAME_COUNT))
    c.set(cv2.CAP_PROP_POS_FRAMES, int(n * at))
    ok, im = c.read()
    if not ok:
        raise SystemExit("cannot read " + path)
    return im


def arena(path):
    """The arena, 1920 x 864 (the menus' backdrop shape): the clip is 16:9, so it is cut to the wide shape; the left, where the
    list sits, is blurred and shaded deep blue (the picture shows through, soft), the foot darkens where the bar is."""
    im = frame(path, 0.30)
    h, w = im.shape[:2]
    ch = int(w * 864 / 1920)
    y0 = int((h - ch) * 0.62)
    im = cv2.resize(im[y0:y0 + ch], (1920, 864), interpolation=cv2.INTER_LANCZOS4).astype(np.float32)
    blur = cv2.GaussianBlur(im, (0, 0), 5)
    x = np.arange(1920, dtype=np.float32)[None, :, None]
    t = np.clip((1060 - x) / 640.0, 0, 1)
    im = im * (1 - t) + blur * t
    shade = np.clip(1 - x / 990.0, 0, 1) ** 1.3 * 0.84
    im = im * (1 - shade) + np.array([36, 10, 4], np.float32) * shade  # (BGR: deep blue)
    y = np.arange(864, dtype=np.float32)[:, None, None]
    low = np.clip((y - 560) / 304.0, 0, 1) * 0.74
    im = im * (1 - low) + np.array([30, 8, 3], np.float32) * low
    Image.fromarray(cv2.cvtColor(np.clip(im, 0, 255).astype(np.uint8), cv2.COLOR_BGR2RGB)).save(OUT / "db_arena.png")


def arena_film(path):
    """The arena as a film (port/tbo/ui/arena.mpg, 1280 x 576, MPEG-1, 24 frames a second; gs/gs_draw.c plays it in place of
    db_arena.png, which is its first picture): the crowd and the flags of the clip moving behind the menus, every picture
    with the same blur and shade on the left as db_arena.png has, forward and then back so the loop has no seam."""
    import av
    c = cv2.VideoCapture(path)
    frames = []
    W, H = 1280, 576
    x = np.arange(W, dtype=np.float32)[None, :, None]
    y = np.arange(H, dtype=np.float32)[:, None, None]
    t = np.clip((707 - x) / 427.0, 0, 1)
    shade = np.clip(1 - x / 660.0, 0, 1) ** 1.3 * 0.84
    low = np.clip((y - 373) / 203.0, 0, 1) * 0.74
    while True:
        ok, im = c.read()
        if not ok:
            break
        h, w = im.shape[:2]
        ch = int(w * H / W)
        y0 = int((h - ch) * 0.62)
        im = cv2.resize(im[y0:y0 + ch], (W, H), interpolation=cv2.INTER_AREA).astype(np.float32)
        blur = cv2.GaussianBlur(im, (0, 0), 3.3)
        im = im * (1 - t) + blur * t
        im = im * (1 - shade) + np.array([36, 10, 4], np.float32) * shade
        im = im * (1 - low) + np.array([30, 8, 3], np.float32) * low
        frames.append(np.clip(im, 0, 255).astype(np.uint8))
    order = list(range(len(frames))) + list(range(len(frames) - 2, 0, -1))
    container = av.open(str(OUT / "arena.mpg"), "w", format="mpeg")
    stream = container.add_stream("mpeg1video", rate=24)
    stream.width, stream.height = W, H
    stream.pix_fmt = "yuv420p"
    stream.bit_rate = 6_000_000
    stream.codec_context.options = {"qmin": "2", "qmax": "8", "g": "12"}
    for i in order:
        fr = av.VideoFrame.from_ndarray(cv2.cvtColor(frames[i], cv2.COLOR_BGR2RGB), format="rgb24")
        for pkt in stream.encode(fr):
            container.mux(pkt)
    for pkt in stream.encode():
        container.mux(pkt)
    container.close()
    Image.fromarray(cv2.cvtColor(frames[0], cv2.COLOR_BGR2RGB)).save(OUT / "db_arena.png")
    print("arena.mpg:", (OUT / "arena.mpg").stat().st_size, "bytes;", len(order), "pictures")


GOKU_FRAMES, GOKU_COLS, GOKU_ROWS, GOKU_W, GOKU_H = 10, 5, 2, 380, 450


def cutout(im):
    """Goku without the white round him: the white that touches the picture's edge is the background, the white inside
    (eyes, highlights) stays. Returns RGBA (h, w, 4)."""
    h, w = im.shape[:2]
    near = (im.min(axis=2) >= 236).astype(np.uint8)
    pad = cv2.copyMakeBorder(near, 1, 1, 1, 1, cv2.BORDER_CONSTANT, value=1)
    lab = cv2.connectedComponents(pad, connectivity=4)[1]
    bg = (lab == lab[0, 0])[1:-1, 1:-1].astype(np.uint8)
    fg = 1 - bg
    fg = cv2.morphologyEx(fg, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    fg = cv2.erode(fg, np.ones((3, 3), np.uint8))
    a = cv2.GaussianBlur(fg.astype(np.float32), (0, 0), 1.1)
    a = np.clip((a - 0.25) / 0.6, 0, 1)
    return np.dstack([cv2.cvtColor(im, cv2.COLOR_BGR2RGB), (a * 255).astype(np.uint8)])


def goku(path):
    """Goku's idle loop: ten stills of the clip (cut out the same way, one box for all so he does not jump), from the
    thigh up (the lower 40% dropped, the last 7% of what is left fading), on one sheet of 5 x 2 cells (db_goku.png).
    The game plays them forward and back."""
    c = cv2.VideoCapture(path)
    n = int(c.get(cv2.CAP_PROP_FRAME_COUNT))
    cuts = []
    for k in range(GOKU_FRAMES):
        c.set(cv2.CAP_PROP_POS_FRAMES, int(n * k / GOKU_FRAMES))
        ok, im = c.read()
        if not ok:
            raise SystemExit("cannot read " + path)
        cuts.append(cutout(im))
    ys = np.concatenate([np.where(r[:, :, 3] > 12)[0] for r in cuts])
    xs = np.concatenate([np.where(r[:, :, 3] > 12)[1] for r in cuts])
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    keep = int((y1 - y0) * 0.60)
    sheet = Image.new("RGBA", (GOKU_W * GOKU_COLS, GOKU_H * GOKU_ROWS))
    for k, r in enumerate(cuts):
        r = r[y0:y0 + keep, x0:x1].copy()
        fade = int(keep * 0.07)
        for i in range(fade):
            r[keep - fade + i, :, 3] = (r[keep - fade + i, :, 3] * (1 - (i + 1) / fade)).astype(np.uint8)
        im = Image.fromarray(r, "RGBA")
        sc = min((GOKU_W - 8) / im.width, (GOKU_H - 4) / im.height)
        im = im.resize((int(im.width * sc), int(im.height * sc)), Image.LANCZOS)
        cell = Image.new("RGBA", (GOKU_W, GOKU_H))
        cell.alpha_composite(im, ((GOKU_W - im.width) // 2, 2))
        sheet.alpha_composite(cell, ((k % GOKU_COLS) * GOKU_W, (k // GOKU_COLS) * GOKU_H))
    sheet.save(OUT / "db_goku.png")
    print("goku sheet", sheet.size)


def logo():
    """TAG TEAM in the gold italic lettering of the mockup: white-yellow to orange, a navy outline, a pale blue one."""
    font = ImageFont.truetype(str(FONTS / "kanit/Kanit-ExtraBoldItalic.ttf"), 300)
    W, H = 1500, 460
    S = Image.new("RGBA", (W, H))
    mask = Image.new("L", (W, H))
    ImageDraw.Draw(mask).text((W // 2, H // 2 + 10), "TAG TEAM", font=font, fill=255, anchor="mm")
    bb = mask.getbbox()
    base = Image.new("RGBA", (W, H))
    for r, col in ((34, (255, 255, 255, 255)), (26, (30, 70, 190, 255)), (16, (6, 20, 90, 255))):
        m = mask.filter(ImageFilter.MaxFilter(r * 2 + 1)) if r < 40 else mask
        base.paste(Image.new("RGBA", (W, H), col), (0, 0), m)
    grad = Image.new("RGBA", (W, H))
    gd = ImageDraw.Draw(grad)
    for y in range(bb[1], bb[3] + 1):
        t = (y - bb[1]) / max(1, bb[3] - bb[1])
        c = (255, int(250 - 130 * t), int(190 - 170 * t), 255)
        if t > 0.55:
            c = (255, int(200 - 60 * (t - 0.55) / 0.45), int(60 - 40 * (t - 0.55) / 0.45), 255)
        gd.line([(0, y), (W, y)], fill=c)
    base.paste(grad, (0, 0), mask)
    S.alpha_composite(base)
    S.crop((bb[0] - 50, bb[1] - 50, bb[2] + 50, bb[3] + 50)).save(OUT / "db_logo.png")


def glass():
    """Deep blue glass, a bright blue-white edge and a pale light along the top (nine pieces: the panels)."""
    S, k = 256, 4
    f = Image.new("RGBA", (S * k, S * k))
    d = ImageDraw.Draw(f)
    d.rounded_rectangle([0, 0, S * k - 1, S * k - 1], radius=40 * k, fill=(170, 220, 255, 235))
    d.rounded_rectangle([3 * k, 3 * k, S * k - 3 * k, S * k - 3 * k], radius=37 * k, fill=(10, 30, 110, 226))
    top = Image.new("RGBA", f.size)
    td = ImageDraw.Draw(top)
    for y in range(3 * k, 90 * k):
        t = (y - 3 * k) / (87 * k)
        td.line([(0, y), (S * k, y)], fill=(120, 190, 255, int(70 * (1 - t))))
    m = Image.new("L", f.size)
    ImageDraw.Draw(m).rounded_rectangle([3 * k, 3 * k, S * k - 3 * k, S * k - 3 * k], radius=37 * k, fill=255)
    o = Image.new("RGBA", f.size)
    o.paste(top, (0, 0), m)
    f.alpha_composite(o)
    f.resize((S, S), Image.LANCZOS).save(OUT / "db_glass.png")


def plate(name, edge, top, bottom, shine):
    """A list plate (nine pieces): a light edge, a gradient from `top` to `bottom`, a pale gloss on its upper half."""
    S, k = 256, 4
    f = Image.new("RGBA", (S * k, S * k))
    d = ImageDraw.Draw(f)
    r = 60 * k
    d.rounded_rectangle([0, 0, S * k - 1, S * k - 1], radius=r, fill=edge)
    m = Image.new("L", f.size)
    ImageDraw.Draw(m).rounded_rectangle([5 * k, 5 * k, S * k - 5 * k, S * k - 5 * k], radius=r - 5 * k, fill=255)
    g = Image.new("RGBA", f.size)
    gd = ImageDraw.Draw(g)
    for y in range(f.height):
        t = y / f.height
        gd.line([(0, y), (f.width, y)], fill=tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)) + (255,))
    f.paste(g, (0, 0), m)
    gl = Image.new("RGBA", f.size)
    gld = ImageDraw.Draw(gl)
    for y in range(5 * k, 128 * k):
        t = (y - 5 * k) / (123 * k)
        gld.line([(0, y), (f.width, y)], fill=shine + (int(46 * (1 - t * 0.6)),))
    o = Image.new("RGBA", f.size)
    o.paste(gl, (0, 0), m)
    f.alpha_composite(o)
    f.resize((S, S), Image.LANCZOS).save(OUT / (name + ".png"))


def plates():
    plate("db_plate", (96, 140, 215, 255), (22, 44, 112), (4, 10, 48), (150, 190, 255))
    plate("db_plate_on", (255, 250, 220, 255), (255, 190, 60), (232, 112, 8), (255, 250, 200))


STEPS = {"goku": lambda a: goku(a[0]), "arena": lambda a: arena(a[1]), "arenafilm": lambda a: arena_film(a[1]), "logo": lambda a: logo(), "glass": lambda a: glass(), "plates": lambda a: plates()}

if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) < 2:
        raise SystemExit(__doc__)
    for name in (args[2:] or STEPS):
        STEPS[name](args)
    print("->", OUT)
