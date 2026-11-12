#ifndef BATTLE_EFT_G_H
#define BATTLE_EFT_G_H

#include "types.h"
#include "sys/math3d.h"

/*
 * src/battle/eft_g.c, 0x147050..0x14B108. Five pieces of effect code:
 *
 * 1. 0x147050..0x147928: helpers for the module before this one (blast records).
 * 2. 0x147928..0x148DF8: "storm", sub-task 3 of scene layer 0: lightning sprites on the horizon and rain
 *    lines around the camera. Visual only.
 * 3. 0x148DF8..0x149818: smoke emitters, sub-task 4 of scene layer 0. Visual only.
 * 4. 0x149818..0x14A828: stage boundary wall, sub-task 8 of scene layer 0. Visual only.
 * 5. 0x14A828..0x14B108: the start of scene layer 1, the shot slots of each character (ki blasts and
 *    techniques). This part belongs to the simulation; it continues in the file after this one.
 *
 * All struct names are local views; every field meaning not backed by a use in this file is a guess.
 */

/* The part of a task (0x40 bytes, 0x1AD150..) that the callbacks here use. */
typedef struct EftTask {
    /* 0x00 */ u8 flags;       /* bit 0: killed */
    /* 0x01 */ u8 chr;         /* EftShotChar_Init stores the character index here */
    /* 0x02 */ u8 unk2[0x26];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;     /* work block of the size given to BtlTask_CreateChildList */
} EftTask;

/* A GS screen position as the projection helpers (0x1210D8, 0x121140) write it. */
typedef struct EftScrXyz {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 unkC;
} EftScrXyz; /* 0x10 */

/* One entry of a texture set loaded by EftTexSet_Load32: the GS TEX0 value, advanced by EftVram_AddTex. */
typedef struct EftTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftTexEntry; /* 0x10 */

