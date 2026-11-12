#ifndef BATTLE_EFT_DET_A_H
#define BATTLE_EFT_DET_A_H

#include "types.h"

/*
 * src/battle/eft_det_a.c, 0x1AE2A8..0x1B16F0 (the brief's range starts at 0x1AE200, which is inside the function
 * 0x1AE1F8..0x1AE2A8; that one belongs to the file before, stem eft_ae). Five groups, in address order:
 *
 *   EftTexSet_* / EftVram_*  0x1AE2A8..0x1AE5F8  tail of the effect texture code: three more "texture set from a
 *                                                pack" loaders and the VRAM upload of one texture. VISUAL.
 *   EftVolleyAim_*           0x1AE5F8..0x1AF508  direction, spread and steering of the shots of a volley
 *                                                (technique effect type 1, eft_h.c). SIMULATION, and it draws
 *                                                the scene generator (BtlScene_RandRange / RandRangeF).
 *   BtlBodyHit_*             0x1AF508..0x1AF7B8  fighter against fighter: a fighter's active attack volumes
 *                                                against the other fighter's body parts. SIMULATION.
 *   EftDet_*                 0x1AF7B8..0x1B1260  the hit detection over gEftHitList: record against record
 *                                                (clash), record against stage, record against fighter.
 *                                                SIMULATION: this is where a projectile hits.
 *   StgGround_*              0x1B1260..0x1B16F0  the ground probe (highest ground triangle under a point) and
 *                                                the per-frame ground query of a fighter. SIMULATION.
 *
 * All structures are local views (the record list is described in battle/eft_a.h, the fighter side in
 * battle/btl_char_coll.h, the stage in battle/stg_a.h and battle/eft_det_b.h).
 */

/* 16-byte aligned vector (EftVec of battle/eft_a.h). */
typedef union EftDetVec {
    struct {
        /* 0x0 */ f32 x;
        /* 0x4 */ f32 y;
        /* 0x8 */ f32 z;
        /* 0xC */ f32 w;
    };
    f32 v[4];
} __attribute__((aligned(16))) EftDetVec;

/* 4x4 matrix. */
typedef struct EftDetMtx {
    /* 0x00 */ EftDetVec row[4];
} EftDetMtx; /* size 0x40 */

/* Sphere as ColSphere_Set fills it (EftHitSphere of battle/eft_a.h). */
typedef struct EftDetSphere {
    /* 0x00 */ EftDetVec pos;
    /* 0x10 */ f32 radius;
    /* 0x14 */ f32 unk14[3];
} EftDetSphere; /* size 0x20 */

/* Capsule as ColCapsule_Set fills it: a segment and a radius (EftHitBox of battle/eft_a.h, which calls it a box). */
typedef struct EftDetCapsule {
    /* 0x00 */ EftDetVec start;
    /* 0x10 */ EftDetVec end;
    /* 0x20 */ f32 radius;
    /* 0x24 */ f32 unk24[3];
} EftDetCapsule; /* size 0x30 */

/* Axis-aligned box (StgBox of battle/stg_a.h). */
typedef struct EftDetBox {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} EftDetBox; /* size 0x18 */

/* A triangle of a collision mesh with its plane (StgColTri of battle/eft_det_b.h). */
typedef struct EftDetTri {
    /* 0x00 */ EftDetVec v[3];
    /* 0x30 */ EftDetVec nrm; /* plane: n . p = w */
} EftDetTri; /* size 0x40 */

/* EftDetShape.hitFlags */
#define EFT_DET_HIT_STAGE   1 /* the stage pass found a contact (stageDist is valid) */
#define EFT_DET_HIT_FIGHTER 2 /* the fighter pass found a contact in front of the stage contact */
#define EFT_DET_HIT_CLASH   4 /* the clash pass found a contact (set on the first record of the pair only) */

/*
 * Shape block of a hit record (record + 0x70, 0x120 bytes). The first 0x14 bytes are what the projectile task fills
 * (EftHitShape of battle/eft_a.h); the rest is the hit detection's work area, rebuilt every frame by
 * EftDet_PrepareAll.
 *
 * type 0: a and b are spheres (EftDetSphere). b is where the sweep STARTS (the previous position) and a where it
 *         ends (the current position): delta = a - b. (battle/eft_a.h describes them the other way round.)
 * type 1: a and b are capsules (EftDetCapsule) from the muzzle to the head, b the previous frame's and a the
 *         current one. Against fighters and other records only `a` is used: a sphere of a's radius at a->start
 *         swept along the capsule's own axis (delta = a->end - a->start). Against the stage the volume is the
 *         capsule from b->end to a->end.
 * types 2..6: no test at all (the callbacks are stubs).
 */
