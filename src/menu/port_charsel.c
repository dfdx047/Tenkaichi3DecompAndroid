#include "common.h"
#include "menu/menu_a.h"
#include "menu/port_charsel.h"
#include "menu/port_screen.h"
#include "menu/menu_g.h"
#include "sys/pad.h"

#ifdef PORT
/*
 * The character select drawn as a roster on the left, the fighter in the middle and its details on the right.
 *
 * Both selects (one on one, menu_c_e.c; teams, menu_e_b.c) describe themselves in a CsView each frame. Only the drawing is new. Their Input, Update and Draw still run as they always did, so the cursors,
 * the forms, the costumes, the item sets and what goes to the battle are the game's own. What CharSel_Draw puts in
 * the frame is left out by two markers in the display list (Port_GsMarker 0x22, 0x23: textures are still uploaded,
 * nothing is drawn). The stage and music choice is the game's screen as it is.
 *
 * The fighter is the character reference's viewer, in a world of its own (src/battle/view_a_d.c): in every mode, online
 * too, it leaves the game's state as it found it. While none is up the portrait the game loads anyway stands in.
 *
 * The chips are the disc's 48 x 41: the renderer enlarges them when it decodes them (Scale2x twice, markers 0x26, 0x27).
 *
 * BT3_SELECT_CLASSIC=1 or the setting select_classic = 1: the game's own screen.
 *
 * (No characters outside ASCII in this file: the ptr32 step breaks on them. Use \x escapes.)
 */

#include <stdio.h>
#include <stdlib.h>
/* (as port/src/port_host.h, which the game's sources do not see: left out of the game's state) */
#ifdef __APPLE__
#define PORT_HOST __attribute__((section("__DATA,__porthst")))
#else
#define PORT_HOST __attribute__((section(".porthst")))
#endif

extern int Port_Setting(const char *name, int def);
extern int Port_NetActive(void), Port_NetSession(void), Port_NetMe(void);
extern int Port_PageAspectMilli(void);
extern void Port_MenuScene3D(int on);
extern void Port_GsMarker(s32 effect);
extern s32 ChrTbl_GetCost(s32 chara);
extern void ChrView_PortFrame(f32 *screenX, f32 *yaw, s32 *chara, s32 *costume, s32 split);
extern s32 ChrView_PortVisible(s32 side);
extern void ChrView_PortEnd(void);
extern int Port_NetListShown(void);
extern int Port_WorldCan(void);
extern s32 ChrView_PortPedestal(s32 side, f32 *cx, f32 *cy, f32 *rx, f32 *ry);
extern f32 sinf(f32), cosf(f32);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
#define MCHR_COLS 7
#define MCHR_RANDOM 0xA1
#define MCHR_LOCKED 0xA2
#define MCHR_CUSTOM 0xA3
#define MCHR_EMPTY 0xA4

/* (sys/loading.h and sys/sprite.h have these, with a gProgress of another type than the menus') */
#define LOAD_SPR_DRAW 1
#define LOAD_SPR_END 2
typedef struct CsSprite {
    s32 flags, x0, y0, x1, y1, u0, v0, u1, v1, tex, r, g, b, a;
} CsSprite; /* LoadSprite: 0x38 bytes */
extern void Sprite_DrawList(void *res, s32 x, s32 y, CsSprite *list);

#define GS_MUTE_ON 0x22
#define GS_MUTE_OFF 0x23
#define GS_WHOLE_ON 0x24 /* 2D is laid out on the whole window (the scene mode would narrow it to 3/4) */
#define GS_WHOLE_OFF 0x25
#define GS_CHIP_ON 0x26 /* the chips drawn from here are enlarged when decoded */
#define GS_CHIP_OFF 0x27

#define CS_VIS_ROWS 6
#define CS_WAIT 10 /* frames the cursor rests on a fighter before its model is read */

/* Host variables: what one machine shows (its own side, its timers) is not part of the game's state. */
static PORT_HOST s32 sOn;       /* the new screen is in use */
static PORT_HOST s32 s3D;       /* with the viewer */
static PORT_HOST s32 sTick;
static PORT_HOST s32 sSplit;    /* two people choosing: the screen in two halves */
static PORT_HOST s32 sNow;      /* the half being drawn */
static PORT_HOST s32 sTopRowOf[2];
static PORT_HOST s32 sWantChara[2], sWantCostume[2], sShownChara[2], sShownCostume[2];
static PORT_HOST s32 sRest[2];
static PORT_HOST CsView sViews[2];
/* the roster's place: set for each half */
static PORT_HOST f32 CS_CELL_H = 47.0f, CS_GAP = 3.0f, CS_GRID_X = 10.0f, CS_GRID_Y = 62.0f;
static PORT_HOST s32 sInScene;
static PORT_HOST CsView sV;
static PORT_HOST f32 sYawOf[2];
static PORT_HOST f32 sTabOf[2];  /* the personalization tab: 0 out of the screen, 1 in */
static PORT_HOST f32 sItemOf[2]; /* the item panel */
static PORT_HOST s32 sRed;           /* what is drawn now is the rival's: the blue goes red */
static PORT_HOST f32 sFormOf[2]; /* the list of forms: 0 closed, 1 open */
static PORT_HOST s32 sFormCell[2]; /* the roster's cell that list is of */
static PORT_HOST f32 sAlpha = 1.0f;
static PORT_HOST f32 sShift;  /* what is drawn now is this far to the right */

static const char *Cs_Words(s32 id) {
    enum { LANGS = 3 };
    static const char *const kWords[][LANGS] = {
        { "Player 1", "Jogador 1", "Jugador 1" },
        { "Player 2", "Jogador 2", "Jugador 2" },
        { "CPU", "CPU", "CPU" },
        { "Choose your fighter", "Escolha seu lutador", "Elige tu luchador" },
        { "Transformations", "Transforma\xC3\xA7\xC3\xB5" "es", "Transformaciones" },
        { "Costume", "Traje", "Traje" },
        { "Customization", "Personaliza\xC3\xA7\xC3\xA3o", "Personalizaci\xC3\xB3n" },
        { "Normal", "Normal", "Normal" },
        { "Custom 1", "Personal 1", "Personal 1" },
        { "Custom 2", "Personal 2", "Personal 2" },
        { "Custom 3", "Personal 3", "Personal 3" },
        { "Battle cost", "Custo em batalha", "Coste en batalla" },
        { "Confirm", "Confirmar", "Confirmar" },
        { "Back", "Voltar", "Volver" },
        { "Random", "Aleat\xC3\xB3rio", "Aleatorio" },
        { "Locked", "Bloqueado", "Bloqueado" },
        { "Saved characters", "Personagens salvos", "Personajes guardados" },
        { "Right stick: rotate", "Anal\xC3\xB3gico direito: girar", "Stick derecho: girar" },
        { "Items", "Itens", "Objetos" },
        { "Empty", "Vazio", "Vac\xC3\xADo" },
        { "Team", "Time", "Equipo" },
        { "Ready!", "Pronto!", "\xC2\xA1Listo!" },
    };
    s32 lang = Port_Setting("dr_lang", 0);

    return kWords[id][lang >= 0 && lang < LANGS ? lang : 0];
}
enum { W_P1, W_P2, W_CPU, W_CHOOSE, W_FORMS, W_COSTUME, W_CUSTOM, W_NORMAL, W_C1, W_C2, W_C3, W_COST, W_OK, W_BACK,
       W_RANDOM, W_LOCKED, W_SAVED, W_ROTATE, W_ITEMS, W_EMPTY, W_TEAM, W_READY };

/* The side a screen is about online (this machine's); otherwise the one its caller worked out. */
s32 PortCharSel_NetSide(s32 fallback) {
    if (Port_NetActive() || Port_NetSession()) {
        return Port_NetMe() & 1;
    }
    return fallback;
}

