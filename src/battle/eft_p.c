#include "common.h"
#include "battle/eft_p.h"

/*
 * Disc projectile, tail of the module: 0x16C2E0..0x16DCA0 (29 functions, 28 matching; EftDisc_Home is left in
 * assembly). The module starts in the file before
 * this one (its manager, the init callback 0x16B6B8 and the update callback 0x16BAC8 are there); this file has
 * the other four callbacks of the task class D_002C3B38, the helpers the update calls, the list of held discs
 * and the two interfaces that create and steer discs:
 *
 *   - the fighter effect layer (btl_char_fx.c): EftDisc_SpawnHeld (request 0x17 with ki blast type 4: a disc
 *     grows in the hand), EftDisc_Throw (the ki blast fire event, type 4: throws the held disc, or creates a
 *     flying one when none is held), EftDisc_SpawnFromNode (type 5: a flying disc at a model node);
 *   - the multi-piece technique EftMulti (eft_j.c): EftDisc_Create, EftDisc_Release, EftDisc_Kill,
 *     EftDisc_SetSpin, EftDisc_SetScale, EftDisc_SetModelTex, EftDisc_SetLastHit, EftDisc_SetFlag200,
 *     EftDisc_IsAlive. Each of these checks that the handle is a task whose class update is 0x16BAC8.
 *
 * SIMULATION: a flying disc adds one hit record per frame (EftDisc_AddHit) with two spheres, at its position and
 * its previous position. A ki blast disc's record is type 0 with radius scale x 4.5 and points at the disc's
 * copy of the fighter's ki blast parameters; a technique piece's record is type 1 with radius = the emitter
 * set's trail width x 3 (so that animation track is simulation input), and only when the definition's flag 1 is
 * set. The post-update ends the disc when the hit pass left any of the result bits 0..5 in the task.
 *
 * The value EftDisc_SetSpin stores (+0x480, the one EftMulti draws from libc rand() for technique 0x202, and the
 * init callback draws from rand() for kinds 0 and 1) is added each frame to one of two model rotation angles
 * (+0x484 / +0x488) which only enter the model matrix: it does not reach the position, the direction or the hit
 * record. Read from the disassembly of the update callback, which is outside this file.
 *
 * Callees named by address: BtlTask_SetDead(task) kills a task; EftMesh_SetTex / EftMesh_SetLayer / EftMesh_SetTexBase /
 * EftMesh_SetColor / EftMesh_Draw model instance: set texture set / set a mode / set a mode / set colour / queue
 * for drawing; EftVram_AddImage / EftVram_AddClut advance one texture entry and return TEX0 fields; EftTexSet_Keep32
 * advances a whole texture set; ColSphere_Set fills a sphere; BtlObj_GetNodeSide(node) tests a node code (which hand).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
extern f32 acosf(f32 x);
extern f32 cosf(f32 x);
extern f32 sinf(f32 x);

extern void Vec4_Copy(void *dst, void *src);
extern void Vec4_Add(EftPVec *dst, EftPVec *a, EftPVec *b);
extern void Vec4_Sub(EftPVec *dst, EftPVec *a, EftPVec *b);
extern void Vec4_Scale(EftPVec *dst, EftPVec *src, f32 scale);
extern void Vec3_Add(EftPVec *dst, EftPVec *a, EftPVec *b);
extern void Vec3_Scale(EftPVec *dst, EftPVec *src, f32 scale);
extern void Vec3_Normalize(EftPVec *dst, EftPVec *src);
extern void Vec3_Cross(EftPVec *dst, EftPVec *a, EftPVec *b);
extern f32 Vec3_Dot(EftPVec *a, EftPVec *b);

extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern void *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftPTask *task);
extern void EftDisc_Update(EftPTask *task);

extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern s32 BtlCharApi_IsLockedOn(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, void *out);
extern void BtlCharApi_GetFrameMove(s32 objId, EftPVec *out);

extern void EftEmit_UpdateAlive(EftPSet *set, void *emit);
extern void EftEmit_KillAll(EftPSet *set, void *emit);
extern void EftEmit_TermState(EftPSet *set, void *emit);
extern s32 EftEmit_GetFlagsFromReq(EftPSet *set, void *emit, s32 objId, s32 group, s32 part, s32 f1, s32 f4);
extern s32 EftEmit_GetResetFlags(EftPSet *set, void *emit, s32 group, s32 part);
extern void EftEmit_SpawnOwn(EftPSet *set, void *emit, void *a2, void *a3, void *t0, s32 group, s32 part, s32 res,
                             f32 scale);
extern f32 EftEmit_GetTrailWidth(void *emit);

/* A hit record of the scene's list (EftHitRec of eft_a.h); only what this file writes. */
typedef struct EftPHitRec {
    /* 0x00 */ s32 objId;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 level;
    /* 0x0C */ s32 type;
    /* 0x10 */ EftPVec unk10;
    /* 0x20 */ EftPVec pos;
    /* 0x30 */ EftPVec prev;
    /* 0x40 */ EftPVec vel;
    /* 0x50 */ s32 flags;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ s32 unk58[2];
    /* 0x60 */ EftPTask *task;
    /* 0x64 */ EftPSrc *src;
    /* 0x68 */ EftDiscAtk *atk;
} EftPHitRec;

