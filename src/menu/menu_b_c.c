#include "common.h"
#include "menu/menu_b.h"
#include "sys/pad.h"
#include "sys/save.h"

HistOutro *gHistOutro = NULL; /* 0x3B12F8 */

/*
 * HistOutro, 0x33ABA0..0x33CBF8: the scene shown when a saga of the story mode was completed (progress mode 9):
 * the saga's guide and a partner talk through a scripted dialogue, then the saga's closing text scrolls in and is
 * read out. Hist_Main, 0x33CBF8..0x33CFC8: the handler of progress modes 6..10 (and 3, the ending movie).
 * A new object starts at 0x33ABA0: its strings ("mc_guide_%02d", ...) repeat those of ModeMenu.
 */

extern char *strcpy(char *, const char *);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Flash_StepFrames(MFlash *flash, s32 step);
extern void Voice_StopWithLip(void);
extern void StreamSe_FadeOutStep(s32 se);
extern void Movie_PlayEnding(void);
extern s32 HistSel_Run(s32 section);
extern s32 HistResult_Run(s32 section);
extern s32 HistSave_Run(s32 section);

/* Moves one of the two guides in or out: side 0 is the saga's own guide (right), side 1 the partner (left). */
void HistOutro_GuideGoto(s32 movie, s32 side, s32 out) {
    MFlashRef ref;
    char name[64];
    char label[64];
    MFlash *flash = &gHistOutro->flash[movie];

    if (side == 0) {
        sprintf(name, "mc_guide_%02d", gProgress->subMenu);
        strcpy(label, out ? "fl_right_out" : "fl_right_in");
    } else {
        sprintf(name, "mc_guide_%02d", gHistOutro->partner);
        strcpy(label, out ? "fl_left_out" : "fl_left_in");
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

#define OUTRO_RES(pack, n) \
    res = (MTexRes *)MPACK_AT(pack, n); \
    Res_RelocateOffsets(&res, res, res)

#define OUTRO_ICONS(a, b, c) \
    gHistOutro->tex[a] = MTEX(res, 0); \
    gHistOutro->tex[b] = MTEX(res, 1); \
    gHistOutro->tex[c] = MTEX(res, 3)

#define OUTRO_SETUP(max, voice, line, other) \
    gHistOutro->itemMax = (max); \
    gHistOutro->voiceBase = (voice); \
    gHistOutro->lineBase = (line); \
    gHistOutro->partner = (other)

/* The first completion of this saga counts (the outro was not seen yet). */
#define OUTRO_COUNT(n) \
    if (!(gSaveData->slot[n].flags & SAVESLOT_OUTRO_SEEN)) { \
        gSaveData->unk100C++; \
        gSaveData->unk1008 |= 1; \
    }

#define OUTRO_BGM 0x10B1C

/* Loads the screen (section `section` of archive 2 and the saga's own file) and starts the music. */
void HistOutro_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    SaveSlot *slot;

    gHistOutro = Heap_Alloc(0x2C0, 0x20, 0, 2);
    memset(gHistOutro, 0, 0x2C0);
    gHistOutro->pack = (u32 *)MPACK_AT(gMenuArc2, section);
    gHistOutro->res = Sprite_Unpack(gHistOutro->pack, NULL, NULL);
    switch (gProgress->subMenu) {
    case 0:
        OUTRO_SETUP(3, 0x8537, 8, 1);
        break;
    case 1:
        OUTRO_SETUP(4, 0x8423, 4, 2);
        break;
    case 2:
        OUTRO_SETUP(7, 0x83B4, 9, 3);
        break;
    case 3:
        OUTRO_SETUP(5, 0x83DB, 10, 5);
        break;
    case 4:
        OUTRO_SETUP(16, 0x8484, 9, 8);
        break;
    case 5:
        OUTRO_SETUP(5, 0x8440, 10, 6);
        break;
    case 6:
        OUTRO_SETUP(4, 0x8402, 6, 8);
        break;
    case 7:
        OUTRO_SETUP(4, 0x8466, 8, 8);
        break;
    }
    gHistOutro->file = File_LoadSync(gProgress->baseFile + gProgress->subMenu + 0x10, NULL, 0);
    gHistOutro->fileRes = Sprite_Unpack(gHistOutro->file, NULL, NULL);
    ModeBg_Init(gHistOutro->fileRes);
    OUTRO_RES(gHistOutro->fileRes, 7);
    switch (gProgress->subMenu) {
    case 0:
        OUTRO_ICONS(1, 2, 3);
        break;
    case 1:
        OUTRO_ICONS(4, 5, 6);
        break;
    case 2:
        OUTRO_ICONS(7, 8, 9);
        break;
    case 3:
        OUTRO_ICONS(10, 12, 11);
        break;
    case 4:
        OUTRO_ICONS(13, 14, 15);
        break;
    case 5:
        OUTRO_ICONS(16, 17, 18);
        break;
    case 6:
        OUTRO_ICONS(19, 20, 21);
        break;
    case 7:
        OUTRO_ICONS(22, 23, 24);
        break;
    }
    OUTRO_RES(gHistOutro->fileRes, 8);
    switch (gHistOutro->partner) {
    case 0:
        OUTRO_ICONS(1, 2, 3);
        break;
    case 1:
        OUTRO_ICONS(4, 5, 6);
        break;
    case 2:
        OUTRO_ICONS(7, 8, 9);
        break;
    case 3:
        OUTRO_ICONS(10, 12, 11);
        break;
    case 4:
        OUTRO_ICONS(13, 14, 15);
        break;
    case 5:
        OUTRO_ICONS(16, 17, 18);
        break;
    case 6:
        OUTRO_ICONS(19, 20, 21);
        break;
    case 7:
        OUTRO_ICONS(22, 23, 24);
        break;
    case 8:
        OUTRO_ICONS(25, 26, 27);
        break;
    }
    OUTRO_RES(gHistOutro->res, 1);
    gHistOutro->tex[29] = MTEX(res, 7);
    OUTRO_RES(gHistOutro->fileRes, 2);
    gHistOutro->tex[30] = MTEX(res, 0);
    OUTRO_RES(gHistOutro->fileRes, 3);
    gHistOutro->tex[28] = MTEX(res, 0);
    OUTRO_RES(gHistOutro->res, 4);
    gHistOutro->tex[0] = MTEX(res, 0);
    Flash_Create(&gHistOutro->flash[0], MPACK_AT(gHistOutro->res, 5), gHistOutro->tex);
    Flash_Play(&gHistOutro->flash[0], 1);
    gHistOutro->msgText = MPACK_AT(gHistOutro->fileRes, 4);
    gHistOutro->subtitles = MPACK_AT(gHistOutro->fileRes, 5);
    MsgWin_Init(MPACK_AT(gHistOutro->res, 8), gHistOutro->msgText, 1, 0);
    gHistOutro->text = MPACK_AT(gHistOutro->res, 6);
    gHistOutro->descr = (ModeMenuDescr *)MPACK_AT(gHistOutro->fileRes, 6);
    gHistOutro->voiceLine = -1;
    for (i = 0; i < 2; i++) {
        gHistOutro->blink[i] = Rand_Range(0x20);
    }
    for (i = 0; i < 3; i++) {
        TextBox_Init(&gHistOutro->box[i], gHistOutro->text, 0);
        TextBox_SetUnk80(&gHistOutro->box[i], 1);
        TextBox_SetUnk50(&gHistOutro->box[i], 1);
    }

    switch (gProgress->subMenu) {
    case 0:
        Bgm_Play(DISC_RESOURCE(OUTRO_BGM, 0x899));
        StreamSe_PlayDefault(0, 0x10BDC);
        break;
    case 1:
        Bgm_Play(DISC_RESOURCE(OUTRO_BGM, 0x834));
        StreamSe_PlayDefault(0, 0x10BDD);
        OUTRO_COUNT(1);
        break;
    case 2:
        Bgm_Play(DISC_RESOURCE(OUTRO_BGM, 0x834));
        StreamSe_PlayDefault(0, 0x10BDE);
        break;
    case 3:
        Bgm_Play(DISC_RESOURCE(OUTRO_BGM, 0x835));
        break;
    case 4:
        Bgm_Play(DISC_RESOURCE(OUTRO_BGM, 0x86C));
        OUTRO_COUNT(4);
        break;
    case 5:
        Bgm_Play(DISC_RESOURCE(0x10B1E, 0x837));
        OUTRO_COUNT(5);
        break;
    case 6:
        Bgm_Play(DISC_RESOURCE(0x10B28, 0x828));
        break;
    case 7:
        Bgm_Play(DISC_RESOURCE(OUTRO_BGM, 0x867));
        OUTRO_COUNT(7);
        break;
    }
    slot = gSaveData->slot;
    slot += gProgress->subMenu;
    slot->flags |= SAVESLOT_OUTRO_SEEN;
}

#define OUTRO_FREE(p) \
    if ((p) != NULL) { \
        Heap_Free(p); \
        (p) = NULL; \
    }

/* Frees the screen. */
void HistOutro_Term(void) {
    s32 i;

    ModeBg_Term();
    MsgWin_Term();
    for (i = 0; i < HISTOUTRO_FLASH_NUM; i++) {
        Flash_Destroy(&gHistOutro->flash[i]);
    }
    OUTRO_FREE(gHistOutro->fileRes);
    OUTRO_FREE(gHistOutro->file);
    OUTRO_FREE(gHistOutro->res);
    OUTRO_FREE(gHistOutro);
}

/* Sets up the clips of the movie (both guides' faces, the three lines of the closing text) and draws. */
void HistOutro_Draw(void) {
    MFlashRef ref;
    char name[64];
    char sub[64];
    s32 i;
    s32 guide;
    MFlash *flash;

    ModeBg_Draw();
    flash = &gHistOutro->flash[0];
    for (i = 0; i < 2; i++) {
        if (i != 0) {
            guide = gHistOutro->partner;
        } else {
            guide = gProgress->subMenu;
        }
        sprintf(name, "mc_guide_%02d", guide);
        sprintf(sub, "mc_guide_%02d_eye", guide);
        Flash_FindLabel(flash, name, sub, &ref);
        FlashAnim_Blink(flash, &ref, &gHistOutro->blink[i], 0);
        sprintf(sub, "mc_guide_%02d_mouth", guide);
        Flash_FindLabel(flash, name, sub, &ref);
        if (gHistOutro->speaker == i) {
            FlashAnim_Talk(flash, &ref, &gHistOutro->talk[i], 0);
        } else {
            FlashAnim_ShowNext2(flash, &ref, 0);
        }
    }
    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_episode_text_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i >= gHistOutro->descr[gHistOutro->itemMax].count) {
            TextBox_AttachLine(flash, &ref, 0, -(s32)gHistOutro->scroll, -1, &gHistOutro->box[i]);
        } else {
            TextBox_AttachLine(flash, &ref, 0, -(s32)gHistOutro->scroll,
                               gHistOutro->descr[gHistOutro->itemMax].line + i, &gHistOutro->box[i]);
        }
    }
    for (i = 0; i < HISTOUTRO_FLASH_NUM; i++) {
        Flash_Draw(&gHistOutro->flash[i]);
    }
    Font_FlushAll();
    MsgWin_Draw(0, 0, gHistOutro->voiceLine);
}

