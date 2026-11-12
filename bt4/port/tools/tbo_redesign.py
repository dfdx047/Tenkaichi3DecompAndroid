#!/usr/bin/env python3
"""The pieces of the redesigned Tenkaichi Budokai Online screens (port/tbo/ui/td_*.png), drawn as in the approved
mock-ups (2712 x 1220, a phone's screen: one pixel of these pictures is one pixel of that screen). Again with:
    python3 port/tools/tbo_redesign.py

Pieces that stretch (src/menu/port_tbo.c, Td_Slice9 / Td_Slice3) keep their corners, or their ends, at that scale:
  td_win      a window: dark glass in a double gold rim, a thin cyan line inside, gold corner marks; its shadow round it
  td_tab      a window's title tab (the red dot on its left; the title is written over it)
  td_opt      an option's bar, slanted; td_opt_on the one under the cursor (orange, glowing)
  td_key      a key of the keypads; td_key_on the one under the cursor
  td_chip     a small button; td_chip_on the one under the cursor
  td_btn      the big orange button
  td_box      the box at the foot of the page
  td_plate    the player's name plate (its 1P seal on the left)
Whole pictures:
  td_band     the title's band at the top left
  td_caps     the seven Capsule Corp. capsules (1 to 7, one colour each), in a row
  td_machine  the hub's Time Machine without what moves or changes (the destinations, the needle, the energy)
  td_vignette the dark edge laid over the time tunnel
Words are not in the pictures (they are the screens', in the chosen language), but for the machine's own name plate.
Fonts: Bangers (titles, menus, buttons) and Chakra Petch Bold (texts), port/third_party/fonts. Then tbo_sizes.py.
"""
import math
import pathlib
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/tbo/ui"
FONTS = ROOT / "port/third_party/fonts"
K = 2  # drawn twice as large, then reduced: smooth edges

TITLE = ((255, 250, 210), (255, 150, 20))


class Pic:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.im = Image.new("RGBA", (w * K, h * K))

    def save(self, name):
        self.im.resize((self.w, self.h), Image.LANCZOS).save(OUT / f"{name}.png", optimize=True)
        print(name, self.w, self.h)


_fonts = {}


def F(kind, size):
    key = (kind, int(size * K))
    if key not in _fonts:
        path = {"b": "bangers/Bangers-Regular.ttf", "cb": "chakrapetch/ChakraPetch-Bold.ttf"}[kind]
        _fonts[key] = ImageFont.truetype(str(FONTS / path), int(size * K))
    return _fonts[key]


def P(*v):
    return [int(round(x * K)) for x in v]


def chamfer(x0, y0, x1, y1, c):
    return [(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c), (x1 - c, y1), (x0 + c, y1), (x0, y1 - c), (x0, y0 + c)]


def skew(x0, y0, x1, y1, s):
    return [(x0 + s, y0), (x1 + s, y0), (x1 - s, y1), (x0 - s, y1)]


def grad_poly(pic, pts, top, bottom):
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    y0, y1 = min(ys), max(ys)
    bx0, by0 = int(min(xs) * K) - 2, int(y0 * K) - 2
    bx1, by1 = int(max(xs) * K) + 3, int(y1 * K) + 3
    w, h = bx1 - bx0, by1 - by0
    m = Image.new("L", (w, h))
    ImageDraw.Draw(m).polygon([(p[0] * K - bx0, p[1] * K - by0) for p in pts], fill=255)
    g = Image.new("RGBA", (1, h))
    for y in range(h):
        t = min(1, max(0, (y + by0 - y0 * K) / max(1, (y1 - y0) * K)))
        g.putpixel((0, y), tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(4)))
    piece = Image.composite(g.resize((w, h)), Image.new("RGBA", (w, h)), m)
    pic.im.alpha_composite(piece, (bx0, by0))


def outline(pic, pts, col, w):
    layer = Image.new("RGBA", pic.im.size)
    q = [tuple(P(*p)) for p in pts]
    ImageDraw.Draw(layer).line(q + [q[0]], fill=col, width=int(w * K), joint="curve")
    pic.im.alpha_composite(layer)


def glow_poly(pic, pts, col, blur):
    layer = Image.new("RGBA", pic.im.size)
    ImageDraw.Draw(layer).polygon([tuple(P(*p)) for p in pts], fill=col)
    pic.im.alpha_composite(layer.filter(ImageFilter.GaussianBlur(blur * K)))


