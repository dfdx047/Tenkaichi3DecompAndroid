#!/usr/bin/env python3
"""add_song.py - add or remove a song in the music ("BGM") select of the game.

A song is one audio file (any format ffmpeg reads: mp3, wav, ogg, flac, ...). Installing it:
  * converts it to ADX (the format the game's player uses) with ffmpeg,
  * copies the .adx into <data>/songs/,
  * adds a line "<file>|<name>" to <data>/songs/songs.txt (the manifest the port reads at start-up),
  * regenerates <data>/songs/names.rgba, the name strips the port draws in the menu.

Usage (from the repository root, or anywhere - the paths are resolved from this file):

    port/tools/add_song.py add <path/to/song.mp3> "<Display Name>"
    port/tools/add_song.py remove "<Display Name>"
    port/tools/add_song.py list
    port/tools/add_song.py rebuild            # regenerate the name strip only

Options:
    --data <dir>    the data folder (default: gamedata, or $BT3_DATA)
    --font <ttf>    the font for the name strip (default: $BT3_STAGE_FONT, or a known path)
    --rate <hz>     the sample rate for the ADX (default 24000, the game's own)
    --no-strip      only edit the manifest; do not render the name strip

Requires ffmpeg (with the adpcm_adx encoder) for `add`. The name strip needs Pillow and a font;
without them the manifest still works (the tracks appear and play) but the menu shows their names
with the game's own plain font instead of the stylized one.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

# Style of the name strips: match the game's own music bar (a left-aligned name, cream->orange fill,
# dark outline, soft shadow). The strip is 2x of the 512x32 game strip.
STRIP_W, STRIP_H = 1024, 64
FONT_PX = 42
OUTLINE = 4
TRACKING = 4
OUTLINE_COL = (120, 52, 24, 255)
FILL_TOP = (255, 255, 255, 255)
FILL_MID = (255, 238, 205, 255)
FILL_BOT = (245, 168, 66, 255)
SHADOW = (18, 5, 2, 255)
SHADOW_OFF = (4, 5)
SHADOW_BLUR = 6
MARGIN = 16

FONT_CANDIDATES = [
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "compacta.ttf"),
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "C:\\Windows\\Fonts\\arialbd.ttf",
]

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def data_dir(args):
    return args.data or os.environ.get("BT3_DATA") or os.path.join(ROOT, "gamedata")


def slug(name):
    s = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()
    return s or "song"


def read_manifest(path):
    rows = []
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                raw = line.strip()
                if not raw or raw.startswith("#") or "|" not in raw:
                    continue
                f_, n_ = raw.split("|", 1)
                rows.append((f_.strip(), n_.strip()))
    return rows


def write_manifest(path, rows):
    with open(path, "w", encoding="utf-8") as f:
        f.write("# Canciones anadidas a la lista de musica (offset 0x1A en adelante).\n")
        f.write("# <fichero bajo gamedata/songs/>|<nombre mostrado en el menu>\n")
        f.write("# Gestiona este fichero con port/tools/add_song.py "
                "(tambien regenera la tira de nombres del overlay).\n")
        for f_, n_ in rows:
            f.write("%s|%s\n" % (f_, n_))


def find_font(args):
    if args.font:
        return args.font if os.path.exists(args.font) else None
    env = os.environ.get("BT3_STAGE_FONT")
    if env and os.path.exists(env):
        return env
    for c in FONT_CANDIDATES:
        if os.path.exists(c):
            return c
    return None


def convert(src, dst, rate, args):
    ff = args.ffmpeg or "ffmpeg"
    try:
        r = subprocess.run([ff, "-y", "-v", "error", "-i", src, "-ar", str(rate), "-ac", "2",
                            "-c:a", "adpcm_adx", dst], capture_output=True, text=True)
    except FileNotFoundError:
        print("error: ffmpeg not found (pass --ffmpeg or install it).", file=sys.stderr)
        return False
    if r.returncode != 0 or not os.path.exists(dst):
        print("error: ffmpeg failed:\n%s" % (r.stderr or r.stdout), file=sys.stderr)
        return False
    return True


def render_strips(out_path, names, font_path):
    try:
        from PIL import Image, ImageDraw, ImageFont, ImageFilter
    except Exception as e:
        print("note: Pillow is not available (%s); the name strip is not made." % e)
        return False
    if font_path is None:
        print("note: no font found (pass --font <ttf> or set BT3_STAGE_FONT); the name strip is not made.")
        return False

    font = ImageFont.truetype(font_path, FONT_PX)

    def draw_tracked(d, text, x, y, fill=None, stroke=0, stroke_fill=None):
        for ch in text:
            d.text((x, y), ch, font=font, fill=fill, stroke_width=stroke, stroke_fill=stroke_fill)
            x += d.textlength(ch, font=font) + TRACKING

    grad = Image.new("RGBA", (STRIP_W, STRIP_H))
    gd = ImageDraw.Draw(grad)
    for y in range(STRIP_H):
        t = y / (STRIP_H - 1)
        if t < 0.45:
            u = t / 0.45
            col = tuple(int(FILL_TOP[i] * (1 - u) + FILL_MID[i] * u) for i in range(3))
        else:
            u = (t - 0.45) / 0.55
            col = tuple(int(FILL_MID[i] * (1 - u) + FILL_BOT[i] * u) for i in range(3))
        gd.line([(0, y), (STRIP_W, y)], fill=col + (255,))

    imgs = []
    for text in names:
        img = Image.new("RGBA", (STRIP_W, STRIP_H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        box = d.textbbox((0, 0), "Hg", font=font)
        th = box[3] - box[1]
        x = MARGIN
        y = (STRIP_H - th) / 2.0 - box[1]
        for blur, off in ((SHADOW_BLUR, SHADOW_OFF), (3, (2, 3))):
            sh = Image.new("RGBA", (STRIP_W, STRIP_H), (0, 0, 0, 0))
            draw_tracked(ImageDraw.Draw(sh), text, x + off[0], y + off[1], fill=SHADOW,
                         stroke=OUTLINE, stroke_fill=SHADOW)
            img = Image.alpha_composite(img, sh.filter(ImageFilter.GaussianBlur(blur)))
        draw_tracked(ImageDraw.Draw(img), text, x, y, fill=(255, 255, 255, 255),
                     stroke=OUTLINE, stroke_fill=OUTLINE_COL)
        mask = Image.new("L", (STRIP_W, STRIP_H), 0)
        mp, px = mask.load(), img.load()
        for yy in range(STRIP_H):
            for xx in range(STRIP_W):
                r, g, b, a = px[xx, yy]
                if a > 200 and r > 245 and g > 245 and b > 245:
                    mp[xx, yy] = 255
        imgs.append(Image.composite(grad, img, mask))

    strip = Image.new("RGBA", (STRIP_W, STRIP_H * len(imgs)), (0, 0, 0, 0))
    for i, im in enumerate(imgs):
        strip.paste(im, (0, i * STRIP_H))
    with open(out_path, "wb") as f:
        f.write(struct.pack("<III", STRIP_W, STRIP_H, len(imgs)))
        f.write(strip.tobytes())
    print("wrote %s (%d names)" % (out_path, len(imgs)))
    return True


def do_rebuild(data, args):
    mf = os.path.join(data, "songs", "songs.txt")
    rows = read_manifest(mf)
    out = os.path.join(data, "songs", "names.rgba")
    if not rows:
        if os.path.exists(out):
            os.remove(out)
        print("no songs in the manifest; removed the name strip.")
        return 0
    if args.no_strip:
        return 0
    if not render_strips(out, [n for _, n in rows], find_font(args)):
        # a stale strip would index the wrong names: drop it so the game falls back to its own font
        if os.path.exists(out):
            os.remove(out)
    return 0


def cmd_add(args):
    data = data_dir(args)
    songs = os.path.join(data, "songs")
    os.makedirs(songs, exist_ok=True)
    mf = os.path.join(songs, "songs.txt")
    rows = read_manifest(mf)
    if len(rows) >= 6:
        print("error: the game has room for 6 added songs (music-list offsets 0x1A..0x1F).", file=sys.stderr)
        return 1
    if not os.path.exists(args.source):
        print("error: %s does not exist." % args.source, file=sys.stderr)
        return 1
    if any(n == args.name for _, n in rows):
        print("error: a song named %r is already installed." % args.name, file=sys.stderr)
        return 1
    dst = slug(args.name) + ".adx"
    if not convert(args.source, os.path.join(songs, dst), args.rate, args):
        return 1
    rows.append((dst, args.name))
    write_manifest(mf, rows)
    print("installed %s as %s (music-list offset 0x%02x)" % (args.source, dst, 0x1A + len(rows) - 1))
    return do_rebuild(data, args)


def cmd_remove(args):
    data = data_dir(args)
    songs = os.path.join(data, "songs")
    mf = os.path.join(songs, "songs.txt")
    rows = read_manifest(mf)
    hit = [i for i, (_, n) in enumerate(rows) if n == args.name]
    if not hit:
        print("error: no song named %r." % args.name, file=sys.stderr)
        return 1
    for i in reversed(hit):
        f_, _ = rows.pop(i)
        p = os.path.join(songs, f_)
        if os.path.exists(p):
            os.remove(p)
        print("removed %s" % f_)
    write_manifest(mf, rows)
    return do_rebuild(data, args)


def cmd_list(args):
    data = data_dir(args)
    rows = read_manifest(os.path.join(data, "songs", "songs.txt"))
    if not rows:
        print("no added songs.")
        return 0
    for i, (f_, n) in enumerate(rows):
        print("0x%02x  %-24s %s" % (0x1A + i, f_, n))
    return 0


def cmd_rebuild(args):
    return do_rebuild(data_dir(args), args)


def main():
    ap = argparse.ArgumentParser(description="Add or remove a song in the game's music select.")
    ap.add_argument("--data")
    ap.add_argument("--font")
    ap.add_argument("--ffmpeg")
    ap.add_argument("--rate", type=int, default=24000)
    ap.add_argument("--no-strip", action="store_true")
    sub = ap.add_subparsers(dest="cmd", required=True)

    a = sub.add_parser("add")
    a.add_argument("source")
    a.add_argument("name")
    a.set_defaults(func=cmd_add)

    r = sub.add_parser("remove")
    r.add_argument("name")
    r.set_defaults(func=cmd_remove)

    sub.add_parser("list").set_defaults(func=cmd_list)
    sub.add_parser("rebuild").set_defaults(func=cmd_rebuild)
    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
