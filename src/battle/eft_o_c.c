#include "battle/btl_effect_owner.h"
#include "common.h"
#include "battle/eft_o.h"
#include "battle/eft_disc_capacity.h"

/*
 * Effect tasks, 0x16AE78..0x16C2E0. Two modules:
 *
 * 1. 0x16AE78..0x16B4E0  EftBodyFx: fighter effect request 0x1A (BtlFx_UpdateReq1A in btl_char_fx.c calls
 *    EftBodyFx_Start when the request appears and EftBodyFx_Stop when it ends). One task per fighter (class
 *    0x2C3B08, manager class 0x2C3AF0) that runs effect pack 0x23F of the common effect file on the fighter's
 *    body: position = node 3, direction = unit vector from node 3 to node 0x2E, size = the fighter's body
 *    scale (at least 0.5). VISUAL ONLY: no hit record, no fighter or battle state is written. No random draw
 *    here (the pack's part modules may draw their own). The task is skipped while BtlScene_IsEffectStopped
 *    (objId, 4).
 *
 * 2. 0x16B4E0..0x16C2E0  EftDisc, first part: the manager callbacks and the disc's init and update (the rest
 *    of the module is in eft_p.c: term, post-update, reset, draw, the hit record, homing and the creators).
 *    SIMULATION: the update moves the disc and, while it flies, publishes a hit record every frame
 *    (EftDisc_AddHit). What the update does, in order (verified by the matching code where the function
 *    matches, read from the disassembly otherwise; see the report):
 *      - a ki blast disc (kinds 0, 1, 3) is skipped while BtlScene_IsEffectStopped(owner, 0); it dies when
 *        the owner is changing form (BtlCharApi_IsChanging); a held kind 0 disc dies (flags DEAD | LOST)
 *        when the owner is no longer charging a ki blast (BtlCharApi_IsChargingKiBlast);
 *      - a technique piece (kinds 2, 4) is skipped while BtlScene_IsEffectStopped(owner, 1) or while the
 *        technique definition has flag 2;
 *      - held (flag HELD, kinds 0..2): it grows over growTime frames (size and alpha), and sits in front
 *        of the hand: pos = hand node B + unit(B - A) * size * 3, with A / B = nodes 0x14 / 0x15 or 0x22 /
 *        0x23, plus a vertical bob of cos(timer * 0.04 * pi). A kind 1 disc (AUTO) is released as soon as
 *        it is grown;
 *      - flying (flag FLYING): EftDisc_Home turns the direction toward the opponent, pos += dir * speed,
 *        the previous position is kept for the hit shape; kinds 3 and 4 fade in over growTime; every kind
 *        fades out over the last fadeTime frames of its life (or at once when END is set); age >= life
 *        kills it;
 *      - the model matrix is built (spin, bank, roll: appearance only);
 *      - END with the model no longer visible -> DEAD; a technique piece drives its pack parts and steps
 *        the pack's trail width; a flying disc that is neither ending nor dead adds its hit record; DEAD
 *        kills the task.
 *    Random draws: EftDisc_Init draws libc rand() once for kinds 0, 1 and 2: the spin rate (0.1 .. 0.11) *
 *    pi radians per frame. It only enters the model's rotation angle (rotY), not the position, direction
 *    or hit record.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 atan2f(f32 y, f32 x);
extern f32 cosf(f32 x);
extern f32 Mathf_Asin(f32 x);

extern void Vec4_Copy(void *dst, void *src);
extern void Vec4_Set(void *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Add(void *dst, void *a, void *b);
extern void Vec3_Sub(void *dst, void *a, void *b);
extern void Vec3_Scale(void *dst, void *src, f32 scale);
extern void Vec3_Normalize(void *dst, void *src);
extern f32 Vec3_Dot(void *a, void *b);
extern f32 EftMath_WrapAngle(f32 angle);
extern void Vu0Cur_LoadIdentity(void);        /* VU0 matrix stack: current = identity */
extern void Vu0Cur_Push(void);        /* push */
extern void Vu0Cur_Pop(void);        /* pop */
extern void Vu0Cur_StoreMtx(Mtx44 *m);    /* store the current matrix */
extern void Vu0Cur_Translate(void *pos);   /* translate */
extern void Vu0Cur_RotateZ(f32 angle);   /* rotate about Z */
extern void Vu0Cur_RotateX(f32 angle);   /* rotate about X */
extern void Vu0Cur_RotateY(f32 angle);   /* rotate about Y */
extern void Vu0Cur_ScaleDiagUniform(f32 scale);   /* scale */

