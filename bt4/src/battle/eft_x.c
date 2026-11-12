#include "common.h"
#include "sys/gfx_ot.h"
#include "battle/eft_x.h"

#ifdef PORT
extern f32 Port_WideFactor(void); /* widescreen on PC: 1, or 4/3 for a 16:9 picture (port/src/plat_stub.c) */
#endif

/*
 * Effect tasks, 0x18D618..0x190CC8: the second half of the particle emitter module of effect pack part
 * kind 10 (the first half, 0x18C190..0x18D618, is the end of eft_w.c). See include/battle/eft_x.h.
 * Nothing here touches a fighter, a hit record or a battle object: the module only builds draw packets.
 * Random draws: EftPart10_InitPtcl, per spawned particle: 18 Rand_FloatRange (VU0 generator; only 3 when the
 * life comes out as 0), up to two libc rand() coin flips (definition flag 4, definition byte 0x20D == 2) and
 * one Rand_IntRange (libc rand(); definition flag 0x10, the texture frame). EftPart10_InitSpin, once per
 * emitter with definition flag 0x40: 3 or 9 Rand_FloatRange. They reach only particle appearance.
 *
 * The key-frame functions (EftPart10_StartKeys, EftPart10_UpdateKeys, EftPart10_SetKey) depend on the element
 * type of the definition's two-value tracks: `f32 track[3][2]` (a 2-D array, [key][0 / 1]), not an array of
 * two-float structures. With the structure the second value is addressed as (def + rest + 4) + index, with the
 * 2-D array as (def + rest) + (index + 4), which shares its base with the first value; that one difference
 * changed the allocation of all fifty shared addresses.
 */

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftXView {
    /* 0x000 */ Mtx44 world2view;
    /* 0x040 */ Mtx44 world2view2;
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xB4];
    /* 0x234 */ f32 screenDist;
} EftXView;

/* One vertex as ClipVtx_Set builds it for the EftGfx_DrawPoly functions. */
typedef struct EftXVert {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 uv;
    /* 0x20 */ Vec4 color;
} EftXVert; /* 0x30 */

/* GS registers of the packets queued here. */
typedef struct EftXXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftXXyzf;

typedef struct EftXRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftXRgbaq;

typedef struct EftXSt {
    f32 s, t;
} EftXSt;

/* DMA tag + REGLIST GIF tag: PRIM, TEX0, four (RGBAQ, ST, XYZF2): a gouraud textured strip. */
typedef struct EftXStripPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000008 */
    /* 0x04 */ struct EftXStripPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000008 */
    /* 0x10 */ u64 gifTag;       /* 0xE400000000008001 */
    /* 0x18 */ u64 regs;         /* 0x42142142142160 */
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    struct {
        /* 0x00 */ EftXRgbaq rgbaq;
        /* 0x08 */ EftXSt st;
        /* 0x10 */ EftXXyzf xyz;
    } v[4];
} EftXStripPkt; /* 0x90 */

/* DMA tag + REGLIST GIF tag: PRIM, TEX0, RGBAQ, four (ST, XYZF2), NOP: a flat textured strip. */
typedef struct EftXQuadPkt {
    /* 0x00 */ u32 dmaTag;       /* 0x20000007 */
    /* 0x04 */ struct EftXQuadPkt *next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;         /* 0x50000007 */
    /* 0x10 */ u64 gifTag;       /* 0xC400000000008001 */
    /* 0x18 */ u64 regs;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftXRgbaq rgbaq;
    /* 0x38 */ EftXSt st0;
    /* 0x40 */ EftXXyzf xyz0;
    /* 0x48 */ EftXSt st1;
    /* 0x50 */ EftXXyzf xyz1;
    /* 0x58 */ EftXSt st2;
    /* 0x60 */ EftXXyzf xyz2;
    /* 0x68 */ EftXSt st3;
    /* 0x70 */ EftXXyzf xyz3;
    /* 0x78 */ u64 nop;
} EftXQuadPkt; /* 0x80 */

#define V(p) ((Vec4 *)(p))

/* TEX0 of entry i of a texture table. Only the explicit shift reproduces the original operand order. */
#define EFT_X_TEX0(tex, i) (*(u64 *)((u32)(tex) + ((i) << 4)))

extern EftPart10Mgr *gEftPart10Mgr;
extern EftXView *gBtlCamView;
extern void *gEftPart10Class[6];

extern s32 rand(void);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);
extern f32 Rand_FloatRange(f32 a, f32 b);
extern s32 Rand_IntRange(s32 a, s32 b);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Vec4_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi); /* clamps each component */
extern void Vec4_Mul(Vec4 *dst, Vec4 *a, Vec4 *b);          /* per-component product */
extern void Vec4_ToInt(EftXIVec *dst, Vec4 *src);             /* float vector to integer vector */
extern void Mtx_Translate(Mtx44 *dst, Mtx44 *src, Vec4 *v);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);       /* matrix product */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);               /* inverse of a rotation + translation matrix */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);    /* rotate about Z */
extern void Vu0Cur_LoadIdentity(void);                                 /* VU0: current matrix = identity */
extern void Vu0Cur_Push(void);                                 /* VU0: push the current matrix */
extern void Vu0Cur_Pop(void);                                 /* VU0: pop */
extern void Vu0Cur_Translate(Vec4 *v);                              /* VU0: translate the current matrix */
extern void Vu0Cur_RotateZ(f32 angle);                            /* VU0: rotate the current matrix (Z) */
extern void Vu0Cur_RotateX(f32 angle);                            /* VU0: rotate (X) */
extern void Vu0Cur_RotateY(f32 angle);                            /* VU0: rotate (Y) */
extern void Vu0Cur_ScaleDiagUniform(f32 scale);                            /* VU0: scale the current matrix */
extern void Vu0Cur_MulVec4(Vec4 *dst, Vec4 *src);                 /* VU0: transform by the current matrix */
extern s32 Vu0Cur_ProjectPoint(EftXIVec *out, Vec4 *pos);              /* project one point with the loaded matrix */
extern s32 Vu0Cur_ProjectPointsStq(EftXIVec *xyz, Vec4 *stq, Vec4 *pos, Vec4 *uv, s32 n); /* project n points; 0 = clipped */
extern void ClipVtx_Set(EftXVert *out, Vec4 *pos, Vec4 *uv, Vec4 *color);
extern f32 EftMath_WrapAngle(f32 angle);
extern void EftGfx_DrawPolyFixedZ(EftXVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex, s32 z);
extern void EftGfx_DrawPolyScaledZ(EftXVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 front, s32 flip, u64 tex,
                                   f32 zScale);
