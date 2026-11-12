#ifndef BATTLE_EFT_AA_H
#define BATTLE_EFT_AA_H

#include "types.h"
#include "sys/math3d.h"
#include "sys/list.h"

/*
 * Effect modules, 0x199F28..0x19E0C0 (src/battle/eft_aa.c). Six small modules, all drawing or sound only:
 *   0x199F28..0x19B7F8  tail of the ground effect module (dust / rock pieces): piece spawners and drawing helpers
 *   0x19B7F8..0x19BA20  delayed sound effects
 *   0x19BA20..0x19BC70  per-fighter effect slots (which fighter has which one-per-fighter effect task)
 *   0x19BC70..0x19D2C8  blade trail (the ribbon behind a drawn weapon)
 *   0x19D2C8..0x19D730  blinding overlay (fighter +0xFFC as a grey full-screen sprite)
 *   0x19D730..0x19DEA8  effect pack part kind 14 (an animated object with delay / hold / fade)
 *   0x19DEA8..0x19E0C0  init callback of the next module's manager
 * All structs are this file's views.
 */

/* A vector passed by value. The effect code's vector type is 16-byte aligned (argument copies use ld / sd). */
typedef struct EftAaVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) EftAaVec;

/* Four floats, 8-byte aligned: locals initialised from a constant table are copied with 64-bit moves. */
typedef struct EftAaVec8 {
    f32 x, y, z, w;
} __attribute__((aligned(8))) EftAaVec8;

/* A GS screen position as the projection helpers (0x121240, 0x1224E0) write it. */
typedef struct EftAaScr {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 unkC;
} EftAaScr;

