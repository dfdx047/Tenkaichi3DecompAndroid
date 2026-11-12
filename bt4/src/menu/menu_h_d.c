#include "common.h"
#include "menu/menu_h.h"
#include "menu/menu_i.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

Train *gTrain = NULL; /* 0x3B4BA8 */

/*
 * Train: the training menu (progress mode 44), 0x359358..0x35D660, one object: the chunk cut at 0x35A558 had
 * put its Init / Update / Input / Run in menu_i.c, merged in below (see the note there). Three menu levels: the
 * top menu (two items), the class menu (three classes) and the lesson list of a class (three rows visible, up to
 * 15 lessons); one bit per lesson in gSaveData->trainClear[class] says the lesson was cleared.
 */

/* Whether the lesson has its "cleared" bit. */
s32 Train_IsCleared(s32 class, s32 lesson) {
    return gSaveTrain->trainClear[class] & (1 << lesson);
}

/* 1 if every lesson of the class is cleared. */
s32 Train_IsClassCleared(s32 class) {
    s32 n;
    s32 i;

    if (class == 0) {
        n = gTrain->count[0];
    } else if (class == 1) {
        n = gTrain->count[1];
    } else if (class == 2) {
        n = gTrain->count[2];
    } else {
        n = 0;
    }
    for (i = 0; i < n; i++) {
        if (!Train_IsCleared(class, i)) {
            return 0;
        }
    }
    return 1;
}

/* 1 if all three classes are cleared. */
s32 Train_IsAllCleared(void) {
    if (Train_IsClassCleared(0) && Train_IsClassCleared(1) && Train_IsClassCleared(2)) {
        return 1;
    }
    return 0;
}

/* Copies the five cursor words (sel[2], saved[3]). */
void Train_CopyCursor(s32 *dst, s32 *src) {
    s32 i;

    for (i = 0; i < 5; i++) {
        dst[i] = src[i];
    }
}

