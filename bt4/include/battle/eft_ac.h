#ifndef BATTLE_EFT_AC_H
#define BATTLE_EFT_AC_H

#include "types.h"
#include "sys/math3d.h"

/*
 * The second part of src/battle/eft_ab_c.c (formerly eft_ac.c), 0x1A21A8..0x1A62C8. Two effect pack part modules, both visual only:
 *
 * 1. 0x1A21A8..0x1A3B00: the tail of the part kind 17 module, "ribbon": a strip of camera-facing quads through a
 *    list of nodes between two points (kind 0) or trailing behind one point (kinds 1 and 2). Its node helpers and
 *    the kind 0 draw are in the file before this one (0x1A0E58..0x1A21A8); this file has the kind 1 / 2 draws, the
 *    task callbacks, the manager and the entries EftEmit_SpawnType17 calls.
 * 2. 0x1A3B00..0x1A62C8: the head of the part kind 12 module, "zap" (named in include/battle/eft_ad.h, which has
 *    its tail): an emitter that every few frames starts lines of points turning around its axis, each drawn as a
 *    ribbon or as sprites. This file has the manager, the task callbacks, the line update and the drawing.
 *
 * Neither module creates hit records, touches fighters or battle objects, or raises events; both read the camera
 * view for drawing only. Random draws: none in the ribbon part here; the zap line init draws from the VU0
 * generator (Rand_FloatRange / Rand_IntRange) and libc rand().
 *
 * All struct names are local views; every field meaning not backed by a use in this file is a guess.
 */

/* Four floats on a 16-byte boundary (the effect code's vector type; copied with ld / sd). */
typedef struct EftAcVec {
    f32 x, y, z, w;
} __attribute__((aligned(16))) EftAcVec;