typedef struct EftTexSet {
    /* 0x000 */ EftTexEntry entry[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ s32 unk204;
} EftTexSet; /* 0x208 */

/* ---- blast record helpers -------------------------------------------------------------- */

/* Definition of a shot (0x8C bytes, EftShotChar.def[]); only what this file reads. */
typedef struct EftShotDef {
    /* 0x00 */ s16 id;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s8 unk4;
    /* 0x05 */ s8 kind;
    /* 0x06 */ u8 unk6[0x36];
    /* 0x3C */ s32 flags;      /* bit 0x2000 tested by EftShot_IsSlotFlag2000 */
    /* 0x40 */ u8 unk40[0x4C];
} EftShotDef; /* 0x8C */

/* One shot slot of a character (0x50 bytes). A blast record's "source" (+0x64) points at one. */
typedef struct EftShotSlot {
    /* 0x00 */ u8 unk0[0x1C];
    /* 0x1C */ void *pack;     /* the slot's data in the character pack (EftShot_GetCharPack) */
    /* 0x20 */ s32 unk20;
    /* 0x24 */ EftShotDef *def;
    /* 0x28 */ u8 unk28[0x28];
} EftShotSlot; /* 0x50 */

/* The part of a blast record (0x190 bytes, include/battle/btl_scene.h) read by EftRec_GetDefClass. */
typedef struct EftRecView {
    /* 0x00 */ u8 unk0[0x64];
    /* 0x64 */ EftShotSlot *src;
} EftRecView;

/* ---- storm ----------------------------------------------------------------------------- */

typedef struct EftStormBolt {
    /* 0x00 */ Vec4 pos;       /* relative to nothing: a point about 5000 away from the origin, y = -700 */
    /* 0x10 */ s32 life;       /* frames left; 0 = respawn */
    /* 0x14 */ s32 lifeMax;
    /* 0x18 */ s32 texA;       /* 0..2, texture of the first sprite */
    /* 0x1C */ s32 texB;       /* 3..5, texture of the second sprite */
    /* 0x20 */ s32 wait;       /* frames before life starts to run */
    /* 0x24 */ s32 kind;       /* 1: one sprite, 0: two */
    /* 0x28 */ s32 unk28[2];
} EftStormBolt; /* 0x30 */

typedef struct EftStormDrop {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 vel;       /* (0, 19.6, 0, 0) */
} EftStormDrop; /* 0x20 */

#define EFT_STORM_BOLTS 4
#define EFT_STORM_DROPS 64

/* gEftStorm, 0x8EF0 bytes from the pool. */
typedef struct EftStorm {
    /* 0x0000 */ EftTexSet tex;
    /* 0x0208 */ s32 unk208[2];
    /* 0x0210 */ EftStormBolt bolt[EFT_STORM_BOLTS];
    /* 0x02D0 */ u8 unk2D0[0x8400];
    /* 0x86D0 */ EftStormDrop drop[EFT_STORM_DROPS];
    /* 0x8ED0 */ Vec4 prevCam[2]; /* camera position of each view on the previous draw */
} EftStorm; /* 0x8EF0 */

/* ---- smoke emitters -------------------------------------------------------------------- */

/* Argument of EftSmoke_Create. */
typedef struct EftSmokeArg {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 ambient;   /* r, g, b 0..255 */
    /* 0x20 */ Vec4 diffuse;   /* added scaled by the light term */
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 alpha;
    /* 0x38 */ f32 speed;
    /* 0x3C */ f32 damp;       /* horizontal velocity factor per frame */
    /* 0x40 */ s32 lifeBase;
    /* 0x44 */ s32 lifeRange;
    /* 0x48 */ s32 rate;       /* one particle per frame with probability 1 / rate */
    /* 0x4C */ s32 unk4C;
} __attribute__((aligned(16))) EftSmokeArg; /* 0x50; copied with 64-bit loads, so 16-byte aligned */

/* Record of the stage's emitter list (BtlStage_GetFxResB2). */
typedef struct EftSmokeSrc {
    /* 0x00 */ f32 pos[3];
    /* 0x0C */ f32 damp;
    /* 0x10 */ f32 size;
    /* 0x14 */ f32 speed;
    /* 0x18 */ u16 lifeBase;
    /* 0x1A */ u16 rate;
    /* 0x1C */ u8 ambient[4];
    /* 0x20 */ u8 diffuse[4];
    /* 0x24 */ u8 unk24[0xC];
} EftSmokeSrc; /* 0x30 */

typedef struct EftSmokePart {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 vel;
    /* 0x20 */ u16 life;
    /* 0x22 */ u8 unk22[0xE];
} EftSmokePart; /* 0x30 */

#define EFT_SMOKE_PARTS 40

/* Work block of an emitter task. */
typedef struct EftSmoke {
    /* 0x000 */ void *tex;     /* &gEftSmokeMgr->tex */
    /* 0x004 */ s32 unk4[3];
    /* 0x010 */ EftSmokeArg arg;
    /* 0x060 */ s32 flags;     /* bit 0: stop emitting, die when the last particle is gone */
    /* 0x064 */ s32 unk64[3];
    /* 0x070 */ EftSmokePart part[EFT_SMOKE_PARTS];
} EftSmoke; /* 0x7F0 */

/* gEftSmokeMgr, 0x90 bytes. */
typedef struct EftSmokeMgr {
    /* 0x00 */ void *list;     /* child task list of emitters */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 tex[0x20];   /* written by EftTexSet_Load8 */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ u8 unk30[0x60];
} EftSmokeMgr; /* 0x90 */

/* ---- boundary wall --------------------------------------------------------------------- */

/* A vector or matrix passed by value: the callee copies it with 64-bit loads, so the type is 16-byte aligned. */
typedef struct EftVecArg {
    f32 v[4];
} __attribute__((aligned(16))) EftVecArg;

typedef struct EftMtxArg {
    f32 m[4][4];
} __attribute__((aligned(16))) EftMtxArg;

typedef struct EftBoundVtx {
    /* 0x00 */ EftVecArg pos;
    /* 0x10 */ EftVecArg unk10;
    /* 0x20 */ EftVecArg st;   /* s, t, q = 1, 0 */
    /* 0x30 */ EftVecArg col;  /* rgb from the stage, a = fade */
} EftBoundVtx; /* 0x40 */

/* Vertex numbers of a quad, copied as a whole (two 64-bit moves). */
typedef struct EftBoundIdx {
    s32 i[4];
} __attribute__((aligned(8))) EftBoundIdx;

typedef struct EftBoundQuad {
    /* 0x00 */ EftBoundIdx idx;
    /* 0x10 */ Vec4 normal;
} EftBoundQuad; /* 0x20 */

typedef struct EftBoundMesh {
    /* 0x000 */ s32 active;
    /* 0x004 */ s16 quadCount;
    /* 0x006 */ s16 unk6;
    /* 0x008 */ f32 dist;      /* how far the fighter is from the stage axis, minus its radius */
    /* 0x00C */ f32 arc;       /* angle the mesh spans */
    /* 0x010 */ f32 cols;      /* 6 */
    /* 0x014 */ f32 rows;      /* 4 */
    /* 0x018 */ f32 phase;
    /* 0x01C */ f32 scrollS;
    /* 0x020 */ f32 scrollT;
    /* 0x024 */ f32 unk24[3];
    /* 0x030 */ Vec4 center;
    /* 0x040 */ EftBoundVtx vtx[24];
    /* 0x640 */ EftBoundQuad quad[15];
    /* 0x820 */
} EftBoundMesh; /* 0x820 */

/* Parameters, entry 0x17 of the stage effect pack. */
typedef struct EftBoundParam {
    /* 0x00 */ f32 range;      /* distance from the wall at which it starts to show */
    /* 0x04 */ f32 height;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 radiusAdd;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 alpha;
    /* 0x18 */ f32 fadeDist;
    /* 0x1C */ f32 arc;
    /* 0x20 */ f32 texArc;
    /* 0x24 */ f32 texHeight;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 scrollS;
    /* 0x30 */ f32 scrollT;
    /* 0x34 */ f32 phaseRate;
    /* 0x38 */ s32 otZ;
} EftBoundParam;

/* Work block of the per-fighter task. */
typedef struct EftBoundChar {
    /* 0x0000 */ s32 objId;
    /* 0x0004 */ f32 radius;   /* BtlStage_GetInnerRadius() + param->radiusAdd */
    /* 0x0008 */ f32 unk8;
    /* 0x000C */ f32 unkC;
    /* 0x0010 */ f32 angle;    /* atan2(x, z) of the fighter */
    /* 0x0014 */ f32 scale;    /* height / 19.35, at least 0.7 */
    /* 0x0018 */ f32 charRadius;
    /* 0x001C */ f32 unk1C;
    /* 0x0020 */ Vec4 pos;     /* node 3 of the fighter */
    /* 0x0030 */ EftBoundMesh wall;
    /* 0x0850 */ EftBoundMesh unk850; /* never activated in this file */
} EftBoundChar; /* 0x1070 */

/* gEftBound, 0x220 bytes. */
typedef struct EftBound {
    /* 0x000 */ s32 texStepped; /* the textures were advanced this frame */
    /* 0x004 */ s32 enabled;
    /* 0x008 */ s32 unk8;
    /* 0x00C */ void *texPack;
    /* 0x010 */ EftTexSet tex;  /* count at +0x210 */
    /* 0x218 */ struct EftBoundParam *param; /* stage pack entry 23 */
    /* 0x21C */ f32 *color;     /* stage pack entry 22: r, g, b */
} EftBound; /* 0x220 */

/* ---- shot slots ------------------------------------------------------------------------ */

#define EFT_SHOT_SLOTS 5

typedef struct EftShotChar {
    /* 0x000 */ EftShotSlot slot[EFT_SHOT_SLOTS];
    /* 0x190 */ u8 unk190[0x50];
    /* 0x1E0 */ EftShotDef def[EFT_SHOT_SLOTS];
    /* 0x49C */ u8 unk49C[0x8C];
    /* 0x528 */ void *list;    /* child task list (6 tasks of 0x2000 bytes) */
    /* 0x52C */ void *task;    /* the character's task */
    /* 0x530 */ s32 curSlot;   /* slot in use, -1 when the fighter is in neither a technique nor a skill */
    /* 0x534 */ s32 unk534[3];
} EftShotChar; /* 0x540 */

/* gEftShot, 0xC bytes. */
typedef struct EftShotMgr {
    /* 0x00 */ void *list;
    /* 0x04 */ EftShotChar *chr;
    /* 0x08 */ s32 count;
} EftShotMgr;

extern EftStorm *gEftStorm;
extern EftSmokeMgr *gEftSmokeMgr;
extern EftBound *gEftBound;
extern void *gEftBoundList;
extern EftShotMgr *gEftShot;

s32 EftRec_GetDefClass(EftRecView *rec);
void EftUtil_MakeFacingMtx(Mtx44 *out, Vec4 *dir, Vec4 *pos);
void EftUtil_ClipSegToWater(Vec4 *out, Vec4 *a, Vec4 *b);
s32 EftUtil_IsCamUnderWater(void);
s32 EftUtil_IsScreenPosClipped(EftScrXyz *p);

void EftStorm_Init(EftTask *task);
void EftStorm_Term(void);
void EftStorm_Update(void);
void EftStorm_DrawBolts(EftStormBolt *bolt);
void EftStorm_Draw(void);
void EftStorm_PostUpdate(void);
void EftStorm_Reset(EftTask *task);
void EftStorm_SpawnBolt(EftStormBolt *bolt);
void EftStorm_InitRain(void);
void EftStorm_MoveRain(void);
void EftStorm_DrawRainLines(EftStormDrop *drop, s32 count, Vec4 *streak);
void EftStorm_WrapRain(Vec4 *center, EftStormDrop *drop, s32 count);
void EftStorm_DrawRain(void);

void *EftSmoke_Create(EftSmokeArg *arg);
void EftSmoke_Stop(EftTask *task);
Vec4 *EftSmoke_GetPos(EftTask *task);
void EftSmoke_SetPos(EftTask *task, Vec4 *pos);
void EftSmokeMgr_Init(EftTask *task);
void EftSmokeMgr_Term(void);
void EftSmokeMgr_Update(void);
void EftSmoke_Init(EftTask *task, EftSmokeArg *arg);
void EftSmoke_Term(void);
void EftSmoke_Reset(void);
void EftSmoke_Update(EftTask *task);
void EftSmoke_Draw(EftTask *task);

void EftBound_UpdateAngle(EftBoundChar *work, EftBoundChar *src);
void EftBound_BuildWall(EftBoundChar *work, EftBoundChar *src);
void EftBound_DrawTri(EftMtxArg m, EftVecArg p0, EftVecArg p1, EftVecArg p2, EftVecArg st0, EftVecArg st1,
                      EftVecArg st2, EftVecArg c0, EftVecArg c1, EftVecArg c2, s32 otZ, u64 tex0);
void EftBound_DrawMeshCulled(EftBoundMesh *mesh);
void EftBound_DrawMesh(EftBoundMesh *mesh);
void EftBound_Init(EftTask *task, s32 *arg);
void EftBound_Term(void);
void EftBound_Update(EftTask *task);
void EftBound_PostUpdate(void);
void EftBound_Reset(void);
void EftBound_Draw(EftTask *task);
void EftBoundMgr_Init(EftTask *task);
void EftBoundMgr_Term(void);
void EftBoundMgr_Update(void);
s32 EftBoundMgr_AddChar(s32 objId);
void EftBoundMgr_Enable(void);
void EftBoundMgr_Disable(void);

u64 EftShot_AttrMaskFromBits(u64 bits);
s32 EftShot_TestBits(s32 objId, s32 mask);
s32 EftShot_GetAttrValue(s32 objId, u64 bits);
s32 EftShot_GetAttrKind(s32 objId, u64 bits);
s32 EftShot_IsSlotFlag2000(s32 chr, s32 slot);
s32 EftShot_HasTwoAttrs(s32 objId, u64 bits);
s32 EftShot_GetAttrPair(s32 objId, u64 bits, s32 *outA, s32 *outB);
void EftShot_SetHeldFlagA8(s32 objId);
void EftShot_SetHeldFlagA9(s32 objId);
void EftShot_Nop(void);
void EftShot_SetCurSlot(s32 chr, s32 slot);
s32 EftShot_GetCurSlot(s32 chr);
void EftShot_PlayFireSound(s32 objId, EftShotSlot *slot);
void EftShotMgr_Init(EftTask *task);
void EftShotMgr_Term(void);
void EftShotMgr_Update(void);
void EftShotChar_Init(EftTask *task, s32 *arg);
void EftShotChar_Term(EftTask *task);
void EftShotChar_Update(void);
void EftShot_SetCharPool(s32 chr);
void EftShot_ResetCharPool(s32 chr);
s32 EftShot_InitSlot(s32 chr, s32 slot, void *pack);

#endif