extern void *BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(void *pool, s32 size);
extern void BtlPool_Free(void *pool, void *p);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern f32 BtlScene_GetCharScale(s32 chr);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern void *BtlTask_CreateChildList(EftOTask *task, s32 count, s32 workSize);
extern EftOTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftOTask *task);            /* kill a task */
extern void BtlTask_SetOwnerTag(EftOTask *task, s32 flags); /* or into the task's class flags */
extern void EftTexSet_Load32(void *set, s32 *pack);      /* binds a texture set to a pack */

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, void *out);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern s32 BtlCharApi_IsLockedOn(s32 objId);
extern s32 BtlCharApi_IsChargingKiBlast(s32 objId);
extern s32 BtlCharApi_IsChanging(s32 objId);

extern void EftEmit_LoadSet(void *owner, EftOSet *set, void *head, s32 *base, s32 common, s32 idx);
extern void EftEmit_FreeSet(EftOSet *set);
extern void EftEmit_BeginFrame(EftOSet *set);
extern s32 EftEmit_GetEndFrames(EftOSet *set);
extern void EftEmit_InitState(EftOSet *set, void *state);
extern void EftEmit_TermState(EftOSet *set, void *state);
extern s32 EftEmit_GetFlagsFromMask(EftOSet *set, void *state, s32 objId, s32 type, s32 idx, s32 ending, s32 kill,
                                    s32 mask);
extern void EftEmit_Spawn(EftOSet *set, void *state, EftOEmitNodes *nodes, void *pos, void *dir, s32 objId,
                          s32 node, s32 arg7, s32 type, s32 idx, s32 flags, f32 scale);
extern void EftEmit_KillAll(EftOSet *set, void *state);
extern s32 EftEmit_UpdateAlive(EftOSet *set, void *state);
extern void EftEmit_SetNode(EftOEmitNodes *nodes, s32 slot, s32 node, void *pos);
extern void EftEmit_RefreshFixedNodes(s32 objId, EftOEmitNodes *nodes);
extern void EftEmit_UpdateTrailWidth(EftOSet *set, void *state);

extern void EftMesh_Init(void *model, s32 *data);    /* model template from pack data */
extern void EftMesh_SetTex(void *model, void *tex);    /* its texture set */
extern void EftMesh_SetMtx(void *model, Mtx44 *m);     /* its matrix */
extern void EftMesh_Copy(void *model, void *proto);  /* instance of a template */
extern void EftMesh_SetTexBase(void *model, s32 a1);
extern void EftMesh_SetOwner(void *model, s32 objId, s32 a2);

extern void EftDisc_SpawnParts(EftOTask *task, s32 mode);
extern void EftDisc_AddHit(EftOTask *task);
extern void EftDisc_SetTex(EftODisc *w, void *tex, s32 a, s32 b);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void EftDisc_Home(void *out, void *pos, void *dir, s32 objId, f32 speed, f32 maxTurn, f32 bank);
#define EftDisc_Home(speed, maxTurn, bank, out, pos, dir, objId) EftDisc_Home(out, pos, dir, objId, speed, maxTurn, bank)
#else
extern void EftDisc_Home(f32 speed, f32 maxTurn, f32 bank, void *out, void *pos, void *dir, s32 objId);
#endif
extern void EftDisc_StepTex(EftODisc *w);
extern s32 EftDiscMgr_PushHeld(EftOTask *task, s32 objId, s32 kind);

extern void *gEftBodyFxClass[6];

/* ---- fighter effect request 0x1A: effect pack 0x23F on the body ------------------------------------------- */

