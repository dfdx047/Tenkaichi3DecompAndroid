#include "common.h"
#include "battle/eft_m.h"

/*
 * Effect tasks, 0x15F728..0x1637A0. See include/battle/eft_m.h.
 */

#define RAND_MAX_F 2147483647.0f
#define RANDF() ((f32)rand() / RAND_MAX_F)

typedef struct EftMBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} EftMBattleWork;

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftMView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
} EftMView;

extern EftMView *gBtlCamView;

#define V(p) ((Vec4 *)(p))

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
extern f32 Mathf_Sin(f32 angle);
extern f32 Mathf_Cos(f32 angle);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Set(Vec4 *dst, f32 x, f32 y, f32 z);  /* sets x, y, z */
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);            /* copies x, y, z */
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec4_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi); /* clamps each component */
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);          /* copy */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);          /* inverse of a rotation + translation matrix */
extern void Vu0Cur_Push(void);                            /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                        /* load the matrix */
extern void Vu0Cur_Pop(void);                            /* pop */
extern void ClipVtx_Set(void *vtx, Vec4 *pos, Vec4 *uv, Vec4 *col);
extern void EftGfx_DrawPolyAvgZFront(void *prim, s32 blend, s32 a2, s32 a3, s32 inView, s32 t1, u64 tex, s32 t3);
extern void EftTexSet_Load4(EftSpdTex *tex, s32 *entry);
extern void EftTexSet_Keep4(EftSpdTex *tex, s32 a, s32 b);
extern EftMBattleWork *Battle_GetWork(void);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 BtlScene_IsCharInView(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern f32 BtlCharApi_GetPartUnk5C(s32 objId, s32 node);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_GetFrameMove(s32 objId, Vec4 *out);
extern void BtlCharApi_GetVelocity(s32 objId, Vec4 *out);
extern void BtlCharApi_GetDir(s32 objId, Vec4 *out);
extern f32 BtlCharApi_GetSpeed(s32 objId);
extern f32 BtlCharApi_GetFallSpeed(s32 objId);
extern s32 BtlCharApi_GetActionFxKind(s32 objId);

/* Spawns `count` streaks in a ring around a model node of the fighter, pointing against `dir`. */
s32 EftSpdLine_SpawnStreakRing(s32 objId, Vec4 *dir, s32 node, s32 count, f32 unused, f32 lifeMin, f32 lifeRange) {
    EftVec pos;
    EftVec tail;
    EftVec back;
    EftVec tmp;
    EftVec up = { 0.0f, -1.0f, 0.0f, 1.0f };
    EftVec side2;
    EftVec side;
    f32 size;
    f32 minSize;
    f32 ang;
    f32 radius;
    f32 dist;
    f32 speed;
    s32 i;

    if (gEftSpdLine == NULL) {
        return 0;
    }
    minSize = 0.8f;
    size = BtlCharApi_GetPartUnk5C(objId, node) * 0.4f;
    if (size < minSize) {
        size = BtlCharApi_GetHeight(objId) * 0.05f * minSize;
    }
    Vec4_Scale(V(&back), dir, -1.0f);
    Vec3_Normalize(V(&back), V(&back));
    Vec3_Cross(V(&side), V(&back), V(&up));
    Vec3_Normalize(V(&side), V(&side));
    Vec3_Cross(V(&side2), V(&back), V(&side));
    Vec3_Normalize(V(&side2), V(&side2));
    for (i = 0; i < count; i++) {
        BtlCharApi_GetNodePos(objId, node, V(&pos));
        ang = RANDF() * 6.2831853f;
        radius = (RANDF() * 1.8f + 0.1f) * size;
        dist = (RANDF() * 2.0f + 1.5f) * size;
        Vec4_Scale(V(&tmp), V(&side), Mathf_Cos(ang) * radius);
        Vec4_Add(V(&pos), V(&pos), V(&tmp));
        Vec4_Scale(V(&tmp), V(&side2), Mathf_Sin(ang) * radius);
        Vec4_Add(V(&pos), V(&pos), V(&tmp));
        Vec4_Scale(V(&tmp), V(&back), dist);
        Vec4_Sub(V(&pos), V(&pos), V(&tmp));
        Vec4_Scale(V(&tmp), V(&back), (RANDF() * 3.0f + 2.0f) * size);
        Vec4_Add(V(&tail), V(&pos), V(&tmp));
        speed = (RANDF() * 2.0f + 0.5f) * (BtlCharApi_GetHeight(objId) * 0.05f);
        EftSpdLine_AddStreak(objId, pos, tail, back, speed, RANDF() * lifeRange + lifeMin, 0.075f);
    }
    return 1;
}

/* Allocates the work once and loads the textures (common entry 0x237). */
void EftSpdLine_Init(void) {
    if (gEftSpdLine == NULL) {
        gEftSpdLine = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftSpdLineWork));
        memset(gEftSpdLine, 0, sizeof(EftSpdLineWork));
    }
    EftTexSet_Load4(gEftSpdLine->tex, BtlScene_GetCommonEntry(0x237));
}

/* Frees the work. */
void EftSpdLine_Term(void) {
    if (gEftSpdLine != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftSpdLine);
        gEftSpdLine = NULL;
    }
}

/* Task update: steps both lists, then the texture. */
void EftSpdLine_Update(void) {
    EftSpdLine_UpdateTrails();
    EftSpdLine_UpdateStreaks();
    EftSpdLine_UpdateTexture(1, 0);
}

/* Empty task slot. */
void EftSpdLine_Stub(void) {
}

/* Task draw: both lists under the current view's world-to-screen matrix. */
void EftSpdLine_Draw(void) {
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    EftSpdLine_DrawTrails();
    EftSpdLine_DrawStreaks();
    Vu0Cur_Pop();
}

/* Returns a zeroed free trail slot, or NULL when all 30 are in use. */
EftSpdTrail *EftSpdLine_AllocTrail(void) {
    EftSpdTrail *p = gEftSpdLine->trails;
    s32 i;

    for (i = 0; i < 30; i++, p++) {
        if (p->flags == 0) {
            memset(p, 0, sizeof(EftSpdTrail));
            return p;
        }
    }
    return NULL;
}

/* Adds a trail at the list tail: random width scaled by the fighter's height, alpha 64, two frames of life. */
void EftSpdLine_AddTrail(s32 objId, EftVec pos, EftVec dir) {
    EftSpdTrail *p = EftSpdLine_AllocTrail();
    s32 r;

    if (p != NULL) {
        if (gEftSpdLine->trailHead == NULL) {
            gEftSpdLine->trailHead = p;
            p->prev = NULL;
            p->next = NULL;
        } else {
            EftSpdTrail *last = gEftSpdLine->trailTail;

            p->next = NULL;
            p->prev = last;
        }
        if (gEftSpdLine->trailTail != NULL) {
            gEftSpdLine->trailTail->next = p;
        }
        gEftSpdLine->trailTail = p;
        p->objId = objId;
        Vec4_Copy(&p->pos, V(&pos));
        Vec4_Copy(&p->tail, V(&pos));
        Vec4_Copy(&p->dir, V(&dir));
        r = rand();
        p->width = ((f32)r / RAND_MAX_F * 0.1f + 0.05f) * (BtlCharApi_GetHeight(objId) * 0.05f);
        p->alpha = 64.0f;
        p->life = 2.0f;
        p->flags = 1;
        p->tex = 0;
        p->blend = 0;
        gEftSpdLine->trailCount++;
    }
}

