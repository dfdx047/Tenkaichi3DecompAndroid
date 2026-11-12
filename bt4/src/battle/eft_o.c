#include "common.h"
#include "battle/eft_o.h"

/*
 * Effect tasks, 0x167E68..0x1699D0. Two modules, both VISUAL ONLY (no hit record, no fighter or battle state is
 * written; fighters are read through BtlCharApi getters):
 *
 * 1. 0x167E68..0x168600  the body lightning task and its manager, the second half of the EftBolt module of
 *    eft_n.c (class tables 0x2C3A78 task / 0x2C3A60 manager). One task per fighter while the fighter's parameter
 *    flags have bit 4 or 8 (EftAura_UpdateLightning calls EftBolt_Enable / EftBolt_Disable). The update does
 *    nothing but texture stepping while the battle is paused (battle flag 0x100).
 *
 * 2. 0x168600..0x1699D0  effect pack part kind 2, "rays" (class tables 0x2C3AA8 task / 0x2C3A90 manager): up
 *    to 10 camera-facing quads fanned around a point. EftEmit_SpawnType2 (eft_h.c) creates, places, stops and
 *    kills a part through the entry points at 0x168600..0x168808; EftEmit_KillAll / EftEmit_UpdateAlive
 *    (eft_i.c) call EftRays_Kill / EftRays_IsAlive. libc rand(): two per ray at creation and two per ray each
 *    time its length flicker turns round (appearance only).
 */

typedef struct EftOBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} EftOBattleWork;

/* The view being drawn (gBtlCamView). */
typedef struct EftOView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ Mtx44 camMtx;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 screenMtx;
} EftOView;

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);

extern void Vec4_Copy(void *dst, void *src);
extern void Vec4_Set(void *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Add(void *dst, void *a, void *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(void *dst, Mtx44 *m, void *src);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b); /* matrix product */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);         /* inverse of a rotation + translation matrix */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Z */
extern void Mtx_ScaleDiag(Mtx44 *dst, Mtx44 *src, void *scale); /* scale by a vector */
extern void Vu0Cur_Push(void);                            /* VU0 matrix stack: push */
extern void Vu0Cur_Pop(void);                            /* VU0 matrix stack: pop */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                        /* VU0 current matrix = m */
extern s32 Vu0Cur_ProjectPoints(void *out, void *pos, s32 count);  /* project to screen */
extern void ClipVtx_Set(void *vtx, void *pos, void *uv, void *col); /* fills one polygon vertex */
extern f32 EftMath_WrapAngle(f32 angle);
extern void EftGfx_DrawPolyAvgZFront(void *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                                     s32 zOfs);

extern void *BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(void *pool, s32 size);
extern void BtlPool_Free(void *pool, void *p);
extern EftOBattleWork *Battle_GetWork(void);
extern s32 BtlScene_GetCharCount(void);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 BtlScene_IsCharInView(s32 objId);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern void *BtlTask_CreateChildList(EftOTask *task, s32 count, s32 workSize);
extern EftOTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftOTask *task);                  /* kill a task */
extern u64 EftVram_AddTex(void *entry, s32 a, s32 b);        /* advances a texture, returns TEX0 */
extern u64 EftVram_AddImage(void *set, s32 a, s32 b);
extern u64 EftVram_AddClut(void *entry);
extern void EftTexSet_Load32(void *set, s32 *pack);            /* binds a texture set to a pack */

