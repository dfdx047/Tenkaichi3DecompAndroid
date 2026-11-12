#!/usr/bin/env python3
"""The Time Machine look of the Tenkaichi Budokai Online screens (port/tbo/ui), drawn with PIL so it can be made
again:  python3 port/tools/tbo_time_machine.py

The hub's machine:
  tm_panel.png   its gold body, a box whose corners keep their shape (Ps_Panel): the header's rule, rivets, the
                 Capsule Corp. seal and "HOPE!!" are drawn by the screen (src/menu/port_tbo.c), so they keep their shape
  tm_screen.png  the green screen the modes are listed on (a box too: scan lines, its glow round it)
  tm_dial.png    the destination dial without its needle (the screen draws the needle, it turns)
  tm_stamp.png   "DESTINATION LOCKED!", stamped when a mode is chosen
  tm_name.png    the player's name plate at the top (a box: gold frame, dark green screen)
  tm_key.png, tm_key_on.png  a key of the keypads, and the one under the cursor (boxes)
The kit every page is built of, in the same look (same sizes and corners as before, so the pages lay out as they did):
  kit_bar.png, kit_bar_compact.png    a row: a dark green screen capsule in a gold rim
  kit_sel.png, kit_sel_compact.png    the row under the cursor: glowing orange
  kit_window.png                      a window: a gold frame round a dark screen
  kit_dialog.png                      the hosts' box at the foot of the page (and the spectators' line)
  kit_gold.png, kit_steel.png         the gold and the grey capsule

The words the player reads are not in the pictures: the screens write them, so the font settings apply. A picture of
another size needs  python3 port/tools/tbo_sizes.py  (the kit's sizes are unchanged).
"""
import math
import pathlib
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/tbo/ui"
FONT = ROOT / "port/third_party/fonts"
SS = 3  # drawn this many times larger, then reduced: smooth edges

GOLD_TOP, GOLD_BOTTOM, GOLD_EDGE = (246, 226, 150), (196, 160, 80), (90, 66, 24)


def anton(size):
    return ImageFont.truetype(str(FONT / "anton/Anton-Regular.ttf"), int(size))


def grad(size, top, bottom):
    """a vertical gradient (RGBA tuples) of `size`"""
    w, h = size
    col = Image.new("RGBA", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        col.putpixel((0, y), tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(4)))
    return col.resize((w, h), Image.NEAREST)


def rrect_mask(size, box, radius):
    m = Image.new("L", size)
    ImageDraw.Draw(m).rounded_rectangle(box, radius=radius, fill=255)
    return m


def fill(im, box, radius, top, bottom):
    """fills a rounded box of `im` with a gradient from `top` to `bottom`"""
    layer = Image.new("RGBA", im.size)
    x0, y0, x1, y1 = [int(v) for v in box]
    layer.paste(grad((x1 - x0 + 1, y1 - y0 + 1), top, bottom), (x0, y0))
    clear = Image.new("RGBA", im.size)
    im.alpha_composite(Image.composite(layer, clear, rrect_mask(im.size, box, radius)))


def glow(im, box, radius, colour, blur):
    g = Image.new("RGBA", im.size)
    ImageDraw.Draw(g).rounded_rectangle(box, radius=radius, fill=colour)
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(blur)))


