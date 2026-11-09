#include "common.h"
#include "menu/menu_x.h"
#include "menu/menu_y.h"
#include "sys/pad.h"
#include "sys/save.h"

Option *gOption = NULL; /* 0x3BC364 */

/*
 * Menu overlay DBZP.BIN, 0x39FBB8..0x3A65E8: the Option object, the options screen of mode 62. (Written as two
 * halves, menu_x_c.c 0x39FBB8..0x3A3848 = Option_Init / Option_Run / Option_Input and menu_y.c
 * 0x3A3848..0x3A65E8 = the draw function, the plate helper, the reset states and the term function; merged
 * here.) Its data starts with the work pointer gOption at 0x3BC364 (.data), followed by its strings from
 * 0x3BC370 ("fl_ok" exists a second time at 0x3BC390: a new source file).
 *
 * Read-only data, 0x3BC370..0x3BC928: the strings and jump tables of Option_Init / Option_Run / Option_Input up
 * to 0x3BC520; "mc_dende_eye" (0x3BC520, first string of Option_Draw) follows Option_Input's last jump table
 * without a gap; the pool ends with Option_UpdateReset's jump table at 0x3BC924. The next address, 0x3BC928, is
 * initialised data of the next link group.
 *
 * Option_Draw's strings are 0x3BC520..0x3BC819 and it has five jump tables (0x3BC820, 0x3BC840, 0x3BC860,
 * 0x3BC880, 0x3BC8A0, eight entries each); ten of its clip names are shared with the functions behind it.
 */

#define OPT_RES(n) \
    res = (MTexRes *)MPACK_AT(gOption->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 10) and sets the first state. */
s32 Option_Init(s32 section) {
    MFlashRef ref;
    char name[0x40];
    MTexRes *res = NULL;
    s32 i;

    gOption = Heap_Alloc(sizeof(Option), 0x20, 0, 2);
    memset(gOption, 0, sizeof(Option));
    gOption->pack = (u32 *)MPACK_AT(gMenuArc10, section);
    gOption->res = Sprite_Unpack(gOption->pack, NULL, NULL);
    /*
     * The rest of the function sits in a block that the compiler sees as a loop (`do { } while (0)`, most likely
     * from a macro), and the counter of the loop at the end is cleared here, not in its `for`: the original
     * sets the counter and the first default key (0 and 2, in saved registers) right after the first load of
     * this block, which only this arrangement reproduces.
     */
    do {
        OPT_RES(30);
        i = 0;
        gOption->bg = res;
        OPT_RES(5);
        gOption->tex[22] = MTEX(res, 0);
        OPT_RES(4);
        gOption->tex[27] = MTEX(res, 0);
        OPT_RES(6);
        gOption->tex[21] = MTEX(res, 0);
        gOption->tex[24] = MTEX(res, 1);
        gOption->tex[23] = MTEX(res, 2);
        OPT_RES(1);
        gOption->tex[32] = MTEX(res, 0);
        gOption->tex[31] = MTEX(res, 1);
        OPT_RES(2);
        gOption->tex[33] = MTEX(res, 0);
        OPT_RES(3);
        gOption->tex[25] = MTEX(res, 0);
        gOption->tex[26] = MTEX(res, 1);
        OPT_RES(10);
        gOption->tex[16] = MTEX(res, 0);
        gOption->tex[19] = MTEX(res, 1);
        gOption->tex[18] = MTEX(res, 2);
        OPT_RES(12);
        gOption->tex[15] = MTEX(res, 0);
        OPT_RES(11);
        gOption->tex[17] = MTEX(res, 0);
        gOption->tex[20] = MTEX(res, 1);
        OPT_RES(9);
        gOption->tex[37] = MTEX(res, 0);
        OPT_RES(13);
        gOption->tex[36] = MTEX(res, 0);
        gOption->tex[38] = MTEX(res, 1);
        OPT_RES(21);
        gOption->tex[46] = MTEX(res, 0);
        gOption->tex[47] = MTEX(res, 1);
        gOption->tex[48] = MTEX(res, 2);
        gOption->tex[50] = MTEX(res, 3);
        OPT_RES(22);
        gOption->tex[49] = MTEX(res, 0);
        OPT_RES(23);
        gOption->tex[28] = MTEX(res, 0);
        gOption->tex[29] = MTEX(res, 1);
        gOption->tex[30] = MTEX(res, 3);
        OPT_RES(34);
        gOption->tex[1] = MTEX(res, 0);
        gOption->tex[0] = MTEX(res, 1);
        OPT_RES(17);
        gOption->tex[2] = MTEX(res, 0);
        gOption->tex[5] = MTEX(res, 1);
        gOption->tex[4] = MTEX(res, 2);
        OPT_RES(16);
        gOption->tex[3] = MTEX(res, 0);
        gOption->tex[7] = MTEX(res, 2);
        OPT_RES(15);
        gOption->tex[10] = MTEX(res, 0);
        gOption->tex[13] = MTEX(res, 1);
        gOption->tex[12] = MTEX(res, 2);
        OPT_RES(14);
        gOption->tex[11] = MTEX(res, 0);
        gOption->tex[14] = MTEX(res, 2);
        OPT_RES(19);
        gOption->tex[40] = MTEX(res, 0);
        gOption->tex[39] = MTEX(res, 1);
        gOption->tex[42] = MTEX(res, 6);
        gOption->tex[41] = MTEX(res, 9);
        OPT_RES(25);
        gOption->tex[45] = MTEX(res, 0);
        OPT_RES(24);
        gOption->tex[43] = MTEX(res, 0);
        OPT_RES(26);
        gOption->tex[44] = MTEX(res, 0);
        OPT_RES(20);
        gOption->tex[34] = MTEX(res, 0);
        OPT_RES(27);
        gOption->tex[8] = MTEX(res, 0);
        gOption->tex[9] = MTEX(res, 1);
        OPT_RES(28);
        gOption->tex[35] = MTEX(res, 0);
        OPT_RES(29);
        gOption->tex[6] = MTEX(res, 0);
        gOption->bgmIds = (s32 *)(MPACK_AT(gOption->res, 37) + 0x10);
        gOption->bgmCount = gOption->res[gOption->res[37] >> 2];
        gOption->bgmCount--;
        gOption->bgmCount -= 4;
        BgmList_ApplyUnlocks(&gOption->bgmCount, gOption->bgmIds);
        Flash_Create(&gOption->flash[0], MPACK_AT(gOption->res, 8), gOption->tex);
        Flash_Play(&gOption->flash[0], 1);
        /* original bug: `name` is never written, so this looks up whatever the stack holds (the result is unused) */
        Flash_FindLabel(&gOption->flash[0], NULL, name, &ref);
        Dialog_Init(MPACK_AT(gOption->res, 35), NULL, 0);
        gOption->dialogMsg = MPACK_AT(gOption->res, 38);
        McFlow_Init(1);
        McFlow_SetDoneCb(0, Option_OnSaved, NULL);
        gOption->subtitles = MPACK_AT(gOption->res, 36);
        OPT_RES(18);
        IconWin_Init(MPACK_AT(gOption->res, 32), res);
        IconWin_Open();
        gOption->msgText = MPACK_AT(gOption->res, 33);
        MsgWin_Init(MPACK_AT(gOption->res, 31), gOption->msgText, 0, (s32)gOption->unk1B0);
        MsgWin_Open();
        gOption->cursor = OPT_ITEM_SAVE;
        gOption->state = OPT_ST_START;
        gOption->bgmVolume = gSaveData->bgmVolume;
        gOption->seVolume = gSaveData->seVolume;
        gOption->voiceLine = 0;
        gOption->picker = 0;
        gOption->page = 0;
        gOption->unk134 = 0;
        gOption->bgmBottom = 6;
        gOption->keyOld = -1;
        gOption->keyRow = 5;
        for (; i < OPTION_PAD_NUM; i++) {
            gOption->key[i][0] = 2;
            gOption->key[i][1] = 1;
            gOption->key[i][2] = 0;
            gOption->key[i][3] = 3;
            gOption->key[i][4] = 4;
            gOption->key[i][5] = 5;
            gOption->key[i][6] = 6;
            gOption->key[i][7] = 7;
        }
    } while (0);
    return 1;
}

