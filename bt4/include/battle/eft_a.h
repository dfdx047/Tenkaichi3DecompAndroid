#ifndef BATTLE_EFT_A_H
#define BATTLE_EFT_A_H

#include "types.h"

/*
 * Effect core, src/battle/eft_a.c = 0x12DD80..0x132290: the first code after the effect scene manager
 * (btl_scene.c). Four groups, in address order:
 *
 *   EftHit_*       0x12DD80..0x12F550  the hit record list (gEftHitList, gp 0x2FE9A8) and the per-frame arena
 *                                      the records' shapes are taken from (gEftHitArena, gp 0x2FE9AC).
 *                                      THIS IS SIMULATION: the records are what can hit a fighter.
 *   EftCam_*       0x12F550..0x12F810  camera cut requested by an effect (gEftCam, gp 0x2FE9B0): drives the
 *                                      demo camera (DemoCam_*) in step with a fighter's technique timer.
 *   EftGfx_* / EftMath_* / EftAim_*
 *                  0x12F810..0x132290  helpers shared by the effect modules: palette lighting, splines,
 *                                      angle wrap, aim direction / homing turn of a projectile (simulation),
 *                                      view-frustum planes, clipped polygon and screen sprite drawing.
 *
 * Object model (verified by the matching C unless marked):
 *   - A hit record (EftHitRec, 0x190 bytes) lives for ONE frame. The list holds 64 and a count; it is emptied
 *     by EftHit_BeginFrame at the start of BtlScene_Update unless time is stopped (then last frame's records
 *     stay), and refilled by the update callbacks of the projectile tasks. A task builds a record in the
 *     free slot returned by EftHit_GetNew (zeroed, not yet counted), fills pos / prevPos / vel, its task
 *     pointer, its source and a shape, then calls EftHit_Add, which derives the header fields from the
 *     source and appends the record (it copies the record onto its own slot, then counts it).
 *   - A record points back at the task that owns it (EftHitRec.task, a view of the 0x40-byte BtlTask). The
 *     hit detection (0x1AF740..0x1B10F0, not in this file) reports what happened by setting bits in
 *     task->flags (EftHit_SetTaskFlag) and bumping the task's hit counters; the task reads the summary
 *     EftHit_UpdateResults leaves in task->result on the next frame and reacts (dies, explodes, goes on).
 *   - A record is either a technique of a fighter (type 1: EftHitRec.src, with its definition src->def) or a
 *     ki blast (type 0: EftHitRec.atk).
 */

/* 16-byte aligned vector: the record is copied with 64-bit moves and its list is padded to 16. A union with an
   array view: EftMath_Lerp and EftHit_SpawnBlastImpact only match with this shape (a plain struct of four floats
   changes how the compiler may reorder loads and stores), so the original vector type very likely had one. */
