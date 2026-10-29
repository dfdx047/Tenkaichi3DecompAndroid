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


if __name__ == "__main__":
    sky(); balls(); glow(); rays(); frame(); dialog()
    print("db_*.png ->", OUT)
