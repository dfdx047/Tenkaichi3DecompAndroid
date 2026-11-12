#include "common.h"
#include "battle/eft_ad.h"

/*
 * 0x1A7608..0x1A9D90: the effect mesh (EftMesh_*), the effect model objects (EftObj_*) and the mesh's two
 * triangle writers. See include/battle/eft_ad.h.
 *
 * EftMesh_IsOffScreen is a non-static inline of the original: it is defined in the middle of the file (the
 * two fan functions after it have it inlined, the two draw loops before it call it) and the compiler emits its
 * out-of-line copy at the end of the object, 0x1A9D38. That is why the object ends at 0x1A9D90.
 */

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftAdView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ Mtx44 world2screen;
} EftAdView;

/* A battle object as far as the wrappers reach into it (BtlObj in include/battle/btl_obj.h). */
typedef struct EftAdObj {
    /* 0x000 */ u8 unk0[0x950];
    /* 0x950 */ Vec4 pos;
    /* 0x960 */ Vec4 rot;
    /* 0x970 */ u8 unk970[0xD0];
    /* 0xA40 */ u32 flags;     /* BTL_OBJ_FLAG_*; 2 = drawn */
} EftAdObj;

/* An order table slot: two lists of packets (include/sys/gfx.h has the full description). */
typedef struct EftAdOtPrim {
    u32 tag;
    void *next;
} EftAdOtPrim;

typedef struct EftAdOtEntry {
    EftAdOtPrim *head;
    EftAdOtPrim *tail;
} EftAdOtEntry;

typedef struct EftAdOtSlot {
    EftAdOtEntry layer[2];
} EftAdOtSlot;

typedef struct EftAdXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftAdXyzf;

typedef struct EftAdRgbaq {
    u8 r, g, b, a;
    f32 q;
} EftAdRgbaq;

typedef struct EftAdSt {
    f32 s, t;
} EftAdSt;

/* Sets CLAMP_1 and CLAMP_2 (texture wrap modes of both GS contexts) to 0. */
typedef struct EftMeshClampPkt {
    /* 0x00 */ u32 dmaTag;     /* 0x20000003 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;       /* 0x10000000 */
    /* 0x0C */ u32 vif1;       /* 0x50000003 */
    /* 0x10 */ u64 gifTag;     /* 0x1000000000008002: PACKED, 1 register, 2 loops */
    /* 0x18 */ u64 regs;       /* A+D */
    /* 0x20 */ u64 data0;
    /* 0x28 */ u64 addr0;      /* 8: CLAMP_1 */
    /* 0x30 */ u64 data1;
    /* 0x38 */ u64 addr1;      /* 9: CLAMP_2 */
} EftMeshClampPkt; /* 0x40 */

/* Textured gouraud triangle. */
typedef struct EftMeshTexTriPkt {
    /* 0x00 */ u32 dmaTag;     /* 0x20000007 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;       /* 0x10000000 */
    /* 0x0C */ u32 vif1;       /* 0x50000007 */
    /* 0x10 */ u64 gifTag;     /* 0xC400000000008001: REGLIST, 12 registers */
    /* 0x18 */ u64 regs;       /* PRIM, TEX0_1 (TEX0_2 for layers 2 / 3), 3 x (RGBAQ, ST, XYZF2), NOP */
    /* 0x20 */ u64 prim;       /* 0x1B (gouraud textured triangle) | ABE << 6 | context << 9 */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ struct {
        EftAdRgbaq rgbaq;
        EftAdSt st;
        EftAdXyzf xyz;
    } v[3];
    /* 0x78 */ u64 nop;
} EftMeshTexTriPkt; /* 0x80 */

/* Untextured gouraud triangle. */
typedef struct EftMeshTriPkt {
    /* 0x00 */ u32 dmaTag;     /* 0x20000005 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;       /* 0x10000000 */
    /* 0x0C */ u32 vif1;       /* 0x50000005 */
    /* 0x10 */ u64 gifTag;     /* 0x8400000000008001: REGLIST, 8 registers */
    /* 0x18 */ u64 regs;       /* PRIM, 3 x (RGBAQ, XYZF2), NOP */
    /* 0x20 */ u64 prim;       /* 0xB (gouraud triangle) | ABE << 6 */
    /* 0x28 */ struct {
        EftAdRgbaq rgbaq;
        EftAdXyzf xyz;
    } v[3];
    /* 0x58 */ u64 nop;
} EftMeshTriPkt; /* 0x60 */

extern u8 *gOtCur;
extern EftAdOtSlot *gOtZ;
extern void *memcpy(void *dst, const void *src, u32 n);
extern u8 *Dma_BeginDirect(void);
extern void Dma_EndDirect(u8 *end);

extern EftAdView *gBtlCamView;

extern void *memset(void *dst, s32 c, u32 n);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);          /* copies a matrix */
extern void Vu0Cur_Push(void);                            /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(Mtx44 *m);                        /* load the matrix */
extern void Vu0Cur_Pop(void);                            /* pop */
extern void Vec4_Mul(Vec4 *dst, Vec4 *a, Vec4 *b);     /* per-component product */
extern s32 Vu0Cur_ProjectPointsStq(EftAdScr *xyz, Vec4 *stq, Vec4 *pos, Vec4 *uv, s32 n); /* projects n points with uv */
extern EftAdVec *EftGfx_GetClipPlanes(void);                 /* the five clip planes of the view */
extern s32 ClipPoly_ClipPlane(void *poly, EftAdVec *plane, s32 count); /* clips a polygon in place, new count */
extern void ClipPoly_ProjectCur(EftAdScr *scr, EftAdVec *stq, void *poly, s32 count); /* projects a polygon */
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 type);
extern s32 BtlObj_Create(s32 type, void *res, s32 active);
extern s32 BtlObj_Destroy(s32 id);
extern EftAdObj *BtlObj_Get(s32 id);
extern void BtlObjXf_Update(EftAdObj *obj);                   /* rebuilds the object's matrix from pos / rot */
extern void BtlObjAnim_PlayModel(EftAdObj *obj, s32 anim, s32 mode); /* starts animation 0..7 of the model */
extern s32 BtlObjAnim_GetMode(EftAdObj *obj);                    /* the animation's play mode (0 = none) */
extern void BtlObjXf_SetMtx(EftAdObj *obj, Mtx44 *m);         /* sets the object's matrix and its inverse */

