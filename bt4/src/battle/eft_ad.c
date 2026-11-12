#include "common.h"
#include "battle/eft_ad.h"

/*
 * 0x1A62C8..0x1A7018: the handle functions of the zap module (effect pack part kind 12; the module starts at
 * 0x1A3B00 in the file before this one) and the fighter shock effect (fighter effect request 6).
 * Both are visual only. See include/battle/eft_ad.h.
 */

/* Argument of EftAnimPart_Create (EftAnimPartArg in include/battle/eft_aa.h). */
typedef struct EftAdAnimPartArg {
    /* 0x00 */ s32 chr;
    /* 0x04 */ s32 mode;
    /* 0x08 */ f32 life;
    /* 0x0C */ s32 padC;
    /* 0x10 */ EftAdVec pos;
    /* 0x20 */ EftAdVec dir;
    /* 0x30 */ f32 size;
    /* 0x34 */ void *tex;
    /* 0x38 */ void *res;
    /* 0x3C */ s32 pad3C;
} EftAdAnimPartArg; /* 0x40 */

extern EftZapMgr *gEftZapMgr;
extern void *gEftZapClass[6];
extern void *gEftShockClass[6];

extern void *memset(void *dst, s32 c, u32 n);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void EftZap_Update(EftAdTask *task);                  /* the zap task's update (file before this one) */
extern u64 EftVram_AddImage(EftAdTex *tex, s32 a1, s32 a2);     /* TEX0 of a table entry */
extern u64 EftVram_AddClut(EftAdTex *tex);                     /* palette block of a table entry */
extern void BtlTask_SetOwnerTag(void *task, s32 flag);             /* tags a task with its character (0x800 / 0x1000) */
extern EftAdTask *BtlTask_GetParent(EftAdTask *task);            /* the task that owns the list this task is in */
extern void BtlTask_SetDead(EftAdTask *task);                  /* kills the task */
extern void EftTexSet_Load32(EftAdTexSet *set, void *data);     /* builds a texture set */
extern void *BtlTaskList_AddTail(void *list, void **cls, void *arg);
extern void *BtlTask_CreateChildList(EftAdTask *task, s32 count, s32 workSize);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 type);
extern f32 BtlScene_GetCharScale(s32 objId);
extern void *BtlScene_GetCharPackEntry(s32 objId, s32 idx);
extern void *BtlScene_GetPackEntry(void *pack, s32 idx);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_GetDir(s32 objId, Vec4 *out);
extern s32 ScrWarp_Spawn(s32 objId, Vec4 *pos, f32 seconds, f32 radius, f32 width, f32 speed, f32 jitter);
extern void *EftAnimPart_Create(EftAdAnimPartArg *arg);
extern s32 EftAnimPart_Kill(void *task);
extern s32 EftAnimPart_SetType(void *task, s32 type);
extern s32 EftAnimPart_IsAlive(void *task);
extern void EftChar_SetList(s32 objId, s32 kind, void *list);
extern void *EftChar_GetList(s32 objId, s32 kind);

/* The original reaches part[] from the address of the word at +4 (part[n] is word n + 4 from there). */
#define EFT_SHOCK_PARTS(w) ((void **)&(w)->pos.y)

#define IS_ZAP(task) ((task)->cls[0] == (void *)EftZap_Update)

/* Copies key idx (0..2) of every animated value from the zap's key block into the values in use. */
void EftZap_LoadKey(EftZap *w, s32 idx) {
    EftZapKeys *keys = w->arg.keys;

    Vec4_Copy(&w->curA, &keys->a[idx]);
    Vec4_Copy(&w->curB, &keys->b[idx]);
    Vec4_Copy(&w->curC, &keys->c[idx]);
    Vec4_Copy(&w->curD, &keys->d[idx]);
    w->curE[0] = keys->e[idx][0];
    w->curE[1] = keys->e[idx][1];
    w->curF[0] = keys->f[idx][0];
    w->curF[1] = keys->f[idx][1];
    w->curG[0] = keys->g[idx][0];
    w->curG[1] = keys->g[idx][1];
    w->curH = keys->h[idx];
    w->curI = keys->i[idx];
    w->curJ = keys->j[idx];
}

