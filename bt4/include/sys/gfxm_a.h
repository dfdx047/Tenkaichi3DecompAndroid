#ifndef SYS_GFXM_A_H
#define SYS_GFXM_A_H

#include "types.h"
#include "sys/dma.h"
#include "sys/math3d.h"
#include "battle/stg_c.h"

/*
 * Full-screen post effects, 0x102F28..0x106D60 (src/sys/gfxm_a.c).
 *
 * Every pass works on the finished scene with GS sprites, 16 strips of 32x448 pixels. Conventions:
 *   - the depth buffer (page 0xE0, texture block 0x1C00) is PSMZ24, so its top byte is spare; the renderer keeps
 *     a per-pixel byte there (GfxPost_CopyAlphaToDepth in the next file) and these passes read the page as an
 *     8-bit texture (PSMT8H) through a 256-entry CLUT;
 *   - GS page 0x150 (texture block 0x2A00) and page 0x170 are work pages;
 *   - XYOFFSET is (0x7000, 0x7200), i.e. screen (0, 0) is GS (1792, 1824).
 */

/* Work pages. */
#define GFXPOST_WORK_FBP 0x150  /* frame page of the work buffer */
#define GFXPOST_WORK_TBP 0x2A00 /* the same memory as a texture */
#define GFXPOST_DEPTH_TBP 0x1C00 /* the depth buffer as a texture */

/* Texture block set up by GfxClut_InitPacket(blk, cbp): a 256-entry CLUT in memory with the packet that uploads it. */
typedef struct GfxPostClut {
    /* 0x000 */ u8 packet[0x490]; /* built by GfxClut_InitPacket; holds the CLUT data */
    /* 0x490 */ u8 *clut;         /* the 256 RGBA entries inside the packet (CSM1 order: see GFXPOST_CLUT_INDEX) */
    /* 0x494 */ u8 upload[0x20];  /* Dma_AddData(upload, 0x10) queues the upload */
    /* 0x4B4 */ u16 cbp;          /* GS block of the CLUT */
    /* 0x4B6 */ u8 pad4B6[0xA];
} GfxPostClut; /* size 0x4C0 */

/* Position of palette entry i in a CSM1 CLUT (bits 3 and 4 swapped). */
#define GFXPOST_CLUT_INDEX(i) (((i) & 0xE7) | (((i) & 8) << 1) | (((i) & 0x10) >> 1))

/* Screen quad worked out by GfxPostQuad_Set. */
typedef struct GfxPostQuad {
    /* 0x00 */ s32 r, g, b, a; /* 0x80 each */
    /* 0x10 */ s32 x0, y0;     /* top-left corner, 12.4 fixed point, base included */
    /* 0x18 */ s32 x1, y1;     /* bottom-right corner */
    /* 0x20 */ s32 u0, v0;     /* texel of the top-left corner, 12.4 (+8 when `half`) */
    /* 0x28 */ s32 u1, v1;
    /* 0x30 */ s32 w, h;       /* size, 12.4 */
    /* 0x38 */ s32 dx, dy;     /* position without the base, 12.4 */
} GfxPostQuad; /* size 0x40 */

/* One shifted copy of the screen drawn by the pan blur. */
typedef struct StgPanBlurLayer {
    /* 0x00 */ Vec4 ofs;   /* x = shift in pixels ((n + 1) * side), y = z = 0, w = 1 */
    /* 0x10 */ Vec4 color; /* 128, 128, 128, alpha (strength * maxAlpha - 32 * n, not below 0) */
    /* 0x20 */ f32 size;   /* extra half size of the drawn quad; never written (0) */
    /* 0x24 */ u8 pad24[0xC];
} StgPanBlurLayer; /* size 0x30 */

