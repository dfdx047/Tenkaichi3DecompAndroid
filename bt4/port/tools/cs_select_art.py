#!/usr/bin/env python3
"""The pieces of BT4's new character select (port/tbo/ui/cs_*.png), drawn as in its approved template (since 20/11/2026
previas-bt4/selecao-nova/selecao.py: indigo glass with a lavender edge, the chosen one violet lined in gold, the
buttons violet and orange; before it "Instinto Prateado", graphite and silver, which the user found black and
dull). 2712 x 1220: one pixel of these pictures is one pixel of that screen. Again with:
python3 port/tools/cs_select_art.py  (then port/tools/tbo_sizes.py)

Each piece is a shape in the middle of its picture with room round it (PAD) for its shadow or glow; the game stretches
it over a box keeping its corners (src/menu/port_charsel.c Cw_Nine, as the online screens' Td_Nine):
  cs_panel    a panel: indigo glass going darker down, lit at the top, a lavender edge, a soft shadow (corners 26)
  cs_lit      the chosen one: violet going deeper down, lined in gold, a gold glow round it (corners 26)
  cs_plain    a plain place (an empty slot, a row): indigo glass, a lavender edge (corners 26)
  cs_card     a card of the roster: light going dark, a faint edge, tinted by the game per fighter (corners 22)
  cs_pill     a button: violet, lit at the top, lined in white (100 tall, round ends), with its shadow
  cs_pill_on  the main button lit (View Team): orange, lined in white, an orange glow
Only our own drawing; no words in them."""
import pathlib
from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/tbo/ui"
K = 3  # drawn three times as large, then reduced: smooth edges


def vgrad(w, h, c0, c1):
    g = Image.new("RGBA", (1, 256))
    for i in range(256):
        t = i / 255.0
        g.putpixel((0, i), tuple(int(round(c0[k] * (1 - t) + c1[k] * t)) for k in range(4)))
    return g.resize((w, h), Image.BILINEAR)


def rrect_mask(size, box, r):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle(box, r, fill=255)
    return m


def layer(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def piece(name, inner_w, inner_h, pad, r, draw):
    W, H = (inner_w + 2 * pad) * K, (inner_h + 2 * pad) * K
    im = layer((W, H))
    box = (pad * K, pad * K, (pad + inner_w) * K - 1, (pad + inner_h) * K - 1)
    draw(im, box, r * K)
    im = im.resize((inner_w + 2 * pad, inner_h + 2 * pad), Image.LANCZOS)
    im.save(OUT / f"{name}.png", optimize=True)
    print(name, im.size, "pad", pad, "corner", pad + r)


def shadow(im, box, r, off, blur, alpha):
    l = layer(im.size)
    b = (box[0] + off[0] * K, box[1] + off[1] * K, box[2] + off[0] * K, box[3] + off[1] * K)
    ImageDraw.Draw(l).rounded_rectangle(b, r, fill=(0, 0, 0, alpha))
    im.alpha_composite(l.filter(ImageFilter.GaussianBlur(blur * K)))


def fill_grad(im, box, r, c0, c1):
    x0, y0, x1, y1 = box
    g = vgrad(x1 - x0 + 1, y1 - y0 + 1, c0, c1)
    l = layer(im.size)
    l.paste(g, (x0, y0), rrect_mask((x1 - x0 + 1, y1 - y0 + 1), (0, 0, x1 - x0, y1 - y0), r))
    im.alpha_composite(l)


def shine(im, box, r, part, alpha):
    """the light at the top: a fading band, clipped to the shape (a little inside its edge)"""
    x0, y0, x1, y1 = box
    inset = 4 * K
    h = int((y1 - y0) * part)
    band = vgrad(x1 - x0 + 1, h, (255, 255, 255, alpha), (255, 255, 255, 0))
    l = layer(im.size)
    l.paste(band, (x0, y0))
    m = Image.new("L", im.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((x0 + inset, y0 + inset, x1 - inset, y1 - inset), max(r - inset, 1), fill=255)
    a = l.getchannel("A")
    l.putalpha(Image.fromarray(__import__("numpy").minimum(__import__("numpy").array(a), __import__("numpy").array(m))))
    im.alpha_composite(l)


def edge(im, box, r, color, width):
    l = layer(im.size)
    ImageDraw.Draw(l).rounded_rectangle(box, r, outline=color, width=int(width * K))
    im.alpha_composite(l)


def glow(im, box, r, color, size):
    x0, y0, x1, y1 = box
    g = layer(im.size)
    ImageDraw.Draw(g).rounded_rectangle((x0 - size * K // 2, y0 - size * K // 2, x1 + size * K // 2, y1 + size * K // 2), r + size * K // 2,
                                        outline=color, width=size * K // 2)
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(size * K * 0.6)))


def glass(im, box, r):  # (the template's panel())
    shadow(im, box, r, (6, 12), 14, 120)
    fill_grad(im, box, r, (28, 22, 64, 215), (10, 8, 30, 230))
    shine(im, box, r, 0.42, 34)
    edge(im, box, r, (150, 170, 255, 120), 3)


def lit(im, box, r):  # (the template's chosen cell, gold lined with its glow, over the chosen row's violet)
    glow(im, box, r, (255, 196, 64, 200), 18)
    fill_grad(im, box, r, (126, 74, 255, 245), (70, 36, 190, 245))
    shine(im, box, r, 0.45, 60)
    edge(im, box, r, (255, 196, 64, 255), 5)


def plain(im, box, r):  # (the template's empty team slot)
    fill_grad(im, box, r, (40, 36, 90, 200), (14, 12, 34, 220))
    edge(im, box, r, (170, 180, 255, 150), 3)


def card(im, box, r):  # (the template's portrait cell: its colour per fighter is the game's tint over this)
    shadow(im, box, r, (3, 5), 4, 90)
    fill_grad(im, box, r, (232, 232, 244, 250), (40, 36, 80, 250))
    edge(im, box, r, (210, 216, 255, 120), 2)


def button(im, box, r, top, bot, glow_color):  # (the template's big_button())
    if glow_color:
        glow(im, box, r, glow_color, 24)
    shadow(im, box, r, (4, 10), 10, 140)
    fill_grad(im, box, r, top, bot)
    shine(im, box, r, 0.48, 110)
    edge(im, box, r, (255, 255, 255, 230), 4)


def main():
    piece("cs_panel", 200, 200, 52, 26, glass)
    piece("cs_lit", 200, 200, 40, 26, lit)
    piece("cs_plain", 120, 120, 4, 26, plain)
    piece("cs_card", 120, 150, 12, 22, card)
    piece("cs_pill", 300, 100, 52, 50, lambda im, b, r: button(im, b, r, (112, 90, 255, 255), (52, 30, 160, 255), None))
    piece("cs_pill_on", 300, 100, 40, 50, lambda im, b, r: button(im, b, r, (255, 214, 90, 255), (230, 120, 20, 255), (255, 170, 40, 200)))


if __name__ == "__main__":
    main()
