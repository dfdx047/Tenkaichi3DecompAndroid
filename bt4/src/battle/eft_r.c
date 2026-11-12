#include "common.h"
#include "battle/eft_r.h"

/*
 * Effect modules 0x174A70..0x178AB0 (see include/battle/eft_r.h for the list and the layouts).
 *
 * Task classes are {update, init, term, post-update, reset, draw}. The per-character sub-managers (kinds 0..9 of
 * gEftCharKindClass, 0x2C3BE0, created by 0x171F10 for each character whose effect pack has the entry) register
 * their child list with EftChar_SetList(chr, kind, list); EftChar_GetList(objId, kind) returns it. Kinds here:
 * 0 charge aura, 1 and 2 ki blasts, 3 ki blast charge glow. A character with a type 2 ki blast gets the EftKiObj
 * manager for kind 1 instead, one with a type 3 ki blast the EftKiBomb manager for kind 2 and no kind 3 (read from
 * the disassembly of 0x171F10 / 0x172128).
 *
 * Random draws in this file:
 *   BtlChar_RandF (fighter generator)  two per deflection or reflection, inside BtlCharApi_GetDeflectDir
 *                                      (EftKiBlast_Turn, EftKiObj_Turn): the blast's new direction. SIMULATION.
 *   Rand_FloatRange (VU0 R register)   three per bomb in EftKiBomb_Launch: the throw direction. SIMULATION.
 *   libc rand()                        two per bomb / object at launch (spin rate and sense), one per bomb
 *                                      explosion (which of two sounds), one when a bomb comes to rest (its
 *                                      resting angle). Appearance and sound only.
 *
 * Emitted data: .lit4 0x2FCC0C..0x2FCC40 (19.35, 0.8, 0.7, 19.35, 0.2, -0.2, 2147483647, 0.3, pi, 1500, pi, 0.7,
 * 2147483647; bits checked against the original) and .rodata 0x2ECD40..0x2ECD5C (the two initialisers of
 * EftKiBomb_HandleHits: {0, -1, 0, 1} and {0, 90, 270}).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sqrtf(f32 x);

extern void Vec4_Set(EftRVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(EftRVec *dst, EftRVec *src);
extern void Vec4_Sub(EftRVec *dst, EftRVec *a, EftRVec *b);
extern void Vec3_Add(EftRVec *dst, EftRVec *a, EftRVec *b);
extern void Vec3_Scale(EftRVec *dst, EftRVec *src, f32 scale);
extern void Vec3_Normalize(EftRVec *dst, EftRVec *src);
extern void Mtx_StoreIdentity(EftRMtx *m);
extern void Mtx_SetTrans(EftRMtx *m, EftRVec *pos);               /* sets the translation row */
extern void Mtx_Mul(EftRMtx *out, EftRMtx *a, EftRMtx *b);   /* matrix product */
extern void Mtx_RotateX(EftRMtx *out, EftRMtx *in, f32 angle);   /* rotation about one axis */
extern void Vec3_Copy(EftRVec *dst, EftRVec *src);             /* copies x, y, z */
extern void Vec3_Lerp(EftRVec *out, EftRVec *a, EftRVec *b, f32 t); /* a + (b - a) * t */
extern f32 Vec3_Dist(EftRVec *a, EftRVec *b);                  /* distance */
extern f32 Rand_FloatRange(f32 a, f32 b);

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void *BtlTask_CreateChildList(EftRTask *task, s32 count, s32 workSize);
extern EftRTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftRTask *task);                 /* kills the task */
extern void BtlTask_SetOwnerTag(EftRTask *task, s32 bits);       /* 0x800 / 0x1000: task of character 0 / 1 */
extern EftRTask *BtlTask_GetParent(EftRTask *task);            /* the manager task */
extern s64 EftVram_AddImage(EftRTexAnim *tex, s32 a, s32 b);
extern s64 EftVram_AddClut(EftRTexAnim *tex);

extern u64 *Battle_GetWork(void);
extern void BtlScene_Reset(s32 mode);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern s32 BtlScene_IsAnyCharStopped(void);
extern f32 BtlScene_GetCharScale(s32 chr);
extern s32 *BtlScene_GetCharPackEntry(s32 chr, s32 idx);
extern void *BtlScene_GetPackEntry(s32 *base, s32 idx);
extern f32 BtlStage_GetBottom(void);

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, EftRVec *out);
extern void BtlCharApi_GetPos(s32 objId, EftRVec *out);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern s32 BtlCharApi_IsLockedOn(s32 objId);
extern s32 BtlCharApi_IsChargingKiBlast(s32 objId);
extern s32 BtlCharApi_IsInClashA(s32 objId);
extern f32 BtlCharApi_GetClashBias(void);
extern void BtlCharApi_GetDeflectDir(s32 objId, EftRVec *out);
extern void BtlCharApi_PlaySoundAt(EftRVec *pos, s32 kind, s32 id, f32 near, f32 far);

extern void EftTechEvt_RequestExpire(s32 objId);
extern s32 ScrWarp_Spawn(s32 view, EftRVec *pos, f32 seconds, f32 radius, f32 width, f32 speed, f32 jitter);
extern f32 EftMath_WrapAngle(f32 angle);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftAim_Home(EftRVec *out, EftRVec *pos, EftRVec *dir, s32 objId, f32 speed, f32 maxTurn);
#define EftAim_Home(out, pos, dir, speed, maxTurn, objId) EftAim_Home(out, pos, dir, objId, speed, maxTurn)
#else
extern void EftAim_Home(EftRVec *out, EftRVec *pos, EftRVec *dir, f32 speed, f32 maxTurn, s32 objId);
#endif
extern EftRRec *EftHit_GetNew(void);
extern void EftHit_Add(EftRRec *rec);
extern EftRSphere *EftHitArena_AllocSphere(void);
extern void EftHit_SetShapeSpheres(EftRRec *rec, EftRSphere *a, EftRSphere *b);
extern void ColSphere_Set(EftRSphere *sphere, EftRVec *pos, f32 radius);

extern void EftEmit_LoadSet(void *arg, void *set, s32 a2, s32 *pack, s32 a4, s32 a5);
extern void EftEmit_FreeSet(void *set);
extern void EftEmit_BeginFrame(void *set);
extern s32 EftEmit_GetEndFrames(EftRSet *set);
extern void EftEmit_InitState(EftRSet *set, EftRState *state);
extern void EftEmit_TermState(EftRSet *set, EftRState *state);
extern s32 EftEmit_GetFlagsFromMask(EftRSet *set, EftRState *state, s32 objId, s32 part, s32 sub, s32 end, s32 fast,
                                    s32 mask);
extern s32 EftEmit_GetResetFlags(EftRSet *set, EftRState *state, s32 part, s32 sub);
extern void EftEmit_Spawn(EftRSet *set, EftRState *state, EftRNodes *nodes, EftRVec *pos, EftRVec *dir, s32 objId,
                          s32 node, s32 arg7, s32 part, s32 sub, s32 flags, f32 scale);
extern void EftEmit_KillAll(EftRSet *set, EftRState *state);
extern s32 EftEmit_UpdateAlive(EftRSet *set, EftRState *state);
extern void EftEmit_SetNode(EftRNodes *nodes, s32 slot, s32 node, EftRVec *pos);
extern void EftEmit_RefreshFixedNodes(s32 objId, EftRNodes *nodes);
extern f32 EftEmit_GetTrailWidth(EftRState *state);
extern s32 EftEmit_HasWidth2(EftRSet *set);
extern f32 EftEmit_GetWidth2(EftRState *state);

extern void EftChar_SetList(s32 chr, s32 kind, void *list);  /* registers a sub-manager's child list */
extern void *EftChar_GetList(s32 objId, s32 kind);           /* the child list of a sub-manager */
#ifdef PORT /* PC build: by address, as the definition takes it (it only reads the struct; the PS2 passes a by-value struct of this size by address) */
extern s32 EftImpact_SpawnBlast(EftRBlastFx *arg, f32 scaleA, f32 scaleB);
#define EftImpact_SpawnBlast(arg, scaleA, scaleB) EftImpact_SpawnBlast(&(arg), scaleA, scaleB)
#else
extern s32 EftImpact_SpawnBlast(EftRBlastFx arg, f32 scaleA, f32 scaleB); /* explosion effect */
#endif
extern void EftGndDust_SpawnImpact(s32 objId, EftRVec *pos, f32 scale); /* ground impact effect */
extern void EftCharSlot_Set0(s32 objId, EftRTask *task);      /* per-fighter task slots (0x19BA98..0x19BC58) */
extern void EftCharSlot_Clear0(s32 objId);
extern EftRTask *EftCharSlot_Get0(s32 objId);
extern void EftCharSlot_Set1(s32 objId, EftRTask *task);
extern void EftCharSlot_Clear1(s32 objId);
extern EftRTask *EftCharSlot_Get1(s32 objId);
extern EftRTask *EftCharSlot_Get3(s32 objId);
extern s32 EftObj_Create(void *buf, void *model);          /* creates a model instance; -1 when none is free */
extern void EftObj_Destroy(s32 handle);                     /* frees it */
extern void EftObj_SetMtx(s32 handle, EftRMtx *mtx);       /* sets its matrix */
extern void EftObj_SetVisible(s32 handle, s32 shown);
extern s32 EftKiObj_Step(EftRTask *task);                  /* motion of the type 2 object (next file) */

extern EftRClass gEftStruggleClass;
extern EftRClass gEftClashSparkClass;
extern EftRClass gEftChargeClass;
extern EftRClass gEftKiBlastClass;
extern EftRClass gEftBlastChargeClass;
extern EftRClass gEftKiBombClass;


#define EFT_TASK_IS_DEAD(task) ((u8)((task)->flags & 1))

/* Work of the task EftCharaFx_Stop ends (module 0x174348..0x174A70, not in this file). */
typedef struct EftCharaFxWork {
    /* 0x000 */ u8 unk0[0x830];
    /* 0x830 */ u32 flags;   /* 1 active, 2 ending */
} EftCharaFxWork;

