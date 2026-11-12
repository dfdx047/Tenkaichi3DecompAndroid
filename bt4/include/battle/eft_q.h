#ifndef BATTLE_EFT_Q_H
#define BATTLE_EFT_Q_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect tasks, 0x170A50..0x174A70 (src/battle/eft_q.c). Seven pieces; none of them touches a fighter, a battle
 * object or a hit record (see the notes on each in the C file):
 *
 * 1. 0x170A50..0x1716A0  power-up glow: the task callbacks, the manager and the entry points of the module whose
 *                        particles are in eft_p_b.c (0x16DCA0..). Fighter effect requests 4 / 5 drive it.
 * 2. 0x1716A0..0x171D80  rush-finish burst: one emitter set (character pack entry 7) at a fighter node, aimed along
 *                        a direction. Fighter effect requests 0x28 (start), 0x29 (second stage), 0x2A (end).
 * 3. 0x171D80..0x1722E8  per-character effect root (scene layer 2): one task per character with the managers of the ten
 *                        per-character effect kinds; EftChar_GetList / EftChar_SetList are how every kind finds
 *                        its task list.
 * 4. 0x1722E8..0x172480  placeholder kind (table entries 6, 8, 10): a manager and a task class that do nothing.
 * 5. 0x172480..0x172D80  full-screen flash: a colour sprite over the whole screen that fades in, holds and fades
 *                        out. Started by technique modules and, for a fighter in action 0x104, by its own manager.
 * 6. 0x172D80..0x174220  trails: up to 20 points drawn as a camera-facing strip with glow sprites at sharp bends.
 *                        Nothing starts one.
 * 7. 0x174220..0x174A70  character body effect: an emitter set (character pack entry 9) on characters 0x37, 0x98
 *                        and 0x99 while fighter effect request 0x3A is set.
 *
 * Task classes are {update, init, term, post-update, reset, draw}. All structs are this file's views.
 */

/* The task view shared by the effect modules (0x40 bytes, 0x1AD150..). */
typedef struct EftQTask {
    /* 0x00 */ u8 flags;       /* bit 0: dead (set by BtlTask_SetDead); read through EFTQ_TASK_FLAGS */
    /* 0x01 */ u8 step;        /* free for the class; the flash keeps its phase here, the root its character */
    /* 0x02 */ u8 unk2[0x26];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftQTask;

/* The flag byte as the original reads it: a one-byte bit-field (as a member of the word-aligned task struct the
   compiler would use word loads). */
typedef struct EftQTaskFlags {
    u8 dead : 1;
} EftQTaskFlags;
#define EFTQ_TASK_FLAGS(t) ((EftQTaskFlags *)&(t)->flags)

/* A vector passed by value (16-byte aligned, copied by the callee). */
typedef struct EftQVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) EftQVec;

/* One entry of a texture set (EftTexSet_Load32). */
typedef struct EftQTex {
    /* 0x0 */ u64 tex0;
    /* 0x8 */ u64 unk8;
} EftQTex;

