#include "common.h"
#include "menu/menu_a.h"
#include "menu/port_charsel.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "menu/menu_g.h"
#include "sys/pad.h"
#include "port_variant.h"

#ifdef PORT
/*
 * The character select drawn as a roster on the left, the fighter in the middle and its details on the right.
 *
 * Both selects (one on one, menu_c_e.c; teams, menu_e_b.c) describe themselves in a CsView each frame. Only the drawing is new. Their Input, Update and Draw still run as they always did, so the cursors,
 * the forms, the costumes, the item sets and what goes to the battle are the game's own. What CharSel_Draw puts in
 * the frame is left out by two markers in the display list (Port_GsMarker 0x60, 0x61: textures are still uploaded,
 * nothing is drawn). The stage and music choice is the game's screen as it is.
 *
 * The fighter is the character reference's viewer, in a world of its own (src/battle/view_a_d.c): in every mode, online
 * too, it leaves the game's state as it found it. While none is up the portrait the game loads anyway stands in.
 *
 * The chips are the disc's 48 x 41: the renderer enlarges them when it decodes them (Scale2x twice, markers 0x64, 0x65).
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
extern s32 ChrTbl_IsRelated(s32 a, s32 b);
extern void ChrView_PortFrame(f32 *screenX, f32 *yaw, s32 *chara, s32 *costume, s32 split);
extern s32 ChrView_PortVisible(s32 side);
extern void ChrView_PortEnd(void);
extern int Port_NetListShown(void);
extern int Port_WorldCan(void);
extern s32 ChrView_PortPedestal(s32 side, f32 *cx, f32 *cy, f32 *rx, f32 *ry);
extern f32 sinf(f32), cosf(f32);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern void ChrView_PortStyle(s32 wide, s32 entrance, f32 turn);
extern void ChrView_PortSplitZoom(f32 zoom);
extern int Port_SelectEntrance(void);
extern int Port_SelectTurnMilli(void);
extern void ChrView_PortAir(s32 side, f32 *air, f32 *land);
extern void ChrView_PortLineup(s32 n, const s32 *chara, const s32 *costume, const f32 *screenX);
extern void ChrView_PortCinema(s32 frame, s32 oppChara, s32 oppCostume);
extern s32 ChrView_PortStageUp(void);
extern int Port_SelectDemo(void);
#define MCHR_COLS 7
#define MCHR_RANDOM 0xA1
#define MCHR_LOCKED 0xA2
#define MCHR_CUSTOM 0xA3
#define MCHR_EMPTY 0xA4
#define CS_CHARA_COUNT 250 /* BT4's fighters (include/battle/view_a.h CHRVIEW_CHARA_COUNT) */

/* A fighter, not one of the grid's special cells: BT4's forms go past BT3's 161, but 0xA1..0xA4 stay the game's own
   random / locked / saved / empty (menu_c_e.c tests id < MCHR_RANDOM the same way). */
static s32 Cs_IsFighter(s32 chara) {
    return chara >= 0 && chara < CS_CHARA_COUNT && (chara < MCHR_RANDOM || chara > MCHR_EMPTY);
}

/* (sys/loading.h and sys/sprite.h have these, with a gProgress of another type than the menus') */
#define LOAD_SPR_DRAW 1
#define LOAD_SPR_END 2
typedef struct CsSprite {
    s32 flags, x0, y0, x1, y1, u0, v0, u1, v1, tex, r, g, b, a;
} CsSprite; /* LoadSprite: 0x38 bytes */
extern void Sprite_DrawList(void *res, s32 x, s32 y, CsSprite *list);

/* (BT3 has these at 0x22..0x27; in BT4 0x22..0x2C are the widescreen menus' parts: gs/gs_draw.c GsGpu_Native) */
#define GS_MUTE_ON 0x60
#define GS_MUTE_OFF 0x61
#define GS_WHOLE_ON 0x62 /* 2D is laid out on the whole window (the scene mode would narrow it to 3/4) */
#define GS_WHOLE_OFF 0x63
/* the 2D drawn after them at the screen's own resolution, not into the game's picture (gs_draw.h: the select only) */
#define GS_NATIVE_ON 0x66
#define GS_NATIVE_OFF 0x67
#define GS_CHIP_ON 0x64 /* the chips drawn from here are enlarged when decoded */
#define GS_CHIP_OFF 0x65

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
/* The way to the battle (PortCharSel_CinemaStart, the user's video of Sparking Zero): its frame (-1 none) and the
   opponent's fighter. The screen shows only the 3D then, the camera its own (view_a_d.c ChrView_PortCinema). */
#define CS_CINE_END 270 /* (view_a_d.c CHRVIEW_CINE_END) */
#define CS_CINE_CUT 55
/* The dust of the press (Cw_Dust): full over the screen at RISE, the way begins there; it has cleared at END */
#define CS_DUST 1 /* the grey streaks over the press: back on, at their full strength (the user); 0 takes them off */
#define CS_FX_RISE (CS_DUST ? 26 : 0)
#define CS_FX_HOLD 46
#define CS_FX_END 98
/* The way to the battle, as the game's own state (NOT host variables: an online session saves and restores the game's variables, so that both
   copies of the game, and a copy run again after a rollback, are at the same blank of it). sCineBlank: 0 not begun, else the blanks since the
   press, from 1; it is the only clock: the picture's variables below are worked out from it each frame. */
static s32 sCineBlank;
static s32 sCineOpp = -1, sCineOppCostume, sCineLead = -1, sCineLeadCostume, sCineToBattle;
static PORT_HOST s32 sCine = -1; /* (the picture's frame of the way, from sCineBlank: -1 none; 0 while the dust rises) */
/* The way to the battle kept up past the screen (the battle's loading): the viewer's last arguments, as drawn. */
static PORT_HOST s32 sCineHold, sKeepOk, sKeepN, sKeepTeam;
/* The way to the battle's lead (the player's first fighter, who the select must show, not the cursor's: against the CPU the cursor was last on the
   other team's pick) and its pre-roll: the clock of the way does not run until he is read and standing (behind the speed lines' flash), so he
   is not swapped, nor does he come down from the sky, in the middle of it. */
static PORT_HOST s32 sCinePre, sCinePreFrames;
static PORT_HOST s32 sCineFx = -1; /* frames since the press that began the way to the battle (the dust: Cw_Dust) */
extern s32 ChrView_PortLeadReady(s32 chara);
extern void ChrView_PortCinemaPre(s32 on);
static PORT_HOST s32 sKeepChara[8], sKeepCostume[8];
static PORT_HOST f32 sKeepAt[8], sKeepMiddle[2];
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
static PORT_HOST s32 sWide;   /* BT4's new select: one screen, wide (roster left, fighter large, its name low right) */
static PORT_HOST f32 sPopOf[2];    /* the chip under the cursor growing in (0..1), each half's */
static PORT_HOST s32 sPopAtOf[2];
#define sPop sPopOf[sNow]
#define sPopAt sPopAtOf[sNow]
static PORT_HOST s32 sDemo, sDemoAt; /* BT4_SELECT_DEMO=<frames>: testing, the fighters shown one after another */

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
        { "End", "Fim", "Fin" },
        { "Start Battle", "Iniciar Batalha", "Iniciar batalla" },
        { "Remove", "Remover", "Quitar" },
        { "Change", "Alternar", "Cambiar" },
        { "View Team", "Visualizar Equipe", "Ver equipo" },
        { "Waiting for the opponent", "Aguardando o oponente", "Esperando al rival" },
    };
    s32 lang = Port_Setting("dr_lang", 0);

    return kWords[id][lang >= 0 && lang < LANGS ? lang : 0];
}
enum { W_P1, W_P2, W_CPU, W_CHOOSE, W_FORMS, W_COSTUME, W_CUSTOM, W_NORMAL, W_C1, W_C2, W_C3, W_COST, W_OK, W_BACK,
       W_RANDOM, W_LOCKED, W_SAVED, W_ROTATE, W_ITEMS, W_EMPTY, W_TEAM, W_READY, W_END, W_START, W_REMOVE, W_SWAP, W_VIEWTEAM, W_WAIT };

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
/* BT4's new select: its letters' width for their height, as the online screens set it (tbo_look.inc Td_ScaleX: from the
   screen's pixels, not the page's units and the menus' narrowing, which squeezed them on a phone). A frame's. */
static PORT_HOST f32 sTextWide = 1.0f;
static PORT_HOST s32 sNarrow; /* no room for roster, fighter and details side by side: the details go under the roster */

/* BT4's new select at every shape of the picture (the user: the fighter in the same place as in "Screen", whatever the shape chosen): the one
   layout, the fighter at the same place across (0.55) and the same size; on a picture narrower than the device's own (2.22: where it was made)
   the menus are drawn at their size and then shrunk to as much of the picture as they take at 2.22 (sUiK, the same all through: shapes and letters
   as they are), the left ones from the left edge, the right ones from the right. The old narrow layout is left for the other variant. */
