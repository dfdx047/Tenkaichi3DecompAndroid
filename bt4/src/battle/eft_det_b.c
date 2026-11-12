/*
 * Stage collision queries: 0x1B16F0..0x1B3510 (include/battle/eft_det_b.h has the layouts).
 *
 * Despite the file stem this is not effect code: it continues the stage collision that starts at the end of the
 * neighbouring file (EftDet_TestStage 0x1B0E88, StgGround_Probe 0x1B14C0 and StgGround_UpdateFighter 0x1B15B8
 * call StgCol_QueryZone, StgCol_FirstBit, StgCol_FighterBreakObj and the mode setters defined here).
 *
 * Source file boundaries (evidence): StgCol_TraceZone and StgCol_CollectZone only match when StgCol_FirstBit
 * (0x1B18B8) is NOT known to the compiler, so a source file ends between 0x1B18F8 and 0x1B2BA0; the .lit4 pool
 * runs on from the neighbouring file (0x2FCFC0 is its last constant, 0x2FCFC4 the first one here).
 *
 * Per fighter and frame (BtlChar_UpdateStage8, fighter 0 first): BtlObjBody_BeginFrame clears the contact word,
 * StgCol_UpdateFighter sweeps the body sphere, StgGround_UpdateFighter probes the ground; BtlColl_UpdateGround
 * (stage 9) then turns the contact word into fighter flags and damage.
 *
 * In address order:
 *   0x1B16F0  query mode, the zone query every other function is built on, lowest-bit helper
 *   0x1B1900  camera: a swept sphere against the stage (StgCol_TraceSphere)
 *   0x1B1CA0  fighter: the body sphere moved through the stage in sub-steps and pushed out of what it touches
 *             (StgCol_UpdateFighter), breaking stage objects on the way (StgCol_FighterBreakObj)
 *   0x1B25E0  segment trace through the zones (StgCol_TraceSegment) and triangle collection around a sphere
 *             (StgCol_CollectSphere, for the stage rigid bodies)
 *   0x1B2FE8  triangles under a box, for the fighter's ground shadow (StgShadow_*)
 *
 * SIMULATION: StgCol_UpdateFighter (moves the fighter, writes its stage contact word, breaks objects),
 * StgCol_FighterBreakObj, StgCol_TraceSegment (lock-on sight, AI, sweeping beam, approach point) and
 * StgCol_CollectSphere (debris rigid bodies; read by no fighter code). The camera sweep feeds the camera only;
 * StgShadow_* only drawing. No random draw anywhere in this file.
 */
#include "common.h"
#include "battle/eft_det_b.h"

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
extern void *memcpy(void *dst, const void *src, u32 n);

extern void Vec4_Copy(StgColVec *dst, StgColVec *src);
extern void Vec4_Add(StgColVec *out, StgColVec *a, StgColVec *b);
extern void Vec3_Sub(StgColVec *out, StgColVec *a, StgColVec *b);
extern void Vec3_Scale(StgColVec *out, StgColVec *v, f32 s);
extern f32 Vec3_Dot(StgColVec *a, StgColVec *b);
extern f32 Vec3_Dist(StgColVec *a, StgColVec *b);          /* distance */

/* Collision mesh (0x230EC0..0x231768). */
extern s32 ColMesh_WalkBox(void *mesh, StgColBox *box, void *ctx, StgColCb cb); /* walk: cb per candidate node */
extern StgColPoly *ColMesh_GetPoly(void *mesh, s32 poly);
extern void ColMesh_GetPolyVerts(void *mesh, StgColPoly *poly, StgColVec *v0, StgColVec *v1, StgColVec *v2);
extern s32 ColNode_TestBox(StgColNode *node, StgColBox *box);     /* node bounds against a box */

/* Box helpers (0x230B10..0x2311C8). */
extern void StgAabb_SetEmpty(StgColBox *box);                   /* 0x230B10: empty box */
extern void ColBox_SetCenterHalf(StgColBox *out, StgColVec *center, StgColVec *extent);
extern void ColBox_AddPoint(StgColBox *box, StgColVec *pt);       /* grow to contain a point */
extern void ColBox_AddPointKeepW(StgColBox *box, StgColVec *pt);       /* grow to contain a point (x, z) */
extern void ColBox_GetCenter(StgColBox *box, StgColVec *center);
extern void ColBox_GetHalf(StgColBox *box, StgColVec *half);
extern s32 ColBox_ContainsPoint(StgColBox *box, StgColVec *pt);        /* point inside */
extern s32 ColBox_Overlaps(StgColBox *a, StgColBox *b);           /* overlap */
extern s32 ColBox_ClipRay(StgColBox *box, StgColRay *ray, StgColVec *out); /* ray against box */

