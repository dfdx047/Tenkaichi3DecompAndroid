#!/usr/bin/env python3
"""Z Warriors' home screen, from the user's two clips: the Kaioshins' world (port/tbo/ui/zw_bg.mpg and zw_bg.png) and
Shin standing on black (zw_shin.mpg, with alpha: colours on top, solidity below, as gs/gs_draw.c's films; zw_shin.png).
The black of Shin's drawing that is shadow (under the shoulders, the sash, between arm and body) stays: only the
black outside his outline is background. Needs PyAV, numpy, pillow, opencv.
    port/tools/zw_kai_films.py WORLD.mp4 SHIN.mp4"""
import pathlib
import sys

import av
import cv2
import numpy as np
from PIL import Image

OUT = pathlib.Path(__file__).resolve().parents[2] / "port/tbo/ui"


def frames(f):
    return [x.to_ndarray(format="rgb24") for x in av.open(f).decode(video=0)]


def cut(a):
    art = (a.max(axis=2) >= 14).astype(np.uint8) * 255
    _, lab, st, _ = cv2.connectedComponentsWithStats(art)
    art = np.where(np.isin(lab, np.flatnonzero(st[:, 4] >= 60)[1:]), 255, 0).astype(np.uint8)
    p = 60
    band = cv2.copyMakeBorder(art[640 - p:1250 + p], 0, 0, p, p, cv2.BORDER_CONSTANT, value=0)  # (only the trunk's rows)
    half = cv2.resize(band, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_NEAREST)  # (at half size: four times quicker)
    half = cv2.morphologyEx(half, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (41, 41)))
    shut = cv2.resize(half, (band.shape[1], band.shape[0]), interpolation=cv2.INTER_NEAREST)[p:-p, p:-p]
    fg = art.copy()
    fg[640:1250] = shut | art[640:1250]  # the trunk, from the collar to the robe's hem (rows of the 560 x 1632 clip)
    hole = cv2.copyMakeBorder((fg == 0).astype(np.uint8), 1, 1, 1, 1, cv2.BORDER_CONSTANT, value=1)
    lab = cv2.connectedComponents(hole, connectivity=4)[1]
    fg[lab[1:-1, 1:-1] != lab[0, 0]] = 255
    fg = cv2.dilate(fg, np.ones((3, 3), np.uint8))
    return np.dstack([a, cv2.GaussianBlur(fg, (3, 3), 0)])


def film(path, pictures, rate_bits):
    h, w = pictures[0].shape[:2]
    c = av.open(str(path), "w", format="mpeg")
    s = c.add_stream("mpeg1video", rate=24)
    s.width, s.height, s.pix_fmt, s.bit_rate = w, h, "yuv420p", rate_bits
    s.codec_context.options = {"qmin": "1", "qmax": "6", "g": "12"}
    for p in pictures:
        for pkt in s.encode(av.VideoFrame.from_ndarray(p, format="rgb24")):
            c.mux(pkt)
    for pkt in s.encode():
        c.mux(pkt)
    c.close()
    print(path.name, w, h, len(pictures), "pictures", path.stat().st_size, "bytes")


def each(f):
    for x in av.open(f).decode(video=0):
        yield x.to_ndarray(format="rgb24")


def main():
    # (one frame in memory at a time: the clips whole do not fit comfortably in a small machine)
    if not (OUT / "zw_bg.mpg").exists() or "--world" in sys.argv:
        world = [cv2.resize(f, (960, 544), interpolation=cv2.INTER_AREA) for f in each(sys.argv[1])]
        film(OUT / "zw_bg.mpg", world, 6_000_000)
        Image.fromarray(cv2.resize(next(each(sys.argv[1])), (1376, 768), interpolation=cv2.INTER_CUBIC)).convert("RGBA").save(OUT / "zw_bg.png")
    box = None
    for n, f in enumerate(each(sys.argv[2])):  # the box he moves in
        if n % 6:
            continue
        ys, xs = np.where(f.max(axis=2) >= 40)
        b = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    x0, y0, x1, y1 = box
    w = 272
    h = int(round((y1 - y0) * w / (x1 - x0) / 16)) * 16
    print("zw_shin", w, h, flush=True)
    stacked = []
    first = None
    for n, f in enumerate(each(sys.argv[2])):
        s = cv2.resize(cut(f)[y0:y1, x0:x1], (w, h), interpolation=cv2.INTER_AREA)
        first = s if first is None else first
        stacked.append(np.concatenate([s[:, :, :3], np.repeat(s[:, :, 3:], 3, axis=2)]))
        if n % 24 == 0:
            print("picture", n, flush=True)
    Image.fromarray(first, "RGBA").save(OUT / "zw_shin.png")
    film(OUT / "zw_shin.mpg", stacked, 7_000_000)


if __name__ == "__main__":
    main()