#define CS_REF_ASPECT 2222
#define CS_CINE_ASPECT 1750 /* (the way to the battle was tried from here up) */
static PORT_HOST f32 sUiK = 1.0f;
static PORT_HOST s32 sNoCine;
static void Cs_MeasureWindow(void) {
    s32 aspect = Port_PageAspectMilli();

    sAcrossPerDown = (512.0f / 448.0f) * 1000.0f / (f32)aspect;
    sNoCine = aspect < CS_CINE_ASPECT;
    if (gPortVariant != 0) {
        sNarrow = 0;
        sUiK = aspect < CS_REF_ASPECT ? (f32)aspect / (f32)CS_REF_ASPECT : 1.0f;
    } else {
        sNarrow = aspect < 1900;
        sUiK = 1.0f;
    }
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

/* (BT4's new select: dark indigo glass as the approved design, previas-bt4/selecao-nova; the rival's in crimson) */
#define CS_GLASS_TOP (sWide ? Ps_Rgba(0x10, 0x18, 0x3A, 0x6C) : sRed ? Ps_Rgba(0x52, 0x10, 0x26, 0x6C) : Ps_Rgba(0x1C, 0x16, 0x40, 0x6C))
#define CS_GLASS_LOW (sWide ? Ps_Rgba(0x04, 0x08, 0x1A, 0x76) : sRed ? Ps_Rgba(0x24, 0x04, 0x0E, 0x76) : Ps_Rgba(0x0A, 0x08, 0x1E, 0x76))
#define CS_GLASS_LINE (sWide ? Ps_Rgba(0x48, 0xDC, 0xFF, 0x50) : sRed ? Ps_Rgba(0xFF, 0x80, 0x90, 0x60) : Ps_Rgba(0x96, 0xAA, 0xFF, 0x60))
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
    list[0].x0 = (s32)Ps_AffX(x0 + sShift); /* (with the select in two halves, the picture goes where the rest of the half goes) */
    list[0].y0 = (s32)Ps_AffY(y0);
    list[0].x1 = (s32)Ps_AffX(x1 + sShift);
    list[0].y1 = (s32)Ps_AffY(y1);
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

/* BT4's new select: an evening field (the approved design) behind the fighters: a violet sky going pink at the
   horizon, far hills, grass, a few soft clouds drifting. Flat quads only: as light as the rays were. */
static void Cs_Field(void) {
    f32 drift = (f32)sTick * 0.05f;
    f32 x;
    s32 i;

    Cs_Rect(0.0f, 0.0f, 512.0f, 236.0f, Ps_Rgba(0x46, 0x30, 0xB8, 0x80), Ps_Rgba(0xE6, 0x8A, 0xC8, 0x80));
    for (i = 0; i < 5; i++) { /* clouds */
        f32 cx = (f32)((i * 137 + 40) % 560) - 24.0f + drift * (0.6f + 0.15f * (f32)i);
        cx -= (f32)((s32)(cx / 600.0f)) * 600.0f;
        Cs_Ellipse(cx, 50.0f + (f32)((i * 53) % 110), 70.0f + (f32)(i % 3) * 20.0f, 14.0f + (f32)(i % 2) * 6.0f,
                   Ps_Rgba(0xFF, 0xDC, 0xF4, 0x22), Ps_Rgba(0xFF, 0xDC, 0xF4, 0x00));
    }
    /* far hills, then near ones, a band of quads whose tops follow two waves */
    for (x = 0.0f; x < 512.0f; x += 16.0f) {
        f32 a0 = 214.0f - 14.0f * sinf(x * 0.0185f) - 7.0f * sinf(x * 0.051f + 1.0f);
        f32 a1 = 214.0f - 14.0f * sinf((x + 16.0f) * 0.0185f) - 7.0f * sinf((x + 16.0f) * 0.051f + 1.0f);
        f32 b0 = 236.0f - 8.0f * sinf(x * 0.025f + 2.0f);
        f32 b1 = 236.0f - 8.0f * sinf((x + 16.0f) * 0.025f + 2.0f);
        u32 far = Ps_Rgba(0x4A, 0x6E, 0x86, 0x80), near = Ps_Rgba(0x3A, 0x8A, 0x4C, 0x80), grass = Ps_Rgba(0x40, 0x9C, 0x44, 0x80);

        Ps_ScreenQuad(x, a0, x + 16.0f, a1, x, 250.0f, x + 16.0f, 250.0f, Cs_A(far), Cs_A(far), Cs_A(near), Cs_A(near));
        Ps_ScreenQuad(x, b0, x + 16.0f, b1, x, 252.0f, x + 16.0f, 252.0f, Cs_A(near), Cs_A(near), Cs_A(grass), Cs_A(grass));
    }
    Cs_Rect(0.0f, 250.0f, 512.0f, 448.0f, Ps_Rgba(0x44, 0xA2, 0x46, 0x80), Ps_Rgba(0x14, 0x46, 0x18, 0x80));
    Cs_Rect(0.0f, 232.0f, 512.0f, 262.0f, Ps_Rgba(0xFF, 0xB0, 0xD8, 0x00), Ps_Rgba(0xFF, 0xB0, 0xD8, 0x18)); /* haze at the horizon */
    Cs_Rect(0.0f, 380.0f, 512.0f, 448.0f, Ps_Rgba(0x04, 0x02, 0x10, 0x00), Ps_Rgba(0x04, 0x02, 0x10, 0x48)); /* the foot, for the prompts */
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
    if (1) { /* BT4's new select: no pedestal, the fighter stands in the grass on a soft shadow */
        f32 air;
        f32 land;
        f32 k;

        ChrView_PortAir(side, &air, &land);
        k = 1.0f - air * 0.7f; /* (from high up a small, faint shadow, growing as it comes down) */
        Cs_Ellipse(cx, cy, rx * 1.05f * k, ry * 1.1f * k, Ps_Rgba(0x00, 0x10, 0x00, (s32)(0x58 * k)), Ps_Rgba(0x00, 0x10, 0x00, 0x00));
        if (land > 0.0f) {
            /* the landing: a ring of dust and wind going out over the grass, fading */
            f32 e = 1.0f - (1.0f - land) * (1.0f - land);
            s32 a = (s32)(0x70 * (1.0f - land));

            Cs_Ring(cx, cy, rx * (0.9f + e * 2.2f), ry * (0.9f + e * 2.2f), rx * (0.75f + e * 2.0f), ry * (0.75f + e * 2.0f),
                    Ps_Rgba(0xF4, 0xEC, 0xD8, a));
            Cs_Ring(cx, cy, rx * (0.6f + e * 1.3f), ry * (0.6f + e * 1.3f), rx * (0.5f + e * 1.2f), ry * (0.5f + e * 1.2f),
                    Ps_Rgba(0xFF, 0xFF, 0xFF, a / 2));
            Cs_Ellipse(cx, cy, rx * (1.0f + e * 1.6f), ry * (1.0f + e * 1.6f), Ps_Rgba(0xE8, 0xDC, 0xC0, a / 3), Ps_Rgba(0xE8, 0xDC, 0xC0, 0x00));
        }
        return;
    }
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
    if (!sWide) {
        f32 pad = Cs_UnitsAcross(4.0f);
        f32 nameEnd = y + (17.0f + 35.0f + 20.0f) * small;
        f32 end = nameEnd + 6.0f + 21.0f * small;
        s32 at = sV.col + sV.row * MCHR_COLS;

        Cs_Glass(x0 - pad, y - 3.0f, x1 + pad, nameEnd + 1.0f, 6.0f, CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_LINE);
        if (sV.isTeam) {
            end += 23.0f + 34.0f + 6.0f + (sV.dpMax > 0 ? 18.0f : 0.0f) + 22.0f;
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
    if (sWide) {
        goto forms; /* (who it is: Cw_Name; the team: Cw_Strip) */
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
    } else if (Cs_IsFighter(chara)) {
        Cs_Line(name, sizeof(name), (u32 *)sV.nameText, chara);
        Cs_Line(form, sizeof(form), (u32 *)sV.formText, chara);
    }
    Cs_Title(tx, y, name, 1.2f * small, x1 - x0, al);
    y += 35.0f * small;
    if (form[0] != 0) {
        Cs_Text(tx, y, form, 0.62f, x1 - x0, al, Ps_Rgba(0xFF, 0xEC, 0xA8, 0x80), Ps_Rgba(0xFF, 0xC0, 0x50, 0x80));
    }
    y += 20.0f * small + 6.0f;
    if (Cs_IsFighter(chara)) {
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
            if (Cs_IsFighter(sV.teamChara[i])) {
                Cs_Chip(sV.teamChara[i], a, y, a + w, y + tall, 0x80);
            }
            if (i == sV.cur) {
                Cs_Frame(a - 1.0f, y - 1.0f, a + w + 1.0f, y + tall + 1.0f, 2.0f,
                         sV.teamActive ? Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80) : Ps_Rgba(0xFF, 0xD8, 0x30, 0x80));
            }
        }
        Port_GsMarker(GS_CHIP_OFF);
        y += tall + 6.0f;
        /* the game's own End plate (cursor past the last slot): X there closes the team */
        {
            f32 b = x0 + CS_TEAM_MAX * w + (CS_TEAM_MAX - 1) * Cs_UnitsAcross(3.0f);

            Cs_Choice(x0, b < x1 ? b : x1, y, 18.0f, (char *)Cs_Words(W_END), sV.cur >= CS_TEAM_MAX, sV.teamActive, 0.5f);
            y += 22.0f;
        }
        if (sV.dpMax > 0) {
            snprintf(line, sizeof(line), "DP %d / %d", (int)sV.cost, (int)sV.dpMax);
            Cs_Text(tx, y - 2.0f, line, 0.56f, x1 - x0, al, Ps_Rgba(0xFF, 0xEC, 0xA8, 0x80), Ps_Rgba(0xFF, 0xC0, 0x50, 0x80));
            y += 18.0f;
        }
    }

    /* The forms of the cell under the cursor: only once it is chosen (the game's own step, with its sound), opening
       from the first chip. Their room is kept while closed, so nothing below moves as they open. */
forms:
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
                if (sWide) { /* (its own panel: nothing under it otherwise but the stage) */
                    Cs_Glass(x0 - Cs_UnitsAcross(5.0f), y - 5.0f, x1 + Cs_UnitsAcross(5.0f), top + (f32)rows * tall + (f32)(rows - 1) * 3.0f + 6.0f,
                             8.0f, CS_GLASS_TOP, CS_GLASS_LOW, sV.formActive ? CS_GLASS_GOLD : CS_GLASS_LINE);
                }
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
    f32 k = sWide ? 1.3f : 1.0f; /* (BT4's new select: larger, they are touched) */
    f32 mid = sWide ? 421.0f : 426.0f;
    f32 half = 17.0f * k;
    f32 y = Cs_PageY(mid);
    f32 x = Cs_PageX(at);
    f32 step = (Ps_X(82.0f) - Ps_X(0.0f)) * k;
    u32 top = Ps_Rgba(255, 255, 255, 0x80);
    u32 bottom = Ps_Rgba(0xD8, 0xE8, 0xFF, 0x80);
    f32 textY = mid - PS_CELL_UNITS * 0.6f * k * 0.5f;
    f32 word = at + Cs_UnitsAcross(26.0f * k);
    f32 right = word + step * 2.1f + Ps_TextWidth((char *)Cs_Words(W_ROTATE), 0.52f * k) * (Ps_X(1.0f) - Ps_X(0.0f));
    s32 rotate = s3D && !sSplit && right + Cs_UnitsAcross(12.0f) < 508.0f;

    if (rotate) {
        f32 smallY = mid - PS_CELL_UNITS * 0.52f * k * 0.5f;

        Cs_Glass(at - Cs_UnitsAcross(10.0f), mid - half, right + Cs_UnitsAcross(12.0f), mid + half, 12.0f * k, CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_GOLD);
        Cs_Text(word + step * 2.1f, smallY, (char *)Cs_Words(W_ROTATE), 0.52f * k, 0.0f, 0, top, bottom);
    } else {
        right = word + step + Ps_TextWidth((char *)Cs_Words(W_BACK), 0.6f * k) * (Ps_X(1.0f) - Ps_X(0.0f));
        Cs_Glass(at - Cs_UnitsAcross(10.0f), mid - half, right + Cs_UnitsAcross(12.0f), mid + half, 12.0f * k, CS_GLASS_TOP, CS_GLASS_LOW, CS_GLASS_GOLD);
    }
    Ps_Button(0, x + 8.0f * k, y, 22.0f * k);
    Cs_Text(word, textY, (char *)Cs_Words(W_OK), 0.6f * k, 0.0f, 0, top, bottom);
    Ps_Button(2, x + 8.0f * k + 82.0f * k, y, 22.0f * k);
    Cs_Text(word + step, textY, (char *)Cs_Words(W_BACK), 0.6f * k, 0.0f, 0, top, bottom);
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
/* BT4's new select drawn sharp, as the online screens are (port_tbo.c: GsDraw_ScreenScaleFloor(16), 4x at least while
   it is up; the player's own resolution again at its end). Only if nobody else has set a floor (the online screens
   under it keep theirs); an online session sets it aside by itself (gs_draw.c screen_scale_session). Host variable: a
   blank run again after a rollback must not take it twice. */
static PORT_HOST s32 sOwnFloor;
static void Cs_Sharp(s32 on) {
    extern void GsDraw_ScreenScaleFloor(int floor4);
    extern int GsDraw_ScreenScaleFloorNow(void);
    extern int GsDraw_ScreenScaleNeeded4(void);

    (void)GsDraw_ScreenScaleNeeded4;
    if (on && !sOwnFloor && gPortVariant != 0 && GsDraw_ScreenScaleFloorNow() == 0 && !Port_Setting("select_native_ui", 1)) {
        /* (only without the 2D's own layer: 2x at least. Not more: the stage and fighters are drawn at this size too; on
           an Adreno 710 the select ran at 100% at 2x and at 50% at 3x) */
        GsDraw_ScreenScaleFloor(8);
        sOwnFloor = 1;
    } else if (!on && sOwnFloor) {
        GsDraw_ScreenScaleFloor(0);
        sOwnFloor = 0;
    }
}

void PortCharSel_Begin(s32 allow) {
    const char *env = getenv("BT3_SELECT_CLASSIC");
    s32 i;

    PortHold_Release(); /* (a held picture the loading never took) */
    sKeepOk = 0;
    if (sCineHold) { /* (a way to the battle that was never loaded behind) */
        sCineHold = 0;
        ChrView_PortEnd();
        Port_MenuScene3D(0);
    }
    sOn = allow && !(env != NULL ? atoi(env) : Port_Setting("select_classic", 0));
    Cs_Sharp(sOn);
    s3D = sOn && Port_WorldCan();
    sTick = 0;
    sSplit = 0;
    sInScene = 0;
    sCine = -1;
    sCinePre = 0;
    sCineFx = -1;
    sCineLead = -1;
    sCineBlank = 0;
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
    extern void Port_SelectStageSet(int stage);
    Port_GsMarker(GS_NATIVE_OFF); /* (never left on past the select) */
    Port_SelectStageSet(-1);
    sCine = -1;
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
    Cs_Sharp(0);
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
    Cs_Sharp(0);
}

/* The fighters chosen: the way to the battle instead of the game's VS (the chosen fighter's opponent: oppChara,
   oppCostume). 0: it cannot be (no 3D on this screen): the game's own way then. */
s32 PortCharSel_CinemaStart(s32 oppChara, s32 oppCostume, s32 leadChara, s32 leadCostume) {
    s32 online = Port_NetActive() || Port_NetSession();

    /* Online, whether this machine can draw it or not must not change the game: the flow (the blanks it takes, the battle that follows) is the
       same on every copy, so it is always begun; a machine that cannot draw it (no 3D, a narrow screen) shows the select meanwhile. */
    if (!online && (!sOn || !s3D || sSplit || sNarrow || sNoCine)) {
        return 0;
    }
    if (Cs_IsFighter(leadChara)) {
        ChrTbl_WrapCostume(leadChara, &leadCostume);
        sCineLead = leadChara;
        sCineLeadCostume = leadCostume;
    } else {
        sCineLead = -1; /* (not a fighter, a random: whatever the screen shows) */
    }
    if (Cs_IsFighter(oppChara)) {
        ChrTbl_WrapCostume(oppChara, &oppCostume);
    } else {
        oppChara = -1;
    }
    sCineOpp = oppChara;
    sCineOppCostume = oppCostume;
    sCineToBattle = 1;
    sCineBlank = 1;
    return 1;
}
/* The battle that follows the way to the battle (set at its start, cleared when the battle is freed): the fighters
   already stand in the stage, so the battle shows no loading screen, no stage presentation and no entrance cameras. */
s32 PortCharSel_CineBattle(void) {
    return sCineToBattle;
}
void PortCharSel_CineBattleEnd(void) {
    sCineToBattle = 0;
}
/* The way to the battle skipped (a press of the user's): to its end, the battle at once. */
void PortCharSel_CinemaSkip(void) {
    /* Online too: the press comes from the pads the two copies exchange (the callers read both, gPad[0] and gPad[1]), so it is the same blank
       on both: when one player skips, the other's way is skipped with it. Not at once: the press that began the way, or one a moment after, is
       not a skip (45 blanks). */
    if ((Port_NetActive() || Port_NetSession()) && sCineBlank < 45) {
        return;
    }
    if (sCineBlank > 0 && sCineBlank <= CS_FX_RISE + CS_CINE_END) {
        sCineBlank = CS_FX_RISE + CS_CINE_END + 1;
    }
}
/* One blank of the way to the battle: called by the screen's loop (the game's logic, once a blank, as the press was). */
void PortCharSel_CineBlank(void) {
    if (sCineBlank > 0 && sCineBlank < CS_FX_RISE + CS_CINE_END + 600) {
        sCineBlank++;
    }
}
s32 PortCharSel_CineFrame(void) {
    return sCinePre ? -1 : sCine;
}
s32 PortCharSel_InCinema(void) {
    return sCineBlank > 0; /* (the flow: on every copy, and in a copy run again) */
}
s32 PortCharSel_CinemaDone(void) {
    /* The way to the battle plays out WHOLE inside the select, where the viewer's world is up and alone, and the screen is
       left when it is over. It cannot go on while the battle is loaded behind it (what the night's build did: the camera, the
       stage and the objects it draws with are the fight's own modules' (gBtlCamView, gBtlStage, the object table), which
       Battle_Load initialises again as the viewer draws: the picture flickered between the way's camera, the stage's own
       camera and black, as in the user's recording). */
    return sCineBlank > CS_FX_RISE + CS_CINE_END;
}

/* Non-zero: the new screen is the one shown (not the game's own, select_classic). */
s32 PortCharSel_On(void) {
    return sOn;
}

/* The map chosen before the fighters (the stage-first order, menu_e_b.c / menu_c_e.c): the world behind them is made
   again with it at the next frame (its fighters read again into it). 0x23 (random) or -1: the usual stage. */
void PortCharSel_SetStage(s32 stage) {
    extern void Port_SelectStageSet(int stage);
    s32 i;

    Port_SelectStageSet(stage == 0x23 ? -1 : stage);
    if (s3D) {
        ChrView_PortEnd();
    }
    for (i = 0; i < 2; i++) {
        sShownChara[i] = sShownCostume[i] = -1;
    }
}

/* Non-zero: the screen's Draw shows this one instead of its own (asked with the screen in its stage choice or leaving). */
s32 PortCharSel_Shown(s32 stagePhase) {
    return sOn && (!stagePhase || sCine >= 0); /* (the way to the battle: shown to its end, the screen leaving) */
}

void PortCharSel_MuteOn(void) {
    Port_GsMarker(GS_MUTE_ON);
}

void PortCharSel_MuteOff(void) {
    Port_GsMarker(GS_MUTE_OFF);
}

/* Once a frame, before the screen is drawn: what it shows now, which models are wanted, and the 3D view on or off. */
void PortCharSel_Frame(const CsView *view, s32 split, s32 stagePhase) {
    s32 scene;

    /* The way to the battle's picture, from its clock (sCineBlank, the game's): the dust rises for CS_FX_RISE blanks (the fighters stand, the lead
       out of the picture till he is read: ChrView_PortLeadReady), then the way runs. A machine that cannot draw it (no 3D, a narrow screen) draws
       the select as it was meanwhile (online the way is always taken: its blanks are the same on every copy). */
    sCine = -1;
    sCinePre = 0;
    sCineFx = -1;
    if (sCineBlank > 0) {
        s32 want = sCineLead >= 0 ? sCineLead : sShownChara[0]; /* (a random: whatever stands) */

        ChrView_PortLeadReady(want); /* (always asked: it tells the viewer whom to wait for) */
        if (sOn && s3D && !sNarrow && !sNoCine && !split) {
            sCineFx = sCineBlank - 1;
            sCinePre = sCineFx < CS_FX_RISE;
            sCine = sCinePre ? 0 : sCineFx - CS_FX_RISE;
            if (sCine > CS_CINE_END + 600) {
                sCine = CS_CINE_END + 600;
            }
            stagePhase = 0; /* (the way to the battle: the 3D on until the screen is left) */
        }
    }
    scene = s3D && PortCharSel_Shown(stagePhase);
    s32 i;

    sSplit = split != 0;
    sViews[0] = view[0];
    if (sSplit) {
        sViews[1] = view[1];
    }
    if (sCine >= 0 && sCineLead >= 0) { /* the way to the battle: the player's fighter, not what the cursor rests on */
        sViews[0].chara = sCineLead;
        sViews[0].color = sCineLeadCostume;
    }
    sV = sViews[0];
    sNow = 0;
    sTick++;
    sInScene = scene;
    Port_MenuScene3D(scene);
    if (!scene) {
        return;
    }
    sDemo = Port_SelectDemo();
    if (sDemo > 0 && !sSplit && sViews[0].cellCount > 0) {
        /* testing (BT4_SELECT_DEMO=<frames>): the roster's fighters one after another, as if the cursor went there */
        if (sTick % sDemo == 0) {
            s32 k;

            for (k = 0; k < sViews[0].cellCount; k++) {
                sDemoAt = (sDemoAt + 1) % sViews[0].cellCount;
                if (Cs_IsFighter(sViews[0].cells[sDemoAt].id)) {
                    break;
                }
            }
        }
        sViews[0].chara = sViews[0].cells[sDemoAt].id;
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
            s32 chara = sWantChara[i];
            /* (the cursor keeps the last fighter's costume: a number this one may not have, whose file is empty) */
            s32 costume = sWantCostume[i];
            if (Cs_IsFighter(chara)) {
                ChrTbl_WrapCostume(chara, &costume);
            } else {
                chara = -1;
            }
            sShownChara[i] = chara;
            sShownCostume[i] = costume;
        }
    }
}

/* ---- BT4's new select: one screen, wide ----
   As the user's two pictures (previas: a select of the same kind). Choosing: up on the left a dark panel with the team's
   slots, a plus in each empty one; under it the roster, light upright chips on dark glass going back a little to the
   right, the one under the cursor orange, larger, its face out over the top; under it the buttons. The fighter stands
   large on the right in a fight's own stage (Supreme Kai's World), from the knees up, coming down from the sky in Goku
   Black's entrance and going over into its own quiet stance. A team complete: its fighters standing in a row in the
   stage, each in its own entrance, the stage left clear; up on the left their faces, under them Remove and Change,
   Start Battle at the foot. Only what is drawn: the cursor, the steps, the forms and the tab are the game's. */
/* (sizes in the window's units down, as measured on the user's picture of 435 lines: times 448 / 435) */
#define CW_STRIP_Y 22.0f /* the slots' panel */
#define CW_SLOT 52.0f
#define CW_GRID_Y 100.0f /* the first row of chips */
#define CW_CELL 50.0f
#define CW_GAP 5.0f
#define CW_ROWS 5
#define CW_UPRIGHT 0.88f /* a chip's width for its height */
#define CW_TILT 0.0f     /* the roster flat (template of 28/11; it went back 2.2% a column before) */
/* (the approved template since 20/11, previas-bt4/selecao-nova: indigo glass, a lavender edge, gold for what is
   chosen; the roster's panel keeps the curve it had) */
#define CW_PANEL_TOP Ps_Rgba(0x1C, 0x16, 0x40, 0x6C)
#define CW_PANEL_LOW Ps_Rgba(0x0A, 0x08, 0x1E, 0x73)
#define CW_PANEL_LINE Ps_Rgba(0x96, 0xAA, 0xFF, 0x3C)
#define CW_LIT Ps_Rgba(0xFF, 0xC4, 0x40, 0x80) /* (the template's GOLD) */
#define CW_LIT_DEEP Ps_Rgba(0xE6, 0x8C, 0x1E, 0x80)
#define CW_DARK_TEXT Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80) /* (words on the lit violet and orange: white) */
#define CW_FORM_TEXT Ps_Rgba(0xFF, 0xDE, 0x78, 0x80) /* (the template's form and cost: gold) */
#define CW_CHIP_TOP Ps_Rgba(0x46, 0x48, 0x58, 0x7A)
#define CW_CHIP_LOW Ps_Rgba(0x1C, 0x1C, 0x26, 0x7C)
#define CW_X Cs_UnitsAcross(26.0f)      /* the slots' left */
#define CW_GRID_X Cs_UnitsAcross(34.0f) /* the roster's left */

static f32 Cw_CellW(void) {
    return Cs_UnitsAcross(CW_CELL * CW_UPRIGHT);
}

/* The roster going back to the right: column c's size (1 for the first). */
static f32 Cw_Depth(f32 c) {
    return 1.0f - CW_TILT * c;
}

/* Where column c begins (its left), and the right of the last. */
static f32 Cw_ColX(s32 c) {
    f32 x = CW_GRID_X;
    s32 j;

    for (j = 0; j < c; j++) {
        x += (Cw_CellW() + Cs_UnitsAcross(CW_GAP)) * Cw_Depth((f32)j);
    }
    return x;
}

static f32 Cw_GridRight(void) {
    return Cw_ColX(MCHR_COLS) - Cs_UnitsAcross(CW_GAP) * Cw_Depth((f32)(MCHR_COLS - 1));
}

/* Row r's top in column c: the rows close up around the panel's middle as the columns go back. */
static f32 Cw_RowY(s32 r, s32 c) {
    f32 mid = CW_GRID_Y + ((CW_CELL + CW_GAP) * CW_ROWS - CW_GAP) * 0.5f;

    return mid + (CW_GRID_Y + (f32)r * (CW_CELL + CW_GAP) - mid) * Cw_Depth((f32)c);
}

static s32 Cw_Pulse(void) {
    return (sTick & 31) < 16 ? (sTick & 31) : 31 - (sTick & 31);
}

static void Cw_Text(f32 x, f32 y, char *s, f32 scale, f32 width, s32 align, u32 top, u32 bottom);
static f32 Cw_TextW(char *s, f32 scale);
static void Cw_Panel(f32 x0, f32 y0, f32 x1, f32 y1, s32 word, char *right);
/* The approved template (previas-bt4/redesign-geral/novos/tela-8-instinto-prateado.jpg and furia.py for the tabs) is
   2712 x 1220: its pixels in the window's units, down and (keeping shapes) across; its letters' sizes (Chakra Petch
   Bold, capitals 0.70 of the size) as the screen's scale; and how far below a line's top its capitals' middle is
   ('la' in the template: 0.642 of the size). */
#define CW_PXU (448.0f / 1220.0f)
#define CW_LA(em) ((em) * 0.642f)
static f32 Cw_Y(f32 p) {
    return p * CW_PXU;
}
static f32 Cw_X(f32 p) {
    return Cs_UnitsAcross(p * CW_PXU);
}
static f32 Cw_Em(f32 em) {
    return em * 0.019077f; /* (em * 0.70 capitals, in units down, over PS_CELL_UNITS * PS_CAP_OF_CELL) */
}
/* A line of the template's letters of size em, its capitals' middle at mid. */
static void Cw_Say(f32 x, f32 mid, char *s, f32 em, f32 width, s32 align, u32 top, u32 bottom) {
    f32 k = Cw_Em(em);

    Cw_Text(x, Ps_TextTop(mid, k), s, k, width, align, top, bottom);
}
static f32 Cw_SayW(char *s, f32 em) {
    return Cw_TextW(s, Cw_Em(em));
}
static s32 Cw_TeamCount(void);


/* A box with round corners (four steps each), its colour going down; moved with the tab sliding in (sShift). */
static void Cw_Box(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, u32 top, u32 bottom) {
    static const f32 kC[5] = { 1.0f, 0.92388f, 0.70711f, 0.38268f, 0.0f };
    f32 tall = y1 - y0;
    f32 rx;
    u32 t = Cs_A(top);
    u32 b = Cs_A(bottom);
    u32 m0;
    u32 m1;
    s32 i;

    if (r > tall * 0.5f) {
        r = tall * 0.5f;
    }
    rx = Cs_UnitsAcross(r);
    if (rx > (x1 - x0) * 0.5f) {
        rx = (x1 - x0) * 0.5f;
    }
    x0 += sShift;
    x1 += sShift;
    m0 = Cs_Mix(t, b, (s32)(r * 256.0f / tall));
    m1 = Cs_Mix(t, b, (s32)((tall - r) * 256.0f / tall));
    Ps_ScreenQuad(x0 + rx, y0, x1 - rx, y0, x0 + rx, y0 + r, x1 - rx, y0 + r, t, t, m0, m0);
    Ps_ScreenQuad(x0, y0 + r, x1, y0 + r, x0, y1 - r, x1, y1 - r, m0, m0, m1, m1);
    Ps_ScreenQuad(x0 + rx, y1 - r, x1 - rx, y1 - r, x0 + rx, y1, x1 - rx, y1, m1, m1, b, b);
    for (i = 0; i < 4; i++) {
        /* the four corners, a fan of four each: (cos, sin) of 90 degrees in four steps */
        f32 ca = kC[i], sa = kC[4 - i], cb = kC[i + 1], sb = kC[3 - i];
        u32 ta = Cs_Mix(t, b, (s32)((r - sa * r) * 256.0f / tall));
        u32 tb = Cs_Mix(t, b, (s32)((r - sb * r) * 256.0f / tall));
        u32 ba = Cs_Mix(t, b, (s32)((tall - r + sa * r) * 256.0f / tall));
        u32 bb = Cs_Mix(t, b, (s32)((tall - r + sb * r) * 256.0f / tall));

        Ps_ScreenQuad(x0 + rx, y0 + r, x0 + rx, y0 + r, x0 + rx - ca * rx, y0 + r - sa * r, x0 + rx - cb * rx, y0 + r - sb * r, m0, m0, ta, tb);
        Ps_ScreenQuad(x1 - rx, y0 + r, x1 - rx, y0 + r, x1 - rx + ca * rx, y0 + r - sa * r, x1 - rx + cb * rx, y0 + r - sb * r, m0, m0, ta, tb);
        Ps_ScreenQuad(x0 + rx, y1 - r, x0 + rx, y1 - r, x0 + rx - ca * rx, y1 - r + sa * r, x0 + rx - cb * rx, y1 - r + sb * r, m1, m1, ba, bb);
        Ps_ScreenQuad(x1 - rx, y1 - r, x1 - rx, y1 - r, x1 - rx + ca * rx, y1 - r + sa * r, x1 - rx + cb * rx, y1 - r + sb * r, m1, m1, ba, bb);
    }
}

/* Its outline, as thick as t, round at the corners as the box. */
static void Cw_Line(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, f32 t, u32 c) {
    static const f32 kC[5] = { 1.0f, 0.92388f, 0.70711f, 0.38268f, 0.0f };
    f32 rx;
    f32 tx = Cs_UnitsAcross(t);
    s32 i;

    if (r > (y1 - y0) * 0.5f) {
        r = (y1 - y0) * 0.5f;
    }
    rx = Cs_UnitsAcross(r);
    if (rx > (x1 - x0) * 0.5f) {
        rx = (x1 - x0) * 0.5f;
    }
    c = Cs_A(c);
    x0 += sShift;
    x1 += sShift;
    Ps_ScreenQuad(x0 + rx, y0, x1 - rx, y0, x0 + rx, y0 + t, x1 - rx, y0 + t, c, c, c, c);
    Ps_ScreenQuad(x0 + rx, y1 - t, x1 - rx, y1 - t, x0 + rx, y1, x1 - rx, y1, c, c, c, c);
    Ps_ScreenQuad(x0, y0 + r, x0 + tx, y0 + r, x0, y1 - r, x0 + tx, y1 - r, c, c, c, c);
    Ps_ScreenQuad(x1 - tx, y0 + r, x1, y0 + r, x1 - tx, y1 - r, x1, y1 - r, c, c, c, c);
    for (i = 0; i < 4; i++) {
        f32 ca = kC[i], sa = kC[4 - i], cb = kC[i + 1], sb = kC[3 - i];
        f32 ix = rx - tx > 0.0f ? rx - tx : 0.0f;
        f32 iy = r - t > 0.0f ? r - t : 0.0f;

        Ps_ScreenQuad(x0 + rx - ca * rx, y0 + r - sa * r, x0 + rx - cb * rx, y0 + r - sb * r, x0 + rx - ca * ix, y0 + r - sa * iy,
                      x0 + rx - cb * ix, y0 + r - sb * iy, c, c, c, c);
        Ps_ScreenQuad(x1 - rx + ca * rx, y0 + r - sa * r, x1 - rx + cb * rx, y0 + r - sb * r, x1 - rx + ca * ix, y0 + r - sa * iy,
                      x1 - rx + cb * ix, y0 + r - sb * iy, c, c, c, c);
        Ps_ScreenQuad(x0 + rx - ca * rx, y1 - r + sa * r, x0 + rx - cb * rx, y1 - r + sb * r, x0 + rx - ca * ix, y1 - r + sa * iy,
                      x0 + rx - cb * ix, y1 - r + sb * iy, c, c, c, c);
        Ps_ScreenQuad(x1 - rx + ca * rx, y1 - r + sa * r, x1 - rx + cb * rx, y1 - r + sb * r, x1 - rx + ca * ix, y1 - r + sa * iy,
                      x1 - rx + cb * ix, y1 - r + sb * iy, c, c, c, c);
    }
}

/* Who stands in slot i of the team: the member, or in the slot being chosen the fighter under the cursor. */
static s32 Cw_Member(s32 i) {
    if (!sV.isTeam) {
        return sV.chara;
    }
    if (i == sV.cur && sV.teamActive == 0 && !Cs_IsFighter(sV.teamChara[i])) {
        return sV.chara;
    }
    return sV.teamChara[i];
}

/* A team being chosen already has this fighter (or another form of the same person): its card is shown dim. The
   game's own rule (TeamSel_IsCharaFree: ChrTbl_IsRelated, which never counts Saibamen, Metal Cooler, Cell Jr. and
   the like as the same): those stay as they are. The place being chosen does not count. */
static s32 Cw_Taken(s32 chara) {
    s32 i;

    if (!sV.isTeam || !Cs_IsFighter(chara)) {
        return 0;
    }
    for (i = 0; i < CS_TEAM_MAX; i++) {
        if (i != sV.cur && Cs_IsFighter(sV.teamChara[i]) && ChrTbl_IsRelated(chara, sV.teamChara[i])) {
            return 1;
        }
    }
    return 0;
}

/* The panel's look (the approved preview, previas-bt4/abas-v2/nome-A-cartao.jpg): a soft shadow, dark glass lit at
   the top, a thin light edge, round corners. */
/* The template's panel: a soft shadow below right, graphite glass going darker down, lit at the top, a thin silver
   edge, round corners (16 of its pixels). r: the corners, in units down (0: the template's). */
/* The template's pieces (port/tools/cs_select_art.py, drawn at its pixels: smooth corners, soft shadows and glows, as
   the online screens' td_*): one stretched over a box keeping its corners (9 parts, Td_Nine's way). pad: the piece's
   room for its shadow or glow, c: its corner, both in its pixels; k: the corner drawn so much larger or smaller (a
   rounder or sharper box than the piece's own); tint 0x80 as it is. */
static void Cw_Nine(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 pad, f32 c, f32 k, u32 tint) {
    f32 w = Ps_PicW(id);
    f32 h = Ps_PicH(id);
    f32 cx = Cw_X(c * k);
    f32 cy = Cw_Y(c * k);
    f32 xs[4], ys[4], us[4], vs[4];
    s32 alpha = (s32)(128.0f * sAlpha);
    s32 i, j;

    if (alpha <= 0 || w <= 0.0f || h <= 0.0f) {
        return;
    }
    x0 += sShift - Cw_X(pad * k);
    x1 += sShift + Cw_X(pad * k);
    y0 -= Cw_Y(pad * k);
    y1 += Cw_Y(pad * k);
    if (cx * 2.0f > x1 - x0) {
        cx = (x1 - x0) * 0.5f;
    }
    if (cy * 2.0f > y1 - y0) {
        cy = (y1 - y0) * 0.5f;
    }
    xs[0] = x0; xs[1] = x0 + cx; xs[2] = x1 - cx; xs[3] = x1;
    ys[0] = y0; ys[1] = y0 + cy; ys[2] = y1 - cy; ys[3] = y1;
    us[0] = 0.0f; us[1] = c / w; us[2] = 1.0f - c / w; us[3] = 1.0f;
    vs[0] = 0.0f; vs[1] = c / h; vs[2] = 1.0f - c / h; vs[3] = 1.0f;
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            Ps_Sprite(id, Cs_PageX(xs[i]), Cs_PageY(ys[j]), Cs_PageX(xs[i + 1]), Cs_PageY(ys[j + 1]), us[i], vs[j], us[i + 1], vs[j + 1], tint, alpha);
        }
    }
}

