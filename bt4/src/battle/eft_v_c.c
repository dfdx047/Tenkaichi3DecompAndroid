#include "common.h"
#include "battle/eft_v_ext.h"

/*
 * Effect code 0x187C50..0x1895E8: the first half of the sprite chain module, effect pack part kind 15 (see
 * include/battle/eft_v.h); the module continues at 0x1895E8 in another file.
 *
 * Drawing only. Read from the simulation: BtlScene_IsEffectStopped. Random numbers: the VU0 register through
 * Rand_FloatRange (EftLink_Place: one per sprite per frame; EftLink_InitGrow: three), appearance only.
 */

/* Manager init: the node pool and the list of 6 chain tasks. */
void EftLinkMgr_Init(EftVTask *task) {
    gEftLink = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftLinkMgr));
    memset(gEftLink, 0, sizeof(EftLinkMgr));
    gEftLink->list = BtlTask_CreateChildList(task, 6, sizeof(EftLinkWork));
    Mtx_StoreIdentity(&gEftLink->identity);
}

/* Manager update: nothing. */
void EftLinkMgr_Update(EftVTask *task) {
}

/* Manager reset: nothing. */
void EftLinkMgr_Reset(EftVTask *task) {
}

/* Manager term. */
void EftLinkMgr_Term(EftVTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftLink);
    gEftLink = NULL;
}

/* Task init: copies the argument block, starts the grow and key animations, builds the sheet's UV table. */
void EftLink_Init(EftVTask *task, EftLinkArg *arg) {
    EftLinkWork *w = task->work;
    EftLinkDef *def = arg->def;
    s32 i;
    s32 j;
    s32 n;

    memset(w, 0, sizeof(EftLinkWork));
    w->arg = *arg;
    w->arg.tex = arg->tex;
    w->arg.def = arg->def;
    w->arg.def2 = arg->def2;
    EftLink_SelectTex(w, arg->tex, arg->frame, arg->unk3C);
    if (def->flags & 0x200) {
        EftLink_InitGrow(w);
        w->flags |= EFT_LINK_GROWING;
    } else {
        w->spacing = def->spacing[0];
        w->scale = 1.0f;
    }
    if (def->flags & 0x400) {
        EftLink_InitKeys(w);
        w->flags |= EFT_LINK_KEYED;
    } else {
        EftLink_SetKey(w, 2);
    }
    {
        f32 d[2] = { 0.0f, 0.0f };

        if (def->cols >= 2 || def->rows >= 2) {
            w->frames = def->cols * def->rows;
            d[0] = 1.0f / def->cols;
            d[1] = 1.0f / def->rows;
            n = 0;
            for (j = 0; j < def->rows; j++) {
                for (i = 0; i < def->cols; i++) {
                    w->uv[n][0] = d[0] * i;
                    w->uv[n][1] = d[1] * j;
                    w->uv[n][2] = d[0] * i + d[0];
                    w->uv[n][3] = d[1] * j + d[1];
                    n++;
                }
            }
            w->flags |= EFT_LINK_SHEET;
        }
    }
    if (def->flags & 2) {
        w->flags |= EFT_LINK_FLAG200;
    }
    w->flags |= EFT_LINK_ALIVE;
}