typedef struct EftDetShape {
    /* 0x00 */ s32 type;
    /* 0x04 */ f32 scale;       /* multiplies the radius in the stage test only */
    /* 0x08 */ f32 unk8;
    /* 0x0C */ void *a;
    /* 0x10 */ void *b;
    /* 0x14 */ s32 unk14[3];
    /* 0x20 */ EftDetVec hitPos;   /* centre of the swept volume at the moment of contact */
    /* 0x30 */ EftDetVec contact;  /* point of contact: what EftHit_SetTaskFlag stores in the task */
    /* 0x40 */ EftDetTri tri;      /* stage triangle that was hit (handed to the blast task) */
    /* 0x80 */ s32 zone;           /* stage zone of the record, followed from frame to frame */
    /* 0x84 */ EftDetBox bounds;   /* box around the swept volume */
    /* 0x9C */ s32 unk9C;
    /* 0xA0 */ EftDetVec delta;    /* movement of the volume this frame */
    /* 0xB0 */ EftDetVec dir;      /* delta normalised */
    /* 0xC0 */ f32 dist;           /* distance from the start of the sweep to the fighter contact */
    /* 0xC4 */ f32 stageDist;      /* distance from the start of the sweep to the stage contact */
    /* 0xC8 */ s32 hitFlags;       /* EFT_DET_HIT_* */
    /* 0xCC */ u8 unkCC[0x120 - 0xCC];
} EftDetShape; /* size 0x120 */

/* Technique definition (EftHitDef of battle/eft_a.h). */
typedef struct EftDetDef {
    /* 0x00 */ u8 unk0[4];
    /* 0x04 */ s8 cls;
    /* 0x05 */ s8 kind;
} EftDetDef;

/* Source of a technique record (EftHitSrc of battle/eft_a.h). */
typedef struct EftDetSrc {
    /* 0x00 */ u8 unk0[0x24];
    /* 0x24 */ EftDetDef *def;
} EftDetSrc;

/* One hit record (EftHitRec of battle/eft_a.h, BtlCollHit of battle/btl_char_coll.h). */
typedef struct EftDetRec {
    /* 0x000 */ s32 objId;      /* fighter it belongs to */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ s32 level;
    /* 0x00C */ s32 type;       /* 0 ki blast, 1 technique */
    /* 0x010 */ EftDetVec unk10;
    /* 0x020 */ EftDetVec pos;
    /* 0x030 */ EftDetVec prevPos;
    /* 0x040 */ EftDetVec vel;
    /* 0x050 */ s32 flags;
    /* 0x054 */ s32 unk54[3];
    /* 0x060 */ void *task;
    /* 0x064 */ EftDetSrc *src; /* technique */
    /* 0x068 */ void *atk;      /* ki blast */
    /* 0x06C */ s32 unk6C;
    /* 0x070 */ EftDetShape shape;
} EftDetRec; /* size 0x190 */

typedef struct EftDetList {
    /* 0x0000 */ EftDetRec rec[64];
    /* 0x6400 */ s32 count;
} EftDetList;

/* Context of the stage test of one record: the swept volume ColSweep_FromCapsule builds from a capsule, then ours. */
typedef struct EftDetStageCtx {
    /* 0x00 */ EftDetVec unk0[2];
    /* 0x20 */ f32 radius;
    /* 0x24 */ f32 unk24[3];
    /* 0x30 */ EftDetVec unk30[2];
    /* 0x50 */ EftDetVec unk50;    /* handed to BtlStage_DestroyObj as the place of the hit */
    /* 0x60 */ EftDetVec unk60;
    /* 0x70 */ f32 length;         /* compared with the radius to pick the first "best distance" */
    /* 0x74 */ f32 unk74[3];
    /* 0x80 */ EftDetShape *shape;
    /* 0x84 */ f32 best;           /* nearest contact so far */
    /* 0x88 */ s32 hit;
    /* 0x8C */ s32 unk8C;
} EftDetStageCtx; /* size 0x90 */