/* A vertex handed to the clipper and the triangle writers. */
typedef struct EftMeshOut {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 uv;
    /* 0x20 */ Vec4 color;
} EftMeshOut; /* 0x30 */

s32 EftMesh_IsOffScreen(EftAdScr p);
void EftMesh_DrawTriClip(EftMeshOut *v, s32 hasTex, s32 blend, s32 flag1, s32 flag2, s32 zflip, u64 tex0, s32 zOfs);
void EftMesh_DrawNowTriClip(EftMeshOut *v, u64 tex0, s32 abe);
void EftMesh_QueueTri(EftAdScr *p0, EftAdScr *p1, EftAdScr *p2, EftAdVec *c0, EftAdVec *c1, EftAdVec *c2,
                      EftAdVec *t0, EftAdVec *t1, EftAdVec *t2, s32 flag1, s32 flag2, s32 hasTex, s32 blend, s32 z,
                      u64 tex0);
void EftMesh_SendTri(EftAdScr *p0, EftAdScr *p1, EftAdScr *p2, EftAdVec *c0, EftAdVec *c1, EftAdVec *c2,
                     EftAdVec *t0, EftAdVec *t1, EftAdVec *t2, u64 tex0, s32 abe);
void EftMesh_DrawClip(EftMesh *mesh);
void EftMesh_DrawNowClip(EftMesh *mesh, s32 abe);
void EftMesh_DrawPlain(EftMesh *mesh);
void EftMesh_DrawNowPlain(EftMesh *mesh, s32 abe);

/* Binds a mesh instance to mesh data: identity matrix, white, uv scale 1, no owner. */
void EftMesh_Init(EftMesh *mesh, EftMeshData *data) {
    memset(mesh, 0, sizeof(EftMesh));
    mesh->data = data;
    mesh->groups = data->groups;
    mesh->tris = (EftMeshTri *)((u8 *)data + data->trisOfs);
    mesh->verts = (EftMeshVtx *)((u8 *)data + data->vtxOfs);
    mesh->r = 0x80;
    mesh->g = 0x80;
    mesh->b = 0x80;
    mesh->a = 0x80;
    mesh->uvScale = 1.0f;
    mesh->type = -1;
    mesh->objId = -1;
    Mtx_StoreIdentity(&mesh->mtx);
}

/* Sets the texture table. */
void EftMesh_SetTex(EftMesh *mesh, EftAdTex *tex) {
    mesh->tex = tex;
}

/* Sets the model-to-world matrix. */
void EftMesh_SetMtx(EftMesh *mesh, Mtx44 *m) {
    Mtx_Copy(&mesh->mtx, m);
}

/* Copies an instance. */
void EftMesh_Copy(EftMesh *dst, EftMesh *src) {
    *dst = *src;
}

/* Sets the layer handed to the triangle writer (0 / 1: the two lists of an order table slot; 2 / 3 the same
   with the other GS context; < 0: layer 0 without alpha blending). */
void EftMesh_SetLayer(EftMesh *mesh, s32 blend) {
    mesh->blend = blend;
}

/* Sets the texture index offset. */
void EftMesh_SetTexBase(EftMesh *mesh, s32 base) {
    mesh->texBase = base;
}

/* Clip every triangle against the view volume (off: triangles with a vertex off screen are dropped). */
void EftMesh_SetClip(EftMesh *mesh, s32 on) {
    mesh->flags &= ~EFT_MESH_CLIP;
    if (on) {
        mesh->flags |= EFT_MESH_CLIP;
    }
}

/* Sets flag 1: every triangle is preceded by a packet that sets three GS registers (see EftMesh_QueueTri). */
void EftMesh_SetRepeat(EftMesh *mesh, s32 on) {
    mesh->flags &= ~EFT_MESH_REPEAT;
    if (on) {
        mesh->flags |= EFT_MESH_REPEAT;
    }
}

/* Sets flag 2 (handed to the triangle writer, which ignores it). No caller. */
void EftMesh_SetFlag2(EftMesh *mesh, s32 on) {
    mesh->flags &= ~EFT_MESH_FLAG2;
    if (on) {
        mesh->flags |= EFT_MESH_FLAG2;
    }
}

/* Reverses the sort depth. No caller. */
void EftMesh_SetZFlip(EftMesh *mesh, s32 on) {
    mesh->flags &= ~EFT_MESH_ZFLIP;
    if (on) {
        mesh->flags |= EFT_MESH_ZFLIP;
    }
}

/* Sets the sort depth offset. No caller. */
void EftMesh_SetZOfs(EftMesh *mesh, s32 ofs) {
    mesh->zOfs = ofs;
}

/* Sets the uv offset (texture scroll). */
void EftMesh_SetUvOfs(EftMesh *mesh, f32 u, f32 v) {
    mesh->uvOfs.x = u;
    mesh->uvOfs.y = v;
    mesh->uvOfs.z = 1.0f;
    mesh->uvOfs.w = 0.0f;
}

/* Sets the uv scale. */
void EftMesh_SetUvScale(EftMesh *mesh, f32 scale) {
    mesh->uvScale = scale;
}