/* Drives every part of the pack with the phase mask of this frame. */
void EftBodyFx_UpdateParts(s32 objId, EftOTask *task, EftOSet *set) {
    EftBodyFx *w = task->work;
    s32 g;
    s32 p;

    for (g = 0; g < 19; g++) {
        if (*set->mask & (1 << g)) {
            EftOGroupDef *gd = set->grp[g].def;

            for (p = 0; p < gd->count; p++) {
                s32 f = EftEmit_GetFlagsFromMask(set, w->state, objId, g, p, w->flags & 2, 0, w->phase);

                if (f != 0) {
                    EftEmit_Spawn(set, w->state, &w->nodes, &w->pos, &w->dir, objId, 0, 4, g, p, f, w->scale);
                }
            }
        }
    }
}

/* Init callback. */
void EftBodyFx_Init(EftOTask *task, EftBodyFxArg *arg) {
    Vec4 tip;
    EftBodyFx *w = task->work;
    EftOVec *dir = &w->dir;

    memset(w, 0, sizeof(EftBodyFx));
    w->arg = *arg;
    w->flags |= 1;
    w->scale = BtlScene_GetCharScale(arg->objId);
    if (w->scale < 0.5f) {
        w->scale = 0.5f;
    }
    BtlCharApi_GetNodePos(arg->objId, 3, &w->pos);
    BtlCharApi_GetNodePos(arg->objId, 3, dir);
    BtlCharApi_GetNodePos(arg->objId, 0x2E, &tip);
    Vec3_Sub(dir, &tip, dir);
    w->dir.w = 1.0f;
    Vec3_Normalize(dir, dir);
    w->set = &gEftBodyFx->set[arg->unk4];
    EftEmit_InitState(w->set, w->state);
    w->phase |= 1;
    EftEmit_SetNode(&w->nodes, 0, 3, NULL);
    w->flags |= 8;
    w->endLife = EftEmit_GetEndFrames(w->set);
    BtlTask_SetOwnerTag(task, BtlScene_CharOwnerTag(arg->objId));
}

/* Term callback. */
void EftBodyFx_Term(EftOTask *task) {
    EftBodyFx *w = task->work;
    EftBodyFxArg *arg = &w->arg;

    EftEmit_TermState(w->set, w->state);
    w->flags = 0;
    gEftBodyFx->task[arg->objId] = NULL;
}

/* Update callback: follows the fighter; after the stop it lives for the pack's end frames. */
void EftBodyFx_Update(EftOTask *task) {
    Vec4 tip;
    EftBodyFx *w = task->work;
    EftBodyFxArg *arg = &w->arg;
    EftOVec *dir = &w->dir;

    if (!BtlScene_IsEffectStopped(arg->objId, 4)) {
        BtlCharApi_GetNodePos(arg->objId, 3, &w->pos);
        BtlCharApi_GetNodePos(arg->objId, 3, dir);
        BtlCharApi_GetNodePos(arg->objId, 0x2E, &tip);
        Vec3_Sub(dir, &tip, dir);
        w->dir.w = 1.0f;
        Vec3_Normalize(dir, dir);
        EftEmit_RefreshFixedNodes(arg->objId, &w->nodes);
        EftBodyFx_UpdateParts(arg->objId, task, w->set);
        if (w->flags & 2) {
            w->endTime += 1.0f;
            if (w->endLife <= w->endTime) {
                w->flags |= 4;
            }
        }
    }
    if (w->flags & 4) {
        BtlTask_SetDead(task);
    }
}

/* Reset callback: the parts and the task die at once. */
void EftBodyFx_Reset(EftOTask *task) {
    EftBodyFx *w = task->work;

    if (!(w->flags & 0x20)) {
        w->flags |= 0x20;
        EftEmit_KillAll(w->set, w->state);
    }
    BtlTask_SetDead(task);
}

/* Post-update callback: from the second frame on, drops finished parts and clears the phase mask. */
void EftBodyFx_PostUpdate(EftOTask *task) {
    EftBodyFx *w = task->work;

    if (w->flags & 0x10) {
        if (!EftEmit_UpdateAlive(w->set, w->state)) {
            w->flags &= ~8;
        } else {
            w->flags |= 8;
        }
        w->phase = 0;
    }
    w->flags |= 0x10;
}

/* Draw callback: nothing. */
void EftBodyFx_Draw(EftOTask *task) {
}