/* The part of a task (0x1AD150..) the callbacks here use. */
typedef struct EftAaTask {
    /* 0x00 */ u8 flags;       /* bit 0: killed */
    /* 0x01 */ u8 unk1;
    /* 0x02 */ u16 id;         /* serial number; a handle is valid while it still matches */
    /* 0x04 */ u8 unk4[0x24];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftAaTask;

/* GS registers as packed into the packets queued here. */
typedef struct EftAaXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftAaXyzf;

typedef struct EftAaRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftAaRgbaq;

typedef struct EftAaSt {
    f32 s, t;
} EftAaSt;

/* ---- ground effect pieces ------------------------------------------------------------------------------- */

typedef struct EftGroundRgba {
    u8 c[4];
} EftGroundRgba;

/* A dust / rock piece (0xC0 bytes), on the module's free or active list. */
typedef struct EftGroundPiece {
    /* 0x00 */ ListNode node;
    /* 0x08 */ u8 unk8[8];
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 dir;        /* unit direction of travel */
    /* 0x40 */ Vec4 color;      /* 128, 128, 128, 128 */
    /* 0x50 */ Vec4 vel;        /* dir * speed */
    /* 0x60 */ Vec4 size;       /* (size, size, 1, 1) */
    /* 0x70 */ Vec4 grow;       /* (growth, growth, 1, 1) */
    /* 0x80 */ Vec4 accel;      /* gravity direction * strength */
    /* 0x90 */ EftGroundRgba colA; /* the stage's two light colours (EftGndDust_GetLightColors) */
    /* 0x94 */ EftGroundRgba colB;
    /* 0x98 */ s16 life;        /* frames */
    /* 0x9A */ s16 lifeMax;
    /* 0x9C */ s16 fade;
    /* 0x9E */ s16 fadeMax;
    /* 0xA0 */ u16 tex;
    /* 0xA2 */ u8 unkA2[2];
    /* 0xA4 */ f32 rot;         /* roll angle */
    /* 0xA8 */ f32 spin;        /* roll per frame; the sign is random */
    /* 0xAC */ f32 speed;
    /* 0xB0 */ f32 drag;
    /* 0xB4 */ f32 unkB4;       /* 1 */
    /* 0xB8 */ f32 unkB8;       /* 1 */
    /* 0xBC */ s32 flags;       /* 2 = alive, plus the caller's bits */
} EftGroundPiece; /* size 0xC0 */

/* What EftGndDust_SpawnPiece reads: a piece description the callers fill and reuse. */
typedef struct EftGroundDef {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 dir;
    /* 0x20 */ Vec4 grav;       /* gravity direction */
    /* 0x30 */ EftGroundRgba colA;
    /* 0x34 */ EftGroundRgba colB;
    /* 0x38 */ s8 objId;        /* fighter the dust belongs to */
    /* 0x39 */ u8 unk39[3];
    /* 0x3C */ u16 tex;
    /* 0x3E */ s16 life;
    /* 0x40 */ s16 fade;
    /* 0x42 */ s16 spin;        /* tenths of a radian per frame */
    /* 0x44 */ u8 unk44[4];
    /* 0x48 */ f32 grow;
    /* 0x4C */ u8 unk4C[4];
    /* 0x50 */ f32 speed;
    /* 0x54 */ f32 speedRand;
    /* 0x58 */ f32 drag;
    /* 0x5C */ f32 gravity;
    /* 0x60 */ f32 scale;
} EftGroundDef;

typedef struct EftGroundTexEntry {
    /* 0x0 */ u64 tex0;
    /* 0x8 */ u64 unk8;
} EftGroundTexEntry;

/* Texture slots of a piece class: TEX0 values fetched on demand. */
typedef struct EftGroundTex {
    /* 0x00 */ EftGroundTexEntry entry[8];
    /* 0x80 */ s32 unk80;
    /* 0x84 */ s32 loaded;      /* bit per entry */
} EftGroundTex;

/* gEftGndDust: the part of the module's work used here. */
typedef struct EftGroundWork {
    /* 0x0000 */ u8 unk0[0xC8];
    /* 0x00C8 */ List *free;
    /* 0x00CC */ List active;
    /* 0x00D8 */ u8 unkD8[0x62B4 - 0xD8];
    /* 0x62B4 */ s32 count;     /* pieces spawned */
} EftGroundWork;

/* One body segment of EftGndDust_SpawnBodyDust. */
typedef struct EftGroundSeg {
    /* 0x0 */ s32 node;         /* fighter node the segment starts at */
    /* 0x4 */ s32 end;          /* node it ends at, or -1 for a single point */
    /* 0x8 */ s32 skipShort;    /* no piece when the segment is shorter than one step */
} EftGroundSeg;

typedef struct EftGroundQuadPkt {
    /* 0x00 */ u32 dmaTag;      /* 0x20000008 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;        /* 0x10000000 */
    /* 0x0C */ u32 vif1;        /* 0x50000008 */
    /* 0x10 */ u64 gifTag;      /* 0xE400000000008001: REGLIST, 14 registers */
    /* 0x18 */ u64 regs;        /* PRIM, TEX0, 4 x (RGBAQ, ST, XYZF2) */
    /* 0x20 */ u64 prim;        /* 0x5C: gouraud textured blended triangle strip */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ struct {
        EftAaRgbaq rgbaq;
        EftAaSt st;
        EftAaXyzf xyz;
    } v[4];
} EftGroundQuadPkt; /* size 0x90 */

/* ---- delayed sound effects ------------------------------------------------------------------------------ */

typedef struct EftDelaySeDef {
    /* 0x0 */ s32 seId;
    /* 0x4 */ s32 delay;        /* frames */
} EftDelaySeDef;

/* Task work (0xC bytes). */
typedef struct EftDelaySe {
    /* 0x0 */ s32 objId;
    /* 0x4 */ s32 seId;
    /* 0x8 */ s32 delay;
} EftDelaySe;

typedef struct EftDelaySeMgr {
    /* 0x0 */ void *list;       /* five tasks */
} EftDelaySeMgr;

/* ---- per-fighter effect slots --------------------------------------------------------------------------- */

typedef struct EftCharSlot {
    /* 0x0 */ void *task;
    /* 0x4 */ s32 on;
} EftCharSlot;

/* gEftCharSlot (0x50 bytes): one slot per kind and fighter. */
typedef struct EftCharSlots {
    /* 0x00 */ EftCharSlot kind0[2];
    /* 0x10 */ EftCharSlot kind1[2];
    /* 0x20 */ EftCharSlot kind2[2];
    /* 0x30 */ EftCharSlot kind3[2];
    /* 0x40 */ EftCharSlot absorb[2]; /* the drain glow (EftAbsorb, eft_l_c.c) */
} EftCharSlots;

/* ---- blade trail ---------------------------------------------------------------------------------------- */

/* One entry of gEftBladeDef (0x40 bytes): which character has a trail and where its blade is. */
typedef struct EftBladeDef {
    /* 0x00 */ Vec4 base;       /* in the space of fighter node 0x40 */
    /* 0x10 */ Vec4 tip;
    /* 0x20 */ Vec4 color;      /* 0..255 */
    /* 0x30 */ s32 layer;       /* ordering table layer; 2 and 3 are layers 0 and 1 with GS context 2 */
    /* 0x34 */ s32 chara;       /* character id */
    /* 0x38 */ u8 unk38[8];
} EftBladeDef;

/* What EftBlade_Create hands to the task (0x12A0 bytes). */
typedef struct EftBladeTrail {
    /* 0x0000 */ EftAaVec edgeA[128];  /* ring of blade base positions */
    /* 0x0800 */ EftAaVec edgeB[128];  /* ring of blade tip positions */
    /* 0x1000 */ EftAaVec lastA[4];    /* the last four bases, for the spline */
    /* 0x1040 */ EftAaVec lastB[4];
    /* 0x1080 */ s32 alpha[128];       /* 0 = empty; set to 0x4C..0x60 and lowered by 20 per frame */
    /* 0x1280 */ s32 objId;
    /* 0x1284 */ s32 unk1284;          /* 60..89, random; not read here */
    /* 0x1288 */ s32 unk1288;
    /* 0x128C */ s32 tail;             /* oldest live ring entry */
    /* 0x1290 */ s32 head;             /* next ring entry to write */
    /* 0x1294 */ s32 wasOut;           /* the weapon was out last frame */
    /* 0x1298 */ u8 unk1298[8];
} EftBladeTrail; /* size 0x12A0 */

/* Task work (0x12B0 bytes). */
typedef struct EftBlade {
    /* 0x0000 */ EftBladeTrail t;
    /* 0x12A0 */ Vec4 unk12A0;         /* (rand * 20, -10, rand * 20, 1); not read here */
} EftBlade;

/* gEftBlade (0x10 bytes). */
typedef struct EftBladeMgr {
    /* 0x0 */ void *list;       /* two tasks */
    /* 0x4 */ s32 chara[2];     /* the fighters' character ids at creation */
    /* 0xC */ u8 *color;        /* gEftBladeColor: default RGBA */
} EftBladeMgr;

typedef struct EftAaIVec {
    s32 x, y, z, w;
} EftAaIVec;

typedef struct EftBladeTriPkt {
    /* 0x00 */ u32 dmaTag;      /* 0x20000005 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;        /* 0x10000000 */
    /* 0x0C */ u32 vif1;        /* 0x50000005 */
    /* 0x10 */ u64 gifTag;      /* 0x8400000000008001: REGLIST, 8 registers */
    /* 0x18 */ u64 regs;        /* PRIM, 3 x (RGBAQ, XYZF2), NOP */
    /* 0x20 */ u64 prim;        /* gouraud blended triangle, context by layer */
    /* 0x28 */ struct {
        EftAaRgbaq rgbaq;
        EftAaXyzf xyz;
    } v[3];
    /* 0x58 */ u64 nop;
} EftBladeTriPkt; /* size 0x60 */

/* ---- blinding overlay ----------------------------------------------------------------------------------- */

#define EFT_BLIND_ACTIVE 1
#define EFT_BLIND_SHOW0 8      /* alpha[0] > 0 */
#define EFT_BLIND_SHOW1 0x10   /* alpha[1] > 0 */

/* gEftBlind (0xC bytes). */
typedef struct EftBlind {
    /* 0x0 */ f32 alpha[2];     /* 0..255, by battle object id */
    /* 0x8 */ s32 flags;
} EftBlind;

typedef struct EftBlindPkt {
    /* 0x00 */ u32 dmaTag;      /* 0x20000003 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;        /* 0x10000000 */
    /* 0x0C */ u32 vif1;        /* 0x50000003 */
    /* 0x10 */ u64 gifTag;      /* 0x4400000000008001: REGLIST, 4 registers */
    /* 0x18 */ u64 regs;        /* PRIM, RGBAQ, XYZF2, XYZF2 */
    /* 0x20 */ u64 prim;        /* 0x46: blended sprite */
    /* 0x28 */ EftAaRgbaq rgbaq;
    /* 0x30 */ EftAaXyzf xyz0;
    /* 0x38 */ EftAaXyzf xyz1;
} EftBlindPkt; /* size 0x40 */

/* ---- effect pack part kind 14 --------------------------------------------------------------------------- */

#define EFT_ANIMPART_VALID 1     /* cleared when the fade ends */
#define EFT_ANIMPART_STARTED 2   /* the object is being stepped and drawn */
#define EFT_ANIMPART_ENDING 4    /* hold, then fade */
#define EFT_ANIMPART_DEAD 8      /* kill the task at the end of this update */
#define EFT_ANIMPART_TIMED 0x10  /* has a life in frames */

/* The spawner's argument block (EftEmitArg14 in eft_h.h). */
typedef struct EftAnimPartArg {
    /* 0x00 */ s32 chr;         /* owner object id */
    /* 0x04 */ s32 mode;        /* 0..9: picks the draw layer */
    /* 0x08 */ f32 life;        /* seconds; 0 = until stopped */
    /* 0x0C */ u8 unkC[4];
    /* 0x10 */ EftAaVec pos;
    /* 0x20 */ EftAaVec dir;
    /* 0x30 */ f32 size;
    /* 0x34 */ void *tex;
    /* 0x38 */ void *res;
    /* 0x3C */ u8 unk3C[4];
} EftAnimPartArg; /* size 0x40 */

/* Task work (0xB0 bytes). */
typedef struct EftAnimPart {
    /* 0x00 */ u8 obj[0x50];    /* the animated object of the 0x1AA8D0 library */
    /* 0x50 */ EftAnimPartArg arg;
    /* 0x90 */ s32 type;        /* effect kind for BtlScene_IsEffectStopped / Hidden */
    /* 0x94 */ s32 taskId;
    /* 0x98 */ s32 flags;       /* EFT_ANIMPART_* */
    /* 0x9C */ f32 delay;       /* frames before the object starts */
    /* 0xA0 */ f32 hold;        /* frames between the end and the fade */
    /* 0xA4 */ f32 fadeTime;    /* fade length in frames */
    /* 0xA8 */ f32 fade;        /* frames of fade left */
    /* 0xAC */ f32 life;        /* frames left when EFT_ANIMPART_TIMED */
} EftAnimPart; /* size 0xB0 */

/* ---- next module's manager ------------------------------------------------------------------------------ */

typedef struct EftAaOrbTailWork {
    /* 0x0000 */ s32 *unk0;       /* common entry 0x25B */
    /* 0x0004 */ s32 *unk4;       /* common entry 0x25A */
    /* 0x0008 */ u8 res0[0x108];  /* resource set of common entry 0x259 */
    /* 0x0110 */ u8 res[3][0x108];/* resource sets of common entries 0x25C, 0x25E, 0x25E */
    /* 0x0428 */ s32 *unk428[3];  /* common entries 0x25D, 0x25F, 0x260 */
    /* 0x0434 */ s32 *unk434[7];  /* common entries 0x261..0x267 */
    /* 0x0450 */ u8 unk450[0xB958 - 0x450];
    /* 0xB958 */ void *list;      /* two 0x1E0-byte tasks */
    /* 0xB95C */ u8 unkB95C[4];
} EftAaOrbTailWork; /* size 0xB960 */

typedef struct EftAaOrbTailMgr {
    /* 0x0 */ EftAaOrbTailWork *w;
} EftAaOrbTailMgr;

ListNode *EftGndDust_MoveNode(List *from, List *to, ListNode *node);
u64 EftGndDust_GetTex(EftGroundTex *tex, s32 idx);
void EftGndDust_GetLightColors(u8 *colA, u8 *colB);
EftGroundPiece *EftGndDust_SpawnPiece(EftGroundWork *w, EftGroundDef *def, s32 flags, f32 size, f32 rot);
EftGroundPiece *EftGndDust_SpawnPieceEx(EftGroundWork *w, Vec4 *pos, Vec4 *dir, EftGroundRgba *colA, EftGroundRgba *colB,
                                       f32 sizeX, f32 sizeY, f32 growX, f32 growY, f32 speed, f32 drag, f32 rot,
                                       f32 spin, s16 life, s16 fade, s16 tex, f32 unkB8, f32 gravity, s32 flags);
void EftGndDust_SpawnBodyDust(EftGroundWork *w, EftGroundDef *def, f32 scale);
void EftGndDust_SpawnChip(EftGroundWork *w, Vec4 *pos, Vec4 *dir, EftGroundRgba *colA, EftGroundRgba *colB, s32 life,
                         s32 fade);
void EftGndDust_MakeFacingMtx(Mtx44 *out, Vec4 *dir, Vec4 *pos);
s32 EftGndDust_IsOnScreen(EftAaScr *p);
void EftGndDust_DrawQuad(Vec4 *pos, Vec4 *color, Mtx44 *mtx, u64 tex0, f32 scale, f32 z0, f32 z1, f32 width, f32 u0,
                        f32 v0, f32 u1, f32 v1, u8 layer);
void EftGndDust_DrawPiece(Vec4 *pos, Vec4 *color, Vec4 *dir, s32 layer, u64 tex0, s32 along, f32 w, f32 h, f32 rot);

void EftDelaySe_Start(s32 objId, EftDelaySeDef *def, s32 count);
void EftDelaySeMgr_Init(EftAaTask *task);
void EftDelaySeMgr_Term(void);
void EftDelaySeMgr_Update(void);
void EftDelaySe_Init(EftAaTask *task, EftDelaySe *arg);
void EftDelaySe_Term(void);
void EftDelaySe_Update(EftAaTask *task);
void EftDelaySe_Reset(EftAaTask *task);

void EftCharSlotMgr_Init(void);
void EftCharSlotMgr_Term(void);
void EftCharSlotMgr_Update(void);
void EftCharSlotMgr_Reset(void);
void EftCharSlot_Set0(s32 objId, void *task);
void EftCharSlot_Clear0(s32 objId);
void *EftCharSlot_Get0(s32 objId);
void EftCharSlot_Set1(s32 objId, void *task);
void EftCharSlot_Clear1(s32 objId);
void *EftCharSlot_Get1(s32 objId);
void EftCharSlot_Set2(s32 objId, void *task);
void EftCharSlot_Clear2(s32 objId);
void *EftCharSlot_Get2(s32 objId);
void EftCharSlot_Set3(s32 objId, void *task);
void EftCharSlot_Clear3(s32 objId);
void *EftCharSlot_Get3(s32 objId);
void EftCharSlot_SetAbsorb(s32 objId, void *task);
void EftCharSlot_ClearAbsorb(s32 objId);
void *EftCharSlot_GetAbsorb(s32 objId);

void EftBlade_Create(s32 objId);
void EftBladeMgr_Init(EftAaTask *task);
void EftBladeMgr_Term(void);
void EftBladeMgr_Update(void);
void EftBlade_Init(EftAaTask *task, EftBladeTrail *arg);
void EftBlade_Term(void);
void EftBlade_Reset(void);
void EftBlade_AddPoint(EftBladeTrail *t, EftAaVec a, EftAaVec b, s32 objId);
void EftBlade_Update(EftAaTask *task);
void EftBlade_Draw(EftAaTask *task);
void EftBlade_Spline(EftAaVec *out, EftAaVec *p, f32 t);
u8 *EftBlade_GetColorPtr(void);
f32 EftBlade_GetColor(EftAaIVec *out);
EftBladeDef *EftBlade_FindDef(s32 objId);
s32 EftBlade_IsClipped(EftAaScr *p);

void EftBlind_Init(void);
void EftBlind_Update(void);
void EftBlind_Draw(void);
void EftBlind_Term(void);
void EftBlind_DrawOverlay(s32 view, s32 full, f32 alpha);

EftAaTask *EftAnimPart_Create(EftAnimPartArg *arg);
s32 EftAnimPart_Stop(EftAaTask *task);
s32 EftAnimPart_Kill(EftAaTask *task);
s32 EftAnimPart_SetType(EftAaTask *task, s32 type);
s32 EftAnimPart_SetPos(EftAaTask *task, Vec4 *pos);
s32 EftAnimPart_SetSize(EftAaTask *task, f32 size);
s32 EftAnimPart_SetDir(EftAaTask *task, Vec4 *dir);
s32 EftAnimPart_SetDelay(EftAaTask *task, f32 frames);
s32 EftAnimPart_SetHold(EftAaTask *task, f32 frames);
s32 EftAnimPart_SetFade(EftAaTask *task, f32 frames);
s32 EftAnimPart_IsAlive(EftAaTask *task);
void EftAnimPartMgr_Init(EftAaTask *task);
void EftAnimPartMgr_Term(void);
void EftAnimPartMgr_Update(void);
void EftAnimPart_Init(EftAaTask *task, EftAnimPartArg *arg);
void EftAnimPart_Term(EftAaTask *task);
void EftAnimPart_Reset(EftAaTask *task);
void EftAnimPart_Update(EftAaTask *task);
void EftAnimPart_Draw(EftAaTask *task);
s32 EftAnimPart_IsValid(EftAaTask *task);

void EftOrbTailMgr_Init(EftAaTask *task);

#endif