/* Primitive tests (0x236228..0x2387E0). */
extern s32 ColSweep_TestTri(StgColSweepCtx *ctx, StgColTri *tri, StgColVec *pos, f32 *dist); /* swept sphere */
extern s32 ColSeg_TestTri(StgColSeg *seg, StgColTri *tri, StgColVec *pos, f32 *dist);      /* segment */
extern s32 ColSphere_TestTri(StgColVec *pos, StgColSphere *sphere, StgColTri *tri);           /* sphere */
extern void ColBounds_OfCapsule(StgColBox *out, StgColSweep *seg);    /* bounds of a swept sphere */
extern void ColBounds_OfSphere(StgColBox *out, StgColSphere *sphere); /* bounds of a sphere */
extern void ColRay_FromSeg(StgColRay *out, StgColSeg *seg);      /* ray of a segment */
extern void ColBounds_OfSeg(StgColBox *out, StgColSeg *seg);      /* bounds of a segment */
extern void ColCapsule_GetLongSegDir(StgColVec *out, StgColVec *dir, StgColSweepCtx *ctx); /* direction of a sweep */

/* Battle object body (0x24DC58..0x24E3F8). */
extern void BtlObjBody_Update(StgColFighter *obj, s32 arg);
extern void BtlObjBody_BeginFrame(StgColFighter *obj);
extern void BtlObjXf_Update(StgColFighter *obj);
extern void BtlObjPose_CalcMatrices(StgColFighter *obj);

/* Stage (stg_a.c). */
extern s32 BtlStage_IsReady(void);
extern s32 BtlStage_FindZoneNear(s32 zone, StgColVec *pos);
extern StgColZone *BtlStage_GetZone(s32 zone);
extern void BtlStage_ClearZoneMarks(void);
extern s32 BtlStage_DestroyObj(s32 objId, s32 idx, StgColVec *hitPos);
extern s32 BtlStage_IsObjBroken(s32 idx);
extern s32 BtlStage_GetObjType(s32 idx);

extern s32 BtlCharApi_TestStageBreakA(s32 objId);
extern s32 BtlCharApi_TestStageBreakB(s32 objId);

extern StgColStage *gBtlStage;
extern s32 gStgColMode;
extern void *gStgColMesh;
extern s32 gStgColZoneMisses;
extern s32 gStgColZoneVisits;
extern f32 gStgColFar[];
extern StgColVec D_002EC2C0;       /* zero vector */
extern StgColVec D_002EC2E0;       /* (0, 1, 0): down */

/* Queries test the zone's static mesh, whole objects and broken objects. */
void StgCol_SetModeAll(void) {
    gStgColMode = 0;
}

/* Queries test unbroken objects only. */
void StgCol_SetModeObjects(void) {
    gStgColMode = 1;
}

/* Walks every collision mesh of a zone that is in force against `box`, calling `cb(node, ctx)` for each candidate
   node, and reports what was touched. */
s32 StgCol_QueryZone(StgColResult *res, StgColZone *zone, StgColBox *box, void *ctx, StgColCb cb) {
    StgColObj *objs = gBtlStage->objs;
    StgColRec *recs = zone->recs;
    s32 i;

    memset(res, 0, sizeof(StgColResult));
    if (gStgColMode == 0) {
        gStgColMesh = zone->mesh;
        if (ColMesh_WalkBox(gStgColMesh, box, ctx, cb)) {
            res->hit = 1;
            res->ground = 1;
        }
    }
    for (i = 0; i < zone->recCount; i++) {
        StgColRec *rec = &recs[i];

        if (rec->broken) {
            if (!(objs[rec->obj].state & 1)) {
                continue;
            }
            if (gStgColMode == 1) {
                continue;
            }
        } else {
            if (objs[rec->obj].state & 1) {
                continue;
            }
        }
        gStgColMesh = rec->mesh;
        if (ColMesh_WalkBox(gStgColMesh, box, ctx, cb)) {
            res->recMask |= 1LL << i;
            res->hit = 1;
            res->rec = 1;
            if (!rec->broken) {
                res->obj = 1;
                res->objMask |= 1LL << rec->obj;
            }
        }
    }
    return res->hit;
}

/* Index of the lowest set bit, -1 when none. */
s32 StgCol_FirstBit(u64 mask) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if ((mask >> i) & 1) {
            return i;
        }
    }
    return -1;
}

