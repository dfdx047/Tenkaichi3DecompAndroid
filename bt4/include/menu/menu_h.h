#ifndef MENU_MENU_H_H
#define MENU_MENU_H_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (include/sys/snd.h), and the code after a call
   shows it (the next value goes to v1, not v0). Hide that declaration and give the right one. */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x356090..0x35A558 (placeholder stem "menu_h"). Four pieces:
 *
 *   (menu_h.c  0x356090..0x3562B8  DuelMenu_Run: merged into menu_g_c.c, the object it ends; view in menu_g.h)
 *   menu_h_b.c  0x3562B8..0x3590A8  CharRef     the character reference screen (mode 60)
 *   menu_h_c.c  0x3590A8..0x359358  the handlers of mode 60 and of modes 44..45 (not part of the CharRef object;
 *                                               possibly the head of the Train object)
 *   menu_h_d.c  0x359358..0x35D660  Train       the training menu (mode 44), with menu_i.c (0x35A558..) merged in
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern s32 rand(void);
extern char *strcpy(char *, const char *);
extern s32 Job_Run(void);
extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern s32 Font_GetHeight(u16 *str);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetScale(MFlash *flash, MFlashRef *ref, f32 x, f32 y);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Flash_ClipGetPos(MFlash *flash, MFlashRef *ref, s32 *x, s32 *y);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetRect(MTextBox *box, s32 x0, s32 x1, s32 y0, s32 y1);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void MsgWin_SetSide(s32 side);
extern void Voice_Stop(s32 voice);
extern void Voice_StopWithLip(void);
extern void Voice_PlayChara(s32 voice, s32 chara, s32 line);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern void ChrView_Init(void);
extern void ChrView_Update(void);
extern void ChrView_Term(void);
extern s32 ChrView_Show(s32 chara, s32 costume, s32 damaged);
extern s32 ChrView_Hide(void);
extern s32 ChrView_IsVisible(void);

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* Common file 4 (gCommonRes->data[2]): section 1 is the character table (battle/view_b.h has the full entry). */
typedef struct MCommonRes {
    /* 0x00 */ void *boot;
    /* 0x04 */ void *data[3];
} MCommonRes;

typedef struct MChrTblEntry {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ u16 flags;      /* bit 0 clear: the character uses the second base plate */
    /* 0x0A */ u8 unkA[0x32];
} MChrTblEntry; /* 0x3C */

extern MCommonRes *gCommonRes;

/* ---- CharRef (menu_h_b.c): the character reference with Chi-Chi's comments (mode 60) ---- */

#define CHARREF_FLASH_NUM 1
#define CHARREF_CHARA_MAX 161
#define CHARREF_ROWS 4            /* list rows the cursor can be on */
#define CHARREF_VOICE_BASE 0x82B7

/* One listed (unlocked) character; the entries are a doubly linked list in list order. */
typedef struct CharRefEntry {
    /* 0x00 */ struct CharRefEntry *prev;
    /* 0x04 */ struct CharRefEntry *next;
    /* 0x08 */ s32 chara;        /* character id */
    /* 0x0C */ s32 commentLine;  /* first subtitle line of the guide's comment on this character, minus 9 */
    /* 0x10 */ s32 commentNum;   /* lines the comment has */
} CharRefEntry; /* 0x14 */

/* TextBox indices */
#define CHARREF_BOX_NAME 0
#define CHARREF_BOX_FORM 1
#define CHARREF_BOX_VOICE 2
#define CHARREF_BOX_PROFILE 3
#define CHARREF_BOX_LIST_NAME 4   /* 5 of them: four rows and the one scrolling in */
#define CHARREF_BOX_LIST_FORM 9   /* 5 of them */
#define CHARREF_BOX_NUM 14

