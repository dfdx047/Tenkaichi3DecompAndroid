#include "common.h"
#include "battle/battle.h"
#include "battle/btl_pool.h"
#include "battle/eft_a.h"
#include "sys/gfx_ot.h"

#ifdef PORT
#include "battle/tag_team.h"
#include "battle/btl_seq.h"
static EftCam fighterCams[PORT_FIGHTER_CAPACITY];
extern void *DemoCam_EnterFighter(s32);
extern void DemoCam_LeaveFighter(void *);
extern f32 Port_WideFactor(void); /* widescreen on PC: 1, or 4/3 for a 16:9 picture (port/src/plat_stub.c) */
#endif

/*
 * Effect core, 0x12DD80..0x132290. The object model is described in include/battle/eft_a.h.
 *
 * Callees outside this file are declared here with local views (an integrator is linking other files).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);

extern s32 BtlScene_IsTimeStopped(void);

extern void Vec4_Copy(EftVec *dst, EftVec *src);
extern void Vec4_Sub(EftVec *dst, EftVec *a, EftVec *b);
extern f32 Vec3_Dot(EftVec *a, EftVec *b);
extern void Vec3_Normalize(EftVec *dst, EftVec *src);
extern void Vec4_Set(EftVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Add(EftVec *dst, EftVec *a, EftVec *b);
extern void Vec4_Scale(EftVec *dst, EftVec *src, f32 scale);
extern void Vec3_Add(EftVec *dst, EftVec *a, EftVec *b);
extern void Vec3_Scale(EftVec *dst, EftVec *src, f32 scale);
extern void Vec3_Cross(EftVec *dst, EftVec *a, EftVec *b);
extern void Mtx_MulVec4(EftVec *dst, EftMtx *m, EftVec *src);
extern void Mtx_StoreIdentity(EftMtx *m);
extern void Mtx_InverseRT(EftMtx *dst, EftMtx *src);            /* inverse of a rigid transform (view -> world) */
extern void Mtx_RotateZ(EftMtx *dst, EftMtx *src, f32 angle); /* rotation about Z */
extern void Mtx_Mul(EftMtx *dst, EftMtx *a, EftMtx *b);   /* matrix product */
extern void Vec4_Div(EftVec *dst, EftVec *src, f32 div);   /* dst = src / div */
extern void Vec4_Lerp(EftVec *dst, EftVec *a, EftVec *b, f32 t); /* dst = a * t + b * (1 - t) */
extern s32 ClipPoly_ClipPlane(EftGfxVert *verts, EftVec *plane, s32 count); /* clips a polygon against a plane, new count */
extern void ClipPoly_ProjectCur(s32 (*scr)[4], EftVec *col, EftGfxVert *verts, s32 count); /* projects a polygon */
extern void EftPrim_DrawTriangle(s32 *p0, s32 *p1, s32 *p2, EftVec *uv0, EftVec *uv1, EftVec *uv2,
                          EftVec *col0, EftVec *col1, EftVec *col2, s32 arg9, s32 arg10, s32 arg11, s32 z, u64 tex);
extern s32 abs(s32 x);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 atanf(f32 x);
extern f32 acosf(f32 x);
extern s32 BtlCharApi_IsLockedOn(s32 objId);
extern void BtlCharApi_CalcAimDir45(s32 objId, s32 arg, EftVec *out);
extern void BtlCharApi_GetRot(s32 objId, EftVec *out);
extern void BtlCharApi_GetFrameMove(s32 objId, EftVec *out);
extern u8 *gBtlCamView;
extern EftVec gEftClipPlanes[5];

/* Argument block of the impact effect 0x187BE0. */
typedef struct EftImpactArg {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ EftVec dir;
    /* 0x20 */ s32 id;
    /* 0x24 */ s32 objId;
    /* 0x28 */ s32 unk28;
} EftImpactArg; /* size 0x30 */

extern void BtlTask_SetTagBits(EftHitTask *task, s32 bits);       /* task->flags |= bits */
extern void BtlTask_SetPos(EftHitTask *task, EftVec pos);     /* task->pos = pos */
extern void EftTechEvt_RequestStop(s32 side);                         /* beam struggle: sets bit 2 in a per-side word */
extern void EftKiBomb_SetContact(EftHitTask *task, void *mtx);    /* copies a 0x40-byte block into the blast task's work */
extern f32 BtlStage_GetInnerRadius(void);                              /* stage radius - 100 */
#ifdef PORT /* PC build: by address, as the definition takes it (it only reads the struct; the PS2 passes a by-value struct of this size by address) */
extern s32 EftImpact_SpawnBlast(EftImpactArg *arg, f32 scale, f32 unk);
#define EftImpact_SpawnBlast(arg, scale, unk) EftImpact_SpawnBlast(&(arg), scale, unk)
#else
extern s32 EftImpact_SpawnBlast(EftImpactArg arg, f32 scale, f32 unk); /* adds an impact effect task (class 0x2C3F20) */
#endif
extern void EftGndDust_SpawnImpact(s32 objId, EftVec *pos, f32 scale);
extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, EftVec *out);
extern s32 BtlCharApi_TestFlagA4(s32 objId);
extern s32 BtlStage_HasFeature(s32 kind);                          /* stage query (0..13) */
extern void *BtlObj_Get(s32 objId);
extern f32 BtlCharApi_GetChargedUnkC78(s32 objId);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern void DemoCam_SetAnim(void *anim);
extern void DemoCam_SetChr(void *chr);
extern void DemoCam_SetTime(f32 time);
extern f32 DemoCam_GetTime(void);
extern f32 DemoCam_GetLength(void);
extern void DemoCam_Start(void);
extern void DemoCam_Stop(void);

/* Allocates and clears the record list, then the shape arena. */
void EftHit_Init(void) {
    gEftHitList = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftHitList));
    memset(gEftHitList, 0, sizeof(EftHitList));
    EftHitArena_Init();
}

/* Frees the shape arena and the record list. */
void EftHit_Term(void) {
    EftHitArena_Term();
    if (gEftHitList != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftHitList);
        gEftHitList = NULL;
    }
}

/* Returns the free slot of the list, zeroed, for the caller to fill before EftHit_Add. */
EftHitRec *EftHit_GetNew(void) {
    if (gEftHitList->count >= EFT_HIT_MAX) {
        gEftHitList->count = 0;
    }
    BtlScene_IsTimeStopped();
    memset(&gEftHitList->rec[gEftHitList->count], 0, sizeof(EftHitRec));
    gEftHitList->rec[gEftHitList->count].shape.scale = 1.0f;
    return &gEftHitList->rec[gEftHitList->count];
}

/* Completes a record from its source (technique) or attack (blast) and appends it; nothing while time is stopped. */
void EftHit_Add(EftHitRec *rec) {
    if (BtlScene_IsTimeStopped()) {
        return;
    }
    if (gEftHitList->count >= EFT_HIT_MAX) {
        gEftHitList->count = 0;
    }
    if (rec->src != NULL) {
        rec->objId = rec->src->objId;
        rec->unkF0 = rec->src->unk8;
        rec->shape.unk8 = rec->src->def->unk34;
        rec->unk4 = rec->src->def->unk6;
        rec->level = rec->src->def->level;
        rec->type = EFT_HIT_TECH;
        rec->unk54 = (rec->src->def->flags >> 1) & 1;
        rec->seenByAi = 0;
        rec->shape.scale = EftHit_GetScale(rec);
        if (rec->src->def->kind == 9) {
            rec->flags |= EFT_HIT_FLAG_8;
        }
        EftHit_InitMultiHit(rec);
    }
    if (rec->atk != NULL) {
        rec->objId = rec->atk->objId;
        rec->unkF0 = rec->atk->unk16;
        rec->shape.unk8 = rec->atk->unk20;
        rec->unk4 = rec->atk->unk1B;
        rec->level = rec->atk->level;
        rec->type = EFT_HIT_BLAST;
        rec->unk54 = 0;
        rec->seenByAi = 0;
        rec->shape.scale = EftHit_GetScale(rec);
    }
    *(gEftHitList->rec + gEftHitList->count) = *rec;
    gEftHitList->count++;
}

/* Start of a frame: empties the list and rewinds the arena, unless time is stopped. */
void EftHit_BeginFrame(void) {
    if (!BtlScene_IsTimeStopped()) {
        EftHitArena_Reset();
        gEftHitList->count = 0;
    }
}

/* Empties the list and rewinds the arena. */
void EftHit_Clear(void) {
    EftHitArena_Reset();
    gEftHitList->count = 0;
}

/* Returns the record list. */
EftHitList *EftHit_GetList(void) {
    return gEftHitList;
}

/* Returns the flags of the task that owns record idx (0 when there is none). */
s32 EftHit_GetTaskFlags(u32 idx) {
    EftHitList *list = gEftHitList;
    EftHitRec *rec;

    if (idx >= (u32)list->count) {
        return 0;
    }
    rec = &list->rec[idx];
    if (rec->task == NULL) {
        return 0;
    }
    return rec->task->flags;
}

/* Reports an event of record idx to its task: ors bits into the task's flags and stores where it happened. */
/* (Matches only with the two tests nested, the global used directly and `rec` assigned inside: a local for the list
 * or `rec` computed up front changes the order of the by-value vector's entry copy.) */
void EftHit_SetTaskFlag(u32 idx, s32 bits, EftVec pos) {
    EftHitRec *rec;

    if (idx < (u32)gEftHitList->count) {
        rec = &gEftHitList->rec[idx];
        if (rec->task != NULL) {
            BtlTask_SetTagBits(rec->task, bits);
            BtlTask_SetPos(rec->task, pos);
        }
    }
}

