#include "common.h"
#include "battle/hud_e.h"
#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "sys/heap.h"
#include "sys/mathf.h"
#include "sys/rand.h"
#include "sys/save.h"
#include "sys/adx.h"

/*
 * Battle HUD, notice part (the full-screen announcements), second half: 0x22A750-0x22B4F8.
 * The animations of announcements 0..6 and 8 are in the neighbouring range (0x226500-0x22A750); this file has
 * announcement 7, the starter HudNotice_Show, and the part's init / term / reset. It is one module with that
 * range (the work is gHudNotice, the struct below is this file's view of it).
 *
 * Announcements (HudNotice_Show(id)); the voice line is HudNotice_GetVoiceBase() + announcer * 7 + line:
 *   0  line 0  BtlSeqReady_Update          5  line 5  finish: time up
 *   1  line 1  BtlSeqReady_Exit            6  none    lower banner (winner's entry in the result sequence)
 *   2  line 2  finish: K.O.                7  none    lower banner, texture 6 (HudNotice_UpdateBanner)
 *   3  line 3  finish: the other K.O.      8  line 2 or 3, picked by Rand_Range(2): Hud_PreUpdate, when
 *   4  line 4  finish: reason bit 2                   BtlCtrl_TestProgressFrameBit() is set
 */

/* The part's work pointer lives in .sdata at 0x2FEB5C, right behind gHudCombo (0x2FEB58). It is defined here because
   this file has the part's init; the neighbouring file (hud_d_b.c) declares it extern. */
HudENotice *gHudNotice = NULL;

/* Sprite / node library at 0x224B50.. and the animations of the other announcements (neighbouring ranges). */
extern void HudSprite_Show(HudESprite *spr, s32 show);
extern void HudSprite_SetColor(HudESprite *spr, s32 r, s32 g, s32 b, s32 a);
extern void HudSprite_InitPlain(HudESprite *spr, s32 r, s32 g, s32 b, s32 a);     /* untextured, coloured */
extern void HudSprite_Move(HudESprite *spr, s32 dx, s32 dy);                 /* moves the rectangle */
extern void HudSprite_Scale(HudESprite *spr, f32 sx, f32 sy);                 /* scales the rectangle */
extern void HudSprite_Center(HudESprite *spr);                                 /* centres the rectangle */
extern void HudSprite_InitTex(HudESprite *spr, HudERes *res, s32 tex, s32 sub); /* sprite of a sheet's texture */
extern void HudGfx_ClearAlpha(void);
extern void HudSprite_Draw(HudESprite *spr, HudERes *res, s32 additive);     /* HudSprite_Draw */
extern void HudNode_SetPos(HudENode *node, s32 x, s32 y);                    /* HudNode_SetPos */
extern void HudNotice_DrawNode();                                                /* HudNotice_DrawNode */
extern void HudNotice_DrawSpin();                                                /* HudNotice_DrawSpin */
extern void HudNotice_DrawReady();                                                /* HudNotice_DrawReady */
extern void HudNotice_UpdateReady();                                                /* HudNotice_UpdateReady */
extern void HudNotice_DrawFight();
extern void HudNotice_UpdateFight();
extern void HudNotice_DrawKo();
extern void HudNotice_UpdateKo();
extern void HudNotice_DrawBandA();
extern void HudNotice_UpdateBandA();
extern void HudNotice_DrawBandB();
extern void HudNotice_UpdateBandB();
extern void HudNotice_UpdateSpinA();
extern void HudNotice_UpdateSpinB();
extern void HudNotice_DrawTrail();
extern void HudNotice_UpdateTrail();
extern void *memset(void *dst, s32 c, u32 n);
extern f32 powf(f32 x, f32 y);

/* Draw callback of announcement 7: the banner sprite. */
void HudNotice_DrawBanner(void) {
    HudGfx_ClearAlpha();
    HudSprite_Draw(&gHudNotice->spr[0], gHudNotice->res, 0);
}

/* Update callback of announcement 7. The banner (texture 6) drops in from 100 pixels above over 0.4 s while
   fading in (sine ease), bounces 10 pixels over 0.36 s, holds 1.73 s, then fades out over 0.4 s (alpha t^3,
   size 0.5 + 0.5 t^3, t from 1 to 0) and is hidden. Not frozen by the pause flag. */
