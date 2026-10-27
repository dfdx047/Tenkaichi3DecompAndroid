#ifndef MENU_MENU_Q_H
#define MENU_MENU_Q_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (as in include/menu/menu_r.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x37F430..0x3851B0 (placeholder stem "menu_q"): mode group 13..30 (handler 0x379A58,
 * archive gMenuArc3, main-menu item 1). Two pieces, cut at an object boundary:
 *
 *   (menu_q.c)  0x37F430..0x37F850  UbScore  tail of the score sheet module: merged into src/menu/menu_p_d.c (the
 *                                            object starts at 0x37EE18); its views are in include/menu/menu_p.h
 *   menu_q_b.c  0x37F850..0x3851B0  SimDay   the day screen of mode 22 (work pointer gSimDay 0x3B7384, 0xBBC
 *                                            bytes), a whole object: the former src/menu/menu_r.c (0x3840E0..:
 *                                            the guide's lines, pad handler 0x384260, 0x384D48, portrait loader,
 *                                            frame loop 0x385020) is merged into it. This header has the layout of
 *                                            the structure; include/menu/menu_r.h only declares the type's name.
 *                                            The two headers cannot be included together (gSaveData).
 *
 * The movie labels call the mode "sim" ("mc_sim_botan", "mc_sim_monita", "fl_syugyo_*" = training): a run is a
 * ladder of rounds of ten turns; on nine of them the player picks a button of a board and an event plays, the
 * tenth is the round's fight. All names are guesses from what the code does ("Ub" = the group of modes 13..30,
 * as in menu_m.h / menu_p.h). The structures are this chunk's own views.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Flash_Reset(MFlash *flash, s32 keepClips);
extern void Flash_Stop(MFlash *flash);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h,
                          s32 mode, s32 parentFmt);
extern void Dialog_Draw(s32 visible);
extern void Dialog_Start(s32 cmd);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetMsg(s32 idx);
extern s32 Dialog_Input(s32 allowCancel);
extern s32 Dialog_IsClosed(void);
extern void Voice_StopWithLip(void);
extern void Dialog_SetCursor(s32 choice);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *charaBits); /* (a pointer: declared s32 it was passed as 32 bits on the stack, the upper half of what the function reads left to chance on a 64-bit build) */
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  u16 *items);
extern void BattleSetup_FinishEx(s32 wide);

/* The battle result (BattleResult of include/battle/battle_setup.h; local view). */
typedef struct QBattleResult {
    /* 0x00 */ u8 unk0[0x2C];
    /* 0x2C */ f32 health[2];   /* what each side had left, in per cent */
} QBattleResult;

extern QBattleResult *BattleResult_GetPtr(void);

/* ---- gSaveData as this chunk uses it (include/sys/save.h has the flat layout) ---- */

/* An entry of the ranking of the mode 22 ladder, best first. */
typedef struct QSaveRank {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 score;
    /* 0x08 */ u8 cleared;      /* 1 = the ladder was finished */
} QSaveRank; /* 0xC */

#define Q_RANK_NUM 10

/* Best result of one mission (PSaveMission of menu_p.h) and of one course of modes 24..30. */
typedef struct QSaveBest {
    /* 0x00 */ u8 cleared;
    /* 0x01 */ u8 rank;
    /* 0x02 */ u8 hours;
    /* 0x03 */ u8 minutes;
    /* 0x04 */ u8 seconds;
    /* 0x08 */ s32 total;
} QSaveBest; /* 0xC */

/* Best result of one course of modes 17..19. */
typedef struct QSaveBestB {
    /* 0x00 */ s32 value;       /* the sheet's fourth line */
    /* 0x04 */ s32 total;
    /* 0x08 */ u8 rank;
    /* 0x09 */ u8 hours;
    /* 0x0A */ u8 minutes;
    /* 0x0B */ u8 seconds;
} QSaveBestB; /* 0xC */

