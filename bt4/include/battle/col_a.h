#ifndef BATTLE_COL_A_H
#define BATTLE_COL_A_H

#include "types.h"

/*
 * Collision primitives (src/battle/col_a.c, 0x230B38..0x236190): axis-aligned boxes, the box tree of a collision
 * mesh, oriented boxes. All types here are this module's own views; the same boxes are StgAabb (battle/stg_d.h),
 * StgColBox / StgColNode / StgColPoly (battle/eft_det_b.h), StgBox (battle/stg_a.h) and BtlObjBox (btl_obj.c).
 *
 * CONVENTIONS (verified by the matching C)
 *   - Every comparison is a plain `<` on floats and every box test is CLOSED: touching counts as overlapping,
 *     a point on a face is inside. A NaN therefore never separates anything (box tests answer "overlap").
 *   - Tolerances: 1e-6 (ColBox_HasVolume, Col_NearEq callers), 0.001 (ColBox_ClipRay, how far the entry point
 *     may miss a face), 100 * FLT_MIN (ColBox_TestSegment, effectively zero), -1e-6 (Col_LineLineParams, how
 *     far from parallel two lines must be). ColBox_Overlaps, ColBox_ContainsPoint and ColObb_Overlaps have none.
 *   - Functions that write a point set w = 1 (ColBox_GetCenter / GetHalf / GetSize, ColBox_ClipRay only when the
 *     origin is inside); ColBox_AddPoint writes w = 1 into the point it was GIVEN.
 *
 * COLLISION MESH (verified)
 *   - gColMeshBase is one pointer for all meshes: the stage's collision block, set once by BtlStage_Init through
 *     ColMesh_SetBase. A ColMesh header holds three offsets counted in 4-byte words from that base.
 *   - Nodes are 0x20 bytes: a box and two links. `right == -1` marks a leaf, whose `left` is a polygon index;
 *     otherwise `left` and `right` are node indices in the same array. Node 0 is the root.
 *   - Polygons are 0x20 bytes: flags, three vertex indices, the plane. Vertices are 16-byte vectors.
 *   - A walk is depth first from node 0: a node whose box does not overlap the query box is dropped with its
 *     whole subtree, otherwise `left` is walked completely before `right`. Leaves are therefore reported in a
 *     fixed order that depends only on the tree and the box. There is no early out: every overlapping leaf is
 *     reported, and ColMesh_WalkBox returns the OR of everything the callback returned.
 *   - The walk recurses on the native stack (one frame per tree level, no depth limit) and keeps its state in
 *     globals (gColWalk*), so a callback must not start another walk.
 *
 * VU0 (read from the callee's disassembly; nothing in this file's range contains VU0 code itself)
 *   The vector helpers called here are hand-written macro-mode routines, and a port has to keep their order of
 *   operations:
 *     Vec3_Dot(a, b)        = (a.x*b.x + a.y*b.y) + a.z*b.z   (the last step is a multiply-add with vf3.x, which
 *                             the maths init leaves at 1.0)
 *     Vec3_LengthSq(v)      = (v.x*v.x + v.y*v.y) + v.z*v.z
 *     Vec4_Sub(d, a, b)     = a - b on all four components
 *     Mtx_MulVec4(d, m, v)  = ((m[0]*v.x + m[1]*v.y) + m[2]*v.z) + m[3]*v.w, per component
 *     Vec3_Copy(d, s)   = copy x, y, z; d.w is left as it was
 *   plus the VU0 / FPU number format of the machine (no denormals, no infinities, results truncated).
 */

/* Vector as the collision code holds it (16-byte aligned, handed to the VU0 vector routines). */
typedef struct ColVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) ColVec; /* size 0x10 */

/* The same vector read by component index. */
typedef struct ColVecA {
    /* 0x0 */ f32 v[4];
} __attribute__((aligned(16))) ColVecA; /* size 0x10 */

/* Axis-aligned box. Empty is min = +1e10, max = -1e10 (StgAabb_SetEmpty, 0x230B10). */
typedef struct ColBox {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} ColBox; /* size 0x18 */

/* Ray: origin and direction. The direction is not normalised and the ray has no end. */
typedef struct ColRay {
    /* 0x00 */ ColVecA origin;
    /* 0x10 */ ColVecA dir;
} ColRay; /* size 0x20 */

/* Segment: two end points. */
typedef struct ColSeg {
    /* 0x00 */ ColVec a;
    /* 0x10 */ ColVec b;
} ColSeg; /* size 0x20 */

/* Node of a collision mesh's box tree. */
typedef struct ColNode {
    /* 0x00 */ ColBox box;
    /* 0x18 */ s32 left;       /* inner node: index of the first child; leaf: polygon index (-1: none) */
    /* 0x1C */ s32 right;      /* inner node: index of the second child; leaf: -1 */
} ColNode; /* size 0x20 */

/* Polygon (triangle) of a collision mesh. */
typedef struct ColPoly {
    /* 0x00 */ s32 flags;      /* see STGCOL_POLY_* in battle/eft_det_b.h; not read here */
    /* 0x04 */ s32 vtx[3];     /* indices into the mesh's vertex array */
    /* 0x10 */ ColVec nrm;     /* plane; not read here */
} ColPoly; /* size 0x20 */

