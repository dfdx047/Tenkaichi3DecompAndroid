#include "common.h"
#include "sys/bpe.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/sprite.h"

extern void TexFile_UploadOne(SpriteRes *res, s32 index, s32 tbp, s32 cbp); /* uploads one texture and its palette */

/* Unpacks a BPE-compressed sprite sheet. */
void *Sprite_Unpack(void *src, void *dst, s32 *rawSize) {
    return Bpe_Decode(src, dst, rawSize);
}

/* Converts a screen Y; the identity in this build. */
s32 Sprite_ConvY(s32 y) {
    return y;
}

/* Draws textures 0..7 of a sheet as a 512x448 picture (4 x 2 tiles of 128x256 texels shown 128x224). */
/* The tile's GIF tag is written in two statements, tag word first (`p[0] = ...; p[0] |= ...; p[1] = regs;`): the
   compiler folds them into one constant but emits the stores in the original's order (both words loaded, then
   `sd regs,8 / sd tag,0`), and the instruction count in front of the stores also settles the scheduling at the
   top of the row loop. One statement per word gives the same operations with 12 of 150 instructions in another
   order / register (either store order). WHICH bits the second statement adds cannot be told: every split of
   the four fields matches (found by decomp-permuter as `p[0] = 1 << 15; p[0] = 1 | p[0] | ...`); the packed tag
   plus the REGLIST flag is the reading chosen here. */
void Sprite_DrawPicture(SpriteRes *res, s32 x, s32 y, s32 alpha) {
    u64 *p;
    s32 row;
    s32 col;
    s32 idx;
    s32 yy;
    s32 y0;
    s32 y1;

    if (alpha == 0) {
        return;
    }
    Gfx_AddDefaultEnv();
    yy = y;
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(5, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x44; /* Cs * As + Cd * (1 - As) */
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_FBA_1;
    p += 2;
    p[0] = 0x30000; /* alpha and depth tests always pass */
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = ((u64)alpha << 24) | 0x808080;
    p[1] = GS_RGBAQ;
    p += 2;
    p[0] = 5; /* clamp S and T */
    p[1] = GS_CLAMP_1;
    p += 2;
    Dma_EndDirect(p);
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 4; col++) {
            idx = col + row * 4;
            TexFile_UploadOne(res, idx, SPRITE_TBP, res->tex[idx].cbpOfs + SPRITE_CBP_PICTURE);
            Dma_AddTexFlush();
            p = Dma_BeginDirect();
            y1 = row ? yy + 0xC0 : yy + 0xE0;
            y0 = row ? yy - 0x20 : yy;
            y0 = (y0 << 4) + SPRITE_OFS_Y;
            p[0] = GIF_TAG(1, 1, 6);
            p[0] |= (u64)GIF_FLG_REGLIST << 58;
            p[1] = 0x535306; /* TEX0_1, PRIM, UV, XYZ2, UV, XYZ2 */
            p += 2;
            p[0] = res->tex[idx].tex0 | ((u64)(res->tex[idx].cbpOfs + SPRITE_CBP_PICTURE) << 37) |
                   (((u64)4 << 32) | SPRITE_TBP);
            p[1] = 0x156; /* sprite, textured, alpha blended, UV coordinates */
            p += 2;
            p[0] = 0;
            p[1] = ((x + col * 0x80 + 0x700) << 4) | ((u64)y0 << 16);
            p += 2;
            p[0] = GS_SET_UV(0x80 << 4, 0x100 << 4);
            p[1] = ((x + col * 0x80 + 0x780) << 4) | ((u64)((y1 + 0x720) << 4) << 16);
            p += 2;
            Dma_EndDirect(p);
        }
        yy += 0x100;
    }
}

/* Draws a run of sprite records at an offset; the run ends at the first record with LOAD_SPR_END. */
void Sprite_DrawList(SpriteRes *res, s32 x, s32 y, LoadSprite *list) {
    u64 *p;
    s32 i;

    if (list->a == 0) {
        return;
    }
    Gfx_AddDefaultEnv();
    p = Dma_BeginDirect();
    p[0] = GIF_TAG(3, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x44; /* Cs * As + Cd * (1 - As) */
    p[1] = GS_ALPHA_1;
    p += 2;
    p[0] = 0x60; /* bilinear */
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    Dma_EndDirect(p);
    for (i = 0; !(list[i].flags & LOAD_SPR_END); i++) {
        LoadSprite *spr = &list[i];

        if (spr->flags & LOAD_SPR_DRAW) {
            if (res != NULL) {
                TexFile_UploadOne(res, spr->tex, SPRITE_TBP, res->tex[spr->tex].cbpOfs + SPRITE_CBP_LIST);
                Dma_AddTexFlush();
            }
            p = Dma_BeginDirect();
            p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 8);
            p[1] = 0x535306E1; /* RGBAQ, A+D (ignored), TEX0_1, PRIM, UV, XYZ2, UV, XYZ2 */
            p += 2;
            p[0] = (u64)spr->r | ((u64)spr->g << 8) | ((u64)spr->b << 16) | ((u64)spr->a << 24);
            p[1] = 0;
            p += 2;
            if ((spr->flags & 4) || res == NULL) {
                p[0] = 0;
                p[1] = 0x46;
            } else {
                p[0] = res->tex[spr->tex].tex0 | ((u64)(res->tex[spr->tex].cbpOfs + SPRITE_CBP_LIST) << 37) |
                       (((u64)4 << 32) | SPRITE_TBP);
                p[1] = 0x156;
            }
            p += 2;
            p[0] = (spr->u0 << 4) | ((u64)(spr->v0 << 4) << 16);
            p[1] = (((spr->x0 + x) << 4) + SPRITE_OFS_X) | ((u64)((Sprite_ConvY(spr->y0 + y) << 4) + SPRITE_OFS_Y) << 16);
            p += 2;
            p[0] = (spr->u1 << 4) | ((u64)(spr->v1 << 4) << 16);
            p[1] = (((spr->x1 + x) << 4) + SPRITE_OFS_X) | ((u64)((Sprite_ConvY(spr->y1 + y) << 4) + SPRITE_OFS_Y) << 16);
            p += 2;
            Dma_EndDirect(p);
        }
    }
}

/* Returns 0; no callers. */
s32 Sprite_Stub126B08(void) {
    return 0;
}

/* Sets the scissor rectangle from exclusive right/bottom edges at the screen border. */
void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1) {
    s32 bottom = (y1 == 0x1C0) ? 0x1BF : y1;
    s32 right = (x1 == 0x200) ? 0x1FF : x1;

    Dma_AddScissor(x0, right, Sprite_ConvY(y0), Sprite_ConvY(bottom));
}
