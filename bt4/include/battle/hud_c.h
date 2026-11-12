#ifndef BATTLE_HUD_C_H
#define BATTLE_HUD_C_H

#include "types.h"
#include "sys/ramp.h"

/*
 * Battle HUD, 0x222400-0x226488: the tail of the gauge part (src/battle/hud_c.c), the combo part
 * (src/battle/hud_c_b.c) and the sprite library every HUD part draws with (src/battle/hud_c_c.c).
 *
 * The sprite, texture, sheet and node layouts below are local views; they agree with HudSprite / HudTex / HudRes /
 * HudNode of include/battle/hud_a.h and add the fields that only the sprite library reads.
 */

/* A HUD sprite (0x1C bytes). */
typedef struct HudCSprite {
    /* 0x00 */ union {
        u32 flags;          /* HUDC_SPR_HIDDEN, HUDC_SPR_MIRROR */
        struct {
            u32 hidden : 1; /* not drawn (HudSprite_Show writes !show) */
            u32 mirror : 1; /* u0 and u1 are exchanged when drawing (HudSprite_SetMirror) */
        };
    };
    /* 0x04 */ s16 tex;     /* texture entry of the sheet; negative: an untextured rectangle */
    /* 0x06 */ u16 texSub;  /* tex + texSub is the entry whose palette is used */
    /* 0x08 */ s16 x0;      /* rectangle relative to the node (the current VU0 matrix), pixels */
    /* 0x0A */ s16 x1;
    /* 0x0C */ s16 y0;
    /* 0x0E */ s16 y1;
    /* 0x10 */ s16 u0;      /* texel rectangle */
    /* 0x12 */ s16 u1;
    /* 0x14 */ s16 v0;
    /* 0x16 */ s16 v1;
    /* 0x18 */ u8 r;        /* 0x80 is neutral */
    /* 0x19 */ u8 g;
    /* 0x1A */ u8 b;
    /* 0x1B */ u8 a;
} HudCSprite; /* size 0x1C */

#define HUDC_SPR_HIDDEN 1
#define HUDC_SPR_MIRROR 2

/* One texture entry of a HUD sprite sheet (0x40 bytes). */
typedef struct HudCTex {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ s32 imageSize;  /* bytes of the image transfer packet (0: none) */
    /* 0x0C */ s32 clutSize;   /* bytes of the palette transfer packet (0: none) */
    /* 0x10 */ u8 unk10[8];
    /* 0x18 */ s32 imageWidth; /* BITBLTBUF destination width (units of 64 pixels) of the image */
    /* 0x1C */ s32 clutWidth;  /* the same for the palette */
    /* 0x20 */ s32 imageBlock; /* VRAM block of the image, relative to the block base given when drawing */
    /* 0x24 */ s32 clutBlock;  /* VRAM block of the palette, relative to the palette base */
    /* 0x28 */ s32 mark;       /* uploaded this frame; cleared by Hud_ClearTexMarks at the end of Hud_Draw */
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ u64 tex0;       /* GS TEX0 without TBP0, TCC and CBP */
    /* 0x38 */ void *image;    /* image transfer packet (referenced by the display list) */
    /* 0x3C */ void *clut;     /* palette transfer packet */
} HudCTex; /* size 0x40 */

/* A HUD sprite sheet after Res_RelocateOffsets. */
typedef struct HudCRes {
    /* 0x00 */ s32 texCount;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ HudCTex *tex;
} HudCRes;

/* A HUD node (0x38 bytes). */
typedef struct HudCNode HudCNode;
struct HudCNode {
    /* 0x00 */ u32 flags;
    /* 0x04 */ f32 rot;
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ s32 x;
    /* 0x14 */ s32 y;
    /* 0x18 */ s32 ofsX;
    /* 0x1C */ s32 ofsY;
    /* 0x20 */ u32 spriteCount;
    /* 0x24 */ HudCSprite **sprites;
    /* 0x28 */ u32 childCount;
    /* 0x2C */ HudCNode **children;
    /* 0x30 */ void (*update)();
    /* 0x34 */ void (*draw)();
}; /* size 0x38 */

/* VRAM blocks HudSprite_DrawGradient uploads to. */
#define HUDC_GRADIENT_TBP 0x2A00
#define HUDC_GRADIENT_CBP 0x2C80

/* hud_c.c: gauge part */
void HudGauge_Term(void);
void HudGauge_Reset(void);

/* hud_c_b.c: combo part */
void HudCombo_SelectSide(s32 side);
void HudCombo_SetMessage(s32 side, s32 message);
void HudCombo_SetText(s32 side, s32 text);
void HudCombo_SetDamage(s32 side, s32 damage, s32 isNew);
void HudCombo_SetHits(s32 side, u32 hits);
void HudCombo_SlideOut(f32 seconds);
void HudCombo_SlideIn(f32 seconds);
void HudCombo_DrawText(void);
void HudCombo_Init(HudCNode **out, HudCRes *res);
void HudCombo_Term(void);
void HudCombo_Reset(void);

/* hud_c_c.c: sprite library */
void HudGfx_CallBegin(void (*fn)(s32));
void HudGfx_CallEnd(void (*fn)(s32));
void HudSprite_Show(HudCSprite *spr, s32 show);
void HudSprite_SetMirror(HudCSprite *spr, s32 mirror);
void HudSprite_SetRect(HudCSprite *spr, s32 x0, s32 x1, s32 y0, s32 y1);
void HudSprite_SetUv(HudCSprite *spr, s32 u0, s32 u1, s32 v0, s32 v1);
void HudSprite_SetTex(HudCSprite *spr, s32 tex, s32 sub);
void HudSprite_SetColor(HudCSprite *spr, s32 r, s32 g, s32 b, s32 a);
void HudSprite_InitPlain(HudCSprite *spr, u8 r, u8 g, u8 b, u8 a);
void HudSprite_Move(HudCSprite *spr, s32 dx, s32 dy);
void HudSprite_MoveUv(HudCSprite *spr, s32 du, s32 dv);
void HudSprite_FlipU(HudCSprite *spr);
void HudSprite_FlipV(HudCSprite *spr);
void HudSprite_Scale(HudCSprite *spr, f32 sx, f32 sy);
void HudSprite_Center(HudCSprite *spr);
void HudSprite_InitTex(HudCSprite *spr, HudCRes *res, s32 tex, s32 sub);
void HudRes_UploadTex(HudCRes *res, s32 tex, s32 sub, s32 tbp, s32 cbp);
void HudSprite_DrawAlphaClear(HudCSprite *spr);
void HudSprite_DrawAlphaSet(HudCSprite *spr);
void HudGfx_ClearAlpha(void);
void HudGfx_SetEnv(void);
void HudSprite_DrawAt(HudCSprite *spr, HudCRes *res, u8 ctx2, s32 tbp, s32 cbp);
void HudSprite_DrawGradient(HudCSprite *spr, HudCRes *res, u8 ctx2, s32 colorL, s32 colorR);

#endif