/* Task update: delay, fade out, grow and key animations, sprite placement and stepping, life, end of the task. */
void EftLink_Update(EftVTask *task) {
    EftLinkWork *w = task->work;
    EftLinkArg *arg = &w->arg;
    f32 fade = 1.0f;

    if (!BtlScene_IsEffectStopped(arg->objId, w->type)) {
        if (w->delay > 0.0f) {
            w->delay -= 1.0f;
        } else {
            if ((w->flags & EFT_LINK_FADING) && !(w->flags & EFT_LINK_STOPPING)) {
                f32 r = w->fade / w->fadeMax;

                if (r < 0.0f) {
                    fade = 0.0f;
                } else if (!(r > 1.0f)) {
                    fade = r;
                }
                w->fade -= 1.0f;
                if (w->fade < 0.0f) {
                    w->flags &= ~EFT_LINK_ALIVE;
                    w->flags |= EFT_LINK_KILLED;
                }
            }
            if (w->flags & EFT_LINK_GROWING) {
                EftLink_UpdateGrow(w);
            }
            if (w->flags & EFT_LINK_KEYED) {
                EftLink_UpdateKeys(w);
            }
            if (!(w->flags & EFT_LINK_STOPPED)) {
                EftLink_Place(w);
            }
            EftLink_StepNodes(w, fade);
            w->time += 1.0f;
            if (w->flags & EFT_LINK_KEYED) {
                EftLink_InitKeys(w);
                if (w->time >= w->keyDur) {
                    EftLink_SetKey(w, 2);
                    w->flags &= ~EFT_LINK_KEYED;
                }
            }
            if (arg->life * 30.0f <= w->time && arg->life > 0.0f) {
                if (w->fadeMax > 0.0f) {
                    w->flags |= 0x1000 | EFT_LINK_FADING;
                } else {
                    w->flags |= EFT_LINK_STOPPED;
                }
            }
            if (w->flags & EFT_LINK_STOPPING) {
                w->stopDelay -= 1.0f;
                if (w->stopDelay <= 0.0f) {
                    w->flags &= ~EFT_LINK_STOPPING;
                    if (!(w->flags & EFT_LINK_FADING)) {
                        w->flags |= EFT_LINK_STOPPED;
                    }
                }
            }
            if ((w->flags & EFT_LINK_STOPPED) && w->head == NULL) {
                w->flags &= ~EFT_LINK_ALIVE;
                w->flags |= EFT_LINK_DEAD;
            }
            if ((w->flags & EFT_LINK_KILLED) || (w->flags & EFT_LINK_DEAD)) {
                BtlTask_SetDead(task);
            }
        }
    }
    if (!(w->flags & EFT_LINK_KILLED)) {
        if (!(w->flags & EFT_LINK_DEAD)) {
            EftLink_BuildTex(w);
        }
    }
}

/* Task post-update: nothing. */
void EftLink_PostUpdate(EftVTask *task) {
}

/* The GS TEX0 value of the chain's texture. Integer arithmetic: the pointer form adds the other way round. */
#define EFT_LINK_TEX0(w) (*(u64 *)((u32)(w)->arg.tex + ((w)->arg.frame << 4)))

/* Task draw: draws every sprite of the chain with one of four routines chosen by the definition flags. */
void EftLink_Draw(EftVTask *task) {
    EftLinkWork *w = task->work;
    EftLinkDef *def = w->arg.def;
    Mtx44 m = { 0 };
    Vec4 pos = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec4 out = { 0.0f, 0.0f, 0.0f, 1.0f };
    EftLinkNode *n;
    f32 size;
    f32 rot;

    Mtx_Copy(EFTV_MTX(&m), &gEftLink->identity);
    Mtx_RotateX(EFTV_MTX(&m), EFTV_MTX(&m), w->pitch);
    Mtx_RotateY(EFTV_MTX(&m), EFTV_MTX(&m), w->yaw);
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    for (n = w->head; n != NULL; n = n->next) {
        if (n->flags & 0x40) {
            if (def->flags & 0x10) {
                Vec4_Copy(EFTV_VEC(&pos), &n->offset);
                if (!(def->flags & 0x2000)) {
                    Vec3_Scale(EFTV_VEC(&pos), EFTV_VEC(&pos), w->arg.size * w->scale);
                }
                Mtx_MulVec4(EFTV_VEC(&pos), EFTV_MTX(&m), EFTV_VEC(&pos));
                Vec3_Add(EFTV_VEC(&out), EFTV_VEC(&pos), &n->pos);
                size = n->size * w->arg.size * w->scale * n->unkFC * 0.5f;
                rot = EftMath_WrapAngle(n->rot + n->twist);
                if (def->flags & 4) {
                    EftLink_DrawBillboardClipped(EFTV_VEC(&out), &n->color, &n->unk40, size, size, n->uv0.x, n->uv0.y, n->uv1.z, n->uv1.w, rot,
                                  def->layer, (w->flags >> 9) & 1, EFT_LINK_TEX0(w), 2.0f);
                } else {
                    EftLink_DrawBillboard(EFTV_VEC(&out), &n->color, (s32)n->unk40.x << 4, (s32)n->unk40.y << 4, size * 16.0f,
                                  size * 16.0f, n->uv0.x, n->uv0.y, n->uv1.z, n->uv1.w, rot, def->layer,
                                  (w->flags >> 9) & 1, EFT_LINK_TEX0(w));
                }
            } else if (def->flags & 4) {
                EftLink_DrawQuadClipped(n, &n->uv0, &n->uv1, &n->color, def->layer, w->arg.frame, (w->flags >> 9) & 1,
                              w->arg.tex);
            } else {
                EftLink_DrawQuad(n, &n->uv0, &n->uv1, &n->color, def->layer, w->arg.frame, (w->flags >> 9) & 1,
                              w->arg.tex);
            }
        }
    }
    Vu0Cur_Pop();
}