def lines(pic, segs, col, w):
    layer = Image.new("RGBA", pic.im.size)
    d = ImageDraw.Draw(layer)
    for s in segs:
        d.line([tuple(P(*p)) for p in s], fill=col, width=int(w * K))
    pic.im.alpha_composite(layer)


def text(pic, xy, s, font, fill, anchor="la"):
    layer = Image.new("RGBA", pic.im.size)
    ImageDraw.Draw(layer).text(tuple(P(*xy)), s, font=font, fill=fill, anchor=anchor)
    pic.im.alpha_composite(layer)


def gtext(pic, xy, s, font, top, bottom, anchor="la", stroke=0, sfill=(40, 10, 0, 255)):
    x, y = P(*xy)
    layer = Image.new("RGBA", pic.im.size)
    if stroke:
        ImageDraw.Draw(layer).text((x, y), s, font=font, fill=sfill, anchor=anchor, stroke_width=int(stroke * K), stroke_fill=sfill)
    pic.im.alpha_composite(layer)
    m = Image.new("L", pic.im.size)
    ImageDraw.Draw(m).text((x, y), s, font=font, fill=255, anchor=anchor)
    bb = m.getbbox()
    g = Image.new("RGBA", (1, bb[3] - bb[1] + 1))
    for i in range(g.height):
        t = i / max(1, g.height - 1)
        g.putpixel((0, i), tuple(int(top[k] * (1 - t) + bottom[k] * t) for k in range(3)) + (255,))
    full = Image.new("RGBA", pic.im.size)
    full.paste(g.resize((bb[2] - bb[0] + 1, g.height)), (bb[0], bb[1]))
    pic.im.alpha_composite(Image.composite(full, Image.new("RGBA", pic.im.size), m))


# ------------------------------------------------------------------------------------------------ the pieces
PAD = 24  # room round a piece for its shadow or glow


def win():
    w, h, c = 400, 300, 30
    p = Pic(w + 2 * PAD, h + 2 * PAD)
    x0, y0, x1, y1 = PAD, PAD, PAD + w, PAD + h
    glow_poly(p, chamfer(x0 + 6, y0 + 10, x1 + 6, y1 + 12, c), (0, 0, 0, 150), 9)
    grad_poly(p, chamfer(x0, y0, x1, y1, c), (226, 190, 100, 255), (150, 104, 34, 255))
    grad_poly(p, chamfer(x0 + 7, y0 + 7, x1 - 7, y1 - 7, c - 4), (24, 18, 56, 246), (8, 6, 24, 250))
    outline(p, chamfer(x0 + 14, y0 + 14, x1 - 14, y1 - 14, c - 8), (110, 220, 255, 70), 2)
    marks = []
    for (cx, cy, sx, sy) in ((x0 + 22, y0 + 22, 1, 1), (x1 - 22, y0 + 22, -1, 1), (x0 + 22, y1 - 22, 1, -1), (x1 - 22, y1 - 22, -1, -1)):
        marks.append([(cx, cy + sy * 34), (cx, cy), (cx + sx * 34, cy)])
    lines(p, marks, (255, 210, 120, 220), 4)
    p.save("td_win")  # corner: PAD + 64


def tab():
    w, h = 400, 80
    p = Pic(w + 2 * 16, h + 2 * 16)
    o = 16
    px0, px1 = o + 26, o + w - 26
    pts = [(px0, o), (px1, o), (px1 + 26, o + 40), (px1, o + 80), (px0, o + 80), (px0 - 26, o + 40)]
    glow_poly(p, pts, (255, 150, 40, 120), 7)
    grad_poly(p, pts, (255, 236, 170, 255), (206, 150, 52, 255))
    outline(p, pts, (90, 50, 10, 255), 3)
    layer = Image.new("RGBA", p.im.size)
    ImageDraw.Draw(layer).ellipse(P(px0 + 16, o + 24, px0 + 48, o + 56), fill=(230, 80, 20, 255), outline=(110, 30, 0, 255), width=2 * K)
    p.im.alpha_composite(layer)
    p.save("td_tab")  # left end: 16 + 26 + 60, right end: 16 + 26 + 4


