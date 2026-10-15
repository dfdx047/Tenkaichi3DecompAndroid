#include "common.h"
#include "menu/menu_a.h"
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "menu/port_screen.h"

#ifdef PORT
/*
 * PC only: the progress mode of the port's own screens (include/menu/port_screen.h, docs/port/menus.md). The game
 * has no mode to spare for a new screen, so the screens run in one that Progress_Main (menu_a_b.c) hands to
 * PortScreen_Main; gPortScreen says which. A screen reads what it needs itself: the windows of the game's menus come
 * from the versus mode's archive (gMenuArc5, as Duel_Main reads it).
 */

extern s32 PortTbo_Run(void); /* menu/port_tbo.c: 1 = left for "Train While Waiting" (its fight is set up) */

s32 gPortScreen;

/* ---------------------------------------------------------------------------------------------- how a screen draws
   The kit every screen of the port's own draws with (it began as Tenkaichi Budokai Online's, menu/port_tbo.c): the
   shapes, the pictures of tbo/ui, the lines of letters, the pad's buttons and their prompts, on a page of the game's
   512 x 448 units in the middle of the window. */
#include "menu/tbo_art.h"
#include "menu/tbo_sizes.h"

extern f32 cosf(f32 x);
extern f32 sinf(f32 x);

extern u64 *Dma_BeginDirect(void);
extern void Dma_EndDirect(u64 *end);

/* A colour for the shapes and the letters' tints: 0..255 a channel. */
u32 Ps_Rgba(s32 r, s32 g, s32 b, s32 a) {
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

/* Any four-cornered plain shape, in the screen's 512 x 448 (corners in the order of a strip: top left, top right,
   bottom left, bottom right); the radar is drawn of these. */
void Ps_ScreenQuad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3) {
    u64 *p = Dma_BeginDirect();

    p[0] = 4 | (1ULL << 15) | (1ULL << 60);
    p[1] = 0xE;
    p += 2;
    p[0] = 0x01BF000001FF0000ULL;
    p[1] = 0x40;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42;
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47;
    p += 2;
    p[0] = 0x4C;
    p[1] = 0x00;
    p += 2;
    p[0] = 4 | (1ULL << 15) | (1ULL << 58) | (2ULL << 60);
    p[1] = 0x51;
    p += 2;
    p[0] = (u64)c0 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x0 * 16.0f) + 0x7000) | ((u64)((s32)(y0 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)c1 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x1 * 16.0f) + 0x7000) | ((u64)((s32)(y1 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)c2 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x2 * 16.0f) + 0x7000) | ((u64)((s32)(y2 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)c3 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x3 * 16.0f) + 0x7000) | ((u64)((s32)(y3 * 16.0f) + 0x7200) << 16);
    p += 2;
    Dma_EndDirect(p);
}

void Ps_Copy(char *dst, const char *src, s32 max) {
    s32 i;

    for (i = 0; i < max - 1 && src[i] != 0; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

/* `src` after what `dst` (`max` bytes) holds. (No sprintf "%s" in this file: the game's code has 32-bit pointers,
   and one passed through sprintf's "..." can reach the C library with garbage in its upper half.) */
void Ps_Cat(char *dst, const char *src, s32 max) {
    s32 n = 0;

    while (n < max - 1 && dst[n] != 0) {
        n++;
    }
    Ps_Copy(dst + n, src, max - n);
}

s32 Ps_Len(const char *s) {
    s32 n = 0;

    while (s[n] != 0) {
        n++;
    }
    return n;
}

s32 Ps_Move(s32 *cur, s32 count) {
    Pad *pad = &gPad[0];
    s32 was = *cur;

    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        *cur = (*cur + 1) % count;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        *cur = (*cur + count - 1) % count;
    }
    if (*cur != was) {
        Snd_PlaySe(1, 0);
    }
    return *cur != was;
}

s32 Ps_Side(void) {
    Pad *pad = &gPad[0];

    if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        return 1;
    }
    if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        return -1;
    }
    return 0;
}

s32 Ps_Wrap(s32 v, s32 lo, s32 hi) {
    return v < lo ? hi : v > hi ? lo : v;
}