typedef struct CharRef {
    /* 0x0000 */ u32 *pack;          /* this screen's section of archive 9 (compressed) */
    /* 0x0004 */ void *imageFile;    /* 0x16800 bytes: the compressed large picture of the current character */
    /* 0x0008 */ MTexRes *imageRes;  /* 0x20800 bytes: the same unpacked */
    /* 0x000C */ u32 *res;           /* the pack unpacked: 29 sections */
    /* 0x0010 */ void *msgText;      /* section 21 */
    /* 0x0014 */ void *subtitles;    /* section 20 */
    /* 0x0018 */ void *text[4];      /* sections 22 (names), 23 (forms), 25, 25 (voice labels and profiles) */
    /* 0x0028 */ void *nameText[9];  /* section 22; five used */
    /* 0x004C */ void *formText[9];  /* section 23; five used */
    /* 0x0070 */ MFlash flash[CHARREF_FLASH_NUM];
    /* 0x009C */ void *bg;           /* section 15: background picture */
    /* 0x00A0 */ u8 *tex[44];
    /* 0x0150 */ u8 unk150[0x3C];
    /* 0x018C */ CharRefEntry *head; /* first listed character */
    /* 0x0190 */ CharRefEntry entry[CHARREF_CHARA_MAX];
    /* 0x0E24 */ s32 *charaTbl;      /* section 17: character id per list position, negative = skipped */
    /* 0x0E28 */ s32 *commentTbl;    /* section 26: first comment line per list position */
    /* 0x0E2C */ s32 *commentNumTbl; /* section 28: comment line count per list position */
    /* 0x0E30 */ s32 *poseTbl;       /* section 27: the guide's pose (0 / 1) per comment line */
    /* 0x0E34 */ MTextBox box[CHARREF_BOX_NUM];
    /* 0x15DC */ s32 cursor;         /* list index of the current character */
    /* 0x15E0 */ s32 count;          /* listed characters */
    /* 0x15E4 */ s32 total;          /* 161: list positions */
    /* 0x15E8 */ s32 costume;        /* costume chosen for the model viewer */
    /* 0x15EC */ s32 loadState;      /* CHARREF_LOAD_ */
    /* 0x15F0 */ s32 blink;
    /* 0x15F4 */ s32 talk;
    /* 0x15F8 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x15FC */ s32 menuCursor;     /* item of the details menu: 0 profile, 1 voice, 2 comment, 3 model (5 while the model page is open) */
    /* 0x1600 */ s32 row;            /* list row the cursor is on, 0..3 */
    /* 0x1604 */ s32 idle;           /* frames since the guide last spoke */
    /* 0x1608 */ s32 comment;        /* line of the comment being spoken */
    /* 0x160C */ s32 scroll;         /* vertical offset of the profile text */
    /* 0x1610 */ s32 textHeight;     /* height of the profile text + 4 */
    /* 0x1614 */ s32 pageDir;        /* 0: paged to the previous character, 1: to the next */
    /* 0x1618 */ s32 commentBase;    /* 9 + entry.commentLine */
    /* 0x161C */ s32 pose;           /* which of the guide's two poses is shown */
    /* 0x1620 */ s32 scrollDir;      /* 4: the list scrolled up (cursor went down), 8: down */
    /* 0x1624 */ s32 lastLine;       /* last idle line spoken */
    /* 0x1628 */ s32 state;          /* CHARREF_ST_ */
    /* 0x162C */ s32 prevState;
    /* 0x1630 */ s32 flags;          /* CHARREF_ */
    /* 0x1634 */ f32 imageAlpha;
} CharRef; /* 0x1638 */

#define CHARREF_IMAGE_CHANGE 1   /* the current character changed: load its picture */
#define CHARREF_IMAGE_READY 2    /* the picture is loaded and fades in */

/* loadState: the large picture's loader, one step per frame (the same machine as ModeMenu's) */
#define CHARREF_LOAD_REQUEST 1
#define CHARREF_LOAD_READ 2
#define CHARREF_LOAD_UNPACK 3
#define CHARREF_LOAD_SHOWN 4
#define CHARREF_LOAD_ABORT 5
#define CHARREF_LOAD_RESTART 6

/* state: index into gCharRefState */
#define CHARREF_ST_INTRO 0
#define CHARREF_ST_LIST 1
#define CHARREF_ST_DETAILS 2
#define CHARREF_ST_PROFILE 3
#define CHARREF_ST_VOICE 4
#define CHARREF_ST_COMMENT 5
#define CHARREF_ST_COSTUME 6
#define CHARREF_ST_VIEW 7
#define CHARREF_ST_VIEW_WAIT 8
#define CHARREF_ST_LEAVE 9       /* no handler: the frame loop waits for the fade */

#define CHARREF_STATE_NUM 9

extern CharRef *gCharRef;                              /* 0x3B4854 */
extern void (*gCharRefState[CHARREF_STATE_NUM])(void); /* 0x31EA80: in the main executable's .bss (a common symbol) */

/* ---- Train (menu_h_d.c): the training menu (mode 44) ---- */

#define TRAIN_FLASH_NUM 1
#define TRAIN_CLASS_NUM 3
#define TRAIN_LESSON_MAX 15
#define TRAIN_ROWS 3             /* lesson rows the cursor can be on */

/* The five words Train_CopyCursor copies */
typedef struct TrainCursor {
    /* 0x00 */ s32 sel[2];                  /* [0] item of the top menu, [1] class */
    /* 0x08 */ s32 saved[TRAIN_CLASS_NUM];  /* per class */
} TrainCursor;

/* The work area `Train` (0x1BE0 bytes), the save view and `gTrain` (0x3B4BA8) are in menu/menu_i.h: the object
   is menu_h_d.c with the next chunk's menu_i.c merged in, and it includes both headers. */

#define TRAIN_VOICE_BASE_A 0x87BD
#define TRAIN_VOICE_BASE_B 0x8278

s32 CharRef_Run(s32 section);
s32 CharRefMode_Main(void);
s32 TrainMode_Main(void);

s32 Train_IsCleared(s32 class, s32 lesson);
s32 Train_IsClassCleared(s32 class);
s32 Train_IsAllCleared(void);
void Train_CopyCursor(s32 *dst, s32 *src);
void Train_PlateGoto(s32 plate, s32 on);
void Train_Plate2Goto(s32 plate, s32 on);
void Train_CursorGoto(s32 on);
void Train_DrawClearIcon(void);
void Train_DrawPlates(void);
void Train_DrawList(void);
void Train_SetClip(void);
void Train_HideArrow(s32 up);
void Train_DrawArrows(void);
s32 Train_MoveRow(s32 dir);
void Train_NextRow(void);
void Train_MoveCursor(s32 dir);

#endif