/* Spawns the impact effect of every record whose task was hit this frame, once per task. */
void EftHit_SpawnImpacts(void) {
    EftHitRec *rec;
    s32 i;
    s32 flags;

    if (BtlScene_IsTimeStopped()) {
        return;
    }
    for (i = 0; i < gEftHitList->count; i++) {
        rec = &gEftHitList->rec[i];
        if (rec->task != NULL) {
            flags = rec->task->flags;
            if (!(flags & EFT_TASK_IMPACT_DONE) && !(rec->flags & EFT_HIT_FLAG_NO_IMPACT) && !(flags & EFT_TASK_ABSORBED) &&
                (flags & 0x3F)) {
                if (rec->atk != NULL) {
                    EftHit_SpawnBlastImpact(rec);
                }
                if (rec->src != NULL) {
                    EftHit_SpawnTechImpact(rec);
                }
                rec->task->flags |= EFT_TASK_IMPACT_DONE;
            }
        }
    }
}

/* Tells the hit detection whether the record may still hit in pass `mode` (0, 1 or 2). */
s32 EftHit_CanHit(EftHitRec *rec, s32 mode) {
    s32 ok = 1;
    s32 flags;
    EftHitDef *def;

    if (rec->task == NULL) {
        return 0;
    }
    flags = rec->task->flags;
    if (rec->type == EFT_HIT_BLAST) {
        if (mode == 0) {
            if (rec->flags & EFT_HIT_FLAG_ALWAYS) {
            } else if (rec->flags & EFT_HIT_FLAG_NEVER) {
                ok = 0;
            } else if (flags & EFT_TASK_KEEP) {
                ok = 0;
            } else if (flags & EFT_TASK_HIT_STAGE) {
                ok = 0;
            }
        } else {
            ok = (flags & 9) == 0;
        }
        if (flags & EFT_TASK_DEFLECTED) {
            ok = 0;
        }
    } else {
        def = rec->src->def;
        switch (mode) {
        case 0:
            if (rec->flags & EFT_HIT_FLAG_ALWAYS) {
            } else if (rec->flags & EFT_HIT_FLAG_NEVER) {
                ok = 0;
            } else if (flags & EFT_TASK_KEEP) {
                ok = 0;
            } else if (def->cls != 0) {
                if (flags & 0x104) {
                    ok = 0;
                }
            } else if (def->kind != 4) {
                ok = 0;
            }
            break;
        case 1:
            if (rec->flags & EFT_HIT_FLAG_NO_MODE1) {
                ok = 0;
            } else if (flags & EFT_TASK_NO_MODE1) {
                ok = 0;
            } else if (flags & EFT_TASK_MULTI) {
            } else if (flags & 0x109) {
                ok = 0;
            }
            break;
        case 2:
            if (flags & EFT_TASK_NO_MODE2) {
                ok = 0;
            } else {
                ok = (flags & 0x10D) == 0;
            }
            break;
        }
    }
    return ok;
}

/* 0 for a technique of class 0 kind 4, or of another class and kind 9 or 7; else 1. */
/* (An if / else on the class with `kind == 9 || kind == 7` in one condition; a switch on the class, or two separate
 * `if`s for 9 and 7, compile to other code.) */
s32 EftHit_IsStoppedByHit(EftHitRec *rec) {
    s32 result = 1;
    EftHitDef *def;

    if (rec->type == EFT_HIT_TECH) {
        def = rec->src->def;
        if (def->cls == 0) {
            if (def->kind == 4) {
                result = 0;
            }
        } else if (def->kind == 9 || def->kind == 7) {
            result = 0;
        }
    }
    return result;
}

/* Lets a multi-hit technique spawn an impact effect again on its next hit. */
s32 EftHit_RearmImpact(EftHitRec *rec) {
    if (rec->type == EFT_HIT_TECH) {
        if (rec->task->flags & EFT_TASK_MULTI) {
            rec->task->flags &= ~EFT_TASK_IMPACT_DONE;
        }
    }
    return 1;
}

/* Clash of two technique records: 4 = head-on (beam struggle), 1 / 2 = which one is passed, 3 = no clash rule. */
/* A head-on pair (a moves toward b's previous position and the two movements oppose) stops the technique timers of
 * the fighters owning the records (EftTechEvt_RequestStop); the caller does not gate this
 * by contact. The kind test is a range test on the signed byte (`kind >= 0 && kind <= 2`) in an `if` of its own:
 * that is what gives lbu / sltiu and the later sign extension. */
s32 EftHit_ClashTech(EftHitRec *a, EftHitRec *b) {
    EftHitDef *defA = a->src->def;
    EftHitDef *defB = b->src->def;
    EftVec moveA;
    EftVec moveB;
    EftVec d;
    f32 t;
    s32 result;

    if (defA->cls == 0 || defB->cls == 0) {
        return 3;
    }
    if (!((defA->kind >= 0 && defA->kind <= 2) || defA->kind == 8)) {
        return 3;
    }
    if ((defA->flags & 0x100) || (defB->flags & 0x100) || defA->unk9 >= 2 || defB->unk9 >= 2) {
        return 3;
    }
    Vec4_Sub(&moveA, &a->pos, &a->prevPos);
    Vec4_Sub(&moveB, &b->pos, &b->prevPos);
    Vec4_Sub(&d, &b->prevPos, &a->prevPos);
    if (Vec3_Dot(&moveA, &d) > 0.0f && Vec3_Dot(&moveA, &moveB) < 0.0f) {
        result = 4;
        EftTechEvt_RequestStop(a->objId);
        EftTechEvt_RequestStop(b->objId);
    } else {
        Vec4_Sub(&d, &a->pos, &b->pos);
        result = 2;
        Vec3_Normalize(&moveA, &moveA);
        Vec3_Normalize(&d, &d);
        t = Vec3_Dot(&d, &moveA);
        Vec4_Sub(&d, &b->pos, &a->pos);
        Vec3_Normalize(&moveB, &moveB);
        Vec3_Normalize(&d, &d);
        if (!(t < Vec3_Dot(&d, &moveB))) {
            result = 1;
        }
    }
    return result;
}

/* Compares the levels of two records: 2 = a is stronger, 1 = b is stronger, 3 = equal. */
static inline s32 EftHit_CompareLevel(EftHitRec *a, EftHitRec *b) {
    if (a->level > b->level) {
        return 2;
    }
    if (a->level < b->level) {
        return 1;
    }
    return 3;
}

/* Clash of two records: 1 = b wins, 2 = a wins, 3 = draw, 4 = beam struggle. */
s32 EftHit_Clash(EftHitRec *a, EftHitRec *b) {
    s32 result = a->atk != NULL;

    if (a->atk != NULL && b->atk != NULL) {
        result = EftHit_CompareLevel(a, b);
    } else {
        if (b->atk != NULL) {
            result = 2;
        }
        if (a->src != NULL && b->src != NULL) {
            result = EftHit_ClashTech(a, b);
            if (result == 3) {
                result = EftHit_CompareLevel(a, b);
            }
        }
    }
    return result;
}

/* Returns 1 when the owning task is a multi-hit technique. */
s32 EftHit_IsMultiHit(EftHitRec *rec) {
    if (rec->task == NULL) {
        return 0;
    }
    if (rec->task->flags & EFT_TASK_MULTI) {
        return 1;
    }
    return 0;
}

/* Counts one more hit of the owning task. */
void EftHit_IncHitCount(EftHitRec *rec) {
    rec->task->hitCount++;
}

/* Returns the number of hits the owning task has landed. */
s32 EftHit_GetHitCount(EftHitRec *rec) {
    if (rec->task != NULL) {
        return rec->task->hitCount;
    }
    return 0;
}

/* Returns the technique definition's hit limit (0 for a blast). */
s32 EftHit_GetMaxHits(EftHitRec *rec) {
    if (rec->atk != NULL) {
        return 0;
    }
    if (rec->src != NULL) {
        return rec->src->def->maxHits;
    }
    return 0;
}

/* Returns byte 0xB of the technique definition (0 for a blast). */
s32 EftHit_GetDefUnkB(EftHitRec *rec) {
    if (rec->atk != NULL) {
        return 0;
    }
    if (rec->src != NULL) {
        return rec->src->def->unkB;
    }
    return 0;
}

/* Increments the owning task's counter A. */
void EftHit_IncCountA(EftHitRec *rec) {
    if (rec->task != NULL) {
        rec->task->countA++;
    }
}

/* Sets the owning task's counter A. */
void EftHit_SetCountA(EftHitRec *rec, s32 value) {
    if (rec->task != NULL) {
        rec->task->countA = value;
    }
}

/* Returns the owning task's counter A. */
s32 EftHit_GetCountA(EftHitRec *rec) {
    if (rec->task != NULL) {
        return rec->task->countA;
    }
    return 0;
}

/* Returns 1 for a technique whose definition has flag 0x20. */
s32 EftHit_HasDefFlag20(EftHitRec *rec) {
    if (rec->atk != NULL) {
        return 0;
    }
    if (rec->src != NULL) {
        if (rec->src->def->flags & 0x20) {
            return 1;
        }
    }
    return 0;
}

/* Flags the record as the last hit of its technique. */
void EftHit_MarkLastHit(EftHitRec *rec) {
    s32 count;

    if (rec->src == NULL) {
        return;
    }
    if (rec->flags & EFT_HIT_FLAG_NO_LAST) {
        return;
    }
    if (EftHit_IsMultiHit(rec)) {
        if (EftHit_GetHitCount(rec) > 0) {
            if (EftHit_HasDefFlag20(rec)) {
                rec->flags |= EFT_HIT_FLAG_LAST;
            } else {
                count = EftHit_GetHitCount(rec);
                if (!(count < EftHit_GetMaxHits(rec))) {
                    rec->flags |= EFT_HIT_FLAG_LAST;
                }
            }
        }
    } else if (rec->src->def->kind != 4) {
        rec->flags |= EFT_HIT_FLAG_LAST;
    }
}

/* Returns 1 for a technique whose definition has flag 0x200000. */
s32 EftHit_HasDefFlag200000(EftHitRec *rec) {
    if (rec->atk != NULL) {
        return 0;
    }
    if (rec->src != NULL) {
        if (rec->src->def->flags & 0x200000) {
            return 1;
        }
    }
    return 0;
}

