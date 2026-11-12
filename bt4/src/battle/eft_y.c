#include "common.h"
#include "battle/eft_y.h"
#include "sys/math3d.h"
#include "sys/gfx_ot.h"

/*
 * Effect pack part module, 0x191D28..0x195038: the second half of the quad emitter (part kind 9). The helpers of
 * the line (part kind 16, 0x195038..0x195EE8) that were written with this file are now the head of eft_z.c.
 * See include/battle/eft_y.h. Nothing here touches a hit record, a fighter or a battle object, and nothing here
 * draws a random number.
 */

extern EftQuadPool *gEftQuadMgr;  /* the quad pool; NULL outside a battle */
extern void *gEftQuadClass[6];      /* task class of the quad emitter */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);
extern f32 EftMath_WrapAngle(f32 angle);
extern void Vec4_Set(EftYVec *v, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(EftYVec *dst, EftYVec *src);
extern void Vec4_Sub(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec4_Add(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec4_Scale(EftYVec *dst, EftYVec *src, f32 s);
extern void Vec3_Add(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec3_Sub(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec3_Scale(EftYVec *dst, EftYVec *src, f32 s);
extern void Vec3_Cross(EftYVec *dst, EftYVec *a, EftYVec *b);
extern void Vec3_Normalize(EftYVec *dst, EftYVec *src);
extern void Vu0Cur_LoadIdentity(void);                 /* VU0 current matrix = identity (inferred) */
extern void Vu0Cur_Push(void);                 /* VU0 matrix stack: push */
extern void Vu0Cur_Pop(void);                 /* pop */
extern void Vu0Cur_Translate(EftYVec *v);           /* current matrix: translate */
extern void Vu0Cur_RotateZ(f32 angle);            /* current matrix: rotate about Z */
extern void Vu0Cur_RotateX(f32 angle);            /* rotate about X */
extern void Vu0Cur_RotateY(f32 angle);            /* rotate about Y */
extern void Vu0Cur_ScaleDiagUniform(f32 scale);            /* scale */
extern void Vu0Cur_MulVec4(EftYVec *dst, EftYVec *src); /* transform a point by the current matrix */
extern s32 Vu0Cur_ProjectPoints(EftYScr *out, EftYVec *pos, s32 count); /* project; 0 when clipped */
extern s32 Vu0Cur_ProjectPointsStq(EftYScr *xyz, EftYVec *stq, EftYVec *pos, EftYVec *uv, s32 n); /* project with texture */
extern void ClipVtx_Set(EftYClipVtx *out, EftYVec *pos, EftYVec *st, EftYVec *color);
extern void Vec3_Copy(EftYVec *dst, EftYVec *src); /* copies x, y, z */
extern void Vec4_ToInt(EftYCol *dst, EftYVec *src); /* float vector to integer vector */
extern void Vec4_Clamp(EftYVec *dst, EftYVec *src, f32 lo, f32 hi); /* clamps each component */
extern void Vec3_ScaleAdd(EftYVec *dst, EftYVec *dir, EftYVec *base, f32 s); /* dst = base + dir * s */
extern void EftGfx_DrawPolyFixedZ(EftYClipVtx *verts, s32 layer, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                                  s32 z);
extern void EftGfx_DrawPolyScaledZ(EftYClipVtx *verts, s32 layer, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                                   f32 zScale);
extern u64 EftVram_AddImage(EftYTex *tex, s32 a, s32 b);
extern u64 EftVram_AddClut(EftYTex *tex);
extern EftYTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern s32 EftQuad_InitQuad(EftQuad *q, EftQuadWork *w); /* fills a new quad from the emitter's current values */

typedef struct EftYCamView {
    /* 0x000 */ u8 unk0[0x220];
    /* 0x220 */ EftYVec pos;
} EftYCamView;
extern EftYCamView *gBtlCamView;

#define EFTY_CLAMP(x, lo, hi) (((x) < (lo)) ? (lo) : (((hi) < (x)) ? (hi) : (x)))
#define EFTY_CLAMP01(x) EFTY_CLAMP(x, 0.0f, 1.0f)

/* Steps every quad of an emitter by one frame, rebuilds its corners and unlinks the ones whose life is over (or
   all of them when the emitter was killed). */
void EftQuad_StepAll(EftQuadWork *w) {
    EftYVec step;
    EftQuadDef *def = w->def;
    f32 pitch;
    f32 yaw;
    EftQuad *q;
    f32 fade = 0.0f;
    f32 alpha = 1.0f;

    memset(&step, 0, sizeof(EftYVec));
    if (w->dir.y < -1.0f) {
        w->dir.y = -1.0f;
    }
    if (1.0f < w->dir.y) {
        w->dir.y = 1.0f;
    }
    pitch = EftMath_WrapAngle(Mathf_Asin(-w->dir.y));
    yaw = EftMath_WrapAngle(atan2f(w->dir.x, w->dir.z));
    for (q = w->head; q != NULL; q = q->next) {
        f32 t = (q->age >= q->life) ? 1.0f : q->age / q->life;

        if (!(q->flags & EFT_QUAD_DEAD)) {
            f32 s = 0.0f;
            f32 osc = 1.0f;
            f32 speed;

            if (q->flags & EFT_QUAD_SIZE_OSC) {
                s = EFTY_CLAMP01(q->sizeOscTime / q->sizeOscPeriod);
                osc = q->sizeOscBase + q->sizeOscAmp * s;
                if (!(q->flags & EFT_QUAD_SIZE_DOWN)) {
                    q->sizeOscTime = q->sizeOscTime + 1.0f;
                } else {
                    q->sizeOscTime = q->sizeOscTime - 1.0f;
                }
                if (q->sizeOscTime < 0.0f || q->sizeOscPeriod <= q->sizeOscTime) {
                    q->sizeOscTime = EFTY_CLAMP(q->sizeOscTime, 0.0f, q->sizeOscPeriod);
                    q->flags ^= EFT_QUAD_SIZE_DOWN;
                }
            }
            if (t < def->speedSplit) {
                q->speedMul += q->speedStep[0];
                q->sizeMul += q->sizeStep[0];
            } else {
                q->speedMul += q->speedStep[1];
                q->sizeMul += q->sizeStep[1];
            }
            if (q->speedMul < 0.0f) {
                q->speedMul = 0.0f;
            }
            if (q->sizeMul < 0.0f) {
                q->sizeMul = 0.0f;
            }
            q->halfSize = q->size * q->sizeMul * osc;
            speed = q->speed * q->speedMul;
            q->rot.v[0] += q->rotVel.v[0];
            q->rot.v[1] += q->rotVel.v[1];
            q->rot.v[2] += q->rotVel.v[2];
            q->rot.v[0] = EftMath_WrapAngle(q->rot.v[0]);
            q->rot.v[1] = EftMath_WrapAngle(q->rot.v[1]);
            q->rot.v[2] = EftMath_WrapAngle(q->rot.v[2]);
            Vec3_Add(&q->vel, &q->vel, &q->accel);
            Vec3_Scale(&step, &q->vel, speed);
            Vec3_Add(&q->pos, &q->pos, &step);
            {
                f32 mul[3] = { 1.0f, 1.0f, 1.0f };
                f32 u = 0.0f;
                f32 r;

                if (def->flags & EFT_QUADDEF_COLOR_OSC) {
                    if (0.0f < q->colorOscPeriod) {
                        u = EFTY_CLAMP01(q->colorOscTime / q->colorOscPeriod);
                    } else {
                        u = 1.0f;
                    }
                    if (q->flags & EFT_QUAD_OSC_R) {
                        mul[0] = q->colorMul.v[0] + q->colorOscAmp.v[0] * u;
                    } else {
                        mul[0] = q->colorMul.v[0];
                    }
                    if (q->flags & EFT_QUAD_OSC_G) {
                        mul[1] = q->colorMul.v[1] + q->colorOscAmp.v[1] * u;
                    } else {
                        mul[1] = q->colorMul.v[1];
                    }
                    if (q->flags & EFT_QUAD_OSC_B) {
                        mul[2] = q->colorMul.v[2] + q->colorOscAmp.v[2] * u;
                    } else {
                        mul[2] = q->colorMul.v[2];
                    }
                    mul[0] = EFTY_CLAMP01(mul[0]);
                    mul[1] = EFTY_CLAMP01(mul[1]);
                    mul[2] = EFTY_CLAMP01(mul[2]);
                    if (!(q->flags & EFT_QUAD_OSC_DOWN)) {
                        q->colorOscTime = q->colorOscTime + 1.0f;
                        if (q->colorOscPeriod <= q->colorOscTime) {
                            q->colorOscTime = q->colorOscPeriod;
                            q->flags |= EFT_QUAD_OSC_DOWN;
                        }
                    } else {
                        q->colorOscTime = q->colorOscTime - 1.0f;
                        if (q->colorOscTime <= 0.0f) {
                            q->colorOscTime = 0.0f;
                            q->flags &= ~EFT_QUAD_OSC_DOWN;
                        }
                    }
                }
                r = 0.0f;
                if (!(q->flags & EFT_QUAD_STATIC_COLOR)) {
                    if (q->flags & EFT_QUAD_COLOR_RAMP) {
                        if (q->rampStart <= t && t < q->rampEnd) {
                            r = q->rampTime / q->rampLen;
                            q->rampTime += 1.0f;
                        } else if (q->rampEnd <= t) {
                            r = 1.0f;
                        }
                    } else {
                        r = t;
                    }
                    q->color.v[0] = (q->colorBase.v[0] + q->colorRamp.v[0] * r) * mul[0];
                    q->color.v[1] = (q->colorBase.v[1] + q->colorRamp.v[1] * r) * mul[1];
                    q->color.v[2] = (q->colorBase.v[2] + q->colorRamp.v[2] * r) * mul[2];
                } else if (def->flags & EFT_QUADDEF_COLOR_OSC) {
                    q->color.v[0] = q->colorBase.v[0] * mul[0];
                    q->color.v[1] = q->colorBase.v[1] * mul[1];
                    q->color.v[2] = q->colorBase.v[2] * mul[2];
                }
            }
            if (t < def->fadeInEnd && 0.0f < q->fadeIn) {
                fade = q->age / q->fadeIn;
                if (1.0f < fade) {
                    fade = 1.0f;
                }
            } else {
                fade = 1.0f;
            }
            if (def->fadeOutStart <= t && 0.0f < q->fadeOut) {
                fade = 1.0f - (q->age - (q->life - q->fadeOut)) / q->fadeOut;
                if (fade < 0.0f) {
                    fade = 0.0f;
                }
            }
            q->color.w = q->alphaBase * fade * alpha;
            Vec4_Clamp(&q->color, &q->color, 0.0f, 255.0f);
            if (w->flags & EFT_QUADEM_SHEET) {
                u8 frame = (u32)q->frame;

                if (frame >= w->frames) {
                    frame = 0;
                }
                if ((def->flags & EFT_QUADDEF_UV_FLIP) && q->rotVel.v[2] < 0.0f) {
                    q->uv[0].u = w->uv[frame][2];
                    q->uv[0].v = w->uv[frame][1];
                    q->uv[1].u = w->uv[frame][0];
                    q->uv[1].v = w->uv[frame][1];
                    q->uv[2].u = w->uv[frame][2];
                    q->uv[2].v = w->uv[frame][3];
                    q->uv[3].u = w->uv[frame][0];
                    q->uv[3].v = w->uv[frame][3];
                } else {
                    q->uv[0].u = w->uv[frame][0];
                    q->uv[0].v = w->uv[frame][1];
                    q->uv[1].u = w->uv[frame][2];
                    q->uv[1].v = w->uv[frame][1];
                    q->uv[2].u = w->uv[frame][0];
                    q->uv[2].v = w->uv[frame][3];
                    q->uv[3].u = w->uv[frame][2];
                    q->uv[3].v = w->uv[frame][3];
                }
                if ((s32)q->age % def->frameStep == 0) {
                    q->frame += 1.0f;
                    if ((u8)(u32)q->frame >= w->frames) {
                        q->frame = 0.0f;
                    }
                }
            }
            q->age += 1.0f;
            if (q->life <= q->age) {
                q->flags |= EFT_QUAD_DEAD;
            }
        }
        if (q->color.w <= 0.0f || q->halfSize <= 0.0f) {
            q->flags &= ~EFT_QUAD_VISIBLE;
        } else {
            Vu0Cur_Push();
            Vu0Cur_LoadIdentity();
            Vu0Cur_ScaleDiagUniform(w->size);
            if (w->flags & EFT_QUADEM_OWN_ORIGIN) {
                EftQuad_BuildCorners(q, w, q->origin, pitch, yaw);
            } else {
                EftQuad_BuildCorners(q, w, w->pos, pitch, yaw);
            }
            Vu0Cur_Pop();
            q->flags |= EFT_QUAD_VISIBLE;
        }
        if ((q->flags & EFT_QUAD_DEAD) || (w->flags & EFT_QUADEM_KILL)) {
            q->flags = 0;
            EftQuad_ListRemove(&w->head, &w->tail, q);
        }
    }
}

/* Builds the four corners of a quad: a square of half size `halfSize` spun by the quad's own angles, moved to its
   position (times the emitter's size), turned to the emitter's direction and moved to `pos`, all through the
   VU0 matrix the caller prepared. */
void EftQuad_BuildCorners(EftQuad *q, EftQuadWork *w, EftYVec pos, f32 pitch, f32 yaw) {
    EftYVec p;
    EftQuadDef *def = w->def;
    f32 h;
    s32 i;

    memset(&p, 0, sizeof(EftYVec));
    Vu0Cur_RotateZ(q->rot.z);
    Vu0Cur_RotateX(q->rot.x);
    Vu0Cur_RotateY(q->rot.y);
    Vec3_Scale(&p, &q->pos, w->size);
    Vu0Cur_Translate(&p);
    Vu0Cur_RotateX(pitch);
    Vu0Cur_RotateY(yaw);
    Vu0Cur_Translate(&pos);
    if (!(def->flags & EFT_QUADDEF_CROSS)) {
        h = q->halfSize;
    } else {
        h = q->halfSize * 0.5f;
    }
    Vec4_Set(&q->corner[0], -h, -h, 0.0f, 1.0f);
    Vec4_Set(&q->corner[1], h, -h, 0.0f, 1.0f);
    Vec4_Set(&q->corner[2], -h, h, 0.0f, 1.0f);
    Vec4_Set(&q->corner[3], h, h, 0.0f, 1.0f);
    for (i = 0; i < 4; i++) {
        Vu0Cur_MulVec4(&q->corner[i], &q->corner[i]);
    }
}

/* Draws the visible quads of an emitter through the clipping polygon drawer. With def flag 0x40 each quad is drawn
   four times, moved half a diagonal towards each corner, with the texture mirrored so the four meet in the middle. */
void EftQuad_DrawAllFacing(EftQuadWork *w, s32 layer) {
    EftYVec c[4];
    EftYVec d;
    EftYVec uv0;
    EftYVec uv1;
    u8 tbl[4][4] = { { 0, 1, 2, 3 }, { 1, 0, 3, 2 }, { 2, 3, 0, 1 }, { 3, 2, 1, 0 } };
    EftQuad *q;
    s32 i;
    s32 j;

    if (!(w->def->flags & EFT_QUADDEF_CROSS)) {
        for (q = w->head; q != NULL; q = q->next) {
            if (q->flags & EFT_QUAD_VISIBLE) {
                Vec4_Set(&uv0, q->uv[0].u, q->uv[0].v, q->uv[1].u, q->uv[1].v);
                Vec4_Set(&uv1, q->uv[2].u, q->uv[2].v, q->uv[3].u, q->uv[3].v);
                EftQuad_DrawFacing(q->corner, uv0, uv1, q->color, layer, q->tex, 0, (w->flags & EFT_QUADEM_VIEW_ONLY) != 0, w->tex);
            }
        }
    } else {
        for (q = w->head; q != NULL; q = q->next) {
            if (q->flags & EFT_QUAD_VISIBLE) {
                for (i = 0; i < 4; i++) {
                    Vec3_Sub(&d, &q->corner[i], &q->corner[3 - i]);
                    for (j = 0; j < 4; j++) {
                        Vec3_ScaleAdd(&c[j], &d, &q->corner[j], 0.5f);
                    }
                    Vec4_Set(&uv0, q->uv[tbl[i][0]].c[0], q->uv[tbl[i][0]].c[1], q->uv[tbl[i][1]].c[0], q->uv[tbl[i][1]].c[1]);
                    Vec4_Set(&uv1, q->uv[tbl[i][2]].c[0], q->uv[tbl[i][2]].c[1], q->uv[tbl[i][3]].c[0], q->uv[tbl[i][3]].c[1]);
                    EftQuad_DrawFacing(c, uv0, uv1, q->color, layer, q->tex, 0, (w->flags & EFT_QUADEM_VIEW_ONLY) != 0, w->tex);
                }
            }
        }
    }
}

/* The same walk, drawing each quad as one order table strip. */
void EftQuad_DrawAllSprite(EftQuadWork *w, s32 layer) {
    EftYVec c[4];
    EftYVec d;
    EftYVec uv0;
    EftYVec uv1;
    u8 tbl[4][4] = { { 0, 1, 2, 3 }, { 1, 0, 3, 2 }, { 2, 3, 0, 1 }, { 3, 2, 1, 0 } };
    EftQuad *q;
    s32 i;
    s32 j;

    if (!(w->def->flags & EFT_QUADDEF_CROSS)) {
        for (q = w->head; q != NULL; q = q->next) {
            if (q->flags & EFT_QUAD_VISIBLE) {
                Vec4_Set(&uv0, q->uv[0].u, q->uv[0].v, q->uv[1].u, q->uv[1].v);
                Vec4_Set(&uv1, q->uv[2].u, q->uv[2].v, q->uv[3].u, q->uv[3].v);
                EftQuad_DrawSprite(q->corner, uv0, uv1, q->color, layer, q->tex, (w->flags & EFT_QUADEM_VIEW_ONLY) != 0, w->tex);
            }
        }
    } else {
        for (q = w->head; q != NULL; q = q->next) {
            if (q->flags & EFT_QUAD_VISIBLE) {
                for (i = 0; i < 4; i++) {
                    Vec3_Sub(&d, &q->corner[i], &q->corner[3 - i]);
                    for (j = 0; j < 4; j++) {
                        Vec3_ScaleAdd(&c[j], &d, &q->corner[j], 0.5f);
                    }
                    Vec4_Set(&uv0, q->uv[tbl[i][0]].c[0], q->uv[tbl[i][0]].c[1], q->uv[tbl[i][1]].c[0], q->uv[tbl[i][1]].c[1]);
                    Vec4_Set(&uv1, q->uv[tbl[i][2]].c[0], q->uv[tbl[i][2]].c[1], q->uv[tbl[i][3]].c[0], q->uv[tbl[i][3]].c[1]);
                    EftQuad_DrawSprite(c, uv0, uv1, q->color, layer, q->tex, (w->flags & EFT_QUADEM_VIEW_ONLY) != 0, w->tex);
                }
            }
        }
    }
}

/* Emits one quad: finds a free quad of the pool, starting after the last one handed out, lets the initialiser
   fill it and links it at the emitter's tail. Returns 0 when none was free (or the initialiser refused them all). */
s32 EftQuad_Emit(EftQuadWork *w) {
    u8 i;
    EftQuad *q;

    if (gEftQuadMgr->next >= EFT_QUAD_MAX) {
        gEftQuadMgr->next = 0;
    }
    i = gEftQuadMgr->next;
    do {
        q = &gEftQuadMgr->quad[i];
        i++;
        if (i >= EFT_QUAD_MAX) {
            i = 0;
        }
        if (q->flags == 0 && EftQuad_InitQuad(q, w)) {
            q->flags |= EFT_QUAD_USED;
            EftQuad_ListAppend(&w->head, &w->tail, q);
            gEftQuadMgr->next = i;
            return 1;
        }
    } while (i != gEftQuadMgr->next);
    return 0;
}

/* Links a quad at the tail of an emitter's list. */
void EftQuad_ListAppend(EftQuad **head, EftQuad **tail, EftQuad *q) {
    if (*head == NULL) {
        *head = q;
        *tail = q;
    } else {
        q->prev = *tail;
        (*tail)->next = q;
        *tail = q;
    }
}

/* Unlinks a quad. Its own links are left alone, so a walk can still step to the next one. */
void EftQuad_ListRemove(EftQuad **head, EftQuad **tail, EftQuad *p) {
    EftQuad *prev;
    EftQuad *a; /* one variable for the list head and later for a neighbour: needed for the registers */
    EftQuad *b;

    a = *head;
    if (a == NULL) {
        return;
    }
    prev = p->prev;
    if (prev == NULL) {
        p = p->next;
        if (p == NULL) {
            *head = NULL;
            *tail = NULL;
        } else {
            *head = p;
            p->prev = NULL;
        }
    } else {
        a = p->next;
        if (a == NULL) {
            *tail = prev;
            prev->next = NULL;
        } else {
            b = a;
            a = prev;
            b->prev = a;
            a->next = b;
        }
    }
}


/* Key animation of the emitter's parameters. At age 0 it sets the two leg lengths and takes the differences key 1
   - key 0; when the age passes the first leg it takes key 2 - key 1 (once). */
void EftQuad_CalcKeyDeltas(EftYTask *task) {
    EftQuadWork *w = task->work;
    EftQuadDef *def = w->def;
    EftQuadDef2 *def2 = w->def2;
    u8 k = 0;

    if (w->age <= 0.0f) {
        k = 1;
        w->animEnd = def->time * 30.0f;
        w->animSplit = w->animEnd * def->split;
    }
    if (w->animSplit <= w->age && !(w->flags & EFT_QUADEM_LEG2)) {
        w->flags |= EFT_QUADEM_LEG2;
        k = 2;
    }
    if ((u8)(k - 1) < 0x7F) { /* k > 0 as a signed char */
        w->delta.a.v[0] = def->a[k].v[0] - def->a[k - 1].v[0];
        w->delta.a.v[1] = def->a[k].v[1] - def->a[k - 1].v[1];
        w->delta.a.v[2] = def->a[k].v[2] - def->a[k - 1].v[2];
        w->delta.b.v[0] = def->b[k].v[0] - def->b[k - 1].v[0];
        w->delta.b.v[1] = def->b[k].v[1] - def->b[k - 1].v[1];
        w->delta.b.v[2] = def->b[k].v[2] - def->b[k - 1].v[2];
        w->delta.c.v[0] = def->c[k].v[0] - def->c[k - 1].v[0];
        w->delta.c.v[1] = def->c[k].v[1] - def->c[k - 1].v[1];
        w->delta.c.v[2] = def->c[k].v[2] - def->c[k - 1].v[2];
        w->delta.d.v[0] = def->d[k].v[0] - def->d[k - 1].v[0];
        w->delta.d.v[1] = def->d[k].v[1] - def->d[k - 1].v[1];
        w->delta.d.v[2] = def->d[k].v[2] - def->d[k - 1].v[2];
        w->delta.m = def->m[k] - def->m[k - 1];
        w->delta.n = def->n[k] - def->n[k - 1];
        w->delta.o = def->o[k] - def->o[k - 1];
        w->delta.q = def->q[k] - def->q[k - 1];
        w->delta.s = def->s[k] - def->s[k - 1];
        w->delta.p = def->p[k] - def->p[k - 1];
        w->delta.r = def->r[k] - def->r[k - 1];
        w->delta.t = def->t[k] - def->t[k - 1];
        w->delta.u = def->u[k] - def->u[k - 1];
        w->delta.v = def->v[k] - def->v[k - 1];
        w->delta.w = def->w[k] - def->w[k - 1];
        w->delta.interval = def->interval[k] - def->interval[k - 1];
        w->delta.count = def->count[k] - def->count[k - 1];
        w->delta.e.v[0] = def->e[k].v[0] - def->e[k - 1].v[0];
        w->delta.e.v[1] = def->e[k].v[1] - def->e[k - 1].v[1];
        w->delta.e.v[2] = def->e[k].v[2] - def->e[k - 1].v[2];
        w->delta.f.v[0] = def->f[k].v[0] - def->f[k - 1].v[0];
        w->delta.f.v[1] = def->f[k].v[1] - def->f[k - 1].v[1];
        w->delta.f.v[2] = def->f[k].v[2] - def->f[k - 1].v[2];
        w->delta.g.v[0] = def->g[k].v[0] - def->g[k - 1].v[0];
        w->delta.g.v[1] = def->g[k].v[1] - def->g[k - 1].v[1];
        w->delta.g.v[2] = def->g[k].v[2] - def->g[k - 1].v[2];
        w->delta.i.v[0] = def->i[k].v[0] - def->i[k - 1].v[0];
        w->delta.i.v[1] = def->i[k].v[1] - def->i[k - 1].v[1];
        w->delta.i.v[2] = def->i[k].v[2] - def->i[k - 1].v[2];
        w->delta.j.v[0] = def->j[k].v[0] - def->j[k - 1].v[0];
        w->delta.j.v[1] = def->j[k].v[1] - def->j[k - 1].v[1];
        w->delta.j.v[2] = def->j[k].v[2] - def->j[k - 1].v[2];
        w->delta.k.v[0] = def->k[k].v[0] - def->k[k - 1].v[0];
        w->delta.k.v[1] = def->k[k].v[1] - def->k[k - 1].v[1];
        w->delta.l = def->l[k] - def->l[k - 1];
        Vec4_Sub((EftYVec *)&w->delta.v0, &def2->v0[k], &def2->v0[k - 1]);
        Vec4_Sub((EftYVec *)&w->delta.v1, &def2->v1[k], &def2->v1[k - 1]);
        Vec4_Sub((EftYVec *)&w->delta.v2, &def2->v2[k], &def2->v2[k - 1]);
        Vec4_Sub((EftYVec *)&w->delta.v3, &def2->v3[k], &def2->v3[k - 1]);
        w->delta.c0 = def2->c0[k] - def2->c0[k - 1];
        w->delta.c1 = def2->c1[k] - def2->c1[k - 1];
        w->delta.p0a = def2->p0[k].v[0] - def2->p0[k - 1].v[0];
        w->delta.p1a = def2->p1[k].v[0] - def2->p1[k - 1].v[0];
        w->delta.p2a = def2->p2[k].v[0] - def2->p2[k - 1].v[0];
        w->delta.p0b = def2->p0[k].v[1] - def2->p0[k - 1].v[1];
        w->delta.p1b = def2->p1[k].v[1] - def2->p1[k - 1].v[1];
        w->delta.p2b = def2->p2[k].v[1] - def2->p2[k - 1].v[1];
        w->delta.e2 = def2->e[k] - def2->e[k - 1];
    }
}

/* Current value of every animated parameter: the leg's first key plus the leg's difference times the progress.
   Original bug: all three components of `d` are advanced with the difference of d[0]. */
void EftQuad_Animate(EftYTask *task) {
    EftYVec tmp;
    s32 k;
    EftQuadWork *w = task->work;
    EftQuadDef *def = w->def;
    EftQuadDef2 *def2;
    f32 t;

    k = 0;
    def2 = w->def2;
    memset(&tmp, 0, sizeof(EftYVec));
    if (!(w->flags & EFT_QUADEM_LEG2)) {
        t = w->age / w->animSplit;
    } else {
        k = 1;
        t = (w->age - w->animSplit) / (w->animEnd - w->animSplit);
    }
    w->cur.a.v[0] = def->a[k].v[0] + w->delta.a.v[0] * t;
    w->cur.a.v[1] = def->a[k].v[1] + w->delta.a.v[1] * t;
    w->cur.a.v[2] = def->a[k].v[2] + w->delta.a.v[2] * t;
    w->cur.b.v[0] = def->b[k].v[0] + w->delta.b.v[0] * t;
    w->cur.b.v[1] = def->b[k].v[1] + w->delta.b.v[1] * t;
    w->cur.b.v[2] = def->b[k].v[2] + w->delta.b.v[2] * t;
    w->cur.c.v[0] = def->c[k].v[0] + w->delta.c.v[0] * t;
    w->cur.c.v[1] = def->c[k].v[1] + w->delta.c.v[1] * t;
    w->cur.c.v[2] = def->c[k].v[2] + w->delta.c.v[2] * t;
    w->cur.d.v[0] = def->d[k].v[0] + w->delta.d.v[0] * t;
    w->cur.d.v[1] = def->d[k].v[1] + w->delta.d.v[0] * t;
    w->cur.d.v[2] = def->d[k].v[2] + w->delta.d.v[0] * t;
    w->cur.o = def->o[k] + w->delta.o * t;
    w->cur.q = def->q[k] + w->delta.q * t;
    w->cur.s = def->s[k] + w->delta.s * t;
    w->cur.p = def->p[k] + w->delta.p * t;
    w->cur.r = def->r[k] + w->delta.r * t;
    w->cur.t = def->t[k] + w->delta.t * t;
    w->cur.e.v[0] = def->e[k].v[0] + w->delta.e.v[0] * t;
    w->cur.e.v[1] = def->e[k].v[1] + w->delta.e.v[1] * t;
    w->cur.e.v[2] = def->e[k].v[2] + w->delta.e.v[2] * t;
    w->cur.f.v[0] = def->f[k].v[0] + w->delta.f.v[0] * t;
    w->cur.f.v[1] = def->f[k].v[1] + w->delta.f.v[1] * t;
    w->cur.f.v[2] = def->f[k].v[2] + w->delta.f.v[2] * t;
    w->cur.g.v[0] = def->g[k].v[0] + w->delta.g.v[0] * t;
    w->cur.g.v[1] = def->g[k].v[1] + w->delta.g.v[1] * t;
    w->cur.g.v[2] = def->g[k].v[2] + w->delta.g.v[2] * t;
    w->cur.i.v[0] = def->i[k].v[0] + w->delta.i.v[0] * t;
    w->cur.i.v[1] = def->i[k].v[1] + w->delta.i.v[1] * t;
    w->cur.i.v[2] = def->i[k].v[2] + w->delta.i.v[2] * t;
    w->cur.j.v[0] = def->j[k].v[0] + w->delta.j.v[0] * t;
    w->cur.j.v[1] = def->j[k].v[1] + w->delta.j.v[1] * t;
    w->cur.j.v[2] = def->j[k].v[2] + w->delta.j.v[2] * t;
    w->cur.k.v[0] = def->k[k].v[0] + w->delta.k.v[0] * t;
    w->cur.k.v[1] = def->k[k].v[1] + w->delta.k.v[1] * t;
    w->cur.l = def->l[k] + w->delta.l * t;
    w->cur.m = def->m[k] + w->delta.m * t;
    w->cur.n = def->n[k] + w->delta.n * t;
    w->cur.u = def->u[k] + w->delta.u * t;
    w->cur.v = def->v[k] + w->delta.v * t;
    w->cur.count = def->count[k] + w->delta.count * t;
    w->cur.interval = def->interval[k] + w->delta.interval * t;
    w->cur.w = def->w[k] + w->delta.w * t;
    w->cur.c0 = def2->c0[k] + w->delta.c0 * t;
    w->cur.c1 = def2->c1[k] + w->delta.c1 * t;
    w->cur.p0a = def2->p0[k].v[0] + w->delta.p0a * t;
    w->cur.p1a = def2->p1[k].v[0] + w->delta.p1a * t;
    w->cur.p2a = def2->p2[k].v[0] + w->delta.p2a * t;
    w->cur.p0b = def2->p0[k].v[1] + w->delta.p0b * t;
    w->cur.p1b = def2->p1[k].v[1] + w->delta.p1b * t;
    w->cur.p2b = def2->p2[k].v[1] + w->delta.p2b * t;
    w->cur.e2 = def2->e[k] + w->delta.e2 * t;
    Vec4_Scale(&tmp, (EftYVec *)&w->delta.v0, t);
    Vec4_Add((EftYVec *)&w->cur.v0, &def2->v0[k], &tmp);
    Vec4_Scale(&tmp, (EftYVec *)&w->delta.v1, t);
    Vec4_Add((EftYVec *)&w->cur.v1, &def2->v1[k], &tmp);
    Vec4_Scale(&tmp, (EftYVec *)&w->delta.v2, t);
    Vec4_Add((EftYVec *)&w->cur.v2, &def2->v2[k], &tmp);
    Vec4_Scale(&tmp, (EftYVec *)&w->delta.v3, t);
    Vec4_Add((EftYVec *)&w->cur.v3, &def2->v3[k], &tmp);
}

/* Queues one quad as a textured strip: projects the four corners with their texture coordinates, and links a
   0x90-byte packet into the order table slot of the average depth. Dropped when the projection rejects it.
   `flip` (the emitter's "view only" flag) forces the depth written to the vertices to the nearest value. */
void EftQuad_DrawSprite(EftYVec *corner, EftYVec uv0, EftYVec uv1, EftYVec color, s32 layer, s32 texIdx, s32 flip,
                        EftYTex8 *tex) {
    EftYVec st[4];
    EftYVec stq[4];
    EftYScr scr[4];
    EftYCol col;
    EftYStripPkt *p;
    OtEntry *e;
    s32 z;
    s32 ctx;
    s32 abe = 1;
    s32 l;

    Vec4_Set(&st[0], uv0.x, uv0.y, 1.0f, 1.0f);
    Vec4_Set(&st[1], uv0.z, uv0.w, 1.0f, 1.0f);
    Vec4_Set(&st[2], uv1.x, uv1.y, 1.0f, 1.0f);
    Vec4_Set(&st[3], uv1.z, uv1.w, 1.0f, 1.0f);
    if (Vu0Cur_ProjectPointsStq(scr, stq, corner, st, 4)) {
        p = (EftYStripPkt *)gOtCur;
        gOtCur = (u32 *)(p + 1);
        if (p != NULL) {
            ctx = layer >= 2;
            p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x1C;
            p->dmaTag = 0x20000008;
            p->vif0 = 0x10000000;
            p->vif1 = 0x50000008;
            p->gifTag = 0xE400000000008001;
            p->regs = 0x42142142142160 + (ctx << 4);
            p->next = NULL;
            z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
            if (flip) {
                scr[3].z = scr[2].z = scr[1].z = scr[0].z = 0xFFFFFF;
            }
            Vec4_ToInt(&col, &color);
            p->v[0].rgbaq.r = col.r;
            p->v[0].rgbaq.g = col.g;
            p->v[0].rgbaq.b = col.b;
            p->v[0].rgbaq.a = col.a;
            p->v[0].rgbaq.q = stq[0].z;
            p->v[1].rgbaq.r = col.r;
            p->v[1].rgbaq.g = col.g;
            p->v[1].rgbaq.b = col.b;
            p->v[1].rgbaq.a = col.a;
            p->v[1].rgbaq.q = stq[1].z;
            p->v[2].rgbaq.r = col.r;
            p->v[2].rgbaq.g = col.g;
            p->v[2].rgbaq.b = col.b;
            p->v[2].rgbaq.a = col.a;
            p->v[2].rgbaq.q = stq[2].z;
            p->v[3].rgbaq.r = col.r;
            p->v[3].rgbaq.g = col.g;
            p->v[3].rgbaq.b = col.b;
            p->v[3].rgbaq.a = col.a;
            p->v[3].rgbaq.q = stq[3].z;
            p->v[0].st.s = stq[0].x;
            p->v[0].st.t = stq[0].y;
            p->v[1].st.s = stq[1].x;
            p->v[1].st.t = stq[1].y;
            p->v[2].st.s = stq[2].x;
            p->v[2].st.t = stq[2].y;
            p->v[3].st.s = stq[3].x;
            p->v[3].st.t = stq[3].y;
            p->v[0].xyz.x = scr[0].x;
            p->v[0].xyz.y = scr[0].y;
            p->v[0].xyz.z = scr[0].z;
            p->v[0].xyz.f = 0xFF;
            p->v[1].xyz.x = scr[1].x;
            p->v[1].xyz.y = scr[1].y;
            p->v[1].xyz.z = scr[1].z;
            p->v[1].xyz.f = 0xFF;
            p->v[2].xyz.x = scr[2].x;
            p->v[2].xyz.y = scr[2].y;
            p->v[2].xyz.z = scr[2].z;
            p->v[2].xyz.f = 0xFF;
            p->v[3].xyz.x = scr[3].x;
            p->v[3].xyz.y = scr[3].y;
            p->v[3].xyz.z = scr[3].z;
            p->v[3].xyz.f = 0xFF;
            p->tex0 = tex->e[texIdx].tex0;
            l = layer;
            if (l >= 2) {
                l -= 2;
            }
            if (z < 0) {
                e = &gOtZ[0].layer[l];
            } else if (z >= 0x1000) {
                e = &gOtZ[0xFFF].layer[l];
            } else {
                e = &gOtZ[z].layer[l];
            }
            e->tail->next = (OtPrim *)p;
            e->tail = (OtPrim *)p;
        }
    }
}


/* Draws one quad as two triangles (corners 0-1-2 and 1-2-3) through the clipping polygon drawer. */
void EftQuad_DrawFacing(EftYVec *corner, EftYVec uv0, EftYVec uv1, EftYVec color, s32 layer, s32 texIdx, s32 arg6,
                        s32 flip, EftYTex8 *tex) {
    EftYVec c[4];
    EftYVec st[4];
    EftYClipVtx v[9];

    Vec4_Set(&st[0], uv0.x, uv0.y, 1.0f, 1.0f);
    Vec4_Set(&st[1], uv0.z, uv0.w, 1.0f, 1.0f);
    Vec4_Set(&st[2], uv1.x, uv1.y, 1.0f, 1.0f);
    Vec4_Set(&st[3], uv1.z, uv1.w, 1.0f, 1.0f);
    Vec4_Copy(&c[0], &corner[0]);
    Vec4_Copy(&c[1], &corner[1]);
    Vec4_Copy(&c[2], &corner[2]);
    Vec4_Copy(&c[3], &corner[3]);
    ClipVtx_Set(&v[0], &c[0], &st[0], &color);
    ClipVtx_Set(&v[1], &c[1], &st[1], &color);
    ClipVtx_Set(&v[2], &c[2], &st[2], &color);
    EftGfx_DrawPolyScaledZ(v, layer, 1, 0, flip, 0, tex->e[texIdx].tex0, 2.0f);
    ClipVtx_Set(&v[0], &c[1], &st[1], &color);
    ClipVtx_Set(&v[1], &c[2], &st[2], &color);
    ClipVtx_Set(&v[2], &c[3], &st[3], &color);
    EftGfx_DrawPolyScaledZ(v, layer, 1, 0, flip, 0, tex->e[texIdx].tex0, 2.0f);
}

/* Fills the UV rectangles of a texture sheet of cols x rows frames (each clamped to 1..4), row by row. */
void EftQuad_SetSheet(EftQuadWork *w, u8 cols, u8 rows) {
    f32 d[2];
    s32 i;
    s32 j;
    s32 n;

    if (cols == 0) {
        cols = 1;
    } else if (cols > 4) {
        cols = 4;
    }
    if (rows == 0) {
        rows = 1;
    } else if (rows > 4) {
        rows = 4;
    }
    d[0] = 1.0f / cols;
    d[1] = 1.0f / rows;
    w->frames = cols * rows;
    n = 0;
    for (j = 0; j < rows; j++) {
        for (i = 0; i < cols; i++) {
            w->uv[n][0] = d[0] * i;
            w->uv[n][1] = d[1] * j;
            w->uv[n][2] = d[0] * i + d[0];
            w->uv[n][3] = d[1] * j + d[1];
            n++;
        }
    }
}

/* Binds the emitter to two entries of a texture table. */
void EftQuad_SetTex(EftQuadWork *w, EftYTex8 *tex, s32 a, s32 b) {
    w->texB = b;
    w->tex = tex;
    w->texA = a;
    w->tex0 = *(a + tex->e);
    w->tex1 = *(b + tex->e);
}

/* Builds the GS TEX0 value of the emitter's texture pair into the table entry, once per entry. */
void EftQuad_LoadTex(EftQuadWork *w, EftYTex8 *tex) {
    if (tex != NULL && !(tex->loaded & (1 << w->texA))) {
        u64 t = EftVram_AddImage(&w->tex0, 1, 0);

        t |= (u64)EftVram_AddClut(&w->tex1) << 37;
        tex->e[w->texA].tex0 = t;
        tex->loaded |= 1 << w->texA;
    }
}

/* Current values = the third key of every animated parameter. */
void EftQuad_SetLastKey(EftQuadWork *w) {
    EftQuadDef *def = w->def;
    EftQuadDef2 *def2 = w->def2;

    w->cur.a.x = def->a[2].x;
    w->cur.a.y = def->a[2].y;
    w->cur.a.z = def->a[2].z;
    w->cur.b.x = def->b[2].x;
    w->cur.b.y = def->b[2].y;
    w->cur.b.z = def->b[2].z;
    w->cur.c.x = def->c[2].x;
    w->cur.c.y = def->c[2].y;
    w->cur.c.z = def->c[2].z;
    w->cur.d.x = def->d[2].x;
    w->cur.d.y = def->d[2].y;
    w->cur.d.z = def->d[2].z;
    w->cur.o = def->o[2];
    w->cur.q = def->q[2];
    w->cur.s = def->s[2];
    w->cur.p = def->p[2];
    w->cur.r = def->r[2];
    w->cur.t = def->t[2];
    w->cur.e.x = def->e[2].x;
    w->cur.e.y = def->e[2].y;
    w->cur.e.z = def->e[2].z;
    w->cur.f.x = def->f[2].x;
    w->cur.f.y = def->f[2].y;
    w->cur.f.z = def->f[2].z;
    w->cur.g.x = def->g[2].x;
    w->cur.g.y = def->g[2].y;
    w->cur.g.z = def->g[2].z;
    w->cur.i.x = def->i[2].x;
    w->cur.i.y = def->i[2].y;
    w->cur.i.z = def->i[2].z;
    w->cur.j.x = def->j[2].x;
    w->cur.j.y = def->j[2].y;
    w->cur.j.z = def->j[2].z;
    w->cur.k.a = def->k[2].a;
    w->cur.k.b = def->k[2].b;
    w->cur.l = def->l[2];
    w->cur.m = def->m[2];
    w->cur.n = def->n[2];
    w->cur.u = def->u[2];
    w->cur.v = def->v[2];
    w->cur.w = def->w[2];
    Vec4_Copy((EftYVec *)&w->cur.v0, &def2->v0[2]);
    Vec4_Copy((EftYVec *)&w->cur.v1, &def2->v1[2]);
    Vec4_Copy((EftYVec *)&w->cur.v2, &def2->v2[2]);
    Vec4_Copy((EftYVec *)&w->cur.v3, &def2->v3[2]);
    w->cur.c0 = def2->c0[2];
    w->cur.c1 = def2->c1[2];
    w->cur.p0a = def2->p0[2].a;
    w->cur.p1a = def2->p1[2].a;
    w->cur.p2a = def2->p2[2].a;
    w->cur.p0b = def2->p0[2].b;
    w->cur.p1b = def2->p1[2].b;
    w->cur.p2b = def2->p2[2].b;
    w->cur.e2 = def2->e[2];
}

/* Creates a quad emitter from separate arguments. No caller in the executable. */
EftYTask *EftQuad_CreateEx(EftQuadDef *def, EftQuadDef2 *def2, EftYTex8 *tex, EftYVec pos, EftYVec dir, f32 size,
                           f32 life) {
    EftQuadArg arg;

    memset(&arg, 0, sizeof(EftQuadArg));
    if (gEftQuadMgr == NULL) {
        return NULL;
    }
    if (def == NULL) {
        return NULL;
    }
    if (def2 == NULL) {
        return NULL;
    }
    arg.tex = tex;
    arg.def = def;
    arg.def2 = def2;
    arg.pos = pos;
    arg.dir = dir;
    arg.size = size;
    arg.life = life;
    return BtlTaskList_AddTail(gEftQuadMgr->tasks, gEftQuadClass, &arg);
}

/* Creates a quad emitter task in the pool's task list. */
EftYTask *EftQuad_Create(EftQuadArg *arg) {
    if (gEftQuadMgr == NULL) {
        return NULL;
    }
    if (arg == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftQuadMgr->tasks, gEftQuadClass, arg);
}

/* Same as EftQuad_Stop. No caller. */
void EftQuad_StopAlias(EftYTask *task) {
    EftQuad_Stop(task);
}

/* Asks the emitter to stop emitting: at once, or after its stop delay, or through its fade when those are set. */
void EftQuad_Stop(EftYTask *task) {
    EftQuadWork *w;
    s32 now = 1;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (!(w->flags & EFT_QUADEM_ALIVE)) {
        return;
    }
    if (w->stopDelay > 0.0f) {
        w->flags |= EFT_QUADEM_FADING;
        now = 0;
    }
    if (w->fade > 0.0f) {
        w->flags |= EFT_QUADEM_STOP_REQ;
        now = 0;
    }
    if (now) {
        w->flags |= EFT_QUADEM_STOPPED;
    }
}

/* Sets the length of the fade that follows a stop, in frames. */
void EftQuad_SetFade(EftYTask *task, s32 frames) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        f32 f = frames;

        w->fade = f;
        w->fadeLeft = f;
    }
}

/* Sets the emitter's position and direction. No caller. */
void EftQuad_SetPosDir(EftYTask *task, EftYVec pos, EftYVec dir) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        Vec3_Copy(&w->pos, &pos);
        Vec3_Copy(&w->dir, &dir);
        Vec3_Normalize(&w->dir, &w->dir);
    }
}

/* Stops the emitter and drops all its quads at the next step. */
void EftQuad_Kill(EftYTask *task) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        w->flags |= EFT_QUADEM_KILL | EFT_QUADEM_STOPPED;
    }
}

