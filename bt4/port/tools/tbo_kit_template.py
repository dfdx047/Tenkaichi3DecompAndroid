#!/usr/bin/env python3
"""The online screens' kit (port/tbo/ui/td_*.png) in the approved template's look (previas-bt4/online-bt4: navy glass,
Capsule Corp gold, the time machine's meadow behind), each piece the same size, room round it and corners as the one it
replaces (src/menu/tbo_look.inc stretches them: Td_Nine / Td_Three with those numbers), so every page takes the new
look without its layout changing. Drawn with the previews' own functions, twice as large and reduced. Also hub_page:
the pages' backdrop (the meadow dimmed and softened, as the previews' inner screens). Again with:
    python3 port/tools/tbo_kit_template.py   (the sizes do not change: tbo_sizes.py only for hub_page if it is new)"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PREV = ROOT.parents[1] / 'previas-tagteam'  # (beside the repository)
sys.path.insert(0, str(PREV))
sys.path.insert(0, str(PREV / 'nova-tela'))
import tagteam_menus as K  # noqa: E402
import nova_tela as NT  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

OUT = ROOT / "port/tbo/ui"
S = K.S
NAVY, NAVY2, GOLD, GOLD2 = NT.NAVY, NT.NAVY2, NT.GOLD, NT.GOLD2


def piece(name, w, h, draw):
    p = K.Pic((0, 0, 0, 0))
    draw(p, w, h)
    im = p.im.crop((0, 0, w * S, h * S)).resize((w, h), Image.LANCZOS)
    im.save(OUT / (name + ".png"), optimize=True)
    print(name, im.size)


def glass_rr(p, box, r, a=215, edge=(190, 220, 255, 110), shadow=True):
    if shadow:
        K.shadow(p, box, r, (0, 6), 8, 110)
    K.rr_grad(p, box, r, [(0, NAVY2 + (a,)), (1, NAVY + (min(255, a + 20),))])
    x0, y0, x1, y1 = box
    K.rr_grad(p, (x0 + 3, y0 + 3, x1 - 3, y0 + (y1 - y0) * 0.38), max(r - 3, 2), [(0, (255, 255, 255, 30)), (1, (255, 255, 255, 0))])
    l = p.layer(); p.d(l).rounded_rectangle(K.P(*box), r * S, outline=edge, width=int(2.5 * S)); p.put(l)


def gold_rr(p, box, r, glow=True):
    if glow:
        K.glow_rr(p, (box[0] - 8, box[1] - 8, box[2] + 8, box[3] + 8), r + 6, (255, 220, 110, 160), 10)
    K.rr_grad(p, box, r, [(0, (255, 240, 160, 255)), (0.5, GOLD + (255,)), (1, GOLD2 + (255,))])
    x0, y0, x1, y1 = box
    K.rr_grad(p, (x0 + 4, y0 + 4, x1 - 4, y0 + (y1 - y0) * 0.42), max(r - 4, 2), [(0, (255, 255, 255, 100)), (1, (255, 255, 255, 0))])
    l = p.layer(); p.d(l).rounded_rectangle(K.P(*box), r * S, outline=(255, 252, 230, 255), width=int(3 * S)); p.put(l)


def poly_gold(p, pts, glow=False):
    if glow:
        K.glow_poly(p, pts, (255, 220, 110, 150), 10)
    K.poly_grad(p, pts, [(0, (255, 240, 160, 255)), (0.5, GOLD + (255,)), (1, GOLD2 + (255,))], (255, 252, 230, 255), 3)


# a window (Td_Nine: room 24, corners 88): navy glass, corners 30
piece("td_win", 448, 348, lambda p, w, h: glass_rr(p, (24, 24, w - 24, h - 24), 30, 205, (150, 180, 255, 90)))
# a window's / the page's title tab (Td_Three: caps 106 | 46, 112 tall): the gold tab with its slanted end
piece("td_tab", 432, 112, lambda p, w, h: (K.shadow(p, (42, 12, w - 20, h - 10), 10, (4, 6), 6, 120),
                                          poly_gold(p, [(42, 10), (w - 10, 10), (w - 44, h - 10), (42, h - 10)])))
# an option (Td_Three: caps 48 = room 6 + 42) and the chosen one (caps 60 = room 18 + 42, its glow)
piece("td_opt", 644, 138, lambda p, w, h: glass_rr(p, (6, 6, w - 6, h - 6), 42, 200, (190, 220, 255, 100), False))
piece("td_opt_on", 668, 162, lambda p, w, h: gold_rr(p, (18, 18, w - 18, h - 18), 42))
# a value's box (Td_Nine: room 2, corners 14): only a faint glass now (the value is between arrows)
piece("td_val", 204, 94, lambda p, w, h: K.rr_grad(p, (2, 2, w - 2, h - 2), 12, [(0, (255, 255, 255, 18)), (1, (255, 255, 255, 8))]))
piece("td_val_on", 204, 94, lambda p, w, h: K.rr_grad(p, (2, 2, w - 2, h - 2), 12, [(0, NAVY2 + (60,)), (1, NAVY + (40,))]))
# a key (Td_Nine: room 2, corners 24) and the one under the cursor (room 14, corners 36): glass, gold
piece("td_key", 164, 104, lambda p, w, h: glass_rr(p, (2, 2, w - 2, h - 2), 16, 225, (190, 220, 255, 120), False))
piece("td_key_on", 188, 128, lambda p, w, h: gold_rr(p, (14, 14, w - 14, h - 14), 16))
# a chip (room 2, corners 20) and lit (room 14, corners 32)
piece("td_chip", 204, 84, lambda p, w, h: K.rr_grad(p, (2, 2, w - 2, h - 2), 18, [(0, (255, 255, 255, 60)), (1, (255, 255, 255, 30))], (255, 255, 255, 110), 2))
piece("td_chip_on", 228, 108, lambda p, w, h: gold_rr(p, (14, 14, w - 14, h - 14), 18))
# the big button (Td_Three: caps 78 = room 20 + 48 + the slant 10; the shape slanted 24 at its 120 of height)
piece("td_btn", 788, 160, lambda p, w, h: poly_gold(p, [(68, 20), (w - 20, 20), (w - 68, h - 20), (20, h - 20)], True))
# the foot's box (Td_Nine: room 14, corners 54): dark glass, soft
piece("td_box", 628, 266, lambda p, w, h: glass_rr(p, (14, 14, w - 14, h - 14), 40, 170, (150, 180, 255, 70), False))
# the player's plate at the top right (drawn whole)
piece("td_plate", 595, 128, lambda p, w, h: glass_rr(p, (8, 8, w - 8, h - 8), 30, 210, (255, 204, 64, 160), False))
# the band behind the old title (no longer drawn by the new top bar): clear
piece("td_band", 890, 214, lambda p, w, h: None)

# the pages' backdrop: the meadow dimmed and softened, its dust, the shade under the left
bg = NT.background(0.25, 6)
NT.dust(bg, 0.6, 9, 14)
NT.left_shade(bg, 2000, 150)
bg.im.resize((K.W, K.H), Image.LANCZOS).convert("RGB").save(OUT / "hub_page.png", optimize=True)
print("hub_page", (K.W, K.H))
