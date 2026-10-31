#include "common.h"
#include "battle/battle.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/heap.h"
#include "sys/gfxm_e.h"

/*
 * Battle object model binding and ground shadow, 0x1131D8..0x114860. See include/sys/gfxm_e.h.
 *
 *   BtlObjMdl_*  what BtlObj_Setup / BtlObj_Destroy (btl_obj.c) call to bind an object to its files
 *   ObjShadow_*  the ground shadow: the model is drawn from straight above into a 256x256 work page
 *                (page 0x150 = texture block 0x2A00), and that picture is projected on the stage triangles
 *                under the object, collected by StgShadow_Collect, with VU1 program 6
 *
 * Everything here is drawing or set-up. Nothing reads a pad, the clock or a random generator.
 *
 * ObjShadow_BuildPacket is INCLUDE_ASM with a behaviourally exact attempt in `#if 0` above it; the 26 other
 * functions match. To check them with fdiff, run it on a copy of this file without the INCLUDE_ASM line (they
 * print OK with or without the attempt compiled). The constant 0.02 of ObjShadow_BuildPacket (0x2FC2CC) is the
 * word in front of this file's .lit4 (0x2FC2D0..0x2FC2FC) and stays in the assembly chunk.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern float sqrtf(float x);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Mtx_MakeCamera(Mtx44 *m, Vec4 *pos, Vec4 *zdir, Vec4 *ydir);
extern void Mtx_MakeViewScreen(Mtx44 *m, f32 scrz, f32 ax, f32 ay, f32 cx, f32 cy, f32 zmin, f32 zmax, f32 nearz,
                               f32 farz);
extern void Vu0Cur_LoadIdentity(void);
extern void Vu0Cur_RotateZXY(void *angles);
extern void Vu0Cur_StoreMtx(Mtx44 *m);
extern void Vu0Screen_StoreMtx(Mtx44 *m);
extern void Vu0Clip_StoreMtx(Mtx44 *m);

extern void ColBox_SetCenterHalf(ColBoxE *box, Vec4 *center, Vec4 *half);
extern void ColBox_GetCenter(ColBoxE *box, Vec4 *out);
extern void ColBox_GetHalf(ColBoxE *box, Vec4 *out);

/* btl_obj.c and the files after it. */
extern s32 BtlObjVis_AllocBit(s32 bit);
extern void BtlObjVis_FreeBit(s32 bit);
extern s32 BtlObjVis_AllocFreeBit(void);
extern void BtlObjLight_GetHalf(Vec4 *out);
extern void BtlObjLight_GetColor(Vec4 *out);
extern ObjMdlPart *BtlObjPoolE0_Alloc(void);
extern s32 BtlObjPoolE0_FreeList(SList *list);
extern ObjShadowPool *BtlObjPool3_Get(void);
extern ObjShadow *BtlObjPool3_Alloc(void);
extern s32 BtlObjPool3_Free(ObjShadow *node);
extern void BtlObj_BindTables(ObjMdlObj *obj);
extern void BtlObj_InitFlags(ObjMdlObj *obj);
extern void BtlObjXf_Reset(ObjMdlObj *obj);
extern void BtlObjBody_Init(ObjMdlObj *obj);
extern void BtlObj_CreateFace(ObjMdlObj *obj);
extern void BtlObj_FreeFace(ObjMdlObj *obj);
extern void BtlObjAnim_Init(ObjMdlObj *obj);
/* Model node: the matrix is at +0x10 (BtlObjPart). */
typedef struct ObjMdlNode {
    /* 0x00 */ u8 unk00[0x10];
    /* 0x10 */ Mtx44 mtx;
} ObjMdlNode;
extern ObjMdlNode *BtlObj_GetNode(ObjMdlObj *obj, s32 node);

/* Stage triangles under a box (eft_det_b). */
typedef struct ObjShadowOut {
    /* 0x0 */ f32 alpha;
    /* 0x4 */ s32 count;
} ObjShadowOut;
extern s32 StgShadow_Collect(s32 zone, ColBoxE *box, s32 max, ObjShadowTri *buf);
extern ObjShadowOut *StgShadow_GetOut(void);

