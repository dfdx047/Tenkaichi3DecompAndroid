#include "common.h"
#include "battle/eft_h.h"

/*
 * Technique effects, second part: 0x14B108..0x14F230. See include/battle/eft_h.h for the layouts.
 * Everything called outside the file is declared here with this file's own view types.
 *
 * Two functions (EftShot_BuildParam, EftEmit_SpawnType0) are INCLUDE_ASM with the C attempt in `#if 0` above them and a note on what differs; turning
 * every `#if 0` into `#if 1` and dropping the INCLUDE_ASM lines gives a file that fdiff can compile.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);          /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);          /* rotate about Y */
extern void Vec3_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi);       /* clamp x, y, z */
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, Vec4 *pos, f32 scale); /* dst = pos + dir * scale */
extern f32 EftMath_WrapAngle(f32 angle);                                   /* wrap to -pi..pi */
extern void EftAim_GetDir(Vec4 *out, Vec4 *target, s32 objId);
extern void EftAim_GetDirKeep(EftHSlot *slot, Vec4 *out, Vec4 *target, s32 objId);

extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlPool_GetCurrent(void);

extern void BtlScene_Reset(s32 mode);
extern s32 BtlScene_IsCharStopped(s32 objId);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);

extern void *BtlTask_CreateChildList(EftHTask *task, s32 count, s32 dataSize);
extern EftHTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_Kill(EftHTask *task);
extern void BtlTask_SetDead(EftHTask *task);          /* task->kill |= 1 */
extern void BtlTask_SetOwnerTag(void *task, s32 flags);   /* task->flags |= flags */
extern EftHTask *BtlTask_GetParent(EftHTask *task);     /* the task that owns the list this task is in */

extern s32 BtlCharApi_GetPlayerObjUnk9C(s32 player, u32 n);
extern void *BtlCharApi_GetPlayerSuperData(s32 player);
extern void *BtlCharApi_GetPlayerSkillData(s32 player);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_ObjQuery24D610(s32 objId, s32 arg1, s32 arg2);

/* eft_g.c */
extern s32 EftShot_TestBits(s32 objId, s32 mask);
extern void EftShot_SetHeldFlagA8(s32 objId);
extern void EftShot_Nop(s32 size);
extern void EftShot_SetCurSlot(s32 chr, s32 slot);
extern void EftShot_SetCharPool(s32 chr);

/* eft_i.c */
extern void EftEmit_SpawnOwn(EftSet *set, EftSetState *st, EftVolleyPose *pose, Vec4 *a, Vec4 *dir, s32 type, s32 idx,
                             s32 flags, f32 scale);
extern void EftEmit_KillAll(EftSet *set, EftSetState *st);
extern s32 EftEmit_UpdateAlive(EftSet *set, EftSetState *st);
extern void EftEmit_UpdateNodesReq(EftHSlot *slot, EftVolleyPose *pose);
extern void EftEmit_UpdateTrailWidth(EftSet *set, EftSetState *st);

/* 0x158438.. (effect requests of a fighter, state gp 0x2FE9FC) */
extern void EftTechEvt_Start(EftHSlot *slot, s32 lead, s32 life);
extern void EftTechEvt_RequestRestart(s32 objId);

/* 0x16A400 module: the shot tasks */
extern void *EftBlastObj_Create(EftVolleyShotArg *arg);
extern void *EftBlastObj_CreateWithModel(EftVolleyShotArg *arg, s32 *extra);
extern void EftBlastObj_Stop(void *shot);
extern s32 EftBlastObj_IsAlive(void *shot);
extern void EftBlastObj_SetSel(void *shot, s32 a, s32 b, s32 c);
extern void EftBlastObj_SetTarget(void *shot, s32 a, EftVolleyShot *pos);
extern void EftBlastObj_SetFrozen(void *shot, s32 a);
extern void EftBlastObj_SetNoHit(void *shot, s32 a);
extern void EftBlastObj_SetModelAnim(void *shot, s32 a);
extern void EftBlastObj_MarkLast(void *shot);

extern void EftTexSet_Load32(u8 *res, s32 *entry);
extern void EftTexSet_Load4(u8 *res, s32 *entry);
extern void EftTexSet_Load8(u8 *res, s32 *entry);
extern void EftTexSet_Load16(u8 *res, s32 *entry);
extern s32 EftVolleyAim_GetNodeSide(s32 node);
extern void EftVolleyAim_InitShot(s32 objId, EftVolleyShot *shot, s32 aimKind, s32 arg3, s32 index, s32 count, f32 a, f32 b);
extern void EftVolleyAim_Spread(s32 objId, Vec4 *dir, s32 a, s32 b, s32 c, s32 index, s32 count);
extern void EftVolleyAim_Update(s32 objId, EftVolleyShot *shot, Vec4 *out, s32 flag, f32 time, f32 a, f32 b);

extern EftHMgr *gEftShot;
extern EftHTaskClass gEftShotCharClass;
extern EftShotClassRow gEftShotClass[11];

EftHSlot *EftShot_GetSlot(s32 chr, s32 slot);
f32 EftShot_GetLeadTime(EftHSlot *slot);
EftVolleyShot *EftVolley_AllocShot(EftVolleyWork *w);
void EftVolley_Fire(s32 objId, EftHTask *task, s32 phase, s32 sub);
s32 EftVolley_PruneShots(EftHTask *task);
s32 EftVolley_UpdateShots(s32 objId, EftHTask *task);
void EftVolley_Aim(s32 objId, EftHTask *task, Vec4 *target, s32 bySlot);
void EftVolley_UpdateParts(s32 objId, EftHTask *task, EftSet *set, s32 reset);
s32 EftEmit_TypeToResKind(s32 type);
void EftEmit_LoadSet(void *owner, EftSet *set, EftSetHead *head, s32 *base, s32 common, s32 idx);
void EftEmit_FreeSet(EftSet *set);
void EftEmit_BeginFrame(EftSet *set);
s32 EftEmit_GetAimKind(EftSet *set);
s32 EftEmit_GetEndFrames(EftSet *set);
s32 EftEmit_GetPhaseMask(EftSet *set);
s32 EftEmit_IsPartDeferred(EftSet *set, s32 type, s32 idx);
void EftEmit_InitState(EftSet *set, EftSetState *st);
void EftEmit_TermState(EftSet *set, EftSetState *st);
s32 EftEmit_GetFlagsFromReq(EftSet *set, EftSetState *st, s32 objId, s32 type, s32 idx, s32 ending, s32 kill);
s32 EftEmit_GetFlags(EftSet *set, EftSetState *st, s32 objId, s32 type, s32 idx, s32 ending, s32 kill, s32 req);
s32 EftEmit_GetResetFlags(EftSet *set, EftSetState *st, s32 type, s32 idx);
void EftEmit_TagTask(void *task, s32 chr, s32 type);

/* Row of gEftShotClass for an effect type. */
static inline s32 EftShot_TypeRow(s32 type) {
    s32 row = type + 1;

    if (type == -1) {
        row = 0;
    }
    return row;
}
#define TYPE_ROW(t) EftShot_TypeRow(t)

/* Fills the parameter block of technique slot `slot` of character `chr`: defaults when `blank`, otherwise a
   flat copy of the fields the effects use out of the character's skill (slots 0, 1) or technique data. */
