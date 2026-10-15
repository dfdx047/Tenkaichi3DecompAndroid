#include "common.h"
#include "menu/menu_o.h"
#include "sys/pad.h"

/*
 * DiscFusion, 0x3782A8..0x379908: the screen of mode 24. Two plates, one per earlier game disc; choosing a plate
 * whose disc was not recognised yet starts the disc swap (take this disc out, put the other in, have it
 * identified, put this disc back). Read-only data 0x3B8290..0x3B83AE.
 */

DiscFusion *gDiscFusion = NULL; /* 0x3B735C */

/* Copies the two "recognised" bits of gProgress into the work area. */
void DiscFusion_UpdateHave(void) {
#ifdef PORT
    /* On a PC there is no drive to put the earlier games' discs into, and the game takes nothing from them: it
       only looks for their boot file and sets these two bits. Both discs count as recognised. */
    UO_PROG->discFlags |= UB_DISC_0 | UB_DISC_1;
#endif
    if (UO_PROG->discFlags & UB_DISC_0) {
        gDiscFusion->have[0] = 1;
    } else {
        gDiscFusion->have[0] = 0;
    }
    if (UO_PROG->discFlags & UB_DISC_1) {
        gDiscFusion->have[1] = 1;
    } else {
        gDiscFusion->have[1] = 0;
    }
}

/* Which disc is in the drive: 0 / 1 the two earlier games, 2 this game, 0x63 not readable yet, -1 anything else. */
s32 DiscFusion_Identify(void) {
    s32 result;

    switch (Disc_Identify()) {
    case 0:
        result = 0;
        break;
    case 1:
        result = 1;
        break;
    case 2:
        result = 2;
        break;
    case -2:
        result = DISCFUSION_ID_BUSY;
        break;
    default:
        result = -1;
        break;
    }
    return result;
}

#define DF_RES(n) \
    res = (MTexRes *)MPACK_AT(gDiscFusion->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 3), builds its two movies, windows and dialog. */
void DiscFusion_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gDiscFusion = Heap_Alloc(sizeof(DiscFusion), 0x20, 0, 2);
    memset(gDiscFusion, 0, sizeof(DiscFusion));
    gDiscFusion->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gDiscFusion->res = Sprite_Unpack(gDiscFusion->pack, NULL, NULL);
    DF_RES(8);
    gDiscFusion->bg = res;
    DF_RES(9);
    gDiscFusion->texA[12] = MTEX(res, 0);
    gDiscFusion->texA[13] = MTEX(res, 1);
    gDiscFusion->texA[14] = MTEX(res, 3);
    DF_RES(3);
    gDiscFusion->texA[0] = MTEX(res, 0);
    gDiscFusion->texA[1] = MTEX(res, 1);
    gDiscFusion->texA[11] = MTEX(res, 6);
    gDiscFusion->texA[3] = MTEX(res, 7);
    gDiscFusion->texA[2] = MTEX(res, 8);
    gDiscFusion->texA[7] = MTEX(res, 9);
    gDiscFusion->texA[10] = MTEX(res, 10);
    gDiscFusion->texA[9] = MTEX(res, 11);
    gDiscFusion->texA[4] = MTEX(res, 13);
    gDiscFusion->texA[5] = MTEX(res, 14);
    gDiscFusion->texA[6] = MTEX(res, 15);
    gDiscFusion->texA[15] = MTEX(res, 16);
    gDiscFusion->texA[16] = MTEX(res, 17);
    gDiscFusion->texA[17] = MTEX(res, 18);
    gDiscFusion->texA[8] = NULL;
    gDiscFusion->texB[3] = MTEX(res, 2);
    gDiscFusion->texB[1] = MTEX(res, 3);
    gDiscFusion->texB[2] = MTEX(res, 4);
    gDiscFusion->texB[0] = MTEX(res, 5);
    Flash_Create(&gDiscFusion->flash[0], MPACK_AT(gDiscFusion->res, 1), gDiscFusion->texA);
    Flash_Play(&gDiscFusion->flash[0], 1);
    Flash_Create(&gDiscFusion->flash[1], MPACK_AT(gDiscFusion->res, 2), gDiscFusion->texB);
    Flash_Play(&gDiscFusion->flash[1], 1);
    for (i = 0; i < 2; i++) {
        gDiscFusion->blink[i] = Rand_Libc() % 32;
    }
    gDiscFusion->msgText = MPACK_AT(gDiscFusion->res, 10);
    gDiscFusion->subtitles = MPACK_AT(gDiscFusion->res, 11);
    DF_RES(4);
    IconWin_Init(MPACK_AT(gDiscFusion->res, 6), res);
    IconWin_Open();
    MsgWin_Init(MPACK_AT(gDiscFusion->res, 7), gDiscFusion->msgText, 0, (s32)gDiscFusion->unk220);
    MsgWin_Open();
    gDiscFusion->text = MPACK_AT(gDiscFusion->res, 12);
    Dialog_Init(MPACK_AT(gDiscFusion->res, 5), gDiscFusion->text, 0);
    for (i = 0; i < DISCFUSION_DISC_NUM; i++) {
        TextBox_Init(&gDiscFusion->box[i], gDiscFusion->text, 0);
        TextBox_SetUnk50(&gDiscFusion->box[i], 0);
    }
    DiscFusion_UpdateHave();
    gDiscFusion->voiceLine = -1;
    gDiscFusion->unk268 = 0;
    gDiscFusion->skip = 0;
}