/* VU1 packet layer (sys/vu1_packet.h). Program 2's constant block: matrices at +0xC0 and +0x100. */
typedef struct ObjVu1Prog2 {
    /* 0x000 */ u8 unk000[0xC0];
    /* 0x0C0 */ Mtx44 screen;
    /* 0x100 */ Mtx44 clip;
} ObjVu1Prog2;
extern ObjVu1Prog2 *Vu1Pkt_LoadProg2(s32 alt);
extern void Vu1Pkt_CallProg6(u32 *chain);
extern void Vu1Pkt_LoadProg6(void *view, Mtx44 *mtx, Vec4 *vec);

/* The object draw of the previous file (0x10EC18..0x112A30): queues the object's meshes. */
extern void ObjDraw_DrawPartsFlat(ObjMdlObj *obj);

void MdlTex_RebaseChain(s32 single, void *chain, s32 tbp, s32 cbp, s32 tbp2, s32 cbp2, u32 minTbp, u32 minCbp);

/* The battle view as ObjShadow_Draw reads it: its scissor rectangle. */
typedef struct ObjShadowCam {
    /* 0x000 */ u8 unk000[0x200];
    /* 0x200 */ s32 x0;
    /* 0x204 */ s32 x1;
    /* 0x208 */ s32 y0;
    /* 0x20C */ s32 y1;
} ObjShadowCam;

/* Four floats, 8-byte aligned: constant initialisers of this type are copied with ld / sd. */
typedef struct ObjVec8 {
    f32 x, y, z, w;
} __attribute__((aligned(8))) ObjVec8;

/* A plane (normal, distance) with the same layout. It has to be a type of its own: the original has two copies
   of the constant 0, -1, 0, 1 in its read-only data (0x2EB980 for the plane of ObjShadow_LoadProg, 0x2EB990 for
   the direction of ObjShadow_Update), and this compiler shares identical initialisers of one type. */
typedef struct ObjPlane8 {
    f32 a, b, c, d;
} __attribute__((aligned(8))) ObjPlane8;

#define GS_PRMODECONT 0x1A
#define GS_PRMODE 0x1B

/* Gives every material texture of the model its row of the alpha table: the alpha bytes of the first 16 CLUT
   entries become (slot + row * 15), and the slot's base alpha is copied into the model header. */
void BtlObjMdl_InitAlphaSlots(void *res, ObjMdl *mdl, s32 row) {
    ObjMdlFile *model = mdl->model;
    TexFile *tex = mdl->tex;
    s32 first;
    s32 count;
    s32 skip;
    s32 i;
    s32 j;

    row *= 15;
    skip = model->texNoAlpha;
    first = model->texFirst;
    count = model->texCount;
    if (skip == 0) {
        skip = -1;
    }
    memset(model->alpha, 0, 16);
    for (i = 0; (u32)i < tex->count; i++) {
        if (i >= first && i < first + count) {
            TexEntry *e = &tex->ent[i];
            s32 alpha = e->unk28[5];
            s32 slot = e->unk28[4];

            if (alpha > 0x80) {
                alpha = 0x7F;
            }
            if (skip == i) {
                alpha = 0;
            } else {
                u8 *p = (u8 *)e->clut;

                for (j = 15; j >= 0; j--) {
                    p[0x60 + j * 4 + 3] = slot + row;
                }
            }
            mdl->model->alpha[slot] = alpha;
        }
    }
}

/* Points the object at its model and texture files. The first time a model file is bound its texture
   references are moved to the object texture area and it claims an alpha row; later binds only re-claim the row. */
