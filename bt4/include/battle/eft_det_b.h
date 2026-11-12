#ifndef BATTLE_EFT_DET_B_H
#define BATTLE_EFT_DET_B_H

#include "types.h"

/*
 * Stage collision queries, stage navigation graph and the head of the AI sequence code:
 *   src/battle/eft_det_b.c    0x1B16F0..0x1B3510  StgCol_* (queries against the stage collision meshes) and
 *                                                 StgShadow_* (triangles under a box, for the blob shadow)
 *   src/battle/eft_det_b_b.c  0x1B3510..0x1B3F78  StgNav_* (way-point graph of the stage, path search for the AI)
 *   src/battle/eft_det_b_c.c  0x1B3F78..0x1B4140  BtlAiSeq_Reset / BtlAiSeq_PushRule (first two functions of the
 *                                                 AI sequence object, btl_ai_seq.c)
 *
 * All structures here are local views; the stage side is described in battle/stg_a.h (StgZone, StgObj, BtlStage)
 * and the fighter side in battle/btl_char_coll.h (BtlCollObjWork).
 *
 * STAGE COLLISION DATA (verified by the matching C unless marked):
 *   - The stage is cut into zones (BtlStage.zones, 0x40 bytes each, rectangles on the ground plane with a list
 *     of neighbours). Every zone has one collision mesh for its static geometry (StgColZone.mesh) and a list of
 *     collision records (StgColZone.recs, 0x10 bytes each) for the destructible objects standing in it.
 *   - A record names an object index (the index of BtlStage.objs / StgObj) and a mesh. Records come in two
 *     kinds: `broken` == 0 is the collision of the WHOLE object, used while the object is not broken;
 *     `broken` != 0 is the collision of what is left of it, used once the object is broken.
 *   - A mesh is opaque here: ColMesh_WalkBox walks it against a box and calls a callback for each candidate
 *     node; ColMesh_GetPoly(mesh, node->poly) gives the polygon header (flags, plane) and ColMesh_GetPolyVerts its three
 *     corner positions.
 *   - Object indices are returned as a 64-bit mask, so a stage has at most 64 destructible objects that
 *     collide, and a zone at most 64 records.
 */

/* Vector as the collision code holds it (copied with 64-bit moves). */
typedef struct StgColVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) StgColVec; /* size 0x10 */

/* Axis-aligned box: StgBox of battle/stg_a.h. */
typedef struct StgColBox {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} StgColBox; /* size 0x18 */

/* Sphere with its squared radius (the 0x20-byte sphere of battle/eft_a.h). */
typedef struct StgColSphere {
    /* 0x00 */ StgColVec pos;
    /* 0x10 */ f32 radius;
    /* 0x14 */ f32 radiusSq;
    /* 0x18 */ f32 unk18[2];
} StgColSphere; /* size 0x20 */

/* Segment handed to StgCol_TraceSegment (BtlAiSegment, BtlMoveSeg, EftISegment are the same thing). */
typedef struct StgColSeg {
    /* 0x00 */ StgColVec a;
    /* 0x10 */ StgColVec b;
} StgColSeg; /* size 0x20 */

/* Ray built from a segment by ColRay_FromSeg: origin and direction. */
typedef struct StgColRay {
    /* 0x00 */ StgColVec origin;
    /* 0x10 */ StgColVec dir;
} StgColRay; /* size 0x20 */

/* Swept sphere handed to StgCol_TraceSphere (the camera). */
typedef struct StgColSweep {
    /* 0x00 */ StgColVec a;
    /* 0x10 */ StgColVec b;
    /* 0x20 */ f32 radius;
    /* 0x24 */ f32 unk24[3];
} StgColSweep; /* size 0x30 */

/* Polygon header of a collision mesh (ColMesh_GetPoly). */
typedef struct StgColPoly {
    /* 0x00 */ s32 flags;      /* STGCOL_POLY_* */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ StgColVec nrm;  /* plane: normal and distance (n . p = w) */
} StgColPoly; /* size 0x20 */

