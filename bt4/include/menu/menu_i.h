#ifndef MENU_MENU_I_H
#define MENU_MENU_I_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x35A558..0x35F650 (placeholder stem "menu_i"). Four pieces:
 *
 *   (menu_i.c  0x35A558..0x35D660  Train     rest of the training menu (mode 44): merged into menu_h_d.c)
 *   menu_i_b.c  0x35D660..0x35D948  BootCard  the memory card check of the first run
 *   menu_i_c.c  0x35D948..0x35E0F8  Logo / FirstRun: the boot logos and the first-run sequence
 *   menu_i_d.c  0x35E0F8..0x3623A8  EntrySel  the tournament's entrant select (mode 34), with menu_j.c merged in
 *
 * Train below is the view of the whole object (menu_h_d.c, which includes menu_h.h and then this header; the
 * partial view that menu_h.h had is gone).
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void MsgWin_SetText(void *text);
extern void MsgWin_SetBoxParam(s32 a, s32 b);
extern void IconWin_SetIcon(s32 icon);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetUnkC(MTextBox *box, s32 a, s32 b);
extern void TextBox_SetMaxSize(MTextBox *box, s32 w, s32 h);
extern void TextBox_SetLineOffsets(MTextBox *box, s32 a, s32 b, s32 c, s32 d, s32 e);
extern void FontIcon_SetPadType(s32 type);
extern void FontIcon_ResetAnim(void);
extern void Battle_ClearWork(void);
extern s32 File_LoadPartitionNw(s32 pt);
extern s32 File_IsPartitionLoaded(s32 pt);
extern void File_WaitPartitionTimeout(s32 pt);
extern void Load_InitScreen(void);
extern void Load_UpdateScreen(void);
extern void Load_DrawScreen(void);
extern void Load_ReadInput(void);

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* ---- Train (menu_i.c): the training menu (mode 44). This chunk's own view of the work area. ---- */

extern void ItemSet_GetStats(u16 *ids, s32 *stats, s32 *ability, s32 chara);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *charaBits); /* (a pointer: declared s32 it was passed as 32 bits on the stack, the upper half of what the function reads left to chance on a 64-bit build) */
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  u16 *items);
extern void BattleSetup_Finish(void);

#define TRAIN_FLASH_NUM 1
#define TRAIN_CLASS_NUM 3
#define TRAIN_LESSON_MAX 15
#define TRAIN_PAGE_MAX 30
#define TRAIN_ROWS 3
#define TRAIN_VOICE_BASE_A 0x87BD
#define TRAIN_VOICE_BASE_B 0x8278

/* One lesson: 0x10-byte records in section 22 of the screen's pack ("ut_normal_index_data"), the three classes
   one after another (13, 11 and 14 records; the last record of a class has TRAIN_LESSON_LAST). */
typedef struct TrainLesson {
    /* 0x00 */ u16 id;         /* line of the lesson's name in the list text; also the key of Train_Leave's table */
    /* 0x02 */ u8 pageNum;     /* explanation pages */
    /* 0x03 */ u8 flags;       /* TRAIN_LESSON_ */
    /* 0x04 */ s32 firstPage;  /* first page id: text line, and picture file 0x402 + id */
    /* 0x08 */ s32 voice;      /* subtitle line said when the cursor is on the lesson */
    /* 0x0C */ u8 stage;       /* battle stage */
    /* 0x0D */ u8 bgm;         /* battle music */
    /* 0x0E */ u8 chara[2];    /* character of side 0 (pad) and side 1 (CPU) */
} TrainLesson; /* 0x10 */

#define TRAIN_LESSON_TUTORIAL 1   /* ends in the tutorial (gProgress session lesson number) */
#define TRAIN_LESSON_BATTLE 2     /* ends in a practice battle set up here */
#define TRAIN_LESSON_LAST 4

#define TRAIN_CLASS0_NUM 13
#define TRAIN_CLASS1_NUM 11
#define TRAIN_CLASS2_NUM 14

