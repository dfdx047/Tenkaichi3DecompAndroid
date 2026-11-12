#ifndef SYS_GFXM_D_C_H
#define SYS_GFXM_D_C_H

#include "common.h"
#include "sys/dma.h"
#include "sys/math3d.h"
#include "sys/gfxm_b_b.h"

/*
 * The battle object renderer, 0x1101D0..0x112A30: local views of the object and of its model file.
 * (BtlObj of battle/btl_obj.h is the same object; the model header fields here are not in that header.)
 */

/* Four floats, 8-byte aligned (constant initialisers are copied with ld / sd). */
typedef struct ObjDrawVec {
    f32 x, y, z, w;
} __attribute__((aligned(8))) ObjDrawVec;

/* One vertex of the seam section (0x70 bytes): skinned on the CPU between two nodes. */
typedef struct ObjSeamVtx {
    /* 0x00 */ Vec4 pos;       /* model position; w = weight of the first node */
    /* 0x10 */ f32 normal[3];
    /* 0x1C */ s32 node;       /* node index (argument of BtlObj_GetNode / BtlObj_FindBound) */
    /* 0x20 */ Vec4 uv;        /* texture coordinates of the base texture */
    /* 0x30 */ union {
        u128 q;
        s32 w[4];              /* GS XYZ, 12.4; w[3] bit 15 set = outside the guard band, do not draw */
    } xyz;                     /* written by ObjSeam_TransformVtx */
    /* 0x40 */ u8 unk40[0x10];
    /* 0x50 */ u128 stBase;    /* written: uv * Q */
    /* 0x60 */ u128 stShade;   /* written: (0.5 + 0.5 * N.L, 0, 1) * Q */
} ObjSeamVtx; /* size 0x70 */

/* One triangle of the seam section (0x20 bytes). Only the first triangle's TEX0 values are used. */
typedef struct ObjSeamTri {
    /* 0x00 */ s32 idx[3];
    /* 0x0C */ s32 pad;
    /* 0x10 */ u64 tex0Base;   /* TEX0 of the base texture, block pointers relative to the model's */
    /* 0x18 */ u64 tex0Shade;  /* TEX0 of the shading ramp */
} ObjSeamTri; /* size 0x20 */

/* Seam section of a model file (model + model->seamOfs). Offsets are from the model header. */
typedef struct ObjSeamSec {
    /* 0x00 */ s32 vtxCount;
    /* 0x04 */ s32 triCount;
    /* 0x08 */ s32 vtxOfs;
    /* 0x0C */ s32 triOfs;
} ObjSeamSec;

/* Model file header, the fields the renderer reads. */
typedef struct ObjDrawModel {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ u32 flags;      /* 0x1000000: lit from the camera when the object has BTL_OBJ_FLAG_COLOR_C */
    /* 0x10 */ u8 unk10[0x3C];
    /* 0x4C */ u8 faceTex;     /* texture replaced by BtlObj_GetFaceTexture */
    /* 0x4D */ u8 fadeTex;     /* texture of the fade pass */
    /* 0x4E */ u8 texFirst;    /* textures [texFirst, texFirst + texCount) are uploaded one by one every frame */
    /* 0x4F */ u8 texCount;
    /* 0x50 */ u8 clutFirst;   /* range uploaded to block clutTbp when the object has a colour flag */
    /* 0x51 */ u8 clutCount;
    /* 0x52 */ u8 fadeFirst;   /* range uploaded again by the fade pass */
    /* 0x53 */ u8 fadeCount;
    /* 0x54 */ u8 unk54[4];
    /* 0x58 */ s32 tbp;        /* GS block of the model's textures */
    /* 0x5C */ s32 cbp;        /* GS block of the model's CLUTs (also drawn into as a 64-pixel wide frame) */
    /* 0x60 */ s32 clutTbp;
    /* 0x64 */ s32 fadeTbp;
    /* 0x68 */ s32 fadeCbp;
    /* 0x6C */ u8 unk6C[4];
    /* 0x70 */ s32 seamOfs;    /* 0 = no seam section */
} ObjDrawModel;

/* One record of the model's part list (BtlObjBound of btl_obj.h): variable length. */
typedef struct ObjDrawPart {
    /* 0x00 */ s32 next;       /* byte offset to the next record */
    /* 0x04 */ u16 unk04;
    /* 0x06 */ u16 last;
    /* 0x08 */ u16 enabled;
    /* 0x0A */ u16 node;
    /* 0x0C */ u16 flags;      /* bit 0: drawn by the fade pass */
    /* 0x0E */ u16 unk0E;
    /* 0x10 */ Vec4 ofsA;
    /* 0x20 */ Vec4 ofsB;
    /* 0x30 */ u8 unk30[0x30];
    /* 0x60 */ u8 chain[1];    /* the part's VIF chain */
} ObjDrawPart;

/* A node's pose (BtlObjPart of btl_obj.h). */
typedef struct ObjDrawNode {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ u8 active;
    /* 0x0D */ u8 unk0D[3];
    /* 0x10 */ Mtx44 mtxA;
    /* 0x50 */ Mtx44 mtxB;
} ObjDrawNode;

