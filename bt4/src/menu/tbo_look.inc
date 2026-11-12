/* ------------------------------------------------------------------------------------- the redesigned screens
   Tenkaichi Budokai Online as its approved mock-ups (previas-bt4/redesign: a 2712 x 1220 phone screen) have it: the
   places, sizes and colours below are those mock-ups' pixels, mapped onto the page (Td_X / Td_Y: the window's whole
   width, its whole height). Their pieces are port/tools/tbo_redesign.py's pictures (td_*.png), their letters Bangers
   (titles, menus, buttons: gs/tbo_text.c kind 3) and Chakra Petch (texts: kind 4). Included by port_tbo.c; only what
   is drawn is here, what the screens do is port_tbo.c's. */

#define TD_W 2712.0f
#define TD_H 1220.0f
#define TD_B 3 /* Bangers */
#define TD_C 4 /* Chakra Petch */

static f32 Td_X(f32 px) {
    f32 l = Ps_Left();
    return l + px * (512.0f - 2.0f * l) / TD_W;
}
static f32 Td_Y(f32 py) {
    return py * 448.0f / TD_H;
}
static f32 Td_Wd(f32 px) { /* a width */
    return px * (512.0f - 2.0f * Ps_Left()) / TD_W;
}
static f32 Td_Ht(f32 py) { /* a height */
    return py * 448.0f / TD_H;
}
/* a colour as the mock-ups give it (alpha 0..255) */
static u32 Td_Rgba(s32 r, s32 g, s32 b, s32 a) {
    return Ps_Rgba(r, g, b, (a * 128 + 127) / 255);
}
#define TD_RGB(r, g, b) Td_Rgba(r, g, b, 255)
#define TD_NONE Ps_Rgba(0, 0, 0, 0)

static void Td_Pic(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, s32 alpha) {
    Ps_Sprite(id, Td_X(x0), Td_Y(y0), Td_X(x1), Td_Y(y1), u0, v0, u1, v1, PS_PLAIN, alpha);
}

/* A piece stretched over a box (x0..y1: the shape; `pad`: the piece's room for its glow or shadow round it) keeping
   its corners (`c` texels, one texel one pixel of the mock-ups) as they are. */
static void Td_Nine(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 pad, f32 c, s32 alpha) {
    f32 w = Ps_PicW(id), h = Ps_PicH(id);
    f32 xs[4], ys[4], us[4], vs[4];
    s32 i, j;

    x0 -= pad;
    y0 -= pad;
    x1 += pad;
    y1 += pad;
    if (c * 2.0f > x1 - x0) {
        c = (x1 - x0) * 0.5f;
    }
    if (c * 2.0f > y1 - y0) {
        c = (y1 - y0) * 0.5f;
    }
    xs[0] = x0; xs[1] = x0 + c; xs[2] = x1 - c; xs[3] = x1;
    ys[0] = y0; ys[1] = y0 + c; ys[2] = y1 - c; ys[3] = y1;
    us[0] = 0.0f; us[1] = c / w; us[2] = 1.0f - c / w; us[3] = 1.0f;
    vs[0] = 0.0f; vs[1] = c / h; vs[2] = 1.0f - c / h; vs[3] = 1.0f;
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            Td_Pic(id, xs[i], ys[j], xs[i + 1], ys[j + 1], us[i], vs[j], us[i + 1], vs[j + 1], alpha);
        }
    }
}

/* A piece stretched across (its ends, `capL` / `capR` texels, keep their shape at the height it is drawn): the box
   it fills, x0..y1, is the whole picture's. */
static void Td_Three(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 capL, f32 capR, s32 alpha) {
    f32 w = Ps_PicW(id), h = Ps_PicH(id);
    f32 s = (y1 - y0) / h;
    f32 l = capL * s, r = capR * s;

    if (l + r > x1 - x0) {
        f32 k = (x1 - x0) / (l + r);
        l *= k;
        r *= k;
    }
    Td_Pic(id, x0, y0, x0 + l, y1, 0.0f, 0.0f, capL / w, 1.0f, alpha);
    Td_Pic(id, x0 + l, y0, x1 - r, y1, capL / w, 0.0f, 1.0f - capR / w, 1.0f, alpha);
    Td_Pic(id, x1 - r, y0, x1, y1, 1.0f - capR / w, 0.0f, 1.0f, 1.0f, alpha);
}

/* a filled shape (its corners in order round it), coloured from `top` at its highest point to `bottom` at its lowest */
static void Td_Poly(const f32 *pts, s32 n, u32 top, u32 bottom) {
    f32 cx = 0.0f, cy = 0.0f, y0 = 1e9f, y1 = -1e9f;
    s32 i;

    for (i = 0; i < n; i++) {
        cx += pts[i * 2];
        cy += pts[i * 2 + 1];
        if (pts[i * 2 + 1] < y0) y0 = pts[i * 2 + 1];
        if (pts[i * 2 + 1] > y1) y1 = pts[i * 2 + 1];
    }
    cx /= (f32)n;
    cy /= (f32)n;
    for (i = 0; i < n; i++) {
        f32 ax = pts[i * 2], ay = pts[i * 2 + 1];
        f32 bx = pts[((i + 1) % n) * 2], by = pts[((i + 1) % n) * 2 + 1];
        f32 d = y1 > y0 ? y1 - y0 : 1.0f;
        u32 ca = Ps_Mix(top, bottom, (ay - y0) / d), cb = Ps_Mix(top, bottom, (by - y0) / d), cc = Ps_Mix(top, bottom, (cy - y0) / d);
        u32 aa = ((top >> 24) & 0xFF) + (u32)((f32)((s32)((bottom >> 24) & 0xFF) - (s32)((top >> 24) & 0xFF)) * (ay - y0) / d);
        u32 ab = ((top >> 24) & 0xFF) + (u32)((f32)((s32)((bottom >> 24) & 0xFF) - (s32)((top >> 24) & 0xFF)) * (by - y0) / d);
        u32 ac = ((top >> 24) & 0xFF) + (u32)((f32)((s32)((bottom >> 24) & 0xFF) - (s32)((top >> 24) & 0xFF)) * (cy - y0) / d);

        ca = (ca & 0xFFFFFFu) | (aa << 24);
        cb = (cb & 0xFFFFFFu) | (ab << 24);
        cc = (cc & 0xFFFFFFu) | (ac << 24);
        Ps_Quad(Td_X(cx), Td_Y(cy), Td_X(ax), Td_Y(ay), Td_X(bx), Td_Y(by), Td_X(bx), Td_Y(by), cc, ca, cb, cb);
    }
}

static void Td_Chamfer(f32 *p, f32 x0, f32 y0, f32 x1, f32 y1, f32 c) {
    f32 q[16] = {x0 + c, y0, x1 - c, y0, x1, y0 + c, x1, y1 - c, x1 - c, y1, x0 + c, y1, x0, y1 - c, x0, y0 + c};
    s32 i;
    for (i = 0; i < 16; i++) p[i] = q[i];
}
static void Td_Skew(f32 *p, f32 x0, f32 y0, f32 x1, f32 y1, f32 s) {
    p[0] = x0 + s; p[1] = y0; p[2] = x1 + s; p[3] = y0; p[4] = x1 - s; p[5] = y1; p[6] = x0 - s; p[7] = y1;
}
static void Td_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    Ps_Rect(Td_X(x0), Td_Y(y0), Td_X(x1), Td_Y(y1), top, bottom);
}
/* a line `w` pixels wide */
static void Td_Line(f32 x0, f32 y0, f32 x1, f32 y1, f32 w, u32 c) {
    f32 dx = x1 - x0, dy = y1 - y0, n = sqrtf(dx * dx + dy * dy), ox, oy;

    if (n < 0.01f) {
        return;
    }
    ox = -dy / n * w * 0.5f;
    oy = dx / n * w * 0.5f;
    Ps_Quad(Td_X(x0 - ox), Td_Y(y0 - oy), Td_X(x0 + ox), Td_Y(y0 + oy), Td_X(x1 - ox), Td_Y(y1 - oy), Td_X(x1 + ox), Td_Y(y1 + oy), c, c, c, c);
}
static void Td_Outline(const f32 *pts, s32 n, f32 w, u32 c) {
    s32 i;
    for (i = 0; i < n; i++) {
        Td_Line(pts[i * 2], pts[i * 2 + 1], pts[((i + 1) % n) * 2], pts[((i + 1) % n) * 2 + 1], w, c);
    }
}
static void Td_Disc(f32 cx, f32 cy, f32 r, u32 mid, u32 edge) {
    Ps_Disc(Td_X(cx), Td_Y(cy), Td_Ht(r), mid, edge);
}
static void Td_Tri(f32 ax, f32 ay, f32 bx, f32 by, f32 cx, f32 cy, u32 c) {
    Ps_Quad(Td_X(ax), Td_Y(ay), Td_X(bx), Td_Y(by), Td_X(cx), Td_Y(cy), Td_X(cx), Td_Y(cy), c, c, c, c);
}

/* ---- letters: `size` the mock-ups' font size in pixels; (x, y) where the mock-ups anchor them, the capitals' middle
   on y; align 0 left, 1 middle, 2 right; `width` (pixels, 0 none) the most they take across (squeezed to it) ---- */
static f32 Td_Scale(s32 kind, f32 size) {
    return size * (kind == TD_C ? 0.70f : 0.742f) * 448.0f / TD_H / (PS_CELL_UNITS * PS_CAP_OF_CELL);
}
typedef struct {
    u32 top, bottom, line, outer;
    f32 outerW;
} TdInk;
extern float Port_MenuTextNarrow(void);
extern int PortTbo_TextMeasure(const char *text, int kind);
/* the capitals' height in pixels of the mock-ups */
static f32 Td_Cap(s32 kind, f32 size) {
    return size * (kind == TD_C ? 0.70f : 0.742f);
}
/* Across, the letters as wide as in the mock-ups: their width is set from the screen's pixels (one pixel across is
   one down), not from the page's units and the menus' narrowing, which squeezed them to about half on a phone. */
static f32 Td_ScaleX(s32 kind, f32 size) {
    return Td_Wd(Td_Cap(kind, size)) / (PS_CELL_UNITS * PS_CAP_OF_CELL * PS_UNIT_X * Port_MenuTextNarrow());
}
static void Td_Words(f32 x, f32 y, char *s, s32 kind, f32 size, TdInk ink, s32 align, f32 width, s32 raw) {
    f32 sc = Td_Scale(kind, size), sx = Td_ScaleX(kind, size);
    s32 was = Ps_TextKind();

    if (s == NULL || s[0] == 0) {
        return;
    }
    Ps_SetTextKind(kind == TD_B && size >= 100.0f ? 5 : kind); /* (the largest from a picture twice as sharp) */
    Ps_SetTextExact(1);
    Ps_Align(align);
    if (raw) {
        PortOnline_Raw(1);
    }
    Ps_TextExOuterXY(Td_X(x), Ps_TextTop(Td_Y(y), sc), s, sx, sc, width > 0.0f ? Td_Wd(width) : 0.0f, ink.top, ink.bottom, ink.line, ink.outer,
                     ink.outerW > 0.0f ? Td_Ht(ink.outerW) : 0.0f);
    if (raw) {
        PortOnline_Raw(0);
    }
    Ps_Align(0);
    Ps_SetTextExact(0);
    Ps_SetTextKind(was);
}
/* how wide a line is, in pixels */
static f32 Td_WordsW(char *s, s32 kind, f32 size) {
    return (f32)PortTbo_TextMeasure(s, kind) / 1000.0f * Td_Cap(kind, size);
}
static TdInk Td_Ink(u32 top, u32 bottom, u32 line) {
    TdInk k;
    k.top = top;
    k.bottom = bottom;
    k.line = line;
    k.outer = TD_NONE;
    k.outerW = 0.0f;
    return k;
}
static TdInk Td_InkOuter(u32 top, u32 bottom, u32 line, u32 outer, f32 w) {
    TdInk k = Td_Ink(top, bottom, line);
    k.outer = outer;
    k.outerW = w;
    return k;
}
#define TD_GOLD Td_InkOuter(TD_RGB(255, 250, 210), TD_RGB(255, 150, 20), TD_RGB(110, 26, 0), TD_RGB(14, 4, 36), 4.0f) /* the titles */
#define TD_GOLD_FLAT Td_Ink(TD_RGB(255, 250, 210), TD_RGB(255, 150, 20), TD_RGB(100, 26, 0))
#define TD_GREEN Td_Ink(TD_RGB(236, 255, 236), TD_RGB(150, 240, 176), TD_RGB(0, 30, 10))
#define TD_AGE Td_Ink(TD_RGB(150, 255, 170), TD_RGB(80, 224, 120), TD_RGB(2, 30, 12))
#define TD_DARK Td_Ink(TD_RGB(60, 20, 0), TD_RGB(30, 8, 0), TD_RGB(255, 240, 200))
#define TD_DARK_FLAT Td_Ink(TD_RGB(60, 20, 0), TD_RGB(30, 8, 0), TD_NONE)
#define TD_WHITE Td_Ink(TD_RGB(255, 255, 255), TD_RGB(210, 220, 255), TD_RGB(14, 8, 40))
#define TD_CYAN Td_Ink(TD_RGB(200, 250, 255), TD_RGB(110, 220, 255), TD_RGB(8, 40, 80))
#define TD_ORANGE Td_Ink(TD_RGB(255, 236, 184), TD_RGB(255, 148, 48), TD_RGB(92, 36, 8))
#define TD_SKY(a) Td_Ink(Td_Rgba(150, 226, 255, a), Td_Rgba(150, 226, 255, a), TD_NONE) /* texts: one colour, no edge */
#define TD_TEXT(r, g, b) Td_Ink(TD_RGB(r, g, b), TD_RGB(r, g, b), TD_NONE)
#define TD_TEXT_EDGE(r, g, b) Td_Ink(TD_RGB(r, g, b), TD_RGB(r, g, b), TD_RGB(10, 6, 30))

