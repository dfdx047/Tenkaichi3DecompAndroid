"""BT4 Online - tela de Perfil (cartão do jogador, lutador mais usado, troféus, medalhas, top).
uso: python3 perfil.py <scratchpad> [anim]"""
import sys, os, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = sys.argv[1]
ANIM = len(sys.argv) > 2
W, H = 2200, 990
K = 2                                  # supersample
FD = S + '/fonts/'
NAVY = (16, 26, 62); NAVY2 = (34, 52, 110); GOLD = (255, 204, 64); GOLD2 = (255, 160, 30)
WHITE = (255, 255, 255); SOFT = (196, 210, 240); DIM = (130, 142, 175)
_f = {}
def font(name, size):
    key = (name, size)
    if key not in _f:
        fn = {'black': 'Kanit-BlackItalic.ttf', 'semi': 'Kanit-SemiBoldItalic.ttf', 'bold': 'Kanit-Bold.ttf',
              'chak': 'ChakraPetch-SemiBold.ttf', 'chakm': 'ChakraPetch-Medium.ttf'}[name]
        _f[key] = ImageFont.truetype(FD + fn, int(size * K))
    return _f[key]

def P(*v): return [x * K for x in v]

class Pic:
    def __init__(self, base): self.im = base.convert('RGBA')
    def layer(self): return Image.new('RGBA', self.im.size, (0, 0, 0, 0))
    def put(self, l, blur=0):
        if blur: l = l.filter(ImageFilter.GaussianBlur(blur * K))
        self.im.alpha_composite(l)

def grad_img(w, h, stops, vertical=True):
    t = np.linspace(0, 1, h if vertical else w)
    cols = np.zeros((len(t), 4))
    for i in range(len(stops) - 1):
        (p0, c0), (p1, c1) = stops[i], stops[i + 1]
        m = (t >= p0) & (t <= p1)
        u = ((t[m] - p0) / max(p1 - p0, 1e-6))[:, None]
        cols[m] = np.array(c0) * (1 - u) + np.array(c1) * u
    arr = cols[:, None, :].repeat(w, 1) if vertical else cols[None, :, :].repeat(h, 0)
    return Image.fromarray(arr.astype(np.uint8), 'RGBA')

def rr(pic, box, r, stops, border=None, bw=2, vertical=True, shadow=0):
    x0, y0, x1, y1 = [int(v * K) for v in box]
    w, h = x1 - x0, y1 - y0
    if shadow:
        l = pic.layer(); ImageDraw.Draw(l).rounded_rectangle((x0, y0 + 8 * K, x1, y1 + 8 * K), r * K, fill=(0, 0, 0, shadow)); pic.put(l, 10)
    g = grad_img(w, h, stops, vertical)
    m = Image.new('L', (w, h), 0); ImageDraw.Draw(m).rounded_rectangle((0, 0, w - 1, h - 1), r * K, fill=255)
    l = pic.layer(); l.paste(g, (x0, y0), m); pic.put(l)
    if border:
        l = pic.layer(); ImageDraw.Draw(l).rounded_rectangle((x0, y0, x1, y1), r * K, outline=border, width=int(bw * K)); pic.put(l)

def glass(pic, box, r=30, gold=False):
    rr(pic, box, r, [(0, NAVY2 + (225,)), (1, NAVY + (235,))], (GOLD + (200,)) if gold else (150, 180, 255, 70), 3 if gold else 2, shadow=120)
    x0, y0, x1, y1 = box   # brilho no alto
    l = pic.layer(); ImageDraw.Draw(l).rounded_rectangle(P(x0 + 3, y0 + 3, x1 - 3, y0 + 60), (r - 3) * K, fill=(255, 255, 255, 14)); pic.put(l)

def text(pic, xy, s, f, size, fill, anchor='lm', shadow=3, stroke=0, stroke_fill=(0, 0, 0, 255)):
    x, y = xy
    if shadow:
        l = pic.layer(); ImageDraw.Draw(l).text(((x + shadow * 0.6) * K, (y + shadow) * K), s, font=font(f, size), fill=(0, 0, 20, 170), anchor=anchor, stroke_width=stroke * K, stroke_fill=(0, 0, 20, 170)); pic.put(l, 1.5)
    l = pic.layer(); ImageDraw.Draw(l).text((x * K, y * K), s, font=font(f, size), fill=fill, anchor=anchor, stroke_width=stroke * K, stroke_fill=stroke_fill); pic.put(l)