/* Task reset: ends the task. */
void EftLink_Reset(EftVTask *task) {
    BtlTask_SetDead(task);
}

/* Task term: returns the chain's sprites to the pool. */
void EftLink_Term(EftVTask *task) {
    EftLinkWork *w = task->work;
    EftLinkNode *n;

    for (n = w->head; n != NULL; n = n->next) {
        n->flags = 0;
        EftLink_UnlinkNode(&w->head, &w->tail, n);
    }
    w->flags = 0;
}

/* Lays the chain out from pos to pos2: one sprite every `spacing`, each pushed along the chain by a random
   amount; sprites that do not exist yet are taken from the pool. Also stores the chain's pitch and yaw. */
void EftLink_Place(EftLinkWork *w) {
    EftVVec *dir = &w->dir;
    EftLinkArg *arg = &w->arg;
    EftVVecU p = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    EftVVecU off = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    EftLinkNode *n;
    f32 len;
    f32 count = 0.0f;
    f32 a;
    f32 fi;
    s32 i;

    Vec3_Sub(dir, &w->arg.pos2, &w->arg.pos);
    len = Vec3_Length(dir);
    if (len > count) {
        count = len / w->spacing;
        Vec3_Normalize(dir, dir);
        a = (w->dir.y > 1.0f) ? -1.0f : (w->dir.y < -1.0f) ? 1.0f : -w->dir.y;
        w->pitch = EftMath_WrapAngle(asinf(a));
        w->yaw = EftMath_WrapAngle(atan2f(w->dir.x, w->dir.z));
        n = w->head;
        for (i = 0; i < (s32)count; i++) {
            fi = i;
            Vec3_ScaleAdd(EFTV_VEC(&p), &w->dir, &arg->pos, len * (fi / count));
            Vec3_Scale(EFTV_VEC(&off), &w->dir, Rand_FloatRange(w->val[11].cur, w->val[12].cur));
            Vec3_Add(EFTV_VEC(&p), EFTV_VEC(&p), EFTV_VEC(&off));
            if (n == NULL) {
                n = EftLink_NewNode(w, EFTV_VEC(&p), fi);
                if (n == NULL) {
                    continue;
                }
            }
            if (!(n->flags & 1)) {
                EftLink_InitNode(n, w);
                Vec4_Copy(&n->pos, EFTV_VEC(&p));
                n->twist = EftMath_WrapAngle(w->val[8].cur * 6.2831853f * fi);
                n->flags |= 1;
            }
            n->flags |= 0x400;
            n = n->next;
        }
    }
}

/* Starts the grow animation: three random scales and the per-frame steps of scale and spacing between them. */
void EftLink_InitGrow(EftLinkWork *w) {
    EftLinkDef *def = w->arg.def;
    f32 s[3];
    f32 dur;

    dur = def->growTime;
    w->growTime = 0.0f;
    w->growDur = dur * 30.0f;
    s[0] = Rand_FloatRange(def->scale[0], def->scale[0] + def->scaleRange[0]);
    s[1] = Rand_FloatRange(def->scale[1], def->scale[1] + def->scaleRange[1]);
    s[2] = Rand_FloatRange(def->scale[2], def->scale[2] + def->scaleRange[2]);
    w->scaleStep[0] = (s[1] - s[0]) / (w->growDur * def->growSplit);
    w->scaleStep[1] = (s[2] - s[1]) / (w->growDur * (1.0f - def->growSplit));
    w->scale = s[0];
    w->spacingStep[0] = (def->spacing[1] - def->spacing[0]) / (w->growDur * def->growSplit);
    w->spacingStep[1] = (def->spacing[2] - def->spacing[1]) / (w->growDur * (1.0f - def->growSplit));
    w->spacing = def->spacing[0];
}

/* Steps the grow animation; clears its flag when the time is up. */
void EftLink_UpdateGrow(EftLinkWork *w) {
    EftLinkDef *def = w->arg.def;

    if (w->growTime / w->growDur < def->growSplit) {
        w->scale += w->scaleStep[0];
        w->spacing += w->spacingStep[0];
    } else {
        w->scale += w->scaleStep[1];
        w->spacing += w->spacingStep[1];
    }
    w->growTime += 1.0f;
    if (w->growDur <= w->growTime) {
        w->flags &= ~EFT_LINK_GROWING;
    }
}

/* At the start of each key interval (time 0, then the split time): stores the difference between the interval's
   two keys for every animated value. */
