#ifndef BATTLE_HUD_D_H
#define BATTLE_HUD_D_H

#include "types.h"
#include "sys/ramp.h"

/*
 * Battle HUD, 0x226488-0x22A750 (src/battle/hud_d.c: the tail of the sprite / node library; src/battle/hud_d_b.c:
 * the animations of the notice part, i.e. the full-screen announcements at the start and end of a round).
 * The sprite and node layouts are local views that agree with HudSprite / HudNode of include/battle/hud_a.h.
 */

/* A HUD sprite (0x1C bytes). */
typedef struct HudDSprite {
    /* 0x00 */ u32 flags;   /* bit 0: hidden, bit 1: mirrored */
    /* 0x04 */ s16 tex;
    /* 0x06 */ s16 texSub;
    /* 0x08 */ s16 pos[4];  /* screen rectangle x0, x1, y0, y1 relative to the node */
    /* 0x10 */ s16 uv[4];   /* texel rectangle u0, u1, v0, v1 */
    /* 0x18 */ u8 r, g, b, a;
} HudDSprite; /* size 0x1C */

/* A HUD node (0x38 bytes). */
typedef struct HudDNode {
    /* 0x00 */ u32 flags;   /* bit 0: hidden, bit 1: mirrored */
    /* 0x04 */ f32 rot;     /* rotation about Z, radians (HudNode_SetRot) */
    /* 0x08 */ f32 unk8;    /* HudNode_SetUnk8: HudNode_Draw does not read it */
    /* 0x0C */ f32 unkC;
    /* 0x10 */ s32 x;       /* HudNode_SetPos */
    /* 0x14 */ s32 y;
    /* 0x18 */ s32 ofsX;    /* HudNode_SetOfs */
    /* 0x1C */ s32 ofsY;
    /* 0x20 */ u32 sprCount;
    /* 0x24 */ HudDSprite **sprList;
    /* 0x28 */ u32 childCount;
    /* 0x2C */ struct HudDNode **childList;
    /* 0x30 */ void (*update)();
    /* 0x34 */ void (*draw)();
} HudDNode; /* size 0x38 */

/* Notice part work (gHudNotice): what 0x226BA8..0x22A750 uses. */
typedef struct HudNotice {
    /* 0x00 */ void *res;          /* sprite sheet 1 of the HUD file */
    /* 0x04 */ HudDSprite *spr;    /* 16: [0] the word, [1..12] its fragments / copies, [13] the streak, [14] the band,
                                      [15] the replay mark (hud_e.c) */
    /* 0x08 */ HudDNode *node;     /* 4: [1] the announcement (its update / draw are set by HudNotice_Show, 0x22AB50),
                                      [2] the band (only its rotation is used, by HudNotice_DrawReady) */
    /* 0x0C */ s32 state;          /* step of the running announcement; 0 when one is started */
    /* 0x10 */ s32 streak;         /* frame of the streak animation, 0..2 */
    /* 0x14 */ Ramp rampA;         /* value at 0x24 */
    /* 0x2C */ Ramp rampB;         /* value at 0x3C */
    /* 0x44 */ f32 trail[12];      /* history of an animation value, [0] newest: the after-images (8 used here) */
} HudNotice; /* size 0x74 (allocated by HudNotice_Init, hud_e.c) */

#define HUD_NOTICE_SPR_WORD 0
#define HUD_NOTICE_SPR_PART 1
#define HUD_NOTICE_SPR_STREAK 13
#define HUD_NOTICE_SPR_BAND 14

extern HudNotice *gHudNotice;

/* hud_d.c */
void HudSprite_Draw(HudDSprite *spr, void *res, s32 additive);
void HudNode_Show(HudDNode *node, s32 show);
void HudNode_SetPos(HudDNode *node, s32 x, s32 y);
void HudNode_SetOfs(HudDNode *node, s32 x, s32 y);
void HudNode_SetRot(HudDNode *node, f32 rot);
void HudNode_SetUnk8(HudDNode *node, f32 a, f32 b);

#endif
