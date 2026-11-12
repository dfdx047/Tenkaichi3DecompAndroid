#include "common.h"
#include "battle/eft_t.h"
#include "sys/gfx_ot.h"

/*
 * 0x17EE68..0x1809C0: the streak field, the "numbered stage effect" technique modules start with
 * EftStreak_Start(objId, kind, colour, angle) and stop with EftStreak_Stop (eft_j.c, eft_k.c, eft_l.c). Manager
 * class 0x2C3E90, task class 0x2C3EA8. One of ten kinds (parameter files, common entries 0x24F..0x258): up to
 * 255 long quads that run along one axis of the view, spread across it with a hole around the middle, drawn
 * either in the world around a point in front of the camera or straight onto the screen.
 *
 * Drawing only: it reads a fighter's node position, height and "in a technique" state and writes nothing but its
 * own work. Random numbers: the VU0 register (Rand_FloatRange), six per streak each time one is rolled.
 *
 * Two functions are INCLUDE_ASM with the attempt in `#if 0` above them: EftStreak_Draw (2 instructions) and
 * EftStreak_DrawScreen (20 instructions, registers). The file's .rodata is 0x2ECE60..0x2ECEC0 (the id table of EftStreakMgr_Init, then the
 * two constants of the INCLUDE_ASM functions); its only .lit4 word is 0x2FCCAC (EftStreak_Step).
 */

extern EftTCamView *gBtlCamView;

extern void *memset(void *dst, s32 c, u32 n);
extern f32 Rand_FloatRange(f32 a, f32 b);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Length(Vec4 *v);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Mtx_Translate(Mtx44 *dst, Mtx44 *src, Vec4 *pos);  /* translate */
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);     /* matrix product */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);             /* inverse of a rotation + translation matrix */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);  /* rotate about Z */
extern void Vu0Cur_Push(void);                               /* saves the VU0 matrix state */
extern void Vu0Cur_Pop(void);                               /* restores it */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                           /* loads a matrix into VU0 */
extern void ClipVtx_Set(EftTClipVtx *out, Vec4 *pos, Vec4 *st, Vec4 *col); /* one clip vertex */
extern void Vec3_Div(Vec4 *dst, Vec4 *src, f32 div);      /* dst = src / div */
extern void Vec4_ToInt(EftTIVec *dst, Vec4 *src);           /* float vector to integer vector */
extern s32 Mtx_ProjectPoint(EftTIVec *out, Mtx44 *m, Vec4 *pos);  /* projects one point */
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, Vec4 *pos, f32 scale); /* dst = pos + dir * scale */

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 type);
extern void *BtlTask_CreateChildList(EftTTask *task, s32 count, s32 workSize);
extern EftTTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_SetDead(EftTTask *task);                     /* kills the task */
extern u64 EftVram_AddImage(EftTTex *tex, s32 a, s32 b);          /* steps a texture, returns its TEX0 */
extern u64 EftVram_AddClut(EftTTex *tex);                        /* steps a texture, returns its CLUT base */
extern void EftTexSet_Load8(void *tex, s32 *entry);              /* builds a texture set from a pack entry */

extern s32 EftCam_IsActive(void);
extern void EftGfx_DrawPolyScaledZ(EftTClipVtx *v, s32 blend, s32 a2, s32 a3, s32 a4, s32 a5, u64 tex0, f32 scale);

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_IsInTechnique(s32 objId);

extern void *gEftStreakClass[6]; /* streak task class */

extern EftStreakRoot *gEftStreak;

void EftStreak_Update(EftTTask *task);
void EftStreak_SpawnAll(EftStreakWork *w);
EftStreak *EftStreak_Alloc(void);
void EftStreak_Roll(EftStreak *s, EftStreakWork *w, f32 offset);
void EftStreak_Step(EftStreakWork *w);
void EftStreak_DrawWorld(Vec4 *corner, EftTVec uv0, EftTVec uv1, EftTVec color, s32 blend, s32 tex, s32 flag,
                         EftStreakMgr *mgr);
void EftStreak_DrawScreen(Vec4 *corner, EftTVec color, s32 blend, s32 segs, s32 z, u64 tex0);
void EftStreak_StepTexture(EftStreakWork *w);

/* Init callback of the manager class 0x2C3E90: the work, the ten parameter files, the colour table, the texture
   set and a list of 3 tasks. */