typedef struct EftQTexSet {
    /* 0x000 */ EftQTex entry[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ s32 unk204;
} EftQTexSet; /* size 0x208 */

/* ---- 1. power-up glow ------------------------------------------------------------------------------------ */

/* Work of a glow task (class 0x2C3B68), 0x2E0 bytes; one per character. Only what this file touches is named. */
typedef struct EftGlow {
    /* 0x000 */ s32 flags;       /* EFT_GLOW_* */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ s32 paramFlags;  /* BtlCharApi_ObjGetParamFlags0 */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ s32 step;        /* 0, 1, then 2 for good: counts the first two unpaused updates */
    /* 0x014 */ s32 unk14;       /* set to 9 when the fade-out starts */
    /* 0x018 */ s32 unk18;
    /* 0x01C */ s32 type;        /* aura type: row of the colour table and texture of the set */
    /* 0x020 */ s32 type2;       /* the same value */
    /* 0x024 */ s32 layers;      /* 1, or 2 for the textured variant */
    /* 0x028 */ s32 texFirst;    /* textured variant: first texture */
    /* 0x02C */ s32 texCount;    /* and how many */
    /* 0x030 */ s32 unk30;
    /* 0x034 */ s32 fade;        /* 2 = fading out */
    /* 0x038 */ f32 fadeTime;    /* cfg->fadeOutTime */
    /* 0x03C */ f32 fadeFrom;    /* 1 */
    /* 0x040 */ u8 unk40[8];
    /* 0x048 */ f32 scale;       /* fighter height / 19.35 */
    /* 0x04C */ s32 objId;
    /* 0x050 */ u8 unk50[0x140];
    /* 0x190 */ Vec4 colorA;
    /* 0x1A0 */ Vec4 colorB;     /* w = 0 */
    /* 0x1B0 */ u8 unk1B0[0xB0];
    /* 0x260 */ Vec4 color[4];
    /* 0x2A0 */ u64 tex[8];      /* GS TEX0: [0] plain, [1..] the textured variant */
} EftGlow; /* size 0x2E0 */

/* The textured variant's TEX0 words as the original addresses them: relative to +8 of the work. */
typedef struct EftGlowTexView {
    /* 0x000 */ u8 unk0[0x2A0];
    /* 0x2A0 */ u64 tex[7];      /* EftGlow.tex[1..] */
} EftGlowTexView;

#define EFT_GLOW_ACTIVE   0x01  /* set by EftGlow_Begin (eft_p_b.c) */
#define EFT_GLOW_FADING   0x02
#define EFT_GLOW_KILL     0x04
#define EFT_GLOW_MODELNEW 0x10  /* during EftGlow_Setup of a frame in which the fighter's model changed */
#define EFT_GLOW_TEXTURED 0x20  /* aura types 8..10 */

/* One texture resource of the manager. */
typedef struct EftGlowRes {
    /* 0x000 */ s32 ready;       /* 0 after the manager's update, 1 once a task refreshed the textures */
    /* 0x004 */ u8 unk4[0x14];
    /* 0x018 */ s32 *entry;      /* common pack entry 9 / 8 */
    /* 0x01C */ s32 unk1C;
    /* 0x020 */ EftQTexSet tex;
} EftGlowRes; /* size 0x228 */

/* gEftGlow: 0x700 bytes from the effect pool. */
typedef struct EftGlowMgr {
    /* 0x000 */ s32 charCount;
    /* 0x004 */ s32 partMax;     /* 70 per character */
    /* 0x008 */ void *parts;     /* partMax particles of 0xC0 bytes */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ s32 unk10;
    /* 0x014 */ s32 unk14;
    /* 0x018 */ s32 live;        /* glow tasks alive */
    /* 0x01C */ u8 unk1C[0x284];
    /* 0x2A0 */ EftGlowRes res[2];
    /* 0x6F0 */ EftQTask **tasks; /* one per character */
    /* 0x6F4 */ s32 *cfg;        /* common pack entry 7 */
    /* 0x6F8 */ u8 unk6F8[8];
} EftGlowMgr; /* size 0x700 */

/* The colour tables in common pack entry 7 are rows of five colours (bytes 0..255 as floats): the base colour and
   four layer colours. */
#define EFT_GLOW_ROW 5

/* gEftGlowCfg (common pack entry 7): what this file reads. */
typedef struct EftGlowCfg {
    /* 0x000 */ u8 unk0[0x1AC];
    /* 0x1AC */ f32 fadeOutTime;
    /* 0x1B0 */ u8 unk1B0[0x470];
    /* 0x620 */ Vec4 color[9 * EFT_GLOW_ROW];     /* by aura type */
    /* 0x8F0 */ Vec4 colorTex[4 * EFT_GLOW_ROW];  /* the textured variant */
} EftGlowCfg;

/* ---- emitter sets (the effect pack library, eft_h.c / eft_i.c) ----------------------------------------------- */

/* Per-task state of an emitter set (EftEmit_InitState). */
typedef struct EftQEmitState {
    u8 unk0[0x2CC];
} EftQEmitState;

typedef struct EftQEmitGroupDef {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 count;       /* emitters in the group */
} EftQEmitGroupDef;

typedef struct EftQEmitGroup {
    /* 0x0 */ EftQEmitGroupDef *def;
    /* 0x4 */ u8 unk4[0xC];
} EftQEmitGroup; /* size 0x10 */

/* An emitter set (EftEmit_LoadSet): 0x13 groups. */
typedef struct EftQEmitSet {
    /* 0x000 */ u32 *mask;    /* bit n: group n exists */
    /* 0x004 */ u8 unk4[8];
    /* 0x00C */ EftQEmitGroup group[0x13];
    /* 0x13C */ u8 unk13C[0x1E0];
    /* 0x31C */ s32 unk31C;
    /* 0x320 */ s32 *pack;    /* the pack entry it was loaded from, or NULL */
} EftQEmitSet; /* size 0x324 */

/* Work of a manager task that owns one emitter set. */
typedef struct EftQSetMgr {
    /* 0x0 */ EftQEmitSet *set;
} EftQSetMgr;

/* ---- 2. rush-finish burst (fighter effect requests 0x28..0x2A) --------------------------------------------- */

/* Argument of EftRushBurst_Start: the fighter layer's FxDirArg. */
typedef struct EftRushBurstArg {
    /* 0x00 */ EftQVec dir;
    /* 0x10 */ s32 objId;
    /* 0x14 */ s32 node;      /* fighter +0x1354 */
    /* 0x18 */ f32 scale;
    /* 0x1C */ s32 unk1C;
} EftRushBurstArg; /* size 0x20 */

#define EFT_RUSHBURST_ACTIVE  0x01  /* set at init */
#define EFT_RUSHBURST_END     0x02  /* request 0x2A: ending, the timer runs */
#define EFT_RUSHBURST_STAGE2  0x04  /* request 0x29: second stage asked for */
#define EFT_RUSHBURST_STAGE2D 0x08  /* second stage started */
#define EFT_RUSHBURST_KILL    0x10
#define EFT_RUSHBURST_ALIVE   0x20  /* a particle is alive */
#define EFT_RUSHBURST_POSTED  0x40  /* the post-update callback has run once */
#define EFT_RUSHBURST_RESET   0x80  /* the reset callback ran */

/* Work of a burst task (class 0x2C3B98), 0x580 bytes. */
typedef struct EftRushBurst {
    /* 0x000 */ Vec4 pos;            /* the fighter's node */
    /* 0x010 */ Vec4 dir;            /* arg.dir, normalised */
    /* 0x020 */ EftRushBurstArg arg;
    /* 0x040 */ EftQEmitSet *set;    /* the manager's emitter set */
    /* 0x044 */ EftQEmitState state;
    /* 0x310 */ u8 nodes[0x250];     /* node slots of the set */
    /* 0x560 */ u32 flags;           /* EFT_RUSHBURST_* */
    /* 0x564 */ f32 scale;
    /* 0x568 */ u32 phase;           /* phase mask handed to EftEmit_GetFlagsFromMask: 1, then |2 at stage 2 */
    /* 0x56C */ f32 timer;           /* frames since the end began */
    /* 0x570 */ f32 life;            /* EftEmit_GetEndFrames */
    /* 0x574 */ u8 unk574[0xC];
} EftRushBurst; /* size 0x580 */

/* ---- 3. per-character effect root ---------------------------------------------------------------------------- */

#define EFT_CHAR_KINDS 10

/* One character's entry (0x30 bytes). */
typedef struct EftCharEntry {
    /* 0x00 */ EftQTask *task;              /* the character's task (class 0x2C3BC8), NULL after EftChar_Kill */
    /* 0x04 */ void *list;                  /* its child list: one manager task per kind */
    /* 0x08 */ void *lists[EFT_CHAR_KINDS]; /* the list each kind's manager registered (EftChar_SetList) */
} EftCharEntry; /* size 0x30 */

/* gEftChar: 0xC bytes from pool 6. */
typedef struct EftCharRoot {
    /* 0x0 */ void *list;           /* child list of the root task: the character tasks */
    /* 0x4 */ EftCharEntry *chars;
    /* 0x8 */ s32 count;            /* BtlScene_GetCharCount() */
} EftCharRoot;

/* ---- 5. full-screen flash -------------------------------------------------------------------------------------- */

/* Argument of EftFlash_Start / EftFlash_StartOwned, and the first 0x30 bytes of the task's work. */
typedef struct EftFlashArg {
    /* 0x00 */ EftQVec color;   /* r, g, b, a as 0..255 (a: 128 = opaque) */
    /* 0x10 */ f32 in;          /* seconds to fade in */
    /* 0x14 */ f32 hold;        /* seconds to hold */
    /* 0x18 */ f32 out;         /* seconds to fade out */
    /* 0x1C */ s32 objId;       /* EftFlash_Start writes -1 here */
    /* 0x20 */ s32 wait;        /* 1: hold until the rush finish reaches state 2 */
    /* 0x24 */ u8 unk24[0xC];
} EftFlashArg; /* size 0x30 */

/* Work of the flash task (class 0x2C3C58), 0x50 bytes. The times are frames after init. */
typedef struct EftFlash {
    /* 0x00 */ EftFlashArg arg;
    /* 0x30 */ f32 fade;        /* 1 = invisible, 0 = full colour */
    /* 0x34 */ f32 inMax;
    /* 0x38 */ f32 outMax;
    /* 0x3C */ s32 pause;       /* nonzero freezes the task (EftFlash_SetPause) */
    /* 0x40 */ s32 phase;       /* 1 fading in, 2 holding, 4 fading out (EftFlash_GetPhase) */
    /* 0x44 */ s32 frames;      /* frames spent holding; 180 forces the fade-out */
    /* 0x48 */ u8 unk48[8];
} EftFlash; /* size 0x50 */

/* gEftFlash: 0x10 bytes from the effect pool. */
typedef struct EftFlashMgr {
    /* 0x0 */ void *list;       /* child list, one task */
    /* 0x4 */ s32 flags;        /* 1 a flash is running, 2 / 4 it was started for character 0 / 1's action 0x104 */
    /* 0x8 */ s32 rushState;    /* EftFlash_GetRushState(), refreshed every frame while a flash runs */
    /* 0xC */ s32 rushBits;     /* 1 / 2: character 0 / 1 is in rush-finish phase 1; 4 / 8: phase 2 */
} EftFlashMgr;

/* A GS screen position as the projection helpers write it. */
typedef struct EftQScr {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 unkC;
} EftQScr; /* size 0x10 */

/* One vertex as ClipVtx_Set builds it for EftGfx_DrawPolyAvgZ. */
typedef struct EftQVert {
    u8 unk0[0x30];
} EftQVert;

/* GS registers and the ordering-table packets this file queues. */
typedef struct EftQXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftQXyzf;

typedef struct EftQRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftQRgbaq;

typedef struct EftQSt {
    f32 s, t;
} EftQSt;

/* DMA tag + REGLIST GIF tag (4 registers): PRIM, RGBAQ, two XYZF2: one flat sprite. */
typedef struct EftQSpritePkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000003: NEXT, 3 quadwords */
    /* 0x04 */ struct EftQSpritePkt *next;
    /* 0x08 */ u32 vif0;         /* 0x10000000 */
    /* 0x0C */ u32 vif1;         /* 0x50000003: DIRECT, 3 quadwords */
    /* 0x10 */ u64 gifTag;       /* 0x4400000000008001: NLOOP 1, EOP, REGLIST, 4 registers */
    /* 0x18 */ u64 regs;         /* 0x4410 */
    /* 0x20 */ u64 prim;         /* 0x46: sprite, alpha blending */
    /* 0x28 */ EftQRgbaq rgbaq;
    /* 0x30 */ EftQXyzf xyz0;
    /* 0x38 */ EftQXyzf xyz1;
} EftQSpritePkt; /* size 0x40 */

