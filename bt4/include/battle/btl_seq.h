#ifndef BATTLE_BTL_SEQ_H
#define BATTLE_BTL_SEQ_H

#include "types.h"
#include "battle/battle.h"
#include "sys/ramp.h"

/*
 * Battle sequence: the state machine that drives one match from the stage intro to the fade out.
 * Source range 0x216AC0-0x2187E0 (core, clocks, win judgement and every state handler); src/battle/btl_seq.c
 * also holds the skill-list text of the pause menu, 0x215420-0x216AC0 (BtlText_*, at the end of this header).
 *
 * The frame loop calls BtlSeq_PreUpdate() before the battle simulation and BtlSeq_Update() after it.
 * BtlSeq_Update() returns 1 when the battle scene has to be left.
 *
 * States (the index is the same in the three tables; a table only leaves slots empty):
 *
 *   0 BTL_SEQ_STAGE_INTRO  stage fly-through, three camera cuts. Default table only.
 *                          -> 1 when the last cut ends or the skip callback fires.
 *   1 BTL_SEQ_INTRO_TALK   the two fighters' entrance poses and voice lines (special dialogue when the
 *                          pair has one, else a random line 0/1 each). Mode 1 plays nothing and waits
 *                          for the "extra" call. -> 2
 *   2 BTL_SEQ_READY        0.8 s wait, then announcement 0 for 2.0 s with the fighters released. -> 3
 *   3 BTL_SEQ_FIGHT        the match. Runs the clocks, the pause request and BtlSeq_CheckBattleEnd().
 *                          -> 4 when the battle is decided, -> 6 directly when the result has the
 *                          "aborted" bit (BTL_RESULT_ABORT) or the mode is 7.
 *   4 BTL_SEQ_FINISH       the finish announcement (K.O. / time up / ...), 3.5 s (1.1 s for reason bit 18).
 *                          -> 5 when a side won and the mode is not 8, else -> 6.
 *   5 BTL_SEQ_WIN_TALK     winner's pose and voice line. Default table and mode 1 only. -> 6
 *   6 BTL_SEQ_END          result handling and 1.2 s fade out. -> 99
 *  99 BTL_SEQ_EXIT         not a table entry: BtlSeq_Update() stops there. It returns 1 (leave the battle
 *                          scene), unless a restart was asked for (result reason bits 0x18000, or mode 6
 *                          with reason bit 2), in which case it sets battle flag 0x8000 and returns 0.
 *
 * A state whose `enter` slot is empty is never entered: BtlSeq_Update() goes to state 6 instead, and
 * BtlSeq_Reset() starts at the first non-empty slot.
 *
 * Table by Battle_GetMode():
 *   mode 1        gBtlSeqTblMode1     1 2 3 4 5 6   (no stage intro; states 1 and 5 have an `extra` handler)
 *   modes 5, 6, 7 gBtlSeqTblMode5to7      2 3 4   6   (no intro, no talk, no winner scene)
 *   anything else gBtlSeqTblDefault   0 1 2 3 4 5 6
 */

enum {
    BTL_SEQ_STAGE_INTRO = 0,
    BTL_SEQ_INTRO_TALK = 1,
    BTL_SEQ_READY = 2,
    BTL_SEQ_FIGHT = 3,
    BTL_SEQ_FINISH = 4,
    BTL_SEQ_WIN_TALK = 5,
    BTL_SEQ_END = 6,
    BTL_SEQ_STATE_COUNT = 7,
    BTL_SEQ_EXIT = 99
};

/* Every handler gets a pointer to BtlSeq.ctx and returns an s32. */
typedef s32 (*BtlSeqFunc)(void *ctx);
/* The per-state argument: a "skip / pause requested" poll, or NULL. */
typedef s32 (*BtlSeqPollFunc)(void);

/* One state of a table. */
typedef struct BtlSeqState {
    /* 0x00 */ BtlSeqFunc enter;     /* on entering the state; NULL marks the state as absent */
    /* 0x04 */ BtlSeqFunc preUpdate; /* every frame, before the battle simulation */
    /* 0x08 */ BtlSeqFunc update;    /* every frame, after it; returns the next state */
    /* 0x0C */ BtlSeqFunc exit;      /* on leaving the state */
    /* 0x10 */ BtlSeqFunc extra;     /* BtlSeq_CallExtra(); only mode 1 has any (skip the talk) */
    /* 0x14 */ BtlSeqPollFunc poll;  /* copied to BtlSeq.ctx.poll before every handler call */
} BtlSeqState; /* size 0x18 */

/* The linear ramp of sys/ramp.h used as a frame timer (Ramp_Start starts it, Ramp_Step steps it). */
typedef Ramp BtlSeqTimer;

/* Context of the states that only wait: READY, FINISH, END. */
typedef struct BtlSeqWaitCtx {
    /* 0x00 */ BtlSeqPollFunc poll;
    /* 0x04 */ s32 step;
    /* 0x08 */ BtlSeqTimer timer;
} BtlSeqWaitCtx;