/* Frees the screen. */
void DiscFusion_Term(void) {
    s32 i;

    MsgWin_Term();
    IconWin_Term();
    Dialog_Term();
    for (i = 0; i < DISCFUSION_FLASH_NUM; i++) {
        Flash_Destroy(&gDiscFusion->flash[i]);
    }
    if (gDiscFusion->res != NULL) {
        Heap_Free(gDiscFusion->res);
        gDiscFusion->res = NULL;
    }
    if (gDiscFusion != NULL) {
        Heap_Free(gDiscFusion);
        gDiscFusion = NULL;
    }
}

/* Draws the screen: guide, the two plates, the icon of the plate under the cursor, windows and dialog. */
void DiscFusion_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;
    s32 x;
    s32 y;

    Sprite_DrawPicture(gDiscFusion->bg, 0, 0, 0x80);
    flash = &gDiscFusion->flash[0];
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gDiscFusion->blink[0], 0);
    Flash_FindLabel(flash, "mc_guide_17go", "mc_guide_17go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gDiscFusion->talk[0], 0);
    for (i = 0; i < DISCFUSION_DISC_NUM; i++) {
        sprintf(name, "mc_disk_plate%02d", i);
        Flash_FindLabel(flash, name, "mc_disk_text", &ref);
        if (gDiscFusion->have[i]) {
            TextBox_AttachLine(flash, &ref, 0, 0, i + 0x15C, &gDiscFusion->box[i]);
        } else {
            TextBox_AttachLine(flash, &ref, 0, 0, i + 0x15A, &gDiscFusion->box[i]);
        }
    }
    x = 0;
    y = 0;
    switch (gDiscFusion->cursor[gDiscFusion->level]) {
    case 0:
        if (gDiscFusion->have[0]) {
            x = 0;
            y = 0x40;
        } else {
            x = 0;
            x = gDiscFusion->toggle * 0x40;
        }
        break;
    case 1:
        if (gDiscFusion->have[1]) {
            x = 0;
            y = 0x80;
        } else {
            x = 0;
            x = gDiscFusion->toggle * 0x40;
        }
        break;
    }
    uv.x0 = x;
    uv.y0 = y;
    uv.x1 = x + 0x40;
    uv.y1 = y + 0x40;
    Flash_FindLabel(flash, NULL, "mc_icon_play1", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    gDiscFusion->toggle ^= 1;
    Flash_Draw(&gDiscFusion->flash[0]);
    IconWin_Draw();
    MsgWin_Draw(0, 0, gDiscFusion->voiceLine);
    Dialog_Draw(1);
    Flash_Draw(&gDiscFusion->flash[1]);
}

/* Per frame: the leave timer, the plate's line once the greeting has ended, the movies. */
void DiscFusion_Update(void) {
    s32 i;

    if (UO_PROG->flags & MPROG_FREEZE) {
        return;
    }
    if (gDiscFusion->timer > 0) {
        gDiscFusion->timer--;
    }
    if (gDiscFusion->flags & UBRES_FADED_IN) {
        if (gDiscFusion->dlgStep == 0 && (gDiscFusion->voiceLine == 0x40 || gDiscFusion->voiceLine == 0x41)) {
            if (Voice_GetStat(0) == UB_VOICE_IDLE_O) {
                gDiscFusion->talkStep = gDiscFusion->cursor[gDiscFusion->level] + 3;
            }
        }
    }
    for (i = 0; i < DISCFUSION_FLASH_NUM; i++) {
        Flash_Advance(&gDiscFusion->flash[i]);
    }
}

/* The disc swap: a dialog driven by the drive's state. */
void DiscFusion_UpdateDisc(void) {
    s32 state;
    s32 paused;
    s32 open;
    s32 noType;
    s32 stopped;
    s32 answer;

    if (gDiscFusion->dlgStep == 0) {
        return;
    }
    state = Disc_GetDriveState();
    stopped = state == DISC_STOPPED;
    open = state == DISC_TRAY_OPEN;
    paused = state == DISC_PAUSED;
    noType = state == DISC_NO_TYPE;
    switch (gDiscFusion->dlgStep) {
    case DISCFUSION_DLG_ASK:
        Dialog_SetChoices(1);
        Dialog_SetCursor(1);
        Dialog_Start(0);
        gDiscFusion->dlgStep = DISCFUSION_DLG_CONFIRM;
        Voice_FadeOutStep(0);
        Bgm_FadeOutStep();
        break;
    case DISCFUSION_DLG_CONFIRM:
        Dialog_SetMsg(gDiscFusion->chosen + 0x14);
        answer = Dialog_Input(0);
        if (answer < 0) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_CLOSE;
            Dialog_Start(1);
        } else if (answer > 0) {
            Dialog_SetChoices(0);
            gDiscFusion->dlgStep = DISCFUSION_DLG_OPEN;
        }
        break;
    case DISCFUSION_DLG_OPEN:
        Dialog_SetMsg(0x16);
        if (open) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_INSERT;
        }
        break;
    case DISCFUSION_DLG_INSERT:
        Dialog_SetMsg(gDiscFusion->chosen + 0x17);
        if (stopped) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_READ;
            Dialog_SetMsg(0x19);
            gDiscFusion->tries = 0;
            gDiscFusion->timeout = DISCFUSION_TIMEOUT;
        }
        break;
    case DISCFUSION_DLG_READ:
        if (paused) {
            if (gDiscFusion->wait == 0) {
                s32 r = DiscFusion_Identify();

                if (r == DISCFUSION_ID_BUSY) {
                    break;
                }
                if (gDiscFusion->chosen == r) {
                    gDiscFusion->dlgStep = DISCFUSION_DLG_FOUND;
                    Dialog_Start(1);
                    break;
                }
                if (gDiscFusion->tries == DISCFUSION_TRIES) {
                    Dialog_SetChoices(1);
                    Dialog_SetCursor(1);
                    gDiscFusion->dlgStep = DISCFUSION_DLG_WRONG;
                    gDiscFusion->error = 0;
                    break;
                }
                gDiscFusion->tries++;
                gDiscFusion->wait = 3;
            } else {
                gDiscFusion->wait--;
            }
        }
        if (open) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_INSERT;
        } else if (noType) {
            if (gDiscFusion->timeout == 0) {
                Dialog_SetChoices(1);
                Dialog_SetCursor(1);
                gDiscFusion->dlgStep = DISCFUSION_DLG_WRONG;
                gDiscFusion->error = 1;
            } else {
                gDiscFusion->timeout--;
            }
        }
        break;
    case DISCFUSION_DLG_FOUND:
        if (gDiscFusion->chosen == 0) {
            UO_PROG->discFlags |= UB_DISC_0;
        } else {
            UO_PROG->discFlags |= UB_DISC_1;
        }
        DiscFusion_UpdateHave();
        Flash_GotoLabel(&gDiscFusion->flash[1], "fl_start", 1);
        gDiscFusion->dlgStep = DISCFUSION_DLG_FOUND_ANIM;
        break;
    case DISCFUSION_DLG_FOUND_ANIM:
        if (gDiscFusion->flash[1].flags & MFLASH_PAD) {
            Dialog_Start(0);
            Dialog_SetMsg(0x1B);
            gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_OPEN;
        }
        break;
    case DISCFUSION_DLG_BACK_OPEN:
        if (open) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_INSERT;
        }
        break;
    case DISCFUSION_DLG_BACK_INSERT:
        Dialog_SetMsg(0x1C);
        if (stopped) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_READ;
            gDiscFusion->tries = 0;
            gDiscFusion->timeout = DISCFUSION_TIMEOUT;
        }
        break;
    case DISCFUSION_DLG_BACK_READ:
        Dialog_SetMsg(0x19);
        if (paused) {
            if (gDiscFusion->wait == 0) {
                s32 r = DiscFusion_Identify();

                if (r == DISCFUSION_ID_BUSY) {
                    break;
                }
                if (r == DISCFUSION_ID_OWN) {
                    gDiscFusion->dlgStep = DISCFUSION_DLG_CLOSE;
                    Dialog_Start(1);
                    break;
                }
                if (gDiscFusion->tries == DISCFUSION_TRIES) {
                    gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_WRONG;
                    gDiscFusion->error = 0;
                    break;
                }
                gDiscFusion->tries++;
                gDiscFusion->wait = 3;
            } else {
                gDiscFusion->wait--;
            }
        }
        if (open) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_INSERT;
        } else if (noType) {
            if (gDiscFusion->timeout == 0) {
                gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_WRONG;
                gDiscFusion->error = 1;
            } else {
                gDiscFusion->timeout--;
            }
        }
        break;
    case DISCFUSION_DLG_WRONG:
        if (gDiscFusion->error) {
            Dialog_SetMsg(0x1E);
        } else {
            Dialog_SetMsg(0x1A);
        }
        answer = Dialog_Input(0);
        if (answer < 0) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_OPEN;
            Dialog_SetChoices(0);
        } else if (answer > 0) {
            Dialog_SetChoices(0);
            gDiscFusion->dlgStep = DISCFUSION_DLG_WRONG_OPEN;
        }
        break;
    case DISCFUSION_DLG_WRONG_OPEN:
        Dialog_SetMsg(0x16);
        if (open) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_INSERT;
        }
        break;
    case DISCFUSION_DLG_BACK_WRONG:
        if (gDiscFusion->error) {
            Dialog_SetMsg(0x1F);
        } else {
            Dialog_SetMsg(0x1D);
        }
        if (open) {
            gDiscFusion->dlgStep = DISCFUSION_DLG_BACK_INSERT;
        }
        break;
    case DISCFUSION_DLG_CLOSE:
        if (Dialog_IsClosed()) {
            Adx_StopAll();
            Bgm_Play(UB_BGM);
            gDiscFusion->dlgStep = 0;
            if (gDiscFusion->chosen == 0) {
                if (UO_PROG->discFlags & UB_DISC_0) {
                    gDiscFusion->talkStep = 6;
                } else {
                    gDiscFusion->talkStep = 7;
                }
            } else {
                if (UO_PROG->discFlags & UB_DISC_1) {
                    gDiscFusion->talkStep = 6;
                } else {
                    gDiscFusion->talkStep = 7;
                }
            }
        }
        break;
    }
}

