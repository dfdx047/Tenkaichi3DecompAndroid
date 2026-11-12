#include "common.h"
#include "battle/hud_d.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/mathf.h"
#include "sys/randf.h"

/*
 * Battle HUD, notice part: the animations of the full-screen announcements, 0x226500-0x22A750.
 * The part's init, term, reset and its starter HudNotice_Show(id) (0x22AB50) follow at 0x22A750 (src/battle/hud_e.c);
 * the starter hides the sprites, clears `state`, puts node 1 at (256, 224) and gives it one of the update / draw
 * pairs below, then plays the announcer's line. Ids: 0 UpdateReady, 1 UpdateFight, 2 UpdateKo, 3 UpdateBandA,
 * 4 UpdateSpinA, 5 UpdateSpinB (both drawn by DrawSpin), 6 UpdateTrail, 7 the banner of hud_e.c, 8 UpdateBandB.
 * Each update is a state machine on gHudNotice->state; its last state hides everything and stays.
 * Nothing here looks at the pause flag, the pad, the clock or the camera: an announcement keeps animating while
 * the battle is paused. The only random draws are the streak's shake (Rand_IntRange = libc rand(), one per frame
 * in one state of UpdateFight, UpdateKo and UpdateTrail).
 */

typedef f32 HudDMtx[4][4];

/* Offset of the streak: fractions of its size (float), or pixels (the shake). */
typedef union HudNoticeVec {
    f32 f[2];
    s32 i[2];
} HudNoticeVec;

/* Index of trail[0] counted in floats from offset 4 of the work (see HudNotice_UpdateKo). */
#define HUDN_TRAIL_AT4 16

#define HUDN_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((hi) < (v) ? (hi) : (v)))
#define HUDN_CLAMP01(v) HUDN_CLAMP(v, 0.0f, 1.0f)

/* Sprite library (0x224B50..0x226488, src/battle/hud_c_c.c; names from config/symbols/hud_c.txt). */
extern void HudGfx_CallBegin(void (*fn)(void)); /* 0x224B50: calls a GS state function before masked sprites */
extern void HudGfx_CallEnd(void (*fn)(void));   /* 0x224B70: and after them */
extern void HudSprite_Show(HudDSprite *spr, s32 show);
extern void HudSprite_SetRect(HudDSprite *spr, s32 x0, s32 x1, s32 y0, s32 y1); /* screen rectangle */
extern void HudSprite_SetUv(HudDSprite *spr, s32 u0, s32 u1, s32 v0, s32 v1); /* texel rectangle */
extern void HudSprite_SetColor(HudDSprite *spr, s32 r, s32 g, s32 b, s32 a);
extern void HudSprite_InitPlain(HudDSprite *spr, s32 r, s32 g, s32 b, s32 a);     /* untextured, coloured */
extern void HudSprite_Move(HudDSprite *spr, s32 dx, s32 dy);                 /* moves the rectangle */
extern void HudSprite_Scale(HudDSprite *spr, f32 sx, f32 sy);                 /* scales the rectangle about its centre */
extern void HudSprite_Center(HudDSprite *spr);                                 /* centres the rectangle on the node */
extern void HudSprite_InitTex(HudDSprite *spr, void *res, s32 tex, s32 sub);    /* sprite of a sheet's texture */
extern void HudGfx_ClearAlpha(void);
extern void *memset(void *dst, s32 c, s32 n); /* not the built-in: the original calls it for 8 bytes */
extern f32 powf(f32 x, f32 y);
extern void Mtx_StoreIdentity(HudDMtx m);
extern void Mtx_RotateZ(HudDMtx dst, HudDMtx src, f32 angle);
extern void Vu0Cur_Push(void);
extern void Vu0Cur_Pop(void);
extern void Vu0Cur_MulMtx(HudDMtx m);

/* GS state for the glowing word: TEST_1 = 0x3301B, FRAME_2 with the alpha plane write-protected, ALPHA_2 = 0x58
   (source added to the frame buffer). */
