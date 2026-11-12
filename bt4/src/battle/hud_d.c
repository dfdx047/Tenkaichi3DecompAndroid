#include "common.h"
#include "battle/hud_d.h"

/*
 * Battle HUD: the last functions of the sprite / node library (0x224B50-0x226500), 0x226488-0x226500.
 * Every HUD part calls them (the gauge, team, caption, notice, combo and prompt parts).
 */

extern void HudSprite_DrawAt(HudDSprite *spr, void *res, u8 additive, s32 a, s32 b);

/* Draws a sprite of a sheet; `additive` picks the blend. 0x2A00 / 0x2C80 are the VRAM blocks HudSprite_DrawAt
   (0x225A50) uploads the texture and its palette to. */
void HudSprite_Draw(HudDSprite *spr, void *res, s32 additive) {
    HudSprite_DrawAt(spr, res, additive, 0x2A00, 0x2C80);
}

/* Shows or hides a node (and with it everything under it: HudNode_Draw stops at a hidden node). */
void HudNode_Show(HudDNode *node, s32 show) {
    node->flags = (node->flags & ~1) | (show == 0);
}

/* Sets a node's position relative to its parent. */
void HudNode_SetPos(HudDNode *node, s32 x, s32 y) {
    node->x = x;
    node->y = y;
}

/* Sets a node's extra offset (the combo part's nodes; the shake of the gauge part writes the fields directly). */
void HudNode_SetOfs(HudDNode *node, s32 x, s32 y) {
    node->ofsX = x;
    node->ofsY = y;
}

/* Sets a node's rotation about Z in radians. */
void HudNode_SetRot(HudDNode *node, f32 rot) {
    node->rot = rot;
}

/* Sets the two floats behind the rotation. */
void HudNode_SetUnk8(HudDNode *node, f32 a, f32 b) {
    node->unk8 = a;
    node->unkC = b;
}
