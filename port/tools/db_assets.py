#!/usr/bin/env python3
"""The Tag Team mod's pictures in the look of Dragon Ball (port/tbo/ui/db_*.png, include/menu/tbo_art.h): the dusk sky
with clouds behind the mod's menus, the seven dragon balls on one sheet, a soft glow, the rays of the picture window,
its gold frame and the white talk box (both drawn as nine pieces: menu/port_screen.c, Ps_Panel). Drawn here, no
artwork of the game's."""
import math
import random
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parents[2] / "port/tbo/ui"


def sky():
    W, H = 1920, 864
    img = Image.new("RGBA", (W, H))
    col = Image.new("RGBA", (1, H))
    stops = [(0, (18, 52, 140)), (0.45, (70, 140, 220)), (0.72, (255, 190, 110)), (1, (255, 120, 40))]
    for y in range(H):
        t = y / H
        for (a, ca), (b, cb) in zip(stops, stops[1:]):
            if a <= t <= b:
                u = (t - a) / (b - a)
                col.putpixel((0, y), tuple(int(ca[i] + (cb[i] - ca[i]) * u) for i in range(3)) + (255,))
    img.alpha_composite(col.resize((W, H)))
    g = Image.new("RGBA", (W, H))
    ImageDraw.Draw(g).ellipse([1200, 370, 1780, 780], fill=(255, 240, 180, 170))
    img.alpha_composite(g.filter(ImageFilter.GaussianBlur(90)))
    random.seed(3)

    def cloud(cx, cy, s, alpha):
        c = Image.new("RGBA", (W, H))
        d = ImageDraw.Draw(c)
        for _ in range(9):
            r = random.uniform(0.5, 1.0) * s
            x = cx + random.uniform(-1.6, 1.6) * s
            y = cy + random.uniform(-0.35, 0.25) * s
            d.ellipse([x - r, y - r * 0.75, x + r, y + r * 0.75], fill=(255, 250, 240, alpha))
        img.alpha_composite(c.filter(ImageFilter.GaussianBlur(3)))
    k = W / 2712
    for cx, cy, s, a in [(300, 980, 190, 230), (1100, 1060, 230, 235), (2300, 1000, 220, 230), (1700, 160, 120, 140),
                         (2500, 260, 100, 130), (900, 140, 90, 110), (2050, 1120, 180, 240)]:
        cloud(cx * k, cy * k, s * k, a)
    st = Image.new("RGBA", (W, H))
    d = ImageDraw.Draw(st)
    for _ in range(40):
        y = random.uniform(60, 640)
        x = random.uniform(-150, W)
        ln = random.uniform(140, 500)
        d.line([(x, y), (x + ln, y - ln * 0.18)], fill=(255, 255, 255, random.randint(25, 60)), width=random.choice([2, 3, 4]))
    img.alpha_composite(st.filter(ImageFilter.GaussianBlur(1)))
    img.convert("RGB").save(OUT / "db_sky.png")


STARS = {1: [(0, 0)], 2: [(-.3, 0), (.3, 0)], 3: [(0, -.3), (-.3, .22), (.3, .22)], 4: [(-.28, -.28), (.28, -.28), (-.28, .28), (.28, .28)],
         5: [(0, -.38), (-.36, -.08), (.36, -.08), (-.22, .34), (.22, .34)],
         6: [(-.3, -.32), (.3, -.32), (-.38, .05), (.38, .05), (-.18, .38), (.18, .38)],
         7: [(0, 0), (0, -.4), (-.36, -.18), (.36, -.18), (-.36, .22), (.36, .22), (0, .42)]}


