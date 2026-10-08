#include "common.h"
#include "battle/eft_j.h"

/*
 * Technique effect modules of scene layer 1, 0x1532A0..0x157398 (65 functions, all matching). Layer 1 keeps,
 * per fighter, a few "shot slots"; a slot that fires a technique creates the manager
 * task of the effect type its definition names (row type + 1 of gEftShotClass, 0x2C3700: {manager class, item
 * class, 0}), and the manager creates up to two items. A class is six callbacks in the order update, init, term,
 * post-update, reset, draw (BtlTaskClass). This file holds, in address order:
 *
 *   type 7 "follow"     0x1532A0..0x1533B0  the item's reset / draw and the four manager callbacks (the module
 *                                           itself is in eft_i.c)
 *   type 5 "multi"      0x1533B0..0x1542A8  up to ten "pieces" (tasks of class 0x2C3B38, created by
 *                                           EftDisc_Create) fired one per START or FIRE event
 *   type 6 "prop shot"  0x1542A8..0x155588  one shot that carries a battle-object model (the "prop")
 *   type 0 "blast"      0x155588..0x156450  the plain blast / beam
 *   type 2 "shots"      0x156450..0x157398  helpers of the item whose callbacks are in eft_k.c: up to 14 blast
 *                                           objects (tasks of class 0x2C3AD8, created by EftBlastObj_Create)
 *
 * What every item does (verified by the matching code):
 *   - it reads the fighter's technique events of this frame with EftShot_TestBits(objId, bits) (the bits are the
 *     EFT_TECH_EVT_* of eft_k.h: 2 START, 4 FIRE, 8 END, 0x10 / 0x20 / 0x40 extra, 0x400 ABORT) and steps a small
 *     state machine in task->state; nothing happens while BtlScene_IsCharStopped(objId);
 *   - items 0 and 6 move a head position themselves (velocity = dir * speed, homing through EftAim_Home when the
 *     definition's homing is positive) and, when the definition's flag 1 is set, add one hit record per frame
 *     (EftHit_GetNew / EftHit_Add) with two spheres or two boxes at the head and the tail. Items 5 and 2 do not add
 *     records: the pieces / blast objects they create do;
 *   - the post-update reads what the scene's hit pass wrote back into the task (task->hit bit 0 + task->pos: the
 *     head was moved, e.g. stopped by the stage; bit 2: stop; task->unk4 bit 0: it hit a fighter);
 *   - the rest is drawing: the emitter set (EftEmit_*), the prop model, the stage blur (StgBlur_*), a screen flash.
 *
 * Item flags (first word of every work block here): 1 ending (the timer runs), 2 dead (kill the task), 4 aborted,
 * 8 END seen, 0x10 fired (the head moves and can hit), 0x20 the hit pass holds the head, 0x40 started, 0x80
 * emitters killed, 0x100 set by the pieces / aim from node 0x30, 0x200 / 0x400 / 0x800 / 0x1000 per module.
 *
 * Evidence about the original source that the matching depends on:
 *   - the vector type was 16-byte aligned: vectors are copied by assignment with two 64-bit moves, parameter
 *     structs have an unnamed hole in front of their first vector, and `(flags & 1) && !(flags & 2)` is folded
 *     into one 64-bit load (ld) because the work block is at least 8-byte aligned. include/sys/math3d.h's Vec4
 *     is not aligned, so this file uses its own EftJVec;
 *   - parameter blocks are built as compound literals and then patched (`arg = (T){...}`); the temporary's
 *     stack slot is reused by later locals;
 *   - address temporaries that the disassembly shows at the top of a function (EftShotTech_Start) are not
 *     variables: the scheduler moves them up across basic blocks.
 *
 * Callees still named by address: BtlTask_GetParent(task) parent task, BtlTask_SetDead(task) mark a task dead,
 * BtlTask_SetOwnerTag(task, bits) or bits into the task's class flags (0x800 / 0x1000 = character 0 / 1, the bits
 * BtlScene_Reset uses); EftObj_Create / EftObj_Destroy / EftObj_SetMtx / EftObj_SetVisible / EftObj_Nop create /
 * destroy / place / show a battle object; EftDisc_Create.. the piece task; EftBlastObj_Create.. the blast object task;
 * EftFlash_Start screen flash; EftDelaySe_Start node effects; EftStreak_Start / EftStreak_Stop start / stop a
 * sub-effect; ColCapsule_Set / ColSphere_Set fill a box / sphere shape; Mtx_Translate / 398 / 428 / 4B8 / 590
 * matrix translate / rotate X / rotate Y / rotate by angles / scale; Vec3_Copy copies a position.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Cos(f32 angle);
extern f32 Mathf_Asin(f32 x);

extern void Vec4_Copy(void *dst, void *src);
extern void Vec4_Set(void *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Add(void *dst, void *a, void *b);
extern void Vec3_Sub(void *dst, void *a, void *b);
extern void Vec3_Scale(void *dst, void *src, f32 scale);
extern void Vec3_Normalize(void *dst, void *src);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Vec3_Copy(void *dst, void *src);
extern void Mtx_ScaleDiagUniform(Mtx44 *dst, Mtx44 *src, f32 scale);
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_Translate(Mtx44 *dst, Mtx44 *src, void *v);
extern void Mtx_RotateZXY(Mtx44 *dst, Mtx44 *src, void *angles);
extern f32 EftMath_WrapAngle(f32 angle);

extern s32 BtlScene_IsCharStopped(s32 objId);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern s32 *BtlScene_GetPackEntry(s32 *base, s32 idx);
extern f32 BtlScene_RandF(void);
extern void BtlTask_CreateChildList(EftJTask *task, s32 count, s32 workSize);

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, void *out);
extern void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out);
extern void BtlCharApi_GetRot(s32 objId, Vec4 *out);
extern s32 BtlCharApi_GetCostume(s32 objId);
extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern s32 BtlCharApi_ObjHasFlags42(s32 objId);
extern void BtlCharApi_ObjSetFlag100(s32 objId, s32 on);

extern f32 BtlCharApi_GetUnkE44(s32 objId);
extern f32 BtlCharApi_GetUnkE5CRatio(s32 objId);
extern f32 BtlCharApi_GetUnkE60Ratio(s32 objId);
extern void EftEmit_UpdateWidth2(EftJSet *set, void *emit);
extern s32 EftShot_GetAttrKind(s32 objId, u64 evt);
extern u8 EftEmit_GetHead26(void *set);
extern s32 EftEmit_GetHead25(void *set);
extern s32 EftEmit_GetPhaseMask(void *set);
extern s32 EftEmit_IsPartDeferred(EftJSet *set, s32 group, s32 part);
extern void EftObj_Nop(s32 obj, s32 a1);
extern void *EftBlastObj_Create(EftJShotArg *arg);
extern void EftBlastObj_Stop(void *h);
extern s32 EftBlastObj_IsAlive(void *h);
extern void EftBlastObj_SetTarget(void *h, s32 a1, void *pos);
extern void EftBlastObj_SetFrozen(void *h, s32 a1);
extern void EftBlastObj_SetNoHit(void *h, s32 a1);
extern void EftFlash_Start(EftJFlashArg *arg);
extern void EftDelaySe_Start(s32 objId, s32 *tbl, s32 count);
extern void *EftStreak_Start(s32 objId, s32 kind, s32 arg, f32 angle);
extern void EftStreak_Stop(void *sub);
extern EftJTask *BtlTask_GetParent(EftJTask *task);
extern void BtlTask_SetDead(EftJTask *task);
extern void BtlTask_SetOwnerTag(EftJTask *task, s32 flags);
extern s32 EftShot_TestBits(s32 objId, s32 bits);
extern void EftShot_Nop(s32 size);
extern void EftShot_SetHeldFlagA8(s32 objId);
extern void EftShot_PlayFireSound(s32 objId, EftJSrc *src);
extern void EftEmit_LoadSet(EftJSrc *src, EftJSet *set, s32 a2, s32 *pack, s32 t0, s32 t1);
extern void EftEmit_FreeSet(EftJSet *set);
extern void EftEmit_BeginFrame(EftJSet *set);
extern s32 EftEmit_GetEndFrames(EftJSet *set);
extern void EftEmit_InitState(EftJSet *set, void *emit);
extern void EftEmit_TermState(EftJSet *set, void *emit);
extern s32 EftEmit_GetFlagsFromReq(EftJSet *set, void *emit, s32 objId, s32 group, s32 part, s32 f1, s32 f4);
extern s32 EftEmit_GetResetFlags(EftJSet *set, void *emit, s32 group, s32 part);
extern void EftEmit_SpawnOwn(EftJSet *set, void *emit, void *a2, void *a3, void *t0, s32 group, s32 part, s32 res, f32 scale);
extern void EftEmit_KillAll(EftJSet *set, void *emit);
extern void EftEmit_MarkKind6(EftJSet *set, void *emit);
extern void EftEmit_UpdateAlive(EftJSet *set, void *emit);
extern void EftEmit_UpdateNodesReq(EftJSrc *src, void *out);
extern s32 EftEmit_HasWidth2(EftJSet *set);
extern f32 EftEmit_GetWidth2(void *emit);
extern f32 EftEmit_GetTrailWidth(void *emit);
extern s32 EftObj_Create(void *arg, s32 *model);
extern void EftObj_Destroy(s32 obj);
extern void EftObj_SetMtx(s32 obj, Mtx44 *m);
extern void EftObj_SetVisible(s32 obj, s32 show);
extern EftJHitRec *EftHit_GetNew(void);
extern void EftHit_Add(EftJHitRec *rec);
extern void *EftHitArena_AllocSphere(void);
extern void *EftHitArena_AllocBox(void);
extern void EftHit_SetShapeSpheres(EftJHitRec *rec, void *a, void *b);
extern void EftHit_SetShapeBoxes(EftJHitRec *rec, void *a, void *b);
extern void ColSphere_Set(void *shape, void *pos, f32 r);
extern void ColCapsule_Set(void *shape, void *a, void *b, f32 r);
extern void EftAim_Home(void *out, void *pos, void *dir, s32 objId, f32 speed, f32 homing);
extern void StgBlur_SetCenter(s32 light, void *dir, s32 a2);
extern void StgBlur_SetColor0Rgba(s32 light, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor1Rgba(s32 light, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor2Rgba(s32 light, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor3Rgba(s32 light, s32 r, s32 g, s32 b, s32 a);
extern void EftEmit_UpdateTrailWidth(EftJSet *set, void *emit);
extern s32 EftAim_GetDir(void *dir, void *from, s32 objId);
extern void EftAim_GetDirKeep(EftJSrc *src, void *dir, void *from, s32 objId);
extern void EftTexSet_Load32(void *tex, s32 *entry);
extern void *EftDisc_Create(EftJPieceArg *arg);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftDisc_Release(void *h, PortVec4 dir);
#define EftDisc_Release(h, dir) EftDisc_Release(h, *(PortVec4 *)(dir))
#else
extern void EftDisc_Release(void *h, Vec4 *dir);
#endif
extern void EftDisc_Kill(void *h);
extern void EftDisc_SetSpin(void *h, f32 angle);
extern void EftDisc_SetScale(void *h, f32 v);
extern void EftDisc_SetModelTex(void *h, void *tex, s32 a2, s32 a3);
extern void EftDisc_SetLastHit(void *h, s32 a1);
extern void EftDisc_SetFlag200(void *h);
extern s32 EftDisc_IsAlive(void *h);

/* ---- effect type 7, "follow" (item class 0x2C3850, manager class 0x2C3838): the tail of eft_i.c's module ---- */