/* Line n of one of the game's text files as UTF-8; its tags left out. */
static void Cs_Line(char *out, s32 cap, u32 *file, s32 n) {
    u32 ofs;
    u16 *str;
    s32 len = 0;
    s32 tag = 0;

    out[0] = 0;
    if (file == NULL || n < 0 || (u32)n + 1 >= file[1] >> 2) {
        return;
    }
    ofs = file[n + 1] >> 2 << 2;
    if (ofs == 0 || ofs >= 0x200000) {
        return;
    }
    str = (u16 *)((u8 *)file + ofs);
    if (str[0] != 0xFEFF) {
        return;
    }
    for (str++; *str != 0 && len < cap - 4; str++) {
        u32 c = *str;

        if (c == '<') {
            tag = 1;
        } else if (c == '>') {
            tag = 0;
        } else if (tag) {
        } else if (c == '\n' || c == '\r') {
            out[len++] = ' ';
        } else if (c < 0x80) {
            out[len++] = (char)c;
        } else if (c < 0x800) {
            out[len++] = (char)(0xC0 | (c >> 6));
            out[len++] = (char)(0x80 | (c & 0x3F));
        } else {
            out[len++] = (char)(0xE0 | (c >> 12));
            out[len++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[len++] = (char)(0x80 | (c & 0x3F));
        }
    }
    out[len] = 0;
}

/* ---- shapes, in the window's 512 x 448 (the whole window, whatever its shape) ---- */

/* (worked out once a frame: these screens ask for it some hundreds of times, in soft floats on many phones) */
static PORT_HOST f32 sAcrossPerDown = 1.0f;
static PORT_HOST s32 sNarrow; /* no room for roster, fighter and details side by side: the details go under the roster */

static void Cs_MeasureWindow(void) {
    s32 aspect = Port_PageAspectMilli();

    sAcrossPerDown = (512.0f / 448.0f) * 1000.0f / (f32)aspect;
    sNarrow = aspect < 1900;
}

static f32 Cs_UnitsAcross(f32 unitsDown) { /* so many units across are as long as this many down */
    return unitsDown * sAcrossPerDown;
}

static u32 Cs_A(u32 c) {
    return (c & 0xFFFFFF) | ((u32)((f32)(c >> 24) * sAlpha) << 24);
}

static void Cs_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    top = Cs_A(top);
    bottom = Cs_A(bottom);
    Ps_ScreenQuad(x0 + sShift, y0, x1 + sShift, y0, x0 + sShift, y1, x1 + sShift, y1, top, top, bottom, bottom);
}

/* A plate with a slanted right end, lit along its top. */
static void Cs_Plate(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    f32 cut = Cs_UnitsAcross((y1 - y0) * 0.45f);
    u32 t = Cs_A(top);
    u32 b = Cs_A(bottom);

    Ps_ScreenQuad(x0 + sShift, y0, x1 + sShift, y0, x0 + sShift, y1, x1 - cut + sShift, y1, t, t, b, b);
    Cs_Rect(x0, y0, x1 - cut * 0.1f, y0 + 1.2f, Ps_Rgba(255, 255, 255, 0x38), Ps_Rgba(255, 255, 255, 0x10));
}

static void Cs_Frame(f32 x0, f32 y0, f32 x1, f32 y1, f32 t, u32 c) {
    f32 tx = Cs_UnitsAcross(t);

    Cs_Rect(x0, y0, x1, y0 + t, c, c);
    Cs_Rect(x0, y1 - t, x1, y1, c, c);
    Cs_Rect(x0, y0, x0 + tx, y1, c, c);
    Cs_Rect(x1 - tx, y0, x1, y1, c, c);
}

/* Between two colours; t from 0 to 256. */
static u32 Cs_Mix(u32 a, u32 b, s32 t) {
    u32 out = 0;
    s32 k;

    for (k = 0; k < 32; k += 8) {
        s32 x = (s32)((a >> k) & 0xFF);
        s32 y = (s32)((b >> k) & 0xFF);

        out |= (u32)(x + ((y - x) * t >> 8)) << k;
    }
    return out;
}

/* The look of these screens: a panel of blue glass with round corners and a line of light around it. Some thirty flat
   quads, no texture. */
static void Cs_Glass(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, u32 top, u32 bottom, u32 line) {
    static const f32 kBend = 0.29f;
    static const f32 kDir[12][2] = { { -1.0f, 0.0f }, { -0.7f, -0.7f }, { 0.0f, -1.0f }, { 0.0f, -1.0f }, { 0.7f, -0.7f }, { 1.0f, 0.0f },
                                     { 1.0f, 0.0f }, { 0.7f, 0.7f }, { 0.0f, 1.0f }, { 0.0f, 1.0f }, { -0.7f, 0.7f }, { -1.0f, 0.0f } };
    f32 unit = sAcrossPerDown;
    f32 rx = r * unit;
    u32 soft;
    f32 xs[12];
    f32 ys[12];
    f32 levelY[6];
    f32 levelX[6];
    f32 tall = y1 - y0;
    u32 off;
    s32 i;

    x0 += sShift;
    x1 += sShift;
    xs[0] = x0; ys[0] = y0 + r;
    xs[1] = x0 + rx * kBend; ys[1] = y0 + r * kBend;
    xs[2] = x0 + rx; ys[2] = y0;
    xs[3] = x1 - rx; ys[3] = y0;
    xs[4] = x1 - rx * kBend; ys[4] = y0 + r * kBend;
    xs[5] = x1; ys[5] = y0 + r;
    xs[6] = x1; ys[6] = y1 - r;
    xs[7] = x1 - rx * kBend; ys[7] = y1 - r * kBend;
    xs[8] = x1 - rx; ys[8] = y1;
    xs[9] = x0 + rx; ys[9] = y1;
    xs[10] = x0 + rx * kBend; ys[10] = y1 - r * kBend;
    xs[11] = x0; ys[11] = y1 - r;
    levelY[0] = y0; levelX[0] = rx;
    levelY[1] = y0 + r * kBend; levelX[1] = rx * kBend;
    levelY[2] = y0 + r; levelX[2] = 0.0f;
    levelY[3] = y1 - r; levelX[3] = 0.0f;
    levelY[4] = y1 - r * kBend; levelX[4] = rx * kBend;
    levelY[5] = y1; levelX[5] = rx;
    top = Cs_A(top);
    bottom = Cs_A(bottom);
    line = Cs_A(line);
    off = line & 0xFFFFFF;
    soft = Cs_Mix(off, line, 115);
    for (i = 0; i < 5; i++) {
        u32 c0 = Cs_Mix(top, bottom, (s32)((levelY[i] - y0) * 256.0f / tall));
        u32 c1 = Cs_Mix(top, bottom, (s32)((levelY[i + 1] - y0) * 256.0f / tall));

        Ps_ScreenQuad(x0 + levelX[i], levelY[i], x1 - levelX[i], levelY[i], x0 + levelX[i + 1], levelY[i + 1], x1 - levelX[i + 1], levelY[i + 1], c0, c0, c1, c1);
    }
    for (i = 0; i < 12; i++) {
        s32 n = (i + 1) % 12;
        f32 ax = xs[i] + kDir[i][0] * unit * 3.5f;
        f32 ay = ys[i] + kDir[i][1] * 3.5f;
        f32 bx = xs[n] + kDir[n][0] * unit * 3.5f;
        f32 by = ys[n] + kDir[n][1] * 3.5f;
        f32 cx = xs[i] + kDir[i][0] * unit * 1.1f;
        f32 cy = ys[i] + kDir[i][1] * 1.1f;
        f32 dx = xs[n] + kDir[n][0] * unit * 1.1f;
        f32 dy = ys[n] + kDir[n][1] * 1.1f;

        /* the soft light going out, then the line itself */
        Ps_ScreenQuad(xs[i], ys[i], xs[n], ys[n], ax, ay, bx, by, soft, soft, off, off);
        Ps_ScreenQuad(xs[i], ys[i], xs[n], ys[n], cx, cy, dx, dy, line, line, line, line);
    }
}

#define CS_GLASS_TOP (sRed ? Ps_Rgba(0xA8, 0x14, 0x1C, 0x5C) : Ps_Rgba(0x12, 0x40, 0xA8, 0x5C))
#define CS_GLASS_LOW (sRed ? Ps_Rgba(0x58, 0x06, 0x0C, 0x6C) : Ps_Rgba(0x04, 0x16, 0x58, 0x6C))
#define CS_GLASS_LINE (sRed ? Ps_Rgba(0xFF, 0x60, 0x50, 0x80) : Ps_Rgba(0x5C, 0xCC, 0xFF, 0x80))
#define CS_GLASS_GOLD Ps_Rgba(0xFF, 0xD0, 0x50, 0x80)

/* Text placed in window units: Ps_TextEx works in the page's. */
static f32 Cs_PageX(f32 x) {
    return (x - Ps_X(0.0f)) * 512.0f / (Ps_X(512.0f) - Ps_X(0.0f));
}
static f32 Cs_PageY(f32 y) {
    return (y - Ps_Y(0.0f)) * 448.0f / (Ps_Y(448.0f) - Ps_Y(0.0f));
}
static void Cs_Text(f32 x, f32 y, char *s, f32 scale, f32 width, s32 align, u32 top, u32 bottom) {
    f32 pageW = Cs_PageX(width) - Cs_PageX(0.0f);

    Ps_Align(align);
    Ps_TextExOuter(Cs_PageX(x + sShift), Cs_PageY(y), s, scale, pageW, Cs_A(top), Cs_A(bottom), Cs_A(Ps_Rgba(0x06, 0x12, 0x38, 0x80)),
                   Cs_A(Ps_Rgba(0x06, 0x12, 0x38, 0x48)), 2.0f);
    Ps_Align(0);
}

