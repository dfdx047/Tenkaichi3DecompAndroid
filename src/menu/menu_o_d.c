#include "common.h"
#include "menu/menu_o.h"
#include "sys/pad.h"

/*
 * MisSel, 0x379F58..0x37B7C0: the mission list of mode 14. Work pointer gMisSel (0x3B7360), one initialised table
 * (gMisSelFaceTex, 0x3B7368), .rodata 0x3B8400..0x3B8630. Merged from two chunks: the head (0x379F58..0x37AFF8:
 * SetRank, SetupBattle, Init, Term, Draw; written as menu_o_d.c) and the last five functions (0x37AFF8..: Update,
 * UpdateVoice, Input, ClipGoto and the frame loop MisSel_Run at 0x37B640; written as menu_p.c with the MisSelP view
 * of include/menu/menu_p.h). The one view used now is MisSel / UoProgress / UoSave of include/menu/menu_o.h.
 */

MisSel *gMisSel = NULL; /* 0x3B7360 */
/*
 * The movie's texture slots of the five opponent chips. The word between the pointer and this table (0x3B7364) is
 * zero and nothing refers to it: it is the padding of the table's 8-byte alignment (the compiler aligns arrays
 * to 8).
 */
s32 gMisSelFaceTex[MISSEL_ROWS] = { 19, 22, 23, 24, 25 }; /* 0x3B7368..0x3B737C */