def save(im, name):
    w, h = im.size
    im.resize((w // SS, h // SS), Image.LANCZOS).save(OUT / f"{name}.png", optimize=True)


def capsule(name, w, h, outer, top, bottom, shine, pad=0):
    """a capsule as wide as the picture, its ends half circles (the pill's corner is half its height)"""
    W, H = w * SS, h * SS
    im = Image.new("RGBA", (W, H))
    p = pad * SS
    r = (H - 2 * p) // 2
    fill(im, (p, p, W - 1 - p, H - 1 - p), r, outer, outer)
    e = int(5 * SS * h / 100)
    fill(im, (p + e, p + e, W - 1 - p - e, H - 1 - p - e), r - e, top, bottom)
    s = Image.new("RGBA", (W, H))
    ImageDraw.Draw(s).rounded_rectangle((p + e + 3 * SS, p + e + 2 * SS, W - 1 - p - e - 3 * SS, p + e + (H - 2 * p) * 0.34),
                                        radius=r // 2, fill=shine)
    im.alpha_composite(s.filter(ImageFilter.GaussianBlur(SS * 2)))
    save(im, name)


def framed_box(name, w, h, radius, rim_top, rim_bottom, edge, inside_top, inside_bottom, rim, line=None, brackets=None):
    """a box: a rim (`rim` pixels of gradient) round a gradient inside; `line`: a thin light line inside the rim"""
    W, H, R = w * SS, h * SS, radius * SS
    im = Image.new("RGBA", (W, H))
    fill(im, (0, 0, W - 1, H - 1), R, edge, edge)
    k = 2 * SS
    fill(im, (k, k, W - 1 - k, H - 1 - k), R - k, rim_top, rim_bottom)
    q = (rim + 2) * SS
    fill(im, (q, q, W - 1 - q, H - 1 - q), max(4, R - q), edge, edge)
    q2 = q + SS
    fill(im, (q2, q2, W - 1 - q2, H - 1 - q2), max(4, R - q2), inside_top, inside_bottom)
    over = Image.new("RGBA", (W, H))  # (lines on a layer of their own: drawn straight on the picture they would replace its pixels)
    d = ImageDraw.Draw(over)
    if line is not None:
        q3 = q2 + 4 * SS
        d.rounded_rectangle((q3, q3, W - 1 - q3, H - 1 - q3), radius=max(4, R - q3), outline=line, width=SS)
    if brackets is not None:  # small gold corner brackets, inside the corners (which keep their shape)
        a, t, o = 22 * SS, 3 * SS, q2 + 6 * SS
        for cx, sx in ((o, 1), (W - 1 - o, -1)):
            for cy, sy in ((o, 1), (H - 1 - o, -1)):
                d.line([(cx, cy + sy * a), (cx, cy), (cx + sx * a, cy)], fill=brackets, width=t, joint="curve")
    im.alpha_composite(over)
    # a light along the top of the rim
    s = Image.new("RGBA", (W, H))
    ImageDraw.Draw(s).rounded_rectangle((k + SS, k + SS, W - 1 - k - SS, k + rim * SS * 0.6), radius=R // 2, fill=(255, 255, 255, 70))
    im.alpha_composite(s.filter(ImageFilter.GaussianBlur(SS)))
    return im


def panel():
    """the machine's body: a gold box (the screen draws its rule, rivets, seal and words)"""
    im = framed_box("tm_panel", 600, 260, 46, GOLD_TOP + (255,), GOLD_BOTTOM + (255,), GOLD_EDGE + (255,), GOLD_TOP + (255,),
                    GOLD_BOTTOM + (255,), 6)
    W, H = im.size
    hl = Image.new("RGBA", (W, H))
    ImageDraw.Draw(hl).rectangle([0, 0, W, H * 0.40], fill=(255, 255, 255, 34))
    im.alpha_composite(Image.composite(hl, Image.new("RGBA", (W, H)), rrect_mask((W, H), (0, 0, W - 1, H - 1), 46 * SS)))
    save(im, "tm_panel")


def screen():
    pad = 24
    w, h = 880, 268
    W, H = (w + 2 * pad) * SS, (h + 2 * pad) * SS
    im = Image.new("RGBA", (W, H))
    p = pad * SS
    glow(im, (p, p, W - p, H - p), 30 * SS, (60, 255, 120, 110), 16 * SS)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((p, p, W - p, H - p), radius=30 * SS, fill=(8, 22, 14, 255), outline=(70, 50, 20, 255), width=8 * SS)
    lines = Image.new("RGBA", (W, H))  # (the scan lines over the glass, blended: drawn straight they made it see-through)
    ld = ImageDraw.Draw(lines)
    for y in range(p + 12 * SS, H - p - 12 * SS, 5 * SS):
        ld.line([(p + 12 * SS, y), (W - p - 12 * SS, y)], fill=(120, 255, 160, 18), width=SS)
    im.alpha_composite(lines)
    save(im, "tm_screen")


def dial():
    R = 196 * 0.8 * SS
    W = int((R + 80 * 0.8 * SS) * 2)
    c = W / 2
    im = Image.new("RGBA", (W, W))
    g = Image.new("RGBA", (W, W))
    k = 36 * 0.8 * SS
    ImageDraw.Draw(g).ellipse([c - R - k, c - R - k, c + R + k, c + R + k], fill=(255, 190, 80, 110))
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(22 * 0.8 * SS)))
    d = ImageDraw.Draw(im)
    d.ellipse([c - R, c - R, c + R, c + R], fill=(150, 115, 50, 255), outline=(80, 56, 20, 255), width=int(8 * 0.8 * SS))
    r2 = R * 172 / 196
    d.ellipse([c - r2, c - r2, c + r2, c + r2], fill=(30, 30, 44, 255), outline=(220, 190, 110, 255), width=int(6 * 0.8 * SS))
    for a in range(0, 360, 15):
        ar = math.radians(a)
        big = a % 45 == 0
        r0 = R * (132 if big else 148) / 196
        r1 = R * 164 / 196
        d.line([(c + math.cos(ar) * r0, c + math.sin(ar) * r0), (c + math.cos(ar) * r1, c + math.sin(ar) * r1)], fill=(255, 220, 130, 255),
               width=int((6 if big else 3) * 0.8 * SS))
    save(im, "tm_dial")


def stamp():
    w, h = 608 * SS, 136 * SS
    im = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([5 * SS, 5 * SS, w - 5 * SS, h - 5 * SS], radius=19 * SS, outline=(230, 40, 30, 255), width=10 * SS)
    d.text((w / 2, h / 2 + 2 * SS), "DESTINATION LOCKED!", font=anton(62 * SS), fill=(230, 40, 30, 255), anchor="mm")
    im = im.rotate(8, expand=True, resample=Image.BICUBIC)
    save(im, "tm_stamp")


def name_plate():
    im = framed_box("tm_name", 600, 140, 40, GOLD_TOP + (255,), GOLD_BOTTOM + (255,), GOLD_EDGE + (255,), (16, 46, 28, 245),
                    (6, 20, 12, 245), 9, line=(110, 255, 150, 60))
    save(im, "tm_name")


def window():
    im = framed_box("kit_window", 465, 224, 34, GOLD_TOP + (255,), GOLD_BOTTOM + (255,), GOLD_EDGE + (255,), (14, 34, 24, 238),
                    (6, 16, 12, 242), 7, line=(110, 255, 150, 40))
    save(im, "kit_window")


def dialog():
    im = framed_box("kit_dialog", 1273, 204, 44, GOLD_TOP + (255,), GOLD_BOTTOM + (255,), (60, 40, 12, 255), (26, 20, 60, 242),
                    (10, 8, 30, 246), 5, line=(150, 210, 255, 110), brackets=(255, 210, 120, 200))
    save(im, "kit_dialog")


def kit():
    green = dict(outer=(200, 165, 85, 255), top=(26, 74, 46, 248), bottom=(8, 30, 18, 250), shine=(140, 255, 180, 34))
    orange = dict(outer=(255, 238, 196, 255), top=(246, 130, 36, 255), bottom=(176, 58, 6, 255), shine=(255, 236, 190, 90))
    capsule("kit_bar", 1273, 100, **green)
    capsule("kit_bar_compact", 160, 100, **green)
    capsule("kit_sel", 1280, 107, **orange)
    capsule("kit_sel_compact", 160, 107, **orange)
    # (the gold capsule a shade deeper than the machine: the gold letters drawn on it keep their contrast)
    capsule("kit_gold", 306, 81, (255, 226, 150, 255), (196, 120, 28, 255), (122, 64, 10, 255), (255, 236, 180, 70))
    capsule("kit_steel", 306, 75, (40, 52, 46, 255), (128, 150, 140, 255), (64, 80, 74, 255), (255, 255, 255, 50))


def keys():
    """a key of the keypads (a box: its corners keep their shape) and the one under the cursor, glowing"""
    im = framed_box("tm_key", 120, 120, 26, GOLD_TOP + (255,), GOLD_BOTTOM + (255,), GOLD_EDGE + (255,), (30, 84, 52, 250), (8, 30, 18, 252), 4,
                    line=(140, 255, 180, 50))
    save(im, "tm_key")
    W = 120 * SS
    im = Image.new("RGBA", (W, W))
    glow(im, (8 * SS, 8 * SS, W - 8 * SS, W - 8 * SS), 26 * SS, (255, 170, 60, 200), 6 * SS)
    k = framed_box("tm_key_on", 104, 104, 22, (255, 246, 214, 255), (255, 214, 140, 255), (120, 50, 0, 255), (250, 140, 40, 255), (186, 64, 6, 255), 4,
                   line=(255, 240, 200, 120))
    im.alpha_composite(k, (8 * SS, 8 * SS))
    save(im, "tm_key_on")


if __name__ == "__main__":
    keys()
    panel()
    screen()
    dial()
    stamp()
    name_plate()
    window()
    dialog()
    kit()
    for n in ("tm_key", "tm_key_on", "tm_panel", "tm_screen", "tm_dial", "tm_stamp", "tm_name", "kit_window", "kit_dialog", "kit_bar", "kit_bar_compact", "kit_sel",
              "kit_sel_compact", "kit_gold", "kit_steel"):
        print(n, Image.open(OUT / f"{n}.png").size)
