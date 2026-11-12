/*
 * Collision primitives: 0x230B38..0x236190 (include/battle/col_a.h has the layouts and the contracts).
 *
 * The object really starts one function earlier: StgAabb_SetEmpty (0x230B10, at the end of stg_d.c) is the first
 * of the box helpers and owns the constant in front of this file's pool (0x2FE3F0, 1e10).
 *
 * In address order:
 *   0x230B38  axis-aligned box helpers (ColBox_*)
 *   0x230E88  collision mesh: tree node tests, polygon and vertex access, base pointer
 *   0x230F70  oriented box set-up (ColObb_Init / ColObb_Update)
 *   0x231148  box against box, ray against box, segment against box
 *   0x231590  the two walks of a mesh's box tree (collect indices / callback per leaf)
 *   0x2317C0  Col_NearEq, Col_LineLineParams
 *   0x231930  oriented box against oriented box (ColObb_Overlaps)
 *   0x231F38  ColObb_Contact (dead code, left in assembly)
 *
 * Everything here is compiled C on the FPU; no function contains VU0 code. The vector helpers it CALLS are
 * hand-written VU0 routines (Vec4_Copy, Vec4_Sub, Vec3_Dot, Mtx_MulVec4, Vec3_Copy, Vec3_LengthSq,
 * Vec3_Normalize): see the header for what that means for a port.
 *
 * SIMULATION: all of it is pure geometry called by the stage collision (eft_det_b.c), the hit detection
 * (eft_det_a.c), the camera and the battle objects. No random draw, no pad or screen input, no time.
 * The only state is the mesh base pointer and the walk variables (gColWalk*), which make the two mesh walks
 * non re-entrant: a callback must not start another walk.
 */
#include "common.h"
#include "battle/col_a.h"

extern void Vec4_Copy(ColVec *dst, ColVec *src);
extern void Vec4_Sub(ColVec *dst, ColVec *a, ColVec *b);
extern void Mtx_MulVec4(ColVec *dst, void *mtx, ColVec *v);
extern f32 Vec3_Dot(ColVec *a, ColVec *b);
extern void Mtx_Copy(void *dst, void *src);      /* copies a 4x4 matrix (128-bit moves) */
extern void Vec3_Copy(ColVec *dst, ColVec *src);  /* copies x, y, z and leaves w (VU0) */
extern f32 Vec3_LengthSq(ColVec *v);                  /* squared length of x, y, z (VU0) */
extern void ColSeg_GetMidpoint(ColSeg *seg, ColVec *out);  /* middle of a segment, w = 1 */

/* Sets a box from its centre and half extents. */
void ColBox_SetCenterHalf(ColBox *box, ColVec *center, ColVec *half) {
    box->min[0] = center->x - half->x;
    box->min[1] = center->y - half->y;
    box->min[2] = center->z - half->z;
    box->max[0] = center->x + half->x;
    box->max[1] = center->y + half->y;
    box->max[2] = center->z + half->z;
}

/* Grows a box to contain a point; also sets the point's w to 1. */
void ColBox_AddPoint(ColBox *box, ColVec *pt) {
    if (pt->x < box->min[0]) {
        box->min[0] = pt->x;
    }
    if (box->max[0] < pt->x) {
        box->max[0] = pt->x;
    }
    if (pt->y < box->min[1]) {
        box->min[1] = pt->y;
    }
    if (box->max[1] < pt->y) {
        box->max[1] = pt->y;
    }
    if (pt->z < box->min[2]) {
        box->min[2] = pt->z;
    }
    if (box->max[2] < pt->z) {
        box->max[2] = pt->z;
    }
    pt->w = 1.0f;
}

/* Grows a box to contain a point, leaving the point alone. */
void ColBox_AddPointKeepW(ColBox *box, ColVec *pt) {
    if (pt->x < box->min[0]) {
        box->min[0] = pt->x;
    }
    if (box->max[0] < pt->x) {
        box->max[0] = pt->x;
    }
    if (pt->y < box->min[1]) {
        box->min[1] = pt->y;
    }
    if (box->max[1] < pt->y) {
        box->max[1] = pt->y;
    }
    if (pt->z < box->min[2]) {
        box->min[2] = pt->z;
    }
    if (box->max[2] < pt->z) {
        box->max[2] = pt->z;
    }
}

/* Centre of a box: (max + min) * 0.5, w = 1. */
void ColBox_GetCenter(ColBox *box, ColVec *out) {
    out->x = (box->max[0] + box->min[0]) * 0.5f;
    out->y = (box->max[1] + box->min[1]) * 0.5f;
    out->z = (box->max[2] + box->min[2]) * 0.5f;
    out->w = 1.0f;
}

