#include "common.h"
#include "menu/menu_j.h"
#include "sys/pad.h"
#include "sys/save.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * TourMenu, 0x3623A8..0x364DA8: the tournament menu of Dragon World Tour (progress mode 33). One object (its
 * strings "fl_off_start", "fl_on_start", "fl_ok" repeat those of EntrySel; its .rodata starts at 0x3B5DB0). Was
 * written as two chunks (menu_j_b.c up to 0x364358, menu_k.c from TourMenu_UpdateSeq 0x364358 on) which share the
 * strings "fl_menu_in", "fl_on_start", ...; TourMenu_Run 0x364B18 copies the choice to gProgress (+0x80 entry,
 * +0x84 tournament, +0x88 level, +0x8C entrants).
 */

TourMenu *gTourMenu = NULL; /* 0x3B5914 */

extern void IconWin_SetIcon(s32 icon);
extern char *strcpy(char *, const char *);
extern void Voice_StopWithLip(void);
extern void StreamSe_FadeOutStep(s32 se);

/* The hour's tournament sends its invitation once: it becomes the open one and gets a random level. */
#define TOUR_INVITE(n) \
    if (!(gSaveData->unkA08 & (1 << (n)))) { \
        gTourMenu->flags |= TOURMENU_INVITE; \
        gTourMenu->invite = (n); \
        gSaveData->unkA08 &= ~TOUR_SAVE_INVITE_MASK; \
        gSaveData->unkA08 |= 1 << (n); \
        gSaveData->unkA10 = Rand_Range(3); \
    }

/* Which tournament is held at the current hour of the mode's clock (gSaveData->unkA0C, 0..23). */
void TourMenu_CheckInvite(void) {
    s32 hour = gSaveData->unkA0C;

    if (hour >= 7 && hour <= 12) {
        TOUR_INVITE(TOUR_WORLD);
    } else if (hour >= 13 && hour <= 18) {
        TOUR_INVITE(TOUR_BIG);
    } else if (hour >= 19 && hour <= 23) {
        TOUR_INVITE(TOUR_CELL);
    } else if (hour >= 0 && hour <= 4) {
        TOUR_INVITE(TOUR_OTHERWORLD);
    } else if (hour >= 5 && hour <= 6) {
        TOUR_INVITE(TOUR_YAMCHA);
    }
}

