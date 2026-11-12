#include "common.h"
#include "menu/menu_p.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work area (.data, 0x3B7380). */
UbMenu *gUbMenu = NULL;

/*
 * UbMenu, 0x37DC38..0x37EE18: the menu of mode 13, the first screen of main-menu item 1. Four plates
 * ("mc_menu_plate_%d") and two guides who comment on the plate under the cursor (Androids 17 and 18 by the clip
 * names). An object of its own: its .rodata (0x3B8AD0..0x3B8DD8) starts with two tables and repeats strings of
 * the objects around it ("mc_guide_17go", "fl_on_start").
 */

/* Dialogue scripts started after 1800 idle frames and when the screen is left with cancel. */
static const s32 sUbMenuIdleScript[3] = { 21, 41, 61 };
static const s32 sUbMenuLeaveScript[3] = { 81, 101, 121 };

#define UM_RES(n) \
    res = (MTexRes *)MPACK_AT(gUbMenu->res, n); \
    Res_RelocateOffsets(&res, res, res)

#define UM_CUR gUbMenu->cur[gUbMenu->level]

/* The clip of guide i. Written out at both uses (as in DuelMenu_Draw): a local variable does not match. */
#define UM_GUIDE(i) ((i) != 0 ? "mc_guide_18go" : "mc_guide_17go")

/* Row n of the plate texture (0x200 x 0x20 each). */
#define UM_ROW_UV(n) \
    uv.x0 = 0; \
    uv.y0 = (n) << 5; \
    uv.x1 = 0x200; \
    uv.y1 = uv.y0 + 0x20


/* The guide's comment on the plate under the cursor: lines 2..5, spoken by guide 0 for plates 1 and 3. */
#define UM_SAY_PLATE() \
    gUbMenu->voiceLine = UM_CUR + 2; \
    if (UM_CUR == 1 || UM_CUR == 3) { \
        gUbMenu->talker = 0; \
    } else { \
        gUbMenu->talker = 1; \
    } \
    Voice_PlayWithSubtitle(gUbMenu->subtitles, P_VOICE_BASE, gUbMenu->voiceLine)

void UbMenu_ClipGoto(s32 movie, s32 level, char *label);

/* Cursor one plate up, wrapping. */
void UbMenu_CursorUp(void) {
    if (--UM_CUR < 0) {
        UM_CUR = UBMENU_ITEM_NUM - 1;
    }
}

/* Cursor one plate down, wrapping. */
void UbMenu_CursorDown(void) {
    if (++UM_CUR >= UBMENU_ITEM_NUM) {
        UM_CUR = 0;
    }
}

/* Loads the screen: section `section` of archive 3. */
void UbMenu_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gUbMenu = Heap_Alloc(sizeof(UbMenu), 0x20, 0, 2);
    memset(gUbMenu, 0, sizeof(UbMenu));
    gUbMenu->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gUbMenu->res = Sprite_Unpack(gUbMenu->pack, NULL, NULL);
    UM_RES(2);
    gUbMenu->bg = res;
    UM_RES(5);
    gUbMenu->tex[8] = MTEX(res, 0);
    gUbMenu->tex[9] = MTEX(res, 1);
    gUbMenu->tex[10] = MTEX(res, 3);
    gUbMenu->tex[11] = MTEX(res, 5);
    gUbMenu->tex[12] = MTEX(res, 6);
    gUbMenu->tex[13] = MTEX(res, 8);
    UM_RES(3);
    gUbMenu->tex[0] = MTEX(res, 0);
    gUbMenu->tex[1] = MTEX(res, 1);
    gUbMenu->tex[6] = MTEX(res, 2);
    gUbMenu->tex[2] = MTEX(res, 3);
    gUbMenu->tex[5] = MTEX(res, 4);
    gUbMenu->tex[4] = MTEX(res, 5);
    gUbMenu->tex[3] = MTEX(res, 6);
    gUbMenu->tex[7] = MTEX(res, 7);
    gUbMenu->tex[14] = MTEX(res, 8);
    gUbMenu->tex[15] = MTEX(res, 9);
    gUbMenu->tex[16] = MTEX(res, 10);
    Flash_Create(&gUbMenu->flash[0], MPACK_AT(gUbMenu->res, 1), gUbMenu->tex);
    Flash_Play(&gUbMenu->flash[0], 1);
    for (i = 0; i < 2; i++) {
        gUbMenu->blink[i] = Rand_Libc() % 32;
    }
    gUbMenu->text = MPACK_AT(gUbMenu->res, 8);
    gUbMenu->subtitles = MPACK_AT(gUbMenu->res, 9);
    UM_RES(4);
    IconWin_Init(MPACK_AT(gUbMenu->res, 7), res);
    IconWin_Open();
    MsgWin_Init(MPACK_AT(gUbMenu->res, 6), gUbMenu->text, 0, 0);
    MsgWin_Open();
    gUbMenu->voiceLine = -1;
    gUbMenuPlate3Open = P_SAVE->ubFlags & P_UBFLAG_ITEM3;
    gUbMenu->cur[0] = P_PROG->ubCursor;
    gUbMenu->voiceSkip = 0;
}

