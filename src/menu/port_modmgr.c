#include "common.h"
extern void Port_ClashLog(int player, const char *what, int fromInput, int total, int frame); /* DBG */
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "sys/save.h"
#ifdef PORT
#include "menu/port_mods.h"
#endif

#ifdef PORT
/*
 * PC only: the Mod Manager (main menu item 12; laid out after the mockup in gamedata/modmgr/src).
 *
 * The Library: every mod folder of <data>/mods (port/src/plat_mods.c finds them and answers the game's file
 * requests from the ones that are on) as a list of plates on the left - the eyes of the fighter it replaces, its
 * name, what it is, ON / OFF - and on the right the fighter itself as a model on a platform, with the mod's
 * files whether it is on or not, over a panel that says what the mod holds. Cross switches a mod on or off (the
 * next battle uses it; nothing is copied and no game file changes; one switched on switches off the mods for the
 * same fighter it would be mixed with), left / right show its other costumes, circle takes a mod out of the library
 * after asking (its folder moves to <data>/mods_removed), square opens the browser to add mods, R2 its settings,
 * START a test fight (L2 against whom), SELECT all on / off, triangle leaves.
 *
 * Everything is drawn here: the backdrop and the title are the two pictures of gamedata/modmgr/ui.bin
 * (port/build/ml/modmgr_assets.py), the plates and panels are GS primitives, the letters the game's font, the
 * model the Character Reference's viewer (ChrView) without its room.
 */
#include "menu/port_modui.h"
#include "menu/tbo_art.h"

ModMgr *sMm;
s32 gPortModTest;         /* the battle that runs is the manager's test fight (its pause menu, where it leaves to) */
static char sMmMoves[5][40]; /* the blasts of the mod under the cursor, read when the cursor has rested */
static s32 sMmMoveCount;
static s32 sMmMovesOf = -1;  /* whose they are; -1 = nobody's */
s32 gMmTestFoe = 31;      /* whom the test fight is against: Vegeta until another is chosen (L2) */
static s32 sMmResume = -1; /* the mod the manager opens on again after that fight */
static MmRes sMmOne;


extern u64 *Dma_BeginDirect(void);
extern void Dma_EndDirect(u64 *end);
extern void Dma_AddTexFlush(void);
extern void TexFile_UploadOne(void *res, s32 index, s32 tbp, s32 cbp);
extern void ChrView_Init(void);
extern void ChrView_Term(void);
extern s32 ChrView_Show(s32 chara, s32 costume, s32 damaged);
extern s32 ChrView_Hide(void);
extern s32 ChrView_IsVisible(void);
extern s32 ChrView_IsLoading(void);
extern void Job_Run(void);
extern s32 Font_GetCmdCount(void);
extern void Font_Flush(s32 first);
extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_SetFlags(s32 flags);
extern void Font_SetScale(f32 scale);
extern void Font_SetScaleXY(f32 sx, f32 sy);
extern void Mm_Align(s32 align);
extern void Font_SetShadowMode(s32 mode);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetShadowColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_PrintAsciiAt(s32 x, s32 y, char *str);
extern char *getenv(const char *name);
extern void OrbitCam_Update(s32 usePad);
extern void OrbitCam_SetTarget(f32 x, f32 y, f32 z);
extern void OrbitCam_SetAngles(f32 pitch, f32 yaw, f32 roll);
extern void OrbitCam_SetDist(f32 dist);
extern void OrbitCam_SetDistRange(f32 min, f32 max);
extern void BtlObj_UpdateAll(void);
extern void BtlObj_UpdateVisibility(s32 view);
extern void BtlObj_FinishVisibility(s32 split);
extern void BtlStage_Update(void);
extern void BtlScene_Update(void);
extern void BtlScene_Draw(s32 first);
extern void Gfx_MarkPass(s32 pass);
extern void Gfx_AddDefaultEnv(void);
extern void BtlObjDraw_Draw(void);
extern s32 Ot_Draw(void);
typedef struct MmStage {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ s32 flags; /* bit 0: not ready */
} MmStage;
extern MmStage *gBtlStage;
typedef struct MmViewer {
    /* 0x00 */ u8 unk0[0x30];
    /* 0x30 */ s32 stageOn;
    /* 0x34 */ s32 visible;
    /* 0x38 */ s32 zoom;
    /* 0x3C */ struct MmSlot *cur; /* the model on screen */
} MmViewer;
typedef struct MmSlot {
    /* 0x00 */ void *node;
    /* 0x04 */ s32 obj;            /* its battle object's id */
} MmSlot;
extern u8 *BtlObj_Get(s32 id);
extern MmViewer *gChrView;
typedef struct MmCharaInfo {
    /* 0x00 */ u8 unk0[0x30];
    /* 0x30 */ f32 targetY;
    /* 0x34 */ f32 dist;
} MmCharaInfo;
extern MmCharaInfo *ChrView_GetCharaInfo(u32 chara);

/* ---- drawing ---- */

u32 Mm_Rgba(s32 r, s32 g, s32 b, s32 a) {
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

/* Any four-cornered shape (corners in the order of a strip: top left, top right, bottom left, bottom right). */
void Mm_Quad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3) {
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

void Mm_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    Mm_Quad(x0, y0, x1, y0, x0, y1, x1, y1, top, top, bottom, bottom);
}

/* A plate leaning to the right (its top edge `lean` further right than its bottom edge), shaded from top to bottom. */
void Mm_Slant(f32 x0, f32 y0, f32 x1, f32 y1, f32 lean, u32 top, u32 bottom) {
    Mm_Quad(x0 + lean, y0, x1 + lean, y0, x0, y1, x1, y1, top, top, bottom, bottom);
}

/* ---- the pictures (gamedata/modmgr/ui.bin, port/build/ml/modmgr_assets2.py) ----
 * One file for the manager and for the mod grid of the character select: opened by whoever is up, closed with it.
 */
static MmRes *sUi;
static s32 sUiRef;
static MmTex *sMmLoaded; /* the texture that is in the menus' block of GS memory now (NULL: not known) */
static s32 sMmAlign;     /* 0 a line starts at its x, 1 it is centred on it, 2 it ends there */

#define MM_PAR 0.857f /* a square thing is this much narrower in the screen's pixels */

MmRes *Mm_UiOpen(void) {
    char path[200];

    if (sUiRef++ > 0) {
        return sUi;
    }
    sUi = Heap_Alloc(MM_UI_SIZE, 0x40, 0, 2);
    if (sUi == NULL) {
        return NULL;
    }
    if (getenv("BT3_DATA") != NULL) {
        sprintf(path, "%s/modmgr/ui.bin", getenv("BT3_DATA"));
    } else {
        sprintf(path, "gamedata/modmgr/ui.bin");
    }
    memset(sUi, 0, 0x40);
    if (Port_ReadFile(path, sUi, MM_UI_SIZE) > 0x60 && sUi->count >= MA_TEX_COUNT) {
        MmRes *res = sUi;

        Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
    } else {
        Heap_Free(sUi);
        sUi = NULL;
    }
    sMmLoaded = NULL;
    return sUi;
}

void Mm_UiClose(void) {
    if (sUiRef > 0 && --sUiRef == 0 && sUi != NULL) {
        Heap_Free(sUi);
        sUi = NULL;
    }
}

/* What was uploaded is not counted on any more (a new frame; something else drew). */
void Mm_TexReset(void) {
    sMmLoaded = NULL;
}

void Mm_Align(s32 align) {
    sMmAlign = align;
}

/* A piece of the sheet, stretched to a box of the screen. */
static void Mm_PiecePart(s32 piece, s32 u0, s32 v0, s32 u1, s32 v1, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha) {
    const u16 *p = sMaPiece[piece];

    if (sUi != NULL && x1 > x0 && y1 > y0) {
        Mm_Tex(&sUi->tex[MA_TEX_PIECES], x0, y0, x1, y1, p[0] + u0, p[1] + v0, p[0] + u1, p[1] + v1, 0x80, alpha);
    }
}

/* A bar with round ends, any length: its two caps keep their shape, its middle stretches. */
void Mm_Pill(s32 piece, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha) {
    const u16 *p = sMaPiece[piece];
    s32 capT = p[3] / 2;
    f32 capS = (y1 - y0) * 0.5f * MM_PAR;

    if (capS * 2.0f > x1 - x0) {
        capS = (x1 - x0) * 0.5f;
    }
    Mm_PiecePart(piece, 0, 0, capT, p[3], x0, y0, x0 + capS, y1, alpha);
    Mm_PiecePart(piece, capT, 0, p[2] - capT, p[3], x0 + capS, y0, x1 - capS, y1, alpha);
    Mm_PiecePart(piece, p[2] - capT, 0, p[2], p[3], x1 - capS, y0, x1, y1, alpha);
}

/* A window, any size: its corners keep their shape. */
void Mm_Panel(s32 piece, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha) {
    const u16 *p = sMaPiece[piece];
    s32 c = 18;
    f32 cy = 9.0f;
    f32 cx = 9.0f;
    s32 us[4];
    s32 vs[4];
    f32 xs[4];
    f32 ys[4];
    s32 i;
    s32 j;

    if (cy * 2.0f > y1 - y0) {
        cy = (y1 - y0) * 0.5f;
    }
    if (cx * 2.0f > x1 - x0) {
        cx = (x1 - x0) * 0.5f;
    }
    us[0] = 0;
    us[1] = c;
    us[2] = p[2] - c;
    us[3] = p[2];
    vs[0] = 0;
    vs[1] = c;
    vs[2] = p[3] - c;
    vs[3] = p[3];
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
            Mm_PiecePart(piece, us[i], vs[j], us[i + 1], vs[j + 1], xs[i], ys[j], xs[i + 1], ys[j + 1], alpha);
        }
    }
}

/* A piece as it is, `size` times its own size, centred. */
void Mm_Icon(s32 piece, f32 cx, f32 cy, f32 size, s32 alpha) {
    const u16 *p = sMaPiece[piece];
    f32 w = (f32)p[2] * 0.25f * size;
    f32 h = (f32)p[3] * 0.25f * size;

    Mm_PiecePart(piece, 0, 0, p[2], p[3], cx - w, cy - h, cx + w, cy + h, alpha);
}

/* The plates of the first look, as the pieces of the second: a low one is a bar (gold when its colour is warm, grey
   when it is grey, the list's teal otherwise), a tall one is a window. */
void Mm_Plate(f32 x0, f32 y0, f32 x1, f32 y1, f32 lean, f32 t, u32 edge, u32 top, u32 bottom) {
    s32 r = top & 0xFF;
    s32 g = (top >> 8) & 0xFF;
    s32 b = (top >> 16) & 0xFF;
    s32 a = (s32)(top >> 24);
    s32 alpha = a >= 0x58 ? 0x80 : a + 0x28;
    f32 h = y1 - y0;
    s32 piece;

    if (h > 50.0f) {
        Mm_Panel(MA_PANEL, x0 - 2.0f, y0 - 2.0f, x1 + lean + 2.0f, y1 + 2.0f, alpha);
        return;
    }
    if (g > r + 0x30 && g > b) {
        piece = MA_ON;
    } else if (r > b + 0x20) {
        piece = h >= 22.0f ? MA_ROW_SEL : MA_ON;
    } else if (r > 0x30 && g - r < 0x18 && b - r < 0x38) {
        piece = MA_OFF;
    } else {
        piece = h >= 22.0f ? MA_ROW : MA_SLOT;
    }
    Mm_Pill(piece, x0 - t, y0 - t, x1 + lean + t, y1 + t, alpha);
}

