#include "common.h"
#include "menu/menu_za.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work (overlay .data, 0x3BC9F8). */
ReplayMenu *gReplayMenu = NULL;

/*
 * Menu overlay DBZP.BIN, 0x3AF290..0x3B0C08: ReplayMenu, the replay list of the Data Center (progress mode 56).
 * A whole object: `.data` gReplayMenu (0x3BC9F8), `.rodata` 0x3BE3A8..0x3BE71C.
 *
 * The screen shows one of the seven replay files of the memory card at a time (its number and the two teams'
 * character chips) and has two uses, chosen by gProgress->replayFlags bit 0:
 *   - clear (entered from the Data Center menu): confirm loads the replay of the slot shown (McFlow mode 6) and
 *     leaves the overlay, which makes Game_Main run the battle with the loaded replay;
 *   - set (entered by Progress_Main after a battle that ended with reason bit 0x1000): confirm saves the
 *     battle's replay into the slot shown (McFlow mode 5), then the progress mode goes back to the character
 *     or team select (39 / 40).
 * Everything the card does is in the main executable's McFlow module; this file registers its per-mode
 * callbacks and turns their outcome into the screen's result.
 */

/* ReplayMenu_Draw has the project's first `double` constant (pow(10.0, n)): `li.d` is in include/gcc_prelude.inc. */

void ReplayMenu_SetPlayable(s32 on);

#define REPLAY_HOST "host:data/ps2/test/main/DC/Replay/"

/* Plays an animation of the "no data" plate. */
void ReplayMenu_PlayNoData(char *label) {
    MFlashRef ref;

    Flash_FindLabel(&gReplayMenu->flash[0], NULL, "mc_no_data", &ref);
    Flash_ClipGotoLabel(&gReplayMenu->flash[0], &ref, label);
}

/* Puts the chips of the two teams of the slot shown on the ten plates (two rows of five). */
void ReplayMenu_DrawChips(void) {
    MFlashRef ref;
    char name[0x100];
    s32 team;
    s32 i;

    for (team = 0; team < REPLAY_TEAM_NUM; team++) {
        for (i = 0; i < REPLAY_MEMBER_NUM; i++) {
            s32 chara = ZAPROG->replay[gReplayMenu->cursor].chara[team * REPLAY_MEMBER_NUM + i];
            MTexRes *res = (MTexRes *)MPACK_AT(gReplayMenu->chips, chara + 1);
            s32 slot = i + team * REPLAY_MEMBER_NUM;

            gReplayMenu->chip[team][i] = res->tex;
            sprintf(name, "mc_chara_base_%d", slot);
            Flash_FindLabel(&gReplayMenu->flash[0], NULL, name, &ref);
            Flash_ClipSetTex(&gReplayMenu->flash[0], &ref, ZAPROG->replay[gReplayMenu->cursor].flags & 1);
        }
    }
}

/* Plays the "chosen" animation of the play / save plate. */
void ReplayMenu_ConfirmPlate(void) {
    MFlashRef ref;

    Flash_FindLabel(&gReplayMenu->flash[0], NULL, "mc_menu_play_plate", &ref);
    Flash_ClipGotoLabel(&gReplayMenu->flash[0], &ref, "fl_ok");
}

