#include "common.h"
#include "battle/eft_x.h"

/*
 * Effect tasks, 0x190DA8..0x191D28: the first half of the quad emitter module of effect pack part kind 9
 * (manager class gEftQuadMgrClass 0x2C40A8, emitter class gEftQuadClass 0x2C40C0). It continues in eft_y.c
 * (EftQuad_StepAll 0x191D28 ...). See include/battle/eft_x.h; drawing only: no fighter, hit record or battle
 * object is written. The emitter reads the scene's "effects of this fighter are stopped / hidden" tests and,
 * in the draw callback only, whether the view being drawn shows the owner.
 * Random draws: EftQuad_InitQuad, per spawned quad: 28 Rand_FloatRange (VU0 generator; 12 when the life comes
 * out as 0) and up to five libc rand() coin flips (definition flags 0x100, 0x200, 0x400, 2; definition byte
 * 0x242 == 2). They reach only the quad's appearance.
 */

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftXcView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ Mtx44 world2screen;
} EftXcView;

#define V(p) ((Vec4 *)(p))

extern EftPart9Mgr *gEftQuadMgr;
extern EftXcView *gBtlCamView;

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 Rand_FloatRange(f32 a, f32 b);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);    /* rotate about Z */
extern void Vu0Cur_Push(void);                                 /* VU0: push the current matrix */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                             /* VU0: load a matrix */
extern void Vu0Cur_Pop(void);                                 /* VU0: pop */
extern void Vec4_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi); /* clamps each component */
extern f32 EftMath_WrapAngle(f32 angle);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void *BtlTask_CreateChildList(void *task, s32 count, s32 workSize);
extern void BtlTask_SetDead(EftXTask *task);                       /* kills a task */
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern s32 BtlScene_IsCharInView(s32 objId);

/* The rest of the module (0x191D28..). */
extern void EftQuad_StepAll(EftPart9 *em);                         /* steps the emitter's particles */
extern void EftQuad_DrawAllFacing(EftPart9 *em, s32 layer);              /* draws them (definition flag 0x20) */
extern void EftQuad_DrawAllSprite(EftPart9 *em, s32 layer);              /* draws them */
extern void EftQuad_Emit(EftPart9 *em);                         /* spawns one particle */
extern void EftQuad_ListRemove(EftPart9Ptcl **head, EftPart9Ptcl **tail, EftPart9Ptcl *p); /* unlinks a particle */
extern void EftQuad_CalcKeyDeltas(EftXTask *task);
extern void EftQuad_Animate(EftXTask *task);
extern void EftQuad_SetSheet(EftPart9 *em, s32 cols, s32 rows);     /* sets up the texture grid */
extern void EftQuad_SetTex(EftPart9 *em, EftXTexSet *tex, s32 a, s32 b); /* chooses the textures */
extern void EftQuad_LoadTex(EftPart9 *em, EftXTexSet *tex);        /* builds the blended TEX0 */
extern void EftQuad_SetLastKey(EftPart9 *em);

static inline f32 EftQuad_Clamp(f32 x, f32 lo, f32 hi) {
    if (x < lo) {
        return lo;
    }
    if (hi < x) {
        return hi;
    }
    return x;
}

/* Manager init callback: allocates the module's work (200 particles) and the list of 30 emitter tasks. */
void EftQuadMgr_Init(EftXTask *task) {
    gEftQuadMgr = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftPart9Mgr));
    memset(gEftQuadMgr, 0, sizeof(EftPart9Mgr));
    gEftQuadMgr->list = BtlTask_CreateChildList(task, 30, sizeof(EftPart9));
}

/* Manager update callback: nothing. */
void EftQuadMgr_Update(EftXTask *task) {
}

/* Manager reset callback: frees every particle slot. */
void EftQuadMgr_Reset(EftXTask *task) {
    memset(gEftQuadMgr, 0, sizeof(gEftQuadMgr->ptcl));
}

/* Manager term callback. */
void EftQuadMgr_Term(EftXTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftQuadMgr);
    gEftQuadMgr = NULL;
}

/* Emitter init callback: copies the creation argument and starts the emitter. */
void EftQuad_Init(EftXTask *task, EftPart9Arg *arg) {
    EftPart9 *em = task->work;
    EftPart9Def *def = arg->def;

    memset(em, 0, sizeof(EftPart9));
    em->def = arg->def;
    em->def2 = arg->def2;
    em->objId = arg->objId;
    em->pos = arg->pos;
    em->dir = arg->dir;
    Vec3_Normalize(V(&em->dir), V(&em->dir));
    em->size = arg->size;
    em->interval = def->interval;
    em->count = def->count;
    em->life = arg->life * 30.0f;
    if (def->texCols >= 2 || def->texRows >= 2) {
        EftQuad_SetSheet(em, def->texCols, def->texRows);
        em->flags |= EFT_PART9_TEXGRID;
    }
    EftQuad_SetTex(em, arg->tex, 0, 0);
    em->head = NULL;
    em->tail = NULL;
    em->flags |= EFT_PART9_ALIVE;
    if (def->flags & 0x10) {
        EftQuad_CalcKeyDeltas(task);
        em->flags |= EFT_PART9_KEYANIM;
    } else {
        EftQuad_SetLastKey(em);
        em->rot[0] = 0.0f;
        em->rot[1] = 0.0f;
        em->rot[2] = 0.0f;
    }
}