/* Mesh callback of the camera sweep: keeps the nearest triangle the swept sphere touches. */
s32 StgCol_SweepCb(StgColNode *node, StgColSweepCtx *ctx) {
    StgColTri tri;
    StgColVec pos;
    f32 dist;
    StgColPoly *poly;
    void *mesh;
    s32 idx;

    memset(&pos, 0, sizeof(pos));
    idx = node->poly;
    mesh = gStgColMesh;
    pos.w = 1.0f;
    poly = ColMesh_GetPoly(mesh, idx);
    if (poly->flags & (STGCOL_POLY_NO_SOLID | STGCOL_POLY_NO_CAMERA | STGCOL_POLY_40 | STGCOL_POLY_20)) {
        return 0;
    }
    ColMesh_GetPolyVerts(gStgColMesh, poly, &tri.v[0], &tri.v[1], &tri.v[2]);
    Vec4_Copy(&tri.nrm, &poly->nrm);
    if (ColSweep_TestTri(ctx, &tri, &pos, &dist)) {
        if (dist < ctx->dist) {
            ctx->dist = dist;
            ctx->hit = 1;
            Vec4_Copy(&ctx->pos, &pos);
            ctx->tri = tri;
            return 1;
        }
    }
    return 0;
}

/* Sweeps a sphere from seg->a to seg->b through one zone (the sweep starts two radii behind a). Returns 1 on a
   hit, with the sphere's position, the fraction of the (lengthened) way and the triangle. Camera only. */
s32 StgCol_TraceSphere(s32 zoneIdx, StgColSweep *seg, StgColVec *hitPos, f32 *frac, StgColTri *tri) {
    StgColBox box;
    StgColSweepCtx ctx;
    StgColResult res;
    StgColVec dir;
    StgColVec tmp[2]; /* the lengthened segment (two points), written by ColCapsule_GetLongSegDir and not read */
    StgColZone *zone = BtlStage_GetZone(zoneIdx);

    ColBounds_OfCapsule(&box, seg);
    memset(&ctx, 0, sizeof(ctx));
    ctx.seg = *seg;
    ColCapsule_GetLongSegDir(tmp, &dir, &ctx);
    ctx.seg.a.x -= dir.x * seg->radius * 2.0f;
    ctx.seg.a.y -= dir.y * seg->radius * 2.0f;
    ctx.seg.a.z -= dir.z * seg->radius * 2.0f;
    Vec4_Copy(&ctx.sphere.pos, &ctx.seg.a);
    ctx.sphere.radius = ctx.seg.radius;
    ctx.sphere.radiusSq = ctx.seg.radius * ctx.seg.radius;
    Vec3_Sub(&ctx.delta, &ctx.seg.b, &ctx.seg.a);
    ctx.length = sqrtf(Vec3_Dot(&ctx.delta, &ctx.delta));
    if (ctx.length > 0.0f) {
        Vec3_Scale(&ctx.dir, &ctx.delta, 1.0f / ctx.length);
    } else {
        Vec4_Copy(&ctx.dir, &D_002EC2C0);
    }
    ctx.dist = ctx.length;
    StgCol_SetModeAll();
    if (StgCol_QueryZone(&res, zone, &box, &ctx, (StgColCb)StgCol_SweepCb)) {
        if (frac != NULL) {
            if (ctx.length > 0.0f) {
                *frac = ctx.dist / ctx.length;
            } else {
                *frac = 0.0f;
            }
        }
        if (tri != NULL) {
            *tri = ctx.tri;
        }
        Vec4_Copy(hitPos, &ctx.pos);
        return 1;
    }
    return 0;
}

/* Splits a movement of length `dist` into steps of at most half a radius: writes one step, returns how many. */
/* The `do { } while (0)` is needed to match: the original keeps `scale = 1.0f` in front of the branch although its
   only use is in the other block; without loop notes around that use the compiler moves the load next to it. */
s32 StgCol_SplitStep(StgColVec *out, StgColVec *delta, f32 radius, f32 dist) {
    s32 n = dist / (radius * 0.5f);
    f32 scale = 1.0f;

    do {
        if (n == 0) {
            Vec4_Copy(out, delta);
            return 1;
        }
        n++;
        scale /= n;
        Vec3_Scale(out, delta, scale);
    } while (0);
    return n;
}

/* Advances the sphere by one step and rebuilds its bounds (radius + 0.1). */
void StgCol_StepSphere(StgColSphere *sphere, StgColBox *box, StgColVec *step) {
    StgColVec ext;

    Vec4_Add(&sphere->pos, &sphere->pos, step);
    ext.z = ext.y = ext.x = sphere->radius + 0.1f;
    ColBox_SetCenterHalf(box, &sphere->pos, &ext);
}

/* Moves the sphere away from a contact point until the point lies on its surface, and rebuilds its bounds. */
void StgCol_PushSphere(StgColSphere *sphere, StgColBox *box, StgColVec *point) {
    StgColVec d;
    StgColVec push;
    StgColVec ext;
    f32 len;

    Vec3_Sub(&d, &sphere->pos, point);
    len = sqrtf(Vec3_Dot(&d, &d));
    Vec3_Scale(&push, &d, (sphere->radius - len) / len);
    Vec4_Add(&sphere->pos, &sphere->pos, &push);
    ext.z = ext.y = ext.x = sphere->radius + 1.1f;
    ColBox_SetCenterHalf(box, &sphere->pos, &ext);
}