/* The guide's lines: waits for the running line (or its skip), then starts the next one. */
void DiscFusion_UpdateTalk(void) {
    if (UO_PROG->flags & MPROG_FREEZE) {
        return;
    }
    if (gDiscFusion->skip == 0) {
        if (gDiscFusion->talkStep == 0) {
            return;
        }
        if (gDiscFusion->voiceLine != -1 && Voice_GetStat(0) != UB_VOICE_IDLE_O &&
            gDiscFusion->talkStep == gDiscFusion->talkPrev) {
            if (gPad[0].gamePressed & 0x200) {
                Snd_PlaySe(1, 1);
                gDiscFusion->skip = 1;
            }
            return;
        }
    } else {
        gDiscFusion->skip = 0;
    }
    switch (gDiscFusion->talkStep) {
    case 1:
        gDiscFusion->voiceLine = 0x40;
        break;
    case 2:
        gDiscFusion->voiceLine = 0x41;
        break;
    case 3:
        if (gDiscFusion->have[0]) {
            gDiscFusion->voiceLine = 0x52;
        } else {
            gDiscFusion->voiceLine = 0x42;
        }
        break;
    case 4:
        if (gDiscFusion->have[1]) {
            gDiscFusion->voiceLine = 0x53;
        } else {
            gDiscFusion->voiceLine = 0x43;
        }
        break;
    case 5:
        gDiscFusion->voiceLine = 0x46;
        break;
    case 6:
        gDiscFusion->voiceLine = 0x47;
        break;
    case 7:
        gDiscFusion->voiceLine = 0x48;
        break;
    }
    if (gDiscFusion->talkStep == 6) {
        gDiscFusion->talkStep = gDiscFusion->cursor[gDiscFusion->level] + 3;
    } else {
        gDiscFusion->talkStep = 0;
    }
    Voice_PlayWithSubtitle(gDiscFusion->subtitles, UB_VOICE_BASE_O, gDiscFusion->voiceLine);
    gDiscFusion->talkPrev = gDiscFusion->talkStep;
}

