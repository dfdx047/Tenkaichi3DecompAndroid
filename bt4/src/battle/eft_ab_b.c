#include "common.h"
#include "battle/eft_ab.h"

/*
 * Transformation / fusion effect, 0x1A0020..0x1A0E58. Manager class 0x2C42C0 = {EftTransformMgr_Update, _Init,
 * _Term, 0, 0, 0}; task class 0x2C42D8 (gEftTransformClass) = {EftTransform_Update, _Init, _Term, _PostUpdate,
 * _Reset, _Draw}. One effect at a time (EftTransformMgr.busy).
 *
 * Driven by the fighter effect layer (btl_char_fx.c BtlChar_SpawnFxBits0), by the fighter's change kind (+0x12DC):
 *   request 0  EftTransform_Start {objId, kind}: the effect starts with its "before" emitter groups;
 *   request 1  EftTransform_Flash: the moment the model changes. The stage effects stop drawing, stage tint 1 goes
 *              on, the fighter is hidden (draw mask bit 3); ten frames later the tint goes off, the fighter is shown
 *              again, two screen shock waves start at node 3 and the "after" group starts;
 *   request 2  EftTransform_End: the emitters run their end frames and the task dies.
 * Kinds 2 / 3 wait for attribute 0x1000 on the fighter or on its partner (the second model of a fusion) and put a
 * node slot at that object's node 0x36. Kind 5 draws the stage sprite (EftStage_DrawSprite) at the fighter's node
 * 0x36 while attribute 0x4000 has toggled it on. Kind 6 binds node slots from attributes 0x200 / 0x400 / 0x1000 /
 * 0x2000.
 *
 * Visual, with one write outside its own data: BtlCharApi_ObjSetMaskBit3 / ClearMaskBit3 on the fighter (hides and
 * shows the model). It reads the fighter's height, node positions, attributes and partner object id.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void Vec4_Set(EftAbVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Normalize(EftAbVec *dst, EftAbVec *src);

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern void *BtlTask_CreateChildList(EftAbTask *task, s32 count, s32 workSize);
extern EftAbTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftAbTask *task);                         /* marks a task as dying */

extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_GetPartnerObjId(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, EftAbVec *out);
extern void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out);
extern s32 BtlCharApi_ObjTestAttr(s32 objId, u64 mask);
extern s32 BtlCharApi_ObjGetAttrKind(s32 objId, u64 mask);
extern void BtlCharApi_ObjSetMaskBit3(s32 objId);
extern void BtlCharApi_ObjClearMaskBit3(s32 objId);

extern void EftEmit_LoadSet(void *arg, EftAbSet *set, s32 a2, s32 *pack, s32 a4, s32 id);
extern void EftEmit_FreeSet(EftAbSet *set);
extern void EftEmit_BeginFrame(EftAbSet *set);
extern s32 EftEmit_GetEndFrames(EftAbSet *set);
extern void EftEmit_InitState(EftAbSet *set, void *state);
extern void EftEmit_TermState(EftAbSet *set, void *state);
extern s32 EftEmit_GetFlagsFromMask(EftAbSet *set, void *state, s32 objId, s32 part, s32 sub, s32 end, s32 fast,
                                    s32 mask);
extern void EftEmit_Spawn(EftAbSet *set, void *state, void *nodes, EftAbVec *pos, EftAbVec *dir, s32 objId, s32 node,
                          s32 arg7, s32 part, s32 sub, s32 flags, f32 scale);
extern void EftEmit_KillAll(EftAbSet *set, void *state);
extern s32 EftEmit_UpdateAlive(EftAbSet *set, void *state);
extern void EftEmit_SetNode(void *nodes, s32 slot, s32 node, EftAbVec *pos);
extern void EftEmit_RefreshFixedNodes(s32 objId, void *nodes);

extern void EftStage_SetDrawOn(s32 on);
extern void EftStage_UpdateSprite(void);
extern void EftStage_DrawSprite(EftAbVec *pos);
extern void StgTint_Start(u32 slot, s32 dir, f32 seconds);
extern s32 ScrWarp_Spawn(s32 view, EftAbVec *pos, f32 seconds, f32 radius, f32 width, f32 speed, f32 jitter);

/* Steps every emitter of the set and spawns its particles at the task's position. */
void EftTransform_SpawnParts(s32 objId, EftAbTask *task, EftAbSet *set) {
    EftTransform *w = task->work;
    s32 part;
    s32 sub;

    for (part = 0; part < 0x13; part++) {
        if (*set->mask & (1 << part)) {
            EftAbPartDef *def = set->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 spawn = EftEmit_GetFlagsFromMask(set, w->state, objId, part, sub, w->flags & EFT_TF_END, 0, w->groups);

                if (spawn != 0) {
                    EftEmit_Spawn(set, w->state, w->nodes, &w->pos, &w->dir, objId, 0, 5, part, sub, spawn, w->scale);
                }
            }
        }
    }
}