/* Steps every trail (not while paused) and unlinks the finished ones. */
void EftSpdLine_UpdateTrails(void) {
    EftSpdTrail *p;
    EftSpdTrail *cur;
    s32 done;

    if (!(Battle_GetWork()->flags & 0x100)) {
        p = gEftSpdLine->trailHead;
        while (p != NULL) {
            cur = p;
            done = EftSpdLine_StepTrail(p);
            p = p->next;
            if (done) {
                cur->flags = 0;
                if (cur->prev == NULL) {
                    gEftSpdLine->trailHead = cur->next;
                } else {
                    cur->prev->next = cur->next;
                }
                if (cur->next == NULL) {
                    gEftSpdLine->trailTail = cur->prev;
                } else {
                    cur->next->prev = cur->prev;
                }
                cur->prev = NULL;
                cur->next = NULL;
                gEftSpdLine->trailCount--;
            }
        }
    }
}

/* Moves a trail with its fighter; returns 1 when its life ran out. */
s32 EftSpdLine_StepTrail(EftSpdTrail *p) {
    Vec4 tmp;
    Vec4 move;
    f32 unit;
    s32 r;

    if (p->flags & 1) {
        unit = 0.05f;
        BtlCharApi_GetFrameMove(p->objId, &move);
        Vec4_Scale(&tmp, &move, BtlCharApi_GetHeight(p->objId) * unit * 0.2f);
        Vec4_Add(&p->pos, &p->pos, &tmp);
        r = rand();
        Vec4_Scale(&tmp, &move, -(((f32)r / RAND_MAX_F * 0.8f + 0.8f) * (BtlCharApi_GetHeight(p->objId) * unit)));
        Vec4_Add(&p->tail, &p->pos, &tmp);
        p->life -= 1.0f;
    }
    if (p->life <= 0.0f) {
        p->life = 0.0f;
        return 1;
    }
    return 0;
}

/* Draws every live trail, white with its alpha. */
void EftSpdLine_DrawTrails(void) {
    Mtx44 mtx;
    Vec4 quad[4];
    EftSpdTrail *p;
    s32 inView;

    Mtx_Copy(&mtx, &gBtlCamView->world2screen);
    for (p = gEftSpdLine->trailHead; p != NULL; p = p->next) {
        if (p->flags & 1) {
            inView = BtlScene_IsCharInView(p->objId);
            EftSpdLine_BuildTrailQuad(quad, p);
            EftSpdLine_DrawQuad(quad, &mtx, &gEftSpdLine->tex[p->tex], 255.0f, 255.0f, 255.0f, p->alpha, p->blend,
                                inView);
        }
    }
}

/* Returns a zeroed free streak slot, or NULL when all 40 are in use. */
EftSpdStreak *EftSpdLine_AllocStreak(void) {
    EftSpdStreak *p = gEftSpdLine->streaks;
    s32 i;

    for (i = 0; i < 40; i++, p++) {
        if (p->flags == 0) {
            memset(p, 0, sizeof(EftSpdStreak));
            return p;
        }
    }
    return NULL;
}

/* Adds a streak at the list tail: alpha 160, fading to 0 in `life` seconds. */
void EftSpdLine_AddStreak(s32 objId, EftVec pos, EftVec tail, EftVec dir, f32 speed, f32 life, f32 width) {
    EftSpdStreak *p = EftSpdLine_AllocStreak();

    if (p != NULL) {
        if (gEftSpdLine->streakHead == NULL) {
            gEftSpdLine->streakHead = p;
            p->prev = NULL;
            p->next = NULL;
        } else {
            EftSpdStreak *last = gEftSpdLine->streakTail;

            p->next = NULL;
            p->prev = last;
        }
        if (gEftSpdLine->streakTail != NULL) {
            gEftSpdLine->streakTail->next = p;
        }
        gEftSpdLine->streakTail = p;
        p->objId = objId;
        Vec4_Copy(&p->pos, V(&pos));
        Vec4_Copy(&p->tail, V(&tail));
        Vec4_Copy(&p->dir, V(&dir));
        p->speed = speed;
        p->width = width;
        p->alpha = 160.0f;
        p->flags = 1;
        p->tex = 0;
        p->fade = 160.0f / (life * 30.0f);
        p->blend = 0;
        gEftSpdLine->streakCount++;
    }
}

/* Steps every streak (not while paused) and unlinks the finished ones. */
void EftSpdLine_UpdateStreaks(void) {
    EftSpdStreak *p;
    EftSpdStreak *cur;
    s32 done;

    if (!(Battle_GetWork()->flags & 0x100)) {
        p = gEftSpdLine->streakHead;
        while (p != NULL) {
            cur = p;
            done = EftSpdLine_StepStreak(p);
            p = p->next;
            if (done) {
                cur->flags = 0;
                if (cur->prev == NULL) {
                    gEftSpdLine->streakHead = cur->next;
                } else {
                    cur->prev->next = cur->next;
                }
                if (cur->next == NULL) {
                    gEftSpdLine->streakTail = cur->prev;
                } else {
                    cur->next->prev = cur->prev;
                }
                cur->prev = NULL;
                cur->next = NULL;
                gEftSpdLine->streakCount--;
            }
        }
    }
}

/* Moves a streak along its direction and fades it; returns 1 when it is fully transparent. */
s32 EftSpdLine_StepStreak(EftSpdStreak *p) {
    Vec4 move;

    if (p->flags & 1) {
        Vec4_Scale(&move, &p->dir, p->speed);
        Vec4_Add(&p->pos, &p->pos, &move);
        Vec4_Add(&p->tail, &p->tail, &move);
        p->alpha -= p->fade;
    }
    if (p->alpha <= 0.0f) {
        p->alpha = 0.0f;
        return 1;
    }
    return 0;
}

/* Draws every live streak, black with its alpha, once the texture is ready. */
void EftSpdLine_DrawStreaks(void) {
    Mtx44 mtx;
    Vec4 quad[4];
    EftSpdStreak *p;
    s32 inView;

    if (gEftSpdLine->texReady == 0) {
        return;
    }
    Mtx_Copy(&mtx, &gBtlCamView->world2screen);
    for (p = gEftSpdLine->streakHead; p != NULL; p = p->next) {
        if (p->flags & 1) {
            inView = BtlScene_IsCharInView(p->objId);
            EftSpdLine_BuildStreakQuad(quad, p);
            EftSpdLine_DrawQuad(quad, &mtx, &gEftSpdLine->tex[p->tex], 0.0f, 0.0f, 0.0f, p->alpha, p->blend, inView);
        }
    }
}

/* Corners of a trail: both ends widened across the line of sight from the camera. */
void EftSpdLine_BuildTrailQuad(Vec4 *out, EftSpdTrail *p) {
    Vec4 eye;
    Vec4 toEye;
    Vec4 side;
    Mtx44 inv;
    Vec4 *first = &p->tail;
    Vec4 *out1 = &out[1];
    Vec4 *dir = &p->dir;
    Vec4 *out3 = &out[3];

    Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
    Vec4_Copy(&eye, (Vec4 *)inv.m[3]);
    Vec4_Sub(&toEye, &eye, first);
    Vec3_Normalize(&toEye, &toEye);
    Vec3_Cross(&side, dir, &toEye);
    Vec3_Normalize(&side, &side);
    Vec4_Scale(&side, &side, p->width);
    Vec4_Add(out, first, &side);
    Vec4_Sub(out1, first, &side);
    out1->w = 1.0f;
    out->w = 1.0f;
    Vec4_Sub(&toEye, &eye, &p->pos);
    Vec3_Normalize(&toEye, &toEye);
    Vec3_Cross(&side, dir, &toEye);
    Vec3_Normalize(&side, &side);
    Vec4_Scale(&side, &side, p->width);
    Vec4_Add(&out[2], &p->pos, &side);
    Vec4_Sub(out3, &p->pos, &side);
    out3->w = 1.0f;
    out[2].w = 1.0f;
}

