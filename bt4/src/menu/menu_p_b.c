#include "common.h"
#include "menu/menu_p.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work area (.data, 0x3B737C). */
MisResult *gMisResult = NULL;

/*
 * MisResult, 0x37B7C0..0x37DC38: the result screen of a mission (mode 16), shown after the battle. The score
 * sheet (UbScore) is filled from the battle result; its lines and bonuses count into the total page by page, a
 * rank letter appears, then the total is paid out as money ("zpoint"); clearing the 30th, 50th and 100th mission
 * opens a reward window. An object of its own: .rodata 0x3B8630..0x3B8AC4, repeating strings of its neighbours.
 */

#define MR gMisResult

#define MR_RES(n) \
    res = (MTexRes *)MPACK_AT(MR->res, n); \
    Res_RelocateOffsets(&res, res, res)

#define MR_SET_UV(parent, clip) \
    Flash_FindLabel(flash, parent, clip, &ref); \
    Flash_ClipSetUv(flash, &ref, &uv)

/*
 * Marks mission `mission` as cleared and counts the cleared missions into *count. Returns 1 when the mission was
 * not cleared before. Sets the "all missions" flag with the hundredth.
 */
s32 MisResult_MarkCleared(s32 mission, s32 *count) {
    s32 isNew = 0;
    s32 i;

    *count = 0;
    for (i = 0; i < P_MISSION_NUM; i++) {
        if (P_SAVE->ub.mission[i].cleared != 0) {
            (*count)++;
        } else if (mission == i) {
            isNew = 1;
            P_SAVE->ub.mission[i].cleared = isNew;
            (*count)++;
        }
    }
    if (*count == P_MISSION_NUM && isNew) {
        P_SAVE->ubFlags |= P_UBFLAG_ALL_MISSIONS;
    }
    return isNew;
}

/* Clip callbacks of the fireworks: they are drawn inside the result window only. */
void MisResult_ClipScissorOn(void) {
    Sprite_SetScissor(0x84, 0x1DC, 0x5A, 0x1A0);
}