/* StgColPoly.flags, as the queries filter them (meaning of the single bits inferred from who skips them). */
#define STGCOL_POLY_20 0x00000020
#define STGCOL_POLY_40 0x00000040
#define STGCOL_POLY_FLAT_ONLY 0x01000000 /* shadow: kept only when the normal points straight up */
#define STGCOL_POLY_NO_SHADOW 0x02000000 /* skipped by the shadow query only */
#define STGCOL_POLY_NO_CAMERA 0x04000000 /* skipped by the camera sweep only */
#define STGCOL_POLY_NO_SOLID  0x08000000 /* skipped by every query: not solid */
/* The fighter reads bits 0x80 and 0xC0 of the flags of the polygon it stands on / touches (ring-out on the
   tournament stages, see BtlColl_UpdateGround). */

/* A triangle fetched from a mesh: three corners and the plane. */
typedef struct StgColTri {
    /* 0x00 */ StgColVec v[3];
    /* 0x30 */ StgColVec nrm;
} StgColTri; /* size 0x40 */

/* Node of a collision mesh as the callbacks see it. */
typedef struct StgColNode {
    /* 0x00 */ u8 unk0[0x18];
    /* 0x18 */ s32 poly;       /* polygon index */
} StgColNode;

/* Collision record of a destructible object inside a zone. */
typedef struct StgColRec {
    /* 0x0 */ s32 broken;      /* 0: collision of the whole object; else: of the broken object */
    /* 0x4 */ s32 obj;         /* object index (BtlStage.objs) */
    /* 0x8 */ void *mesh;
    /* 0xC */ s32 unkC;
} StgColRec; /* size 0x10 */

/* A zone (StgZone of battle/stg_a.h with the collision fields named). */
typedef struct StgColZone {
    /* 0x00 */ s32 mark;       /* bit 0: visited by the segment trace in progress */
    /* 0x04 */ void *mesh;     /* collision mesh of the zone's static geometry */
    /* 0x08 */ s32 nearCount;
    /* 0x0C */ s32 *near;      /* neighbouring zone indices */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 recCount;
    /* 0x18 */ StgColRec *recs;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ StgColVec center;
    /* 0x30 */ StgColVec half;
} StgColZone; /* size 0x40 */

/* Destructible object (StgObj of battle/stg_a.h; only the state is read here). */
typedef struct StgColObj {
    /* 0x00 */ s32 state;      /* bit 0: broken */
    /* 0x04 */ u8 unk4[0x4C];
} StgColObj; /* size 0x50 */

/* The stage object (BtlStage of battle/stg_a.h). */
typedef struct StgColStage {
    /* 0x00 */ u8 unk0[0x3C];
    /* 0x3C */ StgColZone *zones;
    /* 0x40 */ void *bounds;
    /* 0x44 */ u32 objCount;
    /* 0x48 */ StgColObj *objs;
} StgColStage;

/* What one zone query touched. */
typedef struct StgColResult {
    /* 0x00 */ s32 hit;        /* anything */
    /* 0x04 */ s32 ground;     /* the zone's static mesh */
    /* 0x08 */ s32 rec;        /* a record */
    /* 0x0C */ s32 obj;        /* a record of a whole (unbroken) object */
    /* 0x10 */ u64 recMask;    /* bit n: record n of the zone */
    /* 0x18 */ u64 objMask;    /* bit n: unbroken object n */
} StgColResult; /* size 0x20 */

typedef s32 (*StgColCb)(StgColNode *node, void *ctx);

/* Context of the camera's swept sphere (StgCol_TraceSphere). */
typedef struct StgColSweepCtx {
    /* 0x00 */ StgColSweep seg;    /* a is moved back by two radii */
    /* 0x30 */ StgColSphere sphere; /* at the start */
    /* 0x50 */ StgColVec delta;    /* b - a */
    /* 0x60 */ StgColVec dir;      /* normalised */
    /* 0x70 */ f32 length;
    /* 0x74 */ f32 unk74[3];
    /* 0x80 */ f32 dist;           /* distance to the nearest hit so far */
    /* 0x84 */ f32 unk84[3];
    /* 0x90 */ StgColVec pos;      /* where the sphere touched */
    /* 0xA0 */ StgColTri tri;      /* what it touched */
    /* 0xE0 */ s32 hit;
} StgColSweepCtx; /* size 0xF0 */

