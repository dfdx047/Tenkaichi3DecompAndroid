#include "common.h"
#include "menu/menu_r.h"
#include "sys/pad.h"

/*
 * SimTop, 0x387880..0x388EE0: the entry screen of the "sim" ladder (mode 20, run by SimTop_Run 0x388D70): three
 * plates, the ranking list, the how-to-play text. One object, written as two halves and merged here: the first
 * four functions (0x387880..0x388618) and the former src/menu/menu_s.c (0x388618..0x388EE0, the last five).
 * .data 0x3B7420..0x3B743C: the work pointer, a padding word (0x3B7424: the compiler aligns the table to 8; no
 * code refers to that address) and the table gSimTopFaceTex at 0x3B7428. The object's .rodata starts at 0x3B9ED0
 * with the three constant tables below (used by the second half), which are in front of the first string
 * (0x3B9EF0 "mc_icon_star"), so they are file-scope constants defined above the functions; the strings of the
 * first half end at 0x3BA050, where the jump table of SimTop_UpdateVoice follows; strings 0x3BA068..0x3BA138.
 */

/* The screen's work area (.data, 0x3B7420). */
SimTop *gSimTop = NULL;

/* SimTop.tex slots of the portraits of the five ranking rows shown (.data, 0x3B7428). */
s32 gSimTopFaceTex[5] = { 20, 22, 23, 24, 25 };

/* 0x3B9ED0, not referenced (26 = SIMTOP_TEX_CURSOR_FACE, the cursor row's texture slot). */
const s32 gSimTopUnused[1] = { 26 };
/* 0x3B9ED8: the guide's line for each plate. */
const s32 gSimTopPlateVoice[SIMTOP_ROWS] = { 1, 2, 3 };
/* 0x3B9EE8: the guide's two idle lines. */
const s32 gSimTopIdleVoice[2] = { 4, 5 };

/* Shows or hides the star of ranking plate `parent`. */
void SimTop_ShowStar(char *parent, s32 on) {
    MFlashRef ref;
    MFlash *flash = &gSimTop->flash[0];

    Flash_FindLabel(flash, parent, "mc_icon_star", &ref);
    if (on) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
}

/*
 * Loads the screen's section of archive 3 and resets the ladder kept in gProgress: level 0, attack 0, defence 0,
 * health 100 %, points 0, no items, turn 0, one fighter, no DP rule.
 */
