#ifndef BATTLE_STG_A_H
#define BATTLE_STG_A_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Battle stage, first part (src/battle/stg_a.c, 0x23FB20..0x242D28).
 *
 * The stage file (BattleRes.stage, file 0x171 + stage or 0x198 + stage in split-screen) is a pack whose header
 * is a table of byte offsets (member n of BtlStage_GetFileMember is the word at +0x0C + 4 * n; a member whose
 * offset equals the next one is empty). Three blocks are bound here (BtlStage_BindFile):
 *   header +0x08:              StgDataA, the stage parameter block (StgData in battle/stg_b.h is the same block)
 *   header +0x4C (member 16):  an optional "MEF0" block, gBtlStageFxRes (NULL when the member is empty)
 *   header +0x60 (member 21):  StgPathTable, the path points used by the clash / placement code
 * All three hold file-relative offsets that are turned into pointers once, and all positions are stored with
 * the opposite sign on y and z: binding negates them (the game's +Y is down).
 */

/* Generic offset table: entries are file-relative until StgOfsTable_Relocate ran. Used by the stage model code
 * at 0x115290 / 0x115478. */
typedef struct StgOfsTable {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 count;
    /* 0x0C */ s32 relocated;
    /* 0x10 */ s32 entries[1];
} StgOfsTable;

/* One stage path (the same thing as BtlClashPath / BtlCtlPath on the fighter side). */
typedef struct StgPath {
    /* 0x00 */ s32 end;      /* one past the last usable point */
    /* 0x04 */ s32 points;   /* 0x30 bytes per point; an offset until relocated */
    /* 0x08 */ s32 first;
    /* 0x0C */ s32 unkC;
} StgPath; /* size 0x10 */

typedef struct StgPathTable {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 count;
    /* 0x0C */ StgPath *paths;
    /* 0x10 */ s32 relocated;
} StgPathTable;

/* A point of the "MEF0" block: 0x30 bytes, a position first. */
typedef struct StgMefPoint {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ u8 unkC[0x24];
} StgMefPoint; /* size 0x30 */

/* The optional "MEF0" block: three lists of points. Only bound here; read by other stage code. */
typedef struct StgMef {
    /* 0x00 */ u32 magic;  /* 'MEF0' = 0x3046454D */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 count0;
    /* 0x0C */ StgMefPoint *list0;
    /* 0x10 */ s32 count1;
    /* 0x14 */ StgMefPoint *list1;
    /* 0x18 */ s32 count2;
    /* 0x1C */ StgMefPoint *list2;
} StgMef;

/* Definition of a destructible object (StgDataA +0x0C). */
typedef struct StgObjDef {
    /* 0x00 */ s32 hp;       /* damage it takes before breaking */
    /* 0x04 */ s32 type;     /* STG_OBJ_* bits */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 parent;   /* index of the object whose breaking also breaks this one */
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ Vec4 pos;
    /* 0x30 */ u8 unk30[0x10];
} StgObjDef; /* size 0x40 */

/* StgObj.type / StgObjDef.type bits that this file reads. */
#define STG_OBJ_MAT0 0x1          /* material 0 (dust kind 0) */
#define STG_OBJ_MAT1 0x2          /* material 1 */
#define STG_OBJ_MAT2 0x4          /* material 2 */
#define STG_OBJ_SHAKE_S 0x10      /* breaking shakes cameras / pads: small */
#define STG_OBJ_SHAKE_M 0x20      /* medium */
#define STG_OBJ_SHAKE_L 0x40      /* large */
#define STG_OBJ_SPLASH 0x80       /* pieces falling through y = 0 make a splash */
#define STG_OBJ_SMALL_DUST 0x200  /* dust scale 0.7 instead of 1 */
#define STG_OBJ_ITEM 0x20000000   /* may hold the hidden item of mode 1 (rule.unk18) */

/* Stage environment (StgDataA +0x14). */
typedef struct StgEnv {
    /* 0x00 */ s32 id;       /* stage number (battle/stg_b.h) */
    /* 0x04 */ s32 flags;    /* bit 1: the stage has water */
    /* 0x08 */ f32 waterY;   /* water level; stored negated in the file like every y */
    /* 0x0C */ s32 changeKind; /* what a stage change leads to (battle/stg_b.h, BtlStage_GetChangeTarget) */
} StgEnv; /* size 0x10 */

/* A placement: where to stand and what to face. */
typedef struct StgPlace {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 target;
} StgPlace; /* size 0x20 */

/* Stage light (StgDataA +0x1C): two vectors. */
typedef struct StgLight {
    /* 0x00 */ Vec4 dirA;
    /* 0x10 */ Vec4 dir;     /* the one the battle object light uses */
} StgLight; /* size 0x20 */

typedef struct StgVec { f32 x, y, z, w; } StgVec;
typedef struct StgRec18 { f32 x, y, z; u8 unkC[0xC]; } StgRec18;

/* The stage parameter block: pairs of (count, pointer). */
typedef struct StgDataA {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 objCount;    StgObjDef *objDefs;    /* 0x0C */
    /* 0x10 */ s32 envCount;    StgEnv *env;           /* 0x14 */
    /* 0x18 */ s32 lightCount;  StgLight *light;       /* 0x1C */
    /* 0x20 */ s32 pointCount;  StgVec *points;        /* 0x24 */
    /* 0x28 */ s32 startCount;  StgPlace *starts;      /* 0x2C: one per player */
    /* 0x30 */ s32 altCount;    StgPlace *altStarts;   /* 0x34 */
    /* 0x38 */ s32 placeCount;  StgPlace *places;      /* 0x3C */
    /* 0x40 */ s32 count40;     StgVec *list40;        /* 0x44: 0x10 bytes each, not negated */
    /* 0x48 */ s32 count48;     void *list48;          /* 0x4C */
    /* 0x50 */ s32 count50;     void *list50;          /* 0x54 */
    /* 0x58 */ s32 count58;     StgPlace *list58;      /* 0x5C: 0x20 bytes each, y z of the first vector negated */
    /* 0x60 */ s32 count60;     StgRec18 *list60;      /* 0x64: 0x18 bytes each */
    /* 0x68 */ s32 count68;     void *list68;          /* 0x6C */
    /* 0x70 */ s32 count70;     void *list70;          /* 0x74 */
    /* 0x78 */ s32 count78;     void *list78;          /* 0x7C: light colours (btl_obj.c) */
    /* 0x80 */ s32 count80;     void *list80;          /* 0x84 */
    /* 0x88 */ s32 count88;     void *list88;          /* 0x8C */
    /* 0x90 */ s32 count90;     void *list90;          /* 0x94 */
    /* 0x98 */ s32 count98;     void *list98;          /* 0x9C */
    /* 0xA0 */ s32 countA0;     void *listA0;          /* 0xA4 */
} StgDataA;

/* A culling / path-finding zone: an axis-aligned rectangle on the ground plane. */
typedef struct StgZone {
    /* 0x00 */ s32 mark;       /* cleared by BtlStage_ClearZoneMarks */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 nearCount;
    /* 0x0C */ s32 *near;      /* indices of the neighbouring zones */
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ Vec4 center;    /* x and z used */
    /* 0x30 */ Vec4 half;      /* half extents, x and z used */
} StgZone; /* size 0x40 */

/* The three limits a fighter is clamped to (BtlMove_ClampToStage). */
typedef struct StgBounds {
    /* 0x00 */ f32 radius;     /* horizontal distance from the origin */
    /* 0x04 */ f32 top;        /* smallest y (highest point) */
    /* 0x08 */ f32 bottom;     /* largest y (lowest point) */
} StgBounds;

/* Keyframe of a debris piece. */
typedef struct StgKey {
    /* 0x00 */ f32 x, y, z;
    /* 0x0C */ s32 frame;
    /* 0x10 */ f32 rx, ry, rz; /* degrees */
    /* 0x1C */ s32 unk1C;
} StgKey; /* size 0x20 */

/* Transform node of a stage model part. */
typedef struct StgNode {
    /* 0x00 */ Mtx44 mtx;      /* row 3 = position */
    /* 0x40 */ s32 keyCount;   /* 1: the piece is driven by a rigid body instead of keys */
    /* 0x44 */ StgKey *keys;
    /* 0x48 */ s32 endFrame;
    /* 0x4C */ s32 body;       /* rigid body handle (StgRigid_Create), < 0 none */
} StgNode;

/* A stage model part. */
typedef struct StgPart {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u16 flags;      /* bit 0: drawn (the cull at 0x1156E0 skips parts without it) */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ StgNode *node;
    /* 0x0C */ f32 radius;     /* bounding sphere */
    /* 0x10 */ f32 x, y, z;
} StgPart;

/* StgObj.state */
#define STG_OBJ_BROKEN 1     /* broken: no longer takes damage */
#define STG_OBJ_FALLING 2    /* its pieces are being animated */

/* A destructible stage object (runtime). */
typedef struct StgObj {
    /* 0x00 */ s32 state;        /* STG_OBJ_BROKEN | STG_OBJ_FALLING */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 hp;           /* from StgObjDef.hp */
    /* 0x0C */ StgPart *parts[6]; /* [0] [2] [4]: the whole model (drawn while whole); [1] [3] [5]: the broken one.
                                     Any of them may be NULL. [0] gives the object's position and radius. */
    /* 0x24 */ u32 pieceCount;
    /* 0x28 */ StgPart **pieces; /* debris pieces */
    /* 0x2C */ s32 animEnd;      /* length of the break animation in frames, 0x94 when 0 */
    /* 0x30 */ s32 frame;        /* advances by 2 per update while falling */
    /* 0x34 */ s32 type;         /* from StgObjDef.type */
    /* 0x38 */ s32 parent;       /* from StgObjDef.parent */
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ Vec4 hitPos;      /* where the breaking hit was */
} StgObj; /* size 0x50 */

/* The stage object (gBtlStage). Built by the stage model code at 0x114C60; only these fields are used here. */
typedef struct BtlStage {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ StgDataA *data;
    /* 0x08 */ s32 flags;        /* bit 0: not ready */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u32 timerCount;   /* battle/stg_b.h */
    /* 0x14 */ void *timers;
    /* 0x18 */ u8 unk18[0x20];
    /* 0x38 */ u32 zoneCount;
    /* 0x3C */ StgZone *zones;
    /* 0x40 */ StgBounds *bounds;
    /* 0x44 */ u32 objCount;
    /* 0x48 */ StgObj *objs;
} BtlStage;

/* What BtlStage_BreakObj reports about the objects it broke. */
typedef struct StgBreakInfo {
    /* 0x00 */ Vec4 pos;         /* position of the last broken object */
    /* 0x10 */ f32 radius;       /* its radius */
    /* 0x14 */ s32 types;        /* OR of the type bits of all broken objects */
    /* 0x18 */ s32 count;        /* number of objects broken */
    /* 0x1C */ s32 unk1C;
} StgBreakInfo; /* size 0x20 */

/* View frustum of one camera, for culling stage parts. */
typedef struct StgFrustum {
    /* 0x00 */ Vec4 plane[5];    /* view-space normals: +x, -x, +y, -y sides (normalised) and (0, 0, 1) */
    /* 0x50 */ Mtx44 view;       /* copy of the view's matrix at +0x40 */
    /* 0x90 */ Mtx44 proj;       /* perspective projection built from the near / far planes */
} StgFrustum; /* size 0xD0 */

/* Four floats on a 16-byte boundary (the constant extent of BtlStage_ProbeGround is copied with ld / sd). */
typedef struct StgVec16 {
    f32 x, y, z, w;
} __attribute__((aligned(16))) StgVec16;

/* Axis-aligned box as ColBox_SetCenterHalf builds it (BtlObjBox in battle/btl_obj.h). */
typedef struct StgBox {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} StgBox; /* size 0x18 */

/* Result of BtlStage_ProbeGround (filled by StgGround_Probe). */
typedef struct StgGroundHit {
    /* 0x00 */ f32 x, y, z;      /* y = ground height under the probe */
    /* 0x0C */ u8 unkC[0x24];
} StgGroundHit; /* size 0x30 */

/* Local view of the camera view (View in battle/btl_cam.h). */
typedef struct StgView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ Mtx44 mtx;
    /* 0x080 */ u8 unk80[0x180];
    /* 0x200 */ s32 x0;
    /* 0x204 */ s32 unk204;
    /* 0x208 */ s32 x1;
    /* 0x20C */ s32 unk20C;
    /* 0x210 */ u8 unk210[0x10];
    /* 0x220 */ Vec4 pos;
    /* 0x230 */ f32 unk230;
    /* 0x234 */ f32 screenDist;
    /* 0x238 */ u8 unk238[0x18];
    /* 0x250 */ f32 nearZ;
    /* 0x254 */ f32 farZ;
} StgView;

