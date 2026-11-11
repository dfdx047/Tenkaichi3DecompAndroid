#include "common.h"
/*
 * The battle object renderer (0x1101D0..0x112A30): one object in one view. Called from the per-view loops of
 * gfxm_d_b.c; ObjDraw_DrawPartsFlat is called by the shadow code (ObjShadow_LoadProg / ObjShadow_BeginFlat).
 *
 * BtlObjDraw_DrawModel, in order:
 *   ObjDraw_UploadTextures   the model's own textures every frame (the face texture replaced by the current
 *                            expression), then the CLUT range for the object's colour flag
 *   ObjDraw_AddClutPass      draws INTO the model's CLUT (64x64 frame at block model->cbp): dimming, distance fade
 *                            colour, tint and hit flash are blended over the palette, not over the pixels
 *   ObjDraw_SetLight         light direction for the view from the colour flags (battle light, camera, rim light)
 *   ObjSeam_Transform        CPU skinning of the seam vertices (VU0 routine ObjSeam_TransformVtx in front of this file)
 *   ObjDraw_DrawParts(0)     one VU1 packet per visible part (program 0), the head (node 0x33) last
 *   ObjSeam_Draw             the seam triangles: base texture, then the shading ramp through GS context 2
 * BtlObjDraw_DrawModelFade draws the parts with flag 1 again through VU1 program 1 with the fade texture.
 * The ObjGs_* functions at the end queue fixed register packets (same family as the ones in gfxm_e.c).
 *
 * ObjSeam_TransformVtx(vtx, work, mtx, ofs) (0x10FFD0, VU0 macro code, stays assembly):
 *   lightA = work->light * mtx[0], lightB = work->light * mtx[1] (stored in work);
 *   n = lerp(lightB * normal, lightA * normal, pos.w); shade u = n.x * 0.5 + 0.5 (work->k.w);
 *   p = lerp(mtx[1] * (pos - ofs[1]), mtx[0] * (pos - ofs[0]), pos.w), projected with the screen matrix in vf24-27;
 *   vtx->xyz = ftoi4(p / p.w) with w = 0x8000 when x or y leaves 0..4096 (vf19); vtx->stBase = uv / p.w;
 *   vtx->stShade = (u, 0, 1) / p.w.
 */
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/gfxm_d_c.h"
#include "battle/btl_cam.h"

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Mtx_MakeNormalLight(Mtx44 *dst, Vec4 *l0, Vec4 *l1, Vec4 *l2);
extern void Vu0_InitAxisRegs(void);
extern void Vu0Screen_StoreMtx(Mtx44 *dst);
extern void Vu0Clip_StoreMtx(Mtx44 *dst);
extern ObjDrawPkt *Vu1Pkt_CallProg0(u8 *chain);
extern ObjDrawPkt *Vu1Pkt_LoadProg0(void);
extern ObjDrawPkt *Vu1Pkt_CallProg1(ObjDrawPart *part);
extern ObjDrawPkt *Vu1Pkt_LoadProg1(void);
extern ObjDrawPkt *Vu1Pkt_CallProg2(ObjDrawPart *part);
extern ObjDrawNode *BtlObj_GetNode(ObjDrawObj *obj, s32 node);
extern ObjDrawPart *BtlObj_FindBound(ObjDrawObj *obj, s32 node);
extern s32 BtlObj_GetSubState(ObjDrawObj *obj);
extern TexEntry *BtlObj_GetFaceTexture(ObjDrawObj *obj);
extern void BtlObjLight_GetDir(Vec4 *out);
extern void BtlObjLight_FindRes1(s32 key);
extern s32 BtlObjFade_Get(ObjDrawObj *obj, f32 *out);
extern s32 BtlObjFlash_GetColor(ObjDrawObj *obj, Vec4 *color, f32 *alpha);
extern void ObjGs_AddFrameMaskOpaque(u32 fbmsk);
extern void ObjGs_AddFrameMaskBlend(u32 fbmsk);
extern void Dma_AddTexFlush(void);
extern void Dma_AddZbuf(s32 zbp, s32 zmsk);
/* The VU0 routine in front of this file (0x10FFD0): skins and projects one seam vertex. */
extern void ObjSeam_TransformVtx(ObjSeamVtx *vtx, ObjSeamWork *work, Mtx44 *mtx, Vec4 *ofs);

extern Vec4 D_002EC2C0; /* zero vector */
extern Vec4 D_002C3440; /* 128, 128, 128, 128 */

#define OBJ_FLAG_BITS(p) ((ObjDrawFlagBits *)&(p)->flags)
/* FRAME value for the colour buffer being drawn, with a write mask. */
#define OBJ_FRAME_REG(fbmsk) GS_SET_FRAME((gGfx.frame & 1) ? GFX_FBP_A : GFX_FBP_B, GFX_FBW, 0, fbmsk)
#define OBJ_SPRITE_TAG 0x2400000000000001 /* REGLIST, one loop of two registers */

