#include "common.h"
#include "battle/hud_c.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/sprite.h"

/*
 * Battle HUD: the sprite library, 0x224B50-0x226488 (it ends with HudSprite_Draw and the node setters of
 * src/battle/hud_d.c, 0x226488-0x226500).
 *
 * A HudCSprite is a rectangle in the coordinates of its node. Drawing transforms the four corners by the current
 * VU0 matrix (HudNode_Draw has multiplied the node's translation, rotation and mirror into it), so a sprite is
 * sent as a four-vertex triangle strip, not as a GS sprite. Screen coordinates are pixels of the 512x448 frame;
 * the GS coordinate is (x + 1792) * 16, (y + 1824) * 16. Texture coordinates are texels (sent as texel * 16 + 8).
 *
 * Textures are uploaded on first use in a frame: the image to block `tbp + imageBlock`, the palette to
 * `cbp + clutBlock` (HudSprite_Draw passes 0x2A00 / 0x2C80), and HudCTex.mark keeps a second upload away until
 * Hud_ClearTexMarks.
 *
 * Nothing here reads the pad, the clock or the camera; HudGfx_SetEnv reads the frame counter's parity to pick the
 * colour buffer. No random numbers.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern void Vu0Cur_MulVec4(f32 *out, f32 *v);

#define HUDC_BITBLTBUF(block, width) (((s64)(block) << 32) | ((u64)(width) << 48))

/* Calls fn(0). The HUD parts pass the function that queues a GS state (e.g. HudGauge_GsBeginMask) in front of a
   group of sprites. */
void HudGfx_CallBegin(void (*fn)(s32)) {
    fn(0);
}

/* The same; the parts pass the function that restores the state after the group. */
void HudGfx_CallEnd(void (*fn)(s32)) {
    fn(0);
}

/* Shows or hides a sprite. */
void HudSprite_Show(HudCSprite *spr, s32 show) {
    spr->hidden = show == 0;
}

/* Sets whether the sprite's texture is mirrored left to right. */
void HudSprite_SetMirror(HudCSprite *spr, s32 mirror) {
    spr->mirror = mirror;
}

/* Sets the rectangle (relative to the node). */
void HudSprite_SetRect(HudCSprite *spr, s32 x0, s32 x1, s32 y0, s32 y1) {
    spr->x0 = x0;
    spr->x1 = x1;
    spr->y0 = y0;
    spr->y1 = y1;
}

/* Sets the texel rectangle. */
void HudSprite_SetUv(HudCSprite *spr, s32 u0, s32 u1, s32 v0, s32 v1) {
    spr->u0 = u0;
    spr->u1 = u1;
    spr->v0 = v0;
    spr->v1 = v1;
}

/* Sets the texture entry and the offset of the entry that supplies the palette. */
void HudSprite_SetTex(HudCSprite *spr, s32 tex, s32 sub) {
    spr->tex = tex;
    spr->texSub = sub;
}

/* Sets the colour (0x80 = neutral). */
void HudSprite_SetColor(HudCSprite *spr, s32 r, s32 g, s32 b, s32 a) {
    spr->r = r;
    spr->g = g;
    spr->b = b;
    spr->a = a;
}

/* Makes the sprite an untextured rectangle of the given colour. */
void HudSprite_InitPlain(HudCSprite *spr, u8 r, u8 g, u8 b, u8 a) {
    spr->tex = -1;
    memset(&spr->u0, 0, 8);
    spr->r = r;
    spr->g = g;
    spr->b = b;
    spr->a = a;
}

/* Moves the rectangle. */
void HudSprite_Move(HudCSprite *spr, s32 dx, s32 dy) {
    spr->x0 += dx;
    spr->x1 += dx;
    spr->y0 += dy;
    spr->y1 += dy;
}

/* Moves the texel rectangle. */
void HudSprite_MoveUv(HudCSprite *spr, s32 du, s32 dv) {
    spr->u0 += du;
    spr->u1 += du;
    spr->v0 += dv;
    spr->v1 += dv;
}

/* Exchanges u0 and u1. */
void HudSprite_FlipU(HudCSprite *spr) {
    s16 t = spr->u0;

    spr->u0 = spr->u1;
    spr->u1 = t;
}

