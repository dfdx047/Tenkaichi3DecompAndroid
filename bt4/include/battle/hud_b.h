#ifndef BATTLE_HUD_B_H
#define BATTLE_HUD_B_H

#include "types.h"
#include "sys/ramp.h"

/*
 * Battle HUD, gauge part: 0x21CA60-0x222400 (src/battle/hud_b.c). See the top of that file.
 * The sprite and node layouts are local views that agree with HudSprite / HudNode of include/battle/hud_a.h
 * (the neighbouring range); they are kept separate because both files were written at the same time.
 */

/* A HUD sprite (0x1C bytes); written through the setters at 0x224B90..0x224CA0. */
typedef struct HudBSprite {
    /* 0x00 */ u32 flags;   /* bit 0: hidden, bit 1: mirrored */
    /* 0x04 */ s16 tex;     /* texture entry of the sheet, -1 = untextured */
    /* 0x06 */ s16 texSub;
    /* 0x08 */ s16 pos[4];  /* screen rectangle x0, x1, y0, y1 relative to the node (HudSprite_SetRect) */
    /* 0x10 */ s16 uv[4];   /* texel rectangle u0, u1, v0, v1 (HudSprite_SetUv) */
    /* 0x18 */ u8 r, g, b, a;
} HudBSprite; /* size 0x1C */

/* Shake of one HUD node: `count` frames left, a random offset of +-amp each frame. */
typedef struct HudBShake {
    /* 0x00 */ u16 count;
    /* 0x02 */ u16 amp;
} HudBShake;

/* A HUD node (0x38 bytes): it owns a list of sprites and a list of child nodes. HudNode_Update calls `update`
 * (with the node still in $a0) and then the children; HudNode_Draw calls `draw(node)`, which draws the sprites. */
typedef struct HudBGroup {
    /* 0x00 */ u32 flags;   /* bit 0: hidden, bit 1: mirrored (the right-hand player's copy) */
    /* 0x04 */ u8 unk4[0xC];
    /* 0x10 */ s32 x;       /* position relative to the parent (HudNode_SetPos) */
    /* 0x14 */ s32 y;
    /* 0x18 */ s32 ofsX;    /* extra offset: the shake */
    /* 0x1C */ s32 ofsY;
    /* 0x20 */ u32 sprCount;
    /* 0x24 */ HudBSprite **sprList;
    /* 0x28 */ u32 childCount;
    /* 0x2C */ struct HudBGroup **childList;
    /* 0x30 */ void (*update)();
    /* 0x34 */ void (*draw)();
} HudBGroup; /* size 0x38 */

struct HudBObj;

/* Interface used by the HUD manager (src/battle/hud_a.c). */
void HudGauge_SelectSide(s32 side);
void HudGauge_SetHp(s32 side, s32 hp);
void HudGauge_SetKi(s32 side, s32 ki);
void HudGauge_SetMaxPower(s32 side, s32 value);
void HudGauge_SetBlast(s32 side, s32 blast);
void HudGauge_SetStatIcons(s32 side, s32 mask, s32 pos);
void HudGauge_SetKiReserve(s32 side, s32 value);
void HudGauge_ShakeHp(s32 side, u16 count, u16 amp);
void HudGauge_ShakeKi(s32 side, u16 count, u16 amp);
void HudGauge_SetFighter(s32 side, struct HudBObj *obj);
void HudGauge_SlideOut(f32 seconds);
void HudGauge_SlideIn(f32 seconds);
void HudGauge_SetSwitching(s32 side, s32 switching);
void HudGauge_Init(HudBGroup **out, void *res);

#endif