void EftStreakMgr_Init(EftTTask *task) {
    s32 i = 0;

    gEftStreak = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftStreakRoot));
    gEftStreak->mgr = NULL;
    gEftStreak->mgr = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftStreakMgr));
    memset(gEftStreak->mgr, 0, sizeof(EftStreakMgr));
    {
        s32 ids[EFT_STREAK_KINDS] = { 0x24F, 0x250, 0x251, 0x252, 0x253, 0x254, 0x255, 0x256, 0x257, 0x258 };

        do {
            gEftStreak->mgr->def[i] = (EftStreakDef *)BtlScene_GetCommonEntry(ids[i]);
            i++;
        } while (i < EFT_STREAK_KINDS);
    }
    gEftStreak->mgr->colors = (EftTVec *)BtlScene_GetCommonEntry(0x24E);
    EftTexSet_Load8(gEftStreak->mgr, BtlScene_GetCommonEntry(0x24D));
    gEftStreak->mgr->list = BtlTask_CreateChildList(task, 3, sizeof(EftStreakWork));
}

/* Update callback of the manager class: lets the texture be stepped again. */
void EftStreakMgr_Update(EftTTask *task) {
    gEftStreak->mgr->flags = 0;
}

/* Reset callback of the manager class: nothing. */
void EftStreakMgr_Reset(EftTTask *task) {
}

/* Term callback of the manager class. */
void EftStreakMgr_Term(EftTTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftStreak->mgr);
    BtlPool_Free(BtlPool_GetCurrent(), gEftStreak);
    gEftStreak = NULL;
}

/* Init callback of the task class 0x2C3EA8. */
void EftStreak_Init(EftTTask *task, EftStreakArg *arg) {
    EftStreakWork *w = task->work;
    EftStreakDef *def = arg->def;
    EftStreakMgr *mgr;

    memset(w, 0, sizeof(EftStreakWork));
    w->def = arg->def;
    w->colors = arg->colors;
    w->color = arg->color;
    w->objId = arg->objId;
    w->type = 2;
    w->startDelay = def->startDelay;
    w->fade = def->fadeFrames;
    w->alpha = 1.0f;
    w->angle = arg->angle;
    w->cover = 1.0f;
    w->timer = arg->seconds * 30.0f;
    w->head = NULL;
    w->tail = NULL;
    mgr = gEftStreak->mgr;
    w->tex = mgr->tex[0];
    w->tex2 = ((EftTTex *)mgr)[w->color];
    switch (def->space) {
        case 2:
        case 3:
            w->flags |= EFT_STREAK_SCREEN;
            break;
        case 0:
        case 1:
            w->flags |= EFT_STREAK_WORLD;
            break;
    }
    switch (def->space) {
        case 1:
        case 3:
            w->flags |= EFT_STREAK_AT_CHAR;
            break;
    }
    w->flags |= EFT_STREAK_ALIVE;
}

/* Update callback: start delay, the kind's end condition, spawning, fade, the streaks, the life timer; then the
   texture, once per frame for all fields. */
void EftStreak_Update(EftTTask *task) {
    EftStreakWork *w = task->work;
    EftStreakDef *def = w->def;

    if (!BtlScene_IsEffectStopped(w->objId, w->type)) {
        if (0.0f <= w->startDelay) {
            w->startDelay -= 1.0f;
        } else {
            switch (def->endMode) {
                case 1: {
                    s32 hit = BtlCharApi_IsInTechnique(w->objId) != 0;

                    if (BtlCharApi_GetOpponentObjId(w->objId) >= 0) {
                        if (BtlCharApi_IsInTechnique(BtlCharApi_GetOpponentObjId(w->objId))) {
                            hit = 1;
                        }
                    }
                    if (hit) {
                        w->flags |= EFT_STREAK_DEAD;
                    }
                    break;
                }
                case 2:
                    if (EftCam_IsActive()) {
                        w->flags |= EFT_STREAK_DEAD;
                    }
                    break;
            }
            if (!(w->flags & EFT_STREAK_STOP)) {
                EftStreak_SpawnAll(w);
            }
            if (w->flags & EFT_STREAK_STOP) {
                f32 t = w->fade / def->fadeFrames;

                f32 a;

                if (t < 0.0f) {
                    a = 0.0f;
                } else if (1.0f < t) {
                    a = 1.0f;
                } else {
                    a = t;
                }
                w->alpha = a;
                w->fade -= 1.0f;
                if (w->fade < 0.0f) {
                    w->flags |= EFT_STREAK_DEAD;
                }
            }
            EftStreak_Step(w);
            if (0.0f < w->timer) {
                w->timer -= 1.0f;
                if (w->timer < 0.0f) {
                    w->flags |= EFT_STREAK_STOP;
                }
            }
        }
        if (w->flags & EFT_STREAK_DEAD) {
            w->flags &= ~EFT_STREAK_ALIVE;
            BtlTask_SetDead(task);
        }
    }
    if (!(w->flags & EFT_STREAK_DEAD)) {
        EftStreak_StepTexture(w);
    }
}