#if 0 /* not matched (499 instructions against 502). The defaults branch is identical except for the order of six stores; the skill and technique branches read the same fields into the same places, but the original keeps a copy of the slot index (move t8,t9 / addiu t8,t9,-2) and shares the address arithmetic of the structure-of-arrays fields differently (it spills 34 intermediate addresses to the stack, this C 30). Behaviour verified: both versions were run in an interpreter for every slot 0..4, blank 0 / 1 and 40 random source tables; the parameter block and the memory around it come out byte-identical (build/scratch_cleanup_eft/emu_buildparam.py; a deliberately wrong field is caught). Cleanup X2: making the slot index ONE function-scope variable set in both branches (`n = slot;` / `n = slot - 2;`, in front of or behind the data call), or reusing `i` for it, does not give the original's `move t8,t9` and is worse (355 to 384 instructions differ after alignment, against 317 for this form). Not tried: the element-alignment lesson (the original computes `d + n` once per access and spills each copy, the pattern that f32 [16][4] solved in EftOrbTail_DrawStreaks); the views of EftSkillSrc / EftSuperSrc are the place to look. */
void EftShot_BuildParam(s32 chr, s32 slot, EftShotParam *p, s32 blank) {
    s32 i;

    memset(p, 0, sizeof(EftShotParam));
    if (blank) {
        p->id = 0;
        p->unk2 = 100;
        p->kind = 1;
        p->unk5 = 0;
        p->unk6 = 0;
        p->unk7 = 0;
        p->unk8 = 0;
        p->shots = 1;
        p->unkA = 0;
        p->unkB = 0;
        p->unkC = 0;
        p->type = 0;
        p->unkE[0] = 3;
        p->unkE[1] = 3;
        p->unkE[2] = 3;
        p->unkE[3] = 3;
        p->unkE[4] = 3;
        p->unkE[5] = 3;
        p->unk16[0] = 0;
        p->unk16[1] = 10;
        p->unk16[2] = 20;
        p->unk16[3] = -1;
        p->unk16[4] = -1;
        p->unk16[5] = -1;
        p->unk22 = 0;
        p->unk24 = 0;
        p->life = 120;
        p->unk2C = 0.0f;
        p->unk30 = 1.0f;
        p->unk34 = 40.0f;
        p->unk38 = 0.0f;
        p->flags = 0;
        p->unk40 = 0;
        p->unk42 = 0;
        p->volley = 1;
        p->unk50 = 1.0f;
        p->unk14 = -1;
        p->unk48 = 0;
        p->unk4A = 0;
        p->unk4C = 0.0f;
        p->unk54 = 1.0f;
        for (i = 0; i < 8; i++) {
            p->unk58[i] = -1;
            p->unk60[i] = 0.0f;
        }
        p->unk84 = 0.1f;
        p->unk88 = 1.0f;
        p->unk80 = 0;
    } else if (slot < 2) {
        EftSkillSrc *d = BtlCharApi_GetPlayerSkillData(chr);
        s32 n = slot;

        p->id = d->id[n];
        p->unk2 = d->unk14[n];
        p->kind = 0;
        p->unk5 = d->unk62[n];
        p->unk6 = d->unk38[n];
        p->unk7 = 0;
        p->unk8 = d->unk3E[n];
        p->shots = d->unk3A[n];
        p->unkA = d->unk40[n];
        p->unkB = d->unk42[n];
        p->unkC = 0;
        p->type = d->type[n];
        p->unkE[0] = d->unk48[0][n];
        p->unkE[1] = d->unk48[1][n];
        p->unkE[2] = d->unk48[2][n];
        p->unkE[3] = d->unk48[3][n];
        p->unkE[4] = d->unk48[4][n];
        p->unkE[5] = d->unk48[5][n];
        p->unk16[0] = (s8)d->unk54[0][n];
        p->unk16[1] = (s8)d->unk54[1][n];
        p->unk16[2] = (s8)d->unk54[2][n];
        p->unk16[3] = (s8)d->unk54[3][n];
        p->unk16[4] = (s8)d->unk54[4][n];
        p->unk16[5] = (s8)d->unk54[5][n];
        p->unk22 = 0;
        p->unk24 = 0;
        p->life = d->time[n] * 30.0f;
        p->unk2C = 0.0f;
        p->unk30 = d->unk30[n];
        p->unk34 = d->unk20[n];
        p->unk38 = d->unk28[n];
        p->flags = d->flags[n];
        p->unk40 = (s8)d->unk46[n];
        p->unk42 = (s8)d->unk44[n];
        p->volley = (s8)d->unk3C[n];
        p->unk4C = 0.0f;
        p->unk48 = (s8)d->unk6E[n];
        p->unk4A = (s8)d->unk70[n];
        p->unk50 = 1.0f;
        p->unk14 = -1;
        p->unk54 = 1.0f;
        for (i = 0; i < 8; i++) {
            p->unk58[i] = -1;
            p->unk60[i] = 0.0f;
        }
        p->unk84 = 0.1f;
        p->unk88 = 1.0f;
        p->unk80 = 0;
    } else {
        EftSuperSrc *d = BtlCharApi_GetPlayerSuperData(chr);
        s32 n = slot - 2;

        p->id = d->id[n];
        p->unk2 = d->unk1E[n];
        p->kind = slot == 4 ? 2 : 1;
        p->unk5 = d->unk13B[n];
        p->unk6 = d->unk90[n];
        p->unk7 = 0;
        p->unk8 = d->unk99[n];
        p->shots = d->unk93[n];
        p->unkA = d->unk9C[n];
        p->unkB = d->unk9F[n];
        p->unkC = 0;
        p->type = d->type[n];
        p->unkE[0] = d->unkA8[0][n];
        p->unkE[1] = d->unkA8[1][n];
        p->unkE[2] = d->unkA8[2][n];
        p->unkE[3] = d->unkA8[3][n];
        p->unkE[4] = d->unkA8[4][n];
        p->unkE[5] = d->unkA8[5][n];
        p->unk16[0] = -1;
        p->unk16[5] = -1;
        p->unk16[1] = -1;
        p->unk16[2] = -1;
        p->unk16[3] = -1;
        p->unk16[4] = -1;
        p->unk22 = 0;
        p->unk24 = 0;
        p->life = d->time[n] * 30.0f;
        p->unk2C = d->unk30[n];
        p->unk30 = d->unk6C[n];
        p->unk34 = d->unk54[n];
        p->unk38 = d->unk60[n];
        p->flags = d->flags[n];
        p->unk40 = (s8)d->unkA5[n];
        p->unk42 = (s8)d->unkA2[n];
        p->volley = (s8)d->unk96[n];
        p->unk4C = d->unk214[n];
        p->unk48 = (s8)d->unk15C[n];
        p->unk4A = (s8)d->unk15F[n];
        p->unk54 = d->unk78[n];
        p->unk50 = d->unk84[n];
        p->unk14 = d->unk162[n];
        for (i = 0; i < 8; i++) {
            p->unk58[i] = d->unkBA[i][n];
            p->unk60[i] = d->unkD4[i][n];
        }
        p->unk80 = (s8)d->unk134[n];
        p->unk84 = d->unk48[n];
        p->unk88 = d->unk3C[n];
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_h", EftShot_BuildParam);

/* The slot record of one technique of one character. */
EftHSlot *EftShot_GetSlot(s32 chr, s32 slot) {
    EftHChar *c = &gEftShot->chars[chr];

    return &c->slot[slot];
}

/* Adds the group task of a slot (the class its effect type names) to the character's task list. */
void EftShot_CreateSlotTask(s32 chr, s32 slot) {
    EftHChar *c = &gEftShot->chars[chr];
    EftHSlot *s = &c->slot[slot];
    EftHTaskClass *cls = gEftShotClass[TYPE_ROW(s->param->type)].group;

    if (cls != NULL) {
        s->chr = chr;
        s->task = BtlTaskList_AddTail(c->list, cls, s);
    }
}

/* Starts the effect of a technique: stores the request in the slot, overrides the life and shot parameters
   with the request's and adds an instance task under the slot's group task. */
void EftShot_Start(EftHStartArg *arg) {
    EftHSlot *s = EftShot_GetSlot(arg->chr, arg->slot);
    EftHTaskClass *cls;
    EftHTask *group;

    if (s == NULL) {
        return;
    }
    if (s->pack == NULL) {
        return;
    }
    cls = gEftShotClass[TYPE_ROW(s->param->type)].inst;
    if (cls == NULL) {
        return;
    }
    group = s->task;
    s->arg = *arg;
    s->unk40 = 0;
    s->param->life = arg->time * 30.0f;
    s->param->unk34 = arg->unk14;
    s->param->unk38 = arg->unk18;
    if (s->param->kind == 0) {
        s->param->life = 60;
    }
    EftShot_SetCurSlot(arg->chr, arg->slot);
    EftTechEvt_Start(s, EftShot_GetLeadTime(s), s->param->life);
    BtlTaskList_AddTail(group->children, cls, s);
}

/* Kills the character task of the layer, after resetting the scene tasks that belong to the character. */
void EftShot_DestroyChar(s32 chr) {
    EftHChar *c = &gEftShot->chars[chr];

    if (c->task != NULL) {
        BtlScene_Reset(chr == 0 ? 2 : 3);
        EftShot_SetCharPool(chr);
        BtlTask_Kill(c->task);
        c->task = NULL;
    }
}

/* Creates the character task of the layer unless it exists. */
void EftShot_CreateChar(s32 chr) {
    EftHChar *c = &gEftShot->chars[chr];
    s32 arg;

    if (c->task == NULL) {
        arg = chr;
        c->task = BtlTaskList_AddTail(gEftShot->list, &gEftShotCharClass, &arg);
    }
}

/* Frames between the start of a technique's effect and its first shot, by kind of technique: 0.85 s; a technique
   (kind 1) with unk5 == 5: 0.8 s; an ultimate (kind 2): 0.8 s with unk5 == 5, else 1.3 s, 0.7 s for technique
   0x268 and none for 0x290. (Two `return t;` statements: with a single return at the end the value sits in $f0
   from the start; the early return keeps it in $f1 and copies it at the exit.) */
f32 EftShot_GetLeadTime(EftHSlot *slot) {
    f32 t = 0.85f * 30.0f;
    EftShotParam *p = slot->param;

    if (p == NULL) {
        return t;
    }
        switch (p->kind) {
        case 0:
            break;
        case 1:
            if (p->unk5 == 5) {
                t = 0.8f * 30.0f;
            }
            break;
        case 2:
            t = 0.8f * 30.0f;
            if (p->unk5 != 5) {
                t = 1.3f * 30.0f;
            }
            if (p->id == 0x268) {
                t = 0.7f * 30.0f;
            }
            if (p->id == 0x290) {
                t = 0.0f;
            }
            break;
        }
    return t;
}

/* The effect pack of a technique slot, from the fighter object. */
s32 EftShot_GetCharPack(s32 chr, s32 slot) {
    return BtlCharApi_GetPlayerObjUnk9C(chr, slot);
}

/* ---- effect type -1: nothing ------------------------------------------------------------------------------ */

/* Group task init: a list for two instances of 8 bytes. */
void EftShotNullGroup_Init(EftHTask *task) {
    BtlTask_CreateChildList(task, 2, sizeof(EftShotNullWork));
}

/* Group task term: nothing. */
void EftShotNullGroup_Term(EftHTask *task) {
}

/* Group task update: nothing. */
void EftShotNullGroup_Update(EftHTask *task) {
}

/* Instance init: remembers the slot. */
void EftShotNull_Init(EftHTask *task, EftHSlot *slot) {
    EftShotNullWork *w = task->data;

    memset(w, 0, sizeof(EftShotNullWork));
    w->slot = slot;
}

/* Instance term: nothing. */
void EftShotNull_Term(EftHTask *task) {
}

/* Instance reset: kills the instance. */
void EftShotNull_Reset(EftHTask *task) {
    BtlTask_SetDead(task);
}

/* Instance update: kills the instance at once, whatever the slot holds. */
void EftShotNull_Update(EftHTask *task) {
    EftHSlot *slot = ((EftShotNullWork *)task->data)->slot;

    if (slot == NULL) {
        BtlTask_SetDead(task);
        return;
    }
    if (slot->param == NULL) {
        BtlTask_SetDead(task);
        return;
    }
    BtlTask_SetDead(task);
}

/* Instance post-update: nothing. */
void EftShotNull_PostUpdate(EftHTask *task) {
}

/* Instance draw: nothing. */
void EftShotNull_Draw(EftHTask *task) {
}

/* ---- effect type 1: volley of shots ------------------------------------------------------------------------ */

/* Returns a zeroed free shot entry (the last free one of the 30), NULL when all are in use. */
EftVolleyShot *EftVolley_AllocShot(EftVolleyWork *w) {
    EftVolleyShot *shot = w->shot;
    EftVolleyShot *found = NULL;
    s32 i;

    for (i = 0; i < 30; i++, shot++) {
        if (shot->handle == NULL) {
            found = shot;
            memset(found, 0, sizeof(EftVolleyShot));
        }
    }
    return found;
}

/* Fires: creates one shot task (EftShotParam.volley of them in phase 1) at the fighter's firing node. */
void EftVolley_Fire(s32 objId, EftHTask *task, s32 phase, s32 sub) {
    EftVolleyWork *w = task->data;
    EftHSlot *slot = w->slot;
    s32 n = 1;
    s32 i;

    if (phase == 1) {
        n = slot->param->volley;
        if (n <= 0) {
            n = 1;
        }
    }
    for (i = 0; i < n; i++) {
        EftVolleyShot *shot = EftVolley_AllocShot(w);

        if (shot != NULL) {
            s32 count = slot->param->shots;
            EftVolleyShotArg arg = { slot,       w->pack, &w->pose, shot,     &shot->dir, w->fired,
                                     count,      phase,   sub,      0.0f,     w->unk10,   w->unk4 };
            s32 node;
            EftVolleyPose *sp;

            if (w->flags & EFT_VOLLEY_ID1C6) {
                shot->flags |= 2;
                node = i == 0 ? 0x1F : 0x2D;
                sp = &w->shotPose[i];
                *sp = w->pose;
                w->shotPose[i].node = node;
                BtlCharApi_GetNodePos(objId, node, &w->shotPose[i].nodePos);
                arg.pose = &w->shotPose[i];
            } else {
                if (BtlCharApi_ObjQuery24D610(objId, 0x400, 4) <= 0) {
                    shot->flags |= 2;
                }
                node = w->pose.node;
            }
            if (w->flags & EFT_VOLLEY_1000) {
                BtlCharApi_GetNodePos(objId, 0x36, &shot->pos);
                Vec4_Copy(&shot->dir, &w->aim);
                EftVolleyAim_Spread(objId, &shot->dir, 0xB, 1, 0, w->fired, count);
            } else {
                BtlCharApi_GetNodePos(objId, node, &shot->pos);
                Vec4_Copy(&shot->dir, &w->aim);
            }
            if (phase == 1) {
                arg.time = (f32)slot->param->life / 30.0f;
                if (!(w->flags & EFT_VOLLEY_ID1C6)) {
                    if (!(shot->flags & 4)) {
                        w->unk13C8 = EftVolleyAim_GetNodeSide(w->pose.node);
                    }
                    shot->flags |= 4;
                    EftVolleyAim_InitShot(objId, shot, EftEmit_GetAimKind(w->pack), w->unk13C8, w->fired, count, w->unk4,
                                  w->unk8);
                }
            }
            if (w->flags & EFT_VOLLEY_EXTRA) {
                shot->handle = EftBlastObj_CreateWithModel(&arg, ((EftSet *)BtlTask_GetParent(task)->data)->extra);
            } else {
                shot->handle = EftBlastObj_Create(&arg);
            }
            shot->flags |= 1;
            if (w->flags & EFT_VOLLEY_800) {
                EftBlastObj_SetSel(shot->handle, 1, 5, 4);
            }
            if (w->flags & EFT_VOLLEY_1000) {
                EftBlastObj_SetModelAnim(shot->handle, 1);
                if (!(w->fired < count - 1)) {
                    EftBlastObj_MarkLast(shot->handle);
                }
            }
            if (phase == 1) {
                if (shot->flags & 2) {
                    EftBlastObj_MarkLast(shot->handle);
                }
            } else {
                EftBlastObj_SetNoHit(shot->handle, 1);
                EftBlastObj_SetFrozen(shot->handle, 1);
            }
            w->flags |= EFT_VOLLEY_FIRED;
            w->fired++;
            if (phase == 1) {
                w->volleys++;
            }
        }
    }
}

/* Forgets the shots whose task has ended; returns 1 while any is alive. */
s32 EftVolley_PruneShots(EftHTask *task) {
    s32 alive = 0;
    EftVolleyShot *shot = ((EftVolleyWork *)task->data)->shot;
    s32 i;

    for (i = 0; i < 30; i++, shot++) {
        if (EftBlastObj_IsAlive(shot->handle)) {
            alive = 1;
        } else {
            shot->handle = NULL;
        }
    }
    return alive;
}

/* Fires on this frame's fighter event when the set has parts for that phase, then steers the live shots. */
s32 EftVolley_UpdateShots(s32 objId, EftHTask *task) {
    s32 phase = -1;
    s32 i = 0;
    EftVolleyWork *w = task->data;
#define sub i

    if (EftShot_TestBits(objId, 2)) {
        phase = 0;
    } else if (EftShot_TestBits(objId, 4)) {
        phase = 1;
        sub = 1;
        EftTechEvt_RequestRestart(objId);
    } else if (EftShot_TestBits(objId, 0x10)) {
        phase = 3;
        sub = 2;
    } else if (EftShot_TestBits(objId, 0x20)) {
        phase = 4;
        sub = 3;
    } else if (EftShot_TestBits(objId, 0x40)) {
        phase = 5;
        sub = 4;
    }
    if (phase >= 0 && ((EftEmit_GetPhaseMask(w->pack) >> phase) & 1)) {
        EftVolley_Fire(objId, task, phase, sub);
    }
    for (i = 0; i < 30; i++) {
        EftVolleyShot *shot = &w->shot[i];

        if (EftBlastObj_IsAlive(shot->handle)) {
            s32 flag2 = 0;

            if (shot->flags & 2) {
                flag2 = 1;
            }
            if (shot->flags & 4) {
                EftVolleyAim_Update(objId, shot, &shot->unk30, flag2, shot->time, w->unk4, w->unk8);
                EftBlastObj_SetTarget(shot->handle, 1, shot);
            }
            if (w->flags & EFT_VOLLEY_ENDING) {
                EftBlastObj_Stop(shot->handle);
            }
            shot->time += 1.0f;
        }
    }
    return EftVolley_PruneShots(task);
#undef sub
}

static const Vec4A sAimForward = { 0.0f, 0.0f, 1.0f, 1.0f };

/* Sets the aim (unit vector from the fighter towards `target`) and the direction the parts use: the aim, or
   straight up (aim kind 7), or the aim lifted by 45 degrees (aim kind 8). */
void EftVolley_Aim(s32 objId, EftHTask *task, Vec4 *target, s32 bySlot) {
    EftVolleyWork *w = task->data;
    EftHSlot *slot = w->slot;
    s32 kind = EftEmit_GetAimKind(w->pack);

    if (bySlot) {
        EftAim_GetDirKeep(slot, &w->aim, target, objId);
    } else {
        EftAim_GetDir(&w->aim, target, objId);
    }
    Vec4_Copy(&w->dir, &w->aim);
    switch (kind) {
    case 7:
        Vec4_Set(&w->dir, 0.0f, -1.0f, 0.0f, 1.0f);
        break;
    case 8: {
        Mtx44 m;
        Vec4A fwd = sAimForward;
        f32 pitch;
        f32 yaw;

        Vec3_Clamp(&w->dir, &w->dir, -1.0f, 1.0f);
        pitch = Mathf_Asin(-w->dir.y);
        yaw = atan2f(w->dir.x, w->dir.z);
        pitch = EftMath_WrapAngle(pitch + 3.14159265f / 4.0f);
        yaw = EftMath_WrapAngle(yaw);
        Mtx_StoreIdentity(&m);
        Mtx_RotateX(&m, &m, pitch);
        Mtx_RotateY(&m, &m, yaw);
        Mtx_MulVec4(&w->dir, &m, (Vec4 *)&fwd);
        w->dir.w = 1.0f;
        Vec3_Normalize(&w->dir, &w->dir);
        break;
    }
    }
}

/* Gives every emitter of the set its flags of the frame (or the reset flags) and spawns with them. Emitters of
   a later phase are skipped, and so are the phase 5 ones of techniques 0x26E / 0x26F. */
void EftVolley_UpdateParts(s32 objId, EftHTask *task, EftSet *set, s32 reset) {
    EftVolleyWork *w = task->data;
    s32 type;
    s32 i;

    for (type = 0; type < EFT_SET_KIND_COUNT; type++) {
        if (set->head->mask & (1U << type)) {
            EftSetGroupDef *def = set->group[type].entry;

            for (i = 0; i < def->nParts; i++) {
                s32 flags;
                s32 wf;

                if (EftEmit_IsPartDeferred(set, def->kind, i)) {
                    continue;
                }
                wf = w->flags;
                if ((wf & EFT_VOLLEY_800) && set->parts[set->group[def->kind].firstPart + i].phase == 5) {
                    continue;
                }
                if (reset == 0) {
                    flags = EftEmit_GetFlagsFromReq(set, &w->state, objId, type, i, wf & EFT_VOLLEY_ENDING,
                                                    wf & EFT_VOLLEY_4);
                } else {
                    flags = EftEmit_GetResetFlags(set, &w->state, type, i);
                }
                if (w->flags & EFT_VOLLEY_RESET) {
                    flags = 2;
                }
                if (flags != 0) {
                    EftEmit_SpawnOwn(set, &w->state, &w->pose, &w->unk60, &w->dir, type, i, flags, w->unk10);
                }
            }
        }
    }
}

/* Instance init: clears the work, takes the shot parameters of the slot and the group's emitter set. */
/* The slot is a local copy of the argument (`EftHSlot *slot = arg;`, declared behind a local that is initialised
   by a call): the callback's argument was presumably untyped in the original. With the parameter used directly
   6 instructions differ: gcse's PRE then merges the load of slot->param for the id test with the one for the
   flag test (the original reloads it), the same thing as in EftBlast_Init (eft_j.c). Only the LAST use (the
   owner tag) has to go through the copy; a copy declared in front of the first call is folded away again. Found
   by decomp-permuter. */
void EftVolley_Init(EftHTask *task, EftHSlot *arg) {
    EftSet *set = BtlTask_GetParent(task)->data;
    EftVolleyWork *w = task->data;
    EftHSlot *slot = arg;
    EftShotParam *p;
    s32 id;

    memset(w, 0, sizeof(EftVolleyWork));
    w->slot = slot;
    p = slot->param;
    w->unk4 = p->unk34;
    w->unk8 = p->unk38;
    w->unkC = p->unk30;
    w->unk10 = p->unk30;
    w->unk14 = w->unkC;
    w->pack = set;
    EftEmit_InitState(set, &w->state);
    w->endLimit = EftEmit_GetEndFrames(w->pack);
    id = slot->param->id;
    if (id == 0x1C6) {
        w->flags |= EFT_VOLLEY_ID1C6;
    }
    if (slot->param->flags & 0x20000) {
        w->flags |= EFT_VOLLEY_EXTRA;
    }
    if ((u16)slot->param->id - 0x26E < 2U) {
        w->flags |= EFT_VOLLEY_800 | EFT_VOLLEY_1000;
    }
    BtlTask_SetOwnerTag(task, slot->arg.chr == 0 ? 0x800 : 0x1000);
}

/* Instance term: releases the emitter state and, for a technique, sets the fighter's held flag 0xA8. */
void EftVolley_Term(EftHTask *task) {
    EftVolleyWork *w = task->data;
    EftHSlot *slot = w->slot;

    EftEmit_TermState(w->pack, &w->state);
    if (slot->param->kind != 0) {
        EftShot_SetHeldFlagA8(slot->arg.chr);
    }
}

/* Instance update: follows the fighter's effect requests through the steps aim / fire / end. */
void EftVolley_Update(EftHTask *task) {
    EftVolleyWork *w = task->data;
    EftHSlot *slot = w->slot;
    s32 next = 0;
    s32 alive;

    if (BtlScene_IsCharStopped(slot->arg.chr)) {
        return;
    }
    EftEmit_UpdateNodesReq(slot, &w->pose);
    if (!(w->flags & EFT_VOLLEY_AIMED)) {
        if (EftShot_TestBits(slot->arg.chr, 2)) {
            EftVolley_Aim(slot->arg.chr, task, &w->pose.target, 0);
            task->state = 0;
            w->flags |= EFT_VOLLEY_AIMED;
        } else if (EftShot_TestBits(slot->arg.chr, 4)) {
            task->state = 0;
            w->flags |= EFT_VOLLEY_AIMED;
        }
    }
    if (w->flags & EFT_VOLLEY_AIMED) {
        switch (task->state) {
        case 0:
            if (EftShot_TestBits(slot->arg.chr, 4)) {
                w->flags |= EFT_VOLLEY_AIMED2;
                Vec4_Copy(&w->unk50, &w->pose.nodePos);
                Vec4_Copy(&w->unk60, &w->unk50);
                EftVolley_Aim(slot->arg.chr, task, &w->unk60, 1);
                task->state = 1;
            }
            break;
        case 1:
            next = 1;
            break;
        case 2:
            if (EftShot_TestBits(slot->arg.chr, 4) && (slot->param->flags & 0x800)) {
                Vec4_Copy(&w->unk50, &w->pose.nodePos);
                Vec4_Copy(&w->unk60, &w->unk50);
                EftVolley_Aim(slot->arg.chr, task, &w->unk60, 1);
            }
            if (EftShot_TestBits(slot->arg.chr, 8)) {
                w->flags |= EFT_VOLLEY_ENDING | EFT_VOLLEY_8;
                task->state = 3;
            }
            break;
        case 3:
            next = 1;
            break;
        }
    }
    alive = EftVolley_UpdateShots(slot->arg.chr, task);
    if (alive == 0 && (w->flags & EFT_VOLLEY_FIRED) && !(w->volleys < slot->param->shots)) {
        w->flags |= EFT_VOLLEY_ENDING;
    }
    if (EftShot_TestBits(slot->arg.chr, 0x400)) {
        if (!(w->flags & EFT_VOLLEY_8)) {
            w->flags |= EFT_VOLLEY_4;
        }
        w->flags |= EFT_VOLLEY_ENDING;
    }
    EftVolley_UpdateParts(slot->arg.chr, task, w->pack, 0);
    if (next) {
        task->state++;
    }
    if (w->flags & EFT_VOLLEY_ENDING) {
        w->endTime += 1.0f;
    }
    if (alive == 0 && (w->flags & EFT_VOLLEY_DEAD)) {
        BtlTask_SetDead(task);
    } else if (w->flags & EFT_VOLLEY_ENDING) {
        if ((w->flags & EFT_VOLLEY_4) || w->endTime >= w->endLimit) {
            w->flags |= EFT_VOLLEY_DEAD;
        }
    } else if (slot->param->flags & 1) {
        if (w->flags & EFT_VOLLEY_AIMED2) {
            EftEmit_UpdateTrailWidth(w->pack, &w->state);
        }
    }
}

/* Instance post-update: drops dead emitters and shots; a task flagged 4 starts ending. */
void EftVolley_PostUpdate(EftHTask *task) {
    EftVolleyWork *w = task->data;

    if (BtlScene_IsCharStopped(w->slot->arg.chr)) {
        return;
    }
    EftEmit_UpdateAlive(w->pack, &w->state);
    EftVolley_PruneShots(task);
    if (task->unk8 & 4) {
        w->flags |= EFT_VOLLEY_ENDING;
    }
}

/* Instance reset: kills the emitters once and the task. */
void EftVolley_Reset(EftHTask *task) {
    EftVolleyWork *w = task->data;

    if (!(w->flags & EFT_VOLLEY_RESET)) {
        w->flags |= EFT_VOLLEY_RESET;
        EftEmit_KillAll(w->pack, &w->state);
    }
    BtlTask_SetDead(task);
}

/* Instance draw: nothing (the shots and the emitters draw themselves). */
void EftVolley_Draw(EftHTask *task) {
}

/* Group init: loads the technique's emitter set, makes room for two instances, and keeps pack entry 1 when the
   technique has flag 0x20000. */
void EftVolleyGroup_Init(EftHTask *task, EftHSlot *slot) {
    EftSet *set = task->data;

    EftShot_Nop(sizeof(EftSet));
    memset(set, 0, sizeof(EftSet));
    EftEmit_LoadSet(slot, set, NULL, slot->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, sizeof(EftVolleyWork));
    if (slot->param->flags & 0x20000) {
        set->extra = BtlScene_GetPackEntry(slot->pack, 1);
    }
}

/* Group term: frees the set's resources. */
void EftVolleyGroup_Term(EftHTask *task) {
    EftEmit_FreeSet(task->data);
}

/* Group update: per-frame clear of the set's resources. */
void EftVolleyGroup_Update(EftHTask *task) {
    EftEmit_BeginFrame(task->data);
}

/* Group reset: nothing. */
void EftVolleyGroup_Reset(EftHTask *task) {
}

/* ---- emitter set library ----------------------------------------------------------------------------------- */

/* Which of the four resource arrays the groups of a type keep their resources in. */
s32 EftEmit_TypeToResKind(s32 type) {
    s32 kind = 0;

    switch (type) {
    case 2:
        kind = 3;
        break;
    case 5:
    case 10:
    case 12:
    case 15:
    case 16:
    case 17:
    case 18:
        kind = 1;
        break;
    case 9:
        kind = 2;
        break;
    }
    return kind;
}

/* Parses an emitter set: header, group table, emitters. `head` given: only the tables are set up. Otherwise the
   header is entry `idx` of the pack `base` (or common entry `idx` when `common` is 1), and the entries after it
   are the resources of the groups in order, which are loaded into arrays from the current pool. */
void EftEmit_LoadSet(void *owner, EftSet *set, EftSetHead *given, s32 *base, s32 common, s32 idx) {
    s32 nPart = 0;
    s32 nPair = 0;
    s32 nRes = 0;
    s32 i;
    EftSetHead *head;
    EftSetGroupDef *defs;
    s32 off;
    EftSetDef *parts;
    EftSetGroup *g;
    s32 next;
    s32 *tbl;
    s32 j;

    set->owner = owner;
    if (given != NULL) {
        head = given;
    } else if (common == 1) {
        head = (EftSetHead *)BtlScene_GetCommonEntry(idx);
    } else {
        head = (EftSetHead *)BtlScene_GetPackEntry(base, idx);
    }
    off = sizeof(EftSetHead);
    set->head = head;
    defs = (EftSetGroupDef *)((u8 *)head + off);
    set->entries = defs;
    off = head->nGroups * sizeof(EftSetGroupDef) + off;
    parts = (EftSetDef *)((u8 *)head + off);
    set->parts = parts;
    tbl = set->countAt;
    for (i = 3; i >= 0; i--) {
        set->count[i] = 0;
    }
    for (i = 0; i < head->nGroups; i++) {
        EftSetGroupDef *def = &defs[i];
        s32 *count;
        s32 cOff;
        s32 n;

        g = &set->group[def->kind];
        g->parts = &parts[nPart];
        g->entry = def;
        g->firstPart = nPart;
        g->firstPair = nPair;
        g->firstRes = nRes;
        g->cat = EftEmit_TypeToResKind(def->kind);
        cOff = g->cat * sizeof(s32) + EFT_SET_COUNT * sizeof(s32);
        count = (s32 *)((u8 *)tbl + cOff);
        g->catFirst = *count;
        n = g->entry->nParts;
        nPart += n;
        if (def->kind != 0) {
            nPair += n;
            nRes += g->entry->nRes;
            *count += g->entry->nRes;
        }
    }
    if (given != NULL) {
        return;
    }
    next = idx + 1;
    if (set->count[0] > 0) {
        set->array[0] = BtlPool_Alloc(BtlPool_GetCurrent(), set->count[0] * 0x208);
    }
    if (set->count[1] > 0) {
        set->array[1] = BtlPool_Alloc(BtlPool_GetCurrent(), set->count[1] * 0x108);
    }
    if (set->count[2] > 0) {
        set->array[2] = BtlPool_Alloc(BtlPool_GetCurrent(), set->count[2] * 0x88);
    }
    if (set->count[3] > 0) {
        set->array[3] = BtlPool_Alloc(BtlPool_GetCurrent(), set->count[3] * 0x48);
    }
    for (i = 0; i < head->nGroups; i++) {
        EftSetGroupDef *def = &defs[i];
        s32 two;

        if (def->kind == 0) {
            continue;
        }
        two = 1;
        g = &set->group[def->kind];
        if (def->kind == 2 || def->kind == 0xE) {
            two = 0;
        }
        for (j = 0; j < def->nRes; j++) {
            s32 n = g->firstRes + j;
            s32 m;

            if (common == 1) {
                set->resAt[EFT_SET_RES + n] = BtlScene_GetCommonEntry(next++);
            } else {
                set->resAt[EFT_SET_RES + n] = BtlScene_GetPackEntry(base, next++);
            }
            m = g->catFirst + j;
            switch (g->cat) {
            case 0:
                EftTexSet_Load32(set->array[0] + m * 0x208, set->resAt[EFT_SET_RES + n]);
                break;
            case 1:
                EftTexSet_Load16(set->array[1] + m * 0x108, set->resAt[EFT_SET_RES + n]);
                break;
            case 2:
                EftTexSet_Load8(set->array[2] + m * 0x88, set->resAt[EFT_SET_RES + n]);
                break;
            case 3:
                EftTexSet_Load4(set->array[3] + m * 0x48, set->resAt[EFT_SET_RES + n]);
                break;
            }
        }
        for (j = 0; j < def->nParts; j++) {
            s32 n = g->firstPair + j;

            if (common == 1) {
                set->pairAt[EFT_SET_PAIR + n].a = BtlScene_GetCommonEntry(next++);
            } else {
                set->pairAt[EFT_SET_PAIR + n].a = BtlScene_GetPackEntry(base, next++);
            }
            if (two) {
                if (common == 1) {
                    set->pairAt[EFT_SET_PAIR + n].b = BtlScene_GetCommonEntry(next++);
                } else {
                    set->pairAt[EFT_SET_PAIR + n].b = BtlScene_GetPackEntry(base, next++);
                }
            }
        }
    }
}

/* Frees the resource arrays of a set and forgets its tables. */
void EftEmit_FreeSet(EftSet *set) {
    set->head = NULL;
    set->entries = NULL;
    set->parts = NULL;
    if (set->count[0] > 0) {
        BtlPool_Free(BtlPool_GetCurrent(), set->array[0]);
    }
    if (set->count[1] > 0) {
        BtlPool_Free(BtlPool_GetCurrent(), set->array[1]);
    }
    if (set->count[2] > 0) {
        BtlPool_Free(BtlPool_GetCurrent(), set->array[2]);
    }
    if (set->count[3] > 0) {
        BtlPool_Free(BtlPool_GetCurrent(), set->array[3]);
    }
}

/* Clears the last word of every resource object of the set (its per-frame state). */
void EftEmit_BeginFrame(EftSet *set) {
    s32 kind;
    s32 i;

    for (kind = 0; kind < 4; kind++) {
        for (i = 0; i < set->count[kind]; i++) {
            switch (kind) {
            case 0:
                if (set->array[0] != NULL) {
                    *(s32 *)(set->array[0] + i * 0x208 + 0x204) = 0;
                }
                break;
            case 1:
                if (set->array[1] != NULL) {
                    *(s32 *)(set->array[1] + i * 0x108 + 0x104) = 0;
                }
                break;
            case 2:
                if (set->array[2] != NULL) {
                    *(s32 *)(set->array[2] + i * 0x88 + 0x84) = 0;
                }
                break;
            case 3:
                if (set->array[3] != NULL) {
                    *(s32 *)(set->array[3] + i * 0x48 + 0x44) = 0;
                }
                break;
            }
        }
    }
}

/* Header byte 0x21. */
s32 EftEmit_GetHead21(EftSet *set) {
    return set->head->unk21;
}

/* Header byte 0x26. */
s32 EftEmit_GetHead26(EftSet *set) {
    return set->head->unk26;
}

/* Header byte 6: how EftVolley_Aim bends the direction. */
s32 EftEmit_GetAimKind(EftSet *set) {
    return set->head->unk6;
}

/* Header byte 0x22. */
s32 EftEmit_GetHead22(EftSet *set) {
    return set->head->unk22;
}

/* Header byte 0x25. */
s32 EftEmit_GetHead25(EftSet *set) {
    return set->head->unk25;
}

/* Header float 0x28. */
f32 EftEmit_GetHead28(EftSet *set) {
    return set->head->unk28;
}

/* Header byte 0x20: frames an instance lives on after its end was asked. */
s32 EftEmit_GetEndFrames(EftSet *set) {
    return set->head->unk20;
}

/* Header byte 0x27: bit per phase the set has emitters for. */
s32 EftEmit_GetPhaseMask(EftSet *set) {
    return set->head->phaseMask;
}

/* Flags byte of emitter `idx` of group `type`. */
s32 EftEmit_GetPartFlags(EftSet *set, s32 type, s32 idx) {
    return set->parts[set->group[type].firstPart + idx].flags;
}

/* 1 when the emitter's phase is one the set handles through shots and its unkA matches that phase (or is 5):
   such an emitter is not driven by the owner task. */
s32 EftEmit_IsPartDeferred(EftSet *set, s32 type, s32 idx) {
    s32 n = set->group[type].firstPart + idx;
    s32 ret = 0;
    s32 phase = set->parts[n].phase;
    s32 a = set->parts[n].unkA;

    if ((set->head->phaseMask >> phase) & 1) {
        if (a == 5) {
            ret = 1;
        } else {
            switch (phase) {
            case 0:
                if (a == 0) {
                    ret = 1;
                }
                break;
            case 1:
                if (a == 1) {
                    ret = 1;
                }
                break;
            case 3:
                if (a == 2) {
                    ret = 1;
                }
                break;
            case 4:
                if (a == 3) {
                    ret = 1;
                }
                break;
            case 5:
                ret = a == 4;
                break;
            }
        }
    }
    return ret;
}

/* Resets the per-instance state of a set's emitters. */
void EftEmit_InitState(EftSet *set, EftSetState *st) {
    s32 i;
    EftSetHead *head;
    EftSetDef *parts;
    void **h;
    s32 j;

    st->owner = set->owner;
    head = set->head;
    parts = set->parts;
    for (i = 0; i < head->nParts; i++) {
        st->flag[i] = 0;
        st->unk168[i] = parts[i].unk1C;
    }
    st->unk2A8 = head->unk8;
    st->unk2B0 = head->unk14 * 30.0f;
    st->unk2AC = st->unk2B0 * head->unk18;
    st->unk2B4 = 0.0f;
    st->unk2B8 = head->unk2C;
    st->unk2C0 = head->unk38 * 30.0f;
    st->unk2BC = st->unk2C0 * head->unk3C;
    st->unk2C4 = 0.0f;
    h = st->handle[0];
    for (j = 0; j < 2; j++, h += 40) {
        for (i = 39; i >= 0; i--) {
            h[i] = NULL;
        }
    }
}

/* Counterpart of EftEmit_InitState: nothing to release. */
void EftEmit_TermState(EftSet *set, EftSetState *st) {
}

/* Flags for one emitter from the fighter's effect requests of this frame. */
s32 EftEmit_GetFlagsFromReq(EftSet *set, EftSetState *st, s32 objId, s32 type, s32 idx, s32 ending, s32 kill) {
    s32 req = 2;

    if (!EftShot_TestBits(objId, 2)) {
        req = 0;
    }
    if (EftShot_TestBits(objId, 4)) {
        req |= 4;
    }
    if (EftShot_TestBits(objId, 8)) {
        req |= 8;
    }
    if (EftShot_TestBits(objId, 0x10)) {
        req |= 0x10;
    }
    if (EftShot_TestBits(objId, 0x20)) {
        req |= 0x20;
    }
    if (EftShot_TestBits(objId, 0x40)) {
        req |= 0x40;
    }
    return EftEmit_GetFlags(set, st, objId, type, idx, ending, kill, req);
}

/* Flags for one emitter from a mask of phases 0..5 (bit n = phase n happens now). */
s32 EftEmit_GetFlagsFromMask(EftSet *set, EftSetState *st, s32 objId, s32 type, s32 idx, s32 ending, s32 kill,
                             s32 mask) {
    s32 req = (mask & 1) << 1;

    if (mask & 2) {
        req |= 4;
    }
    if (mask & 4) {
        req |= 8;
    }
    if (mask & 8) {
        req |= 0x10;
    }
    if (mask & 0x10) {
        req |= 0x20;
    }
    if (mask & 0x20) {
        req |= 0x40;
    }
    return EftEmit_GetFlags(set, st, objId, type, idx, ending, kill, req);
}

/* The flags (EFT_CMD_*) EftEmit_Spawn should get for emitter `idx` of group `type` this frame. `req` holds the
   request bits 2, 4, 0x10, 0x20, 0x40 (phases 0, 1, 3, 4, 5 begin), `ending` that the instance is ending and
   `kill` that it must stop at once. */
s32 EftEmit_GetFlags(EftSet *set, EftSetState *st, s32 objId, s32 type, s32 idx, s32 ending, s32 kill, s32 req) {
    EftSetGroup *g = &set->group[type];
    s32 flags = 0;
    EftSetGroupDef *def;
    EftSetDef *part;
    s32 n;
    s32 f;
    s32 started;
    s32 pf;
    s32 f2;

    n = g->firstPart + idx;
    def = g->entry;
    part = &set->parts[n];
    if (kill == 0) {
        if (req & 2) {
            flags = part->phase == 0;
            if (part->endPhase == 1) {
                flags |= EFT_CMD_STOP;
            }
        }
        if (req & 4) {
            if (part->phase == 1) {
                flags |= EFT_CMD_START;
            }
            if (part->endPhase == 2) {
                flags |= EFT_CMD_STOP;
            }
        }
        if (req & 0x10) {
            if (part->phase == 3) {
                flags |= EFT_CMD_START;
            }
            if (part->endPhase == 4) {
                flags |= EFT_CMD_STOP;
            }
        }
        if (req & 0x20) {
            if (part->phase == 4) {
                flags |= EFT_CMD_START;
            }
            if (part->endPhase == 5) {
                flags |= EFT_CMD_STOP;
            }
        }
        if (req & 0x40) {
            if (part->phase == 5) {
                flags |= EFT_CMD_START;
            }
            if (part->endPhase == 6) {
                flags |= EFT_CMD_STOP;
            }
        }
        if (st->flag[n] & 0x40) {
            flags |= EFT_CMD_START;
        }
        if ((st->flag[n] & 0x80) && def->kind == 0x11) {
            flags |= EFT_CMD_RESTART;
            st->flag[n] &= 0x7F;
        }
    } else {
        flags = EFT_CMD_FADE;
    }
    if (kill != 0) {
        flags |= EFT_CMD_STOP;
    } else if (ending != 0) {
        if (part->phase == 2) {
            flags |= EFT_CMD_START;
        } else if (part->phase != 6) {
            flags |= EFT_CMD_STOP;
        }
    }
    f = st->flag[n];
    started = f & 2;
    if (started) {
        flags &= ~EFT_CMD_START;
    }
    if (f & 4) {
        flags &= ~EFT_CMD_STOP;
    }
    pf = part->flags;
    if (pf & 2) {
        if (def->kind == 0x11 || def->kind == 0xF) {
            flags &= ~EFT_CMD_MOVE;
            flags |= EFT_CMD_200;
        } else {
            flags &= ~EFT_CMD_MOVE;
        }
    } else {
        if (!started) {
            goto end;
        }
        if (part->phase != 2) {
            flags |= EFT_CMD_MOVE;
        }
    }
    f2 = st->flag[n];
    if (f2 & 2) {
        if (!(f2 & 4)) {
            flags |= EFT_CMD_SCALE;
        }
        if (!(f2 & 4)) {
            s32 a = part->unkA;

            if (f2 & 0x20) {
                a = part->unk36;
            }
            if (a == 5) {
                flags |= EFT_CMD_DIR;
            } else if (pf & 0x80) {
                flags |= EFT_CMD_DIR;
            }
        }
    }
end:
    return flags;
}

/* Flags for a reset: move and warp a started, not yet stopped emitter (type 17 always, others when they aim
   along the direction). */
s32 EftEmit_GetResetFlags(EftSet *set, EftSetState *st, s32 type, s32 idx) {
    s32 n = set->group[type].firstPart + idx;
    s32 flags = 0;
    s32 f = st->flag[n];
    EftSetDef *part = &set->parts[n];

    if (f & 2) {
        if (!(f & 4)) {
            s32 a = part->unkA;

            if (f & 0x20) {
                a = part->unk36;
            }
            flags = 0x30;
            if (type != 0x11 && a != 5) {
                flags = 0;
            }
        }
    }
    return flags;
}

/* Marks the task of a spawned object as belonging to character `chr` (so BtlScene_Reset(2 + chr) reaches it),
   except for the group types 2 and 5. */
void EftEmit_TagTask(void *task, s32 chr, s32 type) {
    s32 flags;

    if (task != NULL && type != 2 && type != 5) {
        flags = chr == 0 ? 0x800 : 0x1000;
        BtlTask_SetOwnerTag(task, flags);
        flags = 0; /* dead store: the original does not tail-call here */
    }
}

/* ---- per-type spawners ------------------------------------------------------------------------------------ */
/* Called by EftEmit_Spawn (0x14FF90) through its jump table. Common arguments: the set, the handle table of the
   instance (one task per emitter), the EFT_CMD_* flags, the group type, the character, the emitter's index in
   its group, the node position and the direction. `size` is the emitter's current size, `scale` what its
   offsets are multiplied by, `rate` goes to the module unchanged. Each one creates the module's object on
   EFT_CMD_START when the handle is free, then forwards move / size / direction / stop / kill. */

#define H(n) (handles->h[n])
#define ZERO_VEC { 0.0f, 0.0f, 0.0f, 0.0f }

extern void *EftRay_Create(EftEmitLightArg *arg);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void *EftRay_CreateByValue(EftEmitLightArg arg);
#define EftRay_CreateByValue(arg) EftRay_CreateByValue(*(EftEmitLightArg *)(arg))
#else
extern void *EftRay_CreateByValue(EftEmitLightArg *arg);
#endif
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftRay_SetPos(void *obj, PortVec4 pos);
#define EftRay_SetPos(obj, pos) EftRay_SetPos(obj, *(PortVec4 *)(pos))
#else
extern void EftRay_SetPos(void *obj, Vec4 *pos);
#endif
extern void EftRay_Kill(void *obj);
extern void EftRay_SetType(void *obj, s32 type);

/* Type 0: a light of kind EftSetDef.unk4 (0 or 1) at the node. */
#if 0 /* not matched: 218 instructions in the original, 219 here; 50 differ after alignment (the form stored before: 223
instructions, 67). What is known (cleanup X2, allocation dumps):
- The original shares `jal EftRay_SetType` and the whole EftEmit_TagTask call between the two cases (case 0 ends
  in a jump into case 1). The form with both calls written out in both cases is NOT merged by the compiler: after
  the first jump pass a `(use (const_int 0))` insn follows the last call of case 1 (a call directly in front of a
  label), so the cross-jump pass finds no common tail. It also gives chr / type five references each and puts
  them in s5 / s6, in front of flags / handles.
- The original's registers (flags s5, handles s6, chr s7, type fp) need chr and type to have FOUR references:
  EftRay_SetType written in both cases, EftEmit_TagTask written once; and handles must not be referenced by the
  shared call, so the address of the handle slot is one value set in both cases (`h` below). With that the four
  saved arguments, the frame and the shared tail come out right.
- Still different: `n` and the address of case 1's block exchange s3 / s4 (the original gives `n` the higher
  priority); 0xC0 / 0x70 exchange v0 / v1 in case 0 and the alpha store comes first there; EftRay_SetType's jal is
  not shared (here the store through `h` is scheduled in front of it in both cases, in the original only in
  case 0); `scale` is copied to f20 in front of the switch instead of behind it. The goto is a stand-in for
  whatever structured form shares the tag call; a `created` flag is not threaded (li v1,1 / beqz v1 remains),
  an if / else chain on `kind` merges the two argument blocks into one frame slot (frame 304 instead of 416).
  The values built and the calls made are the same as in the form stored before. */