void BtlObjMdl_BindFiles(ObjMdlObj *obj) {
    ObjMdl *mdl = &obj->mdl;
    void *res = obj->res;
    s32 i;

    mdl->model = mdl->file;
    mdl->meshes = (ObjMdlMesh *)((u8 *)mdl->file + mdl->file->meshOfs);
    if (!mdl->file->bound) {
        mdl->file->tbp = 0x3480;
        mdl->model->cbp = 0x3C00;
        mdl->model->cbp2 = 0x3D00;
        mdl->model->tbp2 = 0x3D40;
        mdl->model->unk68 = 0x3E40;
        mdl->model->alphaRow = BtlObjVis_AllocFreeBit();
        MdlTex_RebaseChain(0, mdl->meshes, mdl->model->tbp, mdl->model->cbp, 0x3D40, mdl->model->cbp2, 0x7FFFFFFF,
                           0x7FFFFFFF);
        for (i = 0; i < 8; i++) {
            if (obj->mdl.extra[i] != NULL) {
                MdlTex_RebaseChain(1, obj->mdl.extra[i], mdl->model->tbp, mdl->model->cbp, 0x3D40, mdl->model->cbp2,
                                   0, 0);
            }
        }
        Res_RelocateOffsets(&mdl->tex, (u8 *)mdl->texFile, mdl->texFile);
        BtlObjMdl_InitAlphaSlots(res, mdl, mdl->model->alphaRow);
        mdl->model->bound = 1;
    } else {
        Res_RelocateOffsets(&obj->mdl.tex, (u8 *)mdl->texFile, mdl->texFile);
        BtlObjVis_AllocBit(mdl->model->alphaRow);
    }
}

/* Takes one part from the pool for every mesh record and remembers the first part of each node. */
void BtlObjMdl_AllocParts(ObjMdlObj *obj) {
    SList *list = &obj->parts.list;
    ObjMdlMesh *mesh = obj->mdl.meshes;
    ObjMdlPart *part;
    ObjMdlPart **slot;

    for (;;) {
        part = BtlObjPoolE0_Alloc();
        part->node = mesh->node;
        part->unk04 = 0;
        part->active = 0;
        SList_PushBack(list, &part->link);
        slot = obj->parts.first;
        slot += part->node;
        if (*slot == NULL) {
            *slot = part;
        }
        if (mesh->last != 0) {
            break;
        }
        mesh = (ObjMdlMesh *)((u8 *)mesh + mesh->next);
    }
}

/* Gives the object's parts back to the pool. */
s32 BtlObjMdl_FreeParts(ObjMdlObj *obj) {
    return BtlObjPoolE0_FreeList(&obj->parts.list);
}

/* Builds everything of an object whose type and resource slot are set. */
void BtlObjMdl_Init(ObjMdlObj *obj) {
    BtlObj_BindTables(obj);
#ifdef PORT
    {
        /* the model not read (its table empty): read again and bind again (btl_obj.c) */
        extern s32 BtlRes_PortReloadModel(void *slot);
        if (obj->mdl.file == NULL && BtlRes_PortReloadModel(obj->res)) {
            BtlObj_BindTables(obj);
        }
    }
#endif
    BtlObjMdl_BindFiles(obj);
    BtlObjMdl_AllocParts(obj);
    BtlObj_InitFlags(obj);
    BtlObjXf_Reset(obj);
    BtlObjBody_Init(obj);
    BtlObj_CreateFace(obj);
    ObjShadow_Alloc(obj);
    BtlObjAnim_Init(obj);
    obj->ready = 1;
}

/* Clears the object and builds it from a resource slot. */
void BtlObjMdl_Create(s32 type, ObjMdlObj *obj, void *res) {
    memset(obj, 0, sizeof(ObjMdlObj));
    obj->type = type;
    obj->res = res;
    BtlObjMdl_Init(obj);
}

/* Releases what BtlObjMdl_Init claimed and clears the object. Does nothing for an object that was never built. */
void BtlObjMdl_Destroy(ObjMdlObj *obj) {
    if (obj->ready != 0) {
        BtlObjVis_FreeBit(obj->mdl.model->alphaRow);
        BtlObjMdl_FreeParts(obj);
        BtlObj_FreeFace(obj);
        ObjShadow_Free(obj);
        memset(obj, 0, sizeof(ObjMdlObj));
    }
}

/* Size in words of the packet ObjShadow_BuildPacket makes for `count` triangles. */
s32 ObjShadow_CalcPacketWords(s32 count) {
    s32 i = 0;
    s32 words = 8;
    s32 j;
    s32 pad;
    s32 k;

    do {
        words += 14;
        j = 0;
        do {
            words += 0x30;
            i++;
            j++;
        } while (i < count && j < 18);
        words += 1;
    } while (i < count);
    pad = words % 4;
    if (pad != 0) {
        for (k = 0; k < 4 - pad; k++) {
            words++;
        }
    }
    return words + 4;
}

