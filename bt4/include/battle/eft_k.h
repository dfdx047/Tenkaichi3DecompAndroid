#ifndef BATTLE_EFT_K_H
#define BATTLE_EFT_K_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Views local to src/battle/eft_k.c (0x157398..0x15B550). See the comment at the top of that file.
 * Everything here is a partial view: other effect files describe the same blocks under their own names
 * (EftJTask / EftJDef / EftJSrc / EftJSet in eft_j.h, EftTask / EftTechDef / EftTechArg / EftModel in eft_l.h are
 * the same task, definition, shot slot and emitter set).
 */

/* A vector that is copied by assignment (two 64-bit moves): the original type was 16-byte aligned. */
typedef struct EftKVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(16))) EftKVec; /* size 0x10 */

/* A task of the effect scene's task tree (0x40 bytes); only what this file touches. */
typedef struct EftKTask {
    /* 0x00 */ u8 dead;      /* bit 0: set by BtlTask_SetDead, the list update then kills the task */
    /* 0x01 */ u8 state;     /* phase of the item's state machine: 0 started, 1 -> 2 fired / flying, 3 -> 4 ended */
    /* 0x02 */ u8 unk2[2];
    /* 0x04 */ u32 flags;    /* written by the hit pass for the task a hit record names (record +0x60): 1 hit,
                                2 and 4 the record ended, 0x200; cleared by the item when it is released. Also the
                                bits 0x800 / 0x1000 (task of character 0 / 1) that BtlScene_Reset(2 / 3) selects */
    /* 0x08 */ u16 hit;      /* bit 0: the hit pass moved the head (pos is valid); bit 2: stop counting */
    /* 0x0A */ u8 unkA[6];
    /* 0x10 */ Vec4 pos;     /* corrected head position, written by the scene's hit pass */
    /* 0x20 */ struct EftKTask **list; /* the list the task is in; its first word is the owner task */
    /* 0x24 */ s32 *unk24;   /* [1] tested by EftObjTechMgr_Update */
    /* 0x28 */ u8 unk28[0x10];
    /* 0x38 */ void *work;   /* the task's work block, allocated with the list */
    /* 0x3C */ s32 unk3C;
} EftKTask; /* size 0x40 */

/* Definition record of one technique effect. */
typedef struct EftKDef {
    /* 0x00 */ s16 id;        /* effect id: selects the special cases (0x162, 0x167, 0x168, 0x1F3, 0x282, 0x2A8, 0x2B4) */
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s8 kind;       /* 0: skill-like (events from the frame table, ends with BtlCharApi_IsInSkill);
                                 other: technique (events from animation attributes) */
    /* 0x05 */ s8 sub;        /* 5: rush technique (events are read from every fighter's animation) */
    /* 0x06 */ u8 unk6[2];
    /* 0x08 */ s8 shape;      /* hit shape: 0 two spheres, 1 two boxes */
    /* 0x09 */ u8 unk9[0xD];
    /* 0x16 */ s16 frames[6]; /* frame numbers of events 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000; -1 = none */
    /* 0x22 */ u8 unk22[6];
    /* 0x28 */ s32 life;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ f32 scale;
    /* 0x34 */ f32 speed;
    /* 0x38 */ f32 homing;
    /* 0x3C */ s32 flags;     /* 1 hits while flying, 2 does not move, 0x10, 0x400, 0x20000 has a model,
                                 0x40000 has rings */
    /* 0x40 */ u8 unk40[0xC];
    /* 0x4C */ f32 shotSpeed;
} EftKDef;

/* What a module's init callback receives: one technique effect slot of one fighter (0x50 bytes). */
typedef struct EftKSrc {
    /* 0x00 */ s32 objId;  /* object id of the fighter (0 or 1) */
    /* 0x04 */ s32 slot;   /* slot index */
    /* 0x08 */ u8 unk8[0x14];
    /* 0x1C */ s32 *pack;  /* the effect's resource pack */
    /* 0x20 */ s32 chr;    /* character index */
    /* 0x24 */ EftKDef *def;
} EftKSrc;

/* Emitter set of a module (built by EftEmit_LoadSet from the pack). */
typedef struct EftKPart {
    /* 0x00 */ u8 unk0[9];
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 unkA[0xE];
    /* 0x18 */ f32 unk18;
    /* 0x1C */ u8 unk1C[0x24];
} EftKPart; /* size 0x40 */