extern s32 BtlCharApi_ObjGetParamFlags0(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_IsHidden(s32 objId);
extern s32 BtlCharApi_IsModelNew(s32 objId);
extern s32 BtlCharApi_ObjTestFlagBit21(s32 objId);

extern void EftBolt_FreeSegs(EftOBolt *bolt);
extern void EftBolt_UpdateAll(EftOBoltWork *work, s32 objId);
extern void EftBolt_DrawAll(EftOBoltWork *work, f32 alpha);
extern void EftBolt_UpdateFlashes(EftOBoltWork *work, s32 objId);
extern void EftBolt_DrawFlashes(EftOBoltWork *work, f32 alpha);
extern void EftBolt_Spawn(EftOBoltWork *work, s32 objId);

extern EftOView *gBtlCamView;
extern void *gEftBoltTaskClass[6];
extern void *gEftRaysClass[6];

/* ---- body lightning: task and manager -------------------------------------------------------------------- */

/* Advances the two shared textures once per frame and gives the work block this frame's TEX0. */
/* (`one` is needed: with the literal 1 in both stores the first branch is scheduled differently.) */
void EftBolt_UpdateTex(EftOBoltWork *w) {
    EftOTexSet *set = &gEftBoltPool->tex[0].set;
    EftOBoltTex *t = &gEftBoltPool->tex[0];
    u64 frame;

    if (!t->ready) {
        u64 base;
        s32 i;
        s32 one;

        t->tex0 = EftVram_AddImage(set, 1, 0);
        frame = EftVram_AddClut(set);
        one = 1;
        w->tex0 = t->tex0 | (frame << 37);
        t->flags = one;
        t->ready = one;
        t->frame = frame;
        set = &gEftBoltPool->tex[1].set;
        base = EftVram_AddImage(set, 1, 0);
        for (i = 0; i < set->count; i++) {
            set->entry[i].tex0 = base | ((u64)EftVram_AddClut(&set->entry[i]) << 37);
        }
    } else {
        if (!(t->flags & 1)) {
            frame = EftVram_AddClut(set);
        } else {
            frame = t->frame;
        }
        w->tex0 = t->tex0 | (frame << 37);
    }
}

/* Reads what the task needs from the fighter: parameter flags and body scale. */
void EftBolt_ReadChar(EftOBoltWork *w, s32 objId) {
    w->paramFlags = BtlCharApi_ObjGetParamFlags0(objId);
    w->scale = BtlCharApi_GetHeight(objId) / 19.35f;
}

/* Init callback of the per-fighter task. */
void EftBoltTask_Init(EftOTask *task, s32 *arg) {
    EftOBoltWork *w = task->work;

    memset(w, 0, sizeof(EftOBoltWork));
    w->objId = *arg;
    w->flags |= 1;
    EftBolt_ReadChar(w, *arg);
}

/* Term callback: frees the first bolt's joints and clears the fighter's entry. */
void EftBoltTask_Term(EftOTask *task) {
    EftOBoltWork *w = task->work;
    EftOBolt *bolt = w->a;
    s32 *objId = &w->objId;

    EftBolt_FreeSegs(bolt);
    bolt->active = 0;
    gEftBoltPool->tasks[*objId] = NULL;
}

/* Update callback: bolts and flashes unless paused; once stopped, dies when the last bolt and flash are gone. */
void EftBoltTask_Update(EftOTask *task) {
    s32 busy = 0;
    EftOBoltWork *w = task->work;
    s32 *objId = &w->objId;
    s32 i;

    if (BtlCharApi_IsModelNew(*objId)) {
        w->flags |= 8;
    }
    if (w->flags & 8) {
        EftBolt_ReadChar(w, *objId);
        w->flags &= ~8;
    }
    if (!(Battle_GetWork()->flags & 0x100)) {
        if (!(w->flags & 2)) {
            EftBolt_Spawn(w, *objId);
        }
        EftBolt_UpdateAll(w, *objId);
        EftBolt_UpdateFlashes(w, *objId);
        if (w->flags & 2) {
            for (i = 0; i < 10; i++) {
                if (w->a[i].active) {
                    busy = 1;
                    break;
                }
            }
            for (i = 0; i < 20; i++) {
                if (w->b[i].active) {
                    busy = 1;
                    break;
                }
            }
            if (!busy) {
                w->flags |= 4;
            }
        }
    }
    if (w->flags & 4) {
        BtlTask_SetDead(task);
    } else {
        EftBolt_UpdateTex(w);
    }
}

/* Post-update callback: nothing. */
void EftBoltTask_PostUpdate(EftOTask *task) {
}

/* Reset callback: the task dies. */
void EftBoltTask_Reset(EftOTask *task) {
    EftOBoltWork *w = task->work;

    gEftBoltPool->tasks[w->objId] = NULL;
    BtlTask_SetDead(task);
}

/* Draw callback: bolts and flashes, at 0.3 alpha when the camera is inside the fighter's view and its flag 21. */
void EftBoltTask_Draw(EftOTask *task) {
    f32 alpha = 1.0f;
    EftOBoltWork *w = task->work;
    s32 *objId = &w->objId;

    if (!BtlCharApi_IsHidden(*objId)) {
        if (BtlScene_IsCharInView(*objId) && BtlCharApi_ObjTestFlagBit21(*objId)) {
            alpha = 0.3f;
        }
        EftBolt_DrawAll(w, alpha);
        EftBolt_DrawFlashes(w, alpha);
    }
}

/* Init callback of the manager: the joint pool (100 per fighter), the task table and the two texture sets. */
void EftBoltMgr_Init(EftOTask *task) {
    EftOBoltTex *t;
    s32 i;

    gEftBoltPool = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftOBoltPool));
    memset(gEftBoltPool, 0, sizeof(EftOBoltPool));
    gEftBoltPool->count = BtlScene_GetCharCount();
    gEftBoltPool->max = gEftBoltPool->count * 100;
    gEftBoltPool->pool = BtlPool_Alloc(BtlPool_GetCurrent(), gEftBoltPool->max << 6);
    memset(gEftBoltPool->pool, 0, gEftBoltPool->max << 6);
    gEftBoltPool->tasks = BtlPool_Alloc(BtlPool_GetCurrent(), gEftBoltPool->count * 4);
    memset(gEftBoltPool->tasks, 0, gEftBoltPool->count * 4);
    t = &gEftBoltPool->tex[0];
    t->pack = BtlScene_GetCommonEntry(5);
    t = &gEftBoltPool->tex[1];
    t->pack = BtlScene_GetCommonEntry(6);
    for (i = 0; i < 2; i++) {
        EftTexSet_Load32(&gEftBoltPool->tex[i].set, gEftBoltPool->tex[i].pack);
    }
    gEftBoltPool->pack4 = BtlScene_GetCommonEntry(4);
    for (i = 0; i < gEftBoltPool->count; i++) {
        gEftBoltPool->tasks[i] = NULL;
    }
    gEftBoltList = BtlTask_CreateChildList(task, gEftBoltPool->count, sizeof(EftOBoltWork));
}