def text_w(s, f, size): return font(f, size).getlength(s) / K

def gold_text(pic, xy, s, f, size, anchor='lm'):
    x, y = xy
    text(pic, xy, s, f, size, (0, 0, 0, 0), anchor, shadow=4, stroke=3, stroke_fill=(40, 20, 0, 255))
    m = Image.new('L', pic.im.size, 0); ImageDraw.Draw(m).text((x * K, y * K), s, font=font(f, size), fill=255, anchor=anchor)
    bb = m.getbbox(); g = grad_img(bb[2] - bb[0], bb[3] - bb[1], [(0, (255, 244, 180, 255)), (0.5, GOLD + (255,)), (1, GOLD2 + (255,))])
    l = pic.layer(); l.paste(g, bb[:2], m.crop(bb)); pic.put(l)

def chip(pic, x, y, s, size=22, gold=False, anchor='l'):
    w = text_w(s, 'chak', size) + 34; h = size + 18
    x0 = x if anchor == 'l' else (x - w / 2 if anchor == 'm' else x - w)
    if gold:
        rr(pic, (x0, y - h / 2, x0 + w, y + h / 2), h / 2, [(0, (255, 226, 120, 255)), (1, GOLD2 + (255,))], (255, 245, 200, 255), 1.5)
        text(pic, (x0 + w / 2, y + 1), s, 'chak', size, (50, 25, 0, 255), 'mm', shadow=0)
    else:
        rr(pic, (x0, y - h / 2, x0 + w, y + h / 2), h / 2, [(0, (255, 255, 255, 30)), (1, (255, 255, 255, 14))], (255, 255, 255, 90), 1.5)
        text(pic, (x0 + w / 2, y + 1), s, 'chak', size, SOFT + (255,), 'mm', shadow=0)
    return w

# ------------------------------------------------------------- bandeiras (simplificadas)
def flag(pic, x, y, w, code):
    h = w * 0.68; f = Image.new('RGBA', (int(w * K), int(h * K)), (0, 0, 0, 0)); d = ImageDraw.Draw(f); fw, fh = f.size
    if code == 'BR':
        d.rectangle((0, 0, fw, fh), fill=(0, 151, 57)); d.polygon([(fw * .5, fh * .1), (fw * .92, fh * .5), (fw * .5, fh * .9), (fw * .08, fh * .5)], fill=(254, 221, 0))
        d.ellipse((fw * .32, fh * .24, fw * .68, fh * .76), fill=(0, 39, 118))
    elif code == 'AR':
        for i, c in enumerate([(116, 172, 223), WHITE, (116, 172, 223)]): d.rectangle((0, fh * i / 3, fw, fh * (i + 1) / 3), fill=c)
        d.ellipse((fw * .44, fh * .4, fw * .56, fh * .6), fill=(246, 180, 14))
    elif code == 'MX':
        for i, c in enumerate([(0, 104, 71), WHITE, (206, 17, 38)]): d.rectangle((fw * i / 3, 0, fw * (i + 1) / 3, fh), fill=c)
        d.ellipse((fw * .44, fh * .38, fw * .56, fh * .62), fill=(140, 90, 40))
    elif code == 'CL':
        d.rectangle((0, 0, fw, fh / 2), fill=WHITE); d.rectangle((0, fh / 2, fw, fh), fill=(213, 43, 30)); d.rectangle((0, 0, fw / 3, fh / 2), fill=(0, 57, 166))
    elif code == 'PT':
        d.rectangle((0, 0, fw, fh), fill=(255, 0, 0)); d.rectangle((0, 0, fw * .4, fh), fill=(0, 102, 0)); d.ellipse((fw * .3, fh * .3, fw * .5, fh * .7), fill=(255, 220, 0))
    m = Image.new('L', f.size, 0); ImageDraw.Draw(m).rounded_rectangle((0, 0, fw - 1, fh - 1), 4 * K, fill=255)
    l = pic.layer(); l.paste(f, (int(x * K), int(y * K)), m); pic.put(l)
    l = pic.layer(); ImageDraw.Draw(l).rounded_rectangle(P(x, y, x + w, y + h), 4 * K, outline=(255, 255, 255, 90), width=K); pic.put(l)

