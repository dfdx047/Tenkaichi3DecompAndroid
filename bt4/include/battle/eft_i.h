#ifndef BATTLE_EFT_I_H
#define BATTLE_EFT_I_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Technique effects, third part: src/battle/eft_i.c = 0x14F230..0x1532A0 (43 functions).
 * It continues src/battle/eft_h.c (layer 1 of the effect scene, the technique effect manager) and holds
 *
 * 1. The rest of the effect pack library (EftEmit_*): the last four per-kind part drivers
 *    (EftEmit_SpawnType9 / 10 / 15 / 12), the dispatcher every technique effect type calls for each part of
 *    its pack (EftEmit_Spawn, EftEmit_SpawnOwn), the passes over all parts (EftEmit_KillAll,
 *    EftEmit_UpdateAlive, EftEmit_MarkCond, EftEmit_MarkKind6), the node slots a pack's parts are attached to
 *    (EftEmit_UpdateNodes...), and the two key-framed values of the pack header.
 *
 * 2. Two of the technique effect types of gEftShotClass (0x2C3700, row = EftShotParam.type + 1):
 *      type 3 (row 4, classes 0x2C3808 group / 0x2C3820 instance)  EftSweep*: a beam from a fighter node whose
 *                 far end sweeps a circle through the opponent's position. It carries a hit record, scorches
 *                 the stage where the beam touches it and destroys the stage object it touches.
 *      type 7 (row 8, classes 0x2C3838 group / 0x2C3850 instance)  EftFollow*: an effect that stays on the
 *                 fighter (node 3) and carries a hit record there; optionally drives the stage blur light.
 *                 Its group class and its last two instance callbacks (reset 0x1532A0, draw 0x1532E8) are in
 *                 the next file.
 *
 * Everything here is a local view (the same objects are EftSet / EftSetDef / EftSetState / EftHSlot /
 * EftShotParam in eft_h.h): offsets are from the matching code, names from how this file uses them.
 */

#define EFT_EMIT_TYPES 19    /* groups in a set; also the bits of *EftEmitSet.mask */
#define EFT_EMIT_MAX 40      /* emitters over all groups (handle slots per half) */
#define EFT_EMIT_NODES 6

/* Flags argument of EftEmit_Spawn and of the eleven EftEmit_SpawnXxx. */
#define EFT_SPAWN_START 0x001  /* create the particle object if the slot is empty */
#define EFT_SPAWN_STOP 0x002   /* let it finish (module "stop" call) */
#define EFT_SPAWN_FADE 0x004   /* with STOP: clear its second parameter first */
#define EFT_SPAWN_KILL 0x008   /* destroy now and clear the slot */
#define EFT_SPAWN_MOVE 0x010   /* write the position */
#define EFT_SPAWN_WARP 0x020   /* with MOVE: the module's second "set position" entry */
#define EFT_SPAWN_SCALE 0x040  /* write the size; also advances the emitter's scale animation */
#define EFT_SPAWN_DIR 0x080    /* write the direction */
#define EFT_SPAWN_DIR2 0x200   /* module 15 only: write the second point without MOVE */

/* EftEmitState.flags[n] */
#define EFT_EMIT_ONESHOT 0x01  /* a kind 2 / 6 emitter has been fired */
#define EFT_EMIT_STARTED 0x02
#define EFT_EMIT_STOPPED 0x04
#define EFT_EMIT_ALTNODE 0x20  /* use EftEmitDef.altNode (set by EftEmit_MarkCond) */
#define EFT_EMIT_KIND6 0x40    /* set by EftEmit_MarkKind6 */

