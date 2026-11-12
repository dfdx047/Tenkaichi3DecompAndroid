#ifndef BATTLE_EFT_R_H
#define BATTLE_EFT_R_H

#include "types.h"

/*
 * src/battle/eft_r.c, 0x174A70..0x178AB0. Eight pieces of effect code, in address order:
 *
 *   0x174A70            EftCharaFx_Stop     last function of the module before this file (fighter "chara fx")
 *   0x174AB8..0x175660  EftStruggle_*       beam struggle (clash A) between two technique hit records. SIMULATION:
 *                                           moves both beams' heads, ends the loser's technique.
 *   0x175660..0x175CA0  EftClashSpark_*     the spark burst at the struggle point. Visual.
 *   0x175CA0..0x1763E8  EftCharge_*         fighter effect requests 7 / 8: charge aura and its burst (screen
 *                                           shock wave). Visual.
 *   0x1763E8..0x176F10  EftKiBlast_*        THE KI BLAST (ki blast types 0, 1 and anything but 2..5). SIMULATION.
 *   0x176F10..0x177548  EftBlastCharge_*    glow on the hand while a ki blast is charged (request 0x17). Visual.
 *   0x177548..0x178530  EftKiBomb_*         ki blast type 3: a thrown model that falls, bounces off the stage and
 *                                           explodes. SIMULATION.
 *   0x178530..0x178AB0  EftKiObj_*          ki blast type 2: a thrown model (first half; the module continues in
 *                                           eft_s.c with its motion, post-update and manager). SIMULATION.
 *
 * Every structure is a view local to this file. The same things have other local names in the neighbouring
 * headers (EftRSet / EftRState are eft_i.h's EftEmitSet / EftEmitState, EftRRec is eft_a.h's EftHitRec, EftRArg is
 * btl_char_fx.h's FxHitArg2 and eft_a.h's EftHitAtk).
 */

