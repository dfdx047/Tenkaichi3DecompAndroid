#ifndef BATTLE_STG_B_H
#define BATTLE_STG_B_H

#include "types.h"
#include "sys/math3d.h"
#include "sys/ramp.h"

/* One (count, pointer) section of the loaded stage data. */
typedef struct StgInfo {
    /* 0x00 */ s32 id;         /* stage number (0..30 pick the ambience handler; MapBgm id = 0x10BA4 + id) */
    /* 0x04 */ s32 flags;      /* bit 1: the stage has a water level (BtlStage_GetWaterLevel) */
    /* 0x08 */ f32 waterY;
    /* 0x0C */ s32 changeKind; /* 1 / 2: what a stage change leads to, see BtlStage_GetChangeTarget */
} StgInfo;

/* Section +0x58: only the byte at +0x16 is read here. */
typedef struct StgSec58 {
    /* 0x00 */ u8 unk0[0x16];
    /* 0x16 */ u8 unk16;
} StgSec58;

/* The loaded stage data (gBtlStage->data). Only what this file reads. */
typedef struct StgData {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s32 infoCount;
    /* 0x14 */ StgInfo *info;
    /* 0x18 */ u8 unk18[0xC];
    /* 0x24 */ Vec4 *lightDir;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ f32 *place;    /* two points 8 bytes apart per coordinate: x0, z0, x1, z1 at [0], [2], [4], [6] */
    /* 0x30 */ u8 unk30[0x10];
    /* 0x40 */ s32 n40;
    /* 0x44 */ s32 *sec44;    /* records of 0x10 bytes, bit 0 of the first word = present */
    /* 0x48 */ s32 n48;
    /* 0x4C */ s32 *sec4C;
    /* 0x50 */ s32 n50;
    /* 0x54 */ s32 *sec54;
    /* 0x58 */ s32 n58;
    /* 0x5C */ StgSec58 *sec5C;
    /* 0x60 */ s32 n60;
    /* 0x64 */ s32 *sec64;
    /* 0x68 */ u8 unk68[8];
    /* 0x70 */ s32 n70;
    /* 0x74 */ s32 *flags;    /* STG_FLAG_* */
    /* 0x78 */ u8 unk78[0x14];
    /* 0x8C */ u8 *ambient;   /* r, g, b */
    /* 0x90 */ s32 n90;
    /* 0x94 */ s32 *sec94;
    /* 0x98 */ s32 n98;
    /* 0x9C */ s32 *sec9C;
    /* 0xA0 */ s32 nA0;
    /* 0xA4 */ s32 *secA4;
} StgData;

#define STG_FLAG_FX_A 1    /* gBtlStageFxRes entry A may be used */
#define STG_FLAG_FX_B 2
#define STG_FLAG_FX_C 4
#define STG_FLAG_8 8
#define STG_FLAG_MOON 0x10 /* required by one transformation kind */

/* One timer of gBtlStage->timers. */
typedef struct StgTimer {
    /* 0x00 */ u16 flags; /* bit 2: the period ended this frame */
    /* 0x02 */ u16 unk2;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ f32 time;
    /* 0x0C */ f32 period;
} StgTimer; /* size 0x10 */

/* The stage manager (gBtlStage, gp 0x2FEBE0). Only what this file reads. */
typedef struct BtlStageMgr {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ StgData *data;
    /* 0x08 */ s32 flags;     /* bit 0: not ready (BtlStage_IsReady) */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u32 timerCount;
    /* 0x14 */ StgTimer *timers;
} BtlStageMgr;

/* Member 16 of the stage file (gBtlStageFxRes, gp 0x2FEBF4). */
typedef struct StgFxRes {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ void *a;
    /* 0x0C */ void *a2;
    /* 0x10 */ void *b;
    /* 0x14 */ void *b2;
    /* 0x18 */ void *c;
    /* 0x1C */ void *c2;
} StgFxRes;

/* Stage ambience: the looping background sound of a stage and its per-frame volume handler. */
typedef struct StgAmb {
    /* 0x00 */ f32 bgmScale;        /* scales every volume given to StgAmb_SetBgmVolume; 1 at start */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ f32 seScale;         /* scales the volume of StgAmb_PlaySe; 1 at start */
    /* 0x0C */ s32 timer;           /* frames until the next random one-shot (thunder) */
    /* 0x10 */ s32 unk10[2];
    /* 0x18 */ void (*update)(void); /* the stage's handler, called once per stage update */
} StgAmb; /* size 0x1C */