void SimTop_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gSimTop = Heap_Alloc(sizeof(SimTop), 0x20, 0, 2);
    memset(gSimTop, 0, sizeof(SimTop));
    gSimTop->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gSimTop->res = Sprite_Unpack(gSimTop->pack, NULL, NULL);

    res = (MTexRes *)MPACK_AT(gSimTop->res, 4);
    Res_RelocateOffsets(&res, res, res);
    gSimTop->bg = res;

    res = (MTexRes *)MPACK_AT(gSimTop->res, 5);
    Res_RelocateOffsets(&res, res, res);
    gSimTop->tex[32] = MTEX(res, 5);
    gSimTop->tex[33] = MTEX(res, 6);
    gSimTop->tex[34] = MTEX(res, 8);

    res = (MTexRes *)MPACK_AT(gSimTop->res, 2);
    Res_RelocateOffsets(&res, res, res);
    gSimTop->tex[20] = NULL;
    gSimTop->tex[22] = NULL;
    gSimTop->tex[23] = NULL;
    gSimTop->tex[24] = NULL;
    gSimTop->tex[25] = NULL;
    gSimTop->tex[26] = NULL;
    gSimTop->tex[31] = NULL;
    gSimTop->tex[0] = MTEX(res, 0);
    gSimTop->tex[1] = MTEX(res, 1);
    gSimTop->tex[11] = MTEX(res, 2);
    gSimTop->tex[18] = MTEX(res, 3);
    gSimTop->tex[16] = MTEX(res, 4);
    gSimTop->tex[15] = MTEX(res, 5);
    gSimTop->tex[28] = MTEX(res, 6);
    gSimTop->tex[27] = MTEX(res, 7);
    gSimTop->tex[30] = MTEX(res, 8);
    gSimTop->tex[19] = MTEX(res, 9);
    gSimTop->tex[21] = MTEX(res, 10);
    gSimTop->tex[13] = MTEX(res, 11);
    gSimTop->tex[14] = MTEX(res, 12);
    gSimTop->tex[3] = MTEX(res, 13);
    gSimTop->tex[2] = MTEX(res, 14);
    gSimTop->tex[7] = MTEX(res, 15);
    gSimTop->tex[10] = MTEX(res, 16);
    gSimTop->tex[9] = MTEX(res, 17);
    gSimTop->tex[8] = MTEX(res, 18);
    gSimTop->tex[12] = MTEX(res, 19);
    gSimTop->tex[17] = MTEX(res, 20);
    gSimTop->tex[4] = MTEX(res, 21);
    gSimTop->tex[5] = MTEX(res, 22);
    gSimTop->tex[6] = MTEX(res, 23);
    gSimTop->tex[29] = MTEX(res, 24);
    gSimTop->tex[35] = MTEX(res, 25);
    gSimTop->tex[36] = MTEX(res, 26);
    gSimTop->tex[37] = MTEX(res, 27);
    gSimTop->tex[38] = MTEX(res, 28);
    gSimTop->tex[39] = MTEX(res, 29);

    gSimTop->faces = (u32 *)MPACK_AT(gSimTop->res, 7);
    #ifdef PORT
    for (i = 0; i < gSimTop->faces[0] && i < 250; i++) {
#else
    for (i = 0; i < SIMTOP_FACE_NUM; i++) {
#endif
        res = (MTexRes *)MPACK_AT(gSimTop->faces, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }

    Flash_Create(&gSimTop->flash[0], MPACK_AT(gSimTop->res, 1), gSimTop->tex);
    Flash_Play(&gSimTop->flash[0], 1);

    res = (MTexRes *)MPACK_AT(gSimTop->res, 3);
    Res_RelocateOffsets(&res, res, res);
    IconWin_Init(MPACK_AT(gSimTop->res, 6), res);
    IconWin_Open();

    gSimTop->text = MPACK_AT(gSimTop->res, 8);
    gSimTop->subtitles = MPACK_AT(gSimTop->res, 9);
    TextBox_Init(&gSimTop->box, gSimTop->text, 0);
    TextBox_SetUnk50(&gSimTop->box, 0);
    TextBox_SetSpacing(&gSimTop->box, 0, 4);

    gSimTop->voiceLine = -1;
    gSimTop->top = 0;
    gSimTop->cursor = 0;
    gSimTop->voiceSkip = 0;

    SIM_PROG->run.turn = 0;
    SIM_PROG->run.level = 0;
    SIM_PROG->run.stat[SIM_STAT_ATK] = 0;
    SIM_PROG->run.stat[SIM_STAT_DEF] = 0;
    SIM_PROG->run.stat[SIM_STAT_HP] = 100;
    SIM_PROG->run.stat[SIM_STAT_POINT] = 0;
    for (i = 0; i < SIM_ITEM_NUM; i++) {
        SIM_PROG->run.item[i] = -1;
    }
    SIM_PROG->run.itemHead = 0;
    SIM_PROG->run.wait = 0;
    SIM_PROG->run.off = 0;
    SIM_PROG->teamSize = 1;
    SIM_PROG->dpRule = 0;
}

/* Frees the screen. */
void SimTop_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < SIMTOP_FLASH_NUM; i++) {
        Flash_Destroy(&gSimTop->flash[i]);
    }
    if (gSimTop->res != NULL) {
        Heap_Free(gSimTop->res);
        gSimTop->res = NULL;
    }
    if (gSimTop != NULL) {
        Heap_Free(gSimTop);
        gSimTop = NULL;
    }
}