/* Calls 0x177BC8 on the task of blast record idx. */
s32 EftHit_NotifyBlastTask(u32 idx, void *mtx) {
    EftHitRec *rec;

    if (gEftHitList != NULL) {
        if (idx < (u32)gEftHitList->count) {
            rec = &gEftHitList->rec[idx];
            if (rec->task != NULL) {
                if (rec->type == EFT_HIT_BLAST) {
                    EftKiBomb_SetContact(rec->task, mtx);
                }
            }
        }
    }
}

/* Returns 1 for a technique of class 0 kind 4, or of another class and kind 9. */
s32 EftHit_IsRushHit(EftHitRec *rec) {
    s32 result = 0;
    EftHitDef *def;

    if (rec->atk != NULL) {
        return 0;
    }
    if (rec->src != NULL) {
        def = rec->src->def;
        if (def->cls == 0) {
            if (def->kind == 4) {
                result = 1;
            }
        } else {
            result = def->kind == 9;
        }
    }
    return result;
}

/* Returns the radius of the shape's first volume. */
f32 EftHit_GetRadiusA(EftHitRec *rec) {
    switch (rec->shape.type) {
    case 0:
        return ((EftHitSphere *)rec->shape.a)->radius;
    case 1:
        return ((EftHitBox *)rec->shape.a)->radius;
    }
    return 0.0f;
}

/* Returns the radius of the shape's second volume. */
f32 EftHit_GetRadiusB(EftHitRec *rec) {
    switch (rec->shape.type) {
    case 0:
        return ((EftHitSphere *)rec->shape.b)->radius;
    case 1:
        return ((EftHitBox *)rec->shape.b)->radius;
    }
    return 0.0f;
}

/* Sets the radius of the shape's first volume. */
void EftHit_SetRadiusA(EftHitRec *rec, f32 radius) {
    switch (rec->shape.type) {
    case 0:
        ((EftHitSphere *)rec->shape.a)->radius = radius;
        break;
    case 1:
        ((EftHitBox *)rec->shape.a)->radius = radius;
        break;
    }
}

/* Sets the radius of the shape's second volume. */
void EftHit_SetRadiusB(EftHitRec *rec, f32 radius) {
    switch (rec->shape.type) {
    case 0:
        ((EftHitSphere *)rec->shape.b)->radius = radius;
        break;
    case 1:
        ((EftHitBox *)rec->shape.b)->radius = radius;
        break;
    }
}

/* Flags (task flag 0x10) every record that left the stage radius and pulls its task position back onto it. */
void EftHit_ClampToStage(void) {
    EftHitList *list = EftHit_GetList();
    EftHitRec *rec;
    EftHitShape *shape;
    s32 i;
    f32 radius;
    f32 len;

    for (i = 0; i < list->count; i++) {
        rec = &list->rec[i];
        if (rec->task != NULL) {
            radius = BtlStage_GetInnerRadius() + 0.0f;
            if (radius < sqrtf(rec->pos.x * rec->pos.x + rec->pos.z * rec->pos.z)) {
                if (!(rec->task->flags & EFT_TASK_OUT)) {
                    Vec4_Copy(&rec->task->pos, &rec->pos);
                    radius = sqrtf(rec->task->pos.x * rec->task->pos.x + rec->task->pos.z * rec->task->pos.z);
                    rec->task->pos.x *= BtlStage_GetInnerRadius() / radius;
                    rec->task->pos.z *= BtlStage_GetInnerRadius() / radius;
                    shape = &rec->shape;
                    switch (shape->type) {
                    case 0:
                        Vec4_Copy(&((EftHitSphere *)shape->a)->pos, &rec->task->pos);
                        break;
                    case 1:
                        Vec4_Copy(&((EftHitBox *)shape->a)->pos, &rec->task->pos);
                        break;
                    }
                    rec->task->flags |= EFT_TASK_OUT;
                }
            }
        }
    }
}

/* Stores this frame's result code in the task of every record. */
void EftHit_UpdateResults(void) {
    EftHitList *list = EftHit_GetList();
    EftHitRec *rec;
    s32 i;

    for (i = 0; i < list->count; i++) {
        rec = &list->rec[i];
        if (rec->task != NULL) {
            rec->task->result = EftHit_CalcResult(rec);
        }
    }
}

/* Turns the task's event flags into the result code its update reads: 0 none, 1, 3, 4 = hit, 5 = finished. */
s32 EftHit_CalcResult(EftHitRec *rec) {
    s32 result = 0;
    s32 flags;
    s32 count;

    if (rec->task->flags & EFT_TASK_HIT_STAGE) {
        result = 5;
    }
    if (rec->task->flags & EFT_TASK_MULTI_CONTACT) {
        result = 1;
        rec->task->flags &= ~EFT_TASK_MULTI_CONTACT;
    }
    if (rec->task->flags & EFT_TASK_HIT_CHAR) {
        if (rec->task->flags & EFT_TASK_MULTI) {
            count = EftHit_GetHitCount(rec);
            if (EftHit_GetMaxHits(rec) < count) {
                if (!EftHit_HasDefFlag20(rec)) {
                    result = 5;
                }
            }
        } else if (EftHit_HasDefFlag20(rec)) {
            rec->task->flags |= EFT_TASK_KEEP;
        } else {
            result = 4;
        }
    }
    flags = rec->task->flags;
    if (flags & EFT_TASK_LOST_CLASH) {
        result = 4;
    }
    if (flags & EFT_TASK_GUARDED) {
        if (!(flags & EFT_TASK_MULTI)) {
            result = 5;
        }
    }
    if (flags & EFT_TASK_ABSORBED) {
        result = 5;
    }
    if (flags & EFT_TASK_OUT) {
        result = 5;
    }
    if (flags & EFT_TASK_STRUGGLE) {
        result = 3;
    }
    if (EftHit_IsTechClass(rec, 0)) {
        if (!(rec->task->flags & EFT_TASK_LOST_CLASH)) {
            result &= ~5;
        }
    }
    return result;
}

/* Allocates the arena header and its 0x1800-byte buffer. */
void EftHitArena_Init(void) {
    gEftHitArena = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftHitArena));
    memset(gEftHitArena, 0, sizeof(EftHitArena));
    gEftHitArena->size = 0x1800;
    gEftHitArena->base = BtlPool_Alloc(BtlPool_GetCurrent(), gEftHitArena->size);
    memset(gEftHitArena->base, 0, gEftHitArena->size);
    gEftHitArena->cur = gEftHitArena->base;
}

/* Frees the arena buffer and header. */
void EftHitArena_Term(void) {
    if (gEftHitArena->base != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftHitArena->base);
        gEftHitArena->base = NULL;
    }
    if (gEftHitArena != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftHitArena);
        gEftHitArena = NULL;
    }
}

/* Rewinds the arena. */
void EftHitArena_Reset(void) {
    gEftHitArena->cur = gEftHitArena->base;
}

/* Takes n bytes of the arena: a post-increment of the cursor seen as a pointer to an n-byte object. */
#define EFT_ARENA_ALLOC(n) ((void *)(*(u8 (**)[n])&gEftHitArena->cur)++)

/* Takes 0x20 bytes of the arena (no caller). */
void *EftHitArena_Alloc20A(void) {
    return EFT_ARENA_ALLOC(0x20);
}

/* Takes 0x20 bytes of the arena (no caller). */
void *EftHitArena_Alloc20B(void) {
    return EFT_ARENA_ALLOC(0x20);
}

/* Takes 0x40 bytes of the arena (no caller). */
void *EftHitArena_Alloc40(void) {
    return EFT_ARENA_ALLOC(0x40);
}

/* Takes a 0x20-byte sphere of the arena. */
EftHitSphere *EftHitArena_AllocSphere(void) {
    return EFT_ARENA_ALLOC(0x20);
}

/* Takes 0x20 bytes of the arena (no caller). */
void *EftHitArena_Alloc20C(void) {
    return EFT_ARENA_ALLOC(0x20);
}

/* Takes a record (0x190 bytes) of the arena (no caller). */
EftHitRec *EftHitArena_AllocRec(void) {
    return EFT_ARENA_ALLOC(0x190);
}

/* Takes a 0x30-byte box of the arena. */
EftHitBox *EftHitArena_AllocBox(void) {
    return EFT_ARENA_ALLOC(0x30);
}

/* Sets a type 3 shape (no caller). */
void EftHit_SetShape3(EftHitRec *rec, void *a, void *b) {
    EftHitShape *shape = &rec->shape;

    shape->type = 3;
    shape->a = a;
    shape->b = b;
}

/* Sets a type 4 shape (no caller). */
void EftHit_SetShape4(EftHitRec *rec, void *a, void *b) {
    EftHitShape *shape = &rec->shape;

    shape->type = 4;
    shape->a = a;
    shape->b = b;
}

/* Sets a type 5 shape (no caller). */
void EftHit_SetShape5(EftHitRec *rec, void *a, void *b) {
    EftHitShape *shape = &rec->shape;

    shape->type = 5;
    shape->a = a;
    shape->b = b;
}

/* Sets the shape to two spheres: previous and current position. */
void EftHit_SetShapeSpheres(EftHitRec *rec, EftHitSphere *a, EftHitSphere *b) {
    EftHitShape *shape = &rec->shape;

    shape->type = 0;
    shape->a = a;
    shape->b = b;
}

/* Sets a type 6 shape (no caller). */
void EftHit_SetShape6(EftHitRec *rec, void *a, void *b) {
    EftHitShape *shape = &rec->shape;

    shape->type = 6;
    shape->a = a;
    shape->b = b;
}

/* Sets a type 2 shape (no caller). */
void EftHit_SetShape2(EftHitRec *rec, void *a, void *b) {
    EftHitShape *shape = &rec->shape;

    shape->type = 2;
    shape->a = a;
    shape->b = b;
}

/* Sets the shape to two boxes: previous and current position. */
void EftHit_SetShapeBoxes(EftHitRec *rec, EftHitBox *a, EftHitBox *b) {
    EftHitShape *shape = &rec->shape;

    shape->type = 1;
    shape->a = a;
    shape->b = b;
}