/* A panel (cs_panel: its corners 16 of the template's pixels; r unused, the template's own). */
static void Cw_Glass(f32 x0, f32 y0, f32 x1, f32 y1, f32 r) {
    (void)r;
    /* (corners 26 of the template's pixels; the rival's half, the second player's, in red) */
    Cw_Nine(TBO_CS_PANEL, x0, y0, x1, y1, 52.0f, 78.0f, 1.0f, sRed ? Ps_Rgba(0xFF, 0x78, 0x88, 0x80) : PS_PLAIN);
}

/* A button's pill of glass, round ends (cs_pill, drawn for its height). */
static void Cw_PillGlass(f32 x0, f32 y0, f32 x1, f32 y1) {
    Cw_Nine(TBO_CS_PILL, x0, y0, x1, y1, 52.0f, 102.0f, (y1 - y0) / CW_PXU / 100.0f, sRed ? Ps_Rgba(0xFF, 0x78, 0x88, 0x80) : PS_PLAIN);
}

/* The template's lit box (the chosen one): cs_lit, its corners r (units down; the piece's own 26 of its pixels); a pill
   (r half its height or more) cs_pill_on. line: unused (the piece's own white line). */
static void Cw_Lit(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, f32 line) {
    f32 rp = r / CW_PXU;

    (void)line;
    if (r * 2.0f >= (y1 - y0) * 0.98f) {
        Cw_Nine(TBO_CS_PILL_ON, x0, y0, x1, y1, 40.0f, 90.0f, (y1 - y0) / CW_PXU / 100.0f, PS_PLAIN);
    } else {
        Cw_Nine(TBO_CS_LIT, x0, y0, x1, y1, 40.0f, 66.0f, rp / 26.0f, PS_PLAIN);
    }
}