/* Term callback of the manager. */
void EftBoltMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftBoltPool->tasks);
    BtlPool_Free(BtlPool_GetCurrent(), gEftBoltPool->pool);
    BtlPool_Free(BtlPool_GetCurrent(), gEftBoltPool);
    gEftBoltPool = NULL;
}

/* Update callback of the manager: the textures have not been advanced this frame. */
void EftBoltMgr_Update(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gEftBoltPool->tex[i].ready = 0;
    }
}

/* Reset callback of the manager: nothing. */
void EftBoltMgr_Reset(void) {
}

/* Creates the fighter's lightning task unless it has one. */
s32 EftBolt_Enable(s32 objId) {
    s32 arg[4];
    EftOTask *t;

    if (gEftBoltPool->tasks[objId] == NULL) {
        arg[0] = objId;
        t = BtlTaskList_AddTail(gEftBoltList, gEftBoltTaskClass, arg);
        if (t == NULL) {
            return 0;
        }
        gEftBoltPool->tasks[objId] = t;
    }
    return 1;
}

/* Asks the fighter's lightning task to stop: no new bolts, and it dies when the live ones are gone. */
s32 EftBolt_Disable(s32 objId) {
    EftOTask *t = gEftBoltPool->tasks[objId];

    if (t != NULL) {
        EftOBoltWork *w = t->work;

        if (!(w->flags & 1)) {
            return 0;
        }
        w->flags |= 2;
    }
    return 1;
}

/* ---- effect pack part kind 2: rays ------------------------------------------------------------------------ */

/* Creates a part. */
void *EftRays_Create(EftRaysArg *arg) {
    return BtlTaskList_AddTail(gEftRays->list, gEftRaysClass, arg);
}

/* Starts the part's end: hold, then fade. */
void EftRays_Stop(EftOTask *task) {
    if (EftRays_IsTask(task)) {
        ((EftRays *)task->work)->flags |= EFT_RAYS_STOP;
    }
}

/* The part dies on its next update. */
void EftRays_Kill(EftOTask *task) {
    if (EftRays_IsTask(task)) {
        ((EftRays *)task->work)->flags |= EFT_RAYS_DEAD;
    }
}

/* Places the part. */
void EftRays_SetPos(EftOTask *task, Vec4 *pos) {
    if (EftRays_IsTask(task)) {
        Vec4_Copy(&((EftRays *)task->work)->pos, pos);
    }
}