/* Returns 1 for a technique whose definition class is cls. */
s32 EftHit_IsTechClass(EftHitRec *rec, s32 cls) {
    s32 result = 0;

    if (rec->type == EFT_HIT_TECH) {
        result = rec->src->def->cls == cls;
    }
    return result;
}

/* Returns the record's scale: the definition's for a technique, 0.1 for a blast. */
f32 EftHit_GetScale(EftHitRec *rec) {
    f32 scale = 1.0f;

    if (rec->src != NULL) {
        scale = rec->src->def->scale;
    }
    if (rec->atk != NULL) {
        scale = 0.1f;
    }
    return scale;
}

/* Marks the owning task as multi-hit when the definition has a hit limit, and drops a stale "hit a fighter". */
/* FAKE MATCH (permuter): the self-assignment of the definition's count inside the `if`. It emits nothing in the
 * end, but the store (`sb count,10(def)`) is real RTL until AFTER register allocation: only the cse pass that
 * follows reload deletes it, as a store of a value the register was just loaded with. Until then the definition
 * pointer and the count are live into the inner block, so they are no longer two values local to the middle block
 * with equal priority (2 references, a life of one instruction: the tie went to the first-born, the definition,
 * which took $v0 and pushed the count to $v1). Without the statement 3 of 20 instructions differ, registers only:
 * the original keeps rec->src in $v0 and loads the definition into $v1 (lw v1,0x24(v0); lb v0,0xA(v1)).
 * `+= 0`, `|= 0`, `*= 1` and a clamp the compiler folds away (`x = x > 127 ? 127 : x` on the s8 field) do the
 * same; a second READ of the field (a dead local, `(void)`, an empty `if`, `n` loaded again) does not, and
 * `... = n` emits a real store (n is the sign-extended copy).
 * What it stands for is unknown: a statement of the original that wrote the count back unchanged (a vacuous
 * clamp or a macro), or anything else that kept the two values alive in the inner block. About 130 natural forms
 * were tried in the earlier passes (locals for each value, a variable set twice, early returns, && / nested,
 * inline accessors, result variables, casts, loop / goto / switch shapes): all give those 3 instructions or more. */
void EftHit_InitMultiHit(EftHitRec *rec) {
    s32 n;

    if (rec->src != NULL) {
        n = rec->src->def->maxHits;
        if (n > 0) {
            rec->src->def->maxHits = rec->src->def->maxHits;
            rec->task->flags |= EFT_TASK_MULTI;
            if (rec->task->flags & EFT_TASK_HIT_CHAR) {
                rec->task->flags &= ~EFT_TASK_HIT_CHAR;
            }
        }
    }
}

/* Spawns the impact effect of a ki blast at the place its task was hit. */
void EftHit_SpawnBlastImpact(EftHitRec *rec) {
    EftImpactArg arg;
    EftHitAtk *atk;
    f32 scale;

    rec->task->pos.w = 1.0f;
    scale = 1.0f;
    atk = rec->atk;
    arg = (EftImpactArg){ rec->task->pos, { 0.0f, 0.0f, 0.0f, 0.0f }, 0, rec->objId, 0 };
    Vec4_Sub(&arg.dir, &rec->pos, &rec->prevPos);
    Vec3_Normalize(&arg.dir, &arg.dir);
    if (atk->unk1A == 1) {
        arg.id = 1;
        scale = 0.6f;
    }
    if (rec->task->flags & EFT_TASK_HIT_CHAR) {
        if (rec->flags & EFT_HIT_FLAG_GROUND) {
            EftGndDust_SpawnImpact(rec->objId, &rec->task->pos, 1.0f);
        } else {
            EftImpact_SpawnBlast(arg, scale, 0.2f);
        }
    } else if (rec->task->flags & EFT_TASK_HIT_STAGE) {
        if (!(rec->flags & EFT_HIT_FLAG_GROUND)) {
            EftImpact_SpawnBlast(arg, scale, 1.0f);
        } else {
            EftGndDust_SpawnImpact(rec->objId, &rec->task->pos, 1.0f);
        }
    } else {
        EftImpact_SpawnBlast(arg, 1.0f, 0.5f);
    }
}

/* Spawns the impact effect of a technique: on a fighter (definition impactFx) or on the stage (groundFx). */
void EftHit_SpawnTechImpact(EftHitRec *rec) {
    EftVec dir;
    EftHitSrc *src;
    s32 spawn = 0;

    rec->task->pos.w = 1.0f;
    src = rec->src;
    Vec4_Sub(&dir, &rec->prevPos, &rec->pos);
    Vec3_Normalize(&dir, &dir);
    if (rec->task->flags & 3) {
        EftVec pos;
        EftImpactArg arg;

        if (src->def->impactFx >= 0) {
            Vec4_Copy(&pos, &rec->task->pos);
            if (rec->task->flags & EFT_TASK_MULTI) {
                if (EftHit_IsRushHit(rec)) {
                    spawn = 1;
                    BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(rec->objId), 0x11, &pos);
                } else if (EftHit_GetHitCount(rec) == 1) {
                    spawn = 1;
                }
            } else {
                spawn = 1;
            }
            if (spawn) {
                arg = (EftImpactArg){ pos, dir, src->def->impactFx, rec->objId, 0 };
                EftImpact_SpawnBlast(arg, rec->src->def->impactScale, 0.5f);
            }
        }
    } else if (rec->task->flags & EFT_TASK_HIT_STAGE) {
        EftImpactArg arg;

        if (src->def->groundFx >= 0 && !BtlCharApi_TestFlagA4(rec->objId)) {
            BtlStage_HasFeature(7);
            spawn = 1;
        }
        if (spawn) {
            arg = (EftImpactArg){ rec->task->pos, dir, src->def->groundFx, rec->objId, 0 };
            EftImpact_SpawnBlast(arg, rec->src->def->groundScale, 1.0f);
        }
    }
}

/* Allocates and clears the camera cut state. */
void EftCam_Init(void) {
#ifdef PORT
    memset(fighterCams,0,sizeof(fighterCams));
#endif
    gEftCam = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftCam));
    memset(gEftCam, 0, sizeof(EftCam));
}

/* Frees the camera cut state. */
void EftCam_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftCam);
    gEftCam = NULL;
}

/* Clears the camera cut state (BtlScene_Reset). */
void EftCam_Clear(void) {
#ifdef PORT
    extern void DemoCam_ResetFighters(void);
    DemoCam_ResetFighters();
    memset(fighterCams,0,sizeof(fighterCams));
#endif
    if (gEftCam != NULL) {
        memset(gEftCam, 0, sizeof(EftCam));
    }
}

/* Starts a camera cut: plays arg->anim on the demo camera, timed by fighter arg->objId's technique timer if any. */
void EftCam_Start(EftCamArg *arg) {
#ifdef PORT
    EftCam *saved=gEftCam;void *demo=NULL;s32 i;
    if(TagTeam_Active() && BtlSeq_IsFighting()) for(i=0;i<TagTeam_Count();i++)
        if(BattleSide_GetObjId(i)==arg->objId) {
            gEftCam=&fighterCams[i];demo=DemoCam_EnterFighter(i);break;
        }
#endif
    gEftCam->arg = *arg;
    DemoCam_SetAnim(arg->anim);
    if (arg->objId != -1) {
        DemoCam_SetChr(BtlObj_Get(arg->objId));
        DemoCam_SetTime(BtlCharApi_GetChargedUnkC78(arg->objId));
    } else {
        DemoCam_SetTime(0.0f);
    }
    DemoCam_Start();
    gEftCam->objId = arg->objId;
    gEftCam->active = 1;
    gEftCam->hold = 0;
#ifdef PORT
    if(demo)DemoCam_LeaveFighter(demo);
    gEftCam=saved;
#endif
}

/* Sets whether the cut holds its last frame instead of ending. */
void EftCam_SetHold(s32 hold) {
    gEftCam->hold = hold;
}

/* Pauses or resumes an active cut. */
void EftCam_SetPaused(s32 paused) {
    if (gEftCam->active) {
        gEftCam->paused = paused;
    }
}

/* Returns the pause state of the cut. */
s32 EftCam_IsPaused(void) {
    return gEftCam->paused;
}

/* Returns 1 while a cut is playing. */
s32 EftCam_IsActive(void) {
    return gEftCam->active;
}

/* Ends the cut and releases the demo camera. */
void EftCam_Stop(void) {
    if (gEftCam->active) {
        DemoCam_Stop();
        gEftCam->active = 0;
    }
}

/* Per-frame: moves the cut to the fighter's technique time (or 2.0 further) and ends it past the animation's length. */
static void EftCam_UpdateOne(void) {
    f32 time;

    if (!gEftCam->active) {
        return;
    }
    if (gEftCam->paused) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (gEftCam->objId != -1) {
        if (!BtlCharApi_IsInTechnique(gEftCam->objId)) {
#ifdef PORT
            /* An interruption must release this actor's camera too. */
            EftCam_Stop();
#endif
            return;
        }
        time = BtlCharApi_GetChargedUnkC78(gEftCam->objId);
    } else {
        time = DemoCam_GetTime() + 2.0f;
    }
    if (DemoCam_GetLength() < time) {
        DemoCam_SetTime(DemoCam_GetLength());
        if (!gEftCam->hold) {
            DemoCam_Stop();
            gEftCam->active = 0;
        }
    } else {
        DemoCam_SetTime(time);
    }
}

void EftCam_Update(void) {
#ifdef PORT
    extern void DemoCam_BeginFighterFrame(void);
    DemoCam_BeginFighterFrame();
#endif
    EftCam_UpdateOne();
#ifdef PORT
    if(TagTeam_Active()) {
        EftCam *saved=gEftCam;s32 i;
        for(i=0;i<TagTeam_Count();i++) if(fighterCams[i].active && !TagTeam_CinematicPaused(i)) {
            void *demo=DemoCam_EnterFighter(i);
            gEftCam=&fighterCams[i];EftCam_UpdateOne();DemoCam_LeaveFighter(demo);
        }
        gEftCam=saved;
    }
#endif
}