/* Corners of a streak: as for a trail, with half the width on each side. */
void EftSpdLine_BuildStreakQuad(Vec4 *out, EftSpdStreak *p) {
    Vec4 eye;
    Vec4 toEye;
    Vec4 side;
    Mtx44 inv;
    Vec4 *second = &p->tail;
    Vec4 *out1 = &out[1];
    Vec4 *out3 = &out[3];
    Vec4 *dir = &p->dir;

    Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
    Vec4_Copy(&eye, (Vec4 *)inv.m[3]);
    Vec4_Sub(&toEye, &eye, &p->pos);
    Vec3_Normalize(&toEye, &toEye);
    Vec3_Cross(&side, dir, &toEye);
    Vec3_Normalize(&side, &side);
    Vec4_Scale(&side, &side, p->width * 0.5f);
    Vec4_Add(out, &p->pos, &side);
    Vec4_Sub(out1, &p->pos, &side);
    out1->w = 1.0f;
    out->w = 1.0f;
    Vec4_Sub(&toEye, &eye, second);
    Vec3_Normalize(&toEye, &toEye);
    Vec3_Cross(&side, dir, &toEye);
    Vec3_Normalize(&side, &side);
    Vec4_Scale(&side, &side, p->width * 0.5f);
    Vec4_Add(&out[2], second, &side);
    Vec4_Sub(out3, second, &side);
    out3->w = 1.0f;
    out[2].w = 1.0f;
}

/* Sends the four corners as two triangles with one colour; the matrix argument is not used. */
void EftSpdLine_DrawQuad(Vec4 *quad, Mtx44 *mtx, EftSpdTex *tex, f32 r, f32 g, f32 b, f32 a, u8 blend, s32 inView) {
    u8 prim[9][0x30];
    Vec4 pos[4];
    Vec4 uv[4];
    Vec4 col[4];
    s32 i;

    Vec4_Copy(&pos[0], &quad[0]);
    Vec4_Copy(&pos[1], &quad[1]);
    Vec4_Copy(&pos[2], &quad[2]);
    Vec4_Copy(&pos[3], &quad[3]);
    Vec4_Set(&uv[0], 0.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[1], 1.0f, 0.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[2], 0.0f, 1.0f, 1.0f, 0.0f);
    Vec4_Set(&uv[3], 1.0f, 1.0f, 1.0f, 0.0f);
    Vec4_Set(&col[0], r, g, b, a);
    Vec4_Set(&col[1], r, g, b, a);
    Vec4_Set(&col[2], r, g, b, a);
    Vec4_Set(&col[3], r, g, b, a);
    memset(prim, 0, sizeof(prim));
    for (i = 0; i < 2; i++) {
        ClipVtx_Set(prim[0], &pos[i], &uv[i], &col[i]);
        ClipVtx_Set(prim[1], &pos[i + 1], &uv[i + 1], &col[i + 1]);
        ClipVtx_Set(prim[2], &pos[i + 2], &uv[i + 2], &col[i + 2]);
        EftGfx_DrawPolyAvgZFront(prim, blend, 0, 0, inView, 0, tex->tex, 0);
    }
}

/* Keeps the textures referenced while any segment is alive. */
void EftSpdLine_UpdateTexture(s32 a, s32 b) {
    if (gEftSpdLine->trailCount + gEftSpdLine->streakCount != 0) {
        EftTexSet_Keep4(gEftSpdLine->tex, a, b);
        gEftSpdLine->texReady = 1;
    } else {
        gEftSpdLine->texReady = 0;
    }
}

/* ---- aura particles ------------------------------------------------------------------------------------- */

extern void EftSpr_DrawRot(u8 r, u8 g, u8 b, u8 a, f32 x, f32 y, f32 z, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot,
                          s32 t0, s32 t1, s32 w, s32 h, s32 s0, u32 size, s32 s2, s32 s3, void *tex);

/* Drift direction of a spark: against the fighter's velocity, with a random vertical part. */
void EftAura_GetSparkDir(Vec4 *out, s32 objId) {
    Vec4 vel;

    BtlCharApi_GetVelocity(objId, &vel);
    Vec4_Scale(&vel, &vel, gEftAuraPrm->sparkVel);
    Vec4_Set(out, -vel.x, gEftAuraPrm->sparkRise * RANDF(), -vel.z, 1.0f);
    Vec3_Normalize(out, out);
}

/* The same for the alternate aura: straight up / down while the action effect kind is 3 / 4. */
void EftAura_GetSparkDirAlt(Vec4 *out, s32 objId) {
    Vec4 vel;
    s32 kind;

    BtlCharApi_GetVelocity(objId, &vel);
    Vec4_Scale(&vel, &vel, gEftAuraPrm->sparkVelAlt);
    kind = BtlCharApi_GetActionFxKind(objId);
    if (kind == 3) {
        Vec3_Set(out, 0.0f, 1.0f - BtlCharApi_GetFallSpeed(objId), 0.0f);
    } else if (kind == 4) {
        Vec3_Set(out, 0.0f, -1.0f - BtlCharApi_GetFallSpeed(objId), 0.0f);
    } else {
        Vec3_Set(out, -vel.x, gEftAuraPrm->sparkRise * RANDF(), -vel.z);
    }
    out->w = 1.0f;
    Vec3_Normalize(out, out);
}

/* Flame direction term: away from the aura centre, plus a random left / right push across the facing. */
void EftAura_GetFlameSideDir(EftAura *aura, Vec4 *out, s32 objId, f32 power, f32 away, f32 side) {
    Vec4 dir;
    Vec4 centre;
    f32 one = 1.0f;

    side += gEftAuraCfg->unk1A0 * power * gEftAuraPrm->sideScale;
    BtlCharApi_GetDir(objId, &dir);
    dir.y = 0.0f;
    dir.w = one;
    Vec3_Normalize(&dir, &dir);
    if (rand() & 1) {
        side = -side;
    }
    Vec4_Scale(&dir, &dir, side);
    Vec4_Set(&centre, aura->unk310.x, 0.0f, aura->unk310.z, one);
    Vec3_Sub(out, &centre, &aura->pos);
    out->w = one;
    Vec3_Normalize(out, out);
    Vec3_Scale(out, out, away);
    out->x += dir.x;
    out->z += dir.z;
}

/* Flame direction term: against the motion of the body part relative to the fighter's root. */
void EftAura_GetFlameDragDir(EftAura *aura, Vec4 *pos, Vec4 *out, s32 objId, s32 part) {
    Vec4 d;
    Vec4 now;
    Vec4 before;
    f32 one;

    Vec3_Sub(&now, pos, &aura->pos);
    Vec3_Sub(&before, &aura->partPrev[part], &aura->prevPos);
    Vec3_Sub(&d, &now, &before);
    one = 1.0f;
    d.w = one;
    Vec3_Normalize(&d, &d);
    Vec4_Scale(&d, &d, sqrtf(Vec3_Dot(&d, &d)) * gEftAuraPrm->drag);
    Vec3_Scale(out, &d, -1.0f);
    out->w = one;
}

