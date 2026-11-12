#ifndef MENU_MENU_T_H
#define MENU_MENU_T_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x38CB38..0x3911A8 (placeholder stem "menu_t"): 28 of the 37 day-event handlers of
 * the "sim" day screen (mode 22 of the mode group 13..30, handler 0x379A58, archive gMenuArc3, main-menu item
 * 1), table gSimEvent at 0x3B7388. Two pieces, cut at an object boundary:
 *
 *   menu_t.c    0x38CB38..0x3900D0  gSimEvent[3..27]; the object starts in the previous chunk (menu_s_d.c,
 *                                   0x38C360: gSimEvent[0..2])
 *                                   -- now merged into src/menu/menu_s_d.c (0x38C360..0x3900D0), which
 *                                   includes this header
 *   menu_t_b.c  0x3900D0..0x3911A8  the card game: four helpers and gSimEvent[28]; then gSimEvent[29..30]
 *                                   (the events go on in the next chunk, menu_u, at 0x3911A8)
 *
 * Event numbers: gSimEvent[0..2] are the three trainings (board row 0), [3] is board row 2, [4] board row 3,
 * [5..36] are the 32 random events of board row 1, drawn by SimDay_PickEvent from the weights of pack section
 * 28 (so "random event n" below is gSimEvent[n + 5]).
 *
 * SimEvent_Run (menu_r_b.c) calls gSimEvent[day->event](day) once a frame while the day screen is in its
 * script state; a handler runs step day->seq and returns 1 when the event is over. All names are guesses from
 * what the code does. The structures are this chunk's own views (include/menu/menu_q.h has the full SimDay).
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern s32 rand(void);                /* libc rand(), not the shared Mersenne Twister */
extern void *memcpy(void *dst, const void *src, u32 n);
extern void Bgm_Stop(void);
extern void StreamSe_Stop(s32 se);

/* ---- gProgress and gSaveData as the handlers use them ---- */

typedef struct TProgress {
    /* 0x000 */ u8 unk0[0x674];
    /* 0x674 */ s32 turn;           /* SimState.turn of menu_q.h: turns played in this run */
} TProgress;

#define TPROG ((TProgress *)gProgress)

typedef struct TSave {
    /* 0x000 */ u8 unk0[0x20C];
    /* 0x20C */ s32 unk20C;         /* 5 by default, 20 after Save_UnlockAll; event 17 adds one */
} TSave;

extern TSave *gSaveData;

/* ---- The day screen's work area (SimDay of include/menu/menu_q.h): what the handlers touch ---- */

#define SIMEV_FLASH_CARD 2          /* the movie of the card game */

#define SIMTRAIN_OUTCOMES 6

/* Section 29 of the day screen's pack: one block per training (0xC0 bytes). (From menu_s.h, with menu_s_d.c.) */
typedef struct SimTrainTbl {
    /* 0x00 */ s32 weight[3][SIMTRAIN_OUTCOMES]; /* by times the training was repeated: chance of each outcome, of 100 */
    /* 0x48 */ s32 hp[SIMTRAIN_OUTCOMES];        /* change of stat 2 */
    /* 0x60 */ s32 atkMin[SIMTRAIN_OUTCOMES];    /* change of stat 0: drawn from min..max */
    /* 0x78 */ s32 atkMax[SIMTRAIN_OUTCOMES];
    /* 0x90 */ s32 defMin[SIMTRAIN_OUTCOMES];    /* change of stat 1 */
    /* 0xA8 */ s32 defMax[SIMTRAIN_OUTCOMES];
} SimTrainTbl; /* 0xC0 */

typedef struct TSimDay {
    /* 0x000 */ u8 unk0[0x20];
    /* 0x020 */ MFlash flash[7];
    /* 0x154 */ u8 unk154[0x3FC];
    /* 0x550 */ s32 flags;          /* SIMEV_ */
    /* 0x554 */ s32 unk554;
    /* 0x558 */ s32 cur[13];        /* cursor of each state; [3] = the card under the cursor */
    /* 0x58C */ s32 timer;
    /* 0x590 */ s32 state;
    /* 0x594 */ s32 prevState;
    /* 0x598 */ s32 unk598[2];
    /* 0x5A0 */ s32 msgLine;        /* line of the message window, -1 = none */
    /* 0x5A4 */ u8 unk5A4[0x5B8];
    /* 0xB5C */ SimTrainTbl *train; /* section 29 */
    /* 0xB60 */ void *level;
    /* 0xB64 */ s32 event;          /* index into gSimEvent */
    /* 0xB68 */ s32 seq;            /* step of the running handler */
    /* 0xB6C */ s32 seqTimer;       /* frames in this step */
    /* 0xB70 */ s32 lastEvent;
    /* 0xB74 */ s32 repeat;
    /* 0xB78 */ s32 faceA;          /* picture the monitor goes to on command 5 */
    /* 0xB7C */ s32 faceB;          /* picture of the character shown by command 6 */
    /* 0xB80 */ s32 rank[3];        /* rank of each training at the current turn; [0] = the card game */
    /* 0xB8C */ s32 answer;         /* what the yes / no window (command 17) returned: 1 = yes */
} TSimDay;