/* ---------------------------------------------------------------------------------------------- the pictures */
/* Everything on these screens is drawn from tbo/ui/*.png at the pictures' own resolution (include/menu/tbo_art.h:
   the renderer reads them, port/src/gs/gs_draw.c tbo_texture), on a 4:3 page in the middle of the window so that
   nothing is stretched on a wide one; the tournament grounds fill the whole window behind it. Positions below are
   in the game's 512 x 448 units of that page (a unit is 7:6 wide, hence the 0.857s). */
extern int Port_PageAspectMilli(void); /* port/src/plat_stub.c: the shape the menus are shown in (4:3 with "4:3 in menus", else the window's), width / height * 1000 */

static f32 sPsPx, sPsPy, sPsPw = 512.0f, sPsPh = 448.0f; /* the page on the screen, set once a frame */
static s32 sPsAlign;                                      /* 0 a line starts at its x, 1 is centred on it, 2 ends there */

void Ps_PageSet(void) {
    f32 aspect = (f32)Port_PageAspectMilli() / 1000.0f;
    f32 page = 4.0f / 3.0f;

    if (aspect >= page) { /* wider: the whole height, the grounds at the sides */
        sPsPh = 448.0f;
        sPsPw = 512.0f * page / aspect;
    } else {
        sPsPw = 512.0f;
        sPsPh = 448.0f * aspect / page;
    }
    sPsPx = (512.0f - sPsPw) * 0.5f;
    sPsPy = (448.0f - sPsPh) * 0.5f;
}

/* A page coming in (Tb_PageIn): what it draws is moved this far right (from the left, as the hub's bars come). */
static f32 sPsShift;

void Ps_SetShift(f32 x) {
    sPsShift = x;
}

f32 Ps_X(f32 x) {
    return sPsPx + (x + sPsShift) * sPsPw / 512.0f;
}


/* Where the window's left edge is, in the page's units (0 on a 4:3 window, less on a wider one). */
f32 Ps_Left(void) {
    return -sPsPx * 512.0f / sPsPw;
}

f32 Ps_Y(f32 y) {
    return sPsPy + y * sPsPh / 448.0f;
}

/* 0..1 of the picture as a UV coordinate: 1024 texels in 1/16ths, held to the register's 14 bits */
u32 Ps_Uv(f32 t) {
    s32 v = (s32)(t * 1024.0f * 16.0f);

    return v < 0 ? 0 : v > 0x3FFF ? 0x3FFF : (u32)v;
}