typedef struct EftJFollow {
    /* 0x000 */ s32 flags;
    /* 0x004 */ u8 unk4[0x80];
    /* 0x084 */ u8 emit[0x51C];
    /* 0x5A0 */ EftJSet *set;
} EftJFollow;

typedef struct EftJFollowGroup {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ EftJSet set;
} EftJFollowGroup; /* size 0x324 */

/* Reset callback of the follow item: kills the emitters once and the task. */
void EftFollow_Reset(EftJTask *task) {
    EftJFollow *w = task->work;

    if (!(w->flags & 0x80)) {
        w->flags |= 0x80;
        EftEmit_KillAll(w->set, w->emit);
    }
    BtlTask_SetDead(task);
}

/* Draw callback of the follow item: nothing. */
void EftFollow_Draw(EftJTask *task) {
}

/* Init callback of the follow manager: clears the work, builds the emitter set, makes room for 2 items. */
void EftFollowGroup_Init(EftJTask *task, EftJSrc *src) {
    EftJFollowGroup *m = task->work;

    EftShot_Nop(0x324);
    memset(m, 0, 0x324);
    EftEmit_LoadSet(src, &m->set, 0, src->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, 0x5B0);
}

/* Term callback of the follow manager. */
void EftFollowGroup_Term(EftJTask *task) {
    EftEmit_FreeSet(&((EftJFollowGroup *)task->work)->set);
}

/* Update callback of the follow manager. */
void EftFollowGroup_Update(EftJTask *task) {
    EftEmit_BeginFrame(&((EftJFollowGroup *)task->work)->set);
}

/* Reset callback of the follow manager: nothing. */
void EftFollowGroup_Reset(EftJTask *task) {
}

/* ---- effect type 5, "multi" (item class 0x2C3880, manager class 0x2C3868): fires up to ten pieces ---- */

/* Returns the first free piece slot (cleared), or NULL when all ten are in use. */
EftJPiece *EftMulti_AllocPiece(EftMulti *w) {
    s32 i;

    EftJPiece *p = w->piece;

    for (i = 0; i < 10; i++, p++) {
        if (p->h == NULL) {
            p->flags = 0;
            return p;
        }
    }
    return NULL;
}

/* Fires the next piece: fills the parameter block for its kind and creates it. */
void EftMulti_FirePiece(s32 objId, EftJTask *task) {
    EftJPieceArg arg;
    EftJTask *parent = BtlTask_GetParent(task);
    EftMulti *w = task->work;
    EftMultiMgr *mgr = parent->work;
    EftJSrc *src = w->src;
    EftJPiece *piece = EftMulti_AllocPiece(w);
    void *h;

    if (piece == NULL) {
        return;
    }
    arg = (EftJPieceArg){
        src, w->set, {}, {}, src->def->scale, src->def->speed, (f32)src->def->life / 30.0f, 0.1f, 0.9f,
        src->def->homing, 3.5f, 0.0f, 0.0f, 0, 0, objId, 2, 0, 0
    };
    Vec4_Copy(&arg.dir, &w->dir);
    if (w->flags & 0x200) {
        arg.unk57 = 2;
        switch (src->def->id) {
        case 0x157:
            arg.unk55 = 1;
            if (w->count == 0) {
                arg.unk3C = 0.25f;
            } else {
                arg.unk3C = 0.04f;
            }
            if (w->count < 2) {
                arg.unk5C = 1;
            } else {
                arg.unk5C = w->count & 1;
            }
            w->unk18 = 4.0f;
            break;
        case 0x193:
            arg.unk55 = 2;
            arg.unk5C = w->count % 2 == 0;
            w->unk18 = 1.0f;
            break;
        case 0x194:
            if (w->count == 0) {
                arg.unk3C = 0.2f;
            } else {
                arg.unk3C = 0.04f;
            }
            arg.unk55 = 3;
            arg.unk5C = w->count % 2 == 0;
            w->unk18 = 5.0f;
            break;
        case 0x188:
            arg.unk55 = 0;
            arg.unk3C = 0.35f;
            arg.unk5C = w->count & 1;
            w->unk18 = 5.0f;
            break;
        }
    } else {
        Mtx44 m;
        Vec4 rot;
        Vec4 d;

        arg.unk57 = 4;
        arg.unk3C = 0.0f;
        Vec4_Copy(&arg.pos, &w->unk3D0);
        Mtx_StoreIdentity(&m);
        BtlCharApi_GetRot(objId, &rot);
        Mtx_RotateY(&m, &m, -rot.y);
        Vec3_Sub(&d, &w->unk40, &w->unk50);
        Mtx_MulVec4(&d, &m, &d);
        Vec3_Normalize(&d, &d);
        arg.unk4C = atan2f(d.x, -d.y) / 6.2831853f;
        switch (src->def->id) {
        case 0:
            break;
        case 0x1A3:
            arg.unk58 = 1;
            break;
        case 0x202:
            arg.unk4C = 0.0f;
            break;
        }
    }
    h = EftDisc_Create(&arg);
    piece->h = h;
    if (h == NULL) {
        return;
    }
    if (src->def->id == 0x158) {
        EftDisc_SetModelTex(h, mgr->tex, 0, 0);
    }
    if (src->def->id == 0x202) {
        EftDisc_SetModelTex(piece->h, mgr->tex, 0, 0);
        EftDisc_SetSpin(piece->h, ((f32)rand() / 2147483647.0f * 0.01f + 0.13f) * -3.14159265f);
    }
    piece->flags |= 1;
    w->count++;
    if (w->count >= src->def->count) {
        if (src->def->count >= 2) {
            EftDisc_SetLastHit(piece->h, 1);
        }
        w->flags |= 0x100;
    }
}