/* Flame direction term: outward from a random point between the aura axis and the root, scaled down by speed. */
void EftAura_GetFlameOutDir(EftAura *aura, Vec4 *out, s32 objId, s32 part, f32 power, f32 amount) {
    Vec4 from;
    Vec4 span;
    Vec4 d;
    f32 one;
    f32 ratio;
    f32 t;

    ratio = BtlCharApi_GetSpeed(objId) * gEftAuraPrm->speedRef;
    one = 1.0f;
    t = RANDF();
    amount += gEftAuraCfg->unk1A0 * power;
    if (one < ratio) {
        ratio = one;
    }
    amount *= one - ratio;
    span.x = aura->pos.x - aura->axis.x;
    span.z = aura->pos.z - aura->axis.z;
    from.w = 0.0f;
    from.y = aura->pos.y;
    from.x = aura->axis.x + span.x * t;
    from.z = aura->axis.z + span.z * t;
    Vec3_Copy(&d, &aura->part[part]);
    d.w = 0.0f;
    Vec4_Sub(&d, &d, &from);
    Vec3_Normalize(&d, &d);
    Vec4_Set(out, d.x * amount, d.y * amount * gEftAuraPrm->outRise, d.z * amount, one);
}

/* Bends a flame's direction: early towards its start direction and turn axis, late towards `pos`. */
void EftAura_TurnFlame(EftAuraFlame *f, Vec4 *pos, f32 t) {
    Vec4 d;
    Vec4 tmp;
    f32 t3 = t * t * t;
    f32 one = 1.0f;
    Vec4 *dir = &f->dir;
    f32 turn = gEftAuraPrm->kind[f->kind].turn;

    Vec3_Scale(&tmp, &f->dirStart, (one - t3) * 0.1f * turn);
    Vec3_Add(dir, dir, &tmp);
    Vec3_Scale(&tmp, &f->dirTurn, (one - t) * 0.08f * turn);
    Vec3_Add(dir, dir, &tmp);
    Vec3_Sub(&d, pos, &f->pos);
    d.w = one;
    Vec3_Normalize(&d, &d);
    Vec3_Scale(&tmp, &d, t3 * 0.15f * turn);
    Vec3_Add(dir, dir, &tmp);
    f->dir.w = one;
    Vec3_Normalize(dir, dir);
}

/* Accelerates a flame early in its life and damps it later. */
void EftAura_AccelFlame(EftAuraFlame *f, f32 t) {
    f->speed = (f->speed + gEftAuraPrm->speed[1] * (1.0f - t) * gEftAuraPrm->kind[f->kind].accel) * (t * 0.2f + 0.8f);
}

/* Offset from the aura axis (at the root's height) towards a body part. */
void EftAura_GetPartOffset(EftAura *aura, Vec4 *out, s32 objId, s32 part, f32 radial, f32 vertical) {
    Vec4 from;
    Vec4 d;

    from.x = aura->axis.x;
    from.y = aura->pos.y;
    from.z = aura->axis.z;
    from.w = 0.0f;
    Vec3_Copy(&d, &aura->part[part]);
    Vec3_Sub(&d, &d, &from);
    d.w = 1.0f;
    Vec3_Normalize(&d, &d);
    out->w = 0.0f;
    out->x = d.x * radial;
    out->y = d.y * vertical;
    out->z = d.z * radial;
}

/* Takes a spark from the free list, else the next unused slot; links it on the used list. */
EftAuraSpark *EftAura_AllocSpark(void) {
    EftAuraSpark *p;

    if (gEftAuraPool->sparkFree != NULL) {
        p = gEftAuraPool->sparkFree;
        gEftAuraPool->sparkFree = p->next;
    } else if (gEftAuraPool->sparkNext <= gEftAuraPool->sparkMax - 1) {
        p = &gEftAuraPool->sparks[gEftAuraPool->sparkNext++];
    } else {
        return NULL;
    }
    p->next = gEftAuraPool->sparkUsed;
    gEftAuraPool->sparkUsed = p;
    p->flags = 0;
    return p;
}

/* Returns every spark of a fighter to the free list. */
void EftAura_FreeSparks(s32 objId) {
    EftAuraSpark **link = &gEftAuraPool->sparkUsed;
    EftAuraSpark *p;

    while (*link != NULL) {
        p = *link;
        if (p->objId != objId) {
            link = &p->next;
        } else {
            *link = p->next;
            p->next = gEftAuraPool->sparkFree;
            gEftAuraPool->sparkFree = p;
        }
    }
}

/* Creates one spark of an emitter: random life and colour, direction from the fighter's motion. */
s32 EftAura_SpawnSpark(EftAura *aura, s32 objId, s32 emitter, s32 kind, Vec4 *offset, f32 follow) {
    Vec4 dir;
    EftAuraSpark *s = EftAura_AllocSpark();
    s32 color = rand() % aura->colorCount;
    f32 alpha;

    if (s == NULL) {
        return 0;
    }
    s->objId = objId;
    s->flags = 0;
    if (aura->flags & 0x80) {
        s->flags = 8;
    }
    if ((u32)(aura->type - 9) < 2) {
        s->flags |= 0x10;
    }
    if (aura->burst == 1) {
        s->flags |= 2;
    }
    s->emitter = emitter;
    s->kind = kind;
    s->age = 0.0f;
    if (s->flags & 0x10) {
        s->life = gEftAuraCfg->sparkLifeLong + gEftAuraCfg->sparkLifeLongRange * RANDF();
    } else {
        s->life = gEftAuraCfg->sparkLife + gEftAuraCfg->sparkLifeRange * RANDF();
    }
    s->follow = follow;
    Vec4_Copy(&s->offset, offset);
    Vec4_Set(&s->move, 0.0f, 0.0f, 0.0f, 0.0f);
    s->unk7 = 1;
    if (!(aura->flags & 0x20)) {
        Vec3_Copy(&s->color, &aura->sparkColor[color]);
        s->color.w = 0.0f;
        alpha = aura->sparkColor[color].w * aura->level;
        EftAura_GetSparkDir(&dir, objId);
        if (s->flags & 8) {
            s->size = gEftAuraCfg->sparkSize[1][0];
            s->speed = gEftAuraCfg->sparkSize[1][1];
        } else {
            s->size = gEftAuraCfg->sparkSize[0][0];
            s->speed = gEftAuraCfg->sparkSize[0][1];
        }
    } else {
        Vec3_Copy(&s->color, &aura->sparkColorAlt[color]);
        s->color.w = 0.0f;
        alpha = aura->sparkColorAlt[color].w;
        EftAura_GetSparkDirAlt(&dir, objId);
        if (s->flags & 8) {
            s->size = gEftAuraCfg->sparkSize[3][0];
            s->speed = gEftAuraCfg->sparkSize[3][1];
        } else {
            s->size = gEftAuraCfg->sparkSize[2][0];
            s->speed = gEftAuraCfg->sparkSize[2][1];
        }
    }
    Vec4_Copy(&s->dir, &dir);
    s->dir.w = 1.0f;
    Vec3_Set(&s->colorRate, 0.0f, 0.0f, 0.0f);
    s->colorRate.w = alpha / gEftAuraCfg->sparkFadeIn;
    Vec4_Set(&s->uv, 0.0f, 0.0f, 1.0f, 1.0f);
    s->rot = 0.0f;
    s->rotSpeed = gEftAuraCfg->sparkRotSpeed * 3.14159265f;
    if (rand() & 1) {
        s->rotSpeed = -s->rotSpeed;
    }
    if (s->flags & 4) {
        s->rotSpeed *= gEftAuraCfg->sparkMul[0][0];
        s->speed *= gEftAuraCfg->sparkMul[0][1];
    } else if (s->flags & 2) {
        s->rotSpeed *= gEftAuraCfg->sparkMul[1][0];
        s->speed *= gEftAuraCfg->sparkMul[1][1];
    }
    return 1;
}