/* Mesh callback of the fighter's body sphere: pushes the sphere out of every solid triangle it touches, in the
   order the mesh walk delivers them, and remembers the last one. */
s32 StgCol_PushCb(StgColNode *node, StgColPushCtx *ctx) {
    StgColTri tri;
    StgColVec pos;
    StgColPoly *poly;
    void *mesh;
    s32 idx;

    memset(&pos, 0, sizeof(pos));
    idx = node->poly;
    mesh = gStgColMesh;
    pos.w = 1.0f;
    poly = ColMesh_GetPoly(mesh, idx);
    if (poly->flags & STGCOL_POLY_NO_SOLID) {
        return 0;
    }
    ColMesh_GetPolyVerts(gStgColMesh, poly, &tri.v[0], &tri.v[1], &tri.v[2]);
    Vec4_Copy(&tri.nrm, &poly->nrm);
    if (ColSphere_TestTri(&pos, &ctx->sphere, &tri)) {
        StgCol_PushSphere(&ctx->sphere, &ctx->box, &pos);
        ctx->poly = *poly;
        ctx->hit = 1;
        return 1;
    }
    return 0;
}

/* Moves the body sphere `steps` times by `step`, resolving against the zone after every step (the zone is looked
   up again each step when crossZones is set) and breaking the first unbroken object touched in a step. When
   anything pushed the sphere: contact bit 0x20, the polygon into the work buffer, the object's position moved by
   the correction. Returns 1 when pushed. */
s32 StgCol_MoveFighter(StgColFighter *obj, StgColPushCtx *ctx, s32 zoneIdx, s32 steps, StgColVec *step,
                       StgColVec *delta, s32 crossZones, s32 keepSphere) {
    StgColResult res;
    StgColPose *pose = &obj->pose;
    StgColBody *body = &obj->body;
    StgColObjWork *work = obj->work;
    StgColSphere *sphere = body->cur;
    StgColZone *zone;
    s32 cur;

    if (!crossZones) {
        zone = BtlStage_GetZone(zoneIdx);
        cur = zoneIdx;
    } else {
        cur = zoneIdx;
        zone = NULL;
    }
    do {
        StgCol_StepSphere(&ctx->sphere, &ctx->box, step);
        if (crossZones) {
            cur = BtlStage_FindZoneNear(cur, &ctx->sphere.pos);
            zone = BtlStage_GetZone(cur);
        }
        StgCol_SetModeAll();
        if (StgCol_QueryZone(&res, zone, &ctx->box, ctx, (StgColCb)StgCol_PushCb)) {
            if (res.obj) {
                StgCol_FighterBreakObj(obj, StgCol_FirstBit(res.objMask), delta);
            }
        }
    } while (--steps > 0);
    if (ctx->hit) {
        work->flags |= STGCOL_HIT_WALL;
        work->contact = ctx->poly;
        Vec3_Sub(&body->push, &sphere->pos, &ctx->sphere.pos);
        Vec3_Sub(&pose->pos, &pose->pos, &body->push);
        pose->pos.w = 1.0f;
        BtlObjXf_Update(obj);
        BtlObjPose_CalcMatrices(obj);
        if (keepSphere) {
            BtlObjBody_Update(obj, 0);
            return 1;
        }
        Vec4_Copy(&sphere->pos, &ctx->sphere.pos);
        return 1;
    }
    Vec4_Copy(&body->push, &D_002EC2C0);
    return 0;
}

/* Per-frame stage collision of a fighter's battle object: sweeps its body sphere from last frame's position to
   this frame's and pushes the object out of the stage. */
