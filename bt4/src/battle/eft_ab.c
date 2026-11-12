#include "common.h"
#include "battle/eft_ab.h"

/*
 * "Orb tail", 0x19E0C0..0x1A0020: everything but the manager's init callback (0x19DEA8, in the file before this
 * one). Manager class 0x2C4290 = {EftOrbTailMgr_Update, 0x19DEA8 init, EftOrbTailMgr_Term, 0, EftOrbTailMgr_Reset,
 * 0}; task class 0x2C42A8 (gEftOrbTailClass) = {EftOrbTail_Update, _Init, _Term, _PostUpdate, _Reset, _Draw}.
 *
 * The only user is the "shots" technique (eft_k.c EftShotTech_UpdateAuraBall): the fighter's event 0x100 creates a
 * task with the fighter's aura type and calls EftOrbTail_Burst; every frame EftOrbTail_SetPos puts it between the
 * fighter's nodes 10 and 14 (the hands); event 0x200 calls EftOrbTail_Burst and EftOrbTail_Kill.
 *
 * What it draws: a chain of up to nodeMax nodes is laid out behind the ball against its direction of travel and
 * relaxes towards a fixed segment length every frame (a rope). While the fighter moves, batches of streak sprites
 * are born at the head of the chain and travel along it, node frame by node frame; they fade towards the end of the
 * chain and when the fighter stops. EftOrbTail_Burst starts three emitters of the 0x186B50 module at the ball.
 *
 * Purely visual: it reads the fighter (height, direction, speed, fall speed, "in a rush sequence", node positions)
 * and writes only its own pools.
 *
 * Random draws: one libc rand() at task init (Rand_IntRange, the node count, when the parameter range is not 0)
 * and eleven draws of the VU0 generator per streak (EftOrbTail_InitStreak); appearance only.
 *
 * Matching notes: the vector fields are the 16-byte aligned type (with the plain Vec4 the compiler hoists
 * "table + 12" out of the streak loop); tables indexed in the original through a pointer are written
 * "(table + i)->field" (index * size + base in the address); the status entry points need one "return 0" per
 * test (that is what puts "v0 = 0" in front of the first test); the chain walk in EftOrbTail_UpdateStreaks is a
 * for (;;) whose first exit sits more than 30 instructions before the second (the compiler rolls the loop there).
 * The frame rectangles (EftOrbTail.uv) are NOT that aligned type but plain floats, f32 [16][4]: only with a
 * 4-aligned element does the compiler add each component's offset to the base before the index, which gives the
 * four separate `w + f * 16` (three of them register copies after reload) of EftOrbTail_DrawStreaks and
 * EftOrbTail_InitFrames. In EftOrbTail_InitFrames `row` is initialised at its declaration (its `move t1,zero`
 * sits in front of the range test).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);

extern void Vec4_Copy(EftAbVec *dst, EftAbVec *src);
extern void Vec4_Set(EftAbVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Add(EftAbVec *dst, EftAbVec *a, EftAbVec *b);
extern void Vec3_Sub(EftAbVec *dst, EftAbVec *a, EftAbVec *b);
extern void Vec3_Scale(EftAbVec *dst, EftAbVec *src, f32 s);
extern void Vec3_Normalize(EftAbVec *dst, EftAbVec *src);
extern f32 Vec3_Length(EftAbVec *v);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(EftAbVec *dst, Mtx44 *m, EftAbVec *v);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void Vec3_ScaleAdd(EftAbVec *dst, EftAbVec *dir, EftAbVec *base, f32 s);
#define Vec3_ScaleAdd(dst, dir, s, base) Vec3_ScaleAdd(dst, dir, base, s)
#else
extern void Vec3_ScaleAdd(EftAbVec *dst, EftAbVec *dir, f32 s, EftAbVec *base); /* dst = base + dir * s */
#endif
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);       /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);       /* rotate about Y */
extern void Vu0Cur_Push(void);                                    /* VU0 matrix stack: push */
extern void Vu0Cur_Pop(void);                                    /* VU0 matrix stack: pop */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                                /* VU0 current matrix = m */

extern f32 Rand_FloatRange(f32 a, f32 b);
extern s32 Rand_IntRange(s32 a, s32 b);

extern s32 BtlPool_GetCurrent(void);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern EftAbTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftAbTask *task);                         /* marks a task as dying */
extern u64 EftVram_AddImage(EftAbTexEntry *tex, s32 a, s32 b);         /* uploads the image, returns its TEX0 */
extern u64 EftVram_AddClut(EftAbTexEntry *tex);                       /* uploads the palette, returns its block */

extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetDir(s32 objId, EftAbVec *out);
extern f32 BtlCharApi_GetSpeed(s32 objId);
extern f32 BtlCharApi_GetFallSpeed(s32 objId);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, EftAbVec *out);