/* Asks the fighter's "chara fx" task to end. Returns 1 when it was running. */
s32 EftCharaFx_Stop(s32 objId) {
    EftRTask *task = EftCharSlot_Get3(objId);
    EftCharaFxWork *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & 1)) {
        return 0;
    }
    w->flags |= 2;
    return 1;
}

/* ---- beam struggle ------------------------------------------------------------------------------------------ */

/* Starts the struggle between two technique records that met head-on (called by the hit pass, 0x1B0910, after it
   set task flag 0x100 on both). The task gets copies of both records, object 0's first. Returns how far along the
   line between the two start points record 0 got. */
f32 EftStruggle_Start(EftRVec *pos, EftRRec *a, EftRRec *b) {
    EftStruggleArg arg;
    f32 d;

    memset(&arg, 0, sizeof(EftStruggleArg));
    Vec4_Copy(&arg.pos, pos);
    if (a->objId == 0) {
        arg.rec[0] = *a;
        arg.rec[1] = *b;
    } else {
        arg.rec[0] = *b;
        arg.rec[1] = *a;
    }
    arg.rec[1].pose.start.w = 1.0f;
    arg.rec[0].pose.start.w = 1.0f;
    d = Vec3_Dist(&arg.rec[1].pose.start, &arg.rec[0].pose.start);
    arg.ratio = Vec3_Dist(&arg.rec[0].pose.start, &arg.rec[0].pose.pos) / d;
    BtlTaskList_AddTail(gEftStruggle->list, &gEftStruggleClass, &arg);
    return arg.ratio;
}

/* Whether a struggle task exists. */
s32 EftStruggle_IsActive(void) {
    if (gEftStruggle == NULL) {
        return 0;
    }
    return gEftStruggle->list->first != NULL;
}

/* Ends the struggle (BtlClash_UpdateA). winner 0 / 1: the other fighter's technique timers are expired and its
   effect layer is reset (scene layer 2 + character), which kills its beam; -1: both. */
void EftStruggle_End(s32 winner) {
    EftRTask *task;
    EftStruggle *w;
    s32 layer;
    s32 loser;

    if (gEftStruggle == NULL) {
        return;
    }
    task = gEftStruggle->list->first;
    if (task == NULL) {
        return;
    }
    if (EFT_TASK_IS_DEAD(task)) {
        return;
    }
    if (task->cls->update != EftStruggle_Update) {
        return;
    }
    w = task->work;
    if (w->flags & EFT_STRUGGLE_END) {
        return;
    }
    w->flags |= EFT_STRUGGLE_END;
    if (winner == -1) {
        EftTechEvt_RequestExpire(0);
        EftTechEvt_RequestExpire(1);
        BtlScene_Reset(2);
        BtlScene_Reset(3);
    } else {
        if (winner == 0) {
            loser = 1;
            layer = 3;
        } else {
            loser = 0;
            layer = 2;
        }
        EftTechEvt_RequestExpire(loser);
        BtlScene_Reset(layer);
    }
}

/* Sets where the struggle point is between the two fighters, clamped to 0..1 (no caller: the task recomputes it
   from BtlCharApi_GetClashBias every frame). */
void EftStruggle_SetBias(f32 bias) {
    EftRTask *task;
    EftStruggle *w;

    if (gEftStruggle == NULL) {
        return;
    }
    task = gEftStruggle->list->first;
    if (task == NULL) {
        return;
    }
    if (EFT_TASK_IS_DEAD(task)) {
        return;
    }
    if (task->cls->update != EftStruggle_Update) {
        return;
    }
    w = task->work;
    w->bias = bias;
    if (bias > 1.0f) {
        w->bias = 1.0f;
    }
    if (w->bias < 0.0f) {
        w->bias = 0.0f;
    }
}

/* The struggle point's place between the fighters, 0 when there is none (no caller). */
f32 EftStruggle_GetBias(void) {
    EftRTask *task;

    if (gEftStruggle == NULL) {
        return 0.0f;
    }
    task = gEftStruggle->list->first;
    if (task == NULL) {
        return 0.0f;
    }
    if (EFT_TASK_IS_DEAD(task)) {
        return 0.0f;
    }
    if (task->cls->update != EftStruggle_Update) {
        return 0.0f;
    }
    return ((EftStruggle *)task->work)->bias;
}

/* Copies the struggle point (no caller). */
void EftStruggle_GetPos(EftRVec *out) {
    EftRTask *task;

    if (gEftStruggle == NULL) {
        return;
    }
    task = gEftStruggle->list->first;
    if (task == NULL) {
        return;
    }
    if (EFT_TASK_IS_DEAD(task)) {
        return;
    }
    if (task->cls->update != EftStruggle_Update) {
        return;
    }
    Vec4_Copy(out, &((EftStruggle *)task->work)->pos);
}

/* Moves the struggle point (no caller). */
void EftStruggle_SetPos(EftRVec *pos) {
    EftRTask *task;

    if (gEftStruggle == NULL) {
        return;
    }
    task = gEftStruggle->list->first;
    if (task == NULL) {
        return;
    }
    if (EFT_TASK_IS_DEAD(task)) {
        return;
    }
    if (task->cls->update != EftStruggle_Update) {
        return;
    }
    Vec4_Copy(&((EftStruggle *)task->work)->pos, pos);
}

/* Camera distance of the struggle's orbit cut for a fighter (BtlClash_SetOrbitCut): mode 0 gives
   (20 + 5 * power) * body scale with the power at least 0.5; modes 1 and 2 give 0. */
f32 EftStruggle_GetCamDist(s32 objId, s32 mode) {
    f32 ret = 0.0f;
    f32 power = 1.0f;
    f32 scale;

    if (gEftStruggle == NULL) {
        return ret;
    }
    if (gEftStruggle->side[0].objId == objId) {
        power = gEftStruggle->side[0].power;
    }
    if (gEftStruggle->side[1].objId == objId) {
        power = gEftStruggle->side[1].power;
    }
    if (power <= 0.5f) {
        power = 0.5f;
    }
    scale = BtlScene_GetCharScale(objId);
    switch (mode) {
    case 0:
        ret = (gEftStruggle->camDist + power * 5.0f) * scale;
        break;
    case 1:
        break;
    case 2:
        return ret;
    default:
        ret = 0.0f;
        break;
    }
    return ret;
}

/* Camera distance of the struggle's middle cut (BtlClash_SetMidCut): 40 + 20 * the larger power. */
f32 EftStruggle_GetMidDist(void) {
    if (gEftStruggle == NULL) {
        return 0.0f;
    }
    return gEftStruggle->midDist + gEftStruggle->maxPower * 20.0f;
}

/* Sets one side's power and object id (the index), and the larger of the two powers (no caller). */
void EftStruggle_SetPower(s32 side, f32 power) {
    if (gEftStruggle != NULL) {
        gEftStruggle->side[side].power = power;
        gEftStruggle->side[side].objId = side;
        gEftStruggle->maxPower = gEftStruggle->side[0].power > gEftStruggle->side[1].power
                                     ? gEftStruggle->side[0].power
                                     : gEftStruggle->side[1].power;
    }
}

/* One side's power (no caller). */
f32 EftStruggle_GetPower(s32 side) {
    if (gEftStruggle == NULL) {
        return 0.0f;
    }
    return gEftStruggle->side[side].power;
}

/* Manager init: the globals and a child list of one task. */
void EftStruggleMgr_Init(EftRTask *task) {
    gEftStruggle = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftStruggleMgr));
    memset(gEftStruggle, 0, sizeof(EftStruggleMgr));
    gEftStruggle->camDist = 20.0f;
    gEftStruggle->midDist = 40.0f;
    gEftStruggle->maxPower = 1.0f;
    gEftStruggle->side[0].power = 1.0f;
    gEftStruggle->side[0].objId = 0;
    gEftStruggle->side[1].power = 1.0f;
    gEftStruggle->side[1].objId = 1;
    gEftStruggle->list = BtlTask_CreateChildList(task, 1, sizeof(EftStruggle));
}

/* Manager term. */
void EftStruggleMgr_Term(void) {
    if (gEftStruggle != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftStruggle);
        gEftStruggle = NULL;
    }
}

/* Manager update: nothing. */
void EftStruggleMgr_Update(void) {
}

/* Task init: keeps the two records, publishes both sides' powers (technique definition +0x88), places the
   struggle point from the fighters' clash bias and starts the spark there. */
/* The comparison of the two radii decides nothing: both arms of the `if` hold the same code (each with its own
 * locals), the compiler merges them after register allocation and only the compare is left. The arms must hold the
 * whole block, down to maxPower, with block-local variables: that is what makes the sources and definitions
 * block-local values (v0 / a1 / v1 / a2). */
