#include "common.h"
/*
 * Screen-space passes of the battle (0x106D60..0x109938): full-screen strips that read the depth buffer through a
 * CLUT, the alpha-key overlay of the battle objects, the lens discs, the underwater wobble and the depth fog.
 * The strips rely on GfxPostQuad_Set of the neighbouring file (gfxm_a.c), so the two are probably one source.
 * Proper names: gfx_post_b.c (0x106D60..0x107730), gfx_lens.c (..0x1085E0), gfx_alpha_key.c (..0x1087A8),
 * gfx_water.c (..0x1094A8) and gfx_depth_fog.c (..0x109938) if they are split later; nothing forces a split.
 */
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/heap.h"
#include "sys/gfxm_b.h"

extern void *memset(void *dst, s32 c, u32 n);
extern void GfxPostQuad_Set(GfxQuad *out, s32 baseX, s32 baseY, s16 u0, s16 v0, s16 u1, s16 v1, s32 dx, s32 dy, s32 w, s32 h,
                          s16 half);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 k);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Div(Vec4 *dst, Vec4 *src, f32 d);
extern void Vec4_ToFixed4(IVec4 *dst, Vec4 *src);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Mtx_Transpose(Mtx44 *dst, Mtx44 *src);
extern void Vu0Screen_StoreMtx(Mtx44 *dst);
/* gfxm_b_b.c; declared with this file's views of the structures */
void TexFile_UploadPacked(GfxTexFile *file, s32 tbp, s32 cbp);
s32 Tex_Log2Size(s32 n);
void GfxClut_InitPacket(GfxClutWork *work, u16 cbp);
extern GfxWaterStage *BtlStage_GetList90(void);
void GfxWater_SetColor(s32 view, u8 r, u8 g, u8 b, s32 a);
extern s32 BtlStage_GetWaterLevel(f32 *level);
extern s32 Battle_IsSplitScreen(void);
extern s32 DemoCam_IsActive(void);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern f32 sqrtf(f32 x);
extern f32 cosf(f32 x);

/* The part of the battle work this file reads. */
typedef struct GfxBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 paused, 0x2000 loading */
} GfxBattleWork;
extern GfxBattleWork *Battle_GetWork(void);

/* Stack vectors as 16-byte aligned arrays (Sony sceVu0FVECTOR style): the compiler copies them with 64-bit loads. */
typedef f32 GfxFVec[4] __attribute__((aligned(16)));
typedef s32 GfxIVec[4] __attribute__((aligned(16)));
typedef f32 GfxFMtx[4][4] __attribute__((aligned(16)));

void GfxLens_Draw(s32 split, s32 side);
void GfxLens_DrawAll(GfxLensView *view, s32 split, s32 side);
u64 *GfxLens_PutCapture(u64 *p, s32 split, s32 side);
void GfxWater_DrawView(s32 split, s32 view);
void GfxPost_ShiftHighWord(s32 fbp, s32 tbp, s32 h);

#define GFX_RGBAQ_NEUTRAL 0x3F80000080808080 /* grey 0x80, alpha 0x80, Q = 1.0f */

/* Draws the spare byte of the depth buffer through a CLUT over the whole screen (16 strips of 32 pixels). */
void GfxPost_DrawDepthClut(s32 mode, s32 tbp, s32 cbp, u64 alpha) {
    GfxQuad q;
    u64 *p;
    u64 *tag;
    u32 z;
    s32 i;
    s32 u1;
    s32 w = 32;
    s32 tw = 32;
    s32 u = 0;
    s32 x = 0;
    s32 h = 448;
    s32 n = 512 / w;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(8, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    z = 0xFFA8;
    if (mode != 0) {
        p[0] = 0x70000;
    } else {
        p[0] = 0x30000;
        z = 0;
    }
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = alpha;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = 0x8000000000;
    p[1] = GS_TEXA;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEX1_1;
    p += 2;
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
        u1 = u + 32;
        GfxPostQuad_Set(&q, 0x700, 0x720, u, 0, u1, h, x, 0, tw, h, 1);
        p[0] = (s64)q.u0 | ((s64)q.v0 << 16);
        p[1] = (s64)q.x0 | ((s64)q.y0 << 16) | ((u64)z << 32);
        p += 2;
        p[0] = (s64)q.u1 | ((s64)q.v1 << 16);
        p[1] = (s64)q.x1 | ((s64)q.y1 << 16) | ((u64)z << 32);
        p += 2;
        u = u1;
        x += 32;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 4);
    tag[1] = 0x5353;
    Dma_EndDirect(p);
}