/* Context of a fighter's body sphere being moved through the stage (StgCol_UpdateFighter). */
typedef struct StgColPushCtx {
    /* 0x00 */ s32 hit;
    /* 0x10 */ StgColSphere sphere;
    /* 0x30 */ StgColBox box;      /* sphere bounds, handed to the mesh walk */
    /* 0x50 */ StgColPoly poly;    /* last polygon that pushed the sphere */
} StgColPushCtx; /* size 0x70 */

/* Triangle list filled by StgCol_CollectSphere (the hit buffer of a stage rigid body). */
typedef struct StgColTriList {
    /* 0x00 */ u32 count;      /* at most 0x40 */
    /* 0x10 */ StgColTri tris[0x40];
} StgColTriList;

/* Context of the segment trace and of the triangle collection: the one static block gStgColCtx. */
typedef struct StgColCtx {
    /* 0x00 */ StgColSphere sphere; /* collection: the sphere */
    /* 0x20 */ StgColSeg seg;       /* trace: the segment */
    /* 0x40 */ StgColBox box;       /* bounds of the segment / sphere */
    /* 0x60 */ StgColRay ray;       /* trace: the segment as a ray */
    /* 0x80 */ f32 dist;            /* trace: distance of the nearest hit, FLT_MAX at start */
    /* 0x84 */ f32 unk84[3];
    /* 0x90 */ StgColVec pos;       /* trace: hit position */
    /* 0xA0 */ StgColPoly poly;     /* trace: polygon hit */
    /* 0xC0 */ StgColTriList *list; /* collection: output */
    /* 0xC4 */ s32 obj;             /* index of the unbroken object touched, -1 none */
    /* 0xC8 */ s32 hit;
} StgColCtx; /* size 0xD0 */

/* Result of the last segment trace, the static block gStgColHit (BtlAiHit / EftIStageHit are views of it). */
typedef struct StgColHit {
    /* 0x00 */ s32 hit;
    /* 0x10 */ StgColVec pos;
    /* 0x20 */ StgColPoly poly;
    /* 0x40 */ s32 obj;        /* unbroken destructible object hit, -1: static geometry (or a broken object) */
} StgColHit; /* size 0x50 */

/* Context of the shadow query. */
typedef struct StgShadowCtx {
    /* 0x00 */ s32 max;        /* capacity of buf */
    /* 0x04 */ s32 count;
    /* 0x08 */ StgColTri *buf;
    /* 0x10 */ StgColVec pt;   /* centre of the box at its top */
    /* 0x20 */ f32 groundY;    /* highest ground found under pt (smallest y); starts at the probe's bottom */
    /* 0x24 */ f32 bottomY;    /* bottom of the caller's box */
    /* 0x28 */ StgColBox probe; /* thin column through the centre */
    /* 0x40 */ StgColBox box;  /* the caller's box extended downwards by 50 */
} StgShadowCtx; /* size 0x60 */

/* What the shadow query leaves for the drawing code (gStgShadow). */
typedef struct StgShadowOut {
    /* 0x0 */ f32 alpha;       /* 1 up to 20 above the ground, fading to 0 at 50 */
    /* 0x4 */ s32 count;       /* triangles collected, 0 when alpha is 0 */
} StgShadowOut;