/* Post-update callback: nothing. */
void EftStreak_PostUpdate(EftTTask *task) {
}

/* Draw callback. The field's frame is the camera's rotation at a point `dist` in front of it (30, or the owner's
   distance plus half its height, or the position given by EftStreak_SetPos), or that point's screen position for
   a screen field. Every drawn streak is a quad of width x length at (pos, offset), the whole field turned by
   `angle` about the view axis. */
#if 0
/* NON-MATCHING: 2 of 387 instructions: in the argument set-up of the last memset at the top (the `d` initialiser) the
   original has `addiu s1,sp,0xC0` in front of `move a1,zero`, this C the other way round (scheduling only).
   Round 4 (the permuter did not move it in 171,000 iterations either): the first block runs from the entry to
   the POS_SET test (calls do not end blocks), and both scheduling passes issue the sixth memset's arguments as
   `$5 = 0` in the cycle of the fifth call, then `r = sp + 0xC0` / `$6 = 16`, then `$4 = r`. `$5 = 0` wins the
   tie against `$6 = 16` by source position alone (equal priority, weight and dependents), and the argument
   moves are always emitted $4, $5, $6. The original's order (size, address, zero) therefore needs the 16 in a
   pseudo of its own that survives to register allocation (`r16 = 16` has the longer path and is issued with the
   call, `$6 = r16` then beats `$5 = 0` because a register dies in it, and local-alloc ties r16 to $6): the same
   family as the "surviving copy" functions in the guide, here a constant. No source form keeps one: tried
   `= { 0 }`, `= { 0, 0, 0, 0 }`, `= { 0.0f }`, an EftTVec, a one-element array, explicit memset before / after
   `dist = 0.0f`, through a non-builtin alias, with the size or the zero in an address-taken variable, a (u64)
   size, `dist` declared first (all 2 left), a `static inline` clear (49).
   Found earlier: the "no depth" argument of EftStreak_DrawWorld is a variable built in two steps
   (`f = flags & AT_CHAR; f = f == 0; if (flags & POS_SET) f = 0;`), which gives `andi / sltiu / movn` and, with it,
   the original's allocation (w, def, mgr and &w->axis on the stack, fp for &corner[0].y). */