void EftStruggle_Init(EftRTask *task, EftStruggleArg *arg) {
    EftClashSparkArg spark;
    EftStruggle *w = task->work;
    EftRRec *r0;
    EftRRec *r1;
    f32 p0;
    f32 p1;
    f32 one;

    memset(w, 0, sizeof(EftStruggle));
    w->arg = *arg;
    w->bias = w->arg.ratio;
    r0 = &w->arg.rec[0];
    r1 = &w->arg.rec[1];
    p0 = EftStruggle_GetRecRadius(r0);
    p1 = EftStruggle_GetRecRadius(r1);
    if (p1 < p0) {
        EftRSrc *s0 = r0->src;
        EftRSrc *s1 = r1->src;
        EftRDef *d0 = s0->def;
        EftRDef *d1 = s1->def;

        gEftStruggle->side[0].power = d0->power;
        gEftStruggle->side[0].objId = r0->objId;
        gEftStruggle->side[1].power = d1->power;
        gEftStruggle->side[1].objId = r1->objId;
        gEftStruggle->maxPower = gEftStruggle->side[0].power > gEftStruggle->side[1].power
                                     ? gEftStruggle->side[0].power
                                     : gEftStruggle->side[1].power;
    } else {
        EftRSrc *s0 = r0->src;
        EftRSrc *s1 = r1->src;
        EftRDef *d0 = s0->def;
        EftRDef *d1 = s1->def;

        gEftStruggle->side[0].power = d0->power;
        gEftStruggle->side[0].objId = r0->objId;
        gEftStruggle->side[1].power = d1->power;
        gEftStruggle->side[1].objId = r1->objId;
        gEftStruggle->maxPower = gEftStruggle->side[0].power > gEftStruggle->side[1].power
                                     ? gEftStruggle->side[0].power
                                     : gEftStruggle->side[1].power;
    }
    one = 1.0f;
    w->bias = (BtlCharApi_GetClashBias() + one) * 0.5f;
    if (w->bias < 0.0f) {
        w->bias = 0.0f;
    }
    if (w->bias > one) {
        w->bias = one;
    }
    Vec3_Lerp(&w->pos, &w->arg.rec[1].pose.start, &w->arg.rec[0].pose.start, w->bias);
    memset(&spark, 0, sizeof(EftClashSparkArg));
    Vec4_Copy(&spark.pos, &w->pos);
    Vec3_Normalize(&spark.dir, &w->arg.rec[0].pose.dir);
    spark.scale = one;
    spark.kind = 0;
    spark.objId = 0;
    w->spark = EftClashSpark_Create(spark);
}

/* Task term: lets the spark end. */
void EftStruggle_Term(EftRTask *task) {
    EftStruggle *w = task->work;

    EftClashSpark_Stop(w->spark);
}

/* Task update: steps unless the battle is paused (battle flag 0x100). */
void EftStruggle_Update(EftRTask *task) {
    if (!(Battle_GetWork()[0x19F0 / 8] & 0x100)) {
        EftStruggle_Step(task);
    }
}

/* Task reset: dies. */
void EftStruggle_Reset(EftRTask *task) {
    BtlTask_SetDead(task);
}

/* Task draw: nothing. */
void EftStruggle_Draw(void) {
}

/* Radius of a record's current volume (sphere or box). */
f32 EftStruggle_GetRecRadius(EftRRec *rec) {
    switch (rec->shapeType) {
    case 0:
        return ((EftRSphere *)rec->shapeA)->radius;
    case 1:
        return ((EftRBox *)rec->shapeA)->radius;
    }
    return 0.0f;
}

/* One frame of the struggle: ends when either fighter left clash A; otherwise moves the struggle point to the
   clash bias and writes each beam's head (its task's hit position) at power * definition +0x30 * 13 from the point
   towards that beam's start. */
void EftStruggle_Step(EftRTask *task) {
    EftRVec v;
    EftStruggle *w = task->work;
    EftRRec *r0;
    EftRRec *r1;
    f32 one;
    f32 t;
    f32 p0;
    f32 p1;
    f32 k;
    f32 tmp;

    w->frames++;
    if (!BtlCharApi_IsInClashA(0) || !BtlCharApi_IsInClashA(1)) {
        w->flags |= EFT_STRUGGLE_END;
    }
    if (w->flags & EFT_STRUGGLE_END) {
        BtlTask_SetDead(task);
        return;
    }
    one = 1.0f;
    EftClashSpark_SetPos(w->spark, &w->pos);
    r0 = &w->arg.rec[0];
    EftClashSpark_SetScale(w->spark, one);
    r1 = &w->arg.rec[1];
    t = one;
    p0 = gEftStruggle->side[0].power;
    k = 13.0f;
    p1 = gEftStruggle->side[1].power;
    tmp = p0;
    if (gEftStruggle->side[0].objId != r0->objId) {
        p0 = p1;
        p1 = tmp;
    }
    w->bias = (BtlCharApi_GetClashBias() + t) * 0.5f;
    if (w->bias < 0.0f) {
        w->bias = 0.0f;
    }
    if (w->bias > t) {
        w->bias = t;
    }
    Vec3_Lerp(&w->pos, &r1->pose.start, &r0->pose.start, w->bias);
    w->pos.w = one;
    t = r0->src->def->unk30;
    t = p0 * t * k;
    Vec4_Sub(&v, &r0->pose.start, &w->pos);
    Vec3_Normalize(&v, &v);
    Vec3_Scale(&v, &v, t);
    Vec3_Add(&r0->task->pos, &w->pos, &v);
    t = r1->src->def->unk30;
    t = p1 * t * k;
    Vec4_Sub(&v, &r1->pose.start, &w->pos);
    Vec3_Normalize(&v, &v);
    Vec3_Scale(&v, &v, t);
    Vec3_Add(&r1->task->pos, &w->pos, &v);
}

/* ---- clash spark -------------------------------------------------------------------------------------------- */

/* Steps the emitters of the set and spawns their particles at the spark. */
void EftClashSpark_SpawnParts(s32 objId, EftRTask *task, EftRSet *set) {
    EftClashSpark *w = task->work;
    s32 part;
    s32 sub;

    for (part = 0; part < 0x13; part++) {
        if (*set->mask & (1 << part)) {
            EftRPartDef *def = set->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 f = EftEmit_GetFlagsFromMask(set, &w->state, objId, part, sub, w->flags & EFT_CSPARK_END,
                                                 w->flags & EFT_CSPARK_FAST, w->mask);

                if (w->flags & EFT_CSPARK_RESET) {
                    f = 2;
                }
                if (f != 0) {
                    EftEmit_Spawn(set, &w->state, &w->nodes, &w->pos, &w->dir, objId, 0, 2, part, sub, f, w->scale);
                }
            }
        }
    }
}

/* The emitter set of a spark kind (there is one). */
EftRSet *EftClashSpark_GetSet(s32 kind) {
    return gEftClashSparkSet;
}

/* Node slots a spark kind uses (bit per slot): slot 0. */
s32 EftClashSpark_GetNodeMask(s32 kind) {
    u8 mask[1];

    mask[0] = 1;
    return mask[kind];
}

/* Task init. */
void EftClashSpark_Init(EftRTask *task, EftClashSparkArg *arg) {
    EftClashSpark *w = task->work;
    s32 i;

    memset(w, 0, sizeof(EftClashSpark));
    w->flags |= EFT_CSPARK_ACTIVE;
    w->kind = arg->kind;
    w->objId = arg->objId;
    Vec4_Copy(&w->pos, &arg->pos);
    Vec4_Copy(&w->dir, &arg->dir);
    w->dir.w = 1.0f;
    Vec3_Normalize(&w->dir, &w->dir);
    w->scale = arg->scale;
    w->set = EftClashSpark_GetSet(w->kind);
    EftEmit_InitState(w->set, &w->state);
    w->life = EftEmit_GetEndFrames(w->set);
    w->mask = EftClashSpark_GetNodeMask(w->kind);
    for (i = 0; i < 6; i++) {
        if ((w->mask >> i) & 1) {
            EftEmit_SetNode(&w->nodes, i, -1, &w->pos);
        }
    }
}

/* Task term. */
void EftClashSpark_Term(EftRTask *task) {
    EftClashSpark *w = task->work;

    EftEmit_TermState(w->set, &w->state);
}

/* Task update: spawns the particles; once asked to end, dies after the set's end frames. */
void EftClashSpark_Update(EftRTask *task) {
    EftClashSpark *w = task->work;
    s32 dead; /* a dead store after a call keeps it from becoming a tail call, as in the original */

    if (!BtlScene_IsEffectStopped(w->objId, 2)) {
        EftEmit_RefreshFixedNodes(w->objId, &w->nodes);
        EftClashSpark_SpawnParts(w->objId, task, w->set);
        if (w->flags & EFT_CSPARK_END) {
            w->timer += 1.0f;
        }
        if (w->flags & EFT_CSPARK_KILL) {
            BtlTask_SetDead(task);
            dead = 1;
            return;
        }
        if (w->flags & EFT_CSPARK_END) {
            if (w->flags & EFT_CSPARK_FAST) {
                w->flags |= EFT_CSPARK_KILL;
            } else if (w->timer >= w->life) {
                w->flags |= EFT_CSPARK_KILL;
            }
        }
    }
}

/* Task post-update (from the second frame on): refreshes which particles are alive and clears the phase mask. */
void EftClashSpark_PostUpdate(EftRTask *task) {
    EftClashSpark *w = task->work;

    if (!BtlScene_IsEffectStopped(w->objId, 2)) {
        if (w->flags & EFT_CSPARK_POSTED) {
            EftEmit_UpdateAlive(w->set, &w->state);
            w->mask = 0;
        }
        w->flags |= EFT_CSPARK_POSTED;
    }
}

/* Task reset: destroys the particles once and dies. */
void EftClashSpark_Reset(EftRTask *task) {
    EftClashSpark *w = task->work;

    if (!(w->flags & EFT_CSPARK_RESET)) {
        w->flags |= EFT_CSPARK_RESET;
        EftEmit_KillAll(w->set, &w->state);
    }
    BtlTask_SetDead(task);
}

/* Task draw: nothing. */
void EftClashSpark_Draw(void) {
}

/* Manager init: the set (common effect file 0x268) and a child list of three sparks. */
void EftClashSparkMgr_Init(EftRTask *task) {
    s32 i;

    gEftClashSparkSet = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftRSet));
    memset(gEftClashSparkSet, 0, sizeof(EftRSet));
    gEftClashSparkList = BtlTask_CreateChildList(task, 3, sizeof(EftClashSpark));
    {
        s32 file[1] = { 0x268 };

        for (i = 0; i < 1; i++) {
            EftEmit_LoadSet(NULL, &gEftClashSparkSet[i], 0, NULL, 1, file[i]);
        }
    }
}

/* Manager term. */
void EftClashSparkMgr_Term(void) {
    EftEmit_FreeSet(gEftClashSparkSet);
    BtlPool_Free(BtlPool_GetCurrent(), gEftClashSparkSet);
    gEftClashSparkSet = NULL;
}

