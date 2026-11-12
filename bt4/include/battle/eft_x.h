#ifndef BATTLE_EFT_X_H
#define BATTLE_EFT_X_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect tasks, 0x18D618..0x191D28. Everything here only builds draw data.
 *
 *   eft_x.c    0x18D618..0x190CC8  second half of the particle emitter module of effect pack part kind 10
 *                                  (EftPart10_*). The module starts at 0x18C190 in eft_w.c (manager class
 *                                  0x2C3F68, emitter class gEftPart10Class 0x2C3F80 = { update 0x18C4E8, init
 *                                  0x18C238, term 0x18CA38, stub 0x18C7C0, reset 0x18CA20, draw 0x18C7C8 }).
 *                                  Here: its list helpers, the particle spawn, the four ways a particle is drawn,
 *                                  the emitter's spin and key-frame animation, its textures, and the calls the
 *                                  effect pack library makes (EftEmit_SpawnType10 in eft_i.c).
 *   eft_x_b.c  0x190CC8..0x190DA8  effect scene layer 3 (EftLayer3_*): the root task that creates one manager
 *                                  task per entry of gEftLayer3Classes (0x2C3FB0, 31 effect modules).
 *   eft_x_c.c  0x190DA8..0x191D28  first half of the quad emitter module of effect pack part kind 9
 *                                  (EftQuadMgr_*, EftQuad_*): the manager, the emitter task callbacks and the
 *                                  quad initialiser. It continues in eft_y.c, whose header has the fuller view
 *                                  of the same structures (EftQuadWork = EftPart9 here, EftQuad = EftPart9Ptcl).
 *
 * All structs are this file's views.
 */

/* The effect code's vector: 16-byte aligned, passed by value with a callee copy. */
typedef struct EftXVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) EftXVec;