typedef struct EftKGroup {
    /* 0x00 */ u8 *info; /* [0] group id, [1] part count */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 base;  /* first part of the group */
    /* 0x09 */ u8 unk9[7];
} EftKGroup; /* size 0x10 */

typedef struct EftKSet {
    /* 0x00 */ u32 *mask; /* bit g: group g exists */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ EftKPart *parts;
    /* 0x0C */ EftKGroup grp[19];
} EftKSet;

/* A model object of an effect: the argument block of EftObj_Create (BtlObj_Create) and its result. */
typedef struct EftKModel {
    /* 0x00 */ s32 *pack;
    /* 0x04 */ u8 arg[0x30];
    /* 0x34 */ u8 flags;
    /* 0x35 */ s8 objId;  /* battle object id, -1 when none could be created */
    /* 0x36 */ s8 unk36;  /* -1 */
    /* 0x37 */ u8 unk37;
} EftKModel; /* size 0x38 */

/* The 0x40 bytes a hit record copies from its owner (record +0x10). */
typedef struct EftKShape {
    /* 0x00 */ EftKVec unk0;
    /* 0x10 */ EftKVec pos;   /* head */
    /* 0x20 */ EftKVec prev;  /* tail / previous head */
    /* 0x30 */ EftKVec vel;
} EftKShape; /* size 0x40 */

/* A hit record of the effect scene (0x190 bytes, EftHit_GetNew); only what this file writes. */
typedef struct EftKHitRec {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ EftKShape shape;
    /* 0x50 */ s32 flags;     /* 0x10 fixed on the target, 0x800 */
    /* 0x54 */ u8 unk54[0xC];
    /* 0x60 */ EftKTask *task; /* the task the hit pass reports to */
    /* 0x64 */ EftKSrc *src;
} EftKHitRec;

/* ---- module "shots" (classes 0x2C38F8 manager / 0x2C3910 item), effect type 2 ---- */

typedef struct EftShotTechMgr {
    /* 0x000 */ EftKSet set;
    /* 0x13C */ u8 unk13C[0x1DC];
    /* 0x318 */ s32 *unk318[2];
    /* 0x320 */ s32 *packs[3]; /* entries 1..3 of the character's pack 8; indexed by slot from 0x318 */
    /* 0x32C */ s32 *modelPack; /* entry 1 of the effect's pack when the definition has flag 0x20000 */
} EftShotTechMgr; /* size 0x330 */

typedef struct EftShotTech {
    /* 0x000 */ EftShotTechMgr *mgr;
    /* 0x004 */ u8 emit[0x2CC];   /* emitter state (EftEmit_InitState) */
    /* 0x2D0 */ Vec4 nodes[37];   /* node positions (EftEmit_UpdateNodesReq) */
    /* 0x520 */ Vec4 dir;
    /* 0x530 */ Vec4 unk530[2];
    /* 0x550 */ Vec4 start;       /* position of the launch node */
    /* 0x560 */ Vec4 muzzleDir;
    /* 0x570 */ Vec4 muzzlePos;
    /* 0x580 */ s32 flags;
    /* 0x584 */ s32 node;
    /* 0x588 */ s32 phase;
    /* 0x58C */ u8 unk58C[8];
    /* 0x594 */ f32 speed;
    /* 0x598 */ f32 scale;
    /* 0x59C */ u8 unk59C[0x14];
    /* 0x5B0 */ Vec4 pos;         /* target point */
    /* 0x5C0 */ Vec4 prev;
    /* 0x5D0 */ Vec4 vel;
    /* 0x5E0 */ EftKSrc *src;
    /* 0x5E4 */ u8 unk5E4[0xC];
    /* 0x5F0 */ u8 shots[14][0x30];
    /* 0x890 */ s32 shotCount;
    /* 0x894 */ s32 *camPack;
    /* 0x898 */ void *subs[8];
    /* 0x8B8 */ s32 unk8B8;
    /* 0x8BC */ void *auraFx;
    /* 0x8C0 */ EftKModel model;
    /* 0x8F8 */ u8 unk8F8[8];
} EftShotTech; /* size 0x900 */

