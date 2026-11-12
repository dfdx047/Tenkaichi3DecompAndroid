#include "common.h"
#include "menu/menu_s.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work area (.data, 0x3B7440). */
SurvResult *gSurvResult = NULL;

/*
 * SurvResult, 0x38A308..0x38C360: the result screen of a survival run (mode 19 of Ub_Main), a whole object.
 * Work pointer gSurvResult (0x3B7440); read-only data 0x3BA300..0x3BA738 (strings and the three jump tables
 * 0x3BA610, 0x3BA700, 0x3BA720). A sibling of the mission result screen (MisResult, src/menu/menu_p_b.c) and of
 * SimResult (src/menu/menu_r_c.c): the same score sheet (UbScore), pages of bonus plates, pay-out and reward item.
 */

#define SR_RES(n) \
    res = (MTexRes *)MPACK_AT(gSurvResult->res, n); \
    Res_RelocateOffsets(&res, res, res)

/*
 * Loads and unpacks the screen (section `section` of archive 3), builds its movie, fills the score sheet from
 * the battle result and writes the record and the reward item into the save.
 */
void SurvResult_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gSurvResult = Heap_Alloc(sizeof(SurvResult), 0x20, 0, 2);
    memset(gSurvResult, 0, sizeof(SurvResult));
    gSurvResult->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gSurvResult->res = Sprite_Unpack(gSurvResult->pack, NULL, NULL);
    SR_RES(5);
    gSurvResult->bg = res;
    SR_RES(7);
    gSurvResult->tex[37] = MTEX(res, 0);
    gSurvResult->tex[38] = MTEX(res, 1);
    gSurvResult->tex[39] = MTEX(res, 3);
    SR_RES(4);
    gSurvResult->tex[0] = MTEX(res, 0);
    gSurvResult->tex[1] = MTEX(res, 3);
    gSurvResult->tex[28] = MTEX(res, 6);
    gSurvResult->tex[19] = MTEX(res, 7);
    gSurvResult->tex[16] = MTEX(res, 8);
    gSurvResult->tex[36] = MTEX(res, 12);
    gSurvResult->tex[33] = MTEX(res, 13);
    gSurvResult->tex[34] = MTEX(res, 14);
    gSurvResult->tex[31] = MTEX(res, 15);
    gSurvResult->tex[30] = MTEX(res, 16);
    gSurvResult->tex[32] = MTEX(res, 17);
    gSurvResult->tex[21] = MTEX(res, 18);
    gSurvResult->tex[15] = MTEX(res, 19);
    gSurvResult->tex[24] = MTEX(res, 20);
    gSurvResult->tex[25] = MTEX(res, 21);
    gSurvResult->tex[18] = MTEX(res, 22);
    gSurvResult->tex[27] = MTEX(res, 23);
    gSurvResult->tex[14] = MTEX(res, 25);
    gSurvResult->tex[22] = MTEX(res, 26);
    gSurvResult->tex[3] = MTEX(res, 27);
    gSurvResult->tex[2] = MTEX(res, 28);
    gSurvResult->tex[17] = MTEX(res, 29);
    gSurvResult->tex[26] = MTEX(res, 30);
    gSurvResult->tex[4] = MTEX(res, 31);
    gSurvResult->tex[5] = MTEX(res, 32);
    gSurvResult->tex[9] = MTEX(res, 33);
    gSurvResult->tex[13] = MTEX(res, 34);
    gSurvResult->tex[12] = MTEX(res, 35);
    gSurvResult->tex[6] = MTEX(res, 36);
    gSurvResult->tex[8] = MTEX(res, 37);
    gSurvResult->tex[7] = MTEX(res, 38);
    gSurvResult->tex[23] = MTEX(res, 39);
    gSurvResult->tex[29] = MTEX(res, 43);
    gSurvResult->tex[40] = MTEX(res, 45);
    gSurvResult->tex[41] = MTEX(res, 46);
    gSurvResult->tex[42] = MTEX(res, 47);
    gSurvResult->tex[10] = NULL;
    gSurvResult->tex[11] = NULL;
    gSurvResult->tex[20] = NULL;
    gSurvResult->tex[35] = NULL;
    Flash_Create(&gSurvResult->flash[0], MPACK_AT(gSurvResult->res, 2), gSurvResult->tex);
    Flash_Play(&gSurvResult->flash[0], 1);
    gSurvResult->blink = Rand_Libc() % 32;
    SR_RES(8);
    IconWin_Init(MPACK_AT(gSurvResult->res, 9), res);
    IconWin_Open();
    IconWin_SetIcon(2);
    gSurvResult->text = MPACK_AT(gSurvResult->res, 12);
    gSurvResult->subtitles = MPACK_AT(gSurvResult->res, 11);
    gSurvResult->itemText = MPACK_AT(gSurvResult->res, 14);
    for (i = 0; i < 6; i++) {
        TextBox_Init(&gSurvResult->box[i], gSurvResult->text, 0);
        if (i == 5) {
            TextBox_SetUnk50(&gSurvResult->box[5], 1);
        } else {
            TextBox_SetUnk50(&gSurvResult->box[i], 0);
        }
    }
    TextBox_Init(&gSurvResult->itemBox, gSurvResult->itemText, 0);
    TextBox_SetUnk50(&gSurvResult->itemBox, 0);
    TextBox_SetLineOffsets(&gSurvResult->itemBox, 0xC, 0, 0, 0, 0);
    gSurvResult->price = MPACK_AT(gSurvResult->res, 13);
    gSurvResult->voiceLine = -1;
    gSurvResult->page = 0;
    gSurvResult->timer = 15;
    gSurvResult->pageDone = 0;
    gSurvResult->counting = 0;
    gSurvResult->step = 0;
    gSurvResult->gotItem = 0;
    gSurvResult->outcome = UbScore_Fill(2, &gSurvResult->score, &gSurvResult->pages);
    UbScore_CalcPoints(gSurvResult->price, &gSurvResult->score);
    gSurvResult->newRecord =
        UbScore_SaveBestB(S_PROG->cursor, UbScore_CalcRank(2, &gSurvResult->score), &gSurvResult->score);
    if (gSurvResult->score.line[3].value >= 10 && !(S_SAVE->ubFlags & S_UBFLAG_SURV_ITEM)) {
        S_SAVE->ubFlags |= S_UBFLAG_SURV_ITEM;
        gSurvResult->gotItem = 1;
        Save_AddItem(UbScore_GetRewardItem(3));
    }
    gSurvResult->voiceSkip = 0;
}