/* Takes a free point from the manager's 35 (searching round robin from the last one handed out), puts it at the
   chain's position and appends it to the chain. Returns 0 when all 35 are in use. */
s32 EftZap_AddNode(EftZapLine *chain) {
    u8 i;
    EftZapNode *node;

    if (gEftZapMgr->nodeCur >= EFT_ZAP_NODES) {
        gEftZapMgr->nodeCur = 0;
    }
    i = gEftZapMgr->nodeCur;
    do {
        node = &gEftZapMgr->nodes[i];
        i++;
        if (i >= EFT_ZAP_NODES) {
            i = 0;
        }
        if (node->flags == 0) {
            Vec4_Copy(&node->pos, &chain->pos);
            node->next = NULL;
            node->prev = NULL;
            node->flags |= 1;
            if (chain->head == NULL) {
                chain->head = node;
                chain->tail = node;
            } else {
                node->prev = chain->tail;
                chain->tail->next = node;
                chain->tail = node;
            }
            gEftZapMgr->nodeCur = i;
            return 1;
        }
    } while (i != gEftZapMgr->nodeCur);
    return 0;
}

/* Picks the two texture table entries of the zap and the image slot (a when both are the same, else a + b). */
void EftZap_SetTexPair(EftZap *w, EftAdTex *tbl, s32 a, s32 b) {
    if (a == b) {
        w->texIdx = b;
    } else {
        w->texIdx = a + b;
    }
    w->texPair[0] = tbl[a];
    w->texPair[1] = tbl[b];
}

/* Builds the GS TEX0 of the zap's image slot the first time a zap uses it. */
void EftZap_LoadTex(EftZap *w) {
    EftZapTexObj *tex = w->arg.tex;

    if (tex != NULL) {
        if (!(tex->loaded & (1U << w->texIdx))) {
            u64 tex0 = EftVram_AddImage(&w->texPair[0], 1, 0);

            tex0 |= (u64)EftVram_AddClut(&w->texPair[1]) << 37;
            tex->entry[w->texIdx].tex0 = tex0;
            tex->loaded |= 1U << w->texIdx;
        }
    }
}

/* Part kind 12, create: adds a zap task. Returns the task, NULL when the module is not loaded. */
void *EftZap_Create(EftZapArg *arg) {
    if (gEftZapMgr == NULL || arg == NULL || gEftZapMgr->tasks == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftZapMgr->tasks, gEftZapClass, arg);
}

/* Part kind 12, kill: the next update removes the task. */
void EftZap_Kill(EftAdTask *task) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                w->flags |= EFT_ZAP_KILL;
            }
        }
    }
}

/* Stops the zap: through its fade when it has a fade time or a fade delay, else at once. */
void EftZap_Stop(EftAdTask *task) {
    EftZap *w;
    s32 now = 1;

    if (gEftZapMgr != NULL) {
        if (task != NULL && IS_ZAP(task)) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_ZAP_ALIVE) {
                    if (w->fade > 0.0f) {
                        w->flags |= EFT_ZAP_FADE;
                        now = 0;
                    }
                    if (w->fadeDelay > 0.0f) {
                        w->flags |= EFT_ZAP_FADE_WAIT;
                        now = 0;
                    }
                    if (now) {
                        if (w->arg.prm->flags & 0x100) {
                            w->flags |= EFT_ZAP_STOP_20;
                        }
                        w->flags |= EFT_ZAP_STOP_20 | EFT_ZAP_STOP_NOW;
                    }
                }
            }
        }
    }
}

/* Moves the zap's origin. */
void EftZap_SetPos(EftAdTask *task, EftAdVec pos) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                Vec4_Copy(&w->arg.pos, (Vec4 *)&pos);
            }
        }
    }
}

