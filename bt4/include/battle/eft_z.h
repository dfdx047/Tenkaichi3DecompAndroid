#ifndef BATTLE_EFT_Z_H
#define BATTLE_EFT_Z_H

#include "types.h"
#include "sys/math3d.h"
#include "sys/list.h"

/*
 * Effect tasks, 0x195EE8..0x199F28 (src/battle/eft_z.c 0x195EE8..0x198BC0, eft_z_b.c ..0x199500, eft_z_c.c
 * ..0x199F28). Two modules, both drawing only:
 *   0x195EE8..0x196E40  effect pack part kind 16: one camera-facing sprite per part ("EftBill"). The file starts
 *                       in the middle of this module: its helpers (0x195038 texture, 0x195070 position,
 *                       0x195130 / 0x1952E8 key animation, 0x195718 / 0x195C68 the two "shape 1" draws) are in
 *                       the range before.
 *   0x196E40..0x199F28  ground dust ("EftGndDust"): a manager with two particle pools and six task kinds, started
 *                       by the fighter's ground effect requests, by stage debris and by a projectile that
 *                       hits the ground. Its particle helpers from 0x199F28 on are in the range after.
 * All structs are this file's views.
 */

/* A vector on a 16-byte boundary, as the effect code's argument blocks hold them. A union whose FIRST member is
   the array view: initialised through the array, the three identical `up` vectors of the dust spawners stay
   three constants (0x2ED330, 0x2ED340, 0x2ED350) as in the original; initialised through a struct of four floats
   the compiler merges them into one. */
typedef union EftZVec {
    f32 v[4];
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
} __attribute__((aligned(16))) EftZVec;