void HudNotice_GsBeginAdd(void) {
    u32 pkt[20] = {
        DMA_TAG_CNT | 4, 0, VIF_FLUSHE, VIF_DIRECT | 4,
        GIF_EOP | 3, 0x10000000, GIF_REG_AD, 0,
        0x3301B, 0, GS_TEST_1, 0,
        GFX_FRAME_REG(), 0xFF000000, GS_FRAME_2, 0,
        0x58, 0, GS_ALPHA_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Leaves it: TEST_1 = 0x30000, FRAME_2 without mask, ALPHA_2 = 0x44. */
void HudNotice_GsEndAdd(void) {
    u32 pkt[20] = {
        DMA_TAG_CNT | 4, 0, VIF_FLUSHE, VIF_DIRECT | 4,
        GIF_EOP | 3, 0x10000000, GIF_REG_AD, 0,
        0x30000, 0, GS_TEST_1, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
        0x44, 0, GS_ALPHA_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* The same as HudNotice_GsBeginAdd, and FRAME_1 writes only alpha. */
void HudNotice_GsBeginAddAlpha(void) {
    u32 pkt[24] = {
        DMA_TAG_CNT | 5, 0, VIF_FLUSHE, VIF_DIRECT | 5,
        GIF_EOP | 4, 0x10000000, GIF_REG_AD, 0,
        0x3301B, 0, GS_TEST_1, 0,
        GFX_FRAME_REG(), 0xFFFFFF, GS_FRAME_1, 0,
        GFX_FRAME_REG(), 0xFF000000, GS_FRAME_2, 0,
        0x58, 0, GS_ALPHA_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Leaves it. */
void HudNotice_GsEndAddAlpha(void) {
    u32 pkt[24] = {
        DMA_TAG_CNT | 5, 0, VIF_FLUSHE, VIF_DIRECT | 5,
        GIF_EOP | 4, 0x10000000, GIF_REG_AD, 0,
        0x30000, 0, GS_TEST_1, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_1, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
        0x44, 0, GS_ALPHA_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* GS state for drawing a mask into the alpha plane: FBA_1 on, FRAME_1 writes only alpha, TEST_2 = 0x3C000
   (destination alpha test), FRAME_2 without mask. */
void HudNotice_GsBeginMask(void) {
    u32 pkt[24] = {
        DMA_TAG_CNT | 5, 0, VIF_FLUSHE, VIF_DIRECT | 5,
        GIF_EOP | 4, 0x10000000, GIF_REG_AD, 0,
        1, 0, GS_FBA_1, 0,
        GFX_FRAME_REG(), 0xFFFFFF, GS_FRAME_1, 0,
        0x3C000, 0, GS_TEST_2, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Leaves it: FBA_1 off, FRAME_1 without mask, TEST_2 = 0x30000. */
void HudNotice_GsEndMask(void) {
    u32 pkt[24] = {
        DMA_TAG_CNT | 5, 0, VIF_FLUSHE, VIF_DIRECT | 5,
        GIF_EOP | 4, 0x10000000, GIF_REG_AD, 0,
        0, 0, GS_FBA_1, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_1, 0,
        0x30000, 0, GS_TEST_2, 0,
        GFX_FRAME_REG(), 0, GS_FRAME_2, 0,
    };

    Dma_AddData(pkt, sizeof(pkt));
}

/* Draw callback: all sprites of the node, plain. */
void HudNotice_DrawNode(HudDNode *node) {
    u32 i;

    for (i = 0; i < node->sprCount; i++) {
        HudSprite_Draw(node->sprList[i], gHudNotice->res, 0);
    }
}

/* Draw callback of announcements 4 and 5: sprites 0..13 under the additive state, the streak with the other
   blend. */
void HudNotice_DrawSpin(void) {
    s32 i;

    HudGfx_CallBegin(HudNotice_GsBeginAdd);
    for (i = 0; i < 14; i++) {
        HudDSprite *spr = &gHudNotice->spr[i];

        if (i == HUD_NOTICE_SPR_STREAK) {
            HudSprite_Draw(spr, gHudNotice->res, 1);
        } else {
            HudSprite_Draw(spr, gHudNotice->res, 0);
        }
    }
    HudGfx_CallEnd(HudNotice_GsEndAdd);
}

/* Draw callback of announcement 0: the band (rotated by node 2's angle) is the mask of the word; the twelve
   fragments are drawn plain on top. */
void HudNotice_DrawReady(void) {
    HudDMtx m;
    HudDNode *node;
    s32 i;

    HudGfx_ClearAlpha();
    HudGfx_CallBegin(HudNotice_GsBeginMask);
    Vu0Cur_Push();
    node = &gHudNotice->node[2];
    Mtx_StoreIdentity(m);
    Mtx_RotateZ(m, m, node->rot);
    Vu0Cur_MulMtx(m);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_BAND], gHudNotice->res, 0);
    Vu0Cur_Pop();
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndMask);
    for (i = 0; i < 12; i++) {
        HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_PART + i], gHudNotice->res, 0);
    }
}

/* Update of announcement 0. The band grows from a 32 x 4 line to 512 wide (0.2 s), then to 128 high while it
   turns half a turn backwards (0.36 s); after 1.1 s the word and the band swell and fade (0.43 s, cubic) while
   the twelve fragments fly outwards and fade one after another. */
void HudNotice_UpdateReady(void) {
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    HudDSprite *band = &gHudNotice->spr[HUD_NOTICE_SPR_BAND];
    HudDNode *node = &gHudNotice->node[2];
    HudDSprite *part = &gHudNotice->spr[HUD_NOTICE_SPR_PART];
    s16 ofs[12][2] = {
        { 40, -70 }, { 190, -40 }, { 190, 40 }, { 90, 90 }, { -109, -61 }, { -160, -20 },
        { -210, 10 }, { -170, 60 }, { 30, -60 }, { 160, -30 }, { -150, 30 }, { -100, 60 },
    };
    s32 done;
    s32 i;
    f32 t;
    f32 v;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 0, 0);
        HudSprite_Center(word);
        HudSprite_Show(band, 1);
        HudSprite_SetRect(band, 0, 32, 0, 4);
        HudSprite_Center(band);
        HudNode_SetRot(node, 0.0f);
        Ramp_Start(&gHudNotice->rampA, 0.2f, 0.0f, 1.0f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        done = Ramp_Step(&gHudNotice->rampA);
        v = gHudNotice->rampA.value;
        v *= 480.0f;
        v += 32.0f;
        HudSprite_Show(band, 1);
        HudSprite_SetRect(band, 0, (s32)v, 0, 4);
        HudSprite_Center(band);
        if (done) {
            Ramp_Start(&gHudNotice->rampA, 0.36f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 2:
        done = Ramp_Step(&gHudNotice->rampA);
        v = gHudNotice->rampA.value;
        v *= 124.0f;
        v += 4.0f;
        HudSprite_Show(band, 1);
        HudSprite_SetRect(band, 0, 512, 0, (s32)v);
        HudSprite_Center(band);
        HudNode_SetRot(node, -gHudNotice->rampA.value * 3.14159265f);
        if (done) {
            Ramp_Start(&gHudNotice->rampB, 1.1f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.43f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 4:
        done = Ramp_Step(&gHudNotice->rampB);
        t = powf(gHudNotice->rampB.value, 3.0f);
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 0, 0);
        HudSprite_Center(word);
        HudSprite_Scale(word, (1.0f - t) * 0.5f + 1.0f, (1.0f - t) * 0.5f + 1.0f);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)(t * 128.0f));
        HudSprite_Show(band, 1);
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, 128);
        HudSprite_Center(band);
        HudSprite_Scale(band, (1.0f - t) * 0.5f + 1.0f, (1.0f - t) * 0.5f + 1.0f);
        for (i = 0; i < 12; i++) {
            f32 a;


            HudSprite_Show(&part[i], 1);
            HudSprite_InitTex(&part[i], gHudNotice->res, 0, 0);
            HudSprite_Center(&part[i]);
            HudSprite_Move(&part[i], (s32)(ofs[i][0] * (1.0f - t)), (s32)(ofs[i][1] * (1.0f - t)));
            if (i < 6) {
                a = gHudNotice->rampB.value - i / 30.0f;
            } else {
                a = gHudNotice->rampB.value - (11 - i) / 30.0f;
            }
            a = powf(HUDN_CLAMP01(a), 3.0f);
            a *= 64.0f;
            HudSprite_SetColor(&part[i], 0x80, 0x80, 0x80, (u8)a);
        }
        if (done) {
            gHudNotice->state++;
        }
        break;
    case 5:
        HudSprite_Show(word, 0);
        HudSprite_Show(band, 0);
        for (i = 0; i < 12; i++) {
            HudSprite_Show(&part[i], 0);
        }
        break;
    }
}

/* Draw callback of announcement 1: the word masked by the band, the twelve fragments, then the word and the
   streak added on top. */
void HudNotice_DrawFight(void) {
    s32 i;

    HudGfx_ClearAlpha();
    HudGfx_CallBegin(HudNotice_GsBeginMask);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_BAND], gHudNotice->res, 0);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndMask);
    for (i = 0; i < 12; i++) {
        HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_PART + i], gHudNotice->res, 0);
    }
    HudGfx_CallBegin(HudNotice_GsBeginAddAlpha);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 0);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_STREAK], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndAddAlpha);
}