/* Spawns the sparks of one emitter: one per `sparkStep` of the distance to its node (one only with `once`). */
void EftAura_SpawnSparks(EftAura *aura, s32 objId, s32 emitter, s32 once) {
    Vec4 pos;
    Vec4 nodePos;
    Vec4 delta;
    Vec4 d;
    Vec4 offset;
    s32 count = 1;
    f32 step;
    f32 len;
    s32 node;
    s32 kind;
    s32 i;

    step = gEftAuraCfg->sparkStep * aura->scale;
    Vec4_Set(&offset, 0.0f, 0.0f, 0.0f, 1.0f);
    kind = gEftAuraCfg->spark[emitter].kind;
    node = gEftAuraCfg->spark[emitter].node;
    Vec4_Copy(&pos, &aura->sparkPos[emitter]);
    Vec4_Set(&delta, 0.0f, 0.0f, 0.0f, 1.0f);
    if (node >= 0) {
        BtlCharApi_GetNodePos(objId, node, &nodePos);
        Vec4_Sub(&d, &nodePos, &pos);
        len = sqrtf(Vec3_Dot(&d, &d));
        Vec4_Copy(&delta, &d);
        if (step < len) {
            count = len / step + 1.0f;
        }
    }
    for (i = 0; i < count; i++) {
        EftAura_SpawnSpark(aura, objId, emitter, kind, &offset, RANDF());
        if (once) {
            break;
        }
    }
}

/* Moves, spins and fades a fighter's sparks; a spark at the end of its life is replaced by a new one. */
void EftAura_StepSparks(EftAura *aura, s32 objId) {
    Vec4 tmp;
    Vec4 nodePos;
    Vec4 move;
    EftAuraSpark **link;
    EftAuraSpark *s;
    Vec4 *pos;
    Vec4 *drift;
    f32 one = 1.0f;
    f32 t;
    f32 fadeOut;
    s32 node;

    Vec4_Set(&tmp, 0.0f, 0.0f, 0.0f, one);
    link = &gEftAuraPool->sparkUsed;
    while (*link != NULL) {
        s = *link;
        if (s->objId == objId) {
        if (!(s->age < s->life)) {
            EftAura_SpawnSparks(aura, s->objId, s->emitter, 1);
            *link = s->next;
            s->next = gEftAuraPool->sparkFree;
            gEftAuraPool->sparkFree = s;
        } else {
            pos = &s->pos;
            Vec4_Copy(pos, &aura->sparkPos[s->emitter]);
            node = gEftAuraCfg->spark[s->emitter].node;
            if (node >= 0) {
                BtlCharApi_GetNodePos(s->objId, node, &nodePos);
                Vec4_Sub(&tmp, &nodePos, pos);
                Vec3_Scale(&move, &tmp, s->follow);
                Vec4_Add(pos, pos, &move);
            }
            s->pos.w = one;
            Vec3_Scale(&move, &s->dir, s->speed);
            drift = &s->move;
            Vec4_Add(drift, drift, &move);
            t = s->age / s->life;
            s->speed += gEftAuraCfg->sparkAccel * t;
            s->speed *= gEftAuraCfg->sparkDamp * t;
            Vec4_Add(pos, pos, drift);
            s->pos.w = one;
            s->rot += s->rotSpeed;
            if (s->rot >= 3.14159265f) {
                s->rot -= 6.2831853f;
            } else if (s->rot <= -3.14159265f) {
                s->rot += 6.2831853f;
            }
            fadeOut = gEftAuraCfg->sparkFadeOut;
            if (s->life - s->age <= fadeOut) {
                if (!(s->flags & 1)) {
                    s->flags |= 1;
                    s->colorRate.w = -s->color.w / fadeOut;
                }
            } else if (gEftAuraCfg->sparkFadeIn < s->age) {
                s->colorRate.w = 0.0f;
            }
            s->color.w += s->colorRate.w;
            if (s->color.w < 0.0f) {
                s->color.w = 0.0f;
            }
            s->age += one;
            link = &s->next;
        }
        } else {
            link = &s->next;
        }
    }
}

/* Task entry: on the first call spawns the sparks of all twelve emitters, then steps them. */
void EftAura_UpdateSparks(EftAura *aura, s32 objId, s32 *state) {
    s32 i;

    if (!(*state & 1)) {
        for (i = 0; i < EFT_AURA_SPARKS; i++) {
            EftAura_SpawnSparks(aura, objId, i, 0);
        }
        *state |= 1;
    }
    EftAura_StepSparks(aura, objId);
}

/* Draws a fighter's sparks as rotated sprites. */
void EftAura_DrawSparks(EftAura *aura, s32 objId, f32 alpha) {
    EftAuraSpark **link = &gEftAuraPool->sparkUsed;
    s32 *tex = &gEftAuraPool->sparkTex;
    EftAuraSpark *s;
    f32 a;

    if (aura->flags & 0x20) {
        a = alpha;
    } else {
        a = aura->alpha * aura->unk50 * alpha;
    }
    while (*link != NULL) {
        s = *link;
        if (s->objId == objId) {
            EftSpr_DrawRot((u32)(s->color.x * 255.0f), (u32)(s->color.y * 255.0f), (u32)(s->color.z * 255.0f),
                          (u32)(s->color.w * 255.0f * a), s->pos.x, s->pos.y, s->pos.z, s->uv.x, s->uv.y, s->uv.z,
                          s->uv.w, s->rot, 0, 0, 0x40, 0x40, 0, (u32)(s->size * 4096.0f), 0, s->unk7, tex);
        }
        link = &s->next;
    }
}

/* Takes a flame from the free list, else the next unused slot; links it on the used list. */
EftAuraFlame *EftAura_AllocFlame(void) {
    EftAuraFlame *p;

    if (gEftAuraPool->flameFree != NULL) {
        p = gEftAuraPool->flameFree;
        gEftAuraPool->flameFree = p->next;
    } else if (gEftAuraPool->flameNext <= gEftAuraPool->flameMax - 1) {
        p = &gEftAuraPool->flames[gEftAuraPool->flameNext++];
    } else {
        return NULL;
    }
    p->next = gEftAuraPool->flameUsed;
    gEftAuraPool->flameUsed = p;
    p->flags = 0;
    return p;
}

/* Flag word of a new flame from the aura's state. */
s32 EftAura_GetFlameFlags(EftAura *aura, s32 objId, s32 second, s32 alt) {
    s32 flags = 0x800;

    if (second == 0) {
        if (alt == 0) {
            flags = 0;
        } else {
            flags = 0x400;
        }
    }
    if (aura->flags & 0x80) {
        flags |= 0x100;
    }
    if ((u32)(aura->type - 9) < 2) {
        flags |= 0x200;
    }
    if (aura->burst == 1) {
        flags |= 0x20;
    } else if (aura->burst == 2) {
        flags |= 0x40;
    }
    if (aura->flags & 0x20) {
        flags |= 0x1000;
    }
    return flags;
}

/* Life of a new flame in frames. */
f32 EftAura_GetFlameLife(s32 flags, f32 burst) {
    f32 *p = gEftAuraPrm->life;
    f32 life;

    if (flags & 0x200) {
        life = p[2] + p[2] * RANDF();
    } else {
        life = p[0] + p[1] * RANDF();
    }
    if (flags & 0x20) {
        life *= 1.0f - burst * 0.15f;
    } else if (flags & 0x80) {
        life *= 0.85f;
    }
    return life;
}

