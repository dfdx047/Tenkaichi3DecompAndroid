#include "common.h"
#include "menu/menu_o.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * UbResult, 0x3760C8..0x3782A8: the result screen of the two disc modes (modes 27 and 30); work pointer 0x3B7358,
 * .rodata 0x3B7E50..0x3B8284 (from "mc_guide_17go"). Merged from two chunks: the head (0x3760C8..0x376920,
 * MarkCourseCleared and Init, written as menu_n_d.c with the NResult view of include/menu/menu_n.h) and the rest
 * (0x376920.., written as menu_o.c). The one view used now is UbResult / UoProgress / UoSave of
 * include/menu/menu_o.h. The score sheet (UoScore) is filled and counted by the module at 0x37EE18
 * (menu_p / menu_q).
 */

UbResult *gUbResult = NULL; /* 0x3B7358 */

#define RS_RES(n) \
    res = (MTexRes *)MPACK_AT(gUbResult->res, n); \
    Res_RelocateOffsets(&res, res, res)

/*
 * Marks a course as cleared in the save. Returns 1 the first time all five courses are cleared (the caller then
 * gives the reward item).
 */
s32 UbResult_MarkCourseCleared(s32 course) {
    s32 count = 0;
    s32 i;

    gSaveData->course[course].cleared = 1;
    for (i = 0; i < UO_COURSE_NUM; i++) {
        if (gSaveData->course[i].cleared) {
            count++;
        }
    }
    if (count == UO_COURSE_NUM && !(gSaveData->unk208 & 0x10)) {
        gSaveData->unk208 |= 0x10;
        return 1;
    }
    return 0;
}

/*
 * Creates the result screen: movie, icon window, text boxes; fills the score sheet from the battle result and
 * writes what the battle earned into the save (the course's best result, the reward items).
 */
