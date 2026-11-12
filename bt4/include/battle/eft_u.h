#ifndef BATTLE_EFT_U_H
#define BATTLE_EFT_U_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect code, 0x180BF8..0x1853C8. Two modules, both drawing only:
 *
 *   0x180BF8..0x182CE8  src/battle/eft_t_c.c, second part (formerly eft_u.c): vanish lines, the rest of the module that starts at 0x1809C0 in
 *                       its first part (EftShotFx_FindFree, EftShotFx_Start, EftShotFxMgr_Init). Task class D_002C3EC0 =
 *                       {update, init, term, post-update, 0, draw}. The functions keep eft_t's `EftShotFx` prefix so
 *                       the module has one name; what it draws is the teleport ("vanish") effect: fighter effect
 *                       requests 0xC (vanish), 0xD (reappear), 0xE / 0xF (the short forms of both) and object
 *                       animation event bit 42 start it through EftShotFx_Start(objId, kind 0..3).
 *   0x182CE8..0x1853C8  src/battle/eft_u_b.c: the head of the particle emitter module `EftPtcl` (effect pack part
 *                       kind 5) whose task code is in the second part of eft_u_b.c (formerly eft_v.c): texture choice, key values, the particle pool,
 *                       creation and the per-frame step of the particles.
 *
 * All structs are this file's views (eft_t.h and eft_v.h have shorter views of the same objects).
 */