/* Its plain box (cs_plain: a faint light glass, a thin edge). */
static void Cw_Plain(f32 x0, f32 y0, f32 x1, f32 y1, f32 r) {
    Cw_Nine(TBO_CS_PLAIN, x0, y0, x1, y1, 4.0f, 30.0f, r / CW_PXU / 26.0f, PS_PLAIN);
}

/* A fighter's colour on its card (the template's six hues: orange, blue, green, red, yellow, violet, each as light
   as its cells' tops), as a tint of cs_card (R | G << 8 | B << 16, 0x80 as it is); a place without one, plain. */
static u32 Cw_Hue(s32 who, s32 dim) {
    static const u8 kHue[6][3] = {{110, 74, 29}, {44, 52, 96}, {44, 81, 59}, {88, 44, 52}, {104, 88, 36}, {74, 44, 96}}; /* (28/11) */
    const u8 *h;

    if (!Cs_IsFighter(who)) {
        return dim ? 0x303030 : 0x5C5C5C;
    }
    h = kHue[(u32)(who * 7 + 3) % 6u];
    return dim ? (u32)(h[0] / 2) | ((u32)(h[1] / 2) << 8) | ((u32)(h[2] / 2) << 16) : (u32)h[0] | ((u32)h[1] << 8) | ((u32)h[2] << 16);
}

/* A card of the roster (cs_card) in its fighter's colour; dim: darker. */
static void Cw_Line(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, f32 t, u32 c);
static void Cw_Card(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, s32 dim, s32 who) {
    Cw_Nine(TBO_CS_CARD, x0, y0, x1, y1, 12.0f, 34.0f, r / CW_PXU / 22.0f, Cw_Hue(who, dim));
}

/* A face in an upright card: orange and lined in white when it is the one (chosen, under the cursor), light
   otherwise; dim: one the team already has. */
/* A face in an upright card: lit (silver, lined in white, a glow) when it is the one (chosen, under the cursor), a
   dark card otherwise; dim: one the team already has. Corners of 20 of the template's pixels. */
static void Cw_Frame(f32 x0, f32 y0, f32 x1, f32 y1, f32 r, s32 sel) {
    if (sel) {
        Cw_Nine(TBO_CS_FRAME_SEL, x0, y0, x1, y1, 24.0f, 40.0f, r / CW_PXU / 16.0f, PS_PLAIN);
    } else {
        Cw_Nine(TBO_CS_FRAME, x0, y0, x1, y1, 12.0f, 28.0f, r / CW_PXU / 16.0f, PS_PLAIN);
    }
}

/* A fighter's large portrait over a box, cut to the box's shape round the face (136 x 176: port/src/gs/bt4_face.inc). */
static void Cw_Face(s32 who, f32 x0, f32 y0, f32 x1, f32 y1, s32 grey, f32 alpha) {
    f32 shape = (x1 - x0) / Cs_UnitsAcross(y1 - y0) / (136.0f / 176.0f);
    f32 du = shape < 1.0f ? shape : 1.0f;
    f32 dv = shape > 1.0f ? 1.0f / shape : 1.0f;
    s32 a = (s32)(alpha * sAlpha * 128.0f);

    if (!Cs_IsFighter(who)) { /* (random, locked, saved: the game's own little picture) */
        Cs_Tex((MTexRes *)MPACK_AT(sV.chipPack, who + 1), x0, y0, x1, y1, 16, 11, 48, 52, grey, (s32)(alpha * 128.0f));
        return;
    }
    Ps_Sprite(TBO_FACE_BASE + who, Cs_PageX(x0 + sShift), Cs_PageY(y0), Cs_PageX(x1 + sShift), Cs_PageY(y1), 0.5f - du * 0.5f,
              (1.0f - dv) * 0.55f, 0.5f + du * 0.5f, (1.0f - dv) * 0.55f + dv, (u32)(grey | (grey << 8) | (grey << 16)), a);
}

static void Cw_FaceCard(s32 who, f32 x0, f32 y0, f32 x1, f32 y1, s32 lit, s32 dim) {
    f32 r = Cw_Y(20.0f);

    Cw_Card(x0, y0, x1, y1, r, dim, who);
    if (Cs_IsFighter(who)) {
        Cw_Face(who, x0 + Cw_X(4.0f), y0 + Cw_Y(4.0f), x1 - Cw_X(4.0f), y1 - Cw_Y(4.0f), dim ? 0x3C : 0x80, 1.0f);
    }
    Cw_Frame(x0, y0, x1, y1, r, lit);
}

/* The same, the face cut upright as the roster's chips. */
/* The same, the face cut upright as the roster's chips (the one under the cursor in the roster). */
static void Cw_FaceCardCut(s32 who, f32 x0, f32 y0, f32 x1, f32 y1, s32 lit, s32 dim) {
    (void)lit;
    Cw_Card(x0, y0, x1, y1, Cw_Y(18.0f), 0, who);
    Cw_Face(who, x0 + Cw_X(5.0f), y0 + Cw_Y(5.0f), x1 - Cw_X(5.0f), y1 - Cw_Y(5.0f), dim ? 0x48 : 0x80, 1.0f);
    Cw_Frame(x0, y0, x1, y1, Cw_Y(18.0f), 1);
}

/* Up on the left. One on one: the fighter's card (the approved preview A): its face in an orange card, its name large,
   its form in gold, its cost in a pill. A team: its five places (the one being chosen orange), the End plate, the
   points spent. */
/* Up on the left (the template). One on one: the fighter's card: its face in the lit card, its name large, its form in
   lilac under it, its cost in a pill at the right of the form's line. A team: its five places (the one being chosen
   lit, an empty one a plus), the End plate, the points spent. */
static void Cw_Strip(void) {
    f32 x0 = CW_X - Cw_X(20.0f); /* (the template's px(ua(26)) - 20) */
    f32 y0 = CW_STRIP_Y;
    f32 y1 = 88.0f;
    s32 i;
    char line[64];

    if (sV.isTeam) {
        f32 sw = Cw_X(97.0f);
        f32 step = Cw_X(123.0f);
        f32 endW = Cw_X(260.0f);
        f32 a;
        s32 on = sV.cur >= CS_TEAM_MAX;
        f32 s0 = y0 + Cw_Y(22.0f);
        f32 s1 = y1 - Cw_Y(22.0f);

        Cw_Glass(x0, y0, x0 + Cw_X(30.0f) + step * (f32)CS_TEAM_MAX + endW + Cw_X(40.0f), y1, 0.0f);
        for (i = 0; i < CS_TEAM_MAX; i++) {
            s32 who = Cw_Member(i);

            a = x0 + Cw_X(30.0f) + (f32)i * step;
            if (Cs_IsFighter(who)) {
                Cw_FaceCard(who, a, s0, a + sw, s1, i == sV.cur, 0);
            } else {
                Cw_Plain(a, s0, a + sw, s1, Cw_Y(20.0f));
                if (i == sV.cur) { /* (the place being chosen: a silver line round it) */
                    Cw_Line(a, s0, a + sw, s1, Cw_Y(20.0f), Cw_Y(5.0f), Ps_Rgba(0xF0, 0xF4, 0xFF, 0x60 + Cw_Pulse() * 2));
                }
                Cw_Say(a + sw * 0.5f, (s0 + s1) * 0.5f, "+", 90.0f, 0.0f, 1, Ps_Rgba(0xFF, 0xFF, 0xFF, 0x64), Ps_Rgba(0xFF, 0xFF, 0xFF, 0x64));
            }
        }
        a = x0 + Cw_X(30.0f) + step * (f32)CS_TEAM_MAX;
        if (on) {
            Cw_Lit(a, y0 + Cw_Y(40.0f), a + endW, y0 + Cw_Y(112.0f), Cw_Y(36.0f), Cw_Y(4.0f));
        } else {
            Cw_Plain(a, y0 + Cw_Y(40.0f), a + endW, y0 + Cw_Y(112.0f), Cw_Y(36.0f));
        }
        Cw_Say(a + endW * 0.5f, y0 + Cw_Y(76.0f), (char *)Cs_Words(W_END), 46.0f, endW, 1, on ? CW_DARK_TEXT : Ps_Rgba(255, 255, 255, 0x80),
               on ? CW_DARK_TEXT : Ps_Rgba(255, 255, 255, 0x80));
        if (sV.dpMax > 0) {
            snprintf(line, sizeof(line), "DP %d / %d", (int)sV.cost, (int)sV.dpMax);
            Cw_Say(a + endW * 0.5f, y1 - Cw_Y(40.0f), line, 36.0f, endW, 1, CW_FORM_TEXT, CW_FORM_TEXT);
        }
    } else {
    if (sSplit) { /* (two selects side by side: the card with its name, as it was) */
        s32 chara = sV.chara;
        char name[128];
        char form[128];
        f32 x1 = Cw_GridRight() + Cs_UnitsAcross(16.0f);
        f32 fh = (y1 - y0) - Cw_Y(44.0f);
        f32 f0 = x0 + Cw_X(26.0f);
        f32 f1 = f0 + Cs_UnitsAcross(fh);
        f32 nx = f1 + Cw_X(34.0f);
        f32 p0 = x1 - Cw_X(186.0f);
        f32 p1 = x1 - Cw_X(30.0f);

        name[0] = form[0] = 0;
        if (chara == MCHR_RANDOM) {
            Ps_Copy(name, Cs_Words(W_RANDOM), sizeof(name));
        } else if (chara == MCHR_LOCKED) {
            Ps_Copy(name, Cs_Words(W_LOCKED), sizeof(name));
        } else if (chara == MCHR_CUSTOM) {
            Ps_Copy(name, Cs_Words(W_SAVED), sizeof(name));
        } else if (Cs_IsFighter(chara)) {
            Cs_Line(name, sizeof(name), (u32 *)sV.nameText, chara);
            Cs_Line(form, sizeof(form), (u32 *)sV.formText, chara);
        }
        Cw_Glass(x0, y0, x1, y1, 0.0f);
        Cw_FaceCard(chara, f0, y0 + Cw_Y(22.0f), f1, y0 + Cw_Y(22.0f) + fh, 1, 0);
        /* (the name: the template's 62 at y0 + 34; alone, in the card's middle) */
        Cw_Say(nx, form[0] != 0 ? y0 + Cw_Y(34.0f + CW_LA(62.0f)) : (y0 + y1) * 0.5f, name, 62.0f, x1 - nx - Cw_X(30.0f), 0,
               Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(0xBA, 0xDC, 0xFF, 0x80)); /* (the template's name: white to light blue) */
        if (form[0] != 0) {
            Cw_Say(nx + Cw_X(4.0f), y1 - Cw_Y(64.0f - CW_LA(44.0f)), form, 44.0f, p0 - nx - Cw_X(24.0f), 0, CW_FORM_TEXT,
                   CW_FORM_TEXT);
        }
        if (Cs_IsFighter(chara)) {
            /* its cost, in a pill at the right of the form's line */
            snprintf(line, sizeof(line), "%d DP", (int)ChrTbl_GetCost(chara));
            Cw_Box(p0, y1 - Cw_Y(76.0f), p1, y1 - Cw_Y(24.0f), Cw_Y(26.0f), Ps_Rgba(0xDC, 0xE1, 0xF0, 0x0D), Ps_Rgba(0xDC, 0xE1, 0xF0, 0x0D));
            Cw_Line(p0, y1 - Cw_Y(76.0f), p1, y1 - Cw_Y(24.0f), Cw_Y(26.0f), Cw_Y(3.0f), Ps_Rgba(0xF0, 0xF4, 0xFF, 0x80));
            Cw_Say((p0 + p1) * 0.5f, y1 - Cw_Y(50.0f), line, 38.0f, p1 - p0, 1, CW_FORM_TEXT, CW_FORM_TEXT);
        }
    } else {
        s32 chara = sV.chara;
        f32 fh = (y1 - y0) - Cw_Y(44.0f);
        f32 f0 = x0 + Cw_X(26.0f);
        f32 f1 = f0 + Cs_UnitsAcross(fh);
        f32 p0 = f1 + Cw_X(26.0f);
        f32 p1 = p0 + Cw_X(170.0f);

        Cw_Glass(x0, y0, Cs_IsFighter(chara) ? p1 + Cw_X(26.0f) : f1 + Cw_X(26.0f), y1, 0.0f);
        Cw_FaceCard(chara, f0, y0 + Cw_Y(22.0f), f1, y0 + Cw_Y(22.0f) + fh, 1, 0);
        if (Cs_IsFighter(chara)) {
            snprintf(line, sizeof(line), "%d DP", (int)ChrTbl_GetCost(chara));
            Cw_Box(p0, y0 + Cw_Y(52.0f), p1, y0 + Cw_Y(104.0f), Cw_Y(26.0f), Ps_Rgba(0xDC, 0xE1, 0xF0, 0x0D), Ps_Rgba(0xDC, 0xE1, 0xF0, 0x0D));
            Cw_Line(p0, y0 + Cw_Y(52.0f), p1, y0 + Cw_Y(104.0f), Cw_Y(26.0f), Cw_Y(3.0f), Ps_Rgba(0xF0, 0xF4, 0xFF, 0x80));
            Cw_Say((p0 + p1) * 0.5f, y0 + Cw_Y(78.0f), line, 38.0f, p1 - p0, 1, CW_FORM_TEXT, CW_FORM_TEXT);
        }
    }
    }
}