/* ---- the pieces ---- */
/* a window: its shape x0..y1, its title on a gold tab over its top edge (NULL: none) */
static void Td_Window(f32 x0, f32 y0, f32 x1, f32 y1, char *title) {
    Td_Nine(TBO_TD_WIN, x0, y0, x1, y1, 24.0f, 88.0f, 0x80);
    if (title != NULL && title[0] != 0) {
        f32 tw = Td_WordsW(title, TD_B, 54.0f);
        f32 px0 = x0 + 40.0f, px1 = px0 + tw + 110.0f;
        Td_Three(TBO_TD_TAB, px0 - 42.0f, y0 - 46.0f, px1 + 42.0f, y0 + 66.0f, 106.0f, 46.0f, 0x80);
        Td_Words(px0 + 64.0f, y0 + 10.0f, title, TD_B, 54.0f, TD_DARK_FLAT, 0, 0.0f, 0);
    }
}

/* a Capsule Corp. capsule, `r` its half height... as the mock-ups' r: it is 2.7 r long; n 1..7 its colour and number */
static void Td_Capsule(f32 cx, f32 cy, f32 r, s32 n) {
    f32 s = r / 40.0f, w = 136.0f * s, h = 78.0f * s;
    f32 u0 = (f32)(n - 1) / 7.0f;

    Td_Pic(TBO_TD_CAPS, cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f, u0, 0.0f, u0 + 1.0f / 7.0f, 1.0f, 0x80);
}

/* an option: its capsule, its name, what it does (on the one under the cursor), its value in a box between arrows,
   where it is among its choices (`total` dots); `value` NULL: a plain entry */
static void Td_Option(f32 x0, f32 y0, f32 x1, f32 y1, char *label, char *value, s32 idx, s32 total, s32 on, s32 icon, char *hint, s32 raw) {
    f32 h = y1 - y0, s = h / 126.0f, cy = (y0 + y1) * 0.5f;
    f32 r = h * 0.26f, lx = x0 + 40.0f + r * 2.7f + 30.0f;
    f32 vw = value != NULL ? (x1 - x0) * 0.40f : 0.0f, vx1 = x1 - 36.0f, vx0 = vx1 - vw;
    f32 room;

    if (vw > 470.0f) {
        vw = 470.0f;
        vx0 = vx1 - vw;
    }
    if (on) {
        f32 g = 18.0f * s;
        Td_Three(TBO_TD_OPT_ON, x0 - 16.0f * s - g, y0 - g, x1 + 16.0f * s + g, y1 + g, 18.0f + 42.0f, 18.0f + 42.0f, 0x80);
        Td_Tri(x0 - 50.0f, cy - 22.0f, x0 - 16.0f, cy, x0 - 50.0f, cy + 22.0f, TD_RGB(255, 200, 80));
    } else {
        f32 g = 6.0f * s;
        Td_Three(TBO_TD_OPT, x0 - 16.0f * s - g, y0 - g, x1 + 16.0f * s + g, y1 + g, 6.0f + 42.0f, 6.0f + 42.0f, 0x80);
    }
    Td_Capsule(x0 + 40.0f + r * 1.35f, cy, r, icon);
    room = (value != NULL ? vx0 : x1 - 40.0f) - lx - 24.0f;
    if (on && hint != NULL && hint[0] != 0 && Td_WordsW(hint, TD_C, 28.0f) <= room) {
        Td_Words(lx, cy - 18.0f, label, TD_B, 56.0f, TD_DARK, 0, room, 0);
        Td_Words(lx + 2.0f, cy + 30.0f, hint, TD_C, 28.0f, TD_TEXT(70, 26, 0), 0, room, 0);
    } else {
        Td_Words(lx, cy + 2.0f, label, TD_B, 56.0f, on ? TD_DARK : TD_GREEN, 0, room, 0);
    }
    if (value == NULL) {
        return;
    }
    Td_Nine(on ? TBO_TD_VAL_ON : TBO_TD_VAL, vx0, y0 + 12.0f, vx1, y1 - 12.0f, 2.0f, 14.0f, 0x80);
    Td_Words((vx0 + vx1) * 0.5f, cy - (total > 1 ? 8.0f : 0.0f), value, TD_B, 50.0f,
             on ? Td_Ink(TD_RGB(255, 255, 240), TD_RGB(255, 255, 240), TD_RGB(10, 6, 30)) : Td_Ink(TD_RGB(150, 236, 255), TD_RGB(150, 236, 255), TD_RGB(10, 6, 30)),
             1, vw - 110.0f, raw);
    if (total > 1) {
        u32 a = TD_RGB(255, 220, 120);
        f32 dw = 22.0f, d0 = (vx0 + vx1) * 0.5f - (f32)(total - 1) * dw * 0.5f;
        s32 k;

        Td_Tri(vx0 + 16.0f, cy, vx0 + 40.0f, cy - 18.0f, vx0 + 40.0f, cy + 18.0f, a);
        Td_Tri(vx1 - 16.0f, cy, vx1 - 40.0f, cy - 18.0f, vx1 - 40.0f, cy + 18.0f, a);
        if (total <= 12 && idx >= 0) { /* (idx < 0: the arrows alone, where the place among the choices is not known) */
            for (k = 0; k < total; k++) {
                u32 c = k == idx ? TD_RGB(255, 200, 80) : TD_RGB(90, 90, 120);
                Td_Disc(d0 + (f32)k * dw, y1 - 30.0f, k == idx ? 6.0f : 4.0f, c, c);
            }
        }
    }
}

static void Td_Key(f32 x0, f32 y0, f32 x1, f32 y1, char *label, s32 on, f32 size, s32 raw) {
    if (on) {
        Td_Nine(TBO_TD_KEY_ON, x0, y0, x1, y1, 14.0f, 14.0f + 22.0f, 0x80);
        Td_Words((x0 + x1) * 0.5f, (y0 + y1) * 0.5f + 2.0f, label, TD_B, size, Td_Ink(TD_RGB(255, 255, 240), TD_RGB(255, 226, 170), TD_RGB(110, 30, 0)), 1,
                 x1 - x0 - 16.0f, raw);
    } else {
        Td_Nine(TBO_TD_KEY, x0, y0, x1, y1, 2.0f, 24.0f, 0x80);
        Td_Words((x0 + x1) * 0.5f, (y0 + y1) * 0.5f + 2.0f, label, TD_B, size, TD_GREEN, 1, x1 - x0 - 16.0f, raw);
    }
}

static void Td_Chip(f32 x0, f32 y0, f32 x1, f32 y1, char *label, s32 on, f32 size, s32 alpha) {
    if (on) {
        Td_Nine(TBO_TD_CHIP_ON, x0, y0, x1, y1, 14.0f, 14.0f + 18.0f, alpha);
        Td_Words((x0 + x1) * 0.5f, (y0 + y1) * 0.5f + 2.0f, label, TD_B, size, Td_Ink(TD_RGB(255, 255, 240), TD_RGB(255, 226, 170), TD_RGB(100, 20, 0)), 1,
                 x1 - x0 - 20.0f, 0);
    } else {
        Td_Nine(TBO_TD_CHIP, x0, y0, x1, y1, 2.0f, 20.0f, alpha);
        Td_Words((x0 + x1) * 0.5f, (y0 + y1) * 0.5f + 2.0f, label, TD_B, size,
                 alpha < 0x80 ? Td_Ink(TD_RGB(150, 150, 170), TD_RGB(120, 120, 140), TD_RGB(10, 10, 30)) : Td_Ink(TD_RGB(220, 230, 255), TD_RGB(160, 180, 230), TD_RGB(10, 10, 30)),
                 1, x1 - x0 - 20.0f, 0);
    }
}

/* the big orange button; `on` it is under the cursor (it glows brighter), `alpha` lower: it cannot be pressed */
static void Td_BigButton(f32 x0, f32 y0, f32 x1, f32 y1, char *label, f32 size, s32 on, s32 alpha) {
    f32 s = (y1 - y0) / 120.0f, g = 20.0f * s, sk = 24.0f * s;

    Td_Three(TBO_TD_BTN, x0 - sk - g, y0 - g, x1 + sk + g, y1 + g, 20.0f + 48.0f + 10.0f, 20.0f + 48.0f + 10.0f, alpha);
    if (on) {
        f32 p[8];
        Td_Skew(p, x0 - 4.0f, y0 - 4.0f, x1 + 4.0f, y1 + 4.0f, sk);
        Td_Outline(p, 4, 5.0f, TD_RGB(255, 250, 220));
        Td_Tri(x0 - 70.0f, (y0 + y1) * 0.5f - 24.0f, x0 - 30.0f, (y0 + y1) * 0.5f, x0 - 70.0f, (y0 + y1) * 0.5f + 24.0f, TD_RGB(255, 200, 80));
    }
    Td_Words((x0 + x1) * 0.5f, (y0 + y1) * 0.5f + 2.0f, label, TD_B, size,
             Td_Ink(Td_Rgba(255, 255, 240, alpha * 2 - 1), Td_Rgba(255, 220, 150, alpha * 2 - 1), Td_Rgba(100, 10, 0, alpha * 2 - 1)), 1, x1 - x0 - 40.0f, 0);
}

/* Future Gohan (the film), standing with his knees on `feet`, `height` pixels from his hair down */
static void Td_Gohan(f32 cx, f32 feet, f32 height) {
    f32 s = height / 800.0f;
    Td_Pic(TBO_GOHAN, cx - 256.0f * s, feet - 832.0f * s, cx + 256.0f * s, feet, 0.0f, 0.0f, 1.0f, 1.0f, 0x80);
}

/* the time tunnel with its dark edges; `dim` darker still (a window over the page) */
static void Td_Back(s32 dim) {
    Td_Pic(TBO_TD_VIGNETTE, 0.0f, 0.0f, TD_W, TD_H, 0.0f, 0.0f, 1.0f, 1.0f, 0x80);
    if (dim > 0) {
        u32 c = Td_Rgba(4, 0, 18, dim);
        Td_Rect(0.0f, 0.0f, TD_W, TD_H, c, c);
    }
}

/* the title at the top left (the game's name small over the screen's), the player's plate */
static void Td_TopBar(char *page, char *sub) {
    static char rating[32];
    Bt4RankedView rv = {0};
    f32 fs = 108.0f, tw;

    Td_Pic(TBO_TD_BAND, 440.0f, -16.0f, 440.0f + 890.0f, -16.0f + 214.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0x80);
    Td_Words(554.0f, 44.0f, sub, TD_C, 32.0f, Td_Ink(TD_RGB(150, 226, 255), TD_RGB(110, 180, 255), TD_NONE), 0, 700.0f, 1);
    tw = Td_WordsW(page, TD_B, 108.0f);
    if (tw > 710.0f) {
        fs = 108.0f * 710.0f / tw;
    }
    Td_Words(540.0f, 112.0f, page, TD_B, fs, TD_GOLD, 0, 760.0f, 0);
    Td_Pic(TBO_TD_PLATE, 1510.0f, 22.0f, 2105.0f, 150.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0x80);
    Td_Words(1636.0f, 62.0f, "FIGHTER", TD_C, 26.0f, TD_TEXT(120, 230, 160), 0, 0.0f, 0);
    Td_Words(1634.0f, 106.0f, (char *)Port_NetNameGet(), TD_B, 62.0f, Td_Ink(TD_RGB(240, 255, 242), TD_RGB(156, 244, 180), TD_RGB(0, 30, 10)), 0, 330.0f, 1);
    if (!getenv("BT4_RENDER_TEST")) {
        Port_RankedView(&rv);
    }
    if (rv.state == 1) {
        sprintf(rating, Tb_Fmt("RATING %d"), rv.rating);
        Td_Words(2079.0f, 62.0f, rating, TD_C, 24.0f, Td_Ink(TD_RGB(255, 220, 120), TD_RGB(255, 170, 60), TD_NONE), 2, 0.0f, 1);
    }
}

