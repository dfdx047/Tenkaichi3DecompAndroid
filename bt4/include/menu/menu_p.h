#ifndef MENU_MENU_P_H
#define MENU_MENU_P_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x37AFF8..0x37F430 (placeholder stem "menu_p"): screens of the mode group 13..30
 * (handler 0x379A58, archive gMenuArc3, main-menu item 1). Four pieces, cut at object boundaries:
 *
 *   (menu_p.c)  0x37AFF8..0x37B7C0  MisSel     tail of the mission select of mode 14: merged into
 *                                              src/menu/menu_o_d.c (the object starts at 0x379F58)
 *   menu_p_b.c  0x37B7C0..0x37DC38  MisResult  the mission result screen of mode 16 (work pointer 0x3B737C)
 *   menu_p_c.c  0x37DC38..0x37EE18  UbMenu     the menu of mode 13 (work pointer 0x3B7380)
 *   menu_p_d.c  0x37EE18..0x37F850  UbScore    the score sheet module (its tail was written as menu_q.c and
 *                                              merged into menu_p_d.c)
 *
 * All names are guesses from what the code does ("Ub" = the group of modes 13..30, as in menu_m.h). The
 * structures are this chunk's own views.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetMaxWidth(MTextBox *box, s32 w);
extern void TextBox_SetLineOffsets(MTextBox *box, s32 y1, s32 y2, s32 y3, s32 y4, s32 y5);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h,
                          s32 mode, s32 parentFmt);
extern void IconWin_SetIcon(s32 icon);
extern void Voice_StopWithLip(void);
extern s32 Rand_Libc(void);
extern void Save_AddItem(s32 idx);

/*
 * Snd_PlaySe returns a value in the original (menu_a.h declares it void): with a void call the register that holds
 * the work pointer after the call is v0 instead of v1 and the branches of the pad handlers merge differently.
 */
#define Snd_PlaySe ((s32 (*)(u32, s32))Snd_PlaySe)

/* Voice_GetStat result when nothing is playing. */
#define P_VOICE_IDLE 5
/* Voice bank base of this mode group's guides (Voice_PlayWithSubtitle). */
#define P_VOICE_BASE 0x8765

/* The battle result (BattleResult of include/battle/battle.h; local view). */
typedef struct PClock {
    /* 0x00 */ u32 ticks;
    /* 0x04 */ s16 hours;
    /* 0x06 */ s16 minutes;
    /* 0x08 */ s16 seconds;
    /* 0x0A */ s16 ms;
    /* 0x0C */ s16 timeLeft;
    /* 0x0E */ s16 unkE;
} PClock; /* 0x10 */

typedef struct PBattleResult {
    /* 0x00 */ s32 winner;       /* bit 0: side 0 won */
    /* 0x04 */ s32 reason;
    /* 0x08 */ s32 aborted;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u64 eventSummary; /* bit n: bonus n was earned (48 bits used) */
    /* 0x18 */ s32 frames;
    /* 0x1C */ s32 unk1C[2];
    /* 0x24 */ s32 unk24[2];
    /* 0x2C */ f32 health[2];
    /* 0x34 */ PClock clock;
    /* 0x44 */ s32 unk44;
} PBattleResult;

extern PBattleResult *BattleResult_GetPtr(void);

/* gSaveData as these screens use it (include/sys/save.h has the full layout). */
typedef struct PSaveMission {
    /* 0x00 */ u8 cleared;
    /* 0x01 */ u8 rank;         /* of the best score (written in menu_q) */
    /* 0x02 */ u8 time[3];
    /* 0x05 */ u8 unk5[3];
    /* 0x08 */ s32 best;
} PSaveMission; /* 0xC */

#define P_MISSION_NUM 100

/*
 * The mission records start at 0x28C, but the code only matches when they are reached as a member at +0xC of a
 * block at 0x280 (the compiler then keeps 0x280 with the index and 0xC as the displacement).
 */
typedef struct PSaveUb {
    /* 0x000 */ u8 unk0[0xC];
    /* 0x00C */ PSaveMission mission[P_MISSION_NUM];
} PSaveUb;