/* Per-frame piece handling: fires and releases pieces on the fighter's request bits; returns 1 while any piece lives. */
s32 EftMulti_UpdatePieces(s32 objId, EftJTask *task) {
    EftMulti *w = task->work;
    s32 fire = 0;
    s32 release = 0;
    s32 alive = 0;
    s32 done;
    s32 i;
    EftJPiece *p;

    if (EftShot_TestBits(objId, 2)) {
        fire = 1;
    } else if (EftShot_TestBits(objId, 4)) {
        release = 1;
    } else if (!EftShot_TestBits(objId, 0x10) && !EftShot_TestBits(objId, 0x20)) {
        fire = EftShot_TestBits(objId, 0x40) != 0;
    }
    if (w->flags & 0x200) {
        if (fire) {
            EftMulti_FirePiece(objId, task);
        }
        done = 0;
        if (release) {
            for (i = 0; i < 10; i++) {
                if (!done) {
                    p = &w->piece[i];
                    if (EftDisc_IsAlive(p->h) && !(p->flags & 2)) {
                        EftDisc_Release(p->h, &w->dir);
                        done = 1;
                        EftDisc_SetScale(p->h, w->unk18);
                        p->flags |= 2;
                    }
                }
            }
        }
    } else {
        if (fire) {
            BtlCharApi_GetNodePos(objId, 0x15, &w->unk40);
        }
        if (release) {
            BtlCharApi_GetNodePos(objId, 0x15, &w->unk50);
            EftMulti_FirePiece(objId, task);
        }
    }
    for (i = 0; i < 10; i++) {
        p = &w->piece[i];
        if (EftDisc_IsAlive(p->h)) {
            if (w->flags & 4) {
                EftDisc_SetFlag200(p->h);
            }
            if (w->flags & 1) {
                EftDisc_Kill(p->h);
            }
        }
    }
    for (i = 0; i < 10; i++) {
        p = &w->piece[i];
        if (EftDisc_IsAlive(p->h)) {
            alive = 1;
        } else {
            p->h = NULL;
        }
    }
    return alive;
}

/* 1 when part `part` of group `group` is of kind 5. */
s32 EftMulti_IsPartKind5(EftJSet *set, s32 group, s32 part) {
    return set->parts[set->grp[group].base + part].kind == 5;
}

/* Runs every emitter of the set for this frame, except the parts of kind 5. */
void EftMulti_UpdateParts(s32 objId, EftJTask *task, EftJSet *set, s32 mode) {
    EftMulti *w = task->work;
    s32 g;

    for (g = 0; g < 19; g++) {
        if (*set->mask & (1 << g)) {
            u8 *info = set->grp[g].info;
            s32 j;

            for (j = 0; j < info[1]; j++) {
                if (!EftMulti_IsPartKind5(set, info[0], j)) {
                    s32 res;

                    if (mode == 0) {
                        res = EftEmit_GetFlagsFromReq(set, w->emit, objId, g, j, w->flags & 1, w->flags & 4);
                    } else {
                        res = EftEmit_GetResetFlags(set, w->emit, g, j);
                    }
                    if (w->flags & 0x80) {
                        res = 2;
                    }
                    if (res != 0) {
                        EftEmit_SpawnOwn(set, w->emit, &w->node[0], &w->head, &w->dir, g, j, res, w->scale);
                    }
                }
            }
        }
    }
}

/* Init callback of an item. */
void EftMulti_Init(EftJTask *task, EftJSrc *src) {
    EftJTask *parent = BtlTask_GetParent(task);
    EftMulti *w = task->work;
    EftMultiMgr *mgr = parent->work;
    EftJDef *def;

    memset(w, 0, 0x620);
    w->src = src;
    def = src->def;
    w->speed = def->speed;
    w->homing = def->homing;
    w->unkC = def->scale;
    w->scale = def->scale;
    w->unk14 = w->unkC;
    w->set = &mgr->set;
    EftEmit_InitState(&mgr->set, w->emit);
    w->life = EftEmit_GetEndFrames(w->set);
    if (src->def->flags & 0x8000) {
        w->flags |= 0x200;
    } else {
        w->flags |= 0x400;
    }
    BtlTask_SetOwnerTag(task, src->objId == 0 ? 0x800 : 0x1000);
}

/* Term callback of an item. */
void EftMulti_Term(EftJTask *task) {
    EftMulti *w = task->work;
    EftJSrc *src = w->src;

    EftEmit_TermState(w->set, w->emit);
    if (src->def->unk4 != 0) {
        EftShot_SetHeldFlagA8(src->objId);
    }
}

/* Update callback of an item. */
void EftMulti_Update(EftJTask *task) {
    EftMulti *w = task->work;
    EftJSrc *src = w->src;
    s32 advance = 0;
    s32 alive;

    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    EftEmit_UpdateNodesReq(src, w->node);
    if (!(w->flags & 0x40)) {
        if (EftShot_TestBits(src->objId, 2)) {
            task->state = 0;
            w->flags |= 0x40;
        } else if (EftShot_TestBits(src->objId, 4)) {
            task->state = 2;
            w->flags |= 0x40;
        }
    }
    if (w->flags & 0x40) {
        switch (task->state) {
        case 0:
            if (EftShot_TestBits(src->objId, 2)) {
                EftAim_GetDir(&w->dir, w->unk390, src->objId);
                task->state = 1;
            }
            break;
        case 2:
            if (EftShot_TestBits(src->objId, 4)) {
                Vec4_Copy(&w->unk60, &w->unk3D0);
                Vec4_Copy(&w->head, &w->unk60);
                BtlCharApi_GetNodePos(src->objId, 0x11, &w->unk80);
                EftAim_GetDirKeep(src, &w->dir, &w->head, src->objId);
                Vec3_Scale(&w->vel, &w->dir, w->speed);
                task->state = 3;
            }
            break;
        case 4:
            if (EftShot_TestBits(src->objId, 8)) {
                w->flags |= 9;
                task->state = 5;
            }
            break;
        case 1:
        case 3:
        case 5:
            advance = 1;
            break;
        }
    }
    alive = EftMulti_UpdatePieces(src->objId, task);
    if (!alive && (w->flags & 0x100)) {
        w->flags |= 1;
    }
    if (EftShot_TestBits(src->objId, 0x400)) {
        if (!(w->flags & 8)) {
            w->flags |= 4;
        }
        w->flags |= 1;
    }
    EftMulti_UpdateParts(src->objId, task, w->set, 0);
    if (advance) {
        task->state++;
    }
    if (w->flags & 1) {
        w->timer += 1.0f;
    }
    if (!alive && (w->flags & 2)) {
        BtlTask_SetDead(task);
    } else if (w->flags & 1) {
        if (w->flags & 4) {
            w->flags |= 2;
        } else if (w->timer >= w->life) {
            w->flags |= 2;
        }
    } else if (src->def->flags & 1) {
        if (w->flags & 0x10) {
            EftEmit_UpdateTrailWidth(w->set, w->emit);
        }
    }
}

/* Post-update callback of an item: follows the head moved by the stage test. */
void EftMulti_PostUpdate(EftJTask *task) {
    Vec4 d;
    EftMulti *w = task->work;

    if (BtlScene_IsCharStopped(w->src->objId)) {
        return;
    }
    EftEmit_UpdateAlive(w->set, w->emit);
    if ((u16)(task->hit & 1)) {
        Vec3_Sub(&d, &task->pos, &w->head);
        Vec3_Copy(&w->head, &task->pos);
        Vec3_Add(&w->unk80, &w->unk80, &d);
        if (!(w->flags & 0x20)) {
            w->flags |= 0x20;
        }
    } else {
        w->flags &= ~0x20;
    }
    if (task->hit & 4) {
        w->flags |= 1;
    }
}

/* Reset callback of an item: kills the emitters once and the task. */
void EftMulti_Reset(EftJTask *task) {
    EftMulti *w = task->work;

    if (!(w->flags & 0x80)) {
        w->flags |= 0x80;
        EftEmit_KillAll(w->set, w->emit);
    }
    BtlTask_SetDead(task);
}

/* Draw callback of an item: nothing. */
void EftMulti_Draw(EftJTask *task) {
}

/* Init callback of the manager. */
void EftMultiMgr_Init(EftJTask *task, EftJSrc *src) {
    EftMultiMgr *m = task->work;

    EftShot_Nop(0x530);
    memset(m, 0, 0x530);
    if (src->def->id == 0x158 || src->def->id == 0x202) {
        EftTexSet_Load32(m->tex, BtlScene_GetPackEntry(src->pack, 1));
    }
    EftEmit_LoadSet(src, &m->set, 0, src->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, 0x620);
}

/* Term callback of the manager. */
void EftMultiMgr_Term(EftJTask *task) {
    EftEmit_FreeSet(&((EftMultiMgr *)task->work)->set);
}

/* Update callback of the manager. */
void EftMultiMgr_Update(EftJTask *task) {
    EftMultiMgr *m = task->work;

    EftEmit_BeginFrame(&m->set);
    m->unk20C = 0;
}

/* Reset callback of the manager: nothing. */
void EftMultiMgr_Reset(EftJTask *task) {
}

/* ---- effect type 6, "prop shot" (item class 0x2C38B0, manager class 0x2C3898): a shot that carries a model ---- */