/* Update of announcement 1. The reverse of announcement 0's ending: the word shrinks into place and fades in
   while the fragments fly in (0.43 s, cubic); it stands for 0.4 + 0.45 + 0.2 + 0.2 s, then the band closes
   (0.2 s). During the three middle waits a streak (texture 7, three frames of 42 rows) crosses the word:
   sliding in from the left, shaking by a random -16..16 pixels, sliding out to the right while fading. */
void HudNotice_UpdateFight(void) {
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    s16 ofs[12][2] = {
        { 40, -70 }, { 190, -40 }, { 190, 40 }, { 90, 90 }, { -109, -61 }, { -160, -20 },
        { -210, 10 }, { -170, 60 }, { 30, -60 }, { 160, -30 }, { -150, 30 }, { -100, 60 },
    };
    HudDSprite *part = &word[HUD_NOTICE_SPR_PART];
    HudDSprite *streak = &word[HUD_NOTICE_SPR_STREAK];
    HudDSprite *band = &word[HUD_NOTICE_SPR_BAND];
    HudNoticeVec d;
    s32 done;
    s32 i;
    f32 t;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 1, 0);
        HudSprite_Center(word);
        HudSprite_Show(band, 1);
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, 448);
        HudSprite_Center(band);
        Ramp_Start(&gHudNotice->rampB, 0.43f, 0.0f, 1.0f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        done = Ramp_Step(&gHudNotice->rampB);
        t = powf(gHudNotice->rampB.value, 3.0f);
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 1, 0);
        HudSprite_Center(word);
        HudSprite_Scale(word, (1.0f - t) * 0.5f + 1.0f, (1.0f - t) * 0.5f + 1.0f);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)(t * 128.0f));
        for (i = 0; i < 12; i++) {
            f32 a;

            HudSprite_Show(&part[i], 1);
            HudSprite_InitTex(&part[i], gHudNotice->res, 1, 0);
            HudSprite_Center(&part[i]);
            HudSprite_Move(&part[i], (s32)(ofs[i][0] * (1.0f - t)), (s32)(ofs[i][1] * (1.0f - t)));
            if (i < 6) {
                a = gHudNotice->rampB.value - i / 30.0f;
            } else {
                a = gHudNotice->rampB.value - (11 - i) / 30.0f;
            }
            a = powf(HUDN_CLAMP01(a), 3.0f);
            a *= 64.0f;
            HudSprite_SetColor(&part[i], 0x80, 0x80, 0x80, (u8)a);
        }
        if (done) {
            for (i = 0; i < 12; i++) {
                HudSprite_Show(&part[i], 0);
            }
            HudSprite_SetColor(word, 0x80, 0x80, 0x80, 0x80);
            Ramp_Start(&gHudNotice->rampB, 0.4f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 2:
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.45f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.2f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 4:
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.2f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 5:
        done = Ramp_Step(&gHudNotice->rampB);
        t = gHudNotice->rampB.value;
        HudSprite_SetRect(band, 0, 512, 0, (s32)(t * 64.0f));
        HudSprite_Center(band);
        if (done) {
            gHudNotice->state++;
        }
        break;
    case 6:
        HudSprite_Show(word, 0);
        HudSprite_Show(streak, 0);
        return;
    }

    if (gHudNotice->state == 2) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.f[1] = 0.0f;
        d.f[0] = 1.0f - gHudNotice->rampB.value;
        d.f[0] = -(d.f[0] * (streak->uv[1] - streak->uv[0]));
        d.f[1] = d.f[1] * (streak->uv[3] - streak->uv[2]);
        HudSprite_Move(streak, (s32)d.f[0], (s32)d.f[1]);
        HudSprite_SetColor(streak, 0xC0, 0x70, 0x60, 0x80);
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else if (gHudNotice->state == 3) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.i[0] = Rand_IntRange(-16, 16);
        d.i[1] = 0;
        HudSprite_Move(streak, d.i[0], d.i[1]);
        HudSprite_SetColor(streak, 0xC0, 0x70, 0x60, 0x80);
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else if (gHudNotice->state == 4) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.f[1] = 0.0f;
        d.f[0] = 1.0f - gHudNotice->rampB.value;
        d.f[0] = d.f[0] * (streak->uv[1] - streak->uv[0]);
        d.f[1] = d.f[1] * (streak->uv[3] - streak->uv[2]);
        HudSprite_Move(streak, (s32)d.f[0], (s32)d.f[1]);
        {
            f32 v = gHudNotice->rampB.value;

            HudSprite_SetColor(streak, (u8)(v * 192.0f), (u8)(v * 112.0f), (u8)(v * 96.0f), 0x80);
        }
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else {
        HudSprite_Show(streak, 0);
    }
}

