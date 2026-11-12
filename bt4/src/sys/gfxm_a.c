#include "common.h"
#include "battle/battle.h"
#include "battle/btl_cam.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/heap.h"
#include "sys/math3d.h"
#include "sys/gfxm_a.h"

/*
 * Full-screen post effects, 0x102F28..0x106D60. See include/sys/gfxm_a.h for the overview.
 * Everything here is drawing: nothing reads a pad, the clock or a random generator.
 *
 * One function is INCLUDE_ASM with a behaviourally exact attempt in `#if 0` above it: StgDepthTint_Draw.
 * StgPanBlur_UpdateView matches since cleanup 4 (with two stand-in constructs, see its note) and emits the
 * file's .lit4 (0x2FC280..0x2FC290).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern void Vu0Screen_StoreMtx(Mtx44 *dst);              /* the current world-to-screen matrix */
extern void Mtx_ProjectInt(s32 *dst, Mtx44 *m, Vec4 *v); /* v through m, perspective divide, to integers */

extern void GfxClut_InitPacket(GfxPostClut *blk, u16 cbp); /* creates a CLUT texture block */
extern s32 Tex_Log2Size(s32 size);                   /* log2 of a texture size */
extern void GfxPost_DrawDepthClut(s32 mode, s32 tbp, s32 cbp, u64 alpha); /* 0x106D60, next file */

/* Stage parameter file readers (stg_a). */
extern void BtlStage_GetLightVecA(Vec4 *out);
extern s32 BtlStage_GetWaterLevel(f32 *level);
extern StgDepthTintParam *BtlStage_GetList40(s32 index);
extern StgGlareParam *BtlStage_GetList48(void);

/* GS registers the shared headers do not define. */
#define GS_TEX0_2 0x07
#define GS_TEX1_2 0x15
#define GS_SET_TEXA(ta0, aem, ta1) ((u64)(ta0) | ((u64)(aem) << 15) | ((u64)(ta1) << 32))
/* The engine's ready-made textured sprite packet (DMA tag, GIF tag, 8 registers in REGLIST form). */
typedef struct GfxPostSpritePkt {
    /* 0x00 */ union {
        u32 w; /* 0x20000005 */
        struct {
            u16 qwc;
            u16 id;
        } h;
    } tag;
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;   /* 0x10000000 */
    /* 0x0C */ u32 vif1;   /* 0x50000005 */
    /* 0x10 */ u64 gifTag; /* 0x8400000000008001: REGLIST, 8 registers */
    /* 0x18 */ u64 regs;   /* PRIM, TEX0_1, RGBAQ, UV, XYZF2, UV, XYZF2, NOP */
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ struct {
        u8 r, g, b, a;
        f32 q;
    } rgbaq;
    /* 0x38 */ struct {
        u16 u, v;
        u32 pad;
    } uv0;
    /* 0x40 */ struct {
        u64 x : 16;
        u64 y : 16;
        u64 z : 24;
        u64 f : 8;
    } xyz0;
    /* 0x48 */ struct {
        u16 u, v;
        u32 pad;
    } uv1;
    /* 0x50 */ struct {
        u64 x : 16;
        u64 y : 16;
        u64 z : 24;
        u64 f : 8;
    } xyz1;
    /* 0x58 */ u64 nop;
} GfxPostSpritePkt; /* size 0x60 */

#define GFX_RGBAQ_NEUTRAL 0x3F80000080808080 /* grey 0x80, alpha 0x80, Q = 1.0f */

/* Creates the pan blur: the CLUT block and the two views (halves of the screen in split screen). */
void StgPanBlur_Init(s32 cbp) {
    gStgPanBlur = Heap_Alloc(sizeof(StgPanBlur), 0x20, 0, 2);
    memset(gStgPanBlur, 0, sizeof(StgPanBlur));
    GfxClut_InitPacket(&gStgPanBlur->tex, cbp);
    StgPanBlur_BuildClut(gStgPanBlur->tex.clut);
    gStgPanBlur->view[0].maxAlpha = 101.0f;
    gStgPanBlur->view[1].maxAlpha = 101.0f;
    gStgPanBlur->view[0].index = 0;
    gStgPanBlur->view[1].index = 1;
    if (Battle_IsSplitScreen()) {
        gStgPanBlur->split = 1;
        gStgPanBlur->view[0].x = 0;
        gStgPanBlur->view[0].y = 0;
        gStgPanBlur->view[0].w = 0x100;
        gStgPanBlur->view[0].h = 0x1C0;
        gStgPanBlur->view[1].x = 0x100;
        gStgPanBlur->view[1].y = 0;
        gStgPanBlur->view[1].w = 0x100;
        gStgPanBlur->view[1].h = 0x1C0;
    } else {
        gStgPanBlur->split = 0;
        gStgPanBlur->view[0].x = 0;
        gStgPanBlur->view[0].y = 0;
        gStgPanBlur->view[0].w = 0x200;
        gStgPanBlur->view[0].h = 0x1C0;
        gStgPanBlur->view[1].x = 0;
        gStgPanBlur->view[1].y = 0;
        gStgPanBlur->view[1].w = 0x200;
        gStgPanBlur->view[1].h = 0x1C0;
    }
}

#ifdef PORT
extern int Port_FxOff(int bit); /* port/src/gs/gs_draw.c: an effect switched off in the settings window */
#endif

/* Updates the views (unless the battle is paused) and draws those with a strength. */
void StgPanBlur_Draw(void) {
    s32 on0 = 1;
    s32 on1 = 1;

    Gfx_AddDefaultEnv();
    Dma_AddZbuf(0xE0, 1);
    if (Battle_IsSplitScreen()) {
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            on0 = StgPanBlur_UpdateView(&gStgPanBlur->view[0]);
            on1 = StgPanBlur_UpdateView(&gStgPanBlur->view[1]);
        }
#ifdef PORT /* (the setting "Movement blur": the views are kept up to date, nothing is drawn) */
        if (Port_FxOff(32)) {
            on0 = on1 = 0;
        }
#endif
        if (on0 != 0 || on1 != 0) {
            Dma_AddData(gStgPanBlur->tex.upload, 0x10);
            Dma_AddTexFlush();
            Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0xFFFFFF);
            GfxPost_DrawDepthClut(0, GFXPOST_DEPTH_TBP, gStgPanBlur->tex.cbp, 0x44);
            StgPanBlur_CopyScreen();
            if (on0 != 0) {
                StgPanBlur_DrawView(&gStgPanBlur->view[0], gStgPanBlur->split);
            }
            if (on1 != 0) {
                StgPanBlur_DrawView(&gStgPanBlur->view[1], gStgPanBlur->split);
            }
            StgPanBlur_RestoreEnv();
        }
    } else {
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            on0 = StgPanBlur_UpdateView(&gStgPanBlur->view[0]);
        }
#ifdef PORT
        if (Port_FxOff(32)) {
            on0 = 0;
        }