extern f32 EftMath_WrapAngle(f32 angle);
/* The definition's parameter order (src/battle/eft_b.c): with the integers declared in front of the floats the
   registers are the same but the caller loads them in another order. */
extern void EftPrim_DrawQuadDepthScaled(EftAbVec *pos, EftAbVec *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1,
                                        f32 roll, s32 arg2, s32 arg3, u64 tex0, f32 depthScale);

/* The emitters of the 0x186B50 module (eft_h.h EftEmitArgA). */
typedef struct EftOrbBurstArg {
    /* 0x00 */ EftAbVec pos;
    /* 0x10 */ EftAbVec dir;
    /* 0x20 */ s32 objId;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ f32 rate;
    /* 0x2C */ f32 size;
    /* 0x30 */ EftAbTex *tex;
    /* 0x34 */ s32 *image;
    /* 0x38 */ s32 *palette;
    /* 0x3C */ s32 unk3C;
} EftOrbBurstArg; /* 0x40 */

extern void *EftPtcl_Create(EftOrbBurstArg *arg);
extern s32 EftPtcl_Stop(void *h);                                  /* stop emitting */
extern s32 EftPtcl_IsAlive(void *h);                                  /* is the handle still valid */
extern s32 EftPtcl_SetPos(void *h, EftAbVec *pos);
extern s32 EftPtcl_SetDir(void *h, EftAbVec *dir);
extern s32 EftPtcl_SetTexture(void *h, EftAbTex *tex, s32 a2, s32 a3);

typedef struct EftAbView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ Mtx44 viewMtx;
} EftAbView;
extern EftAbView *gBtlCamView;

/* Unlinks p from a doubly linked list (the original repeats this inline). */
#define EFT_ORB_UNLINK(head, tail, p)       \
    if (head != NULL) {                     \
        if (p->prev == NULL) {              \
            if (p->next == NULL) {          \
                head = NULL;                \
                tail = NULL;                \
            } else {                        \
                head = p->next;             \
                head->prev = NULL;          \
            }                               \
        } else if (p->next == NULL) {       \
            tail = p->prev;                 \
            tail->next = NULL;              \
        } else {                            \
            p->next->prev = p->prev;        \
            p->prev->next = p->next;        \
        }                                   \
    }

/* Clamp to 0..1, as an expression: the fades only match with the ternary form (the compiler distributes
   "1.0f - clamp" over its arms). */
#define EFT_ORB_CLAMP01(x) ((x) < 0.0f ? 0.0f : 1.0f < (x) ? 1.0f : (x))

/* Manager update callback: forgets which TEX0 values were built last frame. */
void EftOrbTailMgr_Update(void) {
    s32 i;

    gEftOrbTail->mgr->tex.ready = 0;
    for (i = 0; i < 3; i++) {
        gEftOrbTail->mgr->burstTex[i].ready = 0;
    }
}

/* Manager reset callback: nothing. */
void EftOrbTailMgr_Reset(void) {
}

/* Manager term callback: frees the module's data. */
void EftOrbTailMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftOrbTail->mgr);
    BtlPool_Free(BtlPool_GetCurrent(), gEftOrbTail);
    gEftOrbTail = NULL;
}

/* Init callback: copies the argument, picks the colour group, sizes the chain from the fighter's height. */
void EftOrbTail_Init(EftAbTask *task, EftOrbTailArg *arg) {
    EftOrbTail *w = task->work;
    EftOrbTailParam *prm = arg->param;

    memset(w, 0, sizeof(EftOrbTail));
    w->arg = *arg;
    switch (arg->kind) {
    case 7:
        w->arg.kind = 1;
        break;
    case 8:
    case 9:
    case 10:
        w->arg.kind = 2;
        break;
    }
    EftOrbTail_SetTex(w, &gEftOrbTail->mgr->tex, 0, w->arg.kind);
    EftOrbTail_InitFrames(w);
    w->scale = BtlCharApi_GetHeight(arg->objId) / 19.35f;
    w->nodeMax = Rand_IntRange(prm->nodesBase, prm->nodesBase + prm->nodesRange);
    w->nodeMax = w->nodeMax * w->scale;
    w->growLen = prm->nodeLen * w->nodeMax;
    w->growStep = (prm->speedBase[0] + prm->speedRange[0] + (prm->speedBase[1] + prm->speedRange[1]) +
                   (prm->speedBase[2] + prm->speedRange[2])) *
                  0.5f;
    BtlCharApi_GetDir(arg->objId, &w->dir);
    w->flags |= EFT_ORB_ACTIVE | EFT_ORB_UNK10;
}

