#ifndef BATTLE_EFT_AD_H
#define BATTLE_EFT_AD_H

#include "types.h"
#include "sys/math3d.h"

/*
 * src/battle/eft_ad.c   0x1A62C8..0x1A7018  tail of the zap module (effect pack part kind 12) and the fighter
 *                                           shock effect (fighter effect request 6)
 * (src/battle/btl_pool.c 0x1A7018..0x1A7608 lies between the two files)
 * src/battle/eft_ad_b.c 0x1A7608..0x1AA7E8  effect mesh (EftMesh_*), effect model objects (EftObj_*: battle
 *                                           objects made from a model inside an effect pack), the mesh's
 *                                           triangle writers, a lit quad and a rotated sprite
 * All struct names are local views.
 */

/* Four floats on a 16-byte boundary (the effect code's vector type; copied with ld / sd). */
typedef struct EftAdVec {
    f32 x, y, z, w;
} __attribute__((aligned(16))) EftAdVec;

/* The part of a task (0x40 bytes) the callbacks here use (same view as include/battle/eft_g.h). */
typedef struct EftAdTask {
    /* 0x00 */ u8 flags;
    /* 0x01 */ u8 unk1[0x27];
    /* 0x28 */ void **cls;     /* task class; cls[0] is the update callback */
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work;
} EftAdTask;

/* One entry of a texture table: a GS TEX0 value and the image it belongs to. */
typedef struct EftAdTex {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ u64 unk8;
} EftAdTex; /* 0x10 */

/* A GS screen position as the projection helpers write it (x, y in 12.4 fixed point). */
typedef struct EftAdScr {
    s32 x, y, z, w;
} __attribute__((aligned(16))) EftAdScr;