extern u64 EftVram_AddImage(EftXTexEntry *tex, s32 a, s32 b);
extern u64 EftVram_AddClut(EftXTexEntry *tex);
extern EftXTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void EftPart10_Update(EftXTask *task);                       /* the emitter class's update */

s32 EftPart10_InitPtcl(EftPart10Ptcl *p, EftPart10 *em);
void EftPart10_LinkPtcl(EftPart10Ptcl **head, EftPart10Ptcl **tail, EftPart10Ptcl *p);
void EftPart10_SelectTex(EftPart10 *em, EftXTexEntry *tbl, s32 a, s32 b);
s32 EftPart10_IsCornerOffScreen(s32 x, s32 y, s32 z);

/* Appends a burst group to an emitter's list. */
void EftPart10_LinkGroup(EftPart10Group **head, EftPart10Group **tail, EftPart10Group *g) {
    if (*head == NULL) {
        *head = g;
        *tail = g;
    } else {
        g->prev = *tail;
        (*tail)->next = g;
        *tail = g;
    }
}

/* Removes a burst group from an emitter's list. */
void EftPart10_UnlinkGroup(EftPart10Group **head, EftPart10Group **tail, EftPart10Group *p) {
    EftPart10Group *prev;
    EftPart10Group *a; /* one variable for the list head and later for a neighbour: needed for the registers */
    EftPart10Group *b;

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


/* Takes a free particle, initialises it from the emitter and starts it on the unit sphere at the two angles;
   appends it to the burst group. Does nothing when no particle is free or its life comes out as 0. */
void EftPart10_Emit(EftPart10 *em, EftPart10Group *g, f32 yaw, f32 pitch) {
    EftPart10Ptcl *p;
    u8 i;

    if (gEftPart10Mgr->ptclNext >= EFT_PART10_MAX) {
        gEftPart10Mgr->ptclNext = 0;
    }
    i = gEftPart10Mgr->ptclNext;
    do {
        p = &gEftPart10Mgr->ptcl[i];
        i++;
        if (i >= EFT_PART10_MAX) {
            i = 0;
        }
        if (p->flags == 0 && EftPart10_InitPtcl(p, em)) {
            f32 c = cosf(yaw);

            p->dir.x = sinf(pitch) * c;
            p->dir.y = cosf(pitch) * c;
            p->dir.z = sinf(yaw);
            p->dir.w = 1.0f;
            Vec4_Copy(&p->pos, &p->dir);
            p->flags |= EFT_PART10_P_ALIVE;
            EftPart10_LinkPtcl(&g->head, &g->tail, p);
            gEftPart10Mgr->ptclNext = i;
            return;
        }
    } while (i != gEftPart10Mgr->ptclNext);
}

/* Fills a particle from the emitter's current key values, each drawn from [value, value + range] with the VU0
   generator; coin flips use libc rand(). Returns 0 when the life comes out as 0. */
s32 EftPart10_InitPtcl(EftPart10Ptcl *p, EftPart10 *em) {
    EftPart10Def *def = em->def;
    Vec4 size;
    Vec4 end;
    f32 life;
    f32 t;

    Vec4_Set(&p->pos, 0.0f, 0.0f, 0.0f, 1.0f);
    Vec4_Set(&p->scale, em->scaleX.v, em->scaleY.v, 0.0f, 1.0f);
    p->speed = em->speedBase.v + Rand_FloatRange(em->speed.v, em->speed.v + em->speedRange.v);
    p->unkC4 = Rand_FloatRange(em->unk128.v, em->unk128.v + em->unk130.v);
    p->unkC4 *= em->size;
    life = Rand_FloatRange(em->life.v, em->life.v + em->lifeRange.v) * 30.0f;
    p->age = 0.0f;
    p->life = life;
    if (life <= 0.0f) {
        return 0;
    }
    t = Rand_FloatRange(em->unk158.v, em->unk158.v + em->unk160.v) * 30.0f;
    p->flags = 0;
    p->next = NULL;
    p->prev = NULL;
    p->unkC8 = 0.0f;
    p->unkCC = 0.0f;
    p->unk134 = t;
    p->angle = (Rand_FloatRange(em->angle.v, em->angle.v + em->angleRange.v) + em->unk290) * 6.2831853f;
    p->unkD4 = Rand_FloatRange(em->unkC0.v, em->unkC0.v + em->unkC8.v) * 3.14159265f;
    p->unkD8 = Rand_FloatRange(em->unkD0.v, em->unkD0.v + em->unkD8.v) * 3.14159265f;
    p->spin = Rand_FloatRange(em->spin.v, em->spin.v + em->spinRange.v) * 3.14159265f;
    if (def->flags & 4) {
        if (!(rand() & 1)) {
            p->spin = -p->spin;
        }
    }
    if ((def->flags & 2) && p->spin < 0.0f) {
        Vec4_Set(&p->uv0, 0.984375f, 0.015625f, 0.015625f, 0.015625f);
        Vec4_Set(&p->uv1, 0.984375f, 0.984375f, 0.015625f, 0.984375f);
        p->flags |= EFT_PART10_P_MIRROR;
    } else {
        Vec4_Set(&p->uv0, 0.015625f, 0.015625f, 0.015625f, 0.984375f);
        Vec4_Set(&p->uv1, 0.984375f, 0.015625f, 0.984375f, 0.984375f);
    }
    if (def->flags & 0x10) {
        p->texFrame = Rand_IntRange(0, em->texFrames - 1.0f);
    } else {
        p->texFrame = 0.0f;
    }
    size.x = Rand_FloatRange(em->kSize[0], em->kSize[0] + em->kSizeRange[0]);
    size.y = Rand_FloatRange(em->kSize[1], em->kSize[1] + em->kSizeRange[1]);
    size.z = Rand_FloatRange(em->kSize[2], em->kSize[2] + em->kSizeRange[2]);
    p->sizeStep0 = (size.y - size.x) / (p->life * def->sizeMid);
    p->sizeStep1 = (size.z - size.y) / (p->life * (1.0f - def->sizeMid));
    p->size = size.x;
    if (def->flags & 0x80) {
        p->unk100 = em->unk178.v * 30.0f;
        p->unkFC = 0.0f;
        p->unk12C = em->unk168.b - em->unk168.a;
        p->unkF8 = em->unk168.a;
        p->unkF4 = em->unk168.a;
    } else {
        p->unkF4 = 1.0f;
    }
    if (def->unk20D == 2) {
        if (!(rand() & 1)) {
            p->flags |= EFT_PART10_P_UNK80;
        }
    } else if (def->unk20D != 1) {
        p->flags |= EFT_PART10_P_UNK80;
    }
    p->unk128 = p->life * (em->unk250.v - em->unk248.v);
    p->unk124 = 0.0f;
    if (p->unk128 <= p->unk124) {
        p->unk128 = p->life;
    }
    p->color0.x = Rand_FloatRange(em->color.x, em->color.x + em->colorRange.x);
    p->color0.y = Rand_FloatRange(em->color.y, em->color.y + em->colorRange.y);
    p->color0.z = Rand_FloatRange(em->color.z, em->color.z + em->colorRange.z);
    p->color0.w = Rand_FloatRange(em->color.w, em->color.w + em->colorRange.w);
    end.x = Rand_FloatRange(em->endColor.x, em->endColor.x + em->endColorRange.x);
    end.y = Rand_FloatRange(em->endColor.y, em->endColor.y + em->endColorRange.y);
    end.z = Rand_FloatRange(em->endColor.z, em->endColor.z + em->endColorRange.z);
    end.w = 0.0f;
    Vec4_Clamp(&p->color0, &p->color0, 0.0f, 255.0f);
    Vec4_Clamp(&end, &end, 0.0f, 255.0f);
    Vec4_Copy(&p->color, &p->color0);
    p->color.w = 0.0f;
    Vec3_Sub(&p->colorStep, &end, &p->color);
    p->unkEC = p->life * def->unk1E8;
    p->unkF0 = p->life * (1.0f - def->unk1EC);
    if (def->flags & 0x100) {
        p->stretchTime = em->stretchTime.v * 30.0f;
        p->stretchAge = 0.0f;
        p->stretchStep[0] = em->stretchX.b - em->stretchX.a;
        p->stretchStep[1] = em->stretchY.b - em->stretchY.a;
        p->stretchStep[2] = em->stretchZ.b - em->stretchZ.a;
        p->stretch[0] = em->stretchX.a;
        p->stretch[1] = em->stretchY.a;
        p->stretch[2] = em->stretchZ.a;
        if (p->stretchStep[0] != 0.0f) {
            p->flags |= EFT_PART10_P_SCALEX;
        }
        if (p->stretchStep[1] != 0.0f) {
            p->flags |= EFT_PART10_P_SCALEY;
        }
        if (p->stretchStep[2] != 0.0f) {
            p->flags |= EFT_PART10_P_SCALEZ;
        }
    }
    return 1;
}

/* Builds the four world-space corners of a particle that is not drawn facing the camera: a square of the
   particle's size, scaled, moved to the particle's offset, turned and placed at the emitter. */
void EftPart10_BuildCorners(EftPart10Ptcl *p, EftPart10 *em) {
    Vec4 dir;
    f32 pitch;
    f32 yaw;

    Vec4_Set(&p->corner[0], p->size * -0.5f, p->size * -0.5f, 0.0f, 1.0f);
    Vec4_Set(&p->corner[1], p->size * -0.5f, p->size * 0.5f, 0.0f, 1.0f);
    Vec4_Set(&p->corner[2], p->size * 0.5f, p->size * -0.5f, 0.0f, 1.0f);
    Vec4_Set(&p->corner[3], p->size * 0.5f, p->size * 0.5f, 0.0f, 1.0f);
    Vu0Cur_ScaleDiagUniform(em->size * em->scale * p->unkF4);
    Vu0Cur_Translate(&p->scale);
    Vu0Cur_RotateZ(p->angle);
    Vec3_Normalize(&dir, &p->pos);
    pitch = EftMath_WrapAngle(Mathf_Asin(-dir.y));
    yaw = EftMath_WrapAngle(atan2f(dir.x, dir.z));
    Vu0Cur_RotateX(pitch);
    Vu0Cur_RotateY(yaw);
    Vu0Cur_Translate(&p->pos);
    Vu0Cur_Translate(&em->pos);
    Vu0Cur_MulVec4(&p->corner[0], &p->corner[0]);
    Vu0Cur_MulVec4(&p->corner[1], &p->corner[1]);
    Vu0Cur_MulVec4(&p->corner[2], &p->corner[2]);
    Vu0Cur_MulVec4(&p->corner[3], &p->corner[3]);
}

/* Appends a particle to a burst group. */
void EftPart10_LinkPtcl(EftPart10Ptcl **head, EftPart10Ptcl **tail, EftPart10Ptcl *p) {
    if (*head == NULL) {
        *head = p;
        *tail = p;
    } else {
        p->prev = *tail;
        (*tail)->next = p;
        *tail = p;
    }
}

/* Removes a particle from a burst group. */
void EftPart10_UnlinkPtcl(EftPart10Ptcl **head, EftPart10Ptcl **tail, EftPart10Ptcl *p) {
    EftPart10Ptcl *prev;
    EftPart10Ptcl *a; /* one variable for the list head and later for a neighbour: needed for the registers */
    EftPart10Ptcl *b;

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


/* Queues a particle as a textured strip through its four world-space corners (projected with the loaded
   matrix; dropped when the projection clips), at the average depth of the corners. */
void EftPart10_DrawQuad(Vec4 *corner, EftXVec uv0, EftXVec uv1, EftXVec color, s32 layer, s32 texIdx, s32 noDepth,
                        EftXTexEntry *tex) {
    Vec4 uv[4];
    Vec4 stq[4];
    EftXIVec scr[4];
    EftXIVec col;
    EftXStripPkt *p;
    OtEntry *e;
    s32 abe = 1;
    s32 ctx;
    s32 z;
    s32 l;

    Vec4_Set(&uv[0], uv0.x, uv0.y, 1.0f, 1.0f);
    Vec4_Set(&uv[1], uv0.z, uv0.w, 1.0f, 1.0f);
    Vec4_Set(&uv[2], uv1.x, uv1.y, 1.0f, 1.0f);
    Vec4_Set(&uv[3], uv1.z, uv1.w, 1.0f, 1.0f);
    if (!Vu0Cur_ProjectPointsStq(scr, stq, corner, uv, 4)) {
        return;
    }
    p = (EftXStripPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    if (p == NULL) {
        return;
    }
    ctx = layer >= 2;
    p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x1C;
    p->dmaTag = 0x20000008;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000008;
    p->gifTag = 0xE400000000008001;
    p->regs = 0x42142142142160 + (ctx << 4);
    p->next = NULL;
    z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
    if (noDepth) {
        scr[0].z = 0xFFFFFF;
        scr[1].z = 0xFFFFFF;
        scr[2].z = 0xFFFFFF;
        scr[3].z = 0xFFFFFF;
    }
    Vec4_ToInt(&col, V(&color));
    p->v[0].rgbaq.r = col.x;
    p->v[0].rgbaq.g = col.y;
    p->v[0].rgbaq.b = col.z;
    p->v[0].rgbaq.a = col.w;
    p->v[0].rgbaq.q = stq[0].z;
    p->v[1].rgbaq.r = col.x;
    p->v[1].rgbaq.g = col.y;
    p->v[1].rgbaq.b = col.z;
    p->v[1].rgbaq.a = col.w;
    p->v[1].rgbaq.q = stq[1].z;
    p->v[2].rgbaq.r = col.x;
    p->v[2].rgbaq.g = col.y;
    p->v[2].rgbaq.b = col.z;
    p->v[2].rgbaq.a = col.w;
    p->v[2].rgbaq.q = stq[2].z;
    p->v[3].rgbaq.r = col.x;
    p->v[3].rgbaq.g = col.y;
    p->v[3].rgbaq.b = col.z;
    p->v[3].rgbaq.a = col.w;
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
    p->tex0 = EFT_X_TEX0(tex, texIdx);
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

/* The same quad through the clipping polygon path: two triangles (corners 0 1 2 and 1 2 3). */
void EftPart10_DrawQuadClipped(Vec4 *corner, EftXVec uv0, EftXVec uv1, EftXVec color, s32 layer, s32 texIdx,
                               s32 noDepth, EftXTexEntry *tex) {
    Vec4 c[4];
    Vec4 uv[4];
    EftXVert v[9];

    Vec4_Set(&uv[0], uv0.x, uv0.y, 1.0f, 1.0f);
    Vec4_Set(&uv[1], uv0.z, uv0.w, 1.0f, 1.0f);
    Vec4_Set(&uv[2], uv1.x, uv1.y, 1.0f, 1.0f);
    Vec4_Set(&uv[3], uv1.z, uv1.w, 1.0f, 1.0f);
    Vec4_Copy(&c[0], &corner[0]);
    Vec4_Copy(&c[1], &corner[1]);
    Vec4_Copy(&c[2], &corner[2]);
    Vec4_Copy(&c[3], &corner[3]);
    ClipVtx_Set(&v[0], &c[0], &uv[0], V(&color));
    ClipVtx_Set(&v[1], &c[1], &uv[1], V(&color));
    ClipVtx_Set(&v[2], &c[2], &uv[2], V(&color));
    EftGfx_DrawPolyScaledZ(v, layer, 1, 0, noDepth, 0, EFT_X_TEX0(tex, texIdx), 2.0f);
    ClipVtx_Set(&v[0], &c[1], &uv[1], V(&color));
    ClipVtx_Set(&v[1], &c[2], &uv[2], V(&color));
    ClipVtx_Set(&v[2], &c[3], &uv[3], V(&color));
    EftGfx_DrawPolyScaledZ(v, layer, 1, 0, noDepth, 0, EFT_X_TEX0(tex, texIdx), 2.0f);
}

/* Queues a particle that faces the camera through the clipping polygon path: a w x h rectangle (half sizes)
   stretched by `scale`, turned by `rot` in the view plane and placed at `pos`; two triangles at the depth slot
   of `pos` times zScale. Nothing is drawn when `pos` is behind the near plane. */
void EftPart10_DrawBillboardClipped(Vec4 *pos, f32 w, f32 h, Vec4 *color, Vec4 *scale, f32 u0, f32 v0, f32 u1,
                                    f32 v1, f32 rot, s32 layer, s32 noDepth, u64 tex0, f32 zScale) {
    EftXVert v[9];
    Mtx44 m;
    Mtx44 inv;
    Vec4 c[4];
    Vec4 uv[4];
    EftXIVec scr;
    s32 i;
    s32 z;

    Vec4_Set(&c[0], -w, -h, 0.0f, 1.0f);
    Vec4_Set(&c[1], w, -h, 0.0f, 1.0f);
    Vec4_Set(&c[2], -w, h, 0.0f, 1.0f);
    Vec4_Set(&c[3], w, h, 0.0f, 1.0f);
    Mtx_Translate(&m, &gEftPart10Mgr->ident, scale);
    Mtx_RotateZ(&m, &m, rot);
    Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
    inv.m[3][0] = 0.0f;
    inv.m[3][1] = 0.0f;
    inv.m[3][2] = 0.0f;
    Mtx_Mul(&m, &inv, &m);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&c[i], &m, &c[i]);
        Vec3_Add(&c[i], &c[i], pos);
        c[i].w = 1.0f;
    }
    Vu0Cur_ProjectPoint(&scr, pos);
    z = scr.z >> 8;
    if (scr.z >= 0) {
        Vec4_Set(&uv[0], u0, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[1], u1, v0, 1.0f, 0.0f);
        Vec4_Set(&uv[2], u0, v1, 1.0f, 0.0f);
        Vec4_Set(&uv[3], u1, v1, 1.0f, 0.0f);
        for (i = 0; i < 2; i++) {
            ClipVtx_Set(&v[0], &c[i], &uv[i], color);
            ClipVtx_Set(&v[1], &c[i + 1], &uv[i + 1], color);
            ClipVtx_Set(&v[2], &c[i + 2], &uv[i + 2], color);
            EftGfx_DrawPolyFixedZ(v, layer, 0, 0, noDepth, 0, tex0, z * zScale);
        }
    }
}

/* Queues a particle as a screen-aligned textured quad around the projection of `pos`: w x h are half sizes and
   (offX, offY) the centre offset, all in 1/16 world units scaled by the perspective of `pos`; `rot` turns it
   about the view axis. Dropped when smaller than 2 units or when a corner leaves the GS drawing area.
   The order in which integer and float parameters are interleaved is not known (they travel in separate
   registers); this one reproduces the original's register allocation. */
void EftPart10_DrawBillboard(Vec4 *pos, Vec4 *color, f32 w, s32 offX, s32 offY, f32 h, f32 u0, f32 v0, s32 layer,
                             f32 u1, f32 v1, s32 noDepth, u64 tex0, f32 rot) {
    Mtx44 m;
    Vec4 aspect;
    Vec4 a;
    Vec4 b;
    Vec4 c;
    Vec4 d;
    EftXIVec scr;
    EftXIVec ia;
    EftXIVec ib;
    EftXIVec ic;
    EftXIVec id;
    f32 scale = gBtlCamView->screenDist;
    s32 sw;
    s32 sh;
    s32 k;
    s32 oy;
    s32 z;
    s32 zz;
    s32 x0, y0, x1, y1, x2, y2, x3, y3;
    s32 abe = 1;
    s32 ctx;
    s32 l;
    u32 alpha;
    EftXQuadPkt *p;
    OtEntry *e;

#ifdef PORT
    Vec4_Set(&aspect, 1.0f, 1.1666667f * Port_WideFactor(), 1.0f, 1.0f); /* pixels are wider still at 16:9 */
#else
    Vec4_Set(&aspect, 1.0f, 1.1666667f, 1.0f, 1.0f);
#endif
    sw = w;
    pos->w = 1.0f;
    Vu0Cur_ProjectPoint(&scr, pos);
    scale *= 4096.0f;
    z = scr.z >> 8;
    sh = h;
    scale /= scr.w;
    k = scale;
    sw = (sw * k) >> 12;
    sh = (sh * k) >> 12;
    if (offX >= 0) {
        offX = (offX * k) >> 12;
    } else {
        offX = -((-offX * k) >> 12);
    }
    if (offY >= 0) {
        oy = (offY * k) >> 12;
    } else {
        oy = -((-offY * k) >> 12);
    }
    if (sw < 2) {
        return;
    }
    if (sh < 2) {
        return;
    }
    Vec4_Set(&a, offX - sw, oy - sh, 0.0f, 1.0f);
    Vec4_Set(&b, offX - sw, sh + oy, 0.0f, 1.0f);
    Vec4_Set(&c, sw + offX, oy - sh, 0.0f, 1.0f);
    Vec4_Set(&d, sw + offX, sh + oy, 0.0f, 1.0f);
    Mtx_RotateZ(&m, &gEftPart10Mgr->ident, rot);
    Mtx_MulVec4(&a, &m, &a);
    Mtx_MulVec4(&b, &m, &b);
    Mtx_MulVec4(&c, &m, &c);
    Mtx_MulVec4(&d, &m, &d);
    Vec4_Mul(&a, &a, &aspect);
    Vec4_Mul(&b, &b, &aspect);
    Vec4_Mul(&c, &c, &aspect);
    Vec4_Mul(&d, &d, &aspect);
    Vec4_ToInt(&ia, &a);
    Vec4_ToInt(&ib, &b);
    Vec4_ToInt(&ic, &c);
    Vec4_ToInt(&id, &d);
    if (EftPart10_IsCornerOffScreen(scr.x + ia.x, scr.y + ia.y, scr.z)) {
        return;
    }
    if (EftPart10_IsCornerOffScreen(scr.x + ib.x, scr.y + ib.y, scr.z)) {
        return;
    }
    if (EftPart10_IsCornerOffScreen(scr.x + ic.x, scr.y + ic.y, scr.z)) {
        return;
    }
    if (EftPart10_IsCornerOffScreen(scr.x + id.x, scr.y + id.y, scr.z)) {
        return;
    }
    if (noDepth) {
        scr.z = 0xFFFFFF;
    }
    zz = (scr.z >> 8) << 8;
    x0 = scr.x + ia.x;
    y0 = scr.y + ia.y;
    x1 = scr.x + ib.x;
    y1 = scr.y + ib.y;
    x2 = scr.x + ic.x;
    y2 = scr.y + ic.y;
    x3 = scr.x + id.x;
    y3 = scr.y + id.y;
    p = (EftXQuadPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    if (p == NULL) {
        return;
    }
    ctx = layer >= 2;
    p->prim = ((u64)abe << 6) | ((u64)ctx << 9) | 0x14;
    p->dmaTag = 0x20000007;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000007;
    p->gifTag = 0xC400000000008001;
    p->regs = 0xF42424242160 + (ctx << 4);
    p->next = NULL;
    p->rgbaq.r = color->x;
    p->rgbaq.g = color->y;
    p->rgbaq.b = color->z;
    alpha = color->w;
    p->xyz0.x = x0;
    p->xyz0.y = y0;
    l = layer;
    p->xyz1.x = x1;
    p->xyz1.y = y1;
    p->xyz2.x = x2;
    p->xyz2.y = y2;
    p->xyz3.x = x3;
    p->xyz3.y = y3;
    p->xyz0.z = zz;
    p->xyz1.z = zz;
    p->xyz2.z = zz;
    p->xyz3.z = zz;
    if (l >= 2) {
        l -= 2;
    }
    p->rgbaq.a = alpha;
    p->rgbaq.q = 1.0f;
    p->st0.s = u0;
    p->st0.t = v0;
    p->st1.s = u0;
    p->st1.t = v1;
    p->st2.s = u1;
    p->st2.t = v0;
    p->st3.s = u1;
    p->st3.t = v1;
    p->xyz0.f = 0xFF;
    p->xyz1.f = 0xFF;
    p->xyz2.f = 0xFF;
    p->xyz3.f = 0xFF;
    p->tex0 = tex0;
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

/* Draws the emitter's spin and scale: with definition flag 0x20 one random value each, otherwise three random
   keys turned into per-frame steps before and after spinMid. */
void EftPart10_InitSpin(EftPart10 *em) {
    EftPart10Def *def = em->def;
    f32 k[3];

    if (def->flags & 0x20) {
        em->spinA = Rand_FloatRange(def->spinA[0], def->spinA[0] + def->spinARange[0]) * 6.2831853f;
        em->spinB = Rand_FloatRange(def->spinB[0], def->spinB[0] + def->spinBRange[0]) * 6.2831853f;
        em->scale = Rand_FloatRange(def->scale[0], def->scale[0] + def->scaleRange[0]);
        em->count = def->count[0];
        return;
    }
    em->spinTime = def->spinTime * 30.0f;
    em->spinMid = em->spinTime * def->spinMid;
    k[0] = Rand_FloatRange(def->spinA[0], def->spinA[0] + def->spinARange[0]) * 6.2831853f;
    k[1] = Rand_FloatRange(def->spinA[1], def->spinA[1] + def->spinARange[1]) * 6.2831853f;
    k[2] = Rand_FloatRange(def->spinA[2], def->spinA[2] + def->spinARange[2]) * 6.2831853f;
    em->dSpinA[0] = (k[1] - k[0]) / em->spinMid;
    em->spinA = k[0];
    em->dSpinA[1] = (k[2] - k[1]) / (em->spinTime - em->spinMid);
    k[0] = Rand_FloatRange(def->spinB[0], def->spinB[0] + def->spinBRange[0]) * 6.2831853f;
    k[1] = Rand_FloatRange(def->spinB[1], def->spinB[1] + def->spinBRange[1]) * 6.2831853f;
    k[2] = Rand_FloatRange(def->spinB[2], def->spinB[2] + def->spinBRange[2]) * 6.2831853f;
    em->dSpinB[0] = (k[1] - k[0]) / em->spinMid;
    em->spinB = k[0];
    em->dSpinB[1] = (k[2] - k[1]) / (em->spinTime - em->spinMid);
    k[0] = Rand_FloatRange(def->scale[0], def->scale[0] + def->scaleRange[0]);
    k[1] = Rand_FloatRange(def->scale[1], def->scale[1] + def->scaleRange[1]);
    k[2] = Rand_FloatRange(def->scale[2], def->scale[2] + def->scaleRange[2]);
    em->dScale[0] = (k[1] - k[0]) / em->spinMid;
    em->scale = k[0];
    em->dScale[1] = (k[2] - k[1]) / (em->spinTime - em->spinMid);
    em->dCount[0] = (def->count[1] - def->count[0]) / em->spinMid;
    em->dCount[1] = (def->count[2] - def->count[1]) / (em->spinTime - em->spinMid);
    em->count = def->count[0];
}

/* Steps the emitter's spin, scale and burst count along their keys and turns the emitter by the spin. */
void EftPart10_UpdateSpin(EftPart10 *em) {
    if (!(em->def->flags & 0x20)) {
        if (em->age < em->spinMid) {
            em->spinA += em->dSpinA[0];
            em->spinB += em->dSpinB[0];
            em->scale += em->dScale[0];
            em->count += em->dCount[0];
        } else if (em->age < em->spinTime) {
            em->spinA += em->dSpinA[1];
            em->spinB += em->dSpinB[1];
            em->scale += em->dScale[1];
            em->count += em->dCount[1];
        }
    }
    em->rotA += em->spinA;
    em->rotB += em->spinB;
    em->rotA = EftMath_WrapAngle(em->rotA);
    em->rotB = EftMath_WrapAngle(em->rotB);
}

/* Starts a segment of the emitter's key-frame animation: at age 0 the first (keys 0 to 1), once the age
   reaches keyMid the second (keys 1 to 2). Stores the change of every value over the segment. */
void EftPart10_StartKeys(EftPart10 *em) {
    s32 seg = 0;
    EftPart10Def *def = em->def;
    EftPart10Def2 *def2 = em->def2;

    if (em->age <= 0.0f) {
        seg = 1;
        em->keyTime = def->keyTime * 30.0f;
        em->keyMid = em->keyTime * def->keyMid;
    }
    if (em->keyMid <= em->age && !(em->flags & EFT_PART10_KEY2)) {
        seg = 2;
        em->flags |= EFT_PART10_KEY2;
    }
    if (seg > 0) {
        em->dSize[0] = def->size[seg][0] - def->size[seg - 1][0];
        em->dSize[1] = def->size[seg][1] - def->size[seg - 1][1];
        em->dSize[2] = def->size[seg][2] - def->size[seg - 1][2];
        em->dSizeRange[0] = def->sizeRange[seg][0] - def->sizeRange[seg - 1][0];
        em->dSizeRange[1] = def->sizeRange[seg][1] - def->sizeRange[seg - 1][1];
        em->dSizeRange[2] = def->sizeRange[seg][2] - def->sizeRange[seg - 1][2];
        em->angle.d = def->angle[seg] - def->angle[seg - 1];
        em->angleRange.d = def->angleRange[seg] - def->angleRange[seg - 1];
        em->unkC0.d = def->unkC0[seg] - def->unkC0[seg - 1];
        em->unkD0.d = def->unkD0[seg] - def->unkD0[seg - 1];
        em->spin.d = def->spin[seg] - def->spin[seg - 1];
        em->unkC8.d = def->unkC8[seg] - def->unkC8[seg - 1];
        em->unkD8.d = def->unkD8[seg] - def->unkD8[seg - 1];
        em->spinRange.d = def->spinRange[seg] - def->spinRange[seg - 1];
        em->unkF0.d = def->unkF0[seg] - def->unkF0[seg - 1];
        em->unkF8.d = def->unkF8[seg] - def->unkF8[seg - 1];
        em->unk100.d = def->unk100[seg] - def->unk100[seg - 1];
        em->unk108.d = def->unk108[seg] - def->unk108[seg - 1];
        em->speed.d = def->speed[seg] - def->speed[seg - 1];
        em->speedRange.d = def->speedRange[seg] - def->speedRange[seg - 1];
        em->speedBase.d = def->speedBase[seg] - def->speedBase[seg - 1];
        em->unk128.d = def->unk128[seg] - def->unk128[seg - 1];
        em->unk130.d = def->unk130[seg] - def->unk130[seg - 1];
        em->scaleX.d = def->scaleX[seg] - def->scaleX[seg - 1];
        em->scaleY.d = def->scaleY[seg] - def->scaleY[seg - 1];
        em->life.d = def->life[seg] - def->life[seg - 1];
        em->lifeRange.d = def->lifeRange[seg] - def->lifeRange[seg - 1];
        em->unk158.d = def->unk158[seg] - def->unk158[seg - 1];
        em->unk160.d = def->unk160[seg] - def->unk160[seg - 1];
        em->dUnk168.a = def->unk168[seg][0] - def->unk168[seg - 1][0];
        em->dUnk168.b = def->unk168[seg][1] - def->unk168[seg - 1][1];
        em->unk178.d = def->unk178[seg] - def->unk178[seg - 1];
        em->dStretchX.a = def2->stretchX[seg][0] - def2->stretchX[seg - 1][0];
        em->dStretchX.b = def2->stretchX[seg][1] - def2->stretchX[seg - 1][1];
        em->dStretchY.a = def2->stretchY[seg][0] - def2->stretchY[seg - 1][0];
        em->dStretchY.b = def2->stretchY[seg][1] - def2->stretchY[seg - 1][1];
        em->dStretchZ.a = def2->stretchZ[seg][0] - def2->stretchZ[seg - 1][0];
        em->dStretchZ.b = def2->stretchZ[seg][1] - def2->stretchZ[seg - 1][1];
        em->stretchTime.d = def2->stretchTime[seg] - def2->stretchTime[seg - 1];
        em->unk248.d = def2->unk248[seg] - def2->unk248[seg - 1];
        em->unk250.d = def2->unk250[seg] - def2->unk250[seg - 1];
        Vec4_Sub(&em->dColor, &def2->color[seg], &def2->color[seg - 1]);
        Vec4_Sub(&em->dEndColor, &def2->endColor[seg], &def2->endColor[seg - 1]);
        Vec4_Sub(&em->dColorRange, &def2->colorRange[seg], &def2->colorRange[seg - 1]);
        Vec4_Sub(&em->dEndColorRange, &def2->endColorRange[seg], &def2->endColorRange[seg - 1]);
    }
}

/* Advances the key-frame animation: every value = its key at the start of the segment + change * progress. */
/* The temporary is reached through a pointer variable (`p`); with `&tmp` at every use the `seg = 0` store is
   scheduled in front of the memset call instead of behind it (presumably the pointer's own set takes that
   scheduler slot and reload deletes it afterwards: not verified). The clamp is the conditional expression. */
void EftPart10_UpdateKeys(EftPart10 *em) {
    Vec4 tmp;
    s32 seg;
    EftPart10Def *def = em->def;
    EftPart10Def2 *def2 = em->def2;
    f32 t;
    Vec4 *p = &tmp;

    memset(p, 0, sizeof(tmp));
    seg = 0;
    if (!(em->flags & EFT_PART10_KEY2)) {
        t = em->age / em->keyMid;
    } else {
        seg = 1;
        t = (em->age - em->keyMid) / (em->keyTime - em->keyMid);
    }
    t = (t < 0.0f) ? 0.0f : (1.0f < t) ? 1.0f : t;
    em->kSize[0] = def->size[seg][0] + em->dSize[0] * t;
    em->kSize[1] = def->size[seg][1] + em->dSize[1] * t;
    em->kSize[2] = def->size[seg][2] + em->dSize[2] * t;
    em->kSizeRange[0] = def->sizeRange[seg][0] + em->dSizeRange[0] * t;
    em->kSizeRange[1] = def->sizeRange[seg][1] + em->dSizeRange[1] * t;
    em->kSizeRange[2] = def->sizeRange[seg][2] + em->dSizeRange[2] * t;
    em->angle.v = def->angle[seg] + em->angle.d * t;
    em->angleRange.v = def->angleRange[seg] + em->angleRange.d * t;
    em->unkC0.v = def->unkC0[seg] + em->unkC0.d * t;
    em->unkD0.v = def->unkD0[seg] + em->unkD0.d * t;
    em->spin.v = def->spin[seg] + em->spin.d * t;
    em->unkC8.v = def->unkC8[seg] + em->unkC8.d * t;
    em->unkD8.v = def->unkD8[seg] + em->unkD8.d * t;
    em->spinRange.v = def->spinRange[seg] + em->spinRange.d * t;
    em->unkF0.v = def->unkF0[seg] + em->unkF0.d * t;
    em->unk100.v = def->unk100[seg] + em->unk100.d * t;
    em->unkF8.v = def->unkF8[seg] + em->unkF8.d * t;
    em->unk108.v = def->unk108[seg] + em->unk108.d * t;
    em->speed.v = def->speed[seg] + em->speed.d * t;
    em->speedRange.v = def->speedRange[seg] + em->speedRange.d * t;
    em->speedBase.v = def->speedBase[seg] + em->speedBase.d * t;
    em->unk128.v = def->unk128[seg] + em->unk128.d * t;
    em->unk130.v = def->unk130[seg] + em->unk130.d * t;
    em->scaleX.v = def->scaleX[seg] + em->scaleX.d * t;
    em->scaleY.v = def->scaleY[seg] + em->scaleY.d * t;
    em->life.v = def->life[seg] + em->life.d * t;
    em->lifeRange.v = def->lifeRange[seg] + em->lifeRange.d * t;
    em->unk158.v = def->unk158[seg] + em->unk158.d * t;
    em->unk160.v = def->unk160[seg] + em->unk160.d * t;
    em->unk168.a = def->unk168[seg][0] + em->dUnk168.a * t;
    em->unk168.b = def->unk168[seg][1] + em->dUnk168.b * t;
    em->unk178.v = def->unk178[seg] + em->unk178.d * t;
    em->stretchX.a = def2->stretchX[seg][0] + em->dStretchX.a * t;
    em->stretchX.b = def2->stretchX[seg][1] + em->dStretchX.b * t;
    em->stretchY.a = def2->stretchY[seg][0] + em->dStretchY.a * t;
    em->stretchY.b = def2->stretchY[seg][1] + em->dStretchY.b * t;
    em->stretchZ.a = def2->stretchZ[seg][0] + em->dStretchZ.a * t;
    em->stretchZ.b = def2->stretchZ[seg][1] + em->dStretchZ.b * t;
    em->stretchTime.v = def2->stretchTime[seg] + em->stretchTime.d * t;
    em->unk248.v = def2->unk248[seg] + em->unk248.d * t;
    em->unk250.v = def2->unk250[seg] + em->unk250.d * t;
    Vec3_Scale(p, &em->dColor, t);
    Vec4_Add(&em->color, &def2->color[seg], p);
    Vec3_Scale(p, &em->dEndColor, t);
    Vec4_Add(&em->endColor, &def2->endColor[seg], p);
    Vec3_Scale(p, &em->dColorRange, t);
    Vec4_Add(&em->colorRange, &def2->colorRange[seg], p);
    Vec3_Scale(p, &em->dEndColorRange, t);
    Vec4_Add(&em->endColorRange, &def2->endColorRange[seg], p);
}

/* Sets every animated value of the emitter to key `idx` of its definition. */
void EftPart10_SetKey(EftPart10 *em, s32 idx) {
    EftPart10Def *def = em->def;
    EftPart10Def2 *def2;

    em->kSize[0] = def->size[idx][0];
    em->kSize[1] = def->size[idx][1];
    em->kSize[2] = def->size[idx][2];
    em->kSizeRange[0] = def->sizeRange[idx][0];
    em->kSizeRange[1] = def->sizeRange[idx][1];
    em->kSizeRange[2] = def->sizeRange[idx][2];
    def2 = em->def2;
    em->angle.v = def->angle[idx];
    em->angleRange.v = def->angleRange[idx];
    em->unkC0.v = def->unkC0[idx];
    em->unkC8.v = def->unkC8[idx];
    em->unkD0.v = def->unkD0[idx];
    em->unkD8.v = def->unkD8[idx];
    em->spin.v = def->spin[idx];
    em->spinRange.v = def->spinRange[idx];
    em->unkF0.v = def->unkF0[idx];
    em->unkF8.v = def->unkF8[idx];
    em->unk100.v = def->unk100[idx];
    em->unk108.v = def->unk108[idx];
    em->speed.v = def->speed[idx];
    em->speedRange.v = def->speedRange[idx];
    em->speedBase.v = def->speedBase[idx];
    em->unk128.v = def->unk128[idx];
    em->unk130.v = def->unk130[idx];
    em->scaleX.v = def->scaleX[idx];
    em->scaleY.v = def->scaleY[idx];
    em->life.v = def->life[idx];
    em->lifeRange.v = def->lifeRange[idx];
    em->unk158.v = def->unk158[idx];
    em->unk160.v = def->unk160[idx];
    em->unk168.a = def->unk168[idx][0];
    em->unk168.b = def->unk168[idx][1];
    em->unk178.v = def->unk178[idx];
    em->stretchX.a = def2->stretchX[idx][0];
    em->stretchX.b = def2->stretchX[idx][1];
    em->stretchY.a = def2->stretchY[idx][0];
    em->stretchY.b = def2->stretchY[idx][1];
    em->stretchZ.a = def2->stretchZ[idx][0];
    em->stretchZ.b = def2->stretchZ[idx][1];
    em->stretchTime.v = def2->stretchTime[idx];
    em->unk248.v = def2->unk248[idx];
    em->unk250.v = def2->unk250[idx];
    Vec4_Copy(&em->color, &def2->color[idx]);
    Vec4_Copy(&em->endColor, &def2->endColor[idx]);
    Vec4_Copy(&em->colorRange, &def2->colorRange[idx]);
    Vec4_Copy(&em->endColorRange, &def2->endColorRange[idx]);
}

/* Chooses the emitter's two pack textures; the blended TEX0 goes to entry a + b of the table (b when equal). */
void EftPart10_SelectTex(EftPart10 *em, EftXTexEntry *tbl, s32 a, s32 b) {
    if (a == b) {
        em->texSlot = b;
    } else {
        em->texSlot = a + b;
    }
    em->texA = tbl[a];
    em->texB = tbl[b];
}

/* Builds the emitter's TEX0 (texture A with the CLUT of texture B) once per frame into its table entry. */
void EftPart10_BuildTex(EftPart10 *em, EftXTexSet *tex) {
    if (tex != NULL) {
        if (!(tex->ready & (1 << em->texSlot))) {
            u64 t = EftVram_AddImage(&em->texA, 1, 0);

            t |= (u64)EftVram_AddClut(&em->texB) << 37;
            tex->entry[em->texSlot].tex0 = t;
            tex->ready |= 1 << em->texSlot;
        }
    }
}

/* Creates an emitter task. Returns the task, the handle the other calls take. */
EftXTask *EftPart10_Create(EftPart10Arg *arg) {
    if (gEftPart10Mgr == NULL) {
        return NULL;
    }
    if (arg == NULL) {
        return NULL;
    }
    return BtlTaskList_AddTail(gEftPart10Mgr->list, gEftPart10Class, arg);
}

/* Lets the emitter finish: it keeps emitting for the hold time, then fades for the fade time, then stops
   emitting; the task dies when its last particle is gone. */
void EftPart10_Stop(EftXTask *task) {
    EftPart10 *em;
    s32 now;

    now = 1;
    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_STOPPED) {
        return;
    }
    if (!(em->flags & EFT_PART10_ALIVE)) {
        return;
    }
    if (0.0f < em->hold) {
        em->flags |= EFT_PART10_HOLD;
        now = 0;
    }
    if (0.0f < em->fadeTime) {
        em->flags |= EFT_PART10_FADE;
        now = 0;
    }
    if (now) {
        em->flags |= EFT_PART10_NOEMIT;
    }
    em->flags |= EFT_PART10_STOPPED;
}

/* Sets the length of the fade that follows EftPart10_Stop, in frames. */
void EftPart10_SetFade(EftXTask *task, s32 frames) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_STOPPED) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->fadeTime = frames;
        em->fade = frames;
    }
}

