#include "common.h"
#include "menu/menu_s.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work area (.data, 0x3B743C). */
SurvSel *gSurvSel = NULL;

/*
 * SurvSel, 0x388EE0..0x38A308: the course select of the survival sub family (mode 17 of Ub_Main), a whole
 * object. Work pointer gSurvSel (0x3B743C); read-only data 0x3BA138..0x3BA300 (strings only). The screen is a
 * sibling of the mission select (MisSel, src/menu/menu_o_d.c + menu_p.c): same pack layout, same guide (Android
 * 17), three courses instead of pages of five missions.
 */

void SurvSel_ClipGoto(s32 movie, s32 level, char *label);

/*
 * The battle hand-off of a survival course, up to the opponents: rule (battle mode 3), the player's side as one
 * pad-controlled fighter still to be chosen, side 1 as one CPU fighter backed by a pool of fifty opponents from
 * the course table. The player's fighter is added by Ub_SetupSolo after the character select (mode 18), whose
 * team size (1) and DP rule (none) are left in gProgress.
 */
void SurvSel_SetupBattle(void) {
    u16 items[8];
    SurvCourse *def = &gSurvSel->courses[gSurvSel->cur[0]];
    s32 unk10;
    s32 unk1FC;
    s32 announcer;
    s32 timeLimit;
    s32 stage;
    s32 bgm;
    s32 i;

    unk10 = def->unk4 != 0;
    unk1FC = def->unk14 != 0;
    announcer = def->announcer;
    timeLimit = def->timeLimit;
    stage = def->stage;
    bgm = def->bgm;
    if (announcer == SURV_RANDOM) {
        announcer = Rand_Range(8);
    }
    if (stage == SURV_RANDOM) {
        stage = Rand_Range(0x23);
    }
    if (bgm == SURV_RANDOM) {
        bgm = 0x18;
    }
    Battle_ClearWork();
    BattleSetup_SetRule(0, 3, bgm, timeLimit, announcer, stage, unk10);
    BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
    BattleSetup_SetSide(1, 2, 1, 1, unk1FC, 1, 0, NULL);
    BattleSetup_SetMember(1, 0, 0, 0, 0, 0, 100.0f, NULL);
    for (i = 0; i < SURV_OPP_NUM; i++) {
        SurvOpp *opp = &gSurvSel->opps[def->opp[i]];
        s32 chara = opp->chara;
        s32 n;
        s32 j;

        chara = (chara == SURV_RANDOM) ? gSurvSel->randomChara[Rand_Range(SURV_RANDOM_NUM)] : chara;
        memset(items, 0, sizeof(items));
        n = 0;
        for (j = 0; j < 7; j++) {
            s32 id = opp->item[j];

            if (id != SURV_NONE) {
                items[n++] = id + 1;
            }
        }
        if (opp->item7 == SURV_NONE) {
            items[7] = 0;
        } else {
            items[7] = opp->item7 + 1;
        }
        BattleSetup_SetPoolMember(SURV_OPP_NUM, i, chara, opp->costume, 0, opp->cpuLevel, 100.0f, items);
    }
    S_PROG->teamSize = 1;
    S_PROG->dpRule = 0;
}

#define SS_RES(n) \
    res = (MTexRes *)MPACK_AT(gSurvSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 3) and builds its movie. */
void SurvSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gSurvSel = Heap_Alloc(sizeof(SurvSel), 0x20, 0, 2);
    memset(gSurvSel, 0, sizeof(SurvSel));
    gSurvSel->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gSurvSel->res = Sprite_Unpack(gSurvSel->pack, NULL, NULL);
    gSurvSel->courses = (SurvCourse *)MPACK_AT(gSurvSel->res, 12);
    gSurvSel->randomChara = (s32 *)MPACK_AT(gSurvSel->res, 14);
    gSurvSel->opps = (SurvOpp *)MPACK_AT(gSurvSel->res, 13);
    SS_RES(4);
    gSurvSel->bg = res;
    SS_RES(5);
    gSurvSel->tex[23] = MTEX(res, 0);
    gSurvSel->tex[24] = MTEX(res, 1);
    gSurvSel->tex[25] = MTEX(res, 3);
    SS_RES(3);
    gSurvSel->tex[0] = MTEX(res, 0);
    gSurvSel->tex[1] = MTEX(res, 1);
    gSurvSel->tex[14] = MTEX(res, 5);
    gSurvSel->tex[13] = MTEX(res, 6);
    gSurvSel->tex[15] = MTEX(res, 7);
    gSurvSel->tex[21] = MTEX(res, 8);
    gSurvSel->tex[20] = MTEX(res, 10);
    gSurvSel->tex[8] = MTEX(res, 11);
    gSurvSel->tex[22] = MTEX(res, 12);
    gSurvSel->tex[16] = MTEX(res, 13);
    gSurvSel->tex[3] = MTEX(res, 14);
    gSurvSel->tex[2] = MTEX(res, 15);
    gSurvSel->tex[19] = MTEX(res, 16);
    gSurvSel->tex[18] = MTEX(res, 17);
    gSurvSel->tex[4] = MTEX(res, 18);
    gSurvSel->tex[5] = MTEX(res, 19);
    gSurvSel->tex[7] = MTEX(res, 20);
    gSurvSel->tex[12] = MTEX(res, 21);
    gSurvSel->tex[11] = MTEX(res, 22);
    gSurvSel->tex[6] = MTEX(res, 23);
    gSurvSel->tex[26] = MTEX(res, 25);
    gSurvSel->tex[27] = MTEX(res, 26);
    gSurvSel->tex[28] = MTEX(res, 27);
    gSurvSel->tex[9] = NULL;
    gSurvSel->tex[10] = NULL;
    gSurvSel->tex[17] = NULL;
    Flash_Create(&gSurvSel->flash[0], MPACK_AT(gSurvSel->res, 2), gSurvSel->tex);
    Flash_Play(&gSurvSel->flash[0], 1);
    gSurvSel->blink = Rand_Libc() % 32;
    SS_RES(9);
    IconWin_Init(MPACK_AT(gSurvSel->res, 7), res);
    IconWin_Open();
    gSurvSel->text = MPACK_AT(gSurvSel->res, 18);
    gSurvSel->subtitles = MPACK_AT(gSurvSel->res, 17);
    for (i = 0; i < 4; i++) {
        TextBox_Init(&gSurvSel->box[i], gSurvSel->text, 0);
        TextBox_SetUnk50(&gSurvSel->box[i], 0);
    }
    gSurvSel->voiceLine = -1;
    gSurvSel->voiceSkip = 0;
}

/* Frees the screen. */
void SurvSel_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < SURVSEL_FLASH_NUM; i++) {
        Flash_Destroy(&gSurvSel->flash[i]);
    }
    if (gSurvSel->res != NULL) {
        Heap_Free(gSurvSel->res);
        gSurvSel->res = NULL;
    }
    if (gSurvSel != NULL) {
        Heap_Free(gSurvSel);
        gSurvSel = NULL;
    }
}

/* Draws the three courses with their rank letters and the record of the course under the cursor. */
void SurvSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;
    s32 n;
    s32 t0, t1, t2;
    s32 rank;

#ifdef PORT
    Port_MenuPage(35); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gSurvSel->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gSurvSel->bg, 0, 0, 0x80);
#endif
    flash = &gSurvSel->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gSurvSel->blink, 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gSurvSel->talk, 0);
    for (i = 0; i < SURV_COURSE_NUM; i++) {
        sprintf(name, "mc_window_plate%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, i + 0x13C, &gSurvSel->box[i]);
        rank = S_SAVE->surv[i].rank;
        uv.x0 = rank * 0x20;
        uv.y0 = 0;
        uv.x1 = uv.x0 + 0x20;
        uv.y1 = 0x20;
        sprintf(name, "mc_window_plate%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_rankmoji", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (i == gSurvSel->cur[0]) {
            rank = S_SAVE->surv[i].rank;
            uv.x0 = rank * 0x40;
            uv.y0 = 0;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = 0x40;
            Flash_FindLabel(flash, NULL, "mc_rank", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
        }
    }
    for (i = SURV_COURSE_NUM; i < 5; i++) {
        sprintf(name, "mc_window_plate%02d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
    Flash_FindLabel(flash, NULL, "mc_missionkari01", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, 0x150, &gSurvSel->box[3]);
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, "mc_mission_plate01", "mc_misson_plate_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    n = S_SAVE->surv[gSurvSel->cur[0]].defeated;
    uv.x0 = 0;
    uv.y0 = 0x60;
    uv.x1 = 0x100;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, NULL, "mc_gekiha_text01", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_gekiha_suji%02d", 0, 3, n, 0x20, 0x20, 0);
    t1 = S_SAVE->surv[gSurvSel->cur[0]].minutes;
    t0 = S_SAVE->surv[gSurvSel->cur[0]].hours;
    t2 = S_SAVE->surv[gSurvSel->cur[0]].seconds;
    Num_Draw(flash, "mc_time_suji%02d", 4, 2, t0, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 2, 2, t1, 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 0, 2, t2, 0x20, 0x20, 1);
    n = S_SAVE->surv[gSurvSel->cur[0]].score * 100;
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_time_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_score_suji%02d", 0, 9, n, 0x20, 0x20, 0);
    Flash_Draw(&gSurvSel->flash[0]);
    IconWin_Draw();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Runs the leave timer and the movie. */
void SurvSel_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSurvSel->timer > 0) {
        gSurvSel->timer--;
    }
    for (i = 0; i < SURVSEL_FLASH_NUM; i++) {
        Flash_Advance(&gSurvSel->flash[i]);
    }
}

/* Starts the line the guide was asked for; confirm cuts the running line short. */
void SurvSel_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSurvSel->voiceSkip == 0) {
        if (gSurvSel->voiceReq == 0) {
            return;
        }
        if (gSurvSel->voiceLine != -1 && Voice_GetStat(0) != S_VOICE_IDLE &&
            gSurvSel->voiceReq == gSurvSel->voiceLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                gSurvSel->voiceSkip = 1;
            }
            return;
        }
    } else {
        gSurvSel->voiceSkip = 0;
    }
    switch (gSurvSel->voiceReq) {
    case 1:
        gSurvSel->voiceLine = 0x34;
        break;
    case 2:
        gSurvSel->voiceLine = 0x35;
        break;
    }
    gSurvSel->voiceReq = 0;
    Voice_PlayWithSubtitle(gSurvSel->subtitles, S_VOICE_BASE, gSurvSel->voiceLine);
    gSurvSel->voiceLast = gSurvSel->voiceReq;
}

