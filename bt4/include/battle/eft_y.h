#ifndef BATTLE_EFT_Y_H
#define BATTLE_EFT_Y_H

#include "types.h"

/*
 * src/battle/eft_y.c = 0x191D28..0x195EE8 (41 functions). Two effect pack part modules, both visual only:
 *
 * 1. 0x191D28..0x195038: the rest of part kind 9, "EftQuad": an emitter task that throws textured quads from a
 *    pool of 200 shared by every emitter (gEftQuadMgr). Its task class gEftQuadClass = { update 0x190FE8, init
 *    0x190E78, term 0x191428, stub 0x191340, reset 0x191410, draw 0x191348 } and the quad initialiser 0x191498
 *    are in the file before this one; here are the per-frame quad step, the corner builder, the two draw
 *    paths, the pool allocator and list links, the key-frame animation of the emitter's parameters, and the
 *    interface the effect pack library calls (EftEmit_SpawnType9 in eft_i.c).
 *
 * 2. 0x195038..0x195EE8: helpers of part kind 16, "EftLine" (task class D_002C40F0 = { update 0x1961A0, init
 *    0x195EE8, term 0x196190, stub 0x1967C0, reset 0x1967C8, draw 0x1967E0 }, in the file after this one): one
 *    camera-facing textured strip between a point and that point + dir * length, with three-key animation of
 *    its colour, UV window, width and length.
 *
 * All structures are views local to this file; offsets come from the matching code.
 */