/* The game's heading lettering: gold into red, with its bevel. For what the eye goes to first. */
static void Cs_Title(f32 x, f32 y, char *s, f32 scale, f32 width, s32 align) {
    f32 pageW = Cs_PageX(width) - Cs_PageX(0.0f);

    Ps_Align(align);
    Ps_Heading(Cs_PageX(x + sShift), Cs_PageY(y), s, scale, pageW, (s32)(128.0f * sAlpha));
    Ps_Align(0);
}

/* A texture of the game's, or a part of it: u, v in texels; the sprite in window units. */
static void Cs_Tex(MTexRes *res, f32 x0, f32 y0, f32 x1, f32 y1, s32 u0, s32 v0, s32 u1, s32 v1, s32 grey, s32 alpha) {
    CsSprite list[2];

    if (res == NULL || res->tex == NULL) {
        return;
    }
    list[0].flags = LOAD_SPR_DRAW;
    list[0].x0 = (s32)(x0 + sShift);
    list[0].y0 = (s32)y0;
    list[0].x1 = (s32)(x1 + sShift);
    list[0].y1 = (s32)y1;
    list[0].u0 = u0;
    list[0].v0 = v0;
    list[0].u1 = u1;
    list[0].v1 = v1;
    list[0].tex = 0;
    list[0].r = grey;
    list[0].g = grey;
    list[0].b = grey;
    list[0].a = alpha;
    list[1].flags = LOAD_SPR_END;
    list[1].a = 0;
    Sprite_DrawList(res, 0, 0, list);
}

/* A roster chip: the disc's chips hold their picture in 48 x 41 of their 64 x 64 (x 8..55, y 11..51). */
static void Cs_ChipAlpha(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 grey, s32 alpha) {
    Cs_Tex((MTexRes *)MPACK_AT(sV.chipPack, id + 1), x0, y0, x1, y1, 8, 11, 56, 52, grey, alpha);
}

static void Cs_Chip(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 grey) {
    Cs_ChipAlpha(id, x0, y0, x1, y1, grey, 0x80);
}

/* ---- the parts of the screen ---- */

/* reds: 0 all blue, 1 all red, 2 the rival's half red */
/* Once: the sines the round things take, kept apart from the game's state. */
enum { CS_SIDES = 32, CS_RAYS = 12, CS_RAYS2 = 20, CS_MARKS = 12 };
static PORT_HOST f32 sRoundC[CS_SIDES + 1], sRoundS[CS_SIDES + 1];
static PORT_HOST f32 sRayC[CS_RAYS][2], sRayS[CS_RAYS][2], sMarkC[CS_MARKS][2], sMarkS[CS_MARKS][2];
static PORT_HOST f32 sRay2C[CS_RAYS2][2], sRay2S[CS_RAYS2][2]; /* the two players' blend: more, thinner rays */
static PORT_HOST s32 sRoundReady;

static void Cs_RoundInit(void) {
    s32 i;

    if (sRoundReady) {
        return;
    }
    sRoundReady = 1;
    for (i = 0; i <= CS_SIDES; i++) {
        f32 a = (f32)i * (6.2831853f / CS_SIDES);

        sRoundC[i] = cosf(a);
        sRoundS[i] = sinf(a);
    }
    for (i = 0; i < CS_RAYS; i++) {
        f32 a = (f32)i * (6.2831853f / CS_RAYS);

        sRayC[i][0] = cosf(a);
        sRayS[i][0] = sinf(a);
        sRayC[i][1] = cosf(a + 0.16f);
        sRayS[i][1] = sinf(a + 0.16f);
    }
    for (i = 0; i < CS_RAYS2; i++) {
        f32 a = (f32)i * (6.2831853f / CS_RAYS2);

        sRay2C[i][0] = cosf(a);
        sRay2S[i][0] = sinf(a);
        sRay2C[i][1] = cosf(a + 0.11f);
        sRay2S[i][1] = sinf(a + 0.11f);
    }
    for (i = 0; i < CS_MARKS; i++) {
        f32 a = (f32)i * (6.2831853f / CS_MARKS);

        sMarkC[i][0] = cosf(a);
        sMarkS[i][0] = sinf(a);
        sMarkC[i][1] = cosf(a + 0.10f);
        sMarkS[i][1] = sinf(a + 0.10f);
    }
}

static void Cs_Ellipse(f32 cx, f32 cy, f32 rx, f32 ry, u32 mid, u32 edge);

/* Rays from (cx, cy), turned by the angle whose cosine and sine are tc, ts, fading out at r. */
static void Cs_Rays(s32 n, f32 (*rc)[2], f32 (*rs)[2], f32 cx, f32 cy, f32 tc, f32 ts, f32 r, u32 mid, u32 end) {
    s32 i;

    for (i = 0; i < n; i++) {
        Ps_ScreenQuad(cx, cy, cx, cy, cx + (rc[i][0] * tc - rs[i][0] * ts) * r, cy + (rs[i][0] * tc + rc[i][0] * ts) * r,
                      cx + (rc[i][1] * tc - rs[i][1] * ts) * r, cy + (rs[i][1] * tc + rc[i][1] * ts) * r, mid, mid, end, end);
    }
}

static void Cs_Backdrop(f32 cx, s32 reds) {
    f32 cy = 215.0f;
    u32 blueTop = Ps_Rgba(0x06, 0x28, 0x86, 0x80);
    u32 blueLow = Ps_Rgba(0x12, 0x68, 0xD0, 0x80);
    u32 redTop = Ps_Rgba(0x7A, 0x08, 0x10, 0x80);
    u32 redLow = Ps_Rgba(0xD4, 0x1C, 0x1C, 0x80);

    if (reds == 2) {
        /* two players: one blend, blue on the left going through violet in the middle to red on the right, no line
           between them, lighter at mid height (the colours of the user's picture) */
        static const f32 at[5] = {0.0f, 190.0f, 256.0f, 322.0f, 512.0f};
        static const f32 high[3] = {0.0f, 215.0f, 448.0f};
#define CS_RGB(r, g, b) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | 0x80000000u)
        static const u32 tone[3][5] = {
            {CS_RGB(0x22, 0x62, 0xD2), CS_RGB(0x2A, 0x6C, 0xCE), CS_RGB(0x6E, 0x64, 0xA6), CS_RGB(0xB0, 0x42, 0x5E), CS_RGB(0xC4, 0x26, 0x36)},
            {CS_RGB(0x4A, 0x9C, 0xEC), CS_RGB(0x4A, 0x92, 0xE8), CS_RGB(0x7E, 0x78, 0xBC), CS_RGB(0xC0, 0x40, 0x60), CS_RGB(0xD6, 0x36, 0x40)},
            {CS_RGB(0x18, 0x58, 0xCC), CS_RGB(0x2A, 0x62, 0xC4), CS_RGB(0x6A, 0x5C, 0x98), CS_RGB(0xB0, 0x44, 0x66), CS_RGB(0xC4, 0x22, 0x32)},
        };
#undef CS_RGB
        s32 row;
        s32 col;

        for (row = 0; row < 2; row++) {
            for (col = 0; col < 4; col++) {
                Ps_ScreenQuad(at[col], high[row], at[col + 1], high[row], at[col], high[row + 1], at[col + 1], high[row + 1],
                              Cs_A(tone[row][col]), Cs_A(tone[row][col + 1]), Cs_A(tone[row + 1][col]), Cs_A(tone[row + 1][col + 1]));
            }
        }
        Cs_Ellipse(cx, cy, 150.0f, 170.0f, Cs_A(Ps_Rgba(0xFF, 0xFF, 0xFF, 0x16)), Cs_A(Ps_Rgba(0xFF, 0xFF, 0xFF, 0x00)));
    } else {
        Cs_Rect(0.0f, 0.0f, 512.0f, 448.0f, reds == 1 ? redTop : blueTop, reds == 1 ? redLow : blueLow);
    }
    /* rays behind the fighter, turning slowly */
    {
        f32 turn = (f32)sTick * 0.0025f;
        f32 tc = cosf(turn);
        f32 ts = sinf(turn);
        f32 r = 620.0f;
        u32 mid = Ps_Rgba(0xFF, 0xFF, 0xFF, reds == 2 ? 0x10 : 0x12);
        u32 end = Ps_Rgba(0xFF, 0xFF, 0xFF, 0x00);

        /* (each table by its own call: a pointer chosen by `cond ? a : b` breaks the 32-bit pointer step of the build) */
        if (reds == 2) {
            Cs_Rays(CS_RAYS2, sRay2C, sRay2S, cx, cy, tc, ts, r, mid, end);
        } else {
            Cs_Rays(CS_RAYS, sRayC, sRayS, cx, cy, tc, ts, r, mid, end);
        }
    }
    /* (the shade at the foot: lighter on the two players' blend, which stays bright down to the edge) */
    Cs_Rect(0.0f, 340.0f, 512.0f, 448.0f, Ps_Rgba(0x04, 0x08, 0x20, 0x00), Ps_Rgba(0x04, 0x08, 0x20, reds == 2 ? 0x20 : 0x50));
}

