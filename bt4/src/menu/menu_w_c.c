#include "common.h"
#include "menu/menu_x.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * Menu overlay DBZP.BIN, 0x39EB08..0x39FAA8: the EvoTop object, the top menu of Evolution Z (progress mode 48;
 * guide Krillin, three plates). Unlike the other screens its work structure is a local of EvoTop_Run and is
 * handed to every function, so the object has no work pointer and no `.data`. (This file was written as two
 * halves, menu_w_c.c 0x39EB08..0x39EFC0 and menu_x.c 0x39EFC0..0x39FAA8, merged here.)
 *
 * Read-only data: 0x3BC008..0x3BC364 (the Shop object's ends with the jump table at 0x3BBDA0..0x3BC004);
 * "fl_on_start" / "fl_off_start" exist in both, so they are two source files.
 *
 * This file looks clips up through small inline helpers that own their MFlashRef. That shows in three ways:
 * the strings of EvoTop_ClipOnOff ("fl_on_start" / "fl_off_start") are emitted in front of the first string of
 * the function that uses it; each inlined copy has its own 16-byte stack slot behind the caller's locals; and
 * the slot's address is formed again at every use instead of being kept in a register. (A 128-bit scalar in
 * place of the MFlashRef gives the same code but not the same string order.)
 *
 * "fl_ok" (0x3BC068) sits between the strings of EvoTop_SetPlateText and the "host:" paths of EvoTop_Init: it
 * belongs to the inline helper EvoTop_PlateOk, which the original therefore defined above EvoTop_Init, although
 * only EvoTop_Input uses it. EvoTop_Input only matches when the compiler has seen the definition of EvoTop_Wrap
 * (it is side-effect free, which changes how the branch around its call is filled).
 */

/* MsgWin_Init takes three arguments (menu_a.h declares a fourth, unused one as s32); a pointer is passed here. */
extern void MsgWin_Init4(void *pack, void *text, s32 side, void *arg) __asm__("MsgWin_Init");

/* Starts a line of the guide's voice with its subtitle. */
void EvoTop_PlayVoice(EvoTop *menu, s32 line) {
    Voice_PlayWithSubtitle(menu->subtitles, EVOTOP_VOICE_BASE, line);
    menu->voiceLine = line;
}

/* Wraps a cursor: below min gives max, above max gives min. */
s32 EvoTop_Wrap(s32 value, s32 min, s32 max) {
    if (value < min) {
        value = max;
    } else if (value > max) {
        value = min;
    }
    return value;
}

/* Plays the "on" or the "off" animation of a clip of the movie's root. */
static inline void EvoTop_ClipOnOff(MFlash *flash, char *name, s32 on) {
    MFlashRef ref;

    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Lights (on != 0) or dims the plate the cursor is on. */
void EvoTop_SetPlate(EvoTop *menu, s32 on) {
    char name[256];
    MFlash *flash = &menu->flash[0];

    sprintf(name, "mc_menu_plate_%d", menu->cursor + 1);
    EvoTop_ClipOnOff(flash, name, on);
}

/* Sets the texture rectangle of a clip. */
static inline void EvoTop_ClipSetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Gives each of the three plates its strip of the caption texture. */
void EvoTop_SetPlateText(EvoTop *menu) {
    MFlashUv uv;
    char name[256];
    MFlash *flash = &menu->flash[0];
    s32 i;

    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x200;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_menu_plate_%d", i + 1);
        EvoTop_ClipSetUv(flash, name, "mc_menu_text_on", &uv);
        EvoTop_ClipSetUv(flash, name, "mc_menu_text_off", &uv);
    }
}

/* Plays "fl_ok" on the cursor plate. Inline in the original (its locals are the first of EvoTop_Input's frame)
   and defined here, ahead of its only user: see the header comment. */
static inline void EvoTop_PlateOk(EvoTop *m) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "mc_menu_plate_%d", m->cursor + 1);
    Flash_FindLabel(&m->flash[0], NULL, name, &ref);
    Flash_ClipGotoLabel(&m->flash[0], &ref, "fl_ok");
}

/* Advances the movie. */
void EvoTop_Advance(EvoTop *menu) {
    s32 i;

    for (i = 0; i < EVOTOP_FLASH_NUM; i++) {
        Flash_Advance(&menu->flash[i]);
    }
}

/*
 * A section of the screen's pack. `host` is the file the section was built from: the development build could
 * read it from the host PC. Nothing uses it here, but the strings are still in the object, in this order
 * (0x3BC070..0x3BC2B9).
 */
