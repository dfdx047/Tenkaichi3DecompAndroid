#include "common.h"
#include "menu/menu_e.h"
#include "sys/pad.h"
#ifdef PORT
#include "menu/port_modui.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "menu/mm_sizes.h"
#include "menu/zw_sizes.h"
#include "battle/col_c.h"

/*
 * PC only: how the Mod Manager looks (the approved 16:9 mockups, modmgr_design/mockups). Everything is placed in the
 * mockups' own pixels (1376 x 768), which are fitted into the window (a wider or narrower window gets the backdrop
 * at the sides or above and below); the pictures are tbo/ui/mm_*.png, cut from the Flow art by
 * port/build/ml/mm_art.py, drawn at their own resolution as Tenkaichi Budokai Online's are (TBO_PSM); the letters
 * are lines of the TrueType fonts of the font list (port/src/gs/tbo_text.c), the game's own textures (faces,
 * portraits) are drawn with Mm_Tex.
 */

extern int Port_PageAspectMilli(void);
extern int PortTbo_TextId(const char *text, int kind);
extern int PortTbo_TextInfo(int id, int what);
extern int PortTbo_TextMeasure(const char *text, int kind);
extern void Ps_SpriteScreen(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha);
extern u32 Ps_Tint(u32 c);
extern u32 Ps_Mix(u32 a, u32 b, f32 t);
extern f32 sinf(f32 x);
extern f32 sqrtf(f32 x);

/* ---------------------------------------------------------------------------------------------- the frame
 * Responsive: the mockup's positions are spread over the whole picture, whatever its shape (a squarer one spaces the
 * screen out downwards, a wider one sideways), while everything that has a shape of its own - letters, pictures,
 * faces, round ends and corners, the model - keeps it, at the size the picture's width (or height, if it is wider
 * than 16:9) gives it. sPx / sPy: screen units per mockup pixel for a place; sUx / sUy: for a size. */
static f32 sPx = 512.0f / MX_W, sPy = 448.0f / MX_H, sUx = 512.0f / MX_W, sUy = 448.0f / MX_H, sQ = 0.643f;

void Mx_Begin(void) {
    f32 aspect = (f32)Port_PageAspectMilli() / 1000.0f;

    if (aspect <= 0.1f) {
        aspect = MX_W / MX_H;
    }
    sPx = 512.0f / MX_W;
    sPy = 448.0f / MX_H;
    if (aspect <= MX_W / MX_H) { /* as wide as 16:9 or narrower: sizes by the width */
        sUx = sPx;
        sUy = 448.0f * aspect / MX_W;
    } else {
        sUy = sPy;
        sUx = 512.0f / (MX_H * aspect);
    }
    sQ = (512.0f / 448.0f) / aspect; /* screen units across that are as long as one down */
}

f32 Mx_X(f32 x) {
    return x * sPx;
}

f32 Mx_Y(f32 y) {
    return y * sPy;
}

/* How far across / down (in the mockup's places) a size of `l` reaches. */
f32 Mx_LenX(f32 l) {
    return l * sUx / sPx;
}

f32 Mx_LenY(f32 l) {
    return l * sUy / sPy;
}

/* How far across (in places) is as long as `dy` down (in places): for pictures that keep their shape in a box. */
f32 Mx_SameX(f32 dy) {
    return dy * sPy * sQ / sPx;
}

/* Where a point of the screen (0..1 of its width / height) is in the mockup's pixels (for the 3D model). */
f32 Mx_ScreenFracX(f32 x) {
    return Mx_X(x) / 512.0f;
}

f32 Mx_ScreenFracY(f32 y) {
    return Mx_Y(y) / 448.0f;
}

/* How much of the screen's height a size of `l` is. */
f32 Mx_SizeFracY(f32 l) {
    return l * sUy / 448.0f;
}

u32 Mx_Rgba(s32 r, s32 g, s32 b, s32 a) {
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

/* ---------------------------------------------------------------------------------------------- the pictures */
static const u16 *Pic_Size(s32 id) { /* (the Mod Manager's pictures, the Raging Blast style select's, Z Warriors') */
    if (id >= MM_BG && id <= MM_BG_DUSK) {
        return sMmArtSize[id - MM_BG];
    }
    if (id >= ZW_BG && id <= ZW_SHIN) {
        return sZwArtSize[id - ZW_BG];
    }
    return NULL;
}

static f32 Pic_W(s32 id) {
    return Pic_Size(id) != NULL ? (f32)Pic_Size(id)[0] : 1.0f;
}

static f32 Pic_H(s32 id) {
    return Pic_Size(id) != NULL ? (f32)Pic_Size(id)[1] : 1.0f;
}

static f32 Pic_C(s32 id) {
    return Pic_Size(id) != NULL ? (f32)Pic_Size(id)[2] : 0.0f;
}

/* What was drawn that a finger can press (menu/port_zw.c Zw_Touch): bars, pills, rings, keys; cleared each frame by
   the screen. kind: 1 plain, 2 the cursor's, 3 a lit pill. */
MxTapRect gMxTap[MX_TAPS];
s32 gMxTapCount;

static void Mx_TapNote(s32 id, f32 x0, f32 y0, f32 x1, f32 y1) {
    s32 kind = 0;

    if (id == ZW_BAR_SEL || id == ZW_RING_SEL || id == ZW_KEY_SEL || id == MM_BAR_SEL || id == MM_KEYCAP_SEL) {
        kind = 2;
    } else if (id == ZW_PILL) {
        kind = 3;
    } else if (id == ZW_BAR || id == ZW_PILL_STEEL || id == ZW_RING || id == ZW_KEY || id == MM_BAR || id == MM_CELL || id == MM_KEYCAP) {
        kind = 1;
    }
    if (kind != 0 && gMxTapCount < MX_TAPS && x1 - x0 > 8.0f && y1 - y0 > 8.0f) {
        MxTapRect *r = &gMxTap[gMxTapCount++];

        r->x0 = x0;
        r->y0 = y0;
        r->x1 = x1;
        r->y1 = y1;
        r->kind = kind;
    }
}

void Mx_PicEx(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha) {
    Ps_SpriteScreen(id, Mx_X(x0), Mx_Y(y0), Mx_X(x1), Mx_Y(y1), u0, v0, u1, v1, tint, alpha);
}

void Mx_Pic(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha) {
    Mx_PicEx(id, x0, y0, x1, y1, 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, alpha);
}

/* A picture centred on (cx, cy), `h` pixels high, its own shape kept. */
void Mx_Fit(s32 id, f32 cx, f32 cy, f32 h, s32 alpha) {
    Mx_TapNote(id, cx - Mx_LenX(h * Pic_W(id) / Pic_H(id)) * 0.5f, cy - Mx_LenY(h) * 0.5f, cx + Mx_LenX(h * Pic_W(id) / Pic_H(id)) * 0.5f, cy + Mx_LenY(h) * 0.5f);
    f32 w = Mx_LenX(h * Pic_W(id) / Pic_H(id));

    h = Mx_LenY(h);
    Mx_Pic(id, cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f, alpha);
}

/* A bar: its round ends keep their shape (as high as the bar), its middle stretches. */
void Mx_BarTint(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, u32 tint, s32 alpha) {
    Mx_TapNote(id, x0, y0, x1, y1);
    f32 c = Pic_C(id);
    f32 cu = c / Pic_W(id);
    f32 cap = Mx_SameX(y1 - y0) * c / Pic_H(id);

    if (cap * 2.0f > x1 - x0) {
        cap = (x1 - x0) * 0.5f;
    }
    Mx_PicEx(id, x0, y0, x0 + cap, y1, 0.0f, 0.0f, cu, 1.0f, tint, alpha);
    Mx_PicEx(id, x0 + cap, y0, x1 - cap, y1, cu, 0.0f, 1.0f - cu, 1.0f, tint, alpha);
    Mx_PicEx(id, x1 - cap, y0, x1, y1, 1.0f - cu, 0.0f, 1.0f, 1.0f, tint, alpha);
}

void Mx_Bar(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha) {
    Mx_BarTint(id, x0, y0, x1, y1, PS_PLAIN, alpha);
}

/* A box: the picture's corners (as large as `r` pixels here) keep their shape, its sides and middle stretch. */
void Mx_BoxTint(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 r, u32 tint, s32 alpha) {
    Mx_TapNote(id, x0, y0, x1, y1);
    f32 w = Pic_W(id);
    f32 h = Pic_H(id);
    f32 c = Pic_C(id);
    f32 rx;
    f32 ry;
    f32 us[4];
    f32 vs[4];
    f32 xs[4];
    f32 ys[4];
    s32 i;
    s32 j;

    if (c <= 0.0f) {
        c = (w < h ? w : h) * 0.25f;
    }
    if (c > h * 0.5f - 1.0f) {
        c = h * 0.5f - 1.0f;
    }
    if (c > w * 0.5f - 1.0f) {
        c = w * 0.5f - 1.0f;
    }
    rx = Mx_LenX(r);
    ry = Mx_LenY(r);
    if (rx * 2.0f > x1 - x0) {
        rx = (x1 - x0) * 0.5f;
    }
    if (ry * 2.0f > y1 - y0) {
        ry = (y1 - y0) * 0.5f;
    }
    us[0] = 0.0f;
    us[1] = c / w;
    us[2] = 1.0f - c / w;
    us[3] = 1.0f;
    vs[0] = 0.0f;
    vs[1] = c / h;
    vs[2] = 1.0f - c / h;
    vs[3] = 1.0f;
    xs[0] = x0;
    xs[1] = x0 + rx;
    xs[2] = x1 - rx;
    xs[3] = x1;
    ys[0] = y0;
    ys[1] = y0 + ry;
    ys[2] = y1 - ry;
    ys[3] = y1;
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            Mx_PicEx(id, xs[i], ys[j], xs[i + 1], ys[j + 1], us[i], vs[j], us[i + 1], vs[j + 1], tint, alpha);
        }
    }
}