/* Builds the VU1 packet that draws the collected triangles with the shadow texture: batches of up to 18
   triangles, every vertex lifted 0.02 along the triangle's normal, with the normal and the colour. Returns 1.
   Layout: a DMA "ret" tag whose count is filled in at the end; one quadword unpacked at TOPS; per batch a
   three-quadword header at TOPS + 0 (GIF tags; the triangle count goes into the second one), 12 quadwords per
   triangle at TOPS + 3 (per vertex: x, y, z, a no-draw flag that is 1 on the first two vertices of every
   triangle but the batch's first; the normal with w = 1; the colour; 0, 0, 1, 0) and MSCNT; padding to a
   quadword and a DMA end tag. */
/* NOT MATCHING (registers only, see the note in the attempt): the hoisting of the three header constants, which
   used to be the obstacle, is reproduced by duplicating the statements behind the flag store in its three arms. */
#if 0
/* NOT MATCHING, but the code is now the original's in shape: 182 instructions against 182, and with register
   names masked only 4 of them differ (60 by name). Third cleanup:
   - The missing loop size is found without any filler. The three arms of the no-draw flag each hold the flag
     store AND the first three statements behind it (`f = (f32 *)p; f[0] = normal.x; f[1] = normal.y;`). The arms
     are merged again after register allocation, so nothing of it shows, but the batch loop is then long enough
     at the second loop pass that the three header constants (0x6C038000, 0x8001, 0x10000000) stay in the loop,
     as in the original, while 1, 1.0f and 0.02f are still hoisted. Two statements in the arms are too few
     (186 instructions), four give the same count with other differences, five are too many. (The dead `pad`
     arithmetic of the second cleanup is not needed and is gone.)
   - Header stores in this order (pkt[3] first of the last five), `pad = 4 - pad` directly, tag[0] written
     before tag[4]: unchanged from the second cleanup.
   Still different (all register choice except the last two):
   - the triangle loop: the original has the triangle base in v1, the vertex pointer in a0, k in t2, the k test
     in t3, i * 64 in t4, j in t5; here t1 / t3 / v1 / t5 / a0 / t4. A `tri` pointer with or without a walking
     vertex pointer changes the instruction count (186 / 184) and is worse.
   - the normal's three loads alternate f1 / f0 / f1 in the original, f0 / f1 / f0 here, and `f + 12` is computed
     one store earlier there.
   - `bnez` for the j < 18 test and `blez` in front of the padding loop where this has `bnezl` / `blezl`.
   Behaviour is the same. */
s32 ObjShadow_BuildPacket(u32 *pkt, s32 count, ObjShadowTri *tris, f32 *color) {
    u32 *p;
    f32 *f;
    u32 *tag;
    s32 i;
    s32 j;
    s32 k;
    s32 pad;
    s32 n;
    s32 ret;

    pkt[0] = 0x60000000;
    pkt[2] = 0x6C018000;
    pkt[4] = 0x302EC000;
    pkt[3] = 0x8000;
    pkt[5] = 0x412;
    pkt[7] = 0x17000000;
    pkt[1] = 0;
    pkt[6] = 0;
    p = pkt + 8;
    i = 0;
    do {
        *p++ = 0x6C038000;
        *p++ = 0x8001;
        *p++ = 0x10000000;
        *p++ = 6;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        tag = p;
        *p++ = 0;
        *p++ = 0x302E4000;
        *p++ = 0x412;
        *p++ = 0;
        *p++ = 0;
        f = (f32 *)p;
        j = 0;
        do {
            for (k = 0; k < 3; k++) {
                *f++ = tris[i].v[k].x + tris[i].normal.x * 0.02f;
                *f++ = tris[i].v[k].y + tris[i].normal.y * 0.02f;
                *f++ = tris[i].v[k].z + tris[i].normal.z * 0.02f;
                p = (u32 *)f;
                if (k < 2) {
                    if (j == 0) {
                        *p++ = 0;
                        f = (f32 *)p;
                        f[0] = tris[i].normal.x;
                        f[1] = tris[i].normal.y;
                    } else {
                        *p++ = 1;
                        f = (f32 *)p;
                        f[0] = tris[i].normal.x;
                        f[1] = tris[i].normal.y;
                    }
                } else {
                    *p++ = 0;
                    f = (f32 *)p;
                    f[0] = tris[i].normal.x;
                    f[1] = tris[i].normal.y;
                }
                f[2] = tris[i].normal.z;
                f[3] = 1.0f;
                f[4] = color[0];
                f[5] = color[1];
                f[6] = color[2];
                f[7] = color[3];
                f[8] = 0.0f;
                f[9] = 0.0f;
                f[10] = 1.0f;
                f[11] = 0.0f;
                f += 12;
            }
            i++;
            j++;
        } while (i < count && j < 18);
        p = (u32 *)f;
        n = j * 3;
        tag[0] = n | 0x8000;
        tag[4] = (n << 18) | 0x6C008003;
        *p++ = 0x17000000;
    } while (i < count);
    n = p - pkt;
    pad = n % 4;
    if (pad != 0) {
        pad = 4 - pad;
        for (k = 0; k < pad; k++) {
            *p++ = 0;
        }
    }
    ret = 1;
    *p++ = 0x70000000;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    pkt[0] = (p - pkt) / 4 + 0x5FFFFFFE;
    return ret;
}

