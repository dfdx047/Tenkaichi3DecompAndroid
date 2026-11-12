#ifndef MENU_MENU_S_H
#define MENU_MENU_S_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x388618..0x38CB38 (placeholder stem "menu_s"): screens of the mode group 13..30
 * (handler Ub_Main 0x379A58, archive gMenuArc3, main-menu item 1). Four pieces, cut at object boundaries:
 *
 *   (menu_s.c)  0x388618..0x388EE0  SimTop      tail of the entry screen of the "sim" sub family (mode 20): merged
 *                                               into src/menu/menu_r_d.c, where the object starts (0x387880;
 *                                               work pointer 0x3B7420); its view is in include/menu/menu_r.h
 *   menu_s_b.c  0x388EE0..0x38A308  SurvSel     the course select of the survival sub family (mode 17; movie
 *                                               labels "fl_survival_%02d_in"), a whole object (work pointer
 *                                               0x3B743C). Its first function writes the battle setup.
 *   menu_s_c.c  0x38A308..0x38C360  SurvResult  the survival result screen (mode 19), a whole object (work
 *                                               pointer 0x3B7440)
 *   menu_s_d.c  0x38C360..0x38CB38  SimEvent    the first three of the 37 day-event handlers of the sim day
 *                                               screen (gSimEvent[0..2]: the three trainings), each with its
 *                                               outcome roll. The object goes on in the next chunk (menu_t).
 *
 * All names are guesses from what the code does. The structures are this chunk's own views.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern s32 Rand_Libc(void);           /* libc rand(), not the shared Mersenne Twister */
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetLineOffsets(MTextBox *box, s32 y1, s32 y2, s32 y3, s32 y4, s32 y5);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h,
                          s32 mode, s32 parentFmt);
extern void IconWin_SetIcon(s32 icon);
extern void Voice_StopWithLip(void);
extern void Save_AddItem(s32 idx);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage,
                                s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *bits);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  void *items);
extern void BattleSetup_SetPoolMember(s32 count, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel,
                                      f32 health, void *items);

#define S_VOICE_IDLE 5          /* Voice_GetStat result when nothing is playing */
#define S_VOICE_BASE 0x8765     /* voice bank base of this mode group's guides (Voice_PlayWithSubtitle) */
#define S_MONEY_MAX 9999999

/* ---- gProgress and gSaveData as this chunk uses them ---- */

typedef struct SProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;      /* MPROG_ */
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x624];
    /* 0x640 */ s32 cursor;     /* row the last list screen of the group was left on: here the survival course */
    /* 0x644 */ s32 teamSize;   /* fighters the player chooses in the select that follows (>= 2 = team select) */
    /* 0x648 */ s32 dpRule;     /* 0 = no DP limit */
} SProgress;

#define S_PROG ((SProgress *)gProgress)

/* Best result of one survival course (UbSaveBestB of include/menu/menu_p.h, written by UbScore_SaveBestB). */
typedef struct SSaveSurv {
    /* 0x00 */ s32 defeated;    /* opponents beaten: the score sheet's fourth line */
    /* 0x04 */ s32 score;       /* shown * 100 */
    /* 0x08 */ u8 rank;
    /* 0x09 */ u8 hours;
    /* 0x0A */ u8 minutes;
    /* 0x0B */ u8 seconds;
} SSaveSurv; /* 0xC */

#define SURV_COURSE_NUM 3       /* rows of the course select (the save has room for five records) */

/*
 * The save, flat (include/sys/save.h). The nested form of include/menu/menu_c.h does not reproduce these screens'
 * address arithmetic: 0x73C splits into 0x730 (added to the index) + 0xC, as in include/menu/menu_o.h.
 */
typedef struct SSave {
    /* 0x0000 */ u8 unk0[0x208];
    /* 0x0208 */ s32 ubFlags;           /* S_UBFLAG_ */
    /* 0x020C */ u8 unk20C[0x530];
    /* 0x073C */ SSaveSurv surv[5];
    /* 0x0778 */ u8 unk778[0x28B0];
    /* 0x3028 */ s32 money;             /* Z points */
} SSave;

extern SSave *gSaveData;

#define S_SAVE gSaveData
#define S_UBFLAG_SURV_ITEM 8    /* the reward item for ten opponents beaten in a survival run was given */

/* ---- UbScore (src/menu/menu_p_d.c, menu_q.c): the score sheet behind the result screens ---- */

typedef struct SScoreLine {
    /* 0x00 */ s32 value;       /* what was measured (a bonus line: the bonus id) */
    /* 0x04 */ s32 remain;
    /* 0x08 */ s32 points;      /* shown * 100 */
} SScoreLine; /* 0xC */

