#ifndef MENU_MENU_N_H
#define MENU_MENU_N_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x372148..0x376920 (placeholder stem "menu_n"): screens of the mode group 13..30
 * (handler 0x379A58, main-menu item 1). Four pieces, cut at object boundaries:
 *
 *   (menu_n.c)  0x372148..0x372560  UbTeamSel  tail of the team select of mode 15: merged into
 *                                              src/menu/menu_m_b.c (the object starts at 0x36E028)
 *   menu_n_b.c  0x372560..0x373A68  UbzSel     mode 28: the course select (five courses of up to eight opponents
 *                                              fought in a row) and the first half of its battle set-up
 *   menu_n_c.c  0x373A68..0x3760C8  UbRank     mode 26: the ranking ladder of 100 places (challenge a place above,
 *                                              intruders) and its battle set-up
 *   menu_n_d.c  0x3760C8..0x3782A8  UbResult   the result screen of modes 27 / 30 (Run is 0x378138); merged with
 *                                              its tail (menu_o.c) and built with include/menu/menu_o.h, not with
 *                                              this header
 *
 * All names are guesses from what the code does. (inferred, from the game itself: modes 24..30 are the two modes
 * unlocked by "Disc Fusion": "Ultimate Battle" = the ranking ladder, "Ultimate Battle Z" = the courses.)
 * The structures are this chunk's own views.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern s32 Rand_Libc(void);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetLineOffsets(MTextBox *box, s32 y1, s32 y2, s32 y3, s32 y4, s32 y5);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h,
                          s32 mode, s32 parentFmt);
extern void Flash_SetOffset(MFlash *flash, s32 x, s32 y);
extern void Flash_SetFlag(MFlash *flash, u32 mask, u8 on);
extern void IconWin_SetIcon(s32 icon);
extern void Save_AddItem(s32 idx);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *charaBits);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  u16 *items);
extern void BattleSetup_SetPoolMember(s32 count, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel,
                                      f32 health, u16 *items);
extern void BattleSetup_Finish(void);

#define N_VOICE_IDLE 5          /* Voice_GetStat result when nothing is playing */
#define N_VOICE_BASE 0x8765     /* voice bank base of this mode group's guides (Voice_PlayWithSubtitle) */
#define N_FACE_FILE DISC_RESOURCE(0x2F9, 0x71C)       /* + character id: compressed portrait */
#define N_CHIP_NUM 165          /* texture lists in a chip pack: section id + 1 = chip of a character */
#define N_NONE 999              /* "no entry" in the opponent / rule tables */
#define N_RANDOM 998            /* "draw one" in the rule tables */
#define N_BGM_RANDOM 0x18       /* music id the battle resolves at random */
#define N_IDLE_FRAMES 0x708     /* 1800 frames without input: the guide speaks */

/* ---- gProgress as these screens use it ---- */

typedef struct NProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ u8 unk8[0xC];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x43C];
    /* 0x458 */ s32 color;      /* costume of the player's fighter (team.member[0].color of menu_m.h) */
    /* 0x45C */ s32 chara;      /* the player's fighter (team.member[0].chara) */
    /* 0x460 */ u16 items[8];   /* its items (team.member[0].items) */
    /* 0x470 */ u8 unk470[0x1D0];
    /* 0x640 */ s32 ubKind;     /* 0 = the ranking ladder (modes 25..27), 1 = the courses (modes 28..30) */
    /* 0x644 */ s32 teamSize;   /* fighters the character select lets the player choose */
    /* 0x648 */ s32 dpRule;     /* 0 = no DP limit */
    /* 0x64C */ u8 unk64C[0x38];
    /* 0x684 */ s32 ubFlags;    /* NPROG_UB_ */
    /* 0x688 */ s32 ubChoice;   /* the course chosen (0..4) / the place challenged (0 = first place) */
} NProgress;

#define NPROG ((NProgress *)gProgress)