/* Sets the colour that multiplies the vertex colours (0x80 = 1.0). */
void EftMesh_SetColor(EftMesh *mesh, u8 r, u8 g, u8 b, u8 a) {
    mesh->r = r;
    mesh->g = g;
    mesh->b = b;
    mesh->a = a;
}

/* Sets the fighter and effect type the mesh is hidden with (BtlScene_IsEffectHidden). */
void EftMesh_SetOwner(EftMesh *mesh, s32 objId, s32 type) {
    mesh->objId = objId;
    mesh->type = type;
}

/* Queues the mesh's triangles in the order table, unless its owner's effects of this type are hidden. */
void EftMesh_Draw(EftMesh *mesh) {
    if (mesh->type >= 0 && BtlScene_IsEffectHidden(mesh->objId, mesh->type)) {
        return;
    }
    if (mesh->flags & EFT_MESH_CLIP) {
        EftMesh_DrawClip(mesh);
    } else {
        EftMesh_DrawPlain(mesh);
    }
}

/* Sends the mesh's triangles to the GS at once (abe: alpha blending on), with the same hidden test. */
void EftMesh_DrawNow(EftMesh *mesh, s32 abe) {
    if (mesh->type >= 0 && BtlScene_IsEffectHidden(mesh->objId, mesh->type)) {
        return;
    }
    if (mesh->flags & EFT_MESH_CLIP) {
        EftMesh_DrawNowClip(mesh, abe);
    } else {
        EftMesh_DrawNowPlain(mesh, abe);
    }
}

/* Draws every triangle of the mesh through the clipper, queued in the order table. */
void EftMesh_DrawClip(EftMesh *mesh) {
    EftMeshOut v[9];
    EftAdVec color;
    EftAdVec ofs;
    EftMeshTri *tris;
    EftAdTex *tex;
    s32 blend;
    s32 zOfs;
    s32 flag1;
    s32 flag2;
    s32 zflip;
    u64 tex0 = 0;
    s32 n;
    s32 g;
    s32 i;
    EftMeshGroup *group;
    EftMeshVtx *verts;
    EftMeshTri *tri;
    EftMeshVtx *a;
    EftMeshVtx *b;
    EftMeshVtx *c;
    s32 hasTex;
    f32 scale;

    color.x = mesh->r * 0.0078125f;
    color.y = mesh->g * 0.0078125f;
    color.z = mesh->b * 0.0078125f;
    color.w = mesh->a * 0.0078125f;
    ofs.x = mesh->uvOfs.x;
    ofs.y = mesh->uvOfs.y;
    ofs.z = 0.0f;
    ofs.w = 0.0f;
    flag1 = 0;
    if (mesh->flags & EFT_MESH_REPEAT) {
        flag1 = 1;
    }
    flag2 = (mesh->flags >> 1) & 1;
    zflip = (mesh->flags >> 2) & 1;
    n = mesh->data->numGroups;
    group = mesh->groups;
    tris = mesh->tris;
    verts = mesh->verts;
    tex = mesh->tex;
    blend = mesh->blend;
    zOfs = mesh->zOfs;
    scale = mesh->uvScale;
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    for (g = 0; g < n; g++) {
        tri = &tris[group->firstTri];
        for (i = 0; i < group->numTris; i++) {
            a = &verts[tri->v[0]];
            b = &verts[tri->v[1]];
            c = &verts[tri->v[2]];
            Mtx_MulVec4(&v[0].pos, &mesh->mtx, &a->pos);
            Mtx_MulVec4(&v[1].pos, &mesh->mtx, &b->pos);
            Mtx_MulVec4(&v[2].pos, &mesh->mtx, &c->pos);
            Vec4_Scale(&v[0].uv, &a->uv, scale);
            Vec4_Scale(&v[1].uv, &b->uv, scale);
            Vec4_Scale(&v[2].uv, &c->uv, scale);
            Vec4_Add(&v[0].uv, &v[0].uv, (Vec4 *)&ofs);
            Vec4_Add(&v[1].uv, &v[1].uv, (Vec4 *)&ofs);
            Vec4_Add(&v[2].uv, &v[2].uv, (Vec4 *)&ofs);
            Vec4_Mul(&v[0].color, &a->color, (Vec4 *)&color);
            Vec4_Mul(&v[1].color, &b->color, (Vec4 *)&color);
            Vec4_Mul(&v[2].color, &c->color, (Vec4 *)&color);
            hasTex = 0;
            if (tri->tex >= 0 && tex != NULL) {
                tex0 = *(u64 *)((u8 *)tex + ((tri->tex + mesh->texBase) << 4));
                hasTex = 1;
            }
            tri++;
            v[0].uv.z = 1.0f;
            v[0].uv.w = 0.0f;
            v[1].uv.z = 1.0f;
            v[1].uv.w = 0.0f;
            v[2].uv.z = 1.0f;
            v[2].uv.w = 0.0f;
            EftMesh_DrawTriClip(v, hasTex, blend, flag1, flag2, zflip, tex0, zOfs);
        }
        group++;
    }
    Vu0Cur_Pop();
}