/* The 8-byte checksum and the body, nested as in include/menu/menu_c.h (it reproduces the address arithmetic). */
typedef struct QSaveBody {
    /* 0x0008 */ u8 unk8[0x200];
    /* 0x0208 */ s32 ubFlags;
    /* 0x020C */ s32 unk20C;            /* 20 once everything is unlocked (Save_UnlockAll), 5 by default */
    /* 0x0210 */ QSaveRank rank[Q_RANK_NUM];
    /* 0x0288 */ s32 unk288;
    /* 0x028C */ QSaveBest mission[100];
    /* 0x073C */ QSaveBestB bestB[5];
    /* 0x0778 */ s32 unk778[2];
    /* 0x0780 */ QSaveBest bestC[54];
    /* 0x0A08 */ u8 unkA08[0x2EC8 - 0xA08];
    /* 0x2EC8 */ u8 item[0x15E];        /* bit 0: owned */
} QSaveBody;

typedef struct QSave {
    /* 0x00 */ s32 sum[2];
    /* 0x08 */ QSaveBody body;
} QSave;

extern QSave *gSaveData;

#define QSAVE (&gSaveData->body)
#define Q_ITEM_NUM 0x15E

/* ---- gProgress as this chunk uses it ---- */

/* What was chosen for the player's fighter (UbMember of menu_m.h). */
typedef struct QMember {
    /* 0x00 */ s32 unk0[6];
    /* 0x18 */ s32 color;       /* costume */
    /* 0x1C */ s32 chara;
    /* 0x20 */ u16 items[8];
} QMember; /* 0x30 */

#define SIM_STAT_ATK 0
#define SIM_STAT_DEF 1
#define SIM_STAT_HP 2
#define SIM_STAT_POINT 3
#define SIM_STAT_NUM 4
#define SIM_ITEM_NUM 3
#define SIM_LEVEL_MAX 6
#define SIM_TURNS 10            /* turns of a round; the fight is on the last one */

/* The state of the mode 22 ladder, kept between its screens and its fights. */
typedef struct SimState {
    /* 0x00 (0x64C) */ s32 level;        /* 0..6: stars shown; caps attack and defence (SimLevel.max) */
    /* 0x04 (0x650) */ s32 stat[SIM_STAT_NUM]; /* attack, defence, health in per cent, points / 100 */
    /* 0x14 (0x660) */ s32 unk14;
    /* 0x18 (0x664) */ s32 item[SIM_ITEM_NUM]; /* item ids carried into the fight, -1 = none; a ring */
    /* 0x24 (0x670) */ s32 itemHead;     /* ring position of the oldest item */
    /* 0x28 (0x674) */ s32 turn;         /* turns played; / 10 = round (the opponent), % 10 = turn of the round */
    /* 0x2C (0x678) */ s32 unk2C;
    /* 0x30 (0x67C) */ s32 wait;         /* turns until button 3 of the board is open again */
    /* 0x34 (0x680) */ s32 off;          /* bit n: button n of the board is greyed out */
} SimState;

typedef struct QProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x424];
    /* 0x440 */ QMember member;          /* the player's fighter (chosen by SoloSel, mode 21) */
    /* 0x470 */ u8 unk470[0x1DC];
    /* 0x64C */ SimState sim;
} QProgress;

#define QPROG ((QProgress *)gProgress)

/* ---- SimDay (menu_q_b.c) ---- */

/* One rank of a training (gSimTrain0 the card game, gSimTrain1 and gSimTrain2 the two others). */
typedef struct SimTrain {
    /* 0x00 */ s32 param;       /* kind 0: cards on the table */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 points;
    /* 0x0C */ s32 gain;
    /* 0x10 */ s32 loss;
    /* 0x14 */ s32 turn;        /* first turn (1-based) of this rank */
} SimTrain; /* 0x18 */

#define SIMDAY_TRAIN_RANKS 5
#define SIMDAY_TRAIN_KINDS 3