/* DMA tag + REGLIST GIF tag (12 registers): PRIM, TEX0, RGBAQ, four (ST, XYZF2), NOP: one flat textured strip. */
typedef struct EftQQuadPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000007 */
    /* 0x04 */ struct EftQQuadPkt *next;
    /* 0x08 */ u32 vif0;         /* 0x10000000 */
    /* 0x0C */ u32 vif1;         /* 0x50000007 */
    /* 0x10 */ u64 gifTag;       /* 0xC400000000008001 */
    /* 0x18 */ u64 regs;         /* 0xF42424242160 */
    /* 0x20 */ u64 prim;         /* 0x54: triangle strip, textured, alpha blending */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftQRgbaq rgbaq;
    /* 0x38 */ EftQSt st0;
    /* 0x40 */ EftQXyzf xyz0;
    /* 0x48 */ EftQSt st1;
    /* 0x50 */ EftQXyzf xyz1;
    /* 0x58 */ EftQSt st2;
    /* 0x60 */ EftQXyzf xyz2;
    /* 0x68 */ EftQSt st3;
    /* 0x70 */ EftQXyzf xyz3;
    /* 0x78 */ u64 nop;
} EftQQuadPkt; /* size 0x80 */

/* ---- 6. trails ------------------------------------------------------------------------------------------------ */

