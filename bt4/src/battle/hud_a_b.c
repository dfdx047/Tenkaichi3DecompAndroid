#include "common.h"
#include "battle/hud_a.h"
#include "battle/battle.h"
#include "sys/heap.h"
#include "sys/dma.h"
#include "sys/gfx.h"

/*
 * HUD team panel. Source range 0x219EB0-0x21BCA0.
 *
 * Shows, for each side that has members in reserve, the face of the member that a switch would bring in, that
 * member's health bar and the switch gauge. One set of nodes and sprites serves both sides: Hud_Draw calls
 * HudTeam_SelectSide(side) and then updates / draws the tree, mirrored for side 1.
 *
 * Node tree (gHudTeam->nodes[]):
 *   0 root (12, 66)            update HudTeam_UpdateRoot (slide, flip)
 *     1 current face group     pos = flipA
 *       3 face A (14, 0)       sprites 0 (frame), 8 (face of target[0])
 *         5 health bar A       sprites 1 (bar), 3 (mask)
 *         7 switch gauge       sprites 5 (bar), 6 (mask), 7 ("full" marker)
 *       8 plate                sprites 10, 11
 *     2 previous face group    pos = flipB; shown until its flip ends
 *       4 face B (14, 0)       sprites 0, 9 (face of target[1])
 *         6 health bar B       sprites 2, 4
 *         7 switch gauge
 *       8 plate
 */

/* Local view of BattleMember: the member's face sprite sheet. */
typedef struct HudTeamMember {
    /* 0x00 */ u8 unk0[0x58];
    /* 0x58 */ HudRes *faceRes;
} HudTeamMember;

extern void *memset(void *dst, s32 c, u32 n);
extern HudTeamMember *BattleSide_GetMember(s32 side, s32 idx);

/* The team panel work. Defined here: this object's .sdata (0x2FEB40). */
HudTeam *gHudTeam = NULL;

/* Sprite / node helpers (0x224B50..0x2264C8, not decompiled yet). */
extern void HudGfx_CallBegin(void (*gsBegin)(void));     /* calls it */
extern void HudGfx_CallEnd(void (*gsEnd)(void));       /* calls it */
extern void HudSprite_Show(HudSprite *spr, s32 show);
#ifdef PORT
/* PC build, online play: the player who joined is side 1 of the game, and would have their own gauges, team, prompts
   and counters on the right. With the swap (gs/net.c) the two sides change places on THIS copy's screen only: a
   side's data is the same, where it is drawn is the other side's place. HUD_SCR(side) is the place of a side. */
extern int Port_HudSwap(void);
#define HUD_SCR(side) ((side) ^ Port_HudSwap())
#else
#define HUD_SCR(side) (side)
#endif
extern void HudSprite_SetMirror(HudSprite *spr, s32 mirror);
extern void HudSprite_SetRect(HudSprite *spr, s32 x0, s32 x1, s32 y0, s32 y1);
extern void HudSprite_SetUv(HudSprite *spr, s32 u0, s32 u1, s32 v0, s32 v1);
extern void HudSprite_SetColor(HudSprite *spr, s32 r, s32 g, s32 b, s32 a);
extern void HudSprite_InitPlain(HudSprite *spr, s32 r, s32 g, s32 b, s32 a); /* untextured, colour */
extern void HudSprite_Move(HudSprite *spr, s32 dx, s32 dy);            /* moves the screen rectangle */
extern void HudSprite_InitTex(HudSprite *spr, HudRes *res, s32 tex, s32 sub); /* texture, full size, neutral colour */
extern void HudSprite_DrawAlphaClear(HudSprite *spr);                            /* draws the rectangle as the mask */
extern void HudSprite_DrawAt(HudSprite *spr, HudRes *res, s32 additive, s32 tbp, s32 cbp);
extern void HudSprite_Draw(HudSprite *spr, HudRes *res, s32 additive);  /* 0x225A50 with blocks 0x2A00 / 0x2C80 */
extern void HudNode_Show(HudNode *node, s32 show);
extern void HudNode_SetPos(HudNode *node, s32 x, s32 y);

/* GS state before a mask is drawn: context 2 writes only the alpha channel of the frame buffer (FBMSK
   0x00FFFFFF) with FBA on, always passing. */