/* Draws every triangle of the mesh through the clipper, sent to the GS at once. Always textured. */
void EftMesh_DrawNowClip(EftMesh *mesh, s32 abe) {
    EftMeshOut v[9];
    EftAdVec color;
    EftAdVec ofs;
    EftMeshTri *tris;
    EftAdTex *tex;
    s32 n;
    s32 g;
    s32 i;
    EftMeshGroup *group;
    EftMeshVtx *verts;
    EftMeshTri *tri;
    EftMeshVtx *a;
    EftMeshVtx *b;
    EftMeshVtx *c;
    u64 tex0;
    f32 scale;

    color.x = mesh->r * 0.0078125f;
    color.y = mesh->g * 0.0078125f;
    color.z = mesh->b * 0.0078125f;
    color.w = mesh->a * 0.0078125f;
    ofs.x = mesh->uvOfs.x;
    ofs.y = mesh->uvOfs.y;
    ofs.z = 0.0f;
    ofs.w = 0.0f;
    n = mesh->data->numGroups;
    group = mesh->groups;
    tris = mesh->tris;
    verts = mesh->verts;
    tex = mesh->tex;
    scale = mesh->uvScale;
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    for (g = 0; g < n; g++) {
        tri = &tris[group->firstTri];
        for (i = 0; i < group->numTris; i++) {
            a = &verts[tri->v[0]];
            b = &verts[tri->v[1]];
            c = &verts[tri->v[2]];
            Mtx_MulVec4(&v[0].pos, &mesh->mtx, &a->pos);
            Mtx_MulVec4(&v[1].pos, &mesh->mtx, &b->pos);
            Mtx_MulVec4(&v[2].pos, &mesh->mtx, &c->pos);
            Vec4_Scale(&v[0].uv, &a->uv, scale);
            Vec4_Scale(&v[1].uv, &b->uv, scale);
            Vec4_Scale(&v[2].uv, &c->uv, scale);
            Vec4_Add(&v[0].uv, &v[0].uv, (Vec4 *)&ofs);
            Vec4_Add(&v[1].uv, &v[1].uv, (Vec4 *)&ofs);
            Vec4_Add(&v[2].uv, &v[2].uv, (Vec4 *)&ofs);
            Vec4_Mul(&v[0].color, &a->color, (Vec4 *)&color);
            Vec4_Mul(&v[1].color, &b->color, (Vec4 *)&color);
            Vec4_Mul(&v[2].color, &c->color, (Vec4 *)&color);
            tex0 = *(u64 *)((u8 *)tex + ((tri->tex + mesh->texBase) << 4));
            tri++;
            v[0].uv.z = 1.0f;
            v[0].uv.w = 0.0f;
            v[1].uv.z = 1.0f;
            v[1].uv.w = 0.0f;
            v[2].uv.z = 1.0f;
            v[2].uv.w = 0.0f;
            EftMesh_DrawNowTriClip(v, tex0, abe);
        }
        group++;
    }
    Vu0Cur_Pop();
}

/* Draws every triangle of the mesh that is wholly on screen, queued in the order table by its first vertex's
   depth. */
void EftMesh_DrawPlain(EftMesh *mesh) {
    EftAdScr scr[3];
    EftAdVec col[3];
    EftAdVec color;
    EftAdVec pos[3];
    EftAdVec uv[3];
    EftAdVec stq[3];
    EftAdVec ofs;
    EftMeshTri *tris;
    EftAdTex *tex;
    s32 blend;
    s32 zOfs;
    s32 flag1;
    s32 flag2;
    s32 zflip;
    u64 tex0 = 0;
    s32 n;
    s32 g;
    s32 i;
    EftMeshGroup *group;
    EftMeshVtx *verts;
    EftMeshTri *tri;
    EftMeshVtx *a;
    EftMeshVtx *b;
    EftMeshVtx *c;
    s32 hasTex;
    s32 z;
    f32 scale;

    color.x = mesh->r * 0.0078125f;
    color.y = mesh->g * 0.0078125f;
    color.z = mesh->b * 0.0078125f;
    color.w = mesh->a * 0.0078125f;
    ofs.x = mesh->uvOfs.x;
    ofs.y = mesh->uvOfs.y;
    ofs.z = 0.0f;
    ofs.w = 0.0f;
    flag1 = 0;
    if (mesh->flags & EFT_MESH_REPEAT) {
        flag1 = 1;
    }
    flag2 = (mesh->flags >> 1) & 1;
    zflip = (mesh->flags >> 2) & 1;
    n = mesh->data->numGroups;
    group = mesh->groups;
    tris = mesh->tris;
    verts = mesh->verts;
    tex = mesh->tex;
    blend = mesh->blend;
    zOfs = mesh->zOfs;
    scale = mesh->uvScale;
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    for (g = 0; g < n; g++) {
        tri = &tris[group->firstTri];
        for (i = 0; i < group->numTris; i++) {
            a = &verts[tri->v[0]];
            b = &verts[tri->v[1]];
            c = &verts[tri->v[2]];
            Mtx_MulVec4((Vec4 *)&pos[0], &mesh->mtx, &a->pos);
            Mtx_MulVec4((Vec4 *)&pos[1], &mesh->mtx, &b->pos);
            Mtx_MulVec4((Vec4 *)&pos[2], &mesh->mtx, &c->pos);
            Vec4_Scale((Vec4 *)&uv[0], &a->uv, scale);
            Vec4_Scale((Vec4 *)&uv[1], &b->uv, scale);
            Vec4_Scale((Vec4 *)&uv[2], &c->uv, scale);
            Vec4_Add((Vec4 *)&uv[0], (Vec4 *)&uv[0], (Vec4 *)&ofs);
            Vec4_Add((Vec4 *)&uv[1], (Vec4 *)&uv[1], (Vec4 *)&ofs);
            Vec4_Add((Vec4 *)&uv[2], (Vec4 *)&uv[2], (Vec4 *)&ofs);
            Vec4_Mul((Vec4 *)&col[0], &a->color, (Vec4 *)&color);
            Vec4_Mul((Vec4 *)&col[1], &b->color, (Vec4 *)&color);
            Vec4_Mul((Vec4 *)&col[2], &c->color, (Vec4 *)&color);
            uv[0].z = 1.0f;
            uv[0].w = 0.0f;
            uv[1].z = 1.0f;
            uv[1].w = 0.0f;
            uv[2].z = 1.0f;
            uv[2].w = 0.0f;
            Vu0Cur_ProjectPointsStq(scr, (Vec4 *)stq, (Vec4 *)pos, (Vec4 *)uv, 3);
            z = scr[0].z >> 8;
            if (zflip) {
                z = 0x1000 - z;
            }
            if ((u32)z < 0x1000 && !EftMesh_IsOffScreen(scr[0]) && !EftMesh_IsOffScreen(scr[1]) &&
                !EftMesh_IsOffScreen(scr[2])) {
                hasTex = 0;
                if (tri->tex >= 0 && tex != NULL) {
                    tex0 = *(u64 *)((u8 *)tex + ((tri->tex + mesh->texBase) << 4));
                    hasTex = 1;
                }
                EftMesh_QueueTri(&scr[0], &scr[1], &scr[2], &col[0], &col[1], &col[2], &stq[0], &stq[1], &stq[2],
                                 flag1, flag2, hasTex, blend, z + zOfs, tex0);
            }
            tri++;
        }
        group++;
    }
    Vu0Cur_Pop();
}

