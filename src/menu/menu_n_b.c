#include "common.h"
#include "menu/menu_n.h"
#include "sys/pad.h"

/*
 * UbzSel, 0x372560..0x373A68: the course select of mode 28 (five courses; each is a list of up to eight opponents
 * that are fought one after the other). One object: its .data is the work pointer 0x3B7350, its .rodata runs
 * from 0x3B7AE0 (sUbzFoeTex) to 0x3B7C70.
 */

UbzSel *gUbzSel = NULL; /* 0x3B7350 */

/*
 * This file saw Snd_PlaySe as a function returning a value (an implicit declaration, presumably): UbzSel_Input
 * only matches that way ($v0 stays reserved behind each call).
 */
#define Snd_PlaySe ((s32 (*)(u32, s32))Snd_PlaySe)

/* Movie image records that show the small pictures of the course's opponents. */
static const s32 sUbzFoeTex[8] = { 15, 18, 19, 20, 21, 22, 23, 24 };

#define UZ_RES(n) \
    res = (MTexRes *)MPACK_AT(gUbzSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/*
 * The battle hand-off of the courses, first half: the rules and the opponent pool of the chosen course. The
 * player's side is filled in after the character select (0x379A10), which also finishes the set-up.
 */
void UbzSel_SetupBattle(void) {
    u16 items[8];
    NCourse *course = &gUbzSel->course[gUbzSel->cursor[0]];
    s32 unk4 = course->unk4 != 0;
    s32 unk14 = course->unk14 != 0;
    s32 announcer = course->announcer;
    s32 timeLimit = course->timeLimit;
    s32 stage = course->stage;
    s32 bgm = course->bgm;
    s32 count;
    s32 i;

    if (announcer == N_RANDOM) {
        announcer = Rand_Range(8);
    }
    if (stage == N_RANDOM) {
        stage = Rand_Range(0x23);
    }
    if (bgm == N_RANDOM) {
        bgm = N_BGM_RANDOM;
    }
    count = 0;
    for (i = 0; i < 8; i++) {
        if (gUbzSel->foe[course->foe[i]].chara != N_NONE) {
            count++;
        } else {
            break;
        }
    }
    Battle_ClearWork();
    BattleSetup_SetRule(0, 3, bgm, timeLimit, announcer, stage, unk4);
    BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
    BattleSetup_SetSide(1, 2, 1, 1, unk14, 1, 0, NULL);
    BattleSetup_SetMember(1, 0, gUbzSel->foe[course->foe[0]].chara, 0, 0, gUbzSel->foe[course->foe[0]].cpuLevel, 100.0f,
                          NULL);
    for (i = 0; i < count; i++) {
        NFoe *foe = &gUbzSel->foe[course->foe[i]];
        u16 *dst;
        s32 j;

        memset(items, 0, sizeof(items));
        dst = items;
        for (j = 0; j < 7; j++) {
            s32 item = foe->item[j];

            if (item != N_NONE) {
                *dst++ = item + 1;
            }
        }
        if (foe->lastItem == N_NONE) {
            items[7] = 0;
        } else {
            items[7] = foe->lastItem + 1;
        }
        BattleSetup_SetPoolMember(count, i, foe->chara, foe->color, 0, foe->cpuLevel, 100.0f, items);
    }
    NPROG->teamSize = 1;
    NPROG->dpRule = 0;
}

/* Loads the screen's file, creates the movie, the icon window and the text boxes of the course names. */
void UbzSel_Init(void) {
    MTexRes *res = NULL;
    s32 i;

    gUbzSel = Heap_Alloc(sizeof(UbzSel), 0x20, 0, 2);
    memset(gUbzSel, 0, sizeof(UbzSel));
    gUbzSel->file = File_LoadSync(gProgress->baseFile + 0x1C, NULL, 0);
    gUbzSel->res = Sprite_Unpack(gUbzSel->file, NULL, NULL);
    gUbzSel->course = (NCourse *)MPACK_AT(gUbzSel->res, 8);
    gUbzSel->foe = (NFoe *)MPACK_AT(gUbzSel->res, 9);
    UZ_RES(4);
    gUbzSel->bg = res;
    UZ_RES(5);
    gUbzSel->tex[32] = MTEX(res, 0);
    gUbzSel->tex[33] = MTEX(res, 1);
    gUbzSel->tex[34] = MTEX(res, 3);
    UZ_RES(3);
    gUbzSel->tex[0] = MTEX(res, 0);
    gUbzSel->tex[1] = MTEX(res, 1);
    gUbzSel->tex[16] = MTEX(res, 2);
    gUbzSel->tex[17] = MTEX(res, 3);
    gUbzSel->tex[13] = MTEX(res, 4);
    gUbzSel->tex[12] = MTEX(res, 5);
    gUbzSel->tex[14] = MTEX(res, 6);
    gUbzSel->tex[30] = MTEX(res, 7);
    gUbzSel->tex[29] = MTEX(res, 8);
    gUbzSel->tex[8] = MTEX(res, 9);
    gUbzSel->tex[31] = MTEX(res, 10);
    gUbzSel->tex[25] = MTEX(res, 11);
    gUbzSel->tex[3] = MTEX(res, 12);
    gUbzSel->tex[2] = MTEX(res, 13);
    gUbzSel->tex[28] = MTEX(res, 14);
    gUbzSel->tex[27] = MTEX(res, 15);
    gUbzSel->tex[4] = MTEX(res, 16);
    gUbzSel->tex[5] = MTEX(res, 17);
    gUbzSel->tex[7] = MTEX(res, 18);
    gUbzSel->tex[11] = MTEX(res, 19);
    gUbzSel->tex[10] = MTEX(res, 20);
    gUbzSel->tex[6] = MTEX(res, 21);
    gUbzSel->tex[35] = MTEX(res, 22);
    gUbzSel->tex[36] = MTEX(res, 23);
    gUbzSel->tex[37] = MTEX(res, 24);
    gUbzSel->tex[26] = NULL;
    gUbzSel->tex[9] = NULL;
    gUbzSel->tex[15] = NULL;
    gUbzSel->tex[18] = NULL;
    gUbzSel->tex[19] = NULL;
    gUbzSel->tex[20] = NULL;
    gUbzSel->tex[21] = NULL;
    gUbzSel->tex[22] = NULL;
    gUbzSel->tex[23] = NULL;
    gUbzSel->tex[24] = NULL;
    gUbzSel->chips = (u32 *)MPACK_AT(gUbzSel->res, 6);
    #ifdef PORT
    for (i = 0; i < gUbzSel->chips[0] && i < 250; i++) {
#else
    for (i = 0; i < N_CHIP_NUM; i++) {
#endif
        res = (MTexRes *)MPACK_AT(gUbzSel->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    Flash_Create(&gUbzSel->flash[0], MPACK_AT(gUbzSel->res, 1), gUbzSel->tex);
    Flash_Play(&gUbzSel->flash[0], 1);
    gUbzSel->blink = Rand_Libc() % 32;
    UZ_RES(2);
    IconWin_Init(MPACK_AT(gUbzSel->res, 7), res);
    IconWin_Open();
    gUbzSel->text = MPACK_AT(gUbzSel->res, 10);
    for (i = 0; i < UBZSEL_BOX_NUM; i++) {
        TextBox_Init(&gUbzSel->box[i], gUbzSel->text, 0);
        TextBox_SetUnk50(&gUbzSel->box[i], 0);
    }
    gUbzSel->voiceLine = -1;
    gUbzSel->voiceSkip = 0;
}

/* Frees everything Init made. */
void UbzSel_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < UBZSEL_FLASH_NUM; i++) {
        Flash_Destroy(&gUbzSel->flash[i]);
    }
    if (gUbzSel->res != NULL) {
        Heap_Free(gUbzSel->res);
        gUbzSel->res = NULL;
    }
    if (gUbzSel->file != NULL) {
        Heap_Free(gUbzSel->file);
        gUbzSel->file = NULL;
    }
    if (gUbzSel != NULL) {
        Heap_Free(gUbzSel);
        gUbzSel = NULL;
    }
}

/* Draws the screen: the guide, the five course plates with their best rank, the chosen course's sheet. */
void UbzSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;

    Sprite_DrawPicture(gUbzSel->bg, 0, 0, 0x80);
    flash = &gUbzSel->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gUbzSel->blink, 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gUbzSel->talk, 0);
    for (i = 0; i < N_COURSE_NUM; i++) {
        sprintf(name, "mc_window_plate%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_window_text1_0", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, i + 0x143, &gUbzSel->box[i]);
        uv.x0 = gSaveData->course[i].rank * 0x20;
        uv.y0 = 0;
        uv.x1 = uv.x0 + 0x20;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, name, "mc_rankmoji", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, "mc_mission_plate01", "mc_mission_plate_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < 8; i++) {
        MTexRes *res;
        s32 chara = gUbzSel->foe[gUbzSel->course[gUbzSel->cursor[0]].foe[i]].chara;

        if (chara == N_NONE) {
            res = (MTexRes *)MPACK_AT(gUbzSel->chips, N_CHIP_NUM);
        } else {
            res = (MTexRes *)MPACK_AT(gUbzSel->chips, chara + 1);
        }
        gUbzSel->tex[sUbzFoeTex[i]] = MTEX(res, 0);
    }
    uv.x0 = gSaveData->course[gUbzSel->cursor[0]].rank * 0x40;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_rank", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_time_suji%02d", 4, 2, gSaveData->course[gUbzSel->cursor[0]].time[0], 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 2, 2, gSaveData->course[gUbzSel->cursor[0]].time[1], 0x20, 0x20, 1);
    Num_Draw(flash, "mc_time_suji%02d", 0, 2, gSaveData->course[gUbzSel->cursor[0]].time[2], 0x20, 0x20, 1);
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_time_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_Draw(flash, "mc_score_suji%02d", 0, 9, gSaveData->course[gUbzSel->cursor[0]].score * 100, 0x20, 0x20, 0);
    for (i = 0; i < UBZSEL_FLASH_NUM; i++) {
        Flash_Draw(&gUbzSel->flash[i]);
    }
    IconWin_Draw();
}