/* Task as the effect code sees it (include/battle/eft_g.h has the same view). */
typedef struct EftZTask {
    /* 0x00 */ u8 unk0[0x28];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftZTask;

/* ---- part kind 16: sprite ------------------------------------------------------------------------------- */

/* Sprite definition, a record of the effect pack (EftEmitArgA.texA in eft_h.h points at it). */
typedef struct EftBillDef {
    /* 0x00 */ s32 flags;      /* 1 key animation, 2 depth-sorted quad, 4 size pulse, 8 sheet animation,
                                  0x10 colour pulse */
    /* 0x04 */ u8 blend;       /* blend mode handed to the draw routine */
    /* 0x05 */ u8 shape;       /* 0 camera-facing sprite, 1 the oriented quads at 0x195718 / 0x195C68 */
    /* 0x06 */ u8 cols;        /* sheet columns */
    /* 0x07 */ u8 rows;        /* sheet rows */
    /* 0x08 */ f32 frameTime;  /* seconds per sheet frame */
    /* 0x0C */ f32 fadeIn;     /* seconds */
    /* 0x10 */ f32 fadeOut;    /* seconds */
    /* 0x14 */ f32 keyTime;    /* seconds of the key animation */
    /* 0x18 */ f32 unk18;
    /* 0x1C */ u8 unk1C[0x6C];
    /* 0x88 */ f32 spinRange;  /* random spin per frame, +- this */
    /* 0x8C */ f32 angleRange; /* random start angle, +- this */
} EftBillDef;

/* Create argument (EftEmitArgA in eft_h.h; 0x40 bytes copied to the head of the work). */
typedef struct EftBillArg {
    /* 0x00 */ EftZVec pos;
    /* 0x10 */ EftZVec dir;
    /* 0x20 */ s32 chr;        /* owner object id */
    /* 0x24 */ s32 unk24;
    /* 0x28 */ f32 life;       /* seconds; <= 0: lives until stopped */
    /* 0x2C */ f32 size;       /* scale of width and height */
    /* 0x30 */ void *res;
    /* 0x34 */ EftBillDef *def;
    /* 0x38 */ void *unk38;
} EftBillArg; /* size 0x40 */

/* Current key values, written by 0x195130 / 0x1952E8. */
typedef struct EftBillKey {
    /* 0x00 */ Vec4 color;     /* r, g, b, a (0..128) */
    /* 0x10 */ f32 pulseCol[3];    /* colour pulse: base factor per channel */
    /* 0x1C */ f32 pulseColAmp[3]; /* and amplitude */
    /* 0x28 */ f32 pulseColTime;   /* frames per half period; 0: off */
    /* 0x2C */ f32 spin;       /* radians per frame */
    /* 0x30 */ f32 w;
    /* 0x34 */ f32 h;
    /* 0x38 */ f32 pulseW;
    /* 0x3C */ f32 pulseWAmp;
    /* 0x40 */ f32 pulseWTime;
    /* 0x44 */ f32 pulseH;
    /* 0x48 */ f32 pulseHAmp;
    /* 0x4C */ f32 pulseHTime;
} EftBillKey; /* size 0x50 */

#define EFT_BILL_ALIVE     0x1
#define EFT_BILL_ENDING    0x2   /* stopped or out of life: fades out */
#define EFT_BILL_DEAD      0x4   /* the task kills itself */
#define EFT_BILL_VISIBLE   0x8
#define EFT_BILL_NO_LIFE   0x10  /* no life timer */
#define EFT_BILL_FRONT     0x20  /* only when the owner is in view; drawn in front */
#define EFT_BILL_FADED_IN  0x40
#define EFT_BILL_FADED_OUT 0x80
#define EFT_BILL_W_DOWN    0x100 /* pulse directions */
#define EFT_BILL_H_DOWN    0x200
#define EFT_BILL_COL_DOWN  0x400
#define EFT_BILL_UNK800    0x800

typedef struct EftBill {
    /* 0x000 */ EftBillArg arg;
    /* 0x040 */ EftBillKey key;
    /* 0x090 */ Vec4 color;    /* what is drawn */
    /* 0x0A0 */ f32 u0;
    /* 0x0A4 */ f32 u1;
    /* 0x0A8 */ f32 v0;
    /* 0x0AC */ f32 v1;
    /* 0x0B0 */ s32 flags;     /* EFT_BILL_* */
    /* 0x0B4 */ u8 type;       /* effect type, for BtlScene_IsEffectStopped / Hidden */
    /* 0x0B5 */ u8 unkB5[2];
    /* 0x0B7 */ u8 frame;      /* sheet frame */
    /* 0x0B8 */ s32 frameCount;
    /* 0x0BC */ f32 du;
    /* 0x0C0 */ f32 dv;
    /* 0x0C4 */ f32 frameTimer;
    /* 0x0C8 */ f32 frameTime; /* frames */
    /* 0x0CC */ f32 life;      /* frames left */
    /* 0x0D0 */ f32 delay;     /* frames before the sprite starts */
    /* 0x0D4 */ f32 endDelay;  /* frames between the stop and the fade-out */
    /* 0x0D8 */ f32 unkD8;
    /* 0x0DC */ f32 keyTimer;
    /* 0x0E0 */ f32 keyTime;   /* frames */
    /* 0x0E4 */ f32 unkE4;
    /* 0x0E8 */ f32 angle;
    /* 0x0EC */ f32 spin;
    /* 0x0F0 */ f32 w;
    /* 0x0F4 */ f32 h;
    /* 0x0F8 */ f32 pulseWTimer;
    /* 0x0FC */ f32 pulseHTimer;
    /* 0x100 */ f32 pulseColTimer;
    /* 0x104 */ f32 fadeIn;    /* frames done */
    /* 0x108 */ f32 fadeOut;   /* frames left */
    /* 0x10C */ f32 fadeInLen;
    /* 0x110 */ f32 fadeOutLen;
    /* 0x114 */ u8 unk114[0x24];
    /* 0x138 */ u64 tex;       /* TEX0, set by 0x195038 */
} EftBill; /* size 0x140 */

/* ---- ground dust ---------------------------------------------------------------------------------------- */

/* A byte colour, copied as one unit. */
typedef struct EftZCol {
    u8 c[4];
} EftZCol;

/* Create argument and emitter parameters (0x64 bytes padded to 0x70). */
typedef struct EftGndDustArg {
    /* 0x00 */ EftZVec pos;
    /* 0x10 */ EftZVec dir;    /* direction new particles fly along */
    /* 0x20 */ EftZVec accel;  /* direction of the constant acceleration */
    /* 0x30 */ u8 colA[4];     /* colour at birth */
    /* 0x34 */ u8 colB[4];     /* colour at the end of life */
    /* 0x38 */ s8 chr;         /* owner object id, -1 none */
    /* 0x39 */ s8 pool;        /* particle pool 0 / 1 */
    /* 0x3A */ s8 blend;
    /* 0x3B */ s8 tex;         /* texture index */
    /* 0x3C */ s16 size;       /* sprite size */
    /* 0x3E */ s16 life;       /* frames */
    /* 0x40 */ s16 fade;       /* frames of fade-out after life */
    /* 0x42 */ s16 spin;       /* tenths of a degree per frame */
    /* 0x44 */ s16 rMin;       /* spawn distance from pos */
    /* 0x46 */ s16 rMax;
    /* 0x48 */ f32 grow;       /* size growth per frame */
    /* 0x4C */ f32 bright;     /* factor on r, g, b */
    /* 0x50 */ f32 speed;
    /* 0x54 */ f32 unk54;      /* random speed range, also added to the acceleration */
    /* 0x58 */ f32 drag;       /* factor on the velocity per frame */
    /* 0x5C */ f32 unk5C;      /* acceleration per unit of scale */
    /* 0x60 */ f32 scale;
} EftGndDustArg; /* size 0x70 */

#define EFT_GDUST_PART_ALIVE   0x2
#define EFT_GDUST_PART_FADING  0x4
#define EFT_GDUST_PART_DONE    0x8
#define EFT_GDUST_PART_FOLLOW  0x10 /* base follows the owner's position */
#define EFT_GDUST_PART_TURN    0x20 /* direction follows the owner's */
#define EFT_GDUST_PART_FAR     0x40 /* drawn with the second texture */

typedef struct EftGndDustPart {
    /* 0x00 */ ListNode link;
    /* 0x08 */ u8 unk8[8];
    /* 0x10 */ Vec4 pos;       /* relative to base */
    /* 0x20 */ Vec4 base;
    /* 0x30 */ Vec4 dir;
    /* 0x40 */ Vec4 color;
    /* 0x50 */ Vec4 vel;
    /* 0x60 */ Vec4 size;      /* x, y factors on `scale` */
    /* 0x70 */ Vec4 grow;
    /* 0x80 */ Vec4 accel;
    /* 0x90 */ u8 colA[4];
    /* 0x94 */ u8 colB[4];
    /* 0x98 */ s16 life;
    /* 0x9A */ s16 lifeMax;
    /* 0x9C */ s16 fadeLen;
    /* 0x9E */ s16 fade;
    /* 0xA0 */ s16 scale;
    /* 0xA2 */ s16 unkA2;
    /* 0xA4 */ f32 angle;
    /* 0xA8 */ f32 spin;       /* degrees per frame, as tenths: see EftGndDust_UpdateParts */
    /* 0xAC */ f32 speed;
    /* 0xB0 */ f32 drag;
    /* 0xB4 */ f32 alpha;      /* 1 while alive, then fade / fadeLen */
    /* 0xB8 */ f32 growDamp;
    /* 0xBC */ s32 flags;      /* EFT_GDUST_PART_* */
} EftGndDustPart; /* size 0xC0 */

#define EFT_GDUST_EMIT       0x2  /* an emitter task spawns particles */
#define EFT_GDUST_FORCE_DROP 0x4  /* particles are left behind even when the owner does not turn */
#define EFT_GDUST_DROP       0x8  /* particles are detached from the owner this frame */

/* Work of every dust task. */
typedef struct EftGndDustEmit {
    /* 0x00 */ EftGndDustArg arg;
    /* 0x70 */ s32 tick;
    /* 0x74 */ u8 unk74[0xC];
    /* 0x80 */ Vec4 move;      /* owner's frame move, normalised */
    /* 0x90 */ Vec4 pos;       /* owner's position */
    /* 0xA0 */ Vec4 dir;       /* against the owner's direction */
    /* 0xB0 */ s32 partFlags;  /* tested for EFT_GDUST_PART_FOLLOW when dropping */
    /* 0xB4 */ s32 flags;      /* EFT_GDUST_* */
    /* 0xB8 */ u64 tex;
    /* 0xC0 */ u64 texFar;
    /* 0xC8 */ List *free;     /* pool the particles come from */
    /* 0xCC */ List parts;
    /* 0xD8 */ u8 unkD8[8];
} EftGndDustEmit; /* size 0xE0 */

/* Animated texture loaded by 0x1AE2A8. */
typedef struct EftGndDustTex {
    /* 0x00 */ u8 unk0[0x84];
    /* 0x84 */ s32 unk84;      /* cleared every frame by the manager */
} EftGndDustTex; /* size 0x88 */

/* gEftGndDust: 0x6340 bytes from the effect pool. */
typedef struct EftGndDustWork {
    /* 0x0000 */ EftGndDustPart partsA[64];
    /* 0x3000 */ EftGndDustPart partsB[64];  /* only 63 are linked */
    /* 0x6000 */ List free[2];
    /* 0x6018 */ EftGndDustPart *pool[2];
    /* 0x6020 */ u8 unk6020[0x208];
    /* 0x6228 */ EftGndDustTex tex;
    /* 0x62B0 */ EftGndDustTex *texPtr;
    /* 0x62B4 */ s32 count;               /* live particles */
    /* 0x62B8 */ u8 unk62B8[8];
    /* 0x62C0 */ EftGndDustArg tmpl;         /* the stage's debris dust */
    /* 0x6330 */ EftZTask *slide[2];      /* per fighter object id */
    /* 0x6338 */ EftZTask *dash[2];
} EftGndDustWork; /* size 0x6340 */

/* Stage dust record (BtlStage_GetList68). */
typedef struct EftGndDustStage {
    /* 0x00 */ s32 enabled;
    /* 0x04 */ u8 colA[4];
    /* 0x08 */ u8 colB[4];
    /* 0x0C */ f32 speed;
    /* 0x10 */ f32 unk54;
    /* 0x14 */ u16 size;
    /* 0x16 */ u8 rMin;
    /* 0x17 */ u8 rMax;
} EftGndDustStage;

extern s32 *gEftBillUnused;
extern void *gEftBillList;
extern void *gEftGndDustList;
extern EftGndDustWork *gEftGndDust;

/* part kind 16 */
void EftBill_Init(EftZTask *task, EftBillArg *arg);
void EftBill_Term(EftZTask *task);
void EftBill_Update(EftZTask *task);
void EftBill_PostUpdate(EftZTask *task);
void EftBill_Reset(EftZTask *task);
void EftBill_Draw(EftZTask *task);
void EftBillMgr_Init(EftZTask *task);
void EftBillMgr_Term(EftZTask *task);
void EftBillMgr_Update(EftZTask *task);
void EftBillMgr_Reset(EftZTask *task);
EftZTask *EftBill_Create(EftBillArg *arg);
s32 EftBill_Stop(EftZTask *task);
s32 EftBill_Kill(EftZTask *task);
s32 EftBill_IsAlive(EftZTask *task);
s32 EftBill_SetPos(EftZTask *task, Vec4 *pos);
s32 EftBill_SetDir(EftZTask *task, Vec4 *dir);
s32 EftBill_SetSize(EftZTask *task, f32 size);
s32 EftBill_SetDelay(EftZTask *task, f32 frames);
s32 EftBill_SetEndDelay(EftZTask *task, f32 frames);
s32 EftBill_SetUnkD8(EftZTask *task, f32 v);
s32 EftBill_SetTexture(EftZTask *task, s32 a1, s32 a2, s32 a3);
s32 EftBill_SetFront(EftZTask *task);
s32 EftBill_SetType(EftZTask *task, s32 type);

/* ground dust */
EftZTask *EftGndDust_Create(EftGndDustArg *arg, s32 kind);
void EftGndDust_SpawnDebris(Vec4 *pos, f32 scale, f32 unused, f32 bright);
void EftGndDust_SetSlide(s32 objId, s32 stop, f32 bright);
void EftGndDust_SetDash(s32 objId, s32 stop, f32 bright);
void EftGndDust_SpawnBurst(s32 objId, f32 scale, f32 bright);
void EftGndDust_SpawnLanding(s32 objId, Vec4 *pos, Vec4 *normal, f32 bright);
void EftGndDust_SpawnLandingScaled(s32 objId, Vec4 *pos, Vec4 *dir, f32 bright, f32 scale);
void EftGndDust_Stub(s32 objId, f32 scale);
void EftGndDust_SpawnImpact(s32 objId, Vec4 *pos, f32 bright);
EftGndDustArg *EftGndDust_GetTemplate(void);
void EftGndDust_InitTemplate(void);
void EftGndDust_UpdateParts(EftGndDustEmit *w);
void EftGndDust_LerpColor(Vec4 *out, u8 *a, u8 *b, f32 t);
List *EftGndDust_GetFreeList(s32 pool);
EftGndDustPart *EftGndDust_AllocPart(List *list, List *free);

/* ---- shared by eft_z.c, eft_z_b.c and eft_z_c.c only (they define EFT_Z_IMPL) ------------------------------ */
#ifdef EFT_Z_IMPL

#define RAND_MAX_F 2147483647.0f
#define RANDF() ((f32)rand() / RAND_MAX_F)

#define V(p) ((Vec4 *)(p))

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftZView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 eye;
} EftZView;

