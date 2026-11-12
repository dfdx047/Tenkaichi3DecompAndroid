#ifndef BATTLE_EFT_J_H
#define BATTLE_EFT_J_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Views local to src/battle/eft_j.c (0x1532A0..0x157398). See the comment at the top of that file.
 * Everything here is a partial view; the other effect files describe the same blocks under their own names
 * (EftKTask / EftKDef / EftKSrc / EftKSet / EftShotTech in eft_k.h, EftEmitSet / EftEmitState / EftFollowWork in
 * eft_i.h).
 */

/* The original vector type is 16-byte aligned (see the notes in eft_j.c); include/sys/math3d.h's Vec4 is not, so
   the work blocks whose code depends on the alignment use this one. */
typedef struct EftJVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(16))) EftJVec; /* size 0x10 */

/* A task of the effect scene's task tree (0x40 bytes); only what this file touches. */
typedef struct EftJTask {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 state;   /* phase of the item's state machine */
    /* 0x02 */ u8 unk2[2];
    /* 0x04 */ s32 unk4;   /* written by the hit pass: bit 0 the record hit a fighter, bit 2 it ended */
    /* 0x08 */ u16 hit;    /* written by the hit pass: bit 0 the head was moved (pos is valid), bit 2 stop */
    /* 0x0A */ u8 unkA[6];
    /* 0x10 */ Vec4 pos;   /* corrected head position, written by the scene's hit pass */
    /* 0x20 */ u8 unk20[0x18];
    /* 0x38 */ void *work; /* the task's work block, allocated with the list */
    /* 0x3C */ s32 unk3C;
} EftJTask; /* size 0x40 */

/* Definition record of one technique effect (the record the fighter's technique table points at). */
typedef struct EftJDef {
    /* 0x00 */ s16 id;      /* effect id: selects the look and the special cases below */
    /* 0x02 */ s16 recType; /* copied to the hit record's type */
    /* 0x04 */ s8 unk4;     /* non-zero: EftShot_SetHeldFlagA8(objId) when the item ends; zero: fire sound */
    /* 0x05 */ s8 unk5;
    /* 0x06 */ s8 unk6;
    /* 0x07 */ s8 unk7;
    /* 0x08 */ s8 shape;    /* 0 two spheres, 1 two capsules along the beam */
    /* 0x09 */ s8 count;    /* number of pieces a multi-part effect fires */
    /* 0x0A */ u8 unkA[0x1E];
    /* 0x28 */ s32 life;    /* frames */
    /* 0x2C */ f32 shotLife; /* frames a blast object of the "shots" module lives before it is stopped */
    /* 0x30 */ f32 scale;
    /* 0x34 */ f32 speed;
    /* 0x38 */ f32 homing;
    /* 0x3C */ s32 flags;   /* 1 adds hit records, 2 does not move, 0x20 keeps moving after END, 0x200 stage blur,
                               0x8000 multi: pieces are released one by one, 0x10000, 0x400000 no flash */
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s16 burst;   /* "shots" module: blast objects fired on the FIRE event (at least 1) */
    /* 0x46 */ s16 unk46;
    /* 0x48 */ s16 blurOn;  /* index into the event-bit table: tint starts */
    /* 0x4A */ s16 blurOff; /* tint ends */
    /* 0x4C */ u8 unk4C[0xC];
    /* 0x58 */ s8 subKind[8]; /* "shots" module: kind of the n-th sub-effect (EftStreak_Start), negative = none */
    /* 0x60 */ f32 subAngle[8]; /* its angle in degrees */
    /* 0x80 */ s16 subArg;
} EftJDef;

/* What a module's init callback receives: one technique effect of one fighter. */
typedef struct EftJSrc {
    /* 0x00 */ s32 objId; /* object id of the fighter (0 or 1) */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 unkC[0x10];
    /* 0x1C */ s32 *pack; /* the effect's resource pack */
    /* 0x20 */ s32 objId20;
    /* 0x24 */ EftJDef *def;
} EftJSrc;

/* Model set of a module (built by EftEmit_LoadSet from the pack). */
typedef struct EftJPart {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ u8 type; /* 4: "muzzle" emitter of the shots module */
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 kind; /* 5: skipped by EftMulti_UpdateParts */
    /* 0x0B */ u8 unkB[0x35];
} EftJPart; /* size 0x40 */