/* a caption's lines, broken where they fit `room` pixels at `size` */
static s32 Td_Wrap(char *text, char lines[3][160], f32 size, f32 room) {
    s32 at = 0, row = 0, used = 0, k;
    char trial[160];

    for (k = 0; k < 3; k++) lines[k][0] = 0;
    while (text[at] != 0) {
        s32 end, n = used;
        while (text[at] == ' ') at++;
        if (text[at] == 0) break;
        end = at;
        while (text[end] != 0 && text[end] != ' ') end++;
        for (k = 0; k < used; k++) trial[k] = lines[row][k];
        if (n > 0 && n < 158) trial[n++] = ' ';
        for (k = at; k < end && n < 158; k++) trial[n++] = text[k];
        trial[n] = 0;
        PortOnline_Raw(1);
        if (used > 0 && Td_WordsW(trial, TD_C, size) > room) {
            PortOnline_Raw(0);
            if (++row >= 3) return 4;
            used = 0;
            continue;
        }
        PortOnline_Raw(0);
        for (k = 0; k <= n; k++) lines[row][k] = trial[k];
        used = n;
        at = end;
    }
    return row + 1;
}

/* the box at the foot: what the screen's guide says, and the buttons the screen answers to (its prompts, from the
   right), the hub's Discord button under them. Sets the Discord button's place for the touch screen. */
static void Td_BottomBox(char *caption, s32 discord, s32 live) {
    static char lines[3][160];
    static char last[TB_BOX_LINE];
    static s32 count;
    static f32 size;
    static f32 lastRoom;
    static unsigned lastGen;
    f32 x0 = 18.0f, y0 = 968.0f, x1 = 2694.0f, y1 = 1206.0f;
    f32 x = x1 - 40.0f, left = x1, py, room;
    s32 k, same = 1;

    Td_Nine(TBO_TD_BOX, x0, y0, x1, y1, 14.0f, 54.0f, 0x80);
    if (discord) {
        static char label[96];
        extern int Port_TrLine(const char *original, char *out, int size);
        f32 tw, bx0, p[16];

        if (!Port_TrLine("Join our Discord Server!", label, sizeof(label))) {
            Ps_Copy(label, (char *)PortOnline_Text("Join our Discord Server!"), sizeof(label));
        }
        tw = Td_WordsW(label, TD_B, 40.0f);
        bx0 = x - tw - 110.0f;
        Td_Chamfer(p, bx0, y0 + 148.0f, x, y0 + 214.0f, 14.0f);
        Td_Poly(p, 8, TD_RGB(120, 110, 255), TD_RGB(70, 60, 200));
        Ps_Icon(TBO_DISCORD, Td_X(bx0 + 45.0f), Td_Y(y0 + 181.0f), Td_Ht(50.0f), 0x80);
        Td_Words(x - 24.0f, y0 + 182.0f, label, TD_B, 40.0f, Td_Ink(TD_RGB(255, 255, 255), TD_RGB(220, 220, 255), TD_RGB(30, 20, 90)), 2, 0.0f, 1);
        PortTbo_CommunityBounds(live && !sTb->leaving, Ps_X(Td_X(bx0)), Ps_Y(Td_Y(y0 + 148.0f)), Ps_X(Td_X(x)), Ps_Y(Td_Y(y0 + 214.0f)));
        left = bx0;
    }
    py = discord ? y0 + 70.0f : y0 + 118.0f;
    for (k = Ps_PromptCount() - 1; k >= 0; k--) {
        char *lab = Ps_PromptLabel(k);
        s32 id = Ps_ButtonPic(Ps_PromptKind(k));
        f32 tw = Td_WordsW(lab, TD_B, 46.0f), bw = 64.0f * Ps_PicW(id) / Ps_PicH(id);

        Td_Words(x, py, lab, TD_B, 46.0f, Td_Ink(TD_RGB(255, 255, 255), TD_RGB(220, 230, 255), TD_RGB(10, 6, 30)), 2, 0.0f, 0);
        x -= tw + 14.0f;
        Ps_Icon(id, Td_X(x - bw * 0.5f), Td_Y(py), Td_Ht(64.0f), 0x80);
        x -= bw + 34.0f;
    }
    left = discord ? (x < left ? x : left) : x;
    Td_Line(left - 10.0f, y0 + 40.0f, left - 10.0f, y1 - 40.0f, 3.0f, Td_Rgba(255, 210, 120, 160));
    room = left - 60.0f - 70.0f;
    if (caption == NULL) {
        return;
    }
    for (k = 0; k < TB_BOX_LINE - 1 && (caption[k] != 0 || last[k] != 0); k++) { /* (as far as `last` holds: a longer line is
                                                                                    the same when its start is) */
        if (caption[k] != last[k]) {
            same = 0;
            break;
        }
    }
    if (!same || room != lastRoom || PortTbo_TextGeneration() != lastGen) {
        for (k = 0; caption[k] != 0 && k < TB_BOX_LINE - 1; k++) last[k] = caption[k];
        last[k] = 0;
        lastRoom = room;
        lastGen = PortTbo_TextGeneration();
        size = 50.0f;
        count = Td_Wrap(caption, lines, size, room);
        if (count > 2) {
            size = 42.0f;
            count = Td_Wrap(caption, lines, size, room);
        }
        if (count > 3) {
            count = 3;
        }
    }
    for (k = 0; k < count; k++) {
        f32 ly = count == 1 ? y0 + 118.0f : count == 2 ? y0 + 82.0f + 72.0f * (f32)k : y0 + 52.0f + 66.0f * (f32)k;
        Td_Words(70.0f, ly, lines[k], TD_C, size, TD_TEXT_EDGE(240, 244, 255), 0, room, 1);
    }
}

/* a page's guide line, as the box has said it (the game's box keeps it for the screen reader) */
static char *Td_Caption(void) {
    static char translated[TB_BOX_LINE];
    extern int Port_TrLine(const char *, char *, int);
    /* what the hosts say just now (a warning of the room, a service's answer), else the page's guide */
    char *text = (char *)PortOnline_Text(sTb->say > 0 && sTb->sayLine[0] != 0 ? sTb->sayLine : Tb_Help());

    if (Port_TrLine(text, translated, sizeof(translated))) {
        text = translated;
    }
    return text;
}

static void Td_Card(f32 x0, f32 y0, f32 x1, f32 y1, char *title, char *sub, s32 icon, s32 on, f32 big) {
    f32 ty = (y0 + y1) * 0.5f;
    if (on) {
        f32 p[16];
        Td_Chamfer(p, x0 - 10.0f, y0 - 10.0f, x1 + 10.0f, y1 + 10.0f, 34.0f);
        Td_Poly(p, 8, Td_Rgba(255, 150, 40, 90), Td_Rgba(255, 150, 40, 90));
    }
    Td_Window(x0, y0, x1, y1, NULL);
    if (on) {
        f32 p[16];
        Td_Chamfer(p, x0, y0, x1, y1, 30.0f);
        Td_Outline(p, 8, 6.0f, TD_RGB(255, 180, 60));
    }
    Td_Capsule(x0 + 130.0f, ty, 62.0f, icon);
    Td_Words(x0 + 250.0f, ty - 34.0f, title, TD_B, big, on ? TD_GOLD_FLAT : TD_GREEN, 0, x1 - x0 - 290.0f, 0);
    Td_Words(x0 + 254.0f, ty + 50.0f, sub, TD_C, 36.0f, TD_SKY(255), 0, x1 - x0 - 290.0f, 0);
}

static void Td_Radar(f32 cx, f32 cy, f32 h) {
    s32 f = (sTb->frames * 5 / 24) % TB_RADAR_FRAMES;
    f32 w = h * 549.0f / 618.0f, u = 1.0f / (f32)TB_RADAR_COLS, v = 248.0f / 2728.0f;
    f32 cu = (f32)(f % TB_RADAR_COLS) * u, cv = (f32)(f / TB_RADAR_COLS) * v;

    Td_Disc(cx, cy + h * 0.05f, h * 0.55f, Td_Rgba(80, 255, 120, 90), Td_Rgba(80, 255, 120, 0));
    Td_Pic(TBO_RADAR, cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f, cu, cv, cu + u, cv + v, 0x80);
}

static void Td_Keypad(f32 x0, f32 y0, s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb, f32 kw, f32 kh, f32 gap, f32 size, s32 skipRow) {
    char one[2] = {0, 0};
    s32 r, c, k;
    f32 by = y0 + (f32)rows * (kh + gap) + 6.0f;

    for (r = 0; r < rows; r++) {
        for (c = 0; c < KEY_COLS; c++) {
            f32 x = x0 + (f32)c * (kw + gap), y = y0 + (f32)r * (kh + gap);
            if (keys[r][c] == ' ') continue;
            one[0] = keys[r][c];
            Td_Key(x, y, x + kw, y + kh, one, r == sTb->keyRow && c == sTb->keyCol, size, 1);
        }
    }
    for (k = 0; k < nb; k++) {
        f32 x = x0 + (f32)b[k].c0 * (kw + gap), xe = x0 + (f32)b[k].c1 * (kw + gap) + kw;
        f32 y = by + (f32)b[k].row * (kh + gap);
        s32 on = sTb->keyRow == rows + b[k].row && sTb->keyCol >= b[k].c0 && sTb->keyCol <= b[k].c1;
        if (b[k].row == skipRow) continue;
        Td_Key(x, y, xe, y + kh, b[k].label, on, size * 0.8f, 0);
    }
}

/* the champion or a player's plate (the rival found) */
static void Td_Plate(f32 x0, f32 y0, f32 x1, f32 y1, char *tag, char *name, char *sub, u32 col) {
    Td_Window(x0, y0, x1, y1, NULL);
    Td_Disc(x0 + 120.0f, y0 + 120.0f, 80.0f, TD_RGB(255, 220, 120), TD_RGB(255, 220, 120));
    Td_Disc(x0 + 120.0f, y0 + 120.0f, 70.0f, col, col);
    Td_Words(x0 + 120.0f, y0 + 122.0f, tag, TD_B, 84.0f, Td_Ink(TD_RGB(255, 255, 255), TD_RGB(220, 230, 255), TD_RGB(10, 10, 40)), 1, 0.0f, 1);
    Td_Words(x0 + 240.0f, y0 + 100.0f, name, TD_B, 86.0f, TD_WHITE, 0, x1 - x0 - 280.0f, 1);
    Td_Words(x0 + 244.0f, y0 + 170.0f, sub, TD_C, 34.0f, TD_SKY(255), 0, x1 - x0 - 280.0f, 1);
}

/* ------------------------------------------------------------------------------------------------ the hub */
static const char *sTdAge[HUB_COUNT] = {"AGE 767", "AGE 774", "AGE 784", "AGE 785", "AGE 788", "AGE 790", "AGE 794"};