#define EFT_TRAIL_POINTS 20

/* A table of GS texture words shared by the trails of one frame. */
typedef struct EftTrailTexTbl {
    /* 0x000 */ EftQTex tex[16];
    /* 0x100 */ s32 unk100;
    /* 0x104 */ u32 loaded;      /* bit n: tex[n..] hold this frame's words; cleared by the manager's update */
} EftTrailTexTbl; /* size 0x108 */

/* gEftTrail: 0x110 bytes from the effect pool. */
typedef struct EftTrailMgr {
    /* 0x000 */ void *list;      /* child list, 15 tasks */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ EftTrailTexTbl tbl;
} EftTrailMgr; /* size 0x110 */

/* Work of a trail task (class 0x2C3C88), 0x220 bytes; the spawners build one on the stack as the init argument. */
typedef struct EftTrail {
    /* 0x000 */ EftTrailTexTbl *tbl;
    /* 0x004 */ s32 objId;       /* owner: the task stops and hides with it */
    /* 0x008 */ u8 kind;         /* second argument of BtlScene_IsEffectStopped / IsEffectHidden */
    /* 0x00C */ f32 speed;       /* grows by accel up to 100; never applied to the position */
    /* 0x010 */ f32 accel;
    /* 0x014 */ f32 turn;        /* weight of the pull towards the target; grows by turnAccel */
    /* 0x018 */ f32 turnAccel;
    /* 0x01C */ f32 width;       /* half width of the strip */
    /* 0x020 */ f32 width0;      /* width when the fade began */
    /* 0x024 */ s32 life;        /* frames left, negative = endless */
    /* 0x028 */ s32 flags;       /* 1 = fading (EftTrail_End) */
    /* 0x02C */ s32 fadeFrames;
    /* 0x030 */ Vec4 *target;
    /* 0x034 */ u8 unk34[0xC];
    /* 0x040 */ Vec4 pos;        /* head; moved only through EftTrail_SetPos */
    /* 0x050 */ Vec4 dir;
    /* 0x060 */ Vec4 color;      /* w = alpha while fading */
    /* 0x070 */ Vec4 pts[EFT_TRAIL_POINTS]; /* [0] newest */
    /* 0x1B0 */ s32 count;       /* points in use: 20, in split screen 15 / 8 */
    /* 0x1B4 */ s32 texFirst;    /* first entry of tbl this trail uses */
    /* 0x1B8 */ s32 texCount;
    /* 0x1BC */ s32 unk1BC;
    /* 0x1C0 */ EftQTex tex[4];
    /* 0x200 */ u64 tex0[4];     /* this frame's GS TEX0: [0] middle, [1] head, [2] tail segment, [3] glow sprite */
} EftTrail; /* size 0x220 */