/* A filled ellipse, the colour going from its middle to its edge. */
static void Cs_Ellipse(f32 cx, f32 cy, f32 rx, f32 ry, u32 mid, u32 edge) {
    s32 i;

    for (i = 0; i < CS_SIDES; i++) {
        Ps_ScreenQuad(cx, cy, cx, cy, cx + sRoundC[i] * rx, cy + sRoundS[i] * ry, cx + sRoundC[i + 1] * rx, cy + sRoundS[i + 1] * ry, mid, mid, edge, edge);
    }
}

/* The band between two ellipses around one middle. */
static void Cs_Ring(f32 cx, f32 cy, f32 rx0, f32 ry0, f32 rx1, f32 ry1, u32 c) {
    s32 i;

    for (i = 0; i < CS_SIDES; i++) {
        Ps_ScreenQuad(cx + sRoundC[i] * rx0, cy + sRoundS[i] * ry0, cx + sRoundC[i + 1] * rx0, cy + sRoundS[i + 1] * ry0,
                      cx + sRoundC[i] * rx1, cy + sRoundS[i] * ry1, cx + sRoundC[i + 1] * rx1, cy + sRoundS[i + 1] * ry1, c, c, c, c);
    }
}

/* The round pedestal: its side, its top, a gold rim, marks that turn with the fighter, and a soft shadow. */
static void Cs_Pedestal(s32 side) {
    f32 cx;
    f32 cy;
    f32 rx;
    f32 ry;
    f32 h;
    s32 i;

    if (!s3D || !ChrView_PortPedestal(side, &cx, &cy, &rx, &ry)) {
        return;
    }
    if (ry < rx * 0.16f) {
        ry = rx * 0.16f; /* the camera is low: seen almost from the side it would not read as a disc */
    }
    h = ry * 0.55f;
    /* a ring of light on the floor, its dark side, its top, gold and blue rims */
    if (sRed) {
        Cs_Ellipse(cx, cy + h + 4.0f, rx * 1.3f, ry * 1.3f, Ps_Rgba(0xFF, 0x50, 0x40, 0x48), Ps_Rgba(0xFF, 0x50, 0x40, 0x00));
        Cs_Ellipse(cx, cy + h, rx, ry, Ps_Rgba(0x60, 0x0A, 0x10, 0x80), Ps_Rgba(0x3C, 0x04, 0x08, 0x80));
        Ps_ScreenQuad(cx - rx, cy, cx + rx, cy, cx - rx, cy + h, cx + rx, cy + h, Ps_Rgba(0x78, 0x0C, 0x14, 0x80),
                      Ps_Rgba(0x78, 0x0C, 0x14, 0x80), Ps_Rgba(0x3C, 0x04, 0x08, 0x80), Ps_Rgba(0x3C, 0x04, 0x08, 0x80));
        Cs_Ellipse(cx, cy, rx, ry, Ps_Rgba(0xE0, 0x28, 0x24, 0x80), Ps_Rgba(0x70, 0x08, 0x10, 0x80));
    } else {
        Cs_Ellipse(cx, cy + h + 4.0f, rx * 1.3f, ry * 1.3f, Ps_Rgba(0x50, 0xC8, 0xFF, 0x48), Ps_Rgba(0x50, 0xC8, 0xFF, 0x00));
        Cs_Ellipse(cx, cy + h, rx, ry, Ps_Rgba(0x0A, 0x1E, 0x60, 0x80), Ps_Rgba(0x04, 0x10, 0x3C, 0x80));
        Ps_ScreenQuad(cx - rx, cy, cx + rx, cy, cx - rx, cy + h, cx + rx, cy + h, Ps_Rgba(0x0C, 0x28, 0x78, 0x80),
                      Ps_Rgba(0x0C, 0x28, 0x78, 0x80), Ps_Rgba(0x04, 0x10, 0x3C, 0x80), Ps_Rgba(0x04, 0x10, 0x3C, 0x80));
        Cs_Ellipse(cx, cy, rx, ry, Ps_Rgba(0x24, 0x70, 0xE0, 0x80), Ps_Rgba(0x08, 0x20, 0x70, 0x80));
    }
    Cs_Ring(cx, cy, rx, ry, rx * 0.95f, ry * 0.93f, Ps_Rgba(0xFF, 0xD0, 0x50, 0x80));
    Cs_Ring(cx, cy, rx * 0.86f, ry * 0.85f, rx * 0.83f, ry * 0.81f, sRed ? Ps_Rgba(0xFF, 0x90, 0x70, 0x80) : Ps_Rgba(0x70, 0xE0, 0xFF, 0x80));
    {
        f32 tc = cosf(sYawOf[side]);
        f32 ts = sinf(sYawOf[side]);
        u32 c = Ps_Rgba(0xFF, 0xE4, 0x90, 0x70);

        for (i = 0; i < CS_MARKS; i++) {
            f32 ac = sMarkC[i][0] * tc - sMarkS[i][0] * ts;
            f32 as = sMarkS[i][0] * tc + sMarkC[i][0] * ts;
            f32 bc = sMarkC[i][1] * tc - sMarkS[i][1] * ts;
            f32 bs = sMarkS[i][1] * tc + sMarkC[i][1] * ts;

            Ps_ScreenQuad(cx + ac * rx * 0.94f, cy + as * ry * 0.92f, cx + bc * rx * 0.94f, cy + bs * ry * 0.92f,
                          cx + ac * rx * 0.88f, cy + as * ry * 0.87f, cx + bc * rx * 0.88f, cy + bs * ry * 0.87f, c, c, c, c);
        }
    }
    if (sRed) {
        Cs_Ellipse(cx, cy, rx * 0.62f, ry * 0.62f, Ps_Rgba(0xFF, 0xA8, 0x80, 0x30), Ps_Rgba(0xFF, 0xA8, 0x80, 0x00));
    } else {
        Cs_Ellipse(cx, cy, rx * 0.62f, ry * 0.62f, Ps_Rgba(0x90, 0xE8, 0xFF, 0x30), Ps_Rgba(0x90, 0xE8, 0xFF, 0x00));
    }
}

static void Cs_Portrait(f32 middle, f32 height) {
    MTexRes *res = sV.face;
    u64 tex0;
    s32 w;
    s32 h;
    f32 across;

    if (!sV.faceReady || res == NULL || res->tex == NULL) {
        return;
    }
    tex0 = *(u64 *)(res->tex + 0x30); /* the entry's GS TEX0: its size */
    w = 1 << (s32)((tex0 >> 26) & 0xF);
    h = 1 << (s32)((tex0 >> 30) & 0xF);
    across = Cs_UnitsAcross(height) * (f32)w / (f32)h;
    Cs_Tex(res, middle - across * 0.5f, 34.0f, middle + across * 0.5f, 34.0f + height, 0, 0, w, h, 0x80, (s32)(sV.faceAlpha * 128.0f));
}