void StgCol_UpdateFighter(StgColFighter *obj, s32 keepSphere, s32 grow) {
    StgColVec step;
    StgColVec delta;
    StgColPushCtx ctx;
    StgColPushCtx copy;
    StgColBody *body = &obj->body;
    StgColPose *pose = &obj->pose;
    StgColObjWork *work = obj->work;
    StgColSphere *cur;
    StgColSphere *prev;
    s32 zone;
    s32 steps;
    f32 dist;
    f32 r;

    if (work == NULL) {
        return;
    }
    BtlObjBody_BeginFrame(obj);
    cur = body->cur;
    prev = body->prev;
    work->flags &= ~STGCOL_HIT_WALL;
    work->flags &= ~STGCOL_HIT_BROKE;
    if (keepSphere) {
        BtlObjBody_Update(obj, 0);
    } else {
        r = body->radius;
        cur->radius = r;
        cur->radiusSq = r * r;
    }
    if (grow) {
        body->curRadius += 1.0f;
        if (body->radius < body->curRadius) {
            body->curRadius = body->radius;
        }
    }
    if (body->curRadius < 4.0f) {
        body->curRadius = 4.0f;
    }
    if (prev != NULL) {
        prev->radius = body->curRadius;
        prev->radiusSq = body->curRadius * body->curRadius;
    }
    if (cur != NULL) {
        cur->radius = body->curRadius;
        cur->radiusSq = body->curRadius * body->curRadius;
    }
    zone = BtlStage_FindZoneNear(pose->zone, &cur->pos);
    memset(&ctx, 0, sizeof(ctx));
    if (!body->teleported) {
        ctx.sphere = *prev;
        Vec3_Sub(&delta, &cur->pos, &prev->pos);
        dist = sqrtf(Vec3_Dot(&delta, &delta));
    } else {
        ctx.sphere = *cur;
        Vec4_Copy(&delta, &D_002EC2C0);
        body->teleported = 0;
        dist = 0.0f;
    }
    steps = StgCol_SplitStep(&step, &delta, ctx.sphere.radius, dist);
    copy = ctx;
    if (prev != NULL) {
        if (zone != pose->zone) {
            StgCol_MoveFighter(obj, &ctx, pose->zone, steps, &step, &delta, 1, keepSphere);
        } else {
            StgCol_MoveFighter(obj, &ctx, pose->zone, steps, &step, &delta, 0, keepSphere);
        }
    } else {
        StgCol_MoveFighter(obj, &ctx, zone, steps, &step, &delta, 0, keepSphere);
    }
    pose->zone = zone;
}

/* A fighter's body touched unbroken stage object idx. */
void StgCol_FighterBreakObj(StgColFighter *obj, s32 idx, StgColVec *hitPos) {
    StgColObjWork *work = obj->work;
    s32 type;

    if (idx < 0) {
        return;
    }
    if (work == NULL) {
        return;
    }
    type = BtlStage_GetObjType(idx);
    /* Written out twice, as the original was: the compiler merges the two bodies after register allocation. */
    if (BtlCharApi_TestStageBreakA(obj->objId)) {
        if (type & 0x100) {
            BtlStage_DestroyObj(obj->objId, idx, hitPos);
            work->flags |= STGCOL_HIT_BROKE;
        }
    } else {
        if (BtlCharApi_TestStageBreakB(obj->objId)) {
            BtlStage_DestroyObj(obj->objId, idx, hitPos);
            work->flags |= STGCOL_HIT_BROKE;
        }
    }
    if (work->flags & STGCOL_HIT_BROKE) {
        if (type & 0x800) {
            work->flags |= STGCOL_HIT_DMG_200;
        }
        if (type & 0x2000) {
            work->flags |= STGCOL_HIT_DMG_600;
        }
        if (type & 0x1000) {
            work->flags |= STGCOL_HIT_DMG_1000;
        }
    }
}

/* StgCol_FirstBit as a function not yet seen by the compiler: StgCol_TraceZone and StgCol_CollectZone only match
   that way (branch-likely choice), so in the original it was not defined earlier in their source file. */
extern s32 StgCol_FirstBit_(u64 mask) __asm__("StgCol_FirstBit");

/* Mesh callback of the segment trace: keeps the nearest solid triangle the segment crosses. */
s32 StgCol_SegCb(StgColNode *node, StgColCtx *ctx) {
    StgColTri tri;
    StgColVec pos;
    f32 dist;
    StgColPoly *poly;

    poly = ColMesh_GetPoly(gStgColMesh, node->poly);
    if (poly->flags & STGCOL_POLY_NO_SOLID) {
        return 0;
    }
    ColMesh_GetPolyVerts(gStgColMesh, poly, &tri.v[0], &tri.v[1], &tri.v[2]);
    Vec4_Copy(&tri.nrm, &poly->nrm);
    if (ColSeg_TestTri(&ctx->seg, &tri, &pos, &dist)) {
        if (ctx->dist > dist) {
            ctx->poly = *poly;
            Vec4_Copy(&ctx->pos, &pos);
            ctx->hit = 1;
            ctx->dist = dist;
            return 1;
        }
    }
    return 0;
}

/* Mesh callback of the triangle collection: appends every solid triangle the sphere touches (64 at most). */
s32 StgCol_CollectCb(StgColNode *node, StgColCtx *ctx) {
    StgColTri tri;
    StgColVec pos;
    StgColPoly *poly;

    poly = ColMesh_GetPoly(gStgColMesh, node->poly);
    if (poly->flags & STGCOL_POLY_NO_SOLID) {
        return 0;
    }
    ColMesh_GetPolyVerts(gStgColMesh, poly, &tri.v[0], &tri.v[1], &tri.v[2]);
    Vec4_Copy(&tri.nrm, &poly->nrm);
    if (ColSphere_TestTri(&pos, &ctx->sphere, &tri)) {
        if (ctx->list != NULL) {
            if (ctx->list->count < 0x40) {
                *(StgColTri *)((u8 *)&((StgColTri *)ctx->list)[ctx->list->count] + 0x10) = tri;
                ctx->list->count++;
            }
        }
        ctx->hit = 1;
        return 1;
    }
    return 0;
}

