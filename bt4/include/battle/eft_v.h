#ifndef BATTLE_EFT_V_H
#define BATTLE_EFT_V_H

#include "types.h"

/*
 * src/battle/eft_u_b.c, second part (0x1853C8..0x1871A8; formerly eft_v.c), eft_v_b.c (0x1871A8..0x187C50), eft_v_c.c (0x187C50..0x1895E8).
 * Three effect modules, one per file, all of them drawing only:
 *
 * 1. 0x1853C8..0x1871A8  EftPtcl*: the tail of the particle emitter module that is effect pack part kind 5
 *    (its head, 0x182CE8..0x1853C8, is another file: allocation, stepping and creation of the particles).
 *    Here: the emission step, the four draw routines, the emitter task class (0x2C3EF0), the manager class
 *    (0x2C3ED8) and the entries the effect pack library calls (create, stop, kill, is-alive, setters).
 * 2. 0x1871A8..0x187C50  EftImpact*: the impact effect. A task (class 0x2C3F20, at most 30) that plays one of
 *    fourteen emitter sets of the common effect pack at a point: six "hit spark" sets started by the fighter
 *    effect layer (EftImpact_SpawnHit / SpawnHitScaled) and eight "explosion" sets started when a projectile
 *    hits (EftImpact_SpawnBlast, called by EftHit_SpawnBlastImpact / EftHit_SpawnTechImpact and by the sweeping
 *    beam). Manager class 0x2C3F08.
 * 3. 0x187C50..0x1895E8  EftLink*: the first half of effect pack part kind 15, a chain of sprites laid out
 *    between two points (lightning between two nodes, guess). Manager class 0x2C3F38, task class 0x2C3F50; the
 *    creation entry and the node pool are in the next file (0x1895E8..).
 *
 * Every struct here is a local view; the same objects have other names in eft_h.h / eft_i.h.
 */