/* ---- 7. character body effect ------------------------------------------------------------------------------- */

/* Argument of EftCharaFx_Start: the first 8 bytes of the fighter layer's FxArg2. */
typedef struct EftCharaFxArg {
    /* 0x0 */ s32 objId;
    /* 0x4 */ f32 scale;
} EftCharaFxArg; /* size 0x8 */

#define EFT_CHARAFX_ACTIVE  0x01
#define EFT_CHARAFX_END     0x02  /* asked to end (EftCharaFx_Stop, 0x174A70) */
#define EFT_CHARAFX_KILL    0x04
#define EFT_CHARAFX_ALIVE   0x08  /* a particle of the first set is alive */
#define EFT_CHARAFX_POSTED  0x10
#define EFT_CHARAFX_KILLED  0x20  /* the particles were destroyed */
#define EFT_CHARAFX_PAIR    0x40  /* character 0x37: two emitters, offset to both sides of the node */

/* Work of a body effect task (class 0x2C3CC8), 0x860 bytes. */
typedef struct EftCharaFx {
    /* 0x000 */ Vec4 pos[2];
    /* 0x020 */ Vec4 dir;            /* from node nodeA towards node nodeB */
    /* 0x030 */ EftCharaFxArg arg;
    /* 0x038 */ EftQEmitSet *set;
    /* 0x03C */ EftQEmitState state[2];
    /* 0x5D4 */ u8 unk5D4[0xC];
    /* 0x5E0 */ u8 nodes[0x250];
    /* 0x830 */ u32 flags;           /* EFT_CHARAFX_* */
    /* 0x834 */ f32 scale;
    /* 0x838 */ u32 phase;
    /* 0x83C */ f32 unk83C;
    /* 0x840 */ f32 life;            /* EftEmit_GetEndFrames */
    /* 0x844 */ s32 node;            /* where the particles come from */
    /* 0x848 */ s32 nodeA;
    /* 0x84C */ s32 nodeB;
    /* 0x850 */ f32 offset;          /* PAIR: distance of each emitter from the node along its x axis */
    /* 0x854 */ u8 unk854[0xC];
} EftCharaFx; /* size 0x860 */