/* Emitter update callback. */
void EftQuad_Update(EftXTask *task) {
    EftPart9 *em = task->work;
    EftPart9Def *def = em->def;
    f32 t = 1.0f;
    s32 i;

    if (!BtlScene_IsEffectStopped(em->objId, em->kind)) {
        if (0.0f < em->delay) {
            em->delay -= t;
            return;
        }
        if ((em->flags & EFT_PART9_FADE) && !(em->flags & EFT_PART9_HOLD)) {
            f32 r = em->fade / em->fadeTime;

            r = EftQuad_Clamp(r, 0.0f, 1.0f);
            em->fade -= 1.0f;
            if (em->fade < 0.0f) {
                em->flags |= EFT_PART9_DEAD;
            }
        }
        if (em->flags & EFT_PART9_KEYANIM) {
            EftQuad_Animate(task);
        }
        if (!(em->flags & EFT_PART9_NOEMIT)) {
            if ((s32)em->age % (s32)em->interval == 0) {
                for (i = 0; i < (s32)em->count; i++) {
                    em->rot[0] += em->spin[0] * 3.14159265f;
                    em->rot[1] += em->spin[1] * 3.14159265f;
                    em->rot[2] += em->spin[2] * 3.14159265f;
                    em->rot[0] = EftMath_WrapAngle(em->rot[0]);
                    em->rot[1] = EftMath_WrapAngle(em->rot[1]);
                    em->rot[2] = EftMath_WrapAngle(em->rot[2]);
                    EftQuad_Emit(em);
                }
            }
        }
        EftQuad_StepAll(em);
        if (0.0f < em->life) {
            em->life -= 1.0f;
            if (em->life <= 0.0f) {
                em->life = 0.0f;
                if (0.0f < em->fadeTime) {
                    em->flags |= EFT_PART9_FADE;
                } else {
                    em->flags |= EFT_PART9_NOEMIT;
                }
            }
        }
        if (em->flags & EFT_PART9_HOLD) {
            em->hold -= 1.0f;
            if (em->hold < 0.0f) {
                em->flags &= ~EFT_PART9_HOLD;
                if (!(em->flags & EFT_PART9_FADE)) {
                    em->flags |= EFT_PART9_NOEMIT;
                }
            }
        }
        em->age += 1.0f;
        if (em->head == NULL && (em->flags & EFT_PART9_NOEMIT)) {
            em->flags |= EFT_PART9_DEAD;
        }
        if (em->flags & EFT_PART9_DEAD) {
            BtlTask_SetDead(task);
        }
        if (em->flags & EFT_PART9_KEYANIM) {
            EftQuad_CalcKeyDeltas(task);
            if (em->age >= em->keyTime) {
                EftQuad_SetLastKey(em);
                em->count = def->count;
                em->interval = def->interval;
                em->flags &= ~EFT_PART9_KEYANIM;
            }
        }
    }
    if (!(em->flags & EFT_PART9_KILL)) {
        if (!(em->flags & EFT_PART9_DEAD)) {
            EftQuad_LoadTex(em, em->tex);
        }
    }
}

/* Emitter post-update callback: nothing. */
void EftQuad_PostUpdate(EftXTask *task) {
}