/* Context of INTRO_TALK and WIN_TALK. */
typedef struct BtlSeqTalkCtx {
    /* 0x00 */ BtlSeqPollFunc poll;
    /* 0x04 */ s32 step;       /* 0 start 1st line, 1 wait, 2 start 2nd line, 3 wait, 4 done, 99 wait for skip */
    /* 0x08 */ s32 side[2];    /* side (0/1) speaking first / second; WIN_TALK only uses [0] = winner */
    /* 0x10 */ s32 line[2];    /* voice line of each speaker */
    /* 0x18 */ s32 chara[2];   /* character id of each speaker */
    /* 0x20 */ BtlSeqTimer timer; /* 10 s time-out of a line */
    /* 0x38 */ s32 skip;       /* set by the `extra` handler in mode 1 */
} BtlSeqTalkCtx;

typedef struct BtlSeq {
    /* 0x000 */ s32 state;
    /* 0x004 */ union {
        BtlSeqPollFunc poll; /* +0x004: BtlSeqState.poll of the current state */
        BtlSeqWaitCtx wait;
        BtlSeqTalkCtx talk;
        u8 raw[0x100];
    } ctx;                   /* not cleared between states */
    /* 0x104 */ BtlSeqState *table;
    /* 0x108 */ BtlClock clock;    /* time since the fight started, compared with the time limit */
    /* 0x118 */ BtlClock subClock; /* second clock, restarted by BtlSeq_ResetSubClock() */
    /* 0x128 */ s32 endCheckOff;   /* non-zero: BtlSeq_CheckBattleEnd() does nothing (clocks stop too) */
} BtlSeq; /* size 0x12C */

extern BtlSeq *gBtlSeq;
extern BtlSeqState gBtlSeqTblDefault[BTL_SEQ_STATE_COUNT];
extern BtlSeqState gBtlSeqTblMode1[BTL_SEQ_STATE_COUNT];
extern BtlSeqState gBtlSeqTblMode5to7[BTL_SEQ_STATE_COUNT];

s32 BtlSeq_FirstState(BtlSeqState *table);
s32 BtlSeq_Call(BtlSeqFunc func, s32 state);
s32 BtlSeq_Reset(void);
s32 BtlSeq_Init(void);
void BtlSeq_Term(void);
s32 BtlSeq_PreUpdate(void);
s32 BtlSeq_Update(void);
BtlClock *BtlSeq_GetClock(void);
BtlClock *BtlSeq_GetSubClock(void);
s32 BtlSeq_GetState(void);
s32 BtlSeq_IsFighting(void);
s32 BtlSeq_IsFinish(void);
s32 BtlSeq_CallExtra(void);
void BtlSeq_SetEndCheckOff(s32 off);
s32 BtlSeq_GetEndCheckOff(void);
void BtlClock_Clear(BtlClock *clock);
s32 BtlClock_Tick(BtlClock *clock);
void BtlSeq_ResetClocks(void);
s32 BtlSeq_TickClocks(void);
void BtlSeq_ResetSubClock(void);
s32 BtlSeq_GetTimeLeft(void);
void BtlSeq_StopTalk(void);
s32 BtlSeq_CheckBattleEnd(void);

/*
 * Skill-list text of the pause menu, 0x215420-0x216AC0 (the first part of src/battle/btl_seq.c).
 * Each side has a list of pages ('$' lines of the script) holding entries ('*' lines).
 */

/* One side's list state. */
typedef struct BtlTextList {
    /* 0x00 */ s32 pages;      /* number of '$' lines */
    /* 0x04 */ s32 count[10];  /* '*' entries per page */
    /* 0x2C */ s32 page;       /* current page */
    /* 0x30 */ s32 scroll[10]; /* first visible entry per page */
    /* 0x58 */ s32 cursor[10]; /* selected entry per page */
} BtlTextList; /* size 0x80 */

/* The work behind gBtlMenu (owned by the pause menu code before this range). */
typedef struct BtlTextWork {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ s32 side;   /* which list is shown */
    /* 0x10 */ s32 unk10;  /* bit number tested against a line's mask digit */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ u16 *text;  /* BtlMenu_GetScript() */
    /* 0x1C */ u16 *text2; /* BtlMenu_GetScript2() */
    /* 0x20 */ BtlTextList list[2];
} BtlTextWork;

void BtlText_DrawPart(void *pkt, s32 x, s32 y, s32 w, s32 h, s32 part);
void BtlText_DrawPageIcon(void *pkt, s32 x, s32 y, u16 digit);
s32 BtlText_CheckLineMask(u16 **cursor, s32 side);
s32 BtlText_CheckUnlock(u16 **cursor);
void BtlText_DrawList(void *pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 mode);
void BtlText_DrawScrollBar(void *pkt, s32 unused, s32 x, s32 y0, s32 y1);
void BtlText_CountEntries(void);
u16 *BtlText_FindEntry(s32 n);
void BtlText_DrawEntryName(s32 x, s32 y, s32 n, s32 align, f32 alpha);

#endif