#endif
        if (on0 != 0) {
            Dma_AddData(gStgPanBlur->tex.upload, 0x10);
            Dma_AddTexFlush();
            Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0xFFFFFF);
            GfxPost_DrawDepthClut(0, GFXPOST_DEPTH_TBP, gStgPanBlur->tex.cbp, 0x44);
            StgPanBlur_CopyScreen();
            StgPanBlur_DrawView(&gStgPanBlur->view[0], gStgPanBlur->split);
            StgPanBlur_RestoreEnv();
        }
    }
}

/* Frees the pan blur. */
void StgPanBlur_Term(void) {
    Heap_Free(gStgPanBlur);
}

/* CLUT of the pan blur: grey, alpha 0x80 - depth byte / 2. */
void StgPanBlur_BuildClut(u8 *clut) {
    s32 i;

    for (i = 0; i < 0x100; i++) {
        u8 *e = clut + GFXPOST_CLUT_INDEX(i) * 4;
        s32 a;

        e[0] = 0x80;
        e[1] = 0x80;
        e[2] = 0x80;
        a = 0x80 - (i >> 1);
        if (a < 0) {
            e[3] = 0;
        } else {
            e[3] = a;
        }
    }
}

/* Follows the camera of one view: strength from its horizontal speed, the two layers from that. Returns 0 when
   the strength has run out. */
/* History (it matches now, see the last note). Third cleanup: 13 of 197 instructions (was 60 of 191): the
   layer loop now matches. What remains is in the
   straight-line part in front of it: two callee-saved registers are exchanged (the original keeps
   `view->prev.m[3]` in s3 and `&move` in s4, this has them the other way round), and with that the
   `addiu s1,sp,48` / `li a2,16` pair of the fourth clear is in the other order. Same behaviour.
   What made the loop match (third cleanup): the two layers are written as ONE FLAT float array,
   `layer[i * 12 + k]`, an array member (not a pointer) indexed with the constant inside the subscript.
   The compiler then adds the whole constant (0xB0 + 4 * k) to the index before adding `view`, cse cannot
   relate the eight addresses, combine re-associates each one into `(i * 48 + view) + K`, and only the
   post-reload cse sees that the eight sums are the same value and turns seven of them into register copies:
   that is the copy chain t3 -> v1 -> a1 -> a2 -> a3 -> t1 -> t0 -> v0 of the original. (The same thing
   explains Ot_Reset in gfx_ot.c: its later loads are `slots[cur + 1]`, `slots[cur + 2]` with the constant in
   the subscript.) The four vectors are `{0, 0, 0, 1}` initialisers (same code as memset + store).
   Tried for the s3 / s4 exchange without effect (about 25 variants): pointer variables for the two matrix
   rows declared or assigned in several places, the rows written as `&m[3][0]`, the two `dist` stores in
   either order or chained, `cam` and `dist` initialised in their declarations, a local for gBtlCam or for
   the index, pointers to the two matrices. Local register allocation decides it: the two row addresses
   (three references each, born where the scheduler puts them, in front of Mtx_Copy) and the three
   `sp + N` addresses compete for s1..s5, and the original ranks `view + 0x70` between `&dirCur` and
   `&move` while this ranks both row addresses last. */
/* MATCHED in cleanup 4, with two constructs whose original form is not known (treat as stand-ins):
   - `side.y` is read through a pointer variable declared right behind `side` (`Vec4 *sp = &side;`). The pointer
     itself disappears (it is sp + 0), but its one RTL instruction in the first block changes where the
     scheduler puts the two row addresses, and s3 / s4 come out as in the original (13 -> 2 differing). It
     must be declared directly behind `side` or `move`; a pointer to another vector, a later assignment, an
     inline reader, or `dist` / `cam` initialised among the vectors do not do it.
   - `do { dist = Vec3_Length(&move); } while (0);` (found by the permuter): without it one pair of argument
     loads of the second memset is swapped (`move a0,s4 / move a1,zero`). A plain block does not do it; the
     loop notes are what matters (they bound the first scheduling region). Reads like a statement macro.
   The function emits the file's whole .lit4: 0.1f, 0.15f, 0.1f, 0.15f (0x2FC280..0x2FC28C, bits compared). */
typedef struct StgPanBlurViewF {
    u8 pad[0xB0];
    f32 layer[24];
} StgPanBlurViewF;
#define L(i, k) ((StgPanBlurViewF *)view)->layer[(i) * 12 + (k)]
s32 StgPanBlur_UpdateView(StgPanBlurView *view) {
    Vec4 side = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec4 *sp = &side;
    Vec4 move = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec4 dirCur = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec4 dirPrev = { 0.0f, 0.0f, 0.0f, 1.0f };
    BtlCamView *cam;
    f32 dist;
    s32 i;

    cam = &gBtlCam->views[view->index];
    dist = 0.0f;
    Mtx_Copy(&view->prev, &view->cur);
    Mtx_InverseRT(&view->cur, &cam->view.world2view2);
    view->cur.m[3][1] = dist;
    view->prev.m[3][1] = dist;
    Vec3_Normalize(&dirCur, (Vec4 *)view->cur.m[3]);
    Vec3_Normalize(&dirPrev, (Vec4 *)view->prev.m[3]);
    Vec3_Cross(&side, &dirPrev, &dirCur);
    Vec4_Sub(&move, (Vec4 *)view->prev.m[3], (Vec4 *)view->cur.m[3]);
    do {
        dist = Vec3_Length(&move);
    } while (0);
    if (dist >= 1.0f) {
        if (dist < 2.5f) {
            view->strength += 0.1f;
        }
        if (dist >= 2.5f && dist < 4.0f) {
            view->strength -= 0.15f;
        }
        if (dist > 4.0f) {
            view->strength += 0.1f;
        }
    } else {
        view->strength -= 0.15f;
    }
    if (view->strength > 1.0f) {
        view->strength = 1.0f;
    }
    if (view->strength < 0.0f) {
        view->strength = 0.0f;
        return 0;
    }
    view->side = sp->y * 100.0f;
    if (view->side > 1.0f) {
        view->side = 1.0f;
    }
    if (view->side < -1.0f) {
        view->side = -1.0f;
    }
    for (i = 0; i < 2; i++) {
        s32 a = view->maxAlpha * view->strength - (i * 32);

        if (a < 0) {
            a = 0;
        }
        L(i, 0) = (i + 1) * view->side;
        L(i, 1) = 0.0f;
        L(i, 2) = 0.0f;
        L(i, 3) = 1.0f;
        L(i, 4) = 128.0f;
        L(i, 5) = 128.0f;
        L(i, 6) = 128.0f;
        L(i, 7) = a;
    }
    return 1;
}