static void Td_Hub(s32 live) {
    f32 in = Tb_In(0);
    f32 slide = (1.0f - in) * 700.0f, gs = (1.0f - in) * -260.0f;
    f32 warp = sTb->warp > 0 ? (f32)sTb->warp / (f32)TB_WARP_FRAMES : 0.0f;
    f32 lx0 = 630.0f, lx1 = 1800.0f, ly0 = 342.0f, ly1 = 860.0f, rh = (ly1 - ly0 - 30.0f) / 7.0f;
    f32 dx = 2230.0f, dy = 766.0f, dr = 150.0f;
    f32 target = (f32)sTb->hubCur, a, glow;
    s32 online = getenv("BT4_RENDER_TEST") ? -1 : Port_OnlinePresence(1), i;
    static char line[64];

    if (!sTb->tmInit) {
        sTb->tmInit = 1;
        sTb->tmRow = target;
        sTb->tmNeedle = target;
    }
    sTb->tmRow += (target - sTb->tmRow) * 0.35f;
    sTb->tmNeedle += (target - sTb->tmNeedle) * 0.18f;
    Td_Back(0);
    /* the jump: the tunnel's light streams out, more and longer as it goes on (under the machine) */
    if (warp > 0.0f) {
        f32 tx, ty;
        Tm_Tunnel(&tx, &ty);
        for (i = 0; i < 40; i++) {
            f32 an = (f32)i * 0.15708f + (f32)((i * 7) % 5) * 0.05f;
            f32 r0 = 14.0f + 170.0f * warp + (f32)((i * 13) % 7) * 6.0f;
            f32 r1 = r0 + 20.0f + 300.0f * warp * warp;
            s32 al = (s32)(0x60 * warp);
            Tm_Beam(tx + cosf(an) * r0 * 0.857f, ty + sinf(an) * r0 * 0.75f, tx + cosf(an) * r1 * 0.857f, ty + sinf(an) * r1 * 0.75f, 0.7f,
                    Ps_Rgba(0xD0, 0xF0, 0xFF, al), Ps_Rgba(0xFF, 0xFF, 0xFF, 0));
        }
        Ps_Disc(tx, ty, 26.0f + 60.0f * warp, Ps_Rgba(0xFF, 0xFF, 0xFF, (s32)(0x70 * warp)), Ps_Rgba(0x90, 0xD0, 0xFF, 0));
    }
    Td_Gohan(300.0f + gs, 990.0f + 4.0f * Tb_Beat(150), 700.0f);
    Td_TopBar("Online", "TENKAICHI BUDOKAI");
    /* the machine */
    Td_Pic(TBO_TD_MACHINE, 520.0f + slide, 176.0f, 2712.0f + slide, 990.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0x80);
    /* fighters online */
    {
        f32 p[16];
        Td_Chamfer(p, 1290.0f + slide, 160.0f, 1840.0f + slide, 214.0f, 12.0f);
        Td_Poly(p, 8, Td_Rgba(20, 60, 30, 240), Td_Rgba(6, 24, 12, 245));
        Td_Outline(p, 8, 2.0f, Td_Rgba(120, 255, 150, 200));
        Td_Disc(1323.0f + slide, 187.0f, 11.0f, TD_RGB(80, 255, 110), TD_RGB(80, 255, 110));
        Ps_Copy(line, (char *)PortOnline_Text("Fighters online: "), sizeof(line));
        if (online < 0) {
            Ps_Cat(line, "...", sizeof(line));
        } else {
            static char n[16];
            sprintf(n, "%d", online);
            Ps_Cat(line, n, sizeof(line));
        }
        Td_Words(1350.0f + slide, 188.0f, line, TD_B, 44.0f, Td_Ink(TD_RGB(220, 255, 200), TD_RGB(100, 232, 80), TD_RGB(8, 60, 16)), 0, 470.0f, 1);
    }
    {   /* its words (in the chosen language) */
        TdInk plate = Td_Ink(TD_RGB(96, 56, 12), TD_RGB(56, 30, 4), TD_RGB(255, 240, 190));
        Td_Words(732.0f + slide, 260.0f, "TIME MACHINE", TD_B, 84.0f, plate, 0, 560.0f, 0);
        Td_Words(1320.0f + slide, 262.0f, "CAPSULE CORP. \xE2\x80\xA2 DESTINATION SELECT", TD_C, 30.0f, TD_TEXT(110, 70, 20), 0, 900.0f, 0);
        Td_Words(2610.0f + slide, 260.0f, "HOPE!!", TD_B, 90.0f, Td_Ink(TD_RGB(220, 40, 30), TD_RGB(150, 10, 10), TD_RGB(255, 240, 190)), 2, 380.0f, 0);
        Td_Words(1880.0f + slide, 384.0f, "DESTINATION", TD_C, 30.0f, Td_Ink(Td_Rgba(120, 255, 160, 200), Td_Rgba(120, 255, 160, 200), TD_NONE), 0, 650.0f, 0);
        Td_Words(1870.0f + slide, 530.0f, "TIME ENERGY", TD_C, 28.0f, TD_TEXT(110, 70, 20), 0, 560.0f, 0);
    }
    glow = warp > 0.0f ? 0.5f + 0.5f * (f32)((sTb->warp / 3) & 1) : 0.5f + 0.5f * sinf((f32)sTb->frames * 0.1f);
    {   /* the destination under the cursor: an orange bar that glides, its glow pulsing */
        f32 y = ly0 + 15.0f + sTb->tmRow * rh, a0 = y + 6.0f, b0 = y + rh - 6.0f;
        f32 g = 18.0f * (b0 - a0) / 126.0f;
        u32 c = Td_Rgba(255, 200, 90, (s32)(60.0f * glow));
        Td_Rect(lx0 + 20.0f + slide, a0 - 8.0f, lx1 - 20.0f + slide, b0 + 8.0f, c, c);
        Td_Three(TBO_TD_OPT_ON, lx0 + 34.0f - 12.0f - g + slide, a0 - g, lx1 - 34.0f + 12.0f + g + slide, b0 + g, 18.0f + 42.0f, 18.0f + 42.0f, 0x80);
        Td_Tri(lx0 + 50.0f + slide, (a0 + b0) * 0.5f - 18.0f, lx0 + 76.0f + slide, (a0 + b0) * 0.5f, lx0 + 50.0f + slide, (a0 + b0) * 0.5f + 18.0f,
               TD_RGB(60, 20, 0));
    }
    for (i = 0; i < HUB_COUNT; i++) {
        f32 y = ly0 + 15.0f + (f32)i * rh, cy = y + rh * 0.5f + 2.0f;
        s32 on = i == sTb->hubCur && fabsf(sTb->tmRow - (f32)i) < 0.5f;
        Td_Words(lx0 + 90.0f + slide, cy, (char *)sTdAge[i], TD_B, 54.0f, on ? TD_DARK_FLAT : TD_AGE, 0, 290.0f, 0);
        Td_Words(lx0 + 400.0f + slide, cy, sTbHubName[i], TD_B, on ? 62.0f : 58.0f, on ? TD_DARK : TD_GREEN, 0, 720.0f, 0);
    }
    /* the visor, the needle, the energy */
    Td_Words(1880.0f + slide, 446.0f, (char *)sTdAge[sTb->hubCur], TD_B, 76.0f, TD_CYAN, 0, 330.0f, 0);
    Td_Words(2580.0f + slide, 446.0f, sTbHubName[sTb->hubCur], TD_B, 70.0f, TD_ORANGE, 2, 380.0f, 0);
    {
        f32 e = warp > 0.0f ? 1.0f - warp : 1.0f, ex0 = 1874.0f + slide, ex1 = 2586.0f + slide;
        Td_Words(2590.0f + slide, 530.0f, e >= 0.995f ? "100%" : e > 0.5f ? "75%" : e > 0.25f ? "40%" : "10%", TD_C, 28.0f, TD_TEXT(110, 70, 20), 2, 0.0f, 1);
        Ps_Quad(Td_X(ex0), Td_Y(554.0f), Td_X(ex0 + (ex1 - ex0) * e), Td_Y(554.0f), Td_X(ex0), Td_Y(578.0f), Td_X(ex0 + (ex1 - ex0) * e), Td_Y(578.0f),
                TD_RGB(130, 240, 255), TD_RGB(220, 110, 255), TD_RGB(130, 240, 255), TD_RGB(220, 110, 255));
    }
    a = -140.0f + 280.0f * sTb->tmNeedle / (f32)(HUB_COUNT - 1);
    if (warp > 0.0f && warp < 0.5f) {
        a += sinf((f32)sTb->warp * 1.3f) * 6.0f * (1.0f - warp * 2.0f);
    }
    a *= 0.0174533f;
    Td_Line(dx + slide, dy, dx + slide + sinf(a) * dr * 0.78f, dy - cosf(a) * dr * 0.78f, 10.0f, TD_RGB(255, 90, 30));
    Td_Disc(dx + slide, dy, 22.0f, TD_RGB(120, 70, 10), TD_RGB(120, 70, 10));
    Td_Disc(dx + slide, dy, 19.0f, TD_RGB(255, 220, 110), TD_RGB(255, 220, 110));
    if (warp > 0.0f) {
        f32 p = warp * 4.0f > 1.0f ? 1.0f : warp * 4.0f;
        f32 sh = dr * (1.6f + 0.8f * (1.0f - p)), sw = sh * Ps_PicW(TBO_TM_STAMP) / Ps_PicH(TBO_TM_STAMP);
        Td_Pic(TBO_TM_STAMP, dx - sw * 0.5f, dy - sh * 0.5f, dx + sw * 0.5f, dy + sh * 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, (s32)(0x80 * p));
    }
    Td_BottomBox(Td_Caption(), 1, live);
    if (warp > 0.72f) {
        u32 c = Ps_Rgba(0xFF, 0xFF, 0xFF, (s32)(0x80 * (warp - 0.72f) / 0.28f));
        Ps_Rect(Ps_Left(), 0.0f, 512.0f - Ps_Left(), 448.0f, c, c);
    }
}

/* ------------------------------------------------------------------------------------------------ the pages */
#define TD_GX 2330.0f /* Gohan on the pages: on the right, his knees on the box */

static void Td_Page(char *title, s32 gohan, s32 dim) {
    Td_Back(dim);
    if (gohan) {
        Td_Gohan(TD_GX, 1000.0f + 4.0f * Tb_Beat(150), 700.0f);
    }
    Td_TopBar(title, "TENKAICHI BUDOKAI ONLINE");
}

static void Td_Ranked(void) {
    static char line[96];
    Bt4RankedView rv = {0};
    s32 i;

    if (!getenv("BT4_RENDER_TEST")) {
        Port_RankedView(&rv);
    }
    Td_Page(sTbTitle[TB_RANKED], 1, 0);
    if (sTb->rankedPage == 0) {
        static char *hint[3] = {"ONE ON ONE OR NATIVE TEAMS", "THE BEST OF THE WEEK", "POINTS, SEASONS, RESETS"};
        f32 cx = 1570.0f, cy = 520.0f, r = 190.0f, k;
        Td_Window(40.0f, 230.0f, 1100.0f, 920.0f, "SEASON MENU");
        for (i = 0; i < 3; i++) {
            f32 y = 310.0f + 190.0f * (f32)i;
            Td_Option(110.0f, y, 1040.0f, y + 150.0f, sTbRankedActions[i], NULL, 0, 1, i == sTb->rankedCur, i + 1, hint[i], 0);
        }
        Td_Window(1150.0f, 230.0f, 1990.0f, 920.0f, "COMPETITIVE SEASON");
        Td_Disc(cx, cy, r + 30.0f, Td_Rgba(255, 170, 60, 70), Td_Rgba(255, 170, 60, 0));
        Td_Disc(cx, cy, r, TD_RGB(255, 210, 120), TD_RGB(255, 210, 120));
        Td_Disc(cx, cy, r - 8.0f, TD_RGB(20, 14, 50), TD_RGB(20, 14, 50));
        for (k = 0.0f; k < 54.0f; k += 1.0f) { /* the season's ring: 270 degrees, the part reached orange */
            f32 a0 = (135.0f + 5.0f * k) * 0.0174533f, a1 = (135.0f + 5.0f * (k + 1.0f)) * 0.0174533f, ri = r - 46.0f, ro = r - 24.0f;
            f32 reach = rv.state == 1 ? (f32)(rv.rating > 2000 ? 2000 : rv.rating < 0 ? 0 : rv.rating) / 2000.0f * 54.0f : 0.0f;
            u32 c = k < reach ? TD_RGB(255, 150, 40) : TD_RGB(60, 50, 110);
            Ps_Quad(Td_X(cx + cosf(a0) * ri), Td_Y(cy + sinf(a0) * ri), Td_X(cx + cosf(a0) * ro), Td_Y(cy + sinf(a0) * ro), Td_X(cx + cosf(a1) * ri),
                    Td_Y(cy + sinf(a1) * ri), Td_X(cx + cosf(a1) * ro), Td_Y(cy + sinf(a1) * ro), c, c, c, c);
        }
        Td_Words(cx, cy - 70.0f, "RATING", TD_C, 34.0f, TD_SKY(255), 1, 0.0f, 0);
        if (rv.state == 1) {
            sprintf(line, "%d", rv.rating);
        } else {
            Ps_Copy(line, "...", sizeof(line));
        }
        Td_Words(cx, cy + 20.0f, line, TD_B, 150.0f, TD_GOLD, 1, 320.0f, 1);
        sprintf(line, Tb_Fmt("W %d  L %d"), rv.wins, rv.losses);
        Td_Words(cx, cy + 112.0f, rv.state == 1 ? line : rv.state < 0 ? "Ranked service unavailable" : "Loading Ranked status...", TD_C, 32.0f,
                 TD_TEXT(200, 200, 230), 1, 330.0f, rv.state == 1);
        Td_Words(1220.0f, 760.0f, "Battle", TD_C, 38.0f, TD_SKY(255), 0, 300.0f, 0);
        Td_Words(1930.0f, 760.0f, "One on One", TD_C, 38.0f, TD_TEXT(255, 230, 170), 2, 380.0f, 0);
        Td_Words(1220.0f, 812.0f, "Weekly reset: Sunday 23:59", TD_C, 34.0f, TD_SKY(255), 0, 700.0f, 0);
        Td_Words(1220.0f, 860.0f, "Argentina time (UTC-3)", TD_C, 34.0f, TD_TEXT(255, 230, 170), 0, 700.0f, 0);
    } else if (sTb->rankedPage == 1) {
        static const u32 medal[3] = {0, 0, 0};
        (void)medal;
        Td_Window(40.0f, 230.0f, 1990.0f, 920.0f, "Global Top");
        Td_Words(110.0f, 300.0f, "#", TD_C, 36.0f, TD_SKY(255), 0, 0.0f, 1);
        Td_Words(260.0f, 300.0f, "FIGHTER", TD_C, 36.0f, TD_SKY(255), 0, 0.0f, 0);
        Td_Words(1900.0f, 300.0f, "POINTS", TD_C, 36.0f, TD_SKY(255), 2, 0.0f, 0);
        if (rv.count > 0) {
            for (i = 0; i < rv.count && i < 5; i++) {
                f32 y = 360.0f + 104.0f * (f32)i, p[8];
                s32 place = rv.page * 5 + i + 1;
                Td_Skew(p, 90.0f, y, 1940.0f, y + 86.0f, 16.0f);
                Td_Poly(p, 4, i % 2 == 0 ? Td_Rgba(34, 30, 80, 230) : Td_Rgba(24, 20, 60, 230), Td_Rgba(14, 12, 40, 235));
                sprintf(line, "%d", place);
                if (place <= 3) {
                    u32 mc = place == 1 ? TD_RGB(255, 210, 60) : place == 2 ? TD_RGB(210, 220, 235) : TD_RGB(220, 140, 70);
                    Td_Disc(153.0f, y + 43.0f, 36.0f, TD_RGB(90, 60, 10), TD_RGB(90, 60, 10));
                    Td_Disc(153.0f, y + 43.0f, 33.0f, mc, mc);
                    Td_Words(153.0f, y + 44.0f, line, TD_B, 50.0f, TD_DARK_FLAT, 1, 0.0f, 1);
                } else {
                    Td_Words(153.0f, y + 44.0f, line, TD_B, 50.0f, Td_Ink(TD_RGB(220, 230, 255), TD_RGB(160, 180, 230), TD_RGB(10, 10, 30)), 1, 0.0f, 1);
                }
                Td_Words(260.0f, y + 44.0f, rv.top[i].name, TD_B, 60.0f, TD_WHITE, 0, 1300.0f, 1);
                sprintf(line, "%d", rv.top[i].rating);
                Td_Words(1900.0f, y + 44.0f, line, TD_B, 62.0f, TD_GOLD_FLAT, 2, 0.0f, 1);
            }
            sprintf(line, Tb_Fmt("Page %d/%d"), rv.page + 1, rv.pages);
            Td_Words(110.0f, 892.0f, line, TD_C, 32.0f, TD_TEXT(180, 200, 240), 0, 0.0f, 1);
            Td_Words(1900.0f, 892.0f, "Updates every 10 minutes.", TD_C, 32.0f, TD_TEXT(180, 200, 240), 2, 900.0f, 0);
        } else {
            Td_Words(1015.0f, 560.0f, rv.topState == 0 ? "Loading Global Top..." : rv.topState < 0 ? "Global Top unavailable" : "No ranking published yet.",
                     TD_B, 70.0f, TD_GREEN, 1, 1700.0f, 0);
            Td_Words(1015.0f, 660.0f, "Updates every 10 minutes.", TD_C, 40.0f, TD_SKY(255), 1, 1700.0f, 0);
        }
    } else {
        static char *big[4] = {"1000", "+ / -", "10 MIN", "SUN"};
        static char *a[4] = {"Start at 1000 points.", "Win to gain points; lose to drop.", "Updates every 10 minutes.", "Weekly reset: Sunday 23:59"};
        static char *b[4] = {"Your points carry over each week.", "Points depend on your rival's rating.", "Global Top", "Argentina time (UTC-3)"};
        Td_Window(40.0f, 230.0f, 1990.0f, 920.0f, "How Ranked Works");
        for (i = 0; i < 4; i++) {
            f32 x0 = 100.0f + 465.0f * (f32)i;
            Td_Window(x0, 330.0f, x0 + 420.0f, 880.0f, NULL);
            Td_Capsule(x0 + 210.0f, 430.0f, 62.0f, i + 1);
            Td_Words(x0 + 210.0f, 600.0f, big[i], TD_B, i < 1 || i == 3 ? 110.0f : 92.0f, TD_GOLD, 1, 380.0f, 1);
            Td_Words(x0 + 210.0f, 730.0f, a[i], TD_C, 30.0f, TD_SKY(255), 1, 390.0f, 0);
            Td_Words(x0 + 210.0f, 782.0f, b[i], TD_C, 30.0f, TD_TEXT(255, 236, 190), 1, 390.0f, 0);
        }
    }
}