/* The screen's frame loop; input is skipped while a memory card flow runs. Returns 0. */
s32 Option_Run(s32 section) {
    Option_Init(section);
    ColorFade_StartIn(0, 0, 0, 20);
    while (gOption->mcBusy || Option_Input()) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        Option_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
    }
    Option_Term();
    Dma_ResetBuffers();
    return 0;
}

#define OPT_VOICE(line) Voice_PlayWithSubtitle(gOption->subtitles, OPTION_VOICE_BASE, line)
#define OPT_PLATE(kind, label) Option_PlateGoto(0, kind, label)

/*
 * Pad 0, one state per frame; does nothing until the movie accepts input. Returns 0 when the screen is over
 * (faded out), else 1. Option.state:
 *   -1 start; 0 top page (rows 1..5); 2 / 3 / 4 the screen, sound and controller pages (rows 7..10, 12..16,
 *   17..19); a state equal to a row number means that row is open:
 *   1 save / load picker, 7 three-way type, 8 two-way picker, 9 screen position, 12 stereo / mono, 13 volumes,
 *   14 sound test, 15 voice language, 17 / 21 / 22 key assignment (player, on / off, the keys), 18 / 20
 *   vibration (player, on / off), 10 / 11 / 16 / 19 the reset dialogs (Option_UpdateReset, next chunk),
 *   5 leave (saves first when something changed), -2 that save has ended, 6 fading out.
 * Every setting is written straight into gSaveData; `dirty` only decides whether leaving runs the save flow.
 *
 * Matching notes: the choices inside a state are if / else chains, not switches; the statements that several
 * arms share were merged by the compiler, so they are written out in every arm as the code shape requires
 * (e.g. the busy flag after each McFlow_Start, `valueOld = value` after each inner if of the flag tests).
 */
