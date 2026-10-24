/*
 * Battle objects (0x248F28..0x24BBE8): the table of animated models of a battle, their per-frame update, the
 * battle's light, the per-view draw lists, and the model resource slots the objects are built from.
 * Layouts and an overview of the work block are in include/battle/btl_obj.h.
 *
 * Frame order (Battle_Loop / Battle_Update):
 *   Gfx_BeginFrame          BtlObj_RebuildTable()
 *   Battle_Update           BtlObj_UpdateAll(), then per view BtlObj_UpdateVisibility(view), then
 *                           BtlObj_FinishVisibility(split)
 *
 * Objects and resource slots:
 *   BtlRes_Request(1, model, anim0, anim1) takes one of the two fixed slots (numbers 0 and 1, buffers allocated
 *   once by BtlRes_Init) and BtlRes_Request(0, ...) one of the twelve heap slots (numbers 2..13 when the fixed
 *   slots exist, else 0..11); both queue the files with File_Request and return the slot number, which is what
 *   BattleSide + 0x26C holds. Once the reads are done, BtlObj_Create(type, BtlRes_GetSlot(number), 1) takes a
 *   free object, binds it to the slot's files and returns the object id (BattleSide + 0x268).
 *   A character change keeps both numbers: BtlRes_Reload reads the new model into the spare buffer,
 *   BtlRes_CommitReload swaps it into the slot, BtlObj_Rebind sets the same object up again.
 *
 * The original was several source files (one per part of the work block, each with its own "get" function):
 * BtlObj_UpdateVisibility only matches when BtlObjVis_GetList is not visible to the compiler. They are kept in
 * one file here because their constants are contiguous (.rodata 0x2F23F0..0x2F2420, .lit4 0x2FE630..0x2FE640).
 */
#include "common.h"
#include "battle/btl_obj.h"
#include "battle/battle.h"
#include "battle/btl_cam.h"
#include "sys/common.h"
#include "sys/dma.h"
#include "sys/file.h"
#include "sys/heap.h"

/* The legacy data symbol has only twelve entries. Keep the native table sized
   with the object pool instead of overwriting the following globals. */
static BtlObj *nativeObjectTable[BTL_OBJ_MAX];

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern s32 BtlStage_IsReady(void);

/* Uploads texture `index` of a texture file to texture block tbp, its CLUT to cbp (-1: none). Returns nothing
   (src/sys/gfxm_b_b.c); the older reading of this as a table lookup was wrong. */
extern void TexFile_UploadOne(void *file, s32 index, s32 tbp, s32 cbp);
/* Model object setup / teardown (0x113598 binds the resource, 0x1135F0 clears the object). */
extern void BtlObjMdl_Destroy(BtlObj *obj);
extern void BtlObjMdl_Create(s32 type, BtlObj *obj, BtlResSlot *res);
extern void ObjShadow_Update(BtlObj *obj);
extern void ObjShadow_InitPool(void);
extern void ObjShadow_TermPool(void);
/* Matrix helpers: 0x120230 copies the rotation rows, 0x1202A0 inverts a view matrix, 0x121388 gets the
 * current world-to-screen matrix, 0x122310 projects a point to integer screen coordinates. */
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Vu0Screen_StoreMtx(Mtx44 *dst);
extern void Mtx_ProjectInt(s32 *out, Mtx44 *m, Vec4 *v);
/* Box helpers: reset, set from centre + half size, add a point, get centre, get half size, contains point. */
extern void StgAabb_SetEmpty(BtlObjBox *box);
extern void ColBox_SetCenterHalf(BtlObjBox *box, Vec4 *center, Vec4 *extent);
extern void ColBox_AddPoint(BtlObjBox *box, Vec4 *point);
extern void ColBox_GetCenter(BtlObjBox *box, Vec4 *center);
extern void ColBox_GetHalf(BtlObjBox *box, Vec4 *extent);
extern s32 ColBox_ContainsPoint(BtlObjBox *box, Vec4 *point);
/* Stage: light direction, light record (bytes 8..11 = colour), ambient colour as integers. */
extern void BtlStage_GetLightVecB(Vec4 *dir);
extern u8 *BtlStage_GetLightColors(void);
extern void BtlStage_GetAmbient(s32 *rgb);
/* Object passes of the following files. */
extern void BtlObjAnim_SamplePose(BtlObj *obj);
extern void BtlObjAnim_UpdateEvents(BtlObj *obj);
extern void BtlObjAnim_Step(BtlObj *obj);
extern void BtlObjPose_CalcMatrices(BtlObj *obj);
extern void BtlObjFlash_Reset(BtlObj *obj);
extern void BtlObjFlash_Step(BtlObj *obj);
extern void BtlObjFade_Step(BtlObj *obj);
extern void BtlObjFade_Get(BtlObj *obj, Vec4 *out);
extern void BtlObj_UpdateFace(BtlObj *obj);
extern BtlObjPart *BtlObj_GetNode(BtlObj *obj, s32 node);
extern s32 BtlObj_IsNodeShown(BtlObj *obj, s32 arg);
extern void BtlObj_SaveNodePositions(BtlObj *obj, s32 arg);
extern void BtlObj_InitChains(BtlObj *obj);
extern void BtlObj_UpdateChains(BtlObj *obj);
/* Drawing modules. */
extern void ObjOutline_Init(void);
extern void ObjOutline_Term(void);
extern void ObjOutline_Draw(void);
extern void ObjGlow_Init(void);
extern void ObjGlow_Term(void);
extern void ObjGlow_SetAlphaTable(u8 *table);
extern void ObjGlow_Draw(void);
extern void GfxPost_CopyAlphaToDepth(void);
extern void GfxAlphaKey_Init(void);
extern void GfxAlphaKey_Term(void);
extern void GfxAlphaKey_Draw(void);

extern f32 gBtlObjFarDist[];

/* File_Request is called with a third argument (the buffer size), which it ignores. */
#define File_Request3(id, buf, size) ((void *(*)(s32, void *, s32))File_Request)(id, buf, size)

/* BtlObj_UpdateVisibility only matches when the compiler has not seen the body of BtlObjVis_GetList (it changes
   a branch-likely choice): the original had a file boundary somewhere between the two. */
extern BtlObjDrawList *BtlObjVis_GetListExt(s32 view) __asm__("BtlObjVis_GetList");

/* The flag word as bitfields: tests of two or three of these bits compile to one 64-bit load in the original,
   which is what ee-gcc does when it merges adjacent bitfield tests. */
#define FLAG_BITS(p) ((BtlObjFlagBits *)&(p)->flags)

/* Empty; called by BtlObj_Init. */
void BtlObj_InitNop(void) {
}

/* Empty; called by BtlObj_Term. */
void BtlObj_TermNop(void) {
}