/* Tables of the pack (0x1BBC). */
typedef struct TrainTbl {
    /* 0x00 */ TrainLesson *lessons; /* section 22 */
    /* 0x04 */ s32 *skip;            /* section 24 + 0x10 ("tu_noraml_botu"): page ids left out of the lists */
    /* 0x08 */ u32 skipNum;          /* section 24, word 0 */
    /* 0x0C */ s32 *noImage;         /* section 26 + 0x10 ("tu_noraml_notexture"): pages that keep the picture */
    /* 0x10 */ u32 noImageNum;       /* section 26, word 0 */
} TrainTbl; /* 0x14 */

/* What Train_BuildLists fills in: the five tables of Train from 0x47C on. */
typedef struct TrainLists {
    /* 0x0000 */ s32 pages[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX][TRAIN_PAGE_MAX];
    /* 0x1518 */ s32 nameLine[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX];
    /* 0x15CC */ s32 voiceTbl[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX];
    /* 0x1680 */ s32 pageCount[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX];
    /* 0x1734 */ s32 count[TRAIN_CLASS_NUM];
} TrainLists; /* 0x1740 */

/* Eye and mouth animation of the two guides (0 Great Saiyaman, 1 Videl). It has to be a structure of its own
   for Train_Init's loop to match (offset 0x100 split as 0xF0 + 0x10); where it really starts is a guess. */
typedef struct TrainGuide {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ s32 blink[2];
    /* 0x10 */ s32 talk[2];
} TrainGuide; /* 0x18 */

typedef struct Train {
    /* 0x0000 */ u32 *pack;            /* this screen's section of archive 6 (compressed) */
    /* 0x0004 */ u32 *res;             /* the same unpacked */
    /* 0x0008 */ MFlash flash[TRAIN_FLASH_NUM];
    /* 0x0034 */ void *bg;             /* section 2: background picture */
    /* 0x0038 */ u8 *tex[48];          /* [9] the explanation picture */
    /* 0x00F8 */ TrainGuide guide;
    /* 0x0110 */ MTextBox box[5];      /* the lesson names: three rows and two scrolling in or out */
    /* 0x03CC */ u8 unk3CC[0x3C];      /* passed to MsgWin_Init, which ignores it */
    /* 0x0408 */ s32 pose[2];          /* per guide: which of two pictures */
    /* 0x0410 */ void *msgText[2];     /* sections 14 (top menu), 15 (class and lesson menus) */
    /* 0x0418 */ void *subtitlesA;     /* section 12: used with voice base 0x87BD while level is 0 */
    /* 0x041C */ void *subtitlesB;     /* section 13: used with voice base 0x8278 otherwise */
    /* 0x0420 */ void *text;           /* section 16: the lesson names; also shown for line 0x28 */
    /* 0x0424 */ f32 cloud;            /* scroll position of the background clouds */
    /* 0x0428 */ void *imageFile;      /* 0xE000 bytes: compressed explanation picture */
    /* 0x042C */ MTexRes *imageRes;    /* 0x10800 bytes: the same unpacked */
    /* 0x0430 */ void *unk430;         /* 0x10800 bytes, allocated and freed only */
    /* 0x0434 */ void *pageText;       /* section 17: text of the explanation pages */
    /* 0x0438 */ s32 result;           /* Train_Run's result: 0 back, 1 battle or tutorial, 0x2D character select */
    /* 0x043C */ s32 voiceLine;        /* subtitle / text line shown by the message window, -1 = none */
    /* 0x0440 */ s32 page;             /* explanation page shown */
    /* 0x0444 */ s32 sel[2];           /* cursor of menu level 0 and 1 ([1] is the class) */
    /* 0x044C */ s32 saved[TRAIN_CLASS_NUM]; /* lesson the cursor was on, per class */
    /* 0x0458 */ s32 level;            /* TRAIN_LV_ */
    /* 0x045C */ s32 top;              /* first visible lesson */
    /* 0x0460 */ s32 row;              /* row the cursor is on, 0..2 */
    /* 0x0464 */ s32 extra;            /* lesson shown on the fourth plate while the list scrolls */
    /* 0x0468 */ s32 extra2;           /* lesson shown on the fifth plate */
    /* 0x046C */ s32 unk46C;           /* 30, never read here */
    /* 0x0470 */ s32 idle;             /* frames without input */
    /* 0x0474 */ f32 imageAlpha;
    /* 0x0478 */ s32 flags;            /* TRAIN_ */
    /* 0x047C */ s32 pages[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX][TRAIN_PAGE_MAX]; /* page ids of each listed lesson */
    /* 0x1994 */ s32 nameLine[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX];  /* TrainLesson.id: text line of the name */
    /* 0x1A48 */ s32 voiceTbl[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX];  /* TrainLesson.voice: subtitle line */
    /* 0x1AFC */ s32 pageCount[TRAIN_CLASS_NUM][TRAIN_LESSON_MAX]; /* pages listed */
    /* 0x1BB0 */ s32 count[TRAIN_CLASS_NUM];                       /* lessons listed per class */
    /* 0x1BBC */ TrainTbl tbl;
    /* 0x1BD0 */ s32 clipX;
    /* 0x1BD4 */ s32 clipY;
    /* 0x1BD8 */ s32 pageNum;          /* pages of the lesson being explained */
    /* 0x1BDC */ s32 unk1BDC;
} Train; /* 0x1BE0 */