#define TM_RES(n) \
    res = (MTexRes *)MPACK_AT(gTourMenu->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads the screen: section `section` of archive 4, and the invitation picture if one arrived. */
void TourMenu_Init(s32 section) {
    MTexRes *res = NULL;

    gTourMenu = Heap_Alloc(sizeof(TourMenu), 0x20, 0, 2);
    memset(gTourMenu, 0, sizeof(TourMenu));
    gTourMenu->pack = (u32 *)MPACK_AT(gMenuArc4, section);
    gTourMenu->res = Sprite_Unpack(gTourMenu->pack, NULL, NULL);
    if (!(gSaveData->unkA08 & TOUR_SAVE_STARTED)) {
        gSaveData->unkA0C = 7;
    }
    TourMenu_CheckInvite();
    TM_RES(1);
    gTourMenu->bg = res;
    TM_RES(2);
    gTourMenu->tex[3] = MTEX(res, 0);
    gTourMenu->tex[0] = MTEX(res, 1);
    gTourMenu->tex[1] = MTEX(res, 2);
    gTourMenu->tex[2] = MTEX(res, 3);
    TM_RES(3);
    gTourMenu->tex[4] = MTEX(res, 0);
    gTourMenu->tex[6] = MTEX(res, 1);
    gTourMenu->tex[5] = MTEX(res, 3);
    TM_RES(4);
    gTourMenu->tex[7] = MTEX(res, 0);
    gTourMenu->tex[10] = MTEX(res, 1);
    gTourMenu->tex[9] = MTEX(res, 2);
    TM_RES(5);
    gTourMenu->tex[8] = MTEX(res, 0);
    gTourMenu->tex[11] = MTEX(res, 1);
    TM_RES(6);
    gTourMenu->tex[27] = MTEX(res, 0);
    TM_RES(7);
    gTourMenu->tex[25] = MTEX(res, 0);
    TM_RES(8);
    gTourMenu->tex[22] = MTEX(res, 0);
    gTourMenu->tex[23] = MTEX(res, 1);
    gTourMenu->tex[24] = MTEX(res, 2);
    TM_RES(9);
    gTourMenu->tex[18] = MTEX(res, 0);
    TM_RES(10);
    gTourMenu->tex[17] = MTEX(res, 0);
    TM_RES(11);
    gTourMenu->tex[20] = MTEX(res, 0);
    TM_RES(12);
    gTourMenu->tex[12] = MTEX(res, 1);
    gTourMenu->tex[16] = MTEX(res, 2);
    TM_RES(13);
    gTourMenu->tex[21] = MTEX(res, 0);
    gTourMenu->tex[28] = MTEX(res, 1);
    TM_RES(14);
    gTourMenu->tex[26] = MTEX(res, 0);
    TM_RES(25);
    gTourMenu->tex[14] = MTEX(res, 0);
    gTourMenu->tex[13] = MTEX(res, 1);
    gTourMenu->tex[15] = MTEX(res, 2);
    TM_RES(26);
    gTourMenu->tex[30] = MTEX(res, 0);
    gTourMenu->tex[29] = MTEX(res, 1);
    Flash_Create(&gTourMenu->flash[0], MPACK_AT(gTourMenu->res, 15), gTourMenu->tex);
    Flash_Play(&gTourMenu->flash[0], 1);
    gTourMenu->imageFile = Heap_Alloc(0xF000, 0x40, 0, 2);
    gTourMenu->imageRes = Heap_Alloc(0x41000, 0x20, 0, 2);
    if (gTourMenu->flags & TOURMENU_INVITE) {
        File_LoadSync(gTourMenu->invite + 0x3C4, gTourMenu->imageFile, 0xF000);
        Sprite_Unpack(gTourMenu->imageFile, gTourMenu->imageRes, NULL);
        res = gTourMenu->imageRes;
        Res_RelocateOffsets(&res, res, res);
        gTourMenu->texB[4] = MTEX(res, 0);
        gTourMenu->texB[3] = MTEX(res, 1);
        TM_RES(16);
        gTourMenu->texB[2] = MTEX(res, 0);
        gTourMenu->texB[5] = MTEX(res, 1);
        TM_RES(17);
        gTourMenu->texB[1] = MTEX(res, 0);
        gTourMenu->texB[0] = MTEX(res, 1);
    }
    Flash_Create(&gTourMenu->flash[1], MPACK_AT(gTourMenu->res, 18), gTourMenu->texB);
    TM_RES(22);
    IconWin_Init(MPACK_AT(gTourMenu->res, 21), res);
    IconWin_Open();
    gTourMenu->msgText = MPACK_AT(gTourMenu->res, 19);
    gTourMenu->subtitles = MPACK_AT(gTourMenu->res, 23);
    MsgWin_Init(MPACK_AT(gTourMenu->res, 20), gTourMenu->msgText, 0, 0);
    MsgWin_Open();
    gTourMenu->info = (TourInfo *)MPACK_AT(gTourMenu->res, 27);
    gTourMenu->voiceLine = -1;
    StreamSe_PlayDefault(0, 0x15C);
}

/* Frees everything TourMenu_Init made. */
void TourMenu_Term(void) {
    s32 i;

    MsgWin_Term();
    IconWin_Term();
    for (i = 0; i < TOURMENU_FLASH_NUM; i++) {
        Flash_Destroy(&gTourMenu->flash[i]);
    }
    if (gTourMenu->imageRes != NULL) {
        Heap_Free(gTourMenu->imageRes);
        gTourMenu->imageRes = NULL;
    }
    if (gTourMenu->imageFile != NULL) {
        Heap_Free(gTourMenu->imageFile);
        gTourMenu->imageFile = NULL;
    }
    if (gTourMenu->res != NULL) {
        Heap_Free(gTourMenu->res);
        gTourMenu->res = NULL;
    }
    if (gTourMenu != NULL) {
        Heap_Free(gTourMenu);
        gTourMenu = NULL;
    }
}

/* Draws the background, sets up the clock, the prize money, the plates and the icons, and draws the movies. */
void TourMenu_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    MFlash *flash;

#ifdef PORT
    Port_MenuPage(20); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gTourMenu->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gTourMenu->bg, 0, 0, 0x80);