# ------------------------------------------------------------- troféus e medalhas (desenho próprio)
METAL = {'ouro': [(255, 246, 190), (255, 204, 64), (196, 120, 10)], 'prata': [(255, 255, 255), (200, 212, 230), (120, 132, 160)],
         'bronze': [(255, 214, 170), (214, 130, 70), (130, 66, 26)], 'ki': [(220, 248, 255), (90, 200, 255), (60, 70, 210)],
         'off': [(110, 118, 150), (70, 78, 110), (45, 50, 80)]}

def trophy(pic, cx, base, h, kind, got=True):
    c = METAL[kind if got else 'off']; u = h / 20
    if got:  # brilho atrás
        l = pic.layer(); ImageDraw.Draw(l).ellipse(P(cx - 9 * u, base - 19 * u, cx + 9 * u, base - 4 * u), fill=c[1] + (80,)); pic.put(l, 14)
    m = Image.new('L', pic.im.size, 0); d = ImageDraw.Draw(m)
    d.pieslice(P(cx - 6 * u, base - 24 * u, cx + 6 * u, base - 8 * u), 0, 180, fill=255)       # taça
    d.rectangle(P(cx - 6 * u, base - 18 * u, cx + 6 * u, base - 16 * u), fill=255)
    for s in (-1, 1):                                                                          # alças
        d.arc(P(cx + s * 6.2 * u - 2.6 * u, base - 18 * u, cx + s * 6.2 * u + 2.6 * u, base - 11.5 * u), -90 if s > 0 else 90, 90 if s > 0 else 270, fill=255, width=int(1.1 * u * K))
    d.rectangle(P(cx - 0.9 * u, base - 8.5 * u, cx + 0.9 * u, base - 4.5 * u), fill=255)        # haste
    d.polygon(P(cx - 3.6 * u, base - 4.5 * u, cx + 3.6 * u, base - 4.5 * u, cx + 4.4 * u, base - 2.2 * u, cx - 4.4 * u, base - 2.2 * u), fill=255)
    d.rounded_rectangle(P(cx - 5.2 * u, base - 2.4 * u, cx + 5.2 * u, base), int(0.6 * u * K), fill=255)  # pedestal
    bb = m.getbbox(); g = grad_img(bb[2] - bb[0], bb[3] - bb[1], [(0, c[0] + (255,)), (0.45, c[1] + (255,)), (1, c[2] + (255,))], vertical=False)
    l = pic.layer(); l.paste(g, bb[:2], m.crop(bb)); pic.put(l)
    l = pic.layer(); dd = ImageDraw.Draw(l)
    dd.ellipse(P(cx - 3.8 * u, base - 15.5 * u, cx - 2.2 * u, base - 11 * u), fill=(255, 255, 255, 120 if got else 30))  # reflexo
    if got:
        star(dd, cx, base - 12.6 * u, 2.3 * u, (255, 255, 255, 170))
    else:
        dd.rounded_rectangle(P(cx - 2 * u, base - 13.5 * u, cx + 2 * u, base - 10 * u), int(0.5 * u * K), fill=(25, 30, 55, 230))   # cadeado
        dd.arc(P(cx - 1.4 * u, base - 15.6 * u, cx + 1.4 * u, base - 12 * u), 180, 360, fill=(25, 30, 55, 230), width=int(0.6 * u * K))
    pic.put(l)

def star(d, cx, cy, r, fill, n=5):
    pts = []
    for i in range(n * 2):
        a = -math.pi / 2 + i * math.pi / n; rr_ = r if i % 2 == 0 else r * 0.45
        pts += [(cx + rr_ * math.cos(a)) * K, (cy + rr_ * math.sin(a)) * K]
    d.polygon(pts, fill=fill)

