#!/usr/bin/env python3
"""The README's before/after charts, as SVG files with their own light and dark colours, in the three languages.

    python3 port/tools/perf_chart.py          # writes docs/images/1.0/perf_speed_<lang>.svg and perf_draw_<lang>.svg

The numbers below are copied from the measured logs (docs/release-1.0.md says which run each one comes from). Palette: the
validated two-slot categorical pair, light #2a78d6 #eb6834 on #fcfcfb, dark #3987e5 #d95926 on #1a1a19."""
import os
from xml.sax.saxutils import escape

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "docs", "images", "1.0")

TEXT = {
    "en": {"dec": ".", "cube": "RG Cube", "rp5": "Retroid Pocket 5", "low": "RG Cube held down (G52)", "c": "RG Cube",
           "speed": "Speed in fights, 0.9.0 and 1.0",
           "speed_sub": "30 is the game's normal rate. Longer is better.",
           "bt3": "BT3: pictures a second", "bt4": "BT4: game updates a second",
           "draw": "Drawing time per frame, 0.9.0 and 1.0", "draw_sub": "Milliseconds. Shorter is better.",
           "bt3d": "BT3", "bt4d": "BT4 on the Dragon Rage renderer", "lim": "held down (G52)"},
    "pt": {"dec": ",", "cube": "RG Cube", "rp5": "Retroid Pocket 5", "low": "RG Cube limitado (G52)", "c": "RG Cube",
           "speed": "Velocidade nas lutas, 0.9.0 e 1.0",
           "speed_sub": "30 é o ritmo normal do jogo. Barra maior é melhor.",
           "bt3": "BT3: imagens por segundo", "bt4": "BT4: atualizações do jogo por segundo",
           "draw": "Tempo de desenho por quadro, 0.9.0 e 1.0", "draw_sub": "Milissegundos. Barra menor é melhor.",
           "bt3d": "BT3", "bt4d": "BT4 no renderizador Dragon Rage", "lim": "limitado (G52)"},
    "es": {"dec": ",", "cube": "RG Cube", "rp5": "Retroid Pocket 5", "low": "RG Cube limitado (G52)", "c": "RG Cube",
           "speed": "Velocidad en las peleas, 0.9.0 y 1.0",
           "speed_sub": "30 es el ritmo normal del juego. Barra más larga es mejor.",
           "bt3": "BT3: imágenes por segundo", "bt4": "BT4: actualizaciones del juego por segundo",
           "draw": "Tiempo de dibujo por cuadro, 0.9.0 y 1.0", "draw_sub": "Milisegundos. Barra más corta es mejor.",
           "bt3d": "BT3", "bt4d": "BT4 en el renderizador Dragon Rage", "lim": "limitado (G52)"},
}

# (label, 0.9.0, 1.0)
def speed(t):
    return [
        (t["bt3"], [(f"{t['cube']}, 1x", 30.0, 30.0), (f"{t['rp5']}, 2x", 30.7, 30.7), (f"{t['low']}, 1x", 19.3, 18.5)]),
        (t["bt4"], [(f"{t['cube']}, ARMSX2", 28.9, 29.6), (f"{t['cube']}, Dragon Rage", 29.7, 29.8),
                    (f"{t['rp5']}, ARMSX2", 30.0, 30.0), (f"{t['rp5']}, Dragon Rage", 30.0, 30.0),
                    (f"{t['low']}, ARMSX2", 10.3, 9.9), (f"{t['low']}, Dragon Rage", WEAK_DR_UPD[0], WEAK_DR_UPD[1])]),
    ]

def draw(t):
    return [
        (t["bt3d"], [(f"{t['cube']}, 1x", 3.66, 2.75), (f"{t['rp5']}, 2x", 3.77, 3.05), (f"{t['low']}, 1x", 6.89, 7.21)]),
        (t["bt4d"], [(t["cube"], 8.53, 7.05), (t["rp5"], 6.04, 4.89), (t["low"], WEAK_DR_DRAW[0], WEAK_DR_DRAW[1])]),
    ]