/*
 * Snd_PlaySe returns a value in the original's prototype (menu_a.h declares it void): with a void call the
 * pointer for the store that follows a call is put in v0, the original has it in v1. Only this function of the
 * file shows the difference.
 */
extern s32 Snd_PlaySe_ret(u32 mask, s32 id) __asm__("Snd_PlaySe");

/* Pad 0: up / down move between the two plates, confirm picks one, cancel leaves. */
void DiscFusion_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & 8;
    s32 down = gPad[0].gameRepeat & 4;
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 cancel = gPad[0].gamePressed & 0x400;

    if (!(gDiscFusion->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gDiscFusion->flags & UBRES_STARTED)) {
        if (gDiscFusion->have[0] && gDiscFusion->have[1]) {
            gDiscFusion->talkStep = 2;
        } else {
            gDiscFusion->talkStep = 1;
        }
        DiscFusion_PlateGoto(0, 0, "fl_on_start");
        gDiscFusion->flags |= UBRES_STARTED;
    }
    switch (gDiscFusion->level) {
    case 0:
        if (up) {
            DiscFusion_PlateGoto(0, 0, "fl_off_start");
            if (--gDiscFusion->cursor[gDiscFusion->level] < 0) {
                gDiscFusion->cursor[gDiscFusion->level] = 1;
            }
            DiscFusion_PlateGoto(0, 0, "fl_on_start");
            Snd_PlaySe_ret(1, 0);
            gDiscFusion->idle = 0;
            gDiscFusion->talkStep = gDiscFusion->cursor[gDiscFusion->level] + 3;
        } else if (down) {
            DiscFusion_PlateGoto(0, 0, "fl_off_start");
            if (++gDiscFusion->cursor[gDiscFusion->level] >= 2) {
                gDiscFusion->cursor[gDiscFusion->level] = 0;
            }
            DiscFusion_PlateGoto(0, 0, "fl_on_start");
            Snd_PlaySe_ret(1, 0);
            gDiscFusion->idle = 0;
            gDiscFusion->talkStep = gDiscFusion->cursor[gDiscFusion->level] + 3;
        } else if (ok) {
            if (gDiscFusion->have[gDiscFusion->cursor[0]]) {
                gDiscFusion->flags |= UBRES_DONE;
                gDiscFusion->flags |= UBRES_LEAVING;
                gDiscFusion->timer = 15;
            } else {
                gDiscFusion->chosen = gDiscFusion->cursor[0];
                gDiscFusion->dlgStep = DISCFUSION_DLG_ASK;
                Voice_StopWithLip();
            }
            Snd_PlaySe_ret(1, 1);
            gDiscFusion->idle = 0;
        } else if (cancel) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&gDiscFusion->flash[0], "fl_window_out", 1);
            Snd_PlaySe_ret(1, 2);
            gDiscFusion->idle = 0;
        } else {
            gDiscFusion->idle++;
            if (gDiscFusion->idle == DISCFUSION_IDLE) {
                gDiscFusion->talkStep = 5;
                gDiscFusion->idle = 0;
            }
        }
        break;
    }
}

/* Sends the plate under the cursor of `level` to a label of its clip. */
void DiscFusion_PlateGoto(s32 movie, s32 level, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gDiscFusion->flash[movie];

    sprintf(name, "mc_disk_plate%02d", gDiscFusion->cursor[level]);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * The screen of mode 24. Returns 1 when a recognised disc's plate was chosen (gProgress->cursor = 0 / 1), 0 when
 * the player backed out.
 */
s32 DiscFusion_Run(s32 section) {
    s32 result = 1;

    DiscFusion_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        DiscFusion_Update();
        DiscFusion_UpdateDisc();
        DiscFusion_UpdateTalk();
        DiscFusion_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gDiscFusion->flags & UBRES_FADED_IN) && (gDiscFusion->flash[0].flags & MFLASH_PAD)) {
                gDiscFusion->flags |= UBRES_FADED_IN;
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
        if (gDiscFusion->flags & UBRES_LEAVING) {
            if (gDiscFusion->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                UO_PROG->cursor = gDiscFusion->cursor[0];
            }
        } else if (gDiscFusion->dlgStep == 0 && gDiscFusion->talkStep == 0) {
            DiscFusion_Input(&result);
        }
    }
    DiscFusion_Term();
    Dma_ResetBuffers();
    return result;
}