#ifdef PORT
void EftCam_StopFighter(s32 objId) {
    s32 i;
    if(TagTeam_Active()) for(i=0;i<TagTeam_Count();i++) if(BattleSide_GetObjId(i)==objId && fighterCams[i].active) {
        EftCam *saved=gEftCam;void *demo=DemoCam_EnterFighter(i);
        gEftCam=&fighterCams[i];EftCam_Stop();gEftCam=saved;DemoCam_LeaveFighter(demo);return;
    }
    if(!TagTeam_Active() || gEftCam->objId==objId)EftCam_Stop();
}
s32 EftCam_IsFighterActive(s32 objId) {
    s32 i;
    if(!TagTeam_Active())return EftCam_IsActive();
    for(i=0;i<TagTeam_Count();i++) if(BattleSide_GetObjId(i)==objId)
        if(fighterCams[i].active)return 1;
    return gEftCam->active && (gEftCam->objId<0 || gEftCam->objId==objId);
}
#endif

/*
 * Lights a 256-entry palette (RGBA bytes) from a 256-entry table of normals (bytes, 128 = 0): adds the colour
 * (r, g, b) to every entry and puts the diffuse term (N.L * 64 + 32, at most 128) into alpha.
 */
void EftGfx_LightClutDiffuse(u8 *dst, u8 *nrm, EftVec light, u8 r, u8 g, u8 b) {
    EftVec n;
    s32 i;
    s32 v;

    for (i = 0; i < 256; i++) {
        n.x = nrm[i * 4] - 128.0f;
        n.y = nrm[i * 4 + 1] - 128.0f;
        n.z = nrm[i * 4 + 2] - 128.0f;
        n.w = 1.0f;
        Vec3_Normalize(&n, &n);
        v = Vec3_Dot(&light, &n) * 128.0f;
        dst[i * 4] += r;
        dst[i * 4 + 1] += g;
        dst[i * 4 + 2] += b;
        if (v < 0) {
            v = 0;
        }
        v = v / 2 + 0x20;
        if (v > 0x80) {
            v = 0x80;
        }
        dst[i * 4 + 3] += v;
    }
}

/* Adds a specular highlight to a 256-entry palette from the table of normals: view matrix, light matrix, eye vector. */
void EftGfx_LightClutSpecular(u8 *dst, u8 *nrm, EftMtx view, EftMtx light, EftVec eye, f32 power) {
    EftVec h;
    EftVec r;
    EftVec n;
    EftVec c;
    EftVec z = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftMtx inv;
    s32 i;
    s32 v;
    f32 d = 0.0f;

    z.w = 1.0f;
    Mtx_InverseRT(&inv, &view);
    z.x = -inv.row[2].x;
    z.y = -inv.row[2].y;
    z.z = -inv.row[2].z;
    inv.row[3].x = inv.row[3].y = inv.row[3].z = d;
    c.x = Vec3_Dot(&z, &light.row[0]);
    c.y = Vec3_Dot(&z, &light.row[1]);
    c.z = Vec3_Dot(&z, &light.row[2]);
    c.w = 1.0f;
    for (i = 0; i < 256; i++) {
        n.x = nrm[i * 4] - 128.0f;
        n.y = nrm[i * 4 + 1] - 128.0f;
        n.z = nrm[i * 4 + 2] - 128.0f;
        n.w = 1.0f;
        Vec3_Normalize(&n, &n);
        d = Vec3_Dot(&eye, &n);
        d = d + d;
        r.x = n.x * d - eye.x;
        r.y = n.y * d - eye.y;
        r.z = n.z * d - eye.z;
        r.w = 1.0f;
        Mtx_MulVec4(&h, &inv, &r);
        v = Vec3_Dot(&c, &h) * (power * 255.0f);
        if (v < 0) {
            v = 0;
        }
        if (v > 255) {
            v = 255;
        }
        dst[i * 4] += v;
        dst[i * 4 + 1] += v;
        dst[i * 4 + 2] += v;
        dst[i * 4 + 3] += 0x80;
    }
}

/* Blends two 256-entry palettes: dst = a * t + b * (1 - t), t clamped to 0..1 (no caller). */
void EftGfx_LerpClut(u8 *dst, u8 *a, u8 *b, f32 t) {
    EftVec va;
    EftVec vb;
    EftVec out;
    s32 i;

    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    for (i = 0; i < 256; i++) {
        Vec4_Set(&va, a[i * 4], a[i * 4 + 1], a[i * 4 + 2], a[i * 4 + 3]);
        Vec4_Set(&vb, b[i * 4], b[i * 4 + 1], b[i * 4 + 2], b[i * 4 + 3]);
        Vec4_Lerp(&out, &va, &vb, t);
        dst[i * 4] = (u32)out.x;
        dst[i * 4 + 1] = (u32)out.y;
        dst[i * 4 + 2] = (u32)out.z;
        dst[i * 4 + 3] = (u32)out.w;
    }
}

/* Builds the tangent frame of a triangle from its three positions and three texture coordinates. */
void EftMath_CalcTangentFrame(EftMtx *out, EftVec *pos, EftVec *uv) {
    EftVec n;
    EftVec c;
    EftVec e1;
    EftVec e2;

    Vec4_Set(&e1, pos[1].x - pos[0].x, uv[1].x - uv[0].x, uv[1].y - uv[0].y, 1.0f);
    Vec4_Set(&e2, pos[2].x - pos[0].x, uv[2].x - uv[0].x, uv[2].y - uv[0].y, 1.0f);
    Vec3_Cross(&c, &e1, &e2);
    out->row[0].x = -c.y / c.x;
    out->row[1].x = -c.z / c.x;
    Vec4_Set(&e1, pos[1].y - pos[0].y, uv[1].x - uv[0].x, uv[1].y - uv[0].y, 1.0f);
    Vec4_Set(&e2, pos[2].y - pos[0].y, uv[2].x - uv[0].x, uv[2].y - uv[0].y, 1.0f);
    Vec3_Cross(&c, &e1, &e2);
    out->row[0].y = -c.y / c.x;
    out->row[1].y = -c.z / c.x;
    Vec4_Set(&e1, pos[1].z - pos[0].z, uv[1].x - uv[0].x, uv[1].y - uv[0].y, 1.0f);
    Vec4_Set(&e2, pos[2].z - pos[0].z, uv[2].x - uv[0].x, uv[2].y - uv[0].y, 1.0f);
    Vec3_Cross(&c, &e1, &e2);
    out->row[0].z = -c.y / c.x;
    out->row[1].z = -c.z / c.x;
    Vec3_Cross(&n, &out->row[0], &out->row[1]);
    Vec3_Cross(&out->row[1], &n, &out->row[0]);
    Vec3_Normalize(&out->row[0], &out->row[0]);
    Vec3_Normalize(&out->row[1], &out->row[1]);
    Vec3_Normalize(&out->row[2], &n);
    out->row[0].w = 1.0f;
    out->row[1].w = 1.0f;
    out->row[2].w = 1.0f;
    out->row[3].w = 1.0f;
    out->row[3].x = 0.0f;
    out->row[3].y = 0.0f;
    out->row[3].z = 0.0f;
}

/* Builds the rotation whose Z axis is dir (up = -Y), rolled by `roll` about Z; returns 0 for a null dir. */
s32 EftMath_MtxFromDir(EftMtx *out, EftVec *dir, f32 roll) {
    EftVec up = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftVec right;
    EftVec fwd;
    EftMtx rot;

    up.y = -1.0f;
    Vec3_Normalize(&fwd, dir);
    if (fwd.x == 0.0f && fwd.y == 0.0f && fwd.z == 0.0f) {
        return 0;
    }
    Vec3_Cross(&right, &up, &fwd);
    Vec3_Normalize(&right, &right);
    Vec3_Cross(&up, &right, &fwd);
    Vec3_Normalize(&up, &up);
    Mtx_StoreIdentity(out);
    out->row[0].x = right.x;
    out->row[0].y = right.y;
    out->row[0].z = right.z;
    out->row[1].x = up.x;
    out->row[1].y = up.y;
    out->row[1].z = up.z;
    out->row[2].x = fwd.x;
    out->row[2].y = fwd.y;
    out->row[2].z = fwd.z;
    if (roll != 0.0f) {
        Mtx_StoreIdentity(&rot);
        Mtx_RotateZ(&rot, &rot, roll);
        Mtx_Mul(out, out, &rot);
    }
    return 1;
}

/* Wraps an angle into -pi..pi. */
f32 EftMath_WrapAngle(f32 angle) {
    s32 n;

    if (angle >= 3.14159265f) {
        n = (angle - 3.14159265f) / 6.2831853f;
        n = abs(n);
        angle -= n * 6.2831853f + 6.2831853f;
    } else if (angle <= -3.14159265f) {
        n = (angle + 3.14159265f) / 6.2831853f;
        n = abs(n);
        angle += n * 6.2831853f + 6.2831853f;
    }
    return angle;
}

/* Catmull-Rom point of four control points at t. The result goes to a by-value argument and is lost (no caller). */
void EftMath_CatmullRom(EftVec out, EftVec *p, f32 t) {
    EftMtx basis = { { { -1.0f, 3.0f, -3.0f, 1.0f }, { 2.0f, -5.0f, 4.0f, -1.0f }, { -1.0f, 0.0f, 1.0f, 0.0f },
                       { 0.0f, 2.0f, 0.0f, 0.0f } } };
    EftVec tv;
    EftMtx pts;

    Vec4_Set(&pts.row[0], p[0].x, p[0].y, p[0].z, p[0].w);
    Vec4_Set(&pts.row[1], p[1].x, p[1].y, p[1].z, p[1].w);
    Vec4_Set(&pts.row[2], p[2].x, p[2].y, p[2].z, p[2].w);
    Vec4_Set(&pts.row[3], p[3].x, p[3].y, p[3].z, p[3].w);
    Vec4_Set(&tv, t * t * t, t * t, t, 1.0f);
    Mtx_MulVec4(&tv, &basis, &tv);
    Mtx_MulVec4(&tv, &pts, &tv);
    Vec4_Div(&out, &tv, 2.0f);
}