/* Argument of EftCam_Start, passed by value. */
typedef struct EftKCamArg {
    /* 0x00 */ s32 *pack;
    /* 0x04 */ s32 objId;
    /* 0x08 */ s32 id;
} EftKCamArg;

/* ---- technique events (gEftTechEvt, classes 0x2C3928 layer / 0x2C3940 timeline) ---- */

/* Event bits of a fighter, valid for one frame (EftTechEvt_GetEvents). */
#define EFT_TECH_EVT_START 0x002   /* animation attribute 0x200 or frames[0] */
#define EFT_TECH_EVT_FIRE 0x004    /* 0x400 / frames[1] */
#define EFT_TECH_EVT_END 0x008     /* 0x800 / frames[2], or the end timer ran out */
#define EFT_TECH_EVT_10 0x010      /* 0x1000 / frames[3] */
#define EFT_TECH_EVT_20 0x020      /* 0x2000 / frames[4] */
#define EFT_TECH_EVT_40 0x040      /* 0x4000 / frames[5] */
#define EFT_TECH_EVT_80 0x080      /* animation attribute 0x200000 */
#define EFT_TECH_EVT_100 0x100     /* 0x400000 */
#define EFT_TECH_EVT_200 0x200     /* 0x800000 */
#define EFT_TECH_EVT_ABORT 0x400   /* the fighter left the technique */

/* Requests to a fighter's timeline. */
#define EFT_TECH_REQ_STOP 2    /* the timers stop for good */
#define EFT_TECH_REQ_RESTART 4 /* the end timer starts again */
#define EFT_TECH_REQ_EXPIRE 8  /* the end timer is set to 0 */

typedef struct EftTechEvtEntry {
    /* 0x00 */ s32 events;   /* EFT_TECH_EVT_*, cleared every frame */
    /* 0x04 */ s32 requests; /* EFT_TECH_REQ_* */
    /* 0x08 */ s32 unk8[2];
} EftTechEvtEntry; /* size 0x10 */

typedef struct EftTechEvt {
    /* 0x00 */ void *list;   /* child list of the layer: one timeline task per technique in progress */
    /* 0x04 */ EftTechEvtEntry *entries; /* one per fighter, indexed by object id */
    /* 0x08 */ s32 count;    /* BtlScene_GetCharCount() */
} EftTechEvt; /* size 0xC */

/* Argument of the timeline task's init, and the first 0x10 bytes of its work. */
typedef struct EftTechEvtArg {
    /* 0x00 */ EftKSrc *src;
    /* 0x04 */ s32 fireTimer; /* frames until held flag 0xA7 ("fire") is set, counted from event START */
    /* 0x08 */ s32 endTimer;  /* frames until event END, counted from event FIRE */
    /* 0x0C */ s32 endTime;   /* reload value of endTimer */
} EftTechEvtArg;

typedef struct EftTechEvtTask {
    /* 0x00 */ EftTechEvtArg arg;
    /* 0x10 */ s32 flags;     /* 2 fireTimer runs, 4 endTimer runs, 8 the rush connected, 0x10 timers stepped
                                 this frame, 0x20 fired (kind 0) */
    /* 0x14 */ s32 frame;     /* frames since the task started (rush: since it connected) */
    /* 0x18 */ s32 useFrames; /* 1: events come from the definition's frame table */
} EftTechEvtTask; /* size 0x1C */

/* ---- module "thrown object" (classes 0x2C3958 manager / 0x2C3970 item), effect type 8 ---- */

typedef struct EftObjTechMgr {
    /* 0x000 */ EftKSet set;
    /* 0x13C */ u8 unk13C[0x1E4];
    /* 0x320 */ s32 *modelPack; /* entry 1 of the effect's pack when the definition has flag 0x20000 */
    /* 0x324 */ s32 *ringModel; /* with flag 0x40000: entries 1 and 2 of entry 1 */
    /* 0x328 */ s32 *ringTex;
    /* 0x32C */ s32 unk32C;
    /* 0x330 */ u8 ringProto[0x90]; /* EftMesh_Init */
    /* 0x3C0 */ u8 ringTexSet[0x210]; /* EftTexSet_Load32 */
} EftObjTechMgr; /* size 0x5D0 */