void EftEmit_SpawnType0(EftSet *set, EftSetHandles *handles, s32 flags, s32 type, s32 chr, s32 idx, Vec4 *pos,
                        f32 size, f32 scale, f32 rate) {
    s32 n = set->group[0].firstPart + idx;
    EftSetDef *part = &set->parts[n];
    s32 kind = part->unk4;
    void **h;

    switch (kind) {
    case 0:
        if (flags & EFT_CMD_START) {
            EftEmitLightArg arg = { ZERO_VEC,
                                    { 0xC0, 0xC0, 0xC0, 0x70 },
                                    rate,
                                    scale * 100.0f,
                                    size,
                                    part->unk10 * scale,
                                    part->unk14 * scale,
                                    1,
                                    part->unk3,
                                    chr,
                                    1,
                                    0,
                                    (f32)part->unk5,
                                    (f32)part->unk6,
                                    0 };

            if (part->phase == 2) {
                arg.unk50 = 1;
            }
            Vec4_Copy((Vec4 *)&arg.pos, pos);
            h = &H(n);
            *h = EftRay_Create(&arg);
            EftRay_SetType(*h, type);
            goto tag;
        }
        break;
    case 1:
        if (flags & EFT_CMD_START) {
            EftEmitLightArg arg = { { 0.0f, 0.0f, 0.0f, 1.0f },
                                    { 0x80, 0x80, 0x80, 0x80 },
                                    rate,
                                    scale * 800.0f,
                                    2.0f,
                                    part->unk10 * scale,
                                    part->unk14 * scale,
                                    3,
                                    part->unk3,
                                    chr,
                                    1,
                                    1,
                                    (f32)part->unk5,
                                    (f32)part->unk6,
                                    1 };

            Vec4_Copy((Vec4 *)&arg.pos, pos);
            h = &H(n);
            *h = EftRay_CreateByValue(&arg);
            EftRay_SetType(*h, type);
        tag:
            EftEmit_TagTask(*h, chr, type);
        }
        break;
    default:
        return;
    }
    if (flags & (EFT_CMD_MOVE | EFT_CMD_STOP)) {
        EftRay_SetPos(H(n), pos);
    }
    if (flags & EFT_CMD_STOP) {
        EftRay_Kill(H(n));
        H(n) = NULL;
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_h", EftEmit_SpawnType0);

extern void *EftRays_Create(EftEmitArg2 *arg);
extern void EftRays_Stop(void *obj);
extern void EftRays_Kill(void *obj);
extern void EftRays_SetPos(void *obj, Vec4 *pos);
extern void EftRays_SetDelay(void *obj, f32 v);
extern void EftRays_SetHold(void *obj, f32 v);
extern void EftRays_SetFade(void *obj, f32 v);
extern void EftRays_SetSize(void *obj, f32 size);

/* Type 2. */
void EftEmit_SpawnType2(EftSet *set, EftSetHandles *handles, s32 flags, s32 type, s32 chr, s32 idx, Vec4 *pos,
                        Vec4 *dir, f32 size, f32 scale, f32 rate) {
    EftSetGroup *g = &set->group[2];
    s32 n = g->firstPart + idx;
    EftSetDef *part = &set->parts[n];
    Vec4 p;

    Vec3_ScaleAdd(&p, dir, pos, part->unk10 * scale);
    p.w = 1.0f;
    if (flags & EFT_CMD_START) {
        if (H(n) == NULL) {
            u8 *res = set->array[3] + (g->catFirst + part->res) * 0x48;
            s32 *tex = set->pair[g->firstPair + idx].a;
            EftEmitArg2 arg = { chr, type, part->unk2, 0, tex, res, rate };

            H(n) = EftRays_Create(&arg);
            EftRays_SetSize(H(n), size);
            if (part->flags & 0x40) {
                EftRays_SetPos(H(n), pos);
            } else {
                EftRays_SetPos(H(n), &p);
            }
            EftRays_SetDelay(H(n), part->unk5);
            EftRays_SetHold(H(n), part->unk6);
            EftRays_SetFade(H(n), part->unk7);
            EftEmit_TagTask(H(n), chr, type);
        }
    }
    if (H(n) != NULL) {
        if (flags & (EFT_CMD_MOVE | EFT_CMD_STOP)) {
            EftRays_SetPos(H(n), &p);
        }
        if (flags & EFT_CMD_SCALE) {
            EftRays_SetSize(H(n), size);
        }
        if (flags & EFT_CMD_KILL) {
            EftRays_Kill(H(n));
        } else if (flags & EFT_CMD_STOP) {
            EftRays_Stop(H(n));
        }
    }
}

extern void *EftBill_Create(EftEmitArgA *arg);
extern s32 EftBill_Stop(void *obj);
extern s32 EftBill_Kill(void *obj);
extern s32 EftBill_SetPos(void *obj, Vec4 *pos);
extern s32 EftBill_SetDir(void *obj, Vec4 *dir);
extern s32 EftBill_SetSize(void *obj, f32 size);
extern s32 EftBill_SetDelay(void *obj, f32 v);
extern s32 EftBill_SetEndDelay(void *obj, f32 v);
extern s32 EftBill_SetFront(void *obj);
extern s32 EftBill_SetType(void *obj, s32 type);

/* Type 16. */
/* Matching note (EftEmit_SpawnType16): the resource pointer is built in two statements with texA read in between
   (index first, then texA, `res += ...`, texB). The single-expression form of EftEmit_SpawnType17 / 18 gives the
   same instructions with texA / texB in s1 / s2 exchanged: res, texA and texB are block-local values allocated in
   order of refs / life, and the original's texB lives one instruction less than texA, which needs the first
   scheduling pass to emit "load texA, res add, load texB". That happens only when the add has one dying operand
   (`res += x`, not `res = base + x`), stands behind texA in the source, and g->catFirst is read before
   g->firstPair. */
void EftEmit_SpawnType16(EftSet *set, EftSetHandles *handles, s32 flags, s32 type, s32 chr, s32 idx, Vec4 *pos,
                         Vec4 *dir, f32 size, f32 scale, f32 rate) {
    EftSetGroup *g = &set->group[16];
    s32 n = g->firstPart + idx;
    EftSetDef *part = &set->parts[n];
    Vec4 p;

    Vec3_ScaleAdd(&p, dir, pos, part->unk10 * scale);
    p.w = 1.0f;
    if (flags & EFT_CMD_START) {
        if (H(n) == NULL) {
            u8 *res = set->array[1];
            s32 ri = g->catFirst + part->res;
            s32 *texA = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].a;
            s32 *texB;

            res += ri * 0x108;
            texB = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].b;
            {
                EftEmitArgA arg = { ZERO_VEC, ZERO_VEC, chr, part->unk2, rate, size, res, texA, texB };

                if (part->flags & 0x40) {
                    Vec4_Copy((Vec4 *)&arg.pos, pos);
                } else {
                    Vec4_Copy((Vec4 *)&arg.pos, &p);
                }
                Vec4_Copy((Vec4 *)&arg.dir, dir);
                H(n) = EftBill_Create(&arg);
                EftBill_SetDelay(H(n), part->unk5);
                EftBill_SetEndDelay(H(n), part->unk6);
                if (part->flags & 0x20) {
                    EftBill_SetFront(H(n));
                }
                EftBill_SetType(H(n), type);
                EftEmit_TagTask(H(n), chr, type);
            }
        }
    }
    if (H(n) != NULL) {
        if (flags & EFT_CMD_MOVE) {
            EftBill_SetPos(H(n), &p);
        }
        if (flags & EFT_CMD_SCALE) {
            EftBill_SetSize(H(n), size);
        }
        if (flags & EFT_CMD_DIR) {
            EftBill_SetDir(H(n), dir);
        }
        if (flags & EFT_CMD_KILL) {
            EftBill_Kill(H(n));
            H(n) = NULL;
        } else if (flags & EFT_CMD_STOP) {
            if (flags & EFT_CMD_FADE) {
                EftBill_SetEndDelay(H(n), 0.0f);
            }
            EftBill_Stop(H(n));
        }
    }
}

