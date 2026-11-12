#ifndef SYS_GFXM_E_H
#define SYS_GFXM_E_H

#include "types.h"
#include "sys/list.h"
#include "sys/math3d.h"
#include "sys/gfxm_b_b.h"

/*
 * 0x112A30..0x115170, four source files (the stem is a placeholder; suggested names in brackets):
 *
 *   gfxm_e.c    0x112A30..0x1131D8  ObjGs_*: GS environment packets of the battle object renderer
 *                                   [battle/btl_obj_gs.c; probably the tail of the object draw file before it]
 *   gfxm_e_b.c  0x1131D8..0x114860  BtlObjMdl_*: binding a battle object to its model / texture files
 *                                   ObjShadow_*: the ground shadow of a battle object
 *                                   [battle/btl_obj_mdl.c + battle/btl_obj_shadow.c]
 *   gfxm_e_c.c  0x114860..0x114B18  MdlTex_RebaseChain: texture block pointers inside a model's VIF streams
 *                                   [battle/mdl_tex.c]
 *   gfxm_e_d.c  0x114B18..0x115170  relocation of the stage file (offsets in words -> pointers)
 *                                   [battle/stg_reloc.c, or the head of the stage model file stg_d_b.c]
 *
 * Evidence for the first boundary: ObjShadow_BeginRender only matches when the compiler has NOT seen the body
 * of ObjGs_AddDefaultEnv (beqz instead of beqzl otherwise). The other boundaries are by subject only.
 *
 * The structures below are local views: only what this code reads. The battle object is BtlObj of
 * battle/btl_obj.h, the model header its BtlObjModel, a mesh record its BtlObjBound.
 */

/* Header of a model file (file 0 of the resource slot). */
typedef struct ObjMdlFile {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ u32 flags : 31;
               u32 bound : 1;  /* bound once already (texture pointers rebased, alpha row claimed) */
    /* 0x10 */ u8 unk10[0x18];
    /* 0x28 */ s32 alphaRow;   /* bit claimed from BtlObjVis: row of the per-object alpha table */
    /* 0x2C */ u8 alpha[16];   /* base alpha per material slot */
    /* 0x3C */ u8 unk3C[0x12];
    /* 0x4E */ u8 texFirst;    /* first texture of the file that belongs to a material slot */
    /* 0x4F */ u8 texCount;    /* how many do */
    /* 0x50 */ u8 unk50[4];
    /* 0x54 */ u8 texNoAlpha;  /* index of the texture whose slot alpha stays 0; 0 = none */
    /* 0x55 */ u8 unk55[3];
    /* 0x58 */ s32 tbp;        /* GS block of the model's textures (0x3480) */
    /* 0x5C */ s32 cbp;        /* GS block of their CLUTs (0x3C00) */
    /* 0x60 */ s32 cbp2;       /* 0x3D00 */
    /* 0x64 */ s32 tbp2;       /* 0x3D40 */
    /* 0x68 */ s32 unk68;      /* 0x3E40 */
    /* 0x6C */ s32 meshOfs;    /* byte offset of the mesh list */
} ObjMdlFile;

/* One record of the mesh list (BtlObjBound): variable length, the VIF stream starts at +0x60. */
typedef struct ObjMdlMesh {
    /* 0x00 */ s32 next;       /* byte offset to the next record */
    /* 0x04 */ u16 unk04;
    /* 0x06 */ u16 last;       /* non-zero on the last record */
    /* 0x08 */ u16 enabled;
    /* 0x0A */ u16 node;       /* model node the mesh hangs on */
    /* 0x0C */ u8 unk0C[0x54];
    /* 0x60 */ u32 vif[1];     /* ends with a 0x70000000 word */
} ObjMdlMesh;