/* Advances the movie; brings the guide(s) in when the movie asks for it; scrolls the closing text. */
void HistOutro_Update(void) {
    s32 i;

    for (i = 0; i < HISTOUTRO_FLASH_NUM; i++) {
        Flash_Advance(&gHistOutro->flash[i]);
    }
    if (!(gHistOutro->flags & HISTOUTRO_STARTED) && (gHistOutro->flash[0].trig & 2)) {
        MsgWin_Open();
        switch (gProgress->subMenu) {
        case 4:
            HistOutro_GuideGoto(0, 0, 0);
        case 7:
            HistOutro_GuideGoto(0, 1, 0);
            break;
        default:
            HistOutro_GuideGoto(0, 0, 0);
            break;
        }
    }
    if (gHistOutro->flags & HISTOUTRO_SCROLLING) {
        gHistOutro->scroll += 0.29166667f;
        if (gHistOutro->scroll >= gHistOutro->scrollMax) {
            gHistOutro->scroll = gHistOutro->scrollMax;
            gHistOutro->flags &= ~HISTOUTRO_SCROLLING;
        }
    }
}

/* Pad handling once the script has ended: confirm skips the scroll, then leaves; start leaves at once. */
void HistOutro_Input(s32 *result) {
    if (!(gHistOutro->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gHistOutro->flags & HISTOUTRO_STARTED)) {
        gHistOutro->flags |= HISTOUTRO_STARTED;
    }
    if (gHistOutro->focus != 0) {
        return;
    }
    if (gPad[0].gamePressed & 0x200) {
        if (gHistOutro->flags & HISTOUTRO_SCROLLING) {
            Voice_StopWithLip();
            gHistOutro->scroll = gHistOutro->scrollMax;
            gHistOutro->flags &= ~HISTOUTRO_SCROLLING;
        } else {
            gHistOutro->flags |= HISTOUTRO_CHOSEN;
            gHistOutro->flags |= HISTOUTRO_LEAVING;
            gHistOutro->timer = 15;
        }
        Snd_PlaySe(1, 1);
    } else if (gPad[0].gamePressed & 0x1000) {
        gHistOutro->flags |= HISTOUTRO_CHOSEN;
        gHistOutro->flags |= HISTOUTRO_LEAVING;
        gHistOutro->timer = 15;
        Snd_PlaySe(1, 1);
    }
}

