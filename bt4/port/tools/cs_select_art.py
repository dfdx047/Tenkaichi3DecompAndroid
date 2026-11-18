#!/usr/bin/env python3
"""The pieces of BT4's new character select (port/tbo/ui/cs_*.png), drawn as in its approved template, "Instinto
Prateado" (previas-bt4/redesign-geral: 2712 x 1220, one pixel of these pictures is one pixel of that screen). Again
with:  python3 port/tools/cs_select_art.py  (then port/tools/tbo_sizes.py)

Each piece is a shape in the middle of its picture with room round it (PAD) for its shadow or glow; the game stretches
it over a box keeping its corners (src/menu/port_charsel.c Cw_Nine, as the online screens' Td_Nine):
  cs_panel    a panel: graphite glass going darker down, lit at the top, a thin silver edge, a soft shadow (corners 16)
  cs_lit      the chosen one: silver going down to lilac, lined in white, a lilac glow round it (corners 26)
  cs_plain    a plain row: a faint light glass, a thin edge (corners 26)
  cs_card     a card of the roster: dark, a thin edge (corners 22)
  cs_pill     a button's pill of glass (100 tall, round ends), with its shadow
  cs_pill_on  the pill lit (View Team)
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


def glass(im, box, r):
    shadow(im, box, r, (10, 16), 16, 110)
    fill_grad(im, box, r, (46, 46, 58, 230), (14, 14, 20, 240))
    shine(im, box, r, 0.42, 30)
    edge(im, box, r, (220, 225, 240, 150), 3)


def lit(im, box, r):
    x0, y0, x1, y1 = box
    g = layer(im.size)
    ImageDraw.Draw(g).rounded_rectangle((x0 - 10 * K, y0 - 10 * K, x1 + 10 * K, y1 + 10 * K), r + 8 * K, fill=(200, 200, 255, 150))
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(12 * K)))
    fill_grad(im, box, r, (240, 244, 255, 255), (150, 140, 220, 255))
    shine(im, box, r, 0.45, 70)
    edge(im, box, r, (255, 255, 255, 255), 5)


def plain(im, box, r):
    fill_grad(im, box, r, (255, 255, 255, 22), (255, 255, 255, 14))
    edge(im, box, r, (200, 205, 230, 90), 2)


def card(im, box, r):
    shadow(im, box, r, (3, 5), 4, 90)
    fill_grad(im, box, r, (70, 72, 88, 245), (28, 28, 38, 245))
    edge(im, box, r, (200, 205, 230, 90), 3)


def main():
    piece("cs_panel", 200, 200, 52, 16, glass)
    piece("cs_lit", 200, 200, 40, 26, lit)
    piece("cs_plain", 120, 120, 4, 26, plain)
    piece("cs_card", 120, 150, 12, 22, card)
    piece("cs_pill", 300, 100, 52, 50, glass)
    piece("cs_pill_on", 300, 100, 40, 50, lit)


if __name__ == "__main__":
    main()