#endif
INCLUDE_ASM("asm/nonmatchings/sys/gfxm_e_b", ObjShadow_BuildPacket);

/* Matrix that flattens geometry on the plane (x, y, z, w + bias) along the direction `light`
   (sceVu0DropShadowMatrix for a parallel light, with the light reversed). */
void ObjShadow_MakePlaneMtx(Mtx44 *out, Vec4 *plane, Vec4 *light, f32 bias) {
    Vec4 l;
    f32 lx, ly, lz;
    f32 a, b, c, d;

    Vec4_Copy(&l, light);
    lx = -l.x;
    ly = -l.y;
    lz = -l.z;
    a = plane->x;
    b = plane->y;
    c = plane->z;
    d = -(plane->w + bias);
    out->m[0][0] = b * ly + c * lz;
    out->m[0][1] = -a * ly;
    out->m[0][2] = -a * lz;
    out->m[0][3] = 0.0f;
    out->m[1][0] = -b * lx;
    out->m[1][1] = a * lx + c * lz;
    out->m[1][2] = -b * lz;
    out->m[1][3] = 0.0f;
    out->m[2][0] = -c * lx;
    out->m[2][1] = -c * ly;
    out->m[2][2] = a * lx + b * ly;
    out->m[2][3] = 0.0f;
    out->m[3][0] = d * lx;
    out->m[3][1] = d * ly;
    out->m[3][2] = d * lz;
    out->m[3][3] = a * lx + b * ly + c * lz;
}

/* Screen distance of a camera at distance |a - b| that makes a sphere of radius `size` fill `half` pixels. */
f32 ObjShadow_CalcScreenDist(Vec4 *a, Vec4 *b, f32 size, f32 half) {
    Vec4 d;
    f32 dist;
    f32 s;

    Vec4_Sub(&d, a, b);
    dist = sqrtf(Vec3_Dot(&d, &d));
    s = size / dist;
    return dist * sqrtf(1.0f - s * s) * half / (size + size);
}

/* Non-zero when the object throws a ground shadow now: its flag is set and no stage job is running. */
s32 ObjShadow_IsEnabled(ObjMdlObj *obj) {
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return 0;
    }
    if ((obj->flags & 0x10000000) && obj->shadow != NULL) {
        return 1;
    }
    return 0;
}

/* Non-zero when the object has the flat shadow flag. */
s32 ObjShadow_IsFlat(ObjMdlObj *obj) {
    if (obj->flags & 0x20000000) {
        return 1;
    }
    return 0;
}

/* Points the GS at the 256x256 shadow page (0x150), clears it to black with alpha 0 and leaves it ready for
   untextured, blended drawing inside a one pixel border. */