def opt(on):
    w, h, s = 600, 126, 16  # (the slant of the mock-ups' options)
    g = 18 if on else 6
    p = Pic(int(w + 2 * s + 2 * g), h + 2 * g)
    x0, y0, x1, y1 = g + s, g, g + s + w, g + h
    if on:
        glow_poly(p, skew(x0 - 6, y0 - 6, x1 + 6, y1 + 6, s), (255, 150, 40, 170), 8)
        grad_poly(p, skew(x0, y0, x1, y1, s), (255, 236, 196, 255), (255, 200, 120, 255))
        grad_poly(p, skew(x0 + 5, y0 + 5, x1 - 5, y1 - 5, s), (255, 162, 50, 255), (196, 64, 6, 255))
        hl = Image.new("RGBA", p.im.size)
        ImageDraw.Draw(hl).polygon([tuple(P(*q)) for q in skew(x0 + 8, y0 + 7, x1 - 8, y0 + h * 0.40, s * 0.4)], fill=(255, 255, 255, 50))
        p.im.alpha_composite(hl)
    else:
        grad_poly(p, skew(x0, y0, x1, y1, s), (200, 160, 80, 255), (120, 84, 28, 255))
        grad_poly(p, skew(x0 + 4, y0 + 4, x1 - 4, y1 - 4, s), (24, 70, 44, 245), (6, 26, 16, 250))
    p.save("td_opt_on" if on else "td_opt")  # ends: g + 2 s + 10


def chamfer_piece(name, w, h, c, edge, fill, inner, glow=None, line=None):
    g = 14 if glow else 2
    p = Pic(w + 2 * g, h + 2 * g)
    x0, y0, x1, y1 = g, g, g + w, g + h
    if glow:
        glow_poly(p, chamfer(x0 - 4, y0 - 4, x1 + 4, y1 + 4, c), glow, 6)
    grad_poly(p, chamfer(x0, y0, x1, y1, c), edge[0], edge[1])
    grad_poly(p, chamfer(x0 + inner, y0 + inner, x1 - inner, y1 - inner, max(2, c - 2)), fill[0], fill[1])
    if line:
        outline(p, chamfer(x0, y0, x1, y1, c), line, 3)
    p.save(name)


def keys():
    chamfer_piece("td_key", 160, 100, 18, ((210, 170, 90, 255), (120, 84, 28, 255)), ((30, 84, 52, 248), (8, 30, 18, 252)), 4)
    chamfer_piece("td_key_on", 160, 100, 18, ((255, 240, 200, 255), (255, 200, 120, 255)), ((255, 170, 56, 255), (200, 66, 8, 255)), 5,
                  glow=(255, 150, 40, 190))
    chamfer_piece("td_chip", 200, 80, 14, ((200, 160, 80, 255), (200, 160, 80, 255)), ((30, 40, 80, 245), (12, 14, 36, 250)), 3)
    chamfer_piece("td_chip_on", 200, 80, 14, ((255, 240, 200, 255), (255, 240, 200, 255)), ((255, 176, 60, 255), (200, 70, 8, 255)), 3,
                  glow=(255, 150, 40, 170))
    chamfer_piece("td_val", 200, 90, 12, ((8, 28, 40, 230), (4, 14, 24, 235)), ((8, 28, 40, 230), (4, 14, 24, 235)), 0)
    chamfer_piece("td_val_on", 200, 90, 12, ((120, 30, 0, 230), (80, 16, 0, 235)), ((120, 30, 0, 230), (80, 16, 0, 235)), 0)


def btn():
    w, h, s, g = 700, 120, 24, 20
    p = Pic(w + 2 * s + 2 * g, h + 2 * g)
    x0, y0, x1, y1 = g + s, g, g + s + w, g + h
    glow_poly(p, skew(x0 - 8, y0 - 8, x1 + 8, y1 + 8, s), (255, 120, 30, 150), 9)
    grad_poly(p, skew(x0, y0, x1, y1, s), (255, 246, 214, 255), (255, 200, 120, 255))
    grad_poly(p, skew(x0 + 6, y0 + 6, x1 - 6, y1 - 6, s), (255, 120, 40, 255), (190, 30, 10, 255))
    p.save("td_btn")  # ends: g + 2 s + 10


