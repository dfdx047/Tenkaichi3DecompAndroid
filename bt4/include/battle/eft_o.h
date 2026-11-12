#ifndef BATTLE_EFT_O_H
#define BATTLE_EFT_O_H

#include "types.h"
#include "sys/math3d.h"
#include "sys/list.h"

/*
 * Views local to src/battle/eft_o.c, eft_o_b.c and eft_o_c.c (0x167E68..0x16C2E0). Five modules, in address
 * order (see the comment at the top of each C file):
 *
 *   EftBolt*     0x167E68..0x168600  eft_o.c    second half of the body lightning module of eft_n.c: the
 *                                               per-fighter task (class 0x2C3A78) and its manager (0x2C3A60).
 *                                               Visual.
 *   EftRays*     0x168600..0x1699D0  eft_o.c    effect pack part kind 2: a fan of camera-facing textured
 *                                               quads (classes 0x2C3A90 manager / 0x2C3AA8 item). Visual.
 *   EftBlastObj* 0x1699D0..0x16AE78  eft_o_b.c  THE BLAST OBJECT (classes 0x2C3AC0 manager / 0x2C3AD8 item):
 *                                               one flying shot of a multi-shot technique. SIMULATION.
 *   EftBodyFx*   0x16AE78..0x16B4E0  eft_o_c.c  fighter effect request 0x1A: effect pack 0x23F on the body
 *                                               (classes 0x2C3AF0 manager / 0x2C3B08 item). Visual.
 *   EftDisc*     0x16B4E0..0x16C2E0  eft_o_c.c  first part of the disc module of eft_p.c (classes 0x2C3B20
 *                                               manager / 0x2C3B38 item): manager, init, update. SIMULATION.
 *
 * Every struct is a partial view; the other effect files describe the same blocks under their own names.
 */

/* 16-byte aligned vector (the effect code's vector type; see eft_a.h). */
typedef struct EftOVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) EftOVec;

/* A task of the effect scene (0x40 bytes). */
typedef struct EftOTask {
    /* 0x00 */ u8 state;     /* bit 0: the task was killed (BtlTask_SetDead) */
    /* 0x01 */ u8 unk1[3];
    /* 0x04 */ s32 flags;    /* EFT_TASK_* of eft_a.h, written by the hit pass */
    /* 0x08 */ u16 result;   /* EftHit_CalcResult: bit 0 = the hit pass moved the record (pos is valid),
                                bit 2 = finished */
    /* 0x0A */ u8 unkA[6];
    /* 0x10 */ Vec4 pos;     /* where the record was stopped */
    /* 0x20 */ u8 unk20[8];
    /* 0x28 */ void **cls;   /* class: {update, init, term, post-update, reset, draw} */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
    /* 0x3C */ s32 unk3C;
} EftOTask; /* size 0x40 */

/* Texture set entry: GS TEX0 value plus the image. */
typedef struct EftOTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftOTexEntry; /* size 0x10 */