/* One emitter of a set. */
typedef struct EftEmitDef {
    /* 0x00 */ u8 flags;      /* 0x40 spawn at the node position (not offset along dir), 0x20 extra module call,
                                 0x08 take the node from EftEmitNodes.c[] */
    /* 0x01 */ u8 tex;        /* added to the group's texBase: index into EftEmitSet.tex33 / tex17 */
    /* 0x02 */ u8 unk2;       /* module parameter */
    /* 0x03 */ u8 unk3;
    /* 0x04 */ u8 unk4;
    /* 0x05 */ u8 unk5;       /* module parameters */
    /* 0x06 */ u8 unk6;
    /* 0x07 */ u8 unk7;
    /* 0x08 */ u8 kind;       /* 2 and 6 are one-shots */
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 node;       /* index into gEftEmitNodeSlot; 6 = opponent node 3 */
    /* 0x0B */ u8 dirMode;    /* 0 dir, 1 -dir, 2 / 3 perpendicular, 4 up, 5 down */
    /* 0x0C */ f32 spread;    /* radius of the random offset */
    /* 0x10 */ f32 offset;    /* distance along dir, times the scale */
    /* 0x14 */ f32 offset2;   /* second point (module 15) */
    /* 0x18 */ f32 rate;
    /* 0x1C */ f32 scale0;    /* scale animation: scale0 -> scale1 -> scale2 */
    /* 0x20 */ f32 scale1;
    /* 0x24 */ f32 scale2;
    /* 0x28 */ f32 scaleTime; /* seconds; 0 = add scaleStep every frame up / down to scale2 */
    /* 0x2C */ f32 scaleSplit; /* fraction of scaleTime spent on the first leg */
    /* 0x30 */ f32 scaleStep;
    /* 0x34 */ u8 flags2;     /* 0x01 depends on the opponent's parameter flag 0x80 (0x10: wanted set),
                                 0x04 has a condition, 0x08 extra module call */
    /* 0x35 */ u8 cond;       /* bit number tested by EftEmit_MarkCond */
    /* 0x36 */ u8 altNode;
    /* 0x37 */ u8 scaleMode;  /* 0 caller's scale, 1 owner's body scale, 2 opponent's body scale */
    /* 0x38 */ u8 unk38[8];
} EftEmitDef; /* size 0x40 */

typedef struct EftEmitGroupDef {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 count; /* emitters in the group */
} EftEmitGroupDef;

typedef struct EftEmitGroup {
    /* 0x00 */ EftEmitGroupDef *def;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 first;    /* index of the group's first emitter in defs / handles */
    /* 0x09 */ u8 resFirst; /* index of its first entry in res[] */
    /* 0x0A */ u8 unkA[2];
    /* 0x0C */ u8 texBase;
    /* 0x0D */ u8 unkD[3];
} EftEmitGroup; /* size 0x10 */

/* What the effect knows about the technique (EftShotParam in eft_h.h); only what this file reads. */
typedef struct EftOwnerParam {
    /* 0x00 */ u8 unk0[4];
    /* 0x04 */ s8 unk4;       /* non-zero: EftSweep_Term sets the fighter's held flag 0xA8 (0x14AB60) */
    /* 0x05 */ u8 unk5;
    /* 0x06 */ s8 node;       /* passed on to the particle modules 3 and 4 */
    /* 0x07 */ u8 unk7;
    /* 0x08 */ s8 hitShape;   /* 0: two spheres (0x2399A0), 1: two boxes (0x239588) */
    /* 0x09 */ u8 unk9[5];
    /* 0x0E */ s8 nodes[EFT_EMIT_NODES]; /* fighter node per slot */
    /* 0x14 */ u8 unk14[0x1C];
    /* 0x30 */ f32 scale;
    /* 0x34 */ f32 speed;
    /* 0x38 */ u8 unk38[4];
    /* 0x3C */ s32 flags;     /* 0x1 the sweep carries a hit record, 0x4 node slots come from nodes[], 0x8 the
                                 c[] slots are used, 0x200 drives the stage blur light */
    /* 0x40 */ u8 unk40[2];
    /* 0x42 */ s16 markEffect; /* impact effect started where the beam touched the stage; < 0 none */
    /* 0x44 */ u8 unk44[4];
    /* 0x48 */ s16 lightOnBit; /* index into {2, 4, 8, 0x10, 0x20, 0x40}: the event that lights; 2 = no light */
    /* 0x4A */ s16 lightOffBit;
    /* 0x4C */ u8 unk4C[8];
    /* 0x54 */ f32 markSize;
} EftOwnerParam;

/* The technique slot an effect belongs to (EftHSlot in eft_h.h): argument of the init callbacks. */
typedef struct EftOwner {
    /* 0x00 */ s32 objId;  /* fighter object id; 0 = character 0 */
    /* 0x04 */ u8 unk4[0x18];
    /* 0x1C */ s32 *pack;  /* the technique's effect pack */
    /* 0x20 */ s32 unk20;
    /* 0x24 */ EftOwnerParam *param;
} EftOwner;

/* Header of an emitter set's data. */
typedef struct EftEmitHdr {
    /* 0x00 */ u32 mask;      /* bit t set: group t exists */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ f32 width0;    /* trail width animation: width0 -> width1 -> width2 */
    /* 0x0C */ f32 width1;
    /* 0x10 */ f32 width2;
    /* 0x14 */ f32 widthTime; /* seconds; 0 = add widthStep every frame up to width2 */
    /* 0x18 */ f32 widthSplit;
    /* 0x1C */ f32 widthStep;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 flags;      /* 4: the second animation is used (EftEmit_HasWidth2) */
    /* 0x22 */ u8 unk22[10];
    /* 0x2C */ f32 b0;        /* second animation, same shape without the step form */
    /* 0x30 */ f32 b1;
    /* 0x34 */ f32 b2;
    /* 0x38 */ f32 bTime;
    /* 0x3C */ f32 bSplit;
} EftEmitHdr;