/* Init callback of the manager: loads the effect packs (one: number 0x23F of the common effect file); 3 tasks. */
void EftBodyFxMgr_Init(EftOTask *task) {
    s32 id[EFT_BODYFX_SETS];
    s32 i;

    gEftBodyFx = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftBodyFxMgr));
    memset(gEftBodyFx, 0, sizeof(EftBodyFxMgr));
    gEftBodyFxList = BtlTask_CreateChildList(task, sizeof(gEftBodyFx->task)/sizeof(gEftBodyFx->task[0])+1, sizeof(EftBodyFx));
    id[0] = 0x23F;
    for (i = 0; i < EFT_BODYFX_SETS; i++) {
        EftEmit_LoadSet(NULL, &gEftBodyFx->set[i], NULL, NULL, 1, id[i]);
    }
}

/* Term callback of the manager. */
void EftBodyFxMgr_Term(void) {
    s32 i;

    for (i = 0; i < EFT_BODYFX_SETS; i++) {
        EftEmit_FreeSet(&gEftBodyFx->set[i]);
    }
    BtlPool_Free(BtlPool_GetCurrent(), gEftBodyFx);
    gEftBodyFx = NULL;
}

/* Update callback of the manager. */
void EftBodyFxMgr_Update(void) {
    s32 i;

    for (i = 0; i < EFT_BODYFX_SETS; i++) {
        EftEmit_BeginFrame(&gEftBodyFx->set[i]);
    }
}

/* Starts the effect on a fighter; fails when it is already running. */
s32 EftBodyFx_Start(EftBodyFxArg arg) {
    if(!gEftBodyFx || (u32)arg.objId>=sizeof(gEftBodyFx->task)/sizeof(gEftBodyFx->task[0]) ||
       (u32)arg.unk4>=EFT_BODYFX_SETS)return 0;
    if (gEftBodyFx->task[arg.objId] != NULL) {
        return 0;
    }
    gEftBodyFx->task[arg.objId] = BtlTaskList_AddTail(gEftBodyFxList, gEftBodyFxClass, &arg);
    return gEftBodyFx->task[arg.objId] != NULL;
}

/* Asks the fighter's effect to end. */
s32 EftBodyFx_Stop(s32 objId) {
    EftOTask *t;
    EftBodyFx *w;

    if(!gEftBodyFx || (u32)objId>=sizeof(gEftBodyFx->task)/sizeof(gEftBodyFx->task[0]))return 0;
    t=gEftBodyFx->task[objId];
    if (t == NULL) {
        return 0;
    }
    w = t->work;
    if (!(w->flags & 1)) {
        return 0;
    }
    w->flags |= 2;
    return 1;
}

/* ---- disc: manager, init and update ------------------------------------------------------------------------ */

/* Init callback of the manager: two texture sets, two model templates, a list of 20 discs. */
void EftDiscMgr_Init(EftOTask *task) {
    extern u32 EftDiscMgr_WorkSize(void);
    gEftDisc = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftODiscMgr));
    gEftDisc->res = NULL;
    gEftDisc->res = BtlPool_Alloc(BtlPool_GetCurrent(), EftDiscMgr_WorkSize());
    memset(gEftDisc->res,0,EftDiscMgr_WorkSize());
    gEftDisc->res->packTexA = BtlScene_GetCommonEntry(0x238);
    gEftDisc->res->packTexB = BtlScene_GetCommonEntry(0x23A);
    gEftDisc->res->packModelA = BtlScene_GetCommonEntry(0x239);
    gEftDisc->res->packModelB = BtlScene_GetCommonEntry(0x23B);
    EftTexSet_Load32(&gEftDisc->res->texA, gEftDisc->res->packTexA);
    EftTexSet_Load32(&gEftDisc->res->texB, gEftDisc->res->packTexB);
    EftMesh_Init(gEftDisc->res->modelA, gEftDisc->res->packModelA);
    EftMesh_Init(gEftDisc->res->modelB, gEftDisc->res->packModelB);
    EftMesh_SetTex(gEftDisc->res->modelA, &gEftDisc->res->texA);
    EftMesh_SetTex(gEftDisc->res->modelB, &gEftDisc->res->texB);
    EftMesh_SetTexBase(gEftDisc->res->modelA, 0);
    EftMesh_SetTexBase(gEftDisc->res->modelB, 0);
    gEftDisc->res->unk410 = 0;
    gEftDisc->res->list = BtlTask_CreateChildList(task, EFT_DISC_CAPACITY, sizeof(EftODisc));
}