/* Sets the prop up for the effect id and creates its battle object, hidden. */
void EftPropShot_InitProp(EftJTask *task) {
    EftJTask *parent = BtlTask_GetParent(task);
    EftPropShot *w = task->work;
    EftPropShotMgr *mgr = parent->work;
    EftProp *prop = &w->prop;

    memset(prop, 0, sizeof(EftProp));
    switch (w->src->def->id) {
    case 0x165:
        prop->scale = 0.4f;
        prop->flyScale = 1.0f;
        prop->bobStep = (BtlScene_RandF() * 0.1f + 0.05f) * 3.14159265f;
        prop->bobAmp = BtlScene_RandF() * 0.4f + 0.4f;
        prop->flags |= 0x30;
        break;
    case 0x166:
        prop->scale = 0.6f;
        prop->flyScale = 1.0f;
        prop->flags |= 0x10;
        break;
    case 0x19A:
        prop->scale = 1.0f;
        prop->flyScale = 1.0f;
        prop->flags |= 8;
        break;
    case 0x1F7:
        prop->scale = 1.0f;
        prop->flyScale = 1.0f;
        prop->flags |= 0x10;
        break;
    case 0x1F8:
        prop->scale = 3.0f;
        prop->flyScale = 1.0f;
        prop->flags |= 8;
        break;
    case 0x1F9:
        prop->scale = 2.0f;
        prop->flyScale = 1.0f;
        prop->flags |= 8;
        break;
    case 0x2C1:
        prop->scale = 1.0f;
        prop->flyScale = 5.0f;
        prop->flags |= 0x18;
        break;
    }
    prop->obj = EftObj_Create(prop, mgr->model);
    EftObj_SetVisible(prop->obj, 0);
}

/* Moves the prop: in the hand while charging, on the head of the shot once fired; builds its matrix. */
void EftPropShot_UpdateProp(EftJTask *task) {
    Mtx44 m;
    EftPropShot *w = task->work;
    EftJSrc *src = w->src;
    EftProp *prop;
    f32 pitch;
    f32 yaw;

    memset(&m, 0, sizeof(Mtx44));
    prop = &w->prop;
    if (EftShot_TestBits(src->objId, 2)) {
        if (prop->flags & 0x10) {
            prop->flags |= 0x40;
        }
        Vec4_Copy(&w->prop.pos, &w->hand);
        prop->flags |= 1;
    }
    if (EftShot_TestBits(src->objId, 4)) {
        if (!(prop->flags & 0x10)) {
            prop->flags |= 0x40;
        }
        prop->flags = (prop->flags & ~1) | 2;
    }
    if (w->flags & 5) {
        prop->flags &= ~0x40;
    }
    if (prop->flags & 1) {
        if (prop->flags & 0x10) {
            Vec4_Copy(&prop->pos, &w->hand);
        }
        if (prop->flags & 0x20) {
            prop->bobAngle += prop->bobStep;
            prop->offset.y += EftMath_WrapAngle(Mathf_Cos(prop->bobAngle)) * prop->bobAmp;
        }
    }
    if (prop->flags & 2) {
        Vec4_Copy(&prop->pos, &w->head);
        prop->scale = prop->flyScale;
    }
    if (prop->flags & 0x40) {
        pitch = EftMath_WrapAngle(Mathf_Asin(-w->dir.y));
        yaw = EftMath_WrapAngle(atan2f(w->dir.x, w->dir.z));
        Mtx_StoreIdentity(&m);
        Mtx_ScaleDiagUniform(&m, &m, prop->scale);
        if (prop->flags & 8) {
            Mtx_RotateY(&m, &m, 3.14159265f);
        }
        Mtx_RotateX(&m, &m, pitch);
        Mtx_RotateY(&m, &m, yaw);
        Mtx_Translate(&m, &m, &prop->offset);
        Mtx_Translate(&m, &m, &prop->pos);
        EftObj_SetMtx(prop->obj, &m);
    }
}

/* Shows the prop when it is in use and the fighter's effects are not hidden. */
void EftPropShot_ShowProp(EftJTask *task) {
    EftPropShot *w = task->work;
    EftProp *prop = &w->prop;
    EftJSrc *src = w->src;

    if (prop->flags & 0x40) {
        if (!BtlScene_IsEffectHidden(src->objId, 1)) {
            EftObj_SetVisible(prop->obj, 1);
        } else {
            EftObj_SetVisible(prop->obj, 0);
        }
    } else {
        EftObj_SetVisible(prop->obj, 0);
    }
}

/* Destroys the prop's battle object. */
void EftPropShot_FreeProp(EftJTask *task) {
    EftObj_Destroy(((EftPropShot *)task->work)->prop.obj);
}

/* Adds this frame's hit record: two spheres or two capsules at the head and the tail. */
void EftPropShot_AddHit(EftJTask *task) {
    EftPropShot *w = task->work;
    EftJHitRec *rec = EftHit_GetNew();
    f32 r;

    rec->origin = w->origin;
    rec->pos = w->head;
    rec->prev = w->tail;
    rec->vel = w->vel;
    r = w->radius * EftEmit_GetTrailWidth(w->emit);
    rec->task = task;
    rec->src = w->src;
    switch (w->src->def->shape) {
    case 1: {
        void *a = EftHitArena_AllocBox();
        void *b = EftHitArena_AllocBox();

        ColCapsule_Set(a, &w->muzzle, &w->head, r);
        ColCapsule_Set(b, &w->muzzle, &w->tail, r);
        EftHit_SetShapeBoxes(rec, a, b);
        break;
    }
    case 0: {
        void *a = EftHitArena_AllocSphere();
        void *b = EftHitArena_AllocSphere();

        ColSphere_Set(a, &w->head, r);
        ColSphere_Set(b, &w->tail, r);
        EftHit_SetShapeSpheres(rec, a, b);
        break;
    }
    default:
        return;
    }
    EftHit_Add(rec);
}

/* Stage light tint between two request bits of the definition (flag 0x200): fades stage light 0 in along the shot. */
void EftPropShot_UpdateBlur(EftJTask *task) {
    EftJVec pos;
    EftPropShot *w = task->work;
    EftJSrc *src = w->src;

    if (src->def->blurOn != 2) {
        s32 bits[6] = { 2, 4, 8, 0x10, 0x20, 0x40 };
        s32 on = bits[src->def->blurOn];
        s32 off = bits[src->def->blurOff];
        f32 t;
        s32 a;
        s32 b;
        s32 c;

        if (EftShot_TestBits(src->objId, on)) {
            w->flags |= 0x400;
            BtlCharApi_GetNodePos(src->objId, 0x11, &pos);
            EftAim_GetDir(&w->dir, &pos, src->objId);
        }
        if (EftShot_TestBits(src->objId, off)) {
            w->blur = 0.0f;
            w->flags &= ~0x400;
        }
        if (w->flags & 0x400) {
            w->blur += 0.1f;
            if (w->blur > 1.0f) {
                w->blur = 1.0f;
            }
        }
        t = w->blur;
        a = t * 32.0f;
        b = t * 64.0f;
        c = t * 128.0f;
        StgBlur_SetColor0Rgba(0, 0x80, 0x80, 0x80, (u8)a);
        StgBlur_SetColor1Rgba(0, 0x80, 0x80, 0x80, (u8)b);
        StgBlur_SetColor2Rgba(0, 0x80, 0x80, 0x80, (u8)c);
        StgBlur_SetColor3Rgba(0, 0x80, 0x80, 0x80, 0x40);
        StgBlur_SetCenter(0, &w->dir, 0);
    }
}

/* Runs every emitter of the set for this frame. */
void EftPropShot_UpdateParts(s32 objId, EftJTask *task, EftJSet *set, s32 mode) {
    EftPropShot *w = task->work;
    s32 g;

    for (g = 0; g < 19; g++) {
        if (*set->mask & (1 << g)) {
            u8 *info = set->grp[g].info;
            s32 j;

            for (j = 0; j < info[1]; j++) {
                s32 res;

                if (mode == 0) {
                    res = EftEmit_GetFlagsFromReq(set, w->emit, objId, g, j, w->flags & 1, w->flags & 4);
                } else {
                    res = EftEmit_GetResetFlags(set, w->emit, g, j);
                }
                if (w->flags & 0x80) {
                    res = 2;
                }
                if (res != 0) {
                    EftEmit_SpawnOwn(set, w->emit, &w->node[0], &w->head, &w->dir, g, j, res, w->scale);
                }
            }
        }
    }
}

/* Init callback of an item. */
void EftPropShot_Init(EftJTask *task, EftJSrc *src) {
    EftJTask *parent = BtlTask_GetParent(task);
    EftPropShot *w = task->work;
    EftPropShotMgr *mgr = parent->work;
    EftJDef *def;

    memset(w, 0, 0x630);
    w->src = src;
    def = src->def;
    w->speed = def->speed;
    w->unk8 = def->scale;
    w->scale = def->scale;
    w->radius = w->unk8;
    w->set = &mgr->set;
    EftEmit_InitState(&mgr->set, w->emit);
    if (EftEmit_HasWidth2(w->set)) {
        w->speed = EftEmit_GetWidth2(w->emit);
        w->flags |= 0x200;
    }
    w->life = EftEmit_GetEndFrames(w->set);
    EftPropShot_InitProp(task);
    if (w->src->def->id == 0x19A) {
        w->flags |= 0x800;
    }
    BtlTask_SetOwnerTag(task, src->objId == 0 ? 0x800 : 0x1000);
}