static void Td_Create(void) {
    static char value[CR_COUNT][32];
    static char *hint[CR_COUNT] = {"A CODE, OR YOUR ADDRESS", "ONE ON ONE, TEAMS, 10/15/20 DP", "", "", "", "", "", ""};
    s32 idx[CR_COUNT], tot[CR_COUNT], i;

    for (i = 0; i < CR_COUNT; i++) {
        Tb_CreateValue(value[i], i);
    }
    idx[CR_METHOD] = sTb->createAddress; tot[CR_METHOD] = 2;
    idx[CR_BATTLE] = sTb->battle; tot[CR_BATTLE] = 5;
    idx[CR_TIME] = sTb->time; tot[CR_TIME] = 5;
    idx[CR_SPEED] = (sTb->speed - 100) / 10; tot[CR_SPEED] = 6;
    idx[CR_ROLL] = sTb->roll / 2; tot[CR_ROLL] = 5;
    idx[CR_DELAY] = sTb->delay + 1; tot[CR_DELAY] = 8;
    idx[CR_ADDR] = 0; tot[CR_ADDR] = sTb->createAddress ? 2 : 1;
    Td_Page(sTbTitle[TB_CREATE], 0, 0);
    Td_Gohan(2380.0f, 1000.0f + 4.0f * Tb_Beat(150), 700.0f);
    Td_Window(40.0f, 230.0f, 2030.0f, 940.0f, "ROOM RULES");
    for (i = 0; i < CR_COUNT - 1; i++) {
        f32 x0 = 110.0f + 965.0f * (f32)(i % 2), y = 290.0f + 150.0f * (f32)(i / 2);
        Td_Option(x0, y, x0 + 905.0f, y + 126.0f, sTbCreateLabel[i], value[i], idx[i], tot[i], i == sTb->crCur, i + 1, hint[i], 1);
    }
    Td_BigButton(1120.0f, 755.0f, 1940.0f, 870.0f, sTbCreateLabel[CR_OPEN], 80.0f, sTb->crCur == CR_OPEN, 0x80);
}

#define TD_CODE_KEYS 6
static void Td_Join(void) {
    char one[2] = {0, 0};
    s32 c, rows = KEY_ROWS;
    const TbKeyBtn *b = sTb->tagJoin ? sTagJoinBtn : sTbJoinBtn;
    s32 nb = sTb->tagJoin ? 3 : 4;

    Td_Page((char *)((sTb->mode == TB_JOIN && sTb->tagJoin) ? "Join Room" : sTbTitle[TB_JOIN]), 1, 0);
    Td_Window(40.0f, 230.0f, 1990.0f, 950.0f, "ROOM CODE");
    for (c = 0; c < CODE_LEN; c++) {
        f32 x = 120.0f + 190.0f * (f32)c;
        s32 on = c == sTb->codeLen;
        one[0] = c < sTb->codeLen ? sTb->code[c] : (on && (sTb->frames / 16) & 1 ? '_' : 0);
        Td_Key(x, 290.0f, x + 165.0f, 440.0f, one, on, 110.0f, 1);
    }
    if (!sTb->tagJoin) { /* By Address: the keypad's second row of buttons, at the top right */
        s32 on = sTb->keyRow == rows + 1;
        Td_Chip(1300.0f, 310.0f, 1920.0f, 420.0f, "BY ADDRESS", on, 54.0f, 0x80);
    }
    Td_Keypad(120.0f, 480.0f, rows, sTbKeys, b, nb, 165.0f, 72.0f, 20.0f, 58.0f, 1);
}

static void Td_Addr(void) {
    static char line[ADDR_LEN + 2];

    Td_Page(sTbTitle[TB_ADDR], 1, 0);
    Td_Window(40.0f, 230.0f, 1990.0f, 950.0f, "THE OWNER'S ADDRESS:PORT");
    {
        f32 p[16];
        Td_Chamfer(p, 110.0f, 300.0f, 1920.0f, 400.0f, 16.0f);
        Td_Poly(p, 8, Td_Rgba(16, 50, 32, 250), Td_Rgba(6, 20, 12, 252));
        Td_Outline(p, 8, 3.0f, TD_RGB(200, 165, 85));
    }
    Ps_Copy(line, sTb->addr, ADDR_LEN + 2);
    Ps_Cat(line, (sTb->frames / 16) & 1 ? "_" : " ", ADDR_LEN + 2);
    Td_Words(150.0f, 352.0f, line, TD_B, 72.0f, TD_CYAN, 0, 1300.0f, 1);
    Td_Words(1890.0f, 352.0f, "NO :PORT TYPED: PORT 7000", TD_C, 30.0f, TD_TEXT(150, 200, 180), 2, 500.0f, 0);
    Td_Keypad(120.0f, 425.0f, ADDR_ROWS, sTbAddrKeys, sTbAddrBtn, 3, 165.0f, 62.0f, 16.0f, 54.0f, -1);
}

static void Td_Profile(void) {
    extern int MenuTranslation_TboLanguage(void);
    static char *languages[4] = {"Automatic", "English", "Portugu\xC3\xAAs", "Espa\xC3\xB1ol"};
    static char value[32];
    s32 language = MenuTranslation_TboLanguage();

    Td_Page(sTbTitle[TB_PROFILE], 1, 0);
    Td_Window(40.0f, 230.0f, 1990.0f, 920.0f, "PLAYER CARD");
    Td_Disc(220.0f, 420.0f, 110.0f, TD_RGB(255, 220, 120), TD_RGB(255, 220, 120));
    Td_Disc(220.0f, 420.0f, 98.0f, TD_RGB(40, 100, 220), TD_RGB(40, 100, 220));
    Td_Words(220.0f, 422.0f, "1P", TD_B, 110.0f, Td_Ink(TD_RGB(255, 255, 255), TD_RGB(210, 230, 255), TD_RGB(10, 30, 90)), 1, 0.0f, 1);
    Td_Words(370.0f, 380.0f, (char *)Port_NetNameGet(), TD_B, 110.0f, Td_InkOuter(TD_RGB(240, 255, 242), TD_RGB(156, 244, 180), TD_RGB(0, 30, 10), TD_RGB(14, 4, 36), 3.0f), 0,
             1500.0f, 1);
    {   /* under the name: the rating, as the mock-up (when the ranked service has said it) */
        static char line[64];
        Bt4RankedView rv = {0};
        if (!getenv("BT4_RENDER_TEST")) {
            Port_RankedView(&rv);
        }
        if (rv.state == 1) {
            sprintf(line, Tb_Fmt("RATING %d"), rv.rating);
            Td_Words(376.0f, 470.0f, line, TD_C, 36.0f, TD_SKY(255), 0, 1500.0f, 1);
        } else {
            Td_Words(376.0f, 470.0f, "Loading Ranked status...", TD_C, 36.0f, TD_SKY(255), 0, 1500.0f, 0);
        }
    }
    if (language == 0) {
        Ps_Copy(value, (char *)PortOnline_Text("Automatic"), sizeof(value));
    } else {
        Ps_Copy(value, languages[language], sizeof(value));
    }
    Td_Option(110.0f, 580.0f, 1910.0f, 710.0f, "Name", (char *)Port_NetNameGet(), 0, 1, sTb->prCur == PR_NAME, 1, "Type Name", 1);
    Td_Option(110.0f, 750.0f, 1910.0f, 880.0f, "Language", value, language, 4, sTb->prCur == PR_LANG, 2, "AUTOMATIC, ENGLISH, PORTUGU\xC3\x8AS, ESPA\xC3\x91OL", 1);
}

static void Td_Name(void) {
    static char line[40];
    static char count[16];

    Td_Back(120);
    Td_Window(300.0f, 210.0f, 2410.0f, 940.0f, "TYPE YOUR NAME");
    {
        f32 p[16];
        Td_Chamfer(p, 380.0f, 280.0f, 2330.0f, 380.0f, 16.0f);
        Td_Poly(p, 8, Td_Rgba(16, 50, 32, 250), Td_Rgba(6, 20, 12, 252));
        Td_Outline(p, 8, 3.0f, TD_RGB(200, 165, 85));
    }
    Ps_Copy(line, sTb->typed, 40);
    Ps_Cat(line, (sTb->frames / 16) & 1 ? "_" : " ", 40);
    Td_Words(420.0f, 332.0f, line, TD_B, 76.0f, TD_GOLD_FLAT, 0, 1700.0f, 1);
    sprintf(count, "%d / %d", (int)sTb->typedLen, 19);
    Td_Words(2300.0f, 332.0f, count, TD_C, 32.0f, TD_TEXT(150, 200, 180), 2, 0.0f, 1);
    Td_Keypad(400.0f, 405.0f, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4, 200.0f, 64.0f, 16.0f, 56.0f, -1);
}

