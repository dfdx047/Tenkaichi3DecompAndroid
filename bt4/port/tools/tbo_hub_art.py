#!/usr/bin/env python3
"""The pictures of the BT4 Online main screen in its approved look (previas-bt4/online-bt4: 01-principal, "the Tag
Team's new screen" look: the time machine on the meadow, Capsule Corp yellow and navy), drawn with the previews' own
functions (previas-tagteam: tagteam_menus.py, nova-tela/nova_tela.py) so they are the previews' pixels, 2712 x 1220
like the online screens (one pixel of these pictures is one pixel of that screen), drawn twice as large and reduced.
Again with:  python3 port/tools/tbo_hub_art.py  (then port/tools/tbo_sizes.py)

  hub_bg       the background: the user's photo of the time machine on the meadow (nova_tela.BG), its dust
  hub_row      a mode's row: navy glass, 112 tall with round ends, its shadow (Td_Three, the caps 90 each)
  hub_row_on   the chosen row: the gold plate with its glow
  hub_icons    the modes' badges, 112 x 112 each: a row unlit, a row lit, in the order of port_tbo.c's HUB_*
  hub_logo     the BT4 ONLINE seal and the line under it
  hub_chip     a chip of the chosen mode's facts (glass, 56 tall, Td_Three, caps 28); its words are the game's
  trunks.png   the first picture of trunks.mpg (port/tools/tbo_trunks_film.py), what is shown without the film
The letters of rows and chips are the game's (Bangers / Chakra Petch), so they follow the chosen language."""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PREV = pathlib.Path('/home/lawliet/Projetos-Recomp/previas-tagteam')
sys.path.insert(0, str(PREV))
sys.path.insert(0, str(PREV / 'nova-tela'))
import tagteam_menus as K  # noqa: E402
import nova_tela as NT  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

OUT = ROOT / "port/tbo/ui"
S = K.S
NAVY, NAVY2, GOLD = NT.NAVY, NT.NAVY2, NT.GOLD


def crop(pic, box):
    x0, y0, x1, y1 = box
    return pic.im.crop((x0 * S, y0 * S, x1 * S, y1 * S)).resize((x1 - x0, y1 - y0), Image.LANCZOS)


def clear():
    return K.Pic((0, 0, 0, 0))


def save(im, name):
    im.save(OUT / (name + ".png"), optimize=True)
    print(name, im.size)


# the background
bg = NT.background()
NT.dust(bg, 1.3)
save(bg.im.resize((K.W, K.H), Image.LANCZOS).convert("RGB"), "hub_bg")

# a row and the chosen row: 112 tall, 600 wide, 40 of room round them for the shadow / glow
PAD = 40
p = clear()
NT.glass(p, (PAD, PAD, PAD + 600, PAD + 112), 56, 205)
save(crop(p, (0, 0, 600 + 2 * PAD, 112 + 2 * PAD)), "hub_row")
p = clear()
NT.gold_plate(p, (PAD, PAD, PAD + 600, PAD + 112), 56)
save(crop(p, (0, 0, 600 + 2 * PAD, 112 + 2 * PAD)), "hub_row_on")

# the badges: Quick Match, Ranked, Create Match, Join, Tag Team, Tournament, Profile
KINDS = ["ffa", "cup", "team", "online", "TT", "gauntlet", "person"]
p = clear()
for lit in (0, 1):
    for i, k in enumerate(KINDS):
        cx, cy, r = 56 + i * 112, 56 + lit * 112, 56 - (9 if lit else 11)
        if k in ("TT", "person"):
            NT.disc(p, cx, cy, r, "team", lit)                 # (the badge, its icon drawn over below)
            l = p.layer()
            ImageDraw.Draw(l).ellipse(K.P(cx - r + 3, cy - r + 3, cx + r - 3, cy + r - 3), fill=(NAVY2 if lit else (40, 56, 100)) + (255,))
            p.put(l)
            c = GOLD + (255,) if lit else (220, 232, 255, 255)
            if k == "TT":
                K.text(p, (cx, cy + 2), "TT", "kanit", r * 0.95, c, "mm")
            else:                                              # one person: a head and shoulders
                l = p.layer(); d = ImageDraw.Draw(l)
                d.ellipse(K.P(cx - r * 0.22, cy - r * 0.52, cx + r * 0.22, cy - r * 0.08), fill=c)
                d.pieslice(K.P(cx - r * 0.46, cy - r * 0.02, cx + r * 0.46, cy + r * 0.86), 180, 360, fill=c)
                p.put(l)
        else:
            NT.disc(p, cx, cy, r, k, lit)
save(crop(p, (0, 0, 112 * len(KINDS), 224)), "hub_icons")

# the seal and its line
p = clear()
NT_logo = sys.modules["__main__"]
x, y, w, h = 20, 20, 640, 120
K.shadow(p, (x, y, x + w, y + h), h / 2, (6, 10), 12, 140)
NT.gold_plate(p, (x, y, x + w, y + h), h / 2, glow=False)
K.rr_grad(p, (x + 10, y + 10, x + 130, y + h - 10), (h - 20) / 2, [(0, NAVY2 + (255,)), (1, NAVY + (255,))])
K.text(p, (x + 70, y + h / 2), "BT4", "kanit", 44, GOLD + (255,), "mm")
K.text(p, (x + 150, y + h / 2 + 2), "ONLINE", "kanit", 80, NAVY + (255,), "lm")
K.text(p, (x + 12, y + h + 34), "BUDOKAI TENKAICHI 4  ·  ANDROID", "chak", 28, (235, 242, 255, 255), "lm", spacing=4, shadow_px=2)
save(crop(p, (0, 0, 700, 200)), "hub_logo")

# a chip: 56 tall, 300 wide, 12 of room
p = clear()
K.rr_grad(p, (12, 12, 312, 68), 28, [(0, (255, 255, 255, 60)), (1, (255, 255, 255, 30))], (255, 255, 255, 110), 2)
save(crop(p, (0, 0, 324, 80)), "hub_chip")