void UbResult_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 rank;

    gUbResult = Heap_Alloc(sizeof(UbResult), 0x20, 0, 2);
    memset(gUbResult, 0, sizeof(UbResult));
    gUbResult->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gUbResult->res = Sprite_Unpack(gUbResult->pack, NULL, NULL);
    RS_RES(5);
    gUbResult->bg = res;
    RS_RES(7);
    gUbResult->tex[39] = MTEX(res, 0);
    gUbResult->tex[40] = MTEX(res, 1);
    gUbResult->tex[41] = MTEX(res, 3);
    RS_RES(4);
    gUbResult->tex[0] = MTEX(res, 0);
    gUbResult->tex[1] = MTEX(res, 3);
    gUbResult->tex[30] = MTEX(res, 6);
    gUbResult->tex[21] = MTEX(res, 7);
    gUbResult->tex[18] = MTEX(res, 8);
    gUbResult->tex[10] = MTEX(res, 9);
    gUbResult->tex[9] = MTEX(res, 10);
    gUbResult->tex[38] = MTEX(res, 12);
    gUbResult->tex[35] = MTEX(res, 13);
    gUbResult->tex[36] = MTEX(res, 14);
    gUbResult->tex[33] = MTEX(res, 15);
    gUbResult->tex[32] = MTEX(res, 16);
    gUbResult->tex[34] = MTEX(res, 17);
    gUbResult->tex[23] = MTEX(res, 18);
    gUbResult->tex[17] = MTEX(res, 19);
    gUbResult->tex[26] = MTEX(res, 20);
    gUbResult->tex[27] = MTEX(res, 21);
    gUbResult->tex[20] = MTEX(res, 22);
    gUbResult->tex[29] = MTEX(res, 23);
    gUbResult->tex[16] = MTEX(res, 25);
    gUbResult->tex[24] = MTEX(res, 26);
    gUbResult->tex[3] = MTEX(res, 27);
    gUbResult->tex[2] = MTEX(res, 28);
    gUbResult->tex[19] = MTEX(res, 29);
    gUbResult->tex[28] = MTEX(res, 30);
    gUbResult->tex[4] = MTEX(res, 31);
    gUbResult->tex[5] = MTEX(res, 32);
    gUbResult->tex[11] = MTEX(res, 33);
    gUbResult->tex[15] = MTEX(res, 34);
    gUbResult->tex[14] = MTEX(res, 35);
    gUbResult->tex[6] = MTEX(res, 36);
    gUbResult->tex[8] = MTEX(res, 37);
    gUbResult->tex[7] = MTEX(res, 38);
    gUbResult->tex[25] = MTEX(res, 39);
    gUbResult->tex[31] = MTEX(res, 43);
    gUbResult->tex[45] = MTEX(res, 44);
    gUbResult->tex[42] = MTEX(res, 45);
    gUbResult->tex[43] = MTEX(res, 46);
    gUbResult->tex[44] = MTEX(res, 47);
    gUbResult->tex[12] = NULL;
    gUbResult->tex[13] = NULL;
    gUbResult->tex[22] = NULL;
    gUbResult->tex[37] = NULL;
    gUbResult->tex[46] = NULL;
    Flash_Create(&gUbResult->flash[0], MPACK_AT(gUbResult->res, 1), gUbResult->tex);
    Flash_Play(&gUbResult->flash[0], 1);
    gUbResult->blink = Rand_Libc() % 32;
    RS_RES(8);
    IconWin_Init(MPACK_AT(gUbResult->res, 9), res);
    IconWin_Open();
    gUbResult->text = MPACK_AT(gUbResult->res, 12);
    gUbResult->subtitles = MPACK_AT(gUbResult->res, 11);
    gUbResult->itemText = MPACK_AT(gUbResult->res, 14);
    for (i = 0; i < 4; i++) {
        TextBox_Init(&gUbResult->box[i], gUbResult->text, 0);
        TextBox_SetUnk50(&gUbResult->box[i], 0);
    }
    TextBox_Init(&gUbResult->itemBox, gUbResult->itemText, 0);
    TextBox_SetUnk50(&gUbResult->itemBox, 0);
    TextBox_SetLineOffsets(&gUbResult->itemBox, 12, 0, 0, 0, 0);
    gUbResult->bonusTbl = MPACK_AT(gUbResult->res, 13);
    gUbResult->voiceLine = -1;
    gUbResult->page = 0;
    gUbResult->timer = 15;
    gUbResult->pageDone = 0;
    gUbResult->counting = 0;
    gUbResult->count = 0;
    gUbResult->gotItem = 0;
    gUbResult->skip = 0;
    gUbResult->kind = UO_PROG->cursor;
    gUbResult->lose = UbScore_Fill(3, &gUbResult->score, &gUbResult->pageCount);
    UbScore_CalcPoints(gUbResult->bonusTbl, &gUbResult->score);
    rank = UbScore_CalcRank(3, &gUbResult->score);
    if (gUbResult->kind == 1) {
        /* a course */
        gUbResult->course = UO_PROG->course;
        IconWin_SetIcon(4);
        if (gUbResult->lose == 0) {
            gUbResult->newRecord = UbScore_SaveBestC(gUbResult->course, rank, &gUbResult->score);
            if (UbResult_MarkCourseCleared(gUbResult->course)) {
                Save_AddItem(UbScore_GetRewardItem(5));
                gUbResult->gotItem = 1;
            }
        }
    } else {
        /* the ladder: first place taken */
        IconWin_SetIcon(3);
        if (gUbResult->lose == 0 && gSaveData->rank == 1 && (UO_PROG->discFlags & UB_DISC_FLAG8)) {
            gUbResult->gotItem = 1;
            Save_AddItem(UbScore_GetRewardItem(4));
        }
    }
}

/* Frees the screen. */
void UbResult_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < 1; i++) {
        Flash_Destroy(&gUbResult->flash[i]);
    }
    if (gUbResult->res != NULL) {
        Heap_Free(gUbResult->res);
        gUbResult->res = NULL;
    }
    if (gUbResult != NULL) {
        Heap_Free(gUbResult);
        gUbResult = NULL;
    }
}

/* Draws the sheet: headline, the base rows, the three bonus rows of the page, time, score, Z points, rank. */
void UbResult_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;
    s32 item;

#ifdef PORT
    Port_MenuPage(27); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gUbResult->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gUbResult->bg, 0, 0, 0x80);
