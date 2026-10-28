#ifndef BATTLE_BTL_OBJ_H
#define BATTLE_BTL_OBJ_H

#include "types.h"
#include "battle/tag_capacity.h"
#include "sys/list.h"
#include "sys/math3d.h"

/*
 * Battle objects: every animated model of a battle (the two fighters and the extra models loaded by
 * BtlLoad_StepObject) is a BtlObj, built from a model resource slot (BtlResSlot: the loaded files).
 * src/battle/btl_obj.c = 0x248F28..0x24BBE8.
 *
 * All of the module's state is one heap block of 0x6B740 bytes (gBtlObjWork, allocated by BtlObj_Init):
 *
 *   0x00000  BtlObjNode objs[12]        0x1680 each: list link + id, then the 0x1670-byte object
 *   0x10E00  List freeObjs              ids not in use
 *   0x10E0C  List usedObjs              objects in creation order; the walk order of every pass
 *   0x10E40  u8 charaWork[2][0x1A0C0]   big per-fighter buffer, given to type 0 objects with id < 3 (the test
 *                                       allows three; a third would overlap the pool that follows)
 *   0x44FC4  f32 unk44FC4               = 2.0 at init; getter / setter only
 *   0x45000  part pool                  512 nodes of 0xE0 + free list   (size 0x1C010)
 *   0x61010  pool                       4 nodes of 0x70 + free list     (size 0x1D0)
 *   0x611E0  pool                       free list at +0xA140; initialised by ObjShadow_InitPool
 *   0x6B330  BtlObjLight light          the one directional light of the battle (0x40)
 *   0x6B370  BtlObjVis vis              per-view draw lists (0x7C)
 *   0x6B3EC  BtlResWork res             model resource slots (0x34C)
 *
 * gBtlObjTbl[12] (static, 0x31C640) maps an object id to its BtlObj; it is rebuilt from the used list by
 * BtlObj_RebuildTable, which Gfx_BeginFrame calls every frame and BtlObj_AllocId calls after each creation.
 * Entries of free ids are NULL.
 */

/* Ten fighters may each spawn two seven-model projectile swarms, besides
   other move props. These are objects, not additional playable fighters. */
#define BTL_OBJ_MAX 192
#define BTL_OBJ_VIEW_MAX 2
#define BTL_OBJ_WORK_SIZE 0x6B740

/* BtlObj.type */
#define BTL_OBJ_TYPE_CHARA 0 /* a fighter: gets a charaWork buffer, is drawn last in its own view */

/* BtlObjState.flags / BtlObjView.flags (the view copy is taken each frame, then edited) */
#define BTL_OBJ_FLAG_VISIBLE 0x2        /* drawn; cleared in a view when the bounds are off screen */
#define BTL_OBJ_FLAG_FADE 0x8           /* view only: distance fade alpha is non-zero */
#define BTL_OBJ_FLAG_COLOR_STAGE 0x10000  /* colour = stage ambient (integer triple from BtlStage_GetAmbient) */
#define BTL_OBJ_FLAG_COLOR_A 0x20000    /* these four all give colour 128,128,128,128 */
#define BTL_OBJ_FLAG_COLOR_B 0x40000    /* also: alpha row = BtlObjFade_Get() * 128 */
#define BTL_OBJ_FLAG_COLOR_BLACK 0x80000 /* colour 0,0,0,128; alpha row = 0xFF */
#define BTL_OBJ_FLAG_PASS1 0x100000     /* drawn in the second pass of the draw list */
#define BTL_OBJ_FLAG_PASS2 0x200000     /* drawn in the third pass; set by BtlObj_UpdateView for a hidden fighter */
#define BTL_OBJ_FLAG_COLOR_C 0x400000   /* also: alpha row = (u8)state.unkDC */
#define BTL_OBJ_FLAG_CHECK_NEAR 0x2000000 /* fighter: test whether the camera is inside the body box */