/* Frees everything Init made. */
void UbMenu_Term(void) {
    s32 i;

    MsgWin_Term();
    IconWin_Term();
    for (i = 0; i < UBMENU_FLASH_NUM; i++) {
        Flash_Destroy(&gUbMenu->flash[i]);
    }
    if (gUbMenu->res != NULL) {
        Heap_Free(gUbMenu->res);
        gUbMenu->res = NULL;
    }
    if (gUbMenu != NULL) {
        Heap_Free(gUbMenu);
        gUbMenu = NULL;
    }
}

/* Draws the screen: the two guides, the blinking button icon, the four plates, the message window. */
void UbMenu_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;

#ifdef PORT
    Port_MenuPage(31); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gUbMenu->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gUbMenu->bg, 0, 0, 0x80);
#endif
    flash = &gUbMenu->flash[0];
    for (i = 0; i < 2; i++) {
        Flash_FindLabel(flash, UM_GUIDE(i), i != 0 ? "mc_guide_18go_eye" : "mc_guide_17go_eye", &ref);
        FlashAnim_Blink(flash, &ref, &gUbMenu->blink[i], 0);
        Flash_FindLabel(flash, UM_GUIDE(i), i != 0 ? "mc_guide_18go_mouth" : "mc_guide_17go_mouth", &ref);
        if (gUbMenu->talker == i) {
            FlashAnim_Talk(flash, &ref, &gUbMenu->talk[i], 0);
        } else {
            FlashAnim_ShowNext2(flash, &ref, 0);
        }
    }
    uv.x0 = gUbMenu->iconFrame << 6;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_icon_play1", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    gUbMenu->iconFrame ^= 1;
    for (i = 0; i < UBMENU_ITEM_NUM; i++) {
        UM_ROW_UV(i);
        if (i == 2 && gUbMenuPlate3Open == 0) {
            /* the closed plate's picture: the fifth row (the two redundant stores are needed for the match) */
            UM_ROW_UV(4);
        }
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    for (i = 0; i < UBMENU_FLASH_NUM; i++) {
        Flash_Draw(&gUbMenu->flash[i]);
    }
    IconWin_Draw();
    MsgWin_Draw(0, 0, gUbMenu->voiceLine);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Runs the leave timer and the movie; after a greeting line ends, the guide comments on the current plate. */
void UbMenu_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gUbMenu->timer > 0) {
        gUbMenu->timer--;
    }
    if ((gUbMenu->flags & UBMENU_GREETED) && (u32)gUbMenu->voiceLine < 2) {
        if (Voice_GetStat(0) == P_VOICE_IDLE) {
            UM_SAY_PLATE();
        }
    }
    for (i = 0; i < UBMENU_FLASH_NUM; i++) {
        Flash_Advance(&gUbMenu->flash[i]);
    }
}

#define UM_LINE(line, who) \
    gUbMenu->voiceLine = (line); \
    gUbMenu->talker = (who)

#define UM_PLAY() Voice_PlayWithSubtitle(gUbMenu->subtitles, P_VOICE_BASE, gUbMenu->voiceLine)

/*
 * One step of the guides' dialogue (UbMenu.script; 0 = none, the pad is read). A step starts when the line
 * before it has ended or was cut short with confirm.
 *     1 / 11        greeting by guide 0 / guide 1
 *     21, 22        idle dialogue A
 *     41..44        idle dialogue B
 *     61            idle line C
 *     81 / 101 / 121, then 82 / 102 / 122   a goodbye line, then the fade out
 */