/* The body block of a battle object (BtlObj + 0xF50), as far as the stage collision uses it. */
typedef struct StgColBody {
    /* 0x00 */ u8 unk0[0x50];
    /* 0x50 */ StgColSphere *cur;  /* body sphere at the position of this frame */
    /* 0x54 */ StgColSphere *prev; /* body sphere at the position of the previous frame, NULL: none */
    /* 0x58 */ u8 unk58[8];
    /* 0x60 */ StgColVec push;     /* how far the stage pushed the object this frame (old - new); zero: free */
    /* 0x70 */ u8 unk70[0x44];
    /* 0xB4 */ f32 radius;         /* full radius of the body sphere */
    /* 0xB8 */ f32 curRadius;      /* radius used against the stage: grows by 1 per frame up to radius, >= 4 */
    /* 0xBC */ s32 teleported;     /* 1: do not sweep from the previous position this frame */
} StgColBody;

/* The pose block of a battle object (BtlObj + 0x950). */
typedef struct StgColPose {
    /* 0x00 */ StgColVec pos;
    /* 0x10 */ u8 unk10[0xC4];
    /* 0xD4 */ s32 zone;           /* zone index the object was in last frame */
} StgColPose;

/* Work buffer of a fighter's battle object (BtlCollObjWork of battle/btl_char_coll.h). */
typedef struct StgColObjWork {
    /* 0x00000 */ u8 unk0[0x18020];
    /* 0x18020 */ s32 flags;       /* STGCOL_HIT_* */
    /* 0x18024 */ u8 unk18024[0x3C];
    /* 0x18060 */ StgColPoly contact; /* the polygon the body sphere was pushed out of (flags, plane) */
} StgColObjWork;

/* StgColObjWork.flags, the "stage contact word" of a fighter. */
/* The whole word is zeroed by BtlObjBody_BeginFrame, the first call of StgCol_UpdateFighter, so every bit lasts one frame. */
#define STGCOL_HIT_WALL 0x20    /* the body sphere was pushed out of stage geometry this frame */
#define STGCOL_HIT_BROKE 0x40   /* the fighter broke a stage object this frame */
#define STGCOL_HIT_DMG_200 0x80   /* the broken object has type bit 0x800 (BtlColl_UpdateGround: 200 damage) */
#define STGCOL_HIT_DMG_600 0x100  /* type bit 0x2000 (600 damage) */
#define STGCOL_HIT_DMG_1000 0x200 /* type bit 0x1000 (1000 damage) */

/* Battle object of a fighter (BtlObj of battle/btl_obj.h). */
typedef struct StgColFighter {
    /* 0x0000 */ u8 unk0[0x10];
    /* 0x0010 */ s32 objId;
    /* 0x0014 */ u8 unk14[0x950 - 0x14];
    /* 0x0950 */ StgColPose pose;
    /* 0x0A30 */ u8 unkA30[0xF50 - 0xA30];
    /* 0x0F50 */ StgColBody body;
    /* 0x1010 */ u8 unk1010[0x1660 - 0x1010];
    /* 0x1660 */ StgColObjWork *work;
} StgColFighter;

/* ---- stage navigation graph (eft_det_b_b.c) ---- */

/* Way point: 0x30 bytes. */
typedef struct StgNavNode {
    /* 0x00 */ struct StgNavPoint { f32 v[4]; } pos; /* 4-byte aligned in the file (copied with unaligned moves) */
    /* 0x10 */ s32 flags;      /* STGNAV_NODE_* */
    /* 0x14 */ s32 obj;        /* destructible object standing on the node, -1 none */
    /* 0x18 */ s32 unk18[2];
    /* 0x20 */ s16 link[8];    /* neighbouring nodes, -1 unused */
} StgNavNode; /* size 0x30 */

#define STGNAV_NODE_OFF 1      /* never used: skipped as a start, as a goal and as a step */
#define STGNAV_NODE_OBJ 2      /* its object is still standing; set by StgNav_BlockObj, cleared when it breaks.
                                  Read by nothing in the game: the path search ignores it. */

/* The graph inside the stage file. */
typedef struct StgNavData {
    /* 0x00 */ s32 version;    /* must be 1 */
    /* 0x04 */ s32 count;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ StgNavNode *nodes; /* written at bind time: this + 0x10 */
} StgNavData;