extern void *EftRibbon_Create(EftEmitArg17 *arg);
extern s32 EftRibbon_Stop(void *obj);
extern s32 EftRibbon_Kill(void *obj);
extern s32 EftRibbon_SetPos(void *obj, Vec4 *pos);
extern s32 EftRibbon_SetEnd(void *obj, Vec4 *pos2, s32 warp);
extern s32 EftRibbon_SetSize(void *obj, f32 size);
extern s32 EftRibbon_SetMaxNodes(void *obj, s32 v);
extern s32 EftRibbon_SetDelay(void *obj, f32 v);
extern s32 EftRibbon_SetFadeDelay(void *obj, f32 v);
extern s32 EftRibbon_SetFadeTime(void *obj, f32 v);
extern s32 EftRibbon_SetUnkD8(void *obj, s32 v);

/* Type 17: an object between two points, the node position pushed along the direction by EftSetDef.unk10 and
   pos2 pushed by EftSetDef.unk14. EFT_CMD_RESTART destroys the object and creates it again. */
void EftEmit_SpawnType17(EftSet *set, EftSetHandles *handles, s32 flags, s32 type, s32 chr, s32 arg5, s32 idx,
                         Vec4 *pos, Vec4 *pos2, Vec4 *dir, f32 size, f32 scale, f32 rate) {
    EftSetGroup *g = &set->group[17];
    s32 create = 0;
    s32 n = g->firstPart + idx;
    EftSetDef *part = &set->parts[n];
    Vec4 p;
    Vec4 p2;

    Vec3_ScaleAdd(&p, dir, pos, part->unk10 * scale);
    p.w = 1.0f;
    Vec3_ScaleAdd(&p2, dir, pos2, part->unk14 * scale);
    p2.w = 1.0f;
    if ((flags & EFT_CMD_START) && H(n) == NULL) {
        create = 1;
    } else if (flags & EFT_CMD_RESTART) {
        if (H(n) != NULL) {
            EftRibbon_Kill(H(n));
            create = 1;
            H(n) = NULL;
        }
    }
    if (create) {
        u8 *res = set->array[1] + (g->catFirst + part->res) * 0x108;
        s32 *texA = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].a;
        s32 *texB = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].b;
        EftEmitArg17 arg = { ZERO_VEC, ZERO_VEC, type, chr, part->unk2, rate, size, res, texA, texB };

        Vec4_Copy((Vec4 *)&arg.dir, dir);
        if (part->flags & 0x40) {
            Vec4_Copy((Vec4 *)&arg.pos, pos);
        } else {
            Vec4_Copy((Vec4 *)&arg.pos, &p);
        }
        H(n) = EftRibbon_Create(&arg);
        EftRibbon_SetUnkD8(H(n), arg5);
        EftRibbon_SetDelay(H(n), part->unk5);
        EftRibbon_SetFadeDelay(H(n), part->unk6);
        EftRibbon_SetFadeTime(H(n), part->unk7);
        if (part->unk34 & 2) {
            EftRibbon_SetMaxNodes(H(n), part->unk3);
        }
        EftEmit_TagTask(H(n), chr, type);
    }
    if (H(n) != NULL) {
        s32 warp = 0;

        if (flags & EFT_CMD_MOVE) {
            if (!(flags & EFT_CMD_WARP)) {
                EftRibbon_SetPos(H(n), &p);
            } else {
                warp = 1;
            }
            EftRibbon_SetEnd(H(n), &p2, warp);
        } else if (flags & EFT_CMD_200) {
            EftRibbon_SetEnd(H(n), &p2, 0);
        }
        if (flags & EFT_CMD_SCALE) {
            EftRibbon_SetSize(H(n), size);
        }
        if (flags & EFT_CMD_KILL) {
            EftRibbon_Kill(H(n));
            H(n) = NULL;
        } else if (flags & EFT_CMD_STOP) {
            EftRibbon_Stop(H(n));
        }
    }
}