/* Update callback: tracks the ball's movement, feeds nodes and streaks while the fighter moves, steps everything. */
void EftOrbTail_Update(EftAbTask *task) {
    EftOrbTail *w = task->work;
    EftOrbTailArg *arg = &w->arg;
    EftOrbTailParam *prm = arg->param;
    f32 speed = 0.0f;
    s32 i;

    if (!BtlScene_IsEffectStopped(arg->objId, 2)) {
        Vec3_Sub(&w->dir, &w->pos, &w->prevPos);
        if (w->grown <= w->growLen) {
            speed = EFT_ORB_CLAMP01(w->grown / w->growLen);
            w->grow = speed;
            w->grown += w->growStep;
        }
        if (BtlCharApi_IsInRushSequence(arg->objId)) {
            speed = Vec3_Length(&w->dir);
        } else {
            speed = BtlCharApi_GetSpeed(arg->objId);
            speed += __builtin_fabsf(BtlCharApi_GetFallSpeed(arg->objId));
        }
        if (speed > 0.0f) {
            w->flags |= EFT_ORB_MOVING;
        } else {
            w->flags &= ~EFT_ORB_MOVING;
        }
        Vec3_Normalize(&w->dir, &w->dir);
        if (!(w->flags & (EFT_ORB_END | EFT_ORB_KILL))) {
            if (w->flags & EFT_ORB_MOVING) {
                if (w->nodeCount < w->nodeMax) {
                    for (i = 0; i < w->nodeMax; i++) {
                        EftOrbTail_AddNode(w);
                    }
                }
                if (w->flags & EFT_ORB_CHAIN) {
                    if ((s32)w->frame % prm->streakPeriod == 0) {
                        for (i = 0; i < prm->streakCount; i++) {
                            EftOrbTail_AddStreak(w);
                        }
                    }
                }
            }
            if (w->flags & EFT_ORB_BURST) {
                EftOrbTail_StartBurst(w);
                w->flags &= ~EFT_ORB_BURST;
            }
        } else {
            EftOrbTail_StopBurst(w);
        }
        EftOrbTail_UpdateNodes(w);
        EftOrbTail_UpdateStreaks(w);
        EftOrbTail_PollBurst(w);
        w->frame += 1.0f;
    }
    if ((w->flags & EFT_ORB_END) && w->streakHead == NULL) {
        w->flags |= EFT_ORB_DONE;
    }
    if (w->flags & (EFT_ORB_KILL | EFT_ORB_DONE)) {
        BtlTask_SetDead(task);
        if (w->flags & (EFT_ORB_KILL | EFT_ORB_DONE)) {
            return;
        }
    }
    EftOrbTail_UpdateTex(w);
}

/* Post-update callback: nothing. */
void EftOrbTail_PostUpdate(void) {
}

/* Draw callback: the streaks, in view space. */
void EftOrbTail_Draw(EftAbTask *task) {
    EftOrbTail *w = task->work;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->viewMtx);
    EftOrbTail_DrawStreaks(w);
    Vu0Cur_Pop();
}

/* Reset callback (battle restart): the task dies. */
void EftOrbTail_Reset(EftAbTask *task) {
    BtlTask_SetDead(task);
}

/* Term callback: gives every node and streak back to the pools and stops the burst emitters. */
void EftOrbTail_Term(EftAbTask *task) {
    EftOrbTail *w = task->work;
    EftOrbTailNode *n;
    EftOrbTailStreak *p;

    for (n = w->nodeHead; n != NULL; n = n->next) {
        EFT_ORB_UNLINK(w->nodeHead, w->nodeTail, n);
        n->used = 0;
    }
    for (p = w->streakHead; p != NULL; p = p->next) {
        EFT_ORB_UNLINK(w->streakHead, w->streakTail, p);
        p->flags = 0;
    }
    EftOrbTail_StopBurst(w);
    w->flags = 0;
}

/* Appends one node: one segment behind the last node, or at the head offset in front of the ball. */
void EftOrbTail_AddNode(EftOrbTail *w) {
    EftOrbTailParam *prm = w->arg.param;
    EftAbVec pos = { 0.0f, 0.0f, 0.0f, 1.0f };
    EftOrbTailNode *n;

    if (w->nodeTail != NULL) {
        Vec3_ScaleAdd(&pos, &w->dir, -(prm->nodeLen * w->scale), &w->nodeTail->pos);
    } else {
        Vec3_ScaleAdd(&pos, &w->dir, prm->headOffset * w->scale, &w->pos);
    }
    n = EftOrbTail_AllocNode();
    if (n != NULL) {
        EftOrbTail_SetNodePos(n, w, pos);
        n->next = NULL;
        n->prev = NULL;
        n->used = 1;
        if (w->nodeHead == NULL) {
            w->nodeHead = n;
            w->nodeTail = n;
        } else {
            n->prev = w->nodeTail;
            w->nodeTail->next = n;
            w->nodeTail = n;
        }
        w->nodeCount++;
    }
    if (w->nodeHead != NULL && w->nodeHead->next != NULL) {
        w->flags |= EFT_ORB_CHAIN;
    }
}

