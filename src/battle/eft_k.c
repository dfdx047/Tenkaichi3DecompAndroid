#include "common.h"
#include "battle/eft_k.h"

/*
 * Effect scene, layer 1 (technique effects), 0x157398..0x15C728 (the former eft_l.c, 0x15B550..0x15C728, is
 * appended as the second part: EftRushShot_UpdateAttached only matches with EftRushShot_UpdateModels defined
 * earlier in its file). The first part, 0x157398..0x15B550, is four pieces, in address order:
 *
 *   0x157398..0x158438  second half of the "shots" technique module EftShotTech (effect type 2; the first half,
 *                       0x156450.., is in eft_j.c). Classes gEftShotTechMgrClass / gEftShotTechClass.
 *   0x158438..0x159130  technique events EftTechEvt: per fighter and per frame, a word of event bits that every
 *                       technique module reads through EftShot_TestBits(objId, mask), and the two timers of a
 *                       technique. Classes gEftTechEvtClass (a sub-task of layer 1) / gEftTechEvtTaskClass.
 *   0x159130..0x15AB38  the "thrown object" technique module EftObjTech (effect type 8). Classes
 *                       gEftObjTechMgrClass / gEftObjTechClass.
 *   0x15AB38..0x15B550  first three helpers of the "rush shot" module EftRushShot (effect type 9, second part).
 *
 * A technique module is two task classes: a manager (one per shot slot, created by EftShot_CreateSlotTask from the
 * class table at 0x2C3700, row = definition effect type + 1) whose work holds the emitter set, and items (a child
 * list of two) created by EftShot_Start when the fighter starts the technique. An item's init receives the shot
 * slot (EftKSrc). Callback order in a class: update, init, term, post-update, reset, draw.
 *
 * The float pool of the first part is 0x2FC7D4..0x2FC828 (21 constants, all checked against the original bits); the
 * second part's follows (to 0x2FC838), and the whole is contiguous with the pool of eft_j.c.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sqrtf(f32 x);
extern f32 sinf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_AxisZToEuler(Vec4 *angles, Mtx44 *m);
extern void Mtx_Translate(Mtx44 *dst, Mtx44 *src, Vec4 *pos);
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateZXY(Mtx44 *dst, Mtx44 *src, Vec4 *angles);
extern void Mtx_RotateXYZ(Mtx44 *dst, Mtx44 *src, Vec4 *angles);
extern void Mtx_ScaleDiagUniform(Mtx44 *dst, Mtx44 *src, f32 scale);
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Clamp(Vec4 *dst, Vec4 *src, f32 a, f32 b);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, Vec4 *from, f32 len);
#define Vec3_ScaleAdd(dst, dir, len, from) Vec3_ScaleAdd(dst, dir, from, len)
#else
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, f32 len, Vec4 *from);
#endif

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlScene_GetCharCount(void);
extern s32 BtlScene_IsCharStopped(s32 objId);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);
extern s32 *BtlScene_GetCharPackEntry(s32 side, s32 idx);
extern f32 BtlScene_GetCharScale(s32 chr);
extern void *BtlTask_CreateChildList(EftKTask *task, s32 count, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, void *cls, void *arg);

extern s32 BtlCharApi_GetAuraType(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern s32 BtlCharApi_IsRushConnected(s32 objId);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);
extern s32 BtlCharApi_IsInSkill(s32 objId);
extern s32 BtlCharApi_IsSkillApplied(s32 objId);
extern s32 BtlCharApi_TestAnimFlag10(s32 objId);
extern s32 BtlCharApi_ObjTestAttr(s32 objId, u64 mask);
extern void BtlCharApi_SetHeldFlagA7(s32 objId);

extern EftKTask *BtlTask_GetParent(EftKTask *task);
extern void BtlTask_SetDead(EftKTask *task);
extern void BtlTask_SetOwnerTag(EftKTask *task, s32 flags);
extern void EftTexSet_Keep32(void *tex, s32 a1, s32 a2);
extern void EftTexSet_Load32(void *tex, s32 *entry);
extern s32 EftShot_TestBits(s32 objId, s32 bits);
extern void EftShot_SetHeldFlagA8(s32 objId);
extern void EftShot_Nop(s32 size);
extern void EftEmit_LoadSet(EftKSrc *src, void *set, s32 a2, s32 *pack, s32 t0, s32 t1);
extern void EftEmit_FreeSet(void *set);
extern void EftEmit_BeginFrame(void *set);
extern u8 EftEmit_GetHead21(void *set);
extern u8 EftEmit_GetHead26(void *set);
extern u8 EftEmit_GetHead22(void *set);
extern f32 EftEmit_GetHead28(void *set);
extern u8 EftEmit_GetEndFrames(void *set);
extern void EftEmit_InitState(void *set, void *emit);
extern void EftEmit_TermState(void *set, void *emit);
extern s32 EftEmit_GetFlagsFromReq(EftKSet *set, void *emit, s32 objId, s32 group, s32 part, s32 f1, s32 f8);
extern void EftEmit_SpawnOwn(EftKSet *set, void *emit, Vec4 *nodes, Vec4 *pos, void *work, s32 group, s32 part,
                          s32 res, f32 scale);
extern void EftEmit_MarkKind6(void *set, void *emit);
extern void EftEmit_KillAll(void *set, void *emit);
extern s32 EftEmit_UpdateAlive(void *set, void *emit);
extern void EftEmit_UpdateNodesReq(EftKSrc *src, Vec4 *nodes);
extern void EftEmit_UpdateTrailWidth(void *set, void *emit);
extern f32 EftEmit_GetTrailWidth(void *emit);
extern s32 EftEmit_HasWidth2(void *set);
extern void EftEmit_UpdateWidth2(void *set, void *emit);
extern f32 EftEmit_GetWidth2(void *emit);
extern void EftShotTech_InitModel(EftKTask *task);
extern void EftShotTech_SetModelPose(EftKTask *task, Vec4 *pos, Vec4 *angles);
extern void EftShotTech_FreeModel(EftKTask *task);
extern void EftShotTech_CalcDir(EftKTask *task, Vec4 *dir);
extern s32 EftShotTech_ReapShots(EftKTask *task);
extern s32 EftShotTech_UpdateShots(s32 objId, EftKTask *task);
extern void EftShotTech_UpdateParts(s32 objId, EftKTask *task, void *set);
extern void EftShotTech_Start(EftKTask *task, s32 evt);
extern void EftShotTech_UpdateFlash(EftKTask *task);
extern void EftShotTech_UpdateNodeFx(EftKTask *task);
extern void EftShotTech_UpdateBlur(EftKTask *task);
extern void EftShotTech_UpdateSubs(EftKTask *task);
extern EftKHitRec *EftHit_GetNew(void);
extern void EftHit_Add(EftKHitRec *rec);
extern void *EftHitArena_AllocSphere(void);
extern void *EftHitArena_AllocBox(void);
extern void EftHit_SetShapeSpheres(EftKHitRec *rec, void *a, void *b);
extern void EftHit_SetShapeBoxes(EftKHitRec *rec, void *a, void *b);
extern void EftCam_Start(EftKCamArg *arg);
extern void EftCam_Stop(void);
extern f32 EftMath_WrapAngle(f32 angle);
extern void EftAim_GetDir(Vec4 *dir, Vec4 *from, s32 objId);
extern void EftAim_GetDirKeep(EftKSrc *src, Vec4 *dir, Vec4 *from, s32 objId);
extern void EftAim_Home(Vec4 *out, Vec4 *from, Vec4 *dir, s32 objId, f32 speed, f32 homing);
extern void EftGlow_Request(s32 objId, s32 a1);
extern void EftStreak_Stop(void *sub);
extern void EftGndDust_SpawnLandingScaled(s32 objId, Vec4 *pos, Vec4 *dir, f32 a, f32 b);
extern void *EftOrbTail_Create(s32 objId, s32 auraType);
extern void EftOrbTail_Burst(void *fx);
extern void EftOrbTail_Kill(void *fx);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftOrbTail_SetPos(void *fx, PortVec4 pos);
#define EftOrbTail_SetPos(fx, pos) EftOrbTail_SetPos(fx, *(PortVec4 *)(pos))
#else
extern void EftOrbTail_SetPos(void *fx, Vec4 *pos);
#endif
extern void EftMesh_Init(void *ring, s32 *model);
extern void EftMesh_SetTex(void *ring, void *tex);
extern void EftMesh_SetMtx(void *ring, Mtx44 *m);
extern void EftMesh_Copy(void *ring, void *proto);
extern void EftMesh_SetLayer(void *ring, s32 a1);
extern void EftMesh_SetTexBase(void *ring, s32 a1);
extern void EftMesh_SetOwner(void *ring, s32 objId, s32 a2);
extern void EftMesh_Draw(void *ring);
extern s32 EftObj_Create(void *arg, s32 *pack);
extern void EftObj_Destroy(s32 objId);
extern void EftObj_SetMtx(s32 objId, Mtx44 *m);
extern void EftObj_SetVisible(s32 objId, s32 on);
extern void EftObj_Nop(s32 objId, s32 a1);
extern void ColSphere_Set(void *sphere, Vec4 *pos, f32 radius);
extern void ColCapsule_Set(void *box, Vec4 *a, Vec4 *pos, f32 size);

extern const EftKVec D_002ECB60; /* {0, -1, 0, 1}: a local initialiser of EftShotTech_UpdateTargetBurst */
extern s32 gEftTechEvtTaskClass[6];        /* class of the timeline task */

/* ---- "shots" technique module, second half ---- */

/* Events 0x100 / 0x200 of the fighter create / destroy an aura-coloured effect held between nodes 10 and 14. */
void EftShotTech_UpdateAuraBall(EftKTask *task) {
    Vec4 pos;
    Vec4 pos2;
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;

    if (EftShot_TestBits(src->objId, 0x100) && w->auraFx == NULL) {
        void *fx = EftOrbTail_Create(src->objId, BtlCharApi_GetAuraType(src->objId));
        w->auraFx = fx;
        if (fx == NULL) {
            goto end;
        }
        EftOrbTail_Burst(fx);
        EftGlow_Request(src->objId, 0);
    }
    if (w->auraFx != NULL) {
        BtlCharApi_GetNodePos(src->objId, 10, &pos);
        BtlCharApi_GetNodePos(src->objId, 14, &pos2);
        Vec3_Add(&pos, &pos, &pos2);
        Vec3_Scale(&pos, &pos, 0.5f);
        EftOrbTail_SetPos(w->auraFx, &pos);
    }
end:
    if (EftShot_TestBits(src->objId, 0x200) && w->auraFx != NULL) {
        EftOrbTail_Burst(w->auraFx);
        EftOrbTail_Kill(w->auraFx);
        w->auraFx = NULL;
        EftGlow_Request(src->objId, 1);
    }
}

/* With definition flag 0x400: when the opponent gets event 4, starts an upward effect at the opponent's node 3. */
void EftShotTech_UpdateTargetBurst(EftKTask *task) {
    Vec4 pos;
    EftKVec dir;
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);

    if ((src->def->flags & 0x400) && EftShot_TestBits(opp, 4)) {
        dir = D_002ECB60;
        BtlCharApi_GetNodePos(opp, 3, &pos);
        EftGndDust_SpawnLandingScaled(src->objId, &pos, (Vec4 *)&dir, 1.0f, BtlCharApi_GetHeight(opp) * 0.03f);
    }
}

/* On event 0x20, records the direction and position of node 0x36 for shots fired from it. */
void EftShotTech_UpdateMuzzle(EftKTask *task) {
    Mtx44 m;
    Vec4 pos;
    Vec4 dir;
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;

    if ((EftEmit_GetHead26(w->mgr) & 1) && EftShot_TestBits(src->objId, 0x20)) {
        BtlCharApi_GetNodeMtx(src->objId, 0x36, &m);
        Vec4_Set(&dir, -m.m[2][0], -m.m[2][1], -m.m[2][2], 1.0f);
        Vec3_Normalize(&dir, &dir);
        Vec4_Set(&pos, m.m[3][0], m.m[3][1], m.m[3][2], 1.0f);
        Vec4_Copy(&w->muzzleDir, &dir);
        Vec4_Copy(&w->muzzlePos, &pos);
    }
}

/* Once the rush has connected (flag 0x800): starts the effect scene's camera cut for this technique.
   The argument is built in one block and copied to a second one that is passed: the original passed the 12-byte
   struct by value (this ABI passes the address of a copy). */
