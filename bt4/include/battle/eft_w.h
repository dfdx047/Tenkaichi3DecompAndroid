#ifndef BATTLE_EFT_W_H
#define BATTLE_EFT_W_H

#include "types.h"
#include "sys/math3d.h"

/*
 * src/battle/eft_w.c, 0x1895E8..0x18D618: two sprite particle modules of the effect pack library (EftEmit_*,
 * include/battle/eft_i.h). Both are presentation only.
 *
 * 1. 0x1895E8..0x18C190  EftLink_*, second half of effect pack part kind 15 (first half: eft_v_c.c / eft_v.h).
 *    An emitter lays sprites out between two points (pos, pos2). This file has its sprite pool (200 sprites
 *    of 0x150 bytes in the manager block gEftLink, shared by all emitters), the sprite life cycle, the four
 *    draw routines and the handle API called by EftEmit_SpawnType15.
 * 2. 0x18C190..0x18D618  EftPart10*, first half of effect pack part kind 10 (second half: eft_x.c / eft_x.h).
 *    An emitter keeps rings ("groups") of sprite particles around a point. This file has the manager class
 *    (0x2C3F68: 12 emitter tasks of 0x3D0 bytes, 150 rings, 150 particles), the emitter class callbacks
 *    (0x2C3F80), ring emission and the particle step.
 *
 * All structs are this file's views of objects eft_v.h / eft_x.h also describe; offsets are from the matching
 * code, names from how this file uses them. Where the views disagree the notes say which code verifies what.
 *
 * Emitter flags (both modules, +0x398 / +0x3C0): 1 alive, 2 dead, 4 stopped (no new sprites), 8 stop delay
 * running, 0x10 fading / killed, 0x20 kill now (kind 10), 0x40 set at init (kind 10), 0x100 sprite sheet,
 * 0x400 bit 10: passed to the draw routines as "no depth", 0x1000 stop requested (kind 15), 0x40000 key
 * animation running (kind 10).
 * Sprite flags (+0x140): 1 in use, 0x40 visible this frame, 0x80 colour ramp (kind 10) / mirrored (kind 15),
 * 0x200 mirrored UVs (kind 10), 0x400 keep one more frame (kind 15), 0x800 colour ramp (kind 15), 0x1000 /
 * 0x10000 / 0x20000 direction bits of the ping-pong timers.
 */