typedef struct SScore {
    /* 0x000 */ s32 total;
    /* 0x004 */ s32 count;      /* the total being counted up */
    /* 0x008 */ s32 convert;    /* points still to be turned into Z points */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ SScoreLine line[4];   /* line[3].value: BattleResult + 0x18, the opponents beaten in a survival run */
    /* 0x040 */ SScoreLine bonus[48];
    /* 0x280 */ s32 shown[3];   /* bonus indices on the page shown */
    /* 0x28C */ s32 lineCount;
    /* 0x290 */ s32 bonusCount;
    /* 0x294 */ u32 ticks;
    /* 0x298 */ s16 hours;
    /* 0x29A */ s16 minutes;
    /* 0x29C */ s16 seconds;
    /* 0x29E */ s16 unk29E[3];
    /* 0x2A4 */ s32 rank;       /* 0..4, 4 the best */
} SScore; /* 0x2A8 */

extern s32 UbScore_Fill(s32 kind, SScore *score, s32 *pages);
extern void UbScore_CalcPoints(void *price, SScore *score);
extern s32 UbScore_CalcRank(s32 kind, SScore *score);   /* sets score->rank, returns the total */
extern s32 UbScore_Transfer(s32 *from, s32 *to, s32 step, f32 rate);
extern s32 UbScore_CountLine(SScore *score, s32 isBonus, s32 index, s32 step);
extern s32 UbScore_ConvertStep(SScore *score, s32 step);
extern void UbScore_SetPage(SScore *score, s32 page);
extern void UbScore_PlateGoto(MFlash *flash, s32 kind, s32 on);
extern s32 UbScore_SaveBestB(s32 course, s32 total, SScore *score); /* 1 = a new record was written */
extern s32 UbScore_GetRewardItem(s32 idx);  /* gUbRewardItems[idx]; idx 3 = item 0x6A */

/* ---- SimTop: the former menu_s.c is merged into src/menu/menu_r_d.c; its view is in include/menu/menu_r.h ---- */

/* ---- SurvSel (menu_s_b.c) ---- */

#define SURV_NONE 0x3E7         /* empty slot in the tables */
#define SURV_RANDOM 0x3E6
#define SURV_OPP_NUM 50         /* opponents of a course */
#define SURV_RANDOM_NUM 0x66    /* entries of the random character list */
#define SURVSEL_FLASH_NUM 1
#define SURVSEL_IDLE_FRAMES 0x708

/* Section 12: one course (0xE0 bytes). The head is MisSelDef's (include/menu/menu_o.h). */
typedef struct SurvCourse {
    /* 0x00 */ s32 announcer;   /* BattleSetup_SetRule's fifth argument; SURV_RANDOM = Rand_Range(8) */
    /* 0x04 */ s32 unk4;        /* non-zero: BattleSetup_SetRule's last argument is 1 */
    /* 0x08 */ s32 timeLimit;
    /* 0x0C */ s32 stage;       /* SURV_RANDOM = Rand_Range(35) */
    /* 0x10 */ s32 bgm;         /* SURV_RANDOM = 0x18, the "random" music id */
    /* 0x14 */ s32 unk14;       /* non-zero: side 1's unk1FC = 1 */
    /* 0x18 */ s32 opp[SURV_OPP_NUM]; /* indices into section 13 */
} SurvCourse; /* 0xE0 */

/* Section 13: one opponent (0x2C bytes; MisSelOpp of include/menu/menu_o.h). */
typedef struct SurvOpp {
    /* 0x00 */ s32 chara;       /* SURV_RANDOM = drawn from section 14 */
    /* 0x04 */ s32 costume;
    /* 0x08 */ s32 cpuLevel;
    /* 0x0C */ s32 item7;       /* 0-based id of the item in slot 7 (the AI item), SURV_NONE = none */
    /* 0x10 */ s32 item[7];     /* 0-based ids packed into slots 0.., SURV_NONE = none */
} SurvOpp; /* 0x2C */

typedef struct SurvSel {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 3 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 19 sections */
    /* 0x008 */ void *text;         /* section 18 */
    /* 0x00C */ void *subtitles;    /* section 17 */
    /* 0x010 */ MTextBox box[4];    /* 0..2 course names, 3 the headline */
    /* 0x240 */ MFlash flash[SURVSEL_FLASH_NUM];
    /* 0x26C */ void *bg;           /* section 4 */
    /* 0x270 */ u8 *tex[29];
    /* 0x2E4 */ SurvCourse *courses; /* section 12 */
    /* 0x2E8 */ s32 *randomChara;   /* section 14: SURV_RANDOM_NUM character ids */
    /* 0x2EC */ SurvOpp *opps;      /* section 13 */
    /* 0x2F0 */ s32 flags;          /* SURVSEL_ */
    /* 0x2F4 */ s32 cur[2];         /* cursor of each level; only cur[0] (course 0..2) is used */
    /* 0x2FC */ s32 timer;          /* frames until the fade out starts after the choice */
    /* 0x300 */ s32 unk300[2];
    /* 0x308 */ s32 level;          /* 0 = choosing, 1 = the course's window is open */
    /* 0x30C */ s32 voiceReq;       /* line the guide is asked to say (1 greeting, 2 idle), 0 = none */
    /* 0x310 */ s32 voiceLast;
    /* 0x314 */ s32 voiceSkip;
    /* 0x318 */ s32 voiceLine;      /* subtitle line, -1 = none */
    /* 0x31C */ s32 unk31C;
    /* 0x320 */ s32 blink;
    /* 0x324 */ s32 talk;
    /* 0x328 */ s32 idle;           /* frames without input */
} SurvSel; /* 0x32C */

