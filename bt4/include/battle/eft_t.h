#ifndef BATTLE_EFT_T_H
#define BATTLE_EFT_T_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect modules 0x17CB40..0x180BF8 (71 functions), all of them drawing only:
 *   src/battle/eft_s.c (second part, formerly eft_t.c)  0x17CB40..0x17D290  tail of the "chain" module (effect pack part kind 18): the last three
 *                                             callbacks of its task class 0x2C3E48, its manager class 0x2C3E30 and
 *                                             the by-handle interface the effect pack library calls
 *                         0x17D290..0x17EE68  ray burst (effect pack part kind 0, fighter requests 0x15 and 0x18):
 *                                             manager class 0x2C3E60, task class 0x2C3E78
 *   src/battle/eft_t_b.c  0x17EE68..0x1809C0  streak field (the "numbered stage effect" technique modules start):
 *                                             manager class 0x2C3E90, task class 0x2C3EA8
 *   src/battle/eft_t_c.c  0x1809C0..0x180BF8  head of the vanish-line module EftShotFx (class 0x2C3EC0, the rest is the second part of that file, formerly eft_u.c): the
 *                                             request entry of fighter requests 0xC..0xF, and the init callback
 *
 * A class is six callbacks {update, init, term, post-update, reset, draw}; the manager classes are entries of the
 * list at 0x2C3FB0 (pairs {class, 1}: chain 0x2C4058, ray 0x2C4088, streak 0x2C4000, vanish lines 0x2C3FD8).
 * Every structure is a view local to these files.
 */

/* The effect code's vector: 16-byte aligned, so copies are 64-bit moves. */
typedef struct EftTVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) EftTVec;

struct EftTTaskList;

/* A task of the effect scene (0x40 bytes); only what these modules touch. */
typedef struct EftTTask {
    /* 0x00 */ u8 flags;          /* bit 0: dead */
    /* 0x01 */ u8 unk1[7];
    /* 0x08 */ u8 unk8[0x10];     /* texture animation state A (EftVram_AddImage) */
    /* 0x18 */ u8 unk18[0xC];     /* texture animation state B (EftVram_AddClut) */
    /* 0x24 */ struct EftTTaskList *children; /* the list BtlTask_CreateChildList made for this task */
    /* 0x28 */ void **cls;        /* task class; cls[0] is the update callback, used as a type tag */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
    /* 0x3C */ s32 unk3C;
} EftTTask; /* size 0x40 */

/* A list of child tasks as BtlTask_CreateChildList returns it; only the test the ray manager makes. */
typedef struct EftTTaskList {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 count;         /* live children */
} EftTTaskList;

/* A screen position as Mtx_ProjectPoint writes it: GS units (1 / 16 pixel) with the 0x7000 / 0x7200 offset. */
typedef struct EftTIVec {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftTIVec;

/* The camera view being drawn (gBtlCamView); only what these modules read. */
typedef struct EftTCamView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ Mtx44 view;      /* world to view */
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 screen;    /* world to screen */
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 pos;
} EftTCamView;

/* One entry of a texture set (EftTexSet_Load32 / EftTexSet_Load4 / EftTexSet_Load8). */
typedef struct EftTTex {
    /* 0x0 */ u64 tex0;
    /* 0x8 */ u64 unk8;
} EftTTex;

/* ---- chain module (part kind 18) -------------------------------------------------------------------------- */

/* Argument of EftChain_Create (EftEmitArgA in eft_h.h). */
typedef struct EftChainArg {
    /* 0x00 */ EftTVec pos;
    /* 0x10 */ EftTVec dir;
    /* 0x20 */ s32 objId;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ f32 rate;
    /* 0x2C */ f32 size;
    /* 0x30 */ u8 *res;
    /* 0x34 */ s32 *texA;
    /* 0x38 */ s32 *texB;
} EftChainArg; /* size 0x40 */