typedef struct EftKRings {
    /* 0x000 */ u8 ring[3][0x90];
    /* 0x1B0 */ f32 roll[3];
    /* 0x1BC */ f32 offset[3]; /* distance along the direction */
    /* 0x1C8 */ f32 scale[3];
    /* 0x1D4 */ s32 count;
} EftKRings; /* size 0x1D8 */

typedef struct EftObjTech {
    /* 0x000 */ Vec4 dir;
    /* 0x010 */ EftKShape body;   /* the flying object: unk0, head 0x20, tail 0x30, velocity 0x40 */
    /* 0x050 */ EftKShape held;   /* while held / on the target: start 0x50, position 0x60, previous 0x70,
                                     velocity 0x80 */
    /* 0x090 */ EftKSrc *src;
    /* 0x094 */ u8 emit[0x2CC];   /* emitter state; +0x2A8 width */
    /* 0x360 */ Vec4 nodes[2];
    /* 0x380 */ Vec4 unk380;
    /* 0x390 */ Vec4 unk390[3];
    /* 0x3C0 */ Vec4 unk3C0;
    /* 0x3D0 */ u8 unk3D0[0x60];
    /* 0x430 */ s32 node;
    /* 0x434 */ u8 unk434[0x17C];
    /* 0x5B0 */ EftObjTechMgr *mgr;
    /* 0x5B4 */ s32 flags;
    /* 0x5B8 */ s32 mode;
    /* 0x5BC */ f32 speed;
    /* 0x5C0 */ f32 unk5C0;
    /* 0x5C4 */ f32 scale;
    /* 0x5C8 */ f32 hitScale;
    /* 0x5CC */ f32 unk5CC;
    /* 0x5D0 */ f32 timer;
    /* 0x5D4 */ f32 life;
    /* 0x5D8 */ s32 unk5D8;
    /* 0x5DC */ EftKModel model;
    /* 0x614 */ u8 unk614[0xC];
    /* 0x620 */ EftKRings rings;
    /* 0x7F8 */ u8 unk7F8[8];
} EftObjTech; /* size 0x800 */

/* ---- module "rush shot" (class 0x2C39A0; the rest of it is src/battle/eft_l.c) ---- */

typedef struct EftRushShotMgr {
    /* 0x000 */ u8 unk0[0x320];
    /* 0x320 */ s32 *modelPack;
} EftRushShotMgr;

/* One model of the swarm (0xB0 bytes). */
typedef struct EftRushShotObj {
    /* 0x00 */ EftKModel model;  /* flags: 1 placed, 2 released */
    /* 0x38 */ f32 unk38;
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ f32 delay;        /* frames before it follows the shot */
    /* 0x44 */ f32 scale;
    /* 0x48 */ f32 bobAmp;
    /* 0x4C */ f32 bobPhase;
    /* 0x50 */ f32 bobSpeed;
    /* 0x54 */ f32 sink;         /* subtracted from offset.y every frame */
    /* 0x58 */ f32 unk58;
    /* 0x5C */ f32 unk5C;
    /* 0x60 */ Vec4 center;
    /* 0x70 */ Vec4 offset;
    /* 0x80 */ Vec4 drift;
    /* 0x90 */ Vec4 rot;
    /* 0xA0 */ Vec4 rotSpeed;
} EftRushShotObj; /* size 0xB0 */

typedef struct EftRushShot {
    /* 0x000 */ Vec4 dir;
    /* 0x010 */ Vec4 unk10;
    /* 0x020 */ EftKShape body;  /* head 0x30, tail 0x40, velocity 0x50 */
    /* 0x060 */ EftKSrc *src;
    /* 0x064 */ u8 emit[0x2CC];
    /* 0x330 */ Vec4 unk330[2];
    /* 0x350 */ Vec4 unk350;
    /* 0x360 */ Vec4 unk360[3];
    /* 0x390 */ Vec4 unk390;
    /* 0x3A0 */ u8 unk3A0[0x1E4];
    /* 0x584 */ s32 flags;
    /* 0x588 */ s32 mode;
    /* 0x58C */ u8 unk58C[8];
    /* 0x594 */ f32 scale;
    /* 0x598 */ f32 hitScale;
    /* 0x59C */ u8 unk59C[0x34];
    /* 0x5D0 */ s32 count;
    /* 0x5D4 */ u8 unk5D4[0xC];
    /* 0x5E0 */ EftRushShotObj objs[7];
} EftRushShot; /* size 0xAC0 (0x5E0 + 7 * 0xB0 = 0xAB0, then 0x10 more) */

