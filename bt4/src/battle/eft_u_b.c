#include "common.h"
#include "battle/eft_u.h"

/*
 * Particle emitter (effect pack part kind 5), 0x182CE8..0x1871A8. Written as two files and merged at integration
 * (EftPtcl_SetTexture only matches with EftPtcl_PickTexture defined above it): this first part,
 * 0x182CE8..0x1853C8, is the head of the EftPtcl module; the second part (formerly eft_v.c, 0x1853C8..0x1871A8)
 * has its task code. See include/battle/eft_u.h and eft_v.h. Drawing only: nothing here reads or writes a fighter, a battle
 * object or a hit record; every random value comes from libc rand().
 */

#define RAND_MAX_F 2147483647.0f

#define V(p) ((Vec4 *)(p))
#define M(p) ((Mtx44 *)(p))

extern EftUPtclMgr *gEftPtcl;

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 EftMath_WrapAngle(f32 angle);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern void Vec4_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi); /* clamps each component */
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, Vec4 *base, f32 s); /* dst = base + dir * s */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);   /* rotate about Z */
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);   /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);   /* rotate about Y */
extern u64 EftVram_AddImage(EftUPtclTex *tex, s32 a, s32 b);       /* uploads the image, returns its TEX0 */
extern u64 EftVram_AddClut(EftUPtclTex *tex);                     /* uploads the palette, returns its block */

/* Copies entries a (image) and b (palette) of a texture set into the work and remembers b as the cache slot. */
void EftPtcl_PickTexture(EftUPtclWork *w, EftUPtclTex *tex, s32 a, s32 b) {
    w->texA = tex[a];
    w->texB = tex[b];
    w->arg.texIdx = b;
}

/* The emitter's TEX0 for this frame: uploads image and palette the first time the set's entry is used and
   caches the value in the set; later emitters of the frame take the cached value. */
void EftPtcl_UploadTexture(EftUPtclWork *w, EftUPtclWork *w2) {
    if (!(w2->arg.res->uploaded & (1U << w2->arg.texIdx))) {
        w->tex0 = EftVram_AddImage(&w->texA, 1, 0);
        w->tex0 |= (u64)EftVram_AddClut(&w->texB) << 37;
        w2->arg.res->tex[w2->arg.texIdx].tex0 = w->tex0;
        w2->arg.res->uploaded |= 1U << w2->arg.texIdx;
    } else {
        w->tex0 = w2->arg.res->tex[w2->arg.texIdx].tex0;
    }
}

/* Sets the emitter's current values to key `key` of its definition: seconds become frames, half turns radians,
   {low, high} pairs become {low, range}. */
void EftPtcl_SetKey(EftUPtclCur *cur, EftUPtclWork *w, s32 key) {
    EftUVec a;
    EftUVec b;
    EftUVec c;
    EftUPtclDef2 *def2 = w->arg.def2;
    EftUPtclDef *def = w->arg.def;
    s32 i;

    Vec4_Copy(V(&cur->color), V(&def2->color[key]));
    Vec4_Copy(V(&cur->colorRange), V(&def2->colorRange[key]));
    Vec4_Copy(V(&cur->colorEnd), V(&def2->colorEnd[key]));
    Vec4_Copy(V(&cur->colorEndRange), V(&def2->colorEndRange[key]));
    for (i = 0; i < 2; i++) {
        a.v[i] = def2->pulseR[key][i];
        b.v[i] = def2->pulseG[key][i];
        c.v[i] = def2->pulseB[key][i];
    }
    cur->pulseLo[0] = a.v[0];
    cur->pulseLo[1] = b.v[0];
    cur->pulseLo[2] = c.v[0];
    cur->pulseRange[0] = a.v[1] - a.v[0];
    cur->pulseRange[1] = b.v[1] - b.v[0];
    cur->pulseRange[2] = c.v[1] - c.v[0];
    cur->pulseTime = def2->pulseTime[key] * 30.0f;
    cur->count = def->count[key];
    cur->countRange = def->countRange[key];
    cur->interval = def->interval[key] * 30.0f;
    cur->life = def->life[key] * 30.0f;
    cur->lifeRange = def->lifeRange[key] * 30.0f;
    cur->split = def->split[key];
    for (i = 0; i < 3; i++) {
        cur->speed[i] = def->speed[key][i];
        cur->speedRange[i] = def->speedRange[key][i];
        cur->width[i] = def->width[key][i];
        cur->widthRange[i] = def->widthRange[key][i];
        cur->height[i] = def->height[key][i];
        cur->heightRange[i] = def->heightRange[key][i];
    }
    a.v[0] = def->widthPulseLo[key];
    a.v[1] = def->widthPulseHi[key];
    cur->widthPulseLo = a.v[0];
    cur->widthPulseRange = a.v[1] - a.v[0];
    cur->widthPulseTime = def->widthPulseTime[key] * 30.0f;
    a.v[0] = def->heightPulseLo[key];
    a.v[1] = def->heightPulseHi[key];
    cur->heightPulseLo = a.v[0];
    cur->heightPulseRange = a.v[1] - a.v[0];
    cur->heightPulseTime = def->heightPulseTime[key] * 30.0f;
    cur->fadeIn = def->fadeIn[key] * 30.0f;
    cur->fadeOut = def->fadeOut[key] * 30.0f;
    cur->drift = def->drift[key];
    cur->gravity = def->gravity[key];
    cur->radius = def->radius[key];
    cur->radiusRange = def->radiusRange[key];
    cur->ring = def->ring[key];
    cur->ringRange = def->ringRange[key];
    cur->along = def->along[key];
    cur->coneA = def->coneA[key] * 3.14159265f;
    cur->coneB = def->coneB[key] * 3.14159265f;
    cur->roll = def->roll[key] * 3.14159265f;
    cur->rollRange = def->rollRange[key] * 3.14159265f;
    cur->startDist = def->startDist[key];
    cur->startDistRange = def->startDistRange[key];
    cur->flatten = def->flatten[key];
    cur->rot = def->rot[key] * 3.14159265f;
    cur->rotRange = def->rotRange[key] * 3.14159265f;
    cur->spin = def->spin[key] * 3.14159265f;
    cur->spinRange = def->spinRange[key] * 3.14159265f;
    cur->spin2 = def->spin2[key] * 3.14159265f;
    cur->spin2Range = def->spin2Range[key] * 3.14159265f;
}

/* Blends two keys of the definition into the current values by the key time: keys 0 and 1 before the split,
   keys 1 and 2 after it. */