/* A picture's part (u, v in 0..1) over a box of the SCREEN, tinted (0x80 per channel = as it is). */
void Ps_SpriteScreen(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha) {
    u64 *p;

    if (alpha <= 0 || x1 <= x0 || y1 <= y0) {
        return;
    }
    if (alpha > 0x80) {
        alpha = 0x80;
    }
    p = Dma_BeginDirect();
    p[0] = 4 | (1ULL << 15) | (1ULL << 60); /* A+D x 4 */
    p[1] = 0xE;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42; /* ALPHA_1: blended by the source's alpha */
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14; /* TEX1_1: bilinear */
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47; /* TEST_1: no alpha test, depth always passes */
    p += 2;
    p[0] = 5;
    p[1] = 0x08; /* CLAMP_1: clamped both ways */
    p += 2;
    p[0] = 1 | (1ULL << 15) | (1ULL << 58) | (8ULL << 60);
    p[1] = 0x535306E1; /* RGBAQ, (A+D: nothing), TEX0_1, PRIM, UV, XYZ2, UV, XYZ2 */
    p += 2;
    p[0] = (u64)(tint & 0xFFFFFF) | ((u64)alpha << 24);
    p[1] = 0;
    p += 2;
    /* TEX0: base 1, width 1, format TBO_PSM, 1024 x 1024, the texture's alpha, modulated, the picture's number where
       the palette address goes */
    p[0] = 1ULL | (1ULL << 14) | ((u64)TBO_PSM << 20) | (10ULL << 26) | (10ULL << 30) | (1ULL << 34) | ((u64)(id & 0x3FFF) << 37);
    p[1] = 0x156; /* PRIM: sprite, textured, blended, UV */
    p += 2;
    p[0] = (u64)Ps_Uv(u0) | ((u64)Ps_Uv(v0) << 16);
    p[1] = (u64)((s32)(x0 * 16.0f) + 0x7000) | ((u64)((s32)(y0 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)Ps_Uv(u1) | ((u64)Ps_Uv(v1) << 16);
    p[1] = (u64)((s32)(x1 * 16.0f) + 0x7000) | ((u64)((s32)(y1 * 16.0f) + 0x7200) << 16);
    p += 2;
    Dma_EndDirect(p);
}

/* The same over a box of the page. */
void Ps_Sprite(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha) {
    Ps_SpriteScreen(id, Ps_X(x0), Ps_Y(y0), Ps_X(x1), Ps_Y(y1), u0, v0, u1, v1, tint, alpha);
}


f32 Ps_PicW(s32 id) {
    return (f32)sTboSize[id][0];
}

f32 Ps_PicH(s32 id) {
    return (f32)sTboSize[id][1];
}


/* A picture standing on `feet`, centred on cx, `height` units tall (its own shape kept). */
void Ps_Stand(s32 id, f32 cx, f32 feet, f32 height, s32 alpha) {
    f32 w = height * Ps_PicW(id) / Ps_PicH(id) * 0.857f;

    Ps_Sprite(id, cx - w * 0.5f, feet - height, cx + w * 0.5f, feet, 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, alpha);
}

/* A picture centred on (cx, cy), `height` units tall. */
void Ps_Icon(s32 id, f32 cx, f32 cy, f32 height, s32 alpha) {
    Ps_Stand(id, cx, cy + height * 0.5f, height, alpha);
}


/* A piece of the kit as a bar: its round ends keep their shape, its middle stretches. */
void Ps_Pill(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha) {
    f32 w = Ps_PicW(id);
    f32 h = Ps_PicH(id);
    f32 c = (f32)sTboSize[id][2];
    f32 cap = c * (y1 - y0) / h * 0.857f;
    f32 cu = c / w;

    if (cap * 2.0f > x1 - x0) {
        cap = (x1 - x0) * 0.5f;
    }
    Ps_Sprite(id, x0, y0, x0 + cap, y1, 0.0f, 0.0f, cu, 1.0f, PS_PLAIN, alpha);
    Ps_Sprite(id, x0 + cap, y0, x1 - cap, y1, cu, 0.0f, 1.0f - cu, 1.0f, PS_PLAIN, alpha);
    Ps_Sprite(id, x1 - cap, y0, x1, y1, 1.0f - cu, 0.0f, 1.0f, 1.0f, PS_PLAIN, alpha);
}

/* A piece of the kit as a box: its corners keep their shape, its sides and middle stretch. (kit_window's inside is
   made one teal when it is read: port/src/gs/gs_draw.c, tbo_texture.) */

void Ps_Panel(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha) {
    f32 w = Ps_PicW(id);
    f32 h = Ps_PicH(id);
    f32 c = (f32)sTboSize[id][2];
    f32 cy = c * 0.22f;
    f32 cx;
    f32 us[4];
    f32 vs[4];
    f32 xs[4];
    f32 ys[4];
    s32 i;
    s32 j;

    if (cy > (y1 - y0) * 0.34f) {
        cy = (y1 - y0) * 0.34f;
    }
    cx = cy * 0.857f;
    us[0] = 0.0f;
    us[1] = c / w;
    us[2] = 1.0f - c / w;
    us[3] = 1.0f;
    vs[0] = 0.0f;
    vs[1] = c / h;
    vs[2] = 1.0f - c / h;
    vs[3] = 1.0f;
    xs[0] = x0;
    xs[1] = x0 + cx;
    xs[2] = x1 - cx;
    xs[3] = x1;
    ys[0] = y0;
    ys[1] = y0 + cy;
    ys[2] = y1 - cy;
    ys[3] = y1;
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            Ps_Sprite(id, xs[i], ys[j], xs[i + 1], ys[j + 1], us[i], vs[j], us[i + 1], vs[j + 1], PS_PLAIN, alpha);
        }
    }
}

/* The shapes of the radar, on the page. */
void Ps_Quad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3) {
    Ps_ScreenQuad(Ps_X(x0), Ps_Y(y0), Ps_X(x1), Ps_Y(y1), Ps_X(x2), Ps_Y(y2), Ps_X(x3), Ps_Y(y3), c0, c1, c2, c3);
}

void Ps_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    Ps_Quad(x0, y0, x1, y0, x0, y1, x1, y1, top, top, bottom, bottom);
}

/* ---- the letters: lines written with the TrueType fonts of the font list (port/src/gs/tbo_text.c): the headings
   and buttons in the Menu font, the hosts' talk in the Dialogue / UI font (F1 > UI; on "Original" the first font of
   fonts.tsv for each) ---- */