/* One link of a strand. */
typedef struct EftChainLink {
    /* 0x00 */ Vec4 unk0;
    /* 0x10 */ Vec4 unk10;
    /* 0x20 */ Vec4 pos;
    /* 0x30 */ u8 unk30[0x24];
    /* 0x54 */ struct EftChainLink *next;
} EftChainLink;

/* One of the 16 strands of a chain effect. */
typedef struct EftChainStrand {
    /* 0x00 */ Mtx44 mtx;
    /* 0x40 */ u8 unk40[0x20];
    /* 0x60 */ EftChainLink *head;
    /* 0x64 */ s32 unk64;
    /* 0x68 */ s32 flags;         /* bit 0: in use */
    /* 0x6C */ u8 unk6C[0x64];
} EftChainStrand; /* size 0xD0 */

/* EftChainWork.flags */
#define EFT_CHAIN_ALIVE 0x01
#define EFT_CHAIN_STOP 0x02       /* let it finish */
#define EFT_CHAIN_KILL 0x04       /* end now */
#define EFT_CHAIN_VIEW_ONLY 0x10  /* drawn only in the view that shows its fighter */

/* Work of the task class 0x2C3E48 (update callback 0x17C8F8, in the file before this one). */
typedef struct EftChainWork {
    /* 0x000 */ EftChainArg arg;  /* the create argument, copied whole by the init callback */
    /* 0x040 */ u8 unk40[0x100];
    /* 0x140 */ EftChainStrand strand[16];
    /* 0xE40 */ Mtx44 rot;        /* rotation by pitch about X, then yaw about Y */
    /* 0xE80 */ s32 flags;        /* EFT_CHAIN_* */
    /* 0xE84 */ u8 type;          /* owner's effect type, for the scene's "is this effect hidden / stopped" tests */
    /* 0xE85 */ u8 unkE85[0xB];
    /* 0xE90 */ s32 unkE90;       /* module parameter 3, kept in 3..7 */
    /* 0xE94 */ u8 unkE94[0xC];
    /* 0xEA0 */ f32 unkEA0;       /* module parameter 5 */
    /* 0xEA4 */ f32 unkEA4;       /* module parameter 6; the pack library zeroes it for a fading stop */
    /* 0xEA8 */ u8 unkEA8[0xC];
    /* 0xEB4 */ f32 pitch;
    /* 0xEB8 */ f32 yaw;
    /* 0xEBC */ u8 unkEBC[0x54];
} EftChainWork; /* size 0xF10 */

/* gEftChain (0x2FEA64). */
typedef struct EftChainMgr {
    /* 0x0 */ u8 *buf;            /* 0xBB80 bytes */
    /* 0x4 */ s32 unk4;
} EftChainMgr;

/* ---- ray burst (part kind 0) ------------------------------------------------------------------------------ */

/* Argument of EftRay_Create (EftEmitLightArg in eft_h.h, FxLineArg in btl_char_fx.h). */
typedef struct EftRayArg {
    /* 0x00 */ EftTVec pos;
    /* 0x10 */ s32 color[4];      /* r, g, b, alpha; each ray's alpha is alpha - rand() % 16 */
    /* 0x20 */ f32 life;          /* seconds before the fade starts; <= 0: no timed fade */
    /* 0x24 */ f32 length;        /* ray length; + up to 20 at random */
    /* 0x28 */ f32 width;         /* ray width factor (times up to 5 at random, at least 0.4) */
    /* 0x2C */ f32 inner;         /* distance of the rays' near end from the centre */
    /* 0x30 */ f32 jitter;        /* random extra distance per ray */
    /* 0x34 */ s32 mode;          /* EFT_RAY_MODE_* */
    /* 0x38 */ u32 count;         /* rays, at most 48 */
    /* 0x3C */ s32 objId;         /* owner; negative = none */
    /* 0x40 */ s32 blend;         /* passed to the quad queue */
    /* 0x44 */ s32 space;         /* 0: in the world, facing the camera; 1: on the screen */
    /* 0x48 */ s32 delay;         /* frames before anything happens */
    /* 0x4C */ s32 fadeFrames;    /* frames the final fade takes */
    /* 0x50 */ s32 autoKill;      /* non-zero: the task ends itself when the fade is over */
    /* 0x54 */ s32 unk54[3];
} EftRayArg; /* size 0x60 */