/* Quadratic spline point of three control points at t (basis 0x2EC520), w = 1. */
void EftMath_Spline3(EftVec *out, EftVec *p, f32 t) {
    EftMtx basis = { { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, -2.0f, 1.0f }, { 0.0f, -3.0f, 4.0f, -1.0f },
                       { 0.0f, 2.0f, 0.0f, 0.0f } } };
    EftMtx pts;
    EftVec tv;

    Vec4_Set(&pts.row[0], 0.0f, 0.0f, 0.0f, 0.0f);
    Vec4_Set(&pts.row[1], p[0].x, p[0].y, p[0].z, p[0].w);
    Vec4_Set(&pts.row[2], p[1].x, p[1].y, p[1].z, p[1].w);
    Vec4_Set(&pts.row[3], p[2].x, p[2].y, p[2].z, p[2].w);
    Vec4_Set(&tv, t * t * t, t * t, t, 1.0f);
    Mtx_MulVec4(&tv, &basis, &tv);
    Mtx_MulVec4(&tv, &pts, &tv);
    Vec4_Div(out, &tv, 2.0f);
    out->w = 1.0f;
}

/* Quadratic spline point of three control points at t (basis 0x2EC560); result lost in a by-value argument (no caller). */
void EftMath_Spline3B(EftVec out, EftVec *p, f32 t) {
    EftMtx basis = { { { 0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, -2.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 1.0f, 0.0f },
                       { 0.0f, 2.0f, 0.0f, 0.0f } } };
    EftMtx pts;
    EftVec tv;

    Vec4_Set(&pts.row[0], p[0].x, p[0].y, p[0].z, p[0].w);
    Vec4_Set(&pts.row[1], p[1].x, p[1].y, p[1].z, p[1].w);
    Vec4_Set(&pts.row[2], p[2].x, p[2].y, p[2].z, p[2].w);
    Vec4_Set(&pts.row[3], 0.0f, 0.0f, 0.0f, 0.0f);
    Vec4_Set(&tv, t * t * t, t * t, t, 1.0f);
    Mtx_MulVec4(&tv, &basis, &tv);
    Mtx_MulVec4(&tv, &pts, &tv);
    Vec4_Div(&out, &tv, 2.0f);
}

/* Cubic Bezier point of four control points at t; result lost in a by-value argument (no caller). */
void EftMath_Bezier(EftVec out, EftVec *p, f32 t) {
    EftMtx basis = { { { -1.0f, 3.0f, -3.0f, 1.0f }, { 3.0f, -6.0f, 3.0f, 0.0f }, { -3.0f, 3.0f, 0.0f, 0.0f },
                       { 1.0f, 0.0f, 0.0f, 0.0f } } };
    EftMtx pts;
    EftVec tv;

    Vec4_Set(&pts.row[0], p[0].x, p[0].y, p[0].z, p[0].w);
    Vec4_Set(&pts.row[1], p[1].x, p[1].y, p[1].z, p[1].w);
    Vec4_Set(&pts.row[2], p[2].x, p[2].y, p[2].z, p[2].w);
    Vec4_Set(&pts.row[3], p[3].x, p[3].y, p[3].z, p[3].w);
    Vec4_Set(&tv, t * t * t, t * t, t, 1.0f);
    Mtx_MulVec4(&tv, &basis, &tv);
    Mtx_MulVec4(&tv, &pts, &tv);
    Vec4_Copy(&out, &tv);
}

/* Linear interpolation of two points; result lost in a by-value argument (no caller). */
void EftMath_Lerp(EftVec out, EftVec *p, f32 t) {
    s32 i;

    for (i = 0; i < 3; i++) {
        out.v[i] = p[0].v[i] + (p[1].v[i] - p[0].v[i]) * t;
    }
    out.w = 0.0f;
}

/* Kochanek-Bartels (tension, bias, continuity) spline point of four control points; result lost (no caller). */
void EftMath_TcbSpline(EftVec out, EftVec *p, f32 t, f32 tension, f32 bias, f32 continuity) {
    EftMtx basis = { { { 2.0f, -2.0f, 1.0f, 1.0f }, { -3.0f, 3.0f, -2.0f, -1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },
                       { 1.0f, 0.0f, 0.0f, 0.0f } } };
    EftVec tv;
    EftVec k;
    EftMtx m;
    EftVec h;
    EftVec tmp;

    Vec4_Set(&tv, t * t * t, t * t, t, 1.0f);
    Vec4_Set(&k, (1.0f - tension) * (bias + 1.0f) * (continuity + 1.0f) * 0.5f,
             (1.0f - tension) * (1.0f - bias) * (1.0f - continuity) * 0.5f,
             (1.0f - tension) * (1.0f - bias) * (continuity + 1.0f) * 0.5f,
             (1.0f - tension) * (bias + 1.0f) * (1.0f - continuity) * 0.5f);
    Mtx_MulVec4(&h, &basis, &tv);
    Vec4_Sub(&m.row[0], &p[1], &p[0]);
    Vec4_Sub(&m.row[1], &p[2], &p[1]);
    Vec4_Sub(&m.row[2], &p[2], &p[1]);
    Vec4_Sub(&m.row[3], &p[3], &p[2]);
    Vec4_Scale(&tmp, &m.row[0], k.x);
    Vec4_Scale(&m.row[2], &m.row[1], k.y);
    Vec4_Add(&m.row[2], &m.row[2], &tmp);
    Vec4_Scale(&tmp, &m.row[1], k.z);
    Vec4_Scale(&m.row[3], &m.row[3], k.w);
    Vec4_Add(&m.row[3], &m.row[3], &tmp);
    Vec4_Copy(&m.row[0], &p[1]);
    Vec4_Copy(&m.row[1], &p[2]);
    Mtx_MulVec4(&out, &m, &h);
}

/* Returns the five view-frustum planes (world space) the clipped draw functions use. */
EftVec *EftGfx_GetClipPlanes(void) {
    return gEftClipPlanes;
}

/* Rebuilds the five clip planes from the current camera view (top, bottom, left, right, near). */
void EftGfx_UpdateClipPlanes(void) {
    EftVec *plane = gEftClipPlanes;
    EftMtx m;
    EftVec eye;
    f32 angle = 30.0f * 3.14159265f / 180.0f;
#ifdef PORT
    f32 aspect = 0.75f / Port_WideFactor(); /* the side planes of a 16:9 view */
#else
    f32 aspect = 0.75f;
#endif
    f32 one = 1.0f;
    f32 angle2;
    f32 s;
    f32 c;
    s32 i;

    s = sinf(angle);
    c = cosf(angle);
    plane[0].x = 0.0f;
    plane[0].y = c;
    plane[0].z = s;
    plane[0].w = one;
    plane[1].x = 0.0f;
    plane[1].y = -c;
    plane[1].z = s;
    plane[1].w = one;
    angle2 = atanf(s / (c * aspect));
    s = sinf(angle2);
    c = cosf(angle2);
    plane[2].x = -c;
    plane[2].y = 0.0f;
    plane[2].z = s;
    plane[2].w = one;
    plane[3].x = c;
    plane[3].y = 0.0f;
    plane[3].z = s;
    plane[3].w = one;
#ifdef PORT
    if(TagTeam_Active()) {
        extern void BtlCam_GetFrustumSize(f32 *);
        f32 size[3];
        BtlCam_GetFrustumSize(size);
        /* Clipped clouds, beams and other effects must use this view's field
           of view. The fixed 4:3 planes left straight cuts across wider views. */
        plane[0].y=size[2]; plane[1].y=-size[2];
        plane[0].z=plane[1].z=size[1];
        plane[2].x=-size[2]; plane[3].x=size[2];
        plane[2].z=plane[3].z=size[0];
        for(i=0;i<4;i++) Vec3_Normalize(&plane[i],&plane[i]);
    }
#endif
    plane[4].x = 0.0f;
    plane[4].y = 0.0f;
    plane[4].z = one;
    plane[4].w = -one;
    Mtx_InverseRT(&m, (EftMtx *)(gBtlCamView + 0x40));
    Vec4_Copy(&eye, &m.row[3]);
    m.row[3].x = m.row[3].y = m.row[3].z = 0.0f;
    for (i = 0; i < 5; i++) {
        Mtx_MulVec4(&plane[i], &m, &plane[i]);
        plane[i].w = -Vec3_Dot(&plane[i], &eye);
    }
    plane[4].w -= 0.3f;
}

/* Direction a projectile of fighter objId starts in: at the locked target, else straight ahead (yaw only). */
s32 EftAim_GetDir(EftVec *out, s32 arg, s32 objId) {
    EftVec rot;

    if (BtlCharApi_IsLockedOn(objId)) {
        BtlCharApi_CalcAimDir45(objId, arg, out);
    } else {
        BtlCharApi_GetRot(objId, &rot);
        out->x = sinf(rot.y);
        out->y = 0.0f;
        out->z = cosf(rot.y);
        out->w = 1.0f;
    }
    Vec3_Normalize(out, out);
    return 1;
}

/*
 * Same for a technique: with definition aimMode != 0 the aim is taken once and kept in src->aimDir; an aim more
 * than 60 degrees off the fighter's facing (flat dot < 0.5) falls back to straight ahead.
 */