/* Rebuilds the id -> object table from the list of objects in use. */
void BtlObj_RebuildTable(void) {
    BtlObjNode *node;

    if (gBtlObjWork != NULL) {
        memset(nativeObjectTable, 0, sizeof(nativeObjectTable));
        for (node = (BtlObjNode *)List_GetHead(BtlObj_GetUsedList()); node != NULL;
             node = (BtlObjNode *)List_GetNext(&node->link)) {
            nativeObjectTable[node->id] = &node->obj;
        }
    }
}

/* Additional view records are owned separately: model binary layouts stay intact. */
static BtlObjView extraObjectViews[BTL_OBJ_MAX][2];
static void *extraView[2];
static BtlObjDrawList extraLists[2];
BtlObjView *BtlObj_View(BtlObj *obj, s32 view) {
    return view < 2 ? &obj->state.views[view] : &extraObjectViews[obj->id][view - 2];
}

/* Returns the per-view visibility block. */
BtlObjVis *BtlObjVis_Get(void) {
    return &gBtlObjWork->vis;
}

/* Zeroes the visibility block. */
void BtlObjVis_Clear(void) {
    memset(BtlObjVis_Get(), 0, sizeof(BtlObjVis));
    memset(extraObjectViews, 0, sizeof(extraObjectViews));
    memset(extraLists, 0, sizeof(extraLists));
}

/* Claims one bit of the allocation mask; returns 0 if it was already taken. */
s32 BtlObjVis_AllocBit(s32 bit) {
    BtlObjVis *vis = BtlObjVis_Get();
    u32 mask = 1 << bit;

    if (!(vis->allocBits & mask)) {
        vis->allocBits |= mask;
        return 1;
    }
    return 0;
}

/* Releases one bit of the allocation mask. */
void BtlObjVis_FreeBit(s32 bit) {
    BtlObjVis *vis = BtlObjVis_Get();

    vis->allocBits &= ~(1 << bit);
}

/* Claims the first free bit and returns its number. */
s32 BtlObjVis_AllocFreeBit(void) {
    s32 i;

    for (i = 0; i < 15; i++) {
        if (BtlObjVis_AllocBit(i)) {
            return i;
        }
    }
    return 0;
}

/* Number of views of the last BtlObj_FinishVisibility. */
s32 BtlObjVis_GetViewCount(void) {
    return gBtlObjWork->vis.viewCount;
}

/* Whether any object had one of the flags in any view this frame. */
s32 BtlObjVis_TestFlags(u32 mask) {
    return (gBtlObjWork->vis.flags & mask) != 0;
}

/* Returns the draw list of a view. */
BtlObjDrawList *BtlObjVis_GetList(s32 view) {
    return view < 2 ? &gBtlObjWork->vis.lists[view] : &extraLists[view - 2];
}

/* Returns the View a draw list was built for. */
void *BtlObjVis_GetView(s32 view) {
    return view < 2 ? gBtlObjWork->vis.views[view] : extraView[view - 2];
}

/* Returns the light block. */
BtlObjLight *BtlObjLight_Get(void) {
    return &gBtlObjWork->light;
}

/* Sets the default light and finds the light's table in common file 2. */
void BtlObjLight_Init(void) {
    Vec4 dir;
    Vec4 color;
    BtlObjLight *light = BtlObjLight_Get();
    CommonRes *res = gCommonRes;

    memset(light, 0, sizeof(BtlObjLight));
    light->res = (u8 *)res->data[0] + ((((u32 *)res->data[0])[1] >> 2) << 2);
    Res_RelocateOffsets(&light->res, light->res, light->res);
    Vec4_Set(&dir, 0.34f, -0.006f, 0.941f, 1.0f);
    BtlObjLight_SetDir(&dir);
    Vec4_Set(&color, 10.0f, 10.0f, 10.0f, 32.0f);
    BtlObjLight_SetColor(&color);
}

/* Sets the light direction and derives the half vector from it. */
void BtlObjLight_SetDir(Vec4 *dir) {
    BtlObjLight *light = BtlObjLight_Get();
    BtlObjVec eye = { 0.0f, -1.6f, 0.0f, 1.0f };

    Vec4_Copy(&light->dir, dir);
    Vec3_Normalize(&light->dir, &light->dir);
    Vec4_Copy(&light->half, &light->dir);
    Vec4_Add(&light->half, &light->half, (Vec4 *)&eye);
    Vec3_Scale(&light->half, &light->half, 0.5f);
    Vec3_Normalize(&light->half, &light->half);
}

/* Copies the light direction out. */
void BtlObjLight_GetDir(Vec4 *out) {
    Vec4_Copy(out, &BtlObjLight_Get()->dir);
}

/* Copies the half vector out. */
void BtlObjLight_GetHalf(Vec4 *out) {
    Vec4_Copy(out, &BtlObjLight_Get()->half);
}

/* Sets the light colour. */
void BtlObjLight_SetColor(Vec4 *color) {
    Vec4_Copy(&BtlObjLight_Get()->color, color);
}

/* Copies the light colour out. */
void BtlObjLight_GetColor(Vec4 *out) {
    Vec4_Copy(out, &BtlObjLight_Get()->color);
}

/* Uploads texture 0 of the light's texture file to block 0x3D40. (The name predates the texture-file module: it
   uploads, it does not look anything up, and returns nothing.) */
void BtlObjLight_FindRes0(void) {
    TexFile_UploadOne(BtlObjLight_Get()->res, 0, 0x3D40, -1);
}

/* Uploads texture 1 of the light's texture file to block `key`. */
void BtlObjLight_FindRes1(s32 key) {
    TexFile_UploadOne(BtlObjLight_Get()->res, 1, key, -1);
}

/* Returns the 0xE0-byte node pool. */
BtlObjPoolE0Work *BtlObjPoolE0_Get(void) {
    return &gBtlObjWork->poolE0;
}

/* Puts all 512 nodes on the free list. */
void BtlObjPoolE0_Init(void) {
    BtlObjPoolE0Work *pool = BtlObjPoolE0_Get();
    BtlObjPoolE0 *node;
    s32 i;

    memset(pool, 0, sizeof(BtlObjPoolE0Work));
    node = pool->nodes;
    for (i = 0; i < 2048; i++) {
        SList_PushFront(&pool->free, &node->link);
        node++;
    }
}

/* Takes a node from the pool and resets it. */
BtlObjPoolE0 *BtlObjPoolE0_Alloc(void) {
    BtlObjPoolE0 *node = (BtlObjPoolE0 *)SList_PopFront(&BtlObjPoolE0_Get()->free);

    if (node == NULL) {
        return NULL;
    }
    memset(node, 0, sizeof(BtlObjPoolE0));
    Vec4_Set(&node->unk90, 0.0f, 0.0f, 0.0f, 1.0f);
    Vec4_Set(&node->unkA0, 0.0f, 0.0f, 0.0f, 1.0f);
    return node;
}

/* Returns a whole list of nodes to the pool. */
s32 BtlObjPoolE0_FreeList(SList *list) {
    SList_AppendList(&BtlObjPoolE0_Get()->free, list);
    return 1;
}