/* Copies the frame, halved, to the 256x256 work page. */
void StgPanBlur_CopyScreen(void) {
    u64 *p;
    s32 i;
    s32 x0;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(9, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 1;
    p[1] = GS_COLCLAMP;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x40150;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7800, 0x7800);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0xFF, 0, 0xFF);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0x1FF, 0, 0x1BF);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = GIF_TAG(6, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x2000000664020E00 : 0x2000000664020000;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x116;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GIF_TAG_EX(16, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    x0 = 0;
    for (i = 0; i < 16; i++) {
        p[0] = (i * 32) << 4;
        p[1] = ((x0 + 0x780) << 4) | (0x7800 << 16);
        p += 2;
        p[0] = (((i + 1) * 32) << 4) | (0x1C00 << 16);
        p[1] = (s64)((x0 + 0x790) << 4) | ((u64)0x8800 << 16);
        p += 2;
        x0 += 16;
    }
    Dma_EndDirect(p);
}

/* Draws one view of the pan blur. The work page (256x256 copy of the frame, or the view's half of it in split
   screen) goes to the 128x128 page 0x170 with alpha = strength; the two layers are blended over it, shifted
   sideways; the result is stretched back over the view's rectangle of the frame, blended by its alpha. */
/* Matching notes: the strip x of the layer loop is `i * step + x - r` / `(i + 1) * step + r + x` (the operand
   order decides the order in which the loop pass emits the four start values, and with it the registers);
   in the last loop the view's x is a plain running variable (`x += 32`, read BEFORE `step` is computed, so
   that `view` is dead when `u` is created and u takes its register), the packet is filled in its natural
   member order (prim first; the scheduler emits the stores whose source register dies first), and the
   per-strip members are written texels first, then the two XYZ words. */
void StgPanBlur_DrawView(StgPanBlurView *view, s32 split) {
    GfxPostSpritePkt pkt;
    u64 *p;
    s32 shift = 0;
    s32 srcX = 0;

    if (split != 0) {
        srcX = 0x80;
        shift = 1;
        if (view->index == 0) {
            srcX = 0;
        }
    }
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(9, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0x20170;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7C00, 0x7C00);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x7F, 0, 0x7F);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0xFF, 0, 0xFF);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = ((u64)Tex_Log2Size(0x100) << 26) | ((u64)Tex_Log2Size(0x100) << 30) | 0x400012A00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x116;
    p[1] = GS_PRIM;
    p += 2;
#ifdef PORT
    /* clang's software-float code generator cannot convert a float to a 64-bit integer on 32-bit x86 (it stops
       with an internal error; the 64-bit target is fine). The value is an alpha, 0..128: through 32 bits it is
       the same. */
    p[0] = ((u64)(u32)(view->maxAlpha * view->strength) << 24) | 0x3F80000000808080;
#else
    p[0] = ((u64)(view->maxAlpha * view->strength) << 24) | 0x3F80000000808080;
#endif
    p[1] = GS_RGBAQ;
    p += 2;
    {
        s32 n = (0x100 >> shift) / 32;
        s32 step = 0x80 / n;
        s32 i;

        p[0] = GIF_TAG_EX(n, 0, GIF_FLG_REGLIST, 4);
        p[1] = 0x5353;
        p += 2;
        for (i = 0; i < n; i++) {
            p[0] = (srcX + i * 32) << 4;
            p[1] = (0x7C00 + ((i * step) << 4)) | (0x7C00 << 16);
            p += 2;
            p[0] = ((srcX + (i + 1) * 32) << 4) | (0x1000 << 16);
            p[1] = (s64)(0x7C00 + (((i + 1) * step) << 4)) | ((u64)0x8400 << 16);
            p += 2;
        }
    }
    {
    s32 size = 0x100 >> shift;
    s32 n;
    s32 step;
    StgPanBlurLayer *layer;
    s32 k;

    p[0] = GIF_TAG(1, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0xFF00000000020170;
    p[1] = GS_FRAME_1;
    p += 2;
    layer = view->layer;
    n = size / size;
    step = 0x80 / n;
    for (k = 0; k < 2; k++, layer++) {
        s32 x;
        s32 y;
        s32 r;
        s32 i;

        p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 2);
        p[1] = 0x10;
        p += 2;
        p[0] = 0x156;
#ifdef PORT
        p[1] = ((u64)(u32)layer->color.w << 24) | 0x3F80000000808080; /* as above: an alpha, same value */
#else
        p[1] = ((u64)layer->color.w << 24) | 0x3F80000000808080;
#endif
        p += 2;
        p[0] = GIF_TAG_EX(n, 0, GIF_FLG_REGLIST, 4);
        p[1] = 0x5353;
        p += 2;
        x = layer->ofs.x;
        y = layer->ofs.y;
        r = layer->size;
        for (i = 0; i < n; i++) {
            p[0] = (((srcX + i * size) << 4) + 8) | (8 << 16);
            p[1] = (((i * step + x - r) << 4) + 0x7C00) | ((s64)(((y - r) << 4) + 0x7C00) << 16);
            p += 2;
            p[0] = (((srcX + (i + 1) * size) << 4) + 8) | (0x1008 << 16);
            p[1] = ((((i + 1) * step + r + x) << 4) + 0x7C00) | ((s64)((y + r + 0x840) << 4) << 16);
            p += 2;
        }
    }
    }
    p[0] = GIF_TAG(8, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0xFF00000000080070 : 0xFF00000000080000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7000, 0x7200);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = 0x5DC00AE00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = (s64)view->x | ((s64)(view->x + view->w - 1) << 16) | ((s64)view->y << 32) |
           ((s64)(view->y + view->h - 1) << 48);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    {
    s32 n = view->w / 32;
    s32 x = view->x;
    s32 step = 0x80 / n;
    s32 u = 0;
    s32 u1;
    s32 i;

    pkt.prim = 0x156;
    pkt.tag.w = 0x20000005;
    pkt.next = NULL;
    pkt.vif0 = 0x10000000;
    pkt.vif1 = 0x50000005;
    pkt.gifTag = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 8);
    pkt.regs = 0xF4343160;
    pkt.tex0 = 0x5DC00AE00;
    pkt.rgbaq.r = 0x80;
    pkt.rgbaq.g = 0x80;
    pkt.rgbaq.b = 0x80;
    pkt.rgbaq.a = 0x80;
    pkt.rgbaq.q = 1.0f;
    for (i = 0; i < n; i++) {
        u1 = u + step;
        pkt.uv0.u = (u << 4) + 8;
        pkt.uv0.v = 8;
        pkt.uv1.u = (u1 << 4) + 8;
        pkt.uv1.v = 0x7F8;
        pkt.xyz0.x = (x << 4) + 0x7000;
        pkt.xyz0.y = 0x7200;
        pkt.xyz0.z = 0;
        pkt.xyz0.f = 0xFF;
        pkt.xyz1.x = ((x + 32) << 4) + 0x7000;
        pkt.xyz1.y = 0x8E00;
        pkt.xyz1.z = 0;
        pkt.xyz1.f = 0xFF;
        u = u1;
        x += 32;
        memcpy(p, &pkt.gifTag, pkt.tag.h.qwc << 4);
        p = (u64 *)((u8 *)p + (pkt.tag.h.qwc << 4));
    }
    }
    Dma_EndDirect(p);
}


/* Back to the frame's colour buffer with the default offset, scissor and depth writes. */
void StgPanBlur_RestoreEnv(void) {
    u64 *p;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(6, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7000, 0x7200);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x1FF, 0, 0x1BF);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0x70000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 0);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_CLAMP_1;
    p += 2;
    Dma_EndDirect(p);
}