/* Skins and projects the vertices of the model's seam section for the current view. */
void ObjSeam_Transform(ObjDrawObj *obj, ObjDrawView *view) {
    ObjSeamWork work;
    ObjSeamSec *sec;
    ObjSeamVtx *vtx;
    s32 i;

    if (obj->mdl.model->seamOfs != 0) {
        sec = (ObjSeamSec *)((u8 *)obj->mdl.model + obj->mdl.model->seamOfs);
        vtx = (ObjSeamVtx *)((u8 *)obj->mdl.model + sec->vtxOfs);
        Mtx_MakeNormalLight(&work.light, &view->lightDir, &D_002EC2C0, &D_002EC2C0);
        work.k.w = 0.5f;
        for (i = 0; i < sec->vtxCount; i++) {
            ObjDrawPart *part = BtlObj_FindBound(obj, vtx[i].node);
            ObjDrawNode *node = BtlObj_GetNode(obj, vtx[i].node);

            ObjSeam_TransformVtx(&vtx[i], &work, &node->mtxA, &part->ofsA);
        }
        Vu0_InitAxisRegs();
    }
}

/* Draws the seam triangles twice: base texture in the view's colour, then the shading ramp through context 2. */
void ObjSeam_Draw(ObjDrawObj *obj, ObjDrawState *state, ObjDrawView *view) {
    ObjSeamColor color = { { 0x80, 0x80, 0x80, 0x80 } };
    ObjDrawMdl *mdl;
    ObjDrawModel *model;
    ObjSeamSec *sec;
    ObjSeamTri *tri;
    ObjSeamVtx *vtx;
    u64 rawBase;
    u64 rawShade;
    u64 tex0Base;
    u64 tex0Shade;
    u64 regBase;
    u64 regShade;
    u64 *p;
    u64 *tag;
    s32 pass;
    s32 i;
    s32 n;
    s32 tbpBase;
    s32 cbpBase;
    s32 tbpShade;
    s32 cbpShade;
    s32 tbp;
    s32 clutTbp;

    if (obj->mdl.model->seamOfs != 0) {
        sec = (ObjSeamSec *)((u8 *)obj->mdl.model + obj->mdl.model->seamOfs);
        mdl = &obj->mdl;
        model = mdl->model;
        tri = (ObjSeamTri *)((u8 *)model + sec->triOfs);
        tbp = model->tbp;
        clutTbp = model->clutTbp;
        rawBase = tri->tex0Base;
        rawShade = tri->tex0Shade;
        tbpBase = (s32)(rawBase & 0x3FFF) + tbp;
        cbpBase = (s32)((rawBase >> 37) & 0x3FFF) + model->cbp;
        tbpShade = (s32)(rawShade & 0x3FFF) + 0x3D40;
        cbpShade = (s32)((rawShade >> 37) & 0x3FFF) + clutTbp;
        tex0Base = (rawBase & ~0x3FFF) | tbpBase;
        tex0Shade = (rawShade & ~0x3FFF) | tbpShade;
        tex0Base = (tex0Base & 0xFFF8001FFFFFFFFF) | ((u64)cbpBase << 37);
        tex0Shade = (tex0Shade & 0xFFF8001FFFFFFFFF) | ((u64)cbpShade << 37);
        vtx = (ObjSeamVtx *)((u8 *)model + sec->vtxOfs);
        p = Dma_BeginDirect();
        regBase = tex0Base | 0x400000000;
        regShade = tex0Shade | 0x400000000;
        for (pass = 0; pass < 2; pass++) {
            if (pass == 0) {
                color.v[0] = view->color.x;
                color.v[1] = view->color.y;
                color.v[2] = view->color.z;
                p[0] = GIF_TAG(2, 0, 1);
                p[1] = GIF_REG_AD;
                p += 2;
                p[0] = 0;
                p[1] = GS_TEXFLUSH;
                p += 2;
                p[0] = regBase;
                p[1] = GS_TEX0_1;
                p += 2;
                tag = p;
                p += 2;
                for (i = 0, n = 0; i < sec->triCount; i++) {
                    if (vtx[tri[i].idx[0]].xyz.w[3] & 0x8000) {
                        continue;
                    }
                    if (vtx[tri[i].idx[1]].xyz.w[3] & 0x8000) {
                        continue;
                    }
                    if (vtx[tri[i].idx[2]].xyz.w[3] & 0x8000) {
                        continue;
                    }
                    *(u128 *)p = vtx[tri[i].idx[0]].stBase;
                    p += 2;
                    *(u128 *)p = color.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[0]].xyz.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[1]].stBase;
                    p += 2;
                    *(u128 *)p = color.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[1]].xyz.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[2]].stBase;
                    p += 2;
                    *(u128 *)p = color.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[2]].xyz.q;
                    p += 2;
                    n++;
                }
                tag[0] = (n * 3) | 0x8000 | ((u64)0xC0B7 << 46);
                tag[1] = 0x412;
            } else if (pass == 1) {
                p[0] = GIF_TAG(2, 0, 1);
                p[1] = GIF_REG_AD;
                p += 2;
                p[0] = 0;
                p[1] = GS_TEXFLUSH;
                p += 2;
                p[0] = regShade;
                p[1] = 7;
                p += 2;
                tag = p;
                p += 2;
                for (i = 0, n = 0; i < sec->triCount; i++) {
                    if (vtx[tri[i].idx[0]].xyz.w[3] & 0x8000) {
                        continue;
                    }
                    if (vtx[tri[i].idx[1]].xyz.w[3] & 0x8000) {
                        continue;
                    }
                    if (vtx[tri[i].idx[2]].xyz.w[3] & 0x8000) {
                        continue;
                    }
                    *(u128 *)p = vtx[tri[i].idx[0]].stShade;
                    p += 2;
                    *(u128 *)p = color.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[0]].xyz.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[1]].stShade;
                    p += 2;
                    *(u128 *)p = color.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[1]].xyz.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[2]].stShade;
                    p += 2;
                    *(u128 *)p = color.q;
                    p += 2;
                    *(u128 *)p = vtx[tri[i].idx[2]].xyz.q;
                    p += 2;
                    n++;
                }
                tag[0] = (n * 3) | 0x8000 | ((u64)0xC4B7 << 46);
                tag[1] = 0x412;
            }
        }
        Dma_EndDirect(p);
    }
}

