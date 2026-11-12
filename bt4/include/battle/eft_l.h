#ifndef BATTLE_EFT_L_H
#define BATTLE_EFT_L_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect modules 0x15B550..0x15F728 (53 functions):
 *   src/battle/eft_k.c (second part; formerly eft_l.c)    0x15B550..0x15C728  technique effect type 9 "rush shot": the rest of the module whose first
 *                                             three helpers are the end of eft_k.c (same translation unit, see the
 *                                             note at EftRushShot_UpdateAttached)
 *   src/battle/eft_l_b.c  0x15C728..0x15E5D0  technique effect type 4 "ring shot": up to 20 blast objects per task
 *   src/battle/eft_l_c.c  0x15E5D0..0x15EF18  absorb / drain glow (fighter effect requests 0x37 and 0x38)
 *   src/battle/eft_l_d.c  0x15EF18..0x15F728  the two spawners at the head of the speed-line module (eft_m.c)
 *
 * "Technique effect type" = definition type; gEftShotClass (0x2C3700) row type + 1 holds {manager class, task class}:
 * 0x2C373C {0x2C39B8, 0x2C39D0} is the ring shot, 0x2C3778 {0x2C3988, 0x2C39A0} the rush shot. A class is six
 * callbacks in the order update, init, term, postUpdate, reset, draw (BtlTaskClass).
 *
 * Every structure here is a partial view local to these files. The same things have other local names in the
 * neighbouring headers: EftTechArg / EftTechDef are eft_i.h's EftOwner / EftOwnerParam, EftModel / EftModelInst its
 * EftEmitSet / EftEmitState (an "emitter set": 19 groups of particle emitters loaded from the technique's pack).
 */

/* A 16-byte aligned vector: what the original used inside argument blocks (they are copied 8 bytes at a time). */
typedef struct EftVec {
    f32 x, y, z, w;
} __attribute__((aligned(16))) EftVec;

/* A task of the effect scene's task tree (0x40 bytes); only what these modules touch. */
typedef struct EftTask {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 step;      /* state of the module's update switch */
    /* 0x02 */ u16 unk2;
    /* 0x04 */ u32 flags;    /* 0x800 / 0x1000: belongs to character 0 / 1 (BtlTask_SetOwnerTag). Written by the hit record
                                code: 1 = the record hit something, 2 = it is over, 4 = (also ends the effect) */
    /* 0x08 */ u16 hit;      /* written by the hit record code: 1 = the volume was moved (hitPos is its new head),
                                4 = start the end timer */
    /* 0x0A */ u16 unkA;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ Vec4 hitPos;  /* head position written back by the hit record code */
    /* 0x20 */ u8 unk20[0x38 - 0x20];
    /* 0x38 */ void *work;   /* work area of the size given to BtlTask_CreateChildList */
    /* 0x3C */ s32 unk3C;
} EftTask; /* size 0x40 */

/* Definition of a technique effect (the record a technique's effect request points at); fields used here. */
typedef struct EftTechDef {
    /* 0x00 */ s16 id;        /* effect id; selects the variant */
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s8 unk4;       /* not 0: the term callback sets the fighter's held flag 0xA8 (rush shot) / 0xA9
                                 (ring shot) */
    /* 0x05 */ u8 unk5[3];
    /* 0x08 */ s8 hitShape;   /* 0: two spheres (ColSphere_Set, EftHit_SetShapeSpheres), 1: two boxes
                                 (ColCapsule_Set, EftHit_SetShapeBoxes) */
    /* 0x09 */ s8 unk9;       /* multiplied by count: total number of shots */
    /* 0x0A */ u8 unkA[0x28 - 0xA];
    /* 0x28 */ s32 unk28;     /* frames; / 30 is the blast objects' delay in variant 2 */
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ f32 size;      /* effect scale; times the set's trail width = hit radius */
    /* 0x34 */ f32 speed;
    /* 0x38 */ f32 unk38;     /* > 0: the rush shot homes (EftAim_Home turn rate) */
    /* 0x3C */ u32 flags;     /* 1 makes hit records, 2 the shot does not move by itself, 0x200 lights the stage
                                 (blur), 0x20000 has swarm models, 0x400000 no screen flash */
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s16 count;     /* shots per volley */
    /* 0x46 */ u8 unk46[0x58 - 0x46];
    /* 0x58 */ s8 unk58[8];   /* kind of the effect EftStreak_Start starts at the n-th event 0x80; < 0 none */
    /* 0x60 */ f32 unk60[8];  /* its angle in degrees */
    /* 0x80 */ s16 unk80;
} EftTechDef;

/* Argument of the init callback of a technique effect task. */
typedef struct EftTechArg {
    /* 0x00 */ s32 objId;     /* owner */
    /* 0x04 */ s32 slot;      /* shot slot: 0, 1 ki blasts, 2..4 techniques */
    /* 0x08 */ u8 unk8[0x1C - 0x8];
    /* 0x1C */ s32 *pack;     /* the technique's effect pack */
    /* 0x20 */ s32 side;
    /* 0x24 */ EftTechDef *def;
} EftTechArg;

/* Per-task state of an emitter set (EftEmit_InitState). */
typedef struct EftModelInst {
    u8 unk0[0x2CC];
} EftModelInst;

/* An emitter set (EftEmit_LoadSet): 0x13 groups. */
typedef struct EftModelPartDef {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 count;       /* emitters in the group */
} EftModelPartDef;

typedef struct EftModelNode {
    /* 0x00 */ u8 unk0[9];
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 unkA[0x18 - 0xA];
    /* 0x18 */ f32 unk18;
    /* 0x1C */ u8 unk1C[0x40 - 0x1C];
} EftModelNode; /* size 0x40 */

typedef struct EftModelPart {
    /* 0x0 */ EftModelPartDef *def;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ u8 first;       /* index of the group's first emitter in nodes[] */
    /* 0x9 */ u8 unk9[7];
} EftModelPart; /* size 0x10 */

typedef struct EftModel {
    /* 0x000 */ u32 *mask;    /* bit n: group n exists */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ EftModelNode *nodes;
    /* 0x00C */ EftModelPart part[0x13];
    /* 0x13C */ u8 unk13C[0x31C - 0x13C];
} EftModel; /* size 0x31C */

/* ---- eft_l.c: technique effect type 9, the rush shot ---- */

/* Work of the manager task (class gEftRushShotMgrClass), 0x330 bytes. */
typedef struct EftRushShotMgrWork {
    /* 0x000 */ EftModel model;
    /* 0x31C */ s32 unk31C;
    /* 0x320 */ s32 *sub[4];   /* [0]: entry 1 of the technique's pack (the swarm's model pack) when the definition
                                  has flag 0x20000; [1..3]: entries 1..3 of the character's pack 8 when not empty:
                                  the camera animations of technique slots 2..4 */
} EftRushShotMgrWork; /* size 0x330 */

/* One model of the swarm (eft_k.h EftRushShotObj). */
typedef struct EftRushShotModel {
    /* 0x00 */ u8 unk0[0x35];
    /* 0x35 */ s8 handle;     /* model object, freed with EftObj_Destroy */
    /* 0x36 */ u8 unk36[0xB0 - 0x36];
} EftRushShotModel; /* size 0xB0 */

/* EftRushShotWork.flags */
#define EFT_RUSHSHOT_COUNT      0x1       /* the life timer runs */
#define EFT_RUSHSHOT_END        0x2       /* ending */
#define EFT_RUSHSHOT_KILL       0x4       /* kill the task */
#define EFT_RUSHSHOT_FAST_END   0x8       /* event 0x400 before the normal end: kill without waiting */
#define EFT_RUSHSHOT_ENDED      0x10      /* event 8 seen */
#define EFT_RUSHSHOT_FIRED      0x20      /* in flight: makes hit records */
#define EFT_RUSHSHOT_HIT_MOVED  0x40      /* the hit record code moved the shot this frame */
#define EFT_RUSHSHOT_STARTED    0x80      /* event 2 or 4 seen */
#define EFT_RUSHSHOT_RESET      0x100
#define EFT_RUSHSHOT_SCALED     0x200     /* speed follows the set's second width animation */
#define EFT_RUSHSHOT_FADE       0x400     /* stage blur fading in */
#define EFT_RUSHSHOT_ATTACHED   0x800     /* follows the victim */
#define EFT_RUSHSHOT_ATTACHED2  0x1000
#define EFT_RUSHSHOT_CONNECTED  0x10000   /* the rush connected */
#define EFT_RUSHSHOT_SUB        0x20000   /* the swarm models are updated */
#define EFT_RUSHSHOT_FLIGHT     0x40000   /* released (the swarm follows the shot) */
#define EFT_RUSHSHOT_SENT       0x80000   /* the camera cut was started */
#define EFT_RUSHSHOT_SENT2      0x100000  /* the camera cut was stopped */
#define EFT_RUSHSHOT_STAGE_FX   0x200000  /* a EftStreak_Start effect is running */

/* Work of a projectile task (class gEftRushShotClass), memset to 0xAC0 bytes. */
typedef struct EftRushShotWork {
    /* 0x000 */ Vec4 dir;
    /* 0x010 */ Vec4 fadePos;   /* aim direction handed to StgBlur_SetCenter */
    /* 0x020 */ Vec4 start;     /* 0x20..0x5F is the block copied into the hit record (rec + 0x10) */
    /* 0x030 */ Vec4 pos;       /* head of the hit volume */
    /* 0x040 */ Vec4 prev;      /* tail of the hit volume: where the head was a frame ago */
    /* 0x050 */ Vec4 vel;
    /* 0x060 */ EftTechArg *arg;
    /* 0x064 */ EftModelInst inst;
    /* 0x330 */ u8 unk330[0x20]; /* node slots of the set (EftEmit_UpdateNodesReq), 0x250 bytes from here */
    /* 0x350 */ u8 unk350[0x40];
    /* 0x390 */ Vec4 unk390;    /* node slots + 0x60: position of the start node */
    /* 0x3A0 */ u8 unk3A0[0x580 - 0x3A0];
    /* 0x580 */ EftModel *model;
    /* 0x584 */ u32 flags;
    /* 0x588 */ s32 type;      /* 1 for effect id 0x19D */
    /* 0x58C */ f32 speed;
    /* 0x590 */ f32 size;
    /* 0x594 */ f32 drawSize;
    /* 0x598 */ f32 size2;
    /* 0x59C */ f32 unk59C;
    /* 0x5A0 */ f32 timer;
    /* 0x5A4 */ f32 life;
    /* 0x5A8 */ f32 fade;
    /* 0x5AC */ void *stageFx[8]; /* game pointers returned by EftStreak_Start */
    /* 0x5CC */ s32 stage;      /* how many events 0x80 were seen */
    /* 0x5D0 */ s32 subCount;   /* 7 when the manager has a model pack */
    /* 0x5D4 */ u8 unk5D4[0x5E0 - 0x5D4];
    /* 0x5E0 */ EftRushShotModel sub[7];
    /* 0xAB0 */ s32 *unkAB0;    /* camera animation: mgr->sub[slot - 1] for slots 2..4 */
    /* 0xAB4 */ u8 unkAB4[0xAC0 - 0xAB4];
} EftRushShotWork; /* size 0xAC0 */

/* ---- eft_l_b.c: technique effect type 4, the ring shot ---- */

/* Work of the manager task (class gEftRingShotMgrClass), 0x324 bytes. */
typedef struct EftRingShotMgr {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ EftModel model;
    /* 0x320 */ s32 unk320;
} EftRingShotMgr; /* size 0x324 */

/* One shot. */
typedef struct EftRingShotOne {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 dir;
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 home;      /* its place on the ring */
    /* 0x40 */ Vec4 from;      /* place (with the bob offset) minus EftRingShot.start: slide vector */
    /* 0x50 */ u32 flags;      /* 1 in use, 2 arrived, 4 last of the volley */
    /* 0x54 */ s32 index;
    /* 0x58 */ f32 phase;      /* bobbing phase */
    /* 0x5C */ f32 time;       /* frames since it was created */
    /* 0x60 */ f32 slideTime;  /* frames the slide takes: |from| / def->speed, at least 10 (variant 0) or 1 */
    /* 0x64 */ void *shot;     /* the blast object EftBlastObj_Create created (task of class 0x2C3AD8, eft_j.c) */
    /* 0x68 */ u8 unk68[8];
} EftRingShotOne; /* size 0x70 */

/* Work of a ring shot task (class gEftRingShotClass), 0xEC0 bytes. */
typedef struct EftRingShot {
    /* 0x000 */ EftModelInst inst;
    /* 0x2CC */ s32 unk2CC;
    /* 0x2D0 */ u8 unk2D0[0x20]; /* node set (EftEmit_UpdateNodesReq): 0x250 bytes from here */
    /* 0x2F0 */ u8 unk2F0[0x30];
    /* 0x320 */ s32 node;        /* node set +0x50: the fighter node the shots start from */
    /* 0x324 */ u8 unk324[0xC];
    /* 0x330 */ Vec4 unk330;     /* node set +0x60: that node's position */
    /* 0x340 */ u8 unk340[0x520 - 0x340];
    /* 0x520 */ EftModel *model;
    /* 0x524 */ EftTechArg *arg;
    /* 0x528 */ u8 unk528[8];
    /* 0x530 */ Vec4 start;      /* 0x530..0x56F is the block copied into the hit record (rec + 0x10) */
    /* 0x540 */ Vec4 pos;        /* head of the hit volume: the carrier point (variant 2), opponent node 3 (variant 3) */
    /* 0x550 */ Vec4 prev;       /* tail of the hit volume */
    /* 0x560 */ Vec4 vel;
    /* 0x570 */ Vec4 dir;        /* aim direction (EftAim_GetDirKeep) */
    /* 0x580 */ Vec4 path0;      /* variant 2: the carrier's three-point spline, start */
    /* 0x590 */ Vec4 path1;      /* middle: half way, raised by scale * 10 + 40 */
    /* 0x5A0 */ Vec4 path2;      /* end: opponent node 0x11, raised by scale * 25 + 145 */
    /* 0x5B0 */ Vec4 path3;      /* copy of path2 */
    /* 0x5C0 */ u32 flags;
    /* 0x5C4 */ s32 type;        /* variant 0..3, from the effect id (see eft_l_b.c) */
    /* 0x5C8 */ f32 speed;       /* def->speed: speed of the blast objects, and of the slide to the ring */
    /* 0x5CC */ f32 unk5CC;      /* def->unk38, not read here */
    /* 0x5D0 */ f32 size;
    /* 0x5D4 */ f32 drawSize;    /* scale handed to the emitters and to the blast objects */
    /* 0x5D8 */ f32 hitSize;     /* times the set's trail width = hit radius */
    /* 0x5DC */ f32 unk5DC;
    /* 0x5E0 */ f32 timer;       /* frames since the end began */
    /* 0x5E4 */ f32 life;        /* EftEmit_GetEndFrames: frames the end takes */
    /* 0x5E8 */ f32 pathTime;    /* variant 2: frames along the spline, 0..20 */
    /* 0x5EC */ f32 unk5EC;
    /* 0x5F0 */ EftRingShotOne shot[20];
    /* 0xEB0 */ s32 fired;       /* blast objects created so far */
    /* 0xEB4 */ s32 total;       /* def->unk9 * def->count */
    /* 0xEB8 */ u8 unkEB8[8];
} EftRingShot; /* size 0xEC0 */

/* EftRingShot.flags */
#define EFT_RINGSHOT_COUNT     0x1       /* the end timer runs */
#define EFT_RINGSHOT_KILL      0x2       /* kill the task once no blast object is left */
#define EFT_RINGSHOT_FAST_END  0x4       /* event 0x400 before the normal end */
#define EFT_RINGSHOT_ENDED     0x8       /* event 8 seen */
#define EFT_RINGSHOT_FIRED     0x10      /* event 4 seen in step 0: hit records may be made */
#define EFT_RINGSHOT_HIT_MOVED 0x20      /* the hit record code moved the volume this frame */
#define EFT_RINGSHOT_STARTED   0x40      /* event 2 or 4 seen */
#define EFT_RINGSHOT_RESET     0x80      /* reset callback ran: every emitter gets the stop flag */
#define EFT_RINGSHOT_SHOT      0x100     /* at least one shot was created */
#define EFT_RINGSHOT_LAUNCHED  0x200     /* variants 0, 1: the ring was launched */
#define EFT_RINGSHOT_DONE      0x400     /* the fighter's held flag 0xA8 was set (ring complete / path flown / fixed) */
#define EFT_RINGSHOT_PATH      0x800     /* variant 2: the spline was laid */

/* Argument block of EftBlastObj_Create, which creates one blast object (eft_j.h EftJShotArg). */
typedef struct EftShotArg {
    /* 0x00 */ EftTechArg *arg;
    /* 0x04 */ EftModel *model;
    /* 0x08 */ void *unk8;
    /* 0x0C */ Vec4 *pos;
    /* 0x10 */ Vec4 *dir;
    /* 0x14 */ s32 index;
    /* 0x18 */ s32 total;
    /* 0x1C */ s32 unk1C;      /* 1 for the ring variants, 3 for the volley */
    /* 0x20 */ s32 unk20;      /* the same value */
    /* 0x24 */ f32 delay;      /* seconds; def->unk28 / 30 for the volley */
    /* 0x28 */ f32 size;
    /* 0x2C */ f32 speed;
} EftShotArg; /* size 0x30 */

/* ---- eft_l_c.c: absorb / drain glow ---- */

/* Argument of EftAbsorb_Start / EftAbsorb_StartHands: the first 8 bytes of the fighter layer's FxArg2. */
typedef struct EftAbsorbOwner {
    /* 0x0 */ s32 objId;
    /* 0x4 */ f32 scale;
} EftAbsorbOwner; /* size 0x8 */

/* Argument of the task's init callback. */
typedef struct EftAbsorbArg {
    /* 0x0 */ EftAbsorbOwner owner;
    /* 0x8 */ s32 hands;      /* 0: one glow at node 0x36, 1: one at each hand */
} EftAbsorbArg; /* size 0xC */

/* Work of the manager task (class gEftAbsorbMgrClass): one pointer. */
typedef struct EftAbsorbMgr {
    /* 0x0 */ EftModel *model;   /* 0x324 bytes from the pool; +0x320 is its pack */
} EftAbsorbMgr;

typedef struct EftAbsorbModel {
    /* 0x000 */ EftModel model;
    /* 0x31C */ s32 unk31C;
    /* 0x320 */ s32 *pack;
} EftAbsorbModel; /* size 0x324 */

/* EftAbsorb.flags */
#define EFT_ABSORB_ACTIVE   0x1   /* set at init; EftAbsorb_Stop needs it */
#define EFT_ABSORB_END      0x2   /* ending: the emitters get the stop flag and the timer runs */
#define EFT_ABSORB_KILL     0x4   /* kill the task */
#define EFT_ABSORB_NEW      0x8   /* a particle of the first set is alive */
#define EFT_ABSORB_POSTED   0x10  /* the post-update callback has run once */
#define EFT_ABSORB_RESET    0x20  /* reset callback ran */

/* Work of a glow task (class gEftAbsorbClass), 0x860 bytes. */
typedef struct EftAbsorb {
    /* 0x000 */ Vec4 pos[2];     /* node 0x36, or nodes 0x15 and 0x23 (the hands) */
    /* 0x020 */ Vec4 dir[2];     /* (0, -1, 0), or from nodes 0x14 / 0x22 towards the hands */
    /* 0x040 */ EftAbsorbOwner owner;
    /* 0x048 */ EftModel *model; /* the manager's emitter set */
    /* 0x04C */ EftModelInst inst[2];
    /* 0x5E4 */ u8 unk5E4[0x5F0 - 0x5E4];
    /* 0x5F0 */ u8 unk5F0[0x840 - 0x5F0]; /* node slots of the set */
    /* 0x840 */ s32 hands;       /* EftAbsorbArg.hands */
    /* 0x844 */ u32 flags;       /* EFT_ABSORB_* */
    /* 0x848 */ u32 unk848;      /* phase mask handed to EftEmit_GetFlagsFromMask: 1 (node 0x36) or 2 (hands), set
                                    every update, cleared by the post-update */
    /* 0x84C */ f32 scale;       /* owner.scale */
    /* 0x850 */ f32 timer;       /* frames since the end began */
    /* 0x854 */ f32 life;        /* EftEmit_GetEndFrames */
    /* 0x858 */ u8 unk858[8];
} EftAbsorb; /* size 0x860 */

/* eft_l.c */
void EftRushShot_FreeModels(EftTask *task);
void EftRushShot_StartCam(EftTask *task);
void EftRushShot_UpdateFade(EftTask *task);
void EftRushShot_UpdateStageFx(EftTask *task);
void EftRushShot_UpdateFlash(EftTask *task);
void EftRushShot_UpdateSparks(EftTask *task);
void EftRushShot_Draw(s32 objId, EftTask *task, EftModel *model);
void EftRushShot_UpdateAttached(EftTask *task);
void EftRushShot_Init(EftTask *task, EftTechArg *arg);
void EftRushShot_Term(EftTask *task);
void EftRushShot_Update(EftTask *task);
void EftRushShot_PostUpdate(EftTask *task);
void EftRushShot_Reset(EftTask *task);
void EftRushShot_Nop(void);
void EftRushShotMgr_Init(EftTask *task, EftTechArg *arg);
void EftRushShotMgr_Term(EftTask *task);
void EftRushShotMgr_Update(EftTask *task);
void EftRushShotMgr_Reset(void);

/* eft_l_b.c */
EftRingShotOne *EftRingShot_AllocShot(EftRingShot *w);
void EftRingShot_Fire(s32 objId, EftTask *task, s32 node, s32 arg3, s32 volley);
void EftRingShot_BobShot(EftRingShotOne *shot);
void EftRingShot_UpdateRing(s32 objId, EftTask *task);
void EftRingShot_UpdatePath(s32 objId, EftTask *task);
void EftRingShot_UpdateFixed(s32 objId, EftTask *task);
s32 EftRingShot_ReapShots(EftTask *task);
s32 EftRingShot_UpdateShots(s32 objId, EftTask *task);
void EftRingShot_AddHit(EftTask *task, s32 last);
void EftRingShot_Draw(s32 objId, EftTask *task, EftModel *model, s32 mode);
void EftRingShot_Init(EftTask *task, EftTechArg *arg);
void EftRingShot_Term(EftTask *task);
void EftRingShot_Update(EftTask *task);
void EftRingShot_PostUpdate(EftTask *task);
void EftRingShot_Reset(EftTask *task);
void EftRingShot_Nop(void);
void EftRingShotMgr_Init(EftTask *task, EftTechArg *arg);
void EftRingShotMgr_Term(EftTask *task);
void EftRingShotMgr_Update(EftTask *task);
void EftRingShotMgr_Reset(void);

/* eft_l_c.c */
void EftAbsorb_DrawOne(s32 objId, EftTask *task, EftModel *model, EftModelInst *inst, Vec4 *pos, Vec4 *dir);
void EftAbsorb_Init(EftTask *task, EftAbsorbArg *arg);
void EftAbsorb_Term(EftTask *task);
void EftAbsorb_Update(EftTask *task);
void EftAbsorb_Reset(EftTask *task);
void EftAbsorb_PostUpdate(EftTask *task);
void EftAbsorb_Nop(void);
void EftAbsorbMgr_Init(EftTask *task, s32 *arg);
void EftAbsorbMgr_Term(EftTask *task);
void EftAbsorbMgr_Update(EftTask *task);
s32 EftAbsorb_Start(EftAbsorbOwner *arg);
s32 EftAbsorb_Stop(s32 objId);
void EftAbsorb_StartHands(EftAbsorbOwner *arg);

/* eft_l_d.c */
s32 EftSpdLine_SpawnBodyTrails(s32 objId);
s32 EftSpdLine_SpawnPartStreaks(s32 objId, Vec4 *dir, s32 part);

#endif