s32 EftAim_GetDirKeep(EftHitSrc *src, EftVec *out, s32 arg, s32 objId) {
    EftVec rot;
    EftVec fwd;
    EftVec flat;

    BtlCharApi_GetRot(objId, &rot);
    fwd.x = sinf(rot.y);
    fwd.y = 0.0f;
    fwd.z = cosf(rot.y);
    fwd.w = 1.0f;
    if (BtlCharApi_IsLockedOn(objId)) {
        if (src != NULL) {
            if (src->def->aimMode == 0) {
                BtlCharApi_CalcAimDir45(objId, arg, out);
            } else if (!(src->aimFlags & 2)) {
                BtlCharApi_CalcAimDir45(objId, arg, out);
                Vec4_Copy(&src->aimDir, out);
                src->aimFlags |= 2;
            } else {
                Vec4_Copy(out, &src->aimDir);
            }
            Vec3_Normalize(out, out);
            Vec4_Copy(&flat, out);
            flat.y = 0.0f;
            if (Vec3_Dot(&flat, &fwd) < 0.5f) {
                Vec4_Copy(out, &fwd);
            }
        }
    } else {
        Vec4_Copy(out, &fwd);
    }
    return 1;
}

/* Rotates v about a unit axis by angle (Rodrigues' formula); w is copied from v. */
void EftMath_RotateAboutAxis(EftVec *out, EftVec *v, EftVec *axis, f32 angle) {
    EftVec a;
    EftVec b;
    EftVec c;
    f32 cs = cosf(angle);
    f32 sn = sinf(angle);

    Vec3_Scale(&a, v, cs);
    Vec3_Cross(&b, axis, v);
    Vec3_Scale(&b, &b, sn);
    Vec3_Scale(&c, axis, Vec3_Dot(axis, v) * (1.0f - cs));
    Vec3_Add(out, &a, &b);
    Vec3_Add(out, out, &c);
    out->w = v->w;
}

/*
 * Homing: turns dir towards the opponent of objId by at most maxTurn radians. The target is the opponent's node
 * 0x11 led by half of its frame movement times the frames to impact (distance / speed, at most 15); a target
 * behind the projectile (dot <= 0) leaves dir unchanged, and so does a fighter that is not locked on.
 */
void EftAim_Home(EftVec *out, EftVec *pos, EftVec *dir, s32 objId, f32 speed, f32 maxTurn) {
    EftVec target;
    EftVec move;
    EftVec lead;
    EftVec newDir;
    EftVec cur;
    EftVec to;
    EftVec axis;
    s32 opp = BtlCharApi_GetOpponentObjId(objId);
    f32 frames;
    f32 t;
    f32 d;
    f32 angle;

    Vec3_Normalize(dir, dir);
    if (!BtlCharApi_IsLockedOn(objId)) {
        Vec4_Copy(out, dir);
        return;
    }
    BtlCharApi_GetNodePos(opp, 0x11, &target);
    Vec4_Sub(&to, &target, pos);
    frames = sqrtf(Vec3_Dot(&to, &to)) / speed;
    speed = frames;
    if (speed > 15.0f) {
        speed = 15.0f;
    }
    BtlCharApi_GetFrameMove(opp, &move);
    Vec4_Scale(&lead, &move, speed * 0.5f);
    Vec4_Add(&target, &target, &lead);
    Vec4_Sub(&to, &target, pos);
    Vec3_Normalize(&to, &to);
    Vec3_Normalize(&cur, dir);
    d = Vec3_Dot(&to, &cur);
    if (d > 0.0f) {
        if (d > 1.0f) {
            d = 1.0f;
        }
        angle = acosf(d);
        if (frames > 2.0f) {
            angle /= frames * 0.5f;
        }
        t = maxTurn;
        if (!(t < angle)) {
            t = angle;
        }
        Vec3_Cross(&axis, &cur, &to);
        Vec3_Normalize(&axis, &axis);
        EftMath_RotateAboutAxis(&newDir, &cur, &axis, t);
        Vec3_Normalize(&newDir, &newDir);
    } else {
        Vec4_Copy(&newDir, &cur);
    }
    Vec4_Copy(out, &newDir);
    Vec3_Normalize(out, out);
}

/* One projected vertex as ClipPoly_ProjectCur writes it (fixed-point screen x, y, depth, w). */
typedef struct EftGfxScr {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ s32 w;
} EftGfxScr;

/*
 * Draws a polygon (3 vertices on entry, up to 9 after clipping against the five view planes) as a fan of textured
 * triangles (0x132E80). The sort depth of each triangle is the average of its three projected depths >> 8
 * (mirrored to 0x1000 - z when flip is set) plus zOfs; depths are clamped to 0xFFFFFF.
 */
void EftGfx_DrawPolyAvgZ(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 flip, u64 tex, s32 zOfs) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftGfxVert portPoly[16];
    __builtin_memcpy(portPoly, verts, 3 * sizeof(EftGfxVert));
    verts = portPoly;
#endif
    EftGfxScr scr[9];
    EftVec col[9];
    EftVec *plane;
    s32 count = 3;
    s32 i;
    s32 z;
    
    plane = EftGfx_GetClipPlanes();
    for (i = 0; i < 5; i++) {
        count = ClipPoly_ClipPlane(verts, plane, count);
        plane++;
    }
    if (count != 0) {
        ClipPoly_ProjectCur((void *)scr, col, verts, count);
        for (i = 2; i < count; i++) {
            z = ((scr[0].z + scr[i - 1].z + scr[i].z) / 3) >> 8;
            if (flip) { z = 0x1000 - z; }
            if (scr[0].z > 0xFFFFFF) { scr[0].z = 0xFFFFFF; }
            if (scr[i - 1].z > 0xFFFFFF) { scr[i - 1].z = 0xFFFFFF; }
            if (scr[i].z > 0xFFFFFF) { scr[i].z = 0xFFFFFF; }
            EftPrim_DrawTriangle((void *)&scr[0], (void *)&scr[i - 1], (void *)&scr[i], &verts[0].uv, &verts[i - 1].uv, &verts[i].uv, &col[0],
                          &col[i - 1], &col[i], arg2, arg3, arg1, z + zOfs, tex);
        }
    }
}

/* Same with a fixed sort depth z (mirrored when flip is set); front forces the three depths to 0xFFFFFF. */
void EftGfx_DrawPolyFixedZ(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex, s32 z) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftGfxVert portPoly[16];
    __builtin_memcpy(portPoly, verts, 3 * sizeof(EftGfxVert));
    verts = portPoly;
#endif
    EftGfxScr scr[9];
    EftVec col[9];
    EftVec *plane;
    s32 count = 3;
    s32 i;
    
    plane = EftGfx_GetClipPlanes();
    for (i = 0; i < 5; i++) {
        count = ClipPoly_ClipPlane(verts, plane, count);
        plane++;
    }
    if (count != 0) {
        ClipPoly_ProjectCur((void *)scr, col, verts, count);
        for (i = 2; i < count; i++) {
            if (flip) { z = 0x1000 - z; }
            if (scr[0].z > 0xFFFFFF) { scr[0].z = 0xFFFFFF; }
            if (scr[i - 1].z > 0xFFFFFF) { scr[i - 1].z = 0xFFFFFF; }
            if (scr[i].z > 0xFFFFFF) { scr[i].z = 0xFFFFFF; }
            if (front) { scr[0].z = 0xFFFFFF; scr[i - 1].z = 0xFFFFFF; scr[i].z = 0xFFFFFF; }
            EftPrim_DrawTriangle((void *)&scr[0], (void *)&scr[i - 1], (void *)&scr[i], &verts[0].uv, &verts[i - 1].uv, &verts[i].uv, &col[0],
                          &col[i - 1], &col[i], arg2, arg3, arg1, z, tex);
        }
    }
}

/* Same as EftGfx_DrawPolyAvgZ with the option to force the three depths to 0xFFFFFF (front). */
void EftGfx_DrawPolyAvgZFront(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                              s32 zOfs) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftGfxVert portPoly[16];
    __builtin_memcpy(portPoly, verts, 3 * sizeof(EftGfxVert));
    verts = portPoly;
#endif
    EftGfxScr scr[9];
    EftVec col[9];
    EftVec *plane;
    s32 count = 3;
    s32 i;
    s32 z;
    
    plane = EftGfx_GetClipPlanes();
    for (i = 0; i < 5; i++) {
        count = ClipPoly_ClipPlane(verts, plane, count);
        plane++;
    }
    if (count != 0) {
        ClipPoly_ProjectCur((void *)scr, col, verts, count);
        for (i = 2; i < count; i++) {
            z = ((scr[0].z + scr[i - 1].z + scr[i].z) / 3) >> 8;
            if (flip) { z = 0x1000 - z; }
            if (scr[0].z > 0xFFFFFF) { scr[0].z = 0xFFFFFF; }
            if (scr[i - 1].z > 0xFFFFFF) { scr[i - 1].z = 0xFFFFFF; }
            if (scr[i].z > 0xFFFFFF) { scr[i].z = 0xFFFFFF; }
            if (front) { scr[0].z = 0xFFFFFF; scr[i - 1].z = 0xFFFFFF; scr[i].z = 0xFFFFFF; }
            EftPrim_DrawTriangle((void *)&scr[0], (void *)&scr[i - 1], (void *)&scr[i], &verts[0].uv, &verts[i - 1].uv, &verts[i].uv, &col[0],
                          &col[i - 1], &col[i], arg2, arg3, arg1, z + zOfs, tex);
        }
    }
}

/* Same with the average depth multiplied by zScale. */
void EftGfx_DrawPolyScaledZ(EftGfxVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                              f32 zScale) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftGfxVert portPoly[16];
    __builtin_memcpy(portPoly, verts, 3 * sizeof(EftGfxVert));
    verts = portPoly;