/* The same flags as bitfields (bit 0 first). */
typedef struct BtlObjFlagBits {
    u32 bit0 : 1;
    u32 visible : 1;    /* BTL_OBJ_FLAG_VISIBLE */
    u32 bit2 : 1;
    u32 fade : 1;       /* BTL_OBJ_FLAG_FADE */
    u32 bits4 : 12;
    u32 colorStage : 1; /* 0x10000 */
    u32 colorA : 1;     /* 0x20000 */
    u32 colorB : 1;     /* 0x40000 */
    u32 black : 1;      /* 0x80000 */
    u32 pass1 : 1;      /* 0x100000 */
    u32 pass2 : 1;      /* 0x200000 */
    u32 colorC : 1;     /* 0x400000 */
    u32 bit23 : 1;
    u32 bit24 : 1;
    u32 checkNear : 1;  /* 0x2000000 */
    u32 bits26 : 6;
} __attribute__((aligned(8))) BtlObjFlagBits; /* 8-byte aligned like the record it starts (BtlObjView holds vectors) */

/* One record of the model's bounds list (BtlObj.bounds): variable length, linked by byte offset. */
typedef struct BtlObjBound {
    /* 0x00 */ s32 next;     /* byte offset to the next record */
    /* 0x04 */ u16 unk04;
    /* 0x06 */ u16 last;     /* non-zero on the last record */
    /* 0x08 */ u16 enabled;  /* 0: record skipped */
    /* 0x0A */ u16 node;     /* argument of BtlObj_GetNode: the model node the box hangs on */
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ Vec4 unk10;
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 unk30;
    /* 0x40 */ Vec4 center;  /* centre - unk10 is transformed by the node matrix */
    /* 0x50 */ Vec4 extent;  /* half size on each axis */
} BtlObjBound;

/* Header of the model file as far as this file reads it (BtlObj.model). */
typedef struct BtlObjModel {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ u32 flags;        /* bit 0: fade with distance */
    /* 0x10 */ f32 fadeNear;     /* 0 = 40.0 */
    /* 0x14 */ f32 fadeFar;      /* 0 = 120.0 */
    /* 0x18 */ u8 unk18[0x10];
    /* 0x28 */ s32 alphaRow;     /* row (15 bytes) of the 0x100-byte table sent by BtlObj_UploadAlphaTable */
    /* 0x2C */ u8 alpha[16];     /* base alpha per material slot, 0..0x7F */
} BtlObjModel;

/* Four floats like Vec4, but 8-byte aligned: the original copies the constant initialisers of locals of this
   type with ld/sd pairs (sys/math3d.h's Vec4 gives ldl/ldr). */
typedef struct BtlObjVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) BtlObjVec; /* size 0x10 */

/* Axis-aligned box as StgAabb_SetEmpty.. keep it: two 12-byte corners (layout not read here). */
typedef struct BtlObjBox {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} BtlObjBox; /* size 0x18 */

/* The same box where it is 8-byte aligned (copied with three ld/sd). */
typedef struct BtlObjBox8 {
    /* 0x00 */ f32 min[3];
    /* 0x0C */ f32 max[3];
} __attribute__((aligned(8))) BtlObjBox8; /* size 0x18 */

/* What one view sees of one object. Filled by BtlObj_UpdateView. */
typedef struct BtlObjView {
    /* 0x00 */ u32 flags;   /* BTL_OBJ_FLAG_*, from BtlObjState.flags */
    /* 0x04 */ u8 unk04[0x1C];
    /* 0x20 */ Vec4 color;  /* light colour, 128 = 1.0 */
    /* 0x30 */ u8 unk30[0x10];
    /* 0x40 */ f32 fade;    /* 0..128 distance fade */
    /* 0x44 */ f32 dist;    /* horizontal distance from the camera to a corner of the bounds */
    /* 0x48 */ u8 unk48[8];
} BtlObjView; /* size 0x50 */

/* Node of a model as returned by BtlObj_GetNode. */
typedef struct BtlObjPart {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ s32 unk08;    /* argument of BtlObj_IsNodeShown */
    /* 0x0C */ u8 active;    /* 1 when BtlObj_IsNodeShown returned 1 this frame */
    /* 0x0D */ u8 unk0D[3];
    /* 0x10 */ Mtx44 mtx;
} BtlObjPart;