def box():
    w, h, c, g = 600, 238, 26, 14
    p = Pic(w + 2 * g, h + 2 * g)
    x0, y0, x1, y1 = g, g, g + w, g + h
    glow_poly(p, chamfer(x0, y0 + 4, x1, y1 + 6, c), (0, 0, 0, 140), 6)
    grad_poly(p, chamfer(x0, y0, x1, y1, c), (230, 196, 110, 255), (150, 104, 34, 255))
    grad_poly(p, chamfer(x0 + 6, y0 + 6, x1 - 6, y1 - 6, 22), (30, 22, 70, 244), (10, 8, 30, 248))
    outline(p, chamfer(x0 + 14, y0 + 14, x1 - 14, y1 - 14, 18), (110, 220, 255, 70), 2)
    p.save("td_box")  # corner: 14 + 40


def plate():
    w, h, c = 595, 128, 22
    p = Pic(w, h)
    grad_poly(p, chamfer(0, 0, w, h, c), (246, 226, 150, 255), (196, 160, 80, 255))
    grad_poly(p, chamfer(9, 9, w - 9, h - 9, 16), (18, 50, 32, 250), (6, 20, 12, 252))
    layer = Image.new("RGBA", p.im.size)
    d = ImageDraw.Draw(layer)
    d.ellipse(P(22, 22, 106, 106), fill=(255, 255, 255, 255))
    d.ellipse(P(28, 28, 100, 100), fill=(40, 100, 220, 255))
    p.im.alpha_composite(layer)
    gtext(p, (64, 66), "1P", F("b", 46), (255, 255, 255), (210, 230, 255), anchor="mm", stroke=2, sfill=(10, 30, 90, 255))
    p.save("td_plate")  # left end 120, right end 30


def band():
    x0, x1 = 560, 1250
    ox, oy = x0 - 90 - 30, 14 - 30  # the picture's place on the screen
    p = Pic(x1 + 50 + 30 - ox, 168 + 30 - oy)
    pts = [(x0 - 40 - ox, 14 - oy), (x1 + 50 - ox, 14 - oy), (x1 - ox, 168 - oy), (x0 - 90 - ox, 168 - oy)]
    glow_poly(p, pts, (120, 60, 255, 90), 9)
    grad_poly(p, pts, (40, 20, 96, 235), (14, 6, 40, 240))
    lines(p, [[pts[0], pts[1]], [pts[3], pts[2]]], (255, 210, 110, 255), 4)
    p.save("td_band")
    print("  band at", ox, oy)


CAPS = [(255, 140, 30), (40, 120, 255), (40, 200, 90), (230, 40, 40), (160, 70, 230), (250, 210, 30), (255, 140, 30)]


def caps():
    r = 40
    cw, ch = int(r * 2.7 + 28), int(r * 1.25 + 28)
    p = Pic(cw * 7, ch)
    for n in range(1, 8):
        col = CAPS[n - 1]
        cx, cy = cw * (n - 1) + cw / 2, ch / 2
        w, h = r * 2.7, r * 1.25
        x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
        layer = Image.new("RGBA", p.im.size)
        ImageDraw.Draw(layer).rounded_rectangle(P(x0 - 6, y0 - 6, x1 + 6, y1 + 6), radius=int((h / 2 + 6) * K), fill=col + (110,))
        p.im.alpha_composite(layer.filter(ImageFilter.GaussianBlur(3 * K)))
        layer = Image.new("RGBA", p.im.size)
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle(P(x0, y0, x1, y1), radius=int(h / 2 * K), fill=(70, 70, 80, 255))
        p.im.alpha_composite(layer)
        body = Image.new("RGBA", (int(w * K) - 4, int(h * K) - 4))
        ImageDraw.Draw(body).rounded_rectangle([0, 0, body.width - 1, body.height - 1], radius=int(h / 2 * K) - 2, fill=(236, 238, 245, 255))
        cap = Image.new("RGBA", body.size)
        ImageDraw.Draw(cap).rounded_rectangle([0, 0, body.width - 1, body.height - 1], radius=int(h / 2 * K) - 2, fill=col + (255,))
        mask = Image.new("L", body.size)
        ImageDraw.Draw(mask).rectangle([int(body.width * 0.56), 0, body.width, body.height], fill=255)
        body = Image.composite(cap, body, mask)
        sh = Image.new("RGBA", body.size)
        ImageDraw.Draw(sh).rounded_rectangle([6, 4, body.width - 7, int(body.height * 0.42)], radius=int(h / 4 * K), fill=(255, 255, 255, 110))
        body.alpha_composite(sh.filter(ImageFilter.GaussianBlur(2)))
        p.im.alpha_composite(body, (int(x0 * K) + 2, int(y0 * K) + 2))
        layer = Image.new("RGBA", p.im.size)
        d = ImageDraw.Draw(layer)
        bx = x0 + w * 0.56
        d.rectangle(P(bx - r * 0.12, y0 + 1, bx + r * 0.05, y1 - 1), fill=(60, 60, 72, 255))
        d.ellipse(P(x0 + r * 0.30, cy - r * 0.16, x0 + r * 0.62, cy + r * 0.16), fill=(50, 50, 60, 255), outline=(170, 170, 185, 255), width=K)
        p.im.alpha_composite(layer)
        gtext(p, (x0 + w * 0.33, cy + 1), str(n), F("b", r * 0.8), (60, 60, 80), (30, 30, 50), anchor="lm")
    p.save("td_caps")
    print("  capsule cell", cw, ch, "capsule", r * 2.7, r * 1.25)