/* The emitter set of a kind: kinds 1..4 share set 1. */
EftAbSet *EftTransform_GetSet(s32 kind) {
    EftAbSet *set = NULL;

    switch (kind) {
    case 0:
        set = &gEftTransform->set[0];
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        set = &gEftTransform->set[1];
        break;
    case 5:
        set = &gEftTransform->set[2];
        break;
    case 6:
        set = &gEftTransform->set[3];
        break;
    case 7:
        set = &gEftTransform->set[4];
        break;
    case 8:
        set = &gEftTransform->set[5];
        break;
    case 9:
        set = &gEftTransform->set[6];
        break;
    case 10:
        set = &gEftTransform->set[7];
        break;
    }
    return set;
}

/* True for the kinds whose first emitter group starts at once, at node 3: 0, 1 and 7..10. */
s32 EftTransform_HasStartGroup(s32 kind) {
    if (kind < 7) {
        return kind < 2;
    }
    return 1;
}

/* Per-frame placement: where the emitters spawn and which node slots are bound, by kind. */
void EftTransform_UpdateNodes(EftAbTask *task, s32 kind) {
    Mtx44 m;
    EftTransform *w = task->work;
    EftTransformArg *arg = &w->arg;

    switch (kind) {
    case 5:
        if (BtlCharApi_ObjTestAttr(arg->objId, 0x4000)) {
            if (!(w->flags & EFT_TF_SPRITE)) {
                w->flags |= EFT_TF_SPRITE;
            } else {
                w->flags &= ~EFT_TF_SPRITE;
            }
        }
        BtlCharApi_GetNodePos(arg->objId, 0x36, &w->pos);
        break;
    case 6:
        if (BtlCharApi_ObjTestAttr(arg->objId, 0x200)) {
            EftEmit_SetNode(w->nodes, 0, BtlCharApi_ObjGetAttrKind(arg->objId, 0x200), NULL);
            w->groups |= 1;
        }
        if (BtlCharApi_ObjTestAttr(arg->objId, 0x400)) {
            s32 node = BtlCharApi_ObjGetAttrKind(arg->objId, 0x400);

            EftEmit_SetNode(w->nodes, 1, node, NULL);
            w->groups |= 2;
            w->flags |= EFT_TF_ATTACHED;
            BtlCharApi_GetNodePos(arg->objId, node, &w->pos);
            BtlCharApi_GetNodeMtx(arg->objId, node, &m);
            Vec4_Set(&w->dir, -m.m[2][0], -m.m[2][1], -m.m[2][2], 1.0f);
            Vec3_Normalize(&w->dir, &w->dir);
        }
        if (BtlCharApi_ObjTestAttr(arg->objId, 0x1000)) {
            EftEmit_SetNode(w->nodes, 3, 0x2E, NULL);
            w->groups |= 8;
        }
        if (BtlCharApi_ObjTestAttr(arg->objId, 0x2000)) {
            EftEmit_SetNode(w->nodes, 4, BtlCharApi_ObjGetAttrKind(arg->objId, 0x2000), NULL);
            w->groups |= 0x10;
        }
        if (w->flags & EFT_TF_ATTACHED) {
            BtlCharApi_GetNodePos(arg->objId, 0x36, &w->pos);
        }
        break;
    case 2:
        if (!(w->flags & EFT_TF_JOINED)) {
            s32 objId = arg->objId;
            s32 partner = BtlCharApi_GetPartnerObjId(objId);

            if (BtlCharApi_ObjTestAttr(arg->objId, 0x1000)) {
                w->flags |= EFT_TF_JOINED;
            } else if (partner >= 0 && BtlCharApi_ObjTestAttr(partner, 0x1000)) {
                objId = partner;
                w->flags |= EFT_TF_JOINED;
            }
            if (w->flags & EFT_TF_JOINED) {
                BtlCharApi_GetNodePos(objId, 0x36, &w->pos);
                EftEmit_SetNode(w->nodes, 3, -1, &w->pos);
                w->groups |= 8;
            }
        }
        break;
    case 3:
        if (!(w->flags & EFT_TF_JOINED)) {
            s32 objId = arg->objId;
            s32 partner = BtlCharApi_GetPartnerObjId(objId);

            if (BtlCharApi_ObjTestAttr(arg->objId, 0x1000)) {
                w->flags |= EFT_TF_JOINED;
            } else if (partner >= 0 && BtlCharApi_ObjTestAttr(partner, 0x1000)) {
                objId = partner;
                w->flags |= EFT_TF_JOINED;
            }
            if (w->flags & EFT_TF_JOINED) {
                BtlCharApi_GetNodePos(objId, 0x36, &w->pos);
                EftEmit_SetNode(w->nodes, 4, -1, &w->pos);
                w->groups |= 0x10;
            }
        }
        break;
    default:
        BtlCharApi_GetNodePos(arg->objId, 3, &w->pos);
        break;
    }
}