/* A piece (u0, v0)..(u1, v1) of a texture on the screen. */
void Mm_Tex(MmTex *t, f32 x0, f32 y0, f32 x1, f32 y1, s32 u0, s32 v0, s32 u1, s32 v1, s32 shade, s32 alpha) {
    u64 *p;

    if (t == NULL) {
        return;
    }
    if (sMmLoaded != t) { /* (one upload for all that is drawn from it in a row) */
        sMmOne.tex = t;
        TexFile_UploadOne(&sMmOne, 0, 0x3000, t->cbpOfs + 0x3040);
        Dma_AddTexFlush();
        sMmLoaded = t;
    }
    p = Dma_BeginDirect();
    p[0] = 4 | (1ULL << 15) | (1ULL << 60);
    p[1] = 0xE;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42; /* ALPHA_1 */
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14; /* TEX1_1: bilinear */
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47; /* TEST_1 */
    p += 2;
    p[0] = 5;
    p[1] = 0x08; /* CLAMP_1 */
    p += 2;
    p[0] = 1 | (1ULL << 15) | (1ULL << 58) | (8ULL << 60);
    p[1] = 0x535306E1; /* RGBAQ, A+D (ignored), TEX0_1, PRIM, UV, XYZ2, UV, XYZ2 */
    p += 2;
    p[0] = (u64)shade | ((u64)shade << 8) | ((u64)shade << 16) | ((u64)alpha << 24);
    p[1] = 0;
    p += 2;
    p[0] = t->tex0 | ((u64)(t->cbpOfs + 0x3040) << 37) | (((u64)4 << 32) | 0x3000);
    p[1] = 0x156;
    p += 2;
    p[0] = (u64)(u0 << 4) | ((u64)(v0 << 4) << 16);
    p[1] = (u64)((s32)(x0 * 16.0f) + 0x7000) | ((u64)((s32)(y0 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)(u1 << 4) | ((u64)(v1 << 4) << 16);
    p[1] = (u64)((s32)(x1 * 16.0f) + 0x7000) | ((u64)((s32)(y1 * 16.0f) + 0x7200) << 16);
    p += 2;
    Dma_EndDirect(p);
}

/* ---- the letters: Tahoma Bold from the sheet, an outline under every line ---- */

#define MM_TRACK 1.7f /* room between two letters, in the sheet's pixels: their outlines do not run together */
static s32 sMmTick;   /* frames, for what moves by itself */

/* How wide a line is at `scale` (scale 1: letters 21 lines high, as the game's own font was used here). */
f32 Mm_TextWidth(char *str, f32 scale) {
    f32 k = scale * (21.0f / 32.0f);
    f32 w = 0.0f;
    s32 i;

    for (i = 0; str[i] != 0; i++) {
        s32 c = (u8)str[i];

        if (c < 32 || c > 126) {
            c = '?';
        }
        w += ((f32)sMaGlyph[c - 32][3] * (1.0f / 16.0f) + MM_TRACK) * k;
    }
    return w;
}

/* A line: its letters shaded from `top` to `bottom`, on an outline of `line` (colours as Mm_Rgba makes them, alpha
   0x80 = solid). `width` > 0 is the room it has: a longer line is squeezed a little, and one that is still too long
   scrolls in that room - it waits, runs to its end, waits, and starts again - so all of it can be read. */
void Mm_TextEx(f32 x, f32 y, char *str, f32 scale, f32 width, u32 top, u32 bottom, u32 line) {
    MmTex *t;
    f32 k = scale * (21.0f / 32.0f);
    f32 kx = k;
    f32 w;
    f32 off = 0.0f;
    s32 scroll = 0;
    u64 *p;
    s32 pass;
    s32 i;

    if (sUi == NULL || str[0] == 0) {
        return;
    }
    w = Mm_TextWidth(str, scale);
    if (width > 0.0f && w > width) {
        f32 fit = width / w;

        if (fit < 0.88f) {
            fit = 0.88f;
        }
        kx = k * fit;
        w *= fit;
        if (w > width + 0.5f) {
            f32 over = w - width + 2.0f;
            s32 run = (s32)(over * 2.5f);
            s32 at = sMmTick % (run + 150);

            off = at < 75 ? 0.0f : (f32)(at - 75) * 0.4f;
            if (off > over) {
                off = over;
            }
            scroll = 1;
            w = width;
        }
    }
    if (sMmAlign == 1) {
        x -= w * 0.5f;
    } else if (sMmAlign == 2) {
        x -= w;
    }
    t = &sUi->tex[MA_TEX_FONT];
    if (sMmLoaded != t) {
        sMmOne.tex = t;
        TexFile_UploadOne(&sMmOne, 0, 0x3000, t->cbpOfs + 0x3040);
        Dma_AddTexFlush();
        sMmLoaded = t;
    }
    p = Dma_BeginDirect();
    p[0] = 6 | (1ULL << 15) | (1ULL << 60);
    p[1] = 0xE;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42; /* ALPHA_1 */
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14; /* TEX1_1: bilinear */
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47; /* TEST_1 */
    p += 2;
    p[0] = 5;
    p[1] = 0x08; /* CLAMP_1 */
    p += 2;
    p[0] = t->tex0 | ((u64)(t->cbpOfs + 0x3040) << 37) | (((u64)4 << 32) | 0x3000);
    p[1] = 0x06; /* TEX0_1 */
    p += 2;
    if (scroll) { /* SCISSOR_1: only its room is drawn in */
        s32 cx0 = (s32)x - 1;
        s32 cx1 = (s32)(x + width) + 1;

        if (cx0 < 0) {
            cx0 = 0;
        }
        if (cx1 > 511) {
            cx1 = 511;
        }
        if (cx1 < cx0) {
            cx1 = cx0;
        }
        p[0] = (u64)cx0 | ((u64)cx1 << 16) | ((u64)0x1BF << 48);
    } else {
        p[0] = 0x01BF000001FF0000ULL;
    }
    p[1] = 0x40;
    p += 2;
    for (pass = 0; pass < 2; pass++) { /* every outline, then every letter over them */
        u32 c0 = pass == 0 ? line : top;
        u32 c1 = pass == 0 ? line : bottom;
        s32 v = pass == 0 ? 256 : 0;
        f32 px = x - off;

        /* (a white texture times the vertex's colour, 0x80 = as it is) */
        c0 = ((c0 >> 1) & 0x007F7F7F) | (c0 & 0xFF000000);
        c1 = ((c1 >> 1) & 0x007F7F7F) | (c1 & 0xFF000000);
        for (i = 0; str[i] != 0; i++) {
            s32 c = (u8)str[i];
            const u16 *g;
            s32 sx0;
            s32 sx1;
            s32 sy0;
            s32 sy1;
            u64 u0;
            u64 u1;
            u64 v0;
            u64 v1;

            if (c < 32 || c > 126) {
                c = '?';
            }
            g = sMaGlyph[c - 32];
            if (c != ' ' && px > -40.0f && px < 540.0f) {
                sx0 = (s32)((px - (f32)MA_FONT_PAD * kx) * 16.0f) + 0x7000;
                sx1 = (s32)((px + (f32)(g[2] - MA_FONT_PAD) * kx) * 16.0f) + 0x7000;
                sy0 = (s32)(y * 16.0f) + 0x7200;
                sy1 = (s32)((y + (f32)MA_FONT_CELL * k) * 16.0f) + 0x7200;
                u0 = (u64)(g[0] << 4);
                u1 = (u64)((g[0] + g[2]) << 4);
                v0 = (u64)((g[1] + v) << 4);
                v1 = (u64)((g[1] + v + MA_FONT_CELL) << 4);
                /* PRIM, then four corners of UV, RGBAQ, XYZ2: a shaded strip of two triangles */
                p[0] = 1 | (1ULL << 15) | (1ULL << 58) | (13ULL << 60);
                p[1] = 0x5135135135130ULL;
                p += 2;
                p[0] = 0x15C;
                p[1] = u0 | (v0 << 16);
                p[2] = (u64)c0 | (0x3F800000ULL << 32);
                p[3] = (u64)(u32)sx0 | ((u64)(u32)sy0 << 16);
                p[4] = u1 | (v0 << 16);
                p[5] = (u64)c0 | (0x3F800000ULL << 32);
                p[6] = (u64)(u32)sx1 | ((u64)(u32)sy0 << 16);
                p[7] = u0 | (v1 << 16);
                p[8] = (u64)c1 | (0x3F800000ULL << 32);
                p[9] = (u64)(u32)sx0 | ((u64)(u32)sy1 << 16);
                p[10] = u1 | (v1 << 16);
                p[11] = (u64)c1 | (0x3F800000ULL << 32);
                p[12] = (u64)(u32)sx1 | ((u64)(u32)sy1 << 16);
                p[13] = 0;
                p += 14;
            }
            px += ((f32)g[3] * (1.0f / 16.0f) + MM_TRACK) * kx;
        }
    }
    if (scroll) { /* the whole screen again for what is drawn next */
        p[0] = 1 | (1ULL << 15) | (1ULL << 60);
        p[1] = 0xE;
        p += 2;
        p[0] = 0x01BF000001FF0000ULL;
        p[1] = 0x40;
        p += 2;
    }
    Dma_EndDirect(p);
}

/* A line in one colour, the way the first look asked for it: white is the game's pale lilac on purple, a warm colour
   is shaded to orange on brown, anything else keeps its colour on a dark outline. */
static void Mm_TextPlain(s32 x, s32 y, char *str, f32 scale, f32 width, s32 r, s32 g, s32 b, s32 a) {
    s32 la = a >= 0x80 ? 0x80 : a;

    if (r >= 0xF0 && g >= 0xF0 && b >= 0xF0) {
        Mm_TextEx((f32)x, (f32)y, str, scale, width, Mm_Rgba(0xFF, 0xFF, 0xFF, a), Mm_Rgba(0xE4, 0xD8, 0xFF, a), Mm_Rgba(0x4A, 0x18, 0x6C, la));
    } else if (r >= 0xE0 && b < 0x90) {
        Mm_TextEx((f32)x, (f32)y, str, scale, width, Mm_Rgba(0xFF, g + (0xFF - g) / 2, b + (0xFF - b) / 3, a), Mm_Rgba(r, g * 3 / 4, b / 2, a),
                  Mm_Rgba(0x5C, 0x24, 0x08, la));
    } else if (b >= r && b >= g && r + g + b >= 0x1E0) { /* a pale cool colour: nearly white, on dark blue */
        Mm_TextEx((f32)x, (f32)y, str, scale, width, Mm_Rgba(0xFF, 0xFF, 0xFF, a), Mm_Rgba((r + 0xFF) / 2, (g + 0xFF) / 2, (b + 0xFF) / 2, a),
                  Mm_Rgba(0x0C, 0x18, 0x50, la));
    } else {
        Mm_TextEx((f32)x, (f32)y, str, scale, width, Mm_Rgba((r + 0xFF) / 2, (g + 0xFF) / 2, (b + 0xFF) / 2, a), Mm_Rgba(r, g, b, a), Mm_Rgba(0x0C, 0x14, 0x40, la));
    }
}

void Mm_Text(s32 x, s32 y, char *str, f32 scale, s32 r, s32 g, s32 b, s32 a) {
    Mm_TextPlain(x, y, str, scale, 0.0f, r, g, b, a);
}

f32 Mm_Ease(f32 t) {
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    t = 1.0f - t;
    return 1.0f - t * t * t;
}

/* How far the screen has come in (0 at the start, 1 in place), `delay` frames late. */
f32 Mm_In(s32 delay) {
    return Mm_Ease((f32)(sMm->frames - delay) / 18.0f);
}

/* 0..1..0 over `period` frames. */
f32 Mm_Beat(s32 period) {
    s32 k = sMm->frames % period;

    return (f32)(k * 2 < period ? k * 2 : (period - k) * 2) / (f32)period;
}

/* A line squeezed (not below what can be read) to fit `width`. */
void Mm_TextFit(s32 x, s32 y, char *str, f32 scale, f32 width, s32 r, s32 g, s32 b, s32 a) {
    Mm_TextPlain(x, y, str, scale, width, r, g, b, a);
}

static s32 sMmFont;

/* The text of a frame is drawn between these two. */
void Mm_TextBegin(void) {
    sMmAlign = 0;
    sMmTick++;
}

void Mm_TextEnd(void) {
    sMmAlign = 0;
}

void Mm_SetMode(s32 mode) {
    /* the name screen has the PC keyboard as letters; everywhere else it is the pad */
    Port_TextMode(mode == MM_KEYBOARD); /* (a search line opens with its own button) */
    sMm->typing = 0;
    sMm->mode = mode;
    sMm->modeT = 0;
    sMm->rest = 0;
}

/* A line of news over the buttons for a second and a half. */
void Mm_Note(char *line) {
    s32 i;

    for (i = 0; i < 63 && line[i] != 0; i++) {
        sMm->noteLine[i] = line[i];
    }
    sMm->noteLine[i] = 0;
    sMm->note = 90;
}

/* The name of a fighter or a mod (PORT_REF_MOD + index); "-" for none. */
char *Mm_RefName(s32 ref) {
    static char name[MM_CHARA_COUNT][48];
    static u8 got[MM_CHARA_COUNT];

    if (ref >= PORT_REF_MOD && sMm != NULL && ref - PORT_REF_MOD < sMm->count) {
        return sMm->info[ref - PORT_REF_MOD].name;
    }
    if (ref < 0 || ref >= MM_CHARA_COUNT) {
        return "-";
    }
    if (!got[ref]) {
        Port_CharaName(ref, name[ref], 48);
        got[ref] = 1;
    }
    return name[ref];
}

/* ---- the mods ---- */

/* Whether `text` has `want` in it, any case. */
static s32 Mm_Has(char *text, char *want) {
    s32 i;
    s32 k;

    if (want[0] == 0) {
        return 1;
    }
    for (i = 0; text[i] != 0; i++) {
        for (k = 0; want[k] != 0; k++) {
            s32 a = (u8)text[i + k];
            s32 b = (u8)want[k];

            if (a >= 'a' && a <= 'z') {
                a -= 32;
            }
            if (b >= 'a' && b <= 'z') {
                b -= 32;
            }
            if (a != b) {
                break;
            }
        }
        if (want[k] == 0) {
            return 1;
        }
    }
    return 0;
}

/* Whether mod i needs a look: something wrong with its files, no fighter, or hidden by another. */
static s32 Mm_Trouble(PortModInfo *m) {
    return m->problem != 0 || m->chara < 0 || (m->conflict && !m->wins);
}

/* Whether mod a comes before mod b in the Library's order. */
static s32 Mm_Before(s32 a, s32 b) {
    PortModInfo *x = &sMm->info[a];
    PortModInfo *y = &sMm->info[b];
    s32 k;

    switch (sMm->sort) {
    case 1: /* by fighter */
        if (x->chara != y->chara) {
            return (u32)x->chara < (u32)y->chara;
        }
        break;
    case 2: /* the ones on first */
        if (x->on != y->on) {
            return x->on > y->on;
        }
        break;
    case 3: /* in the order they win in, then the ones off */
        if (x->on != y->on) {
            return x->on > y->on;
        }
        if (x->on && x->priority != y->priority) {
            return x->priority < y->priority;
        }
        break;
    }
    for (k = 0; x->name[k] != 0 && x->name[k] == y->name[k]; k++) {
    }
    return (u8)x->name[k] < (u8)y->name[k];
}

/* The Library's list: the mods its filter and search let through, in its order; the cursor stays on its mod when
   that is still there. */
static void Mm_BuildView(void) {
    s32 n = 0;
    s32 i;
    s32 j;

    for (i = 0; i < sMm->count; i++) {
        PortModInfo *m = &sMm->info[i];
        s32 ok;

        switch (sMm->filter) {
        case 1:
            ok = m->on;
            break;
        case 2:
            ok = !m->on;
            break;
        case 3:
            ok = !m->isNew;
            break;
        case 4:
            ok = m->isNew;
            break;
        case 5:
            ok = Mm_Trouble(m);
            break;
        default:
            ok = 1;
            break;
        }
        if (ok && sMm->findLen > 0) {
            ok = Mm_Has(m->name, sMm->find) || Mm_Has(m->target, sMm->find) || Mm_Has(m->white, sMm->find);
        }
        if (!ok) {
            continue;
        }
        for (j = n; j > 0 && Mm_Before(i, sMm->view[j - 1]); j--) {
            sMm->view[j] = sMm->view[j - 1];
        }
        sMm->view[j] = i;
        n++;
    }
    sMm->viewCount = n;
    for (j = 0; j < n && sMm->view[j] != sMm->cur; j++) {
    }
    if (j < n) {
        sMm->vcur = j;
    } else {
        if (sMm->vcur >= n) {
            sMm->vcur = n > 0 ? n - 1 : 0;
        }
        if (n > 0) {
            sMm->cur = sMm->view[sMm->vcur];
            sMm->costume = 0;
        }
    }
    if (sMm->vcur < sMm->top) {
        sMm->top = sMm->vcur;
    }
    if (sMm->vcur >= sMm->top + MM_ROWS) {
        sMm->top = sMm->vcur - MM_ROWS + 1;
    }
    if (sMm->top > 0 && sMm->top + MM_ROWS > n) {
        sMm->top = n > MM_ROWS ? n - MM_ROWS : 0;
    }
}

s32 Mm_OnCount(void) {
    s32 i;
    s32 n = 0;

    for (i = 0; i < sMm->count; i++) {
        n += sMm->info[i].on != 0;
    }
    return n;
}

void Mm_Refresh(void) {
    s32 i;

    sMmMovesOf = -1;
    sMm->aboutOf = -1;
    sMm->count = Port_ModCount();
    if (sMm->count > MM_MAX) {
        sMm->count = MM_MAX;
    }
    for (i = 0; i < sMm->count; i++) {
        Port_ModGetInfo(i, &sMm->info[i]);
    }
    if (sMm->cur >= sMm->count) {
        sMm->cur = sMm->count > 0 ? sMm->count - 1 : 0;
    }
    Mm_BuildView();
}

/* The cursor onto mod `mod` (the search and the filter are dropped when they hide it); the list scrolls to it. */
void Mm_Select(s32 mod) {
    s32 j;

    if (mod < 0 || mod >= sMm->count) {
        return;
    }
    sMm->cur = mod;
    Mm_BuildView();
    if (sMm->viewCount == 0 || sMm->view[sMm->vcur] != mod) {
        sMm->filter = 0;
        sMm->findLen = 0;
        sMm->find[0] = 0;
        Mm_BuildView();
    }
    for (j = 0; j < sMm->viewCount && sMm->view[j] != mod; j++) {
    }
    sMm->vcur = j < sMm->viewCount ? j : 0;
    sMm->top = sMm->vcur >= MM_ROWS ? sMm->vcur - MM_ROWS + 1 : 0;
    sMm->curY = MM_LIST_Y + MM_PITCH * (f32)(sMm->vcur - sMm->top);
    sMm->costume = 0;
    sMm->rest = 0;
}

void Mm_FreeFaces(void) {
    s32 i;

    for (i = 0; i < MM_MAX; i++) {
        if (sMm->face[i] != NULL) {
            Heap_Free(sMm->face[i]);
            sMm->face[i] = NULL;
        }
        if (sMm->chip[i] != NULL) {
            Heap_Free(sMm->chip[i]);
            sMm->chip[i] = NULL;
        }
        sMm->faceTried[i] = 0;
        sMm->chipTried[i] = 0;
    }
}

/* The chip of mod i: the square of its portrait that is its face on the grids (the library builds it). */
MmTex *Mm_Chip(s32 i) {
    if (i < 0 || i >= MM_MAX) {
        return NULL;
    }
    if (!sMm->chipTried[i]) {
        MmRes *res = Heap_Alloc(0x1600, 0x40, 0, 2);

        sMm->chipTried[i] = 1;
        if (res == NULL) {
            return NULL;
        }
        if (Port_ModChip(i, res, 0x1600) > 0) {
            Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
            sMm->chip[i] = res;
        } else {
            Heap_Free(res);
        }
    }
    return sMm->chip[i] != NULL ? sMm->chip[i]->tex : NULL;
}

/* The eyes of the fighter mod i replaces: the picture the battle gauge shows (file 8 + character * 2), read when the
   plate first comes on screen - through the game's file layer, so a mod's own picture shows once the mod is on. */
MmTex *Mm_Face(s32 i) {
    MmRes *res;

    if (!sMm->faceTried[i]) {
        sMm->faceTried[i] = 1;
        if (sMm->info[i].chara >= 0) {
            res = File_LoadSync(8 + sMm->info[i].chara * 2, NULL, 0);
            if (res != NULL) {
                Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
                sMm->face[i] = res;
            }
        }
    }
    return sMm->face[i] != NULL ? sMm->face[i]->tex : NULL;
}

/* Whether mod i has a model for costume n to show. */
s32 Mm_HasCostume(s32 i, s32 n) {
    PortModInfo *m = &sMm->info[i];

    if (m->kinds & (1 << n)) {
        return 1;
    }
    return !m->raw && (m->kinds & 1); /* a costume the mod lacks is answered with its first */
}

/* ---- the model ---- */

/* One frame of the 3D scene: what the viewer's own frame function does, without its room and without the pad. */
extern void StgAmb_SetBgmScale(f32 scale);
extern void StgAmb_SetSeScale(f32 scale);
extern void MapBgm_Stop(void);
extern s32 MapBgm_IsStopped(void);

static void Mm_Scene(void) {
    OrbitCam_Update(0);
    gChrView->stageOn = 0;
    gBtlStage->flags |= 1;
    BtlObj_UpdateAll();
    if (ChrView_IsVisible()) {
        BtlObj_UpdateVisibility(0);
        BtlObj_FinishVisibility(0);
        StgAmb_SetBgmScale(0.0f); /* the viewer's stage is silent: its wind and its sounds (the next stage that starts sets them again) */
        StgAmb_SetSeScale(0.0f);
        BtlStage_Update();
        if (!MapBgm_IsStopped()) {
            MapBgm_Stop();
        }
        BtlScene_Update();
        Gfx_MarkPass(2);
        BtlObjDraw_Draw();
        Gfx_MarkPass(0);
        Gfx_MarkPass(4);
        Gfx_AddDefaultEnv();
        BtlScene_Draw(1);
        Ot_Draw();
        Gfx_MarkPass(0);
    }
}

/* The model on the platform: what the screen that is up wants (sMm->want*), once the cursor has rested, standing where
   that screen's mockup has it (Mm_SetModel: its feet and its height, in the mockup's pixels), turning slowly. */
static f32 sMmPlace[3] = {785.0f, 578.0f, 410.0f};
extern int Port_PageAspectMilli(void);
extern double atof(const char *s);

/* ---- the models beside the viewer's (the forms screens: before and after, the partners of a fusion) ----
 * The viewer shows one model; these are more battle objects of the same kind, read into buffers of their own the
 * same way (one read at a time, never while the viewer reads), drawn with it by the same scene, under the same
 * camera: each is moved and sized from where the viewer's model stands to where its screen wants it. */
#define MM_EXTRAS 2
typedef struct MmFile {
    void *buf;
    s32 size;
    s32 id;
    s32 pad;
} MmFile;
typedef struct MmExtra {
    s32 want;   /* what the screen wants this frame: a character id or PORT_REF_MOD + mod, -1 none */
    s32 shown;  /* what it is now (-1 none) */
    s32 reading; /* 1 while its file is read */
    s32 obj;    /* its battle object, -1 none */
    f32 place[3]; /* its feet (x, y) and height, mockup pixels */
    f32 base;   /* its own scale */
    MmFile file[3];
} MmExtra;
static MmExtra sMmExtra[MM_EXTRAS];
static s32 sMmExtraMotion = -1; /* the model motion they play (a fusion's dance), -1 their stance */
/* Outside the manager (the viewer for another screen: Mm_ViewPair): each one's costume, the mod whose files it is
   read with (-1 the game's), its turn. */
static s32 sMvExtraCostume[2];
static s32 sMvExtraMod[2] = {-1, -1};
static f32 sMvExtraYaw[2];
static s32 sMvExtraKey = -1; /* what the second model of a pair is (Mm_ViewPair's key) */
static s32 sMvMod = -1;      /* the mod whose files the viewer's model is read with */
static f32 sMmGlow[3];          /* an aura behind a model: its feet, its height; */
static u32 sMmGlowRgb;          /* its colour, alpha 0 none */
static s32 sMmGlowBolts;        /* lightning in it */

void Mm_SetGlowBolts(s32 on) {
    sMmGlowBolts = on;
}

void Mm_SetExtraMotion(s32 motion) {
    sMmExtraMotion = motion;
}

void Mm_SetGlow(f32 cx, f32 feet, f32 height, u32 c) {
    sMmGlow[0] = cx;
    sMmGlow[1] = feet;
    sMmGlow[2] = height;
    sMmGlowRgb = c;
}
extern void BtlRes_LoadSingle(s32 fromHeap, void *slot, s32 model);
extern s32 BtlObj_Create(s32 type, void *res, s32 active);
extern s32 BtlObj_Destroy(s32 id);
extern void BtlObjAnim_PlayModel(u8 *obj, s32 anim, s32 mode);
extern s32 ChrView_GetModelFile(s32 chara, s32 costume, s32 damaged);
extern s32 File_UpdateRequests(void);
extern s32 File_GetSize(s32 id);
#ifdef __ANDROID__
#define PORT_MODVIEW_MODEL_SIZE 0x1A0000 /* battle/view_a.h's (not included here) */
#else
#define PORT_MODVIEW_MODEL_SIZE 0x900000
#endif

/* The screen wants model k beside the viewer's: who, its feet and how tall (called every frame it does). */
void Mm_SetExtra(s32 k, s32 ref, f32 cx, f32 feet, f32 height) {
    if (k >= 0 && k < MM_EXTRAS) {
        sMmExtra[k].want = ref;
        sMmExtra[k].place[0] = cx;
        sMmExtra[k].place[1] = feet;
        sMmExtra[k].place[2] = height;
    }
}

static void Mm_ExtraDrop(MmExtra *e) {
    if (e->obj >= 0) {
        BtlObj_Destroy(e->obj);
        e->obj = -1;
    }
    e->shown = -1;
}

static void Mm_ExtrasInit(void) {
    s32 k;

    memset(sMmExtra, 0, sizeof(sMmExtra));
    for (k = 0; k < MM_EXTRAS; k++) {
        sMmExtra[k].want = -1;
        sMmExtra[k].shown = -1;
        sMmExtra[k].obj = -1;
        sMmExtra[k].file[0].id = -1;
    }
}

/* Every model of these off the screen (before the library changes under them, or the viewer goes). */
static void Mm_ExtrasHide(void) {
    s32 k;

    for (k = 0; k < MM_EXTRAS; k++) {
        if (sMmExtra[k].reading) {
            File_CancelRequests();
            sMmExtra[k].reading = 0;
        }
        Mm_ExtraDrop(&sMmExtra[k]);
    }
}

static void Mm_ExtrasTerm(void) {
    s32 k;

    Mm_ExtrasHide();
    for (k = 0; k < MM_EXTRAS; k++) {
        if (sMmExtra[k].file[0].buf != NULL) {
            Heap_Free(sMmExtra[k].file[0].buf);
            sMmExtra[k].file[0].buf = NULL;
        }
    }
}

/* Reads what the screens want, one at a time; then places each over the camera the viewer's model set. */
static void Mm_ExtrasUpdate(f32 dist, f32 mainFeet, f32 mainH) {
    s32 k;
    s32 busy = ChrView_IsLoading();

    for (k = 0; k < MM_EXTRAS; k++) {
        busy |= sMmExtra[k].reading;
    }
    for (k = 0; k < MM_EXTRAS; k++) {
        MmExtra *e = &sMmExtra[k];

        if (e->reading) {
            if (File_UpdateRequests() != 0) {
                u8 *o;

                e->reading = 0;
                e->obj = BtlObj_Create(3, e->file, 1);
                Port_ClashLog((int)k, "rbdbg obj", 0, (int)e->obj, 0); /* DBG */
                o = BtlObj_Get(e->obj);
                if (o != NULL) {
                    f32 *m = (f32 *)(o + 0x9A0);

                    BtlObjAnim_PlayModel(o, sMmExtraMotion >= 0 ? sMmExtraMotion : 7, 2);
                    *(u32 *)(o + 0xA40) &= ~0x02000000;
                    e->base = sqrtf(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
                }
                if (sMm != NULL) {
                    Port_ModPreview(sMm->wantKey >= 0 ? sMm->wantPreview : -1); /* (the viewer's own files answer again) */
                } else {
                    Port_ModPreview(sMvMod);
                }
            }
            continue;
        }
        if (e->want == e->shown) {
            continue;
        }
        if (e->want < 0) {
            Mm_ExtraDrop(e);
            continue;
        }
        if (busy || (sMm != NULL && sMm->rest < MM_SHOW_DELAY)) {
            continue;
        }
        { /* read it: the file of the fighter (a mod's: its files answer while it is read) */
            s32 chara = e->want;
            s32 preview = sMm == NULL ? sMvExtraMod[k] : -1;

            if (sMm != NULL && e->want >= PORT_REF_MOD) {
                chara = sMm->info[e->want - PORT_REF_MOD].chara;
                preview = e->want - PORT_REF_MOD;
            }
            Mm_ExtraDrop(e);
            if (chara < 0 || chara >= MM_CHARA_COUNT) {
                e->shown = e->want;
                continue;
            }
            if (e->file[0].buf == NULL) {
                e->file[0].buf = Heap_Alloc(PORT_MODVIEW_MODEL_SIZE, 0x40, 0, 2);
                if (e->file[0].buf == NULL) {
                    e->shown = e->want;
                    continue;
                }
            }
            e->file[0].size = 0;
            e->file[0].id = -1;
            *(s32 *)e->file[0].buf = 0;
            Port_ModPreview(preview);
            if (File_GetSize(ChrView_GetModelFile(chara, sMm == NULL ? sMvExtraCostume[k] : 0, 0)) > PORT_MODVIEW_MODEL_SIZE) {
                Port_ModPreview(-1);
                e->shown = e->want; /* (larger than the buffer: not shown) */
                continue;
            }
            BtlRes_LoadSingle(0, e->file, ChrView_GetModelFile(chara, sMm == NULL ? sMvExtraCostume[k] : 0, 0));
            Port_ClashLog((int)k, "rbdbg read", 0, (int)chara, 0); /* DBG */
            e->reading = 1;
            e->shown = e->want;
            busy = 1;
        }
    }
    /* their places: across as the viewer's model is placed; up or down by how far their feet are from its feet; their
       size against its size (they are drawn with its camera) */
    for (k = 0; k < MM_EXTRAS; k++) {
        MmExtra *e = &sMmExtra[k];
        u8 *o = e->obj >= 0 ? BtlObj_Get(e->obj) : NULL;
        f32 *m;
        f32 s;
        f32 ox;
        f32 oy;
        f32 c;
        f32 sn;
        f32 narrow = 1333.0f / (f32)Port_PageAspectMilli();

        if (o == NULL) {
            continue;
        }
        m = (f32 *)(o + 0x9A0);
        s = e->base * Mx_SizeFracY(e->place[2]) / (mainH > 0.01f ? mainH : 0.01f);
        ox = -(Mx_ScreenFracX(e->place[0]) - 0.492f) / 0.76f * dist;
        oy = (Mx_ScreenFracY(e->place[1]) - mainFeet) / 1.115f * dist;
        { /* (turning with the viewer's model; still, facing the camera, while they dance) */
            f32 yaw = sMm == NULL ? sMvExtraYaw[k] : sMmExtraMotion >= 0 ? 0.0f : sMm->yaw + 0.5f * (f32)(k + 1);

            c = cosf(yaw);
            sn = sinf(yaw);
            *(f32 *)(o + 0x964) = yaw;
        }
        *(f32 *)(o + 0x950) = ox;
        *(f32 *)(o + 0x954) = oy;
        m[0] = c * s * narrow;
        m[1] = 0.0f;
        m[2] = -sn * s;
        m[4] = 0.0f;
        m[5] = s;
        m[6] = 0.0f;
        m[8] = sn * s * narrow;
        m[9] = 0.0f;
        m[10] = c * s;
        m[12] = ox;
        m[13] = oy;
    }
}

/* With no model of the viewer's they are not drawn (its camera is not set for them). */
static void Mm_ExtrasHideObjs(void) {
    s32 k;

    for (k = 0; k < MM_EXTRAS; k++) {
        u8 *o = sMmExtra[k].obj >= 0 ? BtlObj_Get(sMmExtra[k].obj) : NULL;

        if (o != NULL) {
            *(f32 *)(o + 0x954) = -10000.0f;
            ((f32 *)(o + 0x9A0))[13] = -10000.0f;
        }
    }
}

/* Each frame, before the screen says what it wants: nothing beside the viewer's model. */
void Mm_ExtrasReload(void) {
    s32 k;

    for (k = 0; k < MM_EXTRAS; k++) {
        if (!sMmExtra[k].reading) {
            Mm_ExtraDrop(&sMmExtra[k]);
        }
    }
}

static void Mm_ExtrasClear(void) {
    s32 k;

    sMmExtraMotion = -1;
    sMmGlowRgb = 0;
    sMmGlowBolts = 0;
    for (k = 0; k < MM_EXTRAS; k++) {
        sMmExtra[k].want = -1;
    }
}

void Mm_SetModel(f32 cx, f32 feet, f32 height) {
    sMmPlace[0] = cx;
    sMmPlace[1] = feet;
    sMmPlace[2] = height;
}

/* The light under the model, before the 3D scene. */
static void Mm_DrawBehind(void) {
    if (sMmGlowRgb >> 24 != 0 && sMm->shown >= 0) { /* (the aura first: the platform and the model over it) */
        Mx_Glow(sMmGlow[0], sMmGlow[1], sMmGlow[2], sMmGlowRgb, sMm->frames);
        if (sMmGlowBolts) {
            Mx_Bolts(sMmGlow[0], sMmGlow[1], sMmGlow[2], sMm->frames);
        }
    }
    if (sMm->wantKey >= 0 && !sMm->importing && !(sMm->mode == MM_LIBRARY && sMm->confirm)) {
        Mx_Platform(sMmPlace[0], sMmPlace[1] + Mx_LenY(4.0f), sMmPlace[2] * 0.82f);
        if (sMm->mode == MM_TRANSFORM && sMm->trPage == 0 && sMmExtra[0].want >= 0) { /* (the "before" stands on one too) */
            Mx_Platform(sMmExtra[0].place[0], sMmExtra[0].place[1] + Mx_LenY(4.0f), sMmExtra[0].place[2] * 0.82f);
        }
    }
}

/* Puts the viewer's model (`obj`, of the character `ci` is about) where `place` says: its feet (x, y) and its height,
   in the mockup's pixels; turned by *yaw, which goes on turning when `spin`. The camera's distance, where the feet are and how tall it
   is, as parts of the picture, for the models beside it. */
static f32 sPlacePitch; /* (the viewer's camera tilted by hand: Mm_ViewCamera) */
f32 gMmViewScale = 1.0f; /* (the model's height against its fighter's: Z Warriors' giants framed at a fighter's size) */

static void Mm_PlaceModel(MmCharaInfo *ci, u8 *obj, const f32 *place, f32 *yaw, s32 spin, f32 *outDist, f32 *outFy, f32 *outHf) {
    f32 fx = Mx_ScreenFracX(place[0]);
    f32 fy = Mx_ScreenFracY(place[1]) + Mx_SizeFracY(24.0f); /* (on the platform, not above it) */
    f32 hf = Mx_SizeFracY(place[2]);
    /* measured (BT3_MM_RAW in Mm_ModelUpdate): a fighter is 0.647 / k of the picture high at k times its viewer's
       distance; its feet are at 0.4755 + 0.662 x that height, 1.115 higher per distance the camera looks lower (the
       game's y is down); a step across of one distance moves it 0.76 of the picture, from 0.492 */
    f32 dist = ci->dist * 0.647f / (hf > 0.05f ? hf : 0.05f) * gMmViewScale;
    f32 ox = -(fx - 0.492f) / 0.76f * dist;
    f32 ty = (0.4755f + 0.662f * hf - fy) / 1.115f;
    f32 narrow = 1333.0f / (f32)Port_PageAspectMilli(); /* (the menus' 3D is stretched to the picture's width) */

        /* the camera looks so that the feet are where the mockup has them; the fighter itself is moved across (the
           camera's +x is the picture's left) and turned */
        OrbitCam_SetDistRange(dist * 0.5f, dist * 2.0f);
        OrbitCam_SetDist(dist);
        OrbitCam_SetTarget(0.0f, ci->targetY + ty * dist, 0.0f);
        OrbitCam_SetAngles(sPlacePitch, 3.1415927f, 0.0f);
        if (obj != NULL) {
            /* its place and its turn, in the pose and in the matrix made from it (rows: right, up, forward, place);
               the rows keep the lengths they have */
            f32 *m = (f32 *)(obj + 0x9A0);
            f32 sx = sqrtf(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]); /* (its scale, from the row that is never narrowed) */
            f32 sz = sx;
            f32 c;
            f32 sn;

            *yaw += spin ? 0.014f : 0.0f;
            if (*yaw > 6.2831853f) {
                *yaw -= 6.2831853f;
            }
            c = cosf(*yaw);
            sn = sinf(*yaw);
            *(f32 *)(obj + 0x950) = ox;
            *(f32 *)(obj + 0x964) = *yaw;
            m[0] = c * sx * narrow;
            m[1] = 0.0f;
            m[2] = -sn * sx;
            m[8] = sn * sz * narrow;
            m[9] = 0.0f;
            m[10] = c * sz;
            m[12] = ox;
        }
    *outDist = dist;
    *outFy = fy;
    *outHf = hf;
}

static void Mm_ModelUpdate(void) {
    if (sMm->rest >= MM_SHOW_DELAY && sMm->wantKey != sMm->shown && !ChrView_IsLoading()) {
        if (sMm->wantKey < 0) {
            ChrView_Hide();
            Port_ModPreview(-1);
        } else {
            Port_ModPreview(sMm->wantPreview);
            ChrView_Show(sMm->wantChara, sMm->wantCostume, 0);
            sMm->shownChara = sMm->wantChara;
        }
        sMm->shown = sMm->wantKey;
    }
    if (sMm->shown >= 0 && ChrView_IsVisible() && gChrView->cur != NULL) {
        MmCharaInfo *ci = ChrView_GetCharaInfo((u32)sMm->shownChara);
        f32 dist;
        f32 fy;
        f32 hf;
        u8 *obj = BtlObj_Get(gChrView->cur->obj);

        if (getenv("BT3_MM_RAW") != NULL) { /* (tuning: known settings in turn, a second each) */
            static const f32 raw[4][3] = {{1.55f, 0.0f, 0.0f}, {1.55f, -0.3f, 0.0f}, {1.55f, 0.0f, 0.2f}, {2.5f, 0.0f, 0.0f}};
            s32 c = (sMm->frames / 60) % 4;

            f32 ox;

            dist = ci->dist * raw[c][0];
            ox = raw[c][1] * dist;
            OrbitCam_SetDistRange(dist * 0.5f, dist * 2.0f);
            OrbitCam_SetDist(dist);
            OrbitCam_SetTarget(0.0f, ci->targetY + raw[c][2] * dist, 0.0f);
            OrbitCam_SetAngles(0.0f, 3.1415927f, 0.0f);
            sMm->yaw = -0.014f;
            if (obj != NULL) {
                f32 *m = (f32 *)(obj + 0x9A0);

                *(f32 *)(obj + 0x950) = ox;
                m[12] = ox;
            }
            return;
        }
        Mm_PlaceModel(ci, obj, sMmPlace, &sMm->yaw, 1, &dist, &fy, &hf);
        Mm_ExtrasUpdate(dist, fy, hf);
    } else {
        Mm_ExtrasUpdate(1.0f, 0.5f, 0.3f);
        Mm_ExtrasHideObjs();
    }
}

/* ---- the viewer for another screen (the mod grid of the character select) ----
 * The same model, read and placed the same way, without the manager open: Mm_ViewOpen when the screen opens, what it
 * wants each frame (Mm_ViewWant), Mm_ViewDraw where the model goes in its drawing, Mm_ViewClose when it closes. */
#define MM_VIEW_YAW 3.59f /* (facing the screen, turned a little: 0 is its back to the camera; it stays so) */
static s32 sMvShown = -1;
static s32 sMvChara;
static f32 sMvYaw;
static f32 sMvPitch;  /* (Mm_ViewCamera: the camera tilted, the model turned by hand) */
static s32 sMvHand;
extern void *gPortTPose;

extern void ChrView_PortInitAsync(void);
extern s32 ChrView_PortInitDone(void);
extern void ChrView_PortTermKeepMusic(void);
extern void Job_Clear(void);

/* (the viewer's backdrop is read in the background: no loading screen, the model comes when it is in) */
void Mm_ViewOpen(void) {
    sMvShown = -1;
    sMvYaw = 0.0f;
    sMvMod = -1;
    sMvExtraKey = -1;
    ChrView_PortInitAsync();
    Mm_ExtrasInit();
}

void Mm_ViewClose(void) {
    gPortTPose = NULL;
    sMvHand = 0;
    sMvPitch = 0.0f;
    { /* (whatever the viewer is still reading, its stage at the start or a model, is finished first: freed under a read
         still going, its buffers were written after they were someone else's, and the game fell over later, in the
         main menu) */
        s32 n;

        for (n = 0; n < 300 && gChrView != NULL && (!ChrView_PortInitDone() || ChrView_IsLoading()); n++) {
            Job_Run();
            Gfx_BeginFrame();
            File_Stub264D90();
            Gfx_EndFrame(1);
            Dma_Flush();
        }
    }
    if (ChrView_PortInitDone()) {
        ChrView_Hide();
    }
    Mm_ExtrasTerm();
    Job_Clear();
    Port_ModPreview(-1);
    ChrView_PortTermKeepMusic(); /* (the character select's music goes on) */
    sMvShown = -1;
}

/* key: what it is (a different key reads again; -1 none), shown once `rested`: the fighter `chara` in `costume`,
   with the files of mod `mod`. */
void Mm_ViewWant(s32 key, s32 chara, s32 costume, s32 mod, s32 rested) {
    if (!ChrView_PortInitDone()) {
        return;
    }
    Job_Run(); /* (the viewer's reads) */
    if (key == sMvShown || ChrView_IsLoading() || (key >= 0 && !rested)) {
        return;
    }
    if (key < 0) {
        ChrView_Hide();
        Port_ModPreview(-1);
    } else {
        Port_ModPreview(mod);
        sMvMod = mod;
        ChrView_Show(chara, costume, 0);
        sMvChara = chara;
        sMvYaw = MM_VIEW_YAW;
    }
    sMvShown = key;
}

/* 1 while the model is there to be drawn. */
s32 Mm_ViewReady(void) {
    return ChrView_PortInitDone() && sMvShown >= 0 && ChrView_IsVisible() && gChrView->cur != NULL && !ChrView_IsLoading();
}

/* Two models (a select's two sides): the first is the viewer's (Mm_ViewWant), the second one of the models beside it.
   Each: a key (another key reads again; -1 none), the fighter, its costume, the mod whose files it has (-1 the
   game's); shown once `rested`. Mm_ViewPairDraw draws them: feet (x, y) and height of each, mockup pixels, and turns. */
void Mm_ViewPair(s32 key0, s32 chara0, s32 costume0, s32 mod0, s32 key1, s32 chara1, s32 costume1, s32 mod1, s32 rested) {
    Mm_ViewWant(key0, chara0, costume0, mod0, rested);
    if (sMm == NULL) { Port_ClashLog((int)key1, "rbdbg pair", 0, (int)sMmExtra[0].want * 1000 + (int)sMmExtra[0].shown, (int)sMmExtra[0].reading); } /* DBG */
    if (sMvExtraKey != key1 && !sMmExtra[0].reading && rested) {
        sMvExtraKey = key1;
        sMmExtra[0].want = key1 < 0 ? -1 : chara1;
        sMvExtraCostume[0] = costume1;
        sMvExtraMod[0] = mod1;
        sMmExtra[0].shown = -2; /* (read again: the key says it changed, not the fighter alone) */
    }
}

void Mm_ViewPairDraw(const f32 *place0, f32 yaw0, const f32 *place1, f32 yaw1) {
    f32 dist;
    f32 fy;
    f32 hf;

    if (!Mm_ViewReady()) {
        return;
    }
    sMvYaw = yaw0;
    Mm_PlaceModel(ChrView_GetCharaInfo((u32)sMvChara), BtlObj_Get(gChrView->cur->obj), place0, &sMvYaw, 0, &dist, &fy, &hf);
    sMmExtra[0].place[0] = place1[0];
    sMmExtra[0].place[1] = place1[1];
    sMmExtra[0].place[2] = place1[2];
    sMvExtraYaw[0] = yaw1;
    Mm_ExtrasUpdate(dist, fy, hf);
    Mm_Scene();
    Mm_TexReset();
}

/* The viewer's model: its object id (BtlObj_Get), -1 none; the object itself, NULL none. */
s32 Mm_ViewObjId(void) {
    return Mm_ViewReady() ? (s32)gChrView->cur->obj : -1;
}

void *Mm_ViewObjPtr(void) {
    return Mm_ViewReady() ? (void *)BtlObj_Get(gChrView->cur->obj) : NULL;
}

/* What the model drawn now is (Mm_ViewWant's key), -1 none yet. */
s32 Mm_ViewKey(void) {
    return Mm_ViewReady() ? sMvShown : -1;
}

/* The viewer's camera by hand: the model turned to `yaw` (radians, 3.59 faces the screen), the camera tilted by
   `pitch` (radians, + looks from above), the model in its bind pose (T-pose) when `tpose`. */
void Mm_ViewCamera(f32 yaw, f32 pitch, s32 tpose) {
    sMvHand = 1;
    sMvYaw = yaw;
    sMvPitch = pitch;
    gPortTPose = tpose && Mm_ViewReady() ? (void *)BtlObj_Get(gChrView->cur->obj) : NULL;
}

/* The texture file of the model on screen (the battle object's model block at +0x18, its relocated texture file at
   +0x30 there: include/sys/gfxm_e.h ObjMdl), NULL when there is none. Z Warriors lights a colour zone in it. */
void *Mm_ViewTex(void) {
    if (!Mm_ViewReady()) {
        return NULL;
    }
    return *(void **)(BtlObj_Get(gChrView->cur->obj) + 0x18 + 0x30);
}

/* The expression texture files of the model on screen (the model block's eyes at +0x34, mouths at +0x64 + 4..7). */
void *Mm_ViewExpr(s32 i) {
    if (!Mm_ViewReady() || i < 0 || i > 4) {
        return NULL;
    }
    return *(void **)(BtlObj_Get(gChrView->cur->obj) + 0x18 + (i == 0 ? 0x34 : 0x64 + (3 + i) * 4));
}

/* The model file of the model on screen (the battle object's model block at +0x18 starts with it), NULL none. */
void *Mm_ViewModel(void) {
    if (!Mm_ViewReady()) {
        return NULL;
    }
    return *(void **)(BtlObj_Get(gChrView->cur->obj) + 0x18);
}

/* The model, with its feet at (cx, feet) and `height` tall (mockup pixels; the feet may be below the screen: a close
   shot), turned one way and still. */
void Mm_ViewDraw(f32 cx, f32 feet, f32 height) {
    f32 place[3];
    f32 dist;
    f32 fy;
    f32 hf;

    if (!Mm_ViewReady()) {
        return;
    }
    place[0] = cx;
    place[1] = feet;
    place[2] = height;
    sPlacePitch = sMvHand ? sMvPitch : 0.0f;
    Mm_PlaceModel(ChrView_GetCharaInfo((u32)sMvChara), BtlObj_Get(gChrView->cur->obj), place, &sMvYaw, 0, &dist, &fy, &hf);
    sPlacePitch = 0.0f;
    Mm_Scene();
    Mm_TexReset();
}

/* ---- the screen ----
 * Laid out as the approved mockups (modmgr_design/mockups/01_library.jpg and on), in their pixels (port_mmkit.c). */

/* What is wrong with a mod's files, in words (PortModInfo.problem). */
static char *Mm_ProblemText(s32 problem) {
    switch (problem) {
    case 1:
        return "A FILE IS DAMAGED (NOT A REAL PAK)";
    case 2:
        return "A FILE IS TOO BIG FOR THE GAME";
    case 3:
        return "A FILE IS EMPTY";
    case 4:
        return "ITS PORTRAIT CAN'T BE READ (SKIPPED)";
    case 5:
        return "SEVERAL FIGHTERS IN ONE: RE-ADD IT";
    }
    return "";
}

/* Whether a mod's problem stops it from being fought with (a portrait the game leaves out does not). */
static s32 Mm_Broken(PortModInfo *m) {
    return m->problem >= 1 && m->problem <= 3;
}

/* The maker's lines of the mod under the cursor: author, version, description. */
static char *Mm_About(s32 part) {
    if (sMm->aboutOf != sMm->cur) {
        Port_ModAbout(sMm->cur, sMm->about);
        sMm->about[31] = 0;
        sMm->about[47] = 0;
        sMm->about[247] = 0;
        sMm->aboutOf = sMm->cur;
    }
    return sMm->about + (part == 0 ? 0 : part == 1 ? 32 : 48);
}

static char *sMmShowName[MM_SHOWS] = {"ALL", "ON ONLY", "OFF ONLY", "REPLACERS", "NEW FIGHTERS", "PROBLEMS"};
static char *sMmSortName[MM_SORTS] = {"NAME", "FIGHTER", "ON FIRST", "PRIORITY"};

/* The Library's list (mockup 01): a bar per mod - its face, its name, what it replaces, ON / OFF - the cursor's gold. */
#define LIB_X0 62.0f
#define LIB_X1 595.0f
#define LIB_H 64.0f

/* An ON / OFF capsule centred on (cx, cy). */
static void Lib_OnOff(s32 on, f32 cx, f32 cy) {
    if (on) {
        Mx_Bar(MM_TAB_SEL, cx - 44.0f, cy - 16.0f, cx + 44.0f, cy + 16.0f, 0x80);
        Mx_TextC(cx, cy, "ON", 24.0f, 0.0f, Mx_Rgba(0xFF, 0xFF, 0xF0, 0x80), Mx_Rgba(0xFF, 0xF0, 0xC8, 0x80), Mx_Rgba(0x6C, 0x3C, 0x08, 0x80), 1, 0x80);
    } else {
        Mx_Bar(MM_BAR_GREY, cx - 44.0f, cy - 16.0f, cx + 44.0f, cy + 16.0f, 0x80);
        Mx_Text(cx, cy, "OFF", 24.0f, 0.0f, MX_WHITE, 1);
    }
}

/* What a mod is, in a few words: "Replaces Goku (GT)", "New fighter, like Vegeta". */
static char *Lib_Sub(PortModInfo *m) {
    static char line[96];

    if (m->chara < 0) {
        return "No fighter set: pick one in Edit details";
    }
    if (m->isNew) {
        sprintf(line, "New fighter, like %.60s", m->target);
    } else {
        sprintf(line, "Replaces %.70s", m->target);
    }
    return line;
}

static void Lib_Row(s32 at, f32 y0, f32 slide) {
    s32 i = sMm->view[at];
    PortModInfo *m = &sMm->info[i];
    s32 sel = at == sMm->vcur;
    f32 x0 = LIB_X0 + slide;
    f32 x1 = LIB_X1 + slide;
    s32 a = m->on || sel ? 0x80 : 0x6C;

    if (sel && sMm->flash > 0) {
        f32 f = (f32)sMm->flash;

        Mx_Box(MM_BAR_SEL, x0 - 4.0f - f, y0 - 3.0f - f * 0.5f, x1 + 4.0f + f, y0 + LIB_H + 3.0f + f * 0.5f, LIB_H * 0.4f, sMm->flash * 6);
    }
    Mx_Row(x0, y0, x1, y0 + LIB_H, sel, a);
    Mx_FaceCell(PORT_REF_MOD + i, x0 + 30.0f, y0 + 8.0f, x0 + 78.0f, y0 + 56.0f, 0);
    Mx_TextA(x0 + 92.0f, y0 + 20.0f, m->name, 24.0f, x1 - x0 - 92.0f - 120.0f, MX_LAV, 0, a);
    Mx_TextA(x0 + 92.0f, y0 + 46.0f, Lib_Sub(m), 17.0f, x1 - x0 - 92.0f - 120.0f, m->chara < 0 ? MX_ORANGE : MX_LAV, 0, a);
    Lib_OnOff(m->on, x1 - 72.0f, y0 + LIB_H * 0.5f);
    if (Mm_Broken(m) || m->conflict || m->problem != 0) { /* (something to look at: a mark at the row's left) */
        f32 cy = y0 + LIB_H * 0.5f;

        Mx_Box(MM_KEY, LIB_X0 - 44.0f + slide, cy - 15.0f, LIB_X0 - 14.0f + slide, cy + 15.0f, 8.0f, 0x80);
        Mx_Text(LIB_X0 - 29.0f + slide, cy, "!", 22.0f, 0.0f, Mm_Broken(m) ? MX_RED : MX_GOLD, 1);
    }
}

/* The blasts of the mod under the cursor: a box with its tab, a ball and a name a line (two columns when there are
   more than three). A name in letters the font has not is left out. */
static void Lib_Blasts(f32 x0, f32 y0, f32 x1, f32 y1) {
    char *name[5];
    s32 n = 0;
    s32 k;

    for (k = 0; k < sMmMoveCount && sMmMovesOf == sMm->cur && k < 5; k++) {
        s32 c;

        for (c = 0; sMmMoves[k][c] != 0 && sMmMoves[k][c] != '?'; c++) {
        }
        if (sMmMoves[k][c] == 0 && c > 0) {
            name[n++] = sMmMoves[k];
        }
    }
    Mx_Box(MM_WINDOW_DARK, x0, y0 + 16.0f, x1, y1, 20.0f, 0x80);
    Mx_Box(MM_TAB_SEL, x0 + 18.0f, y0, x0 + 168.0f, y0 + 34.0f, 15.0f, 0x80);
    Mx_TextC(x0 + 93.0f, y0 + 17.0f, "BLASTS", 22.0f, 0.0f, Mx_Rgba(0xFF, 0xF6, 0xD8, 0x80), Mx_Rgba(0xFF, 0xE0, 0x90, 0x80), Mx_Rgba(0x5C, 0x30, 0x08, 0x80), 1, 0x80);
    if (sMmMovesOf != sMm->cur) {
        return;
    }
    if (n == 0) {
        Mx_Say((x0 + x1) * 0.5f, (y0 + 16.0f + y1) * 0.5f + 8.0f, sMmMoveCount == 0 ? "The fighter's own" : "Names the font cannot show", 21.0f, x1 - x0 - 40.0f, MX_DIM, 1);
        return;
    }
    for (k = 0; k < n; k++) {
        s32 two = 0;
        f32 colW = two ? (x1 - x0 - 30.0f) * 0.5f : x1 - x0 - 30.0f;
        f32 x = x0 + 18.0f + (two && k >= (n + 1) / 2 ? colW + 6.0f : 0.0f);
        s32 row = two ? k % ((n + 1) / 2) : k;
        f32 cy = y0 + 54.0f + 26.0f * (f32)row;

        Mx_Fit(MM_BALL, x + 10.0f, cy, 17.0f, 0x80);
        Mx_Text(x + 26.0f, cy, name[k], two ? 18.0f : 20.0f, colW - 36.0f, MX_WHITE, 0);
    }
}

/* The window at the right: what the mod under the cursor is (mockup 01). */
static void Lib_Info(f32 rise) {
    static char line[128];
    static char lines[4][64];
    static const s32 bit[3] = {0x100, 0x200, 0x400};
    static char *kindName[3] = {"ANIMS", "EFFECTS", "PORTRAIT"};
    PortModInfo *m = &sMm->info[sMm->cur];
    f32 x0 = 968.0f;
    f32 x1 = 1348.0f;
    f32 y = 140.0f + rise;
    f32 w;
    s32 n = 0;
    s32 k;

    Mx_Window(x0, y, x1, y + 356.0f);
    /* its name, NEW FIGHTER after it */
    w = Mx_TextW(m->name, 27.0f);
    if (m->isNew && w + Mx_TextW("NEW FIGHTER", 22.0f) + 14.0f < x1 - x0 - 46.0f) {
        Mx_Text(x0 + 22.0f, y + 30.0f, m->name, 27.0f, 0.0f, MX_GOLD, 0);
        Mx_Text(x0 + 36.0f + w, y + 30.0f, "NEW FIGHTER", 22.0f, 0.0f, MX_CYAN, 0);
    } else {
        Mx_Text(x0 + 22.0f, y + 30.0f, m->name, 27.0f, x1 - x0 - 44.0f, MX_GOLD, 0);
    }
    Mx_Text(x0 + 22.0f, y + 74.0f, m->isNew ? "BASED ON:" : "REPLACES:", 23.0f, 0.0f, MX_GOLD, 0);
    Mx_Text(x0 + 22.0f, y + 106.0f, m->chara < 0 ? "- (EDIT DETAILS TO PICK ONE)" : m->target, 24.0f, x1 - x0 - 44.0f, m->chara < 0 ? MX_ORANGE : MX_WHITE, 0);
    if (Mm_About(0)[0] != 0) {
        Mx_Text(x0 + 22.0f, y + 138.0f, "BY:", 23.0f, 0.0f, MX_GOLD, 0);
        Mx_Text(x0 + 30.0f + Mx_TextW("BY:", 23.0f), y + 138.0f, Mm_About(0), 23.0f, 210.0f, MX_WHITE, 0);
    }
    if (Mm_About(1)[0] != 0) {
        sprintf(line, "V%.14s", Mm_About(1));
        Mx_Text(x1 - 22.0f, y + 138.0f, line, 23.0f, 100.0f, MX_GOLD, 2);
    }
    /* what its maker says it is; else what is wrong, or what it is */
    if (m->problem != 0) {
        n = Mx_SayWrap(Mm_ProblemText(m->problem), 22.0f, x1 - x0 - 44.0f, lines, 2);
        for (k = 0; k < n; k++) {
            Mx_Say(x0 + 22.0f, y + 174.0f + 28.0f * (f32)k, lines[k], 22.0f, 0.0f, MX_RED, 0);
        }
    } else if (m->conflict && m->rival >= 0 && m->rival < sMm->count) {
        sprintf(line, m->wins ? "Used over %.40s (same fighter)" : "Hidden by %.40s (same fighter)", sMm->info[m->rival].name);
        n = Mx_SayWrap(line, 22.0f, x1 - x0 - 44.0f, lines, 2);
        for (k = 0; k < n; k++) {
            Mx_Say(x0 + 22.0f, y + 174.0f + 28.0f * (f32)k, lines[k], 22.0f, 0.0f, MX_ORANGE, 0);
        }
    } else if (Mm_About(2)[0] != 0) {
        n = Mx_SayWrap(Mm_About(2), 23.0f, x1 - x0 - 44.0f, lines, 2);
        for (k = 0; k < n; k++) {
            Mx_Say(x0 + 22.0f, y + 174.0f + 28.0f * (f32)k, lines[k], 23.0f, 0.0f, MX_WHITE, 0);
        }
    } else {
        if (!m->on && m->rival >= 0 && m->rival < sMm->count) {
            sprintf(line, "Turning it on swaps out %.40s", sMm->info[m->rival].name);
        } else if (m->on && m->onCount > 1) {
            sprintf(line, "On. Priority %d of %d%s", (int)m->priority, (int)m->onCount, m->large ? ". A large model." : ".");
        } else {
            sprintf(line, "%s%s", m->on ? "On: the next fight uses it." : "Off: cross turns it on.", m->large ? " A large model." : "");
        }
        n = Mx_SayWrap(line, 23.0f, x1 - x0 - 44.0f, lines, 2);
        for (k = 0; k < n; k++) {
            Mx_Say(x0 + 22.0f, y + 174.0f + 28.0f * (f32)k, lines[k], 23.0f, 0.0f, MX_WHITE, 0);
        }
    }
    Mx_Rect(x0 + 24.0f, y + 232.0f, x1 - 24.0f, y + 234.0f, Mx_Rgba(0xA0, 0xE0, 0xE0, 0x50), Mx_Rgba(0xA0, 0xE0, 0xE0, 0x50));
    /* its costumes: the one shown gold */
    n = 0;
    for (k = 0; k < 4; k++) {
        n += Mm_HasCostume(sMm->cur, k);
    }
    sprintf(line, n == 1 ? "%d COSTUME" : "%d COSTUMES", (int)n);
    Mx_Text(x0 + 22.0f, y + 254.0f, line, 25.0f, 0.0f, MX_GOLD, 0);
    for (k = 0; k < 4; k++) {
        static char *num[4] = {"1", "2", "3", "4"};
        f32 cx = x0 + 59.0f + 87.0f * (f32)k;
        f32 cy = y + 288.0f;

        if (k == sMm->costume && Mm_HasCostume(sMm->cur, k)) {
            Mx_Bar(MM_TAB_SEL, cx - 36.0f, cy - 16.0f, cx + 36.0f, cy + 16.0f, 0x80);
            Mx_TextC(cx, cy, num[k], 24.0f, 0.0f, Mx_Rgba(0xFF, 0xFF, 0xF0, 0x80), Mx_Rgba(0xFF, 0xF0, 0xC8, 0x80), Mx_Rgba(0x6C, 0x3C, 0x08, 0x80), 1, 0x80);
        } else {
            Mx_Bar(MM_NAVBAR, cx - 36.0f, cy - 15.0f, cx + 36.0f, cy + 15.0f, Mm_HasCostume(sMm->cur, k) ? 0x80 : 0x40);
            Mx_TextA(cx, cy, num[k], 24.0f, 0.0f, MX_WHITE, 1, Mm_HasCostume(sMm->cur, k) ? 0x80 : 0x40);
        }
    }
    /* what it brings */
    {
        f32 x = x0 + 22.0f;

        for (k = 0; k < 3; k++) { /* (three alike, across the window: the words squeezed to them if need be) */
            f32 tw = (x1 - x0 - 44.0f - 16.0f) / 3.0f;
            s32 a = (m->kinds & bit[k]) ? 0x80 : 0x38;

            Mx_Bar(MM_NAVBAR, x, y + 312.0f, x + tw, y + 340.0f, a);
            Mx_TextC(x + tw * 0.5f, y + 326.0f, kindName[k], 18.0f, tw - 18.0f, Mx_Rgba(0xE8, 0xDC, 0xFF, 0x80), Mx_Rgba(0xC8, 0xB8, 0xF8, 0x80), Mx_Rgba(0x20, 0x10, 0x40, 0x80), 1, a);
            x += tw + 8.0f;
        }
    }
    /* its blasts, once the cursor has rested: in a box of their own under the window, one a line */
    if (sMmMovesOf != sMm->cur && sMm->rest >= MM_SHOW_DELAY) {
        sMmMoveCount = Port_ModMoves(sMm->cur, sMmMoves, 5);
        sMmMovesOf = sMm->cur;
    }
    Lib_Blasts(x0, y + 362.0f, x1, 684.0f + rise);
}

static void Lib_Draw(void) {
    static char line[96];
    f32 slide = (1.0f - Mm_In(4)) * -700.0f;
    f32 rise = (1.0f - Mm_In(8)) * 600.0f;
    s32 row;

    for (row = 0; row < MM_ROWS && sMm->top + row < sMm->viewCount; row++) {
        s32 at = sMm->top + row;
        f32 y0 = MM_LIST_Y + MM_PITCH * (f32)row;

        if (at == sMm->vcur) {
            continue; /* (the cursor's last: it glides over the others) */
        }
        Lib_Row(at, y0, slide * (1.0f + 0.12f * (f32)row));
    }
    if (sMm->vcur >= sMm->top && sMm->vcur < sMm->top + MM_ROWS && sMm->vcur < sMm->viewCount) {
        Lib_Row(sMm->vcur, sMm->curY, slide);
    }
    /* the scrollbar at the list's left, its arrows; under the list where in it the cursor is */
    if (sMm->viewCount > MM_ROWS) {
        f32 t0 = 236.0f;
        f32 t1 = 640.0f;
        f32 th = (t1 - t0) * (f32)MM_ROWS / (f32)sMm->viewCount;
        f32 ty = t0 + (t1 - t0 - th) * (f32)sMm->top / (f32)(sMm->viewCount - MM_ROWS);

        Mx_Box(MM_FIELD, 25.0f + slide, t0 - 4.0f, 47.0f + slide, t1 + 4.0f, 10.0f, 0x80);
        Mx_Box(MM_TAB, 27.0f + slide, ty, 45.0f + slide, ty + th, 8.0f, 0x80);
        Mx_Fit(MM_UP, 36.0f + slide, 210.0f - 3.0f * Mm_Beat(36), 32.0f, sMm->top > 0 ? 0x80 : 0x40);
        Mx_Fit(MM_DOWN, 36.0f + slide, 650.0f + 3.0f * Mm_Beat(36), 32.0f, sMm->top + MM_ROWS < sMm->viewCount ? 0x80 : 0x40);
    }
    if (sMm->finding || sMm->findLen > 0) { /* the search line */
        sprintf(line, "SEARCH: %s%s", sMm->find, sMm->finding && ((sMm->frames / 16) & 1) ? "_" : "");
        Mx_Box(MM_FIELD, LIB_X0 + slide, 648.0f, LIB_X1 + slide, 688.0f, 20.0f, 0x80);
        Mx_Text(LIB_X0 + 22.0f + slide, 668.0f, line, 23.0f, LIB_X1 - LIB_X0 - 180.0f, MX_WHITE, 0);
        sprintf(line, "%d / %d", (int)(sMm->viewCount > 0 ? sMm->vcur + 1 : 0), (int)sMm->viewCount);
        Mx_Text(LIB_X1 - 22.0f + slide, 668.0f, line, 23.0f, 0.0f, MX_GOLD, 2);
    } else if (sMm->viewCount > 0) {
        f32 cx = (LIB_X0 + LIB_X1) * 0.5f + slide;

        sprintf(line, "%d / %d", (int)(sMm->vcur + 1), (int)sMm->viewCount);
        Mx_Box(MM_KEY, cx - 115.0f, 651.0f, cx - 77.0f, 685.0f, 8.0f, 0x80);
        Mx_Fit(MM_LEFT, cx - 96.0f, 668.0f, 24.0f, 0x80);
        Mx_Box(MM_KEY, cx + 77.0f, 651.0f, cx + 115.0f, 685.0f, 8.0f, 0x80);
        Mx_Fit(MM_RIGHT, cx + 96.0f, 668.0f, 24.0f, 0x80);
        Mx_Text(cx, 668.0f, line, 27.0f, 140.0f, MX_DARK, 1);
        if (sMm->filter != 0) {
            Mx_Text(LIB_X1 + slide, 668.0f, sMmShowName[sMm->filter], 21.0f, 150.0f, MX_DARK, 2);
        }
    }
    if (sMm->count > 0 && sMm->viewCount == 0) {
        Mx_Window(LIB_X0, 300.0f, LIB_X1, 450.0f);
        Mx_Text((LIB_X0 + LIB_X1) * 0.5f, 350.0f, "NOTHING MATCHES", 30.0f, 0.0f, MX_GOLD, 1);
        Mx_Text((LIB_X0 + LIB_X1) * 0.5f, 398.0f, "Square: menu, to show all or search again", 22.0f, LIB_X1 - LIB_X0 - 40.0f, MX_WHITE, 1);
    } else if (sMm->count == 0) {
        Mx_Window(LIB_X0, 230.0f, LIB_X1, 520.0f);
        Mx_Text((LIB_X0 + LIB_X1) * 0.5f, 285.0f, "NO MODS YET", 34.0f, 0.0f, MX_GOLD, 1);
        Mx_Text((LIB_X0 + LIB_X1) * 0.5f, 345.0f, "Square: menu, then Add mods...", 24.0f, LIB_X1 - LIB_X0 - 40.0f, MX_WHITE, 1);
        Mx_Text((LIB_X0 + LIB_X1) * 0.5f, 390.0f, "or drag a mod folder or a zip", 24.0f, LIB_X1 - LIB_X0 - 40.0f, MX_WHITE, 1);
        Mx_Text((LIB_X0 + LIB_X1) * 0.5f, 425.0f, "onto this window.", 24.0f, LIB_X1 - LIB_X0 - 40.0f, MX_WHITE, 1);
    }
    if (sMm->viewCount > 0) {
        Lib_Info(rise);
    }
}

/* What can be done with the mod under the cursor: cross opens it beside the mod's bar. */
#define ACT_TOGGLE 0
#define ACT_COSTUME 1
#define ACT_EDIT 2
#define ACT_FORMS 3
#define ACT_TEST 4
#define ACT_FILES 5
#define ACT_REMOVE 6
#define ACT_COUNT 7

static char *Act_Line(s32 k) {
    static char line[48];

    switch (k) {
    case ACT_TOGGLE:
        return sMm->info[sMm->cur].on ? "TURN OFF" : "TURN ON";
    case ACT_COSTUME:
        sprintf(line, "COSTUME: < %d >", (int)(sMm->costume + 1));
        return line;
    case ACT_EDIT:
        return "EDIT DETAILS...";
    case ACT_FORMS:
        return "TRANSFORMATIONS...";
    case ACT_TEST:
        return "TEST FIGHT";
    case ACT_FILES:
        return "SHOW ITS FILES ON THE PC";
    }
    return "REMOVE...";
}

static void Mm_DrawAct(void) {
    f32 t = Mm_Ease((f32)sMm->actT / 6.0f);
    f32 pitch = 46.0f;
    f32 h = 70.0f + pitch * (f32)ACT_COUNT;
    f32 x0 = 612.0f;
    f32 x1 = 948.0f;
    f32 y0 = sMm->curY - 6.0f;
    s32 k;

    if (!sMm->act || sMm->viewCount == 0) {
        return;
    }
    if (y0 + h > 690.0f) {
        y0 = 690.0f - h;
    }
    x0 -= (1.0f - t) * 40.0f;
    x1 -= (1.0f - t) * 40.0f;
    /* a pointer from the mod's bar to its menu */
    Mx_Fit(MM_RIGHT, x0 - 12.0f, sMm->curY + MM_PITCH * 0.42f, 30.0f, (s32)(t * 128.0f));
    Mx_Box(MM_WINDOW, x0, y0 + 26.0f, x1, y0 + h, 22.0f, (s32)(t * 128.0f));
    Mx_Box(MM_BAR_SEL, x0 + 10.0f, y0, x1 - 10.0f, y0 + 52.0f, 24.0f, (s32)(t * 128.0f));
    if (sMm->actT < 3) {
        return;
    }
    Mx_TextC((x0 + x1) * 0.5f, y0 + 26.0f, sMm->info[sMm->cur].name, 25.0f, x1 - x0 - 60.0f, Mx_Rgba(0xFF, 0xF0, 0xB0, 0x80), Mx_Rgba(0xF0, 0xA0, 0x30, 0x80),
             Mx_Rgba(0x50, 0x20, 0x06, 0x80), 1, 0x80);
    for (k = 0; k < ACT_COUNT; k++) {
        f32 iy = y0 + 66.0f + pitch * (f32)k;
        s32 live = k != ACT_COSTUME || Mm_HasCostume(sMm->cur, 1) || Mm_HasCostume(sMm->cur, 2) || Mm_HasCostume(sMm->cur, 3);

        if (k == sMm->actCur) {
            Mx_Box(MM_BAR_SEL, x0 + 18.0f, iy - 3.0f, x1 - 18.0f, iy + 39.0f, 19.0f, 0x80);
            Mx_TextC((x0 + x1) * 0.5f, iy + 18.0f, Act_Line(k), 23.0f, x1 - x0 - 60.0f, Mx_Rgba(0xFF, 0xFF, 0xF4, 0x80), Mx_Rgba(0xFF, 0xF0, 0xD0, 0x80),
                     Mx_Rgba(0x6C, 0x34, 0x08, 0x80), 1, live ? 0x80 : 0x58);
        } else {
            Mx_Box(k == ACT_REMOVE ? MM_BAR_GREY : MM_BAR, x0 + 22.0f, iy, x1 - 22.0f, iy + 36.0f, 17.0f, 0x80);
            Mx_TextA((x0 + x1) * 0.5f, iy + 18.0f, Act_Line(k), 22.0f, x1 - x0 - 60.0f, live ? MX_LAV : MX_DIM, 1, live ? 0x80 : 0x70);
        }
    }
}

/* The question before a mod is removed (mockup 03): the window over everything but the title. */
static void Mm_DrawConfirm(void) {
    f32 t = Mm_Ease((f32)sMm->confirmT / 8.0f);
    f32 y = (1.0f - t) * 60.0f;
    PortModInfo *m;

    if (!sMm->confirm || sMm->viewCount == 0) {
        return;
    }
    m = &sMm->info[sMm->cur];
    Mx_Box(MM_CARD, 240.0f, 180.0f + y, 1136.0f, 586.0f + y, 40.0f, (s32)(t * 128.0f));
    if (sMm->confirmT < 4) {
        return;
    }
    Mx_Text(688.0f, 245.0f + y, "REMOVE THIS MOD?", 44.0f, 0.0f, MX_GOLD, 1);
    {
        f32 w = Mx_TextW(m->name, 25.0f) + 210.0f;

        if (w > 760.0f) {
            w = 760.0f;
        }
        Mx_Box(MM_BAR_SEL, 688.0f - w * 0.5f, 302.0f + y, 688.0f + w * 0.5f, 354.0f + y, 22.0f, 0x80);
        Mx_FaceCell(PORT_REF_MOD + sMm->cur, 688.0f - w * 0.5f + 24.0f, 307.0f + y, 688.0f - w * 0.5f + 66.0f, 349.0f + y, 0);
        Mx_Text(688.0f - w * 0.5f + 80.0f, 328.0f + y, m->name, 25.0f, w - 200.0f, MX_LAV, 0);
        Lib_OnOff(m->on, 688.0f + w * 0.5f - 66.0f, 328.0f + y);
    }
    Mx_Say(688.0f, 400.0f + y, "Not deleted: it goes to the mods_removed folder,", 27.0f, 800.0f, MX_LAV, 1);
    Mx_Say(688.0f, 443.0f + y, "and Library Menu > Removed Mods puts it back.", 27.0f, 800.0f, MX_LAV, 1);
    Mx_Box(MM_BAR, 415.0f, 495.0f + y, 668.0f, 550.0f + y, 24.0f, 0x80);
    Mx_Button(0, 432.0f, 522.0f + y, 32.0f);
    Mx_Text(558.0f, 522.0f + y, "REMOVE", 28.0f, 0.0f, MX_WHITE, 1);
    Mx_Box(MM_BAR_SEL, 703.0f, 493.0f + y, 966.0f, 552.0f + y, 26.0f, 0x80);
    Mx_Button(2, 722.0f, 522.0f + y, 32.0f);
    Mx_TextC(850.0f, 522.0f + y, "KEEP", 28.0f, 0.0f, Mx_Rgba(0xFF, 0xF6, 0xD8, 0x80), Mx_Rgba(0xFF, 0xE0, 0x90, 0x80), Mx_Rgba(0x5C, 0x30, 0x08, 0x80), 1, 0x80);
}

/* ---- the Library's menu (square): what is done less often than switching a mod on ---- */
#define MI_ADD 0
#define MI_FIND 1
#define MI_SHOW 2
#define MI_SORT 3
#define MI_PRIO 4
#define MI_FOE 5
#define MI_ALL 6
#define MI_GONE 7
#define MI_RELOAD 8
#define MI_FOLDER 9
#define MI_LIBDIR 10
#define MI_COUNT 11
#define MENU_X0 96.0f
#define MENU_X1 416.0f
#define MENU_Y 82.0f
#define MENU_PITCH 25.0f

static s32 Mm_AllOn(void) {
    s32 i;

    for (i = 0; i < sMm->count; i++) {
        if (!sMm->info[i].on) {
            return 0;
        }
    }
    return sMm->count > 0;
}

/* Whether a line of the menu can be chosen now. */
static s32 Mm_MenuLive(s32 item) {
    switch (item) {
    case MI_PRIO:
        return sMm->viewCount > 0 && sMm->info[sMm->cur].on && sMm->info[sMm->cur].onCount > 1;
    case MI_ALL:
    case MI_FIND:
    case MI_SHOW:
    case MI_SORT:
        return sMm->count > 0;
    case MI_FOLDER:
        return sMm->viewCount > 0;
    }
    return 1;
}

static char *Mm_MenuLine(s32 item) {
    static char line[96];

    switch (item) {
    case MI_ADD:
        return "ADD MODS...";
    case MI_FIND:
        if (sMm->findLen > 0) {
            sprintf(line, "SEARCH: %s", sMm->find);
            return line;
        }
        return "SEARCH BY NAME";
    case MI_SHOW:
        sprintf(line, "SHOW: < %s >", sMmShowName[sMm->filter]);
        return line;
    case MI_SORT:
        sprintf(line, "SORT BY: < %s >", sMmSortName[sMm->sort]);
        return line;
    case MI_PRIO:
        if (!Mm_MenuLive(MI_PRIO)) {
            return "PRIORITY (FOR MODS THAT ARE ON)";
        }
        sprintf(line, "PRIORITY: < %d OF %d >", (int)sMm->info[sMm->cur].priority, (int)sMm->info[sMm->cur].onCount);
        return line;
    case MI_FOE:
        sprintf(line, "TEST FIGHT OPPONENT: %.40s", Mm_RefName(gMmTestFoe));
        return line;
    case MI_ALL:
        return Mm_AllOn() ? "TURN ALL MODS OFF" : "TURN ALL MODS ON";
    case MI_GONE:
        return "REMOVED MODS...";
    case MI_RELOAD:
        return "RELOAD THE MODS FOLDER";
    case MI_FOLDER:
        return "SHOW THIS MOD'S FILES ON THE PC";
    case MI_LIBDIR:
        return "OPEN THE MODS FOLDER ON THE PC";
    }
    return "";
}

/* The Library's menu (mockup 02): a window in the middle, a bar per line, its own buttons at its foot. */
static void Mm_DrawMenu(void) {
    f32 t = Mm_Ease((f32)sMm->menuT / 7.0f);
    f32 y = (1.0f - t) * 50.0f;
    f32 pitch = 45.0f;
    s32 k;

    if (!sMm->menu) {
        return;
    }
    Mx_Box(MM_WINDOW, 380.0f, 140.0f + y, 996.0f, 696.0f + y, 26.0f, (s32)(t * 128.0f));
    Mx_Box(MM_BAR_SEL, 392.0f, 116.0f + y, 984.0f, 176.0f + y, 28.0f, (s32)(t * 128.0f));
    if (sMm->menuT < 4) {
        return;
    }
    Mx_TextC(688.0f, 146.0f + y, "LIBRARY MENU", 36.0f, 0.0f, Mx_Rgba(0xFF, 0xF0, 0xB0, 0x80), Mx_Rgba(0xF0, 0xA0, 0x30, 0x80), Mx_Rgba(0x50, 0x20, 0x06, 0x80), 1, 0x80);
    for (k = 0; k < MI_COUNT; k++) {
        f32 y0 = 190.0f + pitch * (f32)k + y;
        s32 live = Mm_MenuLive(k);

        if (k == sMm->menuCur) {
            Mx_Box(MM_BAR_SEL, 426.0f, y0 - 3.0f, 950.0f, y0 + 41.0f, 20.0f, 0x80);
            Mx_TextC(688.0f, y0 + 19.0f, Mm_MenuLine(k), 25.0f, 480.0f, Mx_Rgba(0xFF, 0xFF, 0xF4, 0x80), Mx_Rgba(0xFF, 0xF0, 0xD0, 0x80), Mx_Rgba(0x6C, 0x34, 0x08, 0x80), 1,
                     live ? 0x80 : 0x58);
        } else {
            Mx_Box(MM_BAR, 430.0f, y0, 946.0f, y0 + 38.0f, 18.0f, 0x80);
            Mx_TextA(688.0f, y0 + 19.0f, Mm_MenuLine(k), 24.0f, 480.0f, live ? MX_LAV : MX_DIM, 1, live ? 0x80 : 0x70);
        }
    }
    Mm_PromptReset();
    Mm_Prompt(0, "CHOOSE");
    if (sMm->menuCur == MI_SHOW || sMm->menuCur == MI_SORT || sMm->menuCur == MI_PRIO) {
        Mm_Prompt(3, "CHANGE");
    }
    if (sMm->menuCur == MI_FIND && sMm->findLen > 0) {
        Mm_Prompt(4, "CLEAR");
    }
    Mm_Prompt(2, "CLOSE");
    Mx_Footer(388.0f, 988.0f, 703.0f + y, 763.0f + y);
}

/* The search line of the Library: the PC keyboard types into it, enter keeps it, escape clears it. */
static void Mm_FindType(void) {
    s32 c;
    s32 changed = 0;

    while ((c = Port_TextRead()) != 0) {
        if (c == 13 || c == 27) {
            if (c == 27) {
                sMm->findLen = 0;
                sMm->find[0] = 0;
                changed = 1;
            }
            sMm->finding = 0;
            Port_TextMode(0);
            break;
        } else if (c == 8) {
            if (sMm->findLen > 0) {
                sMm->find[--sMm->findLen] = 0;
                changed = 1;
            }
        } else if (c >= 32 && c < 127 && sMm->findLen < 24) {
            sMm->find[sMm->findLen++] = (char)c;
            sMm->find[sMm->findLen] = 0;
            changed = 1;
        }
    }
    if (changed) {
        sMm->vcur = 0;
        sMm->top = 0;
        sMm->cur = -1;
        Mm_BuildView();
        if (sMm->viewCount > 0) {
            sMm->cur = sMm->view[0];
        } else {
            sMm->cur = 0;
        }
        sMm->rest = 0;
        sMm->costume = 0;
    }
}

static void Mm_AllToggle(void) {
    static char note[64];
    static PortModInfo one;
    s32 off = !Mm_AllOn();
    s32 left = 0;
    s32 i;

    for (i = 0; i < sMm->count; i++) {
        Port_ModGetInfo(i, &one); /* (as things stand now: an earlier one of this loop may be its rival) */
        if (off && !one.on && one.rival >= 0) {
            left++; /* another mod for the same fighter is on: switching this on would switch that off */
        } else if ((off ? !one.on : one.on) && Port_ModToggle(i) >= 0) {
            left++; /* (it switched off one turned on earlier in this loop) */
        }
    }
    Mm_Refresh();
    Mm_FreeFaces();
    sMm->flash = 10;
    if (left > 0) {
        sprintf(note, "ALL ON, EXCEPT %d (SAME FIGHTER AS ANOTHER)", (int)left);
        Mm_Note(note);
    } else {
        Mm_Note(off ? "ALL MODS ON" : "ALL MODS OFF");
    }
    Snd_PlaySe(1, 1);
}

/* The model goes and nothing of a mod is shown: before the library changes under it. */
static void Mm_Unshow(void) {
    Mm_ExtrasHide();
    ChrView_Hide();
    Port_ModPreview(-1);
    sMm->shown = -1;
    sMm->rest = 0;
}

static void Mm_MenuUpdate(void) {
    Pad *pad = &gPad[0];
    s32 k = sMm->menuCur;
    s32 dir = (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) ? 1 : (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -1 : 0;

    if (sMm->menuT < 5) {
        return;
    }
    if (pad->gamePressed & (PADG_TRIANGLE | PADG_SQUARE)) {
        sMm->menu = 0;
        Snd_PlaySe(1, 2);
        return;
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sMm->menuCur = (k + 1) % MI_COUNT;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sMm->menuCur = (k + MI_COUNT - 1) % MI_COUNT;
    }
    if (sMm->menuCur != k) {
        Snd_PlaySe(1, 0);
        return;
    }
    if (!Mm_MenuLive(k)) {
        if (pad->gamePressed & PADG_CROSS) {
            Snd_PlaySe(1, 2);
        }
        return;
    }
    if (dir != 0 && (k == MI_SHOW || k == MI_SORT || k == MI_PRIO)) { /* the lines that cycle */
        if (k == MI_SHOW) {
            sMm->filter = (sMm->filter + MM_SHOWS + dir) % MM_SHOWS;
            Mm_BuildView();
        } else if (k == MI_SORT) {
            sMm->sort = (sMm->sort + MM_SORTS + dir) % MM_SORTS;
            Mm_BuildView();
        } else {
            Mm_Unshow();
            Port_ModPriority(sMm->cur, dir);
            Mm_Refresh();
        }
        sMm->rest = 0;
        Snd_PlaySe(1, 0);
        return;
    }
    if ((pad->gamePressed & PADG_CIRCLE) && k == MI_FIND && sMm->findLen > 0) {
        sMm->findLen = 0;
        sMm->find[0] = 0;
        Mm_BuildView();
        Snd_PlaySe(1, 2);
        return;
    }
    if (!(pad->gamePressed & PADG_CROSS)) {
        return;
    }
    Snd_PlaySe(1, 1);
    switch (k) {
    case MI_ADD:
        sMm->menu = 0;
        Mm2_OpenBrowser();
        break;
    case MI_FIND:
        sMm->menu = 0;
        sMm->finding = 1;
        Port_TextMode(1);
        break;
    case MI_SHOW:
        sMm->filter = (sMm->filter + 1) % MM_SHOWS;
        Mm_BuildView();
        break;
    case MI_SORT:
        sMm->sort = (sMm->sort + 1) % MM_SORTS;
        Mm_BuildView();
        break;
    case MI_PRIO:
        Mm_Unshow();
        Port_ModPriority(sMm->cur, -1);
        Mm_Refresh();
        break;
    case MI_FOE:
        sMm->menu = 0;
        Mm2_OpenFoePicker();
        break;
    case MI_ALL:
        sMm->menu = 0;
        Mm_AllToggle();
        break;
    case MI_GONE:
        sMm->menu = 0;
        Mm2_OpenRemoved();
        break;
    case MI_RELOAD:
        sMm->menu = 0;
        Mm_Unshow();
        Port_ModRescan();
        Mm_FreeFaces();
        Mm_Refresh();
        Mm_Note("MODS FOLDER RELOADED");
        break;
    case MI_FOLDER:
        Port_ModShowFolder(sMm->cur);
        Mm_Note("OPENED ON THE PC (CHECK THE TASKBAR)");
        break;
    case MI_LIBDIR:
        Port_ModShowFolder(-1);
        Mm_Note("OPENED ON THE PC (CHECK THE TASKBAR)");
        break;
    }
}

/* Everything of a frame over the 3D scene: the title and the tabs, the screen that is up, the buttons at the foot,
   the menu or the question over it, the news. */
static void Mm_DrawImport(void);

static void Mm_DrawFrame(void) {
    f32 drop = (1.0f - Mm_In(0)) * -160.0f;
    s32 tab = 0;

    if (sMm->mode == MM_ROSTER || sMm->mode == MM_TRANSFORM) {
        tab = sMm->mode;
    } else if (sMm->mode == MM_PICKER) {
        tab = sMm->back == MM_TRANSFORM ? MM_TRANSFORM : 0;
    }
    Mx_HeaderDraw();
    Mm_TexReset(); /* (the window's movie uploaded its own pictures) */
    Mx_Chrome("Mod Manager", tab, drop);
    if (!sMm->importing) {
        if (sMm->mode == MM_LIBRARY) {
            if (!sMm->confirm) {
                Lib_Draw();
            }
            Mm_DrawAct();
            Mm_DrawConfirm();
        } else {
            Mm2_Draw();
        }
        if (!(sMm->mode == MM_LIBRARY && sMm->menu)) {
            Mx_Footer(12.0f, 1364.0f, MX_FOOT_Y0 + (1.0f - Mm_In(8)) * 120.0f, MX_FOOT_Y1 + (1.0f - Mm_In(8)) * 120.0f);
        }
        if (sMm->mode == MM_LIBRARY) {
            Mm_DrawMenu();
        }
    }
    Mm_DrawImport();
    Mx_Note(sMm->noteLine, sMm->note, 640.0f);
}

static void Lib_Test(void);
static void Lib_Toggle(void);
static void Lib_Costume(s32 dir);
static void Mm_ActUpdate(void);

static void Mm_Library(s32 live) {
    Pad *pad = &gPad[0];
    s32 was = sMm->vcur;
    s32 have = sMm->viewCount > 0;

    sMm->wantKey = -1;
    if (have && Mm_HasCostume(sMm->cur, sMm->costume)) {
        sMm->wantKey = sMm->cur * 4 + sMm->costume;
        sMm->wantChara = sMm->info[sMm->cur].chara >= 0 ? sMm->info[sMm->cur].chara : 0; /* (not set: shown in the first fighter's place) */
        sMm->wantCostume = sMm->costume;
        sMm->wantPreview = sMm->cur;
    }
    Mm_SetModel(785.0f, 572.0f, 405.0f);
    if (sMm->confirm) {
        Mm_Prompt(0, "REMOVE");
        Mm_Prompt(2, "KEEP");
    } else if (sMm->menu) {
        Mm_Prompt(0, "CHOOSE");
        Mm_Prompt(3, "CHANGE");
        Mm_Prompt(2, "CLOSE");
    } else if (sMm->act) {
        Mm_Prompt(0, "CHOOSE");
        if (sMm->actCur == ACT_COSTUME) {
            Mm_Prompt(3, "CHANGE");
        }
        Mm_Prompt(2, "CLOSE");
    } else if (sMm->finding) {
        Mm_Prompt(11, "KEEP SEARCH");
        Mm_Prompt(12, "CLEAR");
    } else {
        if (have) {
            Mm_Prompt(0, "OPTIONS");
        }
        Mm_Prompt(1, "LIBRARY MENU");
        if (have && sMm->viewCount > MM_ROWS) {
            Mm_Prompt(3, "PAGE");
        }
        Mm_Prompt(2, "BACK");
    }
    if (!live) {
        return;
    }
    if (sMm->menu) {
        Mm_MenuUpdate();
        return;
    }
    if (sMm->act) {
        Mm_ActUpdate();
        return;
    }
    if (sMm->finding) { /* letters go into the search; the directions still move */
        Mm_FindType();
        pad->gamePressed &= ~(PADG_TRIANGLE | PADG_CIRCLE | PADG_CROSS | PADG_SQUARE | PADG_START | PADG_SELECT | PADG_L1 | PADG_R1 | PADG_L2 | PADG_R2);
    }
    if (sMm->confirm) { /* the question: cross removes, triangle or circle keeps */
        if (sMm->confirmT < 8) {
            return;
        }
        if (pad->gamePressed & PADG_CROSS) {
            s32 ok;

            Mm_Unshow();
            ok = Port_ModRemove(sMm->cur);
            Mm_FreeFaces();
            Mm_Refresh();
            sMm->costume = 0;
            sMm->confirm = 0;
            if (ok) {
                Mm_Note("REMOVED. MENU > REMOVED MODS PUTS IT BACK");
                Snd_PlaySe(1, 1);
            } else {
                Mm_Note("COULD NOT MOVE IT: IS A FILE OF IT OPEN?");
                Snd_PlaySe(1, 2);
            }
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
            sMm->confirm = 0;
            Snd_PlaySe(1, 2);
        }
        return;
    }
    if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        sMm->leaving = 1;
        ColorFade_StartOut(0, 0, 0, 0x14);
        return;
    }
    if (pad->gamePressed & PADG_R1) {
        Snd_PlaySe(1, 0);
        Mm_SetMode(MM_ROSTER);
        return;
    }
    if (pad->gamePressed & PADG_L1) {
        Snd_PlaySe(1, 0);
        Mm_SetMode(MM_TRANSFORM);
        return;
    }
    if (pad->gamePressed & PADG_SQUARE) {
        Snd_PlaySe(1, 1);
        sMm->menu = 1;
        sMm->menuT = 0;
        return;
    }
    if (!have) {
        return;
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sMm->vcur = (sMm->vcur + 1) % sMm->viewCount;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sMm->vcur = (sMm->vcur + sMm->viewCount - 1) % sMm->viewCount;
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) { /* a page */
        sMm->vcur = sMm->vcur + MM_ROWS < sMm->viewCount ? sMm->vcur + MM_ROWS : sMm->viewCount - 1;
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        sMm->vcur = sMm->vcur >= MM_ROWS ? sMm->vcur - MM_ROWS : 0;
    }
    if (sMm->vcur != was) {
        Snd_PlaySe(1, 0);
        sMm->cur = sMm->view[sMm->vcur];
        sMm->rest = 0;
        sMm->costume = 0;
        if (sMm->vcur < sMm->top) {
            sMm->top = sMm->vcur;
        }
        if (sMm->vcur >= sMm->top + MM_ROWS) {
            sMm->top = sMm->vcur - MM_ROWS + 1;
        }
    }
    if (pad->gamePressed & PADG_L2) { /* (a shortcut: its next costume) */
        Lib_Costume(1);
    }
    if (pad->gamePressed & PADG_SELECT) { /* every mod on - or, when all are, every mod off */
        Mm_AllToggle();
        return;
    }
    if (pad->gamePressed & PADG_START) { /* (a shortcut: a training fight with it at once) */
        Lib_Test();
        return;
    }
    if (pad->gamePressed & PADG_CROSS) { /* the mod's own menu */
        sMm->act = 1;
        sMm->actCur = 0;
        sMm->actT = 0;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_CIRCLE) { /* (shortcuts: remove, edit) */
        sMm->confirm = 1;
        sMm->confirmT = 0;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_R2) {
        Snd_PlaySe(1, 1);
        Mm2_OpenDetail();
    }
}

/* A training fight with the mod under the cursor, if it can be fought with. */
static void Lib_Test(void) {
    {
        PortModInfo *m = &sMm->info[sMm->cur];

        if (m->chara < 0) {
            Mm_Note("PICK ITS FIGHTER FIRST (EDIT DETAILS)");
            Snd_PlaySe(1, 2);
        } else if (Mm_Broken(m)) {
            Mm_Note(Mm_ProblemText(m->problem));
            Snd_PlaySe(1, 2);
        } else if (!m->on || (m->isNew && m->skin < 0)) {
            Mm_Note("TURN IT ON FIRST");
            Snd_PlaySe(1, 2);
        } else if (m->conflict && !m->wins) {
            Mm_Note("ANOTHER MOD IS USED FOR THIS FIGHTER");
            Snd_PlaySe(1, 2);
        } else {
            sMm->test = 1;
            sMm->leaving = 1;
            ColorFade_StartOut(0, 0, 0, 0x14);
            Snd_PlaySe(1, 1);
        }
    }
}

/* The mod under the cursor on, or off. */
static void Lib_Toggle(void) {
    {
        static char note[80];
        s32 off = Port_ModToggle(sMm->cur);

        Mm_Refresh();
        Mm_FreeFaces(); /* the pictures may be other files now */
        sMm->flash = 10;
        if (off >= 0 && off < sMm->count) { /* (two mods for one fighter cannot both be used) */
            sprintf(note, "%.30s TURNED OFF (SAME FIGHTER)", sMm->info[off].name);
            Mm_Note(note);
        } else if (sMm->info[sMm->cur].on && sMm->info[sMm->cur].problem != 0) {
            sprintf(note, "CAREFUL: %.50s", Mm_ProblemText(sMm->info[sMm->cur].problem));
            Mm_Note(note);
        }
        Snd_PlaySe(1, 1);
    }
}

/* The next costume the mod has (dir 1 / -1). */
static void Lib_Costume(s32 dir) {
    s32 n = sMm->costume;
    s32 k;

    for (k = 0; k < 4; k++) {
        n = (n + 4 + dir) % 4;
        if (Mm_HasCostume(sMm->cur, n)) {
            break;
        }
    }
    if (n != sMm->costume && Mm_HasCostume(sMm->cur, n)) {
        sMm->costume = n;
        Snd_PlaySe(1, 0);
    }
}

/* The mod's own menu: up / down choose, cross does it (left / right: the costume), triangle or circle close it. */
static void Mm_ActUpdate(void) {
    Pad *pad = &gPad[0];
    s32 k = sMm->actCur;
    PortModInfo *m = &sMm->info[sMm->cur];

    if (sMm->actT < 4) {
        return;
    }
    if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
        sMm->act = 0;
        Snd_PlaySe(1, 2);
        return;
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sMm->actCur = (k + 1) % ACT_COUNT;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sMm->actCur = (k + ACT_COUNT - 1) % ACT_COUNT;
    }
    if (sMm->actCur != k) {
        Snd_PlaySe(1, 0);
        return;
    }
    if (k == ACT_COSTUME && (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2))) {
        Lib_Costume((pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -1 : 1);
        return;
    }
    if (!(pad->gamePressed & PADG_CROSS)) {
        return;
    }
    switch (k) {
    case ACT_TOGGLE:
        Lib_Toggle();
        sMm->act = 0;
        break;
    case ACT_COSTUME:
        Lib_Costume(1);
        break;
    case ACT_EDIT:
        sMm->act = 0;
        Snd_PlaySe(1, 1);
        Mm2_OpenDetail();
        break;
    case ACT_FORMS:
        if (m->chara < 0 && !m->isNew) {
            Mm_Note("PICK ITS FIGHTER FIRST (EDIT DETAILS)");
            Snd_PlaySe(1, 2);
            break;
        }
        sMm->act = 0;
        sMm->trWho = m->isNew ? PORT_REF_MOD + sMm->cur : m->chara;
        sMm->trCur = 1;
        sMm->trPage = 0;
        sMm->trFocus = 0;
        Port_TransGet(sMm->trWho, &sMm->tr);
        Snd_PlaySe(1, 1);
        Mm_SetMode(MM_TRANSFORM);
        break;
    case ACT_TEST:
        Lib_Test();
        break;
    case ACT_FILES:
        Port_ModShowFolder(sMm->cur);
        Mm_Note("OPENED ON THE PC (CHECK THE TASKBAR)");
        Snd_PlaySe(1, 1);
        break;
    case ACT_REMOVE:
        sMm->act = 0;
        sMm->confirm = 1;
        sMm->confirmT = 0;
        Snd_PlaySe(1, 1);
        break;
    }
}

/* Every second and a half on the Library: when the mods folder changed from outside (a mod copied in or deleted by
   hand while the game runs), the library is read again. */
static void Mm_Watch(void) {
    if (sMm->mode != MM_LIBRARY || sMm->menu || sMm->act || sMm->confirm || sMm->importing || --sMm->watchT > 0) {
        return;
    }
    sMm->watchT = 90;
    if (Port_ModsChanged()) {
        s32 at = sMm->cur < sMm->count ? sMm->cur : -1;
        static char keep[64];

        if (at >= 0) {
            sprintf(keep, "%.63s", sMm->info[at].name);
        }
        Mm_Unshow();
        Port_ModRescan();
        Mm_FreeFaces();
        Mm_Refresh();
        if (at >= 0) { /* (back on the mod it was on, by name: the indices moved) */
            s32 i;
            s32 k;

            for (i = 0; i < sMm->count; i++) {
                for (k = 0; keep[k] != 0 && keep[k] == sMm->info[i].name[k]; k++) {
                }
                if (keep[k] == 0 && sMm->info[i].name[k] == 0) {
                    Mm_Select(i);
                    break;
                }
            }
        }
        Mm_Note("THE MODS FOLDER CHANGED: LIST UPDATED");
    }
}

static void Mm_Init(void) {
    sMm = Heap_Alloc(sizeof(ModMgr), 0x20, 0, 2);
    memset(sMm, 0, sizeof(ModMgr));
    sMm->shown = -1;
    sMm->wantKey = -1;
    sMm->dirLooked = -1;
    sMm->trWho = -1;
    sMm->aboutOf = -1;
    sMm->watchT = 90;
    sMm->curY = MM_LIST_Y;
    sMm->ui = Mm_UiOpen();
    Mx_ChipsOpen();
    Mm_ExtrasInit();
    Port_ModRescan();
    Mm_Refresh();
    if (sMmResume >= 0 && sMmResume < sMm->count) { /* back from a test fight: on the mod that was tested */
        Mm_Select(sMmResume);
    }
    sMmResume = -1;
    ChrView_Init();
}

static void Mm_Term(void) {
    Port_TextMode(0);
    Port_ModPreview(-1);
    Mm2_Term();
    Mm_ExtrasTerm();
    ChrView_Term();
    Mm_FreeFaces();
    Mx_FreeFaces();
    Mx_ChipsClose();
    Mm_UiClose();
    Heap_Free(sMm);
    sMm = NULL;
}

/* Starts adding mods on the library's thread (from: 0 a drop, 1 the browser's one mod, 2 the browser's folder). */
void Mm_StartImport(char *path, s32 all, s32 from) {
    Mm_Unshow();
    sMm->confirm = 0;
    sMm->menu = 0;
    if (sMm->finding) {
        sMm->finding = 0;
        Port_TextMode(0);
    }
    if (Port_ModImportStart(path, all)) {
        sMm->importing = 1;
        sMm->importFrom = from;
        sMm->prog[0] = 0;
        sMm->prog[1] = 0;
        sMm->prog[2] = 0;
    } else {
        Mm_Note("STILL ADDING THE LAST ONES");
    }
}

/* While mods are being added: how far it is. When it is done: the list, the cursor on what came, the next screen. */
static void Mm_ImportPoll(void) {
    s32 r;
    s32 n;
    s32 at;

    if (!sMm->importing) {
        return;
    }
    r = Port_ModImportPoll(sMm->prog);
    if (r != 2) {
        return;
    }
    sMm->importing = 0;
    n = Port_ModImportResult();
    at = Mm_ShowImport(n);
    if (sMm->importFrom == 1 && n == 0 && at < 0) {
        Mm_Note("COULD NOT COPY THAT FOLDER");
    }
    if (at >= 0) {
        if (sMm->importFrom != 0) {
            Mm2_BrowserKeep();
        }
        if (sMm->info[sMm->cur].chara < 0) { /* its files are named after no fighter of the roster: ask which it is */
            Mm_Note("ADDED. NOW PICK WHICH FIGHTER IT IS");
            Mm2_OpenDetail();
            sMm->detCur = 1;
        } else {
            Mm_SetMode(MM_LIBRARY);
        }
    }
}

static void Mm_DrawImport(void) {
    static char line[64];
    f32 k;
    f32 x0 = 334.0f;
    f32 x1 = 1043.0f;
    f32 fill;

    if (!sMm->importing) {
        return;
    }
    k = sMm->prog[1] > 0 ? (f32)sMm->prog[0] / (f32)sMm->prog[1] : 0.0f;
    if (k > 1.0f) {
        k = 1.0f;
    }
    /* (mockup 06) the window, the bar with the ball running along it, how far */
    Mx_Window(294.0f, 208.0f, 1082.0f, 572.0f);
    Mx_Text(688.0f, 253.0f, sMm->prog[2] == 1 ? "UNPACKING THE ARCHIVE..." : sMm->prog[2] == 2 ? "ADDING MODS..." : "LOOKING THROUGH THE FOLDER...", 42.0f, 700.0f, MX_GOLD, 1);
    Mx_Box(MM_FIELD, x0, 294.0f, x1, 352.0f, 29.0f, 0x80);
    if (sMm->prog[2] == 2 && k > 0.02f) {
        fill = x0 + 6.0f + (x1 - x0 - 12.0f) * k;
        Mx_Box(MM_TAB_SEL, x0 + 6.0f, 300.0f, fill, 346.0f, 23.0f, 0x80);
    } else { /* (counting or unpacking: a light that runs back and forth) */
        f32 at = x0 + 6.0f + Mm_Beat(80) * (x1 - x0 - 140.0f);

        Mx_Box(MM_TAB_SEL, at, 300.0f, at + 128.0f, 346.0f, 23.0f, 0x80);
        fill = at + 128.0f;
    }
    Mx_Fit(MM_BALL, fill - 8.0f, 323.0f, 56.0f, 0x80);
    if (sMm->prog[2] == 2 && sMm->prog[1] > 0) {
        sprintf(line, "%d OF %d FILES", (int)sMm->prog[0], (int)sMm->prog[1]);
    } else {
        sprintf(line, "PLEASE WAIT");
    }
    Mx_Say(688.0f, 390.0f, sMm->importFrom == 0 ? "From the folder dropped on the window" : "From the folder picked", 27.0f, 700.0f, MX_WHITE, 1);
    Mx_Text(688.0f, 433.0f, line, 30.0f, 0.0f, MX_LAV, 1);
    Mx_Say(688.0f, 500.0f, "Nothing of the game's own files is touched.", 23.0f, 700.0f, MX_CYAN, 1);
    Mx_Rect(0.0f, 703.0f, MX_W, MX_H, Mx_Rgba(0x20, 0x24, 0x30, 0x70), Mx_Rgba(0x10, 0x12, 0x1C, 0x78));
    Mx_Say(688.0f, 735.0f, "Please wait", 26.0f, 0.0f, MX_WHITE, 1);
}

/* A folder (or a file of one, or an archive) dropped on the game's window from the desktop: every character mod in
   it is added, as the browser's "add" would add them, and the Library shows the first of them. */
static void Mm_Dropped(s32 live) {
    static char path[700];

    if (!Port_DropTake(path)) {
        return;
    }
    if (!live || sMm->importing) {
        return;
    }
    Mm_StartImport(path, 1, 0);
}

/* After mods were added (n of them): the list read again, the cursor on the first one added, a line saying what
   happened. Returns the mod the cursor went to, -1 none. */
s32 Mm_ShowImport(s32 n) {
    static char note[64];
    s32 skipped = 0;
    s32 first = Port_ModImportInfo(&skipped);

    Mm_FreeFaces();
    Mm_Refresh();
    if (first == -2) {
        Mm_Note("THOSE ARE ALREADY THE LIBRARY'S OWN FILES");
        Snd_PlaySe(1, 2);
        return -1;
    }
    if (n > 0 && skipped > 0) {
        sprintf(note, "ADDED %d, SKIPPED %d ALREADY IN THE LIBRARY", (int)n, (int)skipped);
    } else if (n > 0) {
        sprintf(note, n == 1 ? "ADDED %d MOD" : "ADDED %d MODS", (int)n);
    } else if (skipped > 0) {
        sprintf(note, "ALREADY IN THE LIBRARY");
    } else {
        Mm_Note("NO CHARACTER MODS FOUND THERE");
        Snd_PlaySe(1, 2);
        return -1;
    }
    Mm_Note(note);
    Snd_PlaySe(1, n > 0 ? 1 : 2);
    if (first < 0 || first >= sMm->count) {
        return -1;
    }
    Mm_Select(first);
    return first;
}

extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead, void *unk);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health, void *items);
extern void BattleSetup_Finish(void);
#define MM_TEST_FOE 31 /* Vegeta, as he is on Namek and against the androids */