/* Same as EftZap_SetPos (the pack library calls this one for a "warp" move). */
void EftZap_WarpPos(EftAdTask *task, EftAdVec pos) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                Vec4_Copy(&w->arg.pos, (Vec4 *)&pos);
            }
        }
    }
}

/* Sets the zap's direction. */
void EftZap_SetDir(EftAdTask *task, EftAdVec dir) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                Vec4_Copy(&w->arg.dir, (Vec4 *)&dir);
            }
        }
    }
}

/* Sets the zap's size. */
void EftZap_SetSize(EftAdTask *task, f32 size) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                w->arg.size = size;
            }
        }
    }
}

/* Sets the zap's rate (seconds) and its frame count. No caller. */
void EftZap_SetRate(EftAdTask *task, f32 rate) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                w->arg.rate = rate;
                w->rateFrames = rate * 30.0f;
            }
        }
    }
}

/* Gives the zap another texture object and image pair. No caller. */
s32 EftZap_SetTex(EftAdTask *task, EftZapTexObj *tex, s32 a, s32 b) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                w->arg.tex = tex;
                EftZap_SetTexPair(w, tex->entry, a, b);
            }
        }
    }
}

/* Sets the start delay in frames. */
void EftZap_SetDelay(EftAdTask *task, s32 frames) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                w->delay = frames;
            }
        }
    }
}

/* Sets the frames between a stop and the fade. */
void EftZap_SetFadeDelay(EftAdTask *task, s32 frames) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                w->fadeDelay = frames;
            }
        }
    }
}

/* Sets the fade length in frames. */
void EftZap_SetFadeTime(EftAdTask *task, s32 frames) {
    EftZap *w;

    if (gEftZapMgr != NULL && task != NULL && IS_ZAP(task)) {
        w = task->work;
        if (w != NULL) {
            if (w->flags & EFT_ZAP_ALIVE) {
                w->fade = frames;
                w->fadeTime = frames;
            }
        }
    }
}

