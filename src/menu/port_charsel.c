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

static f32 Cs_UnitsAcross(f32 unitsDown) { /* so many units across are as long as this many down */
    return unitsDown * (512.0f / 448.0f) * 1000.0f / (f32)Port_PageAspectMilli();
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
static void Cs_Chip(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 grey) {
    Cs_Tex((MTexRes *)MPACK_AT(sV.chipPack, id + 1), x0, y0, x1, y1, 8, 11, 56, 52, grey, 0x80);
}

/* ---- the parts of the screen ---- */

static void Cs_Backdrop(f32 cx) {
    s32 i;
    f32 cy = 215.0f;

    Cs_Rect(0.0f, 0.0f, 512.0f, 448.0f, Ps_Rgba(0x10, 0x4A, 0xB0, 0x80), Ps_Rgba(0x3F, 0xB4, 0xF4, 0x80));
    /* rays behind the fighter, turning slowly */
    for (i = 0; i < 12; i++) {
        f32 a = (f32)sTick * 0.0025f + (f32)i * (6.2831853f / 12.0f);
        f32 b = a + 0.16f;
        f32 r = 620.0f;
        u32 mid = Ps_Rgba(0xFF, 0xFF, 0xFF, 0x12);
        u32 end = Ps_Rgba(0xFF, 0xFF, 0xFF, 0x00);

        Ps_ScreenQuad(cx, cy, cx, cy, cx + cosf(a) * r, cy + sinf(a) * r, cx + cosf(b) * r, cy + sinf(b) * r, mid, mid, end, end);
    }
    Cs_Rect(0.0f, 340.0f, 512.0f, 448.0f, Ps_Rgba(0x06, 0x24, 0x60, 0x00), Ps_Rgba(0x06, 0x24, 0x60, 0x50));
}

/* A filled ellipse, the colour going from its middle to its edge. */
static void Cs_Ellipse(f32 cx, f32 cy, f32 rx, f32 ry, u32 mid, u32 edge) {
    enum { SIDES = 40 };
    s32 i;

    for (i = 0; i < SIDES; i++) {
        f32 a = (f32)i * (6.2831853f / SIDES);
        f32 b = (f32)(i + 1) * (6.2831853f / SIDES);

        Ps_ScreenQuad(cx, cy, cx, cy, cx + cosf(a) * rx, cy + sinf(a) * ry, cx + cosf(b) * rx, cy + sinf(b) * ry, mid, mid, edge, edge);
    }
}

/* The band between two ellipses around one middle. */
static void Cs_Ring(f32 cx, f32 cy, f32 rx0, f32 ry0, f32 rx1, f32 ry1, u32 c) {
    enum { SIDES = 40 };
    s32 i;

    for (i = 0; i < SIDES; i++) {
        f32 a = (f32)i * (6.2831853f / SIDES);
        f32 b = (f32)(i + 1) * (6.2831853f / SIDES);

        Ps_ScreenQuad(cx + cosf(a) * rx0, cy + sinf(a) * ry0, cx + cosf(b) * rx0, cy + sinf(b) * ry0,
                      cx + cosf(a) * rx1, cy + sinf(a) * ry1, cx + cosf(b) * rx1, cy + sinf(b) * ry1, c, c, c, c);
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
    Cs_Ellipse(cx, cy + h + 6.0f, rx * 1.18f, ry * 1.18f, Ps_Rgba(0x02, 0x10, 0x38, 0x40), Ps_Rgba(0x02, 0x10, 0x38, 0x00));
    Cs_Ellipse(cx, cy + h, rx, ry, Ps_Rgba(0x10, 0x2C, 0x68, 0x80), Ps_Rgba(0x08, 0x18, 0x40, 0x80));
    Ps_ScreenQuad(cx - rx, cy, cx + rx, cy, cx - rx, cy + h, cx + rx, cy + h, Ps_Rgba(0x18, 0x3C, 0x80, 0x80),
                  Ps_Rgba(0x10, 0x2C, 0x68, 0x80), Ps_Rgba(0x0C, 0x20, 0x50, 0x80), Ps_Rgba(0x08, 0x18, 0x40, 0x80));
    Cs_Ellipse(cx, cy, rx, ry, Ps_Rgba(0x5A, 0x8E, 0xD8, 0x80), Ps_Rgba(0x24, 0x54, 0xA8, 0x80));
    Cs_Ring(cx, cy, rx, ry, rx * 0.95f, ry * 0.93f, Ps_Rgba(0xF0, 0xC0, 0x48, 0x80));
    Cs_Ring(cx, cy, rx * 0.76f, ry * 0.76f, rx * 0.74f, ry * 0.74f, Ps_Rgba(0xFF, 0xF0, 0xC0, 0x50));
    for (i = 0; i < 12; i++) {
        f32 a = sYawOf[side] + (f32)i * (6.2831853f / 12.0f);
        f32 b = a + 0.10f;
        u32 c = Ps_Rgba(0xFF, 0xE4, 0x90, 0x70);

        Ps_ScreenQuad(cx + cosf(a) * rx * 0.90f, cy + sinf(a) * ry * 0.88f, cx + cosf(b) * rx * 0.90f, cy + sinf(b) * ry * 0.88f,
                      cx + cosf(a) * rx * 0.80f, cy + sinf(a) * ry * 0.79f, cx + cosf(b) * rx * 0.80f, cy + sinf(b) * ry * 0.79f, c, c, c, c);
    }
    Cs_Ellipse(cx, cy, rx * 0.62f, ry * 0.62f, Ps_Rgba(0x04, 0x10, 0x30, 0x48), Ps_Rgba(0x04, 0x10, 0x30, 0x00));
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
    Cs_Rect(CS_GRID_X - gapW, CS_GRID_Y - CS_GAP, panelR, CS_GRID_Y + (CS_CELL_H + CS_GAP) * CS_VIS_ROWS + CS_GAP,
            Ps_Rgba(0x06, 0x1C, 0x4C, 0x60), Ps_Rgba(0x06, 0x1C, 0x4C, 0x48));
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
    Cs_Plate(x0, y, x1, y + 19.0f, Ps_Rgba(0x0C, 0x2C, 0x70, 0x78), Ps_Rgba(0x06, 0x1C, 0x4C, 0x60));
    Cs_Rect(x0, y, x0 + Cs_UnitsAcross(3.0f), y + 19.0f, Ps_Rgba(0xFF, 0xC8, 0x40, 0x80), Ps_Rgba(0xE0, 0x80, 0x10, 0x80));
    Cs_Text(x0 + Cs_UnitsAcross(8.0f), y - 3.0f, (char *)Cs_Words(word), 0.58f, x1 - x0 - 10.0f, 0,
            Ps_Rgba(0xFF, 0xEC, 0xA8, 0x80), Ps_Rgba(0xFF, 0xC0, 0x50, 0x80));
    return y + 23.0f;
}

/* One row of a list, or a cell of a row. */
static void Cs_Choice(f32 a, f32 b, f32 y, f32 tall, char *text, s32 lit, s32 active, f32 scale) {
    Cs_Plate(a, y, b, y + tall, lit ? Ps_Rgba(0xFF, 0xD0, 0x40, 0x80) : Ps_Rgba(0x16, 0x4C, 0xA4, 0x70),
             lit ? Ps_Rgba(0xE0, 0x80, 0x10, 0x80) : Ps_Rgba(0x08, 0x28, 0x68, 0x70));
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
    f32 tx = sSplit && sNow == 0 ? x0 : x1; /* the words at the outer side of a half */
    s32 al = sSplit && sNow == 0 ? 0 : 2;
    f32 small = sSplit ? 0.8f : 1.0f;
    char name[128];
    char form[128];
    char line[64];
    s32 who;
    s32 want = sV.tabWant;

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
    y += 20.0f * small;
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

    /* the forms of the cell under the cursor */
    if (!(sSplit && sV.isTeam && !sV.formActive) && !sV.showCustom && sV.col >= 0 && sV.row >= 0 &&
        sV.col + sV.row * MCHR_COLS < sV.cellCount) {
        CsCell *cell = &sV.cells[sV.col + sV.row * MCHR_COLS];

        if (cell->formCount > 0 && cell->id != MCHR_RANDOM && cell->id != MCHR_LOCKED && cell->id != MCHR_EMPTY) {
            f32 tall = 36.0f;
            f32 w = Cs_UnitsAcross(tall) * 48.0f / 41.0f;
            s32 i;

            y = Cs_Heading(x0, x1, y, W_FORMS);
            Port_GsMarker(GS_CHIP_ON);
            for (i = 0; i < cell->formCount && i < 7; i++) {
                f32 a = x0 + i * (w + Cs_UnitsAcross(3.0f));

                Cs_Chip(cell->form[i], a, y, a + w, y + tall, cell->form[i] == chara ? 0x80 : 0x50);
                if (cell->form[i] == chara) {
                    Cs_Frame(a - 1.0f, y - 1.0f, a + w + 1.0f, y + tall + 1.0f, 2.0f,
                             sV.formActive ? Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80) : Ps_Rgba(0xFF, 0xD8, 0x30, 0x80));
                }
            }
            Port_GsMarker(GS_CHIP_OFF);
            y += tall + 8.0f;
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
        sShift = (1.0f - e) * (x1 - x0 + 14.0f) * (sSplit && sNow == 0 ? -1.0f : 1.0f);
        sAlpha = e;
        Cs_Rect(x0 - Cs_UnitsAcross(4.0f), y - 4.0f, x1 + Cs_UnitsAcross(4.0f), y + 188.0f, Ps_Rgba(0x06, 0x1C, 0x4C, 0x58), Ps_Rgba(0x06, 0x1C, 0x4C, 0x30));
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

static void Cs_Foot(f32 at) {
    f32 y = Cs_PageY(428.0f);
    f32 x = Cs_PageX(at);
    f32 step = Ps_X(82.0f) - Ps_X(0.0f);
    u32 top = Ps_Rgba(255, 255, 255, 0x80);
    u32 bottom = Ps_Rgba(0xD8, 0xE8, 0xFF, 0x80);

    Ps_Button(0, x + 8.0f, y, 22.0f);
    Cs_Text(at + Cs_UnitsAcross(26.0f), 414.0f, (char *)Cs_Words(W_OK), 0.6f, 0.0f, 0, top, bottom);
    Ps_Button(2, x + 90.0f, y, 22.0f);
    Cs_Text(at + Cs_UnitsAcross(26.0f) + step, 414.0f, (char *)Cs_Words(W_BACK), 0.6f, 0.0f, 0, top, bottom);
    if (s3D && !sSplit) {
        Cs_Text(at + Cs_UnitsAcross(26.0f) + step * 2.1f, 414.0f, (char *)Cs_Words(W_ROTATE), 0.52f, 0.0f, 0, top, bottom);
    }
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
    Cs_Rect(x0 - Cs_UnitsAcross(6.0f), y - 8.0f, x1 + Cs_UnitsAcross(6.0f), y + 352.0f, Ps_Rgba(0x0C, 0x2C, 0x70, 0x78), Ps_Rgba(0x06, 0x1C, 0x4C, 0x78));
    Cs_Rect(x0 - Cs_UnitsAcross(6.0f), y - 8.0f, x1 + Cs_UnitsAcross(6.0f), y - 6.0f, Ps_Rgba(0xFF, 0xC8, 0x40, 0x80), Ps_Rgba(0xE0, 0x80, 0x10, 0x80));
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

/* The roster's measures: the whole screen's, or a half's. */
static void Cs_Place(s32 side) {
    if (sSplit) {
        CS_CELL_H = 31.0f;
        CS_GAP = 2.0f;
        CS_GRID_Y = 34.0f;
        CS_GRID_X = side == 0 ? 6.0f : 506.0f - Cs_GridWidth();
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
    } else {
        Cs_Place(0);
        middle[0] = (CS_GRID_X + Cs_GridWidth() + 502.0f - Cs_UnitsAcross(205.0f)) * 0.5f;
        middle[1] = 0.0f;
    }
    Cs_Backdrop(sSplit ? 256.0f : middle[0]);
    if (sSplit) {
        Cs_Rect(255.0f, 0.0f, 257.0f, 448.0f, Ps_Rgba(0xFF, 0xD8, 0x60, 0x70), Ps_Rgba(0xFF, 0xD8, 0x60, 0x10));
    }
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
            Cs_Pedestal(i);
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
    if (!sSplit) {
        Cs_Title(CS_GRID_X, 18.0f, (char *)Cs_Words(W_CHOOSE), 0.95f, 0.0f, 0);
    }
    for (i = 0; i < sides; i++) {
        sNow = i;
        sV = sViews[i];
        Cs_Place(i);
        Cs_Grid();
        if (sSplit) {
            f32 w = Cs_GridWidth();

            Cs_Details(CS_GRID_X, CS_GRID_X + w, CS_GRID_Y + (CS_CELL_H + CS_GAP) * CS_VIS_ROWS + 4.0f, CS_GRID_Y);
        } else {
            Cs_Details(502.0f - Cs_UnitsAcross(205.0f), 502.0f, 20.0f, 0.0f);
        }
    }
    sNow = 0;
    sV = sViews[0];
    Cs_Foot(sSplit ? 256.0f - (Ps_X(82.0f) - Ps_X(0.0f)) - 6.0f : 14.0f);
    Font_FlushAll();
}

/* The item panel over everything (after the game's panel's movie, which is not shown). */
void PortCharSel_DrawItems(void) {
    s32 i;

    Port_GsMarker(GS_MUTE_OFF);
    Ps_PageSet();
    for (i = 0; i < (sSplit ? 2 : 1); i++) {
        Cs_Items(sViews[i].side);
    }
    Font_FlushAll();
}

void PortCharSel_DrawEnd(void) {
    Port_GsMarker(GS_WHOLE_OFF);
}
#endif