#endif
    flash = &gUbResult->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gUbResult->blink, 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gUbResult->talk, 0);
    if (gUbResult->lose == 0) {
        Flash_FindLabel(flash, "mc_window_plate01", "mc_window_text1_0", &ref);
        if (gUbResult->kind == 0 && (UO_PROG->discFlags & UB_DISC_FLAG8)) {
            TextBox_AttachLine(flash, &ref, 0, 0, 0x140, &gUbResult->box[3]);
        } else {
            TextBox_AttachLine(flash, &ref, 0, 0, 0x141, &gUbResult->box[3]);
        }
    } else {
        Flash_FindLabel(flash, "mc_window_plate01", "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, 0x142, &gUbResult->box[3]);
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
    if (gUbResult->page == 0) {
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
    if (gUbResult->score.bonusCount != 0 && gUbResult->page < gUbResult->pageCount) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    for (i = 0; i < gUbResult->score.baseCount; i++) {
        sprintf(name, "mc_point_plate%02d", i + 1);
        Num_DrawChild(flash, name, "mc_percent_suji%02d", 0, 5, gUbResult->score.base[i].value, 0x20, 0x20, 0, 0);
        Num_DrawChild(flash, name, "mc_pt_suji%02d", 0, 6, gUbResult->score.base[i].pt * 100, 0x20, 0x20, 0, 0);
    }
    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_bonus_plate%02d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (gUbResult->page * 3 + i - 3 >= gUbResult->score.bonusCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_bonus_karisize", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, gUbResult->score.bonus[gUbResult->score.row[i]].value + 0x20,
                               &gUbResult->box[i]);
            sprintf(name, "mc_bonus_plate%02d", i + 1);
            Num_DrawChild(flash, name, "mc_point_suji%02d", 0, 5,
                          gUbResult->score.bonus[gUbResult->score.row[i]].pt * 100, 0x20, 0x20, 0, 0);
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
    Num_Draw(flash, "mc_time_suji%02d", 4, 2, gUbResult->score.time[0], 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 2, 2, gUbResult->score.time[1], 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 0, 2, gUbResult->score.time[2], 0x20, 0x20, 1);
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
    Num_Draw(flash, "mc_henkan_suji%02d", 0, 6, gUbResult->score.points, 0x20, 0x20, 0);
    uv.x0 = 0;
    uv.y0 = 0x40;
    uv.x1 = 0x100;
    uv.y1 = 0x60;
    Flash_FindLabel(flash, NULL, "mc_time_text03", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_zpoint_suji%02d", 0, 9, UO_SAVE->money, 0x20, 0x20, 0);
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_time_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_score_suji%02d", 0, 9, gUbResult->score.score * 100, 0x20, 0x20, 0);
    uv.x0 = gUbResult->score.rank * 0x40;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_rankmoji_anime", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gUbResult->kind == 0) {
        Flash_FindLabel(flash, NULL, "mc_newrecord_text", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_FindLabel(flash, NULL, "mc_newrecord_text", &ref);
        if (gUbResult->newRecord) {
            uv.x0 = 0;
            uv.y0 = 0x40;
            uv.x1 = 0x100;
            uv.y1 = 0x80;
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    if (gUbResult->gotItem) {
        if (gUbResult->kind == 0) {
            item = UbScore_GetRewardItem(4);
        } else {
            item = UbScore_GetRewardItem(5);
        }
        Flash_FindLabel(flash, "mc_item_plate00", "mc_item_text_kari", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, item, &gUbResult->itemBox);
    }
    Flash_Draw(&gUbResult->flash[0]);
    IconWin_Draw();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Per frame: the movie and what its triggers start (headline, page counting, the Z point and item windows). */
void UbResult_Update(void) {
    MFlashRef ref;
    MFlash *flash = &gUbResult->flash[0];

    if (UO_PROG->flags & MPROG_FREEZE) {
        return;
    }
    if (gUbResult->timer > 0) {
        gUbResult->timer--;
    }
    Flash_Advance(&gUbResult->flash[0]);
    /*
     * One pointer variable, re-read from the global before each test and before the call (an accessor in the
     * original, perhaps): with plain gUbResult-> accesses the first test keeps its pointer in v1 instead of a0.
     */
    {
        UbResult *w;

        w = gUbResult;
        if (w->flash[0].trig & 1) {
            if (w->step == UBRES_STEP_INTRO) {
                w->ready = 1;
            }
        }
        w = gUbResult;
        if (w->flash[0].trig & 4) {
            if (w->step == UBRES_STEP_COUNT) {
                w->counting = 1;
                w = gUbResult;
            }
            UbScore_SetPage(&w->score, w->page);
        }
    }
    if (gUbResult->flash[0].trig & 8) {
        switch (gUbResult->step) {
        case UBRES_STEP_INTRO:
            if (gUbResult->lose == 0) {
                Flash_FindLabel(flash, NULL, "mc_win_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
                gUbResult->talkStep = 1;
            } else {
                Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_start");
                gUbResult->talkStep = 6;
            }
            break;
        case UBRES_STEP_PAY:
            Flash_GotoLabel(flash, "fl_zpoint_in", 1);
            break;
        case UBRES_STEP_ITEM:
            Flash_GotoLabel(flash, "fl_itemget_in", 1);
            Snd_PlaySe(2, 6);
            break;
        }
    }
    if (gUbResult->pageDone) {
        if (gUbResult->timer == 0) {
            if (gUbResult->page < gUbResult->pageCount) {
                if (gUbResult->page == 0) {
                    Flash_GotoLabel(flash, "fl_battlebonus_in", 1);
                } else {
                    Flash_GotoLabel(flash, "fl_battlebonus_out_in", 1);
                }
                gUbResult->page++;
            } else {
                if (gUbResult->page == 0) {
                    Flash_GotoLabel(flash, "fl_hyouka_rank_in", 1);
                } else {
                    Flash_GotoLabel(flash, "fl_rank_in", 1);
                }
                Snd_PlaySe(2, 0x13);
                if (gUbResult->gotItem) {
                    gUbResult->talkStep = 15;
                } else if (gUbResult->score.rank == 4) {
                    gUbResult->talkStep = 11;
                } else if (Rand_Range(2)) {
                    gUbResult->talkStep = 12;
                } else {
                    gUbResult->talkStep = 13;
                }
            }
            gUbResult->pageDone = 0;
        }
    }
}

/* The guide's lines: waits for the running line (or its skip), then starts the next one. */
void UbResult_UpdateTalk(void) {
    if (UO_PROG->flags & MPROG_FREEZE) {
        return;
    }
    if (gUbResult->skip == 0) {
        if (gUbResult->talkStep == 0) {
            return;
        }
        if (gUbResult->voiceLine != -1 && Voice_GetStat(0) != UB_VOICE_IDLE_O &&
            gUbResult->talkStep == gUbResult->talkPrev) {
            if (gPad[0].gamePressed & 0x200) {
                Snd_PlaySe(1, 1);
                gUbResult->skip = 1;
            }
            return;
        }
    } else {
        gUbResult->skip = 0;
    }
    switch (gUbResult->talkStep) {
    case 1:
        gUbResult->voiceLine = 0x38;
        gUbResult->talkStep++;
        break;
    case 6:
        gUbResult->voiceLine = 0x39;
        gUbResult->talkStep++;
        break;
    case 2:
    case 7:
        gUbResult->talkStep = 0;
        gUbResult->voiceLine = -1;
        break;
    case 11:
        gUbResult->voiceLine = 0x3A;
        break;
    case 12:
        gUbResult->voiceLine = 0x3B;
        break;
    case 13:
        gUbResult->voiceLine = 0x3C;
        break;
    case 14:
        gUbResult->voiceLine = 0x3D;
        break;
    case 15:
        gUbResult->voiceLine = 0x51;
        break;
    }
    if (gUbResult->talkStep > 10) {
        gUbResult->talkStep = 0;
    }
    if (gUbResult->voiceLine != -1) {
        Voice_PlayWithSubtitle(gUbResult->subtitles, UB_VOICE_BASE_O, gUbResult->voiceLine);
        gUbResult->talkPrev = gUbResult->talkStep;
    }
}

/* Pad 0: confirm skips the counting, up / down turn the sheet's pages, confirm goes on to the Z points. */
void UbResult_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & 8;
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 down = gPad[0].gameRepeat & 4;
    s32 i;
    s32 row;   /* first the "bonus page" flag of the row being counted, then a row index */
    s32 bonus;

    if (gUbResult->step == UBRES_STEP_INTRO && gUbResult->ready) {
        if (gUbResult->lose == 2) {
            gUbResult->flags |= UBRES_DONE;
            gUbResult->flags |= UBRES_LEAVING;
            gUbResult->timer = 15;
            gUbResult->step = UBRES_STEP_LEAVE;
        } else {
            Flash_GotoLabel(&gUbResult->flash[0], "fl_hyouka_menu_in", 1);
            gUbResult->step = UBRES_STEP_COUNT;
        }
        gUbResult->ready = 0;
    }
    if (!(gUbResult->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (UO_PROG->flags & MPROG_FREEZE) {
        return;
    }
    if (!(gUbResult->flags & UBRES_STARTED)) {
        gUbResult->flags |= UBRES_STARTED;
    }
    switch (gUbResult->step) {
    case UBRES_STEP_COUNT:
        if (gUbResult->counting) {
            row = gUbResult->page != 0;
            if (UbScore_CountLine(&gUbResult->score, row, gUbResult->count - row * gUbResult->score.baseCount, 15)) {
                gUbResult->count++;
                if ((row && gUbResult->count % 3 == 0) || (!row && gUbResult->count % gUbResult->score.baseCount == 0) ||
                    gUbResult->count == gUbResult->score.bonusCount + gUbResult->score.baseCount) {
                    gUbResult->counting = 0;
                    gUbResult->pageDone = 1;
                }
            }
        }
        if (ok) {
            for (i = 0; i < gUbResult->score.bonusCount + gUbResult->score.baseCount; i++) {
                /* the redundant assignments are needed for the original's register use */
                row = 0;
                bonus = 0;
                if (i < gUbResult->score.baseCount) {
                    bonus = 0;
                    row = i;
                } else {
                    row = i - gUbResult->score.baseCount;
                    bonus = 1;
                }
                while (!UbScore_CountLine(&gUbResult->score, bonus, row, 9999)) {
                }
            }
            gUbResult->pageDone = 1;
            gUbResult->page = gUbResult->pageCount;
            gUbResult->count = gUbResult->score.bonusCount + gUbResult->score.baseCount;
            gUbResult->page = gUbResult->pageCount;
            UbScore_SetPage(&gUbResult->score, gUbResult->page);
        }
        if (gUbResult->pageDone) {
            if (gUbResult->page < gUbResult->pageCount) {
                if (gUbResult->page == 0) {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_battlebonus_in", 1);
                } else {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_battlebonus_out_in", 1);
                }
                gUbResult->page++;
            } else {
                if (gUbResult->page == 0) {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_hyouka_rank_in", 1);
                } else {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_rank_in", 1);
                }
            }
            gUbResult->pageDone = 0;
        }
        if (gUbResult->count == gUbResult->score.bonusCount + gUbResult->score.baseCount) {
            gUbResult->step = UBRES_STEP_SHEET;
        }
        break;
    case UBRES_STEP_SHEET:
        if (up) {
            if (gUbResult->page > 0) {
                gUbResult->page--;
                if (gUbResult->page != 0) {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_battle_cansel_in", 1);
                } else {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_battlebonus_cancel", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (down) {
            if (gUbResult->page < gUbResult->pageCount) {
                gUbResult->page++;
                if (gUbResult->page == 0) {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_hyouka_cansel", 1);
                } else {
                    Flash_GotoLabel(&gUbResult->flash[0], "fl_battle_cansel_in", 1);
                }
                Snd_PlaySe(1, 0);
            }
        } else if (ok) {
            gUbResult->step = UBRES_STEP_PAY;
            gUbResult->count = 0;
            gUbResult->timer = 0x3C;
            if (gUbResult->page == 0) {
                Flash_GotoLabel(&gUbResult->flash[0], "fl_hyoukakarazpoint_trig", 1);
            } else {
                Flash_GotoLabel(&gUbResult->flash[0], "fl_zpoint_trig", 1);
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case UBRES_STEP_PAY:
        switch (gUbResult->count) {
        case 0:
            if (gUbResult->timer == 0) {
                gUbResult->count++;
                UbScore_PlateGoto(&gUbResult->flash[0], 0, 1);
                gUbResult->score.shown = gUbResult->score.score;
                gUbResult->talkStep = 14;
            }
            break;
        case 1:
            if (UbScore_ConvertStep(&gUbResult->score, 0x40)) {
                gUbResult->count++;
                gUbResult->timer = 15;
                UbScore_PlateGoto(&gUbResult->flash[0], 0, 0);
                gUbResult->score.points = gUbResult->score.shown;
            }
            break;
        case 2:
            if (gUbResult->timer == 0) {
                gUbResult->count++;
                UbScore_PlateGoto(&gUbResult->flash[0], 1, 1);
            }
            break;
        case 3:
            if (UbScore_Transfer(&gUbResult->score.points, &UO_SAVE->money, 0x40, 1.0f)) {
                gUbResult->count++;
                UbScore_PlateGoto(&gUbResult->flash[0], 1, 0);
            }
            if (UO_SAVE->money > UO_MONEY_MAX) {
                UO_SAVE->money = UO_MONEY_MAX;
            }
            break;
        case 4:
            if (ok) {
                Flash_GotoLabel(&gUbResult->flash[0], "fl_zpoint_out", 1);
                if (gUbResult->gotItem) {
                    gUbResult->step = UBRES_STEP_ITEM;
                } else {
                    gUbResult->flags |= UBRES_DONE;
                    gUbResult->flags |= UBRES_LEAVING;
                    gUbResult->timer = 15;
                    gUbResult->step = UBRES_STEP_LEAVE;
                }
                Snd_PlaySe(1, 1);
            }
            break;
        }
        if (gUbResult->count >= 1 && gUbResult->count <= 3) {
            if (ok) {
                if (gUbResult->count < 2) {
                    while (!UbScore_ConvertStep(&gUbResult->score, UO_MONEY_MAX)) {
                    }
                    gUbResult->score.points = gUbResult->score.shown;
                    while (!UbScore_Transfer(&gUbResult->score.points, &UO_SAVE->money, UO_MONEY_MAX, 1.0f)) {
                    }
                } else if (gUbResult->count < 4) {
                    while (!UbScore_Transfer(&gUbResult->score.points, &UO_SAVE->money, UO_MONEY_MAX, 1.0f)) {
                    }
                }
                if (UO_SAVE->money > UO_MONEY_MAX) {
                    UO_SAVE->money = UO_MONEY_MAX;
                }
                UbScore_PlateGoto(&gUbResult->flash[0], 0, 0);
                UbScore_PlateGoto(&gUbResult->flash[0], 1, 0);
                gUbResult->count = 4;
            }
        }
        break;
    case UBRES_STEP_ITEM:
        if (ok) {
            Flash_GotoLabel(&gUbResult->flash[0], "fl_itemget_out", 1);
            gUbResult->flags |= UBRES_DONE;
            gUbResult->flags |= UBRES_LEAVING;
            gUbResult->timer = 15;
            gUbResult->step = UBRES_STEP_LEAVE;
            Snd_PlaySe(1, 1);
        }
        break;
    case UBRES_STEP_LEAVE:
        break;
    }
}

/* The result screen of modes 27 and 30; always returns 1. */
s32 UbResult_Run(s32 section) {
    s32 result = 1;

    UbResult_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        UbResult_Update();
        UbResult_UpdateTalk();
        UbResult_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gUbResult->flags & UBRES_FADED_IN) && (gUbResult->flash[0].flags & MFLASH_PAD)) {
                gUbResult->flags |= UBRES_FADED_IN;
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
        if (gUbResult->flags & UBRES_LEAVING) {
            if (gUbResult->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gUbResult->talkStep == 0) {
            UbResult_Input(&result);
        }
    }
    UbResult_Term();
    Dma_ResetBuffers();
    return result;
}