typedef struct EftRay {
    /* 0x00 */ u8 r, g, b, a;
    /* 0x04 */ f32 angle;
    /* 0x08 */ f32 length;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 width;
    /* 0x14 */ f32 offset;
} EftRay; /* size 0x18 */

#define EFT_RAY_MAX 48

#define EFT_RAY_MODE_SHRINK 0     /* static rays that shrink away */
#define EFT_RAY_MODE_SPIN 1       /* re-rolled every frame */
#define EFT_RAY_MODE_TECH 2       /* re-rolled; ends when its fighter or the opponent starts a technique */
#define EFT_RAY_MODE_CUT 3        /* re-rolled; ends when a technique camera cut is running */

/* EftRayWork.flags */
#define EFT_RAY_TIMED 0x01        /* life is counting */
#define EFT_RAY_FADING 0x02       /* alpha is fading */
#define EFT_RAY_FADED 0x04        /* the fade is over (EftRay_IsAlive turns it into DEAD) */
#define EFT_RAY_DEAD 0x08
#define EFT_RAY_AUTOKILL 0x10

/* Work of the task class 0x2C3E78. */
typedef struct EftRayWork {
    /* 0x000 */ EftRay ray[EFT_RAY_MAX];
    /* 0x480 */ EftTVec pos;      /* centre; screen pixels when space is 1 */
    /* 0x490 */ EftTVec scale;      /* x, y scale the quads; mode 0 shrinks x */
    /* 0x4A0 */ Mtx44 mtx;        /* the camera's rotation (space 0) or identity */
    /* 0x4E0 */ f32 shrink;       /* mode 0: scale.x lost per frame */
    /* 0x4E4 */ f32 life;         /* seconds */
    /* 0x4E8 */ f32 alpha;        /* 1 -> 0 while fading */
    /* 0x4EC */ f32 alphaStep;    /* 1 / fadeFrames */
    /* 0x4F0 */ f32 inner;
    /* 0x4F4 */ f32 jitter;
    /* 0x4F8 */ s32 alphaRange;   /* EftRayArg.color[3]: modes 1..3 re-roll each alpha as rand() % this */
    /* 0x4FC */ s32 count;
    /* 0x500 */ s32 blend;
    /* 0x504 */ s32 space;
    /* 0x508 */ u32 mode;
    /* 0x50C */ s32 objId;
    /* 0x510 */ s32 delay;
    /* 0x514 */ s32 flags;        /* EFT_RAY_* */
    /* 0x518 */ u8 type;          /* owner's effect type (2 unless the pack library sets it) */
    /* 0x519 */ u8 unk519[7];
} EftRayWork; /* size 0x520 */

/* gEftRay (0x2FEA6C), 0x210 bytes. */
typedef struct EftRayMgr {
    /* 0x000 */ EftTTex tex[32];  /* texture set of common entry 0xA; only tex[0] is used */
    /* 0x200 */ s32 texCount;
    /* 0x204 */ s32 unk204;
    /* 0x208 */ EftTTaskList *list; /* 4 tasks of 0x520 bytes */
    /* 0x20C */ s32 unk20C;
} EftRayMgr; /* size 0x210 */

/* A clip-space vertex as ClipVtx_Set / ClipVtx_SetArray build it (0x30 bytes). */
typedef struct EftTClipVtx {
    /* 0x00 */ EftTVec pos;
    /* 0x10 */ EftTVec st;
    /* 0x20 */ EftTVec col;
} EftTClipVtx;

/* GS XYZF2 register value. */
typedef struct EftTXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftTXyzf;

typedef struct EftTGsVtx {
    /* 0x00 */ u8 rgba[4];
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ EftTXyzf xyz;
} EftTGsVtx; /* 0x18: RGBAQ, ST, XYZF2 */

