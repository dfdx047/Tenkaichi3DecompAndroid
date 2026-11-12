/*
 * Collision primitive library, second part: 0x236190..0x239BB0 (include/battle/col_b.h has the shapes).
 *
 * Tests between spheres, segments, capsules, triangles and oriented boxes, the bounds builders the mesh walks are
 * fed with, and the shape builders every projectile's AddHit function calls. The first part (boxes, the mesh
 * walk, box against box) is src/battle/col_a.c.
 *
 * SIMULATION. Called by the stage collision (StgCol_*), the projectile hit detection (EftDet_*), the fighter body
 * test (BtlBodyHit_TestVolumes), the sight / approach / sweeping-beam segment traces and the debris bodies.
 *   - No function here contains VU0 code of its own, draws a random number or keeps state: there is no static
 *     buffer. Every result goes to the caller's pointers.
 *   - The arithmetic is of two kinds, and a port must keep both exactly:
 *       * sums of products written out in C (single-precision FPU, evaluated in the order of the source);
 *       * calls of the VU0 vector routines at 0x121E28.. (Vec4_Sub, Vec3_Dot, Vec3_Cross, Vec3_Normalize, ...),
 *         see the list below. Which of the two a given dot product uses is part of the behaviour.
 *   - sqrtf is the FPU instruction (the libm call only when the result is NaN, which a negative input gives).
 *
 * Sources recognised (the C below matches the original instructions, so the formulas are theirs):
 *   - ColTri_DistSqPoint: the point / triangle squared distance of the Magic Software library (MgcDist3DVecTri),
 *     with one check added in front.
 *   - ColSeg_NearestOnLine / NearestParallel / AdjustNearest / Nearest: "Fast, robust intersection of 3D line
 *     segments" (Game Programming Gems 2): FindNearestPointOnLineSegment, FindNearestPointOfParallelLineSegments,
 *     AdjustNearestPoints, IntersectLineSegments, with epsilon = 1e-6 and infinite_lines = false.
 *   - ColSphere_TestLine: the classic line / sphere quadratic (a, b, c, b*b - 4ac).
 *
 * Original bugs kept (all verified by the matching C):
 *   - ColSphere_ContactObb: when the centre is in front of a z face the normal is (axis[2].x, axis[2].x,
 *     axis[2].x) * sign(z), and in front of a y face (axis[1].x, axis[1].x, axis[1].x) * sign(z): the wrong
 *     components, and for the y face the wrong sign. Only the x face is right.
 *   - ColSphere_ContactSphere (no caller): the contact point is measured from a's centre with b's radius.
 *   - ColSweep_TestTri computes the vector from the contact to the centre (`off`) in all three cases and never
 *     returns it.
 */
#include "common.h"
#include "battle/col_b.h"

extern f32 sqrtf(f32 x);

/* VU0 vector routines (0x121E28..0x122230), hand-written macro-mode code outside this file:
     Vec4_Copy   128-bit move.
     Vec4_Add / Vec4_Sub   all four components (so the difference of two positions has w = 0).
     Vec4_Scale  all four components times s; Vec3_Scale  x, y, z times s, w copied from the input.
     Vec3_Dot    (x*x' + y*y') + VF3.x * (z*z'): three VU0 products, then two VU0 additions in that order. VF3 is
                 (1, 0, 0, 0), loaded once at boot (0x120088) and expected to stay.
     Vec3_Cross  out = a x b, w = 0.
     Vec3_Normalize   len = VU0 sqrt of the dot above, then q = 1 / len (VU0 divide), out.xyz = v.xyz * q, w = 0:
                 a multiplication by the reciprocal, not a division. A zero vector gives (0, 0, 0, 0).
     Vec3_LengthSq    squared length (the same sum as Vec3_Dot); Vec3_Dist  distance of two points (VU0 sqrt). */
extern void Vec4_Copy(CpVec *dst, CpVec *src);
extern void Vec4_Add(CpVec *out, CpVec *a, CpVec *b);
extern void Vec4_Sub(CpVec *out, CpVec *a, CpVec *b);
extern void Vec4_Scale(CpVec *out, CpVec *v, f32 s);
extern void Vec3_Scale(CpVec *out, CpVec *v, f32 s);
extern f32 Vec3_Dot(CpVec *a, CpVec *b);
extern void Vec3_Cross(CpVec *out, CpVec *a, CpVec *b);
extern void Vec3_Normalize(CpVec *out, CpVec *v);
extern f32 Vec3_LengthSq(CpVec *v);            /* squared length */
extern f32 Vec3_Dist(CpVec *a, CpVec *b);  /* distance between two points */

/* Box helpers (col_a.c). */
extern void StgAabb_SetEmpty(CpBox *box);
extern void ColBox_AddPoint(CpBox *box, CpVec *pt);


/* "No hit yet": 0x7F7FFFFE, one bit below FLT_MAX (this compiler's reading of the literal). It is too large for
   the assembler's li.s, so the compiler keeps it as a 4-byte constant in .sdata, one per function that uses it,
   read with lui / lwc1: 0x2FEBAC is ColSweep_TestTri's (this object's whole .sdata). */
#define CP_FLT_MAX 3.402823466e+38F
#define CP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define CP_MIN(a, b) ((a) > (b) ? (b) : (a))
#define CP_OUT_OF_RANGE(a) ((a) < 0.0f || (a) > 1.0f)

/* Two 2D lines p + t (q - p) and r + u (s - r): writes t of their crossing; 0 when they are parallel. */
s32 ColLine2_Intersect(f32 *p, f32 *q, f32 *r, f32 *s, f32 *t) {
    CpVec d1;
    CpVec d2;
    f32 det;

    d1.x = q[0] - p[0];
    d1.y = q[1] - p[1];
    d2.x = r[0] - s[0];
    d2.y = r[1] - s[1];
    det = d2.y * d1.x - d2.x * d1.y;
    if (det == 0.0f) {
        return 0;
    }
    if (t != NULL) {
        *t = (d2.x * (p[1] - r[1]) - d2.y * (p[0] - r[0])) / det;
    }
    return 1;
}

/* A sphere moved along sw->dir against a triangle: the distance travelled until the first touch and the centre's
   position there (x, y, z only). The distance is NOT limited to sw->length: the caller compares it.
     - Rejected at once when dir . normal > -0.001 (moving away or along the face) or when the start centre is
       more than a radius behind the plane.
     - Face: only when the start centre is more than a radius in front of the plane; the centre's position at the
       moment of touching must project inside the triangle (ColTri_ContainsPoint).
     - Corners: the line from each corner against the direction, cut with the start sphere; the smaller root, not
       negative. Replaces the best so far only when strictly smaller (face and earlier corners win ties).
     - Edges: in the plane holding the edge and the direction, the circle the sphere cuts out of that plane is
       moved along dir until it touches the line a..b (solved in 2D after dropping the plane normal's largest
       component: x only when |x| is strictly the largest, else y when |y| > |z|, else z); the touching point must
       lie between a and b (ends included). Replaces the best so far when smaller OR EQUAL (an edge wins ties
       against the face, a corner or an earlier edge).
   Edges are a (v[i]) .. b (v[i + 1], v[0] after v[2]). */