/* Builds the rotation whose third row is `dir` and whose other rows are perpendicular to it and to the vertical. */
void ObjDraw_MakeFacingMtx(Mtx44 *m, Vec4 *dir) {
    ObjDrawVec up = { 0.0f, -1.0f, 0.0f, 1.0f };
    Vec4 side;

    Vec3_Cross(&side, dir, (Vec4 *)&up);
    Vec3_Normalize(&side, &side);
    Vec3_Cross((Vec4 *)&up, &side, dir);
    Vec3_Normalize((Vec4 *)&up, (Vec4 *)&up);
    Mtx_StoreIdentity(m);
    m->m[0][0] = side.x;
    m->m[1][0] = up.x;
    m->m[2][0] = dir->x;
    m->m[0][1] = side.y;
    m->m[1][1] = up.y;
    m->m[2][1] = dir->y;
    m->m[0][2] = side.z;
    m->m[1][2] = up.z;
    m->m[2][2] = dir->z;
}

/* Queues the model's per-frame texture uploads: its own textures one by one (the face texture replaced by the
   current expression), then the CLUT range that matches the object's colour flag. */
void ObjDraw_UploadTextures(ObjDrawObj *obj) {
    u32 pkt[12] = {
        DMA_TAG_CNT | 2, 0, 0, VIF_DIRECT | 2,
        GIF_EOP | 1, 0x10000000, GIF_REG_AD, 0,
        0, 0, 0x50, 0,
    };
    ObjDrawMdl *mdl = &obj->mdl;
    TexFile *file;
    TexEntry *e;
    TexEntry *face;
    s32 i;
    s32 j;
    s32 tbp;
    s32 cbp;
    u8 count;
    u8 first;

    i = 0;
    file = obj->mdl.tex;
    first = obj->mdl.model->texFirst;
    count = obj->mdl.model->texCount;
    for (; i < file->count; i++) {
        e = &file->ent[i];
        if (i >= first && i < first + count && i != obj->mdl.model->faceTex) {
            tbp = e->tbpOfs + mdl->model->tbp;
            cbp = e->cbpOfs + mdl->model->cbp;
            pkt[8] = 0;
            pkt[9] = (((u64)tbp << 32) | ((u64)e->pixBlt << 48)) >> 32;
            Dma_AddData(pkt, 0x30);
            Dma_AddRef(e->pix, e->pixSize);
            pkt[8] = 0;
            pkt[9] = (((u64)cbp << 32) | ((u64)e->clutBlt << 48)) >> 32;
            Dma_AddData(pkt, 0x30);
            Dma_AddRef(e->clut, e->clutSize);
        } else if (i >= first && i < first + count && i == obj->mdl.model->faceTex) {
            face = BtlObj_GetFaceTexture(obj);
            tbp = e->tbpOfs + mdl->model->tbp;
            cbp = e->cbpOfs + mdl->model->cbp;
            if (face != NULL) {
                e = face;
                pkt[8] = 0;
                pkt[9] = (((u64)tbp << 32) | ((u64)e->pixBlt << 48)) >> 32;
                Dma_AddData(pkt, 0x30);
                Dma_AddRef(e->pix, e->pixSize);
                pkt[8] = 0;
                pkt[9] = (((u64)cbp << 32) | ((u64)e->clutBlt << 48)) >> 32;
                Dma_AddData(pkt, 0x30);
                Dma_AddRef(e->clut, e->clutSize);
            } else {
                pkt[8] = 0;
                pkt[9] = (((u64)tbp << 32) | ((u64)e->pixBlt << 48)) >> 32;
                Dma_AddData(pkt, 0x30);
                Dma_AddRef(e->pix, e->pixSize);
                pkt[8] = 0;
                pkt[9] = (((u64)cbp << 32) | ((u64)e->clutBlt << 48)) >> 32;
                Dma_AddData(pkt, 0x30);
                Dma_AddRef(e->clut, e->clutSize);
            }
        }
    }
    if (OBJ_FLAG_BITS(&obj->state)->colorStage || OBJ_FLAG_BITS(&obj->state)->colorA) {
        for (j = 0; j < obj->mdl.model->clutCount; j++) {
            TexFile_UploadRange(obj->mdl.tex, mdl->model->clutFirst, mdl->model->clutCount, mdl->model->clutTbp, -1);
        }
    } else if (obj->state.flags & 0x80000) {
        for (j = 0; j < obj->mdl.model->clutCount; j++) {
            BtlObjLight_FindRes1(mdl->model->clutTbp + j * 4);
        }
    } else if (obj->state.flags & 0x40000) {
        typedef struct { Vec4 dir, half, color; TexFile *res; } Bt4DrawLight;
        extern Bt4DrawLight *BtlObjLight_Get(void);
        for (j = 0; j < obj->mdl.model->clutCount; j++) {
            TexFile_UploadOne(BtlObjLight_Get()->res, 2, mdl->model->clutTbp + j * 4, -1);
        }
    } else if (obj->state.flags & 0x400000) {
        for (j = 0; j < obj->mdl.model->clutCount; j++) {
            TexFile_UploadRange(obj->mdl.tex, mdl->model->clutFirst, mdl->model->clutCount, mdl->model->clutTbp, -1);
        }
    }
}

