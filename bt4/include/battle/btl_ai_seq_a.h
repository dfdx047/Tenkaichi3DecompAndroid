#ifndef BATTLE_BTL_AI_SEQ_A_H
#define BATTLE_BTL_AI_SEQ_A_H

/*
 * CPU player: the generic sequence (scripted action) runner, first part of src/battle/btl_ai_seq.c, 0x1B4140..0x1B6008.
 * To be merged into btl_ai_seq.c. Local views of the parts of the AI block that btl_ai.h leaves unnamed;
 * offsets marked (m) are used by matching code.
 */

#include "battle/btl_ai.h"

/* One step of a scripted action: what to press and what ends it. The array is BtlAiSeqActTable.steps. */
typedef struct BtlAiSeqStep {
    /* 0x00 */ s8 handler;   /* (m) index into gBtlAiStepFuncs (0..23): the function that says "step finished" */
    /* 0x01 */ s8 group;     /* (m) the steps of an action with the same group are alternatives; the pick function
                                    chooses one of them, and the group number goes up by one when a step finishes */
    /* 0x02 */ u8 pad02[2];
    /* 0x04 */ s16 special;  /* (m) BtlAiPad_Set's "special" argument (0 none, 1..3) */
    /* 0x06 */ s16 cls;      /* (m) the state class the fighter is in once the input has taken effect; -1 = none */
    /* 0x08 */ s32 hold;     /* (m) AI button bits sent every frame */
    /* 0x0C */ s32 press;    /* (m) AI button bits sent every other frame */
    /* 0x10 */ s32 once;     /* (m) AI button bits sent on one frame only */
} BtlAiSeqStep; /* size 0x14 */

/* The tables of the action table from +8 on. */
typedef struct BtlAiSeqBody {
    /* 0x000 */ u16 first[0x80];     /* (m) per action id: index of its first step */
    /* 0x100 */ u8 count[0x80];      /* (m) per action id: number of steps */
    /* 0x180 */ s8 pick[0x80];       /* (m) per action id: index into gBtlAiPickFuncs (0..15) */
    /* 0x200 */ s8 timeout[0x80];    /* (m) per action id: seconds the run phase may last before the input has taken
                                            effect; 0 or less = no limit */
    /* 0x280 */ u16 flags[0x80];     /* (m) per action id: BTLAI_SEQF_* */
    /* 0x380 */ s8 stateClass[0x200]; /* (m) per fighter state id (BtlCharApi_GetUnk974) */
    /* 0x580 */ u8 stateFlags[0x200]; /* (m) per fighter state id; bit 1 (0x02) is tested here: the state cannot be
                                             taken as "reached" / the opponent is still a threat */
} BtlAiSeqBody;

/* BtlAiData.act as this file reads it (BtlAiActTable in btl_ai.h is the same block). */
typedef struct BtlAiSeqActTable {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ BtlAiSeqStep *steps; /* (m) */
    /* 0x008 */ BtlAiSeqBody body;
} BtlAiSeqActTable;

#define BTLAI_SEQF_END_WHEN_HIT 0x01   /* (m) the action ends when the fighter's state class is 1..7 (that these are
                                          the classes of being hit is inferred) */
#define BTLAI_SEQF_WAIT_IDLE 0x02      /* (m) the action only starts from state class 0 (class 10: DASH is pressed) */
/* 0x04: tested by rule condition 18 (BtlAiCond_SeqFlag2), not here. */
#define BTLAI_SEQF_END_IN_CLASH 0x08   /* (m) the action ends in state class 0x20 or in a clash */
#define BTLAI_SEQF_END_OPP_SKILL 0x10  /* (m) the action ends when the opponent's technique has a kind */

/* BtlAiSeq.flags bits used by the generic sequence. */
#define BTLAI_SEQ_TOOK_EFFECT 0x40     /* (m) the fighter reached the step's state class in a new state */
#define BTLAI_SEQ_CHAIN 0x80           /* (m) set by sequence 3 kind 1; cleared when the stack empties */
#define BTLAI_SEQ_COMBO_SECOND 0x100   /* (m) pick function 7 chose alternative 1 at rush step 0; read by it and by
                                          AiThink_GetBlastStep */
#define BTLAI_SEQ_FULL_CHARGE 0x200    /* (m) step handler 7: the charge was full on the previous frame */
#define BTLAI_SEQ_NO_HIT_END 0x400     /* (m) set by step handler 15: BTLAI_SEQF_END_WHEN_HIT is ignored */
#define BTLAI_SEQ_RELEASE 0x800        /* (m) set by step handler 17: the run phase sends nothing */