typedef union EftVec {
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftVec;

/* EftHitRec.type */
#define EFT_HIT_BLAST 0 /* EftHitRec.atk is set */
#define EFT_HIT_TECH  1 /* EftHitRec.src is set */

/* EftHitTask.flags: what happened to the record(s) of this task. The bits are set by the hit detection through
   EftHit_SetTaskFlag; only what this file tests or writes is listed. */
#define EFT_TASK_HIT_CHAR    0x0001 /* hit a fighter (result 4, or 5 once a multi-hit ran out) */
#define EFT_TASK_GUARDED     0x0002 /* guarded: result 5 unless multi-hit */
#define EFT_TASK_HIT_STAGE   0x0004 /* hit the stage: result 5; blocks further hits of a blast; picks the weak impact effect */
#define EFT_TASK_LOST_CLASH  0x0008 /* lost a clash against another record: result 4 */
#define EFT_TASK_OUT         0x0010 /* left the stage (EftHit_ClampToStage) or other end: result 5 */
#define EFT_TASK_ABSORBED    0x0020 /* absorbed by the fighter: result 5, no impact effect */
#define EFT_TASK_DEFLECTED   0x0040 /* deflected by the fighter; the record cannot hit any more */
#define EFT_TASK_REFLECTED   0x0080 /* reflected by the fighter (set by the hit detection, eft_det_a.c) */
#define EFT_TASK_STRUGGLE    0x0100 /* beam struggle: result 3 */
#define EFT_TASK_IMPACT_DONE 0x0200 /* the impact effect was spawned (EftHit_SpawnImpacts) */
#define EFT_TASK_MULTI       0x4000 /* multi-hit technique (definition maxHits > 0) */
#define EFT_TASK_MULTI_CONTACT 0x8000 /* a multi-hit is still in contact; one-shot: result 1, cleared when read */
#define EFT_TASK_KEEP        0x10000 /* a multi-hit with definition flag 0x20 hit: stays, cannot hit in mode 0 */
#define EFT_TASK_NO_MODE1    0x20000
#define EFT_TASK_NO_MODE2    0x40000

/* EftHitRec.flags: only what this file tests or writes. */
#define EFT_HIT_FLAG_NO_IMPACT 0x002 /* no impact effect (set by BtlScene_CheckStageChange) */
#define EFT_HIT_FLAG_8         0x008 /* definition kind 9; also set by BtlScene_CheckStageChange */
#define EFT_HIT_FLAG_LAST      0x010 /* last hit of the technique (EftHit_MarkLastHit) */
#define EFT_HIT_FLAG_ALWAYS    0x020 /* EftHit_CanHit mode 0 always passes */
#define EFT_HIT_FLAG_NEVER     0x040 /* EftHit_CanHit mode 0 never passes */
#define EFT_HIT_FLAG_NO_MODE1  0x080
#define EFT_HIT_FLAG_GROUND    0x400 /* blast impact: use the ground effect (0x1975A8) */
#define EFT_HIT_FLAG_NO_LAST   0x800 /* EftHit_MarkLastHit does nothing */

/* The effect task that owns a record: a view of the head of the 0x40-byte BtlTask (0x1AD150..). */
typedef struct EftHitTask {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 flags;    /* EFT_TASK_*; 0x1ADB30(task, bits) ors into it */
    /* 0x08 */ s16 result;   /* EftHit_CalcResult, written every frame by EftHit_UpdateResults */
    /* 0x0A */ s8 countA;    /* EftHit_IncCountA / SetCountA / GetCountA */
    /* 0x0B */ s8 hitCount;  /* EftHit_IncHitCount / GetHitCount: hits landed by a multi-hit technique */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ EftVec pos;   /* where it hit (0x1ADB40 stores it); the impact effect is placed here */
    /* 0x20 */ u8 unk20[0x38 - 0x20];
    /* 0x38 */ void *work;   /* the task's work block (the projectile's state; read by the tasks, not by this file) */
} EftHitTask;

/* Definition of a technique's hit (EftHitSrc.def). */
typedef struct EftHitDef {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 level;    /* copied to EftHitRec.level */
    /* 0x04 */ s8 cls;       /* 0: "rush" class (EftHit_IsTechClass(rec, 0)) */
    /* 0x05 */ s8 kind;      /* 0..2, 4, 7, 8, 9 tested */
    /* 0x06 */ s8 unk6;      /* copied to EftHitRec.unk4 */
    /* 0x07 */ s8 unk7;
    /* 0x08 */ s8 shape;     /* 0: spheres, 1: boxes (read by the tasks that build the record) */
    /* 0x09 */ s8 unk9;      /* < 2 for a technique that can clash */
    /* 0x0A */ s8 maxHits;   /* > 0: multi-hit */
    /* 0x0B */ s8 unkB;
    /* 0x0C */ s8 aimMode;   /* EftAim_GetDirKeep: 0 = aim every call, else aim once and keep */
    /* 0x0D */ u8 unkD[0x34 - 0xD];
    /* 0x34 */ f32 unk34;    /* copied to EftHitRec.shape.unk8 */
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 flags;    /* 0x2 -> EftHitRec.unk54; 0x20; 0x100 cannot clash; 0x200000 */
    /* 0x40 */ s16 impactFx; /* effect id of the impact on a fighter, < 0 none */
    /* 0x42 */ s16 groundFx; /* effect id of the impact on the stage, < 0 none */
    /* 0x44 */ u8 unk44[0x50 - 0x44];
    /* 0x50 */ f32 impactScale;
    /* 0x54 */ f32 groundScale;
    /* 0x58 */ u8 unk58[0x84 - 0x58];
    /* 0x84 */ f32 scale;    /* EftHit_GetScale */
} EftHitDef;

/* Who launched a technique record (the BtlCollOwner of btl_char_coll.h). */
typedef struct EftHitSrc {
    /* 0x00 */ s32 objId;
    /* 0x04 */ s32 slot;
    /* 0x08 */ s32 unk8;     /* copied to EftHitRec.unkF0 */
    /* 0x0C */ u8 unkC[0x24 - 0xC];
    /* 0x24 */ EftHitDef *def;
    /* 0x28 */ u8 unk28[0x30 - 0x28];
    /* 0x30 */ EftVec aimDir; /* EftAim_GetDirKeep: the direction aimed once */
    /* 0x40 */ s32 aimFlags; /* bit 2: aimDir is valid */
} EftHitSrc;

/* Parameters of a ki blast (the BtlCollAtk of btl_char_coll.h). */
typedef struct EftHitAtk {
    /* 0x00 */ u8 unk0[0x12];
    /* 0x12 */ s16 objId;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ s16 unk16;    /* copied to EftHitRec.unkF0 */
    /* 0x18 */ s16 kind;
    /* 0x1A */ u8 unk1A;     /* 1: second impact variant */
    /* 0x1B */ u8 unk1B;     /* copied to EftHitRec.unk4 */
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ f32 unk20;    /* copied to EftHitRec.shape.unk8 */
    /* 0x24 */ u8 unk24[0x30 - 0x24];
    /* 0x30 */ s32 level;    /* copied to EftHitRec.level */
} EftHitAtk;

/* Shape a record sweeps this frame. a and b point into gEftHitArena: the volume at the previous and at the
   current position (0x20-byte spheres built by 0x2399A0 for type 0, 0x30-byte boxes built by 0x239588 for
   type 1). */
typedef struct EftHitShape {
    /* 0x00 */ s32 type;     /* 0..6, see EftHit_SetShape* */
    /* 0x04 */ f32 scale;    /* EftHit_GetScale(rec): 0.1 for a blast, def->scale for a technique */
    /* 0x08 */ f32 unk8;
    /* 0x0C */ void *a;
    /* 0x10 */ void *b;
} EftHitShape;

/* Sphere of a type 0 shape (0x20 bytes of the arena). */
typedef struct EftHitSphere {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ f32 radius;
} EftHitSphere;

/* Box / capsule of a type 1 shape (0x30 bytes of the arena). */
typedef struct EftHitBox {
    /* 0x00 */ EftVec unk0;
    /* 0x10 */ EftVec pos;
    /* 0x20 */ f32 radius;
} EftHitBox;

/* One hit record. */
typedef struct EftHitRec {
    /* 0x000 */ s32 objId;    /* object id of the fighter it belongs to */
    /* 0x004 */ s32 unk4;     /* def->unk6 or atk->unk1B */
    /* 0x008 */ s32 level;    /* def->level or atk->level: the stronger record wins a clash */
    /* 0x00C */ s32 type;     /* EFT_HIT_* */
    /* 0x010 */ EftVec unk10; /* filled by the task (first of the four vectors it copies) */
    /* 0x020 */ EftVec pos;
    /* 0x030 */ EftVec prevPos;
    /* 0x040 */ EftVec vel;
    /* 0x050 */ s32 flags;    /* EFT_HIT_FLAG_* */
    /* 0x054 */ s32 unk54;    /* technique: bit 1 of def->flags */
    /* 0x058 */ s32 unk58[2];
    /* 0x060 */ EftHitTask *task;
    /* 0x064 */ EftHitSrc *src;
    /* 0x068 */ EftHitAtk *atk;
    /* 0x06C */ s32 unk6C;
    /* 0x070 */ EftHitShape shape;
    /* 0x084 */ u8 unk84[0xF0 - 0x84];
    /* 0x0F0 */ s32 unkF0;    /* src->unk8 or atk->unk16 */
    /* 0x0F4 */ u8 unkF4[0x180 - 0xF4];
    /* 0x180 */ s32 seenByAi; /* cleared by EftHit_Add */
    /* 0x184 */ u8 unk184[0x190 - 0x184];
} EftHitRec; /* size 0x190 */

#define EFT_HIT_MAX 64

typedef struct EftHitList {
    /* 0x0000 */ EftHitRec rec[EFT_HIT_MAX];
    /* 0x6400 */ s32 count;
} EftHitList; /* size 0x6410 */

/* Bump allocator for the shapes of this frame's records. */
typedef struct EftHitArena {
    /* 0x0 */ u8 *cur;
    /* 0x4 */ u8 *base;
    /* 0x8 */ s32 size; /* 0x1800, never checked */
} EftHitArena; /* size 0xC */

/* Argument of EftCam_Start. */
typedef struct EftCamArg {
    /* 0x0 */ void *anim;  /* camera animation (DemoCam_SetAnim) */
    /* 0x4 */ s32 objId;   /* fighter whose technique timer drives the cut, or -1 */
    /* 0x8 */ s32 unk8;
} EftCamArg;

/* Camera cut of an effect. */
typedef struct EftCam {
    /* 0x00 */ EftCamArg arg;
    /* 0x0C */ s32 active;
    /* 0x10 */ s32 paused;
    /* 0x14 */ s32 hold;   /* keep the last frame instead of stopping at the end */
    /* 0x18 */ s32 objId;
} EftCam; /* size 0x1C */

/* 4x4 matrix as four aligned rows. */
typedef struct EftMtx {
    /* 0x00 */ EftVec row[4];
} EftMtx; /* size 0x40 */

/* Vertex of the polygons the clipped draw functions take. */
typedef struct EftGfxVert {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ EftVec color;
    /* 0x20 */ EftVec uv;
} EftGfxVert; /* size 0x30 */

extern EftHitList *gEftHitList;
extern EftHitArena *gEftHitArena;
extern EftCam *gEftCam;

void EftHit_Init(void);
void EftHit_Term(void);
EftHitRec *EftHit_GetNew(void);
void EftHit_Add(EftHitRec *rec);
void EftHit_BeginFrame(void);
void EftHit_Clear(void);
EftHitList *EftHit_GetList(void);
s32 EftHit_GetTaskFlags(u32 idx);
void EftHit_SetTaskFlag(u32 idx, s32 bits, EftVec pos);
void EftHit_SpawnImpacts(void);
s32 EftHit_CanHit(EftHitRec *rec, s32 mode);
s32 EftHit_IsStoppedByHit(EftHitRec *rec);
s32 EftHit_RearmImpact(EftHitRec *rec);
s32 EftHit_ClashTech(EftHitRec *a, EftHitRec *b);
s32 EftHit_Clash(EftHitRec *a, EftHitRec *b);
s32 EftHit_IsMultiHit(EftHitRec *rec);
void EftHit_IncHitCount(EftHitRec *rec);
s32 EftHit_GetHitCount(EftHitRec *rec);
s32 EftHit_GetMaxHits(EftHitRec *rec);
s32 EftHit_GetDefUnkB(EftHitRec *rec);
void EftHit_IncCountA(EftHitRec *rec);
void EftHit_SetCountA(EftHitRec *rec, s32 value);
s32 EftHit_GetCountA(EftHitRec *rec);
s32 EftHit_HasDefFlag20(EftHitRec *rec);
void EftHit_MarkLastHit(EftHitRec *rec);
s32 EftHit_HasDefFlag200000(EftHitRec *rec);
s32 EftHit_NotifyBlastTask(u32 idx, void *mtx);
s32 EftHit_IsRushHit(EftHitRec *rec);
f32 EftHit_GetRadiusA(EftHitRec *rec);
f32 EftHit_GetRadiusB(EftHitRec *rec);
void EftHit_SetRadiusA(EftHitRec *rec, f32 radius);
void EftHit_SetRadiusB(EftHitRec *rec, f32 radius);
void EftHit_ClampToStage(void);
void EftHit_UpdateResults(void);
s32 EftHit_CalcResult(EftHitRec *rec);

void EftHitArena_Init(void);
void EftHitArena_Term(void);
void EftHitArena_Reset(void);
void *EftHitArena_Alloc20A(void);
void *EftHitArena_Alloc20B(void);
void *EftHitArena_Alloc40(void);
EftHitSphere *EftHitArena_AllocSphere(void);
void *EftHitArena_Alloc20C(void);
EftHitRec *EftHitArena_AllocRec(void);
EftHitBox *EftHitArena_AllocBox(void);

void EftHit_SetShape3(EftHitRec *rec, void *a, void *b);
void EftHit_SetShape4(EftHitRec *rec, void *a, void *b);
void EftHit_SetShape5(EftHitRec *rec, void *a, void *b);
void EftHit_SetShapeSpheres(EftHitRec *rec, EftHitSphere *a, EftHitSphere *b);
void EftHit_SetShape6(EftHitRec *rec, void *a, void *b);
void EftHit_SetShape2(EftHitRec *rec, void *a, void *b);
void EftHit_SetShapeBoxes(EftHitRec *rec, EftHitBox *a, EftHitBox *b);
s32 EftHit_IsTechClass(EftHitRec *rec, s32 cls);
f32 EftHit_GetScale(EftHitRec *rec);
void EftHit_InitMultiHit(EftHitRec *rec);
void EftHit_SpawnBlastImpact(EftHitRec *rec);
void EftHit_SpawnTechImpact(EftHitRec *rec);

void EftCam_Init(void);
void EftCam_Term(void);
void EftCam_Clear(void);
void EftCam_Start(EftCamArg *arg);
void EftCam_SetHold(s32 hold);
void EftCam_SetPaused(s32 paused);
s32 EftCam_IsPaused(void);
s32 EftCam_IsActive(void);
void EftCam_Stop(void);
void EftCam_Update(void);

void EftGfx_LightClutDiffuse(u8 *dst, u8 *nrm, EftVec light, u8 r, u8 g, u8 b);
void EftGfx_LightClutSpecular(u8 *dst, u8 *nrm, EftMtx view, EftMtx light, EftVec eye, f32 power);
void EftGfx_LerpClut(u8 *dst, u8 *a, u8 *b, f32 t);
void EftMath_CalcTangentFrame(EftMtx *out, EftVec *pos, EftVec *uv);
s32 EftMath_MtxFromDir(EftMtx *out, EftVec *dir, f32 roll);
f32 EftMath_WrapAngle(f32 angle);
void EftMath_CatmullRom(EftVec out, EftVec *p, f32 t);
void EftMath_Spline3(EftVec *out, EftVec *p, f32 t);
void EftMath_Spline3B(EftVec out, EftVec *p, f32 t);
void EftMath_Bezier(EftVec out, EftVec *p, f32 t);
void EftMath_Lerp(EftVec out, EftVec *p, f32 t);
void EftMath_TcbSpline(EftVec out, EftVec *p, f32 t, f32 tension, f32 bias, f32 continuity);
EftVec *EftGfx_GetClipPlanes(void);
void EftGfx_UpdateClipPlanes(void);
s32 EftAim_GetDir(EftVec *out, s32 arg, s32 objId);
s32 EftAim_GetDirKeep(EftHitSrc *src, EftVec *out, s32 arg, s32 objId);
void EftMath_RotateAboutAxis(EftVec *out, EftVec *v, EftVec *axis, f32 angle);
void EftAim_Home(EftVec *out, EftVec *pos, EftVec *dir, s32 objId, f32 speed, f32 maxTurn);
void EftGfx_DrawPolyAvgZ(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 flip, u64 tex, s32 zOfs);
void EftGfx_DrawPolyFixedZ(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex, s32 z);
void EftGfx_DrawPolyAvgZFront(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                              s32 zOfs);
void EftGfx_DrawPolyScaledZ(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                            f32 zScale);
void EftGfx_DrawSprite(EftVec *pos, EftVec *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 layer,
                       s32 front, u64 tex0);

#endif