typedef struct PSave {
    /* 0x0000 */ u8 unk0[0x208];
    /* 0x0208 */ s32 ubFlags;      /* P_UBFLAG_ */
    /* 0x020C */ u8 unk20C[0x74];
    /* 0x0280 */ PSaveUb ub;
    /* 0x073C */ u8 unk73C[0x28EC];
    /* 0x3028 */ s32 money;
} PSave;

extern void *gSaveData;

#define P_SAVE ((PSave *)gSaveData)
#define P_MONEY_MAX 9999999

#define P_UBFLAG_ITEM3 1          /* 30 missions cleared: the third plate of the mode 13 menu (mode 17) is open */
#define P_UBFLAG_ALL_MISSIONS 4   /* all 100 missions cleared */

/* gProgress as these screens use it. */
typedef struct PProgress {
    /* 0x000 */ u8 unk0[0x14];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x61C];
    /* 0x638 */ s32 ubCursor;      /* plate the mode 13 menu was left on (0..3); while it is 0 a score total is capped at 65535 */
    /* 0x63C */ s32 misRank;       /* mission select: page (five missions each) */
    /* 0x640 */ s32 misRow;        /* mission select: plate on the page */
} PProgress;

#define P_PROG ((PProgress *)gProgress)

/* ---- UbScore (menu_p_d.c) ---- */

/* One line of the score sheet. */
typedef struct UbScoreLine {
    /* 0x00 */ s32 value;       /* what was measured (a bonus line: the bonus id) */
    /* 0x04 */ s32 remain;      /* points not yet counted into the total */
    /* 0x08 */ s32 points;      /* points the line is worth */
} UbScoreLine; /* 0xC */

#define UBSCORE_LINE_MAX 4
#define UBSCORE_BONUS_MAX 48
#define UBSCORE_PAGE 3          /* bonus plates on a page */

typedef struct UbScore {
    /* 0x000 */ s32 total;      /* sum of all lines */
    /* 0x004 */ s32 saved;      /* copy of the total taken before it is converted */
    /* 0x008 */ s32 convert;    /* money made of the total and not yet paid out */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ UbScoreLine line[UBSCORE_LINE_MAX];   /* health, unk24, unk1C, battle time */
    /* 0x040 */ UbScoreLine bonus[UBSCORE_BONUS_MAX]; /* one per set bit of the battle's event summary */
    /* 0x280 */ s32 shown[UBSCORE_PAGE]; /* bonus indices on the page shown (set in menu_q) */
    /* 0x28C */ s32 lineCount;  /* 3, or 4 with the time line */
    /* 0x290 */ s32 bonusCount;
    /* 0x294 */ PClock clock;   /* battle clock at the end */
    /* 0x2A4 */ s32 rank;       /* 0 = none, 1..4 */
} UbScore; /* 0x2A8 */

/* An entry of the price table in the screen pack (section 13): what a line value or a bonus is worth. */
typedef struct UbScorePrice {
    /* 0x00 */ s32 line[4];     /* by line; line 2 is looked up by time step */
    /* 0x10 */ s32 bonus;
    /* 0x14 */ s32 unk14;
} UbScorePrice; /* 0x18 */

s32 UbScore_Fill(s32 kind, UbScore *score, s32 *pages);
void UbScore_CalcPoints(UbScorePrice *price, UbScore *score);
s32 UbScore_CalcRank(s32 kind, UbScore *score);
s32 UbScore_Transfer(s32 *from, s32 *to, s32 step, f32 rate);
s32 UbScore_CountLine(UbScore *score, s32 isBonus, s32 index, s32 step);

s32 UbScore_ConvertStep(UbScore *score, s32 step);      /* total -> convert at one tenth; 1 when done */
void UbScore_SetPage(UbScore *score, s32 page);         /* fills UbScore.shown for a bonus page */
void UbScore_PlateGoto(MFlash *flash, s32 plate, s32 on);
void UbScore_AddRanking(s32 score, s32 chara, s32 cleared);
s32 UbScore_SaveMissionBest(s32 mission, s32 total, UbScore *score); /* 1 = a new record was saved */
s32 UbScore_SaveBestB(s32 course, s32 total, UbScore *score);
s32 UbScore_SaveBestC(s32 course, s32 total, UbScore *score);
s32 UbScore_GetRewardItem(s32 idx);

