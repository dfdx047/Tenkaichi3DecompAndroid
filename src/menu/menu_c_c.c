#include "common.h"
#include "menu/menu_c.h"
#include "sys/pad.h"
#include "sys/save.h"

HistResult *gHistResult = NULL; /* 0x3B1300 */

/*
 * HistResult, 0x3405A0..0x341E08: the result screen after a story-mode battle (progress mode 8). An object of
 * its own: its read-only data (0x3B3720..0x3B38D4) repeats "fl_out" of the previous object. A win marks the
 * episode cleared, shows the points of the chosen level counting up and being paid into the money, then one
 * GetWin window per reward; a defeat only shows the caption.
 *
 * state: 1..5 the point count (2 count up by 33 a frame, 3 wait for confirm, 4 pay out by 33 a frame);
 * 51..57 the reward windows (52 set up and open / next, 53 wait, 54 confirm, 55 close, 56 wait, 57 play "fl_out").
 * seState: 1 -> 2 after the fade in, 2 wait for the jingle to end, 3 start the menu music 0x10B18.
 *
 * Reward kinds (GetWin_Setup): 0 character, 1 stage, 2 item, 7 sub menu, 8 episode, 9 dragon ball.
 */

/* Splits an episode number counted through all sub menus into sub menu and item (the inverse of ModeMenu_GetLine). */
void HistResult_SplitEpisode(s32 id, s32 *menu, s32 *item) {
    s32 count[8] = { 3, 4, 7, 5, 16, 5, 4, 4 };
    s32 i;

    switch (id) {
    case 20:
        id = 23;
        break;
    case 21:
        id = 20;
        break;
    case 22:
        id = 21;
        break;
    case 23:
        id = 22;
        break;
    }
    *menu = 0;
    *item = 0;
    for (i = 0; i < 8; i++) {
        if (id < count[i]) {
            break;
        }
        (*menu)++;
        id -= count[i];
    }
    *item = id;
}

#define HR_ADD(k, v) \
    gHistResult->reward[gHistResult->rewardCount].kind = (k); \
    gHistResult->reward[gHistResult->rewardCount].id = (v); \
    gHistResult->rewardCount++

/* Lists what the won battle gives (gProgress->reward, filled when the battle was set up) and writes it to the
   save: items, characters, stages, newly listed sub menus and episodes, and the dragon ball the battle result
   reports (BattleResult.unk44, 1-based). The points are doubled while item 0x88 is owned. */
void HistResult_BuildRewards(void) {
    s32 unlock[3];
    s32 menu;
    s32 item;
    HistProgress *prog = HPROG;
    HistReward *rw = &prog->reward;
    s32 i;
    s32 j;
    s32 id;

    memset(gHistResult->reward, 0, 0x88);
    gHistResult->rewardCount = 0;
#ifdef PORT
    /* PC build: the loop below looks through all of unlock[] for the sub menu before it has set the later entries
       (an original bug: it reads what happens to be on the stack). On the console that is never the sub menu's
       number; here it can be, and then a battle that opens the first episode of a new saga does not list the saga:
       won battles against Vegeta left the Frieza Saga's first battle open in a saga that was not on the list
       (reported twice, reproduced 2026-10-08). */
    unlock[0] = unlock[1] = unlock[2] = -1;
#endif
    for (i = 0; i < 3; i++) {
        unlock[i] = -1;
        id = prog->reward.episode[i];
        if (id >= 0) {
            HistResult_SplitEpisode(id, &menu, &item);
            if (!(MSAVE->slot[menu].flags & MSLOT_LISTED)) {
                for (j = 0; j < 3; j++) {
                    if (unlock[j] == menu) {
                        break;
                    }
                }
                if (j == 3) {
                    unlock[i] = menu;
                }
            }
        }
    }
    if (MSAVE->item[0x88] & 1) {
        gHistResult->pointsTotal = rw->points[HPROG->level] * 2;
    } else {
        gHistResult->pointsTotal = rw->points[HPROG->level];
    }
    for (i = 0; i < 3; i++) {
        id = rw->item[i];
        if (id >= 0 && !(MSAVE->item[id] & 1)) {
            HR_ADD(2, id);
            Save_AddItem(id);
        }
    }
    for (i = 0; i < 3; i++) {
        id = rw->chara[i];
        if (id >= 0 && !(s32)((MSAVE->charaBits[id >> 6] >> (id - ((id >> 6) << 6))) & 1)) {
            HR_ADD(0, id);
            MSAVE->charaBits[id >> 6] |= 1LL << (id - ((id >> 6) << 6));
        }
    }
    for (i = 0; i < 3; i++) {
        id = rw->stage[i];
        if (id >= 0 && !(s32)((MSAVE->stageBits >> id) & 1)) {
            HR_ADD(1, id);
            MSAVE->stageBits |= 1LL << id;
        }
    }
    for (i = 0; i < 3; i++) {
        id = unlock[i];
        if (id >= 0 && !(MSAVE->slot[id].flags & MSLOT_LISTED)) {
            HR_ADD(7, id);
            MSAVE->slot[id].flags |= MSLOT_LISTED;
            MSAVE->slot[id].flags |= MSLOT_NEW;
            MSAVE->unlockFlags |= MUNLOCK_HIST_NEW_SAGA;
        }
    }
    for (i = 0; i < 3; i++) {
        id = rw->episode[i];
        if (id >= 0) {
            HistResult_SplitEpisode(id, &menu, &item);
            if (!(MSAVE->slot[menu].val[0] & (s32)(1U << item))) {
                HR_ADD(8, id);
                MSAVE->slot[menu].val[0] |= (s32)(1U << item);
                MSAVE->slot[menu].val[2] |= (s32)(1U << item);
                MSAVE->slot[menu].flags |= MSLOT_NEW_EPISODE;
            }
        }
    }
    id = BattleResult_GetPtr()->unk44 - 1;
    if (id >= 0) {
        HR_ADD(9, id);
        MSAVE->unlockFlags |= (s32)(1U << id);
    }
}