#endif
    flash = &gTourMenu->flash[0];
    Flash_FindLabel(flash, NULL, "mc_guide_satan_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gTourMenu->blink, 0);
    Flash_FindLabel(flash, NULL, "mc_guide_satan_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gTourMenu->talk, 0);
    /* the clock */
    Num_DrawChild(flash, "mc_timer", "mc_timer_num%02d", 2, 2, gSaveData->unkA0C, 0x40, 0x40, 1, 0);
    for (i = 0; i < 2; i++) {
        /* the two prizes of the tournament under the cursor, at the open tournament's level */
        sprintf(name, "mc_menu_text_zenny%d", i);
        Num_DrawChild(flash, name, "menu_text_num%d", 0, 7,
                      gTourMenu->info[gTourMenu->cursor[TOURMENU_LV_TOUR]].prize[i][gSaveData->unkA10], 0x20, 0x20,
                      gTourMenu->cursor[TOURMENU_LV_TOP] == 1 ? 2 : 0, 0);
    }
    for (i = 0; i < 2; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x200;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_menu_plate_%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    uv.y0 = 0;
    uv.x0 = 0;
    uv.y1 = 0x40;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_icon_ship", &ref);
    FlashAnim_Sheet(flash, &ref, &gTourMenu->iconTimer, &gTourMenu->iconFrame, &uv, 4, 1, 4);
    for (i = 0; i < TOUR_NUM; i++) {
        /* the five tournaments: dimmed in the top entry unless the tournament is open */
        uv.x0 = 0;
        uv.y0 = i * 0x20 + 0x40;
        uv.y1 = i * 0x20 + 0x60;
        uv.x1 = 0x200;
        sprintf(name, "mc_taikai_plate_%02d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (gTourMenu->cursor[TOURMENU_LV_TOP] != 1 && !(gSaveData->unkA08 & (s32)(1U << i))) {
            Flash_ClipSetColor(flash, &ref, 0.5f);
        } else {
            Flash_ClipSetColor(flash, &ref, 1.0f);
        }
        Flash_FindLabel(flash, name, "mc_taikai_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_taikai_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        uv.y0 = 0;
        uv.y1 = 0x40;
        uv.x0 = i * 0x40;
        uv.x1 = uv.x0 + 0x40;
        Flash_FindLabel(flash, name, "mc_taikai_icon_s", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    uv.y0 = (gTourMenu->cursor[TOURMENU_LV_TOUR] % 4) * 0x40;
    uv.x0 = 0;
    uv.x1 = 0x200;
    uv.y1 = uv.y0 + 0x40;
    Flash_FindLabel(flash, "mc_taikaititle_plate", "mc_taikaititle_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, gTourMenu->cursor[TOURMENU_LV_TOUR] / 4);
    uv.y0 = (gTourMenu->cursor[TOURMENU_LV_TOUR] / 4) * 0x40;
    uv.y1 = uv.y0 + 0x40;
    uv.x0 = (gTourMenu->cursor[TOURMENU_LV_TOUR] % 4) * 0x40;
    uv.x1 = uv.x0 + 0x40;
    Flash_FindLabel(flash, "mc_taikaititle_plate", "mc_plate_icon_mark", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, NULL, "mc_taikai_icon", &ref);
    Flash_ClipSetTex(flash, &ref, gTourMenu->cursor[TOURMENU_LV_TOUR]);
    for (i = 0; i < 2; i++) {
        /* the arrows of the level and entrant numbers: only the free entry lets them change */
        uv.y0 = 0;
        uv.y1 = 0x40;
        uv.x0 = i * 0x40;
        uv.x1 = uv.x0 + 0x40;
        Flash_FindLabel(flash, "mc_yajirusi_u_l", i != 0 ? "mc_yajirusi_l" : "mc_yajirusi_r", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetFlags(flash, &ref, 2, gTourMenu->cursor[TOURMENU_LV_TOP] == 1);
    }
    for (i = 0; i < 3; i++) {
        /* the three levels, shown as dragon balls */
        uv.x0 = i * 0x40;
        uv.x1 = uv.x0 + 0x40;
        uv.y0 = 0;
        uv.y1 = 0x40;
        sprintf(name, "mc_dragonball_off_%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_dragonball_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_dragonball_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    /* the number of entrants */
    uv.y0 = ((gTourMenu->cursor[TOURMENU_LV_NUM] + 1) / 4) * 0x40;
    uv.y1 = uv.y0 + 0x40;
    uv.x0 = ((gTourMenu->cursor[TOURMENU_LV_NUM] + 1) % 4) * 0x40;
    uv.x1 = uv.x0 + 0x40;
    Flash_FindLabel(flash, NULL, "mc_suji_01", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < 2; i++) {
        uv.y0 = (i ^ 1) * 0x20;
        uv.x0 = 0;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_plate_text_01_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    for (i = 1; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_menu_text_01_%d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    for (i = 0; i < TOURMENU_FLASH_NUM; i++) {
        Flash_Draw(&gTourMenu->flash[i]);
    }
    IconWin_Draw();
    MsgWin_Draw(0, 0, gTourMenu->voiceLine);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Sends a clip of movie `movie` to a label: the plate under the cursor of menu level `kind`. */
void TourMenu_ClipGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gTourMenu->flash[movie];

    switch (kind) {
    case TOURMENU_LV_TOP:
        sprintf(name, "mc_menu_plate_%02d", gTourMenu->cursor[kind] + 1);
        break;
    case TOURMENU_LV_TOUR:
        sprintf(name, "mc_taikai_plate_%02d", gTourMenu->cursor[kind] + 1);
        break;
    case TOURMENU_LV_INFO:
        strcpy(name, "mc_taikaititle_plate");
        break;
    case TOURMENU_LV_LEVEL:
        sprintf(name, "mc_dragonball_off_%02d", gTourMenu->cursor[kind] + 1);
        break;
    case TOURMENU_LV_NUM:
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Advances the movies; after the greeting, the guide reads the entry under the cursor. */
void TourMenu_Update(void) {
    s32 i;

    if ((gTourMenu->flags & TOURMENU_GREETED) && gTourMenu->voiceLine == 3 && Voice_GetStat(0) == MVOICE_IDLE) {
        gTourMenu->voiceLine = gTourMenu->cursor[TOURMENU_LV_TOP] + 0xE;
        Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
    }
    for (i = 0; i < TOURMENU_FLASH_NUM; i++) {
        Flash_Advance(&gTourMenu->flash[i]);
    }
}

#define TM_CUR (gTourMenu->cursor[gTourMenu->level])

/* Pad 0: the five menu levels. Confirming the last one starts the closing sequence (seq 451). */
void TourMenu_Input(s32 *result) {
    if (!(gTourMenu->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gTourMenu->flags & TOURMENU_STARTED)) {
        gTourMenu->flags |= TOURMENU_STARTED;
    }
    switch (gTourMenu->level) {
    case TOURMENU_LV_TOP:
        if (gPad[0].gameRepeat & 8) {
            TourMenu_ClipGoto(0, 0, "fl_off_start");
            TM_CUR--;
            if (TM_CUR < 0) {
                TM_CUR = 1;
            }
            TourMenu_ClipGoto(0, 0, "fl_on_start");
            gTourMenu->voiceLine = TM_CUR + 0xE;
            Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 4) {
            TourMenu_ClipGoto(0, 0, "fl_off_start");
            TM_CUR++;
            if (TM_CUR >= 2) {
                TM_CUR = 0;
            }
            TourMenu_ClipGoto(0, 0, "fl_on_start");
            gTourMenu->voiceLine = TM_CUR + 0xE;
            Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_taikaisentaku_in", 1);
            TourMenu_ClipGoto(0, 0, "fl_ok");
            TourMenu_ClipGoto(0, 1, "fl_on_start");
            if (TM_CUR == 0) {
                gTourMenu->voiceLine = 0x11;
                Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
            }
            gTourMenu->level = TOURMENU_LV_TOUR;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_cansel", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case TOURMENU_LV_TOUR:
        if (gPad[0].gameRepeat & 8) {
            TourMenu_ClipGoto(0, 1, "fl_off_start");
            TM_CUR--;
            if (TM_CUR < 0) {
                TM_CUR = TOUR_NUM - 1;
            }
            TourMenu_ClipGoto(0, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 4) {
            TourMenu_ClipGoto(0, 1, "fl_off_start");
            TM_CUR++;
            if (TM_CUR >= TOUR_NUM) {
                TM_CUR = 0;
            }
            TourMenu_ClipGoto(0, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            switch (gTourMenu->cursor[TOURMENU_LV_TOP]) {
            case 0:
                /* only the open tournament can be entered for prizes */
                if (gSaveData->unkA08 & (s32)(1U << gTourMenu->cursor[TOURMENU_LV_TOUR])) {
                    Flash_GotoLabel(&gTourMenu->flash[0], "fl_taikai_setumei_in", 1);
                    TourMenu_ClipGoto(0, 1, "fl_ok");
                    TourMenu_ClipGoto(0, 2, "fl_on_start");
                    gTourMenu->seq = gTourMenu->cursor[TOURMENU_LV_TOUR] * 50 + 151;
                    gTourMenu->level = TOURMENU_LV_INFO;
                    Snd_PlaySe(1, 1);
                } else {
                    gTourMenu->voiceLine = 0x1C;
                    Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
                    Snd_PlaySe(1, 7);
                }
                break;
            case 1:
                Flash_GotoLabel(&gTourMenu->flash[0], "fl_taikai_setumei_in", 1);
                TourMenu_ClipGoto(0, 1, "fl_ok");
                TourMenu_ClipGoto(0, 2, "fl_on_start");
                gTourMenu->seq = gTourMenu->cursor[TOURMENU_LV_TOUR] * 50 + 151;
                gTourMenu->level = TOURMENU_LV_INFO;
                Snd_PlaySe(1, 1);
                break;
            }
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_taikaisentaku_cancel", 1);
            TourMenu_ClipGoto(0, 1, "fl_off_start");
            TourMenu_ClipGoto(0, 0, "fl_on_start");
            gTourMenu->level = TOURMENU_LV_TOP;
            Snd_PlaySe(1, 2);
        }
        break;
    case TOURMENU_LV_INFO:
        if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_nanidoi_in", 1);
            gTourMenu->voiceLine = 0x1D;
            if (gTourMenu->cursor[TOURMENU_LV_TOP] != 1) {
                /* the open tournament has its level fixed */
                gTourMenu->voiceLine++;
                gTourMenu->cursor[TOURMENU_LV_LEVEL] = gSaveData->unkA10;
            }
            TourMenu_ClipGoto(0, 3, "fl_on_start");
            Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
            gTourMenu->level = TOURMENU_LV_LEVEL;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_taikai_setumei_cancel", 1);
            TourMenu_ClipGoto(0, 2, "fl_off_start");
            gTourMenu->level = TOURMENU_LV_TOUR;
            Snd_PlaySe(1, 2);
        }
        break;
    case TOURMENU_LV_LEVEL:
        if ((gPad[0].gameRepeat & 1) && gTourMenu->cursor[TOURMENU_LV_TOP] == 1) {
            TourMenu_ClipGoto(0, 3, "fl_off_start");
            TM_CUR--;
            if (TM_CUR < 0) {
                TM_CUR = 2;
            }
            TourMenu_ClipGoto(0, 3, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameRepeat & 2) && gTourMenu->cursor[TOURMENU_LV_TOP] == 1) {
            TourMenu_ClipGoto(0, 3, "fl_off_start");
            TM_CUR++;
            if (TM_CUR >= 3) {
                TM_CUR = 0;
            }
            TourMenu_ClipGoto(0, 3, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_ninzu_in", 1);
            gTourMenu->voiceLine = 0x1F;
            if (gTourMenu->cursor[TOURMENU_LV_TOP] != 1) {
                /* and a single entrant */
                gTourMenu->voiceLine++;
                gTourMenu->cursor[TOURMENU_LV_NUM] = 0;
            }
            TourMenu_ClipGoto(0, 3, "fl_ok");
            Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
            gTourMenu->level = TOURMENU_LV_NUM;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_nanidoi_cancel", 1);
            TourMenu_ClipGoto(0, 3, "fl_off_start");
            gTourMenu->seq = gTourMenu->cursor[TOURMENU_LV_TOUR] * 50 + 151;
            gTourMenu->level = TOURMENU_LV_INFO;
            Snd_PlaySe(1, 2);
        }
        break;
    case TOURMENU_LV_NUM:
        if ((gPad[0].gameRepeat & 1) && gTourMenu->cursor[TOURMENU_LV_TOP] == 1) {
            TM_CUR--;
            if (TM_CUR < 0) {
                TM_CUR = ESEL_ENTRY_MAX - 1;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameRepeat & 2) && gTourMenu->cursor[TOURMENU_LV_TOP] == 1) {
            TM_CUR++;
            if (TM_CUR >= ESEL_ENTRY_MAX) {
                TM_CUR = 0;
            }
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq = 451;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_ninzu_cancel", 1);
            gTourMenu->voiceLine = 0x1D;
            if (gTourMenu->cursor[TOURMENU_LV_TOP] != 1) {
                gTourMenu->voiceLine++;
            }
            Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, gTourMenu->voiceLine);
            gTourMenu->level = TOURMENU_LV_LEVEL;
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

#define TM_SAY(n) Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, n)

/* Steps the guide's scripted speech: `seq` is the step, 0 = idle (the menu takes input). */
void TourMenu_UpdateSeq(void) {
    if (gTourMenu->seq == 0) {
        return;
    }
    switch (gTourMenu->seq) {
    /* 1..7: first visit, three lines */
    case 1:
        gTourMenu->voiceLine = 0;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 3:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 5:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 7:
        gSaveData->unkA08 |= TOUR_SAVE_STARTED;
        if (gTourMenu->flags & TOURMENU_INVITE) {
            gTourMenu->voiceLine = -1;
            Voice_StopWithLip();
            gTourMenu->seq = 0x33;
        } else {
            gTourMenu->voiceLine = 3;
            TM_SAY(gTourMenu->voiceLine);
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
            TourMenu_ClipGoto(0, 0, "fl_on_start");
            gTourMenu->seq = 0;
        }
        break;
    case 2:
    case 4:
    case 6:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    /* 0x33..0x39: an invitation arrived */
    case 0x33:
        Flash_Play(&gTourMenu->flash[1], 1);
        Flash_GotoLabel(&gTourMenu->flash[1], "fl_taikai_syotai_in", 1);
        gTourMenu->seq++;
        break;
    case 0x34:
        if (gTourMenu->flash[1].trig & 1) {
            gTourMenu->seq = 0x35;
        }
        break;
    case 0x35:
        gTourMenu->voiceLine = 4;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x36:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x37:
        Flash_GotoLabel(&gTourMenu->flash[1], "fl_taikai_syotai_out", 1);
        gTourMenu->seq++;
        break;
    case 0x38:
        if (gTourMenu->flash[1].trig & 1) {
            gTourMenu->seq++;
        }
        break;
    case 0x39:
        if (!(gSaveData->unkA08 & TOUR_SAVE_EXPLAINED)) {
            gTourMenu->seq = 0x65;
        } else {
            gTourMenu->voiceLine = 3;
            TM_SAY(gTourMenu->voiceLine);
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
            TourMenu_ClipGoto(0, 0, "fl_on_start");
            gTourMenu->seq = 0;
        }
        break;
    /* 0x65..0x69: the long explanation, once */
    case 0x65:
        gTourMenu->voiceLine = 5;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x67:
        gTourMenu->voiceLine = 9;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x66:
    case 0x68:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x69:
        gSaveData->unkA08 |= TOUR_SAVE_EXPLAINED;
        gTourMenu->voiceLine = 3;
        TM_SAY(gTourMenu->voiceLine);
        Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
        TourMenu_ClipGoto(0, 0, "fl_on_start");
        gTourMenu->seq = 0;
        break;
    /* 0x97.., 0xC9.., 0xFB.., 0x12D.., 0x15F..: the description of one tournament, two lines; cancel leaves */
    case 0x97:
        gTourMenu->voiceLine = 0x12;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x98:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0x99:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xC9:
        gTourMenu->voiceLine = 0x14;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xCA:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0xCB:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xFB:
        gTourMenu->voiceLine = 0x16;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xFC:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0xFD:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x12D:
        gTourMenu->voiceLine = 0x18;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x12E:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0x12F:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x15F:
        gTourMenu->voiceLine = 0x1A;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x160:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0x161:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    /* the description was cancelled: back to the tournament list */
    case 0x191:
        gTourMenu->seq = 0;
        gTourMenu->voiceLine = -1;
        Voice_StopWithLip();
        Flash_GotoLabel(&gTourMenu->flash[0], "fl_taikai_setumei_cancel", 1);
        TourMenu_ClipGoto(0, 2, "fl_off_start");
        gTourMenu->level = TOURMENU_LV_TOUR;
        break;
    /* 0x1C3..0x1C5: everything is chosen: the guide's send-off, then leave */
    case 0x1C3:
        gTourMenu->voiceLine = 0x21;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x1C4:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x1C5:
        gTourMenu->seq = 0;
        gTourMenu->flags |= TOURMENU_DONE;
        gTourMenu->flags |= TOURMENU_LEAVING;
        gTourMenu->leaveTimer = 0xF;
        break;
    }
}

/* Runs the tournament menu until it fades out; stores the choices in gProgress. Returns what TourMenu_Input left in
 * `result` (1 unless the menu was cancelled). */
s32 TourMenu_Run(s32 section) {
    s32 result = 1;

    TourMenu_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            TourMenu_Update();
            TourMenu_UpdateSeq();
        }
        TourMenu_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (!(gTourMenu->flags & TOURMENU_GREETED) && (gTourMenu->flash[0].flags & MFLASH_PAD)) {
            gTourMenu->flags |= TOURMENU_GREETED;
            if (!(gSaveData->unkA08 & TOUR_SAVE_STARTED)) {
                gTourMenu->seq = 1;
            } else if (gTourMenu->flags & TOURMENU_INVITE) {
                gTourMenu->seq = 0x33;
            } else {
                gTourMenu->voiceLine = 3;
                TM_SAY(gTourMenu->voiceLine);
                Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
                TourMenu_ClipGoto(0, 0, "fl_on_start");
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            StreamSe_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gTourMenu->flags & TOURMENU_LEAVING) {
            if (--gTourMenu->leaveTimer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                TOUR_PROG->t.entry = gTourMenu->cursor[TOURMENU_LV_TOP];
                TOUR_PROG->t.tour = gTourMenu->cursor[TOURMENU_LV_TOUR];
                TOUR_PROG->t.level = gTourMenu->cursor[TOURMENU_LV_LEVEL];
                TOUR_PROG->t.entryNum = gTourMenu->cursor[TOURMENU_LV_NUM] + 1;
            }
        } else if (gTourMenu->seq == 0) {
            TourMenu_Input(&result);
        }
    }
    TourMenu_Term();
    Dma_ResetBuffers();
    return result;
}
