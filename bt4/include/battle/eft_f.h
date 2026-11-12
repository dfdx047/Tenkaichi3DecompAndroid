#ifndef BATTLE_EFT_F_H
#define BATTLE_EFT_F_H

#include "types.h"

/* Water-surface effects, second half: the particle pools of the water module (splashes, technique trails),
   their update and draw code. As linked this is the second part of src/battle/eft_e.c (0x142CA0..0x147050; formerly eft_f.c).
   The module starts in eft_e.c (0x140338: EftWater_GetSurfaceY, the EftWater_Add... entry points, the task
   functions EftWater_Init / Update / PostUpdate / Draw and the splash allocator) and both halves were one source
   file: EftWaterRing_Update only compiles to the original bytes when EftWater_GetSurfaceY is defined earlier in
   the same file. The screen-space helpers it calls continue in eft_g.c (0x147050..).
   All names are guesses. The types here are this file's own views; eft_e.h describes the same state block as
   EftWater and the splash as EftWaterSplash (drop = billboard, spray = streak, mist = swirl, trail = burst). */

/* Four floats, 8-byte aligned: passed by value (the callee copies it with two ld/sd pairs). */
typedef struct EftWaterVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) EftWaterVec; /* size 0x10 */

/* The same four floats as a second type: the mist draw's constant vector is a separate copy in .rodata, which
   this compiler only emits when the two initialisers have different types. */
typedef struct EftWaterVec2 {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) EftWaterVec2; /* size 0x10 */

/* A projected vertex as the screen-space helpers return it: GS x, y (12.4 fixed), z, and a fourth word. */
typedef struct EftWaterIVec {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 w;
} __attribute__((aligned(8))) EftWaterIVec; /* size 0x10 */

/* A vertex of the clipper's polygon (ClipVtx_SetArray / ClipPoly_ClipPlane / ClipPoly_ProjectCur). */
typedef struct EftWaterClipVtx {
    /* 0x00 */ EftWaterVec pos;
    /* 0x10 */ EftWaterVec uv;
    /* 0x20 */ EftWaterVec color; /* 0..255 */
} EftWaterClipVtx; /* size 0x30 */

typedef struct EftWaterMtx {
    /* 0x00 */ f32 m[4][4];
} __attribute__((aligned(16))) EftWaterMtx; /* size 0x40 */

/* Particle flags, shared by drops, rings and sprays (flags word of each). */
#define EFT_WATER_WAIT    0x001 /* counting down `delay`, not yet visible */
#define EFT_WATER_LIVE    0x002 /* moving and drawn */
#define EFT_WATER_FADE_IN 0x004 /* ring: alpha rises to alphaMax; drop: alpha rises to 128 */
#define EFT_WATER_FADE_OUT 0x008 /* alpha falls to 0 */
#define EFT_WATER_ENDING  0x040 /* last fade running */
#define EFT_WATER_DEAD    0x080 /* finished: the list update unlinks it */
#define EFT_WATER_RISE    0x100 /* drop / spray: alpha (or progress) starts at 0 and counts up */

/* Billboard drop. Pool of 60 at EftWaterView.dropPool. */
typedef struct EftWaterDrop {
    /* 0x00 */ EftWaterVec pos;     /* offset from the emitter origin (the draw adds the origin) */
    /* 0x10 */ EftWaterVec base;    /* start offset */
    /* 0x20 */ EftWaterVec vel;     /* per unit of `t` */
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 sizeVel;
    /* 0x38 */ f32 sizeDamp;       /* sizeVel += sizeVel * sizeDamp each frame; 1 / the argument, or 0 */
    /* 0x3C */ f32 rot;            /* degrees; constant */
    /* 0x40 */ f32 rotVel;         /* stored, not read here */
    /* 0x44 */ f32 alpha;          /* 0..128 */
    /* 0x48 */ f32 alphaStep;
    /* 0x4C */ f32 gravity;        /* argument * 9.80665 */
    /* 0x50 */ f32 t;              /* seconds since it went live */
    /* 0x54 */ f32 dt;             /* 1 / 30 */
    /* 0x58 */ f32 delay;          /* frames */
    /* 0x5C */ u8 r;
    /* 0x5D */ u8 g;
    /* 0x5E */ u8 b;
    /* 0x5F */ u8 tex;             /* index into EftWaterView.tex */
    /* 0x60 */ u8 layer;
    /* 0x61 */ u8 unk61[3];
    /* 0x64 */ s32 flags;          /* EFT_WATER_*; 0 = free slot */
    /* 0x68 */ struct EftWaterDrop *prev;
    /* 0x6C */ struct EftWaterDrop *next;
} EftWaterDrop; /* size 0x70 */