typedef struct EftEmitRes {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
} EftEmitRes;

typedef struct EftEmitSet {
    /* 0x000 */ EftEmitHdr *hdr;
    /* 0x004 */ s32 unk4;
    /* 0x008 */ EftEmitDef *defs;
    union {
        /* 0x00C */ EftEmitGroup grp[EFT_EMIT_TYPES];
        /* 0x00C */ EftEmitRes pair[94]; /* [54 + EftEmitGroup.resFirst + idx] (0x1BC on): per emitter resource
                                            words. The code indexes from 0xC with the 54 in the index, which is
                                            what this union reproduces. */
    };
    /* 0x2FC */ s32 unk2FC;
    /* 0x300 */ u8 *tex33;   /* entries of 0x108 bytes */
    /* 0x304 */ u8 *tex17;   /* entries of 0x88 bytes */
    /* 0x308 */ u8 unk308[0x14];
    /* 0x31C */ EftOwner *owner;
} EftEmitSet; /* size 0x320 */

#define EFT_EMIT_RES(set, i) ((set)->pair[(i) + 54])

/* Particle objects of a set, one slot per emitter. */
typedef struct EftEmitHandles {
    /* 0x00 */ void *h[EFT_EMIT_MAX];
} EftEmitHandles; /* size 0xA0 */

/* Run-time state of a set: the particle objects it has started. */
typedef struct EftEmitState {
    /* 0x000 */ EftEmitHandles handle[2];      /* [1] is the second node of a paired slot */
    /* 0x140 */ u8 flags[EFT_EMIT_MAX];        /* EFT_EMIT_* */
    /* 0x168 */ f32 scale[EFT_EMIT_MAX];
    /* 0x208 */ f32 time[EFT_EMIT_MAX];        /* frames since the scale animation started */
    /* 0x2A8 */ f32 trailWidth;                /* animated by EftEmit_UpdateTrailWidth; scales the hit radius */
    /* 0x2AC */ u8 unk2AC[8];
    /* 0x2B4 */ f32 trailTime;
    /* 0x2B8 */ f32 width2;                    /* animated by EftEmit_UpdateWidth2 */
    /* 0x2BC */ u8 unk2BC[8];
    /* 0x2C4 */ f32 time2;
    /* 0x2C8 */ u8 unk2C8[4];
} EftEmitState; /* size 0x2CC */

typedef struct EftEmitNode {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 pad[3];
    /* 0x10 */ Vec4 pos;
} EftEmitNode; /* size 0x20 */

/* Node positions sampled once per frame for a set (EftEmit_UpdateNodes). */
typedef struct EftEmitNodes {
    /* 0x000 */ u32 flags;  /* bit 2i slot i resolved, bit 2i+1 slot i is a pair; 0x1000 frozen */
    /* 0x004 */ u32 flagsC; /* bit i: c[i] resolved */
    /* 0x008 */ u32 fixed;  /* bit 2i: slot i was given a node by EftEmit_SetNode */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ EftEmitNode n[EFT_EMIT_NODES][2];
    /* 0x190 */ EftEmitNode c[EFT_EMIT_NODES];
} EftEmitNodes; /* size 0x250 */

/* Effect task as this file sees it (0x40 bytes, see btl_scene.h). */
typedef struct EftTask {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 step;
    /* 0x02 */ u8 unk2[2];
    /* 0x04 */ s32 flags; /* 1 and 2 are tested by the post-update callbacks */
    /* 0x08 */ u8 unk8[0x30];
    /* 0x38 */ void *work;
    /* 0x3C */ s32 unk3C;
} EftTask;

/* The four vectors a hit record carries (record +0x10); copied whole, with 64-bit moves. */
typedef struct EftHitPose {
    /* 0x00 */ Vec4 start; /* where the effect was started */
    /* 0x10 */ Vec4 cur;   /* position this frame (record +0x20) */
    /* 0x20 */ Vec4 prev;  /* position last frame (record +0x30) */
    /* 0x30 */ Vec4 vel;
} __attribute__((aligned(8))) EftHitPose; /* size 0x40 */