/* A two-tag DMA chain that sends one captured half: REF to the buffer, then END. */
typedef struct ScrXfadeChain {
    /* 0x00 */ u32 ref[4];
    /* 0x10 */ u32 end[4];
} ScrXfadeChain; /* size 0x20 */

/* Screen cross-fade (gScrXfade, gp 0x2FEC00, 0x70 bytes from the heap): the last frame, captured in two halves
   and drawn over the new picture with a falling alpha. */
typedef struct ScrXfade {
    /* 0x00 */ u64 *buf[2];       /* 0x60090 bytes each: an upload packet around 512 x 224 pixels of 24 bits */
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ ScrXfadeChain chain[2];
    /* 0x50 */ s32 alpha;         /* 128 -> 0 while fading; 0 = nothing drawn */
    /* 0x54 */ s32 request;       /* 1: capture at the end of this frame */
    /* 0x58 */ Ramp ramp;         /* 0 -> 1 over the fade time */
} ScrXfade; /* size 0x70 */

/* One screen shock wave: a ring of 20 x 2 quads that redraws the frame with shifted texture coordinates. */
typedef struct ScrWarpObj {
    /* 0x000 */ s32 flags;        /* bit 0: in use; bit 1: screen position computed */
    /* 0x004 */ s32 view;         /* 0 / 1: the camera view it was spawned for */
    /* 0x008 */ f32 life;         /* frames left */
    /* 0x00C */ f32 radius;       /* inner radius, pixels; freed above 600 */
    /* 0x010 */ f32 speed;        /* radius += speed */
    /* 0x014 */ f32 speedAcc;     /* speed += speedAcc: -(speed * 0.3) / life */
    /* 0x018 */ f32 width;        /* ring width */
    /* 0x01C */ f32 widthVel;     /* width += widthVel: width * 3.2 / life */
    /* 0x020 */ f32 alpha;        /* 128 / 255, falls to 0 over the last 6 frames */
    /* 0x024 */ f32 alphaVel;
    /* 0x028 */ s32 unk28[2];
    /* 0x030 */ Vec4 pos;         /* world position */
    /* 0x040 */ Vec4 screen;      /* projected, relative to the frame's corner; x -= 256 for view 1 in split-screen */
    /* 0x050 */ f32 cx;           /* ring centre in frame pixels */
    /* 0x054 */ f32 cy;
    /* 0x058 */ s32 unk58[2];
    /* 0x060 */ f32 jitter[20];   /* where between the inner and outer edge the middle vertex lies */
    /* 0x0B0 */ f32 xy[20][3][2]; /* per direction: inner, middle, outer vertex */
    /* 0x290 */ f32 uv[20][3][2]; /* texture coordinates in the half-size frame copy */
} ScrWarpObj; /* size 0x470 */

#define SCRWARP_MAX 10
#define SCRWARP_DIRS 20

/* The shock wave manager (gScrWarp, gp 0x2FEC04, 0xAC bytes from the heap). */
typedef struct ScrWarp {
    /* 0x00 */ ScrWarpObj *objs;        /* SCRWARP_MAX objects */
    /* 0x04 */ f32 dir[SCRWARP_DIRS][2]; /* sin, cos of -pi + i * pi / 10 */
    /* 0xA4 */ s32 count[2];            /* objects in use per view */
} ScrWarp; /* size 0xAC */

/* A control point of the depth curve: x = depth index 0..255, z = value 0..255 (y unused, w = 1). */
typedef struct StgFogPoint {
    /* 0x00 */ Vec4 v;
    /* 0x10 */ u8 unk10[0x30];
} StgFogPoint; /* size 0x40 */

/* Stage parameters of the depth curve (argument of StgFog_SetParams). */
typedef struct StgFogParams {
    /* 0x00 */ s32 flags;  /* bit 1: StgFog.clearLast */
    /* 0x04 */ u8 pt[6];   /* x, z of the three control points */
} StgFogParams;

/* Depth tone (gStgFog, gp 0x2FEC08, 0x600 bytes from the heap): an alpha look-up table indexed by depth, drawn
   over the frame with the Z buffer as an 8-bit texture. */