/* Flat quad on the water surface (ripple ring). Pool of 30 at EftWaterView.ringPool. */
typedef struct EftWaterRing {
    /* 0x00 */ EftWaterVec pos;     /* world position; y follows the surface under it */
    /* 0x10 */ f32 size;
    /* 0x14 */ f32 sizeVel;
    /* 0x18 */ f32 sizeDamp;
    /* 0x1C */ f32 alpha;
    /* 0x20 */ f32 alphaStep;
    /* 0x24 */ f32 alphaMax;
    /* 0x28 */ f32 rot;            /* degrees about the vertical */
    /* 0x2C */ f32 fadeTime;       /* seconds of the fade out that follows the fade in */
    /* 0x30 */ f32 delay;          /* frames */
    /* 0x34 */ u8 r;
    /* 0x35 */ u8 g;
    /* 0x36 */ u8 b;
    /* 0x37 */ u8 tex;
    /* 0x38 */ u8 layer;
    /* 0x39 */ u8 unk39[3];
    /* 0x3C */ s32 flags;
    /* 0x40 */ struct EftWaterRing *prev;
    /* 0x44 */ struct EftWaterRing *next;
    /* 0x48 */ u8 unk48[8];
} EftWaterRing; /* size 0x50 */

/* Stretched quad that flies along a direction (a sheet of spray). Pool of 30 at EftWaterView.sprayPool. */
typedef struct EftWaterSpray {
    /* 0x00 */ EftWaterVec pos;     /* offset from the emitter origin */
    /* 0x10 */ EftWaterVec base;
    /* 0x20 */ EftWaterVec vel;
    /* 0x30 */ EftWaterVec dir;     /* unit horizontal direction */
    /* 0x40 */ f32 tilt;           /* degrees, turns by tiltVel */
    /* 0x44 */ f32 tiltVel;
    /* 0x48 */ f32 size;
    /* 0x4C */ f32 sizeVel;
    /* 0x50 */ f32 sizeDamp;
    /* 0x54 */ f32 progress;       /* 0..progressMax; drawn as the alpha-test reference (the quad dissolves) */
    /* 0x58 */ f32 progressStep;
    /* 0x5C */ f32 progressMax;    /* 96 */
    /* 0x60 */ f32 roll;
    /* 0x64 */ f32 gravity;
    /* 0x68 */ f32 nearScale;      /* the quad runs from size * nearScale to 0 along its direction */
    /* 0x6C */ f32 widthScale;
    /* 0x70 */ f32 t;
    /* 0x74 */ f32 dt;
    /* 0x78 */ f32 delay;
    /* 0x7C */ u8 r;
    /* 0x7D */ u8 g;
    /* 0x7E */ u8 b;
    /* 0x7F */ u8 tex;
    /* 0x80 */ u8 layer;
    /* 0x81 */ u8 unk81[3];
    /* 0x84 */ s32 flags;
    /* 0x88 */ struct EftWaterSpray *prev;
    /* 0x8C */ struct EftWaterSpray *next;
} EftWaterSpray; /* size 0x90 */

/* A spray that orbits: its direction turns every frame. Pool of 30 at EftWaterView.mistPool. */
typedef struct EftWaterMist {
    /* 0x00 */ EftWaterVec pos;
    /* 0x10 */ EftWaterVec base;
    /* 0x20 */ EftWaterVec dir;
    /* 0x30 */ EftWaterVec vel;
    /* 0x40 */ f32 radius;
    /* 0x44 */ f32 tilt;
    /* 0x48 */ f32 tiltVel;
    /* 0x4C */ f32 size;
    /* 0x50 */ f32 sizeVel;
    /* 0x54 */ f32 sizeDamp;
    /* 0x58 */ f32 progress;       /* drawn as the alpha-test reference: the quad dissolves as it rises */
    /* 0x5C */ f32 progressStep;
    /* 0x60 */ f32 progressMax;    /* 96 */
    /* 0x64 */ f32 yaw;            /* degrees, turns by yawVel */
    /* 0x68 */ f32 yawVel;
    /* 0x6C */ f32 yawDamp;
    /* 0x70 */ f32 roll;
    /* 0x74 */ f32 gravity;
    /* 0x78 */ f32 nearScale;      /* as EftWaterSpray */
    /* 0x7C */ f32 widthScale;
    /* 0x80 */ f32 t;
    /* 0x84 */ f32 dt;
    /* 0x88 */ f32 delay;
    /* 0x8C */ u8 r;
    /* 0x8D */ u8 g;
    /* 0x8E */ u8 b;
    /* 0x8F */ u8 state;           /* 0 waiting, 1 just started, 2 running */
    /* 0x90 */ u8 fading;          /* bit 0: progress still moving; the mist is finished when it stops */
    /* 0x91 */ u8 tex;
    /* 0x92 */ u8 layer;
    /* 0x93 */ u8 unk93;
    /* 0x94 */ s32 used;           /* 0 = free slot */
    /* 0x98 */ s32 live;
    /* 0x9C */ struct EftWaterMist *prev;
    /* 0xA0 */ struct EftWaterMist *next;
    /* 0xA4 */ u8 unkA4[0xC];
} EftWaterMist; /* size 0xB0 */