void Mx_Box(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 r, s32 alpha) {
    Mx_BoxTint(id, x0, y0, x1, y1, r, PS_PLAIN, alpha);
}

/* A plain shape: a box filled from `top` to `bottom` (Mx_Rgba, alpha 0x80 solid). */
void Mx_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    Ps_ScreenQuad(Mx_X(x0), Mx_Y(y0), Mx_X(x1), Mx_Y(y0), Mx_X(x0), Mx_Y(y1), Mx_X(x1), Mx_Y(y1), top, top, bottom, bottom);
}

/* The frame of a box, `t` pixels thick. */
void Mx_Frame(f32 x0, f32 y0, f32 x1, f32 y1, f32 t, u32 c) {
    Mx_Rect(x0, y0, x1, y0 + t, c, c);
    Mx_Rect(x0, y1 - t, x1, y1, c, c);
    Mx_Rect(x0, y0 + t, x0 + t, y1 - t, c, c);
    Mx_Rect(x1 - t, y0 + t, x1, y1 - t, c, c);
}

/* A part (u, v in texels) of one of the game's textures over a box of the mockup. */
void Mx_Tex(MmTex *t, f32 x0, f32 y0, f32 x1, f32 y1, s32 u0, s32 v0, s32 u1, s32 v1, s32 shade, s32 alpha) {
    Mm_Tex(t, Mx_X(x0), Mx_Y(y0), Mx_X(x1), Mx_Y(y1), u0, v0, u1, v1, shade, alpha);
}

/* ---------------------------------------------------------------------------------------------- the letters
 * A line is `cap` pixels high in its capitals, its capitals centred on cy; filled from one colour to another, an
 * outline round it and a soft shadow under it, as the mockups letter everything. */
#define MXC(r, g, b) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16))
static const u32 sMxStyle[MX_STYLES][3] = {
    {MXC(0xFF, 0xFF, 0xFF), MXC(0xE4, 0xE0, 0xFF), MXC(0x18, 0x1A, 0x44)}, /* MX_WHITE */
    {MXC(0xFF, 0xF2, 0xA0), MXC(0xFF, 0xB4, 0x20), MXC(0x4A, 0x22, 0x08)}, /* MX_GOLD */
    {MXC(0xFF, 0xF2, 0xFF), MXC(0xF0, 0xD0, 0xF4), MXC(0x40, 0x14, 0x52)}, /* MX_LAV: the names of the lists */
    {MXC(0xC8, 0xFC, 0xFF), MXC(0x58, 0xD8, 0xFF), MXC(0x06, 0x28, 0x4C)}, /* MX_CYAN */
    {MXC(0xC8, 0xC8, 0xD4), MXC(0x98, 0x98, 0xAC), MXC(0x30, 0x30, 0x4A)}, /* MX_DIM */
    {MXC(0xFF, 0xC0, 0xB0), MXC(0xFF, 0x5A, 0x40), MXC(0x50, 0x08, 0x08)}, /* MX_RED */
    {MXC(0x2A, 0x2A, 0x5C), MXC(0x1A, 0x1A, 0x40), MXC(0xF0, 0xF0, 0xFF)}, /* MX_DARK: on the light backdrop */
    {MXC(0xB0, 0xD8, 0xFF), MXC(0x70, 0xA8, 0xF0), MXC(0x0C, 0x18, 0x40)}, /* MX_BLUE: labels */
    {MXC(0xFF, 0xF4, 0xD0), MXC(0xFF, 0xC0, 0x60), MXC(0x5C, 0x24, 0x08)}, /* MX_ORANGE */
    {MXC(0xD8, 0xFF, 0xC0), MXC(0x60, 0xE8, 0x50), MXC(0x08, 0x40, 0x0F)}, /* MX_GREEN */
};

static u32 Mx_StyleColour(s32 style, s32 which) {
    if (style < 0 || style >= MX_STYLES) {
        style = 0;
    }
    return sMxStyle[style][which] | 0x80000000u;
}

/* The letters: lines (tbo_text.c kind 3) and headings (kind 4) in the lettering of the game's menus, Tahoma Bold. */
#define MX_KIND_LINE 3
#define MX_KIND_HEADING 4
#define MX_TEXT_K 0.92f /* (the font's letters run larger than the mockups' for the same capitals) */
#define MX_SHADOW 1 /* (a line's soft shadow: on) */

/* One half of a line's picture: the outline (bands 0) or the fill in bands of the gradient. */
static void Mx_LinePart(s32 id, f32 x, f32 base, f32 cap, f32 fit, u32 top, u32 bottom, u32 line, s32 bands, s32 alpha) {
    f32 k = Mx_LenY(cap) / 1000.0f;
    f32 kx = Mx_LenX(cap) / 1000.0f * fit;
    f32 x0 = x - (f32)PortTbo_TextInfo(id, 4) * kx;
    f32 x1 = x0 + (f32)PortTbo_TextInfo(id, 1) * kx;
    f32 y0 = base - (f32)PortTbo_TextInfo(id, 3) * k;
    f32 h = (f32)PortTbo_TextInfo(id, 2) * k;
    s32 band;

    if (bands == 0) {
        Mx_PicEx(id, x0, y0, x1, y0 + h, 0.0f, 0.5f, 1.0f, 1.0f, Ps_Tint(line), alpha);
        return;
    }
    for (band = 0; band < bands; band++) {
        f32 t0 = (f32)band / (f32)bands;
        f32 t1 = (f32)(band + 1) / (f32)bands;

        Mx_PicEx(id, x0, y0 + h * t0, x1, y0 + h * t1, 0.0f, t0 * 0.5f, 1.0f, t1 * 0.5f, Ps_Tint(Ps_Mix(top, bottom, ((f32)band + 0.5f) / (f32)bands)), alpha);
    }
}

f32 Mx_TextW(char *s, f32 cap) {
    if (s == NULL || s[0] == 0) {
        return 0.0f;
    }
    return (f32)PortTbo_TextMeasure(s, MX_KIND_LINE) / 1000.0f * Mx_LenX(cap * MX_TEXT_K);
}