/* Speed of a new flame. */
f32 EftAura_GetFlameSpeed(s32 flags, s32 objId, f32 level, f32 burst) {
    f32 *p = gEftAuraPrm->speed;
    f32 speed;

    speed = (BtlCharApi_GetSpeed(objId) * p[2] + p[0]) * level;
    if (flags & 0x100) {
        if (flags & 0x800) {
            speed += p[3];
        }
    }
    if (flags & 0x80) {
        speed += p[4];
    } else if (flags & 0x20) {
        speed += p[5] * burst;
    }
    if (!(flags & 0x800)) {
        if (flags & 0x400) {
            p = gEftAuraPrm->speedRand[1];
        } else {
            p = gEftAuraPrm->speedRand[0];
        }
    } else {
        p = gEftAuraPrm->speedRand[2];
    }
    return speed * (p[0] + p[1] * RANDF());
}

/* Peak alpha of a new flame. */
f32 EftAura_GetFlameAlpha(EftAura *aura, s32 flags, f32 alpha) {
    f32 *base = gEftAuraPrm->alpha;
    f32 *mul = gEftAuraPrm->alphaMul;

    if (!(flags & 0x800)) {
        alpha *= base[0];
    } else {
        alpha *= base[1];
    }
    if (flags & 0x80) {
        alpha *= mul[2];
    } else if (flags & 0x20) {
        alpha *= mul[3];
    } else if (!(flags & 0x800)) {
        alpha *= mul[0];
    } else {
        alpha *= mul[1];
    }
    return alpha;
}

/* Stretch of a flame at a phase (0 start, 1 grown, 2 late, 3 end); 0 for an invisible flame. */
f32 EftAura_GetFlameStretch(EftAuraFlame *f, s32 objId, s32 phase) {
    f32 *mul = gEftAuraPrm->stretchMul;
    f32 *kind = gEftAuraPrm->stretchKind;
    f32 *rnd = gEftAuraPrm->stretchRand[f->kind];
    f32 *st = &gEftAuraPrm->stretch[phase];
    f32 v = *st;
    f32 k;

    if (f->flags & 0x1000) {
        return 0.0f;
    }
    if (f->flags & 0x100) {
        if (!(f->flags & 0x800)) {
            v *= kind[2];
        } else {
            v *= kind[3];
        }
    } else if (!(f->flags & 0x800)) {
        v *= kind[0];
    } else {
        v *= kind[1];
    }
    if (f->flags & 0x80) {
        v *= mul[0];
    } else if (f->flags & 0x20) {
        v *= mul[1];
    } else if (f->flags & 0x40) {
        v *= mul[2];
    }
    k = rnd[0] + rnd[1] * RANDF();
    k += BtlCharApi_GetSpeed(objId) * rnd[2];
    if (1.0f < k) {
        k = 1.0f;
    }
    return v * k;
}

/* Size of a flame at a phase; 0 for an invisible flame. */
f32 EftAura_GetFlameSize(EftAuraFlame *f, s32 objId, s32 phase) {
    f32 v = gEftAuraPrm->size[phase];

    if (f->flags & 0x1000) {
        return 0.0f;
    }
    if (f->flags & 0x100) {
        if (!(f->flags & 0x800)) {
            v *= gEftAuraPrm->sizeKind[2];
        } else {
            v *= gEftAuraPrm->sizeKind[2];
        }
    } else {
        v *= gEftAuraPrm->sizeKind[0];
    }
    if (f->flags & 0x80) {
        v *= gEftAuraPrm->sizeMul[0];
    } else if (f->flags & 0x20) {
        v *= gEftAuraPrm->sizeMul[1];
    } else if (f->flags & 0x40) {
        v *= gEftAuraPrm->sizeMul[2];
    }
    return v;
}

/* End ratio of a flame: random, lower at speed, at least 0.65 (0.9 for the alternate kind). */
f32 EftAura_GetFlameEnd(EftAuraFlame *f, s32 objId) {
    f32 *p = gEftAuraPrm->end[f->kind];
    f32 v;

    v = p[0] + p[1] * RANDF();
    v -= BtlCharApi_GetSpeed(objId) * p[2];
    if (!(f->flags & 0x800)) {
        if (v < 0.65f) {
            v = 0.65f;
        }
    } else {
        if (v < 0.9f) {
            v = 0.9f;
        }
    }
    return v;
}

/* Creates one flame at a body part. `second` makes the extra flame of kind 2 (flag 0x800). */
s32 EftAura_SpawnFlame(EftAura *aura, s32 objId, s32 part, Vec4 *pos, Vec4 *offset, f32 follow, s32 alt, s32 second) {
    Vec4 dir;
    Vec4 side;
    Vec4 drag;
    Vec4 out;
    Vec4 d;
    EftAuraFlame *f = EftAura_AllocFlame();
    s32 color = rand() % aura->colorCount;
    f32 lifeScale;
    f32 level;
    f32 burst;
    f32 grow;
    f32 r1;
    f32 r2;
    f32 r3;
    EftAuraPrmKind *kind;

    Vec4_Set(&side, 0.0f, 0.0f, 0.0f, 1.0f);
    Vec4_Set(&drag, 0.0f, 0.0f, 0.0f, 1.0f);
    Vec4_Set(&out, 0.0f, 0.0f, 0.0f, 1.0f);
    if (f == NULL) {
        return 0;
    }
    lifeScale = aura->lifeScale;
    level = aura->level;
    burst = aura->burstLevel;
    f->objId = objId;
    f->part = part;
    f->unk8 = gEftAuraCfg->flame[part].unk0;
    f->flags = EftAura_GetFlameFlags(aura, objId, second, alt);
    if (second == 0) {
        f->unk4 = 0;
        if (f->flags & 0x400) {
            f->kind = 1;
        } else {
            f->kind = 0;
        }
    } else {
        f->unk4 = 1;
        f->kind = 2;
    }
    f->life = f->time = EftAura_GetFlameLife(f->flags, burst);
    Vec4_Copy(&f->offset, offset);
    f->follow = follow;
    f->speed = EftAura_GetFlameSpeed(f->flags, objId, level, burst);
    f->power = lifeScale * burst;
    f->scale = aura->scale * level;
    f->size = EftAura_GetFlameSize(f, objId, 0);
    f->stretch = EftAura_GetFlameStretch(f, objId, 0);
    grow = f->life * gEftAuraPrm->growRatio;
    f->sizeRate = (EftAura_GetFlameSize(f, objId, 1) - f->size) / grow;
    f->stretchRate = (EftAura_GetFlameStretch(f, objId, 1) - f->stretch) / grow;
    f->end = EftAura_GetFlameEnd(f, objId);
    EftAura_GetSparkDir(&dir, objId);
    EftAura_GetFlameDragDir(aura, pos, &drag, objId, part);
    if (f->flags & 0x100) {
        f->tex = rand() % aura->texCount;
    }
    if (!(f->flags & 0x800)) {
        f->alt = 0;
        Vec3_Scale(&f->color, &aura->colorStart, RANDF() * 0.2f + 1.0f);
        f->color.w = 0.0f;
        Vec4_Sub(&d, &aura->colorEnd, &f->color);
        Vec3_Scale(&f->colorRate, &d, 1.0f / f->life);
        f->colorRate.w = EftAura_GetFlameAlpha(aura, f->flags, aura->colorStart.w) * level / gEftAuraPrm->fadeIn;
    } else {
        f->alt = 1;
        Vec3_Scale(&f->color, &aura->colorAlt[color], 1.0f);
        f->color.w = 0.0f;
        Vec4_Sub(&d, &aura->colorAlt[color], &f->color);
        Vec3_Scale(&f->colorRate, &d, 1.0f / f->life);
        f->colorRate.w = EftAura_GetFlameAlpha(aura, f->flags, aura->colorAlt[color].w) * level / gEftAuraPrm->fadeIn;
    }
    kind = &gEftAuraPrm->kind[f->kind];
    r1 = kind->side[0] + kind->side[1] * RANDF();
    r2 = kind->away[0] + kind->away[1] * RANDF();
    r3 = kind->out[0] + kind->out[1] * RANDF();
    EftAura_GetFlameSideDir(aura, &side, objId, f->power, r1, r2);
    EftAura_GetFlameOutDir(aura, &out, objId, f->part, f->power, r3);
    Vec4_Add(&f->dirTurn, &dir, &drag);
    Vec3_Normalize(&f->dirTurn, &f->dirTurn);
    Vec4_Add(&dir, &dir, &side);
    Vec4_Add(&dir, &dir, &drag);
    Vec3_Normalize(&dir, &dir);
    Vec4_Add(&f->dir, &out, &dir);
    Vec3_Normalize(&f->dir, &f->dir);
    Vec4_Copy(&f->dirStart, &f->dir);
    Vec4_Sub(&f->dirTurn, &f->dir, &f->dirTurn);
    Vec3_Normalize(&f->dirTurn, &f->dirTurn);
    Vec3_Scale(&f->move, &f->dir, gEftAuraPrm->startDist);
    Vec4_Add(&f->pos, pos, offset);
    Vec4_Add(&f->pos, &f->pos, &f->move);
    f->pos.w = 1.0f;
    f->endTime = f->life * gEftAuraPrm->endRatio;
    f->flip = rand() % 2;
    return 1;
}