/* Number of free 0xE0-byte nodes. */
s32 BtlObjPoolE0_GetFreeCount(void) {
    return SList_GetCount(&BtlObjPoolE0_Get()->free);
}

/* Returns the 0x70-byte node pool. */
BtlObjPool70Work *BtlObjPool70_Get(void) {
    return &gBtlObjWork->pool70;
}

/* Puts all 4 nodes on the free list. */
void BtlObjPool70_Init(void) {
    BtlObjPool70Work *pool = BtlObjPool70_Get();
    s32 i;

    memset(pool, 0, sizeof(BtlObjPool70Work));
    for (i = 0; i < BTL_OBJ_MAX; i++) {
        SList_PushFront(&pool->free, &pool->nodes[i].link);
    }
}

/* Takes a 0x70-byte node. */
BtlObjPool70 *BtlObjPool70_Alloc(void) {
    SListNode *link = SList_PopFront(&BtlObjPool70_Get()->free);
    BtlObjPool70 *node = (BtlObjPool70 *)link;

    return node;
}

/* Returns a 0x70-byte node. */
s32 BtlObjPool70_Free(BtlObjPool70 *node) {
    SList_PushBack(&BtlObjPool70_Get()->free, &node->link);
    return 1;
}

/* Number of free 0x70-byte nodes. */
s32 BtlObjPool70_GetFreeCount(void) {
    return SList_GetCount(&BtlObjPool70_Get()->free);
}

/* Returns the third pool. */
BtlObjPool3Work *BtlObjPool3_Get(void) {
    return &gBtlObjWork->pool3;
}

/* Takes a node of the third pool. */
SListNode *BtlObjPool3_Alloc(void) {
    SListNode *link = SList_PopFront(&BtlObjPool3_Get()->free);
    SListNode *node = link;

    return node;
}

/* Returns a node to the third pool. */
s32 BtlObjPool3_Free(SListNode *node) {
    SList_PushBack(&BtlObjPool3_Get()->free, node);
    return 1;
}

/* Number of free nodes of the third pool. */
s32 BtlObjPool3_GetFreeCount(void) {
    return SList_GetCount(&BtlObjPool3_Get()->free);
}

/* Returns the object table (the start of the work block). */
BtlObjTable *BtlObj_GetTable(void) {
    return &gBtlObjWork->table;
}

/* Returns the list of objects in use. */
List *BtlObj_GetUsedList(void) {
    return &BtlObj_GetTable()->usedObjs;
}

/* Clears the table and puts all 12 objects on the free list, numbered in order. */
void BtlObj_InitTable(void) {
    BtlObjTable *tbl = BtlObj_GetTable();
    BtlObjNode *node;
    s32 i;

    memset(tbl, 0, sizeof(BtlObjTable));
    tbl->unk44FC4 = 2.0f;
    List_Init(&tbl->freeObjs);
    List_Init(&tbl->usedObjs);
    node = tbl->objs;
    for (i = 0; i < BTL_OBJ_MAX; i++) {
        node->id = i;
        List_PushBack(&tbl->freeObjs, &node->link);
        node++;
    }
}

/* Returns the float at table + 0x44FC4. */
f32 BtlObj_GetUnk44FC4(void) {
    return BtlObj_GetTable()->unk44FC4;
}

/* Sets the float at table + 0x44FC4. */
void BtlObj_SetUnk44FC4(f32 value) {
    BtlObj_GetTable()->unk44FC4 = value;
}

/* Number of objects in use. */
s32 BtlObj_GetCount(void) {
    return List_GetCount(BtlObj_GetUsedList());
}

/* Moves a free object to the used list and returns its id, or -1. */
s32 BtlObj_AllocId(void) {
    BtlObjTable *tbl = BtlObj_GetTable();
    BtlObjNode *node = (BtlObjNode *)List_PopFront(&tbl->freeObjs);


    if (node != NULL) {
        List_PushBack(&tbl->usedObjs, &node->link);
        BtlObj_RebuildTable();
        return node->id;
    }
    return -1;
}

/* Moves the object with this id back to the free list; returns 1 if it was in use. */
s32 BtlObj_FreeId(s32 id) {
    BtlObjTable *tbl = BtlObj_GetTable();
    BtlObjNode *node;

    for (node = (BtlObjNode *)List_GetHead(BtlObj_GetUsedList()); node != NULL;
         node = (BtlObjNode *)List_GetNext(&node->link)) {
        if (node->id == id) {
            List_PushBack(&tbl->freeObjs, List_Remove(&tbl->usedObjs, &node->link));
            return 1;
        }
    }
    return 0;
}

/* Returns the object with this id, NULL if the id is free. */
BtlObj *BtlObj_Get(s32 id) {
    return (u32)id < BTL_OBJ_MAX ? nativeObjectTable[id] : NULL;
}

/* Returns the big work buffer that goes with a fighter object id. */
u8 *BtlObj_GetCharaWork(u32 id) {
    BtlObjTable *tbl = BtlObj_GetTable();

    if (id < PORT_FIGHTER_CAPACITY) {
        return tbl->charaWork[id];
    }
    return NULL;
}

/* Binds an object to a loaded model and fills its header. */
void BtlObj_Setup(BtlObj *obj, s32 type, BtlResSlot *res, s32 id, s32 active) {
    BtlObjMdl_Destroy(obj);
    BtlObjMdl_Create(type, obj, res);
    obj->active = active;
    obj->id = id;
    obj->chara = PortGame_ModelCharacter(res->file[0].id);
    BtlObj_InitChains(obj);
}

/* Creates an object from a loaded model; returns its id or -1 when the table is full. */
s32 BtlObj_Create(s32 type, BtlResSlot *res, s32 active) {
    s32 id = BtlObj_AllocId();
    BtlObj *obj;

    if (id < 0) {
        return -1;
    }
    obj = BtlObj_Get(id);
    BtlObj_Setup(obj, type, res, id, active);
    if (type == BTL_OBJ_TYPE_CHARA) {
        obj->charaWork = BtlObj_GetCharaWork(id);
        BtlObjFlash_Reset(obj);
    }
    return id;
}

/* Releases an object; returns 1 if it existed. */
s32 BtlObj_Destroy(s32 id) {
    BtlObj *obj = BtlObj_Get(id);


    if (obj == NULL) {
        return 0;
    }
    if (!BtlObj_FreeId(id)) {
        return 0;
    }
    BtlObjMdl_Destroy(obj);
    return 1;
}

/* Creates a fighter object from a model resource slot. */
s32 BtlObj_CreateChara(s32 slot) {
    return BtlObj_Create(BTL_OBJ_TYPE_CHARA, BtlRes_GetSlot(slot), 1);
}