/* Manager update: steps the set. */
void EftClashSparkMgr_Update(void) {
    EftRSet *set = gEftClashSparkSet;
    s32 i;

    for (i = 0; i < 1; i++) {
        EftEmit_BeginFrame(&set[i]);
    }
}

/* Manager reset: nothing. */
void EftClashSparkMgr_Reset(void) {
}

/* Creates a spark; NULL when the three tasks are in use. */
EftRTask *EftClashSpark_Create(EftClashSparkArg arg) {
    return BtlTaskList_AddTail(gEftClashSparkList, &gEftClashSparkClass, &arg);
}

/* Asks a spark to end. Returns 1 when it was running. */
s32 EftClashSpark_Stop(EftRTask *task) {
    EftClashSpark *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls->update != (void (*)(EftRTask *))EftClashSpark_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CSPARK_ACTIVE)) {
        return 0;
    }
    w->flags |= EFT_CSPARK_END;
    return 1;
}

/* Moves a spark. */
s32 EftClashSpark_SetPos(EftRTask *task, EftRVec *pos) {
    EftClashSpark *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls->update != (void (*)(EftRTask *))EftClashSpark_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CSPARK_ACTIVE)) {
        return 0;
    }
    Vec4_Copy(&w->pos, pos);
    return 1;
}

/* Sets a spark's size. */
s32 EftClashSpark_SetScale(EftRTask *task, f32 scale) {
    EftClashSpark *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls->update != (void (*)(EftRTask *))EftClashSpark_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CSPARK_ACTIVE)) {
        return 0;
    }
    w->scale = scale;
    return 1;
}

/* ---- charge aura (fighter effect requests 7 and 8) ---------------------------------------------------------- */

/* Steps the emitters of the set and spawns their particles at the fighter's node 3, pointing up. */
void EftCharge_SpawnParts(s32 objId, EftRTask *task, EftRSet *set) {
    EftRVec dir;
    EftCharge *w = task->work;
    s32 part;
    s32 sub;

    Vec4_Set(&dir, 0.0f, -1.0f, 0.0f, 1.0f);
    for (part = 0; part < 0x13; part++) {
        if (*set->mask & (1 << part)) {
            EftRPartDef *def = set->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 f = EftEmit_GetFlagsFromMask(set, &w->state, objId, part, sub, w->flags & EFT_CHARGE_END, 0,
                                                 w->mask);

                if (f != 0) {
                    EftEmit_Spawn(set, &w->state, &w->nodes, &w->pos, &dir, objId, 0, 3, part, sub, f, w->scale);
                }
            }
        }
    }
}

/* Task init. */
void EftCharge_Init(EftRTask *task, s32 *arg) {
    EftRMgr *mgr = BtlTask_GetParent(task)->work;
    EftCharge *w = task->work;
    EftRSet *set = &mgr->set->set;
    s32 objId;

    memset(w, 0, sizeof(EftCharge));
    objId = arg[0];
    w->flags |= EFT_CHARGE_ACTIVE;
    w->objId = objId;
    w->scale = BtlCharApi_GetHeight(objId) / 19.35f;
    if (w->scale < 0.5f) {
        w->scale = 0.5f;
    }
    BtlCharApi_GetNodePos(w->objId, 3, &w->pos);
    w->set = set;
    EftEmit_InitState(set, &w->state);
    w->mask |= 1;
    EftEmit_SetNode(&w->nodes, 0, 3, NULL);
    w->flags |= EFT_CHARGE_ALIVE;
    w->life = EftEmit_GetEndFrames(w->set);
    BtlTask_SetOwnerTag(task, arg[0] == 0 ? 0x800 : 0x1000);
}

/* Task term: releases the emitter state and the fighter's task slot. */
void EftCharge_Term(EftRTask *task) {
    EftCharge *w = task->work;
    s32 *owner = &w->objId;

    EftEmit_TermState(w->set, &w->state);
    w->flags = 0;
    EftCharSlot_Clear0(*owner);
}

/* Task update: follows node 3; request 8 adds the burst emitters and one screen shock wave; once asked to end,
   dies after the set's end frames. */
void EftCharge_Update(EftRTask *task) {
    EftCharge *w = task->work;
    s32 *owner = &w->objId;
    EftRNodes *nodes;

    if (!BtlScene_IsEffectStopped(*owner, 3)) {
        BtlCharApi_GetNodePos(*owner, 3, &w->pos);
        if (w->flags & EFT_CHARGE_BURST) {
            if (!(w->flags & EFT_CHARGE_BURST_DONE)) {
            w->mask |= 2;
            nodes = &w->nodes;
            EftEmit_SetNode(nodes, 1, 3, NULL);
            ScrWarp_Spawn(*owner, &w->pos, 0.8f, 10.0f, 50.0f, 10.0f, 0.5f);
            w->flags |= EFT_CHARGE_BURST_DONE;
            } else {
 nodes = &w->nodes;
            }
        } else {
 nodes = &w->nodes;
        }
        EftEmit_RefreshFixedNodes(w->objId, nodes);
        EftCharge_SpawnParts(w->objId, task, w->set);
        if (w->flags & EFT_CHARGE_END) {
            w->timer += 1.0f;
            if (w->timer >= w->life) {
                w->flags |= EFT_CHARGE_KILL;
            }
        }
    }
    if (w->flags & EFT_CHARGE_KILL) {
        BtlTask_SetDead(task);
    }
}

/* Task reset: destroys the particles once and dies. */
void EftCharge_Reset(EftRTask *task) {
    EftCharge *w = task->work;

    if (!(w->flags & EFT_CHARGE_RESET)) {
        w->flags |= EFT_CHARGE_RESET;
        EftEmit_KillAll(w->set, &w->state);
    }
    BtlTask_SetDead(task);
}

/* Task post-update (from the second frame on): notes whether a particle is alive, clears the phase mask. */
void EftCharge_PostUpdate(EftRTask *task) {
    EftCharge *w = task->work;

    if (w->flags & EFT_CHARGE_POSTED) {
        if (!EftEmit_UpdateAlive(w->set, &w->state)) {
            w->flags &= ~EFT_CHARGE_ALIVE;
        } else {
            w->flags |= EFT_CHARGE_ALIVE;
        }
        w->mask = 0;
    }
    w->flags |= EFT_CHARGE_POSTED;
}

/* Task draw: nothing. */
void EftCharge_Draw(void) {
}

/* Manager init: loads the set from entry 2 of the character's effect pack, makes room for four tasks. */
void EftChargeMgr_Init(EftRTask *task, s32 *arg) {
    EftRMgr *mgr = task->work;
    EftRSetPack *set;
    s32 *pack;
    void *list;

    mgr->set = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftRSetPack));
    memset(mgr->set, 0, sizeof(EftRSetPack));
    set = mgr->set;
    pack = BtlScene_GetCharPackEntry(arg[0], 2);
    set->pack = pack;
    if (pack != NULL) {
        EftEmit_LoadSet(NULL, set, 0, pack, 2, 1);
    }
    list = BtlTask_CreateChildList(task, 4, sizeof(EftCharge));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term. */
void EftChargeMgr_Term(EftRTask *task) {
    EftRMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_FreeSet(mgr->set);
    }
    if (mgr->set != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->set);
    }
}

/* Manager update: steps the set. */
void EftChargeMgr_Update(EftRTask *task) {
    EftRMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_BeginFrame(mgr->set);
    }
}

/* Fighter effect request 7 is set and the fighter has no aura: starts one. Returns 1 when a task was created. */
s32 EftCharge_Start(s32 *arg) {
    s32 objId = arg[0];
    void *list;
    EftRTask *task;

    if (EftCharSlot_Get0(objId) != NULL) {
        return 0;
    }
    list = EftChar_GetList(objId, 0);
    if (list == NULL) {
        return 0;
    }
    task = BtlTaskList_AddTail(list, &gEftChargeClass, &objId);
    if (task == NULL) {
        return 0;
    }
    EftCharSlot_Set0(objId, task);
    return 1;
}

/* Fighter effect request 8: the aura's burst. */
s32 EftCharge_Burst(s32 objId) {
    EftRTask *task = EftCharSlot_Get0(objId);
    EftCharge *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHARGE_ACTIVE)) {
        return 0;
    }
    w->flags |= EFT_CHARGE_BURST;
    return 1;
}

/* Request 7 ended: asks the aura to end. */
s32 EftCharge_Stop(s32 objId) {
    EftRTask *task = EftCharSlot_Get0(objId);
    EftCharge *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_CHARGE_ACTIVE)) {
        return 0;
    }
    w->flags |= EFT_CHARGE_END;
    return 1;
}

/* Whether the fighter has an aura task. */
s32 EftCharge_IsActive(s32 objId) {
    EftRTask *task = EftCharSlot_Get0(objId);
    EftCharge *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (w->flags & EFT_CHARGE_ACTIVE) {
        return 1;
    }
    return 0;
}

/* ---- ki blast ----------------------------------------------------------------------------------------------- */

/* Publishes this frame's hit record: two spheres at the current and the previous position, radius = the blast's
   radius x the emitter state's trail width. The ki blast never steps that animation (EftEmit_UpdateTrailWidth is
   not called), so the factor stays at the set header's first width. EftHit_Add then takes the owner, the kind's
   class, the speed and the clash level from the argument block (rec->atk). */
void EftKiBlast_AddHitRecord(EftRTask *task) {
    EftKiBlast *w = task->work;
    EftRRec *rec = EftHit_GetNew();
    EftRSphere *a;
    EftRSphere *b;
    f32 radius;

    rec->task = task;
    rec->pose = w->pose;
    rec->unk54 = 0;
    rec->unk5C = (w->flags >> 9) & 1;
    rec->atk = &w->arg;
    radius = w->radius * EftEmit_GetTrailWidth(&w->state);
    a = EftHitArena_AllocSphere();
    b = EftHitArena_AllocSphere();
    ColSphere_Set(a, &w->pose.pos, radius);
    ColSphere_Set(b, &w->pose.prev, radius);
    EftHit_SetShapeSpheres(rec, a, b);
    EftHit_Add(rec);
}