/* Triangle strip of four vertices, as queued in the ordering table (0x90 bytes). */
typedef struct EftTQuadPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftTGsVtx v[4];
} EftTQuadPkt;

/* ---- streak field ----------------------------------------------------------------------------------------- */

/* Parameters of one streak field kind (common entries 0x24F..0x258, one per kind 0..9). */
typedef struct EftStreakDef {
    /* 0x00 */ f32 widthMin;
    /* 0x04 */ f32 widthRange;
    /* 0x08 */ f32 lengthMin;
    /* 0x0C */ f32 lengthRange;
    /* 0x10 */ f32 lifeMin;       /* seconds */
    /* 0x14 */ f32 lifeRange;
    /* 0x18 */ f32 delayMax;      /* frames */
    /* 0x1C */ u8 unk1C[4];
    /* 0x20 */ f32 startDelay;    /* frames before the field starts */
    /* 0x24 */ f32 fadeFrames;    /* frames the field takes to fade once stopped */
    /* 0x28 */ f32 posMin;        /* a streak's start position along the field's axis */
    /* 0x2C */ f32 posRange;
    /* 0x30 */ f32 speedMin;      /* added to it per frame */
    /* 0x34 */ f32 speedRange;
    /* 0x38 */ f32 spread;        /* width of the band the streaks are spread over, across the axis */
    /* 0x3C */ f32 hole;          /* fraction of the band, around its middle, that is left empty */
    /* 0x40 */ s32 endMode;       /* 1: ends when either fighter starts a technique, 2: when a camera cut runs */
    /* 0x44 */ s32 space;         /* 0, 1: in the world (EFT_STREAK_WORLD); 2, 3: on the screen; 1, 3: at the
                                     owner (EFT_STREAK_AT_CHAR) */
    /* 0x48 */ u8 blend;
    /* 0x49 */ u8 count;          /* streaks */
} EftStreakDef;

typedef struct EftStreak {
    /* 0x00 */ EftTVec color;     /* r, g, b, alpha */
    /* 0x10 */ f32 width;
    /* 0x14 */ f32 length;
    /* 0x18 */ f32 pos;           /* along the axis */
    /* 0x1C */ f32 speed;
    /* 0x20 */ f32 offset;        /* across the axis */
    /* 0x24 */ f32 time;          /* frames lived */
    /* 0x28 */ f32 life;          /* frames; 0 = lives until the field ends */
    /* 0x2C */ f32 delay;         /* frames before it appears */
    /* 0x30 */ s32 flags;         /* 0 free; EFT_STREAK_ITEM_* */
    /* 0x34 */ struct EftStreak *next;
    /* 0x38 */ s32 unk38[2];
} EftStreak; /* size 0x40 */

#define EFT_STREAK_ITEM_USED 0x02
#define EFT_STREAK_ITEM_DRAWN 0x10
#define EFT_STREAK_ITEM_EXPIRED 0x80 /* re-rolled at once */

#define EFT_STREAK_MAX 360
#define EFT_STREAK_KINDS 10

/* EftStreakWork.flags */
#define EFT_STREAK_ALIVE 0x002
#define EFT_STREAK_STOP 0x004     /* fading out */
#define EFT_STREAK_DEAD 0x008
#define EFT_STREAK_SCREEN 0x100
#define EFT_STREAK_WORLD 0x200
#define EFT_STREAK_AT_CHAR 0x400
#define EFT_STREAK_POS_SET 0x800  /* EftStreak_SetPos gave it a position */

/* Argument of the task class 0x2C3EA8's init (built by EftStreak_Start). */
typedef struct EftStreakArg {
    /* 0x00 */ EftStreakDef *def;
    /* 0x04 */ EftTVec *colors;   /* common entry 0x24E: one colour per colour index */
    /* 0x08 */ f32 angle;
    /* 0x0C */ s32 kind;
    /* 0x10 */ s32 color;         /* colour index */
    /* 0x14 */ s32 objId;
    /* 0x18 */ f32 seconds;       /* how long the field runs; 0 = until stopped (what EftStreak_Start passes) */
} EftStreakArg;