/* A round of the ladder (pack section 25). 998 = drawn at random, 999 = none. */
typedef struct SimRound {
    /* 0x00 */ s32 announcer;
    /* 0x04 */ s32 unk4;        /* BattleSetup_SetRule's last argument (non-zero = 1) */
    /* 0x08 */ s32 timeLimit;
    /* 0x0C */ s32 stage;
    /* 0x10 */ s32 bgm;
    /* 0x14 */ s32 unk14;       /* BattleSetup_SetSide's unk1FC of the opponent's side (non-zero = 1) */
    /* 0x18 */ s32 enemy;       /* index into the enemy table */
} SimRound; /* 0x1C */

/* An opponent (pack section 26). */
typedef struct SimEnemy {
    /* 0x00 */ s32 chara;       /* 998 = drawn from the pool of the round */
    /* 0x04 */ s32 color;
    /* 0x08 */ s32 cpuLevel;
    /* 0x0C */ s32 lastItem;    /* item id for slot 7, 999 = none */
    /* 0x10 */ s32 item[7];     /* item ids, 999 = none (the list is packed into slots 0..) */
} SimEnemy; /* 0x2C */

/* A pool of characters for a random opponent (pack section 27): ends at 999 or after 16. */
typedef struct SimPool {
    s32 chara[16];
} SimPool; /* 0x40 */

/* A level of the board (pack section 30). */
typedef struct SimLevel {
    /* 0x00 */ s32 max;         /* highest attack / defence at this level */
    /* 0x04 */ s32 need;        /* points / 100 above which this level is reached */
} SimLevel; /* 0x8 */

/*
 * A random event (pack section 28): its weight (of 256) in each state of the save (SimDay.eventSet picks the
 * column); row n runs script n + 5.
 */
typedef struct SimEventRow {
    s32 weight[5];
} SimEventRow; /* 0x14 */

#define SIMDAY_BGM_FIRST 0x10B16  /* Bgm_Play id of music 0 */
#define SIMDAY_FACE_FILE DISC_RESOURCE(0x2F9, 0x71C)     /* + character id: compressed portrait */
#define SIMDAY_FACE_SIZE 0x16800
#define SIMDAY_FACE_RES_SIZE 0x20800

/* Voice_GetStat result when nothing is playing. */
#define SIMDAY_VOICE_IDLE 5
/* Voice bank base of this mode group's guides (Voice_PlayWithSubtitle). */
#define SIMDAY_VOICE_BASE 0x8765

#define SIMDAY_RANDOM 998
#define SIMDAY_NONE 999
#define SIMDAY_EVENT_ROWS 32

/* The stat changes an event queued, shown one after the other (nested: it reproduces the address arithmetic). */
typedef struct SimChange {
    /* 0x00 (0xB90) */ s32 mask;        /* bit n: stat n has a change pending; 0x10 = level-up shown */
    /* 0x04 (0xB94) */ s32 applying;    /* the change shown is being counted into the stat */
    /* 0x08 (0xB98) */ s32 delta[SIM_STAT_NUM]; /* pending change of each stat */
    /* 0x18 (0xBA8) */ s32 stat;        /* stat whose change is shown */
    /* 0x1C (0xBAC) */ s32 shown;       /* attack / defence: picture of the step (2 + step); health / points: the amount */
    /* 0x20 (0xBB0) */ s32 mark;        /* which of three marks shows the step */
} SimChange; /* 0x24 */

#define SIMDAY_FLASH_NUM 7
#define SIMDAY_FL_BOARD 0        /* the board: status plates, buttons, turn lamps, the monitor */
#define SIMDAY_FL_EVENT 1        /* event icon, stat change marks */
#define SIMDAY_FL_CARD 2         /* training 0: cards (the movie depends on the rank) */
#define SIMDAY_FL_POPO 3         /* training 1 */
#define SIMDAY_FL_SHOT 4         /* training 2 */
#define SIMDAY_FL_MENU 5         /* the three-row window */
#define SIMDAY_FL_ROUND 6        /* round number */