/* 16-byte aligned vector, passed by value with a callee copy. */
typedef union EftWVec {
    struct {
        f32 x, y, z, w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftWVec;

typedef struct EftWIVec {
    s32 x, y, z, w;
} __attribute__((aligned(16))) EftWIVec;

/* Task view (0x40 bytes). */
typedef struct EftWTask {
    /* 0x00 */ u8 flags;
    /* 0x01 */ u8 unk1[0x27];
    /* 0x28 */ void **cls;
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftWTask;

typedef struct EftWTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftWTexEntry;

typedef struct EftWTexSet {
    /* 0x000 */ EftWTexEntry e[16];
    /* 0x100 */ s32 count;
    /* 0x104 */ u32 built;
} EftWTexSet; /* 0x108 */

/* Definition of a kind 15 emitter (pack data); only what this file reads. */
typedef struct EftWLinkDef {
    /* 0x000 */ f32 rotX[3];      /* three keys of every animated value */
    /* 0x00C */ f32 rotXRange[3];
    /* 0x018 */ f32 rotZ[3];
    /* 0x024 */ f32 rotZRange[3];
    /* 0x030 */ f32 dirAng[3];
    /* 0x03C */ f32 dirAngRange[3];
    /* 0x048 */ f32 spin[3];
    /* 0x054 */ f32 spinRange[3];
    /* 0x060 */ f32 twist[3];
    /* 0x06C */ f32 ofsX[3];
    /* 0x078 */ f32 ofsY[3];
    /* 0x084 */ f32 unk84[3];
    /* 0x090 */ f32 unk90[3];
    /* 0x09C */ f32 dist[3];
    /* 0x0A8 */ f32 distRange[3];
    /* 0x0B4 */ f32 distBase[3];
    /* 0x0C0 */ f32 size[3][3];
    /* 0x0E4 */ f32 sizeRange[3][3];
    /* 0x108 */ f32 life[3];
    /* 0x114 */ f32 lifeRange[3];
    /* 0x120 */ f32 wait[3];
    /* 0x12C */ f32 waitRange[3];
    /* 0x138 */ f32 pulse[3][2];
    /* 0x150 */ f32 pulseTime[3];
    /* 0x15C */ u8 unk15C[8];
    /* 0x164 */ f32 fadeIn;       /* fraction of the life */
    /* 0x168 */ f32 fadeOut;
    /* 0x16C */ f32 sizeSplit;
    /* 0x170 */ u8 unk170[0x2E];
    /* 0x19E */ u8 texStep;
    /* 0x19F */ u8 unk19F;
    /* 0x1A0 */ s32 flags;
} EftWLinkDef;

/* Second key block of an emitter. */
typedef struct EftWLinkKey {
    /* 0x000 */ Vec4 col0[3];
    /* 0x030 */ Vec4 col0Range[3];
    /* 0x060 */ Vec4 col1[3];
    /* 0x090 */ Vec4 col1Range[3];
    /* 0x0C0 */ f32 mulR[3][2];
    /* 0x0D8 */ f32 mulG[3][2];
    /* 0x0F0 */ f32 mulB[3][2];
    /* 0x108 */ f32 mulTime[3];
    /* 0x114 */ f32 fade0[3];
    /* 0x120 */ f32 fade1[3];
} EftWLinkKey;

/* One particle (0x150 bytes). */
typedef struct EftWLinkNode {
    /* 0x000 */ Vec4 corner[4];
    /* 0x040 */ Vec4 ofs;
    /* 0x050 */ Vec4 dir;
    /* 0x060 */ Vec4 origin;
    /* 0x070 */ Vec4 unk70;
    /* 0x080 */ Vec4 col;
    /* 0x090 */ Vec4 col0;
    /* 0x0A0 */ Vec4 colDelta;
    /* 0x0B0 */ Vec4 uv0;
    /* 0x0C0 */ Vec4 uv1;
    /* 0x0D0 */ f32 rotX;
    /* 0x0D4 */ f32 rotZ;
    /* 0x0D8 */ f32 twist;
    /* 0x0DC */ f32 spin;
    /* 0x0E0 */ f32 size;
    /* 0x0E4 */ f32 sizeVel0;
    /* 0x0E8 */ f32 sizeVel1;
    /* 0x0EC */ f32 fadeIn;
    /* 0x0F0 */ f32 fadeOut;
    /* 0x0F4 */ f32 age;
    /* 0x0F8 */ f32 life;
    /* 0x0FC */ f32 scale;
    /* 0x100 */ f32 pulseT;
    /* 0x104 */ f32 pulseTime;
    /* 0x108 */ f32 pulse0;
    /* 0x10C */ f32 pulseD;
    /* 0x110 */ f32 mulTime;
    /* 0x114 */ f32 mulT;
    /* 0x118 */ f32 mulD[3];
    /* 0x124 */ f32 mul0[3];
    /* 0x130 */ f32 fadeT;
    /* 0x134 */ f32 fadeTime;
    /* 0x138 */ f32 texFrame;
    /* 0x13C */ f32 delay;
    /* 0x140 */ s32 flags;
    /* 0x144 */ struct EftWLinkNode *next;
    /* 0x148 */ struct EftWLinkNode *prev;
    /* 0x14C */ s32 pad14C;
} EftWLinkNode; /* 0x150 */

typedef struct EftWLinkArg {
    /* 0x00 */ EftWLinkDef *def;
    /* 0x04 */ EftWLinkKey *key;
    /* 0x08 */ EftWTexSet *tex;
    /* 0x0C */ s32 padC;
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 pos2;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 rate;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s32 objId;
    /* 0x44 */ s32 pad44[3];
} EftWLinkArg; /* 0x50 */

typedef struct EftWLink {
    /* 0x000 */ u8 unk0[8];
    /* 0x008 */ EftWTexEntry tex;
    /* 0x018 */ EftWTexEntry clut;
    /* 0x028 */ u8 unk28[8];
    /* 0x030 */ EftWLinkArg arg;
    /* 0x080 */ Vec4 col0;
    /* 0x090 */ Vec4 unk90;
    /* 0x0A0 */ Vec4 col0Range;
    /* 0x0B0 */ Vec4 unkB0;
    /* 0x0C0 */ Vec4 col1;
    /* 0x0D0 */ Vec4 unkD0;
    /* 0x0E0 */ Vec4 col1Range;
    /* 0x0F0 */ Vec4 unkF0;
    /* 0x100 */ f32 mulR[2];
    /* 0x108 */ f32 unk108[2];
    /* 0x110 */ f32 mulG[2];
    /* 0x118 */ f32 unk118[2];
    /* 0x120 */ f32 mulB[2];
    /* 0x128 */ f32 unk128[2];
    /* 0x130 */ f32 mulTime, unk134;
    /* 0x138 */ f32 fade0, unk13C;
    /* 0x140 */ f32 fade1, unk144;
    /* 0x148 */ f32 rotX, unk14C;
    /* 0x150 */ f32 rotXRange, unk154;
    /* 0x158 */ f32 rotZ, unk15C;
    /* 0x160 */ f32 rotZRange, unk164;
    /* 0x168 */ f32 dirAng, unk16C;
    /* 0x170 */ f32 dirAngRange, unk174;
    /* 0x178 */ f32 spin, unk17C;
    /* 0x180 */ f32 spinRange, unk184;
    /* 0x188 */ f32 twist, unk18C;
    /* 0x190 */ f32 ofsX, unk194;
    /* 0x198 */ f32 ofsY, unk19C;
    /* 0x1A0 */ f32 unk1A0, unk1A4;
    /* 0x1A8 */ f32 unk1A8, unk1AC;
    /* 0x1B0 */ f32 dist, unk1B4;
    /* 0x1B8 */ f32 distRange, unk1BC;
    /* 0x1C0 */ f32 distBase, unk1C4;
    /* 0x1C8 */ f32 size[3];
    /* 0x1D4 */ f32 unk1D4[3];
    /* 0x1E0 */ f32 sizeRange[3];
    /* 0x1EC */ f32 unk1EC[3];
    /* 0x1F8 */ f32 life, unk1FC;
    /* 0x200 */ f32 lifeRange, unk204;
    /* 0x208 */ f32 wait, unk20C;
    /* 0x210 */ f32 waitRange, unk214;
    /* 0x218 */ f32 pulse[2];
    /* 0x220 */ f32 unk220[2];
    /* 0x228 */ f32 pulseTime;
    /* 0x22C */ u8 unk22C[0x24];
    /* 0x250 */ f32 frame;
    /* 0x254 */ f32 texFrames;
    /* 0x258 */ f32 delay;         /* seconds (EftLink_SetDelay stores frames / 30) */
    /* 0x25C */ f32 stopDelay;     /* frames a stop request waits */
    /* 0x260 */ f32 fade;          /* fade-out frames left */
    /* 0x264 */ f32 fadeMax;
    /* 0x268 */ f32 uv[16][4];
    /* 0x368 */ u8 unk368[8];
    /* 0x370 */ f32 scale;
    /* 0x374 */ u8 unk374[0x14];
    /* 0x388 */ f32 pitch;         /* the chain's direction as two angles (set by eft_v_c.c) */
    /* 0x38C */ f32 yaw;
    /* 0x390 */ s32 type;
    /* 0x394 */ u8 texIdx;
    /* 0x398 */ s32 flags;
    /* 0x39C */ EftWLinkNode *head;
    /* 0x3A0 */ EftWLinkNode *tail;
} EftWLink;

#define EFT_WLINK_NODES 200

typedef struct EftWLinkMgr {
    /* 0x00000 */ void *list;
    /* 0x00004 */ s32 unk4[3];
    /* 0x00010 */ EftWLinkNode ptcl[EFT_WLINK_NODES];
    /* 0x10690 */ s32 next;
    /* 0x10694 */ s32 unk10694[3];
    /* 0x106A0 */ Mtx44 camMtx;
} EftWLinkMgr;

/* ---- part kind 10 ---- */

typedef struct EftPart10Def {
    /* 0x000 */ u8 unk0[0x180];
    /* 0x180 */ f32 twistStep;
    /* 0x184 */ u8 unk184[0x48];
    /* 0x1CC */ f32 count;
    /* 0x1D0 */ u8 unk1D0[0x18];
    /* 0x1E8 */ f32 fadeIn;
    /* 0x1EC */ f32 fadeOut;
    /* 0x1F0 */ f32 sizeSplit;
    /* 0x1F4 */ s32 flags;
    /* 0x1F8 */ u8 cols;
    /* 0x1F9 */ u8 rows;
    /* 0x1FA */ u8 texStep;
    /* 0x1FB */ u8 blend;
    /* 0x1FC */ f32 ringFrom;
    /* 0x200 */ f32 ringTo;
    /* 0x204 */ f32 yaw;
    /* 0x208 */ f32 pitch;
    /* 0x20C */ u8 mode;
} EftPart10Def;

typedef struct EftPart10Arg {
    /* 0x00 */ EftPart10Def *def;
    /* 0x04 */ void *key;
    /* 0x08 */ EftWTexSet *tex;
    /* 0x0C */ s32 padC;
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 dir;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 life;
    /* 0x38 */ s32 texSel;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s32 objId;
    /* 0x44 */ s32 pad44[3];
} __attribute__((aligned(8))) EftPart10Arg; /* 0x50 */

typedef struct EftPart10Ptcl {
    /* 0x000 */ Vec4 pos;
    /* 0x010 */ Vec4 base;
    /* 0x020 */ Vec4 unk20;
    /* 0x030 */ Vec4 col0;
    /* 0x040 */ Vec4 colDelta;
    /* 0x050 */ Vec4 col;
    /* 0x060 */ Vec4 corner[4];
    /* 0x0A0 */ Vec4 uv0;
    /* 0x0B0 */ Vec4 uv1;
    /* 0x0C0 */ f32 dist;
    /* 0x0C4 */ f32 distVel;
    /* 0x0C8 */ f32 ang[3];
    /* 0x0D4 */ f32 angVel[3];
    /* 0x0E0 */ f32 size;
    /* 0x0E4 */ f32 sizeVel0;
    /* 0x0E8 */ f32 sizeVel1;
    /* 0x0EC */ f32 fadeIn;
    /* 0x0F0 */ f32 fadeOut;
    /* 0x0F4 */ f32 scale;
    /* 0x0F8 */ f32 pulse0;
    /* 0x0FC */ f32 pulseT;
    /* 0x100 */ f32 pulseTime;
    /* 0x104 */ f32 mulD[3];
    /* 0x110 */ f32 mul0[3];
    /* 0x11C */ f32 mulT;
    /* 0x120 */ f32 mulTime;
    /* 0x124 */ f32 fadeT;
    /* 0x128 */ f32 fadeTime;
    /* 0x12C */ f32 pulseD;
    /* 0x130 */ f32 texFrame;
    /* 0x134 */ f32 delay;
    /* 0x138 */ f32 life;
    /* 0x13C */ f32 age;
    /* 0x140 */ s32 flags;
    /* 0x144 */ struct EftPart10Ptcl *next;
    /* 0x148 */ struct EftPart10Ptcl *prev;
    /* 0x14C */ s32 pad14C;
} EftPart10Ptcl; /* 0x150 */

typedef struct EftPart10Grp {
    /* 0x00 */ s32 flags;
    /* 0x04 */ EftPart10Ptcl *head;
    /* 0x08 */ EftPart10Ptcl *tail;
    /* 0x0C */ struct EftPart10Grp *next;
    /* 0x10 */ struct EftPart10Grp *prev;
} EftPart10Grp; /* 0x14 */

typedef struct EftPart10 {
    /* 0x000 */ u8 unk0[0x30];
    /* 0x030 */ EftPart10Arg arg;
    /* 0x080 */ u8 unk80[0x70];
    /* 0x0F0 */ f32 angA, unkF4;
    /* 0x0F8 */ f32 angARange, unkFC;
    /* 0x100 */ f32 angB, unk104;
    /* 0x108 */ f32 angBRange, unk10C;
    /* 0x110 */ u8 unk110[0x70];
    /* 0x180 */ f32 unk180;
    /* 0x184 */ u8 unk184[0xC4];
    /* 0x248 */ f32 fade0, unk24C;
    /* 0x250 */ f32 fade1, unk254;
    /* 0x258 */ f32 unk258;
    /* 0x25C */ f32 stopDelay;
    /* 0x260 */ f32 angA0;
    /* 0x264 */ f32 angB0;
    /* 0x268 */ f32 scale;
    /* 0x26C */ f32 count;
    /* 0x270 */ u8 unk270[0x20];
    /* 0x290 */ f32 twist;
    /* 0x294 */ f32 yaw;
    /* 0x298 */ f32 pitch;
    /* 0x29C */ f32 uv[16][4];
    /* 0x39C */ f32 texFrames;
    /* 0x3A0 */ f32 frame;
    /* 0x3A4 */ f32 delay;
    /* 0x3A8 */ f32 timer;
    /* 0x3AC */ f32 fade;
    /* 0x3B0 */ f32 fadeTime;
    /* 0x3B4 */ u8 unk3B4[8];
    /* 0x3BC */ u8 texIdx;
    /* 0x3BD */ u8 kind;
    /* 0x3C0 */ s32 flags;
    /* 0x3C4 */ EftPart10Grp *grpHead;
    /* 0x3C8 */ EftPart10Grp *grpTail;
    /* 0x3CC */ s32 pad3CC;
} __attribute__((aligned(8))) EftPart10; /* 0x3D0 */

#define EFT_PART10_GRPS 150

typedef struct EftPart10Mgr {
    /* 0x0000 */ void *list;
    /* 0x0004 */ EftPart10Grp grp[EFT_PART10_GRPS];
    /* 0x0BBC */ s32 padBBC;
    /* 0x0BC0 */ EftPart10Ptcl ptcl[EFT_PART10_GRPS]; /* particle pool (used by eft_x.c) */
    /* 0xD0A0 */ s32 grpNext;      /* where the search for a free ring starts */
    /* 0xD0A4 */ s32 ptclNext;
    /* 0xD0A8 */ s32 padD0A8[2];
    /* 0xD0B0 */ Mtx44 camMtx;
} EftPart10Mgr; /* 0xD0F0 */

/* The part of the camera view (gBtlCamView) the draw code reads. */
typedef struct EftWView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ Mtx44 unk40;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 unk140;
    /* 0x180 */ u8 unk180[0xB4];
    /* 0x234 */ f32 screenDist;
} EftWView;

#endif