/* Frees the screen. */
void SurvResult_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < SURVRESULT_FLASH_NUM; i++) {
        Flash_Destroy(&gSurvResult->flash[i]);
    }
    if (gSurvResult->res != NULL) {
        Heap_Free(gSurvResult->res);
        gSurvResult->res = NULL;
    }
    if (gSurvResult != NULL) {
        Heap_Free(gSurvResult);
        gSurvResult = NULL;
    }
}

/* Draws the score lines, the page of bonus plates, the totals, the rank and the reward item. */
void SurvResult_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;

#ifdef PORT
    Port_MenuPage(36); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gSurvResult->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gSurvResult->bg, 0, 0, 0x80);
#endif
    flash = &gSurvResult->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gSurvResult->blink, 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gSurvResult->mouth, 0);
    if (gSurvResult->outcome == 0) {
        Flash_FindLabel(flash, "mc_window_plate01", "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, 0x141, &gSurvResult->box[3]);
    } else {
        Flash_FindLabel(flash, "mc_window_plate01", "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, 0x142, &gSurvResult->box[3]);
    }
    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x200;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, NULL, "mc_win_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, NULL, "mc_win_text03", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x20;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_yajirusi_icon_up", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gSurvResult->page == 0) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }
    uv.x0 = 0x20;
    uv.y0 = 0x20;
    uv.x1 = 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_yajirusi_icon_down", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gSurvResult->score.bonusCount != 0 && gSurvResult->page < gSurvResult->pages) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    for (i = 0; i < gSurvResult->score.lineCount; i++) {
        sprintf(name, "mc_point_plate%02d", i + 1);
        Num_DrawChild(flash, name, "mc_percent_suji%02d", 0, 5, gSurvResult->score.line[i].value, 0x20, 0x20, 0, 0);
        Num_DrawChild(flash, name, "mc_pt_suji%02d", 0, 6, gSurvResult->score.line[i].points * 100, 0x20, 0x20, 0, 0);
    }
    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_bonus_plate%02d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (gSurvResult->page * 3 + i - 3 >= gSurvResult->score.bonusCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_bonus_karisize", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0,
                               gSurvResult->score.bonus[gSurvResult->score.shown[i]].value + 0x20,
                               &gSurvResult->box[i]);
            sprintf(name, "mc_bonus_plate%02d", i + 1);
            Num_DrawChild(flash, name, "mc_point_suji%02d", 0, 5,
                          gSurvResult->score.bonus[gSurvResult->score.shown[i]].points * 100, 0x20, 0x20, 0, 0);
        }
    }
    for (i = 1; i < 4; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_point_plate%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_bonus_text", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_pt_percent", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    Num_Draw(flash, "mc_time_suji%02d", 4, 2, gSurvResult->score.hours, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 2, 2, gSurvResult->score.minutes, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 0, 2, gSurvResult->score.seconds, 0x20, 0x20, 1);
    uv.x0 = 0;
    uv.y0 = 0x80;
    uv.x1 = 0x100;
    uv.y1 = 0xA0;
    Flash_FindLabel(flash, NULL, "mc_bonus_title", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = 0;
    uv.y0 = 0x80;
    uv.x1 = 0x200;
    uv.y1 = 0xC0;
    Flash_FindLabel(flash, NULL, "mc_win_text04", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_henkan_suji%02d", 0, 6, gSurvResult->score.convert, 0x20, 0x20, 0);
    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x100;
    uv.y1 = 0x60;
    Flash_FindLabel(flash, NULL, "mc_time_text03", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_zpoint_suji%02d", 0, 9, S_SAVE->money, 0x20, 0x20, 0);
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_time_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_score_suji%02d", 0, 9, gSurvResult->score.total * 100, 0x20, 0x20, 0);
    uv.x0 = gSurvResult->score.rank * 0x40;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_rankmoji_anime", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, NULL, "mc_newrecord_text", &ref);
    if (gSurvResult->newRecord) {
        uv.x0 = 0;
        uv.y0 = 0x40;
        uv.x1 = 0x100;
        uv.y1 = 0x80;
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    if (gSurvResult->gotItem) {
        s32 item = UbScore_GetRewardItem(3);

        Flash_FindLabel(flash, "mc_item_plate00", "mc_item_text_kari", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, item, &gSurvResult->itemBox);
    }
    for (i = 0; i < SURVRESULT_FLASH_NUM; i++) {
        Flash_Draw(&gSurvResult->flash[i]);
    }
    IconWin_Draw();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Runs the leave timer and the movie, and acts on the movie's triggers. */
void SurvResult_Update(void) {
    MFlashRef ref;
    MFlash *flash = &gSurvResult->flash[0];
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSurvResult->timer > 0) {
        gSurvResult->timer--;
    }
    for (i = 0; i < SURVRESULT_FLASH_NUM; i++) {
        Flash_Advance(&gSurvResult->flash[i]);
    }
    if (gSurvResult->flash[0].trig & 1) {
        if (gSurvResult->state == SURVRESULT_ST_INTRO) {
            gSurvResult->started = 1;
        }
    }
    if (gSurvResult->flash[0].trig & 4) {
        if (gSurvResult->state == SURVRESULT_ST_COUNT) {
            gSurvResult->counting = 1;
        }
        UbScore_SetPage(&gSurvResult->score, gSurvResult->page);
    }
    if (gSurvResult->flash[0].trig & 8) {
        switch (gSurvResult->state) {
        case SURVRESULT_ST_INTRO:
            if (gSurvResult->outcome == 0) {
                Flash_FindLabel(flash, NULL, "mc_win_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
                gSurvResult->talk = 1;
            } else {
                Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
                gSurvResult->talk = 6;
            }
            break;
        case SURVRESULT_ST_MONEY:
            Flash_GotoLabel(flash, "fl_zpoint_in", 1);
            break;
        case SURVRESULT_ST_ITEM:
            Flash_GotoLabel(flash, "fl_itemget_in", 1);
            break;
        }
    }
    if (gSurvResult->pageDone && gSurvResult->timer == 0) {
        if (gSurvResult->page < gSurvResult->pages) {
            if (gSurvResult->page == 0) {
                Flash_GotoLabel(flash, "fl_battlebonus_in", 1);
            } else {
                Flash_GotoLabel(flash, "fl_battlebonus_out_in", 1);
            }
            gSurvResult->page++;
        } else {
            if (gSurvResult->page == 0) {
                Flash_GotoLabel(flash, "fl_hyouka_rank_in", 1);
            } else {
                Flash_GotoLabel(flash, "fl_rank_in", 1);
            }
            Snd_PlaySe(2, 0x13);
            if (gSurvResult->score.rank == 4) {
                gSurvResult->talk = 11;
            } else if (Rand_Range(2)) {
                gSurvResult->talk = 12;
            } else {
                gSurvResult->talk = 13;
            }
        }
        gSurvResult->pageDone = 0;
    }
}

/* The guide's script: starts the line of the current step; confirm cuts the running line short. */
void SurvResult_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSurvResult->voiceSkip == 0) {
        if (gSurvResult->talk == 0) {
            return;
        }
        if (gSurvResult->voiceLine != -1 && Voice_GetStat(0) != S_VOICE_IDLE &&
            gSurvResult->talk == gSurvResult->talkLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                gSurvResult->voiceSkip = 1;
            }
            return;
        }
    } else {
        gSurvResult->voiceSkip = 0;
    }
    switch (gSurvResult->talk) {
    case 1:
        gSurvResult->voiceLine = 0x38;
        gSurvResult->talk++;
        break;
    case 2:
        gSurvResult->talk = 0;
        gSurvResult->voiceLine = -1;
        break;
    case 6:
        gSurvResult->voiceLine = 0x39;
        gSurvResult->talk++;
        break;
    case 7:
        gSurvResult->talk = 0;
        gSurvResult->voiceLine = -1;
        break;
    case 11:
        gSurvResult->voiceLine = 0x3A;
        break;
    case 12:
        gSurvResult->voiceLine = 0x3B;
        break;
    case 13:
        gSurvResult->voiceLine = 0x3C;
        break;
    case 14:
        gSurvResult->voiceLine = 0x3D;
        break;
    case 15:
        gSurvResult->voiceLine = 0x3F;
        break;
    }
    if (gSurvResult->talk >= 11) {
        gSurvResult->talk = 0;
    }
    if (gSurvResult->voiceLine == -1) {
        return;
    }
    Voice_PlayWithSubtitle(gSurvResult->subtitles, S_VOICE_BASE, gSurvResult->voiceLine);
    gSurvResult->talkLast = gSurvResult->talk;
}

/*
 * Pad 0, by state: the lines count up (confirm finishes them at once), the pages are turned with up / down,
 * the total is paid out as Z points (confirm skips the animation), the reward window is closed.
 */
void SurvResult_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;

    if (gSurvResult->state == SURVRESULT_ST_INTRO && gSurvResult->started) {
        if (gSurvResult->outcome == 2) {
            gSurvResult->flags |= SURVRESULT_DONE;
            gSurvResult->flags |= SURVRESULT_LEAVING;
            gSurvResult->timer = 15;
            gSurvResult->state = SURVRESULT_ST_LEAVE;
        } else {
            Flash_GotoLabel(&gSurvResult->flash[0], "fl_hyouka_menu_in", 1);
            gSurvResult->state = SURVRESULT_ST_COUNT;
        }
        gSurvResult->started = 0;
    }
    if (!(gSurvResult->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (!(gSurvResult->flags & SURVRESULT_STARTED)) {
        gSurvResult->flags |= SURVRESULT_STARTED;
    }
    switch (gSurvResult->state) {
    case SURVRESULT_ST_COUNT:
        if (gSurvResult->counting) {
            s32 isBonus = gSurvResult->page != 0;
            s32 idx = gSurvResult->step - isBonus * gSurvResult->score.lineCount;

            if (UbScore_CountLine(&gSurvResult->score, isBonus, idx, 15)) {
                gSurvResult->step++;
                if ((isBonus && (idx + 1) % 3 == 0) || (!isBonus && gSurvResult->step == gSurvResult->score.lineCount) ||
                    gSurvResult->step == gSurvResult->score.bonusCount + gSurvResult->score.lineCount) {
                    gSurvResult->counting = 0;
                    gSurvResult->pageDone = 1;
                }
            }
        }
        if (confirm) {
            s32 i;

            for (i = 0; i < gSurvResult->score.bonusCount + gSurvResult->score.lineCount; i++) {
                s32 idx = 0;
                s32 isBonus = 0;

                if (i < gSurvResult->score.lineCount) {
                    isBonus = 0;
                    idx = i;
                } else {
                    idx = i - gSurvResult->score.lineCount;
                    isBonus = 1;
                }
                while (!UbScore_CountLine(&gSurvResult->score, isBonus, idx, 9999)) {
                }
            }
            gSurvResult->pageDone = 1;
            gSurvResult->page = gSurvResult->pages;
            gSurvResult->step = gSurvResult->score.bonusCount + gSurvResult->score.lineCount;
            gSurvResult->page = gSurvResult->pages;
            UbScore_SetPage(&gSurvResult->score, gSurvResult->page);
        }
        if (gSurvResult->pageDone) {
            if (gSurvResult->page < gSurvResult->pages) {
                if (gSurvResult->page == 0) {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_battlebonus_in", 1);
                } else {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_battlebonus_out_in", 1);
                }
                gSurvResult->page++;
            } else {
                if (gSurvResult->page == 0) {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_hyouka_rank_in", 1);
                } else {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_rank_in", 1);
                }
                if (gSurvResult->score.line[3].value >= 50) {
                    gSurvResult->talk = 15;
                } else if (gSurvResult->score.rank == 4) {
                    gSurvResult->talk = 11;
                } else if (Rand_Range(2)) {
                    gSurvResult->talk = 12;
                } else {
                    gSurvResult->talk = 13;
                }
            }
            gSurvResult->pageDone = 0;
        }
        if (gSurvResult->step == gSurvResult->score.bonusCount + gSurvResult->score.lineCount) {
            gSurvResult->state = SURVRESULT_ST_PAGES;
        }
        break;
    case SURVRESULT_ST_PAGES:
        if (up) {
            if (gSurvResult->page > 0) {
                gSurvResult->page--;
                if (gSurvResult->page != 0) {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_battle_cansel_in", 1);
                } else {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_battlebonus_cancel", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (down) {
            if (gSurvResult->page < gSurvResult->pages) {
                gSurvResult->page++;
                if (gSurvResult->page == 0) {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_hyouka_cansel", 1);
                } else {
                    Flash_GotoLabel(&gSurvResult->flash[0], "fl_battle_cansel_in", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (confirm) {
            gSurvResult->state = SURVRESULT_ST_MONEY;
            gSurvResult->step = 0;
            gSurvResult->timer = 60;
            if (gSurvResult->page == 0) {
                Flash_GotoLabel(&gSurvResult->flash[0], "fl_hyoukakarazpoint_trig", 1);
            } else {
                Flash_GotoLabel(&gSurvResult->flash[0], "fl_zpoint_trig", 1);
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case SURVRESULT_ST_MONEY:
        switch (gSurvResult->step) {
        case 0:
            if (gSurvResult->timer == 0) {
                gSurvResult->step++;
                UbScore_PlateGoto(&gSurvResult->flash[0], 0, 1);
                gSurvResult->score.count = gSurvResult->score.total;
                gSurvResult->talk = 14;
            }
            break;
        case 1:
            if (UbScore_ConvertStep(&gSurvResult->score, 0x40)) {
                gSurvResult->step++;
                gSurvResult->timer = 15;
                UbScore_PlateGoto(&gSurvResult->flash[0], 0, 0);
                gSurvResult->score.convert = gSurvResult->score.count;
            }
            break;
        case 2:
            if (gSurvResult->timer == 0) {
                gSurvResult->step++;
                UbScore_PlateGoto(&gSurvResult->flash[0], 1, 1);
            }
            break;
        case 3:
            if (UbScore_Transfer(&gSurvResult->score.convert, &S_SAVE->money, 0x40, 1.0f)) {
                gSurvResult->step++;
                UbScore_PlateGoto(&gSurvResult->flash[0], 1, 0);
            }
            if (S_SAVE->money > S_MONEY_MAX) {
                S_SAVE->money = S_MONEY_MAX;
            }
            break;
        case 4:
            if (confirm) {
                Flash_GotoLabel(&gSurvResult->flash[0], "fl_zpoint_out", 1);
                if (gSurvResult->gotItem) {
                    gSurvResult->state = SURVRESULT_ST_ITEM;
                    Snd_PlaySe(2, 6);
                } else {
                    gSurvResult->flags |= SURVRESULT_DONE;
                    gSurvResult->flags |= SURVRESULT_LEAVING;
                    gSurvResult->timer = 15;
                    Snd_PlaySe(1, 1);
                    gSurvResult->state = SURVRESULT_ST_LEAVE;
                }
            }
            break;
        }
        if (gSurvResult->step >= 1 && gSurvResult->step <= 3 && confirm) {
            if (gSurvResult->step < 2) {
                while (!UbScore_ConvertStep(&gSurvResult->score, S_MONEY_MAX)) {
                }
                gSurvResult->score.convert = gSurvResult->score.count;
                while (!UbScore_Transfer(&gSurvResult->score.convert, &S_SAVE->money, S_MONEY_MAX, 1.0f)) {
                }
            } else if (gSurvResult->step < 4) {
                while (!UbScore_Transfer(&gSurvResult->score.convert, &S_SAVE->money, S_MONEY_MAX, 1.0f)) {
                }
            }
            if (S_SAVE->money > S_MONEY_MAX) {
                S_SAVE->money = S_MONEY_MAX;
            }
            UbScore_PlateGoto(&gSurvResult->flash[0], 0, 0);
            UbScore_PlateGoto(&gSurvResult->flash[0], 1, 0);
            gSurvResult->step = 4;
        }
        break;
    case SURVRESULT_ST_ITEM:
        if (confirm) {
            Flash_GotoLabel(&gSurvResult->flash[0], "fl_itemget_out", 1);
            gSurvResult->flags |= SURVRESULT_DONE;
            gSurvResult->flags |= SURVRESULT_LEAVING;
            gSurvResult->timer = 15;
            gSurvResult->state = SURVRESULT_ST_LEAVE;
            Snd_PlaySe(1, 1);
        }
        break;
    case SURVRESULT_ST_LEAVE:
        break;
    }
}

/* The survival result screen (mode 19). Always returns 1; Ub_Main goes back to the course select. */
s32 SurvResult_Run(s32 section) {
    s32 result = 1;

    SurvResult_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        SurvResult_Update();
        SurvResult_UpdateVoice();
        SurvResult_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gSurvResult->flags & SURVRESULT_GREETED) && (gSurvResult->flash[0].flags & MFLASH_PAD)) {
                gSurvResult->flags |= SURVRESULT_GREETED;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            Voice_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gSurvResult->flags & SURVRESULT_LEAVING) {
            if (gSurvResult->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gSurvResult->talk == 0) {
            SurvResult_Input(&result);
        }
    }
    SurvResult_Term();
    Dma_ResetBuffers();
    return result;
}