def medal(pic, cx, cy, r, kind, icon, got=True):
    c = METAL[kind if got else 'off']
    l = pic.layer(); d = ImageDraw.Draw(l)                             # fita
    rib = [(200, 40, 50), (30, 70, 170)] if got else [(70, 76, 100), (55, 60, 85)]
    d.polygon(P(cx - r * .9, cy - r * 2.0, cx - r * .15, cy - r * 2.0, cx + r * .25, cy - r * .5, cx - r * .45, cy - r * .5), fill=rib[0] + (255,))
    d.polygon(P(cx + r * .9, cy - r * 2.0, cx + r * .15, cy - r * 2.0, cx - r * .25, cy - r * .5, cx + r * .45, cy - r * .5), fill=rib[1] + (255,))
    pic.put(l)
    if got:
        l = pic.layer(); ImageDraw.Draw(l).ellipse(P(cx - r * 1.3, cy - r * 1.3, cx + r * 1.3, cy + r * 1.3), fill=c[1] + (70,)); pic.put(l, 10)
    m = Image.new('L', pic.im.size, 0); ImageDraw.Draw(m).ellipse(P(cx - r, cy - r, cx + r, cy + r), fill=255)
    bb = m.getbbox(); g = grad_img(bb[2] - bb[0], bb[3] - bb[1], [(0, c[0] + (255,)), (0.5, c[1] + (255,)), (1, c[2] + (255,))])
    l = pic.layer(); l.paste(g, bb[:2], m.crop(bb)); pic.put(l)
    l = pic.layer(); d = ImageDraw.Draw(l)
    d.ellipse(P(cx - r * .74, cy - r * .74, cx + r * .74, cy + r * .74), outline=c[2] + (255,), width=int(r * .09 * K))
    ic = (60, 35, 0, 230) if got else (30, 34, 60, 230)
    if icon == 'star': star(d, cx, cy, r * .5, ic)
    elif icon == 'bolt': d.polygon(P(cx + r * .1, cy - r * .55, cx - r * .35, cy + r * .08, cx - r * .02, cy + r * .08, cx - r * .12, cy + r * .55, cx + r * .35, cy - r * .1, cx + r * .02, cy - r * .1), fill=ic)
    elif icon == 'fist':
        d.rounded_rectangle(P(cx - r * .38, cy - r * .3, cx + r * .38, cy + r * .35), int(r * .12 * K), fill=ic)
        for i in range(4): d.line(P(cx - r * .38 + i * r * .19 + r * .095, cy - r * .3, cx - r * .38 + i * r * .19 + r * .095, cy), fill=c[1] + (255,), width=max(1, int(r * .04 * K)))
    elif icon == 'shield': d.polygon(P(cx - r * .4, cy - r * .45, cx + r * .4, cy - r * .45, cx + r * .4, cy + r * .05, cx, cy + r * .5, cx - r * .4, cy + r * .05), fill=ic)
    elif icon == 'flame': d.polygon(P(cx, cy - r * .55, cx + r * .35, cy, cx + r * .25, cy + r * .4, cx - r * .25, cy + r * .4, cx - r * .35, cy, cx - r * .1, cy - r * .15), fill=ic)
    elif icon == 'up':
        d.polygon(P(cx, cy - r * .5, cx + r * .42, cy - r * .02, cx + r * .15, cy - r * .02, cx + r * .15, cy + r * .45, cx - r * .15, cy + r * .45, cx - r * .15, cy - r * .02, cx - r * .42, cy - r * .02), fill=ic)
    elif icon == 'globe':
        d.ellipse(P(cx - r * .45, cy - r * .45, cx + r * .45, cy + r * .45), outline=ic, width=int(r * .08 * K)); d.line(P(cx - r * .45, cy, cx + r * .45, cy), fill=ic, width=int(r * .07 * K))
        d.ellipse(P(cx - r * .18, cy - r * .45, cx + r * .18, cy + r * .45), outline=ic, width=int(r * .07 * K))
    elif icon == 'clock':
        d.ellipse(P(cx - r * .45, cy - r * .45, cx + r * .45, cy + r * .45), outline=ic, width=int(r * .09 * K)); d.line(P(cx, cy, cx, cy - r * .3), fill=ic, width=int(r * .08 * K)); d.line(P(cx, cy, cx + r * .22, cy + r * .1), fill=ic, width=int(r * .08 * K))
    d.arc(P(cx - r * .9, cy - r * .9, cx + r * .9, cy + r * .9), 200, 250, fill=(255, 255, 255, 140 if got else 40), width=int(r * .08 * K))
    pic.put(l)