extern int PortTbo_TextId(const char *text, int kind);      /* a line's number for Ps_Sprite, -1: not written */
extern int PortTbo_TextInfo(int id, int what);              /* its measures, in thousandths of its capitals */
extern int PortTbo_TextMeasure(const char *text, int kind); /* how far the pen goes, without keeping it */

void Ps_Align(s32 align) {
    sPsAlign = align;
}


f32 Ps_TextWidth(char *str, f32 scale) {
    return (f32)PortTbo_TextMeasure(str, 0) / 1000.0f * PS_CELL_UNITS * scale * PS_CAP_OF_CELL * PS_UNIT_X;
}

/* Where a line of `scale` starts (its y) for its capitals to be centred on `middle`. */
f32 Ps_TextTop(f32 middle, f32 scale) {
    f32 cell = PS_CELL_UNITS * scale;

    return middle - cell * (PS_BASE_OF_CELL - PS_CAP_OF_CELL * 0.5f);
}

/* Colour of Ps_Rgba (0..255 per channel) as a tint of the white letters (0x80 = as they are). */
u32 Ps_Tint(u32 c) {
    return ((c & 0xFF) >> 1) | ((((c >> 8) & 0xFF) >> 1) << 8) | ((((c >> 16) & 0xFF) >> 1) << 16);
}

u32 Ps_Mix(u32 a, u32 b, f32 t) {
    s32 k;
    u32 out = 0;

    for (k = 0; k < 24; k += 8) {
        f32 ca = (f32)((a >> k) & 0xFF);
        f32 cb = (f32)((b >> k) & 0xFF);

        out |= (u32)(ca + (cb - ca) * t) << k;
    }
    return out;
}

/* A line's picture (the line filled above, outlined below) over the page: the pen starting at x, the baseline at
   `base`, capitals `cap` units high, squeezed across by `fit`. bands 0: the outline in `line`; else the fill from
   `top` to `bottom` in that many bands. */
void Ps_Line(s32 id, f32 x, f32 base, f32 cap, f32 fit, u32 top, u32 bottom, u32 line, s32 bands, s32 alpha) {
    f32 ky = cap / 1000.0f;
    f32 kx = ky * fit * PS_UNIT_X;
    f32 x0 = x - (f32)PortTbo_TextInfo(id, 4) * kx;
    f32 x1 = x0 + (f32)PortTbo_TextInfo(id, 1) * kx;
    f32 y0 = base - (f32)PortTbo_TextInfo(id, 3) * ky;
    f32 h = (f32)PortTbo_TextInfo(id, 2) * ky;
    s32 band;

    if (bands == 0) {
        Ps_Sprite(id, x0, y0, x1, y0 + h, 0.0f, 0.5f, 1.0f, 1.0f, Ps_Tint(line), alpha);
        return;
    }
    for (band = 0; band < bands; band++) {
        f32 t0 = (f32)band / (f32)bands;
        f32 t1 = (f32)(band + 1) / (f32)bands;

        Ps_Sprite(id, x0, y0 + h * t0, x1, y0 + h * t1, 0.0f, t0 * 0.5f, 1.0f, t1 * 0.5f,
                  Ps_Tint(Ps_Mix(top, bottom, ((f32)band + 0.5f) / (f32)bands)), alpha);
    }
}

/* A line of letters: filled from `top` to `bottom` (a gradient), outlined in `line`; squeezed to `width` if it is
   wider (0: no limit). The alpha is the top colour's. */
void Ps_TextEx(f32 x, f32 y, char *str, f32 scale, f32 width, u32 top, u32 bottom, u32 line) {
    f32 cell = PS_CELL_UNITS * scale;
    f32 cap = cell * PS_CAP_OF_CELL;
    f32 fit = 1.0f;
    f32 w;
    s32 alpha = (s32)((top >> 24) & 0xFF);
    s32 id;

    if (str[0] == 0 || (id = PortTbo_TextId(str, 0)) < 0) {
        return;
    }
    w = (f32)PortTbo_TextInfo(id, 0) / 1000.0f * cap * PS_UNIT_X;
    if (width > 0.0f && w > width) {
        fit = width / w;
        if (fit < 0.6f) {
            fit = 0.6f;
        }
        w *= fit;
    }
    if (sPsAlign == 1) {
        x -= w * 0.5f;
    } else if (sPsAlign == 2) {
        x -= w;
    }
    Ps_Line(id, x, y + cell * PS_BASE_OF_CELL, cap, fit, top, bottom, line, 0, alpha); /* the outline under the fill */
    Ps_Line(id, x, y + cell * PS_BASE_OF_CELL, cap, fit, top, bottom, line, 3, alpha);
}