void ObjShadow_AddTargetEnv(void) {
    u64 *p = Dma_BeginDirect();
    u64 *tag;
    s32 i;
    s32 x;

    p[0] = GIF_TAG(12, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0xFF, 0, 0xFF);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = GS_SET_FRAME(0x150, 4, 0, 0);
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7800, 0x7800);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = 5;
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 1;
    p[1] = GS_COLCLAMP;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x60;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = ((u64)0x80 << 32) | 0x64;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 6;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = (u64)0x3F800000 << 32;
    p[1] = GS_RGBAQ;
    p += 2;
    tag = p;
    p += 2;
    x = 0;
    for (i = 0; i < 8; i++) {
        p[0] = ((x + 0x780) << 4) | (0x7800 << 16);
        p[1] = (s64)((x + 0x7A0) << 4) | ((u64)0x8800 << 16);
        p += 2;
        x += 32;
    }
    tag[0] = GIF_TAG_EX(8, 0, GIF_FLG_REGLIST, 2);
    tag[1] = 0x55;
    p[0] = GIF_TAG(4, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GS_SET_SCISSOR(1, 0xFE, 1, 0xFE);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0x3F80000080808080;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0;
    p[1] = GS_PRMODECONT;
    p += 2;
    p[0] = 0x48;
    p[1] = GS_PRMODE;
    p += 2;
    Dma_EndDirect(p);
}

/* Box to search for ground triangles: the object's bounds, widened in x and z. Returns 1 when the shadow is
   thrown straight down (split screen, or a body scale above 50) instead of along the light. */
s32 ObjShadow_GetBox(ColBoxE *out, ObjMdlObj *obj) {
    Vec4 center;
    Vec4 half;
    s32 down = 0;

    *out = obj->box;
    ColBox_GetCenter(out, &center);
    ColBox_GetHalf(out, &half);
    if (Battle_IsSplitScreen()) {
        half.x *= 1.1f;
        half.z *= 1.1f;
        down = 1;
    } else if (obj->scale > 50.0f) {
        half.x *= 1.1f;
        half.z *= 1.1f;
        down = 1;
    } else {
        half.x *= 2.3f;
        half.z *= 2.3f;
    }
    ColBox_SetCenterHalf(out, &center, &half);
    return down;
}

/* Picks the size of the shadow area and the texture scale from the body scale, and aims at the object. */
void ObjShadow_SetTarget(ObjMdlObj *obj, ObjShadowWork *work) {
    f32 scale = obj->scale;
    ObjShadowXf *xf = &obj->xf;

    if (scale <= 23.0f) {
        work->size = 18.0f;
        work->scale.x = 0.055f;
    } else if (scale <= 50.0f) {
        work->size = 24.3999005f;
        work->scale.x = 0.042f;
    } else if (scale <= 70.0f) {
        work->size = 50.0f;
        work->scale.x = 0.021f;
    } else {
        work->size = 85.0f;
        work->scale.x = 0.012f;
    }
    Vec4_Copy(&work->target, &xf->pos);
}

/* Puts the shadow camera 1000 above the target, looking straight down, and stores its view matrix. */
void ObjShadow_MakeLightMtx(ObjMdlObj *obj, ObjShadowWork *work) {
    Mtx44 m;
    ObjVec8 angles = { 1.5707963f, -1.5707963f, 0.0f, 1.0f };

    work->eye.x = work->target.x;
    work->eye.y = work->target.y + 1000.0f;
    work->eye.z = work->target.z;
    work->eye.w = 1.0f;
    Mtx_StoreIdentity(&m);
    m.m[1][1] = -1.0f;
    Vu0Cur_LoadIdentity();
    Vu0Cur_RotateZXY(&angles);
    Vu0Cur_StoreMtx(&m);
    m.m[3][0] = work->eye.x;
    m.m[3][1] = work->eye.y;
    m.m[3][2] = work->eye.z;
    Mtx_InverseRT(&work->view, &m);
}

/* Builds the world-to-shadow-texture matrix: a camera at the eye looking at the target, with a screen distance
   that makes the shadow area fill the 256x256 page (centre 2048, 2048). */