void EftPtcl_BlendKeys(EftUPtclWork *w) {
    EftUVec d;
    EftUVec a;
    EftUVec b;
    EftUVec c;
    s32 k0;
    s32 k1;
    EftUPtclCur *cur = &w->cur;
    EftUPtclDef *def = w->arg.def;
    EftUPtclDef2 *def2 = w->arg.def2;
    f32 t;
    s32 i;

    if (w->keyTime < w->keySplit) {
        t = w->keyTime / w->keySplit;
        k0 = 0;
        k1 = 1;
    } else {
        t = w->keyTime - w->keySplit;
        t /= w->keyDur - w->keySplit;
        k0 = 1;
        k1 = 2;
    }
    Vec4_Sub(V(&d), V(&def2->color[k1]), V(&def2->color[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&cur->color), V(&def2->color[k0]), V(&d));
    Vec4_Sub(V(&d), V(&def2->colorRange[k1]), V(&def2->colorRange[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&cur->colorRange), V(&def2->colorRange[k0]), V(&d));
    Vec4_Sub(V(&d), V(&def2->colorEnd[k1]), V(&def2->colorEnd[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&cur->colorEnd), V(&def2->colorEnd[k0]), V(&d));
    Vec4_Sub(V(&d), V(&def2->colorEndRange[k1]), V(&def2->colorEndRange[k0]));
    Vec4_Scale(V(&d), V(&d), t);
    Vec4_Add(V(&cur->colorEndRange), V(&def2->colorEndRange[k0]), V(&d));
    for (i = 0; i < 2; i++) {
        d.v[0] = def2->pulseR[k1][i] - def2->pulseR[k0][i];
        d.v[1] = def2->pulseG[k1][i] - def2->pulseG[k0][i];
        d.v[2] = def2->pulseB[k1][i] - def2->pulseB[k0][i];
        a.v[i] = def2->pulseR[k0][i] + d.v[0] * t;
        b.v[i] = def2->pulseG[k0][i] + d.v[1] * t;
        c.v[i] = def2->pulseB[k0][i] + d.v[2] * t;
    }
    cur->pulseLo[0] = a.v[0];
    cur->pulseLo[1] = b.v[0];
    cur->pulseLo[2] = c.v[0];
    cur->pulseRange[0] = a.v[1] - a.v[0];
    cur->pulseRange[1] = b.v[1] - b.v[0];
    cur->pulseRange[2] = c.v[1] - c.v[0];
    d.v[0] = def2->pulseTime[k1] - def2->pulseTime[k0];
    cur->pulseTime = (def2->pulseTime[k0] + d.v[0] * t) * 30.0f;
    d.v[0] = def->count[k1] - def->count[k0];
    d.v[1] = def->countRange[k1] - def->countRange[k0];
    cur->count = def->count[k0] + d.v[0] * t;
    cur->countRange = def->countRange[k0] + d.v[1] * t;
    d.v[0] = def->interval[k1] - def->interval[k0];
    cur->interval = (def->interval[k0] + d.v[0] * t) * 30.0f;
    d.v[0] = def->life[k1] - def->life[k0];
    d.v[1] = def->lifeRange[k1] - def->lifeRange[k0];
    d.v[2] = def->split[k1] - def->split[k0];
    cur->life = (def->life[k0] + d.v[0] * t) * 30.0f;
    cur->lifeRange = (def->lifeRange[k0] + d.v[1] * t) * 30.0f;
    cur->split = def->split[k0] + d.v[2] * t;
    for (i = 0; i < 3; i++) {
        d.v[0] = def->speed[k1][i] - def->speed[k0][i];
        d.v[1] = def->speedRange[k1][i] - def->speedRange[k0][i];
        cur->speed[i] = def->speed[k0][i] + d.v[0] * t;
        cur->speedRange[i] = def->speedRange[k0][i] + d.v[1] * t;
        d.v[0] = def->width[k1][i] - def->width[k0][i];
        d.v[1] = def->widthRange[k1][i] - def->widthRange[k0][i];
        cur->width[i] = def->width[k0][i] + d.v[0] * t;
        cur->widthRange[i] = def->widthRange[k0][i] + d.v[1] * t;
        d.v[0] = def->height[k1][i] - def->height[k0][i];
        d.v[1] = def->heightRange[k1][i] - def->heightRange[k0][i];
        cur->height[i] = def->height[k0][i] + d.v[0] * t;
        cur->heightRange[i] = def->heightRange[k0][i] + d.v[1] * t;
    }
    d.v[0] = def->widthPulseLo[k1] - def->widthPulseLo[k0];
    d.v[1] = def->widthPulseHi[k1] - def->widthPulseHi[k0];
    d.v[2] = def->widthPulseTime[k1] - def->widthPulseTime[k0];
    a.v[0] = def->widthPulseLo[k0] + d.v[0] * t;
    a.v[1] = def->widthPulseHi[k0] + d.v[1] * t;
    cur->widthPulseLo = a.v[0];
    cur->widthPulseRange = a.v[1] - a.v[0];
    cur->widthPulseTime = (def->widthPulseTime[k0] + d.v[2] * t) * 30.0f;
    d.v[0] = def->heightPulseLo[k1] - def->heightPulseLo[k0];
    d.v[1] = def->heightPulseHi[k1] - def->heightPulseHi[k0];
    d.v[2] = def->heightPulseTime[k1] - def->heightPulseTime[k0];
    a.v[0] = def->heightPulseLo[k0] + d.v[0] * t;
    a.v[1] = def->heightPulseHi[k0] + d.v[1] * t;
    cur->heightPulseLo = a.v[0];
    cur->heightPulseRange = a.v[1] - a.v[0];
    cur->heightPulseTime = (def->heightPulseTime[k0] + d.v[2] * t) * 30.0f;
    d.v[0] = def->fadeIn[k1] - def->fadeIn[k0];
    d.v[1] = def->fadeOut[k1] - def->fadeOut[k0];
    cur->fadeIn = (def->fadeIn[k0] + d.v[0] * t) * 30.0f;
    cur->fadeOut = (def->fadeOut[k0] + d.v[1] * t) * 30.0f;
    d.v[0] = def->drift[k1] - def->drift[k0];
    d.v[1] = def->gravity[k1] - def->gravity[k0];
    cur->drift = def->drift[k0] + d.v[0] * t;
    cur->gravity = def->gravity[k0] + d.v[1] * t;
    d.v[0] = def->radius[k1] - def->radius[k0];
    d.v[1] = def->radiusRange[k1] - def->radiusRange[k0];
    cur->radius = def->radius[k0] + d.v[0] * t;
    cur->radiusRange = def->radiusRange[k0] + d.v[1] * t;
    d.v[0] = def->ring[k1] - def->ring[k0];
    d.v[1] = def->ringRange[k1] - def->ringRange[k0];
    cur->ring = def->ring[k0] + d.v[0] * t;
    cur->ringRange = def->ringRange[k0] + d.v[1] * t;
    d.v[0] = def->along[k1] - def->along[k0];
    cur->along = def->along[k0] + d.v[0] * t;
    d.v[0] = def->coneA[k1] - def->coneA[k0];
    d.v[1] = def->coneB[k1] - def->coneB[k0];
    cur->coneA = (def->coneA[k0] + d.v[0] * t) * 3.14159265f;
    cur->coneB = (def->coneB[k0] + d.v[1] * t) * 3.14159265f;
    d.v[0] = def->roll[k1] - def->roll[k0];
    d.v[1] = def->rollRange[k1] - def->rollRange[k0];
    cur->roll = (def->roll[k0] + d.v[0] * t) * 3.14159265f;
    cur->rollRange = (def->rollRange[k0] + d.v[1] * t) * 3.14159265f;
    d.v[0] = def->startDist[k1] - def->startDist[k0];
    d.v[1] = def->startDistRange[k1] - def->startDistRange[k0];
    cur->startDist = def->startDist[k0] + d.v[0] * t;
    cur->startDistRange = def->startDistRange[k0] + d.v[1] * t;
    d.v[0] = def->flatten[k1] - def->flatten[k0];
    cur->flatten = def->flatten[k0] + d.v[0] * t;
    d.v[0] = def->rot[k1] - def->rot[k0];
    d.v[1] = def->rotRange[k1] - def->rotRange[k0];
    cur->rot = (def->rot[k0] + d.v[0] * t) * 3.14159265f;
    cur->rotRange = (def->rotRange[k0] + d.v[1] * t) * 3.14159265f;
    d.v[0] = def->spin[k1] - def->spin[k0];
    d.v[1] = def->spinRange[k1] - def->spinRange[k0];
    cur->spin = (def->spin[k0] + d.v[0] * t) * 3.14159265f;
    cur->spinRange = (def->spinRange[k0] + d.v[1] * t) * 3.14159265f;
    d.v[0] = def->spin2[k1] - def->spin2[k0];
    d.v[1] = def->spin2Range[k1] - def->spin2Range[k0];
    cur->spin2 = (def->spin2[k0] + d.v[0] * t) * 3.14159265f;
    cur->spin2Range = (def->spin2Range[k0] + d.v[1] * t) * 3.14159265f;
}

/* Returns a zeroed free particle linked at the tail of the emitter's list, or NULL when the pool of 500 is
   full. The search starts behind the last particle handed out. */
EftUPtcl *EftPtcl_AllocPtcl(EftUPtclWork *w) {
    EftUPtcl *p = NULL;
    s32 i;
    s32 n;

    i = gEftPtcl->next;
    for (n = 0; n < 500; n++) {
        if (gEftPtcl->pool[i].flags == 0) {
            p = &gEftPtcl->pool[i];
            memset(p, 0, sizeof(EftUPtcl));
            gEftPtcl->next = i + 1;
            if (gEftPtcl->next >= 500) {
                gEftPtcl->next = 0;
            }
            gEftPtcl->used++;
            break;
        }
        i++;
        if (i >= 500) {
            i = 0;
        }
    }
    if (p != NULL) {
        if (w->head == NULL) {
            w->head = p;
        }
        if (w->tail == NULL) {
            w->tail = p;
        } else {
            p->prev = w->tail;
            w->tail->next = p;
            w->tail = p;
        }
    }
    return p;
}

/* Takes a particle out of the emitter's list. */
void EftPtcl_UnlinkPtcl(EftUPtclWork *w, EftUPtcl *p) {
    if (p->prev != NULL) {
        p->prev->next = p->next;
    } else {
        w->head = p->next;
    }
    if (p->next != NULL) {
        p->next->prev = p->prev;
    } else {
        w->tail = p->prev;
    }
    gEftPtcl->used--;
}

/* Frees every particle of the emitter. */
void EftPtcl_FreePtcls(EftUPtclWork *w) {
    EftUPtcl *p;
    EftUPtcl *next;

    p = w->head;
    if (p != NULL) {
        do {
            p->flags = 0;
            gEftPtcl->used--;
            next = p->next;
            p = next;
        } while (next != NULL);
    }
    w->head = NULL;
    w->tail = NULL;
}

/* Creates `count` particles from the emitter's current values: life, spin, the three stages of speed, width and
   height, a direction inside the emitter's cone, a start offset and a drift, start and end colours and the fade
   times, each with a libc random part. Returns 0 when the pool runs out. */
s32 EftPtcl_Spawn(EftUPtclWork *w, EftUPtclWork *w2, s32 count) {
    EftUMtx m;
    EftUVec tmp;
    EftUVec step;
    EftUPtclCur *cur = &w->cur;
    EftUPtclDef *def = w2->arg.def;
    s32 n = count;
    EftUPtcl *p;
    f32 a;
    f32 t;
    f32 d;
    f32 spd;
    f32 k;
    f32 from;
    f32 to;
    f32 life;
    s32 i;

    while (n > 0) {
        p = EftPtcl_AllocPtcl(w);
        if (p == NULL) {
            return 0;
        }
        p->flags |= EFT_UPTCL_ALIVE;
        life = cur->life + cur->lifeRange * ((f32)rand() / RAND_MAX_F);
        p->age = 0.0f;
        p->life = life;
        p->split = p->life * cur->split;
        p->fadeOutAt = p->life - cur->fadeOut;
        if (p->fadeOutAt < p->age) {
            p->fadeOutAt = p->age;
        }
        if (def->flags & 0x40) {
            if (!(rand() & 1)) {
                p->flags |= EFT_UPTCL_REVERSE_SPIN;
            }
        }
        p->rot = cur->rot + cur->rotRange * ((f32)rand() / RAND_MAX_F);
        p->rot = EftMath_WrapAngle(p->rot);
        p->spin = cur->spin + cur->spinRange * ((f32)rand() / RAND_MAX_F);
        p->spin = EftMath_WrapAngle(p->spin);
        if (p->flags & EFT_UPTCL_REVERSE_SPIN) {
            p->spin = -p->spin;
        }
        if (def->flags & 8) {
            p->widthPulseT = 0.0f;
            p->widthPulseTime = cur->widthPulseTime;
            p->widthPulseRange = cur->widthPulseRange;
            if (def->flags & 0x10) {
                p->widthPulseT = cur->widthPulseTime * ((f32)rand() / RAND_MAX_F);
            }
            p->heightPulseT = 0.0f;
            p->heightPulseTime = cur->heightPulseTime;
            p->heightPulseRange = cur->heightPulseRange;
            if (def->flags & 0x10) {
                p->heightPulseT = cur->heightPulseTime * ((f32)rand() / RAND_MAX_F);
            }
        }
        for (i = 0; i < 3; i++) {
            p->speedKey[i] = cur->speed[i] + cur->speedRange[i] * ((f32)rand() / RAND_MAX_F);
            p->widthKey[i] = cur->width[i] + cur->widthRange[i] * ((f32)rand() / RAND_MAX_F);
            p->heightKey[i] = cur->height[i] + cur->heightRange[i] * ((f32)rand() / RAND_MAX_F);
        }
        p->speed = p->speedKey[0];
        p->width = p->widthKey[0];
        p->height = p->heightKey[0];
        if (def->flags & 0x100) {
            Vec4_Set(V(&p->axis), 0.0f, cur->gravity, 0.0f, 1.0f);
            Vec3_Normalize(V(&p->axis), V(&p->axis));
        } else {
            a = (cur->coneA - cur->coneB) * ((f32)rand() / RAND_MAX_F);
            a = EftMath_WrapAngle(a + cur->coneB);
            Vec4_Set(V(&p->axis), 0.0f, 0.0f, 1.0f, 1.0f);
            Mtx_RotateX(M(&m), M(&gEftPtcl->identity), a);
            Mtx_RotateZ(M(&m), M(&m), w->roll);
            Mtx_MulVec4(V(&p->axis), M(&m), V(&p->axis));
            p->axis.x *= cur->flatten + 1.0f;
            Vec3_Normalize(V(&p->axis), V(&p->axis));
            Mtx_MulVec4(V(&p->axis), M(&w->mtx), V(&p->axis));
        }
        a = cur->startDist + cur->startDistRange * ((f32)rand() / RAND_MAX_F);
        Vec3_Scale(V(&p->travel), V(&p->axis), a * w2->arg.size);
        p->travel.w = 1.0f;
        Vec4_Set(V(&p->origin), 0.0f, 0.0f, 0.0f, 1.0f);
        a = (cur->radius + cur->radiusRange * ((f32)rand() / RAND_MAX_F)) * ((f32)rand() / RAND_MAX_F);
        a += cur->ring + cur->ringRange * ((f32)rand() / RAND_MAX_F);
        a *= w2->arg.size;
        switch (def->originKind) {
        case 0:
            Vec4_Set(V(&p->origin), 0.0f, 0.0f, a, 1.0f);
            Mtx_RotateX(M(&m), M(&gEftPtcl->identity), w->ang.x);
            Mtx_RotateY(M(&m), M(&m), w->ang.y);
            Mtx_MulVec4(V(&p->origin), M(&m), V(&p->origin));
            break;
        case 1:
            Vec4_Set(V(&p->origin), 0.0f, 0.0f, a, 1.0f);
            Mtx_RotateY(M(&m), M(&gEftPtcl->identity), w->ang.y);
            Mtx_MulVec4(V(&p->origin), M(&m), V(&p->origin));
            break;
        case 2:
            Vec4_Set(V(&p->origin), 0.0f, a, 0.0f, 1.0f);
            Mtx_RotateY(M(&m), M(&gEftPtcl->identity), w->ang.y);
            Mtx_RotateZ(M(&m), M(&m), w->ang.z);
            Mtx_MulVec4(V(&p->origin), M(&m), V(&p->origin));
            Mtx_MulVec4(V(&p->origin), M(&w->mtx), V(&p->origin));
            Vec3_ScaleAdd(V(&p->origin), V(&w2->arg.dir), V(&p->origin), cur->along * ((f32)rand() / RAND_MAX_F));
            break;
        }
        switch (def->accelKind) {
        case 0:
            Vec3_Sub(V(&p->accel), V(&w2->arg.dir), V(&p->axis));
            p->accel.w = 1.0f;
            Vec3_Normalize(V(&p->accel), V(&p->accel));
            Vec3_Scale(V(&p->accel), V(&p->accel), cur->drift);
            break;
        case 1:
            Vec3_Normalize(V(&tmp), V(&p->origin));
            Vec3_Sub(V(&p->accel), V(&w2->arg.dir), V(&tmp));
            p->accel.w = 1.0f;
            Vec3_Normalize(V(&p->accel), V(&p->accel));
            Vec3_Scale(V(&p->accel), V(&p->accel), cur->drift);
            break;
        case 2:
            Vec3_Normalize(V(&tmp), V(&p->origin));
            Vec3_Add(V(&tmp), V(&tmp), V(&p->axis));
            Vec3_Normalize(V(&tmp), V(&tmp));
            Vec3_Sub(V(&p->accel), V(&w2->arg.dir), V(&tmp));
            p->accel.w = 1.0f;
            Vec3_Normalize(V(&p->accel), V(&p->accel));
            Vec3_Scale(V(&p->accel), V(&p->accel), cur->drift);
            break;
        }
        p->accel.y += cur->gravity;
        Vec4_Set((Vec4 *)&p->u0, 0.0f, 1.0f, 0.0f, 1.0f);
        p->colorStart.x = cur->color.x + cur->colorRange.x * ((f32)rand() / RAND_MAX_F);
        p->colorStart.y = cur->color.y + cur->colorRange.y * ((f32)rand() / RAND_MAX_F);
        p->colorStart.z = cur->color.z + cur->colorRange.z * ((f32)rand() / RAND_MAX_F);
        p->colorStart.w = cur->color.w + cur->colorRange.w * ((f32)rand() / RAND_MAX_F);
        p->colorDelta.x = cur->colorEnd.x + cur->colorEndRange.x * ((f32)rand() / RAND_MAX_F);
        p->colorDelta.y = cur->colorEnd.y + cur->colorEndRange.y * ((f32)rand() / RAND_MAX_F);
        p->colorDelta.z = cur->colorEnd.z + cur->colorEndRange.z * ((f32)rand() / RAND_MAX_F);
        p->colorDelta.w = cur->colorEnd.w + cur->colorEndRange.w * ((f32)rand() / RAND_MAX_F);
        Vec4_Sub(V(&p->colorDelta), V(&p->colorDelta), V(&p->colorStart));
        Vec4_Copy(V(&p->color), V(&p->colorStart));
        p->colorPulseT = 0.0f;
        p->fadeIn = cur->fadeIn;
        if (p->fadeIn <= 0.0f) {
            p->flags |= EFT_UPTCL_FADED_IN;
        }
        p->fadeOut = cur->fadeOut;
        if (p->life < p->fadeOut) {
            p->fadeOut = cur->fadeOut;
        }
        if (p->fadeOut <= 0.0f) {
            p->flags |= EFT_UPTCL_FADED_OUT;
        } else {
            p->fadeOutT = p->fadeOut;
        }
        if (def->reverse == 1) {
            spd = 0.0f;
            d = p->life - p->split;
            for (t = 0.0f; t < p->life; t += 1.0f) {
                Vec3_Scale(V(&step), V(&p->axis), spd);
                Vec3_Add(V(&p->travel), V(&p->travel), V(&step));
                if (t >= p->split) {
                    k = (t - p->split) / d;
                    spd = p->speedKey[1] + (p->speedKey[2] - p->speedKey[1]) * k;
                } else {
                    k = t / p->split;
                    spd = p->speedKey[0] + (p->speedKey[1] - p->speedKey[0]) * k;
                }
                spd *= w2->arg.size;
            }
            Vec3_Scale(V(&p->axis), V(&p->axis), -1.0f);
        }
        w->roll += cur->roll + cur->rollRange * ((f32)rand() / RAND_MAX_F);
        n--;
        w->roll = EftMath_WrapAngle(w->roll);
        Vec3_Add(V(&w->ang), V(&w->ang), V(&w->spin));
        w->ang.x = EftMath_WrapAngle(w->ang.x);
        w->ang.y = EftMath_WrapAngle(w->ang.y);
        w->ang.z = EftMath_WrapAngle(w->ang.z);
        w->count++;
    }
    return 1;
}

/* One frame of every particle of the emitter: spin, the staged speed / width / height, their pulses, the move
   along the (drifting) axis, the sheet frame, colour, fades and the colour pulse; decides whether it is drawn.
   A particle whose life is over is unlinked and freed. */
void EftPtcl_StepPtcls(EftUPtclWork *w, EftUPtclWork *w2) {
    EftUPtclCur *cur = &w->cur;
    EftUPtclDef *def = w2->arg.def;
    EftUPtcl *p;
    EftUPtcl **link;
    EftUPtcl *next;
    f32 t;
    f32 k;
    f32 s;
    s32 frame;

    p = w->head;
    if (p == NULL) {
        return;
    }
    do {
        if (p->age < p->life) {
            t = p->age / p->life;
            if (t > 1.0f) {
                t = 1.0f;
            }
            if (!(p->flags & EFT_UPTCL_STAGE2) && p->age >= p->split) {
                p->flags |= EFT_UPTCL_STAGE2;
                p->spin = cur->spin2 + cur->spin2Range * ((f32)rand() / RAND_MAX_F);
                p->spin = EftMath_WrapAngle(p->spin);
                if (p->flags & EFT_UPTCL_REVERSE_SPIN) {
                    p->spin = -p->spin;
                }
            }
            p->rot += p->spin;
            p->rot = EftMath_WrapAngle(p->rot);
            if (!(p->flags & EFT_UPTCL_STAGE2)) {
                k = p->age / p->split;
                p->speed = p->speedKey[0] + (p->speedKey[1] - p->speedKey[0]) * k;
                p->width = p->widthKey[0] + (p->widthKey[1] - p->widthKey[0]) * k;
                p->height = p->heightKey[0] + (p->heightKey[1] - p->heightKey[0]) * k;
            } else {
                k = (p->age - p->split) / (p->life - p->split);
                p->speed = p->speedKey[1] + (p->speedKey[2] - p->speedKey[1]) * k;
                p->width = p->widthKey[1] + (p->widthKey[2] - p->widthKey[1]) * k;
                p->height = p->heightKey[1] + (p->heightKey[2] - p->heightKey[1]) * k;
            }
            p->speed *= w2->arg.size;
            p->width *= w2->arg.size;
            p->height *= w2->arg.size;
            if (def->flags & 8) {
                if (cur->widthPulseTime > 0.0f) {
                    k = p->widthPulseT / cur->widthPulseTime;
                    s = cur->widthPulseLo + cur->widthPulseRange * k;
                    p->width *= s;
                    if (!(p->flags & EFT_UPTCL_WIDTH_DOWN)) {
                        p->widthPulseT += 1.0f;
                        if (p->widthPulseT >= cur->widthPulseTime) {
                            p->widthPulseT = cur->widthPulseTime;
                            p->flags |= EFT_UPTCL_WIDTH_DOWN;
                        }
                    } else {
                        p->widthPulseT -= 1.0f;
                        if (p->widthPulseT <= 0.0f) {
                            p->widthPulseT = 0.0f;
                            p->flags &= ~EFT_UPTCL_WIDTH_DOWN;
                        }
                    }
                }
                if (cur->heightPulseTime > 0.0f) {
                    k = p->heightPulseT / cur->heightPulseTime;
                    s = cur->heightPulseLo + cur->heightPulseRange * k;
                    p->height *= s;
                    if (!(p->flags & EFT_UPTCL_HEIGHT_DOWN)) {
                        p->heightPulseT += 1.0f;
                        if (p->heightPulseT >= cur->heightPulseTime) {
                            p->heightPulseT = cur->heightPulseTime;
                            p->flags |= EFT_UPTCL_HEIGHT_DOWN;
                        }
                    } else {
                        p->heightPulseT -= 1.0f;
                        if (p->heightPulseT <= 0.0f) {
                            p->heightPulseT = 0.0f;
                            p->flags &= ~EFT_UPTCL_HEIGHT_DOWN;
                        }
                    }
                }
            }
            Vec3_ScaleAdd(V(&p->travel), V(&p->axis), V(&p->travel), p->speed);
            if (!(p->flags & EFT_UPTCL_DETACHED)) {
                Vec3_Add(V(&p->rel), V(&p->travel), V(&p->origin));
                Vec3_Add(V(&p->pos), V(&p->rel), V(&w2->arg.pos));
            } else {
                Vec3_Add(V(&p->pos), V(&p->travel), V(&p->origin));
                Vec3_Add(V(&p->pos), V(&p->pos), V(&p->rel));
            }
            Vec3_Add(V(&p->axis), V(&p->axis), V(&p->accel));
            Vec3_Normalize(V(&p->axis), V(&p->axis));
            if ((w->flags & 0x40) && !(p->flags & EFT_UPTCL_DETACHED)) {
                Vec4_Copy(V(&p->rel), V(&w2->arg.pos));
                p->flags |= EFT_UPTCL_DETACHED;
            }
            if (w->flags & 0x20) {
                k = p->animT / w->animFrames;
                s = p->animT + 1.0f;
                frame = w->frames * k;
                p->u0 = (frame % def->cols) * w->du;
                p->u1 = p->u0 + w->du;
                p->v0 = (frame / def->cols) * w->dv;
                p->v1 = p->v0 + w->dv;
                p->animT = s;
                if (s >= w->animFrames) {
                    p->animT = s - w->animFrames;
                }
            }
            if (w->flags & 0x200) {
                Vec4_Scale(V(&p->color), V(&p->colorDelta), t);
                Vec4_Add(V(&p->color), V(&p->color), V(&p->colorStart));
            } else {
                Vec4_Copy(V(&p->color), V(&p->colorStart));
            }
            Vec4_Clamp(V(&p->color), V(&p->color), 0.0f, 255.0f);
            if (!(p->flags & EFT_UPTCL_FADED_IN)) {
                k = p->fadeInT / p->fadeIn;
                p->fadeInT += 1.0f;
                if (p->fadeInT >= p->fadeIn) {
                    p->flags |= EFT_UPTCL_FADED_IN;
                } else {
                    p->color.w *= k;
                }
            }
            if (p->age >= p->fadeOutAt) {
                if (!(p->flags & EFT_UPTCL_FADED_OUT)) {
                    k = p->fadeOutT / p->fadeOut;
                    p->fadeOutT -= 1.0f;
                    if (p->fadeOutT <= 0.0f) {
                        p->color.w = 0.0f;
                        p->flags |= EFT_UPTCL_FADED_OUT;
                    } else {
                        p->color.w *= k;
                    }
                } else {
                    p->color.w = 0.0f;
                }
            }
            if (def->flags & 0x20) {
                if (cur->pulseTime > 0.0f) {
                    k = p->colorPulseT / cur->pulseTime;
                    s = cur->pulseLo[0] + cur->pulseRange[0] * k;
                    p->color.x *= s;
                    s = cur->pulseLo[1] + cur->pulseRange[1] * k;
                    p->color.y *= s;
                    s = cur->pulseLo[2] + cur->pulseRange[2] * k;
                    p->color.z *= s;
                    if (!(p->flags & EFT_UPTCL_COLOR_DOWN)) {
                        p->colorPulseT += 1.0f;
                        if (p->colorPulseT >= cur->pulseTime) {
                            p->colorPulseT = cur->pulseTime;
                            p->flags |= EFT_UPTCL_COLOR_DOWN;
                        }
                    } else {
                        p->colorPulseT -= 1.0f;
                        if (p->colorPulseT <= 0.0f) {
                            p->colorPulseT = 0.0f;
                            p->flags &= ~EFT_UPTCL_COLOR_DOWN;
                        }
                    }
                }
            }
            if (p->color.w <= 0.0f || p->width <= 0.0f || p->height <= 0.0f) {
                p->flags &= ~EFT_UPTCL_VISIBLE;
            } else {
                p->flags |= EFT_UPTCL_VISIBLE;
                switch (def->shape) {
                case 0:
                    p->width *= 0.5f;
                    if (def->centred == 1) {
                        p->height *= 0.5f;
                    }
                    break;
                case 1:
                case 2:
                    p->width *= 0.5f;
                    p->height *= 0.5f;
                    if (!(def->flags & 2)) {
                        p->width *= 16.0f;
                        p->height *= 16.0f;
                    }
                    break;
                }
            }
            p->age += 1.0f;
            link = &p->next;
        } else {
            EftPtcl_UnlinkPtcl(w, p);
            p->flags = 0;
            link = &p->next;
            w->count -= 1;
        }
        next = *link;
        p = next;
    } while (next != NULL);
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly eft_v.c), with its own header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
#define gEftPtcl gEftPtcl__p2
#define Vec4_Set Vec4_Set__p2
#define Vec4_Copy Vec4_Copy__p2
#define Vec4_Add Vec4_Add__p2
#define Vec4_Sub Vec4_Sub__p2
#define Vec4_Scale Vec4_Scale__p2
#define Vec3_Add Vec3_Add__p2
#define Vec3_Sub Vec3_Sub__p2
#define Vec3_Scale Vec3_Scale__p2
#define Vec3_Normalize Vec3_Normalize__p2
#define Mtx_MulVec4 Mtx_MulVec4__p2
#define Mtx_RotateX func_00120398__p2
#define Mtx_RotateY func_00120428__p2
#define Vec3_ScaleAdd func_001225D0__p2
#define EftPtcl_PickTexture EftPtcl_PickTexture__p2
#define EftPtcl_UploadTexture EftPtcl_UploadTexture__p2
#define EftPtcl_SetKey EftPtcl_SetKey__p2
#define EftPtcl_BlendKeys EftPtcl_BlendKeys__p2
#define EftPtcl_FreePtcls EftPtcl_FreePtcls__p2
#define EftPtcl_Spawn EftPtcl_Spawn__p2
#define EftPtcl_StepPtcls EftPtcl_StepPtcls__p2
#include "battle/eft_v_ext.h"
#undef gEftPtcl
#undef Vec4_Set
#undef Vec4_Copy
#undef Vec4_Add
#undef Vec4_Sub
#undef Vec4_Scale
#undef Vec3_Add
#undef Vec3_Sub
#undef Vec3_Scale
#undef Vec3_Normalize
#undef Mtx_MulVec4
#undef Mtx_RotateX
#undef Mtx_RotateY
#undef Vec3_ScaleAdd
#undef EftPtcl_PickTexture
#undef EftPtcl_UploadTexture
#undef EftPtcl_SetKey
#undef EftPtcl_BlendKeys
#undef EftPtcl_FreePtcls
#undef EftPtcl_Spawn
#undef EftPtcl_StepPtcls
#ifdef PORT /* assigned to: a cast is not an lvalue for a modern compiler */
#define gEftPtcl (*(EftPtclMgr **)&gEftPtcl)
#else
#define gEftPtcl ((EftPtclMgr *)gEftPtcl)
#endif
#define Vec4_Set ((void (*)(EftVVec *dst, f32 x, f32 y, f32 z, f32 w))Vec4_Set)
#define Vec4_Copy ((void (*)(EftVVec *dst, EftVVec *src))Vec4_Copy)
#define Vec4_Add ((void (*)(EftVVec *dst, EftVVec *a, EftVVec *b))Vec4_Add)
#define Vec4_Sub ((void (*)(EftVVec *dst, EftVVec *a, EftVVec *b))Vec4_Sub)
#define Vec4_Scale ((void (*)(EftVVec *dst, EftVVec *src, f32 s))Vec4_Scale)
#define Vec3_Add ((void (*)(EftVVec *dst, EftVVec *a, EftVVec *b))Vec3_Add)
#define Vec3_Sub ((void (*)(EftVVec *dst, EftVVec *a, EftVVec *b))Vec3_Sub)
#define Vec3_Scale ((void (*)(EftVVec *dst, EftVVec *src, f32 s))Vec3_Scale)
#define Vec3_Normalize ((void (*)(EftVVec *dst, EftVVec *src))Vec3_Normalize)
#define Mtx_MulVec4 ((void (*)(EftVVec *dst, EftVMtx *m, EftVVec *src))Mtx_MulVec4)
#define Mtx_RotateX ((void (*)(EftVMtx *dst, EftVMtx *src, f32 angle))Mtx_RotateX)
#define Mtx_RotateY ((void (*)(EftVMtx *dst, EftVMtx *src, f32 angle))Mtx_RotateY)
#define Vec3_ScaleAdd ((void (*)(EftVVec *dst, EftVVec *dir, EftVVec *base, f32 s))Vec3_ScaleAdd)
#define EftPtcl_PickTexture ((void (*)(EftPtclWork *w, void *res, s32 a, s32 b))EftPtcl_PickTexture)
#define EftPtcl_UploadTexture ((void (*)(EftPtclWork *w, EftPtclWork *w2))EftPtcl_UploadTexture)
#define EftPtcl_SetKey ((void (*)(EftPtclCur *cur, EftPtclWork *w, s32 key))EftPtcl_SetKey)
#define EftPtcl_BlendKeys ((void (*)(EftPtclWork *w))EftPtcl_BlendKeys)
#define EftPtcl_FreePtcls ((void (*)(EftPtclWork *w))EftPtcl_FreePtcls)
#define EftPtcl_Spawn ((void (*)(EftPtclWork *w, EftPtclWork *w2, s32 count))EftPtcl_Spawn)
#define EftPtcl_StepPtcls ((void (*)(EftPtclWork *w, EftPtclWork *w2))EftPtcl_StepPtcls)

/*
 * Effect code 0x1853C8..0x1871A8: the tail of the particle emitter module, effect pack part kind 5 (see
 * include/battle/eft_v.h). The head of the module, 0x182CE8..0x1853C8, is the SAME translation unit:
 * EftPtcl_SetTexture only matches when EftPtcl_PickTexture is defined above it in the same file.
 *
 * Drawing only. Nothing here creates, moves or reads a hit record, calls a fighter, stage or camera function that
 * changes state, or raises a battle event. Read from the simulation: BtlScene_IsEffectStopped / IsEffectHidden /
 * IsCharInView. Random numbers: the C library rand() (EftPtcl_Emit, EftPtcl_Init), appearance only.
 */

/* Emits particles while the emission timer is due: count + random range each time, limited by the free pool. */
void EftPtcl_Emit(EftPtclWork *w, EftPtclWork *w2) {
    EftPtclCur *cur = &w->cur;
    s32 n = 0;

    while (w->emitTimer <= 0.0f) {
        s32 free = EFT_PTCL_MAX - gEftPtcl->used;
        s32 any = 0;

        if (free > 0) {
            n = cur->count;
            if (cur->countRange > 0.0f) {
                n = n + cur->countRange * EFTV_RAND01();
            }
            any = 1;
            if (free < n) {
                n = free;
            }
        }
        if (any) {
            EftPtcl_Spawn(w, w2, n);
        }
        w->emitTimer += cur->interval;
        if (!any) {
            break;
        }
    }
}

/* Draws the emitter's particles as square sprites (size = height). */
void EftPtcl_DrawSquares(EftPtclWork *w, EftPtclWork *w2) {
    EftPtclDef *def = w2->arg.def;
    EftPtcl **list = &w->head;
    EftPtcl *p;

    if (def->flags & 2) {
        for (p = *list; p != NULL; p = p->next) {
            if (p->flags & 0x400) {
                EftPrim_DrawQuadDepth(&p->pos, &p->color, def->layer, (w->flags >> 4) & 1, w->tex0, p->height,
                                      p->height, p->u0, p->v0, p->u1, p->v1, p->rot);
            }
        }
    } else {
        while (*list != NULL) {
            p = *list;
            if (p->flags & 0x400) {
                EftGfx_DrawSprite(&p->pos, &p->color, p->height, p->height, p->u0, p->v0, p->u1,
                                  p->v1, p->rot, def->layer, (w->flags >> 4) & 1, w->tex0);
            }
            list = &p->next;
        }
    }
}

/* Draws the emitter's particles as width x height sprites. */
void EftPtcl_DrawSprites(EftPtclWork *w, EftPtclWork *w2) {
    EftPtclDef *def = w2->arg.def;
    EftPtcl **list = &w->head;
    EftPtcl *p;

    if (def->flags & 2) {
        for (p = *list; p != NULL; p = p->next) {
            if (p->flags & 0x400) {
                EftPrim_DrawQuadDepth(&p->pos, &p->color, def->layer, (w->flags >> 4) & 1, w->tex0, p->width,
                                      p->height, p->u0, p->v0, p->u1, p->v1, p->rot);
            }
        }
    } else {
        while (*list != NULL) {
            p = *list;
            if (p->flags & 0x400) {
                EftGfx_DrawSprite(&p->pos, &p->color, p->width, p->height, p->u0, p->v0, p->u1,
                                  p->v1, p->rot, def->layer, (w->flags >> 4) & 1, w->tex0);
            }
            list = &p->next;
        }
    }
}

/* Draws the particles as quads that lie along each particle's axis and face the camera, one packet each. */
/* The header arms each end in their own `pkt->next = NULL` (with one common store behind the if / else the
   store shares a basic block with `i = 0` and the hoisted 1.0 of the texture loop, and the second scheduling pass
   then issues `i = 0` in front of the 1.0).
   FAKE MATCH: the packet pointer is read through a volatile alias of gOtCur (as in eft_s.c). The original has
   `beqzl v0, <loop end + 4>` with the load of p->next in the slot after the projection call; a plain read gives
   `beqz` with the load of gOtCur in the slot. Analysis (cleanup round 4): the branch is predicted 40 %, so the
   delay-slot pass first tries the fall-through instruction, which it may take only when s0 is dead at the loop
   end. In the original the pass had no block for that label (the label behind the last barrier in front of it,
   the start of the `gOtZ[z]` arm, was not a block head any more: something the last jump pass rewrote), assumed
   every register live there and fell back to the annulled copy of the target's first instruction. The flag test
   at the top of the loop is consistent with that (its slot instruction sets v0, which the loop end sets first).
   No natural source form that makes the last jump pass replace that label was found (tried: `continue` forms,
   the link assignment in every arm, the chain tail in every arm, unsigned compares of the result). */
extern u32 *volatile gOtCurRead __asm__("gOtCur");
void EftPtcl_DrawAxisQuads(EftPtclWork *w, EftPtclWork *w2) {
    EftVVec quad[4];
    EftVVec cam;
    EftVVec side;
    EftVVec uv[4];
    EftVVec st[4];
    EftVVec half;
    EftVVec len;
    EftVIVec scr[4];
    EftVIVec col;
    EftPtclDef *def = w2->arg.def;
    EftPtcl **link;
    EftPtcl *p;
    EftVStripPkt *pkt;
    OtEntry *e;
    s32 i;
    s32 z;
    s32 layer;

    Vec4_Copy(&cam, &gBtlCamView->pos);
    for (i = 0; i < 4; i++) {
        s32 o = i * sizeof(EftVVec);

        EFTV_AT(&uv[0].z, o) = 1.0f;
        EFTV_AT(&uv[0].w, o) = 0.0f;
        Vec4_Set(&quad[i], 0.0f, 0.0f, 0.0f, 1.0f);
    }
    link = &w->head;
    while (*link != NULL) {
        p = *link;
        if (p->flags & 0x400) {
            Vec3_Sub(&side, &p->pos, &cam);
            Vec3_Cross(&side, &p->axis, &side);
            Vec3_Normalize(&side, &side);
            Vec3_Scale(&half, &side, p->width);
            if (def->flags & 0x80) {
                Vec3_Add(&quad[0], &p->pos, &half);
                Vec3_Sub(&quad[1], &p->pos, &half);
                Vec3_Scale(&len, &p->axis, p->height);
                if (def->centred == 0) {
                    Vec3_Add(&quad[2], &quad[0], &len);
                    Vec3_Add(&quad[3], &quad[1], &len);
                } else {
                    Vec3_Add(&quad[2], &quad[0], &len);
                    Vec3_Add(&quad[3], &quad[1], &len);
                    Vec3_Sub(&quad[0], &quad[0], &len);
                    Vec3_Sub(&quad[1], &quad[1], &len);
                }
            } else {
                Vec3_Add(&quad[2], &p->pos, &half);
                Vec3_Sub(&quad[3], &p->pos, &half);
                Vec3_Scale(&len, &p->axis, p->height);
                if (def->centred == 0) {
                    Vec3_Add(&quad[0], &quad[2], &len);
                    Vec3_Add(&quad[1], &quad[3], &len);
                } else {
                    Vec3_Add(&quad[0], &quad[2], &len);
                    Vec3_Add(&quad[1], &quad[3], &len);
                    Vec3_Sub(&quad[2], &quad[2], &len);
                    Vec3_Sub(&quad[3], &quad[3], &len);
                }
            }
            if (Vu0Cur_ProjectPoints(scr, quad, 4)) {
                pkt = (EftVStripPkt *)gOtCurRead;
                gOtCur = (u32 *)(pkt + 1);
                if (pkt == NULL) {
                    return;
                }
                if (def->layer < 2) {
                    pkt->prim = 0x5C;
                    pkt->dmaTag = 0x20000008;
                    pkt->vif0 = 0x10000000;
                    pkt->vif1 = 0x50000008;
                    pkt->gifTag = 0xE400000000008001;
                    pkt->regs = 0x42142142142160;
                    pkt->next = NULL;
                } else {
                    pkt->prim = 0x25C;
                    pkt->dmaTag = 0x20000008;
                    pkt->vif0 = 0x10000000;
                    pkt->vif1 = 0x50000008;
                    pkt->gifTag = 0xE400000000008001;
                    pkt->regs = 0x42142142142170;
                    pkt->next = NULL;
                }
                for (i = 0; i < 4; i++) {
                    s32 o = i * sizeof(EftVVec);
                    f32 q = 1.0f / *(s32 *)((u8 *)&scr[0].w + o);

                    EFTV_AT(&uv[0].x, o) = (i % 2) * w->du + p->u0;
                    EFTV_AT(&uv[0].y, o) = (i / 2) * w->dv + p->v0;
                    Vec3_Scale((EftVVec *)((u8 *)st + o), (EftVVec *)((u8 *)&uv[0].x + o), q);
                }
                Vec4_ToInt(&col, &p->color);
                z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
                if (w->flags & EFT_PTCL_FRONT) {
                    for (i = 0; i < 4; i++) {
                        s32 o = i * sizeof(EftVIVec);

                        *(s32 *)((u8 *)&scr[0].z + o) = 0xFFFFFF;
                    }
                }
                pkt->rgbaq0.r = col.x;
                pkt->rgbaq0.g = col.y;
                pkt->rgbaq0.b = col.z;
                pkt->rgbaq0.a = col.w;
                pkt->rgbaq0.q = st[0].z;
                pkt->rgbaq1.r = col.x;
                pkt->rgbaq1.g = col.y;
                pkt->rgbaq1.b = col.z;
                pkt->rgbaq1.a = col.w;
                pkt->rgbaq1.q = st[1].z;
                pkt->rgbaq2.r = col.x;
                pkt->rgbaq2.g = col.y;
                pkt->rgbaq2.b = col.z;
                pkt->rgbaq2.a = col.w;
                pkt->rgbaq2.q = st[2].z;
                pkt->rgbaq3.r = col.x;
                pkt->rgbaq3.g = col.y;
                pkt->rgbaq3.b = col.z;
                pkt->rgbaq3.a = col.w;
                pkt->rgbaq3.q = st[3].z;
                pkt->st0.s = st[0].x;
                pkt->st0.t = st[0].y;
                pkt->st1.s = st[1].x;
                pkt->st1.t = st[1].y;
                pkt->st2.s = st[2].x;
                pkt->st2.t = st[2].y;
                pkt->st3.s = st[3].x;
                pkt->st3.t = st[3].y;
                pkt->xyz0.x = scr[0].x;
                pkt->xyz0.y = scr[0].y;
                pkt->xyz0.z = scr[0].z;
                pkt->xyz0.f = 0xFF;
                pkt->xyz1.x = scr[1].x;
                pkt->xyz1.y = scr[1].y;
                pkt->xyz1.z = scr[1].z;
                pkt->xyz1.f = 0xFF;
                pkt->xyz2.x = scr[2].x;
                pkt->xyz2.y = scr[2].y;
                pkt->xyz2.z = scr[2].z;
                pkt->xyz2.f = 0xFF;
                pkt->xyz3.x = scr[3].x;
                pkt->xyz3.y = scr[3].y;
                pkt->xyz3.z = scr[3].z;
                pkt->xyz3.f = 0xFF;
                pkt->tex0 = w->tex0;
                layer = def->layer;
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
                e->tail->next = (OtPrim *)pkt;
                e->tail = (OtPrim *)pkt;
            }
        }
        link = &p->next;
    }
}

/* Same quads through the clipped polygon drawing: two triangles per particle. */
void EftPtcl_DrawAxisPolys(EftPtclWork *w, EftPtclWork *w2) {
    EftVVec quad[4];
    EftVVec cam;
    EftVVec side;
    EftVVec uv[4];
    EftVVec half;
    EftVVec len;
    EftVIVec scr[4];
    EftVVert vert[9];
    EftPtclDef *def = w2->arg.def;
    EftPtcl **link;
    EftPtcl *p;
    s32 i;

    Vec4_Copy(&cam, &gBtlCamView->pos);
    for (i = 0; i < 4; i++) {
        s32 o = i * sizeof(EftVVec);

        EFTV_AT(&uv[0].z, o) = 1.0f;
        EFTV_AT(&uv[0].w, o) = 1.0f;
        Vec4_Set(&quad[i], 0.0f, 0.0f, 0.0f, 1.0f);
    }
    link = &w->head;
    while (*link != NULL) {
        p = *link;
        if (p->flags & 0x400) {
            Vec3_Sub(&side, &p->pos, &cam);
            Vec3_Cross(&side, &p->axis, &side);
            Vec3_Normalize(&side, &side);
            Vec3_Scale(&half, &side, p->width);
            if (def->flags & 0x80) {
                Vec3_Add(&quad[0], &p->pos, &half);
                Vec3_Sub(&quad[1], &p->pos, &half);
                Vec3_Scale(&len, &p->axis, p->height);
                if (def->centred == 0) {
                    Vec3_Add(&quad[2], &quad[0], &len);
                    Vec3_Add(&quad[3], &quad[1], &len);
                } else {
                    Vec3_Add(&quad[2], &quad[0], &len);
                    Vec3_Add(&quad[3], &quad[1], &len);
                    Vec3_Sub(&quad[0], &quad[0], &len);
                    Vec3_Sub(&quad[1], &quad[1], &len);
                }
            } else {
                Vec3_Add(&quad[2], &p->pos, &half);
                Vec3_Sub(&quad[3], &p->pos, &half);
                Vec3_Scale(&len, &p->axis, p->height);
                if (def->centred == 0) {
                    Vec3_Add(&quad[0], &quad[2], &len);
                    Vec3_Add(&quad[1], &quad[3], &len);
                } else {
                    Vec3_Add(&quad[0], &quad[2], &len);
                    Vec3_Add(&quad[1], &quad[3], &len);
                    Vec3_Sub(&quad[2], &quad[2], &len);
                    Vec3_Sub(&quad[3], &quad[3], &len);
                }
            }
            for (i = 0; i < 4; i++) {
                s32 o = i * sizeof(EftVVec);

                EFTV_AT(&uv[0].x, o) = (i % 2) * w->du + p->u0;
                EFTV_AT(&uv[0].y, o) = (i / 2) * w->dv + p->v0;
            }
            Vu0Cur_ProjectPoints(scr, quad, 4);
            for (i = 0; i < 2; i++) {
                ClipVtx_Set(&vert[0], &quad[i], &uv[i], &p->color);
                ClipVtx_Set(&vert[1], &quad[i + 1], &uv[i + 1], &p->color);
                ClipVtx_Set(&vert[2], &quad[i + 2], &uv[i + 2], &p->color);
                EftGfx_DrawPolyScaledZ(vert, def->layer, 0, 0, (w->flags >> 4) & 1, 0, w->tex0, 2.0f);
            }
        }
        link = &p->next;
    }
}

/* Task init: copies the argument block, sets up life, orientation, random angles and the texture sheet. */
void EftPtcl_Init(EftVTask *task, EftPtclArg *arg) {
    EftPtclWork *w = task->work;
    EftPtclDef *def = arg->def;
    EftPtclCur *cur;

    memset(w, 0, sizeof(EftPtclWork));
    cur = &w->cur;
    w->arg = *arg;
    w->flags |= EFT_PTCL_ALIVE;
    switch (def->flip) {
    case 0:
        w->flags |= EFT_PTCL_FLIP;
        break;
    case 1:
        if (!(rand() & 1)) {
            w->flags |= EFT_PTCL_FLIP;
        }
        break;
    case 2:
        break;
    }
    if (arg->life <= 0.0f) {
        w->flags |= EFT_PTCL_ENDLESS;
    } else {
        w->life = arg->life * 30.0f;
    }
    if (def->flags & 1) {
        w->keyDur = def->keyTime * 30.0f;
        w->keySplit = w->keyDur * def->keySplit;
        EftPtcl_SetKey(cur, w, 0);
    } else {
        EftPtcl_SetKey(cur, w, 2);
    }
    Vec3_Clamp(&arg->dir, &arg->dir, -1.0f, 1.0f);
    w->pitch = Mathf_Asin(-arg->dir.y);
    w->yaw = atan2f(arg->dir.x, arg->dir.z);
    Mtx_RotateX(&w->mtx, &gEftPtcl->identity, w->pitch);
    Mtx_RotateY(&w->mtx, &w->mtx, w->yaw);
    w->roll = cur->roll + cur->rollRange * EFTV_RAND01();
    w->roll = EftMath_WrapAngle(w->roll);
    w->ang.x = (def->angX + def->angXRange * EFTV_RAND01()) * 3.14159265f;
    w->ang.x = EftMath_WrapAngle(w->ang.x);
    w->spin.x = (def->spinX + def->spinXRange * EFTV_RAND01()) * 3.14159265f;
    w->spin.x = EftMath_WrapAngle(w->spin.x);
    w->ang.y = (def->angY + def->angYRange * EFTV_RAND01()) * 3.14159265f;
    w->ang.y = EftMath_WrapAngle(w->ang.y);
    w->spin.y = (def->spinY + def->spinYRange * EFTV_RAND01()) * 3.14159265f;
    w->spin.y = EftMath_WrapAngle(w->spin.y);
    w->ang.z = (def->angZ + def->angZRange * EFTV_RAND01()) * 3.14159265f;
    w->ang.z = EftMath_WrapAngle(w->ang.z);
    w->spin.z = (def->spinZ + def->spinZRange * EFTV_RAND01()) * 3.14159265f;
    w->spin.z = EftMath_WrapAngle(w->spin.z);
    if (def->flags & 4) {
        w->frames = def->cols * def->rows;
    }
    if (w->frames >= 2) {
        w->flags |= EFT_PTCL_SHEET;
        w->du = 1.0f / def->cols;
        w->dv = 1.0f / def->rows;
        w->animFrames = def->animTime * w->frames * 30.0f;
        if (w->animFrames < w->frames) {
            w->animFrames = w->frames;
        }
    } else {
        w->du = 1.0f;
        w->dv = 1.0f;
    }
    EftPtcl_PickTexture(w, arg->res, arg->unk24, arg->unk24);
}

/* Task term: returns the particles to the pool. */
void EftPtcl_Term(EftVTask *task) {
    EftPtclWork *w = task->work;

    w->flags = 0;
    EftPtcl_FreePtcls(w);
}

/* Task update: start delay, emission, particle step, key animation, life; ends the task once it is dead. */
void EftPtcl_Update(EftVTask *task) {
    EftPtclWork *w = task->work;
    EftPtclCur *cur = &w->cur;
    EftPtclDef *def = w->arg.def;

    if (!BtlScene_IsEffectStopped(w->arg.objId, w->type)) {
        if (w->flags & EFT_PTCL_STOPPING) {
            if (w->stopDelay <= 0.0f) {
                if (w->flags & EFT_PTCL_LINGER) {
                    w->linger -= 1.0f;
                    if (w->linger <= 0.0f) {
                        w->linger = 0.0f;
                        w->flags |= EFT_PTCL_DEAD;
                    }
                } else if (w->count <= 0) {
                    w->flags |= EFT_PTCL_DEAD;
                }
            }
        }
        if (w->startDelay <= 0.0f) {
            if (w->emitTimer <= 0.0f) {
                if (!(w->flags & EFT_PTCL_STOPPING) || w->stopDelay > 0.0f || w->linger > 0.0f) {
                    EftPtcl_Emit(w, w);
                }
            } else {
                w->emitTimer -= 1.0f;
            }
            EftPtcl_StepPtcls(w, w);
            if ((def->flags & 1) && w->keyTime <= w->keyDur) {
                EftPtcl_BlendKeys(w);
                w->keyTime += 1.0f;
                if (w->keyDur <= w->keyTime) {
                    EftPtcl_SetKey(cur, w, 2);
                }
            }
            if (!(w->flags & EFT_PTCL_ENDLESS)) {
                w->life -= 1.0f;
                if (w->life <= 0.0f) {
                    w->flags |= EFT_PTCL_STOPPING;
                    if (w->lingerInit > 0.0f) {
                        w->flags |= EFT_PTCL_LINGER;
                    }
                }
            }
        } else {
            w->startDelay -= 1.0f;
        }
        if ((w->flags & EFT_PTCL_STOPPING) && w->stopDelay > 0.0f) {
            w->stopDelay -= 1.0f;
        }
    }
    if (w->flags & EFT_PTCL_DEAD) {
        BtlTask_SetDead(task);
    } else {
        EftPtcl_UploadTexture(w, w);
    }
}

/* Task post-update: nothing. */
void EftPtcl_PostUpdate(EftVTask *task) {
}

/* Task reset: ends the task. */
void EftPtcl_Reset(EftVTask *task) {
    BtlTask_SetDead(task);
}

/* Task draw: picks the draw routine for the definition's shape. */
void EftPtcl_Draw(EftVTask *task) {
    EftPtclWork *w = task->work;
    EftPtclDef *def = w->arg.def;

    if (BtlScene_IsEffectHidden(w->arg.objId, w->type)) {
        return;
    }
    if ((w->flags & EFT_PTCL_FRONT) && !BtlScene_IsCharInView(w->arg.objId)) {
        return;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    switch (def->shape) {
    case 0:
        if (def->flags & 2) {
            EftPtcl_DrawAxisPolys(w, w);
        } else {
            EftPtcl_DrawAxisQuads(w, w);
        }
        break;
    case 1:
        EftPtcl_DrawSquares(w, w);
        break;
    case 2:
        EftPtcl_DrawSprites(w, w);
        break;
    }
    Vu0Cur_Pop();
}

/* Manager init: the manager block, the particle pool and the list of 60 emitter tasks. */
void EftPtclMgr_Init(EftVTask *task) {
    gEftPtcl = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftPtclMgr));
    memset(gEftPtcl, 0, sizeof(EftPtclMgr));
    gEftPtcl->pool = BtlPool_Alloc(BtlPool_GetCurrent(), EFT_PTCL_MAX * sizeof(EftPtcl));
    memset(gEftPtcl->pool, 0, EFT_PTCL_MAX * sizeof(EftPtcl));
    Mtx_StoreIdentity(&gEftPtcl->identity);
    gEftPtclList = BtlTask_CreateChildList(task, 60, sizeof(EftPtclWork));
}

/* Manager term: frees the pool and the manager block. */
void EftPtclMgr_Term(EftVTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftPtcl->pool);
    BtlPool_Free(BtlPool_GetCurrent(), gEftPtcl);
    gEftPtcl = NULL;
}

/* Manager update: nothing. */
void EftPtclMgr_Update(EftVTask *task) {
}

/* Manager reset: nothing. */
void EftPtclMgr_Reset(EftVTask *task) {
}

/* Creates an emitter task from an argument block; returns the task (the effect pack's handle). */
EftVTask *EftPtcl_Create(EftPtclArg *arg) {
    EftPtclArg a = *arg;

    return BtlTaskList_AddTail(gEftPtclList, gEftPtclClass, &a);
}

/* Stops the emission (after the stop delay, and with the linger time if one was set). */
s32 EftPtcl_Stop(EftVTask *task) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_PTCL_STOPPING;
    if (w->lingerInit > 0.0f) {
        w->flags |= EFT_PTCL_LINGER;
    }
    return 1;
}