/* A hit record of the effect scene (0x190 bytes, EftHit_GetNew); only what this file writes. */
typedef struct EftIHitRec {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ EftHitPose pose;
    /* 0x50 */ s32 flags;      /* 0x40 set here */
    /* 0x54 */ u8 unk54[0xC];
    /* 0x60 */ struct EftTask *task;
    /* 0x64 */ EftOwner *owner;
} EftIHitRec;

/* Work of the parent task of both classes: one emitter set shared by its children. */
typedef struct EftSetWork {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ EftEmitSet set;
} EftSetWork; /* size 0x324 */

/* Four floats, 8-byte aligned: copied with 64-bit moves. */
typedef struct EftIVec4A {
    f32 x, y, z, w;
} __attribute__((aligned(8))) EftIVec4A;

typedef struct EftSweepMark {
    /* 0x00 */ EftIVec4A pos;
    /* 0x10 */ s32 active;
    /* 0x14 */ f32 timer; /* frames */
    /* 0x18 */ s32 pad[2];
} EftSweepMark; /* size 0x20 */

/* EftSweepWork.flags */
#define EFT_SWEEP_ENDING 0x001   /* counting time up to life */
#define EFT_SWEEP_DEAD 0x002
#define EFT_SWEEP_NOW 0x004      /* end without waiting for life */
#define EFT_SWEEP_STOP_SEEN 0x008
#define EFT_SWEEP_TRAIL 0x010    /* fired: add the hit record every frame */
#define EFT_SWEEP_STARTED 0x020
#define EFT_SWEEP_PREV_VALID 0x040
#define EFT_SWEEP_KILLED 0x080
#define EFT_SWEEP_OWN_WIDTH 0x100

/* Work of the instance class 0x2C3820 (technique effect type 3). */
typedef struct EftSweepWork {
    /* 0x000 */ Vec4 pos;      /* beam origin and centre of the sweep: node slot 1 */
    /* 0x010 */ Vec4 dir;
    /* 0x020 */ EftHitPose pose; /* start = node slot 1 at the start, cur = end point of the beam (0x30),
                                    prev = its last position (0x40), vel = dir * speed (0x50) */
    /* 0x060 */ EftOwner *owner;
    /* 0x064 */ EftEmitState state;
    /* 0x330 */ EftEmitNodes nodes;
    /* 0x580 */ EftEmitSet *set;
    /* 0x584 */ s32 unk584[3];
    /* 0x590 */ EftSweepMark mark[15];
    /* 0x770 */ f32 radius;    /* distance between the fighters' node 3, plus 50, at most 800 */
    /* 0x774 */ f32 angle;
    /* 0x778 */ f32 angleStep;
    /* 0x77C */ s32 flags;     /* EFT_SWEEP_* */
    /* 0x780 */ f32 speed;
    /* 0x784 */ f32 unk784;
    /* 0x788 */ f32 scale;
    /* 0x78C */ f32 width;     /* hit radius before the pack's width animation */
    /* 0x790 */ f32 unk790;
    /* 0x794 */ f32 time;
    /* 0x798 */ f32 life;
    /* 0x79C */ s32 unk79C;
} EftSweepWork; /* size 0x7A0 */

/* EftFollowWork.flags: the EFT_SWEEP_* values shifted where they differ. */
#define EFT_FOLLOW_ENDING 0x001
#define EFT_FOLLOW_DEAD 0x002
#define EFT_FOLLOW_NOW 0x004
#define EFT_FOLLOW_STOP_SEEN 0x008
#define EFT_FOLLOW_TRAIL 0x010
#define EFT_FOLLOW_STARTED 0x040
#define EFT_FOLLOW_KILLED 0x080
#define EFT_FOLLOW_OWN_WIDTH 0x200
#define EFT_FOLLOW_LIGHT 0x400

/* Work of the instance class 0x2C3850 (technique effect type 7). */
typedef struct EftFollowWork {
    /* 0x000 */ s32 flags;
    /* 0x004 */ f32 unk4;
    /* 0x008 */ f32 unk8;
    /* 0x00C */ f32 scale;
    /* 0x010 */ f32 width;
    /* 0x014 */ f32 unk14;
    /* 0x018 */ f32 time;
    /* 0x01C */ f32 life;
    /* 0x020 */ f32 light;   /* 0..1, stage light strength */
    /* 0x024 */ s32 unk24[3];
    /* 0x030 */ Vec4 dir;    /* unit vector from node 3 to node 0x2E; also the stage light direction */
    /* 0x040 */ EftHitPose pose; /* start = node 3 at the start, cur = node 3 now (0x50), prev (0x60), vel unused */
    /* 0x080 */ EftOwner *owner;
    /* 0x084 */ EftEmitState state;
    /* 0x350 */ EftEmitNodes nodes;
    /* 0x5A0 */ EftEmitSet *set;
    /* 0x5A4 */ s32 unk5A4[3];
} EftFollowWork; /* size 0x5B0 */