/* One puff of a technique's trail over the water: three particle lists that live and die together. Pool of 15 at EftWaterView.trailPool. */
typedef struct EftWaterTrail {
    /* 0x00 */ s32 flags;          /* 2 while in use, | EFT_WATER_ENDING once told to fade; 0 = free slot */
    /* 0x04 */ EftWaterDrop *dropHead;
    /* 0x08 */ EftWaterDrop *dropTail;
    /* 0x0C */ EftWaterSpray *sprayHead;
    /* 0x10 */ EftWaterSpray *sprayTail;
    /* 0x14 */ EftWaterRing *ringHead;
    /* 0x18 */ EftWaterRing *ringTail;
    /* 0x1C */ struct EftWaterTrail *prev;
    /* 0x20 */ struct EftWaterTrail *next;
} EftWaterTrail; /* size 0x24 */

/* A splash (EftWaterSplash in eft_e.h, allocated there): the particles one impact with the water created. */
typedef struct EftWaterSplashView {
    /* 0x00 */ EftWaterVec pos;     /* origin the drops and sprays are drawn relative to */
    /* 0x10 */ s32 flags;          /* | EFT_WATER_ENDING once told to fade; cleared when unlinked */
    /* 0x14 */ EftWaterDrop *dropHead;
    /* 0x18 */ EftWaterDrop *dropTail;
    /* 0x1C */ EftWaterSpray *sprayHead;
    /* 0x20 */ EftWaterSpray *sprayTail;
    /* 0x24 */ EftWaterRing *ringHead;
    /* 0x28 */ EftWaterRing *ringTail;
    /* 0x2C */ struct EftWaterSplashView *prev;
    /* 0x30 */ struct EftWaterSplashView *next;
} EftWaterSplashView; /* size >= 0x34 */

/* The module's state (gEftDust, EftWater in eft_e.h; the global's name predates the finding that this is the
   water module). Only the fields this range touches. */
typedef struct EftWaterView {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ EftWaterTrail *trailPool;   /* 15 */
    /* 0x0C */ EftWaterDrop *dropPool;     /* 60 */
    /* 0x10 */ EftWaterRing *ringPool;     /* 30 */
    /* 0x14 */ EftWaterSpray *sprayPool; /* 30 */
    /* 0x18 */ EftWaterMist *mistPool;   /* 30 */
    /* 0x1C */ s32 srcCount;
    /* 0x20 */ s32 trailCount;
    /* 0x24 */ s32 unk24;                 /* zeroed at the start of the source update */
    /* 0x28 */ s32 trailFading;           /* trails told to fade this frame */
    /* 0x2C */ s32 dropCount;
    /* 0x30 */ s32 ringCount;
    /* 0x34 */ s32 sprayCount;
    /* 0x38 */ s32 mistCount;
    /* 0x3C */ u8 unk3C[0x34];
    /* 0x70 */ EftWaterMtx billboard;      /* multiplied into the drop corners (camera-facing rotation) */
    /* 0xB0 */ struct {
        /* 0x00 */ u64 tex0;              /* GS TEX0 of texture i (EftWater_UpdateTextures) */
        /* 0x08 */ u64 unk8;
    } tex[5];
} EftWaterView;

/* The part of a blast hit record the trail spawner reads (0x190-byte record, see btl_char_coll.h). */
typedef struct EftWaterHitRec {
    /* 0x00 */ u8 unk0[0x20];
    /* 0x20 */ EftWaterVec pos;
    /* 0x30 */ EftWaterVec prevPos;
} EftWaterHitRec;

extern EftWaterView *gEftDust;

void EftWaterSplash_UpdateList(EftWaterSplashView **head, EftWaterSplashView **tail);
s32 EftWaterSplash_Update(EftWaterSplashView *src);
void EftWaterSplash_DrawList(EftWaterSplashView *src);
void EftWaterSplash_FadeOut(EftWaterSplashView *src);
EftWaterTrail *EftWaterTrail_Alloc(void);
s32 EftWaterTrail_IsEmpty(EftWaterTrail *trail);
void EftWaterTrail_Spawn(EftWaterTrail **head, EftWaterTrail **tail, s32 objId, EftWaterHitRec *hit);
void EftWaterTrail_UpdateList(EftWaterTrail **head, EftWaterTrail **tail);
s32 EftWaterTrail_Update(EftWaterTrail *trail);
void EftWaterTrail_DrawList(EftWaterTrail *trail);
void EftWaterTrail_FadeOut(EftWaterTrail *trail);