s32 ColSweep_TestTri(CpSweep *sw, CpTri *tri, CpVec *pos, f32 *dist) {
    CpVec off;
    CpVec dir;
    CpVec plane;
    CpVec p;
    CpVec q;
    CpVec e;
    CpVecA a;
    CpVecA b;
    CpVec pl;
    CpVec c;
    CpVec proj;
    CpVec near;
    CpVec n2;
    CpVecA hit;
    CpVecA hit2;
    CpVecA p0;
    CpVecA p1;
    CpVecA p2;
    CpVecA p3;
    CpVec at;
    CpVec va;
    CpVec vb;
    s32 cnt;
    f32 t0;
    f32 t1;
    f32 t;
    s32 kind;
    f32 best;
    f32 h;
    s32 i;

    Vec4_Copy(&dir, &sw->dir);
    if (Vec3_Dot(&dir, &tri->nrm) > -0.001f) {
        return 0;
    }
    best = CP_FLT_MAX;
    Vec4_Copy(&plane, &tri->nrm);
    plane.w = -plane.w;
    kind = -1;
    h = Vec3_Dot(&plane, &sw->sphere.pos) + plane.w;
    if (h < -sw->sphere.radius) {
        return 0;
    }
    if (h > sw->sphere.radius) {
        f32 dn;

        h -= sw->sphere.radius;
        dn = Vec3_Dot(&tri->nrm, &dir);
        if (dn != 0.0f) {
            h = -h / dn;
            p.x = sw->sphere.pos.x + dir.x * h;
            p.y = sw->sphere.pos.y + dir.y * h;
            p.z = sw->sphere.pos.z + dir.z * h;
            p.w = 1.0f;
            if (ColTri_ContainsPoint(tri, &p) && h < best) {
                Vec4_Copy(&off, &tri->nrm);
                best = h;
                kind = 0;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        t0 = CP_FLT_MAX;
        t1 = CP_FLT_MAX;
        Vec4_Copy(&p, &tri->v[i]);
        Vec4_Sub(&q, &p, &dir);
        Vec4_Sub(&e, &q, &p);
        if (ColSphere_TestLine(&sw->sphere, &p, &q, &cnt, &t0, &t1)) {
            f32 m = t0;

            if (t1 < m) {
                m = t1;
            }
            if (m < 0.0f) {
                continue;
            }
            if (m < best) {
                best = m;
                a.v[0] = p.x + e.x * best;
                a.v[1] = p.y + e.y * best;
                a.v[2] = p.z + e.z * best;
                kind = 1;
                Vec4_Sub(&off, &sw->sphere.pos, (CpVec *)&a);
            }
        }
    }
    for (i = 0; i < 3; i++) {
        f32 rad;
        f32 d;
        f32 ax;
        f32 ay;
        f32 az;
        s32 i0;
        s32 i1;

        s32 j;

        Vec4_Copy((CpVec *)&a, &tri->v[i]);
        j = i + 1;
        if (j == 3) {
            j = 0;
        }
        Vec4_Copy((CpVec *)&b, &tri->v[j]);
        i0 = 0;
        i1 = 1;
        Vec4_Sub(&c, (CpVec *)&b, &dir);
        ColPlane_FromPoints(&pl, (CpVec *)&a, (CpVec *)&b, &c);
        d = Vec3_Dot(&pl, &sw->sphere.pos) + pl.w;
        if (d > sw->sphere.radius) {
            continue;
        }
        if (d < -sw->sphere.radius) {
            continue;
        }
        rad = sqrtf(sw->sphere.radius * sw->sphere.radius - d * d);
        ColPlane_ProjectPoint(&proj, &pl, &sw->sphere.pos);
        ColLine_DistPoint(&proj, (CpVec *)&a, (CpVec *)&b, &near);
        Vec4_Sub(&n2, &near, &proj);
        Vec3_Normalize(&n2, &n2);
        hit.v[0] = n2.x * rad + proj.x;
        hit.v[1] = n2.y * rad + proj.y;
        hit.v[2] = n2.z * rad + proj.z;
        ax = __builtin_fabsf(pl.x);
        ay = __builtin_fabsf(pl.y);
        az = __builtin_fabsf(pl.z);
        if (ax > ay && ax > az) {
            i0 = 1;
            i1 = 2;
        } else if (ay > az) {
            i0 = 0;
            i1 = 2;
        }
        Vec4_Add((CpVec *)&hit2, (CpVec *)&hit, &dir);
        p0.v[0] = hit.v[i0];
        p0.v[1] = hit.v[i1];
        p1.v[0] = hit2.v[i0];
        p1.v[1] = hit2.v[i1];
        p2.v[0] = a.v[i0];
        p2.v[1] = a.v[i1];
        p3.v[0] = b.v[i0];
        p3.v[1] = b.v[i1];
        if (ColLine2_Intersect(p0.v, p1.v, p2.v, p3.v, &t)) {
            if (t < 0.0f) {
                continue;
            }
            at.x = hit.v[0] + dir.x * t;
            at.y = hit.v[1] + dir.y * t;
            at.z = hit.v[2] + dir.z * t;
            Vec4_Sub(&va, (CpVec *)&a, &at);
            Vec4_Sub(&vb, (CpVec *)&b, &at);
            if (Vec3_Dot(&va, &vb) > 0.0f) {
                continue;
            }
            if (t > best) {
                continue;
            }
            best = t;
            Vec4_Sub(&off, &sw->sphere.pos, (CpVec *)&hit);
            kind = 2;
        }
    }
    if (kind != -1) {
        pos->x = sw->cap.a.x + best * dir.x;
        pos->y = sw->cap.a.y + best * dir.y;
        pos->z = sw->cap.a.z + best * dir.z;
        *dist = best;
    }
    return kind != -1;
}

/* Segment a..b against the front side of a triangle (the side (v1 - v0) x (v2 - v0) points to; the stored plane
   is not used): the crossing point (w = 1) and its fraction along the segment, both ends and the triangle's edges
   included. A segment parallel to the plane or coming from behind misses. *dist is also written (with an
   unscaled value) when the test fails after the plane check. */
s32 ColSeg_TestTri(CpSeg *seg, CpTri *tri, CpVec *pos, f32 *dist) {
    CpVec e1;
    CpVec e2;
    CpVec n;
    CpVec d;
    CpVec c;
    CpVec w;
    f32 det;
    f32 u;
    f32 v;
    f32 inv;
    f32 k;
    s32 ret;

    Vec4_Sub(&e1, &tri->v[1], &tri->v[0]);
    Vec4_Sub(&e2, &tri->v[2], &tri->v[0]);
    Vec4_Sub(&d, &seg->a, &seg->b);
    Vec3_Cross(&n, &e1, &e2);
    det = Vec3_Dot(&d, &n);
    if (det <= 0.0f) {
        return 0;
    }
    Vec4_Sub(&w, &seg->a, &tri->v[0]);
    *dist = Vec3_Dot(&w, &n);
    if (*dist < 0.0f) {
        return 0;
    }
    if (*dist > det) {
        return 0;
    }
    Vec3_Cross(&c, &d, &w);
    u = Vec3_Dot(&e2, &c);
    if (u < 0.0f || u > det) {
        return 0;
    }
    v = -Vec3_Dot(&e1, &c);
    if (v < 0.0f || u + v > det) {
        return 0;
    }
    ret = 1;
    inv = 1.0f / det;
    u *= inv;
    v *= inv;
    *dist *= inv;
    k = 1.0f - u - v;
    pos->x = tri->v[0].x * k + tri->v[1].x * u + tri->v[2].x * v;
    pos->y = tri->v[0].y * k + tri->v[1].y * u + tri->v[2].y * v;
    pos->z = tri->v[0].z * k + tri->v[1].z * u + tri->v[2].z * v;
    pos->w = 1.0f;
    return ret;
}

/* Sphere against an oriented box: contact normal, the point of the sphere's surface facing the box and the
   depth. Two of the three face cases use the wrong axis components (see the header notes). */
s32 ColSphere_ContactObb(CpSphere *sph, CpObb *obb, CpVec *nrm, CpVec *pos, f32 *depth) {
    CpVec d;
    CpVec p;
    CpVec loc;
    CpVec l;
    f32 sx;
    f32 sy;
    f32 sz;

    Vec4_Sub(&d, &sph->pos, &obb->center);
    loc.x = d.x * obb->axis[0].x + d.y * obb->axis[0].y + d.z * obb->axis[0].z;
    loc.y = d.x * obb->axis[1].x + d.y * obb->axis[1].y + d.z * obb->axis[1].z;
    loc.z = d.x * obb->axis[2].x + d.y * obb->axis[2].y + d.z * obb->axis[2].z;
    sz = 1.0f;
    sy = 1.0f;
    sx = 1.0f;
    Vec4_Copy(&p, &loc);
    if (p.x < 0.0f) {
        p.x = -p.x;
        sx = -1.0f;
    }
    if (p.y < 0.0f) {
        p.y = -p.y;
        sy = -1.0f;
    }
    if (p.z < 0.0f) {
        p.z = -p.z;
        sz = -1.0f;
    }
    p.x -= obb->half.x;
    p.y -= obb->half.y;
    p.z -= obb->half.z;
    if (p.x > sph->radius || p.y > sph->radius || p.z > sph->radius) {
        return 0;
    }
    if (p.x < 0.0f) {
        if (p.y < 0.0f) {
            nrm->x = obb->axis[2].x * sz;
            nrm->y = obb->axis[2].x * sz;
            nrm->z = obb->axis[2].x * sz;
            pos->x = sph->pos.x - nrm->x * sph->radius;
            pos->y = sph->pos.y - nrm->y * sph->radius;
            pos->z = sph->pos.z - nrm->z * sph->radius;
            *depth = sph->radius - p.z;
            return 1;
        } else if (p.z < 0.0f) {
            nrm->x = obb->axis[1].x * sz;
            nrm->y = obb->axis[1].x * sz;
            nrm->z = obb->axis[1].x * sz;
            pos->x = sph->pos.x - nrm->x * sph->radius;
            pos->y = sph->pos.y - nrm->y * sph->radius;
            pos->z = sph->pos.z - nrm->z * sph->radius;
            *depth = sph->radius - p.y;
            return 1;
        } else {
            f32 dd = p.y * p.y + p.z * p.z;
            f32 dist;

            if (dd <= sph->radius * sph->radius) {
                dist = sqrtf(dd);
                l.x = 0.0f;
                l.y = sy * p.y / dist;
                l.z = sz * p.z / dist;
                nrm->x = obb->axis[0].x * l.x + l.y * obb->axis[1].x + l.z * obb->axis[2].x;
                nrm->y = obb->axis[0].y * l.x + l.y * obb->axis[1].y + l.z * obb->axis[2].y;
                nrm->z = obb->axis[0].z * l.x + l.y * obb->axis[1].z + l.z * obb->axis[2].z;
                pos->x = sph->pos.x - nrm->x * sph->radius;
                pos->y = sph->pos.y - nrm->y * sph->radius;
                pos->z = sph->pos.z - nrm->z * sph->radius;
                *depth = sph->radius - dist;
                return 1;
            }
        }
    } else if (p.y < 0.0f) {
        if (p.z < 0.0f) {
            nrm->x = obb->axis[0].x * sx;
            nrm->y = obb->axis[0].y * sx;
            nrm->z = obb->axis[0].z * sx;
            pos->x = sph->pos.x - nrm->x * sph->radius;
            pos->y = sph->pos.y - nrm->y * sph->radius;
            pos->z = sph->pos.z - nrm->z * sph->radius;
            *depth = sph->radius - p.x;
            return 1;
        } else {
            f32 dd = p.x * p.x + p.z * p.z;
            f32 dist;

            if (dd <= sph->radius * sph->radius) {
                dist = sqrtf(dd);
                l.y = 0.0f;
                l.x = sx * p.x / dist;
                l.z = sz * p.z / dist;
                nrm->x = l.x * obb->axis[0].x + obb->axis[1].x * l.y + l.z * obb->axis[2].x;
                nrm->y = l.x * obb->axis[0].y + obb->axis[1].y * l.y + l.z * obb->axis[2].y;
                nrm->z = l.x * obb->axis[0].z + obb->axis[1].z * l.y + l.z * obb->axis[2].z;
                pos->x = sph->pos.x - nrm->x * sph->radius;
                pos->y = sph->pos.y - nrm->y * sph->radius;
                pos->z = sph->pos.z - nrm->z * sph->radius;
                *depth = sph->radius - dist;
                return 1;
            }
        }
    } else if (p.z < 0.0f) {
        f32 dd = p.x * p.x + p.y * p.y;
        f32 dist;

        if (dd <= sph->radius * sph->radius) {
            dist = sqrtf(dd);
            l.z = 0.0f;
            l.x = sx * p.x / dist;
            l.y = sy * p.y / dist;
            nrm->x = l.x * obb->axis[0].x + l.y * obb->axis[1].x + obb->axis[2].x * l.z;
            nrm->y = l.x * obb->axis[0].y + l.y * obb->axis[1].y + obb->axis[2].y * l.z;
            nrm->z = l.x * obb->axis[0].z + l.y * obb->axis[1].z + obb->axis[2].z * l.z;
            pos->x = sph->pos.x - nrm->x * sph->radius;
            pos->y = sph->pos.y - nrm->y * sph->radius;
            pos->z = sph->pos.z - nrm->z * sph->radius;
            *depth = sph->radius - dist;
            return 1;
        }
    } else {
        f32 dd = p.x * p.x + p.y * p.y + p.z * p.z;
        f32 dist;

        if (dd <= sph->radius * sph->radius) {
            dist = sqrtf(dd);
            l.x = sx * p.x / dist;
            l.y = sy * p.y / dist;
            l.z = sz * p.z / dist;
            nrm->x = l.x * obb->axis[0].x + l.y * obb->axis[1].x + l.z * obb->axis[2].x;
            nrm->y = l.x * obb->axis[0].y + l.y * obb->axis[1].y + l.z * obb->axis[2].y;
            nrm->z = l.x * obb->axis[0].z + l.y * obb->axis[1].z + l.z * obb->axis[2].z;
            pos->x = sph->pos.x - nrm->x * sph->radius;
            pos->y = sph->pos.y - nrm->y * sph->radius;
            pos->z = sph->pos.z - nrm->z * sph->radius;
            *depth = sph->radius - dist;
            return 1;
        }
    }
    return 0;
}

/* Sphere against an oriented box, yes or no. */
s32 ColSphere_TestObb(CpSphere *sph, CpObb *obb) {
    CpVec d;
    CpVec p;
    CpVec loc;

    Vec4_Sub(&d, &sph->pos, &obb->center);
    loc.x = d.x * obb->axis[0].x + d.y * obb->axis[0].y + d.z * obb->axis[0].z;
    loc.y = d.x * obb->axis[1].x + d.y * obb->axis[1].y + d.z * obb->axis[1].z;
    loc.z = d.x * obb->axis[2].x + d.y * obb->axis[2].y + d.z * obb->axis[2].z;
    Vec4_Copy(&p, &loc);
    if (p.x < 0.0f) {
        p.x = -p.x;
    }
    if (p.y < 0.0f) {
        p.y = -p.y;
    }
    if (p.z < 0.0f) {
        p.z = -p.z;
    }
    p.x -= obb->half.x;
    p.y -= obb->half.y;
    p.z -= obb->half.z;
    if (p.x > sph->radius || p.y > sph->radius || p.z > sph->radius) {
        return 0;
    }
    if (p.x < 0.0f) {
        if (p.y < 0.0f) {
            return 1;
        }
        if (p.z < 0.0f) {
            return 1;
        }
        return p.y * p.y + p.z * p.z <= sph->radius * sph->radius;
    }
    if (p.y < 0.0f) {
        if (p.z < 0.0f) {
            return 1;
        }
        return p.x * p.x + p.z * p.z <= sph->radius * sph->radius;
    }
    if (p.z < 0.0f) {
        if (p.x * p.x + p.y * p.y <= sph->radius * sph->radius) {
            return 1;
        }
    } else if (p.x * p.x + p.y * p.y + p.z * p.z <= sph->radius * sph->radius) {
        return 1;
    }
    return 0;
}

/* Ray (origin, unit direction) against a sphere: distance to the entry point, 0 when the origin is inside. */
s32 ColSphere_TestRay(CpSphere *sph, CpRay *ray, f32 *t, CpVec *pos) {
    CpVec d;
    f32 b;
    f32 c;
    f32 disc;

    Vec4_Sub(&d, &ray->origin, &sph->pos);
    b = Vec3_Dot(&d, &ray->dir);
    c = Vec3_Dot(&d, &d) - sph->radius * sph->radius;
    if (c > 0.0f && b > 0.0f) {
        return 0;
    }
    disc = b * b - c;
    if (disc < 0.0f) {
        return 0;
    }
    *t = -b - sqrtf(disc);
    if (*t < 0.0f) {
        *t = 0.0f;
    }
    pos->x = ray->origin.x + *t * ray->dir.x;
    pos->y = ray->origin.y + *t * ray->dir.y;
    pos->z = ray->origin.z + *t * ray->dir.z;
    pos->w = 1.0f;
    return 1;
}

/* The line a + t (b - a) against a sphere: number of roots (1 or 2) and both parameters, larger root first. */
s32 ColSphere_TestLine(CpSphere *sph, CpVec *a, CpVec *b, s32 *count, f32 *t0, f32 *t1) {
    f32 qa;
    f32 qb;
    f32 qc;
    f32 disc;

    qa = (b->x - a->x) * (b->x - a->x) + (b->y - a->y) * (b->y - a->y) + (b->z - a->z) * (b->z - a->z);
    qb = 2.0f * ((b->x - a->x) * (a->x - sph->pos.x) + (b->y - a->y) * (a->y - sph->pos.y) +
                 (b->z - a->z) * (a->z - sph->pos.z));
    qc = sph->pos.x * sph->pos.x + sph->pos.y * sph->pos.y + sph->pos.z * sph->pos.z + a->x * a->x + a->y * a->y +
         a->z * a->z - 2.0f * (sph->pos.x * a->x + sph->pos.y * a->y + sph->pos.z * a->z) -
         sph->radius * sph->radius;
    disc = qb * qb - 4.0f * qa * qc;
    if (disc < 0.0f) {
        return 0;
    }
    if (qa == 0.0f) {
        qa = 0.0001f;
    }
    if (disc == 0.0f) {
        if (count != NULL) {
            *count = 1;
        }
        if (t0 != NULL) {
            *t0 = -qb / (2.0f * qa);
        }
        return 1;
    }
    if (count != NULL) {
        *count = 2;
    }
    if (t0 != NULL) {
        *t0 = (-qb + sqrtf(qb * qb - 4.0f * qa * qc)) / (2.0f * qa);
    }
    if (t1 != NULL) {
        *t1 = (-qb - sqrtf(qb * qb - 4.0f * qa * qc)) / (2.0f * qa);
    }
    return 1;
}

/* Two spheres overlap or touch. */
s32 ColSphere_TestSphere(CpSphere *a, CpSphere *b) {
    CpVec d;
    f32 r = a->radius + b->radius;

    Vec4_Sub(&d, &a->pos, &b->pos);
    if (Vec3_LengthSq(&d) <= r * r) {
        return 1;
    }
    return 0;
}

/* Two spheres overlap: unit normal from b to a, a contact point and the depth. */
s32 ColSphere_ContactSphere(CpSphere *a, CpSphere *b, CpVec *nrm, CpVec *pos, f32 *depth) {
    CpVec d;
    f32 r = a->radius + b->radius;
    f32 dist;

    Vec4_Sub(&d, &a->pos, &b->pos);
    dist = Vec3_LengthSq(&d);
    if (!(dist <= r * r)) {
        return 0;
    }
    dist = sqrtf(dist);
    *depth = r - dist;
    nrm->x = d.x / dist;
    nrm->y = d.y / dist;
    nrm->z = d.z / dist;
    pos->x = a->pos.x + nrm->x * (b->radius - *depth * 0.5f);
    pos->y = a->pos.y + nrm->y * (b->radius - *depth * 0.5f);
    pos->z = a->pos.z + nrm->z * (b->radius - *depth * 0.5f);
    Vec3_Normalize(nrm, nrm);
    return 1;
}

/* Two spheres moving by moveA / moveB during the frame: the fraction of the frame at which they first touch.
   Returns 1 with *t = 0 when they overlap at the start (strictly: centres closer than the radii). Returns 0 when
   the squared relative movement is below 1.1754944e-36, when they are not approaching (relative movement .
   offset >= 0) or when the paths never touch. *t is not limited to 0..1: the caller tests it. */
s32 ColSphere_SweepSphere(CpSphere *a, CpSphere *b, CpVec *moveA, CpVec *moveB, f32 *t) {
    CpVec d;
    CpVec v;
    f32 r;
    f32 qa;
    f32 qb;
    f32 qc;
    f32 disc;

    Vec4_Sub(&d, &b->pos, &a->pos);
    Vec4_Sub(&v, moveB, moveA);
    r = b->radius + a->radius;
    qc = Vec3_Dot(&d, &d) - r * r;
    if (qc < 0.0f) {
        *t = 0.0f;
        return 1;
    }
    qa = Vec3_Dot(&v, &v);
    if (qa < 1.1754944e-36f) {
        return 0;
    }
    qb = Vec3_Dot(&v, &d);
    if (qb >= 0.0f) {
        return 0;
    }
    disc = qb * qb - qa * qc;
    if (disc < 0.0f) {
        return 0;
    }
    *t = (-qb - sqrtf(disc)) / qa;
    return 1;
}

/* Two spheres that touch (or are no more than 0.001 apart): the unit direction a -> b ((1, 0, 0) when the centres
   coincide) and each centre moved half the overlap plus 0.001 away from the other (x, y, z only). */
s32 ColSphere_SeparateSphere(CpSphere *a, CpSphere *b, CpVec *posA, CpVec *posB, CpVec *nrm) {
    f32 dist;
    f32 r;
    f32 push;

    Vec4_Sub(nrm, &b->pos, &a->pos);
    dist = sqrtf(Vec3_Dot(nrm, nrm));
    push = dist;
    if (push != 0.0f) {
        f32 inv = 1.0f / push;

        nrm->x *= inv;
        nrm->y *= inv;
        nrm->z *= inv;
        nrm->w = 1.0f;
    } else {
        nrm->x = 1.0f;
        nrm->y = 0.0f;
        nrm->z = 0.0f;
        nrm->w = 1.0f;
    }
    r = a->radius + b->radius;
    if (dist > r) {
        if (__builtin_fabsf(dist - r) > 0.001f) {
            return 0;
        }
    }
    push = (r - dist) * 0.5f + 0.001f;
    posA->x = a->pos.x - nrm->x * push;
    posA->y = a->pos.y - nrm->y * push;
    posA->z = a->pos.z - nrm->z * push;
    posB->x = b->pos.x + nrm->x * push;
    posB->y = b->pos.y + nrm->y * push;
    posB->z = b->pos.z + nrm->z * push;
    return 1;
}

/* Squared distance from a point to a triangle, with the parameters (s along v0->v1, t along v0->v2) of the
   nearest point. A degenerate triangle gives 1e8 and leaves s and t unwritten. */
f32 ColTri_DistSqPoint(CpVec *pt, CpTri *tri, f32 *s, f32 *t) {
    CpVec e0;
    CpVec e1;
    CpVec diff;
    f32 fA00;
    f32 fA01;
    f32 fA11;
    f32 fB0;
    f32 fB1;
    f32 fC;
    f32 fDet;
    f32 fS;
    f32 fT;
    f32 fSqrDist;

    Vec4_Sub(&e0, &tri->v[1], &tri->v[0]);
    Vec4_Sub(&e1, &tri->v[2], &tri->v[0]);
    Vec4_Sub(&diff, &tri->v[0], pt);
    fA00 = Vec3_Dot(&e0, &e0);
    fA01 = Vec3_Dot(&e0, &e1);
    fA11 = Vec3_Dot(&e1, &e1);
    fB0 = Vec3_Dot(&diff, &e0);
    fB1 = Vec3_Dot(&diff, &e1);
    fC = Vec3_Dot(&diff, &diff);
    fDet = __builtin_fabsf(fA00 * fA11 - fA01 * fA01);
    fS = fA01 * fB1 - fA11 * fB0;
    fT = fA01 * fB0 - fA00 * fB1;
    if (__builtin_fabsf(fDet) < 1e-9f) {
        return 100000000.0f;
    }
    if (fS + fT <= fDet) {
        if (fS < 0.0f) {
            if (fT < 0.0f) {
                if (fB0 < 0.0f) {
                    fT = 0.0f;
                    if (-fB0 >= fA00) {
                        fS = 1.0f;
                        fSqrDist = fA00 + 2.0f * fB0 + fC;
                    } else {
                        fS = -fB0 / fA00;
                        fSqrDist = fB0 * fS + fC;
                    }
                } else {
                    fS = 0.0f;
                    if (fB1 >= 0.0f) {
                        fT = 0.0f;
                        fSqrDist = fC;
                    } else if (-fB1 >= fA11) {
                        fT = 1.0f;
                        fSqrDist = fA11 + 2.0f * fB1 + fC;
                    } else {
                        fT = -fB1 / fA11;
                        fSqrDist = fB1 * fT + fC;
                    }
                }
            } else {
                fS = 0.0f;
                if (fB1 >= 0.0f) {
                    fT = 0.0f;
                    fSqrDist = fC;
                } else if (-fB1 >= fA11) {
                    fT = 1.0f;
                    fSqrDist = fA11 + 2.0f * fB1 + fC;
                } else {
                    fT = -fB1 / fA11;
                    fSqrDist = fB1 * fT + fC;
                }
            }
        } else if (fT < 0.0f) {
            fT = 0.0f;
            if (fB0 >= 0.0f) {
                fS = 0.0f;
                fSqrDist = fC;
            } else if (-fB0 >= fA00) {
                fS = 1.0f;
                fSqrDist = fA00 + 2.0f * fB0 + fC;
            } else {
                fS = -fB0 / fA00;
                fSqrDist = fB0 * fS + fC;
            }
        } else {
            f32 fInvDet = 1.0f / fDet;

            fS *= fInvDet;
            fT *= fInvDet;
            fSqrDist = fS * (fA00 * fS + fA01 * fT + 2.0f * fB0) + fT * (fA01 * fS + fA11 * fT + 2.0f * fB1) + fC;
        }
    } else {
        f32 fTmp0;
        f32 fTmp1;
        f32 fNumer;
        f32 fDenom;

        if (fS < 0.0f) {
            fTmp0 = fA01 + fB0;
            fTmp1 = fA11 + fB1;
            if (fTmp1 > fTmp0) {
                fNumer = fTmp1 - fTmp0;
                fDenom = fA00 - 2.0f * fA01 + fA11;
                if (fNumer >= fDenom) {
                    fS = 1.0f;
                    fT = 0.0f;
                    fSqrDist = fA00 + 2.0f * fB0 + fC;
                } else {
                    fS = fNumer / fDenom;
                    fT = 1.0f - fS;
                    fSqrDist =
                        fS * (fA00 * fS + fA01 * fT + 2.0f * fB0) + fT * (fA01 * fS + fA11 * fT + 2.0f * fB1) + fC;
                }
            } else {
                fS = 0.0f;
                if (fTmp1 <= 0.0f) {
                    fT = 1.0f;
                    fSqrDist = fA11 + 2.0f * fB1 + fC;
                } else if (fB1 >= 0.0f) {
                    fT = 0.0f;
                    fSqrDist = fC;
                } else {
                    fT = -fB1 / fA11;
                    fSqrDist = fB1 * fT + fC;
                }
            }
        } else if (fT < 0.0f) {
            fTmp0 = fA01 + fB1;
            fTmp1 = fA00 + fB0;
            if (fTmp1 > fTmp0) {
                fNumer = fTmp1 - fTmp0;
                fDenom = fA00 - 2.0f * fA01 + fA11;
                if (fNumer >= fDenom) {
                    fT = 1.0f;
                    fS = 0.0f;
                    fSqrDist = fA11 + 2.0f * fB1 + fC;
                } else {
                    fT = fNumer / fDenom;
                    fS = 1.0f - fT;
                    fSqrDist =
                        fS * (fA00 * fS + fA01 * fT + 2.0f * fB0) + fT * (fA01 * fS + fA11 * fT + 2.0f * fB1) + fC;
                }
            } else {
                fT = 0.0f;
                if (fTmp1 <= 0.0f) {
                    fS = 1.0f;
                    fSqrDist = fA00 + 2.0f * fB0 + fC;
                } else if (fB0 >= 0.0f) {
                    fS = 0.0f;
                    fSqrDist = fC;
                } else {
                    fS = -fB0 / fA00;
                    fSqrDist = fB0 * fS + fC;
                }
            }
        } else {
            fNumer = fA11 + fB1 - fA01 - fB0;
            if (fNumer <= 0.0f) {
                fS = 0.0f;
                fT = 1.0f;
                fSqrDist = fA11 + 2.0f * fB1 + fC;
            } else {
                fDenom = fA00 - 2.0f * fA01 + fA11;
                if (fNumer >= fDenom) {
                    fS = 1.0f;
                    fT = 0.0f;
                    fSqrDist = fA00 + 2.0f * fB0 + fC;
                } else {
                    fS = fNumer / fDenom;
                    fT = 1.0f - fS;
                    fSqrDist =
                        fS * (fA00 * fS + fA01 * fT + 2.0f * fB0) + fT * (fA01 * fS + fA11 * fT + 2.0f * fB1) + fC;
                }
            }
        }
    }
    if (s != NULL) {
        *s = fS;
    }
    if (t != NULL) {
        *t = fT;
    }
    return __builtin_fabsf(fSqrDist);
}

/* The point v0 + s (v1 - v0) + t (v2 - v0) of a triangle. */
void ColTri_GetPoint(CpVec *out, CpTri *tri, f32 s, f32 t) {
    CpVec e0;
    CpVec e1;

    Vec4_Sub(&e0, &tri->v[1], &tri->v[0]);
    Vec4_Scale(&e0, &e0, s);
    Vec4_Add(&e0, &e0, &tri->v[0]);
    Vec4_Sub(&e1, &tri->v[2], &tri->v[0]);
    Vec4_Scale(&e1, &e1, t);
    Vec4_Add(out, &e0, &e1);
}

/* Sphere touches a triangle (squared distance <= sph->radiusSq, the stored field): writes the triangle's point
   nearest to the centre (w = 1). */
s32 ColSphere_TestTri(CpVec *pos, CpSphere *sph, CpTri *tri) {
    f32 st[2];

    if (ColTri_DistSqPoint(&sph->pos, tri, &st[0], &st[1]) > sph->radiusSq) {
        return 0;
    }
    ColTri_GetPoint(pos, tri, st[0], st[1]);
    return 1;
}

/* Axis-aligned bounds of a capsule. */
void ColBounds_OfCapsule(CpBox *box, CpCapsule *cap) {
    CpVecA p;
    s32 i;

    StgAabb_SetEmpty(box);
    for (i = 0; i < 3; i++) {
        Vec4_Copy((CpVec *)&p, &cap->a);
        p.v[i] += cap->radius;
        ColBox_AddPoint(box, (CpVec *)&p);
        Vec4_Copy((CpVec *)&p, &cap->a);
        p.v[i] -= cap->radius;
        ColBox_AddPoint(box, (CpVec *)&p);
        Vec4_Copy((CpVec *)&p, &cap->b);
        p.v[i] += cap->radius;
        ColBox_AddPoint(box, (CpVec *)&p);
        Vec4_Copy((CpVec *)&p, &cap->b);
        p.v[i] -= cap->radius;
        ColBox_AddPoint(box, (CpVec *)&p);
    }
}

/* Axis-aligned bounds of a sphere. */
void ColBounds_OfSphere(CpBox *box, CpSphere *sph) {
    CpVec p;

    StgAabb_SetEmpty(box);
    Vec4_Copy(&p, &sph->pos);
    p.x += sph->radius;
    p.y += sph->radius;
    p.z += sph->radius;
    ColBox_AddPoint(box, &p);
    Vec4_Copy(&p, &sph->pos);
    p.x -= sph->radius;
    p.y -= sph->radius;
    p.z -= sph->radius;
    ColBox_AddPoint(box, &p);
}

/* Ray of a segment: origin a, unit direction towards b. */
void ColRay_FromSeg(CpRay *ray, CpSeg *seg) {
    Vec4_Copy(&ray->origin, &seg->a);
    Vec4_Sub(&ray->dir, &seg->b, &seg->a);
    Vec3_Normalize(&ray->dir, &ray->dir);
}

/* Grows a box to contain a sphere. */
void ColBounds_AddSphere(CpBox *box, CpSphere *sph) {
    CpVec p;

    Vec4_Copy(&p, &sph->pos);
    p.x += sph->radius;
    p.y += sph->radius;
    p.z += sph->radius;
    ColBox_AddPoint(box, &p);
    Vec4_Copy(&p, &sph->pos);
    p.x -= sph->radius;
    p.y -= sph->radius;
    p.z -= sph->radius;
    ColBox_AddPoint(box, &p);
}

/* Axis-aligned bounds of an oriented box (its eight world corners). */
void ColBounds_OfObb(CpBox *box, CpObb *obb) {
    s32 i;

    StgAabb_SetEmpty(box);
    for (i = 0; i < 8; i++) {
        ColBox_AddPoint(box, &obb->world[i]);
    }
}

/* Axis-aligned bounds of a segment. */
void ColBounds_OfSeg(CpBox *box, CpSeg *seg) {
    StgAabb_SetEmpty(box);
    ColBox_AddPoint(box, &seg->a);
    ColBox_AddPoint(box, &seg->b);
}

/* The segment of a capsule lengthened by the radius at both ends. */
void ColCapsule_GetLongSeg(CpSeg *out, CpCapsule *cap) {
    CpVec dir;

    Vec4_Sub(&dir, &cap->b, &cap->a);
    Vec3_Normalize(&dir, &dir);
    Vec4_Copy(&out->a, &cap->a);
    Vec4_Copy(&out->b, &cap->b);
    out->b.x += dir.x * cap->radius;
    out->b.y += dir.y * cap->radius;
    out->b.z += dir.z * cap->radius;
    out->a.x -= dir.x * cap->radius;
    out->a.y -= dir.y * cap->radius;
    out->a.z -= dir.z * cap->radius;
}

/* The same, also returning the unit direction (w = 1). */
void ColCapsule_GetLongSegDir(CpSeg *out, CpVec *dir, CpCapsule *cap) {
    Vec4_Sub(dir, &cap->b, &cap->a);
    Vec3_Normalize(dir, dir);
    Vec4_Copy(&out->a, &cap->a);
    Vec4_Copy(&out->b, &cap->b);
    out->b.x += dir->x * cap->radius;
    out->b.y += dir->y * cap->radius;
    out->b.z += dir->z * cap->radius;
    out->a.x -= dir->x * cap->radius;
    out->a.y -= dir->y * cap->radius;
    out->a.z -= dir->z * cap->radius;
    dir->w = 1.0f;
}

/* Sphere around a capsule: centre at the middle, radius = half the length + the capsule's radius. */
void ColCapsule_GetBoundSphere(CpSphere *out, CpCapsule *cap) {
    f32 r;

    ColSeg_GetMidpoint3(&out->pos, (CpSeg *)cap);
    r = Vec3_Dist(&cap->a, &cap->b) * 0.5f + cap->radius;
    out->radius = r;
    out->radiusSq = r * r;
}

/* Nearest points of two segments (copies of both are handed to ColSeg_Nearest); 1 when the segments cross, and
   only then is w of the two points set to 1. No caller. */
s32 ColSeg_ClosestToSeg(CpSeg *a, CpSeg *b, CpVec *onA, CpVec *onB) {
    CpSeg sa;
    CpSeg sb;

    Vec4_Copy(&sa.a, &a->a);
    Vec4_Copy(&sa.b, &a->b);
    Vec4_Copy(&sb.a, &b->a);
    Vec4_Copy(&sb.b, &b->b);
    if (ColSeg_Nearest(&sa, &sb, onA, onB)) {
        onA->w = 1.0f;
        onB->w = 1.0f;
        return 1;
    }
    return 0;
}

/* Squared distance from a point to the infinite line a..b; `out` gets the foot of the perpendicular. */
f32 ColLine_DistSqPoint(CpVec *pt, CpVec *a, CpVec *b, CpVec *out) {
    CpVec d;
    CpVec l;
    CpVec proj;
    f32 len;

    Vec4_Sub(&d, pt, a);
    Vec4_Sub(&l, b, a);
    len = Vec3_Dot(&l, &l);
    if (len == 0.0f) {
        len = 0.0001f;
    }
    Vec3_Scale(&proj, &l, Vec3_Dot(&d, &l) / len);
    Vec4_Add(out, a, &proj);
    Vec4_Sub(&d, &d, &proj);
    return Vec3_Dot(&d, &d);
}

/* Distance from a point to the infinite line a..b. */
f32 ColLine_DistPoint(CpVec *pt, CpVec *a, CpVec *b, CpVec *out) {
    return sqrtf(ColLine_DistSqPoint(pt, a, b, out));
}

/* Nearest point to b on the line (or segment) a + param * l. A line shorter than sqrt(epsSq) gives a and leaves
   param unwritten. */
void ColSeg_NearestOnLine(CpVec *a, CpVec *l, CpVec *b, s32 infinite, f32 epsSq, CpVec *out, f32 *param) {
    CpVec ab;
    f32 d = Vec3_Dot(l, l);

    if (d < epsSq) {
        Vec4_Copy(out, a);
        return;
    }
    Vec4_Sub(&ab, b, a);
    *param = Vec3_Dot(l, &ab) / d;
    if (!infinite) {
        *param = CP_MAX(0.0f, CP_MIN(1.0f, *param));
    }
    Vec4_Scale(out, l, *param);
    Vec4_Add(out, out, a);
}

/* Nearest points of two parallel segments. */
void ColSeg_NearestParallel(CpSeg *a, CpVec *la, CpSeg *b, CpVec *lb, s32 infinite, f32 epsSq, CpVec *onA,
                            CpVec *onB) {
    f32 s[2];
    CpVec tp;
    f32 temp;

    ColSeg_NearestOnLine(&a->a, la, &b->a, 1, epsSq, onA, &s[0]);
    if (infinite == 1) {
        Vec4_Copy(onB, &b->a);
    } else {
        ColSeg_NearestOnLine(&a->a, la, &b->b, 1, epsSq, &tp, &s[1]);
        if (s[0] < 0.0f && s[1] < 0.0f) {
            Vec4_Copy(onA, &a->a);
            if (s[0] < s[1]) {
                Vec4_Copy(onB, &b->b);
            } else {
                Vec4_Copy(onB, &b->a);
            }
        } else if (s[0] > 1.0f && s[1] > 1.0f) {
            Vec4_Copy(onA, &a->b);
            if (s[0] < s[1]) {
                Vec4_Copy(onB, &b->a);
            } else {
                Vec4_Copy(onB, &b->b);
            }
        } else {
            temp = 0.5f * (CP_MAX(0.0f, CP_MIN(1.0f, s[0])) + CP_MAX(0.0f, CP_MIN(1.0f, s[1])));
            onA->x = a->a.x + temp * la->x;
            onA->y = a->a.y + temp * la->y;
            onA->z = a->a.z + temp * la->z;
            ColSeg_NearestOnLine(&b->a, lb, onA, 1, epsSq, onB, &temp);
        }
    }
}

/* Nearest points of two segments when a parameter of the infinite-line solution fell outside [0, 1]. */
void ColSeg_AdjustNearest(CpVec *a, CpVec *la, CpVec *b, CpVec *lb, f32 epsSq, f32 s, f32 t, CpVec *onA,
                          CpVec *onB) {
    if (CP_OUT_OF_RANGE(s) && CP_OUT_OF_RANGE(t)) {
        s = CP_MAX(0.0f, CP_MIN(1.0f, s));
        onA->x = a->x + s * la->x;
        onA->y = a->y + s * la->y;
        onA->z = a->z + s * la->z;
        ColSeg_NearestOnLine(b, lb, onA, 1, epsSq, onB, &t);
        if (CP_OUT_OF_RANGE(t)) {
            t = CP_MAX(0.0f, CP_MIN(1.0f, t));
            onB->x = b->x + t * lb->x;
            onB->y = b->y + t * lb->y;
            onB->z = b->z + t * lb->z;
            ColSeg_NearestOnLine(a, la, onB, 0, epsSq, onA, &s);
            ColSeg_NearestOnLine(b, lb, onA, 0, epsSq, onB, &t);
        }
    } else if (CP_OUT_OF_RANGE(s)) {
        s = CP_MAX(0.0f, CP_MIN(1.0f, s));
        onA->x = a->x + s * la->x;
        onA->y = a->y + s * la->y;
        onA->z = a->z + s * la->z;
        ColSeg_NearestOnLine(b, lb, onA, 0, epsSq, onB, &t);
    } else if (CP_OUT_OF_RANGE(t)) {
        t = CP_MAX(0.0f, CP_MIN(1.0f, t));
        onB->x = b->x + t * lb->x;
        onB->y = b->y + t * lb->y;
        onB->z = b->z + t * lb->z;
        ColSeg_NearestOnLine(a, la, onB, 0, epsSq, onA, &s);
    }
}

/* Nearest points of two segments (x, y, z); returns 1 when they are closer than 1e-6 (sum of the component
   distances). Tolerances: a segment whose squared length is below 1e-6 * 1e-6 is a point; the segments are
   parallel when |L11 L22 - L12^2| < 1e-6; the helpers get 1e-6 (not its square) as their squared tolerance, as in
   the published routine. The midpoint and the difference of the two points are computed and dropped. */
s32 ColSeg_Nearest(CpSeg *a, CpSeg *b, CpVec *onA, CpVec *onB) {
    CpVec la;
    CpVec lb;
    CpVec ab;
    CpVec mid;
    CpVec vec;
    f32 temp = 0.0f;
    f32 eps = 1e-6f;
    f32 epsSq = eps * eps;
    f32 l11;
    f32 l22;

    Vec4_Sub(&la, &a->b, &a->a);
    Vec4_Sub(&lb, &b->b, &b->a);
    l11 = Vec3_Dot(&la, &la);
    l22 = Vec3_Dot(&lb, &lb);
    if (l11 < epsSq) {
        Vec4_Copy(onA, &a->a);
        ColSeg_NearestOnLine(&b->a, &lb, &a->a, 0, eps, onB, &temp);
    } else if (l22 < epsSq) {
        Vec4_Copy(onB, &b->a);
        ColSeg_NearestOnLine(&a->a, &la, &b->a, 0, eps, onA, &temp);
    } else {
        f32 l12;
        f32 detL;

        Vec4_Sub(&ab, &b->a, &a->a);
        l12 = -(la.x * lb.x) - (la.y * lb.y) - (la.z * lb.z);
        detL = l11 * l22 - l12 * l12;
        if (__builtin_fabsf(detL) < eps) {
            ColSeg_NearestParallel(a, &la, b, &lb, 0, eps, onA, onB);
        } else {
            f32 ra = la.x * ab.x + la.y * ab.y + la.z * ab.z;
            f32 rb = -lb.x * ab.x - lb.y * ab.y - lb.z * ab.z;
            f32 t = (l11 * rb - ra * l12) / detL;
            f32 s = (ra - l12 * t) / l11;

            onA->x = a->a.x + s * la.x;
            onA->y = a->a.y + s * la.y;
            onA->z = a->a.z + s * la.z;
            onB->x = b->a.x + t * lb.x;
            onB->y = b->a.y + t * lb.y;
            onB->z = b->a.z + t * lb.z;
            if (CP_OUT_OF_RANGE(s) || CP_OUT_OF_RANGE(t)) {
                ColSeg_AdjustNearest(&a->a, &la, &b->a, &lb, eps, s, t, onA, onB);
            }
        }
    }
    mid.x = 0.5f * (onA->x + onB->x);
    mid.y = 0.5f * (onA->y + onB->y);
    mid.z = 0.5f * (onA->z + onB->z);
    vec.x = onB->x - onA->x;
    vec.y = onB->y - onA->y;
    vec.z = onB->z - onA->z;
    return (__builtin_fabsf(vec.x) + __builtin_fabsf(vec.y) + __builtin_fabsf(vec.z)) < eps ? 1 : 0;
}

/* Builds a capsule: the "box" shape of a hit record (a segment from `a` to `b` with a radius). */
void ColCapsule_Set(CpCapsule *cap, CpVec *a, CpVec *b, f32 radius) {
    Vec4_Copy(&cap->a, a);
    Vec4_Copy(&cap->b, b);
    cap->radius = radius;
}

/* Midpoint of a segment (w = 1). */
void ColSeg_GetMidpoint3(CpVec *out, CpSeg *seg) {
    Vec4_Add(out, &seg->a, &seg->b);
    Vec3_Scale(out, out, 0.5f);
    out->w = 1.0f;
}

/* Builds the swept sphere of a capsule: start sphere, movement, unit direction and length. */
void ColSweep_FromCapsule(CpSweep *sw, CpCapsule *cap) {
    f32 len;

    sw->cap = *cap;
    Vec4_Copy(&sw->sphere.pos, &sw->cap.a);
    sw->sphere.radius = sw->cap.radius;
    sw->sphere.radiusSq = sw->cap.radius * sw->cap.radius;
    Vec4_Sub(&sw->delta, &sw->cap.b, &sw->cap.a);
    len = sqrtf(Vec3_Dot(&sw->delta, &sw->delta));
    sw->length = len;
    if (len > 0.0f) {
        Vec4_Scale(&sw->dir, &sw->delta, 1.0f / len);
    } else {
        sw->dir.x = 0.0f;
        sw->dir.y = 0.0f;
        sw->dir.z = 0.0f;
        sw->dir.w = 1.0f;
    }
}

/* Plane through three points: unit normal (c - b) x (a - b), w = -(normal . a). */
void ColPlane_FromPoints(CpVec *plane, CpVec *a, CpVec *b, CpVec *c) {
    CpVec e0;
    CpVec e1;
    CpVec n;

    Vec4_Sub(&e0, a, b);
    Vec4_Sub(&e1, c, b);
    Vec3_Cross(&n, &e1, &e0);
    Vec3_Normalize(&n, &n);
    Vec4_Copy(plane, &n);
    plane->w = -Vec3_Dot(a, plane);
}

/* Projects a point onto a plane (normal . p + w = 0). */
void ColPlane_ProjectPoint(CpVec *out, CpVec *plane, CpVec *pt) {
    f32 d = Vec3_Dot(plane, pt) + plane->w;

    out->x = pt->x - plane->x * d;
    out->y = pt->y - plane->y * d;
    out->z = pt->z - plane->z * d;
    out->w = 1.0f;
}

/* Signed distance of a point from a plane stored as normal . p = w. */
f32 ColPlane_DistPoint(CpVec *plane, CpVec *pt) {
    return Vec3_Dot(plane, pt) - plane->w;
}

/* Builds a segment from two points (w = 1). Same code as ColSeg_Set; no caller. */
void ColSeg_Set2(CpSeg *seg, CpVec *a, CpVec *b) {
    seg->a.x = a->x;
    seg->a.y = a->y;
    seg->a.z = a->z;
    seg->a.w = 1.0f;
    seg->b.x = b->x;
    seg->b.y = b->y;
    seg->b.z = b->z;
    seg->b.w = 1.0f;
}

/* Builds a segment from two points (w = 1): what StgCol_TraceSegment takes. */
void ColSeg_Set(CpSeg *seg, CpVec *a, CpVec *b) {
    seg->a.x = a->x;
    seg->a.y = a->y;
    seg->a.z = a->z;
    seg->a.w = 1.0f;
    seg->b.x = b->x;
    seg->b.y = b->y;
    seg->b.z = b->z;
    seg->b.w = 1.0f;
}

/* Midpoint of a segment (w = 1). */
void ColSeg_GetMidpoint(CpSeg *seg, CpVec *out) {
    Vec4_Copy(out, &seg->a);
    Vec4_Add(out, out, &seg->b);
    Vec4_Scale(out, out, 0.5f);
    out->w = 1.0f;
}

/* Builds a sphere: the "sphere" shape of a hit record. */
void ColSphere_Set(CpSphere *sph, CpVec *pos, f32 radius) {
    sph->pos.x = pos->x;
    sph->pos.y = pos->y;
    sph->pos.z = pos->z;
    sph->pos.w = 1.0f;
    sph->radius = radius;
    sph->radiusSq = radius * radius;
}

/* Builds a triangle from three corners and a plane. */
void ColTri_Set(CpTri *tri, CpVec *a, CpVec *b, CpVec *c, CpVec *nrm) {
    Vec4_Copy(&tri->v[0], a);
    Vec4_Copy(&tri->v[1], b);
    Vec4_Copy(&tri->v[2], c);
    Vec4_Copy(&tri->nrm, nrm);
}

/* A point of the triangle's plane lies inside the triangle (edges included). */
s32 ColTri_ContainsPoint(CpTri *tri, CpVec *pt) {
    CpVec e0;
    CpVec e1;
    CpVec d;
    f32 a;
    f32 b;
    f32 c;
    f32 d0;
    f32 d1;
    f32 det;
    f32 inv;
    f32 u;
    f32 v;
    s32 ret;

    Vec4_Sub(&e0, &tri->v[1], &tri->v[0]);
    Vec4_Sub(&e1, &tri->v[2], &tri->v[0]);
    Vec4_Sub(&d, pt, &tri->v[0]);
    a = Vec3_Dot(&e0, &e0);
    b = Vec3_Dot(&e0, &e1);
    c = Vec3_Dot(&e1, &e1);
    d0 = Vec3_Dot(&d, &e0);
    d1 = Vec3_Dot(&d, &e1);
    det = b * b - a * c;
    if (det == 0.0f) {
        det = 0.001f;
    }
    inv = 1.0f / det;
    u = (b * d1 - c * d0) * inv;
    if (u < 0.0f || u > 1.0f) {
        return 0;
    }
    v = (b * d0 - a * d1) * inv;
    if (v < 0.0f) {
        return 0;
    }
    ret = 1;
    if (u + v > 1.0f) {
        ret = 0;
    }
    return ret;
}
