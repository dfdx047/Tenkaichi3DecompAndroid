#!/usr/bin/env python3
"""Trunks's film for the BT4 Online main screen (port/tbo/ui/trunks.mpg, and trunks.png its first picture): the frames
of the cut-out animation (previas/bt4-online: RGBA, the box 436 x 993 per scale, his feet at y 989), from his head
down to where the screen cuts him (his thighs), as the game's films with alpha are: the colours above, how solid each
pixel is below (gray), MPEG-1 at 24 frames a second (gs/gs_draw.c tbo_film_picture). The colours where he is not
are his edge's carried outwards, so the picture's colour coding does not bleed dark or white into his outline.
Needs PyAV (pip install av), numpy, pillow, opencv.
    python3 port/tools/tbo_trunks_film.py <frames folder> [box scale: 1 or 2] [film scale against the 1x box: 1.5]"""
import os
import pathlib
import sys

import av
import cv2
import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/tbo/ui"
src = sys.argv[1]
O = int(sys.argv[2]) if len(sys.argv) > 2 else 1
K = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
ROWS, COLS = 608, 448          # of the 1x box: his head to below where the screen cuts him; his width
W = int(round(COLS * K / 16)) * 16
H = int(round(ROWS * K / 16)) * 16
names = sorted(n for n in os.listdir(src) if n.endswith(".png"))


def fill_out(rgb, a):
    """the colours of the transparent parts: his edge's, carried out (premultiplied blur, several sizes)"""
    out = rgb.copy()
    have = a.copy()
    for s in (2, 6, 18, 50):
        num = cv2.GaussianBlur(out * have[..., None], (0, 0), s)
        den = cv2.GaussianBlur(have, (0, 0), s)
        fill = num / np.maximum(den, 1e-4)[..., None]
        w = (1 - have)[..., None] * (den > 1e-3)[..., None]
        out = out * (1 - w) + fill * w
        have = np.maximum(have, (den > 1e-3).astype(np.float32))
    return out


container = av.open(str(OUT / "trunks.mpg"), "w", format="mpeg")
stream = container.add_stream("mpeg1video", rate=24)
stream.width, stream.height = W, 2 * H
stream.pix_fmt = "yuv420p"
stream.bit_rate = 9_000_000
stream.codec_context.options = {"qmin": "1", "qmax": "5", "g": "12"}
for i, n in enumerate(names):
    im = np.asarray(Image.open(os.path.join(src, n)).convert("RGBA")).astype(np.float32)
    im = im[: ROWS * O, : COLS * O] if im.shape[1] >= COLS * O else np.pad(im, ((0, 0), (0, COLS * O - im.shape[1]), (0, 0)))[: ROWS * O]
    im = cv2.resize(im, (W, H), interpolation=cv2.INTER_AREA)
    a = np.clip(im[..., 3] / 255, 0, 1)
    rgb = fill_out(im[..., :3], a)
    pic = np.zeros((2 * H, W, 3), np.float32)
    pic[:H] = rgb
    pic[H:] = (a * 255)[..., None]
    frame = av.VideoFrame.from_ndarray(np.clip(pic, 0, 255).astype(np.uint8), format="rgb24")
    for pkt in stream.encode(frame):
        container.mux(pkt)
    if i == 0:
        Image.fromarray(np.dstack([np.clip(im[..., :3], 0, 255), a * 255]).astype(np.uint8), "RGBA").save(OUT / "trunks.png", optimize=True)
for pkt in stream.encode():
    container.mux(pkt)
container.close()
print("trunks.mpg", W, "x", 2 * H, len(names), "frames,", (OUT / "trunks.mpg").stat().st_size, "bytes")