extern EftPHitRec *EftHit_GetNew(void);
extern void EftHit_Add(EftPHitRec *rec);
extern void *EftHitArena_AllocSphere(void);
extern void EftHit_SetShapeSpheres(EftPHitRec *rec, void *a, void *b);
extern void ColSphere_Set(void *sphere, void *pos, f32 radius);
extern s32 EftAim_GetDirKeep(EftPSrc *src, EftPVec *out, EftPVec *pos, s32 objId);

extern void EftMesh_SetTex(void *model, void *tex);
extern void EftMesh_SetLayer(void *model, s32 a1);
extern void EftMesh_SetTexBase(void *model, s32 a1);
extern void EftMesh_SetColor(void *model, u8 r, u8 g, u8 b, u8 a);
extern void EftMesh_Draw(void *model);
extern u64 EftVram_AddImage(EftPTexEntry *tex, s32 a, s32 b);
extern u64 EftVram_AddClut(EftPTexEntry *tex);
extern void EftTexSet_Keep32(void *set, s32 a, s32 b);
extern s32 BtlObj_GetNodeSide(s32 node);

extern void *D_002C3B38[];

u32 EftDiscMgr_WorkSize(void) { return (sizeof(EftDiscMgr)+15)&~15; }

/* Discs can finish out of order after hitting different opponents. Removing
   the head for every completion can leave a dead task in this owner's list. */
static void EftDiscMgr_Unlink(s32 objId,EftPTask *task) {
    EftDiscMgr *mgr;
    EftDiscNode *n,*prev=NULL;
    if(!gEftDisc || !gEftDisc->mgr || (u32)objId>=EFT_DISC_OWNERS)return;
    mgr=gEftDisc->mgr;
    for(n=mgr->head[objId];n;prev=n,n=n->next) if(n->task==task) {
        if(prev)prev->next=n->next;
        else mgr->head[objId]=n->next;
        if(mgr->tail[objId]==n)mgr->tail[objId]=prev;
        n->flags=0;n->task=NULL;n->next=NULL;
        return;
    }
}

/* Clamp as the module writes it everywhere (the update callback has four more copies). */
static inline f32 EftDisc_Clamp(f32 x, f32 lo, f32 hi) {
    if (x < lo) {
        return lo;
    }
    if (hi < x) {
        return hi;
    }
    return x;
}

/* Post-update callback: ends the disc on a hit result; a technique piece also updates and spawns its emitter parts. */
void EftDisc_PostUpdate(EftPTask *task) {
    EftDisc *w = task->work;
    EftDiscArg *arg = &w->arg;
    s32 stopped;

    if (!(w->flags & EFT_DISC_PIECE)) {
        stopped = BtlScene_IsEffectStopped(arg->objId, 0);
    } else {
        stopped = BtlScene_IsEffectStopped(arg->objId, 1);
    }
    if (!stopped) {
        if (task->result & 0x3F) {
            w->flags |= EFT_DISC_DEAD;
        }
        if (w->flags & EFT_DISC_PIECE) {
            EftEmit_UpdateAlive(arg->set, w->emit);
            if (task->hit & 1) {
                EftDisc_SpawnParts(task, 1);
            }
        }
    }
}

