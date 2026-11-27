#!/usr/bin/env python3
"""The pictures of Z Warriors' touch screens (the approved templates): panels, pills, the back button, the arrow
(port/tbo/ui/zw_t_*.png). Sizes and corners: include/menu/zw_sizes.h, sZwTArtSize."""
import pathlib
from PIL import Image, ImageDraw

OUT = pathlib.Path(__file__).resolve().parents[2] / "port/tbo/ui"
K = 4
BLUE, LINE, ORANGE, WHITE = (14, 24, 66, 228), (90, 130, 230, 255), (255, 168, 40, 255), (255, 255, 255, 205)


def rounded(name, w, h, r, fill, out, wd):
    im = Image.new("RGBA", (w * K, h * K), out[:3] + (0,))
    ImageDraw.Draw(im).rounded_rectangle((0, 0, w * K - 1, h * K - 1), r * K, fill=fill, outline=out, width=wd * K)
    im.resize((w, h), Image.LANCZOS).save(OUT / (name + ".png"))


rounded("zw_t_panel", 200, 200, 40, BLUE, LINE, 4)
rounded("zw_t_sel", 200, 200, 40, (48, 76, 170, 245), ORANGE, 9)
rounded("zw_t_stage", 200, 200, 40, (8, 14, 48, 225), LINE, 4)
rounded("zw_t_ask", 200, 200, 40, (16, 26, 76, 255), LINE, 4)
for n, c in (("navy", BLUE), ("orange", (230, 128, 18, 245)), ("green", (70, 200, 130, 255)),
             ("red", (214, 70, 80, 255)), ("grey", (70, 80, 130, 255)), ("bar", (34, 56, 128, 245))):
    rounded("zw_t_" + n, 400, 100, 50, c, WHITE, 3)
rounded("zw_t_ring", 400, 100, 50, (0, 0, 0, 0), ORANGE, 8)

im = Image.new("RGBA", (112 * K, 112 * K))
d = ImageDraw.Draw(im)
d.ellipse((0, 0, 112 * K - 1, 112 * K - 1), fill=BLUE, outline=LINE, width=3 * K)
d.line([(70 * K, 28 * K), (40 * K, 56 * K), (70 * K, 84 * K)], fill=(255, 255, 255, 255), width=11 * K, joint="curve")
im.resize((112, 112), Image.LANCZOS).save(OUT / "zw_t_back.png")

im = Image.new("RGBA", (28 * K, 44 * K))
ImageDraw.Draw(im).line([(6 * K, 6 * K), (22 * K, 22 * K), (6 * K, 38 * K)], fill=(255, 255, 255, 255), width=6 * K, joint="curve")
im.resize((28, 44), Image.LANCZOS).save(OUT / "zw_t_chev.png")

# ---- the online screens' own pictures (the "play online" button without its letters, its glow, the shine that crosses
# it, and the ring the search sends out)
from PIL import ImageFilter

S_ = 2


def online_button():
    w, h, pad = 700, 116, 30
    L = Image.new("RGBA", ((w + 2 * pad) * K, (h + 2 * pad) * K))
    P, W_, H_, r = pad * K, w * K, h * K, h * K // 2
    sh = Image.new("RGBA", L.size)
    ImageDraw.Draw(sh).rounded_rectangle((P, P + 9 * K, P + W_, P + H_ + 9 * K), r, fill=(40, 8, 30, 170))
    L.alpha_composite(sh.filter(ImageFilter.GaussianBlur(6 * K)))
    d = ImageDraw.Draw(L)
    d.rounded_rectangle((P, P, P + W_, P + H_), r, fill=(255, 226, 130, 255))
    d.rounded_rectangle((P + 5 * K, P + 5 * K, P + W_ - 5 * K, P + H_ - 5 * K), r, fill=(120, 30, 10, 255))
    iw, ih = W_ - 18 * K, H_ - 18 * K
    col = Image.new("RGBA", (1, ih))
    for i in range(ih):
        f = i / ih
        a, b, c = (255, 184, 40), (244, 104, 12), (200, 36, 24)
        q = tuple(int(a[k] + (b[k] - a[k]) * f * 2) if f < 0.5 else int(b[k] + (c[k] - b[k]) * (f * 2 - 1)) for k in range(3))
        col.putpixel((0, i), q + (255,))
    m = Image.new("L", (iw, ih))
    ImageDraw.Draw(m).rounded_rectangle((0, 0, iw - 1, ih - 1), ih // 2, fill=255)
    L.paste(col.resize((iw, ih)), (P + 9 * K, P + 9 * K), m)
    shine = Image.new("RGBA", L.size)
    ImageDraw.Draw(shine).rounded_rectangle((P + 40 * K, P + 13 * K, P + W_ - 40 * K, P + H_ * 0.42), H_ // 5, fill=(255, 255, 255, 70))
    L.alpha_composite(shine)
    d = ImageDraw.Draw(L)
    cx = cy = P + H_ // 2
    R, g, wd = H_ * 0.36, H_ * 0.24, 3 * K
    d.ellipse((cx - R, cy - R, cx + R, cy + R), fill=(36, 60, 150, 255), outline=(255, 226, 130, 255), width=4 * K)
    d.ellipse((cx - g, cy - g, cx + g, cy + g), outline="white", width=wd)
    d.ellipse((cx - g * 0.45, cy - g, cx + g * 0.45, cy + g), outline="white", width=wd)
    d.line((cx - g, cy, cx + g, cy), fill="white", width=wd)
    for s in (-1, 1):
        d.line((cx - g * 0.85, cy + s * g * 0.52, cx + g * 0.85, cy + s * g * 0.52), fill="white", width=wd)
    for s in range(3):
        ax = P + W_ - H_ * 0.50 + s * H_ * 0.09
        d.line([(ax, cy - H_ * 0.13), (ax + H_ * 0.08, cy), (ax, cy + H_ * 0.13)], fill=(255, 255, 255, 115 + 70 * s), width=4 * K, joint="curve")
    L.resize(((w + 2 * pad) * S_, (h + 2 * pad) * S_), Image.LANCZOS).save(OUT / "zw_t_online.png")


online_button()
im = Image.new("RGBA", (760, 176))
ImageDraw.Draw(im).rounded_rectangle((28, 28, 732, 148), 60, fill=(255, 172, 30, 255))
im.filter(ImageFilter.GaussianBlur(17)).save(OUT / "zw_t_glow.png")
im = Image.new("RGBA", (200 * K, 200 * K))
ImageDraw.Draw(im).polygon([(110 * K, 0), (160 * K, 0), (90 * K, 200 * K), (40 * K, 200 * K)], fill=(255, 255, 255, 105))
im.filter(ImageFilter.GaussianBlur(5 * K)).resize((200, 200), Image.LANCZOS).save(OUT / "zw_t_shine.png")
im = Image.new("RGBA", (400 * K, 400 * K))
ImageDraw.Draw(im).ellipse((4 * K, 4 * K, 396 * K, 396 * K), outline=(130, 225, 255, 255), width=6 * K)
im.resize((400, 400), Image.LANCZOS).save(OUT / "zw_t_ripple.png")