/* Up on the right (the template of 28/11): the fighter's name large, its form under it, and "Pericia": its DP cost as
   seven stars. */
static void Cw_Name(void) {
    s32 chara = sV.chara;
    s32 lang = Port_Setting("dr_lang", 0);
    char name[128];
    char form[128];
    f32 x1 = 512.0f - Cw_X(60.0f);
    s32 i, stars;

    name[0] = form[0] = 0;
    if (chara == MCHR_RANDOM) {
        Ps_Copy(name, Cs_Words(W_RANDOM), sizeof(name));
    } else if (chara == MCHR_LOCKED) {
        Ps_Copy(name, Cs_Words(W_LOCKED), sizeof(name));
    } else if (chara == MCHR_CUSTOM) {
        Ps_Copy(name, Cs_Words(W_SAVED), sizeof(name));
    } else if (Cs_IsFighter(chara)) {
        Cs_Line(name, sizeof(name), (u32 *)sV.nameText, chara);
        Cs_Line(form, sizeof(form), (u32 *)sV.formText, chara);
    }
    Cw_Say(x1, Cw_Y(46.0f), (char *)Cs_Words(sV.side == 1 ? W_P2 : W_P1), 44.0f, 0.0f, 2, Ps_Rgba(0x46, 0xAA, 0xFF, 0x80), Ps_Rgba(0x46, 0xAA, 0xFF, 0x80));
    for (i = 0; i < 8; i++) { /* (its thick dark outline: the name drawn round itself) */
        static const f32 kO[8][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f} };

        Cw_Say(x1 + Cw_X(kO[i][0] * 8.0f), Cw_Y(122.0f + kO[i][1] * 8.0f), name, 112.0f, Cw_X(900.0f), 2, Ps_Rgba(0x08, 0x1E, 0x46, 0x80),
               Ps_Rgba(0x08, 0x1E, 0x46, 0x80));
    }
    Cw_Say(x1, Cw_Y(122.0f), name, 112.0f, Cw_X(900.0f), 2, Ps_Rgba(0xA0, 0xF0, 0xFF, 0x80), Ps_Rgba(0x46, 0xC8, 0xFF, 0x80));
    if (form[0] != 0) {
        Cw_Say(x1, Cw_Y(196.0f), form, 40.0f, Cw_X(880.0f), 2, CW_FORM_TEXT, CW_FORM_TEXT);
    }
    if (!Cs_IsFighter(chara)) {
        return;
    }
    stars = (s32)ChrTbl_GetCost(chara);
    stars = stars < 0 ? 0 : stars > 7 ? 7 : stars;
    Cw_Say(x1 - Cw_X(7.0f * 62.0f + 20.0f), Cw_Y(256.0f), (char *)(lang == 1 ? "Per\303\255cia" : lang == 2 ? "Pericia" : "Skill"), 44.0f, 0.0f, 2,
           Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(255, 255, 255, 0x80));
    for (i = 0; i < 7; i++) {
        f32 cx = x1 - Cw_X((f32)(6 - i) * 62.0f + 30.0f);

        Ps_Sprite(i < stars ? TBO_CS_STAR_ON : TBO_CS_STAR_OFF, Cs_PageX(cx - Cw_X(32.0f)), Cs_PageY(Cw_Y(236.0f - 4.0f * (f32)i)), Cs_PageX(cx + Cw_X(32.0f)),
                  Cs_PageY(Cw_Y(300.0f - 4.0f * (f32)i)), 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    }
}

static void Cw_Grid(void) {
    s32 custom = sV.showCustom;
    CsCell *cells = custom ? sV.custom : sV.cells;
    s32 count = custom ? sV.customCount : sV.cellCount;
    s32 rows = (count + MCHR_COLS - 1) / MCHR_COLS;
    s32 col = custom ? sV.customCol : sV.col;
    s32 row = custom ? sV.customRow : sV.row;
    f32 cw = Cw_CellW();
    f32 right;
    f32 padX = Cs_UnitsAcross(9.0f);
    f32 litX = 0.0f;
    f32 litY = 0.0f;
    f32 litK = 1.0f;
    s32 lit = 0;
    s32 at;
    s32 top;
    s32 r;
    s32 c;
    /* where each column begins, how small it is, each row's top in it: worked out once a frame (the three passes
       below each worked every cell's out again, a column's left by adding up those before it) */
    f32 dep[MCHR_COLS];
    f32 colX[MCHR_COLS + 1];
    f32 rowY[CW_ROWS + 1][MCHR_COLS + 1];

    {
        f32 x = CW_GRID_X;
        f32 gap = Cs_UnitsAcross(CW_GAP);

        for (c = 0; c <= MCHR_COLS; c++) {
            colX[c] = x;
            if (c < MCHR_COLS) {
                dep[c] = Cw_Depth((f32)c);
                x += (cw + gap) * dep[c];
            }
            for (r = 0; r <= CW_ROWS; r++) {
                rowY[r][c] = Cw_RowY(r, c);
            }
        }
        right = colX[MCHR_COLS] - gap * dep[MCHR_COLS - 1];
    }
    if (row < sTopRowOf[sNow]) {
        sTopRowOf[sNow] = row;
    }
    if (row >= sTopRowOf[sNow] + CW_ROWS) {
        sTopRowOf[sNow] = row - CW_ROWS + 1;
    }
    if (sTopRowOf[sNow] > rows - CW_ROWS) {
        sTopRowOf[sNow] = rows - CW_ROWS;
    }
    if (sTopRowOf[sNow] < 0) {
        sTopRowOf[sNow] = 0;
    }
    top = sTopRowOf[sNow];
    at = col + row * MCHR_COLS + (custom ? 1000 : 0);
    if (at != sPopAt) {
        sPopAt = at;
        sPop = 0.0f;
    }
    sPop += (1.0f - sPop) * 0.3f;
    {
        /* the panel: dark glass going back to the right, as the chips do (its right side shorter) */
        f32 lt = rowY[0][0] - 9.0f;
        f32 lb = rowY[CW_ROWS][0] + 4.0f;
        f32 rt = rowY[0][MCHR_COLS] - 9.0f;
        f32 rb = rowY[CW_ROWS][MCHR_COLS] + 4.0f;
        f32 x0 = CW_GRID_X - padX;
        f32 x1 = right + padX * 1.8f;
        u32 t = Cs_A(CW_PANEL_TOP);
        u32 b = Cs_A(CW_PANEL_LOW);
        u32 l = Cs_A(CW_PANEL_LINE);
        f32 tx = Cs_UnitsAcross(1.0f);

        Ps_ScreenQuad(x0, lt, x1, rt, x0, lb, x1, rb, t, t, b, b);
        Ps_ScreenQuad(x0, lt, x1, rt, x0, lt + 1.0f, x1, rt + 1.0f, l, l, l, l);
        Ps_ScreenQuad(x0, lb - 1.0f, x1, rb - 1.0f, x0, lb, x1, rb, l, l, l, l);
        Ps_ScreenQuad(x0, lt, x0 + tx, lt, x0, lb, x0 + tx, lb, l, l, l, l);
        Ps_ScreenQuad(x1 - tx, rt, x1, rt, x1 - tx, rb, x1, rb, l, l, l, l);
    }
    /* the cells' backs, light as the picture's; the one under the cursor afterwards */
    for (r = 0; r < CW_ROWS; r++) {
        for (c = 0; c < MCHR_COLS; c++) {
            s32 i = c + (r + top) * MCHR_COLS;
            f32 k = dep[c];
            f32 x0 = colX[c];
            f32 y0 = rowY[r][c];

            if (i >= count || cells[i].id == MCHR_EMPTY) {
                continue;
            }
            if (c == col && r + top == row) {
                lit = 1;
                litX = x0;
                litY = y0;
                litK = k;
                continue;
            }
            Cw_Card(x0, y0, x0 + cw * k, y0 + CW_CELL * k, Cw_Y(16.0f) * k, Cw_Taken(cells[i].id), cells[i].id);
        }
    }
    Port_GsMarker(GS_CHIP_ON);
    for (r = 0; r < CW_ROWS; r++) {
        for (c = 0; c < MCHR_COLS; c++) {
            s32 i = c + (r + top) * MCHR_COLS;
            f32 k = dep[c];
            f32 x0 = colX[c];
            f32 y0 = rowY[r][c];

            if (i >= count || cells[i].id == MCHR_EMPTY || (c == col && r + top == row)) {
                continue;
            }
            Cw_Face(cells[i].id, x0 + Cs_UnitsAcross(1.5f * k), y0 + 1.5f * k, x0 + cw * k - Cs_UnitsAcross(1.5f * k), y0 + CW_CELL * k - 1.5f * k,
                    Cw_Taken(cells[i].id) ? 0x38 : 0x80, 1.0f);
        }
    }
    Port_GsMarker(GS_CHIP_OFF);
    for (r = 0; r < CW_ROWS; r++) { /* the edges, over the faces */
        for (c = 0; c < MCHR_COLS; c++) {
            s32 i = c + (r + top) * MCHR_COLS;
            f32 k = dep[c];

            if (i >= count || cells[i].id == MCHR_EMPTY || (c == col && r + top == row)) {
                continue;
            }
            Cw_Frame(colX[c], rowY[r][c], colX[c] + cw * k, rowY[r][c] + CW_CELL * k, Cw_Y(16.0f) * k, 0);
        }
    }
    if (lit) {
        /* the one under the cursor: in orange, a little larger, glowing, lined in white; its face inside the card (the
           user: it came out of the frame) */
        s32 i = col + row * MCHR_COLS;
        f32 e = sPop * sPop * (3.0f - 2.0f * sPop);
        f32 w = cw * litK;
        f32 h = CW_CELL * litK;
        f32 gy = h * 0.06f * e;
        f32 gx = Cs_UnitsAcross(gy);

        Cw_FaceCardCut(cells[i].id, litX - gx, litY - gy, litX + w + gx, litY + h + gy, 1, Cw_Taken(cells[i].id));
    }
    if (rows > CW_ROWS) {
        /* how far down the list is: a thin bar at the panel's right */
        f32 x = right + padX * 0.9f;
        f32 w = Cs_UnitsAcross(3.0f);
        f32 y0 = Cw_RowY(0, MCHR_COLS);
        f32 tall = Cw_RowY(CW_ROWS, MCHR_COLS) - CW_GAP - y0;

        Cw_Box(x, y0, x + w, y0 + tall, 1.5f, Ps_Rgba(0x00, 0x00, 0x00, 0x30), Ps_Rgba(0x00, 0x00, 0x00, 0x30));
        Cw_Box(x, y0 + tall * (f32)top / (f32)rows, x + w, y0 + tall * (f32)(top + CW_ROWS) / (f32)rows, 1.5f, Ps_Rgba(0xFF, 0xFF, 0xFF, 0x70),
               Ps_Rgba(0xE8, 0xEC, 0xF4, 0x68));
    }
}

/* Under the roster, from its left (the approved template "Instinto Prateado", previas-bt4/redesign-geral/novos): each
   button with its word in a pill of dark glass; on a team's select View Team after them, lit silver, its word dark. */
/* Under the roster, from its left (the template): each button with its word in a pill of dark glass (100 of the
   template's pixels tall, its mark 72 across, its word of 52); on a team's select View Team after them, lit silver,
   its word dark. */

static void Cw_Foot(void) {
    static const s32 kKind[3] = { 0, 2, 1 };
    static const s32 kWord[3] = { W_OK, W_BACK, W_VIEWTEAM };
    f32 mid = Cw_RowY(CW_ROWS, 0) + 32.0f;
    f32 x = CW_GRID_X - Cw_X(10.0f);
    f32 h = Cw_Y(46.0f);
    f32 w[3];
    f32 all = Cw_X(24.0f);
    s32 n = sV.isTeam && s3D && Cw_TeamCount() > 0 ? 3 : 2; /* (View Team: only on a team's select) */
    s32 i;

    for (i = 0; i < n; i++) {
        w[i] = Cw_X(110.0f) + Cw_SayW((char *)Cs_Words(kWord[i]), 46.0f);
        all += w[i] + Cw_X(30.0f);
    }
    Cw_Glass(x, mid - h, x + all, mid + h, 0.0f);
    x += Cw_X(24.0f);
    for (i = 0; i < n; i++) {
        Ps_Button(kKind[i], Cs_PageX(x + Cw_X(40.0f) + sShift), Cs_PageY(mid), 33.0f);
        Cw_Say(x + Cw_X(90.0f), mid, (char *)Cs_Words(kWord[i]), 46.0f, 0.0f, 0, i == 2 ? CW_FORM_TEXT : Ps_Rgba(255, 255, 255, 0x80),
               i == 2 ? CW_FORM_TEXT : Ps_Rgba(255, 255, 255, 0x80));
        x += w[i] + Cw_X(30.0f);
    }
}

/* Online, once this player has chosen: the other is still choosing, and what he chooses is not shown (a surprise, until the way to the battle,
   which begins by itself the moment he has chosen). A pill of glass at the foot, in the middle, its words breathing. */
static void Cw_Waiting(void) {
    char *text = (char *)Cs_Words(W_WAIT);
    f32 w = Cw_SayW(text, 52.0f);
    f32 h = Cw_Y(120.0f);
    f32 mid = 448.0f * 0.80f;
    f32 pad = Cw_X(70.0f);

    sAlpha = 0.62f + 0.38f * (f32)Cw_Pulse() / 16.0f;
    Cw_PillGlass(256.0f - w * 0.5f - pad, mid - h * 0.5f, 256.0f + w * 0.5f + pad, mid + h * 0.5f);
    Cw_Say(256.0f, mid, text, 52.0f, w + 4.0f, 1, Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(0xE0, 0xEA, 0xFF, 0x80));
    sAlpha = 1.0f;
}