s32 Option_Input(void) {
    s32 ret = 1;
    s32 i;

    if (!(gOption->flash[0].flags & MFLASH_PAD)) {
        return 1;
    }
    switch (gOption->state) {
    case OPT_ST_START:
        if (gOption->started == 0) {
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            gOption->started = 1;
            OPT_VOICE(gOption->voiceLine);
            gOption->state = OPT_ST_TOP;
        }
        break;
    case OPT_ST_TOP:
        if (gOption->greeted == 0) {
            if (Voice_GetStat(0) == MVOICE_IDLE) {
                gOption->voiceLine = 1;
                OPT_VOICE(1);
                gOption->greeted = 1;
            }
        }
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor <= 0) {
                gOption->cursor = OPT_ITEM_EXIT;
            }
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
            gOption->greeted = 1;
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_EXIT) {
                gOption->cursor = OPT_ITEM_SAVE;
            }
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
            gOption->greeted = 1;
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            /* original quirk: the state is 0 here, so the test is always true (the cursor was probably meant) */
            if (gOption->state != OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_ROW, "fl_ok");
            }
            gOption->state = gOption->cursor;
            Snd_PlaySe(1, 1);
            if (gOption->state == OPT_ITEM_SAVE) {
                if (gOption->greeted == 0) {
                    gOption->voiceLine = 1;
                    OPT_VOICE(1);
                    gOption->greeted = 1;
                }
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->picker = 0;
            } else if (gOption->state == OPT_ITEM_SCREEN) {
                Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_TYPE;
                gOption->voiceLine = gOption->cursor;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
                gOption->fromPage = 0;
                gOption->page = 1;
                OPT_VOICE(gOption->cursor);
            } else if (gOption->state == OPT_ITEM_SOUND) {
                Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_STEREO;
                gOption->voiceLine = 11;
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                gOption->fromPage = 0;
                gOption->page = 2;
                OPT_VOICE(gOption->voiceLine);
            } else if (gOption->state == OPT_ITEM_CTRL) {
                Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_KEYS;
                gOption->voiceLine = 23;
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
                gOption->fromPage = 0;
                gOption->page = 3;
                OPT_VOICE(gOption->voiceLine);
            }
            gOption->greeted = 1;
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            if (gOption->cursor == OPT_ITEM_EXIT) {
                if (gOption->dirty != 0) {
                    gOption->unk194 = 1;
                    gOption->state = OPT_ST_SAVED;
                    McFlow_Start(4); /* save, with the prompt used on leaving */
                    gOption->mcBusy = 1;
                    Snd_PlaySe(1, 2);
                    break;
                }
                ColorFade_StartOut(0, 0, 0, 20);
                gOption->state = OPT_ITEM_EXIT;
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_EXIT;
                gOption->voiceLine = gOption->cursor;
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            }
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_SCREEN:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor < OPT_ITEM_TYPE) {
                gOption->cursor = OPT_ITEM_SCR_RESET;
            }
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            }
            gOption->voiceLine = gOption->cursor;
            Snd_PlaySe(1, 0);
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_SCR_RESET) {
                gOption->cursor = OPT_ITEM_TYPE;
            }
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            s32 cur = gOption->cursor;

            if (cur == OPT_ITEM_TYPE) {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_ok");
                gOption->state = gOption->cursor;
                gOption->type = gSaveData->unk1694;
                gOption->valueOld = gOption->type;
                Flash_GotoLabel(&gOption->flash[0], "fl_type_in", 1);
                Snd_PlaySe(1, 1);
            } else if (cur == OPT_ITEM_SCR2) {
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = gSaveData->unk1698;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->valueOld = gOption->value;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_ok");
                gOption->state = gOption->cursor;
                gOption->picker = 4;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                Snd_PlaySe(1, 1);
            } else if (cur == OPT_ITEM_ADJUST) {
                gOption->screenX = gSaveData->screenX;
                gOption->screenY = gSaveData->screenY;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_ok");
                IconWin_Close();
                MsgWin_Close();
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_layout_in", 1);
                Snd_PlaySe(1, 1);
            } else if (cur == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_ok");
                gOption->state = cur;
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_off_start");
            OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            gOption->state = OPT_ST_TOP;
            gOption->cursor = OPT_ITEM_SCREEN;
            gOption->voiceLine = gOption->cursor;
            gOption->page = 0;
            gOption->fromPage = 1;
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_TYPE:
        if (gPad[0].gamePressed & PADG_LEFT) {
            gOption->typePrev = gOption->type;
            gOption->type--;
            if (gOption->type < 0) {
                gOption->type = 2;
            }
            Flash_GotoLabel(&gOption->flash[0], "fl_type_right", 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_RIGHT) {
            gOption->typePrev = gOption->type;
            gOption->type++;
            if (gOption->type > 2) {
                gOption->type = 0;
            }
            Flash_GotoLabel(&gOption->flash[0], "fl_type_left", 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            if (gOption->valueOld != gOption->type) {
                gOption->dirty = 1;
            }
            gSaveData->unk1694 = gOption->type;
            gOption->state = OPT_ITEM_SCREEN;
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_type_out", 1);
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->state = OPT_ITEM_SCREEN;
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_type_out", 1);
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_ADJUST:
        if ((gPad[0].gameRepeat & PADG_LEFT) && gSaveData->screenX > -16) {
            gSaveData->screenX--;
        } else if ((gPad[0].gameRepeat & PADG_RIGHT) && gSaveData->screenX < 16) {
            gSaveData->screenX++;
        } else if ((gPad[0].gameRepeat & PADG_UP) && gSaveData->screenY > -16) {
            gSaveData->screenY--;
        } else if ((gPad[0].gameRepeat & PADG_DOWN) && gSaveData->screenY < 16) {
            gSaveData->screenY++;
        } else if (gPad[0].gamePressed & PADG_START) {
            gOption->state = OPT_ST_ADJUST_RESET;
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            /* original quirk: only a change of both axes counts as a change */
            if (gSaveData->screenX != gOption->screenX && gSaveData->screenY != gOption->screenY) {
                gOption->dirty = 1;
            }
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_layout_out", 1);
            gOption->state = OPT_ITEM_SCREEN;
            IconWin_Open();
            MsgWin_Open();
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Flash_GotoLabel(&gOption->flash[0], "fl_layout_out", 1);
            gSaveData->screenX = gOption->screenX;
            gSaveData->screenY = gOption->screenY;
            gOption->state = OPT_ITEM_SCREEN;
            MsgWin_Open();
            IconWin_Open();
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_SOUND:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor < OPT_ITEM_STEREO) {
                gOption->cursor = OPT_ITEM_SND_RESET;
            }
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_STEREO) {
                gOption->voiceLine = 11;
            } else if (gOption->cursor == OPT_ITEM_VOLUME) {
                gOption->voiceLine = gOption->cursor;
            } else if (gOption->cursor == OPT_ITEM_BGM) {
                gOption->voiceLine = 17;
            } else if (gOption->cursor == OPT_ITEM_SND_RESET) {
                gOption->voiceLine = 10;
            } else {
                gOption->voiceLine = 18;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_SND_RESET) {
                gOption->cursor = OPT_ITEM_STEREO;
            }
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_STEREO) {
                gOption->voiceLine = 11;
            } else if (gOption->cursor == OPT_ITEM_VOLUME) {
                gOption->voiceLine = gOption->cursor;
            } else if (gOption->cursor == OPT_ITEM_BGM) {
                gOption->voiceLine = 17;
            } else if (gOption->cursor == OPT_ITEM_SND_RESET) {
                gOption->voiceLine = 10;
            } else {
                gOption->voiceLine = 18;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            if (gOption->cursor == OPT_ITEM_STEREO) {
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                gOption->value = gSaveData->soundMode;
                gOption->valueOld = gOption->value;
                gOption->picker = 3;
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_VOLUME) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                gOption->value = 0;
                gOption->picker = 7;
                gOption->bgmVolume = gSaveData->bgmVolume;
                gOption->seVolume = gSaveData->seVolume;
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_BGM) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                MsgWin_Close();
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_bgm_in", 1);
                OPT_PLATE(OPT_CLIP_BGM, "fl_on_start");
                Bgm_Stop();
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_VOICE) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = (gSaveData->flags ^ 1) & 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->state = gOption->cursor;
                gOption->picker = 6;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_ok");
                gOption->state = gOption->cursor;
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            if (gOption->cursor != OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            }
            gOption->state = OPT_ST_TOP;
            gOption->cursor = OPT_ITEM_SOUND;
            gOption->voiceLine = gOption->cursor;
            gOption->page = 0;
            gOption->fromPage = 2;
            OPT_VOICE(gOption->voiceLine);
            gOption->fromPage = 3;
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ITEM_VOLUME:
        if ((gPad[0].gamePressed & PADG_UP) && gOption->value != 0) {
            gOption->value = 0;
            OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
            OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_DOWN) && gOption->value != 1) {
            gOption->value = 1;
            OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
            OPT_PLATE(OPT_CLIP_VOL_SE, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
            OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
            OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            gSaveData->bgmVolume = gOption->bgmVolume;
            gSaveData->seVolume = gOption->seVolume;
            Bgm_SetVolume(0x40);
            gOption->state = OPT_ITEM_SOUND;
            Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        } else if (gOption->value == 0) {
            if (gPad[0].gameRepeat & PADG_LEFT) {
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
                if (--gSaveData->bgmVolume < 0) {
                    gSaveData->bgmVolume = 9;
                }
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
                Bgm_SetVolume(0x40);
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gameRepeat & PADG_RIGHT) {
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
                if (++gSaveData->bgmVolume > 9) {
                    gSaveData->bgmVolume = 0;
                }
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
                Bgm_SetVolume(0x40);
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gamePressed & PADG_CROSS) {
                if (gOption->bgmVolume != gSaveData->bgmVolume) {
                    gOption->dirty = 1;
                }
                gOption->bgmVolume = gSaveData->bgmVolume;
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_ok");
                Snd_PlaySe(1, 1);
            }
        } else {
            if (gPad[0].gameRepeat & PADG_LEFT) {
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
                if (--gSaveData->seVolume < 0) {
                    gSaveData->seVolume = 9;
                }
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gameRepeat & PADG_RIGHT) {
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
                if (++gSaveData->seVolume > 9) {
                    gSaveData->seVolume = 0;
                }
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gamePressed & PADG_CROSS) {
                if (gOption->seVolume != gSaveData->seVolume) {
                    gOption->dirty = 1;
                }
                gOption->seVolume = gSaveData->seVolume;
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_ok");
                Snd_PlaySe(1, 1);
            }
        }
        break;
    case OPT_ITEM_BGM:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->bgmCursor > 0) {
                OPT_PLATE(OPT_CLIP_BGM, "fl_off_start");
                gOption->bgmCursor--;
                if (gOption->bgmTop > gOption->bgmCursor) {
                    gOption->bgmExtra = gOption->bgmBottom - 1;
                    gOption->bgmTop--;
                    gOption->bgmBottom--;
                    Flash_GotoLabel(&gOption->flash[0], "fl_bgm_down", 1);
                }
                OPT_PLATE(OPT_CLIP_BGM, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->bgmCursor < gOption->bgmCount - 1) {
                OPT_PLATE(OPT_CLIP_BGM, "fl_off_start");
                gOption->bgmCursor++;
                if (gOption->bgmBottom <= gOption->bgmCursor) {
                    gOption->bgmExtra = gOption->bgmTop;
                    gOption->bgmTop++;
                    gOption->bgmBottom++;
                    Flash_GotoLabel(&gOption->flash[0], "fl_bgm_up", 1);
                }
                OPT_PLATE(OPT_CLIP_BGM, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            OPT_PLATE(OPT_CLIP_BGM, "fl_ok");
            if (gOption->bgmIds[gOption->bgmCursor] != 0x19) {
                Bgm_Play(gOption->bgmIds[gOption->bgmCursor] + 0x10B16);
                Snd_PlaySe(1, 1);
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            MsgWin_Open();
            gOption->state = OPT_ITEM_SOUND;
            gOption->cursor = OPT_ITEM_BGM;
            Flash_GotoLabel(&gOption->flash[0], "fl_bgm_cansel", 1);
            OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            Bgm_Play(DISC_RESOURCE(0x10B18, 0x83E));
            Snd_PlaySe(1, 2);
            OPT_VOICE(17);
        }
        break;
    case OPT_ITEM_CTRL:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor < OPT_ITEM_KEYS) {
                gOption->cursor = OPT_ITEM_CTRL_RESET;
            }
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_KEYS) {
                gOption->voiceLine = 23;
            } else if (gOption->cursor == OPT_ITEM_VIB) {
                gOption->voiceLine = 24;
            } else {
                gOption->voiceLine = 10;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_CTRL_RESET) {
                gOption->cursor = OPT_ITEM_KEYS;
            }
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_KEYS) {
                gOption->voiceLine = 23;
            } else if (gOption->cursor == OPT_ITEM_VIB) {
                gOption->voiceLine = 24;
            } else {
                gOption->voiceLine = 10;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            gOption->state = gOption->cursor;
            gOption->value = 0;
            gOption->pad = 0;
            Option_DimPickers();
            if (gOption->state == OPT_ITEM_VIB) {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_ok");
                gOption->ctrlKind = 0;
                gOption->picker = 2;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_ok");
                Snd_PlaySe(1, 1);
            } else if (gOption->state == OPT_ITEM_KEYS) {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_ok");
                gOption->ctrlKind = 1;
                gOption->picker = 2;
                Flash_GotoLabel(&gOption->flash[0], "fl_ctrl_in", 1);
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->pad = 0;
                OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
                Snd_PlaySe(1, 1);
            } else {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_ok");
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->fromPage = 4;
            OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_off_start");
            OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            gOption->state = OPT_ST_TOP;
            gOption->cursor = OPT_ITEM_CTRL;
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
            gOption->page = 0;
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ITEM_KEYS:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_off_start");
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            gOption->pad = gOption->value = gOption->value ^ 1;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_DOWN) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_ok");
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_on_start");
            gOption->pad = gOption->value;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(0)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(1)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            }
            gOption->state = OPT_ST_KEYS_PAD;
            Snd_PlaySe(1, 1);
            {
                /* one load of the pointer and one store: with gOption written out the two arms reload it */
                Option *o = gOption;

                if (o->value != 0) {
                    o->voiceLine = 31;
                } else {
                    o->voiceLine = 30;
                }
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->state = OPT_ITEM_CTRL;
            gOption->cursor = OPT_ITEM_KEYS;
            gOption->voiceLine = 23;
            OPT_VOICE(gOption->voiceLine);
            OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_ctrl_out", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ST_KEYS_PAD:
        if (((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) && gOption->keyEdit == 0) {
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_off_start");
            gOption->value ^= 1;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(0)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_B(0);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_B(0);
                }
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(1)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_B(1);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_B(1);
                }
            }
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_on_start");
            Snd_PlaySe(1, 0);
            if (gOption->value != 0) {
                gOption->voiceLine = 31;
            } else {
                gOption->voiceLine = 30;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_UP) {
            /* original bug: compares the addresses of the two tables, so this is always true */
            if (gSaveData->key[gOption->pad] != gSaveData->keyEdit[gOption->pad]) {
                gOption->dirty = 1;
            }
            if (gOption->value == 0) {
                gSaveData->key[gOption->pad][0] = 2;
                gSaveData->key[gOption->pad][1] = 1;
                gSaveData->key[gOption->pad][2] = 0;
                gSaveData->key[gOption->pad][3] = 3;
                gSaveData->key[gOption->pad][4] = 4;
                gSaveData->key[gOption->pad][5] = 5;
                gSaveData->key[gOption->pad][6] = 6;
                gSaveData->key[gOption->pad][7] = 7;
            } else {
                for (i = 0; i < OPTION_KEY_NUM; i++) {
                    gSaveData->key[0][gOption->pad * 8 + i] = gSaveData->keyEdit[0][gOption->pad * 8 + i];
                }
            }
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
            gOption->value = gOption->pad;
            gOption->state = OPT_ITEM_KEYS;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            Snd_PlaySe(1, 2);
        } else if ((gSaveData->flags & SAVE_FLAG_PAD_B(0)) || (gSaveData->flags & SAVE_FLAG_PAD_B(1))) {
            if ((gPad[0].gamePressed & PADG_CROSS) && gOption->value != 0) {
                OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
                gOption->state = OPT_ST_KEYS_EDIT;
                gOption->keyEdit = 1;
                gOption->keyRow = 3;
                gOption->keyCursor = 2;
                Option_SetKeyMark(gOption->keyRow);
                Snd_PlaySe(1, 1);
            }
        }
        break;
    case OPT_ST_KEYS_EDIT:
        gOption->keyHeld = 0;
        if ((gPad[0].gamePressed & PADG_LEFT) && gOption->keyOld < 0) {
            gOption->keyCursor -= 4;
            if (gOption->keyCursor < 0) {
                gOption->keyCursor += 8;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_RIGHT) && gOption->keyOld < 0) {
            gOption->keyCursor += 4;
            if (gOption->keyCursor > 7) {
                gOption->keyCursor -= 8;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_UP) && gOption->keyOld < 0) {
            if (gOption->keyCursor == 2 || gOption->keyCursor == 6) {
                gOption->keyCursor++;
            } else {
                gOption->keyCursor--;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_DOWN) && gOption->keyOld < 0) {
            if (gOption->keyCursor == 3 || gOption->keyCursor == 7) {
                gOption->keyCursor--;
            } else {
                gOption->keyCursor++;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_TRIANGLE) && gOption->keyOld < 0) {
            /* original bug: address comparisons again, always true */
            if (gOption->key[0] != gSaveData->keyEdit[0] || gOption->key[1] != gSaveData->keyEdit[1]) {
                gOption->dirty = 1;
            }
            gOption->keyEdit = 0;
            gOption->state = OPT_ST_KEYS_PAD;
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_on_start");
            Snd_PlaySe(1, 2);
        }
        if (gPad[0].gamePressed != 0) {
            switch (gOption->keyCursor) {
            case 0:
                gOption->keyRow = 5;
                break;
            case 1:
                gOption->keyRow = 4;
                break;
            case 2:
                gOption->keyRow = 3;
                break;
            case 3:
                gOption->keyRow = 2;
                break;
            case 4:
                gOption->keyRow = 7;
                break;
            case 5:
                gOption->keyRow = 6;
                break;
            case 6:
                gOption->keyRow = 0;
                break;
            case 7:
                gOption->keyRow = 1;
                break;
            }
            Option_SetKeyMark(gOption->keyRow);
        }
        if (gPad[0].gameHeld & PADG_CROSS) {
            gOption->keyHeld = 1;
            if (gOption->keyOld < 0) {
                gOption->keyOld = gSaveData->keyEdit[gOption->pad][gOption->keyRow];
            }
        }
        if ((gPad[0].gameHeld & PADG_CROSS) && (gPad[0].gamePressed & PADG_LEFT)) {
            if (--gSaveData->keyEdit[gOption->pad][gOption->keyRow] < 0) {
                gSaveData->keyEdit[gOption->pad][gOption->keyRow] = 3;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameHeld & PADG_CROSS) && (gPad[0].gamePressed & PADG_RIGHT)) {
            if (++gSaveData->keyEdit[gOption->pad][gOption->keyRow] > 3) {
                gSaveData->keyEdit[gOption->pad][gOption->keyRow] = 0;
            }
            Snd_PlaySe(1, 0);
        }
        if (gOption->keyHeld == 0) {
            if (gOption->keyOld >= 0) {
                if (gOption->keyOld != gSaveData->keyEdit[gOption->pad][gOption->keyRow]) {
                    for (i = 0; i < OPTION_KEY_NUM; i++) {
                        if (i != gOption->keyRow) {
                            if (gSaveData->keyEdit[gOption->pad][gOption->keyRow] ==
                                gSaveData->keyEdit[gOption->pad][i]) {
                                gSaveData->keyEdit[gOption->pad][i] = gOption->keyOld;
                                break;
                            }
                        }
                    }
                }
                gOption->keyOld = -1;
            }
        }
        break;
    case OPT_ITEM_VIB:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_off_start");
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            gOption->pad = gOption->value = gOption->value ^ 1;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_ok");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_DOWN) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_ok");
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_on_start");
            gOption->pad = gOption->value;
            gOption->state = OPT_ST_VIB_EDIT;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(0)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(1)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            }
            Snd_PlaySe(1, 1);
            if (gOption->value != 0) {
                gOption->voiceLine = 39;
            } else {
                gOption->voiceLine = 40;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->state = OPT_ITEM_CTRL;
            gOption->cursor = OPT_ITEM_VIB;
            Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
            OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            gOption->voiceLine = 24;
            OPT_VOICE(gOption->voiceLine);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ST_VIB_EDIT:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_off_start");
            gOption->value ^= 1;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(0)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_A(0);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_A(0);
                }
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(1)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_A(1);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_A(1);
                }
            }
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_on_start");
            Snd_PlaySe(1, 0);
            if (gOption->value != 0) {
                gOption->voiceLine = 39;
            } else {
                gOption->voiceLine = 40;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_UP) {
            if (gOption->valueOld != gOption->value) {
                gOption->dirty = 1;
            }
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_ok");
            gOption->state = OPT_ITEM_VIB;
            gOption->value = gOption->pad;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ITEM_SAVE:
    case OPT_ITEM_SCR2:
    case OPT_ITEM_STEREO:
    case OPT_ITEM_VOICE:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            gOption->value ^= 1;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_ok");
            Snd_PlaySe(1, 1);
            if (gOption->state == OPT_ITEM_STEREO) {
                if (gOption->valueOld != gOption->value) {
                    gOption->dirty = 1;
                }
                gOption->state = OPT_ITEM_SOUND;
                gSaveData->soundMode = gOption->value;
                SndOpt_Apply();
                Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            } else if (gOption->state == OPT_ITEM_SCR2) {
                if (gOption->valueOld != gOption->value) {
                    gOption->dirty = 1;
                }
                gSaveData->unk1698 = gOption->value;
                gOption->state = OPT_ITEM_SCREEN;
                gOption->cursor = OPT_ITEM_SCR2;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            } else if (gOption->state == OPT_ITEM_SAVE) {
                if (gOption->value == 0) {
                    McFlow_Start(0); /* save */
                    gOption->mcBusy = 1;
                } else {
                    McFlow_Start(1); /* load */
                    gOption->mcBusy = 1;
                }
            } else if (gOption->state == OPT_ITEM_VOICE) {
                gOption->dirty = 1;
                gOption->state = OPT_ITEM_SOUND;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                if (gOption->value != 0) {
                    gSaveData->flags &= ~SAVE_FLAG_VOICE;
                } else {
                    gSaveData->flags |= SAVE_FLAG_VOICE;
                }
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Snd_PlaySe(1, 2);
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            if (gOption->state == OPT_ITEM_STEREO) {
                gOption->value = gSaveData->soundMode;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->state = OPT_ITEM_SOUND;
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                gOption->voiceLine = 11;
                OPT_VOICE(gOption->voiceLine);
            } else if (gOption->state == OPT_ITEM_SCR2) {
                gOption->value = gSaveData->unk1698;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->state = OPT_ITEM_SCREEN;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
                OPT_VOICE(gOption->cursor);
            } else if (gOption->state == OPT_ITEM_SAVE) {
                gOption->state = OPT_ST_TOP;
                gOption->cursor = OPT_ITEM_SAVE;
                OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
                OPT_VOICE(gOption->cursor);
            } else if (gOption->state == OPT_ITEM_VOICE) {
                gOption->state = OPT_ITEM_SOUND;
                gOption->cursor = OPT_ITEM_VOICE;
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                OPT_VOICE(18);
            }
            Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
        }
        break;
    case OPT_ITEM_SCR_RESET:
    case OPT_ST_ADJUST_RESET:
    case OPT_ITEM_SND_RESET:
    case OPT_ITEM_CTRL_RESET:
        Option_UpdateReset();
        break;
    case OPT_ITEM_EXIT:
        if (gOption->dirty != 0) {
            gOption->state = OPT_ST_SAVED;
            McFlow_Start(4);
            gOption->mcBusy = 1;
        } else {
            ColorFade_StartOut(0, 0, 0, 20);
            gOption->state = OPT_ST_FADE;
        }
        break;
    case OPT_ST_SAVED:
        ColorFade_StartOut(0, 0, 0, 20);
        gOption->state = OPT_ST_FADE;
        break;
    case OPT_ST_FADE:
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
        }
        if (ColorFade_IsOutDone()) {
            ret = 0;
        }
        break;
    }
    return ret;
}