/* Model data block of an object (BtlObj + 0x18). */
typedef struct BtlObjMdl {
    /* 0x00 */ u8 unk00[0x28];
    /* 0x28 */ BtlObjModel *model;  /* BtlObj + 0x40 */
    /* 0x2C */ BtlObjBound *bounds; /* BtlObj + 0x44 */
    /* 0x30 */ u8 unk30[0x68];
    /* 0x98 */ void **anims;        /* BtlObj + 0xB0: animation table (DemoCam_PlayObjAnim) */
} BtlObjMdl;

/* Draw state of an object (BtlObj + 0xA40). */
typedef struct BtlObjState {
    /* 0x00 */ u32 flags;      /* BTL_OBJ_FLAG_* */
    /* 0x04 */ u32 viewFlags;  /* OR of views[].flags, by BtlObj_FinishVisibility */
    /* 0x08 */ u8 unk08[8];
    /* 0x10 */ BtlObjView views[BTL_OBJ_VIEW_MAX];
    /* 0xB0 */ BtlObjView *view; /* the record of the view being built */
    /* 0xB4 */ BtlObjBox box;  /* world bounds, rebuilt by BtlObj_UpdateBounds */
    /* 0xCC */ u8 unkCC[0x10];
    /* 0xDC */ f32 unkDC;      /* alpha used with BTL_OBJ_FLAG_COLOR_C */
    /* 0xE0 */ u8 alpha[16];   /* model->alpha[i] + alphaAdd, clamped to 0x7F */
    /* 0xF0 */ u8 alphaAdd;
    /* 0xF1 */ u8 unkF1[3];
} BtlObjState; /* size 0xF4 at least */

/* Body block of a fighter object (BtlObj + 0xF50). */
typedef struct BtlObjBody {
    /* 0x00 */ u8 unk00[0x70];
    /* 0x70 */ BtlObjBox8 box; /* BtlObj + 0xFC0 */
    /* 0x88 */ u8 unk88[0x1C];
    /* 0xA4 */ f32 scale;      /* BtlObj + 0xFF4: body scale (BtlScene divides it by 19.35) */
} BtlObjBody;

/* A battle object. Partial: only the fields this file touches or other modules were seen to use. */
typedef struct BtlObj {
    /* 0x0000 */ s32 type;       /* BTL_OBJ_TYPE_*: first argument of BtlObj_Create */
    /* 0x0004 */ s32 unk04;      /* 0 = skipped by every pass (written outside this file, by BtlObjMdl_Create) */
    /* 0x0008 */ s32 active;     /* third argument of BtlObj_Create; 0 = skipped by every pass */
    /* 0x000C */ s32 chara;      /* (model file id - 0x590) / 10 */
    /* 0x0010 */ s32 id;         /* index in gBtlObjTbl */
    /* 0x0014 */ s32 unk14;
    /* 0x0018 */ BtlObjMdl mdl;
    /* 0x00B4 */ u8 unkB4[0x98C];
    /* 0x0A40 */ BtlObjState state;
    /* 0x0B34 */ u8 unkB34[0x13C];
    /* 0x0C70 */ s32 noAnim;     /* non-zero: the animation / pose passes of BtlObj_UpdateAll are skipped */
    /* 0x0C74 */ u8 unkC74[0x2DC];
    /* 0x0F50 */ BtlObjBody body;
    /* 0x0FF8 */ u8 unkFF8[0x668];
    /* 0x1660 */ u8 *charaWork;  /* BtlObj_GetCharaWork(id) for a fighter */
    /* 0x1664 */ u8 unk1664[0xC];
} BtlObj; /* size 0x1670 */

/* One file of a model resource slot. */
typedef struct BtlResFile {
    /* 0x00 */ void *buf;   /* where the file is (being) loaded; NULL = none */
    /* 0x04 */ s32 size;    /* File_GetSize(id) */
    /* 0x08 */ s32 id;      /* file id, -1 = none */
    /* 0x0C */ void *orig;  /* spare buffers only: the allocation (BtlRes_Term frees this one) */
} BtlResFile; /* size 0x10 */