/* Exchanges v0 and v1. */
void HudSprite_FlipV(HudCSprite *spr) {
    s16 t = spr->v0;

    spr->v0 = spr->v1;
    spr->v1 = t;
}

/* Scales the rectangle about the node's origin. */
void HudSprite_Scale(HudCSprite *spr, f32 sx, f32 sy) {
    spr->x0 = spr->x0 * sx;
    spr->x1 = spr->x1 * sx;
    spr->y0 = spr->y0 * sy;
    spr->y1 = spr->y1 * sy;
}

/* Moves the rectangle so that its centre is the node's origin. */
void HudSprite_Center(HudCSprite *spr) {
    s32 cx = (spr->x0 + spr->x1) / 2;
    s32 cy = (spr->y0 + spr->y1) / 2;

    spr->x0 -= cx;
    spr->x1 -= cx;
    spr->y0 -= cy;
    spr->y1 -= cy;
}

/* Makes the sprite show the whole of texture `tex` of a sheet: rectangle and texel rectangle (0, w, 0, h) from the
   texture's size in TEX0, neutral colour. */
void HudSprite_InitTex(HudCSprite *spr, HudCRes *res, s32 tex, s32 sub) {
    HudCTex *t = &res->tex[tex];

    if (t != NULL) {
        s32 w = 1 << (s32)((t->tex0 >> 26) & 0xF);
        s32 h = 1 << (s32)((t->tex0 >> 30) & 0xF);

        HudSprite_SetTex(spr, tex, sub);
        HudSprite_SetRect(spr, 0, w, 0, h);
        HudSprite_SetUv(spr, 0, w, 0, h);
        HudSprite_SetColor(spr, 0x80, 0x80, 0x80, 0x80);
    }
}

/* Queues the upload of texture `tex` (image to block tbp + imageBlock, palette to cbp + clutBlock) and of the
   palette of texture `tex + sub`, each only if it has not been uploaded this frame. The palette of `tex + sub`
   goes to the block and width of its own entry but with the palette WIDTH of entry `tex`. */
void HudRes_UploadTex(HudCRes *res, s32 tex, s32 sub, s32 tbp, s32 cbp) {
    u32 pkt[12] = {
        0x10000002, 0x00000000, 0x00000000, 0x50000002, 0x00008001, 0x10000000,
        0x0000000E, 0x00000000, 0x00000000, 0x00000000, 0x00000050, 0x00000000,
    };
    HudCTex *t = &res->tex[tex];
    s32 imageWidth = t->imageWidth;
    s32 clutWidth = t->clutWidth;

    if (!t->mark) {
        t->mark = 1;
        if (t->imageSize != 0) {
            s32 block = t->imageBlock + tbp;

            pkt[8] = HUDC_BITBLTBUF(block, imageWidth);
            pkt[9] = HUDC_BITBLTBUF(block, imageWidth) >> 32;
            Dma_AddData(pkt, sizeof(pkt));
            Dma_AddRef(t->image, t->imageSize);
        }
        if (t->clutSize != 0) {
            s32 block = t->clutBlock + cbp;

            pkt[8] = HUDC_BITBLTBUF(block, clutWidth);
            pkt[9] = HUDC_BITBLTBUF(block, clutWidth) >> 32;
            Dma_AddData(pkt, sizeof(pkt));
            Dma_AddRef(t->clut, t->clutSize);
        }
    }
    t = &res->tex[tex + sub];
    if (!t->mark) {
        t->mark = 1;
        if (t->clutSize != 0) {
            s32 block = t->clutBlock + cbp;

            pkt[8] = HUDC_BITBLTBUF(block, clutWidth);
            pkt[9] = HUDC_BITBLTBUF(block, clutWidth) >> 32;
            Dma_AddData(pkt, sizeof(pkt));
            Dma_AddRef(t->clut, t->clutSize);
        }
    }
}

#define HUDC_XYZ(v) \
    ((((s32)(v)[0] << 4) + SPRITE_OFS_X) | ((u64)(((s32)(v)[1] << 4) + SPRITE_OFS_Y) << 16))
