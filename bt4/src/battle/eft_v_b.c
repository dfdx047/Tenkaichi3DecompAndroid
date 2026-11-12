#include "common.h"
#include "battle/eft_v_ext.h"

/*
 * Effect code 0x1871A8..0x187C50: the impact effect (see include/battle/eft_v.h): hit sparks started by the
 * fighter effect layer and the explosion a projectile leaves where it hits.
 *
 * Drawing only: a task that runs one emitter set of the common effect pack at a point. It creates no hit record,
 * calls no fighter, stage or camera function, raises no battle event and draws no random number. Read from the
 * simulation: BtlScene_IsEffectStopped, BtlScene_IsTimeStopped, BtlScene_GetCharScale.
 */

/* Runs every part of the impact's emitter set for this frame: start, move, stop or kill, as the library decides. */
void EftImpact_SpawnParts(s32 objId, EftVTask *task, EftVSet *set) {
    EftImpactWork *w = task->work;
    s32 type;
    s32 i;

    for (type = 0; type < 19; type++) {
        if (*set->mask & (1 << type)) {
            EftVGroupDef *g = set->grp[type].def;

            for (i = 0; i < g->count; i++) {
                s32 flags = EftEmit_GetFlagsFromMask(set, &w->state, objId, type, i, w->flags & EFT_IMPACT_ENDING,
                                                     w->flags & EFT_IMPACT_KILL, w->mask);

                if (w->flags & EFT_IMPACT_KILLED) {
                    flags = 2;
                }
                if (flags) {
                    EftEmit_Spawn(set, &w->state, &w->nodes, &w->pos, &w->dir, objId, 0, 2, type, i, flags, w->scale);
                }
            }
        }
    }
}

/* The emitter set of an impact id: six hit spark sets, eight blast impact sets. */
EftVSet *EftImpact_GetSet(s32 group, u32 id) {
    if (group == 0) {
        switch (id) {
        case 4:
        case 5:
        case 6:
            return &gEftImpact->hit[1];
        case 7:
        case 8:
        case 9:
        case 10:
            return &gEftImpact->hit[2];
        case 11:
        case 12:
        case 13:
            return &gEftImpact->hit[3];
        case 14:
            return &gEftImpact->hit[4];
        case 15:
        case 16:
            return &gEftImpact->hit[5];
        case 0:
        case 1:
        case 2:
        case 3:
        default:
            return &gEftImpact->hit[0];
        }
    } else {
        switch (id) {
        case 0:
            return &gEftImpact->blast[0];
        case 1:
            return &gEftImpact->blast[1];
        case 2:
            return &gEftImpact->blast[2];
        case 3:
        case 4:
            return &gEftImpact->blast[3];
        case 5:
        case 6:
            return &gEftImpact->blast[4];
        case 7:
        case 8:
            return &gEftImpact->blast[5];
        case 9:
        case 10:
            return &gEftImpact->blast[6];
        case 11:
        case 12:
            return &gEftImpact->blast[7];
        default:
            return &gEftImpact->blast[0];
        }
    }
}

/* The node slots (bit n = slot n) an impact id places at its position. */
u8 EftImpact_GetNodeMask(s32 group, s32 id) {
    if (group == 0) {
        u8 hit[17] = { 1, 1, 8, 0x10, 1, 2, 8, 1, 2, 8, 0x10, 1, 2, 8, 1, 1, 2 };

        return hit[id];
    } else {
        u8 blast[13] = { 1, 1, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 };

        return blast[id];
    }
}

/* Task init: position, direction, scale, emitter set and node slots of the impact. */
void EftImpact_Init(EftVTask *task, EftImpactInit *init) {
    EftImpactWork *w = task->work;
    EftImpactArg *arg = &init->arg;
    EftVVec *pos = &w->pos;
    s32 i;

    memset(w, 0, sizeof(EftImpactWork));
    w->group = init->group;
    w->id = arg->id;
    w->objId = arg->objId;
    Vec4_Copy(pos, &arg->pos);
    Vec4_Copy(&w->dir, &init->arg.dir);
    w->dir.w = 1.0f;
    Vec3_Normalize(&w->dir, &w->dir);
    if (w->group == 0) {
        if (init->scale <= 0.0f) {
            u32 chr = w->objId;

            if (chr >= 2) {
                chr = 0;
            }
            w->scale = BtlScene_GetCharScale(chr);
            if (w->scale < 0.7f) {
                w->scale = 0.7f;
            }
            w->scale *= 0.75f;
            if (w->id == 1) {
                w->scale *= 1.2f;
            }
        } else {
            w->scale = init->scale;
        }
    } else {
        w->scale = init->scale;
        w->rise = init->rise;
    }
    w->set = EftImpact_GetSet(init->group, w->id);
    EftEmit_InitState(w->set, &w->state);
    w->endFrames = EftEmit_GetEndFrames(w->set);
    w->mask = EftImpact_GetNodeMask(w->group, w->id);
    for (i = 0; i < 6; i++) {
        if ((w->mask >> i) & 1) {
            EftEmit_SetNode(&w->nodes, i, -1, pos);
        }
    }
    gEftImpact->count[w->group]++;
}

/* Task term. */
void EftImpact_Term(EftVTask *task) {
    EftImpactWork *w = task->work;

    EftEmit_TermState(w->set, &w->state);
    gEftImpact->count[w->group]--;
}