void ObjShadow_MakeProjMtx(ObjMdlObj *obj, ObjShadowWork *work) {
    Vec4 side;
    Vec4 dir;
    Mtx44 proj;
    Mtx44 cam;
    Vec4 up;

    memset(&up, 0, sizeof(up));
    up.z = 1.0f;
    Vec3_Sub(&dir, &work->target, &work->eye);
    Vec3_Normalize(&dir, &dir);
    Vec3_Cross(&side, &dir, &up);
    Vec3_Normalize(&side, &side);
    Mtx_MakeViewScreen(&proj, ObjShadow_CalcScreenDist(&work->target, &work->eye, work->size, 254.0f), 1.0f, 1.0f,
                       2048.0f, 2048.0f, 1.0f, 10877000.0f, 1.0f, 10877000.0f);
    Mtx_MakeCamera(&cam, &work->eye, &dir, &side);
    Mtx_Mul(&work->proj, &proj, &cam);
}

/* Loads VU1 program 2 with the shadow camera's matrix times the flattening matrix (plane y = the target's
   height, along the shadow direction), sets the object's colour to 128 and queues its meshes. */
void ObjShadow_LoadProg(ObjMdlObj *obj, ObjShadowWork *work) {
    Mtx44 flat;
    ObjPlane8 plane = { 0.0f, -1.0f, 0.0f, 1.0f };
    Mtx44 inv;
    Mtx44 *proj = &work->proj;
    ObjShadow *sh = obj->shadow;
    ObjVu1Prog2 *prog = Vu1Pkt_LoadProg2(1);
    Mtx44 *screen;
    ObjShadowView *view;

    Mtx_InverseRT(&inv, &BtlObj_GetNode(obj, 0)->mtx);
    screen = &prog->screen;
    Mtx_Mul(screen, &inv, proj);
    plane.d = work->target.y;
    ObjShadow_MakePlaneMtx(&flat, (Vec4 *)&plane, &sh->light, -0.01f);
    Mtx_Copy(screen, proj);
    Mtx_Mul(screen, screen, &flat);
    Mtx_Copy(&prog->clip, screen);
    view = obj->view;
    view->color.x = 128.0f;
    view->color.y = 128.0f;
    view->color.z = 128.0f;
    view->color.w = 128.0f;
    ObjDraw_DrawPartsFlat(obj);
}

/* Takes a shadow from the pool for an object that has the shadow flag. */
void ObjShadow_Alloc(ObjMdlObj *obj) {
    if (obj->flags & 0x10000000) {
        obj->shadow = BtlObjPool3_Alloc();
    }
}

/* Gives the object's shadow back to the pool. */
void ObjShadow_Free(ObjMdlObj *obj) {
    if (obj->shadow != NULL) {
        BtlObjPool3_Free(obj->shadow);
    }
}

/* Restores the full-screen scissor and the renderer's normal state after the shadow passes. */
void ObjShadow_ResetEnv(void) {
    Dma_AddScissor(0, 0x1FF, 0, 0x1BF);
    ObjGs_AddDefaultEnv();
}

/* Once per frame (not while paused): collects the stage triangles under the object and the shadow's alpha,
   picks the shadow direction and flips the packet buffer. */
void ObjShadow_Update(ObjMdlObj *obj) {
    ColBoxE box;

    if (ObjShadow_IsEnabled(obj)) {
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            ObjShadow *sh = obj->shadow;
            ObjShadowXf *xf = &obj->xf;
            ObjShadowWork *work;

            sh->count = 0;
            sh->unk2014 = 0;
            sh->alpha = 0;
            work = sh->work;
            if (ObjShadow_GetBox(&box, obj)) {
                ObjVec8 down = { 0.0f, -1.0f, 0.0f, 1.0f };

                Vec4_Copy(&sh->light, (Vec4 *)&down);
            } else {
                BtlObjLight_GetHalf(&sh->light);
            }
            work->flip ^= 1;
            sh->count = StgShadow_Collect(xf->zone, &box, OBJ_SHADOW_TRI_MAX, sh->tris);
            sh->alpha = StgShadow_GetOut()->alpha;
        }
    }
}