/* Term callback of an item. */
void EftPropShot_Term(EftJTask *task) {
    EftPropShot *w = task->work;
    EftJSrc *src = w->src;

    EftEmit_TermState(w->set, w->emit);
    EftPropShot_FreeProp(task);
    if (!(w->flags & 0x800) && src->def->unk4 != 0) {
        EftShot_SetHeldFlagA8(src->objId);
    }
}

/* Update callback of an item: charge, fire, fly (with homing), end. */
void EftPropShot_Update(EftJTask *task) {
    EftJVec pos;
    EftPropShot *w = task->work;
    EftJSrc *src = w->src;
    s32 advance = 0;

    memset(&pos, 0, sizeof(pos));
    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    if (src->def->unk4 == 0) {
        EftShot_PlayFireSound(src->objId, src);
    }
    EftEmit_UpdateNodesReq(src, w->node);
    if (!(w->flags & 0x40)) {
        if (EftShot_TestBits(src->objId, 2)) {
            EftAim_GetDir(&w->dir, &w->hand, src->objId);
            if (w->flags & 0x100) {
                BtlCharApi_GetNodePos(src->objId, 0x30, &pos);
                Vec3_Sub(&w->dir, &w->hand, &pos);
                Vec3_Normalize(&w->dir, &w->dir);
            }
            task->state = 0;
            w->flags |= 0x40;
        } else if (EftShot_TestBits(src->objId, 4)) {
            task->state = 0;
            w->flags |= 0x40;
        }
    }
    if (w->flags & 0x40) {
        switch (task->state) {
        case 0:
            if (w->flags & 0x100) {
                BtlCharApi_GetNodePos(src->objId, 0x30, &pos);
                Vec3_Sub(&w->dir, &w->hand, &pos);
                Vec3_Normalize(&w->dir, &w->dir);
            }
            if (EftShot_TestBits(src->objId, 4)) {
                w->flags |= 0x10;
                Vec4_Copy(&w->origin, &w->muzzle);
                Vec4_Copy(&w->head, &w->origin);
                BtlCharApi_GetNodePos(src->objId, 0x11, &w->tail);
                EftAim_GetDirKeep(src, &w->dir, &w->head, src->objId);
                Vec3_Scale(&w->vel, &w->dir, w->speed);
                if (w->flags & 0x800) {
                    BtlCharApi_ObjSetFlag100(src->objId, 1);
                }
                task->state = 1;
            }
            break;
        case 2:
            if (!(w->flags & 0x20)) {
                Vec4_Copy(&w->tail, &w->head);
            }
            if (!(w->src->def->flags & 2)) {
                if (!(w->flags & 0x20)) {
                    if (src->def->homing > 0.0f) {
                        EftAim_Home(&w->dir, &w->head, &w->dir, src->objId, w->speed, src->def->homing);
                        Vec3_Scale(&w->vel, &w->dir, w->speed);
                    }
                    Vec3_Add(&w->head, &w->head, &w->vel);
                } else {
                    Vec3_Copy(&w->head, &task->pos);
                }
            }
            if (EftShot_TestBits(src->objId, 8)) {
                w->flags |= 9;
                task->state = 3;
            }
            break;
        case 1:
        case 3:
            advance = 1;
            break;
        }
    }
    if (EftShot_TestBits(src->objId, 0x400)) {
        if (!(w->flags & 8)) {
            w->flags |= 4;
        }
        w->flags |= 1;
    }
    EftPropShot_UpdateParts(src->objId, task, w->set, 0);
    EftPropShot_UpdateProp(task);
    if (src->def->flags & 0x200) {
        EftPropShot_UpdateBlur(task);
    }
    if (advance) {
        task->state++;
    }
    if (w->flags & 1) {
        w->timer += 1.0f;
    }
    if ((w->flags & 1) && !(w->flags & 2)) {
        if ((w->flags & 0x800) && (w->timer >= w->life || (w->flags & 4))) {
            if (!(w->flags & 0x1000)) {
                if (src->def->unk4 != 0) {
                    EftShot_SetHeldFlagA8(src->objId);
                }
                w->flags |= 0x1000;
            } else if (!BtlCharApi_ObjHasFlags42(src->objId)) {
                w->flags &= ~0x1000;
            }
        }
    }
    if (w->flags & 2) {
        if (w->flags & 0x800) {
            BtlCharApi_ObjSetFlag100(src->objId, 0);
        }
        BtlTask_SetDead(task);
    } else if (w->flags & 1) {
        if (!(w->flags & 0x1000)) {
            if (w->flags & 4) {
                w->flags |= 2;
            } else if (w->timer >= w->life) {
                w->flags |= 2;
            }
        }
    } else if (src->def->flags & 1) {
        if (w->flags & 0x10) {
            EftEmit_UpdateTrailWidth(w->set, w->emit);
            EftPropShot_AddHit(task);
        }
    }
}

/* Post-update callback of an item: shows the prop and follows the head moved by the stage test. */
void EftPropShot_PostUpdate(EftJTask *task) {
    EftJVec d;
    EftPropShot *w = task->work;
    EftJSrc *src = w->src;

    EftPropShot_ShowProp(task);
    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    EftEmit_UpdateAlive(w->set, w->emit);
    if (task->unk4 & 1) {
        EftEmit_MarkKind6(w->set, w->emit);
    }
    if ((u16)(task->hit & 1)) {
        Vec3_Sub(&d, &task->pos, &w->head);
        Vec3_Copy(&w->head, &task->pos);
        Vec3_Add(&w->tail, &w->tail, &d);
        if (!(w->flags & 0x20)) {
            w->flags |= 0x20;
        }
    } else {
        w->flags &= ~0x20;
    }
    if (task->hit & 4) {
        w->flags |= 1;
    }
}

/* Reset callback of an item: kills the emitters once and the task. */
void EftPropShot_Reset(EftJTask *task) {
    EftPropShot *w = task->work;

    if (!(w->flags & 0x80)) {
        w->flags |= 0x80;
        EftEmit_KillAll(w->set, w->emit);
    }
    BtlTask_SetDead(task);
}

/* Draw callback of an item: nothing. */
void EftPropShot_Draw(EftJTask *task) {
}

/* Init callback of the manager: picks the prop model (by costume for id 0x19A) and builds the emitter set. */
void EftPropShotMgr_Init(EftJTask *task, EftJSrc *src) {
    EftPropShotMgr *m = task->work;

    EftShot_Nop(0x328);
    memset(m, 0, 0x328);
    if (src->def->id == 0x19A) {
        u8 costume = BtlCharApi_GetCostume(src->objId20);

        m->model = BtlScene_GetPackEntry(BtlScene_GetPackEntry(src->pack, 1), costume + 1);
    } else {
        m->model = BtlScene_GetPackEntry(src->pack, 1);
    }
    EftEmit_LoadSet(src, &m->set, 0, src->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, 0x630);
}

/* Term callback of the manager. */
void EftPropShotMgr_Term(EftJTask *task) {
    EftEmit_FreeSet(&((EftPropShotMgr *)task->work)->set);
}

/* Update callback of the manager. */
void EftPropShotMgr_Update(EftJTask *task) {
    EftEmit_BeginFrame(&((EftPropShotMgr *)task->work)->set);
}

/* Reset callback of the manager: nothing. */
void EftPropShotMgr_Reset(EftJTask *task) {
}

/* ---- effect type 0, "blast" (item class 0x2C38E0, manager class 0x2C38C8): the plain ki blast / beam ---- */

/* Adds this frame's hit record: two spheres or two boxes at the head and the tail. */
void EftBlast_AddHit(EftJTask *task) {
    EftBlast *w = task->work;
    EftJHitRec *rec = EftHit_GetNew();
    f32 r;

    r = w->radius * EftEmit_GetTrailWidth(w->emit);
    rec->origin = w->origin;
    rec->pos = w->head;
    rec->prev = w->tail;
    rec->vel = w->vel;
    rec->task = task;
    rec->src = w->src;
    switch (w->src->def->shape) {
    case 1: {
        void *a = EftHitArena_AllocBox();
        void *b = EftHitArena_AllocBox();

        ColCapsule_Set(a, &w->muzzle, &w->head, r);
        ColCapsule_Set(b, &w->muzzle, &w->tail, r);
        EftHit_SetShapeBoxes(rec, a, b);
        break;
    }
    case 0: {
        void *a = EftHitArena_AllocSphere();
        void *b = EftHitArena_AllocSphere();

        ColSphere_Set(a, &w->head, r);
        ColSphere_Set(b, &w->tail, r);
        EftHit_SetShapeSpheres(rec, a, b);
        break;
    }
    default:
        return;
    }
    EftHit_Add(rec);
}