static void Td_Quick(void) {
    static char line[96];
    static char *tagModes[] = {"One on One", "Free for All", "2 vs 2", "5 vs 5"};
    s32 q, row = 0;
    s32 from = sTb->quickFromTag ? 1 : 0, to = sTb->quickFromTag ? 4 : 2;

    Td_Page(sTbTitle[TB_QUICK], 1, 0);
    Td_Window(40.0f, 230.0f, 1210.0f, 920.0f, "PICK YOUR BATTLE");
    Td_Option(110.0f, 300.0f, 1150.0f, 440.0f, "Battle",
              sTb->quickFromTag ? (char *[]){"ONE ON ONE", "FREE FOR ALL (6)", "2 VS 2", "5 VS 5"}[sTb->queue] : (sTb->queue ? "TEAMS (UP TO 5)" : "ONE ON ONE"),
              sTb->quickFromTag ? sTb->queue - 1 : sTb->queue, sTb->quickFromTag ? 3 : 2, sTb->qkCur == 0, 1, "ONE ON ONE OR TEAMS (UP TO 5)", 0);
    Td_Option(110.0f, 470.0f, 1150.0f, 610.0f, "Region", sTbRegionName[sTb->region], sTb->region, QK_REGIONS, sTb->qkCur == 1, 2, "RIVALS THERE FIRST, THEN ANYWHERE", 0);
    Td_BigButton(150.0f, 680.0f, 1110.0f, 830.0f, "Search", 96.0f, sTb->qkCur == 2, 0x80);
    Td_Window(1250.0f, 230.0f, 1990.0f, 920.0f, "WAITING RIGHT NOW");
    for (q = from; q < to; q++, row++) {
        s32 n = getenv("BT4_RENDER_TEST") ? -1 : sTb->quickFromTag ? Port_TagQueueWaiting(q - 1) : Port_LobbyQueueWaiting(q);
        f32 y = 350.0f + (to - from > 2 ? 120.0f : 170.0f) * (f32)row, f;
        char *label = sTb->quickFromTag ? tagModes[q] : (q ? "Teams" : "One on One");
        u32 bar0 = TD_RGB(30, 30, 70), bar1 = TD_RGB(20, 20, 50);

        Td_Words(1310.0f, y, label, TD_B, 60.0f, q == sTb->queue ? TD_GOLD_FLAT : TD_GREEN, 0, 380.0f, 0);
        if (n < 0) {
            Ps_Copy(line, "...", sizeof(line));
        } else {
            sprintf(line, Tb_Fmt("%d WAITING"), (int)n);
        }
        Td_Words(1930.0f, y, line, TD_B, 48.0f, TD_GOLD_FLAT, 2, 260.0f, 1);
        Td_Rect(1310.0f, y + 46.0f, 1930.0f, y + 68.0f, bar0, bar1);
        f = n > 0 ? (n > 10 ? 1.0f : (f32)n / 10.0f) : 0.0f;
        if (f > 0.0f) {
            Td_Rect(1310.0f, y + 46.0f, 1310.0f + 620.0f * f, y + 68.0f, TD_RGB(120, 255, 150), TD_RGB(40, 180, 80));
        }
    }
    {
        f32 p[16];
        Td_Chamfer(p, 1300.0f, 700.0f, 1940.0f, 860.0f, 16.0f);
        Td_Poly(p, 8, Td_Rgba(20, 40, 80, 230), Td_Rgba(8, 16, 40, 240));
        sprintf(line, Tb_Fmt("PLAYERS THERE FIRST, EVERYONE AFTER %d S"), (int)QK_REGION_WIDEN);
        Td_Words(1620.0f, 780.0f, sTb->queue ? "NO TIME LIMIT - 114% SPEED" : sTb->region == 0 ? "RIVALS NEAR YOU FIRST, THEN ANYWHERE" :
                 sTb->region == QK_REGIONS - 1 ? "THE NEXT RIVAL, WHEREVER: THE FASTEST" : line, TD_C, 30.0f, TD_SKY(255), 1, 600.0f, 0);
    }
}