/* A task as the callbacks here use it (see eft_g.h). */
typedef struct EftXTask {
    /* 0x00 */ u8 flags;
    /* 0x01 */ u8 unk1[0x27];
    /* 0x28 */ void **cls;     /* class table; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftXTask;

/* One texture of an effect pack: the GS TEX0 value and a second word. */
typedef struct EftXTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftXTexEntry; /* 0x10 */

/* A pack's texture block (0x108 bytes, EftEmitSet.tex33 in eft_i.h). */
typedef struct EftXTexSet {
    /* 0x000 */ EftXTexEntry entry[16];
    /* 0x100 */ s32 unk100;
    /* 0x104 */ u32 ready;     /* bit n: entry n holds a blended TEX0 built by EftPart10_BuildTex */
} EftXTexSet; /* 0x108 */

/* A GS screen position as Vu0Cur_ProjectPoint / Vu0Cur_ProjectPointsStq / Vec4_ToInt write it. */
typedef struct EftXIVec {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftXIVec;

/* ---- part kind 10: particle emitter -------------------------------------------------------------------- */

#define EFT_PART10_MAX 150
#define EFT_PART10_KEYS 3      /* key frames per animated value: start, middle, end */

/* Emitter flags (EftPart10.flags). */
#define EFT_PART10_ALIVE 0x00001
#define EFT_PART10_DONE 0x00002       /* nothing left: the update kills the task */
#define EFT_PART10_NOEMIT 0x00004     /* stopped: no new particles */
#define EFT_PART10_HOLD 0x00008       /* stop requested, hold timer running */
#define EFT_PART10_FADE 0x00010       /* stop requested, fade timer running */
#define EFT_PART10_KILL 0x00020       /* kill at the next update */
#define EFT_PART10_TEXGRID 0x00100    /* the texture is a grid of frames */
#define EFT_PART10_NODEPTH 0x00400    /* drawn with the far depth value */
#define EFT_PART10_KEY2 0x20000       /* the key-frame animation is in its second segment */
#define EFT_PART10_KEYANIM 0x40000    /* the key-frame animation runs */
#define EFT_PART10_STOPPED 0x80000    /* EftPart10_Stop was called */

/* Particle flags (EftPart10Ptcl.flags). */
#define EFT_PART10_P_ALIVE 0x0001
#define EFT_PART10_P_DRAW 0x0040
#define EFT_PART10_P_UNK80 0x0080
#define EFT_PART10_P_MIRROR 0x0200    /* texture mirrored (negative spin) */
#define EFT_PART10_P_SCALEX 0x2000
#define EFT_PART10_P_SCALEY 0x4000
#define EFT_PART10_P_SCALEZ 0x8000

/* A value animated over the emitter's life: the current value and its change over the current segment. */
typedef struct EftPart10Key {
    /* 0x0 */ f32 v;
    /* 0x4 */ f32 d;
} EftPart10Key;

typedef struct EftPart10Key2 {
    /* 0x0 */ f32 a;
    /* 0x4 */ f32 b;
} EftPart10Key2;

/* Emitter definition, first block (EftEmitRes.unk0 of the pack). Every value has three keys. */
typedef struct EftPart10Def {
    /* 0x000 */ f32 size[EFT_PART10_KEYS][3];     /* particle size at birth / middle / death: minimum */
    /* 0x024 */ f32 sizeRange[EFT_PART10_KEYS][3];
    /* 0x048 */ f32 angle[EFT_PART10_KEYS];  /* start angle, turns */
    /* 0x054 */ f32 angleRange[EFT_PART10_KEYS];
    /* 0x060 */ f32 unkC0[EFT_PART10_KEYS];  /* half turns */
    /* 0x06C */ f32 unkC8[EFT_PART10_KEYS];
    /* 0x078 */ f32 unkD0[EFT_PART10_KEYS];
    /* 0x084 */ f32 unkD8[EFT_PART10_KEYS];
    /* 0x090 */ f32 spin[EFT_PART10_KEYS];  /* spin, half turns */
    /* 0x09C */ f32 spinRange[EFT_PART10_KEYS];
    /* 0x0A8 */ f32 unkF0[EFT_PART10_KEYS];  /* burst direction jitter */
    /* 0x0B4 */ f32 unkF8[EFT_PART10_KEYS];
    /* 0x0C0 */ f32 unk100[EFT_PART10_KEYS];
    /* 0x0CC */ f32 unk108[EFT_PART10_KEYS];
    /* 0x0D8 */ f32 speed[EFT_PART10_KEYS];
    /* 0x0E4 */ f32 speedRange[EFT_PART10_KEYS];
    /* 0x0F0 */ f32 speedBase[EFT_PART10_KEYS];
    /* 0x0FC */ f32 unk128[EFT_PART10_KEYS];
    /* 0x108 */ f32 unk130[EFT_PART10_KEYS];
    /* 0x114 */ f32 scaleX[EFT_PART10_KEYS];
    /* 0x120 */ f32 scaleY[EFT_PART10_KEYS];
    /* 0x12C */ f32 life[EFT_PART10_KEYS];  /* seconds */
    /* 0x138 */ f32 lifeRange[EFT_PART10_KEYS];
    /* 0x144 */ f32 unk158[EFT_PART10_KEYS];  /* seconds */
    /* 0x150 */ f32 unk160[EFT_PART10_KEYS];
    /* 0x15C */ f32 unk168[EFT_PART10_KEYS][2];
    /* 0x174 */ f32 unk178[EFT_PART10_KEYS];
    /* 0x180 */ f32 unk180;
    /* 0x184 */ f32 spinA[3];                   /* emitter spin (flag 0x40): three keys or one range */
    /* 0x190 */ f32 spinARange[3];
    /* 0x19C */ f32 spinB[3];
    /* 0x1A8 */ f32 spinBRange[3];
    /* 0x1B4 */ f32 scale[3];
    /* 0x1C0 */ f32 scaleRange[3];
    /* 0x1CC */ f32 count[3];                   /* particles per burst: three keys */
    /* 0x1D8 */ f32 spinTime;                   /* seconds */
    /* 0x1DC */ f32 spinMid;                    /* 0..1 */
    /* 0x1E0 */ f32 keyTime;                    /* seconds the key-frame animation lasts */
    /* 0x1E4 */ f32 keyMid;                     /* 0..1: where the middle key is */
    /* 0x1E8 */ f32 unk1E8;
    /* 0x1EC */ f32 unk1EC;
    /* 0x1F0 */ f32 sizeMid;                    /* 0..1: where a particle's middle size is */
    /* 0x1F4 */ s32 flags;
    /* 0x1F8 */ u8 texCols;
    /* 0x1F9 */ u8 texRows;
    /* 0x1FA */ u8 unk1FA;
    /* 0x1FB */ u8 layer;
    /* 0x1FC */ f32 unk1FC;
    /* 0x200 */ f32 unk200;
    /* 0x204 */ f32 unk204;
    /* 0x208 */ f32 unk208;
    /* 0x20C */ u8 drawMode;                    /* 1: facing the camera */
    /* 0x20D */ u8 unk20D;
} EftPart10Def;

/* Emitter definition, second block (EftEmitRes.unk4). */
typedef struct EftPart10Def2 {
    /* 0x000 */ Vec4 color[EFT_PART10_KEYS];      /* 0..255 */
    /* 0x030 */ Vec4 colorRange[EFT_PART10_KEYS];
    /* 0x060 */ Vec4 endColor[EFT_PART10_KEYS];
    /* 0x090 */ Vec4 endColorRange[EFT_PART10_KEYS];
    /* 0x0C0 */ f32 stretchX[EFT_PART10_KEYS][2];
    /* 0x0D8 */ f32 stretchY[EFT_PART10_KEYS][2];
    /* 0x0F0 */ f32 stretchZ[EFT_PART10_KEYS][2];
    /* 0x108 */ f32 stretchTime[EFT_PART10_KEYS];
    /* 0x114 */ f32 unk248[EFT_PART10_KEYS];
    /* 0x120 */ f32 unk250[EFT_PART10_KEYS];
} EftPart10Def2;

struct EftPart10Ptcl;

/* The particles one emitter spawned in one burst. */
typedef struct EftPart10Group {
    /* 0x00 */ s32 flags;                       /* 0 = free slot */
    /* 0x04 */ struct EftPart10Ptcl *head;
    /* 0x08 */ struct EftPart10Ptcl *tail;
    /* 0x0C */ struct EftPart10Group *next;
    /* 0x10 */ struct EftPart10Group *prev;
} EftPart10Group; /* 0x14 */

typedef struct EftPart10Ptcl {
    /* 0x000 */ Vec4 pos;                       /* relative to the emitter */
    /* 0x010 */ Vec4 dir;
    /* 0x020 */ Vec4 scale;
    /* 0x030 */ Vec4 color0;
    /* 0x040 */ Vec4 colorStep;                 /* end colour - start colour */
    /* 0x050 */ Vec4 color;
    /* 0x060 */ Vec4 corner[4];
    /* 0x0A0 */ Vec4 uv0;                       /* texture coordinates of corners 0 and 1 */
    /* 0x0B0 */ Vec4 uv1;                       /* of corners 2 and 3 */
    /* 0x0C0 */ f32 speed;
    /* 0x0C4 */ f32 unkC4;
    /* 0x0C8 */ f32 unkC8;
    /* 0x0CC */ f32 unkCC;
    /* 0x0D0 */ f32 angle;
    /* 0x0D4 */ f32 unkD4;
    /* 0x0D8 */ f32 unkD8;
    /* 0x0DC */ f32 spin;
    /* 0x0E0 */ f32 size;
    /* 0x0E4 */ f32 sizeStep0;                  /* per frame until life * sizeMid */
    /* 0x0E8 */ f32 sizeStep1;
    /* 0x0EC */ f32 unkEC;
    /* 0x0F0 */ f32 unkF0;
    /* 0x0F4 */ f32 unkF4;                      /* extra size factor */
    /* 0x0F8 */ f32 unkF8;
    /* 0x0FC */ f32 unkFC;
    /* 0x100 */ f32 unk100;
    /* 0x104 */ f32 stretchStep[3];
    /* 0x110 */ f32 stretch[3];
    /* 0x11C */ f32 stretchAge;
    /* 0x120 */ f32 stretchTime;
    /* 0x124 */ f32 unk124;
    /* 0x128 */ f32 unk128;
    /* 0x12C */ f32 unk12C;
    /* 0x130 */ f32 texFrame;
    /* 0x134 */ f32 unk134;
    /* 0x138 */ f32 life;                       /* frames */
    /* 0x13C */ f32 age;
    /* 0x140 */ s32 flags;                      /* 0 = free slot */
    /* 0x144 */ struct EftPart10Ptcl *next;
    /* 0x148 */ struct EftPart10Ptcl *prev;
    /* 0x14C */ s32 pad14C;
} EftPart10Ptcl; /* 0x150 */

/* Emitter: the work of an emitter task (0x3D0 bytes). */
typedef struct EftPart10 {
    /* 0x000 */ u8 unk0[8];
    /* 0x008 */ EftXTexEntry texA;              /* the two pack textures the emitter blends */
    /* 0x018 */ EftXTexEntry texB;
    /* 0x028 */ u8 unk28[8];
    /* 0x030 */ EftPart10Def *def;                /* 0x30..0x80 is a copy of the creation argument */
    /* 0x034 */ EftPart10Def2 *def2;
    /* 0x038 */ EftXTexSet *tex;
    /* 0x03C */ s32 pad3C;
    /* 0x040 */ Vec4 pos;
    /* 0x050 */ Vec4 dir;
    /* 0x060 */ f32 size;
    /* 0x064 */ f32 rate;                       /* seconds between bursts */
    /* 0x068 */ s32 texIdxA;
    /* 0x06C */ s32 texIdxB;
    /* 0x070 */ s32 objId;
    /* 0x074 */ s32 pad74[3];
    /* 0x080 */ f32 kSize[3];                   /* current values of the EftPart10Def tracks, each followed */
    /* 0x08C */ f32 dSize[3];                   /* by its change over the current key segment */
    /* 0x098 */ f32 kSizeRange[3];
    /* 0x0A4 */ f32 dSizeRange[3];
    /* 0x0B0 */ EftPart10Key angle;
    /* 0x0B8 */ EftPart10Key angleRange;
    /* 0x0C0 */ EftPart10Key unkC0;
    /* 0x0C8 */ EftPart10Key unkC8;
    /* 0x0D0 */ EftPart10Key unkD0;
    /* 0x0D8 */ EftPart10Key unkD8;
    /* 0x0E0 */ EftPart10Key spin;
    /* 0x0E8 */ EftPart10Key spinRange;
    /* 0x0F0 */ EftPart10Key unkF0;
    /* 0x0F8 */ EftPart10Key unkF8;
    /* 0x100 */ EftPart10Key unk100;
    /* 0x108 */ EftPart10Key unk108;
    /* 0x110 */ EftPart10Key speed;
    /* 0x118 */ EftPart10Key speedRange;
    /* 0x120 */ EftPart10Key speedBase;
    /* 0x128 */ EftPart10Key unk128;
    /* 0x130 */ EftPart10Key unk130;
    /* 0x138 */ EftPart10Key scaleX;
    /* 0x140 */ EftPart10Key scaleY;
    /* 0x148 */ EftPart10Key life;
    /* 0x150 */ EftPart10Key lifeRange;
    /* 0x158 */ EftPart10Key unk158;
    /* 0x160 */ EftPart10Key unk160;
    /* 0x168 */ EftPart10Key2 unk168;
    /* 0x170 */ EftPart10Key2 dUnk168;
    /* 0x178 */ EftPart10Key unk178;
    /* 0x180 */ f32 keyTime;                    /* frames */
    /* 0x184 */ f32 keyMid;                     /* frames */
    /* 0x188 */ u8 unk188[8];
    /* 0x190 */ Vec4 color;                     /* current values of the EftPart10Def2 tracks */
    /* 0x1A0 */ Vec4 dColor;
    /* 0x1B0 */ Vec4 colorRange;
    /* 0x1C0 */ Vec4 dColorRange;
    /* 0x1D0 */ Vec4 endColor;
    /* 0x1E0 */ Vec4 dEndColor;
    /* 0x1F0 */ Vec4 endColorRange;
    /* 0x200 */ Vec4 dEndColorRange;
    /* 0x210 */ EftPart10Key2 stretchX;
    /* 0x218 */ EftPart10Key2 dStretchX;
    /* 0x220 */ EftPart10Key2 stretchY;
    /* 0x228 */ EftPart10Key2 dStretchY;
    /* 0x230 */ EftPart10Key2 stretchZ;
    /* 0x238 */ EftPart10Key2 dStretchZ;
    /* 0x240 */ EftPart10Key stretchTime;
    /* 0x248 */ EftPart10Key unk248;
    /* 0x250 */ EftPart10Key unk250;
    /* 0x258 */ f32 spinA;                      /* emitter spin per frame */
    /* 0x25C */ f32 spinB;
    /* 0x260 */ f32 rotA;                       /* emitter angles */
    /* 0x264 */ f32 rotB;
    /* 0x268 */ f32 scale;
    /* 0x26C */ f32 count;
    /* 0x270 */ f32 dSpinA[2];                  /* per frame, before / after spinMid */
    /* 0x278 */ f32 dSpinB[2];
    /* 0x280 */ f32 dScale[2];
    /* 0x288 */ f32 dCount[2];
    /* 0x290 */ f32 unk290;
    /* 0x294 */ f32 unk294;
    /* 0x298 */ f32 unk298;
    /* 0x29C */ u8 unk29C[0x100];               /* texture grid cells (see the init at 0x18C238) */
    /* 0x39C */ f32 texFrames;
    /* 0x3A0 */ f32 age;                        /* frames */
    /* 0x3A4 */ f32 delay;                      /* frames before the emitter starts */
    /* 0x3A8 */ f32 hold;                       /* frames it keeps emitting after EftPart10_Stop */
    /* 0x3AC */ f32 fade;                       /* frames left of the fade after EftPart10_Stop */
    /* 0x3B0 */ f32 fadeTime;
    /* 0x3B4 */ f32 spinTime;                   /* frames */
    /* 0x3B8 */ f32 spinMid;                    /* frames */
    /* 0x3BC */ u8 texSlot;                     /* entry of `tex` the blended TEX0 goes to */
    /* 0x3BD */ u8 kind;                        /* second argument of BtlScene_IsEffectStopped */
    /* 0x3BE */ u8 unk3BE[2];
    /* 0x3C0 */ s32 flags;
    /* 0x3C4 */ EftPart10Group *groupHead;
    /* 0x3C8 */ EftPart10Group *groupTail;
    /* 0x3CC */ s32 pad3CC;
} EftPart10; /* 0x3D0 */

/* The module's work (0xD0F0 bytes from the BtlPool, gEftPart10Mgr). */
typedef struct EftPart10Mgr {
    /* 0x0000 */ void *list;                    /* 12 emitter tasks */
    /* 0x0004 */ EftPart10Group group[EFT_PART10_MAX];
    /* 0x0BBC */ s32 padBBC;
    /* 0x0BC0 */ EftPart10Ptcl ptcl[EFT_PART10_MAX];
    /* 0xD0A0 */ s32 groupNext;                 /* where the search for a free group starts */
    /* 0xD0A4 */ s32 ptclNext;
    /* 0xD0A8 */ s32 padD0A8[2];
    /* 0xD0B0 */ Mtx44 ident;
} EftPart10Mgr; /* 0xD0F0 */

/* Creation argument (EftArg10 in eft_i.c). */
typedef struct EftPart10Arg {
    /* 0x00 */ EftPart10Def *def;
    /* 0x04 */ EftPart10Def2 *def2;
    /* 0x08 */ EftXTexSet *tex;
    /* 0x0C */ s32 padC;
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 dir;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 rate;
    /* 0x38 */ s32 texIdxA;
    /* 0x3C */ s32 texIdxB;
    /* 0x40 */ s32 objId;
    /* 0x44 */ s32 pad44[3];
} EftPart10Arg; /* 0x50 */

/* ---- part kind 9: quad emitter (EftQuad_* functions) ---------------------------------------------------- */

#define EFT_PART9_MAX 200

/* Emitter flags (EftPart9.flags). */
#define EFT_PART9_ALIVE 0x0001
#define EFT_PART9_NOEMIT 0x0002     /* stopped: no new particles; the task dies with its last particle */
#define EFT_PART9_HOLD 0x0004       /* stop requested, hold timer running */
#define EFT_PART9_FADE 0x0008       /* fade timer running */
#define EFT_PART9_DEAD 0x0010       /* the update kills the task */
#define EFT_PART9_KILL 0x0020
#define EFT_PART9_VIEWONLY 0x0080   /* drawn only in the view that shows the owner */
#define EFT_PART9_TEXGRID 0x0400    /* the texture is a grid of frames */
#define EFT_PART9_KEYANIM 0x2000    /* definition flag 0x10: the key-frame animation runs (eft_y.c) */

struct EftPart9Ptcl;

/* Emitter definition (first resource of the pack entry). Only what this file reads. */
typedef struct EftPart9Def {
    /* 0x000 */ u8 unk0[0x218];
    /* 0x218 */ f32 unk218;
    /* 0x21C */ f32 unk21C;
    /* 0x220 */ f32 sizeMid;
    /* 0x224 */ f32 unk224;
    /* 0x228 */ f32 unk228;
    /* 0x22C */ f32 unk22C;
    /* 0x230 */ f32 unk230;
    /* 0x234 */ s32 flags;
    /* 0x238 */ u8 unk238[2];
    /* 0x23A */ u8 interval;                    /* frames between bursts */
    /* 0x23B */ u8 unk23B[2];
    /* 0x23D */ u8 count;                       /* particles per burst */
    /* 0x23E */ u8 layer;
    /* 0x23F */ u8 texCols;
    /* 0x240 */ u8 texRows;
    /* 0x241 */ u8 unk241;
    /* 0x242 */ u8 unk242;
} EftPart9Def;

typedef struct EftPart9Ptcl {
    /* 0x000 */ Vec4 pos;
    /* 0x010 */ Vec4 origin;
    /* 0x020 */ Vec4 dir;
    /* 0x030 */ Vec4 vel;
    /* 0x040 */ Vec4 color0;
    /* 0x050 */ Vec4 colorStep;
    /* 0x060 */ Vec4 color;
    /* 0x070 */ Vec4 rot;
    /* 0x080 */ u8 unk80[0x40];
    /* 0x0C0 */ f32 uv[8];
    /* 0x0E0 */ f32 rotVel[3];
    /* 0x0EC */ f32 unkEC;
    /* 0x0F0 */ f32 unkF0;
    /* 0x0F4 */ f32 unkF4;
    /* 0x0F8 */ f32 stretchStep[3];
    /* 0x104 */ f32 unk104;
    /* 0x108 */ f32 unk108;
    /* 0x10C */ f32 unk10C;
    /* 0x110 */ f32 unk110;
    /* 0x114 */ f32 unk114;
    /* 0x118 */ f32 unk118;
    /* 0x11C */ f32 unk11C;
    /* 0x120 */ f32 stretch[3];
    /* 0x12C */ f32 stretchAge;
    /* 0x130 */ f32 stretchTime;
    /* 0x134 */ f32 unk134;
    /* 0x138 */ f32 unk138;
    /* 0x13C */ f32 size;
    /* 0x140 */ f32 sizeStep0;
    /* 0x144 */ f32 sizeStep1;
    /* 0x148 */ f32 unk148;
    /* 0x14C */ f32 unk14C;
    /* 0x150 */ f32 life;                       /* frames */
    /* 0x154 */ f32 age;
    /* 0x158 */ f32 texFrame;
    /* 0x15C */ f32 unk15C;
    /* 0x160 */ f32 unk160;
    /* 0x164 */ s32 flags;
    /* 0x168 */ u8 texSlot;
    /* 0x169 */ u8 unk169[3];
    /* 0x16C */ struct EftPart9Ptcl *next;
    /* 0x170 */ struct EftPart9Ptcl *prev;
    /* 0x174 */ u8 unk174[0xC];
} EftPart9Ptcl; /* 0x180 */

/* Emitter work (0x3D0 bytes). */
typedef struct EftPart9 {
    /* 0x000 */ EftPart9Def *def;
    /* 0x004 */ void *def2;
    /* 0x008 */ EftXTexSet *tex;
    /* 0x00C */ u8 unkC[0x24];
    /* 0x030 */ EftXVec pos;
    /* 0x040 */ EftXVec dir;
    /* 0x050 */ f32 rot[3];
    /* 0x05C */ f32 size;
    /* 0x060 */ f32 life;                       /* frames; 0 = until stopped */
    /* 0x064 */ f32 age;
    /* 0x068 */ f32 delay;
    /* 0x06C */ f32 hold;
    /* 0x070 */ f32 fadeTime;
    /* 0x074 */ f32 fade;
    /* 0x078 */ u8 unk78[0x21C];
    /* 0x294 */ f32 keyTime;                    /* frames the key-frame animation lasts */
    /* 0x298 */ u8 unk298[4];
    /* 0x29C */ f32 size0[3];
    /* 0x2A8 */ f32 size0Range[3];
    /* 0x2B4 */ f32 size1[3];
    /* 0x2C0 */ f32 size1Range[3];
    /* 0x2CC */ f32 arc;
    /* 0x2D0 */ f32 arcRange;
    /* 0x2D4 */ f32 offset[3][2];               /* minimum, range per axis */
    /* 0x2EC */ f32 lifeMin;
    /* 0x2F0 */ f32 lifeRange;
    /* 0x2F4 */ f32 speed;
    /* 0x2F8 */ f32 rotMin[3];
    /* 0x304 */ f32 rotRange[3];
    /* 0x310 */ f32 spin[3];
    /* 0x31C */ u8 unk31C[0xC];
    /* 0x328 */ f32 rotVelMin[3];
    /* 0x334 */ f32 rotVelRange[3];
    /* 0x340 */ f32 unk340;
    /* 0x344 */ f32 unk344;
    /* 0x348 */ f32 unk348;
    /* 0x34C */ u8 unk34C[4];
    /* 0x350 */ f32 colorMin[4];
    /* 0x360 */ f32 colorRange[4];
    /* 0x370 */ f32 endColorMin[4];
    /* 0x380 */ f32 endColorRange[4];
    /* 0x390 */ f32 unk390;
    /* 0x394 */ f32 unk394;
    /* 0x398 */ f32 stretch0[3];
    /* 0x3A4 */ f32 stretch1[3];
    /* 0x3B0 */ f32 stretchTime;
    /* 0x3B4 */ f32 interval;
    /* 0x3B8 */ f32 count;
    /* 0x3BC */ u8 texFrames;
    /* 0x3BD */ u8 objId;
    /* 0x3BE */ u8 texIdxA;
    /* 0x3BF */ u8 texIdxB;
    /* 0x3C0 */ u8 kind;
    /* 0x3C1 */ u8 unk3C1[3];
    /* 0x3C4 */ s32 flags;
    /* 0x3C8 */ EftPart9Ptcl *head;
    /* 0x3CC */ EftPart9Ptcl *tail;
} EftPart9; /* 0x3D0 */

/* The module's work (0x12C10 bytes, gEftQuadMgr). */
typedef struct EftPart9Mgr {
    /* 0x00000 */ EftPart9Ptcl ptcl[EFT_PART9_MAX];
    /* 0x12C00 */ s32 next;
    /* 0x12C04 */ void *list;                   /* 30 emitter tasks */
    /* 0x12C08 */ s32 pad[2];
} EftPart9Mgr; /* 0x12C10 */

/* Creation argument. */
typedef struct EftPart9Arg {
    /* 0x00 */ EftPart9Def *def;
    /* 0x04 */ void *def2;
    /* 0x08 */ EftXTexSet *tex;
    /* 0x0C */ s32 padC;
    /* 0x10 */ EftXVec pos;
    /* 0x20 */ EftXVec dir;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 life;                        /* seconds */
    /* 0x38 */ u8 objId;
} EftPart9Arg;

/* eft_x.c */
void EftPart10_LinkGroup(EftPart10Group **head, EftPart10Group **tail, EftPart10Group *g);
void EftPart10_UnlinkGroup(EftPart10Group **head, EftPart10Group **tail, EftPart10Group *p);
void EftPart10_Emit(EftPart10 *em, EftPart10Group *g, f32 yaw, f32 pitch);
s32 EftPart10_InitPtcl(EftPart10Ptcl *p, EftPart10 *em);
void EftPart10_BuildCorners(EftPart10Ptcl *p, EftPart10 *em);
void EftPart10_LinkPtcl(EftPart10Ptcl **head, EftPart10Ptcl **tail, EftPart10Ptcl *p);
void EftPart10_UnlinkPtcl(EftPart10Ptcl **head, EftPart10Ptcl **tail, EftPart10Ptcl *p);
void EftPart10_DrawQuad(Vec4 *corner, EftXVec uv0, EftXVec uv1, EftXVec color, s32 layer, s32 texIdx, s32 noDepth,
                        EftXTexEntry *tex);
void EftPart10_DrawQuadClipped(Vec4 *corner, EftXVec uv0, EftXVec uv1, EftXVec color, s32 layer, s32 texIdx,
                               s32 noDepth, EftXTexEntry *tex);
void EftPart10_DrawBillboardClipped(Vec4 *pos, f32 w, f32 h, Vec4 *color, Vec4 *scale, f32 u0, f32 v0, f32 u1,
                                    f32 v1, f32 rot, s32 layer, s32 noDepth, u64 tex0, f32 zScale);
void EftPart10_DrawBillboard(Vec4 *pos, Vec4 *color, f32 w, s32 offX, s32 offY, f32 h, f32 u0, f32 v0, s32 layer,
                             f32 u1, f32 v1, s32 noDepth, u64 tex0, f32 rot);
void EftPart10_InitSpin(EftPart10 *em);
void EftPart10_UpdateSpin(EftPart10 *em);
void EftPart10_StartKeys(EftPart10 *em);
void EftPart10_UpdateKeys(EftPart10 *em);
void EftPart10_SetKey(EftPart10 *em, s32 idx);
void EftPart10_SelectTex(EftPart10 *em, EftXTexEntry *tbl, s32 a, s32 b);
void EftPart10_BuildTex(EftPart10 *em, EftXTexSet *tex);
EftXTask *EftPart10_Create(EftPart10Arg *arg);
void EftPart10_Stop(EftXTask *task);
void EftPart10_SetFade(EftXTask *task, s32 frames);
void EftPart10_Kill(EftXTask *task);
void EftPart10_SetPos(EftXTask *task, EftXVec pos);
void EftPart10_Warp(EftXTask *task, EftXVec pos);
void EftPart10_SetDir(EftXTask *task, EftXVec dir);
void EftPart10_SetSize(EftXTask *task, f32 size);
void EftPart10_SetRate(EftXTask *task, f32 rate);
void EftPart10_SetTex(EftXTask *task, EftXTexSet *tex, s32 a, s32 b);
void EftPart10_SetDelay(EftXTask *task, s32 frames);
void EftPart10_SetHold(EftXTask *task, s32 frames);
s32 EftPart10_SetNoDepth(EftXTask *task);
s32 EftPart10_SetKind(EftXTask *task, s32 kind);
s32 EftPart10_IsAlive(EftXTask *task);
s32 EftPart10_IsCornerOffScreen(s32 x, s32 y, s32 z);

/* eft_x_b.c */
void EftLayer3_Init(EftXTask *task);
void EftLayer3_Term(EftXTask *task);
void EftLayer3_Update(EftXTask *task);
void EftLayer3_Reset(EftXTask *task);

/* eft_x_c.c */
void EftQuadMgr_Init(EftXTask *task);
void EftQuadMgr_Update(EftXTask *task);
void EftQuadMgr_Reset(EftXTask *task);
void EftQuadMgr_Term(EftXTask *task);
void EftQuad_Init(EftXTask *task, EftPart9Arg *arg);
void EftQuad_Update(EftXTask *task);
void EftQuad_PostUpdate(EftXTask *task);
void EftQuad_Draw(EftXTask *task);
void EftQuad_Reset(EftXTask *task);
void EftQuad_Term(EftXTask *task);
s32 EftQuad_InitQuad(EftPart9Ptcl *p, EftPart9 *em);

#endif