/* The Mod Manager, from the main menu until triangle on the Library. Returns 1 when it was left for a test fight:
   the battle is set up (the mod under the cursor against Vegeta, in training on the first stage) and the caller
   starts it. */
s32 PortModMgr_Run(void) {
    static u8 noItems[0x10]; /* an empty set of Z items */
    s32 test;
    s32 testChara = 0;
    s32 testCostume = 0;

    Mm_Init();
    Port_EscQuit(0); /* Esc is "back" in here, not the end of the game */
    Mx_HeaderInit();
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        f32 want;
        s32 live;

        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        if (Port_EscTake() && !sMm->finding && !sMm->typing && sMm->mode != MM_KEYBOARD) {
            gPad[0].gamePressed |= PADG_TRIANGLE; /* (while typing, escape belongs to the line) */
        }
        Snd_Update();
        ColorFade_Update();
        live = !sMm->leaving && ColorFade_IsInDone() && !sMm->importing;
        Mm_PromptReset();
        Mm_TexReset();
        Mm_ExtrasClear();
        Mm_ImportPoll();
        Mm_Dropped(live);
        Mm_Watch();
        if (!sMm->importing) { /* (while mods are added nothing of the library is touched: it is changing) */
            if (sMm->mode == MM_LIBRARY) {
                Mm_Library(live);
            } else {
                Mm2_Update(live);
            }
            Mm_ModelUpdate();
        }
        Mx_Begin();
        Mx_Background(0);
        Mm_DrawBehind();
        Mm_Scene();
        Mm_TexReset();
        Mm_DrawFrame();
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        sMm->frames++;
        sMm->modeT++;
        if (sMm->rest < 1000) {
            sMm->rest++;
        }
        if (sMm->flash > 0) {
            sMm->flash--;
        }
        if (sMm->note > 0) {
            sMm->note--;
        }
        if (sMm->confirm && sMm->confirmT < 100) {
            sMm->confirmT++;
        }
        if (sMm->menu && sMm->menuT < 100) {
            sMm->menuT++;
        }
        if (sMm->act && sMm->actT < 100) {
            sMm->actT++;
        }
        want = MM_LIST_Y + MM_PITCH * (f32)(sMm->vcur - sMm->top);
        sMm->curY += (want - sMm->curY) * 0.4f;
        if (sMm->leaving && ColorFade_IsOutDone()) {
            break;
        }
    }
    sceGsSyncPath(0, 0);
    Mx_HeaderTerm();
    Port_EscQuit(1);
    test = sMm->test;
    if (test) {
        PortModInfo *m = &sMm->info[sMm->cur];

        sMmResume = sMm->cur;
        gPortModTest = 1;
        testChara = m->chara;
        /* a new fighter is its template in a costume number that says which one it is */
        testCostume = m->isNew ? 8 + m->skin * 4 + sMm->costume : sMm->costume;
    }
    Mm_Term();
    Dma_ResetBuffers();
    if (test) {
        Battle_ClearWork();
        /* a training fight, set up as the Training's own select sets it up: no time limit, an opponent who stands */
        BattleSetup_SetRule(0, 6, 8, 0, 1, Port_ModTestStage(), 0);
        BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
        BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, NULL);
        BattleSetup_SetMember(0, 0, testChara, testCostume, 0, -1, 100.0f, noItems);
        BattleSetup_SetMember(1, 0, gMmTestFoe, 0, 0, -1, 100.0f, noItems);
        BattleSetup_Finish();
    }
    return test;
}
#endif