/* Lights or dims the play / save plate. */
void ReplayMenu_LightPlate(s32 on) {
    MFlashRef ref;

    Flash_FindLabel(&gReplayMenu->flash[0], NULL, "mc_menu_play_plate", &ref);
    if (on) {
        Flash_ClipGotoLabel(&gReplayMenu->flash[0], &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(&gReplayMenu->flash[0], &ref, "fl_off_start");
    }
}

/* Records whether the slot shown holds a replay, and lights the plate accordingly (not in save mode). */
void ReplayMenu_SetPlayable(s32 on) {
    MFlashRef ref;

    Flash_FindLabel(&gReplayMenu->flash[0], NULL, "mc_menu_play_plate", &ref);
    if (on) {
        gReplayMenu->flags |= REPLAY_PLAYABLE;
        if (!(ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE)) {
            Flash_ClipGotoLabel(&gReplayMenu->flash[0], &ref, "fl_on_start");
        }
    } else {
        if (gReplayMenu->flags & REPLAY_PLAYABLE) {
            gReplayMenu->flags ^= REPLAY_PLAYABLE;
        }
        if (!(ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE)) {
            Flash_ClipGotoLabel(&gReplayMenu->flash[0], &ref, "fl_off_start");
        }
    }
}

/* Whether a slot holds a replay. */
static inline s32 ReplayMenu_HasData(s32 slot) {
    if (ZAPROG->replay[slot].flags & 1) {
        return 1;
    }
    return 0;
}

/* Shows or hides the "no data" plate for the slot shown, after the cursor moved or the card was read. */
void ReplayMenu_RefreshSlot(void) {
    s32 has = ReplayMenu_HasData(gReplayMenu->cursor);

    if (!(gReplayMenu->flags & REPLAY_PLAYABLE) && !has) {
        ReplayMenu_PlayNoData("fl_no_data_out_in");
        return;
    } else if ((gReplayMenu->flags & REPLAY_PLAYABLE) && has) {
        return;
    } else if (!(gReplayMenu->flags & REPLAY_PLAYABLE) && has) {
        ReplayMenu_PlayNoData("fl_no_data_out");
    } else if ((gReplayMenu->flags & REPLAY_PLAYABLE) && !has) {
        ReplayMenu_PlayNoData("fl_no_data_in");
    }
    ReplayMenu_SetPlayable(has);
}

/* Leaves the screen: back to the select screen the battle came from in save mode, else to the Data Center. */
static inline void ReplayMenu_Leave(void) {
    gReplayMenu->flags |= REPLAY_LEAVE;
    if (ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE) {
        gReplayMenu->result = ZAPROG->battleType != 0 ? REPLAY_RESULT_TEAM : REPLAY_RESULT_SINGLE;
    } else {
        gReplayMenu->result = REPLAY_RESULT_BACK;
    }
}

/* McFlow mode 6 (replay load), done: the replay is in the battle's replay block; leave to play it. */
void ReplayMenu_OnLoadDone(void) {
    gReplayMenu->unkEC = 0;
    gReplayMenu->result = REPLAY_RESULT_PLAY;
    gReplayMenu->flags |= REPLAY_LEAVE;
}

/* McFlow mode 6, failed: forget the slot list and leave. */
void ReplayMenu_OnLoadFailed(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    Progress_ClearTeams();
    ReplayMenu_RefreshSlot();
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_Leave();
}

/* McFlow mode 6, "no" answered: stay. */
void ReplayMenu_OnLoadRefused(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
}

/* McFlow mode 5 (replay save), done: go back to the select screen. */
void ReplayMenu_OnSaveDone(void) {
    gReplayMenu->unkEC = 0;
    gReplayMenu->result = ZAPROG->battleType != 0 ? REPLAY_RESULT_TEAM : REPLAY_RESULT_SINGLE;
    gReplayMenu->flags |= REPLAY_LEAVE;
}

/* McFlow mode 5, failed: forget the slot list and leave. */
void ReplayMenu_OnSaveFailed(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    Progress_ClearTeams();
    ReplayMenu_RefreshSlot();
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_Leave();
}

/* McFlow mode 5, "no" answered: stay. */
void ReplayMenu_OnSaveRefused(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
}

/* McFlow mode 7 (card scan), done: the slot list in gProgress is valid. */
void ReplayMenu_OnScanDone(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    gReplayMenu->unkEC = 0;
    gReplayMenu->flags |= REPLAY_SCANNED;
    ReplayMenu_RefreshSlot();
}

/* McFlow mode 7, failed: leave. */
void ReplayMenu_OnScanFailed(void) {
    Progress_ClearTeams();
    ReplayMenu_RefreshSlot();
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_Leave();
}

/* McFlow mode 7, "no" answered: nothing. */
void ReplayMenu_OnScanRefused(void) {
}

/* McFlow mode 8 ("leave without saving the replay?"), yes: leave. */
void ReplayMenu_OnAskYes(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_RefreshSlot();
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_Leave();
}

/* McFlow mode 8, failed: the same. */
void ReplayMenu_OnAskFailed(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_RefreshSlot();
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_Leave();
}

/* McFlow mode 8, no: stay. */
void ReplayMenu_OnAskNo(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
}

/* McFlow mode 9 ("the card was removed"), done: stay. */
void ReplayMenu_OnRemovedDone(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_RefreshSlot();
}

/* McFlow mode 9, failed: leave. */
void ReplayMenu_OnRemovedFailed(void) {
    gReplayMenu->busy = REPLAY_BUSY_NONE;
    ReplayMenu_Leave();
}

/* McFlow mode 9, "no" answered: nothing. */
void ReplayMenu_OnRemovedRefused(void) {
}

/* Any mode, the card went away while the window was up: forget the slot list. */
void ReplayMenu_OnCardRemoved(void) {
    Progress_ClearTeams();
    ReplayMenu_RefreshSlot();
}

/* A section of the screen's pack; `host` is the development file it was built from (unused: see DcPass_Init). */
static inline u8 *ReplayMenu_Section(s32 n, const char *host) {
    return MPACK_AT(gReplayMenu->res, n);
}

#define RP_RES(n, host) \
    res = (MTexRes *)ReplayMenu_Section(n, host); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen (section `section` of archive 8), builds its movie and sets the card flows up. */
void ReplayMenu_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gReplayMenu = Heap_Alloc(sizeof(ReplayMenu), 0x20, 0, 2);
    memset(gReplayMenu, 0, sizeof(ReplayMenu));
    gReplayMenu->pack = MPACK_AT(gMenuArc8, section);
    gReplayMenu->res = Sprite_Unpack(gReplayMenu->pack, NULL, NULL);
    RP_RES(6, REPLAY_HOST "dc_select_bg_PS2_.dbt");
    gReplayMenu->bg = res;
    RP_RES(7, REPLAY_HOST "dc_compane_PS2_.dbt");
    gReplayMenu->tex[0] = MTEX(res, 0);
    gReplayMenu->tex[1] = MTEX(res, 1);
    gReplayMenu->tex[2] = MTEX(res, 2);
    gReplayMenu->tex[3] = MTEX(res, 3);
    gReplayMenu->tex[4] = MTEX(res, 4);
    gReplayMenu->tex[5] = MTEX(res, 5);
    gReplayMenu->tex[6] = MTEX(res, 6);
    RP_RES(3, REPLAY_HOST "dc_replay_tex_PS2_.dbt");
    gReplayMenu->tex[7] = MTEX(res, 0);
    gReplayMenu->tex[8] = MTEX(res, 1);
    gReplayMenu->tex[11] = MTEX(res, 2);
    gReplayMenu->tex[12] = MTEX(res, 3);
    gReplayMenu->tex[13] = MTEX(res, 4);
    gReplayMenu->tex[15] = MTEX(res, 5);
    gReplayMenu->tex[16] = MTEX(res, 6);
    gReplayMenu->tex[17] = MTEX(res, 7);
    gReplayMenu->texB[1] = MTEX(res, 8);
    gReplayMenu->texB[2] = MTEX(res, 9);
    gReplayMenu->texB[4] = MTEX(res, 10);
    gReplayMenu->texB[6] = MTEX(res, 11);
    gReplayMenu->texB[7] = MTEX(res, 12);
    gReplayMenu->texB[8] = MTEX(res, 13);
    RP_RES(4, REPLAY_HOST "dc_replay_text_JP_PS2_.dbt");
    gReplayMenu->tex[9] = MTEX(res, 0);
    gReplayMenu->tex[10] = MTEX(res, 1);
    gReplayMenu->tex[14] = MTEX(res, 2);
    gReplayMenu->tex[18] = MTEX(res, 3);
    gReplayMenu->texB[0] = MTEX(res, 4);
    gReplayMenu->texB[3] = MTEX(res, 5);
    gReplayMenu->texB[5] = MTEX(res, 6);
    RP_RES(2, REPLAY_HOST "dc_replay_tex_plate_PS2_.dbt");
    gReplayMenu->tex[19] = MTEX(res, 0);
    gReplayMenu->chips = (u32 *)ReplayMenu_Section(8, REPLAY_HOST "chara_chip_PS2_.pak");
    for (i = 0; i < 0xA5; i++) {
        res = (MTexRes *)MPACK_AT(gReplayMenu->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    Flash_Create(&gReplayMenu->flash[0], ReplayMenu_Section(1, REPLAY_HOST "replay_data_PS2_.fod"), gReplayMenu->tex);
    Flash_Play(&gReplayMenu->flash[0], 1);
    gReplayMenu->font = ReplayMenu_Section(9, REPLAY_HOST "font_datacenter_PS2_.pak");
    Dialog_Init(ReplayMenu_Section(5, REPLAY_HOST "if_system_window_JP_PS2_.pak"), NULL, 0);
    McFlow_Init(1);
    McFlow_SetModeCb(7, 0, ReplayMenu_OnScanDone, 0);
    McFlow_SetModeCb(7, 1, ReplayMenu_OnScanFailed, 0);
    McFlow_SetModeCb(7, 2, ReplayMenu_OnScanRefused, 0);
    McFlow_SetModeCb(7, 3, ReplayMenu_OnCardRemoved, 0);
    McFlow_SetModeCb(8, 0, ReplayMenu_OnAskYes, 0);
    McFlow_SetModeCb(8, 1, ReplayMenu_OnAskFailed, 0);
    McFlow_SetModeCb(8, 2, ReplayMenu_OnAskNo, 0);
    McFlow_SetModeCb(8, 3, ReplayMenu_OnCardRemoved, 0);
    McFlow_SetModeCb(9, 0, ReplayMenu_OnRemovedDone, 0);
    McFlow_SetModeCb(9, 1, ReplayMenu_OnRemovedFailed, 0);
    McFlow_SetModeCb(9, 2, ReplayMenu_OnRemovedRefused, 0);
    McFlow_SetModeCb(9, 3, ReplayMenu_OnCardRemoved, 0);
    McFlow_SetModeCb(6, 0, ReplayMenu_OnLoadDone, 0);
    McFlow_SetModeCb(6, 1, ReplayMenu_OnLoadFailed, 0);
    McFlow_SetModeCb(6, 2, ReplayMenu_OnLoadRefused, 0);
    McFlow_SetModeCb(6, 3, ReplayMenu_OnCardRemoved, 0);
    McFlow_SetModeCb(5, 0, ReplayMenu_OnSaveDone, 0);
    McFlow_SetModeCb(5, 1, ReplayMenu_OnSaveFailed, 0);
    McFlow_SetModeCb(5, 2, ReplayMenu_OnSaveRefused, 0);
    McFlow_SetModeCb(5, 3, ReplayMenu_OnCardRemoved, 0);
    Progress_ClearTeams();
    gReplayMenu->timer = 30;
    gReplayMenu->cursor = ZAPROG->replayCursor;
}

/* Advances the movie; on the first frame starts the card scan (McFlow mode 7). */
void ReplayMenu_Update(void) {
    s32 i;

    for (i = 0; i < 1; i++) {
        Flash_Advance(&gReplayMenu->flash[i]);
    }
    if (!(gReplayMenu->flags & REPLAY_STARTED)) {
        Progress_ClearTeams();
        ReplayMenu_PlayNoData("fl_no_data_in");
        ReplayMenu_LightPlate(ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE);
        gReplayMenu->busy = REPLAY_BUSY_SCAN;
        Dialog_SetLayout(1);
        McFlow_Start(7);
        gReplayMenu->flags |= REPLAY_STARTED;
    }
}

/* Frees everything ReplayMenu_Init made. */
void ReplayMenu_Term(void) {
    s32 i;

    Dialog_Term();
    McFlow_Term();
    for (i = 0; i < 1; i++) {
        Flash_Destroy(&gReplayMenu->flash[i]);
    }
    if (gReplayMenu->res != NULL) {
        Heap_Free(gReplayMenu->res);
        gReplayMenu->res = NULL;
    }
    if (gReplayMenu != NULL) {
        Heap_Free(gReplayMenu);
        gReplayMenu = NULL;
    }
}

/* Sets the texture rectangle of a clip found by name (each use has its own MFlashRef on the stack). */
static inline void ReplayMenu_SetUv(char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(&gReplayMenu->flash[0], parent, name, &ref);
    Flash_ClipSetUv(&gReplayMenu->flash[0], &ref, uv);
}

/* Texture rectangle of one cell of a sheet. */
static inline void ReplayMenu_SetCell(MFlashUv *uv, s32 col, s32 row, s32 w, s32 h) {
    uv->x0 = col * w;
    uv->x1 = uv->x0 + w;
    uv->y0 = row * h;
    uv->y1 = uv->y0 + h;
}

/* Draws the frame and runs the card flow. */
void ReplayMenu_Draw(void) {
    MFlashRef ref;
    char parent[0x100];
    char name[0x100];
    MFlashUv uv;
    s32 num;
    s32 i;

    {
        ReplayMenu_SetCell(&uv, 0, 1, 0x20, 0x20);
        ReplayMenu_SetUv("mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
        ReplayMenu_SetCell(&uv, 1, 1, 0x20, 0x20);
        ReplayMenu_SetUv("mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
    }
    /* Hides the arrow that leads nowhere. On slots 1..5 the two names are whatever the buffers hold. */
    if (gReplayMenu->cursor == 0) {
        strcpy(parent, "mc_yajirusi_up");
        strcpy(name, "mc_yajirusi_icon_up");
    } else if (gReplayMenu->cursor == REPLAY_SLOT_NUM - 1) {
        strcpy(parent, "mc_yajirusi_down");
        strcpy(name, "mc_yajirusi_icon_down");
    }
    Flash_FindLabel(&gReplayMenu->flash[0], parent, name, &ref);
    Flash_ClipSetFlags(&gReplayMenu->flash[0], &ref, 2, 0);
    /* The slot number, two digits from a 4-column sheet. */
    num = gReplayMenu->cursor + 1;
    for (i = 1; i >= 0; i--) {
        s32 digit;

        sprintf(parent, "mc_list_num_%d", i);
        digit = num / (s32)pow(10.0, i);
        num -= digit * (s32)pow(10.0, i);
        ReplayMenu_SetCell(&uv, digit % 4, digit / 4, 0x20, 0x20);
        ReplayMenu_SetUv(NULL, parent, &uv);
    }
    /* The plate's caption: "play", or "save" in save mode. */
    {
        ReplayMenu_SetCell(&uv, 0, ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE, 0x100, 0x20);
        ReplayMenu_SetUv("mc_menu_play_plate", "mc_menu_text1_on", &uv);
        ReplayMenu_SetCell(&uv, 0, ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE, 0x100, 0x20);
        ReplayMenu_SetUv("mc_menu_play_plate", "mc_menu_text1_off", &uv);
    }
    ReplayMenu_DrawChips();
#ifdef PORT
    Port_MenuPage(41); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gReplayMenu->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gReplayMenu->bg, 0, 0, 0x80);
#endif
    Flash_Draw(&gReplayMenu->flash[0]);
    gReplayMenu->mcState = McFlow_Update();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Watches the card while no flow runs: rescans when one appears, reports when it goes away. */
void ReplayMenu_WatchCard(void) {
    s32 ret;

    if (gReplayMenu->busy == REPLAY_BUSY_SCAN || gReplayMenu->busy == REPLAY_BUSY_FILE) {
        return;
    }
    if (gReplayMenu->busy == REPLAY_BUSY_REMOVED) {
        return;
    }
    if (gReplayMenu->busy == REPLAY_BUSY_ASK) {
        return;
    }
    if (gReplayMenu->flags & REPLAY_LEAVE) {
        return;
    }
    if (gReplayMenu->mcState != 0) {
        return;
    }
    ret = McFlow_PollCard();
    if (ret == 0) {
        return;
    }
    switch (ret) {
    case 2:
        if (gReplayMenu->flags & REPLAY_SCANNED) {
            Progress_ClearTeams();
            ReplayMenu_RefreshSlot();
            gReplayMenu->busy = REPLAY_BUSY_REMOVED;
            Dialog_SetLayout(1);
            McFlow_Start(9);
        }
        break;
    case 1:
        if (!(gReplayMenu->flags & REPLAY_SCANNED) && gReplayMenu->busy != REPLAY_BUSY_ASK) {
            gReplayMenu->flags |= REPLAY_SCANNED;
            gReplayMenu->busy = ret;
            Dialog_SetLayout(1);
            McFlow_Start(7);
        }
        break;
    }
}

/* Keeps the cursor in 0..6; returns whether it was in range. */
static inline s32 ReplayMenu_ClampCursor(s32 *cursor) {
    s32 max = REPLAY_SLOT_NUM - 1;

    if (*cursor < 0) {
        *cursor = 0;
        return 0;
    }
    if (*cursor > max) {
        *cursor = max;
        return 0;
    }
    return 1;
}

/* Reads pad 0: up / down change the slot, confirm loads or saves, cancel leaves. */
void ReplayMenu_Input(void) {
    if (gReplayMenu->busy == REPLAY_BUSY_ASK && Dialog_IsClosed()) {
        gReplayMenu->busy = REPLAY_BUSY_NONE;
    }
    if (!(gReplayMenu->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gReplayMenu->flags & REPLAY_LEAVE) {
        return;
    }
    if (ZAPROG->flags & ZAPROG_FREEZE) {
        return;
    }
    if (gReplayMenu->mcState != 0) {
        return;
    }
    if (gReplayMenu->busy != REPLAY_BUSY_NONE) {
        return;
    }
    if (gPad[0].gameRepeat & PADG_UP) {
        gReplayMenu->cursor--;
        if (ReplayMenu_ClampCursor(&gReplayMenu->cursor)) {
            Snd_PlaySe(1, 0);
            ReplayMenu_RefreshSlot();
            Flash_GotoLabel(&gReplayMenu->flash[0], "fl_next_file", 1);
        }
    } else if (gPad[0].gameRepeat & PADG_DOWN) {
        gReplayMenu->cursor++;
        if (ReplayMenu_ClampCursor(&gReplayMenu->cursor)) {
            Snd_PlaySe(1, 0);
            ReplayMenu_RefreshSlot();
            Flash_GotoLabel(&gReplayMenu->flash[0], "fl_next_file", 1);
        }
    } else if (gPad[0].gamePressed & PADG_CROSS) {
        if (ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE) {
            /* Save the last battle's replay into this slot. */
            gReplayMenu->busy = REPLAY_BUSY_FILE;
            Dialog_SetLayout(1);
            McFlow_SetSlot(gReplayMenu->cursor);
            McFlow_Start(5);
            ReplayMenu_ConfirmPlate();
            Snd_PlaySe(1, 1);
        } else if (ZAPROG->replay[gReplayMenu->cursor].flags & 1) {
            /* Load this slot's replay; ReplayMenu_OnLoadDone leaves the overlay to play it. */
            gReplayMenu->busy = REPLAY_BUSY_FILE;
            Dialog_SetLayout(1);
            McFlow_SetSlot(gReplayMenu->cursor);
            McFlow_Start(6);
            ReplayMenu_ConfirmPlate();
            Snd_PlaySe(1, 1);
        } else {
            Snd_PlaySe(1, 7);
        }
    } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
        if (ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE) {
            gReplayMenu->busy = REPLAY_BUSY_ASK;
            Dialog_SetLayout(1);
            McFlow_Start(8);
        } else {
            gReplayMenu->flags |= REPLAY_LEAVE;
            gReplayMenu->result = REPLAY_RESULT_BACK;
        }
        Snd_PlaySe(1, 2);
    }
}

/* Once the screen is left: fades out, and after 30 frames stores the result and the cursor; returns 1 then. */
s32 ReplayMenu_CheckLeave(s32 *result) {
    if ((gReplayMenu->flags & REPLAY_LEAVE) && gReplayMenu->busy != REPLAY_BUSY_ASK) {
        if (!(gReplayMenu->flags & REPLAY_LEAVING)) {
            gReplayMenu->flags |= REPLAY_LEAVING;
            Flash_GotoLabel(&gReplayMenu->flash[0], "fl_list_out", 1);
            ColorFade_StartOut(0, 0, 0, 20);
        }
        if (--gReplayMenu->timer == 0) {
            *result = gReplayMenu->result;
            ZAPROG->replayCursor = gReplayMenu->cursor;
            return 1;
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
        }
    }
    return 0;
}

/*
 * The replay list (progress mode 56). Returns REPLAY_RESULT_: 0 back to the Data Center menu, 1 a replay was
 * loaded and the overlay must be left for the battle, 39 / 40 the progress mode to continue with after saving.
 * The save-mode flag is cleared on the way out.
 */
s32 ReplayMenu_Run(s32 section) {
    s32 result;

    ReplayMenu_Init(section);
    ColorFade_StartIn(0, 0, 0, 20);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        ReplayMenu_Update();
        ReplayMenu_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ReplayMenu_CheckLeave(&result)) {
            break;
        }
        ReplayMenu_Input();
        ReplayMenu_WatchCard();
    }
    ReplayMenu_Term();
    Dma_ResetBuffers();
    if (ZAPROG->replayFlags & ZAPROG_REPLAY_SAVE) {
        ZAPROG->replayFlags ^= ZAPROG_REPLAY_SAVE;
    }
    return result;
}