/*
 * The key-caption loop of the controller page (the default layout). Case 5 of the first switch leaves uv.x0
 * alone (it is 0 already); cases 6 and 7 store it again. The order of the `uv` stores in each block of
 * Option_Draw was found by search (build/scratch_menu_y/perm.py).
 */
#define DEFAULT_KEYS_LOOP() \
                    for (i = 0; i < 8; i++) { \
                        uv.y0 = 0; \
                        uv.x0 = 0; \
                        switch (gOption->key[gOption->pad][i]) { \
                            case 2: \
                                uv.y0 = 0x50; \
                                uv.x0 = 0x80; \
                                break; \
                            case 1: \
                                uv.y0 = 0x78; \
                                uv.x0 = 0x80; \
                                break; \
                            case 0: \
                                uv.y0 = 0x28; \
                                uv.x0 = 0x80; \
                                break; \
                            case 3: \
                                uv.y0 = 0; \
                                uv.x0 = 0x80; \
                                break; \
                            case 4: \
                                break; \
                            case 5: \
                                uv.y0 = 0x28; \
                                break; \
                            case 6: \
                                uv.x0 = 0; \
                                uv.y0 = 0x50; \
                                break; \
                            case 7: \
                                uv.x0 = 0; \
                                uv.y0 = 0x78; \
                                break; \
                        } \
                        switch (i) { \
                            case 5: \
                                text = 0; \
                                break; \
                            case 4: \
                                text = 1; \
                                break; \
                            case 7: \
                                text = 2; \
                                break; \
                            case 6: \
                                text = 3; \
                                break; \
                            case 3: \
                                text = 4; \
                                break; \
                            case 2: \
                                text = 5; \
                                break; \
                            case 0: \
                                text = 6; \
                                break; \
                            case 1: \
                                text = 7; \
                                break; \
                        } \
                        uv.y1 = uv.y0 + 0x28; \
                        uv.x1 = uv.x0 + 0x80; \
                        sprintf(name, "mc_button_text_%d", text); \
                        Flash_FindLabel(flash, NULL, name, &ref); \
                        Flash_ClipSetUv(flash, &ref, &uv); \
                    }