/* Node of a collision mesh handed to a query callback (StgColNode of battle/eft_det_b.h). */
typedef struct EftDetNode {
    /* 0x00 */ u8 unk0[0x18];
    /* 0x18 */ s32 poly;
} EftDetNode;

/* Polygon header of a collision mesh (StgColPoly of battle/eft_det_b.h). */
typedef struct EftDetPoly {
    /* 0x00 */ s32 flags;       /* 0x8000000: not solid */
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ EftDetVec nrm;
} EftDetPoly;

/* Result of a zone query (StgColResult of battle/eft_det_b.h). */
typedef struct EftDetResult {
    /* 0x00 */ s32 hit;
    /* 0x04 */ s32 ground;
    /* 0x08 */ s32 rec;
    /* 0x0C */ s32 obj;         /* an unbroken destructible object was touched */
    /* 0x10 */ u64 recMask;
    /* 0x18 */ u64 objMask;     /* bit n: unbroken object n */
} EftDetResult; /* size 0x20 */

/* What the ground probe returns (StgGroundHit of battle/stg_a.h, BtlCollGround of battle/btl_char_coll.h). */
typedef struct StgGroundOut {
    /* 0x00 */ f32 x;           /* the probed point; only y is written */
    /* 0x04 */ f32 y;           /* height of the ground: FLT_MAX when nothing was found */
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
    /* 0x10 */ EftDetVec nrm;   /* plane of the triangle */
    /* 0x20 */ s32 flags;       /* flags of the polygon (ring-out bits 0x80 / 0xC0 read by BtlColl_UpdateGround) */
} StgGroundOut;

/* Context of the ground probe's callback. */
typedef struct StgGroundCtx {
    /* 0x0 */ StgGroundOut *out;
    /* 0x4 */ f32 minY;         /* only ground below this height counts (+Y is down) */
} StgGroundCtx; /* size 0x8 */

/* A body part of a fighter object: the volumes a hit is tested against. */
typedef struct EftDetPart {
    /* 0x000 */ s32 flags;      /* bit 0: last part of the list */
    /* 0x004 */ u8 unk4[0x1C];
    /* 0x020 */ u8 box[0x20];   /* oriented volume tested by ColSphere_SweepSphere / ColSphere_TestSphere (projectiles) */
    /* 0x040 */ u8 vol[0x190];  /* volume tested by ColSphere_ContactObb / ColObb_Overlaps (strikes) */
} EftDetPart; /* size 0x1D0 */

/* Work buffer of a fighter object (BtlCollObjWork of battle/btl_char_coll.h). */
typedef struct EftDetObjWork {
    /* 0x00000 */ u8 unk0[0x18020];
    /* 0x18020 */ s32 hitFlags;         /* bit 0x18 + n: my strike touched object n; bit 0x1C + n: object n's strike
                                           touched me */
    /* 0x18024 */ u8 unk18024[0xC];
    /* 0x18030 */ StgGroundOut ground;  /* ground under the fighter */
    /* 0x18060 */ u8 unk18060[0x20];
    /* 0x18080 */ EftDetBox atkBounds;  /* box around the active attack volumes */
    /* 0x18098 */ u8 unk18098[8];
    /* 0x180A0 */ EftDetSphere atkSphere[19];
    /* 0x18300 */ u8 atkVol[19][0x190];
    /* 0x1A0B0 */ s32 atkSphereCount;
    /* 0x1A0B4 */ s32 atkVolCount;
} EftDetObjWork;

/* What obj + 0xFA0 points to (BtlMoveObjBody of battle/btl_char_move.h). */
typedef struct EftDetBodyPos {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
} EftDetBodyPos;

/* Model node as BtlObj_GetNode returns it (BtlObjPart of battle/btl_obj.h). */
typedef struct EftDetNodeMtx {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ f32 mtx[4][4];   /* row 3 = position */
} EftDetNodeMtx;