def balls():
    S = 256
    sheet = Image.new("RGBA", (S * 8, S))
    for n in range(8):  # 0: no stars
        b = Image.new("RGBA", (S * 2, S * 2))
        d = ImageDraw.Draw(b)
        c = S
        r = S * 0.92
        d.ellipse([c - r, c - r, c + r, c + r], fill=(170, 70, 0, 255))
        r2 = r - S * 0.05
        for k in range(int(r2), 0, -1):
            t = k / r2
            col = (255, int(120 + 110 * (1 - t)), int(20 + 120 * (1 - t) ** 2), 255)
            ox, oy = -r2 * 0.28 * (1 - t), -r2 * 0.3 * (1 - t)
            d.ellipse([c + ox - k, c + oy - k, c + ox + k, c + oy + k], fill=col)
        for px, py in STARS.get(n, []):
            sr = r2 * (0.42 if n == 1 else 0.26 if n <= 4 else 0.2)
            pts = []
            for k in range(10):
                rr = sr if k % 2 == 0 else sr * 0.42
                a = -math.pi / 2 + k * math.pi / 5
                pts.append((c + px * r2 + math.cos(a) * rr, c + py * r2 + math.sin(a) * rr))
            d.polygon(pts, fill=(220, 30, 20, 255))
        hl = Image.new("RGBA", b.size)
        ImageDraw.Draw(hl).ellipse([c - r2 * 0.62, c - r2 * 0.7, c - r2 * 0.1, c - r2 * 0.32], fill=(255, 255, 255, 170))
        b.alpha_composite(hl.filter(ImageFilter.GaussianBlur(r2 * 0.05)))
        sheet.alpha_composite(b.resize((S, S), Image.LANCZOS), (n * S, 0))
    sheet.save(OUT / "db_balls.png")


def glow():
    S = 256
    g = Image.new("RGBA", (S, S))
    ImageDraw.Draw(g).ellipse([S * 0.2, S * 0.2, S * 0.8, S * 0.8], fill=(255, 255, 255, 255))
    g.filter(ImageFilter.GaussianBlur(S * 0.12)).save(OUT / "db_glow.png")


def rays():
    W, H = 1024, 512
    r = Image.new("RGBA", (W, H))
    d = ImageDraw.Draw(r)
    cx, cy = W / 2, H / 2
    for k in range(18):
        a = k * math.pi / 9
        d.polygon([(cx, cy), (cx + math.cos(a) * 900, cy + math.sin(a) * 900), (cx + math.cos(a + 0.12) * 900, cy + math.sin(a + 0.12) * 900)],
                  fill=(255, 255, 220, 90))
    r.filter(ImageFilter.GaussianBlur(2)).save(OUT / "db_rays.png")


def frame():
    S = 256  # gold rounded frame, its inside empty (the window's picture is drawn first, under it)
    k = 4
    f = Image.new("RGBA", (S * k, S * k))
    d = ImageDraw.Draw(f)
    d.rounded_rectangle([0, 0, S * k - 1, S * k - 1], radius=36 * k, fill=(120, 40, 0, 255))
    d.rounded_rectangle([6 * k, 6 * k, S * k - 6 * k, S * k - 6 * k], radius=31 * k, fill=(255, 200, 70, 255))
    d.rounded_rectangle([10 * k, 8 * k, S * k - 10 * k, 30 * k], radius=14 * k, fill=(255, 235, 150, 255))
    d.rounded_rectangle([22 * k, 22 * k, S * k - 22 * k, S * k - 22 * k], radius=22 * k, fill=(0, 0, 0, 0))
    f.resize((S, S), Image.LANCZOS).save(OUT / "db_frame.png")


def dialog():
    S = 256
    k = 4
    f = Image.new("RGBA", (S * k, S * k))
    d = ImageDraw.Draw(f)
    d.rounded_rectangle([0, 0, S * k - 1, S * k - 1], radius=40 * k, fill=(30, 50, 140, 255))
    d.rounded_rectangle([8 * k, 8 * k, S * k - 8 * k, S * k - 8 * k], radius=33 * k, fill=(250, 250, 255, 248))
    d.rounded_rectangle([16 * k, 16 * k, S * k - 16 * k, S * k - 16 * k], radius=26 * k, outline=(255, 170, 40, 255), width=5 * k)
    f.resize((S, S), Image.LANCZOS).save(OUT / "db_dialog.png")


