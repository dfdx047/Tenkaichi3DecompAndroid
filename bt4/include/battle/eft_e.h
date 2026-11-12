#ifndef BATTLE_EFT_E_H
#define BATTLE_EFT_E_H

#include "types.h"
#include "sys/math3d.h"
#include "battle/btl_scene.h"

/*
 * Effect code 0x13EA00..0x142CA0 (as linked: 0x13EA00..0x13F430 is the second part of src/battle/eft_d_b.c, the rest src/battle/eft_e.c): the ends and beginnings of three modules. None of them
 * writes a fighter, a battle object or a hit record; the only simulation-side store is one bookkeeping bit in the
 * task that owns a hit record (EftWater_UpdateBlast).
 *
 *   EftBurst_*   0x13EA00..0x13F3D8   the stage-change transition. Its particles, model and draw code are the
 *                                     second half of eft_d.c (0x13C300..); here are the scene's layer 4 task,
 *                                     the transition task (create / update / draw) and the three entry points
 *                                     the stage swap of battle_load.c drives it with. One original source file
 *                                     with eft_d.c's half: EftBurst_Update only matches with EftBurst_InitFlash,
 *                                     EftBurst_InitRing and EftBurst_InitDebris defined earlier in the same file.
 *   Eft_IsScreenPos*  0x13F3D8..0x13F430   two screen-position tests without callers.
 *   EftSteam_*   0x13F430..0x140338   particle emitters, sub-task 5 of effect layer 0: an emitter owns up to 32
 *                                     camera-facing quads that rise, drift and die. One per record of the stage's
 *                                     third effect list (BtlStage_GetFxResC2), plus the second emitter of every
 *                                     geyser (eft_b.c / eft_c.c: EftGeyser). Visual only.
 *   EftWater_*   0x140338..0x142CA0   water surface effects, sub-task 7 of effect layer 0: the splash when a
 *                                     fighter, a ki blast or a technique crosses the stage's water level, the
 *                                     spray trail of a technique skimming it, and the wake of a fighter moving
 *                                     through it. The particle pools are eft_f.c (0x142CA0..), same module.
 *                                     Visual only, and entirely off in split-screen.
 *
 * Task classes (BtlTaskClass: update, init, term, postUpdate, reset, draw), from the data at 0x2C35D0..:
 *   0x2C35D0  layer 4            {EftBurstLayer_Update, EftBurstLayer_Init, EftBurstLayer_Term, 0, 0, 0}
 *   0x2C35E8  gEftBurstClass     {EftBurst_Update, EftBurst_Init, EftBurst_Term, EftBurst_PostUpdate,
 *                                 EftBurst_Reset, EftBurst_Draw}
 *   0x2C3600  steam manager      {EftSteamMgr_Update, EftSteamMgr_Init, EftSteamMgr_Term, 0, 0, 0}
 *   0x2C3618  gEftSteamClass     {EftSteam_Update, EftSteam_Init, EftSteam_Term, 0, EftSteam_Reset, EftSteam_Draw}
 *   0x2C3640  water              {EftWater_Update, EftWater_Init, EftWater_Term, EftWater_PostUpdate,
 *                                 EftWater_Reset, EftWater_Draw}
 * 0x2C3568 is the list of layer 0's sub-tasks, {class, id} pairs: the steam manager is id 5, the water task id 7.
 * 0x2C3630 (between the last two classes) is the constant vector (0, 0, 0, 1).
 */

/* Four floats, 8-byte aligned: locals and by-value arguments of this type are copied with ld/sd pairs. */
typedef struct EftEVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) EftEVec; /* size 0x10 */

/* ---- stage-change transition ------------------------------------------------------------------------- */

