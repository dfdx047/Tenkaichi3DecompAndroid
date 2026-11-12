#include "common.h"
#include "menu/menu_r.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work area (.data, 0x3B741C). */
SimResult *gSimResult = NULL;

/*
 * SimResult, 0x3851E8..0x387880: the result screen of the "sim" ladder (mode 23, shown after a round's fight). A
 * whole object: work pointer 0x3B741C (.data), .rodata 0x3B98F0..0x3B9EC4 (its first string is the first one this
 * file emits, its last jump table is that of SimResult_Input's money switch; the music table of SimResult_Init is
 * at 0x3B9908). The score sheet is the UbScore of the previous chunks (menu_p_d.c, menu_q.c).
 */

/* Sends the yes / no row under the cursor to `label`. */
void SimResult_YesNoGoto(char *label) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = &gSimResult->flash[0];

    sprintf(name, "mc_yes_no_text%02d", gSimResult->pick[SIMRESULT_ST_CONTINUE] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * Loads the screen's section of archive 3, builds the score sheet from the battle result and starts the music of
 * the round. On the result screen of the ladder's last fight the reward item goes into the save at once, whatever
 * the outcome of the fight (once per save: SimSave.simCleared).
 */
void SimResult_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gSimResult = Heap_Alloc(sizeof(SimResult), 0x20, 0, 2);
    memset(gSimResult, 0, sizeof(SimResult));
    gSimResult->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gSimResult->res = Sprite_Unpack(gSimResult->pack, NULL, NULL);

    res = (MTexRes *)MPACK_AT(gSimResult->res, 6);
    Res_RelocateOffsets(&res, res, res);
    gSimResult->bg = res;

    res = (MTexRes *)MPACK_AT(gSimResult->res, 7);
    Res_RelocateOffsets(&res, res, res);
    gSimResult->tex[46] = MTEX(res, 5);
    gSimResult->tex[47] = MTEX(res, 6);
    gSimResult->tex[48] = MTEX(res, 8);

    res = (MTexRes *)MPACK_AT(gSimResult->res, 4);
    Res_RelocateOffsets(&res, res, res);
    gSimResult->tex[0] = MTEX(res, 1);
    gSimResult->tex[3] = MTEX(res, 2);
    gSimResult->tex[2] = MTEX(res, 4);
    gSimResult->tex[1] = MTEX(res, 5);
    gSimResult->tex[29] = MTEX(res, 6);
    gSimResult->tex[21] = MTEX(res, 7);
    gSimResult->tex[18] = MTEX(res, 8);
    gSimResult->tex[39] = MTEX(res, 9);
    gSimResult->tex[38] = MTEX(res, 10);
    gSimResult->tex[40] = MTEX(res, 11);
    gSimResult->tex[37] = MTEX(res, 12);
    gSimResult->tex[34] = MTEX(res, 13);
    gSimResult->tex[35] = MTEX(res, 14);
    gSimResult->tex[32] = MTEX(res, 15);
    gSimResult->tex[31] = MTEX(res, 16);
    gSimResult->tex[33] = MTEX(res, 17);
    gSimResult->tex[17] = MTEX(res, 19);
    gSimResult->tex[25] = MTEX(res, 20);
    gSimResult->tex[26] = MTEX(res, 21);
    gSimResult->tex[20] = MTEX(res, 22);
    gSimResult->tex[28] = MTEX(res, 23);
    gSimResult->tex[16] = MTEX(res, 25);
    gSimResult->tex[23] = MTEX(res, 26);
    gSimResult->tex[5] = MTEX(res, 27);
    gSimResult->tex[4] = MTEX(res, 28);
    gSimResult->tex[19] = MTEX(res, 29);
    gSimResult->tex[27] = MTEX(res, 30);
    gSimResult->tex[6] = MTEX(res, 31);
    gSimResult->tex[7] = MTEX(res, 32);
    gSimResult->tex[11] = MTEX(res, 33);
    gSimResult->tex[15] = MTEX(res, 34);
    gSimResult->tex[14] = MTEX(res, 35);
    gSimResult->tex[8] = MTEX(res, 36);
    gSimResult->tex[10] = MTEX(res, 37);
    gSimResult->tex[9] = MTEX(res, 38);
    gSimResult->tex[24] = MTEX(res, 39);
    gSimResult->tex[43] = MTEX(res, 40);
    gSimResult->tex[44] = MTEX(res, 41);
    gSimResult->tex[45] = MTEX(res, 42);
    gSimResult->tex[30] = MTEX(res, 43);
    gSimResult->tex[12] = NULL;
    gSimResult->tex[13] = NULL;
    gSimResult->tex[22] = NULL;
    gSimResult->tex[36] = NULL;
    gSimResult->tex[41] = NULL;
    gSimResult->tex[42] = NULL;

    Flash_Create(&gSimResult->flash[0], MPACK_AT(gSimResult->res, 3), gSimResult->tex);
    Flash_Play(&gSimResult->flash[0], 1);
    gSimResult->blink = Rand_Libc() % 32;

    res = (MTexRes *)MPACK_AT(gSimResult->res, 8);
    Res_RelocateOffsets(&res, res, res);
    IconWin_Init(MPACK_AT(gSimResult->res, 9), res);
    IconWin_Open();
    IconWin_SetIcon(0);

    gSimResult->text = MPACK_AT(gSimResult->res, 12);
    gSimResult->subtitles = MPACK_AT(gSimResult->res, 11);
    gSimResult->itemText = MPACK_AT(gSimResult->res, 14);
    for (i = 0; i < 6; i++) {
        TextBox_Init(&gSimResult->box[i], gSimResult->text, 0);
        TextBox_SetUnk50(&gSimResult->box[i], 0);
    }
    TextBox_Init(&gSimResult->itemBox, gSimResult->itemText, 0);
    TextBox_SetUnk50(&gSimResult->itemBox, 0);
    TextBox_SetLineOffsets(&gSimResult->itemBox, 12, 0, 0, 0, 0);
    gSimResult->bonusTbl = MPACK_AT(gSimResult->res, 13);

    gSimResult->voiceLine = -1;
    gSimResult->page = 0;
    gSimResult->timer = 15;
    gSimResult->pageShown = 0;
    gSimResult->counting = 0;
    gSimResult->step = 0;
    gSimResult->skip = 0;
    gSimResult->gotItem = 0;
    gSimResult->outcome = UbScore_Fill(0, &gSimResult->score, &gSimResult->pageMax);
    UbScore_CalcPoints(gSimResult->bonusTbl, &gSimResult->score);
    UbScore_CalcRank(0, &gSimResult->score);
    if (SIM_PROG->run.turn >= SIM_LAST_TURN) {
        gSimResult->cleared = 1;
        if (!SIM_SAVE->simCleared) {
            SIM_SAVE->simCleared = 1;
            gSimResult->gotItem = 1;
            Save_AddItem(UbScore_GetRewardItem(0));
        }
    } else {
        gSimResult->cleared = 0;
    }
    gSimResult->score.total = SIM_PROG->run.stat[SIM_STAT_POINT];
    {
        s32 bgm[7] = { 4, 1, 2, 0, 6, 4, 8 };
        s32 span = 10;

        Bgm_Play(bgm[SIM_PROG->run.turn / span] + SIM_BGM_FIRST);
    }
}