/* Draw callback: sets the model's colour and queues it when shown. */
void EftDisc_Draw(EftPTask *task) {
    EftDisc *w = task->work;

    if (w->flags & EFT_DISC_DEAD) {
        return;
    }
    EftMesh_SetLayer(w->model, 1);
    EftMesh_SetColor(w->model, w->color.x, w->color.y, w->color.z, w->color.w);
    if (w->flags & EFT_DISC_SHOWN) {
        EftMesh_Draw(w->model);
    }
}

/* Reset callback: kills the emitter parts and the task. */
void EftDisc_Reset(EftPTask *task) {
    EftDisc *w = task->work;
    EftDiscArg *arg = &w->arg;

    if (w->flags & EFT_DISC_PIECE) {
        EftEmit_KillAll(arg->set, w->emit);
    }
    w->flags |= EFT_DISC_DEAD;
    BtlTask_SetDead(task);
}

/* Term callback: frees the emitter state, or unlinks a held ki blast disc from its fighter's list. */
void EftDisc_Term(EftPTask *task) {
    EftDisc *w = task->work;
    EftDiscArg *arg = &w->arg;

    if (w->flags & EFT_DISC_PIECE) {
        EftEmit_TermState(arg->set, w->emit);
    } else if (arg->kind < 2) {
        EftDiscMgr_Unlink(arg->objId,task);
    }
    memset(w, 0, sizeof(EftDisc));
}

/* Spawns the emitter parts of a technique piece: while held only parts of type other than 1 and kind 5, in flight
   only parts of non-zero type and kind 5. mode 0 asks by request, 1 by reset flags. */
void EftDisc_SpawnParts(EftPTask *task, s32 mode) {
    u8 nodes[0x250];
    EftDisc *w = task->work;
    EftDiscArg *arg = &w->arg;
    s32 g;

    memset(nodes, 0, sizeof(nodes));
    for (g = 0; g < 19; g++) {
        if (*arg->set->mask & (1 << g)) {
            u8 *info = arg->set->grp[g].info;
            s32 j;

            for (j = 0; j < info[1]; j++) {
                s32 part = arg->set->grp[g].base + j;
                s32 ok = 0;
                s32 res;

                if (w->flags & EFT_DISC_HELD) {
                    if (arg->set->parts[part].type != 1) {
                        ok = arg->set->parts[part].kind == 5;
                    }
                }
                if (w->flags & EFT_DISC_FLYING) {
                    if (arg->set->parts[part].type != 0) {
                        if (arg->set->parts[part].kind == 5) {
                            ok = 1;
                        }
                    }
                }
                if (ok) {
                    if (mode == 0) {
                        res = EftEmit_GetFlagsFromReq(arg->set, w->emit, arg->objId, g, j, w->flags & 6,
                                                      w->flags & 0x200);
                    } else {
                        res = EftEmit_GetResetFlags(arg->set, w->emit, g, j);
                    }
                    if (res != 0) {
                        EftEmit_SpawnOwn(arg->set, w->emit, nodes, &arg->pos, &arg->dir, g, j, res, arg->scale);
                    }
                }
            }
        }
    }
}

/* Adds this frame's hit record: two spheres, at the position and at the previous position. */
void EftDisc_AddHit(EftPTask *task) {
    EftPHitRec *rec = EftHit_GetNew();
    EftDisc *w = task->work;
    f32 r;
    void *a;
    void *b;

    Vec4_Copy(&w->atk, &w->arg.dir);
    Vec4_Copy(&rec->unk10, &w->arg.pos);
    Vec4_Copy(&rec->vel, &w->arg.dir);
    Vec4_Copy(&rec->pos, &w->arg.pos);
    Vec4_Copy(&rec->prev, &w->prev);
    rec->task = task;
    rec->unk54 = 0;
    rec->objId = w->arg.objId;
    switch (w->arg.texB) {
    case 0:
        rec->unk4 = 1;
        break;
    case 1:
        rec->unk4 = 2;
        break;
    case 2:
        rec->unk4 = 4;
        break;
    case 3:
        rec->unk4 = 6;
        break;
    default:
        rec->unk4 = 1;
        break;
    }
    if (!(w->flags & EFT_DISC_PIECE)) {
        rec->atk = &w->atk;
        rec->type = 0;
        r = w->arg.scale * 4.5f;
    } else {
        EftPSrc *src = w->arg.src;

        if (!(src->def->flags & 1)) {
            return;
        }
        rec->src = src;
        rec->type = 1;
        r = EftEmit_GetTrailWidth(w->emit) * 3.0f;
        if (w->arg.lastHit != 0) {
            rec->flags |= 0x10;
        }
    }
    a = EftHitArena_AllocSphere();
    b = EftHitArena_AllocSphere();
    ColSphere_Set(a, &w->arg.pos, r);
    ColSphere_Set(b, &w->prev, r);
    EftHit_SetShapeSpheres(rec, a, b);
    EftHit_Add(rec);
}