/* Frames before the part starts to animate. */
void EftRays_SetDelay(EftOTask *task, f32 frames) {
    if (EftRays_IsTask(task)) {
        ((EftRays *)task->work)->delay = frames;
    }
}

/* Frames between the stop and the start of the fade. */
void EftRays_SetHold(EftOTask *task, f32 frames) {
    if (EftRays_IsTask(task)) {
        ((EftRays *)task->work)->hold = frames;
    }
}

/* Length of the fade in frames. */
void EftRays_SetFade(EftOTask *task, f32 frames) {
    if (EftRays_IsTask(task)) {
        EftRays *w = task->work;

        w->fade = frames;
        w->fadeTime = frames;
    }
}

/* The part has not finished fading. */
s32 EftRays_IsAlive(EftOTask *task) {
    s32 ret = EftRays_IsTask(task);

    if (ret) {
        if (((EftRays *)task->work)->flags & EFT_RAYS_ALIVE) {
            return 1;
        }
        return 0;
    }
    return ret;
}

/* Size factor of the part. */
void EftRays_SetSize(EftOTask *task, f32 size) {
    if (EftRays_IsTask(task)) {
        ((EftRays *)task->work)->size = size;
    }
}

/* Init callback of the manager: 20 rays on the free list, 3 part tasks. */
void EftRaysMgr_Init(EftOTask *task) {
    s32 i;

    gEftRays = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftRaysMgr));
    memset(gEftRays, 0, sizeof(EftRaysMgr));
    List_Init(&gEftRays->free);
    for (i = 0; i < 20; i++) {
        List_PushBack(&gEftRays->free, &gEftRays->ray[i].node);
    }
    gEftRays->list = BtlTask_CreateChildList(task, 3, sizeof(EftRays));
}

/* Term callback of the manager. */
void EftRaysMgr_Term(void) {
    if (gEftRays != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftRays);
        gEftRays = NULL;
    }
}

/* Update callback of the manager: nothing. */
void EftRaysMgr_Update(void) {
}

/* Init callback of a part: one ray per entry of the part data, each with two random lengths. */
void EftRays_Init(EftOTask *task, EftRaysArg *arg) {
    f32 one = 1.0f;
    EftRays *w = task->work;
    EftRaysDef *def;
    List *list;
    s32 i;

    memset(w, 0, sizeof(EftRays));
    w->arg = *arg;
    w->alpha = one;
    w->size = one;
    w->flags |= EFT_RAYS_ALIVE;
    if (arg->life > 0.0f) {
        w->life = arg->life * 30.0f;
        w->flags |= EFT_RAYS_TIMED;
    }
    def = w->arg.def;
    list = &w->list;
    i = 0;
    List_Init(list);
    for (; i < def->count; i++) {
        EftRay *ray = EftRays_AllocRay(list);

        if (ray == NULL) {
            return;
        }
        memset(ray, 0, sizeof(EftRay));
        ray->idx = i;
        ray->rot = def->ray[ray->idx].angle * 3.14159265f / 180.0f;
        ray->delay = def->ray[ray->idx].delay;
        ray->flickT = 0.0f;
        ray->flickTime = def->ray[ray->idx].flickTime * 30.0f;
        ray->lenA = def->ray[ray->idx].len.x + def->ray[ray->idx].len.y * ((f32)rand() / 2147483647.0f);
        ray->lenB = def->ray[ray->idx].len.z + def->ray[ray->idx].len.w * ((f32)rand() / 2147483647.0f);
        ray->flickDir = 0;
        ray->len = one;
        ray->pulseT = 0.0f;
        ray->pulseTime = def->ray[ray->idx].colB.w * 30.0f;
        ray->pulseDir = 0;
        ray->col[0] = one;
        ray->col[1] = one;
        ray->col[2] = one;
        ray->col[3] = one;
        if (def->flags & 1) {
            EftRays_SetKey(ray, def, 0);
        } else {
            EftRays_SetKey(ray, def, 2);
        }
    }
}

/* Term callback of a part: gives its rays back. */
void EftRays_Term(EftOTask *task) {
    List *list = &((EftRays *)task->work)->list;
    EftRay *ray = (EftRay *)List_GetHead(list);

    while (ray != NULL) {
        ray = EftRays_FreeRay(list, ray);
    }
}