/* Draws every triangle of the mesh that is wholly on screen, sent to the GS at once. Always textured. */
void EftMesh_DrawNowPlain(EftMesh *mesh, s32 abe) {
    EftAdScr scr[3];
    EftAdVec col[3];
    EftAdVec color;
    EftAdVec pos[3];
    EftAdVec uv[3];
    EftAdVec stq[3];
    EftAdVec ofs;
    EftMeshTri *tris;
    EftAdTex *tex;
    s32 n;
    s32 g;
    s32 i;
    EftMeshGroup *group;
    EftMeshVtx *verts;
    EftMeshTri *tri;
    EftMeshVtx *a;
    EftMeshVtx *b;
    EftMeshVtx *c;
    u64 tex0;
    f32 scale;

    color.x = mesh->r * 0.0078125f;
    color.y = mesh->g * 0.0078125f;
    color.z = mesh->b * 0.0078125f;
    color.w = mesh->a * 0.0078125f;
    ofs.x = mesh->uvOfs.x;
    ofs.y = mesh->uvOfs.y;
    ofs.z = 0.0f;
    ofs.w = 0.0f;
    n = mesh->data->numGroups;
    group = mesh->groups;
    tris = mesh->tris;
    verts = mesh->verts;
    tex = mesh->tex;
    scale = mesh->uvScale;
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    for (g = 0; g < n; g++) {
        tri = &tris[group->firstTri];
        for (i = 0; i < group->numTris; i++) {
            a = &verts[tri->v[0]];
            b = &verts[tri->v[1]];
            c = &verts[tri->v[2]];
            Mtx_MulVec4((Vec4 *)&pos[0], &mesh->mtx, &a->pos);
            Mtx_MulVec4((Vec4 *)&pos[1], &mesh->mtx, &b->pos);
            Mtx_MulVec4((Vec4 *)&pos[2], &mesh->mtx, &c->pos);
            Vec4_Scale((Vec4 *)&uv[0], &a->uv, scale);
            Vec4_Scale((Vec4 *)&uv[1], &b->uv, scale);
            Vec4_Scale((Vec4 *)&uv[2], &c->uv, scale);
            Vec4_Add((Vec4 *)&uv[0], (Vec4 *)&uv[0], (Vec4 *)&ofs);
            Vec4_Add((Vec4 *)&uv[1], (Vec4 *)&uv[1], (Vec4 *)&ofs);
            Vec4_Add((Vec4 *)&uv[2], (Vec4 *)&uv[2], (Vec4 *)&ofs);
            Vec4_Mul((Vec4 *)&col[0], &a->color, (Vec4 *)&color);
            Vec4_Mul((Vec4 *)&col[1], &b->color, (Vec4 *)&color);
            Vec4_Mul((Vec4 *)&col[2], &c->color, (Vec4 *)&color);
            uv[0].z = 1.0f;
            uv[0].w = 0.0f;
            uv[1].z = 1.0f;
            uv[1].w = 0.0f;
            uv[2].z = 1.0f;
            uv[2].w = 0.0f;
            Vu0Cur_ProjectPointsStq(scr, (Vec4 *)stq, (Vec4 *)pos, (Vec4 *)uv, 3);
            if (!EftMesh_IsOffScreen(scr[0]) && !EftMesh_IsOffScreen(scr[1]) && !EftMesh_IsOffScreen(scr[2])) {
                tex0 = *(u64 *)((u8 *)tex + ((tri->tex + mesh->texBase) << 4));
                EftMesh_SendTri(&scr[0], &scr[1], &scr[2], &col[0], &col[1], &col[2], &stq[0], &stq[1], &stq[2],
                                tex0, abe);
            }
            tri++;
        }
        group++;
    }
    Vu0Cur_Pop();
}

/* Whether a projected point is outside the GS drawing area or behind the camera. A non-static inline of the
   original: the two functions after it have it inlined, and its out-of-line copy is emitted at the end of the
   object (0x1A9D38), where the two draw loops above call it. */
inline s32 EftMesh_IsOffScreen(EftAdScr p) {
    if (p.z <= 0) {
        return 1;
    }
    if (p.x > 0xFFEF) {
        return 1;
    }
    if (p.x <= 0) {
        return 1;
    }
    if (p.y > 0xFFEF) {
        return 1;
    }
    if (p.y <= 0) {
        return 1;
    }
    return 0;
}

/* Clips one triangle (v has room for the 9 vertices clipping can produce) against the five view planes,
   projects it and queues the resulting fan, each triangle at the mean depth of its three points. */
/* The fan's depth values are read through a pointer to scr[0].z plus the byte offset i << 4 (matching needs this
   form: indexing scr[i].z gives other induction variables). */