#define HUDC_XYZ_CONV(v) \
    ((((s32)(v)[0] << 4) + SPRITE_OFS_X) | ((u64)((Sprite_ConvY((s32)(v)[1]) << 4) + SPRITE_OFS_Y) << 16))

/* Draws the sprite's rectangle with colour 0, alpha 0 (source alpha blend, no texture, no tests): with the
   blend equation in force this leaves the picture and clears the destination alpha, which the gauge part uses as
   a mask. */
void HudSprite_DrawAlphaClear(HudCSprite *spr) {
    f32 v[4][4];
    u64 *p;

    v[0][0] = spr->x0;
    v[0][1] = spr->y0;
    v[0][3] = 1.0f;
    v[1][0] = spr->x1;
    v[1][1] = spr->y0;
    v[1][3] = 1.0f;
    v[2][0] = spr->x0;
    v[2][1] = spr->y1;
    v[2][3] = 1.0f;
    v[3][0] = spr->x1;
    v[3][1] = spr->y1;
    v[3][3] = 1.0f;
    Vu0Cur_MulVec4(v[0], v[0]);
    Vu0Cur_MulVec4(v[1], v[1]);
    Vu0Cur_MulVec4(v[2], v[2]);
    Vu0Cur_MulVec4(v[3], v[3]);
    v[0][1] = Sprite_ConvY(v[0][1]);
    v[1][1] = Sprite_ConvY(v[1][1]);
    v[2][1] = Sprite_ConvY(v[2][1]);
    v[3][1] = Sprite_ConvY(v[3][1]);
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(5, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x30000; /* alpha and depth tests always pass */
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = GFX_CLEAR_RGBAQ;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0x44; /* triangle strip, alpha blended */
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 4);
    p[1] = 0x5555;
    p += 2;
    p[0] = HUDC_XYZ_CONV(v[0]);
    p[1] = HUDC_XYZ_CONV(v[1]);
    p += 2;
    p[0] = HUDC_XYZ_CONV(v[2]);
    p[1] = HUDC_XYZ_CONV(v[3]);
    p += 2;
    Dma_EndDirect(p);
}

/* The same with alpha 0xFF. */
void HudSprite_DrawAlphaSet(HudCSprite *spr) {
    f32 v[4][4];
    u64 *p;

    v[0][0] = spr->x0;
    v[0][1] = spr->y0;
    v[0][3] = 1.0f;
    v[1][0] = spr->x1;
    v[1][1] = spr->y0;
    v[1][3] = 1.0f;
    v[2][0] = spr->x0;
    v[2][1] = spr->y1;
    v[2][3] = 1.0f;
    v[3][0] = spr->x1;
    v[3][1] = spr->y1;
    v[3][3] = 1.0f;
    Vu0Cur_MulVec4(v[0], v[0]);
    Vu0Cur_MulVec4(v[1], v[1]);
    Vu0Cur_MulVec4(v[2], v[2]);
    Vu0Cur_MulVec4(v[3], v[3]);
    v[0][1] = Sprite_ConvY(v[0][1]);
    v[1][1] = Sprite_ConvY(v[1][1]);
    v[2][1] = Sprite_ConvY(v[2][1]);
    v[3][1] = Sprite_ConvY(v[3][1]);
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(5, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x3F800000FF000000;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 0x44;
    p[1] = GS_PRIM;
    p += 2;
    p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 4);
    p[1] = 0x5555;
    p += 2;
    p[0] = HUDC_XYZ_CONV(v[0]);
    p[1] = HUDC_XYZ_CONV(v[1]);
    p += 2;
    p[0] = HUDC_XYZ_CONV(v[2]);
    p[1] = HUDC_XYZ_CONV(v[3]);
    p += 2;
    Dma_EndDirect(p);
}

/* Fills the frame with colour 0, alpha 0 in 16 strips of 32 x 448 (blended sprites, depth writes on), then sets
   the neutral colour and turns depth writes off. Hud_Draw calls it in front of each side's gauges. */