void EftShotTech_StartCam(EftKTask *task) {
    EftKCamArg copy;
    EftKCamArg arg;
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;

    if (w->flags & 0x800) {
        arg.pack = w->camPack;
        arg.objId = src->objId;
        arg.id = src->def->id;
        copy = arg;
        EftCam_Start(&copy);
        w->flags |= 0x20;
    }
}

/* Aim kind 0: the target point flies on with the velocity (or sits on the opponent's node 3) until the
   opponent's event 0x40. */
void EftShotTech_MoveStraight(EftKTask *task, s32 toTarget) {
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;
    Vec4 *pos = &w->pos;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);

    if (w->phase == 0) {
        BtlCharApi_GetNodePos(src->objId, w->node, &w->start);
        Vec4_Copy(&w->prev, pos);
        if (toTarget == 0) {
            Vec3_Add(pos, pos, &w->vel);
        } else {
            BtlCharApi_GetNodePos(opp, 3, pos);
        }
        if (EftShot_TestBits(opp, 0x40)) {
            w->phase = 1;
        }
    }
}

/* Aim kind 1: flies with the velocity, then (event 0x10) sits at a fixed distance in front of the opponent on the
   line to the fighter; event 0x20 makes it grow to twice the size; the opponent's event 0x40 ends it. */
void EftShotTech_MoveFromTarget(EftKTask *task, s32 wait) {
    Vec4 d;
    Vec4 a;
    Vec4 b;
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);
    f32 dist = EftEmit_GetHead28(w->mgr);

    switch (w->phase) {
    case 0: {
        Vec4 *pos;
        s32 go;

        BtlCharApi_GetNodePos(src->objId, w->node, &w->start);
        pos = &w->pos;
        go = 0;
        Vec4_Copy(&w->prev, pos);
        Vec3_Add(pos, pos, &w->vel);
        if (wait == 0 || EftShot_TestBits(src->objId, 0x10)) {
            go = 1;
        }
        if (go) {
            BtlCharApi_GetNodePos(opp, 0x11, &a);
            BtlCharApi_GetNodePos(src->objId, 0x11, &b);
            Vec3_Sub(&d, &b, &a);
            Vec3_Normalize(&d, &d);
            Vec3_Scale(&d, &d, dist);
            Vec3_Add(&a, &a, &d);
            Vec4_Copy(pos, &a);
            w->phase = 1;
        }
        break;
    }
    case 1:
        BtlCharApi_GetNodePos(opp, 0x11, &a);
        BtlCharApi_GetNodePos(src->objId, 0x11, &b);
        Vec3_Sub(&d, &b, &a);
        Vec3_Normalize(&d, &d);
        Vec3_Scale(&d, &d, dist);
        Vec3_Add(&a, &a, &d);
        Vec4_Copy(&w->pos, &a);
        if (EftShot_TestBits(src->objId, 0x20)) {
            w->flags |= 0x100;
        }
        if (w->flags & 0x100) {
            w->scale += 0.05f;
            if (w->scale > 2.0f) {
                w->scale = 2.0f;
            }
        }
        if (EftShot_TestBits(opp, 0x40)) {
            w->phase = 2;
        }
        break;
    case 2:
        break;
    }
}

/* Aim kind 2: the target point is 30 past the opponent's node 3 on the line from the launch node; after event
   0x10 it is 30 (18 for effect 0x2B4) along the line to the fighter's own node 0x1F. Stores the direction. */
void EftShotTech_MoveAimed(EftKTask *task) {
    Vec4 d;
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);
    f32 dist;

    if (w->phase == 0) {
        Vec4 *start;
        Vec4 *pos;

        if (EftShot_TestBits(src->objId, 0x10)) {
            w->flags |= 0x4000;
        }
        start = &w->start;
        pos = &w->pos;
        BtlCharApi_GetNodePos(src->objId, w->node, start);
        Vec4_Copy(&w->prev, pos);
        if (!(w->flags & 0x4000)) {
            BtlCharApi_GetNodePos(opp, 3, pos);
            Vec3_Sub(&d, pos, start);
            dist = sqrtf(Vec3_Dot(&d, &d)) + 30.0f;
            Vec3_Normalize(&d, &d);
            Vec3_ScaleAdd(pos, &d, dist, start);
        } else {
            dist = 30.0f;
            BtlCharApi_GetNodePos(src->objId, 0x1F, pos);
            Vec3_Sub(&d, pos, start);
            if (src->def->id == 0x2B4) {
                dist = 30.0f * 0.6f;
            }
            Vec3_Normalize(&d, &d);
            Vec3_ScaleAdd(pos, &d, dist, start);
        }
        Vec4_Copy(&w->dir, &d);
    }
}

/* After the rush connected: runs the per-frame effect helpers and, between events 0x10 and 0x20, keeps the model
   object at node 0x36 pointing against the direction. */
void EftShotTech_UpdateConnected(EftKTask *task) {
    Vec4 pos;
    Vec4 ang;
    Vec4 d;
    EftKTask *owner = BtlTask_GetParent(task);
    EftShotTech *w = task->work;
    EftShotTechMgr *mgr = owner->work;
    EftKSrc *src = w->src;

    EftShotTech_UpdateFlash(task);
    EftShotTech_UpdateNodeFx(task);
    EftShotTech_UpdateBlur(task);
    EftShotTech_UpdateSubs(task);
    EftShotTech_UpdateAuraBall(task);
    EftShotTech_UpdateTargetBurst(task);
    EftShotTech_UpdateMuzzle(task);
    if (mgr->modelPack != NULL) {
        if (EftShot_TestBits(src->objId, 0x10)) {
            w->flags |= 0x100000;
        }
        if (EftShot_TestBits(src->objId, 0x20)) {
            w->flags &= ~0x100000;
        }
        if (w->flags & 0x100000) {
            BtlCharApi_GetNodePos(src->objId, 0x36, &pos);
            Vec3_Clamp(&d, &w->dir, -1.0f, 1.0f);
            ang.x = Mathf_Asin(d.y);
            ang.y = atan2f(d.x, d.z);
            ang.x = EftMath_WrapAngle(ang.x);
            ang.y = EftMath_WrapAngle(ang.y);
            ang.z = 0.0f;
            EftShotTech_SetModelPose(task, &pos, &ang);
        }
    }
}

/* Item init: clears the 0x900-byte work, reads speed and scale from the definition and the aim kind from the
   emitter set's header, creates the model when the manager loaded one. */
void EftShotTech_Init(EftKTask *task, EftKSrc *src) {
    EftKTask *owner = BtlTask_GetParent(task);
    EftShotTech *w = task->work;
    EftShotTechMgr *mgr = owner->work;
    EftKDef *def;
    s32 head;

    memset(w, 0, sizeof(EftShotTech));
    w->src = src;
    def = src->def;
    w->speed = def->shotSpeed;
    w->scale = def->scale;
    w->mgr = mgr;
    EftEmit_InitState(mgr, w->emit);
    head = EftEmit_GetHead26(w->mgr);
    if ((EftEmit_GetHead21(w->mgr) & 1) && EftEmit_GetHead22(w->mgr) == 2) {
        w->node = 0x15;
        w->flags |= 0x2000;
    }
    if (head & 0x20) {
        w->flags |= 0x20000;
    }
    if (src->slot >= 2) {
        s32 *pack = mgr->unk318[src->slot];
        if (pack != NULL) {
            w->camPack = pack;
        }
    }
    if (mgr->modelPack != NULL) {
        EftShotTech_InitModel(task);
    }
    BtlTask_SetOwnerTag(task, src->objId == 0 ? 0x800 : 0x1000);
}

/* Item term: destroys the model, the eight sub-effects and the aura effect, frees the emitter state and sets the
   fighter's held flag 0xA8. */
void EftShotTech_Term(EftKTask *task) {
    EftKTask *owner = BtlTask_GetParent(task);
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;
    s32 i;

    if (((EftShotTechMgr *)owner->work)->modelPack != NULL) {
        EftShotTech_FreeModel(task);
    }
    for (i = 0; i < 8; i++) {
        if (w->subs[i] != NULL) {
            EftStreak_Stop(w->subs[i]);
        }
    }
    if (w->auraFx != NULL) {
        EftOrbTail_Kill(w->auraFx);
        w->auraFx = NULL;
        EftGlow_Request(src->objId, 2);
    }
    EftEmit_TermState(w->mgr, w->emit);
    EftShot_SetHeldFlagA8(src->objId);
}

/* Item update: the state machine driven by the fighter's technique events (2 start, 4 fire, 8 end, 0x400 abort). */
void EftShotTech_Update(EftKTask *task) {
    Vec4 tmp;
    s32 advance = 0;
    s32 busy = 0;
    EftShotTech *w = task->work;
    EftKSrc *src = w->src;
    s32 head;

    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    if (BtlCharApi_IsRushConnected(src->objId)) {
        w->flags |= 0x800;
    }
    if (!(w->flags & 0x20)) {
        EftShotTech_StartCam(task);
    }
    head = EftEmit_GetHead26(w->mgr);
    EftEmit_UpdateNodesReq(src, w->nodes);
    if (!(w->flags & 0x1000)) {
        if (EftShot_TestBits(src->objId, 2)) {
            EftShotTech_Start(task, 2);
            task->state = 0;
            w->flags |= 0x1000;
        } else if (EftShot_TestBits(src->objId, 4)) {
            EftShotTech_Start(task, 4);
            task->state = 2;
            w->flags |= 0x1000;
        } else if (head & 4) {
            if (EftShot_TestBits(src->objId, 0x10)) {
                EftShotTech_Start(task, 0x10);
                task->state = 2;
                w->flags |= 0x1000;
            }
        }
    }
    switch (task->state) {
    case 0:
        if (EftShot_TestBits(src->objId, 4)) {
            EftShotTech_Start(task, 4);
            task->state = 1;
        }
        break;
    case 1:
        advance = 1;
        break;
    case 2:
        if (src->def->flags & 0x10) {
            Vec4_Scale(&tmp, &w->dir, -1.0f);
        }
        if (EftEmit_GetHead21(w->mgr) & 1) {
            s32 kind = EftEmit_GetHead22(w->mgr);
            u8 toTarget = (head & 2) > 0;
            u8 wait = (head & 8) > 0;

            switch (kind) {
            case 0:
                EftShotTech_MoveStraight(task, toTarget);
                break;
            case 1:
                EftShotTech_MoveFromTarget(task, wait);
                break;
            case 2:
                EftShotTech_MoveAimed(task);
                break;
            }
        }
        if (EftShot_TestBits(src->objId, 8)) {
            w->flags |= 9;
            task->state = 3;
        }
        break;
    case 3:
        advance = 1;
        break;
    }
    if (w->flags & 0x800) {
        if (head & 0x10) {
            BtlCharApi_GetNodePos(src->objId, 0x36, &w->pos);
        }
        if (head & 0x40) {
            EftShotTech_CalcDir(task, &w->dir);
        }
        if (w->flags & 0x800) {
            EftShotTech_UpdateConnected(task);
        }
    }
    if (w->flags & 0x20000) {
        busy = EftShotTech_UpdateShots(src->objId, task);
    }
    if (EftShot_TestBits(src->objId, 0x400)) {
        if (!(w->flags & 8)) {
            w->flags |= 4;
        }
        w->flags |= 1;
    }
    EftShotTech_UpdateParts(src->objId, task, w->mgr);
    if (advance) {
        task->state++;
    }
    if (!busy && (w->flags & 2)) {
        BtlTask_SetDead(task);
        return;
    }
    if (w->flags & 1) {
        if (w->flags & 0x20) {
            if (!(w->flags & 0x40)) {
                EftCam_Stop();
                w->flags |= 0x40;
            }
        }
        w->flags |= 2;
    }
}

/* Item post-update: steps the emitters and the shots unless time is stopped for the fighter. */
void EftShotTech_PostUpdate(EftKTask *task) {
    EftShotTech *w = task->work;

    if (!BtlScene_IsCharStopped(w->src->objId)) {
        EftEmit_UpdateAlive(w->mgr, w->emit);
        EftShotTech_ReapShots(task);
    }
}

/* Item reset: the task dies. */
void EftShotTech_Reset(EftKTask *task) {
    BtlTask_SetDead(task);
}

/* Item draw: nothing. */
void EftShotTech_Draw(EftKTask *task) {
}

/* Manager init: loads the emitter set from the effect's pack, creates the list of two items, and looks up the
   three camera packs of the character's pack 8 and the model. */