#define NPROG_UB_FOUGHT 4       /* a ladder battle was started: the ladder shows its outcome when it comes back */
#define NPROG_UB_UPWARD 8       /* the place challenged is above the player's own (a win moves the player up) */
#define NPROG_UB_INTRUDER 0x10  /* the opponent is an intruder */

/* ---- gSaveData as these screens use it (include/sys/save.h has the full layout) ---- */

/* Best result of one course. */
typedef struct NCourseRec {
    /* 0x0 */ u8 cleared;
    /* 0x1 */ u8 rank;          /* frame of the rank letter strip */
    /* 0x2 */ u8 time[3];       /* best time: three two-digit groups, most significant first */
    /* 0x5 */ u8 unk5[3];
    /* 0x8 */ s32 score;        /* best score / 100 */
} NCourseRec; /* 0xC */

#define N_COURSE_NUM 5

typedef struct NSave {
    /* 0x000 */ u8 unk0[0x208];
    /* 0x208 */ s32 unk208;     /* bit 0x10: the reward for clearing all five courses was given */
    /* 0x20C */ u8 unk20C[0x570];
    /* 0x77C */ s32 rank;       /* the player's place on the ladder: 99 (place 100) by default, 0 = first */
    /* 0x780 */ NCourseRec course[N_COURSE_NUM];
} NSave;

extern NSave *gSaveData;

/* ---- the opponent tables (in the screens' packs) ---- */

/* One opponent. */
typedef struct NFoe {
    /* 0x00 */ s32 chara;       /* character id, N_NONE = end of a course's list */
    /* 0x04 */ s32 color;       /* costume */
    /* 0x08 */ s32 cpuLevel;
    /* 0x0C */ s32 lastItem;    /* item id - 1 for slot 7, N_NONE = none */
    /* 0x10 */ s32 item[7];     /* item ids - 1, N_NONE = skipped (the rest move up) */
} NFoe; /* 0x2C */

/* The rules of one course. */
typedef struct NCourse {
    /* 0x00 */ s32 announcer;   /* N_RANDOM: Rand_Range(8) */
    /* 0x04 */ s32 unk4;        /* non-zero: BattleSetup_SetRule's last argument is 1 */
    /* 0x08 */ s32 timeLimit;
    /* 0x0C */ s32 stage;       /* N_RANDOM: Rand_Range(35) */
    /* 0x10 */ s32 bgm;         /* N_RANDOM: N_BGM_RANDOM */
    /* 0x14 */ s32 unk14;       /* non-zero: side 1's unk1FC is 1 */
    /* 0x18 */ s32 foe[8];      /* indices into the NFoe table */
} NCourse; /* 0x38 */

/* The rules of one place of the ladder (or of one intruder). */
typedef struct NRankRule {
    /* 0x00 */ s32 announcer;   /* N_RANDOM: Rand_Range(8) */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 timeLimit;
    /* 0x0C */ s32 stage;       /* N_RANDOM: Rand_Range(28) */
    /* 0x10 */ s32 bgm;         /* N_RANDOM: one of the eleven ids of the screen's music list */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 foe;         /* index into the NFoe table */
} NRankRule; /* 0x1C */

/* ---- UbzSel (menu_n_b.c) ---- */

#define UBZSEL_FLASH_NUM 1
#define UBZSEL_BOX_NUM 6

