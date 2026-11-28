#!/usr/bin/env python3
"""The online inner pages' extra pieces (port/tbo/ui/td_*.png) in the approved templates' look
(previas-bt4/online-bt4/online_bt4.py: navy glass, Capsule Corp gold), drawn with the previews' own functions, twice as
large and reduced, as port/tools/tbo_kit_template.py's:
  td_win_gold  a window with the templates' gold edge (Td_Nine as td_win: room 24, corners 88)
  td_card_on   the chosen card, a gold plate with its glow (Td_Nine: room 24, corners 88)
  td_cell      a light cell inside a window: a row, a number's box (Td_Nine: room 2, corners 30)
  td_tile      a white tile with a gold edge: a room code's letter (Td_Nine: room 2, corners 26)
Again with:  python3 port/tools/tbo_page_art.py  then  python3 port/tools/tbo_sizes.py"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PREV = ROOT.parents[1] / 'previas-tagteam'  # (beside the repository)
sys.path.insert(0, str(PREV))
sys.path.insert(0, str(PREV / 'nova-tela'))
import tagteam_menus as K  # noqa: E402
import nova_tela as NT  # noqa: E402
from PIL import Image  # noqa: E402

OUT = ROOT / "port/tbo/ui"
S = K.S
NAVY, NAVY2, GOLD, GOLD2 = NT.NAVY, NT.NAVY2, NT.GOLD, NT.GOLD2


def piece(name, w, h, draw):
    p = K.Pic((0, 0, 0, 0))
    draw(p, w, h)
    im = p.im.crop((0, 0, w * S, h * S)).resize((w, h), Image.LANCZOS)
    im.save(OUT / (name + ".png"), optimize=True)
    print(name, im.size)


def glass_gold(p, box, r, a=215):
    K.shadow(p, box, r, (0, 6), 8, 110)
    K.rr_grad(p, box, r, [(0, NAVY2 + (a,)), (1, NAVY + (min(255, a + 20),))])
    x0, y0, x1, y1 = box
    K.rr_grad(p, (x0 + 4, y0 + 4, x1 - 4, y0 + (y1 - y0) * 0.38), max(r - 4, 2), [(0, (255, 255, 255, 34)), (1, (255, 255, 255, 0))])
    layer = p.layer()
    p.d(layer).rounded_rectangle(K.P(*box), r * S, outline=GOLD + (200,), width=int(3 * S))
    p.put(layer)


def gold_card(p, box, r):
    K.glow_rr(p, (box[0] - 10, box[1] - 10, box[2] + 10, box[3] + 10), r + 8, (255, 220, 110, 170), 12)
    K.rr_grad(p, box, r, [(0, (255, 240, 160, 255)), (0.5, GOLD + (255,)), (1, GOLD2 + (255,))])
    x0, y0, x1, y1 = box
    K.rr_grad(p, (x0 + 5, y0 + 5, x1 - 5, y0 + (y1 - y0) * 0.42), max(r - 5, 2), [(0, (255, 255, 255, 110)), (1, (255, 255, 255, 0))])
    layer = p.layer()
    p.d(layer).rounded_rectangle(K.P(*box), r * S, outline=(255, 252, 230, 255), width=int(4 * S))
    p.put(layer)


piece("td_win_gold", 448, 348, lambda p, w, h: glass_gold(p, (24, 24, w - 24, h - 24), 34))
piece("td_card_on", 448, 348, lambda p, w, h: gold_card(p, (24, 24, w - 24, h - 24), 36))
piece("td_cell", 204, 124, lambda p, w, h: K.rr_grad(p, (2, 2, w - 2, h - 2), 24, [(0, (255, 255, 255, 34)), (1, (255, 255, 255, 12))],
                                                    (255, 255, 255, 60), 2))
piece("td_tile", 164, 164, lambda p, w, h: K.rr_grad(p, (2, 2, w - 2, h - 2), 20, [(0, (255, 255, 255, 235)), (1, (225, 232, 245, 235))],
                                                    GOLD + (255,), 2))