/* One particle of the transition ("EftBurstPtcl" in eft_d.h, which has the full layout). */
typedef struct EftTransPart {
    /* 0x00 */ u8 unk0[0x50];
    /* 0x50 */ s32 flags;                                 /* 0x10 (scale applies to x) set here on the two rings */
    /* 0x54 */ s32 (*update)(struct EftTransPart *part);  /* NULL = free slot; non-zero result = draw it */
    /* 0x58 */ void (*draw)(struct EftTransPart *part);
    /* 0x5C */ s32 unk5C;
    /* 0x60 */ s32 alpha;                                 /* 0x20 / 0x40 / 0x45 written here after the init */
    /* 0x64 */ s32 visible;                               /* result of update this frame */
    /* 0x68 */ f32 size;                                  /* peak scale: 70 / 50 for the two rings */
    /* 0x6C */ s32 unk6C;
} EftTransPart; /* size 0x70 */

#define EFT_TRANS_PART_COUNT 350

/* Work of the transition task: Heap_Alloc(0x9B70), gEftBurst (gp 0x2FE9C8; "EftBurstWork" in eft_d.h). */
typedef struct EftTransWork {
    /* 0x0000 */ s32 spawnWait;      /* frames until the next glow particle: (rand() & 3) + 5 */
    /* 0x0004 */ s32 frame;          /* 0..151, stops counting at 151 */
    /* 0x0008 */ f32 fadeDist;       /* starts at 8, +0.4 per frame up to 50 (the model's fade distance, eft_d.c) */
    /* 0x000C */ f32 unkC;           /* starts at 20, *0.995 per frame */
    /* 0x0010 */ f32 sway;           /* mean of the sines of 1..6 times the phase, -1..1 (added to a spark's scale) */
    /* 0x0014 */ s32 phase;          /* degrees, + (rand() & 7) + 3 per frame, wraps at 360 */
    /* 0x0018 */ s32 camArg[3];      /* {transition file entry 1, -1, -1}: camera animation given to EftCam_Start */
    /* 0x0024 */ u8 unk24[0x21C];
    /* 0x0240 */ EftTransPart part[EFT_TRANS_PART_COUNT];
    /* 0x9B60 */ s32 seWait;         /* frames until the next rumble: 60, then rand() % 80 + 60 */
    /* 0x9B64 */ u8 unk9B64[0xC];
} EftTransWork; /* size 0x9B70 */

/* Static state of the transition, gEftBurstRes (0x31BE60; "EftBurstRes" in eft_d.h). */
typedef struct EftTransRes {
    /* 0x00 */ void *list;      /* child list of the layer 4 task: one task, 4 bytes of work */
    /* 0x04 */ void *file3;     /* transition file entry 3 (texture pack), relocated in place */
    /* 0x08 */ void *file1;     /* transition file entry 1 (camera animation) */
    /* 0x0C */ void *file2;     /* transition file entry 2 (model) */
    /* 0x10 */ s32 startReq;    /* set by EftBurst_Start, consumed by EftBurstLayer_Update */
    /* 0x14 */ s32 endReq;      /* set by EftBurst_End: the task ends on its next update and is not drawn */
} EftTransRes; /* size 0x18 */

/* ---- steam emitters ---------------------------------------------------------------------------------- */

/* The 0x40-byte task of 0x1AD150.. as this module sees it. */
typedef struct EftSteamTask {
    /* 0x00 */ u8 flags;            /* bit 0: dead (set by BtlTask_SetDead; the list update then kills the task) */
    /* 0x01 */ u8 unk1[0x27];
    /* 0x28 */ BtlTaskClass *cls;
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;          /* workSize bytes given to BtlTask_CreateChildList */
    /* 0x3C */ s32 unk3C;
} EftSteamTask; /* size 0x40 */

#define EFT_TASK_IS_DEAD(task) ((u8)((task)->flags & 1))

/* One particle of an emitter. */
typedef struct EftSteamPart {
    /* 0x00 */ EftEVec pos;
    /* 0x10 */ EftEVec dir;     /* unit direction */
    /* 0x20 */ EftEVec color;   /* r, g, b, a as floats (0..255), copied from the emitter */
    /* 0x30 */ EftEVec vel;     /* dir * speed; y gets accel added every frame */
    /* 0x40 */ f32 speed;
    /* 0x44 */ f32 accel;
    /* 0x48 */ f32 size;
    /* 0x4C */ f32 angle;       /* roll of the quad, 0..2 pi */
    /* 0x50 */ s32 life;        /* frames left; 0 = free slot */
    /* 0x54 */ u8 unk54[0xC];
} EftSteamPart; /* size 0x60 */

