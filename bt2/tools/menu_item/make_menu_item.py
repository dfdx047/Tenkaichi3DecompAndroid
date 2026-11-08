#!/usr/bin/env python3
"""The pictures of Dragon Rage's "Online Mode" item and screen in BT2's main menu (runtime/game/main_menu_item.cpp).

Every picture is drawn here: nothing of them comes from the game's disc. The lettering is a heavy italic face with a
yellow ramp and a navy and a white outline, like the game's but not the same (Ubuntu Sans, SIL OFL); the fighter is
the picture the project's author gave (cut out of its white background); the rest is drawn with shapes and
gradients. What the game itself holds, the item's plate and its icon, is made on the phone from the game's own data
while it runs (main_menu_item.cpp), so none of it is kept anywhere.

    python3 bt2/tools/menu_item/make_menu_item.py --bust <fighter bust.png> [--out <folder>]
    python3 bt2/tools/menu_item/make_menu_item.py --fighter <full-length picture on white> [--out <folder>]

Writes item_text.png, plate_text.png, online.png, ranked.png, rooms_create.png, rooms_join.png, rooms_quick.png, wait.png,
left.png, soon.png and description.png into the folder (default:
android/app/src/main/assets/bt2/menu-item, which the app copies to <root>/bt2/user/menu-item at each start).
Needs Pillow and numpy; the fonts below (any heavy italic face and a monospace one will do: --font-italic, --font-mono).
"""
import argparse
import math
import pathlib
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[3]
NAVY, WHITE = (1, 55, 106), (255, 255, 255)
FONT_ITALIC = "/usr/share/fonts/truetype/ubuntu/UbuntuSans-Italic[wdth,wght].ttf"
FONT_SANS = "/usr/share/fonts/truetype/ubuntu/Ubuntu[wdth,wght].ttf"
FONT_MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
SCREEN = (1280, 720)


def face(path, size, axes=None, name=None):
    f = ImageFont.truetype(path, size)
    try:
        if axes:
            f.set_variation_by_axes(axes)
        elif name:
            f.set_variation_by_name(name)
    except (OSError, AttributeError):
        pass  # (a font without those axes: its own weight)
    return f


# ---- the lettering ----------------------------------------------------------------------------------------------

CAP, BASE, LEFT = 22, 41, 17  # a 512 x 64 row of lettering: capital height, baseline row, where the text starts


def yellow(t):
    """The fill's ramp, top (t = 0) to bottom (t = 1): near-white yellow to orange."""
    t = min(1.0, max(0.0, t))
    return (255, round(253 - 66 * t), 1 if t < 0.9 else 2)


def lettering(text, scale=1.0, origin=(0, 0), size=(512, 64)):
    """`text` as a row of the menu's lettering: RGBA of `size`, the text `scale` times as large, at `origin`."""
    S = 4  # (drawn four times larger and brought down: soft edges)
    W, H = size
    ref = face(FONT_ITALIC, 100, axes=[78, 800])
    box = ref.getbbox("H")
    px = CAP * scale * S / ((box[3] - box[1]) / 100)
    f = face(FONT_ITALIC, px, axes=[78, 800])
    mask = Image.new("L", (W * S, H * S), 0)
    draw = ImageDraw.Draw(mask)
    x = (origin[0] + LEFT * scale) * S
    base = (origin[1] + BASE * scale) * S
    for ch in text:  # (a little air between the letters: the outlines run into each other)
        draw.text((x, base), ch, font=f, fill=255, anchor="ls", stroke_width=max(1, round(0.7 * scale * S)))  # (a little heavier)
        x += f.getlength(ch) + 1.4 * scale * S
    cov = np.asarray(mask.resize((W, H), Image.BOX), dtype=np.float32) / 255.0

    # the outlines: navy within 3.2 pixels of the fill, white within 5.6 (the distance to the nearest fill pixel)
    r1, r2 = 3.2 * scale, 5.6 * scale
    reach = int(math.ceil(r2 + 0.5))
    solid = np.pad(cov >= 0.5, reach)
    dist = np.full((H, W), 99.0, np.float32)
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            d = math.hypot(dx, dy)
            if d > reach:
                continue
            seen = solid[reach - dy:reach - dy + H, reach - dx:reach - dx + W]
            dist = np.where(seen & (d < dist), np.float32(d), dist)
    white = np.clip(r2 + 0.5 - dist, 0, 1)
    navy = np.clip(r1 + 0.5 - dist, 0, 1)
    rows = np.array([yellow((y - (origin[1] + (BASE - CAP) * scale)) / (CAP * scale)) for y in range(H)], dtype=np.float32)
    fill = np.broadcast_to(rows[:, None, :], (H, W, 3))
    base_col = np.array(WHITE, np.float32) * (1 - navy[..., None]) + np.array(NAVY, np.float32) * navy[..., None]
    col = base_col * (1 - cov[..., None]) + fill * cov[..., None]
    alpha = np.maximum(white, cov)
    out = np.dstack([np.rint(col), np.rint(alpha * 255)]).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def trimmed(img):
    return img.crop(img.getbbox())


