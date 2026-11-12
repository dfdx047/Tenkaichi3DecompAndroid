#include "common.h"
#include "battle/hud_c.h"
#include "sys/heap.h"

/*
 * Battle HUD: the end of the gauge part, 0x222400-0x222730. These two functions belong to the module of
 * src/battle/hud_b.c (work gHudGauge, built by HudGauge_Init at 0x21FC10): Hud_Term calls HudGauge_Term and
 * Hud_Reset calls HudGauge_Reset. The struct is this file's view of the gauge work; names follow HudBWork of hud_b.c.
 */

extern void *memset(void *dst, s32 c, u32 n);

#define HUDC_GAUGE_NODE_COUNT 12

/* Shake of one node (HudBShake of battle/hud_b.h). */
typedef struct HudCShake {
    /* 0x00 */ u16 count;
    /* 0x02 */ u16 amp;
} HudCShake;

/* Gauge work (0x288 bytes). */
typedef struct HudCGauge {
    /* 0x000 */ void *res;
    /* 0x004 */ HudCSprite *sprites;
    /* 0x008 */ HudCNode *nodes;       /* HUDC_GAUGE_NODE_COUNT */
    /* 0x00C */ HudCShake shake[2][3];
    /* 0x024 */ s32 side;
    /* 0x028 */ s32 hpShown[2];
    /* 0x030 */ s32 hp[2];
    /* 0x038 */ s32 auraAlpha[2];
    /* 0x040 */ Ramp lowHpPulse[2];
    /* 0x070 */ s32 blast[2];
    /* 0x078 */ s32 ki[2];
    /* 0x080 */ s32 kiShown[2];
    /* 0x088 */ Ramp kiBarFlash[2];
    /* 0x0B8 */ Ramp kiFullPulse[2];
    /* 0x0E8 */ s32 maxPower[2];
    /* 0x0F0 */ Ramp blastFlash[2];
    /* 0x120 */ u32 statMask[2];
    /* 0x128 */ u32 statPos[2];
    /* 0x130 */ s32 kiReserve[2];
    /* 0x138 */ u32 statPrev[2];
    /* 0x140 */ s32 statTo[2][4];
    /* 0x160 */ s32 statFrom[2][4];
    /* 0x180 */ Ramp statRamp[2];
    /* 0x1B0 */ s32 statState[2];
    /* 0x1B8 */ Ramp hpBarFade[2];
    /* 0x1E8 */ void *faceRes[2];
    /* 0x1F0 */ void *obj[2];
    /* 0x1F8 */ s32 switching[2];
    /* 0x200 */ Ramp slide[2];
    /* 0x230 */ Ramp slideAll;
    /* 0x248 */ s32 hpDrain[2];
    /* 0x250 */ s32 kiDrain[2];
    /* 0x258 */ Ramp kiReservePulse[2];
} HudCGauge; /* size 0x288 */

extern HudCGauge *gHudGauge;

/* Battle end: frees the sprites, every node's sprite and child lists, the nodes and the work. */
void HudGauge_Term(void) {
    s32 i;

    if (gHudGauge->sprites != NULL) {
        Heap_Free(gHudGauge->sprites);
    }
    for (i = 0; i < HUDC_GAUGE_NODE_COUNT; i++) {
        if (gHudGauge->nodes[i].sprites != NULL) {
            Heap_Free(gHudGauge->nodes[i].sprites);
        }
        if (gHudGauge->nodes[i].children != NULL) {
            Heap_Free(gHudGauge->nodes[i].children);
        }
    }
    if (gHudGauge->nodes != NULL) {
        Heap_Free(gHudGauge->nodes);
    }
    if (gHudGauge != NULL) {
        Heap_Free(gHudGauge);
    }
}

/* Round reset: zeroes the values, ramps and shakes of both sides and the whole-part slide. The status icon
   animation is put at rest (state -1). hpDrain, kiDrain, statPos and kiReservePulse are left as they are. */
void HudGauge_Reset(void) {
    s32 i;
    s32 j;

    for (i = 0; i < 2; i++) {
        gHudGauge->hpShown[i] = 0;
        gHudGauge->hp[i] = 0;
        gHudGauge->auraAlpha[i] = 0;
        gHudGauge->blast[i] = 0;
        gHudGauge->ki[i] = 0;
        gHudGauge->kiShown[i] = 0;
        gHudGauge->maxPower[i] = 0;
        gHudGauge->statMask[i] = 0;
        gHudGauge->statPrev[i] = 0;
        gHudGauge->statState[i] = -1;
        memset(gHudGauge->statTo[i], 0, sizeof(gHudGauge->statTo[i]));
        memset(gHudGauge->statFrom[i], 0, sizeof(gHudGauge->statFrom[i]));
        memset(&gHudGauge->statRamp[i], 0, sizeof(Ramp));
        gHudGauge->kiReserve[i] = 0;
        memset(&gHudGauge->kiBarFlash[i], 0, sizeof(Ramp));
        memset(&gHudGauge->kiFullPulse[i], 0, sizeof(Ramp));
        memset(&gHudGauge->blastFlash[i], 0, sizeof(Ramp));
        memset(&gHudGauge->lowHpPulse[i], 0, sizeof(Ramp));
        memset(&gHudGauge->slide[i], 0, sizeof(Ramp));
        memset(&gHudGauge->hpBarFade[i], 0, sizeof(Ramp));
        gHudGauge->faceRes[i] = NULL;
        gHudGauge->obj[i] = NULL;
        gHudGauge->switching[i] = 0;
        memset(&gHudGauge->slide[i], 0, sizeof(Ramp));
        for (j = 0; j < 3; j++) {
            memset(&gHudGauge->shake[i][j], 0, sizeof(HudCShake));
        }
    }
    memset(&gHudGauge->slideAll, 0, sizeof(Ramp));
}