/* The fighter the blast reached knocked it away (task flag 0x40) or sent it back (0x80): the blast gets a new
   direction from that fighter (BtlCharApi_GetDeflectDir: two draws of the fighter generator) and that fighter
   becomes its owner. Deflected: homing off, and no later deflection or reflection is taken. Reflected: the
   lifetime starts again and homing (if it had it) now steers at the new owner's opponent. */
void EftKiBlast_Turn(EftRTask *task, EftKiBlast *w) {
    EftRVec dir;
    u16 id;
    s32 life;

    Vec3_Lerp(&w->pose.pos, &task->pos, &w->pose.prev, 0.7f);
    if (task->hit & EFT_R_HIT_DEFLECT) {
        BtlCharApi_GetDeflectDir((s16)(w->arg.objId ^ 1), &dir);
        Vec4_Copy(&w->pose.dir, &dir);
        w->arg.objId ^= 1;
        w->flags = (w->flags & ~EFT_KIBLAST_HOMING) | EFT_KIBLAST_DEFLECTED;
    } else if (task->hit & EFT_R_HIT_REFLECT) {
        BtlCharApi_GetDeflectDir((s16)(w->arg.objId ^ 1), &dir);
        Vec4_Copy(&w->pose.dir, &dir);
        w->arg.objId ^= 1;
        w->life = w->arg.life;
    }
}

/* Steps the emitters of the set and spawns their particles at the blast; reset: the flags of an emitter restart. */
void EftKiBlast_SpawnParts(s32 objId, EftRTask *task, EftRSet *set, s32 reset) {
    EftKiBlast *w = task->work;
    s32 part;
    s32 sub;

    for (part = 0; part < 0x13; part++) {
        if (*set->mask & (1 << part)) {
            EftRPartDef *def = set->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 f;

                if (reset == 0) {
                    f = EftEmit_GetFlagsFromMask(set, &w->state, objId, part, sub, w->flags & EFT_KIBLAST_ENDING, 0,
                                                 w->mask);
                } else {
                    f = EftEmit_GetResetFlags(set, &w->state, part, sub);
                }
                if (f != 0) {
                    EftEmit_Spawn(set, &w->state, &w->nodes, &w->pose.pos, &w->pose.dir, objId, 0, 0, part, sub, f,
                                  w->radius);
                }
            }
        }
    }
}

/* Sets bit 0x80 in the state flags of every emitter of group 17 (the trail). */
void EftKiBlast_EndTrail(s32 objId, EftRTask *task, EftRSet *set) {
    EftKiBlast *w = task->work;
    s32 i;

    if (*set->mask & 0x20000) {
        EftRPart *part = &set->part[17];
        EftRPartDef *def = part->def;

        for (i = 0; i < def->count; i++) {
            w->state.flags[part->first + i] |= 0x80;
        }
    }
}

/* Task init: the blast starts at the model node of the argument, flying along the argument's direction (not
   normalised here); the "previous" position starts at the owner's node 0x11, so the first swept volumes reach back
   to the body. It homes when its owner is locked on at this moment. If the set header has flag 4 the speed is
   replaced by the state's second width, which is still 0 at this point (the state is initialised after): such a
   blast would not move. */
void EftKiBlast_Init(EftRTask *task, EftRArg *arg) {
    EftRMgr *mgr = BtlTask_GetParent(task)->work;
    EftKiBlast *w = task->work;
    EftRSet *set = &mgr->set->set;

    memset(w, 0, sizeof(EftKiBlast));
    w->arg = *arg;
    w->arg.srcId = arg->objId;
    if (arg->type != 0) {
        w->flags |= EFT_KIBLAST_TYPED;
    }
    BtlCharApi_GetNodePos(arg->objId, arg->node, &w->pose.start);
    BtlCharApi_GetNodePos(arg->objId, arg->node, &w->pose.pos);
    BtlCharApi_GetNodePos(arg->objId, 0x11, &w->pose.prev);
    Vec4_Copy(&w->pose.dir, &arg->dir);
    w->radius = arg->radius;
    w->unk5C0 = arg->unk2C;
    w->life = arg->life;
    w->unk5B8 = arg->unk1B;
    w->set = set;
    if (EftEmit_HasWidth2(set)) {
        w->arg.speed = EftEmit_GetWidth2(&w->state);
        w->flags |= EFT_KIBLAST_WIDTH2;
    }
    EftEmit_InitState(w->set, &w->state);
    w->mask |= 2;
    EftEmit_SetNode(&w->nodes, 1, arg->node, NULL);
    EftEmit_RefreshFixedNodes(arg->objId, &w->nodes);
    w->endFrames = EftEmit_GetEndFrames(w->set);
    if (BtlCharApi_IsLockedOn(arg->objId)) {
        w->flags |= EFT_KIBLAST_HOMING;
    }
    BtlTask_SetOwnerTag(task, arg->objId == 0 ? 0x800 : 0x1000);
}

/* Task term. */
void EftKiBlast_Term(EftRTask *task) {
    EftKiBlast *w = task->work;

    EftEmit_TermState(w->set, &w->state);
}

/* Task update, skipped while the owner's effects are stopped: homing turn (EftAim_Home, every frame from the
   first), one step of `speed` along the direction (not on the first frame, not once stopped or ending), the
   lifetime, the particles, and the hit record while the blast is neither ending nor ended. With a lifetime of n
   frames the blast publishes n - 1 records; then the end animation runs for the set's end frames + 2 and the task
   dies. */
void EftKiBlast_Update(EftRTask *task) {
    EftRVec dir;
    EftRVec step;
    EftKiBlast *w = task->work;
    EftRArg *arg = &w->arg;

    if (!BtlScene_IsEffectStopped(arg->objId, 0)) {
        if (!(w->flags & EFT_KIBLAST_STOPPED)) {
            if (w->flags & EFT_KIBLAST_HOMING) {
                EftAim_Home(&dir, &w->pose.pos, &w->pose.dir, arg->speed, arg->turn, arg->objId);
                Vec4_Copy(&w->pose.dir, &dir);
            }
            if (w->flags & EFT_KIBLAST_STARTED) {
                if (w->flags & EFT_KIBLAST_MOVED) {
                    Vec4_Copy(&w->pose.prev, &w->pose.pos);
                }
                if (!(w->flags & EFT_KIBLAST_ENDING)) {
                    Vec3_Scale(&step, &w->pose.dir, arg->speed);
                    Vec3_Add(&w->pose.pos, &w->pose.pos, &step);
                }
                w->flags |= EFT_KIBLAST_MOVED;
            }
            w->flags |= EFT_KIBLAST_STARTED;
        }
        w->life--;
        if (w->life <= 0) {
            w->flags |= EFT_KIBLAST_ENDING;
        }
        EftKiBlast_SpawnParts(w->arg.srcId, task, w->set, 0);
        if (w->flags & EFT_KIBLAST_ENDED) {
            w->flags |= EFT_KIBLAST_KILL;
        } else if (w->flags & EFT_KIBLAST_ENDING) {
            if (w->endTimer >= w->endFrames) {
                w->flags |= EFT_KIBLAST_ENDED;
            }
            w->endTimer += 1.0f;
        } else if (arg->canHit != 0) {
            EftKiBlast_AddHitRecord(task);
        }
    }
    if (w->flags & EFT_KIBLAST_KILL) {
        BtlTask_SetDead(task);
    }
    if (w->flags & EFT_KIBLAST_ENDING) {
        w->endCount++;
    }
}

/* Task reset: destroys the particles once and dies. */
void EftKiBlast_Reset(EftRTask *task) {
    EftKiBlast *w = task->work;

    if (!(w->flags & EFT_KIBLAST_RESET)) {
        w->flags |= EFT_KIBLAST_RESET;
        EftEmit_KillAll(w->set, &w->state);
    }
    BtlTask_SetDead(task);
}

/* Task post-update: reacts to what the hit pass reported. Any end result (hit, guarded, stage, lost clash, left
   the stage, absorbed): the blast stops at the reported position and starts its end. Deflected / reflected (once
   deflected, never again): EftKiBlast_Turn, and it goes on from the reported position. */
void EftKiBlast_PostUpdate(EftRTask *task) {
    EftKiBlast *w = task->work;
    EftRArg *arg = &w->arg;

    EftEmit_UpdateAlive(w->set, &w->state);
    w->mask = 0;
    if (task->hit & EFT_R_HIT_END) {
        w->life = 0;
        w->flags |= EFT_KIBLAST_ENDING;
        if (!(w->flags & EFT_KIBLAST_STOPPED)) {
            Vec3_Copy(&w->pose.pos, &task->pos);
        }
        EftKiBlast_SpawnParts(arg->srcId, task, w->set, 1);
        if (!(w->flags & EFT_KIBLAST_STOPPED)) {
            EftKiBlast_EndTrail(arg->srcId, task, w->set);
        }
        w->flags |= EFT_KIBLAST_STOPPED;
    } else if (task->hit & EFT_R_HIT_TURN) {
        if (!(w->flags & EFT_KIBLAST_DEFLECTED)) {
            EftKiBlast_Turn(task, w);
            w->pose.pos.x = task->pos.x;
            w->pose.pos.y = task->pos.y;
            w->pose.pos.z = task->pos.z;
            EftKiBlast_SpawnParts(arg->srcId, task, w->set, 1);
            EftKiBlast_EndTrail(arg->srcId, task, w->set);
            w->flags |= EFT_KIBLAST_TURNED;
            task->hit &= ~EFT_R_HIT_TURN;
        }
    }
}

/* Task draw: only asks whether the owner's effects are hidden (the particles draw themselves). */
void EftKiBlast_Draw(EftRTask *task) {
    EftKiBlast *w = task->work;

    if (BtlScene_IsEffectHidden(w->arg.objId, 0)) {
        return;
    }
}