/* Sets a node's position. */
void EftOrbTail_SetNodePos(EftOrbTailNode *node, EftOrbTail *w, EftAbVec pos) {
    Vec4_Copy(&node->pos, &pos);
}

/* Moves the chain: the head follows the ball, every other node is pulled to one segment length from the node
   before it, then lengths and orientations are rebuilt. */
void EftOrbTail_UpdateNodes(EftOrbTail *w) {
    EftOrbTailParam *prm = w->arg.param;
    EftAbVec d = { 0.0f, 0.0f, 0.0f, 1.0f };
    EftAbVec t = { 0.0f, 0.0f, 0.0f, 1.0f };
    EftOrbTailNode *n;
    f32 len = 0.0f;

    if (w->nodeHead != NULL) {
        Vec3_ScaleAdd(&w->nodeHead->pos, &w->dir, prm->headOffset * w->scale, &w->pos);
    }
    for (n = w->nodeHead; n != NULL; n = n->next) {
        if (n->next != NULL) {
            Vec3_Sub(&d, &n->next->pos, &n->pos);
            len = Vec3_Length(&d);
            if (len > 0.0f) {
                Vec3_Normalize(&d, &d);
                if (!(prm->nodeLen * w->scale < len)) {
                    Vec3_ScaleAdd(&t, &d, len, &n->pos);
                } else {
                    Vec3_ScaleAdd(&t, &d, prm->nodeLen * w->scale, &n->pos);
                }
            } else {
                Vec4_Copy(&t, &n->pos);
            }
            Vec3_Sub(&d, &t, &n->next->pos);
            Vec3_Scale(&d, &d, -(prm->stiffness * w->scale));
            Vec3_Add(&n->next->pos, &t, &d);
        }
    }
    w->chainLen = 0.0f;
    for (n = w->nodeHead; n != NULL; n = n->next) {
        if (n->next != NULL) {
            Vec3_Sub(&d, &n->next->pos, &n->pos);
        } else if (n->prev != NULL) {
            Vec3_Sub(&d, &n->pos, &n->prev->pos);
        }
        n->len = Vec3_Length(&d);
        w->chainLen += n->len;
        if (n->len > 0.0f) {
            Vec3_Normalize(&d, &d);
            n->pitch = EftMath_WrapAngle(Mathf_Asin(-d.y));
            n->yaw = EftMath_WrapAngle(atan2f(d.x, d.z));
        } else if (n->prev != NULL) {
            n->pitch = n->prev->pitch;
            n->yaw = n->prev->yaw;
        }
        Mtx_StoreIdentity(&n->mtx);
        Mtx_RotateX(&n->mtx, &n->mtx, n->pitch);
        Mtx_RotateY(&n->mtx, &n->mtx, n->yaw);
        Vec4_Copy((EftAbVec *)n->mtx.m[3], &n->pos);
    }
}

/* Returns a free node of the pool, searching round-robin from where the last search ended. */
EftOrbTailNode *EftOrbTail_AllocNode(void) {
    EftOrbTailMgr *mgr = gEftOrbTail->mgr;
    u32 next = mgr->nodeNext;
    u32 i;
    u32 start;

    if (next >= EFT_ORB_NODES) {
        mgr->nodeNext = 0;
        next = 0;
    }
    i = next;
    start = i;
    do {
        EftOrbTailNode *n = &mgr->node[i];

        i++;
        if (i >= EFT_ORB_NODES) {
            i = 0;
        }
        if (n->used == 0) {
            mgr->nodeNext = i;
            return n;
        }
    } while (i != start);
    return NULL;
}

/* Appends one streak at the head of the chain. */
void EftOrbTail_AddStreak(EftOrbTail *w) {
    EftOrbTailStreak *p;

    if (w->nodeHead != NULL) {
        p = EftOrbTail_AllocStreak();
        if (p != NULL) {
            EftOrbTail_InitStreak(p, w);
            p->flags = EFT_ORB_STREAK_USED;
            p->prev = NULL;
            p->next = NULL;
            if (w->streakHead == NULL) {
                w->streakHead = p;
                w->streakTail = p;
            } else {
                p->prev = w->streakTail;
                w->streakTail->next = p;
                w->streakTail = p;
            }
        }
    }
}

/* Rolls a streak: life, three speed and size keys, fade times, roll and first animation frame (11 draws of the
   VU0 generator). */
