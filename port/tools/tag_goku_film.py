#!/usr/bin/env python3
"""Goku's film for the Tag Team mod's menus (port/tbo/ui/goku.mpg, and db_goku.png its first picture): every picture of the
user's clip (a still-standing Goku on white, 24 frames a second), cut out, from his head down to where the screen cuts him (the
thighs), played forward and then back so the loop has no seam. As the game's films with alpha are (gs/gs_draw.c,
tbo_film_picture): the colours on top, how solid each pixel is below (gray), MPEG-1. The colours where he is not are his
edge's carried outwards, so the colour coding does not bleed dark or white into his outline.
Needs PyAV (pip install av), numpy, pillow, opencv.
    port/tools/tag_goku_film.py GOKU.mp4 [width of the film: 640]"""
import pathlib
import sys

import av
import cv2
import numpy as np
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/tbo/ui"
KEEP = 0.62  # of his height: from the head down to the thighs


def cutout(im):
    """RGBA from a frame on white: the white that touches the edge is the background (the white of his eyes stays)."""
    near = (im.min(axis=2) >= 236).astype(np.uint8)
    pad = cv2.copyMakeBorder(near, 1, 1, 1, 1, cv2.BORDER_CONSTANT, value=1)
    lab = cv2.connectedComponents(pad, connectivity=4)[1]
    bg = (lab == lab[0, 0])[1:-1, 1:-1].astype(np.uint8)
    fg = cv2.morphologyEx(1 - bg, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    fg = cv2.erode(fg, np.ones((3, 3), np.uint8))
    a = np.clip((cv2.GaussianBlur(fg.astype(np.float32), (0, 0), 1.1) - 0.25) / 0.6, 0, 1)
    return np.dstack([cv2.cvtColor(im, cv2.COLOR_BGR2RGB).astype(np.float32), a * 255])


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


def main():
    src = sys.argv[1]
    W = int(round((float(sys.argv[2]) if len(sys.argv) > 2 else 640) / 16)) * 16
    cap = cv2.VideoCapture(src)
    cuts = []
    while True:
        ok, im = cap.read()
        if not ok:
            break
        cuts.append(cutout(im))
    print(len(cuts), "pictures")
    ys = np.concatenate([np.where(c[:, :, 3] > 12)[0] for c in cuts])
    xs = np.concatenate([np.where(c[:, :, 3] > 12)[1] for c in cuts])
    y0, y1, x0, x1 = int(ys.min()), int(ys.max()) + 1, int(xs.min()), int(xs.max()) + 1
    keep = int((y1 - y0) * KEEP)
    box_w, box_h = x1 - x0, keep
    H = int(round(box_h * W / box_w / 16)) * 16
    print("box", box_w, box_h, "-> film", W, H)
    order = list(range(len(cuts))) + list(range(len(cuts) - 2, 0, -1))  # forward, then back
    container = av.open(str(OUT / "goku.mpg"), "w", format="mpeg")
    stream = container.add_stream("mpeg1video", rate=24)
    stream.width, stream.height = W, 2 * H
    stream.pix_fmt = "yuv420p"
    stream.bit_rate = 9_000_000
    stream.codec_context.options = {"qmin": "1", "qmax": "5", "g": "12"}
    first = None
    done = {}
    for n, i in enumerate(order):
        if i not in done:
            c = cuts[i][y0:y0 + keep, x0:x1]
            c = cv2.resize(c, (W, H), interpolation=cv2.INTER_AREA)
            a = np.clip(c[..., 3] / 255, 0, 1)
            rgb = fill_out(c[..., :3], a)
            pic = np.zeros((2 * H, W, 3), np.float32)
            pic[:H] = rgb
            pic[H:] = (a * 255)[..., None]
            done[i] = (np.clip(pic, 0, 255).astype(np.uint8), np.dstack([np.clip(c[..., :3], 0, 255), c[..., 3]]).astype(np.uint8))
        frame = av.VideoFrame.from_ndarray(done[i][0], format="rgb24")
        for pkt in stream.encode(frame):
            container.mux(pkt)
        if first is None:
            first = done[i][1]
    for pkt in stream.encode():
        container.mux(pkt)
    container.close()
    Image.fromarray(first, "RGBA").save(OUT / "db_goku.png")
    print("goku.mpg:", (OUT / "goku.mpg").stat().st_size, "bytes;", len(order), "pictures")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    main()