/* A line in colours of its own. width > 0: squeezed to it when longer (down to 60%, then it runs over). align: 0
   from x, 1 centred on x, 2 ending at x. alpha 0x80 solid. */
void Mx_TextC(f32 x, f32 cy, char *s, f32 cap, f32 width, u32 top, u32 bottom, u32 line, s32 align, s32 alpha) {
    f32 fit = 1.0f;
    f32 w;
    f32 base = cy + cap * 0.5f;
    s32 id;

    if (s == NULL || s[0] == 0 || alpha <= 0 || (id = PortTbo_TextId(s, MX_KIND_LINE)) < 0) {
        return;
    }
    cap *= MX_TEXT_K;
    w = (f32)PortTbo_TextInfo(id, 0) / 1000.0f * Mx_LenX(cap);
    if (width > 0.0f && w > width) { /* narrower, down to 70%; then smaller too */
        fit = width / w;
        if (fit < 0.7f) {
            cap *= fit / 0.7f;
            fit = 0.7f;
        }
        w = width;
    }
    if (align == 1) {
        x -= w * 0.5f;
    } else if (align == 2) {
        x -= w;
    }
    base = cy + Mx_LenY(cap) * 0.5f;
#if MX_SHADOW
    Mx_LinePart(id, x + Mx_LenX(cap * 0.05f), base + Mx_LenY(cap * 0.08f), cap, fit, 0, 0, 0x000000, 0, alpha / 3);
#endif
    Mx_LinePart(id, x, base, cap, fit, top, bottom, line, 0, alpha);
    Mx_LinePart(id, x, base, cap, fit, top, bottom, line, 3, alpha);
}

void Mx_Text(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 style, s32 align) {
    Mx_TextC(x, cy, s, cap, width, Mx_StyleColour(style, 0), Mx_StyleColour(style, 1), Mx_StyleColour(style, 2), align, 0x80);
}

void Mx_TextA(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 style, s32 align, s32 alpha) {
    Mx_TextC(x, cy, s, cap, width, Mx_StyleColour(style, 0), Mx_StyleColour(style, 1), Mx_StyleColour(style, 2), align, alpha);
}

/* A heading in the game's heading style (gold to red, a dark bevelled outline): one picture in its own colours. */
static f32 sMxHeadingSx = 1.0f; /* how wide the heading is drawn, and how solid (Mx_Chrome: the title's entrance) */
static s32 sMxHeadingA = 0x80;

void Mx_Heading(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 align) {
    f32 k = cap / 1000.0f;
    f32 kx;
    f32 fit = 1.0f;
    f32 w;
    f32 x0;
    f32 y0;
    s32 id;

    if (s == NULL || s[0] == 0 || (id = PortTbo_TextId(s, MX_KIND_HEADING)) < 0) {
        return;
    }
    w = (f32)PortTbo_TextInfo(id, 0) / 1000.0f * Mx_LenX(cap);
    if (width > 0.0f && w > width) { /* narrower, down to 70%; then smaller too */
        fit = width / w;
        if (fit < 0.7f) {
            cap *= fit / 0.7f;
            fit = 0.7f;
        }
        w = width;
    }
    if (align == 1) {
        x -= w * 0.5f;
    } else if (align == 2) {
        x -= w;
    }
    k = Mx_LenY(cap) / 1000.0f;
    kx = Mx_LenX(cap) / 1000.0f * fit * sMxHeadingSx;
    x0 = x - (f32)PortTbo_TextInfo(id, 4) * kx;
    y0 = cy + Mx_LenY(cap) * 0.5f - (f32)PortTbo_TextInfo(id, 3) * k;
    Mx_PicEx(id, x0, y0, x0 + (f32)PortTbo_TextInfo(id, 1) * kx, y0 + (f32)PortTbo_TextInfo(id, 2) * k, 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, sMxHeadingA);
}

/* ---------------------------------------------------------------------------------------------- the sentences
 * What is said in sentences (what a screen is for, what to do, what is wrong) is in the game's own font, the one of
 * its message box (battle/col_c.c: Font_*), as the game's screens write theirs; names, buttons and headings are in
 * the lettering of its menus (Mx_Text). The font is drawn at once (each line flushed), so it lies in the order of
 * the rest. */
extern void Font_PrintAsciiAt(s32 x, s32 y, char *str);
extern s32 Font_GetWidthAscii(char *str);
extern s32 Font_GetGlyphHeight(void);
extern void Font_FlushAll(void);
extern FontStyle gFontStyle;

#define MX_SAY_K 1.45f /* the font's line is this many capitals high */
#define MX_SAY_GAME 0  /* 0: sentences are in the menus' lettering too, until their font is settled */

static s32 sMxWrapSay; /* Mx_Wrap measures in the game's font */

/* The font's scale (down; across it keeps its shape) for capitals `cap` high. */
static f32 Mx_SayScale(f32 cap) {
    s32 lh = Font_GetGlyphHeight();

    return lh > 0 ? cap * sUy * MX_SAY_K / (f32)lh : 1.0f;
}

f32 Mx_SayW(char *s, f32 cap) {
    FontStyle saved = gFontStyle;
    f32 w;

    if (!MX_SAY_GAME) {
        return Mx_TextW(s, cap);
    }
    if (s == NULL || s[0] == 0) {
        return 0.0f;
    }
    gFontStyle.scale = 1.0f;
    gFontStyle.scaleX = 1.0f;
    gFontStyle.scaleY = 1.0f;
    gFontStyle.spacingX = 0;
    w = (f32)Font_GetWidthAscii(s);
    gFontStyle = saved;
    return w * Mx_SayScale(cap) * (sUx / sUy) / sPx;
}

/* A sentence in one colour (`rgb`, as Mx_Rgba makes it; its alpha is not used), on the game's soft shadow. width > 0:
   squeezed to it when longer. align: 0 from x, 1 centred on x, 2 ending at x. alpha 0x80 solid. */
void Mx_SayRgb(f32 x, f32 cy, char *s, f32 cap, f32 width, u32 rgb, s32 align, s32 alpha) {
    FontStyle saved;
    f32 sy;
    f32 sx;
    f32 w;

    if (s == NULL || s[0] == 0 || alpha <= 0) {
        return;
    }
    sy = Mx_SayScale(cap);
    sx = sy * (sUx / sUy);
    w = Mx_SayW(s, cap);
    if (width > 0.0f && w > width) {
        sx *= width / w;
        w = width;
    }
    if (align == 1) {
        x -= w * 0.5f;
    } else if (align == 2) {
        x -= w;
    }
    saved = gFontStyle;
    gFontStyle.scale = 1.0f;
    gFontStyle.scaleX = sx;
    gFontStyle.scaleY = sy;
    gFontStyle.spacingX = 0;
    gFontStyle.align = FONT_ALIGN_LEFT;
    gFontStyle.flags = 0;
    gFontStyle.color = (rgb & 0xFFFFFFu) | ((u32)alpha << 24);
    gFontStyle.shadowMode = FONT_SHADOW_DROP;
    gFontStyle.shadowColor = (u32)(alpha * 5 / 8) << 24;
    gFontStyle.shadowDx = 1;
    gFontStyle.shadowDy = 1;
    Font_PrintAsciiAt((s32)(Mx_X(x) + 0.5f), (s32)(Mx_Y(cy) - (f32)Font_GetGlyphHeight() * sy * 0.5f + 0.5f), s);
    Font_FlushAll();
    gFontStyle = saved;
}

/* As Mx_Text / Mx_TextC, in the game's font (in the line's top colour). */
void Mx_Say(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 style, s32 align) {
    if (!MX_SAY_GAME) {
        Mx_Text(x, cy, s, cap, width, style, align);
        return;
    }
    Mx_SayRgb(x, cy, s, cap, width, Mx_StyleColour(style, 0), align, 0x80);
}

void Mx_SayC(f32 x, f32 cy, char *s, f32 cap, f32 width, u32 top, u32 bottom, u32 line, s32 align, s32 alpha) {
    if (!MX_SAY_GAME) {
        Mx_TextC(x, cy, s, cap, width, top, bottom, line, align, alpha);
        return;
    }
    Mx_SayRgb(x, cy, s, cap, width, top, align, alpha);
}