/* The effect code's vector: 16-byte aligned. */
typedef union EftVVec {
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftVVec;

/* The same vector without the alignment: the type of the locals in the sprite chain module (an initialiser is
   compiled as a memset call, and EftLink_Place only matches with the union). */
typedef union EftVVecU {
    struct {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };
    f32 v[4];
} EftVVecU;

typedef struct EftVMtx {
    EftVVec row[4];
} EftVMtx; /* size 0x40 */

/* A projected point (GS 12.4 fixed point) as Vu0Cur_ProjectPoints writes it, or a colour as Vec4_ToInt writes it. */
typedef struct EftVIVec {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 w;
} __attribute__((aligned(16))) EftVIVec;

/* Effect task (0x40 bytes). */
typedef struct EftVTask {
    /* 0x00 */ u8 unk0[0x28];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftVTask;

/* A camera view: only what is read here. */
typedef struct EftVView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ EftVMtx world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ EftVVec pos;
} EftVView;

/* One entry of a texture set: the GS TEX0 value. */
typedef struct EftVTex {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftVTex; /* size 0x10 */

/* ---- GS packets ------------------------------------------------------------------------------------------- */

typedef struct EftVXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftVXyzf;

typedef struct EftVRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftVRgbaq;

typedef struct EftVSt {
    f32 s, t;
} EftVSt;

/* DMA tag + REGLIST GIF tag (14 registers): PRIM, TEX0, four (RGBAQ, ST, XYZF2): one textured strip of two
   triangles. */
typedef struct EftVStripPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000008 */
    /* 0x04 */ struct EftVStripPkt *next;
    /* 0x08 */ u32 vif0;         /* 0x10000000 */
    /* 0x0C */ u32 vif1;         /* 0x50000008 */
    /* 0x10 */ u64 gifTag;       /* 0xE400000000008001 */
    /* 0x18 */ u64 regs;         /* 0x42142142142160, or ...2170 (UV in place of TEX0, guess) */
    /* 0x20 */ u64 prim;         /* 0x5C or 0x25C */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftVRgbaq rgbaq0;
    /* 0x38 */ EftVSt st0;
    /* 0x40 */ EftVXyzf xyz0;
    /* 0x48 */ EftVRgbaq rgbaq1;
    /* 0x50 */ EftVSt st1;
    /* 0x58 */ EftVXyzf xyz1;
    /* 0x60 */ EftVRgbaq rgbaq2;
    /* 0x68 */ EftVSt st2;
    /* 0x70 */ EftVXyzf xyz2;
    /* 0x78 */ EftVRgbaq rgbaq3;
    /* 0x80 */ EftVSt st3;
    /* 0x88 */ EftVXyzf xyz3;
} EftVStripPkt; /* size 0x90 */

/* A vertex of the clipped polygon drawing (EftGfx_DrawPoly*), filled by ClipVtx_Set. */
typedef struct EftVVert {
    /* 0x00 */ EftVVec pos;
    /* 0x10 */ EftVVec uv;
    /* 0x20 */ EftVVec color;
} EftVVert; /* size 0x30 */

/* ---- 1. particle emitter (effect pack part kind 5) -------------------------------------------------------- */

/* The emitter's definition in the effect pack (EftEmitArgA.texA in eft_h.h). Three keys of every animated value
   live in the part not mapped here. */
typedef struct EftPtclDef {
    /* 0x000 */ s32 flags;     /* 1: the values are key-framed, 2: clipped polygon drawing (kind 0), 4: the texture
                                  is a cols x rows sheet, 0x80: the quad starts at the particle (kind 0) */
    /* 0x004 */ u8 unk4;
    /* 0x005 */ u8 shape;      /* 0 quad along the particle's axis facing the camera, 1 square sprite, 2 sprite */
    /* 0x006 */ u8 flip;       /* 0: flag 0x200, 1: flag 0x200 at random, 2: never */
    /* 0x007 */ u8 centred;    /* kind 0: the quad extends both ways along the axis */
    /* 0x008 */ u8 unk8[2];
    /* 0x00A */ u8 layer;      /* ordering table layer; 2 and 3 select the second packet form */
    /* 0x00B */ u8 unkB[3];
    /* 0x00E */ u8 cols;
    /* 0x00F */ u8 rows;
    /* 0x010 */ f32 animTime;  /* seconds per sheet frame */
    /* 0x014 */ f32 keyTime;   /* seconds the key animation lasts */
    /* 0x018 */ f32 keySplit;  /* fraction of keyTime at which the second key is reached */
    /* 0x01C */ u8 unk1C[0x264];
    /* 0x280 */ f32 angX, angXRange;   /* half turns: base, random range */
    /* 0x288 */ f32 spinX, spinXRange;
    /* 0x290 */ f32 angY, angYRange;
    /* 0x298 */ f32 spinY, spinYRange;
    /* 0x2A0 */ f32 angZ, angZRange;
    /* 0x2A8 */ f32 spinZ, spinZRange;
} EftPtclDef;

/* Argument block of EftPtcl_Create (EftEmitArgA in eft_h.h). Copied with 64-bit moves. */
typedef struct EftPtclArg {
    /* 0x00 */ EftVVec pos;
    /* 0x10 */ EftVVec dir;
    /* 0x20 */ s32 objId;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ f32 life;       /* seconds; <= 0: runs until stopped */
    /* 0x2C */ f32 size;
    /* 0x30 */ void *res;
    /* 0x34 */ EftPtclDef *def;
    /* 0x38 */ void *unk38;
    /* 0x3C */ s32 pad3C;
} EftPtclArg; /* size 0x40 */

/* The emitter's current (key-interpolated) values, written by EftPtcl_SetKey / EftPtcl_BlendKeys. */
typedef struct EftPtclCur {
    /* 0x00 */ u8 unk0[0x5C];
    /* 0x5C */ f32 count;      /* particles per emission */
    /* 0x60 */ f32 countRange; /* random addition */
    /* 0x64 */ f32 interval;   /* frames between emissions */
    /* 0x68 */ u8 unk68[0x90];
    /* 0xF8 */ f32 roll;
    /* 0xFC */ f32 rollRange;
    /* 0x100 */ u8 unk100[0x30];
} EftPtclCur; /* size 0x130 */

/* One particle (0x130 bytes, 500 in the pool). */
typedef struct EftPtcl {
    /* 0x000 */ EftVVec unk0;
    /* 0x010 */ EftVVec unk10;
    /* 0x020 */ EftVVec unk20;
    /* 0x030 */ EftVVec pos;
    /* 0x040 */ EftVVec axis;
    /* 0x050 */ u8 unk50[0x10];
    /* 0x060 */ f32 u0;
    /* 0x064 */ f32 u1;
    /* 0x068 */ f32 v0;
    /* 0x06C */ f32 v1;
    /* 0x070 */ EftVVec color;
    /* 0x080 */ u8 unk80[0x20];
    /* 0x0A0 */ s32 flags;     /* 0x100: free of the emitter, 0x400: drawn */
    /* 0x0A4 */ u8 unkA4[0x34];
    /* 0x0D8 */ f32 width;
    /* 0x0DC */ u8 unkDC[0xC];
    /* 0x0E8 */ f32 height;
    /* 0x0EC */ u8 unkEC[0xC];
    /* 0x0F8 */ f32 rot;
    /* 0x0FC */ u8 unkFC[0x20];
    /* 0x11C */ struct EftPtcl *next;
    /* 0x120 */ u8 unk120[0x10];
} EftPtcl; /* size 0x130 */

#define EFT_PTCL_MAX 500

/* EftPtclWork.flags */
#define EFT_PTCL_ALIVE 0x001
#define EFT_PTCL_STOPPING 0x002  /* no more emission once stopDelay has run out */
#define EFT_PTCL_DEAD 0x004      /* the task ends on its next update */
#define EFT_PTCL_ENDLESS 0x008   /* no life time */
#define EFT_PTCL_FRONT 0x010     /* drawn in front (depth forced), and only when its fighter is in view */
#define EFT_PTCL_SHEET 0x020     /* animated sheet texture */
#define EFT_PTCL_FLAG40 0x040
#define EFT_PTCL_FLAG80 0x080
#define EFT_PTCL_LINGER 0x100    /* stopping: emit for `linger` more frames */
#define EFT_PTCL_FLIP 0x200

/* Work block of an emitter task (class gEftPtclClass). */
typedef struct EftPtclWork {
    /* 0x000 */ EftPtclArg arg;
    /* 0x040 */ EftPtclCur cur;
    /* 0x170 */ EftVMtx mtx;       /* pitch, then yaw, of arg.dir */
    /* 0x1B0 */ EftVVec ang;
    /* 0x1C0 */ EftVVec spin;
    /* 0x1D0 */ EftPtcl *head;
    /* 0x1D4 */ EftPtcl *tail;
    /* 0x1D8 */ s32 flags;
    /* 0x1DC */ s16 count;         /* live particles */
    /* 0x1DE */ u8 type;           /* effect pack type, for the stop / hide tests */
    /* 0x1DF */ u8 unk1DF;
    /* 0x1E0 */ s32 frames;        /* sheet frames */
    /* 0x1E4 */ f32 du;
    /* 0x1E8 */ f32 dv;
    /* 0x1EC */ f32 unk1EC;
    /* 0x1F0 */ f32 animFrames;
    /* 0x1F4 */ f32 life;          /* frames left */
    /* 0x1F8 */ f32 emitTimer;
    /* 0x1FC */ f32 startDelay;
    /* 0x200 */ f32 stopDelay;
    /* 0x204 */ f32 linger;
    /* 0x208 */ f32 lingerInit;
    /* 0x20C */ f32 keyTime;
    /* 0x210 */ f32 keyDur;
    /* 0x214 */ f32 keySplit;
    /* 0x218 */ f32 unk218;
    /* 0x21C */ f32 pitch;
    /* 0x220 */ f32 yaw;
    /* 0x224 */ f32 roll;
    /* 0x228 */ u8 unk228[0x20];
    /* 0x248 */ u64 tex0;
} EftPtclWork; /* size 0x250 */

/* gEftPtcl (0x50 bytes). */
typedef struct EftPtclMgr {
    /* 0x00 */ EftVMtx identity;
    /* 0x40 */ EftPtcl *pool;      /* EFT_PTCL_MAX particles */
    /* 0x44 */ s32 unk44;
    /* 0x48 */ s32 used;
    /* 0x4C */ s32 unk4C;
} EftPtclMgr;

/* ---- 2. impact effect ------------------------------------------------------------------------------------- */

/* Views of the effect pack library's objects (EftEmitSet / EftEmitState / EftEmitNodes in eft_i.h). */
typedef struct EftVGroupDef {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 count;
} EftVGroupDef;

typedef struct EftVGroup {
    /* 0x00 */ EftVGroupDef *def;
    /* 0x04 */ u8 unk4[0xC];
} EftVGroup; /* size 0x10 */

typedef struct EftVSet {
    /* 0x000 */ u32 *mask;         /* header; its first word has a bit per part kind */
    /* 0x004 */ s32 unk4[2];
    /* 0x00C */ EftVGroup grp[19];
    /* 0x13C */ u8 unk13C[0x1E4];
} EftVSet; /* size 0x320 */

typedef struct EftVState {
    u8 unk0[0x2CC];
} EftVState;

typedef struct EftVNodes {
    u8 unk0[0x250];
} EftVNodes;

/* Argument of the three spawn entries (FxPosArg, BtlMemberHitFxReq, EftImpactArg elsewhere). */
typedef struct EftImpactArg {
    /* 0x00 */ EftVVec pos;
    /* 0x10 */ EftVVec dir;
    /* 0x20 */ s32 id;         /* 0..16 for a hit, 0..12 for a blast */
    /* 0x24 */ s32 objId;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} EftImpactArg; /* size 0x30 */

/* What the spawn entries hand to the task's init. */
typedef struct EftImpactInit {
    /* 0x00 */ s32 group;      /* 0 hit spark, 1 blast impact */
    /* 0x04 */ f32 scale;      /* group 0: <= 0 selects the fighter's scale */
    /* 0x08 */ f32 rise;       /* group 1: the effect moves up by this much per frame */
    /* 0x0C */ s32 padC;
    /* 0x10 */ EftImpactArg arg;
} EftImpactInit; /* size 0x40 */

/* EftImpactWork.flags */
#define EFT_IMPACT_ENDING 0x01   /* no part is alive any more */
#define EFT_IMPACT_DEAD 0x02
#define EFT_IMPACT_KILL 0x04
#define EFT_IMPACT_KILLED 0x08   /* reset: the parts were killed */
#define EFT_IMPACT_STARTED 0x10  /* the first frame is over */

/* Work block of an impact task (class gEftImpactClass). */
typedef struct EftImpactWork {
    /* 0x000 */ EftVVec dir;
    /* 0x010 */ EftVVec pos;
    /* 0x020 */ EftVSet *set;
    /* 0x024 */ EftVState state;
    /* 0x2F0 */ EftVNodes nodes;
    /* 0x540 */ s32 flags;
    /* 0x544 */ u8 mask;           /* node slots / phases used on the first frame */
    /* 0x545 */ u8 group;
    /* 0x546 */ u8 id;
    /* 0x547 */ u8 objId;
    /* 0x548 */ f32 scale;
    /* 0x54C */ f32 rise;
    /* 0x550 */ f32 time;          /* frames since the last part died */
    /* 0x554 */ f32 endFrames;
    /* 0x558 */ u8 unk558[8];
} EftImpactWork; /* size 0x560 */

#define EFT_IMPACT_HIT_SETS 6
#define EFT_IMPACT_BLAST_SETS 8

/* gEftImpact (0x2BC8 bytes). */
typedef struct EftImpactMgr {
    /* 0x0000 */ s32 count[2];     /* live tasks of each group */
    /* 0x0008 */ EftVSet hit[EFT_IMPACT_HIT_SETS];
    /* 0x12C8 */ EftVSet blast[EFT_IMPACT_BLAST_SETS];
} EftImpactMgr;

/* ---- 3. sprite chain (effect pack part kind 15) ----------------------------------------------------------- */

typedef struct EftLinkPair {
    f32 a, b;
} EftLinkPair;

/* First definition block of a chain: three keys of every animated value, then the fixed parameters. */
typedef struct EftLinkDef {
    /* 0x000 */ f32 rotX[3];       /* three keys of every animated value */
    /* 0x00C */ f32 rotXRange[3];
    /* 0x018 */ f32 rotZ[3];
    /* 0x024 */ f32 rotZRange[3];
    /* 0x030 */ f32 dirAng[3];
    /* 0x03C */ f32 dirAngRange[3];
    /* 0x048 */ f32 spin[3];
    /* 0x054 */ f32 spinRange[3];
    /* 0x060 */ f32 twist[3];      /* twist per node (turns) */
    /* 0x06C */ f32 ofsX[3];
    /* 0x078 */ f32 ofsY[3];
    /* 0x084 */ f32 unk84[3];      /* unk84 / unk90: jitter along the chain */
    /* 0x090 */ f32 unk90[3];
    /* 0x09C */ f32 dist[3];
    /* 0x0A8 */ f32 distRange[3];
    /* 0x0B4 */ f32 distBase[3];
    /* 0x0C0 */ f32 size[3][3];    /* [key][axis] */
    /* 0x0E4 */ f32 sizeRange[3][3];
    /* 0x108 */ f32 life[3];
    /* 0x114 */ f32 lifeRange[3];
    /* 0x120 */ f32 wait[3];
    /* 0x12C */ f32 waitRange[3];
    /* 0x138 */ f32 pulse[3][2];   /* [key][0 / 1] */
    /* 0x150 */ f32 pulseTime[3];
    /* 0x15C */ f32 keyTime;       /* seconds */
    /* 0x160 */ f32 keySplit;      /* fraction of keyTime at which key 1 is reached */
    /* 0x164 */ u8 unk164[0xC];
    /* 0x170 */ f32 growSplit;
    /* 0x174 */ f32 growTime;      /* seconds */
    /* 0x178 */ f32 spacing[3];    /* distance between two sprites */
    /* 0x184 */ f32 scale[3];
    /* 0x190 */ f32 scaleRange[3];
    /* 0x19C */ u8 cols;
    /* 0x19D */ u8 rows;
    /* 0x19E */ u8 unk19E;
    /* 0x19F */ u8 layer;
    /* 0x1A0 */ s32 flags;         /* 2, 4, 0x10, 0x200 grows, 0x400 key-framed, 0x2000 */
} EftLinkDef;

/* Second definition block: three keys of the vector values. */
typedef struct EftLinkDef2 {
    /* 0x000 */ EftVVec col0[3];
    /* 0x030 */ EftVVec col0Range[3];
    /* 0x060 */ EftVVec col1[3];
    /* 0x090 */ EftVVec col1Range[3];
    /* 0x0C0 */ f32 mulR[3][2];    /* [key][0 / 1] */
    /* 0x0D8 */ f32 mulG[3][2];
    /* 0x0F0 */ f32 mulB[3][2];
    /* 0x108 */ f32 mulTime[3];
    /* 0x114 */ f32 fade0[3];
    /* 0x120 */ f32 fade1[3];
} EftLinkDef2;

/* Argument block of the creation entry 0x18BB08 (EftArg15 in eft_i.c). */
typedef struct EftLinkArg {
    /* 0x00 */ EftLinkDef *def;
    /* 0x04 */ EftLinkDef2 *def2;
    /* 0x08 */ EftVTex *tex;
    /* 0x0C */ s32 padC;
    /* 0x10 */ EftVVec pos;
    /* 0x20 */ EftVVec pos2;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 life;           /* seconds; <= 0 runs until stopped */
    /* 0x38 */ s32 frame;          /* texture of the set */
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s32 objId;
    /* 0x44 */ s32 pad44[3];
} EftLinkArg; /* size 0x50 */

/* One sprite of a chain (0x150 bytes, 200 in the pool). */
typedef struct EftLinkNode {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ EftVVec unk40;
    /* 0x050 */ EftVVec offset;
    /* 0x060 */ EftVVec pos;
    /* 0x070 */ u8 unk70[0x10];
    /* 0x080 */ EftVVec color;
    /* 0x090 */ u8 unk90[0x20];
    /* 0x0B0 */ EftVVec uv0;
    /* 0x0C0 */ EftVVec uv1;
    /* 0x0D0 */ f32 unkD0;
    /* 0x0D4 */ f32 rot;
    /* 0x0D8 */ f32 twist;
    /* 0x0DC */ f32 unkDC;
    /* 0x0E0 */ f32 size;
    /* 0x0E4 */ u8 unkE4[0x18];
    /* 0x0FC */ f32 unkFC;
    /* 0x100 */ u8 unk100[0x40];
    /* 0x140 */ s32 flags;         /* 1 placed, 0x40 drawn, 0x400 still on the chain this frame */
    /* 0x144 */ struct EftLinkNode *next;
    /* 0x148 */ u8 unk148[8];
} EftLinkNode; /* size 0x150 */

/* A value with its per-unit change between two keys. */
typedef struct EftLinkAnim {
    f32 cur, delta;
} EftLinkAnim;

/* EftLinkWork.flags */
#define EFT_LINK_ALIVE 0x0001
#define EFT_LINK_DEAD 0x0002
#define EFT_LINK_STOPPED 0x0004  /* no new sprites; dies when the last one is gone */
#define EFT_LINK_STOPPING 0x0008
#define EFT_LINK_KILLED 0x0010
#define EFT_LINK_FADING 0x0020
#define EFT_LINK_SHEET 0x0100
#define EFT_LINK_FLAG200 0x0200
#define EFT_LINK_GROWING 0x2000
#define EFT_LINK_KEYED 0x4000
#define EFT_LINK_KEY2 0x8000     /* the second key interval has started */

/* Work block of a chain task (class gEftLinkClass). */
typedef struct EftLinkWork {
    /* 0x000 */ u8 unk0[0x30];
    /* 0x030 */ EftLinkArg arg;
    /* 0x080 */ EftVVec vec[4][2];     /* [n][0] current, [n][1] change between the two keys */
    /* 0x100 */ EftLinkPair pair[3][2];
    /* 0x130 */ EftLinkAnim val4[3];
    /* 0x148 */ EftLinkAnim val[16];
    /* 0x1C8 */ f32 vecA[2][3];
    /* 0x1E0 */ f32 vecB[2][3];
    /* 0x1F8 */ EftLinkAnim val2[4];
    /* 0x218 */ EftLinkPair pair2[2];
    /* 0x228 */ EftLinkAnim val3;
    /* 0x230 */ f32 keyDur;
    /* 0x234 */ f32 keySplit;
    /* 0x238 */ u8 unk238[8];
    /* 0x240 */ EftVVec dir;           /* unit vector from pos to pos2 */
    /* 0x250 */ f32 time;
    /* 0x254 */ f32 frames;
    /* 0x258 */ f32 delay;
    /* 0x25C */ f32 stopDelay;
    /* 0x260 */ f32 fade;
    /* 0x264 */ f32 fadeMax;
    /* 0x268 */ f32 uv[16][4];         /* u0, v0, u1, v1 of each sheet frame */
    /* 0x368 */ f32 growTime;
    /* 0x36C */ f32 growDur;
    /* 0x370 */ f32 scale;
    /* 0x374 */ f32 scaleStep[2];
    /* 0x37C */ f32 spacing;
    /* 0x380 */ f32 spacingStep[2];
    /* 0x388 */ f32 pitch;
    /* 0x38C */ f32 yaw;
    /* 0x390 */ s32 type;
    /* 0x394 */ s32 unk394;
    /* 0x398 */ s32 flags;
    /* 0x39C */ EftLinkNode *head;
    /* 0x3A0 */ EftLinkNode *tail;
    /* 0x3A4 */ u8 unk3A4[0xC];
} EftLinkWork; /* size 0x3B0 */

#define EFT_LINK_NODES 200

/* gEftLink (0x106E0 bytes). */
typedef struct EftLinkMgr {
    /* 0x00000 */ void *list;          /* task list, 6 chains */
    /* 0x00004 */ u8 unk4[0x1C];
    /* 0x00020 */ EftLinkNode node[EFT_LINK_NODES];
    /* 0x106A0 */ EftVMtx identity;
} EftLinkMgr;

void EftPtcl_Emit(EftPtclWork *w, EftPtclWork *w2);
void EftPtcl_DrawSquares(EftPtclWork *w, EftPtclWork *w2);
void EftPtcl_DrawSprites(EftPtclWork *w, EftPtclWork *w2);
void EftPtcl_DrawAxisQuads(EftPtclWork *w, EftPtclWork *w2);
void EftPtcl_DrawAxisPolys(EftPtclWork *w, EftPtclWork *w2);
void EftPtcl_Init(EftVTask *task, EftPtclArg *arg);
void EftPtcl_Term(EftVTask *task);
void EftPtcl_Update(EftVTask *task);
void EftPtcl_PostUpdate(EftVTask *task);
void EftPtcl_Reset(EftVTask *task);
void EftPtcl_Draw(EftVTask *task);
void EftPtclMgr_Init(EftVTask *task);
void EftPtclMgr_Term(EftVTask *task);
void EftPtclMgr_Update(EftVTask *task);
void EftPtclMgr_Reset(EftVTask *task);
EftVTask *EftPtcl_Create(EftPtclArg *arg);
s32 EftPtcl_Stop(EftVTask *task);
s32 EftPtcl_Kill(EftVTask *task);
s32 EftPtcl_IsAlive(EftVTask *task);
s32 EftPtcl_SetPos(EftVTask *task, EftVVec *pos);
s32 EftPtcl_Warp(EftVTask *task, EftVVec *pos);
s32 EftPtcl_SetDir(EftVTask *task, EftVVec *dir);
s32 EftPtcl_SetSize(EftVTask *task, f32 size);
s32 EftPtcl_SetStartDelay(EftVTask *task, f32 frames);
s32 EftPtcl_SetStopDelay(EftVTask *task, f32 frames);
s32 EftPtcl_SetLinger(EftVTask *task, f32 frames);
s32 EftPtcl_SetTexture(EftVTask *task, void *res, s32 a, s32 b);
s32 EftPtcl_SetFront(EftVTask *task);
s32 EftPtcl_SetFlag40(EftVTask *task, s32 on);
s32 EftPtcl_SetFlag80(EftVTask *task, s32 on);
s32 EftPtcl_SetType(EftVTask *task, s32 type);

void EftImpact_SpawnParts(s32 objId, EftVTask *task, EftVSet *set);
EftVSet *EftImpact_GetSet(s32 group, u32 id);
u8 EftImpact_GetNodeMask(s32 group, s32 id);
void EftImpact_Init(EftVTask *task, EftImpactInit *init);
void EftImpact_Term(EftVTask *task);
void EftImpact_Update(EftVTask *task);
void EftImpact_PostUpdate(EftVTask *task);
void EftImpact_Reset(EftVTask *task);
void EftImpact_Draw(EftVTask *task);
void EftImpactMgr_Init(EftVTask *task);
void EftImpactMgr_Term(EftVTask *task);
void EftImpactMgr_Update(EftVTask *task);
void EftImpactMgr_Reset(EftVTask *task);
s32 EftImpact_SpawnHit(EftImpactArg *arg);
s32 EftImpact_SpawnHitScaled(EftImpactArg *arg, f32 scale);
s32 EftImpact_SpawnBlast(EftImpactArg *arg, f32 scale, f32 rise);

void EftLinkMgr_Init(EftVTask *task);
void EftLinkMgr_Update(EftVTask *task);
void EftLinkMgr_Reset(EftVTask *task);
void EftLinkMgr_Term(EftVTask *task);
void EftLink_Init(EftVTask *task, EftLinkArg *arg);
void EftLink_Update(EftVTask *task);
void EftLink_PostUpdate(EftVTask *task);
void EftLink_Draw(EftVTask *task);
void EftLink_Reset(EftVTask *task);
void EftLink_Term(EftVTask *task);
void EftLink_Place(EftLinkWork *w);
void EftLink_InitGrow(EftLinkWork *w);
void EftLink_UpdateGrow(EftLinkWork *w);
void EftLink_InitKeys(EftLinkWork *w);
void EftLink_UpdateKeys(EftLinkWork *w);

#endif