void EftStreak_Draw(EftTTask *task) {
    EftStreakWork *w = task->work;
    EftStreakDef *def = w->def;
    EftStreakMgr *mgr = gEftStreak->mgr;
    Mtx44 frame = { 0 };
    Vec4 node = { 0 };
    Mtx44 camInv = { 0 };
    EftTVec dir = { 0.0f, 0.0f, 1.0f, 1.0f };
    Vec4 center = { 0 };
    EftTIVec scr = { 0 };
    Vec4 d = { 0 };
    f32 dist = 0.0f;

    Mtx_StoreIdentity(&frame);
    Mtx_InverseRT(&camInv, &gBtlCamView->view);
    Vec4_Set((Vec4 *)camInv.m[3], 0.0f, 0.0f, 0.0f, 1.0f);
    Mtx_MulVec4((Vec4 *)&dir, &camInv, (Vec4 *)&dir);
    Vec3_Normalize((Vec4 *)&dir, (Vec4 *)&dir);
    if (!(w->flags & EFT_STREAK_POS_SET)) {
        if (w->flags & EFT_STREAK_AT_CHAR) {
            BtlCharApi_GetNodePos(w->objId, 3, &node);
            Vec3_Sub(&d, &node, &gBtlCamView->pos);
            dist = Vec3_Length(&d);
            dist += BtlCharApi_GetHeight(w->objId) * 0.5f;
        } else {
            dist = 30.0f;
        }
        Vec3_ScaleAdd(&center, (Vec4 *)&dir, &gBtlCamView->pos, dist);
    } else {
        Vec4_Copy(&center, (Vec4 *)&w->pos);
    }
    Mtx_ProjectPoint(&scr, &gBtlCamView->screen, &center);
    if (w->flags & EFT_STREAK_WORLD) {
        Mtx_Mul(&frame, &frame, &camInv);
        Vec4_Copy((Vec4 *)frame.m[3], &center);
    } else {
        if (scr.z < 0) {
            return;
        }
        frame.m[3][0] = (scr.x - 0x7000) >> 4;
        frame.m[3][1] = (scr.y - 0x7200) >> 4;
        frame.m[3][2] = 0.0f;
        frame.m[3][3] = 1.0f;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->screen);
    {
        Vec4 base[4] = { 0 };
        Vec4 corner[4] = { 0 };
        Vec4 along = { 0 };
        Vec4 across = { 0 };
        Vec4 uv[2] = { 0 };
        Mtx44 m = { 0 };
        EftStreak *s;
        s32 j;

        Vec4_Set(&base[0], -0.5f, -0.5f, 0.0f, 1.0f);
        Vec4_Set(&base[1], -0.5f, 0.5f, 0.0f, 1.0f);
        Vec4_Set(&base[2], 0.5f, -0.5f, 0.0f, 1.0f);
        Vec4_Set(&base[3], 0.5f, 0.5f, 0.0f, 1.0f);
        Vec4_Set(&uv[0], 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Set(&uv[1], 1.0f, 0.0f, 1.0f, 1.0f);
        for (s = w->head; s != NULL; s = s->next) {
            if (s->flags & EFT_STREAK_ITEM_DRAWN) {
                Mtx_StoreIdentity(&m);
                Vec4_Set((Vec4 *)&w->axis, 1.0f, 0.0f, 0.0f, 1.0f);
                Vec3_Scale(&along, (Vec4 *)&w->axis, s->pos);
                Vec4_Set(&across, 0.0f, s->offset, 0.0f, 1.0f);
                Mtx_Translate(&m, &m, &across);
                Mtx_Translate(&m, &m, &along);
                Mtx_RotateZ(&m, &m, w->angle);
                for (j = 0; j < 4; j++) {
                    corner[j].x = base[j].x * s->width;
                    corner[j].y = base[j].y * s->length;
                    corner[j].z = base[j].z;
                    corner[j].w = 1.0f;
                    Mtx_MulVec4(&corner[j], &m, &corner[j]);
                    Mtx_MulVec4(&corner[j], &frame, &corner[j]);
                }
                if (w->flags & EFT_STREAK_WORLD) {
                    s32 f = w->flags & EFT_STREAK_AT_CHAR;

                    f = f == 0;

                    if (w->flags & EFT_STREAK_POS_SET) {
                        f = 0;
                    }
                    EftStreak_DrawWorld(corner, *(EftTVec *)&uv[0], *(EftTVec *)&uv[1], s->color, def->blend, 0,
                                        f, mgr);
                } else {
                    EftStreak_DrawScreen(corner, s->color, def->blend, 4,
                                         (w->flags & EFT_STREAK_AT_CHAR) ? scr.z : 0xFFFFFF, mgr->tex[0].tex0);
                }
            }
        }
    }
    Vu0Cur_Pop();
}
#else
RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/battle/eft_t_b", D_002ECE90); /* (0, 0, 1, 1): dir below */
INCLUDE_ASM("asm/nonmatchings/battle/eft_t_b", EftStreak_Draw);
#endif

/* Reset callback: the field does not survive a scene reset. */
void EftStreak_Reset(EftTTask *task) {
    EftStreakWork *w = task->work;

    w->flags &= ~EFT_STREAK_ALIVE;
    BtlTask_SetDead(task);
}

/* Term callback: gives the streaks back to the pool. */
void EftStreak_Term(EftTTask *task) {
    EftStreakWork *w = task->work;
    EftStreak *s;

    for (s = w->head; s != NULL; s = s->next) {
        s->flags = 0;
    }
    memset(w, 0, sizeof(EftStreakWork));
}

/* Creates the kind's streaks, evenly spread across the band; stops early when the pool is used up. */
void EftStreak_SpawnAll(EftStreakWork *w) {
    EftStreakDef *def = w->def;
    f32 gap = def->spread / (f32)def->count;

    if (w->spawned < def->count) {
        do {
            EftStreak *s = EftStreak_Alloc();

            if (s == NULL) {
                break;
            }
            EftStreak_Roll(s, w, def->spread * 0.5f - gap * (f32)w->spawned);
            s->next = NULL;
            s->flags = EFT_STREAK_ITEM_USED;
            if (w->head == NULL) {
                w->head = s;
                w->tail = s;
            } else {
                w->tail->next = s;
                w->tail = s;
            }
            w->spawned++;
        } while (w->spawned < def->count);
    }
}

/* Finds a free streak in the pool, searching on from the last one handed out. */
EftStreak *EftStreak_Alloc(void) {
    u32 i;

    if (gEftStreak->mgr->next >= EFT_STREAK_MAX) {
        gEftStreak->mgr->next = 0;
    }
    i = gEftStreak->mgr->next;
    do {
        EftStreak *s = &gEftStreak->mgr->pool[i];

        i++;
        if (i >= EFT_STREAK_MAX) {
            i = 0;
        }
        if (s->flags == 0) {
            gEftStreak->mgr->next = i;
            return s;
        }
    } while (i != gEftStreak->mgr->next);
    return NULL;
}

/* Gives a streak its colour and random size, position, speed, life and delay. */
void EftStreak_Roll(EftStreak *s, EftStreakWork *w, f32 offset) {
    EftStreakDef *def = w->def;

    Vec4_Copy((Vec4 *)&s->color, (Vec4 *)&w->colors[w->color]);
    s->width = Rand_FloatRange(def->widthMin, def->widthMin + def->widthRange);
    s->length = Rand_FloatRange(def->lengthMin, def->lengthMin + def->lengthRange);
    s->pos = Rand_FloatRange(def->posMin, def->posMin + def->posRange);
    s->speed = Rand_FloatRange(def->speedMin, def->speedMin + def->speedRange);
    s->life = Rand_FloatRange(def->lifeMin, def->lifeMin + def->lifeRange) * 30.0f;
    s->time = 0.0f;
    s->delay = Rand_FloatRange(s->time, def->delayMax);
    s->offset = offset;
}

/* Steps every streak: delay, movement, the fade in over the first 40 % of its life and out over the rest, and
   whether it is drawn (not inside the hole, alpha above 0). An expired streak is rolled again in place. */
void EftStreak_Step(EftStreakWork *w) {
    EftStreakDef *def = w->def;
    EftTVec *colors;
    EftStreak *s;
    f32 half = 0.0f;
    f32 a;
    f32 in; /* fraction of the life spent fading in */
    f32 hole;
    f32 r;

    colors = w->colors;
    hole = def->hole * w->cover;
    a = 1.0f;
    in = 0.4f;
    if (hole < half) {
        r = half;
    } else if (a < hole) {
        r = a;
    } else {
        r = hole;
    }
    half = def->spread * r * 0.5f;
    for (s = w->head; s != NULL; s = s->next) {
        if (0.0f < s->delay) {
            s->delay -= 1.0f;
            continue;
        }
        s->pos += s->speed;
        if (0.0f < s->life) {
            f32 t = s->time / s->life;

            if (t < in) {
                f32 x = t / in;

                if (x < 0.0f) {
                    a = 0.0f;
                } else {
                    a = x;
                    if (1.0f < a) {
                        a = 1.0f;
                    }
                }
            } else {
                f32 x = 1.0f - (t - in) / (1.0f - in);

                /* the upper bound is compared through `a`: with a literal the 1.0 of this arm is not hoisted */
                if (x < 0.0f) {
                    a = 0.0f;
                } else {
                    a = 1.0f;
                    if (!(a < x)) {
                        a = x;
                    }
                }
            }
            s->time += 1.0f;
            if (s->life <= s->time) {
                s->flags |= EFT_STREAK_ITEM_EXPIRED;
            }
        }
        s->color.w = colors[w->color].w * w->alpha * a;
        if ((s->offset < half && -half < s->offset) || s->color.w <= 0.0f) {
            s->flags &= ~EFT_STREAK_ITEM_DRAWN;
        } else {
            s->flags |= EFT_STREAK_ITEM_DRAWN;
        }
        if (s->flags & EFT_STREAK_ITEM_EXPIRED) {
            EftStreak_Roll(s, w, s->offset);
            s->flags = EFT_STREAK_ITEM_USED;
        }
    }
}

/* A quad in the world: two clipped triangles; uv0 / uv1 hold the texture coordinates of two corners each. */
void EftStreak_DrawWorld(Vec4 *corner, EftTVec uv0, EftTVec uv1, EftTVec color, s32 blend, s32 tex, s32 flag,
                         EftStreakMgr *mgr) {
    Vec4 p[4];
    Vec4 st[4];
    EftTClipVtx tri[9];

    Vec4_Set(&st[0], uv0.x, uv0.y, 1.0f, 1.0f);
    Vec4_Set(&st[1], uv0.z, uv0.w, 1.0f, 1.0f);
    Vec4_Set(&st[2], uv1.x, uv1.y, 1.0f, 1.0f);
    Vec4_Set(&st[3], uv1.z, uv1.w, 1.0f, 1.0f);
    Vec4_Copy(&p[0], &corner[0]);
    Vec4_Copy(&p[1], &corner[1]);
    Vec4_Copy(&p[2], &corner[2]);
    Vec4_Copy(&p[3], &corner[3]);
    ClipVtx_Set(&tri[0], &p[0], &st[0], (Vec4 *)&color);
    ClipVtx_Set(&tri[1], &p[1], &st[1], (Vec4 *)&color);
    ClipVtx_Set(&tri[2], &p[2], &st[2], (Vec4 *)&color);
    EftGfx_DrawPolyScaledZ(tri, blend, 1, 0, flag, 0, mgr->tex[tex].tex0, 2.0f);
    ClipVtx_Set(&tri[0], &p[1], &st[1], (Vec4 *)&color);
    ClipVtx_Set(&tri[1], &p[2], &st[2], (Vec4 *)&color);
    ClipVtx_Set(&tri[2], &p[3], &st[3], (Vec4 *)&color);
    EftGfx_DrawPolyScaledZ(tri, blend, 1, 0, flag, 0, mgr->tex[tex].tex0, 2.0f);
}

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF); layers 2 and 3 are 0 and 1. */
static inline void EftTOt_Add(OtPrim *p, s32 z, s32 layer) {
    OtEntry *e;

    if (layer >= 2) {
        layer -= 2;
    }
    if (z < 0) {
        e = &gOtZ[0].layer[layer];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[layer];
    } else {
        e = &gOtZ[z].layer[layer];
    }
    e->tail->next = p;
    e->tail = p;
}