/* A heading in the main menu's heading style (gold to red, a brown bevelled outline, a light rim: one picture in
   its own colours), its capitals as Ps_TextEx's of that scale; squeezed to `width` if it is wider (0: no limit). */
void Ps_Heading(f32 x, f32 y, char *str, f32 scale, f32 width, s32 alpha) {
    f32 cell = PS_CELL_UNITS * scale;
    f32 cap = cell * PS_CAP_OF_CELL;
    f32 ky = cap / 1000.0f;
    f32 kx;
    f32 fit = 1.0f;
    f32 w;
    f32 x0;
    f32 y0;
    s32 id;

    if (str[0] == 0 || (id = PortTbo_TextId(str, 2)) < 0) {
        return;
    }
    w = (f32)PortTbo_TextInfo(id, 0) / 1000.0f * cap * PS_UNIT_X;
    if (width > 0.0f && w > width) {
        fit = width / w;
        if (fit < 0.6f) {
            fit = 0.6f;
        }
        w *= fit;
    }
    if (sPsAlign == 1) {
        x -= w * 0.5f;
    } else if (sPsAlign == 2) {
        x -= w;
    }
    kx = ky * fit * PS_UNIT_X;
    x0 = x - (f32)PortTbo_TextInfo(id, 4) * kx;
    y0 = y + cell * PS_BASE_OF_CELL - (f32)PortTbo_TextInfo(id, 3) * ky;
    Ps_Sprite(id, x0, y0, x0 + (f32)PortTbo_TextInfo(id, 1) * kx, y0 + (f32)PortTbo_TextInfo(id, 2) * ky, 0.0f, 0.0f, 1.0f, 1.0f,
              Ps_Tint(0xFFFFFF), alpha);
}

/* ---- the talk box's letters: capitals 11.5 units high, the baseline 18.25 down (about the size of the main
   menu's own), white with a thin dark edge ---- */

f32 Ps_MsgWidth(char *str, f32 scale) {
    return (f32)PortTbo_TextMeasure(str, 1) / 1000.0f * PS_MSG_CAP * scale * PS_UNIT_X;
}

void Ps_Msg(f32 x, f32 y, char *str, f32 scale, s32 alpha) {
    s32 id;

    if (str[0] == 0 || (id = PortTbo_TextId(str, 1)) < 0) {
        return;
    }
    Ps_Line(id, x, y + PS_MSG_BASE * scale, PS_MSG_CAP * scale, 1.0f, 0, 0, Ps_Rgba(16, 20, 40, 255), 0, alpha);
    Ps_Line(id, x, y + PS_MSG_BASE * scale, PS_MSG_CAP * scale, 1.0f, Ps_Rgba(255, 255, 255, 255),
            Ps_Rgba(255, 255, 255, 255), 0, 1, alpha);
}

/* ---- the pad's buttons ---- */
/* kind: 0 cross, 1 square, 2 triangle, 3 left / right, 4 circle, 7 START */
s32 Ps_ButtonPic(s32 kind) {
    switch (kind) {
    case 1:
        return TBO_BTN_SQUARE;
    case 2:
        return TBO_BTN_TRIANGLE;
    case 3:
        return TBO_BTN_LEFTRIGHT;
    case 4:
        return TBO_BTN_CIRCLE;
    case 7:
        return TBO_BTN_START;
    }
    return TBO_BTN_CROSS;
}

f32 Ps_ButtonW(s32 kind, f32 size) {
    s32 id = Ps_ButtonPic(kind);

    return size * Ps_PicW(id) / Ps_PicH(id) * 0.857f;
}

void Ps_Button(s32 kind, f32 cx, f32 cy, f32 size) {
    Ps_Icon(Ps_ButtonPic(kind), cx, cy, size, 0x80);
}

/* The buttons the screen that is up answers to, at the foot of the hosts' box (bottom right, as in the mockups). */
#define PS_PROMPTS 6
static s32 sPsPromptKind[PS_PROMPTS];
static char *sPsPromptLabel[PS_PROMPTS];
static s32 sPsPromptCount;