/* Model block of a battle object (BtlObj + 0x18, BtlObjMdl of btl_obj.h). */
typedef struct ObjMdl {
    /* 0x00 */ ObjMdlFile *file;   /* the model file as loaded */
    /* 0x04 */ TexFile *texFile;   /* the texture file as loaded */
    /* 0x08 */ u8 unk08[0x20];
    /* 0x28 */ ObjMdlFile *model;  /* = file */
    /* 0x2C */ ObjMdlMesh *meshes; /* file + file->meshOfs */
    /* 0x30 */ TexFile *tex;       /* texFile after Res_RelocateOffsets */
    /* 0x34 */ u8 unk34[0x10];
    /* 0x44 */ u32 *extra[8];      /* extra VIF streams with texture references, NULL = none */
} ObjMdl;

/* Element of the 512-entry part pool (BtlObjPoolE0) as this file fills it. */
typedef struct ObjMdlPart {
    /* 0x00 */ SListNode link;
    /* 0x04 */ s32 unk04;
    /* 0x08 */ s32 node;
    /* 0x0C */ u8 active;
} ObjMdlPart;

/* Axis-aligned box (ColBox of battle/col_a.h). */
typedef struct ColBoxE {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} ColBoxE; /* size 0x18 */

/* Placement block of a battle object (BtlObj + 0x950). */
typedef struct ObjShadowXf {
    /* 0x00 */ u8 unk00[0x20];
    /* 0x20 */ Vec4 pos;       /* BtlObj + 0x970 */
    /* 0x30 */ u8 unk30[0xA4];
    /* 0xD4 */ s32 zone;       /* BtlObj + 0xA24: stage zone, first argument of StgShadow_Collect */
    /* 0xD8 */ u8 unkD8[0x18];
} ObjShadowXf; /* size 0xF0 */

/* BtlObjView (btl_obj.h) as this file writes it. */
typedef struct ObjShadowView {
    /* 0x00 */ u8 unk00[0x30];
    /* 0x30 */ Vec4 color;
} ObjShadowView;

struct ObjShadow;

/* The battle object (BtlObj, 0x1670 bytes) as this file reads it. */
typedef struct ObjMdlObj {
    /* 0x0000 */ s32 type;
    /* 0x0004 */ s32 ready;          /* 1 once BtlObjMdl_Init has run; BtlObjMdl_Destroy does nothing when 0 */
    /* 0x0008 */ u8 unk08[0xC];
    /* 0x0014 */ void *res;          /* the BtlResSlot it was created from */
    /* 0x0018 */ ObjMdl mdl;
    /* 0x007C */ u8 unk7C[0x8D4];
    /* 0x0950 */ ObjShadowXf xf;
    /* 0x0A40 */ u32 flags;          /* BtlObjState.flags: bit 28 = has a ground shadow, bit 29 = flat shadow */
    /* 0x0A44 */ u8 unkA44[0xAC];
    /* 0x0AF0 */ ObjShadowView *view;
    /* 0x0AF4 */ ColBoxE box;        /* world bounds */
    /* 0x0B0C */ u8 unkB0C[0x254];
    /* 0x0D60 */ struct {
        /* 0x000 */ SList list;             /* ObjMdlPart, one per mesh record */
        /* 0x00C */ ObjMdlPart *first[162]; /* first part of each model node (the array's real size is not known) */
    } parts;
    /* 0x0FF4 */ f32 scale;          /* body scale */
    /* 0x0FF8 */ u8 unkFF8[0x670];
    /* 0x1668 */ struct ObjShadow *shadow;
    /* 0x166C */ s32 unk166C;
} ObjMdlObj; /* size 0x1670 */