/* Breaks `text` into lines no wider than `width` at `cap` (words kept whole): up to `max` lines of 64 bytes. */
s32 Mx_Wrap(char *text, f32 cap, f32 width, char (*out)[64], s32 max) {
    s32 n = 0;
    s32 at = 0;

    while (text[at] != 0 && n < max) {
        s32 len = 0;
        s32 cut = -1;

        while (text[at] == ' ') {
            at++;
        }
        while (text[at + len] != 0 && len < 63) {
            out[n][len] = text[at + len];
            out[n][len + 1] = 0;
            if ((sMxWrapSay ? Mx_SayW(out[n], cap) : Mx_TextW(out[n], cap)) > width) {
                break;
            }
            if (text[at + len] == ' ') {
                cut = len;
            }
            len++;
        }
        if (text[at + len] != 0 && cut > 0 && text[at + len] != ' ') {
            len = cut; /* (back to the last space) */
        }
        out[n][len] = 0;
        at += len;
        if (len == 0) {
            break;
        }
        n++;
    }
    if (text[at] != 0 && n == max && n > 0) { /* (more than fits: the last line ends in dots) */
        s32 k = 0;

        while (out[n - 1][k] != 0) {
            k++;
        }
        while (k > 0 && (sMxWrapSay ? Mx_SayW(out[n - 1], cap) + Mx_SayW("...", cap) : Mx_TextW(out[n - 1], cap) + Mx_TextW("...", cap)) > width) {
            out[n - 1][--k] = 0;
        }
        if (k < 60) {
            out[n - 1][k] = '.';
            out[n - 1][k + 1] = '.';
            out[n - 1][k + 2] = '.';
            out[n - 1][k + 3] = 0;
        }
    }
    return n;
}

/* As Mx_Wrap, for lines in the game's font (Mx_Say). */
s32 Mx_SayWrap(char *text, f32 cap, f32 width, char (*out)[64], s32 max) {
    s32 n;

    sMxWrapSay = 1;
    n = Mx_Wrap(text, cap, width, out, max);
    sMxWrapSay = 0;
    return n;
}

/* The end of a long line (a path): what fits in `width`, "..." before it. */
char *Mx_Tail(char *s, f32 cap, f32 width) {
    static char out[2][160];
    static s32 which;
    char *o = out[which ^= 1];
    s32 n = 0;
    s32 at;

    while (s[n] != 0) {
        n++;
    }
    if (Mx_TextW(s, cap) <= width) {
        return s;
    }
    for (at = 0; at < n; at++) {
        sprintf(o, "...%.150s", s + at);
        if (Mx_TextW(o, cap) <= width) {
            return o;
        }
    }
    return o;
}

/* ---------------------------------------------------------------------------------------------- the pad's signs
 * kind: 0 cross, 1 square, 2 triangle, 3 left / right, 4 circle; keys with letters: 5 R2, 6 L2, 7 START, 8 R1, 9 L1,
 * 10 L2 R2, 11 ENTER, 12 ESC, 13 SELECT, 14 L1 R1, 15 the D-pad, 16 the right stick, 17 R3. */
static char *sMxKey[] = {"R2", "L2", "START", "R1", "L1", "L2 R2", "ENTER", "ESC", "SELECT", "L1 R1", "", "R STICK", "R3", "L3"};

static s32 Mx_ButtonPic(s32 kind) {
    switch (kind) {
    case 1:
        return TBO_BTN_SQUARE;
    case 2:
        return TBO_BTN_TRIANGLE;
    case 3:
        return TBO_BTN_LEFTRIGHT;
    case 4:
        return TBO_BTN_CIRCLE;
    }
    return TBO_BTN_CROSS;
}

static f32 Mx_KeyW(s32 kind, f32 h) {
    char *s = sMxKey[kind - 5];
    f32 w = Mx_TextW(s, h * 0.42f) + Mx_LenX(h * 0.5f);

    return w < Mx_LenX(h * 1.2f) ? Mx_LenX(h * 1.2f) : w;
}

f32 Mx_ButtonW(s32 kind, f32 h) {
    if (kind == 3) {
        return Mx_LenX(h * 1.9f);
    }
    if (kind == 15) {
        return Mx_LenX(h);
    }
    if ((kind >= 5 && kind <= 14) || kind == 16 || kind == 17 || kind == 18) {
        if (kind == 10 || kind == 14) { /* two keys side by side */
            return Mx_KeyW(kind == 10 ? 6 : 9, h) + Mx_KeyW(kind == 10 ? 5 : 8, h) + Mx_LenX(h * 0.15f);
        }
        return Mx_KeyW(kind, h);
    }
    return Mx_LenX(h);
}

static void Mx_KeyCap(char *s, f32 x0, f32 cy, f32 w, f32 h) {
    Mx_Box(MM_KEY, x0, cy - Mx_LenY(h * 0.42f), x0 + w, cy + Mx_LenY(h * 0.42f), h * 0.22f, 0x80);
    Mx_Text(x0 + w * 0.5f, cy, s, h * 0.40f, w - Mx_LenX(h * 0.2f), MX_WHITE, 1);
}

/* A sign `h` high whose left edge is at x0, its middle at cy. */
void Mx_Button(s32 kind, f32 x0, f32 cy, f32 h) {
    if (kind == 3) {
        Mx_Fit(TBO_BTN_LEFTRIGHT, x0 + Mx_LenX(h * 0.95f), cy, h, 0x80);
    } else if (kind == 15) { /* the D-pad: a cross of two bars */
        u32 rim = Mx_Rgba(0x20, 0x24, 0x40, 0x80);
        u32 hi = Mx_Rgba(0xD8, 0xDC, 0xE8, 0x80);
        u32 lo = Mx_Rgba(0x90, 0x94, 0xA8, 0x80);
        f32 tx = Mx_LenX(h * 0.17f);
        f32 ty = Mx_LenY(h * 0.17f);
        f32 hx = Mx_LenX(h);
        f32 hy = Mx_LenY(h);

        Mx_Rect(x0 + hx * 0.5f - tx - 2.0f, cy - hy * 0.5f - 2.0f, x0 + hx * 0.5f + tx + 2.0f, cy + hy * 0.5f + 2.0f, rim, rim);
        Mx_Rect(x0 - 2.0f, cy - ty - 2.0f, x0 + hx + 2.0f, cy + ty + 2.0f, rim, rim);
        Mx_Rect(x0 + hx * 0.5f - tx, cy - hy * 0.5f, x0 + hx * 0.5f + tx, cy + hy * 0.5f, hi, lo);
        Mx_Rect(x0, cy - ty, x0 + hx, cy + ty, hi, lo);
    } else if (kind == 10 || kind == 14) {
        f32 w0 = Mx_KeyW(kind == 10 ? 6 : 9, h);

        Mx_KeyCap(sMxKey[(kind == 10 ? 6 : 9) - 5], x0, cy, w0, h);
        Mx_KeyCap(sMxKey[(kind == 10 ? 5 : 8) - 5], x0 + w0 + Mx_LenX(h * 0.15f), cy, Mx_KeyW(kind == 10 ? 5 : 8, h), h);
    } else if ((kind >= 5 && kind <= 13) || kind == 16 || kind == 17 || kind == 18) {
        Mx_KeyCap(sMxKey[kind - 5], x0, cy, Mx_KeyW(kind, h), h);
    } else {
        Mx_Fit(Mx_ButtonPic(kind), x0 + Mx_LenX(h * 0.5f), cy, h, 0x80);
    }
}

/* ---------------------------------------------------------------------------------------------- the prompts
 * The buttons at the foot of the screen: the screen that is up says which, every frame (Mm_Prompt). */
#define MX_PROMPTS 8
static s32 sPromptKind[MX_PROMPTS];
static char *sPromptLabel[MX_PROMPTS];
static s32 sPromptCount;

void Mm_PromptReset(void) {
    sPromptCount = 0;
}