/* Spawns the flames of one body part: one per `flameStep` of its length (one only with `once`), each with a
   possible second flame while fewer than 45 are alive. Returns the last primary spawn's result. */
s32 EftAura_SpawnFlames(EftAura *aura, s32 objId, s32 part, s32 once) {
    Vec4 pos;
    Vec4 nodePos;
    Vec4 delta;
    Vec4 d;
    Vec4 offset;
    Vec4 along;
    Vec4 radial;
    s32 ret = 0;
    s32 count = 1;
    f32 one = 1.0f;
    f32 scale = aura->scale;
    f32 step = gEftAuraPrm->flameStep * scale;
    f32 len;
    f32 t;
    f32 r1;
    s32 node;
    s32 alt;
    s32 i;

    Vec4_Copy(&pos, &aura->part[part]);
    Vec4_Set(&delta, 0.0f, 0.0f, 0.0f, one);
    node = gEftAuraCfg->flame[part].node;
    if (node >= 0) {
        BtlCharApi_GetNodePos(objId, node, &nodePos);
        Vec4_Sub(&d, &nodePos, &pos);
        len = sqrtf(Vec3_Dot(&d, &d));
        Vec4_Copy(&delta, &d);
        if (step < len) {
            count = len / step + one;
        }
    }
    for (i = 0; i < count; i++) {
        if (part < 2) {
            t = RANDF() * 0.4f + 0.3f;
        } else {
            t = RANDF();
        }
        Vec3_Scale(&along, &delta, t);
        r1 = RANDF() * 0.05f + 0.3f;
        EftAura_GetPartOffset(aura, &radial, objId, part, r1, RANDF() * 0.05f + 0.3f);
        Vec4_Scale(&radial, &radial, scale);
        Vec3_Copy(&offset, &radial);
        if (aura->altMask & (1U << part)) {
            aura->altMask &= ~(1U << part);
            alt = 1;
        } else {
            aura->altMask |= 1U << part;
            alt = 0;
        }
        if ((u32)(aura->burst - 1) < 2) {
            if (part < 6) {
                alt = 0;
            } else {
                alt = 1;
            }
        }
        ret = EftAura_SpawnFlame(aura, objId, part, &pos, &offset, t, alt, 0);
        if (ret != 0) {
            aura->flameCount++;
            if (aura->flameCount < 45) {
                if (part < 2 || !(rand() & 1)) {
                    if (EftAura_SpawnFlame(aura, objId, part, &pos, &offset, t, 0, 1)) {
                        aura->flameCount++;
                    }
                }
            }
        }
        if (once) {
            break;
        }
    }
    return ret;
}

/* Marks the aura active, picks the variant from the argument, and with `reset` restarts its fade-in. */
void EftAura_Start(EftAura *aura, s32 *arg, s32 reset) {
    aura->flags |= 1;
    if (arg[1] != 0) {
        aura->flags |= 0x20;
    } else {
        aura->flags |= 0x10;
    }
    if (reset) {
        aura->unk5C = 0.0f;
        aura->burst = 0;
        aura->burstLevel = 0.0f;
        aura->fadeA = 0;
        aura->unk54 = 0.0f;
        aura->fadeB = 0;
        aura->lifeScale = 1.0f;
        aura->unk50 = 1.0f;
        aura->unk58 = 1.0f;
        aura->fade = 1;
    }
}

/* Returns every flame of a fighter to the free list. */
void EftAura_FreeFlames(s32 objId) {
    EftAuraFlame **link = &gEftAuraPool->flameUsed;
    EftAuraFlame *p;

    while (*link != NULL) {
        p = *link;
        if (p->objId != objId) {
            link = &p->next;
        } else {
            *link = p->next;
            p->next = gEftAuraPool->flameFree;
            gEftAuraPool->flameFree = p;
        }
    }
}

/* Moves, grows, colours and fades a fighter's flames; a flame near its end spawns its successor, a finished
   one is freed. */