/* The menus shrunk (sUiK) for a picture narrower than the one they were made for: the left ones from the left edge, the right ones from the right. */
static void Cw_UiLeft(void) {
    Ps_SetAffine(sUiK, 0.0f, 0.0f);
}
static void Cw_UiRight(void) {
    Ps_SetAffine(sUiK, 512.0f * (1.0f - sUiK), 0.0f);
}
static void Cw_UiOff(void) {
    Ps_SetAffine(1.0f, 0.0f, 0.0f);
}

/* The fighter's soft shadow on the grass, small and faint from high up, and the dust of its landing. */
static void Cw_Shadow(s32 view, s32 lit) {
    f32 cx;
    f32 cy;
    f32 rx;
    f32 ry;
    f32 air;
    f32 land;
    f32 k;

    if (!s3D || !ChrView_PortPedestal(view, &cx, &cy, &rx, &ry)) {
        return;
    }
    if (ry < rx * 0.18f) {
        ry = rx * 0.18f;
    }
    ChrView_PortAir(view, &air, &land);
    k = 1.0f - air * 0.7f;
    if (lit) { /* (the team's member under the cursor: a light on the ground under it) */
        s32 glow = Cw_Pulse();

        Cs_Ellipse(cx, cy, rx * 1.6f, ry * 1.6f, Ps_Rgba(0xD2, 0xD2, 0xFF, 0x30 + glow), Ps_Rgba(0xB4, 0xAA, 0xF0, 0x00));
    }
    Cs_Ellipse(cx, cy, rx * 1.05f * k, ry * 1.1f * k, Ps_Rgba(0x00, 0x10, 0x00, (s32)(0x58 * k)), Ps_Rgba(0x00, 0x10, 0x00, 0x00));
    if (land > 0.0f) {
        f32 e = 1.0f - (1.0f - land) * (1.0f - land);
        s32 a = (s32)(0x70 * (1.0f - land));

        Cs_Ring(cx, cy, rx * (0.9f + e * 2.2f), ry * (0.9f + e * 2.2f), rx * (0.75f + e * 2.0f), ry * (0.75f + e * 2.0f), Ps_Rgba(0xF4, 0xEC, 0xD8, a));
        Cs_Ring(cx, cy, rx * (0.6f + e * 1.3f), ry * (0.6f + e * 1.3f), rx * (0.5f + e * 1.2f), ry * (0.5f + e * 1.2f), Ps_Rgba(0xFF, 0xFF, 0xFF, a / 2));
    }
}

/* ---- the tabs (approved previews, previas-bt4/abas-v1): dark rounded glass, a thin light edge, an orange mark by the
   heading; the chosen thing orange and lined in white, as the roster's card under the cursor ---- */

/* Text in the screens' own letters (Ps_SetTextKind 4 while this look draws: Chakra Petch, as the approved template,
   written as pictures at full resolution). As the template: clean, no dark outline or outer border (the two, the
   border eight times over, smeared every line on the user's picture). */
static void Cw_Text(f32 x, f32 y, char *s, f32 scale, f32 width, s32 align, u32 top, u32 bottom) {
    f32 pageW = Cs_PageX(width) - Cs_PageX(0.0f);

    Ps_Align(align);
    /* (the template's letters: a dark navy edge round them, readable over the stage) */
    Ps_TextExOuterXY(Cs_PageX(x + sShift), Cs_PageY(y), s, scale * sTextWide, scale, pageW, Cs_A(top), Cs_A(bottom),
                     Cs_A(Ps_Rgba(0x12, 0x0E, 0x34, 0x70)), 0, 0.0f);
    Ps_Align(0);
}

static f32 Cw_TextW(char *s, f32 scale) {
    return Ps_TextWidth(s, scale * sTextWide) * (Ps_X(1.0f) - Ps_X(0.0f));
}

/* A tab's panel: a soft shadow, dark glass lit at the top, a thin light edge; its heading with the orange mark. */
/* A tab's panel (the template's): its heading with the silver mark, the heading's word (46), and on its right a count
   (40, lilac). */
static void Cw_Head(f32 x0, f32 y, f32 x1, s32 word, char *right) {
    f32 a = x0 + Cw_X(16.0f);
    f32 b = x1 - Cw_X(16.0f);
    f32 y0 = y - Cw_Y(12.0f);
    f32 y1 = y + Cw_Y(60.0f);
    f32 mid = (y0 + y1) * 0.5f;

    Cw_Box(a, y0, b, y1, (y1 - y0) * 0.5f, Ps_Rgba(0x0C, 0x0C, 0x1E, 0x70), Ps_Rgba(0x0C, 0x0C, 0x1E, 0x70));
    Cw_Say(b - Cw_X(80.0f), mid, (char *)Cs_Words(word), 44.0f, b - a - Cw_X(230.0f), 2, Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(255, 255, 255, 0x80));
    Ps_Sprite(TBO_CS_STAR_OFF, Cs_PageX(b - Cw_X(62.0f) + sShift), Cs_PageY(mid - Cw_Y(22.0f)), Cs_PageX(b - Cw_X(18.0f) + sShift),
              Cs_PageY(mid + Cw_Y(22.0f)), 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, (s32)(0x80 * sAlpha));
    if (right != NULL) {
        Cw_Say(a + Cw_X(30.0f), mid, right, 38.0f, 0.0f, 0, CW_FORM_TEXT, CW_FORM_TEXT);
    }
}

/* A slanted bar, as the template's technique rows. */
static void Cw_Slant(f32 x0, f32 y0, f32 x1, f32 y1, u32 c) {
    f32 k = Cs_UnitsAcross((y1 - y0) * 0.28f);

    c = Cs_A(c);
    Ps_ScreenQuad(x0 + k + sShift, y0, x1 + sShift, y0, x0 + sShift, y1, x1 - k + sShift, y1, c, c, c, c);
}

/* A row of the right column: a coloured slanted bar, the chosen one blue lined in gold, a gold dot at its left. */
static void Cw_Row(f32 x0, f32 y0, f32 x1, f32 y1, s32 lit) {
    f32 mid = (y0 + y1) * 0.5f;
    f32 r = (y1 - y0) * 0.2f;

    if (lit) {
        Cw_Slant(x0 - Cs_UnitsAcross(2.5f), y0 - 1.8f, x1 + Cs_UnitsAcross(2.5f), y1 + 1.8f, Ps_Rgba(0xFF, 0xD7, 0x50, 0x80));
        Cw_Slant(x0, y0, x1, y1, Ps_Rgba(0x34, 0x70, 0xC4, 0x80));
    } else {
        Cw_Slant(x0, y0, x1, y1, Ps_Rgba(0x2D, 0x4E, 0x86, 0x6C));
    }
    Cs_Ellipse(x0 + Cw_X(38.0f) + sShift, mid, Cs_UnitsAcross(r), r, Cs_A(Ps_Rgba(0xFF, 0xCD, 0x32, 0x80)), Cs_A(Ps_Rgba(0xF0, 0xB4, 0x20, 0x80)));
}

static void Cw_Panel(f32 x0, f32 y0, f32 x1, f32 y1, s32 word, char *right) {
    Cw_Glass(x0, y0, x1, y1, 0.0f);
    Cw_Head(x0, y0 + Cw_Y(30.0f), x1, word, right);
}

/* The forms, the personalization and the costume (the game's steps: only drawn here). */
/* The forms, the personalization and the costume (the game's steps: only drawn here), as the template: on the right,
   760 of its pixels wide, 60 from the screen's edge. Transformations at 60 from the top: cards of 150 x 192 (the one
   chosen lit and raised), the form's name beside them (with four or more, smaller cards and the name under them).
   Personalization 30 under it: the sets as numbered rows of 88, the one chosen lit with a tick; the costume as one bar
   of four. */
static void Cw_Details(void) {
    f32 x1 = 512.0f - Cw_X(60.0f);
    f32 x0 = x1 - Cw_X(760.0f);
    s32 chara = sV.chara;
    CsCell *cell = NULL;
    s32 at = sV.col + sV.row * MCHR_COLS;
    s32 open;
    s32 want = sV.tabWant;
    f32 y = Cw_Y(sSplit ? 60.0f : 290.0f); /* (under the name and its stars) */
    char line[32];
    char form[128];

    if (!sV.showCustom && sV.col >= 0 && sV.row >= 0 && at < sV.cellCount) {
        cell = &sV.cells[at];
        if (cell->formCount <= 0 || cell->id == MCHR_RANDOM || cell->id == MCHR_LOCKED || cell->id == MCHR_EMPTY) {
            cell = NULL;
        }
    }
    open = cell != NULL && (sV.formActive || sV.tabWant || sV.done);
    if (cell == NULL || (!open && at != sFormCell[sNow])) {
        sFormOf[sNow] = 0.0f;
    }
    sFormCell[sNow] = at;
    sFormOf[sNow] += open ? 0.16f : -0.25f;
    sFormOf[sNow] = sFormOf[sNow] < 0.0f ? 0.0f : sFormOf[sNow] > 1.0f ? 1.0f : sFormOf[sNow];
    if (cell != NULL && sFormOf[sNow] > 0.0f) {
        s32 n = cell->formCount < 7 ? cell->formCount : 7;
        s32 beside = n <= 3;
        f32 t = sFormOf[sNow];
        f32 e = t * t * (3.0f - 2.0f * t);
        f32 cwp = beside ? 150.0f : (680.0f - 26.0f * (f32)(n - 1)) / (f32)n;
        f32 chp;
        f32 cw;
        f32 ch;
        f32 top = y + Cw_Y(86.0f);
        f32 bottom;
        f32 rc = Cw_Y(22.0f);
        s32 i;
        s32 now = 0;

        cwp = cwp > 118.0f && !beside ? 118.0f : cwp;
        cwp = beside ? 128.0f : cwp;
        chp = cwp * 192.0f / 150.0f;
        cw = Cw_X(cwp);
        ch = Cw_Y(chp);
        bottom = beside ? top + ch + Cw_Y(24.0f) : top + ch + Cw_Y(90.0f);
        for (i = 0; i < n; i++) {
            if (cell->form[i] == chara) {
                now = i + 1;
            }
        }
        sAlpha = e;
        snprintf(line, sizeof(line), "%d / %d", (int)now, (int)n);
        Cw_Panel(x0, y, x1, bottom, W_FORMS, line);
        for (i = 0; i < n; i++) { /* the cards, sliding out one after another */
            f32 p = (t * (f32)(n + 2) - (f32)i) * 0.5f;
            s32 lit = cell->form[i] == chara;
            f32 up = lit ? Cw_Y(10.0f) : 0.0f;
            f32 a;

            if (p <= 0.0f) {
                continue;
            }
            p = p > 1.0f ? 1.0f : p * p * (3.0f - 2.0f * p);
            a = x0 + Cw_X(40.0f) + (f32)i * (cw + Cw_X(26.0f)) * p;
            sAlpha = p * e;
            Cw_Card(a, top - up, a + cw, top + ch - up, rc, 0, cell->form[i]); /* (each form in its own colour) */
        }
        Port_GsMarker(GS_CHIP_ON);
        for (i = 0; i < n; i++) {
            f32 p = (t * (f32)(n + 2) - (f32)i) * 0.5f;
            f32 up = cell->form[i] == chara ? Cw_Y(10.0f) : 0.0f;
            f32 a;

            if (p <= 0.0f) {
                continue;
            }
            p = p > 1.0f ? 1.0f : p * p * (3.0f - 2.0f * p);
            a = x0 + Cw_X(40.0f) + (f32)i * (cw + Cw_X(26.0f)) * p;
            sAlpha = 1.0f;
            Cw_Face(cell->form[i], a + Cw_X(5.0f), top + Cw_Y(5.0f) - up, a + cw - Cw_X(5.0f), top + ch - Cw_Y(5.0f) - up, 0x80, p * e);
        }
        Port_GsMarker(GS_CHIP_OFF);
        for (i = 0; i < n; i++) {
            f32 p = (t * (f32)(n + 2) - (f32)i) * 0.5f;
            f32 up = cell->form[i] == chara ? Cw_Y(10.0f) : 0.0f;
            f32 a;

            if (p <= 0.0f) {
                continue;
            }
            p = p > 1.0f ? 1.0f : p * p * (3.0f - 2.0f * p);
            a = x0 + Cw_X(40.0f) + (f32)i * (cw + Cw_X(26.0f)) * p;
            sAlpha = p * e;
            Cw_Frame(a, top - up, a + cw, top + ch - up, rc, cell->form[i] == chara);
            {
                char num[4];

                snprintf(num, sizeof(num), "%d", (int)(i + 1));
                Cw_Say(a + cw - Cw_X(18.0f), top + ch - Cw_Y(20.0f) - up, num, 60.0f, 0.0f, 1, Ps_Rgba(0x10, 0x10, 0x20, 0x80), Ps_Rgba(0x10, 0x10, 0x20, 0x80));
                Cw_Say(a + cw - Cw_X(22.0f), top + ch - Cw_Y(24.0f) - up, num, 60.0f, 0.0f, 1, Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(255, 255, 255, 0x80));
            }
        }
        sAlpha = e;
        form[0] = 0;
        if (Cs_IsFighter(chara)) {
            Cs_Line(form, sizeof(form), (u32 *)sV.formText, chara);
        }
        if (form[0] != 0) {
            if (beside) {
                f32 fx = x0 + Cw_X(40.0f + (f32)n * 176.0f + 6.0f);

                Cw_Say(fx, y + Cw_Y(160.0f + CW_LA(46.0f)), form, 46.0f, x1 - Cw_X(30.0f) - fx, 0, CW_FORM_TEXT,
                       CW_FORM_TEXT);
            } else {
                Cw_Say(x0 + Cw_X(40.0f), top + ch + Cw_Y(50.0f), form, 46.0f, x1 - x0 - Cw_X(80.0f), 0, CW_FORM_TEXT,
                       CW_FORM_TEXT);
            }
        }
        sAlpha = 1.0f;
        y = bottom + Cw_Y(16.0f);
    }

    /* Personalization and costume: in from the screen's edge when the fighter is chosen, back with the triangle */
    sTabOf[sNow] += ((want ? 1.0f : 0.0f) - sTabOf[sNow]) * 0.2f;
    if (sTabOf[sNow] > 0.01f) {
        f32 t = sTabOf[sNow];
        f32 e = t * t * (3.0f - 2.0f * t);
        s32 sets = sV.plateCount < 4 ? sV.plateCount : 4;
        s32 colors = sV.colorCount < 12 ? sV.colorCount : 12; /* (every costume the fighter has: the game's own count, ChrTbl_WrapCostume) */
        f32 step = 80.0f; /* (rows in the template's pixels; closer when the forms took more room) */
        f32 room = 1200.0f - y / CW_PXU - 90.0f - 170.0f;
        f32 a = x0 + Cw_X(34.0f);
        f32 b = x1 - Cw_X(34.0f);
        f32 rr = Cw_Y(26.0f);
        f32 ry;
        s32 i;

        if (sets > 0 && room < step * (f32)sets) {
            step = room / (f32)sets;
            step = step < 56.0f ? 56.0f : step;
        }
        sShift = (1.0f - e) * (x1 - x0 + Cw_X(80.0f));
        sAlpha = e;
        Cw_Panel(x0, y, x1, y + Cw_Y(90.0f + (f32)sets * step + 170.0f), W_CUSTOM, NULL);
        ry = y + Cw_Y(90.0f);
        for (i = 0; i < sets; i++) {
            s32 lit = i == sV.plate;
            f32 r0 = ry;
            f32 r1 = ry + Cw_Y(step - 16.0f);
            f32 mid = (r0 + r1) * 0.5f;
            char num[4];

            Cw_Row(a, r0, b, r1, lit);
            num[0] = '0';
            num[1] = (char)('1' + i);
            num[2] = 0;
            Cw_Say(a + Cw_X(70.0f), mid, (char *)Cs_Words(W_NORMAL + i), 46.0f, b - a - Cw_X(190.0f), 0, Ps_Rgba(0xFF, 0xF0, 0x96, 0x80),
                   Ps_Rgba(0xFF, 0xF0, 0x96, 0x80));
            Cw_Say(b - Cw_X(60.0f), mid + Cw_Y(4.0f), num, 46.0f, 0.0f, 1, Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(255, 255, 255, 0x80));
            ry += Cw_Y(step);
        }
        /* the costume: one bar of four */
        snprintf(line, sizeof(line), "%d / %d", (int)(sV.color + 1), (int)colors);
        Cw_Head(x0, ry, x1, W_COSTUME, colors > 0 ? line : NULL);
        ry += Cw_Y(72.0f);
        if (colors > 0) {
            f32 h = Cw_Y(84.0f);
            f32 w = (b - a - Cw_X(16.0f)) / (f32)colors;

            Cw_Slant(a, ry, b, ry + h, Ps_Rgba(0x6E, 0x34, 0x98, 0x70));
            for (i = 0; i < colors; i++) {
                f32 s0 = a + Cw_X(8.0f) + w * (f32)i;
                s32 lit = i == sV.color;
                char digit[4];

                if (lit) {
                    Cw_Slant(s0, ry + Cw_Y(6.0f), s0 + w, ry + h - Cw_Y(6.0f), Ps_Rgba(0xF0, 0xA8, 0x28, 0x80));
                }
                snprintf(digit, sizeof(digit), "%d", (int)(i + 1));
                Cw_Say(s0 + w * 0.5f, ry + h * 0.5f, digit, colors > 8 ? 44.0f : 56.0f, 0.0f, 1, lit ? CW_DARK_TEXT : Ps_Rgba(0xC8, 0xCE, 0xE0, 0x80),
                       lit ? CW_DARK_TEXT : Ps_Rgba(0xC8, 0xCE, 0xE0, 0x80));
            }
        }
        sShift = 0.0f;
        sAlpha = 1.0f;
    }
}