# ------------------------------------------------------------- fundo e cabeçalho (igual às telas de dentro)
def screen_bg():
    b = Image.open(S + '/out/fundo-limpo.png').convert('RGB')
    b = b.crop((330, 0, 1100, 495)).resize((W * K, int(W * K * 495 / 770)), Image.LANCZOS)
    b = b.crop((0, 0, W * K, H * K)).filter(ImageFilter.GaussianBlur(7 * K))
    pic = Pic(b)
    pic.put(grad_img(W * K, H * K, [(0, (8, 14, 40, 120)), (0.45, (8, 14, 40, 150)), (1, (6, 10, 30, 215))]))
    return pic

def logo(pic):
    rr(pic, (40, 30, 300, 86), 28, [(0, (255, 232, 130, 255)), (1, GOLD2 + (255,))], (255, 248, 210, 255), 2, shadow=100)
    l = pic.layer(); ImageDraw.Draw(l).ellipse(P(44, 34, 92, 82), fill=NAVY + (255,)); pic.put(l)
    text(pic, (68, 58), 'BT4', 'black', 19, GOLD + (255,), 'mm', shadow=0)
    text(pic, (104, 57), 'ONLINE', 'black', 36, NAVY + (255,), 'lm', shadow=0)
    text(pic, (44, 112), 'ONLINE  ›  PERFIL E TOP  ›', 'chak', 20, SOFT + (220,), 'lm', shadow=2)
    l = pic.layer(); ImageDraw.Draw(l).polygon(P(40, 140, 330, 140, 312, 202, 40, 202), fill=(0, 0, 0, 120)); pic.put(l, 6)
    m = grad_img(int(300 * K), int(62 * K), [(0, (255, 232, 130, 255)), (1, GOLD2 + (255,))])
    mk = Image.new('L', m.size, 0); ImageDraw.Draw(mk).polygon([(0, 0), (300 * K, 0), (282 * K, 62 * K), (0, 62 * K)], fill=255)
    l = pic.layer(); l.paste(m, (40 * K, 136 * K), mk); pic.put(l)
    text(pic, (60, 168), 'PERFIL', 'black', 44, NAVY + (255,), 'lm', shadow=0)

def button(pic, x, y, kind):
    l = pic.layer(); d = ImageDraw.Draw(l)
    if kind == 'select':
        d.rounded_rectangle(P(x - 4, y - 15, x + 70, y + 15), 15 * K, outline=WHITE + (200,), width=2 * K); pic.put(l)
        text(pic, (x + 33, y + 1), 'R1 / L1', 'chak', 15, WHITE + (230,), 'mm', shadow=0); return 82
    d.ellipse(P(x, y - 18, x + 36, y + 18), outline=WHITE + (180,), width=2 * K)
    cx, cy = x + 18, y
    if kind == 'x':
        d.line(P(cx - 8, cy - 8, cx + 8, cy + 8), fill=(160, 180, 255, 255), width=3 * K); d.line(P(cx - 8, cy + 8, cx + 8, cy - 8), fill=(160, 180, 255, 255), width=3 * K)
    elif kind == 's': d.rectangle(P(cx - 8, cy - 8, cx + 8, cy + 8), outline=(255, 130, 200, 255), width=3 * K)
    elif kind == 't': d.polygon(P(cx, cy - 10, cx + 10, cy + 7, cx - 10, cy + 7), outline=(80, 230, 200, 255), width=3 * K)
    pic.put(l); return 46

def bottom_bar(pic, items):
    x = W - 40
    widths = [(k, s, (82 if k == 'select' else 46) + text_w(s, 'chak', 24) + 34) for k, s in items]
    x -= sum(w for _, _, w in widths)
    for k, s, w in widths:
        bw = button(pic, x, 950, k); text(pic, (x + bw + 4, 950), s, 'chak', 24, WHITE + (255,), 'lm', shadow=2); x += w