void Mm_Prompt(s32 kind, char *label) {
    if (sPromptCount < MX_PROMPTS) {
        sPromptKind[sPromptCount] = kind;
        sPromptLabel[sPromptCount++] = label;
    }
}

/* The bar at the foot of the screen from x0 to x1 (the mockups' navy bar), the prompts spread over it. */
void Mx_Footer(f32 x0, f32 x1, f32 y0, f32 y1) {
    f32 cap = 25.0f;
    f32 sign = 30.0f;
    f32 room = x1 - x0 - 50.0f;
    f32 need = 0.0f;
    f32 gap;
    f32 x;
    f32 cy = (y0 + y1) * 0.5f;
    s32 k;

    Mx_Box(MM_NAVBAR, x0, y0, x1, y1, 12.0f, 0x80);
    Mx_Frame(x0 + 7.0f, y0 + 6.0f, x1 - 7.0f, y1 - 6.0f, 1.5f, Mx_Rgba(0x80, 0xA8, 0xE8, 0x50));
    Mx_Rect(x0 + 3.0f, y0 + 3.0f, x0 + 9.0f, y0 + 9.0f, Mx_Rgba(0x90, 0xB8, 0xF0, 0x80), Mx_Rgba(0x90, 0xB8, 0xF0, 0x80)); /* (the rivets) */
    Mx_Rect(x1 - 9.0f, y0 + 3.0f, x1 - 3.0f, y0 + 9.0f, Mx_Rgba(0x90, 0xB8, 0xF0, 0x80), Mx_Rgba(0x90, 0xB8, 0xF0, 0x80));
    Mx_Rect(x0 + 3.0f, y1 - 9.0f, x0 + 9.0f, y1 - 3.0f, Mx_Rgba(0x90, 0xB8, 0xF0, 0x80), Mx_Rgba(0x90, 0xB8, 0xF0, 0x80));
    Mx_Rect(x1 - 9.0f, y1 - 9.0f, x1 - 3.0f, y1 - 3.0f, Mx_Rgba(0x90, 0xB8, 0xF0, 0x80), Mx_Rgba(0x90, 0xB8, 0xF0, 0x80));
    for (k = 0; k < sPromptCount; k++) {
        need += Mx_ButtonW(sPromptKind[k], sign) + 8.0f + Mx_TextW(sPromptLabel[k], cap);
    }
    if (sPromptCount > 0 && need + 24.0f * (f32)(sPromptCount - 1) > room) { /* too many words: smaller */
        f32 k2 = room / (need + 24.0f * (f32)(sPromptCount - 1));

        cap *= k2;
        sign *= k2 > 0.8f ? k2 : 0.8f;
        need = 0.0f;
        for (k = 0; k < sPromptCount; k++) {
            need += Mx_ButtonW(sPromptKind[k], sign) + 8.0f + Mx_TextW(sPromptLabel[k], cap);
        }
    }
    gap = sPromptCount > 1 ? (room - need) / (f32)(sPromptCount - 1) : 0.0f;
    if (gap > 140.0f) { /* (a few: spread, in the middle of the bar) */
        gap = 140.0f;
    }
    x = (x0 + x1) * 0.5f - (need + gap * (f32)(sPromptCount - 1)) * 0.5f;
    for (k = 0; k < sPromptCount; k++) {
        f32 bw = Mx_ButtonW(sPromptKind[k], sign);

        Mx_Button(sPromptKind[k], x, cy, sign);
        Mx_Text(x + bw + 8.0f, cy, sPromptLabel[k], cap, 0.0f, MX_WHITE, 0);
        x += bw + 8.0f + Mx_TextW(sPromptLabel[k], cap) + gap;
    }
}

/* The other screens that ask for prompts the manager's way (the fighter creator, the scenario builder): the same bar
   at the foot of the screen. (Its words are drawn with it: the second call has nothing left to do.) */
void Mm_PromptsDraw(f32 rise) {
    Mx_Begin();
    Mx_Footer(12.0f, 1364.0f, MX_FOOT_Y0 + rise, MX_FOOT_Y1 + rise);
}

void Mm_PromptsText(f32 rise) {
    (void)rise;
}