/* Half extents of a box: (max - min) * 0.5, w = 1. */
void ColBox_GetHalf(ColBox *box, ColVec *out) {
    out->x = (box->max[0] - box->min[0]) * 0.5f;
    out->y = (box->max[1] - box->min[1]) * 0.5f;
    out->z = (box->max[2] - box->min[2]) * 0.5f;
    out->w = 1.0f;
}

/* Size of a box: max - min, w = 1. No caller. */
void ColBox_GetSize(ColBox *box, ColVec *out) {
    out->x = box->max[0] - box->min[0];
    out->y = box->max[1] - box->min[1];
    out->z = box->max[2] - box->min[2];
    out->w = 1.0f;
}

/* 1 when the box is at least 1e-6 thick on every axis (max >= min + 1e-6); an empty box gives 0. No caller. */
s32 ColBox_HasVolume(ColBox *box) {
    if (box->max[0] < box->min[0] + 0.000001f || box->max[1] < box->min[1] + 0.000001f ||
        box->max[2] < box->min[2] + 0.000001f) {
        return 0;
    }
    return 1;
}

/* 1 when the point is inside the box, faces included. */
s32 ColBox_ContainsPoint(ColBox *box, ColVec *pt) {
    s32 ret = 0;

    if (!(box->max[0] < pt->x) && !(pt->x < box->min[0]) && !(box->max[1] < pt->y) && !(pt->y < box->min[1]) &&
        !(box->max[2] < pt->z) && !(pt->z < box->min[2])) {
        ret = 1;
    }
    return ret;
}

/* 1 when the tree node is a leaf (no second child). */
s32 ColNode_IsLeaf(ColNode *node) {
    return node->right == -1;
}

/* 1 when the tree node is a leaf without a polygon. No caller. */
s32 ColNode_IsEmptyLeaf(ColNode *node) {
    s32 ret = 0;

    if (node->right == -1) {
        ret = node->left == -1;
    }
    return ret;
}

/* Copies the three corners of a polygon out of the mesh's vertex array. */
void ColMesh_GetPolyVerts(ColMesh *mesh, ColPoly *poly, ColVec *v0, ColVec *v1, ColVec *v2) {
    ColVec *vtx = (ColVec *)(gColMeshBase + mesh->vtxOfs);

    Vec4_Copy(v0, &vtx[poly->vtx[0]]);
    Vec4_Copy(v1, &vtx[poly->vtx[1]]);
    Vec4_Copy(v2, &vtx[poly->vtx[2]]);
}

/* Polygon `idx` of a mesh. */
ColPoly *ColMesh_GetPoly(ColMesh *mesh, s32 idx) {
    ColPoly *polys;

    polys = (ColPoly *)(gColMeshBase + mesh->polyOfs);
    return polys + idx;
}

/* Sets the base the mesh offsets are counted from (the stage's collision block). */
void ColMesh_SetBase(s32 *base) {
    gColMeshBase = base;
}

/* Sets up an oriented box from a centre and three half extents: the eight local corners. */
void ColObb_Init(ColObb *obb, ColVec *center, f32 hx, f32 hy, f32 hz) {
    obb->center.x = center->x;
    obb->center.y = center->y;
    obb->center.z = center->z;
    obb->center.w = 1.0f;
    obb->half.x = hx;
    obb->half.y = hy;
    obb->half.z = hz;
    obb->half.w = 1.0f;
    obb->local[0].x = hx;
    obb->local[0].y = hy;
    obb->local[0].z = hz;
    obb->local[0].w = 1.0f;
    obb->local[1].x = -hx;
    obb->local[1].y = hy;
    obb->local[1].z = hz;
    obb->local[1].w = 1.0f;
    obb->local[2].x = hx;
    obb->local[2].y = -hy;
    obb->local[2].z = hz;
    obb->local[2].w = 1.0f;
    obb->local[3].x = -hx;
    obb->local[3].y = -hy;
    obb->local[3].z = hz;
    obb->local[3].w = 1.0f;
    obb->local[4].x = hx;
    obb->local[4].y = hy;
    obb->local[4].z = -hz;
    obb->local[4].w = 1.0f;
    obb->local[5].x = -hx;
    obb->local[5].y = hy;
    obb->local[5].z = -hz;
    obb->local[5].w = 1.0f;
    obb->local[6].x = hx;
    obb->local[6].y = -hy;
    obb->local[6].z = -hz;
    obb->local[6].w = 1.0f;
    obb->local[7].x = -hx;
    obb->local[7].y = -hy;
    obb->local[7].z = -hz;
    obb->local[7].w = 1.0f;
}