#define SIMEV_WAIT_KEY 0x80         /* the handler waits for the confirm button */
#define SIMEV_FLAG200 0x200
#define SIMEV_LEVEL_UP 0x400        /* SIMDAY_LEVEL_UP of menu_q.h */
#define SIMEV_FLAG800 0x800         /* the next training comes out as outcome 0 (menu_s_d.c) */
#define SIMEV_FLAG1000 0x1000       /* the next training comes out as outcome 5 */

/* SimDay_Cmd numbers the handlers use (the switch is in menu_q_b.c). */
#define SIMEV_CMD_MONITOR_OFF 5     /* the monitor's picture goes out (then shows faceA) */
#define SIMEV_CMD_CHARA_IN 6        /* character faceB comes onto the monitor */
#define SIMEV_CMD_CHARA_OUT 7
#define SIMEV_CMD_SELECT 17         /* opens the yes / no window */
#define SIMEV_CMD_SURPRISE 25       /* "mc_surprise_icon" */
#define SIMEV_CMD_SHOW_CHANGE 26    /* shows the stat changes queued with SimDay_AddChange */
#define SIMEV_CMD_LEVEL_UP 33
#define SIMEV_CMD_CARD_PLAY 36      /* 36..43: the card game's movie */

/* Stats of SimDay_AddChange / SimDay_GetStat. */
#define SIMEV_STAT_ATK 0
#define SIMEV_STAT_DEF 1
#define SIMEV_STAT_HP 2             /* per cent */
#define SIMEV_STAT_POINT 3

/* One rank of a training (SimTrain of menu_q.h); gSimTrain0 is the card game. */
typedef struct TSimTrain {
    /* 0x00 */ s32 param;           /* cards on the table */
    /* 0x04 */ s32 seconds;         /* how long the cards stay face up */
    /* 0x08 */ s32 points;          /* points for a win */
    /* 0x0C */ s32 gain;            /* attack for a win */
    /* 0x10 */ s32 loss;            /* attack for a loss */
    /* 0x14 */ s32 turn;
} TSimTrain; /* 0x18 */

extern TSimTrain gSimTrain0[5];     /* 0x3B9010, defined with the day screen (menu_q_b.c) */

/* previous chunks (menu_q_b.c) */
extern void SimDay_AddChange(s32 stat, s32 amount);
extern s32 SimDay_GetStat(s32 stat, s32 preview);
extern void SimDay_GiveItem(void);
extern void SimDay_PlayBgm(void);
extern void SimDay_Cmd(s32 cmd);

/*
 * The card game's state. In the main executable's .bss, like gSimTrainOutcome0..2 (0x31EA94) and gCharRefState:
 * uninitialised globals of overlay source placed as common symbols.
 */
extern s32 gSimCardShown[10];       /* 0x31EAA0: the card on each place, as dealt */
extern s32 gSimCardOrder[10];       /* 0x31EAC8: the dealt cards in rising order (after a full shuffle of 0..9) */
extern s32 gSimCardNext;            /* 0x31EAF0: how many were picked in the right order; -1 = a wrong pick */
extern s32 gSimCardRank;            /* 0x31EAF4: the rank the game is played at (day->rank[0]) */
extern u8 gSimCardPicked;           /* 0x31EAF8: bit n = place n was picked */
extern u8 gSimCardWrong;            /* 0x31EAF9: bit n = place n was the wrong pick */

/*
 * Outcome of the last training of each kind. In the main executable's .bss (common symbols of overlay source,
 * like gCharRefState at 0x31EA80, whose nine entries would overlap these: see the report).
 */
extern s32 gSimTrainOutcome0; /* 0x31EA94 */
extern s32 gSimTrainOutcome1; /* 0x31EA98 */
extern s32 gSimTrainOutcome2; /* 0x31EA9C */

s32 SimEv00_Roll(TSimDay *day);
s32 SimEv00(TSimDay *day);
s32 SimEv01_Roll(TSimDay *day);
s32 SimEv01(TSimDay *day);
s32 SimEv02_Roll(TSimDay *day);
s32 SimEv02(TSimDay *day);
s32 SimEv03(TSimDay *day);
s32 SimEv04(TSimDay *day);
s32 SimEv05(TSimDay *day);
s32 SimEv06(TSimDay *day);
s32 SimEv07(TSimDay *day);
s32 SimEv08(TSimDay *day);
s32 SimEv09(TSimDay *day);
s32 SimEv10(TSimDay *day);
s32 SimEv11(TSimDay *day);
s32 SimEv12(TSimDay *day);
s32 SimEv13(TSimDay *day);
s32 SimEv14(TSimDay *day);
s32 SimEv15(TSimDay *day);
s32 SimEv16(TSimDay *day);
s32 SimEv17(TSimDay *day);
s32 SimEv18(TSimDay *day);
s32 SimEv19(TSimDay *day);
s32 SimEv20(TSimDay *day);
s32 SimEv21(TSimDay *day);
s32 SimEv22(TSimDay *day);
s32 SimEv23(TSimDay *day);
s32 SimEv24(TSimDay *day);
s32 SimEv25(TSimDay *day);
s32 SimEv26(TSimDay *day);
s32 SimEv27(TSimDay *day);
void SimEv28_SelectCard(TSimDay *day);
void SimEv28_UnselectCard(TSimDay *day);
s32 SimEv28_MoveCursor(TSimDay *day, s32 dir);
void SimEv28_Deal(s32 n);
s32 SimEv28(TSimDay *day);
s32 SimEv29(TSimDay *day);
s32 SimEv30(TSimDay *day);

#endif