/* Copies the hit parameters into the specific disc being released/created.
   The oldest disc can already be flying at a different target. */
void EftDisc_SetHeldAtk(EftPTask *task, EftDiscAtk *atk) {
    EftDisc *w;

    if (!gEftDisc || !task || !atk || (u32)atk->objId>=EFT_DISC_OWNERS) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_DISC_ALIVE) {
        w->atk = *atk;
    }
}

/* Binds the disc to a texture set and copies the two entries it animates. */
void EftDisc_SetTex(EftDisc *w, EftPTexSet *tex, s32 a, s32 b) {
    w->arg.texA = a;
    w->tex = tex;
    w->texA = *(a + tex->entry);
    w->texB = *(b + tex->entry);
}

/*
 * Homing: EftAim_Home with a banking term. Turns dir toward the opponent's node 0x11 (led by half its frame
 * movement times the frames to impact, at most 15) by at most maxTurn, reduced by |cur.y * to.z - cur.z * to.y|
 * x bank (clamped to 1). No turn when the owner is not locked on or the target is behind.
 * The clamp is written out as a conditional expression here: the inline EftDisc_Clamp gives other registers.
 */
void EftDisc_Home(EftPVec *out, EftPVec *pos, EftPVec *dir, s32 objId, f32 speed, f32 maxTurn, f32 bank) {
    EftPVec target;
    EftPVec move;
    EftPVec lead;
    EftPVec newDir;
    EftPVec cur;
    EftPVec to;
    EftPVec axis;
    s32 opp = BtlCharApi_GetOpponentObjId(objId);
    f32 frames;
    f32 t;
    f32 d;
    f32 angle;
    f32 k;
    f32 c;

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
    c = 0.0f;
    if (d > 0.0f) {
        if (d > 1.0f) {
            d = 1.0f;
        }
        angle = acosf(d);
        if (frames > 2.0f) {
            angle /= frames * 0.5f;
        }
        t = maxTurn < angle ? maxTurn : angle;
        Vec3_Cross(&axis, &cur, &to);
        Vec3_Normalize(&axis, &axis);
        k = (cur.y * to.z - cur.z * to.y) * bank;
        c = (k < -1.0f) ? -1.0f : ((1.0f < k) ? 1.0f : k);
        k = __builtin_fabsf(c);
        c = 1.0f - k;
        EftDisc_RotateAboutAxis(&newDir, &cur, &axis, t * c);
        Vec3_Normalize(&newDir, &newDir);
    } else {
        Vec4_Copy(&newDir, &cur);
    }
    Vec4_Copy(out, &newDir);
}