#define OUTRO_PLAY() Voice_PlayWithSubtitle(gHistOutro->subtitles, gHistOutro->voiceBase, gHistOutro->voiceLine)

/* The first line of the dialogue, said by guide `who` (0 = the saga's, 1 = the partner). */
#define OUTRO_SAY_FIRST(who) \
    gHistOutro->speaker = (who); \
    gHistOutro->voiceLine = gHistOutro->lineBase; \
    OUTRO_PLAY(); \
    gHistOutro->step++

/* The next line of the dialogue. */
#define OUTRO_SAY_NEXT(who) \
    gHistOutro->speaker = (who); \
    gHistOutro->voiceLine++; \
    OUTRO_PLAY(); \
    gHistOutro->step++

/* Waits for the end of the line; confirm goes on at once (the voice is not stopped). */
#define OUTRO_WAIT() \
    if (Voice_GetStat(0) == 5) { \
        gHistOutro->step++; \
    } else if (gPad[0].gamePressed & 0x200) { \
        gHistOutro->step++; \
        Snd_PlaySe(1, 1); \
    }

/* Waits for the movie's trigger 4 (a guide has arrived or left). */
#define OUTRO_WAIT_MOVIE() \
    if (gHistOutro->flash[0].trig & 4) { \
        gHistOutro->step++; \
    }