/* Draw callback of announcement 2: as HudNotice_DrawFight with four sprites behind the word. */
void HudNotice_DrawKo(void) {
    s32 i;

    HudGfx_ClearAlpha();
    HudGfx_CallBegin(HudNotice_GsBeginMask);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_BAND], gHudNotice->res, 0);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndMask);
    for (i = 0; i < 4; i++) {
        HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_PART + i], gHudNotice->res, 0);
    }
    HudGfx_CallBegin(HudNotice_GsBeginAddAlpha);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 0);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_STREAK], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndAddAlpha);
}

/* Update of announcement 2. The word pops: its scale is 1 + 0.4 * sin(pi * v) with v going 0 -> 0.5 in 0.15 s
   and 0.5 -> 1 in 0.4 s, its alpha min(128, 256 * v); three after-images (sprites 2..4, alpha at most 48) repeat
   the scale of the last three frames and are hidden once they have caught up. Then waits of 0.2 / 0.55 / 0.4 /
   0.2 s (the streak crosses during the middle three, in red) and the band closes. */
void HudNotice_UpdateKo(void) {
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    HudDSprite *part = &word[HUD_NOTICE_SPR_PART];
    HudDSprite *streak = &word[HUD_NOTICE_SPR_STREAK];
    HudDSprite *band = &word[HUD_NOTICE_SPR_BAND];
    HudNoticeVec d;
    f32 *p;
    s32 i;
    f32 s;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 2, 0);
        HudSprite_Center(word);
        HudSprite_Show(band, 1);
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, 448);
        HudSprite_Center(band);
        /* Clears trail[0..3] and starts the ramp from the last one cleared (0). Written this way only because it
           is the form found to match: the original adds 4 to the work first, walks an offset from 0x40 and reads
           the value back through the last pointer. */
        for (i = 3; i >= 0; i--) {
            f32 *base = (f32 *)&gHudNotice->spr;

            p = &base[HUDN_TRAIL_AT4 + 3 - i];
            *p = 0.0f;
        }
        Ramp_Start(&gHudNotice->rampB, 0.15f, *p, 0.5f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        s = Mathf_Sin(gHudNotice->rampB.value * 3.14159265f);
        for (i = 0; i < 3; i++) {
            gHudNotice->trail[3 - i] = gHudNotice->trail[2 - i];
        }
        gHudNotice->trail[0] = s;
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 2, 0);
        HudSprite_Center(word);
        HudSprite_Scale(word, s * 0.4f + 1.0f, s * 0.4f + 1.0f);
        s = HUDN_CLAMP(gHudNotice->rampB.value * 256.0f, 0.0f, 128.0f);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)s);
        for (i = 0; i < 3; i++) {
            s = gHudNotice->trail[i + 1];
            if (s == gHudNotice->trail[0]) {
                HudSprite_Show(&part[i + 1], 0);
            } else {
                HudSprite_Show(&part[i + 1], 1);
            }
            HudSprite_InitTex(&part[i + 1], gHudNotice->res, 2, 0);
            HudSprite_Center(&part[i + 1]);
            HudSprite_Scale(&part[i + 1], s * 0.4f + 1.0f, s * 0.4f + 1.0f);
            s = HUDN_CLAMP(gHudNotice->rampB.value * 256.0f, 0.0f, 48.0f);
            HudSprite_SetColor(&part[i + 1], 0x80, 0x80, 0x80, (u8)s);
        }
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.4f, 0.5f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 2:
        s = Mathf_Sin(gHudNotice->rampB.value * 3.14159265f);
        for (i = 0; i < 3; i++) {
            gHudNotice->trail[3 - i] = gHudNotice->trail[2 - i];
        }
        gHudNotice->trail[0] = s;
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 2, 0);
        HudSprite_Center(word);
        HudSprite_Scale(word, s * 0.4f + 1.0f, s * 0.4f + 1.0f);
        s = HUDN_CLAMP(gHudNotice->rampB.value * 256.0f, 0.0f, 128.0f);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)s);
        for (i = 0; i < 3; i++) {
            s = gHudNotice->trail[i + 1];
            if (s == gHudNotice->trail[0]) {
                HudSprite_Show(&part[i + 1], 0);
            } else {
                HudSprite_Show(&part[i + 1], 1);
            }
            HudSprite_InitTex(&part[i + 1], gHudNotice->res, 2, 0);
            HudSprite_Center(&part[i + 1]);
            HudSprite_Scale(&part[i + 1], s * 0.4f + 1.0f, s * 0.4f + 1.0f);
            s = HUDN_CLAMP(gHudNotice->rampB.value * 256.0f, 0.0f, 48.0f);
            HudSprite_SetColor(&part[i + 1], 0x80, 0x80, 0x80, (u8)s);
        }
        if (Ramp_Step(&gHudNotice->rampB) && (part[i].flags & 1)) { /* i == 3: the last after-image is hidden */
            Ramp_Start(&gHudNotice->rampB, 0.2f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.55f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 4:
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.4f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 5:
        if (Ramp_Step(&gHudNotice->rampB)) {
            Ramp_Start(&gHudNotice->rampB, 0.2f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 6:
        if (Ramp_Step(&gHudNotice->rampB)) {
            gHudNotice->state++;
        }
        HudSprite_SetRect(band, 0, 512, 0, (s32)(gHudNotice->rampB.value * 64.0f));
        HudSprite_Center(band);
        break;
    case 7:
        HudSprite_Show(word, 0);
        HudSprite_Show(part, 0);
        HudSprite_Show(streak, 0);
        return;
    }

    if (gHudNotice->state == 3) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.f[1] = 0.0f;
        d.f[0] = 1.0f - gHudNotice->rampB.value;
        d.f[0] = -(d.f[0] * (streak->uv[1] - streak->uv[0]));
        d.f[1] = d.f[1] * (streak->uv[3] - streak->uv[2]);
        HudSprite_Move(streak, (s32)d.f[0], (s32)d.f[1]);
        HudSprite_SetColor(streak, 0xFF, 0x60, 0x60, 0x80);
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else if (gHudNotice->state == 4) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.i[0] = Rand_IntRange(-16, 16);
        d.i[1] = 0;
        HudSprite_Move(streak, d.i[0], d.i[1]);
        HudSprite_SetColor(streak, 0xFF, 0x60, 0x60, 0x80);
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else if (gHudNotice->state == 5) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.f[1] = 0.0f;
        d.f[0] = 1.0f - gHudNotice->rampB.value;
        d.f[0] = d.f[0] * (streak->uv[1] - streak->uv[0]);
        d.f[1] = d.f[1] * (streak->uv[3] - streak->uv[2]);
        HudSprite_Move(streak, (s32)d.f[0], (s32)d.f[1]);
        {
            f32 v = gHudNotice->rampB.value;

            HudSprite_SetColor(streak, (u8)(v * 255.0f), (u8)(v * 96.0f), (u8)(v * 96.0f), 0x80);
        }
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else {
        HudSprite_Show(streak, 0);
    }
}

/* Draw callback of announcement 3: the word masked by the band, its copy plain on top. */
void HudNotice_DrawBandA(void) {
    HudGfx_ClearAlpha();
    HudGfx_CallBegin(HudNotice_GsBeginMask);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_BAND], gHudNotice->res, 0);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndMask);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_PART], gHudNotice->res, 0);
}

