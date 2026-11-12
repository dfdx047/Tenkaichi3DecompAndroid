#ifndef BATTLE_EFT_D_H
#define BATTLE_EFT_D_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect drawing code 0x13A9D0..0x13EA00, in two C files that follow the original object boundary. Neither
 * reads or writes a fighter, a hit record or a battle object.
 *
 * src/battle/eft_d.c    0x13A9D0..0x13C300  the end of the animated stage surfaces (water, lava, ...) whose
 *                                           work block is gEftSurf (gp 0x2FE9C4) and whose first part is
 *                                           eft_c.c (0x138178..): four routines that turn one lit triangle
 *                                           into GS packets, and six parameter accessors nobody calls.
 * src/battle/eft_d_b.c  0x13C300..0x13EA00  the first half of the stage-change transition ("burst"): the three
 *                                           ways a particle is drawn, the model drawn around the camera, the
 *                                           particle initialisers and their per-frame updates. The task that
 *                                           creates, steps and draws them is eft_e.c (0x13EA00..0x13F3D8).
 *
 * The types below are local views. EftSurfVtxD / EftSurfD repeat EftSurfVtx / EftSurf of eft_c.h, and
 * EftBurstPtcl / EftBurstWork / EftBurstRes are the full layouts of EftTransPart / EftTransWork / EftTransRes
 * of eft_e.h.
 */

/* ---- surface triangles ---------------------------------------------------------------- */

/* A projected vertex as ClipPoly_ProjectCur / Mtx_ProjectPoint write it: GS XYZ, x and y in 12.4 fixed point. */
typedef struct EftScrPos {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;     /* <= 0: behind the camera */
    /* 0x0C */ s32 unkC;
} EftScrPos; /* 0x10 */

/* One vertex of the surface module. A triangle is three of them; the clipper (ClipPoly_ClipPlane) grows it in
   place to up to nine. */
typedef struct EftSurfVtxD {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 st;    /* s, t, q */
    /* 0x20 */ Vec4 col;   /* rgb 0..255, w = alpha */
} EftSurfVtxD; /* 0x30 */

/* The part of the surface work block this file reaches. */
typedef struct EftSurfD {
    /* 0x0000 */ u8 unk0[0x10];
    /* 0x0010 */ Vec4 *clipPlanes;   /* five planes, set by 0x138BE0 from EftGfx_GetClipPlanes() */
    /* 0x0014 */ u8 unk14[0x12AC];
    /* 0x12C0 */ Vec4 colorA;        /* +0x12C0..0x1300: the parameter block (EftSurfParam of eft_c.h) */
    /* 0x12D0 */ Vec4 colorB;
    /* 0x12E0 */ Vec4 lightDir;
    /* 0x12F0 */ f32 param30;
    /* 0x12F4 */ s32 animFrames;
} EftSurfD;

/* ---- stage-change transition ---------------------------------------------------------- */

/* Texture record: the texture pack (transition file entry 3) has an array of them at +0x10, and the
   model (entry 2) has its own. */
typedef struct EftBurstTex {
    /* 0x00 */ s32 ofs0;    /* model textures: file offsets of two blocks */
    /* 0x04 */ s32 ofs4;
    /* 0x08 */ u8 unk8[8];
    /* 0x10 */ s32 w;
    /* 0x14 */ s32 h;
    /* 0x18 */ u8 unk18[8];
    /* 0x20 */ s32 vramX;   /* where the texture was uploaded */
    /* 0x24 */ s32 vramY;
    /* 0x28 */ u8 unk28[8];
    /* 0x30 */ u64 tex0;    /* GS TEX0 without the buffer address */
    /* 0x38 */ s32 ptr0;    /* ofs0 / ofs4 as pointers (EftBurst_RelocateModel) */
    /* 0x3C */ s32 ptr4;
} EftBurstTex; /* 0x40 */

struct EftBurstPtcl;
typedef s32 (*EftBurstUpdateFn)(struct EftBurstPtcl *);
typedef void (*EftBurstDrawFn)(struct EftBurstPtcl *);

/* EftBurstPtcl.flags */
#define EFT_BURST_F_FADE 0x01     /* set on glow and debris; no reader in this file */
#define EFT_BURST_F_TIMED 0x02    /* the update counts life down and ends the particle at 0 */
#define EFT_BURST_F_ADD 0x08      /* ALPHA 0x44 instead of 0x48 */
#define EFT_BURST_F_SCALE_X 0x10  /* DrawQuadRot: scale applies to x */
#define EFT_BURST_F_SCALE_Y 0x20  /* DrawQuadRot: scale applies to y */
#define EFT_BURST_F_COLOR 0x40    /* use color / alpha instead of the fixed grey and the life fade */
#define EFT_BURST_F_ROTATE 0x80   /* DrawQuadRot: rotate the quad by rot */
#define EFT_BURST_F_SUB 0x100     /* DrawSprite: ALPHA 0x42 */

typedef struct EftBurstPtcl {
    /* 0x00 */ Vec4 pos;       /* camera space; the transition sits around z = -50 */
    /* 0x10 */ Vec4 vel;
    /* 0x20 */ Vec4 rot;       /* x, y, z angles (streaks) */
    /* 0x30 */ Vec4 rotVel;
    /* 0x40 */ s32 life;       /* frames left; 0 = slot free for EftBurst_AllocPtcl */
    /* 0x44 */ s32 lifeMax;
    /* 0x48 */ EftBurstTex *tex;
    /* 0x4C */ f32 scale;
    /* 0x50 */ s32 flags;      /* EFT_BURST_F_* */
    /* 0x54 */ EftBurstUpdateFn update;   /* returns 0 when the particle ends */
    /* 0x58 */ EftBurstDrawFn draw;
    /* 0x5C */ u32 color;      /* rgb for RGBAQ */
    /* 0x60 */ s32 alpha;
    /* 0x64 */ s32 visible;    /* result of update this frame, tested by EftBurst_Draw (eft_e.c) */
    /* 0x68 */ f32 size;       /* UpdateRing: peak scale */
    /* 0x6C */ s32 unk6C;
} EftBurstPtcl; /* 0x70 */