extern EftTechEvt *gEftTechEvt;

void EftShotTech_UpdateAuraBall(EftKTask *task);
void EftShotTech_UpdateTargetBurst(EftKTask *task);
void EftShotTech_UpdateMuzzle(EftKTask *task);
void EftShotTech_StartCam(EftKTask *task);
void EftShotTech_MoveStraight(EftKTask *task, s32 toTarget);
void EftShotTech_MoveFromTarget(EftKTask *task, s32 wait);
void EftShotTech_MoveAimed(EftKTask *task);
void EftShotTech_UpdateConnected(EftKTask *task);
void EftShotTech_Init(EftKTask *task, EftKSrc *src);
void EftShotTech_Term(EftKTask *task);
void EftShotTech_Update(EftKTask *task);
void EftShotTech_PostUpdate(EftKTask *task);
void EftShotTech_Reset(EftKTask *task);
void EftShotTech_Draw(EftKTask *task);
void EftShotTechMgr_Init(EftKTask *task, EftKSrc *src);
void EftShotTechMgr_Term(EftKTask *task);
void EftShotTechMgr_Update(EftKTask *task);
void EftShotTechMgr_Reset(EftKTask *task);

s32 EftTechEvt_GetEvents(s32 objId);
void *EftTechEvt_Start(EftKSrc *src, s32 flagTime, s32 endTime);
void EftTechEvt_RequestStop(s32 objId);
void EftTechEvt_RequestRestart(s32 objId);
void EftTechEvt_RequestExpire(s32 objId);
void EftTechEvt_Init(EftKTask *task);
void EftTechEvt_Term(void);
void EftTechEvt_Update(void);
void EftTechEvtTask_Init(EftKTask *task, EftTechEvtArg *arg);
void EftTechEvtTask_Term(EftKTask *task);
s32 EftTechEvtTask_Update(EftKTask *task);
void EftTechEvtTask_Reset(EftKTask *task);
void EftTechEvtTask_Draw(EftKTask *task);
void EftTechEvtTask_UpdateNormal(EftKTask *task);
void EftTechEvtTask_UpdateRush(EftKTask *task);
s32 EftTechEvt_IsRushTech(EftKSrc *src);
u64 EftTechEvtTask_CollectEvents(EftTechEvtTask *w);
s32 EftTechEvt_IsInterrupted(s32 objId);
s32 EftTechEvt_IsInSkill(s32 objId);
s32 EftTechEvt_IsInTechnique(s32 objId);

void EftObjTech_AddHitRecord(EftKTask *task);
void EftObjTech_CreateModel(EftKTask *task);
void EftObjTech_SetModelPose(EftKTask *task, Vec4 *pos, Vec4 *angles);
void EftObjTech_DestroyModel(EftKTask *task);
void EftObjTech_InitRings(EftKTask *task, f32 roll, f32 step, f32 grow);
void EftObjTech_SetRingParams(EftKTask *task, f32 roll, f32 step, f32 grow);
void EftObjTech_PlaceRings(EftKTask *task, Vec4 *pos, Vec4 *angles, f32 width, f32 length);
void EftObjTech_DrawRings(EftKTask *task);
void EftObjTech_TermRings(EftKTask *task);
void EftObjTech_UpdateEmitters(s32 objId, EftKTask *task, EftKSet *set);
void EftObjTech_UpdateMotion(EftKTask *task);
void EftObjTech_UpdateModelVisible(EftKTask *task);
void EftObjTech_Init(EftKTask *task, EftKSrc *src);
void EftObjTech_Term(EftKTask *task);
void EftObjTech_Update(EftKTask *task);
void EftObjTech_PostUpdate(EftKTask *task);
void EftObjTech_Reset(EftKTask *task);
void EftObjTech_Draw(EftKTask *task);
void EftObjTechMgr_Init(EftKTask *task, EftKSrc *src);
void EftObjTechMgr_Term(EftKTask *task);
void EftObjTechMgr_Update(EftKTask *task);
void EftObjTechMgr_Reset(EftKTask *task);

void EftRushShot_AddHitRecord(EftKTask *task);
void EftRushShot_CreateModels(EftKTask *task);
void EftRushShot_UpdateModels(EftKTask *task);

#endif