/* Sets work flag 0x20000 (the pack library does it for a part whose definition has flag 0x20). */
s32 EftZap_SetFlag20000(EftAdTask *task) {
    EftZap *w;

    if (gEftZapMgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (!IS_ZAP(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_ZAP_ALIVE) {
        w->flags |= EFT_ZAP_FLAG_20000;
        return 1;
    }
    return 0;
}

/* Writes the byte the creation argument carries at +0x40. No caller. */
s32 EftZap_SetUnk40(EftAdTask *task, s32 value) {
    EftZap *w;

    if (gEftZapMgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (!IS_ZAP(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_ZAP_ALIVE) {
        w->arg.unk40 = value;
        return 1;
    }
    return 0;
}

/* Part kind 12, is-alive. */
s32 EftZap_IsAlive(EftAdTask *task) {
    EftZap *w;

    if (gEftZapMgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (!IS_ZAP(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_ZAP_ALIVE) {
        return 1;
    }
    return 0;
}

/* ---- fighter shock effect (fighter effect request 6) ----------------------------------------------------- */

/* Task init: remembers the fighter and tags the task with its character. */
void EftShock_Init(EftAdTask *task, s32 *arg) {
    EftShock *w = task->work;

    memset(w, 0, sizeof(EftShock));
    w->objId = arg[0];
    BtlTask_SetOwnerTag(task, arg[0] == 0 ? 0x800 : 0x1000);
    w->timer = 2;
}

/* Task term: kills the two parts. */
void EftShock_Term(EftAdTask *task) {
    EftShock *w = task->work;

    if (w->part[0] != NULL) {
        EftAnimPart_Kill(w->part[0]);
        w->part[0] = NULL;
    }
    if (w->part[1] != NULL) {
        EftAnimPart_Kill(w->part[1]);
        w->part[1] = NULL;
    }
    w->flags = 0;
}

/* Task update: follows fighter node 3, spawns one screen shock wave, starts two animated parts two frames
   apart and ends when both are gone. */
void EftShock_Update(EftAdTask *task) {
    EftShock *w = task->work;
    EftShockRes *res = ((EftShockMgr *)BtlTask_GetParent(task)->work)->res;
    s32 *objId = &w->objId;
    s32 alive;
    s32 i;

    if (!BtlScene_IsEffectStopped(*objId, 3)) {
        BtlCharApi_GetNodePos(*objId, 3, &w->pos);
        if (!(w->flags & EFT_SHOCK_WARPED)) {
            ScrWarp_Spawn(*objId, &w->pos, 0.8f, 10.0f, 50.0f, 8.0f, 0.4f);
            w->flags |= EFT_SHOCK_WARPED;
        }
        if (!(w->flags & EFT_SHOCK_WAIT)) {
            EftAdAnimPartArg arg;
            void **part = EFT_SHOCK_PARTS(w);

            arg.chr = *objId;
            arg.mode = 4;
            arg.life = 0.8f;
            arg.size = BtlScene_GetCharScale(*objId) * 2.0f;
            arg.tex = res->anim;
            arg.res = &res->tex;
            Vec4_Copy((Vec4 *)&arg.pos, &w->pos);
            BtlCharApi_GetDir(*objId, (Vec4 *)&arg.dir);
            part[w->count + 4] = EftAnimPart_Create(&arg);
            EftAnimPart_SetType(part[w->count + 4], 3);
            BtlTask_SetOwnerTag(part[w->count + 4], *objId == 0 ? 0x800 : 0x1000);
            w->count++;
            w->flags |= EFT_SHOCK_WAIT;
        }
        if (w->timer > 0) {
            if (--w->timer == 0) {
                w->flags &= ~EFT_SHOCK_WAIT;
            }
        }
        if (w->count == 2) {
            alive = 0;
            for (i = 0; i < 2; i++) {
                if (w->part[i] != NULL && EftAnimPart_IsAlive(w->part[i])) {
                    alive = 1;
                }
            }
            if (!alive) {
                w->flags |= EFT_SHOCK_DONE;
            }
        }
    }
    if (w->flags & EFT_SHOCK_DONE) {
        BtlTask_SetDead(task);
    }
}

/* Task reset: kills the task. */
void EftShock_Reset(EftAdTask *task) {
    BtlTask_SetDead(task);
}

/* Task post-update: nothing. */
void EftShock_PostUpdate(EftAdTask *task) {
}

/* Task draw: nothing (the parts draw themselves). */
void EftShock_Draw(EftAdTask *task) {
}

/* Manager init (per fighter; arg = {object id, list kind}): loads the textures of the fighter's effect pack
   entry 6 and registers a list of two tasks as the fighter's list of that kind. */
void EftShockMgr_Init(EftAdTask *task, s32 *arg) {
    EftShockMgr *mgr = task->work;
    EftShockRes *res;
    void *list;

    mgr->res = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftShockRes));
    memset(mgr->res, 0, sizeof(EftShockRes));
    res = mgr->res;
    res->pack = BtlScene_GetCharPackEntry(arg[0], 6);
    if (res->pack != NULL) {
        res->anim = BtlScene_GetPackEntry(res->pack, 1);
        res->texData = BtlScene_GetPackEntry(res->pack, 2);
        EftTexSet_Load32(&res->tex, res->texData);
    }
    list = BtlTask_CreateChildList(task, 2, sizeof(EftShock));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term: frees the block. */
void EftShockMgr_Term(EftAdTask *task) {
    EftShockMgr *mgr = task->work;

    if (mgr->res != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->res);
    }
}

/* Manager update: clears the texture set's per-frame word. */
void EftShockMgr_Update(EftAdTask *task) {
    EftShockMgr *mgr = task->work;

    mgr->res->tex.unk204 = 0;
}

/* Fighter effect request 6: starts the shock effect of this fighter (arg[0] = object id). Returns the task,
   NULL when the fighter has no list for it. */
void *EftShock_Start(s32 *arg) {
    s32 init[4];
    void *list;

    init[0] = arg[0];
    list = EftChar_GetList(init[0], 4);
    if (list == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(list, gEftShockClass, init);
}
