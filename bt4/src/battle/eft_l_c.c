#include "common.h"
#include "battle/eft_l.h"

/*
 * Absorb / drain glow, 0x15E5D0..0x15EF18: manager class 0x2C39E8 (gEftAbsorbMgrClass, an entry of the layer's
 * sub-task table next to the speed lines' 0x2C3A18), task class 0x2C3A00 (gEftAbsorbClass). Started by the fighter effect layer:
 *   request 0x38 (the drain technique, btl_act_h_b.c)  EftAbsorb_Start, then EftAbsorb_Stop when the bit ends:
 *                                                      one emitter set at the fighter's node 0x36;
 *   request 0x37 (a blast was absorbed, btl_char_coll_b.c)  EftAbsorb_StartHands: one set at each hand
 *                                                      (nodes 0x15 and 0x23, aimed away from 0x14 and 0x22),
 *                                                      which ends by itself.
 * Purely visual: it reads node positions and one object attribute and writes only its own work.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 *BtlScene_GetCharPackEntry(s32 side, s32 idx);
extern void *BtlTask_CreateChildList(EftTask *task, s32 count, s32 workSize);
extern EftTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_ObjTestAttr(s32 objId, u64 mask);

extern void EftEmit_LoadSet(void *arg, void *model, s32 a2, s32 *pack, s32 a4, s32 a5);
extern void EftEmit_FreeSet(void *model);
extern void EftEmit_BeginFrame(void *model);
extern s32 EftEmit_GetEndFrames(EftModel *model);
extern void EftEmit_InitState(EftModel *model, EftModelInst *inst);
extern void EftEmit_TermState(EftModel *model, EftModelInst *inst);
extern s32 EftEmit_GetFlagsFromMask(EftModel *model, EftModelInst *inst, s32 objId, s32 part, s32 sub, s32 end, s32 fast, s32 mask);
extern void EftEmit_Spawn(EftModel *model, EftModelInst *inst, void *nodes, Vec4 *pos, Vec4 *dir, s32 objId, s32 node,
                          s32 arg7, s32 part, s32 sub, s32 flags, f32 scale);
extern void EftEmit_KillAll(EftModel *model, EftModelInst *inst);
extern s32 EftEmit_UpdateAlive(EftModel *model, EftModelInst *inst);
extern void EftEmit_SetNode(void *nodes, s32 slot, s32 node, Vec4 *pos);
extern void EftEmit_RefreshFixedNodes(s32 objId, void *nodes);
extern void EftChar_SetList(s32 side, s32 arg1, void *list);
extern void *EftChar_GetList(s32 objId, s32 kind);
extern void EftCharSlot_SetAbsorb(s32 objId, EftTask *task);
extern void EftCharSlot_ClearAbsorb(s32 objId);
extern EftTask *EftCharSlot_GetAbsorb(s32 objId);
extern void BtlTask_SetDead(EftTask *task);
extern void BtlTask_SetOwnerTag(EftTask *task, s32 flag);
extern EftTask *BtlTask_GetParent(EftTask *task);

extern u8 gEftAbsorbClass[0x18]; /* the task class (0x2C3A00): six callbacks, see config/symbols/eft_l.txt */

/* Steps the emitters of one set and spawns their particles at pos, facing dir. */
void EftAbsorb_DrawOne(s32 objId, EftTask *task, EftModel *model, EftModelInst *inst, Vec4 *pos, Vec4 *dir) {
    EftAbsorb *w = task->work;
    s32 part;
    s32 sub;

    for (part = 0; part < 0x13; part++) {
        if (*model->mask & (1 << part)) {
            EftModelPartDef *def = model->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 spawn = EftEmit_GetFlagsFromMask(model, inst, objId, part, sub, w->flags & EFT_ABSORB_END, 0, w->unk848);

                if (spawn != 0) {
                    EftEmit_Spawn(model, inst, w->unk5F0, pos, dir, objId, 0, 3, part, sub, spawn, w->scale);
                }
            }
        }
    }
}