/* A line of the voice file that this dialogue leaves out. */
#define OUTRO_SKIP_LINE() \
    gHistOutro->voiceLine++; \
    gHistOutro->step++; \
    gHistOutro->step++

/* The scene's script: step 101 + 50 * saga starts that saga's dialogue; 51 closes it; 501.. is the closing text. */
void HistOutro_UpdateStep(void) {
    SaveSlot *slot;

    if (gHistOutro->step == 0) {
        return;
    }
    switch (gHistOutro->step) {
    case 1:
        slot = gSaveData->slot;
        slot += gProgress->subMenu;
        slot->flags |= SAVESLOT_OUTRO_STARTED;
        gHistOutro->step++;
        gHistOutro->voiceLine = -1;
        break;
    case 2:
        gHistOutro->step = gProgress->subMenu * 50 + 101;
        break;
    case OUTRO_STEP_CLOSE:
        Flash_StepFrames(&gHistOutro->flash[0], 1);
        MsgWin_Close();
        gHistOutro->step = OUTRO_STEP_TEXT;
        break;
    /* saga 0 */
    case 101:
        OUTRO_SAY_FIRST(0);
        break;
    case 103:
    case 109:
    case 115:
    case 117:
    case 119:
        OUTRO_SAY_NEXT(0);
        break;
    case 107:
    case 111:
    case 113:
    case 121:
    case 123:
        OUTRO_SAY_NEXT(1);
        break;
    case 102:
    case 104:
    case 108:
    case 110:
    case 112:
    case 114:
    case 116:
    case 118:
    case 120:
    case 122:
    case 124:
        OUTRO_WAIT();
        break;
    case 105:
        HistOutro_GuideGoto(0, 1, 0);
        gHistOutro->step++;
        break;
    /* saga 1 */
    case 151:
        OUTRO_SAY_FIRST(0);
        break;
    case 153:
    case 155:
    case 159:
    case 163:
        OUTRO_SAY_NEXT(0);
        break;
    case 161:
    case 167:
    case 169:
        OUTRO_SAY_NEXT(1);
        break;
    case 152:
    case 154:
    case 156:
    case 160:
    case 162:
    case 164:
    case 168:
    case 170:
        OUTRO_WAIT();
        break;
    case 165:
        HistOutro_GuideGoto(0, 0, 1);
        gHistOutro->step++;
        break;
    case 157:
        HistOutro_GuideGoto(0, 1, 0);
        gHistOutro->step++;
        break;
    /* saga 2 */
    case 201:
        OUTRO_SAY_FIRST(0);
        break;
    case 207:
    case 213:
    case 221:
        OUTRO_SAY_NEXT(0);
        break;
    case 205:
    case 209:
    case 211:
    case 215:
    case 217:
        OUTRO_SAY_NEXT(1);
        break;
    case 202:
    case 206:
    case 208:
    case 210:
    case 212:
    case 214:
    case 216:
    case 218:
    case 222:
        OUTRO_WAIT();
        break;
    case 203:
        HistOutro_GuideGoto(0, 1, 0);
        gHistOutro->step++;
        break;
    case 219:
        HistOutro_GuideGoto(0, 1, 1);
        gHistOutro->step++;
        break;
    case 223:
        HistOutro_GuideGoto(0, 0, 1);
        gHistOutro->step = OUTRO_STEP_CLOSE;
        break;
    /* saga 3 */
    case 251:
        OUTRO_SAY_FIRST(0);
        break;
    case 253:
    case 255:
    case 261:
    case 275:
        OUTRO_SAY_NEXT(0);
        break;
    case 259:
    case 263:
    case 265:
    case 267:
    case 269:
    case 271:
        OUTRO_SAY_NEXT(1);
        break;
    case 252:
    case 254:
    case 256:
    case 260:
    case 262:
    case 264:
    case 266:
    case 268:
    case 270:
    case 272:
    case 276:
        OUTRO_WAIT();
        break;
    case 257:
        HistOutro_GuideGoto(0, 1, 0);
        gHistOutro->step++;
        break;
    case 273:
        HistOutro_GuideGoto(0, 1, 1);
        gHistOutro->step++;
        break;
    case 277:
        HistOutro_GuideGoto(0, 0, 1);
        gHistOutro->step = OUTRO_STEP_CLOSE;
        break;
    /* saga 4 */
    case 301:
        OUTRO_SAY_FIRST(0);
        break;
    case 305:
    case 307:
    case 311:
    case 313:
    case 315:
    case 319:
        OUTRO_SAY_NEXT(0);
        break;
    case 303:
    case 309:
    case 317:
        OUTRO_SAY_NEXT(1);
        break;
    case 302:
    case 304:
    case 306:
    case 308:
    case 310:
    case 312:
    case 314:
    case 316:
    case 318:
    case 320:
        OUTRO_WAIT();
        break;
    /* saga 5 */
    case 351:
        OUTRO_SAY_FIRST(0);
        break;
    case 357:
    case 373:
        OUTRO_SAY_NEXT(0);
        break;
    case 355:
        OUTRO_SKIP_LINE();
        break;
    case 359:
    case 361:
    case 363:
    case 365:
    case 367:
    case 369:
        OUTRO_SAY_NEXT(1);
        break;
    case 352:
    case 356:
    case 358:
    case 360:
    case 362:
    case 364:
    case 366:
    case 368:
    case 370:
    case 374:
        OUTRO_WAIT();
        break;
    case 353:
        HistOutro_GuideGoto(0, 1, 0);
        gHistOutro->step++;
        break;
    case 371:
        HistOutro_GuideGoto(0, 1, 1);
        gHistOutro->step++;
        break;
    case 375:
        HistOutro_GuideGoto(0, 0, 1);
        gHistOutro->step = OUTRO_STEP_CLOSE;
        break;
    /* saga 6 */
    case 401:
        OUTRO_SAY_FIRST(0);
        break;
    case 403:
    case 405:
        OUTRO_SAY_NEXT(0);
        break;
    case 409:
    case 411:
    case 413:
    case 415:
    case 417:
    case 419:
    case 421:
        OUTRO_SAY_NEXT(1);
        break;
    case 402:
    case 404:
    case 406:
    case 410:
    case 412:
    case 414:
    case 416:
    case 418:
    case 420:
    case 422:
        OUTRO_WAIT();
        break;
    case 407:
        HistOutro_GuideGoto(0, 0, 1);
        HistOutro_GuideGoto(0, 1, 0);
        gHistOutro->step++;
        break;
    /* saga 7 */
    case 451:
        OUTRO_SAY_FIRST(1);
        break;
    case 455:
    case 461:
        OUTRO_SAY_NEXT(0);
        break;
    case 457:
    case 459:
    case 463:
        OUTRO_SAY_NEXT(1);
        break;
    case 452:
    case 456:
    case 458:
    case 460:
    case 462:
    case 464:
        OUTRO_WAIT();
        break;
    case 453:
        HistOutro_GuideGoto(0, 0, 0);
        gHistOutro->step++;
        break;
    /* all sagas */
    case 106:
    case 158:
    case 166:
    case 204:
    case 220:
    case 258:
    case 274:
    case 354:
    case 372:
    case 408:
    case 454:
        OUTRO_WAIT_MOVIE();
        break;
    case 125:
    case 321:
    case 465:
        HistOutro_GuideGoto(0, 0, 1);
        /* fall through */
    case 171:
    case 423:
        HistOutro_GuideGoto(0, 1, 1);
        gHistOutro->step = OUTRO_STEP_CLOSE;
        break;
    case OUTRO_STEP_TEXT:
        gHistOutro->scroll = 0.0f;
        gHistOutro->scrollMax = gHistOutro->descr[gHistOutro->itemMax].count * 40.0f + 230.0f;
        gHistOutro->narrWait = 0;
        gHistOutro->narrLine = 0;
        gHistOutro->step++;
        break;
    case 502:
        if (gHistOutro->flash[0].trig & 1) {
            gHistOutro->flags |= HISTOUTRO_SCROLLING;
            gHistOutro->step++;
        }
        break;
    case 503:
        if (gHistOutro->descr[gHistOutro->itemMax].delay[gHistOutro->narrLine] <= gHistOutro->narrWait++) {
            Voice_PlayWithSubtitle(NULL, MODEMENU_NARR_VOICE,
                                   gHistOutro->descr[gHistOutro->itemMax].line + gHistOutro->narrLine);
            gHistOutro->narrWait = 0;
            gHistOutro->step++;
        }
        break;
    case 504:
        if (Voice_GetStat(0) == 5) {
            if (gHistOutro->descr[gHistOutro->itemMax].count <= ++gHistOutro->narrLine) {
                gHistOutro->step++;
            } else {
                gHistOutro->step--;
            }
        } else if (gPad[0].gamePressed & 0x1000) {
            gHistOutro->step = OUTRO_STEP_GO;
            Snd_PlaySe(1, 1);
        }
        break;
    case 505:
        gHistOutro->step = 0;
        break;
    case OUTRO_STEP_GO:
        gHistOutro->step = 0;
        gHistOutro->flags |= HISTOUTRO_CHOSEN;
        gHistOutro->flags |= HISTOUTRO_LEAVING;
        gHistOutro->timer = 15;
        break;
    }
}