typedef struct SimDay {
    /* 0x000 */ void *pack;         /* file baseFile + 0x1A (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 34 sections */
    /* 0x008 */ void *faceFile[2];  /* 0x16800 bytes each: compressed portraits (the player's character, the round's opponent) */
    /* 0x010 */ void *faceRes[2];   /* 0x20800 bytes each: the same unpacked */
    /* 0x018 */ void *msgText;      /* section 24 */
    /* 0x01C */ void *text;         /* section 32 */
    /* 0x020 */ MFlash flash[SIMDAY_FLASH_NUM];
    /* 0x154 */ void *bg;           /* section 4: background picture */
    /* 0x158 */ u8 *tex0[63];       /* board */
    /* 0x254 */ u8 *tex5[6];        /* window */
    /* 0x26C */ u8 *tex1[11];       /* event */
    /* 0x298 */ u8 *tex2[16];       /* cards */
    /* 0x2D8 */ u8 *tex3[15];
    /* 0x314 */ u8 *tex4[18];
    /* 0x35C */ u8 *tex6[16];       /* round; [13] and [14] are the two portraits: the opponent's, the player's (SimDay_UpdateFaceLoad) */
    /* 0x39C */ MTexRes *numRes;    /* section 22 */
    /* 0x3A0 */ MTexRes *faceResA;  /* section 6: pictures for tex0[32] */
    /* 0x3A4 */ MTexRes *faceResB;  /* section 7: pictures for tex0[33] */
    /* 0x3A8 */ u8 *faceDefault;    /* shown for picture 12 */
    /* 0x3AC */ MTextBox box[3];    /* item names */
    /* 0x550 */ s32 flags;          /* SIMDAY_ */
    /* 0x554 */ s32 loadState;      /* portrait loader: SIMDAY_LOAD_ */
    /* 0x558 */ s32 cur[13];        /* cursor of each state */
    /* 0x58C */ s32 timer;
    /* 0x590 */ s32 state;          /* SIMDAY_ST_ */
    /* 0x594 */ s32 prevState;      /* state the window was opened from */
    /* 0x598 */ s32 talk[2];        /* [0] the guide's closing line to say (1..5), 0 = none; [1] the last one said */
    /* 0x5A0 */ s32 msgLine;        /* line of the message window, -1 = none */
    /* 0x5A4 */ s32 menuText;       /* 1 = the window shows the item names, 0 = its plates */
    /* 0x5A8 */ s32 bgm;            /* + 0x10B16 = music of the round */
    /* 0x5AC */ s32 wait;           /* frames the round picture stays (300) */
    /* 0x5B0 */ s32 day;            /* turn of the round, 0..9 */
    /* 0x5B4 */ s32 preview[5];     /* what a script wants shown for each stat */
    /* 0x5C8 */ s32 own[Q_ITEM_NUM]; /* owned items an event can hand out */
    /* 0xB40 */ s32 ownCount;
    /* 0xB44 */ s32 eventSet;       /* column of the event table: 0..3 by what the save has unlocked */
    /* 0xB48 */ SimEventRow *eventTbl; /* section 28: the 32 random events */
    /* 0xB4C */ SimRound *round;  /* section 25 */
    /* 0xB50 */ SimPool *pool;    /* section 27 */
    /* 0xB54 */ SimEnemy *enemy;  /* section 26 */
    /* 0xB58 */ s32 enemyChara;     /* the opponent of the fight set up */
    /* 0xB5C */ void *unkB5C;       /* section 29 */
    /* 0xB60 */ SimLevel *level;  /* section 30 */
    /* 0xB64 */ s32 event;          /* what the turn turned into (script number) */
    /* 0xB68 */ s32 unkB68[2];
    /* 0xB70 */ s32 lastEvent;
    /* 0xB74 */ s32 repeat;         /* times in a row the same event came up, at most 2 */
    /* 0xB78 */ s32 faceA;          /* pictures the monitor goes back to */
    /* 0xB7C */ s32 faceB;
    /* 0xB80 */ s32 rank[SIMDAY_TRAIN_KINDS]; /* rank of each training at the current turn */
    /* 0xB8C */ s32 unkB8C;         /* answer of the two-row menu: 1 = first row */
    /* 0xB90 */ SimChange chg;
    /* 0xBB4 */ s32 potara;         /* bit n: item plate n is lit */
    /* 0xBB8 */ s32 levelUp;        /* a level-up is due when the board comes back */
} SimDay; /* 0xBBC */