/* Manager init (sub-manager kinds 1 and 2): kind 1 loads entry 3 of the character's effect pack and makes room for
   20 blasts, kind 2 entry 4 and 10 blasts. */
void EftKiBlastMgr_Init(EftRTask *task, s32 *arg) {
    EftRMgr *mgr = task->work;
    EftRSetPack *set;
    s32 *pack;
    s32 idx;
    s32 count;
    void *list;

    mgr->set = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftRSetPack));
    memset(mgr->set, 0, sizeof(EftRSetPack));
    idx = 4;
    set = mgr->set;
    if (arg[1] == 1) {
        idx = 3;
        count = 20;
    } else {
        count = 10;
    }
    pack = BtlScene_GetCharPackEntry(arg[0], idx);
    set->pack = pack;
    if (pack != NULL) {
        EftEmit_LoadSet(NULL, set, 0, pack, 2, 1);
    }
    list = BtlTask_CreateChildList(task, count, sizeof(EftKiBlast));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term. */
void EftKiBlastMgr_Term(EftRTask *task) {
    EftRMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_FreeSet(mgr->set);
    }
    if (mgr->set != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->set);
    }
}

/* Manager update: steps the set. */
void EftKiBlastMgr_Update(EftRTask *task) {
    EftRMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_BeginFrame(mgr->set);
    }
}

/* Launches one ki blast (BtlFx_FireKiBlast, once per shot of the volley): a task in the owner's list of
   kind 1 (ki blast type 0) or 2 (any other type). Nothing happens when the list is full. */
void EftKiBlast_Fire(EftRArg arg) {
    void *list = EftChar_GetList(arg.objId, arg.type == 0 ? 1 : 2);

    if (list != NULL) {
        BtlTaskList_AddTail(list, &gEftKiBlastClass, &arg);
    }
}

/* Returns 1 (no caller). */
s32 EftKiBlast_Ret1A(void) {
    return 1;
}

/* Returns 1 (no caller). */
s32 EftKiBlast_Ret1B(void) {
    return 1;
}

/* ---- ki blast charge glow (fighter effect request 0x17) ----------------------------------------------------- */

/* Steps the emitters of the set and spawns their particles at the node. */
void EftBlastCharge_SpawnParts(s32 objId, EftRTask *task, EftRSet *set) {
    EftBlastCharge *w = task->work;
    s32 part;
    s32 sub;

    for (part = 0; part < 0x13; part++) {
        if (*set->mask & (1 << part)) {
            EftRPartDef *def = set->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 f = EftEmit_GetFlagsFromMask(set, &w->state, objId, part, sub, w->flags & EFT_BCHARGE_END, 0,
                                                 w->mask);

                if (f != 0) {
                    EftEmit_Spawn(set, &w->state, &w->nodes, &w->pos, &w->dir, objId, 0, 0, part, sub, f, w->scale);
                }
            }
        }
    }
}

/* Task init. */
void EftBlastCharge_Init(EftRTask *task, EftBlastChargeArg *arg) {
    EftRMgr *mgr = BtlTask_GetParent(task)->work;
    EftBlastCharge *w = task->work;
    EftRSet *set = &mgr->set->set;

    memset(w, 0, sizeof(EftBlastCharge));
    w->arg = *arg;
    BtlCharApi_GetNodePos(arg->objId, arg->node, &w->pos);
    Vec4_Copy(&w->dir, &w->arg.pos);
    w->flags |= EFT_BCHARGE_ACTIVE;
    w->scale = BtlCharApi_GetHeight(arg->objId) / 19.35f;
    if (w->scale < 0.5f) {
        w->scale = 0.5f;
    }
    w->unk564 = w->arg.unk18;
    w->set = set;
    if (set == NULL) {
        BtlTask_SetDead(task);
        return;
    }
    EftEmit_InitState(set, &w->state);
    w->mask |= 1;
    EftEmit_SetNode(&w->nodes, 0, arg->node, NULL);
    w->life = EftEmit_GetEndFrames(w->set);
    BtlTask_SetOwnerTag(task, arg->objId == 0 ? 0x800 : 0x1000);
}

/* Task term: releases the emitter state and the fighter's task slot. */
void EftBlastCharge_Term(EftRTask *task) {
    EftBlastCharge *w = task->work;
    EftBlastChargeArg *arg = &w->arg;

    if (w->set != NULL) {
        EftEmit_TermState(w->set, &w->state);
    }
    EftCharSlot_Clear1(arg->objId);
}

/* Task update: follows the node while the fighter is charging a ki blast; afterwards one more particle step with
   the end flag, then dies. */
void EftBlastCharge_Update(EftRTask *task) {
    EftBlastCharge *w = task->work;
    EftBlastChargeArg *arg = &w->arg;
    s32 dead; /* a dead store after a call keeps it from becoming a tail call, as in the original */

    if (w->set == NULL) {
        BtlTask_SetDead(task);
        return;
    }
    if (!BtlScene_IsEffectStopped(arg->objId, 0)) {
        BtlCharApi_GetNodePos(arg->objId, arg->node, &w->pos);
        EftEmit_RefreshFixedNodes(w->arg.objId, &w->nodes);
        EftBlastCharge_SpawnParts(w->arg.objId, task, w->set);
        if (!BtlCharApi_IsChargingKiBlast(arg->objId)) {
            w->flags |= EFT_BCHARGE_END;
        }
    }
    if (w->flags & EFT_BCHARGE_KILL) {
        BtlTask_SetDead(task);
        dead = 1;
    } else if (w->flags & EFT_BCHARGE_END) {
        EftBlastCharge_SpawnParts(w->arg.objId, task, w->set);
        w->flags |= EFT_BCHARGE_KILL;
    }
}

/* Task reset: dies. */
void EftBlastCharge_Reset(EftRTask *task) {
    BtlTask_SetDead(task);
}

/* Task post-update (from the second frame on): refreshes which particles are alive, clears the phase mask. */
void EftBlastCharge_PostUpdate(EftRTask *task) {
    EftBlastCharge *w = task->work;

    if (w->flags & EFT_BCHARGE_POSTED) {
        EftEmit_UpdateAlive(w->set, &w->state);
        w->mask = 0;
    }
    w->flags |= EFT_BCHARGE_POSTED;
}

/* Task draw: nothing. */
void EftBlastCharge_Draw(void) {
}

/* Manager init (sub-manager kind 3): the set from entry 4 of the character's effect pack, ten tasks. */
void EftBlastChargeMgr_Init(EftRTask *task, s32 *arg) {
    EftRMgr *mgr = task->work;
    EftRSetPack *set;
    s32 *pack;
    void *list;

    mgr->set = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftRSetPack));
    memset(mgr->set, 0, sizeof(EftRSetPack));
    set = mgr->set;
    pack = BtlScene_GetCharPackEntry(arg[0], 4);
    set->pack = pack;
    if (pack != NULL) {
        EftEmit_LoadSet(NULL, set, 0, pack, 2, 1);
    }
    list = BtlTask_CreateChildList(task, 10, sizeof(EftBlastCharge));
    EftChar_SetList(arg[0], 3, list);
}

/* Manager term. */
void EftBlastChargeMgr_Term(EftRTask *task) {
    EftRMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_FreeSet(mgr->set);
    }
    if (mgr->set != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->set);
    }
}

/* Manager update: steps the set. */
void EftBlastChargeMgr_Update(EftRTask *task) {
    EftRMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_BeginFrame(mgr->set);
    }
}

/* Fighter effect request 0x17 (BtlFx_SpawnHitSparkReq17): starts the glow. Returns 1 when a task was created. */
s32 EftBlastCharge_Start(EftBlastChargeArg *arg) {
    EftBlastChargeArg a;
    void *list;
    EftRTask *task;

    if (arg == NULL) {
        return 0;
    }
    a = *arg;
    list = EftChar_GetList(a.objId, 3);
    if (list == NULL) {
        return 0;
    }
    task = BtlTaskList_AddTail(list, &gEftBlastChargeClass, &a);
    if (task == NULL) {
        return 0;
    }
    EftCharSlot_Set1(a.objId, task);
    return 1;
}

/* Asks the fighter's glow to end (no caller: the task ends by itself). */
s32 EftBlastCharge_Stop(s32 objId) {
    EftRTask *task = EftCharSlot_Get1(objId);
    EftBlastCharge *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BCHARGE_ACTIVE)) {
        return 0;
    }
    w->flags |= EFT_BCHARGE_END;
    return 1;
}

/* ---- ki bomb (ki blast type 3) ------------------------------------------------------------------------------ */

/* Publishes this frame's hit record: two spheres of the bomb's radius (5, or 28 while it explodes). */
void EftKiBomb_AddHitRecord(EftRTask *task) {
    EftKiBomb *w = task->work;
    EftRRec *rec = EftHit_GetNew();
    EftRSphere *a;
    EftRSphere *b;
    f32 radius;

    rec->task = task;
    rec->pose = w->pose;
    rec->unk54 = 0;
    rec->unk5C = (w->flags >> 5) & 1;
    rec->flags |= 0x202;
    rec->unk54 = (w->flags >> 9) & 1;
    rec->atk = w;
    radius = w->radius;
    a = EftHitArena_AllocSphere();
    b = EftHitArena_AllocSphere();
    ColSphere_Set(a, &w->pose.pos, radius);
    ColSphere_Set(b, &w->pose.prev, radius);
    EftHit_SetShapeSpheres(rec, a, b);
    EftHit_Add(rec);
}

/* Places the bomb at its node and throws it: the argument's direction with a jitter from the VU0 generator
   (x and z +-0.2, y up to 0.3 upwards), normalised, times the speed. The spin rate (27..37 degrees per frame,
   either sense) comes from libc rand(). Creates the model instance. */
