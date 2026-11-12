#ifndef BATTLE_EFT_B_H
#define BATTLE_EFT_B_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect code 0x132290..0x136760 (src/battle/eft_b.c). Four unrelated pieces, all of them drawing only:
 *
 *   0x132290..0x1333C8  EftPrim_*        the tail of the shared primitive helpers of the previous file
 *                                        (camera-facing quads and gouraud triangles queued in the ordering
 *                                        table) and their "is this screen point usable" test.
 *   0x1333C8..0x135070  EftBubble_*      underwater air bubbles: sub-task of effect layer 0 (class
 *                                        gEftBubbleClass 0x2C34C0, id 7 in the layer's table 0x2C3568).
 *   0x135070..0x135610  EftStageScroll_* a scrolling textured sheet read from the stage model file: sub-task of
 *                                        effect layer 0 (class gEftStageScrollClass 0x2C34D8, id 0); drawn from
 *                                        the stage draw (0x115DE0), not from the task tree.
 *   0x135610..0x136760  EftGeyser_*      columns that rise at fixed stage positions on a timer: manager sub-task
 *                                        of layer 0 (class gEftGeyserMgrClass 0x2C34F0, id 6) with one child task
 *                                        per column (class gEftGeyserClass 0x2C3508). The module continues in
 *                                        the next file (0x136760..: the emitters a column owns).
 *
 * Task classes are {update, init, term, postUpdate, reset, draw} (BtlTaskClass in battle/btl_scene.h).
 */

/* Flag bytes are read as one-byte bit-field structs through a cast: as members of a word-aligned struct the
   compiler would use word loads, and the original uses byte loads. */
typedef struct EftBTaskFlags {
    u8 dying : 1;                   /* set by BtlTask_SetDead */
} EftBTaskFlags;
#define EFTB_TASK_FLAGS(t) ((EftBTaskFlags *)&(t)->flags)

typedef struct EftBubbleChrFlags {
    u8 trail : 1;                   /* never set: emit from the whole body every frame */
    u8 bursting : 1;                /* a burst is running */
} EftBubbleChrFlags;
#define EFT_BUBBLE_CHR_FLAGS(c) ((EftBubbleChrFlags *)&(c)->flags)

/* The 0x40-byte task of 0x1AD150.. (only what this file reads). */
typedef struct EftBTask {
    /* 0x00 */ u8 flags;            /* EftBTaskFlags */
    /* 0x01 */ u8 state;            /* free for the class: the geyser's phase */
    /* 0x02 */ u8 unk2[0x26];
    /* 0x28 */ void **cls;          /* BtlTaskClass: [0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;          /* work memory of the size given to BtlTask_CreateChildList */
    /* 0x3C */ s32 unk3C;
} EftBTask; /* size 0x40 */

/* One texture of a texture set (EftTexSet_Load32 fills it from a pack entry). */
typedef struct EftBTex {
    /* 0x00 */ u64 tex0;            /* GS TEX0 */
    /* 0x08 */ struct EftBTexImg *img;
    /* 0x0C */ s32 unkC;
} EftBTex; /* size 0x10 */

typedef struct EftBTexImg {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s32 texBlocks;       /* VRAM blocks the pixels take: added to the next texture's TBP0 */
    /* 0x14 */ s32 clutBlocks;      /* same for the palette and CBP */
} EftBTexImg;

typedef struct EftBTexSet {
    /* 0x000 */ EftBTex tex[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ s32 unk204;
} EftBTexSet; /* size 0x208 */

/* Vectors and matrices passed BY VALUE in this module (a pointer to the caller's object is passed and the callee
   copies it with doubleword loads) are 16-byte aligned types. */
typedef Vec4 EftBVec __attribute__((aligned(16)));
typedef Mtx44 EftBMtx __attribute__((aligned(16)));

/* A projected vertex as Mtx_ProjectPoint / Vu0Cur_ProjectPoint / Vu0Cur_ProjectPoints write it: GS 12.4 fixed point. */
typedef struct EftBIVec {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 w;
} __attribute__((aligned(16))) EftBIVec;

/* ---- bubbles ---- */

/* Per-fighter bubble state, gEftBubble->chr[object id]. */
typedef struct EftBubbleChr {
    /* 0x00 */ s32 objId;           /* written by EftBubble_StartBurst; 0 until then */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ f32 burst;           /* bubbles to emit this frame; 8 less every frame */
    /* 0x0C */ u8 flags;            /* EftBubbleChrFlags */
} EftBubbleChr; /* size 0x10 */

typedef struct EftBubble {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 vel;
    /* 0x20 */ f32 drag;            /* vel += vel * drag per frame (negative: slows down) */
    /* 0x24 */ f32 size;
    /* 0x28 */ f32 rot;             /* degrees, about the view axis */
    /* 0x2C */ f32 alpha;
    /* 0x30 */ f32 alphaStep;
    /* 0x34 */ f32 rise;            /* added to pos.y every frame (negative = up) */
    /* 0x38 */ f32 riseGrow;        /* rise += rise * riseGrow */
    /* 0x3C */ f32 phase;           /* sideways wobble angle */
    /* 0x40 */ f32 amp;             /* wobble amplitude */
    /* 0x44 */ f32 phaseStep;
    /* 0x48 */ s32 objId;           /* stored, never read */
    /* 0x4C */ s32 view;            /* camera view it is drawn in, -1 = both */
    /* 0x50 */ u8 r, g, b;
    /* 0x53 */ u8 tex;              /* index into gEftBubble->tex */
    /* 0x54 */ u8 layer;            /* ordering-table layer */
    /* 0x58 */ s32 active;
    /* 0x5C */ struct EftBubble *prev;
    /* 0x60 */ struct EftBubble *next;
    /* 0x64 */ u8 unk64[0xC];
} EftBubble; /* size 0x70 */

#define EFT_BUBBLE_MAX 100

typedef struct EftBubbleWork {
    /* 0x000 */ EftBubbleChr *chr;  /* 2 entries */
    /* 0x004 */ EftBubble *pool;    /* EFT_BUBBLE_MAX entries */
    /* 0x008 */ EftBubble *head;    /* active list */
    /* 0x00C */ EftBubble *tail;
    /* 0x010 */ s32 count;
    /* 0x014 */ u8 unk14[0xC];
    /* 0x020 */ Mtx44 camMtx;       /* camera rotation of the view being drawn */
    /* 0x060 */ f32 camTimer[2];    /* per view: accumulates towards the next bubble in front of the camera */
    /* 0x068 */ EftBTexSet tex;     /* stage pack entry 0x12 */
} EftBubbleWork; /* size 0x270 */

/* ---- stage scroll sheet ---- */

typedef struct EftStageScroll {
    /* 0x000 */ u8 unk0[8];
    /* 0x008 */ EftBTexSet tex;
    /* 0x210 */ u8 model[0x90];     /* object of 0x1A7608.. */
    /* 0x2A0 */ f32 dirX;           /* scroll direction (cos, sin of the angle) */
    /* 0x2A4 */ f32 dirY;
    /* 0x2A8 */ f32 offX;           /* scroll offset, kept within +-size */
    /* 0x2AC */ f32 offY;
    /* 0x2B0 */ f32 speed;          /* file value * 0.00001 * size */
    /* 0x2B4 */ f32 size;
    /* 0x2B8 */ f32 unk2B8;
    /* 0x2BC */ f32 angle;          /* degrees; negative in the file = random */
    /* 0x2C0 */ s32 enabled;
    /* 0x2C4 */ u8 unk2C4[0xC];
} EftStageScroll; /* size 0x2D0 */

/* ---- geysers ---- */

/* Parameters of one column: the argument of EftGeyser_Create, kept twice in the work (initial, current). */
typedef struct EftGeyserParam {
    /* 0x00 */ Vec4 pos;            /* stage record position */
    /* 0x10 */ Vec4 base;           /* pos with y + height; the column is drawn upwards from here */
    /* 0x20 */ Vec4 color;          /* r, g, b, a (0..255) */
    /* 0x30 */ Vec4 curColor;       /* w = current alpha */
    /* 0x40 */ f32 width;
    /* 0x44 */ f32 height;
    /* 0x48 */ f32 unk48;           /* 1.0 */
    /* 0x4C */ f32 unk4C;           /* 1.0 */
    /* 0x50 */ f32 sink;            /* 2.0: base.y gain per frame while fading out */
    /* 0x54 */ f32 scroll;          /* texture scroll per frame */
    /* 0x58 */ s32 hold;            /* frames in phase 3 */
    /* 0x5C */ s32 fade;            /* frames to reach full alpha */
    /* 0x60 */ s32 wait;            /* frames in phase 5 */
    /* 0x64 */ u8 unk64[0xC];
} __attribute__((aligned(16))) EftGeyserParam; /* size 0x70; 16-byte aligned: it is copied with doubleword moves */

typedef struct EftGeyser {
    /* 0x00 */ EftGeyserParam init;
    /* 0x70 */ EftGeyserParam cur;
    /* 0xE0 */ f32 alphaStep;       /* color.w / fade */
    /* 0xE4 */ f32 texV;            /* scroll position, 0..1 */
    /* 0xE8 */ EftBTexSet *tex;
    /* 0xEC */ void *emitterA;      /* 0x148DF8 / 0x148E38 / 0x148ED0 */
    /* 0xF0 */ void *emitterB;      /* 0x13F470 / 0x13F4C0 / 0x13F568 */
    /* 0xF4 */ s32 delay;           /* frames before the first eruption: rand() % 180 */
    /* 0xF8 */ u8 unkF8[8];
} EftGeyser; /* size 0x100 */

/* A geyser record of the stage data (BtlStage_GetFxResA2). */
typedef struct EftGeyserRec {
    /* 0x00 */ f32 x, y, z;
    /* 0x0C */ f32 width;
    /* 0x10 */ f32 height;
    /* 0x14 */ f32 scroll;
    /* 0x18 */ u16 hold;
    /* 0x1A */ u16 fade;
    /* 0x1C */ u16 wait;
    /* 0x1E */ u8 r, g, b, a;
    /* 0x22 */ u8 unk22[0xE];
} EftGeyserRec; /* size 0x30 */

typedef struct EftGeyserMgr {
    /* 0x000 */ void *list;         /* child task list, one task per column */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ EftBTexSet tex;     /* stage pack entry 0x12 */
} EftGeyserMgr; /* size 0x210 */

extern EftBubbleWork *gEftBubble;
extern EftStageScroll *gEftStageScroll;
extern EftGeyserMgr *gEftGeyserMgr;

void EftPrim_DrawBillboard(Vec4 *pos, Vec4 *color, s32 layer, s32 noDepth, u64 tex0, f32 w, f32 h, f32 u0, f32 v0,
                           f32 u1, f32 v1, f32 rot, f32 zScale);
s32 EftPrim_IsOffScreen(s32 x, s32 y, s32 z);

s32 EftBubble_StartBurst(s32 objId);
void EftBubble_Add(EftBVec pos, EftBVec vel, f32 drag, f32 size, f32 alpha, f32 rot, f32 rise, f32 riseGrow, f32 phase,
                   f32 amp, u8 r, u8 g, u8 b, s32 objId, s32 view, u8 tex, f32 phaseStep, f32 life, u8 layer);

void EftStageScroll_Draw(void);

void *EftGeyser_Create(EftGeyserParam *param);
EftGeyser *EftGeyser_GetWork(EftBTask *task);
void EftGeyser_Kill(EftBTask *task);

#endif