void HudNotice_UpdateBanner(void) {
    Ramp *ramp = &gHudNotice->rampA;
    HudESprite *spr = &gHudNotice->spr[0];
    HudESprite *streak = &spr[HUD_NOTICE_SPR_STREAK];
    s32 done;
    f32 t;

    switch (gHudNotice->state) {
        case 0:
            HudSprite_Show(spr, 1);
            HudSprite_InitTex(spr, gHudNotice->res, 6, 0);
            HudSprite_Center(spr);
            HudSprite_Show(streak, 0);
            Ramp_Start(ramp, 0.4f, 1.0f, 0.0f);
            gHudNotice->state++;
            /* fall through */
        case 1:
            done = Ramp_Step(ramp);
            t = Mathf_Sin(ramp->value * 1.5707963f);
            HudSprite_InitTex(spr, gHudNotice->res, 6, 0);
            HudSprite_Center(spr);
            HudSprite_Move(spr, 0, t * -100.0f);
            t = 1.0f - t;
            HudSprite_SetColor(spr, 0x80, 0x80, 0x80, (u8)(u32)(t * 128.0f));
            if (done) {
                Ramp_Start(&gHudNotice->rampA, 0.36f, 0.0f, 1.0f);
                gHudNotice->state++;
            }
            break;
        case 2:
            done = Ramp_Step(ramp);
            t = Mathf_Sin(ramp->value * 3.1415926f) * 0.1f;
            HudSprite_InitTex(spr, gHudNotice->res, 6, 0);
            HudSprite_Center(spr);
            HudSprite_Move(spr, 0, t * -100.0f);
            t = 1.0f - t;
            HudSprite_SetColor(spr, 0x80, 0x80, 0x80, (u8)(u32)(t * 128.0f));
            if (done) {
                Ramp_Start(ramp, 1.73f, 1.0f, 0.0f);
                gHudNotice->state++;
            }
            break;
        case 3:
            if (Ramp_Step(ramp)) {
                Ramp_Start(ramp, 0.4f, 1.0f, 0.0f);
                gHudNotice->state++;
            }
            break;
        case 4:
            done = Ramp_Step(ramp);
            if (done) {
                spr->a = 0;
                gHudNotice->state++;
            } else {
                t = powf(ramp->value, 3.0f);
                spr->a = (u32)(t * 128.0f);
                t = t * 0.5f + 0.5f;
                HudSprite_Scale(spr, t, t);
            }
            /* An empty statement that still needs a test: without it the call above becomes a tail call. The
               original had something here that compiled to nothing. */
            if (done) {
            }
            break;
        case 5:
            HudSprite_Show(spr, 0);
            HudSprite_Show(streak, 0);
            break;
    }
}

/* Base id of the announcer's voice lines for the voice set of the options. */
s32 HudNotice_GetVoiceBase(void) {
    if (gSaveData->flags & SAVE_FLAG_VOICE) {
        return DISC_RESOURCE(0x10B6C, 0x971);
    }
    return DISC_RESOURCE(0x10B34, 0x939);
}

/* Starts announcement `id` (see the top of the file): hides the word's sprites, resets node 1 and gives it the
   announcement's place and callbacks, and plays the announcer's line. */
void HudNotice_Show(s32 id) {
    HudENode *node;
    s32 i;

    HudSprite_Show(&gHudNotice->spr[HUD_NOTICE_SPR_STREAK], 0);
    for (i = 0; i < 12; i++) {
        HudSprite_Show(&gHudNotice->spr[i + 1], 0);
    }
    HudSprite_Show(&gHudNotice->spr[0], 0);
    node = &gHudNotice->node[1];
    node->rot = 0.0f;
    node->unk8 = 0.0f;
    node->unkC = 0.0f;
    gHudNotice->state = 0;
    switch (id) {
        case 0:
            HudNode_SetPos(node, 256, 224);
            node->update = HudNotice_UpdateReady;
            node->draw = HudNotice_DrawReady;
            StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7);
            break;
        case 1:
            HudNode_SetPos(node, 256, 224);
            node->update = HudNotice_UpdateFight;
            node->draw = HudNotice_DrawFight;
            StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7 + 1);
            break;
        case 2:
            HudNode_SetPos(node, 256, 224);
            node->update = HudNotice_UpdateKo;
            node->draw = HudNotice_DrawKo;
            StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7 + 2);
            break;
        case 3:
            HudNode_SetPos(node, 256, 224);
            node->update = HudNotice_UpdateBandA;
            node->draw = HudNotice_DrawBandA;
            StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7 + 3);
            break;
        case 4:
            HudNode_SetPos(node, 256, 224);
            node->update = HudNotice_UpdateSpinA;
            node->draw = HudNotice_DrawSpin;
            StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7 + 4);
            break;
        case 5:
            HudNode_SetPos(node, 256, 224);
            node->update = HudNotice_UpdateSpinB;
            node->draw = HudNotice_DrawSpin;
            StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7 + 5);
            break;
        case 6:
            HudNode_SetPos(node, 256, 352);
            node->update = HudNotice_UpdateTrail;
            node->draw = HudNotice_DrawTrail;
            break;
        case 7:
            HudNode_SetPos(node, 256, 352);
            node->update = HudNotice_UpdateBanner;
            node->draw = HudNotice_DrawBanner;
            break;
        case 8:
            HudNode_SetPos(node, 256, 224);
            node->update = HudNotice_UpdateBandB;
            node->draw = HudNotice_DrawBandB;
            if (Rand_Range(2)) {
                StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7 + 2);
            } else {
                StreamSe_PlayDefault(0, HudNotice_GetVoiceBase() + Battle_GetAnnouncer() * 7 + 3);
            }
            break;
    }
}

/* Shows or hides the replay mark (sprite 15, texture 8, top centre). */
void HudNotice_ShowReplayMark(s32 on) {
    HudSprite_Show(&gHudNotice->spr[HUD_NOTICE_SPR_REPLAY], on);
}