/* Places an oriented box: copies the matrix, then takes the axes, the centre and the world corners from it. */
void ColObb_Update(ColObb *obb, void *mtx) {
    Mtx_Copy(obb->mtx, mtx);
    obb->axis[0].x = obb->mtx[0][0];
    obb->axis[0].y = obb->mtx[0][1];
    obb->axis[0].z = obb->mtx[0][2];
    obb->axis[1].x = obb->mtx[1][0];
    obb->axis[1].y = obb->mtx[1][1];
    obb->axis[1].z = obb->mtx[1][2];
    obb->axis[2].x = obb->mtx[2][0];
    obb->axis[2].y = obb->mtx[2][1];
    obb->axis[2].z = obb->mtx[2][2];
    obb->center.x = obb->mtx[3][0];
    obb->center.y = obb->mtx[3][1];
    obb->center.z = obb->mtx[3][2];
    Mtx_MulVec4(&obb->world[0], obb->mtx, &obb->local[0]);
    Mtx_MulVec4(&obb->world[1], obb->mtx, &obb->local[1]);
    Mtx_MulVec4(&obb->world[2], obb->mtx, &obb->local[2]);
    Mtx_MulVec4(&obb->world[3], obb->mtx, &obb->local[3]);
    Mtx_MulVec4(&obb->world[4], obb->mtx, &obb->local[4]);
    Mtx_MulVec4(&obb->world[5], obb->mtx, &obb->local[5]);
    Mtx_MulVec4(&obb->world[6], obb->mtx, &obb->local[6]);
    Mtx_MulVec4(&obb->world[7], obb->mtx, &obb->local[7]);
}

/* 1 when two boxes overlap or touch. */
s32 ColBox_Overlaps(ColBox *a, ColBox *b) {
    s32 ret = 0;

    if (!(a->min[0] > b->max[0]) && !(a->max[0] < b->min[0]) && !(a->min[1] > b->max[1]) &&
        !(a->max[1] < b->min[1]) && !(a->min[2] > b->max[2]) && !(a->max[2] < b->min[2])) {
        ret = 1;
    }
    return ret;
}

/*
 * Where a ray enters a box (Woo's method, "Fast Ray-Box Intersection"). Returns 1 and the entry point, or 1 and
 * the origin itself (w = 1) when the origin is inside or on the box. The ray is unbounded: `dir` is not
 * normalised and a hit beyond origin + dir still counts. The entry point may miss the face by 0.001 per axis.
 * A zero direction component on an axis where the origin is outside gives t = -1 for that axis (never chosen
 * over a real candidate); a ray that starts outside and points away gives 0. `out->w` is only written for the
 * inside case. Ties between candidate planes go to the lower axis (x, then y, then z).
 */
s32 ColBox_ClipRay(ColBox *box, ColRay *ray, ColVecA *out) {
    s8 side[3];
    f32 t[3];
    f32 plane[3];
    s32 inside = 1;
    s32 i;
    s32 best;

    for (i = 0; i < 3; i++) {
        if (ray->origin.v[i] < box->min[i]) {
            side[i] = 1;
            plane[i] = box->min[i];
            inside = 0;
        } else if (box->max[i] < ray->origin.v[i]) {
            side[i] = 0;
            plane[i] = box->max[i];
            inside = 0;
        } else {
            side[i] = 2;
        }
    }
    if (inside) {
        out->v[0] = ray->origin.v[0];
        out->v[1] = ray->origin.v[1];
        out->v[2] = ray->origin.v[2];
        out->v[3] = 1.0f;
        return 1;
    }
    for (i = 0; i < 3; i++) {
        if (side[i] != 2 && ray->dir.v[i] != 0.0f) {
            t[i] = (plane[i] - ray->origin.v[i]) / ray->dir.v[i];
        } else {
            t[i] = -1.0f;
        }
    }
    best = 0;
    for (i = 1; i < 3; i++) {
        if (t[best] < t[i]) {
            best = i;
        }
    }
    if (t[best] < 0.0f) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (best != i) {
            out->v[i] = ray->origin.v[i] + t[best] * ray->dir.v[i];
            if (out->v[i] < box->min[i] && __builtin_fabsf(box->min[i] - out->v[i]) > 0.001f) {
                return 0;
            }
            if (out->v[i] > box->max[i] && __builtin_fabsf(out->v[i] - box->max[i]) > 0.001f) {
                return 0;
            }
        } else {
            out->v[i] = plane[i];
        }
    }
    return 1;
}

/*
 * 1 when a segment touches a box: separating axes (the box's three, then the three cross products with the
 * segment), touching counts as a hit. The cross tests add 100 * FLT_MIN (0x03C80000) to the absolute direction,
 * which is nothing in practice, so a zero-length segment is simply a point-in-box test. No caller.
 */