/* Fighter battle object (BtlObj of battle/btl_obj.h). */
typedef struct EftDetObj {
    /* 0x0000 */ u8 unk0[0x10];
    /* 0x0010 */ s32 id;
    /* 0x0014 */ u8 unk14[0x950 - 0x14];
    /* 0x0950 */ u8 unk950[0x20];
    /* 0x0970 */ EftDetVec pos;
    /* 0x0980 */ u8 unk980[0xA24 - 0x980];
    /* 0x0A24 */ s32 area;              /* stage zone of the fighter */
    /* 0x0A28 */ u8 unkA28[0xCAD - 0xA28];
    /* 0x0CAD */ s8 unkCAD;             /* < 0: the fighter's strikes are not tested */
    /* 0x0CAE */ u8 unkCAE[0xF50 - 0xCAE];
    /* 0x0F50 */ EftDetPart *parts;     /* start of the body block */
    /* 0x0F54 */ u8 unkF54[0xFA0 - 0xF54];
    /* 0x0FA0 */ EftDetBodyPos *bodyPos;
    /* 0x0FA4 */ u8 unkFA4[0xFC0 - 0xFA4];
    /* 0x0FC0 */ EftDetBox box;         /* tested against the other fighter's attack bounds */
    /* 0x0FD8 */ EftDetBox hitBox;      /* tested against a record's bounds */
    /* 0x0FF0 */ u8 unkFF0[0x1660 - 0xFF0];
    /* 0x1660 */ EftDetObjWork *work;
} EftDetObj;

/* Entry of a "texture set" (see EftTexSet_Load32 in the file before): GS TEX0 value and image. */
typedef struct EftTexSetEntry {
    /* 0x0 */ u64 tex0;
    /* 0x8 */ void *image;
    /* 0xC */ s32 unkC;
} EftTexSetEntry; /* size 0x10 */

typedef struct EftTexSet8 {
    /* 0x00 */ EftTexSetEntry tex[8];
    /* 0x80 */ s32 count;
} EftTexSet8; /* size 0x88 */

typedef struct EftTexSet16 {
    /* 0x000 */ EftTexSetEntry tex[16];
    /* 0x100 */ s32 count;
} EftTexSet16; /* size 0x108 */

typedef struct EftTexSet34 {
    /* 0x000 */ EftTexSetEntry tex[34];
    /* 0x220 */ s32 count;
} EftTexSet34; /* size 0x228 */

/* Image record of a texture pack: 0x40 bytes, the TEX0 value at +0x30. */
typedef struct EftTexPackImage {
    /* 0x00 */ u8 unk0[0x30];
    /* 0x30 */ u64 tex0;
    /* 0x38 */ u8 unk38[8];
} EftTexPackImage; /* size 0x40 */

/* Header of a texture pack once relocated. */
typedef struct EftTexPack {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ EftTexPackImage *images;
} EftTexPack;

/* Texture as the VRAM upload reads it. */
typedef struct EftVramImage {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ s32 imageSize;   /* 0: no image */
    /* 0x0C */ s32 clutSize;    /* 0: no CLUT */
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ u32 imageWidth;  /* buffer width for BITBLTBUF */
    /* 0x1C */ u32 clutWidth;
    /* 0x20 */ u8 unk20[0x18];
    /* 0x38 */ void *image;
    /* 0x3C */ void *clut;
} EftVramImage;

typedef struct EftVramTex {
    /* 0x0 */ u8 unk0[8];
    /* 0x8 */ EftVramImage *img;
} EftVramTex;

/* The effect VRAM allocator (gp 0x2FEAF4; managed by the file before). */
typedef struct EftVram {
    /* 0x000 */ u8 entries[0x800];
    /* 0x800 */ u32 count;
    /* 0x804 */ s32 next;       /* next free block, starts at 0x2B00 */
    /* 0x808 */ s32 unk808;
} EftVram;

/* A shot of a volley (EftVolleyShot of battle/eft_h.h). */
typedef struct EftVolleyAimShot {
    /* 0x00 */ EftDetVec pos;
    /* 0x10 */ EftDetVec dir;
    /* 0x20 */ s32 script;      /* -1 none, 0 / 1: steering script (lobbed shots, aim kinds 7 / 8) */
    /* 0x24 */ f32 speed;       /* distance per frame */
    /* 0x28 */ f32 maxTurn;     /* radians per frame */
} EftVolleyAimShot;

/* One step of a steering script: applies while t0 <= frames since fired <= t1. */
typedef struct EftVolleyAimStep {
    /* 0x0 */ s32 op;           /* EFT_VOLLEY_OP_*, 0 ends the script */
    /* 0x4 */ u8 t0;
    /* 0x5 */ u8 t1;
    /* 0x6 */ u8 v0;
    /* 0x7 */ u8 v1;
} EftVolleyAimStep; /* size 0x8 */