/* Builds the part: the work, 16 sprites and 4 nodes. Node 1 (256, 224) is the announcement (sprites 0, 1, 13),
   node 2 the band (sprite 14, untextured; it is not linked into the tree), node 3 (256, 50) the replay mark;
   the root has nodes 1 and 3 as children. */
void HudNotice_Init(HudENode **out, HudERes *res) {
    HudESprite *spr;
    HudENode *node;
    s32 one = 1;
    s32 cx = 256;
    s32 cy = 224;

    gHudNotice = Heap_Alloc(sizeof(HudENotice), 0x20, 0, 2);
    memset(gHudNotice, 0, sizeof(HudENotice));
    gHudNotice->spr = Heap_Alloc(HUD_NOTICE_SPR_COUNT * sizeof(HudESprite), 0x20, 0, 2);
    memset(gHudNotice->spr, 0, HUD_NOTICE_SPR_COUNT * sizeof(HudESprite));
    gHudNotice->node = Heap_Alloc(HUD_NOTICE_NODE_COUNT * sizeof(HudENode), 0x20, 0, 2);
    memset(gHudNotice->node, 0, HUD_NOTICE_NODE_COUNT * sizeof(HudENode));
    gHudNotice->state = -1;
    gHudNotice->res = res;

    spr = &gHudNotice->spr[0];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudNotice->spr[1];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudNotice->spr[HUD_NOTICE_SPR_STREAK];
    HudSprite_InitTex(spr, res, 7, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudNotice->spr[HUD_NOTICE_SPR_BAND];
    HudSprite_Show(spr, 0);
    HudSprite_InitPlain(spr, 0x80, 0x80, 0x80, 0x80);
    HudSprite_Center(spr);

    spr = &gHudNotice->spr[HUD_NOTICE_SPR_REPLAY];
    HudSprite_Show(spr, 0);
    HudSprite_InitTex(spr, res, 8, 0);
    HudSprite_Center(spr);

    node = &gHudNotice->node[2];
    node->x = 0;
    node->y = 0;
    node->sprCount = one;
    node->sprList = Heap_Alloc(1 * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudNotice->spr[HUD_NOTICE_SPR_BAND];
    node->update = NULL;
    node->draw = NULL;

    node = &gHudNotice->node[3];
    node->x = cx;
    node->y = 50;
    node->sprCount = one;
    node->sprList = Heap_Alloc(1 * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudNotice->spr[HUD_NOTICE_SPR_REPLAY];
    node->update = NULL;
    node->draw = HudNotice_DrawNode;

    node = &gHudNotice->node[1];
    node->x = cx;
    node->y = cy;
    node->sprCount = 3;
    node->sprList = Heap_Alloc(3 * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudNotice->spr[0];
    node->sprList[1] = &gHudNotice->spr[1];
    node->sprList[2] = &gHudNotice->spr[HUD_NOTICE_SPR_STREAK];
    node->update = HudNotice_UpdateReady;
    node->draw = HudNotice_DrawSpin;

    node = &gHudNotice->node[0];
    node->x = 0;
    node->y = 0;
    node->childCount = 2;
    node->childList = Heap_Alloc(2 * sizeof(HudENode *), 0x20, 0, 2);
    memset(node->childList, 0, node->childCount * sizeof(HudENode *));
    node->childList[0] = &gHudNotice->node[1];
    node->childList[1] = &gHudNotice->node[3];
    node->update = NULL;
    node->draw = NULL;

    *out = &gHudNotice->node[0];
}

/* Frees the part. */
void HudNotice_Term(void) {
    s32 i;

    if (gHudNotice->spr != NULL) {
        Heap_Free(gHudNotice->spr);
    }
    for (i = 0; i < HUD_NOTICE_NODE_COUNT; i++) {
        if (gHudNotice->node[i].sprList != NULL) {
            Heap_Free(gHudNotice->node[i].sprList);
        }
        if (gHudNotice->node[i].childList != NULL) {
            Heap_Free(gHudNotice->node[i].childList);
        }
    }
    if (gHudNotice->node != NULL) {
        Heap_Free(gHudNotice->node);
    }
    if (gHudNotice != NULL) {
        Heap_Free(gHudNotice);
    }
}

/* Round reset: no announcement, ramps cleared, the word's sprites hidden, node 1 unrotated. */
void HudNotice_Reset(void) {
    HudENode *node;
    s32 i;

    gHudNotice->state = -1;
    memset(&gHudNotice->rampA, 0, sizeof(Ramp));
    memset(&gHudNotice->rampB, 0, sizeof(Ramp));
    HudSprite_Show(&gHudNotice->spr[HUD_NOTICE_SPR_STREAK], 0);
    for (i = 0; i < 12; i++) {
        HudSprite_Show(&gHudNotice->spr[i + 1], 0);
    }
    HudSprite_Show(&gHudNotice->spr[0], 0);
    node = &gHudNotice->node[1];
    node->rot = 0.0f;
    node->unk8 = 0.0f;
    node->unkC = 0.0f;
}