/* Init callback: copies the argument, makes two emitter states and places the glow(s). */
void EftAbsorb_Init(EftTask *task, EftAbsorbArg *arg) {
    EftAbsorbMgr *mgr = BtlTask_GetParent(task)->work;
    EftAbsorb *w = task->work;
    EftAbsorbOwner *owner = &w->owner;
    EftModel *model = mgr->model;
    Vec4 a;
    Vec4 b;

    memset(w, 0, sizeof(EftAbsorb));
    *owner = arg->owner;
    w->hands = arg->hands;
    w->scale = w->owner.scale;
    w->flags |= EFT_ABSORB_ACTIVE;
    w->model = model;
    EftEmit_InitState(model, &w->inst[0]);
    EftEmit_InitState(w->model, &w->inst[1]);
    w->flags |= EFT_ABSORB_NEW;
    w->life = EftEmit_GetEndFrames(w->model);
    BtlTask_SetOwnerTag(task, owner->objId == 0 ? 0x800 : 0x1000);
    if (w->hands == 0) {
        BtlCharApi_GetNodePos(owner->objId, 0x36, &w->pos[0]);
        Vec4_Set(&w->dir[0], 0.0f, -1.0f, 0.0f, 1.0f);
        w->unk848 |= 1;
        EftEmit_SetNode(w->unk5F0, 0, 0x36, NULL);
    } else {
        BtlCharApi_GetNodePos(owner->objId, 0x15, &w->pos[0]);
        BtlCharApi_GetNodePos(owner->objId, 0x23, &w->pos[1]);
        w->unk848 |= 2;
        EftEmit_SetNode(w->unk5F0, 1, 0x36, NULL);
        BtlCharApi_GetNodePos(owner->objId, 0x14, &a);
        BtlCharApi_GetNodePos(owner->objId, 0x22, &b);
        Vec3_Sub(&w->dir[0], &w->pos[0], &a);
        /* The original subtracts from pos[0] here too (EftAbsorb_Update uses pos[1]). */
        Vec3_Sub(&w->dir[1], &w->pos[0], &b);
        Vec3_Normalize(&w->dir[0], &w->dir[0]);
        Vec3_Normalize(&w->dir[1], &w->dir[1]);
        w->dir[0].w = 1.0f;
        w->dir[1].w = 1.0f;
    }
}

/* Term callback: releases both emitter states; the node-0x36 glow also clears the per-fighter slot (0x19BC38). */
void EftAbsorb_Term(EftTask *task) {
    EftAbsorb *w = task->work;
    EftAbsorbOwner *owner = &w->owner;

    EftEmit_TermState(w->model, &w->inst[0]);
    EftEmit_TermState(w->model, &w->inst[1]);
    w->flags = 0;
    if (w->hands == 0) {
        EftCharSlot_ClearAbsorb(owner->objId);
    }
}

/* Update callback: follows the fighter's nodes, spawns the particles, counts down once the end was asked for. */
void EftAbsorb_Update(EftTask *task) {
    EftAbsorb *w = task->work;
    EftAbsorbOwner *owner = &w->owner;
    void *nodes;
    Vec4 a;
    Vec4 b;
    s32 i;

    if (!BtlScene_IsEffectStopped(owner->objId, 3)) {
        if (w->hands == 0) {
            BtlCharApi_GetNodePos(owner->objId, 0x36, &w->pos[0]);
            if (BtlCharApi_ObjTestAttr(owner->objId, 0x400)) {
                w->unk848 |= 1;
                nodes = w->unk5F0;
                EftEmit_SetNode(nodes, 0, 0x36, NULL);
            } else {
                nodes = w->unk5F0;
                BtlCharApi_ObjTestAttr(owner->objId, 0x1000);
            }
        } else {
            BtlCharApi_GetNodePos(owner->objId, 0x15, &w->pos[0]);
            BtlCharApi_GetNodePos(owner->objId, 0x23, &w->pos[1]);
            w->unk848 |= 2;
            nodes = w->unk5F0;
            EftEmit_SetNode(nodes, 1, 0x36, NULL);
            BtlCharApi_GetNodePos(owner->objId, 0x14, &a);
            BtlCharApi_GetNodePos(owner->objId, 0x22, &b);
            Vec3_Sub(&w->dir[0], &w->pos[0], &a);
            Vec3_Sub(&w->dir[1], &w->pos[1], &b);
            Vec3_Normalize(&w->dir[0], &w->dir[0]);
            Vec3_Normalize(&w->dir[1], &w->dir[1]);
            w->dir[0].w = 1.0f;
            w->dir[1].w = 1.0f;
        }
        EftEmit_RefreshFixedNodes(owner->objId, nodes);
        if (w->hands == 0) {
            EftAbsorb_DrawOne(owner->objId, task, w->model, &w->inst[0], &w->pos[0], &w->dir[0]);
        } else {
            for (i = 0; i < 2; i++) {
                EftAbsorb_DrawOne(owner->objId, task, w->model, &w->inst[i], &w->pos[i], &w->dir[i]);
            }
        }
        if (w->flags & EFT_ABSORB_END) {
            w->timer += 1.0f;
            if (w->timer >= w->life) {
                w->flags |= EFT_ABSORB_KILL;
            }
        }
    }
    if (w->flags & EFT_ABSORB_KILL) {
        BtlTask_SetDead(task);
    }
}