/* 16-byte aligned vector (copied with 64-bit moves). */
typedef union EftRVec {
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftRVec;

typedef struct EftRMtx {
    /* 0x00 */ EftRVec row[4];
} EftRMtx; /* size 0x40 */

struct EftRTask;

/* A task class: the six callbacks of the tables at 0x2C3CE0..0x2C3E18. */
typedef struct EftRClass {
    /* 0x00 */ void (*update)(struct EftRTask *task);
    /* 0x04 */ void *init;
    /* 0x08 */ void *term;
    /* 0x0C */ void *postUpdate;
    /* 0x10 */ void *reset;
    /* 0x14 */ void *draw;
} EftRClass; /* size 0x18 */

/* EftRTask.hit: what the hit pass (0x1AFDB0..0x1B10F0) reported for the task's record. */
#define EFT_R_HIT_CHAR    0x01 /* hit the opponent */
#define EFT_R_HIT_GUARD   0x02 /* guarded */
#define EFT_R_HIT_STAGE   0x04 /* hit the stage (EftHit_NotifyBlastTask hands over the contact) */
#define EFT_R_HIT_CLASH   0x08 /* lost a clash with another record */
#define EFT_R_HIT_OUT     0x10 /* left the stage (EftHit_ClampToStage) */
#define EFT_R_HIT_ABSORB  0x20 /* absorbed (BtlColl_TryAbsorb) */
#define EFT_R_HIT_DEFLECT 0x40 /* knocked away (BtlColl_TryDeflect) */
#define EFT_R_HIT_REFLECT 0x80 /* sent back (BtlColl_TryReflect) */
#define EFT_R_HIT_END     0x3F
#define EFT_R_HIT_TURN    0xC0

/* A task of the effect scene (0x40 bytes); only what this file touches. */
typedef struct EftRTask {
    /* 0x00 */ u8 flags;     /* bit 0: killed */
    /* 0x01 */ u8 unk1[3];
    /* 0x04 */ s32 hit;      /* EFT_R_HIT_*, written by the hit pass through EftHit_SetTaskFlag */
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ EftRVec pos;  /* where the record hit, written with the bits */
    /* 0x20 */ s32 unk20[2];
    /* 0x28 */ EftRClass *cls;
    /* 0x2C */ s32 unk2C[3];
    /* 0x38 */ void *work;
    /* 0x3C */ s32 unk3C;
} EftRTask; /* size 0x40 */

typedef struct EftRList {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ EftRTask *first;
} EftRList;

/* ---- emitter set (eft_h.c / eft_i.c EftEmit_*) ---- */

typedef struct EftRPartDef {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 count;      /* emitters in the group */
} EftRPartDef;

typedef struct EftRPart {
    /* 0x0 */ EftRPartDef *def;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ u8 first;      /* index of the group's first emitter in the state arrays */
    /* 0x9 */ u8 unk9[7];
} EftRPart; /* size 0x10 */

typedef struct EftRSet {
    /* 0x000 */ u32 *mask;   /* bit n: group n exists */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ s32 unk8;
    /* 0x00C */ EftRPart part[0x13];
    /* 0x13C */ u8 unk13C[0x320 - 0x13C];
} EftRSet; /* size 0x320 */

/* What a manager task allocates: the set and its pack. */
typedef struct EftRSetPack {
    /* 0x000 */ EftRSet set;
    /* 0x320 */ s32 *pack;
} EftRSetPack; /* size 0x324 */

typedef struct EftRState {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ u8 flags[0x28];   /* per emitter */
    /* 0x168 */ u8 unk168[0x2CC - 0x168];
} EftRState; /* size 0x2CC */

typedef struct EftRNodes {
    /* 0x000 */ u8 unk0[0x250];
} EftRNodes; /* size 0x250 */

/* Work of a manager task: one pointer. */
typedef struct EftRMgr {
    /* 0x0 */ EftRSetPack *set;
} EftRMgr;

/* ---- hit record (eft_a.h EftHitRec) ---- */

typedef struct EftRDef {
    /* 0x00 */ u8 unk0[0x30];
    /* 0x30 */ f32 unk30;    /* length scale of a beam in a struggle */
    /* 0x34 */ u8 unk34[0x54];
    /* 0x88 */ f32 power;    /* struggle power */
} EftRDef;

typedef struct EftRSrc {
    /* 0x00 */ u8 unk0[0x24];
    /* 0x24 */ EftRDef *def;
} EftRSrc;

typedef struct EftRSphere {
    /* 0x00 */ EftRVec pos;
    /* 0x10 */ f32 radius;
} EftRSphere;

typedef struct EftRBox {
    /* 0x00 */ EftRVec unk0;
    /* 0x10 */ EftRVec pos;
    /* 0x20 */ f32 radius;
} EftRBox;

/* The four vectors a record carries (record +0x10), copied whole. */
typedef struct EftRPose {
    /* 0x00 */ EftRVec start; /* where it was launched */
    /* 0x10 */ EftRVec pos;
    /* 0x20 */ EftRVec prev;
    /* 0x30 */ EftRVec dir;   /* unit direction (not multiplied by the speed) */
} EftRPose; /* size 0x40 */

typedef struct EftRRec {
    /* 0x000 */ s32 objId;
    /* 0x004 */ s32 unk4[3];
    /* 0x010 */ EftRPose pose;
    /* 0x050 */ s32 flags;
    /* 0x054 */ s32 unk54;    /* EftHit_Add overwrites it: 0 for a ki blast */
    /* 0x058 */ s32 unk58;
    /* 0x05C */ u8 unk5C;
    /* 0x05D */ u8 unk5D[3];
    /* 0x060 */ EftRTask *task;
    /* 0x064 */ EftRSrc *src;
    /* 0x068 */ void *atk;    /* EftRArg of a ki blast */
    /* 0x06C */ s32 unk6C;
    /* 0x070 */ s32 shapeType;
    /* 0x074 */ f32 unk74[2];
    /* 0x07C */ void *shapeA;
    /* 0x080 */ void *shapeB;
    /* 0x084 */ u8 unk84[0x190 - 0x84];
} EftRRec; /* size 0x190 */

/* ---- beam struggle ---- */

typedef struct EftStruggleSide {
    /* 0x0 */ s32 objId;
    /* 0x4 */ f32 power;     /* definition +0x88 of the side's technique */
} EftStruggleSide;

/* gEftStruggle, 0x20 bytes from the pool. */
typedef struct EftStruggleMgr {
    /* 0x00 */ EftRList *list;        /* child list: one task of 0x380 bytes */
    /* 0x04 */ EftStruggleSide side[2];
    /* 0x14 */ f32 camDist;           /* 20 */
    /* 0x18 */ f32 midDist;           /* 40 */
    /* 0x1C */ f32 maxPower;          /* the larger of the two powers */
} EftStruggleMgr; /* size 0x20 */

/* Argument of the struggle task. */
typedef struct EftStruggleArg {
    /* 0x000 */ EftRVec pos;          /* where the records met */
    /* 0x010 */ f32 ratio;            /* (record 0's travel) / (distance between the two start points) */
    /* 0x014 */ s32 unk14[3];
    /* 0x020 */ EftRRec rec[2];       /* copies of the two records; [0] is object 0's */
} EftStruggleArg; /* size 0x340 */

#define EFT_STRUGGLE_END 0x1

typedef struct EftStruggle {
    /* 0x000 */ EftRVec pos;          /* the struggle point */
    /* 0x010 */ EftRVec unk10;
    /* 0x020 */ f32 bias;             /* 0..1 along the line from record 1's start to record 0's start */
    /* 0x024 */ s32 frames;
    /* 0x028 */ s32 flags;            /* EFT_STRUGGLE_END */
    /* 0x02C */ s32 unk2C;
    /* 0x030 */ EftStruggleArg arg;
    /* 0x370 */ EftRTask *spark;
    /* 0x374 */ s32 unk374[3];
} EftStruggle; /* size 0x380 */

/* ---- clash spark ---- */

typedef struct EftClashSparkArg {
    /* 0x00 */ EftRVec pos;
    /* 0x10 */ EftRVec dir;
    /* 0x20 */ s32 kind;              /* index of the set (always 0) */
    /* 0x24 */ f32 scale;
    /* 0x28 */ s32 objId;
    /* 0x2C */ s32 unk2C;
} EftClashSparkArg; /* size 0x30 */

#define EFT_CSPARK_ACTIVE 0x01
#define EFT_CSPARK_END    0x02
#define EFT_CSPARK_KILL   0x04
#define EFT_CSPARK_FAST   0x08
#define EFT_CSPARK_RESET  0x10
#define EFT_CSPARK_POSTED 0x20

typedef struct EftClashSpark {
    /* 0x000 */ EftRVec dir;
    /* 0x010 */ EftRVec pos;
    /* 0x020 */ EftRSet *set;
    /* 0x024 */ EftRState state;
    /* 0x2F0 */ EftRNodes nodes;
    /* 0x540 */ u32 flags;            /* EFT_CSPARK_* */
    /* 0x544 */ u8 mask;              /* node slots in use / phase mask */
    /* 0x545 */ u8 kind;
    /* 0x546 */ u8 objId;
    /* 0x547 */ u8 unk547;
    /* 0x548 */ f32 scale;
    /* 0x54C */ f32 unk54C;
    /* 0x550 */ f32 timer;
    /* 0x554 */ f32 life;
    /* 0x558 */ s32 unk558[2];
} EftClashSpark; /* size 0x560 */

/* ---- charge aura ---- */

#define EFT_CHARGE_ACTIVE 0x01
#define EFT_CHARGE_END    0x02
#define EFT_CHARGE_KILL   0x04
#define EFT_CHARGE_BURST  0x08  /* request 8 */
#define EFT_CHARGE_BURST_DONE 0x10
#define EFT_CHARGE_ALIVE  0x20
#define EFT_CHARGE_POSTED 0x40
#define EFT_CHARGE_RESET  0x80

typedef struct EftCharge {
    /* 0x000 */ EftRVec pos;          /* node 3 */
    /* 0x010 */ s32 objId;
    /* 0x014 */ EftRSet *set;
    /* 0x018 */ EftRState state;
    /* 0x2E4 */ s32 unk2E4[3];
    /* 0x2F0 */ EftRNodes nodes;
    /* 0x540 */ u32 flags;            /* EFT_CHARGE_* */
    /* 0x544 */ f32 scale;            /* height / 19.35, at least 0.5 */
    /* 0x548 */ u32 mask;             /* phase mask: 1 aura, 2 burst */
    /* 0x54C */ f32 timer;
    /* 0x550 */ f32 life;
    /* 0x554 */ s32 unk554[3];
} EftCharge; /* size 0x560 */

/* ---- ki blast ---- */

/* Argument of EftKiBlast_Fire / EftKiBomb_Fire / the type 2 launcher, built by BtlFx_FireKiBlast (which, in
   spite of its name, is the ki blast launcher). A record's atk pointer (+0x68) points at the task's copy. */
typedef struct EftRArg {
    /* 0x00 */ EftRVec dir;           /* launch direction */
    /* 0x10 */ s16 srcId;             /* who fired it; never changes. BtlMove_CanFireBlast counts live blasts by it */
    /* 0x12 */ s16 objId;             /* owner (the record's objId: the fighter it cannot hit); flips when the
                                         blast is deflected or reflected */
    /* 0x14 */ s16 node;              /* model node it leaves from */
    /* 0x16 */ s16 unk16;
    /* 0x18 */ s16 kind;              /* 0..12: BtlKiBlast_GetCurrentKind (0, 4, 8: blast class 0, else class 1) */
    /* 0x1A */ u8 type;               /* ki blast record type */
    /* 0x1B */ u8 unk1B;
    /* 0x1C */ s16 life;              /* frames */
    /* 0x1E */ s16 unk1E;
    /* 0x20 */ f32 speed;             /* units per frame (EftHit_Add copies it to the record's shape +8) */
    /* 0x24 */ f32 turn;              /* homing turn per frame */
    /* 0x28 */ f32 radius;            /* ki blast record +0x28 */
    /* 0x2C */ f32 unk2C;             /* ki blast record +0x2C */
    /* 0x30 */ s32 level;             /* 0..3: charge level; the record's clash level (the higher blast wins) */
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 canHit;            /* 1; 0 would keep the blast from publishing hit records */
    /* 0x3C */ s32 unk3C;             /* bit 0x20 of the ki blast record's flags */
    /* 0x40 */ u8 unk40[0x10];
} EftRArg; /* size 0x50 */

#define EFT_KIBLAST_STARTED   0x0001 /* the first update is over (the blast does not move on its first frame) */
#define EFT_KIBLAST_MOVED     0x0002 /* prev is valid */
#define EFT_KIBLAST_HOMING    0x0004 /* the owner was locked on at launch */
#define EFT_KIBLAST_DEFLECTED 0x0008 /* deflected once: no homing, later deflections are ignored */
#define EFT_KIBLAST_ENDING    0x0010 /* no more hit records; the end animation runs */
#define EFT_KIBLAST_ENDED     0x0020
#define EFT_KIBLAST_KILL      0x0040
#define EFT_KIBLAST_TYPED     0x0080 /* argument type != 0 (not read in this file) */
#define EFT_KIBLAST_STOPPED   0x0100 /* hit something: stays where it hit */
#define EFT_KIBLAST_TURNED    0x0200 /* deflected or reflected at least once (record +0x5C) */
#define EFT_KIBLAST_WIDTH2    0x0800 /* the set header has flag 4: the speed was replaced (by 0, see EftKiBlast_Init) */
#define EFT_KIBLAST_RESET     0x1000

typedef struct EftKiBlast {
    /* 0x000 */ EftRPose pose;
    /* 0x040 */ EftRArg arg;
    /* 0x090 */ EftRSet *set;
    /* 0x094 */ EftRState state;
    /* 0x360 */ EftRNodes nodes;
    /* 0x5B0 */ s32 flags;            /* EFT_KIBLAST_* */
    /* 0x5B4 */ s32 life;             /* frames left */
    /* 0x5B8 */ s32 unk5B8;           /* arg.unk1B */
    /* 0x5BC */ f32 radius;
    /* 0x5C0 */ f32 unk5C0;           /* arg.unk2C */
    /* 0x5C4 */ u32 mask;             /* phase mask */
    /* 0x5C8 */ f32 endTimer;
    /* 0x5CC */ f32 endFrames;
    /* 0x5D0 */ s32 endCount;
    /* 0x5D4 */ s32 unk5D4[3];
} EftKiBlast; /* size 0x5E0 */

/* ---- ki blast charge glow ---- */

/* Argument of EftBlastCharge_Start: btl_char_fx.h's FxHitArg. */
typedef struct EftBlastChargeArg {
    /* 0x00 */ EftRVec pos;
    /* 0x10 */ s32 objId;
    /* 0x14 */ s32 node;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
} EftBlastChargeArg; /* size 0x20 */

#define EFT_BCHARGE_ACTIVE 0x1
#define EFT_BCHARGE_END    0x2
#define EFT_BCHARGE_KILL   0x4
#define EFT_BCHARGE_POSTED 0x8

typedef struct EftBlastCharge {
    /* 0x000 */ EftRVec pos;
    /* 0x010 */ EftRVec dir;
    /* 0x020 */ EftBlastChargeArg arg;
    /* 0x040 */ EftRSet *set;
    /* 0x044 */ EftRState state;
    /* 0x310 */ EftRNodes nodes;
    /* 0x560 */ u32 flags;            /* EFT_BCHARGE_* */
    /* 0x564 */ s32 unk564;           /* arg.unk18 */
    /* 0x568 */ f32 scale;
    /* 0x56C */ u32 mask;
    /* 0x570 */ f32 unk570;
    /* 0x574 */ f32 life;
    /* 0x578 */ s32 unk578[2];
} EftBlastCharge; /* size 0x580 */

/* ---- ki bomb (type 3) and thrown object (type 2) ---- */

#define EFT_KIBOMB_MOVED     0x002 /* prev is valid */
#define EFT_KIBOMB_DEFLECTED 0x004
#define EFT_KIBOMB_KILL      0x008
#define EFT_KIBOMB_UNK20     0x020 /* record +0x5C */
#define EFT_KIBOMB_SHOWN     0x040 /* the model is drawn */
#define EFT_KIBOMB_HITS      0x080 /* publishes a hit record */
#define EFT_KIBOMB_EXPLODE   0x100 /* explode on the next move */
#define EFT_KIBOMB_REST      0x200 /* came to rest on the ground after three bounces */
#define EFT_KIBOMB_EXPLODED  0x400 /* the explosion is running */

/* Work of the manager task of the two model blasts. */
typedef struct EftKiBombRes {
    /* 0x00 */ u8 unk0[0x50];
    /* 0x50 */ void *model;           /* entry 1 of the pack */
    /* 0x54 */ s32 *pack;             /* entry 4 of the character's pack */
} EftKiBombRes; /* size 0x58 */

typedef struct EftKiBombMgr {
    /* 0x0 */ EftKiBombRes *res;
} EftKiBombMgr;

typedef struct EftKiBomb {
    /* 0x000 */ EftRArg arg;
    /* 0x050 */ EftRPose pose;        /* pose.dir is the velocity here */
    /* 0x090 */ EftRMtx contact;      /* the stage contact handed over by EftHit_NotifyBlastTask; row 3: normal */
    /* 0x0D0 */ s32 flags;            /* EFT_KIBOMB_* */
    /* 0x0D4 */ s32 life;             /* frames until it explodes by itself */
    /* 0x0D8 */ s32 unkD8;
    /* 0x0DC */ f32 speed;
    /* 0x0E0 */ f32 size;
    /* 0x0E4 */ f32 radius;           /* 5, then 28 while exploding */
    /* 0x0E8 */ f32 angle;            /* spin */
    /* 0x0EC */ f32 spin;             /* degrees per frame, either sign */
    /* 0x0F0 */ f32 gravity;          /* 0.5 per frame, +y is down */
    /* 0x0F4 */ s16 frame;
    /* 0x0F6 */ s16 maxLife;          /* 300 frames */
    /* 0x0F8 */ s16 bounces;
    /* 0x0FA */ s16 blast;            /* frames the explosion lasts: 15 */
    /* 0x0FC */ u8 handleBuf[0x30];
    /* 0x12C */ s32 handle;           /* model instance, -1 none */
} EftKiBomb; /* size 0x130 */

/* The type 2 object: the same head, a longer tail. */
typedef struct EftKiObj {
    /* 0x000 */ EftKiBomb b;
    /* 0x130 */ u8 unk130[0xA0];
    /* 0x1D0 */ void *tex;            /* manager resource + 8 */
    /* 0x1D4 */ s16 unk1D4;           /* 15 */
    /* 0x1D6 */ s16 unk1D6;
    /* 0x1D8 */ s32 unk1D8[2];
} EftKiObj; /* size 0x1E0 */

/* Argument of the explosion effect EftImpact_SpawnBlast (eft_v.h's EftImpactArg), passed by value. */
typedef struct EftRBlastFx {
    /* 0x00 */ EftRVec pos;
    /* 0x10 */ EftRVec dir;
    /* 0x20 */ s32 kind;
    /* 0x24 */ s32 objId;
    /* 0x28 */ s32 unk28;
} EftRBlastFx; /* size 0x30 */

/* A texture animation header (EftKiObj_StepTex). */
typedef struct EftRTexAnim {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u8 unk8[0x3C];
    /* 0x44 */ s32 flags;             /* bit 0: tex0 is built */
} EftRTexAnim;

extern EftStruggleMgr *gEftStruggle;
extern EftRSet *gEftClashSparkSet;
extern void *gEftClashSparkList;

s32 EftCharaFx_Stop(s32 objId);

f32 EftStruggle_Start(EftRVec *pos, EftRRec *a, EftRRec *b);
s32 EftStruggle_IsActive(void);
void EftStruggle_End(s32 winner);
void EftStruggle_SetBias(f32 bias);
f32 EftStruggle_GetBias(void);
void EftStruggle_GetPos(EftRVec *out);
void EftStruggle_SetPos(EftRVec *pos);
f32 EftStruggle_GetCamDist(s32 objId, s32 mode);
f32 EftStruggle_GetMidDist(void);
void EftStruggle_SetPower(s32 side, f32 power);
f32 EftStruggle_GetPower(s32 side);
void EftStruggleMgr_Init(EftRTask *task);
void EftStruggleMgr_Term(void);
void EftStruggleMgr_Update(void);
void EftStruggle_Init(EftRTask *task, EftStruggleArg *arg);
void EftStruggle_Term(EftRTask *task);
void EftStruggle_Update(EftRTask *task);
void EftStruggle_Reset(EftRTask *task);
void EftStruggle_Draw(void);
f32 EftStruggle_GetRecRadius(EftRRec *rec);
void EftStruggle_Step(EftRTask *task);

void EftClashSpark_SpawnParts(s32 objId, EftRTask *task, EftRSet *set);
EftRSet *EftClashSpark_GetSet(s32 kind);
s32 EftClashSpark_GetNodeMask(s32 kind);
void EftClashSpark_Init(EftRTask *task, EftClashSparkArg *arg);
void EftClashSpark_Term(EftRTask *task);
void EftClashSpark_Update(EftRTask *task);
void EftClashSpark_PostUpdate(EftRTask *task);
void EftClashSpark_Reset(EftRTask *task);
void EftClashSpark_Draw(void);
void EftClashSparkMgr_Init(EftRTask *task);
void EftClashSparkMgr_Term(void);
void EftClashSparkMgr_Update(void);
void EftClashSparkMgr_Reset(void);
EftRTask *EftClashSpark_Create(EftClashSparkArg arg);
s32 EftClashSpark_Stop(EftRTask *task);
s32 EftClashSpark_SetPos(EftRTask *task, EftRVec *pos);
s32 EftClashSpark_SetScale(EftRTask *task, f32 scale);

void EftCharge_SpawnParts(s32 objId, EftRTask *task, EftRSet *set);
void EftCharge_Init(EftRTask *task, s32 *arg);
void EftCharge_Term(EftRTask *task);
void EftCharge_Update(EftRTask *task);
void EftCharge_Reset(EftRTask *task);
void EftCharge_PostUpdate(EftRTask *task);
void EftCharge_Draw(void);
void EftChargeMgr_Init(EftRTask *task, s32 *arg);
void EftChargeMgr_Term(EftRTask *task);
void EftChargeMgr_Update(EftRTask *task);
s32 EftCharge_Start(s32 *arg);
s32 EftCharge_Burst(s32 objId);
s32 EftCharge_Stop(s32 objId);
s32 EftCharge_IsActive(s32 objId);

void EftKiBlast_AddHitRecord(EftRTask *task);
void EftKiBlast_Turn(EftRTask *task, EftKiBlast *w);
void EftKiBlast_SpawnParts(s32 objId, EftRTask *task, EftRSet *set, s32 reset);
void EftKiBlast_EndTrail(s32 objId, EftRTask *task, EftRSet *set);
void EftKiBlast_Init(EftRTask *task, EftRArg *arg);
void EftKiBlast_Term(EftRTask *task);
void EftKiBlast_Update(EftRTask *task);
void EftKiBlast_Reset(EftRTask *task);
void EftKiBlast_PostUpdate(EftRTask *task);
void EftKiBlast_Draw(EftRTask *task);
void EftKiBlastMgr_Init(EftRTask *task, s32 *arg);
void EftKiBlastMgr_Term(EftRTask *task);
void EftKiBlastMgr_Update(EftRTask *task);
void EftKiBlast_Fire(EftRArg arg);
s32 EftKiBlast_Ret1A(void);
s32 EftKiBlast_Ret1B(void);

void EftBlastCharge_SpawnParts(s32 objId, EftRTask *task, EftRSet *set);
void EftBlastCharge_Init(EftRTask *task, EftBlastChargeArg *arg);
void EftBlastCharge_Term(EftRTask *task);
void EftBlastCharge_Update(EftRTask *task);
void EftBlastCharge_Reset(EftRTask *task);
void EftBlastCharge_PostUpdate(EftRTask *task);
void EftBlastCharge_Draw(void);
void EftBlastChargeMgr_Init(EftRTask *task, s32 *arg);
void EftBlastChargeMgr_Term(EftRTask *task);
void EftBlastChargeMgr_Update(EftRTask *task);
s32 EftBlastCharge_Start(EftBlastChargeArg *arg);
s32 EftBlastCharge_Stop(s32 objId);

void EftKiBomb_AddHitRecord(EftRTask *task);
void EftKiBomb_Launch(EftRTask *task);
void EftKiBomb_Init(EftRTask *task, EftRArg *arg);
void EftKiBomb_Term(EftRTask *task);
void EftKiBomb_Update(EftRTask *task);
void EftKiBomb_Reset(EftRTask *task);
void EftKiBomb_PostUpdate(EftRTask *task);
void EftKiBomb_Draw(EftRTask *task);
void EftKiBombMgr_Init(EftRTask *task, s32 *arg);
void EftKiBombMgr_Term(EftRTask *task);
void EftKiBombMgr_Update(void);
void EftKiBomb_Fire(EftRArg arg);
void EftKiBomb_SetContact(EftRTask *task, EftRMtx *mtx);
void EftKiBomb_Reflect(EftRVec *out, EftRVec *v, EftRVec *normal);
s32 EftKiBomb_Move(EftRTask *task);
void EftKiBomb_HandleHits(EftRTask *task);
void EftKiBomb_UpdateModel(EftRTask *task);

void EftKiObj_AddHitRecord(EftRTask *task);
void EftKiObj_Turn(EftRTask *task, EftKiObj *w);
void EftKiObj_Launch(EftRTask *task);
void EftKiObj_StepTex(EftRTexAnim *tex);
void EftKiObj_Init(EftRTask *task, EftRArg *arg);
void EftKiObj_Term(EftRTask *task);
void EftKiObj_Update(EftRTask *task);

#endif