/* A model resource slot: the files one battle object is built from. BtlObj_Create takes a pointer to it. */
typedef struct BtlResSlot {
    /* 0x00 */ BtlResFile file[3]; /* 0 = model, 1 and 2 = a character's two animation files */
    /* 0x30 */ u32 flags;          /* BTL_RES_SLOT_* */
    /* 0x34 */ s32 number;         /* what BtlRes_Request returns and BtlRes_GetSlot takes */
} BtlResSlot; /* size 0x38 */

#define BTL_RES_SLOT_USED 1
#define BTL_RES_SLOT_HEAP 2 /* the buffers came from the heap (File_Request allocated them) */

#define BTL_RES_SLOT_MAX 12
#define BTL_RES_FIXED_MAX PORT_FIGHTER_CAPACITY
#define BTL_RES_ID_KEEP (-99) /* BtlRes_SetFileId: leave the file as it is */

/* Buffer sizes of a fixed slot. */
#define BTL_RES_MODEL_SIZE 0xCE000
#define BTL_RES_ANIM0_SIZE 0x160800
#define BTL_RES_ANIM1_SIZE 0xCE800

/* The resource part of the work block (gBtlObjWork + 0x6B3EC). */
typedef struct BtlResWork {
    /* 0x000 */ BtlResSlot slots[BTL_RES_SLOT_MAX];  /* heap-loaded models */
    /* 0x2A0 */ BtlResSlot fixed[BTL_RES_FIXED_MAX]; /* the two fighters, preallocated buffers */
    /* 0x310 */ BtlResFile spare[PORT_FIGHTER_CAPACITY + 1];  /* model buffers: [0], [1] start in fixed[0], fixed[1]; [2] is free */
    /* 0x340 */ BtlResFile *next;     /* the model buffer not in use: a character change loads into it */
    /* 0x344 */ s32 pending;          /* fixed slot waiting for BtlRes_CommitReload, -1 = none */
    /* 0x348 */ s32 prealloc;         /* argument of BtlObj_Init: 1 = the fixed slots exist */
} BtlResWork; /* size 0x34C */

/* Pool element: the list link and the id come first, the object itself starts at +0x10. */
typedef struct BtlObjNode {
    /* 0x00 */ ListNode link;
    /* 0x08 */ s32 id;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ BtlObj obj;
} BtlObjNode; /* size 0x1680 */

/* The object table part of the work block. */
typedef struct BtlObjTable {
    /* 0x00000 */ BtlObjNode objs[BTL_OBJ_MAX];
    /* 0x10E00 */ List freeObjs;
    /* 0x10E0C */ List usedObjs;
    /* 0x10E18 */ u8 unk10E18[0x28];
    /* 0x10E40 */ u8 charaWork[PORT_FIGHTER_CAPACITY][0x1A0C0];
    /* 0x44FC0 */ s32 unk44FC0;
    /* 0x44FC4 */ f32 unk44FC4;
    /* 0x44FC8 */ u8 unk44FC8[0x38];
} BtlObjTable; /* size 0x45000 */

/* Element of the 512-entry pool. */
typedef struct BtlObjPoolE0 {
    /* 0x00 */ SListNode link;
    /* 0x04 */ u8 unk04[0x8C];
    /* 0x90 */ Vec4 unk90; /* 0, 0, 0, 1 */
    /* 0xA0 */ Vec4 unkA0; /* 0, 0, 0, 1 */
    /* 0xB0 */ u8 unkB0[0x30];
} BtlObjPoolE0; /* size 0xE0 */

typedef struct BtlObjPoolE0Work {
    /* 0x00000 */ BtlObjPoolE0 nodes[2048];
    /* 0x1C000 */ SList free;
    /* 0x1C00C */ s32 pad;
} BtlObjPoolE0Work; /* size 0x1C010 */

typedef struct BtlObjPool70 {
    /* 0x00 */ SListNode link;
    /* 0x04 */ u8 unk04[0x6C];
} BtlObjPool70; /* size 0x70 */

typedef struct BtlObjPool70Work {
    /* 0x000 */ BtlObjPool70 nodes[BTL_OBJ_MAX];
    /* 0x1C0 */ SList free;
    /* 0x1CC */ s32 pad;
} BtlObjPool70Work; /* size 0x1D0 */

