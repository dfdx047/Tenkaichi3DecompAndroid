#include "common.h"
#include "battle/eft_p.h"
#include "sys/gfx_ot.h"

/*
 * Power-up glow, first part of the module: 0x16DCA0..0x170A50 (22 functions, 21 matching; EftGlow_DrawParts is
 * left in assembly with the attempt in `#if 0`). The glow is what the fighter effect
 * layer starts with requests 4 / 5 (BtlFx_UpdatePowerUpLook); its tasks, manager and entry points are in eft_q.c
 * (EftGlow*), which calls EftGlow_BuildTables, EftGlow_Begin, EftGlow_Step, EftGlow_StepParts, EftGlow_FreeParts
 * and EftGlow_DrawParts of this file.
 *
 * VISUAL ONLY. Nothing here writes fighter, battle object or hit record state. What it reads from the fighter:
 * node positions, direction, rotation, fall speed, frame movement, height, the "action effect kind" and the
 * circle dash test (which only choose the order table layer of a quad).
 *
 * How it works (verified by the matching code unless marked):
 *   - gEftGlow owns a pool of quads (0xC0 bytes, 70 handed out lazily, then a free list) and two profile tables
 *     built once: 20 points sampled from a quadratic spline through the 13 control points of the configuration,
 *     and 20 unit directions from 20 to 115 degrees.
 *   - An emitter (the task's work, one per fighter) spawns up to 14 quads per frame while fewer than 0x41 are
 *     alive, walking the ten configured fighter nodes four quads at a time; angles around the fighter's axis come
 *     from libc rand().
 *   - A quad follows its node, is placed along the profile, is rotated about the axis and by the emitter's
 *     orientation of `frame` frames ago, leans by the fighter's frame movement, animates its width / length in
 *     two stages, fades in and out, and is drawn as a camera-facing textured strip of four vertices.
 *
 * Callees named by address: Mtx_Copy matrix copy; Mtx_RotateZ / Mtx_RotateX / Mtx_RotateY rotate about
 * Z / X / Y; Vec3_Set sets x, y, z; Vec4_Div divides a vector; Vec4_Clamp clamps each component;
 * Vu0Cur_ProjectPoint projects a point to GS screen coordinates.
 */

extern s32 rand(void);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 asinf(f32 x);
extern f32 atan2f(f32 y, f32 x);