void Ps_PromptsClear(void) {
    sPsPromptCount = 0;
}

void Ps_Prompt(s32 kind, char *label) {
    if (sPsPromptCount < PS_PROMPTS) {
        sPsPromptKind[sPsPromptCount] = kind;
        sPsPromptLabel[sPsPromptCount++] = label;
    }
}

f32 Ps_PromptX(s32 k) { /* where prompt k starts: laid from the right edge leftwards */
    f32 x = 470.0f;
    s32 i;

    for (i = sPsPromptCount - 1; i >= k; i--) {
        x -= Ps_MsgWidth(sPsPromptLabel[i], 1.0f) + Ps_ButtonW(sPsPromptKind[i], 19.0f) + 16.0f;
    }
    return x;
}

void Ps_PromptsDraw(void) {
    s32 k;

    for (k = 0; k < sPsPromptCount; k++) {
        f32 bw = Ps_ButtonW(sPsPromptKind[k], 19.0f);
        f32 x = Ps_PromptX(k);

        Ps_Button(sPsPromptKind[k], x + bw * 0.5f, 425.0f, 19.0f);
        Ps_Msg(x + bw + 5.0f, 425.0f - (PS_MSG_BASE - PS_MSG_CAP * 0.5f), sPsPromptLabel[k], 1.0f, 0x80); /* its capitals beside the button's middle */
    }
}

void Ps_Text(f32 x, f32 y, char *s, f32 scale, f32 width, s32 style) {
    switch (style) {
    case 1: /* gold: under the cursor */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xFF, 0xEC, 0xB8, 0x80), Ps_Rgba(0xFF, 0x94, 0x30, 0x80), Ps_Rgba(0x5C, 0x24, 0x08, 0x80));
        break;
    case 2: /* a heading: as the lines (gold is the cursor's alone, as in the game's menus) */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80), Ps_Rgba(0xE4, 0xD8, 0xFF, 0x80), Ps_Rgba(0x40, 0x14, 0x64, 0x80));
        break;
    case 3: /* cyan: a value */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xD0, 0xFC, 0xFF, 0x80), Ps_Rgba(0x70, 0xE0, 0xFF, 0x80), Ps_Rgba(0x08, 0x28, 0x50, 0x80));
        break;
    case 4: /* grey: not open yet */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xC8, 0xC8, 0xD4, 0x80), Ps_Rgba(0x98, 0x98, 0xAC, 0x80), Ps_Rgba(0x30, 0x30, 0x44, 0x80));
        break;
    case 5: /* green: online */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xD8, 0xFF, 0xC0, 0x80), Ps_Rgba(0x60, 0xE8, 0x50, 0x80), Ps_Rgba(0x08, 0x40, 0x10, 0x80));
        break;
    default:
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80), Ps_Rgba(0xE4, 0xD8, 0xFF, 0x80), Ps_Rgba(0x40, 0x14, 0x64, 0x80));
        break;
    }
}

/* A filled circle, as a fan of quads. */
void Ps_Disc(f32 cx, f32 cy, f32 r, u32 mid, u32 edge) {
    s32 k;
    f32 rx = r * 0.857f; /* (a round thing is this much narrower in the screen's pixels) */

    for (k = 0; k < 32; k++) {
        f32 a0 = (f32)k * 6.2831853f / 32.0f;
        f32 a1 = (f32)(k + 1) * 6.2831853f / 32.0f;

        Ps_Quad(cx, cy, cx, cy, cx + cosf(a0) * rx, cy + sinf(a0) * r, cx + cosf(a1) * rx, cy + sinf(a1) * r, mid, mid, edge, edge);
    }
}



s32 PortScreen_Main(void) {
    s32 screen = gPortScreen;
    s32 result = 0;

    gPortScreen = PORT_SCREEN_NONE;
    if (gMenuArc5 == NULL) {
        gMenuArc5 = File_LoadSync(gProgress->baseFile, NULL, 0);
    }
    switch (screen) {
    case PORT_SCREEN_TBO:
        Bgm_Play(0x10B16 + 2);
        result = PortTbo_Run();
        Adx_StopAll();
        gProgress->mode = result ? 45 : 4; /* the Ultimate Training's fight, or back to the main menu */
        break;
    default:
        gProgress->mode = 4;
        break;
    }
    return result;
}
#endif