/* Sends the clip named by `fmt` and n + 1 to "fl_on_start" or "fl_off_start". */
static inline void Train_ClipGoto(char *fmt, s32 n, s32 on) {
    MFlashRef ref;
    char name[256];
    MFlash *flash = &gTrain->flash[0];

    sprintf(name, fmt, n + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Lights or dims plate `plate` of the top / class menu. */
void Train_PlateGoto(s32 plate, s32 on) {
    Train_ClipGoto("mc_menu_plate_%d", plate, on);
}

/* Lights or dims row `plate` of the lesson list. */
void Train_Plate2Goto(s32 plate, s32 on) {
    Train_ClipGoto("mc_menu_plate_2_%d", plate, on);
}

/* Lights or dims the plate under the cursor of the current menu level. */
void Train_CursorGoto(s32 on) {
    switch (gTrain->level) {
    case 0:
    case 1:
        Train_PlateGoto(gTrain->sel[gTrain->level], on);
        break;
    case 2:
        Train_Plate2Goto(gTrain->saved[gTrain->sel[1]], on);
        break;
    case 3:
    case 4:
    case 5:
        break;
    }
}

/* Shows the "cleared" icon of the current lesson: the animated one on levels 2 and 5, the still one on 3 and 4. */
void Train_DrawClearIcon(void) {
    MFlashRef ref;
    MFlash *flash = &gTrain->flash[0];

    if (gTrain->level == 3 || gTrain->level == 4) {
        if (Train_IsCleared(gTrain->sel[1], gTrain->row + gTrain->top)) {
            Flash_FindLabel(flash, NULL, "mc_icon_training_clear_in", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, "mc_menu_plate_2_5", "mc_icon_training_clear", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_FindLabel(flash, NULL, "mc_icon_training_clear_in", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, "mc_menu_plate_2_5", "mc_icon_training_clear", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    } else if (gTrain->level == 5 || gTrain->level == 2) {
        Flash_FindLabel(flash, NULL, "mc_icon_training_clear_in", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_FindLabel(flash, "mc_menu_plate_2_5", "mc_icon_training_clear", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
}

/* Gives the clip `name` of `parent` a texture rectangle. */
static inline void Train_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Sets the text strips of the six menu plates (texture 0 on the top menu, 2 on the class menu). */
void Train_DrawPlates(void) {
    MFlashRef ref;
    char name[256];
    MFlashUv uv;
    MFlash *flash = &gTrain->flash[0];
    s32 tex;
    s32 i;

    switch (gTrain->level) {
    case 0:
        tex = 0;
        break;
    case 1:
        tex = gTrain->level * 2; /* a plain `tex = 2` compiles the test to `xori` instead of `li / xor` */
        break;
    default:
        tex = 0;
        break;
    }
    for (i = 0; i < 6; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetTex(flash, &ref, tex);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetTex(flash, &ref, tex);
        uv.x0 = 0;
        uv.x1 = 0x200;
        uv.y0 = i * 0x20;
        uv.y1 = i * 0x20 + 0x20;
        Train_SetUv(flash, name, "mc_menu_text_on", &uv);
        Train_SetUv(flash, name, "mc_menu_text_off", &uv);
        Flash_FindLabel(flash, name, "mc_icon_wii_style", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
}

/* Attaches the lesson names to the five list plates and shows the "cleared" icon of each. */
void Train_DrawList(void) {
    MFlashRef ref;
    char name[256];
    MFlash *flash = &gTrain->flash[0];
    s32 i;
    s32 lesson;
    s32 line;

    for (i = 0; i < 5; i++) {
        sprintf(name, "mc_menu_plate_2_%d", i + 1);
        if (i == 3) {
            lesson = gTrain->extra;
        } else if (i == 4) {
            lesson = gTrain->extra2;
        } else {
            lesson = gTrain->top + i;
        }
        line = gTrain->nameLine[gTrain->sel[1]][lesson];
        Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, line, &gTrain->box[i]);
        Flash_FindLabel(flash, name, "mc_icon_training_clear", &ref);
        if (i != 4) {
            if (Train_IsCleared(gTrain->sel[1], lesson)) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
    }
}

/* Clip callback: scissor to the lesson list. */
void Train_ScissorOn(void) {
    Sprite_SetScissor(0, 0x200, 0x4F, 0x12F);
}

/* Clip callback: scissor back to the full screen. */
void Train_ScissorOff(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Reads the position of the two "mc_ss" clips and makes them draw inside the list scissor. */
void Train_SetClip(void) {
    MFlashRef ref;

    Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_speace", &ref);
    Flash_ClipGetPos(&gTrain->flash[0], &ref, &gTrain->clipX, &gTrain->clipY);
    Flash_ClipSetCallbackA(&gTrain->flash[0], &ref, Train_ScissorOn, NULL);
    Flash_ClipSetCallbackB(&gTrain->flash[0], &ref, Train_ScissorOff, NULL);
    Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_dammy", &ref);
    Flash_ClipGetPos(&gTrain->flash[0], &ref, &gTrain->clipX, &gTrain->clipY);
    Flash_ClipSetCallbackA(&gTrain->flash[0], &ref, Train_ScissorOn, NULL);
    Flash_ClipSetCallbackB(&gTrain->flash[0], &ref, Train_ScissorOff, NULL);
}

/* Hides the up (or down) arrow of the lesson list. */
void Train_HideArrow(s32 up) {
    MFlashRef ref;
    char parent[256];
    char name[256];

    if (up) {
        strcpy(parent, "mc_yajirusi_up");
        strcpy(name, "mc_yajirusi_icon_up");
    } else {
        strcpy(parent, "mc_yajirusi_down");
        strcpy(name, "mc_yajirusi_icon_down");
    }
    Flash_FindLabel(&gTrain->flash[0], parent, name, &ref);
    Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
}

/* Sets the arrow icons' texture rectangles and hides the arrows that lead nowhere. */
void Train_DrawArrows(void) {
    MFlashUv uv;

    uv.x0 = 0;
    uv.x1 = 0x20;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    Train_SetUv(&gTrain->flash[0], "mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    Train_SetUv(&gTrain->flash[0], "mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0;
    uv.y1 = 0x20;
    Train_SetUv(&gTrain->flash[0], "mc_yajirusi_right", "mc_yajirusi_icon_right", &uv);
    if (gTrain->top == 0) {
        Train_HideArrow(1);
    } else if (gTrain->top == gTrain->count[gTrain->sel[1]] - TRAIN_ROWS) {
        Train_HideArrow(0);
    }
    {
        MFlashRef ref;

        Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_next_2", &ref);
        if (gTrain->page >= gTrain->pageNum - 1) {
            Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
        } else {
            Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 1);
        }
        Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_next", &ref);
        Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
    }
}

/* Clamps *v to lo..hi; 1 if it had to. */
static inline s32 Train_Clamp(s32 *v, s32 lo, s32 hi) {
    if (*v < lo) {
        *v = lo;
        return 1;
    }
    if (*v > hi) {
        *v = hi;
        return 1;
    }
    return 0;
}

/* 1 if v is not in lo..hi. */
static inline s32 Train_IsOutside(s32 *v, s32 lo, s32 hi) {
    if (*v < lo) {
        return 1;
    }
    if (*v > hi) {
        return 1;
    }
    return 0;
}

/* Wraps *v around lo..hi. */
static inline void Train_Wrap(s32 *v, s32 lo, s32 hi) {
    if (*v < lo) {
        *v = hi;
    } else if (*v > hi) {
        *v = lo;
    }
}

/* Says the current lesson's line and remembers it. */
#define TRAIN_SAY(line) \
    if (gTrain->level == 0) { \
        Voice_PlayWithSubtitle(gTrain->subtitlesA, TRAIN_VOICE_BASE_A, line); \
    } else { \
        Voice_PlayWithSubtitle(gTrain->subtitlesB, TRAIN_VOICE_BASE_B, line); \
    } \
    gTrain->voiceLine = line

/* Moves the lesson cursor by dir (-1 / 1), scrolling the list at its edges; 0 if it is at the end. */
s32 Train_MoveRow(s32 dir) {
    char name[256];
    s32 line;
    s32 lesson;
    s32 hi;

    hi = gTrain->count[gTrain->sel[1]] - 1;
    lesson = gTrain->row + gTrain->top + dir;
    if (Train_IsOutside(&lesson, 0, hi)) {
        return 0;
    }
    Train_Plate2Goto(gTrain->row, 0);
    gTrain->row += dir;
    if (Train_Clamp(&gTrain->row, 0, TRAIN_ROWS - 1)) {
        gTrain->top += dir;
        if (!Train_Clamp(&gTrain->top, 0, gTrain->count[gTrain->sel[1]] - TRAIN_ROWS)) {
            switch (dir) {
            case 1:
                Flash_GotoLabel(&gTrain->flash[0], "fl_class_menu_up", 1);
                gTrain->extra = gTrain->top - 1;
                break;
            case -1:
                Flash_GotoLabel(&gTrain->flash[0], "fl_class_menu_down", 1);
                gTrain->extra = gTrain->top + 3;
                break;
            }
        }
    }
    sprintf(name, "mc_menu_plate_2_%d", gTrain->row + 1);
    Train_Plate2Goto(gTrain->row, 1);
    Snd_PlaySe(1, 0);
    line = gTrain->voiceTbl[gTrain->sel[1]][gTrain->row + gTrain->top];
    TRAIN_SAY(line);
    return 1;
}

/* Moves the lesson cursor to the next lesson without the scroll animation or the sound. */
void Train_NextRow(void) {
    char name[256];
    s32 line;

    Train_Plate2Goto(gTrain->row, 0);
    gTrain->row++;
    if (Train_Clamp(&gTrain->row, 0, TRAIN_ROWS - 1)) {
        gTrain->top++;
    }
    if (!Train_Clamp(&gTrain->top, 0, gTrain->count[gTrain->sel[1]] - TRAIN_ROWS)) {
        gTrain->extra = gTrain->top - 1;
    }
    sprintf(name, "mc_menu_plate_2_%d", gTrain->row + 1);
    Train_Plate2Goto(gTrain->row, 1);
    line = gTrain->voiceTbl[gTrain->sel[1]][gTrain->row + gTrain->top];
    TRAIN_SAY(line);
}

/* Up (dir 0) or down (dir 1) on the current menu level. */
void Train_MoveCursor(s32 dir) {
    s32 d = 0;
    s32 line;

    switch (dir) {
    case 0:
        d = -1;
        break;
    case 1:
        d = 1;
        break;
    }
    switch (gTrain->level) {
    case 0:
        Train_CursorGoto(0);
        gTrain->sel[gTrain->level] += d;
        Train_Wrap(&gTrain->sel[gTrain->level], 0, 1);
        Train_CursorGoto(1);
        Snd_PlaySe(1, 0);
        line = gTrain->sel[0] + 2;
        TRAIN_SAY(line);
        break;
    case 1:
        Train_CursorGoto(0);
        gTrain->sel[gTrain->level] += d;
        Train_Wrap(&gTrain->sel[gTrain->level], 0, 2);
        Train_CursorGoto(1);
        Snd_PlaySe(1, 0);
        line = gTrain->sel[1] + 1;
        TRAIN_SAY(line);
        break;
    case 2:
        Train_MoveRow(d);
        break;
    case 3:
    case 4:
    case 5:
        break;
    }
}

/*
 * 0x35A558..0x35D660: the rest of the training menu ("Ultimate Training", mode 44); this part was menu_i.c
 * until the merge. The object's .data is the pointer gTrain (0x3B4BA8), its .rodata runs from 0x3B4BB0 to
 * 0x3B5908, and strings first used in the first half are shared ("mc_ss_speace" of Train_Draw is at 0x3B4CB8).
 * Train_Update and Train_Input match only in this one file (five branches next to calls of the first half's
 * functions come out branch-likely when they are external).
 *
 * Three menu levels (top menu, class, lesson), then the guide's introduction, the explanation pages (a text
 * line and a picture per page) and, by the lesson's flags, a tutorial or a practice battle. The guides are
 * Great Saiyaman (top menu) and Videl (the rest). Train_BuildLists is INCLUDE_ASM; everything else matches.
 */

/* Says a line with its subtitle and remembers it (the top menu and the rest have different voice banks). */
static inline void Train_Say(s32 line) {
    if (gTrain->level == 0) {
        Voice_PlayWithSubtitle(gTrain->subtitlesA, TRAIN_VOICE_BASE_A, line);
    } else {
        Voice_PlayWithSubtitle(gTrain->subtitlesB, TRAIN_VOICE_BASE_B, line);
    }
    gTrain->voiceLine = line;
}

#define TRAIN_IS_TUTORIAL(flags) ((u8)((flags) & TRAIN_LESSON_TUTORIAL))
#define TRAIN_CUR_LESSON (gTrain->row + gTrain->top)

/* Puts the list where the class's saved cursor is: the saved lesson on the first row, or the last three. */
void Train_RestoreScroll(void) {
    s32 *scroll = &gTrain->top;
    s32 top;

    scroll[0] = gTrain->saved[gTrain->sel[1]];
    top = gTrain->count[gTrain->sel[1]] - TRAIN_ROWS;
    if (top < gTrain->saved[gTrain->sel[1]]) {
        scroll[0] = top;
        scroll[1] = TRAIN_ROWS - (gTrain->count[gTrain->sel[1]] - gTrain->saved[gTrain->sel[1]]);
    } else {
        scroll[1] = 0;
    }
}

/* Remembers the lesson under the cursor for the current class. */
void Train_SaveCursor(void) {
    gTrain->saved[gTrain->sel[1]] = gTrain->row + gTrain->top;
}

/*
 * Train_BuildLists (0x35A610) is the one function of this object that does not match: 11 of 118 instructions,
 * all in the inlined skip-list test. The original starts that loop with
 *     move t0,zero / sltu v0,t0,t2 / beqz v0 / move t1,t0      (found = 0; if (found < num); i = found)
 * and the attempt with
 *     move t1,zero / beqz t0 / move t2,zero                    (i = 0; if (num != 0); found = 0)
 * i.e. here the compiler folds the entry test `0 < skipNum` into `skipNum != 0` (in combine) and loads both
 * zeros as constants; the rest differs only by the three registers that follow from it. Behaviour is the same.
 * Tried without success: every loop shape (for / while / while (1) / goto), found and i at function or block
 * scope and in either order, i initialised from found, u8 / 64-bit flags, a result passed by pointer, a walking
 * pointer, a hand-inlined test.
 *
 * Cleanup notes (RTL dumps, build/scratch_cleanup2_I/rtl/hd.*): up to the second CSE pass the attempt has what
 * the original must have had, `found = 0; i = 0; n = tbl->skipNum; t = found <u n` (CSE itself writes the
 * compare with `found`, the older register that holds 0). The fold happens in combine, which merges
 * `found = 0` into the compare because the compare is the FIRST use of `found` behind it in the same basic
 * block. The original therefore had either another use of `found` in front of the compare or a block boundary
 * between the two, and its `move t1,t0` is a real copy `i = found` that the first CSE pass could not turn into
 * `i = 0`, i.e. `found = 0` was not visible on the path that CSE followed to it. Also tried here: the count
 * read into a local in front of the two initialisations, `found` as a parameter of a nested inline, `do { }
 * while (0)` around the initialisation or the loop, `if (found < n)` written out with a do / while: all give
 * the same 11 (10 for the written-out test).
 */
#if 0
/* The lesson records of a class. */
static inline TrainLesson *Train_ClassLessons(TrainTbl *tbl, s32 class) {
    TrainLesson *p = NULL;

    switch (class) {
    case 0:
        p = tbl->lessons;
        break;
    case 1:
        p = tbl->lessons + TRAIN_CLASS0_NUM;
        break;
    case 2:
        p = tbl->lessons + TRAIN_CLASS0_NUM + TRAIN_CLASS1_NUM;
        break;
    }
    return p;
}

/* Whether a page id is in the skip list. */
static inline s32 Train_IsSkipped(TrainTbl *tbl, u32 page) {
    s32 found = 0;
    u32 i;

    for (i = 0; i < tbl->skipNum; i++) {
        if (tbl->skip[i] == page) {
            found = 1;
        }
    }
    return found;
}

/* Builds, per class, the list of lessons that have at least one page left after the skip list. */
void Train_BuildLists(TrainTbl *tbl, TrainLists *out) {
    s32 class;

    for (class = 0; class < TRAIN_CLASS_NUM; class++) {
        TrainLesson *p = Train_ClassLessons(tbl, class);
        s32 count = 0;

        while (1) {
            s32 n = 0;
            u32 page;

            for (page = p->firstPage; page < p->pageNum + p->firstPage; page++) {
                if (!Train_IsSkipped(tbl, page)) {
                    out->pages[class][count][n] = page;
                    n++;
                }
            }
            if (n != 0) {
                out->pageCount[class][count] = n;
                out->nameLine[class][count] = p->id;
                out->voiceTbl[class][count] = p->voice;
                count++;
            }
            if (p->flags & TRAIN_LESSON_LAST) {
                break;
            }
            p++;
        }
        out->count[class] = count;
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/menu/menu_h_d", Train_BuildLists);

/* Picks the title of the current class. */
void Train_DrawTitle(void) {
    MFlashUv uv;
    MFlash *flash = &gTrain->flash[0];

    uv.y0 = gTrain->sel[1] * 0x40;
    uv.y1 = uv.y0 + 0x40;
    uv.x0 = 0;
    uv.x1 = 0x100;
    Train_SetUv(flash, NULL, "mc_title_class", &uv);
}

/* Draws the icon window with the icon of the current level. */
void Train_DrawIconWin(void) {
    if (gTrain->level == TRAIN_LV_CLASS) {
        IconWin_SetIcon(2);
    } else {
        IconWin_SetIcon(0);
    }
    IconWin_Draw();
}

/* Scrolls the clouds and draws the background picture. */
void Train_DrawBg(void) {
    MFlashRef ref;
    MFlashUv uv;

    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x200;
    uv.y1 = 0x80;
    Flash_FindLabel(&gTrain->flash[0], NULL, "mc_bg_cloud", &ref);
    FlashAnim_Scroll(&gTrain->flash[0], &ref, &uv, &gTrain->cloud, NULL, 0.047407407f, 0.0f);
#ifdef PORT
    Port_MenuFill(1); /* widescreen menus: the backdrop fills the width (port/src/gs_marker.c) */
    Sprite_DrawPicture(gTrain->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gTrain->bg, 0, 0, 0x80);
#endif
}

/* The two guides: pose, and for the one who is talking (Videl below the top menu) eyes and mouth. */
void Train_DrawGuides(void) {
    MFlashRef ref;
    char guide[2][0x100] = {"mc_guide_saiyaman", "mc_guide_bidel"};
    char eye[2][0x100] = {"", "mc_guide_%c_bidel_eye"};
    char mouth[2][0x100] = {"mc_guide_%c_saiyaman_mouth", "mc_guide_%c_bidel_mouth"};
    char name[0x100];
    s32 who;
    s32 i;

    who = gTrain->level != TRAIN_LV_TOP;
    for (i = 0; i < 2; i++) {
        sprintf(name, eye[i], gTrain->pose[i] == 0 ? 'b' : 'a');
        Flash_FindLabel(&gTrain->flash[0], NULL, name, &ref);
        Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
        sprintf(name, eye[i], gTrain->pose[i] + 'a');
        if (who == i) {
            Flash_FindLabel(&gTrain->flash[0], NULL, name, &ref);
            FlashAnim_Blink(&gTrain->flash[0], &ref, &gTrain->guide.blink[i], 0);
            sprintf(name, mouth[i], gTrain->pose[i] == 0 ? 'b' : 'a');
            Flash_FindLabel(&gTrain->flash[0], NULL, name, &ref);
            Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
            sprintf(name, mouth[i], gTrain->pose[i] + 'a');
            Flash_FindLabel(&gTrain->flash[0], NULL, name, &ref);
            if (Voice_GetStat(0) == 3) {
                FlashAnim_Talk(&gTrain->flash[0], &ref, &gTrain->guide.talk[i], 0);
            } else {
                FlashAnim_ShowNext2(&gTrain->flash[0], &ref, 0);
            }
        }
        Flash_FindLabel(&gTrain->flash[0], NULL, guide[i], &ref);
        Flash_ClipSetTex(&gTrain->flash[0], &ref, gTrain->pose[i]);
    }
}

/*
 * A section of the screen's pack. `host` is the file the section was built from: the development build could
 * read it from the host PC. Nothing uses it here, but the strings are still in the object, in this order.
 */
static inline u8 *Train_Section(s32 n, const char *host) {
    return MPACK_AT(gTrain->res, n);
}

#define TR_HOST "host:data/test/ut/"

#define TR_RES(n, host) \
    res = (MTexRes *)Train_Section(n, host); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 6) and builds its movie, windows and lists. */
void Train_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gTrain = Heap_Alloc(0x1BE0, 0x20, 0, 2);
    memset(gTrain, 0, 0x1BE0);
    gTrain->pack = (u32 *)MPACK_AT(gMenuArc6, section);
    gTrain->res = Sprite_Unpack(gTrain->pack, NULL, NULL);
    TR_RES(2, TR_HOST "ut_bg_PS2_.dbt");
    gTrain->bg = res;
    TR_RES(7, TR_HOST "ut_select_tex_PS2_.dbt");
    gTrain->tex[0] = MTEX(res, 0);
    gTrain->tex[5] = MTEX(res, 1);
    gTrain->tex[6] = MTEX(res, 2);
    gTrain->tex[7] = MTEX(res, 3);
    gTrain->tex[18] = MTEX(res, 4);
    gTrain->tex[19] = MTEX(res, 5);
    gTrain->tex[21] = MTEX(res, 6);
    gTrain->tex[22] = MTEX(res, 7);
    gTrain->tex[23] = MTEX(res, 8);
    gTrain->tex[24] = MTEX(res, 9);
    gTrain->tex[27] = MTEX(res, 10);
    gTrain->tex[28] = MTEX(res, 11);
    gTrain->tex[29] = MTEX(res, 12);
    gTrain->tex[30] = MTEX(res, 13);
    gTrain->tex[32] = MTEX(res, 14);
    gTrain->tex[33] = MTEX(res, 15);
    TR_RES(6, TR_HOST "UT/ut_select_tex_chara_PS2_.dbt");
    gTrain->tex[1] = MTEX(res, 0);
    gTrain->tex[2] = MTEX(res, 1);
    gTrain->tex[3] = MTEX(res, 2);
    gTrain->tex[4] = MTEX(res, 3);
    TR_RES(9, TR_HOST "ut_select_text_JP_PS2_.dbt");
    gTrain->tex[25] = MTEX(res, 0);
    gTrain->tex[26] = MTEX(res, 4);
    TR_RES(8, TR_HOST "ut_select_text_class_JP_PS2_.dbt");
    gTrain->tex[31] = MTEX(res, 0);
    TR_RES(4, TR_HOST "ut_guide_saiyaman_PS2_.dbt");
    gTrain->tex[10] = MTEX(res, 0);
    gTrain->tex[12] = MTEX(res, 2);
    gTrain->tex[11] = MTEX(res, 4);
    TR_RES(3, TR_HOST "ut_guide_bidel_PS2_.dbt");
    gTrain->tex[13] = MTEX(res, 0);
    gTrain->tex[16] = MTEX(res, 2);
    gTrain->tex[14] = MTEX(res, 4);
    gTrain->tex[17] = MTEX(res, 6);
    gTrain->tex[15] = MTEX(res, 8);
    gTrain->tex[20] = NULL;
    Flash_Create(&gTrain->flash[0], Train_Section(1, TR_HOST "UltimateTraining_top_PS2_.fod"), gTrain->tex);
    Flash_Play(&gTrain->flash[0], 1);
    TR_RES(5, TR_HOST "ut_title_JP_PS2_.dbt");
    IconWin_Init(Train_Section(10, TR_HOST "if_title_line_PS2_.pak"), res);
    gTrain->msgText[0] = Train_Section(14, TR_HOST "ut_msg_JP_PS2_.pak");
    gTrain->msgText[1] = Train_Section(15, TR_HOST "bt_msg_JP_PS2_.pak");
    MsgWin_Init(Train_Section(11, TR_HOST "if_msg_window_PS2_.pak"), gTrain->msgText[0], 0, (s32)gTrain->unk3CC);
    gTrain->subtitlesA = Train_Section(12, TR_HOST "utraining_lips_PS2_.pak");
    gTrain->subtitlesB = Train_Section(13, TR_HOST "btutorial_lips_PS2_.pak");
    gTrain->text = Train_Section(16, TR_HOST "font_Training_JP_PS2_.pak");
    for (i = 0; i < 5; i++) {
        TextBox_Init(&gTrain->box[i], gTrain->text, 2);
        TextBox_SetLineOffsets(&gTrain->box[i], 9, 0, 0, 0, 0);
        TextBox_SetMaxSize(&gTrain->box[i], 0x100, 0x40);
        TextBox_SetUnkC(&gTrain->box[i], 0xA, 4);
    }
    gTrain->pageText = Train_Section(17, TR_HOST "font_BattleTutorial_JP_PS2_.dat");
    gTrain->tbl.lessons = (TrainLesson *)Train_Section(22, TR_HOST "ut_normal_index_data_PS2_.dat");
    gTrain->tbl.skip = (s32 *)(Train_Section(24, TR_HOST "tu_noraml_botu_PS2_.dat") + 0x10);
    gTrain->tbl.skipNum = *(u32 *)Train_Section(24, TR_HOST "tu_noraml_botu_PS2_.dat");
    gTrain->tbl.noImage = (s32 *)(Train_Section(26, TR_HOST "tu_noraml_notexture_PS2_.dat") + 0x10);
    gTrain->tbl.noImageNum = *(u32 *)Train_Section(26, TR_HOST "tu_noraml_notexture_PS2_.dat");
    gTrain->imageFile = Heap_Alloc(0xE000, 0x40, 0, 2);
    gTrain->imageRes = Heap_Alloc(0x10800, 0x20, 0, 2);
    gTrain->unk430 = Heap_Alloc(0x10800, 0x20, 0, 2);
    for (i = 0; i < 2; i++) {
        gTrain->guide.blink[i] = Rand_Range(0x20);
    }
    gTrain->unk46C = 0x1E;
    gTrain->voiceLine = -1;
    Train_BuildLists(&gTrain->tbl, (TrainLists *)gTrain->pages);
}

/* Frees the screen. */
void Train_Term(void) {
    s32 i;

    for (i = 0; i < TRAIN_FLASH_NUM; i++) {
        Flash_Destroy(&gTrain->flash[i]);
    }
    if (gTrain->imageFile != NULL) {
        Heap_Free(gTrain->imageFile);
        gTrain->imageFile = NULL;
    }
    if (gTrain->imageRes != NULL) {
        Heap_Free(gTrain->imageRes);
        gTrain->imageRes = NULL;
    }
    if (gTrain->unk430 != NULL) {
        Heap_Free(gTrain->unk430);
        gTrain->unk430 = NULL;
    }
    MsgWin_Term();
    IconWin_Term();
    if (gTrain->res != NULL) {
        Heap_Free(gTrain->res);
        gTrain->res = NULL;
    }
    if (gTrain != NULL) {
        Heap_Free(gTrain);
        gTrain = NULL;
    }
}

/*
 * Advances the movie. The first call opens the menu where the session flags say it was left (after a lesson's
 * battle: on the lesson, with the "cleared" sequence the first time); once the movie accepts input, the
 * matching line is said.
 */
void Train_Update(void) {
    s32 i;

    for (i = 0; i < TRAIN_FLASH_NUM; i++) {
        Flash_Advance(&gTrain->flash[i]);
    }
    if (!(gTrain->flags & TRAIN_STARTED)) {
        gTrain->flags |= TRAIN_STARTED;
        Train_Plate2Goto(4, 1);
        if (TRAIN_PROG->flags & MPTRAIN_FIRST_CLEAR) {
            Train_CopyCursor(gTrain->sel, TRAIN_PROG->cursor);
            Train_RestoreScroll();
            MsgWin_SetText(gTrain->msgText[1]);
            Flash_GotoLabel(&gTrain->flash[0], "fl_clear_in", 1);
            gTrain->level = TRAIN_LV_CLEAR;
            gTrain->extra2 = TRAIN_CUR_LESSON;
            MsgWin_Open();
        } else if (TRAIN_PROG->flags & MPTRAIN_AGAIN) {
            Train_CopyCursor(gTrain->sel, TRAIN_PROG->cursor);
            Train_RestoreScroll();
            MsgWin_SetText(gTrain->msgText[1]);
            Flash_GotoLabel(&gTrain->flash[0], "fl_clear_in_2", 1);
            gTrain->level = TRAIN_LV_LESSON;
            Train_RestoreScroll();
            Train_NextRow();
            MsgWin_Open();
        } else {
            MsgWin_Open();
            IconWin_Open();
            Flash_GotoLabel(&gTrain->flash[0], "fl_in_1", 1);
            MsgWin_SetText(gTrain->msgText[0]);
            gTrain->level = TRAIN_LV_TOP;
        }
    } else if (gTrain->flash[0].flags & MFLASH_PAD) {
        if (!(gTrain->flags & TRAIN_GREETED)) {
            gTrain->flags |= TRAIN_GREETED;
            if (TRAIN_PROG->flags & MPTRAIN_SELECT) {
                Train_Say(gTrain->sel[0] + 2);
                Train_CursorGoto(1);
            } else if (TRAIN_PROG->flags & MPTRAIN_AGAIN) {
                Train_Say(gTrain->voiceTbl[gTrain->sel[1]][TRAIN_CUR_LESSON]);
            } else if (TRAIN_PROG->flags & MPTRAIN_FIRST_CLEAR) {
                Train_Say(0x39);
            } else {
                Train_Say(0);
                Train_CursorGoto(1);
            }
            TRAIN_PROG->flags = 0;
        }
    }
}

/* Draws the screen. */
void Train_Draw(void) {
#ifdef PORT
    Port_MenuPage(18); /* widescreen menus: this screen's page */
#endif
    MFlashRef ref;
    s32 i = 0;

    Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_speace", &ref); /* sic: the movie's label is misspelt */
    Flash_ClipSetAlpha(&gTrain->flash[0], &ref, gTrain->imageAlpha);
    Train_DrawPlates();
    Train_DrawArrows();
    Train_DrawList();
    Train_DrawClearIcon();
    if (!(gTrain->flags & TRAIN_LEAVING)) {
        Train_SetClip();
    }
    Train_DrawGuides();
    Train_DrawBg();
    Train_DrawTitle();
    for (; i < TRAIN_FLASH_NUM; i++) {
        Flash_Draw(&gTrain->flash[i]);
    }
    Train_DrawIconWin();
    MsgWin_Draw(0, 0, gTrain->voiceLine);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/*
 * Once the menu is being left: starts the fade out (after the farewell line, or at a button, when going back
 * to the main menu) and fades the sound with it. Returns 1 when the fade is over.
 */
s32 Train_CheckLeave(void) {
    if (gTrain->flags & TRAIN_LEAVING) {
        if (!(gTrain->flags & TRAIN_FADING)) {
            if (gTrain->voiceLine != 6) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                gTrain->flags |= TRAIN_FADING;
            } else if (Voice_GetStat(0) == MVOICE_IDLE || (gPad[0].gamePressed & PADG_CROSS) ||
                       (gPad[0].gamePressed & PADG_TRIANGLE)) {
                if (gPad[0].gamePressed & PADG_CROSS) {
                    Snd_PlaySe(1, 1);
                } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
                    Snd_PlaySe(1, 2);
                }
                ColorFade_StartOut(0, 0, 0, 0x14);
                gTrain->flags |= TRAIN_FADING;
            }
        }
        if (gTrain->flags & TRAIN_FADING) {
            if (ColorFade_IsFadingOut()) {
                Voice_FadeOutStep(0);
                if (gTrain->result == 0) {
                    Bgm_FadeOutStep();
                }
            } else {
                return 1;
            }
        }
    }
    return 0;
}

/* The record of the lesson under the cursor (NULL if its id is not in the class). */
static inline TrainLesson *Train_FindLesson(void) {
    TrainLesson *p = NULL;
    s32 num = 0;
    s32 i;

    switch (gTrain->sel[1]) {
    case 0:
        p = gTrain->tbl.lessons;
        num = TRAIN_CLASS0_NUM;
        break;
    case 1:
        num = TRAIN_CLASS1_NUM;
        p = gTrain->tbl.lessons + TRAIN_CLASS0_NUM;
        break;
    case 2:
        num = TRAIN_CLASS2_NUM;
        p = gTrain->tbl.lessons + TRAIN_CLASS0_NUM + TRAIN_CLASS1_NUM;
        break;
    }
    for (i = 0; i < num; i++) {
        if (p[i].id == gTrain->nameLine[gTrain->sel[1]][TRAIN_CUR_LESSON]) {
            return &p[i];
        }
    }
    return NULL;
}

/* The position of the lesson under the cursor in its class (the class size if it is not found). */
static inline s32 Train_FindLessonIndex(void) {
    TrainLesson *p = NULL;
    s32 num = 0;
    s32 i;

    switch (gTrain->sel[1]) {
    case 0:
        p = gTrain->tbl.lessons;
        num = TRAIN_CLASS0_NUM;
        break;
    case 1:
        num = TRAIN_CLASS1_NUM;
        p = gTrain->tbl.lessons + TRAIN_CLASS0_NUM;
        break;
    case 2:
        num = TRAIN_CLASS2_NUM;
        p = gTrain->tbl.lessons + TRAIN_CLASS0_NUM + TRAIN_CLASS1_NUM;
        break;
    }
    for (i = 0; i < num; i++) {
        if (p[i].id == gTrain->nameLine[gTrain->sel[1]][TRAIN_CUR_LESSON]) {
            break;
        }
    }
    return i;
}

/* Whether a page keeps the picture of the page before it. */
static inline s32 Train_PageHasImage(void) {
    u32 i;

    for (i = 0; i < gTrain->tbl.noImageNum; i++) {
        if (gTrain->voiceLine == gTrain->tbl.noImage[i]) {
            return 0;
        }
    }
    return 1;
}

#define TRAIN_VOICE_DONE(n) \
    (((gPad[0].gamePressed & PADG_CROSS) || Voice_GetStat(0) == MVOICE_IDLE) && gTrain->voiceLine == (n))

/* Reads pad 0 for the current menu level; also the guide's idle lines after 3600 frames without input. */
void Train_Input(void) {
    s32 done;

    if (!(gTrain->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gTrain->flags & TRAIN_LEAVING) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    switch (gTrain->level) {
    case TRAIN_LV_TOP:
        if (Voice_GetStat(0) == MVOICE_IDLE && gTrain->voiceLine == 0) {
            Train_Say(1);
        } else if (Voice_GetStat(0) == MVOICE_IDLE && gTrain->voiceLine == 1) {
            Train_Say(gTrain->sel[0] + 2);
        }
        if (gPad[0].gameRepeat & PADG_UP) {
            Train_MoveCursor(0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            Train_MoveCursor(1);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            switch (gTrain->sel[gTrain->level]) {
            case 0:
                gTrain->result = 0x2D;
                gTrain->flags |= TRAIN_LEAVING;
                Train_CopyCursor(TRAIN_PROG->cursor, gTrain->sel);
                Flash_GotoLabel(&gTrain->flash[0], "fl_cancel_1", 1);
                TRAIN_PROG->flags |= MPTRAIN_SELECT;
                break;
            case 1:
                Train_CursorGoto(0);
                gTrain->level = TRAIN_LV_CLASS;
                Flash_GotoLabel(&gTrain->flash[0], "fl_class_in", 1);
                Train_CursorGoto(1);
                Train_Say(0);
                MsgWin_SetText(gTrain->msgText[1]);
                break;
            }
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Flash_GotoLabel(&gTrain->flash[0], "fl_cancel_1", 1);
            gTrain->flags |= TRAIN_LEAVING;
            gTrain->result = 0;
            Snd_PlaySe(1, 2);
            Train_Say(6);
        }
        break;
    case TRAIN_LV_CLASS:
        if (Voice_GetStat(0) == MVOICE_IDLE && gTrain->voiceLine == 0) {
            Train_Say(gTrain->sel[1] + 1);
        }
        if (gPad[0].gameRepeat & PADG_UP) {
            Train_MoveCursor(0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            Train_MoveCursor(1);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            Train_CursorGoto(0);
            Train_RestoreScroll();
            gTrain->level = TRAIN_LV_LESSON;
            Flash_GotoLabel(&gTrain->flash[0], "fl_class_menu_in", 1);
            IconWin_Close();
            Train_Plate2Goto(gTrain->row, 1);
            Train_Say(9);
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Train_CursorGoto(0);
            gTrain->level = TRAIN_LV_TOP;
            Flash_GotoLabel(&gTrain->flash[0], "fl_class_cancel", 1);
            Train_CursorGoto(1);
            Train_Say(gTrain->sel[0] + 2);
            MsgWin_SetText(gTrain->msgText[0]);
            Snd_PlaySe(1, 2);
        }
        break;
    case TRAIN_LV_LESSON:
        if (Voice_GetStat(0) == MVOICE_IDLE && gTrain->voiceLine == 9) {
            Train_Say(gTrain->voiceTbl[gTrain->sel[1]][TRAIN_CUR_LESSON]);
        }
        if (gPad[0].gameRepeat & PADG_UP) {
            Train_MoveCursor(0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            Train_MoveCursor(1);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            Train_Plate2Goto(gTrain->row, 0);
            gTrain->level = TRAIN_LV_INTRO;
            Flash_GotoLabel(&gTrain->flash[0], "fl_class_menu_2", 1);
            Snd_PlaySe(1, 1);
            Train_Say(Rand_Range(4) + 0x35);
            gTrain->pose[1] = 1;
            gTrain->extra2 = TRAIN_CUR_LESSON;
        }
        if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Train_Plate2Goto(gTrain->row, 0);
            gTrain->level = TRAIN_LV_CLASS;
            Flash_GotoLabel(&gTrain->flash[0], "fl_class_menu_out", 1);
            Train_CursorGoto(1);
            Train_SaveCursor();
            IconWin_Open();
            Snd_PlaySe(1, 2);
            Train_Say(gTrain->sel[1] + 1);
        }
        break;
    case TRAIN_LV_INTRO:
        if (TRAIN_VOICE_DONE(0x35) || TRAIN_VOICE_DONE(0x36) || TRAIN_VOICE_DONE(0x37) || TRAIN_VOICE_DONE(0x38)) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
            }
            gTrain->level = TRAIN_LV_PAGES;
            Flash_GotoLabel(&gTrain->flash[0], "fl_ss_view_in", 1);
            gTrain->page = 0;
            MsgWin_SetBoxParam(0, 5);
            FontIcon_SetPadType(0);
            MsgWin_SetText(gTrain->pageText);
            gTrain->voiceLine = gTrain->pages[gTrain->sel[1]][TRAIN_CUR_LESSON][gTrain->page];
            gTrain->pageNum = gTrain->pageCount[gTrain->sel[1]][TRAIN_CUR_LESSON];
            FontIcon_ResetAnim();
            gTrain->flags |= TRAIN_IMAGE_CHANGE;
        }
        break;
    case TRAIN_LV_PAGES:
        if (gPad[0].gamePressed & PADG_CROSS) {
            gTrain->page++;
            gTrain->pose[1] = 0;
            if (gTrain->page < gTrain->pageNum) {
                gTrain->voiceLine = gTrain->pages[gTrain->sel[1]][TRAIN_CUR_LESSON][gTrain->page];
                if (Train_PageHasImage()) {
                    gTrain->flags |= TRAIN_IMAGE_CHANGE;
                    Flash_GotoLabel(&gTrain->flash[0], "fl_ss_view", 1);
                }
                FontIcon_ResetAnim();
                Snd_PlaySe(2, 0x29);
            } else {
                u8 flags = Train_FindLesson()->flags;

                Snd_PlaySe(1, 1);
                if ((TRAIN_IS_TUTORIAL(flags) || (flags & TRAIN_LESSON_BATTLE)) && gTrain->voiceLine == 0x28) {
                    gTrain->result = 1;
                    Train_SaveCursor();
                    Train_CopyCursor(TRAIN_PROG->cursor, gTrain->sel);
                    gTrain->flags |= TRAIN_LEAVING;
                    if (Train_IsCleared(gTrain->sel[1], TRAIN_CUR_LESSON)) {
                        TRAIN_PROG->flags |= MPTRAIN_AGAIN;
                    } else {
                        TRAIN_PROG->flags |= MPTRAIN_FIRST_CLEAR;
                    }
                } else if (TRAIN_IS_TUTORIAL(flags) || (flags & TRAIN_LESSON_BATTLE)) {
                    gTrain->voiceLine = 0x28;
                    MsgWin_SetText(gTrain->text);
                } else {
                    gTrain->voiceLine = -1;
                    MsgWin_SetBoxParam(0, 0);
                    MsgWin_SetText(gTrain->msgText[1]);
                    if (Train_IsCleared(gTrain->sel[1], TRAIN_CUR_LESSON)) {
                        gTrain->level = TRAIN_LV_LESSON;
                        Flash_GotoLabel(&gTrain->flash[0], "fl_ss_view_out", 1);
                        Train_NextRow();
                        Train_Say(gTrain->voiceTbl[gTrain->sel[1]][TRAIN_CUR_LESSON]);
                    } else {
                        gTrain->level = TRAIN_LV_CLEAR;
                        Flash_GotoLabel(&gTrain->flash[0], "fl_clear_in_1", 1);
                        Train_Say(0x39);
                    }
                }
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gTrain->pose[1] = 0;
            gTrain->level = TRAIN_LV_LESSON;
            Flash_GotoLabel(&gTrain->flash[0], "fl_ss_view_out", 1);
            Train_Plate2Goto(gTrain->row, 1);
            Snd_PlaySe(1, 2);
            MsgWin_SetBoxParam(0, 0);
            MsgWin_SetText(gTrain->msgText[1]);
            Train_Say(gTrain->voiceTbl[gTrain->sel[1]][TRAIN_CUR_LESSON]);
        }
        break;
    case TRAIN_LV_CLEAR:
        if (!(gTrain->flags & TRAIN_CLEAR_SAVED)) {
            gSaveTrain->trainClear[gTrain->sel[1]] |= 1 << TRAIN_CUR_LESSON;
            gTrain->flags |= TRAIN_CLEAR_SAVED;
        }
        done = 0;
        if (TRAIN_VOICE_DONE(0x39)) {
            if (Train_IsClassCleared(gTrain->sel[1])) {
                Train_Say(gTrain->sel[1] + 0x3A);
            } else {
                done = 1;
            }
        } else if (TRAIN_VOICE_DONE(gTrain->sel[1] + 0x3A)) {
            if (Train_IsAllCleared()) {
                Train_Say(0x3D);
            } else {
                done = 1;
            }
        } else if (TRAIN_VOICE_DONE(0x3D)) {
            if (Train_IsAllCleared()) {
                Train_Say(0x3E);
            } else {
                done = 1;
            }
        } else if (TRAIN_VOICE_DONE(0x3E)) {
            done = 1;
        }
        if (gPad[0].gamePressed & PADG_CROSS) {
            Snd_PlaySe(1, 1);
        }
        if (done) {
            gTrain->level = TRAIN_LV_LESSON;
            Flash_GotoLabel(&gTrain->flash[0], "fl_clear_out", 1);
            if (gTrain->flags & TRAIN_CLEAR_SAVED) {
                gTrain->flags ^= TRAIN_CLEAR_SAVED;
            }
            Train_NextRow();
            Train_Say(gTrain->voiceTbl[gTrain->sel[1]][TRAIN_CUR_LESSON]);
        }
        break;
    }
    if (gPad[0].gamePressed != 0 || gPad[0].gameRepeat != 0) {
        gTrain->idle = 0;
    }
    if (gTrain->idle > TRAIN_IDLE_FRAMES) {
        gTrain->idle = 0;
        gTrain->flags |= TRAIN_IDLE_SAID;
        if (gTrain->level == TRAIN_LV_TOP) {
            Train_Say(Rand_Range(2) + 4);
            if (gTrain->pose[0] != 0) {
                gTrain->pose[0] = 0;
            } else {
                gTrain->pose[0] = 1;
            }
        } else if (gTrain->level < TRAIN_LV_INTRO) {
            Train_Say(Rand_Range(2) + 4);
        }
    } else {
        gTrain->idle++;
    }
    if (gTrain->flags & TRAIN_IDLE_SAID) {
        if (gTrain->level == TRAIN_LV_TOP) {
            if (Voice_GetStat(0) == MVOICE_IDLE) {
                /* compares with a new random draw, not with the line that was said */
                if (gTrain->voiceLine == Rand_Range(2) + 4) {
                    Snd_PlaySe(2, 0x28);
                    if (gTrain->flags & TRAIN_IDLE_SAID) {
                        gTrain->flags ^= TRAIN_IDLE_SAID;
                    }
                }
            }
        }
    }
}

/* Loads the picture of the current page (file 0x402 + page id) in the background, then fades it in. */
void Train_UpdateImage(void) {
    MTexRes *res;

    if (gTrain->flags & TRAIN_IMAGE_CHANGE) {
        gTrain->imageAlpha = 0.0f;
        if (gTrain->flags & TRAIN_IMAGE_CHANGE) {
            gTrain->flags ^= TRAIN_IMAGE_CHANGE;
        }
        gTrain->flags |= TRAIN_IMAGE_LOADING;
        File_CancelRequests();
        File_Request(gTrain->voiceLine + 0x402, gTrain->imageFile, 0xE000);
    }
    if (gTrain->flags & TRAIN_IMAGE_LOADING) {
        if (File_UpdateRequests()) {
            res = NULL;
            if (gTrain->flags & TRAIN_IMAGE_LOADING) {
                gTrain->flags ^= TRAIN_IMAGE_LOADING;
            }
            Sprite_Unpack(gTrain->imageFile, gTrain->imageRes, NULL);
            res = gTrain->imageRes;
            Res_RelocateOffsets(&res, res, res);
            gTrain->tex[9] = MTEX(res, 0);
        }
    } else {
        gTrain->imageAlpha += 0.05f;
        if (gTrain->imageAlpha > 1.0f) {
            gTrain->imageAlpha = 1.0f;
        }
    }
}

/* Columns of the per-lesson battle parameters in Train_Leave, looked up by TrainLesson.id. */
#define TRAINCPU_ID 0
#define TRAINCPU_LEVEL 1   /* CPU level of both members */
#define TRAINCPU_ITEM 2    /* item id put in the last item slot */

/*
 * Leaving for a lesson (result 1): sets up the practice battle and / or the tutorial number, by the lesson's
 * flags. THE BATTLE HAND-OFF of this screen.
 */
void Train_Leave(void) {
    u8 flags;
    s32 i;

    if (gTrain->result != 1) {
        return;
    }
    flags = Train_FindLesson()->flags;
    if (flags & TRAIN_LESSON_BATTLE) {
        s32 tbl[8][3] = {
            {7, -1, 0x89},     {0x11, 0xF, 0x99}, {0x15, -1, 0x89},  {0x16, -1, 0x89},
            {0x19, 0xF, 0x99}, {0x1A, -1, 0x89},  {0x1B, 0xF, 0x9A}, {0x1C, -1, 0x89},
        };
        TrainLesson *rec = Train_FindLesson();
        u16 items[8];
        s32 stats[8];
        s32 ability[4];
        s32 bgm = rec->bgm;
        s32 stage = rec->stage;
        s32 cpuLevel;

        i = 0;
        memset(items, 0, sizeof(items));
        Battle_ClearWork();
        cpuLevel = -999;
        for (;; i++) {
            if (i >= 8) {
                break;
            }
            if (tbl[i][TRAINCPU_ID] == rec->id) {
                cpuLevel = tbl[i][TRAINCPU_LEVEL];
                items[7] = tbl[i][TRAINCPU_ITEM];
                break;
            }
        }
        if (cpuLevel != -999) {
            ItemSet_GetStats(items, stats, ability, 0x72);
        }
        BattleSetup_SetRule(0, 5, bgm, 0, 7, stage, 0);
        BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, 0);
        BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, 0);
        {
            u8 *chara = rec->chara;

            for (i = 0; i < 2; i++) {
                BattleSetup_SetMember(i, 0, *chara++, 0, 0, cpuLevel, 100.0f, items);
            }
        }
        BattleSetup_Finish();
        TRAIN_PROG->flags |= MPTRAIN_BATTLE;
    }
    if (TRAIN_IS_TUTORIAL(flags)) {
        s32 index = Train_FindLessonIndex();

        switch (gTrain->sel[1]) {
        case 0:
            TRAIN_PROG->tutorial = index;
            break;
        case 1:
            TRAIN_PROG->tutorial = index + TRAIN_CLASS0_NUM;
            break;
        case 2:
            TRAIN_PROG->tutorial = index + TRAIN_CLASS0_NUM + TRAIN_CLASS1_NUM;
            break;
        }
        TRAIN_PROG->unk7F8 = 0;
        TRAIN_PROG->flags |= MPTRAIN_TUTORIAL;
    }
}

/* The screen's frame loop. Returns 0 (back to the main menu), 1 (a lesson's battle) or 0x2D (character select). */
s32 Train_Run(s32 section) {
    s32 result;

    Train_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        Train_Update();
        Train_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (Train_CheckLeave()) {
            break;
        }
        Train_Input();
        Train_UpdateImage();
    }
    result = gTrain->result;
    Train_Leave();
    Train_Term();
    Dma_ResetBuffers();
    return result;
}