void EftShotTechMgr_Init(EftKTask *task, EftKSrc *src) {
    EftShotTechMgr *mgr = task->work;
    s32 i;

    EftShot_Nop(sizeof(EftShotTechMgr));
    memset(mgr, 0, sizeof(EftShotTechMgr));
    EftEmit_LoadSet(src, mgr, 0, src->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, sizeof(EftShotTech));
    for (i = 0; i < 3; i++) {
        s32 *pack = BtlScene_GetCharPackEntry(src->chr, 8);
        if (pack[i + 2] != pack[i + 1]) {
            mgr->packs[i] = BtlScene_GetPackEntry(pack, i + 1);
        }
    }
    if (src->def->flags & 0x20000) {
        mgr->modelPack = BtlScene_GetPackEntry(src->pack, 1);
    }
}

/* Manager term: frees the emitter set. */
void EftShotTechMgr_Term(EftKTask *task) {
    EftEmit_FreeSet(task->work);
}

/* Manager update: per-frame start of the emitter set. */
void EftShotTechMgr_Update(EftKTask *task) {
    EftEmit_BeginFrame(task->work);
}

/* Manager reset: nothing. */
void EftShotTechMgr_Reset(EftKTask *task) {
}

/* ---- technique events ---- */

/* Event bits of a fighter this frame (0 when the layer does not exist). */
s32 EftTechEvt_GetEvents(s32 objId) {
    EftTechEvt *evt = gEftTechEvt;
    EftTechEvtEntry *e;

    if (evt == NULL) {
        return 0;
    }
    e = &evt->entries[objId];
    return e->events;
}

/* Starts the timeline of a technique: one task that turns the fighter's animation into event bits. */
void *EftTechEvt_Start(EftKSrc *src, s32 flagTime, s32 endTime) {
    EftTechEvtArg arg;

    if (gEftTechEvt == NULL) {
        return NULL;
    }
    memset(&arg, 0, sizeof(arg));
    arg.src = src;
    arg.fireTimer = flagTime;
    arg.endTimer = endTime - 1;
    arg.endTime = endTime - 1;
    return BtlTaskList_AddTail(gEftTechEvt->list, gEftTechEvtTaskClass, &arg);
}

/* The fighter's timers stop for good (the projectile hit); a pending END event is withdrawn. */
void EftTechEvt_RequestStop(s32 objId) {
    EftTechEvtEntry *e = &gEftTechEvt->entries[objId];

    e->requests |= EFT_TECH_REQ_STOP;
    if (e->events & EFT_TECH_EVT_END) {
        e->events &= ~EFT_TECH_EVT_END;
    }
}

/* Restarts the fighter's end timer, unless the END event is already out. */
void EftTechEvt_RequestRestart(s32 objId) {
    EftTechEvtEntry *e = &gEftTechEvt->entries[objId];

    if (!(e->events & EFT_TECH_EVT_END)) {
        e->requests |= EFT_TECH_REQ_RESTART;
    }
}

/* Makes the fighter's end timer run out, unless the END event is already out. */
void EftTechEvt_RequestExpire(s32 objId) {
    EftTechEvtEntry *e = &gEftTechEvt->entries[objId];

    if (!(e->events & EFT_TECH_EVT_END)) {
        e->requests |= EFT_TECH_REQ_EXPIRE;
    }
}

/* Layer init: allocates the state and one entry per fighter, creates the list of timeline tasks. */
void EftTechEvt_Init(EftKTask *task) {
    s32 size;

    gEftTechEvt = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftTechEvt));
    memset(gEftTechEvt, 0, sizeof(EftTechEvt));
    gEftTechEvt->count = BtlScene_GetCharCount();
    size = gEftTechEvt->count * sizeof(EftTechEvtEntry);
    gEftTechEvt->entries = BtlPool_Alloc(BtlPool_GetCurrent(), size);
    memset(gEftTechEvt->entries, 0, size);
    gEftTechEvt->list = BtlTask_CreateChildList(task, gEftTechEvt->count, sizeof(EftTechEvtTask));
}

/* Layer term. */
void EftTechEvt_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftTechEvt->entries);
    BtlPool_Free(BtlPool_GetCurrent(), gEftTechEvt);
    gEftTechEvt = NULL;
}

/* Layer update (before its timeline tasks): clears every fighter's event bits. */
void EftTechEvt_Update(void) {
    s32 i;

    for (i = 0; i < gEftTechEvt->count; i++) {
        EftTechEvtEntry *e = &gEftTechEvt->entries[i];
        e->events = 0;
    }
}

/* Timeline init: keeps the argument; a kind-0 definition with any frame number uses the frame table. */
void EftTechEvtTask_Init(EftKTask *task, EftTechEvtArg *arg) {
    EftTechEvtTask *w = task->work;
    EftKSrc *src = arg->src;
    EftKDef *def = src->def;
    EftTechEvtEntry *e;
    s32 i;

    memset(w, 0, sizeof(EftTechEvtTask));
    w->arg = *arg;
    w->useFrames = 0;
    if (def->kind == 0) {
        for (i = 0; i < 6; i++) {
            if (def->frames[i] != -1) {
                w->useFrames = 1;
            }
        }
    }
    e = &gEftTechEvt->entries[src->objId];
    e->requests = 0;
}

/* Timeline term: nothing. */
void EftTechEvtTask_Term(EftKTask *task) {
}

/* Timeline update: produces this frame's events unless time is stopped for the fighter.
   Declared with a result it never sets: the original calls the two updaters with jal and falls into the epilogue,
   which this compiler only does in a non-void function (a void one gets two tail calls). */
s32 EftTechEvtTask_Update(EftKTask *task) {
    EftTechEvtTask *w = task->work;
    EftKSrc *src = w->arg.src;

    w->flags &= ~0x10;
    if (!BtlScene_IsCharStopped(src->objId)) {
        if (EftTechEvt_IsRushTech(src)) {
            EftTechEvtTask_UpdateRush(task);
        } else {
            EftTechEvtTask_UpdateNormal(task);
        }
    }
}

/* Timeline reset: the task dies. */
void EftTechEvtTask_Reset(EftKTask *task) {
    BtlTask_SetDead(task);
}

/* Timeline draw callback, first view of the frame only: applies the requests and steps the two timers. When the
   flag timer runs out the fighter's held flag 0xA7 is set. */
void EftTechEvtTask_Draw(EftKTask *task) {
    EftTechEvtTask *w = task->work;
    EftKSrc *src = w->arg.src;
    EftTechEvtEntry *e = &gEftTechEvt->entries[src->objId];

    if (w->flags & 0x10) {
        return;
    }
    w->flags |= 0x10;
    if (EftTechEvt_IsRushTech(src)) {
        return;
    }
    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    if (e->requests & EFT_TECH_REQ_STOP) {
        return;
    }
    if (e->requests & EFT_TECH_REQ_RESTART) {
        w->arg.endTimer = w->arg.endTime;
        e->requests &= ~EFT_TECH_REQ_RESTART;
    }
    if (e->requests & EFT_TECH_REQ_EXPIRE) {
        w->arg.endTimer = 0;
        e->requests &= ~EFT_TECH_REQ_EXPIRE;
    }
    if (w->flags & 2) {
        if (--w->arg.fireTimer <= 0) {
            BtlCharApi_SetHeldFlagA7(src->objId);
            w->flags &= ~2;
        }
    }
    if (w->flags & 4) {
        if (--w->arg.endTimer <= 0) {
            w->arg.endTimer = 0;
            w->flags &= ~4;
        }
    }
}

/* Events of an ordinary technique or skill: its own animation attributes (or frame table), and ABORT when the
   fighter is interrupted or leaves the technique / skill. The task dies on END or ABORT. */
void EftTechEvtTask_UpdateNormal(EftKTask *task) {
    EftTechEvtTask *w = task->work;
    EftKSrc *src = w->arg.src;
    EftTechEvtEntry *e = &gEftTechEvt->entries[src->objId];
    u64 ev = EftTechEvtTask_CollectEvents(w);

    w->frame++;
    if (ev & 0x200) {
        e->events |= EFT_TECH_EVT_START;
        w->flags |= 2;
    }
    if (ev & 0x400) {
        e->events |= EFT_TECH_EVT_FIRE;
        if (src->def->kind != 0) {
            w->flags |= 4;
        } else {
            w->flags |= 0x20;
        }
    }
    if (ev & 0x800) {
        e->events |= EFT_TECH_EVT_END;
        BtlTask_SetDead(task);
        return;
    }
    if (src->def->kind == 0) {
        if (EftTechEvt_IsInterrupted(src->objId)) {
            e->events |= EFT_TECH_EVT_ABORT;
            BtlTask_SetDead(task);
            return;
        }
        BtlCharApi_IsSkillApplied(src->objId);
        if (!EftTechEvt_IsInSkill(src->objId)) {
            if (w->flags & 0x20) {
                e->events |= EFT_TECH_EVT_END;
            } else {
                e->events |= EFT_TECH_EVT_ABORT;
            }
            BtlTask_SetDead(task);
            return;
        }
    } else {
        if (!EftTechEvt_IsInTechnique(src->objId)) {
            e->events |= EFT_TECH_EVT_ABORT;
            BtlTask_SetDead(task);
            return;
        }
        if (EftTechEvt_IsInterrupted(src->objId)) {
            e->events |= EFT_TECH_EVT_ABORT;
            BtlTask_SetDead(task);
            return;
        }
    }
    if (ev & 0x1000) {
        e->events |= EFT_TECH_EVT_10;
    }
    if (ev & 0x2000) {
        e->events |= EFT_TECH_EVT_20;
    }
    if (ev & 0x4000) {
        e->events |= EFT_TECH_EVT_40;
    }
    if (ev & 0x200000) {
        e->events |= EFT_TECH_EVT_80;
    }
    if (ev & 0x400000) {
        e->events |= EFT_TECH_EVT_100;
    }
    if (ev & 0x800000) {
        e->events |= EFT_TECH_EVT_200;
    }
    if (w->arg.endTimer <= 0) {
        e->events |= EFT_TECH_EVT_END;
    }
}

/* Events of a rush technique: the animation attributes of EVERY fighter become that fighter's events (the victim
   plays its own animation). ABORT when the attacker leaves the technique or the rush sequence. */
void EftTechEvtTask_UpdateRush(EftKTask *task) {
    EftTechEvtTask *w = task->work;
    EftKSrc *src = w->arg.src;
    EftTechEvtEntry *e;
    s32 i;

    if (BtlCharApi_IsRushConnected(src->objId)) {
        w->flags |= 8;
    }
    if (w->flags & 8) {
        w->frame++;
    }
    if (!BtlCharApi_IsInTechnique(src->objId)) {
        e = &gEftTechEvt->entries[src->objId];
        e->events |= EFT_TECH_EVT_ABORT;
        BtlTask_SetDead(task);
        return;
    }
    if ((w->flags & 8) && !BtlCharApi_IsInRushSequence(src->objId)) {
        e = &gEftTechEvt->entries[src->objId];
        e->events |= EFT_TECH_EVT_ABORT;
        BtlTask_SetDead(task);
        return;
    }
    if (BtlCharApi_ObjTestAttr(src->objId, 0x800)) {
        e = &gEftTechEvt->entries[src->objId];
        e->events |= EFT_TECH_EVT_END;
        BtlTask_SetDead(task);
        return;
    }
    for (i = 0; i < gEftTechEvt->count; i++) {
        e = &gEftTechEvt->entries[i];
        if (BtlCharApi_ObjTestAttr(i, 0x200)) {
            e->events |= EFT_TECH_EVT_START;
            w->flags |= 2;
        }
        if (BtlCharApi_ObjTestAttr(i, 0x400)) {
            e->events |= EFT_TECH_EVT_FIRE;
            w->flags |= 4;
        }
        if (BtlCharApi_ObjTestAttr(i, 0x1000)) {
            e->events |= EFT_TECH_EVT_10;
        }
        if (BtlCharApi_ObjTestAttr(i, 0x2000)) {
            e->events |= EFT_TECH_EVT_20;
        }
        if (BtlCharApi_ObjTestAttr(i, 0x4000)) {
            e->events |= EFT_TECH_EVT_40;
        }
        if (BtlCharApi_ObjTestAttr(i, 0x200000)) {
            e->events |= EFT_TECH_EVT_80;
        }
        if (BtlCharApi_ObjTestAttr(i, 0x400000)) {
            e->events |= EFT_TECH_EVT_100;
        }
        if (BtlCharApi_ObjTestAttr(i, 0x800000)) {
            e->events |= EFT_TECH_EVT_200;
        }
    }
}