/* Whether every episode of the current sub menu has its "cleared" bit. */
s32 HistResult_IsMenuCleared(void) {
    s32 count[8] = { 3, 4, 7, 5, 16, 5, 4, 4 };
    s32 result = 1;
    s32 i;

    for (i = 0; i < count[gProgress->subMenu]; i++) {
        if (!(MSAVE->slot[gProgress->subMenu].val[1] & (s32)(1U << i))) {
            result = 0;
            break;
        }
    }
    return result;
}

#define HR_RES(pack, n) \
    res = (MTexRes *)MPACK_AT(pack, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads the screen, picks the picture, starts the jingle and, after a win, lists the rewards. */
void HistResult_Init(s32 section) {
    MTexRes *res = NULL;
    s32 flags;
    s32 bg;

    flags = BattleResult_GetFlags();
    gHistResult = Heap_Alloc(0x140, 0x20, 0, 2);
    memset(gHistResult, 0, 0x140);
    gHistResult->pack = (u32 *)MPACK_AT(gMenuArc2, section);
    gHistResult->res = Sprite_Unpack(gHistResult->pack, NULL, NULL);
    gHistResult->file = File_LoadSync(gProgress->baseFile + gProgress->subMenu + 0x10, NULL, 0);
    gHistResult->fileRes = Sprite_Unpack(gHistResult->file, NULL, NULL);
    if (flags & 1) {
        bg = Rand_Range(3);
        MSAVE->slot[gProgress->subMenu].val[1] |= (s32)(1U << gProgress->subMenuItem);
    } else {
        bg = 3;
        gHistResult->flags |= HISTRESULT_LOST;
    }
    gHistResult->bgFile = File_LoadSync(bg + DISC_RESOURCE(0x3FE, 8), NULL, 0);
    gHistResult->bgRes = Sprite_Unpack(gHistResult->bgFile, NULL, NULL);
    res = gHistResult->bgRes;
    Res_RelocateOffsets(&res, res, res);
    switch (bg) {
    case 0:
        gHistResult->tex[10] = MTEX(res, 0);
        gHistResult->tex[9] = MTEX(res, 1);
        break;
    case 1:
        gHistResult->tex[11] = MTEX(res, 0);
        break;
    case 2:
        gHistResult->tex[12] = MTEX(res, 0);
        break;
    case 3:
        gHistResult->tex[13] = MTEX(res, 0);
        break;
    }
    HR_RES(gHistResult->res, 1);
    gHistResult->bg = res;
    HR_RES(gHistResult->fileRes, 2);
    gHistResult->tex[4] = MTEX(res, 1);
    HR_RES(gHistResult->res, 2);
    gHistResult->tex[3] = MTEX(res, 0);
    gHistResult->tex[0] = MTEX(res, 1);
    gHistResult->tex[1] = MTEX(res, 2);
    gHistResult->tex[8] = MTEX(res, 3);
    gHistResult->tex[5] = MTEX(res, 4);
    gHistResult->tex[7] = MTEX(res, 5);
    HR_RES(gHistResult->res, 3);
    gHistResult->tex[6] = MTEX(res, 0);
    gHistResult->tex[2] = MTEX(res, 1);
    Flash_Create(&gHistResult->flash[0], MPACK_AT(gHistResult->res, 4), gHistResult->tex);
    Flash_Play(&gHistResult->flash[0], 1);
    GetWin_Init(MPACK_AT(gHistResult->res, 5), 1);
    if (gHistResult->flags & HISTRESULT_LOST) {
        gHistResult->pointsTotal = 0;
        StreamSe_PlayPausedDefault(0, 0x10BC8);
    } else {
        if (HistResult_IsMenuCleared() && !(MSAVE->slot[gProgress->subMenu].flags & MSLOT_OUTRO_SEEN)) {
            StreamSe_PlayPausedDefault(0, 0x10BC9);
        } else {
            StreamSe_PlayPausedDefault(0, 0x10BC7);
        }
        HistResult_BuildRewards();
    }
}

#define HR_FREE(p) \
    if ((p) != NULL) { \
        Heap_Free(p); \
        (p) = NULL; \
    }

/* Frees the screen. */
void HistResult_Term(void) {
    s32 i;

    GetWin_Term();
    for (i = 0; i < HISTRESULT_FLASH_NUM; i++) {
        Flash_Destroy(&gHistResult->flash[i]);
    }
    HR_FREE(gHistResult->bgRes);
    HR_FREE(gHistResult->bgFile);
    HR_FREE(gHistResult->fileRes);
    HR_FREE(gHistResult->file);
    HR_FREE(gHistResult->res);
    HR_FREE(gHistResult);
}

/* Sets up the clips (win / lose caption, point counters, scrolling lines, episode title) and draws. */
void HistResult_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    MFlash *flash;

    Sprite_DrawPicture(gHistResult->bg, 0, 0, 0x80);
    flash = &gHistResult->flash[0];
    for (i = 0; i < 2; i++) {
        sprintf(name, "mc_result_text_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i == 0) {
            if (gHistResult->flags & HISTRESULT_LOST) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                uv.x0 = 0;
                uv.y0 = 0;
                uv.x1 = 0x200;
                uv.y1 = 0x80;
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
        } else {
            if (gHistResult->flags & HISTRESULT_LOST) {
                uv.x0 = 0;
                uv.y0 = 0x80;
                uv.x1 = 0x200;
                uv.y1 = 0x100;
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
    }
    for (i = 0; i < 2; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x200;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_point_text_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    Num_Draw(flash, "mc_get_point_%d", 0, 4, gHistResult->points, 0x40, 0x40, 0);
    Num_Draw(flash, "mc_total_point_%d", 0, 7, MSAVE->money, 0x40, 0x40, 0);
    {
        f32 speed[4] = { 17.066667f, 17.066667f, 8.5333333f, 8.5333333f };

        uv.x0 = 0;
        uv.y0 = 0;
        uv.x1 = 0x200;
        uv.y1 = 0x100;
        for (i = 0; i < 4; i++) {
            sprintf(name, "mc_line_%d", i + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 0x20, 1);
            FlashAnim_Scroll(flash, &ref, &uv, &gHistResult->scroll[i], NULL, -speed[i], 0.0f);
        }
    }
    uv.x0 = 0;
    uv.y0 = (gProgress->subMenuItem % 4) * 0x40;
    uv.x1 = 0x200;
    uv.y1 = uv.y0 + 0x40;
    Flash_FindLabel(flash, NULL, "mc_battle_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, gProgress->subMenuItem / 4);
    for (i = 0; i < HISTRESULT_FLASH_NUM; i++) {
        Flash_Draw(&gHistResult->flash[i]);
    }
    GetWin_Draw();
}

/* Steps the movie; when the "fl_out" animation has ended, leaves. */
void HistResult_Update(void) {
    s32 i;

    for (i = 0; i < HISTRESULT_FLASH_NUM; i++) {
        Flash_Advance(&gHistResult->flash[i]);
    }
    if (gHistResult->flags & HISTRESULT_OUT) {
        if (gHistResult->flash[0].trig & 1) {
            gHistResult->flags |= HISTRESULT_CHOSEN;
            gHistResult->flags |= HISTRESULT_LEAVING;
            gHistResult->timer = 1;
            gHistResult->flags ^= HISTRESULT_OUT;
        }
    }
}

/* Pad 0: starts the point count once the movie accepts input; confirm afterwards leaves through the rewards. */
void HistResult_Input(s32 *result) {
    if (!(gHistResult->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gHistResult->flags & HISTRESULT_STARTED)) {
        gHistResult->flags |= HISTRESULT_STARTED;
        if (!(gHistResult->flags & HISTRESULT_LOST)) {
            gHistResult->state = 1;
            return;
        }
    }
    if (gHistResult->unk8C != 0) {
        return;
    }
    if (gPad[0].gamePressed & 0x200) {
        if (gHistResult->flags & HISTRESULT_LOST) {
            *result = 0;
        } else if (HistResult_IsMenuCleared()) {
            if (MSAVE->slot[gProgress->subMenu].flags & MSLOT_OUTRO_SEEN) {
                *result = 1;
            } else {
                *result = 2;
            }
        } else {
            *result = 1;
        }
        gHistResult->state = 0x33;
        Snd_PlaySe(1, 1);
    }
}

/* The point count and the reward windows, one step per frame. */
void HistResult_UpdateState(void) {
    if (gHistResult->state == 0) {
        return;
    }
    if (gHistResult->seState != 0) {
        return;
    }
    switch (gHistResult->state) {
    case 1:
        gHistResult->state++;
        break;
    case 2:
        if (gPad[0].gamePressed & 0x200) {
            gHistResult->points = gHistResult->pointsTotal;
            gHistResult->state++;
            Snd_PlaySe(1, 1);
        } else {
            gHistResult->points += 0x21;
            if (gHistResult->points >= gHistResult->pointsTotal) {
                gHistResult->points = gHistResult->pointsTotal;
                gHistResult->state++;
            }
        }
        break;
    case 4:
        if (gPad[0].gamePressed & 0x200) {
            Save_AddMoney(gHistResult->points);
            gHistResult->points = 0;
            gHistResult->state++;
            Snd_PlaySe(1, 1);
        } else if (gHistResult->points <= 0x20) {
            Save_AddMoney(gHistResult->points);
            gHistResult->points = 0;
            gHistResult->state++;
        } else {
            gHistResult->points -= 0x21;
            Save_AddMoney(0x21);
        }
        break;
    case 3:
        if (gPad[0].gamePressed & 0x200) {
            gHistResult->state++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 5:
        gHistResult->state = 0;
        break;
    case 51:
        if (gHistResult->rewardCount == 0) {
            gHistResult->state = 57;
        } else {
            gHistResult->state++;
            gHistResult->rewardIdx = 0;
        }
        break;
    case 52:
        GetWin_Setup(gHistResult->reward[gHistResult->rewardIdx].kind, gHistResult->reward[gHistResult->rewardIdx].id);
        if (gHistResult->rewardIdx != 0) {
            GetWin_Next();
        } else {
            GetWin_Open();
        }
        gHistResult->rewardIdx++;
        gHistResult->state++;
        break;
    case 53:
    case 56:
        if (GetWin_IsAnimating()) {
            gHistResult->state++;
        }
        break;
    case 54:
        if (gPad[0].gamePressed & 0x200) {
            if (gHistResult->rewardIdx == gHistResult->rewardCount) {
                gHistResult->state++;
            } else {
                gHistResult->state = 52;
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case 55:
        gHistResult->state++;
        GetWin_Close();
        break;
    case 57:
        gHistResult->state = 0;
        gHistResult->flags |= HISTRESULT_OUT;
        Flash_GotoLabel(&gHistResult->flash[0], "fl_out", 1);
        break;
    }
}

/* The jingle (started paused by Init, resumed after the fade in), then the menu music. */
void HistResult_UpdateSe(void) {
    if (gHistResult->seState == 0) {
        return;
    }
    switch (gHistResult->seState) {
    case 1:
        gHistResult->seState = 2;
        break;
    case 2:
        if (StreamSe_GetStat(0) == 5) {
            gHistResult->seState++;
        }
        break;
    case 3:
        Bgm_Play(DISC_RESOURCE(0x10B18, 0x87D));
        gHistResult->seState = 0;
        break;
    }
}

/* The result screen's frame loop. Returns 0 after a defeat, 1 after a win, 2 when the win completed the sub menu. */
s32 HistResult_Run(s32 section) {
    s32 result = 1;

    HistResult_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            HistResult_Update();
            HistResult_UpdateSe();
            HistResult_UpdateState();
        }
        HistResult_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gHistResult->flags & HISTRESULT_FADED_IN)) {
                gHistResult->flags |= HISTRESULT_FADED_IN;
                gHistResult->seState = 1;
                StreamSe_Resume(0);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gHistResult->flags & HISTRESULT_LEAVING) {
            if (--gHistResult->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gHistResult->seState == 0 && gHistResult->state == 0 && !(gHistResult->flags & HISTRESULT_OUT)) {
            HistResult_Input(&result);
        }
    }
    HistResult_Term();
    Dma_ResetBuffers();
    return result;
}