#define EFT_VOLLEY_OP_TURN   1 /* maxTurn = base turn * lerp(v0, v1) / 10 */
#define EFT_VOLLEY_OP_SPEED  2 /* speed = base speed * lerp(v0, v1) / 10 */
#define EFT_VOLLEY_OP_SCATTER 3 /* new random target offset around the opponent (radius v0, chance 1 / (v1 + 1)) */
#define EFT_VOLLEY_OP_BEHIND 4 /* target offset = 1000 beyond the opponent (horizontal) */
#define EFT_VOLLEY_OP_ABOVE  5 /* the same, 400 towards the stage centre and 1000 up */
#define EFT_VOLLEY_OP_FRONT  6 /* v0 != 0: steer only while the target is in front */

void EftTexSet_Load8(EftTexSet8 *set, void *pack);
void EftTexSet_Load16(EftTexSet16 *set, void *pack);
void EftTexSet_Load34(EftTexSet34 *set, void *pack);
s32 EftVram_GetUsed(void);
s32 EftVram_GetCapacity(void);
void EftVram_Upload(EftVramTex *tex, s32 imageBlock, s32 clutBlock);

f32 EftVolleyAim_Lerp(f32 a, f32 b, f32 t);
f32 EftVolleyAim_GetFighterDistSq(void);
void EftVolleyAim_GetToOpponent(s32 objId, EftDetVec *out);
s32 EftVolleyAim_GetNodeSide(s32 node);
void EftVolleyAim_InitShot(s32 objId, EftVolleyAimShot *shot, s32 kind, s32 side, s32 index, s32 count, f32 speed,
                           f32 maxTurn);
s32 EftVolleyAim_Spread(s32 objId, EftDetVec *dir, s32 kind, s32 lockedOn, s32 side, s32 index, s32 count);
void EftVolleyAim_Steer(EftVolleyAimShot *shot, s32 objId, EftDetVec *offset, s32 frontOnly);
void EftVolleyAim_Update(s32 objId, EftVolleyAimShot *shot, EftDetVec *offset, s32 exact, f32 time, f32 speed,
                         f32 maxTurn);

s32 BtlBodyHit_TestVolumes(EftDetObjWork *work, EftDetPart **body);
s32 BtlBodyHit_Test(EftDetObj *atk, EftDetObj *def);
void BtlBodyHit_Update(void);

s32 EftDet_PrepareSpheres(EftDetShape *shape);
s32 EftDet_PrepareCapsule(EftDetShape *shape);
s32 EftDet_PrepareNone(EftDetShape *shape);
void EftDet_PrepareAll(void);
void EftDet_Update(void);
s32 EftDet_SpheresVsFighter(EftDetObj *obj, EftDetShape *shape);
s32 EftDet_CapsuleVsFighter(EftDetObj *obj, EftDetShape *shape);
s32 EftDet_NoneVsFighter(EftDetObj *obj, EftDetShape *shape);
void EftDet_HitFighter(EftDetRec *rec, s32 target, s32 idx);
s32 EftDet_HitFighterMulti(EftDetRec *rec, s32 target, s32 idx);
void EftDet_FighterPass(void);
s32 EftDet_SpheresVsSpheres(EftDetShape *a, EftDetShape *b);
s32 EftDet_CapsuleVsCapsule(EftDetShape *a, EftDetShape *b);
s32 EftDet_CapsuleVsSpheres(EftDetShape *a, EftDetShape *b);
void EftDet_ClashPass(void);
s32 EftDet_StageCb(EftDetNode *node, EftDetStageCtx *ctx);
s32 EftDet_TestStage(EftDetShape *shape, s32 isBlast, s32 objectsOnly);
void EftDet_StagePass(void);

void StgGround_ExtendBoxDown(EftDetBox *box);
s32 StgGround_TestTri(StgGroundOut *out, EftDetTri *tri, s32 flags, f32 minY);
s32 StgGround_Cb(EftDetNode *node, StgGroundCtx *ctx);
void StgGround_Probe(s32 zone, EftDetBox *box, StgGroundOut *out, f32 minY);
void StgGround_UpdateFighter(EftDetObj *obj);

#endif