#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void *EftChain_Create(EftEmitArgA arg);
#define EftChain_Create(arg) EftChain_Create(*(EftEmitArgA *)(arg))
#else
extern void *EftChain_Create(EftEmitArgA *arg);
#endif
extern s32 EftChain_Stop(void *obj);
extern s32 EftChain_Kill(void *obj);
extern s32 EftChain_SetPos(void *obj, Vec4 *pos);
extern s32 EftChain_Warp(void *obj, Vec4 *pos);
extern s32 EftChain_SetDir(void *obj, Vec4 *dir);
extern s32 EftChain_SetSize(void *obj, f32 size);
extern s32 EftChain_SetParam3(void *obj, s32 v);
extern s32 EftChain_SetParam5(void *obj, f32 v);
extern s32 EftChain_SetParam6(void *obj, f32 v);
extern s32 EftChain_SetViewOnly(void *obj);
extern s32 EftChain_SetType(void *obj, s32 type);

/* Type 18. */
void EftEmit_SpawnType18(EftSet *set, EftSetHandles *handles, s32 flags, s32 type, s32 chr, s32 arg5, s32 idx,
                         Vec4 *pos, Vec4 *dir, f32 size, f32 scale, f32 rate) {
    EftSetGroup *g = &set->group[18];
    s32 n = g->firstPart + idx;
    EftSetDef *part = &set->parts[n];
    Vec4 p;

    Vec3_ScaleAdd(&p, dir, pos, part->unk10 * scale);
    p.w = 1.0f;
    if (flags & EFT_CMD_START) {
        if (H(n) == NULL) {
            u8 *res = set->array[1] + (g->catFirst + part->res) * 0x108;
            s32 *texA = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].a;
            s32 *texB = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].b;
            EftEmitArgA arg = { ZERO_VEC, ZERO_VEC, chr, part->unk2, rate, size, res, texA, texB };

            Vec4_Copy((Vec4 *)&arg.dir, dir);
            if (part->flags & 0x40) {
                Vec4_Copy((Vec4 *)&arg.pos, pos);
            } else {
                Vec4_Copy((Vec4 *)&arg.pos, &p);
            }
            H(n) = EftChain_Create(&arg);
            EftChain_SetParam3(H(n), part->unk3);
            EftChain_SetParam5(H(n), part->unk5);
            EftChain_SetParam6(H(n), part->unk6);
            if (part->flags & 0x20) {
                EftChain_SetViewOnly(H(n));
            }
            EftChain_SetType(H(n), type);
            EftEmit_TagTask(H(n), chr, type);
        }
    }
    if (H(n) != NULL) {
        if (flags & EFT_CMD_MOVE) {
            if (flags & EFT_CMD_WARP) {
                EftChain_Warp(H(n), &p);
            } else {
                EftChain_SetPos(H(n), &p);
            }
        }
        if (flags & EFT_CMD_SCALE) {
            EftChain_SetSize(H(n), size);
        }
        if (flags & EFT_CMD_DIR) {
            EftChain_SetDir(H(n), dir);
        }
        if (flags & EFT_CMD_KILL) {
            EftChain_Kill(H(n));
            H(n) = NULL;
        } else if (flags & EFT_CMD_STOP) {
            if (flags & EFT_CMD_FADE) {
                EftChain_SetParam6(H(n), 0.0f);
            }
            EftChain_Stop(H(n));
        }
    }
}

