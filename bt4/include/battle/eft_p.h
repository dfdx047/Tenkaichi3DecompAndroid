#ifndef BATTLE_EFT_P_H
#define BATTLE_EFT_P_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Views local to src/battle/eft_p.c (0x16C2E0..0x16DCA0, the tail of the "disc" projectile module) and
 * src/battle/eft_p_b.c (0x16DCA0..0x170A50, the streak particles of the powered-up look). Everything here is a
 * partial view; fields without a use in these files are guesses.
 */

/* The effect code's vector: 16-byte aligned, copied with two 64-bit moves. */
typedef union EftPVec {
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftPVec;

/* A task of the effect scene (0x40 bytes); only what these files touch. */
typedef struct EftPTask {
    /* 0x00 */ u8 unk0[4];
    /* 0x04 */ s32 result;  /* flags written by the hit pass (EFT_TASK_* of eft_a.h); bits 0..5 end a disc */
    /* 0x08 */ u16 hit;     /* bit 0: tested by the post-update of a technique piece */
    /* 0x0A */ u8 unkA[0x1E];
    /* 0x28 */ void **cls;  /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
    /* 0x3C */ s32 unk3C;
} EftPTask; /* size 0x40 */

/* ---- disc projectile (task class D_002C3B38, work 0x4C0 bytes) ------------------------------------------- */

/* Definition and source of a technique (EftJDef / EftJSrc of eft_j.h); only what is read here. */
typedef struct EftPDef {
    /* 0x00 */ u8 unk0[0x3C];
    /* 0x3C */ s32 flags;   /* bit 0: the technique adds hit records */
} EftPDef;

typedef struct EftPSrc {
    /* 0x00 */ u8 unk0[0x24];
    /* 0x24 */ EftPDef *def;
} EftPSrc;

/* Emitter set of a technique (EftJSet of eft_j.h). */
typedef struct EftPPart {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ u8 type;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 kind;
    /* 0x0B */ u8 unkB[0x35];
} EftPPart; /* size 0x40 */

typedef struct EftPGroup {
    /* 0x00 */ u8 *info;    /* [1] part count */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 base;     /* first part of the group */
    /* 0x09 */ u8 unk9[7];
} EftPGroup; /* size 0x10 */

typedef struct EftPSet {
    /* 0x00 */ u32 *mask;   /* bit g: group g exists */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ EftPPart *parts;
    /* 0x0C */ EftPGroup grp[19];
} EftPSet;

/* One entry of a texture set (GS TEX0 value) and the set itself (32 entries, count, "stepped" mask). */
typedef struct EftPTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftPTexEntry; /* size 0x10 */

typedef struct EftPTexSet {
    /* 0x000 */ EftPTexEntry entry[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ s32 stepped; /* bit n: entry n was advanced this frame (bit 0: the whole set) */
} EftPTexSet;

/* Creation parameters of a disc (the EftJPieceArg of eft_j.h); copied whole into the work block. */
typedef struct EftDiscArg {
    /* 0x00 */ EftPSrc *src;  /* technique piece: the technique's source; else NULL */
    /* 0x04 */ EftPSet *set;  /* technique piece: its emitter set */
    /* 0x10 */ EftPVec dir;   /* flight direction (unit) */
    /* 0x20 */ EftPVec pos;   /* position; moved by the update */
    /* 0x30 */ f32 scale;     /* model scale; the hit radius is scale x 4.5 */
    /* 0x34 */ f32 speed;     /* per frame */
    /* 0x38 */ f32 life;      /* seconds in flight */
    /* 0x3C */ f32 grow;      /* seconds to reach full size */
    /* 0x40 */ f32 fade;      /* fraction of the life after which it fades out */
    /* 0x44 */ f32 turn;      /* homing: maximum turn per frame */
    /* 0x48 */ f32 bank;      /* homing: turn reduction by the vertical component */
    /* 0x4C */ f32 roll;      /* in turns */
    /* 0x50 */ f32 rollTime;  /* seconds over which the roll decays */
    /* 0x54 */ u8 texA;       /* entry of the model's texture set */
    /* 0x55 */ u8 texB;       /* second entry; also selects the hit record's level class */
    /* 0x56 */ u8 objId;      /* owning fighter */
    /* 0x57 */ u8 kind;       /* 0 held ki blast, 1 thrown ki blast, 2 held technique piece, 3 thrown from a node,
                                 4 technique piece */
    /* 0x58 */ s32 lastHit;   /* non-zero: the hit record is marked "last hit" */
    /* 0x5C */ s32 hand;      /* which hand holds it (nodes 0x14 / 0x15, else 0x22 / 0x23) */
} EftDiscArg; /* size 0x60 */

/* What a fighter passes for a ki blast (FxHitArg2 of btl_char_fx.h, 0x50 bytes); the disc keeps a copy that its
   hit records point at (EftHitRec.atk). */
typedef struct EftDiscAtk {
    /* 0x00 */ EftPVec pos;  /* direction for EftDisc_SpawnFromNode / EftDisc_Throw */
    /* 0x10 */ u8 unk10[2];
    /* 0x12 */ s16 objId;
    /* 0x14 */ s16 node;
    /* 0x16 */ s16 unk16;
    /* 0x18 */ s16 unk18;
    /* 0x1A */ u8 unk1A;
    /* 0x1B */ u8 level;     /* 2, 4, 6: texture pair 1, 2, 3 */
    /* 0x1C */ s16 life;     /* frames */
    /* 0x1E */ s16 unk1E;
    /* 0x20 */ f32 speed;
    /* 0x24 */ f32 turn;
    /* 0x28 */ f32 scale;
    /* 0x2C */ u8 unk2C[0x24];
} EftDiscAtk; /* size 0x50 */

/* Argument of EftDisc_SpawnHeld (FxHitArg of btl_char_fx.h). */
typedef struct EftDiscHeldArg {
    /* 0x00 */ EftPVec pos;
    /* 0x10 */ s32 objId;
    /* 0x14 */ s32 node;
    /* 0x18 */ s32 level;
    /* 0x1C */ f32 scale;
} EftDiscHeldArg;

/* Work block of a disc. */
typedef struct EftDisc {
    /* 0x000 */ EftPTexSet *tex;
    /* 0x008 */ EftPTexEntry texA;  /* working copies of the two texture entries */
    /* 0x018 */ EftPTexEntry texB;
    /* 0x030 */ EftDiscArg arg;
    /* 0x090 */ EftDiscAtk atk;     /* ki blast parameters (not for technique pieces) */
    /* 0x0E0 */ u8 emit[0x2D0];     /* emitter state of a technique piece */
    /* 0x3B0 */ u8 model[0x90];     /* model instance (EftMesh_Copy..); +0x48 is its texture set */
    /* 0x440 */ EftPVec color;      /* 127, 127, 127, alpha */
    /* 0x450 */ EftPVec dir;
    /* 0x460 */ EftPVec prev;       /* previous position (the first frame: the owner's node 0x11) */
    /* 0x470 */ EftPVec prev2;
    /* 0x480 */ f32 spin;           /* added every frame to rotX or rotY: appearance only */
    /* 0x484 */ f32 rotX;
    /* 0x488 */ f32 rotY;
    /* 0x48C */ f32 bankAngle;
    /* 0x490 */ f32 rollAngle;
    /* 0x494 */ f32 rollLeft;
    /* 0x498 */ f32 rollTime;
    /* 0x49C */ f32 grown;          /* 0..1 */
    /* 0x4A0 */ f32 growTime;       /* frames */
    /* 0x4A4 */ f32 fadeTime;       /* frames */
    /* 0x4A8 */ f32 fadeLeft;
    /* 0x4AC */ f32 life;           /* frames */
    /* 0x4B0 */ f32 age;
    /* 0x4B4 */ f32 size;           /* owner height / 19.35 */
    /* 0x4B8 */ f32 timer;
    /* 0x4BC */ s32 flags;          /* EFT_DISC_* */
} EftDisc; /* size 0x4C0 */

#define EFT_DISC_ALIVE   0x0001
#define EFT_DISC_END     0x0002 /* fade out, then die */
#define EFT_DISC_DEAD    0x0004
#define EFT_DISC_HELD    0x0008 /* follows the owner's hand */
#define EFT_DISC_FLYING  0x0020 /* moves and adds hit records */
#define EFT_DISC_SHOWN   0x0040
#define EFT_DISC_PIECE   0x0080 /* technique piece: has an emitter set */
#define EFT_DISC_REQ200  0x0200
#define EFT_DISC_LOST    0x0400
#define EFT_DISC_GROWN   0x0800
#define EFT_DISC_AUTO    0x1000 /* thrown as soon as it is grown */
#define EFT_DISC_MOVED   0x2000

/* A held disc of a fighter: node of the per-fighter list. */
typedef struct EftDiscNode {
    /* 0x0 */ EftPTask *task;
    /* 0x4 */ s32 flags;    /* 1 used, 0x10 kind 1, 0x20 thrown */
    /* 0x8 */ struct EftDiscNode *next;
} EftDiscNode; /* size 0xC */

/* Work of the disc manager (the first word of *gEftDisc points at it). */
typedef struct EftDiscMgr {
    /* 0x000 */ u8 unk0[0x420];
    /* 0x420 */ u8 modelA[0x90];    /* model template of kinds 0 and 1 */
    /* 0x4B0 */ u8 modelB[0xA0];    /* model template of kinds 2 and 3 */
    /* 0x550 */ void *list;         /* task list of the discs */
    /* 0x554 */ EftDiscNode *head[2]; /* per fighter */
    /* 0x55C */ EftDiscNode *tail[2];
    /* 0x564 */ EftDiscNode node[20];
    /* 0x654 */ u8 cursor;
} EftDiscMgr;

typedef struct EftDiscRoot {
    /* 0x0 */ EftDiscMgr *mgr;
} EftDiscRoot;

extern EftDiscRoot *gEftDisc;

void EftDisc_PostUpdate(EftPTask *task);
void EftDisc_Draw(EftPTask *task);
void EftDisc_Reset(EftPTask *task);
void EftDisc_Term(EftPTask *task);
void EftDisc_SpawnParts(EftPTask *task, s32 mode);
void EftDisc_AddHit(EftPTask *task);
void EftDisc_SetHeldAtk(EftPTask *task, EftDiscAtk *atk);
void EftDisc_SetTex(EftDisc *w, EftPTexSet *tex, s32 a, s32 b);
void EftDisc_Home(EftPVec *out, EftPVec *pos, EftPVec *dir, s32 objId, f32 speed, f32 maxTurn, f32 bank);
void EftDisc_RotateAboutAxis(EftPVec *out, EftPVec *v, EftPVec *axis, f32 angle);
void EftDisc_StepTex(EftDisc *w);
s32 EftDiscMgr_PushHeld(EftPTask *task, s32 objId, u8 kind);
s32 EftDiscMgr_PopHeld(s32 objId);
EftDiscNode *EftDiscMgr_AllocNode(void);
s32 EftDisc_SpawnHeld(EftDiscHeldArg *a, s32 hand);
s32 EftDisc_Throw(EftDiscAtk *a);
s32 EftDisc_SpawnThrown(EftDiscAtk *a);
s32 EftDisc_DropHeld(s32 objId);
s32 EftDisc_SpawnFromNode(EftDiscAtk *a);
EftPTask *EftDisc_Create(EftDiscArg *arg);
void EftDisc_Release(EftPTask *task, EftPVec dir);
void EftDisc_RequestEnd(EftPTask *task);
void EftDisc_Kill(EftPTask *task);
void EftDisc_SetSpin(EftPTask *task, f32 spin);
void EftDisc_SetScale(EftPTask *task, f32 scale);
s32 EftDisc_SetModelTex(EftPTask *task, void *tex, s32 a, s32 b);
void EftDisc_SetLastHit(EftPTask *task, s32 on);
void EftDisc_SetFlag200(EftPTask *task);
s32 EftDisc_IsAlive(EftPTask *task);

/* ---- power-up glow particles (src/battle/eft_p_b.c) ---------------------------------------------------- */

/* 4x4 matrix as four aligned rows. */
typedef struct EftPMtx {
    /* 0x00 */ EftPVec row[4];
} EftPMtx; /* size 0x40 */

/* A GS screen position as Vu0Cur_ProjectPoint writes it. */
typedef struct EftPScr {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftPScr; /* size 0x10 */

/* Work of a glow task (class 0x2C3B68, 0x2E0 bytes; EftGlow of eft_q.h): the emitter of one fighter. */
typedef struct EftPGlow {
    /* 0x000 */ s32 flags;      /* 1 active, 2 fading out (set by eft_q.c), 4 finished, 8 a paired quad was spawned
                                   for this node, 0x20 textured variant */
    /* 0x004 */ s32 nodeMask;   /* bit n toggles each time node n spawns: alternates the quad's side */
    /* 0x008 */ s32 unk8;
    /* 0x00C */ s32 kind;       /* BtlCharApi_GetActionFxKind at start: 3 rising, 4 falling, 5 node axis */
    /* 0x010 */ s32 step;
    /* 0x014 */ s32 node;       /* node the next quad spawns at, 0..9 */
    /* 0x018 */ s32 count;      /* quads spawned at this node, 0..3 */
    /* 0x01C */ s32 unk1C[2];
    /* 0x024 */ s32 layers;     /* number of entries of color[] in use */
    /* 0x028 */ s32 unk28;
    /* 0x02C */ s32 texCount;   /* textured variant: number of textures */
    /* 0x030 */ s32 live;       /* quads alive, counted by EftGlow_StepParts */
    /* 0x034 */ s32 fade;       /* 0 steady, 1 fading in, 2 fading out */
    /* 0x038 */ f32 fadeTime;
    /* 0x03C */ f32 alpha;      /* 0..1 */
    /* 0x040 */ f32 alphaA;     /* 48 / 255: alpha of a trailing quad */
    /* 0x044 */ f32 alphaB;     /* 32 / 255: the same for the paired quad */
    /* 0x048 */ f32 scale;      /* fighter height / 19.35 */
    /* 0x04C */ s32 objId;
    /* 0x050 */ Mtx44 frame[5]; /* orientation of this frame and the four before it */
    /* 0x190 */ EftPVec colorA;
    /* 0x1A0 */ EftPVec colorB;
    /* 0x1B0 */ EftPVec nodePos[10];
    /* 0x250 */ EftPVec offset; /* random offset of the quads, re-rolled after every round of the ten nodes */
    /* 0x260 */ EftPVec color[4];
    /* 0x2A0 */ u64 tex[8];     /* GS TEX0: [0] plain, [1..] the textured variant */
} EftPGlow; /* size 0x2E0 */

/* One glow quad. */
typedef struct EftPGlowPart {
    /* 0x00 */ s32 flags;       /* 1 fading out, 2 trailing (emitter was fading), 4 second stage of the size
                                   animation, 8 / 0x10 from the emitter, 0x20 paired quad, 0x40 odd side,
                                   0x80 / 0x100 emitter kind 3 / 4 */
    /* 0x04 */ u8 pair;         /* selects the vertex table */
    /* 0x05 */ u8 kind;         /* row of EftPGlowCfg2.kind */
    /* 0x06 */ u8 tex;          /* textured variant: texture index */
    /* 0x07 */ u8 objId;
    /* 0x08 */ u8 frame;        /* index into EftPGlow.frame: grows to 4, so an old quad lags the fighter's turn */
    /* 0x09 */ u8 flipA;
    /* 0x0A */ u8 flipB;        /* animation frame 0 / 1 */
    /* 0x0B */ u8 animTimer;
    /* 0x0C */ u8 layer;        /* order table layer */
    /* 0x0D */ u8 node;
    /* 0x0E */ u8 shape;        /* sample of the profile tables, 0..19 */
    /* 0x0F */ u8 unkF;
    /* 0x10 */ f32 life;        /* frames left */
    /* 0x14 */ f32 lifeMax;
    /* 0x18 */ f32 size;
    /* 0x1C */ f32 width;
    /* 0x20 */ f32 length;
    /* 0x24 */ f32 widthScale;
    /* 0x28 */ f32 taper;       /* length factor of the far two vertices */
    /* 0x2C */ f32 dWidth;
    /* 0x30 */ f32 dLength;
    /* 0x34 */ f32 dist;        /* distance along the profile */
    /* 0x38 */ f32 angle;       /* degrees about the fighter's axis */
    /* 0x3C */ f32 dirY;
    /* 0x40 */ EftPVec pos;
    /* 0x50 */ EftPVec color;   /* 0..1 */
    /* 0x60 */ EftPVec dColor;
    /* 0x70 */ EftPVec offset;  /* w (+0x7C) is a value of its own: how far a trailing quad is pushed along its
                                   direction each frame */
    /* 0x80 */ EftPVec point;   /* profile point */
    /* 0x90 */ EftPVec dir;     /* profile direction */
    /* 0xA0 */ EftPVec worldDir;
    /* 0xB0 */ struct EftPGlowPart *next;
    /* 0xB4 */ u8 unkB4[0xC];
} EftPGlowPart; /* size 0xC0 */

/* gEftGlow (EftGlowMgr of eft_q.h): what the particle code uses. */
typedef struct EftPGlowMgr {
    /* 0x000 */ s32 charCount;
    /* 0x004 */ s32 partMax;
    /* 0x008 */ EftPGlowPart *parts;
    /* 0x00C */ EftPGlowPart *free;
    /* 0x010 */ EftPGlowPart *active;
    /* 0x014 */ s32 used;       /* parts handed out from the array so far (at most 70) */
    /* 0x018 */ s32 unk18[2];
    /* 0x020 */ EftPVec point[20]; /* profile: 10 spline segments x 2 samples */
    /* 0x160 */ EftPVec dir[20];   /* profile directions */
    /* 0x2A0 */ u8 unk2A0[0x248];
    /* 0x4E8 */ u64 pairTex;    /* TEX0 of the paired quads */
} EftPGlowMgr;

typedef struct EftPGlowNode {
    /* 0x00 */ s32 node;        /* fighter node */
    /* 0x04 */ s32 shape;       /* first profile sample */
    /* 0x08 */ f32 length;
    /* 0x0C */ f32 width;
    /* 0x10 */ f32 pairLength;
    /* 0x14 */ f32 pairWidth;
} EftPGlowNode; /* size 0x18 */

typedef struct EftPGlowNodeB {
    /* 0x0 */ f32 length;
    /* 0x4 */ f32 width;
    /* 0x8 */ f32 pairLength;
    /* 0xC */ f32 pairWidth;
} EftPGlowNodeB; /* size 0x10 */

/* gEftGlowCfg (common pack entry 7). */
typedef struct EftPGlowCfg {
    /* 0x000 */ u8 unk0[0x1A4];
    /* 0x1A4 */ f32 shapeScale;
    /* 0x1A8 */ f32 fadeIn;     /* frames */
    /* 0x1AC */ f32 fadeOut;
    /* 0x1B0 */ f32 offX;
    /* 0x1B4 */ f32 offY;
    /* 0x1B8 */ f32 offZ;
    /* 0x1BC */ f32 unk1BC;
    /* 0x1C0 */ EftPVec pts[13];   /* control points of the profile */
    /* 0x290 */ EftPVec vtx[2][4]; /* quad corners by EftPGlowPart.pair */
    /* 0x310 */ EftPVec uvA0[4];
    /* 0x350 */ EftPVec uvA2[4];
    /* 0x390 */ EftPVec uvA1[4];
    /* 0x3D0 */ EftPVec uvA3[4];
    /* 0x410 */ EftPVec uvB[2][4];
    /* 0x490 */ EftPGlowNode node[10];
    /* 0x580 */ EftPGlowNodeB nodeB[10]; /* for emitter kinds 3 / 4 */
} EftPGlowCfg;

typedef struct EftPGlowKind {
    /* 0x00 */ f32 dirY[2];     /* base, random range */
    /* 0x08 */ f32 dist[2];
    /* 0x10 */ f32 size[2];
    /* 0x18 */ f32 widthScale[2];
    /* 0x20 */ f32 taper[2];
    /* 0x28 */ f32 unk28[2];
    /* 0x30 */ f32 offset[3];
} EftPGlowKind; /* size 0x3C */

/* gEftGlowCfg2. */
typedef struct EftPGlowCfg2 {
    /* 0x00 */ f32 stage;       /* fraction of the life after which the size animation enters its second stage */
    /* 0x04 */ f32 animFrames;
    /* 0x08 */ f32 pointY;
    /* 0x0C */ f32 driftBase;
    /* 0x10 */ f32 driftRand;
    /* 0x14 */ f32 moveScale;
    /* 0x18 */ f32 trailPush;
    /* 0x1C */ f32 lifeBase;
    /* 0x20 */ f32 lifeRand;
    /* 0x24 */ f32 unk24[2];
    /* 0x2C */ f32 width[3];    /* by stage */
    /* 0x38 */ f32 trailWidth;
    /* 0x3C */ f32 length[3];
    /* 0x48 */ f32 trailLength;
    /* 0x4C */ f32 widthMul[2]; /* flags 0x10, 8 */
    /* 0x54 */ f32 lengthMul[2];
    /* 0x5C */ f32 widthSide[3]; /* odd side, even side, paired */
    /* 0x68 */ f32 lengthSide[3];
    /* 0x74 */ f32 trailWidthBy[2][4];
    /* 0x94 */ f32 trailLengthBy[2][4];
    /* 0xB4 */ EftPGlowKind kind[4];
} EftPGlowCfg2;

extern EftPGlowMgr *gEftGlow;
extern EftPGlowCfg *gEftGlowCfg;
extern EftPGlowCfg2 *gEftGlowCfg2;

void EftGlow_Spline3(EftPVec *out, EftPVec *p, f32 t);
void EftGlow_BuildTables(void);
void EftGlow_GetShapePoint(EftPVec *out, s32 objId, s32 idx, f32 scale);
void EftGlow_GetShapeDir(EftPVec *out, s32 objId, s32 idx, f32 y);
void EftGlow_CalcFrame(Mtx44 *m, s32 objId, s32 kind);
void EftGlow_CalcDrift(EftPVec *out, s32 objId);
s32 EftGlow_CalcPartFlags(EftPGlow *e, s32 pair, s32 odd);
f32 EftGlow_GetAlpha(EftPGlow *e, s32 flags, s32 idx);
f32 EftGlow_CalcLength(EftPGlowPart *p, s32 stage);
f32 EftGlow_CalcWidth(EftPGlowPart *p, s32 stage);
EftPGlowPart *EftGlow_AllocPart(void);
EftPGlowPart *EftGlow_SpawnPart(EftPGlow *e, s32 objId, f32 angle, EftPVec *scale, s32 odd, s32 pair);
void EftGlow_PairPart(EftPGlow *e, EftPGlowPart *p, EftPGlowPart *src);
void EftGlow_SpawnNext(EftPGlow *e, s32 objId);
void EftGlow_Begin(EftPGlow *e, s32 *arg);
void EftGlow_UpdateFrames(EftPGlow *e, s32 objId);
void EftGlow_Emit(EftPGlow *e, s32 objId);
void EftGlow_Step(EftPGlow *e, s32 objId);
void EftGlow_FreeParts(s32 objId);
void EftGlow_StepParts(EftPGlow *e, s32 objId);
void EftGlow_MakeFacingMtx(Mtx44 *m, EftPVec *dir, EftPVec *pos);
void EftGlow_DrawParts(EftPGlow *e, s32 objId, f32 alpha);

#endif
