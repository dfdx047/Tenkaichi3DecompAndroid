#ifndef BATTLE_COL_B_H
#define BATTLE_COL_B_H

#include "types.h"

/*
 * Collision primitive library, second part: 0x236190..0x239EA0.
 *   src/battle/col_b.c    0x236190..0x239BB0  tests between spheres, segments, capsules, triangles, oriented
 *                                             boxes; bounds builders; shape builders
 *   head of src/battle/col_c.c  0x239BB0..0x239EA0  NOT collision: the first five functions of the text printer (Font_*,
 *                                             continued in col_c): GS packet helpers
 *
 * All types here are local views (prefix Cp) of shapes other modules know under their own names:
 *   CpVec     = ColVec (battle/col_a.h), StgColVec, Vec4
 *   CpBox     = ColBox, StgColBox
 *   CpSphere  = StgColSphere, EftHitSphere, RigidSphere
 *   CpSeg     = ColSeg, StgColSeg, BtlMoveSeg, EftISegment
 *   CpCapsule = StgColSweep, EftDetCapsule, EftHitBox ("box" shape of a hit record)
 *   CpTri     = StgColTri, EftDetTri, StgRigidTri
 *   CpSweep   = StgColSweepCtx (first 0x74 bytes), EftDetStageCtx
 *   CpObb     = ColObb (battle/col_a.h)
 *
 * Conventions: a position has w = 1; the difference of two positions (Vec4_Sub) has w = 0. Tests are inclusive
 * unless a function's comment says otherwise. Nothing here is static: every function writes only through its
 * arguments.
 */

/* Vector: always four floats, 16-byte aligned, handed to the VU0 vector routines. A position has w = 1. */
typedef struct CpVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) CpVec; /* size 0x10 */

/* Vector read by index. */
typedef struct CpVecA {
    /* 0x0 */ f32 v[4];
} __attribute__((aligned(16))) CpVecA; /* size 0x10 */

/* Axis-aligned box. */
typedef struct CpBox {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} CpBox; /* size 0x18 */

/* Sphere with its squared radius (built by ColSphere_Set). */
typedef struct CpSphere {
    /* 0x00 */ CpVec pos;      /* w = 1 */
    /* 0x10 */ f32 radius;
    /* 0x14 */ f32 radiusSq;
    /* 0x18 */ f32 unk18[2];   /* never written here */
} CpSphere; /* size 0x20 */

/* Segment: two end points, w = 1 (built by ColSeg_Set). */
typedef struct CpSeg {
    /* 0x00 */ CpVec a;
    /* 0x10 */ CpVec b;
} CpSeg; /* size 0x20 */

/* Ray: origin and unit direction (built by ColRay_FromSeg). */
typedef struct CpRay {
    /* 0x00 */ CpVec origin;
    /* 0x10 */ CpVec dir;
} CpRay; /* size 0x20 */

/* Capsule: a segment and a radius (built by ColCapsule_Set). The "two boxes" shape of a hit record is two of these. */
typedef struct CpCapsule {
    /* 0x00 */ CpVec a;
    /* 0x10 */ CpVec b;
    /* 0x20 */ f32 radius;
    /* 0x24 */ f32 unk24[3];
} CpCapsule; /* size 0x30 */

/* Triangle with its plane (built by ColTri_Set; the stage code fills it from a mesh polygon). The plane is
   stored as nrm.xyz . p = nrm.w (ColSweep_TestTri negates w before use; ColPlane_DistPoint subtracts it).
   ColPlane_FromPoints / ColPlane_ProjectPoint use the other convention, normal . p + w = 0. */
typedef struct CpTri {
    /* 0x00 */ CpVec v[3];
    /* 0x30 */ CpVec nrm;
} CpTri; /* size 0x40 */

/* A sphere moved along a segment (built from a capsule by ColSweep_FromCapsule). */
typedef struct CpSweep {
    /* 0x00 */ CpCapsule cap;  /* a: start, b: end, radius */
    /* 0x30 */ CpSphere sphere;/* the sphere at the start: pos = cap.a, radius, radius squared */
    /* 0x50 */ CpVec delta;    /* cap.b - cap.a (w = 0) */
    /* 0x60 */ CpVec dir;      /* delta / length; (0, 0, 0, 1) when the length is 0 */
    /* 0x70 */ f32 length;
} CpSweep; /* size 0x80 */

/* Oriented box (ColObb of battle/col_a.h). */
typedef struct CpObb {
    /* 0x000 */ CpVec center;
    /* 0x010 */ CpVec half;    /* half extents along the three axes */
    /* 0x020 */ CpVec axis[3]; /* unit axes in world space */
    /* 0x050 */ f32 mtx[4][4];
    /* 0x090 */ CpVec local[8];
    /* 0x110 */ CpVec world[8];/* the eight corners in world space */
} CpObb; /* size 0x190 */

/* ---- head of col_c.c: GS packet helpers of the text printer ---- */

/* Where a packet is being written: a pointer into the display list that every call advances. */
typedef struct CbFontOut {
    /* 0x0 */ u64 *p;
} CbFontOut;

/* The text printer's object as far as the clip test reads it: its clip rectangle in screen pixels. */
typedef struct CbFontClip {
    /* 0x00 */ u8 unk0[0x30];
    /* 0x30 */ u16 x0;
    /* 0x32 */ u16 y0;
    /* 0x34 */ u16 x1;
    /* 0x36 */ u16 y1;
} CbFontClip;