#define EFT_FAN_Z(zp, i) (*(s32 *)((u8 *)(zp) + ((i) << 4)))
void EftMesh_DrawTriClip(EftMeshOut *v, s32 hasTex, s32 blend, s32 flag1, s32 flag2, s32 zflip, u64 tex0, s32 zOfs) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftMeshOut portPoly[16];
    __builtin_memcpy(portPoly, v, 3 * sizeof(EftMeshOut));
    v = portPoly;
#endif
    EftAdScr scr[9];
    EftAdVec stq[9];
    EftAdVec *plane;
    s32 n;
    s32 i;
    s32 z;
    s32 *zp;

    plane = EftGfx_GetClipPlanes();
    n = 3;
    for (i = 0; i < 5; i++) {
        n = ClipPoly_ClipPlane(v, plane, n);
        plane++;
    }
    if (n == 0) {
        return;
    }
    ClipPoly_ProjectCur(scr, stq, v, n);
    zp = &scr[0].z;
    for (i = 2; i < n; i++) {
        z = ((scr[0].z + EFT_FAN_Z(zp, i - 1) + EFT_FAN_Z(zp, i)) / 3) >> 8;
        if (zflip) {
            z = 0x1000 - z;
        }
        if (scr[0].z > 0xFFFFFF) {
            scr[0].z = 0xFFFFFF;
        }
        if (EFT_FAN_Z(zp, i - 1) > 0xFFFFFF) {
            EFT_FAN_Z(zp, i - 1) = 0xFFFFFF;
        }
        if (EFT_FAN_Z(zp, i) > 0xFFFFFF) {
            EFT_FAN_Z(zp, i) = 0xFFFFFF;
        }
        if (EftMesh_IsOffScreen(scr[0]) && EftMesh_IsOffScreen(scr[i - 1]) && EftMesh_IsOffScreen(scr[i])) {
            continue;
        }
        EftMesh_QueueTri(&scr[0], &scr[i - 1], &scr[i], (EftAdVec *)&v[0].color, (EftAdVec *)&v[i - 1].color,
                         (EftAdVec *)&v[i].color, &stq[0], &stq[i - 1], &stq[i], flag1, flag2, hasTex, blend,
                         z + zOfs, tex0);
    }
}

/* The same, with each triangle of the fan sent to the GS at once. */
/* Note the screen tests: this variant tests points 0, 1 and 2 for every triangle of the fan, not 0, i - 1 and i
   (fixed stack offsets 0x20 / 0x30 / 0x40 in the original). */
void EftMesh_DrawNowTriClip(EftMeshOut *v, u64 tex0, s32 abe) {
#ifdef PORT
    /* The triangle is clipped in place and can grow to eight corners, but callers hand over room for three. On
       the PS2 the extra corners land in the caller's other locals; on PC they reach the return address (the
       crash in EftRibbon_DrawStrip). The work is done on a copy with room. */
    EftMeshOut portPoly[16];
    __builtin_memcpy(portPoly, v, 3 * sizeof(EftMeshOut));
    v = portPoly;
#endif
    EftAdScr scr[9];
    EftAdVec stq[9];
    EftAdVec *plane;
    s32 n;
    s32 i;
    s32 *zp;

    plane = EftGfx_GetClipPlanes();
    n = 3;
    for (i = 0; i < 5; i++) {
        n = ClipPoly_ClipPlane(v, plane, n);
        plane++;
    }
    if (n == 0) {
        return;
    }
    ClipPoly_ProjectCur(scr, stq, v, n);
    zp = &scr[0].z;
    for (i = 2; i < n; i++) {
        if (scr[0].z > 0xFFFFFF) {
            scr[0].z = 0xFFFFFF;
        }
        if ((&scr[i - 1].z)[0] > 0xFFFFFF) {
            (&scr[i - 1].z)[0] = 0xFFFFFF;
        }
        if (EFT_FAN_Z(zp, i) > 0xFFFFFF) {
            EFT_FAN_Z(zp, i) = 0xFFFFFF;
        }
        if (EftMesh_IsOffScreen(scr[0]) && EftMesh_IsOffScreen(scr[1]) && EftMesh_IsOffScreen(scr[2])) {
            continue;
        }
        EftMesh_SendTri(&scr[0], &scr[i - 1], &scr[i], (EftAdVec *)&v[0].color, (EftAdVec *)&v[i - 1].color,
                        (EftAdVec *)&v[i].color, &stq[0], &stq[i - 1], &stq[i], tex0, abe);
    }
}

/* ---- effect model objects --------------------------------------------------------------------------------- */

/* Makes a battle object (type 1, active) from a model that is already in memory: fills the caller's resource
   record with the model pointer and hands it to BtlObj_Create. Returns the object id, -1 when the 12 objects
   are all in use. */
s32 EftObj_Create(EftObjRes *res, void *model) {
    s32 id;

    memset(res, 0, sizeof(EftObjRes));
    res->model = model;
    res->size = 0x20;
    id = BtlObj_Create(1, res, 1);
    if (id < 0) {
        id = -1;
    }
    return id;
}

/* Releases the battle object. */
s32 EftObj_Destroy(s32 id) {
    return BtlObj_Destroy(id);
}

/* Writes the object's position (used by EftObj_UpdateMtx). No caller. */
void EftObj_SetPos(s32 id, Vec4 *pos) {
    Vec4_Copy(&BtlObj_Get(id)->pos, pos);
}

/* Rebuilds the object's matrix from its position and rotation. No caller. */
void EftObj_UpdateMtx(s32 id) {
    BtlObjXf_Update(BtlObj_Get(id));
}

/* Starts one of the model's animations. */
void EftObj_PlayAnim(s32 id, s32 anim, s32 mode) {
    BtlObjAnim_PlayModel(BtlObj_Get(id), anim, mode);
}