/* Update of announcement 3 (texture 3, rows 64..128, 512 x 64 on screen). A 4-pixel line slides in from the
   left (0.24 s) and opens to 64 pixels (0.24 s); after 0.25 s a copy of the word swells and fades over it
   (0.64 s, cubic); 0.6 s later the band closes to the line (0.24 s), which slides out to the right (0.24 s).
   State 4 starts ramp B but state 5 steps ramp A, which has ended: the 0.6 s wait does not happen. */
void HudNotice_UpdateBandA(void) {
    Ramp *ramp = &gHudNotice->rampA;
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    HudDSprite *band = &word[HUD_NOTICE_SPR_BAND];
    HudDSprite *copy = &word[HUD_NOTICE_SPR_PART];
    s32 done;
    f32 v;
    f32 one;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 3, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 64, 128);
        HudSprite_Show(copy, 0);
        HudSprite_InitTex(copy, gHudNotice->res, 3, 0);
        HudSprite_SetRect(copy, -256, 256, -32, 32);
        HudSprite_SetUv(copy, 0, 512, 64, 128);
        Ramp_Start(ramp, 0.24f, 0.0f, 1.0f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        one = 1.0f;
        done = Ramp_Step(ramp);
        v = one - ramp->value;
        HudSprite_Show(band, 1);
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, 4);
        HudSprite_Center(band);
        HudSprite_Move(band, (s32)(v * -512.0f), 0);
        if (done) {
            Ramp_Start(ramp, 0.24f, 0.0f, one);
            gHudNotice->state++;
        }
        break;
    case 2:
        done = Ramp_Step(ramp);
        v = ramp->value * 60.0f + 4.0f;
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, (s32)v);
        HudSprite_Center(band);
        if (done) {
            Ramp_Start(ramp, 0.25f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.64f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 4:
        done = Ramp_Step(ramp);
        HudSprite_Show(copy, 1);
        HudSprite_InitTex(copy, gHudNotice->res, 3, 0);
        HudSprite_SetRect(copy, -256, 256, -32, 32);
        HudSprite_SetUv(copy, 0, 512, 64, 128);
        HudSprite_SetColor(copy, 0x80, 0x80, 0x80, (u8)(powf(ramp->value, 3.0f) * 128.0f));
        one = 1.0f;
        v = (one - powf(ramp->value, 3.0f)) + one;
        HudSprite_Scale(copy, v, v);
        if (done) {
            Ramp_Start(&gHudNotice->rampB, 0.6f, one, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 5:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.24f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 6:
        done = Ramp_Step(ramp);
        v = ramp->value * 60.0f + 4.0f;
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, (s32)v);
        HudSprite_Center(band);
        if (done) {
            Ramp_Start(ramp, 0.24f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 7:
        done = Ramp_Step(ramp);
        v = ramp->value;
        HudSprite_SetRect(band, 0, 512, 0, 4);
        HudSprite_Center(band);
        HudSprite_Move(band, (s32)(v * 512.0f), 0);
        if (done) {
            gHudNotice->state++;
        }
        break;
    case 8:
        HudSprite_Show(word, 0);
        HudSprite_Show(band, 0);
        HudSprite_Show(copy, 0);
        break;
    }
}

/* Draw callback of announcement 8: the word masked by the band, its copy plain on top. */
void HudNotice_DrawBandB(void) {
    HudGfx_ClearAlpha();
    HudGfx_CallBegin(HudNotice_GsBeginMask);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_BAND], gHudNotice->res, 0);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndMask);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_PART], gHudNotice->res, 0);
}