void EftOrbTail_InitStreak(EftOrbTailStreak *p, EftOrbTail *w) {
    EftOrbTailParam *prm = w->arg.param;
    EftOrbTailColor *col = w->arg.color;
    f32 key[3] = { 0.0f, 0.0f, 0.0f };

    Vec4_Copy(&p->color, &col->color[w->arg.kind]);
    Vec4_Set(&p->pos, 0.0f, 0.0f, 0.0f, 1.0f);
    Vec4_Set(&p->local, 0.0f, 0.0f, 0.0f, 1.0f);
    Vec4_Set(&p->dir, 0.0f, 0.0f, 1.0f, 1.0f);
    p->life = Rand_FloatRange(prm->lifeBase, prm->lifeBase + prm->lifeRange) * 30.0f * w->scale;
    p->age = 0.0f;

    key[0] = Rand_FloatRange(prm->speedBase[0], prm->speedBase[0] + prm->speedRange[0]) * w->scale;
    key[1] = Rand_FloatRange(prm->speedBase[1], prm->speedBase[1] + prm->speedRange[1]) * w->scale;
    key[2] = Rand_FloatRange(prm->speedBase[2], prm->speedBase[2] + prm->speedRange[2]) * w->scale;
    p->speedStepA = (key[1] - key[0]) / (p->life * prm->turn);
    p->speedStepB = (key[2] - key[1]) / (p->life * (1.0f - prm->turn));
    p->speed = key[0];

    key[0] = Rand_FloatRange(prm->sizeBase[0], prm->sizeBase[0] + prm->sizeRange[0]) * w->scale;
    key[1] = Rand_FloatRange(prm->sizeBase[1], prm->sizeBase[1] + prm->sizeRange[1]) * w->scale;
    key[2] = Rand_FloatRange(prm->sizeBase[2], prm->sizeBase[2] + prm->sizeRange[2]) * w->scale;
    p->sizeStepA = (key[1] - key[0]) / (p->life * prm->turn);
    p->sizeStepB = (key[2] - key[1]) / (p->life * (1.0f - prm->turn));
    p->size = key[0];

    key[0] = col->alphaA;
    key[1] = (col->alphaC + col->alphaA) * 0.5f;
    key[2] = col->alphaC;
    p->alphaStepA = (key[1] - key[0]) / (p->life * col->alphaTurn);
    p->alphaStepB = (key[2] - key[1]) / (p->life * (1.0f - col->alphaTurn));
    p->alpha = key[0];

    p->roll = Rand_FloatRange(-3.14159265f, 3.14159265f);
    p->fadeInLen = Rand_FloatRange(prm->fadeInBase, prm->fadeInBase + prm->fadeInRange) * 30.0f * w->scale;
    p->fadeOutLen = Rand_FloatRange(prm->fadeOutBase, prm->fadeOutBase + prm->fadeOutRange) * 30.0f * w->scale;
    p->fadeOut = p->fadeOutLen;
    p->fadeIn = 0.0f;
    p->frame = Rand_FloatRange(0.0f, w->texFrames - 1.0f);
    p->dist = 0.0f;
    p->node = w->nodeHead;
}

