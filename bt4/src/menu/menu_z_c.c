#include "common.h"
#include "menu/menu_z.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * Menu overlay DBZP.BIN, 0x3A9A70..0x3AAF30: DcMenu, the top menu of the Data Center (progress mode 53): three
 * plates and the guide (Bulma). A whole object: its read-only data is 0x3BD350..0x3BD700 (the strings before
 * and after it repeat "fl_on_start" / "fl_ok", so the neighbours are other source files). The work structure is
 * a local variable of DcMenu_Run.
 */

/* Sets the texture rectangle of a clip found by name (each use has its own MFlashRef on the stack). */
static inline void DcMenu_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Starts a line of the guide and shows its subtitle. */
static inline void DcMenu_Say(DcMenu *menu, s32 line) {
    Voice_PlayWithSubtitle(menu->subtitles, DC_VOICE_BASE, line);
    menu->voiceLine = line;
}

/* Whether the guide has finished her line. */
static inline s32 DcMenu_VoiceEnded(void) {
    return Voice_GetStat(0) == MVOICE_IDLE;
}

/* Keeps a value in lo..hi as a ring. */
s32 DcMenu_Wrap(s32 value, s32 lo, s32 hi) {
    if (value < lo) {
        value = hi;
    } else if (value > hi) {
        value = lo;
    }
    return value;
}

/* Blinks the guide's eyes and moves her mouth while she talks. */
void DcMenu_AnimGuide(DcMenu *menu) {
    MFlashRef ref;
    MFlash *flash = &menu->view.flash;

    Flash_FindLabel(flash, NULL, "mc_guide_blma_eye", &ref);
    FlashAnim_Blink(flash, &ref, &menu->view.blink, 0);
    Flash_FindLabel(flash, NULL, "mc_guide_blma_mouth", &ref);
    if (Voice_GetStat(0) != MVOICE_IDLE && !DcSave_IsStarted()) {
        FlashAnim_Talk(flash, &ref, &menu->view.talk, 0);
    } else {
        FlashAnim_ShowNext2(flash, &ref, 0);
    }
}

/* Scrolls the backdrop pattern. */
void DcMenu_AnimBg(DcMenu *menu) {
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash = &menu->view.flash;

    uv.y0 = 0;
    uv.y1 = 0x40;
    uv.x0 = 0;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_compane_3", &ref);
    FlashAnim_Scroll(flash, &ref, &uv, &menu->scroll, NULL, -0.15238095f, 0.0f);
}

/* Cuts the three plates' captions from their sheet (rows 0, 2, 1). */
/* (Written out: with DcMenu_SetRect the constants come out in other saved registers.) */
void DcMenu_DrawPlates(MFlash *flash) {
    MFlashUv uv;
    char name[0x100];
    s32 i;

    for (i = 0; i < DCMENU_PLATES; i++) {
        if (i == 1) {
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y0 = 0x40;
            uv.y1 = 0x60;
        } else if (i == 2) {
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y0 = 0x20;
            uv.y1 = 0x40;
        } else {
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y0 = i << 5;
            uv.y1 = (i << 5) + 0x20;
        }
        sprintf(name, "mc_menu_plate_%d", i + 1);
        DcMenu_SetUv(flash, name, "mc_menu_text_on", &uv);
        DcMenu_SetUv(flash, name, "mc_menu_text_off", &uv);
    }
}

/* (Defined here, behind DcMenu_DrawPlates: its two literals follow that function's in the object.) */
/* Lights or dims a clip found by name. */
static inline void DcMenu_LightClip(MFlash *flash, char *name, s32 on) {
    MFlashRef ref;

    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* (Defined here for the same reason: its literals come next in the object.) */
/* Plays the "chosen" animation of the plate under the cursor and leaves it lit. */
static inline void DcMenu_FlashPlate(DcMenu *menu) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_menu_plate_%d", menu->cursor + 1);
    flash = &menu->view.flash;
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
    Flash_ClipGotoLabel(flash, &ref, "fl_on_loop");
}

/* Wraps the cursor and lights or dims its plate. */
void DcMenu_LightPlate(DcMenu *menu, s32 on) {
    char name[0x100];
    MFlash *flash = &menu->view.flash;

    menu->cursor = DcMenu_Wrap(menu->cursor, 0, DCMENU_PLATES - 1);
    sprintf(name, "mc_menu_plate_%d", menu->cursor + 1);
    DcMenu_LightClip(flash, name, on);
}