/* A quad on the screen (corners in pixels), cut into `segs` strips; z is its GS depth (0xFFFFFF = the far limit,
   queued in depth slot 0; otherwise the slot is z >> 8). */
#if 0
/* NON-MATCHING: 20 of 376 instructions, registers only (s4..s7); same length.
   Round 4: the header stores in the order prim, tag, vif0, vif1, gif0, gif1, next give the original's header
   (it was 41; the a1 / t2 exchange is gone). What is left is ONE allocation race: slot is in s7 where the
   original has s4, which shifts z and the three hoisted addresses &p[3] / &p[1] / &p[0] (original s5 / s6 / s7).
   Numbers from the -dl / -dg dumps (build/scratch_cleanup2_D/lr.py): slot has 9 references (2 sets + 1 use
   outside the loop, 3 uses at loop depth 1 counted twice) over a live length of 538, priority 3 * 9 / 538 = 501;
   the three addresses have 5 references over 190 / 192 / 193, priority 526 / 520 / 518. The 538 is DOUBLE the
   real range (269 instructions from `slot = 0` to the loop end): the first cse pass puts a REG_EQUAL note on
   every `reg = constant`, and local-alloc doubles the live length of a register whose first set has such a
   note even when the register is set again. So the original had either a 10th reference to slot (3 * 10 / 538
   = 557), or `slot = 0` at least 13 instructions later in the first block at allocation time (the first
   scheduling pass puts it in the first free second slot, here cycle 15, behind the third memset), or loop
   bodies 7 to 10 instructions longer at allocation time. The addresses are NOT doubled in the original (the
   fourth one, &p[2] in fp, is: it would then have come before two of them).
   Tried without effect (20 left each time): `slot = 0` as a statement in six places, first / last declaration,
   the chain insertion written out in the loop with and without a copy of slot, `zz = z` in the far arm.
   Worse: `slot = 0` only in the far arm (154), the far arm first (358), z reused instead of zz (358),
   `do { } while (0)` around the chain insertion (219: it becomes the innermost loop).
   Found earlier: the far z goes to a second variable (zz) set in both arms; corner itself is advanced by 3 after
   &corner[2] is taken; the w stores are written 3, 2, 1, 0; the loop is `if (segs > 0) do { } while (--segs !=
   0)`; the screen x / y are read as u16. */