/* Advances the leave timer and the movie. */
void UbzSel_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gUbzSel->timer > 0) {
        gUbzSel->timer--;
    }
    for (i = 0; i < UBZSEL_FLASH_NUM; i++) {
        Flash_Advance(&gUbzSel->flash[i]);
    }
}

/* The guide: starts the requested line; while one plays, confirm cuts it short. */
void UbzSel_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (!gUbzSel->voiceSkip) {
        if (gUbzSel->voiceReq == 0) {
            return;
        }
        if (gUbzSel->voiceLine != -1 && Voice_GetStat(0) != N_VOICE_IDLE && gUbzSel->voiceReq == gUbzSel->voicePrev) {
            if (gPad[0].gamePressed & 0x200) {
                Snd_PlaySe(1, 1);
                gUbzSel->voiceSkip = 1;
            }
            return;
        }
    } else {
        gUbzSel->voiceSkip = 0;
    }
    switch (gUbzSel->voiceReq) {
    case 1:
        gUbzSel->voiceLine = 0x33;
        break;
    case 2:
        gUbzSel->voiceLine = 0x35;
        break;
    }
    gUbzSel->voiceReq = 0;
    Voice_PlayWithSubtitle(gUbzSel->subtitles, N_VOICE_BASE, gUbzSel->voiceLine);
    gUbzSel->voicePrev = gUbzSel->voiceReq;
}