/* Prepares the static context for a segment trace. */
void StgCol_InitSegCtx(StgColCtx *ctx, StgColSeg *seg) {
    f32 far;

    memset(ctx, 0, sizeof(StgColCtx));
    ctx->seg = *seg;
    ColBounds_OfSeg(&ctx->box, seg);
    ctx->dist = Vec3_Dist(&seg->a, &seg->b);
    ColRay_FromSeg(&ctx->ray, seg);
    far = gStgColFar[0];
    ctx->obj = -1;
    ctx->dist = far;
}

/* Prepares the static context for a triangle collection. */
void StgCol_InitSphereCtx(StgColCtx *ctx, StgColSphere *sphere, StgColTriList *list) {
    memset(ctx, 0, sizeof(StgColCtx));
    ctx->sphere = *sphere;
    ColBounds_OfSphere(&ctx->box, sphere);
    ctx->list = list;
    if (list != NULL) {
        list->count = 0;
    }
    ctx->obj = -1;
}

/* The zone's rectangle as a box of unlimited height. */
void StgCol_GetZoneBox(StgColBox *box, s32 zoneIdx) {
    StgColZone *zones = gBtlStage->zones;
    StgColVec lo = { 0.0f, -10000.0f, 0.0f, 1.0f };
    StgColVec hi = { 0.0f, 10000.0f, 0.0f, 1.0f };

    StgAabb_SetEmpty(box);
    lo.x = zones[zoneIdx].center.x - zones[zoneIdx].half.x;
    lo.z = zones[zoneIdx].center.z - zones[zoneIdx].half.z;
    hi.x = zones[zoneIdx].center.x + zones[zoneIdx].half.x;
    hi.z = zones[zoneIdx].center.z + zones[zoneIdx].half.z;
    ColBox_AddPointKeepW(box, &lo);
    ColBox_AddPointKeepW(box, &hi);
}

/* Bounds of the part of the segment that lies inside zone `cur`: between the point where it enters (`entry`, or
   its start in the first zone) and the point where it leaves (found with the reversed ray, or its end in the last
   zone). */
void StgCol_ClipSegToZone(StgColBox *out, s32 cur, s32 from, s32 to, StgColCtx *ctx, StgColBox *zoneBox,
                          StgColVec *entry) {
    StgColRay back;

    StgAabb_SetEmpty(out);
    if (from == to) {
        *out = ctx->box;
        return;
    }
    if (cur == from) {
        ColBox_AddPoint(out, &ctx->seg.a);
        back = ctx->ray;
        Vec4_Copy(&back.origin, &ctx->seg.b);
        Vec3_Scale(&back.dir, &back.dir, -1.0f);
        if (ColBox_ClipRay(zoneBox, &back, entry)) {
            ColBox_AddPoint(out, entry);
        }
    } else if (cur == to) {
        ColBox_AddPoint(out, &ctx->seg.b);
        ColBox_AddPoint(out, entry);
    } else {
        back = ctx->ray;
        Vec4_Copy(&back.origin, &ctx->seg.b);
        Vec3_Scale(&back.dir, &back.dir, -1.0f);
        ColBox_AddPoint(out, entry);
        if (ColBox_ClipRay(zoneBox, &back, entry)) {
            ColBox_AddPoint(out, entry);
        }
    }
}

/* Segment trace, one zone: tests the zone when the segment passes through it; without a hit goes on to every
   neighbour that contains the point where the segment entered this zone... The first zone with a hit ends the
   walk. */
void StgCol_TraceZone(s32 cur, s32 from, s32 to, StgColCtx *ctx) {
    StgColResult res;
    StgColBox zoneBox;
    StgColBox box;
    StgColVec entry;
    StgColZone *zones = gBtlStage->zones; /* indexed at every use, as the original was: no element pointer */
    s32 last;
    s32 obj;
    s32 *near;
    s32 i;

    if (zones[cur].mark & 1) {
        return;
    }
    zones[cur].mark |= 1;
    last = to == cur;
    gStgColZoneVisits++;
    StgCol_GetZoneBox(&zoneBox, cur);
    if (!ColBox_Overlaps(&zoneBox, &ctx->box)) {
        return;
    }
    if (!ColBox_ClipRay(&zoneBox, &ctx->ray, &entry)) {
        return;
    }
    StgCol_ClipSegToZone(&box, cur, from, to, ctx, &zoneBox, &entry);
    StgCol_SetModeAll();
    if (StgCol_QueryZone(&res, &zones[cur], &box, ctx, (StgColCb)StgCol_SegCb)) {
        obj = StgCol_FirstBit_(res.objMask);
        if (obj >= 0) {
            if (!BtlStage_IsObjBroken(obj)) {
                ctx->obj = obj;
            }
        }
    } else {
        gStgColZoneMisses++;
        if (!last) {
            near = zones[cur].near;
            for (i = 0; i < zones[cur].nearCount; i++) {
                StgCol_GetZoneBox(&zoneBox, near[i]);
                if (ColBox_ContainsPoint(&zoneBox, &entry)) {
                    StgCol_TraceZone(near[i], from, to, ctx);
                }
            }
        }
    }
}