void EftStreak_DrawScreen(Vec4 *corner, EftTVec color, s32 blend, s32 segs, s32 z, u64 tex0) {
    Vec4 d[2] = { 0 };
    Vec4 p[4] = { 0 };
    EftTIVec scr[4] = { 0 };
    EftTIVec col = { 0 };
    Vec4 uv[2] = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } };
    Vec4 t = { 0 };
    f32 step[2] = { 0 };
    f32 fsegs = segs;
    s32 slot = 0;
    EftTQuadPkt *q;
    s32 zz;
    Vec4 *c3;
    Vec4 *c1 = corner + 1;

    Vec3_Sub(&d[0], &corner[0], c1);
    c3 = corner + 2;
    corner += 3;
    Vec3_Sub(&d[1], c3, corner);
    Vec3_Div(&d[0], &d[0], fsegs);
    Vec3_Div(&d[1], &d[1], fsegs);
    Vec4_Copy(&p[1], c1);
    Vec4_Copy(&p[3], corner);
    p[3].w = 1.0f;
    p[2].w = 1.0f;
    p[1].w = 1.0f;
    p[0].w = 1.0f;
    Vec4_ToInt(&col, (Vec4 *)&color);
    t.y = uv[0].w;
    t.w = uv[1].w;
    step[0] = (uv[0].y - uv[0].w) / fsegs;
    step[1] = (uv[1].y - uv[1].w) / fsegs;
    if (z != 0xFFFFFF) {
        slot = z >> 8;
        zz = slot << 8;
    } else {
        zz = 0xFFFFFF;
    }
    if (segs > 0) do {
        Vec3_Add(&p[0], &p[1], &d[0]);
        Vec3_Add(&p[2], &p[3], &d[1]);
        t.x = t.y + step[0];
        t.z = t.w + step[1];
        Vec4_ToInt(&scr[0], &p[0]);
        Vec4_ToInt(&scr[1], &p[1]);
        Vec4_ToInt(&scr[2], &p[2]);
        Vec4_ToInt(&scr[3], &p[3]);
        q = (EftTQuadPkt *)gOtCur;
        gOtCur = (u32 *)(q + 1);
        q->prim = 0x5C;
        q->tag = 0x20000008;
        q->vif0 = 0x10000000;
        q->vif1 = 0x50000008;
        q->gif0 = 0xE400000000008001;
        q->gif1 = 0x42142142142160;
        q->next = 0;
        q->v[0].rgba[0] = col.x;
        q->v[0].rgba[1] = col.y;
        q->v[0].rgba[2] = col.z;
        q->v[0].rgba[3] = col.w;
        q->v[0].q = 1.0f;
        q->v[1].rgba[0] = col.x;
        q->v[1].rgba[1] = col.y;
        q->v[1].rgba[2] = col.z;
        q->v[1].rgba[3] = col.w;
        q->v[1].q = 1.0f;
        q->v[2].rgba[0] = col.x;
        q->v[2].rgba[1] = col.y;
        q->v[2].rgba[2] = col.z;
        q->v[2].rgba[3] = col.w;
        q->v[2].q = 1.0f;
        q->v[3].rgba[0] = col.x;
        q->v[3].rgba[1] = col.y;
        q->v[3].rgba[2] = col.z;
        q->v[3].rgba[3] = col.w;
        q->v[3].q = 1.0f;
        q->v[0].s = uv[0].x;
        q->v[0].t = t.x;
        q->v[1].s = uv[0].z;
        q->v[1].t = t.y;
        q->v[2].s = uv[1].x;
        q->v[2].t = t.z;
        q->v[3].s = uv[1].z;
        q->v[3].t = t.w;
        q->v[0].xyz.x = (u16)scr[0].x * 16 + 0x7000;
        q->v[0].xyz.y = (u16)scr[0].y * 16 + 0x7200;
        q->v[0].xyz.z = zz;
        q->v[0].xyz.f = 0xFF;
        q->v[1].xyz.x = (u16)scr[1].x * 16 + 0x7000;
        q->v[1].xyz.y = (u16)scr[1].y * 16 + 0x7200;
        q->v[1].xyz.z = zz;
        q->v[1].xyz.f = 0xFF;
        q->v[2].xyz.x = (u16)scr[2].x * 16 + 0x7000;
        q->v[2].xyz.y = (u16)scr[2].y * 16 + 0x7200;
        q->v[2].xyz.z = zz;
        q->v[2].xyz.f = 0xFF;
        q->v[3].xyz.x = (u16)scr[3].x * 16 + 0x7000;
        q->v[3].xyz.y = (u16)scr[3].y * 16 + 0x7200;
        q->tex0 = tex0;
        q->v[3].xyz.z = zz;
        q->v[3].xyz.f = 0xFF;
        EftTOt_Add((OtPrim *)q, slot, blend);
        Vec4_Copy(&p[1], &p[0]);
        Vec4_Copy(&p[3], &p[2]);
        t.y = t.x;
        t.w = t.z;
    } while (--segs != 0);
}
#else
INCLUDE_RODATA("asm/nonmatchings/battle/eft_t_b", D_002ECEA0); /* (0, 0, 0, 1), (1, 0, 1, 1): uv below */
INCLUDE_ASM("asm/nonmatchings/battle/eft_t_b", EftStreak_DrawScreen);
#endif