/* Update callback of the manager: the texture sets have not been advanced this frame. */
void EftDiscMgr_Update(void) {
    gEftDisc->res->texA.unk204 = 0;
    gEftDisc->res->texB.unk204 = 0;
}

/* Reset callback of the manager: nothing. */
void EftDiscMgr_Reset(void) {
}

/* Term callback of the manager. */
void EftDiscMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftDisc->res);
    BtlPool_Free(BtlPool_GetCurrent(), gEftDisc);
    gEftDisc = NULL;
}

/* Init callback of a disc. */
void EftDisc_Init(EftOTask *task, EftODiscArg *arg) {
    EftODisc *w = task->work;
    void *model;
    f32 life;
    f32 roll;

    memset(w, 0, sizeof(EftODisc));
    w->arg = *arg;
    BtlCharApi_GetNodePos(arg->objId, arg->hand ? 0x15 : 0x23, &arg->pos);
    BtlCharApi_GetNodePos(arg->objId, 0x11, &w->prev);
    Vec4_Copy(&w->prev2, &w->prev);
    Vec4_Copy(&w->dir, &arg->dir);
    Vec4_Set(w->color, 127.0f, 127.0f, 127.0f, 0.0f);
    w->rollAngle = EftMath_WrapAngle(arg->roll * 6.2831853f);
    w->rollLeft = w->rollTime = arg->rollTime * 30.0f;
    if (arg->rollTime < 0.0f) {
        w->rollTime = 1.0f;
        w->rollLeft = -1.0f;
    }
    w->size = BtlCharApi_GetHeight(arg->objId) / 19.35f;
    life = arg->life * 30.0f;
    w->age = 0.0f;
    w->life = life;
    w->growTime = arg->grow * 30.0f;
    w->fadeLeft = w->fadeTime = life * (1.0f - arg->fade);
    switch (arg->kind) {
    case 0:
    case 1:
        roll = w->rollAngle;
        if (arg->hand == 0) {
            roll = -roll;
        }
        w->rollAngle = roll;
        w->spin = ((f32)rand() / 2147483647.0f * 0.01f + 0.1f) * 3.14159265f;
        model = w->model;
        EftMesh_Copy(model, gEftDisc->res->modelA);
        if (!EftDiscMgr_PushHeld(task, arg->objId, arg->kind)) {
            w->flags |= EFT_ODISC_DEAD;
        }
        if (arg->kind == 1) {
            w->flags |= EFT_ODISC_AUTO;
        }
        w->flags |= EFT_ODISC_HELD;
        w->size *= 1.5f;
        break;
    case 2:
        roll = w->rollAngle;
        if (arg->hand == 0) {
            roll = -roll;
        }
        w->rollAngle = roll;
        w->spin = ((f32)rand() / 2147483647.0f * 0.01f + 0.1f) * 3.14159265f;
        model = w->model;
        EftEmit_InitState(arg->set, w->state);
        EftMesh_Copy(model, gEftDisc->res->modelA);
        w->flags |= EFT_ODISC_PIECE | EFT_ODISC_HELD;
        break;
    case 3:
        w->rotY = 3.14159265f;
        model = w->model;
        w->grown = 1.0f;
        EftMesh_Copy(model, gEftDisc->res->modelB);
        w->flags |= EFT_ODISC_FLYING;
        break;
    case 4:
        w->rotY = 3.14159265f;
        w->grown = 1.0f;
        model = w->model;
        EftEmit_InitState(arg->set, w->state);
        EftMesh_Copy(model, gEftDisc->res->modelB);
        w->flags |= EFT_ODISC_PIECE | EFT_ODISC_FLYING;
        break;
    default:
        model = w->model;
        break;
    }
    if (!(w->flags & EFT_ODISC_PIECE)) {
        EftMesh_SetOwner(model, arg->objId, 0);
    } else {
        EftMesh_SetOwner(model, arg->objId, 1);
    }
    EftDisc_SetTex(w, w->tex, arg->texA, arg->texB);
    w->flags |= EFT_ODISC_ALIVE;
    BtlTask_SetOwnerTag(task, BtlScene_CharOwnerTag(arg->objId));
}