/* Rotates v about the unit vector axis by angle (Rodrigues' formula); w is copied. */
void EftDisc_RotateAboutAxis(EftPVec *out, EftPVec *v, EftPVec *axis, f32 angle) {
    EftPVec a;
    EftPVec b;
    EftPVec c;
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

/* Advances the disc's textures once per frame: its own two entries (kinds 0..2), or the whole set. */
void EftDisc_StepTex(EftDisc *w) {
    EftPTexSet *tex = w->tex;
    EftDiscArg *arg = &w->arg;

    if (tex != NULL) {
        if (arg->kind < 3) {
            s32 bit = 1 << w->arg.texA;

            if (!(tex->stepped & bit)) {
                u64 lo = EftVram_AddImage(&w->texA, 1, 0);
                u64 hi = EftVram_AddClut(&w->texB);

                tex->entry[w->arg.texA].tex0 = lo | (hi << 37);
                tex->stepped |= 1 << w->arg.texA;
            }
        } else {
            if (!(tex->stepped & 1)) {
                EftTexSet_Keep32(tex, 1, 0);
                tex->stepped |= 1;
            }
        }
    }
}

/* Appends a disc to its fighter's list of held discs. Returns 0 when no node is free. */
s32 EftDiscMgr_PushHeld(EftPTask *task, s32 objId, u8 kind) {
    EftDiscMgr *mgr;
    EftDiscNode *n;

    if (task == NULL || (u32)objId>=EFT_DISC_OWNERS) {
        return 0;
    }
    if (gEftDisc == NULL) {
        return 0;
    }
    mgr = gEftDisc->mgr;
    if (mgr == NULL) {
        return 0;
    }
    n = EftDiscMgr_AllocNode();
    if (n == NULL) {
        return 0;
    }
    n->task = task;
    n->next = NULL;
    n->flags |= 1;
    if (kind == 1) {
        n->flags |= 0x10;
    }
    if (mgr->head[objId] == NULL) {
        mgr->head[objId] = n;
        mgr->tail[objId] = n;
    } else {
        mgr->tail[objId]->next = n;
        mgr->tail[objId] = n;
    }
    return 1;
}

/* Unlinks the first held disc of a fighter. */
s32 EftDiscMgr_PopHeld(s32 objId) {
    EftDiscMgr *mgr;
    EftDiscNode *n;

    if (gEftDisc == NULL || (u32)objId>=EFT_DISC_OWNERS) {
        return 0;
    }
    mgr = gEftDisc->mgr;
    if (mgr == NULL) {
        return 0;
    }
    n = mgr->head[objId];
    if (n != NULL) {
        EftDiscMgr_Unlink(objId,n->task);
    }
    return 1;
}

/* Finds a free list node, starting at the cursor. */
EftDiscNode *EftDiscMgr_AllocNode(void) {
    EftDiscMgr *mgr = gEftDisc->mgr;
    u8 i;
    EftDiscNode *n;

    if (mgr->cursor >= EFT_DISC_CAPACITY) {
        mgr->cursor = 0;
    }
    i = mgr->cursor;
    do {
        n = &mgr->node[i];
        i++;
        if (i >= EFT_DISC_CAPACITY) {
            i = 0;
        }
        if (n->flags == 0) {
            mgr->cursor = i;
            return n;
        }
    } while (i != mgr->cursor);
    return NULL;
}

/* Fighter request 0x17 with ki blast type 4: creates a disc that grows in the fighter's hand (kind 0). */
s32 EftDisc_SpawnHeld(EftDiscHeldArg *a, s32 hand) {
    EftDiscArg arg;

    if (gEftDisc == NULL || a == NULL || (u32)a->objId>=EFT_DISC_OWNERS) {
        return 0;
    }
    if (gEftDisc->mgr == NULL) {
        return 0;
    }
    if (gEftDisc->mgr->list == NULL) {
        return 0;
    }
    arg = (EftDiscArg){ NULL, NULL, a->pos, {}, a->scale, 0.0f, 1.0f, 0.3f, 0.7f, 1.0f, 1.5f, 0.0f, 0.0f, 0, 0,
                        a->objId, 0, 0, hand };
    switch (a->level) {
    case 1:
        arg.texB = 0;
        break;
    case 2:
        arg.texB = 1;
        break;
    case 4:
        arg.texB = 2;
        break;
    case 6:
        arg.texB = 3;
        break;
    default:
        arg.texB = 0;
        break;
    }
    if (BtlObj_GetNodeSide(a->node) == 0) {
        arg.hand = 0;
    } else {
        arg.hand = 1;
    }
    return BtlTaskList_AddTail(gEftDisc->mgr->list, D_002C3B38, &arg) != NULL;
}

/* Ki blast fire event, type 4: throws the fighter's first held disc that is not thrown yet; without one (or when
   the held one lost its owner's lock) creates a flying disc. */
s32 EftDisc_Throw(EftDiscAtk *a) {
    EftDiscNode *n;
    EftPTask *task;
    EftDisc *w;

    if (gEftDisc == NULL || !gEftDisc->mgr || !a || (u32)a->objId>=EFT_DISC_OWNERS) {
        return 0;
    }
    n = gEftDisc->mgr->head[a->objId];
    while (n != NULL && (n->flags & 0x30)) {
        n = n->next;
    }
    if (n == NULL) {
        return EftDisc_SpawnThrown(a);
    }
    task = n->task;
    if (task == NULL) {
        return EftDisc_SpawnThrown(a);
    }
    w = task->work;
    if (w == NULL) {
        return EftDisc_SpawnThrown(a);
    }
    if (w->flags & EFT_DISC_LOST) {
        return EftDisc_SpawnThrown(a);
    }
    if (w->flags & EFT_DISC_ALIVE) {
        EftDisc_SetHeldAtk(task, a);
        Vec4_Copy(&w->arg.dir, a);
        Vec4_Copy(&w->dir, a);
        w->arg.life = (f32)a->life / 30.0f;
        w->life = a->life;
        w->age = 0.0f;
        w->arg.speed = a->speed;
        w->arg.turn = a->turn;
        w->fadeLeft = w->fadeTime = (f32)a->life * (1.0f - w->arg.fade);
        w->flags |= EFT_DISC_FLYING;
        n->flags |= 0x20;
        w->flags &= ~EFT_DISC_HELD;
        return 1;
    }
    return 0;
}

/* Creates a flying ki blast disc (kind 1); its init callback places it at the owner's hand node. */
s32 EftDisc_SpawnThrown(EftDiscAtk *a) {
    EftDiscArg arg;
    EftPTask *task;

    if (gEftDisc == NULL || a == NULL || (u32)a->objId>=EFT_DISC_OWNERS) {
        return 0;
    }
    if (gEftDisc->mgr == NULL) {
        return 0;
    }
    if (gEftDisc->mgr->list == NULL) {
        return 0;
    }
    arg = (EftDiscArg){ NULL, NULL, a->pos, {}, a->scale, a->speed, (f32)a->life / 30.0f, 0.0f, 0.7f, a->turn, 1.5f,
                        0.0f, 0.0f, 0, 0, a->objId, 1, 0, 0 };
    if (BtlObj_GetNodeSide(a->node) == 0) {
        arg.hand = 0;
    } else {
        arg.hand = 1;
    }
    switch (a->level) {
    case 1:
        arg.texB = 0;
        break;
    case 2:
        arg.texB = 1;
        break;
    case 4:
        arg.texB = 2;
        break;
    case 6:
        arg.texB = 3;
        break;
    default:
        arg.texB = 0;
        break;
    }
    task = BtlTaskList_AddTail(gEftDisc->mgr->list, D_002C3B38, &arg);
    if (task == NULL) {
        return 0;
    }
    EftDisc_SetHeldAtk(task, a);
    return 1;
}

/* Kills the first held disc of a fighter and unlinks it. No caller in the main executable. */
s32 EftDisc_DropHeld(s32 objId) {
    EftDiscMgr *mgr;
    EftDiscNode *n;
    EftPTask *t;
    EftDisc *w;

    if (gEftDisc == NULL || !gEftDisc->mgr || (u32)objId>=EFT_DISC_OWNERS) {
        return 0;
    }
    mgr = gEftDisc->mgr;
    n = mgr->head[objId];
    if (n == NULL) {
        return 0;
    }
    t = n->task;
    if (t == NULL) {
        return 0;
    }
    w = t->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_DISC_ALIVE) {
        w->flags |= EFT_DISC_DEAD;
        EftDiscMgr_Unlink(objId,t);
        return 1;
    }
    return 0;
}