/* Update of announcement 8 (texture 3, rows 0..64, 512 x 64 on screen). A 4-pixel line slides in from the
   left (0.24 s) and opens to 64 pixels (0.24 s); after 0.25 s a copy of the word swells and fades over it
   (0.64 s, cubic); 0.6 s later the band closes to the line (0.24 s), which slides out to the right (0.24 s).
   State 4 starts ramp B but state 5 steps ramp A, which has ended: the 0.6 s wait does not happen. */
void HudNotice_UpdateBandB(void) {
    Ramp *ramp = &gHudNotice->rampA;
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    HudDSprite *band = &word[HUD_NOTICE_SPR_BAND];
    HudDSprite *copy = &word[HUD_NOTICE_SPR_PART];
    s32 done;
    f32 v;
    f32 one;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 3, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 0, 64);
        HudSprite_Show(copy, 0);
        HudSprite_InitTex(copy, gHudNotice->res, 3, 0);
        HudSprite_SetRect(copy, -256, 256, -32, 32);
        HudSprite_SetUv(copy, 0, 512, 0, 64);
        Ramp_Start(ramp, 0.24f, 0.0f, 1.0f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        one = 1.0f;
        done = Ramp_Step(ramp);
        v = one - ramp->value;
        HudSprite_Show(band, 1);
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, 4);
        HudSprite_Center(band);
        HudSprite_Move(band, (s32)(v * -512.0f), 0);
        if (done) {
            Ramp_Start(ramp, 0.24f, 0.0f, one);
            gHudNotice->state++;
        }
        break;
    case 2:
        done = Ramp_Step(ramp);
        v = ramp->value * 60.0f + 4.0f;
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, (s32)v);
        HudSprite_Center(band);
        if (done) {
            Ramp_Start(ramp, 0.25f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.64f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 4:
        done = Ramp_Step(ramp);
        HudSprite_Show(copy, 1);
        HudSprite_InitTex(copy, gHudNotice->res, 3, 0);
        HudSprite_SetRect(copy, -256, 256, -32, 32);
        HudSprite_SetUv(copy, 0, 512, 0, 64);
        HudSprite_SetColor(copy, 0x80, 0x80, 0x80, (u8)(powf(ramp->value, 3.0f) * 128.0f));
        one = 1.0f;
        v = (one - powf(ramp->value, 3.0f)) + one;
        HudSprite_Scale(copy, v, v);
        if (done) {
            Ramp_Start(&gHudNotice->rampB, 0.6f, one, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 5:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.24f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 6:
        done = Ramp_Step(ramp);
        v = ramp->value * 60.0f + 4.0f;
        HudSprite_InitPlain(band, 0x80, 0x80, 0x80, 0x80);
        HudSprite_SetRect(band, 0, 512, 0, (s32)v);
        HudSprite_Center(band);
        if (done) {
            Ramp_Start(ramp, 0.24f, 0.0f, 1.0f);
            gHudNotice->state++;
        }
        break;
    case 7:
        done = Ramp_Step(ramp);
        v = ramp->value;
        HudSprite_SetRect(band, 0, 512, 0, 4);
        HudSprite_Center(band);
        HudSprite_Move(band, (s32)(v * 512.0f), 0);
        if (done) {
            gHudNotice->state++;
        }
        break;
    case 8:
        HudSprite_Show(word, 0);
        HudSprite_Show(band, 0);
        HudSprite_Show(copy, 0);
        break;
    }
}

/* Update of announcement 4 (texture 4, rows 0..64). The word shrinks from double size to its own while it
   fades in and turns one and a half turns (0.83 s, cubic; the angle is brought into -pi..pi); it stands for 1 s
   and 0.23 s and then swells to double size and fades out (0.23 s). The angle goes to the node, which is the
   update callback's argument. */
void HudNotice_UpdateSpinA(HudDNode *node) {
    Ramp *ramp = &gHudNotice->rampA;
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    HudDSprite *streak = &word[HUD_NOTICE_SPR_STREAK];
    f32 rot = 0.0f;
    s32 done;
    f32 v;
    f32 one;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 4, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 0, 64);
        Ramp_Start(ramp, 0.83f, 1.0f, 0.0f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        done = Ramp_Step(ramp);
        rot = powf(ramp->value, 3.0f) * 1.5f * 6.2831853f;
        while (rot > 3.14159265f) {
            rot -= 6.2831853f;
        }
        HudSprite_InitTex(word, gHudNotice->res, 4, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 0, 64);
        v = 1.0f - powf(ramp->value, 3.0f);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)(v * 128.0f));
        one = 1.0f;
        v = powf(ramp->value, 3.0f) + one;
        HudSprite_Scale(word, v, v);
        if (done) {
            Ramp_Start(ramp, one, 0.0f, one);
            rot = 0.0f;
            gHudNotice->state++;
        }
        break;
    case 2:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.23f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        done = Ramp_Step(ramp);
        HudSprite_InitTex(word, gHudNotice->res, 4, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 0, 64);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)(powf(ramp->value, 3.0f) * 128.0f));
        v = 2.0f - powf(ramp->value, 3.0f);
        HudSprite_Scale(word, v, v);
        if (done) {
            word->a = 0;
            gHudNotice->state++;
        }
        break;
    case 4:
        HudSprite_Show(word, 0);
        HudSprite_Show(streak, 0);
        return;
    }
    HudNode_SetRot(node, rot);
}