/* Stage blur tint between two events of the definition (flag 0x200). */
void EftBlast_UpdateBlur(EftJTask *task) {
    EftJVec pos;
    EftBlast *w = task->work;
    EftJSrc *src = w->src;

    if (src->def->blurOn != 2) {
        s32 bits[6] = { 2, 4, 8, 0x10, 0x20, 0x40 };
        s32 on = bits[src->def->blurOn];
        s32 off = bits[src->def->blurOff];
        f32 t;
        s32 a;
        s32 b;
        s32 c;

        if (EftShot_TestBits(src->objId, on)) {
            w->flags |= 0x400;
            BtlCharApi_GetNodePos(src->objId, 0x11, &pos);
            EftAim_GetDir(&w->blurDir, &pos, src->objId);
        }
        if (EftShot_TestBits(src->objId, off)) {
            w->blur = 0.0f;
            w->flags &= ~0x400;
        }
        if (w->flags & 0x400) {
            w->blur += 0.1f;
            if (w->blur > 1.0f) {
                w->blur = 1.0f;
            }
        }
        t = w->blur;
        a = t * 32.0f;
        b = t * 64.0f;
        c = t * 128.0f;
        StgBlur_SetColor0Rgba(0, 0x80, 0x80, 0x80, (u8)a);
        StgBlur_SetColor1Rgba(0, 0x80, 0x80, 0x80, (u8)b);
        StgBlur_SetColor2Rgba(0, 0x80, 0x80, 0x80, (u8)c);
        StgBlur_SetColor3Rgba(0, 0x80, 0x80, 0x80, 0x40);
        StgBlur_SetCenter(0, &w->blurDir, 0);
    }
}

/* Runs every emitter of the set for this frame. */
void EftBlast_UpdateParts(s32 objId, EftJTask *task, EftJSet *set, s32 mode) {
    EftBlast *w = task->work;
    s32 g;

    for (g = 0; g < 19; g++) {
        if (*set->mask & (1 << g)) {
            u8 *info = set->grp[g].info;
            s32 j;

            for (j = 0; j < info[1]; j++) {
                s32 res;

                if (mode == 0) {
                    res = EftEmit_GetFlagsFromReq(set, w->emit, objId, g, j, w->flags & 1, w->flags & 4);
                } else {
                    res = EftEmit_GetResetFlags(set, w->emit, g, j);
                }
                if (w->flags & 0x80) {
                    res = 2;
                }
                if (res != 0) {
                    EftEmit_SpawnOwn(set, w->emit, &w->node[0], &w->head, &w->dir, g, j, res, w->drawScale);
                }
            }
        }
    }
}

/*
 * Init callback of an item: copies speed and scale from the definition, applies the per-id cases (0x268: scale
 * + 0.3 * BtlCharApi_GetUnkE5CRatio and flags 0x1800; 0x2CD: scale * 0.8 + min(BtlCharApi_GetUnkE60Ratio, 0.7)
 * and flag 0x800; 0x14B: flag 0x800), binds the manager's emitter set, takes the speed from the set's second
 * width track when it has one (flag 0x200), reads the end time and tags the task with its character's reset bit.
 *
 * `src` is a local copy of the argument, declared behind the locals that a call initialises (the callback's
 * argument was presumably untyped in the original). With the parameter used directly, gcse's PRE makes one pseudo
 * of the loads of src->def in front of the 0x268 and 0x2CD tests (the load is partially redundant on the edge that
 * skips the 0x268 block) and the registers of all three tests change (9 instructions); the original shows no PRE
 * there (bnel with `li a0,717` in the slot, target behind both loads). The same form fixes EftVolley_Init
 * (eft_h.c), where decomp-permuter found it. It replaces the volatile read this function used as a stand-in.
 */
void EftBlast_Init(EftJTask *task, EftJSrc *arg) {
    EftJTask *parent = BtlTask_GetParent(task);
    EftBlast *w = task->work;
    EftJSrc *src = arg;
    EftBlastMgr *mgr = parent->work;
    EftJDef *def;

    memset(w, 0, 0x5C0);
    w->src = src;
    def = src->def;
    w->speed = def->speed;
    w->scale = def->scale;
    if (src->def->id == 0x268) {
        f32 ratio = BtlCharApi_GetUnkE5CRatio(src->objId);

        w->ratio = ratio;
        w->scale += ratio * 0.3f;
        w->flags |= 0x1800;
    }
    if (src->def->id == 0x2CD) {
        f32 ratio = BtlCharApi_GetUnkE60Ratio(src->objId);
        f32 add;

        w->ratio = ratio;
        add = ratio;
        if (add > 0.7f) {
            add = 0.7f;
        }
        w->flags |= 0x800;
        w->scale = w->scale * 0.8f + add;
    }
    w->drawScale = w->scale;
    w->radius = w->scale;
    if (src->def->id == 0x14B) {
        w->flags |= 0x800;
    }
    w->set = &mgr->set;
    EftEmit_InitState(&mgr->set, w->emit);
    if (EftEmit_HasWidth2(w->set)) {
        w->speed = EftEmit_GetWidth2(w->emit) * def->scale;
        w->flags |= 0x200;
    }
    w->life = EftEmit_GetEndFrames(w->set);
    BtlTask_SetOwnerTag(task, src->objId == 0 ? 0x800 : 0x1000);
}

/* Term callback of an item. */
void EftBlast_Term(EftJTask *task) {
    EftBlast *w = task->work;
    EftJSrc *src = w->src;

    EftEmit_TermState(w->set, w->emit);
    if (src->def->unk4 != 0) {
        EftShot_SetHeldFlagA8(src->objId);
    }
}

/* Update callback of an item: charge (grows with the fighter's charge), fire, fly (with homing), end. */
void EftBlast_Update(EftJTask *task) {
    EftJVec pos;
    EftBlast *w = task->work;
    EftJSrc *src = w->src;
    s32 advance = 0;

    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    if (src->def->unk4 == 0) {
        EftShot_PlayFireSound(src->objId, src);
    }
    EftEmit_UpdateNodesReq(src, w->node);
    if (!(w->flags & 0x40)) {
        if (EftShot_TestBits(src->objId, 2)) {
            EftAim_GetDir(&w->dir, &w->hand, src->objId);
            if (w->flags & 0x100) {
                BtlCharApi_GetNodePos(src->objId, 0x30, &pos);
                Vec3_Sub(&w->dir, &w->hand, &pos);
                Vec3_Normalize(&w->dir, &w->dir);
            }
            task->state = 0;
            w->flags |= 0x40;
        } else if (EftShot_TestBits(src->objId, 4)) {
            task->state = 0;
            w->flags |= 0x40;
        }
    }
    if (w->flags & 0x40) {
        switch (task->state) {
        case 0:
            if (!(w->flags & 0x800)) {
                f32 ratio = BtlCharApi_GetUnkE44(src->objId);

                w->ratio = ratio;
                w->drawScale = w->scale * (ratio * 0.5f + 1.0f);
                w->radius = w->scale * (ratio * 0.0f + 1.0f);
            }
            if (w->flags & 0x100) {
                BtlCharApi_GetNodePos(src->objId, 0x30, &pos);
                Vec3_Sub(&w->dir, &w->hand, &pos);
                Vec3_Normalize(&w->dir, &w->dir);
            }
            if (EftShot_TestBits(src->objId, 4)) {
                w->flags |= 0x10;
                Vec4_Copy(&w->origin, &w->muzzle);
                Vec4_Copy(&w->head, &w->origin);
                BtlCharApi_GetNodePos(src->objId, 0x11, &w->tail);
                EftAim_GetDirKeep(src, &w->dir, &w->head, src->objId);
                Vec3_Scale(&w->vel, &w->dir, w->speed);
                if (w->flags & 0x1000) {
                    if (w->ratio > 0.0f) {
                        w->scale = src->def->scale + w->ratio * 0.3f * 4.0f;
                    }
                }
                task->state = 1;
            }
            break;
        case 1:
            advance = 1;
            break;
        case 2:
            if ((w->flags & 0x1000) && w->ratio > 0.0f && w->drawScale < w->scale) {
                w->drawScale += 0.1f;
                if (w->drawScale > w->scale) {
                    w->drawScale = w->scale;
                }
            }
            if (!(w->flags & 0x20)) {
                Vec4_Copy(&w->tail, &w->head);
            }
            if (!(w->src->def->flags & 2)) {
                if (!(w->flags & 0x20)) {
                    if (src->def->homing > 0.0f) {
                        EftAim_Home(&w->dir, &w->head, &w->dir, src->objId, w->speed, src->def->homing);
                        Vec3_Scale(&w->vel, &w->dir, w->speed);
                    }
                    Vec3_Add(&w->head, &w->head, &w->vel);
                } else {
                    Vec3_Copy(&w->head, &task->pos);
                }
            }
            if (EftShot_TestBits(src->objId, 8)) {
                w->flags |= 9;
                task->state = 3;
            }
            break;
        case 3:
            if (src->def->flags & 0x20) {
                Vec3_Add(&w->head, &w->head, &w->vel);
            }
            break;
        }
    }
    if (EftShot_TestBits(src->objId, 0x400)) {
        if (!(w->flags & 8)) {
            w->flags |= 4;
        }
        w->flags |= 1;
    }
    EftBlast_UpdateParts(src->objId, task, w->set, 0);
    if (w->flags & 0x10) {
        if (w->flags & 0x200) {
            EftEmit_UpdateWidth2(w->set, w->emit);
            w->speed = EftEmit_GetWidth2(w->emit) * src->def->scale;
            Vec3_Scale(&w->vel, &w->dir, w->speed);
        }
    }
    if (src->def->flags & 0x200) {
        EftBlast_UpdateBlur(task);
    }
    if (advance) {
        task->state++;
    }
    if (w->flags & 1) {
        w->timer += 1.0f;
    }
    if (w->flags & 2) {
        BtlTask_SetDead(task);
    } else if (w->flags & 1) {
        if (w->flags & 4) {
            w->flags |= 2;
        } else if (w->timer >= w->life) {
            w->flags |= 2;
        }
    } else if (src->def->flags & 1) {
        if (w->flags & 0x10) {
            EftEmit_UpdateTrailWidth(w->set, w->emit);
            EftBlast_AddHit(task);
        }
    }
}