def outlined(text, font, fill, outline, width):
    """Plain text with an outline, for the hints."""
    pad = width + 2
    w = int(font.getlength(text)) + 2 * pad
    h = int(font.size * 1.5) + 2 * pad
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(im).text((pad, pad), text, font=font, fill=fill + (255,), stroke_width=width, stroke_fill=outline + (255,))
    return trimmed(im)


# ---- the screen's pieces ----------------------------------------------------------------------------------------

def pill(w, h, selected):
    """A menu bar: rounded, dark blue on the left and fading to nothing on the right (the chosen one is brighter)."""
    S = 3
    im = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    xs = np.linspace(0, 1, w * S, dtype=np.float32)
    left = np.array((8, 10, 92) if selected else (14, 18, 96), np.float32)
    right = np.array((156, 146, 218) if selected else (60, 70, 160), np.float32)
    col = left[None, :] * (1 - xs[:, None]) + right[None, :] * xs[:, None]
    alpha = np.clip(1.15 - 0.95 * xs, 0, 1) if selected else np.clip(1.0 - 0.8 * xs, 0.25, 1)
    strip = np.dstack([np.broadcast_to(col[None, :, :], (h * S, w * S, 3)), np.broadcast_to((alpha * 255)[None, :, None], (h * S, w * S, 1))])
    body = Image.fromarray(strip.astype(np.uint8), "RGBA")
    shape = Image.new("L", im.size, 0)
    ImageDraw.Draw(shape).rounded_rectangle([0, 0, w * S - 1, h * S - 1], radius=h * S // 2, fill=255)
    im.paste(body, (0, 0), shape)
    edge = Image.new("L", im.size, 0)
    ImageDraw.Draw(edge).rounded_rectangle([0, 0, w * S - 1, h * S - 1], radius=h * S // 2, outline=255, width=2 * S)
    ring = Image.new("RGBA", im.size, (120, 160, 255, 255))
    fade = Image.fromarray((np.clip(1.1 - 1.0 * xs, 0.15, 1)[None, :] * 255 * np.ones((h * S, 1))).astype(np.uint8), "L")
    edge = Image.composite(edge, Image.new("L", im.size, 0), fade)
    im.paste(ring, (0, 0), edge)
    return im.resize((w, h), Image.LANCZOS)


def button(kind, size=34):
    """A pad button for the hints: a grey disc with a cross or a triangle."""
    S = 4
    n = size * S
    im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([2 * S, 2 * S, n - 2 * S, n - 2 * S], fill=(30, 34, 60, 255), outline=(235, 235, 245, 255), width=3 * S)
    c, r = n / 2, n * 0.22
    if kind == "cross":
        for a, b in (((c - r, c - r), (c + r, c + r)), ((c - r, c + r), (c + r, c - r))):
            d.line([a, b], fill=(110, 150, 255, 255), width=int(2.6 * S))
    else:
        pts = [(c, c - r * 1.1), (c + r * 1.1, c + r * 0.8), (c - r * 1.1, c + r * 0.8)]
        d.polygon(pts, outline=(70, 215, 130, 255), width=int(2.6 * S))
    return im.resize((size, size), Image.LANCZOS)


def hints():
    f = face(FONT_ITALIC, 30, axes=[90, 800])
    parts = [button("cross"), outlined("Decidir", f, (255, 255, 255), NAVY, 3), button("triangle"), outlined("Voltar", f, (255, 255, 255), NAVY, 3)]
    gap = 10
    w = sum(p.width for p in parts) + gap * 4 + 34
    im = Image.new("RGBA", (w, 44), (0, 0, 0, 0))
    x = 0
    for i, p in enumerate(parts):
        im.alpha_composite(p, (x, (44 - p.height) // 2))
        x += p.width + gap + (34 if i == 1 else 0)
    return im


def background(title):
    W, H = SCREEN
    random.seed(7)
    LX, LY = 980, 300
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    t = yy / H
    l2 = np.maximum(0, 1 - np.hypot((xx - LX) / 700, (yy - LY) / 520)) ** 2
    rgb = np.dstack([np.minimum(255, np.floor(10 + 30 * t + 110 * l2)),
                     np.minimum(255, np.floor(14 + 10 * t + 80 * l2)),
                     np.minimum(255, np.floor(60 + 60 * (1 - t) + 150 * l2))]).astype(np.uint8)
    bg = Image.fromarray(rgb, "RGB").convert("RGBA")
    rays = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    rd = ImageDraw.Draw(rays)
    for i in range(28):
        a, w = i / 28 * 2 * math.pi + 0.1, 0.035
        rd.polygon([(LX, LY), (LX + 1800 * math.cos(a - w), LY + 1800 * math.sin(a - w)),
                    (LX + 1800 * math.cos(a + w), LY + 1800 * math.sin(a + w))], fill=(170, 190, 255, 22))
    bg.alpha_composite(rays.filter(ImageFilter.GaussianBlur(6)))
    lines = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ld = ImageDraw.Draw(lines)
    for i in range(60):
        y, x0, length = random.randint(0, H), random.randint(-200, W), random.randint(150, 600)
        ld.line([(x0, y), (x0 + length, y)], fill=(200, 210, 255, random.randint(18, 50)), width=random.choice((1, 2, 3)))
    bg.alpha_composite(lines.filter(ImageFilter.GaussianBlur(1.2)))
    sp = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sp)
    for i in range(90):
        x, y, r = random.randint(0, W), random.randint(0, H), random.choice((1, 1, 2, 3))
        sd.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, random.randint(60, 200)))
    bg.alpha_composite(sp.filter(ImageFilter.GaussianBlur(0.6)))
    return bg  # (the title band is its own picture, title_sprite: the game moves it)


def title_sprite(title):
    """The band with the screen's title, on its own (transparent around it), 700 x 120, to be drawn at (0, 20) of the 1280 x 720 screen and made to float."""
    im = Image.new("RGBA", (700, 120), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle([-40, 10, 640, 100], radius=44, fill=(10, 14, 70, 200), outline=(120, 150, 255, 255), width=4)
    t = title.resize((int(title.width * 1.7), int(title.height * 1.7)), Image.LANCZOS)
    im.alpha_composite(t, (20, 0))
    return im


def add_fighter(bg, bust, draw_figure=True):
    W, H = SCREEN
    k = 600 / bust.height
    ch = bust.resize((int(bust.width * k), 600), Image.LANCZOS)
    cx, cy = W - ch.width - 30, H - ch.height - 50
    m = Image.new("L", (W, H), 0)
    m.paste(ch.split()[3], (cx, cy))
    m = m.filter(ImageFilter.MaxFilter(21)).filter(ImageFilter.GaussianBlur(22))
    glow = Image.new("RGBA", (W, H), (130, 180, 255, 0))
    glow.putalpha(m.point(lambda v: int(v * 0.85)))
    bg.alpha_composite(glow)
    if draw_figure:
        bg.alpha_composite(ch, (cx, cy))   # (an animated fighter is drawn by the game over the screen: make_fighter_video.py)


DESCRIPTIONS = [("Lute contra outros jogadores pela internet.", "Salas com código e partidas rápidas."),
                ("Partidas que valem pontos no ranking.", "Suba de posição e mostre a sua força!")]
ROOM_DESCRIPTIONS = [("Crie uma sala e passe o código", "para um amigo entrar."),
                     ("Digite o código da sala de um", "amigo para jogar com ele."),
                     ("Procure um oponente online que", "tenha o jogo na mesma versão.")]


def screen(choice, bg, labels, hint, descriptions=DESCRIPTIONS, top=200, step=110, window=True):
    im = bg.copy()
    W, H = SCREEN
    bar = pill(600, 84, True)
    for i, lab in enumerate(labels):
        y = top + i * step
        text = trimmed(lab)
        text = text.resize((int(text.width * 1.5), int(text.height * 1.5)), Image.LANCZOS)
        if i == choice:
            im.alpha_composite(bar, (30, y + 6))
        else:
            p = Image.new("RGBA", (W, H), (0, 0, 0, 0))
            ImageDraw.Draw(p).rounded_rectangle([46, y + 14, 600, y + 82], radius=34, fill=(20, 26, 90, 170), outline=(70, 90, 190, 255), width=3)
            im.alpha_composite(p)
            text.putalpha(text.split()[3].point(lambda v: int(v * 0.8)))
        im.alpha_composite(text, (80, y + 6 + (84 - text.height) // 2 - 6))
    if window:
        win = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(win).rounded_rectangle([40, 520, 700, 640], radius=18, fill=(40, 20, 90, 215), outline=(200, 190, 255, 255), width=3)
        im.alpha_composite(win)
        d = ImageDraw.Draw(im)
        f = face(FONT_SANS, 30, name="Bold")
        d.text((66, 540), descriptions[choice][0], font=f, fill=(255, 255, 255, 255))
        d.text((66, 584), descriptions[choice][1], font=f, fill=(210, 210, 240, 255))
    im.alpha_composite(hint, (W - hint.width - 30, H - hint.height - 18))
    return im


def soon(label):
    im = Image.new("RGBA", (720, 330), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([6, 6, 714, 324], radius=30, fill=(16, 18, 70, 235), outline=(255, 210, 90, 255), width=5)
    text = trimmed(label)
    text = text.resize((text.width * 2, text.height * 2), Image.LANCZOS)
    im.alpha_composite(text, ((720 - text.width) // 2, 40))
    f = face(FONT_SANS, 30, name="Bold")
    for i, t in enumerate(("O modo online ainda está em desenvolvimento.", "Aguarde as próximas atualizações!")):
        d.text(((720 - d.textlength(t, font=f)) / 2, 170 + i * 44), t, font=f, fill=(255, 255, 255, 255))
    return im


def left():
    """The whole frame black with the message of an opponent who left or was lost (shown for a moment, then back to the Online screen)."""
    im = Image.new("RGBA", (640, 448), (0, 0, 0, 255))
    d = ImageDraw.Draw(im)
    f = face(FONT_SANS, 30, name="Bold")
    g = face(FONT_SANS, 22, name="Regular")
    for text, font, y in (("O oponente saiu ou foi desconectado.", f, 190), ("Voltando ao Modo Online…", g, 240)):
        d.text(((640 - d.textlength(text, font=font)) / 2, y), text, font=font, fill=(255, 255, 255, 255))
    return im


def description():
    """The menu's message window, with the item's line: the window's own lines are about 42.6 screen pixels a letter."""
    W, H = 1090, 120
    f = ImageFont.truetype(FONT_MONO, 52)
    adv = f.getlength("M")
    canvas = Image.new("RGBA", (int(adv * 26) + 20, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(canvas)
    for i, t in enumerate(("Lute contra jogadores do", "mundo todo pela internet!")):
        d.text((8, 10 + i * 50), t, font=f, fill=(236, 236, 236, 255), stroke_width=3, stroke_fill=(28, 28, 40, 255))
    canvas = canvas.resize((int(canvas.width * 42.6 / adv), H), Image.LANCZOS)
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    im.alpha_composite(canvas.crop((0, 0, min(W, canvas.width), H)), (0, 0))
    return im


# ---- the fighter ------------------------------------------------------------------------------------------------

def cut_out(path, waist=500, fade=50):
    """The figure of a picture on a white background, from the top down to `waist` rows (a soft end), the soft edge
    kept: the white that touches the border is the background, the largest piece left is the figure."""
    im = Image.open(path).convert("RGB")
    W, H = im.size
    a = np.asarray(im).astype(np.int32)
    white = (a > 225).all(axis=2)
    bg = np.zeros((H, W), bool)
    stack = [(x, 0) for x in range(W)] + [(x, H - 1) for x in range(W)] + [(0, y) for y in range(H)] + [(W - 1, y) for y in range(H)]
    while stack:
        x, y = stack.pop()
        if 0 <= x < W and 0 <= y < H and white[y, x] and not bg[y, x]:
            bg[y, x] = True
            stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    labels = np.zeros((H, W), np.int32)
    sizes = [0]
    for y0 in range(H):
        for x0 in range(W):
            if bg[y0, x0] or labels[y0, x0]:
                continue
            cid = len(sizes)
            sizes.append(0)
            labels[y0, x0] = cid
            todo = [(x0, y0)]
            while todo:
                x, y = todo.pop()
                sizes[cid] += 1
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < W and 0 <= ny < H and not bg[ny, nx] and not labels[ny, nx]:
                        labels[ny, nx] = cid
                        todo.append((nx, ny))
    main = max(range(1, len(sizes)), key=lambda c: sizes[c])
    edge = np.zeros((H, W), bool)
    edge[1:, :] |= bg[:-1, :]; edge[:-1, :] |= bg[1:, :]; edge[:, 1:] |= bg[:, :-1]; edge[:, :-1] |= bg[:, 1:]
    alpha = np.where(labels == main, 255, 0)
    soft = np.clip((255 - a.min(axis=2)) * 2.2, 0, 255)
    alpha = np.where(edge & (labels == main), soft, alpha).astype(np.uint8)
    out = Image.fromarray(np.dstack([a.astype(np.uint8), alpha]), "RGBA")
    out = out.crop(out.getbbox())
    bust = out.crop((0, 0, out.width, waist))
    p = bust.load()
    for y in range(waist - fade, waist):
        f = (waist - y) / fade
        for x in range(bust.width):
            r, g, b, al = p[x, y]
            p[x, y] = (r, g, b, int(al * f))
    return trimmed(bust)


def main():
    global FONT_ITALIC, FONT_SANS, FONT_MONO
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bust", type=pathlib.Path, help="the fighter, already cut out (RGBA, from the waist up)")
    ap.add_argument("--fighter", type=pathlib.Path, help="the fighter's full-length picture on a white background")
    ap.add_argument("--animated", action="store_true", help="the fighter is a looping clip the game draws: only its glow goes into the screens")
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "android/app/src/main/assets/bt2/menu-item")
    ap.add_argument("--font-italic", default=FONT_ITALIC)
    ap.add_argument("--font-sans", default=FONT_SANS)
    ap.add_argument("--font-mono", default=FONT_MONO)
    args = ap.parse_args()
    FONT_ITALIC, FONT_SANS, FONT_MONO = args.font_italic, args.font_sans, args.font_mono
    if args.bust:
        bust = Image.open(args.bust).convert("RGBA")
    elif args.fighter:
        bust = cut_out(args.fighter)
    else:
        sys.exit("give the fighter: --bust or --fighter")
    args.out.mkdir(parents=True, exist_ok=True)

    name = lettering("Online Mode")
    labels = [lettering("Online"), lettering("Ranqueado")]
    rooms = [lettering("Criar Sala"), lettering("Entrar em Sala"), lettering("Partida Rápida")]
    bg = background(name)
    add_fighter(bg, bust, draw_figure=not args.animated)
    hint = hints()
    pictures = {
        "item_text": name,                                                         # the chosen item's name (a row of the sheet of names)
        "plate_text": lettering("Online Mode", scale=0.75, origin=(36, 9)),        # its word on the plate (a row of the sheet of plates)
        "online": screen(0, bg, labels, hint),
        "ranked": screen(1, bg, labels, hint),
        # Online chosen: the three ways in (create a room, join one by its code, find an opponent)
        "rooms_create": screen(0, bg, rooms, hint, ROOM_DESCRIPTIONS, top=150, step=100),
        "rooms_join": screen(1, bg, rooms, hint, ROOM_DESCRIPTIONS, top=150, step=100),
        "rooms_quick": screen(2, bg, rooms, hint, ROOM_DESCRIPTIONS, top=150, step=100),
        # the app is making the connection: the background alone, the app puts what it knows (code, search, opponent) over it
        "wait": screen(0, bg, [], hint, window=False),
        "soon": soon(lettering("Em breve!")),
        "left": left(),
        "title": title_sprite(name),
        "shade": Image.new("RGBA", (16, 16), (0, 0, 0, 255)),  # dims the entries of Dueling's menu that are not open while an opponent is connected
        "description": description(),
    }
    for key, im in pictures.items():
        im.save(args.out / f"{key}.png", optimize=True)
        print(f"{key}.png {im.width}x{im.height}")


if __name__ == "__main__":
    main()