/* The scene's own frame loop. Returns 1 when the ending movie is due (saga 6 completed for the first time). */
s32 HistOutro_Run(s32 section) {
    s32 result = 1;

    HistOutro_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            HistOutro_Update();
            HistOutro_UpdateStep();
        }
        HistOutro_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gHistOutro->flags & HISTOUTRO_BEGUN) && (gHistOutro->flash[0].flags & MFLASH_PAD)) {
                gHistOutro->flags |= HISTOUTRO_BEGUN;
                gHistOutro->step = 1;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            StreamSe_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gHistOutro->flags & HISTOUTRO_LEAVING) {
            if (--gHistOutro->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gHistOutro->step == 0) {
            HistOutro_Input(&result);
        }
    }
    if (gProgress->subMenu == 6 && !(gSaveData->slot[8].flags & 1)) {
        result = 1;
    } else {
        result = 0;
    }
    HistOutro_Term();
    Dma_ResetBuffers();
    return result;
}

#define HIST_NEXT(m) \
    gProgress->prevMode = gProgress->mode; \
    gProgress->mode = (m)

#define HIST_LOAD_ARC() \
    if (gMenuArc2 == NULL) { \
        gMenuArc2 = File_LoadSync(gProgress->baseFile + 0xC, NULL, 0); \
    }

