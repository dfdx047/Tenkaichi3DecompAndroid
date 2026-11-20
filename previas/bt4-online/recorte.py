"""Recorte do lutador em fundo branco: quadros PNG -> PNG RGBA (fundo transparente).
uso: python3 cut.py <pasta quadros> <pasta saida>"""
import os, sys
import numpy as np, cv2
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
names = sorted(n for n in os.listdir(src) if n.endswith('.png'))

for n in names:
    im = np.asarray(Image.open(os.path.join(src, n)).convert('RGB')).astype(np.float32)
    h, w, _ = im.shape
    border = np.concatenate([im[:4].reshape(-1, 3), im[-4:].reshape(-1, 3), im[:, :4].reshape(-1, 3), im[:, -4:].reshape(-1, 3)])
    bg = np.median(border, 0)
    d = np.sqrt(((im - bg) ** 2).sum(2))                  # distância ao branco do fundo
    cand = (d < 38).astype(np.uint8)
    cnt, lab, stats, _ = cv2.connectedComponentsWithStats(cand, connectivity=4)
    keep = np.zeros(cnt, bool)
    edge = np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))
    keep[edge] = True
    keep[stats[:, cv2.CC_STAT_AREA] > 900] = True          # vãos fechados (entre braço e corpo)
    keep[0] = False
    # vão entre a luva e o cabelo (no alto, à esquerda): branco puro preso ali, também é fundo
    for k in range(1, cnt):
        x, y, ww, hh, a = stats[k]
        if a > 60 and 95 <= x and x + ww <= 175 and 150 <= y <= 205 and d[lab == k].mean() < 24:
            keep[k] = True
    bgm = keep[lab] & (cand > 0)
    # marca d'água no canto de cima (texto cinza claro sobre o fundo): fundo também
    wm = np.zeros_like(bgm); wm[:70, int(w * 0.55):] = True
    bgm |= wm & (d < 90) & (np.abs(im - im.mean(2, keepdims=True)).max(2) < 18)
    fg = (~bgm).astype(np.uint8)
    # só o maior corpo (o lutador) e o que está ligado a ele
    cnt, lab, stats, _ = cv2.connectedComponentsWithStats(fg, connectivity=8)
    big = 1 + np.argmax(stats[1:, cv2.CC_STAT_AREA])
    fg = ((lab == big) | ((stats[lab, cv2.CC_STAT_AREA] > 1500) & (lab > 0))).astype(np.uint8)
    # borda macia: alfa pela distância ao branco perto do contorno
    fgf = cv2.GaussianBlur(fg.astype(np.float32), (0, 0), 0.8)
    soft = np.clip((d - 6) / 60, 0, 1)
    ring = cv2.dilate(fg, np.ones((3, 3), np.uint8)) - cv2.erode(fg, np.ones((3, 3), np.uint8))
    a = np.where(ring > 0, np.minimum(fgf, np.maximum(soft, fgf * 0.5)), fg.astype(np.float32))
    # tira o halo branco: desfaz a mistura com o fundo
    aa = np.clip(a, 1e-3, 1)[..., None]
    rgb = np.where(a[..., None] < 0.999, (im - bg * (1 - aa)) / aa, im)
    out = np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8)
    Image.fromarray(out, 'RGBA').save(os.path.join(dst, n), optimize=True)
print(len(names), 'quadros ->', dst)