/* searching, a room open, joining one; the rival found */
static void Td_Wait(void) {
    static char line[96], region[64];
    s32 secs = (sTb->frames - sTb->waitFrom) / 60;
    const char *code = Port_LobbyCode();
    char *title = sTbTitle[TB_WAIT];

    if (sTb->mode == TB_WAIT && !sTb->found && sTb->host != 2) {
        title = sTb->host ? "Room Open" : "Joining Room";
    }
    if (sTb->host == 2) {
        title = sTb->waitBack == TB_RANKED ? "Ranked" : "Quick Match";
    }
    if (sTb->found) { /* the rival is here */
        const char *from = Port_LobbyRivalRegion();
        Td_Page(title, 0, 0);
        Td_Disc(1356.0f, 600.0f, 380.0f, Td_Rgba(255, 160, 40, 110), Td_Rgba(255, 160, 40, 0));
        Td_Plate(60.0f, 330.0f, 1180.0f, 570.0f, "1P", (char *)Port_NetNameGet(), "", TD_RGB(40, 100, 220));
        Tb_Upper(region, from, sizeof(region));
        Td_Plate(1530.0f, 640.0f, 2650.0f, 880.0f, "2P", (char *)PortOnline_Text("YOUR RIVAL IS FROM"), region, TD_RGB(220, 50, 40));
        Td_Words(1356.0f, 600.0f, "VS", TD_B, 300.0f, Td_InkOuter(TD_RGB(255, 250, 210), TD_RGB(255, 120, 20), TD_RGB(110, 20, 0), TD_RGB(14, 4, 36), 6.0f), 1, 0.0f, 1);
        Td_Words(1356.0f, 270.0f, "YOUR RIVAL IS HERE!", TD_B, 110.0f, Td_InkOuter(TD_RGB(220, 255, 200), TD_RGB(100, 232, 80), TD_RGB(8, 60, 16), TD_RGB(14, 4, 36), 3.0f),
                 1, 2400.0f, 0);
        return;
    }
    Td_Page(title, 1, 0);
    if (sTb->host == 2) {
        s32 n = sTb->waitBack == TB_RANKED ? 0 : Port_LobbyQueueWaiting(sTb->queue), widen = Port_LobbyWiden();
        char *l1 = "THE SERVICE PAIRS YOU", *l2 = "WITH THE NEXT RIVAL";
        Td_Window(40.0f, 230.0f, 900.0f, 920.0f, "SEARCHING");
        Td_Words(470.0f, 360.0f, sTb->waitBack != TB_RANKED && sTb->queue ? "TEAMS" : "ONE ON ONE", TD_B, 100.0f, TD_GOLD, 1, 780.0f, 0);
        if (sTb->region > 0 && sTb->region < QK_REGIONS - 1 && widen > 0 && secs >= widen) {
            l1 = "NO ONE THERE:"; l2 = "SEARCHING EVERYWHERE";
        } else if (sTb->region > 0 && sTb->region < QK_REGIONS - 1) {
            l1 = "PLAYERS IN"; l2 = sTbRegionName[sTb->region];
        } else if (sTb->region == 0 && widen > 0 && secs >= widen) {
            l1 = "NO ONE NEARBY:"; l2 = "SEARCHING EVERYWHERE";
        } else if (sTb->region == 0 && widen > 0) {
            l1 = "LOOKING FOR A RIVAL"; l2 = "NEAR YOU FIRST";
        }
        Td_Words(470.0f, 520.0f, l1, TD_C, 44.0f, TD_TEXT(240, 244, 255), 1, 780.0f, 0);
        Td_Words(470.0f, 590.0f, l2, TD_C, 44.0f, TD_SKY(255), 1, 780.0f, 0);
        if (n > 0) {
            sprintf(line, Tb_Fmt("%d IN THIS QUEUE"), (int)n);
            Td_Words(470.0f, 660.0f, line, TD_C, 44.0f, TD_TEXT(120, 255, 150), 1, 780.0f, 1);
        }
        sprintf(line, Tb_Fmt("SEARCHING...  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
    } else if (sTb->host) {
        Td_Window(40.0f, 230.0f, 900.0f, 920.0f, "ROOM CODE");
        if (code[0] != 0) {
            Td_Words(470.0f, 440.0f, (char *)code, TD_B, 170.0f, TD_GOLD, 1, 780.0f, 1);
            Td_Words(470.0f, 640.0f, "GIVE THIS CODE TO", TD_C, 38.0f, TD_SKY(255), 1, 780.0f, 0);
            Td_Words(470.0f, 690.0f, "YOUR RIVAL", TD_C, 44.0f, TD_TEXT(255, 236, 190), 1, 780.0f, 0);
        } else {
            Td_Words(470.0f, 440.0f, "GETTING A CODE...", TD_B, 80.0f, TD_GREEN, 1, 780.0f, 0);
            Td_Words(470.0f, 640.0f, "ASKING THE", TD_C, 38.0f, TD_SKY(255), 1, 780.0f, 0);
            Td_Words(470.0f, 690.0f, "MATCHMAKING SERVICE", TD_C, 38.0f, TD_SKY(255), 1, 780.0f, 0);
        }
        if (Port_LobbyOther()) {
            Td_Words(470.0f, 780.0f, "ANOTHER VERSION TRIED", TD_C, 34.0f, TD_TEXT(255, 160, 120), 1, 780.0f, 0);
        }
        sprintf(line, Tb_Fmt("WAITING...  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
    } else {
        Td_Window(40.0f, 230.0f, 900.0f, 920.0f, "JOINING ROOM");
        Td_Words(470.0f, 430.0f, sTb->code, TD_B, 170.0f, TD_GOLD, 1, 780.0f, 1);
        Td_Words(470.0f, 580.0f, "LOOKING FOR", TD_C, 44.0f, TD_TEXT(240, 244, 255), 1, 780.0f, 0);
        Td_Words(470.0f, 640.0f, "THE HOST...", TD_C, 44.0f, TD_SKY(255), 1, 780.0f, 0);
        sprintf(line, Tb_Fmt("WAITING...  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
    }
    Td_Radar(1400.0f, 580.0f, 640.0f);
    Td_Chip(1130.0f, 870.0f, 1670.0f, 945.0f, line, 1, 50.0f, 0x80);
}

static void Td_Spar(void) {
    static char line[64];
    s32 secs = (sTb->frames - sTb->waitFrom) / 60;

    Td_Page("SPARRING AT THE TOURNAMENT", 1, 0);
    Td_Window(40.0f, 230.0f, 1210.0f, 920.0f, "SPARRING AT THE TOURNAMENT");
    Td_Option(110.0f, 300.0f, 1150.0f, 430.0f, "You", Tb_Fighter(sTb->spMe), 0, 1, sTb->spCur == 0, 1, NULL, 1);
    Td_Option(110.0f, 460.0f, 1150.0f, 590.0f, "Partner", Tb_Fighter(sTb->spFoe), 0, 1, sTb->spCur == 1, 2, NULL, 1);
    Td_BigButton(150.0f, 660.0f, 1110.0f, 800.0f, "Start Training", 80.0f, sTb->spCur == 2, 0x80);
    Td_Window(1250.0f, 230.0f, 1990.0f, 920.0f, sTb->host == 2 ? "QUICK MATCH SEARCH" : "JOINING A LOBBY");
    sprintf(line, Tb_Fmt("SEARCHING  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
    Td_Words(1620.0f, 420.0f, line, TD_B, 70.0f, Td_Ink(TD_RGB(220, 255, 200), TD_RGB(100, 232, 80), TD_RGB(8, 60, 16)), 1, 680.0f, 1);
    Td_Words(1620.0f, 560.0f, "THE MATCH STARTS THE MOMENT", TD_C, 36.0f, TD_TEXT(240, 244, 255), 1, 680.0f, 0);
    Td_Words(1620.0f, 620.0f, "YOUR RIVAL ARRIVES", TD_C, 36.0f, TD_SKY(255), 1, 680.0f, 0);
}

static void Td_Tag(void) {
    static char value[48];
    s32 i;

    Td_Page(sTbTitle[TB_TAG], 1, 0);
    if (!sTb->tagSetup) {
        static char *entry[4] = {"Quick Match", "Create Room", "Join Room", "Local Room"};
        static char *sub[4] = {"FIND A TEAM BATTLE NOW", "YOUR RULES, YOUR TEAMS", "ENTER A ROOM CODE", "PLAY ON THIS DEVICE"};
        for (i = 0; i < Tb_TagEntries(); i++) {
            f32 x0 = 60.0f + 990.0f * (f32)(i % 2), y0 = 250.0f + 350.0f * (f32)(i / 2);
            Td_Card(x0, y0, x0 + 930.0f, y0 + 310.0f, entry[i], sub[i], i + 1, i == sTb->tagCur, 88.0f);
        }
        return;
    }
    {
        static char *labels[8] = {"Mode", "Team 1", "Team 2", "Time Limit", "Empty Slots", "CPU Difficulty", "Create Room", "Join Room"};
        static char *modes[4] = {"2 vs 2", "5 vs 5", "Free for All", "Custom Teams"};
        s32 slot = 0;
        Td_Window(40.0f, 230.0f, 2030.0f, 940.0f, sTb->tagLocal ? "LOCAL MATCH" : "CREATE MATCH");
        for (i = 0; i < 8; i++) {
            s32 ix = 0, tot = 1;
            if (i == 6) continue; /* (Create Room: the big button) */
            if (sTb->tagPreset != 3 && (i == 1 || i == 2)) continue;
            value[0] = 0;
            if (i == 0) { Ps_Copy(value, modes[sTb->tagPreset], 48); ix = sTb->tagPreset; tot = 4; }
            if (i == 1 || i == 2) { sprintf(value, (char *)PortOnline_Text("%d slots"), i == 1 ? sTb->tagLeft : sTb->tagRight); ix = (i == 1 ? sTb->tagLeft : sTb->tagRight) - 1; tot = 5; }
            if (i == 3) { Ps_Copy(value, sTagTime[sTb->time], 48); ix = sTb->time; tot = 5; }
            if (i == 4) { Ps_Copy(value, sTb->tagCPU ? "CPU" : "Open", 48); ix = !sTb->tagCPU; tot = 2; }
            if (i == 5) { Ps_Copy(value, sTagDifficulty[sTb->tagDifficulty], 48); ix = sTb->tagDifficulty; tot = 5; }
            {
                f32 x0 = 110.0f + 965.0f * (f32)(slot % 2), y = 290.0f + 150.0f * (f32)(slot / 2);
                Td_Option(x0, y, x0 + 905.0f, y + 126.0f, labels[i], i == 7 ? NULL : value, ix, tot, i == sTb->tagCur, i + 1,
                          i == 0 ? "2 VS 2, 5 VS 5, FREE FOR ALL, CUSTOM" : NULL, 1);
            }
            slot++;
        }
        Td_BigButton(1120.0f, 755.0f, 1940.0f, 870.0f, labels[6], 80.0f, sTb->tagCur == 6, 0x80);
    }
}

static f32 sTdTagStep = 96.0f; /* the room's places, one from the next (pixels) */
static void Td_TagRoom(void) {
    static char line[128];
    s32 i, team, local = !strcmp(Port_TagCode(), "LOCAL0");

    if (sTb->tagQuick && Port_TagPhase() < 2) {
        s32 seconds = sTb->modeT / 60;
        Td_Page("Quick Match", 1, 0);
        Td_Window(40.0f, 230.0f, 900.0f, 920.0f, "QUICK MATCH");
        Td_Words(470.0f, 360.0f, Port_TagFreeForAll() ? "FREE FOR ALL" : "TAG TEAM", TD_B, 110.0f, TD_GOLD, 1, 780.0f, 0);
        Td_Words(470.0f, 520.0f, Port_TagPhase() ? "Connecting players..." : "Searching for players...", TD_C, 44.0f, TD_TEXT(240, 244, 255), 1, 780.0f, 0);
        Td_Radar(1400.0f, 580.0f, 640.0f);
        sprintf(line, "%d:%02d", seconds / 60, seconds % 60);
        Td_Chip(1180.0f, 870.0f, 1620.0f, 945.0f, line, 1, 50.0f, 0x80);
        return;
    }
    Td_Page(sTbTitle[TB_TAG_ROOM], 0, 0);
    if (sTb->tagChatTyping) {
        Td_Window(300.0f, 230.0f, 2410.0f, 940.0f, "ROOM CHAT");
        for (i = 0; i < Port_TagChatCount(); i++) {
            Td_Words(360.0f, 310.0f + 60.0f * (f32)i, (char *)Port_TagChatLine(i), TD_C, 38.0f, TD_TEXT(230, 236, 255), 0, 1980.0f, 1);
        }
        {
            f32 p[16];
            Td_Chamfer(p, 360.0f, 780.0f, 2350.0f, 880.0f, 16.0f);
            Td_Poly(p, 8, Td_Rgba(16, 50, 32, 250), Td_Rgba(6, 20, 12, 252));
            Td_Outline(p, 8, 3.0f, TD_RGB(200, 165, 85));
        }
        if (sTb->tagChatLen) {
            Td_Words(400.0f, 830.0f, sTb->tagChat, TD_C, 44.0f, TD_TEXT(240, 255, 240), 0, 1900.0f, 1);
        } else {
            Td_Words(400.0f, 830.0f, "Type a message...", TD_C, 44.0f, TD_TEXT(140, 150, 170), 0, 1900.0f, 0);
        }
        return;
    }
    for (team = 0; team < 2; team++) {
        f32 x0 = team ? 1030.0f : 40.0f;
        u32 col = team ? TD_RGB(255, 70, 60) : TD_RGB(60, 120, 255);
        Td_Window(x0, 230.0f, x0 + 950.0f, 800.0f, Port_TagFreeForAll() ? "FIGHTERS" : team ? "TEAM 2" : "TEAM 1");
        sprintf(line, Tb_Fmt(Port_TagTeamCount(team) == 1 ? "%d fighter" : "%d fighters"), Port_TagTeamCount(team));
        Td_Words(x0 + 900.0f, 236.0f, line, TD_C, 30.0f, TD_SKY(255), 2, 0.0f, 1);
        (void)col;
    }
    {
        s32 a = Port_TagTeamSlots(0), b = Port_TagTeamSlots(1), rows = a > b ? a : b;
        sTdTagStep = rows > 5 ? 480.0f / (f32)rows : 96.0f; /* (a team of more than five: its places closer together) */
    }
    for (i = 0; i < Port_TagSlots(); i++) {
        const char *name = Port_TagPlayerName(i);
        s32 t = Port_TagTeam(i), k = Port_TagTeamIndex(i), occupied = name[0] != 0, cpu = Port_TagCPU(i);
        s32 on = !sTb->tagRoomAction && i == sTb->tagRoomCur, moving = i == sTb->tagMoving;
        f32 step = sTdTagStep, h = step - 18.0f, y = 300.0f + step * (f32)k, x0 = t ? 1030.0f : 40.0f, p[8];
        u32 col = t ? TD_RGB(255, 70, 60) : TD_RGB(60, 120, 255), dim = TD_RGB(90, 90, 130);
        static char tag[8];

        Td_Skew(p, x0 + 60.0f, y, x0 + 890.0f, y + h, 14.0f);
        if (on || moving) {
            Td_Poly(p, 4, TD_RGB(255, 168, 54), TD_RGB(204, 70, 8));
            Td_Outline(p, 4, 3.0f, TD_RGB(255, 236, 196));
        } else {
            Td_Poly(p, 4, occupied && !cpu ? ((col & 0xFFFFFFu) | (0x2Du << 24)) : Td_Rgba(30, 30, 60, 220), Td_Rgba(14, 12, 40, 235));
            Td_Outline(p, 4, 3.0f, occupied && !cpu ? col : dim);
        }
        Td_Rect(x0 + 90.0f, y + h * 0.18f, x0 + 170.0f, y + h * 0.82f, occupied && !cpu ? col : dim, occupied && !cpu ? col : dim);
        sprintf(tag, "%dP", k + 1);
        Td_Words(x0 + 130.0f, y + h * 0.5f, tag, TD_B, 38.0f * (h < 60.0f ? h / 60.0f : 1.0f), Td_Ink(TD_RGB(255, 255, 255), TD_RGB(230, 230, 255), TD_NONE), 1, 0.0f, 1);
        if (cpu) {
            sprintf(line, "CPU %02d", k + 1);
        } else {
            Ps_Copy(line, name[0] ? (char *)name : (char *)PortOnline_Text("Empty"), sizeof(line));
        }
        Td_Words(x0 + 200.0f, y + h * 0.5f, line, TD_B, 56.0f * (h < 70.0f ? h / 70.0f : 1.0f),
                 on ? TD_DARK : occupied ? TD_WHITE : Td_Ink(TD_RGB(130, 130, 160), TD_RGB(100, 100, 130), TD_RGB(10, 6, 30)), 0, 500.0f, 1);
        if (cpu) {
            Td_Words(x0 + 860.0f, y + h * 0.5f, sTagDifficulty[Port_TagDifficulty(i)], TD_C, 32.0f, TD_SKY(255), 2, 220.0f, 0);
        }
    }
    if (!local) {
        Td_Window(40.0f, 840.0f, 980.0f, 940.0f, NULL);
        Td_Words(90.0f, 890.0f, "ROOM CODE", TD_C, 34.0f, TD_SKY(255), 0, 0.0f, 0);
        Td_Words(380.0f, 890.0f, (char *)Port_TagCode(), TD_B, 72.0f, TD_GOLD_FLAT, 0, 560.0f, 1);
    }
    Td_BigButton(local ? 640.0f : 1080.0f, 836.0f, local ? 1500.0f : 1940.0f, 944.0f, Port_TagHost() ? "Choose Fighters" : "Waiting for host", 70.0f,
                 sTb->tagRoomAction, Port_TagCanStart() ? 0x80 : 0x58);
}

static void Td_Block(void) {
    s32 tag = sTb->blockBack == TB_TAG || (sTb->blockBack == TB_JOIN && sTb->tagJoin);
    static char parts[96];

    Td_Back(140);
    Td_Gohan(2200.0f, 1000.0f + 4.0f * Tb_Beat(150), 700.0f);
    Td_Window(360.0f, 250.0f, 1940.0f, 900.0f, tag ? "Unable to Connect" : "Cannot Go Online");
    Td_Tri(520.0f, 340.0f, 620.0f, 520.0f, 420.0f, 520.0f, TD_RGB(110, 40, 0));
    Td_Tri(520.0f, 352.0f, 610.0f, 514.0f, 430.0f, 514.0f, TD_RGB(255, 190, 40));
    Td_Words(520.0f, 470.0f, "!", TD_B, 120.0f, TD_DARK_FLAT, 1, 0.0f, 1);
    if (tag) {
        Td_Words(680.0f, 350.0f, "Tag Team requires matching BT4 builds", TD_C, 44.0f, TD_TEXT(240, 244, 255), 0, 1200.0f, 0);
        Td_Words(680.0f, 416.0f, "and the same B14 Rev2 data on every computer.", TD_C, 44.0f, TD_TEXT(240, 244, 255), 0, 1200.0f, 0);
        Td_Words(680.0f, 482.0f, "Game data verification has not passed.", TD_C, 44.0f, TD_TEXT(255, 220, 140), 0, 1200.0f, 0);
        Td_Words(680.0f, 548.0f, "Check your game files, then try again.", TD_C, 44.0f, TD_SKY(255), 0, 1200.0f, 0);
    } else {
        const char *t = Port_DataFingerprintText(), *m = t;
        s32 n;
        while (*m != 0 && strncmp(m, "modified:", 9) != 0) m++;
        if (*m != 0) {
            Ps_Copy(parts, "Changed: ", 96);
            Ps_Cat(parts, m + 10, 9 + 81);
        } else if (Port_DataFingerprint(NULL) == 1) {
            Ps_Copy(parts, "Your data differs from BT4 B14 Rev2.", 96);
        } else {
            Ps_Copy(parts, "BT4 data verification has not passed.", 96);
        }
        for (n = 0; parts[n] != 0; n++) if (parts[n] == ',') parts[n] = ' ';
        Td_Words(680.0f, 350.0f, "Online matches are only between", TD_C, 44.0f, TD_TEXT(240, 244, 255), 0, 1200.0f, 0);
        Td_Words(680.0f, 416.0f, "two matching BT4 B14 Rev2 builds.", TD_C, 44.0f, TD_TEXT(240, 244, 255), 0, 1200.0f, 0);
        Td_Words(680.0f, 482.0f, parts, TD_C, 44.0f, TD_TEXT(255, 220, 140), 0, 1200.0f, 0);
        Td_Words(680.0f, 548.0f, "Use this BT4 build on both computers.", TD_C, 44.0f, TD_SKY(255), 0, 1200.0f, 0);
        Td_Words(680.0f, 614.0f, "(Original BT3 and other mods use different data.)", TD_C, 44.0f, TD_TEXT(170, 176, 210), 0, 1200.0f, 0);
    }
    Td_BigButton(880.0f, 740.0f, 1420.0f, 850.0f, "BACK", 74.0f, 1, 0x80);
}

/* the lobby (Versus or Tournament): its rules, its places, its code, its buttons */
static void Td_Lobby(void) {
    static char line[96];
    PortLobbyView *v = &sTbLv;
    s32 i, k, watch = 0;

    Td_Page(v->mode ? "Tournament" : sTbTitle[TB_LOBBY], 0, 0);
    if (v->state != 2) {
        if (v->state == 3) {
            Td_Gohan(TD_GX, 1000.0f + 4.0f * Tb_Beat(150), 700.0f);
        }
        return;
    }
    for (i = v->entrants; i < LB_ROWS; i++) watch += v->taken[i];
    /* the rules */
    Td_Window(40.0f, 230.0f, 1230.0f, 940.0f, v->owner ? "RULES" : "RULES (THE OWNER'S)");
    for (i = 0; i < LB_RULES; i++) {
        f32 y = 290.0f + 106.0f * (f32)i;
        s32 on = sTb->lbCol == 1 && sTb->lbRight == i && !sTb->lbChat;
        Tb_RuleValue(line, i);
        Td_Option(100.0f, y, 1180.0f, y + 92.0f, Tb_RuleName(i), line, -1, v->owner && v->phase == 0 && !Tb_RuleFixed(i) ? 2 : 1, on, i + 1, NULL, 1);
    }
    /* the places */
    if (v->mode) {
        sprintf(line, Tb_Fmt("PLAYERS  %d / %d"), (int)v->entrantCount, (int)v->entrants);
    } else {
        Ps_Copy(line, "PLAYERS", sizeof(line));
    }
    Td_Window(1280.0f, 230.0f, 2680.0f, 940.0f, line);
    for (i = 0; i < LB_ROWS; i++) {
        f32 y = 286.0f + 62.0f * (f32)i, p[8];
        s32 on = sTb->lbCol == 0 && i == sTb->lbPlace && !sTb->lbChat;
        static char tag[8];
        if (i < v->entrants) {
            sprintf(tag, v->mode ? "%d" : "%dP", (int)(i + 1));
        } else {
            sprintf(tag, "%d", (int)(i - v->entrants + 1));
        }
        Td_Skew(p, 1330.0f, y, 2080.0f, y + 54.0f, 10.0f);
        if (on) {
            Td_Poly(p, 4, TD_RGB(255, 168, 54), TD_RGB(204, 70, 8));
        } else if (i == sTb->lbPicked) {
            Td_Poly(p, 4, TD_RGB(230, 190, 100), TD_RGB(150, 104, 34));
        } else {
            Td_Poly(p, 4, v->taken[i] ? Td_Rgba(34, 30, 80, 230) : Td_Rgba(24, 20, 50, 160), Td_Rgba(14, 12, 40, 235));
        }
        Td_Words(1370.0f, y + 28.0f, tag, TD_B, 36.0f, i < v->entrants ? TD_SKY(255) : TD_TEXT(150, 150, 180), 1, 0.0f, 1);
        if (v->taken[i]) {
            Td_Words(1410.0f, y + 28.0f, v->name[i], TD_B, 44.0f, on ? TD_DARK : i == v->me ? TD_CYAN : TD_WHITE, 0, 400.0f, 1);
            if (v->isOwner[i]) {
                Td_Chip(1830.0f, y + 8.0f, 1950.0f, y + 48.0f, "OWNER", 1, 26.0f, 0x80);
            }
            if (v->ready[i] && !v->mode) {
                Td_Disc(2010.0f, y + 28.0f, 13.0f, TD_RGB(80, 255, 110), TD_RGB(80, 255, 110));
            }
        } else {
            Td_Words(1410.0f, y + 28.0f, i == sTb->lbPicked ? "" : "EMPTY", TD_B, 40.0f, TD_TEXT(110, 110, 140), 0, 0.0f, 0);
        }
        if (i == v->entrants - 1 && v->entrants < LB_ROWS) {
            sprintf(line, Tb_Fmt("SPECTATORS  %d / %d"), (int)watch, (int)(LB_ROWS - v->entrants));
            Td_Words(2084.0f, y + 66.0f, line, TD_C, 26.0f, TD_SKY(255), 2, 0.0f, 1);
        }
    }
    /* the code */
    if (v->code[0] != 0) {
        Td_Words(1340.0f, 820.0f, "LOBBY CODE", TD_C, 30.0f, TD_SKY(255), 0, 0.0f, 0);
        Td_Words(1340.0f, 880.0f, v->code, TD_B, 80.0f, TD_GOLD_FLAT, 0, 600.0f, 1);
    } else if (v->owner && v->port > 0) {
        const char *lan = Port_NetLanAddress();
        Td_Words(1340.0f, 820.0f, "PORT", TD_C, 30.0f, TD_SKY(255), 0, 0.0f, 0);
        sprintf(line, "%d", (int)v->port);
        Td_Words(1340.0f, 880.0f, line, TD_B, 70.0f, TD_GOLD_FLAT, 0, 260.0f, 1);
        Td_Words(1620.0f, 880.0f, lan[0] != 0 ? (char *)lan : "-", TD_C, 34.0f, TD_TEXT(120, 255, 150), 0, 440.0f, 1);
    } else {
        Td_Words(1340.0f, 850.0f, "JOINED BY ADDRESS", TD_B, 50.0f, TD_GREEN, 0, 700.0f, 0);
    }
    if (v->phase || v->tour != 0) {
        Td_Words(2080.0f, 820.0f, v->phase ? "A MATCH IS ON" : v->tour == 1 ? "TOURNAMENT ON" : "TOURNAMENT OVER", TD_C, 30.0f, TD_TEXT(120, 255, 150), 2, 0.0f, 0);
    }
    /* the buttons */
    for (k = 0; k < 4; k++) {
        s32 act = Tb_LobbyAct(k), on = sTb->lbCol == 1 && sTb->lbRight == LB_BTN + k && !sTb->lbChat;
        f32 y = 300.0f + 150.0f * (f32)k;
        if (act == 0) continue;
        Td_Chip(2130.0f, y, 2630.0f, y + 110.0f, Tb_LobbyActName(act), on, 46.0f, act == LB_ACT_START && !Tb_LobbyCanStart() ? 0x58 : 0x80);
    }
}

/* a lobby's windows over its page: connecting or closed, a place's menu, the yes / no questions; the chat's keypad */
static void Td_LobbyWin(void) {
    PortLobbyView *v = &sTbLv;
    static char line[256];
    s32 i;

    if (v->state == 1 || v->state == 3) {
        Td_Back(140);
        Td_Window(360.0f, 300.0f, 1940.0f, 860.0f, v->state == 1 ? (v->owner ? "Opening the Lobby" : "Joining the Lobby") : "No Longer in the Lobby");
        if (v->state == 1) {
            Td_Words(1150.0f, 520.0f, !v->owner ? "Looking for the lobby's owner..." : v->port > 0 ? "Opening the port..." : "Asking the matchmaking service for a code...",
                     TD_C, 46.0f, TD_TEXT(240, 244, 255), 1, 1460.0f, 0);
        } else {
            static char lines[3][160];
            s32 n;
            if (v->error[0] != 0) {
                Tb_LobbyLine(line, 256, v->error);
            } else {
                Tb_LobbyLine(line, 256, "The lobby is closed.");
            }
            n = Td_Wrap(line, lines, 44.0f, 1420.0f);
            for (i = 0; i < n && i < 3; i++) {
                Td_Words(1150.0f, 470.0f + 70.0f * (f32)i, lines[i], TD_C, 44.0f, TD_TEXT(240, 244, 255), 1, 1460.0f, 1);
            }
        }
        return;
    }
    if (sTb->lbPop == LB_POP_MEMBER || sTb->lbPop == LB_POP_LEAVE || sTb->lbPop == LB_POP_CALLOFF) {
        s32 count = Tb_LobbyPopCount(), ask = sTb->lbPop != LB_POP_MEMBER;
        f32 in = Tb_LobbyPopIn(), y0 = 300.0f, x0 = 460.0f, x1 = 1840.0f;
        f32 y1 = y0 + 260.0f + 120.0f * (f32)count;

        Ps_SetShift((1.0f - in) * -200.0f);
        Td_Back(150);
        if (sTb->lbPop == LB_POP_CALLOFF) {
            Td_Window(x0, y0, x1, y1, "Call Off the Tournament?");
            Td_Words(1150.0f, y0 + 140.0f, "Its bracket is lost; the lobby is Versus again.", TD_C, 46.0f, TD_TEXT(240, 244, 255), 1, 1300.0f, 0);
        } else if (sTb->lbPop == LB_POP_LEAVE) {
            Td_Window(x0, y0, x1, y1, v->owner ? "Close the Lobby?" : "Leave the Lobby?");
            Td_Words(1150.0f, y0 + 140.0f, v->owner ? "Everyone in it goes back to the menus." : "You go back to the online menu.", TD_C, 46.0f, TD_TEXT(240, 244, 255), 1,
                     1300.0f, 0);
        } else {
            Td_Window(x0, y0, x1, y1, NULL);
            Td_Words(x0 + 70.0f, y0 + 120.0f, v->name[sTb->lbPopPlace], TD_B, 80.0f, TD_CYAN, 0, 1200.0f, 1);
        }
        (void)ask;
        for (i = 0; i < count; i++) {
            f32 y = y0 + 230.0f + 120.0f * (f32)i;
            Td_Chip(x0 + 120.0f, y, x1 - 120.0f, y + 96.0f, Tb_LobbyPopItem(i), i == sTb->lbPopCur, 60.0f, 0x80);
        }
        Ps_SetShift(0.0f);
    }
    if (sTb->lbChat) {
        static char typed[TB_CHAT_MAX + 2];
        s32 from = 0;
        Td_Back(120);
        Td_Window(300.0f, 210.0f, 2410.0f, 940.0f, "SAY SOMETHING TO THE LOBBY");
        {
            f32 p[16];
            Td_Chamfer(p, 380.0f, 280.0f, 2330.0f, 380.0f, 16.0f);
            Td_Poly(p, 8, Td_Rgba(16, 50, 32, 250), Td_Rgba(6, 20, 12, 252));
            Td_Outline(p, 8, 3.0f, TD_RGB(200, 165, 85));
        }
        Ps_Copy(typed, sTb->chat, TB_CHAT_MAX + 2);
        Ps_Cat(typed, (sTb->frames / 16) & 1 ? "_" : " ", TB_CHAT_MAX + 2);
        PortOnline_Raw(1);
        while (typed[from] != 0 && Td_WordsW(typed + from, TD_C, 50.0f) > 1880.0f) from++;
        PortOnline_Raw(0);
        Td_Words(420.0f, 332.0f, typed + from, TD_C, 50.0f, TD_TEXT(240, 255, 240), 0, 0.0f, 1);
        Td_Keypad(400.0f, 405.0f, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4, 200.0f, 64.0f, 16.0f, 56.0f, -1);
    }
}

/* Draws the whole screen in its redesign; 0: not this page (the old drawing does it) */
static s32 Td_Draw(s32 live) {
    s32 mode = sTb->mode;

    if (mode == TB_BRACKET) { /* the bracket: its own layout, in a window of the redesign */
        Td_Page("Tournament", 0, 0);
        Td_Window(40.0f, 250.0f, 2680.0f, 950.0f, "BRACKET");
        Ps_SetShiftY(Td_Y(300.0f) - 70.0f);
        Tb_DrawBracket();
        Tb_DrawBracketText();
        Ps_SetShiftY(0.0f);
        Td_BottomBox(Td_Caption(), 0, live);
        return 1;
    }
    if (mode == TB_HUB) {
        Td_Hub(live);
        return 1;
    }
    Tb_PageIn(1);
    switch (mode) {
    case TB_RANKED: Td_Ranked(); break;
    case TB_CREATE: Td_Create(); break;
    case TB_JOIN: Td_Join(); break;
    case TB_ADDR: Td_Addr(); break;
    case TB_WAIT: Td_Wait(); break;
    case TB_QUICK: Td_Quick(); break;
    case TB_SPAR: Td_Spar(); break;
    case TB_TAG: Td_Tag(); break;
    case TB_TAG_ROOM: Td_TagRoom(); break;
    case TB_LOBBY: Td_Lobby(); break;
    case TB_BLOCK: Td_Block(); break;
    case TB_NAME: Td_Profile(); break;
    default: Td_Profile(); break;
    }
    Tb_PageIn(0);
    if (mode == TB_NAME) {
        Tb_PageIn(2);
        Td_Name();
        Tb_PageIn(0);
    }
    Td_BottomBox(Td_Caption(), 0, live);
    if (mode == TB_LOBBY) {
        Td_LobbyWin();
    }
    if (sTb->note > 0) {
        s32 a = sTb->note > 16 ? 0x80 : sTb->note * 8;
        Td_Words(1356.0f, 930.0f, sTb->noteLine, TD_B, 50.0f, Td_Ink(Ps_Rgba(0xFF, 0xF8, 0xB0, a), Ps_Rgba(0xFF, 0xC8, 0x30, a), Ps_Rgba(0x5C, 0x24, 0x08, a)), 1,
                 2400.0f, 0); /* (through the online screens' language: a note can be any of their lines) */
    }
    return 1;
}