/* Progress modes 6..10, the story mode, and mode 3, its ending movie. Returns 1 to start the battle that
   ModeMenu_Run set up, 0 to go back to the main menu. */
s32 Hist_Main(void) {
    s32 done = 0;
    s32 result = 1;
    s32 r;
    s32 played; /* set in the modes that follow a battle and never read */

    HIST_LOAD_ARC();
    do {
        switch (gProgress->mode) {
        case 6:
            Bgm_Play(DISC_RESOURCE(0x10B18, 0x838));
            r = HistSel_Run(1);
            Adx_StopAll();
            gProgress->prevMode = gProgress->mode;
            switch (r) {
            case 0:
                gProgress->mode = 4;
                result = 0;
                done = 1;
                break;
            case 1:
                gProgress->mode = 7;
                break;
            case 2:
                gProgress->mode = 3;
                break;
            }
            break;
        case 7:
            if (ModeMenu_Run(2)) {
                Adx_StopAll();
                gProgress->mode = 8;
                done = 1;
            } else {
                Adx_StopAll();
                gProgress->mode = 6;
            }
            break;
        case 8:
            played = 1;
            r = HistResult_Run(3);
            if (r) {
                Adx_StopAll();
                gProgress->prevMode = gProgress->mode;
                switch (r) {
                case 1:
                    gProgress->mode = 10;
                    break;
                case 2:
                    gProgress->mode = 9;
                    break;
                }
            } else {
                Adx_StopAll();
                HIST_NEXT(7);
            }
            break;
        case 9:
            played = 1;
            r = HistOutro_Run(2);
            Adx_StopAll();
            gProgress->prevMode = gProgress->mode;
            switch (r) {
            case 0:
                gProgress->mode = 10;
                break;
            case 1:
                gProgress->mode = 3;
                break;
            }
            break;
        case 10:
            played = 1;
            r = HistSave_Run(4);
            Adx_StopAll();
            gProgress->prevMode = gProgress->mode;
            switch (r) {
            case 0:
                gProgress->mode = 6;
                break;
            case 1:
                gProgress->mode = 7;
                break;
            }
            break;
        case 3:
            OUTRO_FREE(gMenuArc2);
            PadWatch_SetEnabled(0);
            Movie_PlayEnding();
            PadWatch_SetEnabled(1);
            Adx_StopAll();
            gSaveData->slot[8].flags |= 1;
            gSaveData->bgmBits |= 0x80;
            switch (gProgress->prevMode) {
            case 6:
                HIST_NEXT(6);
                break;
            case 9:
                gSaveData->slot[8].flags |= 2;
                HIST_NEXT(10);
                break;
            }
            HIST_LOAD_ARC();
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    OUTRO_FREE(gMenuArc2);
    return result;
}