/* Third pool; elements are not touched in this file (ObjShadow_InitPool fills the free list). */
typedef struct BtlObjPool3Work {
    /* Fighter shadows have a separate pool, shared with ObjShadowPool. */
    /* 0x0000 */ u8 nodes[0x2040*TAG_SHADOW_CAPACITY];
    /* 0xA140 */ SList free;
    /* 0xA14C */ s32 pad;
} BtlObjPool3Work; /* size 0xA150 */

/* The battle's directional light. */
typedef struct BtlObjLight {
    /* 0x00 */ Vec4 dir;     /* unit vector */
    /* 0x10 */ Vec4 half;    /* normalize((dir + {0, -1.6, 0, 1}) * 0.5) */
    /* 0x20 */ Vec4 color;   /* 10, 10, 10, 32 at init; 10, 10, 40, 64 or the stage's bytes during a battle */
    /* 0x30 */ void *res;    /* table inside common file 2 (gCommonRes->data[0]), a texture file (TexFile_UploadOne uploads from it) */
    /* 0x34 */ u8 unk34[0xC];
} BtlObjLight; /* size 0x40 */

/* Draw list of one view: pointers in draw order. */
typedef struct BtlObjDrawList {
    /* 0x00 */ BtlObj *objs[BTL_OBJ_MAX];
    /* 0x30 */ s32 count;
} BtlObjDrawList; /* size 0x34 */

/* Per-view visibility results. */
typedef struct BtlObjVis {
    /* 0x00 */ u32 flags;      /* OR of every object's view flags this frame */
    /* 0x04 */ u32 allocBits;  /* a 15-bit allocator (BtlObjVis_AllocBit); claimed at 0x113378 and released at
                                  0x113610, i.e. by the model bind / clear code */
    /* 0x08 */ s32 viewCount;  /* 1, or 2 in split screen */
    /* 0x0C */ void *views[BTL_OBJ_VIEW_MAX]; /* the View (gBtlCamView) each list was built for */
    /* 0x14 */ BtlObjDrawList lists[BTL_OBJ_VIEW_MAX];
} BtlObjVis; /* size 0x7C */

/* The whole work block. */
typedef struct BtlObjWork {
    /* 0x00000 */ BtlObjTable table;
    /* 0x45000 */ BtlObjPoolE0Work poolE0;
    /* 0x61010 */ BtlObjPool70Work pool70;
    /* 0x611E0 */ BtlObjPool3Work pool3;
    /* 0x6B330 */ BtlObjLight light;
    /* 0x6B370 */ BtlObjVis vis;
    /* 0x6B3EC */ BtlResWork res;
    /* 0x6B738 */ u8 pad[8];
} BtlObjWork; /* size 0x6B740 */

extern BtlObjWork *gBtlObjWork;
extern BtlObj *gBtlObjTbl[BTL_OBJ_MAX];

BtlObjView *BtlObj_View(BtlObj *obj, s32 view);
void BtlObj_InitNop(void);
void BtlObj_TermNop(void);
void BtlObj_RebuildTable(void);

BtlObjVis *BtlObjVis_Get(void);
void BtlObjVis_Clear(void);
s32 BtlObjVis_AllocBit(s32 bit);
void BtlObjVis_FreeBit(s32 bit);
s32 BtlObjVis_AllocFreeBit(void);
s32 BtlObjVis_GetViewCount(void);
s32 BtlObjVis_TestFlags(u32 mask);
BtlObjDrawList *BtlObjVis_GetList(s32 view);
void *BtlObjVis_GetView(s32 view);

BtlObjLight *BtlObjLight_Get(void);
void BtlObjLight_Init(void);
void BtlObjLight_SetDir(Vec4 *dir);
void BtlObjLight_GetDir(Vec4 *out);
void BtlObjLight_GetHalf(Vec4 *out);
void BtlObjLight_SetColor(Vec4 *color);
void BtlObjLight_GetColor(Vec4 *out);
void BtlObjLight_FindRes0(void);
void BtlObjLight_FindRes1(s32 key);

BtlObjPoolE0Work *BtlObjPoolE0_Get(void);
void BtlObjPoolE0_Init(void);
BtlObjPoolE0 *BtlObjPoolE0_Alloc(void);
s32 BtlObjPoolE0_FreeList(SList *list);
s32 BtlObjPoolE0_GetFreeCount(void);