/* Shows a mission's rank letter on its row, and enlarged when the row is under the cursor. */
void MisSel_SetRank(s32 row, s32 rank) {
    MFlashRef ref;
    char name[64];
    MFlashUv uv;
    MFlash *flash = &gMisSel->flash[0];

    uv.x0 = rank * 0x20;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x20;
    uv.y1 = 0x20;
    sprintf(name, "mc_window_plate%02d", row + 1);
    Flash_FindLabel(flash, name, "mc_rankmoji", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (row == gMisSel->cursor[0]) {
        uv.x0 = rank * 0x40;
        uv.y0 = 0;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, NULL, "mc_rank", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
}

/*
 * The battle hand-off of a mission, up to the opponents: rule (battle mode 2) and side 1 (CPU team of up to five
 * from the mission tables). The player's side is added by Ub_SetupTeam after the character select, whose team
 * size and DP rule are left in gProgress.
 */
void MisSel_SetupBattle(void) {
    u16 items[8];
    s32 unk1FC;
    s32 unk10;
    MisSelDef *def = &gMisSel->missions[gMisSel->mission];
    s32 announcer;
    s32 timeLimit;
    s32 stage;
    s32 bgm;
    s32 teamSize;
    u32 count;
    s32 i;
    s32 j;
    MisSelOpp *opp;
    u16 *item;

    unk10 = def->unk4 != 0;
    unk1FC = def->unk14 != 0;
    announcer = def->announcer;
    timeLimit = def->timeLimit;
    stage = def->stage;
    bgm = def->bgm;
    if (announcer == MISSEL_RANDOM) {
        announcer = Rand_Range(8);
    }
    if (stage == MISSEL_RANDOM) {
        stage = Rand_Range(0x23);
    }
    if (bgm == MISSEL_RANDOM) {
        bgm = 0x18;
    }
    teamSize = 0;
    switch (def->kind) {
    case 0:
        teamSize = 1;
        break;
    case 1:
        teamSize = 2;
        break;
    case 2:
        teamSize = 3;
        break;
    case 3:
        teamSize = 4;
        break;
    case 6:
    case 7:
    case 10:
        teamSize = 5;
        break;
    }
    count = 0;
    for (i = 0; i < 5; i++) {
        if (gMisSel->opps[def->opp[i]].chara != MISSEL_NONE) {
            count++;
        } else {
            break;
        }
    }
    Battle_ClearWork();
    BattleSetup_SetRule(0, 2, bgm, timeLimit, announcer, stage, unk10);
    BattleSetup_SetSide(1, 2, 1, count, unk1FC, 1, 0, NULL);
    for (i = 0; i < count; i++) {
        opp = &gMisSel->opps[def->opp[i]];
        memset(items, 0, sizeof(items));
        item = items;
        for (j = 0; j < 7; j++) {
            s32 id = opp->item[j];

            if (id != MISSEL_NONE) {
                *item++ = id + 1;
            }
        }
        if (opp->item7 == MISSEL_NONE) {
            items[7] = 0;
        } else {
            items[7] = opp->item7 + 1;
        }
        BattleSetup_SetMember(1, i, opp->chara, opp->costume, 0, opp->cpuLevel, 100.0f, items);
    }
    UO_PROG->teamSize = teamSize;
    UO_PROG->dpRule = def->dpRule;
}

#define MS_RES(n) \
    res = (MTexRes *)MPACK_AT(gMisSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 3) and builds its movie. */
void MisSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gMisSel = Heap_Alloc(sizeof(MisSel), 0x20, 0, 2);
    memset(gMisSel, 0, sizeof(MisSel));
    gMisSel->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gMisSel->res = Sprite_Unpack(gMisSel->pack, NULL, NULL);
    gMisSel->missions = (MisSelDef *)MPACK_AT(gMisSel->res, 10);
    gMisSel->opps = (MisSelOpp *)MPACK_AT(gMisSel->res, 11);
    MS_RES(4);
    gMisSel->bg = res;
    MS_RES(5);
    gMisSel->tex[33] = MTEX(res, 0);
    gMisSel->tex[34] = MTEX(res, 1);
    gMisSel->tex[35] = MTEX(res, 3);
    MS_RES(3);
    gMisSel->tex[0] = MTEX(res, 0);
    gMisSel->tex[1] = MTEX(res, 1);
    gMisSel->tex[13] = MTEX(res, 2);
    gMisSel->tex[20] = MTEX(res, 3);
    gMisSel->tex[21] = MTEX(res, 4);
    gMisSel->tex[17] = MTEX(res, 5);
    gMisSel->tex[16] = MTEX(res, 6);
    gMisSel->tex[18] = MTEX(res, 7);
    gMisSel->tex[31] = MTEX(res, 8);
    gMisSel->tex[15] = MTEX(res, 9);
    gMisSel->tex[30] = MTEX(res, 10);
    gMisSel->tex[8] = MTEX(res, 11);
    gMisSel->tex[32] = MTEX(res, 12);
    gMisSel->tex[26] = MTEX(res, 13);
    gMisSel->tex[3] = MTEX(res, 14);
    gMisSel->tex[2] = MTEX(res, 15);
    gMisSel->tex[14] = MTEX(res, 16);
    gMisSel->tex[29] = MTEX(res, 17);
    gMisSel->tex[4] = MTEX(res, 18);
    gMisSel->tex[5] = MTEX(res, 19);
    gMisSel->tex[7] = MTEX(res, 20);
    gMisSel->tex[12] = MTEX(res, 21);
    gMisSel->tex[11] = MTEX(res, 22);
    gMisSel->tex[6] = MTEX(res, 23);
    gMisSel->tex[28] = MTEX(res, 24);
    gMisSel->tex[36] = MTEX(res, 25);
    gMisSel->tex[37] = MTEX(res, 26);
    gMisSel->tex[38] = MTEX(res, 27);
    gMisSel->tex[10] = NULL;
    gMisSel->tex[19] = NULL;
    gMisSel->tex[22] = NULL;
    gMisSel->tex[23] = NULL;
    gMisSel->tex[24] = NULL;
    gMisSel->tex[25] = NULL;
    gMisSel->tex[27] = NULL;
    gMisSel->chips = (u32 *)MPACK_AT(gMisSel->res, 15);
    #ifdef PORT
    for (i = 0; i < gMisSel->chips[0] && i < 250; i++) {
#else
    for (i = 0; i < MISSEL_CHIP_NUM; i++) {
#endif
        res = (MTexRes *)MPACK_AT(gMisSel->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    Flash_Create(&gMisSel->flash[0], MPACK_AT(gMisSel->res, 1), gMisSel->tex);
    Flash_Play(&gMisSel->flash[0], 1);
    gMisSel->blink = Rand_Libc() % 32;
    MS_RES(8);
    IconWin_Init(MPACK_AT(gMisSel->res, 7), res);
    IconWin_Open();
    gMisSel->text = MPACK_AT(gMisSel->res, 18);
    gMisSel->subtitles = MPACK_AT(gMisSel->res, 17);
    for (i = 0; i < 7; i++) {
        TextBox_Init(&gMisSel->box[i], gMisSel->text, 0);
        TextBox_SetUnk50(&gMisSel->box[i], 0);
        if (i < MISSEL_ROWS) {
            TextBox_SetMaxWidth(&gMisSel->box[i], 0xEB);
            TextBox_SetLineOffsets(&gMisSel->box[i], 0, -10, 0, 0, 0);
        }
    }
    gMisSel->voiceLine = -1;
    gMisSel->page = UO_PROG->missionPage;
    gMisSel->cursor[0] = UO_PROG->cursor;
    gMisSel->voiceSkip = 0;
    gMisSel->pageCount = UO_SAVE->missionPages;
}

/* Frees the screen. */
void MisSel_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < 1; i++) {
        Flash_Destroy(&gMisSel->flash[i]);
    }
    if (gMisSel->res != NULL) {
        Heap_Free(gMisSel->res);
        gMisSel->res = NULL;
    }
    if (gMisSel != NULL) {
        Heap_Free(gMisSel);
        gMisSel = NULL;
    }
}

/* Draws the list page, the details of the mission under the cursor and its opponents' chips. */
void MisSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;
    s32 cur;
    MTexRes *chip;

    Sprite_DrawPicture(gMisSel->bg, 0, 0, 0x80);
    flash = &gMisSel->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gMisSel->blink, 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gMisSel->talk, 0);
    Num_Draw(flash, "mc_pt_suji%02d", 0, 2, gMisSel->pageCount, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_pt_suji%02d", 2, 2, gMisSel->page + 1, 0x20, 0x20, 1);
    uv.x0 = 0x20;
    uv.y0 = 0;
    uv.x1 = 0x40;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, NULL, "mc_yajirusi_r", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gMisSel->page == 0) {
        Flash_FindLabel(flash, NULL, "mc_yajirusi_l", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    if (gMisSel->page == gMisSel->pageCount - 1) {
        Flash_FindLabel(flash, NULL, "mc_yajirusi_r", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    for (i = 0; i < MISSEL_ROWS; i++) {
        s32 n = gMisSel->page * MISSEL_ROWS + i;
        s32 rank = UO_SAVE->mission[n].rank;

        sprintf(name, "mc_window_plate%02d", i + 1);
        Num_DrawChild(flash, name, "mc_window_text_off%d", 0, 3, n + 1, 0x20, 0x20, 1, 0);
        Flash_FindLabel(flash, name, "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, n + 0xD8, &gMisSel->box[i]);
        MisSel_SetRank(i, rank);
    }
    cur = gMisSel->mission;
    Flash_FindLabel(flash, NULL, "mc_missionkari01", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gMisSel->missions[cur].kind + 0x148, &gMisSel->box[5]);
    Flash_FindLabel(flash, NULL, "mc_missionkari02", &ref);
    if (gMisSel->missions[cur].dpRule != 0) {
        TextBox_AttachLine(flash, &ref, 0, 0, gMisSel->missions[cur].dpRule + 0x156, &gMisSel->box[6]);
    }
    for (i = 0; i < MISSEL_ROWS; i++) {
        if (gMisSel->opps[gMisSel->missions[cur].opp[i]].chara == MISSEL_NONE) {
            chip = (MTexRes *)MPACK_AT(gMisSel->chips, MISSEL_CHIP_NUM);
        } else {
            chip = (MTexRes *)MPACK_AT(gMisSel->chips, gMisSel->opps[gMisSel->missions[cur].opp[i]].chara + 1);
        }
        gMisSel->tex[gMisSelFaceTex[i]] = chip->tex;
    }
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, "mc_mission_plate01", "mc_misson_plate_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    {
        s32 m = UO_SAVE->mission[cur].time1;
        s32 h = UO_SAVE->mission[cur].time0;
        s32 sec = UO_SAVE->mission[cur].time2;
        s32 score;

        Num_Draw(flash, "mc_time_suji%02d", 4, 2, h, 0x20, 0x20, 1);
        Num_Draw(flash, "mc_time_suji%02d", 2, 2, m, 0x20, 0x20, 1);
        Num_Draw(flash, "mc_time_suji%02d", 0, 2, sec, 0x20, 0x20, 1);
        score = UO_SAVE->mission[cur].score * 100;
        uv.x0 = 0;
        uv.y0 = 0x20;
        uv.x1 = 0x100;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, NULL, "mc_time_text02", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Num_Draw(flash, "mc_score_suji%02d", 0, 9, score, 0x20, 0x20, 0);
    }
    Flash_Draw(&gMisSel->flash[0]);
    IconWin_Draw();
}

/* Runs the leave timer and the movie. */
void MisSel_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gMisSel->timer > 0) {
        gMisSel->timer--;
    }
    for (i = 0; i < MISSEL_FLASH_NUM; i++) {
        Flash_Advance(&gMisSel->flash[i]);
    }
}

/* Starts the line the guide was asked for; confirm cuts the running line short. */
void MisSel_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gMisSel->voiceSkip == 0) {
        if (gMisSel->voiceReq == 0) {
            return;
        }
        if (gMisSel->voiceLine != -1 && Voice_GetStat(0) != UB_VOICE_IDLE_O && gMisSel->voiceReq == gMisSel->voiceLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                gMisSel->voiceSkip = 1;
            }
            return;
        }
    } else {
        gMisSel->voiceSkip = 0;
    }
    switch (gMisSel->voiceReq) {
    case 1:
        gMisSel->voiceLine = 0x33;
        break;
    case 2:
        gMisSel->voiceLine = 0x35;
        break;
    }
    gMisSel->voiceReq = 0;
    Voice_PlayWithSubtitle(gMisSel->subtitles, UB_VOICE_BASE_O, gMisSel->voiceLine);
    gMisSel->voiceLast = gMisSel->voiceReq;
}