/* Entry of the search lists. */
typedef struct StgNavEntry {
    /* 0x0 */ s32 node;
    /* 0x4 */ struct StgNavEntry *from; /* the entry this one was reached from (in the closed list) */
    /* 0x8 */ s32 steps;      /* links walked from the start */
} StgNavEntry; /* size 0xC */

/* The manager (gStgNav, 0x14 bytes from the heap). */
typedef struct StgNav {
    /* 0x00 */ StgNavData *data;
    /* 0x04 */ s32 ready;
    /* 0x08 */ s32 stage;      /* Battle_GetStage() at init */
    /* 0x0C */ StgNavEntry *open;   /* 0xC00 bytes = 256 entries */
    /* 0x10 */ StgNavEntry *closed; /* 0xC00 bytes */
} StgNav; /* size 0x14 */

/* Path as the AI holds it (BtlAiMovePath of battle/btl_ai.h): goal first, start side last. */
typedef struct StgNavPoint StgNavPoint;

typedef struct StgNavPath {
    /* 0x000 */ StgNavPoint pts[16];
    /* 0x100 */ s32 count;
} StgNavPath;

/* ---- head of the AI sequence object (eft_det_b_c.c): local views of battle/btl_ai.h ---- */

typedef struct DetAiSeqEntry {
    /* 0x0 */ u32 id;
    /* 0x4 */ s8 arg;
} DetAiSeqEntry; /* size 8 */

/* BtlAiSeq of battle/btl_ai.h. */
typedef struct DetAiSeq {
    /* 0x00 */ s32 flags;
    /* 0x04 */ s32 depth;
    /* 0x08 */ s32 phase;
    /* 0x0C */ s32 stepCount;
    /* 0x10 */ s32 step;
    /* 0x14 */ DetAiSeqEntry stack[8];
    /* 0x54 */ u8 unk54[0x1C];
    /* 0x70 */ s32 skip;       /* rule pushes left during which action 2 with argument 0x80 is dropped */
    /* 0x74 */ u8 unk74[0x24];
} DetAiSeq; /* size 0x98 */

/* Plan block (BtlAiWork + 0x2E8), the one field used here. */
typedef struct DetAiPlan {
    /* 0x00 */ u8 unk0[0xA4];
    /* 0xA4 */ s32 timerA4;    /* set to 90 when action 0x3D is pushed */
} DetAiPlan;

/* BtlAiWork of battle/btl_ai.h. */
typedef struct DetAiWork {
    /* 0x000 */ s32 objId;
    /* 0x004 */ s32 type;
    /* 0x008 */ s32 level;     /* cpu level 0..29, negative for the training dummy */
    /* 0x00C */ u8 unkC[0x1C];
    /* 0x028 */ DetAiSeq seq;
    /* 0x0C0 */ u8 unkC0[0x120];
    /* 0x1E0 */ s32 pathCount; /* move.path.count */
    /* 0x1E4 */ u8 unk1E4[0x104];
    /* 0x2E8 */ u8 plan[0xA4];
    /* 0x38C */ s32 planTimerA4; /* plan + 0xA4: set to 90 when action 0x3D is pushed */
} DetAiWork;

/* AiThRule of battle/btl_ai_think.h: the action part. */
typedef struct DetAiRule {
    /* 0x00 */ u8 unk0[4];
    /* 0x04 */ u8 unk4[4];     /* BtlAiSeq_PushRule reads the action ids as unk4[i + 16] (it matches only so) */
    /* 0x08 */ u8 unk8[4];     /* and their arguments as unk8[i + 16] */
    /* 0x0C */ u8 unkC[8];
    /* 0x14 */ u8 act[4];      /* action (sequence) ids, 0xFF ends the list */
    /* 0x18 */ u8 arg[4];      /* argument + 0x7F */
} DetAiRule;

void BtlAiSeq_Reset(DetAiSeq *seq);
void BtlAiSeq_PushRule(DetAiWork *ai, DetAiRule *rule);

extern StgColHit gStgColHit;
extern StgColCtx gStgColCtx;
extern StgShadowOut gStgShadow;
extern StgNav *gStgNav;