static void Cs_Grid(void) {
    s32 custom = sV.showCustom;
    CsCell *cells = custom ? sV.custom : sV.cells;
    s32 count = custom ? sV.customCount : sV.cellCount;
    s32 rows = (count + MCHR_COLS - 1) / MCHR_COLS;
    s32 col = custom ? sV.customCol : sV.col;
    s32 row = custom ? sV.customRow : sV.row;
    f32 cellW = Cs_UnitsAcross(CS_CELL_H) * 48.0f / 41.0f;
    f32 gapW = Cs_UnitsAcross(CS_GAP);
    f32 panelR = CS_GRID_X + (cellW + gapW) * MCHR_COLS + gapW;
    u32 pulse;
    s32 r;
    s32 c;

    if (row < sTopRowOf[sNow]) {
        sTopRowOf[sNow] = row;
    }
    if (row >= sTopRowOf[sNow] + CS_VIS_ROWS) {
        sTopRowOf[sNow] = row - CS_VIS_ROWS + 1;
    }
    if (sTopRowOf[sNow] > rows - CS_VIS_ROWS) {
        sTopRowOf[sNow] = rows - CS_VIS_ROWS;
    }
    if (sTopRowOf[sNow] < 0) {
        sTopRowOf[sNow] = 0;
    }
    Port_GsMarker(GS_CHIP_ON);
    Port_GsMarker(GS_CHIP_OFF);
    Cs_Glass(CS_GRID_X - gapW * 2.0f, CS_GRID_Y - CS_GAP - 2.0f, panelR + gapW * 3.0f, CS_GRID_Y + (CS_CELL_H + CS_GAP) * CS_VIS_ROWS + CS_GAP + 1.0f, 7.0f,
             CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_LINE);
    Port_GsMarker(GS_CHIP_ON);
    for (r = 0; r < CS_VIS_ROWS; r++) {
        for (c = 0; c < MCHR_COLS; c++) {
            s32 at = c + (r + sTopRowOf[sNow]) * MCHR_COLS;
            f32 x0 = CS_GRID_X + gapW + c * (cellW + gapW);
            f32 y0 = CS_GRID_Y + r * (CS_CELL_H + CS_GAP);

            if (at >= count || cells[at].id == MCHR_EMPTY) {
                continue;
            }
            Cs_Chip(cells[at].id, x0, y0, x0 + cellW, y0 + CS_CELL_H, 0x80);
        }
    }
    Port_GsMarker(GS_CHIP_OFF);
    /* the cursor over the chips, and how far down the list is */
    if (row >= sTopRowOf[sNow] && row < sTopRowOf[sNow] + CS_VIS_ROWS) {
        f32 x0 = CS_GRID_X + gapW + col * (cellW + gapW);
        f32 y0 = CS_GRID_Y + (row - sTopRowOf[sNow]) * (CS_CELL_H + CS_GAP);
        s32 glow = 0x50 + ((sTick & 31) < 16 ? (sTick & 31) : 31 - (sTick & 31)) * 3;

        pulse = Ps_Rgba(0xFF, 0xD8, 0x30, 0x80);
        Cs_Rect(x0, y0, x0 + cellW, y0 + CS_CELL_H, Ps_Rgba(0xFF, 0xE8, 0x80, glow / 4), Ps_Rgba(0xFF, 0xE8, 0x80, 0));
        Cs_Frame(x0 - 1.0f, y0 - 1.0f, x0 + cellW + 1.0f, y0 + CS_CELL_H + 1.0f, 3.0f, pulse);
    }
    if (sSplit && sV.done) {
        f32 tall = (CS_CELL_H + CS_GAP) * CS_VIS_ROWS;

        Cs_Rect(CS_GRID_X - gapW, CS_GRID_Y - CS_GAP, panelR, CS_GRID_Y + tall + CS_GAP, Ps_Rgba(0x02, 0x08, 0x20, 0x58), Ps_Rgba(0x02, 0x08, 0x20, 0x58));
        Cs_Title((CS_GRID_X + panelR) * 0.5f, CS_GRID_Y + tall * 0.5f - 24.0f, (char *)Cs_Words(W_READY), 1.2f, panelR - CS_GRID_X, 1);
    }
    if (rows > CS_VIS_ROWS) {
        f32 top = CS_GRID_Y;
        f32 tall = (CS_CELL_H + CS_GAP) * CS_VIS_ROWS;
        f32 x = panelR + gapW;
        f32 w = Cs_UnitsAcross(4.0f);

        Cs_Rect(x, top, x + w, top + tall, Ps_Rgba(0, 0, 0, 0x30), Ps_Rgba(0, 0, 0, 0x30));
        Cs_Rect(x, top + tall * sTopRowOf[sNow] / rows, x + w, top + tall * (sTopRowOf[sNow] + CS_VIS_ROWS) / rows,
                Ps_Rgba(0xFF, 0xFF, 0xFF, 0x70), Ps_Rgba(0xFF, 0xFF, 0xFF, 0x70));
    }
}

static f32 Cs_Heading(f32 x0, f32 x1, f32 y, s32 word) {
    Cs_Plate(x0, y, x1, y + 19.0f, sRed ? Ps_Rgba(0x70, 0x10, 0x14, 0x78) : Ps_Rgba(0x0C, 0x2C, 0x70, 0x78),
             sRed ? Ps_Rgba(0x4C, 0x08, 0x0C, 0x60) : Ps_Rgba(0x06, 0x1C, 0x4C, 0x60));
    Cs_Rect(x0, y, x0 + Cs_UnitsAcross(3.0f), y + 19.0f, Ps_Rgba(0xFF, 0xC8, 0x40, 0x80), Ps_Rgba(0xE0, 0x80, 0x10, 0x80));
    Cs_Text(x0 + Cs_UnitsAcross(8.0f), y - 3.0f, (char *)Cs_Words(word), 0.58f, x1 - x0 - 10.0f, 0,
            Ps_Rgba(0xFF, 0xEC, 0xA8, 0x80), Ps_Rgba(0xFF, 0xC0, 0x50, 0x80));
    return y + 23.0f;
}

/* The list of forms has two rows only where it has the room for them: six or seven on the screen of one. */
s32 PortCharSel_FormRowSize(s32 count, s32 split, s32 team) {
    return !split && !team && count >= 6 ? (count + 1) / 2 : count;
}

s32 PortCharSel_FormOtherRow(s32 count, s32 form, s32 split, s32 team) {
    s32 size = PortCharSel_FormRowSize(count, split, team);
    s32 other;

    if (size >= count || form < 0 || form >= count) {
        return -1;
    }
    other = form < size ? form + size : form - size;
    return other < count ? other : count - 1;
}

/* The height of the chips, for that many in a row, so that they fit in the room there is.
   unit: how many units across a chip is for each unit of its height; gapW: the gap across, gapH: down. */
#define CS_FORM_TALL 36.0f
static f32 Cs_FormFit(s32 n, s32 perRow, f32 width, f32 room, f32 unit, f32 gapW, f32 gapH) {
    s32 rows = (n + perRow - 1) / perRow;
    f32 byWidth = (width - gapW * (f32)(perRow - 1)) / (f32)perRow / unit;
    f32 byRoom = (room - gapH * (f32)(rows - 1)) / (f32)rows;
    f32 tall = byWidth < byRoom ? byWidth : byRoom;

    return tall < CS_FORM_TALL ? tall : CS_FORM_TALL;
}

/* One row of a list, or a cell of a row. */
static void Cs_Choice(f32 a, f32 b, f32 y, f32 tall, char *text, s32 lit, s32 active, f32 scale) {
    Cs_Plate(a, y, b, y + tall, lit ? Ps_Rgba(0xFF, 0xD0, 0x40, 0x80) : sRed ? Ps_Rgba(0xA4, 0x18, 0x1C, 0x70) : Ps_Rgba(0x16, 0x4C, 0xA4, 0x70),
             lit ? Ps_Rgba(0xE0, 0x80, 0x10, 0x80) : sRed ? Ps_Rgba(0x68, 0x0C, 0x10, 0x70) : Ps_Rgba(0x08, 0x28, 0x68, 0x70));
    if (lit && active) {
        s32 glow = (sTick & 31) < 16 ? (sTick & 31) : 31 - (sTick & 31);

        Cs_Rect(a, y, b, y + 2.0f, Ps_Rgba(255, 255, 255, 0x50 + glow * 3), Ps_Rgba(255, 255, 255, 0x50 + glow * 3));
        Cs_Rect(a, y + tall - 2.0f, b, y + tall, Ps_Rgba(255, 255, 255, 0x50 + glow * 3), Ps_Rgba(255, 255, 255, 0x50 + glow * 3));
    }
    Cs_Text((a + b) * 0.5f, y + tall * 0.5f - PS_CELL_UNITS * scale * 0.5f, text, scale, b - a - 6.0f, 1,
            Ps_Rgba(255, 255, 255, 0x80), lit ? Ps_Rgba(0xFF, 0xF4, 0xD0, 0x80) : Ps_Rgba(0xC8, 0xDC, 0xFF, 0x80));
}