/* Sets an existing fighter object up again from a model resource slot (character change). */
void BtlObj_Rebind(s32 id, s32 slot) {
    BtlResSlot *res = BtlRes_GetSlot(slot);
    BtlObj *obj = BtlObj_Get(id);

    BtlObj_Setup(obj, BTL_OBJ_TYPE_CHARA, res, id, 1);
    obj->charaWork = BtlObj_GetCharaWork(id);
    BtlObjFlash_Reset(obj);
}

/* Returns the model resource slot block. */
BtlResWork *BtlRes_GetWork(void) {
    return &gBtlObjWork->res;
}

/* Requests a character's model and its two animation files; returns the resource slot number. */
s32 BtlObj_RequestCharaModel(s32 side, s32 chara, s32 costume, s32 variant) {
    s32 model;
#ifdef PORT
    { extern void Port_ModelRequestTrace(s32,s32,s32,s32,s32);
      Port_ModelRequestTrace(side,-1,chara,costume,variant); }
    return BtlRes_Request(1, PortGame_Model(chara,costume,variant), PortGame_Animation(chara), PortGame_Combat(chara));
#endif
    if (variant != 0) {
        model = chara * 10 + costume + 0x594;
    } else {
        model = chara * 10 + costume + 0x590;
    }
    return BtlRes_Request(1, model, chara * 10 + 0x598, chara * 10 + 0x599);
}

/* Returns the work block. */
BtlObjWork *BtlObj_GetWork(void) {
    return gBtlObjWork;
}

/* Whether the module is initialised. */
s32 BtlObj_IsInit(void) {
    return gBtlObjWork != NULL;
}

/* Commits a pending model reload. */
void BtlRes_CommitReloadEx(void) {
    BtlRes_CommitReload();
}

/* Allocates the work block and initialises every part of it. */
void BtlObj_Init(s32 prealloc) {
    gBtlObjWork = Heap_Alloc(sizeof(BtlObjWork), 0x20, 0, HEAP_ANY);
#ifdef PORT
    PortGame_ResetAnimations();
#endif
    memset(gBtlObjWork, 0, sizeof(BtlObjWork));
    BtlRes_Init(prealloc);
    BtlObj_InitTable();
    BtlObjPoolE0_Init();
    BtlObjPool70_Init();
    ObjShadow_InitPool();
    BtlObj_InitNop();
    BtlObjLight_Init();
    BtlObjVis_Clear();
    BtlObj_InitDraw();
}

/* Shuts the parts down and frees the work block. */
void BtlObj_Term(void) {
    BtlObj_TermDraw();
    BtlObj_TermNop();
    BtlRes_Term();
    ObjShadow_TermPool();
    Heap_Free(gBtlObjWork);
    gBtlObjWork = NULL;
}

/* Rebuilds the object's world box from the bounds records of its model. */
void BtlObj_UpdateBounds(BtlObj *obj, BtlObjState *state) {
    Vec4 rel;
    Vec4 center;
    Mtx44 mtx;
    Vec4 unk60;
    Vec4 ext;
    Vec4 unk80[7];
    Vec4 corner[8];
    Vec4 out[8];
    BtlObjBound *bound = obj->mdl.bounds;
    BtlObjPart *part;

    StgAabb_SetEmpty(&state->box);
    while (1) {
        if (bound->enabled != 0) {
            part = BtlObj_GetNode(obj, bound->node);
            if (BtlObj_IsNodeShown(obj, part->unk08) == 1) {
                part->active = 1;
                Vec3_Sub(&rel, &bound->center, &bound->unk10);
                Mtx_MulVec4(&center, &part->mtx, &rel);
                Mtx_Copy(&mtx, &part->mtx);
                Vec4_Copy((Vec4 *)mtx.m[3], &center);
                Vec4_Set(&ext, bound->extent.x, bound->extent.y, bound->extent.z, 0.0f);
                Vec4_Set(&corner[0], ext.x, ext.y, ext.z, 1.0f);
                Vec4_Set(&corner[1], -ext.x, ext.y, ext.z, 1.0f);
                Vec4_Set(&corner[2], ext.x, -ext.y, ext.z, 1.0f);
                Vec4_Set(&corner[3], -ext.x, -ext.y, ext.z, 1.0f);
                Vec4_Set(&corner[4], ext.x, ext.y, -ext.z, 1.0f);
                Vec4_Set(&corner[5], -ext.x, ext.y, -ext.z, 1.0f);
                Vec4_Set(&corner[6], ext.x, -ext.y, -ext.z, 1.0f);
                Vec4_Set(&corner[7], -ext.x, -ext.y, -ext.z, 1.0f);
                Mtx_MulVec4(&out[0], &mtx, &corner[0]);
                Mtx_MulVec4(&out[1], &mtx, &corner[1]);
                Mtx_MulVec4(&out[2], &mtx, &corner[2]);
                Mtx_MulVec4(&out[3], &mtx, &corner[3]);
                Mtx_MulVec4(&out[4], &mtx, &corner[4]);
                Mtx_MulVec4(&out[5], &mtx, &corner[5]);
                Mtx_MulVec4(&out[6], &mtx, &corner[6]);
                Mtx_MulVec4(&out[7], &mtx, &corner[7]);
                ColBox_AddPoint(&state->box, &out[0]);
                ColBox_AddPoint(&state->box, &out[1]);
                ColBox_AddPoint(&state->box, &out[2]);
                ColBox_AddPoint(&state->box, &out[3]);
                ColBox_AddPoint(&state->box, &out[4]);
                ColBox_AddPoint(&state->box, &out[5]);
                ColBox_AddPoint(&state->box, &out[6]);
                ColBox_AddPoint(&state->box, &out[7]);
            } else {
                part->active = 0;
            }
        }
        if (bound->last != 0) {
            return;
        }
        bound = (BtlObjBound *)((u8 *)bound + bound->next);
    }
}

/* Stores the object's horizontal distance from the camera in the view record. */
void BtlObj_CalcViewDist(BtlObjState *state, BtlObjView *view) {
    Vec4 center;
    Vec4 ext;
    Vec4 corner[4];
    s32 i;
    f32 dist;

    view->dist = gBtlObjFarDist[0];
    ColBox_GetCenter(&state->box, &center);
    ColBox_GetHalf(&state->box, &ext);
    Vec4_Set(&corner[0], ext.x + center.x, 0.0f, ext.z + center.z, 1.0f);
    Vec4_Set(&corner[1], ext.x + center.x, 0.0f, center.z - ext.z, 1.0f);
    Vec4_Set(&corner[2], center.x - ext.x, 0.0f, ext.z + center.z, 1.0f);
    Vec4_Set(&corner[3], center.x - ext.x, 0.0f, center.z - ext.z, 1.0f);
    /* Tests the last corner four times: the original does not index the array. */
    for (i = 0; i < 4; i++) {
        dist = View_GetDistXZ(&corner[3]);
        if (dist < view->dist) {
            view->dist = dist;
        }
    }
    if (view->dist > 0.0f) {
        view->dist = sqrtf(view->dist);
    }
}