void MisResult_ClipScissorOff(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Loads the screen (section `section` of archive 3), works the score out and records the mission as cleared. */
void MisResult_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 total;

    MR = Heap_Alloc(sizeof(MisResult), 0x20, 0, 2);
    memset(MR, 0, sizeof(MisResult));
    MR->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    MR->res = Sprite_Unpack(MR->pack, NULL, NULL);
    MR_RES(5);
    MR->bg = res;
    MR_RES(7);
    MR->tex[39] = MTEX(res, 0);
    MR->tex[40] = MTEX(res, 1);
    MR->tex[41] = MTEX(res, 3);
    MR_RES(4);
    MR->tex[0] = MTEX(res, 0);
    MR->tex[1] = MTEX(res, 3);
    MR->tex[30] = MTEX(res, 6);
    MR->tex[21] = MTEX(res, 7);
    MR->tex[18] = MTEX(res, 8);
    MR->tex[10] = MTEX(res, 9);
    MR->tex[9] = MTEX(res, 10);
    MR->tex[38] = MTEX(res, 12);
    MR->tex[35] = MTEX(res, 13);
    MR->tex[36] = MTEX(res, 14);
    MR->tex[33] = MTEX(res, 15);
    MR->tex[32] = MTEX(res, 16);
    MR->tex[34] = MTEX(res, 17);
    MR->tex[23] = MTEX(res, 18);
    MR->tex[17] = MTEX(res, 19);
    MR->tex[26] = MTEX(res, 20);
    MR->tex[27] = MTEX(res, 21);
    MR->tex[20] = MTEX(res, 22);
    MR->tex[29] = MTEX(res, 23);
    MR->tex[16] = MTEX(res, 25);
    MR->tex[24] = MTEX(res, 26);
    MR->tex[3] = MTEX(res, 27);
    MR->tex[2] = MTEX(res, 28);
    MR->tex[19] = MTEX(res, 29);
    MR->tex[28] = MTEX(res, 30);
    MR->tex[4] = MTEX(res, 31);
    MR->tex[5] = MTEX(res, 32);
    MR->tex[11] = MTEX(res, 33);
    MR->tex[15] = MTEX(res, 34);
    MR->tex[14] = MTEX(res, 35);
    MR->tex[6] = MTEX(res, 36);
    MR->tex[8] = MTEX(res, 37);
    MR->tex[7] = MTEX(res, 38);
    MR->tex[25] = MTEX(res, 39);
    MR->tex[31] = MTEX(res, 43);
    MR->tex[45] = MTEX(res, 44);
    MR->tex[42] = MTEX(res, 45);
    MR->tex[43] = MTEX(res, 46);
    MR->tex[44] = MTEX(res, 47);
    MR->tex[12] = NULL;
    MR->tex[13] = NULL;
    MR->tex[22] = NULL;
    MR->tex[37] = NULL;
    MR->tex[46] = NULL;
    Flash_Create(&MR->flash[0], MPACK_AT(MR->res, 1), MR->tex);
    Flash_Play(&MR->flash[0], 1);
    MR->blink = Rand_Libc() % 32;
    MR_RES(8);
    IconWin_Init(MPACK_AT(MR->res, 9), res);
    IconWin_Open();
    IconWin_SetIcon(1);
    MR->text = MPACK_AT(MR->res, 12);
    MR->subtitles = MPACK_AT(MR->res, 11);
    MR->itemText = MPACK_AT(MR->res, 14);
    for (i = 0; i < 6; i++) {
        TextBox_Init(&MR->box[i], MR->text, 0);
        if (i == 4 || i == 5) {
            TextBox_SetUnk50(&MR->box[i], 1);
        } else {
            TextBox_SetUnk50(&MR->box[i], 0);
        }
    }
    TextBox_Init(&MR->itemBox, MR->itemText, 0);
    TextBox_SetUnk50(&MR->itemBox, 0);
    TextBox_SetLineOffsets(&MR->itemBox, 0xC, 0, 0, 0, 0);
    TextBox_SetMaxWidth(&MR->box[3], 0xEB);
    TextBox_SetLineOffsets(&MR->box[3], 0, -10, 0, 0, 0);
    MR->price = (UbScorePrice *)MPACK_AT(MR->res, 13);
    MR->voiceLine = -1;
    MR->page = 0;
    MR->timer = 15;
    MR->pageDone = 0;
    MR->counting = 0;
    MR->step = 0;
    MR->voiceSkip = 0;
    MR->reward = 0;
    MR->mission = P_PROG->misRank * MISSEL_ROWS + P_PROG->misRow;
    MR->outcome = UbScore_Fill(1, &MR->score, &MR->pages);
    UbScore_CalcPoints(MR->price, &MR->score);
    total = UbScore_CalcRank(1, &MR->score);
    if (MR->outcome == 0) {
        MR->newRecord = UbScore_SaveMissionBest(MR->mission, total, &MR->score);
        if (MisResult_MarkCleared(MR->mission, &MR->cleared)) {
            if (MR->cleared == 30) {
                MR->reward = 1;
                P_SAVE->ubFlags |= P_UBFLAG_ITEM3;
            } else if (MR->cleared == 50) {
                MR->reward = 2;
                MR->rewardItem = UbScore_GetRewardItem(1);
                Save_AddItem(MR->rewardItem);
            } else if (MR->cleared == 100) {
                MR->complete = 1;
                MR->reward = 3;
                MR->rewardItem = UbScore_GetRewardItem(2);
                Save_AddItem(MR->rewardItem);
            }
        }
    }
}

/* Frees everything Init made. */
void MisResult_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < MISRESULT_FLASH_NUM; i++) {
        Flash_Destroy(&MR->flash[i]);
    }
    if (MR->res != NULL) {
        Heap_Free(MR->res);
        MR->res = NULL;
    }
    if (MR != NULL) {
        Heap_Free(MR);
        MR = NULL;
    }
}

/* Draws the screen. */
void MisResult_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;
    s32 mission;