extern void *EftAnimPart_Create(EftEmitArg14 *arg);
extern s32 EftAnimPart_Stop(void *obj);
extern s32 EftAnimPart_Kill(void *obj);
extern s32 EftAnimPart_SetType(void *obj, s32 type);
extern s32 EftAnimPart_SetPos(void *obj, Vec4 *pos);
extern s32 EftAnimPart_SetSize(void *obj, f32 size);
extern s32 EftAnimPart_SetDir(void *obj, Vec4 *dir);
extern s32 EftAnimPart_SetDelay(void *obj, f32 v);
extern s32 EftAnimPart_SetHold(void *obj, f32 v);
extern s32 EftAnimPart_SetFade(void *obj, f32 v);

/* Type 14. */
/* Matching note: the resource pointer is built in two statements (`res = base; res += index * size;`), as in
   EftEmit_SpawnType16. Written as one expression the sum and the product exchange v1 / a2 (16 instructions). */
void EftEmit_SpawnType14(EftSet *set, EftSetHandles *handles, s32 flags, s32 type, s32 chr, s32 idx, Vec4 *pos,
                         Vec4 *dir, f32 size, f32 scale, f32 rate) {
    EftSetGroup *g = &set->group[14];
    s32 n = g->firstPart + idx;
    EftSetDef *part = &set->parts[n];
    Vec4 p;

    Vec3_ScaleAdd(&p, dir, pos, part->unk10 * scale);
    p.w = 1.0f;
    if (flags & EFT_CMD_START) {
        if (H(n) == NULL) {
            u8 *res = set->array[0];
            s32 *tex;

            res += (g->catFirst + part->res) * 0x208;
            tex = set->pair[g->firstPair + idx].a;
            {
                EftEmitArg14 arg = { chr, part->unk4, rate, ZERO_VEC, ZERO_VEC, size, tex, res };

                if (part->flags & 0x40) {
                    Vec4_Copy((Vec4 *)&arg.pos, pos);
                } else {
                    Vec4_Copy((Vec4 *)&arg.pos, &p);
                }
                Vec4_Copy((Vec4 *)&arg.dir, dir);
                H(n) = EftAnimPart_Create(&arg);
                EftAnimPart_SetDelay(H(n), part->unk5);
                EftAnimPart_SetHold(H(n), part->unk6);
                EftAnimPart_SetFade(H(n), part->unk7);
                EftAnimPart_SetType(H(n), type);
                EftEmit_TagTask(H(n), chr, type);
            }
        }
    }
    if (H(n) != NULL) {
        if (flags & EFT_CMD_MOVE) {
            EftAnimPart_SetPos(H(n), &p);
        }
        if (flags & EFT_CMD_SCALE) {
            EftAnimPart_SetSize(H(n), size);
        }
        if (flags & EFT_CMD_DIR) {
            EftAnimPart_SetDir(H(n), dir);
        }
        if (flags & EFT_CMD_KILL) {
            EftAnimPart_Kill(H(n));
            H(n) = NULL;
        } else if (flags & EFT_CMD_STOP) {
            if (flags & EFT_CMD_FADE) {
                EftAnimPart_SetHold(H(n), 0.0f);
            }
            EftAnimPart_Stop(H(n));
        }
    }
}