/* The items of a set (the panel's movie still runs behind, not shown: its input is the game's). */
/* The items of a set (the panel's movie still runs behind, not shown: its input is the game's), as the template: the
   screen dimmed, a panel 1000 of its pixels wide at the right, 60 from the top; Items and the set's name, the slots in
   a pill; a row of 132 for each (as many as fit: eight rows here), the one under the cursor lit, the item's cost as
   bars at its right. */
static void Cw_Items(s32 side) {
    ItemPanel *p = gItemPanel[side];
    s32 want = (p->flags & ITEMPANEL_SHOWN) && !(p->flags & ITEMPANEL_CLOSING);
    f32 x1 = 512.0f - Cw_X(60.0f);
    f32 x0 = x1 - Cw_X(1000.0f);
    f32 y = Cw_Y(60.0f);
    f32 step = (1200.0f - 60.0f - 110.0f - 10.0f) / (f32)ITEMPANEL_ROWS; /* (150 in the template's six rows) */
    f32 a = x0 + Cw_X(34.0f);
    f32 b = x1 - Cw_X(34.0f);
    f32 rr = Cw_Y(26.0f);
    f32 e;
    s32 i;
    char line[96];
    char head[96];

    step = step > 150.0f ? 150.0f : step;
    sItemOf[side] += ((want ? 1.0f : 0.0f) - sItemOf[side]) * 0.2f;
    if (sItemOf[side] <= 0.01f) {
        return;
    }
    e = sItemOf[side] * sItemOf[side] * (3.0f - 2.0f * sItemOf[side]);
    sAlpha = e;
    Cs_Rect(0.0f, 0.0f, 512.0f, 448.0f, Ps_Rgba(0x04, 0x04, 0x0A, 0x2D), Ps_Rgba(0x04, 0x04, 0x0A, 0x2D));
    sShift = (1.0f - e) * (x1 - x0 + Cw_X(80.0f));
    Cw_Panel(x0, y, x1, y + Cw_Y(110.0f + (f32)ITEMPANEL_ROWS * step + 10.0f), W_ITEMS, NULL);
    Ps_Copy(head, Cs_Words(W_NORMAL + (p->set >= 0 && p->set <= 3 ? p->set : 0)), sizeof(head));
    Cw_Say(x0 + Cw_X(60.0f) + Cw_SayW((char *)Cs_Words(W_ITEMS), 46.0f) + Cw_X(40.0f), y + Cw_Y(38.0f + CW_LA(40.0f)), head, 40.0f, 0.0f, 0,
           Ps_Rgba(0xC8, 0xCE, 0xE0, 0x80), Ps_Rgba(0xC8, 0xCE, 0xE0, 0x80));
    {
        /* the slots used, in a pill on the right */
        f32 p0 = x1 - Cw_X(200.0f);
        f32 p1 = x1 - Cw_X(34.0f);

        snprintf(line, sizeof(line), "%d / %d", (int)p->used, (int)p->limit);
        Cw_Box(p0, y + Cw_Y(28.0f), p1, y + Cw_Y(86.0f), Cw_Y(29.0f), Ps_Rgba(0xDC, 0xE1, 0xF0, 0x0D), Ps_Rgba(0xDC, 0xE1, 0xF0, 0x0D));
        Cw_Line(p0, y + Cw_Y(28.0f), p1, y + Cw_Y(86.0f), Cw_Y(29.0f), Cw_Y(3.0f), Ps_Rgba(0xF0, 0xF4, 0xFF, 0x80));
        Cw_Say((p0 + p1) * 0.5f, y + Cw_Y(57.0f), line, 38.0f, p1 - p0, 1, CW_FORM_TEXT, CW_FORM_TEXT);
    }
    y += Cw_Y(110.0f);
    for (i = 0; i < ITEMPANEL_ROWS; i++) {
        s32 id = p->item.id[i];
        s32 lit = p->cursor == i;
        s32 usable = i < p->count || i == ITEMPANEL_BACK;
        s32 empty = 0;
        f32 r0 = y;
        f32 r1 = y + Cw_Y(step - 18.0f);
        f32 mid = (r0 + r1) * 0.5f;
        f32 keep = sAlpha;

        line[0] = 0;
        if (i == ITEMPANEL_BACK) {
            Ps_Copy(line, Cs_Words(W_BACK), sizeof(line));
        } else if (id != 0) {
            Cs_Line(line, sizeof(line), (u32 *)p->text[2], id - 1);
        } else if (usable) {
            Ps_Copy(line, Cs_Words(W_EMPTY), sizeof(line));
            empty = 1;
        }
        if (!usable) {
            sAlpha = keep * 0.4f;
        }
        if (lit) {
            Cw_Lit(a, r0, b, r1, rr, 0.0f);
        } else {
            Cw_Plain(a, r0, b, r1, rr);
        }
        Cw_Say(a + Cw_X(52.0f), mid, line, 54.0f, b - a - Cw_X(420.0f), 0,
               lit ? CW_DARK_TEXT : empty ? Ps_Rgba(0x98, 0x9E, 0xB4, 0x80) : Ps_Rgba(255, 255, 255, 0x80),
               lit ? CW_DARK_TEXT : empty ? Ps_Rgba(0x98, 0x9E, 0xB4, 0x80) : Ps_Rgba(255, 255, 255, 0x80));
        if (i != ITEMPANEL_BACK && id != 0) {
            /* its cost in slots, as bars */
            s32 cost = p->items[id - 1].slots;
            s32 n;

            for (n = 0; n < cost && n < 7; n++) {
                f32 c = b - Cw_X(64.0f + (f32)n * 50.0f);

                Cw_Box(c - Cw_X(16.0f), mid - Cw_Y(20.0f), c + Cw_X(16.0f), mid + Cw_Y(20.0f), Cw_Y(8.0f), CW_LIT, CW_LIT_DEEP);
                Cw_Line(c - Cw_X(16.0f), mid - Cw_Y(20.0f), c + Cw_X(16.0f), mid + Cw_Y(20.0f), Cw_Y(8.0f), Cw_Y(2.0f), Ps_Rgba(0xFF, 0xFF, 0xFF, 0x70));
            }
        }
        sAlpha = keep;
        y += Cw_Y(step);
    }
    sShift = 0.0f;
    sAlpha = 1.0f;
}

/* ---- a team complete: its fighters in a row ---- */
#define CW_LINE_VIEW 2 /* the viewer of the team's first (src/battle/view_a_d.c CHRVIEW_PORT_LINE) */

/* Where member i of n stands across the screen: the first in the middle, the others out to each side in turn. */
static f32 Cw_LineX(s32 i, s32 n) {
    static const f32 kAt[5][5] = {
        { 0.0f }, { -0.5f, 0.5f }, { 0.0f, -1.0f, 1.0f }, { -0.5f, 0.5f, -1.5f, 1.5f }, { 0.0f, -1.0f, 1.0f, -2.0f, 2.0f },
    };

    return 512.0f * 0.53f + kAt[n - 1][i] * 512.0f * 0.155f;
}

/* How many members the team has, and whether it is shown in a row now: the team complete (all five, or the cursor on
   its End plate), the cursor among them. */
static s32 Cw_TeamCount(void) {
    s32 n = 0;
    s32 i;

    for (i = 0; i < CS_TEAM_MAX; i++) {
        n += Cs_IsFighter(sV.teamChara[i]);
    }
    return n;
}

/* "View Team" (square, while a team is chosen: a button the team select does not use): the team in a row as it is so
   far, a look only; square again or any other button goes back to the roster (the game reads those as ever). */
static PORT_HOST s32 sViewTeam;

static void Cw_ViewTeamPad(void) {
    Pad *pad = &gPad[Port_NetActive() || Port_NetSession() ? sViews[0].side : 0];
    u32 others = PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_START | PAD_SELECT;

    if (!sV.isTeam || sV.done || !s3D || Cw_TeamCount() == 0) {
        sViewTeam = 0;
        return;
    }
    if (pad->pressed & PAD_SQUARE) {
        sViewTeam = !sViewTeam;
    } else if (pad->pressed & others) {
        sViewTeam = 0;
    }
}

static s32 Cw_LineShown(void) {
    s32 n = Cw_TeamCount();

    return s3D && sViewTeam && n > 0; /* (only when asked for: the user) */
}

/* View Team (the template's last screen): the faces up on the left in a panel, cards of 150 x 210 (the one under the
   cursor lit and raised, an empty place a plus), and the square to go back in a pill at the foot, in the middle. */
static void Cw_Line2D(void) {
    f32 x0 = Cw_X(70.0f);
    f32 y0 = Cw_Y(60.0f);
    f32 step = Cw_X(176.0f);
    f32 cw = Cw_X(150.0f);
    f32 rc = Cw_Y(22.0f);
    s32 i;

    Cw_Glass(x0, y0, x0 + Cw_X(30.0f) + step * (f32)CS_TEAM_MAX + Cw_X(20.0f), y0 + Cw_Y(270.0f), 0.0f);
    for (i = 0; i < CS_TEAM_MAX; i++) {
        f32 a = x0 + Cw_X(30.0f) + (f32)i * step;
        s32 here = i == sV.cur;
        f32 up = here ? Cw_Y(12.0f) : 0.0f;
        f32 c0 = y0 + Cw_Y(30.0f) - up;
        f32 c1 = y0 + Cw_Y(240.0f) - up;

        if (!Cs_IsFighter(sV.teamChara[i])) {
            Cw_Plain(a, c0, a + cw, c1, rc); /* (an empty place: the template's indigo slot with its plus) */
            Cw_Say(a + cw * 0.5f, (c0 + c1) * 0.5f, "+", 90.0f, 0.0f, 1, Ps_Rgba(0xFF, 0xFF, 0xFF, 0x64), Ps_Rgba(0xFF, 0xFF, 0xFF, 0x64));
            continue;
        }
        Cw_FaceCard(sV.teamChara[i], a, c0, a + cw, c1, here, 0);
    }
    {
        /* (only looked at: square goes back to the select) */
        f32 mid = Cw_Y(1110.0f);
        f32 w = Cw_X(130.0f) + Cw_SayW((char *)Cs_Words(W_BACK), 50.0f);
        f32 x = 256.0f - w * 0.5f;
        f32 h = Cw_Y(50.0f);

        Cw_PillGlass(x, mid - h, x + w, mid + h);
        Ps_Button(1, Cs_PageX(x + Cw_X(50.0f)), Cs_PageY(mid), 36.0f);
        Cw_Say(x + Cw_X(96.0f), mid, (char *)Cs_Words(W_BACK), 50.0f, 0.0f, 0, Ps_Rgba(255, 255, 255, 0x80), Ps_Rgba(255, 255, 255, 0x80));
    }
}

/* The dust that covers the screen at the press that starts the battle (the user: the effect must come at that very moment, then the
   way to the battle begins under it, and the dust clears away on it): brown earth smoke, puffs rising and swirling from the bottom, soft at
   their edges, over a brown veil. t: frames since the press. It is full over the screen at CS_FX_RISE (the way begins there) and has cleared
   at CS_FX_END. */
/* One streak of the effect: a solid core across most of its width and only a narrow soft edge on each side (crisp: the user, "very sharp"), from (cx, cy)
   along (dx, dy), `len` long and `wid` wide. */
static void Cw_Wisp(f32 cx, f32 cy, f32 dx, f32 dy, f32 len, f32 wid, u32 mid, u32 edge) {
    static const f32 kFringe = 0.16f; /* (the share of the width, each side, that fades) */
    f32 hx = dx * len * 0.5f, hy = dy * len * 0.5f, nx = -dy * wid * 0.5f, ny = dx * wid * 0.5f;
    f32 ix = nx * (1.0f - 2.0f * kFringe), iy = ny * (1.0f - 2.0f * kFringe); /* (the core's half width) */

    Ps_ScreenQuad(cx - hx - nx, cy - hy - ny, cx + hx - nx, cy + hy - ny, cx - hx - ix, cy - hy - iy, cx + hx - ix, cy + hy - iy, edge, edge, mid, mid);
    Ps_ScreenQuad(cx - hx - ix, cy - hy - iy, cx + hx - ix, cy + hy - iy, cx - hx + ix, cy - hy + iy, cx + hx + ix, cy + hy + iy, mid, mid, mid, mid);
    Ps_ScreenQuad(cx - hx + ix, cy - hy + iy, cx + hx + ix, cy + hy + iy, cx - hx + nx, cy - hy + ny, cx + hx + nx, cy + hy + ny, mid, mid, edge, edge);
}

/* The transition of the press, as the speed lines were, in dust: brown earth smoke streaming across the screen from the bottom left to the top
   right, in quickly and out quickly, with light and dark streaks racing along it. All drawn; nothing of the game's. t: frames since the press. */