/* Reset callback: destroys the particles once and kills the task. */
void EftAbsorb_Reset(EftTask *task) {
    EftAbsorb *w = task->work;

    if (!(w->flags & EFT_ABSORB_RESET)) {
        w->flags |= EFT_ABSORB_RESET;
        EftEmit_KillAll(w->model, &w->inst[0]);
        EftEmit_KillAll(w->model, &w->inst[1]);
    }
    BtlTask_SetDead(task);
}

/* Post-update callback (from the second frame on): once no particle of the first set is alive, the hands variant
 * starts its end by itself; the other one waits for EftAbsorb_Stop. */
void EftAbsorb_PostUpdate(EftTask *task) {
    EftAbsorb *w = task->work;

    if (w->flags & EFT_ABSORB_POSTED) {
        if (!EftEmit_UpdateAlive(w->model, &w->inst[0])) {
            if (w->hands == 0) {
                w->flags &= ~EFT_ABSORB_NEW;
            } else {
                w->flags |= EFT_ABSORB_END;
            }
        } else {
            w->flags |= EFT_ABSORB_NEW;
        }
        w->unk848 = 0;
    }
    w->flags |= EFT_ABSORB_POSTED;
}

/* Draw callback: nothing. */
void EftAbsorb_Nop(void) {
}

/* Manager init: loads the emitter set from entry 0xB of the character's pack, makes room for six glows and
 * registers the list (0x172298). */
void EftAbsorbMgr_Init(EftTask *task, s32 *arg) {
    EftAbsorbMgr *mgr = task->work;
    EftAbsorbModel *model;
    s32 *pack;
    void *list;

    mgr->model = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftAbsorbModel));
    memset(mgr->model, 0, sizeof(EftAbsorbModel));
    model = (EftAbsorbModel *)mgr->model;
    pack = BtlScene_GetCharPackEntry(arg[0], 0xB);
    model->pack = pack;
    if (pack != NULL) {
        EftEmit_LoadSet(NULL, model, 0, pack, 2, 1);
    }
    list = BtlTask_CreateChildList(task, 6, sizeof(EftAbsorb));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term: releases the set and its memory. */
void EftAbsorbMgr_Term(EftTask *task) {
    EftAbsorbMgr *mgr = task->work;

    if (((EftAbsorbModel *)mgr->model)->pack != NULL) {
        EftEmit_FreeSet(mgr->model);
    }
    if (mgr->model != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->model);
    }
}

/* Manager update: steps the set. */
void EftAbsorbMgr_Update(EftTask *task) {
    EftAbsorbMgr *mgr = task->work;

    if (((EftAbsorbModel *)mgr->model)->pack != NULL) {
        EftEmit_BeginFrame(mgr->model);
    }
}

/* Fighter effect request 0x38 began: starts the node-0x36 glow unless the fighter already has one. Returns 1
 * when a task was created. */
s32 EftAbsorb_Start(EftAbsorbOwner *arg) {
    EftAbsorbArg init;
    void *list;
    EftTask *task;

    init.owner = *arg;
    init.hands = 0;
    if (EftCharSlot_GetAbsorb(init.owner.objId) != NULL) {
        return 0;
    }
    list = EftChar_GetList(init.owner.objId, 9);
    if (list == NULL) {
        return 0;
    }
    task = BtlTaskList_AddTail(list, gEftAbsorbClass, &init);
    if (task == NULL) {
        return 0;
    }
    EftCharSlot_SetAbsorb(init.owner.objId, task);
    return 1;
}

/* Fighter effect request 0x38 ended: asks the fighter's glow to end. Returns 1 when it was running. */
s32 EftAbsorb_Stop(s32 objId) {
    EftTask *task = EftCharSlot_GetAbsorb(objId);

    EftAbsorb *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_ABSORB_ACTIVE)) {
        return 0;
    }
    w->flags |= EFT_ABSORB_END;
    return 1;
}

/* Fighter effect request 0x37: starts the two-hand glow. */
void EftAbsorb_StartHands(EftAbsorbOwner *arg) {
    EftAbsorbArg init;
    void *list;

    init.owner = *arg;
    init.hands = 1;
    list = EftChar_GetList(init.owner.objId, 9);
    if (list != NULL) {
        BtlTaskList_AddTail(list, gEftAbsorbClass, &init);
    }
}