static void Cs_Details(f32 x0, f32 x1, f32 y, f32 tabY) {
    s32 chara = sV.chara;
    s32 stack = sSplit || sNarrow; /* the details under the roster */
    f32 tx = stack && sNow == 0 ? x0 : x1; /* the words at the outer side of a half */
    s32 al = stack && sNow == 0 ? 0 : 2;
    f32 small = stack ? 0.8f : 1.0f;
    char name[128];
    char form[128];
    char line[64];
    s32 who;
    s32 want = sV.tabWant;

    /* the panel of who it is; then the one of what follows, as far down as it goes (the same sums as below) */
    {
        f32 pad = Cs_UnitsAcross(4.0f);
        f32 nameEnd = y + (17.0f + 35.0f + 20.0f) * small;
        f32 end = nameEnd + 6.0f + 21.0f * small;
        s32 at = sV.col + sV.row * MCHR_COLS;

        Cs_Glass(x0 - pad, y - 3.0f, x1 + pad, nameEnd + 1.0f, 6.0f, CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_LINE);
        if (sV.isTeam) {
            end += 23.0f + 34.0f + 6.0f + (sV.dpMax > 0 ? 18.0f : 0.0f);
        }
        /* (stretches as the list of forms opens, and back) */
        if (sFormOf[sNow] > 0.0f && !sV.showCustom && sV.col >= 0 && sV.row >= 0 && at < sV.cellCount) {
            CsCell *cell = &sV.cells[at];

            if (cell->formCount > 0 && cell->id != MCHR_RANDOM && cell->id != MCHR_LOCKED && cell->id != MCHR_EMPTY) {
                s32 n = cell->formCount < 7 ? cell->formCount : 7;
                s32 per = PortCharSel_FormRowSize(n, sSplit, sV.isTeam);
                f32 topY = end + 23.0f;
                f32 chip = Cs_FormFit(n, per, x1 - x0, (stack ? 444.0f : 253.0f) - topY, sAcrossPerDown * 48.0f / 41.0f, Cs_UnitsAcross(3.0f), 3.0f);
                s32 rows = (n + per - 1) / per;
                f32 open = topY + (f32)rows * chip + (f32)(rows - 1) * 3.0f + 6.0f;
                f32 t = sFormOf[sNow];

                end += (open - end) * t * t * (3.0f - 2.0f * t);
            }
        }
        Cs_Glass(x0 - pad, nameEnd + 5.0f, x1 + pad, end, 6.0f, CS_GLASS_TOP, CS_GLASS_LOW, sV.formActive ? CS_GLASS_GOLD : CS_GLASS_LINE);
    }
    who = sV.players == 2 ? W_CPU : sV.side == 0 ? W_P1 : sV.players == 0 ? W_CPU : W_P2;
    Cs_Text(tx, y, (char *)Cs_Words(who), 0.6f, x1 - x0, al, Ps_Rgba(0xD8, 0xEE, 0xFF, 0x80), Ps_Rgba(0x98, 0xCC, 0xFF, 0x80));
    y += 17.0f * small;
    name[0] = form[0] = 0;
    if (chara == MCHR_RANDOM) {
        Ps_Copy(name, Cs_Words(W_RANDOM), sizeof(name));
    } else if (chara == MCHR_LOCKED) {
        Ps_Copy(name, Cs_Words(W_LOCKED), sizeof(name));
    } else if (chara == MCHR_CUSTOM) {
        Ps_Copy(name, Cs_Words(W_SAVED), sizeof(name));
    } else if (chara >= 0 && chara < MCHR_RANDOM) {
        Cs_Line(name, sizeof(name), (u32 *)sV.nameText, chara);
        Cs_Line(form, sizeof(form), (u32 *)sV.formText, chara);
    }
    Cs_Title(tx, y, name, 1.2f * small, x1 - x0, al);
    y += 35.0f * small;
    if (form[0] != 0) {
        Cs_Text(tx, y, form, 0.62f, x1 - x0, al, Ps_Rgba(0xFF, 0xEC, 0xA8, 0x80), Ps_Rgba(0xFF, 0xC0, 0x50, 0x80));
    }
    y += 20.0f * small + 6.0f;
    if (chara >= 0 && chara < MCHR_RANDOM) {
        snprintf(line, sizeof(line), "%s: %d DP", Cs_Words(W_COST), (int)ChrTbl_GetCost(chara));
        Cs_Text(tx, y, line, 0.56f, x1 - x0, al, Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(0xD8, 0xE8, 0xFF, 0x80));
    }
    y += 21.0f * small;

    /* a team's fighters, the one being chosen lit */
    if (sV.isTeam) {
        f32 tall = 34.0f;
        f32 w = Cs_UnitsAcross(tall) * 48.0f / 41.0f;
        s32 i;

        y = Cs_Heading(x0, x1, y, W_TEAM);
        Port_GsMarker(GS_CHIP_ON);
        for (i = 0; i < CS_TEAM_MAX; i++) {
            f32 a = x0 + i * (w + Cs_UnitsAcross(3.0f));

            Cs_Rect(a, y, a + w, y + tall, Ps_Rgba(0x06, 0x1C, 0x4C, 0x60), Ps_Rgba(0x06, 0x1C, 0x4C, 0x40));
            if (sV.teamChara[i] >= 0 && sV.teamChara[i] < MCHR_RANDOM) {
                Cs_Chip(sV.teamChara[i], a, y, a + w, y + tall, 0x80);
            }
            if (i == sV.cur) {
                Cs_Frame(a - 1.0f, y - 1.0f, a + w + 1.0f, y + tall + 1.0f, 2.0f,
                         sV.teamActive ? Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80) : Ps_Rgba(0xFF, 0xD8, 0x30, 0x80));
            }
        }
        Port_GsMarker(GS_CHIP_OFF);
        y += tall + 6.0f;
        if (sV.dpMax > 0) {
            snprintf(line, sizeof(line), "DP %d / %d", (int)sV.cost, (int)sV.dpMax);
            Cs_Text(tx, y - 2.0f, line, 0.56f, x1 - x0, al, Ps_Rgba(0xFF, 0xEC, 0xA8, 0x80), Ps_Rgba(0xFF, 0xC0, 0x50, 0x80));
            y += 18.0f;
        }
    }

    /* The forms of the cell under the cursor: only once it is chosen (the game's own step, with its sound), opening
       from the first chip. Their room is kept while closed, so nothing below moves as they open. */
    {
        CsCell *cell = NULL;
        s32 at = sV.col + sV.row * MCHR_COLS;
        s32 open;

        if (!sV.showCustom && sV.col >= 0 && sV.row >= 0 && at < sV.cellCount) {
            cell = &sV.cells[at];
            if (cell->formCount <= 0 || cell->id == MCHR_RANDOM || cell->id == MCHR_LOCKED || cell->id == MCHR_EMPTY) {
                cell = NULL;
            }
        }
        /* (two teams on one screen have no room for it but in its own step) */
        open = cell != NULL && (sV.formActive || (!(stack && sV.isTeam) && (sV.tabWant || sV.done)));
        if (cell == NULL || (!open && at != sFormCell[sNow])) {
            sFormOf[sNow] = 0.0f; /* the list closing is not to show another fighter's forms */
        }
        sFormCell[sNow] = at;
        sFormOf[sNow] += open ? 0.16f : -0.25f;
        if (sFormOf[sNow] < 0.0f) {
            sFormOf[sNow] = 0.0f;
        } else if (sFormOf[sNow] > 1.0f) {
            sFormOf[sNow] = 1.0f;
        }
        if (cell != NULL && !(stack && sV.isTeam && !open && sFormOf[sNow] <= 0.0f)) {
            s32 n = cell->formCount < 7 ? cell->formCount : 7;
            f32 gapW = Cs_UnitsAcross(3.0f);
            f32 t = sFormOf[sNow];
            s32 perRow;
            s32 rows;
            s32 i;
            f32 tall;
            f32 w;
            f32 top = y + 23.0f;

            /* (over a whole screen the tab comes under the list and has to end on the screen too) */
            perRow = PortCharSel_FormRowSize(n, sSplit, sV.isTeam);
            tall = Cs_FormFit(n, perRow, x1 - x0, (stack ? 444.0f : 253.0f) - top, Cs_UnitsAcross(1.0f) * 48.0f / 41.0f, gapW, 3.0f);
            w = Cs_UnitsAcross(tall) * 48.0f / 41.0f;
            rows = (n + perRow - 1) / perRow;
            if (t > 0.0f) {
                sAlpha = t * t * (3.0f - 2.0f * t);
                Cs_Heading(x0, x1, y, W_FORMS);
                Port_GsMarker(GS_CHIP_ON);
                for (i = 0; i < n; i++) {
                    /* one after the other */
                    f32 p = (t * (f32)(n + 2) - (f32)i) * 0.5f;
                    f32 a;
                    f32 b = top + (f32)(i / perRow) * (tall + 3.0f);

                    if (p <= 0.0f) {
                        continue;
                    }
                    p = p > 1.0f ? 1.0f : p * p * (3.0f - 2.0f * p);
                    a = x0 + (f32)(i % perRow) * (w + gapW) * p;
                    Cs_ChipAlpha(cell->form[i], a, b, a + w, b + tall, cell->form[i] == chara ? 0x80 : 0x50, (s32)(128.0f * p));
                    if (cell->form[i] == chara) {
                        sAlpha = p;
                        Cs_Frame(a - 1.0f, b - 1.0f, a + w + 1.0f, b + tall + 1.0f, 2.0f,
                                 sV.formActive ? Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80) : Ps_Rgba(0xFF, 0xD8, 0x30, 0x80));
                        sAlpha = t * t * (3.0f - 2.0f * t);
                    }
                }
                Port_GsMarker(GS_CHIP_OFF);
                sAlpha = 1.0f;
            }
            y = top + (f32)rows * tall + (f32)(rows - 1) * 3.0f + 8.0f;
        }
    }

    /* The tab of what the step asks for: it comes in from the screen's edge when the fighter is chosen and goes back
       with the triangle. The item sets are chosen with up and down, as a list; the costume with left and right. */
    sTabOf[sNow] += ((want ? 1.0f : 0.0f) - sTabOf[sNow]) * 0.2f;
    if (sTabOf[sNow] > 0.01f) {
        f32 t = sTabOf[sNow];
        f32 e = t * t * (3.0f - 2.0f * t);
        s32 i;

        if (tabY > 0.0f) {
            y = tabY; /* (in a half: over the roster) */
        }
        sShift = (1.0f - e) * (x1 - x0 + 14.0f) * (stack && sNow == 0 ? -1.0f : 1.0f);
        sAlpha = e;
        Cs_Glass(x0 - Cs_UnitsAcross(4.0f), y - 4.0f, x1 + Cs_UnitsAcross(4.0f), y + 188.0f, 6.0f, CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_GOLD);
        y = Cs_Heading(x0, x1, y, W_CUSTOM);
        for (i = 0; i < sV.plateCount && i < 4; i++) {
            Cs_Choice(x0, x1, y, 25.0f, (char *)Cs_Words(W_NORMAL + i), i == sV.plate, sV.plateActive, 0.56f);
            y += 27.0f;
        }
        y += 3.0f;
        y = Cs_Heading(x0, x1, y, W_COSTUME);
        for (i = 0; i < sV.colorCount && i < 4; i++) {
            f32 w = (x1 - x0) / (f32)sV.colorCount;
            char digit[2];

            digit[0] = (char)('1' + i);
            digit[1] = 0;
            Cs_Choice(x0 + w * i, x0 + w * (i + 1) - Cs_UnitsAcross(3.0f), y, 28.0f, digit, i == sV.color, sV.colorActive, 0.72f);
        }
        sShift = 0.0f;
        sAlpha = 1.0f;
    }
}