/* Per frame: sets up every clip of the movie from the screen's state and draws the screen. */
void Option_Draw(void) {
    MFlashRef ref;
    MFlashRef eye;
    MFlashRef mouth;
    char name[0x40];
    MFlashUv uv;
    MFlash *flash;
    s32 i;
    s32 text = 0;
    s32 num = 0;
    s32 y;
    s32 rows4;
    s32 typeY;
    s32 prevY;
    s32 bgmY;
    s32 baseY;
    s32 ctrlY;
    s32 row;
    s32 col;
    s32 id;
    f32 scale;
    f32 fy;
    f32 rows;
    f32 len;

    flash = gOption->flash;
    Sprite_DrawPicture(gOption->bg, 0, 0, 0x80);
    Flash_Advance(gOption->flash);
    Flash_FindLabel(flash, NULL, "mc_dende_eye", &eye);
    Flash_FindLabel(flash, NULL, "mc_dende_mouth", &mouth);
    FlashAnim_Blink(flash, &eye, &gOption->blink, 0);
    FlashAnim_Talk(flash, &mouth, &gOption->talk, 0);

    /* the icon of the page the cursor is in */
    if (gOption->state == YOPT_SCREEN || (gOption->state >= YOPT_TYPE && gOption->state <= YOPT_ADJUST_RESET)) {
        IconWin_SetIcon(1);
    } else if (gOption->state == YOPT_SOUND ||
               (gOption->state >= YOPT_STEREO && gOption->state <= YOPT_SND_RESET && gOption->state != YOPT_BGM)) {
        IconWin_SetIcon(2);
    } else if (gOption->state == YOPT_BGM) {
        IconWin_SetIcon(3);
    } else if (gOption->state == YOPT_CTRL || gOption->state >= YOPT_KEYS) {
        IconWin_SetIcon(4);
    } else {
        IconWin_SetIcon(0);
    }

    switch (gOption->page) {
        case 0:
            rows4 = 4;
            for (i = 0; i < rows4; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 1);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            if (gOption->fromPage == 1) {
                for (i = 0; i < 3; i++) {
                    /* The same variable as the picker's base below: with a variable of its own (or the
                       shift written in place) the constants 0x80 / 0x200 and the picker's base come out in
                       each other's saved registers. */
                    baseY = rows4 << 5;
                    uv.x0 = 0;
                    uv.y0 = i * 0x20 + baseY;
                    uv.x1 = 0x200;
                    uv.y1 = i * 0x20 + 0xA0;
                    sprintf(name, "mc_menu_plate_%d", i + 5);
                    Option_SetMenuText(flash, &ref, name, uv);
                }
                sprintf(name, "mc_menu_plate_%d", 8);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else if (gOption->fromPage == 2 || gOption->fromPage == 3) {
                uv.x0 = 0;
                uv.y0 = 0xE0;
                uv.y1 = 0x100;
                uv.x1 = 0x200;
                sprintf(name, "mc_menu_plate_%d", 5);
                Option_SetMenuText(flash, &ref, name, uv);
                for (i = 0; i < 3; i++) {
                    uv.x0 = 0;
                    uv.y0 = i * 0x20;
                    uv.x1 = 0x200;
                    uv.y1 = i * 0x20 + 0x20;
                    sprintf(name, "mc_menu_plate_%d", i + 6);
                    Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                    Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                }
                if (gOption->fromPage == 2) {
                    sprintf(name, "mc_menu_plate_%d", 8);
                    Flash_FindLabel(flash, NULL, name, &ref);
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                }
            } else {
                for (i = 0; i < 2; i++) {
                    uv.x0 = 0;
                    uv.y0 = i * 0x20 + 0x60;
                    uv.x1 = 0x200;
                    uv.y1 = i * 0x20 + 0x80;
                    sprintf(name, "mc_menu_plate_%d", i + 5);
                    Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                    Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                }
                sprintf(name, "mc_menu_plate_%d", 7);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
                sprintf(name, "mc_menu_plate_%d", 8);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            uv.y0 = 0;
            uv.y1 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Flash_FindLabel(flash, name, "mc_bottom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_bottom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            uv.x0 = 0;
            uv.y0 = 0x20;
            uv.y1 = 0x40;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Option_SetBottomText(flash, &ref, name, uv);
            break;
        case 1:
            for (i = 0; i < 3; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20 + 0x80;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0xA0;
                sprintf(name, "mc_menu_plate_%d", i + 1);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            sprintf(name, "mc_menu_plate_%d", 4);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            for (i = 0; i < 4; i++) {
                uv.x0 = 0;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                uv.y0 = i * 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 5);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            Flash_FindLabel(flash, NULL, "mc_layout_bg", &ref);
            Flash_ClipSetTex(flash, &ref, gSaveData->unk1694 * 2);
            Flash_FindLabel(flash, NULL, "mc_layout_window", &ref);
            Flash_ClipSetTex(flash, &ref, 0);
            uv.x0 = 0;
            uv.y0 = 0x20;
            uv.x1 = 0x100;
            uv.y1 = 0x40;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Option_SetBottomText(flash, &ref, name, uv);
            uv.y0 = 0;
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y1 = 0x20;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Option_SetBottomText(flash, &ref, name, uv);
            uv.y0 = 0;
            uv.y1 = 0x100;
            uv.x0 = 0;
            uv.x1 = 0x100;
            sprintf(name, "mc_type_image_%d", 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            typeY = gOption->type * 0x28;
            uv.y0 = typeY;
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y1 = typeY + 0x28;
            sprintf(name, "mc_type_text_%d", 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            prevY = gOption->typePrev * 0x28;
            uv.y0 = prevY;
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y1 = prevY + 0x28;
            sprintf(name, "mc_type_text_%d", 2);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            uv.y0 = 0;
            uv.y1 = 0x100;
            uv.x1 = 0x100;
            uv.x0 = 0;
            sprintf(name, "mc_type_image_%d", 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->type);
            sprintf(name, "mc_type_image_%d", 2);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->typePrev);
            break;
        case 2:
            uv.x0 = 0;
            uv.y0 = 0xE0;
            uv.y1 = 0x100;
            uv.x1 = 0x200;
            sprintf(name, "mc_menu_plate_%d", 1);
            Option_SetMenuText(flash, &ref, name, uv);
            for (i = 0; i < 3; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 2);
                Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
                Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
            }
            for (i = 0; i < 4; i++) {
                uv.x0 = 0;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                uv.y0 = i * 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 5);
                Option_SetMenuText(flash, &ref, name, uv);
                Flash_ClipSetTex(flash, &ref, 0);
            }
            uv.y0 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y1 = 0x40;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Option_SetBottomText(flash, &ref, name, uv);
            uv.y0 = 0;
            uv.x0 = 0;
            uv.y1 = 0x20;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Option_SetBottomText(flash, &ref, name, uv);
            break;
        case 3:
            for (i = 0; i < 2; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20 + 0x60;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x80;
                sprintf(name, "mc_menu_plate_%d", i + 1);
                Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
                Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
            }
            for (i = 0; i < 4; i++) {
                uv.x0 = 0;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                uv.y0 = i * 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 5);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            sprintf(name, "mc_menu_plate_%d", 3);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            sprintf(name, "mc_menu_plate_%d", 4);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            uv.y0 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y1 = 0x40;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Flash_FindLabel(flash, name, "mc_bottom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_bottom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            uv.y0 = 0;
            uv.y1 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Flash_FindLabel(flash, name, "mc_bottom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_bottom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);

            /* the eight key names: the assignment being edited */
            uv.y0 = 0;
            uv.y1 = 0xA0;
            uv.x0 = 0x30;
            uv.x1 = 0x60;
            Flash_FindLabel(flash, NULL, "mc_button_mark_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            for (i = 0; i < 8; i++) {
                uv.y0 = 0;
                uv.x0 = 0;
                switch (gSaveData->keyEdit[gOption->pad][i]) {
                    case 2:
                        uv.y0 = 0x50;
                        uv.x0 = 0x80;
                        break;
                    case 1:
                        uv.y0 = 0x78;
                        uv.x0 = 0x80;
                        break;
                    case 0:
                        uv.y0 = 0x28;
                        uv.x0 = 0x80;
                        break;
                    case 3:
                        uv.y0 = 0;
                        uv.x0 = 0x80;
                        break;
                    case 4:
                        break;
                    case 5:
                        uv.y0 = 0x28;
                        break;
                    case 6:
                        uv.x0 = 0;
                        uv.y0 = 0x50;
                        break;
                    case 7:
                        uv.x0 = 0;
                        uv.y0 = 0x78;
                        break;
                }
                switch (i) {
                    case 0:
                        text = 6;
                        break;
                    case 1:
                        text = 7;
                        break;
                    case 2:
                        text = 5;
                        break;
                    case 3:
                        text = 4;
                        break;
                    case 4:
                        text = 1;
                        break;
                    case 5:
                        text = 0;
                        break;
                    case 6:
                        text = 3;
                        break;
                    case 7:
                        text = 2;
                        break;
                }
                uv.y1 = uv.y0 + 0x28;
                uv.x1 = uv.x0 + 0x80;
                sprintf(name, "mc_button_text_%d", text);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
            }

            /* the default assignment instead, while the page offers it */
            if (gOption->state == YOPT_KEYS || gOption->state == YOPT_KEYS_PAD || gOption->state == YOPT_CTRL) {
                do {
                    if (gOption->state == YOPT_KEYS || gOption->state == YOPT_CTRL) {
                        if (gOption->value == 0) {
                            if (gSaveData->flags & 8) {
                                break;
                            }
                        } else if (gSaveData->flags & 0x10) {
                            break;
                        }
                    }
                    if (gOption->state == YOPT_KEYS_PAD) {
                        if (gOption->value != 0) {
                            break;
                        }
                    }
                    DEFAULT_KEYS_LOOP();
                } while (0);
            }

            /* the mark on the row being edited */
            if (gOption->keyEdit) {
                switch (gOption->keyRow) {
                    case 0:
                        uv.y0 = 0x50;
                        uv.x0 = 0x30;
                        break;
                    case 1:
                        uv.y0 = 0x78;
                        uv.x0 = 0x30;
                        break;
                    case 2:
                        uv.x0 = 0;
                        uv.y0 = 0x78;
                        break;
                    case 3:
                        uv.x0 = 0;
                        uv.y0 = 0x50;
                        break;
                    case 4:
                        uv.x0 = 0;
                        uv.y0 = 0x28;
                        break;
                    case 5:
                        uv.y0 = 0;
                        uv.x0 = 0;
                        break;
                    case 6:
                        uv.y0 = 0x28;
                        uv.x0 = 0x30;
                        break;
                    case 7:
                        uv.y0 = 0;
                        uv.x0 = 0x30;
                        break;
                }
                uv.y1 = uv.y0 + 0x28;
                uv.x1 = uv.x0 + 0x30;
                Flash_FindLabel(flash, NULL, "mc_button_mark_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetOffset(flash, &ref, gOption->markX, gOption->markY);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_FindLabel(flash, NULL, "mc_button_mark_on", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            break;
    }

    /* the two-way picker's captions */
    if (gOption->picker == 4 || gOption->picker == 2) {
        baseY = 0;
    } else if (gOption->picker == 0) {
        baseY = 0x40;
    } else if (gOption->picker == 3) {
        baseY = 0x80;
    } else {
        baseY = 0xC0;
    }
    for (i = 0; i < 2; i++) {
        uv.x0 = 0;
        uv.y1 = baseY + i * 0x20 + 0x20;
        uv.x1 = 0x100;
        uv.y0 = baseY + i * 0x20;
        sprintf(name, "mc_select_plate_%d", i + 1);
        if (gOption->picker < 7) {
            Flash_FindLabel(flash, name, "mc_select_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (gOption->picker == 2) {
                Flash_ClipSetTex(flash, &ref, 1);
            }
            Flash_FindLabel(flash, name, "mc_select_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (gOption->picker == 2) {
                Flash_ClipSetTex(flash, &ref, 1);
            }
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    ctrlY = 0;
    if (gOption->ctrlKind) {
        ctrlY = 0x40;
    }
    for (i = 0; i < 2; i++) {
        uv.y0 = ctrlY + i * 0x20;
        uv.y1 = ctrlY + i * 0x20 + 0x20;
        uv.x1 = 0x100;
        uv.x0 = 0;
        sprintf(name, "mc_select_plate_%d", i + 3);
        if (gOption->picker != 3 && gOption->picker != 4 && gOption->picker != 0 && gOption->picker != 6 &&
            gOption->picker < 7) {
            Flash_FindLabel(flash, name, "mc_select_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->ctrlKind != 0);
            Flash_FindLabel(flash, name, "mc_select_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->ctrlKind != 0);
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }

    /* the volume window: ten numbered plates per volume */
    for (i = 0; i < 3; i++) {
        for (col = 0; col < 4; col++) {
            uv.y0 = i * 0x20;
            uv.x0 = col * 0x20;
            uv.x1 = col * 0x20 + 0x20;
            uv.y1 = i * 0x20 + 0x20;
            sprintf(name, "mc_vol_plate_bgm_%d", num);
            if (gOption->picker < 7) {
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_FindLabel(flash, name, "mc_vol_num_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, name, "mc_vol_num_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
            }
            sprintf(name, "mc_vol_plate_se_%d", num);
            if (gOption->picker < 7) {
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_FindLabel(flash, name, "mc_vol_num_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, name, "mc_vol_num_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
            }
            num++;
            if (num >= 10) {
                break;
            }
        }
    }
    if (gOption->picker < 7) {
        Flash_FindLabel(flash, NULL, "mc_vol_now_bgm", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, NULL, "mc_vol_now_se", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, NULL, "mc_vol_window", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    /* the sound test's list: six rows and the one scrolling out */
    if (gOption->state == YOPT_BGM) {
        for (i = 0; i < 6; i++) {
            sprintf(name, "mc_bgm_plate_%d", i + 1);
            if (i == 0 || i == 5) {
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetCallbackA(flash, &ref, Option_SetBgmScissor, NULL);
                Flash_ClipSetCallbackB(flash, &ref, Option_ResetScissor, NULL);
            }
            bgmY = gOption->bgmIds[gOption->bgmTop + i] % 8 * 0x20;
            uv.y0 = bgmY;
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y1 = bgmY + 0x20;
            uv.unk10 = gOption->bgmIds[gOption->bgmTop + i] / 8;
            sprintf(name, "mc_bgm_plate_%d", i + 1);
            Flash_FindLabel(flash, name, "mc_bgm_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, uv.unk10);
            Flash_FindLabel(flash, name, "mc_bgm_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, uv.unk10);
        }
        Flash_FindLabel(flash, NULL, "mc_bgm_plate_7", &ref);
        Flash_ClipSetCallbackA(flash, &ref, Option_SetBgmScissor, NULL);
        Flash_ClipSetCallbackB(flash, &ref, Option_ResetScissor, NULL);
        rows = 6.0f;
        bgmY = gOption->bgmIds[gOption->bgmExtra] % 8 * 0x20;
        uv.y1 = bgmY + 0x20;
        uv.x1 = 0x200;
        uv.y0 = bgmY;
        uv.x0 = 0;
        uv.unk10 = gOption->bgmIds[gOption->bgmExtra] / 8;
        Flash_FindLabel(flash, "mc_bgm_plate_7", "mc_bgm_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, uv.unk10);
        Flash_FindLabel(flash, "mc_bgm_plate_7", "mc_bgm_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, uv.unk10);
        len = 7.3f;
        y = 255.49999f / gOption->bgmCount * gOption->bgmTop;
        scale = rows / gOption->bgmCount * len;
        Flash_FindLabel(flash, NULL, "mc_bgm_scroll_bar", &ref);
        Flash_ClipSetScale(flash, &ref, 1.0f, scale);
        Flash_ClipSetOffset(flash, &ref, 0, y);
    }

    /* the volume marks and the arrows */
    uv.y0 = 0;
    uv.x0 = 0;
    uv.x1 = 0x40;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, NULL, "mc_vol_now_bgm", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetOffset(flash, &ref, gOption->bgmVolume * 0x23, 0);
    Flash_FindLabel(flash, NULL, "mc_vol_now_se", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetOffset(flash, &ref, gOption->seVolume * 0x23, 0);
    Flash_FindLabel(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, gOption->bgmTop != 0);
    Flash_FindLabel(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, gOption->bgmBottom < gOption->bgmCount);
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    uv.x0 = 0;
    uv.x1 = 0x20;
    Flash_FindLabel(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.y0 = 0;
    uv.y1 = 0x20;
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, "mc_yajirusi_right", "mc_yajirusi_icon_right", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gOption->state == YOPT_VOLUME && gOption->value == 1) {
        Flash_ClipSetOffset(flash, &ref, 0, 0x70);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetOffset(flash, &ref, 0, 0x70);
    } else if (gOption->state == YOPT_KEYS_EDIT && gOption->keyHeld) {
        Flash_FindLabel(flash, NULL, "mc_yajirusi_right", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_ClipSetOffset(flash, &ref, gOption->markX, gOption->markY);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_ClipSetOffset(flash, &ref, gOption->markX, gOption->markY);
    } else {
        Flash_ClipSetOffset(flash, &ref, 0, 0);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetOffset(flash, &ref, 0, 0);
    }
    if (gOption->state != YOPT_VOLUME && gOption->state != YOPT_TYPE && !gOption->keyHeld) {
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_right", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    for (i = 0; i < OPTION_FLASH_NUM; i++) {
        Flash_Draw(&gOption->flash[i]);
    }
    IconWin_Draw();
    MsgWin_Draw(0, 0, gOption->voiceLine);
    ColorFade_Draw();
    if (gOption->mcBusy) {
        gOption->mcBusy = McFlow_Update();
        if (!gOption->mcBusy) {
            Option_PlateGoto(0, OPT_CLIP_PICK, "fl_on_start");
        }
    } else {
        Dialog_Draw(1);
    }
}

/* Sends one of the screen's plates to a label of its timeline. */
void Option_PlateGoto(s32 unused, s32 clip, char *label) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = gOption->flash;

    switch (clip) {
        case OPT_CLIP_ROW:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor);
            break;
        case OPT_CLIP_ROW_SCREEN:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor - 6);
            break;
        case OPT_CLIP_ROW_SOUND:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor - 11);
            break;
        case OPT_CLIP_BOTTOM:
            sprintf(name, "mc_bottom_plate_%d", 1);
            break;
        case OPT_CLIP_PICK:
            sprintf(name, "mc_select_plate_%d", gOption->value + 1);
            break;
        case OPT_CLIP_VIB_OFF:
            sprintf(name, "mc_select_plate_%d",
                    gOption->pad == 0 ? ((gSaveData->flags & 2) ? 3 : 4) : ((gSaveData->flags & 4) ? 3 : 4));
            break;
        case OPT_CLIP_KEY_OFF:
            sprintf(name, "mc_select_plate_%d",
                    gOption->pad == 0 ? ((gSaveData->flags & 8) ? 4 : 3) : ((gSaveData->flags & 0x10) ? 4 : 3));
            break;
        case OPT_CLIP_BGM:
            sprintf(name, "mc_bgm_plate_%d", gOption->bgmCursor - gOption->bgmTop + 1);
            break;
        case OPT_CLIP_VOL_BGM:
            sprintf(name, "mc_vol_plate_bgm_%d", gSaveData->bgmVolume);
            break;
        case OPT_CLIP_VOL_SE:
            sprintf(name, "mc_vol_plate_se_%d", gSaveData->seVolume);
            break;
        case OPT_CLIP_ROW_CTRL:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor - 16);
            break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Key page: where the cursor mark and the arrows go for a row of the key table; returns the row. */
s32 Option_SetKeyMark(s32 row) {
    s32 ret = 0;

    switch (row) {
        case 0:
            ret = 0;
            gOption->markX = 0xB4;
            gOption->markY = 0x50;
            gOption->arrowX = 0xB4;
            gOption->arrowY = 0x50;
            break;
        case 1:
            ret = 1;
            gOption->markX = 0xB4;
            gOption->markY = 0x78;
            gOption->arrowX = 0xB4;
            gOption->arrowY = 0x78;
            break;
        case 2:
            ret = 2;
            gOption->markX = 0;
            gOption->markY = 0x78;
            gOption->arrowX = 0;
            gOption->arrowY = 0x78;
            break;
        case 3:
            ret = 3;
            gOption->markX = 0;
            gOption->markY = 0x50;
            gOption->arrowX = 0;
            gOption->arrowY = 0x50;
            break;
        case 4:
            ret = 4;
            gOption->markX = 0;
            gOption->markY = 0x28;
            gOption->arrowX = -0xB4;
            gOption->arrowY = 0;
            break;
        case 5:
            ret = 5;
            gOption->markX = 0;
            gOption->markY = 0;
            gOption->arrowX = -0xB4;
            gOption->arrowY = -0x28;
            break;
        case 6:
            ret = 6;
            gOption->markX = 0xB4;
            gOption->markY = 0x28;
            gOption->arrowX = 0;
            gOption->arrowY = 0;
            break;
        case 7:
            ret = 7;
            gOption->markX = 0xB4;
            gOption->markY = 0;
            gOption->arrowX = 0;
            gOption->arrowY = -0x28;
            break;
    }
    return ret;
}

/* The "reset to defaults?" dialog of the three setting pages and of the screen adjustment. */
void Option_UpdateReset(void) {
    s32 answer;
    s32 i;

    switch (gOption->resetStep) {
        case 0:
            Dialog_Init(MPACK_AT(gOption->res, 35), NULL, 0);
            Dialog_SetCursor(1);
            Dialog_SetMsgTable(gOption->dialogMsg);
            Dialog_SetMsg(0);
            Dialog_SetChoices(1);
            Dialog_Start(0);
            gOption->resetStep = 1;
            break;
        case 1:
            answer = Dialog_Input(1);
            if (answer < 0) {
                gOption->resetStep = 3;
            } else if (answer > 0) {
                gOption->resetStep = 2;
            }
            break;
        case 2:
            if (gOption->state == YOPT_SCR_RESET) {
                if (gSaveData->screenX != 0 || gSaveData->screenY != 0 || gSaveData->unk1694 != 0 ||
                    gSaveData->unk1698 != 0) {
                    gOption->dirty = 1;
                }
                gSaveData->screenX = gSaveData->screenY = 0;
                gSaveData->unk1694 = 0;
                gSaveData->unk1698 = 0;
            } else if (gOption->state == YOPT_ADJUST_RESET) {
                if (gSaveData->screenX != 0 || gSaveData->screenY != 0) {
                    gOption->dirty = 1;
                }
                gSaveData->screenX = gSaveData->screenY = 0;
            } else if (gOption->state == YOPT_SND_RESET) {
                if (gSaveData->soundMode != 0 || gSaveData->bgmVolume != 9 || gSaveData->seVolume != 9 ||
                    !(gSaveData->flags & SAVE_FLAG_VOICE)) {
                    gOption->dirty = 1;
                }
                gSaveData->soundMode = 0;
                gSaveData->bgmVolume = 9;
                gSaveData->seVolume = 9;
                gSaveData->flags |= SAVE_FLAG_VOICE;
                SndOpt_Apply();
            } else if (gOption->state == YOPT_CTRL_RESET) {
                if (gSaveData->flags & 8) {
                    gSaveData->flags &= ~8;
                }
                if (gSaveData->flags & 0x10) {
                    gSaveData->flags &= ~0x10;
                }
                if (!(gSaveData->flags & 2)) {
                    gSaveData->flags |= 2;
                }
                if (!(gSaveData->flags & 4)) {
                    gSaveData->flags |= 4;
                }
                for (i = 0; i < 2; i++) {
                    gSaveData->keyEdit[i][0] = 2;
                    gSaveData->keyEdit[i][1] = 1;
                    gSaveData->keyEdit[i][2] = 0;
                    gSaveData->keyEdit[i][3] = 3;
                    gSaveData->keyEdit[i][4] = 4;
                    gSaveData->keyEdit[i][5] = 5;
                    gSaveData->keyEdit[i][6] = 6;
                    gSaveData->keyEdit[i][7] = 7;
                }
            }
            gOption->resetStep = 3;
            break;
        case 3:
            Dialog_SetChoices(0);
            Dialog_Start(1);
            gOption->resetStep = 4;
            break;
        case 4:
            if (Dialog_IsClosed()) {
                if (gOption->state == YOPT_SCR_RESET) {
                    gOption->state = YOPT_SCREEN;
                } else if (gOption->state == YOPT_ADJUST_RESET) {
                    gOption->state = YOPT_ADJUST;
                } else if (gOption->state == YOPT_SND_RESET) {
                    gOption->state = YOPT_SOUND;
                } else {
                    gOption->state = YOPT_CTRL;
                }
                if (gOption->state != YOPT_ADJUST) {
                    Option_PlateGoto(0, OPT_CLIP_BOTTOM, "fl_on_start");
                }
                gOption->resetStep = 0;
            }
            break;
    }
}

/* Gives the two text clips of a bottom plate their rectangle and their first picture. */
void Option_SetBottomText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv) {
    Flash_FindLabel(flash, name, "mc_bottom_text_off", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
    Flash_FindLabel(flash, name, "mc_bottom_text_on", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
}

/* The same for a menu plate. */
void Option_SetMenuText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv) {
    Flash_FindLabel(flash, name, "mc_menu_text_off", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
    Flash_FindLabel(flash, name, "mc_menu_text_on", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
}

/* Dims the four picker plates. */
void Option_DimPickers(void) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = gOption->flash;

    sprintf(name, "mc_select_plate_%d", 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    sprintf(name, "mc_select_plate_%d", 2);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    sprintf(name, "mc_select_plate_%d", 3);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    sprintf(name, "mc_select_plate_%d", 4);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
}

/* Clip callback: limits drawing to the sound test's list. */
void Option_SetBgmScissor(void) {
    Sprite_SetScissor(0, 0x200, 0x45, 0x174);
}

/* Clip callback: back to the whole screen. */
void Option_ResetScissor(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* McFlow "done" callback: the settings are saved. */
void Option_OnSaved(void) {
    gOption->unk194 = 0;
    gOption->dirty = 0;
}

/* Frees the screen. */
void Option_Term(void) {
    s32 i;

    Dialog_Term();
    MsgWin_Term();
    IconWin_Term();
    McFlow_Term();
    for (i = 0; i < OPTION_FLASH_NUM; i++) {
        Flash_Destroy(&gOption->flash[i]);
    }
    if (gOption->res != NULL) {
        Heap_Free(gOption->res);
        gOption->res = NULL;
    }
    if (gOption != NULL) {
        Heap_Free(gOption);
        gOption = NULL;
    }
}