void HudTeam_GsBeginMask(void) {
    u32 pkt[24] = {
        DMA_TAG_CNT | 5, 0, VIF_FLUSHE, VIF_DIRECT | 5,
        GIF_EOP | 4, 0x10000000, GIF_REG_AD, 0,
        0x34000, 0, GS_TEST_1, 0,
        GFX_FRAME_REG(), 0xFFFFFF, GS_FRAME_2, 0,
        1, 0, GS_FBA_2, 0,
        0x3000F, 0, GS_TEST_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* GS state after the masked sprite: context 2 writes all channels again, FBA off, normal test. */
void HudTeam_GsEndMask(void) {
    u32 pkt[24] = {
        DMA_TAG_CNT | 5, 0, VIF_FLUSHE, VIF_DIRECT | 5,
        GIF_EOP | 4, 0x10000000, GIF_REG_AD, 0,
        0x30000, 0, GS_TEST_1, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
        0, 0, GS_FBA_2, 0,
        0x30000, 0, GS_TEST_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Root node: steps the slide and flip ramps (not while paused) and places the root and the two face groups.
   The root moves by (-256, -224) * (slide + hide). A face group is at (-|v| * 128, v * 32) for its flip value v. */
void HudTeam_UpdateRoot(HudNode *node) {
    Ramp *slide = &gHudTeam->slide[gHudTeam->side];
    Ramp *flipA = &gHudTeam->flipA[gHudTeam->side];
    Ramp *flipB = &gHudTeam->flipB[gHudTeam->side];
    HudNode *group;
    f32 x;
    f32 y;

    if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
        Ramp_Step(slide);
        if (gHudTeam->side == 0) {
            Ramp_Step(&gHudTeam->hide);
        }
    }
    HudNode_SetPos(node, (s32)(slide->value * -256.0f) + (s32)(gHudTeam->hide.value * -256.0f) + 12,
                  (s32)(slide->value * -224.0f) + (s32)(gHudTeam->hide.value * -224.0f) + 66);
    if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
        if (Ramp_Step(flipA)) {
            Ramp_Stop(flipA);
        }
        if (Ramp_Step(flipB)) {
            Ramp_Stop(flipB);
            gHudTeam->flipDone[gHudTeam->side] = 1;
        } else {
            gHudTeam->flipDone[gHudTeam->side] = 0;
        }
    }
    if (0.0f < flipA->value) {
        x = -flipA->value;
        y = flipA->value;
    } else if (flipA->value < 0.0f) {
        y = flipA->value;
        x = flipA->value;
    } else {
        y = 0.0f;
        x = 0.0f;
    }
    x *= 128.0f;
    y *= 32.0f;
    HudNode_SetPos(&gHudTeam->nodes[1], (s32)x, (s32)y);
    if (0.0f < flipB->value) {
        x = -flipB->value;
        y = flipB->value;
    } else if (flipB->value < 0.0f) {
        y = flipB->value;
        x = flipB->value;
    } else {
        y = 0.0f;
        x = 0.0f;
    }
    group = &gHudTeam->nodes[2];
    x *= 128.0f;
    y *= 32.0f;
    HudNode_SetPos(group, (s32)x, (s32)y);
    if (gHudTeam->flipDone[gHudTeam->side]) {
        HudNode_Show(group, 0);
    } else {
        HudNode_Show(group, 1);
    }
}

/* Face A node: with two or more members in reserve the face sits 14 to the right and the plate shows. */
void HudTeam_UpdateFaceA(void) {
    if (gHudTeam->sw.reserve[gHudTeam->side] >= 2) {
        HudNode_SetPos(&gHudTeam->nodes[3], 14, 0);
        HudNode_Show(&gHudTeam->nodes[8], 1);
    } else {
        HudNode_SetPos(&gHudTeam->nodes[3], 0, 0);
        HudNode_Show(&gHudTeam->nodes[8], 0);
        gHudTeam->faceRes[gHudTeam->side] = NULL;
    }
}

/* Face B node: the same for the outgoing face. */
void HudTeam_UpdateFaceB(void) {
    if (gHudTeam->sw.reserve[gHudTeam->side] >= 2) {
        HudNode_SetPos(&gHudTeam->nodes[4], 14, 0);
        HudNode_Show(&gHudTeam->nodes[8], 1);
    } else {
        HudNode_SetPos(&gHudTeam->nodes[4], 0, 0);
        HudNode_Show(&gHudTeam->nodes[8], 0);
        gHudTeam->faceRes[gHudTeam->side] = NULL;
    }
}

/* Health bar A: the 32-wide bar is shifted right by 30 * health / maximum. */
void HudTeam_UpdateHpA(void) {
    HudSprite *spr = &gHudTeam->sprites[1];
    f32 rate = (f32)gHudTeam->hp.cur[gHudTeam->side] / (f32)gHudTeam->hp.max[gHudTeam->side];

    HudSprite_SetRect(spr, 0, 32, 0, 16);
    HudSprite_Move(spr, (s32)(rate * 30.0f), 0);
}

/* Health bar B: the same from the values saved when the flip started. */
void HudTeam_UpdateHpB(void) {
    HudSprite *spr = &gHudTeam->sprites[2];
    f32 rate = (f32)gHudTeam->prevHp.cur[gHudTeam->side] / (f32)gHudTeam->prevHp.max[gHudTeam->side];

    HudSprite_SetRect(spr, 0, 32, 0, 16);
    HudSprite_Move(spr, (s32)(rate * 30.0f), 0);
}

/* Draws health bar A through its mask. */
void HudTeam_DrawHpA(void) {
    HudSprite_DrawAlphaClear(&gHudTeam->sprites[3]);
    HudGfx_CallBegin(HudTeam_GsBeginMask);
    HudSprite_Draw(&gHudTeam->sprites[1], gHudTeam->res, 1);
    HudSprite_Draw(&gHudTeam->sprites[3], gHudTeam->res, 0);
    HudGfx_CallEnd(HudTeam_GsEndMask);
}

/* Draws health bar B through its mask. */
void HudTeam_DrawHpB(void) {
    HudSprite_DrawAlphaClear(&gHudTeam->sprites[4]);
    HudGfx_CallBegin(HudTeam_GsBeginMask);
    HudSprite_Draw(&gHudTeam->sprites[2], gHudTeam->res, 1);
    HudSprite_Draw(&gHudTeam->sprites[4], gHudTeam->res, 0);
    HudGfx_CallEnd(HudTeam_GsEndMask);
}

/* Switch gauge: bar shifted by 30 * gauge / 100000; at 100000 the "full" marker shows and its alpha runs
   0 -> 128 -> 0 over two seconds (not while paused). */
void HudTeam_UpdateGauge(void) {
    s32 full = HUD_TEAM_GAUGE_FULL;
    s32 gauge = gHudTeam->sw.gauge[gHudTeam->side];
    HudSprite *spr = &gHudTeam->sprites[5];
    Ramp *flash = &gHudTeam->flash[gHudTeam->side];
    f32 rate = (f32)gauge / (f32)full;

    HudSprite_SetRect(spr, 0, 32, 0, 16);
    HudSprite_Move(spr, (s32)(rate * 30.0f), 0);
    spr = &gHudTeam->sprites[7];
    if (gauge == full) {
        s32 done;

        HudSprite_Show(spr, 1);
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            done = Ramp_Step(flash);
        } else {
            done = 0;
        }
        if (done) {
            if (flash->value == 0.0f) {
                Ramp_Start(flash, 1.0f, 0.0f, 1.0f);
            } else {
                Ramp_Start(flash, 1.0f, 1.0f, 0.0f);
            }
        }
        HudSprite_SetColor(spr, 0x80, 0x80, 0x80, (u8)(flash->value * 128.0f));
    } else {
        HudSprite_Show(spr, 0);
        HudSprite_SetColor(spr, 0x80, 0x80, 0x80, 0x80);
    }
}

/* Draws the switch gauge through its mask, then the "full" marker. */
void HudTeam_DrawGauge(void) {
    HudSprite_DrawAlphaClear(&gHudTeam->sprites[6]);
    HudGfx_CallBegin(HudTeam_GsBeginMask);
    HudSprite_Draw(&gHudTeam->sprites[5], gHudTeam->res, 1);
    HudSprite_Draw(&gHudTeam->sprites[6], gHudTeam->res, 0);
    HudGfx_CallEnd(HudTeam_GsEndMask);
    HudSprite_Draw(&gHudTeam->sprites[7], gHudTeam->res, 0);
}

/* Generic draw callback: the node's sprites from the panel's sheet. */
void HudTeam_DrawSprites(HudNode *node) {
    u32 i;

    for (i = 0; i < node->spriteCount; i++) {
        HudSprite_Draw(node->sprites[i], gHudTeam->res, 0);
    }
}

/* Clears the mark of the first texture of a side's face sheet. */
void HudTeam_ClearFaceTexMark(s32 side) {
    if (gHudTeam->faceRes[side] != NULL) {
        HudTex *tex = gHudTeam->faceRes[side]->tex;

        if (tex != NULL) {
            tex->mark = 0;
        }
    }
}

/* Face A: the frame, then texture 0 of the stand-by member's face sheet (uploaded to blocks 0x2C00 / 0x2CD0).
   Declared with a return type and no return statement: the last call is a plain `jal`, not a tail call. */
s32 HudTeam_DrawFaceA(void) {
    HudSprite *spr;

    HudSprite_Draw(&gHudTeam->sprites[0], gHudTeam->res, 0);
    if (gHudTeam->target.cur[gHudTeam->side] >= 0) {
        gHudTeam->faceRes[gHudTeam->side] =
            BattleSide_GetMember(gHudTeam->side, gHudTeam->target.cur[gHudTeam->side])->faceRes;
        if (gHudTeam->faceRes[gHudTeam->side] != NULL) {
            HudTeam_ClearFaceTexMark(gHudTeam->side);
            spr = &gHudTeam->sprites[8];
            HudSprite_InitTex(spr, gHudTeam->faceRes[gHudTeam->side], 0, 0);
            HudSprite_Move(spr, -10, -11);
            HudSprite_DrawAt(spr, gHudTeam->faceRes[gHudTeam->side], 0, 0x2C00, 0x2CD0);
        }
    }
}

/* Face B: the same for the member shown before the flip. */
s32 HudTeam_DrawFaceB(void) {
    HudSprite *spr;

    HudSprite_Draw(&gHudTeam->sprites[0], gHudTeam->res, 0);
    if (gHudTeam->target.prev[gHudTeam->side] >= 0) {
        gHudTeam->faceRes[gHudTeam->side] =
            BattleSide_GetMember(gHudTeam->side, gHudTeam->target.prev[gHudTeam->side])->faceRes;
        if (gHudTeam->faceRes[gHudTeam->side] != NULL) {
            HudTeam_ClearFaceTexMark(gHudTeam->side);
            spr = &gHudTeam->sprites[9];
            HudSprite_InitTex(spr, gHudTeam->faceRes[gHudTeam->side], 0, 0);
            HudSprite_Move(spr, -10, -11);
            HudSprite_DrawAt(spr, gHudTeam->faceRes[gHudTeam->side], 0, 0x2C00, 0x2CD0);
        }
    }
}

/* Makes `side` the side shown by the next update / draw: side 1 is mirrored (with the faces mirrored back),
   and the panel is hidden for a side without reserve members. */
#ifdef PORT
/* Widescreen on PC: tells the renderer where the 2D pieces that follow belong (port/src/gs_marker.c). */
#define PORT_2D_LEFT 0x10
#define PORT_2D_RIGHT 0x11
#define PORT_2D_CENTER 0x12
#define PORT_2D_END 0x13
extern void Port_GsMarker(s32 effect);
#endif

void HudTeam_SelectSide(s32 side) {
    s32 mirror = HUD_SCR(side) != 0;

    gHudTeam->side = side;
#ifdef PORT
    Port_GsMarker(HUD_SCR(side) ? PORT_2D_RIGHT : PORT_2D_LEFT);
#endif
    gHudTeam->nodes[0].flags = (gHudTeam->nodes[0].flags & ~HUD_NODE_MIRROR) | (mirror << 1);
    HudSprite_SetMirror(&gHudTeam->sprites[8], mirror);
    HudSprite_SetMirror(&gHudTeam->sprites[9], mirror);
    HudNode_Show(&gHudTeam->nodes[0], gHudTeam->shown[side]);
}

/* Health of the member a switch would bring in. */
void HudTeam_SetTargetHp(s32 side, s32 hp) {
    gHudTeam->hp.cur[side] = hp;
}

/* Its maximum health. */
void HudTeam_SetTargetHpMax(s32 side, s32 hpMax) {
    gHudTeam->hp.max[side] = hpMax;
}

/* Switch gauge; on the frame it becomes full the marker's blink starts (1 -> 0 over a second). */
void HudTeam_SetSwitchGauge(s32 side, s32 gauge) {
    if (gHudTeam->sw.gauge[side] != HUD_TEAM_GAUGE_FULL && gauge == HUD_TEAM_GAUGE_FULL) {
        Ramp_Start(&gHudTeam->flash[side], 1.0f, 1.0f, 0.0f);
    }
    gHudTeam->sw.gauge[side] = gauge;
}

/* Member index of the member a switch would bring in. */
void HudTeam_SetTarget(s32 side, s32 member) {
    gHudTeam->target.cur[side] = member;
}

/* Number of members in reserve; the panel shows when it is not zero. */
void HudTeam_SetReserveCount(s32 side, s32 count) {
    gHudTeam->sw.reserve[side] = count;
    gHudTeam->shown[side] = count != 0;
}

/* Slides both panels off the screen. */
void HudTeam_SlideOut(f32 seconds) {
    Ramp_Start(&gHudTeam->hide, seconds, 0.0f, 1.0f);
}

/* Slides them back. */
void HudTeam_SlideIn(f32 seconds) {
    Ramp_Start(&gHudTeam->hide, seconds, 1.0f, 0.0f);
}

/* While a side is switching its panel slides away, and back afterwards, over 0.3 s. */
void HudTeam_SetSwitching(s32 side, s32 on) {
    if (gHudTeam->switching[side] != on) {
        if (on) {
            Ramp_Start(&gHudTeam->slide[side], 0.3f, 0.0f, 1.0f);
        } else {
            Ramp_Start(&gHudTeam->slide[side], 0.3f, 1.0f, 0.0f);
        }
    }
    gHudTeam->switching[side] = on;
}

/* Starts the face change animation (0.25 s): the new face comes in from one side while the old one, with the
   health it had, leaves to the other. */
void HudTeam_StartFlipNext(s32 side) {
    Ramp *flipA = &gHudTeam->flipA[side];
    Ramp *flipB = &gHudTeam->flipB[side];

    Ramp_Start(flipA, 0.25f, 1.0f, 0.0f);
    Ramp_Start(flipB, 0.25f, 0.0f, -1.0f);
    gHudTeam->target.prev[side] = gHudTeam->target.cur[side];
    gHudTeam->prevHp.cur[side] = gHudTeam->hp.cur[side];
    gHudTeam->prevHp.max[side] = gHudTeam->hp.max[side];
    gHudTeam->flipDone[side] = 0;
}

/* The same in the other direction. */
void HudTeam_StartFlipPrev(s32 side) {
    Ramp *flipA = &gHudTeam->flipA[side];
    Ramp *flipB = &gHudTeam->flipB[side];

    Ramp_Start(flipA, 0.25f, -1.0f, 0.0f);
    Ramp_Start(flipB, 0.25f, 0.0f, 1.0f);
    gHudTeam->target.prev[side] = gHudTeam->target.cur[side];
    gHudTeam->prevHp.cur[side] = gHudTeam->hp.cur[side];
    gHudTeam->prevHp.max[side] = gHudTeam->hp.max[side];
    gHudTeam->flipDone[side] = 0;
}

/* Builds the panel: work, 12 sprites, 9 nodes; *out receives the root node. */
void HudTeam_Init(HudNode **out, HudRes *res) {
    HudSprite *spr;
    HudNode *node;

    gHudTeam = Heap_Alloc(sizeof(HudTeam), 0x20, 0, 2);
    memset(gHudTeam, 0, sizeof(HudTeam));
    gHudTeam->sprites = Heap_Alloc(HUD_TEAM_SPR_COUNT * sizeof(HudSprite), 0x20, 0, 2);
    memset(gHudTeam->sprites, 0, HUD_TEAM_SPR_COUNT * sizeof(HudSprite));
    gHudTeam->nodes = Heap_Alloc(HUD_TEAM_NODE_COUNT * sizeof(HudNode), 0x20, 0, 2);
    memset(gHudTeam->nodes, 0, HUD_TEAM_NODE_COUNT * sizeof(HudNode));
    gHudTeam->res = res;

    spr = &gHudTeam->sprites[0];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x17, 0);
    HudSprite_SetRect(spr, 0, 0x5E, 0, 0x14);
    HudSprite_SetUv(spr, 0, 0x5E, 0, 0x14);

    spr = &gHudTeam->sprites[8];
    HudSprite_Show(spr, 1);
    HudSprite_InitPlain(spr, 0x80, 0x80, 0x80, 0x80);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x20);

    spr = &gHudTeam->sprites[9];
    HudSprite_Show(spr, 1);
    HudSprite_InitPlain(spr, 0x80, 0x80, 0x80, 0x80);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x20);

    spr = &gHudTeam->sprites[1];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x18, 0);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x10);
    HudSprite_SetUv(spr, 0, 0x20, 0x10, 0x20);

    spr = &gHudTeam->sprites[3];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x18, 0);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x10);
    HudSprite_SetUv(spr, 0, 0x20, 0, 0x10);

    spr = &gHudTeam->sprites[2];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x18, 0);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x10);
    HudSprite_SetUv(spr, 0, 0x20, 0x10, 0x20);

    spr = &gHudTeam->sprites[4];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x18, 0);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x10);
    HudSprite_SetUv(spr, 0, 0x20, 0, 0x10);

    spr = &gHudTeam->sprites[5];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x18, 0);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x10);
    HudSprite_SetUv(spr, 0, 0x20, 0x10, 0x20);

    spr = &gHudTeam->sprites[6];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x18, 0);
    HudSprite_SetRect(spr, 0, 0x20, 0, 0x10);
    HudSprite_SetUv(spr, 0x40, 0x60, 0, 0x10);

    spr = &gHudTeam->sprites[7];
    HudSprite_Show(spr, 0);
    HudSprite_InitTex(spr, res, 0x11, 0);
    HudSprite_SetRect(spr, 0, 0x30, 0, 0x10);
    HudSprite_SetUv(spr, 0, 0x30, 0x20, 0x30);
    HudSprite_Move(spr, -3, -3);

    spr = &gHudTeam->sprites[10];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x17, 0);
    HudSprite_SetRect(spr, 0, 0x1E, 0, 0x14);
    HudSprite_SetUv(spr, 0x60, 0x7E, 0, 0x14);

    spr = &gHudTeam->sprites[11];
    HudSprite_Show(spr, 1);
    HudSprite_InitTex(spr, res, 0x18, 0);
    HudSprite_SetRect(spr, 0, 0x10, 0, 0x10);
    HudSprite_SetUv(spr, 0x40, 0x50, 0x10, 0x20);
    HudSprite_Move(spr, -1, 3);

    node = &gHudTeam->nodes[5];
    node->x = 0x2E;
    node->y = 4;
    node->spriteCount = 2;
    node->sprites = Heap_Alloc(2 * sizeof(HudSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudSprite *));
    node->sprites[0] = &gHudTeam->sprites[1];
    node->sprites[1] = &gHudTeam->sprites[3];
    node->update = HudTeam_UpdateHpA;
    node->draw = (void (*)(HudNode *))HudTeam_DrawHpA;

    node = &gHudTeam->nodes[6];
    node->x = 0x2E;
    node->y = 4;
    node->spriteCount = 2;
    node->sprites = Heap_Alloc(2 * sizeof(HudSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudSprite *));
    node->sprites[0] = &gHudTeam->sprites[2];
    node->sprites[1] = &gHudTeam->sprites[4];
    node->update = HudTeam_UpdateHpB;
    node->draw = (void (*)(HudNode *))HudTeam_DrawHpB;

    node = &gHudTeam->nodes[7];
    node->x = 0x29;
    node->spriteCount = 3;
    node->y = 12;
    node->sprites = Heap_Alloc(3 * sizeof(HudSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudSprite *));
    node->sprites[0] = &gHudTeam->sprites[5];
    node->sprites[1] = &gHudTeam->sprites[6];
    node->sprites[2] = &gHudTeam->sprites[7];
    node->update = HudTeam_UpdateGauge;
    node->draw = (void (*)(HudNode *))HudTeam_DrawGauge;

    node = &gHudTeam->nodes[8];
    node->spriteCount = 2;
    node->x = 0;
    node->y = 0;
    node->sprites = Heap_Alloc(2 * sizeof(HudSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudSprite *));
    node->sprites[0] = &gHudTeam->sprites[10];
    node->sprites[1] = &gHudTeam->sprites[11];
    node->draw = HudTeam_DrawSprites;
    node->update = NULL;

    node = &gHudTeam->nodes[3];
    node->x = 14;
    node->spriteCount = 2;
    node->y = 0;
    node->sprites = Heap_Alloc(2 * sizeof(HudSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudSprite *));
    node->sprites[0] = &gHudTeam->sprites[0];
    node->sprites[1] = &gHudTeam->sprites[8];
    node->childCount = 2;
    node->children = Heap_Alloc(2 * sizeof(HudNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudNode *));
    node->children[0] = &gHudTeam->nodes[5];
    node->children[1] = &gHudTeam->nodes[7];
    node->update = HudTeam_UpdateFaceA;
    node->draw = (void (*)(HudNode *))HudTeam_DrawFaceA;

    node = &gHudTeam->nodes[4];
    node->x = 14;
    node->spriteCount = 2;
    node->y = 0;
    node->sprites = Heap_Alloc(2 * sizeof(HudSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudSprite *));
    node->sprites[0] = &gHudTeam->sprites[0];
    node->sprites[1] = &gHudTeam->sprites[9];
    node->childCount = 2;
    node->children = Heap_Alloc(2 * sizeof(HudNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudNode *));
    node->children[0] = &gHudTeam->nodes[6];
    node->children[1] = &gHudTeam->nodes[7];
    node->update = HudTeam_UpdateFaceB;
    node->draw = (void (*)(HudNode *))HudTeam_DrawFaceB;

    node = &gHudTeam->nodes[1];
    node->childCount = 2;
    node->x = 0;
    node->y = 0;
    node->children = Heap_Alloc(2 * sizeof(HudNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudNode *));
    node->children[0] = &gHudTeam->nodes[3];
    node->children[1] = &gHudTeam->nodes[8];
    node->draw = HudTeam_DrawSprites;
    node->update = NULL;

    node = &gHudTeam->nodes[2];
    node->childCount = 2;
    node->x = 0;
    node->y = 0;
    node->children = Heap_Alloc(2 * sizeof(HudNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudNode *));
    node->children[0] = &gHudTeam->nodes[4];
    node->children[1] = &gHudTeam->nodes[8];
    node->draw = HudTeam_DrawSprites;
    node->update = NULL;

    node = &gHudTeam->nodes[0];
    node->y = 66;
    node->x = 12;
    node->childCount = 2;
    node->children = Heap_Alloc(2 * sizeof(HudNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudNode *));
    node->children[0] = &gHudTeam->nodes[1];
    node->children[1] = &gHudTeam->nodes[2];
    node->draw = HudTeam_DrawSprites;
    node->update = HudTeam_UpdateRoot;

    *out = gHudTeam->nodes;
}