/* Pad 0. Level 0: up / down the course, confirm opens its window; level 1: confirm or back. */
void SurvSel_Input(s32 *result) {
    char name[64];
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;
    s32 cancel = gPad[0].gamePressed & PADG_TRIANGLE;

    if (!(gSurvSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gSurvSel->flags & SURVSEL_STARTED)) {
        gSurvSel->voiceReq = 1;
        SurvSel_ClipGoto(0, 0, "fl_on_start");
        gSurvSel->flags |= SURVSEL_STARTED;
    }
    switch (gSurvSel->level) {
    case 0:
        if (up) {
            SurvSel_ClipGoto(0, 0, "fl_off_start");
            if (--gSurvSel->cur[gSurvSel->level] < 0) {
                gSurvSel->cur[gSurvSel->level] = SURV_COURSE_NUM - 1;
            }
            SurvSel_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSurvSel->idle = 0;
        } else if (down) {
            SurvSel_ClipGoto(0, 0, "fl_off_start");
            if (++gSurvSel->cur[gSurvSel->level] >= SURV_COURSE_NUM) {
                gSurvSel->cur[gSurvSel->level] = 0;
            }
            SurvSel_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSurvSel->idle = 0;
        } else if (confirm) {
            SurvSel_ClipGoto(0, 0, "fl_rank_out");
            sprintf(name, "fl_survival_%02d_in", gSurvSel->cur[gSurvSel->level] + 1);
            Flash_GotoLabel(&gSurvSel->flash[0], name, 1);
            gSurvSel->level = 1;
            Snd_PlaySe(1, 1);
            gSurvSel->idle = 0;
        } else if (cancel) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&gSurvSel->flash[0], "fl_furvival_menu_cancel", 1);
            Snd_PlaySe(1, 2);
            gSurvSel->idle = 0;
        } else {
            gSurvSel->idle++;
            if (gSurvSel->idle == SURVSEL_IDLE_FRAMES) {
                gSurvSel->voiceReq = 2;
                gSurvSel->idle = 0;
            }
        }
        break;
    case 1:
        if (confirm) {
            gSurvSel->flags |= SURVSEL_CHOSEN;
            gSurvSel->flags |= SURVSEL_LEAVING;
            gSurvSel->timer = 15;
            S_PROG->cursor = gSurvSel->cur[0];
            Snd_PlaySe(1, 1);
        } else if (cancel) {
            gSurvSel->level = 0;
            SurvSel_ClipGoto(0, 0, "fl_rank_in");
            sprintf(name, "fl_survival_%02d_cansel", gSurvSel->cur[gSurvSel->level] + 1);
            Flash_GotoLabel(&gSurvSel->flash[0], name, 1);
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* Sends the plate under the cursor of `level` to a label. */
void SurvSel_ClipGoto(s32 movie, s32 level, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gSurvSel->flash[movie];

    sprintf(name, "mc_window_plate%02d", gSurvSel->cur[level] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * The survival course select (mode 17). Returns 1 when a course was chosen (its row is then in
 * gProgress->cursor and the battle setup was written by SurvSel_SetupBattle), 0 when the player backed out.
 */
s32 SurvSel_Run(s32 section) {
    s32 result = 1;

    SurvSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        SurvSel_Update();
        SurvSel_UpdateVoice();
        SurvSel_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gSurvSel->flags & SURVSEL_GREETED) && (gSurvSel->flash[0].flags & MFLASH_PAD)) {
                gSurvSel->flags |= SURVSEL_GREETED;
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
        if (gSurvSel->flags & SURVSEL_LEAVING) {
            if (gSurvSel->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gSurvSel->voiceReq == 0) {
            SurvSel_Input(&result);
        }
    }
    if (result) {
        SurvSel_SetupBattle();
    }
    SurvSel_Term();
    Dma_ResetBuffers();
    return result;
}