/* Steps the animation of the field's two textures and rebuilds the TEX0 the streaks are drawn with; once per
   frame however many fields exist. */
void EftStreak_StepTexture(EftStreakWork *w) {
    EftStreakMgr *mgr = gEftStreak->mgr;

    if (!(mgr->flags & 1)) {
        u64 tex0 = EftVram_AddImage(&w->tex, 1, 0);

        tex0 |= (u64)EftVram_AddClut(&w->tex2) << 37;
        mgr->tex[0].tex0 = tex0;
        mgr->flags |= 1;
    }
}

/* Starts streak field `kind` (0..9) for a fighter, turned by `angle`; returns its task. Colour 0..6 picks the
   colour and texture; 7 means 1, anything else 2. */
EftTTask *EftStreak_Start(s32 objId, s32 kind, s32 color, f32 angle) {
    EftStreakArg arg;

    if (gEftStreak == NULL) {
        return NULL;
    }
    if (gEftStreak->mgr == NULL) {
        return NULL;
    }
    if (gEftStreak->mgr->list == NULL) {
        return NULL;
    }
    if (kind >= EFT_STREAK_KINDS) {
        return NULL;
    }
    arg.def = gEftStreak->mgr->def[kind];
    arg.colors = gEftStreak->mgr->colors;
    arg.objId = objId;
    arg.kind = kind;
    arg.angle = angle;
    arg.seconds = 0.0f;
    if (color < 7) {
        arg.color = color & 0xFF;
    } else if (color == 7) {
        arg.color = 1;
    } else {
        arg.color = 2;
    }
    return BtlTaskList_AddTail(gEftStreak->mgr->list, gEftStreakClass, &arg);
}