s32 ColBox_TestSegment(ColBox *box, ColSeg *seg) {
    ColVec center;
    ColVec half;
    ColVec mid;
    ColVec dir;
    f32 ax, ay, az;

    ColBox_GetCenter(box, &center);
    ColBox_GetHalf(box, &half);
    ColSeg_GetMidpoint(seg, &mid);
    Vec4_Sub(&dir, &seg->b, &mid);
    Vec4_Sub(&mid, &mid, &center);
    ax = __builtin_fabsf(dir.x);
    if (__builtin_fabsf(mid.x) > half.x + ax) {
        return 0;
    }
    ay = __builtin_fabsf(dir.y);
    if (__builtin_fabsf(mid.y) > half.y + ay) {
        return 0;
    }
    az = __builtin_fabsf(dir.z);
    if (__builtin_fabsf(mid.z) > half.z + az) {
        return 0;
    }
    ax += 1.175494351e-36f;
    ay += 1.175494351e-36f;
    az += 1.175494351e-36f;
    if (__builtin_fabsf(mid.y * dir.z - mid.z * dir.y) > half.y * az + half.z * ay) {
        return 0;
    }
    if (__builtin_fabsf(mid.z * dir.x - mid.x * dir.z) > half.x * az + half.z * ax) {
        return 0;
    }
    if (__builtin_fabsf(mid.x * dir.y - mid.y * dir.x) > half.x * ay + half.y * ax) {
        return 0;
    }
    return 1;
}

/* 1 when a tree node's box overlaps a box. */
s32 ColNode_TestBox(ColNode *node, ColBox *box) {
    return ColBox_Overlaps(box, &node->box) != 0;
}

/* Tree walk of ColMesh_CollectBox: appends the polygon index of every leaf whose box overlaps `box`. */
void ColMesh_CollectNode(ColNode *nodes, ColBox *box, s32 idx) {
    ColNode *node = &nodes[idx];

    if (ColNode_TestBox(node, box)) {
        if (ColNode_IsLeaf(node)) {
            if (gColWalkBuf != NULL) {
                if (gColWalkCount >= gColWalkMax) {
                    return;
                }
                gColWalkBuf[gColWalkCount] = node->left;
            }
            gColWalkCount++;
        } else {
            ColMesh_CollectNode(nodes, box, node->left);
            ColMesh_CollectNode(nodes, box, node->right);
        }
    }
}

/* Collects the polygon indices of a mesh whose leaf boxes overlap `box` (at most `max`); returns the count. */
s32 ColMesh_CollectBox(s32 max, u16 *buf, ColMesh *mesh, ColBox *box) {
    ColNode *nodes;

    if (mesh->nodeCount <= 0) {
        return 0;
    }
    nodes = (ColNode *)(gColMeshBase + mesh->nodeOfs);
    gColWalkMax = max;
    gColWalkBuf = buf;
    gColWalkCount = 0;
    ColMesh_CollectNode(nodes, box, 0);
    gColWalkBuf = NULL;
    return gColWalkCount;
}

/* Tree walk of ColMesh_WalkBox: calls the callback for every leaf whose box overlaps `box`. */
void ColMesh_WalkNode(ColNode *nodes, ColBox *box, s32 idx) {
    ColNode *node = &nodes[idx];

    if (ColNode_TestBox(node, box)) {
        if (ColNode_IsLeaf(node)) {
            s32 ret = gColWalkCb(node, gColWalkCtx);

            gColWalkCount |= ret;
            return;
        }
        ColMesh_WalkNode(nodes, box, node->left);
        ColMesh_WalkNode(nodes, box, node->right);
    }
}

/* Calls `cb(leaf, ctx)` for every leaf of a mesh whose box overlaps `box`; returns the OR of the results. */
s32 ColMesh_WalkBox(ColMesh *mesh, ColBox *box, void *ctx, ColMeshCb cb) {
    ColNode *nodes;

    if (mesh->nodeCount <= 0) {
        return 0;
    }
    if (cb == NULL) {
        return 0;
    }
    nodes = (ColNode *)(gColMeshBase + mesh->nodeOfs);
    gColWalkBox = box;
    gColWalkCb = cb;
    gColWalkCtx = ctx;
    gColWalkCount = 0;
    ColMesh_WalkNode(nodes, box, 0);
    return gColWalkCount;
}

/* 1 when |a - b| <= eps (0 when either is not a number). */
s32 Col_NearEq(f32 a, f32 b, f32 eps) {
    if (__builtin_fabsf(a - b) <= eps) {
        return 1;
    }
    return 0;
}

/*
 * Parameters of the closest points of two lines p1 + s d1 and p2 + t d2 with unit directions.
 * As built the two guards are inverted: a direction whose squared length IS within 1e-6 of 1 gives s = t = 0,
 * so the formula only runs for directions that are not unit length. Parallel lines (d1.d2 squared within 1e-6 of
 * 1, or larger) give 0, 0 too. Only called by ColObb_Contact.
 */
