#ifndef BATTLE_STGM_A_H
#define BATTLE_STGM_A_H

#include "types.h"
#include "sys/math3d.h"
#include "sys/gfxm_b_b.h"

/*
 * Stage model drawing (src/battle/stgm_a.c, 0x115478..0x116B98): the last part of the stage model code that
 * starts at 0x114C60. Everything here is drawing; see the comment at the top of the C file.
 *
 * The structures below are local views of what the stage model loader (0x114C60.., not decompiled yet) builds
 * inside the stage object. The simulation side of the same objects is in battle/stg_a.h (BtlStage, StgObj,
 * StgPart, StgNode); the names here carry an `M` so both headers can be included together.
 */

/* One drawable mesh of the stage model (0x10 bytes). */
typedef struct StgMMesh {
    /* 0x00 */ u16 visIdx;     /* index of its byte in the visibility table (BtlStageM.vis) */
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 prog;        /* 0: drawn with VU1 program 4; anything else is skipped by group 2 */
    /* 0x04 */ u8 alpha;       /* written by the debris fade, read by the blended group (program 7) */
    /* 0x05 */ u8 unk5[3];
    /* 0x08 */ u32 chain;      /* VIF chain of the mesh (never 0) */
    /* 0x0C */ void *mtx;      /* matrix loaded before a blended mesh (the debris node's matrix) */
} StgMMesh; /* size 0x10 */

/* Texture animation of a material: a list of texture numbers stepped once per drawn frame. */
typedef struct StgMTexAnim {
    /* 0x00 */ u16 flags;      /* bit 2: animated; bit 0: loop (otherwise back and forth) */
    /* 0x02 */ u16 count;
    /* 0x04 */ s32 *frames;    /* texture numbers (only the low 16 bits are used) */
} StgMTexAnim;

/* One material: the meshes that share a texture (0x10 bytes). */
typedef struct StgMMat {
    /* 0x00 */ u16 tex;        /* texture number in the stage texture file */
    /* 0x02 */ u16 count;
    /* 0x04 */ StgMMesh *meshes;
    /* 0x08 */ StgMTexAnim *anim; /* NULL: `tex` is used. Read by group 2 only */
    /* 0x0C */ u16 cur;        /* current texture of the animation; bit 15: playing backwards */
    /* 0x0E */ u16 unkE;
} StgMMat; /* size 0x10 */

/* One draw group: a list of materials. */
typedef struct StgMGroup {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u16 count;
    /* 0x04 */ StgMMat *mats;
} StgMGroup; /* size 8 */

/* Indices of BtlStageM.mdl->grp[] in the order StgModel_Draw draws them. */
#define STGM_GRP_BACK 0     /* drawn first, without depth writes (sky / far background) */
#define STGM_GRP_BACK2 1    /* after the scrolling stage effect, depth writes on */
#define STGM_GRP_MAIN 2     /* the stage proper: animated textures, per-mesh program */
#define STGM_GRP_BLEND_A 3  /* three groups drawn with StgModel_SetBlendEnv after a full-screen rectangle of colour 0;
                               A is skipped on stage 5 */
#define STGM_GRP_BLEND_B 4
#define STGM_GRP_BLEND_C 5
#define STGM_GRP_DEBRIS 6   /* program 7 with a per-mesh alpha: fading debris */

typedef struct StgMModel {
    /* 0x00 */ StgMGroup grp[7];
} StgMModel;

/* A culling unit: some meshes with a bounding sphere (StgPart in battle/stg_a.h). */
typedef struct StgMPart {
    /* 0x00 */ u16 count;
    /* 0x02 */ u16 flags;      /* bit 0: culled through the tree */
    /* 0x04 */ StgMMesh **meshes;
    /* 0x08 */ struct StgMNode *node;
    /* 0x0C */ f32 radius;
    /* 0x10 */ f32 x, y, z;
    /* 0x1C */ s32 unk1C;
} StgMPart; /* size 0x20 */