/* Steps every streak: speed, size and alpha ramps, travel along the chain, fades, and removal. */
void EftOrbTail_UpdateStreaks(EftOrbTail *w) {
    EftOrbTailArg *arg = &w->arg;
    s32 gap = 0;
    EftOrbTailParam *prm = arg->param;
    EftOrbTailColor *col = arg->color;
    EftAbVec v = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftOrbTailStreak *p;

    f32 fadeIn = 0.0f;
    f32 fadeOut;
    f32 tailFade = 1.0f;

    for (p = w->streakHead; p != NULL; p = p->next) {
        f32 t = p->age / p->life;

        p->age += 1.0f;
        if (t < prm->turn) {
            p->size += p->sizeStepA;
            p->speed += p->speedStepA;
        } else {
            p->size += p->sizeStepB;
            p->speed += p->speedStepB;
        }
        if (t < col->alphaTurn) {
            p->alpha += p->alphaStepA;
        } else {
            p->alpha += p->alphaStepB;
        }
        Vec3_Scale(&v, &p->dir, p->speed);
        Vec3_Add(&p->local, &p->local, &v);
        if (p->node->len <= p->local.z) {
            f32 rest = p->local.z - p->node->len;

            if (p->node->next != NULL) {
                if (p->node->next->len <= rest) {
                    for (;;) {
                        p->node = p->node->next;
                        p->dist += p->node->len;
                        if (p->node->next != NULL && p->node->next->next == NULL) {
                            p->flags |= EFT_ORB_STREAK_FADE;
                        }
                        if (p->node->len <= rest) {
                            rest -= p->node->len;
                        } else {
                            break;
                        }
                        if (p->node->next == NULL) {
                            break;
                        }
                    }
                } else {
                    p->node = p->node->next;
                    p->dist += p->node->len;
                }
                p->dist += rest;
                Vec4_Set(&p->local, 0.0f, 0.0f, rest, 1.0f);
            }
        }
        Mtx_MulVec4(&p->pos, &p->node->mtx, &p->local);

        if (w->chainLen * prm->tailFade > 0.0f) {
            tailFade = 1.0f - EFT_ORB_CLAMP01(p->dist / (w->chainLen * (prm->tailFade * w->grow)));
        } else {
            tailFade = 0.0f;
        }
        if (w->flags & EFT_ORB_ANIM) {
            p->frame += 1.0f;
            if (w->texFrames <= p->frame) {
                p->frame = 0.0f;
            }
        }
        if (!((w->flags & EFT_ORB_MOVING) && !(w->flags & EFT_ORB_END))) {
            p->flags |= EFT_ORB_STREAK_FADE;
        }
        if (p->node->next == NULL || p->node->next->next == NULL) {
            p->flags |= EFT_ORB_STREAK_FADE;
        }
        if (p->flags & EFT_ORB_STREAK_FADE) {
            if (p->fadeOutLen > 0.0f) {
                fadeOut = EFT_ORB_CLAMP01(p->fadeOut / p->fadeOutLen);
            } else {
                fadeOut = 0.0f;
            }
        } else {
            fadeOut = 1.0f;
        }
        if (p->fadeInLen > 0.0f) {
            fadeIn = EFT_ORB_CLAMP01(p->fadeIn / p->fadeInLen);
        } else {
            fadeIn = 1.0f;
        }
        if (p->flags & EFT_ORB_STREAK_FADE) {
            if (p->fadeOut > 0.0f) {
                p->fadeOut -= 1.0f;
            }
        } else if (p->fadeIn < p->fadeInLen) {
            p->fadeIn += 1.0f;
        }
        p->color.w = (col->color + arg->kind)->w * p->alpha * (1.0f - t) * fadeIn * fadeOut * tailFade;
        if (p->color.w <= 0.0f || p->size <= 0.0f) {
            p->flags &= ~EFT_ORB_STREAK_SHOW;
            if (fadeIn > 0.0f) {
                p->flags |= EFT_ORB_STREAK_FADE;
            }
        } else {
            p->flags |= EFT_ORB_STREAK_SHOW;
        }
        if (!(!(p->flags & EFT_ORB_STREAK_FADE) || (p->flags & EFT_ORB_STREAK_SHOW))) {
            p->flags |= EFT_ORB_STREAK_DEAD;
        }
        if (p->age >= p->life || (p->flags & EFT_ORB_STREAK_DEAD)) {
            EFT_ORB_UNLINK(w->streakHead, w->streakTail, p);
            p->flags = 0;
        }
    }
    for (p = w->streakTail; p != NULL; p = p->prev) {
        if (!(p->flags & EFT_ORB_STREAK_FADE)) {
            if (p->prev != NULL) {
                Vec3_Sub(&v, &p->pos, &p->prev->pos);
                if (Vec3_Length(&v) > p->size * 1.5f) {
                    gap = 1;
                }
            }
            if (gap) {
                p->flags |= EFT_ORB_STREAK_FADE;
            }
        }
    }
}

/* Draws every visible streak as a camera-facing quad. */
void EftOrbTail_DrawStreaks(EftOrbTail *w) {
    EftAbTex *tex = w->tex;
    EftOrbTailParam *prm = w->arg.param;
    EftAbVec uv = { 0.0f, 0.0f, 0.0f, 0.0f };
    EftOrbTailStreak *p;

    for (p = w->streakHead; p != NULL; p = p->next) {
        if (p->flags & EFT_ORB_STREAK_SHOW) {
            if (w->flags & EFT_ORB_ANIM) {
                s32 f;

                if (p->frame < 0.0f) {
                    f = 0;
                } else if (w->texFrames < p->frame) {
                    f = w->texFrames;
                } else {
                    f = p->frame;
                }
                uv.x = w->uv[f][0];
                uv.y = w->uv[f][1];
                uv.z = w->uv[f][2];
                uv.w = w->uv[f][3];
            } else {
                uv.x = 0.0f;
                uv.y = 0.0f;
                uv.z = 1.0f;
                uv.w = 1.0f;
            }
            EftPrim_DrawQuadDepthScaled(&p->pos, &p->color, p->size, p->size, uv.x, uv.y, uv.z, uv.w, p->roll, prm->otZ,
                                        0, tex->entry[w->texIdx].tex0, 2.0f);
        }
    }
}