/* Distance fade: 128 up to nearDist, 0 beyond farDist, linear between; flag 8 says the fade is non-zero. */
void BtlObjView_SetFade(BtlObjView *view, f32 nearDist, f32 farDist) {
    if (farDist < view->dist) {
        view->fade = 0.0f;
        view->flags &= ~BTL_OBJ_FLAG_FADE;
    } else if (nearDist < view->dist) {
        view->fade = 128.0f - (view->dist - nearDist) / (farDist - nearDist) * 128.0f;
        if ((s32)view->fade == 0) {
            view->flags &= ~BTL_OBJ_FLAG_FADE;
        } else {
            view->flags |= BTL_OBJ_FLAG_FADE;
        }
    } else {
        view->fade = 128.0f;
        view->flags |= BTL_OBJ_FLAG_FADE;
    }
}

/* Final alpha of each of the 16 material slots: the model's value plus the object's offset, at most 0x7F. */
void BtlObj_UpdateAlpha(BtlObj *obj) {
    BtlObjState *state = &obj->state;
    BtlObjMdl *mdl = &obj->mdl;
    s32 i;
    s32 alpha;

    for (i = 0; i < 16; i++) {
        alpha = state->alphaAdd + mdl->model->alpha[i];
        if (alpha >= 0x80) {
            alpha = 0x7F;
        }
        state->alpha[i] = alpha;
    }
}

/* Chooses the colour the object is lit with in this view from its flags. */
void BtlObj_CalcViewColor(BtlObj *obj, BtlObjView *view) {
    BtlObjVec color = { 128.0f, 128.0f, 128.0f, 128.0f };
    s32 ambient[4];

    BtlStage_GetAmbient(ambient);
    if (view->flags & BTL_OBJ_FLAG_COLOR_STAGE) {
        color.x = ambient[0];
        color.y = ambient[1];
        color.z = ambient[2];
        Vec4_Copy(&view->color, (Vec4 *)&color);
    } else if (view->flags & BTL_OBJ_FLAG_COLOR_A) {
        Vec4_Copy(&view->color, (Vec4 *)&color);
    } else if (view->flags & BTL_OBJ_FLAG_COLOR_B) {
        Vec4_Copy(&view->color, (Vec4 *)&color);
    } else if (view->flags & BTL_OBJ_FLAG_COLOR_BLACK) {
        Vec4_Set(&view->color, 0.0f, 0.0f, 0.0f, 128.0f);
    } else if (view->flags & BTL_OBJ_FLAG_PASS1) {
        Vec4_Copy(&view->color, (Vec4 *)&color);
    } else if (view->flags & BTL_OBJ_FLAG_COLOR_C) {
        Vec4_Copy(&view->color, (Vec4 *)&color);
    } else {
        Vec4_Copy(&view->color, (Vec4 *)&color);
    }
}

/* Returns 1 when all eight corners of the object's box are beyond the same edge of the view. */
s32 BtlObj_IsOffscreen(BtlObj *obj) {
    Vec4 corner[8];
    Vec4 center;
    Vec4 ext;
    Mtx44 mtx;
    s32 count[5];
    s32 scr[4];
    BtlObjBox *box;
    s32 out = 0;
    s32 total;
    s32 prev;
    s32 i;

    memset(count, 0, sizeof(count));
    box = &obj->state.box;
    ColBox_GetCenter(box, &center);
    ColBox_GetHalf(box, &ext);
    total = 0;
    Vec4_Set(&corner[0], ext.x, ext.y, ext.z, 1.0f);
    Vec4_Set(&corner[1], -ext.x, ext.y, ext.z, 1.0f);
    Vec4_Set(&corner[2], ext.x, -ext.y, ext.z, 1.0f);
    Vec4_Set(&corner[3], -ext.x, -ext.y, ext.z, 1.0f);
    Vec4_Set(&corner[4], ext.x, ext.y, -ext.z, 1.0f);
    Vec4_Set(&corner[5], -ext.x, ext.y, -ext.z, 1.0f);
    Vec4_Set(&corner[6], ext.x, -ext.y, -ext.z, 1.0f);
    Vec4_Set(&corner[7], -ext.x, -ext.y, -ext.z, 1.0f);
    Vu0Screen_StoreMtx(&mtx);
    for (i = 0; i < 8; i++) {
        Vec3_Add(&corner[i], &corner[i], &center);
        Mtx_ProjectInt(scr, &mtx, &corner[i]);
        scr[0] -= 0x700;
        scr[1] -= 0x720;
        if (scr[2] < 0) {
            scr[0] = -scr[0];
            scr[1] = -scr[1];
        }
        prev = total;
        if (scr[2] < 0) {
            count[4]++;
            total++;
        }
        if (scr[0] < gBtlCamView->scissorX0) {
            count[0]++;
            total++;
        }
        if (gBtlCamView->scissorX1 + 1 < scr[0]) {
            count[1]++;
            total++;
        }
        if (scr[1] < gBtlCamView->scissorY0) {
            count[2]++;
            total++;
        }
        if (gBtlCamView->scissorY1 + 1 < scr[1]) {
            count[3]++;
            total++;
        }
        if (prev != total) {
            out++;
        }
    }
    if (out == 8) {
        if (count[0] == 8) {
            return 1;
        }
        if (count[1] == 8) {
            return 1;
        }
        if (count[2] == 8) {
            return 1;
        }
        if (count[3] == 8) {
            return 1;
        }
        if (count[4] == 8) {
            return 1;
        }
    }
    return 0;
}

/* Fills the object's record for the view being built: flags, distance, culling, colour and fade. */
void BtlObj_UpdateView(BtlObj *obj) {
    BtlObjState *state = &obj->state;
    BtlObjMdl *mdl = &obj->mdl;
    BtlObjView *view = obj->state.view;

    memset(view, 0, sizeof(BtlObjView));
    view->flags = state->flags;
    BtlObj_CalcViewDist(state, view);
    if (view->flags & BTL_OBJ_FLAG_VISIBLE) {
        if (BtlObj_IsOffscreen(obj)) {
            view->flags &= ~BTL_OBJ_FLAG_VISIBLE;
        } else {
            if (!(Battle_GetWork()->flags & 0x2000000000000000) && obj->type == BTL_OBJ_TYPE_CHARA &&
                (state->flags & BTL_OBJ_FLAG_CHECK_NEAR)) {
                BtlObjBody *body = &obj->body;

                if (body != NULL) {
                    Mtx44 cam;
                    Vec4 center;
                    Vec4 ext;
                    BtlObjBox8 box;

                    Mtx_InverseRT(&cam, &gBtlCamView->world2view2);
                    box = body->box;
                    ColBox_GetCenter((BtlObjBox *)&box, &center);
                    ColBox_GetHalf((BtlObjBox *)&box, &ext);
                    Vec3_Scale(&ext, &ext, 2.2f);
                    StgAabb_SetEmpty((BtlObjBox *)&box);
                    ColBox_SetCenterHalf((BtlObjBox *)&box, &center, &ext);
                    if (ColBox_ContainsPoint((BtlObjBox *)&box, (Vec4 *)cam.m[3])) {
                        view->flags = (view->flags & 0xFF00FFFF) | BTL_OBJ_FLAG_PASS2;
                    }
                }
            }
            BtlObj_CalcViewColor(obj, view);
            if ((mdl->model->flags & 1) && !(FLAG_BITS(state->view)->black || FLAG_BITS(state->view)->pass1 || FLAG_BITS(state->view)->pass2)) {
                BtlObjView_SetFade(view, (mdl->model->fadeNear == 0.0f) ? 40.0f : mdl->model->fadeNear,
                                   (mdl->model->fadeFar == 0.0f) ? 120.0f : mdl->model->fadeFar);
            } else {
                view->fade = 0.0f;
                view->flags &= ~BTL_OBJ_FLAG_FADE;
            }
        }
    }
}