void EftEmit_SpawnType9(EftEmitSet *set, EftEmitHandles *handles, s32 flags, s32 arg3, s32 objId, s32 idx, f32 size,
                        f32 scale, f32 rate, Vec4 *pos, Vec4 *dir);
void EftEmit_SpawnType10(EftEmitSet *set, EftEmitHandles *handles, s32 flags, s32 arg3, s32 objId, s32 idx, f32 size,
                         f32 scale, f32 rate, Vec4 *pos, Vec4 *unused, Vec4 *dir);
void EftEmit_SpawnType15(EftEmitSet *set, EftEmitHandles *handles, s32 flags, s32 arg3, s32 objId, s32 idx, f32 size,
                         f32 scale, f32 rate, Vec4 *pos, Vec4 *pos2, Vec4 *dir);
void EftEmit_SpawnType12(EftEmitSet *set, EftEmitHandles *handles, s32 flags, s32 arg3, s32 objId, s32 idx, f32 size,
                         f32 scale, f32 rate, Vec4 *pos, Vec4 *dir);
void EftEmit_SpawnOwn(EftEmitSet *set, EftEmitState *state, EftEmitNodes *nodes, Vec4 *pos, Vec4 *dir, s32 type,
                      s32 idx, s32 flags, f32 scale);
void EftEmit_Spawn(EftEmitSet *set, EftEmitState *state, EftEmitNodes *nodes, Vec4 *pos, Vec4 *dir, s32 objId,
                   s32 node, s32 arg7, s32 type, s32 idx, s32 flags, f32 scale);
void EftEmit_MarkCond(s32 objId, s32 extra, EftEmitSet *set, EftEmitState *state);
void EftEmit_MarkKind6(EftEmitSet *set, EftEmitState *state);
void EftEmit_KillAll(EftEmitSet *set, EftEmitState *state);
s32 EftEmit_UpdateAlive(EftEmitSet *set, EftEmitState *state);
void EftEmit_UpdateNodesReq(EftOwner *owner, EftEmitNodes *nodes);
void EftEmit_UpdateNodes(EftOwner *owner, EftEmitNodes *nodes, s32 req);
void EftEmit_SetNode(EftEmitNodes *nodes, s32 slot, s32 node, Vec4 *pos);
void EftEmit_SetNodePos(EftEmitNodes *nodes, s32 slot, Vec4 *pos);
void EftEmit_RefreshFixedNodes(s32 objId, EftEmitNodes *nodes);
void EftEmit_UpdateTrailWidth(EftEmitSet *set, EftEmitState *state);
f32 EftEmit_GetTrailWidth(EftEmitState *state);
s32 EftEmit_HasWidth2(EftEmitSet *set);
void EftEmit_UpdateWidth2(EftEmitSet *set, EftEmitState *state);
f32 EftEmit_GetWidth2(EftEmitState *state);

void EftSweep_AddMark(EftTask *task);
s32 EftSweep_UpdateMarks(EftTask *task);
void EftSweep_InitPath(EftTask *task);
void EftSweep_Move(EftTask *task);
void EftSweep_AddHit(EftTask *task);
void EftSweep_Emit(s32 objId, EftTask *task, EftEmitSet *set, s32 reqOnly);
void EftSweep_Init(EftTask *task, EftOwner *owner);
void EftSweep_Term(EftTask *task);
void EftSweep_Update(EftTask *task);
void EftSweep_PostUpdate(EftTask *task);
void EftSweep_Reset(EftTask *task);
void EftSweep_Draw(EftTask *task);
void EftSweepGroup_Init(EftTask *task, EftOwner *owner);
void EftSweepGroup_Term(EftTask *task);
void EftSweepGroup_Update(EftTask *task);
void EftSweepGroup_Reset(EftTask *task);

void EftFollow_AddHit(EftTask *task);
void EftFollow_UpdateLight(EftTask *task);
void EftFollow_Emit(s32 objId, EftTask *task, EftEmitSet *set, s32 reqOnly);
void EftFollow_Init(EftTask *task, EftOwner *owner);
void EftFollow_Term(EftTask *task);
void EftFollow_Update(EftTask *task);
void EftFollow_PostUpdate(EftTask *task);

#endif