def machine():
    """the static Time Machine (the screen's x 520.. / y 180..); the dial without its needle"""
    ox, oy = 520, 176
    x0, y0, x1, y1 = 560 - ox, 196 - oy, 2690 - ox, 952 - oy
    p = Pic(2712 - ox, 990 - oy)
    for cx in (x0 + 10, x1 - 10):
        glow = Image.new("RGBA", p.im.size)
        ImageDraw.Draw(glow).rounded_rectangle(P(cx - 34, y0 + 90, cx + 34, y1 - 70), radius=34 * K, fill=(80, 220, 255, 120))
        p.im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(7 * K)))
        grad_poly(p, chamfer(cx - 30, y0 + 80, cx + 30, y1 - 60, 14), (230, 200, 120, 255), (140, 100, 30, 255))
        grad_poly(p, chamfer(cx - 18, y0 + 110, cx + 18, y1 - 90, 10), (160, 240, 255, 255), (40, 140, 230, 255))
        lines(p, [[(cx - 18, yy), (cx + 18, yy)] for yy in range(int(y0 + 130), int(y1 - 100), 38)], (255, 255, 255, 150), 3)
    glow_poly(p, chamfer(x0 + 40, y0 + 20, x1 - 10, y1 + 24, 40), (0, 0, 0, 160), 8)
    grad_poly(p, chamfer(x0 + 30, y0, x1 - 30, y1, 40), (250, 232, 160, 255), (176, 132, 52, 255))
    outline(p, chamfer(x0 + 30, y0, x1 - 30, y1, 40), (90, 60, 16, 255), 4)
    hl = Image.new("RGBA", p.im.size)
    ImageDraw.Draw(hl).polygon([tuple(P(*q)) for q in chamfer(x0 + 40, y0 + 8, x1 - 40, y0 + 120, 34)], fill=(255, 255, 255, 50))
    p.im.alpha_composite(hl)
    layer = Image.new("RGBA", p.im.size)
    d = ImageDraw.Draw(layer)
    d.ellipse(P(x0 + 70, y0 + 22, x0 + 150, y0 + 102), fill=(255, 255, 255, 255))
    d.ellipse(P(x0 + 77, y0 + 29, x0 + 143, y0 + 95), fill=(40, 100, 220, 255))
    p.im.alpha_composite(layer)
    gtext(p, (x0 + 110, y0 + 63), "CC", F("b", 46), (255, 255, 255), (220, 230, 255), anchor="mm")
    lines(p, [[(x0 + 60, y0 + 124), (x1 - 60, y0 + 124)]], (120, 84, 28, 255), 4)
    layer = Image.new("RGBA", p.im.size)
    d = ImageDraw.Draw(layer)
    for x in range(int(x0 + 80), int(x1 - 60), 64):
        d.ellipse(P(x - 7, y1 - 22, x + 7, y1 - 8), fill=(240, 220, 140, 255), outline=(110, 80, 30, 255), width=2 * K)
    p.im.alpha_composite(layer)
    lx0, lx1, ly0, ly1 = x0 + 70, x0 + 1240, y0 + 146, y1 - 92
    sx0, sx1, sy0, sy1 = lx0, lx1, y1 - 74, y1 - 34
    m = Image.new("L", p.im.size)
    ImageDraw.Draw(m).rectangle(P(sx0, sy0, sx1, sy1), fill=255)
    st = Image.new("RGBA", p.im.size)
    sd = ImageDraw.Draw(st)
    sd.rectangle(P(sx0, sy0, sx1, sy1), fill=(30, 26, 20, 255))
    for x in range(int(sx0) - 60, int(sx1), 60):
        sd.polygon([tuple(P(x, sy1)), tuple(P(x + 30, sy1)), tuple(P(x + 70, sy0)), tuple(P(x + 40, sy0))], fill=(250, 200, 30, 255))
    p.im.alpha_composite(Image.composite(st, Image.new("RGBA", p.im.size), m))
    glow_poly(p, chamfer(lx0, ly0, lx1, ly1, 22), (60, 255, 120, 90), 7)
    grad_poly(p, chamfer(lx0, ly0, lx1, ly1, 22), (70, 50, 20, 255), (70, 50, 20, 255))
    grad_poly(p, chamfer(lx0 + 8, ly0 + 8, lx1 - 8, ly1 - 8, 18), (10, 30, 18, 255), (4, 14, 8, 255))
    lines(p, [[(lx0 + 16, yy), (lx1 - 16, yy)] for yy in range(int(ly0 + 14), int(ly1 - 14), 6)], (120, 255, 160, 14), 1)
    vx0, vx1, vy0, vy1 = x0 + 1290, x1 - 80, y0 + 146, y0 + 300
    grad_poly(p, chamfer(vx0, vy0, vx1, vy1, 18), (70, 50, 20, 255), (70, 50, 20, 255))
    grad_poly(p, chamfer(vx0 + 8, vy0 + 8, vx1 - 8, vy1 - 8, 14), (10, 40, 22, 255), (4, 18, 10, 255))
    ex0, ex1, ey = vx0 + 20, vx1 - 20, vy1 + 64
    grad_poly(p, chamfer(ex0, ey - 10, ex1, ey + 22, 8), (40, 30, 70, 255), (20, 16, 40, 255))
    dx, dy, dr = (vx0 + vx1) / 2, vy1 + 270, 150
    gl = Image.new("RGBA", p.im.size)
    ImageDraw.Draw(gl).ellipse(P(dx - dr - 40, dy - dr - 40, dx + dr + 40, dy + dr + 40), fill=(80, 220, 255, 90))
    p.im.alpha_composite(gl.filter(ImageFilter.GaussianBlur(9 * K)))
    dial = Image.open(OUT / "tm_dial.png").convert("RGBA")
    ds = int(dr * 2 * 1.30 * K)
    p.im.alpha_composite(dial.resize((ds, ds), Image.LANCZOS), (int(dx * K - ds / 2), int(dy * K - ds / 2)))
    layer = Image.new("RGBA", p.im.size)
    ImageDraw.Draw(layer).arc(P(dx - dr * 0.66, dy - dr * 0.66, dx + dr * 0.66, dy + dr * 0.66), 130, 410, fill=(80, 220, 255, 200), width=6 * K)
    p.im.alpha_composite(layer)
    p.save("td_machine")
    print("  machine at", ox, oy, "list", lx0 + ox, ly0 + oy, lx1 + ox, ly1 + oy, "visor", vx0 + ox, vy0 + oy, vx1 + ox, vy1 + oy,
          "dial", dx + ox, dy + oy, dr, "energy", ex0 + ox, ex1 + ox, ey + oy)


def vignette():
    w, h = 160, 72
    v = Image.new("L", (w, h), 0)
    dv = ImageDraw.Draw(v)
    for i in range(30):
        dv.rectangle([i * 2, i, w - i * 2, h - i], outline=int(150 - i * 150 / 30))
    v = v.filter(ImageFilter.GaussianBlur(4))
    im = Image.new("RGBA", (w, h), (4, 0, 20, 255))
    im.putalpha(v.point(lambda a: int(a * 0.8)))
    im.save(OUT / "td_vignette.png", optimize=True)
    print("td_vignette", w, h)


if __name__ == "__main__":
    win()
    tab()
    opt(False)
    opt(True)
    keys()
    btn()
    box()
    plate()
    band()
    caps()
    machine()
    vignette()