#define SURVSEL_CHOSEN 1
#define SURVSEL_LEAVING 2
#define SURVSEL_STARTED 4
#define SURVSEL_GREETED 8

extern SurvSel *gSurvSel;   /* 0x3B743C */

void SurvSel_SetupBattle(void);
void SurvSel_Init(s32 section);
void SurvSel_Term(void);
void SurvSel_Draw(void);
void SurvSel_Update(void);
void SurvSel_UpdateVoice(void);
void SurvSel_Input(s32 *result);
void SurvSel_ClipGoto(s32 movie, s32 level, char *label);
s32 SurvSel_Run(s32 section);

/* ---- SurvResult (menu_s_c.c) ---- */

#define SURVRESULT_FLASH_NUM 1

typedef struct SurvResult {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 3 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 15 sections */
    /* 0x008 */ s32 unk8;
    /* 0x00C */ void *text;         /* section 12 */
    /* 0x010 */ void *subtitles;    /* section 11 */
    /* 0x014 */ void *itemText;     /* section 14: item names */
    /* 0x018 */ MTextBox box[6];    /* 0..2 bonus plates, 3 the headline */
    /* 0x360 */ MTextBox itemBox;
    /* 0x3EC */ void *price;        /* section 13: UbScorePrice table */
    /* 0x3F0 */ MFlash flash[SURVRESULT_FLASH_NUM];
    /* 0x41C */ void *bg;           /* section 5 */
    /* 0x420 */ u8 *tex[43];
    /* 0x4CC */ s32 flags;          /* SURVRESULT_ */
    /* 0x4D0 */ s32 unk4D0[12];
    /* 0x500 */ s32 timer;
    /* 0x504 */ s32 unk504[2];
    /* 0x50C */ s32 state;          /* SURVRESULT_ST_ */
    /* 0x510 */ s32 talk;           /* step of the guide's script, 0 = none */
    /* 0x514 */ s32 talkLast;
    /* 0x518 */ s32 voiceSkip;
    /* 0x51C */ s32 voiceLine;      /* subtitle line, -1 = none */
    /* 0x520 */ s32 unk520;
    /* 0x524 */ s32 blink;
    /* 0x528 */ s32 mouth;
    /* 0x52C */ s32 pages;          /* pages of bonus plates */
    /* 0x530 */ s32 page;           /* 0 = the score lines, 1.. = bonus pages */
    /* 0x534 */ s32 outcome;        /* UbScore_Fill: 0 won, 1 lost, 2 aborted */
    /* 0x538 */ s32 newRecord;
    /* 0x53C */ s32 gotItem;        /* the reward item is given on this visit */
    /* 0x540 */ u8 pageDone;        /* every line of the page was counted */
    /* 0x541 */ u8 unk541[3];
    /* 0x544 */ SScore score;
    /* 0x7EC */ s32 counting;
    /* 0x7F0 */ s32 step;           /* line being counted; in the money state its sub step */
    /* 0x7F4 */ s32 started;        /* the movie's intro has ended */
} SurvResult; /* 0x7F8 */

#define SURVRESULT_DONE 1
#define SURVRESULT_LEAVING 2
#define SURVRESULT_STARTED 4
#define SURVRESULT_GREETED 8

#define SURVRESULT_ST_INTRO 0
#define SURVRESULT_ST_COUNT 1    /* the lines count into the total */
#define SURVRESULT_ST_PAGES 2    /* the pages can be turned; confirm goes on */
#define SURVRESULT_ST_MONEY 3    /* the total is paid out */
#define SURVRESULT_ST_ITEM 4     /* the reward window */
#define SURVRESULT_ST_LEAVE 5

extern SurvResult *gSurvResult;  /* 0x3B7440 */

void SurvResult_Init(s32 section);
void SurvResult_Term(void);
void SurvResult_Draw(void);
void SurvResult_Update(void);
void SurvResult_UpdateVoice(void);
void SurvResult_Input(s32 *result);
s32 SurvResult_Run(s32 section);

/* ---- SimEvent handlers (menu_s_d.c): merged with menu_t.c; SimTrainTbl, the outcome globals and the
 * prototypes of SimEv00..02 are in include/menu/menu_t.h now (the SimDayS view is replaced by TSimDay). ---- */

#endif