/* Builds a view's draw list in three passes; the view's own fighter, when in the last pass, goes last. */
void BtlObj_BuildDrawList(BtlObjDrawList *list, s32 view) {
    BtlObj *own = NULL;
    BtlObjNode *node;
    BtlObj *obj;
    BtlObjState *state;
    BtlObjView *rec;
    s32 pass;

    memset(list, 0, sizeof(BtlObjDrawList));
    for (pass = 0; pass < 3; pass++) {
        for (node = (BtlObjNode *)List_GetHead(BtlObj_GetUsedList()); node != NULL;
             node = (BtlObjNode *)List_GetNext(&node->link)) {
            obj = &node->obj;
            state = &node->obj.state;
            if (obj->unk04 != 0 && obj->active != 0) {
                rec = state->view;
                if (rec->flags & BTL_OBJ_FLAG_VISIBLE) {
                    if (pass == 0) {
                        if (!(FLAG_BITS(rec)->pass1 || FLAG_BITS(rec)->pass2)) {
                            list->objs[list->count++] = obj;
                        }
                    } else if (pass == 1) {
                        if (rec->flags & BTL_OBJ_FLAG_PASS1) {
                            list->objs[list->count++] = obj;
                        }
                    } else if (rec->flags & BTL_OBJ_FLAG_PASS2) {
                        if (
#ifdef PORT
                            (TagTeam_Active() ? TagTeam_ViewActor(view) : view)
#else
                            view
#endif
                            == obj->id && obj->type == BTL_OBJ_TYPE_CHARA) {
                            own = obj;
                        } else {
                            list->objs[list->count++] = obj;
                        }
                    }
                }
            }
        }
    }
    if (own != NULL) {
        list->objs[list->count++] = own;
    }
}

/* Per-frame update of every object: light from the stage, animation and pose, bounds, alpha. */
void BtlObj_UpdateAll(void) {
    Vec4 dir;
    BtlObjNode *node;
    BtlObj *obj;
    BtlObjState *state;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (BtlStage_IsReady()) {
        BtlObjVec color = { 10.0f, 10.0f, 40.0f, 64.0f };
        u8 *light;

        BtlStage_GetLightVecB(&dir);
        light = BtlStage_GetLightColors();
        if (light != NULL) {
            Vec4_Set((Vec4 *)&color, light[8], light[9], light[10], light[11]);
        }
        BtlObjLight_SetDir(&dir);
        BtlObjLight_SetColor((Vec4 *)&color);
    }
    for (node = (BtlObjNode *)List_GetHead(BtlObj_GetUsedList()); node != NULL;
         node = (BtlObjNode *)List_GetNext(&node->link)) {
        obj = &node->obj;
        if (obj->unk04 != 0 && obj->active != 0) {
            state = &node->obj.state;
            state->viewFlags = 0;
            if (obj->noAnim == 0) {
                BtlObj_SaveNodePositions(obj, 0);
                BtlObjAnim_Step(obj);
                BtlObjAnim_SamplePose(obj);
                BtlObjAnim_UpdateEvents(obj);
                BtlObjPose_CalcMatrices(obj);
                BtlObj_UpdateChains(obj);
                BtlObjPose_CalcMatrices(obj);
            }
            BtlObj_UpdateBounds(obj, state);
            BtlObj_UpdateFace(obj);
            BtlObjFlash_Step(obj);
            BtlObjFade_Step(obj);
            ObjShadow_Update(obj);
            BtlObj_UpdateAlpha(obj);
        }
    }
}

/* Builds every object's record and the draw list for one view (the current gBtlCamView). */
void BtlObj_UpdateVisibility(s32 view) {
    BtlObjVis *vis = BtlObjVis_Get();
    BtlObjNode *node;
    BtlObj *obj;
    BtlObjState *state;
    s32 idx;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    idx = TagTeam_Active() ? view : view == 1;
    if (idx < 2) vis->views[idx] = gBtlCamView; else extraView[idx - 2] = gBtlCamView;
    for (node = (BtlObjNode *)List_GetHead(BtlObj_GetUsedList()); node != NULL;
         node = (BtlObjNode *)List_GetNext(&node->link)) {
        obj = &node->obj;
        if (obj->unk04 != 0 && obj->active != 0) {
            state = &node->obj.state;
            state->view = BtlObj_View(obj, idx);
            BtlObj_UpdateView(obj);
#ifdef PORT
            if (TagTeam_Active()) {
                extern void Port_TagObjectTrace(s32,s32,u32,u32);
                Port_TagObjectTrace(idx,obj->id,state->view->flags,(u32)state->view);
            }
#endif
        }
    }
    BtlObj_BuildDrawList(BtlObjVis_GetListExt(idx), idx);
}

/* Combines the view records into per-object and global flags, then sends the alpha table. */
void BtlObj_FinishVisibility(s32 split) {
    BtlObjVis *vis = BtlObjVis_Get();
    BtlObjNode *node;
    BtlObj *obj;
    BtlObjState *state;
    BtlObjView *rec;
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    vis->flags = 0;
    vis->viewCount = (split == 0) ? 1 : TagTeam_ViewCount();
    for (i = 0; i < vis->viewCount; i++) {
        if (split && !TagTeam_ViewVisible(i)) continue;
        for (node = (BtlObjNode *)List_GetHead(BtlObj_GetUsedList()); node != NULL;
             node = (BtlObjNode *)List_GetNext(&node->link)) {
            obj = &node->obj;
            if (obj->unk04 != 0 && obj->active != 0) {
                state = &node->obj.state;
                rec = BtlObj_View(obj, i);
                state->viewFlags |= rec->flags;
                vis->flags |= rec->flags;
            }
        }
    }
    BtlObj_UploadAlphaTable();
}