typedef struct EftOTexSet {
    /* 0x000 */ EftOTexEntry entry[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ s32 unk204;
} EftOTexSet; /* size 0x208 */

/* ---- EftBolt (body lightning; EftBoltWork / EftBoltPool in eft_n.h) -------------------------------------- */

/* One animated texture of the module. */
typedef struct EftOBoltTex {
    /* 0x000 */ s32 ready;   /* the texture was advanced this frame */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ s32 flags;   /* bit 0: frame is valid */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ u64 tex0;
    /* 0x018 */ u64 frame;
    /* 0x020 */ s32 *pack;   /* common effect pack entry 5 / 6 */
    /* 0x024 */ s32 unk24;
    /* 0x028 */ EftOTexSet set;
} EftOBoltTex; /* size 0x230 */

/* gEftBoltPool, 0x478 bytes. */
typedef struct EftOBoltPool {
    /* 0x000 */ s32 count;      /* BtlScene_GetCharCount() */
    /* 0x004 */ s32 max;        /* count * 100 */
    /* 0x008 */ void *pool;     /* max * 0x40 bytes */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ EftOBoltTex tex[2];
    /* 0x470 */ EftOTask **tasks; /* one per fighter, NULL when off */
    /* 0x474 */ s32 *pack4;     /* common effect pack entry 4 */
} EftOBoltPool; /* size 0x478 */

typedef struct EftOBolt {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ s32 active;
    /* 0x10 */ u8 unk10[0x80];
} EftOBolt; /* size 0x90 */

typedef struct EftOBoltFlash {
    /* 0x00 */ s32 active;
    /* 0x04 */ u8 unk4[0x6C];
} EftOBoltFlash; /* size 0x70 */

/* Work of the per-fighter task (class 0x2C3A78). */
typedef struct EftOBoltWork {
    /* 0x000 */ s32 flags;      /* 1 alive, 2 stop asked, 4 dead, 8 the fighter's model changed */
    /* 0x004 */ s32 objId;
    /* 0x008 */ s32 unk8[2];
    /* 0x010 */ EftOBolt a[10];
    /* 0x5B0 */ EftOBoltFlash b[20];
    /* 0xE70 */ s32 paramFlags; /* BtlCharApi_ObjGetParamFlags0 */
    /* 0xE74 */ u8 unkE74[0x24];
    /* 0xE98 */ u64 tex0;
    /* 0xEA0 */ f32 scale;      /* fighter height / 19.35 */
    /* 0xEA4 */ u8 unkEA4[0xC];
} EftOBoltWork; /* size 0xEB0 */

/* ---- EftRays (effect pack part kind 2) ------------------------------------------------------------------- */

/* Argument of EftRays_Create (EftEmitArg2 in eft_h.h). */
typedef struct EftRaysArg {
    /* 0x00 */ u8 chr;
    /* 0x01 */ u8 type;
    /* 0x02 */ u8 tex;      /* texture number in *res */
    /* 0x03 */ u8 unk3;
    /* 0x04 */ struct EftRaysDef *def;
    /* 0x08 */ struct EftRaysTex *res;
    /* 0x0C */ f32 life;    /* seconds; 0 = until stopped */
} EftRaysArg; /* size 0x10 */

/* Four-texture resource of a part. */
typedef struct EftRaysTex {
    /* 0x00 */ EftOTexEntry entry[4];
    /* 0x40 */ s32 count;
    /* 0x44 */ u32 stepped; /* bit n: entry n was advanced this frame */
} EftRaysTex;

typedef struct EftOColor {
    u8 r, g, b, a;
} EftOColor;

typedef struct EftRaysKey {
    /* 0x0 */ f32 height;
    /* 0x4 */ f32 width;
    /* 0x8 */ f32 unk8;
    /* 0xC */ EftOColor color;
} EftRaysKey; /* size 0x10 */

typedef struct EftRaysRange {
    /* 0x0 */ f32 base;
    /* 0x4 */ f32 range;
} EftRaysRange;

/* One ray of the part data. */
typedef struct EftRaysRayDef {
    /* 0x00 */ EftRaysKey key[3];    /* key[2].height is also the reference height of the texture mapping */
    /* 0x30 */ Vec4 len;             /* the width flickers between two random values: x + y * rand, z + w * rand */
    /* 0x40 */ f32 flickTime;        /* seconds */
    /* 0x44 */ f32 colA[3];          /* colour factor pulses between colA and colB */
    /* 0x50 */ Vec4 colB;            /* w is the pulse time in seconds */
    /* 0x60 */ f32 angle;            /* degrees */
    /* 0x64 */ f32 spin;             /* radians per frame */
    /* 0x68 */ f32 keyTime;          /* seconds for key 0 -> 1 -> 2 */
    /* 0x6C */ f32 keySplit;         /* fraction spent on the first leg */
    /* 0x70 */ u16 delay;            /* frames before the ray appears */
    /* 0x72 */ s8 blend;
    /* 0x73 */ u8 unk73[5];
} EftRaysRayDef; /* size 0x78 */

typedef struct EftRaysDef {
    /* 0x000 */ EftRaysRayDef ray[10];
    /* 0x4B0 */ s32 count;
    /* 0x4B4 */ s32 flags;  /* 1 key animation, 2 length flicker, 4 colour pulse */
} EftRaysDef;

typedef struct EftRaysRect {
    /* 0x0 */ EftOColor color;
    /* 0x4 */ f32 width;
    /* 0x8 */ f32 height;
    /* 0xC */ f32 ref;
} EftRaysRect;

typedef struct EftRaysKeyAnim {
    /* 0x0 */ f32 time;
    /* 0x4 */ f32 total;
    /* 0x8 */ f32 split;
} EftRaysKeyAnim;

/* One ray (0x80 bytes of gEftRays). */
typedef struct EftRay {
    /* 0x00 */ ListNode node;
    /* 0x08 */ EftRaysRect rect;
    /* 0x18 */ EftRaysKeyAnim anim;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ f32 rot;
    /* 0x2C */ f32 len;       /* width factor */
    /* 0x30 */ f32 flickT;
    /* 0x34 */ f32 flickTime;
    /* 0x38 */ f32 lenA;
    /* 0x3C */ f32 lenB;
    /* 0x40 */ s32 flickDir;
    /* 0x44 */ s32 unk44[3];
    /* 0x50 */ f32 col[4];
    /* 0x60 */ f32 pulseT;
    /* 0x64 */ f32 pulseTime;
    /* 0x68 */ s32 pulseDir;
    /* 0x6C */ s16 idx;
    /* 0x6E */ s16 delay;
    /* 0x70 */ s32 flags;     /* 1 visible */
    /* 0x74 */ s32 unk74[3];
} EftRay; /* size 0x80 */

#define EFT_RAYS_ALIVE 0x01
#define EFT_RAYS_TIMED 0x04
#define EFT_RAYS_STOP  0x08
#define EFT_RAYS_DEAD  0x10

/* Work of a part task (class 0x2C3AA8). */
typedef struct EftRays {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ List list;     /* its rays */
    /* 0x10 */ EftRaysArg arg;
    /* 0x20 */ Vec4 pos;
    /* 0x30 */ u64 tex0;
    /* 0x38 */ f32 delay;     /* frames before it starts */
    /* 0x3C */ f32 hold;      /* frames between the stop and the fade */
    /* 0x40 */ f32 fadeTime;
    /* 0x44 */ f32 fade;
    /* 0x48 */ f32 life;      /* frames */
    /* 0x4C */ f32 alpha;
    /* 0x50 */ f32 size;
    /* 0x54 */ s32 flags;     /* EFT_RAYS_* */
    /* 0x58 */ s32 unk58[2];
} EftRays; /* size 0x60 */

/* gEftRays, 0xA10 bytes. */
typedef struct EftRaysMgr {
    /* 0x000 */ EftRay ray[20];
    /* 0xA00 */ List free;
    /* 0xA0C */ void *list;   /* child task list (3 tasks) */
} EftRaysMgr;

/* ---- EftBlastObj ----------------------------------------------------------------------------------------- */

/* Definition of a technique's effect (EftHitDef / EftJDef / EftOwnerParam elsewhere). */
typedef struct EftODef {
    /* 0x00 */ s16 id;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ u8 unk4[4];
    /* 0x08 */ s8 shape;    /* 0 two spheres, 1 two boxes */
    /* 0x09 */ u8 unk9[0x2F];
    /* 0x38 */ f32 homing;  /* largest turn per frame; 0 = flies straight */
    /* 0x3C */ s32 flags;   /* 2: the shot does not move */
} EftODef;

/* The shot slot a technique effect belongs to (EftShotSlot / EftHitSrc elsewhere). */
typedef struct EftOSrc {
    /* 0x00 */ s32 objId;
    /* 0x04 */ u8 unk4[0x20];
    /* 0x24 */ EftODef *def;
} EftOSrc;

typedef struct EftOEmitNode {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 pad[3];
    /* 0x10 */ Vec4 pos;
} EftOEmitNode; /* size 0x20 */

/* Node slots of an effect pack (EftEmitNodes in eft_i.h). */
typedef struct EftOEmitNodes {
    /* 0x000 */ u32 flags;
    /* 0x004 */ u32 flagsC;
    /* 0x008 */ u32 fixed;
    /* 0x00C */ s32 unkC;
    /* 0x010 */ EftOEmitNode n[6][2];
    /* 0x190 */ EftOEmitNode c[6];
} __attribute__((aligned(8))) EftOEmitNodes; /* size 0x250 */

/* Part of an effect pack (EftEmitDef in eft_i.h). */
typedef struct EftOPart {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ u8 kind;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 node;
    /* 0x0B */ u8 unkB[0x35];
} EftOPart; /* size 0x40 */

typedef struct EftOGroupDef {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 count;
} EftOGroupDef;

typedef struct EftOGroup {
    /* 0x00 */ EftOGroupDef *def;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 first;
    /* 0x09 */ u8 unk9[7];
} EftOGroup; /* size 0x10 */

/* Effect pack (EftEmitSet in eft_i.h). */
typedef struct EftOSet {
    /* 0x000 */ u32 *mask;
    /* 0x004 */ s32 unk4;
    /* 0x008 */ EftOPart *parts;
    /* 0x00C */ EftOGroup grp[19];
    /* 0x13C */ u8 unk13C[0x1E4];
} EftOSet; /* size 0x320 */

/* The pair a part must match to be driven by this blast object: part kind and node slot. */
typedef struct EftBlastObjSel {
    /* 0x0 */ s32 kind;
    /* 0x4 */ s32 node;
} EftBlastObjSel;

/* Argument of EftBlastObj_Create / EftBlastObj_CreateWithModel (EftJShotArg, EftVolleyShotArg, EftShotArg). */
typedef struct EftBlastObjArg {
    /* 0x00 */ EftOSrc *src;
    /* 0x04 */ EftOSet *set;
    /* 0x08 */ EftOEmitNodes *nodes; /* the creator's node slots, copied */
    /* 0x0C */ Vec4 *pos;
    /* 0x10 */ Vec4 *dir;
    /* 0x14 */ s32 index;    /* number of this shot */
    /* 0x18 */ s32 count;    /* shots of the technique */
    /* 0x1C */ s32 kind;     /* sel[0].kind */
    /* 0x20 */ s32 evtIdx;   /* sel[0].node: 0..4 = the shot's own node slot is refreshed from the fighter */
    /* 0x24 */ f32 life;     /* seconds; 0 = until stopped */
    /* 0x28 */ f32 scale;
    /* 0x2C */ f32 speed;    /* per frame */
    /* 0x30 */ s32 *model;   /* model pack, or NULL */
} EftBlastObjArg; /* size 0x34 */

/* The four vectors a hit record carries (record +0x10). */
typedef struct EftOHitPose {
    /* 0x00 */ Vec4 start;
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 prev;
    /* 0x30 */ Vec4 vel;
} __attribute__((aligned(8))) EftOHitPose; /* size 0x40 */

/* A hit record (EftHitRec in eft_a.h), as far as this file fills it. */
typedef struct EftOHitRec {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ EftOHitPose pose;
    /* 0x50 */ s32 flags;    /* 0x80 and 0x10 set here */
    /* 0x54 */ u8 unk54[0xC];
    /* 0x60 */ EftOTask *task;
    /* 0x64 */ EftOSrc *src;
} EftOHitRec;

/* Optional model of a blast object. */
typedef struct EftBlastObjModel {
    /* 0x00 */ s32 *pack;
    /* 0x04 */ u8 arg[0x31]; /* filled by EftObj_Create */
    /* 0x35 */ s8 obj;       /* battle object id */
    /* 0x36 */ s8 step;      /* animation step, -1 before the first */
    /* 0x37 */ u8 unk37;
    /* 0x38 */ f32 time;     /* frames in the step */
    /* 0x3C */ s32 unk3C;
} EftBlastObjModel; /* size 0x40 */

/* EftBlastObj.flags */
#define EFT_BLASTOBJ_ALIVE    0x0001
#define EFT_BLASTOBJ_STOP     0x0002 /* end asked: no record this frame, dying on the next update */
#define EFT_BLASTOBJ_DYING    0x0004 /* the next update kills the task */
#define EFT_BLASTOBJ_KILLED   0x0010 /* the pack parts were killed */
#define EFT_BLASTOBJ_STUCK    0x0020 /* the hit pass stopped it (task result bit 0): it no longer moves */
#define EFT_BLASTOBJ_FROZEN   0x0040 /* EftBlastObj_SetFrozen: no movement; the model does not turn up / down */
#define EFT_BLASTOBJ_NO_HIT   0x0080 /* EftBlastObj_SetNoHit: no hit record */
#define EFT_BLASTOBJ_HELD     0x0100 /* EftBlastObj_SetHeld: its records carry EFT_HIT_FLAG_NO_MODE1 */
#define EFT_BLASTOBJ_PLACED   0x0200 /* EftBlastObj_SetTarget: the position is written by the owner */
#define EFT_BLASTOBJ_TIMED    0x0400 /* life counts down */
#define EFT_BLASTOBJ_LAST     0x0800 /* EftBlastObj_MarkLast: its records carry EFT_HIT_FLAG_LAST */
#define EFT_BLASTOBJ_MODEL    0x1000 /* it has a model */
#define EFT_BLASTOBJ_ANIM     0x2000 /* EftBlastObj_SetModelAnim: the model plays its steps */
#define EFT_BLASTOBJ_FIRED    0x4000 /* the owner's FIRE event was seen (with ANIM) */

/* Work of a blast object task (class 0x2C3AD8). */
typedef struct EftBlastObj {
    /* 0x000 */ EftOSrc *src;
    /* 0x004 */ Vec4 *posArg;
    /* 0x008 */ Vec4 *dirArg;
    /* 0x00C */ s32 index;
    /* 0x010 */ s32 count;
    /* 0x014 */ f32 life;      /* seconds */
    /* 0x018 */ f32 scaleArg;
    /* 0x01C */ f32 speed;
    /* 0x020 */ EftBlastObjSel sel[2];
    /* 0x030 */ EftOSet *set;
    /* 0x034 */ s32 unk34[3];
    /* 0x040 */ EftOEmitNodes nodes;
    /* 0x290 */ u8 state[0x2CC]; /* EftEmitState */
    /* 0x55C */ s32 unk55C;
    /* 0x560 */ EftOVec dir;   /* unit */
    /* 0x570 */ EftOVec target; /* position written by EftBlastObj_SetTarget */
    /* 0x580 */ s32 flags;     /* EFT_BLASTOBJ_* */
    /* 0x584 */ s32 selCount;
    /* 0x588 */ f32 lifeLeft;  /* frames */
    /* 0x58C */ f32 scale;
    /* 0x590 */ f32 delay;     /* frames before it starts to move; 1 at creation */
    /* 0x594 */ s32 unk594[3];
    /* 0x5A0 */ EftOHitPose pose;
    /* 0x5E0 */ EftBlastObjModel model;
} EftBlastObj; /* size 0x620 */

/* ---- EftBodyFx (fighter effect request 0x1A) ------------------------------------------------------------- */

/* Argument of EftBodyFx_Start (FxArg3 in btl_char_fx.h). */
typedef struct EftBodyFxArg {
    /* 0x0 */ s32 objId;
    /* 0x4 */ s32 unk4;   /* effect pack number in gEftBodyFx (always 0) */
    /* 0x8 */ f32 scale;
} EftBodyFxArg; /* size 0xC */

#define EFT_BODYFX_SETS 1 /* the manager's callbacks loop over this many packs */

/* gEftBodyFx, 0x328 bytes: the effect packs and the task of each fighter. */
typedef struct EftBodyFxMgr {
    /* 0x000 */ EftOSet set[EFT_BODYFX_SETS];
    /* 0x320 */ EftOTask *task[2];
} EftBodyFxMgr;

/* Work of the task (class 0x2C3B08). */
typedef struct EftBodyFx {
    /* 0x000 */ EftOVec pos;   /* fighter node 3 */
    /* 0x010 */ EftOVec dir;   /* unit vector from node 0x2E to node 3 */
    /* 0x020 */ EftBodyFxArg arg;
    /* 0x02C */ EftOSet *set;
    /* 0x030 */ u8 state[0x2CC]; /* EftEmitState */
    /* 0x2FC */ s32 unk2FC;
    /* 0x300 */ EftOEmitNodes nodes;
    /* 0x550 */ s32 flags;     /* 1 alive, 2 stop asked, 4 dead, 8 parts alive, 0x10 was reset, 0x20 parts killed */
    /* 0x554 */ f32 scale;     /* body scale, at least 0.5 */
    /* 0x558 */ s32 phase;     /* phase mask given to the pack: 1 */
    /* 0x55C */ f32 endTime;
    /* 0x560 */ f32 endLife;   /* EftEmit_GetEndFrames */
    /* 0x564 */ s32 unk564[3];
} EftBodyFx; /* size 0x570 */

/* ---- EftDisc (EftDisc / EftDiscMgr in eft_p.h) ------------------------------------------------------------ */

/* Resources and lists of the disc module (0x660 bytes; EftDiscMgr in eft_p.h). */
typedef struct EftODiscRes {
    /* 0x000 */ EftOTexSet texA;   /* common effect pack entry 0x238 */
    /* 0x208 */ EftOTexSet texB;   /* entry 0x23A */
    /* 0x410 */ s32 unk410;
    /* 0x414 */ u8 unk414[0xC];
    /* 0x420 */ u8 modelA[0x90];   /* model template of kinds 0..2: entry 0x239 with texA */
    /* 0x4B0 */ u8 modelB[0x90];   /* model template of kinds 3 and 4: entry 0x23B with texB */
    /* 0x540 */ s32 *packTexA;
    /* 0x544 */ s32 *packTexB;
    /* 0x548 */ s32 *packModelA;
    /* 0x54C */ s32 *packModelB;
    /* 0x550 */ void *list;        /* child task list: 20 discs */
} EftODiscRes;

typedef struct EftODiscMgr {
    /* 0x0 */ EftODiscRes *res;
} EftODiscMgr;

/* Argument of the disc's init (EftDiscArg in eft_p.h, built by the creators after this range). */
typedef struct EftODiscArg {
    /* 0x00 */ EftOSrc *src;  /* technique piece: the technique's shot slot */
    /* 0x04 */ EftOSet *set;  /* technique piece: its effect pack */
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ EftOVec dir;
    /* 0x20 */ EftOVec pos;
    /* 0x30 */ f32 scale;
    /* 0x34 */ f32 speed;     /* per frame */
    /* 0x38 */ f32 life;      /* seconds in flight */
    /* 0x3C */ f32 grow;      /* seconds to reach full size */
    /* 0x40 */ f32 fade;      /* fraction of the life after which it fades out */
    /* 0x44 */ f32 turn;      /* homing: largest turn per frame */
    /* 0x48 */ f32 bank;
    /* 0x4C */ f32 roll;      /* in turns */
    /* 0x50 */ f32 rollTime;  /* seconds over which the roll decays; negative = keep */
    /* 0x54 */ u8 texA;
    /* 0x55 */ u8 texB;
    /* 0x56 */ u8 objId;
    /* 0x57 */ u8 kind;       /* 0 held ki blast, 1 thrown ki blast, 2 held technique piece, 3 thrown from a
                                 node, 4 technique piece */
    /* 0x58 */ s32 lastHit;
    /* 0x5C */ s32 hand;      /* 0: nodes 0x22 / 0x23, else 0x14 / 0x15 */
} EftODiscArg; /* size 0x60 */

/* EftODisc.flags (EFT_DISC_* in eft_p.h) */
#define EFT_ODISC_ALIVE    0x0001
#define EFT_ODISC_END      0x0002 /* fade out, then die */
#define EFT_ODISC_DEAD     0x0004
#define EFT_ODISC_HELD     0x0008 /* follows the owner's hand */
#define EFT_ODISC_FLYING   0x0020 /* moves and adds hit records */
#define EFT_ODISC_SHOWN    0x0040
#define EFT_ODISC_PIECE    0x0080 /* technique piece: has an effect pack */
#define EFT_ODISC_LOST     0x0400 /* the owner stopped charging */
#define EFT_ODISC_GROWN    0x0800
#define EFT_ODISC_AUTO     0x1000 /* thrown as soon as it is grown */
#define EFT_ODISC_MOVED    0x2000

/* Work of a disc (class 0x2C3B38; EftDisc in eft_p.h), 0x4C0 bytes. */
typedef struct EftODisc {
    /* 0x000 */ u8 unk0[0x30];
    /* 0x030 */ EftODiscArg arg;
    /* 0x090 */ u8 atk[0x50];     /* ki blast parameters */
    /* 0x0E0 */ u8 state[0x2CC];  /* EftEmitState of a technique piece */
    /* 0x3AC */ s32 unk3AC;
    /* 0x3B0 */ u8 model[0x48];   /* model instance */
    /* 0x3F8 */ void *tex;        /* its texture set */
    /* 0x3FC */ u8 unk3FC[0x44];
    /* 0x440 */ f32 color[3];     /* 127, 127, 127 */
    /* 0x44C */ f32 alpha;        /* 0..127 */
    /* 0x450 */ EftOVec dir;
    /* 0x460 */ EftOVec prev;     /* previous position (first frame: the owner's node 0x11) */
    /* 0x470 */ EftOVec prev2;
    /* 0x480 */ f32 spin;         /* radians per frame, random for kinds 0..2: appearance only */
    /* 0x484 */ f32 rotX;
    /* 0x488 */ f32 rotY;
    /* 0x48C */ f32 bankAngle;
    /* 0x490 */ f32 rollAngle;
    /* 0x494 */ f32 rollLeft;
    /* 0x498 */ f32 rollTime;
    /* 0x49C */ f32 grown;        /* 0..1 */
    /* 0x4A0 */ f32 growTime;     /* frames */
    /* 0x4A4 */ f32 fadeTime;     /* frames */
    /* 0x4A8 */ f32 fadeLeft;
    /* 0x4AC */ f32 life;         /* frames */
    /* 0x4B0 */ f32 age;          /* frames in flight */
    /* 0x4B4 */ f32 size;         /* owner height / 19.35 (x 1.5 for kinds 0 and 1) */
    /* 0x4B8 */ f32 timer;        /* frames since creation */
    /* 0x4BC */ s32 flags;        /* EFT_ODISC_* */
} EftODisc; /* size 0x4C0 */

extern EftOBoltPool *gEftBoltPool;
extern void *gEftBoltList;
extern EftRaysMgr *gEftRays;
extern s32 *gEftBlastObjMgr;
extern void *gEftBlastObjList;
extern EftBodyFxMgr *gEftBodyFx;
extern void *gEftBodyFxList;
extern EftODiscMgr *gEftDisc;

void EftBolt_UpdateTex(EftOBoltWork *w);
void EftBolt_ReadChar(EftOBoltWork *w, s32 objId);
void EftBoltTask_Init(EftOTask *task, s32 *arg);
void EftBoltTask_Term(EftOTask *task);
void EftBoltTask_Update(EftOTask *task);
void EftBoltTask_PostUpdate(EftOTask *task);
void EftBoltTask_Reset(EftOTask *task);
void EftBoltTask_Draw(EftOTask *task);
void EftBoltMgr_Init(EftOTask *task);
void EftBoltMgr_Term(void);
void EftBoltMgr_Update(void);
void EftBoltMgr_Reset(void);
s32 EftBolt_Enable(s32 objId);
s32 EftBolt_Disable(s32 objId);

void *EftRays_Create(EftRaysArg *arg);
void EftRays_Stop(EftOTask *task);
void EftRays_Kill(EftOTask *task);
void EftRays_SetPos(EftOTask *task, Vec4 *pos);
void EftRays_SetDelay(EftOTask *task, f32 frames);
void EftRays_SetHold(EftOTask *task, f32 frames);
void EftRays_SetFade(EftOTask *task, f32 frames);
s32 EftRays_IsAlive(EftOTask *task);
void EftRays_SetSize(EftOTask *task, f32 size);
void EftRaysMgr_Init(EftOTask *task);
void EftRaysMgr_Term(void);
void EftRaysMgr_Update(void);
void EftRays_Init(EftOTask *task, EftRaysArg *arg);
void EftRays_Term(EftOTask *task);
void EftRays_Update(EftOTask *task);
void EftRays_Reset(EftOTask *task);
void EftRays_Draw(EftOTask *task);
void EftRays_DrawRay(EftRays *w, EftRay *ray);
void EftRays_Step(EftRays *w);
EftRay *EftRays_AllocRay(List *list);
EftRay *EftRays_FreeRay(List *list, EftRay *ray);
u64 EftRays_GetTex(EftRaysTex *res, s32 n);
void EftRays_SetKey(EftRay *ray, EftRaysDef *def, s32 key);
void EftRays_LerpKey(EftRay *ray, EftRaysDef *def);
s32 EftRays_IsTask(EftOTask *task);

void EftBlastObj_AddHit(EftOTask *task);
void EftBlastObj_InitModel(EftOTask *task);
void EftBlastObj_UpdateModel(EftOTask *task);
void EftBlastObj_TermModel(EftOTask *task);
void EftBlastObj_UpdateParts(s32 objId, EftOTask *task, EftOSet *set, s32 reset);
void EftBlastObj_Init(EftOTask *task, EftBlastObjArg *arg);
void EftBlastObj_Term(EftOTask *task);
void EftBlastObj_Update(EftOTask *task);
void EftBlastObj_PostUpdate(EftOTask *task);
void EftBlastObj_Reset(EftOTask *task);
void EftBlastObj_Draw(EftOTask *task);
void EftBlastObjMgr_Init(EftOTask *task);
void EftBlastObjMgr_Term(void);
void EftBlastObjMgr_Update(void);
void EftBlastObjMgr_Reset(void);
void *EftBlastObj_Create(EftBlastObjArg *arg);
void *EftBlastObj_CreateWithModel(EftBlastObjArg *arg, s32 *model);
s32 EftBlastObj_Stop(EftOTask *task);
s32 EftBlastObj_Kill(EftOTask *task);
s32 EftBlastObj_IsAlive(EftOTask *task);
s32 EftBlastObj_IsDying(EftOTask *task);
s32 EftBlastObj_SetSel(EftOTask *task, s32 n, s32 kind, s32 node);
EftOHitPose *EftBlastObj_GetPose(EftOTask *task);
s32 EftBlastObj_SetTarget(EftOTask *task, s32 on, Vec4 *pos);
s32 EftBlastObj_SetPrevPos(EftOTask *task, Vec4 *pos);
s32 EftBlastObj_SetDir(EftOTask *task, Vec4 *dir);
s32 EftBlastObj_SetFrozen(EftOTask *task, s32 on);
s32 EftBlastObj_SetNoHit(EftOTask *task, s32 on);
s32 EftBlastObj_SetHeld(EftOTask *task, s32 on);
s32 EftBlastObj_SetModelAnim(EftOTask *task, s32 on);
s32 EftBlastObj_MarkLast(EftOTask *task);
s32 EftBlastObj_SetDelay(EftOTask *task, f32 frames);

void EftBodyFx_UpdateParts(s32 objId, EftOTask *task, EftOSet *set);
void EftBodyFx_Init(EftOTask *task, EftBodyFxArg *arg);
void EftBodyFx_Term(EftOTask *task);
void EftBodyFx_Update(EftOTask *task);
void EftBodyFx_Reset(EftOTask *task);
void EftBodyFx_PostUpdate(EftOTask *task);
void EftBodyFx_Draw(EftOTask *task);
void EftBodyFxMgr_Init(EftOTask *task);
void EftBodyFxMgr_Term(void);
void EftBodyFxMgr_Update(void);
s32 EftBodyFx_Start(EftBodyFxArg arg);
s32 EftBodyFx_Stop(s32 objId);

void EftDiscMgr_Init(EftOTask *task);
void EftDiscMgr_Update(void);
void EftDiscMgr_Reset(void);
void EftDiscMgr_Term(void);
void EftDisc_Init(EftOTask *task, EftODiscArg *arg);
void EftDisc_Update(EftOTask *task);

#endif