/* Draws the frame the guide's first blink happens on. */
void DcMenu_ResetBlink(DcMenuView *view) {
    view->blink = Rand_Range(0x20);
}

/*
 * First frame: picks the guide's opening line (the explanation on the very first visit, which sets the save's
 * flag; a greeting otherwise) and puts the cursor back where the menu was left, with that plate's line.
 */
void DcMenu_Start(DcMenu *menu, s32 visits) {
    DcMenu_ResetBlink(&menu->view);
    if (!(ZSAVE->dcFlags & 1)) {
        menu->voiceLine = DCLINE_INTRO;
        ZSAVE->dcFlags |= 1;
    } else {
        menu->voiceLine = DCLINE_HELLO;
    }
    menu->visits = visits;
    menu->unkD4 = 0x1E;
    menu->cursor = ZPROG->dcCursor;
    switch (menu->cursor) {
    case -1:
        menu->cursor = 0;
        break;
    case 0:
        menu->voiceLine = DCLINE_PASSWORD;
        break;
    case 1:
        menu->voiceLine = DCLINE_LIST;
        break;
    case 2:
        menu->voiceLine = DCLINE_REPLAY;
        break;
    }
}

/* Advances the movie; starts the screen, then the opening line once the movie accepts input. */
void DcMenu_Update(DcMenu *menu) {
    Flash_Advance(&menu->view.flash);
    if (!(menu->flags & DCMENU_STARTED)) {
        DcMenu_Start(menu, ZPROG->dcVisits);
        menu->flags |= DCMENU_STARTED;
    }
    if (!(menu->flags & DCMENU_GREETED)) {
        if (menu->view.flash.flags & MFLASH_PAD) {
            DcMenu_LightPlate(menu, 1);
            DcMenu_Say(menu, menu->voiceLine);
            menu->flags |= DCMENU_GREETED;
        }
    }
    if (!(menu->flags & DCMENU_ITEM_LINE)) {
        if (menu->voiceLine == DCLINE_PASSWORD || menu->voiceLine == DCLINE_REPLAY ||
            menu->voiceLine == DCLINE_LIST) {
            menu->flags |= DCMENU_ITEM_LINE;
        }
    }
}

/* A section of the screen's pack. `host` is the file the section was built from (the development build could
   read it from the host PC); nothing uses it, but the strings are in the object, in this order. */
static inline u8 *DcMenu_Section(DcMenu *menu, s32 n, const char *host) {
    return MPACK_AT(menu->res, n);
}

#define DCMENU_HOST "host:data/ps2/test/main/DC/"

#define DCMENU_RES(n, host) \
    res = (MTexRes *)DcMenu_Section(menu, n, host); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen's section of archive 8, builds the movie and opens the shared windows. */
void DcMenu_Init(DcMenu *menu, s32 section) {
    MTexRes *res = NULL;

    menu->pack = MPACK_AT(gMenuArc8, section);
    menu->res = Sprite_Unpack(menu->pack, NULL, NULL);
    DCMENU_RES(6, DCMENU_HOST "dc_select_bg_PS2_.dbt");
    menu->view.bg = res;
    DCMENU_RES(11, DCMENU_HOST "dc_compane_PS2_.dbt");
    menu->view.tex[0] = MTEX(res, 0);
    menu->view.tex[1] = MTEX(res, 1);
    menu->view.tex[2] = MTEX(res, 2);
    menu->view.tex[3] = MTEX(res, 3);
    menu->view.tex[4] = MTEX(res, 4);
    menu->view.tex[5] = MTEX(res, 5);
    menu->view.tex[6] = MTEX(res, 6);
    DCMENU_RES(7, DCMENU_HOST "dc_select_guide_PS2_.dbt");
    menu->view.tex[7] = MTEX(res, 0);
    menu->view.tex[8] = MTEX(res, 1);
    menu->view.tex[9] = MTEX(res, 3);
    DCMENU_RES(10, DCMENU_HOST "dc_select_PS2_.dbt");
    menu->view.tex[10] = MTEX(res, 0);
    menu->view.tex[12] = MTEX(res, 1);
    menu->view.tex[13] = MTEX(res, 2);
    menu->view.tex[14] = MTEX(res, 3);
    DCMENU_RES(8, DCMENU_HOST "dc_select_text_JP_PS2_.dbt");
    menu->view.tex[11] = MTEX(res, 0);
    menu->view.tex[15] = MTEX(res, 1);
    Flash_Create(&menu->view.flash, DcMenu_Section(menu, 5, DCMENU_HOST "data_center_top_PS2_.fod"), menu->view.tex);
    Flash_Play(&menu->view.flash, 1);
    menu->subtitles = DcMenu_Section(menu, 1, DCMENU_HOST "dcenter_lips_PS2_.pak");
    DCMENU_RES(9, DCMENU_HOST "dc_select_title_JP_PS2_.dbt");
    IconWin_Init(DcMenu_Section(menu, 3, DCMENU_HOST "if_title_line_PS2_.pak"), res);
    IconWin_Open();
    menu->msgText = DcMenu_Section(menu, 4, DCMENU_HOST "dc_msg_JP_PS2_.pak");
    MsgWin_Init(DcMenu_Section(menu, 2, DCMENU_HOST "if_msg_window_PS2_.pak"), menu->msgText, 0, (s32)menu->unk88);
    MsgWin_Open();
    menu->voiceLine = -1;
    DcSave_Init(DcMenu_Section(menu, 12, DCMENU_HOST "if_system_window_JP_PS2_.pak"));
}