EftWaterDrop *EftWaterDrop_Alloc(void);
void EftWaterDrop_Spawn(EftWaterDrop **head, EftWaterDrop **tail, EftWaterVec pos, f32 yaw, f32 pitch, f32 speed, f32 rot, f32 rotVel, f32 size, f32 sizeVel, f32 sizeDamp,
                       f32 gravity, f32 life, f32 delay, u8 r, u8 g, u8 b, u8 tex,
                       u8 layer, s32 flags);
void EftWaterDrop_UpdateList(EftWaterDrop **head, EftWaterDrop **tail);
s32 EftWaterDrop_Update(EftWaterDrop *drop);
void EftWaterDrop_DrawList(EftWaterDrop *drop, EftWaterVec origin);
void EftWaterDrop_FadeOutList(EftWaterDrop *drop);

EftWaterRing *EftWaterRing_Alloc(void);
void EftWaterRing_Spawn(EftWaterRing **head, EftWaterRing **tail, EftWaterVec pos, f32 rot, f32 size, f32 sizeVel, f32 sizeDamp, f32 alphaMax, f32 time, f32 delay, u8 r, u8 g, u8 b, u8 tex,
                       u8 layer, s32 flags);
void EftWaterRing_UpdateList(EftWaterRing **head, EftWaterRing **tail);
s32 EftWaterRing_Update(EftWaterRing *ring);
void EftWaterRing_DrawList(EftWaterRing *ring);
void EftWaterRing_FadeOutList(EftWaterRing *ring);

EftWaterSpray *EftWaterSpray_Alloc(void);
void EftWaterSpray_Spawn(EftWaterSpray **head, EftWaterSpray **tail, EftWaterVec pos, f32 dist, f32 height, f32 yaw, f32 pitch, f32 roll, f32 tilt, f32 tiltEnd,
                         f32 speed, f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity, f32 nearScale,
                         f32 widthScale, f32 life, f32 delay, u8 r, u8 g, u8 b, u8 tex,
                         u8 layer, s32 flags);
void EftWaterSpray_UpdateList(EftWaterSpray **head, EftWaterSpray **tail);
s32 EftWaterSpray_Update(EftWaterSpray *spray);
void EftWaterSpray_DrawList(EftWaterSpray *spray, EftWaterVec origin);
void EftWaterSpray_FadeOutList(EftWaterSpray *spray);

EftWaterMist *EftWaterMist_Alloc(void);
void EftWaterMist_Spawn(EftWaterMist **head, EftWaterMist **tail, EftWaterVec pos, f32 radius, f32 height, f32 yaw, f32 yawVel, f32 yawDamp, f32 pitch, f32 roll,
                        f32 tilt, f32 tiltEnd, f32 speed, f32 size, f32 sizeVel, f32 sizeDamp, f32 gravity,
                        f32 nearScale, f32 widthScale, f32 life, f32 delay, u8 r, u8 g, u8 b,
                        u8 tex, u8 layer);
void EftWaterMist_UpdateList(EftWaterMist **head, EftWaterMist **tail);
s32 EftWaterMist_Update(EftWaterMist *mist);
void EftWaterMist_DrawList(EftWaterMist *mist);

s32 EftWater_IsOnScreen(EftWaterIVec v, s32 margin);
void EftWater_DrawBillboard(EftWaterVec *pos, EftWaterMtx *world2screen, f32 size, f32 rot, u8 r, u8 g, u8 b, u8 a,
                           u8 alphaRef, u64 *tex, u8 layer);
void EftWater_DrawGroundQuad(EftWaterVec *pos, EftWaterMtx *world2screen, f32 size, f32 rot, u8 r, u8 g, u8 b, u8 a,
                            u64 *tex, u8 layer);
void EftWater_DrawSprayQuad(EftWaterVec *pos, EftWaterMtx *orient, EftWaterMtx *world2screen, f32 roll, f32 size,
                            f32 near, f32 far, f32 width, u8 r, u8 g, u8 b, u8 a, u8 alphaRef, u64 *tex,
                            u8 layer);
void EftWater_UpdateTextures(s32 a0, s32 a1);
void EftWater_DrawClippedFan(EftWaterClipVtx *poly, s32 layer, u64 tex0);

#endif