/* Copies the alpha of the colour buffer being drawn into the spare top byte of the 24-bit depth buffer. */
void GfxPost_CopyAlphaToDepth(void) {
    GfxQuad q;
    u64 *p;
    u64 *tag;
    s32 i;
    s32 u1;
    s32 w = 32;
    s32 tw = 32;
    s32 u = 0;
    s32 x = 0;
    s32 h = 448;
    s32 n = 512 / w;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(9, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_FRAME(GFX_ZBP, GFX_FBW, 0, 0xFFFFFF);
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x8000000044;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0xE64020E00 : 0xE64020000;
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
        u1 = u + 32;
        GfxPostQuad_Set(&q, 0x700, 0x720, u, 0, u1, h, x, 0, tw, h, 1);
        p[0] = (s64)q.u0 | ((s64)q.v0 << 16);
        p[1] = (s64)q.x0 | ((s64)q.y0 << 16);
        p += 2;
        p[0] = (s64)q.u1 | ((s64)q.v1 << 16);
        p[1] = (s64)q.x1 | ((s64)q.y1 << 16);
        p += 2;
        u = u1;
        x += 32;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 4);
    tag[1] = 0x5353;
    p[0] = GIF_TAG(1, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = GS_FRAME_1;
    p += 2;
    Dma_EndDirect(p);
}

/* Draws one textured sprite: `tex0` at (x, y) with size (w, h); the texel box is the whole texture, or 448 rows of it. */
void GfxPost_DrawTexRect(u64 tex0, s32 x, s32 y, s32 w, s32 h, s32 fullHeight) {
    GfxQuad q;
    u64 *p;
    s32 tw;
    s32 th;
    GfxLens *f;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(8, 0, 1);
    p[1] = GIF_REG_AD;
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
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = tex0;
    p[1] = GS_TEX0_1;
    p += 2;
    tw = 1 << ((s32)(tex0 >> 26) & 0xF);
    if (fullHeight != 0) {
        th = 448;
    } else {
        th = 1 << ((s32)(tex0 >> 30) & 0xF);
    }
    GfxPostQuad_Set(&q, 0x700, 0x720, 0, 0, tw, th, x, y, w, h, 0);
    p[0] = GIF_TAG_EX(2, 1, GIF_FLG_REGLIST, 2);
    p[1] = 0x53;
    p += 2;
    p[0] = (s64)q.u0 | ((s64)q.v0 << 16);
    p[1] = (s64)q.x0 | ((s64)q.y0 << 16);
    p += 2;
    p[0] = (s64)q.u1 | ((s64)q.v1 << 16);
    p[1] = (s64)q.x1 | ((s64)q.y1 << 16);
    p += 2;
    p[0] = GIF_TAG(1, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x50000;
    p[1] = GS_TEST_1;
    p += 2;
    Dma_EndDirect(p);
}

/* Draws a blended untextured rectangle over the whole screen (16 strips), depth writes off; colour from four floats.
   `x` has to be a second induction variable (not `i * 32`): the compiler then builds the strips' start values from
   its initial value at run time (`move v1,zero / sll v1,v1,4`) instead of folding them. */
void GfxPost_DrawTintRect(u64 alpha, Vec4 *color) {
    u64 *p;
    u64 *tag;
    s32 i;
    s32 x = 0;

    Gfx_AddDefaultEnv();
    Dma_AddZbuf(GFX_ZBP, 1);
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(5, 0, 1);
    p[1] = GIF_REG_AD;
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
    p[0] = (u64)(u8)(u32)color->x | ((u64)(u8)(u32)color->y << 8) | ((u64)(u8)(u32)color->z << 16) |
           ((u64)(u8)(u32)color->w << 24) | 0x3F80000000000000;
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    for (i = 0; i < 16; i++) {
        p[0] = (s64)((x << 4) + GFX_OFX) | 0x72000000;
        p[1] = (s64)(((x + 32) << 4) + GFX_OFX) | 0x8E000000;
        p += 2;
        x += 32;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 2);
    tag[1] = 0x55;
    Dma_EndDirect(p);
    Dma_AddZbuf(GFX_ZBP, 0);
}

/* Clears the flare table and its trailing words. */
void GfxLens_Init(void) {
    memset(&gGfxLens, 0, sizeof(GfxLensWork));
}

/* Empty. */
void GfxLens_Stub107758(void) {
}

/* Clears the eight flare slots. */
void GfxLens_Clear(void) {
    memset(&gGfxLens, 0, sizeof(gGfxLens.slot));
}

/* Starts a flare at a world position in the first free slot; returns the slot, or -1 when all eight are busy. */
s32 GfxLens_Start(Vec4 *pos, s32 life, u32 rgba, s32 blend, f32 size) {
    s32 i;
    GfxLens *f;

    for (i = 0, f = gGfxLens.slot; i < 8; i++, f++) {
        if (f->life == 0) {
            f->lifeMax = life;
            f->life = life;
            f->rgba = rgba;
            f->blend = blend;
            f->size = size;
            Vec4_Copy(&f->pos, pos);
            return i;
        }
    }
    return -1;
}

/* Frees a flare slot. */
void GfxLens_Stop(u32 slot) {
    if (slot < 8) {
        gGfxLens.slot[slot].life = 0;
    }
}

/* Sets a slot's hold flag: while set, the flare does not age. */
void GfxLens_SetHold(u32 slot, s32 hold) {
    if (slot < 8) {
        gGfxLens.slot[slot].hold = hold;
    }
}

/* Moves a flare. */
void GfxLens_SetPos(u32 slot, Vec4 *pos) {
    if (slot < 8) {
        Vec4_Copy(&gGfxLens.slot[slot].pos, pos);
    }
}

/* Draws one lens from its seven projected points: a black screen wipe in the chosen blend, the four mirrored
   quarters of the lens texture, then the captured screen stretched over the lens. */
void GfxLens_DrawOne(IVec4 *pt, s32 split, s32 side, u32 rgba, s32 blend) {
    u64 *p;
    s32 i;
    f32 s;
    f32 t;

    Gfx_AddDefaultEnv();
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(1, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    switch (blend) {
        case 0:
        default:
            p[0] = 0x54;
            p[1] = GS_ALPHA_1;
            p += 2;
            break;
        case 1:
            p[0] = 0x58;
            p[1] = GS_ALPHA_1;
            p += 2;
            break;
        case 2:
            p[0] = 0x52;
            p[1] = GS_ALPHA_1;
            p += 2;
            break;
    }
    p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 4);
    p[1] = 0x5510;
    p += 2;
    p[0] = 0x246;
    p[1] = 0x3F80000000000000;
    p += 2;
    p[0] = 0x72007000;
    p[1] = 0x8E009000;
    p += 2;
    p[0] = GIF_TAG_EX(4, 1, GIF_FLG_REGLIST, 6);
    p[1] = 0x535310;
    p += 2;
    for (i = 0; i < 4; i++) {
        p[0] = 0x356;
        p[1] = 0x3F80000020808080;
        p += 2;
        p[0] = 0;
        p[1] = (s64)pt[i].x | ((s64)pt[i].y << 16);
        p += 2;
        p[0] = 0x0FF00FF0;
        p[1] = (s64)pt[4].x | ((s64)pt[4].y << 16);
        p += 2;
    }
    p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 6);
    p[1] = 0x525210;
    p += 2;
    p[0] = 0x56;
    p[1] = (u64)rgba | 0x3F80000000000000;
    p += 2;
    s = (f32)(pt[5].x - GFX_OFX) * (1.0f / 512.0f) * 0.0625f;
    t = (f32)(pt[5].y - GFX_OFY) * (1.0f / 448.0f) * 0.0625f;
    if (split != 0) {
        s += s;
        if (side != 0) {
            s -= 1.0f;
        }
    }
    p[0] = (u64) * (u32 *)&s | ((u64) * (u32 *)&t << 32);
    p[1] = (s64)pt[0].x | ((s64)pt[0].y << 16);
    p += 2;
    s = (f32)(pt[6].x - GFX_OFX) * (1.0f / 512.0f) * 0.0625f;
    t = (f32)(pt[6].y - GFX_OFY) * (1.0f / 448.0f) * 0.0625f;
    if (split != 0) {
        s += s;
        if (side != 0) {
            s -= 1.0f;
        }
    }
    p[0] = (u64) * (u32 *)&s | ((u64) * (u32 *)&t << 32);
    p[1] = (s64)pt[3].x | ((s64)pt[3].y << 16);
    p += 2;
    Dma_EndDirect(p);
}

/* True when a lens is in front of the camera and its box (points 0 and 3) touches the 512x448 screen. */
static inline s32 GfxLens_IsOnScreen(GfxIVec *scr) {
    s32 flags;

    if (scr[4][2] <= 0) {
        return 0;
    }
    flags = (scr[0][0] >= 0x7000 && scr[0][0] <= 0x9000) | (scr[3][0] >= 0x7000 && scr[3][0] <= 0x9000);
    if (scr[0][0] <= 0x7000 && scr[3][0] >= 0x9000) {
        flags |= 1;
    }
    if (scr[3][0] <= 0x7000) {
        flags |= scr[0][0] >= 0x9000;
    }
    if (scr[0][1] >= 0x7200 && scr[0][1] <= 0x8E00) {
        flags |= 2;
    }
    if (scr[3][1] >= 0x7200 && scr[3][1] <= 0x8E00) {
        flags |= 2;
    }
    if (scr[0][1] <= 0x7200 && scr[3][1] >= 0x8E00) {
        flags |= 2;
    }
    if (scr[3][1] <= 0x7200 && scr[0][1] >= 0x8E00) {
        flags |= 2;
    }
    return flags == 3;
}

/* Ages every live lens, projects its seven points with the camera and draws those that touch the screen.
   Its `offs` initialiser is the 0x70-byte table at 0x2EB6A0 (.rodata).
   Matching notes: the first inner loop has an explicit BYTE OFFSET as a second induction variable (`o += 0x10`,
   added to `offs` and to the address of pos[0].w, itself a variable set between `i = 0` and `v = pos`) next to
   the walking pointer `v`. With `offs[i]` / `pos[i].w` the loop pass builds the address of the w store from the
   `i * 16` that gcse leaves as a copy with a multiplication note (cost 12) and gives it a pointer of its own;
   with a real variable the two addresses are plain sums of benefit 0 and stay `base + o`. The form is what
   matches; whether the original spelled it this way (a macro, or three cursors) is not known. */
void GfxLens_DrawAll(GfxLensView *view, s32 split, s32 side) {
    GfxFMtx rot;
    GfxFVec offs[7] = {
        { -1.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, 0.0f, 0.0f }, { -1.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },   { -1.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f },
    };
    GfxFVec pos[7];
    GfxIVec scr[7];
    GfxFVec tmp;
    GfxFMtx w2s;
    s32 i;
    s32 j;
    GfxLens *f;
    f32 t;
    f32 half;
    GfxFVec *v;
    s32 o;
    f32 *w;

    Vu0Screen_StoreMtx((Mtx44 *)w2s);
    Mtx_Transpose((Mtx44 *)rot, &view->mtx40);
    for (j = 0; j < 8; j++) {
        f = &gGfxLens.slot[j];
        if (f->life == 0) {
            continue;
        }
        if (f->hold == 0) {
            f->life--;
        }
        t = (f32)f->life / (f32)f->lifeMax * f->size;
        half = f->size * 0.5f;
        for (i = 0, w = &pos[0][3], v = pos, o = 0; i < 7; i++, o += 0x10) {
            if (i < 5) {
                Vec4_Scale((Vec4 *)*v, (Vec4 *)((u8 *)offs + o), t);
            } else {
                Vec4_Scale((Vec4 *)*v, (Vec4 *)((u8 *)offs + o), half + t * 0.5f);
            }
            Mtx_MulVec4((Vec4 *)*v, (Mtx44 *)rot, (Vec4 *)*v);
            Vec4_Add((Vec4 *)*v, &f->pos, (Vec4 *)*v);
            *(f32 *)((u8 *)w + o) = 1.0f;
            v++;
        }
        for (i = 0; i < 7; i++) {
            Mtx_MulVec4((Vec4 *)tmp, (Mtx44 *)w2s, (Vec4 *)pos[i]);
            Vec4_Div((Vec4 *)tmp, (Vec4 *)tmp, tmp[3]);
            Vec4_ToFixed4((IVec4 *)scr[i], (Vec4 *)tmp);
        }
        if (GfxLens_IsOnScreen(scr)) {
            GfxLens_DrawOne((IVec4 *)scr, split, side, f->rgba, f->blend);
        }
    }
}


/* Matching notes (strip loop, from the loop pass's dump): `col` and `x` are two variables; `u` exists only in
   the two later arms; the first strip's right edge is `x + i * 32 + 32` written out (its own induction
   variable); the Y halves are `(0x78 + i) << 8` and `(0x79 + i) << 8` (two expressions that do not share
   `i * 0x100`). Where `p += 2` stands decides the schedule, because each arm is scheduled as a block of its own
   before identical tails are merged: the two `side` arms of the first strip each hold both stores AND their
   `p += 2`; the two later arms share ONE final `p += 2` behind their if / else (which also breaks an
   allocation tie between `split` and the hoisted constant 1); the three scissor arms each end in `p += 2`. */
/* Writes the packet that copies the screen (or one half of a split screen) into the 256x256 buffer at page 0x150,
   as 16 strips, and then restores the frame, offset and scissor. Returns the end of the packet. */
u64 *GfxLens_PutCapture(u64 *p, s32 split, s32 side) {
    s32 i;
    s32 u;
    s32 sh;
    s32 x;
    s32 col;

    p[0] = GIF_TAG(13, 1, 1);
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
    p[0] = GS_SET_FRAME(0x150, 4, 0, 0);
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7800, 0x7800);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 255, 0, 255);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0x6FC007FC00A;
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x20000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x46;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = 0x3F80000080000000;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GIF_TAG(6, 1, 1);
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
    p[0] = 0;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x116;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    col = 0;
    sh = 4;
    if (split != 0) {
        col = side == 1 ? 16 : 0;
        sh = 3;
    }
    x = col << 5;
    p[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    for (i = 0; i < 16; i++) {
        if (i == 0) {
            if (side == 1) {
                p[0] = (x + 3) << sh;
                p[1] = ((0x78 + i) << 8) | 0x78000000;
                p += 2;
            } else {
                p[0] = x << sh;
                p[1] = ((0x78 + i) << 8) | 0x78000000;
                p += 2;
            }
            p[0] = ((x + i * 32 + 32) << sh) | 0x1C000000;
            p[1] = (u64)((0x79 + i) << 8) | 0x88000000;
            p += 2;
        } else {
            u = x + i * 32;
            if (i == 15) {
                p[0] = u << sh;
                p[1] = ((0x78 + i) << 8) | 0x78000000;
                p += 2;
                p[0] = ((u + 31) << sh) | 0x1C000000;
                p[1] = (u64)((0x79 + i) << 8) | 0x88000000;
            } else {
                p[0] = u << sh;
                p[1] = ((0x78 + i) << 8) | 0x78000000;
                p += 2;
                p[0] = ((u + 32) << sh) | 0x1C000000;
                p[1] = (u64)((0x79 + i) << 8) | 0x88000000;
            }
            p += 2;
        }
    }
    p[0] = GIF_TAG(6, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(GFX_OFX, GFX_OFY);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    if (split != 0) {
        if (side == 0) {
            p[0] = GS_SET_SCISSOR(0, 256, 0, 447);
            p[1] = GS_SCISSOR_1;
            p += 2;
        } else {
            p[0] = GS_SET_SCISSOR(256, 511, 0, 447);
            p[1] = GS_SCISSOR_1;
            p += 2;
        }
    } else {
        p[0] = GS_SET_SCISSOR(0, 511, 0, 447);
        p[1] = GS_SCISSOR_1;
        p += 2;
    }
    p[0] = 0x70000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_CLAMP_1;
    p += 2;
    return p;
}


/* Draws the lenses of one view: uploads the lens texture, captures the screen and draws every live lens. */
void GfxLens_Draw(s32 split, s32 side) {
    u64 *p;
    s32 i;
    GfxTexEntry *ent;
    s32 tw;
    s32 th;
    GfxLens *f;

    f = gGfxLens.slot;
    for (i = 0; i < 8; i++) {
        if (f[i].life != 0) {
            break;
        }
    }
    if (i == 8) {
        return;
    }
    TexFile_UploadPacked(gGfxLens.tex, 0x2F00, 0x2E00);
    Dma_AddTexFlush();
    ent = gGfxLens.tex->entries;
    Gfx_AddDefaultEnv();
    p = Dma_BeginDirect();
    p = GfxLens_PutCapture(p, split, side);
    p[0] = GIF_TAG(10, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x54;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x4A;
    p[1] = GS_ALPHA_2;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_2;
    p += 2;
    p[0] = 0x2000C;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x2000C;
    p[1] = GS_TEST_2;
    p += 2;
    p[0] = 5;
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 5;
    p[1] = GS_CLAMP_2;
    p += 2;
    tw = Tex_Log2Size(0x100);
    th = Tex_Log2Size(0x100);
    p[0] = ((s64)tw << 26) | ((s64)th << 30) | 0x12A00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = ent->tex0 | 0x5C00400002F00;
    p[1] = GS_TEX0_1 + 1;
    p += 2;
    Dma_EndDirect(p);
    GfxLens_DrawAll(gBtlCam->view, split, side);
}

/* Draws the lenses for a full-screen view. */
void GfxLens_DrawFull(void) {
    GfxLens_Draw(0, 0);
}

/* Fills a 256-entry CLUT (GS order) that is transparent except for entries 0xF4..0xFE, which get eleven fixed
   colours with alpha 0x30. */
void GfxAlphaKey_BuildClut(u8 *clut) {
    u8 rgb[33] = {
        0x3F, 0x3F, 0xFF, /* values checked against 0x2EB710 when the file was linked */
        0x80, 0x00, 0xFF,
        0xFF, 0xFF, 0x20,
        0xFF, 0x00, 0x00,
        0x00, 0xFF, 0x00,
        0x54, 0xFD, 0xFF,
        0xFF, 0x00, 0xFF,
        0x40, 0x00, 0xFF,
        0xFF, 0xFF, 0x20,
        0xFF, 0xFF, 0x2A,
        0xFF, 0xFF, 0x34,
    };
    s32 i;
    s32 n;

    clut[0] = 0;
    clut[1] = 0;
    clut[2] = 0;
    clut[3] = 0;
    for (i = 0; i < 256; i++) {
        n = ((i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1)) * 4;
        if (i >= 0xF4 && i != 0xFF) {
            clut[n + 0] = rgb[(i - 0xF4) * 3 + 0];
            clut[n + 1] = rgb[(i - 0xF4) * 3 + 1];
            clut[n + 2] = rgb[(i - 0xF4) * 3 + 2];
            clut[n + 3] = 0x30;
        } else {
            clut[n + 0] = 0;
            clut[n + 1] = 0;
            clut[n + 2] = 0;
            clut[n + 3] = 0;
        }
    }
}

/* Allocates the depth-tint work, builds its CLUT upload packet and fills the CLUT. */
void GfxAlphaKey_Init(void) {
    gGfxAlphaKey = Heap_Alloc(sizeof(GfxClutWork), 0x20, 0, HEAP_ANY);
    memset(gGfxAlphaKey, 0, sizeof(GfxClutWork));
    GfxClut_InitPacket(gGfxAlphaKey, 0x3E94);
    GfxAlphaKey_BuildClut(gGfxAlphaKey->clut);
}

/* Frees the depth-tint work. */
void GfxAlphaKey_Term(void) {
    Heap_Free(gGfxAlphaKey);
    gGfxAlphaKey = NULL;
}

#ifdef PORT
/* A marker in the display list for the PC renderer (port/src/gs_marker.c). */
#define PORT_FX_ALPHA_KEY 2
extern void Port_GsMarker(s32 effect);
#endif

/* Uploads the CLUT and draws the depth buffer's spare byte through it over the screen. */
void GfxAlphaKey_Draw(void) {
#ifdef PORT
    Port_GsMarker(PORT_FX_ALPHA_KEY); /* the GPU renderer draws the tint natively here and drops the pass below */
#endif
    Dma_AddData(gGfxAlphaKey->ref, 0x10);
    Dma_AddTexFlush();
    Dma_AddZbuf(GFX_ZBP, 1);
    GfxPost_DrawDepthClut(0, GFX_ZBP * 32, gGfxAlphaKey->cbp, 0x44);
    Dma_AddZbuf(GFX_ZBP, 0);
}

/* Gives both water layers their default scroll speeds and an opaque grey. */
void GfxWater_Reset(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gGfxWater[i].speedU = 0.06f;
        gGfxWater[i].speedV = 0.04f;
        GfxWater_SetColor(i, 0x5C, 0x5C, 0x80, 0x80);
    }
}

/* Allocates the two water layers. */
void GfxWater_Init(void) {
    gGfxWater = Heap_Alloc(sizeof(GfxWaterLayer) * 2, 0x20, 0, HEAP_ANY);
    memset(gGfxWater, 0, sizeof(GfxWaterLayer) * 2);
    GfxWater_Reset();
}

/* Frees the water layers. */
void GfxWater_Term(void) {
    if (gGfxWater != NULL) {
        Heap_Free(gGfxWater);
        gGfxWater = NULL;
    }
}

/* Takes the colour of both layers from the stage's water record when it has one. */
void GfxWater_LoadStageColor(void) {
    GfxWaterStage *st = BtlStage_GetList90();

    if (st != NULL) {
        if (st->flags & 1) {
            GfxWater_SetColor(0, st->r, st->g, st->b, st->a);
            GfxWater_SetColor(1, st->r, st->g, st->b, st->a);
        }
    }
}

ASM_STUB_BEGIN(); /* compiled so that GfxWater_Draw sees the definition (it decides a branch-likely there); the assembler skips it */
/* NOT MATCHING: 96 of 639 instructions differ (was about 390 aligned), and the instruction count is now the
   original's. Same operations and the same six .lit4 constants (0x2FC2A0..0x2FC2B4: pi, 2 pi, 2 pi, 0.8, 0.2,
   44.8; bits compared) plus the .sdata word 0x2FE8D0 = 224.
   Found in the third cleanup (each of these removed a block of differences):
   - `h = 0xE0` and `srcH = 0x1C0` are VARIABLES set at the top (as in StgHaze_Draw / StgBlur_Draw). That is
     why the original loads 0xE00 into a register for the gv clamp (`h << 4`), keeps `(s64)h << 34` apart
     from the `| 0xA` of the second CLAMP word, multiplies `srcH * row` with a real `mult`, and converts 224
     to float from a constant-pool word in .sdata (`fh = h`: the old attempt read D_002FE8D0 as a global).
   - the copy loop is the twin of the first loop of StgPanBlur_DrawView (same statements, other constants):
     `step = w / n` unshifted and `(i * step) << 4` in the loop.
   - there is no `y` variable: `cy = h * row / 5`. The slot at sp+0x234 that looked like one is the loop
     pass's strength-reduced `h * row` (it sits BEHIND the `view * 4` that gcse creates, so it was made later
     than any declared variable).
   - the three row loops share one counter (the original keeps it in s3 throughout).
   What still differs:
   1. float registers of the inner body: the original has ph in f20, d in f21, dx in f22; this has d, dx, ph
      (allocation priority: ph needs about a third more references). In the original the result of the
      SECOND sqrtf is computed straight into f20 (`sqrt.s f20,f12`, a callee-saved register, so that pseudo
      is live across a call) and copied to d; the first goes through f0. `ph = sqrtf(..); d = ph;` is folded
      back by cse.
   2. the 9-register packet header: the original loads x0 early and builds the ZBUF constant late; the first
      CLAMP word is `x0 << 4 | 0x1C0 << 34 | x1 << 14 | 0xA` with the two constants NOT merged, which
      `(s64)srcH << 34` reproduces locally but then the function's first third changes (the phase update
      stops spilling `view * 4`), so it is not in this attempt.
   3. in the last loop the original loads gv before gu (`gv << 16 | gu` makes other things worse).
   Also tried: `rows = 6` as a variable with `rows - 1` as the divisor (worse everywhere, although the
   original divides by a 5 held in a register without a zero check). */
/* Draws the underwater wobble for one view: advances the view's two phases, builds a 6 x N grid of texel positions
   pushed around two wave centres, copies the view into the half-height buffer at page 0x150 and draws it back over
   the screen as five triangle strips tinted with the view's colour. */
void GfxWater_DrawView(s32 split, s32 view) {
    s32 gu[6][10];
    s32 gv[6][10];
    s32 srcW;
    s32 w;
    s32 x0;
    s32 x1;
    s32 cols;
    s32 row;
    s32 col;
    s32 cx;
    s32 cy;
    s32 i;
    s32 n;
    s32 step;
    s32 sx;
    u32 rgba;
    u64 *p;
    f32 fw;
    f32 fx;
    f32 fy;
    f32 fh;
    f32 dx;
    f32 dy;
    f32 d;
    f32 ph;
    s32 h = 0xE0;
    s32 srcH = 0x1C0;

    if (Battle_GetWork()->flags & 0x2000) {
        return;
    }
    if (!(BtlStage_GetList90()->flags & 1)) {
        return;
    }
    if (!(Battle_GetWork()->flags & 0x100)) {
        gGfxWater[view].phaseU += gGfxWater[view].speedU;
        if (gGfxWater[view].phaseU > 3.14159265f) {
            gGfxWater[view].phaseU -= 6.2831853f;
        }
        gGfxWater[view].phaseV += gGfxWater[view].speedV;
        if (gGfxWater[view].phaseV > 3.14159265f) {
            gGfxWater[view].phaseV -= 6.2831853f;
        }
    }
    if (split != 0) {
        srcW = 0x100;
        w = 0x100;
        cols = 5;
        if (view == 0) {
            x0 = 0;
            x1 = 0x100;
        } else {
            x0 = 0x100;
            x1 = 0x200;
        }
    } else {
        srcW = 0x200;
        w = 0x200;
        x0 = 0;
        x1 = 0x200;
        cols = 10;
    }
    for (row = 0; row < 6; row++) {
        for (col = 0; col < cols; col++) {
            cx = col * w / (cols - 1);
            cy = h * row / 5;
            gu[row][col] = cx << 4;
            gv[row][col] = cy << 4;
            if (row != 0 && row != 9) {
                fw = w;
                fx = cx;
                fy = cy;
                fh = h;
                dx = fx - fw * 0.2f;
                dy = fy - 224 * 0.2f;
                d = sqrtf(dx * dx + dy * dy);
                ph = d * 0.2f;
                gu[row][col] += (s32)(cosf(gGfxWater[view].phaseU + ph) * (dx / d) * 3.0f * 16.0f);
                gv[row][col] += (s32)(cosf(gGfxWater[view].phaseU + ph) * (dy / d) * 3.0f * 16.0f);
                dx = fx - fw * 0.8f;
                dy = fy - 0.8f * fh;
                d = sqrtf(dx * dx + dy * dy);
                ph = d * 0.2f;
                gu[row][col] += (s32)(cosf(gGfxWater[view].phaseV + ph) * (dx / d) * 3.0f * 16.0f);
                gv[row][col] += (s32)(cosf(gGfxWater[view].phaseV + ph) * (dy / d) * 3.0f * 16.0f);
            }
            if (gu[row][col] < 0) {
                gu[row][col] = 0;
            }
            if (gu[row][col] > w << 4) {
                gu[row][col] = w << 4;
            }
            if (gv[row][col] < 0) {
                gv[row][col] = 0;
            }
            if (gv[row][col] > h << 4) {
                gv[row][col] = h << 4;
            }
        }
    }
    Gfx_AddDefaultEnv();
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(9, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x264020E00 : 0x264020000;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = GS_SET_FRAME(0x150, 8, 0, 0);
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = ((s64)(w - 1) << 16) | ((u64)0xDF << 48);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = ((s64)x0 << 4) | ((u64)0x1C0 << 34) | ((s64)x1 << 14) | 0xA;
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = GIF_TAG(2, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x116;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    n = (u32)srcW >> 5;
    step = w / n;
    p[0] = GIF_TAG_EX(n, 0, GIF_FLG_REGLIST, 4);
    p[1] = 0x5353;
    p += 2;
    for (row = 0; row < n; row++) {
        p[0] = (x0 + row * 32) << 4;
        p[1] = (GFX_OFX + ((row * step) << 4)) | (0x7200 << 16);
        p += 2;
        p[0] = ((x0 + (row + 1) * 32) << 4) | (0x1C00 << 16);
        p[1] = (s64)(GFX_OFX + (((row + 1) * step) << 4)) | ((u64)0x8000 << 16);
        p += 2;
    }
    p[0] = GIF_TAG(4, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x264022A00;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = (s64)x0 | ((s64)(x1 - 1) << 16) | ((u64)0x1BF << 48);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = ((s64)w << 14) | ((s64)h << 34) | 0xA;
    p[1] = GS_CLAMP_1;
    p += 2;
    for (row = 0; row < 5; row++) {
        p[0] = GIF_TAG_EX(1, 0, GIF_FLG_REGLIST, 2);
        p[1] = 0x10;
        p += 2;
        p[0] = 0x114;
        rgba = gGfxWater[view].rgba;
        p[1] = (u64)(rgba & 0xFF) | ((u64)((rgba >> 8) & 0xFF) << 8) | ((u64)((rgba >> 16) & 0xFF) << 16) |
               ((u64)((rgba >> 24) & 0xFF) << 24);
        p += 2;
        p[0] = GIF_TAG_EX(cols, 0, GIF_FLG_REGLIST, 4);
        p[1] = 0x5353;
        p += 2;
        for (col = 0; col < cols; col++) {
            sx = ((x0 + col * srcW / (cols - 1)) << 4) + GFX_OFX;
            p[0] = (s64)gu[row][col] | ((s64)gv[row][col] << 16);
            p[1] = (s64)sx | ((s64)(((srcH * row / 5) << 4) + GFX_OFY) << 16);
            p += 2;
            p[0] = (s64)gu[row + 1][col] | ((s64)gv[row + 1][col] << 16);
            p[1] = (s64)sx | ((s64)(((srcH * (row + 1) / 5) << 4) + GFX_OFY) << 16);
            p += 2;
        }
    }
    p[0] = GIF_TAG(2, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 0);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 511, 0, 447);
    p[1] = GS_SCISSOR_1;
    p += 2;
    Dma_EndDirect(p);
}

ASM_STUB_END();
INCLUDE_ASM("asm/nonmatchings/sys/gfxm_b", GfxWater_DrawView);

/* Draws the underwater wobble for every view whose camera is below the stage's water level. */
void GfxWater_Draw(void) {
    Mtx44 cam;
    f32 level;
    s32 views = 1;
    s32 split = 0;
    s32 i;
    GfxLensView *v;

    if (gBtlCam == NULL) {
        return;
    }
    if (Battle_IsSplitScreen() && !DemoCam_IsActive() && gBtlCam->view->unk280 != 0) {
        split = 1;
        views = 2;
    }
    if (views >= 2) {
        for (i = 0; i < views; i++) {
            GfxLensCam *c = gBtlCam;

            if (BtlStage_GetWaterLevel(&level)) {
                Mtx_InverseRT(&cam, &c->views[i].mtx40);
                if (level < cam.m[3][1]) {
                    GfxWater_DrawView(split, i);
                }
            }
        }
    } else {
        v = gBtlCamView;
        if (BtlStage_GetWaterLevel(&level)) {
            Mtx_InverseRT(&cam, &v->mtx40);
            if (level < cam.m[3][1]) {
                GfxWater_DrawView(split, 0);
            }
        }
    }
}

/* Sets the tint of a view's underwater wobble. */
void GfxWater_SetColor(s32 view, u8 r, u8 g, u8 b, s32 a) {
    gGfxWater[view].rgba = r | (a << 24) | ((b << 16) | (g << 8));
}

/* Fills a 256-entry CLUT (GS order) with black whose alpha falls from 255 at index 0 to 0 at index 255. */
void GfxDepthFog_BuildClut(u8 *clut) {
    s32 i;
    s32 n;

    for (i = 0; i < 256; i++) {
        n = ((i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1)) * 4;
        clut[n + 0] = 0;
        clut[n + 1] = 0;
        clut[n + 2] = 0;
        clut[n + 3] = 255 - i;
    }
}

/* Moves the upper 16 bits of every pixel of the 32-bit buffer at block `tbp` into the top two bits of page `fbp`
   seen as a 16-bit buffer: 32 sprites of 8 x `h` shifted right by 8 pixels, write mask 0x3FFF.
   `w` and `tw` are two variables (both 8) and `u` is computed in front of the call: that gives the two strip
   counters their own step registers (s7, fp) in the original's order. */
void GfxPost_ShiftHighWord(s32 fbp, s32 tbp, s32 h) {
    GfxQuad q;
    u64 *p;
    u64 *tag;
    s32 i;
    s32 w = 8;
    s32 tw = 8;
    s32 n;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(11, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = (s64)(fbp | 0x02080000) | 0x3FFF00000000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = ((u64)(h - 1) << 48) | 0x1FF0000;
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = 0x8000000000;
    p[1] = GS_TEXA;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = (s64)(tbp | 0x2B220000) | 0xE80000000;
    p[1] = GS_TEX0_1;
    p += 2;
    p[0] = 0x116;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GFX_RGBAQ_NEUTRAL;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(GFX_OFX, GFX_OFY + 8);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    tag = p;
    n = 512 / w;
    p += 2;
    for (i = 0; i < n; i += 2) {
        s32 u = (i + 1) * tw;

        GfxPostQuad_Set(&q, 0x700, 0x720, i * w, 0, i * w + 8, h, u, 0, tw, h, 0);
        p[0] = (s64)q.u0 | ((s64)q.v0 << 16);
        p[1] = (s64)q.x0 | ((s64)q.y0 << 16);
        p += 2;
        p[0] = (s64)q.u1 | ((s64)q.v1 << 16);
        p[1] = (s64)q.x1 | ((s64)q.y1 << 16);
        p += 2;
    }
    tag[0] = GIF_TAG_EX(64, 1, GIF_FLG_REGLIST, 2);
    tag[1] = 0x53;
    p[0] = GIF_TAG(2, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 511, 0, 447);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(GFX_OFX, GFX_OFY);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    Dma_EndDirect(p);
}

/* Builds the depth fog's CLUT work: the upload packet for block `cbp` and the alpha ramp. */
void GfxDepthFog_Init(u16 cbp) {
    memset(&gGfxDepthFog, 0, sizeof(GfxClutWork));
    GfxClut_InitPacket(&gGfxDepthFog, cbp);
    GfxDepthFog_BuildClut(gGfxDepthFog.clut);
}

/* Draws the depth fog: uploads the ramp, moves the depth's high word into the frame's alpha bits, clears the
   colour of the depth page's alias and draws the depth through the ramp with the destination-alpha test on. */
void GfxDepthFog_Draw(void) {
    Dma_AddData(gGfxDepthFog.ref, 0x10);
    Dma_AddTexFlush();
    Dma_AddZbuf(GFX_ZBP, 1);
    GfxPost_ShiftHighWord((gGfx.frame & 1) ? GFX_FBP_A : GFX_FBP_B, GFX_ZBP * 32, 0x380);
    Dma_AddFrame(GFX_ZBP, GFX_FBW, 0xFFFFFF);
    Dma_AddFillRect(0x700, 0x720, 0x200, 0x1C0, 0);
    GfxPost_DrawDepthClut(1, (gGfx.frame & 1) ? GFX_FBP_A * 32 : GFX_FBP_B * 32, gGfxDepthFog.cbp, 0x44);
    Dma_AddFrame((gGfx.frame & 1) ? GFX_FBP_A : GFX_FBP_B, GFX_FBW, 0);
}

/* Empty. */
void GfxDepthFog_Stub109928(void) {
}

/* Empty. */
void GfxDepthFog_Stub109930(void) {
}