/*
 * The glow pass. The frame's destination alpha is the mask (set up by the caller through a depth CLUT).
 *   1. clear the 256x256 work page (0x150) and draw the frame into it, halved, blended by the frame's alpha and
 *      tinted with `color`;
 *   2. halve it again to 128x128 (page 0x170), and again to 64x64 (page 0x1F6, two sprites one texel apart);
 *   3. `passes` (rounded up to even) blur passes between the 64x64 pages 0x1F6 and 0x1F8, bilinear, every other
 *      one shifted by a texel, each averaged 50/50 with what is there;
 *   4. back up to 128x128 tinted with `glow`, to 256x256, and added to the frame (ALPHA 0x68);
 *   5. restore the frame environment.
 */
/* The two up-scaling loops (4 and 8 strips) are written the way the induction variables require: the two texel
   coordinates go through `s64` temporaries (so that the `|` with the V half is not narrowed to 32 bits and they
   step as 64-bit values), and the first corner's X comes from `i`, not from `x` (so the loop cannot be reversed
   before the second loop pass, which puts the counter's initial value behind that corner's). */
#ifdef PORT
extern void Port_GsMarker(s32 effect); /* port/src/gs_marker.c */
#define PORT_GLOW_BEGIN 0x20 /* the PC renderer draws the chain's work pages at the PS2's resolution (gs_draw.c) */
#define PORT_GLOW_END 0x21
#endif
void GfxPost_DrawGlow(s32 passes, u32 color, u32 glow) {
    u64 *p;
    s32 i;
    s32 x;

    passes += passes & 1;
#ifdef PORT
    Port_GsMarker(PORT_GLOW_BEGIN);
#endif
    Gfx_AddDefaultEnv();
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(13, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 1;
    p[1] = GS_COLCLAMP;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_2;
    p += 2;
    p[0] = 0x40150;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7800, 0x7800);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0xFF, 0, 0xFF);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0x1FF, 0, 0x1BF);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x46;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = 0x3F80000080000000;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GIF_TAG_EX(1, 0, GIF_FLG_REGLIST, 2);
    p[1] = 0x55;
    p += 2;
    p[0] = 0x78007800;
    p[1] = 0x88008800;
    p += 2;
    p[0] = GIF_TAG(6, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x2000000664020E00 : 0x2000000664020000;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = (u64)color | GFX_CLEAR_RGBAQ;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GIF_TAG_EX(16, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    x = 0;
    for (i = 0; i < 16; i++) {
        p[0] = (i * 32) << 4;
        p[1] = ((x + 0x780) << 4) | (0x7800 << 16);
        p += 2;
        p[0] = (((i + 1) * 32) << 4) | (0x1C00 << 16);
        p[1] = (s64)((x + 0x790) << 4) | ((u64)0x8800 << 16);
        p += 2;
        x += 16;
    }
    p[0] = GIF_TAG(9, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0x20170;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7C00, 0x7C00);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x7F, 0, 0x7F);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0xFF, 0, 0xFF);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = ((u64)Tex_Log2Size(0x100) << 26) | ((u64)Tex_Log2Size(0x100) << 30) | 0x12A00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GIF_TAG_EX(8, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    x = 0;
    for (i = 0; i < 8; i++) {
        p[0] = (i * 32) << 4;
        p[1] = ((x + 0x7C0) << 4) | (0x7C00 << 16);
        p += 2;
        p[0] = (((i + 1) * 32) << 4) | (0x1000 << 16);
        p[1] = (s64)((x + 0x7D0) << 4) | ((u64)0x8400 << 16);
        p += 2;
        x += 16;
    }
    p[0] = GIF_TAG(10, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0x101F6;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7E00, 0x7E00);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x3F, 0, 0x3F);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0x7F, 0, 0x7F);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = ((u64)Tex_Log2Size(0x80) << 26) | ((u64)Tex_Log2Size(0x80) << 30) | 0xAE00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GIF_TAG_EX(2, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    p[0] = 0;
    p[1] = 0x7E007E00;
    p += 2;
    p[0] = 0x08000800;
    p[1] = 0x82008200;
    p += 2;
    p[0] = 0x00100010;
    p[1] = 0x7E007E00;
    p += 2;
    p[0] = 0x08100810;
    p[1] = 0x82008200;
    p += 2;
    p[0] = GIF_TAG(15, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x101F8;
    p[1] = GS_FRAME_2;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_2;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x3F, 0, 0x3F);
    p[1] = GS_SCISSOR_2;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7E00, 0x7E00);
    p[1] = GS_XYOFFSET_2;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_2;
    p += 2;
    p[0] = ((u64)Tex_Log2Size(0x40) << 26) | ((u64)Tex_Log2Size(0x40) << 30) | 0x7F00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = ((u64)Tex_Log2Size(0x40) << 26) | ((u64)Tex_Log2Size(0x40) << 30) | 0x7EC0;
    p[1] = GS_TEX0_2;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0x3F, 0, 0x3F);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0x3F, 0, 0x3F);
    p[1] = GS_CLAMP_2;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_2;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_2;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_2;
    p += 2;
    p[0] = GIF_TAG_EX(passes, 0, GIF_FLG_REGLIST, 6);
    p[1] = 0x535310;
    p += 2;
    for (i = 0; i < passes; i++) {
        s32 u;
        s32 v;

        if (i & 1) {
            u = 0;
            v = 0;
        } else {
            u = 0x10;
            v = 0x10;
        }
        p[0] = !(i & 1) ? 0x356 : 0x156;
        p[1] = GFX_RGBAQ_NEUTRAL;
        p += 2;
        p[0] = u | ((s64)v << 16);
        p[1] = 0x7E007E00;
        p += 2;
        p[0] = (u + 0x400) | ((s64)(v + 0x400) << 16);
        p[1] = 0x82008200;
        p += 2;
    }
    p[0] = GIF_TAG(10, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x20170;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7C00, 0x7C00);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x7F, 0, 0x7F);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0x3F, 0, 0x3F);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = (u64)glow | GFX_CLEAR_RGBAQ;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GIF_TAG_EX(1, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    p[0] = 0x00040004;
    p[1] = 0x7C007C00;
    p += 2;
    p[0] = 0x04040404;
    p[1] = 0x84008400;
    p += 2;
    p[0] = GIF_TAG(12, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x40150;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7800, 0x7800);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0xFF, 0, 0xFF);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0x7F, 0, 0x7F);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = ((u64)Tex_Log2Size(0x80) << 26) | ((u64)Tex_Log2Size(0x80) << 30) | 0xAE00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = GIF_TAG_EX(4, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    x = 0;
    for (i = 0; i < 4; i++) {
        s64 u0 = (x << 4) + 8;
        s64 u1 = ((x + 32) << 4) + 8;

        p[0] = u0 | ((u64)8 << 16);
        p[1] = ((i * 64 + 0x780) << 4) | (0x7800 << 16);
        p += 2;
        p[0] = u1 | ((u64)0x808 << 16);
        p[1] = (s64)((x * 2 + 0x7C0) << 4) | ((u64)0x8800 << 16);
        p += 2;
        x += 32;
    }
    p[0] = GIF_TAG(10, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7000, 0x7200);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x1FF, 0, 0x1BF);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = GS_SET_CLAMP(2, 2, 0, 0xFF, 0, 0xFF);
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = ((u64)Tex_Log2Size(0x100) << 26) | ((u64)Tex_Log2Size(0x100) << 30) | 0x12A00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = 0x8000000068;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = GIF_TAG_EX(8, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    x = 0;
    for (i = 0; i < 8; i++) {
        s64 u0 = (x << 4) + 8;
        s64 u1 = ((x + 32) << 4) + 8;

        p[0] = u0 | ((u64)8 << 16);
        p[1] = ((i * 64 + 0x700) << 4) | (0x7200 << 16);
        p += 2;
        p[0] = u1 | ((u64)0x1008 << 16);
        p[1] = (s64)((x * 2 + 0x740) << 4) | ((u64)0x8E00 << 16);
        p += 2;
        x += 32;
    }
    p[0] = GIF_TAG(6, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7000, 0x7200);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x1FF, 0, 0x1BF);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0x70000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_CLAMP_1;
    p += 2;
    Dma_EndDirect(p);