/* 1 for a rush technique: kind not 0 and either sub-kind 5 or the fighter is in the rush sequence. */
s32 EftTechEvt_IsRushTech(EftKSrc *src) {
    EftKDef *def = src->def;

    if (def != NULL) {
        if (def->kind != 0) {
            if (def->sub == 5) {
                return 1;
            }
            if (BtlCharApi_IsInRushSequence(src->chr)) {
                return 1;
            }
        }
    }
    return 0;
}

/* This frame's raw events of the fighter, as animation attribute bits. */
u64 EftTechEvtTask_CollectEvents(EftTechEvtTask *w) {
    u64 ev = 0;
    EftKSrc *src = w->arg.src;
    EftKDef *def = src->def;
    s32 frame;

    if (w->useFrames == 0) {
        if (BtlCharApi_ObjTestAttr(src->objId, 0x200)) {
            ev |= 0x200;
        }
        if (BtlCharApi_ObjTestAttr(src->objId, 0x400)) {
            ev |= 0x400;
        }
        if (BtlCharApi_ObjTestAttr(src->objId, 0x800)) {
            ev |= 0x800;
        }
        if (BtlCharApi_ObjTestAttr(src->objId, 0x1000)) {
            ev |= 0x1000;
        }
        if (BtlCharApi_ObjTestAttr(src->objId, 0x2000)) {
            ev |= 0x2000;
        }
        if (BtlCharApi_ObjTestAttr(src->objId, 0x4000)) {
            ev |= 0x4000;
        }
    } else {
        frame = w->frame;
        if (frame == def->frames[0]) {
            ev |= 0x200;
        }
        if (frame == def->frames[1]) {
            ev |= 0x400;
        }
        if (frame == def->frames[2]) {
            ev |= 0x800;
        }
        if (frame == def->frames[3]) {
            ev |= 0x1000;
        }
        if (frame == def->frames[4]) {
            ev |= 0x2000;
        }
        if (frame == def->frames[5]) {
            ev |= 0x4000;
        }
    }
    if (BtlCharApi_ObjTestAttr(src->objId, 0x200000)) {
        ev |= 0x200000;
    }
    if (BtlCharApi_ObjTestAttr(src->objId, 0x400000)) {
        ev |= 0x400000;
    }
    if (BtlCharApi_ObjTestAttr(src->objId, 0x800000)) {
        ev |= 0x800000;
    }
    return ev;
}

/* The fighter's animation has flag 0x10 (BtlCharApi_TestAnimFlag10). */
s32 EftTechEvt_IsInterrupted(s32 objId) {
    return BtlCharApi_TestAnimFlag10(objId);
}

/* The fighter is in a skill action. */
s32 EftTechEvt_IsInSkill(s32 objId) {
    return BtlCharApi_IsInSkill(objId);
}

/* The fighter is in a technique action. */
s32 EftTechEvt_IsInTechnique(s32 objId) {
    return BtlCharApi_IsInTechnique(objId);
}

/* ---- "thrown object" technique module ---- */

/* Adds this frame's hit record: the body (or, on the target, the held block) with two spheres or two boxes
   scaled by the emitter's width. */
void EftObjTech_AddHitRecord(EftKTask *task) {
    EftObjTech *w = task->work;
    EftKHitRec *rec = EftHit_GetNew();
    f32 size = w->hitScale * EftEmit_GetTrailWidth(w->emit);
    EftKShape *shape = &w->body;

    if (!(w->flags & 0x800)) {
        if (w->mode == 0) {
            rec->flags |= 0x800;
        }
    } else {
        rec->flags |= 0x10;
        shape = &w->held;
    }
    rec->shape = *shape;
    rec->task = task;
    rec->src = w->src;
    switch (w->src->def->shape) {
    case 1: {
        void *a = EftHitArena_AllocBox();
        void *b = EftHitArena_AllocBox();

        ColCapsule_Set(a, &w->unk3C0, (Vec4 *)&shape->pos, size);
        ColCapsule_Set(b, &w->unk3C0, (Vec4 *)&shape->prev, size);
        EftHit_SetShapeBoxes(rec, a, b);
        break;
    }
    case 0: {
        void *a = EftHitArena_AllocSphere();
        void *b = EftHitArena_AllocSphere();

        ColSphere_Set(a, (Vec4 *)&shape->pos, size);
        ColSphere_Set(b, (Vec4 *)&shape->prev, size);
        EftHit_SetShapeSpheres(rec, a, b);
        break;
    }
    default:
        return;
    }
    EftHit_Add(rec);
}

/* Creates the model object from the manager's model pack, hidden. */
void EftObjTech_CreateModel(EftKTask *task) {
    EftKTask *owner = BtlTask_GetParent(task);
    EftObjTech *w = task->work;
    EftKModel *model = &w->model;

    model->pack = ((EftObjTechMgr *)owner->work)->modelPack;
    model->objId = EftObj_Create(model->arg, model->pack);
    model->unk36 = -1;
    EftObj_SetVisible(model->objId, 0);
    EftObj_Nop(model->objId, 0);
}

/* Places the model: scale, Euler rotation, translation. */
void EftObjTech_SetModelPose(EftKTask *task, Vec4 *pos, Vec4 *angles) {
    Mtx44 m;
    EftObjTech *w = task->work;
    EftKModel *model;

    Mtx_StoreIdentity(&m);
    Mtx_ScaleDiagUniform(&m, &m, w->scale);
    model = &w->model;
    Mtx_RotateZXY(&m, &m, angles);
    Mtx_Translate(&m, &m, pos);
    EftObj_SetMtx(model->objId, &m);
}

/* Destroys the model object. */
void EftObjTech_DestroyModel(EftKTask *task) {
    EftObj_Destroy(((EftObjTech *)task->work)->model.objId);
}

/* Sets up the rings from the manager's prototype and gives each its roll, its offset (step apart) and its
   scale (1, growing by grow). */
void EftObjTech_InitRings(EftKTask *task, f32 roll, f32 step, f32 grow) {
    s32 i;
    EftKTask *owner = BtlTask_GetParent(task);
    EftObjTech *w = task->work;
    EftObjTechMgr *mgr = owner->work;
    EftKRings *rings = &w->rings;
    EftKSrc *src = w->src;
    f32 offset = 0.0f;
    f32 scale = 1.0f;

    for (i = 0; i < rings->count; i++) {
        EftMesh_Copy(rings->ring[i], mgr->ringProto);
        EftMesh_SetTex(rings->ring[i], mgr->ringTexSet);
        EftMesh_SetLayer(rings->ring[i], 1);
        EftMesh_SetTexBase(rings->ring[i], 0);
        EftMesh_SetOwner(rings->ring[i], src->objId, 1);
        rings->roll[i] = roll;
        rings->offset[i] = offset;
        offset += step;
        rings->scale[i] = scale;
        scale += grow;
    }
}

/* The same parameters without touching the ring objects. */
void EftObjTech_SetRingParams(EftKTask *task, f32 roll, f32 step, f32 grow) {
    s32 i;
    EftObjTech *w = task->work;
    EftKRings *rings = &w->rings;
    f32 offset = 0.0f;
    f32 scale = 1.0f;

    for (i = 0; i < rings->count; i++) {
        rings->roll[i] = roll;
        rings->offset[i] = offset;
        offset += step;
        rings->scale[i] = scale;
        scale += grow;
    }
}

/* Builds each ring's matrix: scaled (width on x and z, length on y), rotated by the angles with the ring's roll
   in x, placed at pos + direction * offset. */
void EftObjTech_PlaceRings(EftKTask *task, Vec4 *pos, Vec4 *angles, f32 width, f32 length) {
    Mtx44 m;
    Vec4 p;
    s32 i;
    EftObjTech *w = task->work;
    EftKRings *rings = &w->rings;

    for (i = 0; i < rings->count; i++) {
        Mtx_StoreIdentity(&m);
        m.m[0][0] *= width * rings->scale[i];
        m.m[1][1] *= length;
        m.m[2][2] *= width * rings->scale[i];
        angles->x = rings->roll[i];
        angles->x = EftMath_WrapAngle(angles->x);
        Mtx_RotateZXY(&m, &m, angles);
        Vec3_Scale(&p, &w->dir, rings->offset[i]);
        Vec3_Add(&p, &p, pos);
        p.w = 1.0f;
        Mtx_Translate(&m, &m, &p);
        EftMesh_SetMtx(rings->ring[i], &m);
    }
}

/* Draws the rings. */
void EftObjTech_DrawRings(EftKTask *task) {
    s32 i;
    EftObjTech *w = task->work;
    EftKRings *rings = &w->rings;

    for (i = 0; i < rings->count; i++) {
        EftMesh_Draw(rings->ring[i]);
    }
}

/* Nothing to free for the rings. */
void EftObjTech_TermRings(EftKTask *task) {
}

/* Runs the 19 emitter groups of the set: asks for each part what to do this frame (flag 1 = the object is ending,
   flag 8 = it was cut short) and starts or stops it at the body's head. Once the object has hit (0x400) the parts
   move to the position on the target, and a part with a life (unk18 > 0) is left to run out.
   endOnTarget, onTarget, objId and a loop pointer live on the stack in the original: the function uses all nine
   saved registers. */
void EftObjTech_UpdateEmitters(s32 objId, EftKTask *task, EftKSet *set) {
    EftObjTech *w = task->work;
    Vec4 pos;
    s32 endOnTarget = 0;
    s32 onTarget = 0;
    s32 end;
    s32 fast;
    s32 g;
    s32 i;

    Vec4_Copy(&pos, (Vec4 *)&w->body.pos);
    fast = w->flags & 8;
    if (!(w->flags & 0x400)) {
        end = w->flags & 1;
    } else if (!(w->flags & 0x800)) {
        end = w->flags & 1;
        w->flags |= 0x800;
    } else {
        Vec4_Copy(&pos, (Vec4 *)&w->held.pos);
        end = w->flags & 2;
        onTarget = 1;
        endOnTarget = end;
    }
    for (g = 0; g < 19; g++) {
        if (*set->mask & (1 << g)) {
            u8 *info = set->grp[g].info;

            for (i = 0; i < info[1]; i++) {
                s32 idx = set->grp[g].base + i;
                s32 res;

                if (onTarget) {
                    end = endOnTarget;
                    if (set->parts[idx].unk9 == 0 && set->parts[idx].unk18 > 0.0f) {
                        end = 0;
                        fast = 0;
                    }
                }
                res = EftEmit_GetFlagsFromReq(set, w->emit, objId, g, i, end, fast);
                if (res != 0) {
                    EftEmit_SpawnOwn(set, w->emit, w->nodes, &pos, w, g, i, res, w->scale);
                }
            }
        }
    }
}