/* Triangle collection in one zone. */
void StgCol_CollectZone(s32 zoneIdx, StgColCtx *ctx) {
    StgColResult res;
    StgColZone *zones = gBtlStage->zones;
    s32 obj;

    StgCol_SetModeAll();
    if (!StgCol_QueryZone(&res, &zones[zoneIdx], &ctx->box, ctx, (StgColCb)StgCol_CollectCb)) {
        return;
    }
    obj = StgCol_FirstBit_(res.objMask);
    if (obj >= 0) {
        if (!BtlStage_IsObjBroken(obj)) {
            ctx->obj = obj;
        }
    }
}

/* Does the stage block the segment a -> b? On 1, gStgColHit has the position, the polygon and the index of the
   unbroken object hit (-1: static geometry). 0 while the stage is not ready. */
s32 StgCol_TraceSegment(StgColSeg *seg) {
    s32 from;
    s32 to;

    if (!BtlStage_IsReady()) {
        return 0;
    }
    memset(&gStgColHit, 0, sizeof(StgColHit));
    BtlStage_ClearZoneMarks();
    from = BtlStage_FindZoneNear(-1, &seg->a);
    to = BtlStage_FindZoneNear(-1, &seg->b);
    StgCol_InitSegCtx(&gStgColCtx, seg);
    gStgColZoneMisses = 0;
    gStgColZoneVisits = 0;
    StgCol_TraceZone(from, from, to, &gStgColCtx);
    if (gStgColCtx.hit) {
        gStgColHit.hit = 1;
        Vec4_Copy(&gStgColHit.pos, &gStgColCtx.pos);
        memcpy(&gStgColHit.poly, &gStgColCtx.poly, sizeof(StgColPoly));
        gStgColHit.obj = gStgColCtx.obj;
        return 1;
    }
    return 0;
}

/* Hit position of the last trace or collection; 0 when it found nothing. */
s32 StgCol_GetHitPos(StgColVec *out) {
    if (gStgColCtx.hit) {
        Vec4_Copy(out, &gStgColCtx.pos);
        return 1;
    }
    return 0;
}

/* Result block of the last segment trace. */
StgColHit *StgCol_GetHit(void) {
    return &gStgColHit;
}

/* Collects the solid triangles touching a sphere, in the sphere's zone only. Used by the stage rigid bodies. */
s32 StgCol_CollectSphere(StgColSphere *sphere, StgColTriList *list) {
    s32 zone;
    StgColCtx *ctx;

    memset(&gStgColHit, 0, sizeof(StgColHit));
    BtlStage_ClearZoneMarks();
    zone = BtlStage_FindZoneNear(-1, &sphere->pos);
    ctx = &gStgColCtx;
    StgCol_InitSphereCtx(ctx, sphere, list);
    gStgColZoneMisses = 0;
    gStgColZoneVisits = 0;
    StgCol_CollectZone(zone, ctx);
    return ctx->hit != 0;
}

/* Shadow query setup: the caller's box extended 50 downwards, and a thin column through its centre. */
void StgShadow_InitCtx(StgShadowCtx *ctx, StgColBox *box, s32 max, StgColTri *buf) {
    StgColVec center;
    StgColVec half;

    memset(ctx, 0, sizeof(StgShadowCtx));
    ctx->max = max;
    ctx->buf = buf;
    ColBox_GetCenter(box, &center);
    ColBox_GetHalf(box, &half);
    half.y = (half.y * 2.0f + 50.0f) * 0.5f;
    center.y = box->min[1] + half.y;
    ColBox_SetCenterHalf(&ctx->box, &center, &half);
    half.z = half.x = 0.1f;
    ColBox_SetCenterHalf(&ctx->probe, &center, &half);
    ctx->pt.x = center.x;
    ctx->pt.y = ctx->probe.min[1];
    ctx->pt.z = center.z;
    ctx->pt.w = 1.0f;
    ctx->groundY = ctx->probe.max[1];
    ctx->bottomY = box->max[1];
}