/* Update callback of a part: delay, life, hold and fade; the rays are stepped until the fade is over. */
void EftRays_Update(EftOTask *task) {
    f32 alpha = 0.0f;
    EftRays *w = task->work;
    EftRaysArg *arg = &w->arg;

    if (!BtlScene_IsEffectStopped(arg->chr, arg->type)) {
        if (w->delay <= 0.0f) {
            EftRays_Step(w);
            if (w->flags & EFT_RAYS_TIMED) {
                w->life -= 1.0f;
                if (w->life <= 0.0f) {
                    w->flags |= EFT_RAYS_STOP;
                }
            }
        } else {
            w->delay -= 1.0f;
        }
        if (w->flags & EFT_RAYS_STOP) {
            if (0.0f < w->hold) {
                w->hold -= 1.0f;
            } else {
                if (0.0f < w->fadeTime) {
                    alpha = w->fade / w->fadeTime;
                    w->fade -= 1.0f;
                }
                if (w->fade <= 0.0f) {
                    w->flags |= EFT_RAYS_DEAD;
                    w->flags &= ~EFT_RAYS_ALIVE;
                    alpha = 0.0f;
                }
                w->alpha = alpha;
            }
        }
    }
    if (w->flags & EFT_RAYS_DEAD) {
        BtlTask_SetDead(task);
    } else {
        w->tex0 = EftRays_GetTex(arg->res, arg->tex);
    }
}

/* Reset callback of a part: the task dies. */
void EftRays_Reset(EftOTask *task) {
    BtlTask_SetDead(task);
}

/* Draw callback of a part. */
void EftRays_Draw(EftOTask *task) {
    EftRays *w = task->work;
    EftRaysArg *arg = &w->arg;
    EftRay *ray;

    if (!BtlScene_IsEffectHidden(arg->chr, arg->type)) {
        ray = (EftRay *)List_GetHead(&w->list);
        Vu0Cur_Push();
        Vu0Cur_LoadMtx(&gBtlCamView->screenMtx);
        for (; ray != NULL; ray = (EftRay *)List_GetNext(&ray->node)) {
            if (ray->flags & 1) {
                EftRays_DrawRay(w, ray);
            }
        }
        Vu0Cur_Pop();
    }
}

#define EFT_RAYS_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((hi) < (x) ? (hi) : (x)))

/* Draws one ray: a quad in the camera plane, scaled, turned by the ray's angle and placed at the part's
   position; its colour is the key colour times the pulse factors, alpha times the part's fade.
   (`grow = 0.0f` has to stand directly in front of the loop: set earlier, the 0.0f stores in between use its
   register and the frame and the callee-saved float registers change.) */
