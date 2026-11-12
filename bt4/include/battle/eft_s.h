#ifndef BATTLE_EFT_S_H
#define BATTLE_EFT_S_H

#include "types.h"
#include "sys/math3d.h"

/*
 * src/battle/eft_s.c, 0x178AB0..0x17CB40. Two modules, each cut by the file boundaries:
 *
 * 1. 0x178AB0..0x1793A8: the second half of ki blast type 2 (a blast that is a model, thrown with gravity and
 *    spin; it breaks into five sprite fragments when it hits). Its first half (init, update, the hit record, the
 *    deflect handler) is in eft_r.c, 0x178530..0x178AB0, which named the module EftKiObj. Simulation: the blast's
 *    position, velocity and end are here. The types below are called EftKiProp* (this file's views).
 * 2. 0x1793A8..0x17CB40: the "chain" effect (named by the former eft_t.c, now the second part of eft_s.c, which has its remaining class callbacks and its
 *    public API from 0x17CB40 on): up to 16 strands of ribbon nodes radiating from a point, driven by a three-key
 *    parameter block. Drawing only. The types below are called EftArc* (this file's views; EftArcChain is one
 *    strand, eft_t.h's EftChainStrand).
 *
 * All structs are this file's views; a field named unkXX / fXX has no meaning backed by this file.
 */