/* Closes the windows, destroys the movie and frees the unpacked section. */
void DcMenu_Term(DcMenu *menu) {
    MsgWin_Term();
    IconWin_Term();
    Flash_Destroy(&menu->view.flash);
    DcSave_Term();
    if (menu->res != NULL) {
        Heap_Free(menu->res);
        menu->res = NULL;
    }
}

/* Draws the backdrop, the movie and the windows. */
void DcMenu_Draw(DcMenu *menu) {
    MFlash *flash = &menu->view.flash;

    DcMenu_AnimGuide(menu);
    DcMenu_AnimBg(menu);
    DcMenu_DrawPlates(flash);
#ifdef PORT
    Port_MenuPage(43); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(menu->view.bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(menu->view.bg, 0, 0, 0x80);
#endif
    IconWin_Draw();
    Flash_Draw(flash);
    MsgWin_Draw(0, 0, menu->voiceLine);
    DcSave_Update();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

#define DCMENU_NEXT (DcMenu_VoiceEnded() || (gPad[0].gamePressed & ZPAD_OK))
#define DCMENU_CLICK() \
    if (gPad[0].gamePressed & ZPAD_OK) { \
        Snd_PlaySe(1, 1); \
    }

/* The guide's chain of lines: the line being said decides which one follows when it ends or confirm is pressed. */
void DcMenu_UpdateVoice(DcMenu *menu) {
    switch (menu->voiceLine) {
    case 0:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, 1);
        }
        break;
    case 1:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, 2);
        }
        break;
    case 2:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, 3);
        }
        break;
    case 3:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, 4);
        }
        break;
    case 4:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, DCLINE_ASK);
        }
        break;
    case DCLINE_HELLO:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            if (menu->visits < 6) {
                DcMenu_Say(menu, 6);
            } else {
                s32 n = menu->visits - 5;

                if (n >= 4) {
                    n = 3;
                }
                n = Rand_Range(n + 1);
                switch (n) {
                case 0:
                    DcMenu_Say(menu, 6);
                    break;
                case 1:
                    DcMenu_Say(menu, 7);
                    break;
                case 2:
                    DcMenu_Say(menu, 8);
                    break;
                case 3:
                    DcMenu_Say(menu, 9);
                    break;
                }
            }
        }
        break;
    case 6:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, DCLINE_ASK);
        }
        break;
    case 7:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, DCLINE_ASK);
        }
        break;
    case 8:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, DCLINE_ASK);
        }
        break;
    case 9:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            DcMenu_Say(menu, DCLINE_ASK);
        }
        break;
    case DCLINE_ASK:
        if (DCMENU_NEXT) {
            DCMENU_CLICK();
            if (menu->cursor == 0) {
                DcMenu_Say(menu, DCLINE_PASSWORD);
            } else if (menu->cursor == 1) {
                DcMenu_Say(menu, DCLINE_LIST);
            } else if (menu->cursor == 2) {
                DcMenu_Say(menu, DCLINE_REPLAY);
            }
        }
        break;
    case DCLINE_PASSWORD:
        if (DcMenu_VoiceEnded()) {
            switch ((s32)Rand_Range(3)) {
            case 0:
                DcMenu_Say(menu, 12);
                break;
            case 1:
                DcMenu_Say(menu, 13);
                break;
            case 2:
                DcMenu_Say(menu, 14);
                break;
            }
        }
        break;
    case 12:
    case 13:
    case 14:
        break;
    case DCLINE_REPLAY:
        if (DcMenu_VoiceEnded()) {
            switch ((s32)Rand_Range(3)) {
            case 0:
                DcMenu_Say(menu, 16);
                break;
            case 1:
                DcMenu_Say(menu, 17);
                break;
            case 2:
                DcMenu_Say(menu, 18);
                break;
            }
        }
        break;
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
        break;
    case DCLINE_LIST:
        if (DcMenu_VoiceEnded()) {
            switch ((s32)Rand_Range(3)) {
            case 0:
                DcMenu_Say(menu, 23);
                break;
            case 1:
                DcMenu_Say(menu, 24);
                break;
            case 2:
                DcMenu_Say(menu, 25);
                break;
            }
        }
        break;
    case 23:
    case 24:
    case 25:
        break;
    }
}