void Col_LineLineParams(f32 *s, f32 *t, ColVec *p1, ColVec *d1, ColVec *p2, ColVec *d2) {
    ColVec diff;
    f32 a;
    f32 b;
    f32 c;
    f32 den;
    f32 inv;

    if (Col_NearEq(Vec3_LengthSq(d1), 1.0f, 0.000001f)) {
        *s = 0.0f;
        *t = 0.0f;
        return;
    }
    if (Col_NearEq(Vec3_LengthSq(d2), 1.0f, 0.000001f)) {
        *s = 0.0f;
        *t = 0.0f;
        return;
    }
    Vec4_Sub(&diff, p2, p1);
    a = Vec3_Dot(&diff, d1);
    b = Vec3_Dot(&diff, d2);
    c = Vec3_Dot(d1, d2);
    den = c * c - 1.0f;
    if (den < -0.000001f) {
        inv = 1.0f / den;
        *s = (c * b - a) * inv;
        *t = (b - c * a) * inv;
    } else {
        *s = 0.0f;
        *t = 0.0f;
    }
}

/* 1 when two oriented boxes overlap or touch (the fifteen separating axes, no tolerance). */
s32 ColObb_Overlaps(ColObb *a, ColObb *b) {
    ColVec ha;
    ColVec hb;
    ColVec diff;
    ColMat3 R;
    ColMat3 absR;
    ColVec d;
    f32 t;
    f32 rb;
    f32 sum;
    f32 ra;

    Vec3_Copy(&ha, &a->half);
    Vec3_Copy(&hb, &b->half);
    Vec4_Sub(&diff, &a->center, &b->center);
    R.m[0][0] = Vec3_Dot(&a->axis[0], &b->axis[0]);
    R.m[0][1] = Vec3_Dot(&a->axis[0], &b->axis[1]);
    R.m[0][2] = Vec3_Dot(&a->axis[0], &b->axis[2]);
    d.x = Vec3_Dot(&a->axis[0], &diff);
    absR.m[0][0] = __builtin_fabsf(R.m[0][0]);
    absR.m[0][1] = __builtin_fabsf(R.m[0][1]);
    absR.m[0][2] = __builtin_fabsf(R.m[0][2]);
    t = __builtin_fabsf(d.x);
    rb = hb.x * absR.m[0][0] + hb.y * absR.m[0][1] + hb.z * absR.m[0][2];
    sum = ha.x + rb;
    if (t > sum) {
        return 0;
    }
    R.m[1][0] = Vec3_Dot(&a->axis[1], &b->axis[0]);
    R.m[1][1] = Vec3_Dot(&a->axis[1], &b->axis[1]);
    R.m[1][2] = Vec3_Dot(&a->axis[1], &b->axis[2]);
    d.y = Vec3_Dot(&a->axis[1], &diff);
    absR.m[1][0] = __builtin_fabsf(R.m[1][0]);
    absR.m[1][1] = __builtin_fabsf(R.m[1][1]);
    absR.m[1][2] = __builtin_fabsf(R.m[1][2]);
    t = __builtin_fabsf(d.y);
    rb = hb.x * absR.m[1][0] + hb.y * absR.m[1][1] + hb.z * absR.m[1][2];
    sum = ha.y + rb;
    if (t > sum) {
        return 0;
    }
    R.m[2][0] = Vec3_Dot(&a->axis[2], &b->axis[0]);
    R.m[2][1] = Vec3_Dot(&a->axis[2], &b->axis[1]);
    R.m[2][2] = Vec3_Dot(&a->axis[2], &b->axis[2]);
    d.z = Vec3_Dot(&a->axis[2], &diff);
    absR.m[2][0] = __builtin_fabsf(R.m[2][0]);
    absR.m[2][1] = __builtin_fabsf(R.m[2][1]);
    absR.m[2][2] = __builtin_fabsf(R.m[2][2]);
    t = __builtin_fabsf(d.z);
    rb = hb.x * absR.m[2][0] + hb.y * absR.m[2][1] + hb.z * absR.m[2][2];
    sum = ha.z + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(Vec3_Dot(&b->axis[0], &diff));
    ra = ha.x * absR.m[0][0] + ha.y * absR.m[1][0] + ha.z * absR.m[2][0];
    sum = ra + hb.x;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(Vec3_Dot(&b->axis[1], &diff));
    ra = ha.x * absR.m[0][1] + ha.y * absR.m[1][1] + ha.z * absR.m[2][1];
    sum = ra + hb.y;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(Vec3_Dot(&b->axis[2], &diff));
    ra = ha.x * absR.m[0][2] + ha.y * absR.m[1][2] + ha.z * absR.m[2][2];
    sum = ra + hb.z;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.z * R.m[1][0] - d.y * R.m[2][0]);
    ra = ha.y * absR.m[2][0] + ha.z * absR.m[1][0];
    rb = hb.y * absR.m[0][2] + hb.z * absR.m[0][1];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.z * R.m[1][1] - d.y * R.m[2][1]);
    ra = ha.y * absR.m[2][1] + ha.z * absR.m[1][1];
    rb = hb.x * absR.m[0][2] + hb.z * absR.m[0][0];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.z * R.m[1][2] - d.y * R.m[2][2]);
    ra = ha.y * absR.m[2][2] + ha.z * absR.m[1][2];
    rb = hb.x * absR.m[0][1] + hb.y * absR.m[0][0];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.x * R.m[2][0] - d.z * R.m[0][0]);
    ra = ha.x * absR.m[2][0] + ha.z * absR.m[0][0];
    rb = hb.y * absR.m[1][2] + hb.z * absR.m[1][1];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.x * R.m[2][1] - d.z * R.m[0][1]);
    ra = ha.x * absR.m[2][1] + ha.z * absR.m[0][1];
    rb = hb.x * absR.m[1][2] + hb.z * absR.m[1][0];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.x * R.m[2][2] - d.z * R.m[0][2]);
    ra = ha.x * absR.m[2][2] + ha.z * absR.m[0][2];
    rb = hb.x * absR.m[1][1] + hb.y * absR.m[1][0];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.y * R.m[0][0] - d.x * R.m[1][0]);
    ra = ha.x * absR.m[1][0] + ha.y * absR.m[0][0];
    rb = hb.y * absR.m[2][2] + hb.z * absR.m[2][1];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.y * R.m[0][1] - d.x * R.m[1][1]);
    ra = ha.x * absR.m[1][1] + ha.y * absR.m[0][1];
    rb = hb.x * absR.m[2][2] + hb.z * absR.m[2][0];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    t = __builtin_fabsf(d.y * R.m[0][2] - d.x * R.m[1][2]);
    ra = ha.x * absR.m[1][2] + ha.y * absR.m[0][2];
    rb = hb.x * absR.m[2][1] + hb.y * absR.m[2][0];
    sum = ra + rb;
    if (t > sum) {
        return 0;
    }
    return 1;
}