typedef struct StgFog {
    /* 0x000 */ u8 tex[0x490];  /* texture object set up by GfxClut_InitPacket */
    /* 0x490 */ u8 *clutSrc;
    /* 0x494 */ u8 load[0x20];  /* 0x10 bytes of it are handed to Dma_AddData: loads the table */
    /* 0x4B4 */ u16 cbp;
    /* 0x4B6 */ u8 unk4B6[0xA];
    /* 0x4C0 */ u8 *clut;       /* 256 RGBA entries in CSM1 order; only alpha is written */
    /* 0x4C4 */ u8 unk4C4[0x3C];
    /* 0x500 */ StgFogPoint pt[3];
    /* 0x5C0 */ u8 unk5C0[0x10];
    /* 0x5D0 */ u32 color;      /* 0x80808080 */
    /* 0x5D4 */ s32 clearLast;  /* 1: the last table entry (the farthest depth) is forced to 0 */
    /* 0x5D8 */ u8 unk5D8[0x28];
} StgFog; /* size 0x600 */

extern BtlStageMgr *gBtlStage;
extern ScrWarp *gScrWarp;
extern StgFog *gStgFog;
extern ScrXfade *gScrXfade;
extern StgFxRes *gBtlStageFxRes;

s32 BtlStage_HasFeature(u32 kind);
void *BtlStage_GetFileMember(s32 index);
void *BtlStage_GetFxResA(void);
void *BtlStage_GetFxResB(void);
void *BtlStage_GetFxResC(void);
void *BtlStage_GetFxResA2(void);
void *BtlStage_GetFxResB2(void);
void *BtlStage_GetFxResC2(void);
s32 BtlStage_HasFlag8(void);
s32 BtlStage_HasMoon(void);
s32 BtlStage_GetAmbient(s32 *rgba);
s32 BtlStage_SetAmbient(s32 *rgba);
s32 BtlStage_GetId(void);
s32 BtlStage_GetLightDir(Vec4 *out);
s32 BtlStage_GetPlace2C(Vec4 *pos, Vec4 *rot);
s32 BtlStage_GetChangeTarget(void);
void BtlStage_UpdateTimers(void);
void BtlStage_Update(void);


void StgAmb_SetBgmVolume(s32 volume);
s32 StgAmb_PlaySe(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch);
f32 StgAmb_Falloff(f32 x, f32 near, f32 far);
void StgAmb_Start(void);
void StgAmb_Update(void);
void StgAmb_Stop(void);
void StgAmb_Term(void);
void StgAmb_Init(void);
void StgAmb_SetBgmScale(f32 scale);
void StgAmb_SetSeScale(f32 scale);


void ScrXfade_AllocBuffers(void);
void ScrXfade_FreeBuffers(void);
void ScrXfade_Capture(void);
void ScrXfade_StoreHalf(s16 sbp, u16 w, u16 h, s32 index);
void ScrXfade_Draw(s32 unused, s32 tbp, u8 alpha);
void ScrXfade_Init(void);
void ScrXfade_Term(void);
void ScrXfade_Reset(void);
void ScrXfade_Update(void);
void ScrXfade_PostDraw(void);
void ScrXfade_Start(s32 request, f32 seconds);
void ScrXfade_RequestCapture(void);
s32 ScrXfade_IsActive(void);


void ScrWarp_BeginPacket(u64 **pp, s32 x0, s32 x1, s32 w, s32 h, s32 tw, s32 th);
void ScrWarp_EndPacket(u64 **pp);
void ScrWarp_BeginStrip(u64 **pp, s32 count);
void ScrWarp_EndStrip(u64 **pp);
void ScrWarp_PutVertex(u64 **pp, s32 x, s32 y, s32 u, s32 v, s32 rgba);
ScrWarpObj *ScrWarp_Alloc(s32 view);
void ScrWarp_Free(ScrWarpObj *o);
s32 ScrWarp_IsOnScreen(Vec4 *screen);
void ScrWarp_Step(ScrWarpObj *o);
void ScrWarp_Draw(s32 split, s32 view);
void ScrWarp_Init(void);
void ScrWarp_Term(void);
void ScrWarp_Reset(void);
void ScrWarp_UpdateAll(void);
void ScrWarp_DrawAll(void);
void ScrWarp_Update(void);
s32 ScrWarp_Spawn(s32 view, Vec4 *pos, f32 seconds, f32 radius, f32 width, f32 speed, f32 jitter);

void StgFog_SetupTex(StgFog *tone, u16 arg);
void StgFog_BuildClut(StgFog *tone, StgFogPoint *pts, s32 clearLast, f32 scale);
void StgFog_Init(u16 arg);
void StgFog_Term(void);
void StgFog_ResetColor(void);
void StgFog_Draw(void);
void StgFog_SetParams(StgFogParams *prm);

#endif