void HudGfx_ClearAlpha(void) {
    u64 *p = Dma_BeginDirect();
    s32 i;
    s32 n;
    u64 *tag;

    p[0] = GIF_TAG(3, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = GFX_CLEAR_RGBAQ;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 0);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0x46; /* sprite, alpha blended */
    p[1] = GS_PRIM;
    p += 2;
    tag = p;
    p += 2;
    i = 0;
    for (n = 0; n < 16; n++) {
        p[0] = ((i * 32 + 1792) << 4) | ((1824 << 4) << 16);
        p[1] = GS_SET_XYZ((i * 32 + 1792 + 32) << 4, (1824 + 448) << 4, 0);
        p += 2;
        i++;
    }
    tag[0] = GIF_TAG_EX(16, 1, GIF_FLG_REGLIST, 2);
    tag[1] = GS_REG_XYZ2 | (GS_REG_XYZ2 << 4);
    p[0] = GIF_TAG(2, 0, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x3F80000080808080;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = GS_SET_ZBUF(GFX_ZBP, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    Dma_EndDirect(p);
}

#define HUDC_FRAME_WORD() (!(gGfx.frame & 1) ? 0x80070 : 0x80000)

/* Queues the HUD's drawing state, the same for GS contexts 1 and 2: the frame's colour buffer, the depth buffer
   with writes off, the full-screen offset and scissor, source alpha blending, tests that always pass, bilinear
   filtering, no FBA, repeating textures, colour clamp and PRMODECONT. Called at the top of Hud_Draw. */
void HudGfx_SetEnv(void) {
    u32 pkt[88] = {
        0x10000015, 0, 0x10000000, 0x50000015,
        0x00008014, 0x10000000, GIF_REG_AD, 0,
        HUDC_FRAME_WORD(), 0, GS_FRAME_1, 0,
        HUDC_FRAME_WORD(), 0, GS_FRAME_2, 0,
        0x310000E0, 1, GS_ZBUF_1, 0,
        0x310000E0, 1, GS_ZBUF_2, 0,
        GFX_OFX, GFX_OFY, GS_XYOFFSET_1, 0,
        GFX_OFX, GFX_OFY, GS_XYOFFSET_2, 0,
        0x01FF0000, 0x01BF0000, GS_SCISSOR_1, 0,
        0x01FF0000, 0x01BF0000, GS_SCISSOR_2, 0,
        0x44, 0, GS_ALPHA_1, 0,
        0x44, 0, GS_ALPHA_2, 0,
        0x30000, 0, GS_TEST_1, 0,
        0x30000, 0, GS_TEST_2, 0,
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

/* Draws a sprite as a four-vertex triangle strip under the current VU0 matrix. A textured sprite uploads its
   texture (HudRes_UploadTex with tbp / cbp) and sets TEX0 of both GS contexts; `ctx2` draws through context 2. */
void HudSprite_DrawAt(HudCSprite *spr, HudCRes *res, u8 ctx2, s32 tbp, s32 cbp) {
    f32 v[4][4];
    s32 uv[4][2];
    HudCTex *t = NULL;
    HudCTex *pal = NULL;
    u64 *p;
    u64 *tag;
    s32 ctx = ctx2 != 0;

    if (spr->flags & HUDC_SPR_HIDDEN) {
        return;
    }
    memset(v, 0, sizeof(v));
    if (spr->tex >= 0) {
        HudRes_UploadTex(res, spr->tex, spr->texSub, tbp, cbp);
        t = &res->tex[spr->tex];
        pal = &res->tex[spr->tex + spr->texSub];
    }
    p = Dma_BeginDirect();
    tag = p;
    p += 2;
    v[0][0] = spr->x0;
    v[0][1] = spr->y0;
    v[0][3] = 1.0f;
    v[1][0] = spr->x1;
    v[1][1] = spr->y0;
    v[1][3] = 1.0f;
    v[2][0] = spr->x0;
    v[2][1] = spr->y1;
    v[2][3] = 1.0f;
    v[3][0] = spr->x1;
    v[3][1] = spr->y1;
    v[3][3] = 1.0f;
    Vu0Cur_MulVec4(v[0], v[0]);
    Vu0Cur_MulVec4(v[1], v[1]);
    Vu0Cur_MulVec4(v[2], v[2]);
    Vu0Cur_MulVec4(v[3], v[3]);
    v[0][1] = Sprite_ConvY(v[0][1]);
    v[1][1] = Sprite_ConvY(v[1][1]);
    v[2][1] = Sprite_ConvY(v[2][1]);
    v[3][1] = Sprite_ConvY(v[3][1]);
    if (spr->flags & HUDC_SPR_MIRROR) {
        uv[0][0] = spr->u1;
        uv[2][0] = spr->u1;
        uv[1][0] = spr->u0;
        uv[3][0] = spr->u0;
        uv[0][1] = spr->v0;
        uv[1][1] = spr->v0;
        uv[2][1] = spr->v1;
        uv[3][1] = spr->v1;
    } else {
        uv[0][0] = spr->u0;
        uv[2][0] = spr->u0;
        uv[1][0] = spr->u1;
        uv[3][0] = spr->u1;
        uv[0][1] = spr->v0;
        uv[1][1] = spr->v0;
        uv[2][1] = spr->v1;
        uv[3][1] = spr->v1;
    }
    if (spr->tex >= 0) {
        s32 block = t->imageBlock + tbp;
        u64 clut = (u64)(pal->clutBlock + cbp) << 37;

        p[0] = (u64)spr->r | ((u64)spr->g << 8) | ((u64)spr->b << 16) | ((u64)spr->a << 24);
        p[1] = t->tex0 | clut | block | ((u64)0x8000 << 19);
        p += 2;
        p[0] = t->tex0 | clut | block | ((u64)0x8000 << 19);
        p[1] = ((u64)ctx << 9) | 0x154; /* triangle strip, textured, alpha blended, UV coordinates */
        p += 2;
    } else {
        p[0] = (u64)spr->r | ((u64)spr->g << 8) | ((u64)spr->b << 16) | ((u64)spr->a << 24);
        p[1] = ((u64)ctx << 9) | 0x44;
        p += 2;
    }
    p[0] = ((uv[0][0] << 4) + 8) | ((u64)((uv[0][1] << 4) + 8) << 16);
    p[1] = HUDC_XYZ(v[0]);
    p += 2;
    p[0] = ((uv[1][0] << 4) + 8) | ((u64)((uv[1][1] << 4) + 8) << 16);
    p[1] = HUDC_XYZ(v[1]);
    p += 2;
    p[0] = ((uv[2][0] << 4) + 8) | ((u64)((uv[2][1] << 4) + 8) << 16);
    p[1] = HUDC_XYZ(v[2]);
    p += 2;
    p[0] = ((uv[3][0] << 4) + 8) | ((u64)((uv[3][1] << 4) + 8) << 16);
    p[1] = HUDC_XYZ(v[3]);
    p += 2;
    if (spr->tex >= 0) {
        tag[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 12);
        tag[1] = 0x535353530671; /* RGBAQ, TEX0_2, TEX0_1, PRIM, 4 x (UV, XYZ2) */
    } else {
        tag[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 10);
        tag[1] = 0x5353535301; /* RGBAQ, PRIM, 4 x (UV, XYZ2) */
    }
    Dma_EndDirect(p);
}

#define HUDC_RGBA(c) \
    ((u64)(((c) >> 24) & 0xFF) | ((u64)(((c) >> 16) & 0xFF) << 8) | ((u64)(((c) >> 8) & 0xFF) << 16) | \
     ((u64)((c) & 0xFF) << 24))

/* Draws a sprite like HudSprite_DrawAt (textures at HUDC_GRADIENT_TBP / CBP), but with a colour per vertex instead
   of the sprite's own: colorL on the two left corners, colorR on the two right ones, each 0xRRGGBBAA. */
void HudSprite_DrawGradient(HudCSprite *spr, HudCRes *res, u8 ctx2, s32 colorL, s32 colorR) {
    f32 v[4][4];
    s32 uv[4][2];
    HudCTex *t = NULL;
    HudCTex *pal = NULL;
    u64 *p;
    u64 *tag;
    s32 ctx = ctx2 != 0;

    if (spr->flags & HUDC_SPR_HIDDEN) {
        return;
    }
    memset(v, 0, sizeof(v));
    if (spr->tex >= 0) {
        HudRes_UploadTex(res, spr->tex, spr->texSub, HUDC_GRADIENT_TBP, HUDC_GRADIENT_CBP);
        t = &res->tex[spr->tex];
        pal = &res->tex[spr->tex + spr->texSub];
    }
    p = Dma_BeginDirect();
    tag = p;
    p += 2;
    v[0][0] = spr->x0;
    v[0][1] = spr->y0;
    v[0][3] = 1.0f;
    v[1][0] = spr->x1;
    v[1][1] = spr->y0;
    v[1][3] = 1.0f;
    v[2][0] = spr->x0;
    v[2][1] = spr->y1;
    v[2][3] = 1.0f;
    v[3][0] = spr->x1;
    v[3][1] = spr->y1;
    v[3][3] = 1.0f;
    Vu0Cur_MulVec4(v[0], v[0]);
    Vu0Cur_MulVec4(v[1], v[1]);
    Vu0Cur_MulVec4(v[2], v[2]);
    Vu0Cur_MulVec4(v[3], v[3]);
    v[0][1] = Sprite_ConvY(v[0][1]);
    v[1][1] = Sprite_ConvY(v[1][1]);
    v[2][1] = Sprite_ConvY(v[2][1]);
    v[3][1] = Sprite_ConvY(v[3][1]);
    if (spr->flags & HUDC_SPR_MIRROR) {
        uv[0][0] = spr->u1;
        uv[2][0] = spr->u1;
        uv[1][0] = spr->u0;
        uv[3][0] = spr->u0;
        uv[0][1] = spr->v0;
        uv[1][1] = spr->v0;
        uv[2][1] = spr->v1;
        uv[3][1] = spr->v1;
    } else {
        uv[0][0] = spr->u0;
        uv[2][0] = spr->u0;
        uv[1][0] = spr->u1;
        uv[3][0] = spr->u1;
        uv[0][1] = spr->v0;
        uv[1][1] = spr->v0;
        uv[2][1] = spr->v1;
        uv[3][1] = spr->v1;
    }
    if (spr->tex >= 0) {
        s32 block = t->imageBlock + HUDC_GRADIENT_TBP;
        u64 clut = (u64)(pal->clutBlock + HUDC_GRADIENT_CBP) << 37;

        p[0] = t->tex0 | clut | block | ((u64)0x8000 << 19);
        p[1] = t->tex0 | clut | block | ((u64)0x8000 << 19);
        p += 2;
        p[0] = ((u64)ctx << 9) | 0x154;
        p[1] = HUDC_RGBA(colorL);
        p += 2;
    } else {
        p[0] = ((u64)ctx << 9) | 0x44;
        p[1] = HUDC_RGBA(colorL);
        p += 2;
    }
    p[0] = ((uv[0][0] << 4) + 8) | ((u64)((uv[0][1] << 4) + 8) << 16);
    p[1] = HUDC_XYZ(v[0]);
    p += 2;
    p[0] = HUDC_RGBA(colorR);
    p[1] = ((uv[1][0] << 4) + 8) | ((u64)((uv[1][1] << 4) + 8) << 16);
    p += 2;
    p[0] = HUDC_XYZ(v[1]);
    p[1] = HUDC_RGBA(colorL);
    p += 2;
    p[0] = ((uv[2][0] << 4) + 8) | ((u64)((uv[2][1] << 4) + 8) << 16);
    p[1] = HUDC_XYZ(v[2]);
    p += 2;
    p[0] = HUDC_RGBA(colorR);
    p[1] = ((uv[3][0] << 4) + 8) | ((u64)((uv[3][1] << 4) + 8) << 16);
    p += 2;
    p[0] = HUDC_XYZ(v[3]);
    p[1] = 0;
    p += 2;
    if (spr->tex >= 0) {
        tag[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 15);
        tag[1] = 0x0531531531531067; /* TEX0_2, TEX0_1, PRIM, 4 x (RGBAQ, UV, XYZ2) */
    } else {
        tag[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 13);
        tag[1] = 0x5315315315310; /* PRIM, 4 x (RGBAQ, UV, XYZ2) */
    }
    Dma_EndDirect(p);
}