void EftKiBomb_Launch(EftRTask *task) {
    EftRMtx m;
    EftKiBomb *w = task->work;
    EftKiBombRes *res;
    f32 spin;

    BtlCharApi_GetNodePos(w->arg.objId, w->arg.node, &w->pose.start);
    BtlCharApi_GetNodePos(w->arg.objId, w->arg.node, &w->pose.pos);
    BtlCharApi_GetNodePos(w->arg.objId, 0x11, &w->pose.prev);
    Vec4_Copy(&w->pose.dir, &w->arg.dir);
    w->pose.dir.x += Rand_FloatRange(-0.2f, 0.2f);
    w->pose.dir.y -= Rand_FloatRange(0.0f, 0.3f);
    w->pose.dir.z += Rand_FloatRange(-0.2f, 0.2f);
    Vec3_Normalize(&w->pose.dir, &w->pose.dir);
    w->size = w->arg.radius;
    w->life = w->arg.life;
    w->speed = w->arg.speed;
    w->blast = 15;
    w->maxLife = 300;
    w->unkD8 = 3;
    w->gravity = 0.5f;
    w->radius = 5.0f;
    spin = (f32)rand() / 2147483647.0f * 10.0f - 5.0f + 32.0f;
    w->spin = spin;
    w->spin = ((f32)rand() / 2147483647.0f < 0.5f) ? spin : -spin;
    Vec3_Scale(&w->pose.dir, &w->pose.dir, w->speed);
    res = ((EftKiBombMgr *)BtlTask_GetParent(task)->work)->res;
    Mtx_StoreIdentity(&m);
    w->handle = EftObj_Create(w->handleBuf, res->model);
    if (w->handle != -1) {
        Mtx_SetTrans(&m, &w->pose.pos);
        EftObj_SetMtx(w->handle, &m);
    }
}

/* Task init. */
void EftKiBomb_Init(EftRTask *task, EftRArg *arg) {
    EftKiBomb *w = task->work;

    memset(w, 0, sizeof(EftKiBomb));
    w->arg = *arg;
    w->arg.srcId = arg->objId;
    EftKiBomb_Launch(task);
    w->flags |= EFT_KIBOMB_SHOWN | EFT_KIBOMB_HITS;
    BtlTask_SetOwnerTag(task, arg->objId == 0 ? 0x800 : 0x1000);
}

/* Task term: frees the model instance. */
void EftKiBomb_Term(EftRTask *task) {
    EftKiBomb *w = task->work;

    if (w->handle != -1) {
        EftObj_Destroy(w->handle);
    }
}

/* Task update, unless any character is stopped: moves, publishes the hit record, dies when the blast is over. */
void EftKiBomb_Update(EftRTask *task) {
    EftKiBomb *w = task->work;
    s32 dead; /* a dead store after a call keeps it from becoming a tail call, as in the original */

    if (!BtlScene_IsAnyCharStopped()) {
        EftKiBomb_Move(task);
        if (w->flags & EFT_KIBOMB_HITS) {
            EftKiBomb_AddHitRecord(task);
        }
        if (w->flags & EFT_KIBOMB_KILL) {
            BtlTask_SetDead(task);
            dead = 1;
        }
    }
}

/* Task reset: dies. */
void EftKiBomb_Reset(EftRTask *task) {
    BtlTask_SetDead(task);
}

/* Task post-update. */
void EftKiBomb_PostUpdate(EftRTask *task) {
    EftKiBomb_HandleHits(task);
}

/* Task draw. */
void EftKiBomb_Draw(EftRTask *task) {
    EftKiBomb_UpdateModel(task);
}

/* Manager init (replaces sub-manager kind 2 for a character with a type 3 ki blast): the model from entry 1 of
   entry 4 of the character's effect pack, ten tasks. */
void EftKiBombMgr_Init(EftRTask *task, s32 *arg) {
    EftKiBombMgr *mgr = task->work;
    EftKiBombRes *res;
    void *list;

    mgr->res = NULL;
    mgr->res = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftKiBombRes));
    memset(mgr->res, 0, sizeof(EftKiBombRes));
    res = mgr->res;
    res->pack = BtlScene_GetCharPackEntry(arg[0], 4);
    res->model = BtlScene_GetPackEntry(res->pack, 1);
    list = BtlTask_CreateChildList(task, 10, sizeof(EftKiBomb));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term. */
void EftKiBombMgr_Term(EftRTask *task) {
    EftKiBombMgr *mgr = task->work;

    if (mgr->res != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->res);
        mgr->res = NULL;
    }
}

/* Manager update: nothing. */
void EftKiBombMgr_Update(void) {
}

/* Launches a type 3 ki blast (BtlFx_FireKiBlast): TWO bombs per shot, in the owner's list of kind 2. */
void EftKiBomb_Fire(EftRArg arg) {
    void *list = EftChar_GetList(arg.objId, 2);

    if (list != NULL) {
        BtlTaskList_AddTail(list, &gEftKiBombClass, &arg);
        BtlTaskList_AddTail(list, &gEftKiBombClass, &arg);
    }
}

/* Called by the hit pass through EftHit_NotifyBlastTask when a blast record hit the stage: keeps the contact
   (0x40 bytes of the record's collision result; the last vector is the surface normal) if the task is a bomb. */
void EftKiBomb_SetContact(EftRTask *task, EftRMtx *mtx) {
    if (task != NULL && !EFT_TASK_IS_DEAD(task) && task->cls->update == EftKiBomb_Update) {
        ((EftKiBomb *)task->work)->contact = *mtx;
    }
}

/* out = v mirrored on the plane with the given normal, normalised. */
void EftKiBomb_Reflect(EftRVec *out, EftRVec *v, EftRVec *normal) {
    EftRVec n;
    f32 d;

    n.x = -normal->x;
    n.y = -normal->y;
    n.z = -normal->z;
    d = (v->x * n.x + v->y * n.y + v->z * n.z) * -2.0f;
    out->x = v->x + d * n.x;
    out->y = v->y + d * n.y;
    out->z = v->z + d * n.z;
    out->w = 1.0f;
    Vec3_Normalize(out, out);
}

/* One frame of flight: position += velocity, velocity.y += gravity. The bomb explodes after 300 frames, below the
   stage bottom, within 28 of the opponent, or (once it lies on the ground) when the argument's lifetime has run
   out; the explosion is a 15-frame hit record of radius 28. Returns 1 on the frame the explosion ends. */
s32 EftKiBomb_Move(EftRTask *task) {
    EftRVec d;
    EftRVec opp;
    EftRBlastFx arg;
    EftKiBomb *w = task->work;
    EftRVec *pos;
    f32 dist;
    f32 r;
    s32 snd;

    w->frame++;
    if (w->handle == -1) {
        w->flags |= EFT_KIBOMB_KILL;
    }
    pos = &w->pose.pos;
    if (w->flags & EFT_KIBOMB_MOVED) {
        Vec4_Copy(&w->pose.prev, pos);
    }
    w->flags |= EFT_KIBOMB_MOVED;
    Vec3_Add(pos, pos, &w->pose.dir);
    w->pose.dir.y += w->gravity;
    w->maxLife--;
    if (w->maxLife <= 0) {
        w->flags |= EFT_KIBOMB_EXPLODE;
    }
    if (BtlStage_GetBottom() < w->pose.pos.y) {
        w->flags |= EFT_KIBOMB_EXPLODE;
    }
    BtlCharApi_GetPos(BtlCharApi_GetOpponentObjId(w->arg.objId), &opp);
    d.x = opp.x - pos->x;
    d.y = opp.y - w->pose.pos.y;
    d.z = opp.z - w->pose.pos.z;
    dist = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    if (dist < 28.0f) {
        w->flags |= EFT_KIBOMB_EXPLODE;
    }
    if (!(w->flags & EFT_KIBOMB_REST)) {
        w->angle = EftMath_WrapAngle((f32)((w->frame * (s32)w->spin) % 360) * 3.14159265f / 180.0f);
    }
    if (w->flags & EFT_KIBOMB_REST) {
        w->life = (f32)w->life - 1.0f;
    }
    if (!(w->flags & EFT_KIBOMB_EXPLODED)) {
        if ((f32)w->life <= 0.0f || (w->flags & EFT_KIBOMB_EXPLODE)) {
            w->life = 0;
            arg = (EftRBlastFx){ w->pose.pos, { { 0.0f, -1.0f, 0.0f, 1.0f } }, 1, w->arg.objId, 0 };
            EftImpact_SpawnBlast(arg, 1.0f, 1.0f);
            r = 28.0f;
            BtlCharApi_PlaySoundAt(pos, 0, (rand() & 2) ? 0x1B : 0x11, 200.0f, 1500.0f);
            w->radius = r;
            w->flags = (w->flags & ~EFT_KIBOMB_SHOWN) | EFT_KIBOMB_EXPLODED;
        }
    }
    if (w->flags & EFT_KIBOMB_EXPLODED) {
        w->blast = (f32)w->blast - 1.0f;
        if ((f32)w->blast <= 0.0f) {
            w->blast = 0;
            w->flags |= EFT_KIBOMB_KILL;
            return 1;
        }
    }
    return 0;
}

/* Post-update: shows or hides the model, then reacts to the hit pass. Guarded: explodes next frame without a hit
   record. Hit a fighter: explodes next frame. Hit the stage or the stage boundary: bounces (mirrored on the
   contact normal, or on the direction to the stage axis at the boundary; speed and spin halved) up to three times,
   then lies where it landed, at one of three fixed angles picked with libc rand(). */