/* Model block of the object (object + 0x18). */
typedef struct ObjDrawMdl {
    /* 0x000 */ u8 unk00[0x28];
    /* 0x028 */ ObjDrawModel *model;  /* object + 0x40 */
    /* 0x02C */ ObjDrawPart *parts;   /* object + 0x44 */
    /* 0x030 */ TexFile *tex;         /* object + 0x48 */
    /* 0x034 */ u8 unk34[0x10];
    /* 0x044 */ u8 *chains[0x230];    /* object + 0x5C: replacement chains of node 0x30, by BtlObj_GetSubState */
    /* 0x904 */ u8 *param;            /* object + 0x91C: character parameter block; byte 3 is added to the alpha */
    /* 0x908 */ u8 unk908[0x120];
} ObjDrawMdl; /* size 0xA28 */

/* What one view sees of the object (BtlObjView of btl_obj.h). */
typedef struct ObjDrawView {
    /* 0x00 */ s32 flags;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ Vec4 lightDir;  /* written by ObjDraw_SetLight */
    /* 0x20 */ Vec4 color;
    /* 0x30 */ Vec4 unk30;     /* given to VU1 program 2 */
    /* 0x40 */ f32 fade;
    /* 0x44 */ u8 unk44[0xC];
} ObjDrawView; /* size 0x50 */

/* The same flags as bit fields (BtlObjFlagBits of btl_obj.h). */
typedef struct ObjDrawFlagBits {
    u32 bit0 : 1;
    u32 visible : 1;
    u32 bit2 : 1;
    u32 bit3 : 1;
    u32 bit4 : 1;
    u32 bits5 : 11;
    u32 colorStage : 1; /* 0x10000 */
    u32 colorA : 1;     /* 0x20000 */
    u32 rest : 14;
} __attribute__((aligned(8))) ObjDrawFlagBits;

/* Draw state (object + 0xA40). */
typedef struct ObjDrawState {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u8 unk04[0xC];
    /* 0x10 */ ObjDrawView views[2];
    /* 0xB0 */ ObjDrawView *view;
    /* 0xB4 */ u8 unkB4[0x18];
    /* 0xCC */ Vec4 tint;      /* colour (0..255) and alpha blended over the CLUT with BTL_OBJ_FLAG_COLOR_C */
} ObjDrawState;

typedef struct ObjDrawObj {
    /* 0x000 */ u8 unk00[0x18];
    /* 0x018 */ ObjDrawMdl mdl;
    /* 0xA40 */ ObjDrawState state;
} ObjDrawObj;

/* Input block of the VU1 programs 0 / 1 as far as it is filled here (what Vu1Pkt_CallProgN returns). */
typedef struct ObjDrawPkt {
    /* 0x000 */ u8 unk00[0x10];
    /* 0x010 */ Mtx44 mtxA;
    /* 0x050 */ Mtx44 mtxB;
    /* 0x090 */ Vec4 ofsA;
    /* 0x0A0 */ Vec4 ofsB;
    /* 0x0B0 */ Mtx44 light;   /* program 2: a vector, w = 128 */
    /* 0x0F0 */ Mtx44 screen;
    /* 0x130 */ Mtx44 clip;
    /* 0x170 */ Vec4 color;
    /* 0x180 */ Vec4 k;
    /* 0x190 */ u8 unk190[0x20];
    /* 0x1B0 */ s32 tex0[2];
    /* 0x1B8 */ s32 texReg;
    /* 0x1BC */ s32 unk1BC;
} ObjDrawPkt;

/* Work of ObjSeam_Transform, read and written by the VU0 routine ObjSeam_TransformVtx. */
typedef struct ObjSeamWork {
    /* 0x00 */ Mtx44 lightA;   /* out: light * mtxA */
    /* 0x40 */ Mtx44 lightB;   /* out: light * mtxB */
    /* 0x80 */ Mtx44 light;
    /* 0xC0 */ Vec4 k;         /* w = 0.5 */
} ObjSeamWork;

typedef union ObjSeamColor {
    s32 v[4];
    u128 q;
} ObjSeamColor;

void ObjSeam_Transform(ObjDrawObj *obj, ObjDrawView *view);
void ObjSeam_Draw(ObjDrawObj *obj, ObjDrawState *state, ObjDrawView *view);
void ObjDraw_MakeFacingMtx(Mtx44 *m, Vec4 *dir);
void ObjDraw_UploadTextures(ObjDrawObj *obj);
void ObjDraw_AddClutPass(ObjDrawObj *obj, ObjDrawState *state, ObjDrawView *view, s32 dim);
void ObjDraw_DrawParts(ObjDrawObj *obj, ObjDrawView *view, s32 mode, u64 tex0, u32 fbmsk, s32 noMask);
u32 ObjDraw_SetLight(ObjDrawObj *obj, ObjDrawView *view);
void BtlObjDraw_DrawModel(ObjDrawObj *obj);
void BtlObjDraw_DrawModelFade(ObjDrawObj *obj);
void ObjDraw_DrawPartsFlat(ObjDrawObj *obj);
void BtlObjDraw_End(void);
void BtlObjDraw_SetEnv(void);
void ObjGs_AddTexEnv(void);
void ObjGs_AddFrame1(u32 fbmsk, s32 unused);
void ObjGs_AddFrame2Alpha(u32 fbmsk, u64 alpha);
void ObjGs_AddFadeEnv(void);
void ObjGs_AddFadeTex(u64 tex0, s32 aref);
void ObjGs_AddZbufBoth(s32 zbp, s32 zmsk);

#endif