typedef struct UbzSel {
    /* 0x000 */ void *file;         /* file baseFile + 0x1C (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 10 sections */
    /* 0x008 */ u32 *chips;         /* section 6: N_CHIP_NUM texture lists */
    /* 0x00C */ void *text;         /* section 10: the course names */
    /* 0x010 */ void *subtitles;    /* never set: the guide's lines are played with a NULL subtitle table */
    /* 0x014 */ MFlash flash[UBZSEL_FLASH_NUM];
    /* 0x040 */ void *bg;           /* section 4: background picture */
    /* 0x044 */ u8 *tex[38];
    /* 0x0DC */ MTextBox box[UBZSEL_BOX_NUM]; /* one per course plate (five used) */
    /* 0x424 */ s32 flags;          /* UBZSEL_ */
    /* 0x428 */ s32 cursor[4];      /* indexed by step; only [0], the course, is used */
    /* 0x438 */ s32 timer;          /* frames until the fade out starts */
    /* 0x43C */ s32 unk43C[2];
    /* 0x444 */ s32 step;           /* 0 = choosing a course, 1 = the course's sheet is open */
    /* 0x448 */ s32 voiceReq;       /* line the guide is asked to say: 1 = greeting (never asked), 2 = idle */
    /* 0x44C */ s32 voicePrev;
    /* 0x450 */ s32 voiceSkip;      /* the line was cut short with the confirm button */
    /* 0x454 */ s32 voiceLine;      /* -1 = none yet */
    /* 0x458 */ s32 unk458;
    /* 0x45C */ s32 blink;
    /* 0x460 */ s32 talk;
    /* 0x464 */ s32 idle;           /* frames without input */
    /* 0x468 */ NCourse *course;    /* section 8 */
    /* 0x46C */ NFoe *foe;          /* section 9 */
} UbzSel; /* 0x470 */

#define UBZSEL_CHOSEN 1
#define UBZSEL_LEAVING 2
#define UBZSEL_STARTED 4        /* the cursor plate was lit once */
#define UBZSEL_FADED_IN 8

extern UbzSel *gUbzSel;         /* 0x3B7350 */

/* ---- UbRank (menu_n_c.c) ---- */

#define UBRANK_FLASH_NUM 3
#define UBRANK_BOX_NUM 6
#define UBRANK_ROWS 5           /* plates of the list the cursor can be on */
#define UBRANK_PLACES 100

typedef struct UbRank {
    /* 0x000 */ void *file;         /* file baseFile + 0x1B (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 18 sections */
    /* 0x008 */ u32 *chips;         /* section 6: N_CHIP_NUM texture lists */
    /* 0x00C */ void *faceFile[2];  /* 0x16800 bytes each: compressed portraits, [0] the player's, [1] the opponent's */
    /* 0x014 */ MTexRes *faceRes[2]; /* 0x20800 bytes each: the same unpacked */
    /* 0x01C */ void *text;         /* section 14: the names of the places' holders */
    /* 0x020 */ void *subtitles;    /* section 18 */
    /* 0x024 */ NRankRule *rule;    /* section 8: one per place, first place first */
    /* 0x028 */ NRankRule *intruderRule; /* section 9: 37 intruders */
    /* 0x02C */ NFoe *foe;          /* section 10 */
    /* 0x030 */ NFoe *intruderFoe;  /* section 11 */
    /* 0x034 */ s32 *stageList;     /* section 12: 28 stage ids */
    /* 0x038 */ s32 *bgmList;       /* section 13: eleven music ids */
    /* 0x03C */ MFlash flash[UBRANK_FLASH_NUM]; /* 0 the list, 1 the versus panel, 2 the closing movie */
    /* 0x0C0 */ void *bg;           /* section 4: background picture */
    /* 0x0C4 */ u8 *tex[37];        /* movie 0 */
    /* 0x158 */ u8 *texB[6];        /* movie 1 */
    /* 0x170 */ u8 *texC[13];       /* movie 2; [10] the opponent's portrait, [11] the player's */
    /* 0x1A4 */ u8 *playerChip;     /* small picture that stands for the player in the list */
    /* 0x1A8 */ MTextBox box[UBRANK_BOX_NUM];
    /* 0x4F0 */ s32 flags;          /* UBRANK_ */
    /* 0x4F4 */ s32 cursor[14];     /* indexed by state; only [0], the row (0..4), is used */
    /* 0x52C */ s32 timer;          /* frames until the fade out starts */
    /* 0x530 */ s32 unk530[2];
    /* 0x538 */ s32 state;          /* UBRANK_ST_ */
    /* 0x53C */ s32 voiceReq;       /* line the guide is asked to say (1..6) */
    /* 0x540 */ s32 voicePrev;
    /* 0x544 */ s32 voiceSkip;
    /* 0x548 */ s32 voiceLine;      /* -1 = none yet */
    /* 0x54C */ s32 unk54C;
    /* 0x550 */ s32 blink;
    /* 0x554 */ s32 talk;
    /* 0x558 */ s32 idle;           /* frames without input */
    /* 0x55C */ s32 extra;          /* place shown on the sixth plate while the list scrolls */
    /* 0x560 */ s32 loadState;      /* UBRANK_LOAD_ */
    /* 0x564 */ s32 top;            /* place on the first plate (0 = first place) */
    /* 0x568 */ s32 unk568;
    /* 0x56C */ NRankRule *curRule; /* the opponent chosen: rules */
    /* 0x570 */ NFoe *curFoe;       /* the opponent chosen: fighter */
    /* 0x574 */ s32 isIntruder;
    /* 0x578 */ s32 won;            /* the battle just fought was won */
    /* 0x57C */ s32 climb;          /* places still to move up after beating an intruder */
    /* 0x580 */ s32 wait;           /* frames the versus panel stays */
} UbRank; /* 0x584 */