/* Ki blast fire event, type 5: creates a flying disc of kind 3 at a model node of the fighter. */
s32 EftDisc_SpawnFromNode(EftDiscAtk *a) {
    EftDiscArg arg;
    EftPTask *task;

    if (gEftDisc == NULL || a == NULL || (u32)a->objId>=EFT_DISC_OWNERS) {
        return 0;
    }
    if (gEftDisc->mgr == NULL) {
        return 0;
    }
    if (gEftDisc->mgr->list == NULL) {
        return 0;
    }
    arg = (EftDiscArg){ NULL, NULL, {}, {}, a->scale, a->speed, a->life, 0.05f, 0.7f, a->turn, 0.0f, -0.42f, 0.0f,
                        0, 0, a->objId, 3, 0, 0 };
    Vec4_Copy(&arg.dir, a);
    BtlCharApi_GetNodePos(a->objId, a->node, &arg.pos);
    task = BtlTaskList_AddTail(gEftDisc->mgr->list, D_002C3B38, &arg);
    if (task == NULL) {
        return 0;
    }
    ((EftDisc *)task->work)->atk = *a;
    return 1;
}

/* Creates a disc from a parameter block; returns its task (the handle the functions below take). */
EftPTask *EftDisc_Create(EftDiscArg *arg) {
    if (gEftDisc == NULL || arg == NULL) {
        return NULL;
    }
    if (gEftDisc->mgr == NULL) {
        return NULL;
    }
    if (gEftDisc->mgr->list == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftDisc->mgr->list, D_002C3B38, arg);
}