/* Whether an animation is playing. */
s32 EftObj_IsAnimPlaying(s32 id) {
    return BtlObjAnim_GetMode(BtlObj_Get(id)) != 0;
}

/* Writes the object's rotation (used by EftObj_UpdateMtx). No caller. */
void EftObj_SetRot(s32 id, Vec4 *rot) {
    Vec4_Copy(&BtlObj_Get(id)->rot, rot);
}

/* Poses the object: sets its world matrix. */
void EftObj_SetMtx(s32 id, Mtx44 *m) {
    BtlObjXf_SetMtx(BtlObj_Get(id), m);
}

/* Shows or hides the object (the battle object's "drawn" flag). */
void EftObj_SetVisible(s32 id, s32 on) {
    EftAdObj *obj = BtlObj_Get(id);

    if (on) {
        obj->flags |= 2;
    } else {
        obj->flags &= ~2;
    }
}

/* Nothing (called right after EftObj_Create by every user). */
void EftObj_Nop(void) {
}

/* Queues one gouraud triangle in the order table at depth z, list `layer` (0 / 1; 2 / 3 = the same lists with
   GS context 2; < 0 = list 0 without alpha blending). Nothing when all three alphas are below 0.1. With `repeat`
   a packet that sets CLAMP_1 and CLAMP_2 to 0 (texture repeat) is queued in front. Textured (hasTex) the packet
   is PRIM, TEX0, 3 x (RGBAQ, ST, XYZF2); untextured PRIM, 3 x (RGBAQ, XYZF2). flag2 is not used. */
/* Matching needs: the list index l and the entry pointer e as block-local variables of each of the three blocks
   (function-scope ones merge the three order-table tails completely), ctx as an int widened separately for PRIM
   and shifted as an int for the register list, and the header stores in this order. */
void EftMesh_QueueTri(EftAdScr *p0, EftAdScr *p1, EftAdScr *p2, EftAdVec *c0, EftAdVec *c1, EftAdVec *c2,
                      EftAdVec *t0, EftAdVec *t1, EftAdVec *t2, s32 repeat, s32 flag2, s32 hasTex, s32 layer, s32 z,
                      u64 tex0) {
    s64 abe = 1;

    if (c0->w < 0.1f && c1->w < 0.1f && c2->w < 0.1f) {
        return;
    }
    if (layer < 0) {
        layer = 0;
        abe = 0;
    }
    if (repeat) {
        EftMeshClampPkt *p;
        s32 l;
        EftAdOtEntry *e;

        l = layer;
        if (l >= 2) {
            l -= 2;
        }
        if (z < 0) {
            e = &gOtZ[0].layer[l];
        } else if (z >= 0x1000) {
            e = &gOtZ[0xFFF].layer[l];
        } else {
            e = &gOtZ[z].layer[l];
        }
        p = (EftMeshClampPkt *)gOtCur;
        gOtCur = (u8 *)(p + 1);
        p->dmaTag = 0x20000003;
        p->vif0 = 0x10000000;
        p->vif1 = 0x50000003;
        p->gifTag = 0x1000000000008002;
        p->regs = 0xE;
        p->addr0 = 8;
        p->addr1 = 9;
        p->next = NULL;
        p->data0 = 0;
        p->data1 = 0;
        e->tail->next = p;
        e->tail = (EftAdOtPrim *)p;
    }
    if (hasTex) {
        EftMeshTexTriPkt *p = (EftMeshTexTriPkt *)gOtCur;
        s32 ctx;
        s32 l;
        EftAdOtEntry *e;

        gOtCur = (u8 *)(p + 1);
        ctx = layer >= 2;
        p->prim = ((s64)abe << 6) | ((s64)ctx << 9) | 0x1B;
        p->dmaTag = 0x20000007;
        p->vif0 = 0x10000000;
        p->vif1 = 0x50000007;
        p->gifTag = 0xC400000000008001;
        p->regs = (s64)(ctx << 4) + 0xF42142142160;
        p->next = NULL;
        p->nop = 0;
        p->v[0].rgbaq.r = c0->x;
        p->v[0].rgbaq.g = c0->y;
        p->v[0].rgbaq.b = c0->z;
        p->v[0].rgbaq.a = c0->w;
        p->v[0].rgbaq.q = t0->z;
        p->v[1].rgbaq.r = c1->x;
        p->v[1].rgbaq.g = c1->y;
        p->v[1].rgbaq.b = c1->z;
        p->v[1].rgbaq.a = c1->w;
        p->v[1].rgbaq.q = t1->z;
        p->v[2].rgbaq.r = c2->x;
        p->v[2].rgbaq.g = c2->y;
        p->v[2].rgbaq.b = c2->z;
        p->v[2].rgbaq.a = c2->w;
        p->v[2].rgbaq.q = t2->z;
        p->tex0 = tex0;
        p->v[0].st.s = t0->x;
        p->v[0].st.t = t0->y;
        p->v[1].st.s = t1->x;
        p->v[1].st.t = t1->y;
        p->v[2].st.s = t2->x;
        p->v[2].st.t = t2->y;
        p->v[0].xyz.x = p0->x;
        p->v[0].xyz.y = p0->y;
        p->v[0].xyz.z = p0->z;
        p->v[0].xyz.f = 0xFF;
        p->v[1].xyz.x = p1->x;
        p->v[1].xyz.y = p1->y;
        p->v[1].xyz.z = p1->z;
        p->v[1].xyz.f = 0xFF;
        p->v[2].xyz.x = p2->x;
        p->v[2].xyz.y = p2->y;
        p->v[2].xyz.z = p2->z;
        p->v[2].xyz.f = 0xFF;
        l = layer;
        if (l >= 2) {
            l -= 2;
        }
        if (z < 0) {
            e = &gOtZ[0].layer[l];
        } else if (z >= 0x1000) {
            e = &gOtZ[0xFFF].layer[l];
        } else {
            e = &gOtZ[z].layer[l];
        }
        e->tail->next = p;
        e->tail = (EftAdOtPrim *)p;
    } else {
        EftMeshTriPkt *p = (EftMeshTriPkt *)gOtCur;
        s32 l;
        EftAdOtEntry *e;

        gOtCur = (u8 *)(p + 1);
        p->prim = ((s64)abe << 6) | 0xB;
        p->dmaTag = 0x20000005;
        p->vif0 = 0x10000000;
        p->vif1 = 0x50000005;
        p->gifTag = 0x8400000000008001;
        p->regs = 0xF4141410;
        p->next = NULL;
        p->v[0].rgbaq.r = c0->x;
        p->v[0].rgbaq.g = c0->y;
        p->v[0].rgbaq.b = c0->z;
        p->v[0].rgbaq.a = c0->w;
        p->v[0].rgbaq.q = 1.0f;
        p->v[1].rgbaq.r = c1->x;
        p->v[1].rgbaq.g = c1->y;
        p->v[1].rgbaq.b = c1->z;
        p->v[1].rgbaq.a = c1->w;
        p->v[1].rgbaq.q = 1.0f;
        p->v[2].rgbaq.r = c2->x;
        p->v[2].rgbaq.g = c2->y;
        p->v[2].rgbaq.b = c2->z;
        p->v[2].rgbaq.a = c2->w;
        p->v[2].rgbaq.q = 1.0f;
        p->v[0].xyz.x = p0->x;
        p->v[0].xyz.y = p0->y;
        p->v[0].xyz.z = p0->z;
        p->v[0].xyz.f = 0xFF;
        p->v[1].xyz.x = p1->x;
        p->v[1].xyz.y = p1->y;
        p->v[1].xyz.z = p1->z;
        p->v[1].xyz.f = 0xFF;
        p->v[2].xyz.x = p2->x;
        p->v[2].xyz.y = p2->y;
        p->v[2].xyz.z = p2->z;
        p->v[2].xyz.f = 0xFF;
        l = layer;
        if (l >= 2) {
            l -= 2;
        }
        if (z < 0) {
            e = &gOtZ[0].layer[l];
        } else if (z >= 0x1000) {
            e = &gOtZ[0xFFF].layer[l];
        } else {
            e = &gOtZ[z].layer[l];
        }
        e->tail->next = p;
        e->tail = (EftAdOtPrim *)p;
    }
}