/* Moves the emitter. */
void EftQuad_SetPos(EftYTask *task, EftYVec pos) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        Vec3_Copy(&w->pos, &pos);
    }
}

/* Moves the emitter and rebuilds the corners of all its quads at the new place at once. */
void EftQuad_Warp(EftYTask *task, EftYVec pos) {
    EftQuadWork *w;
    EftQuad *q;
    f32 pitch;
    f32 yaw;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        pitch = EftMath_WrapAngle(Mathf_Asin(-w->dir.y));
        yaw = EftMath_WrapAngle(atan2f(w->dir.x, w->dir.z));
        Vec3_Copy(&w->pos, &pos);
        Vu0Cur_Push();
        for (q = w->head; q != NULL; q = q->next) {
            Vu0Cur_LoadIdentity();
            Vu0Cur_ScaleDiagUniform(w->size);
            if (w->flags & EFT_QUADEM_OWN_ORIGIN) {
                EftQuad_BuildCorners(q, w, q->origin, pitch, yaw);
            } else {
                EftQuad_BuildCorners(q, w, w->pos, pitch, yaw);
            }
        }
        Vu0Cur_Pop();
    }
}

/* Sets the emitter's direction (normalised). */
void EftQuad_SetDir(EftYTask *task, EftYVec dir) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        Vec3_Copy(&w->dir, &dir);
        Vec3_Normalize(&w->dir, &w->dir);
    }
}