/* Releases a held disc: a ki blast disc flies along dir, a technique piece along the technique's aim direction. */
void EftDisc_Release(EftPTask *task, EftPVec dir) {
    EftDisc *w;

    if (gEftDisc == NULL || task == NULL) {
        return;
    }
    if (*task->cls != (void *)EftDisc_Update) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (!(w->flags & EFT_DISC_ALIVE)) {
        return;
    }
    if (!(w->flags & EFT_DISC_PIECE)) {
        Vec4_Copy(&w->arg.dir, &dir);
    } else {
        if (w->grown <= 0.0f || w->color.w <= 0.0f) {
            w->grown = 1.0f;
            w->color.w = 127.0f;
            w->flags |= EFT_DISC_SHOWN;
        }
        EftAim_GetDirKeep(w->arg.src, &w->arg.dir, &w->arg.pos, w->arg.objId);
    }
    w->flags |= EFT_DISC_FLYING;
    w->flags &= ~EFT_DISC_HELD;
}

/* Asks a disc to fade out and die. No caller in the main executable. */
void EftDisc_RequestEnd(EftPTask *task) {
    EftDisc *w;

    if (gEftDisc != NULL && task != NULL) {
        if (*task->cls == (void *)EftDisc_Update) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_DISC_ALIVE) {
                    w->flags |= EFT_DISC_END;
                }
            }
        }
    }
}

/* Kills a disc. */
void EftDisc_Kill(EftPTask *task) {
    EftDisc *w;

    if (gEftDisc != NULL && task != NULL) {
        if (*task->cls == (void *)EftDisc_Update) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_DISC_ALIVE) {
                    w->flags |= EFT_DISC_DEAD;
                }
            }
        }
    }
}

/* Sets the model's spin per frame (radians). Appearance only: see the note at the top of the file. */
void EftDisc_SetSpin(EftPTask *task, f32 spin) {
    EftDisc *w;

    if (gEftDisc != NULL && task != NULL) {
        if (*task->cls == (void *)EftDisc_Update) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_DISC_ALIVE) {
                    w->spin = spin;
                }
            }
        }
    }
}

/* Sets the disc's scale (the hit radius of a ki blast disc is scale x 4.5). */
void EftDisc_SetScale(EftPTask *task, f32 scale) {
    EftDisc *w;

    if (gEftDisc != NULL && task != NULL) {
        if (*task->cls == (void *)EftDisc_Update) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_DISC_ALIVE) {
                    w->arg.scale = scale;
                }
            }
        }
    }
}

/* Gives the disc's model another texture set and rebinds the two animated entries. */
s32 EftDisc_SetModelTex(EftPTask *task, void *tex, s32 a, s32 b) {
    EftDisc *w;

    if (gEftDisc != NULL && task != NULL) {
        if (*task->cls == (void *)EftDisc_Update) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_DISC_ALIVE) {
                    EftMesh_SetTex(w->model, tex);
                    EftMesh_SetTexBase(w->model, 0);
                    EftDisc_SetTex(w, *(EftPTexSet **)(w->model + 0x48), a, b);
                }
            }
        }
    }
}

/* Marks the piece's hit records as the technique's last hit. */
void EftDisc_SetLastHit(EftPTask *task, s32 on) {
    EftDisc *w;

    if (gEftDisc != NULL && task != NULL) {
        if (*task->cls == (void *)EftDisc_Update) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_DISC_ALIVE) {
                    w->arg.lastHit = on;
                }
            }
        }
    }
}

/* Sets flag 0x200 (passed to the emitter request test of the piece's parts). */
void EftDisc_SetFlag200(EftPTask *task) {
    EftDisc *w;

    if (gEftDisc != NULL && task != NULL) {
        if (*task->cls == (void *)EftDisc_Update) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_DISC_ALIVE) {
                    w->flags |= EFT_DISC_REQ200;
                }
            }
        }
    }
}

/* Is the handle a live disc? */
s32 EftDisc_IsAlive(EftPTask *task) {
    EftDisc *w;

    if (gEftDisc == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (*task->cls != (void *)EftDisc_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_DISC_ALIVE) {
        return 1;
    }
    return 0;
}