void EftRays_DrawRay(EftRays *w, EftRay *ray) {
    Mtx44 m;
    Mtx44 cam;
    Vec4 out[4];
    Vec4 corner[4];
    Vec4 uv[4];
    u8 verts[9][0x30];
    Vec4 p;
    Vec4 q;
    Vec4 scale;
    Vec4 color;
    Vec4 scr[4];
    EftRaysDef *def;
    Vec4 *pos;
    s32 i = 0;
    EftRaysRect *rect = &ray->rect;
    f32 grow;
    f32 ratio;

    def = w->arg.def;
    Vec4_Set(&corner[0], -1.0f, -1.0f, 1.0f, 1.0f);
    Vec4_Set(&corner[1], 1.0f, -1.0f, 1.0f, 1.0f);
    Vec4_Set(&corner[2], -1.0f, 0.01f, 0.0f, 1.0f);
    Vec4_Set(&corner[3], 1.0f, 0.01f, 0.0f, 1.0f);
    Vec4_Set(&uv[0], 0.0f, 0.05f, 1.0f, 1.0f);
    Vec4_Set(&uv[1], 1.0f, 0.05f, 1.0f, 1.0f);
    Vec4_Set(&uv[2], 0.0f, 1.0f, 1.0f, 1.0f);
    Vec4_Set(&uv[3], 1.0f, 1.0f, 1.0f, 1.0f);
    Mtx_StoreIdentity(&m);
    Mtx_StoreIdentity(&cam);
    pos = &w->pos;
    Mtx_InverseRT(&m, &gBtlCamView->camMtx);
    m.m[3][0] = 0.0f;
    m.m[3][1] = 0.0f;
    m.m[3][2] = 0.0f;
    Mtx_Mul(&cam, &cam, &m);
    ratio = rect->height / rect->ref;
    cam.m[3][2] = 0.0f;
    cam.m[3][1] = 0.0f;
    cam.m[3][0] = 0.0f;
    grow = 0.0f;
    for (; i < 4; i++) {
        Vec4_Copy(&p, &corner[i]);
        Mtx_StoreIdentity(&m);
        scale.x = (rect->width * ray->len + grow) * w->size;
        scale.y = (rect->height + grow) * w->size;
        scale.z = ratio * -10.0f * w->size;
        scale.w = 1.0f;
        Mtx_ScaleDiag(&m, &m, &scale);
        Mtx_RotateZ(&m, &m, ray->rot);
        Mtx_MulVec4(&q, &m, &p);
        Mtx_MulVec4(&out[i], &cam, &q);
        Vec4_Add(&out[i], &out[i], pos);
        out[i].w = 1.0f;
    }
    color.x = rect->color.r * ray->col[0];
    color.y = rect->color.g * ray->col[1];
    color.z = rect->color.b * ray->col[2];
    color.w = rect->color.a * w->alpha;
    color.x = EFT_RAYS_CLAMP(color.x, 0.0f, 255.0f);
    color.y = EFT_RAYS_CLAMP(color.y, 0.0f, 255.0f);
    color.z = EFT_RAYS_CLAMP(color.z, 0.0f, 255.0f);
    color.w = EFT_RAYS_CLAMP(color.w, 0.0f, 255.0f);
    Vu0Cur_ProjectPoints(scr, out, 4);
    for (i = 0; i < 2; i++) {
        ClipVtx_Set(verts[0], &out[i], &uv[i], &color);
        ClipVtx_Set(verts[1], &out[i + 1], &uv[i + 1], &color);
        ClipVtx_Set(verts[2], &out[i + 2], &uv[i + 2], &color);
        EftGfx_DrawPolyAvgZFront(verts, def->ray[ray->idx].blend, 0, 0, 0, 0, w->tex0, 0);
    }
}

/* Steps every ray of a part. */
/* (The re-roll block is written out in both arms: the compiler merges them, but the four uses of 2147483647.0f
   decide which callee-saved register it gets.) */
void EftRays_Step(EftRays *w) {
    f32 t = 0.0f;
    f32 d[4];
    EftRaysDef *def = w->arg.def;
    EftRay *ray = (EftRay *)List_GetHead(&w->list);

    while (ray != NULL) {
        EftRaysRayDef *rd = &def->ray[ray->idx];

        ray->delay--;
        if (ray->delay <= 0) {
            ray->delay = 0;
            ray->flags |= 1;
        }
        if (!(ray->flags & 1)) {
            ray = (EftRay *)List_GetNext(&ray->node);
            continue;
        }
        if (def->flags & 1) {
            EftRays_LerpKey(ray, def);
        }
        if (def->flags & 2) {
            if (ray->flickDir) {
                ray->flickT -= 1.0f;
            } else {
                ray->flickT += 1.0f;
            }
            if (ray->flickT >= ray->flickTime) {
                ray->flickT = ray->flickTime;
                ray->lenA = rd->len.x + rd->len.y * ((f32)rand() / 2147483647.0f);
                ray->lenB = rd->len.z + rd->len.w * ((f32)rand() / 2147483647.0f);
                ray->flickDir ^= 1;
            } else if (ray->flickT <= 0.0f) {
                ray->flickT = 0.0f;
                ray->lenA = rd->len.x + rd->len.y * ((f32)rand() / 2147483647.0f);
                ray->lenB = rd->len.z + rd->len.w * ((f32)rand() / 2147483647.0f);
                ray->flickDir ^= 1;
            }
            t = ray->flickT / ray->flickTime;
            ray->len = ray->lenA + (ray->lenB - ray->lenA) * t;
        }
        if (def->flags & 4) {
            if (ray->pulseDir) {
                ray->pulseT -= 1.0f;
            } else {
                ray->pulseT += 1.0f;
            }
            if (ray->pulseT >= ray->pulseTime) {
                ray->pulseT = ray->pulseTime;
                ray->pulseDir ^= 1;
            } else if (ray->pulseT <= 0.0f) {
                ray->pulseT = 0.0f;
                ray->pulseDir ^= 1;
            }
            d[0] = rd->colB.x - rd->colA[0];
            d[1] = rd->colB.y - rd->colA[1];
            d[2] = rd->colB.z - rd->colA[2];
            t = ray->pulseT / ray->pulseTime;
            ray->col[0] = rd->colA[0] + d[0] * t;
            ray->col[1] = rd->colA[1] + d[1] * t;
            ray->col[2] = rd->colA[2] + d[2] * t;
        }
        ray->rot += rd->spin;
        ray->rot = EftMath_WrapAngle(ray->rot);
        ray = (EftRay *)List_GetNext(&ray->node);
    }
}