#endif
    EftGfxScr scr[9];
    EftVec col[9];
    EftVec *plane;
    s32 count = 3;
    s32 i;
    s32 z;
    
    plane = EftGfx_GetClipPlanes();
    for (i = 0; i < 5; i++) {
        count = ClipPoly_ClipPlane(verts, plane, count);
        plane++;
    }
    if (count != 0) {
        ClipPoly_ProjectCur((void *)scr, col, verts, count);
        for (i = 2; i < count; i++) {
            z = ((scr[0].z + scr[i - 1].z + scr[i].z) / 3) >> 8;
            if (flip) { z = 0x1000 - z; }
            if (scr[0].z > 0xFFFFFF) { scr[0].z = 0xFFFFFF; }
            if (scr[i - 1].z > 0xFFFFFF) { scr[i - 1].z = 0xFFFFFF; }
            if (scr[i].z > 0xFFFFFF) { scr[i].z = 0xFFFFFF; }
            if (front) { scr[0].z = 0xFFFFFF; scr[i - 1].z = 0xFFFFFF; scr[i].z = 0xFFFFFF; }
            EftPrim_DrawTriangle((void *)&scr[0], (void *)&scr[i - 1], (void *)&scr[i], &verts[0].uv, &verts[i - 1].uv, &verts[i].uv, &col[0],
                          &col[i - 1], &col[i], arg2, arg3, arg1, (s32)(z * zScale), tex);
        }
    }
}

/* GS XYZF2 register. */
typedef struct EftGfxXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftGfxXyzf;

typedef struct EftGfxRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftGfxRgbaq;

typedef struct EftGfxSt {
    f32 s, t;
} EftGfxSt;

/* DMA tag + REGLIST GIF tag (14 registers): PRIM, TEX0, four (RGBAQ, ST, XYZF2): one gouraud textured strip
   (the same packet as EftBStripPkt of eft_b.c). */
typedef struct EftGfxStripPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000008: NEXT, 8 quadwords */
    /* 0x04 */ struct EftGfxStripPkt *next;
    /* 0x08 */ u32 vif0;         /* 0x10000000 */
    /* 0x0C */ u32 vif1;         /* 0x50000008: DIRECT, 8 quadwords */
    /* 0x10 */ u64 gifTag;       /* 0xE400000000008001: NLOOP 1, EOP, REGLIST, 14 registers */
    /* 0x18 */ u64 regs;         /* 0x42142142142160; + 0x10 for context 2 (TEX0_1 becomes TEX0_2) */
    /* 0x20 */ u64 prim;         /* 0x5C (gouraud textured triangle strip, alpha blended) | context << 9 */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftGfxRgbaq rgbaq0;
    /* 0x38 */ EftGfxSt st0;
    /* 0x40 */ EftGfxXyzf xyz0;
    /* 0x48 */ EftGfxRgbaq rgbaq1;
    /* 0x50 */ EftGfxSt st1;
    /* 0x58 */ EftGfxXyzf xyz1;
    /* 0x60 */ EftGfxRgbaq rgbaq2;
    /* 0x68 */ EftGfxSt st2;
    /* 0x70 */ EftGfxXyzf xyz2;
    /* 0x78 */ EftGfxRgbaq rgbaq3;
    /* 0x80 */ EftGfxSt st3;
    /* 0x88 */ EftGfxXyzf xyz3;
} EftGfxStripPkt; /* size 0x90 */

extern s32 Vu0Cur_ProjectPoint(EftGfxScr *out, EftVec *pos); /* projects a point to GS 12.4 screen x, y, depth, w */
extern void Vec4_Mul(EftVec *dst, EftVec *a, EftVec *b);      /* per-component product */
extern void Vec4_ToInt(EftGfxScr *dst, EftVec *src);          /* float vector to fixed point */
extern s32 EftPrim_IsOffScreen(s32 x, s32 y, s32 z);          /* eft_b.c: outside the GS drawing area */

/*
 * Draws a camera-facing textured quad at a world position, written straight into the display list.
 *   - pos->w = 1; Vu0Cur_ProjectPoint projects pos into fixed-point screen x, y, z, w. The half size in pixels is
 *     (s32)w (h) * (s32)(gBtlCamView + 0x234 (screen distance) * 4096 / screen w) >> 12; nothing is drawn below 2
 *     pixels in either direction.
 *   - the two corner offsets (w, h) and (-w, h) are rotated about Z by rot (Mtx_StoreIdentity + Mtx_RotateZ),
 *     scaled by the aspect vector (1, 7/6, 1, 1) and converted to fixed point; the quad is dropped when any of the
 *     four corners centre -/+ a, centre +/- b is off screen (EftPrim_IsOffScreen).
 *   - takes 0x90 bytes at gOtCur: a 4-vertex gouraud textured, alpha-blended triangle strip (context 2 when
 *     layer >= 2), TEX0 = tex0, per vertex RGBAQ (the four floats of color converted to bytes, q = 1), ST
 *     ((u0,v0), (u0,v1), (u1,v0), (u1,v1)) and XYZF2 (centre - a, centre + b, centre - b, centre + a; depth
 *     forced to 0xFFFFFF when front is set; fog 0xFF).
 *   - links the packet into the ordering table at depth slot (projected z >> 8, taken BEFORE front is applied,
 *     clamped to 0..0xFFF) and chain layer (layer - 2 for layers >= 2).
 * It reads the camera view only and writes nothing but pos->w and display-list memory.
 * The parameter order (floats in front of layer / front / tex0) is the one three callers declare (eft_aa.c,
 * eft_ab_c.c, eft_c.c); it decides which saved float register u1 and v1 get.
 */
void EftGfx_DrawSprite(EftVec *pos, EftVec *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 layer,
                       s32 front, u64 tex0) {
    EftMtx m;
    EftVec aspect;
    EftVec a;
    EftVec b;
    EftGfxScr scr __attribute__((aligned(16)));
    EftGfxScr ia __attribute__((aligned(16)));
    EftGfxScr ib __attribute__((aligned(16)));
    f32 scale = *(f32 *)(gBtlCamView + 0x234);
    s32 sw;
    s32 sh;
    s32 z;
    s32 x0, y0, x1, y1, x2, y2, x3, y3;
    s32 zz;
    s32 abe = 1;
    s32 ctx;
    s32 l;
    EftGfxStripPkt *p;
    OtEntry *e;

#ifdef PORT
    Vec4_Set(&aspect, 1.0f, 1.1666667f * Port_WideFactor(), 1.0f, 1.0f); /* pixels are wider still at 16:9 */
#else
    Vec4_Set(&aspect, 1.0f, 1.1666667f, 1.0f, 1.0f);
#endif
    pos->w = 1.0f;
    Vu0Cur_ProjectPoint(&scr, pos);
    sw = w;
    sh = h;
    scale *= 4096.0f;
    z = scr.z >> 8;
    scale /= scr.w;
    sw = (sw * (s32)scale) >> 12;
    sh = (sh * (s32)scale) >> 12;
    if (sw < 2 || sh < 2) {
        return;
    }
    w = sw;
    h = sh;
    Vec4_Set(&a, w, h, 0.0f, 1.0f);
    Vec4_Set(&b, -w, h, 0.0f, 1.0f);
    Mtx_StoreIdentity(&m);
    Mtx_RotateZ(&m, &m, rot);
    Mtx_MulVec4(&a, &m, &a);
    Mtx_MulVec4(&b, &m, &b);
    Vec4_Mul(&a, &a, &aspect);
    Vec4_Mul(&b, &b, &aspect);
    Vec4_ToInt(&ia, &a);
    Vec4_ToInt(&ib, &b);
    if (EftPrim_IsOffScreen(scr.x - ia.x, scr.y - ia.y, scr.z)) {
        return;
    }
    if (EftPrim_IsOffScreen(scr.x + ib.x, scr.y + ib.y, scr.z)) {
        return;
    }
    if (EftPrim_IsOffScreen(scr.x - ib.x, scr.y - ib.y, scr.z)) {
        return;
    }
    if (EftPrim_IsOffScreen(scr.x + ia.x, scr.y + ia.y, scr.z)) {
        return;
    }
    if (front) {
        scr.z = 0xFFFFFF;
    }
    p = (EftGfxStripPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    x0 = scr.x - ia.x;
    y0 = scr.y - ia.y;
    x1 = scr.x + ib.x;
    y1 = scr.y + ib.y;
    x2 = scr.x - ib.x;
    y2 = scr.y - ib.y;
    x3 = scr.x + ia.x;
    y3 = scr.y + ia.y;
    zz = scr.z;
    if (p == NULL) {
        return;
    }
    ctx = layer >= 2;
    p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x1C;
    p->dmaTag = 0x20000008;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000008;
    p->gifTag = 0xE400000000008001;
    p->regs = 0x42142142142160 + (ctx << 4);
    p->next = NULL;
    p->rgbaq0.r = color->x;
    p->rgbaq0.g = color->y;
    p->rgbaq0.b = color->z;
    p->rgbaq0.a = color->w;
    p->rgbaq0.q = 1.0f;
    p->rgbaq1.r = color->x;
    p->rgbaq1.g = color->y;
    p->rgbaq1.b = color->z;
    p->rgbaq1.a = color->w;
    p->rgbaq1.q = 1.0f;
    p->rgbaq2.r = color->x;
    p->rgbaq2.g = color->y;
    p->rgbaq2.b = color->z;
    p->rgbaq2.a = color->w;
    p->rgbaq2.q = 1.0f;
    p->rgbaq3.r = color->x;
    p->rgbaq3.g = color->y;
    p->rgbaq3.b = color->z;
    p->rgbaq3.a = color->w;
    p->rgbaq3.q = 1.0f;
    p->st0.s = u0;
    p->st0.t = v0;
    p->st1.s = u0;
    p->st1.t = v1;
    p->st2.s = u1;
    p->st2.t = v0;
    p->st3.s = u1;
    p->st3.t = v1;
    p->xyz0.x = x0;
    p->xyz0.y = y0;
    p->xyz0.z = zz;
    p->xyz0.f = 0xFF;
    p->xyz1.x = x1;
    p->xyz1.y = y1;
    p->xyz1.z = zz;
    p->xyz1.f = 0xFF;
    p->xyz2.x = x2;
    p->xyz2.y = y2;
    p->xyz2.z = zz;
    p->xyz2.f = 0xFF;
    p->xyz3.x = x3;
    p->xyz3.y = y3;
    p->xyz3.z = zz;
    p->xyz3.f = 0xFF;
    p->tex0 = tex0;
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    if (z < 0) {
        e = &gOtZ[0].layer[l];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[z].layer[l];
    }
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
}