/* ---------------------------------------------------------------------------------------------- what every screen has */
void Mx_Background(s32 dusk) {
    Ps_SpriteScreen(dusk ? MM_BG_DUSK : MM_BG, 0.0f, 0.0f, 512.0f, 448.0f, 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
}

static char *sMxTab[3] = {"LIBRARY", "MOD GRID", "FORMS"};

/* The heading's window of every menu of the game (IconWin, battle/view_a_c.c): the cloud swirls in the corners and the
   line under the title, with their own entrance; made from the Duel menu's data as Tenkaichi Budokai Online's
   (port_tbo.c), without its words and emblem: the title and the emblem are the Mod Manager's. */
extern void IconWin_PortHideWords(void);
extern s32 ColorFade_IsInDone(void);
extern void Heap_Free(void *ptr);
extern void *File_LoadSync(s32 id, void *buf, s32 unused);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void *gMenuArc5;
extern void *Sprite_Unpack(void *src, void *dst, s32 *size);
#define MX_MPACK_AT(pack, n) ((u8 *)(pack) + ((((u32 *)(pack))[n] >> 2) << 2))

static void *sMxWinArc; /* the Duel menu's archive, when it was loaded here */
static void *sMxWinRes;
static s32 sMxWinOpen;  /* 1 once its entrance started (when the screen has faded in, so that it is seen) */
static s32 sMxWinT;     /* frames since it opened */

void Mx_HeaderInit(void) {
    void *arc = gMenuArc5;
    void *icons;

    if (sMxWinRes != NULL) {
        return;
    }
    if (arc == NULL) {
        arc = sMxWinArc = File_LoadSync(0x1C1, NULL, 0); /* (the Duel menu's: gProgress->baseFile) */
    }
    if (arc == NULL) {
        return;
    }
    sMxWinRes = Sprite_Unpack(MX_MPACK_AT(arc, 1), NULL, NULL); /* (Duel_Main: DuelMenu_Run(1)) */
    if (sMxWinRes == NULL) {
        return;
    }
    icons = MX_MPACK_AT(sMxWinRes, 24);
    Res_RelocateOffsets(&icons, icons, icons);
    IconWin_Init(MX_MPACK_AT(sMxWinRes, 23), icons);
    IconWin_PortHideWords();
    sMxWinOpen = 0;
    sMxWinT = 0;
}

void Mx_HeaderTerm(void) {
    if (sMxWinRes != NULL) {
        IconWin_Term();
        Heap_Free(sMxWinRes);
        sMxWinRes = NULL;
    }
    if (sMxWinArc != NULL) {
        Heap_Free(sMxWinArc);
        sMxWinArc = NULL;
    }
}

/* The swirls and the line (under everything of the screen but its backdrop). */
void Mx_HeaderDraw(void) {
    if (sMxWinRes == NULL) {
        return;
    }
    if (!sMxWinOpen) {
        if (!ColorFade_IsInDone()) {
            return;
        }
        IconWin_Open();
        sMxWinOpen = 1;
    }
    IconWin_Draw();
    sMxWinT++;
}

/* The entrance of the title (0) and the emblem (1), as the game's headings come in: the emblem fades in with the
   swirls; the title waits until they are nearly in, comes in squeezed narrow and a little short, and springs out to
   its width (a little past it, and back). Width and height (1 at rest) and opacity (0..1); without the window, at
   rest. */
static void Mx_TitleClip(s32 which, f32 *sx, f32 *sy, f32 *alpha) {
    f32 t = (f32)sMxWinT;

    *sx = 1.0f;
    *sy = 1.0f;
    *alpha = 1.0f;
    if (sMxWinRes == NULL) {
        return;
    }
    if (!sMxWinOpen) {
        *alpha = 0.0f;
        return;
    }
    if (which == 1) {
        *alpha = t < 8.0f ? t / 8.0f : 1.0f;
        return;
    }
    t -= 16.0f;
    if (t < 0.0f) {
        *alpha = 0.0f;
    } else if (t < 8.0f) {
        f32 e = 1.0f - (1.0f - t / 8.0f) * (1.0f - t / 8.0f) * (1.0f - t / 8.0f);

        *sx = 0.3f + 0.76f * e; /* to 1.06 */
        *sy = 0.85f + 0.15f * e;
        *alpha = t < 2.0f ? 0.5f + 0.25f * t : 1.0f;
    } else if (t < 12.0f) {
        *sx = 1.06f - 0.06f * ((t - 8.0f) / 4.0f);
    }
}

/* The emblem and the title, the line under it; the three tabs at the top right (tab: the one lit, -1 none lit,
   -2 no tabs). drop: how far above its place it still is (it comes in). */
void Mx_Chrome(char *title, s32 tab, f32 drop) {
    s32 k;
    f32 x;

    {
        f32 sx;
        f32 sy;
        f32 a;

        Mx_TitleClip(1, &sx, &sy, &a);
        if (a > 0.0f && sx > 0.0f && sy > 0.0f) {
            Mx_Fit(MM_EMBLEM, 76.0f, 66.0f, 112.0f * (sx > sy ? sx : sy), (s32)(a * 128.0f));
        }
        Mx_TitleClip(0, &sx, &sy, &a);
        if (a > 0.0f && sx > 0.0f && sy > 0.0f) {
            sMxHeadingSx = sx / sy; /* (the height is the size: the width is how it is squeezed) */
            sMxHeadingA = (s32)(a * 128.0f);
            Mx_Heading(136.0f, 66.0f, title, 48.0f * sy, 470.0f, 0);
            sMxHeadingSx = 1.0f;
            sMxHeadingA = 0x80;
        }
    }
    if (sMxWinRes == NULL) { /* (the game's line under it: the window's; without the window, one of its own) */
        Mx_Rect(100.0f, 125.0f + drop, 486.0f, 128.0f + drop, Mx_Rgba(0xB8, 0xB0, 0xF0, 0x70), Mx_Rgba(0x70, 0x68, 0xC0, 0x70));
    }
    if (tab == -2) {
        return;
    }
    { /* laid from the right edge leftwards: R1, the three tabs, L1 */
        f32 w[3];
        f32 all = Mx_ButtonW(8, 34.0f) + Mx_ButtonW(9, 34.0f) + 32.0f;

        for (k = 0; k < 3; k++) {
            w[k] = Mx_TextW(sMxTab[k], 25.0f) + 40.0f;
            w[k] = w[k] < 120.0f ? 120.0f : w[k];
            all += w[k];
        }
        x = 1366.0f - all;
        Mx_Button(9, x, 54.0f + drop, 34.0f);
        x += Mx_ButtonW(9, 34.0f) + 8.0f;
        for (k = 0; k < 3; k++) {
            if (k == tab) {
                Mx_Bar(MM_TAB_SEL, x, 33.0f + drop, x + w[k], 76.0f + drop, 0x80);
                Mx_TextC(x + w[k] * 0.5f, 54.0f + drop, sMxTab[k], 25.0f, w[k] - 24.0f, Mx_Rgba(0xFF, 0xF2, 0xB8, 0x80), Mx_Rgba(0xFF, 0xD0, 0x60, 0x80), Mx_Rgba(0x5C, 0x30, 0x08, 0x80), 1, 0x80);
            } else {
                Mx_BarTint(MM_TAB, x, 35.0f + drop, x + w[k], 74.0f + drop, Mx_Rgba(0x66, 0x5C, 0x80, 0x80), tab == -1 ? 0x60 : 0x80);
                Mx_TextC(x + w[k] * 0.5f, 54.0f + drop, sMxTab[k], 25.0f, w[k] - 24.0f, Mx_Rgba(0xE8, 0xDC, 0xFF, 0x80), Mx_Rgba(0xC8, 0xB8, 0xF8, 0x80), Mx_Rgba(0x34, 0x18, 0x60, 0x80), 1,
                         tab == -1 ? 0x60 : 0x80);
            }
            x += w[k] + 8.0f;
        }
    }
    Mx_Button(8, x, 54.0f + drop, 34.0f);
}

/* The two pages of a screen (FORMS: transformations / fusions) as tabs under the three, L2 / R2 at their ends. */
void Mx_SubTabs(char *a, char *b, s32 lit, f32 y) {
    char *name[2];
    f32 x = 1345.0f;
    f32 w[2];
    s32 k;

    name[0] = a;
    name[1] = b;
    for (k = 0; k < 2; k++) {
        w[k] = Mx_TextW(name[k], 26.0f) + 48.0f;
    }
    x -= Mx_ButtonW(5, 34.0f);
    Mx_Button(5, x, y, 34.0f);
    for (k = 1; k >= 0; k--) {
        x -= w[k] + 8.0f;
        if (k == lit) {
            Mx_Bar(MM_TAB_SEL, x, y - 22.0f, x + w[k], y + 22.0f, 0x80);
            Mx_TextC(x + w[k] * 0.5f, y, name[k], 26.0f, 0.0f, Mx_Rgba(0xFF, 0xF2, 0xB8, 0x80), Mx_Rgba(0xFF, 0xD0, 0x60, 0x80), Mx_Rgba(0x5C, 0x30, 0x08, 0x80), 1, 0x80);
        } else {
            Mx_BarTint(MM_TAB, x, y - 20.0f, x + w[k], y + 20.0f, Mx_Rgba(0x66, 0x5C, 0x80, 0x80), 0x80);
            Mx_TextC(x + w[k] * 0.5f, y, name[k], 26.0f, 0.0f, Mx_Rgba(0xE8, 0xDC, 0xFF, 0x80), Mx_Rgba(0xC8, 0xB8, 0xF8, 0x80), Mx_Rgba(0x34, 0x18, 0x60, 0x80), 1, 0x80);
        }
    }
    x -= Mx_ButtonW(6, 34.0f) + 8.0f;
    Mx_Button(6, x, y, 34.0f);
}

/* A label bar under the title (the screen's own name: "ADD MODS", "CHOOSE ITS ICON"). */
void Mx_Label(f32 x0, f32 y0, f32 x1, f32 y1, char *s) {
    Mx_Box(MM_NAVBAR, x0, y0, x1, y1, (y1 - y0) * 0.5f, 0x80);
    Mx_Text(x0 + 22.0f, (y0 + y1) * 0.5f, s, (y1 - y0) * 0.56f, x1 - x0 - 40.0f, MX_GOLD, 0);
}

/* A line of news (Mm_Note) over the foot of the screen, fading. */
void Mx_Note(char *line, s32 left, f32 y) {
    s32 a = left > 16 ? 0x80 : left * 8;
    f32 w;

    if (left <= 0 || line[0] == 0) {
        return;
    }
    w = Mx_TextW(line, 24.0f) + 70.0f;
    if (w > 1200.0f) {
        w = 1200.0f;
    }
    Mx_Box(MM_WINDOW_DARK, 688.0f - w * 0.5f, y - 25.0f, 688.0f + w * 0.5f, y + 25.0f, 16.0f, a);
    Mx_Frame(688.0f - w * 0.5f + 4.0f, y - 21.0f, 688.0f + w * 0.5f - 4.0f, y + 21.0f, 1.5f, Mx_Rgba(0xFF, 0xC8, 0x40, a / 2));
    Mx_TextA(688.0f, y, line, 24.0f, w - 40.0f, MX_GOLD, 1, a);
}

/* A row of stock pips: `n` lit of `max` shown (max 0: only the lit ones), each `size` across, from x0. */
void Mx_Pips(f32 x0, f32 cy, s32 n, s32 max, f32 size) {
    s32 k;
    s32 all = max > 0 ? max : n;

    for (k = 0; k < all; k++) {
        Mx_Fit(k < n ? MM_PIP : MM_PIP_OFF, x0 + Mx_LenX(size * 0.5f + size * 0.82f * (f32)k), cy, size, 0x80);
    }
}

/* ---------------------------------------------------------------------------------------------- faces
 * The face of any fighter for the lists: the picture the battle gauge shows (file 8 + character * 2), read the first
 * time it is asked for and kept until the manager closes (Mx_FreeFaces). Mods: their chip when they have one. */
static MmRes *sMxFace[MM_CHARA_COUNT];
static u8 sMxFaceTried[MM_CHARA_COUNT];

/* The character select's own faces (the miniatures of its grids): the chip pack of its screen (the menu archive,
   file 0x1C1, section 2; in it section 45, a picture per character id + 1), copied out once when the manager
   opens, everything else of the archive let go at once. */
extern void *gMenuArc5;
extern void *Sprite_Unpack(void *src, void *dst, s32 *size);
static u8 *sMxChips;
static u8 sMxChipDone[MM_CHARA_COUNT + 8];

void Mx_ChipsOpen(void) {
    void *arc = gMenuArc5;
    s32 own = 0;
    u32 *res;

    if (sMxChips != NULL) {
        return;
    }
    if (arc == NULL) {
        arc = File_LoadSync(0x1C1, NULL, 0);
        own = 1;
    }
    if (arc == NULL) {
        return;
    }
    res = Sprite_Unpack(MX_MPACK_AT(arc, 2), NULL, NULL);
    if (res != NULL) {
        if (res[0] > 46 && res[0] < 1000) {
            u32 a = (res[45] >> 2) << 2;
            u32 b = (res[46] >> 2) << 2;

            if (b > a && b - a < 0x400000) {
                sMxChips = Heap_Alloc(b - a, 0x40, 0, 2);
                if (sMxChips != NULL) {
                    memcpy(sMxChips, (u8 *)res + a, b - a);
                }
            }
        }
        Heap_Free(res);
    }
    if (own) {
        Heap_Free(arc);
    }
    memset(sMxChipDone, 0, sizeof(sMxChipDone));
}

void Mx_ChipsClose(void) {
    if (sMxChips != NULL) {
        Heap_Free(sMxChips);
        sMxChips = NULL;
    }
}

/* A character's select-screen face, NULL if there is none. */
static MmTex *Mx_ChipTex(s32 chara) {
    MmRes *res;

    if (sMxChips == NULL || chara < 0 || chara >= MM_CHARA_COUNT || ((u32 *)sMxChips)[0] < (u32)chara + 1) {
        return NULL;
    }
    res = (MmRes *)MX_MPACK_AT(sMxChips, chara + 1);
    if (!sMxChipDone[chara]) {
        Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
        sMxChipDone[chara] = 1;
    }
    return res->tex;
}

void Mx_FreeFaces(void) {
    s32 i;

    for (i = 0; i < MM_CHARA_COUNT; i++) {
        if (sMxFace[i] != NULL) {
            Heap_Free(sMxFace[i]);
            sMxFace[i] = NULL;
        }
        sMxFaceTried[i] = 0;
    }
}

static MmTex *Mx_FaceTex(s32 chara) {
    if (chara < 0 || chara >= MM_CHARA_COUNT) {
        return NULL;
    }
    if (!sMxFaceTried[chara]) {
        MmRes *res;

        sMxFaceTried[chara] = 1;
        res = File_LoadSync(8 + chara * 2, NULL, 0);
        if (res != NULL) {
            Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
            sMxFace[chara] = res;
        }
    }
    return sMxFace[chara] != NULL ? sMxFace[chara]->tex : NULL;
}

/* The largest box of the picture's real shape square in (x0, y0)..(x1, y1), about its middle. */
static void Mx_Square(f32 *x0, f32 *y0, f32 *x1, f32 *y1) {
    f32 cx = (*x0 + *x1) * 0.5f;
    f32 cy = (*y0 + *y1) * 0.5f;
    f32 h = *y1 - *y0;
    f32 w = Mx_SameX(h);

    if (w > *x1 - *x0) {
        h *= (*x1 - *x0) / w;
        w = *x1 - *x0;
    }
    *x0 = cx - w * 0.5f;
    *x1 = cx + w * 0.5f;
    *y0 = cy - h * 0.5f;
    *y1 = cy + h * 0.5f;
}

/* A fighter's face in a square: a character id, or PORT_REF_MOD + a mod (its chip, else its fighter's face). */
void Mx_Face(s32 ref, f32 x0, f32 y0, f32 x1, f32 y1, s32 shade) {
    MmTex *t;
    s32 chara = ref;

    Mx_Square(&x0, &y0, &x1, &y1);
    Mx_Rect(x0, y0, x1, y1, Mx_Rgba(0x1C, 0x2A, 0x5C, 0x80), Mx_Rgba(0x0A, 0x12, 0x30, 0x80));
    if (ref >= PORT_REF_MOD && sMm != NULL && ref - PORT_REF_MOD < sMm->count) {
        t = Mm_Chip(ref - PORT_REF_MOD);
        if (t != NULL) {
            Mx_Tex(t, x0, y0, x1, y1, 0, 0, 64, 64, shade, 0x80);
            return;
        }
        chara = sMm->info[ref - PORT_REF_MOD].chara;
    }
    t = Mx_ChipTex(chara);
    if (t != NULL) {
        Mx_Tex(t, x0, y0, x1, y1, 0, 0, 64, 64, shade, 0x80);
        return;
    }
    t = Mx_FaceTex(chara); /* (no select face: the battle gauge's) */
    if (t != NULL) {
        Mx_Tex(t, x0, y0, x1, y1, 18, 2, 46, 32, shade, 0x80);
    }
}

/* A disc `d` high about (cx, cy), filled from `top` to `bottom`: thin bands, each as wide as the circle there. */
#define MX_BANDS 16
void Mx_Disc(f32 cx, f32 cy, f32 d, u32 top, u32 bottom) {
    f32 r = d * 0.5f;
    f32 rx = Mx_SameX(d) * 0.5f;
    s32 i;

    for (i = 0; i < MX_BANDS; i++) {
        f32 a = -1.0f + 2.0f * (f32)i / MX_BANDS;
        f32 m = a + 1.0f / MX_BANDS;
        f32 half = sqrtf(1.0f - m * m);
        u32 c = Ps_Mix(top, bottom, ((f32)i + 0.5f) / MX_BANDS) | (top & 0xFF000000u); /* (Ps_Mix leaves the alpha out) */

        Mx_Rect(cx - rx * half, cy + a * r, cx + rx * half, cy + (a + 2.0f / MX_BANDS) * r, c, c);
    }
}

/* A fighter's face in a circle `d` high about (cx, cy) (its select chip, else its battle gauge's face), drawn as bands
   as wide as the circle so that none of the square shows outside it. */
static void Mx_RoundOf(MmTex *t, f32 cx, f32 cy, f32 d, s32 shade, s32 chara);

void Mx_TexRound(MmTex *t, f32 cx, f32 cy, f32 d, s32 shade) {
    Mx_RoundOf(t, cx, cy, d, shade, -1);
}

void Mx_FaceRound(s32 chara, f32 cx, f32 cy, f32 d, s32 shade) {
    Mx_RoundOf(Mx_ChipTex(chara), cx, cy, d, shade, chara);
}

static void Mx_RoundOf(MmTex *t, f32 cx, f32 cy, f32 d, s32 shade, s32 chara) {
    f32 r = d * 0.5f;
    f32 rx = Mx_SameX(d) * 0.5f;
    f32 u0 = 0.0f, v0 = 0.0f, u1 = 64.0f, v1 = 64.0f;
    s32 i;

    Mx_Disc(cx, cy, d, Mx_Rgba(0x2C, 0x22, 0x5C, 0x80), Mx_Rgba(0x10, 0x0A, 0x30, 0x80));
    if (t == NULL && (t = Mx_FaceTex(chara)) != NULL) {
        u0 = 18.0f;
        v0 = 2.0f;
        u1 = 46.0f;
        v1 = 32.0f;
    }
    if (t == NULL) {
        return;
    }
    for (i = 0; i < MX_BANDS; i++) {
        f32 a = -1.0f + 2.0f * (f32)i / MX_BANDS;
        f32 m = a + 1.0f / MX_BANDS;
        f32 half = sqrtf(1.0f - m * m);
        f32 uc = (u0 + u1) * 0.5f;
        f32 uh = (u1 - u0) * 0.5f * half;
        f32 va = v0 + (v1 - v0) * (f32)i / MX_BANDS;
        f32 vb = v0 + (v1 - v0) * (f32)(i + 1) / MX_BANDS;

        Mx_Tex(t, cx - rx * half, cy + a * r, cx + rx * half, cy + (a + 2.0f / MX_BANDS) * r, (s32)(uc - uh + 0.5f), (s32)(va + 0.5f),
               (s32)(uc + uh + 0.5f), (s32)(vb + 0.5f), shade, 0x80);
    }
}

/* A square face in the mockups' frame: a light rim, the face, a gold rim when it is lit. */
void Mx_FaceCell(s32 ref, f32 x0, f32 y0, f32 x1, f32 y1, s32 lit) {
    Mx_Square(&x0, &y0, &x1, &y1);
    Mx_Rect(x0 - Mx_LenX(3.0f), y0 - Mx_LenY(3.0f), x1 + Mx_LenX(3.0f), y1 + Mx_LenY(3.0f), lit ? Mx_Rgba(0xFF, 0xE8, 0x80, 0x80) : Mx_Rgba(0xC8, 0xE0, 0xF0, 0x80),
            lit ? Mx_Rgba(0xE0, 0x98, 0x20, 0x80) : Mx_Rgba(0x70, 0x98, 0xB8, 0x80));
    Mx_Face(ref, x0, y0, x1, y1, 0x80);
}

/* The light the model stands on. */
void Mx_Platform(f32 cx, f32 feet, f32 w) {
    f32 h = Mx_LenY(w * Pic_H(MM_PLATFORM) / Pic_W(MM_PLATFORM));

    w = Mx_LenX(w);
    Mx_Pic(MM_PLATFORM, cx - w * 0.5f, feet - h * 0.5f, cx + w * 0.5f, feet + h * 0.5f, 0x80);
}

/* Gold corner brackets round a box (the cursor of the face grids and of the icon's square). */
void Mx_Brackets(f32 x0, f32 y0, f32 x1, f32 y1, f32 len, f32 t, u32 c) {
    f32 lx = Mx_LenX(len);
    f32 ly = Mx_LenY(len);
    f32 tx = Mx_LenX(t);
    f32 ty = Mx_LenY(t);

    Mx_Rect(x0, y0, x0 + lx, y0 + ty, c, c);
    Mx_Rect(x0, y0, x0 + tx, y0 + ly, c, c);
    Mx_Rect(x1 - lx, y0, x1, y0 + ty, c, c);
    Mx_Rect(x1 - tx, y0, x1, y0 + ly, c, c);
    Mx_Rect(x0, y1 - ty, x0 + lx, y1, c, c);
    Mx_Rect(x0, y1 - ly, x0 + tx, y1, c, c);
    Mx_Rect(x1 - lx, y1 - ty, x1, y1, c, c);
    Mx_Rect(x1 - tx, y1 - ly, x1, y1, c, c);
}

/* An aura's glow standing on `feet`, `h` high (a size), in `c` (its alpha the brightest): a flame of soft bands, wider
   below, that flickers with `time`. */
static void Mx_GlowLayer(f32 cx, f32 feet, f32 h, u32 c, s32 time, f32 wide, f32 tall);

void Mx_Glow(f32 cx, f32 feet, f32 h, u32 c, s32 time) {
    Mx_GlowLayer(cx, feet, h, c, time, 0.85f, 1.6f);                                     /* (the outer, softer) */
    Mx_GlowLayer(cx, feet, h, (c & 0xFFFFFF) | 0x60000000u, time + 40, 0.5f, 1.4f); /* (the core) */
}

static void Mx_GlowLayer(f32 cx, f32 feet, f32 h, u32 c, s32 time, f32 wide, f32 tall) {
    f32 hy = Mx_LenY(h) * tall;
    f32 w = Mx_LenX(h) * wide;
    s32 a = (s32)(c >> 24);
    s32 b;

    for (b = 0; b < 12; b++) {
        f32 u0 = (f32)b / 12.0f;
        f32 u1 = (f32)(b + 1) / 12.0f;
        f32 w0 = w * (1.0f - u0 * 0.45f) * (u0 < 0.1f ? 0.55f + u0 * 4.5f : 1.0f) * (1.0f + 0.06f * sinf((f32)time * 0.21f + (f32)b * 1.7f));
        f32 w1 = w * (1.0f - u1 * 0.45f) * (u1 < 0.1f ? 0.55f + u1 * 4.5f : 1.0f) * (1.0f + 0.06f * sinf((f32)time * 0.21f + (f32)(b + 1) * 1.7f));
        f32 y0 = feet - hy * u0;
        f32 y1 = feet - hy * u1;
        u32 m0 = (c & 0xFFFFFF) | ((u32)((f32)a * (1.0f - u0 * 0.7f)) << 24);
        u32 m1 = (c & 0xFFFFFF) | ((u32)((f32)a * (1.0f - u1 * 0.7f)) << 24);
        u32 e = c & 0xFFFFFF;

        Ps_ScreenQuad(Mx_X(cx - w1), Mx_Y(y1), Mx_X(cx), Mx_Y(y1), Mx_X(cx - w0), Mx_Y(y0), Mx_X(cx), Mx_Y(y0), e, m1, e, m0);
        Ps_ScreenQuad(Mx_X(cx), Mx_Y(y1), Mx_X(cx + w1), Mx_Y(y1), Mx_X(cx), Mx_Y(y0), Mx_X(cx + w0), Mx_Y(y0), m1, e, m0, e);
    }
}

/* Lightning round a model (Super Saiyan 2 / 3): a few jagged bolts, new ones every few frames. */
void Mx_Bolts(f32 cx, f32 feet, f32 h, s32 time) {
    u32 seed = (u32)(time / 4) * 2654435761u;
    f32 hy = Mx_LenY(h);
    f32 wx = Mx_LenX(h) * 0.42f;
    s32 b;
    s32 s;

    for (b = 0; b < 3; b++) {
        f32 x;
        f32 y;
        f32 t = Mx_LenX(2.2f);

        seed = seed * 1103515245u + 12345u;
        x = cx + ((f32)((seed >> 8) & 1023) / 1023.0f - 0.5f) * wx * 2.0f;
        seed = seed * 1103515245u + 12345u;
        y = feet - hy * (0.25f + 0.7f * (f32)((seed >> 8) & 1023) / 1023.0f);
        if (((seed >> 20) & 3) == 0) {
            continue; /* (now and then fewer) */
        }
        for (s = 0; s < 5; s++) {
            f32 nx;
            f32 ny = y + hy * 0.045f;
            u32 c = Mx_Rgba(255, 250, 200, 0x78);

            seed = seed * 1103515245u + 12345u;
            nx = x + ((f32)((seed >> 8) & 1023) / 1023.0f - 0.5f) * wx * 0.35f;
            Ps_ScreenQuad(Mx_X(x - t), Mx_Y(y), Mx_X(x + t), Mx_Y(y), Mx_X(nx - t), Mx_Y(ny), Mx_X(nx + t), Mx_Y(ny), c, c, c, c);
            x = nx;
            y = ny;
        }
    }
}

/* The window the mockups have most: teal, a light rim. */
void Mx_Window(f32 x0, f32 y0, f32 x1, f32 y1) {
    Mx_Box(MM_WINDOW, x0, y0, x1, y1, 22.0f, 0x80);
}

/* A row of a list: teal, or gold when it is the cursor's. */
void Mx_Row(f32 x0, f32 y0, f32 x1, f32 y1, s32 sel, s32 alpha) {
    if (sel) {
        Mx_Box(MM_BAR_SEL, x0 - 4.0f, y0 - 3.0f, x1 + 4.0f, y1 + 3.0f, (y1 - y0) * 0.36f, alpha);
    } else {
        Mx_Box(MM_BAR, x0, y0, x1, y1, (y1 - y0) * 0.36f, alpha);
    }
}
#endif