#define SIMDAY_DONE 1
#define SIMDAY_LEAVING 2
#define SIMDAY_STARTED 4
#define SIMDAY_GREETED 8
#define SIMDAY_FACES_READY 0x10   /* the two portraits of the round picture are loaded */
#define SIMDAY_FACE_ON 0x20       /* the monitor shows a character */
#define SIMDAY_BUSY 0x40          /* set while the monitor closes: the pad handler waits */
#define SIMDAY_WAIT_KEY 0x80      /* the script waits for the confirm button */
#define SIMDAY_SAME_TURN 0x100    /* the board comes back without a turn passing */
#define SIMDAY_SKIP_TURNS 0x200   /* five turns pass at once */
#define SIMDAY_LEVEL_UP 0x400     /* the level-up animation runs */
#define SIMDAY_WINDOW 0x2000      /* the window is open: only its movie advances */

#define SIMDAY_LOAD_IDLE 0
#define SIMDAY_LOAD_REQUEST 1
#define SIMDAY_LOAD_READ 2
#define SIMDAY_LOAD_UNPACK 3

/* state */
#define SIMDAY_ST_BOARD 0        /* the board's four buttons */
#define SIMDAY_ST_TRAIN 1        /* the three trainings (button 0) */
#define SIMDAY_ST_SELECT 2       /* a two-row choice of an event */
#define SIMDAY_ST_SCRIPT 3       /* the turn's event script runs (gSimEvent) */
#define SIMDAY_ST_WAIT 4         /* an animation started by SimDay_Cmd runs */
#define SIMDAY_ST_VERSUS 5       /* the fight is announced */
#define SIMDAY_ST_WINDOW 6       /* the three-row window (movie 5) is open */
#define SIMDAY_ST_WINDOW_ITEMS 7 /* the window shows the carried items */
#define SIMDAY_ST_CONFIRM 8      /* its row 2: the yes / no dialog */
#define SIMDAY_ST_BACK 9         /* back from the dialog to the window */
#define SIMDAY_ST_ROUND 10       /* the round number is shown */
#define SIMDAY_ST_ROUND_WAIT 11
#define SIMDAY_ST_QUIT 12

extern SimDay *gSimDay;   /* 0x3B7384 */

extern s32 SimEvent_Run(SimDay *day, u32 event);   /* menu_r_b.c: one step of event script `event` */

s32 SimDay_GetTrainRank(s32 kind);
void SimDay_SetFaceA(s32 n);
void SimDay_SetFaceB(s32 n);
void SimDay_SetupBattle(void);
s32 SimDay_IsEventItem(s32 item);
void SimDay_ListItems(void);
void SimDay_AddChange(s32 stat, s32 amount);
s32 SimDay_GetStat(s32 stat, s32 preview);
void SimDay_GiveItem(void);
void SimDay_TakeItem(void);
s32 SimDay_CountItems(void);
s32 SimDay_CountItems2(void);
void SimDay_SetButton(s32 on, s32 button);
void SimDay_PassWait(s32 turns);
s32 SimDay_NextChange(void);
void SimDay_ShowChange(void);
void SimDay_SetMenuAlpha(s32 text);
void SimDay_PlayBgm(void);
void SimDay_Cmd(s32 cmd);
void SimDay_PickEvent(s32 unused);   /* both callers pass an argument (0 / 1) that it does not use */
void SimDay_RefreshBoard(void);
void SimDay_Init(s32 section);
void SimDay_Term(void);
void SimDay_Draw(void);
void SimDay_Update(void);
void SimDay_UpdateTalk(void);
void SimDay_Input(s32 *result);
void SimDay_ClipGoto(s32 movie, s32 kind, char *label);   /* plays a label on the cursor's plate */
void SimDay_UpdateFaceLoad(void);
s32 SimDay_Run(s32 section);

#endif