/* Makes the emitter and its particles disappear at its next update. */
void EftPart10_Kill(EftXTask *task) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->flags |= EFT_PART10_KILL;
    }
}

/* Moves the emitter; particles already flying stay where they are. */
void EftPart10_SetPos(EftXTask *task, EftXVec pos) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        Vec4_Copy(&em->pos, V(&pos));
    }
}

/* Moves the emitter and translates the corners of every live particle by the new position. */
void EftPart10_Warp(EftXTask *task, EftXVec pos) {
    EftPart10 *em;
    EftPart10Group *g;
    EftPart10Ptcl *p;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (!(em->flags & EFT_PART10_ALIVE)) {
        return;
    }
    Vu0Cur_Push();
    Vec4_Copy(&em->pos, V(&pos));
    for (g = em->groupHead; g != NULL; g = g->next) {
        for (p = g->head; p != NULL; p = p->next) {
            Vu0Cur_LoadIdentity();
            Vu0Cur_Translate(V(&pos));
            Vu0Cur_MulVec4(&p->corner[0], &p->corner[0]);
            Vu0Cur_MulVec4(&p->corner[1], &p->corner[1]);
            Vu0Cur_MulVec4(&p->corner[2], &p->corner[2]);
            Vu0Cur_MulVec4(&p->corner[3], &p->corner[3]);
        }
    }
    Vu0Cur_Pop();
}