/* Pad 0: up / down the courses, confirm opens the course's sheet and confirms it, cancel goes back. */
void UbzSel_Input(s32 *result) {
    char name[64];
    s32 up = gPad[0].gameRepeat & 8;
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 cancel = gPad[0].gamePressed & 0x400;
    s32 down = gPad[0].gameRepeat & 4;

    if (gUbzSel->flash[0].flags & MFLASH_PAD) {
        if (!(gUbzSel->flags & UBZSEL_STARTED)) {
            UbzSel_ClipGoto(0, 0, "fl_on_start");
            gUbzSel->flags |= UBZSEL_STARTED;
        }
        switch (gUbzSel->step) {
        case 0:
            if (up) {
                UbzSel_ClipGoto(0, 0, "fl_off_start");
                if (--gUbzSel->cursor[gUbzSel->step] < 0) {
                    gUbzSel->cursor[gUbzSel->step] = N_COURSE_NUM - 1;
                }
                UbzSel_ClipGoto(0, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
                gUbzSel->idle = 0;
            } else if (down) {
                UbzSel_ClipGoto(0, 0, "fl_off_start");
                if (++gUbzSel->cursor[gUbzSel->step] >= N_COURSE_NUM) {
                    gUbzSel->cursor[gUbzSel->step] = 0;
                }
                UbzSel_ClipGoto(0, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
                gUbzSel->idle = 0;
            } else if (ok) {
                UbzSel_ClipGoto(0, 0, "fl_rank_out");
                sprintf(name, "fl_menu%d_in", gUbzSel->cursor[gUbzSel->step]);
                Flash_GotoLabel(&gUbzSel->flash[0], name, 1);
                gUbzSel->step = 1;
                Snd_PlaySe(1, 1);
                gUbzSel->idle = 0;
            } else if (cancel) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                *result = 0;
                Flash_GotoLabel(&gUbzSel->flash[0], "fl_menu_cansel", 1);
                Snd_PlaySe(1, 2);
                gUbzSel->idle = 0;
            } else {
                gUbzSel->idle++;
                if (gUbzSel->idle == N_IDLE_FRAMES) {
                    gUbzSel->voiceReq = 2;
                    gUbzSel->idle = 0;
                }
            }
            break;
        case 1:
            if (ok) {
                gUbzSel->flags |= UBZSEL_CHOSEN;
                gUbzSel->flags |= UBZSEL_LEAVING;
                gUbzSel->timer = 15;
                NPROG->ubChoice = gUbzSel->cursor[0];
                Snd_PlaySe(1, 1);
            } else if (cancel) {
                gUbzSel->step = 0;
                UbzSel_ClipGoto(0, 0, "fl_rank_in");
                sprintf(name, "fl_menu%d_cansel", gUbzSel->cursor[gUbzSel->step]);
                Flash_GotoLabel(&gUbzSel->flash[0], name, 1);
                Snd_PlaySe(1, 2);
            }
            break;
        }
    }
}

/* Sends the plate of cursor[step] in one of the movies to a label. */
void UbzSel_ClipGoto(s32 movie, s32 step, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gUbzSel->flash[movie];

    sprintf(name, "mc_window_plate%02d", gUbzSel->cursor[step] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * Mode 28: the course select. Returns 1 when a course was chosen (gProgress->ubChoice; the battle's rules and
 * opponent pool are then set up), 0 when the player backed out.
 */
s32 UbzSel_Run(void) {
    s32 result = 1;

    UbzSel_Init();
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        UbzSel_Update();
        UbzSel_UpdateVoice();
        UbzSel_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gUbzSel->flags & UBZSEL_FADED_IN) && (gUbzSel->flash[0].flags & MFLASH_PAD)) {
                gUbzSel->flags |= UBZSEL_FADED_IN;
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
        if (gUbzSel->flags & UBZSEL_LEAVING) {
            if (gUbzSel->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gUbzSel->voiceReq == 0) {
            UbzSel_Input(&result);
        }
    }
    if (result) {
        UbzSel_SetupBattle();
    }
    UbzSel_Term();
    Dma_ResetBuffers();
    return result;
}