#define EFT_STEAM_PART_COUNT 32

/* Texture of the emitters. */
typedef struct EftSteamTex {
    /* 0x00 */ u8 unk0[0x30];
    /* 0x30 */ u64 tex0;       /* GS TEX0, renewed every frame by EftVram_AddTex(&tex0, 1, 0) */
} EftSteamTex; /* size 0x38 */

/* Shared state of the emitters: BtlPool_Alloc(0x90), gEftSteam (gp 0x2FE9DC). */
typedef struct EftSteamShared {
    /* 0x00 */ void *list;       /* child list of the manager task: emitters with 0xC80 bytes of work */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ EftSteamTex tex;  /* built by EftTexSet_Load8 from stage pack entry 0x12 */
    /* 0x40 */ u8 unk40[0x50];
} EftSteamShared; /* size 0x90 */

/* Creation argument of an emitter (0x60 bytes, copied to the start of its work). */
typedef struct EftSteamArg {
    /* 0x00 */ EftEVec pos;
    /* 0x10 */ EftEVec dir;     /* (0, -1, 0, 1) for the stage's emitters: up. Not read by the code here */
    /* 0x20 */ EftEVec color;
    /* 0x30 */ f32 speed;
    /* 0x34 */ f32 accel;       /* added to a particle's vertical speed every frame */
    /* 0x38 */ f32 size;
    /* 0x3C */ s32 life;        /* frames */
    /* 0x40 */ f32 posRange;    /* random spread of the start position, sideways as seen from the camera */
    /* 0x44 */ f32 dirRange;    /* random spread of the direction */
    /* 0x48 */ f32 speedRange;
    /* 0x4C */ f32 sizeRange;
    /* 0x50 */ s32 lifeRange;
    /* 0x54 */ u8 unk54[0xC];
} EftSteamArg; /* size 0x60 */

/* Work of an emitter task. */
typedef struct EftSteamWork {
    /* 0x000 */ EftSteamArg arg;
    /* 0x060 */ EftSteamTex *tex;                 /* &gEftSteam->tex */
    /* 0x064 */ u8 unk64[0xC];
    /* 0x070 */ EftSteamPart part[EFT_STEAM_PART_COUNT];
    /* 0xC70 */ s32 flags;                        /* EFT_STEAM_* */
    /* 0xC74 */ s32 count;                        /* live particles */
    /* 0xC78 */ u8 unkC78[8];
} EftSteamWork; /* size 0xC80 */

#define EFT_STEAM_PAUSED 1   /* no new particles */
#define EFT_STEAM_STOP 2     /* no new particles, and the task ends once all have died */
#define EFT_STEAM_STARTED 4  /* set by the first update: one new particle per frame from the second on */

/* A record of the stage's third effect list (BtlStage_GetFxResC2; BtlStage_GetFxResC() of them). */
typedef struct EftStageMarker {
    /* 0x00 */ f32 pos[3];
    /* 0x0C */ f32 speed;
    /* 0x10 */ f32 accel;
    /* 0x14 */ f32 size;
    /* 0x18 */ u16 life;
    /* 0x1A */ u8 color[4];
    /* 0x1E */ u8 unk1E[0x12];
} EftStageMarker; /* size 0x30 */

/* ---- water effects ----------------------------------------------------------------------------------- */

/* Per-fighter wake state, gEftDust->wake[objId]. */
typedef struct EftWaterWake {
    /* 0x00 */ s32 objId;
    /* 0x04 */ u32 frame;   /* frames the wake has been on */
    /* 0x08 */ s32 on;
} EftWaterWake; /* size 0xC */

