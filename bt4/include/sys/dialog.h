#ifndef SYS_DIALOG_H
#define SYS_DIALOG_H

#include "types.h"

/*
 * Two-choice confirmation window ("Yes / No" dialog). Source range 0x268248-0x269228.
 * Source: src/sys/dialog.c (written under the placeholder stem lib_a).
 *
 * One window exists at a time, heap-allocated (0x8C bytes, heap 2) by Dialog_Init and reached
 * through gDialog. It is a UI animation object ("Flash", 0x2C bytes) made from a resource file,
 * plus text taken from two message tables. The animation has the clips
 *   mc_dummy_text_1 .. 4   anchors (position + alpha) for the texts
 *   mc_menu_plate_1 / 2    the two choices, each with sub-clips mc_menu_text_off / mc_menu_text_on
 * and the labels fl_window_s_in / _l_in / _s_out / _l_out / _s_open / _l_open (window, small or
 * large), fl_on_start / fl_off_start / fl_ok (choice plates).
 */

/* The animation object as far as this module reads it (owned by the 0x10D4F0 library). */
typedef struct DialogFlash {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ s32 flags; /* bit 1 (2): window fully open (choices usable); bit 3 (8): close animation finished */
    /* 0x0C */ s32 unkC[8];
} DialogFlash; /* size 0x2C */

/* Result of Flash_FindLabel: a clip / label handle, id < 0 when not found. */
typedef struct DialogFlashRef {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 unk4;
} DialogFlashRef;

/* Texture / palette set handed to Flash_Create. */
typedef struct DialogFlashRes {
    /* 0x00 */ void *tex0;   /* Dialog +0x34: file part 1; cleared by Dialog_Draw(0) to hide it */
    /* 0x04 */ void *tex1;   /* +0x38: file part 4 */
    /* 0x08 */ void *clut1;  /* +0x3C: file part 4 + 0x40 */
    /* 0x0C */ s32 unkC[3];
    /* 0x18 */ void *tex2;   /* +0x4C: file part 2 */
    /* 0x1C */ void *tex3;   /* +0x50: file part 3 */
    /* 0x20 */ void *clut2;  /* +0x54: file part 2 + 0x40 */
    /* 0x24 */ void *clut3;  /* +0x58: file part 3 + 0x40 */
    /* 0x28 */ s32 unk28;
} DialogFlashRes; /* size 0x2C */

/* Dialog.flags */
#define DIALOG_FLAG_DECIDED 0x01  /* a choice was confirmed */
#define DIALOG_FLAG_CLOSED 0x02   /* close animation has finished (Dialog_IsClosed) */
#define DIALOG_FLAG_OPEN 0x04     /* opened */
#define DIALOG_FLAG_CLOSING 0x08  /* close requested */
#define DIALOG_FLAG_CURSOR_ON 0x10 /* the cursor plate's fl_on_start was played after opening */

/* Dialog_Start commands */
#define DIALOG_CMD_OPEN 0   /* play the "in" animation + SE 4, unless already open */
#define DIALOG_CMD_CLOSE 1  /* play the "out" animation + SE 5, unless already closing */
#define DIALOG_CMD_SHOW 2   /* jump to the "open" state without sound */

/* Dialog_Input results */
#define DIALOG_RESULT_NONE 0
#define DIALOG_RESULT_FIRST 1    /* choice 0 confirmed */
#define DIALOG_RESULT_SECOND (-2) /* choice 1 confirmed */
#define DIALOG_RESULT_CANCEL (-1)

typedef struct Dialog {
    /* 0x00 */ u32 *titleTbl;      /* message table from the resource file (part 6): [i + 1] = offset of text i */
    /* 0x04 */ u32 *msgTbl;        /* message table given by the caller, same layout */
    /* 0x08 */ DialogFlash flash[1];
    /* 0x34 */ DialogFlashRes res;
    /* 0x60 */ void *tex0;         /* copy of res.tex0, restored by Dialog_Draw(1) */
    /* 0x64 */ s32 flags;          /* DIALOG_FLAG_* */
    /* 0x68 */ s32 cursor;         /* highlighted choice, 0 or 1 */
    /* 0x6C */ s32 msgIdx;         /* text shown in the body, < 0 = none */
    /* 0x70 */ s32 defCursor;      /* choice the cursor starts on when the window opens */
    /* 0x74 */ s32 size;           /* 0 = small window, 1 = large window */
    /* 0x78 */ s32 layout;         /* 0 = one text from msgTbl; 1 = heading from titleTbl + body from msgTbl */
    /* 0x7C */ s32 titleIdx;       /* heading text (layout 1), < 0 = none */
    /* 0x80 */ s32 unk80;
    /* 0x84 */ s32 choices;        /* non-zero: the two choice plates are shown and input is read */
    /* 0x88 */ s32 port;           /* controller port that operates the window */
} Dialog; /* size 0x8C */

extern Dialog *gDialog;

void Dialog_DrawText(void);
#ifdef PORT
void Dialog_SetChoiceLabels(u16 *first,u16 *second);
#endif
void Dialog_DrawTitle(void);
void Dialog_DrawBody(void);
void Dialog_PlayCursorPlate(s32 flash, const char *label);
void Dialog_Init(u32 *file, u32 *msgTbl, s32 size);
void Dialog_Term(void);
void Dialog_Draw(s32 visible);
void Dialog_Start(s32 cmd);
s32 Dialog_Input(s32 allowCancel);
void Dialog_SetChoices(s32 on);
void Dialog_SetPort(s32 port);
void Dialog_SetLayout(s32 layout);
void Dialog_SetMsgTable(u32 *tbl);
void Dialog_SetMsg(s32 idx);
void Dialog_SetTitle(s32 idx);
void Dialog_SetCursor(s32 choice);
s32 Dialog_IsClosed(void);
s32 Dialog_IsOpen(void);
u32 *Dialog_GetTitleTable(void);

#endif
