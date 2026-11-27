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