/* Sets up the movie's clips (ranking rows, portraits, the description) and draws it. */
void SimTop_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[0x40];
    MFlash *flash;
    s32 i;
    s32 x;
    s32 n;
    s32 score;
    s32 star;
    s32 chara;
    MTexRes *face;

    Sprite_DrawPicture(gSimTop->bg, 0, 0, 0x80);
    flash = &gSimTop->flash[0];
    Flash_FindLabel(flash, "mc_guide_18go", "mc_guide_18go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gSimTop->blink, 0);
    Flash_FindLabel(flash, "mc_guide_18go", "mc_guide_18go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gSimTop->mouth, 0);

    x = gSimTop->iconFrame << 6;
    uv.x0 = x;
    uv.y0 = 0;
    uv.x1 = x + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_icon_play1", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    gSimTop->iconFrame ^= 1;

    for (i = 1; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i << 5;
        uv.x1 = 0x200;
        uv.y1 = (i << 5) + 0x20;
        sprintf(name, "mc_sim_plate%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_friend_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_friend_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x20;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_menu_yajirusi_up", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gSimTop->top == 0) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }

    uv.x0 = 0x20;
    uv.y0 = 0x20;
    uv.x1 = 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_menu_yajirusi_down", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gSimTop->top == 5) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }

    for (i = 0; i < 6; i++) {
        sprintf(name, "mc_senreki_plate%02d", i);
        if (i != 5) {
            n = gSimTop->top + i + 1;
        } else {
            n = gSimTop->cursor + 1;
        }
        uv.x0 = (n % 4) << 6;
        uv.y0 = (n / 4) << 6;
        uv.x1 = ((n % 4) << 6) + 0x40;
        uv.y1 = ((n / 4) << 6) + 0x40;
        Flash_FindLabel(flash, name, "mc_jyuni_suji", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_jyuni_suji_ef", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);

        if (i != 5) {
            chara = SIM_SAVE->rank[gSimTop->top + i].chara;
            face = (MTexRes *)MPACK_AT(gSimTop->faces, chara + 1);
            gSimTop->tex[gSimTopFaceTex[i]] = face->tex;
        } else {
            chara = SIM_SAVE->rank[gSimTop->cursor].chara;
            face = (MTexRes *)MPACK_AT(gSimTop->faces, chara + 1);
            gSimTop->tex[SIMTOP_TEX_CURSOR_FACE] = face->tex;
        }
        if (i != 5) {
            score = SIM_SAVE->rank[gSimTop->top + i].score;
        } else {
            score = SIM_SAVE->rank[gSimTop->cursor].score;
        }
        Num_DrawChild(flash, name, "mc_pt_suji%02d", 0, 9, score * 100, 0x20, 0x20, 0, 0);
        if (i != 5) {
            star = SIM_SAVE->rank[gSimTop->top + i].cleared;
        } else {
            star = SIM_SAVE->rank[gSimTop->cursor].cleared;
        }
        SimTop_ShowStar(name, star);

        uv.x0 = 0;
        uv.y0 = 0;
        uv.x1 = 0x20;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, name, "mc_icon_star", &ref);
        FlashAnim_Sheet(flash, &ref, &gSimTop->starTimer, &gSimTop->starFrame, &uv, 2, 2, 0x80);
    }

    Flash_FindLabel(flash, NULL, "mc_setumeikari", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gSimTop->page, &gSimTop->box);
    Flash_FindLabel(flash, NULL, "mc_episode_next_2", &ref);
    if (gSimTop->page >= 10) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }
    Flash_FindLabel(flash, NULL, "mc_episode_next", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);

    for (i = 0; i < SIMTOP_FLASH_NUM; i++) {
        Flash_Draw(&gSimTop->flash[i]);
    }
    IconWin_Draw();
}

/*
 * ---- The second half of the object (formerly src/menu/menu_s.c, 0x388618..0x388EE0) ----
 * Read-only data emitted here: the jump table of SimTop_UpdateVoice (0x3BA050) and the strings 0x3BA068..0x3BA138.
 * "mc_sim_plate%02d" (SimTop_ClipGoto) is shared with SimTop_Draw's copy at 0x3B9F50 (compiled alone the second
 * half emitted it once more behind its last string).
 */

/* Runs the leave timer and the movie. */
void SimTop_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSimTop->timer > 0) {
        gSimTop->timer--;
    }
    for (i = 0; i < SIMTOP_FLASH_NUM; i++) {
        Flash_Advance(&gSimTop->flash[i]);
    }
}

/* Starts the line the guide was asked for; confirm cuts the running line short. */
void SimTop_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSimTop->voiceSkip == 0) {
        if (gSimTop->voiceReq == 0) {
            return;
        }
        if (gSimTop->voiceLine != -1 && Voice_GetStat(0) != SIM_VOICE_IDLE && gSimTop->voiceReq == gSimTop->voiceLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                gSimTop->voiceSkip = 1;
            }
            return;
        }
    } else {
        gSimTop->voiceSkip = 0;
    }
    switch (gSimTop->voiceReq) {
    case 1:
        gSimTop->voiceLine = 0x11;
        break;
    case 2:
        gSimTop->voiceLine = 0x12;
        break;
    case 3:
        gSimTop->voiceLine = 0x13;
        break;
    case 4:
        gSimTop->voiceLine = 0x14;
        break;
    case 5:
        gSimTop->voiceLine = 0x15;
        break;
    }
    gSimTop->voiceReq = 0;
    Voice_PlayWithSubtitle(gSimTop->subtitles, SIM_VOICE_BASE, gSimTop->voiceLine);
    gSimTop->voiceLast = gSimTop->voiceReq;
}

/*
 * Pad 0. Level 0: up / down the plate, confirm opens it (plate 0 starts a run), cancel leaves; level 1 scrolls
 * the ranking; level 2 pages through the "how to play" text.
 */