static inline u8 *EvoTop_Section(EvoTop *menu, s32 n, const char *host) {
    return MPACK_AT(menu->res, n);
}

#define EVO_HOST "host:data/ps2/test/main/evoZ/"

#define EVO_RES(n, host) \
    res = (MTexRes *)EvoTop_Section(menu, n, host); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the menu's section of archive 7 and builds its movie, windows and sounds. */
void EvoTop_Init(EvoTop *menu, s32 section) {
    MTexRes *res = NULL;
    MFlash *flash = &menu->flash[0];

    menu->pack = (u32 *)MPACK_AT(gMenuArc7, section);
    menu->res = Sprite_Unpack(menu->pack, NULL, NULL);

    EVO_RES(4, EVO_HOST "EvoZ_TEX_PS2_.dbt");
    menu->tex[0] = MTEX(res, 0);
    menu->tex[1] = MTEX(res, 1);
    menu->tex[2] = MTEX(res, 2);
    menu->tex[3] = MTEX(res, 3);
    menu->tex[7] = MTEX(res, 4);
    menu->tex[9] = MTEX(res, 5);
    menu->tex[10] = MTEX(res, 6);
    menu->tex[11] = MTEX(res, 7);
    EVO_RES(2, EVO_HOST "EvoZ_Cririn_PS2_.dbt");
    menu->tex[4] = MTEX(res, 0);
    menu->tex[5] = MTEX(res, 1);
    menu->tex[6] = MTEX(res, 3);
    EVO_RES(5, EVO_HOST "EvoZ_TEXT_JP_PS2_.dbt");
    menu->tex[8] = MTEX(res, 0);
    menu->tex[12] = MTEX(res, 1);
    EVO_RES(1, EVO_HOST "EvoZ_Top_bg_PS2_.dbt");
    menu->bg = res;

    EVO_RES(3, EVO_HOST "EvoZ_Title_JP_PS2_.dbt");
    IconWin_Init(EvoTop_Section(menu, 9, EVO_HOST "if_title_line_PS2_.pak"), res);
    IconWin_Open();

    menu->msgText = EvoTop_Section(menu, 7, EVO_HOST "ez_msg_JP_PS2_.pak");
    MsgWin_Init4(EvoTop_Section(menu, 8, EVO_HOST "if_msg_window_PS2_.pak"), menu->msgText, 1, menu->unk7C);
    MsgWin_Open();

    menu->subtitles = EvoTop_Section(menu, 10, EVO_HOST "evoz_lips_PS2_.pak");
    Flash_Create(flash, EvoTop_Section(menu, 6, EVO_HOST "evo_z_top_PS2_.fod"), menu->tex);
    Flash_Play(flash, 1);

    DcSave_Init(EvoTop_Section(menu, 11, EVO_HOST "if_system_window_JP_PS2_.pak"));

    menu->blink = Rand_Range(32);
    menu->cursor = EVO_PROGRESS_CURSOR;
    menu->voiceLine = -1;
    StreamSe_PlayDefault(0, 0x15D);
}

/* Frees the screen. */
void EvoTop_Term(EvoTop *m) {
    IconWin_Term();
    MsgWin_Term();
    Flash_Destroy(&m->flash[0]);
    if (m->res != NULL) {
        Heap_Free(m->res);
        m->res = NULL;
    }
    DcSave_Term();
}

/* Clip callback: scissor to the left cloud layer. */
void EvoTop_ScissorCloud1(void) {
    Sprite_SetScissor(0, 0x100, 0x12, 0x112);
}

/* Clip callback: scissor to the right cloud layer. */
void EvoTop_ScissorCloud2(void) {
    Sprite_SetScissor(0x180, 0x200, 0x12, 0x112);
}