/* Sets the emitter's size. */
void EftQuad_SetSize(EftYTask *task, f32 size) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        w->size = size;
    }
}

/* Sets how long the emitter runs before it stops by itself, in seconds. No caller. */
void EftQuad_SetLife(EftYTask *task, f32 seconds) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        w->life = seconds * 30.0f;
    }
}

/* Replaces the texture table pointer. No caller. */
void EftQuad_SetTexTable(EftYTask *task, EftYTex8 *tex) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (tex == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        w->tex = tex;
    }
}

/* Binds the emitter to two entries of a texture table. */
void EftQuad_SetTexPair(EftYTask *task, EftYTex8 *tex, s32 a, s32 b) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (tex == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (!(w->flags & EFT_QUADEM_ALIVE)) {
        return;
    }
    EftQuad_SetTex(w, tex, a, b);
}

/* Sets the frames to wait before the emitter starts. */
void EftQuad_SetDelay(EftYTask *task, s32 frames) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        w->delay = frames;
    }
}

/* Sets the frames between a stop request and the stop. */
void EftQuad_SetStopDelay(EftYTask *task, s32 frames) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        w->stopDelay = frames;
    }
}

/* Marks the emitter as drawn only in its owner's view. Returns 1 when the emitter is alive. */
s32 EftQuad_SetViewOnly(EftYTask *task) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (!(w->flags & EFT_QUADEM_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_QUADEM_VIEW_ONLY;
    return 1;
}

/* Returns 1 while the emitter exists. */
s32 EftQuad_IsAlive(EftYTask *task) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        return 1;
    }
    return 0;
}

/* Chooses whether emitted quads stay where they were emitted (1) or follow the emitter (0). */
void EftQuad_SetOwnOrigin(EftYTask *task, s32 on) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        if (on) {
            w->flags |= EFT_QUADEM_OWN_ORIGIN;
        } else {
            w->flags &= ~EFT_QUADEM_OWN_ORIGIN;
        }
    }
}

/* Stores the cut id the emitter's update and draw hand to BtlScene_IsEffectStopped / IsEffectHidden. */
void EftQuad_SetCut(EftYTask *task, s32 cut) {
    EftQuadWork *w;

    if (gEftQuadMgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    w = task->work;
    if (w == NULL) {
        return;
    }
    if (w->flags & EFT_QUADEM_ALIVE) {
        w->cut = cut;
    }
}