/* First shadow pass of an object: draws its silhouette from above into the shadow page. */
void ObjShadow_BeginRender(ObjMdlObj *obj) {
    if (ObjShadow_IsEnabled(obj)) {
        ObjShadowWork *work = obj->shadow->work;

        ObjGs_AddDefaultEnv();
        ObjShadow_SetTarget(obj, work);
        ObjShadow_MakeLightMtx(obj, work);
        ObjShadow_MakeProjMtx(obj, work);
        ObjShadow_AddTargetEnv();
        ObjShadow_LoadProg(obj, work);
    }
}

/* Second shadow pass: draws the collected ground triangles in the view, textured with the shadow page. */
void ObjShadow_Draw(ObjMdlObj *obj, ObjShadowCam *view) {
    Vec4 color;

    if (ObjShadow_IsEnabled(obj)) {
        ObjShadow *sh = obj->shadow;
        ObjShadowWork *work = sh->work;

        if (sh->count != 0) {
            if (sh->count >= OBJ_SHADOW_PKT_TRIS) {
                sh->count = OBJ_SHADOW_PKT_TRIS - 1;
            }
            Dma_AddScissor(view->x0, view->x1, view->y0, view->y1);
            ObjGs_AddDefaultEnv();
            BtlObjLight_GetColor(&color);
            color.w *= sh->alpha;
            ObjShadow_BuildPacket(work->pkt[work->flip], sh->count, sh->tris, (f32 *)&color);
            Dma_AddTexFlush();
            ObjGs_AddShadowTexEnv(0x2A00, 4);
            Vu1Pkt_LoadProg6(view, &work->view, &work->scale);
            Vu1Pkt_CallProg6(work->pkt[work->flip]);
            Dma_AddTexFlush();
        }
    }
}

/* Flat shadow: draws the object's meshes squashed on the plane y = 0 along the light, in the light's colour. */
void ObjShadow_BeginFlat(ObjMdlObj *obj) {
    Mtx44 flat;
    Vec4 half;
    Vec4 plane;
    ObjVu1Prog2 *prog;

    memset(&plane, 0, sizeof(plane));
    plane.y = -1.0f;
    if (ObjShadow_IsFlat(obj)) {
        BtlObjLight_GetColor(&obj->view->color);
        Vu1Pkt_LoadProg2(0);
        BtlObjLight_GetHalf(&half);
        ObjShadow_MakePlaneMtx(&flat, &plane, &half, -0.01f);
        prog = Vu1Pkt_LoadProg2(0);
        Vu0Screen_StoreMtx(&prog->screen);
        Vu0Clip_StoreMtx(&prog->clip);
        Mtx_Mul(&prog->screen, &prog->screen, &flat);
        Mtx_Mul(&prog->clip, &prog->clip, &flat);
        ObjDraw_DrawPartsFlat(obj);
    }
}

/* Clears the shadow pool and gives each of its five shadows a work block and two packet buffers. */
void ObjShadow_InitPool(void) {
    ObjShadowPool *pool = BtlObjPool3_Get();
    s32 size;
    s32 i;

    memset(pool, 0, sizeof(ObjShadowPool));
    size = ObjShadow_CalcPacketWords(OBJ_SHADOW_PKT_TRIS) * 4;
    for (i = 0; i < OBJ_SHADOW_MAX; i++) {
        ObjShadowWork *work;

        pool->nodes[i].work = Heap_Alloc(sizeof(ObjShadowWork), 0x20, 0, 2);
        memset(pool->nodes[i].work, 0, sizeof(ObjShadowWork));
        work = pool->nodes[i].work;
        work->pkt[0] = Heap_Alloc(size, 0x40, 0, 2);
        work->pkt[1] = Heap_Alloc(size, 0x40, 0, 2);
        memset(work->pkt[0], 0, size);
        memset(work->pkt[1], 0, size);
        SList_PushFront(&pool->free, &pool->nodes[i].link);
    }
}

/* Frees the buffers of the shadow pool. */
void ObjShadow_TermPool(void) {
    ObjShadowPool *pool = BtlObjPool3_Get();
    s32 i;

    for (i = 0; i < OBJ_SHADOW_MAX; i++) {
        ObjShadowWork *work = pool->nodes[i].work;

        Heap_Free(work->pkt[0]);
        Heap_Free(work->pkt[1]);
        Heap_Free(pool->nodes[i].work);
    }
}