void EftKiBomb_HandleHits(EftRTask *task) {
    EftRVec n = { { 0.0f, -1.0f, 0.0f, 1.0f } };
    f32 angles[3] = { 0.0f, 90.0f, 270.0f };
    EftRVec refl;
    EftKiBomb *w = task->work;
    EftRVec *vel;
    f32 r;
    f32 speed;
    f32 spin;

    if (w->handle == -1) {
        return;
    }
    if (w->flags & EFT_KIBOMB_SHOWN) {
        if (!BtlScene_IsEffectHidden(w->arg.objId, 0)) {
            EftObj_SetVisible(w->handle, 1);
        } else {
            EftObj_SetVisible(w->handle, 0);
        }
    } else {
        EftObj_SetVisible(w->handle, 0);
    }
    if (task->hit & EFT_R_HIT_GUARD) {
        Vec4_Copy(&w->pose.pos, &task->pos);
        w->flags = (w->flags | EFT_KIBOMB_EXPLODE) & ~EFT_KIBOMB_HITS;
    } else if (task->hit & EFT_R_HIT_CHAR) {
        Vec4_Copy(&w->pose.pos, &task->pos);
        w->flags |= EFT_KIBOMB_EXPLODE;
    } else if (task->hit & (EFT_R_HIT_STAGE | EFT_R_HIT_OUT)) {
        vel = &w->pose.dir;
        Vec3_Normalize(vel, vel);
        w->pose.pos.x = task->pos.x + -vel->x * w->radius;
        w->pose.pos.y = task->pos.y + -w->pose.dir.y * w->radius;
        w->pose.pos.z = task->pos.z + -w->pose.dir.z * w->radius;
        if (task->hit & EFT_R_HIT_STAGE) {
            EftGndDust_SpawnImpact(w->arg.objId, &w->pose.pos, 1.0f);
        }
        if (w->bounces < 3) {
            if (task->hit & EFT_R_HIT_OUT) {
                Vec3_Scale(&n, &w->pose.pos, -1.0f);
                Vec3_Normalize(&n, &n);
            } else {
                Vec4_Copy(&n, &w->contact.row[3]);
            }
            EftKiBomb_Reflect(&refl, vel, &n);
            speed = w->speed * 0.5f;
            w->spin *= 0.5f;
            r = w->radius + 1.0f;
            w->speed = speed;
            w->pose.pos.x = task->pos.x + n.x * r;
            w->pose.pos.y = task->pos.y + n.y * r;
            w->pose.pos.z = task->pos.z + n.z * r;
            vel->x = refl.x * w->speed;
            w->pose.dir.y = refl.y * w->speed;
            w->pose.dir.z = refl.z * w->speed;
            w->bounces++;
            if (task->hit & EFT_R_HIT_STAGE) {
                task->hit &= ~EFT_R_HIT_STAGE;
            }
            if (task->hit & EFT_R_HIT_OUT) {
                task->hit &= ~EFT_R_HIT_OUT;
            }
        } else {
            vel->x = 0.0f;
            w->pose.dir.y = 0.0f;
            w->pose.dir.z = 0.0f;
            w->gravity = 0.0f;
            w->spin = 0.0f;
            w->pose.pos.x = task->pos.x;
            w->pose.pos.y = task->pos.y - w->radius;
            w->pose.pos.z = task->pos.z;
            Vec4_Copy(&n, &w->contact.row[3]);
            w->pose.pos.x = task->pos.x + n.x * w->radius;
            w->pose.pos.y = task->pos.y + n.y * w->radius;
            w->pose.pos.z = task->pos.z + n.z * w->radius;
            if (!(w->flags & EFT_KIBOMB_REST)) {
                w->angle = EftMath_WrapAngle(angles[rand() % 3] * 3.14159265f / 180.0f);
                task->hit &= ~EFT_R_HIT_STAGE;
            }
            w->flags |= EFT_KIBOMB_REST;
        }
    }
}

/* Draw: the model's matrix: scale 3.5 x size, the spin angle, 1.5 below the bomb's position (+y is down). */
void EftKiBomb_UpdateModel(EftRTask *task) {
    EftRMtx m;
    EftRMtx s;
    EftRVec pos;
    EftKiBomb *w = task->work;

    if (w->handle != -1) {
        Mtx_StoreIdentity(&m);
        Mtx_StoreIdentity(&s);
        s.row[0].x = w->size * 3.5f;
        s.row[1].y = w->size * 3.5f;
        s.row[2].z = w->size * 3.5f;
        Mtx_Mul(&m, &m, &s);
        Mtx_RotateX(&m, &m, EftMath_WrapAngle(-w->angle));
        Vec4_Set(&pos, w->pose.pos.x, w->pose.pos.y + 1.5f, w->pose.pos.z, 1.0f);
        Mtx_SetTrans(&m, &pos);
        EftObj_SetMtx(w->handle, &m);
    }
}

/* ---- thrown object (ki blast type 2), first half ------------------------------------------------------------ */

/* Publishes this frame's hit record: two spheres of radius 5 x the argument's radius. */
void EftKiObj_AddHitRecord(EftRTask *task) {
    EftKiObj *w = task->work;
    EftRRec *rec = EftHit_GetNew();
    EftRSphere *a;
    EftRSphere *b;
    f32 radius;

    rec->task = task;
    rec->pose = w->b.pose;
    rec->unk54 = 0;
    rec->unk5C = (w->b.flags >> 5) & 1;
    rec->flags |= 0x400;
    rec->atk = w;
    radius = w->b.radius;
    a = EftHitArena_AllocSphere();
    b = EftHitArena_AllocSphere();
    ColSphere_Set(a, &w->b.pose.pos, radius);
    ColSphere_Set(b, &w->b.pose.prev, radius);
    EftHit_SetShapeSpheres(rec, a, b);
    EftHit_Add(rec);
}

/* Deflection / reflection of the object (called by the next file's post-update): as EftKiBlast_Turn; the new
   direction replaces the velocity. */
void EftKiObj_Turn(EftRTask *task, EftKiObj *w) {
    EftRVec dir;

    Vec3_Lerp(&w->b.pose.pos, &task->pos, &w->b.pose.prev, 0.7f);
    if (task->hit & EFT_R_HIT_DEFLECT) {
        BtlCharApi_GetDeflectDir((s16)(w->b.arg.objId ^ 1), &dir);
        Vec4_Copy(&w->b.pose.dir, &dir);
        w->b.arg.objId ^= 1;
        w->b.flags |= EFT_KIBOMB_DEFLECTED;
    } else if (task->hit & EFT_R_HIT_REFLECT) {
        BtlCharApi_GetDeflectDir((s16)(w->b.arg.objId ^ 1), &dir);
        Vec4_Copy(&w->b.pose.dir, &dir);
        w->b.arg.objId ^= 1;
        w->b.life = w->b.arg.life;
    }
}

/* Places the object at its node and throws it along the argument's direction (no jitter); spin rate from libc
   rand(). Creates the model instance. */
void EftKiObj_Launch(EftRTask *task) {
    EftRMtx m;
    EftKiObj *w = task->work;
    EftKiBombRes *res;
    f32 spin;

    BtlCharApi_GetNodePos(w->b.arg.objId, w->b.arg.node, &w->b.pose.start);
    BtlCharApi_GetNodePos(w->b.arg.objId, w->b.arg.node, &w->b.pose.pos);
    BtlCharApi_GetNodePos(w->b.arg.objId, 0x11, &w->b.pose.prev);
    Vec4_Copy(&w->b.pose.dir, &w->b.arg.dir);
    w->b.size = w->b.arg.radius;
    w->b.speed = w->b.arg.speed;
    w->b.life = w->b.arg.life;
    w->b.radius = w->b.arg.radius * 5.0f;
    w->b.gravity = 0.5f;
    w->b.blast = 0;
    w->b.maxLife = 0;
    spin = (f32)rand() / 2147483647.0f * 10.0f - 5.0f + 32.0f;
    w->b.spin = spin;
    w->b.spin = ((f32)rand() / 2147483647.0f < 0.5f) ? spin : -spin;
    Vec3_Scale(&w->b.pose.dir, &w->b.pose.dir, w->b.speed);
    w->b.unkD8 = 0;
    w->unk1D4 = 15;
    res = ((EftKiBombMgr *)BtlTask_GetParent(task)->work)->res;
    Mtx_StoreIdentity(&m);
    w->b.handle = EftObj_Create(w->b.handleBuf, res->model);
    if (w->b.handle != -1) {
        Mtx_SetTrans(&m, &w->b.pose.pos);
        EftObj_SetMtx(w->b.handle, &m);
    }
    w->tex = (u8 *)res + 8;
}

/* Builds the GS TEX0 value of a texture header once. */
void EftKiObj_StepTex(EftRTexAnim *tex) {
    if (tex != NULL && !(tex->flags & 1)) {
        u64 a = EftVram_AddImage(tex, 1, 0);
        u64 b = EftVram_AddClut(tex);

        tex->tex0 = a | (b << 37);
        tex->flags |= 1;
    }
}

/* Task init. */
void EftKiObj_Init(EftRTask *task, EftRArg *arg) {
    EftKiObj *w = task->work;

    memset(w, 0, sizeof(EftKiObj));
    w->b.arg = *arg;
    w->b.arg.srcId = arg->objId;
    EftKiObj_Launch(task);
    w->b.flags |= EFT_KIBOMB_SHOWN | EFT_KIBOMB_HITS;
    BtlTask_SetOwnerTag(task, arg->objId == 0 ? 0x800 : 0x1000);
}

/* Task term: frees the model instance. */
void EftKiObj_Term(EftRTask *task) {
    EftKiObj *w = task->work;

    if (w->b.handle != -1) {
        EftObj_Destroy(w->b.handle);
    }
}

/* Task update, unless any character is stopped: moves (next file), publishes the hit record, dies or readies the
   texture. */
void EftKiObj_Update(EftRTask *task) {
    EftKiObj *w = task->work;
    s32 dead; /* a dead store after a call keeps it from becoming a tail call, as in the original */

    if (!BtlScene_IsAnyCharStopped()) {
        EftKiObj_Step(task);
        if (w->b.flags & EFT_KIBOMB_HITS) {
            EftKiObj_AddHitRecord(task);
        }
        if (w->b.flags & EFT_KIBOMB_KILL) {
            BtlTask_SetDead(task);
            dead = 1;
        } else {
            EftKiObj_StepTex(w->tex);
            dead = 0;
        }
    }
}