/* A texture set as EftTexSet_Load32 builds it (EftTexSet in eft_g.h). */
typedef struct EftAdTexSet {
    /* 0x000 */ EftAdTex entry[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ s32 unk204;
} EftAdTexSet; /* 0x208 */

/* ---- zap (effect pack part kind 12) ---------------------------------------------------------------------
 * The module starts at 0x1A3B00 (the file before this one); only what the functions from 0x1A62C8 on touch is
 * described. "Zap" is a guess: the object is a chain of pooled points grown from a position along a direction
 * with random steps (0x1A4C98).
 */

/* A point of a chain; 35 of them live in the manager and are handed out round robin. */
typedef struct EftZapNode {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ s32 flags;      /* 0 = free; bit 0 = in use */
    /* 0x24 */ struct EftZapNode *next;
    /* 0x28 */ struct EftZapNode *prev;
    /* 0x2C */ s32 pad2C;
} EftZapNode; /* 0x30 */

#define EFT_ZAP_NODES 35

/* One chain (allocated by the first half of the module; where it lives was not read here). */
typedef struct EftZapLine {
    /* 0x000 */ u8 unk0[0x30];
    /* 0x030 */ Vec4 pos;      /* where the next point goes */
    /* 0x040 */ u8 unk40[0xD8];
    /* 0x118 */ EftZapNode *head;
    /* 0x11C */ EftZapNode *tail;
} EftZapLine;

/* The module's manager block (gEftZapMgr). */
typedef struct EftZapMgr {
    /* 0x0000 */ void *tasks;  /* task list the zaps are created in */
    /* 0x0004 */ u8 unk4[0x17CC];
    /* 0x17D0 */ EftZapNode nodes[EFT_ZAP_NODES];
    /* 0x1E60 */ u8 unk1E60[0x41];
    /* 0x1EA1 */ u8 nodeCur;   /* where the search for a free point starts */
} EftZapMgr;

/* A texture object of an effect pack as this module uses it (0x108 bytes each in the pack's table). */
typedef struct EftZapTexObj {
    /* 0x000 */ EftAdTex entry[16]; /* per image; tex0 is replaced by the full GS TEX0 on first use */
    /* 0x100 */ s32 unk100;
    /* 0x104 */ u32 loaded;      /* bit per image: tex0 is valid */
} EftZapTexObj; /* 0x108 */

/* Parameter block of a zap in the effect pack (first word of the part's resource pair). */
typedef struct EftZapPrm {
    /* 0x00 */ u8 unk0[0xB4];
    /* 0xB4 */ s32 flags;      /* 0x100: stopping also sets work flag 0x20 */
} EftZapPrm;

/* Three keys of every animated value of a zap (second word of the part's resource pair). */
typedef struct EftZapKeys {
    /* 0x000 */ Vec4 a[3];
    /* 0x030 */ Vec4 b[3];
    /* 0x060 */ Vec4 c[3];
    /* 0x090 */ Vec4 d[3];
    /* 0x0C0 */ f32 e[3][2];
    /* 0x0D8 */ f32 f[3][2];
    /* 0x0F0 */ f32 g[3][2];
    /* 0x108 */ f32 h[3];
    /* 0x114 */ f32 i[3];
    /* 0x120 */ f32 j[3];
} EftZapKeys; /* 0x12C */

/* Argument of EftZap_Create (EftArg12 in src/battle/eft_i.c). */
typedef struct EftZapArg {
    /* 0x00 */ EftZapPrm *prm;
    /* 0x04 */ EftZapKeys *keys;
    /* 0x08 */ EftZapTexObj *tex;
    /* 0x0C */ s32 padC;
    /* 0x10 */ Vec4 dir;
    /* 0x20 */ Vec4 pos;
    /* 0x30 */ f32 size;
    /* 0x34 */ f32 rate;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 objId;
    /* 0x40 */ u8 unk40;
    /* 0x41 */ u8 pad41[0xF];
} EftZapArg; /* 0x50 */

/* Work block of a zap task. */
typedef struct EftZap {
    /* 0x000 */ u8 unk0[8];
    /* 0x008 */ EftAdTex texPair[2]; /* the two table entries the zap's TEX0 is built from */
    /* 0x028 */ u8 unk28[8];
    /* 0x030 */ EftZapArg arg;
    /* 0x080 */ u8 unk80[0x10];
    /* 0x090 */ Vec4 curA;     /* key values in use (EftZap_LoadKey) */
    /* 0x0A0 */ Vec4 unkA0;
    /* 0x0B0 */ Vec4 curB;
    /* 0x0C0 */ Vec4 unkC0;
    /* 0x0D0 */ Vec4 curC;
    /* 0x0E0 */ Vec4 unkE0;
    /* 0x0F0 */ Vec4 curD;
    /* 0x100 */ Vec4 unk100;
    /* 0x110 */ f32 curE[2];
    /* 0x118 */ f32 unk118[2];
    /* 0x120 */ f32 curF[2];
    /* 0x128 */ f32 unk128[2];
    /* 0x130 */ f32 curG[2];
    /* 0x138 */ f32 unk138[2];
    /* 0x140 */ f32 curH;
    /* 0x144 */ f32 unk144;
    /* 0x148 */ f32 curI;
    /* 0x14C */ f32 unk14C;
    /* 0x150 */ f32 curJ;
    /* 0x154 */ u8 unk154[0xC];
    /* 0x160 */ f32 delay;     /* frames (set from a part definition byte) */
    /* 0x164 */ f32 fadeDelay; /* frames between stop and the fade; > 0 makes a stop gradual */
    /* 0x168 */ f32 fadeTime;
    /* 0x16C */ f32 fade;      /* > 0 makes a stop gradual */
    /* 0x170 */ f32 unk170;
    /* 0x174 */ f32 rateFrames; /* rate x 30 */
    /* 0x178 */ u8 texIdx;     /* which image of the texture object */
    /* 0x179 */ u8 pad179[3];
    /* 0x17C */ s32 flags;     /* EFT_ZAP_* */
} EftZap;

#define EFT_ZAP_ALIVE      0x00001
#define EFT_ZAP_FADE_WAIT  0x00004 /* stop requested with a fade delay */
#define EFT_ZAP_STOP_NOW   0x00008
#define EFT_ZAP_FADE       0x00010 /* stop requested with a fade time */
#define EFT_ZAP_STOP_20    0x00020
#define EFT_ZAP_KILL       0x00040 /* the update kills the task */
#define EFT_ZAP_FLAG_20000 0x20000 /* part definition flag 0x20 */

void EftZap_LoadKey(EftZap *w, s32 idx);
s32 EftZap_AddNode(EftZapLine *chain);
void EftZap_SetTexPair(EftZap *w, EftAdTex *tbl, s32 a, s32 b);
void EftZap_LoadTex(EftZap *w);
void *EftZap_Create(EftZapArg *arg);
void EftZap_Kill(EftAdTask *task);
void EftZap_Stop(EftAdTask *task);
void EftZap_SetPos(EftAdTask *task, EftAdVec pos);
void EftZap_WarpPos(EftAdTask *task, EftAdVec pos);
void EftZap_SetDir(EftAdTask *task, EftAdVec dir);
void EftZap_SetSize(EftAdTask *task, f32 size);
void EftZap_SetRate(EftAdTask *task, f32 rate);
s32 EftZap_SetTex(EftAdTask *task, EftZapTexObj *tex, s32 a, s32 b);
void EftZap_SetDelay(EftAdTask *task, s32 frames);
void EftZap_SetFadeDelay(EftAdTask *task, s32 frames);
void EftZap_SetFadeTime(EftAdTask *task, s32 frames);
s32 EftZap_SetFlag20000(EftAdTask *task);
s32 EftZap_SetUnk40(EftAdTask *task, s32 value);
s32 EftZap_IsAlive(EftAdTask *task);

/* ---- fighter shock effect (fighter effect request 6) ----------------------------------------------------- */

/* Manager data: one block per fighter, from the fighter's effect pack entry 6. */
typedef struct EftShockRes {
    /* 0x000 */ void *pack;    /* BtlScene_GetCharPackEntry(objId, 6) */
    /* 0x004 */ void *anim;    /* pack entry 1 */
    /* 0x008 */ void *texData; /* pack entry 2 */
    /* 0x00C */ s32 padC;
    /* 0x010 */ EftAdTexSet tex;
} EftShockRes; /* 0x218 */

/* Manager task work. */
typedef struct EftShockMgr {
    /* 0x00 */ EftShockRes *res;
} EftShockMgr;

/* Task work (0x30 bytes). */
typedef struct EftShock {
    /* 0x00 */ Vec4 pos;       /* fighter node 3 */
    /* 0x10 */ s32 objId;
    /* 0x14 */ void *part[2];
    /* 0x1C */ s32 flags;
    /* 0x20 */ s16 count;
    /* 0x22 */ s16 timer;
    /* 0x24 */ u8 pad24[0xC];
} EftShock; /* 0x30 */

#define EFT_SHOCK_DONE   2 /* both parts gone: the update kills the task */
#define EFT_SHOCK_WARPED 4 /* the screen shock wave was spawned */
#define EFT_SHOCK_WAIT   8 /* a part was started; cleared when the timer runs out */

void *EftShock_Start(s32 *arg);

/* ---- effect mesh ------------------------------------------------------------------------------------------ */

/* Mesh data in an effect or stage pack. */
typedef struct EftMeshGroup {
    /* 0x00 */ s32 numTris;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 firstTri;
    /* 0x0C */ s32 unkC;
} EftMeshGroup; /* 0x10 */

typedef struct EftMeshTri {
    /* 0x00 */ s32 v[3];       /* vertex indices */
    /* 0x0C */ s32 tex;        /* texture index, < 0 = untextured */
} EftMeshTri; /* 0x10 */

typedef struct EftMeshVtx {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 color;
    /* 0x20 */ Vec4 uv;
} EftMeshVtx; /* 0x30 */

typedef struct EftMeshData {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 numGroups;
    /* 0x08 */ s32 trisOfs;    /* byte offset of the triangle table */
    /* 0x0C */ s32 vtxOfs;     /* byte offset of the vertex table */
    /* 0x10 */ EftMeshGroup groups[1];
} EftMeshData;

/* An instance of a mesh: what EftMesh_Draw draws. */
typedef struct EftMesh {
    /* 0x00 */ Mtx44 mtx;      /* model to world */
    /* 0x40 */ u8 r, g, b, a;  /* 0x80 = 1.0; multiplies the vertex colours */
    /* 0x44 */ s32 blend;      /* handed to the triangle writer */
    /* 0x48 */ EftAdTex *tex;  /* texture table, NULL = untextured */
    /* 0x4C */ s32 pad4C;
    /* 0x50 */ Vec4 uvOfs;     /* added to every uv (x, y, 1, 0) */
    /* 0x60 */ f32 uvScale;
    /* 0x64 */ s32 texBase;    /* added to a triangle's texture index */
    /* 0x68 */ s32 flags;      /* EFT_MESH_* */
    /* 0x6C */ s32 zOfs;       /* added to the sort depth */
    /* 0x70 */ s32 type;       /* effect type for BtlScene_IsEffectHidden, < 0 = never hidden */
    /* 0x74 */ s32 objId;
    /* 0x78 */ EftMeshData *data;
    /* 0x7C */ EftMeshGroup *groups;
    /* 0x80 */ EftMeshTri *tris;
    /* 0x84 */ EftMeshVtx *verts;
    /* 0x88 */ u8 pad88[8];
} __attribute__((aligned(16))) EftMesh; /* 0x90 */

#define EFT_MESH_REPEAT 1
#define EFT_MESH_FLAG2 2
#define EFT_MESH_ZFLIP 4 /* sort depth = 0x1000 - depth */
#define EFT_MESH_CLIP  8 /* clip every triangle against the view volume */

void EftMesh_Init(EftMesh *mesh, EftMeshData *data);
void EftMesh_SetTex(EftMesh *mesh, EftAdTex *tex);
void EftMesh_SetMtx(EftMesh *mesh, Mtx44 *m);
void EftMesh_Copy(EftMesh *dst, EftMesh *src);
void EftMesh_SetLayer(EftMesh *mesh, s32 blend);
void EftMesh_SetTexBase(EftMesh *mesh, s32 base);
void EftMesh_SetClip(EftMesh *mesh, s32 on);
void EftMesh_SetRepeat(EftMesh *mesh, s32 on);
void EftMesh_SetFlag2(EftMesh *mesh, s32 on);
void EftMesh_SetZFlip(EftMesh *mesh, s32 on);
void EftMesh_SetZOfs(EftMesh *mesh, s32 ofs);
void EftMesh_SetUvOfs(EftMesh *mesh, f32 u, f32 v);
void EftMesh_SetUvScale(EftMesh *mesh, f32 scale);
void EftMesh_SetColor(EftMesh *mesh, u8 r, u8 g, u8 b, u8 a);
void EftMesh_SetOwner(EftMesh *mesh, s32 objId, s32 type);
void EftMesh_Draw(EftMesh *mesh);
void EftMesh_DrawNow(EftMesh *mesh, s32 abe);

/* ---- effect model objects --------------------------------------------------------------------------------- */

/* What EftObj_Create fills and hands to BtlObj_Create: the files part of a BtlResSlot (include/battle/btl_obj.h)
   with only the model set. The caller owns it and must keep it while the object lives. */
typedef struct EftObjRes {
    /* 0x00 */ void *model;    /* file[0].buf: the model inside the effect pack */
    /* 0x04 */ s32 size;       /* file[0].size: always 0x20 */
    /* 0x08 */ u8 unk8[0x28];  /* file[0].id, .orig, file[1], file[2]: zero */
} EftObjRes; /* 0x30 */

s32 EftObj_Create(EftObjRes *res, void *model);
s32 EftObj_Destroy(s32 id);
void EftObj_SetPos(s32 id, Vec4 *pos);
void EftObj_UpdateMtx(s32 id);
void EftObj_PlayAnim(s32 id, s32 anim, s32 mode);
s32 EftObj_IsAnimPlaying(s32 id);
void EftObj_SetRot(s32 id, Vec4 *rot);
void EftObj_SetMtx(s32 id, Mtx44 *m);
void EftObj_SetVisible(s32 id, s32 on);
void EftObj_Nop(void);

#endif