/* col_b.c */
s32 ColLine2_Intersect(f32 *p, f32 *q, f32 *r, f32 *s, f32 *t);
s32 ColSweep_TestTri(CpSweep *sw, CpTri *tri, CpVec *pos, f32 *dist);
s32 ColSeg_TestTri(CpSeg *seg, CpTri *tri, CpVec *pos, f32 *dist);
s32 ColSphere_ContactObb(CpSphere *sph, CpObb *obb, CpVec *nrm, CpVec *pos, f32 *depth);
s32 ColSphere_TestObb(CpSphere *sph, CpObb *obb);
s32 ColSphere_TestRay(CpSphere *sph, CpRay *ray, f32 *t, CpVec *pos);
s32 ColSphere_TestLine(CpSphere *sph, CpVec *a, CpVec *b, s32 *count, f32 *t0, f32 *t1);
s32 ColSphere_TestSphere(CpSphere *a, CpSphere *b);
s32 ColSphere_ContactSphere(CpSphere *a, CpSphere *b, CpVec *nrm, CpVec *pos, f32 *depth);
s32 ColSphere_SweepSphere(CpSphere *a, CpSphere *b, CpVec *moveA, CpVec *moveB, f32 *t);
s32 ColSphere_SeparateSphere(CpSphere *a, CpSphere *b, CpVec *posA, CpVec *posB, CpVec *nrm);
f32 ColTri_DistSqPoint(CpVec *pt, CpTri *tri, f32 *s, f32 *t);
void ColTri_GetPoint(CpVec *out, CpTri *tri, f32 s, f32 t);
s32 ColSphere_TestTri(CpVec *pos, CpSphere *sph, CpTri *tri);
void ColBounds_OfCapsule(CpBox *box, CpCapsule *cap);
void ColBounds_OfSphere(CpBox *box, CpSphere *sph);
void ColRay_FromSeg(CpRay *ray, CpSeg *seg);
void ColBounds_AddSphere(CpBox *box, CpSphere *sph);
void ColBounds_OfObb(CpBox *box, CpObb *obb);
void ColBounds_OfSeg(CpBox *box, CpSeg *seg);
void ColCapsule_GetLongSeg(CpSeg *out, CpCapsule *cap);
void ColCapsule_GetLongSegDir(CpSeg *out, CpVec *dir, CpCapsule *cap);
void ColCapsule_GetBoundSphere(CpSphere *out, CpCapsule *cap);
s32 ColSeg_ClosestToSeg(CpSeg *a, CpSeg *b, CpVec *onA, CpVec *onB);
f32 ColLine_DistSqPoint(CpVec *pt, CpVec *a, CpVec *b, CpVec *out);
f32 ColLine_DistPoint(CpVec *pt, CpVec *a, CpVec *b, CpVec *out);
void ColSeg_NearestOnLine(CpVec *a, CpVec *l, CpVec *b, s32 infinite, f32 epsSq, CpVec *out, f32 *param);
void ColSeg_NearestParallel(CpSeg *a, CpVec *la, CpSeg *b, CpVec *lb, s32 infinite, f32 epsSq, CpVec *onA,
                            CpVec *onB);
void ColSeg_AdjustNearest(CpVec *a, CpVec *la, CpVec *b, CpVec *lb, f32 epsSq, f32 s, f32 t, CpVec *onA,
                          CpVec *onB);
s32 ColSeg_Nearest(CpSeg *a, CpSeg *b, CpVec *onA, CpVec *onB);
void ColCapsule_Set(CpCapsule *cap, CpVec *a, CpVec *b, f32 radius);
void ColSeg_GetMidpoint3(CpVec *out, CpSeg *seg);
void ColSweep_FromCapsule(CpSweep *sw, CpCapsule *cap);
void ColPlane_FromPoints(CpVec *plane, CpVec *a, CpVec *b, CpVec *c);
void ColPlane_ProjectPoint(CpVec *out, CpVec *plane, CpVec *pt);
f32 ColPlane_DistPoint(CpVec *plane, CpVec *pt);
void ColSeg_Set2(CpSeg *seg, CpVec *a, CpVec *b);
void ColSeg_Set(CpSeg *seg, CpVec *a, CpVec *b);
void ColSeg_GetMidpoint(CpSeg *seg, CpVec *out);
void ColSphere_Set(CpSphere *sph, CpVec *pos, f32 radius);
void ColTri_Set(CpTri *tri, CpVec *a, CpVec *b, CpVec *c, CpVec *nrm);
s32 ColTri_ContainsPoint(CpTri *tri, CpVec *pt);

/* head of col_c.c */
s32 Font_IsInGsRange(s32 x0, s32 y0, s32 x1, s32 y1);
s32 Font_IsInClip(CbFontClip *clip, s32 x0, s32 y0, s32 x1, s32 y1);
void Font_BeginPacket(CbFontOut *out);
void Font_EndPacket(CbFontOut *out);
void Font_PutSprite(CbFontOut *out, s32 x0, s32 y0, s32 x1, s32 y1, s32 u0, s32 v0, s32 u1, s32 v1, u32 rgba,
                    u64 tex0);

#endif