/* The buttons and their words, all on the middle line of their box. */
static void Cs_Foot(f32 at) {
    f32 mid = 426.0f;
    f32 y = Cs_PageY(mid);
    f32 x = Cs_PageX(at);
    f32 step = Ps_X(82.0f) - Ps_X(0.0f);
    u32 top = Ps_Rgba(255, 255, 255, 0x80);
    u32 bottom = Ps_Rgba(0xD8, 0xE8, 0xFF, 0x80);
    f32 textY = mid - PS_CELL_UNITS * 0.6f * 0.5f;
    f32 word = at + Cs_UnitsAcross(26.0f);
    f32 right = word + step * 2.1f + Ps_TextWidth((char *)Cs_Words(W_ROTATE), 0.52f) * (Ps_X(1.0f) - Ps_X(0.0f));
    s32 rotate = s3D && !sSplit && right + Cs_UnitsAcross(12.0f) < 508.0f;

    if (rotate) {
        f32 smallY = mid - PS_CELL_UNITS * 0.52f * 0.5f;

        Cs_Glass(at - Cs_UnitsAcross(10.0f), 409.0f, right + Cs_UnitsAcross(12.0f), 443.0f, 12.0f, CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_GOLD);
        Cs_Text(word + step * 2.1f, smallY, (char *)Cs_Words(W_ROTATE), 0.52f, 0.0f, 0, top, bottom);
    } else {
        right = word + step + Ps_TextWidth((char *)Cs_Words(W_BACK), 0.6f) * (Ps_X(1.0f) - Ps_X(0.0f));
        Cs_Glass(at - Cs_UnitsAcross(10.0f), 409.0f, right + Cs_UnitsAcross(12.0f), 443.0f, 12.0f, CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_GOLD);
    }
    Ps_Button(0, x + 8.0f, y, 22.0f);
    Cs_Text(word, textY, (char *)Cs_Words(W_OK), 0.6f, 0.0f, 0, top, bottom);
    Ps_Button(2, x + 90.0f, y, 22.0f);
    Cs_Text(word + step, textY, (char *)Cs_Words(W_BACK), 0.6f, 0.0f, 0, top, bottom);
}

/* The items of a set, in the screen's own look (the panel's movie still runs behind, not shown: its input is the
   game's). */
static void Cs_Items(s32 side) {
    ItemPanel *p = gItemPanel[side];
    s32 want = (p->flags & ITEMPANEL_SHOWN) && !(p->flags & ITEMPANEL_CLOSING);
    f32 x1 = sSplit && side == 0 ? 250.0f : 502.0f;
    f32 x0 = x1 - Cs_UnitsAcross(sSplit ? 235.0f : 300.0f);
    f32 y = 48.0f;
    f32 e;
    s32 i;
    char line[96];

    sItemOf[side] += ((want ? 1.0f : 0.0f) - sItemOf[side]) * 0.2f;
    if (sItemOf[side] <= 0.01f) {
        return;
    }
    e = sItemOf[side] * sItemOf[side] * (3.0f - 2.0f * sItemOf[side]);
    sAlpha = e;
    if (sSplit) {
        Cs_Rect(side ? 256.0f : 0.0f, 0.0f, side ? 512.0f : 256.0f, 448.0f, Ps_Rgba(0x02, 0x08, 0x20, 0x48), Ps_Rgba(0x02, 0x08, 0x20, 0x58));
    } else {
        Cs_Rect(0.0f, 0.0f, 512.0f, 448.0f, Ps_Rgba(0x02, 0x08, 0x20, 0x48), Ps_Rgba(0x02, 0x08, 0x20, 0x58));
    }
    sShift = (1.0f - e) * (x1 - x0 + 14.0f) * (sSplit && side == 0 ? -1.0f : 1.0f);
    Cs_Glass(x0 - Cs_UnitsAcross(6.0f), y - 8.0f, x1 + Cs_UnitsAcross(6.0f), y + 352.0f, 8.0f, Ps_Rgba(0x12, 0x40, 0xA8, 0x78), Ps_Rgba(0x04, 0x16, 0x58, 0x7C), CS_GLASS_LINE);
    Cs_Title(x0, y - 8.0f, (char *)Cs_Words(W_NORMAL + (p->set >= 0 && p->set <= 3 ? p->set : 0)), 0.8f, (x1 - x0) * 0.5f, 0);
    /* how many of the slots the items take */
    snprintf(line, sizeof(line), "%d / %d", (int)p->used, (int)p->limit);
    Cs_Text(x1, y - 6.0f, line, 0.7f, 0.0f, 2, Ps_Rgba(0xFF, 0xEC, 0xA8, 0x80), Ps_Rgba(0xFF, 0xC0, 0x50, 0x80));
    y += 24.0f;
    for (i = 0; i < ITEMPANEL_ROWS; i++) {
        s32 id = p->item.id[i];
        s32 lit = p->cursor == i;
        s32 usable = i < p->count || i == ITEMPANEL_BACK;

        line[0] = 0;
        if (i == ITEMPANEL_BACK) {
            Ps_Copy(line, Cs_Words(W_BACK), sizeof(line));
        } else if (id != 0) {
            Cs_Line(line, sizeof(line), (u32 *)p->text[2], id - 1);
        } else if (usable) {
            Ps_Copy(line, Cs_Words(W_EMPTY), sizeof(line));
        }
        if (!usable) {
            f32 keep = sAlpha;

            sAlpha = keep * 0.4f;
            Cs_Choice(x0, x1, y, 34.0f, line, 0, 0, 0.56f);
            sAlpha = keep;
        } else {
            Cs_Choice(x0, x1, y, 34.0f, line, lit, lit, 0.56f);
        }
        /* its cost in slots, as pips */
        if (i != ITEMPANEL_BACK && id != 0) {
            s32 cost = p->items[id - 1].slots;
            s32 n;

            for (n = 0; n < cost && n < 7; n++) {
                f32 px = x1 - Cs_UnitsAcross(22.0f) - n * Cs_UnitsAcross(9.0f);

                Cs_Rect(px, y + 13.0f, px + Cs_UnitsAcross(6.0f), y + 21.0f, Ps_Rgba(0xFF, 0xE4, 0x90, 0x80), Ps_Rgba(0xE0, 0x90, 0x20, 0x80));
            }
        }
        y += 36.0f;
    }
    sShift = 0.0f;
    sAlpha = 1.0f;
}

/* ---- what menu_c_e.c calls ---- */

/* Right after the screen's Init. allow: this game and this moment take the new screen at all (not a fight's practice
   roster, which runs inside a fight on every peer). */
void PortCharSel_Begin(s32 allow) {
    const char *env = getenv("BT3_SELECT_CLASSIC");
    s32 i;

    sOn = allow && !(env != NULL ? atoi(env) : Port_Setting("select_classic", 0));
    s3D = sOn && Port_WorldCan();
    sTick = 0;
    sSplit = 0;
    sInScene = 0;
    for (i = 0; i < 2; i++) {
        sTopRowOf[i] = 0;
        sWantChara[i] = sWantCostume[i] = sShownChara[i] = sShownCostume[i] = -1;
        sRest[i] = 0;
        sYawOf[i] = 0.0f;
        sTabOf[i] = 0.0f;
        sItemOf[i] = 0.0f;
        sFormOf[i] = 0.0f;
        sFormCell[i] = -1;
    }
}