/* Moves the object and its model according to the mode and the fighter's technique events. */
void EftObjTech_UpdateMotion(EftKTask *task) {
    EftKTask *owner = BtlTask_GetParent(task);
    EftObjTech *w = task->work;
    EftObjTechMgr *mgr = owner->work;
    EftKSrc *src = w->src;

    switch (w->mode) {
    case 0: {
        Vec4 *pos;
        s32 taskFlags;

        if (EftShot_TestBits(src->objId, 0x10)) {
            Vec4 *start = (Vec4 *)&w->held.unk0;

            BtlCharApi_GetNodePos(src->objId, w->node, start);
            pos = (Vec4 *)&w->held.pos;
            Vec4_Copy(pos, start);
            EftAim_GetDir(&w->dir, pos, src->objId);
            Vec3_Scale((Vec4 *)&w->held.vel, &w->dir, w->speed);
            w->flags |= 0x1000;
        } else {
            pos = (Vec4 *)&w->held.pos;
        }
        if (EftShot_TestBits(src->objId, 0x20)) {
            w->speed = src->def->speed;
            EftAim_GetDir(&w->dir, pos, src->objId);
            Vec3_Scale((Vec4 *)&w->held.vel, &w->dir, w->speed);
            w->flags |= 0x20;
            taskFlags = task->flags;
            taskFlags &= ~1;
            taskFlags &= ~4;
            taskFlags &= ~0x200;
            task->flags = taskFlags;
            w->flags |= 0x2000;
        }
        Vec4_Copy((Vec4 *)&w->held.prev, pos);
        if (w->flags & 0x2000) {
            Vec3_Add(pos, pos, (Vec4 *)&w->held.vel);
        } else if (w->flags & 0x1000) {
            BtlCharApi_GetNodePos(src->objId, w->node, pos);
        }
        if (mgr->modelPack != NULL && (w->flags & 0x1000)) {
            Vec4 ang;
            Vec4 d;

            Vec3_Clamp(&d, &w->dir, -1.0f, 1.0f);
            ang.x = Mathf_Asin(d.y);
            ang.y = atan2f(d.x, d.z) + 3.14159265f;
            ang.x = EftMath_WrapAngle(ang.x);
            ang.y = EftMath_WrapAngle(ang.y);
            ang.z = 0.0f;
            EftObjTech_SetModelPose(task, pos, &ang);
        }
        break;
    }
    case 1:
        if (w->flags & 0x400) {
            BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(src->objId), 3, (Vec4 *)&w->held.pos);
        }
        break;
    case 2:
        if (EftShot_TestBits(src->objId, 4)) {
            w->flags |= 0x4000;
        }
        if (w->flags & 0x4000) {
            s32 id = src->objId;

            if (!(w->flags & 0x400)) {
                BtlCharApi_GetNodePos(id, 0x1F, (Vec4 *)&w->body.pos);
            } else {
                id = BtlCharApi_GetOpponentObjId(id);
                BtlCharApi_GetNodePos(id, 0x1F, (Vec4 *)&w->held.pos);
            }
            if (mgr->modelPack != NULL) {
                Vec4 ang;
                Vec4 x;
                Mtx44 m;
                Vec4 a;
                Vec4 b;
                Vec4 y;
                Vec4 z;

                BtlCharApi_GetNodePos(id, 0x15, &a);
                BtlCharApi_GetNodePos(id, 0x19, &b);
                Vec3_Sub(&z, &b, &a);
                Vec3_Normalize(&z, &z);
                BtlCharApi_GetNodePos(id, 0x1F, &b);
                Vec3_Sub(&x, &b, &a);
                Vec3_Normalize(&x, &x);
                Vec3_Cross(&y, &z, &x);
                Vec3_Cross(&y, &z, &y);
                Vec3_Normalize(&y, &y);
                Vec3_Copy((Vec4 *)m.m[0], &x);
                Vec3_Copy((Vec4 *)m.m[1], &y);
                Vec3_Copy((Vec4 *)m.m[2], &z);
                Mtx_AxisZToEuler(&ang, &m);
                BtlCharApi_GetNodePos(id, 0x1F, &a);
                EftObjTech_SetModelPose(task, &a, &ang);
            }
        }
        break;
    case 3:
        if (w->flags & 0x400) {
            BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(src->objId), 0x11, (Vec4 *)&w->held.pos);
            Vec4_Set(&w->dir, 0.0f, -1.0f, 0.0f, 1.0f);
            if (!(w->flags & 0x800)) {
                if (src->def->flags & 0x40000) {
                    f32 step = 0.0f;

                    if (src->def->id == 0x168) {
                        step = BtlScene_GetCharScale(BtlCharApi_GetOpponentObjId(src->objId)) * -4.0f;
                    }
                    EftObjTech_SetRingParams(task, 0.0f, step, 0.0f);
                }
            }
        }
        if (EftShot_TestBits(src->objId, 2)) {
            if (w->flags & 0x8000) {
                w->flags |= 0x4000;
            }
        }
        if (EftShot_TestBits(src->objId, 4)) {
            w->flags |= 0x14000;
        }
        if (EftShot_TestBits(src->objId, 0x10)) {
            w->flags |= 0x20000;
        }
        EftShot_TestBits(src->objId, 0x20);
        if ((src->def->flags & 0x40000) && (w->flags & 0x4000)) {
            Vec4 pos;
            Vec4 ang;
            Vec4 d;
            f32 width;
            f32 length;

            memset(&pos, 0, sizeof(pos));
            memset(&ang, 0, sizeof(ang));
            memset(&d, 0, sizeof(d));
            length = w->scale + w->scale;
            width = w->scale * 1.5f;
            if (w->flags & 0x400) {
                s32 opp = BtlCharApi_GetOpponentObjId(src->objId);

                if (!(w->flags & 0x20000)) {
                    width *= BtlScene_GetCharScale(opp) * 0.8f;
                    length *= BtlScene_GetCharScale(opp) * 0.8f;
                } else {
                    width *= BtlScene_GetCharScale(opp) * 0.5f;
                    length *= BtlScene_GetCharScale(opp) * 0.5f;
                }
                Vec4_Copy(&pos, (Vec4 *)&w->held.pos);
            } else {
                Vec3_Clamp(&d, &w->dir, -1.0f, 1.0f);
                ang.x = Mathf_Asin(-d.y);
                ang.y = atan2f(d.x, d.z);
                ang.x = EftMath_WrapAngle(ang.x);
                ang.y = EftMath_WrapAngle(ang.y);
                ang.z = 0.0f;
                if (!(w->flags & 0x10000)) {
                    ang.x = 0.0f;
                    Vec4_Copy(&pos, &w->unk380);
                } else {
                    width += width;
                    length += length;
                    Vec4_Copy(&pos, (Vec4 *)&w->body.pos);
                }
            }
            EftObjTech_PlaceRings(task, &pos, &ang, width, length);
        }
        break;
    }
}

/* Shows the model while it is in use (hidden when the effect scene hides effects of this fighter). */
void EftObjTech_UpdateModelVisible(EftKTask *task) {
    EftKTask *owner = BtlTask_GetParent(task);
    EftObjTech *w = task->work;
    EftObjTechMgr *mgr = owner->work;
    s32 show = 0;
    EftKModel *model = &w->model;
    EftKSrc *src = w->src;

    switch (w->mode) {
    case 0:
        if (mgr->modelPack != NULL) {
            if (w->flags & 0x1000) {
                show = 1;
            }
        }
        break;
    case 1:
        break;
    case 2:
        if (w->flags & 0x4000) {
            show = mgr->modelPack != NULL;
        }
        break;
    }
    if (show) {
        if (!BtlScene_IsEffectHidden(src->objId, 1)) {
            EftObj_SetVisible(model->objId, 1);
        } else {
            EftObj_SetVisible(model->objId, 0);
        }
    }
}

/* Item init: clears the 0x800-byte work, reads speed and scale from the definition, picks the mode from the
   effect id, creates the model and the rings. */
void EftObjTech_Init(EftKTask *task, EftKSrc *src) {
    EftKTask *owner = BtlTask_GetParent(task);
    EftObjTech *w = task->work;
    EftObjTechMgr *mgr = owner->work;
    void *emit;
    EftKDef *def;

    memset(w, 0, sizeof(EftObjTech));
    emit = w->emit;
    w->src = src;
    def = src->def;
    w->speed = def->speed;
    w->unk5C0 = def->scale;
    w->scale = def->scale;
    w->hitScale = w->unk5C0;
    w->mgr = mgr;
    EftEmit_InitState(mgr, emit);
    if (EftEmit_HasWidth2(w->mgr)) {
        w->speed = EftEmit_GetWidth2(emit) * def->scale;
        w->flags |= 0x200;
    }
    w->life = EftEmit_GetEndFrames(w->mgr);
    if (def->id == 0x282) {
        w->mode = 0;
    } else if (def->id == 0x2A8) {
        w->mode = 1;
    } else if (def->id == 0x162) {
        w->mode = 2;
    } else if (def->id == 0x1F3) {
        w->mode = 0;
    } else if (def->id == 0x167) {
        w->mode = 3;
        w->rings.count = 1;
        w->flags |= 0x8000;
    } else if (def->id == 0x168) {
        w->mode = 3;
        w->rings.count = 3;
    }
    if (mgr->modelPack != NULL) {
        EftObjTech_CreateModel(task);
    }
    if (src->def->flags & 0x40000) {
        f32 roll = 0.0f;
        f32 step = 0.0f;
        f32 grow = 0.0f;

        if (def->id == 0x168) {
            roll = -1.5707963f;
            step = -20.0f;
            grow = 0.3f;
        }
        EftObjTech_InitRings(task, roll, step, grow);
    }
    BtlTask_SetOwnerTag(task, src->objId == 0 ? 0x800 : 0x1000);
}

/* Item term: destroys the model, frees the emitter state; a technique (kind not 0) sets held flag 0xA8. */
void EftObjTech_Term(EftKTask *task) {
    EftKTask *owner = BtlTask_GetParent(task);
    EftObjTech *w = task->work;
    EftKSrc *src = w->src;

    if (((EftObjTechMgr *)owner->work)->modelPack != NULL) {
        EftObjTech_DestroyModel(task);
    }
    if (src->def->flags & 0x40000) {
        EftObjTech_TermRings(task);
    }
    EftEmit_TermState(w->mgr, w->emit);
    if (src->def->kind != 0) {
        EftShot_SetHeldFlagA8(src->objId);
    }
}

/* Item update: the state machine driven by the fighter's technique events (2 start, 4 fire, 8 end, 0x400 abort);
   while flying it homes, moves and adds a hit record every frame. */
void EftObjTech_Update(EftKTask *task) {
    s32 advance = 0;
    EftObjTech *w = task->work;
    EftKSrc *src = w->src;

    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    EftEmit_UpdateNodesReq(src, w->nodes);
    if (!(w->flags & 0x80)) {
        if (EftShot_TestBits(src->objId, 2)) {
            EftAim_GetDir(&w->dir, &w->unk380, src->objId);
            task->state = 0;
            w->flags |= 0x80;
        } else if (EftShot_TestBits(src->objId, 4)) {
            task->state = 0;
            w->flags |= 0x80;
        }
        if (!(w->flags & 0x80)) {
            goto skip;
        }
    }
    switch (task->state) {
    case 0:
        if (EftShot_TestBits(src->objId, 4)) {
            w->flags |= 0x20;
            Vec4_Copy((Vec4 *)&w->body.unk0, &w->unk3C0);
            Vec4_Copy((Vec4 *)&w->body.pos, (Vec4 *)&w->body.unk0);
            BtlCharApi_GetNodePos(src->objId, 0x11, (Vec4 *)&w->body.prev);
            EftAim_GetDirKeep(src, &w->dir, (Vec4 *)&w->body.pos, src->objId);
            Vec3_Scale((Vec4 *)&w->body.vel, &w->dir, w->speed);
            task->state = 1;
        }
        break;
    case 1:
        advance = 1;
        break;
    case 2:
        if (!(w->flags & 0x400)) {
            if (!(w->flags & 0x40)) {
                Vec4_Copy((Vec4 *)&w->body.prev, (Vec4 *)&w->body.pos);
            }
            if (!(w->src->def->flags & 2)) {
                if (!(w->flags & 0x40)) {
                    if (0.0f < src->def->homing) {
                        EftAim_Home(&w->dir, (Vec4 *)&w->body.pos, &w->dir, src->objId, w->speed,
                                      src->def->homing);
                        Vec3_Scale((Vec4 *)&w->body.vel, &w->dir, w->speed);
                    }
                    Vec3_Add((Vec4 *)&w->body.pos, (Vec4 *)&w->body.pos, (Vec4 *)&w->body.vel);
                } else {
                    Vec3_Copy((Vec4 *)&w->body.pos, &task->pos);
                }
            }
        }
        if (EftShot_TestBits(src->objId, 8)) {
            w->flags |= 0x13;
            task->state = 3;
        }
        break;
    case 3:
        advance = 1;
        break;
    }
skip:
    EftObjTech_UpdateMotion(task);
    if (EftShot_TestBits(src->objId, 0x400)) {
        if (!(w->flags & 0x10)) {
            w->flags |= 8;
        }
        w->flags |= 2;
    }
    EftObjTech_UpdateEmitters(src->objId, task, &w->mgr->set);
    if (w->flags & 0x20) {
        if (w->flags & 0x200) {
            EftEmit_UpdateWidth2(w->mgr, w->emit);
            w->speed = EftEmit_GetWidth2(w->emit) * src->def->scale;
            Vec3_Scale((Vec4 *)&w->body.vel, &w->dir, w->speed);
        }
    }
    if (advance) {
        task->state++;
    }
    if (w->flags & 1) {
        w->timer += 1.0f;
    }
    if (w->flags & 4) {
        BtlTask_SetDead(task);
    } else if (w->flags & 2) {
        if (w->flags & 8) {
            w->flags |= 4;
        } else if (w->timer >= w->life) {
            w->flags |= 4;
        }
    } else if (src->def->flags & 1) {
        if (!(w->flags & 0x20)) {
            return;
        }
        EftEmit_UpdateTrailWidth(w->mgr, w->emit);
        EftObjTech_AddHitRecord(task);
    }
}