# ------------------------------------------------------------- o lutador mais usado (quadro do recorte)
def portrait(pic, box, frame):
    x0, y0, x1, y1 = box; w, h = x1 - x0, y1 - y0
    rr(pic, box, 26, [(0, (120, 30, 40, 255)), (0.55, (40, 22, 70, 255)), (1, NAVY + (255,))], None, shadow=140)
    l = pic.layer(); d = ImageDraw.Draw(l)                                  # raios de ki atrás
    cx, cy = x0 + w * 0.55, y0 + h * 0.35
    for i in range(18):
        a = i * math.pi * 2 / 18
        d.polygon(P(cx, cy, cx + math.cos(a - 0.07) * 600, cy + math.sin(a - 0.07) * 600, cx + math.cos(a + 0.07) * 600, cy + math.sin(a + 0.07) * 600), fill=(255, 140, 60, 26))
    d.ellipse(P(cx - 150, cy - 150, cx + 150, cy + 150), fill=(255, 120, 60, 60))
    m = Image.new('L', pic.im.size, 0); ImageDraw.Draw(m).rounded_rectangle(P(*box), 26 * K, fill=255)
    l2 = pic.layer(); l2.paste(l.filter(ImageFilter.GaussianBlur(6 * K)), (0, 0), m); pic.put(l2)
    # recorte: cabeça e tronco, maior que o quadro (sai pela base)
    sc = (h * 1.95) / frame.height
    f = frame.resize((int(frame.width * sc * K), int(frame.height * sc * K)), Image.LANCZOS)
    l = pic.layer(); l.alpha_composite(f, (int((x0 + w / 2 - f.width / K * 0.52) * K), int((y0 + 14) * K)))
    l2 = pic.layer(); l2.paste(l, (0, 0), m); pic.put(l2)
    g = grad_img(int(w * K), int(130 * K), [(0, (8, 12, 36, 0)), (1, (8, 12, 36, 245))])
    mk = Image.new('L', g.size, 0); ImageDraw.Draw(mk).rounded_rectangle((0, -40 * K, g.width - 1, g.height - 1), 26 * K, fill=255)
    l = pic.layer(); l.paste(g, (int(x0 * K), int((y1 - 130) * K)), mk); pic.put(l)
    l = pic.layer(); ImageDraw.Draw(l).rounded_rectangle(P(*box), 26 * K, outline=GOLD + (230,), width=3 * K); pic.put(l)
    chip(pic, x0 + 18, y0 + 30, 'MAIS USADO', 18, gold=True)
    gold_text(pic, (x0 + 22, y1 - 62), 'TRUNKS', 'black', 46)
    text(pic, (x0 + 24, y1 - 24), '148 lutas  ·  47% das partidas', 'chak', 19, SOFT + (255,), 'lm', shadow=2)