/* eft_det_b.c */
void StgCol_SetModeAll(void);
void StgCol_SetModeObjects(void);
s32 StgCol_QueryZone(StgColResult *res, StgColZone *zone, StgColBox *box, void *ctx, StgColCb cb);
s32 StgCol_FirstBit(u64 mask);
s32 StgCol_SweepCb(StgColNode *node, StgColSweepCtx *ctx);
s32 StgCol_TraceSphere(s32 zoneIdx, StgColSweep *seg, StgColVec *hitPos, f32 *frac, StgColTri *tri);
s32 StgCol_SplitStep(StgColVec *out, StgColVec *delta, f32 radius, f32 dist);
void StgCol_StepSphere(StgColSphere *sphere, StgColBox *box, StgColVec *step);
void StgCol_PushSphere(StgColSphere *sphere, StgColBox *box, StgColVec *point);
s32 StgCol_PushCb(StgColNode *node, StgColPushCtx *ctx);
s32 StgCol_MoveFighter(StgColFighter *obj, StgColPushCtx *ctx, s32 zoneIdx, s32 steps, StgColVec *step,
                       StgColVec *delta, s32 crossZones, s32 keepSphere);
void StgCol_UpdateFighter(StgColFighter *obj, s32 keepSphere, s32 grow);
void StgCol_FighterBreakObj(StgColFighter *obj, s32 idx, StgColVec *hitPos);
s32 StgCol_SegCb(StgColNode *node, StgColCtx *ctx);
s32 StgCol_CollectCb(StgColNode *node, StgColCtx *ctx);
void StgCol_InitSegCtx(StgColCtx *ctx, StgColSeg *seg);
void StgCol_InitSphereCtx(StgColCtx *ctx, StgColSphere *sphere, StgColTriList *list);
void StgCol_GetZoneBox(StgColBox *box, s32 zoneIdx);
void StgCol_ClipSegToZone(StgColBox *out, s32 cur, s32 from, s32 to, StgColCtx *ctx, StgColBox *zoneBox,
                          StgColVec *entry);
void StgCol_TraceZone(s32 cur, s32 from, s32 to, StgColCtx *ctx);
void StgCol_CollectZone(s32 zoneIdx, StgColCtx *ctx);
s32 StgCol_TraceSegment(StgColSeg *seg);
s32 StgCol_GetHitPos(StgColVec *out);
StgColHit *StgCol_GetHit(void);
s32 StgCol_CollectSphere(StgColSphere *sphere, StgColTriList *list);
void StgShadow_InitCtx(StgShadowCtx *ctx, StgColBox *box, s32 max, StgColTri *buf);
s32 StgShadow_ProbeTri(StgColVec *pt, StgColTri *tri, f32 *y);
s32 StgShadow_AddTri(StgShadowCtx *ctx, StgColTri *tri, s32 flags);
s32 StgShadow_Cb(StgColNode *node, StgShadowCtx *ctx);
f32 StgShadow_CalcAlpha(f32 height);
void StgShadow_Finish(StgShadowCtx *ctx);
s32 StgShadow_Collect(s32 zoneIdx, StgColBox *box, s32 max, StgColTri *buf);
StgShadowOut *StgShadow_GetOut(void);

/* eft_det_b_b.c */
s32 StgNav_IsReady(void);
s32 StgNav_Bind(void);
void StgNav_Term(void);
void StgNav_Rebind(void);
s32 StgNav_Init(void);
s32 StgNav_FindNearest(StgColVec *pos);
void StgNavNode_Clear(StgNavNode *node);
void StgNav_SwapEntries(StgNavEntry *a, StgNavEntry *b);
void StgNav_SortEntries(s32 lo, s32 hi, StgNavEntry *list);
void StgNav_FindPath(StgColVec *from, StgNavPoint *to, StgNavPath *path);
void StgNav_UnblockObj(s32 idx);
void StgNav_BlockObj(s32 idx);

#endif