/* Item post-update: applies what the hit pass wrote into the task (moved head, hit, end). A hit on a fighter
   stops the fighter's technique timers and pins the object on the target. */
void EftObjTech_PostUpdate(EftKTask *task) {
    Vec4 d;
    EftObjTech *w = task->work;
    EftKSrc *src = w->src;

    EftObjTech_UpdateModelVisible(task);
    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    EftEmit_UpdateAlive(w->mgr, w->emit);
    if (task->hit & 1) {
        Vec3_Sub(&d, &task->pos, (Vec4 *)&w->body.pos);
        Vec3_Copy((Vec4 *)&w->body.pos, &task->pos);
        Vec3_Add((Vec4 *)&w->body.prev, (Vec4 *)&w->body.prev, &d);
        w->flags |= 0x40;
    }
    if (task->flags & 1) {
        if (!(task->flags & 2)) {
            EftTechEvt_RequestStop(src->objId);
            w->flags = (w->flags & ~0x20) | 0x400;
            EftEmit_MarkKind6(w->mgr, w->emit);
        } else {
            w->flags = (w->flags | 2) & ~0x20;
        }
    } else if (task->flags & 2) {
        w->flags = (w->flags | 2) & ~0x20;
    } else if (task->flags & 4) {
        w->flags = (w->flags | 2) & ~0x20;
    }
    if (task->hit & 4) {
        w->flags = (w->flags | 1) & ~0x20;
    }
}

/* Item reset: kills the emitters once, the task dies. */
void EftObjTech_Reset(EftKTask *task) {
    EftObjTech *w = task->work;

    if (!(w->flags & 0x100)) {
        w->flags |= 0x100;
        EftEmit_KillAll(w->mgr, w->emit);
    }
    BtlTask_SetDead(task);
}

/* Item draw: the rings, once they are in use. */
void EftObjTech_Draw(EftKTask *task) {
    EftObjTech *w = task->work;

    if ((w->src->def->flags & 0x40000) && (w->flags & 0x4000)) {
        EftObjTech_DrawRings(task);
    }
}

/* Manager init: loads the emitter set, creates the list of two items, looks up the model and the ring model. */
void EftObjTechMgr_Init(EftKTask *task, EftKSrc *src) {
    EftObjTechMgr *mgr = task->work;

    EftShot_Nop(sizeof(EftObjTechMgr));
    memset(mgr, 0, sizeof(EftObjTechMgr));
    EftEmit_LoadSet(src, mgr, 0, src->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, sizeof(EftObjTech));
    if (src->def->flags & 0x20000) {
        mgr->modelPack = BtlScene_GetPackEntry(src->pack, 1);
    }
    /* STOPGAP: the original loads src->def again here, into a different register than the first test did. Written
       plainly, this compiler's global CSE shares the first test's load on the path that skips the call. The volatile
       read keeps the two loads apart and gives the original instructions; what the original source did is unknown. */
    if (((volatile EftKSrc *)src)->def->flags & 0x40000) {
        s32 *pack = BtlScene_GetPackEntry(src->pack, 1);

        mgr->ringModel = BtlScene_GetPackEntry(pack, 1);
        mgr->ringTex = BtlScene_GetPackEntry(pack, 2);
        EftMesh_Init(mgr->ringProto, mgr->ringModel);
        EftTexSet_Load32(mgr->ringTexSet, mgr->ringTex);
    }
}

/* Manager term: frees the emitter set. */
void EftObjTechMgr_Term(EftKTask *task) {
    EftEmit_FreeSet(task->work);
}

/* Manager update: per-frame start of the emitter set; animates the ring texture while it has items. */
void EftObjTechMgr_Update(EftKTask *task) {
    EftObjTechMgr *mgr = task->work;

    EftEmit_BeginFrame(mgr);
    if (task->unk24[1] != 0 && mgr->ringTex != NULL) {
        EftTexSet_Keep32(mgr->ringTexSet, 1, 0);
    }
}

/* Manager reset: nothing. */
void EftObjTechMgr_Reset(EftKTask *task) {
}

/* ---- "rush shot" module, first helpers ---- */

/* Adds this frame's hit record for the shot body. */
void EftRushShot_AddHitRecord(EftKTask *task) {
    EftRushShot *w = task->work;
    EftKHitRec *rec = EftHit_GetNew();
    f32 size = w->hitScale * EftEmit_GetTrailWidth(w->emit);
    EftKShape *shape = &w->body;

    rec->shape = *shape;
    rec->task = task;
    rec->src = w->src;
    switch (w->src->def->shape) {
    case 1: {
        void *a = EftHitArena_AllocBox();
        void *b = EftHitArena_AllocBox();

        ColCapsule_Set(a, &w->unk390, (Vec4 *)&w->body.pos, size);
        ColCapsule_Set(b, &w->unk390, (Vec4 *)&w->body.prev, size);
        EftHit_SetShapeBoxes(rec, a, b);
        break;
    }
    case 0: {
        void *a = EftHitArena_AllocSphere();
        void *b = EftHitArena_AllocSphere();

        ColSphere_Set(a, (Vec4 *)&w->body.pos, size);
        ColSphere_Set(b, (Vec4 *)&w->body.prev, size);
        EftHit_SetShapeSpheres(rec, a, b);
        break;
    }
    default:
        return;
    }
    EftHit_Add(rec);
}

/* Creates the swarm's model objects from the manager's model pack, hidden. */
void EftRushShot_CreateModels(EftKTask *task) {
    EftKTask *owner = BtlTask_GetParent(task);
    s32 i;
    EftRushShot *w = task->work;
    EftRushShotMgr *mgr = owner->work;

    for (i = 0; i < w->count; i++) {
        EftKModel *model = &w->objs[i].model;

        model->pack = mgr->modelPack;
        model->objId = EftObj_Create(model->arg, model->pack);
        model->unk36 = -1;
        EftObj_SetVisible(model->objId, 0);
        EftObj_Nop(model->objId, 0);
    }
}

/* Places the swarm's models every frame. Mode 1: each model hovers around the centre (bobbing, spinning, sinking),
   and once the shot is released (flag 0x40000) follows it after its own delay; other modes: all models sit on the
   shot's head, turned along its direction. Appearance only, but it draws from libc rand(). */