/* A splash: owner of the particles one impact created. */
typedef struct EftWaterSplash {
    /* 0x00 */ EftEVec pos;
    /* 0x10 */ s32 state;               /* 0 = free, 2 after creation */
    /* 0x14 */ void *dropList[2];       /* head / tail of its billboards (EftWaterDrop) */
    /* 0x1C */ void *sprayList[2];      /* head / tail of its streaks (EftWaterSpray) */
    /* 0x24 */ void *ringList[2];       /* head / tail of its surface rings (EftWaterRing) */
    /* 0x2C */ struct EftWaterSplash *prev;
    /* 0x30 */ struct EftWaterSplash *next;
    /* 0x34 */ u8 unk34[0xC];
} EftWaterSplash; /* size 0x40 */

#define EFT_WATER_SPLASH_COUNT 10

/* State of the water effects: BtlPool_Alloc(0x2C0), gEftDust (gp 0x2FF1A8). The name of the global comes from
   the first reading of eft_f.c ("ground dust"); gEftWater would fit. */
typedef struct EftWater {
    /* 0x00 */ EftWaterWake *wake;      /* 2 entries (0x18 bytes) */
    /* 0x04 */ EftWaterSplash *splash;  /* 10 entries (0x280 bytes) */
    /* 0x08 */ void *trailPool;         /* 15 * 0x24 = 0x21C bytes */
    /* 0x0C */ void *dropPool;          /* 60 * 0x70 = 0x1A40 bytes */
    /* 0x10 */ void *ringPool;          /* 30 * 0x50 = 0x960 bytes */
    /* 0x14 */ void *sprayPool;         /* 30 * 0x90 = 0x10E0 bytes */
    /* 0x18 */ void *mistPool;          /* 30 * 0xB0 = 0x14A0 bytes */
    /* 0x1C */ s32 splashCount;         /* +1 for every splash created (the list code of eft_f.c counts it down) */
    /* 0x20 */ u8 unk20[0x1C];
    /* 0x3C */ EftWaterSplash *splashList[2]; /* head / tail of the splashes in use */
    /* 0x44 */ void *trailList[2];      /* spray trails of techniques */
    /* 0x4C */ void *ringList[2];       /* rings that belong to no splash (wakes, bursts) */
    /* 0x54 */ void *dropList[2];       /* billboards that belong to no splash */
    /* 0x5C */ void *sprayList[2];      /* streaks that belong to no splash */
    /* 0x64 */ void *mistList[2];       /* swirls (EftWater_AddBurst only) */
    /* 0x6C */ u8 blastWait[2];         /* per character: splashes its technique records of kind 3 / 4 still skip */
    /* 0x6E */ u8 unk6E[2];
    /* 0x70 */ Mtx44 camMtx;            /* Mtx_InverseRT of the view matrix with the translation zeroed */
    /* 0xB0 */ u8 tex[0x210];           /* texture set built by EftTexSet_Load32 from stage pack entry 0x12 */
} EftWater; /* size 0x2C0 */

/* The task that owns a hit record ("EftHitTask" in eft_a.h). */
typedef struct EftWaterBlastTask {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 flags;    /* EFT_WATER_TASK_* */
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ EftEVec pos;  /* where it hit */
} EftWaterBlastTask;

#define EFT_WATER_TASK_HIT_CHAR 0x001 /* EFT_TASK_HIT_CHAR of eft_a.h: it hit a fighter; use the hit position */
#define EFT_WATER_TASK_HIT_4 0x004    /* EFT_TASK_HIT_STAGE of eft_a.h: no water effect */
#define EFT_WATER_TASK_DONE 0x400     /* set here once the hit position was used: no more water effects */

typedef struct EftWaterBlastSrc {
    /* 0x00 */ u8 unk0[0x24];
    /* 0x24 */ u8 *def;      /* +5: kind; 3 and 4 are rate limited */
} EftWaterBlastSrc;