/* What the flash does to the emitters: kind 4 starts group 3 at node 3, kind 6 kills everything. */
void EftTransform_OnFlash(EftAbTask *task, s32 kind) {
    EftTransform *w = task->work;

    switch (kind) {
    case 6:
        EftEmit_KillAll(w->set, w->state);
        break;
    case 4:
        EftEmit_SetNode(w->nodes, 3, 3, NULL);
        w->groups |= 8;
        break;
    }
}

/* Init callback: scale from the fighter's height, emitter state, first group for the kinds that have one. */
void EftTransform_Init(EftAbTask *task, EftTransformArg *req) {
    EftTransform *w = task->work;
    EftTransformArg *arg;

    memset(w, 0, sizeof(EftTransform));
    arg = &w->arg;
    *arg = *req;
    w->scale = BtlCharApi_GetHeight(arg->objId) / 19.35f;
    if (w->scale < 0.5f) {
        w->scale = 0.5f;
    }
    BtlCharApi_GetNodePos(arg->objId, 3, &w->pos);
    Vec4_Set(&w->dir, 0.0f, -1.0f, 0.0f, 1.0f);
    w->set = EftTransform_GetSet(req->kind);
    EftEmit_InitState(w->set, w->state);
    w->delay = 10.0f;
    if (EftTransform_HasStartGroup(req->kind)) {
        w->groups |= 1;
        EftEmit_SetNode(w->nodes, 0, 3, NULL);
    }
    BtlCharApi_GetNodePos(arg->objId, 3, &w->pos);
    w->flags |= EFT_TF_ALIVE;
    w->endFrames = EftEmit_GetEndFrames(w->set);
    gEftTransform->busy = 1;
}

/* Term callback: undoes the tint and the hidden fighter if the task dies between flash and reveal. */
void EftTransform_Term(EftAbTask *task) {
    EftAbVec pos;
    EftTransform *w = task->work;
    EftTransformArg *arg = &w->arg;

    if (w->flags & EFT_TF_TINT) {
        StgTint_Start(1, 1, 0.0f);
        BtlCharApi_ObjClearMaskBit3(arg->objId);
        BtlCharApi_GetNodePos(arg->objId, 3, &pos);
        ScrWarp_Spawn(arg->objId, &pos, 0.6f, 10.0f, 50.0f, 10.0f, 0.4f);
        ScrWarp_Spawn(arg->objId, &pos, 0.6f, 10.0f, 50.0f, 10.0f, 0.4f);
    }
    EftStage_SetDrawOn(1);
    EftEmit_TermState(w->set, w->state);
    if (!(w->flags & EFT_TF_DETACHED)) {
        gEftTransform->task = NULL;
        gEftTransform->busy = 0;
    }
}

/* Update callback: the flash sequence (task->step 0 start, 1 wait for the flash, 2 wait for the reveal). */
void EftTransform_Update(EftAbTask *task) {
    EftAbVec pos;
    EftTransform *w = task->work;
    EftTransformArg *arg = &w->arg;

    if (!BtlScene_IsEffectStopped(arg->objId, 5)) {
        EftTransform_UpdateNodes(task, arg->kind);
        switch (task->step) {
        case 0:
            task->step = 1;
            break;
        case 1:
            if (w->flags & EFT_TF_FLASH) {
                EftTransform_OnFlash(task, arg->kind);
                EftStage_SetDrawOn(0);
                StgTint_Start(1, 0, 0.0f);
                w->flags |= EFT_TF_TINT;
                BtlCharApi_ObjSetMaskBit3(arg->objId);
                task->step++;
            }
            break;
        case 2:
            w->timer += 1.0f;
            if (w->delay < w->timer) {
                if (w->flags & EFT_TF_TINT) {
                    StgTint_Start(1, 1, 0.0f);
                    BtlCharApi_ObjClearMaskBit3(arg->objId);
                    BtlCharApi_GetNodePos(arg->objId, 3, &pos);
                    ScrWarp_Spawn(arg->objId, &pos, 0.6f, 10.0f, 50.0f, 10.0f, 0.4f);
                    ScrWarp_Spawn(arg->objId, &pos, 0.6f, 10.0f, 50.0f, 10.0f, 0.4f);
                    w->flags &= ~EFT_TF_TINT;
                }
                w->flags |= EFT_TF_AFTER;
                task->step++;
            }
            break;
        }
        if (w->flags & EFT_TF_AFTER) {
            if (!(w->flags & EFT_TF_AFTER_ON)) {
                w->groups |= 2;
                EftEmit_SetNode(w->nodes, 1, 3, NULL);
                w->flags |= EFT_TF_AFTER_ON;
            }
        }
        EftEmit_RefreshFixedNodes(arg->objId, w->nodes);
        EftTransform_SpawnParts(arg->objId, task, w->set);
        if (w->flags & EFT_TF_END) {
            w->endTimer += 1.0f;
            if (w->endFrames <= w->endTimer) {
                w->flags |= EFT_TF_DEAD;
            }
        }
    }
    if (arg->kind == 5) {
        EftStage_UpdateSprite();
    }
    if (w->flags & EFT_TF_DEAD) {
        BtlTask_SetDead(task);
    }
}