extern BtlStage *gBtlStage; /* named in config/symbols/stg_b.txt */

/* Offset tables of the stage file. */
void StgOfsTable_Relocate(StgOfsTable *t);
s32 StgOfsTable_Get(StgOfsTable *t, s32 i);
s32 StgOfsTable_GetCount(StgOfsTable *t);
void StgPathTable_Relocate(StgPathTable *t);

/* Life cycle. */
s32 BtlStage_IsReady(void);
void BtlStage_Reset(void);
void BtlStage_Init(BtlStage *stage, void *arg);
void BtlStage_Term(void);
void BtlStage_BindFile(BtlStage *stage);
void BtlStage_BindData(BtlStage *stage, StgDataA *base);
void BtlStage_BindMef(void);
void BtlStage_BindPaths(void);
void BtlStage_RequestChange(void);

/* Bounds, zones, ground, water, placements: what the simulation reads. */
f32 BtlStage_GetRadius(void);
f32 BtlStage_GetInnerRadius(void);
f32 BtlStage_GetTop(void);
f32 BtlStage_GetBottom(void);
s32 BtlStage_FindZone(Vec4 *pos);
s32 BtlStage_FindZoneNear(s32 zone, Vec4 *pos);
StgZone *BtlStage_GetZone(s32 zone);
void BtlStage_ClearZoneMarks(void);
s32 BtlStage_GetWaterLevel(f32 *level);
s32 BtlStage_ProbeGround(s32 zone, Vec4 *pos, StgGroundHit *out);
s32 BtlStage_GetStartPlace(s32 side, Vec4 *pos, Vec4 *rot, s32 alt);
s32 BtlStage_GetPlace(Vec4 *pos, Vec4 *rot);
f32 Stg_CalcYaw(f32 x0, f32 z0, f32 x1, f32 z1);
void BtlStage_ClearPaths(void);
s32 BtlStage_GetPathCount(void);
StgPath *BtlStage_GetPath(s32 n);
StgPath *BtlStage_GetPath0(void);