/* A vector passed by value: 16-byte aligned (argument copies use ld / sd), unlike Vec4 in sys/math3d.h. */
typedef union EftUVec {
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftUVec;

typedef struct EftUMtx {
    f32 m[4][4];
} __attribute__((aligned(16))) EftUMtx;

/* A projected vertex in GS coordinates (12.4 fixed). */
typedef struct EftUIVec {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftUIVec;

/* ---- vanish lines (module EftShotFx) -------------------------------------------------------------------- */

/* One vertical black line (a camera-facing quad). 0x60 bytes, 200 in the pool. */
typedef struct EftShotFxLine {
    /* 0x00 */ Vec4 pos;          /* anchor on the body */
    /* 0x10 */ Vec4 off;          /* offset in camera space (x right, y down), rotated by the work's matrix */
    /* 0x20 */ f32 toward;        /* distance the line is pulled towards the camera */
    /* 0x24 */ f32 len;           /* length, along world up */
    /* 0x28 */ f32 growth;        /* length change per frame (flag 0x40 adds, 0x80 subtracts) */
    /* 0x2C */ f32 accel;         /* growth += growth * accel each frame */
    /* 0x30 */ f32 width;
    /* 0x34 */ f32 alpha;
    /* 0x38 */ f32 alphaStep;
    /* 0x3C */ f32 alphaEnd;      /* flag 0x10: alpha rises to it; flag 0x20: 0, alpha falls to it */
    /* 0x40 */ f32 life;          /* frames left */
    /* 0x44 */ u8 tex;            /* index into the work's textures */
    /* 0x45 */ u8 blend;          /* ordering table layer */
    /* 0x46 */ u8 unk46[2];
    /* 0x48 */ s32 flags;         /* EFT_SHOTFX_LINE_* ; 0 = free */
    /* 0x4C */ s32 delay;         /* frames before a waiting line starts */
    /* 0x50 */ struct EftShotFxLine *prev;
    /* 0x54 */ struct EftShotFxLine *next;
    /* 0x58 */ u8 unk58[8];
} EftShotFxLine; /* size 0x60 */

#define EFT_SHOTFX_LINE_ACTIVE 0x02   /* stepped and drawn */
#define EFT_SHOTFX_LINE_WAIT 0x04     /* counts `delay` down, then becomes active */
#define EFT_SHOTFX_LINE_DONE 0x08     /* life ran out */
#define EFT_SHOTFX_LINE_FADE_IN 0x10
#define EFT_SHOTFX_LINE_FADE_OUT 0x20
#define EFT_SHOTFX_LINE_GROW 0x40
#define EFT_SHOTFX_LINE_SHRINK 0x80

/* One request (two slots; a slot is free when flags == 0). EftShotFx_Start sets: kind 0 (vanish) = A | BODY_A,
   kind 1 (reappear) = B | APPEAR_COLUMN, kind 2 = A | BODY_A | BURST, kind 3 = B | APPEAR_COLUMN | BURST,
   kind 4 = clear. */
typedef struct EftShotFxReq {
    /* 0x0 */ s32 objId;
    /* 0x4 */ s32 frameA;         /* frames since a kind 0 / 2 request */
    /* 0x8 */ s32 frameB;         /* frames since a kind 1 / 3 request */
    /* 0xC */ s32 flags;          /* EFT_SHOTFX_REQ_* */
} EftShotFxReq;

#define EFT_SHOTFX_REQ_A 0x001        /* kind 0 / 2 sequence running (frameA 0..4) */
#define EFT_SHOTFX_REQ_B 0x002        /* kind 1 / 3 sequence running (frameB 0..4) */
#define EFT_SHOTFX_REQ_HOLD 0x004     /* skipped while set (set by nothing in this range) */
#define EFT_SHOTFX_REQ_VANISH_COLUMN 0x008
#define EFT_SHOTFX_REQ_APPEAR_COLUMN 0x010
#define EFT_SHOTFX_REQ_BODY_A 0x020   /* lines on the body every frame (kind 0: frames 0 and 1) */
#define EFT_SHOTFX_REQ_BODY_B 0x040   /* same for kind 1 (frames 2, 3 and 4) */
#define EFT_SHOTFX_REQ_BURST 0x080    /* kinds 2 / 3: four staggered sets of body lines once, no column */
#define EFT_SHOTFX_REQ_BURST_DONE 0x100

typedef struct EftShotFxTex {
    /* 0x0 */ u64 tex0;
    /* 0x8 */ u64 unk8;
} EftShotFxTex;

/* gEftShotFx: 0x60 bytes from the effect pool. */
typedef struct EftShotFxWork {
    /* 0x00 */ EftShotFxReq *req;         /* 2 */
    /* 0x04 */ EftShotFxLine *lines;      /* 200 */
    /* 0x08 */ s32 count;                 /* lines in the list */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ Mtx44 camRot;              /* camera-to-world rotation of the view being drawn */
    /* 0x50 */ EftShotFxTex *tex;         /* 0x48 bytes, filled by EftTexSet_Load4 from common entry 0x237 */
    /* 0x54 */ EftShotFxLine *head;
    /* 0x58 */ EftShotFxLine *tail;
    /* 0x5C */ s32 unk5C;
} EftShotFxWork; /* size 0x60 */

/* A body segment between two model nodes (gEftShotFxSegs at 0x2ECEC0, 23 rows of 0x18 bytes). The
   spawner stops at the first row whose node is 4, which is row 20: the last three rows (nodes 4, 5, 6) are unused. */
typedef struct EftShotFxSeg {
    /* 0x00 */ s32 node;
    /* 0x04 */ s32 nodeEnd;       /* -1 = a single point */
    /* 0x08 */ f32 spacing;       /* distance between rings along the segment */
    /* 0x0C */ f32 lenBase;       /* line length = (rand + lenBase) * height * 0.05 * 2 */
    /* 0x10 */ f32 radiusBase;    /* ring radius = (rand * 0.5 + radiusBase) * height * 0.05 * 1.5 */
    /* 0x14 */ s32 count;         /* lines per ring */
} EftShotFxSeg;

/* A slice of the column around the fighter (gEftShotFxRows at 0x2ED0E8, 6 rows of 0x18 bytes). */
typedef struct EftShotFxRow {
    /* 0x00 */ f32 from;          /* position along the column, 0 = top, 1 = bottom */
    /* 0x04 */ f32 to;            /* < 0 = a single ring */
    /* 0x08 */ f32 spacing;
    /* 0x0C */ f32 lenBase;
    /* 0x10 */ f32 radiusBase;
    /* 0x14 */ s32 count;
} EftShotFxRow;

void EftShotFx_Term(void);
void EftShotFx_Update(void);
void EftShotFx_PostUpdate(void);
void EftShotFx_Draw(void);
EftShotFxLine *EftShotFx_Alloc(void);
void EftShotFx_Add(EftUVec pos, EftUVec off, f32 toward, f32 len, f32 growth, f32 accelDiv, f32 width, f32 alpha,
                     f32 life, u8 tex, u8 blend, s32 flags, s32 delay);
void EftShotFx_UpdateLines(void);
s32 EftShotFx_Step(EftShotFxLine *p);
void EftShotFx_DrawLines(void);
void EftShotFx_UpdateReqs(void);
void EftShotFx_UpdateTexture(s32 a, s32 b);
void EftShotFx_BuildQuad(Vec4 *out, EftShotFxLine *p);
void EftShotFx_DrawQuad(Vec4 *quad, EftUMtx mtx, u8 r, u8 g, u8 b, u8 a, u64 *tex, u8 blend);
void EftShotFx_SpawnBodyLines(s32 objId, f32 life, s32 delay);
void EftShotFx_SpawnVanishColumn(s32 objId, f32 life);
void EftShotFx_SpawnAppearColumn(s32 objId, f32 life);
s32 EftShotFx_IsOnScreen(EftUIVec *v, s32 margin);

/* ---- particle emitter (effect pack part kind 5; module EftPtcl, continued in the second part of eft_u_b.c) ------------------- */

/* Second definition block (arg.def2): three keys of the vector values. */
typedef struct EftUPtclDef2 {
    /* 0x00 */ Vec4 color[3];           /* [key] start colour of a particle, 0..255 */
    /* 0x30 */ Vec4 colorRange[3];      /* random addition */
    /* 0x60 */ Vec4 colorEnd[3];        /* end colour */
    /* 0x90 */ Vec4 colorEndRange[3];
    /* 0xC0 */ f32 pulseR[3][2];        /* [key] {low, high} factor of the colour pulse, red */
    /* 0xD8 */ f32 pulseG[3][2];
    /* 0xF0 */ f32 pulseB[3][2];
    /* 0x108 */ f32 pulseTime[3];       /* seconds of half a pulse */
} EftUPtclDef2;

/* First definition block (arg.def). Every animated value has three keys. */
typedef struct EftUPtclDef {
    /* 0x000 */ s32 flags;      /* 2: sprites are not scaled by 16; 8: width / height pulse; 0x10: random pulse
                                   phase; 0x20: colour pulse; 0x40: random spin direction; 0x100: fixed axis */
    /* 0x004 */ u8 reverse;     /* 1: particles start at the end of their path and fly it backwards */
    /* 0x005 */ u8 shape;       /* 0 quad along the axis, 1 square sprite, 2 sprite */
    /* 0x006 */ u8 flip;
    /* 0x007 */ u8 centred;
    /* 0x008 */ u8 originKind;  /* 0, 1, 2: how the start offset is turned */
    /* 0x009 */ u8 accelKind;   /* 0, 1, 2: what the drift pulls away from */
    /* 0x00A */ u8 layer;
    /* 0x00B */ u8 unkB[3];
    /* 0x00E */ u8 cols;
    /* 0x00F */ u8 rows;
    /* 0x010 */ f32 animTime;
    /* 0x014 */ f32 keyTime;
    /* 0x018 */ f32 keySplit;
    /* 0x01C */ s16 count[3];           /* particles per emission */
    /* 0x022 */ s16 countRange[3];
    /* 0x028 */ f32 interval[3];        /* seconds */
    /* 0x034 */ f32 life[3];            /* seconds */
    /* 0x040 */ f32 lifeRange[3];
    /* 0x04C */ f32 split[3];           /* fraction of the life at which a particle reaches its middle stage */
    /* 0x058 */ f32 speed[3][3];        /* [key][stage] */
    /* 0x07C */ f32 speedRange[3][3];
    /* 0x0A0 */ f32 width[3][3];
    /* 0x0C4 */ f32 widthRange[3][3];
    /* 0x0E8 */ f32 height[3][3];
    /* 0x10C */ f32 heightRange[3][3];
    /* 0x130 */ f32 widthPulseLo[3];
    /* 0x13C */ f32 widthPulseHi[3];
    /* 0x148 */ f32 widthPulseTime[3];  /* seconds */
    /* 0x154 */ f32 heightPulseLo[3];
    /* 0x160 */ f32 heightPulseHi[3];
    /* 0x16C */ f32 heightPulseTime[3];
    /* 0x178 */ f32 drift[3];           /* length of the drift vector */
    /* 0x184 */ f32 gravity[3];         /* added to the drift's y */
    /* 0x190 */ f32 radius[3];          /* start offset: (radius + rand * radiusRange) * rand + ring + rand * ringRange */
    /* 0x19C */ f32 radiusRange[3];
    /* 0x1A8 */ f32 ring[3];
    /* 0x1B4 */ f32 ringRange[3];
    /* 0x1C0 */ f32 along[3];           /* originKind 2: random shift along the emitter direction */
    /* 0x1CC */ f32 coneA[3];           /* half turns */
    /* 0x1D8 */ f32 coneB[3];
    /* 0x1E4 */ f32 roll[3];            /* half turns added to the emitter's roll per particle */
    /* 0x1F0 */ f32 rollRange[3];
    /* 0x1FC */ f32 startDist[3];
    /* 0x208 */ f32 startDistRange[3];
    /* 0x214 */ f32 flatten[3];         /* axis.x *= 1 + flatten before normalising */
    /* 0x220 */ f32 fadeIn[3];          /* seconds */
    /* 0x22C */ f32 fadeOut[3];
    /* 0x238 */ f32 rot[3];             /* half turns */
    /* 0x244 */ f32 rotRange[3];
    /* 0x250 */ f32 spin[3];            /* half turns per frame, first stage */
    /* 0x25C */ f32 spinRange[3];
    /* 0x268 */ f32 spin2[3];           /* second stage */
    /* 0x274 */ f32 spin2Range[3];
    /* 0x280 */ f32 unk280[12];         /* emitter angles, read in the second part of eft_u_b.c */
} EftUPtclDef;

/* A texture set entry and the set (arg.res): TEX0 values cached per frame, one bit per entry. */
typedef struct EftUPtclTex {
    /* 0x0 */ u64 tex0;
    /* 0x8 */ u64 unk8;
} EftUPtclTex;

typedef struct EftUPtclRes {
    /* 0x000 */ EftUPtclTex tex[16];
    /* 0x100 */ s32 unk100;
    /* 0x104 */ u32 uploaded;   /* bit n: tex[n].tex0 is valid (cleared elsewhere) */
} EftUPtclRes;

/* Argument block of the emitter (start of its work). */
typedef struct EftUPtclArg {
    /* 0x00 */ EftUVec pos;
    /* 0x10 */ EftUVec dir;
    /* 0x20 */ s32 objId;
    /* 0x24 */ s32 texIdx;
    /* 0x28 */ f32 life;
    /* 0x2C */ f32 size;
    /* 0x30 */ EftUPtclRes *res;
    /* 0x34 */ EftUPtclDef *def;
    /* 0x38 */ EftUPtclDef2 *def2;
    /* 0x3C */ s32 pad3C;
} EftUPtclArg; /* size 0x40 */

/* The emitter's current values: one key, or two keys blended. Times are frames, angles radians. */
typedef struct EftUPtclCur {
    /* 0x000 */ EftUVec color;
    /* 0x010 */ EftUVec colorRange;
    /* 0x020 */ EftUVec colorEnd;
    /* 0x030 */ EftUVec colorEndRange;
    /* 0x040 */ f32 pulseLo[3];
    /* 0x04C */ f32 pulseRange[3];
    /* 0x058 */ f32 pulseTime;
    /* 0x05C */ f32 count;
    /* 0x060 */ f32 countRange;
    /* 0x064 */ f32 interval;
    /* 0x068 */ f32 life;
    /* 0x06C */ f32 lifeRange;
    /* 0x070 */ f32 split;
    /* 0x074 */ f32 speed[3];
    /* 0x080 */ f32 speedRange[3];
    /* 0x08C */ f32 width[3];
    /* 0x098 */ f32 widthRange[3];
    /* 0x0A4 */ f32 height[3];
    /* 0x0B0 */ f32 heightRange[3];
    /* 0x0BC */ f32 widthPulseLo;
    /* 0x0C0 */ f32 widthPulseRange;
    /* 0x0C4 */ f32 widthPulseTime;
    /* 0x0C8 */ f32 heightPulseLo;
    /* 0x0CC */ f32 heightPulseRange;
    /* 0x0D0 */ f32 heightPulseTime;
    /* 0x0D4 */ f32 drift;
    /* 0x0D8 */ f32 gravity;
    /* 0x0DC */ f32 radius;
    /* 0x0E0 */ f32 radiusRange;
    /* 0x0E4 */ f32 ring;
    /* 0x0E8 */ f32 ringRange;
    /* 0x0EC */ f32 along;
    /* 0x0F0 */ f32 coneA;
    /* 0x0F4 */ f32 coneB;
    /* 0x0F8 */ f32 roll;
    /* 0x0FC */ f32 rollRange;
    /* 0x100 */ f32 startDist;
    /* 0x104 */ f32 startDistRange;
    /* 0x108 */ f32 flatten;
    /* 0x10C */ f32 fadeIn;
    /* 0x110 */ f32 fadeOut;
    /* 0x114 */ f32 rot;
    /* 0x118 */ f32 rotRange;
    /* 0x11C */ f32 spin;
    /* 0x120 */ f32 spinRange;
    /* 0x124 */ f32 spin2;
    /* 0x128 */ f32 spin2Range;
    /* 0x12C */ f32 pad12C;
} EftUPtclCur; /* size 0x130 */

/* One particle (0x130 bytes, 500 in the pool). */
typedef struct EftUPtcl {
    /* 0x000 */ EftUVec travel;     /* distance covered along its (curving) path */
    /* 0x010 */ EftUVec origin;     /* start offset from the emitter */
    /* 0x020 */ EftUVec rel;        /* travel + origin; once detached, the emitter position at that moment */
    /* 0x030 */ EftUVec pos;        /* world position */
    /* 0x040 */ EftUVec axis;       /* unit direction of travel */
    /* 0x050 */ EftUVec accel;      /* added to the axis every frame */
    /* 0x060 */ f32 u0;
    /* 0x064 */ f32 u1;
    /* 0x068 */ f32 v0;
    /* 0x06C */ f32 v1;
    /* 0x070 */ EftUVec color;
    /* 0x080 */ EftUVec colorStart;
    /* 0x090 */ EftUVec colorDelta;
    /* 0x0A0 */ s32 flags;          /* EFT_UPTCL_* ; 0 = free */
    /* 0x0A4 */ f32 animT;          /* frames into the sheet animation */
    /* 0x0A8 */ f32 age;            /* frames */
    /* 0x0AC */ f32 life;
    /* 0x0B0 */ f32 split;          /* age at which the second stage starts */
    /* 0x0B4 */ f32 fadeOutAt;      /* age at which the fade out starts */
    /* 0x0B8 */ f32 fadeInT;
    /* 0x0BC */ f32 fadeOutT;
    /* 0x0C0 */ f32 fadeIn;         /* frames */
    /* 0x0C4 */ f32 fadeOut;
    /* 0x0C8 */ f32 speed;
    /* 0x0CC */ f32 speedKey[3];    /* at birth, at the split, at death */
    /* 0x0D8 */ f32 width;
    /* 0x0DC */ f32 widthKey[3];
    /* 0x0E8 */ f32 height;
    /* 0x0EC */ f32 heightKey[3];
    /* 0x0F8 */ f32 rot;
    /* 0x0FC */ f32 spin;
    /* 0x100 */ f32 widthPulseT;
    /* 0x104 */ f32 widthPulseRange;
    /* 0x108 */ f32 widthPulseTime;
    /* 0x10C */ f32 heightPulseT;
    /* 0x110 */ f32 heightPulseRange;
    /* 0x114 */ f32 heightPulseTime;
    /* 0x118 */ f32 colorPulseT;
    /* 0x11C */ struct EftUPtcl *next;
    /* 0x120 */ struct EftUPtcl *prev;
    /* 0x124 */ s32 unk124[3];
} EftUPtcl; /* size 0x130 */

#define EFT_UPTCL_ALIVE 0x001
#define EFT_UPTCL_FADED_IN 0x002
#define EFT_UPTCL_FADED_OUT 0x004
#define EFT_UPTCL_STAGE2 0x008
#define EFT_UPTCL_WIDTH_DOWN 0x010   /* width pulse running backwards */
#define EFT_UPTCL_HEIGHT_DOWN 0x020
#define EFT_UPTCL_COLOR_DOWN 0x040
#define EFT_UPTCL_REVERSE_SPIN 0x080
#define EFT_UPTCL_DETACHED 0x100     /* no longer follows the emitter */
#define EFT_UPTCL_VISIBLE 0x400

/* Work block of an emitter task (EftPtclWork in eft_v.h). */
typedef struct EftUPtclWork {
    /* 0x000 */ EftUPtclArg arg;
    /* 0x040 */ EftUPtclCur cur;
    /* 0x170 */ EftUMtx mtx;
    /* 0x1B0 */ EftUVec ang;
    /* 0x1C0 */ EftUVec spin;
    /* 0x1D0 */ EftUPtcl *head;
    /* 0x1D4 */ EftUPtcl *tail;
    /* 0x1D8 */ s32 flags;          /* 0x20 sheet, 0x40 detach the particles, 0x200 colour animated */
    /* 0x1DC */ s16 count;
    /* 0x1DE */ u8 type;
    /* 0x1DF */ u8 unk1DF;
    /* 0x1E0 */ s32 frames;
    /* 0x1E4 */ f32 du;
    /* 0x1E8 */ f32 dv;
    /* 0x1EC */ f32 unk1EC;
    /* 0x1F0 */ f32 animFrames;
    /* 0x1F4 */ f32 life;
    /* 0x1F8 */ f32 emitTimer;
    /* 0x1FC */ f32 startDelay;
    /* 0x200 */ f32 stopDelay;
    /* 0x204 */ f32 linger;
    /* 0x208 */ f32 lingerInit;
    /* 0x20C */ f32 keyTime;        /* frames into the key animation */
    /* 0x210 */ f32 keyDur;
    /* 0x214 */ f32 keySplit;       /* frame at which key 1 is reached */
    /* 0x218 */ f32 unk218;
    /* 0x21C */ f32 pitch;
    /* 0x220 */ f32 yaw;
    /* 0x224 */ f32 roll;
    /* 0x228 */ EftUPtclTex texA;   /* image entry */
    /* 0x238 */ EftUPtclTex texB;   /* palette entry */
    /* 0x248 */ u64 tex0;
} EftUPtclWork; /* size 0x250 */

/* gEftPtcl (0x50 bytes). */
typedef struct EftUPtclMgr {
    /* 0x00 */ EftUMtx identity;
    /* 0x40 */ EftUPtcl *pool;      /* 500 */
    /* 0x44 */ s32 next;            /* where the search for a free particle starts */
    /* 0x48 */ s32 used;
    /* 0x4C */ s32 unk4C;
} EftUPtclMgr;

void EftPtcl_PickTexture(EftUPtclWork *w, EftUPtclTex *tex, s32 a, s32 b);
void EftPtcl_UploadTexture(EftUPtclWork *w, EftUPtclWork *w2);
void EftPtcl_SetKey(EftUPtclCur *cur, EftUPtclWork *w, s32 key);
void EftPtcl_BlendKeys(EftUPtclWork *w);
EftUPtcl *EftPtcl_AllocPtcl(EftUPtclWork *w);
void EftPtcl_UnlinkPtcl(EftUPtclWork *w, EftUPtcl *p);
void EftPtcl_FreePtcls(EftUPtclWork *w);
s32 EftPtcl_Spawn(EftUPtclWork *w, EftUPtclWork *w2, s32 count);
void EftPtcl_StepPtcls(EftUPtclWork *w, EftUPtclWork *w2);

#endif