#define IS_STREAK(task) ((task)->cls[0] == EftStreak_Update)

/* Starts the field's fade. */
void EftStreak_Stop(EftTTask *task) {
    EftStreakWork *w;

    if (gEftStreak != NULL && gEftStreak->mgr != NULL && task != NULL) {
        if (IS_STREAK(task)) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_STREAK_ALIVE) {
                    w->flags |= EFT_STREAK_STOP;
                }
            }
        }
    }
}

/* Gives the field a fixed centre instead of the point in front of the camera. No caller. */
void EftStreak_SetPos(EftTTask *task, EftTVec pos) {
    EftStreakWork *w;

    if (gEftStreak != NULL && gEftStreak->mgr != NULL && task != NULL) {
        if (IS_STREAK(task)) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_STREAK_ALIVE) {
                    Vec4_Copy((Vec4 *)&w->pos, (Vec4 *)&pos);
                    w->flags |= EFT_STREAK_POS_SET;
                }
            }
        }
    }
}

/* Scales the hole in the middle of the band. No caller. */
void EftStreak_SetCover(EftTTask *task, f32 cover) {
    EftStreakWork *w;

    if (gEftStreak != NULL && gEftStreak->mgr != NULL && task != NULL) {
        if (IS_STREAK(task)) {
            w = task->work;
            if (w != NULL) {
                if (w->flags & EFT_STREAK_ALIVE) {
                    w->cover = cover;
                }
            }
        }
    }
}

/* True while the task is a live streak field. No caller. */
s32 EftStreak_IsAlive(EftTTask *task) {
    EftStreakWork *w;

    if (gEftStreak == NULL) {
        return 0;
    }
    if (gEftStreak->mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (!IS_STREAK(task)) {
        return 0;
    }
    w = task->work;
    if (w == NULL) {
        return 0;
    }
    if (w->flags & EFT_STREAK_ALIVE) {
        return 1;
    }
    return 0;
}

/* Two empty functions without callers between the streak field and the vanish lines. */
void EftStreak_NopA(void) {
}

void EftStreak_NopB(void) {
}
