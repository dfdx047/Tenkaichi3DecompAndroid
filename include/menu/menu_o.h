#ifndef MENU_MENU_O_H
#define MENU_MENU_O_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x376920..0x37AFF8 (placeholder stem "menu_o"): the mode group 13..30 (main-menu item 1,
 * archive gMenuArc3). Four pieces, cut where the read-only data says a new object starts (the string
 * "mc_guide_17go" exists three times in this range):
 *
 *   (menu_o.c)  0x376920..0x3782A8  UbResult    tail of the result screen of modes 27 and 30 (work pointer
 *                                               0x3B7358): merged into src/menu/menu_n_d.c, which has the
 *                                               object's head (0x3760C8, Init at 0x376150)
 *   menu_o_b.c  0x3782A8..0x379908  DiscFusion  the disc-swap screen of mode 24 (work pointer 0x3B735C)
 *   menu_o_c.c  0x379908..0x379F58  Ub_Main     the handler of modes 13..30 and its three battle hand-offs
 *   menu_o_d.c  0x379F58..0x37B7C0  MisSel      the mission list of mode 14 (work pointer 0x3B7360); merged with
 *                                               its tail (0x37AFF8.., formerly menu_p.c)
 *
 * All names are guesses from what the code does unless the symbol file says otherwise. The structures are this
 * chunk's own views.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern s32 Rand_Libc(void);     /* the C library rand(), not the shared Mersenne Twister */
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetMaxWidth(MTextBox *box, s32 w);
extern void TextBox_SetLineOffsets(MTextBox *box, s32 a, s32 b, s32 c, s32 d, s32 e);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h,
                          s32 mode, s32 parentFmt);
extern void IconWin_SetIcon(s32 icon);
extern void Save_AddItem(s32 idx);
extern void Dialog_Draw(s32 visible);
extern void Dialog_Start(s32 cmd);
extern s32 Dialog_Input(s32 allowCancel);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetMsg(s32 idx);
extern void Dialog_SetCursor(s32 choice);
extern s32 Dialog_IsClosed(void);
extern void Voice_StopWithLip(void);
extern s32 Disc_Identify(void);       /* which of the three known boot files the disc in the drive has */
extern s32 Disc_GetDriveState(void);  /* DISC_ */
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage,
                                s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *bits);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  void *items);
extern void BattleSetup_Finish(void);

/* Disc_GetDriveState results this chunk tests (sceCdStatus 0 / 1 / 10; 7 = the disc type reads 0) */
#define DISC_STOPPED 0
#define DISC_TRAY_OPEN 1
#define DISC_PAUSED 4
#define DISC_NO_TYPE 7

#define UB_BGM 0x10B1B          /* Bgm_Play id of every screen of the group */
#define UB_VOICE_BASE_O 0x8765  /* voice bank base of the group's guide (Voice_PlayWithSubtitle) */
#define UB_VOICE_IDLE_O 5       /* Voice_GetStat result when nothing is playing */

/* ---- gProgress and gSaveData as this chunk uses them ---- */

/* What was chosen for one fighter (TsMember of include/menu/menu_e.h). */
typedef struct UoMember {
    /* 0x00 */ s32 unk0[6];
    /* 0x18 */ s32 color;       /* costume */
    /* 0x1C */ s32 chara;       /* character id */
    /* 0x20 */ u16 items[8];    /* equipped items (a BattleItemSet) */
} UoMember; /* 0x30 */

typedef struct UoProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ s32 unk8[3];
    /* 0x014 */ s32 flags;      /* MPROG_ */
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x424];
    /* 0x440 */ UoMember team[5]; /* the player's fighters, written by SoloSel / UbTeamSel */
    /* 0x530 */ u8 unk530[0x10C];
    /* 0x63C */ s32 missionPage; /* page of the mission list */
    /* 0x640 */ s32 cursor;     /* cursor the last list screen was left on: disc 0 / 1 after DiscFusion (kept as the
                                   "which disc mode" of modes 25..30), row 0..4 of the mission list */
    /* 0x644 */ s32 teamSize;   /* fighters the player chooses (>= 2 runs UbTeamSel) */
    /* 0x648 */ s32 dpRule;     /* 0 = no DP limit, 1 / 2 / 3 = limit 10 / 15 / 20 */
    /* 0x64C */ u8 unk64C[0x28];
    /* 0x674 */ s32 unk674;     /* counted up each time mode 23 goes back to mode 22 */
    /* 0x678 */ s32 unk678[3];
    /* 0x684 */ s32 discFlags;  /* UB_DISC_; bit 8 is read by the result screen (menu_n.h: ubFlags, where bit 8 is
                                   NPROG_UB_UPWARD: the place challenged on the ladder is above the player's) */
    /* 0x688 */ s32 course;     /* copied to UbResult.course when cursor == 1 (menu_n.h: ubChoice) */
} UoProgress;