/*
 * Contact between two oriented boxes: least-penetration axis, then a contact normal and point. 0 when the boxes
 * are apart. NO CALLER anywhere in the game (dead code), and it does not do what it was meant to:
 *   - the nine edge-edge axes are only considered when two ways of computing the same quantity DISAGREE by more
 *     than 1e-6 (an inverted check, like the one in Col_LineLineParams), and the first of them never is;
 *   - Col_LineLineParams returns 0, 0 for unit axes, so an edge contact is just the chosen corner of box a.
 * Left in assembly (0x4258 bytes, a nine-way jump table at 0x2F2170 whose cases are written out by hand).
 * What differs in the attempt below: it folds the nine edge cases into one indexed block, so it produces no jump
 * table and a different frame; only the fifteen-axis search and the two face cases follow the original's shape.
 */
#if 0
extern f32 D_002FEBA8[];     /* 0x2FEBA8: FLT_MAX (initialised data, reached with lui / lwc1, so not a small extern) */
extern f32 sqrtf(f32 x);
extern void Vec3_Normalize(ColVec *dst, ColVec *src);

#define COL_AXIS_KEEP(code_)   \
    if (d < 0.0f) {            \
        return 0;              \
    }                          \
    if (d < *depth) {          \
        *depth = d;            \
        code = (code_);        \
    }