/* The effect code's vector: 16-byte aligned, copied with 64-bit loads. */
typedef union EftVec {
    struct {
        f32 x, y, z, w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftVec;

/* The part of a task (0x40 bytes) that the callbacks here use. */
typedef struct EftTask {
    /* 0x00 */ u8 flags;       /* bit 0: killed */
    /* 0x01 */ u8 unk1[3];
    /* 0x04 */ u32 result;     /* written by the hit pass: low six bits = the record hit something / is over;
                                  0x40 / 0x80 = the blast was deflected / reflected */
    /* 0x08 */ u32 unk8[2];
    /* 0x10 */ EftVec hitPos;  /* position written back by the hit pass */
    /* 0x20 */ u8 unk20[8];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;     /* work block of the size given to BtlTask_CreateChildList */
    /* 0x3C */ s32 unk3C;
} EftTask; /* 0x40 */

/* One texture entry (GS TEX0 value plus loader data). */
typedef struct EftSTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} __attribute__((aligned(16))) EftSTexEntry; /* 0x10 */

/* ---- prop ki blast -------------------------------------------------------------------------------------- */

/* Argument of EftKiObj_Create: the ki blast launch block the fighter fills (FxHitArg2 in btl_char_fx.h). */
typedef struct EftKiPropArg {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ s16 objId;     /* fighter that fired */
    /* 0x12 */ s16 owner;     /* fighter the blast currently belongs to (toggled when it is deflected) */
    /* 0x14 */ u16 code;
    /* 0x16 */ u16 unk16;
    /* 0x18 */ s16 level;
    /* 0x1A */ u8 type;
    /* 0x1B */ u8 unk1B;
    /* 0x1C */ s16 frames;    /* life in frames */
    /* 0x1E */ s16 unk1E;
    /* 0x20 */ f32 speed;
    /* 0x24 */ f32 unk24;
    /* 0x28 */ f32 scale;
    /* 0x2C */ f32 unk2C;
    /* 0x30 */ s32 unk30[8];
} __attribute__((aligned(16))) EftKiPropArg; /* 0x50 */

/* A block of 0x40 bytes handed to EftKiObj_SetBlock. */
typedef struct EftKiPropBlock {
    /* 0x00 */ u64 d[8];
} EftKiPropBlock; /* 0x40 */

typedef struct EftKiPropFrag {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ EftVec vel;    /* w = size factor 0.5..1 */
} EftKiPropFrag; /* 0x20 */

#define EFT_KIPROP_FRAGS 5

/* flags */
#define EFT_KIPROP_MOVED 0x2      /* prevPos is valid */
#define EFT_KIPROP_DEFLECTED 0x4
#define EFT_KIPROP_DEAD 0x8       /* the update kills the task */
#define EFT_KIPROP_BROKEN 0x10    /* stopped moving; fragments are playing */
#define EFT_KIPROP_HITMOVED 0x20
#define EFT_KIPROP_SHOWN 0x40
#define EFT_KIPROP_HITTING 0x80   /* publishes a hit record */
#define EFT_KIPROP_FRAGS_ON 0x100

/* Work block of one prop ki blast, 0x1E0 bytes. */
typedef struct EftKiProp {
    /* 0x000 */ EftKiPropArg arg;
    /* 0x050 */ EftVec nodePos;
    /* 0x060 */ EftVec pos;
    /* 0x070 */ EftVec prevPos;
    /* 0x080 */ EftVec vel;
    /* 0x090 */ EftKiPropBlock block;
    /* 0x0D0 */ s32 flags;
    /* 0x0D4 */ s32 life;
    /* 0x0D8 */ s32 unkD8;
    /* 0x0DC */ f32 speed;
    /* 0x0E0 */ f32 scale;
    /* 0x0E4 */ f32 unkE4;     /* scale * 5 */
    /* 0x0E8 */ f32 spin;      /* current angle about X */
    /* 0x0EC */ f32 spinRate;  /* degrees per frame, +-27..37 */
    /* 0x0F0 */ f32 gravity;   /* 0.5 per frame */
    /* 0x0F4 */ s16 frame;
    /* 0x0F6 */ s16 unkF6;
    /* 0x0F8 */ s16 unkF8;
    /* 0x0FA */ s16 unkFA;
    /* 0x0FC */ u8 modelInfo[0x30];
    /* 0x12C */ s32 modelObj;  /* battle object of the model, -1 = none */
    /* 0x130 */ EftKiPropFrag frag[EFT_KIPROP_FRAGS];
    /* 0x1D0 */ void *tex;     /* fragment texture entry (EftKiPropRes.tex) */
    /* 0x1D4 */ s16 fragTimer; /* 15 frames */
    /* 0x1D6 */ u8 unk1D6[0xA];
} EftKiProp; /* 0x1E0 */

/* Resources of the manager task, 0x58 bytes from the pool. */
typedef struct EftKiPropRes {
    /* 0x00 */ u32 unk0[2];
    /* 0x08 */ u8 tex[0x44];   /* texture entry of the fragments */
    /* 0x4C */ s32 texReady;   /* bit 0: tex0 was built this frame */
    /* 0x50 */ s32 *model;     /* entry 1 of the pack */
    /* 0x54 */ s32 *pack;      /* entry 3 of the character pack */
} EftKiPropRes; /* 0x58 */

typedef struct EftKiPropMgr {
    /* 0x00 */ EftKiPropRes *res;
} EftKiPropMgr;

/* ---- arcs ----------------------------------------------------------------------------------------------- */

/* Texture set an arc effect is given: 16 entries and a mask of the entries whose TEX0 is built. */
typedef struct EftArcTexSet {
    /* 0x000 */ EftSTexEntry entry[16];
    /* 0x100 */ s32 unk100;
    /* 0x104 */ s32 built;
} EftArcTexSet;

/* Static parameters of an arc effect. From 0x14 on, 39 rows of f32[3]: one value per key; the comment names the
   EftArcKey field the row feeds. They are separate members in the original (each access splits its offset by the
   member's position modulo the struct's 16-byte alignment). */
typedef struct EftArcParam {
    /* 0x00 */ u32 flags;      /* 1: animated between the three keys; 2: clipped polygons; 4: width pulse;
                                  8: colour pulse; 0x10: attached to the effect matrix */
    /* 0x04 */ u8 kind;        /* 0 / 2: head fades; 1: the chain crawls; 2: random yaw per chain */
    /* 0x05 */ u8 offsetMode;  /* 0: every chain drifts along its colour delta; 1: every other one at random */
    /* 0x06 */ u8 layer;       /* draw layer / blend */
    /* 0x07 */ u8 unk7;
    /* 0x08 */ u8 countBase;   /* chains per burst */
    /* 0x09 */ u8 countRange;
    /* 0x0A */ u8 unkA[2];
    /* 0x0C */ f32 keyTime;    /* seconds of the key animation */
    /* 0x10 */ f32 keyMid;     /* fraction of keyTime at which key 1 is reached */
    /* 0x014 */ f32 r0[3];       /* spawnWait */
    /* 0x020 */ f32 r1[3];       /* f60 */
    /* 0x02C */ f32 r2[3];       /* f64 */
    /* 0x038 */ f32 r3[3];       /* f68 */
    /* 0x044 */ f32 r4[3];       /* f6C */
    /* 0x050 */ f32 r5[3];       /* f70 */
    /* 0x05C */ f32 r6[3];       /* f74 */
    /* 0x068 */ f32 r7[3];       /* nodeCount */
    /* 0x074 */ f32 r8[3];       /* growRate */
    /* 0x080 */ f32 r9[3];       /* shrinkRate */
    /* 0x08C */ f32 r10[3];      /* lifeBase */
    /* 0x098 */ f32 r11[3];      /* lifeRange */
    /* 0x0A4 */ f32 r12[3];      /* f8C */
    /* 0x0B0 */ f32 r13[3];      /* widthBase */
    /* 0x0BC */ f32 r14[3];      /* widthRange */
    /* 0x0C8 */ f32 r15[3];      /* pulseFrom */
    /* 0x0D4 */ f32 r16[3];      /* pulseTo */
    /* 0x0E0 */ f32 r17[3];      /* pulseTime */
    /* 0x0EC */ f32 r18[3];      /* fA4 */
    /* 0x0F8 */ f32 r19[3];      /* endWidth */
    /* 0x104 */ f32 r20[3];      /* endAlpha */
    /* 0x110 */ f32 r21[3];      /* radBase */
    /* 0x11C */ f32 r22[3];      /* radRange */
    /* 0x128 */ f32 r23[3];      /* rad2Base */
    /* 0x134 */ f32 r24[3];      /* rad2Range */
    /* 0x140 */ f32 r25[3];      /* jitter */
    /* 0x14C */ f32 r26[3];      /* fC4 */
    /* 0x158 */ f32 r27[3];      /* turnBase */
    /* 0x164 */ f32 r28[3];      /* turnRange */
    /* 0x170 */ f32 r29[3];      /* stepXBase */
    /* 0x17C */ f32 r30[3];      /* stepXRange */
    /* 0x188 */ f32 r31[3];      /* stepYBase */
    /* 0x194 */ f32 r32[3];      /* stepYRange */
    /* 0x1A0 */ f32 r33[3];      /* crawlXBase */
    /* 0x1AC */ f32 r34[3];      /* crawlXRange */
    /* 0x1B8 */ f32 r35[3];      /* crawlYBase */
    /* 0x1C4 */ f32 r36[3];      /* crawlYRange */
    /* 0x1D0 */ f32 r37[3];      /* fadeIn */
    /* 0x1DC */ f32 r38[3];      /* fadeOut */
} __attribute__((aligned(16))) EftArcParam; /* 0x1E8 */

/* Key data: vectors and pairs per key. */
typedef struct EftArcKeys {
    /* 0x000 */ EftVec colBase[3];
    /* 0x030 */ EftVec colRange[3];
    /* 0x060 */ EftVec col2Base[3];
    /* 0x090 */ EftVec col2Range[3];
    /* 0x0C0 */ f32 mulR[3][2];    /* from, to */
    /* 0x0D8 */ f32 mulG[3][2];
    /* 0x0F0 */ f32 mulB[3][2];
    /* 0x108 */ f32 mulTime[3];    /* seconds */
} EftArcKeys;

/* The parameters in effect (one key, or the blend of two). 0xF8 bytes. */
typedef struct EftArcKey {
    /* 0x00 */ EftVec colBase;
    /* 0x10 */ EftVec colRange;
    /* 0x20 */ EftVec col2Base;
    /* 0x30 */ EftVec col2Range;
    /* 0x40 */ f32 mulBase[3];   /* colour pulse: factor at phase 0 */
    /* 0x4C */ f32 mulRange[3];  /* ... and its change over the pulse */
    /* 0x58 */ f32 mulTime;      /* frames of half a colour pulse */
    /* 0x5C */ f32 spawnWait;    /* frames between bursts */
    /* 0x60 */ f32 f60;          /* angles (param value * pi) */
    /* 0x64 */ f32 f64;
    /* 0x68 */ f32 f68;
    /* 0x6C */ f32 f6C;
    /* 0x70 */ f32 f70;
    /* 0x74 */ f32 f74;
    /* 0x78 */ f32 nodeCount;
    /* 0x7C */ f32 growRate;     /* nodes shown per frame */
    /* 0x80 */ f32 shrinkRate;
    /* 0x84 */ f32 lifeBase;     /* frames */
    /* 0x88 */ f32 lifeRange;
    /* 0x8C */ f32 f8C;
    /* 0x90 */ f32 widthBase;
    /* 0x94 */ f32 widthRange;
    /* 0x98 */ f32 pulseBase;    /* width pulse */
    /* 0x9C */ f32 pulseRange;
    /* 0xA0 */ f32 pulseTime;    /* frames */
    /* 0xA4 */ f32 fA4;
    /* 0xA8 */ f32 endWidth;
    /* 0xAC */ f32 endAlpha;
    /* 0xB0 */ f32 radBase;      /* radius of the first node */
    /* 0xB4 */ f32 radRange;
    /* 0xB8 */ f32 rad2Base;     /* radius of the last node */
    /* 0xBC */ f32 rad2Range;
    /* 0xC0 */ f32 jitter;
    /* 0xC4 */ f32 fC4;
    /* 0xC8 */ f32 turnBase;
    /* 0xCC */ f32 turnRange;
    /* 0xD0 */ f32 stepXBase;
    /* 0xD4 */ f32 stepXRange;
    /* 0xD8 */ f32 stepYBase;
    /* 0xDC */ f32 stepYRange;
    /* 0xE0 */ f32 crawlXBase;
    /* 0xE4 */ f32 crawlXRange;
    /* 0xE8 */ f32 crawlYBase;
    /* 0xEC */ f32 crawlYRange;
    /* 0xF0 */ f32 fadeIn;       /* frames */
    /* 0xF4 */ f32 fadeOut;
} EftArcKey; /* 0xF8 used, 0x100 with alignment */

/* Argument of the arc effect's init. */
typedef struct EftArcArg {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ EftVec dir;
    /* 0x20 */ s32 objId;
    /* 0x24 */ s32 texIdx;
    /* 0x28 */ f32 life;          /* seconds; <= 0: until told to end */
    /* 0x2C */ f32 scale;
    /* 0x30 */ EftArcTexSet *texSet;
    /* 0x34 */ EftArcParam *param;
    /* 0x38 */ EftArcKeys *keys;
    /* 0x3C */ s32 unk3C;
} EftArcArg; /* 0x40 */

typedef struct EftArcNode {
    /* 0x00 */ EftVec pos;       /* relative to the chain */
    /* 0x10 */ EftVec jit;       /* random offset */
    /* 0x20 */ EftVec world;
    /* 0x30 */ EftVec color;     /* 0..255 */
    /* 0x40 */ s32 flags;        /* 1 in use, 2 shown */
    /* 0x44 */ f32 widthA;
    /* 0x48 */ f32 widthB;
    /* 0x4C */ f32 radius;
    /* 0x50 */ f32 amp;          /* jitter amplitude */
    /* 0x54 */ struct EftArcNode *next;
    /* 0x58 */ u32 unk58[2];
} EftArcNode; /* 0x60 */

#define EFT_ARC_NODES 500

/* gEftChain (0x2FEA64) points at this. */
typedef struct EftArcPool {
    /* 0x00 */ EftArcNode *node;
    /* 0x04 */ s32 cursor;
} EftArcPool;

/* chain flags */
#define EFT_ARCCH_ON 0x1
#define EFT_ARCCH_GROWN 0x2
#define EFT_ARCCH_SHRUNK 0x4
#define EFT_ARCCH_DEAD 0x8
#define EFT_ARCCH_FADED_IN 0x10
#define EFT_ARCCH_FADED_OUT 0x20
#define EFT_ARCCH_F40 0x40
#define EFT_ARCCH_ENDING 0x80
#define EFT_ARCCH_DRIFT 0x100
#define EFT_ARCCH_MUL_DOWN 0x200
#define EFT_ARCCH_PULSE_DOWN 0x400
#define EFT_ARCCH_VISIBLE 0x800

typedef struct EftArcChain {
    /* 0x00 */ EftVec pos;       /* added to every node */
    /* 0x10 */ EftVec drift;     /* added to the node positions each frame (kinds other than 1) */
    /* 0x20 */ EftVec unk20;
    /* 0x30 */ EftVec color;
    /* 0x40 */ EftVec colA;
    /* 0x50 */ EftVec colB;      /* col2 - colA */
    /* 0x60 */ EftArcNode *head;
    /* 0x64 */ s32 count;
    /* 0x68 */ s32 flags;
    /* 0x6C */ f32 shown;        /* nodes shown */
    /* 0x70 */ f32 time;
    /* 0x74 */ f32 life;
    /* 0x78 */ f32 shrinkAt;
    /* 0x7C */ f32 f7C;
    /* 0x80 */ f32 endAt;
    /* 0x84 */ f32 fadeAt;
    /* 0x88 */ f32 rotX;
    /* 0x8C */ f32 rotY;
    /* 0x90 */ f32 rotZ;
    /* 0x94 */ f32 stepX;
    /* 0x98 */ f32 stepY;
    /* 0x9C */ f32 crawlX;
    /* 0xA0 */ f32 crawlY;
    /* 0xA4 */ f32 width;
    /* 0xA8 */ f32 alpha;
    /* 0xAC */ f32 radStep;
    /* 0xB0 */ f32 fadeInT;
    /* 0xB4 */ f32 fadeOutT;
    /* 0xB8 */ f32 fadeIn;
    /* 0xBC */ f32 fadeOut;
    /* 0xC0 */ f32 pulseT;
    /* 0xC4 */ f32 mulT;
    /* 0xC8 */ u32 unkC8[2];
} EftArcChain; /* 0xD0 */

#define EFT_ARC_CHAINS 16

/* effect flags */
#define EFT_ARC_ON 0x1
#define EFT_ARC_ENDING 0x2
#define EFT_ARC_DEAD 0x4
#define EFT_ARC_ENDLESS 0x8
#define EFT_ARC_WHITE 0x10
#define EFT_ARC_ORIENTED 0x20

/* Work block of the arc effect, 0xF10 bytes. */
typedef struct EftArc {
    /* 0x000 */ EftArcArg arg;
    /* 0x040 */ EftArcKey cur;
    /* 0x140 */ EftArcChain chain[EFT_ARC_CHAINS];
    /* 0xE40 */ Mtx44 mtx;        /* rotation that turns +Z onto arg.dir */
    /* 0xE80 */ s32 flags;
    /* 0xE84 */ u8 stopKind;      /* second argument of BtlScene_IsEffectStopped */
    /* 0xE85 */ u8 unkE85[3];
    /* 0xE88 */ s32 live;         /* chains in use */
    /* 0xE8C */ s32 slot;         /* 0..3, next rotation slot */
    /* 0xE90 */ s32 nodeCount;
    /* 0xE94 */ f32 time;
    /* 0xE98 */ f32 life;         /* frames */
    /* 0xE9C */ f32 spawnWait;
    /* 0xEA0 */ f32 delay;
    /* 0xEA4 */ f32 endWait;
    /* 0xEA8 */ f32 keyT;
    /* 0xEAC */ f32 keyEnd;
    /* 0xEB0 */ f32 keyMid;
    /* 0xEB4 */ f32 pitch;
    /* 0xEB8 */ f32 yaw;
    /* 0xEBC */ f32 rotX[4];      /* per slot */
    /* 0xECC */ f32 rotY[4];
    /* 0xEDC */ f32 rotZ;
    /* 0xEE0 */ EftSTexEntry texA;
    /* 0xEF0 */ EftSTexEntry texB;
    /* 0xF00 */ u64 tex0;
    /* 0xF08 */ u64 unkF08;
} EftArc; /* 0xF10 */

void EftKiObj_Reset(EftTask *task);
void EftKiObj_PostUpdateCb(EftTask *task);
void EftKiObj_DrawCb(EftTask *task);
void EftKiObjMgr_Init(EftTask *task, s32 *arg);
void EftKiObjMgr_Term(EftTask *task);
void EftKiObjMgr_Update(EftTask *task);
void EftKiObj_Create(EftKiPropArg arg);
void EftKiObj_SetBlock(EftTask *task, EftKiPropBlock *src);
s32 EftKiObj_Step(EftTask *task);
void EftKiObj_PostUpdate(EftTask *task);
void EftKiObj_Draw(EftTask *task);
void EftKiObj_Break(EftTask *task, EftVec *pos);
void EftKiObj_StepFrags(EftTask *task);
void EftKiObj_DrawFrags(EftTask *task);

void EftChain_SetTex(EftArc *w, EftArcTexSet *set, s32 a, s32 b);
void EftChain_BuildTex(EftArc *dst, EftArc *src);
void EftChain_SetKey(EftArcKey *out, EftArc *w, s32 key);
void EftChain_BlendKeys(EftArc *w);
s32 EftChain_AllocNodes(EftArcChain *ch, s32 count);
void EftChain_FreeNodes(EftArcChain *ch);
void EftChain_InitStrand(EftTask *task, EftArcChain *ch);
void EftChain_UpdateStrand(EftTask *task, EftArcChain *ch);
void EftChain_DrawStrand(EftArc *w, EftArc *w2, EftArcChain *ch);
void EftChain_DrawStrandClipped(EftArc *w, EftArc *w2, EftArcChain *ch);
s32 EftChain_StartStrand(EftTask *task, EftArcChain *ch);
void EftChain_UpdateStrands(EftTask *task);
void EftChain_DrawStrands(EftTask *task);
void EftChain_Spawn(EftTask *task);
void EftChain_Init(EftTask *task, EftArcArg *arg);
void EftChain_Term(EftTask *task);
void EftChain_Update(EftTask *task);

#endif