#define UO_PROG ((UoProgress *)gProgress)

#define UB_DISC_0 1             /* disc 0 was recognised this session */
#define UB_DISC_1 2             /* disc 1 was recognised this session */
#define UB_DISC_FLAG8 8

/* One mission's record in the save (12 bytes). */
typedef struct UoMissionRec {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 rank;
    /* 0x2 */ u8 time0;         /* best time, shown as three two-digit groups (separate members, not an array:
                                   the code's address arithmetic shows it) */
    /* 0x3 */ u8 time1;
    /* 0x4 */ u8 time2;
    /* 0x5 */ u8 unk5[3];
    /* 0x8 */ s32 score;        /* shown * 100 */
} UoMissionRec;

/* Best result of one course (NCourseRec of include/menu/menu_n.h). */
typedef struct UoCourseRec {
    /* 0x0 */ u8 cleared;
    /* 0x1 */ u8 rank;
    /* 0x2 */ u8 time[3];
    /* 0x5 */ u8 unk5[3];
    /* 0x8 */ s32 score;
} UoCourseRec; /* 0xC */

#define UO_COURSE_NUM 5

/*
 * gSaveData as this chunk reads it: one flat structure. (menu_c.h's view nests a body behind the 8-byte checksum;
 * here the flat form is the one that reproduces the address arithmetic: 0x28C splits into 0x280 + 0xC.)
 */
typedef struct UoSave {
    /* 0x0000 */ u8 unk0[0x208];
    /* 0x0208 */ s32 unk208;            /* bit 0x10: the reward for clearing all five courses was given */
    /* 0x020C */ s32 missionPages;      /* pages of five missions the list shows */
    /* 0x0210 */ u8 unk210[0x7C];
    /* 0x028C */ UoMissionRec mission[100];
    /* 0x073C */ u8 unk73C[0x40];
    /* 0x077C */ s32 rank;              /* the player's place on the ladder (menu_n.h: 99 by default, 0 = first) */
    /* 0x0780 */ UoCourseRec course[UO_COURSE_NUM];
    /* 0x07BC */ u8 unk7BC[0x286C];
    /* 0x3028 */ s32 money;             /* Z points */
} UoSave;

extern UoSave *gSaveData;

#define UO_SAVE gSaveData

#define UO_MONEY_MAX 9999999

/* ---- UbResult (menu_o.c): result screen of the two disc modes ---- */

/* The score sheet (filled by UbScore_Fill / 0037F008 / 0037F180 of the next chunk). */
typedef struct UoScoreRow {
    /* 0x0 */ s32 value;        /* base rows: a percentage; bonus rows: text line - 0x20 */
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 pt;           /* points / 100 */
} UoScoreRow; /* 0xC */

typedef struct UoScore {
    /* 0x000 */ s32 score;      /* total, shown * 100 */
    /* 0x004 */ s32 shown;      /* counter that runs while the score is converted */
    /* 0x008 */ s32 points;     /* Z points still to pay into the save */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ UoScoreRow base[4];
    /* 0x040 */ UoScoreRow bonus[48];
    /* 0x280 */ s32 row[3];     /* bonus rows shown on the current page */
    /* 0x28C */ s32 baseCount;
    /* 0x290 */ s32 bonusCount;
    /* 0x294 */ s32 unk294;
    /* 0x298 */ s16 time[3];
    /* 0x29E */ s16 unk29E;
    /* 0x2A0 */ s32 unk2A0;
    /* 0x2A4 */ s32 rank;       /* 0..4, 4 the best (guess) */
} UoScore; /* 0x2A8 */