/* Alpha of the dimming sprite: 0xF4 plus byte 3 of the character parameter block (inlined in the original). */
static inline s32 ObjDraw_GetDimAlpha(ObjDrawMdl *mdl) {
    if (mdl->param == NULL) {
        return 0xF4;
    }
    return (u8)(mdl->param[3] + 0xF4);
}

/* Draws into the model's CLUT (a 64-pixel wide frame at block `cbp`): sets the state for the pass, then blends
   up to four 64x64 sprites over the palette: the dimming alpha, the fade colour, the object's tint, the flash. */
void ObjDraw_AddClutPass(ObjDrawObj *obj, ObjDrawState *state, ObjDrawView *view, s32 dim) {
    Vec4 color;
    u8 c[4];
    f32 f;
    u64 *p;
    ObjDrawMdl *mdl;

    p = Dma_BeginDirect();
    p[0] = GIF_TAG(11, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    mdl = &obj->mdl;
    p[0] = ((u32)mdl->model->cbp >> 5) | 0x10000;
    p[1] = GS_FRAME_1;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = GS_SET_XYOFFSET(0x7E00, 0x7E00);
    p[1] = GS_XYOFFSET_1;
    p += 2;
    p[0] = GS_SET_SCISSOR(0, 0x3F, 0, 0x3F);
    p[1] = GS_SCISSOR_1;
    p += 2;
    p[0] = 0xFC000FC00A;
    p[1] = GS_CLAMP_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0x8000000064;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = 0x156;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = 0x3F80000080808080;
    p[1] = GS_RGBAQ;
    p += 2;
    if (dim) {
        s32 a = ObjDraw_GetDimAlpha(mdl);

        p[0] = GIF_TAG(3, 0, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = 0x46;
        p[1] = GS_PRIM;
        p += 2;
        p[0] = ((u64)a << 24) | 0x3F800000000000FF;
        p[1] = GS_RGBAQ;
        p += 2;
        p[0] = 0x44;
        p[1] = GS_ALPHA_1;
        p += 2;
        p[0] = OBJ_SPRITE_TAG;
        p[1] = 0x55;
        p += 2;
        p[0] = 0x7E007E00;
        p[1] = 0x82008200;
        p += 2;
    }
    if (BtlObjFade_Get(obj, &f)) {
        s32 a = f * 70.0f;

        p[0] = GIF_TAG(4, 0, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = (((u32)mdl->model->cbp >> 5) | 0x10000) | 0xFF00000000000000;
        p[1] = GS_FRAME_1;
        p += 2;
        p[0] = 0x46;
        p[1] = GS_PRIM;
        p += 2;
        p[0] = 0x3F800000006644FF;
        p[1] = GS_RGBAQ;
        p += 2;
        p[0] = ((u64)a << 32) | 0x64;
        p[1] = GS_ALPHA_1;
        p += 2;
        p[0] = OBJ_SPRITE_TAG;
        p[1] = 0x55;
        p += 2;
        p[0] = 0x7E007E00;
        p[1] = 0x82008200;
        p += 2;
    }
    if (obj->state.flags & 0x400000) {
        c[0] = (u32)state->tint.x;
        c[1] = (u32)state->tint.y;
        c[2] = (u32)state->tint.z;
        c[3] = (u32)state->tint.w;
        p[0] = GIF_TAG(4, 0, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = (((u32)mdl->model->cbp >> 5) | 0x10000) | 0xFF00000000000000;
        p[1] = GS_FRAME_1;
        p += 2;
        p[0] = 0x46;
        p[1] = GS_PRIM;
        p += 2;
        p[0] = c[0] | ((u64)c[1] << 8) | ((u64)c[2] << 16) | 0x3F80000000000000;
        p[1] = GS_RGBAQ;
        p += 2;
        p[0] = ((u64)c[3] << 32) | 0x68;
        p[1] = GS_ALPHA_1;
        p += 2;
        p[0] = OBJ_SPRITE_TAG;
        p[1] = 0x55;
        p += 2;
        p[0] = 0x7E007E00;
        p[1] = 0x82008200;
        p += 2;
    }
    if (BtlObjFlash_GetColor(obj, &color, &f)) {
        s32 a = f * 128.0f;

        if (a > 0x80) {
            a = 0x80;
        }
        p[0] = GIF_TAG(4, 0, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = (((u32)mdl->model->cbp >> 5) | 0x10000) | 0xFF00000000000000;
        p[1] = GS_FRAME_1;
        p += 2;
        p[0] = 0x46;
        p[1] = GS_PRIM;
        p += 2;
        p[0] = (u8)(u32)color.x | (((u64)(u8)(u32)color.z << 16) | ((u64)(u8)(u32)color.y << 8)) | 0x3F80000000000000;
        p[1] = GS_RGBAQ;
        p += 2;
        p[0] = ((u64)a << 32) | 0x68;
        p[1] = GS_ALPHA_1;
        p += 2;
        p[0] = OBJ_SPRITE_TAG;
        p[1] = 0x55;
        p += 2;
        p[0] = 0x7E007E00;
        p[1] = 0x82008200;
        p += 2;
    }
    Dma_EndDirect(p);
}

/* Queues the VU1 packets of the model's parts. mode 0: lit draw (program 0; the head, node 0x33, last and
   between two frame-mask packets unless `noMask`), 1: fade pass (program 1, parts with flag 1, texture `tex0`),
   2: flat draw (program 2). */
void ObjDraw_DrawParts(ObjDrawObj *obj, ObjDrawView *view, s32 mode, u64 tex0, u32 fbmsk, s32 noMask) {
    Mtx44 facing;
    Mtx44 rot;
    Mtx44 light;
    Vec4 dir;
    Vec4 flat;
    ObjDrawMdl *mdl;
    ObjDrawNode *headNode;
    ObjDrawPart *headPart;
    ObjDrawPart *part;
    ObjDrawNode *node;
    f32 k;

    headNode = NULL;
    headPart = NULL;
    if (mode == 1) {
        dir.x = gBtlCamView->world2view2.m[0][2];
        dir.y = gBtlCamView->world2view2.m[1][2];
        dir.z = gBtlCamView->world2view2.m[2][2];
        dir.w = 1.0f;
        ObjDraw_MakeFacingMtx(&facing, &dir);
        Mtx_InverseRT(&facing, &facing);
    }
    if (mode == 2) {
        Vec4_Copy(&flat, &view->unk30);
    }
    part = obj->mdl.parts;
    mdl = &obj->mdl;
    k = 128.0f;
    while (1) {
        if (part->enabled) {
            node = BtlObj_GetNode(obj, part->node);
            if (node->active) {
                if (mode == 0) {
                    if (part->node == 0x33) {
                        headNode = node;
                        headPart = part;
                    } else {
                        ObjDrawPkt *pkt = Vu1Pkt_CallProg0((part->node == 0x30 && BtlObj_GetSubState(obj) != -1)
                                                               ? mdl->chains[BtlObj_GetSubState(obj)]
                                                               : part->chain);

                        Mtx_Copy(&pkt->mtxB, &node->mtxB);
                        Mtx_Copy(&pkt->mtxA, &node->mtxA);
                        Vec4_Copy(&pkt->ofsA, &part->ofsA);
                        Vec4_Copy(&pkt->ofsB, &part->ofsB);
                        Mtx_MakeNormalLight(&pkt->light, &view->lightDir, &D_002EC2C0, &D_002EC2C0);
                        Vec4_Copy(&pkt->color, &view->color);
                        Vec4_Copy(&pkt->k, &D_002C3440);
                        Vu0Screen_StoreMtx(&pkt->screen);
                        Vu0Clip_StoreMtx(&pkt->clip);
                    }
                } else if (mode == 1) {
                    if (part->flags & 1) {
                        ObjDrawPkt *pkt = Vu1Pkt_CallProg1(part);

                        pkt->tex0[0] = tex0;
                        pkt->tex0[1] = tex0 >> 32;
                        pkt->texReg = 7;
                        pkt->unk1BC = 0;
                        Mtx_Copy(&pkt->mtxB, &node->mtxB);
                        Mtx_Copy(&pkt->mtxA, &node->mtxA);
                        Vec4_Copy(&pkt->ofsA, &part->ofsA);
                        Vec4_Copy(&pkt->ofsB, &part->ofsB);
                        pkt->color.x = k;
                        pkt->color.y = k;
                        pkt->color.z = k;
                        pkt->color.w = view->fade;
                        pkt->k.x = k;
                        pkt->k.y = k;
                        pkt->k.z = k;
                        pkt->k.w = k;
                        Mtx_Copy(&rot, &node->mtxA);
                        rot.m[3][0] = 0.0f;
                        rot.m[3][1] = 0.0f;
                        rot.m[3][2] = 0.0f;
                        Mtx_Mul(&light, &facing, &rot);
                        Mtx_Copy(&pkt->light, &light);
                        pkt->light.m[3][0] = 0.0f;
                        pkt->light.m[3][1] = 0.0f;
                        pkt->light.m[3][2] = 0.0f;
                        Vu0Screen_StoreMtx(&pkt->screen);
                        Vu0Clip_StoreMtx(&pkt->clip);
                    }
                } else if (mode == 2) {
                    ObjDrawPkt *pkt = Vu1Pkt_CallProg2(part);

                    Mtx_Copy(&pkt->mtxB, &node->mtxB);
                    Mtx_Copy(&pkt->mtxA, &node->mtxA);
                    Vec4_Copy(&pkt->ofsA, &part->ofsA);
                    Vec4_Copy(&pkt->ofsB, &part->ofsB);
                    Vec4_Copy((Vec4 *)&pkt->light, &flat);
                    pkt->light.m[0][3] = k;
                }
            }
        }
        if (part->last) {
            break;
        }
        part = (ObjDrawPart *)((u8 *)part + part->next);
    }
    if (headNode != NULL) {
        ObjDrawPkt *pkt;

        if (!noMask) {
            ObjGs_AddFrameMaskOpaque(fbmsk);
        }
        pkt = Vu1Pkt_CallProg0(headPart->chain);
        Mtx_Copy(&pkt->mtxB, &headNode->mtxB);
        Mtx_Copy(&pkt->mtxA, &headNode->mtxA);
        Vec4_Copy(&pkt->ofsA, &headPart->ofsA);
        Vec4_Copy(&pkt->ofsB, &headPart->ofsB);
        Mtx_MakeNormalLight(&pkt->light, &view->lightDir, &D_002EC2C0, &D_002EC2C0);
        Vec4_Copy(&pkt->color, &view->color);
        Vec4_Copy(&pkt->k, &D_002C3440);
        Vu0Screen_StoreMtx(&pkt->screen);
        Vu0Clip_StoreMtx(&pkt->clip);
        if (!noMask) {
            ObjGs_AddFrameMaskBlend(fbmsk);
        }
    }
}

/* Chooses the light direction of the object in this view (stored in view->lightDir) from its colour flags and
   queues the frame / alpha / depth registers of the lit draw. Returns the frame mask for the head (0 or alpha). */
u32 ObjDraw_SetLight(ObjDrawObj *obj, ObjDrawView *view) {
    Vec4 d;
    Vec4 l;
    Mtx44 inv;
    f32 f;
    u32 mask;
    s32 flags;

    mask = 0xFF000000;
    if (OBJ_FLAG_BITS(&obj->state)->visible && OBJ_FLAG_BITS(&obj->state)->bit2) {
        mask = 0;
    }
    flags = view->flags;
    if (flags & 0x10000) {
        BtlObjLight_GetDir(&view->lightDir);
        ObjGs_AddFrame1(mask, 0);
        ObjGs_AddFrame2Alpha(0xFF000000, 0x8000000062);
        ObjGs_AddZbufBoth(GFX_ZBP, 0);
    } else if (flags & 0x20000) {
        view->lightDir.x = gBtlCamView->world2view2.m[0][2];
        view->lightDir.y = gBtlCamView->world2view2.m[1][2];
        view->lightDir.z = gBtlCamView->world2view2.m[2][2];
        view->lightDir.w = 1.0f;
        ObjGs_AddFrame1(mask, 0);
        ObjGs_AddFrame2Alpha(0xFF000000, 0x8000000062);
        ObjGs_AddZbufBoth(GFX_ZBP, 0);
    } else if (flags & 0x400000) {
        if (obj->mdl.model->flags & 0x1000000) {
            view->lightDir.x = gBtlCamView->world2view2.m[0][2];
            view->lightDir.y = gBtlCamView->world2view2.m[1][2];
            view->lightDir.z = gBtlCamView->world2view2.m[2][2];
            view->lightDir.w = 1.0f;
            ObjGs_AddFrame1(mask, 0);
        } else {
            f32 k = 1.0f;
            ObjDrawNode *node = BtlObj_GetNode(obj, 3);
            Vec4 *dst;

            Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
            dst = &view->lightDir;
            Vec4_Sub(&d, (Vec4 *)inv.m[3], (Vec4 *)node->mtxA.m[3]);
            Vec3_Normalize(&d, &d);
            d.x += gBtlCamView->world2view2.m[0][0] * 0.7f;
            d.y = gBtlCamView->world2view2.m[1][0];
            d.z += gBtlCamView->world2view2.m[2][0] * 0.7f;
            d.w = k;
            Vec3_Normalize(&d, &d);
            Vec3_Scale(&d, &d, k);
            BtlObjLight_GetDir(&l);
            l.x *= 1.0f - k;
            l.y *= 1.0f - k;
            l.z *= 1.0f - k;
            l.w = k;
            Vec4_Add(dst, &d, &l);
            Vec3_Normalize(dst, dst);
            ObjGs_AddFrame1(mask, 0);
        }
        ObjGs_AddFrame2Alpha(0xFF000000, 0x8000000062);
        ObjGs_AddZbufBoth(GFX_ZBP, 0);
    } else if (flags & 0x80000) {
        view->lightDir.x = -gBtlCamView->world2view2.m[0][0] - gBtlCamView->world2view2.m[0][2] * -0.7f;
        view->lightDir.y = -gBtlCamView->world2view2.m[1][0] - gBtlCamView->world2view2.m[1][2] * -0.7f;
        view->lightDir.z = -gBtlCamView->world2view2.m[2][0] - gBtlCamView->world2view2.m[2][2] * -0.7f;
        view->lightDir.w = 1.0f;
        Vec3_Normalize(&view->lightDir, &view->lightDir);
        ObjGs_AddFrame1(mask, 0);
        ObjGs_AddFrame2Alpha(0xFF000000, 0x8000000048);
        ObjGs_AddZbufBoth(GFX_ZBP, 0);
    } else if (flags & 0x40000) {
        ObjDrawNode *node;
        Vec4 *dst;
        f32 k;

        if (!BtlObjFade_Get(obj, &f)) {
            f = 0.0f;
        }
        node = BtlObj_GetNode(obj, 3);
        k = 1.0f;
        Mtx_InverseRT(&inv, &gBtlCamView->world2view2);
        Vec4_Sub(&d, (Vec4 *)inv.m[3], (Vec4 *)node->mtxA.m[3]);
        dst = &view->lightDir;
        Vec3_Normalize(&d, &d);
        d.x += gBtlCamView->world2view2.m[0][0] * 0.7f;
        d.y = gBtlCamView->world2view2.m[1][0];
        d.z += gBtlCamView->world2view2.m[2][0] * 0.7f;
        d.w = k;
        Vec3_Normalize(&d, &d);
        Vec3_Scale(&d, &d, f);
        BtlObjLight_GetDir(&l);
        l.x *= k - f;
        l.y *= k - f;
        l.z *= k - f;
        l.w = k;
        Vec4_Add(dst, &d, &l);
        Vec3_Normalize(dst, dst);
        ObjGs_AddFrame1(mask, 0);
        ObjGs_AddFrame2Alpha(0xFF000000, 0x8000000062);
        ObjGs_AddZbufBoth(GFX_ZBP, 0);
    } else if (flags & 0x100000) {
        BtlObjLight_GetDir(&view->lightDir);
        mask |= 0xFFFFFF;
        ObjGs_AddFrame1(mask | 0xFFFFFF, 0);
        ObjGs_AddFrame2Alpha(0xFFFFFFFF, 0x8000000062);
        ObjGs_AddZbufBoth(GFX_ZBP, 0);
    } else {
        BtlObjLight_GetDir(&view->lightDir);
        ObjGs_AddFrame1(0xFFFFFF, 1);
        ObjGs_AddFrame2Alpha(0xFFFFFFFF, 0x8000000062);
        ObjGs_AddZbufBoth(GFX_ZBP, 1);
    }
    return mask;
}

/* Draws one object in the current view: texture uploads, the CLUT pass, the lit model and its seams. */
void BtlObjDraw_DrawModel(ObjDrawObj *obj) {
    ObjDrawState *state = &obj->state;
    ObjDrawView *view = obj->state.view;
    u32 mask;

    ObjDraw_UploadTextures(obj);
    BtlObjDraw_SetEnv();
    ObjGs_AddTexEnv();
    ObjDraw_AddClutPass(obj, state, view, (view->flags >> 21) & 1);
    BtlObjDraw_SetEnv();
    ObjGs_AddTexEnv();
    mask = ObjDraw_SetLight(obj, view);
    Vu1Pkt_LoadProg0();
    ObjSeam_Transform(obj, view);
    ObjDraw_DrawParts(obj, view, 0, 0, mask, (view->flags >> 21) & 1);
    ObjSeam_Draw(obj, state, view);
}

/* Draws the distance-fade layer of one object: its fade texture over the parts that have flag 1. */
void BtlObjDraw_DrawModelFade(ObjDrawObj *obj) {
    ObjDrawMdl *mdl = &obj->mdl;
    ObjDrawView *view = obj->state.view;
    ObjDrawPkt *pkt;
    u64 tex0;

    TexFile_UploadOne(mdl->tex, obj->mdl.model->fadeTex, mdl->model->fadeTbp, mdl->model->fadeCbp);
    Dma_AddTexFlush();
    TexFile_UploadRange(mdl->tex, mdl->model->fadeFirst, mdl->model->fadeCount, mdl->model->tbp, mdl->model->cbp);
    Dma_AddTexFlush();
    {
        TexEntry *e = &mdl->tex->ent[mdl->model->fadeTex];

        tex0 = e->tex0;
        tex0 |= (u64)(u32)mdl->model->fadeCbp << 37;
        tex0 |= (u32)mdl->model->fadeTbp;
        tex0 |= 0xC00000000;
    }
    ObjGs_AddTexEnv();
    ObjGs_AddFadeEnv();
    Dma_AddZbuf(GFX_ZBP, 1);
    pkt = Vu1Pkt_LoadProg1();
    Vu0Screen_StoreMtx(&pkt->screen);
    Vu0Clip_StoreMtx(&pkt->clip);
    ObjGs_AddFadeTex(tex0, view->fade);
    ObjDraw_DrawParts(obj, view, 1, tex0, 0, (view->flags >> 21) & 1);
    Dma_AddZbuf(GFX_ZBP, 0);
}

/* Queues the parts with VU1 program 2 (flat, for the shadow render). */
void ObjDraw_DrawPartsFlat(ObjDrawObj *obj) {
    ObjDraw_DrawParts(obj, obj->state.view, 2, 0, 0, 0);
}

/* End of the objects' pass: restores the default environment. */
void BtlObjDraw_End(void) {
    BtlObjDraw_SetEnv();
}

/* Queues the default environment of the objects' pass for both GS contexts: frame, depth buffer, offset, the
   current view's scissor, blending, tests, texture filter, clamp. */
void BtlObjDraw_SetEnv(void) {
    u32 pkt[88] = {
        DMA_TAG_CNT | 21, 0, VIF_FLUSHE, VIF_DIRECT | 21,
        GIF_EOP | 20, 0x10000000, GIF_REG_AD, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_1, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
        GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 0), 0, GS_ZBUF_1, 0,
        GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 0), 0, GS_ZBUF_2, 0,
        GFX_OFX, GFX_OFY, GS_XYOFFSET_1, 0,
        GFX_OFX, GFX_OFY, GS_XYOFFSET_2, 0,
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1),
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1) >> 32,
        GS_SCISSOR_1, 0,
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1),
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1) >> 32,
        GS_SCISSOR_2, 0,
        0x44, 0, GS_ALPHA_1, 0,
        0x44, 0, GS_ALPHA_2, 0,
        0x50000, 0, GS_TEST_1, 0,
        0x50000, 0, GS_TEST_2, 0,
        0x60, 0, GS_TEX1_1, 0,
        0x60, 0, 0x15, 0,
        0, 0, GS_FBA_1, 0,
        0, 0, GS_FBA_2, 0,
        0, 0, GS_CLAMP_1, 0,
        0, 0, GS_CLAMP_2, 0,
        1, 0, GS_COLCLAMP, 0,
        1, 0, GS_PRMODECONT, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues the texturing state of the lit draw: blend, tests, bilinear filter, clamp of context 2. */
void ObjGs_AddTexEnv(void) {
    u32 pkt[48] = {
        DMA_TAG_CNT | 11, 0, VIF_FLUSHE, VIF_DIRECT | 11,
        GIF_EOP | 10, 0x10000000, GIF_REG_AD, 0,
        0x64, 0x80, GS_ALPHA_1, 0,
        0x50000, 0, GS_TEST_1, 0,
        0, 0, GS_FBA_1, 0,
        0x60, 0, GS_TEX1_1, 0,
        0, 0, GS_CLAMP_1, 0,
        0x50000, 0, GS_TEST_2, 0,
        0x60, 0, 0x15, 0,
        5, 0, GS_CLAMP_2, 0,
        1, 0, GS_COLCLAMP, 0,
        0, 0, GS_FBA_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues FRAME_1 (current colour buffer, write mask `fbmsk`) with the blend, test and FBA of context 1. */
void ObjGs_AddFrame1(u32 fbmsk, s32 unused) {
    u32 pkt[24] = {
        DMA_TAG_CNT | 5, 0, VIF_FLUSHE, VIF_DIRECT | 5,
        GIF_EOP | 4, 0x10000000, GIF_REG_AD, 0,
        OBJ_FRAME_REG(fbmsk), OBJ_FRAME_REG(fbmsk) >> 32, GS_FRAME_1, 0,
        0x64, 0x80, GS_ALPHA_1, 0,
        0x50000, 0, GS_TEST_1, 0,
        0, 0, GS_FBA_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues ALPHA_2 = `alpha` and FRAME_2 (current colour buffer, write mask `fbmsk`). */
void ObjGs_AddFrame2Alpha(u32 fbmsk, u64 alpha) {
    u32 pkt[16] = {
        DMA_TAG_CNT | 3, 0, VIF_FLUSHE, VIF_DIRECT | 3,
        GIF_EOP | 2, 0x10000000, GIF_REG_AD, 0,
        0, 0, GS_ALPHA_2, 0,
        OBJ_FRAME_REG(fbmsk), OBJ_FRAME_REG(fbmsk) >> 32, GS_FRAME_2, 0,
    };

    pkt[8] = alpha;
    pkt[9] = alpha >> 32;
    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues the environment of the fade pass: as BtlObjDraw_SetEnv, but context 1 writes alpha only (FBMSK
   0x00FFFFFF) and context 2 blends with ALPHA 0x58 and clamps. */
void ObjGs_AddFadeEnv(void) {
    u32 pkt[84] = {
        DMA_TAG_CNT | 20, 0, VIF_FLUSHE, VIF_DIRECT | 20,
        GIF_EOP | 19, 0x10000000, GIF_REG_AD, 0,
        GFX_FRAME_REG(), 0xFFFFFF, GS_FRAME_1, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
        GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 0), 0, GS_ZBUF_1, 0,
        GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 0), 0, GS_ZBUF_2, 0,
        GFX_OFX, GFX_OFY, GS_XYOFFSET_1, 0,
        GFX_OFX, GFX_OFY, GS_XYOFFSET_2, 0,
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1),
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1) >> 32,
        GS_SCISSOR_1, 0,
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1),
        GS_SET_SCISSOR(gBtlCamView->scissorX0, gBtlCamView->scissorX1, gBtlCamView->scissorY0, gBtlCamView->scissorY1) >> 32,
        GS_SCISSOR_2, 0,
        0x44, 0, GS_ALPHA_1, 0,
        0x58, 0, GS_ALPHA_2, 0,
        0x50000, 0, GS_TEST_1, 0,
        0x50000, 0, GS_TEST_2, 0,
        0x60, 0, GS_TEX1_1, 0,
        0x60, 0, 0x15, 0,
        0, 0, GS_FBA_1, 0,
        0, 0, GS_FBA_2, 0,
        0, 0, GS_CLAMP_1, 0,
        5, 0, GS_CLAMP_2, 0,
        1, 0, GS_COLCLAMP, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues TEST_1 with alpha reference `aref` (pass when greater), TEX0_2 = `tex0`, TEXFLUSH. */
void ObjGs_AddFadeTex(u64 tex0, s32 aref) {
    u32 pkt[20] = {
        DMA_TAG_CNT | 4, 0, VIF_FLUSHE, VIF_DIRECT | 4,
        GIF_EOP | 3, 0x10000000, GIF_REG_AD, 0,
        ((u64)aref << 4) | 0x50004, (((u64)aref << 4) | 0x50004) >> 32, GS_TEST_1, 0,
        tex0, tex0 >> 32, 7, 0,
        0, 0, GS_TEXFLUSH, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues ZBUF_1 and ZBUF_2: depth buffer page (24-bit) and the "do not write depth" flag. */
void ObjGs_AddZbufBoth(s32 zbp, s32 zmsk) {
    u32 pkt[16] = {
        DMA_TAG_CNT | 3, 0, VIF_FLUSHE, VIF_DIRECT | 3,
        GIF_EOP | 2, 0x10000000, GIF_REG_AD, 0,
        GS_SET_ZBUF(zbp, GS_PSMZ24, zmsk), GS_SET_ZBUF(zbp, GS_PSMZ24, zmsk) >> 32, GS_ZBUF_1, 0,
        GS_SET_ZBUF(zbp, GS_PSMZ24, zmsk), GS_SET_ZBUF(zbp, GS_PSMZ24, zmsk) >> 32, GS_ZBUF_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}