/* Pad 0. Level 0: up / down the plate, left / right the page; level 1: confirm or back. */
void MisSel_Input(s32 *result) {
    char name[64];
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 left = gPad[0].gameRepeat & PADG_LEFT;
    s32 right = gPad[0].gameRepeat & PADG_RIGHT;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;
    s32 cancel = gPad[0].gamePressed & PADG_TRIANGLE;

    if (!(gMisSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gMisSel->flags & MISSEL_STARTED)) {
        gMisSel->voiceReq = 1;
        MisSel_ClipGoto(0, 0, "fl_on_start");
        gMisSel->flags |= MISSEL_STARTED;
    }
    switch (gMisSel->level) {
    case 0:
        if (up) {
            MisSel_ClipGoto(0, 0, "fl_off_start");
            if (--gMisSel->cursor[gMisSel->level] < 0) {
                gMisSel->cursor[gMisSel->level] = MISSEL_ROWS - 1;
            }
            MisSel_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gMisSel->idle = 0;
        } else if (down) {
            MisSel_ClipGoto(0, 0, "fl_off_start");
            if (++gMisSel->cursor[gMisSel->level] >= MISSEL_ROWS) {
                gMisSel->cursor[gMisSel->level] = 0;
            }
            MisSel_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gMisSel->idle = 0;
        } else if (left) {
            if (gMisSel->page != 0) {
                gMisSel->page--;
                Snd_PlaySe(1, 0);
                gMisSel->idle = 0;
            }
        } else if (right) {
            if ((u32)gMisSel->page < (u32)(gMisSel->pageCount - 1)) {
                gMisSel->page++;
                Snd_PlaySe(1, 0);
                gMisSel->idle = 0;
            }
        } else if (confirm) {
            gMisSel->mission = gMisSel->page * MISSEL_ROWS + gMisSel->cursor[0];
            MisSel_ClipGoto(0, 0, "fl_rank_out");
            sprintf(name, "fl_mission_%02d_in", gMisSel->cursor[gMisSel->level] + 1);
            Flash_GotoLabel(&gMisSel->flash[0], name, 1);
            gMisSel->level = 1;
            Snd_PlaySe(1, 1);
            gMisSel->idle = 0;
        } else if (cancel) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&gMisSel->flash[0], "fl_mission100_menu_cansel", 1);
            Snd_PlaySe(1, 2);
            gMisSel->idle = 0;
        } else {
            gMisSel->idle++;
            if (gMisSel->idle == MISSEL_IDLE_FRAMES) {
                gMisSel->voiceReq = 2;
                gMisSel->idle = 0;
            }
        }
        break;
    case 1:
        if (confirm) {
            gMisSel->flags |= MISSEL_CHOSEN;
            gMisSel->flags |= MISSEL_LEAVING;
            gMisSel->timer = 15;
            UO_PROG->missionPage = gMisSel->page;
            UO_PROG->cursor = gMisSel->cursor[0];
            Snd_PlaySe(1, 1);
        } else if (cancel) {
            gMisSel->level = 0;
            MisSel_ClipGoto(0, 0, "fl_rank_in");
            sprintf(name, "fl_mission_%02d_cansel", gMisSel->cursor[gMisSel->level] + 1);
            Flash_GotoLabel(&gMisSel->flash[0], name, 1);
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* Sends the plate under the cursor of `level` to a label. */
void MisSel_ClipGoto(s32 movie, s32 level, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gMisSel->flash[movie];

    sprintf(name, "mc_window_plate%02d", gMisSel->cursor[level] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * The mission select (mode 14). Returns 1 when a mission was chosen (its page and plate are then in gProgress and
 * the rule and the opponents were written to the battle setup by MisSel_SetupBattle), 0 when the player backed out.
 */
s32 MisSel_Run(s32 section) {
    s32 result = 1;

    MisSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        MisSel_Update();
        MisSel_UpdateVoice();
        MisSel_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gMisSel->flags & MISSEL_GREETED) && (gMisSel->flash[0].flags & MFLASH_PAD)) {
                gMisSel->flags |= MISSEL_GREETED;
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
        if (gMisSel->flags & MISSEL_LEAVING) {
            if (gMisSel->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gMisSel->voiceReq == 0) {
            MisSel_Input(&result);
        }
    }
    if (result) {
        MisSel_SetupBattle();
    }
    MisSel_Term();
    Dma_ResetBuffers();
    return result;
}