typedef struct UbResult {
    /* 0x000 */ u32 *pack;
    /* 0x004 */ u32 *res;
    /* 0x008 */ s32 unk8;
    /* 0x00C */ void *text;         /* section 12 */
    /* 0x010 */ void *subtitles;    /* section 11 */
    /* 0x014 */ void *itemText;     /* section 14 */
    /* 0x018 */ MTextBox box[4];    /* 0..2 bonus rows, 3 the headline */
    /* 0x248 */ MTextBox itemBox;
    /* 0x2D4 */ u8 unk2D4[0x8C];
    /* 0x360 */ void *bonusTbl;     /* section 13 */
    /* 0x364 */ MFlash flash[1];
    /* 0x390 */ void *bg;
    /* 0x394 */ u8 *tex[47];
    /* 0x450 */ s32 flags;          /* UBRES_ */
    /* 0x454 */ s32 unk454[12];
    /* 0x484 */ s32 timer;
    /* 0x488 */ s32 unk488[2];
    /* 0x490 */ s32 step;           /* UBRES_STEP_ */
    /* 0x494 */ s32 talkStep;       /* guide speech state, 0 = silent */
    /* 0x498 */ s32 talkPrev;
    /* 0x49C */ s32 skip;           /* the current line was skipped with the confirm button */
    /* 0x4A0 */ s32 voiceLine;      /* -1 = none */
    /* 0x4A4 */ s32 unk4A4;
    /* 0x4A8 */ s32 blink;
    /* 0x4AC */ s32 talk;
    /* 0x4B0 */ s32 pageCount;      /* bonus pages */
    /* 0x4B4 */ s32 page;           /* 0 = the base sheet */
    /* 0x4B8 */ s32 lose;           /* 0 won, 2 leave at once */
    /* 0x4BC */ s32 newRecord;
    /* 0x4C0 */ s32 gotItem;
    /* 0x4C4 */ u8 pageDone;        /* the rows of the page were all counted up */
    /* 0x4C5 */ u8 unk4C5[3];
    /* 0x4C8 */ s32 kind;           /* gProgress->cursor: which of the two disc modes */
    /* 0x4CC */ s32 course;
    /* 0x4D0 */ UoScore score;
    /* 0x778 */ s32 counting;       /* rows are being counted up */
    /* 0x77C */ s32 count;          /* rows counted so far; sub step while the points are paid */
    /* 0x780 */ s32 ready;          /* the movie reached its first trigger */
} UbResult; /* 0x784 */

#define UBRES_DONE 1
#define UBRES_LEAVING 2
#define UBRES_STARTED 4
#define UBRES_FADED_IN 8

#define UBRES_STEP_INTRO 0
#define UBRES_STEP_COUNT 1      /* rows count up */
#define UBRES_STEP_SHEET 2      /* pages can be turned */
#define UBRES_STEP_PAY 3        /* the score becomes Z points */
#define UBRES_STEP_ITEM 4       /* the item window */
#define UBRES_STEP_LEAVE 5

extern UbResult *gUbResult;     /* 0x3B7358 */

/* next chunk (menu_p): the score sheet */
extern s32 UbScore_Transfer(s32 *from, s32 *to, s32 step, f32 rate);
extern s32 UbScore_CountLine(UoScore *score, s32 bonus, s32 row, s32 step);
extern s32 UbScore_ConvertStep(UoScore *score, s32 step);
extern void UbScore_SetPage(UoScore *score, s32 page);
extern void UbScore_PlateGoto(MFlash *flash, s32 which, s32 on);
extern s32 UbScore_GetRewardItem(s32 kind);
extern s32 UbScore_Fill(s32 kind, UoScore *score, s32 *pageCount);
extern void UbScore_CalcPoints(void *bonusTbl, UoScore *score);
extern s32 UbScore_CalcRank(s32 kind, UoScore *score);
extern s32 UbScore_SaveBestC(s32 course, s32 rank, UoScore *score);

s32 UbResult_MarkCourseCleared(s32 course);
void UbResult_Init(s32 section);
void UbResult_Term(void);
void UbResult_Draw(void);
void UbResult_Update(void);
void UbResult_UpdateTalk(void);
void UbResult_Input(s32 *result);
s32 UbResult_Run(s32 section);

/* ---- DiscFusion (menu_o_b.c) ---- */

#define DISCFUSION_FLASH_NUM 2
#define DISCFUSION_DISC_NUM 2