/* ---- functions, in address order --------------------------------------------------------------------------------- */

void EftGlow_UpdateTextures(EftGlow *w);
void EftGlow_SetType(EftGlow *w, s32 objId, s32 type, s32 paramFlags);
void EftGlow_Setup(EftGlow *w, s32 objId);
void EftGlowTask_Init(EftQTask *task, s32 *arg);
void EftGlowTask_Term(EftQTask *task);
void EftGlowTask_Update(EftQTask *task);
void EftGlowTask_PostUpdate(EftQTask *task);
void EftGlowTask_Reset(EftQTask *task);
void EftGlowTask_Draw(EftQTask *task);
void EftGlowMgr_Init(EftQTask *task);
void EftGlowMgr_Term(EftQTask *task);
void EftGlowMgr_Update(EftQTask *task);
s32 EftGlow_Start(s32 objId);
s32 EftGlow_FadeOut(s32 objId);
s32 EftGlow_Kill(s32 objId);
s32 EftGlow_Request(s32 objId, s32 cmd);
s32 EftGlow_IsActive(s32 objId);
void EftGlow_ChangeType(s32 objId, s32 type, s32 paramFlags);
Vec4 *EftGlow_GetColors(s32 type, s32 paramFlags);
void EftRushBurst_Emit(s32 objId, EftQTask *task, EftQEmitSet *set);
void EftRushBurst_Init(EftQTask *task, EftRushBurstArg *arg);
void EftRushBurst_Term(EftQTask *task);
void EftRushBurst_Update(EftQTask *task);
void EftRushBurst_Reset(EftQTask *task);
void EftRushBurst_PostUpdate(EftQTask *task);
void EftRushBurst_Draw(EftQTask *task);
void EftRushBurstMgr_Init(EftQTask *task, s32 *arg);
void EftRushBurstMgr_Term(EftQTask *task);
void EftRushBurstMgr_Update(EftQTask *task);
void EftRushBurst_Start(EftRushBurstArg *arg);
s32 EftRushBurst_Stage2(s32 objId);
s32 EftRushBurst_Stop(s32 objId);
void EftRushBurst_Nop(void);
void EftCharRoot_Init(EftQTask *task);
void EftCharRoot_Term(EftQTask *task);
void EftCharRoot_Update(EftQTask *task);
void EftChar_Init(EftQTask *task, s32 chr);
void EftChar_Term(EftQTask *task);
void EftChar_Update(EftQTask *task);
void EftChar_SetPool(s32 chr);
void EftChar_ResetPool(s32 chr);
void EftChar_CreateKind(s32 chr, s32 kind, s32 mode);
void EftChar_Kill(s32 chr);
void EftChar_Create(s32 chr);
void EftChar_SetList(s32 chr, s32 kind, void *list);
void *EftChar_GetList(s32 chr, s32 kind);
EftQTask *EftCharNull_Start(s32 chr);
void EftCharNullMgr_Init(EftQTask *task, s32 *arg);
void EftCharNullMgr_Term(EftQTask *task);
void EftCharNullMgr_Update(EftQTask *task);
void EftCharNull_Init(EftQTask *task, s32 chr);
void EftCharNull_Term(EftQTask *task);
void EftCharNull_Reset(EftQTask *task);
void EftCharNull_Update(EftQTask *task);
void EftCharNull_PostUpdate(EftQTask *task);
void EftCharNull_Draw(EftQTask *task);
EftQTask *EftFlash_Start(EftFlashArg *arg);
EftQTask *EftFlash_StartOwned(EftFlashArg *arg);
s32 EftFlash_GetPhase(EftQTask *task);
void EftFlash_SetPause(EftQTask *task, s32 pause);
void EftFlashMgr_Init(EftQTask *task);
void EftFlashMgr_Term(EftQTask *task);
void EftFlashMgr_Update(EftQTask *task);
void EftFlashMgr_Reset(EftQTask *task);
void EftFlash_Init(EftQTask *task, EftFlashArg *arg);
void EftFlash_Term(EftQTask *task);
void EftFlash_Reset(EftQTask *task);
void EftFlash_Update(EftQTask *task);
void EftFlash_Draw(EftQTask *task);
s32 EftFlash_GetRushState(void);
void EftTrail_Start(Vec4 *pos, Vec4 *dir, s32 life, Vec4 *target, f32 speed, f32 accel, f32 turn, f32 turnAccel);
void EftTrail_StartOwned(u8 kind, s32 objId, Vec4 *pos, Vec4 *color, EftTrailTexTbl *tbl, f32 width);
void EftTrail_End(EftQTask *task, s32 frames);
void EftTrail_SetColor(EftQTask *task, Vec4 *color);
void EftTrail_GetPos(EftQTask *task, Vec4 *out);
void EftTrail_SetPos(EftQTask *task, Vec4 *pos);
void EftTrail_SetWidth(EftQTask *task, f32 width);
void EftTrailMgr_Init(EftQTask *task);
void EftTrailMgr_Term(EftQTask *task);
void EftTrailMgr_Update(EftQTask *task);
void EftTrail_Init(EftQTask *task, EftTrail *arg);
void EftTrail_Term(EftQTask *task);
void EftTrail_Reset(EftQTask *task);
void EftTrail_Update(EftQTask *task);
void EftTrail_Draw(EftQTask *task);
void EftTrail_UpdateTextures(EftTrail *w);
void EftTrail_SetTextures(EftTrail *w, EftQTex *src);
void EftTrail_DrawSprite(u8 r, u8 g, u8 b, u8 a, f32 x, f32 y, f32 z, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 scale, f32 rot, s32 layer, u64 tex0);
void EftCharaFx_Emit(s32 objId, EftQTask *task, EftQEmitSet *set, EftQEmitState *state, Vec4 *pos);
void EftCharaFx_Init(EftQTask *task, EftCharaFxArg *arg);
void EftCharaFx_Term(EftQTask *task);
void EftCharaFx_Update(EftQTask *task);
void EftCharaFx_Reset(EftQTask *task);
void EftCharaFx_PostUpdate(EftQTask *task);
void EftCharaFx_Draw(EftQTask *task);
void EftCharaFxMgr_Init(EftQTask *task, s32 *arg);
void EftCharaFxMgr_Term(EftQTask *task);
void EftCharaFxMgr_Update(EftQTask *task);
s32 EftCharaFx_Start(EftCharaFxArg *arg);

#endif
