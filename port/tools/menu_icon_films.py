#!/usr/bin/env python3
"""The main menu's animated icons (port/tbo/ui/menu_ball.mpg, menu_radar.mpg and their first pictures): Z Warriors'
four-star ball, from the user's clip (on black), and Tag Team's Dragon Radar (drawn here: its sweep turns, its dots
blink). Films with alpha, as gs/gs_draw.c's: colours on top, how solid each pixel is below, MPEG-1.
    port/tools/menu_icon_films.py BALL.mp4"""
import math
import pathlib
import sys

import av
import numpy as np
from PIL import Image, ImageDraw

OUT = pathlib.Path(__file__).resolve().parents[2] / "port/tbo/ui"
N = 128


def film(name, pictures):
    c = av.open(str(OUT / (name + ".mpg")), "w", format="mpeg")
    s = c.add_stream("mpeg1video", rate=24)
    s.width, s.height, s.pix_fmt, s.bit_rate = N, 2 * N, "yuv420p", 2_500_000
    s.codec_context.options = {"qmin": "1", "qmax": "5", "g": "12"}
    for p in pictures:
        a = np.asarray(p, np.uint8)
        stacked = np.concatenate([a[:, :, :3], np.repeat(a[:, :, 3:], 3, axis=2)])
        for pkt in s.encode(av.VideoFrame.from_ndarray(stacked, format="rgb24")):
            c.mux(pkt)
    for pkt in s.encode():
        c.mux(pkt)
    c.close()
    pictures[0].save(OUT / (name + ".png"))
    print(name, len(pictures), "pictures", (OUT / (name + ".mpg")).stat().st_size, "bytes")


def ball(clip):
    box = None
    out = []
    mask = Image.new("L", (N * 4, N * 4), 0)
    ImageDraw.Draw(mask).ellipse((4, 4, N * 4 - 4, N * 4 - 4), fill=255)
    mask = mask.resize((N, N), Image.LANCZOS)
    for x in av.open(clip).decode(video=0):
        f = x.to_image().convert("RGB")
        if box is None:  # the ball: the bright pixels of the first picture
            x0, y0, x1, y1 = f.convert("L").point(lambda v: 255 if v > 60 else 0).getbbox()
            cx, cy, r = (x0 + x1) / 2, (y0 + y1) / 2, max(x1 - x0, y1 - y0) / 2 * 1.02
            box = (int(cx - r), int(cy - r), int(cx + r), int(cy + r))
        p = f.crop(box).resize((N, N), Image.LANCZOS).convert("RGBA")
        p.putalpha(mask)
        out.append(p)
    return out


def radar():
    out = []
    k = 4
    for i in range(48):
        im = Image.new("RGBA", (360, 360))
        d = ImageDraw.Draw(im)
        d.rounded_rectangle((146, 4, 214, 52), 16, fill=(200, 206, 220), outline=(40, 44, 70), width=11)
        d.ellipse((22, 34, 338, 350), fill=(222, 228, 240), outline=(40, 44, 70), width=13)
        d.ellipse((58, 70, 302, 314), fill=(28, 128, 60), outline=(20, 60, 35), width=11)
        for j in range(1, 6):
            x = 58 + j * 244 / 6
            h = (122 ** 2 - (x - 180) ** 2) ** 0.5
            d.line((x, 192 - h, x, 192 + h), fill=(90, 200, 110), width=5)
            y = 70 + j * 244 / 6
            w = (122 ** 2 - (y - 192) ** 2) ** 0.5
            d.line((180 - w, y, 180 + w, y), fill=(90, 200, 110), width=5)
        turn = i * 360 / 48  # the sweep: one round in two seconds
        sweep = Image.new("RGBA", im.size)
        sd = ImageDraw.Draw(sweep)
        for t in range(12):
            sd.pieslice((66, 78, 294, 306), turn - 8 * (t + 1) - 90, turn - 8 * t - 90, fill=(170, 255, 170, 170 - 13 * t))
        im.alpha_composite(sweep)
        d = ImageDraw.Draw(im)
        d.polygon([(180, 166), (204, 212), (156, 212)], fill=(255, 70, 60))
        for x, y in ((120, 140), (236, 246), (240, 134)):
            at = (math.degrees(math.atan2(y - 192, x - 180)) + 90) % 360
            r = 19 + 9 * max(0.0, 1.0 - ((turn - at) % 360) / 90.0)  # a dot swells as the sweep passes it
            d.ellipse((x - r, y - r, x + r, y + r), fill=(255, 226, 60), outline=(120, 80, 0), width=6)
        out.append(im.resize((N, N), Image.LANCZOS))
    return out


film("menu_ball", ball(sys.argv[1]))
film("menu_radar", radar())