typedef struct DiscFusion {
    /* 0x000 */ u32 *pack;
    /* 0x004 */ u32 *res;
    /* 0x008 */ void *msgText;      /* section 10 */
    /* 0x00C */ void *subtitles;    /* section 11 */
    /* 0x010 */ void *text;         /* section 12: plate text and the dialog's messages */
    /* 0x014 */ MFlash flash[DISCFUSION_FLASH_NUM]; /* 0 the screen, 1 the "recognised" movie */
    /* 0x06C */ void *bg;
    /* 0x070 */ u8 *texA[18];
    /* 0x0B8 */ u8 *texB[4];
    /* 0x0C8 */ MTextBox box[DISCFUSION_DISC_NUM];
    /* 0x1E0 */ s32 flags;          /* UBRES_ (same bits) */
    /* 0x1E4 */ s32 cursor[1];      /* indexed by level, which is always 0 */
    /* 0x1E8 */ s32 timer;
    /* 0x1EC */ s32 unk1EC[2];
    /* 0x1F4 */ s32 level;
    /* 0x1F8 */ s32 talkStep;
    /* 0x1FC */ s32 talkPrev;
    /* 0x200 */ s32 skip;
    /* 0x204 */ s32 dlgStep;        /* DISCFUSION_DLG_, 0 = no disc swap running */
    /* 0x208 */ s32 voiceLine;
    /* 0x20C */ s32 unk20C;
    /* 0x210 */ s32 blink[2];
    /* 0x218 */ s32 talk[2];
    /* 0x220 */ u8 unk220[0x3C];    /* handed to MsgWin_Init as its unused fourth argument */
    /* 0x25C */ s32 have[DISCFUSION_DISC_NUM]; /* the disc was recognised (gProgress->discFlags) */
    /* 0x264 */ s32 chosen;         /* disc being asked for */
    /* 0x268 */ s32 unk268;
    /* 0x26C */ s32 tries;
    /* 0x270 */ s32 idle;           /* frames without input */
    /* 0x274 */ s32 toggle;         /* flips every frame: the icon flickers */
    /* 0x278 */ s32 wait;           /* frames until the disc is looked at again */
    /* 0x27C */ s32 timeout;
    /* 0x280 */ s32 error;          /* 1 = the drive never reported a disc type */
} DiscFusion; /* 0x284 */

/* dlgStep */
#define DISCFUSION_DLG_ASK 1
#define DISCFUSION_DLG_CONFIRM 2
#define DISCFUSION_DLG_OPEN 3
#define DISCFUSION_DLG_INSERT 4
#define DISCFUSION_DLG_READ 5
#define DISCFUSION_DLG_FOUND 6
#define DISCFUSION_DLG_FOUND_ANIM 7
#define DISCFUSION_DLG_BACK_OPEN 8
#define DISCFUSION_DLG_BACK_INSERT 9
#define DISCFUSION_DLG_BACK_READ 10
#define DISCFUSION_DLG_WRONG 11
#define DISCFUSION_DLG_WRONG_OPEN 12
#define DISCFUSION_DLG_BACK_WRONG 13
#define DISCFUSION_DLG_CLOSE 14

#define DISCFUSION_ID_BUSY 0x63   /* DiscFusion_Identify: the drive is not ready */
#define DISCFUSION_ID_OWN 2       /* this game's own disc */
#define DISCFUSION_TRIES 0x40
#define DISCFUSION_TIMEOUT 0xF0
#define DISCFUSION_IDLE 0x708

extern DiscFusion *gDiscFusion; /* 0x3B735C */

void DiscFusion_UpdateHave(void);
s32 DiscFusion_Identify(void);
void DiscFusion_Init(s32 section);
void DiscFusion_Term(void);
void DiscFusion_Draw(void);
void DiscFusion_Update(void);
void DiscFusion_UpdateDisc(void);
void DiscFusion_UpdateTalk(void);
void DiscFusion_Input(s32 *result);
void DiscFusion_PlateGoto(s32 movie, s32 level, char *label);
s32 DiscFusion_Run(s32 section);

/* ---- Ub_Main (menu_o_c.c) ---- */

void Ub_SetupTeam(void);
void Ub_SetupSolo(void);
void Ub_SetupSolo2(void);
s32 Ub_Main(void);

/* ---- MisSel (menu_o_d.c) ---- */

#define MISSEL_NONE 0x3E7    /* list terminator / empty slot in the mission tables */
#define MISSEL_RANDOM 0x3E6
#define MISSEL_ROWS 5
#define MISSEL_CHIP_NUM 165
#define MISSEL_FLASH_NUM 1