/*
 * gSaveData as the module's record functions use it (the views of the tail half, formerly QSave* of menu_q.h):
 * the body behind the 8-byte checksum, nested as in include/menu/menu_c.h (it reproduces the address arithmetic).
 */

/* An entry of the ranking of the mode 22 ladder, best first. */
typedef struct UbSaveRank {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 score;
    /* 0x08 */ u8 cleared;      /* 1 = the ladder was finished */
} UbSaveRank; /* 0xC */

#define UB_RANK_NUM 10

/* Best result of one mission (PSaveMission above) and of one course of modes 24..30. */
typedef struct UbSaveBest {
    /* 0x00 */ u8 cleared;
    /* 0x01 */ u8 rank;
    /* 0x02 */ u8 hours;
    /* 0x03 */ u8 minutes;
    /* 0x04 */ u8 seconds;
    /* 0x08 */ s32 total;
} UbSaveBest; /* 0xC */

/* Best result of one course of modes 17..19. */
typedef struct UbSaveBestB {
    /* 0x00 */ s32 value;       /* the sheet's fourth line */
    /* 0x04 */ s32 total;
    /* 0x08 */ u8 rank;
    /* 0x09 */ u8 hours;
    /* 0x0A */ u8 minutes;
    /* 0x0B */ u8 seconds;
} UbSaveBestB; /* 0xC */

typedef struct UbSaveBody {
    /* 0x0008 */ u8 unk8[0x200];
    /* 0x0208 */ s32 ubFlags;
    /* 0x020C */ s32 unk20C;
    /* 0x0210 */ UbSaveRank rank[UB_RANK_NUM];
    /* 0x0288 */ s32 unk288;
    /* 0x028C */ UbSaveBest mission[100];
    /* 0x073C */ UbSaveBestB bestB[5];
    /* 0x0778 */ s32 unk778[2];
    /* 0x0780 */ UbSaveBest bestC[54];
} UbSaveBody;

typedef struct UbSave {
    /* 0x00 */ s32 sum[2];
    /* 0x08 */ UbSaveBody body;
} UbSave;

#define UB_SAVE (&((UbSave *)gSaveData)->body)

/* ---- MisSel: merged into src/menu/menu_o_d.c, built with include/menu/menu_o.h (which has its layout) ---- */

#define MISSEL_ROWS 5           /* missions on a page of the mission select */

/* ---- MisResult (menu_p_b.c) ---- */

#define MISRESULT_FLASH_NUM 1