/* Post-update callback of an item: reacts to what the hit pass reported and follows the corrected head. */
void EftBlast_PostUpdate(EftJTask *task) {
    EftJVec d;
    EftBlast *w = task->work;
    EftJSrc *src = w->src;

    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    EftEmit_UpdateAlive(w->set, w->emit);
    if (task->unk4 & 1) {
        EftEmit_MarkKind6(w->set, w->emit);
        if (src->def->flags & 0x10000) {
            w->flags &= ~0x10;
        }
        w->flags |= 0x2000;
    } else if (task->unk4 & 4) {
        w->flags |= 0x4000;
    }
    if ((u16)(task->hit & 1)) {
        Vec3_Sub(&d, &task->pos, &w->head);
        Vec3_Copy(&w->head, &task->pos);
        Vec3_Add(&w->tail, &w->tail, &d);
        if (!(w->flags & 0x20)) {
            w->flags |= 0x20;
        }
    } else {
        w->flags &= ~0x20;
    }
    if (task->hit & 4) {
        w->flags |= 1;
    }
}

/* Reset callback of an item: kills the emitters once and the task. */
void EftBlast_Reset(EftJTask *task) {
    EftBlast *w = task->work;

    if (!(w->flags & 0x80)) {
        w->flags |= 0x80;
        EftEmit_KillAll(w->set, w->emit);
    }
    BtlTask_SetDead(task);
}

/* Draw callback of an item: nothing. */
void EftBlast_Draw(EftJTask *task) {
}

/* Init callback of the manager. */
void EftBlastMgr_Init(EftJTask *task, EftJSrc *src) {
    EftBlastMgr *m = task->work;

    EftShot_Nop(0x324);
    memset(m, 0, 0x324);
    EftEmit_LoadSet(src, &m->set, 0, src->pack, 0, 4);
    BtlTask_CreateChildList(task, 2, 0x5C0);
}

/* Term callback of the manager. */
void EftBlastMgr_Term(EftJTask *task) {
    EftEmit_FreeSet(&((EftBlastMgr *)task->work)->set);
}

/* Update callback of the manager. */
void EftBlastMgr_Update(EftJTask *task) {
    EftEmit_BeginFrame(&((EftBlastMgr *)task->work)->set);
}

/* Reset callback of the manager: nothing. */
void EftBlastMgr_Reset(EftJTask *task) {
}

/* ---- effect type 2, "shots" (item class 0x2C3910, manager class 0x2C38F8): helpers; callbacks in eft_k.c ---- */

/* Creates the item's model object, hidden. */
void EftShotTech_InitModel(EftJTask *task) {
    EftJTask *parent = BtlTask_GetParent(task);
    EftJShotMgr *mgr = parent->work;
    EftJShotTech *w = task->work;
    EftJShotModel *model = &w->model;

    model->pack = mgr->modelPack;
    model->objId = EftObj_Create(w->model.arg, model->pack);
    model->unk36 = -1;
    EftObj_SetVisible(model->objId, 0);
    EftObj_Nop(model->objId, 0);
}

/* Places the model object (scale, rotation, position) and shows it. */
void EftShotTech_SetModelPose(EftJTask *task, void *pos, void *angles) {
    Mtx44 m;
    EftJShotTech *w = task->work;
    EftJShotModel *model;

    Mtx_StoreIdentity(&m);
    Mtx_ScaleDiagUniform(&m, &m, w->scale);
    model = &w->model;
    Mtx_RotateZXY(&m, &m, angles);
    Mtx_Translate(&m, &m, pos);
    EftObj_SetMtx(model->objId, &m);
    EftObj_SetVisible(model->objId, 1);
}

/* Destroys the model object. */
void EftShotTech_FreeModel(EftJTask *task) {
    EftObj_Destroy(((EftJShotTech *)task->work)->model.objId);
}

/* Computes the firing direction for the set's aim mode into `dir` and the velocity from it. */
void EftShotTech_CalcDir(EftJTask *task, EftJVec *dir) {
    EftJVec p;
    Mtx44 m;
    EftJShotTech *w = task->work;
    EftJSrc *src = w->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);

    switch (EftEmit_GetHead25(w->mgr)) {
    case 0:
        BtlCharApi_GetNodePos(opp, 3, &p);
        Vec3_Sub(dir, &p, &w->pos);
        dir->w = 1.0f;
        Vec3_Normalize(dir, dir);
        break;
    case 1:
        BtlCharApi_GetNodePos(src->objId, 0x2E, dir);
        BtlCharApi_GetNodePos(src->objId, 3, &p);
        Vec3_Sub(dir, &p, dir);
        dir->w = 1.0f;
        Vec3_Normalize(dir, dir);
        break;
    case 2:
        EftAim_GetDir(dir, &w->pos, src->objId);
        break;
    case 3:
        BtlCharApi_GetNodePos(src->objId, 0x22, dir);
        BtlCharApi_GetNodePos(src->objId, 0x23, &p);
        Vec3_Sub(dir, &p, dir);
        dir->w = 1.0f;
        Vec3_Normalize(dir, dir);
        break;
    case 4:
        BtlCharApi_GetNodePos(src->objId, 0x14, dir);
        BtlCharApi_GetNodePos(src->objId, 0x15, &p);
        Vec3_Sub(dir, &p, dir);
        dir->w = 1.0f;
        Vec3_Normalize(dir, dir);
        break;
    case 5:
        BtlCharApi_GetNodeMtx(src->objId, 0x36, &m);
        Vec4_Set(dir, -m.m[2][0], -m.m[2][1], -m.m[2][2], 1.0f);
        Vec3_Normalize(dir, dir);
        break;
    }
    Vec3_Scale(&w->vel, dir, w->speed);
}

/* Returns the first free blast slot (cleared), or NULL. */
EftJShotSlot *EftShotTech_AllocShot(EftJShotTech *w) {
    EftJShotSlot *slot = w->shots;
    EftJShotSlot *res = NULL;
    s32 i;

    i = 0;
    while (i < 14) {
        i++;
        if (slot->h == NULL) {
            res = slot;
            memset(slot, 0, sizeof(EftJShotSlot));
            break;
        }
        slot++;
    }
    return res;
}

/* Fires the blast objects of one event: one, or the definition's burst count for kind 1. */
void EftShotTech_FireShots(s32 objId, EftJTask *task, s32 evt, s32 kind, s32 evtIdx, f32 scale, f32 speed) {
    EftJVec pos;
    EftJShotArg arg;
    EftJShotTech *w = task->work;
    EftJSrc *src = w->src;
    s32 n = 1;
    s32 i;

    if (kind == 1) {
        n = src->def->burst;
        if (n <= 0) {
            n = kind;
        }
    }
    for (i = 0; i < n; i++) {
        EftJShotSlot *slot = EftShotTech_AllocShot(w);

        if (slot != NULL) {
            s32 alt = 0;
            s32 count;

            if ((u8)(EftEmit_GetHead26(w->mgr) & 1)) {
                alt = kind == 4;
            }
            BtlCharApi_GetNodePos(src->objId, EftShot_GetAttrKind(src->objId, evt), &pos);
            count = src->def->count;
            arg = (EftJShotArg){
                src, w->mgr, w->nodes, &pos, &w->shotDir, w->shotCount, count, kind, evtIdx, 0, scale, speed
            };
            if (alt) {
                arg.pos = &w->muzzlePos;
                arg.dir = &w->muzzleDir;
            }
            slot->h = EftBlastObj_Create(&arg);
            slot->flags |= 1;
            EftBlastObj_SetNoHit(slot->h, 1);
            if (alt) {
                EftBlastObj_SetFrozen(slot->h, 1);
            }
            w->flags |= 0x40000;
            w->shotCount++;
        }
    }
}