def night():
    """The night behind the mod's menus (the radar's page and the others): deep blue, stars, rock spires in two
    layers with a mist over them, a warm glow on the left where the radar stands. No rays."""
    W, H = 1920, 864
    rnd = random.Random(11)
    img = Image.new("RGBA", (W, H))
    col = Image.new("RGBA", (1, H))
    for y in range(H):
        t = y / H
        col.putpixel((0, y), (int(8 + 18 * t), int(16 + 34 * t), int(42 + 50 * t), 255))
    img.alpha_composite(col.resize((W, H)))
    st = Image.new("RGBA", (W, H))
    d = ImageDraw.Draw(st)
    for _ in range(190):
        x, y, r = rnd.uniform(0, W), rnd.uniform(0, H * 0.7), rnd.choice([1.0, 1.4, 1.8, 2.5])
        d.ellipse([x - r, y - r, x + r, y + r], fill=(220, 235, 255, rnd.randint(60, 200)))
    img.alpha_composite(st.filter(ImageFilter.GaussianBlur(0.7)))
    warm = Image.new("RGBA", (W, H))
    ImageDraw.Draw(warm).ellipse([140, 180, 1060, 820], fill=(255, 140, 40, 60))
    img.alpha_composite(warm.filter(ImageFilter.GaussianBlur(130)))
    k = W / 2712
    for shade, base, seed, hmin, hmax in [((22, 40, 74, 255), 1010, 3, 180, 380), ((12, 24, 48, 255), 1080, 5, 90, 240)]:
        r2 = random.Random(seed)
        pts = [(0, H)]
        x = 0
        while x < 2712 + 200:
            w = r2.uniform(90, 220)
            h = r2.uniform(hmin, hmax)
            pts += [(x * k, base * k), ((x + w * 0.2) * k, (base - h) * k), ((x + w * 0.75) * k, (base - h * r2.uniform(0.85, 1.0)) * k),
                    ((x + w) * k, base * k)]
            x += w + r2.uniform(-30, 60)
        pts.append((W, H))
        m = Image.new("RGBA", (W, H))
        ImageDraw.Draw(m).polygon(pts, fill=shade)
        img.alpha_composite(m)
    fog = Image.new("RGBA", (W, H))
    fd = ImageDraw.Draw(fog)
    for y in range(int(820 * k), H):
        t = (y - 820 * k) / (H - 820 * k)
        fd.line([(0, y), (W, y)], fill=(40, 70, 120, int(90 * (1 - abs(t - 0.4)))))
    img.alpha_composite(fog.filter(ImageFilter.GaussianBlur(14)))
    img.convert("RGB").save(OUT / "db_night.png")