/* Right before the screen's Term (and before the battle is set up). */
void PortCharSel_End(void) {
    if (sInScene) {
        Port_MenuScene3D(0);
        sInScene = 0;
    }
    if (s3D) {
        ChrView_PortEnd();
    }
    s3D = 0;
    sOn = 0;
    sSplit = 0;
}

/* A session was left with the screen up: the game's state goes back to before it (Port_SessionReturn), past the End
   of the screen, and what is here is the machine's, not put back with it. Only that is cleared: the state that
   comes back has its own idea of the 3D view. */
void PortCharSel_Abandon(void) {
    if (s3D) {
        ChrView_PortEnd();
    }
    s3D = 0;
    sOn = 0;
    sSplit = 0;
    sInScene = 0;
}

/* Non-zero: the screen's Draw shows this one instead of its own (asked with the screen in its stage choice or leaving). */
s32 PortCharSel_Shown(s32 stagePhase) {
    return sOn && !stagePhase;
}

void PortCharSel_MuteOn(void) {
    Port_GsMarker(GS_MUTE_ON);
}

/* Once a frame, before the screen is drawn: what it shows now, which models are wanted, and the 3D view on or off. */
void PortCharSel_Frame(const CsView *view, s32 split, s32 stagePhase) {
    s32 scene = s3D && PortCharSel_Shown(stagePhase);
    s32 i;

    sSplit = split != 0;
    sViews[0] = view[0];
    if (sSplit) {
        sViews[1] = view[1];
    }
    sV = sViews[0];
    sNow = 0;
    sTick++;
    sInScene = scene;
    Port_MenuScene3D(scene);
    if (!scene) {
        return;
    }
    for (i = 0; i < (sSplit ? 2 : 1); i++) {
        if (sViews[i].chara != sWantChara[i] || sViews[i].color != sWantCostume[i]) {
            sWantChara[i] = sViews[i].chara;
            sWantCostume[i] = sViews[i].color;
            sRest[i] = 0;
        } else if (sRest[i] < CS_WAIT) {
            sRest[i]++;
        }
        if (sRest[i] >= CS_WAIT) {
            sShownChara[i] = sWantChara[i];
            /* (the cursor keeps the last fighter's costume: a number this one may not have, whose file is empty) */
            sShownCostume[i] = sWantCostume[i];
            if (sWantChara[i] >= 0 && sWantChara[i] < MCHR_RANDOM) {
                ChrTbl_WrapCostume(sWantChara[i], &sShownCostume[i]);
            } else {
                sShownChara[i] = -1;
            }
        }
    }
}

static f32 Cs_GridWidth(void) {
    return (Cs_UnitsAcross(CS_CELL_H) * 48.0f / 41.0f + Cs_UnitsAcross(CS_GAP)) * MCHR_COLS + Cs_UnitsAcross(CS_GAP);
}

/* The roster's measures: beside the fighter on a wide window; on a narrower one, or in a half, smaller with the details
   under it, and as small as it takes to leave a fighter its room (a chip is 48 across for 41 down). */
static void Cs_Place(s32 side) {
    if (sSplit || sNarrow) {
        f32 room = sSplit ? 150.0f : 235.0f; /* across, in the window's units */
        f32 tall = ((room / sAcrossPerDown - 2.0f) / (f32)MCHR_COLS - 2.0f) * 41.0f / 48.0f;

        CS_CELL_H = tall < 31.0f ? tall : 31.0f;
        if (!sSplit && !sV.isTeam && tall > 36.0f) {
            CS_CELL_H = 36.0f; /* (alone and with no team to show, there is room under it for more) */
        } else if (!sSplit && !sV.isTeam && tall > 31.0f) {
            CS_CELL_H = tall;
        }
        CS_GAP = 2.0f;
        CS_GRID_Y = 34.0f;
        CS_GRID_X = sSplit && side != 0 ? 506.0f - Cs_GridWidth() : 6.0f;
    } else {
        CS_CELL_H = 47.0f;
        CS_GAP = 3.0f;
        CS_GRID_Y = 62.0f;
        CS_GRID_X = 10.0f;
    }
}

/* In the screen's Draw's place. */
void PortCharSel_Draw(void) {
    f32 middle[2];
    s32 sides;
    s32 i;

    Port_GsMarker(GS_MUTE_OFF);
    Port_GsMarker(GS_WHOLE_ON);
    Cs_MeasureWindow();
    Cs_RoundInit();
    /* (Always drawn, also on a blank run again: the game's own picture is off, and the list of the last such blank is
       shown. Left out it was a black picture at every rollback. The renderer discards the others.) */
    Ps_PageSet();
    sides = sSplit ? 2 : 1;
    /* each fighter in the middle of the room next to its roster */
    if (sSplit) {
        Cs_Place(0);
        middle[0] = (CS_GRID_X + Cs_GridWidth() + 256.0f) * 0.5f;
        Cs_Place(1);
        middle[1] = (256.0f + CS_GRID_X) * 0.5f;
    } else if (sNarrow) {
        Cs_Place(0);
        middle[0] = (CS_GRID_X + Cs_GridWidth() + 512.0f) * 0.5f;
        middle[1] = 0.0f;
    } else {
        Cs_Place(0);
        middle[0] = (CS_GRID_X + Cs_GridWidth() + 502.0f - Cs_UnitsAcross(205.0f)) * 0.5f;
        middle[1] = 0.0f;
    }
    Cs_Backdrop(sSplit ? 256.0f : middle[0], sSplit ? 2 : sViews[0].side == 1 ? 1 : 0);
    if (s3D) {
        for (i = 0; i < sides; i++) {
            f32 turn = gPad[sSplit || Port_NetActive() || Port_NetSession() ? sViews[i].side : 0].gameRight[0];

            if (turn > 0.1f || turn < -0.1f) {
                sYawOf[i] += turn * 0.06f;
                /* (the game's rotation holds only from -pi to pi: past that its matrix is no rotation at all, and the
                   fighter came out huge and bent) */
                if (sYawOf[i] > 3.14159265f) {
                    sYawOf[i] -= 6.2831853f;
                } else if (sYawOf[i] < -3.14159265f) {
                    sYawOf[i] += 6.2831853f;
                }
            }
            sRed = sViews[i].side == 1;
            Cs_Pedestal(i);
            sRed = 0;
        }
        if (Port_NetListShown()) {
            ChrView_PortFrame(middle, sYawOf, sShownChara, sShownCostume, sSplit);
        }
    }
    /* (nothing of the viewer's 3D yet, a fighter with no model of its own, a marker: the portrait stands in) */
    for (i = 0; i < sides; i++) {
        if (!s3D || !ChrView_PortVisible(i)) {
            sV = sViews[i];
            Cs_Portrait(middle[i], sSplit ? 230.0f : 330.0f);
        }
    }
    if (!sSplit && !sNarrow) {
        Cs_Title(CS_GRID_X, 18.0f, (char *)Cs_Words(W_CHOOSE), 0.95f, 0.0f, 0);
    }
    for (i = 0; i < sides; i++) {
        sNow = i;
        sV = sViews[i];
        sRed = sV.side == 1;
        Cs_Place(i);
        Cs_Grid();
        if (sSplit || sNarrow) {
            f32 w = Cs_GridWidth();

            Cs_Details(CS_GRID_X, CS_GRID_X + w, CS_GRID_Y + (CS_CELL_H + CS_GAP) * CS_VIS_ROWS + 4.0f, CS_GRID_Y);
        } else {
            Cs_Details(502.0f - Cs_UnitsAcross(205.0f), 502.0f, 20.0f, 0.0f);
        }
    }
    sRed = 0;
    sNow = 0;
    sV = sViews[0];
    if (sSplit) {
        Cs_Foot(256.0f - (Ps_X(82.0f) - Ps_X(0.0f)) - 6.0f);
    } else if (sNarrow) {
        Cs_Place(0);
        Cs_Foot(CS_GRID_X + Cs_GridWidth() + Cs_UnitsAcross(16.0f));
    } else {
        Cs_Foot(14.0f);
    }
    Font_FlushAll();
}

/* The item panel over everything (after the game's panel's movie, which is not shown). */
void PortCharSel_DrawItems(void) {
    s32 i;

    Port_GsMarker(GS_MUTE_OFF);
    Cs_MeasureWindow();
    Ps_PageSet();
    for (i = 0; i < (sSplit ? 2 : 1); i++) {
        sRed = sViews[i].side == 1;
        Cs_Items(sViews[i].side);
    }
    sRed = 0;
    Font_FlushAll();
}

void PortCharSel_DrawEnd(void) {
    Port_GsMarker(GS_WHOLE_OFF);
}
#endif