/* Kills the emitter: its task ends on the next update. */
s32 EftPtcl_Kill(EftVTask *task) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_PTCL_DEAD;
    return 1;
}

/* Tells whether the handle is a live emitter. */
s32 EftPtcl_IsAlive(EftVTask *task) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    return 1;
}

/* Moves the emitter. */
s32 EftPtcl_SetPos(EftVTask *task, EftVVec *pos) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    Vec3_Copy(&w->arg.pos, pos);
    return 1;
}

/* Moves the emitter and re-bases the particles it has already emitted. */
s32 EftPtcl_Warp(EftVTask *task, EftVVec *pos) {
    EftPtclWork *w;
    EftPtcl **link;
    EftPtcl *p;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    Vec3_Copy(&w->arg.pos, pos);
    link = &w->head;
    while (*link != NULL) {
        p = *link;
        if (!(p->flags & 0x100)) {
            Vec3_Add(&p->unk20, &p->unk0, &p->unk10);
            Vec3_Add(&p->pos, &p->unk20, &w->arg.pos);
        } else {
            Vec3_Add(&p->pos, &p->unk0, &p->unk10);
            Vec3_Add(&p->pos, &p->pos, &p->unk20);
        }
        link = &p->next;
    }
    return 1;
}

/* Turns the emitter: stores the unit direction and rebuilds its matrix. */
s32 EftPtcl_SetDir(EftVTask *task, EftVVec *dir) {
    EftPtclWork *w;
    EftVVec *d;
    EftVMtx *m;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    d = &w->arg.dir;
    m = &w->mtx;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    Vec3_Normalize(d, dir);
    w->arg.dir.w = 1.0f;
    w->pitch = Mathf_Asin(-w->arg.dir.y);
    w->yaw = atan2f(d->x, w->arg.dir.z);
    Mtx_RotateX(m, &gEftPtcl->identity, w->pitch);
    Mtx_RotateY(m, m, w->yaw);
    return 1;
}