typedef struct MisResult {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 3 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked */
    /* 0x008 */ s32 unk8;
    /* 0x00C */ void *text;         /* section 12 */
    /* 0x010 */ void *subtitles;    /* section 11 */
    /* 0x014 */ void *itemText;     /* section 14: item names */
    /* 0x018 */ MTextBox box[6];    /* 0..2 bonus plates, 3 mission title, 4 / 5 reward window */
    /* 0x360 */ MTextBox itemBox;
    /* 0x3EC */ UbScorePrice *price; /* section 13 */
    /* 0x3F0 */ MFlash flash[MISRESULT_FLASH_NUM];
    /* 0x41C */ void *bg;           /* section 5: background picture */
    /* 0x420 */ u8 *tex[47];        /* textures of the movie's image records */
    /* 0x4DC */ s32 flags;          /* MISRESULT_ */
    /* 0x4E0 */ u8 unk4E0[0x38];
    /* 0x518 */ s32 timer;
    /* 0x51C */ s32 unk51C[2];
    /* 0x524 */ s32 state;          /* MISRESULT_ST_ */
    /* 0x528 */ s32 voiceReq;       /* step of the guide's script, 0 = none */
    /* 0x52C */ s32 voiceLast;
    /* 0x530 */ s32 voiceSkip;
    /* 0x534 */ s32 voiceLine;      /* subtitle line, -1 = none */
    /* 0x538 */ s32 unk538;
    /* 0x53C */ s32 blink;
    /* 0x540 */ s32 talk;
    /* 0x544 */ s32 pages;          /* pages of bonus plates */
    /* 0x548 */ s32 page;           /* 0 = the score lines, 1.. = bonus pages */
    /* 0x54C */ s32 outcome;        /* UbScore_Fill: 0 won, 1 lost, 2 aborted */
    /* 0x550 */ s32 complete;       /* the 100th mission was cleared just now */
    /* 0x554 */ s32 newRecord;
    /* 0x558 */ s32 cleared;        /* missions cleared so far */
    /* 0x55C */ s32 reward;         /* 0 none, 1 the new mode, 2 / 3 an item */
    /* 0x560 */ s32 rewardItem;
    /* 0x564 */ u8 pageDone;        /* every line of the page was counted */
    /* 0x565 */ u8 unk565[3];
    /* 0x568 */ s32 mission;        /* 0..99 */
    /* 0x56C */ UbScore score;
    /* 0x814 */ s32 counting;
    /* 0x818 */ s32 step;           /* line being counted; in the money state its sub step */
    /* 0x81C */ s32 started;        /* the movie's intro has ended */
} MisResult; /* 0x820 */

#define MISRESULT_DONE 1
#define MISRESULT_LEAVING 2
#define MISRESULT_STARTED 4
#define MISRESULT_GREETED 8

#define MISRESULT_ST_INTRO 0
#define MISRESULT_ST_COUNT 1    /* the lines count into the total */
#define MISRESULT_ST_PAGES 2    /* the pages can be turned; confirm goes on */
#define MISRESULT_ST_MONEY 3    /* the total is paid out */
#define MISRESULT_ST_COMPLETE 4 /* "all missions cleared" */
#define MISRESULT_ST_REWARD 5   /* reward window */
#define MISRESULT_ST_LEAVE 6

extern MisResult *gMisResult;

s32 MisResult_Run(s32 section);

/* ---- UbMenu (menu_p_c.c) ---- */

#define UBMENU_FLASH_NUM 1
#define UBMENU_ITEM_NUM 4

typedef struct UbMenu {
    /* 0x00 */ u32 *pack;
    /* 0x04 */ u32 *res;
    /* 0x08 */ void *text;          /* section 8: text of the message window */
    /* 0x0C */ void *subtitles;     /* section 9 */
    /* 0x10 */ MFlash flash[UBMENU_FLASH_NUM];
    /* 0x3C */ void *bg;            /* section 2: background picture */
    /* 0x40 */ u8 *tex[17];         /* textures of the movie's image records */
    /* 0x84 */ s32 flags;           /* UBMENU_ */
    /* 0x88 */ s32 cur[1];          /* plate, 0..3 */
    /* 0x8C */ s32 timer;
    /* 0x90 */ s32 unk90[2];
    /* 0x98 */ s32 level;           /* always 0 */
    /* 0x9C */ s32 script;          /* step of the guides' dialogue, 0 = none */
    /* 0xA0 */ s32 scriptLast;
    /* 0xA4 */ s32 voiceSkip;
    /* 0xA8 */ s32 voiceLine;       /* subtitle line, -1 = none */
    /* 0xAC */ s32 talker;          /* which guide speaks */
    /* 0xB0 */ s32 blink[2];
    /* 0xB8 */ s32 talk[2];
    /* 0xC0 */ s32 idle;
    /* 0xC4 */ s32 iconFrame;
} UbMenu; /* 0xC8 */

#define UBMENU_CHOSEN 1
#define UBMENU_LEAVING 2
#define UBMENU_STARTED 4
#define UBMENU_GREETED 8

extern UbMenu *gUbMenu;
extern s32 gUbMenuPlate3Open;   /* 0x31EA90 (bss): P_UBFLAG_ITEM3 as it was when the screen was entered */

s32 UbMenu_Run(s32 section);

#endif
