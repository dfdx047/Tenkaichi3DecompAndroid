#!/usr/bin/env python3
"""The six pictures of the "Toque Pro" guide for the changelog and the README (docs/images/touch-pro/*.png), drawn
here as the app draws them (android/.../ui/screens/TouchGuide.kt): thumbs, rings and arrows, nothing of the game.
  python3 -I port/tools/touch_guide_images.py
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parents[2] / "docs/images/touch-pro"
W, H = 1200, 675
SS = 2  # drawn at twice the size, then reduced: smooth edges
BG = (13, 15, 28)
GOLD = (255, 213, 79)
KI = (111, 211, 255)
DIM = (58, 64, 96)
SOFT = (184, 192, 224)
RED = (215, 38, 30)
PANEL = (22, 26, 46)
SKIN = (232, 196, 160)


def u(f):
    return H * SS * f


def canvas():
    # (an RGB picture: ImageDraw blends the colours' alpha only into RGB, not into RGBA)
    img = Image.new("RGB", (W * SS, H * SS), BG)
    return img, ImageDraw.Draw(img, "RGBA")


def save(img, name):
    OUT.mkdir(parents=True, exist_ok=True)
    img.resize((W, H), Image.LANCZOS).save(OUT / name, optimize=True)


def circle(d, c, r, fill=None, outline=None, width=0):
    d.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], fill=fill, outline=outline, width=int(width))


def dashed_circle(d, c, r, color, width, dash=0.12):
    n = max(12, int(2 * math.pi * r / (u(0.05))))
    for i in range(n):
        a0 = 2 * math.pi * i / n
        a1 = a0 + 2 * math.pi / n * 0.6
        d.arc([c[0] - r, c[1] - r, c[0] + r, c[1] + r], math.degrees(a0), math.degrees(a1), fill=color, width=int(width))


def thumb(img, at, r):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.ellipse([at[0] - r * 0.8, at[1] - r, at[0] + r * 0.8, at[1] + r * 1.6], fill=SKIN + (240,))
    d.ellipse([at[0] - r * 0.45, at[1] - r * 0.8, at[0] + r * 0.45, at[1] + r * 0.1], fill=(248, 226, 204, 240))
    layer = layer.filter(ImageFilter.GaussianBlur(r * 0.04))
    rgba = img.convert("RGBA")
    rgba.alpha_composite(layer)
    img.paste(rgba.convert("RGB"))


def arrow(d, a, b, color, w):
    d.line([a, b], fill=color, width=int(w))
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    h = w * 3.2
    p1 = (b[0] + math.cos(ang) * h * 0.6, b[1] + math.sin(ang) * h * 0.6)
    p2 = (b[0] - math.cos(ang) * h - math.sin(ang) * h * 0.7, b[1] - math.sin(ang) * h + math.cos(ang) * h * 0.7)
    p3 = (b[0] - math.cos(ang) * h + math.sin(ang) * h * 0.7, b[1] - math.sin(ang) * h - math.cos(ang) * h * 0.7)
    d.polygon([p1, p2, p3], fill=color)


def stick_base(d, c, r, color, alpha=1.0):
    circle(d, c, r, fill=color + (int(46 * alpha),))
    circle(d, c, r, outline=color + (int(204 * alpha),), width=u(0.012))


def button(d, c, r, color, pressed=False):
    circle(d, c, r, fill=color + (255,) if pressed else (26, 31, 53, 255))
    circle(d, c, r, outline=color + (255,), width=u(0.014))


def dynamic():
    img, d = canvas()
    r = u(0.20)
    a = (W * SS * 0.30, u(0.55))
    b = (W * SS * 0.58, u(0.45))
    stick_base(d, a, r, DIM, 0.9)
    arrow(d, (a[0] + r * 0.2, a[1]), (b[0] - r * 0.25, b[1] + u(0.02)), GOLD, u(0.014))
    stick_base(d, b, r, KI)
    circle(d, (b[0] + r * 0.6, b[1] - r * 0.12), r * 0.42, fill=KI + (255,))
    thumb(img, (b[0] + r * 0.95, b[1] - r * 0.2), u(0.10))
    save(img, "01-dynamic-stick.png")


def deadzone():
    img, d = canvas()
    c = (W * SS * 0.42, u(0.5))
    r = u(0.40)
    circle(d, c, r, fill=KI + (26,))
    dashed_circle(d, c, r, KI + (140,), u(0.012))
    circle(d, c, r * 0.75, fill=GOLD + (64,))
    circle(d, c, r * 0.75, outline=GOLD + (255,), width=u(0.012))
    circle(d, c, r * 0.14, fill=(42, 47, 72, 255), outline=SOFT + (255,), width=u(0.01))
    x0, y0, h, bw = W * SS * 0.72, u(0.15), u(0.70), u(0.06)
    d.rectangle([x0, y0 + h * 0.86, x0 + bw, y0 + h], fill=(42, 47, 72))
    d.rectangle([x0, y0 + h * 0.25, x0 + bw, y0 + h * 0.86], fill=GOLD + (180,))
    d.rectangle([x0, y0, x0 + bw, y0 + h * 0.25], fill=KI)
    save(img, "02-dead-zone.png")


def hit_area():
    img, d = canvas()
    c = (W * SS * 0.5, u(0.5))
    r = u(0.20)
    circle(d, c, r * 1.35, fill=GOLD + (30,))
    dashed_circle(d, c, r * 1.35, GOLD + (255,), u(0.012))
    button(d, c, r, KI)
    thumb(img, (c[0] + r * 1.15, c[1] + r * 0.75), u(0.10))
    save(img, "03-touch-area.png")


def slide():
    img, d = canvas()
    ys = u(0.5)
    xs = [W * SS * f for f in (0.25, 0.5, 0.75)]
    for i, (x, col) in enumerate(zip(xs, (KI, GOLD, RED))):
        button(d, (x, ys), u(0.13), col, pressed=i == 2)
    pts = []
    for i in range(2):
        for t in [k / 40 for k in range(41)]:
            x = xs[i] + (xs[i + 1] - xs[i]) * t
            y = ys - math.sin(t * math.pi * 2) * u(0.10)
            pts.append((x, y))
    for i in range(0, len(pts) - 1, 2):
        d.line([pts[i], pts[i + 1]], fill=(255, 255, 255, 210), width=int(u(0.014)))
    thumb(img, (xs[2] + u(0.03), ys + u(0.05)), u(0.10))
    save(img, "04-slide.png")


def toggle():
    img, d = canvas()
    y = u(0.5)
    a = (W * SS * 0.28, y)
    b = (W * SS * 0.72, y)
    circle(d, a, u(0.26), outline=GOLD + (64,), width=u(0.02))
    button(d, a, u(0.17), GOLD, pressed=True)
    button(d, b, u(0.17), DIM)
    arrow(d, (a[0] + u(0.24), y), (b[0] - u(0.26), y), SOFT, u(0.012))
    thumb(img, (a[0] + u(0.05), a[1] + u(0.12)), u(0.085))
    thumb(img, (b[0] + u(0.05), b[1] + u(0.12)), u(0.085))
    save(img, "05-tap-toggles.png")


def profiles():
    img, d = canvas()
    w, h = W * SS * 0.40, u(0.62)
    for side in (0, 1):
        x0 = W * SS * (0.06 if side == 0 else 0.54)
        y0 = u(0.19)
        d.rounded_rectangle([x0, y0, x0 + w, y0 + h], radius=u(0.04), fill=PANEL, outline=SOFT + (128,), width=int(u(0.008)))
        sx = x0 + w * (0.22 if side == 0 else 0.78)
        bx = x0 + w * (0.76 if side == 0 else 0.24)
        stick_base(d, (sx, y0 + h * 0.62), h * 0.2, KI)
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            button(d, (bx + dx * h * 0.13, y0 + h * 0.55 + dy * h * 0.13), h * 0.07, GOLD)
    arrow(d, (W * SS * 0.47, u(0.5)), (W * SS * 0.53, u(0.5)), GOLD, u(0.014))
    save(img, "06-profiles.png")


if __name__ == "__main__":
    for f in (dynamic, deadzone, hit_area, slide, toggle, profiles):
        f()
    print("\n".join(str(p) for p in sorted(OUT.glob("*.png"))))
