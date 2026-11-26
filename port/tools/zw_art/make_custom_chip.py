"""Makes the CUSTOM tile of the character select (Z Warriors' custom characters): a 64 x 64 RGBA picture, the shape of the
game's own chips (the face area is drawn by the select; the rest is the reel's frame).

    python port/tools/zw_art/make_custom_chip.py [out_file]

Default output: port/tools/zw_art/zw_custom_chip.raw. A release puts it at <game folder>/gamedata/zw_custom/chip_photo.raw
(a game without it still works: the tile has no picture of its own then)."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
S = 256


def font(size):
    for f in ('C:/Windows/Fonts/ariblk.ttf', 'C:/Windows/Fonts/arialbd.ttf', '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'zw_custom_chip.raw')
    im = Image.new('RGBA', (S, S))
    d = ImageDraw.Draw(im)
    for y in range(S):
        t = y / S
        d.line([(0, y), (S, y)], fill=(int(120 - 60 * t), int(60 - 30 * t), int(200 - 80 * t), 255))
    for r in range(0, 180, 6):
        d.ellipse([S / 2 - r, S / 2 - r - 20, S / 2 + r, S / 2 + r - 20], outline=(150, 110, 255, 60), width=2)
    d.ellipse([S / 2 - 62, 34, S / 2 + 62, 60], outline=(255, 215, 90, 255), width=8)
    f = font(44)
    txt = 'CUSTOM'
    x, y = (S - d.textlength(txt, font=f)) / 2, 104
    for dx in range(-6, 7, 2):
        for dy in range(-6, 7, 2):
            d.text((x + dx, y + dy), txt, font=f, fill=(60, 20, 10, 255))
    d.text((x, y), txt, font=f, fill=(255, 225, 110, 255))
    f2 = font(28)
    t2 = 'WARRIORS'
    w2 = d.textlength(t2, font=f2)
    for dx in (-3, 0, 3):
        for dy in (-3, 0, 3):
            d.text(((S - w2) / 2 + dx, 176 + dy), t2, font=f2, fill=(30, 10, 60, 255))
    d.text(((S - w2) / 2, 176), t2, font=f2, fill=(255, 255, 255, 255))
    d.rectangle([3, 3, S - 4, S - 4], outline=(255, 210, 80, 255), width=8)
    open(out, 'wb').write(im.resize((64, 64), Image.LANCZOS).tobytes())
    print('wrote', out)


main()