extern void Vec4_Copy(void *dst, void *src);
extern void Vec4_Set(EftPVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Add(EftPVec *dst, EftPVec *a, EftPVec *b);
extern void Vec4_Sub(EftPVec *dst, EftPVec *a, EftPVec *b);
extern void Vec4_Scale(EftPVec *dst, EftPVec *src, f32 scale);
extern void Vec3_Sub(EftPVec *dst, EftPVec *a, EftPVec *b);
extern void Vec3_Scale(EftPVec *dst, EftPVec *src, f32 scale);
extern void Vec3_Normalize(EftPVec *dst, EftPVec *src);
extern void Vec3_Cross(EftPVec *dst, EftPVec *a, EftPVec *b);
extern f32 Vec3_Dot(EftPVec *a, EftPVec *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(EftPVec *dst, void *m, EftPVec *src);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Vec3_Set(EftPVec *dst, f32 x, f32 y, f32 z);
extern void Vec4_Div(EftPVec *dst, EftPVec *src, f32 div);
extern void Vec4_Clamp(EftPVec *dst, EftPVec *src, f32 lo, f32 hi);
extern s32 Vu0Cur_ProjectPoint(EftPScr *out, EftPVec *pos); /* returns a value (unused here); `void` changes the callers' registers */

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, EftPVec *out);
extern void BtlCharApi_GetDir(s32 objId, EftPVec *out);
extern void BtlCharApi_GetRot(s32 objId, EftPVec *out);
extern void BtlCharApi_GetFrameMove(s32 objId, EftPVec *out);
extern f32 BtlCharApi_GetFallSpeed(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_GetActionFxKind(s32 objId);
extern s32 BtlCharApi_IsCircleDash(s32 objId);
extern s32 BtlScene_GetStageFlag(void);
extern s32 EftAura_IsVisible(s32 objId);

/* The view being drawn: only the view matrix is read. */
typedef struct EftPView {
    /* 0x00 */ u8 unk0[0x40];
    /* 0x40 */ Mtx44 view;
} EftPView;

extern EftPView *gBtlCamView;

/* GS XYZF2 register value. */
typedef struct EftPXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftPXyzf;

typedef struct EftPGsVtx {
    /* 0x00 */ u8 rgba[4];
    /* 0x04 */ f32 q;
    /* 0x08 */ f32 s;
    /* 0x0C */ f32 t;
    /* 0x10 */ EftPXyzf xyz;
} EftPGsVtx; /* size 0x18 */

/* Triangle strip of four vertices as queued in the order table (0x90 bytes). */
typedef struct EftPQuadPkt {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 next;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 gif0;
    /* 0x18 */ u64 gif1;
    /* 0x20 */ u64 prim;
    /* 0x28 */ u64 tex0;
    /* 0x30 */ EftPGsVtx v[4];
} EftPQuadPkt;

/* Links a packet into the chain of a depth slot (clamped to 0..0xFFF); layers 2 and 3 are 0 and 1. */
static inline void EftPOt_Add(OtPrim *p, s32 z, s32 layer) {
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

/* Quadratic spline point of three control points at t, w = 1 (the same curve as EftMath_Spline3). */
void EftGlow_Spline3(EftPVec *out, EftPVec *p, f32 t) {
    EftPMtx basis;
    EftPMtx pts;
    EftPVec tv;

    Vec4_Set(&basis.row[0], 0.0f, 0.0f, 0.0f, 0.0f);
    Vec4_Set(&basis.row[1], 0.0f, 1.0f, -2.0f, 1.0f);
    Vec4_Set(&basis.row[2], 0.0f, -3.0f, 4.0f, -1.0f);
    Vec4_Set(&basis.row[3], 0.0f, 2.0f, 0.0f, 0.0f);
    Vec4_Set(&pts.row[0], 0.0f, 0.0f, 0.0f, 0.0f);
    Vec4_Set(&pts.row[1], p[0].x, p[0].y, p[0].z, p[0].w);
    Vec4_Set(&pts.row[2], p[1].x, p[1].y, p[1].z, p[1].w);
    Vec4_Set(&pts.row[3], p[2].x, p[2].y, p[2].z, p[2].w);
    Vec4_Set(&tv, t * t * t, t * t, t, 1.0f);
    Mtx_MulVec4(&tv, &basis, &tv);
    Mtx_MulVec4(&tv, &pts, &tv);
    Vec4_Div(out, &tv, 2.0f);
    out->w = 1.0f;
}

/* The profile directions of gEftGlow seen as four floats WITHOUT the 16-byte alignment of EftPVec. The original
   forms the address of dir[i].y / .z as (gEftGlow + 4) + (i * 16 + 0x160) in front of the cosf / sinf calls:
   the compiler adds a member's offset to the base register first only when the alignment it knows for the
   element equals the alignment of the member's mode (4 for a float), so the element type was not 16-aligned
   where this was compiled. With EftPVec the offset stays in the store (`swc1 $f0,4(s0)`). */
typedef struct EftPVecU {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} EftPVecU;
typedef struct EftPGlowDirView {
    /* 0x000 */ u8 pad[0x160];
    /* 0x160 */ EftPVecU dir[20];
} EftPGlowDirView;
#define GLOWDIR (((EftPGlowDirView *)gEftGlow)->dir)

/* Builds the two profile tables of gEftGlow: 20 spline samples of the scaled control points, 20 directions. */
void EftGlow_BuildTables(void) {
    EftPVec pts[13];
    EftPVec win[4];
    EftPVec v;
    f32 tbl[20];
    f32 yMin = 20.0f;
    f32 yRange = 95.0f;
    f32 zMin = yMin;
    f32 zRange = yRange;
    s32 i;
    s32 j;

    for (i = 0; i < 13; i++) {
        Vec3_Scale(&pts[i], &gEftGlowCfg->pts[i], gEftGlowCfg->shapeScale);
    }
    Vec4_Copy(&win[0], &pts[0]);
    Vec4_Copy(&win[1], &pts[1]);
    Vec4_Copy(&win[2], &pts[2]);
    Vec4_Copy(&win[3], &pts[3]);
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 2; j++) {
            EftGlow_Spline3(&v, win, (f32)j * 0.5f);
            Vec4_Copy(&gEftGlow->point[i * 2 + j], &v);
        }
        Vec4_Copy(&win[0], &win[1]);
        Vec4_Copy(&win[1], &win[2]);
        Vec4_Copy(&win[2], &win[3]);
        Vec4_Copy(&win[3], &pts[4 + i]);
    }
    for (i = 0; i < 6; i++) {
        tbl[i] = (f32)i * 0.5f / 6.0f;
    }
    for (i = 6; i < 20; i++) {
        tbl[i] = (f32)(i - 6) * 0.5f / 14.0f + 0.5f;
    }
    for (i = 0; i < 20; i++) {
        GLOWDIR[i].x = 0.0f;
        GLOWDIR[i].y = -cosf((yMin + tbl[i] * yRange) * 3.14159265f / 180.0f);
        GLOWDIR[i].z = sinf((zMin + tbl[i] * zRange) * 3.14159265f / 180.0f);
        GLOWDIR[i].w = 1.0f;
        Vec3_Normalize(&gEftGlow->dir[i], &gEftGlow->dir[i]);
    }
}

/* Profile point idx scaled; its y is scaled again by the configuration. */
void EftGlow_GetShapePoint(EftPVec *out, s32 objId, s32 idx, f32 scale) {
    Vec3_Scale(out, &gEftGlow->point[idx], scale);
    out->y *= gEftGlowCfg2->pointY;
    out->w = 1.0f;
}

/* Profile direction idx with its y scaled. */
void EftGlow_GetShapeDir(EftPVec *out, s32 objId, s32 idx, f32 y) {
    Vec4_Copy(out, &gEftGlow->dir[idx]);
    out->y *= y;
}

/*
 * Orientation of the glow. Kinds other than 5: pitch from the fighter's direction (with the fall speed -1 / +1
 * added to its y for kinds 3 / 4) and yaw = the fighter's yaw + pi. Kind 5: the line from node 0x2E to node 3.
 */
void EftGlow_CalcFrame(Mtx44 *m, s32 objId, s32 kind) {
    EftPVec dir;
    EftPVec a;
    EftPVec rot;

    Mtx_StoreIdentity(m);
    if (kind != 5) {
        BtlCharApi_GetDir(objId, &dir);
        if (kind == 3) {
            dir.y += BtlCharApi_GetFallSpeed(objId) + -1.0f;
        } else if (kind == 4) {
            dir.y += BtlCharApi_GetFallSpeed(objId) + 1.0f;
        }
        Vec3_Normalize(&dir, &dir);
        rot.x = asinf(dir.y);
        Mtx_RotateX(m, m, rot.x);
        BtlCharApi_GetRot(objId, &rot);
        rot.y += 3.14159265f;
        if (rot.y > 3.14159265f) {
            rot.y -= 6.2831853f;
        }
        Mtx_RotateY(m, m, rot.y);
    } else {
        BtlCharApi_GetNodePos(objId, 0x2E, &a);
        BtlCharApi_GetNodePos(objId, 3, &dir);
        Vec3_Sub(&dir, &dir, &a);
        Vec3_Normalize(&dir, &dir);
        rot.x = asinf(-dir.y);
        rot.y = atan2f(dir.x, dir.z);
        Mtx_RotateX(m, m, rot.x);
        Mtx_RotateY(m, m, rot.y);
    }
}

/* What every quad's direction leans by this frame: a random length along node 0x30 -> node 3, plus the fighter's
   frame movement (scaled, each component clamped to +-0.5). */
void EftGlow_CalcDrift(EftPVec *out, s32 objId) {
    EftPVec a;
    EftPVec b;
    EftPVec d;
    f32 s = gEftGlowCfg2->driftBase + gEftGlowCfg2->driftRand * ((f32)rand() / 2147483647.0f);

    BtlCharApi_GetNodePos(objId, 0x30, &a);
    a.w = 0.0f;
    BtlCharApi_GetNodePos(objId, 3, &b);
    b.w = 0.0f;
    Vec4_Sub(&d, &b, &a);
    Vec3_Normalize(&d, &d);
    Vec4_Scale(&d, &d, s);
    BtlCharApi_GetFrameMove(objId, out);
    Vec3_Scale(out, out, gEftGlowCfg2->moveScale);
    Vec4_Clamp(out, out, -0.5f, 0.5f);
    Vec4_Add(out, out, &d);
    out->w = 0.0f;
}

/* Flags of a new quad from the emitter's state. */
s32 EftGlow_CalcPartFlags(EftPGlow *e, s32 pair, s32 odd) {
    s32 f = e->flags & 2;
    s32 tex = e->flags & 0x20;

    if (pair) {
        f |= 0x20;
    } else if (odd) {
        f |= 0x40;
    }
    if (e->kind == 3) {
        f |= 0x80;
    } else if (e->kind == 4) {
        f |= 0x100;
    }
    if (tex) {
        f |= 0x10;
    }
    return f;
}

/* Target alpha of a new quad. */
f32 EftGlow_GetAlpha(EftPGlow *e, s32 flags, s32 idx) {
    f32 a;

    if (flags & 2) {
        if (flags & 0x20) {
            a = e->alphaB;
        } else {
            a = e->alphaA;
        }
    } else if (flags & 0x20) {
        a = e->color[idx].w;
    } else {
        a = e->colorA.w;
    }
    if (flags & 8) {
        a *= 1.1f;
    }
    return a;
}

/* Length of a quad at a stage of its size animation (0 start, 1 middle, 2 end). */
f32 EftGlow_CalcLength(EftPGlowPart *p, s32 stage) {
    EftPGlowCfg2 *cfg = gEftGlowCfg2;
    f32 r = cfg->length[stage];
    f32 *mul = cfg->lengthMul;
    f32 *side = cfg->lengthSide;

    if (p->flags & 2) {
        r = cfg->trailLength;
        r *= cfg->trailLengthBy[p->pair][stage];
    } else if (p->flags & 0x20) {
        r *= side[2];
        if ((p->flags & 0x80) || (p->flags & 0x100)) {
            r *= gEftGlowCfg->nodeB[p->node].pairLength;
        } else {
            r *= gEftGlowCfg->node[p->node].pairLength;
        }
    } else {
        if (!(p->flags & 0x40)) {
            r *= side[0];
        }
        if ((p->flags & 0x80) || (p->flags & 0x100)) {
            r *= gEftGlowCfg->nodeB[p->node].length;
        } else {
            r *= gEftGlowCfg->node[p->node].length;
        }
    }
    if (p->flags & 0x10) {
        r *= mul[0];
    }
    if (p->flags & 8) {
        r *= mul[1];
    }
    return r;
}

/* Width of a quad at a stage of its size animation. */
f32 EftGlow_CalcWidth(EftPGlowPart *p, s32 stage) {
    EftPGlowCfg2 *cfg = gEftGlowCfg2;
    f32 r = cfg->width[stage];
    f32 *mul = cfg->widthMul;
    f32 *side = cfg->widthSide;

    if (p->flags & 2) {
        r = cfg->trailWidth;
        r *= cfg->trailWidthBy[p->pair][stage];
    } else if (p->flags & 0x20) {
        r *= side[2];
        if ((p->flags & 0x80) || (p->flags & 0x100)) {
            r *= gEftGlowCfg->nodeB[p->node].pairWidth;
        } else {
            r *= gEftGlowCfg->node[p->node].pairWidth;
        }
    } else {
        if (p->flags & 0x40) {
            r *= side[0];
        } else {
            r *= side[1];
        }
        if ((p->flags & 0x80) || (p->flags & 0x100)) {
            r *= gEftGlowCfg->nodeB[p->node].width;
        } else {
            r *= gEftGlowCfg->node[p->node].width;
        }
    }
    if (p->flags & 0x10) {
        r *= mul[0];
    }
    if (p->flags & 8) {
        r *= mul[1];
    }
    return r;
}

/* Takes a quad from the free list, or the next unused one of the array (70 at most), and links it as active. */
EftPGlowPart *EftGlow_AllocPart(void) {
    EftPGlowPart *p;

    if (gEftGlow->free != NULL) {
        p = gEftGlow->free;
        gEftGlow->free = p->next;
    } else if (gEftGlow->used < 70) {
        p = &gEftGlow->parts[gEftGlow->used++];
    } else {
        return NULL;
    }
    p->next = gEftGlow->active;
    gEftGlow->active = p;
    p->flags = 0;
    return p;
}

/* Creates one quad at the emitter's current node. Eleven or more draws of libc rand().
   Notes for the match: in the first colour branch (flags 2) the three statements behind Vec4_Sub stand in BOTH
   arms (the compiler merges them, and with them the two `jal Vec4_Sub`, into one tail); the float parameter is
   reused as the constant 1.0 after it was stored; `inv` is 0.0 until a branch gives it the fade rate, and the
   paired branch (flags 0x20 without 2) passes the OLD value (0.0) as the x / y / z colour steps. */
EftPGlowPart *EftGlow_SpawnPart(EftPGlow *e, s32 objId, f32 angle, EftPVec *scale, s32 odd, s32 pair) {
    EftPVec d;
    EftPGlowPart *p = EftGlow_AllocPart();
    s32 shape = gEftGlowCfg->node[e->node].shape;
    s32 idx = rand() % e->layers;
    EftPGlowKind *k;
    f32 alpha;
    f32 inv;

    if (p == NULL) {
        return NULL;
    }
    p->flags = EftGlow_CalcPartFlags(e, pair, odd);
    if (pair != 0) {
        p->pair = 1;
    } else {
        p->pair = 0;
    }
    if (p->flags & 2) {
        p->kind = 3;
    } else if (p->flags & 0x20) {
        p->kind = 2;
    } else if (p->flags & 0x40) {
        p->kind = 0;
    } else {
        p->kind = 1;
    }
    p->life = gEftGlowCfg2->lifeBase + gEftGlowCfg2->lifeRand * ((f32)rand() / 2147483647.0f);
    if (p->flags & 8) {
        p->life = p->life * 0.9f;
    }
    p->lifeMax = p->life;
    p->objId = objId;
    p->node = e->node;
    p->angle = angle;
    inv = 0.0f;
    Vec4_Set(&p->point, inv, inv, inv, inv);
    p->width = EftGlow_CalcWidth(p, 0);
    p->length = EftGlow_CalcLength(p, 0);
    p->dWidth = EftGlow_CalcWidth(p, 1) - p->width;
    p->dWidth = p->dWidth / (p->life * gEftGlowCfg2->stage);
    p->dLength = EftGlow_CalcLength(p, 1) - p->length;
    p->dLength = p->dLength / (p->life * gEftGlowCfg2->stage);
    if (p->flags & 0x10) {
        p->tex = rand() % e->texCount;
    }
    alpha = EftGlow_GetAlpha(e, p->flags, idx);
    k = &gEftGlowCfg2->kind[p->kind];
    p->dirY = k->dirY[0] + k->dirY[1] * ((f32)rand() / 2147483647.0f);
    p->dist = k->dist[0] + k->dist[1] * ((f32)rand() / 2147483647.0f);
    p->size = (k->size[0] + k->size[1] * ((f32)rand() / 2147483647.0f)) * e->scale;
    p->widthScale = k->widthScale[0] + k->widthScale[1] * ((f32)rand() / 2147483647.0f);
    p->taper = k->taper[0] + k->taper[1] * ((f32)rand() / 2147483647.0f);
    p->offset.w = k->unk28[0] + k->unk28[1] * ((f32)rand() / 2147483647.0f);
    p->offset.x = scale->x * k->offset[0];
    p->offset.y = scale->y * k->offset[1];
    p->offset.z = scale->z * k->offset[2];
    if (p->flags & 2) {
        if (p->flags & 0x20) {
            angle = 1.0f;
            p->layer = 1;
            Vec3_Scale(&p->color, &e->color[idx], angle);
            p->color.w = inv;
            Vec4_Sub(&d, &e->color[idx], &p->color);
            Vec3_Scale(&p->dColor, &d, angle / p->lifeMax);
            inv = angle / gEftGlowCfg->fadeIn;
            p->dColor.w = alpha * inv;
        } else {
            p->layer = 0;
            Vec4_Copy(&p->color, &e->colorA);
            p->color.w = inv;
            angle = 1.0f;
            Vec4_Sub(&d, &e->colorB, &p->color);
            Vec3_Scale(&p->dColor, &d, angle / p->lifeMax);
            inv = angle / gEftGlowCfg->fadeIn;
            p->dColor.w = alpha * inv;
        }
        p->offset.z = p->offset.z * ((f32)rand() / 2147483647.0f * 1.5f + 1.0f);
        p->shape = shape / 2 + rand() % 2;
    } else if (p->flags & 0x20) {
        p->layer = 1;
        Vec4_Copy(&p->color, &e->color[idx]);
        p->color.w = inv;
        {
            f32 z = inv;

            inv = 1.0f / gEftGlowCfg->fadeIn;
            Vec4_Set(&p->dColor, z, z, z, alpha * inv);
        }
        if ((u32)(e->kind - 3) < 2) {
            p->dist *= 0.3f;
            if (p->node == 0) {
                p->dist = p->dist * 0.5f;
            } else if (p->node == 1) {
                p->dist = p->dist * 0.65f;
            } else if ((u8)(p->node - 3) < 2) {
                p->offset.z *= 0.65f;
                p->dist *= 0.5f;
            }
        }
        p->dist += 0.1f;
        p->size *= 1.3f;
        p->shape = shape + rand() % 3;
    } else {
        p->layer = 0;
        angle = 1.0f;
        Vec3_Scale(&p->color, &e->colorA, (f32)rand() / 2147483647.0f * 0.2f + angle);
        p->color.w = inv;
        inv = angle / p->lifeMax;
        Vec3_Sub(&p->dColor, &e->colorB, &p->color);
        Vec3_Scale(&p->dColor, &p->dColor, inv);
        inv = angle / gEftGlowCfg->fadeIn;
        p->dColor.w = alpha * inv;
        if ((u32)(e->kind - 3) < 2) {
            p->dirY *= 1.7f;
            p->dist *= 0.9f;
            if (p->node == 0) {
                p->dist = p->dist * 0.5f;
            } else if (p->node == 1) {
                p->dist = p->dist * 0.65f;
            } else if ((u8)(p->node - 3) < 2) {
                p->offset.z *= 0.65f;
                p->dist *= 0.5f;
            }
        } else if ((u8)(p->node - 3) < 2) {
            p->dist *= 0.7f;
        }
        p->dist *= 0.85f;
        p->size *= 1.6f;
        p->shape = shape + rand() % 4;
    }
    if (p->shape >= 20) {
        p->shape = 19;
    }
    EftGlow_GetShapeDir(&p->dir, objId, p->shape, p->dirY);
    p->frame = 0;
    p->flipA = rand() % 2;
    p->flipB = rand() % 2;
    p->animTimer = rand() % (s32)gEftGlowCfg2->animFrames;
    return p;
}

/* The emitter's colour table without the 16-byte alignment of EftPVec (see EftPVecU above): the original computes
   `e + idx * 16 + 0x260` three times, once per component read, which this compiler does only for an element
   type whose alignment equals the float's. */
typedef struct EftPGlowColorView {
    /* 0x000 */ u8 pad[0x260];
    /* 0x260 */ EftPVecU color[4];
} EftPGlowColorView;
#define GLOWCOL(e) (((EftPGlowColorView *)(e))->color)

/* Makes p the partner of a trailing quad: same profile and side, 1.5 times its life, pushed back along its
   direction, fading towards a random colour of the emitter. One draw of libc rand(). */
void EftGlow_PairPart(EftPGlow *e, EftPGlowPart *p, EftPGlowPart *src) {
    s32 idx = rand() % e->layers;
    f32 s;

    p->flipA = src->flipA;
    p->shape = src->shape;
    p->life = src->life * 1.5f;
    p->lifeMax = src->lifeMax * 1.5f;
    p->dWidth = (EftGlow_CalcWidth(p, 1) - p->width) / (p->life * gEftGlowCfg2->stage);
    p->dLength = (EftGlow_CalcLength(p, 1) - p->length) / (p->life * gEftGlowCfg2->stage);
    p->dist = src->dist;
    p->dirY = src->dirY;
    Vec4_Copy(&p->dir, &src->dir);
    Vec4_Copy(&p->offset, &src->offset);
    s = e->scale * 4.0f;
    p->offset.x -= p->dir.x * s;
    p->offset.y -= p->dir.y * s;
    p->offset.z -= p->dir.z * s;
    p->dColor.x = (GLOWCOL(e)[idx].x - p->color.x) / p->lifeMax;
    p->dColor.y = (GLOWCOL(e)[idx].y - p->color.y) / p->lifeMax;
    p->dColor.z = (GLOWCOL(e)[idx].z - p->color.z) / p->lifeMax;
}

/* Spawns the emitter's next quad (and sometimes its partner) and advances the node walk. Angles around the
   fighter's axis depend on the node; draws of libc rand(): 1 for the angle, 0..2 for the partner, 4 when the walk
   wraps. */
void EftGlow_SpawnNext(EftPGlow *e, s32 objId) {
    EftPVec off;
    EftPVec v;
    s32 n = e->count;
    f32 angle;
    f32 step;
    EftPGlowPart *p;
    EftPGlowPart *q;
    s32 odd;
    s32 pair;
    s32 bit;
    s32 side;
    s32 armL = 3;
    s32 armR = 4;

    if (e->node == armL) {
        step = -25.0f;
        angle = (f32)n * step + -40.0f + (f32)rand() / 2147483647.0f * step;
        if (angle <= -180.0f) {
            angle += 360.0f;
        }
    } else if (e->node == armR) {
        step = 25.0f;
        angle = (f32)n * step + 40.0f + (f32)rand() / 2147483647.0f * step;
        if (angle >= 180.0f) {
            angle -= 360.0f;
        }
    } else if (e->node == 3 || e->node == 6 || e->node == 8) {
        step = -35.0f;
        angle = (f32)n * step + -20.0f + (f32)rand() / 2147483647.0f * step;
        if (angle <= -180.0f) {
            angle += 360.0f;
        }
    } else if (e->node == 4 || e->node == 7 || e->node == 9) {
        step = 35.0f;
        angle = (f32)n * step + 20.0f + (f32)rand() / 2147483647.0f * step;
        if (angle >= 180.0f) {
            angle -= 360.0f;
        }
    } else {
        step = 25.0f;
        angle = (f32)n * step + -25.0f + (f32)rand() / 2147483647.0f * step;
        if (n >= 2) {
            angle += 130.0f;
        }
        if (angle >= 180.0f) {
            angle -= 360.0f;
        }
    }
    Vec3_Set(&v, 0.0f, 1.0f, 0.3f);
    Vec3_Normalize(&v, &v);
    Vec3_Scale(&off, &e->offset, e->scale);
    off.w = 1.0f;
    bit = 1 << e->node;
    if (e->nodeMask & bit) {
        e->nodeMask &= ~bit;
        odd = 1;
    } else {
        e->nodeMask |= bit;
        odd = 0;
    }
    side = 0;
    if (!(e->flags & 2)) {
        side = odd;
    }
    p = EftGlow_SpawnPart(e, objId, angle, &off, side, 0);
    if (p == NULL) {
        return;
    }
    pair = 0;
    if (e->live < 0x41) {
        if (e->flags & 2) {
            pair = 1;
        } else if (!(e->flags & 8)) {
            if (e->count < 3) {
                if (!(rand() & 1)) {
                    pair = 1;
                }
            } else {
                pair = 1;
            }
        } else {
            pair = rand() % 5 == 0;
        }
        if (pair) {
            q = EftGlow_SpawnPart(e, objId, angle, &off, 0, 1);
            if (q != NULL) {
                e->flags |= 8;
                if (e->flags & 2) {
                    EftGlow_PairPart(e, q, p);
                }
            }
        }
    }
    e->count++;
    if (e->count >= 4) {
        e->count = 0;
        if (e->flags & 2) {
            e->node--;
            if (e->node < 0) {
                e->node = 7;
            }
        } else {
            e->flags &= ~8;
            e->node++;
            if (e->node >= 10) {
                e->node = 0;
                e->offset.x = gEftGlowCfg->offX * ((f32)rand() / 2147483647.0f - (f32)rand() / 2147483647.0f);
                e->offset.y = gEftGlowCfg->offY * ((f32)rand() / 2147483647.0f - (f32)rand() / 2147483647.0f);
            }
        }
    }
}

/* Starts the emitter of a fighter (arg[0] = object id). Four draws of libc rand(). */
void EftGlow_Begin(EftPGlow *e, s32 *arg) {
    Mtx44 m;
    s32 i;
    s32 j;

    e->flags = 1;
    e->kind = BtlCharApi_GetActionFxKind(arg[0]);
    e->step = 0;
    e->objId = arg[0];
    e->alphaA = 48.0f / 255.0f;
    e->alphaB = 32.0f / 255.0f;
    e->count = 0;
    for (j = 0; j < 10; j++) {
        BtlCharApi_GetNodePos(arg[0], gEftGlowCfg->node[j].node, &e->nodePos[j]);
    }
    e->offset.x = gEftGlowCfg->offX * ((f32)rand() / 2147483647.0f - (f32)rand() / 2147483647.0f);
    e->offset.y = gEftGlowCfg->offY * ((f32)rand() / 2147483647.0f - (f32)rand() / 2147483647.0f);
    e->offset.z = gEftGlowCfg->offZ;
    EftGlow_CalcFrame(&m, arg[0], e->kind);
    for (i = 4; i >= 0; i--) {
        Mtx_Copy(&e->frame[i], &m);
    }
    e->fade = 1;
    e->fadeTime = gEftGlowCfg->fadeIn;
    e->alpha = 0.0f;
    e->live = 0;
    e->scale = BtlCharApi_GetHeight(arg[0]) / 19.35f;
}

/* Shifts the orientation history by one frame; while not fading out, frame 0 is recomputed first. */
void EftGlow_UpdateFrames(EftPGlow *e, s32 objId) {
    s32 i;

    if (e->flags & 2) {
        for (i = 4; i > 0; i--) {
            Mtx_Copy(&e->frame[i], &e->frame[i - 1]);
        }
    } else {
        EftGlow_CalcFrame(&e->frame[0], objId, e->kind);
        for (i = 4; i > 0; i--) {
            Mtx_Copy(&e->frame[i], &e->frame[i - 1]);
        }
    }
}

/* Spawns up to 14 quads this frame, keeping at most 0x41 alive. */
void EftGlow_Emit(EftPGlow *e, s32 objId) {
    s32 n;
    s32 i;

    if (e->live < 0x41) {
        n = 14;
        if (e->live + n >= 0x42) {
            n = 0x41 - e->live;
        }
        while (n > 0) {
            EftGlow_SpawnNext(e, objId);
            n--;
        }
    }
}

/* Per-frame emitter update: node positions, orientation history, emission, fade in / out (flag 4 when gone). */
void EftGlow_Step(EftPGlow *e, s32 objId) {
    s32 i;
    f32 step;

    if (!(e->flags & 2)) {
        EftAura_IsVisible(objId);
    }
    for (i = 0; i < 10; i++) {
        BtlCharApi_GetNodePos(objId, gEftGlowCfg->node[i].node, &e->nodePos[i]);
    }
    EftGlow_UpdateFrames(e, objId);
    EftGlow_Emit(e, objId);
    if (e->fade != 0) {
        if (e->fade == 1) {
            step = 1.0f / gEftGlowCfg->fadeIn;
            e->alpha += step;
            if (e->alpha >= 1.0f) {
                e->alpha = 1.0f;
                e->fade = 0;
            }
        } else {
            step = 1.0f / gEftGlowCfg->fadeOut;
            e->alpha -= step;
            if (e->alpha <= 0.0f) {
                e->alpha = 0.0f;
                e->flags |= 4;
                e->fade = 0;
            }
        }
    }
}

/* Returns every quad of a fighter to the free list. */
void EftGlow_FreeParts(s32 objId) {
    EftPGlowPart **link = &gEftGlow->active;
    EftPGlowPart *p;

    while (*link != NULL) {
        p = *link;
        if (p->objId != objId) {
            link = &p->next;
        } else {
            *link = p->next;
            p->next = gEftGlow->free;
            gEftGlow->free = p;
        }
    }
}

/* Per-frame update of a fighter's quads: position, direction, size animation, colour fade, life. */
void EftGlow_StepParts(EftPGlow *e, s32 objId) {
    Mtx44 rot;
    Mtx44 frame;
    EftPVec a;
    EftPVec b;
    EftPVec drift;
    EftPVec c;
    EftPGlowPart **link;
    EftPGlowPart *p;

    e->live = 0;
    EftGlow_CalcDrift(&drift, objId);
    link = &gEftGlow->active;
    while (*link != NULL) {
        p = *link;
        if (p->objId == objId) {
            if (!(p->life > 0.0f)) {
                *link = p->next;
                p->next = gEftGlow->free;
                gEftGlow->free = p;
            } else {
        Mtx_Copy(&frame, &e->frame[p->frame]);
        p->frame++;
        if (p->frame >= 5) {
            p->frame = 4;
        }
        Mtx_MulVec4(&a, &frame, &p->offset);
        Vec4_Add(&p->pos, &e->nodePos[p->node], &a);
        EftGlow_GetShapePoint(&p->point, objId, p->shape, e->scale * p->dist);
        EftGlow_GetShapeDir(&p->dir, objId, p->shape, p->dirY);
        Mtx_StoreIdentity(&rot);
        Mtx_RotateZ(&rot, &rot, p->angle * 3.14159265f / 180.0f);
        Mtx_MulVec4(&b, &rot, &p->point);
        Mtx_MulVec4(&p->dir, &rot, &p->dir);
        Mtx_MulVec4(&b, &frame, &b);
        Vec4_Add(&p->pos, &p->pos, &b);
        p->pos.w = 1.0f;
        Mtx_MulVec4(&p->worldDir, &frame, &p->dir);
        if (!(p->flags & 2)) {
            Vec4_Add(&p->worldDir, &p->worldDir, &drift);
            Vec3_Normalize(&p->worldDir, &p->worldDir);
        }
        p->width += p->dWidth;
        p->length += p->dLength;
        if (p->width < 0.0f) {
            p->width = 0.0f;
        }
        if (p->length < 0.0f) {
            p->length = 0.0f;
        }
        if (!(p->flags & 4)) {
            if (gEftGlowCfg2->stage < 1.0f - p->life / p->lifeMax) {
                p->dWidth = (EftGlow_CalcWidth(p, 2) - p->width) / p->life;
                p->dLength = (EftGlow_CalcLength(p, 2) - p->length) / p->life;
                p->flags |= 4;
            }
        }
        if (!(p->lifeMax - p->life < gEftGlowCfg->fadeIn)) {
            if (p->life <= gEftGlowCfg->fadeOut) {
                if (!(p->flags & 1)) {
                    f32 inv = 1.0f / (p->life + 1.0f);
                    f32 d = p->color.w - e->colorB.w;

                    p->flags |= 1;
                    p->dColor.w = -d * inv;
                }
            } else {
                p->dColor.w = 0.0f;
            }
        }
        Vec4_Add(&p->color, &p->color, &p->dColor);
        Vec4_Clamp(&p->color, &p->color, 0.0f, 1.0f);
        if (p->flags & 2) {
            f32 t = p->life / p->lifeMax;

            Vec3_Scale(&c, &p->dir, gEftGlowCfg2->trailPush * (t * (t * t * t) * (p->offset.w * e->scale)));
            Vec4_Add(&p->offset, &p->offset, &c);
        }
        p->animTimer++;
        if (p->animTimer >= gEftGlowCfg2->animFrames) {
            p->animTimer = 0;
            p->flipB++;
            if (p->flipB >= 2) {
                p->flipB = 0;
            }
        }
        link = &p->next;
        p->life -= 1.0f;
        e->live++;
            }
        } else {
            link = &p->next;
        }
    }
}

/* Matrix of a quad at pos whose z axis is dir bent towards (or away from) the camera's forward by 0.4 of their
   dot product, x across the view and y completing the frame. */
void EftGlow_MakeFacingMtx(Mtx44 *m, EftPVec *dir, EftPVec *pos) {
    EftPVec x;
    EftPVec y;
    EftPVec z;
    EftPVec cam;
    EftPVec t;
    f32 d;
    f32 s = 0.0f;

    cam.x = gBtlCamView->view.m[0][2];
    cam.y = gBtlCamView->view.m[1][2];
    cam.z = gBtlCamView->view.m[2][2];
    cam.w = 1.0f;
    d = Vec3_Dot(&cam, dir);
    if (d < s) {
        s = -d * 0.4f;
        Vec3_Set(&t, -cam.x, -cam.y, -cam.z);
    } else {
        s = d * 0.4f;
        Vec3_Set(&t, cam.x, cam.y, cam.z);
    }
    Vec4_Sub(&t, dir, &t);
    Vec3_Normalize(&t, &t);
    Vec4_Scale(&t, &t, s);
    Vec4_Add(&z, dir, &t);
    Vec3_Normalize(&z, &z);
    Vec3_Cross(&x, &cam, &z);
    Vec3_Normalize(&x, &x);
    Vec3_Cross(&y, &z, &x);
    Vec3_Normalize(&y, &y);
    Mtx_StoreIdentity(m);
    m->m[0][0] = x.x;
    m->m[1][0] = y.x;
    m->m[2][0] = z.x;
    m->m[0][1] = x.y;
    m->m[1][1] = y.y;
    m->m[2][1] = z.y;
    m->m[0][2] = x.z;
    m->m[1][2] = y.z;
    m->m[2][2] = z.z;
    m->m[3][0] = pos->x;
    m->m[3][1] = pos->y;
    m->m[3][2] = pos->z;
}

/* Matched once Vu0Cur_ProjectPoint was declared with its return value (`s32`, as its definition has): with the
   local `void` prototype the three off-screen tests came out with x / the limit 0xFFF0 / the record pointer
   copies in permuted registers (9 of 487 instructions), because a call that returns nothing leaves v0 free for
   local allocation in the block behind it. Same cause as EftAura_DrawFlames (eft_n.c). */
/* Draws a fighter's quads (not the trailing ones): each is projected, skipped when a corner is off the GS
   coordinate range or behind the near plane, and queued in the order table by its average depth. */
void EftGlow_DrawParts(EftPGlow *e, s32 objId, f32 alpha) {
    Mtx44 m;
    EftPVec *vtx[2];
    EftPVec *uvA[4];
    EftPVec *uvB[2];
    EftPVec uv[4];
    EftPVec w;
    EftPVec v;
    EftPScr scr[4];
    s32 col[3];
    EftPGlowPart **link;
    EftPGlowPart *p;
    EftPQuadPkt *pkt;
    u64 *pairTex;
    s32 clip;
    s32 i;
    s32 z;
    f32 sx;
    f32 sz;
    f32 a;
    f32 q;

    vtx[0] = gEftGlowCfg->vtx[0];
    vtx[1] = gEftGlowCfg->vtx[1];
    uvA[0] = gEftGlowCfg->uvA0;
    uvA[1] = gEftGlowCfg->uvA1;
    uvA[2] = gEftGlowCfg->uvA2;
    uvA[3] = gEftGlowCfg->uvA3;
    uvB[0] = gEftGlowCfg->uvB[0];
    uvB[1] = gEftGlowCfg->uvB[1];
    pairTex = &gEftGlow->pairTex;
    link = &gEftGlow->active;
    while (*link != NULL) {
        p = *link;
        if (p->objId != objId) {
            goto next;
        }
        clip = 0;
        if (p->flags & 2) {
            clip = 1;
        }
        if (clip) {
            goto next;
        }
        EftGlow_MakeFacingMtx(&m, &p->worldDir, &p->pos);
        sx = p->size * p->width * p->widthScale;
        sz = p->size * p->length;
        for (i = 0; i < 4; i++) {
            v.x = vtx[p->pair][i].x * sx;
            v.y = vtx[p->pair][i].y * p->size;
            v.z = vtx[p->pair][i].z * sz;
            v.w = 1.0f;
            if (i >= 2) {
                v.z *= p->taper;
            }
            Mtx_MulVec4(&w, &m, &v);
            Vu0Cur_ProjectPoint(&scr[i], &w);
            if ((u32)scr[i].x > 0xFFF0) {
                clip = 1;
                break;
            }
            if ((u32)scr[i].y > 0xFFF0) {
                clip = 1;
                break;
            }
            if (scr[i].z < 0) {
                clip = 1;
                break;
            }
            q = 1.0f / (f32)scr[i].w;
            if (p->flags & 0x20) {
                Vec4_Scale(&uv[i], &uvB[p->flipA][i], q);
            } else {
                Vec4_Scale(&uv[i], &uvA[p->flipB * 2 + p->flipA][i], q);
            }
        }
        if (clip) {
            goto next;
        }
        col[0] = p->color.x * 255.0f;
        col[1] = p->color.y * 255.0f;
        col[2] = p->color.z * 255.0f;
        a = p->color.w * 255.0f * e->alpha * alpha;
        pkt = (EftPQuadPkt *)gOtCur;
        gOtCur = (u32 *)(pkt + 1);
        if (pkt == NULL) {
            return;
        }
        pkt->prim = 0x5C;
        pkt->tag = 0x20000008;
        pkt->vif0 = 0x10000000;
        pkt->vif1 = 0x50000008;
        pkt->gif0 = 0xE400000000008001;
        pkt->gif1 = 0x42142142142160;
        pkt->next = 0;
        pkt->v[0].rgba[0] = col[0];
        pkt->v[0].rgba[1] = col[1];
        pkt->v[0].rgba[2] = col[2];
        pkt->v[0].rgba[3] = a;
        pkt->v[0].q = uv[0].z;
        pkt->v[1].rgba[0] = col[0];
        pkt->v[1].rgba[1] = col[1];
        pkt->v[1].rgba[2] = col[2];
        pkt->v[1].rgba[3] = a;
        pkt->v[1].q = uv[1].z;
        pkt->v[2].rgba[0] = col[0];
        pkt->v[2].rgba[1] = col[1];
        pkt->v[2].rgba[2] = col[2];
        pkt->v[2].rgba[3] = a;
        pkt->v[2].q = uv[2].z;
        pkt->v[3].rgba[0] = col[0];
        pkt->v[3].rgba[1] = col[1];
        pkt->v[3].rgba[2] = col[2];
        pkt->v[3].rgba[3] = a;
        pkt->v[3].q = uv[3].z;
        pkt->v[0].s = uv[0].x;
        pkt->v[0].t = uv[0].y;
        pkt->v[1].s = uv[1].x;
        pkt->v[1].t = uv[1].y;
        pkt->v[2].s = uv[2].x;
        pkt->v[2].t = uv[2].y;
        pkt->v[3].s = uv[3].x;
        pkt->v[3].t = uv[3].y;
        pkt->v[0].xyz.x = scr[0].x;
        pkt->v[0].xyz.y = scr[0].y;
        pkt->v[0].xyz.z = scr[0].z;
        pkt->v[0].xyz.f = 0xFF;
        pkt->v[1].xyz.x = scr[1].x;
        pkt->v[1].xyz.y = scr[1].y;
        pkt->v[1].xyz.z = scr[1].z;
        pkt->v[1].xyz.f = 0xFF;
        pkt->v[2].xyz.x = scr[2].x;
        pkt->v[2].xyz.y = scr[2].y;
        pkt->v[2].xyz.z = scr[2].z;
        pkt->v[2].xyz.f = 0xFF;
        pkt->v[3].xyz.x = scr[3].x;
        pkt->v[3].xyz.y = scr[3].y;
        pkt->v[3].xyz.z = scr[3].z;
        pkt->v[3].xyz.f = 0xFF;
        if (p->flags & 0x20) {
            pkt->tex0 = *pairTex;
        } else if (e->flags & 0x20) {
            pkt->tex0 = e->tex[1 + p->tex];
        } else {
            pkt->tex0 = e->tex[0];
        }
        z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
        if (BtlCharApi_IsCircleDash(p->objId)) {
            EftPOt_Add((OtPrim *)pkt, z, 1);
        } else if (BtlScene_GetStageFlag()) {
            EftPOt_Add((OtPrim *)pkt, z, 1);
        } else {
            EftPOt_Add((OtPrim *)pkt, z, p->layer);
        }
    next:
        link = &p->next;
    }
}