/* Work of the task class 0x2C3EA8. */
typedef struct EftStreakWork {
    /* 0x00 */ EftStreakDef *def;
    /* 0x04 */ EftTVec *colors;
    /* 0x08 */ EftTTex tex;       /* copy of the manager's base texture */
    /* 0x18 */ EftTTex tex2;      /* copy of the manager's texture for this colour index */
    /* 0x28 */ u8 unk28[8];
    /* 0x30 */ EftTVec pos;       /* EftStreak_SetPos */
    /* 0x40 */ EftTVec axis;      /* scratch of the draw callback */
    /* 0x50 */ f32 angle;         /* rotation of the whole field about the view axis */
    /* 0x54 */ f32 cover;         /* scales EftStreakDef.hole; 1 unless EftStreak_SetCover */
    /* 0x58 */ f32 timer;         /* frames until the field stops by itself; 0 = never */
    /* 0x5C */ f32 startDelay;
    /* 0x60 */ f32 fade;          /* frames left of the fade */
    /* 0x64 */ f32 alpha;         /* 1 -> 0 while fading */
    /* 0x68 */ u8 spawned;        /* streaks created so far */
    /* 0x69 */ u8 type;           /* 2: effect type for BtlScene_IsEffectStopped */
    /* 0x6A */ u8 color;
    /* 0x6B */ u8 objId;
    /* 0x6C */ s32 flags;         /* EFT_STREAK_* */
    /* 0x70 */ EftStreak *head;
    /* 0x74 */ EftStreak *tail;
    /* 0x78 */ s32 unk78[2];
} EftStreakWork; /* size 0x80 */

/* The module's work (0x5AD0 bytes). */
typedef struct EftStreakMgr {
    /* 0x0000 */ EftTTex tex[8];  /* texture set of common entry 0x24D; [0] is the one drawn, its TEX0 rebuilt
                                     once per frame */
    /* 0x0080 */ s32 texCount;
    /* 0x0084 */ s32 flags;       /* bit 0: the texture was stepped this frame */
    /* 0x0088 */ void *list;      /* 3 tasks of 0x80 bytes */
    /* 0x008C */ EftStreakDef *def[EFT_STREAK_KINDS];
    /* 0x00B4 */ EftTVec *colors; /* common entry 0x24E */
    /* 0x00B8 */ s32 unkB8[2];
    /* 0x00C0 */ EftStreak pool[EFT_STREAK_MAX];
    /* 0x5AC0 */ u32 next;        /* where the search for a free streak starts */
    /* 0x5AC4 */ s32 unk5AC4[3];
} EftStreakMgr; /* size 0x5AD0 */

/* gEftStreak (0x2FEA70) points at a one-word block that points at the work. */
typedef struct EftStreakRoot {
    /* 0x0 */ EftStreakMgr *mgr;
} EftStreakRoot;

/* ---- vanish lines (class 0x2C3EC0; the full layout is EftShotFxWork / EftTShotFxReq in eft_u.h) ------------- */

typedef struct EftTShotFxReq {
    /* 0x0 */ s32 objId;
    /* 0x4 */ s32 frameA;         /* frames since a kind 0 / 2 request */
    /* 0x8 */ s32 frameB;         /* frames since a kind 1 / 3 request */
    /* 0xC */ s32 flags;          /* 0 = free */
} EftTShotFxReq; /* size 0x10 */

/* gEftShotFx (0x2FF1B0), 0x60 bytes; only what the three functions of eft_t_c.c touch. */
typedef struct EftTShotFxMgr {
    /* 0x00 */ EftTShotFxReq *req; /* 2 */
    /* 0x04 */ u8 *lines;         /* 0x4B00 bytes: 200 lines of 0x60 */
    /* 0x08 */ u8 unk8[0x48];
    /* 0x50 */ EftTTex *tex;      /* 0x48 bytes: texture set of common entry 0x237 */
    /* 0x54 */ u8 unk54[0xC];
} EftTShotFxMgr; /* size 0x60 */

#endif