/* Matches only with every track a member of its own in EftLinkDef (`f32 rotX[3]`, ...; the two-value tracks
   `f32 pulse[3][2]`): the member offset then splits into a 16-byte multiple added to the index and a rest
   added to the base, and the bases def, def + 4, def + 8, def + 12 are shared between the statements. */
void EftLink_InitKeys(EftLinkWork *w) {
    EftLinkDef *def = w->arg.def;
    EftLinkDef2 *def2 = w->arg.def2;
    s32 k = 0;

    if (w->time <= 0.0f) {
        k = 1;
        w->keyDur = def->keyTime * 30.0f;
        w->keySplit = w->keyDur * def->keySplit;
    }
    if (w->keySplit <= w->time && !(w->flags & EFT_LINK_KEY2)) {
        k = 2;
        w->flags |= EFT_LINK_KEY2;
    }
    if (k != 0) {
        w->val[0].delta = def->rotX[k] - def->rotX[k - 1];
        w->val[1].delta = def->rotXRange[k] - def->rotXRange[k - 1];
        w->val[2].delta = def->rotZ[k] - def->rotZ[k - 1];
        w->val[3].delta = def->rotZRange[k] - def->rotZRange[k - 1];
        w->val[4].delta = def->dirAng[k] - def->dirAng[k - 1];
        w->val[5].delta = def->dirAngRange[k] - def->dirAngRange[k - 1];
        w->val[6].delta = def->spin[k] - def->spin[k - 1];
        w->val[7].delta = def->spinRange[k] - def->spinRange[k - 1];
        w->val[8].delta = def->twist[k] - def->twist[k - 1];
        w->val[9].delta = def->ofsX[k] - def->ofsX[k - 1];
        w->val[10].delta = def->ofsY[k] - def->ofsY[k - 1];
        w->val[11].delta = def->unk84[k] - def->unk84[k - 1];
        w->val[12].delta = def->unk90[k] - def->unk90[k - 1];
        w->val[13].delta = def->dist[k] - def->dist[k - 1];
        w->val[14].delta = def->distRange[k] - def->distRange[k - 1];
        w->val[15].delta = def->distBase[k] - def->distBase[k - 1];
        w->vecA[1][0] = def->size[k][0] - def->size[k - 1][0];
        w->vecA[1][1] = def->size[k][1] - def->size[k - 1][1];
        w->vecA[1][2] = def->size[k][2] - def->size[k - 1][2];
        w->vecB[1][0] = def->sizeRange[k][0] - def->sizeRange[k - 1][0];
        w->vecB[1][1] = def->sizeRange[k][1] - def->sizeRange[k - 1][1];
        w->vecB[1][2] = def->sizeRange[k][2] - def->sizeRange[k - 1][2];
        w->val2[0].delta = def->life[k] - def->life[k - 1];
        w->val2[1].delta = def->lifeRange[k] - def->lifeRange[k - 1];
        w->val2[2].delta = def->wait[k] - def->wait[k - 1];
        w->val2[3].delta = def->waitRange[k] - def->waitRange[k - 1];
        w->pair2[1].a = def->pulse[k][0] - def->pulse[k - 1][0];
        w->pair2[1].b = def->pulse[k][1] - def->pulse[k - 1][1];
        w->val3.delta = def->pulseTime[k] - def->pulseTime[k - 1];
        w->pair[0][1].a = def2->mulR[k][0] - def2->mulR[k - 1][0];
        w->pair[0][1].b = def2->mulR[k][1] - def2->mulR[k - 1][1];
        w->pair[1][1].a = def2->mulG[k][0] - def2->mulG[k - 1][0];
        w->pair[1][1].b = def2->mulG[k][1] - def2->mulG[k - 1][1];
        w->pair[2][1].a = def2->mulB[k][0] - def2->mulB[k - 1][0];
        w->pair[2][1].b = def2->mulB[k][1] - def2->mulB[k - 1][1];
        w->val4[0].delta = def2->mulTime[k] - def2->mulTime[k - 1];
        w->val4[1].delta = def2->fade0[k] - def2->fade0[k - 1];
        w->val4[2].delta = def2->fade1[k] - def2->fade1[k - 1];
        Vec4_Sub(&w->vec[0][1], &def2->col0[k], &def2->col0[k - 1]);
        Vec4_Sub(&w->vec[2][1], &def2->col1[k], &def2->col1[k - 1]);
        Vec4_Sub(&w->vec[1][1], &def2->col0Range[k], &def2->col0Range[k - 1]);
        Vec4_Sub(&w->vec[3][1], &def2->col1Range[k], &def2->col1Range[k - 1]);
    }
}