typedef struct EftJGroup {
    /* 0x00 */ u8 *info; /* [0] group id, [1] part count */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 base;  /* first part of the group */
    /* 0x09 */ u8 unk9[7];
} EftJGroup; /* size 0x10 */

typedef struct EftJSet {
    /* 0x00 */ u32 *mask; /* bit g: group g exists */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ EftJPart *parts;
    /* 0x0C */ EftJGroup grp[19];
} EftJSet;

/* One of the ten pieces of a multi-part effect. */
typedef struct EftJPiece {
    /* 0x00 */ s32 flags; /* 1 started, 2 released */
    /* 0x04 */ void *h;   /* handle from EftDisc_Create */
} EftJPiece;

/* Parameter block of EftDisc_Create. */
typedef struct EftJPieceArg {
    /* 0x00 */ EftJSrc *src;
    /* 0x04 */ EftJSet *set;
    /* 0x10 */ Vec4 dir __attribute__((aligned(16)));
    /* 0x20 */ Vec4 pos;
    /* 0x30 */ f32 scale;
    /* 0x34 */ f32 speed;
    /* 0x38 */ f32 life; /* seconds */
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ f32 unk40;
    /* 0x44 */ f32 homing;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ f32 unk4C;
    /* 0x50 */ f32 unk50;
    /* 0x54 */ u8 unk54;
    /* 0x55 */ u8 unk55;
    /* 0x56 */ u8 objId;
    /* 0x57 */ u8 unk57;
    /* 0x58 */ s32 unk58;
    /* 0x5C */ s32 unk5C;
} EftJPieceArg; /* size 0x60 */

/* Work of the manager task of effect type 5, "multi" (class 0x2C3868). */
typedef struct EftMultiMgr {
    /* 0x000 */ u8 unk0[8];
    /* 0x008 */ u8 tex[0x204]; /* filled by EftTexSet_Load32 for ids 0x158 / 0x202 */
    /* 0x20C */ s32 unk20C;
    /* 0x210 */ EftJSet set;
    /* 0x34C */ u8 unk34C[0x1E4];
} EftMultiMgr; /* size 0x530 */

/* Work of an item of effect type 5 (class 0x2C3880). */
typedef struct EftMulti {
    /* 0x000 */ s32 flags;
    /* 0x004 */ f32 speed;
    /* 0x008 */ f32 homing;
    /* 0x00C */ f32 unkC;
    /* 0x010 */ f32 scale;
    /* 0x014 */ f32 unk14;
    /* 0x018 */ f32 unk18;
    /* 0x01C */ f32 unk1C;
    /* 0x020 */ f32 timer;
    /* 0x024 */ f32 life;
    /* 0x028 */ u8 unk28[8];
    /* 0x030 */ Vec4 dir;
    /* 0x040 */ Vec4 unk40;
    /* 0x050 */ Vec4 unk50;
    /* 0x060 */ Vec4 unk60;
    /* 0x070 */ Vec4 head;
    /* 0x080 */ Vec4 unk80;
    /* 0x090 */ Vec4 vel;
    /* 0x0A0 */ EftJSrc *src;
    /* 0x0A4 */ u8 emit[0x2CC];  /* emitter state (EftEmit_InitState) */
    /* 0x370 */ Vec4 node[2];
    /* 0x390 */ Vec4 unk390[4];
    /* 0x3D0 */ Vec4 unk3D0;
    /* 0x3E0 */ u8 unk3E0[0x1E0];
    /* 0x5C0 */ EftJSet *set;
    /* 0x5C4 */ EftJPiece piece[10];
    /* 0x614 */ s32 count;
    /* 0x618 */ s32 unk618[2];
} EftMulti; /* size 0x620 */

