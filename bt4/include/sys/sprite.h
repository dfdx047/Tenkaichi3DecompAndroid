#ifndef SYS_SPRITE_H
#define SYS_SPRITE_H

#include "types.h"
#include "sys/loading.h"

/*
 * 2D sprite helpers. Source range 0x126608-0x126B88 (src/sys/sprite.c).
 *
 * A sprite sheet ("resource") is a BPE-packed file. Once unpacked, the word at +0x10 points to an array of
 * 0x40-byte texture entries (SpriteTex); the entry index is LoadSprite.tex. TexFile_UploadOne(res, index, tbp, cbp)
 * uploads entry `index`: its pixels to GS block `tbp` and its palette to block `cbp` (either is skipped when
 * negative). This module always uses tbp 0x3000 and cbp = 0x3000 (+ 0x40 for the list drawer) + entry->cbpOfs,
 * so only one texture is in video memory at a time and it is uploaded again for every sprite drawn.
 *
 * Screen coordinates are pixels with the origin at the top left of the 512x448 frame; the GS primitive
 * coordinate is (x + 1792) * 16, (y + 1824) * 16. Texture coordinates are texels (sent as texel * 16).
 */

#define SPRITE_TBP 0x3000       /* GS block the pixels of the current texture are uploaded to */
#define SPRITE_CBP_PICTURE 0x3000 /* palette base used by Sprite_DrawPicture */
#define SPRITE_CBP_LIST 0x3040  /* palette base used by Sprite_DrawList */
#define SPRITE_OFS_X 0x7000     /* 1792 << 4 */
#define SPRITE_OFS_Y 0x7200     /* 1824 << 4 */

/* One texture of a sprite sheet (0x40 bytes); only the fields this module reads. */
typedef struct SpriteTex {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s32 cbpOfs;  /* added to the palette base block */
    /* 0x14 */ u8 unk14[0x1C];
    /* 0x30 */ u64 tex0;    /* GS TEX0 without TBP0, TCC and CBP (size, format, palette format) */
    /* 0x38 */ u8 unk38[8];
} SpriteTex;

/* Header of an unpacked sprite sheet; only the fields this module reads. */
typedef struct SpriteRes {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ SpriteTex *tex;
} SpriteRes;

void *Sprite_Unpack(void *src, void *dst, s32 *rawSize);
s32 Sprite_ConvY(s32 y);
void Sprite_DrawPicture(SpriteRes *res, s32 x, s32 y, s32 alpha);
void Sprite_DrawList(SpriteRes *res, s32 x, s32 y, LoadSprite *list);
s32 Sprite_Stub126B08(void);
void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);

#endif