/* Emitter draw callback. */
void EftQuad_Draw(EftXTask *task) {
    EftPart9 *em = task->work;
    EftPart9Def *def = em->def;

    if (em->tex == NULL) {
        return;
    }
    if (BtlScene_IsEffectHidden(em->objId, em->kind)) {
        return;
    }
    if ((em->flags & EFT_PART9_VIEWONLY) && !BtlScene_IsCharInView(em->objId)) {
        return;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    if (em->def->flags & 0x20) {
        EftQuad_DrawAllFacing(em, def->layer);
    } else {
        EftQuad_DrawAllSprite(em, def->layer);
    }
    Vu0Cur_Pop();
}

/* Emitter reset callback: the emitter does not survive a scene reset. */
void EftQuad_Reset(EftXTask *task) {
    BtlTask_SetDead(task);
}

/* Emitter term callback: frees its particles. */
void EftQuad_Term(EftXTask *task) {
    EftPart9 *em = task->work;
    EftPart9Ptcl *p;

    for (p = em->head; p != NULL; p = p->next) {
        p->flags = 0;
        EftQuad_ListRemove(&em->head, &em->tail, p);
    }
    em->flags = 0;
}

/* Fills a particle from the emitter's ranges, each value drawn from [value, value + range] with the VU0
   generator; coin flips use libc rand(). Returns 0 when the life comes out as 0. */
s32 EftQuad_InitQuad(EftPart9Ptcl *p, EftPart9 *em) {
    Mtx44 m;
    Vec4 up;
    f32 sz[4];
    Vec4 end;
    EftPart9Def *def = em->def;
    f32 x;
    f32 y;
    f32 a;

    x = Rand_FloatRange(em->offset[0][0], em->offset[0][0] + em->offset[0][1]);
    y = Rand_FloatRange(em->offset[1][0], em->offset[1][0] + em->offset[1][1]);
    Vec4_Set(&p->pos, x, y, Rand_FloatRange(em->offset[2][0], em->offset[2][0] + em->offset[2][1]), 1.0f);
    Vec4_Copy(&p->origin, V(&em->pos));
    Mtx_StoreIdentity(&m);
    Vec4_Set(&up, 0.0f, 0.0f, 1.0f, 1.0f);
    a = Rand_FloatRange(em->arc, em->arc + em->arcRange) * 3.14159265f;
    p->dir.x = sinf(a);
    p->dir.y = 0.0f;
    p->dir.z = cosf(a);
    p->dir.w = 1.0f;
    Mtx_RotateZ(&m, &m, EftMath_WrapAngle(Rand_FloatRange(0.0f, 1.0f) * 6.2831853f));
    Mtx_MulVec4(&p->dir, &m, &p->dir);
    Vec3_Sub(&p->vel, &up, &p->dir);
    Vec3_Normalize(&p->vel, &p->vel);
    Vec3_Scale(&p->vel, &p->vel, em->speed);
    p->rotVel[0] = Rand_FloatRange(em->rotVelMin[0], em->rotVelMin[0] + em->rotVelRange[0]) * 3.14159265f;
    p->rotVel[1] = Rand_FloatRange(em->rotVelMin[1], em->rotVelMin[1] + em->rotVelRange[1]) * 3.14159265f;
    p->rotVel[2] = Rand_FloatRange(em->rotVelMin[2], em->rotVelMin[2] + em->rotVelRange[2]) * 3.14159265f;
    p->rot.x = Rand_FloatRange(em->rotMin[0], em->rotMin[0] + em->rotRange[0]) * 6.2831853f;
    p->rot.y = Rand_FloatRange(em->rotMin[1], em->rotMin[1] + em->rotRange[1]) * 6.2831853f;
    p->rot.z = Rand_FloatRange(em->rotMin[2], em->rotMin[2] + em->rotRange[2]) * 6.2831853f;
    p->rot.w = 1.0f;
    p->rot.x += em->rot[0];
    p->rot.y += em->rot[1];
    p->rot.z += em->rot[2];
    if (def->flags & 0x100) {
        if (!(rand() & 1)) {
            p->rot.x = -p->rot.x;
        }
    }
    if (def->flags & 0x200) {
        if (!(rand() & 1)) {
            p->rot.y = -p->rot.y;
        }
    }
    if (def->flags & 0x400) {
        if (!(rand() & 1)) {
            p->rot.z = -p->rot.z;
        }
    }
    if (def->flags & 2) {
        if (!(rand() & 1)) {
            p->rotVel[2] = -p->rotVel[2];
        }
    }
    if (def->flags & 1) {
        p->rotVel[2] = -p->rotVel[2];
    }
    p->life = Rand_FloatRange(em->lifeMin, em->lifeMin + em->lifeRange) * 30.0f;
    p->age = 0.0f;
    if (p->life <= p->age) {
        return 0;
    }
    p->next = NULL;
    p->prev = NULL;
    if (em->texIdxA == em->texIdxB) {
        p->texSlot = em->texIdxA;
    } else {
        p->texSlot = em->texIdxA + em->texIdxB;
    }
    if ((def->flags & 4) && p->rotVel[2] < 0.0f) {
        p->uv[0] = 0.984375f;
        p->uv[1] = 0.015625f;
        p->uv[2] = 0.015625f;
        p->uv[3] = 0.015625f;
        p->uv[4] = 0.984375f;
        p->uv[5] = 0.984375f;
        p->uv[6] = 0.015625f;
        p->uv[7] = 0.984375f;
    } else {
        p->uv[0] = 0.015625f;
        p->uv[1] = 0.015625f;
        p->uv[2] = 0.015625f;
        p->uv[3] = 0.984375f;
        p->uv[4] = 0.984375f;
        p->uv[5] = 0.015625f;
        p->uv[6] = 0.984375f;
        p->uv[7] = 0.984375f;
    }
    p->texFrame = Rand_FloatRange(0.0f, em->texFrames - 1);
    if (def->flags & 8) {
        p->unk11C = em->unk348 * 30.0f;
        p->unk118 = 0.0f;
        if (0.0f < em->unk348) {
            p->unkF4 = em->unk344 - em->unk340;
            p->unk114 = em->unk340;
            p->flags |= 0x800;
        }
    }
    if (def->flags & 0x80) {
        p->stretchTime = em->stretchTime * 30.0f;
        p->stretchAge = 0.0f;
        if (p->stretchAge < p->stretchTime) {
            p->stretchStep[0] = em->stretch1[0] - em->stretch0[0];
            p->stretchStep[1] = em->stretch1[1] - em->stretch0[1];
            p->stretchStep[2] = em->stretch1[2] - em->stretch0[2];
            p->stretch[0] = em->stretch0[0];
            p->stretch[1] = em->stretch0[1];
            p->stretch[2] = em->stretch0[2];
            if (p->stretchStep[0] != 0.0f) {
                p->flags |= 0x10000;
            }
            if (p->stretchStep[1] != 0.0f) {
                p->flags |= 0x20000;
            }
            if (p->stretchStep[2] != 0.0f) {
                p->flags |= 0x40000;
            }
        }
    } else {
        p->stretch[0] = 1.0f;
        p->stretch[1] = 1.0f;
        p->stretch[2] = 1.0f;
    }
    sz[0] = Rand_FloatRange(em->size0[0], em->size0[0] + em->size0Range[0]);
    sz[1] = Rand_FloatRange(em->size0[1], em->size0[1] + em->size0Range[1]);
    sz[2] = Rand_FloatRange(em->size0[2], em->size0[2] + em->size0Range[2]);
    p->size = sz[0];
    p->sizeStep0 = (sz[1] - sz[0]) / (p->life * def->sizeMid);
    p->sizeStep1 = (sz[2] - sz[1]) / (p->life * (1.0f - def->sizeMid));
    p->unk138 = p->unk134 = Rand_FloatRange(def->unk224, def->unk224 + def->unk228);
    sz[0] = Rand_FloatRange(em->size1[0], em->size1[0] + em->size1Range[0]);
    sz[1] = Rand_FloatRange(em->size1[1], em->size1[1] + em->size1Range[1]);
    sz[2] = Rand_FloatRange(em->size1[2], em->size1[2] + em->size1Range[2]);
    p->unk108 = sz[0];
    p->unk10C = (sz[1] - sz[0]) / (p->life * def->sizeMid);
    p->unk110 = (sz[2] - sz[1]) / (p->life * (1.0f - def->sizeMid));
    p->unk104 = Rand_FloatRange(def->unk22C, def->unk22C + def->unk230);
    memset(&end, 0, sizeof(end));
    switch (def->unk242) {
    case 2:
        if (rand() & 1) {
            break;
        }
    case 1:
        p->flags |= 0x40;
        break;
    }
    p->unk15C = em->unk390;
    p->unk160 = em->unk394;
    p->unk14C = 0.0f;
    p->unk148 = p->life * (p->unk160 - p->unk15C);
    if (p->unk148 != p->life && p->unk14C < p->unk148) {
        p->flags |= 0x8000;
    }
    p->color0.x = Rand_FloatRange(em->colorMin[0], em->colorMin[0] + em->colorRange[0]);
    p->color0.y = Rand_FloatRange(em->colorMin[1], em->colorMin[1] + em->colorRange[1]);
    p->color0.z = Rand_FloatRange(em->colorMin[2], em->colorMin[2] + em->colorRange[2]);
    p->color0.w = 0.0f;
    end.x = Rand_FloatRange(em->endColorMin[0], em->endColorMin[0] + em->endColorRange[0]);
    end.y = Rand_FloatRange(em->endColorMin[1], em->endColorMin[1] + em->endColorRange[1]);
    end.z = Rand_FloatRange(em->endColorMin[2], em->endColorMin[2] + em->endColorRange[2]);
    end.w = 0.0f;
    Vec4_Clamp(&p->color0, &p->color0, end.w, 255.0f);
    Vec4_Clamp(&end, &end, 0.0f, 255.0f);
    Vec4_Copy(&p->color, &p->color0);
    Vec3_Sub(&p->colorStep, &end, &p->color0);
    p->color.w = Rand_FloatRange(em->colorMin[3], em->colorMin[3] + em->colorRange[3]);
    p->color.w = EftQuad_Clamp(p->color.w, 0.0f, 255.0f);
    p->unkEC = p->life * def->unk218;
    p->unkF0 = p->life * (1.0f - def->unk21C);
    return 1;
}