/* Reset callback (battle restart): kills the particles once and the task. */
void EftTransform_Reset(EftAbTask *task) {
    EftTransform *w = task->work;

    if (!(w->flags & EFT_TF_KILLED)) {
        w->flags |= EFT_TF_KILLED;
        EftEmit_KillAll(w->set, w->state);
    }
    BtlTask_SetDead(task);
}

/* Post-update callback: refreshes the "alive" flag and forgets this frame's node bindings. */
void EftTransform_PostUpdate(EftAbTask *task) {
    EftTransform *w = task->work;

    if (!EftEmit_UpdateAlive(w->set, w->state)) {
        w->flags &= ~EFT_TF_ALIVE;
    } else {
        w->flags |= EFT_TF_ALIVE;
    }
    w->groups = 0;
}

/* Draw callback: kind 5 draws the stage sprite at the task's position while it is toggled on. */
void EftTransform_Draw(EftAbTask *task) {
    EftTransform *w = task->work;

    if (w->arg.kind == 5 && (w->flags & EFT_TF_SPRITE)) {
        EftStage_DrawSprite(&w->pos);
    }
}

/* Manager init callback: allocates the module, creates the task list and loads the eight emitter sets. */
void EftTransformMgr_Init(EftAbTask *task) {
    s32 i;

    gEftTransform = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftTransformMgr));
    memset(gEftTransform, 0, sizeof(EftTransformMgr));
    gEftTransformList = BtlTask_CreateChildList(task, 2, sizeof(EftTransform));
    {
        s32 ids[EFT_TF_SETS] = { 0x192, 0x1AA, 0x1C0, 0x1C1, 0x1CB, 0x1E6, 0x1FC, 0x217 };

        for (i = 0; i < EFT_TF_SETS; i++) {
            EftEmit_LoadSet(NULL, &gEftTransform->set[i], 0, NULL, 1, ids[i]);
        }
    }
}

/* Manager term callback: frees the sets and the module. */
void EftTransformMgr_Term(void) {
    s32 i;

    for (i = 0; i < EFT_TF_SETS; i++) {
        EftEmit_FreeSet(&gEftTransform->set[i]);
    }
    BtlPool_Free(BtlPool_GetCurrent(), gEftTransform);
    gEftTransform = NULL;
}

/* Manager update callback: starts a new frame for each set. */
void EftTransformMgr_Update(void) {
    s32 i;

    for (i = 0; i < EFT_TF_SETS; i++) {
        EftEmit_BeginFrame(&gEftTransform->set[i]);
    }
}

/* Fighter effect request 0: starts the effect unless one is running. Returns 1 when it started. */
s32 EftTransform_Start(EftTransformArg *req) {
    EftTransformArg arg = *req;
    EftAbTask *task;

    if (gEftTransform->busy != 0) {
        return 0;
    }
    task = BtlTaskList_AddTail(gEftTransformList, gEftTransformClass, &arg);
    if (task == NULL) {
        return 0;
    }
    gEftTransform->task = task;
    return 1;
}

/* Fighter effect request 1: tells the running effect that the model changes now. */
s32 EftTransform_Flash(void) {
    EftAbTask *task = gEftTransform->task;
    EftTransform *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (gEftTransform->busy == 0) {
        return 0;
    }
    w->flags |= EFT_TF_FLASH;
    return 1;
}

/* Fighter effect request 2: ends the running effect and lets a new one start. */
s32 EftTransform_End(void) {
    EftAbTask *task = gEftTransform->task;
    EftTransform *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (gEftTransform->busy == 0) {
        return 0;
    }
    w->flags |= EFT_TF_END | EFT_TF_DETACHED;
    gEftTransform->task = NULL;
    gEftTransform->busy = 0;
    return 1;
}