/* Frees the sprites, every node's lists, the nodes and the work. */
void HudTeam_Term(void) {
    s32 i;

    if (gHudTeam->sprites != NULL) {
        Heap_Free(gHudTeam->sprites);
    }
    for (i = 0; i < HUD_TEAM_NODE_COUNT; i++) {
        if (gHudTeam->nodes[i].sprites != NULL) {
            Heap_Free(gHudTeam->nodes[i].sprites);
        }
        if (gHudTeam->nodes[i].children != NULL) {
            Heap_Free(gHudTeam->nodes[i].children);
        }
    }
    if (gHudTeam->nodes != NULL) {
        Heap_Free(gHudTeam->nodes);
    }
    if (gHudTeam != NULL) {
        Heap_Free(gHudTeam);
    }
}

/* Round reset: zeroes the values and ramps of both sides. */
void HudTeam_Reset(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gHudTeam->hp.cur[i] = 0;
        gHudTeam->hp.max[i] = 0;
        gHudTeam->sw.gauge[i] = 0;
        gHudTeam->sw.reserve[i] = 0;
        gHudTeam->shown[i] = 0;
        gHudTeam->switching[i] = 0;
        gHudTeam->flipDone[i] = 0;
        gHudTeam->target.cur[i] = 0;
        gHudTeam->target.prev[i] = 0;
        gHudTeam->prevHp.cur[i] = 0;
        gHudTeam->prevHp.max[i] = 0;
        memset(&gHudTeam->flash[i], 0, sizeof(Ramp));
        memset(&gHudTeam->slide[i], 0, sizeof(Ramp));
        memset(&gHudTeam->flipA[i], 0, sizeof(Ramp));
        memset(&gHudTeam->flipB[i], 0, sizeof(Ramp));
    }
    memset(&gHudTeam->hide, 0, sizeof(Ramp));
}