/* Returns a free streak of the pool, searching round-robin from where the last search ended. */
EftOrbTailStreak *EftOrbTail_AllocStreak(void) {
    EftOrbTailMgr *mgr = gEftOrbTail->mgr;
    u32 next = mgr->streakNext;
    u32 i;
    u32 start;

    if (next >= EFT_ORB_STREAKS) {
        mgr->streakNext = 0;
        next = 0;
    }
    i = next;
    start = i;
    do {
        EftOrbTailStreak *p = &mgr->streak[i];

        i++;
        if (i >= EFT_ORB_STREAKS) {
            i = 0;
        }
        if (p->flags == 0) {
            mgr->streakNext = i;
            return p;
        }
    } while (i != start);
    return NULL;
}

/* Starts the three burst emitters at the head offset in front of the ball. */
void EftOrbTail_StartBurst(EftOrbTail *w) {
    EftOrbBurstArg arg;
    EftOrbTailArg *src = &w->arg;
    EftOrbTailParam *prm = src->param;
    s32 i;

    {
        EftOrbBurstArg init = { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, 0, 0, 0.0f, 1.0f };

        init.objId = src->objId;
        arg = init;
    }
    {
        EftAbVec pos = { 0.0f, 0.0f, 0.0f, 1.0f };

        for (i = 0; i < 3; i++) {
            arg.image = gEftOrbTail->mgr->burstImage[i];
            arg.palette = gEftOrbTail->mgr->burstPalette[src->kind];
            arg.rate = prm->burstRate[i] * w->scale;
            arg.size = prm->burstSize[i] * w->scale;
            arg.tex = &gEftOrbTail->mgr->burstTex[i];
            w->burst[i] = EftPtcl_Create(&arg);
            Vec3_ScaleAdd(&pos, &w->dir, prm->headOffset * w->scale, &w->pos);
            EftPtcl_SetPos(w->burst[i], &pos);
            EftPtcl_SetDir(w->burst[i], &w->dir);
            EftPtcl_SetTexture(w->burst[i], &gEftOrbTail->mgr->burstTex[i], 0, 0);
        }
    }
}

/* Polls the three burst emitters (the results are not used). */
void EftOrbTail_PollBurst(EftOrbTail *w) {
    s32 i;

    for (i = 0; i < 3; i++) {
        EftPtcl_IsAlive(w->burst[i]);
    }
}

/* Stops the burst emitters that still exist and forgets the others. */
void EftOrbTail_StopBurst(EftOrbTail *w) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (EftPtcl_IsAlive(w->burst[i])) {
            EftPtcl_Stop(w->burst[i]);
        } else {
            w->burst[i] = NULL;
        }
    }
}

/* Builds the texture rectangle of each animation frame (a texCols x texRows grid, at most 16 frames). */
void EftOrbTail_InitFrames(EftOrbTail *w) {
    EftOrbTailParam *prm = w->arg.param;
    f32 step[2] = { 0.0f, 0.0f };
    u8 n = 0;
    s32 row = 0;
    s32 col;

    w->texFrames = prm->texCols * prm->texRows;
    if (w->texFrames > 1.0f && w->texFrames <= 16.0f) {
        step[0] = 1.0f / prm->texCols;
        step[1] = 1.0f / prm->texRows;
        for (; row < prm->texRows; row++) {
            for (col = 0; col < prm->texCols; col++) {
                w->uv[n][0] = step[0] * col;
                w->uv[n][1] = step[1] * row;
                w->uv[n][2] = step[0] * col + step[0];
                w->uv[n][3] = step[1] * row + step[1];
                n++;
            }
        }
        w->flags |= EFT_ORB_ANIM;
    }
}

/* Binds the task to a texture table: which entries are its image and palette, and where its TEX0 goes. */
void EftOrbTail_SetTex(EftOrbTail *w, EftAbTex *tex, s32 image, s32 palette) {
    w->tex = tex;
    if (image == palette) {
        w->texIdx = palette;
    } else {
        w->texIdx = image + palette;
    }
    w->image = *(tex->entry + image);
    w->palette = *(tex->entry + palette);
}

/* Builds the task's TEX0 (image + palette) once per frame. */
void EftOrbTail_UpdateTex(EftOrbTail *w) {
    EftAbTex *tex = w->tex;

    if (tex != NULL) {
        if (!(tex->ready & (1 << w->texIdx))) {
            u64 tex0 = EftVram_AddImage(&w->image, 1, 0);

            tex0 |= (u64)EftVram_AddClut(&w->palette) << 37;
            tex->entry[w->texIdx].tex0 = tex0;
            tex->ready |= 1 << w->texIdx;
        }
    }
}

