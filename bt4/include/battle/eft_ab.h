#ifndef BATTLE_EFT_AB_H
#define BATTLE_EFT_AB_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect modules 0x19E0C0..0x1A21A8, three files:
 *   src/battle/eft_ab.c    0x19E0C0..0x1A0020  "orb tail": the aura-coloured ball a fighter holds between its hands
 *                                              during a "shots" technique (eft_k.c EftShotTech_UpdateAuraBall), with a
 *                                              chain of nodes behind it and streak sprites flowing along the chain.
 *                                              The module starts before this range (manager init at 0x19DEA8).
 *   src/battle/eft_ab_b.c  0x1A0020..0x1A0E58  transformation / fusion effect (fighter effect requests 0, 1, 2).
 *   src/battle/eft_ab_c.c  0x1A0E58..0x1A21A8  ribbon ("slash trail") helpers: the first functions of the module whose
 *                                              task classes start at 0x2C42F0 (it continues at 0x1A21A8).
 * All three are drawing only: none writes a fighter, a battle object or a hit record, with one exception in the
 * transformation effect (the fighter's draw-mask bit 3, see EftTransform_Update).
 * All struct names are local views.
 */

/* A vector passed by value: the original type is 16-byte aligned and the callee copies it with ld / sd. */
typedef struct EftAbVec {
    f32 x, y, z, w;
} __attribute__((aligned(16))) EftAbVec;

/* The part of an effect task (0x40 bytes) these modules use. */
typedef struct EftAbTask {
    /* 0x00 */ u8 flags;
    /* 0x01 */ u8 step;        /* state of the module's update switch */
    /* 0x02 */ u8 unk2[0x26];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftAbTask;

/* One entry of a texture table: the GS TEX0 value and the image / palette it was built from. */
typedef struct EftAbTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftAbTexEntry; /* 0x10 */

/* A texture table as EftTexSet_Load16 loads it: 16 entries. */
typedef struct EftAbTex {
    /* 0x000 */ EftAbTexEntry entry[16];
    /* 0x100 */ s32 count;
    /* 0x104 */ u32 ready;     /* bit n: entry n holds a TEX0 built this frame */
} EftAbTex; /* 0x108 */

/* ---- orb tail --------------------------------------------------------------------------- */

/* Parameters: common pack entry 0x25B. Ranges are {base, spread}; "A / B / C" are the values a streak has at birth,
   at the turn point and at death. */
typedef struct EftOrbTailParam {
    /* 0x00 */ f32 speedBase[3];   /* streak speed along the chain: A, B, C */
    /* 0x0C */ f32 speedRange[3];
    /* 0x18 */ f32 sizeBase[3];    /* streak size: A, B, C */
    /* 0x24 */ f32 sizeRange[3];
    /* 0x30 */ f32 turn;           /* fraction of a streak's life at which A..B ends and B..C starts */
    /* 0x34 */ f32 lifeBase;       /* seconds */
    /* 0x38 */ f32 lifeRange;
    /* 0x3C */ f32 headOffset;     /* the chain's first node sits this far in front of the ball */
    /* 0x40 */ f32 fadeInBase;     /* seconds */
    /* 0x44 */ f32 fadeInRange;
    /* 0x48 */ f32 fadeOutBase;    /* seconds */
    /* 0x4C */ f32 fadeOutRange;
    /* 0x50 */ f32 tailFade;       /* fraction of the chain length over which streaks fade out towards its end */
    /* 0x54 */ f32 nodeLen;        /* length of one chain segment */
    /* 0x58 */ f32 stiffness;      /* how much of the stretch beyond nodeLen a node gives back per frame */
    /* 0x5C */ f32 burstSize[3];   /* size of the three burst emitters */
    /* 0x68 */ f32 burstRate[3];
    /* 0x74 */ u8 nodesBase;
    /* 0x75 */ u8 nodesRange;
    /* 0x76 */ u8 streakPeriod;    /* frames between streak batches */
    /* 0x77 */ u8 streakCount;     /* streaks per batch */
    /* 0x78 */ u8 texCols;         /* animation frames per row of the streak texture */
    /* 0x79 */ u8 texRows;
    /* 0x7A */ u8 unk7A;
    /* 0x7B */ u8 otZ;             /* argument of EftPrim_DrawQuadDepthScaled */
} EftOrbTailParam;

/* Colours: common pack entry 0x25A. */
typedef struct EftOrbTailColor {
    /* 0x00 */ EftAbVec color[7];      /* by colour group (EftOrbTail.arg.kind after init: 0, 1 or 2); w = alpha */
    /* 0x70 */ f32 alphaA;         /* alpha factor of a streak: A, C; the turn value is their mean */
    /* 0x74 */ f32 alphaC;
    /* 0x78 */ f32 alphaTurn;      /* fraction of the life at which the alpha ramp turns */
} EftOrbTailColor;

/* Argument of the task's init callback. */
typedef struct EftOrbTailArg {
    /* 0x00 */ EftOrbTailParam *param;
    /* 0x04 */ EftOrbTailColor *color;
    /* 0x10 */ EftAbVec unk10;     /* (0, 0, 0, 1); never read */
    /* 0x20 */ u8 kind;            /* the fighter's aura type; the work copy is reduced to a colour group 0..2 */
    /* 0x21 */ u8 objId;
} EftOrbTailArg; /* 0x30 */

/* A chain node (pool of 128). */
typedef struct EftOrbTailNode {
    /* 0x00 */ Mtx44 mtx;          /* orientation of the segment starting here, translation = pos */
    /* 0x40 */ EftAbVec pos;
    /* 0x50 */ f32 len;            /* distance to the next node (to the previous one for the last node) */
    /* 0x54 */ f32 pitch;
    /* 0x58 */ f32 yaw;
    /* 0x5C */ s32 used;
    /* 0x60 */ struct EftOrbTailNode *next;
    /* 0x64 */ struct EftOrbTailNode *prev;
    /* 0x68 */ s32 unk68[2];
} EftOrbTailNode; /* 0x70 */

/* EftOrbTailStreak.flags */
#define EFT_ORB_STREAK_USED 0x01
#define EFT_ORB_STREAK_FADE 0x02   /* fading out */
#define EFT_ORB_STREAK_DEAD 0x08
#define EFT_ORB_STREAK_SHOW 0x10

/* A streak sprite travelling along the chain (pool of 200). */
typedef struct EftOrbTailStreak {
    /* 0x00 */ EftAbVec color;         /* w = alpha this frame */
    /* 0x10 */ EftAbVec pos;           /* world position */
    /* 0x20 */ EftAbVec local;         /* position in the current node's frame: z = distance travelled on the segment */
    /* 0x30 */ EftAbVec dir;           /* (0, 0, 1, 1) */
    /* 0x40 */ f32 speed;
    /* 0x44 */ f32 speedStepA;     /* per frame before / after the turn point */
    /* 0x48 */ f32 speedStepB;
    /* 0x4C */ f32 roll;           /* random, -pi..pi */
    /* 0x50 */ f32 size;
    /* 0x54 */ f32 sizeStepA;
    /* 0x58 */ f32 sizeStepB;
    /* 0x5C */ f32 alpha;
    /* 0x60 */ f32 alphaStepA;
    /* 0x64 */ f32 alphaStepB;
    /* 0x68 */ f32 age;            /* frames */
    /* 0x6C */ f32 life;
    /* 0x70 */ f32 dist;           /* distance travelled along the chain */
    /* 0x74 */ f32 fadeIn;
    /* 0x78 */ f32 fadeOut;        /* counts down while fading out */
    /* 0x7C */ f32 fadeInLen;
    /* 0x80 */ f32 fadeOutLen;
    /* 0x84 */ f32 frame;          /* texture animation frame */
    /* 0x88 */ s32 flags;
    /* 0x8C */ struct EftOrbTailStreak *next;
    /* 0x90 */ struct EftOrbTailStreak *prev;
    /* 0x94 */ EftOrbTailNode *node; /* start of the segment the streak is on */
    /* 0x98 */ s32 unk98[2];
} EftOrbTailStreak; /* 0xA0 */

/* EftOrbTail.flags */
#define EFT_ORB_ACTIVE   0x001
#define EFT_ORB_END      0x002     /* EftOrbTail_End: stop feeding, die when the last streak is gone */
#define EFT_ORB_KILL     0x004     /* EftOrbTail_Kill */
#define EFT_ORB_DONE     0x008
#define EFT_ORB_UNK10    0x010     /* set at init, never read */
#define EFT_ORB_CHAIN    0x020     /* the chain has at least two nodes */
#define EFT_ORB_MOVING   0x040     /* the fighter moved this frame */
#define EFT_ORB_BURST    0x080     /* EftOrbTail_Burst: start the three burst emitters */
#define EFT_ORB_ANIM     0x100     /* the streak texture is animated */
#define EFT_ORB_PLACED   0x200     /* EftOrbTail_SetPos has been called once */

/* Work block of an orb tail task (class 0x2C42A8). */
typedef struct EftOrbTail {
    /* 0x000 */ EftAbTex *tex;     /* &mgr->tex */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ EftAbTexEntry image;   /* copied from tex->entry[] */
    /* 0x018 */ EftAbTexEntry palette;
    /* 0x028 */ s32 unk28[2];
    /* 0x030 */ EftOrbTailArg arg;
    /* 0x060 */ EftAbVec pos;
    /* 0x070 */ EftAbVec prevPos;
    /* 0x080 */ EftAbVec dir;          /* unit, pos - prevPos */
    /* 0x090 */ f32 growLen;       /* chain length at which the fade ratio reaches 1 */
    /* 0x094 */ f32 grown;
    /* 0x098 */ f32 growStep;
    /* 0x09C */ f32 grow;          /* grown / growLen, 0..1 */
    /* 0x0A0 */ f32 scale;         /* fighter height / 19.35 */
    /* 0x0A4 */ f32 chainLen;      /* sum of the node lengths */
    /* 0x0A8 */ f32 frame;
    /* 0x0AC */ f32 texFrames;     /* texCols * texRows */
    /* 0x0B0 */ f32 uv[16][4];     /* u0, v0, u1, v1 of each animation frame; plain floats (4-aligned): the
                                      code adds each component's offset to the base before the index */
    /* 0x1B0 */ u8 nodeCount;
    /* 0x1B1 */ u8 nodeMax;
    /* 0x1B2 */ u8 texIdx;         /* entry of tex this task's TEX0 goes to */
    /* 0x1B3 */ u8 unk1B3;
    /* 0x1B4 */ s32 flags;
    /* 0x1B8 */ EftOrbTailNode *nodeHead;
    /* 0x1BC */ EftOrbTailNode *nodeTail;
    /* 0x1C0 */ EftOrbTailStreak *streakHead;
    /* 0x1C4 */ EftOrbTailStreak *streakTail;
    /* 0x1C8 */ void *burst[3];    /* emitter tasks of the 0x186B50 module */
    /* 0x1D4 */ s32 unk1D4[3];
} EftOrbTail; /* 0x1E0 */

#define EFT_ORB_NODES 128
#define EFT_ORB_STREAKS 200

/* The module's data (0xB960 bytes); gEftOrbTail points at a pointer to it. */
typedef struct EftOrbTailMgr {
    /* 0x0000 */ EftOrbTailParam *param;      /* common pack entry 0x25B */
    /* 0x0004 */ EftOrbTailColor *color;      /* common pack entry 0x25A */
    /* 0x0008 */ EftAbTex tex;                /* common pack entry 0x259 */
    /* 0x0110 */ EftAbTex burstTex[3];
    /* 0x0428 */ s32 *burstImage[3];
    /* 0x0434 */ s32 *burstPalette[7];        /* by colour group */
    /* 0x0450 */ EftOrbTailNode node[EFT_ORB_NODES];
    /* 0x3C50 */ EftOrbTailStreak streak[EFT_ORB_STREAKS];
    /* 0xB950 */ u32 nodeNext;                /* where the next free-node search starts */
    /* 0xB954 */ u32 streakNext;
    /* 0xB958 */ void *list;                  /* child task list: 2 tasks of 0x1E0 bytes */
    /* 0xB95C */ s32 unkB95C;
} EftOrbTailMgr; /* 0xB960 */

typedef struct EftOrbTailRef {
    EftOrbTailMgr *mgr;
} EftOrbTailRef;

extern EftOrbTailRef *gEftOrbTail;
extern void *gEftOrbTailClass[6];

void EftOrbTailMgr_Update(void);
void EftOrbTailMgr_Reset(void);
void EftOrbTailMgr_Term(void);
void EftOrbTail_Init(EftAbTask *task, EftOrbTailArg *arg);
void EftOrbTail_Update(EftAbTask *task);
void EftOrbTail_PostUpdate(void);
void EftOrbTail_Draw(EftAbTask *task);
void EftOrbTail_Reset(EftAbTask *task);
void EftOrbTail_Term(EftAbTask *task);
void EftOrbTail_AddNode(EftOrbTail *w);
void EftOrbTail_SetNodePos(EftOrbTailNode *node, EftOrbTail *w, EftAbVec pos);
void EftOrbTail_UpdateNodes(EftOrbTail *w);
EftOrbTailNode *EftOrbTail_AllocNode(void);
void EftOrbTail_AddStreak(EftOrbTail *w);
void EftOrbTail_InitStreak(EftOrbTailStreak *p, EftOrbTail *w);
void EftOrbTail_UpdateStreaks(EftOrbTail *w);
void EftOrbTail_DrawStreaks(EftOrbTail *w);
EftOrbTailStreak *EftOrbTail_AllocStreak(void);
void EftOrbTail_StartBurst(EftOrbTail *w);
void EftOrbTail_PollBurst(EftOrbTail *w);
void EftOrbTail_StopBurst(EftOrbTail *w);
void EftOrbTail_InitFrames(EftOrbTail *w);
void EftOrbTail_SetTex(EftOrbTail *w, EftAbTex *tex, s32 image, s32 palette);
void EftOrbTail_UpdateTex(EftOrbTail *w);
EftAbTask *EftOrbTail_Create(s32 objId, s32 kind);
s32 EftOrbTail_Burst(EftAbTask *task);
s32 EftOrbTail_End(EftAbTask *task);
s32 EftOrbTail_Kill(EftAbTask *task);
void EftOrbTail_SetPos(EftAbTask *task, EftAbVec pos);
s32 EftOrbTail_IsActive(EftAbTask *task);
s32 EftOrbTail_Stub0(void);
s32 EftOrbTail_Stub1(void);
s32 EftOrbTail_Stub2(void);
void EftOrbTail_Stub3(void);
void EftOrbTail_Stub4(void);

/* ---- transformation effect ---------------------------------------------------------------- */

/* An emitter set of the effect pack library (eft_h.h / eft_l.h EftModel); only what this file reads. */
typedef struct EftAbPartDef {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 count;            /* emitters in the group */
} EftAbPartDef;

typedef struct EftAbPart {
    /* 0x0 */ EftAbPartDef *def;
    /* 0x4 */ s32 unk4[3];
} EftAbPart; /* 0x10 */

typedef struct EftAbSet {
    /* 0x000 */ u32 *mask;         /* bit n: group n exists */
    /* 0x004 */ s32 unk4[2];
    /* 0x00C */ EftAbPart part[0x13];
    /* 0x13C */ u8 unk13C[0x320 - 0x13C];
} EftAbSet; /* 0x320 */

/* The request the fighter effect layer passes (btl_char_member.h BtlMemberFx0Req). */
typedef struct EftTransformArg {
    /* 0x0 */ s32 objId;
    /* 0x4 */ s32 kind;            /* 0..10, from the fighter's change kind (fighter +0x12DC) */
} EftTransformArg;

/* EftTransform.flags */
#define EFT_TF_END       0x001     /* EftTransform_End: run the emitters' end frames, then die */
#define EFT_TF_DETACHED  0x002     /* EftTransform_End: the manager no longer points at this task */
#define EFT_TF_DEAD      0x004
#define EFT_TF_ALIVE     0x008     /* some emitter is still alive (EftEmit_UpdateAlive) */
#define EFT_TF_FLASH     0x010     /* EftTransform_Flash: the model swap happens now */
#define EFT_TF_AFTER     0x020     /* the delay after the flash has passed */
#define EFT_TF_AFTER_ON  0x040     /* the "after" emitter group has been started */
#define EFT_TF_ATTACHED  0x080     /* kind 6: follow node 0x36 */
#define EFT_TF_TINT      0x100     /* the stage tint is on and the fighter is hidden */
#define EFT_TF_SPRITE    0x200     /* kind 5: draw the stage sprite at the fighter (toggled by attribute 0x4000) */
#define EFT_TF_KILLED    0x400     /* reset callback ran */
#define EFT_TF_JOINED    0x800     /* kinds 2 / 3: the attribute 0x1000 of the fighter or its partner was seen */

/* Work block of the transformation effect task (class 0x2C42D8). */
typedef struct EftTransform {
    /* 0x000 */ EftAbVec pos;      /* where the emitters spawn */
    /* 0x010 */ EftAbVec dir;      /* (0, -1, 0): up; kind 6 takes it from a node */
    /* 0x020 */ EftTransformArg arg;
    /* 0x028 */ EftAbSet *set;
    /* 0x02C */ u8 state[0x2D4];   /* emitter state (EftEmit_InitState) */
    /* 0x300 */ u8 nodes[0x250];   /* node slots (EftEmit_SetNode) */
    /* 0x550 */ s32 flags;
    /* 0x554 */ f32 scale;         /* fighter height / 19.35, at least 0.5 */
    /* 0x558 */ s32 groups;        /* node slots bound this frame; cleared in the post-update */
    /* 0x55C */ f32 endTimer;
    /* 0x560 */ f32 endFrames;     /* EftEmit_GetEndFrames */
    /* 0x564 */ f32 timer;         /* frames since the flash */
    /* 0x568 */ f32 delay;         /* 10 frames */
    /* 0x56C */ s32 unk56C;
} EftTransform; /* 0x570 */

#define EFT_TF_SETS 8

/* gEftTransform, 0x1908 bytes. */
typedef struct EftTransformMgr {
    /* 0x0000 */ EftAbSet set[EFT_TF_SETS];   /* common effect packs 0x192, 0x1AA, 0x1C0, 0x1C1, 0x1CB, 0x1E6, 0x1FC, 0x217 */
    /* 0x1900 */ EftAbTask *task;             /* the running effect */
    /* 0x1904 */ s32 busy;
} EftTransformMgr; /* 0x1908 */

extern EftTransformMgr *gEftTransform;
extern void *gEftTransformList;
extern void *gEftTransformClass[6];

void EftTransform_SpawnParts(s32 objId, EftAbTask *task, EftAbSet *set);
EftAbSet *EftTransform_GetSet(s32 kind);
s32 EftTransform_HasStartGroup(s32 kind);
void EftTransform_UpdateNodes(EftAbTask *task, s32 kind);
void EftTransform_OnFlash(EftAbTask *task, s32 kind);
void EftTransform_Init(EftAbTask *task, EftTransformArg *req);
void EftTransform_Term(EftAbTask *task);
void EftTransform_Update(EftAbTask *task);
void EftTransform_Reset(EftAbTask *task);
void EftTransform_PostUpdate(EftAbTask *task);
void EftTransform_Draw(EftAbTask *task);
void EftTransformMgr_Init(EftAbTask *task);
void EftTransformMgr_Term(void);
void EftTransformMgr_Update(void);
s32 EftTransform_Start(EftTransformArg *req);
s32 EftTransform_Flash(void);
s32 EftTransform_End(void);

/* ---- ribbon helpers (the module continues in the file after this one, eft_ac) ---------------- */

/* The vector type with its array view (the key loaders index the components). */
typedef union EftRbnVec {
    struct {
        f32 x, y, z, w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftRbnVec;

/* Parameter block of a ribbon in the effect pack. "[3]" = the value at the three keys of the animation. */
typedef struct EftRbnPrm {
    /* 0x00 */ s32 flags;          /* 1 animated, 2 random mirror, 4 scroll (second layer) */
    /* 0x04 */ u8 blend;           /* second argument of EftGfx_DrawPolyScaledZ */
    /* 0x05 */ u8 kind;            /* 0 strip between two points, 1 and 2 trails */
    /* 0x06 */ u8 unk6[2];
    /* 0x08 */ f32 fadeIn;         /* seconds */
    /* 0x0C */ f32 fadeOut;        /* seconds */
    /* 0x10 */ f32 animTime;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 width[3];
    /* 0x24 */ f32 widthLo[3];     /* width pulse: low and high value */
    /* 0x30 */ f32 widthHi[3];
    /* 0x3C */ f32 widthTime[3];   /* seconds */
    /* 0x48 */ f32 alpha[3];       /* only with flag 4 */
    /* 0x54 */ f32 scroll[3];      /* only with flag 4 */
} EftRbnPrm;

/* The second data block of a ribbon (EftRbnArg.anim): colours and the colour pulse at the three keys. */
typedef struct EftRbnAnim {
    /* 0x00 */ EftRbnVec color[3];
    /* 0x30 */ EftRbnVec unk30[3];
    /* 0x60 */ f32 pulseR[3][2];   /* low and high value of the pulse, per component */
    /* 0x78 */ f32 pulseG[3][2];
    /* 0x90 */ f32 pulseB[3][2];
    /* 0xA8 */ f32 pulseTime[3];   /* seconds */
} EftRbnAnim;

/* Argument of the ribbon task (eft_h.h EftEmitArg17). */
typedef struct EftRbnArg {
    /* 0x00 */ EftRbnVec pos;
    /* 0x10 */ EftRbnVec dir;
    /* 0x20 */ s32 type;
    /* 0x24 */ s32 chr;
    /* 0x28 */ s32 texBase;        /* first entry of tex this ribbon's TEX0 values go to */
    /* 0x2C */ f32 life;
    /* 0x30 */ f32 size;
    /* 0x34 */ EftAbTex *tex;
    /* 0x38 */ EftRbnPrm *prm;
    /* 0x3C */ EftRbnAnim *anim;
} EftRbnArg; /* 0x40 */

/* Animated values, filled from one key (EftRibbon_LoadKey) or between two (EftRibbon_UpdateKeys). */
typedef struct EftRbnCur {
    /* 0x00 */ EftRbnVec color;
    /* 0x10 */ f32 pulse[3];       /* colour pulse: low value per component */
    /* 0x1C */ f32 pulseAmp[3];    /* high - low */
    /* 0x28 */ f32 pulseTime;      /* frames */
    /* 0x2C */ f32 width;
    /* 0x30 */ f32 widthBase;
    /* 0x34 */ f32 widthAmp;
    /* 0x38 */ f32 widthTime;      /* frames */
    /* 0x3C */ f32 alpha;
    /* 0x40 */ f32 scroll;
    /* 0x44 */ s32 unk44[3];
} EftRbnCur; /* 0x50 */

/* A node (pool of 500 in gEftRibbonMgr). */
typedef struct EftRbnNode {
    /* 0x00 */ s32 flags;          /* 0 = free */
    /* 0x04 */ u8 tex;             /* which of the ribbon's textures the segment starting here uses */
    /* 0x05 */ u8 unk5[0xB];
    /* 0x10 */ EftRbnVec pos;
    /* 0x20 */ EftRbnVec color;
    /* 0x30 */ EftRbnVec color2;   /* second layer (scrolling ribbons) */
    /* 0x40 */ struct EftRbnNode *next;
    /* 0x44 */ struct EftRbnNode *prev;
    /* 0x48 */ s32 unk48[2];
} EftRbnNode; /* 0x50 */

#define EFT_RBN_NODES 500

/* Work block of a ribbon task (include/battle/eft_ac.h EftRibbon has the fields the rest of the module uses). */
typedef struct EftRbn {
    /* 0x000 */ EftRbnNode *head;
    /* 0x004 */ EftRbnNode *tail;
    /* 0x008 */ s32 unk8[2];
    /* 0x010 */ EftRbnArg arg;
    /* 0x050 */ EftRbnCur cur;
    /* 0x0A0 */ EftRbnVec end;     /* second point */
    /* 0x0B0 */ EftRbnVec color;   /* w is cleared and compared as a zero at init */
    /* 0x0C0 */ EftRbnVec color2;
    /* 0x0D0 */ s32 flags;         /* 0x20 scroll layer, 0x40 mirrored, 0x80 faded in, 0x100 no fade out */
    /* 0x0D4 */ s32 unkD4[2];
    /* 0x0DC */ s32 maxNodes;
    /* 0x0E0 */ s32 unkE0;
    /* 0x0E4 */ s32 numNodes;
    /* 0x0E8 */ f32 unkE8[4];
    /* 0x0F8 */ f32 animFrame;
    /* 0x0FC */ f32 animTime;
    /* 0x100 */ f32 animSplit;     /* frame of the middle key */
    /* 0x104 */ f32 width;
    /* 0x108 */ f32 unk108[3];
    /* 0x114 */ f32 fadeOutFrame;
    /* 0x118 */ f32 fadeInTime;
    /* 0x11C */ f32 fadeOutTime;
    /* 0x120 */ f32 alpha;
    /* 0x124 */ f32 scroll;
    /* 0x128 */ f32 scrollPos;
    /* 0x12C */ s32 numTex;
    /* 0x130 */ EftAbTexEntry frame[4][2]; /* image and palette entry of each texture */
    /* 0x1B0 */ u64 tex0[6];       /* TEX0 of each texture; [3] is the scroll layer's */
} EftRbn; /* 0x1E0 */

/* gEftRibbonMgr. */
typedef struct EftRbnMgr {
    /* 0x0 */ EftRbnNode *nodes;   /* 500 nodes */
    /* 0x4 */ s32 next;            /* where the next free-node search starts */
} EftRbnMgr;

extern EftRbnMgr *gEftRibbonMgr;

EftRbnNode *EftRibbon_AllocNode(EftRbn *w);
void EftRibbon_FreeNodes(EftRbn *w);
void EftRibbon_SetTexPair(EftRbn *w, s32 slot, EftAbTexEntry *tex, s32 image, s32 palette);
void EftRibbon_UpdateTex(EftRbnPrm *prm, EftRbn *w, EftRbnArg *arg);
void EftRibbon_LoadKey(EftRbnCur *cur, EftRbnArg *arg, s32 key);
void EftRibbon_UpdateKeys(EftRbn *w);
s32 EftRibbon_AddNode(EftRbn *w, EftRbnVec *pos);
void EftRibbon_InitNodes(EftRbn *w, EftRbnArg *arg);
void EftRibbon_PlaceStrip(EftRbnPrm *prm, EftRbn *w, EftRbnVec *start);
void EftRibbon_PlaceTrail1(EftRbnPrm *prm, EftRbn *w, EftRbnArg *arg);
void EftRibbon_PlaceTrail2(EftRbnPrm *prm, EftRbn *w, EftRbnArg *arg);
void EftRibbon_DrawStrip(EftRbn *w, EftRbnArg *arg, EftRbnPrm *prm);

#endif
