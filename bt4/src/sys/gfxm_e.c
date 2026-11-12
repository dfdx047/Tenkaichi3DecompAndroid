#include "common.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/gfxm_e.h"

/*
 * GS environment packets of the battle object renderer, 0x112A30..0x1131D8. See include/sys/gfxm_e.h.
 * Each function builds a packet on the stack (DMA tag, VIF DIRECT, GIF tag, A+D registers) and queues a copy
 * with Dma_AddData. Callers: the object draw at 0x111358 and the pass function at 0x10FD98 (previous file) and
 * the shadow code of gfxm_e_b.c.
 */

#define GS_TEX0_1_REG 0x06
#define GS_TEXA 0x3B
#define GS_PRMODECONT 0x1A
#define GS_PRMODE 0x1B

/* FRAME_1 value of the frame's colour buffer with a write mask. */
#define OBJGS_FRAME(mask) GS_SET_FRAME((gGfx.frame & 1) ? 0 : 0x70, 8, 0, mask)

/* Queues FRAME_1 with the mask (alpha is always protected), ALPHA_1 = normal blend, TEST_1 = depth only. */
void ObjGs_AddFrameMaskOpaque(u32 mask) {
    u32 pkt[20] = {
        DMA_TAG_CNT | 4, 0, VIF_FLUSHE, VIF_DIRECT | 4,
        GIF_EOP | 3, 0x10000000, GIF_REG_AD, 0,
        OBJGS_FRAME(mask | 0xFF000000), OBJGS_FRAME(mask | 0xFF000000) >> 32, GS_FRAME_1, 0,
        0x44, 0, GS_ALPHA_1, 0,
        0x50000, 0, GS_TEST_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues FRAME_1 with the mask, ALPHA_1 = (Cs - Cd) * 0x80 / 128 + Cd with a fixed factor, TEST_1 = depth only. */
void ObjGs_AddFrameMaskBlend(u32 mask) {
    u32 pkt[20] = {
        DMA_TAG_CNT | 4, 0, VIF_FLUSHE, VIF_DIRECT | 4,
        GIF_EOP | 3, 0x10000000, GIF_REG_AD, 0,
        OBJGS_FRAME(mask), OBJGS_FRAME(mask) >> 32, GS_FRAME_1, 0,
        0x64, 0x80, GS_ALPHA_1, 0,
        0x50000, 0, GS_TEST_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues the state of the flat shadow pass: depth writes off, alpha test, FBA on, blend by a fixed 0x46,
   PRMODECONT = 0 with PRMODE = gouraud + blend. */
void ObjGs_AddFlatShadowEnv(void) {
    u32 pkt[32] = {
        DMA_TAG_CNT | 7, 0, VIF_FLUSHE, VIF_DIRECT | 7,
        GIF_EOP | 6, 0x10000000, GIF_REG_AD, 0,
        0x310000E0, 1, GS_ZBUF_1, 0,
        0x54000, 0, GS_TEST_1, 0,
        1, 0, GS_FBA_1, 0,
        0x64, 0x46, GS_ALPHA_1, 0,
        0, 0, GS_PRMODECONT, 0,
        0x48, 0, GS_PRMODE, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* TEX0: PSMCT24, 256x256, RGBA, modulate. */
#define OBJGS_SHADOW_TEX0(tbp, tbw) ((u64)(tbp) | ((u64)(tbw) << 14) | ((u64)0xC402 << 19))

/* Queues the state for drawing the shadow texture on the ground: the frame's colour buffer, depth test without
   depth writes, alpha test, FBA on, clamped PSMCT24 256x256 texture at `tbp`, point sampled. */
void ObjGs_AddShadowTexEnv(s32 tbp, s32 tbw) {
    u32 pkt[48] = {
        DMA_TAG_CNT | 11, 0, VIF_FLUSHE, VIF_DIRECT | 11,
        GIF_EOP | 10, 0x10000000, GIF_REG_AD, 0,
        !(gGfx.frame & 1) ? 0x80070 : 0x80000, 0, GS_FRAME_1, 0,
        0x310000E0, 1, GS_ZBUF_1, 0,
        0x44, 0x80, GS_ALPHA_1, 0,
        0x5400F, 0, GS_TEST_1, 0,
        0, 0, GS_TEX1_1, 0,
        1, 0, GS_FBA_1, 0,
        5, 0, GS_CLAMP_1, 0,
        1, 0, GS_COLCLAMP, 0,
        0x8080, 0x80, GS_TEXA, 0,
        OBJGS_SHADOW_TEX0(tbp, tbw), OBJGS_SHADOW_TEX0(tbp, tbw) >> 32, GS_TEX0_1_REG, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Queues the renderer's normal state: the frame's colour buffer, depth test and writes, normal blend, bilinear,
   FBA off, repeat, grey RGBAQ, PRMODECONT = 1 and the screen's XYOFFSET. */
void ObjGs_AddDefaultEnv(void) {
    u32 pkt[52] = {
        DMA_TAG_CNT | 12, 0, VIF_FLUSHE, VIF_DIRECT | 12,
        GIF_EOP | 11, 0x10000000, GIF_REG_AD, 0,
        !(gGfx.frame & 1) ? 0x80070 : 0x80000, 0, GS_FRAME_1, 0,
        0x310000E0, 0, GS_ZBUF_1, 0,
        0x44, 0, GS_ALPHA_1, 0,
        0x50000, 0, GS_TEST_1, 0,
        0x60, 0, GS_TEX1_1, 0,
        0, 0, GS_FBA_1, 0,
        0, 0, GS_CLAMP_1, 0,
        1, 0, GS_COLCLAMP, 0,
        0x80808080, 0x3F800000, GS_RGBAQ, 0,
        1, 0, GS_PRMODECONT, 0,
        0x7000, 0x7200, GS_XYOFFSET_1, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}