/* Hit record view (0x190 bytes, "EftHitRec" in eft_a.h); only what the water code reads. */
typedef struct EftWaterBlast {
    /* 0x00 */ s32 objId;
    /* 0x04 */ s32 unk4[2];
    /* 0x0C */ s32 type;     /* 0: ki blast, 1: technique */
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ EftEVec pos;
    /* 0x30 */ EftEVec prevPos;
    /* 0x40 */ u8 unk40[0x10];
    /* 0x50 */ s32 flags;    /* 8: no water effect */
    /* 0x54 */ u8 unk54[0xC];
    /* 0x60 */ EftWaterBlastTask *task;
    /* 0x64 */ EftWaterBlastSrc *src;
    /* 0x68 */ u8 unk68[0x10];
    /* 0x78 */ f32 radius;   /* third word of the record's shape */
} EftWaterBlast;

/* EftWater_AddSplashFor kinds. */
#define EFT_WATER_SPLASH_CHAR 0  /* a fighter entering or leaving the water */
#define EFT_WATER_SPLASH_BLAST 1 /* a ki blast */
#define EFT_WATER_SPLASH_TECH 2  /* a technique */

extern EftTransRes gEftBurstRes;
extern EftTransWork *gEftBurst;
extern EftSteamShared *gEftSteam;
extern EftWater *gEftDust;
extern BtlTaskClass gEftBurstClass;
extern BtlTaskClass gEftSteamClass;

/* stage-change transition */
void EftBurstLayer_Init(void *task);
void EftBurstLayer_Term(void);
void EftBurstLayer_Update(void);
void EftBurst_Init(void);
void EftBurst_Term(void);
void EftBurst_Update(void *task);
void EftBurst_Draw(void);
void EftBurst_PostUpdate(void);
void EftBurst_Reset(void *task);
void EftBurst_Start(void);
s32 EftBurst_IsBusy(void);
void EftBurst_End(void);

s32 Eft_IsScreenPosVisible(s32 *pos);
s32 Eft_IsScreenPosInFront(s32 *pos);

/* steam emitters */
void *EftSteam_Create(EftSteamArg *arg);
void EftSteam_Stop(EftSteamTask *task);
void EftSteam_SetPaused(EftSteamTask *task, s32 paused);
EftSteamWork *EftSteam_GetWork(EftSteamTask *task);
void EftSteam_SetPos(EftSteamTask *task, Vec4 *pos);
void EftSteamMgr_Init(void *task);
void EftSteamMgr_Term(void);
void EftSteamMgr_Update(void);
void EftSteam_Init(EftSteamTask *task, EftSteamArg *arg);
void EftSteam_Term(void);
void EftSteam_Reset(void);
void EftSteam_Update(EftSteamTask *task);
void EftSteam_Draw(EftSteamTask *task);
EftSteamPart *EftSteam_FindFreePart(EftSteamPart *part);
void EftSteam_Emit(EftSteamWork *work);
void EftSteam_MoveParts(EftSteamWork *work);
void EftSteam_DrawParts(EftSteamWork *work, u64 tex0);
void EftSteam_DrawQuad(EftEVec *color, EftEVec *pos, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, s32 prio, u64 tex0,
                       f32 size, f32 angle);

/* water effects */
s32 EftWater_GetSurfaceY(f32 *y);
s32 EftWater_SetWake(s32 objId, u8 off);
s32 EftWater_AddBlastTrail(s32 objId, EftWaterBlast *rec);
s32 EftWater_AddSplashFor(s32 objId, EftEVec pos, u8 kind, f32 speed);
s32 EftWater_AddSplashAt(EftEVec pos, f32 scale);
s32 EftWater_AddBurst(s32 objId, s32 count, f32 delay);
void EftWater_UpdateBlast(EftWaterBlast *rec);
void EftWater_Init(void *task);
void EftWater_Term(void);
void EftWater_Update(void);
void EftWater_PostUpdate(void);
void EftWater_Reset(void);
void EftWater_Draw(void);
void EftWater_UpdateWakes(void);
EftWaterSplash *EftWater_AllocSplash(void);
s32 EftWaterSplash_IsEmpty(EftWaterSplash *splash);
void EftWater_AddSplash(EftWaterSplash **head, EftWaterSplash **tail, EftEVec pos, s32 sprayA, s32 sprayB, s32 dropA,
                        s32 dropB, f32 scale);

#endif