void UbMenu_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gUbMenu->voiceSkip == 0) {
        if (gUbMenu->script == 0) {
            return;
        }
        if (gUbMenu->voiceLine != -1 && Voice_GetStat(0) != P_VOICE_IDLE &&
            gUbMenu->script == gUbMenu->scriptLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                gUbMenu->voiceSkip = 1;
            }
            return;
        }
    } else {
        gUbMenu->voiceSkip = 0;
    }
    switch (gUbMenu->script) {
    case 1:
        UM_LINE(0, 0);
        gUbMenu->script = 0;
        UM_PLAY();
        break;
    case 11:
        UM_LINE(1, 1);
        gUbMenu->script = 0;
        UM_PLAY();
        break;
    case 21:
        UM_LINE(7, 0);
        UM_PLAY();
        gUbMenu->script++;
        break;
    case 22:
        UM_LINE(8, 1);
        UM_PLAY();
        gUbMenu->script = 0;
        break;
    case 41:
        UM_LINE(9, 1);
        UM_PLAY();
        gUbMenu->script++;
        break;
    case 42:
        UM_LINE(10, 0);
        UM_PLAY();
        gUbMenu->script++;
        break;
    case 43:
        UM_LINE(11, 0);
        UM_PLAY();
        gUbMenu->script++;
        break;
    case 44:
        UM_LINE(12, 1);
        UM_PLAY();
        gUbMenu->script = 0;
        break;
    case 61:
        UM_LINE(13, 1);
        UM_PLAY();
        gUbMenu->script = 0;
        break;
    case 81:
        UM_LINE(14, 0);
        UM_PLAY();
        gUbMenu->script++;
        break;
    case 101:
        UM_LINE(15, 1);
        UM_PLAY();
        gUbMenu->script++;
        break;
    case 121:
        UM_LINE(16, 1);
        UM_PLAY();
        gUbMenu->script++;
        break;
    case 82:
    case 102:
    case 122:
        ColorFade_StartOut(0, 0, 0, 0x14);
        gUbMenu->script = 0;
        break;
    }
    gUbMenu->scriptLast = gUbMenu->script;
}

/* Pad 0: up / down the plates (plate 3 is skipped while it is closed), confirm chooses, cancel leaves. */
void UbMenu_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;
    s32 cancel = gPad[0].gamePressed & PADG_TRIANGLE;

    if (!(gUbMenu->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gUbMenu->flags & UBMENU_STARTED)) {
        UbMenu_ClipGoto(0, 0, "fl_on_start");
        if (Rand_Range(2) != 0) {
            gUbMenu->script = 1;
        } else {
            gUbMenu->script = 11;
        }
        gUbMenu->flags |= UBMENU_STARTED;
    }
    switch (gUbMenu->level) {
    case 0:
        if (up) {
            UbMenu_ClipGoto(0, 0, "fl_off_start");
            UbMenu_CursorUp();
            if (UM_CUR == 2 && gUbMenuPlate3Open == 0) {
                UbMenu_CursorUp();
            }
            UbMenu_ClipGoto(0, 0, "fl_on_start");
            UM_SAY_PLATE();
            Snd_PlaySe(1, 0);
            gUbMenu->idle = 0;
        } else if (down) {
            UbMenu_ClipGoto(0, 0, "fl_off_start");
            UbMenu_CursorDown();
            if (UM_CUR == 2 && gUbMenuPlate3Open == 0) {
                UbMenu_CursorDown();
            }
            UbMenu_ClipGoto(0, 0, "fl_on_start");
            UM_SAY_PLATE();
            Snd_PlaySe(1, 0);
            gUbMenu->idle = 0;
        } else if (confirm) {
            gUbMenu->flags |= UBMENU_CHOSEN;
            gUbMenu->flags |= UBMENU_LEAVING;
            gUbMenu->timer = 15;
            *result = UM_CUR + 1;
            Snd_PlaySe(1, 1);
            gUbMenu->idle = 0;
            Voice_StopWithLip();
        } else if (cancel) {
            *result = 0;
            gUbMenu->script = sUbMenuLeaveScript[Rand_Range(3)];
            Flash_GotoLabel(&gUbMenu->flash[0], "fl_menu_cancel", 1);
            Snd_PlaySe(1, 2);
            gUbMenu->idle = 0;
        } else {
            gUbMenu->idle++;
            if (gUbMenu->idle == 0x708) {
                gUbMenu->script = sUbMenuIdleScript[Rand_Range(3)];
                gUbMenu->idle = 0;
            }
        }
        break;
    }
}

/* Sends the plate under the cursor of `level` to a label. */
void UbMenu_ClipGoto(s32 movie, s32 level, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gUbMenu->flash[movie];

    sprintf(name, "mc_menu_plate_%d", gUbMenu->cur[level] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * The menu of mode 13. Returns the plate chosen, 1..4, or 0 when the player backed out. The cursor is kept in
 * gProgress for the next visit.
 */
s32 UbMenu_Run(s32 section) {
    s32 result = 1;

    UbMenu_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        UbMenu_Update();
        UbMenu_UpdateVoice();
        UbMenu_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gUbMenu->flags & UBMENU_GREETED) && (gUbMenu->flash[0].flags & MFLASH_PAD)) {
                gUbMenu->flags |= UBMENU_GREETED;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gUbMenu->flags & UBMENU_LEAVING) {
            if (gUbMenu->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                P_PROG->ubCursor = gUbMenu->cur[0];
            }
        } else if (gUbMenu->script == 0) {
            UbMenu_Input(&result);
        }
    }
    UbMenu_Term();
    Dma_ResetBuffers();
    return result;
}