# BT4, Dragon Rage renderer, RG Cube held down: 0.9.0 and 1.0 in turn in one session (port/tools/lab/ab_bt4.sh)
WEAK_DR_UPD = (12.2, 11.8)
WEAK_DR_DRAW = (5.04, 5.36)

STYLE = """
.s{fill:#fcfcfb}.a{fill:#2a78d6}.b{fill:#eb6834}.h{fill:#0b0b0b;font-weight:600}.t{fill:#0b0b0b}.m{fill:#52514e}
.g{stroke:#e4e3df;stroke-width:1}.r{stroke:#8a8983;stroke-width:1;stroke-dasharray:4 4}
@media (prefers-color-scheme:dark){.s{fill:#1a1a19}.a{fill:#3987e5}.b{fill:#d95926}.h{fill:#f0efec}.t{fill:#f0efec}.m{fill:#c3c2b7}
.g{stroke:#33332f}.r{stroke:#8a8983}}
text{font-family:system-ui,-apple-system,'Segoe UI',Roboto,'Noto Sans',sans-serif}
"""


def chart(title, sub, panels, top, line, dec, unit_digits):
    W, left, right = 880, 250, 120
    plot = W - left - right
    x = lambda v: left + plot * v / top
    n = lambda v: f"{v:.{unit_digits}f}".replace(".", dec)
    o, y = [], 34
    o.append(f'<text class="h" x="20" y="{y}" font-size="19">{escape(title)}</text>'); y += 22
    o.append(f'<text class="m" x="20" y="{y}" font-size="13">{escape(sub)}</text>')
    o.append(f'<rect class="a" x="{W-150}" y="{y-11}" width="11" height="11" rx="2"/><text class="t" x="{W-134}" y="{y}" font-size="13">0.9.0</text>')
    o.append(f'<rect class="b" x="{W-85}" y="{y-11}" width="11" height="11" rx="2"/><text class="t" x="{W-69}" y="{y}" font-size="13">1.0</text>')
    y += 14
    for head, rows in panels:
        y += 22
        o.append(f'<text class="h" x="20" y="{y}" font-size="14">{escape(head)}</text>'); y += 10
        y0 = y
        for label, a, b in rows:
            mid = y + 13
            o.append(f'<text class="t" x="{left-12}" y="{mid+4}" font-size="13" text-anchor="end">{escape(label)}</text>')
            for cls, v, by in (("a", a, y + 2), ("b", b, y + 14)):
                w = x(v) - left
                o.append(f'<path class="{cls}" d="M{left} {by}h{w-3:.1f}a3 3 0 0 1 3 3v4a3 3 0 0 1-3 3h-{w-3:.1f}z"><title>{escape(label)}: {n(v)}</title></path>')
            o.append(f'<text class="m" x="{x(max(a, b))+10:.1f}" y="{mid+4}" font-size="13">{n(a)} → {n(b)}</text>')
            y += 30
        o.append(f'<line class="g" x1="{left}" y1="{y0}" x2="{left}" y2="{y}"/>')
        if line:
            o.append(f'<line class="r" x1="{x(line):.1f}" y1="{y0}" x2="{x(line):.1f}" y2="{y}"/>')
    H = y + 18
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" aria-label="{escape(title)}">'
            f'<style>{STYLE}</style><rect class="s" width="{W}" height="{H}" rx="10"/>' + "".join(p for p in o if p) + "</svg>\n")


def main():
    for lang, t in TEXT.items():
        with open(os.path.join(OUT, f"perf_speed_{lang}.svg"), "w", encoding="utf-8") as f:
            f.write(chart(t["speed"], t["speed_sub"], speed(t), 36, 30, t["dec"], 1))
        with open(os.path.join(OUT, f"perf_draw_{lang}.svg"), "w", encoding="utf-8") as f:
            f.write(chart(t["draw"], t["draw_sub"], draw(t), 12, None, t["dec"], 2))


if __name__ == "__main__":
    main()