/* Update of announcement 5 (texture 4, rows 64..128). The word shrinks from double size to its own while it
   fades in and turns one and a half turns (0.83 s, cubic; the angle is brought into -pi..pi); it stands for 1 s
   and 0.23 s and then swells to double size and fades out (0.23 s). The angle goes to the node, which is the
   update callback's argument. */
void HudNotice_UpdateSpinB(HudDNode *node) {
    Ramp *ramp = &gHudNotice->rampA;
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    HudDSprite *streak = &word[HUD_NOTICE_SPR_STREAK];
    f32 rot = 0.0f;
    s32 done;
    f32 v;
    f32 one;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 4, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 64, 128);
        Ramp_Start(ramp, 0.83f, 1.0f, 0.0f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        done = Ramp_Step(ramp);
        rot = powf(ramp->value, 3.0f) * 1.5f * 6.2831853f;
        while (rot > 3.14159265f) {
            rot -= 6.2831853f;
        }
        HudSprite_InitTex(word, gHudNotice->res, 4, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 64, 128);
        v = 1.0f - powf(ramp->value, 3.0f);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)(v * 128.0f));
        one = 1.0f;
        v = powf(ramp->value, 3.0f) + one;
        HudSprite_Scale(word, v, v);
        if (done) {
            Ramp_Start(ramp, one, 0.0f, one);
            rot = 0.0f;
            gHudNotice->state++;
        }
        break;
    case 2:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.23f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        done = Ramp_Step(ramp);
        HudSprite_InitTex(word, gHudNotice->res, 4, 0);
        HudSprite_SetRect(word, -256, 256, -32, 32);
        HudSprite_SetUv(word, 0, 512, 64, 128);
        HudSprite_SetColor(word, 0x80, 0x80, 0x80, (u8)(powf(ramp->value, 3.0f) * 128.0f));
        v = 2.0f - powf(ramp->value, 3.0f);
        HudSprite_Scale(word, v, v);
        if (done) {
            word->a = 0;
            gHudNotice->state++;
        }
        break;
    case 4:
        HudSprite_Show(word, 0);
        HudSprite_Show(streak, 0);
        return;
    }
    HudNode_SetRot(node, rot);
}

/* Draw callback of announcement 6: the word and its eight copies plain, then the word and the streak added. */
void HudNotice_DrawTrail(void) {
    s32 i;

    HudGfx_ClearAlpha();
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 0);
    for (i = 0; i < 8; i++) {
        HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_PART + i], gHudNotice->res, 0);
    }
    HudGfx_CallBegin(HudNotice_GsBeginAddAlpha);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_WORD], gHudNotice->res, 0);
    HudSprite_Draw(&gHudNotice->spr[HUD_NOTICE_SPR_STREAK], gHudNotice->res, 1);
    HudGfx_CallEnd(HudNotice_GsEndAddAlpha);
}

/* Update of announcement 6 (texture 5). Eight copies of the word zoom in one frame behind each other: copy i
   shows the ramp value of i frames ago (v: 1 -> 0 in 0.93 s; scale 2 - (1 - v)^3, alpha 128 * (1 - v)^3) and is
   hidden when it has caught up with copy 0. When all have arrived the word itself replaces them; waits of 0.35 /
   0.8 / 0.35 / 0.3 s (the streak crosses during the middle three), then the word swells by 0.3 and fades
   (0.3 s, cubic). */