/* level */
#define TRAIN_LV_TOP 0       /* "training" / "character select" */
#define TRAIN_LV_CLASS 1     /* the three classes */
#define TRAIN_LV_LESSON 2    /* the lesson list */
#define TRAIN_LV_INTRO 3     /* the guide introduces the lesson */
#define TRAIN_LV_PAGES 4     /* the explanation pages */
#define TRAIN_LV_CLEAR 5     /* "lesson cleared" */

/* flags */
#define TRAIN_IMAGE_CHANGE 1   /* load the picture of the current page */
#define TRAIN_IMAGE_LOADING 2
#define TRAIN_LEAVING 8
#define TRAIN_FADING 0x10
#define TRAIN_STARTED 0x20     /* the first Update ran */
#define TRAIN_GREETED 0x40
#define TRAIN_IDLE_SAID 0x80   /* the idle line is being said */
#define TRAIN_CLEAR_SAVED 0x200

#define TRAIN_IDLE_FRAMES 0xE10

/* gProgress: the training session block (cleared by Progress_ClearSession). */
typedef struct MenuProgressTrain {
    /* 0x000 */ u8 unk0[0x7D4];
    /* 0x7D4 */ s32 flags;       /* MPTRAIN_ */
    /* 0x7D8 */ s32 unk7D8;
    /* 0x7DC */ s32 cursor[5];   /* Train.sel and Train.saved while the menu is left */
    /* 0x7F0 */ s32 unk7F0;
    /* 0x7F4 */ s32 tutorial;    /* tutorial number: lesson index + 0 / 13 / 24 by class */
    /* 0x7F8 */ s32 unk7F8;
} MenuProgressTrain;

#define TRAIN_PROG ((MenuProgressTrain *)gProgress)

#define MPTRAIN_TUTORIAL 1      /* the menu was left for a tutorial */
#define MPTRAIN_BATTLE 2        /* the menu was left for a practice battle */
#define MPTRAIN_FIRST_CLEAR 4   /* the lesson left for had not been cleared */
#define MPTRAIN_AGAIN 8         /* it had */
#define MPTRAIN_SELECT 0x10     /* the menu was left for the character select */

/* gSaveData (include/sys/save.h): one word of "lesson cleared" bits per class. */
typedef struct MSaveTrain {
    /* 0x000 */ u8 unk0[0xE0C];
    /* 0xE0C */ s32 trainClear[TRAIN_CLASS_NUM];
} MSaveTrain;

struct SaveData;
extern struct SaveData *gSaveData; /* include/sys/save.h */
#define gSaveTrain ((MSaveTrain *)gSaveData)

extern Train *gTrain; /* 0x3B4BA8 */