/* Sets the emitter's axis. */
void EftPart10_SetDir(EftXTask *task, EftXVec dir) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        Vec4_Copy(&em->dir, V(&dir));
    }
}

/* Sets the emitter's size factor. */
void EftPart10_SetSize(EftXTask *task, f32 size) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->size = size;
    }
}

/* Sets the emitter's life in seconds (0 = until stopped). No caller. */
void EftPart10_SetRate(EftXTask *task, f32 rate) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->rate = rate;
    }
}

/* Changes the emitter's texture table and its two textures. No caller. */
void EftPart10_SetTex(EftXTask *task, EftXTexSet *tex, s32 a, s32 b) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->tex = tex;
        em->texIdxA = a;
        em->texIdxB = b;
        EftPart10_SelectTex(em, (EftXTexEntry *)tex, a, b);
    }
}

/* Sets the number of frames before the emitter starts. */
void EftPart10_SetDelay(EftXTask *task, s32 frames) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->delay = frames;
    }
}

/* Sets the number of frames the emitter keeps emitting after EftPart10_Stop. */
void EftPart10_SetHold(EftXTask *task, s32 frames) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return;
    }
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftPart10_Update) {
        return;
    }
    em = task->work;
    if (em == NULL) {
        return;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->hold = frames;
    }
}