/* Writes the object's 15 alpha bytes into its model's row of the alpha table. */
void BtlObj_FillAlphaRow(u8 *table, BtlObj *obj) {
    BtlObjState *state = &obj->state;
    s32 row = obj->mdl.model->alphaRow * 15;
    s32 i;

    if (state->flags & BTL_OBJ_FLAG_COLOR_BLACK) {
        for (i = 0; i < 15; i++) {
            table[row + i] = 0xFF;
        }
    } else if (obj->state.flags & BTL_OBJ_FLAG_COLOR_B) {
        Vec4 value;
        u8 alpha;

        BtlObjFade_Get(obj, &value);
        alpha = (u32)(value.x * 128.0f);
        for (i = 0; i < 15; i++) {
            table[row + i] = alpha;
        }
    } else if (obj->state.flags & BTL_OBJ_FLAG_COLOR_C) {
        u8 alpha = (u32)obj->state.unkDC;

        for (i = 0; i < 15; i++) {
            table[row + i] = alpha;
        }
    } else {
        for (i = 0; i < 15; i++) {
            table[row + i] = state->alpha[i];
        }
    }
}

/* Initialises the three drawing modules objects are drawn with. */
void BtlObj_InitDraw(void) {
    ObjOutline_Init();
    ObjGlow_Init();
    GfxAlphaKey_Init();
}

/* Shuts the three drawing modules down. */
void BtlObj_TermDraw(void) {
    ObjOutline_Term();
    ObjGlow_Term();
    GfxAlphaKey_Term();
}

/* Sets the full-screen scissor and starts a frame of the drawing modules. */
void BtlObj_BeginDraw(void) {
    Dma_AddScissor(0, 0x1FF, 0, 0x1BF);
    GfxPost_CopyAlphaToDepth();
    GfxAlphaKey_Draw();
    ObjOutline_Draw();
    ObjGlow_Draw();
}

/* Builds the 0x100-byte alpha table from every active object and hands it to the drawing module. */
void BtlObj_UploadAlphaTable(void) {
    u8 table[0x100];
    BtlObjNode *node;
    BtlObj *obj;

    memset(table, 0, sizeof(table));
    for (node = (BtlObjNode *)List_GetHead(BtlObj_GetUsedList()); node != NULL;
         node = (BtlObjNode *)List_GetNext(&node->link)) {
        obj = &node->obj;
        if (obj->unk04 != 0 && obj->active != 0) {
            BtlObj_FillAlphaRow(table, obj);
        }
    }
    ObjGlow_SetAlphaTable(table);
}

/* Takes a free fixed slot (the two with preallocated buffers). */
BtlResSlot *BtlRes_AllocFixed(void) {
    BtlResSlot *slot = BtlRes_GetWork()->fixed;
    s32 i;

    for (i = 0; i < BTL_RES_FIXED_MAX; i++) {
        if (!(slot->flags & BTL_RES_SLOT_USED)) {
            slot->flags |= BTL_RES_SLOT_USED;
            return slot;
        }
        slot++;
    }
    return NULL;
}

/* Takes a free heap slot. */
BtlResSlot *BtlRes_AllocSlot(void) {
    BtlResSlot *slot = BtlRes_GetWork()->slots;
    s32 i;

    for (i = 0; i < BTL_RES_SLOT_MAX; i++) {
        if (!(slot->flags & BTL_RES_SLOT_USED)) {
            slot->flags |= BTL_RES_SLOT_USED;
            return slot;
        }
        slot++;
    }
    return NULL;
}

/* Finds a heap slot by number. */
BtlResSlot *BtlRes_FindSlot(s32 number) {
    BtlResSlot *slot = BtlRes_GetWork()->slots;
    s32 i;

    for (i = 0; i < BTL_RES_SLOT_MAX; i++) {
        if (slot->number == number) {
            return slot;
        }
        slot++;
    }
    return NULL;
}

/* Copies buffer, size and id of a file record. */
void BtlRes_CopyFile(BtlResFile *dst, BtlResFile *src) {
    dst->buf = src->buf;
    dst->size = src->size;
    dst->id = src->id;
}

/* Exchanges buffer, size and id of two file records. */
void BtlRes_SwapFile(BtlResFile *a, BtlResFile *b) {
    BtlResFile tmp;

    tmp.buf = a->buf;
    tmp.size = a->size;
    tmp.id = a->id;
    BtlRes_CopyFile(a, b);
    b->buf = tmp.buf;
    b->size = tmp.size;
    b->id = tmp.id;
}

/* Sets which file a record holds: an id, none (negative), or unchanged (BTL_RES_ID_KEEP). */
void BtlRes_SetFileId(BtlResFile *file, s32 id) {
    if (id >= 0) {
        file->id = id;
        file->size = File_GetSize(id);
    } else if (id != BTL_RES_ID_KEEP) {
        file->size = 0;
        file->id = -1;
    }
}

/* Sets the three file ids of a slot. */
void BtlRes_SetIds(BtlResSlot *slot, s32 model, s32 anim0, s32 anim1) {
    BtlRes_SetFileId(&slot->file[0], model);
    BtlRes_SetFileId(&slot->file[1], anim0);
    BtlRes_SetFileId(&slot->file[2], anim1);
}

/* Frees a heap-loaded file; returns 1 if there was one. */
s32 BtlRes_FreeFile(BtlResFile *file) {
    s32 freed = 0;

    if (file->buf != NULL) {
        Heap_Free(file->buf);
        freed = 1;
        file->buf = NULL;
        file->size = 0;
        file->id = -1;
    }
    return freed;
}

/* Queues the read of a file: into a new heap buffer (fromHeap == 1) or into the record's own buffer. */
void BtlRes_LoadFile(BtlResFile *file, s32 fromHeap) {
    if (fromHeap == 1) {
        BtlRes_FreeFile(file);
        if (file->id >= 0) {
            file->buf = File_Request3(file->id, NULL, 0);
        }
    } else {
        if (file->id >= 0) {
            file->buf = File_Request3(file->id, file->buf, file->size);
        }
    }
}

/* After a reload has been read: swaps the new model buffer into the fixed slot. */
void BtlRes_CommitReload(void) {
    BtlResWork *res = BtlRes_GetWork();

    if (res->pending >= 0) {
        BtlRes_SwapFile(&BtlRes_GetSlot(res->pending)->file[0], res->next);
        res->pending = -1;
    }
}