extern void *EftPtcl_Create(EftEmitArgA *arg);
extern s32 EftPtcl_Stop(void *obj);
extern s32 EftPtcl_Kill(void *obj);
extern s32 EftPtcl_SetPos(void *obj, Vec4 *pos);
extern s32 EftPtcl_Warp(void *obj, Vec4 *pos);
extern s32 EftPtcl_SetDir(void *obj, Vec4 *dir);
extern s32 EftPtcl_SetSize(void *obj, f32 size);
extern s32 EftPtcl_SetStartDelay(void *obj, f32 v);
extern s32 EftPtcl_SetStopDelay(void *obj, f32 v);
extern s32 EftPtcl_SetLinger(void *obj, f32 v);
extern s32 EftPtcl_SetFront(void *obj);
extern s32 EftPtcl_SetFlag40(void *obj, s32 v);
extern s32 EftPtcl_SetType(void *obj, s32 type);

/* Type 5. */
/* Matching note: see EftEmit_SpawnType16 (same two-step resource pointer). */
void EftEmit_SpawnType5(EftSet *set, EftSetHandles *handles, s32 flags, s32 type, s32 chr, s32 idx, Vec4 *pos,
                        Vec4 *dir, f32 size, f32 scale, f32 rate) {
    EftSetGroup *g = &set->group[5];
    s32 n = g->firstPart + idx;
    EftSetDef *part = &set->parts[n];
    Vec4 p;

    Vec3_ScaleAdd(&p, dir, pos, part->unk10 * scale);
    p.w = 1.0f;
    if (flags & EFT_CMD_START) {
        if (H(n) == NULL) {
            u8 *res = set->array[1];
            s32 ri = g->catFirst + part->res;
            s32 *texA = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].a;
            s32 *texB;

            res += ri * 0x108;
            texB = set->pairAt[EFT_SET_PAIR + g->firstPair + idx].b;
            {
                EftEmitArgA arg = { ZERO_VEC, ZERO_VEC, chr, part->unk2, rate, size, res, texA, texB };

                if (part->flags & 0x40) {
                    Vec4_Copy((Vec4 *)&arg.pos, pos);
                } else {
                    Vec4_Copy((Vec4 *)&arg.pos, &p);
                }
                Vec4_Copy((Vec4 *)&arg.dir, dir);
                H(n) = EftPtcl_Create(&arg);
                EftPtcl_SetStartDelay(H(n), part->unk5);
                EftPtcl_SetStopDelay(H(n), part->unk6);
                EftPtcl_SetLinger(H(n), part->unk7);
                if (part->flags & 0x20) {
                    EftPtcl_SetFront(H(n));
                }
                if (part->unk34 & 8) {
                    EftPtcl_SetFlag40(H(n), 1);
                }
                EftPtcl_SetType(H(n), type);
                EftEmit_TagTask(H(n), chr, type);
            }
        }
    }
    if (H(n) != NULL) {
        if (flags & EFT_CMD_MOVE) {
            if (flags & EFT_CMD_WARP) {
                EftPtcl_Warp(H(n), &p);
            } else {
                EftPtcl_SetPos(H(n), &p);
            }
        }
        if (flags & EFT_CMD_SCALE) {
            EftPtcl_SetSize(H(n), size);
        }
        if (flags & EFT_CMD_DIR) {
            EftPtcl_SetDir(H(n), dir);
        }
        if (flags & EFT_CMD_KILL) {
            EftPtcl_Kill(H(n));
            H(n) = NULL;
        } else if (flags & EFT_CMD_STOP) {
            if (flags & EFT_CMD_FADE) {
                EftPtcl_SetStopDelay(H(n), 0.0f);
            }
            EftPtcl_Stop(H(n));
        }
    }
}