s32 ColObb_Contact(ColObb *a, ColObb *b, ColVec *normal, ColVec *point, f32 *depth) {
    ColVec diff;
    ColMat3 R;
    ColMat3 absR;
    ColVecA dA;
    ColVecA dB;
    ColVecA pA;
    ColVecA pB;
    ColVec n;
    s32 sgn[3];
    f32 s;
    f32 u;
    f32 t;
    f32 ra;
    f32 rb;
    f32 sum;
    f32 d;
    f32 len;
    f32 w;
    f32 *ha = &a->half.x;
    f32 *hb = &b->half.x;
    s32 code = -1;
    s32 i;
    s32 j;
    s32 i1;
    s32 i2;
    s32 j1;
    s32 j2;
    s32 k;
    s32 m;
    s32 eq;

    *depth = D_002FEBA8[0];
    Vec4_Sub(&diff, &b->center, &a->center);

    /* faces of a: codes 0..2 */
    for (i = 0; i < 3; i++) {
        R.m[i][0] = Vec3_Dot(&a->axis[i], &b->axis[0]);
        R.m[i][1] = Vec3_Dot(&a->axis[i], &b->axis[1]);
        R.m[i][2] = Vec3_Dot(&a->axis[i], &b->axis[2]);
        dA.v[i] = Vec3_Dot(&a->axis[i], &diff);
        absR.m[i][0] = __builtin_fabsf(R.m[i][0]);
        absR.m[i][1] = __builtin_fabsf(R.m[i][1]);
        absR.m[i][2] = __builtin_fabsf(R.m[i][2]);
        t = __builtin_fabsf(dA.v[i]);
        rb = hb[0] * absR.m[i][0] + hb[1] * absR.m[i][1] + hb[2] * absR.m[i][2];
        sum = ha[i] + rb;
        d = sum - t;
        COL_AXIS_KEEP(i)
    }
    /* faces of b: codes 3..5 */
    for (j = 0; j < 3; j++) {
        dB.v[j] = Vec3_Dot(&b->axis[j], &diff);
        t = __builtin_fabsf(dB.v[j]);
        ra = ha[0] * absR.m[0][j] + ha[1] * absR.m[1][j] + ha[2] * absR.m[2][j];
        sum = ra + hb[j];
        d = sum - t;
        COL_AXIS_KEEP(j + 3)
    }
    /* edge of a x edge of b: codes 6 + 3 i + j. lo / hi are the other two indices in ascending order. */
    for (i = 0; i < 3; i++) {
        i1 = (i == 0) ? 1 : 0;
        i2 = (i == 2) ? 1 : 2;
        for (j = 0; j < 3; j++) {
            j1 = (j == 0) ? 1 : 0;
            j2 = (j == 2) ? 1 : 2;
            /* length of the cross product, from column j of R; code 6 alone takes it from row i */
            if (i == 0 && j == 0) {
                len = sqrtf(R.m[0][2] * R.m[0][2] + R.m[0][1] * R.m[0][1]);
            } else {
                len = sqrtf(R.m[i2][j] * R.m[i2][j] + R.m[i1][j] * R.m[i1][j]);
            }
            t = __builtin_fabsf(dA.v[i2] * R.m[i1][j] - dA.v[i1] * R.m[i2][j]);
            /* the same two values again, from b's side */
            eq = Col_NearEq(__builtin_fabsf(dB.v[j1] * R.m[i][j2] - dB.v[j2] * R.m[i][j1]), t, 0.000001f);
            eq |= Col_NearEq(len, sqrtf(R.m[i][j2] * R.m[i][j2] + R.m[i][j1] * R.m[i][j1]), 0.000001f);
            if (eq == 0) {
                ra = ha[i1] * absR.m[i2][j] + ha[i2] * absR.m[i1][j];
                rb = hb[j1] * absR.m[i][j2] + hb[j2] * absR.m[i][j1];
                sum = ra + rb;
                if (len == 0.0f) {
                    len = 0.000001f;
                }
                d = (sum - t) / len;
                if (d != 0.0f) {
                    COL_AXIS_KEEP(6 + i * 3 + j)
                }
            }
        }
    }

    if (code < 3) {
        /* face of a: the corner of b that is deepest along the axis */
        sgn[0] = (R.m[code][0] > -0.00001f) ? 1 : -1;
        sgn[1] = (R.m[code][1] > -0.00001f) ? 1 : -1;
        sgn[2] = (R.m[code][2] > -0.00001f) ? 1 : -1;
        if (!(dA.v[code] > 0.0f)) {
            normal->x = a->axis[code].x;
            normal->y = a->axis[code].y;
            normal->z = a->axis[code].z;
            point->x = b->center.x + sgn[0] * b->axis[0].x * hb[0] + sgn[1] * b->axis[1].x * hb[1] +
                       sgn[2] * b->axis[2].x * hb[2];
            point->y = b->center.y + sgn[0] * b->axis[0].y * hb[0] + sgn[1] * b->axis[1].y * hb[1] +
                       sgn[2] * b->axis[2].y * hb[2];
            point->z = b->center.z + sgn[0] * b->axis[0].z * hb[0] + sgn[1] * b->axis[1].z * hb[1] +
                       sgn[2] * b->axis[2].z * hb[2];
        } else {
            normal->x = -a->axis[code].x;
            normal->y = -a->axis[code].y;
            normal->z = -a->axis[code].z;
            point->x = b->center.x - sgn[0] * b->axis[0].x * hb[0] - sgn[1] * b->axis[1].x * hb[1] -
                       sgn[2] * b->axis[2].x * hb[2];
            point->y = b->center.y - sgn[0] * b->axis[0].y * hb[0] - sgn[1] * b->axis[1].y * hb[1] -
                       sgn[2] * b->axis[2].y * hb[2];
            point->z = b->center.z - sgn[0] * b->axis[0].z * hb[0] - sgn[1] * b->axis[1].z * hb[1] -
                       sgn[2] * b->axis[2].z * hb[2];
        }
    } else if (code < 6) {
        /* face of b: the corner of a that is deepest along the axis (the two branches mirror the ones above) */
        k = code - 3;
        sgn[0] = (R.m[0][k] > -0.00001f) ? 1 : -1;
        sgn[1] = (R.m[1][k] > -0.00001f) ? 1 : -1;
        sgn[2] = (R.m[2][k] > -0.00001f) ? 1 : -1;
        if (!(Vec3_Dot(&b->axis[k], &diff) > 0.0f)) {
            normal->x = b->axis[k].x;
            normal->y = b->axis[k].y;
            normal->z = b->axis[k].z;
            point->x = a->center.x - sgn[0] * a->axis[0].x * ha[0] - sgn[1] * a->axis[1].x * ha[1] -
                       sgn[2] * a->axis[2].x * ha[2];
            point->y = a->center.y - sgn[0] * a->axis[0].y * ha[0] - sgn[1] * a->axis[1].y * ha[1] -
                       sgn[2] * a->axis[2].y * ha[2];
            point->z = a->center.z - sgn[0] * a->axis[0].z * ha[0] - sgn[1] * a->axis[1].z * ha[1] -
                       sgn[2] * a->axis[2].z * ha[2];
        } else {
            normal->x = -b->axis[k].x;
            normal->y = -b->axis[k].y;
            normal->z = -b->axis[k].z;
            point->x = a->center.x + sgn[0] * a->axis[0].x * ha[0] + sgn[1] * a->axis[1].x * ha[1] +
                       sgn[2] * a->axis[2].x * ha[2];
            point->y = a->center.y + sgn[0] * a->axis[0].y * ha[0] + sgn[1] * a->axis[1].y * ha[1] +
                       sgn[2] * a->axis[2].y * ha[2];
            point->z = a->center.z + sgn[0] * a->axis[0].z * ha[0] + sgn[1] * a->axis[1].z * ha[1] +
                       sgn[2] * a->axis[2].z * ha[2];
        }
    } else if ((u32)(code - 6) < 9) {
        /* edge against edge (read from case 0 of the jump table; the other eight permute the indices) */
        f32 sa1;
        f32 sa2;
        f32 sb1;
        f32 sb2;

        i = (code - 6) / 3;
        j = (code - 6) % 3;
        i1 = (i + 1) % 3;
        i2 = (i + 2) % 3;
        j1 = (j + 1) % 3;
        j2 = (j + 2) % 3;
        sa1 = (R.m[i2][j] < 0.0f) ? 1 : -1;
        sa2 = (0.0f < R.m[i1][j]) ? 1 : -1;
        sb1 = (R.m[i][j2] < 0.0f) ? 1 : -1;
        sb2 = (0.0f < R.m[i][j1]) ? 1 : -1;
        /* the edge of a: its far end along -axis i, on the side the cross axis points to */
        w = -R.m[i2][j] * dA.v[i1] + R.m[i1][j] * dA.v[i2];
        if (ha[i1] * sa1 * (-R.m[i2][j] * w) + ha[i2] * sa2 * (R.m[i1][j] * w) > -0.00001f) {
            for (m = 0; m < 3; m++) {
                pA.v[m] = (&a->center.x)[m] + -(&a->axis[i].x)[m] * ha[i] + (&a->axis[i1].x)[m] * ha[i1] * sa1 +
                          (&a->axis[i2].x)[m] * ha[i2] * sa2;
            }
        } else {
            for (m = 0; m < 3; m++) {
                pA.v[m] = (&a->center.x)[m] + -(&a->axis[i].x)[m] * ha[i] + -(&a->axis[i1].x)[m] * ha[i1] * sa1 +
                          -(&a->axis[i2].x)[m] * ha[i2] * sa2;
            }
        }
        /* the edge of b, with the opposite choice */
        w = -R.m[i][j2] * dB.v[j1] + R.m[i][j1] * dB.v[j2];
        if (hb[j1] * sb1 * (-R.m[i][j2] * w) + hb[j2] * sb2 * (R.m[i][j1] * w) > -0.00001f) {
            for (m = 0; m < 3; m++) {
                pB.v[m] = (&b->center.x)[m] + -(&b->axis[j].x)[m] * hb[j] + -(&b->axis[j1].x)[m] * hb[j1] * sb1 +
                          -(&b->axis[j2].x)[m] * hb[j2] * sb2;
            }
        } else {
            for (m = 0; m < 3; m++) {
                pB.v[m] = (&b->center.x)[m] + -(&b->axis[j].x)[m] * hb[j] + (&b->axis[j1].x)[m] * hb[j1] * sb1 +
                          (&b->axis[j2].x)[m] * hb[j2] * sb2;
            }
        }
        Col_LineLineParams(&s, &u, (ColVec *)&pA, &a->axis[i], (ColVec *)&pB, &b->axis[j]);
        point->x = pA.v[0] + a->axis[i].x * s;
        point->y = pA.v[1] + a->axis[i].y * s;
        point->z = pA.v[2] + a->axis[i].z * s;
        n.x = pB.v[0] + b->axis[j].x * u - point->x;
        n.y = pB.v[1] + b->axis[j].y * u - point->y;
        n.z = pB.v[2] + b->axis[j].z * u - point->z;
        Vec3_Normalize(normal, &n);
    }
    return 1;
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/col_a", ColObb_Contact);