/* Clears the slots; with prealloc, allocates the buffers of the two fixed slots and the spare model buffer. */
void BtlRes_Init(s32 prealloc) {
    BtlResWork *res = BtlRes_GetWork();
    s32 number = 0;
    BtlResSlot *slot;
    BtlResFile *spare;
    s32 i;

    memset(res, 0, sizeof(BtlResWork));
    res->prealloc = prealloc;
    if (prealloc != 0) {
        for (i = 0; i < BTL_RES_FIXED_MAX + 1; i++) {
            res->spare[i].buf = Heap_Alloc((PortGame_IsBT4()?0xE6800:BTL_RES_MODEL_SIZE), 0x40, 0, HEAP_ANY);
            res->spare[i].size = 0;
            res->spare[i].id = -1;
            res->spare[i].orig = res->spare[i].buf;
            memset(res->spare[i].buf, 0, (PortGame_IsBT4()?0xE6800:BTL_RES_MODEL_SIZE));
        }
        for (i = 0; i < BTL_RES_FIXED_MAX; i++) {
            slot = &res->fixed[i];
            spare = &res->spare[i];
            slot->file[1].buf = Heap_Alloc(BTL_RES_ANIM0_SIZE, 0x40, 0, HEAP_ANY);
            slot->file[1].size = 0;
            slot->file[1].id = -1;
            slot->file[2].buf = Heap_Alloc((PortGame_IsBT4()?0xD3800:BTL_RES_ANIM1_SIZE), 0x40, 0, HEAP_ANY);
            slot->file[2].size = 0;
            slot->file[2].id = -1;
            memset(slot->file[1].buf, 0, BTL_RES_ANIM0_SIZE);
            memset(slot->file[2].buf, 0, (PortGame_IsBT4()?0xD3800:BTL_RES_ANIM1_SIZE));
            BtlRes_CopyFile(&slot->file[0], spare);
            slot->number = number++;
        }
        res->next = &res->spare[BTL_RES_FIXED_MAX];
    }
    for (i = 0; i < BTL_RES_SLOT_MAX; i++) {
        slot = &res->slots[i];
        slot->number = number++;
    }
    res->pending = -1;
}

/* Frees every slot and the preallocated buffers. */
void BtlRes_Term(void) {
    BtlResWork *res = BtlRes_GetWork();
    BtlResSlot *slot;
    s32 i;

    for (i = 0; i < BTL_RES_SLOT_MAX; i++) {
        BtlRes_Free(res->slots[i].number);
    }
    slot = res->fixed;
    for (i = 0; i < BTL_RES_FIXED_MAX; i++) {
        if (slot->file[1].buf != NULL) {
            Heap_Free(slot->file[1].buf);
        }
        if (slot->file[2].buf != NULL) {
            Heap_Free(slot->file[2].buf);
        }
        slot++;
    }
    for (i = 0; i < BTL_RES_FIXED_MAX + 1; i++) {
        if (res->spare[i].orig != NULL) {
            Heap_Free(res->spare[i].orig);
        }
    }
}

/* Takes a slot and queues its files; returns the slot number or -1. */
s32 BtlRes_Request(s32 fixed, s32 model, s32 anim0, s32 anim1) {
    BtlResSlot *slot;

    if (fixed == 1) {
        slot = BtlRes_AllocFixed();
        if (slot == NULL) {
            return -1;
        }
        BtlRes_SetIds(slot, model, anim0, anim1);
        BtlRes_LoadFile(&slot->file[0], 0);
        BtlRes_LoadFile(&slot->file[1], 0);
        BtlRes_LoadFile(&slot->file[2], 0);
    } else {
        slot = BtlRes_AllocSlot();
        if (slot == NULL) {
            return -1;
        }
        BtlRes_SetIds(slot, model, anim0, anim1);
        BtlRes_LoadFile(&slot->file[0], 1);
        BtlRes_LoadFile(&slot->file[1], 1);
        BtlRes_LoadFile(&slot->file[2], 1);
        slot->flags |= BTL_RES_SLOT_HEAP;
    }
    return slot->number;
}

/* Releases a heap slot and its files. Fixed slots are never released. */
void BtlRes_Free(s32 number) {
    BtlResSlot *slot;

    if (BtlRes_GetWork()->prealloc != 0 && number < BTL_RES_FIXED_MAX) {
        return;
    }
    slot = BtlRes_FindSlot(number);
    if (slot != NULL) {
        BtlRes_FreeFile(&slot->file[0]);
        BtlRes_FreeFile(&slot->file[1]);
        BtlRes_FreeFile(&slot->file[2]);
        slot->flags = 0;
    }
}

/* Returns the slot with this number, NULL if there is none. */
BtlResSlot *BtlRes_GetSlot(s32 number) {
    BtlResWork *res = BtlRes_GetWork();
    BtlResSlot *slot;
    s32 i;

    if (res->prealloc != 0) {
        slot = res->fixed;
        for (i = 0; i < BTL_RES_FIXED_MAX; i++) {
            if (slot->number == number) {
                return slot;
            }
            slot++;
        }
    }
    slot = res->slots;
    for (i = 0; i < BTL_RES_SLOT_MAX; i++) {
        if (slot->number == number) {
            return slot;
        }
        slot++;
    }
    return NULL;
}

/* Character change: queues new files for a fixed slot. The model goes to the spare buffer and is swapped in
   by BtlRes_CommitReload; the animation files are read over the old ones unless only the model changes. */
void BtlRes_Reload(s32 number, s32 model, s32 anim0, s32 anim1) {
    BtlResWork *res = BtlRes_GetWork();
    BtlResSlot *slot;
    s32 modelOnly = 0;

    if (res->pending < 0 && number < BTL_RES_FIXED_MAX) {
        slot = BtlRes_GetSlot(number);
        if (slot != NULL) {
            if (model != -1 && anim0 == -1) {
                modelOnly = anim1 == anim0;
            }
            if (!modelOnly) {
                BtlRes_SetIds(slot, BTL_RES_ID_KEEP, anim0, anim1);
                BtlRes_SetFileId(res->next, model);
                BtlRes_LoadFile(res->next, 0);
                BtlRes_LoadFile(&slot->file[1], 0);
                BtlRes_LoadFile(&slot->file[2], 0);
            } else {
                BtlRes_SetFileId(res->next, model);
                BtlRes_LoadFile(res->next, 0);
            }
            res->pending = number;
        }
    }
}

/* Same as BtlRes_CommitReload (battle_load.c calls this copy). */
void BtlRes_CommitReload2(void) {
    BtlResWork *res = BtlRes_GetWork();

    if (res->pending >= 0) {
        BtlRes_SwapFile(&BtlRes_GetSlot(res->pending)->file[0], res->next);
        res->pending = -1;
    }
}

/* Loads one model into a caller-owned slot record (not one of the work block's). */
void BtlRes_LoadSingle(s32 fromHeap, BtlResSlot *slot, s32 model) {
    if (model > 0) {
        if (fromHeap != 0) {
            memset(slot, 0, sizeof(slot->file));
        } else {
            slot->file[1].buf = NULL;
            slot->file[2].buf = NULL;
        }
        BtlRes_SetIds(slot, model, -1, -1);
        BtlRes_LoadFile(&slot->file[0], fromHeap);
    }
}

/* Frees the files of a caller-owned slot record. */
void BtlRes_FreeSingle(BtlResSlot *slot) {
    BtlRes_FreeFile(&slot->file[0]);
    BtlRes_FreeFile(&slot->file[1]);
    BtlRes_FreeFile(&slot->file[2]);
    memset(slot, 0, sizeof(slot->file));
}