/* Destructible objects. */
s32 BtlStage_BreakObj(s32 idx, Vec4 *hitPos, StgBreakInfo *info);
void BtlStage_ResetObjs(s32 broken);
s32 BtlStage_DestroyObj(s32 objId, s32 idx, Vec4 *hitPos);
s32 BtlStage_IsObjBroken(s32 idx);
s32 BtlStage_DamageObj(s32 idx, s32 damage);
s32 BtlStage_GetObjType(s32 idx);
void BtlStage_UpdateObjs(void);
void StgObj_LoadDef(StgObj *obj, s32 idx);
s32 StgObj_GetDust(StgObj *obj, f32 *scale);
s32 StgObj_GetMaterial(StgObj *obj);
s32 StgObj_GetItemPos(Vec4 *out, StgObj *obj, s32 idx);
s32 StgNode_IsRigid(StgNode *node);
void StgPart_GetPos(StgPart *part, Vec4 *out);
void StgPart_Animate(StgPart *part, f32 frame, Vec4 *out);
f32 Stg_WrapRange(f32 lo, f32 hi, f32 v);

/* Drawing helpers (culling, fades, light). */
void StgFrustum_Build(StgView *view, StgFrustum *fr);
s32 StgFrustum_TestBox(StgFrustum *fr, f32 x, f32 y, f32 z, f32 r);
s32 StgFrustum_TestPart(StgPart *part, StgFrustum *fr);
f32 Stg_FadeRatio(f32 a, f32 b);
f32 Stg_FadeByCamDist(Vec4 *pos, f32 value);
void BtlStage_GetLightVecA(Vec4 *out);
void BtlStage_GetLightVecB(Vec4 *out);

/* Sections of the stage data. */
StgObjDef *BtlStage_GetObjDef(s32 idx);
StgVec *BtlStage_GetList40(s32 idx);
void *BtlStage_GetList48(void);
void *BtlStage_GetList50(void);
void *BtlStage_GetList90(void);
void *BtlStage_GetListA0(void);
void *BtlStage_GetList98(void);
void *BtlStage_GetList60(void);
void *BtlStage_GetList68(void);
void *BtlStage_GetList58(void);
void *BtlStage_GetLightColors(void);
void *BtlStage_GetList80(void);

#endif