void EftRushShot_UpdateModels(EftKTask *task) {
    Mtx44 base;
    Mtx44 m;
    Vec4 d;
    Vec4 p;
    Vec4 off;
    Mtx44 rot;
    s32 i;
    EftRushShot *w = task->work;

    Mtx_StoreIdentity(&base);
    for (i = 0; i < w->count; i++) {
        EftRushShotObj *o = &w->objs[i];

        if (w->mode == 1) {
            if (!(w->flags & 0x40000)) {
                if (!(o->model.flags & 1)) {
                    f32 ang = (f32)i / (f32)w->count * 6.2831853f;

                    if (ang > 3.14159265f) {
                        ang -= 6.2831853f;
                    }
                    Mtx_StoreIdentity(&rot);
                    Mtx_RotateY(&rot, &rot, ang);
                    Vec4_Copy(&o->center, &w->unk350);
                    Vec4_Set(&o->offset, 0.0f, (rand() / 2147483647.0f * 8.0f + 3.0f) * w->scale,
                             (rand() / 2147483647.0f * 4.0f + 6.0f) * w->scale, 1.0f);
                    Mtx_MulVec4(&o->offset, &rot, &o->offset);
                    Vec4_Set(&o->drift, 0.0f, 0.0f, 0.0f, 0.0f);
                    o->rot.x = rand() / 2147483647.0f * 3.14159265f;
                    o->rot.y = rand() / 2147483647.0f * 3.14159265f;
                    o->rot.z = 0.0f;
                    o->rotSpeed.x = (rand() / 2147483647.0f - rand() / 2147483647.0f) * 0.08f * 3.14159265f;
                    o->rotSpeed.y = (rand() / 2147483647.0f - rand() / 2147483647.0f) * 0.08f * 3.14159265f;
                    o->rotSpeed.z = 0.0f;
                    if ((i & 3) == 0) {
                        o->unk3C = 0.0f;
                    } else {
                        o->unk3C = rand() / 2147483647.0f * 4.0f;
                    }
                    if ((i & 3) == 0) {
                        o->delay = 0.0f;
                    } else {
                        o->delay = rand() / 2147483647.0f * 3.0f;
                    }
                    if (!(i & 1)) {
                        o->scale = rand() / 2147483647.0f * 0.4f + 0.5f;
                    } else {
                        o->scale = rand() / 2147483647.0f * 0.25f + 0.25f;
                    }
                    o->bobAmp = rand() / 2147483647.0f * 0.2f + 0.3f;
                    o->bobPhase = rand() / 2147483647.0f * 3.14159265f;
                    o->bobSpeed = (rand() / 2147483647.0f * 0.05f + 0.05f) * 3.14159265f;
                    o->sink = (rand() / 2147483647.0f * 0.5f + 1.0f) * 0.2f;
                    o->unk58 = 1.0f;
                    o->model.flags |= 1;
                }
                o->offset.y -= o->sink;
            } else if (o->delay <= 0.0f) {
                if (!(o->model.flags & 2)) {
                    Vec3_Scale(&o->offset, &o->offset, 3.0f);
                    o->rotSpeed.x *= rand() / 2147483647.0f * 0.1f + 1.1f;
                    o->rotSpeed.y *= rand() / 2147483647.0f * 0.1f + 1.1f;
                    o->rotSpeed.z = 0.0f;
                    o->bobAmp = rand() / 2147483647.0f * 0.4f + 0.4f;
                    o->bobSpeed = (rand() / 2147483647.0f * 0.1f + 0.05f) * 3.14159265f;
                    o->model.flags |= 2;
                }
                Vec3_Add(&o->center, &o->center, (Vec4 *)&w->body.vel);
            } else {
                o->delay -= 1.0f;
            }
            if (w->flags & 0x800) {
                EftObj_SetVisible(o->model.objId, 0);
            } else {
                Vec4_Copy(&off, &o->offset);
                off.y += sinf(o->bobPhase) * o->bobAmp;
                o->bobPhase += o->bobSpeed;
                o->bobPhase = EftMath_WrapAngle(o->bobPhase);
                Vec3_Add(&o->offset, &o->offset, &o->drift);
                Vec3_Add(&o->rot, &o->rot, &o->rotSpeed);
                o->rot.x = EftMath_WrapAngle(o->rot.x);
                o->rot.y = EftMath_WrapAngle(o->rot.y);
                o->rot.z = EftMath_WrapAngle(o->rot.z);
                Mtx_ScaleDiagUniform(&m, &base, o->scale);
                Mtx_RotateXYZ(&m, &m, &o->rot);
                Vec3_Add(&p, &o->center, &off);
                p.w = 1.0f;
                Mtx_Translate(&m, &m, &p);
                EftObj_SetMtx(o->model.objId, &m);
                EftObj_SetVisible(o->model.objId, 1);
            }
        } else {
            f32 pitch;
            f32 yaw;

            Mtx_ScaleDiagUniform(&base, &base, w->scale);
            Vec3_Clamp(&d, &w->dir, -1.0f, 1.0f);
            pitch = Mathf_Asin(-d.y);
            yaw = atan2f(d.x, d.z);
            pitch = EftMath_WrapAngle(pitch);
            yaw = EftMath_WrapAngle(yaw);
            Mtx_RotateX(&m, &base, pitch);
            Mtx_RotateY(&m, &m, yaw);
            Mtx_Translate(&m, &m, (Vec4 *)&w->body.pos);
            EftObj_SetMtx(o->model.objId, &m);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly eft_l.c), with its own header and view types. Functions already declared above with
 * the first part's types are reached through cast macros.
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/eft_l.h"

/*
 * Technique effect type 9, the "rush shot", 0x15B550..0x15C728: everything of the module except its first three
 * helpers (EftRushShot_AddHitRecord, _CreateModels, _UpdateModels at 0x15AB38..0x15B550, the end of eft_k.c).
 * Classes: manager gEftRushShotMgrClass, task gEftRushShotClass (gEftShotClass row 10).
 *
 * What it is: the projectile of a rush technique. Technique events of the OWNER (EftShot_TestBits: 2 / 4 start,
 * 4 fire, 8 end, 0x400 abort) fire it from the set's start node towards the owner's aim; it flies at the
 * definition's speed (homing when the definition says so) and adds one hit record per frame. When the rush
 * connects (BtlCharApi_IsRushConnected) it stops flying and sits on the victim (node 3, or the owner's node
 * 0x36), starts the technique's camera cut, tints the stage, and from then on follows the technique events of
 * the VICTIM: stage blur (0x10 / 0x20), stage effects (0x80 / 0x100), screen flash and node sparks (0x40).
 *
 * Translation unit: EftRushShot_UpdateAttached only matches when EftRushShot_UpdateModels (0x15AD78) is defined
 * above it in the same file (a "beqz" becomes "beqzl" otherwise), so this file is the continuation of eft_k.c's
 * object. Checked by compiling eft_k.c with this file appended: all 18 functions match there.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);

extern s32 BtlScene_IsCharStopped(s32 objId);
extern s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);
extern s32 *BtlScene_GetCharPackEntry(s32 side, s32 idx);
#define BtlTask_CreateChildList ((void (*)(EftTask *task, s32 count, s32 workSize))BtlTask_CreateChildList)

extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_IsRushConnected(s32 objId);

/* Argument of EftCam_Start. */
typedef struct EftRushShotMsg {
    s32 unk0;
    s32 objId;
    s32 id;
} EftRushShotMsg;

/* Argument of EftFlash_Start (screen flash). */
typedef struct EftFlashArg {
    /* 0x00 */ EftVec color;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ s32 objId;
    /* 0x20 */ s32 unk20;
} EftFlashArg; /* size 0x30: the vector makes it 16-byte aligned */

/* Argument of EftDelaySe_Start: five (kind, node) pairs. */
typedef struct EftSparkTbl {
    s32 v[10];
} EftSparkTbl;

#define EftCam_Start ((void (*)(EftRushShotMsg *msg))EftCam_Start)
extern void EftCam_Stop(void);
extern f32 EftMath_WrapAngle(f32 angle);
#define EftAim_GetDir ((void (*)(void *dst, void *src, s32 objId))EftAim_GetDir)
#define EftAim_GetDirKeep ((void (*)(EftTechArg *arg, Vec4 *dir, Vec4 *pos, s32 objId))EftAim_GetDirKeep)
extern void EftAim_Home(Vec4 *dir, Vec4 *pos, Vec4 *dir2, s32 objId, f32 speed, f32 turn);
extern s32 EftShot_TestBits(s32 objId, s32 mask);
extern void EftShot_SetHeldFlagA8(s32 objId);
extern void EftShot_Nop(s32 size);
#define EftEmit_LoadSet ((void (*)(EftTechArg *arg, void *model, s32 a2, s32 *pack, s32 a4, s32 a5))EftEmit_LoadSet)
extern void EftEmit_FreeSet(void *model);
extern void EftEmit_BeginFrame(void *model);
#define EftEmit_GetEndFrames ((s32 (*)(EftModel *model))EftEmit_GetEndFrames)
#define EftEmit_InitState ((void (*)(void *model, EftModelInst *inst))EftEmit_InitState)
#define EftEmit_TermState ((void (*)(EftModel *model, EftModelInst *inst))EftEmit_TermState)
#define EftEmit_GetFlagsFromReq ((s32 (*)(EftModel *model, EftModelInst *inst, s32 objId, s32 part, s32 sub, s32 end, s32 fast))EftEmit_GetFlagsFromReq)
#define EftEmit_SpawnOwn ((void (*)(EftModel *model, EftModelInst *inst, void *nodes, Vec4 *pos, void *w, s32 part, s32 sub, s32 flags, f32 size))EftEmit_SpawnOwn)
#define EftEmit_MarkKind6 ((void (*)(EftModel *model, EftModelInst *inst))EftEmit_MarkKind6)
#define EftEmit_KillAll ((void (*)(EftModel *model, EftModelInst *inst))EftEmit_KillAll)
#define EftEmit_UpdateAlive ((s32 (*)(EftModel *model, EftModelInst *inst))EftEmit_UpdateAlive)
#define EftEmit_UpdateNodesReq ((void (*)(EftTechArg *arg, void *nodes))EftEmit_UpdateNodesReq)
#define EftEmit_UpdateTrailWidth ((void (*)(EftModel *model, EftModelInst *inst))EftEmit_UpdateTrailWidth)
#define EftEmit_HasWidth2 ((s32 (*)(EftModel *model))EftEmit_HasWidth2)
#define EftEmit_UpdateWidth2 ((void (*)(EftModel *model, EftModelInst *inst))EftEmit_UpdateWidth2)
#define EftEmit_GetWidth2 ((f32 (*)(EftModelInst *inst))EftEmit_GetWidth2)
extern void EftTechEvt_RequestStop(s32 objId);
#define EftRushShot_AddHitRecord ((void (*)(EftTask *task))EftRushShot_AddHitRecord)
#define EftRushShot_CreateModels ((void (*)(EftTask *task))EftRushShot_CreateModels)
#define EftRushShot_UpdateModels ((void (*)(EftTask *task))EftRushShot_UpdateModels)
extern void EftFlash_Start(EftFlashArg *arg);
extern s32 EftStreak_Start(s32 objId, s32 kind, s32 arg2, f32 angle);
#define EftStreak_Stop ((void (*)(s32 handle))EftStreak_Stop)
extern void EftDelaySe_Start(s32 objId, EftSparkTbl *tbl, s32 count);
extern void EftObj_Destroy(s32 handle);
#define BtlTask_SetDead ((void (*)(EftTask *task))BtlTask_SetDead)
#define BtlTask_SetOwnerTag ((void (*)(EftTask *task, s32 flag))BtlTask_SetOwnerTag)
#define BtlTask_GetParent ((EftTask *(*)(EftTask *task))BtlTask_GetParent)
extern void StgTint_Start(s32 a0, s32 a1, f32 time);
extern void StgBlur_SetCenter(s32 a0, Vec4 *pos, s32 a2);
extern void StgBlur_SetColor0Rgba(s32 a0, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor1Rgba(s32 a0, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor2Rgba(s32 a0, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor3Rgba(s32 a0, s32 r, s32 g, s32 b, s32 a);

extern const EftSparkTbl gEftRushShotSparkTbl;

/* Frees the swarm model objects EftRushShot_CreateModels made. */
void EftRushShot_FreeModels(EftTask *task) {
    EftRushShotWork *w = task->work;
    EftRushShotModel *sub;
    s32 i;

    for (i = 0; i < w->subCount; i++) {
        sub = &w->sub[i];
        EftObj_Destroy(sub->handle);
    }
}

/* Once the rush has connected, starts the technique's camera cut (EftCam_Start: animation, owner, effect id). */
void EftRushShot_StartCam(EftTask *task) {
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;
    EftRushShotMsg msg;

    if (w->flags & EFT_RUSHSHOT_CONNECTED) {
        msg = (EftRushShotMsg){ (s32)w->unkAB0, arg->objId, arg->def->id };
        EftCam_Start(&msg);
        w->flags |= EFT_RUSHSHOT_SENT;
    }
}

/* Definition flag 0x200: the stage blur. The victim's event 0x10 starts a fade-in (0.1 per frame) and takes the
 * owner's aim from node 0x11 as the blur centre argument; event 0x20 switches it off. */
void EftRushShot_UpdateFade(EftTask *task) {
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;
    s32 opp = BtlCharApi_GetOpponentObjId(arg->objId);
    Vec4 pos;
    Vec4 *center;
    f32 fade;
    s32 a1;
    s32 a2;

    if (arg->def->flags & 0x200) {
        if (EftShot_TestBits(opp, 0x10)) {
            w->flags |= EFT_RUSHSHOT_FADE;
            center = &w->fadePos;
            BtlCharApi_GetNodePos(arg->objId, 0x11, &pos);
            EftAim_GetDir(center, &pos, arg->objId);
        } else {
            center = &w->fadePos;
        }
        if (EftShot_TestBits(opp, 0x20)) {
            w->fade = 0.0f;
            w->flags &= ~EFT_RUSHSHOT_FADE;
        }
        if (w->flags & EFT_RUSHSHOT_FADE) {
            w->fade += 0.1f;
            if (w->fade > 1.0f) {
                w->fade = 1.0f;
            }
        }
        fade = w->fade;
        a1 = fade * 64.0f;
        a2 = fade * 128.0f;
        StgBlur_SetColor0Rgba(0, 0x80, 0x80, 0x80, (u8)(s32)(fade * 32.0f));
        StgBlur_SetColor1Rgba(0, 0x80, 0x80, 0x80, (u8)a1);
        StgBlur_SetColor2Rgba(0, 0x80, 0x80, 0x80, (u8)a2);
        StgBlur_SetColor3Rgba(0, 0x80, 0x80, 0x80, 0x40);
        StgBlur_SetCenter(0, center, 0);
    }
}

/* Victim's bit 0x80 starts the next stage effect of the definition (0x180760), bit 0x100 stops it. */
void EftRushShot_UpdateStageFx(EftTask *task) {
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;
    s32 opp = BtlCharApi_GetOpponentObjId(arg->objId);

    if (EftShot_TestBits(opp, 0x80)) {
        s32 i = w->stage;
        f32 angle = arg->def->unk60[i] * 3.14159265f / 180.0f;
        s32 kind = arg->def->unk58[i];
        s32 unk80 = arg->def->unk80;

        angle = EftMath_WrapAngle(angle);
        if (kind >= 0) {
            w->stageFx[i] = EftStreak_Start(arg->objId, kind, unk80, angle);
            w->flags |= EFT_RUSHSHOT_STAGE_FX;
        }
        w->stage++;
    }
    if (EftShot_TestBits(opp, 0x100)) {
        s32 i = w->stage - 1;

        if (i < 0) {
            i = 0;
        }
        if (w->flags & EFT_RUSHSHOT_STAGE_FX) {
            EftStreak_Stop(w->stageFx[i]);
            w->stageFx[i] = 0;
            w->flags &= ~EFT_RUSHSHOT_STAGE_FX;
        }
    }
}

/* Victim's bit 0x40 (unless definition flag 0x400000): white screen flash (0x172480). */
void EftRushShot_UpdateFlash(EftTask *task) {
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;
    s32 opp = BtlCharApi_GetOpponentObjId(arg->objId);
    EftFlashArg flash;

    if (!(arg->def->flags & 0x400000) && EftShot_TestBits(opp, 0x40)) {
        flash = (EftFlashArg){ { 255.0f, 255.0f, 255.0f, 255.0f }, 0.5f, 0, 1.0f, arg->objId, 0 };
        EftFlash_Start(&flash);
    }
}

/* Victim's bit 0x40: sparks at five of the owner's nodes (0x19B7F8 with the table gEftRushShotSparkTbl). */
void EftRushShot_UpdateSparks(EftTask *task) {
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;
    s32 opp = BtlCharApi_GetOpponentObjId(arg->objId);

    if (EftShot_TestBits(opp, 0x40)) {
        EftSparkTbl tbl = gEftRushShotSparkTbl;

        EftDelaySe_Start(arg->objId, &tbl, 5);
    }
}

/* Steps every emitter of the model (19 groups) and spawns its particles at the projectile. */
void EftRushShot_Draw(s32 objId, EftTask *task, EftModel *model) {
    EftRushShotWork *w = task->work;
    Vec4 pos;
    s32 endAlt = 0;
    s32 alt = 0;
    s32 end;
    s32 fast;
    s32 part;
    s32 sub;

    Vec4_Copy(&pos, &w->pos);
    fast = w->flags & EFT_RUSHSHOT_FAST_END;
    end = w->flags & EFT_RUSHSHOT_END;
    if (w->type == 1) {
        if (!(w->flags & EFT_RUSHSHOT_ATTACHED)) {
            end = w->flags & EFT_RUSHSHOT_COUNT;
        } else if (!(w->flags & EFT_RUSHSHOT_ATTACHED2)) {
            end = w->flags & EFT_RUSHSHOT_COUNT;
            w->flags |= EFT_RUSHSHOT_ATTACHED2;
        } else {
            Vec4_Copy(&pos, &w->pos);
            end = w->flags & EFT_RUSHSHOT_END;
            alt = 1;
            endAlt = end;
        }
    }
    for (part = 0; part < 0x13; part++) {
        if (*model->mask & (1 << part)) {
            EftModelPartDef *def = model->part[part].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 idx = model->part[part].first + sub;
                s32 spawn;

                if (alt) {
                    end = endAlt;
                    if (model->nodes[idx].unk9 == 0 && model->nodes[idx].unk18 > 0.0f) {
                        end = 0;
                        fast = 0;
                    }
                }
                spawn = EftEmit_GetFlagsFromReq(model, &w->inst, objId, part, sub, end, fast);
                if (spawn != 0) {
                    EftEmit_SpawnOwn(model, &w->inst, w->unk330, &pos, w, part, sub, spawn, w->drawSize);
                }
            }
        }
    }
}

/* Swarm models, the node the shot sits on while attached, and the four handlers of the victim's events.
 * Standalone this differs by one instruction (beqz / beqzl at +0x48); see the note at the top of the file. */
void EftRushShot_UpdateAttached(EftTask *task) {
    EftRushShotMgrWork *mgr = BtlTask_GetParent(task)->work;
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;

    if (w->type == 1) {
        if (mgr->sub[0] != NULL) {
            if (w->flags & EFT_RUSHSHOT_SUB) {
                EftRushShot_UpdateModels(task);
            }
        }
        if (w->flags & EFT_RUSHSHOT_ATTACHED) {
            BtlCharApi_GetNodePos(BtlCharApi_GetOpponentObjId(arg->objId), 3, &w->pos);
            EftAim_GetDir(w, &w->unk390, arg->objId);
        }
    } else {
        if (w->flags & EFT_RUSHSHOT_ATTACHED) {
            BtlCharApi_GetNodePos(arg->objId, 0x36, &w->pos);
            EftAim_GetDir(w, &w->unk390, arg->objId);
        }
    }
    if (w->flags & EFT_RUSHSHOT_CONNECTED) {
        if (arg->def->flags & 0x200) {
            EftRushShot_UpdateFade(task);
        }
        EftRushShot_UpdateStageFx(task);
        EftRushShot_UpdateFlash(task);
        EftRushShot_UpdateSparks(task);
    }
}

/* Init callback: clears the work, takes speed and size from the definition and creates the model instance. */
void EftRushShot_Init(EftTask *task, EftTechArg *arg) {
    EftRushShotMgrWork *mgr = BtlTask_GetParent(task)->work;
    EftRushShotWork *w = task->work;
    EftTechDef *def;

    memset(w, 0, sizeof(EftRushShotWork));
    w->arg = arg;
    def = arg->def;
    w->speed = def->speed;
    w->size = def->size;
    w->drawSize = def->size;
    w->size2 = w->size;
    w->model = &mgr->model;
    EftEmit_InitState(mgr, &w->inst);
    if (EftEmit_HasWidth2(w->model)) {
        w->speed = EftEmit_GetWidth2(&w->inst) * def->size;
        w->flags |= EFT_RUSHSHOT_SCALED;
    }
    w->life = EftEmit_GetEndFrames(w->model);
    if (def->id == 0x19D) {
        w->type = 1;
    } else {
        w->type = 0;
    }
    if (mgr->sub[0] != NULL) {
        w->subCount = 7;
        EftRushShot_CreateModels(task);
    }
    if (arg->slot >= 2) {
        s32 *v = mgr->sub[arg->slot - 1];

        if (v != NULL) {
            w->unkAB0 = v;
        }
    }
    BtlTask_SetOwnerTag(task, arg->objId == 0 ? 0x800 : 0x1000);
}

/* Term callback: ends the stage tint, the sub-effects and the model instance. */
void EftRushShot_Term(EftTask *task) {
    EftRushShotMgrWork *mgr = BtlTask_GetParent(task)->work;
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;

    if (w->flags & EFT_RUSHSHOT_ATTACHED) {
        StgTint_Start(0, 1, 0.2f);
    }
    if (mgr->sub[0] != NULL) {
        EftRushShot_FreeModels(task);
    }
    EftEmit_TermState(w->model, &w->inst);
    if (arg->def->unk4 != 0) {
        EftShot_SetHeldFlagA8(arg->objId);
    }
}

/* Update callback. */
void EftRushShot_Update(EftTask *task) {
    s32 advance = 0;
    EftRushShotMgrWork *mgr = BtlTask_GetParent(task)->work;
    EftRushShotWork *w = task->work;
    EftTechArg *arg = w->arg;

    if (BtlScene_IsCharStopped(arg->objId)) {
        return;
    }
    if (BtlCharApi_IsRushConnected(arg->objId) && !(w->flags & EFT_RUSHSHOT_CONNECTED)) {
        EftEmit_MarkKind6(w->model, &w->inst);
        EftTechEvt_RequestStop(arg->objId);
        w->flags = (w->flags & ~EFT_RUSHSHOT_FIRED) | EFT_RUSHSHOT_ATTACHED;
        StgTint_Start(0, 0, 0.2f);
        w->flags |= EFT_RUSHSHOT_CONNECTED;
    }
    if (!(w->flags & EFT_RUSHSHOT_SENT)) {
        EftRushShot_StartCam(task);
    }
    EftEmit_UpdateNodesReq(arg, w->unk330);
    if (!(w->flags & EFT_RUSHSHOT_STARTED)) {
        if (EftShot_TestBits(arg->objId, 2)) {
            EftAim_GetDir(w, w->unk350, arg->objId);
            task->step = 0;
            w->flags |= EFT_RUSHSHOT_STARTED;
            if (mgr->sub[0] != NULL) {
                w->flags |= EFT_RUSHSHOT_SUB;
            }
        } else if (EftShot_TestBits(arg->objId, 4)) {
            task->step = 0;
            w->flags |= EFT_RUSHSHOT_STARTED;
        }
    }
    if (w->flags & EFT_RUSHSHOT_STARTED) {
        switch (task->step) {
            case 0:
                if (EftShot_TestBits(arg->objId, 4)) {
                    w->flags |= EFT_RUSHSHOT_FIRED;
                    Vec4_Copy(&w->start, &w->unk390);
                    Vec4_Copy(&w->pos, &w->start);
                    BtlCharApi_GetNodePos(arg->objId, 0x11, &w->prev);
                    EftAim_GetDirKeep(arg, &w->dir, &w->pos, arg->objId);
                    Vec3_Scale(&w->vel, &w->dir, w->speed);
                    task->step = 1;
                    w->flags |= EFT_RUSHSHOT_FLIGHT;
                }
                break;
            case 2:
                if (!(w->flags & EFT_RUSHSHOT_HIT_MOVED)) {
                    Vec4_Copy(&w->prev, &w->pos);
                }
                if (!(w->arg->def->flags & 2)) {
                    if (!(w->flags & EFT_RUSHSHOT_HIT_MOVED)) {
                        if (arg->def->unk38 > 0.0f) {
                            EftAim_Home(&w->dir, &w->pos, &w->dir, arg->objId, w->speed, arg->def->unk38);
                            Vec3_Scale(&w->vel, &w->dir, w->speed);
                        }
                        Vec3_Add(&w->pos, &w->pos, &w->vel);
                    } else {
                        Vec3_Copy(&w->pos, &task->hitPos);
                    }
                }
                if (EftShot_TestBits(arg->objId, 8)) {
                    w->flags |= EFT_RUSHSHOT_ENDED | EFT_RUSHSHOT_END | EFT_RUSHSHOT_COUNT;
                    task->step = 3;
                }
                break;
            case 1:
                advance = 1;
                break;
            case 3:
                advance = 1;
                break;
        }
    }
    EftRushShot_UpdateAttached(task);
    if (EftShot_TestBits(arg->objId, 0x400)) {
        if (!(w->flags & EFT_RUSHSHOT_ENDED)) {
            w->flags |= EFT_RUSHSHOT_FAST_END;
        }
        w->flags |= EFT_RUSHSHOT_END;
    }
    EftRushShot_Draw(arg->objId, task, w->model);
    if (w->flags & EFT_RUSHSHOT_FIRED) {
        if (w->flags & EFT_RUSHSHOT_SCALED) {
            EftEmit_UpdateWidth2(w->model, &w->inst);
            w->speed = EftEmit_GetWidth2(&w->inst) * arg->def->size;
            Vec3_Scale(&w->vel, &w->dir, w->speed);
        }
    }
    if (advance) {
        task->step++;
    }
    if (w->flags & EFT_RUSHSHOT_COUNT) {
        w->timer += 1.0f;
    }
    if (w->flags & EFT_RUSHSHOT_KILL) {
        BtlTask_SetDead(task);
    } else if (w->flags & EFT_RUSHSHOT_END) {
        if ((w->flags & EFT_RUSHSHOT_FAST_END) || w->timer >= w->life) {
            w->flags |= EFT_RUSHSHOT_KILL;
        }
        if (w->flags & EFT_RUSHSHOT_SENT) {
            if (!(w->flags & EFT_RUSHSHOT_SENT2)) {
                EftCam_Stop();
                w->flags |= EFT_RUSHSHOT_SENT2;
            }
        }
    } else if (arg->def->flags & 1) {
        if (w->flags & EFT_RUSHSHOT_FIRED) {
            EftEmit_UpdateTrailWidth(w->model, &w->inst);
            EftRushShot_AddHitRecord(task);
        }
    }
}

/* Post-update callback: reads what the hit record did this frame (moved, hit, ended). */
void EftRushShot_PostUpdate(EftTask *task) {
    EftRushShotWork *w = task->work;
    Vec4 move;

    if (BtlScene_IsCharStopped(w->arg->objId)) {
        return;
    }
    EftEmit_UpdateAlive(w->model, &w->inst);
    if (task->hit & 1) {
        Vec3_Sub(&move, &task->hitPos, &w->pos);
        Vec3_Copy(&w->pos, &task->hitPos);
        Vec3_Add(&w->prev, &w->prev, &move);
        w->flags |= EFT_RUSHSHOT_HIT_MOVED;
    }
    if (task->flags & 1) {
        if (task->flags & 2) {
            w->flags = (w->flags | EFT_RUSHSHOT_END) & ~EFT_RUSHSHOT_FIRED;
        } else {
            w->flags &= ~EFT_RUSHSHOT_FIRED;
        }
    } else if (task->flags & 2) {
        w->flags = (w->flags | EFT_RUSHSHOT_END) & ~EFT_RUSHSHOT_FIRED;
    } else if (task->flags & 4) {
        w->flags = (w->flags | EFT_RUSHSHOT_END) & ~EFT_RUSHSHOT_FIRED;
    }
    if (task->hit & 4) {
        w->flags = (w->flags | EFT_RUSHSHOT_COUNT) & ~EFT_RUSHSHOT_FIRED;
    }
}

/* Reset callback: destroys the particles once and kills the task. */
void EftRushShot_Reset(EftTask *task) {
    EftRushShotWork *w = task->work;

    if (!(w->flags & EFT_RUSHSHOT_RESET)) {
        w->flags |= EFT_RUSHSHOT_RESET;
        EftEmit_KillAll(w->model, &w->inst);
    }
    BtlTask_SetDead(task);
}

/* Draw callback: nothing. */
void EftRushShot_Nop(void) {
}

/* Manager init: loads the effect model from the technique's pack and makes room for two projectiles. */
void EftRushShotMgr_Init(EftTask *task, EftTechArg *arg) {
    EftRushShotMgrWork *mgr = task->work;
    EftRushShotMgrWork *next;
    s32 i;

    EftShot_Nop(sizeof(EftRushShotMgrWork));
    memset(mgr, 0, sizeof(EftRushShotMgrWork));
    EftEmit_LoadSet(arg, mgr, 0, arg->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, sizeof(EftRushShotWork));
    if (arg->def->flags & 0x20000) {
        mgr->sub[0] = BtlScene_GetPackEntry(arg->pack, 1);
    }
    /* The original addresses sub[i + 1] as "(mgr + 4)->sub[i]": a base pointer one word into the work. */
    next = (EftRushShotMgrWork *)((u8 *)mgr + 4);
    for (i = 0; i < 3; i++) {
        s32 *pack = BtlScene_GetCharPackEntry(arg->side, 8);

        if (pack[i + 2] != pack[i + 1]) {
            next->sub[i] = BtlScene_GetPackEntry(pack, i + 1);
        }
    }
}

/* Manager term: releases the model. */
void EftRushShotMgr_Term(EftTask *task) {
    EftEmit_FreeSet(task->work);
}

/* Manager update: steps the model. */
void EftRushShotMgr_Update(EftTask *task) {
    EftEmit_BeginFrame(task->work);
}

/* Manager reset: nothing. */
void EftRushShotMgr_Reset(void) {
}