/* Header of a collision mesh. The three offsets count 4-byte words from gColMeshBase. */
typedef struct ColMesh {
    /* 0x00 */ s32 nodeCount;  /* only tested: <= 0 means the mesh is empty and is never walked */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 nodeOfs;    /* ColNode[], node 0 is the root */
    /* 0x10 */ s32 polyOfs;    /* ColPoly[] */
    /* 0x14 */ s32 vtxOfs;     /* ColVec[] */
} ColMesh;

/* 3x3 matrix on the stack of the oriented box tests. */
typedef struct ColMat3 {
    /* 0x00 */ f32 m[3][3];
} __attribute__((aligned(16))) ColMat3; /* size 0x30 */

/* Leaf callback of ColMesh_WalkBox. The results of all calls are OR-ed. */
typedef s32 (*ColMeshCb)(ColNode *node, void *ctx);

/*
 * Oriented box, 0x190 bytes. BtlObjHit_BuildVolumes (0x24DDF0) keeps an array of them in the battle object's
 * work buffer and BtlBodyHit_TestVolumes (0x1AF508) tests them with ColObb_Overlaps.
 */
typedef struct ColObb {
    /* 0x000 */ ColVec center; /* ColObb_Init: the centre it was given; ColObb_Update: row 3 of mtx. w = 1 */
    /* 0x010 */ ColVec half;   /* half extents along the three axes, w = 1 */
    /* 0x020 */ ColVec axis[3];/* world axes: rows 0..2 of mtx (x, y, z only; w is never written) */
    /* 0x050 */ f32 mtx[4][4]; /* local to world */
    /* 0x090 */ ColVec local[8]; /* corners in local space, w = 1. Corner n: x negative when bit 0 of n is set,
                                    y when bit 1, z when bit 2 */
    /* 0x110 */ ColVec world[8]; /* corners in world space (mtx applied to local) */
} ColObb; /* size 0x190 */

extern s32 *gColMeshBase;      /* 0x2FEB90: base the mesh offsets are counted from, in words */
extern s32 gColWalkCount;      /* 0x2FEB94: leaves collected / OR of the callback results of the walk in progress */
extern s32 gColWalkMax;        /* 0x2FEB98: capacity of gColWalkBuf */
extern u16 *gColWalkBuf;       /* 0x2FEB9C: where ColMesh_CollectBox writes polygon indices; NULL outside it */
extern ColBox *gColWalkBox;    /* 0x2FEBA0: the box of the last ColMesh_WalkBox (written, never read) */
extern ColMeshCb gColWalkCb;   /* 0x2FEBA4 */
extern void *gColWalkCtx;      /* 0x2FF1D0 */

void ColBox_SetCenterHalf(ColBox *box, ColVec *center, ColVec *half);
void ColBox_AddPoint(ColBox *box, ColVec *pt);
void ColBox_AddPointKeepW(ColBox *box, ColVec *pt);
void ColBox_GetCenter(ColBox *box, ColVec *out);
void ColBox_GetHalf(ColBox *box, ColVec *out);
void ColBox_GetSize(ColBox *box, ColVec *out);
s32 ColBox_HasVolume(ColBox *box);
s32 ColBox_ContainsPoint(ColBox *box, ColVec *pt);
s32 ColNode_IsLeaf(ColNode *node);
s32 ColNode_IsEmptyLeaf(ColNode *node);
void ColMesh_GetPolyVerts(ColMesh *mesh, ColPoly *poly, ColVec *v0, ColVec *v1, ColVec *v2);
ColPoly *ColMesh_GetPoly(ColMesh *mesh, s32 idx);
void ColMesh_SetBase(s32 *base);
void ColObb_Init(ColObb *obb, ColVec *center, f32 hx, f32 hy, f32 hz);
void ColObb_Update(ColObb *obb, void *mtx);
s32 ColBox_Overlaps(ColBox *a, ColBox *b);
s32 ColBox_ClipRay(ColBox *box, ColRay *ray, ColVecA *out);
s32 ColBox_TestSegment(ColBox *box, ColSeg *seg);
s32 ColNode_TestBox(ColNode *node, ColBox *box);
void ColMesh_CollectNode(ColNode *nodes, ColBox *box, s32 idx);
s32 ColMesh_CollectBox(s32 max, u16 *buf, ColMesh *mesh, ColBox *box);
void ColMesh_WalkNode(ColNode *nodes, ColBox *box, s32 idx);
s32 ColMesh_WalkBox(ColMesh *mesh, ColBox *box, void *ctx, ColMeshCb cb);
s32 Col_NearEq(f32 a, f32 b, f32 eps);
void Col_LineLineParams(f32 *s, f32 *t, ColVec *p1, ColVec *d1, ColVec *p2, ColVec *d2);
s32 ColObb_Overlaps(ColObb *a, ColObb *b);
s32 ColObb_Contact(ColObb *a, ColObb *b, ColVec *normal, ColVec *point, f32 *depth);

#endif