extern EftZView *gBtlCamView;

extern void *D_002C40F0[]; /* EftBill item class */
extern void *D_002C4120[]; /* dust classes, kinds 0..5 */
extern void *D_002C4138[];
extern void *D_002C4150[];
extern void *D_002C4168[];
extern void *D_002C4180[];
extern void *D_002C4198[];

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 Rand_Float01(void);
extern f32 Mathf_SinFast(f32 angle);
extern f32 Mathf_CosFast(f32 angle);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);            /* copies x, y, z */
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Lerp(Vec4 *out, Vec4 *up, Vec4 *dir, f32 angle);
extern f32 Vec3_LengthSq(Vec4 *v);                          /* squared length */
extern void Vu0Cur_Push(void);                            /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                        /* load the matrix */
extern void Vu0Cur_Pop(void);                            /* pop */
extern f32 EftMath_WrapAngle(f32 angle);
/* tex0 is declared last here: the callers load it after the floats (eft_b.c declares it fifth; the registers are
   the same either way). */
extern void EftGfx_DrawSprite(Vec4 *pos, Vec4 *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 blend, s32 front, u64 tex0);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftPrim_DrawQuadDepth(Vec4 *pos, Vec4 *color, s32 blend, s32 front, u64 tex0, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot);
#define EftPrim_DrawQuadDepth(pos, color, blend, w, h, u0, v0, u1, v1, rot, front, tex0) EftPrim_DrawQuadDepth(pos, color, blend, front, tex0, w, h, u0, v0, u1, v1, rot)
#else
extern void EftPrim_DrawQuadDepth(Vec4 *pos, Vec4 *color, s32 blend, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 front, u64 tex0);
#endif
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void *BtlTask_CreateChildList(EftZTask *task, s32 count, s32 workSize);
extern EftZTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftZTask *task);                  /* kills the task */
extern void EftTexSet_Load8(EftGndDustTex *tex, s32 *data);      /* loads one animated texture */
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern s32 BtlScene_IsCharInView(s32 objId);
extern s32 BtlScene_IsTimeStopped(void);
extern f32 BtlScene_GetCharScale(s32 chr);
extern void BtlCharApi_GetPos(s32 objId, Vec4 *out);
extern void BtlCharApi_GetDir(s32 objId, Vec4 *out);
extern void BtlCharApi_GetFrameMove(s32 objId, Vec4 *out);
extern EftGndDustStage *BtlStage_GetList68(void);

