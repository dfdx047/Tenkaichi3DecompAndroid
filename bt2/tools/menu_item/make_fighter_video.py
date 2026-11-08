#!/usr/bin/env python3
"""The fighter of the Online Mode screens as a looping video: a clip of a figure on a white background becomes a row of cut-out pictures.

    python3 bt2/tools/menu_item/make_fighter_video.py <clip.mp4> [--out <folder>] [--fps 12]

Needs GStreamer (gst-launch-1.0 with the usual decoders) to read the clip; its sound is not used. Writes into the screens' folder
(default android/app/src/main/assets/bt2/menu-item): gohan_00.png, gohan_01.png, ... (the figure from the top down to the belt, a soft end,
the white background and anything else that is not the figure taken out, 8-bit palette so a frame is a few tens of KB) and gohan.txt
("frames fps width height", the numbers the game reads), and bust.png (the first frame, for make_menu_item.py --bust --animated, which
draws the screens' glow behind it but not the figure: the game draws the frames over them).
"""
import argparse
import pathlib
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = pathlib.Path(__file__).resolve().parents[3]
HEIGHT = 600            # the figure's height in the 1280 x 720 screen (the same as the still picture had)
BELT = 0.66             # where the figure ends, as a part of the clip's height (the belt); a soft end of FADE rows follows up to it
FADE = 28


def decode(clip, fps, folder):
    """The clip's frames as PNG at `fps`, scaled so the figure down to the belt is about HEIGHT tall."""
    probe = subprocess.run(["gst-discoverer-1.0", str(clip)], capture_output=True, text=True).stdout
    width = height = 0
    for line in probe.splitlines():
        line = line.strip()
        if line.startswith("Width:"): width = int(line.split()[1])
        if line.startswith("Height:"): height = int(line.split()[1])
    if not width or not height:
        sys.exit("cannot read the clip's size (is GStreamer installed?)")
    scale = HEIGHT / (height * BELT)
    w, h = int(width * scale) // 2 * 2, int(height * scale) // 2 * 2
    subprocess.run(["gst-launch-1.0", "-q", "filesrc", f"location={clip}", "!", "decodebin", "!", "videoconvert", "!", "videorate", "!",
                    f"video/x-raw,framerate={fps}/1", "!", "videoscale", "!", f"video/x-raw,width={w},height={h}", "!", "pngenc", "!",
                    "multifilesink", f"location={folder}/f_%03d.png"], check=True)
    return sorted(folder.glob("f_*.png"))


def cut(path):
    """The figure of one frame as RGBA: the white that touches the border is the background, the largest piece left is the figure
    (so the marks in the corners go with the background); the edge is soft; the figure ends at the belt with a soft end."""
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.int32)
    h, w = a.shape[:2]
    white = (a > 225).all(axis=2)
    lab, _ = ndimage.label(white)
    border = np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))
    bg = np.isin(lab, border[border > 0])
    fg = ~bg
    flab, n = ndimage.label(fg)
    sizes = ndimage.sum(fg, flab, range(1, n + 1))
    main = flab == (1 + int(np.argmax(sizes)))
    edge = ndimage.binary_dilation(bg, iterations=1) & main
    soft = np.clip((255 - a.min(axis=2)) * 2.2, 0, 255)
    alpha = np.where(main, 255, 0)
    alpha = np.where(edge, soft, alpha).astype(np.uint8)
    belt = int(h * BELT)
    alpha[belt:] = 0
    for y in range(belt - FADE, belt):
        alpha[y] = (alpha[y] * ((belt - y) / FADE)).astype(np.uint8)
    return Image.fromarray(np.dstack([a.astype(np.uint8), alpha]), "RGBA").crop((0, 0, w, belt))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("clip", type=pathlib.Path)
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "android/app/src/main/assets/bt2/menu-item")
    ap.add_argument("--fps", type=int, default=12)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        frames = [cut(p) for p in decode(args.clip, args.fps, pathlib.Path(tmp))]
    # one box for all the frames, so the figure does not jump from frame to frame
    box = None
    for f in frames:
        b = f.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    frames = [f.crop(box) for f in frames]
    sw = round(frames[0].width * HEIGHT / frames[0].height)
    frames = [f.resize((sw, HEIGHT), Image.LANCZOS) for f in frames]
    for old in args.out.glob("gohan_*.png"):
        old.unlink()
    total = 0
    for i, f in enumerate(frames):
        small = f.quantize(colors=160, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
        path = args.out / f"gohan_{i:02d}.png"
        small.save(path, optimize=True)
        total += path.stat().st_size
    (args.out / "gohan.txt").write_text(f"{len(frames)} {args.fps} {sw} {HEIGHT}\n")
    frames[0].save(args.out.parent.parent / "bust_video.png") if False else frames[0].save(args.out / "bust.png")
    print(f"{len(frames)} frames of {sw} x {HEIGHT}, {total // 1024} KB in all")


if __name__ == "__main__":
    main()