/* Section 10: one mission (0x34 bytes). */
typedef struct MisSelDef {
    /* 0x00 */ s32 announcer;   /* BattleSetup_SetRule's fifth argument; MISSEL_RANDOM = Rand_Range(8) */
    /* 0x04 */ s32 unk4;        /* non-zero: rule.unk10 = 1 */
    /* 0x08 */ s32 timeLimit;
    /* 0x0C */ s32 stage;       /* MISSEL_RANDOM = Rand_Range(35) */
    /* 0x10 */ s32 bgm;         /* MISSEL_RANDOM = 0x18, the "random" music id */
    /* 0x14 */ s32 unk14;       /* non-zero: side 1's unk1FC = 1 */
    /* 0x18 */ s32 kind;        /* low u16: type 0..10; BT4 type 9 high u16: battle script id */
    /* 0x1C */ s32 dpRule;      /* text line 0x156 + dpRule when non-zero */
    /* 0x20 */ s32 opp[5];      /* indices into section 11 */
} MisSelDef; /* 0x34 */

/* Section 11: one opponent (0x2C bytes). */
typedef struct MisSelOpp {
    /* 0x00 */ s32 chara;       /* MISSEL_NONE ends the team */
    /* 0x04 */ s32 costume;
    /* 0x08 */ s32 cpuLevel;
    /* 0x0C */ s32 item7;       /* 0-based id of the item in slot 7 (the AI item), MISSEL_NONE = none */
    /* 0x10 */ s32 item[7];     /* 0-based ids packed into slots 0.., MISSEL_NONE = none */
} MisSelOpp; /* 0x2C */

typedef struct MisSel {
    /* 0x000 */ u32 *pack;
    /* 0x004 */ u32 *res;
    /* 0x008 */ u32 *chips;         /* section 15: 165 texture lists, section id + 1 = chip of a character */
    /* 0x00C */ void *text;         /* section 18 */
    /* 0x010 */ void *subtitles;    /* section 17 */
    /* 0x014 */ MTextBox box[7];    /* 0..4 mission names, 5 team kind, 6 DP rule */
    /* 0x3E8 */ MisSelDef *missions; /* section 10 */
    /* 0x3EC */ MisSelOpp *opps; /* section 11 */
    /* 0x3F0 */ MFlash flash[MISSEL_FLASH_NUM];
    /* 0x41C */ void *bg;
    /* 0x420 */ u8 *tex[39];
    /* 0x4BC */ s32 flags;          /* MISSEL_ */
    /* 0x4C0 */ s32 cursor[4];      /* cursor of each level; only [0] (row 0..4) is used */
    /* 0x4D0 */ s32 timer;          /* frames until the fade out starts after the choice */
    /* 0x4D4 */ s32 unk4D4[2];
    /* 0x4DC */ s32 level;          /* 0 = choosing, 1 = the mission's window is open */
    /* 0x4E0 */ s32 voiceReq;       /* line the guide is asked to say (1 greeting, 2 idle), 0 = none */
    /* 0x4E4 */ s32 voiceLast;
    /* 0x4E8 */ s32 voiceSkip;      /* confirm was pressed while the guide spoke */
    /* 0x4EC */ s32 voiceLine;      /* subtitle line, -1 = none */
    /* 0x4F0 */ s32 unk4F0;
    /* 0x4F4 */ s32 blink;
    /* 0x4F8 */ s32 talk;
    /* 0x4FC */ s32 pageCount;      /* pages available (five missions each) */
    /* 0x500 */ s32 page;
    /* 0x504 */ s32 mission;        /* mission under the cursor: page * 5 + row */
    /* 0x508 */ s32 idle;           /* frames without input */
} MisSel; /* 0x50C */

#define MISSEL_CHOSEN 1
#define MISSEL_LEAVING 2
#define MISSEL_STARTED 4
#define MISSEL_GREETED 8
#define MISSEL_IDLE_FRAMES 0x708

extern MisSel *gMisSel;   /* 0x3B7360 */
extern s32 gMisSelFaceTex[MISSEL_ROWS]; /* 0x3B7368: {19, 22, 23, 24, 25}, tex slots of the opponents' chips */

void MisSel_SetRank(s32 row, s32 rank);
void MisSel_SetupBattle(void);
void MisSel_Init(s32 section);
void MisSel_Term(void);
void MisSel_Draw(void);
void MisSel_Update(void);
void MisSel_UpdateVoice(void);
void MisSel_Input(s32 *result);
void MisSel_ClipGoto(s32 movie, s32 level, char *label);
s32 MisSel_Run(s32 section);

#endif