/* Transform node of a part (StgNode in battle/stg_a.h). */
typedef struct StgMNode {
    /* 0x00 */ f32 mtx[12];
    /* 0x30 */ Vec4 pos;       /* row 3 */
    /* 0x40 */ s32 keyCount;
    /* 0x44 */ void *keys;
    /* 0x48 */ s32 endFrame;
    /* 0x4C */ s32 body;       /* < 0: the piece is not shown as debris */
} StgMNode;

/* The parts that are never culled. */
typedef struct StgMPartList {
    /* 0x00 */ u32 count;
    /* 0x04 */ StgMPart *parts;
} StgMPartList;

/* Node of the culling tree: a cube with eight children. */
typedef struct StgMCell {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ struct StgMCell *child[2][2][2];
    /* 0x24 */ s32 vis;        /* result of the last cull: 0 inside, 1 crossing, 2 outside */
    /* 0x28 */ f32 x, y, z;    /* centre */
    /* 0x34 */ s32 depth;      /* 0 for the root; the half size is 2 * root size / (1 << depth) */
    /* 0x38 */ s32 partCount;
    /* 0x3C */ StgMPart **parts;
} StgMCell;

/* Destructible object, the fields the debris fade reads (StgObj in battle/stg_a.h). */
typedef struct StgMObj {
    /* 0x00 */ s32 state;      /* bit 1: its pieces are falling */
    /* 0x04 */ u8 unk4[0x20];
    /* 0x24 */ u32 pieceCount;
    /* 0x28 */ StgMPart **pieces;
    /* 0x2C */ s32 animEnd;
    /* 0x30 */ s32 frame;
    /* 0x34 */ u8 unk34[0x1C];
} StgMObj; /* size 0x50 */

/* Stage object definition, the fields the animated-object pass reads (StgObjDef in battle/stg_a.h). */
typedef struct StgMObjDef {
    /* 0x00 */ s32 hp;
    /* 0x04 */ s32 type;       /* bit 0x40000000: animated even when broken */
    /* 0x08 */ s32 anim;       /* 1-based animation number */
    /* 0x0C */ s32 parent;
    /* 0x10 */ u8 unk10[0xC];
    /* 0x1C */ f32 frame;      /* current animation frame (StgModel_UpdateAnims) */
    /* 0x20 */ Vec4 pos;
    /* 0x30 */ Vec4 rot;       /* degrees */
} StgMObjDef; /* size 0x40 */

typedef struct StgMData {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 objCount;
    /* 0x0C */ StgMObjDef *objDefs;
} StgMData;

/* The stage object (gBtlStage), drawing side. */
typedef struct BtlStageM {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ StgMData *data;
    /* 0x08 */ s32 flags;      /* bit 0: not ready */
    /* 0x0C */ u8 unkC[0x10];
    /* 0x1C */ StgMModel *mdl;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ StgMPartList *always; /* parts drawn whatever the camera sees */
    /* 0x28 */ s32 visSize;
    /* 0x2C */ u8 *vis;        /* one byte per mesh: 1 = draw it this pass */
    /* 0x30 */ u8 unk30[0x14];
    /* 0x44 */ u32 objCount;
    /* 0x48 */ StgMObj *objs;
    /* 0x4C */ f32 size;       /* size of the culling tree's root */
    /* 0x50 */ StgMCell *root;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ TexFile *tex;   /* the stage texture file */
} BtlStageM;

struct StgFrustum;

void StgModel_DrawAnims(void);
void StgModel_CullCell(StgMCell *cell, f32 size, s32 vis, struct StgFrustum *fr);
void StgModel_Cull(void *view);
void StgModel_FadeDebris(void);
void StgModel_SetTexture(s32 index);
s32 StgModel_DrawDebris(s32 drawn, u8 *vis);
void StgModel_Draw(void *view);
void StgModel_SetDrawEnv(void);
void StgModel_SetBlendEnv(void);
void StgModel_SetTestOpaque(void);
void StgModel_SetTestDebris(void);
void StgModel_SetTestDefault(void);
void StgModel_SetAnimDrawEnv(void);

#endif