/* One view of the pan blur. */
typedef struct StgPanBlurView {
    /* 0x00 */ Mtx44 cur;      /* camera world matrix of this frame, translation Y forced to 0 */
    /* 0x40 */ Mtx44 prev;     /* the same of the previous frame */
    /* 0x80 */ f32 maxAlpha;   /* 101 */
    /* 0x84 */ f32 strength;   /* 0..1 */
    /* 0x88 */ f32 side;       /* -1..1: which way the camera circles (cross product of the two positions) */
    /* 0x8C */ s32 unk8C;
    /* 0x90 */ s32 index;      /* which gBtlCam view */
    /* 0x94 */ s32 x;          /* screen rectangle */
    /* 0x98 */ s32 y;
    /* 0x9C */ s32 w;
    /* 0xA0 */ s32 h;
    /* 0xA4 */ u8 padA4[0xC];
    /* 0xB0 */ StgPanBlurLayer layer[2];
} StgPanBlurView; /* size 0x110 */

typedef struct StgPanBlur {
    /* 0x000 */ GfxPostClut tex;
    /* 0x4C0 */ StgPanBlurView view[2];
    /* 0x6E0 */ s32 split; /* Battle_IsSplitScreen() at creation */
    /* 0x6E4 */ u8 pad6E4[0x1C];
} StgPanBlur; /* size 0x700 */

typedef struct ObjOutline {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ GfxPostClut tex;
} ObjOutline; /* size 0x500 */

/* Sky glare. */
typedef struct StgGlare {
    /* 0x000 */ GfxPostClut tex;
    /* 0x4C0 */ s32 unk4C0;
    /* 0x4C4 */ s32 enabled; /* stage flag bit 0 */
    /* 0x4C8 */ s32 track;   /* stage flag bit 1: the level follows whether the light is on screen */
    /* 0x4CC */ s32 unk4CC;
    /* 0x4D0 */ s32 reset;   /* nonzero: jump to `max` on the next update; nothing sets it */
    /* 0x4D4 */ s32 level;   /* glow colour component, per channel */
    /* 0x4D8 */ s32 unk4D8;
    /* 0x4DC */ s32 unk4DC;  /* = step; written, never read */
    /* 0x4E0 */ s32 state;   /* 0 light off screen, 1 rising, 2 reached max and settling to `hold` */
    /* 0x4E4 */ s32 max;
    /* 0x4E8 */ s32 hold;
    /* 0x4EC */ s32 min;
    /* 0x4F0 */ s32 step;    /* per frame while the light is on screen (off screen the level falls by 1) */
    /* 0x4F4 */ s32 alpha;   /* CLUT alpha of every depth byte but 0xFF (which gets 0x80) */
    /* 0x4F8 */ u8 pad4F8[8];
} StgGlare; /* size 0x500 */

/* Stage section +0x48 (BtlStage_GetList48). */
typedef struct StgGlareParam {
    /* 0x0 */ s32 flags; /* 1 enabled, 2 track */
    /* 0x4 */ s32 step;
    /* 0x8 */ u8 max;
    /* 0x9 */ u8 hold;
    /* 0xA */ u8 min;
    /* 0xB */ u8 alpha;
} StgGlareParam;

/* Depth tint of one medium (0 air, 1 under water). */
typedef struct StgDepthTint {
    /* 0x000 */ GfxPostClut tex;
    /* 0x4C0 */ f32 r; /* 0..1 */
    /* 0x4C4 */ f32 g;
    /* 0x4C8 */ f32 b;
    /* 0x4CC */ f32 a;
    /* 0x4D0 */ StgCurve curve; /* depth byte -> 0..1 */
    /* 0x5A0 */ s32 enabled;    /* stage flag bit 0 */
    /* 0x5A4 */ s32 clearLast;  /* stage flag bit 2: depth byte 0xFF gets a transparent black entry */
    /* 0x5A8 */ s32 additive;   /* stage flag bit 1: colour scaled by the curve, added; else constant colour, alpha from the curve */
    /* 0x5AC */ u8 pad5AC[0x14];
} StgDepthTint; /* size 0x5C0 */