void SimTop_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 right = gPad[0].gameRepeat & PADG_RIGHT;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;
    s32 cancel = gPad[0].gamePressed & PADG_TRIANGLE;

    if (!(gSimTop->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gSimTop->flags & SIMTOP_STARTED)) {
        SimTop_ClipGoto(0, 0, "fl_on_start");
        gSimTop->voiceReq = 1;
        gSimTop->flags |= SIMTOP_STARTED;
    }
    switch (gSimTop->level) {
    case SIMTOP_LV_MENU:
        if (up) {
            SimTop_ClipGoto(0, 0, "fl_off_start");
            if (--gSimTop->cur[gSimTop->level] < 0) {
                gSimTop->cur[gSimTop->level] = SIMTOP_ROWS - 1;
            }
            SimTop_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimTop->voiceReq = gSimTopPlateVoice[gSimTop->cur[gSimTop->level]];
            gSimTop->idle = 0;
        } else if (down) {
            SimTop_ClipGoto(0, 0, "fl_off_start");
            if (++gSimTop->cur[gSimTop->level] >= SIMTOP_ROWS) {
                gSimTop->cur[gSimTop->level] = 0;
            }
            SimTop_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimTop->voiceReq = gSimTopPlateVoice[gSimTop->cur[gSimTop->level]];
            gSimTop->idle = 0;
        } else if (confirm) {
            switch (gSimTop->cur[0]) {
            case 0:
                gSimTop->flags |= SIMTOP_CHOSEN;
                gSimTop->flags |= SIMTOP_LEAVING;
                gSimTop->timer = 15;
                break;
            case 1:
                gSimTop->level = SIMTOP_LV_RANKING;
                Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_in", 1);
                break;
            case 2:
                gSimTop->level = SIMTOP_LV_HELP;
                gSimTop->page = 0;
                Flash_GotoLabel(&gSimTop->flash[0], "fl_asobikata_plate_in", 1);
                break;
            }
            Voice_StopWithLip();
            Snd_PlaySe(1, 1);
            gSimTop->idle = 0;
        } else if (cancel) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&gSimTop->flash[0], "fl_sim_plate_cansel", 1);
            Snd_PlaySe(1, 2);
        } else {
            gSimTop->idle++;
            if (gSimTop->idle == SIMTOP_IDLE_FRAMES) {
                gSimTop->idle = 0;
                gSimTop->voiceReq = gSimTopIdleVoice[Rand_Range(2)];
            }
        }
        break;
    case SIMTOP_LV_RANKING:
        if (up && gSimTop->top != 0) {
            Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_down", 1);
            gSimTop->top--;
            gSimTop->cursor = gSimTop->top + 5;
            Snd_PlaySe(1, 0);
        } else if (down && (u32)gSimTop->top < SIMTOP_RANK_TOP_MAX) {
            Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_up", 1);
            gSimTop->top++;
            gSimTop->cursor = gSimTop->top - 1;
            Snd_PlaySe(1, 0);
        } else if (cancel) {
            gSimTop->level = SIMTOP_LV_MENU;
            Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_cansel", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case SIMTOP_LV_HELP:
        if (confirm || right) {
            if (gSimTop->page < SIMTOP_HELP_PAGES) {
                gSimTop->page++;
            } else {
                gSimTop->level = SIMTOP_LV_MENU;
                Flash_GotoLabel(&gSimTop->flash[0], "fl_asobikata_plate_cansel", 1);
            }
            Snd_PlaySe(1, 1);
        } else if (cancel) {
            gSimTop->level = SIMTOP_LV_MENU;
            Flash_GotoLabel(&gSimTop->flash[0], "fl_asobikata_plate_cansel", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* Sends the plate under the cursor of `level` to a label. */
void SimTop_ClipGoto(s32 movie, s32 level, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gSimTop->flash[movie];

    sprintf(name, "mc_sim_plate%02d", gSimTop->cur[level] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * The entry screen of the sim sub family (mode 20). Returns 1 when "start" was chosen (Ub_Main goes on to the
 * character select, mode 21), 0 when the player backed out (mode 13).
 */
s32 SimTop_Run(s32 section) {
    s32 result = 1;

    SimTop_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        SimTop_Update();
        SimTop_UpdateVoice();
        SimTop_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gSimTop->flags & SIMTOP_GREETED) && (gSimTop->flash[0].flags & MFLASH_PAD)) {
                gSimTop->flags |= SIMTOP_GREETED;
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
        if (gSimTop->flags & SIMTOP_LEAVING) {
            if (gSimTop->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gSimTop->voiceReq == 0) {
            SimTop_Input(&result);
        }
    }
    SimTop_Term();
    Dma_ResetBuffers();
    return result;
}