/* Says the line of the plate under the cursor. */
static inline void DcMenu_SayPlate(DcMenu *menu) {
    if (menu->cursor == 0) {
        DcMenu_Say(menu, DCLINE_PASSWORD);
    } else if (menu->cursor == 1) {
        DcMenu_Say(menu, DCLINE_LIST);
    } else {
        DcMenu_Say(menu, DCLINE_REPLAY);
    }
}

/* Reads the pad: up / down move over the three plates, confirm picks the mode, cancel leaves. */
void DcMenu_Input(DcMenu *menu) {
    if (!(menu->view.flash.flags & MFLASH_PAD)) {
        return;
    }
    if (menu->flags & DCMENU_CHOSEN) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (DcSave_GetState()) {
        return;
    }
    if (gPad[0].gameRepeat & ZPAD_UP) {
        DcMenu_LightPlate(menu, 0);
        menu->cursor--;
        DcMenu_LightPlate(menu, 1);
        DcMenu_SayPlate(menu);
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gameRepeat & ZPAD_DOWN) {
        DcMenu_LightPlate(menu, 0);
        menu->cursor++;
        DcMenu_LightPlate(menu, 1);
        DcMenu_SayPlate(menu);
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gamePressed & ZPAD_OK) {
        DcMenu_FlashPlate(menu);
        menu->flags |= DCMENU_CHOSEN;
        if (menu->cursor == 0) {
            menu->result = 54;
        } else if (menu->cursor == 1) {
            menu->result = 55;
        } else {
            menu->result = 56;
        }
        Snd_PlaySe(1, 1);
        MsgWin_Close();
    } else if (gPad[0].gamePressed & ZPAD_CANCEL) {
        menu->result = 0;
        menu->flags |= DCMENU_CHOSEN;
        Snd_PlaySe(1, 2);
        if (gProgress->flags & ZPROG_DIRTY) {
            DcSave_Start();
        }
    }
    DcMenu_UpdateVoice(menu);
}

/* After a choice: waits for the save prompt, starts the fade out and returns 1 when it is over. */
s32 DcMenu_CheckLeave(DcMenu *menu) {
    if (menu->flags & DCMENU_CHOSEN) {
        if (!(menu->flags & DCMENU_LEAVING)) {
            if (DcSave_IsDone()) {
                if (menu->result == 0) {
                    ColorFade_StartOut(0, 0, 0, 0x14);
                }
                menu->flags |= DCMENU_LEAVING;
                Flash_GotoLabel(&menu->view.flash, "fl_out", 1);
                IconWin_Close();
                MsgWin_Close();
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        }
        if (DcSave_IsStarted()) {
            Voice_FadeOutStep(0);
        }
        if (menu->flags & DCMENU_LEAVING) {
            if (ColorFade_IsFadingOut()) {
                Voice_FadeOutStep(0);
                Bgm_FadeOutStep();
            } else {
                return 1;
            }
        } else {
            return 0;
        }
    }
    return 0;
}

/* Runs the top menu (progress mode 53) with its own frame loop; returns the mode chosen, 0 = leave. */
s32 DcMenu_Run(s32 section) {
    DcMenu menu;

    memset(&menu, 0, sizeof(DcMenu));
    DcMenu_Init(&menu, section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        DcMenu_Update(&menu);
        DcMenu_Draw(&menu);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (DcMenu_CheckLeave(&menu)) {
            break;
        }
        DcMenu_Input(&menu);
    }
    ZPROG->dcCursor = menu.cursor;
    DcMenu_Term(&menu);
    Dma_ResetBuffers();
    return menu.result;
}
