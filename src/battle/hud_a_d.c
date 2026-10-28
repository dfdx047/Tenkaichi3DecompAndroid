#include "common.h"
#include "battle/hud_a.h"
#include "sys/dma.h"
#include "sys/gfx.h"

/*
 * Head of the HUD health / ki gauge part. Source range 0x21C0E0-0x21CA60 (the module goes on behind it: its
 * setters are at 0x21F648..0x21FB40, its init at 0x21FC10).
 */

extern void HudSprite_Show(HudSprite *spr, s32 show);
extern void HudSprite_SetTex(HudSprite *spr, s32 tex, s32 sub);
extern void HudSprite_Move(HudSprite *spr, s32 dx, s32 dy);

/* The gauge work (full layout: HudBWork in src/battle/hud_b.c). Defined here: this object's .sdata (0x2FEB48). */
HudGauge *gHudGauge = NULL;

/* GS state before a mask is drawn (the same packet as HudTeam_GsBeginMask). */
void HudGauge_GsBeginMask(void) {
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

/* GS state after the masked sprite (the same packet as HudTeam_GsEndMask). */
void HudGauge_GsEndMask(void) {
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

/* A second mask state: ALPHA_2 = 0x48, TEST_2 = 0x3C000, TEST_1 = 0x307FB, FRAME_1 writing only alpha, FBA on
   in both contexts. */
void HudGauge_GsBeginMask2(void) {
    u32 pkt[32] = {
        DMA_TAG_CNT | 7, 0, VIF_FLUSHE, VIF_DIRECT | 7,
        GIF_EOP | 6, 0x10000000, GIF_REG_AD, 0,
        0x48, 0, GS_ALPHA_2, 0,
        0x3C000, 0, GS_TEST_2, 0,
        0x307FB, 0, GS_TEST_1, 0,
        GFX_FRAME_REG(), 0xFFFFFF, GS_FRAME_1, 0,
        1, 0, GS_FBA_1, 0,
        1, 0, GS_FBA_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Leaves it: both FRAME registers without mask, ALPHA_2 = 0x44, FBA off, TEST_2 = 0x30000. */
void HudGauge_GsEndMask2(void) {
    u32 pkt[32] = {
        DMA_TAG_CNT | 7, 0, VIF_FLUSHE, VIF_DIRECT | 7,
        GIF_EOP | 6, 0x10000000, GIF_REG_AD, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
        0x44, 0, GS_ALPHA_2, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_1, 0,
        0, 0, GS_FBA_1, 0,
        0, 0, GS_FBA_2, 0,
        0x30000, 0, GS_TEST_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Health bars of the side being shown. Health is drawn in bars of 10000, each 160 pixels wide; a bar that is
   exactly full counts as the top bar (10000 of it) rather than as an empty one above it. */
void HudGauge_UpdateHp(void) {
    HudSprite *bar;
    HudSprite *tip;
    s32 side = gHudGauge->side;
    s32 rem;
    s32 bars;
    s32 rem2;
    s32 bars2;
    s32 w;

    rem = gHudGauge->hpShown[side] % HUD_GAUGE_BAR;
    bars = gHudGauge->hpShown[side] / HUD_GAUGE_BAR;
    if (rem == 0 && bars != 0) {
        rem = HUD_GAUGE_BAR;
        bars--;
    }
    if (gHudGauge->hpShown[side] != gHudGauge->cur.hp[side]) {
        tip = &gHudGauge->sprites[6];
        bar = &gHudGauge->sprites[5];
        bar->pos = tip->pos;
        w = rem * 160 / HUD_GAUGE_BAR;
        if (w < 3 && rem != 0 && gHudGauge->cur.hp[side] != 0 && bars == 0) {
            HudSprite_Move(bar, 3, 0);
        } else {
            HudSprite_Move(bar, w, 0);
        }
        HudSprite_Show(tip, 1);
        HudSprite_Show(bar, 1);
    } else {
        tip = &gHudGauge->sprites[5];
        bar = &gHudGauge->sprites[6];
        HudSprite_Show(tip, 0);
        HudSprite_Show(bar, 0);
    }

    rem2 = gHudGauge->cur.hp[side] % HUD_GAUGE_BAR;
    bars2 = gHudGauge->cur.hp[side] / HUD_GAUGE_BAR;
    if (rem2 == 0 && bars2 != 0) {
        rem2 = HUD_GAUGE_BAR;
        bars2--;
    }
    tip = &gHudGauge->sprites[8];
    bar = &gHudGauge->sprites[7];
    bar->pos = tip->pos;
    w = rem2 * 160 / HUD_GAUGE_BAR;
    if (bars2 < bars) {
        w = 0;
    }
    if (w < 3 && rem2 != 0 && bars == 0) {
        HudSprite_Move(bar, 3, 0);
    } else {
        HudSprite_Move(bar, w, 0);
    }
    HudSprite_Show(tip, 1);
    HudSprite_Show(bar, 1);
    if (bars >=
#ifdef PORT
        (PortGame_IsBT4() ? 4 : 7)
#else
        7
#endif
    ) {
        HudSprite_SetTex(tip, 8, 5);
    } else if (bars >= 2) {
        HudSprite_SetTex(tip, 8, 1);
    } else if (bars > 0) {
        HudSprite_SetTex(tip, 8, 4);
    } else {
        HudSprite_SetTex(tip, 8, 3);
    }
    HudSprite_Show(&gHudGauge->sprites[4], 1);
    if (bars == bars2) {
        HudSprite_Show(&gHudGauge->sprites[1], 0);
        HudSprite_Show(&gHudGauge->sprites[2], 0);
        HudSprite_Show(&gHudGauge->sprites[3], 1);
    } else {
        HudSprite_Show(&gHudGauge->sprites[1], 1);
        HudSprite_Show(&gHudGauge->sprites[2], 1);
        HudSprite_Show(&gHudGauge->sprites[3], 1);
        bar = &gHudGauge->sprites[2];
        bar->pos = gHudGauge->sprites[3].pos;
        if (bars == bars2 + 1) {
            HudSprite_Move(bar, rem2 * 160 / HUD_GAUGE_BAR, 0);
        }
    }
    bar = &gHudGauge->sprites[3];
    if (bars - 1 >=
#ifdef PORT
        (PortGame_IsBT4() ? 4 : 7)
#else
        7
#endif
    ) {
        HudSprite_SetTex(bar, 8, 5);
    } else if (bars - 1 >= 2) {
        HudSprite_SetTex(bar, 8, 1);
    } else if (bars - 1 > 0) {
        HudSprite_SetTex(bar, 8, 4);
    } else if (bars - 1 >= 0) {
        HudSprite_SetTex(bar, 8, 3);
    } else {
        HudSprite_Show(bar, 0);
        HudSprite_Show(&gHudGauge->sprites[4], 0);
    }
}