/* menu_h */
s32 Train_IsCleared(s32 class, s32 lesson);
s32 Train_IsClassCleared(s32 class);
s32 Train_IsAllCleared(void);
void Train_CopyCursor(s32 *dst, s32 *src);
void Train_Plate2Goto(s32 plate, s32 on);
void Train_CursorGoto(s32 on);
void Train_DrawClearIcon(void);
void Train_DrawPlates(void);
void Train_DrawList(void);
void Train_SetClip(void);
void Train_DrawArrows(void);
void Train_NextRow(void);
void Train_MoveCursor(s32 dir);

void Train_RestoreScroll(void);
void Train_SaveCursor(void);
void Train_BuildLists(TrainTbl *tbl, TrainLists *out);
void Train_DrawTitle(void);
void Train_DrawIconWin(void);
void Train_DrawBg(void);
void Train_DrawGuides(void);
void Train_Init(s32 section);
void Train_Term(void);
void Train_Update(void);
void Train_Draw(void);
s32 Train_CheckLeave(void);
void Train_Input(void);
void Train_UpdateImage(void);
void Train_Leave(void);
s32 Train_Run(s32 section);

/* ---- BootCard (menu_i_b.c) ---- */

typedef struct BootCard {
    /* 0x00 */ u32 *pack;    /* section of archive 0 (compressed) */
    /* 0x04 */ u32 *res;     /* the same unpacked; section 1 is the dialog file */
    /* 0x08 */ s32 flags;    /* BOOTCARD_ */
    /* 0x0C */ s32 timer;    /* frames until the fade out starts once the flow is done */
    /* 0x10 */ s32 mcState;  /* McFlow_Update result */
} BootCard; /* 0x14 */

#define BOOTCARD_DONE 1
#define BOOTCARD_LEAVING 2
#define BOOTCARD_STARTED 8   /* McFlow_Start(2) was called */

extern BootCard *gBootCard; /* 0x3B5908 */

void BootCard_OnFlowDone(void);
void BootCard_Init(s32 section);
void BootCard_Term(void);
s32 BootCard_Run(s32 section);

/* ---- Logo (menu_i_c.c) ---- */

typedef struct Logo {
    /* 0x00 */ void *pic;    /* the picture (relocated texture list) */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 flags;    /* LOGO_ */
    /* 0x0C */ s32 frames;   /* frames the picture stays */
    /* 0x10 */ s32 timer;    /* frames since the fade in ended */
    /* 0x14 */ s32 unk14[4];
} Logo; /* 0x24 */

#define LOGO_FADED_IN 1

/* Logo_Show skip modes */
#define LOGO_SKIP_NONE 0
#define LOGO_SKIP_ANY 1      /* confirm / start ends the picture at once, fading twice as fast */
#define LOGO_SKIP_AFTER_60 2 /* the same, but only after 60 frames */

extern Logo *gLogo; /* 0x3B590C */

void Logo_Init(u32 *pack, s32 section, s32 frames);
void Logo_Term(void);
void Logo_Show(u32 *pack, s32 section, s32 frames, s32 fade, s32 skip, s32 inBlack, s32 outBlack, s32 fadeFirst);
void Logo_ShowAll(u32 *pack);
void FirstRun_WaitPartition(s32 pt);
s32 FirstRun_Main(void);
s32 FirstRun_WaitPad();

/* ---- EntrySel (menu_i_d.c): the entrant select of the tournament mode (mode 34): what its first half needs
   beyond menu_j.h. ---- */

extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern void ItemPanel_Init(u32 *pack, s32 side);     /* menu_g */
extern void TourBg_Init(void *file, u32 kind, u8 **tex); /* menu_k (0x366F58) */
extern void ItemHelp_Init(void *pack);

/* Always-loaded resources (include/sys/common.h): common file 4 is data[2]. menu_h.h has the same view. */
#ifndef MENU_MENU_H_H
typedef struct MCommonRes {
    /* 0x00 */ void *boot;
    /* 0x04 */ void *data[3];
} MCommonRes;

extern MCommonRes *gCommonRes;
#endif

/* The work area (ESel), the grid views, the gProgress view, gEntrySel (0x3B5910) and the prototypes are in
   menu/menu_j.h. */

#endif