def static_layer():
    pic = screen_bg(); logo(pic)
    # --- cartão do jogador
    glass(pic, (40, 230, 820, 920), 34, gold=True)
    # (o retrato entra por quadro: x 64..380, y 256..700)
    gold_text(pic, (404, 290), 'Regulus', 'black', 54)
    flag(pic, 406, 326, 46, 'BR'); text(pic, (464, 342), 'Brasil  ·  desde 11/2026', 'chak', 21, SOFT + (255,), 'lm', shadow=2)
    chip(pic, 404, 396, 'CAMPEÃO DE TORNEIO', 18, gold=True)
    # selo de liga
    cx, cy = 612, 520
    l = pic.layer(); ImageDraw.Draw(l).ellipse(P(cx - 120, cy - 120, cx + 120, cy + 120), fill=(255, 190, 60, 50)); pic.put(l, 18)
    l = pic.layer(); d = ImageDraw.Draw(l)
    hexa = [(cx + 78 * math.cos(math.pi / 6 + i * math.pi / 3), cy + 78 * math.sin(math.pi / 6 + i * math.pi / 3)) for i in range(6)]
    d.polygon([v * K for p in hexa for v in p], fill=(40, 30, 10, 255))
    hexa2 = [(cx + 70 * math.cos(math.pi / 6 + i * math.pi / 3), cy + 70 * math.sin(math.pi / 6 + i * math.pi / 3)) for i in range(6)]
    pic.put(l)
    m = Image.new('L', pic.im.size, 0); ImageDraw.Draw(m).polygon([v * K for p in hexa2 for v in p], fill=255)
    bb = m.getbbox(); g = grad_img(bb[2] - bb[0], bb[3] - bb[1], [(0, (255, 246, 190, 255)), (0.5, GOLD + (255,)), (1, (196, 120, 10, 255))])
    l = pic.layer(); l.paste(g, bb[:2], m.crop(bb)); pic.put(l)
    l = pic.layer(); star(ImageDraw.Draw(l), cx, cy + 2, 38, (90, 50, 0, 235)); pic.put(l)
    text(pic, (cx, cy + 108), 'LIGA OURO', 'black', 28, GOLD + (255,), 'mm')
    text(pic, (cx, cy + 138), '1 388 pontos  ·  #37', 'chak', 19, SOFT + (255,), 'mm', shadow=2)
    # números
    stats = [('Partidas', '312'), ('Vitórias', '201'), ('Taxa', '64%'), ('Sequência', '6')]
    for i, (a, b) in enumerate(stats):
        x0 = 64 + i * 185; y0 = 724
        rr(pic, (x0, y0, x0 + 170, y0 + 96), 18, [(0, (255, 255, 255, 26)), (1, (255, 255, 255, 8))], (255, 255, 255, 50), 1.5)
        text(pic, (x0 + 85, y0 + 38), b, 'black', 40, WHITE + (255,), 'mm')
        text(pic, (x0 + 85, y0 + 76), a, 'chak', 18, SOFT + (255,), 'mm', shadow=0)
    # barra de vitórias / derrotas
    x0, x1, y = 64, 796, 858
    text(pic, (x0, y - 8), 'ÚLTIMAS 10', 'chak', 17, DIM + (255,), 'lm', shadow=0)
    res = 'VVDVVVVDVV'
    for i, r_ in enumerate(res):
        bx = x0 + 136 + i * 59
        c = [(60, 200, 120, 255), (30, 140, 80, 255)] if r_ == 'V' else [(240, 80, 90, 255), (170, 40, 60, 255)]
        rr(pic, (bx, y - 26, bx + 50, y + 12), 10, [(0, c[0]), (1, c[1])], (255, 255, 255, 70), 1)
        text(pic, (bx + 25, y - 7), r_, 'black', 20, WHITE + (255,), 'mm', shadow=0)
    text(pic, (x1, y + 36), 'Melhor posição: #1 semanal  ·  Temporada 3', 'chak', 17, DIM + (255,), 'rm', shadow=0)

    # --- troféus
    glass(pic, (850, 230, 2160, 560), 30)
    gold_text(pic, (884, 278), 'TROFÉUS', 'black', 38)
    text(pic, (2124, 278), '7 de 12', 'chak', 24, SOFT + (255,), 'rm', shadow=2)
    rr(pic, (1580, 266, 1580 + 400, 266 + 22), 11, [(0, (255, 255, 255, 30)), (1, (255, 255, 255, 30))])
    rr(pic, (1580, 266, 1580 + 400 * 7 / 12, 266 + 22), 11, [(0, (255, 232, 130, 255)), (1, GOLD2 + (255,))])
    shelf = [('ouro', 'Campeão de', 'torneio', '×3', True), ('ouro', '#1 da', 'semana', '×1', True), ('prata', 'Top 10', 'global', '', True),
             ('ki', '100 vitórias', 'ranqueadas', '', True), ('bronze', 'Primeira', 'vitória', '', True), ('prata', '10 vitórias', 'seguidas', '', True),
             ('ouro', 'Invicto na', 'temporada', '', False)]
    l = pic.layer(); d = ImageDraw.Draw(l)
    d.rounded_rectangle(P(884, 470, 2126, 486), 8 * K, fill=(255, 255, 255, 46)); d.rounded_rectangle(P(884, 486, 2126, 494), 4 * K, fill=(0, 0, 0, 70))
    pic.put(l)
    for i, (t, a1, a2, cnt, got) in enumerate(shelf):
        cx = 970 + i * 172
        trophy(pic, cx, 470, 150, t, got)
        if cnt: chip(pic, cx + 26, 330, cnt, 18, gold=True, anchor='m')
        c = WHITE if got else DIM
        text(pic, (cx, 514), a1, 'chak', 19, c + (255,), 'mm', shadow=2)
        text(pic, (cx, 538), a2, 'chak', 19, c + (255,), 'mm', shadow=2)

    # --- medalhas
    glass(pic, (850, 580, 2160, 790), 30)
    gold_text(pic, (884, 624), 'MEDALHAS', 'black', 34)
    text(pic, (2124, 624), '23 ganhas  ·  ganhe ao fim das lutas', 'chak', 20, SOFT + (255,), 'rm', shadow=2)
    meds = [('ouro', 'star', 'Perfeito', '×12', True), ('prata', 'up', 'Virada', '×31', True), ('ouro', 'bolt', 'Golpe final', '×58', True),
            ('bronze', 'fist', 'Combo 20+', '×44', True), ('ki', 'flame', 'Super ataque', '×19', True), ('prata', 'shield', 'Defesa total', '×7', True),
            ('bronze', 'clock', 'Luta rápida', '×26', True), ('ouro', 'globe', 'Rival de fora', '', False)]
    for i, (k_, ic, nm, cnt, got) in enumerate(meds):
        cx = 950 + i * 157
        medal(pic, cx, 712, 34, k_, ic, got)
        text(pic, (cx, 766), nm + (f'  {cnt}' if cnt else ''), 'chak', 18, (WHITE if got else DIM) + (255,), 'mm', shadow=2)

    # --- lutadores mais usados
    glass(pic, (850, 810, 1500, 920), 26)
    text(pic, (878, 836), 'MAIS USADOS', 'black', 20, GOLD + (255,), 'lm')
    for i, (n, v) in enumerate([('Trunks', 47), ('Goku', 23), ('Vegeta', 16)]):
        y = 866 + i * 0
        x0 = 878 + i * 206
        text(pic, (x0, 870), n, 'chak', 20, WHITE + (255,), 'lm', shadow=2)
        text(pic, (x0 + 180, 870), f'{v}%', 'black', 20, SOFT + (255,), 'rm', shadow=0)
        rr(pic, (x0, 890, x0 + 180, 902), 6, [(0, (255, 255, 255, 30)), (1, (255, 255, 255, 30))])
        rr(pic, (x0, 890, x0 + 180 * v / 47, 902), 6, [(0, (255, 232, 130, 255)), (1, GOLD2 + (255,))] if i == 0 else [(0, (150, 180, 255, 255)), (1, (90, 120, 220, 255))])
    # --- top global
    glass(pic, (1520, 810, 2160, 920), 26)
    text(pic, (1548, 836), 'TOP GLOBAL', 'black', 20, GOLD + (255,), 'lm')
    chip(pic, 2132, 836, '1 CONTRA 1', 14, gold=True, anchor='r')
    for i, (n, p, r_) in enumerate([('Regulus', 1388, 'BR'), ('YAMI', 1342, 'AR'), ('Geovany', 1301, 'BR')]):
        x0 = 1548 + i * 204
        text(pic, (x0, 880), f'{i + 1}', 'black', 24, GOLD + (255,), 'lm', shadow=0)
        flag(pic, x0 + 24, 868, 34, r_)
        text(pic, (x0 + 66, 872), n, 'chak', 18, WHITE + (255,), 'lm', shadow=2)
        text(pic, (x0 + 66, 896), str(p), 'chak', 15, SOFT + (255,), 'lm', shadow=0)
    bottom_bar(pic, [('select', 'Formato'), ('s', 'Todos os troféus'), ('t', 'Voltar')])
    return pic

base = static_layer()
frames_dir = S + '/trunks-recorte'
names = sorted(os.listdir(frames_dir))
if not ANIM: names = names[:1]
out = []
for n in names:
    pic = Pic(base.im.copy())
    portrait(pic, (64, 256, 380, 700), Image.open(os.path.join(frames_dir, n)))
    out.append(pic.im.resize((W, H), Image.LANCZOS).convert('RGB'))
os.makedirs(S + '/out', exist_ok=True)
out[0].save(S + '/out/08-perfil.png')
if ANIM:
    out[0].save(S + '/out/08-perfil-animado.webp', save_all=True, append_images=out[1:], duration=50, loop=0, quality=86, method=4)
print('ok', len(out))