#ifdef PORT
    Port_MenuPage(30); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(MR->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(MR->bg, 0, 0, 0x80);
#endif
    flash = &MR->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &MR->blink, 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &MR->talk, 0);
    /* the mission's name */
    mission = P_PROG->misRank * MISSEL_ROWS + P_PROG->misRow;
    Flash_FindLabel(flash, "mc_window_plate01", "mc_window_text1_0", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, mission + 0xD8, &MR->box[3]);
    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x200;
    uv.y1 = 0x80;
    MR_SET_UV(NULL, "mc_win_text02");
    MR_SET_UV(NULL, "mc_win_text03");
    /* page arrows */
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x20;
    uv.y1 = 0x40;
    MR_SET_UV(NULL, "mc_yajirusi_icon_up");
    if (MR->page == 0) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }
    uv.x0 = 0x20;
    uv.y0 = 0x20;
    uv.x1 = 0x40;
    uv.y1 = 0x40;
    MR_SET_UV(NULL, "mc_yajirusi_icon_down");
    if (MR->score.bonusCount != 0 && MR->page < MR->pages) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    /* the score lines: value and points */
    for (i = 0; i < MR->score.lineCount; i++) {
        sprintf(name, "mc_point_plate%02d", i + 1);
        Num_DrawChild(flash, name, "mc_percent_suji%02d", 0, 5, MR->score.line[i].value, 0x20, 0x20, 0, 0);
        Num_DrawChild(flash, name, "mc_pt_suji%02d", 0, 6, MR->score.line[i].points * 100, 0x20, 0x20, 0, 0);
    }
    /* the three bonus plates of the page */
    for (i = 0; i < UBSCORE_PAGE; i++) {
        sprintf(name, "mc_bonus_plate%02d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (MR->page * UBSCORE_PAGE + i - UBSCORE_PAGE >= MR->score.bonusCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_bonus_karisize", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, MR->score.bonus[MR->score.shown[i]].value + 0x20, &MR->box[i]);
            sprintf(name, "mc_bonus_plate%02d", i + 1);
            Num_DrawChild(flash, name, "mc_point_suji%02d", 0, 5, MR->score.bonus[MR->score.shown[i]].points * 100,
                          0x20, 0x20, 0, 0);
        }
    }
    /* the captions of lines 2..4 */
    for (i = 1; i < 4; i++) {
        uv.x0 = 0;
        uv.y0 = i << 5;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_point_plate%02d", i + 1);
        MR_SET_UV(name, "mc_bonus_text");
        Flash_FindLabel(flash, name, "mc_pt_percent", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    Num_Draw(flash, "mc_time_suji%02d", 4, 2, MR->score.clock.hours, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 2, 2, MR->score.clock.minutes, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 0, 2, MR->score.clock.seconds, 0x20, 0x20, 1);
    uv.x0 = 0;
    uv.y0 = 0x80;
    uv.x1 = 0x100;
    uv.y1 = 0xA0;
    MR_SET_UV(NULL, "mc_bonus_title");
    uv.x0 = 0;
    uv.y0 = 0x80;
    uv.x1 = 0x200;
    uv.y1 = 0xC0;
    MR_SET_UV(NULL, "mc_win_text04");
    Num_Draw(flash, "mc_henkan_suji%02d", 0, 6, MR->score.convert, 0x20, 0x20, 0);
    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x100;
    uv.y1 = 0x60;
    MR_SET_UV(NULL, "mc_time_text03");
    Num_Draw(flash, "mc_zpoint_suji%02d", 0, 9, P_SAVE->money, 0x20, 0x20, 0);
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    MR_SET_UV(NULL, "mc_time_text02");
    Num_Draw(flash, "mc_score_suji%02d", 0, 9, MR->score.total * 100, 0x20, 0x20, 0);
    /* rank letter */
    uv.x0 = MR->score.rank << 6;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = 0x40;
    MR_SET_UV(NULL, "mc_rankmoji_anime");
    Flash_FindLabel(flash, NULL, "mc_newrecord_text", &ref);
    if (MR->newRecord) {
        uv.x0 = 0;
        uv.y0 = 0x40;
        uv.x1 = 0x100;
        uv.y1 = 0x80;
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    Flash_FindLabel(flash, NULL, "mc_hanabi", &ref);
    Flash_ClipSetCallbackA(flash, &ref, MisResult_ClipScissorOn, NULL);
    Flash_ClipSetCallbackB(flash, &ref, MisResult_ClipScissorOff, NULL);
    if (MR->reward != 0) {
        Flash_FindLabel(flash, NULL, "mc_text_kari0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, MR->reward + 0xF, &MR->box[4]);
        switch (MR->reward) {
        case 1:
            Flash_FindLabel(flash, NULL, "mc_item_get_text", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, NULL, "mc_item_plate00", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, NULL, "mc_text_kari1", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, 0x13, &MR->box[5]);
            break;
        case 2:
        case 3:
            Flash_FindLabel(flash, "mc_item_plate00", "mc_item_text_kari", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, MR->rewardItem, &MR->itemBox);
            break;
        }
    }
    for (i = 0; i < MISRESULT_FLASH_NUM; i++) {
        Flash_Draw(&MR->flash[i]);
    }
    IconWin_Draw();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* After a page has finished counting: the next bonus page, or the rank with the guide's verdict. */
#define MR_NEXT_PAGE(flash, snd) \
    if (MR->page < MR->pages) { \
        if (MR->page == 0) { \
            Flash_GotoLabel(flash, "fl_battlebonus_in", 1); \
        } else { \
            Flash_GotoLabel(flash, "fl_battlebonus_out_in", 1); \
        } \
        MR->page++; \
    } else { \
        if (MR->page == 0) { \
            Flash_GotoLabel(flash, "fl_hyouka_rank_in", 1); \
        } else { \
            Flash_GotoLabel(flash, "fl_rank_in", 1); \
        } \
        snd; \
        if (MR->score.rank == 4) { \
            MR->voiceReq = 16; \
        } else if (Rand_Range(2) != 0) { \
            MR->voiceReq = 17; \
        } else { \
            MR->voiceReq = 18; \
        } \
    } \
    MR->pageDone = 0

/* Runs the timer and the movie and answers the movie's triggers. */
void MisResult_Update(void) {
    MFlashRef ref;
    MFlash *flash = &MR->flash[0];
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (MR->timer > 0) {
        MR->timer--;
    }
    for (i = 0; i < MISRESULT_FLASH_NUM; i++) {
        Flash_Advance(&MR->flash[i]);
    }
    if (MR->flash[0].trig & 1) {
        if (MR->state == MISRESULT_ST_INTRO) {
            MR->started = 1;
        }
    }
    if (MR->flash[0].trig & 2) {
        if (MR->state == MISRESULT_ST_COMPLETE) {
            Flash_GotoLabel(flash, "fl_itemget_in", 1);
            MR->state = MISRESULT_ST_REWARD;
        }
    }
    if (MR->flash[0].trig & 4) {
        if (MR->state == MISRESULT_ST_COUNT) {
            MR->counting = 1;
        }
        UbScore_SetPage(&MR->score, MR->page);
    }
    if (MR->flash[0].trig & 8) {
        switch (MR->state) {
        case MISRESULT_ST_INTRO:
            if (MR->outcome == 0) {
                Flash_FindLabel(flash, NULL, "mc_win_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
                if (MR->complete) {
                    MR->voiceReq = 11;
                } else {
                    MR->voiceReq = 1;
                }
            } else {
                Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
                MR->voiceReq = 6;
            }
            break;
        case MISRESULT_ST_MONEY:
            Flash_GotoLabel(flash, "fl_zpoint_in", 1);
            break;
        case MISRESULT_ST_COMPLETE:
            Flash_FindLabel(flash, NULL, "mc_clear_anime", &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_start");
            Flash_FindLabel(flash, NULL, "mc_hanabi", &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_start");
            break;
        case MISRESULT_ST_REWARD:
            Flash_GotoLabel(flash, "fl_itemget_in", 1);
            Snd_PlaySe(2, 6);
            break;
        }
    }
    if (MR->pageDone) {
        if (MR->timer == 0) {
            MR_NEXT_PAGE(flash, Snd_PlaySe(2, 0x13));
        }
    }
}

/*
 * The guide's script (MisResult.voiceReq; 0 = none, the pad is read): 1 won, 6 lost, 11 all missions cleared (each
 * followed by a step that ends the script), 16..19 one-line comments (rank 4, the two verdicts, the pay-out).
 */
void MisResult_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (MR->voiceSkip == 0) {
        if (MR->voiceReq == 0) {
            return;
        }
        if (MR->voiceLine != -1 && Voice_GetStat(0) != P_VOICE_IDLE && MR->voiceReq == MR->voiceLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                Voice_StopWithLip();
                MR->voiceSkip = 1;
            }
            return;
        }
    } else {
        MR->voiceSkip = 0;
    }
    switch (MR->voiceReq) {
    case 1:
        MR->voiceLine = 0x38;
        MR->voiceReq++;
        break;
    case 6:
        MR->voiceLine = 0x39;
        MR->voiceReq++;
        break;
    case 16:
        MR->voiceLine = 0x3A;
        break;
    case 17:
        MR->voiceLine = 0x3B;
        break;
    case 18:
        MR->voiceLine = 0x3C;
        break;
    case 19:
        MR->voiceLine = 0x3D;
        break;
    case 11:
        MR->voiceLine = 0x3E;
        MR->voiceReq++;
        break;
    case 2:
    case 7:
    case 12:
        MR->voiceReq = 0;
        MR->voiceLine = -1;
        break;
    }
    if (MR->voiceReq >= 16) {
        MR->voiceReq = 0;
    }
    if (MR->voiceLine != -1) {
        Voice_PlayWithSubtitle(MR->subtitles, P_VOICE_BASE, MR->voiceLine);
        MR->voiceLast = MR->voiceReq;
    }
}

#define MR_LEAVE() \
    MR->flags |= MISRESULT_DONE; \
    MR->flags |= MISRESULT_LEAVING; \
    MR->timer = 15; \
    MR->state = MISRESULT_ST_LEAVE

#define MR_PAY(step) UbScore_Transfer(&MR->score.convert, &P_SAVE->money, step, 1.0f)

/* Pad 0, by state. */
void MisResult_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;

    if (MR->state == MISRESULT_ST_INTRO && MR->started) {
        if (MR->outcome == 2) {
            /* the battle was aborted: nothing to show */
            MR_LEAVE();
        } else {
            Flash_GotoLabel(&MR->flash[0], "fl_hyouka_menu_in", 1);
            MR->state = MISRESULT_ST_COUNT;
        }
        MR->started = 0;
    }
    if (!(MR->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (!(MR->flags & MISRESULT_STARTED)) {
        MR->flags |= MISRESULT_STARTED;
    }
    switch (MR->state) {
    case MISRESULT_ST_COUNT:
        if (MR->counting) {
            s32 isBonus = MR->page != 0;

            if (UbScore_CountLine(&MR->score, isBonus, MR->step - isBonus * MR->score.lineCount, 15)) {
                MR->step++;
                /*
                 * The page is done after three bonuses, after the last line, or when everything is counted. Written
                 * with gotos: the forms tried without them (a flag, ?:, the test repeated in both arms) do not
                 * give the original's single copy of the second test. The divisor is a variable (no reciprocal
                 * multiply in the original).
                 */
                if (isBonus) {
                    s32 n = UBSCORE_PAGE;

                    if (MR->step % n != 0) {
                        goto check;
                    }
                } else {
                    if (MR->step % MR->score.lineCount != 0) {
                        goto check;
                    }
                }
                goto done;
            check:
                if (MR->step == MR->score.bonusCount + MR->score.lineCount) {
                done:
                    MR->counting = 0;
                    MR->pageDone = 1;
                }
            }
        }
        if (confirm) {
            /* skip the counting */
            s32 i;

            for (i = 0; i < MR->score.bonusCount + MR->score.lineCount; i++) {
                s32 index = 0;
                s32 isBonus = 0;

                if (i < MR->score.lineCount) {
                    isBonus = 0;
                    index = i;
                } else {
                    index = i - MR->score.lineCount;
                    isBonus = 1;
                }
                while (!UbScore_CountLine(&MR->score, isBonus, index, 9999)) {
                }
            }
            MR->pageDone = 1;
            MR->page = MR->pages;
            MR->step = MR->score.bonusCount + MR->score.lineCount;
            MR->page = MR->pages;
            UbScore_SetPage(&MR->score, MR->page);
        }
        if (MR->pageDone) {
            MR_NEXT_PAGE(&MR->flash[0], (void)0);
        }
        if (MR->step == MR->score.bonusCount + MR->score.lineCount) {
            MR->state = MISRESULT_ST_PAGES;
        }
        break;
    case MISRESULT_ST_PAGES:
        if (up) {
            if (MR->page > 0) {
                MR->page--;
                if (MR->page != 0) {
                    Flash_GotoLabel(&MR->flash[0], "fl_battle_cansel_in", 1);
                } else {
                    Flash_GotoLabel(&MR->flash[0], "fl_battlebonus_cancel", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (down) {
            if (MR->page < MR->pages) {
                MR->page++;
                if (MR->page == 0) {
                    Flash_GotoLabel(&MR->flash[0], "fl_hyouka_cansel", 1);
                } else {
                    Flash_GotoLabel(&MR->flash[0], "fl_battle_cansel_in", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (confirm) {
            MR->state = MISRESULT_ST_MONEY;
            MR->step = 0;
            MR->timer = 60;
            if (MR->page == 0) {
                Flash_GotoLabel(&MR->flash[0], "fl_hyoukakarazpoint_trig", 1);
            } else {
                Flash_GotoLabel(&MR->flash[0], "fl_zpoint_trig", 1);
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case MISRESULT_ST_MONEY:
        switch (MR->step) {
        case 0:
            if (MR->timer == 0) {
                MR->step++;
                UbScore_PlateGoto(&MR->flash[0], 0, 1);
                MR->score.saved = MR->score.total;
                MR->voiceReq = 19;
            }
            break;
        case 1:
            if (UbScore_ConvertStep(&MR->score, 0x40)) {
                MR->step++;
                MR->timer = 15;
                UbScore_PlateGoto(&MR->flash[0], 0, 0);
                MR->score.convert = MR->score.saved;
            }
            break;
        case 2:
            if (MR->timer == 0) {
                MR->step++;
                UbScore_PlateGoto(&MR->flash[0], 1, 1);
            }
            break;
        case 3:
            if (MR_PAY(0x40)) {
                MR->step++;
                UbScore_PlateGoto(&MR->flash[0], 1, 0);
            }
            if (P_SAVE->money > P_MONEY_MAX) {
                P_SAVE->money = P_MONEY_MAX;
            }
            break;
        case 4:
            if (confirm) {
                Flash_GotoLabel(&MR->flash[0], "fl_zpoint_out", 1);
                if (MR->complete) {
                    MR->state = MISRESULT_ST_COMPLETE;
                } else if (MR->reward != 0) {
                    MR->state = MISRESULT_ST_REWARD;
                    if (MR->reward == 1) {
                        Snd_PlaySe(2, 7);
                    } else {
                        Snd_PlaySe(2, 6);
                    }
                } else {
                    MR_LEAVE();
                }
                Snd_PlaySe(1, 1);
            }
            break;
        }
        if (MR->step >= 1 && MR->step <= 3 && confirm) {
            /* skip the pay-out */
            if (MR->step < 2) {
                while (!UbScore_ConvertStep(&MR->score, P_MONEY_MAX)) {
                }
                MR->score.convert = MR->score.saved;
                while (!MR_PAY(P_MONEY_MAX)) {
                }
            } else if (MR->step < 4) {
                while (!MR_PAY(P_MONEY_MAX)) {
                }
            }
            if (P_SAVE->money > P_MONEY_MAX) {
                P_SAVE->money = P_MONEY_MAX;
            }
            UbScore_PlateGoto(&MR->flash[0], 0, 0);
            UbScore_PlateGoto(&MR->flash[0], 1, 0);
            MR->step = 4;
        }
        break;
    case MISRESULT_ST_COMPLETE:
        if (confirm) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Snd_PlaySe(1, 1);
        }
        break;
    case MISRESULT_ST_REWARD:
        if (confirm) {
            Flash_GotoLabel(&MR->flash[0], "fl_itemget_out", 1);
            MR_LEAVE();
            Snd_PlaySe(1, 1);
        }
        break;
    case MISRESULT_ST_LEAVE:
        break;
    }
}

/* The mission result screen (mode 16). The return value is not used. */
s32 MisResult_Run(s32 section) {
    s32 result = 1;

    MisResult_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        MisResult_Update();
        MisResult_UpdateVoice();
        MisResult_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(MR->flags & MISRESULT_GREETED) && (MR->flash[0].flags & MFLASH_PAD)) {
                MR->flags |= MISRESULT_GREETED;
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
        if (MR->flags & MISRESULT_LEAVING) {
            if (MR->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (MR->voiceReq == 0) {
            MisResult_Input(&result);
        }
    }
    MisResult_Term();
    Dma_ResetBuffers();
    return result;
}