/* BtlAiSeq with the fields this file gives a meaning (same layout as BtlAiSeq). */
typedef struct BtlAiSeqA {
    /* 0x00 */ s32 flags;      /* (m) */
    /* 0x04 */ s32 depth;      /* (m) */
    /* 0x08 */ s32 phase;      /* (m) 0 init, 1 start, 2 run, 3 end */
    /* 0x0C */ s32 group;      /* (m) step group that is being played (BtlAiSeq.stepCount): 0 at init, +1 per
                                      finished step */
    /* 0x10 */ s32 step;       /* (m) index of the chosen step within the action */
    /* 0x14 */ BtlAiSeqEntry stack[8];
    /* 0x54 */ s32 timer;      /* (m) the step's parameter: frames, percent, member number ... set by the pick
                                      function, used by the step handler */
    /* 0x58 */ s32 waitTimer;  /* (m) frames the run phase has waited for the input to take effect */
    /* 0x5C */ s32 startState; /* (m) the fighter's state id when the step started */
    /* 0x60 */ f32 charge;     /* (m) BtlCharApi_GetChargeRate when the step started / last frame */
    /* 0x64 */ f32 startDist;  /* (m) fighter distance when the step started; refreshed by step handler 2 */
    /* 0x68 */ s32 ruleSet;
    /* 0x6C */ s32 unk6C[2];
    /* 0x74 */ f32 dirX;       /* (m) the fighter's facing on the previous frame (step handler 12) */
    /* 0x78 */ f32 dirZ;       /* (m) */
    /* 0x7C */ s32 prevId;     /* (m) the action that just ended when action 0x43 is chained after it */
    /* 0x80 */ s32 unk80;
    /* 0x84 */ s32 roll;
    /* 0x88 */ s32 threshold;
    /* 0x8C */ s32 unk8C[3];
} BtlAiSeqA; /* size 0x98 */

/* Chooses which of the n alternative steps of the current group is played; arg is the stack entry's argument.
   Returns the alternative (0..n-1) or a negative number to end the action. */
typedef s32 (*BtlAiPickFunc)(BtlAiWork *ai, s32 n, s32 arg);

s32 BtlAiSeq_IsInterrupted(BtlAiWork *ai, s32 id);
s32 BtlAiSeq_CheckHeight(BtlAiWork *ai, s32 mode);
s32 BtlAiSeq_CheckStageAbort(BtlAiWork *ai, s32 id);
void BtlAiSeq_PhaseInit(BtlAiWork *ai);
void BtlAiSeq_PhaseStart(BtlAiWork *ai);
void BtlAiSeq_PhaseRun(BtlAiWork *ai);
s32 BtlAiSeq_RollPowerUpChain(BtlAiWork *ai);
void BtlAiSeq_PhaseEnd(BtlAiWork *ai);

s32 BtlAiPick_Random(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_ByArg(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_ArgSeconds(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_ArgFrames(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_KiTarget(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_SkillCharge(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_LevelDelay(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_ComboBranch(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_FusionSlot(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_TransformSlot(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_SwitchMember(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_DummyMode(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_PromptButton(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_GuardMode(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_Direction(BtlAiWork *ai, s32 n, s32 arg);
s32 BtlAiPick_SetTimer(BtlAiWork *ai, s32 n, s32 arg);

s32 BtlAiStep_ReachClass(BtlAiWork *ai);
s32 BtlAiStep_ChargeKi(BtlAiWork *ai);
s32 BtlAiStep_UntilBlocked(BtlAiWork *ai);
s32 BtlAiStep_Countdown(BtlAiWork *ai);
s32 BtlAiStep_BlockedOrCharged(BtlAiWork *ai);
s32 BtlAiStep_BlockedAndCharged(BtlAiWork *ai);
s32 BtlAiStep_NotHit(BtlAiWork *ai);
s32 BtlAiStep_Charged(BtlAiWork *ai);
s32 BtlAiStep_GuardUntilSafe(BtlAiWork *ai);
s32 BtlAiStep_GuardCountdown(BtlAiWork *ai);
s32 BtlAiStep_ReachClassNoShots(BtlAiWork *ai);
s32 BtlAiStep_OneFrame(BtlAiWork *ai);
s32 BtlAiStep_FacingOpponent(BtlAiWork *ai);
s32 BtlAiStep_SwitchQueued(BtlAiWork *ai);

#endif