/* The roll decay block needs its own zero / clamp variable `k` (not the function-wide `t`) and the clamp written
   as an `if` around a ?: so that rollLeft is allocated before the clamp result (f2 / f3). */
#define EFT_O_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((hi) < (x) ? (hi) : (x)))

/* Update callback of a disc: see the notes at the top of the file. */
void EftDisc_Update(EftOTask *task) {
    Vec4 v[2];
    Vec4 c;
    Mtx44 mtx;
    s32 hold = 0;
    f32 pitch = 0.0f;
    EftODisc *w = task->work;
    EftODiscArg *arg = &w->arg;
    s32 stopped;
    f32 yaw;
    f32 r;
    f32 t;

    if (!(w->flags & EFT_ODISC_PIECE)) {
        stopped = BtlScene_IsEffectStopped(arg->objId, 0);
        if (BtlCharApi_IsChanging(arg->objId)) {
            w->flags |= EFT_ODISC_DEAD;
        }
        if (arg->kind == 0 && (w->flags & EFT_ODISC_HELD) && !BtlCharApi_IsChargingKiBlast(arg->objId)) {
            w->flags |= EFT_ODISC_LOST | EFT_ODISC_DEAD;
        }
    } else {
        stopped = BtlScene_IsEffectStopped(arg->objId, 1);
        hold = (arg->src->def->flags >> 1) & 1;
    }
    if (!stopped && !hold) {
        memset(v, 0, sizeof(v));
        memset(&c, 0, sizeof(c));
        if (w->flags & EFT_ODISC_HELD) {
            if (arg->kind < 3) {
                if (w->timer < w->growTime) {
                    r = w->timer / w->growTime;
                    t = EFT_O_CLAMP(r, 0.0f, 1.0f);
                    w->grown = t;
                    w->alpha = t * 127.0f;
                } else {
                    w->alpha = 127.0f;
                    w->grown = 1.0f;
                    w->flags |= EFT_ODISC_GROWN;
                }
                if (arg->hand != 0) {
                    BtlCharApi_GetNodePos(arg->objId, 0x14, &v[0]);
                    BtlCharApi_GetNodePos(arg->objId, 0x15, &v[1]);
                } else {
                    BtlCharApi_GetNodePos(arg->objId, 0x22, &v[0]);
                    BtlCharApi_GetNodePos(arg->objId, 0x23, &v[1]);
                }
                Vec3_Sub(&c, &v[1], &v[0]);
                Vec3_Normalize(&c, &c);
                Vec3_Scale(&c, &c, w->size * 3.0f);
                Vec3_Add(&arg->pos, &v[1], &c);
                arg->pos.y += cosf(EftMath_WrapAngle(w->timer * 0.04f * 3.14159265f));
            }
            if (w->flags & EFT_ODISC_AUTO) {
                if (w->flags & EFT_ODISC_GROWN) {
                    w->flags &= ~EFT_ODISC_HELD;
                    w->flags |= EFT_ODISC_FLYING;
                }
            }
        }
        memset(&v[0], 0, sizeof(Vec4));
        memset(&v[1], 0, sizeof(Vec4));
        memset(&c, 0, sizeof(c));
        if (w->flags & EFT_ODISC_FLYING) {
            EftDisc_Home(arg->speed, arg->turn, arg->bank, &arg->dir, &arg->pos, &arg->dir, arg->objId);
            Vec4_Copy(&w->dir, &arg->dir);
            Vec3_Scale(&c, &arg->dir, arg->speed);
            if (w->flags & EFT_ODISC_MOVED) {
                Vec4_Copy(&w->prev2, &w->prev);
                Vec4_Copy(&w->prev, &arg->pos);
            } else {
                BtlCharApi_GetNodePos(arg->objId, 0x11, &w->prev);
                Vec4_Copy(&w->prev2, &w->prev);
            }
            Vec3_Add(&arg->pos, &arg->pos, &c);
            pitch = EftMath_WrapAngle(Mathf_Asin(-arg->dir.y));
            if (!(arg->kind < 3)) {
                if (w->timer < w->growTime) {
                    r = w->timer / w->growTime;
                    t = EFT_O_CLAMP(r, 0.0f, 1.0f);
                    w->alpha = t * 127.0f;
                } else {
                    w->alpha = 127.0f;
                }
            }
            if (w->life - w->fadeTime <= w->age || (w->flags & EFT_ODISC_END)) {
                r = w->fadeLeft / w->fadeTime;
                t = EFT_O_CLAMP(r, 0.0f, 1.0f);
                w->alpha = t * 127.0f;
                w->fadeLeft -= 1.0f;
            }
            if (arg->kind < 3) {
                {
                    f32 k = 0.0f;

                    if (k < w->rollLeft) {
                        r = w->rollLeft / w->rollTime;
                        if (!(r < k)) {
                            k = (1.0f < r) ? 1.0f : r;
                        }
                        w->rollAngle = w->rollAngle * k;
                        w->rollLeft -= 1.0f;
                    }
                }
                if (BtlCharApi_IsLockedOn(arg->objId)) {
                    f32 side;

                    BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(arg->objId), 3, &v[0]);
                    Vec3_Sub(&v[1], &v[0], &arg->pos);
                    Vec3_Normalize(&v[1], &v[1]);
                    side = w->dir.z * v[1].x - w->dir.x * v[1].z;
                    if (0.0f <= Vec3_Dot(&w->dir, &v[1])) {
                        f32 bank = side * 3.14159265f * 1.2f;

                        if (bank < -0.78539816f) {
                            w->bankAngle = -0.78539816f;
                        } else if (0.78539816f < bank) {
                            w->bankAngle = 0.78539816f;
                        } else {
                            w->bankAngle = bank;
                        }
                    }
                }
            }
            w->age += 1.0f;
            if (w->life <= w->age) {
                w->flags |= EFT_ODISC_DEAD;
            }
            w->flags |= EFT_ODISC_MOVED;
        }
        if (w->grown <= 0.0f || w->alpha <= 0.0f) {
            w->flags &= ~EFT_ODISC_SHOWN;
        } else {
            w->flags |= EFT_ODISC_SHOWN;
        }
        if (arg->kind < 3) {
            w->rotY += w->spin;
            w->rotY = EftMath_WrapAngle(-w->rotY);
        }
        if (!(arg->kind < 3)) {
            w->rotX += w->spin;
            w->rotX = EftMath_WrapAngle(w->rotX);
        }
        yaw = EftMath_WrapAngle(atan2f(arg->dir.x, arg->dir.z));
        Vu0Cur_Push();
        Vu0Cur_LoadIdentity();
        Vu0Cur_ScaleDiagUniform(arg->scale * w->size * w->grown);
        Vu0Cur_RotateY(w->rotY);
        Vu0Cur_RotateX(w->rotX);
        Vu0Cur_RotateZ(EftMath_WrapAngle(w->bankAngle + w->rollAngle));
        Vu0Cur_RotateX(pitch);
        Vu0Cur_RotateY(yaw);
        Vu0Cur_Translate(&arg->pos);
        Vu0Cur_StoreMtx(&mtx);
        EftMesh_SetMtx(w->model, &mtx);
        Vu0Cur_Pop();
        if (w->flags & EFT_ODISC_END) {
            if (!(w->flags & EFT_ODISC_SHOWN)) {
                w->flags |= EFT_ODISC_DEAD;
            }
        }
        if (w->flags & EFT_ODISC_PIECE) {
            EftDisc_SpawnParts(task, 0);
            EftEmit_UpdateTrailWidth(arg->set, w->state);
        }
        if (w->flags & EFT_ODISC_FLYING) {
            if (!(w->flags & (EFT_ODISC_END | EFT_ODISC_DEAD))) {
                EftDisc_AddHit(task);
            }
        }
        if (w->flags & EFT_ODISC_DEAD) {
            w->flags &= ~EFT_ODISC_ALIVE;
            BtlTask_SetDead(task);
        }
        w->timer += 1.0f;
    }
    if (!(w->flags & EFT_ODISC_DEAD)) {
        EftDisc_StepTex(w);
    }
}