/* Makes the emitter's particles ignore depth (drawn with the far depth value). */
s32 EftPart10_SetNoDepth(EftXTask *task) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftPart10_Update) {
        return 0;
    }
    em = task->work;
    if (em == NULL) {
        return 0;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->flags |= EFT_PART10_NODEPTH;
    }
    return 1;
}

/* Sets the effect kind the scene's "effects stopped / hidden" tests are asked about. */
s32 EftPart10_SetKind(EftXTask *task, s32 kind) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftPart10_Update) {
        return 0;
    }
    em = task->work;
    if (em == NULL) {
        return 0;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        em->kind = kind;
    }
    return 1;
}

/* Non-zero while the handle is a live emitter of this module. */
s32 EftPart10_IsAlive(EftXTask *task) {
    EftPart10 *em;

    if (gEftPart10Mgr == NULL) {
        return 0;
    }
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftPart10_Update) {
        return 0;
    }
    em = task->work;
    if (em == NULL) {
        return 0;
    }
    if (em->flags & EFT_PART10_ALIVE) {
        return 1;
    }
    return 0;
}

/* Non-zero when a GS screen position is outside the drawing area (or behind the near plane). */
s32 EftPart10_IsCornerOffScreen(s32 x, s32 y, s32 z) {
    if (z <= 0) {
        return 1;
    }
    if (x > 0xFFEF) {
        return 1;
    }
    if (x <= 0) {
        return 1;
    }
    if (y > 0xFFEF) {
        return 1;
    }
    if (y <= 0) {
        return 1;
    }
    return 0;
}