/* Sets the emitter's size factor. */
s32 EftPtcl_SetSize(EftVTask *task, f32 size) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->arg.size = size;
    return 1;
}

/* Sets the frames before the emitter starts. */
s32 EftPtcl_SetStartDelay(EftVTask *task, f32 frames) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->startDelay = frames;
    return 1;
}

/* Sets the frames the emitter keeps emitting after a stop. */
s32 EftPtcl_SetStopDelay(EftVTask *task, f32 frames) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->stopDelay = frames;
    return 1;
}

/* Sets the linger time used once the emitter is stopping. */
s32 EftPtcl_SetLinger(EftVTask *task, f32 frames) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->linger = frames;
    w->lingerInit = frames;
    return 1;
}

/* Re-selects the emitter's texture. */
/* NOT MATCHING: standalone, 2 instructions (a `bne` that is `bnel` here, and the branch after it one instruction
   short). It matches as written once EftPtcl_PickTexture is DEFINED above it in the same file (checked with a stand-in
   definition): the head of this module, 0x182CE8..0x1853C8, is the same translation unit. */
s32 EftPtcl_SetTexture(EftVTask *task, void *res, s32 a, s32 b) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    EftPtcl_PickTexture(w, res, a, b);
    return 1;
}

/* Marks the emitter as drawn in front and only while its fighter is in view. */
s32 EftPtcl_SetFront(EftVTask *task) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_PTCL_FRONT;
    return 1;
}

/* Sets or clears emitter flag 0x40. */
s32 EftPtcl_SetFlag40(EftVTask *task, s32 on) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    if (on) {
        w->flags |= EFT_PTCL_FLAG40;
    } else {
        w->flags &= ~EFT_PTCL_FLAG40;
    }
    return 1;
}

/* Sets or clears emitter flag 0x80. */
s32 EftPtcl_SetFlag80(EftVTask *task, s32 on) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    if (on) {
        w->flags |= EFT_PTCL_FLAG80;
    } else {
        w->flags &= ~EFT_PTCL_FLAG80;
    }
    return 1;
}

/* Stores the effect pack type the stop / hide tests use. */
s32 EftPtcl_SetType(EftVTask *task, s32 type) {
    EftPtclWork *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftPtcl_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_PTCL_ALIVE)) {
        return 0;
    }
    w->type = type;
    return 1;
}