void HudNotice_UpdateTrail(void) {
    Ramp *ramp = &gHudNotice->rampA;
    HudDSprite *word = &gHudNotice->spr[HUD_NOTICE_SPR_WORD];
    HudDSprite *part = &word[HUD_NOTICE_SPR_PART];
    HudDSprite *streak = &word[HUD_NOTICE_SPR_STREAK];
    HudNoticeVec d;
    HudDSprite *spr;
    s32 done;
    s32 i;

    switch (gHudNotice->state) {
    case 0:
        HudSprite_Show(word, 0);
        HudSprite_InitTex(word, gHudNotice->res, 5, 0);
        HudSprite_Center(word);
        HudSprite_Show(streak, 0);
        for (i = 0; i < 8; i++) {
            gHudNotice->trail[i] = 1.0f;
            HudSprite_Show(&part[i], 0);
            HudSprite_InitTex(&part[i], gHudNotice->res, 5, 0);
            HudSprite_Center(&part[i]);
        }
        Ramp_Start(ramp, 0.93f, 1.0f, 0.0f);
        gHudNotice->state++;
        /* fall through */
    case 1:
        for (i = 6; i >= 0; i--) {
            gHudNotice->trail[i + 1] = gHudNotice->trail[i];
        }
        done = Ramp_Step(ramp);
        gHudNotice->trail[0] = ramp->value;
        for (i = 0; i < 8; i++) {
            f32 a;

            if (i > 0) {
                if (gHudNotice->trail[0] != gHudNotice->trail[i - 1]) {
                    HudSprite_Show(&part[i], 1);
                } else {
                    HudSprite_Show(&part[i], 0);
                }
            } else {
                HudSprite_Show(&part[i], 1);
            }
            HudSprite_InitTex(&part[i], gHudNotice->res, 5, 0);
            spr = &part[i];
            HudSprite_Center(&part[i]);
            a = 1.0f - powf(1.0f - gHudNotice->trail[i], 3.0f);
            a += 1.0f;
            HudSprite_Scale(&part[i], a, a);
            HudSprite_SetColor(spr, 0x80, 0x80, 0x80,
                          (u8)((1.0f - (1.0f - powf(1.0f - gHudNotice->trail[i], 3.0f))) * 128.0f));
        }
        if (done && gHudNotice->trail[0] == gHudNotice->trail[7]) {
            Ramp_Start(ramp, 0.35f, 0.0f, 1.0f);
            for (i = 0; i < 8; i++) {
                HudSprite_Show(&part[i], 0);
            }
            HudSprite_Show(word, 1);
            gHudNotice->state++;
        }
        break;
    case 2:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.8f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 3:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.35f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 4:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.3f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 5:
        if (Ramp_Step(ramp)) {
            Ramp_Start(ramp, 0.3f, 1.0f, 0.0f);
            gHudNotice->state++;
        }
        break;
    case 6:
        HudSprite_Show(word, 1);
        HudSprite_InitTex(word, gHudNotice->res, 5, 0);
        HudSprite_Center(word);
        if (Ramp_Step(ramp)) {
            word->a = 0;
            gHudNotice->state++;
        } else {
            f32 a;

            word->a = (u8)(powf(ramp->value, 3.0f) * 128.0f);
            a = (1.0f - powf(ramp->value, 3.0f)) * 0.3f + 1.0f;
            HudSprite_Scale(word, a, a);
        }
        break;
    case 7:
        HudSprite_Show(word, 0);
        HudSprite_Show(streak, 0);
        for (i = 0; i < 8; i++) {
            HudSprite_Show(&part[i], 0);
        }
        return;
    }

    if (gHudNotice->state == 2) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.f[1] = 0.0f;
        d.f[0] = 1.0f - ramp->value;
        d.f[0] = -(d.f[0] * (streak->uv[1] - streak->uv[0]));
        d.f[1] = d.f[1] * (streak->uv[3] - streak->uv[2]);
        HudSprite_Move(streak, (s32)d.f[0], (s32)d.f[1]);
        HudSprite_SetColor(streak, 0xC0, 0x70, 0x60, 0x80);
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else if (gHudNotice->state == 3) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.i[0] = Rand_IntRange(-16, 16);
        d.i[1] = 0;
        HudSprite_Move(streak, d.i[0], d.i[1]);
        HudSprite_SetColor(streak, 0xC0, 0x70, 0x60, 0x80);
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else if (gHudNotice->state == 4) {
        memset(&d, 0, sizeof(d));
        HudSprite_Show(streak, 1);
        HudSprite_InitTex(streak, gHudNotice->res, 7, 0);
        HudSprite_SetRect(streak, 0, 512, 0, 42);
        HudSprite_SetUv(streak, 0, 512, gHudNotice->streak * 42, gHudNotice->streak * 42 + 42);
        HudSprite_Center(streak);
        d.f[1] = 0.0f;
        d.f[0] = 1.0f - ramp->value;
        d.f[0] = d.f[0] * (streak->uv[1] - streak->uv[0]);
        d.f[1] = d.f[1] * (streak->uv[3] - streak->uv[2]);
        HudSprite_Move(streak, (s32)d.f[0], (s32)d.f[1]);
        {
            f32 v = ramp->value;

            HudSprite_SetColor(streak, (u8)(v * 192.0f), (u8)(v * 112.0f), (u8)(v * 96.0f), 0x80);
        }
        gHudNotice->streak++;
        if (gHudNotice->streak >= 3) {
            gHudNotice->streak = 0;
        }
    } else {
        HudSprite_Show(streak, 0);
    }
}