#ifdef PORT
    Port_GsMarker(PORT_GLOW_END);
#endif
}

/* Draws a 16-bit texture over the screen, one line up, with the given blend. */
void GfxPost_DrawTex16(s32 tbp, u64 alpha) {
    GfxPostQuad q;
    u64 *p;
    u64 *tag;
    s32 i;
    s32 u1;
    s32 step = 32;
    s32 u = 0;
    s32 x = 0;
    s32 half = 1;
    s32 w = 32;
    s32 h = 448;
    s32 n = 512 / step;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(8, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = alpha;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = 0x8000000080;
    p[1] = GS_TEXA;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = (u32)(tbp | 0x24220000) | 0xE40000000;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    for (i = 0; i < n; i++) {
        u1 = u + step;
        GfxPostQuad_Set(&q, 0x700, 0x720, u, 0, u1, h, x, 0, w, h, half);
        p[0] = (s64)q.u0 | ((s64)q.v0 << 16);
        p[1] = (s64)q.x0 | ((s64)(q.y0 - 0x10) << 16);
        p += 2;
        p[0] = (s64)q.u1 | ((s64)q.v1 << 16);
        p[1] = (s64)q.x1 | ((s64)(q.y1 - 0x10) << 16);
        p += 2;
        u = u1;
        x += step;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 4);
    tag[1] = 0x5353;
    Dma_EndDirect(p);
}

/* Queues FRAME_1: a 16-bit page, 512 wide, with a write mask. */
void GfxPost_AddFrame16(s32 fbp, s32 fbmsk) {
    s32 lo = fbp | 0x2080000;
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        lo, (s64)(lo | ((s64)fbmsk << 32)) >> 32, GS_FRAME_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

typedef struct GfxPostPkt40 {
    u32 w[16];
} GfxPostPkt40;

/* Queues FRAME_1 = the frame's colour buffer and the default XYOFFSET_1. */
void GfxPost_AddScreenFrame(void) {
    GfxPostPkt40 pkt = {{
        DMA_TAG_CNT | 3, 0, VIF_FLUSHE, VIF_DIRECT | 3,
        GIF_EOP | 2, 0x10000000, GIF_REG_AD, 0,
        !(gGfx.frame & 1) ? 0x80070 : 0x80000, 0, GS_FRAME_1, 0,
        0x7000, 0x7200, GS_XYOFFSET_1, 0,
    }};

    Dma_AddData(&pkt, sizeof(pkt));
}

/* CLUT of the outline pass: id byte -> red and green (id * 8, or 0x80), id 0xFF -> nothing. */
void ObjOutline_BuildClut(u8 *clut) {
    s32 i;

    for (i = 0; i < 0x100; i++) {
        s32 k = GFXPOST_CLUT_INDEX(i);

        k *= 4;
        if (i == 0xFF) {
            clut[k + 0] = 0;
            clut[k + 1] = 0;
            clut[k + 2] = 0;
            clut[k + 3] = 0;
        } else {
            u8 c = i << 3;

            if (c == 0) {
                c = 0x80;
            }
            clut[k + 0] = c;
            clut[k + 1] = c;
            clut[k + 2] = 0;
            clut[k + 3] = 0x80;
        }
    }
}

/* Draws the depth page through a CLUT over the screen, shifted by (dx, dy) sixteenths of a pixel. */
void GfxPost_DrawDepthClutAt(s32 half, s32 tbp, s32 cbp, u64 alpha, s32 dx, s32 dy) {
    GfxPostQuad q;
    u64 *p;
    u64 *tag;
    s32 i;
    s32 u1;
    s32 step = 32;
    s32 u = 0;
    s32 x = 0;
    s32 w = 32;
    s32 h = 448;
    s32 n = 512 / step;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(10, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_COLCLAMP;
    p += 2;
    p[0] = alpha;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = 0x8000;
    p[1] = GS_TEXA;
    p += 2;
    p[0] = GS_SET_XYOFFSET(dx + 0x7000, dy + 0x7200);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    if (half != 0) {
        p[0] = 0x60;
        p[1] = GS_TEX1_1;
        p += 2;
    } else {
        p[0] = 0;
        p[1] = GS_TEX1_1;
        p += 2;
    }
    p[0] = (u32)(tbp | 0x25B20000) | ((u64)cbp << 37) | 0x2000000E40000000;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    for (i = 0; i < n; i++) {
        u1 = u + step;
        GfxPostQuad_Set(&q, 0x700, 0x720, u, 0, u1, h, x, 0, w, h, half);
        p[0] = (s64)q.u0 | ((s64)q.v0 << 16);
        p[1] = (s64)q.x0 | ((s64)q.y0 << 16);
        p += 2;
        p[0] = (s64)q.u1 | ((s64)q.v1 << 16);
        p[1] = (s64)q.x1 | ((s64)q.y1 << 16);
        p += 2;
        u = u1;
        x += step;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 4);
    tag[1] = 0x5353;
    Dma_EndDirect(p);
}

/* Draws a 16-bit texture over the screen, one line up, bilinear, skipping texels of alpha 0 (TEST 0x3000F);
   `ta0` is the alpha given to texels whose alpha bit is clear (TEXA). */
void GfxPost_DrawTex16Texa(s32 tbp, u64 alpha, u8 ta0) {
    GfxPostQuad q;
    u64 *p;
    u64 *tag;
    s32 i;
    s32 u1;
    s32 step = 32;
    s32 u = 0;
    s32 x = 0;
    s32 half = 1;
    s32 w = 32;
    s32 h = 448;
    s32 n = 512 / step;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(8, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x3000F;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = alpha;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = GS_SET_TEXA(ta0, 1, 0);
    p[1] = GS_TEXA;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = (u32)(tbp | 0x24220000) | 0xE40000000;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    for (i = 0; i < n; i++) {
        u1 = u + step;
        GfxPostQuad_Set(&q, 0x700, 0x720, u, 0, u1, h, x, 0, w, h, half);
        p[0] = (s64)q.u0 | ((s64)q.v0 << 16);
        p[1] = (s64)q.x0 | ((s64)(q.y0 - 0x10) << 16);
        p += 2;
        p[0] = (s64)q.u1 | ((s64)q.v1 << 16);
        p[1] = (s64)q.x1 | ((s64)(q.y1 - 0x10) << 16);
        p += 2;
        u = u1;
        x += step;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 4);
    tag[1] = 0x5353;
    Dma_EndDirect(p);
}

/* Blends an untextured rectangle over the screen. */
void GfxPost_FillBlend(u64 alpha, u64 rgbaq) {
    u64 *p;
    u64 *tag;
    s32 i;
    s32 x0;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(6, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 1;
    p[1] = GS_COLCLAMP;
    p += 2;
    p[0] = alpha;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x46;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = rgbaq;
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    x0 = 0;
    for (i = 0; i < 16; i++) {
        p[0] = ((x0 + 0x700) << 4) | (0x7200 << 16);
        p[1] = (s64)((x0 + 0x720) << 4) | ((u64)0x8E00 << 16);
        p += 2;
        x0 += 32;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 2);
    tag[1] = 0x55;
    Dma_EndDirect(p);
}

/* Points FRAME_1 at a 16-bit page, clears it to black and then sets its write mask. */
void GfxPost_ClearWork(s32 fbp, s32 fbmsk) {
    u64 *p;
    u64 *tag;
    s32 i;
    s32 x0;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(8, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = fbp | 0x2080000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7000, 0x7200);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x46;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_CLEAR_RGBAQ;
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    x0 = 0;
    for (i = 0; i < 16; i++) {
        p[0] = ((x0 + 0x700) << 4) | (0x7200 << 16);
        p[1] = (s64)((x0 + 0x720) << 4) | ((u64)0x8E00 << 16);
        p += 2;
        x0 += 32;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 2);
    tag[1] = 0x55;
    p[0] = GIF_TAG(1, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = (fbp | 0x2080000) | ((s64)fbmsk << 32);
    p[1] = GS_FRAME_1;
    p += 2;
    Dma_EndDirect(p);
}

/* Queues XYOFFSET_1. */
void GfxPost_AddXyOffset(s32 x, s32 y) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, VIF_FLUSHE, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        x, (s64)(x | ((s64)y << 32)) >> 32, GS_XYOFFSET_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Creates the outline pass. */
void ObjOutline_Init(void) {
    gObjOutline = Heap_Alloc(sizeof(ObjOutline), 0x20, 0, 2);
    memset(gObjOutline, 0, sizeof(ObjOutline));
    GfxClut_InitPacket(&gObjOutline->tex, 0x3E8C);
    ObjOutline_BuildClut(gObjOutline->tex.clut);
}

/* Frees the outline pass. */
void ObjOutline_Term(void) {
    Heap_Free(gObjOutline);
    gObjOutline = NULL;
}

#ifdef PORT
/* A marker in the display list for the PC renderer (port/src/gs_marker.c). */
#define PORT_FX_OUTLINE 1
extern void Port_GsMarker(s32 effect);
#endif

/* The outline pass: edges of the id image into the frame's alpha, then a dark rectangle blended by it. */
void ObjOutline_Draw(void) {
#ifdef PORT
    Port_GsMarker(PORT_FX_OUTLINE); /* the GPU renderer draws the outline natively here and drops the passes below */
#endif
    Dma_AddData(gObjOutline->tex.upload, 0x10);
    Dma_AddTexFlush();
    GfxPost_ClearWork(GFXPOST_WORK_FBP, 0xFF000000);
    GfxPost_DrawDepthClutAt(0, GFXPOST_DEPTH_TBP, gObjOutline->tex.cbp, 0x8000000044, 0, 0);
    GfxPost_AddFrame16(GFXPOST_WORK_FBP, 0xFFFFFF00);
    GfxPost_DrawDepthClutAt(0, GFXPOST_DEPTH_TBP, gObjOutline->tex.cbp, 0x8000000062, 0, 8);
    GfxPost_AddFrame16(GFXPOST_WORK_FBP, 0xFFFF00FF);
    GfxPost_DrawDepthClutAt(0, GFXPOST_DEPTH_TBP, gObjOutline->tex.cbp, 0x8000000062, 0x10, 0);
    GfxPost_AddScreenFrame();
    Gfx_ClearScreen(0xFFFFFF, 0);
    Dma_AddZbuf(0xE0, 1);
    Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0xFFFFFF);
    Dma_AddColClamp(1);
    GfxPost_AddXyOffset(0x7000, 0x7210);
    GfxPost_DrawTex16Texa(GFXPOST_WORK_TBP, 0x8000000064, 0x30);
    GfxPost_AddXyOffset(0x7000, 0x71F0);
    GfxPost_DrawTex16Texa(GFXPOST_WORK_TBP, 0x8000000064, 0x30);
    GfxPost_AddXyOffset(0x7000, 0x7200);
    GfxPost_DrawTex16Texa(GFXPOST_WORK_TBP, 0x44, 0x80);
    Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0);
    GfxPost_FillBlend(0x52, 0x3F80000080646464);
}

/* CLUT of the sky glare: white; alpha = the stage's value, 0x80 for depth byte 0xFF. */
void StgGlare_BuildClut(u8 *clut) {
    s32 i;

    for (i = 0; i < 0x100; i++) {
        s32 k = GFXPOST_CLUT_INDEX(i) * 4;

        if (i == 0xFF) {
            clut[k + 0] = 0xFF;
            clut[k + 1] = 0xFF;
            clut[k + 2] = 0xFF;
            clut[k + 3] = 0x80;
        } else {
            clut[k + 0] = 0xFF;
            clut[k + 1] = 0xFF;
            clut[k + 2] = 0xFF;
            clut[k + 3] = gStgGlare.alpha;
        }
    }
}

/* Nothing. */
void StgGlare_Nop(void) {
}

/* Steps the glare level: up while the stage's light is on screen, down otherwise. */
void StgGlare_Update(void) {
    Mtx44 m;
    Vec4 light;
    s32 scr[4];
    s32 pt[3];
    View *view = gBtlCamView;

    if (gStgGlare.enabled != 0) {
        if (gStgGlare.track != 0) {
            BtlStage_GetLightVecA(&light);
            Vu0Screen_StoreMtx(&m);
            Mtx_ProjectInt(scr, &m, &light);
            pt[0] = scr[0] - 1792.0f;
            pt[1] = scr[1] - 1824.0f;
            pt[2] = scr[2];
            if (pt[2] > 0 && pt[0] >= view->scissorX0 && pt[0] <= view->scissorX1 + 1 && pt[1] >= view->scissorY0 &&
                pt[1] <= view->scissorY1 + 1) {
                if ((u32)gStgGlare.state < 2) {
                    gStgGlare.state = 1;
                    gStgGlare.level += gStgGlare.step;
                    if (gStgGlare.level > gStgGlare.max) {
                        gStgGlare.level = gStgGlare.max;
                        gStgGlare.state = 2;
                    }
                } else {
                    gStgGlare.level -= gStgGlare.step;
                    if (gStgGlare.level < gStgGlare.hold) {
                        gStgGlare.level = gStgGlare.hold;
                    }
                }
            } else {
                gStgGlare.level--;
                gStgGlare.state = 0;
                if (gStgGlare.level < gStgGlare.min) {
                    gStgGlare.level = gStgGlare.min;
                }
                gStgGlare.unk4DC = gStgGlare.step;
            }
        } else {
            gStgGlare.level = gStgGlare.max;
            gStgGlare.unk4DC = gStgGlare.step;
        }
        if (gStgGlare.reset != 0) {
            gStgGlare.level = gStgGlare.max;
            gStgGlare.unk4DC = gStgGlare.step;
            gStgGlare.reset = 0;
        } else {
            gStgGlare.reset = 0;
        }
    }
}

/* Restarts the level of a glare that is off. */
void StgGlare_Reset(void) {
    if (gStgGlare.enabled == 0) {
        gStgGlare.reset = 0;
        gStgGlare.state = 0;
        gStgGlare.level = gStgGlare.step;
    }
}

/* Creates the sky glare. */
void StgGlare_Init(u16 cbp) {
    memset(&gStgGlare, 0, sizeof(StgGlare));
    GfxClut_InitPacket(&gStgGlare.tex, cbp);
    StgGlare_Reset();
}

/* Nothing (the block is static). */
void StgGlare_Term(void) {
}

/* The sky glare: clears the frame's alpha, writes the CLUT alpha by depth, then the glow pass. */
void StgGlare_Draw(void) {
    if (gStgGlare.enabled != 0) {
        Gfx_AddDefaultEnv();
        Dma_AddData(gStgGlare.tex.upload, 0x10);
        Dma_AddTexFlush();
        Dma_AddZbuf(0xE0, 1);
        Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0xFFFFFF);
        Dma_AddFillRect(0x700, 0x720, 0x200, 0x1C0, 0);
        Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0xFFFFFF);
        GfxPost_DrawDepthClut(0, GFXPOST_DEPTH_TBP, gStgGlare.tex.cbp, 0x44);
        Dma_AddTexFlush();
        GfxPost_DrawGlow(10, 0x80808080, (gStgGlare.level << 16) | (gStgGlare.level << 8) | gStgGlare.level);
    }
}

/* Takes the glare parameters from the stage file (defaults: off) and rebuilds the CLUT. */
void StgGlare_LoadParams(void) {
    StgGlareParam *param = BtlStage_GetList48();

    if (param == NULL) {
        gStgGlare.alpha = 0x40;
        gStgGlare.step = 0x20;
        gStgGlare.max = 0x80;
        gStgGlare.hold = 0x50;
        gStgGlare.min = 4;
        gStgGlare.track = 0;
        gStgGlare.enabled = 0;
    } else {
        gStgGlare.enabled = param->flags & 1;
        gStgGlare.track = (param->flags >> 1) & 1;
        gStgGlare.step = param->step;
        gStgGlare.max = param->max;
        gStgGlare.hold = param->hold;
        gStgGlare.min = param->min;
        gStgGlare.alpha = param->alpha;
    }
    StgGlare_BuildClut(gStgGlare.tex.clut);
}

/* CLUT of a depth tint: curve(depth) scales the colour (additive) or the alpha (blended). */
void StgDepthTint_BuildClut(u8 *clut, StgDepthTint *tint) {
    f32 table[0x100];
    s32 c[3];
    s32 i;

    memset(table, 0, sizeof(table));
    StgCurve_Build(&tint->curve, table);
    if (tint->additive != 0) {
        for (i = 0; i < 0x100; i++) {
            s32 k = GFXPOST_CLUT_INDEX(i);

            k *= 4;
            if (tint->clearLast != 0 && i == 0xFF) {
                clut[k + 0] = 0;
                clut[k + 1] = 0;
                clut[k + 2] = 0;
                clut[k + 3] = 0;
            } else {
                f32 r = tint->r;
                f32 g = tint->g;
                f32 b = tint->b;
                f32 a = tint->a;

                c[0] = table[i] * r * 128.0f * a;
                clut[k + 0] = c[0];
                c[1] = table[i] * g * 128.0f * a;
                clut[k + 1] = c[1];
                c[2] = table[i] * b * 128.0f * a;
                clut[k + 2] = c[2];
                clut[k + 3] = 0x80;
            }
        }
    } else {
        for (i = 0; i < 0x100; i++) {
            s32 k = GFXPOST_CLUT_INDEX(i);

            k *= 4;
            if (tint->clearLast != 0 && i == 0xFF) {
                clut[k + 0] = 0;
                clut[k + 1] = 0;
                clut[k + 2] = 0;
                clut[k + 3] = 0;
            } else {
                c[0] = tint->r * 255.0f;
                c[1] = tint->g * 255.0f;
                c[2] = tint->b * 255.0f;
                clut[k + 0] = c[0];
                clut[k + 1] = c[1];
                clut[k + 2] = c[2];
                clut[k + 3] = (u32)(table[i] * 128.0f * tint->a);
            }
        }
    }
}

/* Rebuilds both CLUTs. */
void StgDepthTint_Rebuild(void) {
    StgDepthTint *tint = gStgDepthTint;
    s32 i;

    for (i = 0; i < 2; i++, tint++) {
        StgDepthTint_BuildClut(tint->tex.clut, tint);
    }
}

/* Creates the two depth tints. Both get the same CLUT block, cbp + 4. */
void StgDepthTint_Init(u16 cbp) {
    s32 i;

    for (i = 0; i < 2; i++) {
        memset(&gStgDepthTint[i], 0, sizeof(StgDepthTint));
        GfxClut_InitPacket(&gStgDepthTint[i].tex, cbp + 4);
    }
}

/* Nothing (the blocks are static). */
void StgDepthTint_Term(void) {
}

/* Draws the tint of the medium the camera is in. */
/* NOT MATCHING (51 of 59 instructions): the original keeps a zero in a callee-saved register across
   Battle_IsSplitScreen() and copies it to the medium index on the split-screen path; here the constant is
   propagated, which shifts the register allocation. Same behaviour. */
/* Cleanup pass 2: with a second variable (`s32 zero = 0;` at the top, `medium` assigned in every arm and
   `else { medium = zero; }` for the split-screen case) the code is the original's except for one thing: cse
   folds `medium = zero` to `move v1,zero`, so the zero is not kept in s0 across Battle_IsSplitScreen
   (28 of 65 aligned by count, but only the `move s0,zero` / `move v1,s0` pair and the registers that follow from
   it). Needed: a zero that cse cannot see at that point. Tried: a loop that runs once (`for (i = 0; i < 1; i++)`
   with `medium = i`), an inline helper returning the variable, a dead conditional in front (the trick that
   matched IopHeap_PrintFree), none keeps the copy. */
/* Cleanup pass 3: read the copy as `medium = zero` done BEFORE the split-screen test (`move v1,s0` sits in the
   delay slot of that branch, so it is in the first block, behind the call), with `medium` set again in the two
   water arms. A zero whose address goes to an inline reader (`medium = Get(&zero)`, an ADDRESSOF that is purged
   after the first cse) is still folded (cse does not treat such memory as clobbered by the call). The zero has
   to reach register allocation as a register copy, so whatever hides it must survive cse, gcse, cse2 AND
   combine: not found. */
#if 0
void StgDepthTint_Draw(void) {
    Mtx44 m;
    f32 level;
    View *view = gBtlCamView;
    StgDepthTint *tint;
    s32 medium = 0;
    s32 idx;

    if (!Battle_IsSplitScreen()) {
        if (BtlStage_GetWaterLevel(&level)) {
            Mtx_InverseRT(&m, &view->world2view2);
            medium = 0;
            if (level < m.m[3][1]) {
                medium = 1;
            }
        } else {
            medium = 0;
        }
    }
    tint = &gStgDepthTint[medium];
    if (tint->enabled != 0) {
        Gfx_AddDefaultEnv();
        Dma_AddData(tint->tex.upload, 0x10);
        Dma_AddTexFlush();
        Dma_AddZbuf(0xE0, 1);
        if (tint->additive != 0) {
            GfxPost_DrawDepthClut(0, GFXPOST_DEPTH_TBP, tint->tex.cbp, 0x48);
        } else {
            GfxPost_DrawDepthClut(0, GFXPOST_DEPTH_TBP, tint->tex.cbp, 0x44);
        }
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/sys/gfxm_a", StgDepthTint_Draw);

/* Takes both tints' parameters from the stage file (defaults: off) and rebuilds the CLUTs. */
void StgDepthTint_LoadParams(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        StgDepthTint *tint = &gStgDepthTint[i];
        StgDepthTintParam *param = BtlStage_GetList40(i);

        if (param == NULL) {
            tint->additive = 1;
            tint->enabled = 0;
            tint->clearLast = 0;
            tint->r = 0.5f;
            tint->g = 0.5f;
            tint->b = 0.5f;
            tint->a = 1.0f;
            tint->curve.key[0].pos.x = 0.0f;
            tint->curve.key[0].pos.y = 0.0f;
            tint->curve.key[0].pos.z = 0.0f;
            tint->curve.key[0].pos.w = 1.0f;
            tint->curve.key[1].pos.x = 128.0f;
            tint->curve.key[1].pos.y = 0.0f;
            tint->curve.key[1].pos.z = 128.0f;
            tint->curve.key[1].pos.w = 1.0f;
            tint->curve.key[2].pos.x = 255.0f;
            tint->curve.key[2].pos.y = 0.0f;
            tint->curve.key[2].pos.z = 255.0f;
            tint->curve.key[2].pos.w = 1.0f;
        } else {
            tint->enabled = param->flags & 1;
            tint->additive = (param->flags >> 1) & 1;
            tint->clearLast = (param->flags >> 2) & 1;
            tint->r = param->r / 255.0f;
            tint->g = param->g / 255.0f;
            tint->b = param->b / 255.0f;
            tint->a = param->a / 255.0f;
            tint->curve.key[0].pos.x = param->key[0][0];
            tint->curve.key[0].pos.y = 0.0f;
            tint->curve.key[0].pos.z = param->key[0][1];
            tint->curve.key[0].pos.w = 1.0f;
            tint->curve.key[1].pos.x = param->key[1][0];
            tint->curve.key[1].pos.y = 0.0f;
            tint->curve.key[1].pos.z = param->key[1][1];
            tint->curve.key[1].pos.w = 1.0f;
            tint->curve.key[2].pos.x = param->key[2][0];
            tint->curve.key[2].pos.y = 0.0f;
            tint->curve.key[2].pos.z = param->key[2][1];
            tint->curve.key[2].pos.w = 1.0f;
        }
        StgDepthTint_BuildClut(tint->tex.clut, tint);
    }
}

/* CLUT of the object glow: white, alpha from the table. */
void ObjGlow_BuildClut(void) {
    u8 *clut = gObjGlow->cur->clut;
    u8 *alpha = gObjGlow->table.alpha;
    s32 i;

    for (i = 0; i < 0x100; i++) {
        u8 *e = clut + GFXPOST_CLUT_INDEX(i) * 4;

        e[0] = 0xFF;
        e[1] = 0xFF;
        e[2] = 0xFF;
        e[3] = alpha[i];
    }
}

/* Creates the object glow. */
void ObjGlow_Init(void) {
    gObjGlow = Heap_Alloc(sizeof(ObjGlow), 0x20, 0, 2);
    memset(gObjGlow, 0, sizeof(ObjGlow));
    GfxClut_InitPacket(&gObjGlow->tex[0], 0x3E90);
    GfxClut_InitPacket(&gObjGlow->tex[1], 0x3E90);
    gObjGlow->cur = &gObjGlow->tex[gObjGlow->index];
    gObjGlow->index ^= 1;
    gObjGlow->cur = &gObjGlow->tex[gObjGlow->index];
    ObjGlow_BuildClut();
}

/* Frees the object glow. */
void ObjGlow_Term(void) {
    Heap_Free(gObjGlow);
    gObjGlow = NULL;
}

/* Takes this frame's alpha table. */
void ObjGlow_SetAlphaTable(ObjGlowTable *table) {
    gObjGlow->table = *table;
}

/* The object glow: the table's alpha into the frame by id byte, then the glow pass. */
void ObjGlow_Draw(void) {
    ObjGlow_BuildClut();
    Dma_AddData(gObjGlow->cur->upload, 0x10);
    Dma_AddTexFlush();
    Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0xFFFFFF);
    GfxPost_DrawDepthClut(0, GFXPOST_DEPTH_TBP, gObjGlow->cur->cbp, 0x44);
    Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0);
    GfxPost_DrawGlow(10, 0x80808080, 0x80808080);
}

/* Nothing; first call of StgFx_Init. */
void GfxPost_InitNop(void) {
}

/* Nothing; last call of StgFx_Term. */
void GfxPost_TermNop(void) {
}

/* Nothing. */
void GfxPost_Nop106C70(void) {
}

/* Returns 0. */
s32 GfxPost_Stub106C78(void) {
    return 0;
}

/* Sets a quad's colour. */
void GfxPostQuad_SetColor(GfxPostQuad *q, s32 r, s32 g, s32 b, s32 a) {
    q->r = r;
    q->g = g;
    q->b = b;
    q->a = a;
}

/* Works out a quad of w x h pixels at (dx, dy) from the base, textured from (u0, v0) to (u1, v1); `half` adds
   half a texel. Everything goes to 12.4 fixed point. */
void GfxPostQuad_Set(GfxPostQuad *q, s32 baseX, s32 baseY, s16 u0, s16 v0, s16 u1, s16 v1, s32 dx, s32 dy, s32 w, s32 h,
                     s16 half) {
    q->u0 = u0 << 4;
    q->v0 = v0 << 4;
    q->u1 = u1 << 4;
    q->v1 = v1 << 4;
    if (half != 0) {
        q->u0 = (u0 << 4) + 8;
        q->u1 = (u1 << 4) + 8;
        q->v0 = (v0 << 4) + 8;
        q->v1 = (v1 << 4) + 8;
    }
    q->dx = dx << 4;
    q->dy = dy << 4;
    q->w = w << 4;
    q->h = h << 4;
    q->x0 = (baseX << 4) + (dx << 4);
    q->y0 = (baseY << 4) + (dy << 4);
    q->x1 = (baseX << 4) + (dx << 4) + (w << 4);
    q->y1 = (baseY << 4) + (dy << 4) + (h << 4);
    q->r = 0x80;
    q->g = 0x80;
    q->b = 0x80;
    q->a = 0x80;
}