/* Double-buffered state of one object's shadow (0xC0 bytes from the heap). */
typedef struct ObjShadowWork {
    /* 0x00 */ u32 *pkt[2];   /* VU1 packets, ObjShadow_CalcPacketWords(0x90) words each */
    /* 0x08 */ f32 size;      /* half width of the area the shadow texture covers */
    /* 0x0C */ s32 flip;      /* which packet this frame builds */
    /* 0x10 */ Vec4 target;   /* the object's position */
    /* 0x20 */ Vec4 eye;      /* 1000 above it */
    /* 0x30 */ Vec4 scale;    /* x = texture scale for VU1 program 6 */
    /* 0x40 */ Mtx44 view;    /* world to the shadow camera */
    /* 0x80 */ Mtx44 proj;    /* world to the shadow texture */
} ObjShadowWork; /* size 0xC0 */

/* Triangle as StgShadow_Collect writes it (StgColTri). */
typedef struct ObjShadowTri {
    /* 0x00 */ Vec4 v[3];
    /* 0x30 */ Vec4 normal;
} ObjShadowTri; /* size 0x40 */

#define OBJ_SHADOW_TRI_MAX 0x80
#define OBJ_SHADOW_PKT_TRIS 0x90
#define OBJ_SHADOW_MAX 5

/* Element of the third pool of the battle object work block (BtlObjPool3Work.nodes). */
typedef struct ObjShadow {
    /* 0x0000 */ SListNode link;
    /* 0x0004 */ u8 unk04[0xC];
    /* 0x0010 */ ObjShadowTri tris[OBJ_SHADOW_TRI_MAX];
    /* 0x2010 */ u32 count;       /* triangles under the object this frame */
    /* 0x2014 */ s32 unk2014;
    /* 0x2018 */ f32 alpha;       /* StgShadowOut.alpha: fades with the height above the ground */
    /* 0x201C */ s32 unk201C;
    /* 0x2020 */ Vec4 light;      /* direction the shadow is thrown in */
    /* 0x2030 */ ObjShadowWork *work;
    /* 0x2034 */ u8 unk2034[0xC];
} ObjShadow; /* size 0x2040 */

typedef struct ObjShadowPool {
    /* 0x0000 */ ObjShadow nodes[OBJ_SHADOW_MAX];
    /* 0xA140 */ SList free;
    /* 0xA14C */ s32 pad;
} ObjShadowPool; /* size 0xA150 */

/* Node of the stage file's octree. */
typedef struct StgOctNode {
    /* 0x00 */ s32 *unk00;                    /* relocated when not 0 */
    /* 0x04 */ struct StgOctNode *child[2][2][2];
    /* 0x24 */ u8 unk24[0x14];
    /* 0x38 */ s32 count;
    /* 0x3C */ s32 **list;                    /* count entries; 0 and -1 both become NULL */
} StgOctNode; /* size 0x40 */

/* Tables of the stage header, as far as the relocation shows them. Names are placeholders. */
typedef struct StgRelocTimer {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ void *ptr;
    /* 0x08 */ s32 unk08[2];
} StgRelocTimer; /* size 0x10 (StgTimer of battle/stg_b.h) */

typedef struct StgRelocA2 {
    /* 0x00 */ s32 unk00[2];
    /* 0x08 */ void *ptr8;
    /* 0x0C */ void *ptrC;
} StgRelocA2; /* size 0x10 */

typedef struct StgRelocA1 {
    /* 0x00 */ u16 unk00;
    /* 0x02 */ u16 count;
    /* 0x04 */ StgRelocA2 *items;
    /* 0x08 */ void *opt;        /* 0 = none */
    /* 0x0C */ s32 unk0C;
} StgRelocA1; /* size 0x10 */

typedef struct StgRelocA {
    /* 0x00 */ u16 unk00;
    /* 0x02 */ u16 count;
    /* 0x04 */ StgRelocA1 *items;
} StgRelocA; /* size 8 */

typedef struct StgRelocB1 {
    /* 0x00 */ u16 count;
    /* 0x02 */ u16 unk02;
    /* 0x04 */ void **list;
    /* 0x08 */ void *opt;        /* 0 = none */
    /* 0x0C */ u8 unk0C[0x14];
} StgRelocB1; /* size 0x20 */