/* A battle object (model) that follows a shot: the prop of effect type 6. */
typedef struct EftProp {
    /* 0x00 */ u8 objArg[0x30]; /* argument block of BtlObj_Create, filled by EftObj_Create */
    /* 0x30 */ EftJVec pos;
    /* 0x40 */ EftJVec offset;
    /* 0x50 */ f32 scale;
    /* 0x54 */ f32 flyScale;    /* scale once fired */
    /* 0x58 */ f32 bobAmp;
    /* 0x5C */ f32 bobAngle;
    /* 0x60 */ f32 bobStep;
    /* 0x64 */ s32 obj;         /* battle object id, -1 when none */
    /* 0x68 */ s32 flags;       /* 1 held, 2 flying, 8 turned half a turn, 0x10 follows the hand, 0x20 bobs, 0x40 shown */
    /* 0x6C */ s32 unk6C;
} EftProp; /* size 0x70 */

/* Work of the manager task of effect type 6, "prop shot" (class 0x2C3898). */
typedef struct EftPropShotMgr {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ EftJSet set;
    /* 0x140 */ u8 unk140[0x1E4];
    /* 0x324 */ s32 *model; /* pack entry of the prop's model */
} EftPropShotMgr; /* size 0x328 */

/* Work of an item of effect type 6 (class 0x2C38B0). */
typedef struct EftPropShot {
    /* 0x000 */ s32 flags;
    /* 0x004 */ f32 speed;
    /* 0x008 */ f32 unk8;
    /* 0x00C */ f32 scale;
    /* 0x010 */ f32 radius;
    /* 0x014 */ f32 unk14;
    /* 0x018 */ f32 timer;
    /* 0x01C */ f32 life;
    /* 0x020 */ f32 blur;
    /* 0x024 */ u8 unk24[0xC];
    /* 0x030 */ EftJVec dir;
    /* 0x040 */ EftJVec origin;
    /* 0x050 */ EftJVec head;
    /* 0x060 */ EftJVec tail;
    /* 0x070 */ EftJVec vel;
    /* 0x080 */ EftJSrc *src;
    /* 0x084 */ u8 emit[0x2CC];  /* emitter state (EftEmit_InitState) */
    /* 0x350 */ EftJVec node[2];
    /* 0x370 */ EftJVec hand;
    /* 0x380 */ EftJVec unk380[3];
    /* 0x3B0 */ EftJVec muzzle;
    /* 0x3C0 */ u8 unk3C0[0x1E0];
    /* 0x5A0 */ EftJSet *set;
    /* 0x5A4 */ u8 unk5A4[0xC];
    /* 0x5B0 */ EftProp prop;
    /* 0x620 */ u8 unk620[0x10];
} EftPropShot; /* size 0x630 */

/* A hit record of the scene's list (0x190 bytes); only what this file writes. */
typedef struct EftJHitRec {
    /* 0x00 */ s32 objId;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ EftJVec origin;
    /* 0x20 */ EftJVec pos;
    /* 0x30 */ EftJVec prev;
    /* 0x40 */ EftJVec vel;
    /* 0x50 */ s32 flags;
    /* 0x54 */ u8 unk54[0xC];
    /* 0x60 */ EftJTask *task;
    /* 0x64 */ EftJSrc *src;
    /* 0x68 */ void *def2;
    /* 0x6C */ u8 unk6C[0x124];
} EftJHitRec; /* size 0x190 */

/* Work of the manager task of effect type 0, "blast" (class 0x2C38C8). */
typedef struct EftBlastMgr {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ EftJSet set;
    /* 0x140 */ u8 unk140[0x1E4];
} EftBlastMgr; /* size 0x324 */

/* Work of an item of effect type 0 (class 0x2C38E0). */
typedef struct EftBlast {
    /* 0x000 */ s32 flags;
    /* 0x004 */ f32 speed;
    /* 0x008 */ f32 scale;       /* target scale */
    /* 0x00C */ f32 drawScale;   /* scale the emitters are drawn with */
    /* 0x010 */ f32 radius;      /* scale of the hit shapes */
    /* 0x014 */ f32 ratio;       /* charge ratio read from the fighter */
    /* 0x018 */ f32 timer;
    /* 0x01C */ f32 life;
    /* 0x020 */ f32 blur;
    /* 0x024 */ u8 unk24[0xC];
    /* 0x030 */ EftJVec dir;
    /* 0x040 */ EftJVec blurDir;
    /* 0x050 */ EftJVec origin;
    /* 0x060 */ EftJVec head;
    /* 0x070 */ EftJVec tail;
    /* 0x080 */ EftJVec vel;
    /* 0x090 */ EftJSrc *src;
    /* 0x094 */ u8 emit[0x2CC];
    /* 0x360 */ EftJVec node[2];
    /* 0x380 */ EftJVec hand;
    /* 0x390 */ EftJVec unk390[3];
    /* 0x3C0 */ EftJVec muzzle;
    /* 0x3D0 */ u8 unk3D0[0x1E0];
    /* 0x5B0 */ EftJSet *set;
    /* 0x5B4 */ u8 unk5B4[0xC];
} EftBlast; /* size 0x5C0 */