#define EFT_BURST_PTCL_MAX 350

typedef struct EftBurstWork {
    /* 0x000 */ s32 spawnWait;
    /* 0x004 */ s32 frame;      /* 0..151 */
    /* 0x008 */ f32 fadeDist;   /* DrawModel: distance from the centre at which the model's light is gone */
    /* 0x00C */ f32 unkC;
    /* 0x010 */ f32 sway;       /* added to a spark's scale */
    /* 0x014 */ u8 unk14[0x21C];
    /* 0x230 */ EftBurstTex *curTex;   /* texture uploaded last (EftBurst_SetTexture) */
    /* 0x234 */ u8 unk234[0xC];
    /* 0x240 */ EftBurstPtcl ptcl[EFT_BURST_PTCL_MAX];
} EftBurstWork;

typedef struct EftBurstTexPack {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ EftBurstTex *tex;   /* [0] flash, [1] ring, [3] debris and streak, [4] glow, [5] spark, [6] model env map */
} EftBurstTexPack;

/* One group of the transition model. */
typedef struct EftBurstGroup {
    /* 0x00 */ s32 flags;      /* 0x800: second, environment-mapped pass */
    /* 0x04 */ s32 texIdx;
    /* 0x08 */ s16 *verts;     /* 3 s16 per vertex, unpacked by IVec4_Set */
    /* 0x0C */ s16 *tris;      /* 0x1E bytes per triangle: s16 vertex[3], s16 normal[3][2], u8 rgba[3][4] */
    /* 0x10 */ f32 scale;
    /* 0x14 */ s16 triCount;
    /* 0x16 */ s16 unk16;
} EftBurstGroup; /* 0x18 */

typedef struct EftBurstModel {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 groupCount;
    /* 0x08 */ EftBurstGroup *groups;
    /* 0x0C */ s32 texCount;
    /* 0x10 */ EftBurstTex *tex;
    /* 0x14 */ s32 relocated;
} EftBurstModel;

/* 0x31BE60 */
typedef struct EftBurstRes {
    /* 0x00 */ void *list;
    /* 0x04 */ EftBurstTexPack *texPack;   /* transition file entry 3 */
    /* 0x08 */ void *camAnim;              /* entry 1 */
    /* 0x0C */ EftBurstModel *model;       /* entry 2 */
    /* 0x10 */ s32 startReq;
    /* 0x14 */ s32 endReq;
} EftBurstRes; /* 0x18 */

extern EftSurfD *gEftSurf;
extern EftBurstWork *gEftBurst;
extern f32 gEftBurstStreakAngle;
extern EftBurstRes gEftBurstRes;

void EftSurf_DrawPolyOtClipped(EftSurfVtxD *poly, s32 unused, u64 *tex, Vec4 *fog, s32 zBias);
void EftSurf_SetAnimFrames(f32 v);
void EftSurf_SetParam30(f32 v);
Vec4 *EftSurf_GetParam(void);
void EftSurf_DrawTriOt(EftSurfVtxD *tri, s32 unused, u64 *tex, Vec4 *fog, s32 zBias);
void EftSurf_DrawTriOtEx(EftScrPos *scr0, EftScrPos *scr1, EftScrPos *scr2, Vec4 *col0, Vec4 *col1, Vec4 *col2,
                         Vec4 *st0, Vec4 *st1, Vec4 *st2, s32 unused, s32 z, u64 *tex, Vec4 *fog, s32 count);
void EftSurf_DrawTriDirect(EftScrPos *scr0, EftScrPos *scr1, EftScrPos *scr2, Vec4 *col0, Vec4 *col1, Vec4 *col2,
                           Vec4 *st0, Vec4 *st1, Vec4 *st2, s32 blend, u64 *tex, Vec4 *fog, s32 count);

void EftBurst_DrawQuad(EftBurstPtcl *p);
void EftBurst_DrawQuadRot(EftBurstPtcl *p);
void EftBurst_DrawSprite(EftBurstPtcl *p);
void EftBurst_RelocateModel(EftBurstModel *model);
u64 *EftBurst_PutModelEnv(u64 *pkt);
u64 *EftBurst_PutModelEnvEnd(u64 *pkt);
void EftBurst_ClearAlphaPlane(void);
void EftBurst_DrawModel(EftBurstModel *model, s32 unused);
void EftBurst_SetTexture(EftBurstTex *tex, s32 x, s32 y);
EftBurstPtcl *EftBurst_AllocPtcl(void);
void EftBurst_InitSpark(EftBurstPtcl *p, s32 idx);
void EftBurst_InitStreak(EftBurstPtcl *p);
void EftBurst_InitFlash(EftBurstPtcl *p);
void EftBurst_InitRing(EftBurstPtcl *p);
void EftBurst_InitGlow(EftBurstPtcl *p);
void EftBurst_InitDebris(EftBurstPtcl *p);
s32 EftBurst_UpdateFlash(EftBurstPtcl *p);
s32 EftBurst_UpdateStreak(EftBurstPtcl *p);
s32 EftBurst_UpdateSpark(EftBurstPtcl *p);
s32 EftBurst_UpdateMove(EftBurstPtcl *p);
s32 EftBurst_UpdateRing(EftBurstPtcl *p);
s32 EftBurst_UpdateGlow(EftBurstPtcl *p);
s32 EftBurst_UpdateDebris(EftBurstPtcl *p);

#endif