/* Frees the screen. */
void SimResult_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < SIMRESULT_FLASH_NUM; i++) {
        Flash_Destroy(&gSimResult->flash[i]);
    }
    if (gSimResult->res != NULL) {
        Heap_Free(gSimResult->res);
        gSimResult->res = NULL;
    }
    if (gSimResult != NULL) {
        Heap_Free(gSimResult);
        gSimResult = NULL;
    }
}

/* Sets up the movie's clips (texts, numbers, texture rectangles) and draws it. */
void SimResult_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[0x40];
    MFlash *flash;
    s32 i;
    s32 x;

#ifdef PORT
    Port_MenuPage(33); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gSimResult->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gSimResult->bg, 0, 0, 0x80);
#endif
    flash = &gSimResult->flash[0];
    Flash_FindLabel(flash, "mc_guide_18go", "mc_guide_18go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gSimResult->blink, 0);
    Flash_FindLabel(flash, "mc_guide_18go", "mc_guide_18go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gSimResult->mouth, 0);

    if (gSimResult->outcome == 0) {
        Flash_FindLabel(flash, "mc_window_plate01", "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, 0x141, &gSimResult->box[3]);
        Flash_FindLabel(flash, NULL, "mc_day_text_kari", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, 0x141, &gSimResult->box[4]);
    } else {
        Flash_FindLabel(flash, "mc_window_plate01", "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, 0x142, &gSimResult->box[3]);
        Flash_FindLabel(flash, NULL, "mc_day_text_kari", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, 0x142, &gSimResult->box[4]);
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
    if (gSimResult->page == 0) {
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
    if (gSimResult->score.bonusCount != 0 && gSimResult->page < gSimResult->pageMax) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    for (i = 0; i < gSimResult->score.lineCount; i++) {
        sprintf(name, "mc_point_plate%02d", i + 1);
        Num_DrawChild(flash, name, "mc_percent_suji%02d", 0, 5, gSimResult->score.line[i].value, 0x20, 0x20, 0, 0);
        Num_DrawChild(flash, name, "mc_pt_suji%02d", 0, 6, gSimResult->score.line[i].points * 100, 0x20, 0x20, 0, 0);
    }

    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_bonus_plate%02d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (gSimResult->page * 3 + i - 3 >= gSimResult->score.bonusCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_bonus_karisize", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, gSimResult->score.bonus[gSimResult->score.shown[i]].value + 0x20,
                               &gSimResult->box[i]);
            sprintf(name, "mc_bonus_plate%02d", i + 1);
            Num_DrawChild(flash, name, "mc_point_suji%02d", 0, 5,
                          gSimResult->score.bonus[gSimResult->score.shown[i]].points * 100, 0x20, 0x20, 0, 0);
        }
    }

    for (i = 1; i < 4; i++) {
        uv.x0 = 0;
        uv.y0 = i << 5;
        uv.x1 = 0x100;
        uv.y1 = (i << 5) + 0x20;
        sprintf(name, "mc_point_plate%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_bonus_text", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_pt_percent", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    Num_Draw(flash, "mc_time_suji%02d", 4, 2, gSimResult->score.hours, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 2, 2, gSimResult->score.minutes, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 0, 2, gSimResult->score.seconds, 0x20, 0x20, 1);

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

    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    sprintf(name, "mc_yes_no_text%02d", 2);
    Flash_FindLabel(flash, name, "mc_yes_no_text_on", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, name, "mc_yes_no_text_off", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    Flash_FindLabel(flash, NULL, "mc_sim_text_kari", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, 0xF, &gSimResult->box[5]);

    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x200;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, NULL, "mc_clear_text01", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    uv.x0 = 0;
    uv.y0 = 0x80;
    uv.x1 = 0x200;
    uv.y1 = 0xC0;
    Flash_FindLabel(flash, NULL, "mc_clear_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x200;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, NULL, "mc_clear_text01", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    Num_Draw(flash, "mc_henkan_suji%02d", 0, 6, gSimResult->score.convert, 0x20, 0x20, 0);

    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x100;
    uv.y1 = 0x60;
    Flash_FindLabel(flash, NULL, "mc_time_text03", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    Num_Draw(flash, "mc_zpoint_suji%02d", 0, 9, SIM_SAVE->money, 0x20, 0x20, 0);

    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_time_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    Num_Draw(flash, "mc_score_suji%02d", 0, 9, gSimResult->score.total * 100, 0x20, 0x20, 0);

    x = gSimResult->score.rank << 6;
    uv.x0 = x;
    uv.y0 = 0;
    uv.x1 = x + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_rankmoji_anime", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    if (gSimResult->gotItem) {
        s32 line = UbScore_GetRewardItem(0);

        Flash_FindLabel(flash, "mc_item_plate00", "mc_item_text_kari", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, line, &gSimResult->itemBox);
    }
    for (i = 0; i < SIMRESULT_FLASH_NUM; i++) {
        Flash_Draw(&gSimResult->flash[i]);
    }
    IconWin_Draw();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Advances the movie and acts on its triggers: 1 = intro over, 4 = a page is in place, 8 = a plate may come in. */
void SimResult_Update(void) {
    MFlashRef ref;
    MFlash *flash;
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    flash = &gSimResult->flash[0];
    if (gSimResult->timer > 0) {
        gSimResult->timer--;
    }
    for (i = 0; i < SIMRESULT_FLASH_NUM; i++) {
        Flash_Advance(&gSimResult->flash[i]);
    }
    if (gSimResult->flash[0].trig & 1) {
        if (gSimResult->outcome == 2) {
            gSimResult->flags |= SIMRESULT_DONE;
            gSimResult->flags |= SIMRESULT_LEAVING;
            gSimResult->timer = 15;
            gSimResult->state = SIMRESULT_ST_END;
        } else if (gSimResult->state == SIMRESULT_ST_INTRO) {
            Flash_GotoLabel(flash, "fl_hyouka_menu_in", 1);
            gSimResult->state = SIMRESULT_ST_COUNT;
        }
    }
    if (gSimResult->flash[0].trig & 4) {
        if (gSimResult->state == SIMRESULT_ST_COUNT) {
            gSimResult->counting = 1;
        }
        UbScore_SetPage(&gSimResult->score, gSimResult->page);
    }
    if (gSimResult->flash[0].trig & 8) {
        switch (gSimResult->state) {
        case SIMRESULT_ST_INTRO:
            if (gSimResult->outcome == 0) {
                Flash_FindLabel(flash, NULL, "mc_win_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
            } else {
                Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
            }
            break;
        case SIMRESULT_ST_CONTINUE:
            Flash_GotoLabel(flash, "fl_zokkou_in", 1);
            SimResult_YesNoGoto("fl_on_start");
            break;
        case SIMRESULT_ST_OVER:
            Flash_GotoLabel(flash, "fl_gameover_in", 1);
            gSimResult->score.total = 0;
            break;
        case SIMRESULT_ST_TRAIN:
            Flash_GotoLabel(flash, "fl_syugyo_in", 1);
            break;
        case SIMRESULT_ST_MONEY:
            Flash_GotoLabel(flash, "fl_zpoint_in", 1);
            break;
        case SIMRESULT_ST_CLEAR:
            Flash_GotoLabel(flash, "fl_gameclear_in", 1);
            break;
        case SIMRESULT_ST_ITEM:
            Flash_GotoLabel(flash, "fl_itemget_in", 1);
            if (SIM_PROG->chara == 0x66) {
                gSimResult->talk = 0x15;
            } else {
                gSimResult->talk = 0x14;
            }
            break;
        case SIMRESULT_ST_COUNT:
        case SIMRESULT_ST_PAGES:
            break;
        }
    }
    if (gSimResult->pageShown && gSimResult->timer == 0) {
        if (gSimResult->page < gSimResult->pageMax) {
            if (gSimResult->page == 0) {
                Flash_GotoLabel(flash, "fl_battlebonus_in", 1);
            } else {
                Flash_GotoLabel(flash, "fl_battlebonus_out_in", 1);
            }
            gSimResult->page++;
        } else {
            if (gSimResult->page == 0) {
                Flash_GotoLabel(flash, "fl_hyouka_rank_in", 1);
            } else {
                Flash_GotoLabel(flash, "fl_rank_in", 1);
            }
            Snd_PlaySe(2, 0x13);
        }
        gSimResult->pageShown = 0;
    }
}

/* The guide's lines: starts line `talk` when the previous one is over (or was cut short by the confirm button). */
void SimResult_UpdateTalk(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSimResult->skip == 0) {
        if (gSimResult->talk == 0) {
            return;
        }
        if (gSimResult->voiceLine != -1 && Voice_GetStat(0) != SIM_VOICE_IDLE &&
            gSimResult->talk == gSimResult->talkLast) {
            if (gPad[0].gamePressed & 0x200) {
                Snd_PlaySe(1, 1);
                gSimResult->skip = 1;
            }
            return;
        }
    } else {
        gSimResult->skip = 0;
    }
    switch (gSimResult->talk) {
    case 1:
        gSimResult->voiceLine = 0x1F;
        break;
    case 2:
        gSimResult->voiceLine = 0x20;
        break;
    case 3:
        gSimResult->voiceLine = 0x21;
        break;
    case 4:
        gSimResult->voiceLine = 0x22;
        break;
    case 5:
        gSimResult->voiceLine = 0x23;
        break;
    case 6:
        gSimResult->voiceLine = 0x24;
        break;
    case 7:
        gSimResult->voiceLine = 0x25;
        break;
    case 8:
        gSimResult->voiceLine = 0x26;
        break;
    case 9:
        gSimResult->voiceLine = 0x27;
        break;
    case 10:
        gSimResult->voiceLine = 0x28;
        break;
    case 11:
        gSimResult->voiceLine = 0x29;
        break;
    case 12:
        gSimResult->voiceLine = 0x2A;
        gSimResult->talk = 14;
        break;
    case 13:
        gSimResult->voiceLine = 0x2B;
        gSimResult->talk = 14;
        break;
    case 14:
        ColorFade_StartOut(0, 0, 0, 0x14);
        gSimResult->talk = 0;
        return;
    case 15:
        gSimResult->voiceLine = 0x2C;
        break;
    case 16:
        gSimResult->voiceLine = 0x2D;
        break;
    case 17:
        gSimResult->voiceLine = 0x2E;
        break;
    case 18:
        gSimResult->voiceLine = 0x2F;
        break;
    case 19:
        gSimResult->voiceLine = 0x30;
        break;
    case 20:
        gSimResult->voiceLine = 0x31;
        break;
    case 21:
        gSimResult->voiceLine = 0x32;
        break;
    }
    if (gSimResult->talk != 14) {
        gSimResult->talk = 0;
    }
    Voice_PlayWithSubtitle(gSimResult->subtitles, SIM_VOICE_BASE, gSimResult->voiceLine);
    gSimResult->talkLast = gSimResult->talk;
}

/*
 * Pad 0. Clears *result when the player chooses to go on with the ladder (the counted total goes back to
 * gProgress as the ladder's points); otherwise the total is entered into the ranking and turned into money.
 */
void SimResult_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & 8;
    s32 down = gPad[0].gameRepeat & 4;
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 i;
    s32 idx;
    s32 kind;
    s32 rows;

    if (!(gSimResult->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (!(gSimResult->flags & SIMRESULT_STARTED)) {
        gSimResult->flags |= SIMRESULT_STARTED;
    }
    switch (gSimResult->state) {
    case SIMRESULT_ST_COUNT:
        if (gSimResult->counting) {
            idx = gSimResult->page != 0;
            if (UbScore_CountLine(&gSimResult->score, idx, gSimResult->step - idx * gSimResult->score.lineCount, 15)) {
                gSimResult->step++;
                if ((idx && (rows = 3, gSimResult->step % rows == 0)) ||
                    (!idx && gSimResult->step % gSimResult->score.lineCount == 0) ||
                    gSimResult->step == gSimResult->score.bonusCount + gSimResult->score.lineCount) {
                    gSimResult->counting = 0;
                    gSimResult->pageShown = 1;
                }
            }
        }
        if (ok) {
            for (i = 0; i < gSimResult->score.bonusCount + gSimResult->score.lineCount; i++) {
                idx = 0;
                kind = 0;
                if (i < gSimResult->score.lineCount) {
                    kind = 0;
                    idx = i;
                } else {
                    kind = 1;
                    idx = i - gSimResult->score.lineCount;
                }
                while (!UbScore_CountLine(&gSimResult->score, kind, idx, 9999)) {
                }
            }
            gSimResult->pageShown = 1;
            gSimResult->page = gSimResult->pageMax;
            gSimResult->step = gSimResult->score.bonusCount + gSimResult->score.lineCount;
            gSimResult->page = gSimResult->pageMax;
            UbScore_SetPage(&gSimResult->score, gSimResult->page);
        }
        if (gSimResult->pageShown) {
            if (gSimResult->page < gSimResult->pageMax) {
                if (gSimResult->page == 0) {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_battlebonus_in", 1);
                } else {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_battlebonus_out_in", 1);
                }
                gSimResult->page++;
            } else {
                if (gSimResult->page == 0) {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_hyouka_rank_in", 1);
                } else {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_rank_in", 1);
                }
                if (gSimResult->outcome == 0) {
                    if (SIM_PROG->chara == 0x66) {
                        gSimResult->talk = 4;
                    } else if (gSimResult->score.rank == 4) {
                        gSimResult->talk = 1;
                    } else {
                        gSimResult->talk = Rand_Libc() % 2 + 2;
                    }
                } else {
                    if (SIM_PROG->chara == 0x66) {
                        gSimResult->talk = 7;
                    } else {
                        gSimResult->talk = Rand_Libc() % 2 + 5;
                    }
                }
            }
            gSimResult->pageShown = 0;
        }
        if (gSimResult->step == gSimResult->score.bonusCount + gSimResult->score.lineCount) {
            gSimResult->state = SIMRESULT_ST_PAGES;
        }
        break;
    case SIMRESULT_ST_PAGES:
        if (up) {
            if (gSimResult->page > 0) {
                gSimResult->page--;
                if (gSimResult->page != 0) {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_battle_cansel_in", 1);
                } else {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_battlebonus_cancel", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (down) {
            if (gSimResult->page < gSimResult->pageMax) {
                gSimResult->page++;
                if (gSimResult->page == 0) {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_hyouka_cansel", 1);
                } else {
                    Flash_GotoLabel(&gSimResult->flash[0], "fl_battle_cansel_in", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (ok) {
            if (gSimResult->outcome == 0) {
                if (gSimResult->cleared) {
                    gSimResult->state = SIMRESULT_ST_CLEAR;
                    if (SIM_PROG->chara == 0x66) {
                        gSimResult->talk = 0x13;
                    } else {
                        gSimResult->talk = Rand_Range(2) + 0x11;
                    }
                } else {
                    gSimResult->state = SIMRESULT_ST_CONTINUE;
                    if (SIM_PROG->chara == 0x66) {
                        gSimResult->talk = 11;
                    } else {
                        gSimResult->talk = Rand_Range(3) + 8;
                    }
                }
            } else {
                gSimResult->state = SIMRESULT_ST_OVER;
            }
            if (gSimResult->page == 0) {
                Flash_GotoLabel(&gSimResult->flash[0], "fl_hyoukakarazpoint_trig", 1);
            } else {
                Flash_GotoLabel(&gSimResult->flash[0], "fl_zpoint_trig", 1);
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMRESULT_ST_OVER:
        if (ok) {
            Flash_GotoLabel(&gSimResult->flash[0], "fl_gameover_out", 1);
            gSimResult->flags |= SIMRESULT_DONE;
            gSimResult->flags |= SIMRESULT_LEAVING;
            gSimResult->timer = 15;
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMRESULT_ST_CONTINUE:
        if (up) {
            if (gSimResult->pick[gSimResult->state] != 0) {
                SimResult_YesNoGoto("fl_off_start");
                gSimResult->pick[gSimResult->state] = 0;
                SimResult_YesNoGoto("fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (down) {
            if (gSimResult->pick[gSimResult->state] == 0) {
                SimResult_YesNoGoto("fl_off_start");
                gSimResult->pick[gSimResult->state] = 1;
                SimResult_YesNoGoto("fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (ok) {
            if (gSimResult->pick[gSimResult->state] == 0) {
                *result = 0;
                gSimResult->talk = Rand_Range(2) + 12;
                SIM_PROG->run.stat[SIM_STAT_POINT] = gSimResult->score.total;
            } else {
                Flash_GotoLabel(&gSimResult->flash[0], "fl_zokkou_out", 1);
                gSimResult->state = SIMRESULT_ST_TRAIN;
                gSimResult->talk = Rand_Range(2) + 15;
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMRESULT_ST_TRAIN:
        if (ok) {
            Flash_GotoLabel(&gSimResult->flash[0], "fl_syugyo_out", 1);
            gSimResult->state = SIMRESULT_ST_MONEY;
            gSimResult->step = 0;
            gSimResult->timer = 60;
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMRESULT_ST_CLEAR:
        if (ok) {
            Flash_GotoLabel(&gSimResult->flash[0], "fl_gameclear_out", 1);
            gSimResult->state = SIMRESULT_ST_MONEY;
            gSimResult->step = 0;
            gSimResult->timer = 60;
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMRESULT_ST_MONEY:
        switch (gSimResult->step) {
        case 0:
            if (gSimResult->timer == 0) {
                gSimResult->step++;
                UbScore_PlateGoto(&gSimResult->flash[0], 0, 1);
                gSimResult->score.count = gSimResult->score.total;
                UbScore_AddRanking(gSimResult->score.count, SIM_PROG->chara, gSimResult->cleared);
            }
            break;
        case 1:
            if (UbScore_ConvertStep(&gSimResult->score, 0x3F)) {
                gSimResult->step++;
                gSimResult->timer = 15;
                UbScore_PlateGoto(&gSimResult->flash[0], 0, 0);
                gSimResult->score.convert = gSimResult->score.count;
            }
            break;
        case 2:
            if (gSimResult->timer == 0) {
                gSimResult->step++;
                UbScore_PlateGoto(&gSimResult->flash[0], 1, 1);
            }
            break;
        case 3:
            if (UbScore_Transfer(&gSimResult->score.convert, &SIM_SAVE->money, 0x3F, 1.0f)) {
                gSimResult->step++;
                UbScore_PlateGoto(&gSimResult->flash[0], 1, 0);
            }
            if (SIM_SAVE->money > SIM_MONEY_MAX) {
                SIM_SAVE->money = SIM_MONEY_MAX;
            }
            break;
        case 4:
            if (ok) {
                Flash_GotoLabel(&gSimResult->flash[0], "fl_zpoint_out", 1);
                if (gSimResult->gotItem == 0) {
                    gSimResult->flags |= SIMRESULT_DONE;
                    gSimResult->flags |= SIMRESULT_LEAVING;
                    gSimResult->timer = 15;
                    gSimResult->state = SIMRESULT_ST_END;
                } else {
                    gSimResult->state = SIMRESULT_ST_ITEM;
                    Snd_PlaySe(2, 6);
                }
                Snd_PlaySe(1, 1);
            }
            break;
        }
        if (gSimResult->step >= 1 && gSimResult->step <= 3 && ok) {
            if (gSimResult->step < 2) {
                while (!UbScore_ConvertStep(&gSimResult->score, SIM_MONEY_MAX)) {
                }
                gSimResult->score.convert = gSimResult->score.count;
                while (!UbScore_Transfer(&gSimResult->score.convert, &SIM_SAVE->money, SIM_MONEY_MAX, 1.0f)) {
                }
            } else if (gSimResult->step < 4) {
                while (!UbScore_Transfer(&gSimResult->score.convert, &SIM_SAVE->money, SIM_MONEY_MAX, 1.0f)) {
                }
            }
            if (SIM_SAVE->money > SIM_MONEY_MAX) {
                SIM_SAVE->money = SIM_MONEY_MAX;
            }
            UbScore_PlateGoto(&gSimResult->flash[0], 0, 0);
            UbScore_PlateGoto(&gSimResult->flash[0], 1, 0);
            gSimResult->step = 4;
        }
        break;
    case SIMRESULT_ST_ITEM:
        if (ok) {
            Flash_GotoLabel(&gSimResult->flash[0], "fl_itemget_out", 1);
            gSimResult->flags |= SIMRESULT_DONE;
            gSimResult->flags |= SIMRESULT_LEAVING;
            gSimResult->timer = 15;
            gSimResult->state = SIMRESULT_ST_END;
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMRESULT_ST_END:
        break;
    }
}

/*
 * The result screen (mode 23). Returns 0 when the ladder goes on (the handler then adds 1 to gProgress->run.turn
 * and sets mode 22), 1 when it is over: lost, finished, declined or the fight aborted (mode 20).
 */
s32 SimResult_Run(s32 section) {
    s32 result = 1;

    SimResult_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        SimResult_Update();
        SimResult_UpdateTalk();
        SimResult_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gSimResult->flags & SIMRESULT_GREETED) && (gSimResult->flash[0].flags & MFLASH_PAD)) {
                gSimResult->flags |= SIMRESULT_GREETED;
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
        if (gSimResult->flags & SIMRESULT_LEAVING) {
            if (gSimResult->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gSimResult->talk == 0) {
            SimResult_Input(&result);
        }
    }
    SimResult_Term();
    Dma_ResetBuffers();
    return result;
}