/* The effect code's vector: 16-byte aligned, passed by value with a callee copy (ld / sd pairs). */
typedef union EftYVec {
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftYVec;

typedef union EftYVec3 {
    struct {
        f32 x, y, z;
    };
    f32 v[3];
} EftYVec3;

typedef union EftYPair {
    struct {
        f32 a, b;
    };
    f32 v[2];
} EftYPair;

/* Four floats without the alignment, for members of blocks that do not start on a 16-byte boundary. */
typedef struct EftYVecU {
    f32 x, y, z, w;
} EftYVecU;

typedef union EftYUv {
    struct {
        f32 u, v;
    };
    f32 c[2];
} EftYUv;

/* One texture of a pack texture table: the GS TEX0 value once loaded. */
typedef struct EftYTex {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftYTex; /* size 0x10 */

/* Texture table of 8 entries (EftEmitSet.tex17 entries, 0x88 bytes): used by the quad emitter. */
typedef struct EftYTex8 {
    /* 0x00 */ EftYTex e[8];
    /* 0x80 */ s32 unk80;
    /* 0x84 */ u32 loaded; /* bit n: e[n].tex0 has been built */
} EftYTex8; /* size 0x88 */

/* Texture table of 16 entries (EftEmitSet.tex33 entries, 0x108 bytes): used by the line. */
typedef struct EftYTex16 {
    /* 0x000 */ EftYTex e[16];
    /* 0x100 */ s32 unk100;
    /* 0x104 */ u32 loaded;
} EftYTex16; /* size 0x108 */

/* The task fields used here (0x40 bytes, see eft_g.h). */
typedef struct EftYTask {
    /* 0x00 */ u8 unk0[0x34];
    /* 0x34 */ void *unk34;
    /* 0x38 */ void *work;
} EftYTask;

/* ---- quad emitter (part kind 9) ------------------------------------------------------------------------ */

/* EftQuadDef.flags */
#define EFT_QUADDEF_UV_FLIP 0x04   /* mirror U while the quad's spin about Z is negative */
#define EFT_QUADDEF_ANIM 0x10      /* the emitter's parameters are key-framed (else the last key is used) */
#define EFT_QUADDEF_FACING 0x20    /* draw path: EftQuad_DrawAllFacing, else EftQuad_DrawAllSprite */
#define EFT_QUADDEF_CROSS 0x40     /* drawn as four quarter quads; half size is halved */
#define EFT_QUADDEF_COLOR_OSC 0x80 /* colour multiplier oscillates */

/* First parameter block of a quad emitter (pack resource word 0). Every animated parameter has three keys:
   key 0 -> key 1 over time * split seconds, key 1 -> key 2 over the rest. The meaning of each parameter is
   decided by the quad initialiser 0x191498 (previous file); the names here are structural. */
typedef struct EftQuadDef {
    /* 0x000 */ EftYVec3 a[3];
    /* 0x024 */ EftYVec3 b[3];
    /* 0x048 */ EftYVec3 c[3];
    /* 0x06C */ EftYVec3 d[3];
    /* 0x090 */ EftYVec3 e[3];
    /* 0x0B4 */ EftYVec3 f[3];
    /* 0x0D8 */ EftYVec3 g[3];  /* cur.g times a constant (0x2FCD6C) is added to the emitter's angles per emitted quad */
    /* 0x0FC */ EftYVec3 h[3];  /* not animated */
    /* 0x120 */ EftYVec3 i[3];
    /* 0x144 */ EftYVec3 j[3];
    /* 0x168 */ EftYPair k[3];
    /* 0x180 */ f32 l[3];
    /* 0x18C */ f32 m[3];
    /* 0x198 */ f32 n[3];
    /* 0x1A4 */ f32 o[3];
    /* 0x1B0 */ f32 p[3];
    /* 0x1BC */ f32 q[3];
    /* 0x1C8 */ f32 r[3];
    /* 0x1D4 */ f32 s[3];
    /* 0x1E0 */ f32 t[3];
    /* 0x1EC */ f32 u[3];
    /* 0x1F8 */ f32 v[3];
    /* 0x204 */ f32 w[3];
    /* 0x210 */ f32 time;       /* seconds of the whole key animation */
    /* 0x214 */ f32 split;      /* fraction of it spent on the first leg */
    /* 0x218 */ f32 fadeInEnd;  /* a quad fades in while age / life is below this */
    /* 0x21C */ f32 fadeOutStart; /* and fades out from this fraction on */
    /* 0x220 */ f32 speedSplit; /* fraction of a quad's life after which the second speed / size step applies */
    /* 0x224 */ u8 unk224[0x10];
    /* 0x234 */ s32 flags;      /* EFT_QUADDEF_* */
    /* 0x238 */ u8 interval[3]; /* frames between emissions */
    /* 0x23B */ u8 count[3];    /* quads per emission */
    /* 0x23E */ u8 layer;       /* order table layer argument of the draw */
    /* 0x23F */ u8 cols;        /* texture sheet columns (1..4) */
    /* 0x240 */ u8 rows;        /* texture sheet rows (1..4) */
    /* 0x241 */ u8 frameStep;   /* frames of age per sheet frame */
} EftQuadDef;

/* Second parameter block (pack resource word 1), same three keys. */
typedef struct EftQuadDef2 {
    /* 0x000 */ EftYVec v0[3];
    /* 0x030 */ EftYVec v1[3];
    /* 0x060 */ EftYVec v2[3];
    /* 0x090 */ EftYVec v3[3];
    /* 0x0C0 */ f32 c0[3];
    /* 0x0CC */ f32 c1[3];
    /* 0x0D8 */ EftYPair p0[3];
    /* 0x0F0 */ EftYPair p1[3];
    /* 0x108 */ EftYPair p2[3];
    /* 0x120 */ f32 e[3];
} EftQuadDef2;

/* EftQuad.flags */
#define EFT_QUAD_USED 0x00001
#define EFT_QUAD_DEAD 0x00010       /* life over: unlinked at the end of this step */
#define EFT_QUAD_STATIC_COLOR 0x00040 /* colour is not ramped */
#define EFT_QUAD_VISIBLE 0x00200    /* corners were built this frame: draw it */
#define EFT_QUAD_SIZE_OSC 0x00800   /* size oscillates */
#define EFT_QUAD_SIZE_DOWN 0x01000  /* size oscillation runs backwards */
#define EFT_QUAD_COLOR_RAMP 0x08000 /* colour ramp is limited to [rampStart, rampEnd) of the life */
#define EFT_QUAD_OSC_R 0x10000
#define EFT_QUAD_OSC_G 0x20000
#define EFT_QUAD_OSC_B 0x40000
#define EFT_QUAD_OSC_DOWN 0x80000   /* colour oscillation runs backwards */

/* One quad of the shared pool. */
typedef struct EftQuad {
    /* 0x000 */ EftYVec pos;     /* relative to the emitter position, or to `origin` (emitter flag 0x100) */
    /* 0x010 */ EftYVec origin;  /* emitter position when the quad was created */
    /* 0x020 */ EftYVec vel;
    /* 0x030 */ EftYVec accel;
    /* 0x040 */ EftYVec color;   /* 0..255; w = alpha */
    /* 0x050 */ EftYVec3 colorRamp; /* added to colorBase, scaled by the ramp progress */
    /* 0x05C */ f32 unk5C;
    /* 0x060 */ EftYVec3 colorBase;
    /* 0x06C */ f32 alphaBase;
    /* 0x070 */ EftYVec3 rot;    /* radians, wrapped every frame */
    /* 0x07C */ f32 unk7C;
    /* 0x080 */ EftYVec corner[4]; /* view space */
    /* 0x0C0 */ EftYUv uv[4];    /* per corner: (u0, v0), (u1, v0), (u0, v1), (u1, v1) */
    /* 0x0E0 */ EftYVec3 rotVel;
    /* 0x0EC */ f32 fadeIn;      /* frames */
    /* 0x0F0 */ f32 fadeOut;     /* frames */
    /* 0x0F4 */ f32 sizeOscAmp;
    /* 0x0F8 */ EftYVec3 colorOscAmp;
    /* 0x104 */ f32 speed;
    /* 0x108 */ f32 speedMul;    /* stepped by speedStep[0 / 1], not below 0 */
    /* 0x10C */ f32 speedStep[2];
    /* 0x114 */ f32 sizeOscBase;
    /* 0x118 */ f32 sizeOscTime;
    /* 0x11C */ f32 sizeOscPeriod;
    /* 0x120 */ EftYVec3 colorMul;
    /* 0x12C */ f32 colorOscTime;
    /* 0x130 */ f32 colorOscPeriod;
    /* 0x134 */ f32 halfSize;    /* size * sizeMul * the size oscillation */
    /* 0x138 */ f32 size;
    /* 0x13C */ f32 sizeMul;     /* stepped by sizeStep[0 / 1], not below 0 */
    /* 0x140 */ f32 sizeStep[2];
    /* 0x148 */ f32 rampLen;
    /* 0x14C */ f32 rampTime;
    /* 0x150 */ f32 life;        /* frames */
    /* 0x154 */ f32 age;
    /* 0x158 */ f32 frame;       /* texture sheet frame */
    /* 0x15C */ f32 rampStart;   /* fractions of the life */
    /* 0x160 */ f32 rampEnd;
    /* 0x164 */ s32 flags;       /* EFT_QUAD_* */
    /* 0x168 */ u8 tex;          /* index into the emitter's texture table */
    /* 0x169 */ u8 unk169[3];
    /* 0x16C */ struct EftQuad *next;
    /* 0x170 */ struct EftQuad *prev;
    /* 0x174 */ u8 unk174[0xC];
} EftQuad; /* size 0x180 */

#define EFT_QUAD_MAX 200

/* *gEftQuadMgr: the pool every quad emitter shares, and the task list the emitters are created in. */
typedef struct EftQuadPool {
    /* 0x00000 */ EftQuad quad[EFT_QUAD_MAX];
    /* 0x12C00 */ s32 next;      /* where the search for a free quad starts */
    /* 0x12C04 */ void *tasks;
} EftQuadPool;

/* EftQuadWork.flags */
#define EFT_QUADEM_ALIVE 0x0001
#define EFT_QUADEM_STOPPED 0x0002   /* emits no more; dies when its last quad has gone */
#define EFT_QUADEM_FADING 0x0004    /* counting stopDelay down */
#define EFT_QUADEM_STOP_REQ 0x0008  /* stop asked while a delay or a life was still running */
#define EFT_QUADEM_DEAD 0x0010
#define EFT_QUADEM_KILL 0x0020      /* drop every quad at the next step */
#define EFT_QUADEM_VIEW_ONLY 0x0080 /* drawn only in the owner's view; also passed to the draw as "flip" */
#define EFT_QUADEM_OWN_ORIGIN 0x0100 /* quads stay where they were emitted instead of following the emitter */
#define EFT_QUADEM_SHEET 0x0400     /* the texture is a sheet of frames */
#define EFT_QUADEM_ANIM 0x2000      /* key animation running */
#define EFT_QUADEM_LEG2 0x4000      /* on the second leg of the key animation */

/* Key animation: per-leg difference of every animated parameter (EftQuad_CalcKeyDeltas). */
typedef struct EftQuadDelta {
    /* 0x178 */ EftYVec3 a;
    /* 0x184 */ EftYVec3 b;
    /* 0x190 */ EftYVec3 c;
    /* 0x19C */ EftYVec3 d;
    /* 0x1A8 */ f32 m;
    /* 0x1AC */ f32 n;
    /* 0x1B0 */ f32 o;
    /* 0x1B4 */ f32 p;
    /* 0x1B8 */ f32 q;
    /* 0x1BC */ f32 r;
    /* 0x1C0 */ f32 s;
    /* 0x1C4 */ f32 t;
    /* 0x1C8 */ f32 u;
    /* 0x1CC */ f32 v;
    /* 0x1D0 */ f32 w;
    /* 0x1D4 */ f32 interval;
    /* 0x1D8 */ f32 count;
    /* 0x1DC */ EftYVec3 e;
    /* 0x1E8 */ EftYVec3 f;
    /* 0x1F4 */ EftYVec3 g;
    /* 0x200 */ EftYVec3 i;
    /* 0x20C */ EftYVec3 j;
    /* 0x218 */ EftYPair k;
    /* 0x220 */ f32 l;
    /* 0x224 */ f32 unk224[3];
    /* 0x230 */ EftYVecU v0;
    /* 0x240 */ EftYVecU v1;
    /* 0x250 */ EftYVecU v2;
    /* 0x260 */ EftYVecU v3;
    /* 0x270 */ f32 c0;
    /* 0x274 */ f32 c1;
    /* 0x278 */ f32 p0a;
    /* 0x27C */ f32 p1a;
    /* 0x280 */ f32 p2a;
    /* 0x284 */ f32 p0b;
    /* 0x288 */ f32 p1b;
    /* 0x28C */ f32 p2b;
    /* 0x290 */ f32 e2;
} EftQuadDelta; /* size 0x11C */

/* Current value of every animated parameter (read by the quad initialiser). */
typedef struct EftQuadCur {
    /* 0x29C */ EftYVec3 a;
    /* 0x2A8 */ EftYVec3 b;
    /* 0x2B4 */ EftYVec3 c;
    /* 0x2C0 */ EftYVec3 d;
    /* 0x2CC */ f32 m;
    /* 0x2D0 */ f32 n;
    /* 0x2D4 */ f32 o;
    /* 0x2D8 */ f32 p;
    /* 0x2DC */ f32 q;
    /* 0x2E0 */ f32 r;
    /* 0x2E4 */ f32 s;
    /* 0x2E8 */ f32 t;
    /* 0x2EC */ f32 u;
    /* 0x2F0 */ f32 v;
    /* 0x2F4 */ f32 w;
    /* 0x2F8 */ EftYVec3 e;
    /* 0x304 */ EftYVec3 f;
    /* 0x310 */ EftYVec3 g;      /* added (times a constant) to the emitter's angles per emitted quad */
    /* 0x31C */ EftYVec3 h;      /* never written here */
    /* 0x328 */ EftYVec3 i;
    /* 0x334 */ EftYVec3 j;
    /* 0x340 */ EftYPair k;
    /* 0x348 */ f32 l;
    /* 0x34C */ f32 unk34C;
    /* 0x350 */ EftYVecU v0;
    /* 0x360 */ EftYVecU v1;
    /* 0x370 */ EftYVecU v2;
    /* 0x380 */ EftYVecU v3;
    /* 0x390 */ f32 c0;
    /* 0x394 */ f32 c1;
    /* 0x398 */ f32 p0a;
    /* 0x39C */ f32 p1a;
    /* 0x3A0 */ f32 p2a;
    /* 0x3A4 */ f32 p0b;
    /* 0x3A8 */ f32 p1b;
    /* 0x3AC */ f32 p2b;
    /* 0x3B0 */ f32 e2;
    /* 0x3B4 */ f32 interval;    /* frames between emissions */
    /* 0x3B8 */ f32 count;       /* quads per emission */
} EftQuadCur; /* size 0x120 */

/* Work of a quad emitter task (class gEftQuadClass). */
typedef struct EftQuadWork {
    /* 0x000 */ EftQuadDef *def;
    /* 0x004 */ EftQuadDef2 *def2;
    /* 0x008 */ EftYTex8 *tex;
    /* 0x00C */ s32 unkC;
    /* 0x010 */ EftYTex tex0;    /* copies of tex->e[texA] and tex->e[texB] */
    /* 0x020 */ EftYTex tex1;
    /* 0x030 */ EftYVec pos;
    /* 0x040 */ EftYVec dir;     /* unit */
    /* 0x050 */ EftYVec3 angle;  /* advanced by cur.g per emitted quad */
    /* 0x05C */ f32 size;
    /* 0x060 */ f32 life;        /* frames left before it stops by itself; 0 = no limit */
    /* 0x064 */ f32 age;         /* frames */
    /* 0x068 */ f32 delay;       /* frames before it starts */
    /* 0x06C */ f32 stopDelay;   /* frames between a stop request and the stop */
    /* 0x070 */ f32 fade;        /* frames of fade-out after a stop */
    /* 0x074 */ f32 fadeLeft;
    /* 0x078 */ f32 uv[16][4];   /* texture sheet frames {u0, v0, u1, v1} */
    /* 0x178 */ EftQuadDelta delta;
    /* 0x294 */ f32 animEnd;     /* frames */
    /* 0x298 */ f32 animSplit;   /* frames */
    /* 0x29C */ EftQuadCur cur;
    /* 0x3BC */ u8 frames;       /* cols * rows */
    /* 0x3BD */ u8 objId;        /* owner fighter */
    /* 0x3BE */ u8 texA;
    /* 0x3BF */ u8 texB;
    /* 0x3C0 */ u8 cut;          /* what the pack library passes as its arg3 (BtlScene_IsEffectStopped) */
    /* 0x3C1 */ u8 unk3C1[3];
    /* 0x3C4 */ s32 flags;       /* EFT_QUADEM_* */
    /* 0x3C8 */ EftQuad *head;
    /* 0x3CC */ EftQuad *tail;
} EftQuadWork; /* size 0x3D0 */

/* Argument of the emitter's init callback (EftArg9 in eft_i.c). */
typedef struct EftQuadArg {
    /* 0x00 */ EftQuadDef *def;
    /* 0x04 */ EftQuadDef2 *def2;
    /* 0x08 */ EftYTex8 *tex;
    /* 0x0C */ s32 padC;
    /* 0x10 */ EftYVec pos;
    /* 0x20 */ EftYVec dir;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 life;         /* seconds */
    /* 0x38 */ s32 objId;
    /* 0x3C */ s32 pad3C;
} EftQuadArg; /* size 0x40 */

/* One vertex as ClipVtx_Set builds it for the clipped polygon drawers of eft_a. */
typedef struct EftYClipVtx {
    /* 0x00 */ u8 unk0[0x30];
} EftYClipVtx; /* size 0x30 */

/* A screen position as the projection helpers write it. */
typedef struct EftYScr {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftYScr;

/* A colour as Vec4_ToInt converts it. */
typedef struct EftYCol {
    /* 0x0 */ s32 r;
    /* 0x4 */ s32 g;
    /* 0x8 */ s32 b;
    /* 0xC */ s32 a;
} EftYCol;

typedef struct EftYRgbaq {
    /* 0x0 */ u8 r;
    /* 0x1 */ u8 g;
    /* 0x2 */ u8 b;
    /* 0x3 */ u8 a;
    /* 0x4 */ f32 q;
} EftYRgbaq;

typedef struct EftYSt {
    /* 0x0 */ f32 s;
    /* 0x4 */ f32 t;
} EftYSt;

typedef struct EftYXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftYXyzf;

/* DMA tag + REGLIST GIF tag: PRIM, TEX0, four (RGBAQ, ST, XYZF2). */
typedef struct EftYStripPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000008 */
    /* 0x04 */ struct EftYStripPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000008 */
    /* 0x10 */ u64 gifTag;       /* 0xE400000000008001 */
    /* 0x18 */ u64 regs;         /* 0x42142142142160, or ...70 (PRIM written with the A+D register) */
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    struct {
        EftYRgbaq rgbaq;
        EftYSt st;
        EftYXyzf xyz;
    } v[4];
} EftYStripPkt; /* size 0x90 */

/* ---- line (part kind 16) ------------------------------------------------------------------------------- */

/* First parameter block of a line (pack resource word 0); the animated ones have three keys. */
typedef struct EftLineDef {
    /* 0x00 */ s32 flags;        /* 1: key-framed (else the last key is used) */
    /* 0x04 */ u8 layer;         /* order table layer; 2 and 3 are layers 0 and 1 of GS context 2 */
    /* 0x05 */ u8 unk5[7];
    /* 0x0C */ f32 unkC;         /* seconds */
    /* 0x10 */ f32 unk10;        /* seconds */
    /* 0x14 */ f32 time;         /* seconds of the whole key animation */
    /* 0x18 */ f32 split;        /* fraction of it spent on the first leg */
    /* 0x1C */ f32 spin[3];      /* half turns per second (inferred from the factors: * pi) */
    /* 0x28 */ f32 width[3];
    /* 0x34 */ f32 length[3];
    /* 0x40 */ f32 b0[3];
    /* 0x4C */ f32 b1[3];
    /* 0x58 */ f32 bTime[3];     /* seconds */
    /* 0x64 */ f32 c0[3];
    /* 0x70 */ f32 c1[3];
    /* 0x7C */ f32 cTime[3];     /* seconds */
    /* 0x88 */ f32 unk88;        /* random ranges used by the init */
    /* 0x8C */ f32 unk8C;
} EftLineDef;

/* Second parameter block (pack resource word 1), three keys each. */
typedef struct EftLineDef2 {
    /* 0x00 */ EftYVec color[3];
    /* 0x30 */ f32 u[3][2];      /* texture window: [0] at one edge, [1] at the other */
    /* 0x48 */ f32 v[3][2];
    /* 0x60 */ f32 q[3][2];
    /* 0x78 */ f32 scroll[3];    /* per second */
} EftLineDef2;

/* The animated values of a line. */
typedef struct EftLineCur {
    /* 0x00 */ EftYVec color;
    /* 0x10 */ f32 u0;
    /* 0x14 */ f32 v0;
    /* 0x18 */ f32 q0;
    /* 0x1C */ f32 du;
    /* 0x20 */ f32 dv;
    /* 0x24 */ f32 dq;
    /* 0x28 */ f32 scroll;       /* the key value times 30 */
    /* 0x2C */ f32 spin;
    /* 0x30 */ f32 width;
    /* 0x34 */ f32 length;
    /* 0x38 */ f32 b0;
    /* 0x3C */ f32 bRange;       /* b1 - b0 */
    /* 0x40 */ f32 bTime;        /* frames */
    /* 0x44 */ f32 c0;
    /* 0x48 */ f32 cRange;
    /* 0x4C */ f32 cTime;        /* frames */
} EftLineCur; /* size 0x50 */

/* Argument of the line's init callback; the work starts with a copy of it. */
typedef struct EftLineArg {
    /* 0x00 */ EftYVec pos;
    /* 0x10 */ EftYVec dir;
    /* 0x20 */ s32 objId;
    /* 0x24 */ s32 texIdx;       /* entry of `tex` that receives the built TEX0 */
    /* 0x28 */ f32 life;         /* seconds */
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ EftYTex16 *tex;
    /* 0x34 */ EftLineDef *def;
    /* 0x38 */ EftLineDef2 *def2;
    /* 0x3C */ s32 unk3C;
} EftLineArg; /* size 0x40 */

/* Work of a line task (class D_002C40F0). Only the fields this file touches are named. */
typedef struct EftLineWork {
    /* 0x000 */ EftLineArg arg;
    /* 0x040 */ EftLineCur cur;   /* EftLine_SetKey / EftLine_Animate */
    /* 0x090 */ EftYVec color;    /* drawn colour */
    /* 0x0A0 */ f32 u0;
    /* 0x0A4 */ f32 unkA4;
    /* 0x0A8 */ f32 v0;
    /* 0x0AC */ f32 unkAC;
    /* 0x0B0 */ s32 flags;        /* 1 alive, 0x20 drawn in front of everything (z forced to the nearest) */
    /* 0x0B4 */ u8 unkB4[8];
    /* 0x0BC */ f32 du;
    /* 0x0C0 */ f32 dv;
    /* 0x0C4 */ u8 unkC4[0x18];
    /* 0x0DC */ f32 time;         /* frames since the key animation started */
    /* 0x0E0 */ f32 animEnd;      /* frames */
    /* 0x0E4 */ f32 animSplit;    /* frames */
    /* 0x0E8 */ f32 unkE8[2];
    /* 0x0F0 */ f32 width;        /* half width */
    /* 0x0F4 */ f32 length;
    /* 0x0F8 */ u8 unkF8[0x20];
    /* 0x118 */ EftYTex tex0;     /* copies of tex->e[a] and tex->e[b] (EftLine_SetTex) */
    /* 0x128 */ EftYTex tex1;
    /* 0x138 */ u64 gsTex0;       /* the TEX0 value drawn with */
} EftLineWork; /* size 0x140 */

void EftQuad_StepAll(EftQuadWork *w);
void EftQuad_BuildCorners(EftQuad *q, EftQuadWork *w, EftYVec pos, f32 pitch, f32 yaw);
void EftQuad_DrawAllFacing(EftQuadWork *w, s32 layer);
void EftQuad_DrawAllSprite(EftQuadWork *w, s32 layer);
s32 EftQuad_Emit(EftQuadWork *w);
void EftQuad_ListAppend(EftQuad **head, EftQuad **tail, EftQuad *q);
void EftQuad_ListRemove(EftQuad **head, EftQuad **tail, EftQuad *q);
void EftQuad_CalcKeyDeltas(EftYTask *task);
void EftQuad_Animate(EftYTask *task);
void EftQuad_DrawSprite(EftYVec *corner, EftYVec uv0, EftYVec uv1, EftYVec color, s32 layer, s32 texIdx, s32 flip,
                        EftYTex8 *tex);
void EftQuad_DrawFacing(EftYVec *corner, EftYVec uv0, EftYVec uv1, EftYVec color, s32 layer, s32 texIdx, s32 arg6,
                        s32 flip, EftYTex8 *tex);
void EftQuad_SetSheet(EftQuadWork *w, u8 cols, u8 rows);
void EftQuad_SetTex(EftQuadWork *w, EftYTex8 *tex, s32 a, s32 b);
void EftQuad_LoadTex(EftQuadWork *w, EftYTex8 *tex);
void EftQuad_SetLastKey(EftQuadWork *w);
EftYTask *EftQuad_CreateEx(EftQuadDef *def, EftQuadDef2 *def2, EftYTex8 *tex, EftYVec pos, EftYVec dir, f32 size,
                           f32 life);
EftYTask *EftQuad_Create(EftQuadArg *arg);
void EftQuad_StopAlias(EftYTask *task);
void EftQuad_Stop(EftYTask *task);
void EftQuad_SetFade(EftYTask *task, s32 frames);
void EftQuad_SetPosDir(EftYTask *task, EftYVec pos, EftYVec dir);
void EftQuad_Kill(EftYTask *task);
void EftQuad_SetPos(EftYTask *task, EftYVec pos);
void EftQuad_Warp(EftYTask *task, EftYVec pos);
void EftQuad_SetDir(EftYTask *task, EftYVec dir);
void EftQuad_SetSize(EftYTask *task, f32 size);
void EftQuad_SetLife(EftYTask *task, f32 seconds);
void EftQuad_SetTexTable(EftYTask *task, EftYTex8 *tex);
void EftQuad_SetTexPair(EftYTask *task, EftYTex8 *tex, s32 a, s32 b);
void EftQuad_SetDelay(EftYTask *task, s32 frames);
void EftQuad_SetStopDelay(EftYTask *task, s32 frames);
s32 EftQuad_SetViewOnly(EftYTask *task);
s32 EftQuad_IsAlive(EftYTask *task);
void EftQuad_SetOwnOrigin(EftYTask *task, s32 on);
void EftQuad_SetCut(EftYTask *task, s32 cut);

void EftLine_SetTex(EftLineWork *w, EftYTex16 *tex, s32 a, s32 b);
void EftLine_LoadTex(EftLineWork *w, EftLineArg *arg);
void EftLine_SetKey(EftLineCur *out, EftLineArg *arg, s32 key);
void EftLine_Animate(EftLineWork *w);
void EftLine_DrawSprite(EftYTask *task);
void EftLine_DrawClipped(EftYTask *task);

#endif