/* Every frame of the key animation: current value = first key of the interval + difference * progress. */
/* As EftPart10_UpdateKeys (eft_x.c): the temporary is reached through a pointer variable. */
void EftLink_UpdateKeys(EftLinkWork *w) {
    EftVVec tmp;
    s32 k;
    EftLinkDef *def = w->arg.def;
    EftLinkDef2 *def2 = w->arg.def2;
    f32 t;
    EftVVec *p = &tmp;

    memset(p, 0, sizeof(tmp));
    k = 0;
    if (!(w->flags & EFT_LINK_KEY2)) {
        t = w->time / w->keySplit;
    } else {
        k = 1;
        t = (w->time - w->keySplit) / (w->keyDur - w->keySplit);
    }
    w->val[0].cur = def->rotX[k] + w->val[0].delta * t;
    w->val[1].cur = def->rotXRange[k] + w->val[1].delta * t;
    w->val[2].cur = def->rotZ[k] + w->val[2].delta * t;
    w->val[3].cur = def->rotZRange[k] + w->val[3].delta * t;
    w->val[4].cur = def->dirAng[k] + w->val[4].delta * t;
    w->val[5].cur = def->dirAngRange[k] + w->val[5].delta * t;
    w->val[6].cur = def->spin[k] + w->val[6].delta * t;
    w->val[7].cur = def->spinRange[k] + w->val[7].delta * t;
    w->val[8].cur = def->twist[k] + w->val[8].delta * t;
    w->val[9].cur = def->ofsX[k] + w->val[9].delta * t;
    w->val[10].cur = def->ofsY[k] + w->val[10].delta * t;
    w->val[11].cur = def->unk84[k] + w->val[11].delta * t;
    w->val[12].cur = def->unk90[k] + w->val[12].delta * t;
    w->val[13].cur = def->dist[k] + w->val[13].delta * t;
    w->val[14].cur = def->distRange[k] + w->val[14].delta * t;
    w->val[15].cur = def->distBase[k] + w->val[15].delta * t;
    w->vecA[0][0] = def->size[k][0] + w->vecA[1][0] * t;
    w->vecA[0][1] = def->size[k][1] + w->vecA[1][1] * t;
    w->vecA[0][2] = def->size[k][2] + w->vecA[1][2] * t;
    w->vecB[0][0] = def->sizeRange[k][0] + w->vecB[1][0] * t;
    w->vecB[0][1] = def->sizeRange[k][1] + w->vecB[1][1] * t;
    w->vecB[0][2] = def->sizeRange[k][2] + w->vecB[1][2] * t;
    w->val2[0].cur = def->life[k] + w->val2[0].delta * t;
    w->val2[1].cur = def->lifeRange[k] + w->val2[1].delta * t;
    w->val2[2].cur = def->wait[k] + w->val2[2].delta * t;
    w->val2[3].cur = def->waitRange[k] + w->val2[3].delta * t;
    w->pair2[0].a = def->pulse[k][0] + w->pair2[1].a * t;
    w->pair2[0].b = def->pulse[k][1] + w->pair2[1].b * t;
    w->val3.cur = def->pulseTime[k] + w->val3.delta * t;
    w->pair[0][0].a = def2->mulR[k][0] + w->pair[0][1].a * t;
    w->pair[0][0].b = def2->mulR[k][1] + w->pair[0][1].b * t;
    w->pair[1][0].a = def2->mulG[k][0] + w->pair[1][1].a * t;
    w->pair[1][0].b = def2->mulG[k][1] + w->pair[1][1].b * t;
    w->pair[2][0].a = def2->mulB[k][0] + w->pair[2][1].a * t;
    w->pair[2][0].b = def2->mulB[k][1] + w->pair[2][1].b * t;
    w->val4[0].cur = def2->mulTime[k] + w->val4[0].delta * t;
    w->val4[1].cur = def2->fade0[k] + w->val4[1].delta * t;
    w->val4[2].cur = def2->fade1[k] + w->val4[2].delta * t;
    Vec4_Scale(p, &w->vec[0][1], t);
    Vec4_Add(&w->vec[0][0], &def2->col0[k], p);
    Vec4_Scale(p, &w->vec[2][1], t);
    Vec4_Add(&w->vec[2][0], &def2->col1[k], p);
    Vec4_Scale(p, &w->vec[1][1], t);
    Vec4_Add(&w->vec[1][0], &def2->col0Range[k], p);
    Vec4_Scale(p, &w->vec[3][1], t);
    Vec4_Add(&w->vec[3][0], &def2->col1Range[k], p);
}