def radar():
    """The dragon radar of the mod's main page: a pale round case with its button on top, the green screen with its
    grid and two rings, the glass's light. Its circle's middle is at (512, 588) of 1024 x 1100, the case 500 round,
    the screen 452 (menu_a.c draws the balls on it by those numbers)."""
    W, H, CX, CY, RO, RS = 1024, 1100, 512, 588, 500, 452
    k = 2
    big = Image.new("RGBA", (W * k, H * k))
    d = ImageDraw.Draw(big)
    S = lambda v: v * k
    d.ellipse([S(CX - RO + 14), S(CY - RO + 26), S(CX + RO + 14), S(CY + RO + 26)], fill=(0, 0, 0, 120))
    big = big.filter(ImageFilter.GaussianBlur(S(18)))
    d = ImageDraw.Draw(big)
    d.rounded_rectangle([S(CX - 80), S(CY - RO - 66), S(CX + 80), S(CY - RO + 24)], radius=S(22), fill=(225, 228, 235, 255), outline=(120, 125, 140, 255), width=S(5))
    d.rounded_rectangle([S(CX - 52), S(CY - RO - 48), S(CX + 52), S(CY - RO - 8)], radius=S(13), fill=(250, 110, 40, 255), outline=(150, 50, 10, 255), width=S(3))
    for i in range(60, 0, -1):
        c = int(150 + 100 * (1 - i / 60))
        rr = RO - (60 - i) * 0.0
        d.ellipse([S(CX - rr), S(CY - rr), S(CX + rr), S(CY + rr)], fill=(c, c, min(255, int(c * 1.02)), 255))
    hl = Image.new("RGBA", big.size)
    ImageDraw.Draw(hl).ellipse([S(CX - RO * 0.9), S(CY - RO * 0.98), S(CX + RO * 0.4), S(CY - RO * 0.2)], fill=(255, 255, 255, 90))
    case = Image.new("L", big.size)
    ImageDraw.Draw(case).ellipse([S(CX - RO), S(CY - RO), S(CX + RO), S(CY + RO)], fill=255)
    o = Image.new("RGBA", big.size)
    o.paste(hl.filter(ImageFilter.GaussianBlur(S(30))), (0, 0), case)
    big.alpha_composite(o)
    d = ImageDraw.Draw(big)
    d.ellipse([S(CX - RO), S(CY - RO), S(CX + RO), S(CY + RO)], outline=(110, 115, 130, 255), width=S(8))
    d.ellipse([S(CX - RS - 14), S(CY - RS - 14), S(CX + RS + 14), S(CY + RS + 14)], fill=(60, 66, 80, 255))
    scr = Image.new("RGBA", big.size)
    sd = ImageDraw.Draw(scr)
    for r in range(RS, 0, -1):
        t = r / RS
        sd.ellipse([S(CX - r), S(CY - r), S(CX + r), S(CY + r)], fill=(int(40 + 40 * (1 - t)), int(110 + 80 * (1 - t)), int(50 + 30 * (1 - t)), 255))
    mask = Image.new("L", big.size)
    ImageDraw.Draw(mask).ellipse([S(CX - RS), S(CY - RS), S(CX + RS), S(CY + RS)], fill=255)
    gd = ImageDraw.Draw(scr)
    for g in range(-8, 9):
        gd.line([(S(CX + g * 84), S(CY - RS)), (S(CX + g * 84), S(CY + RS))], fill=(190, 255, 190, 70), width=S(3))
        gd.line([(S(CX - RS), S(CY + g * 84)), (S(CX + RS), S(CY + g * 84))], fill=(190, 255, 190, 70), width=S(3))
    for r in (RS * 0.33, RS * 0.66):
        gd.ellipse([S(CX - r), S(CY - r), S(CX + r), S(CY + r)], outline=(200, 255, 200, 90), width=S(3))
    ref = Image.new("RGBA", big.size)
    ImageDraw.Draw(ref).ellipse([S(CX - RS * 0.85), S(CY - RS * 0.95), S(CX + RS * 0.55), S(CY - RS * 0.15)], fill=(255, 255, 255, 40))
    scr.alpha_composite(ref.filter(ImageFilter.GaussianBlur(S(20))))
    o = Image.new("RGBA", big.size)
    o.paste(scr, (0, 0), mask)
    big.alpha_composite(o)
    d = ImageDraw.Draw(big)
    d.ellipse([S(CX - RS), S(CY - RS), S(CX + RS), S(CY + RS)], outline=(20, 50, 25, 255), width=S(6))
    big.resize((W, H), Image.LANCZOS).save(OUT / "db_radar.png")


def ring():
    """A thin white ring, its edges soft (tinted: the radar's pulses and the chosen ball's search rings)."""
    S, k = 256, 4
    r = Image.new("RGBA", (S * k, S * k))
    ImageDraw.Draw(r).ellipse([10 * k, 10 * k, S * k - 10 * k, S * k - 10 * k], outline=(255, 255, 255, 255), width=9 * k)
    r.filter(ImageFilter.GaussianBlur(2 * k)).resize((S, S), Image.LANCZOS).save(OUT / "db_ring.png")


def glass():
    """Dark blue glass with a light edge and a little light along its top (nine pieces: the panels and plates)."""
    S, k = 256, 4
    f = Image.new("RGBA", (S * k, S * k))
    d = ImageDraw.Draw(f)
    d.rounded_rectangle([0, 0, S * k - 1, S * k - 1], radius=40 * k, fill=(120, 200, 255, 190))
    d.rounded_rectangle([3 * k, 3 * k, S * k - 3 * k, S * k - 3 * k], radius=37 * k, fill=(12, 24, 62, 232))
    top = Image.new("RGBA", f.size)
    td = ImageDraw.Draw(top)
    for y in range(3 * k, 70 * k):
        t = (y - 3 * k) / (67 * k)
        td.line([(0, y), (S * k, y)], fill=(255, 255, 255, int(34 * (1 - t))))
    m = Image.new("L", f.size)
    ImageDraw.Draw(m).rounded_rectangle([3 * k, 3 * k, S * k - 3 * k, S * k - 3 * k], radius=37 * k, fill=255)
    o = Image.new("RGBA", f.size)
    o.paste(top, (0, 0), m)
    f.alpha_composite(o)
    f.resize((S, S), Image.LANCZOS).save(OUT / "db_glass.png")


if __name__ == "__main__":
    sky(); balls(); glow(); rays(); frame(); dialog(); night(); radar(); ring(); glass()
    print("db_*.png ->", OUT)