static void Cw_Dust(s32 t) {
    /* the user's cartoon smoke (28/11; streaks of dust before): its frames, on the same clock: coming in until the
       screen is covered at CS_FX_RISE (its frame 11), covered a moment, gone by CS_FX_END */
    s32 n;

    if (!CS_DUST || t < 0 || t >= CS_FX_END) {
        return;
    }
    n = t < CS_FX_RISE ? t * 12 / CS_FX_RISE : t < CS_FX_HOLD ? 12 + (t - CS_FX_RISE) * 2 / (CS_FX_HOLD - CS_FX_RISE) :
        14 + (t - CS_FX_HOLD) * (TBO_SMOKE_FRAMES - 14) / (CS_FX_END - CS_FX_HOLD);
    n = n < 0 ? 0 : n >= TBO_SMOKE_FRAMES ? TBO_SMOKE_FRAMES - 1 : n;
    Ps_Sprite(TBO_SMOKE_BASE + n, Cs_PageX(-4.0f), Cs_PageY(-2.0f), Cs_PageX(516.0f), Cs_PageY(450.0f), 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
}

static void Cw_Draw(void) {
    f32 middle[2];
    s32 line = (sV = sViews[0], Cw_ViewTeamPad(), Cw_LineShown());
    s32 n = 0;
    s32 i;

    middle[0] = 512.0f * 0.55f; /* (the fighter between the roster and the panels on the right: at 0.61 he stood too
                                    far right, as the user saw) */
    middle[1] = 0.0f;
    if (!s3D || !Port_NetListShown() || !ChrView_PortStageUp()) {
        /* (until the stage is up: then it covers all of this. Drawn under it every frame before, full screen four times
           over, with 128 sines worked out each frame) */
        Cs_Field();
    }
    if (s3D) {
        f32 turn = gPad[Port_NetActive() || Port_NetSession() ? sViews[0].side : 0].gameRight[0];
        s32 chara[CS_TEAM_MAX];
        s32 costume[CS_TEAM_MAX];
        f32 at[CS_TEAM_MAX];

        if (sCine >= 0) { /* the way to the battle: a team in its row, from where it starts; nothing turned by hand */
            line = sV.isTeam && Cw_TeamCount() > 0;
            turn = 0.0f;
        }
        if (!line && (turn > 0.1f || turn < -0.1f)) {
            sYawOf[0] += turn * 0.06f;
            if (sYawOf[0] > 3.14159265f) {
                sYawOf[0] -= 6.2831853f;
            } else if (sYawOf[0] < -3.14159265f) {
                sYawOf[0] += 6.2831853f;
            }
        }
        if (line) {
            for (i = 0; i < CS_TEAM_MAX; i++) {
                if (Cs_IsFighter(sV.teamChara[i])) {
                    chara[n] = sV.teamChara[i];
                    costume[n] = sV.teamColor[i];
                    ChrTbl_WrapCostume(chara[n], &costume[n]);
                    n++;
                }
            }
            for (i = 0; i < n; i++) {
                at[i] = Cw_LineX(i, n);
            }
            for (i = 0; i < n; i++) {
                /* (the member under the cursor: which of the row it is, the members counted in order) */
                s32 j;
                s32 m = -1;

                for (j = 0, m = 0; j < CS_TEAM_MAX && j < sV.cur; j++) {
                    m += Cs_IsFighter(sV.teamChara[j]);
                }
                Cw_Shadow(CW_LINE_VIEW + i, sV.cur < CS_TEAM_MAX && i == m);
            }
        } else {
            Cw_Shadow(0, 0);
        }
        /* (one on one: turned a little more to the right than on a team's select, as the user asked) */
        ChrView_PortStyle(1, Port_SelectEntrance(), (f32)(Port_SelectTurnMilli() - (sV.isTeam ? 0 : 100)) * 0.001f);
        ChrView_PortLineup(n, chara, costume, at);
        ChrView_PortCinemaPre(sCinePre);
        ChrView_PortCinema(sCinePre ? -1 : sCine, sCineOpp, sCineOppCostume);
        if (sCine >= 0) {
            s32 k;

            sKeepOk = 1;
            sKeepN = n;
            sKeepTeam = sV.isTeam;
            sKeepMiddle[0] = middle[0];
            sKeepMiddle[1] = middle[1];
            for (k = 0; k < n && k < 8; k++) {
                sKeepChara[k] = chara[k];
                sKeepCostume[k] = costume[k];
                sKeepAt[k] = at[k];
            }
        }
        if (Port_NetListShown()) {
            ChrView_PortFrame(middle, sYawOf, sShownChara, sShownCostume, 0);
        }
    }
    if (Port_Setting("select_native_ui", 1)) { /* (on since 2026-11-28, the user: crisp menus; select_native_ui = 0 turns it off) */
        Port_GsMarker(GS_NATIVE_ON); /* (from here to PortCharSel_DrawEnd: the screen's 2D, at the screen's resolution) */
    }
    if (sCine >= 0) { /* the way to the battle: only the 3D, and the speed lines over it */
        Cw_Dust(sCineFx); /* (from the press: the dust covers the screen, the way begins under it, it clears away on it) */
        Font_FlushAll();
        return;
    }
    if (!line && !s3D) { /* (the portrait only where there is no 3D model at all: with it, none shows before the model comes, the user) */
        sV = sViews[0];
        Cs_Portrait(middle[0], 330.0f);
    }
    sNow = 0;
    sV = sViews[0];
    sRed = 0;
    if (line) {
        Cw_UiLeft();
        Cw_Line2D();
        Cw_UiOff();
        Font_FlushAll();
        return;
    }
    Cw_UiLeft();
    Cw_Strip();
    Cw_Grid();
    Cw_Foot();
    Cw_UiRight();
    Cw_Name();
    Cw_Details();
    Cw_UiOff();
    if (sV.done && sCineBlank == 0 && (Port_NetActive() || Port_NetSession())) {
        Cw_Waiting();
    }
    if (sDemo > 0 && sTick == 90) { /* testing: the roster's names, by number, once */
        s32 k;
        char nm[128];
        char fm[128];

        for (k = 0; k < CS_CHARA_COUNT; k++) {
            Cs_Line(nm, sizeof(nm), (u32 *)sV.nameText, k);
            Cs_Line(fm, sizeof(fm), (u32 *)sV.formText, k);
            fprintf(stderr, "bt3: select: name %d %s / %s\n", (int)k, nm, fm);
        }
    }
}

/* Two players choosing on one screen (BT4's new select, the same template as one player's: the user, "use the single mode's template, redo it"):
   each player has the single screen's card with the name and its roster at its full size, the first player's on the left edge, the second's, in
   red, on the right edge (the roster of the one player's screen takes a third of the width: two of them leave the middle third), and the two
   fighters stand close together between, large, the camera nearer than on one player's screen. The panels with the forms and the costumes (the
   single screen's right side) open smaller over the middle, each player's toward his own side. The buttons are drawn once, at the foot. */
#define CW_SPLIT_K 0.80f       /* each player's card and roster: this size of the single screen's (the user: the menus smaller, side by side with room) */
#define CW_SPLIT_DK 0.70f      /* the panels of the forms and costumes: this size, under each player's roster */
#define CW_SPLIT_DY 297.0f
#define CW_SPLIT_DY_TAB 62.0f  /* the personalization tab (the sets, the costumes), which is tall: the panel goes up over the roster, which is not used then */
#define CW_SPLIT_ZOOM 0.72f
/* (the measures of the one player's screen, in the picture's own units, which change with its shape: where the card and the roster end, where the right
   panel begins and ends, and the margin at the edge; the second player's half is put by them against the right edge) */
static f32 Cw_SplitCardRight(void) {
    return Cw_GridRight() + Cs_UnitsAcross(17.0f);
}
static f32 Cw_SplitMargin(void) {
    return Cs_UnitsAcross(25.0f) * CW_SPLIT_K * sUiK;
}
static void Cw_SplitCardAffine(s32 i) {
    f32 k = CW_SPLIT_K * sUiK;

    Ps_SetAffine(k, i == 0 ? 0.0f : 512.0f - Cw_SplitMargin() - k * Cw_SplitCardRight(), 4.0f);
}
static void Cw_SplitPanelAffine(s32 i) {
    f32 k = CW_SPLIT_DK * sUiK;
    f32 px0 = 512.0f - Cw_X(60.0f) - Cw_X(760.0f); /* (the single screen's right panel: its left and its right) */
    f32 px1 = 512.0f - Cw_X(60.0f);
    f32 t = sTabOf[i] < 0.0f ? 0.0f : sTabOf[i] > 1.0f ? 1.0f : sTabOf[i]; /* (as the tab comes in: the panel rises with it) */
    f32 oy = (CW_SPLIT_DY + (CW_SPLIT_DY_TAB - CW_SPLIT_DY) * t * t * (3.0f - 2.0f * t)) * sUiK;

    Ps_SetAffine(k, i == 0 ? Cw_SplitMargin() - k * px0 : 512.0f - Cw_SplitMargin() - k * px1, oy);
}

static void Cw_DrawSplit(void) {
    f32 middle[2];
    s32 i;

    middle[0] = 512.0f * 0.385f; /* (apart, in the middle: room on both sides for the menus) */
    middle[1] = 512.0f * 0.615f;
    if (!s3D || !Port_NetListShown() || !ChrView_PortStageUp()) {
        Cs_Field();
    }
    if (s3D) {
        ChrView_PortStyle(0, Port_SelectEntrance(), 0.0f); /* (every fighter comes down in the same entrance, and stays) */
        ChrView_PortSplitZoom(CW_SPLIT_ZOOM); /* (larger, nearer: not as far as it was) */
        for (i = 0; i < 2; i++) {
            f32 turn = gPad[Port_NetActive() || Port_NetSession() ? sViews[i].side : 0].gameRight[0];

            if (turn > 0.1f || turn < -0.1f) {
                sYawOf[i] += turn * 0.06f;
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
            ChrView_PortFrame(middle, sYawOf, sShownChara, sShownCostume, 1);
        }
    }
    if (!s3D) { /* (the portrait only where there is no 3D at all) */
        for (i = 0; i < 2; i++) {
            sV = sViews[i];
            Cs_Portrait(middle[i], 230.0f);
        }
    }
    if (Port_Setting("select_native_ui", 1)) {
        Port_GsMarker(GS_NATIVE_ON); /* (the two halves' menus at the screen's resolution too) */
    }
    for (i = 0; i < 2; i++) {
        sNow = i;
        sV = sViews[i];
        sRed = sV.side == 1;
        Cw_SplitCardAffine(i);
        Cw_Strip();
        Cw_Grid();
        Cw_SplitPanelAffine(i);
        Cw_Details();
        Ps_SetAffine(1.0f, 0.0f, 0.0f);
    }
    sRed = 0;
    sNow = 0;
    sV = sViews[0];
    {   /* (the buttons: once, in the middle, above the Android button at the bottom edge) */
        f32 left = CW_GRID_X - Cw_X(10.0f);
        f32 wide = Cw_X(120.0f) + Cw_SayW((char *)Cs_Words(W_OK), 52.0f) + Cw_X(26.0f) + Cw_X(120.0f) + Cw_SayW((char *)Cs_Words(W_BACK), 52.0f);
        f32 mid = Cw_RowY(CW_ROWS, 0) + 34.0f;

        Ps_SetAffine(sUiK, 256.0f - sUiK * (left + wide * 0.5f), 448.0f * 0.80f - sUiK * mid);
        Cw_Foot();
        Cw_UiOff();
    }
    Font_FlushAll();
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
    sWide = !sNarrow && gPortVariant != 0; /* (the two halves too: Cw_DrawSplit) */
    if (sDemo > 0) {
        sViews[0].chara = sShownChara[0]; /* (testing: what the grid and the name show follow the fighters shown) */
        sViews[0].col = sDemoAt % MCHR_COLS;
        sViews[0].row = sDemoAt / MCHR_COLS;
    }
    if (sWide) {
        extern float Port_MenuTextNarrow(void);

        /* (Td_ScaleX's: a line's capitals of c units down are c * 1220 / 448 of the template's pixels, as many across) */
        sTextWide = (1220.0f / 448.0f) * (512.0f - 2.0f * Ps_Left()) / (2712.0f * PS_UNIT_X * Port_MenuTextNarrow());
        Ps_SetTextKind(4);
        Ps_SetTextExact(1);
        if (sSplit) {
            Cw_DrawSplit();
        } else {
            Cw_Draw();
        }
        Font_FlushAll();
        Ps_SetTextKind(0);
        Ps_SetTextExact(0);
        return;
    }
    ChrView_PortStyle(0, Port_SelectEntrance(), 0.0f); /* (BT4: every fighter comes down in the same entrance, and stays) */
    ChrView_PortSplitZoom(0.8f); /* (the older halves, as they were) */
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
    Cs_Field(); /* (BT4: the field; BT3's rays are Cs_Backdrop) */
    (void)Cs_Backdrop;
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
        if (sWide) {
            Ps_SetTextKind(4);
            Ps_SetTextExact(1);
            if (sSplit) {
                sNow = i;
                Cw_SplitPanelAffine(i);
            }
            if (!sSplit) {
                Cw_UiRight();
            }
            Cw_Items(sViews[i].side);
            Cw_UiOff();
            sNow = 0;
            Ps_SetTextKind(0);
            Ps_SetTextExact(0);
        } else {
            Cs_Items(sViews[i].side);
        }
    }
    sRed = 0;
    Font_FlushAll();
}

void PortCharSel_DrawEnd(void) {
    Port_GsMarker(GS_NATIVE_OFF);
    Port_GsMarker(GS_WHOLE_OFF);
}
#endif

/* The way to the battle as the battle's loading screen (Load_RunBlocking): one frame of it, the viewer's world kept
   up past the select. */
s32 PortCharSel_CineHold(void) {
    return sCineHold;
}
s32 PortCharSel_CineLoadFrame(void) {
    if (!sCineHold) {
        return 0;
    }
    if (sCine < CS_CINE_END + 600) {
        sCine++;
    }
    fprintf(stderr, "bt3: cine: load frame %d\n", (int)sCine); /* (measured, for the recordings) */
    Port_MenuScene3D(1);
    ChrView_PortStyle(1, Port_SelectEntrance(), (f32)(Port_SelectTurnMilli() - (sKeepTeam ? 0 : 100)) * 0.001f);
    ChrView_PortLineup(sKeepN, sKeepChara, sKeepCostume, sKeepAt);
    ChrView_PortCinema(sCine, sCineOpp, sCineOppCostume);
    ChrView_PortFrame(sKeepMiddle, sYawOf, sShownChara, sShownCostume, 0);
    Cw_Dust(sCineFx);
    Font_FlushAll();
    return 1;
}
s32 PortCharSel_CineLoadDone(void) {
    return !sCineHold || sCine >= CS_CINE_END;
}
void PortCharSel_CineFinish(void) {
    if (!sCineHold) {
        return;
    }
    sCineHold = 0;
    ChrView_PortEnd();
    Port_MenuScene3D(0);
    sCine = -1;
    s3D = 0;
}