void EftAura_StepFlames(EftAura *aura, s32 objId) {
    Vec4 pos;
    Vec4 base;
    Vec4 end;
    Vec4 d;
    Vec4 tmp;
    EftAuraFlame **link;
    EftAuraFlame *f;
    Vec4 *move;
    Vec4 *color;
    f32 t;

    link = &gEftAuraPool->flameUsed;
    while (*link != NULL) {
        f = *link;
        if (f->objId == objId) {
            if (0.0f < f->time) {
                Vec4_Copy(&base, &aura->part[f->part]);
                Vec4_Copy(&pos, &base);
                if (gEftAuraCfg->flame[f->part].node >= 0) {
                    Vec4_Copy(&end, &aura->partEnd[f->part]);
                    Vec4_Sub(&d, &end, &base);
                    Vec3_Scale(&tmp, &d, f->follow);
                    Vec4_Add(&pos, &pos, &tmp);
                }
                Vec4_Add(&pos, &pos, &f->offset);
                t = f->time / f->life;
                EftAura_TurnFlame(f, &pos, t);
                EftAura_AccelFlame(f, t);
                Vec3_Scale(&tmp, &f->dir, f->speed + aura->burstSpeed);
                move = &f->move;
                Vec3_Add(move, move, &tmp);
                Vec4_Add(&f->pos, &pos, move);
                f->pos.w = 1.0f;
                f->size += f->sizeRate;
                f->stretch += f->stretchRate;
                if (f->size < 0.0f) {
                    f->size = 0.0f;
                }
                if (f->stretch < 0.0f) {
                    f->stretch = 0.0f;
                }
                if (!(f->flags & 8)) {
                    if (gEftAuraPrm->growRatio < 1.0f - f->time / f->life) {
                        f->sizeRate = (EftAura_GetFlameSize(f, f->objId, 2) - f->size) / f->time;
                        f->stretchRate = (EftAura_GetFlameStretch(f, f->objId, 2) - f->stretch) / f->time;
                        f->flags |= 8;
                    }
                }
                if (!(f->flags & 1)) {
                    if (gEftAuraPrm->fadeIn <= f->life - f->time) {
                        f->colorRate.w = 0.0f;
                        f->flags |= 1;
                    }
                }
                if (!(f->flags & 2)) {
                    if (f->time <= gEftAuraPrm->fadeOut) {
                        f->colorRate.w = -((f->color.w - aura->colorEnd.w) * (1.0f / (f->time + 1.0f)));
                        f->flags |= 2;
                    }
                }
                color = &f->color;
                Vec4_Add(color, color, &f->colorRate);
                Vec4_Clamp(color, color, 0.0f, 1.0f);
                f->time -= 1.0f;
                if (f->time <= f->endTime) {
                    if (!(f->flags & 0x10)) {
                        f->sizeRate = (EftAura_GetFlameSize(f, f->objId, 3) - f->size) / f->time;
                        f->stretchRate = (EftAura_GetFlameStretch(f, f->objId, 3) - f->stretch) / f->time;
                        f->flags |= 0x10;
                    }
                    if (!(f->flags & 4)) {
                        if (f->flags & 0x800) {
                            f->flags |= 4;
                        } else if (EftAura_SpawnFlames(aura, f->objId, f->part, 1)) {
                            f->flags |= 4;
                        }
                    }
                }
                link = &f->next;
            } else if (!(f->flags & 4)) {
                if (EftAura_SpawnFlames(aura, f->objId, f->part, 1)) {
                    f->flags |= 4;
                }
            } else {
                *link = f->next;
                f->next = gEftAuraPool->flameFree;
                gEftAuraPool->flameFree = f;
                aura->flameCount--;
            }
        } else {
            link = &f->next;
        }
    }
}

/* Per-frame aura state: first flames of the ten body parts, the burst and the three fades, then the flames. */
void EftAura_UpdateState(EftAura *aura, s32 objId) {
    while (aura->partsStarted < EFT_AURA_PARTS) {
        EftAura_SpawnFlames(aura, objId, aura->partsStarted, 0);
        aura->partsStarted++;
    }
    if (aura->burst != 0) {
        if (aura->burst == 1) {
            aura->burstLevel += 0.15f;
            aura->lifeScale = 1.2f;
            if (1.0f <= aura->burstLevel) {
                aura->burstLevel = 1.0f;
            }
        } else if (aura->burst == 2) {
            aura->burstLevel = 1.0f;
            aura->burstTime += 1.0f;
            if (gEftAuraCfg->burstHold <= aura->burstTime) {
                aura->burst = 3;
                aura->burstTime = 0.0f;
                aura->fadeA = 1;
                aura->fadeB = 1;
            }
        } else {
            aura->burstLevel -= 0.1f;
            if (aura->burstLevel <= 0.0f) {
                aura->burstLevel = 0.0f;
                aura->lifeScale -= 0.2f;
                if (aura->lifeScale <= 1.0f) {
                    aura->lifeScale = 1.0f;
                }
            }
            if (aura->lifeScale <= 1.0f && aura->burstLevel <= 0.0f) {
                aura->burst = 0;
            }
        }
    }
    aura->burstSpeed = gEftAuraCfg->burstSpeed * aura->burstLevel;
    if (aura->fadeA == 1) {
        if (0.0f < aura->unk50) {
            aura->unk50 -= 1.0f / gEftAuraCfg->unk1B4;
            if (aura->unk50 <= 0.0f) {
                aura->unk50 = 0.0f;
                aura->unk54 = 0.0f;
                aura->fadeA = 0;
            }
        }
    } else if (aura->unk50 < 1.0f) {
        aura->unk54 += 1.0f / gEftAuraCfg->unk1B0;
        aura->unk50 += aura->unk54;
        if (1.0f <= aura->unk50) {
            aura->unk50 = 1.0f;
            aura->unk54 = 0.0f;
        }
    }
    if (aura->fadeB == 1) {
        if (0.0f < aura->unk58) {
            aura->unk58 -= 1.0f / gEftAuraCfg->unk1AC;
            if (aura->unk58 <= 0.0f) {
                aura->unk58 = 0.0f;
                aura->unk5C = 0.0f;
                aura->fadeB = 0;
            }
        }
    } else if (aura->unk58 < 1.0f) {
        aura->unk5C += 1.0f / gEftAuraCfg->unk1A8;
        aura->unk58 += aura->unk5C;
        if (1.0f <= aura->unk58) {
            aura->unk58 = 1.0f;
            aura->unk5C = 0.0f;
        }
    }
    if (aura->fade != 0) {
        f32 step;

        if (aura->fade == 1) {
            step = 1.0f / gEftAuraCfg->fadeInTime;
            aura->alpha += step;
            if (1.0f <= aura->alpha) {
                aura->alpha = 1.0f;
                aura->fade = 0;
            }
        } else {
            step = 1.0f / gEftAuraCfg->fadeOutTime;
            aura->alpha -= step;
            if (aura->alpha <= 0.0f) {
                if (aura->fade == 3) {
                    aura->flags |= 2;
                }
                aura->alpha = 0.0f;
                aura->fade = 0;
            }
        }
    }
    EftAura_StepFlames(aura, objId);
}

/* Matrix for a flame quad: third axis along `dir` leaned by the camera's view axis, translation `pos`. */
void EftAura_BuildFlameMtx(Mtx44 *out, Vec4 *dir, Vec4 *pos) {
    Vec4 v;
    Vec4 x;
    Vec4 y;
    Vec4 z;
    Vec4 view;
    f32 one = 1.0f;
    f32 zero = 0.0f;
    f32 d;
    f32 lean;

    view.x = gBtlCamView->world2view2.m[0][2];
    view.y = gBtlCamView->world2view2.m[1][2];
    view.z = gBtlCamView->world2view2.m[2][2];
    view.w = one;
    d = Vec3_Dot(&view, dir);
    if (d < zero) {
        lean = -d * gEftAuraPrm->lean;
        Vec3_Scale(&v, &view, -1.0f);
        v.w = one;
    } else {
        lean = d * gEftAuraPrm->lean;
        Vec3_Copy(&v, &view);
        v.w = one;
    }
    Vec4_Sub(&v, dir, &v);
    Vec3_Normalize(&v, &v);
    Vec3_Scale(&v, &v, lean);
    z.x = dir->x + v.x;
    z.y = dir->y + v.y;
    z.z = dir->z + v.z;
    z.w = 1.0f;
    Vec3_Normalize(&z, &z);
    Vec3_Cross(&x, &view, &z);
    Vec3_Normalize(&x, &x);
    Vec3_Cross(&y, &x, &z);
    Vec3_Normalize(&y, &y);
    Mtx_StoreIdentity(out);
    out->m[0][0] = x.x;
    out->m[1][0] = y.x;
    out->m[2][0] = z.x;
    out->m[0][1] = x.y;
    out->m[1][1] = y.y;
    out->m[2][1] = z.y;
    out->m[0][2] = x.z;
    out->m[1][2] = y.z;
    out->m[2][2] = z.z;
    out->m[3][0] = pos->x;
    out->m[3][1] = pos->y;
    out->m[3][2] = pos->z;
}