/* The sprite module's functions before this file. */
extern void EftLine_SetTex(EftBill *w, void *res, s32 a2, s32 a3);  /* sets w->tex */
extern void EftLine_LoadTex(EftBill *w, EftBill *w2);                /* end-of-update step */
extern void EftLine_SetKey(EftBillKey *key, EftBill *w, s32 mode);  /* loads key values */
extern void EftLine_Animate(EftBill *w);                             /* steps the key animation */
extern void EftLine_DrawSprite(EftZTask *task);                         /* shape 1 draw */
extern void EftLine_DrawClipped(EftZTask *task);                         /* shape 1 draw, depth sorted */

/* The dust module's functions after this file. */
extern EftGndDustPart *EftGndDust_MoveNode(List *list, List *free, EftGndDustPart *part); /* frees a particle, returns the next */
extern u64 EftGndDust_GetTex(EftGndDustTex *tex, s32 idx);                        /* TEX0 of a texture */
extern void EftGndDust_GetLightColors(u8 *colA, u8 *colB);                             /* default colours */
extern EftGndDustPart *EftGndDust_SpawnPiece(EftGndDustEmit *w, EftGndDustArg *arg, s32 flags, f32 a, f32 angle); /* returns the
   piece (eft_aa.c); the return type matters: EftGndDustSlide_Init matches only with a non-void callee */
