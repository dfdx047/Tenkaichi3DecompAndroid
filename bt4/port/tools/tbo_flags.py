#!/usr/bin/env python3
"""The countries' flags of the online screens (BT4 Android's Profile and Global Top, src/menu/tbo_look.inc): one picture,
port/tbo/ui/flags.png, every flag in a cell of CELL_W x CELL_H (3:2) less a transparent pixel all round (a flag of another
shape fits inside, centred, the rest transparent), COLS a row, in the order of include/menu/tbo_flags.h's list of ISO 3166-1 codes (written here too).

    python3 port/tools/tbo_flags.py [--cache DIR]

The flags are flagcdn.com's PNGs (Wikimedia Commons' flags: public domain), fetched once into the cache folder.
"""
import argparse, io, json, pathlib, sys, urllib.request

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
CELL_W, CELL_H, COLS = 72, 48, 14
SIZE = 1024
UA = {"User-Agent": "tbo-flags-builder"}


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=30) as r:
        return r.read()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=str(ROOT / "port/build/flags-cache"))
    a = ap.parse_args()
    cache = pathlib.Path(a.cache)
    cache.mkdir(parents=True, exist_ok=True)
    codes_file = cache / "codes.json"
    if not codes_file.exists():
        codes_file.write_bytes(fetch("https://flagcdn.com/en/codes.json"))
    names = json.loads(codes_file.read_text())
    codes = sorted(c for c in names if len(c) == 2 and c.isalpha())  # (the countries: not "us-ca", "gb-eng"...)
    rows = (len(codes) + COLS - 1) // COLS
    if COLS * CELL_W > SIZE or rows * CELL_H > SIZE:
        sys.exit(f"{len(codes)} flags do not fit {SIZE} x {SIZE}")
    atlas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    for i, c in enumerate(codes):
        f = cache / f"{c}.png"
        if not f.exists():
            f.write_bytes(fetch(f"https://flagcdn.com/w160/{c}.png"))
        im = Image.open(io.BytesIO(f.read_bytes())).convert("RGBA")
        scale = min((CELL_W - 2) / im.width, (CELL_H - 2) / im.height)  # (a pixel of nothing round each: no neighbour bleeds in)
        w, h = max(1, round(im.width * scale)), max(1, round(im.height * scale))
        im = im.resize((w, h), Image.LANCZOS)
        x, y = (i % COLS) * CELL_W + (CELL_W - w) // 2, (i // COLS) * CELL_H + (CELL_H - h) // 2
        atlas.alpha_composite(im, (x, y))
    out = ROOT / "port/tbo/ui/flags.png"
    atlas.save(out, optimize=True)
    lines = ["/* The countries of port/tbo/ui/flags.png, in its order (written by port/tools/tbo_flags.py: do not edit). A",
             f"   flag is the cell CELL_W x CELL_H at (i % TBO_FLAG_COLS, i / TBO_FLAG_COLS) of a {SIZE} x {SIZE} picture. */",
             "#ifndef TBO_FLAGS_H", "#define TBO_FLAGS_H",
             f"#define TBO_FLAG_COLS {COLS}", f"#define TBO_FLAG_CELL_W {CELL_W}", f"#define TBO_FLAG_CELL_H {CELL_H}",
             f"#define TBO_FLAG_SIZE {SIZE}", f"#define TBO_FLAG_COUNT {len(codes)}",
             "static const char kTboFlagCodes[TBO_FLAG_COUNT][3] = {"]
    for k in range(0, len(codes), 16):
        lines.append("    " + ", ".join(f'"{c.upper()}"' for c in codes[k:k + 16]) + ",")
    lines += ["};", "#endif", ""]
    (ROOT / "include/menu/tbo_flags.h").write_text("\n".join(lines))
    print(f"{len(codes)} flags -> {out} ({out.stat().st_size // 1024} KB), include/menu/tbo_flags.h")


if __name__ == "__main__":
    main()