/* Task update: moves a blast impact up, runs the set's parts, and ends the task after the set's end frames. */
void EftImpact_Update(EftVTask *task) {
    EftImpactWork *w = task->work;
    s32 i;

    if (BtlScene_IsEffectStopped(w->objId, 2)) {
        return;
    }
    if (w->group != 0) {
        w->pos.y -= w->rise;
    }
    for (i = 0; i < 6; i++) {
        if ((w->mask >> i) & 1) {
            EftEmit_SetNodePos(&w->nodes, i, &w->pos);
        }
    }
    EftImpact_SpawnParts(w->objId, task, w->set);
    if (w->flags & EFT_IMPACT_ENDING) {
        w->time += 1.0f;
    }
    if (w->flags & EFT_IMPACT_DEAD) {
        BtlTask_SetDead(task);
    } else if (w->flags & EFT_IMPACT_ENDING) {
        if ((w->flags & EFT_IMPACT_KILL) || w->time >= w->endFrames) {
            w->flags |= EFT_IMPACT_DEAD;
        }
    }
}

/* Task post-update: from the second frame on, marks the impact as ending once none of its parts is alive. */
void EftImpact_PostUpdate(EftVTask *task) {
    EftImpactWork *w = task->work;

    if (BtlScene_IsTimeStopped()) {
        return;
    }
    if (w->flags & EFT_IMPACT_STARTED) {
        if (!EftEmit_UpdateAlive(w->set, &w->state)) {
            w->flags |= EFT_IMPACT_ENDING;
        }
        w->mask = 0;
    }
    w->flags |= EFT_IMPACT_STARTED;
}

/* Task reset: kills the parts and ends the task. */
void EftImpact_Reset(EftVTask *task) {
    EftImpactWork *w = task->work;

    if (!(w->flags & EFT_IMPACT_KILLED)) {
        w->flags |= EFT_IMPACT_KILLED;
        EftEmit_KillAll(w->set, &w->state);
    }
    BtlTask_SetDead(task);
}

/* Task draw: nothing (the parts draw themselves). */
void EftImpact_Draw(EftVTask *task) {
}

/* Manager init: loads the fourteen emitter sets from the common effect pack; list of 30 tasks. */
void EftImpactMgr_Init(EftVTask *task) {
    s32 i;

    gEftImpact = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftImpactMgr));
    memset(gEftImpact, 0, sizeof(EftImpactMgr));
    gEftImpactList = BtlTask_CreateChildList(task, 30, sizeof(EftImpactWork));
    {
        s32 hit[EFT_IMPACT_HIT_SETS] = { 0xC, 0x20, 0x25, 0x4C, 0x66, 0x73 };

        for (i = 0; i < EFT_IMPACT_HIT_SETS; i++) {
            EftEmit_LoadSet(NULL, &gEftImpact->hit[i], NULL, NULL, 1, hit[i]);
        }
    }
    {
        s32 blast[EFT_IMPACT_BLAST_SETS] = { 0x88, 0x91, 0xA4, 0xB9, 0xF2, 0x11A, 0x137, 0x158 };

        for (i = 0; i < EFT_IMPACT_BLAST_SETS; i++) {
            EftEmit_LoadSet(NULL, &gEftImpact->blast[i], NULL, NULL, 1, blast[i]);
        }
    }
}

/* Manager term. */
void EftImpactMgr_Term(EftVTask *task) {
    s32 i;

    for (i = 0; i < EFT_IMPACT_HIT_SETS; i++) {
        EftEmit_FreeSet(&gEftImpact->hit[i]);
    }
    for (i = 0; i < EFT_IMPACT_BLAST_SETS; i++) {
        EftEmit_FreeSet(&gEftImpact->blast[i]);
    }
    BtlPool_Free(BtlPool_GetCurrent(), gEftImpact);
    gEftImpact = NULL;
}

/* Manager update: starts the frame of every set. */
void EftImpactMgr_Update(EftVTask *task) {
    s32 i;

    for (i = 0; i < EFT_IMPACT_HIT_SETS; i++) {
        EftEmit_BeginFrame(&gEftImpact->hit[i]);
    }
    for (i = 0; i < EFT_IMPACT_BLAST_SETS; i++) {
        EftEmit_BeginFrame(&gEftImpact->blast[i]);
    }
}

/* Manager reset: nothing. */
void EftImpactMgr_Reset(EftVTask *task) {
}

/* Starts a hit spark (group 0) sized by the fighter's scale. Returns 0 when the list is full. */
s32 EftImpact_SpawnHit(EftImpactArg *arg) {
    EftImpactInit init;

    init.arg = *arg;
    init.group = 0;
    init.scale = 0.0f;
    init.rise = 0.0f;
    return BtlTaskList_AddTail(gEftImpactList, gEftImpactClass, &init) != NULL;
}

/* Starts a hit spark (group 0) with a given scale. */
s32 EftImpact_SpawnHitScaled(EftImpactArg *arg, f32 scale) {
    EftImpactInit init;

    init.scale = scale;
    init.arg = *arg;
    init.group = 0;
    init.rise = 0.0f;
    return BtlTaskList_AddTail(gEftImpactList, gEftImpactClass, &init) != NULL;
}

/* Starts a blast impact (group 1): what a projectile leaves where it hits. */
s32 EftImpact_SpawnBlast(EftImpactArg *arg, f32 scale, f32 rise) {
    EftImpactInit init;

    init.group = 1;
    init.scale = scale;
    init.rise = rise;
    init.arg = *arg;
    return BtlTaskList_AddTail(gEftImpactList, gEftImpactClass, &init) != NULL;
}