typedef struct StgRelocB {
    /* 0x00 */ u32 count;
    /* 0x04 */ StgRelocB1 *items;
} StgRelocB; /* size 8 */

typedef struct StgRelocC {
    /* 0x00 */ u8 unk00[0x44];
    /* 0x44 */ void *ptr44;
    /* 0x48 */ u8 unk48[8];
} StgRelocC; /* size 0x50 */

typedef struct StgRelocD1 {
    /* 0x00 */ s32 unk00[2];
    /* 0x08 */ void *ptr8;
    /* 0x0C */ s32 unk0C;
} StgRelocD1; /* size 0x10 */

typedef struct StgRelocD {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ void *ptr04;
    /* 0x08 */ s32 unk08;
    /* 0x0C */ void *ptr0C;
    /* 0x10 */ void *ptr10;
    /* 0x14 */ s32 count;
    /* 0x18 */ StgRelocD1 *items;
    /* 0x1C */ u8 unk1C[0x24];
} StgRelocD; /* size 0x40 */

typedef struct StgRelocE {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ void *ptr0C[2];
    /* 0x14 */ void *ptr14[2];
    /* 0x1C */ void *ptr1C[2];
    /* 0x24 */ u32 count;
    /* 0x28 */ void **list;
    /* 0x2C */ u8 unk2C[0x24];
} StgRelocE; /* size 0x50 */

/* The stage header (what gBtlStage points at; BtlStageMgr of battle/stg_b.h) as BtlStage_Relocate sees it. */
typedef struct StgRelocHdr {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ u32 timerCount;
    /* 0x14 */ StgRelocTimer *timers;
    /* 0x18 */ u32 aCount;
    /* 0x1C */ StgRelocA *a;
    /* 0x20 */ u32 bCount;
    /* 0x24 */ StgRelocB *b;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ void *unk2C;
    /* 0x30 */ u32 cCount;
    /* 0x34 */ StgRelocC *c;
    /* 0x38 */ u32 dCount;
    /* 0x3C */ StgRelocD *d;
    /* 0x40 */ void *unk40;
    /* 0x44 */ u32 eCount;
    /* 0x48 */ StgRelocE *e;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ StgOctNode *tree;
    /* 0x54 */ s32 texOfs;       /* texture file, in words from `base` */
    /* 0x58 */ TexFile *tex;     /* set by Res_RelocateOffsets */
    /* 0x5C */ s32 *base;        /* the argument of BtlStage_Relocate */
} StgRelocHdr;

void ObjGs_AddFrameMaskOpaque(u32 mask);
void ObjGs_AddFrameMaskBlend(u32 mask);
void ObjGs_AddFlatShadowEnv(void);
void ObjGs_AddShadowTexEnv(s32 tbp, s32 tbw);
void ObjGs_AddDefaultEnv(void);

s32 ObjShadow_CalcPacketWords(s32 count);
s32 ObjShadow_BuildPacket(u32 *pkt, s32 count, ObjShadowTri *tris, f32 *color);
void ObjShadow_MakePlaneMtx(Mtx44 *out, Vec4 *plane, Vec4 *light, f32 bias);
f32 ObjShadow_CalcScreenDist(Vec4 *a, Vec4 *b, f32 size, f32 half);
void ObjShadow_AddTargetEnv(void);
void ObjShadow_ResetEnv(void);
void ObjShadow_Alloc(ObjMdlObj *obj);
void ObjShadow_Free(ObjMdlObj *obj);
void ObjShadow_InitPool(void);
void ObjShadow_TermPool(void);

void MdlTex_RebaseChain(s32 single, void *chain, s32 tbp, s32 cbp, s32 tbp2, s32 cbp2, u32 minTbp, u32 minCbp);

void StgOctree_Relocate(StgOctNode *node, s32 *base);
void BtlStage_Relocate(s32 *base);

#endif