/* Clears the slots whose blast object is gone; returns 1 while any is left. */
s32 EftShotTech_ReapShots(EftJTask *task) {
    EftJShotTech *w = task->work;
    s32 alive = 0;
    s32 i;

    for (i = 0; i < 14; i++) {
        EftJShotSlot *slot = &w->shots[i];

        if (EftBlastObj_IsAlive(slot->h)) {
            alive = 1;
        } else {
            slot->h = NULL;
        }
    }
    return alive;
}

/* Fires on the fighter's events and ages the blast objects; returns 1 while any is left. */
s32 EftShotTech_UpdateShots(s32 objId, EftJTask *task) {
    s32 kind = -1;
    EftJShotTech *w;
    s32 evt = 0;
    s32 evtIdx = 0;
    EftJDef *def;
    f32 scale;
    f32 speed;
    s32 i;

    w = task->work;
    def = w->src->def;
    scale = w->scale;
    speed = w->speed;
    if (EftShot_TestBits(objId, 2)) {
        kind = 0;
        evt = 2;
        EftShotTech_CalcDir(task, &w->shotDir);
    } else if (EftShot_TestBits(objId, 4)) {
        kind = 1;
        evtIdx = 1;
        evt = 4;
        if (EftShot_TestBits(objId, 0x40)) {
            scale *= 1.2f;
            speed *= 0.8f;
        }
        EftShotTech_CalcDir(task, &w->shotDir);
    } else if (EftShot_TestBits(objId, 0x10)) {
        kind = 3;
        evtIdx = 2;
        evt = 0x10;
        EftShotTech_CalcDir(task, &w->shotDir);
    } else if (EftShot_TestBits(objId, 0x20)) {
        kind = 4;
        evtIdx = 3;
        evt = 0x20;
        EftShotTech_CalcDir(task, &w->shotDir);
    } else if (EftShot_TestBits(objId, 0x40)) {
        kind = 5;
        evtIdx = 4;
        evt = 0x40;
        EftShotTech_CalcDir(task, &w->shotDir);
    }
    if (kind >= 0) {
        if ((EftEmit_GetPhaseMask(w->mgr) >> kind) & 1) {
            EftShotTech_FireShots(objId, task, evt, kind, evtIdx, scale, speed);
        }
    }
    for (i = 0; i < 14; i++) {
        EftJShotSlot *slot = &w->shots[i];

        if (EftBlastObj_IsAlive(slot->h)) {
            if (!(slot->flags & 8)) {
                if (EftEmit_GetHead26(w->mgr) & 0x10) {
                    EftBlastObj_SetTarget(slot->h, 1, &w->pos);
                }
                if (slot->timer >= def->shotLife) {
                    slot->flags |= 8;
                    EftBlastObj_Stop(slot->h);
                } else {
                    slot->timer += 1.0f;
                }
            }
            if (w->flags & 1) {
                EftBlastObj_Stop(slot->h);
            }
        }
    }
    return EftShotTech_ReapShots(task);
}

/* Runs every emitter of the set for this frame; the "muzzle" emitters (kind 4) use the muzzle position. */
void EftShotTech_UpdateParts(s32 objId, EftJTask *task, EftJSet *set) {
    EftJShotTech *w = task->work;
    s32 g;

    for (g = 0; g < 19; g++) {
        if (*set->mask & (1 << g)) {
            u8 *info = set->grp[g].info;
            s32 j;

            for (j = 0; j < info[1]; j++) {
                s32 res;
                s32 alt;

                if (w->flags & 0x20000) {
                    if (EftEmit_IsPartDeferred(set, info[0], j)) {
                        continue;
                    }
                }
                res = EftEmit_GetFlagsFromReq(set, w->emit, objId, g, j, w->flags & 1, w->flags & 4);
                if (res != 0) {
                    alt = 0;
                    if ((u8)(EftEmit_GetHead26(w->mgr) & 1)) {
                        alt = set->parts[set->grp[info[0]].base + j].type == 4;
                    }
                    if (alt) {
                        EftEmit_SpawnOwn(set, w->emit, w->nodes, &w->muzzlePos, &w->muzzleDir, g, j, res, w->scale);
                    } else {
                        EftEmit_SpawnOwn(set, w->emit, w->nodes, &w->pos, &w->dir, g, j, res, w->scale);
                    }
                }
            }
        }
    }
}

/* Samples the launch node and starts the target point there. */
void EftShotTech_Start(EftJTask *task, s32 evt) {
    EftJShotTech *w = task->work;
    EftJSrc *src = w->src;

    if (w->flags & 0x2000) {
        if (EftShot_TestBits(src->objId, 0x10)) {
            w->flags |= 0x4000;
        }
        w->node = 0x15;
    } else {
        w->node = EftShot_GetAttrKind(src->objId, evt);
    }
    BtlCharApi_GetNodePos(src->objId, w->node, &w->start);
    Vec4_Copy(&w->unk5A0, &w->start);
    Vec4_Copy(&w->pos, &w->unk5A0);
    Vec4_Copy(&w->prev, &w->pos);
    EftShotTech_CalcDir(task, &w->dir);
}

/* White screen flash when the opponent raises event 0x40 (not with definition flag 0x400000). */
void EftShotTech_UpdateFlash(EftJTask *task) {
    EftJFlashArg arg;
    EftJSrc *src = ((EftJShotTech *)task->work)->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);

    if (!(src->def->flags & 0x400000) && EftShot_TestBits(opp, 0x40)) {
        arg = (EftJFlashArg){ { 255.0f, 255.0f, 255.0f, 255.0f }, 0.5f, 0, 1.0f, src->objId, 0 };
        if (src->def->id == 0x27C) {
            arg.unk20 = 1;
        }
        EftFlash_Start(&arg);
    }
}

/* Starts five node effects on the fighter when the opponent raises event 0x40. */
void EftShotTech_UpdateNodeFx(EftJTask *task) {
    EftJSrc *src = ((EftJShotTech *)task->work)->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);

    if (EftShot_TestBits(opp, 0x40)) {
        s32 init[10] = { 0x47, 0, 0x47, 3, 0x47, 0xA, 0x47, 0xE, 0x47, 0x15 };

        EftDelaySe_Start(src->objId, init, 5);
    }
}

/* Stage blur tint between the opponent's events 0x10 and 0x20 (definition flag 0x200). */
void EftShotTech_UpdateBlur(EftJTask *task) {
    EftJVec pos;
    EftJShotTech *w = task->work;
    EftJSrc *src = w->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);
    f32 t;
    s32 a;
    s32 b;
    s32 c;

    if (src->def->flags & 0x200) {
        if (EftShot_TestBits(opp, 0x10)) {
            w->flags |= 0x400;
            BtlCharApi_GetNodePos(src->objId, 0x11, &pos);
            EftAim_GetDir(&w->blurDir, &pos, src->objId);
        }
        if (EftShot_TestBits(opp, 0x20)) {
            w->blur = 0.0f;
            w->flags &= ~0x400;
        }
        if (w->flags & 0x400) {
            w->blur += 0.1f;
            if (w->blur > 1.0f) {
                w->blur = 1.0f;
            }
        }
    }
    t = w->blur;
    a = t * 8.0f;
    b = t * 32.0f;
    c = t * 128.0f;
    StgBlur_SetColor0Rgba(0, 0x80, 0x80, 0x80, (u8)a);
    StgBlur_SetColor1Rgba(0, 0x80, 0x80, 0x80, (u8)b);
    StgBlur_SetColor2Rgba(0, 0x80, 0x80, 0x80, (u8)c);
    StgBlur_SetColor3Rgba(0, 0x80, 0x80, 0x80, 0x40);
    StgBlur_SetCenter(0, &w->blurDir, 0);
}

/* Starts a sub-effect on the opponent's event 0x80 and stops the last one on event 0x100. */
void EftShotTech_UpdateSubs(EftJTask *task) {
    EftJShotTech *w = task->work;
    EftJSrc *src = w->src;
    s32 opp = BtlCharApi_GetOpponentObjId(src->objId);

    if (EftShot_TestBits(opp, 0x80)) {
        s32 n = w->subCount;
        EftJDef *def = src->def;
        s32 kind = def->subKind[n];
        s32 subArg = def->subArg;
        f32 angle = EftMath_WrapAngle(def->subAngle[n] * 3.14159265f / 180.0f);

        if (kind >= 0) {
            w->subs[n] = EftStreak_Start(src->objId, kind, subArg, angle);
            w->flags |= 0x80000;
        }
        w->subCount++;
    }
    if (EftShot_TestBits(opp, 0x100)) {
        s32 i = w->subCount - 1;

        if (i < 0) {
            i = 0;
        }
        if (w->flags & 0x80000) {
            EftStreak_Stop(w->subs[i]);
            w->subs[i] = NULL;
            w->flags &= ~0x80000;
        }
    }
}