#define UBRANK_CHOSEN 1
#define UBRANK_LEAVING 2
#define UBRANK_STARTED 4        /* the cursor plate was lit once */
#define UBRANK_FADED_IN 8
#define UBRANK_FACES_READY 0x10 /* both portraits are loaded */
#define UBRANK_PANEL_DONE 0x20  /* the versus panel's movie reached its mark */
#define UBRANK_INTRUDE 0x40     /* the intruder's entrance is playing */
#define UBRANK_INTRUDE_DONE 0x80
#define UBRANK_RESULT 0x100     /* the screen was entered from a battle for a higher place */

#define UBRANK_ST_LIST 0        /* choosing a place */
#define UBRANK_ST_INTRUDE 1     /* an intruder cuts in */
#define UBRANK_ST_VS 2          /* the versus panel opens; waits for the portraits */
#define UBRANK_ST_CLOSING 3     /* starts the closing movie */
#define UBRANK_ST_WAIT 4        /* 300 frames or confirm, then leave for the battle */
#define UBRANK_ST_RESULT 5      /* back from a battle: the win / lose panel */
#define UBRANK_ST_CLIMB 6       /* the list scrolls while the player moves up */

#define UBRANK_LOAD_NONE 0
#define UBRANK_LOAD_REQUEST 1
#define UBRANK_LOAD_READ 2
#define UBRANK_LOAD_UNPACK 3

extern UbRank *gUbRank;         /* 0x3B7354 */

void UbzSel_SetupBattle(void);
void UbzSel_Init(void);
void UbzSel_Term(void);
void UbzSel_Draw(void);
void UbzSel_Update(void);
void UbzSel_UpdateVoice(void);
void UbzSel_Input(s32 *result);
void UbzSel_ClipGoto(s32 movie, s32 step, char *label);
s32 UbzSel_Run(void);
void UbRank_SetupBattle(void);
s32 UbRank_PlaceToRule(s32 place);
void UbRank_SetFoe(s32 idx, s32 intruder);
s32 UbRank_PickFoe(s32 place);
s32 UbRank_ScrollUp(void);
void UbRank_Init(void);
void UbRank_Term(void);
void UbRank_Draw(void);
void UbRank_Update(void);
void UbRank_UpdateVoice(void);
void UbRank_Input(s32 *result);
void UbRank_ClipGoto(s32 movie, s32 state, char *label);
void UbRank_UpdateFaceLoad(void);
s32 UbRank_Run(void);

#endif