BtlObjPool70Work *BtlObjPool70_Get(void);
void BtlObjPool70_Init(void);
BtlObjPool70 *BtlObjPool70_Alloc(void);
s32 BtlObjPool70_Free(BtlObjPool70 *node);
s32 BtlObjPool70_GetFreeCount(void);

BtlObjPool3Work *BtlObjPool3_Get(void);
SListNode *BtlObjPool3_Alloc(void);
s32 BtlObjPool3_Free(SListNode *node);
s32 BtlObjPool3_GetFreeCount(void);

BtlObjTable *BtlObj_GetTable(void);
List *BtlObj_GetUsedList(void);
void BtlObj_InitTable(void);
f32 BtlObj_GetUnk44FC4(void);
void BtlObj_SetUnk44FC4(f32 value);
s32 BtlObj_GetCount(void);
s32 BtlObj_AllocId(void);
s32 BtlObj_FreeId(s32 id);
BtlObj *BtlObj_Get(s32 id);
u8 *BtlObj_GetCharaWork(u32 id);
void BtlObj_Setup(BtlObj *obj, s32 type, BtlResSlot *res, s32 id, s32 active);
s32 BtlObj_Create(s32 type, BtlResSlot *res, s32 active);
s32 BtlObj_Destroy(s32 id);
s32 BtlObj_CreateChara(s32 slot);
void BtlObj_Rebind(s32 id, s32 slot);
BtlResWork *BtlRes_GetWork(void);
s32 BtlObj_RequestCharaModel(s32 side, s32 chara, s32 costume, s32 variant);
BtlObjWork *BtlObj_GetWork(void);
s32 BtlObj_IsInit(void);
void BtlRes_CommitReloadEx(void);
void BtlObj_Init(s32 prealloc);
void BtlObj_Term(void);

void BtlObj_UpdateBounds(BtlObj *obj, BtlObjState *state);
void BtlObj_CalcViewDist(BtlObjState *state, BtlObjView *view);
void BtlObjView_SetFade(BtlObjView *view, f32 nearDist, f32 farDist);
void BtlObj_UpdateAlpha(BtlObj *obj);
void BtlObj_CalcViewColor(BtlObj *obj, BtlObjView *view);
s32 BtlObj_IsOffscreen(BtlObj *obj);
void BtlObj_UpdateView(BtlObj *obj);
void BtlObj_BuildDrawList(BtlObjDrawList *list, s32 view);
void BtlObj_UpdateAll(void);
void BtlObj_UpdateVisibility(s32 view);
void BtlObj_FinishVisibility(s32 split);
void BtlObj_FillAlphaRow(u8 *table, BtlObj *obj);
void BtlObj_InitDraw(void);
void BtlObj_TermDraw(void);
void BtlObj_BeginDraw(void);
void BtlObj_UploadAlphaTable(void);

BtlResSlot *BtlRes_AllocFixed(void);
BtlResSlot *BtlRes_AllocSlot(void);
BtlResSlot *BtlRes_FindSlot(s32 number);
void BtlRes_CopyFile(BtlResFile *dst, BtlResFile *src);
void BtlRes_SwapFile(BtlResFile *a, BtlResFile *b);
void BtlRes_SetFileId(BtlResFile *file, s32 id);
void BtlRes_SetIds(BtlResSlot *slot, s32 model, s32 anim0, s32 anim1);
s32 BtlRes_FreeFile(BtlResFile *file);
void BtlRes_LoadFile(BtlResFile *file, s32 fromHeap);
void BtlRes_CommitReload(void);
void BtlRes_Init(s32 prealloc);
void BtlRes_Term(void);
s32 BtlRes_Request(s32 fixed, s32 model, s32 anim0, s32 anim1);
void BtlRes_Free(s32 number);
BtlResSlot *BtlRes_GetSlot(s32 number);
void BtlRes_Reload(s32 number, s32 model, s32 anim0, s32 anim1);
void BtlRes_CommitReload2(void);
void BtlRes_LoadSingle(s32 fromHeap, BtlResSlot *slot, s32 model);
void BtlRes_FreeSingle(BtlResSlot *slot);

#endif