/* Takes a ray from the pool and appends it to a part's list. */
EftRay *EftRays_AllocRay(List *list) {
    EftRay *ray = (EftRay *)List_PopFront(&gEftRays->free);

    if (ray == NULL) {
        return NULL;
    }
    List_PushBack(list, &ray->node);
    return ray;
}

/* Gives a ray back to the pool; returns the one after it. */
EftRay *EftRays_FreeRay(List *list, EftRay *ray) {
    EftRay *next = NULL;

    if (ray != NULL) {
        next = (EftRay *)List_GetNext(&ray->node);
        List_Remove(list, &ray->node);
        List_PushBack(&gEftRays->free, &ray->node);
    }
    return next;
}

/* TEX0 of texture n of a part resource; the texture is advanced the first time it is asked for in a frame. */
u64 EftRays_GetTex(EftRaysTex *res, s32 n) {
    if (!(res->stepped & (1U << n))) {
        res->entry[n].tex0 = EftVram_AddTex(&res->entry[n], 1, 0);
        res->stepped |= 1U << n;
    }
    return res->entry[n].tex0;
}

/* Starts a ray's key animation and gives it the colour and size of one key. */
void EftRays_SetKey(EftRay *ray, EftRaysDef *def, s32 key) {
    EftRaysKeyAnim *anim = &ray->anim;
    EftRaysRect *rect = &ray->rect;
    EftRaysKey *k;
    EftRaysRayDef *rd;

    anim->time = 0.0f;
    anim->total = def->ray[ray->idx].keyTime * 30.0f;
    anim->split = anim->total * def->ray[ray->idx].keySplit;
    k = &def->ray[ray->idx].key[key];
    rect->color = k->color;
    rect->width = k->width;
    rect->height = k->height;
    rd = &def->ray[ray->idx];
    rect->ref = rd->key[2].height;
}

/* Advances a ray's key animation: key 0 to 1 over the first part, 1 to 2 over the rest. */
/* (One int temporary `di` reused for the three colour differences, as `d` is for the two sizes: with the
   subtraction written inside each expression, or one temporary per channel, the registers come out differently.) */
void EftRays_LerpKey(EftRay *ray, EftRaysDef *def) {
    EftRaysKeyAnim *anim = &ray->anim;
    EftRaysRayDef *rays = def->ray;
    EftRaysRect *rect = &ray->rect;
    f32 d;
    s32 di;
    EftRaysKey *a;
    EftRaysKey *b;
    s32 ka;
    s32 kb;
    f32 t;

    anim->time += 1.0f;
    if (anim->total < anim->time) {
        anim->time = anim->total;
    }
    if (anim->time < anim->split) {
        t = anim->time / anim->split;
        ka = 0;
        kb = 1;
    } else {
        ka = 1;
        t = anim->time - anim->split;
        kb = 2;
        t /= anim->total - anim->split;
    }
    a = &rays[ray->idx].key[ka];
    b = &rays[ray->idx].key[kb];
    di = b->color.r - a->color.r;
    rect->color.r = a->color.r + (u32)((f32)di * t);
    di = b->color.g - a->color.g;
    rect->color.g = a->color.g + (u32)((f32)di * t);
    di = b->color.b - a->color.b;
    rect->color.b = a->color.b + (u32)((f32)di * t);
    d = b->width - a->width;
    rect->width = a->width + d * t;
    d = b->height - a->height;
    rect->height = a->height + d * t;
}

/* The handle is a live task of this class. */
s32 EftRays_IsTask(EftOTask *task) {
    if (task == NULL) {
        return 0;
    }
    if ((u8)(task->state & 1)) {
        return 0;
    }
    return task->cls[0] == (void *)EftRays_Update;
}