/* When the point lies over the triangle (x, z), lowers *y to the triangle's height there. */
s32 StgShadow_ProbeTri(StgColVec *pt, StgColTri *tri, f32 *y) {
    s32 ret = 0;
    f32 x0 = tri->v[0].x;
    f32 z0 = tri->v[0].z;
    f32 pz = pt->z;
    f32 px = pt->x;
    f32 x2 = tri->v[2].x;
    f32 z2 = tri->v[2].z;
    f32 x1 = tri->v[1].x;
    f32 z1 = tri->v[1].z;
    f32 h;

    h = (x2 - x0) * (pz - z0) - (z2 - z0) * (px - x0);
    if (h <= 0.001f) {
        if ((x1 - x2) * (pz - z2) - (z1 - z2) * (px - x2) <= 0.001f) {
            if ((x0 - x1) * (pz - z1) - (z0 - z1) * (px - x1) <= 0.001f) {
                h = (tri->nrm.w - tri->nrm.x * px - tri->nrm.z * pz) / tri->nrm.y;
                if (h < *y) {
                    *y = h;
                }
                ret = 1;
            }
        }
    }
    return ret;
}

/* Appends a triangle to the shadow list. A polygon with the "flat only" flag is dropped unless it faces straight
   up. */
s32 StgShadow_AddTri(StgShadowCtx *ctx, StgColTri *tri, s32 flags) {
    s32 *count;

    if (ctx->count >= ctx->max) {
        return 0;
    }
    if (flags & STGCOL_POLY_FLAT_ONLY) {
        if (tri->nrm.y >= -0.999f) {
            return 0;
        }
    }
    count = &ctx->count;
    ctx->buf[(*count)++] = *tri;
    return 1;
}

/* Mesh callback of the shadow query: upward-facing triangles only. */
s32 StgShadow_Cb(StgColNode *node, StgShadowCtx *ctx) {
    StgColTri tri;
    StgColPoly *poly;

    poly = ColMesh_GetPoly(gStgColMesh, node->poly);
    if (poly->flags & (STGCOL_POLY_NO_SOLID | STGCOL_POLY_NO_SHADOW | STGCOL_POLY_40 | STGCOL_POLY_20)) {
        return 0;
    }
    if (!(Vec3_Dot(&D_002EC2E0, &poly->nrm) < 0.0f)) {
        return 0;
    }
    ColMesh_GetPolyVerts(gStgColMesh, poly, &tri.v[0], &tri.v[1], &tri.v[2]);
    Vec4_Copy(&tri.nrm, &poly->nrm);
    if (ColNode_TestBox(node, &ctx->probe)) {
        StgShadow_ProbeTri(&ctx->pt, &tri, &ctx->groundY);
    }
    return StgShadow_AddTri(ctx, &tri, poly->flags) != 0;
}

/* Shadow strength by height above the ground: 1 up to 20, falling to 0 at 50. */
f32 StgShadow_CalcAlpha(f32 height) {
    f32 alpha = 1.0f;

    if (height <= 0.0f) {
        return alpha;
    }
    if (height >= 50.0f) {
        return 0.0f;
    }
    if (height > 20.0f) {
        return alpha - (height - 20.0f) / 30.0f;
    }
    return alpha;
}

/* Writes the output block of the shadow query. */
void StgShadow_Finish(StgShadowCtx *ctx) {
    f32 alpha;

    memset(&gStgShadow, 0, sizeof(StgShadowOut));
    alpha = StgShadow_CalcAlpha(ctx->groundY - ctx->bottomY);
    (&gStgShadow)->alpha = alpha;
    if (alpha <= 0.0f) {
        (&gStgShadow)->count = 0;
    } else {
        (&gStgShadow)->count = ctx->count;
    }
}

/* Collects up to `max` upward-facing triangles under `box` into `buf` and the shadow strength into gStgShadow.
   Returns the triangle count. Drawing only (caller: the stage model code at 0x1143A0). */
s32 StgShadow_Collect(s32 zoneIdx, StgColBox *box, s32 max, StgColTri *buf) {
    StgShadowCtx ctx;
    StgColResult res;
    StgColZone *zone;

    if (buf == NULL) {
        return 0;
    }
    if (box == NULL) {
        return 0;
    }
    if (!BtlStage_IsReady()) {
        return 0;
    }
    StgShadow_InitCtx(&ctx, box, max, buf);
    zone = BtlStage_GetZone(BtlStage_FindZoneNear(zoneIdx, &ctx.pt));
    StgCol_SetModeAll();
    StgCol_QueryZone(&res, zone, &ctx.box, &ctx, (StgColCb)StgShadow_Cb);
    StgShadow_Finish(&ctx);
    return ctx.count;
}

/* Output block of the last shadow query. */
StgShadowOut *StgShadow_GetOut(void) {
    return &gStgShadow;
}