/* Creates an orb tail for a fighter; kind is the fighter's aura type. Returns the task or NULL. */
/* FAKE MATCH (permuter): the manager pointer is read into a local in front of a `do { } while (0)` that holds
   the two tests on it, and read again through the global afterwards. The wrapper is the artificial part; it
   stands for a statement macro or a removed loop. Written as three plain tests, 2 of 45 instructions differ: the
   store of unk10.w (swc1 $f0,28(sp)) and of kind (sb s0,32(sp)) are exchanged, because the first scheduling pass
   gives its one memory slot per cycle to the load of gEftOrbTail (it feeds the branch) before the 1.0f load.
   The local alone, the wrapper alone, the wrapper around all three tests, or `mgr` used for the reads behind the
   tests do not match; the wrapper around the first test on `mgr` alone does. */
EftAbTask *EftOrbTail_Create(s32 objId, s32 kind) {
    EftOrbTailArg arg = { NULL, NULL, { 0.0f, 0.0f, 0.0f, 1.0f }, kind, objId };
    EftOrbTailMgr *mgr;

    if (gEftOrbTail == NULL) {
        return NULL;
    }
    mgr = gEftOrbTail->mgr;
    do {
        if (mgr == NULL) {
            return NULL;
        }
        if (mgr->list == NULL) {
            return NULL;
        }
    } while (0);
    arg.color = gEftOrbTail->mgr->color;
    arg.param = gEftOrbTail->mgr->param;
    return BtlTaskList_AddTail(gEftOrbTail->mgr->list, gEftOrbTailClass, &arg);
}

/* Asks the task to start its burst emitters. Returns 1 when the task is a live orb tail. */
s32 EftOrbTail_Burst(EftAbTask *task) {
    EftOrbTail *w;

    if (gEftOrbTail == NULL) {
        return 0;
    }
    if (gEftOrbTail->mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftOrbTail_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_ORB_ACTIVE) {
        w->flags |= EFT_ORB_BURST;
        return 1;
    }
    return 0;
}

/* Asks the task to stop feeding and die when its streaks are gone. No caller. */
s32 EftOrbTail_End(EftAbTask *task) {
    EftOrbTail *w;

    if (gEftOrbTail == NULL) {
        return 0;
    }
    if (gEftOrbTail->mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftOrbTail_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_ORB_ACTIVE) {
        w->flags |= EFT_ORB_END;
        return 1;
    }
    return 0;
}

/* Asks the task to die on its next update. */
s32 EftOrbTail_Kill(EftAbTask *task) {
    EftOrbTail *w;

    if (gEftOrbTail == NULL) {
        return 0;
    }
    if (gEftOrbTail->mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftOrbTail_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_ORB_ACTIVE) {
        w->flags |= EFT_ORB_KILL;
        return 1;
    }
    return 0;
}

/* Moves the ball. The first call invents a previous position one unit towards the fighter's node 0x2E from node 3. */
void EftOrbTail_SetPos(EftAbTask *task, EftAbVec pos) {
    EftAbVec node[2] = { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } };
    EftAbVec d = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (gEftOrbTail != NULL && gEftOrbTail->mgr != NULL) {
        if (task != NULL && task->cls[0] == EftOrbTail_Update) {
            EftOrbTail *w = task->work;

            if (w != NULL) {
                if (w->flags & EFT_ORB_ACTIVE) {
                    if (w->flags & EFT_ORB_PLACED) {
                        Vec4_Copy(&w->prevPos, &w->pos);
                    } else {
                        BtlCharApi_GetNodePos(w->arg.objId, 0x2E, &node[0]);
                        BtlCharApi_GetNodePos(w->arg.objId, 3, &node[1]);
                        Vec3_Sub(&d, &node[1], &node[0]);
                        Vec3_Normalize(&d, &d);
                        Vec3_Add(&w->prevPos, &pos, &d);
                        w->flags |= EFT_ORB_PLACED;
                    }
                    Vec4_Copy(&w->pos, &pos);
                }
            }
        }
    }
}

/* Returns 1 when the task is a live orb tail. No caller. */
s32 EftOrbTail_IsActive(EftAbTask *task) {
    EftOrbTail *w;

    if (gEftOrbTail == NULL) {
        return 0;
    }
    if (gEftOrbTail->mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftOrbTail_Update) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (!(w->flags & EFT_ORB_ACTIVE)) {
        return 0;
    }
    return 1;
}

/* Five empty functions without callers (three return 0). */
s32 EftOrbTail_Stub0(void) {
    return 0;
}

s32 EftOrbTail_Stub1(void) {
    return 0;
}

s32 EftOrbTail_Stub2(void) {
    return 0;
}

void EftOrbTail_Stub3(void) {
}

void EftOrbTail_Stub4(void) {
}