/* Clip callback: scissor back to the whole screen. */
void EvoTop_ScissorOff(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Hangs the scissor callbacks on the two cloud clips. */
void EvoTop_SetCloudClips(EvoTop *m) {
    MFlashRef ref;

    Flash_FindLabel(&m->flash[0], NULL, "mc_bg_cloud_1", &ref);
    Flash_ClipSetCallbackA(&m->flash[0], &ref, EvoTop_ScissorCloud1, NULL);
    Flash_ClipSetCallbackB(&m->flash[0], &ref, EvoTop_ScissorOff, NULL);
    Flash_FindLabel(&m->flash[0], NULL, "mc_bg_cloud_2", &ref);
    Flash_ClipSetCallbackA(&m->flash[0], &ref, EvoTop_ScissorCloud2, NULL);
    Flash_ClipSetCallbackB(&m->flash[0], &ref, EvoTop_ScissorOff, NULL);
}

/* Animates the guide's eyes and mouth. */
void EvoTop_UpdateFace(EvoTop *m) {
    MFlashRef ref;

    Flash_FindLabel(&m->flash[0], NULL, "mc_guide_cririn_eye", &ref);
    FlashAnim_Blink(&m->flash[0], &ref, &m->blink, 0);
    Flash_FindLabel(&m->flash[0], NULL, "mc_guide_cririn_mouth", &ref);
    if (Voice_GetStat(0) != MVOICE_IDLE && !DcSave_IsStarted()) {
        FlashAnim_Talk(&m->flash[0], &ref, &m->talk, 0);
    } else {
        FlashAnim_ShowNext2(&m->flash[0], &ref, 0);
    }
}

/* Starts the movie and the first voice line when due, then draws the screen. */
void EvoTop_Draw(EvoTop *m) {
    MFlashRef ref;
    MFlashUv uv;
    char name[0x100];
    s32 i;
    MFlash *flash;

    if (!(m->flags & EVOTOP_STARTED)) {
        Flash_GotoLabel(&m->flash[0], "fl_in", 1);
        m->flags |= EVOTOP_STARTED;
    }
    if (!(m->flags & EVOTOP_GREETED)) {
        if (m->flash[0].flags & MFLASH_PAD) {
            switch (m->cursor) {
            case -1:
                m->cursor = 0;
                EvoTop_PlayVoice(m, 0);
                break;
            case 0:
                EvoTop_PlayVoice(m, 1);
                break;
            case 1:
                EvoTop_PlayVoice(m, 2);
                break;
            case 2:
                EvoTop_PlayVoice(m, 3);
                break;
            }
            EvoTop_SetPlate(m, 1);
            m->flags |= EVOTOP_GREETED;
        }
    }
    EvoTop_UpdateFace(m);
    EvoTop_SetPlateText(m);
    EvoTop_SetCloudClips(m);
    flash = &m->flash[0];
    uv.x1 = 0x200;
    uv.y1 = 0x100;
    uv.x0 = 0;
    uv.y0 = 0;
    for (i = 0; i < 2; i++) {
        sprintf(name, "mc_bg_cloud_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        FlashAnim_Scroll(flash, &ref, &uv, &m->cloud, NULL, 0.28444445f, 0.0f);
    }
#ifdef PORT
    Port_MenuPage(42); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(m->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(m->bg, 0, 0, 0x80);
#endif
    for (i = 0; i < EVOTOP_FLASH_NUM; i++) {
        Flash_Draw(&m->flash[i]);
    }
    IconWin_Draw();
    MsgWin_Draw(0, 0, m->voiceLine);
    DcSave_Update();
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/*
 * Pad 0 (gameRepeat up / down, gamePressed confirm / cancel), once the movie accepts input and no save flow
 * runs. Plates 0 and 1 leave with result 1 / 2; plate 2 starts the guide's explanation (voice lines 4..16,
 * advanced by confirm or when a line ends, cancel leaves it); cancel leaves with result 0 and, when
 * gProgress->flags bit 0 is set, starts the save flow first. After 3600 idle frames the guide says line 17.
 */
void EvoTop_Input(EvoTop *m) {
    if (!(m->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (m->flags & EVOTOP_CHOSEN) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (DcSave_GetState()) {
        return;
    }
    if ((gPad[0].gamePressed || Voice_GetStat(0) == MVOICE_IDLE) && m->voiceLine == 0) {
        EvoTop_PlayVoice(m, m->cursor + 1);
    }
    if (gPad[0].gameRepeat || gPad[0].gamePressed) {
        m->timer = 0;
    }
    if (!(m->flags & EVOTOP_EXPLAIN)) {
        if (gPad[0].gameRepeat & MPAD_UP) {
            EvoTop_SetPlate(m, 0);
            m->cursor--;
            m->cursor = EvoTop_Wrap(m->cursor, 0, 2);
            EvoTop_SetPlate(m, 1);
            Snd_PlaySe(1, 0);
            EvoTop_PlayVoice(m, m->cursor + 1);
        } else if (gPad[0].gameRepeat & MPAD_DOWN) {
            EvoTop_SetPlate(m, 0);
            m->cursor++;
            m->cursor = EvoTop_Wrap(m->cursor, 0, 2);
            EvoTop_SetPlate(m, 1);
            Snd_PlaySe(1, 0);
            EvoTop_PlayVoice(m, m->cursor + 1);
        } else if (gPad[0].gamePressed & MPAD_OK) {
            EvoTop_PlateOk(m);
            switch (m->cursor) {
            case 0:
                Flash_GotoLabel(&m->flash[0], "fl_out", 1);
                m->flags |= EVOTOP_CHOSEN;
                m->result = 1;
                break;
            case 1:
                Flash_GotoLabel(&m->flash[0], "fl_out", 1);
                m->flags |= EVOTOP_CHOSEN;
                m->result = 2;
                break;
            case 2:
                Flash_GotoLabel(&m->flash[0], "fl_evo_z_ets_in", 1);
                m->flags |= EVOTOP_EXPLAIN;
                EvoTop_PlayVoice(m, EVOTOP_LINE_EXPLAIN_FIRST);
                break;
            }
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & MPAD_CANCEL) {
            m->result = 0;
            m->flags |= EVOTOP_CHOSEN;
            Snd_PlaySe(1, 2);
            if (gProgress->flags & 1) {
                DcSave_Start();
            }
        }
        m->timer++;
    } else {
        if ((gPad[0].gamePressed & MPAD_OK) || Voice_GetStat(0) == MVOICE_IDLE) {
            if (m->voiceLine == 3) {
                EvoTop_PlateOk(m);
            }
            if (m->voiceLine == EVOTOP_LINE_EXPLAIN_LAST) {
                m->flags ^= EVOTOP_EXPLAIN;
                Flash_GotoLabel(&m->flash[0], "fl_evo_z_ets_out", 1);
                EvoTop_PlayVoice(m, 3);
            } else {
                m->voiceLine++;
                EvoTop_PlayVoice(m, m->voiceLine);
            }
            if (gPad[0].gamePressed & MPAD_OK) {
                Snd_PlaySe(1, 1);
            }
        } else if (gPad[0].gamePressed & MPAD_CANCEL) {
            m->flags ^= EVOTOP_EXPLAIN;
            EvoTop_PlayVoice(m, 3);
            Flash_GotoLabel(&m->flash[0], "fl_evo_z_ets_out", 1);
            Snd_PlaySe(1, 2);
        }
    }
    if (m->timer == EVOTOP_IDLE_FRAMES) {
        if (m->flags & EVOTOP_EXPLAIN) {
            m->timer = 0;
        } else {
            EvoTop_PlayVoice(m, EVOTOP_LINE_IDLE);
        }
    }
    if (Voice_GetStat(0) == MVOICE_IDLE) {
        if (m->voiceLine == EVOTOP_LINE_IDLE) {
            m->timer = 0;
        }
    }
}

/*
 * After the choice: waits for the save flow to end, starts the fade (20 frames) and a 30-frame timer, fades the
 * voice, the stream sound and (when going back) the music; when the timer runs out stores the cursor in
 * gProgress + 0x7D0 and returns 1.
 */
s32 EvoTop_Leave(EvoTop *m) {
    if (m->flags & EVOTOP_CHOSEN) {
        if (!(m->flags & EVOTOP_LEAVING) && DcSave_IsDone()) {
            if (m->result == 0) {
                Flash_GotoLabel(&m->flash[0], "fl_cancel", 1);
            } else {
                Flash_GotoLabel(&m->flash[0], "fl_out", 1);
            }
            ColorFade_StartOut(0, 0, 0, 20);
            MsgWin_Close();
            IconWin_Close();
            m->timer = 30;
            m->flags |= EVOTOP_LEAVING;
        }
        if (DcSave_IsStarted()) {
            Voice_FadeOutStep(0);
        }
        if (m->flags & EVOTOP_LEAVING) {
            if (ColorFade_IsFadingOut()) {
                Voice_FadeOutStep(0);
                StreamSe_FadeOutStep(0);
                if (m->result == 0) {
                    Bgm_FadeOutStep();
                }
            }
            if (--m->timer == 0) {
                EVO_PROGRESS_CURSOR = m->cursor;
                return 1;
            }
        } else {
            return 0;
        }
    }
    return 0;
}

/* The screen's frame loop. Returns 0 (back), 1 or 2 (the plate chosen). */
s32 EvoTop_Run(s32 section) {
    EvoTop m;

    memset(&m, 0, sizeof(EvoTop));
    EvoTop_Init(&m, section);
    ColorFade_StartIn(0, 0, 0, 20);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        EvoTop_Advance(&m);
        EvoTop_Draw(&m);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (EvoTop_Leave(&m)) {
            break;
        }
        EvoTop_Input(&m);
    }
    EvoTop_Term(&m);
    Dma_ResetBuffers();
    return m.result;
}