/* Sends one textured gouraud triangle to the GS at once (the same packet as the textured case above, built on
   the stack and copied into a direct DMA transfer). Nothing when all three alphas are below 0.1. */
/* abe goes through an s8 local (set after the DMA call) before it is widened: that gives the original's
   sll / sra ahead of the 64-bit shift pair. */
void EftMesh_SendTri(EftAdScr *p0, EftAdScr *p1, EftAdScr *p2, EftAdVec *c0, EftAdVec *c1, EftAdVec *c2,
                     EftAdVec *t0, EftAdVec *t1, EftAdVec *t2, u64 tex0, s32 abe) {
    EftMeshTexTriPkt pkt;
    u8 *dst;
    s8 a;

    if (c0->w < 0.1f && c1->w < 0.1f && c2->w < 0.1f) {
        return;
    }
    dst = Dma_BeginDirect();
    a = abe;
    pkt.prim = ((s64)(s8)a << 6) | 0x1B;
    pkt.dmaTag = 0x20000007;
    pkt.vif0 = 0x10000000;
    pkt.vif1 = 0x50000007;
    pkt.gifTag = 0xC400000000008001;
    pkt.regs = 0xF42142142160;
    pkt.next = NULL;
    pkt.nop = 0;
    pkt.v[0].rgbaq.r = c0->x;
    pkt.v[0].rgbaq.g = c0->y;
    pkt.v[0].rgbaq.b = c0->z;
    pkt.v[0].rgbaq.a = c0->w;
    pkt.v[0].rgbaq.q = t0->z;
    pkt.v[1].rgbaq.r = c1->x;
    pkt.v[1].rgbaq.g = c1->y;
    pkt.v[1].rgbaq.b = c1->z;
    pkt.v[1].rgbaq.a = c1->w;
    pkt.v[1].rgbaq.q = t1->z;
    pkt.v[2].rgbaq.r = c2->x;
    pkt.v[2].rgbaq.g = c2->y;
    pkt.v[2].rgbaq.b = c2->z;
    pkt.v[2].rgbaq.a = c2->w;
    pkt.v[2].rgbaq.q = t2->z;
    pkt.tex0 = tex0;
    pkt.v[0].st.s = t0->x;
    pkt.v[0].st.t = t0->y;
    pkt.v[1].st.s = t1->x;
    pkt.v[1].st.t = t1->y;
    pkt.v[2].st.s = t2->x;
    pkt.v[2].st.t = t2->y;
    pkt.v[0].xyz.x = p0->x;
    pkt.v[0].xyz.y = p0->y;
    pkt.v[0].xyz.z = p0->z;
    pkt.v[0].xyz.f = 0xFF;
    pkt.v[1].xyz.x = p1->x;
    pkt.v[1].xyz.y = p1->y;
    pkt.v[1].xyz.z = p1->z;
    pkt.v[1].xyz.f = 0xFF;
    pkt.v[2].xyz.x = p2->x;
    pkt.v[2].xyz.y = p2->y;
    pkt.v[2].xyz.z = p2->z;
    pkt.v[2].xyz.f = 0xFF;
    memcpy(dst, &pkt.gifTag, (u16)pkt.dmaTag << 4);
    Dma_EndDirect(dst + ((u16)pkt.dmaTag << 4));
}