/* ---- module "shots" (classes 0x2C38F8 manager / 0x2C3910 item); its callbacks are in eft_k.c, where the same
   blocks are called EftShotTechMgr / EftShotTech / EftKModel. ---- */

typedef struct EftJShotMgr {
    /* 0x000 */ EftJSet set;
    /* 0x13C */ u8 unk13C[0x1F0];
    /* 0x32C */ s32 *modelPack;
} EftJShotMgr; /* size 0x330 */

typedef struct EftJShotModel {
    /* 0x00 */ s32 *pack;
    /* 0x04 */ u8 arg[0x30];
    /* 0x34 */ u8 flags;
    /* 0x35 */ s8 objId; /* battle object id, -1 when none */
    /* 0x36 */ s8 unk36;
    /* 0x37 */ u8 unk37;
} EftJShotModel; /* size 0x38 */

/* One of the 14 blast objects of an item. */
typedef struct EftJShotSlot {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s32 flags; /* 1 fired, 8 stopped */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ f32 timer;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ void *h;   /* blast object (EftBlastObj_Create), NULL when free */
    /* 0x24 */ u8 unk24[0xC];
} EftJShotSlot; /* size 0x30 */

typedef struct EftJShotTech {
    /* 0x000 */ EftJShotMgr *mgr;
    /* 0x004 */ u8 emit[0x2CC];
    /* 0x2D0 */ EftJVec nodes[37];
    /* 0x520 */ EftJVec dir;
    /* 0x530 */ EftJVec shotDir;
    /* 0x540 */ EftJVec blurDir;
    /* 0x550 */ EftJVec start;
    /* 0x560 */ EftJVec muzzleDir;
    /* 0x570 */ EftJVec muzzlePos;
    /* 0x580 */ s32 flags;
    /* 0x584 */ s32 node;
    /* 0x588 */ s32 phase;
    /* 0x58C */ u8 unk58C[8];
    /* 0x594 */ f32 speed;
    /* 0x598 */ f32 scale;
    /* 0x59C */ f32 blur;
    /* 0x5A0 */ EftJVec unk5A0;
    /* 0x5B0 */ EftJVec pos;
    /* 0x5C0 */ EftJVec prev;
    /* 0x5D0 */ EftJVec vel;
    /* 0x5E0 */ EftJSrc *src;
    /* 0x5E4 */ u8 unk5E4[0xC];
    /* 0x5F0 */ EftJShotSlot shots[14];
    /* 0x890 */ s32 shotCount;
    /* 0x894 */ s32 *camPack;
    /* 0x898 */ void *subs[8];
    /* 0x8B8 */ s32 subCount;
    /* 0x8BC */ void *auraFx;
    /* 0x8C0 */ EftJShotModel model;
    /* 0x8F8 */ u8 unk8F8[8];
} EftJShotTech; /* size 0x900 */

/* Parameter block of EftBlastObj_Create (creates one blast object). */
typedef struct EftJShotArg {
    /* 0x00 */ EftJSrc *src;
    /* 0x04 */ EftJShotMgr *mgr;
    /* 0x08 */ void *nodes;
    /* 0x0C */ void *pos;
    /* 0x10 */ void *dir;
    /* 0x14 */ s32 index;
    /* 0x18 */ s32 count;
    /* 0x1C */ s32 kind;
    /* 0x20 */ s32 evtIdx;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ f32 scale;
    /* 0x2C */ f32 speed;
} EftJShotArg; /* size 0x30 */

/* Parameter block of EftFlash_Start (screen flash). */
typedef struct EftJFlashArg {
    /* 0x00 */ EftJVec color;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ s32 objId;
    /* 0x20 */ s32 unk20;
} EftJFlashArg; /* size 0x30 */

#endif
