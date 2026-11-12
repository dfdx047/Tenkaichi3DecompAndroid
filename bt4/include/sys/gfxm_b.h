#ifndef SYS_GFXM_B_H
#define SYS_GFXM_B_H

#include "types.h"
#include "sys/dma.h"
#include "sys/math3d.h"
#include "sys/gfxm_a.h"

/* The screen quad GfxPostQuad_Set works out: the type is GfxPostQuad of sys/gfxm_a.h (the two files were written
 * side by side with one copy each; this alias keeps this file's name for it). */
typedef GfxPostQuad GfxQuad;

/* Integer vector as Vec4_ToFixed4 writes it. */
typedef struct IVec4 {
    s32 x, y, z, w;
} IVec4;

/* One texture of a texture file (0x40 bytes; only what this module reads). */
typedef struct GfxTexEntry {
    /* 0x00 */ u8 unk00[0x30];
    /* 0x30 */ u64 tex0;
    /* 0x38 */ u8 unk38[8];
} GfxTexEntry;

/* Header of a texture file. */
typedef struct GfxTexFile {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ GfxTexEntry *entries;
} GfxTexFile;

/* The camera view block GfxLens_DrawAll is given (gBtlCam + 0xC40 pointer). */
typedef struct GfxLensView {
    /* 0x00 */ u8 unk00[0x40];
    /* 0x40 */ Mtx44 mtx40; /* world-to-view matrix */
    /* 0x80 */ u8 unk80[0x200];
    /* 0x280 */ s32 unk280; /* non-zero: the view is shown (split screen) */
    /* 0x284 */ u8 unk284[0xC];
} GfxLensView; /* 0x290 */

typedef struct GfxLensCam {
    /* 0x000 */ u8 unk000[0x720];
    /* 0x720 */ GfxLensView views[2];
    /* 0xC40 */ GfxLensView *view;
} GfxLensCam;

/* Work of a CLUT built at run time by GfxClut_InitPacket (0x4C0 bytes). */
typedef struct GfxClutWork {
    /* 0x000 */ u8 packet[0x490]; /* upload packet: headers + 0x400 bytes of CLUT */
    /* 0x490 */ u8 *clut;         /* the CLUT inside `packet` */
    /* 0x494 */ u32 ref[4];       /* DMA tag that sends `packet` */
    /* 0x4A4 */ u32 end[4];       /* DMA END tag */
    /* 0x4B4 */ u16 cbp;
    /* 0x4B6 */ u8 unk4B6[0xA];
} GfxClutWork;

/* One lens (0x30 bytes): a camera-facing disc that shows the screen behind it through a round texture. */
typedef struct GfxLens {
    /* 0x00 */ Vec4 pos;    /* world position of the centre */
    /* 0x10 */ s32 life;    /* frames left; 0 = slot free */
    /* 0x14 */ s32 lifeMax; /* frames it started with */
    /* 0x18 */ u32 rgba;
    /* 0x1C */ f32 size;    /* world half-size at the start; the disc shrinks to 0 with life / lifeMax */
    /* 0x20 */ s32 blend;   /* 0 normal, 1 additive, 2 subtractive */
    /* 0x24 */ s32 hold;    /* non-zero: does not age */
    /* 0x28 */ s32 unk28[2];
} GfxLens;

typedef struct GfxLensWork {
    /* 0x000 */ GfxLens slot[8];
    /* 0x180 */ GfxTexFile *tex; /* texture file of the lens shape (never set inside this range) */
    /* 0x184 */ s32 unk184[3];
} GfxLensWork; /* 0x190 */

/* Underwater wobble state of one view (0x14 bytes; gGfxWater points to two). */
typedef struct GfxWaterLayer {
    /* 0x00 */ f32 phaseU; /* phase of the wave around the first centre, radians; wraps by 2 pi above pi */
    /* 0x04 */ f32 phaseV; /* phase of the wave around the second centre */
    /* 0x08 */ f32 speedU; /* added to phaseU per frame (0.06) */
    /* 0x0C */ f32 speedV; /* added to phaseV per frame (0.04) */
    /* 0x10 */ u32 rgba;   /* tint of the redrawn screen */
} GfxWaterLayer;

/* The stage record BtlStage_GetList90 returns (only what this module reads). */
typedef struct GfxWaterStage {
    /* 0x00 */ u32 flags; /* bit 0: the stage has water */
    /* 0x04 */ u8 r, g, b, a;
} GfxWaterStage;

extern GfxWaterLayer *gGfxWater;
extern GfxClutWork gGfxDepthFog;
extern GfxLensView *gBtlCamView;
extern s32 D_002FE8D0[];
extern GfxLensWork gGfxLens;
extern GfxClutWork *gGfxAlphaKey;
extern GfxLensCam *gBtlCam;

void GfxPost_DrawDepthClut(s32 mode, s32 tbp, s32 cbp, u64 alpha);
void GfxPost_CopyAlphaToDepth(void);
void GfxPost_DrawTexRect(u64 tex0, s32 x, s32 y, s32 w, s32 h, s32 fullHeight);
void GfxPost_DrawTintRect(u64 alpha, Vec4 *color);
void GfxLens_Init(void);
void GfxLens_Clear(void);
s32 GfxLens_Start(Vec4 *pos, s32 life, u32 rgba, s32 blend, f32 size);
void GfxLens_Stop(u32 slot);
void GfxLens_SetHold(u32 slot, s32 hold);
void GfxLens_SetPos(u32 slot, Vec4 *pos);
void GfxLens_DrawFull(void);
void GfxAlphaKey_Init(void);
void GfxAlphaKey_Term(void);
void GfxAlphaKey_Draw(void);
void GfxWater_Init(void);
void GfxWater_Term(void);
void GfxWater_LoadStageColor(void);
void GfxWater_Draw(void);
void GfxDepthFog_Init(u16 cbp);
void GfxDepthFog_Draw(void);

#endif