/* The part of a task (0x40 bytes) the callbacks here use (same view as include/battle/eft_g.h). */
typedef struct EftAcTask {
    /* 0x00 */ u8 flags;
    /* 0x01 */ u8 unk1[0x27];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftAcTask;

/* A vertex as ClipVtx_Set fills it and the EftGfx_DrawPoly* functions take it (EftGfxVert in eft_a.h). */
typedef struct EftAcVert {
    /* 0x00 */ EftAcVec pos;
    /* 0x10 */ EftAcVec color;
    /* 0x20 */ EftAcVec uv;
} EftAcVert; /* 0x30 */

/* One entry of a texture table (EftTexEntry in eft_g.h). */
typedef struct EftAcTex {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftAcTex; /* 0x10 */

/* A GS screen position as Vu0Cur_ProjectPoints writes it. */
typedef struct EftAcScr {
    s32 x, y, z, w;
} __attribute__((aligned(16))) EftAcScr;

/* GS registers as packed into the packet EftZap_DrawQuad queues (same views as include/battle/eft_aa.h). */
typedef struct EftAcXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftAcXyzf;

typedef struct EftAcRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftAcRgbaq;

typedef struct EftAcSt {
    f32 s, t;
} EftAcSt;

typedef struct EftAcQuadPkt {
    /* 0x00 */ u32 dmaTag;      /* 0x20000008 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;        /* 0x10000000 */
    /* 0x0C */ u32 vif1;        /* 0x50000008 */
    /* 0x10 */ u64 gifTag;      /* 0xE400000000008001: REGLIST, 14 registers */
    /* 0x18 */ u64 regs;        /* PRIM, TEX0, 4 x (RGBAQ, ST, XYZF2) */
    /* 0x20 */ u64 prim;        /* 0x5C: gouraud textured blended triangle strip; bit 9 = context 2 */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ struct {
        EftAcRgbaq rgbaq;
        EftAcSt st;
        EftAcXyzf xyz;
    } v[4];
} EftAcQuadPkt; /* 0x90 */

/* The ordering table (sys/gfx): two layers per depth. */
typedef struct EftAcOtPrim {
    u32 tag;
    void *next;
} EftAcOtPrim;

typedef struct EftAcOtEntry {
    EftAcOtPrim *head;
    EftAcOtPrim *tail;
} EftAcOtEntry;

typedef struct EftAcOtSlot {
    EftAcOtEntry layer[2];
} EftAcOtSlot;

/* ---- ribbon (part kind 17) -------------------------------------------------------------- */

/* Parameter block of a ribbon in the effect pack (EftSetPair.a of the part). */
typedef struct EftRibbonPrm {
    /* 0x00 */ s32 flags;      /* 1 animated texture, 2 random mirror, 4 scroll, 8 width pulse, 0x10 scale pulse */
    /* 0x04 */ u8 blend;       /* second argument of EftGfx_DrawPolyScaledZ */
    /* 0x05 */ u8 kind;        /* 0 strip between two points, 1 and 2 trails */
    /* 0x06 */ u8 unk6[2];
    /* 0x08 */ f32 fadeIn;     /* seconds */
    /* 0x0C */ f32 fadeOut;    /* seconds */
    /* 0x10 */ f32 animTime;   /* seconds per texture cycle */
    /* 0x14 */ f32 unk14;
} EftRibbonPrm;

/* Argument of EftRibbon_Create (EftEmitArg17 in include/battle/eft_h.h). */
typedef struct EftRibbonArg {
    /* 0x00 */ EftAcVec pos;
    /* 0x10 */ EftAcVec dir;
    /* 0x20 */ s32 type;
    /* 0x24 */ s32 chr;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ f32 life;       /* seconds; <= 0 = until stopped */
    /* 0x30 */ f32 size;
    /* 0x34 */ u8 *res;
    /* 0x38 */ EftRibbonPrm *prm;
    /* 0x3C */ s32 *texB;
} EftRibbonArg; /* 0x40 */

/* Animated values of a ribbon, filled by EftRibbon_LoadKey from the resource. */
typedef struct EftRibbonCur {
    /* 0x00 */ Vec4 scale;
    /* 0x10 */ f32 pulse[3];   /* scale pulse: base per axis */
    /* 0x1C */ f32 pulseAmp[3];
    /* 0x28 */ f32 pulseTime;
    /* 0x2C */ f32 width;
    /* 0x30 */ f32 widthBase;
    /* 0x34 */ f32 widthAmp;
    /* 0x38 */ f32 widthTime;
    /* 0x3C */ f32 alpha;
    /* 0x40 */ f32 scroll;
    /* 0x44 */ u8 unk44[0xC];
} EftRibbonCur; /* 0x50 */

/* A node of a ribbon (from the manager's 40000-byte buffer); only what this file reads. */
typedef struct EftRibbonNode {
    /* 0x00 */ s32 flags;
    /* 0x04 */ u8 tex;         /* index into EftRibbon.tex */
    /* 0x05 */ u8 unk5[0xB];
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 color;
    /* 0x30 */ u8 unk30[0x10];
    /* 0x40 */ struct EftRibbonNode *next;
} EftRibbonNode;

/* Work block of a ribbon task. */
typedef struct EftRibbon {
    /* 0x000 */ EftRibbonNode *head;
    /* 0x004 */ EftRibbonNode *tail;
    /* 0x008 */ s32 unk8[2];
    /* 0x010 */ EftRibbonArg arg;
    /* 0x050 */ EftRibbonCur cur;
    /* 0x0A0 */ Vec4 end;      /* second point */
    /* 0x0B0 */ Vec4 scaleA;
    /* 0x0C0 */ Vec4 scaleB;
    /* 0x0D0 */ s32 flags;     /* EFT_RIBBON_* */
    /* 0x0D4 */ s32 kind;
    /* 0x0D8 */ s32 unkD8;
    /* 0x0DC */ s32 maxNodes;
    /* 0x0E0 */ s32 wantNodes;
    /* 0x0E4 */ s32 numNodes;
    /* 0x0E8 */ f32 life;      /* frames left before the fade starts */
    /* 0x0EC */ f32 delay;     /* frames before the ribbon starts */
    /* 0x0F0 */ f32 fadeDelay; /* frames between stop and the fade */
    /* 0x0F4 */ f32 fadeTime;
    /* 0x0F8 */ f32 animFrame;
    /* 0x0FC */ f32 animTime;
    /* 0x100 */ f32 unk100;
    /* 0x104 */ f32 width;
    /* 0x108 */ f32 widthFrame;
    /* 0x10C */ f32 pulseFrame;
    /* 0x110 */ f32 fadeInFrame;
    /* 0x114 */ f32 fadeOutFrame;
    /* 0x118 */ f32 fadeInTime;
    /* 0x11C */ f32 fadeOutTime;
    /* 0x120 */ f32 alpha;
    /* 0x124 */ f32 scroll;
    /* 0x128 */ f32 scrollPos;
    /* 0x12C */ s32 numTex;
    /* 0x130 */ EftAcVec frame[4][2]; /* texture frames written by EftRibbon_SetTexPair */
    /* 0x1B0 */ u64 tex[6];    /* GS TEX0 by EftRibbonNode.tex */
} EftRibbon; /* 0x1E0 */

#define EFT_RIBBON_ALIVE     0x0001
#define EFT_RIBBON_STOP      0x0002 /* fading out */
#define EFT_RIBBON_DEAD      0x0004 /* the update kills the task */
#define EFT_RIBBON_VISIBLE   0x0008
#define EFT_RIBBON_NO_LIFE   0x0010 /* lives until stopped */
#define EFT_RIBBON_SCROLL    0x0020
#define EFT_RIBBON_MIRROR    0x0040
#define EFT_RIBBON_FADED_IN  0x0080
#define EFT_RIBBON_FADED_OUT 0x0100
#define EFT_RIBBON_WIDTH_DN  0x0200 /* width pulse running backwards */
#define EFT_RIBBON_PULSE_DN  0x0400 /* scale pulse running backwards */
#define EFT_RIBBON_TIMED_OUT 0x1000

/* gEftRibbonMgr */
typedef struct EftRibbonMgr {
    /* 0x000 */ void *nodes;   /* 40000 bytes */
    /* 0x004 */ u8 unk4[0x124];
} EftRibbonMgr; /* 0x128 */

/* ---- zap (part kind 12) ---------------------------------------------------------------- */
/* The module's name and its create / control entries are in include/battle/eft_ad.h (the file after this one);
   the type names here are this file's own views of the same structures. */

/* Definition of a zap emitter in the effect pack (EftEmitRes.unk0 of the part); what this file reads.
   A "key" triple is the value at the start, at `split` and at the end of a line's life. */
typedef struct EftZapDef {
    /* 0x00 */ f32 lifeMin;        /* seconds; a line lives lifeMin + rand * lifeRange */
    /* 0x04 */ f32 lifeRange;
    /* 0x08 */ f32 delayMin;       /* seconds before a new line starts to move */
    /* 0x0C */ f32 delayRange;
    /* 0x10 */ f32 fadeIn;         /* fraction of the life the alpha fades in over */
    /* 0x14 */ f32 fadeOut;        /* fraction of the life where the alpha starts to fade out */
    /* 0x18 */ f32 split;          /* fraction of the life where the first key segment ends */
    /* 0x1C */ f32 widthKey[3];
    /* 0x28 */ f32 widthKeyRange[3];
    /* 0x34 */ f32 radiusKey[3];
    /* 0x40 */ f32 radiusKeyRange[3];
    /* 0x4C */ f32 riseKey[3];     /* step along the emitter's axis per frame */
    /* 0x58 */ f32 riseKeyRange[3];
    /* 0x64 */ f32 spinKey[3];     /* angle step per frame, in units of pi */
    /* 0x70 */ f32 spinKeyRange[3];
    /* 0x7C */ f32 angle;          /* start angle, in turns */
    /* 0x80 */ f32 angleRange;
    /* 0x84 */ f32 dirOfs[2];      /* facing 1: added to the pitch and yaw of the emitter's direction, units of pi */
    /* 0x8C */ f32 offset;         /* origin = pos + dir * offset */
    /* 0x90 */ f32 tiltMin;        /* facing 1: elevation of the line's point above the turning plane, units of pi */
    /* 0x94 */ f32 tiltMax;
    /* 0x98 */ f32 keyTime;        /* seconds: length of the emitter's own key animation (flag 0x10) */
    /* 0x9C */ f32 keySplit;       /* fraction of keyTime where its first segment ends */
    /* 0xA0 */ s32 ptsMin;         /* points per line: ptsMin + rand(ptsRange) */
    /* 0xA4 */ s32 ptsRange;
    /* 0xA8 */ u8 blend;           /* layer argument of the draw calls */
    /* 0xA9 */ u8 facing;          /* 0: points on a circle, ribbon faces the camera; 1: points on a sphere, ribbon
                                      faces away from the origin */
    /* 0xAA */ u8 modeColor;       /* the mode bytes decide a line flag: 0 sets it, 2 sets it at random (rand() & 1) */
    /* 0xAB */ u8 modeRise;
    /* 0xAC */ u8 modeWidth;
    /* 0xAD */ u8 modeRadius;
    /* 0xAE */ u8 modeSpin;
    /* 0xAF */ u8 modeReverse;     /* 1 sets the flag, 2 at random */
    /* 0xB0 */ u8 interval;        /* frames between bursts */
    /* 0xB1 */ u8 count;           /* lines per burst */
    /* 0xB2 */ u8 unkB2[2];
    /* 0xB4 */ s32 flags;          /* 1 / 2 / 4 / 8: middle key = mean of the outer two (rise, width, radius, spin);
                                      0x10 keyed emitter; 0x20 tint keys; 0x40 sprites instead of a ribbon; 0x80 clipped
                                      polygons; 0x100 fade the lines out when the emitter's life ends */
} EftZapDef;

/* Key frames of the emitter's own animation (EftEmitRes.unk4): three keys per value. */
typedef struct EftZapKeyTbl {
    /* 0x000 */ EftAcVec a[3];     /* colour base */
    /* 0x030 */ EftAcVec b[3];     /* colour range */
    /* 0x060 */ EftAcVec c[3];     /* end colour base */
    /* 0x090 */ EftAcVec d[3];     /* end colour range */
    /* 0x0C0 */ f32 e[3][2];       /* tint from / to, red */
    /* 0x0D8 */ f32 f[3][2];       /* green */
    /* 0x0F0 */ f32 g[3][2];       /* blue */
    /* 0x108 */ f32 h[3];          /* tint time, seconds */
    /* 0x114 */ f32 i[3];          /* colour change start, fraction of the life */
    /* 0x120 */ f32 j[3];          /* colour change end */
} EftZapKeyTbl; /* 0x12C */

/* Argument of the create function at 0x1A6598 (EftArg12 in eft_i.c). */
typedef struct EftZapInit {
    /* 0x00 */ EftZapDef *def;
    /* 0x04 */ EftZapKeyTbl *keys;
    /* 0x08 */ EftAcTex *tex;      /* texture entries */
    /* 0x0C */ s32 padC;
    /* 0x10 */ EftAcVec dir;
    /* 0x20 */ EftAcVec pos;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 life;           /* seconds; <= 0 = until stopped */
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 objId;
    /* 0x40 */ u8 type;
    /* 0x41 */ u8 pad41[15];
} EftZapInit; /* 0x50 */

/* A point of a line (from the manager's pool). */
typedef struct EftZapPt {
    /* 0x00 */ EftAcVec pos;     /* in the line's space */
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ s32 flags;
    /* 0x24 */ struct EftZapPt *next;
    /* 0x28 */ struct EftZapPt *prev;
} EftZapPt;

/* One line: a chain of points turning around the emitter's axis. 20 in the manager, shared by all emitters. */
typedef struct EftZapStrand {
    /* 0x000 */ EftAcVec color0;
    /* 0x010 */ EftAcVec colorStep;  /* end colour - start colour */
    /* 0x020 */ EftAcVec color;      /* drawn colour; w = alpha */
    /* 0x030 */ EftAcVec axis;       /* newest point, in the emitter's space */
    /* 0x040 */ EftAcVec axisN;
    /* 0x050 */ Mtx44 mtx;           /* emitter space to world */
    /* 0x090 */ f32 width;
    /* 0x094 */ f32 spin;            /* angle step per frame */
    /* 0x098 */ f32 radius;
    /* 0x09C */ f32 angle;
    /* 0x0A0 */ f32 rise;
    /* 0x0A4 */ f32 widthStep[2];    /* per frame, first and second key segment */
    /* 0x0AC */ f32 spinStep[2];
    /* 0x0B4 */ f32 radiusStep[2];
    /* 0x0BC */ f32 riseStep[2];
    /* 0x0C4 */ f32 tint[3];         /* colour multipliers at the start of the tint animation */
    /* 0x0D0 */ f32 tintStep[3];
    /* 0x0DC */ f32 tintFrame;       /* runs up and down between 0 and tintTime */
    /* 0x0E0 */ f32 tintTime;
    /* 0x0E4 */ f32 colorFrame;
    /* 0x0E8 */ f32 colorTime;
    /* 0x0EC */ f32 colorStart;      /* fractions of the life */
    /* 0x0F0 */ f32 colorEnd;
    /* 0x0F4 */ f32 tilt;
    /* 0x0F8 */ f32 fadeInTime;      /* frames */
    /* 0x0FC */ f32 fadeOutTime;
    /* 0x100 */ f32 fadeLeft;        /* counts down once the emitter has flag EFT_ZAPF_20 */
    /* 0x104 */ f32 delay;
    /* 0x108 */ f32 life;            /* frames */
    /* 0x10C */ f32 age;
    /* 0x110 */ s32 numPts;
    /* 0x114 */ s32 flags;           /* 0 = free; 1 used, 8 dead, 0x80 visible, 0x100 colour / 0x200 rise / 0x400 width /
                                        0x800 radius / 0x1000 spin animated, 0x2000 turns backwards, 0x10000 tint
                                        animation running backwards */
    /* 0x118 */ EftZapPt *head;
    /* 0x11C */ EftZapPt *tail;
    /* 0x120 */ struct EftZapStrand *next;
    /* 0x124 */ struct EftZapStrand *prev;
    /* 0x128 */ s32 unk128[2];
} EftZapStrand; /* 0x130 */

/* A value of the emitter animated between its keys. */
typedef struct EftZapAnim2 {
    f32 cur[2];
    f32 delta[2];
} EftZapAnim2;

typedef struct EftZapAnim1 {
    f32 cur;
    f32 delta;
} EftZapAnim1;

/* Work block of a zap emitter task. */
typedef struct EftZapWork {
    /* 0x000 */ u8 unk0[0x30];
    /* 0x030 */ EftZapInit arg;
    /* 0x080 */ EftAcVec origin;
    /* 0x090 */ EftAcVec curA;   /* colour base of new lines; the cur* / d* pairs are the values in use and their
                                    difference over the current key segment (see EftZapKeyTbl) */
    /* 0x0A0 */ EftAcVec dA;
    /* 0x0B0 */ EftAcVec curB;   /* colour range */
    /* 0x0C0 */ EftAcVec dB;
    /* 0x0D0 */ EftAcVec curC;   /* end colour base */
    /* 0x0E0 */ EftAcVec dC;
    /* 0x0F0 */ EftAcVec curD;   /* end colour range */
    /* 0x100 */ EftAcVec dD;
    /* 0x110 */ EftZapAnim2 tintR;
    /* 0x120 */ EftZapAnim2 tintG;
    /* 0x130 */ EftZapAnim2 tintB;
    /* 0x140 */ EftZapAnim1 tintTime;
    /* 0x148 */ EftZapAnim1 colorStart;
    /* 0x150 */ EftZapAnim1 colorEnd;
    /* 0x158 */ f32 keyTime;     /* frames */
    /* 0x15C */ f32 keySplit;
    /* 0x160 */ f32 unk160;      /* counts down to 0; nothing in this file waits on it */
    /* 0x164 */ f32 holdTime;    /* frames */
    /* 0x168 */ f32 fadeFrame;   /* frames left of the fade */
    /* 0x16C */ f32 fadeTime;
    /* 0x170 */ f32 frame;
    /* 0x174 */ f32 life;        /* frames */
    /* 0x178 */ u8 texIdx;
    /* 0x179 */ u8 unk179[3];
    /* 0x17C */ s32 flags;       /* EFT_ZAPF_* */
    /* 0x180 */ EftZapStrand *head;
    /* 0x184 */ EftZapStrand *tail;
    /* 0x188 */ s32 unk188[2];
} EftZapWork; /* 0x190 */

#define EFT_ZAPF_ALIVE   0x00001
#define EFT_ZAPF_DEAD    0x00002 /* the update kills the task */
#define EFT_ZAPF_HOLD    0x00004 /* stop requested with a delay: holdTime counts down, then FADE or STOPPED */
#define EFT_ZAPF_STOPPED 0x00008 /* spawns no more lines; dies when the last line is gone */
#define EFT_ZAPF_FADE    0x00010 /* fadeFrame counts down; the task dies when it runs out */
#define EFT_ZAPF_20      0x00020 /* the emitter's life ran out and the definition has flag 0x100: lines fade out */
#define EFT_ZAPF_KILL    0x00040 /* the update kills the task */
#define EFT_ZAPF_KEYED   0x04000 /* the emitter's own key animation is running */
#define EFT_ZAPF_KEY2    0x08000 /* second key segment */
#define EFT_ZAPF_FRONT   0x20000
#define EFT_ZAPF_NO_LIFE 0x40000

#define EFT_ZAP_STRANDS 20

/* gEftZapMgr */
typedef struct EftZapPool {
    /* 0x0000 */ void *tasks;    /* list of 5 emitter tasks */
    /* 0x0004 */ s32 unk4[3];
    /* 0x0010 */ EftZapStrand line[EFT_ZAP_STRANDS];
    /* 0x17D0 */ u8 pts[0x690];  /* point pool: 35 of 0x30 bytes (EftZap_AddNode, eft_ad) */
    /* 0x1E60 */ Mtx44 identity;
    /* 0x1EA0 */ u8 nextLine;
    /* 0x1EA1 */ u8 unk1EA1[0xF];
} EftZapPool; /* 0x1EB0 */

#endif