extern EftGndDustPart *EftGndDust_SpawnPieceEx(EftGndDustEmit *w, Vec4 *pos, Vec4 *dir, u8 *colA, u8 *colB, f32 f12, f32 f13, f32 f14,
                          f32 f15, f32 f16, f32 f17, f32 spin, f32 f19, s16 life, s16 fade, s32 size, f32 s0, f32 grow,
                          s32 flags);
extern void EftGndDust_SpawnBodyDust(EftGndDustEmit *w, EftGndDustArg *arg, f32 a, f32 b);
extern void EftGndDust_SpawnChip(EftGndDustEmit *w, EftGndDustArg *arg, Vec4 *dir, u8 *colA, u8 *colB, s32 a5, s32 a6);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftGndDust_DrawPiece(Vec4 *pos, Vec4 *color, Vec4 *dir, s32 blend, u64 tex0, s32 far, f32 w, f32 h, f32 rot);
#define EftGndDust_DrawPiece(pos, color, dir, blend, w, h, rot, tex0, far) EftGndDust_DrawPiece(pos, color, dir, blend, tex0, far, w, h, rot)
#else
extern void EftGndDust_DrawPiece(Vec4 *pos, Vec4 *color, Vec4 *dir, s32 blend, f32 w, f32 h, f32 rot, u64 tex0, s32 far);
#endif

/* Returns every particle of a task to its pool. */
static inline void EftGndDust_FreeParts(EftGndDustEmit *w) {
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);

    while (p != NULL) {
        p->flags &= ~EFT_GDUST_PART_ALIVE;
        p = EftGndDust_MoveNode(&w->parts, w->free, p);
        gEftGndDust->count--;
    }
}

#endif /* EFT_Z_IMPL */

#endif