/* Entry of stage section +0x40 (BtlStage_GetList40(i)). */
typedef struct StgDepthTintParam {
    /* 0x0 */ s32 flags; /* 1 enabled, 2 additive, 4 clearLast */
    /* 0x4 */ u8 r, g, b, a;
    /* 0x8 */ u8 key[3][2]; /* x and z of the three curve keys */
} StgDepthTintParam;

/* Alpha per id byte, built by BtlObj_UploadAlphaTable. */
typedef struct ObjGlowTable {
    u8 alpha[0x100];
} ObjGlowTable;

typedef struct ObjGlow {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ GfxPostClut tex[2];
    /* 0x9C0 */ GfxPostClut *cur;
    /* 0x9C4 */ s32 index;
    /* 0x9C8 */ ObjGlowTable table;
    /* 0xAC8 */ u8 padAC8[0x38];
} ObjGlow; /* size 0xB00 */

extern StgPanBlur *gStgPanBlur;
extern ObjOutline *gObjOutline;
extern ObjGlow *gObjGlow;
extern StgGlare gStgGlare;
extern StgDepthTint gStgDepthTint[2];

void StgPanBlur_Init(s32 cbp);
void StgPanBlur_Draw(void);
void StgPanBlur_Term(void);
void StgPanBlur_BuildClut(u8 *clut);
s32 StgPanBlur_UpdateView(StgPanBlurView *view);
void StgPanBlur_CopyScreen(void);
void StgPanBlur_DrawView(StgPanBlurView *view, s32 split);
void StgPanBlur_RestoreEnv(void);
void GfxPost_DrawGlow(s32 passes, u32 color, u32 glow);
void GfxPost_DrawTex16(s32 tbp, u64 alpha);
void GfxPost_AddFrame16(s32 fbp, s32 fbmsk);
void GfxPost_AddScreenFrame(void);
void ObjOutline_BuildClut(u8 *clut);
void GfxPost_DrawDepthClutAt(s32 half, s32 tbp, s32 cbp, u64 alpha, s32 dx, s32 dy);
void GfxPost_DrawTex16Texa(s32 tbp, u64 alpha, u8 ta0);
void GfxPost_FillBlend(u64 alpha, u64 rgbaq);
void GfxPost_ClearWork(s32 fbp, s32 fbmsk);
void GfxPost_AddXyOffset(s32 x, s32 y);
void ObjOutline_Init(void);
void ObjOutline_Term(void);
void ObjOutline_Draw(void);
void StgGlare_BuildClut(u8 *clut);
void StgGlare_Nop(void);
void StgGlare_Update(void);
void StgGlare_Reset(void);
void StgGlare_Init(u16 cbp);
void StgGlare_Term(void);
void StgGlare_Draw(void);
void StgGlare_LoadParams(void);
void StgDepthTint_BuildClut(u8 *clut, StgDepthTint *tint);
void StgDepthTint_Rebuild(void);
void StgDepthTint_Init(u16 cbp);
void StgDepthTint_Term(void);
void StgDepthTint_Draw(void);
void StgDepthTint_LoadParams(void);
void ObjGlow_BuildClut(void);
void ObjGlow_Init(void);
void ObjGlow_Term(void);
void ObjGlow_SetAlphaTable(ObjGlowTable *table);
void ObjGlow_Draw(void);
void GfxPost_InitNop(void);
void GfxPost_TermNop(void);
void GfxPost_Nop106C70(void);
s32 GfxPost_Stub106C78(void);
void GfxPostQuad_SetColor(GfxPostQuad *q, s32 r, s32 g, s32 b, s32 a);
void GfxPostQuad_Set(GfxPostQuad *q, s32 baseX, s32 baseY, s16 u0, s16 v0, s16 u1, s16 v1, s32 dx, s32 dy, s32 w, s32 h,
                     s16 half);

#endif
