#ifndef BATTLE_BTL_AI_INT_H
#define BATTLE_BTL_AI_INT_H

/* Shared by the two CPU-player files btl_ai_seq.c and btl_ai_cond.c: views of other modules' data, the functions
 * they call, and the macros for the sequence stack and the level tables. */

#include "battle/btl_ai.h"
#include "sys/rand.h"

extern BtlAi *gBtlAi;
extern BtlAiStateFunc gBtlAiStateFuncs[4];
extern s32 D_002EDA70[]; /* rule condition id -> index of the condition function */

/* Part of the fighter data that BtlCharApi_GetMoveTable returns. */
typedef struct BtlAiChrMoves {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s16 unk10[0x47];
    /* 0x9E */ s8 unk9E[2];
} BtlAiChrMoves;

/* Part of the fighter data that BtlCharApi_GetSkillTable returns. */
typedef struct BtlAiChrSkills {
    /* 0x000 */ u8 unk0[0x13E];
    /* 0x13E */ s8 unk13E[0x27];
    /* 0x165 */ s8 unk165[0x37];
    /* 0x19C */ s32 cost[1];
} BtlAiChrSkills;

/* The action table seen from its +8 (the form step handler 17 uses). */
typedef struct BtlAiActBody {
    /* 0x000 */ u8 unk0[0x380];
    /* 0x380 */ u8 actClass[1];
} BtlAiActBody;

extern s32 BtlSeq_GetState(void);
extern s32 BtlChar_IsStage4Or27(void);
extern void BtlCharApi_SetInjectedInput(s32 objId, u32 buttons, f32 stickX, f32 stickY);
extern s32 BtlCharApi_GetUnk974(s32 objId); /* the fighter's current action id */
extern s32 BtlAiStep_ReachClass(BtlAiWork *ai);
extern s32 BtlAiStep_NotHit(BtlAiWork *ai);
extern void BtlAiPad_Clear(BtlAiOutput *out, s32 keep);
extern void BtlAiPad_EndFrame(BtlAiOutput *out);
extern void BtlAiMove_Dispatch(BtlAiWork *ai);
extern void BtlAiCombo_Dispatch(BtlAiWork *ai);
extern void BtlAiFollow_Dispatch(BtlAiWork *ai);
extern void BtlAiAct36_Dispatch(BtlAiWork *ai);
extern s32 BtlCharApi_IsInClashA(s32 objId);
extern s32 BtlCharApi_FindIncomingBlast(s32 objId, s32 arg);
extern void BtlCharApi_MarkIncomingBlast(s32 objId);
extern BtlAiChrSkills *BtlCharApi_GetSkillTable(s32 objId);
extern BtlAiChrMoves *BtlCharApi_GetMoveTable(s32 objId);
extern s32 BtlCharApi_HasBlastLimit(s32 objId);
extern s32 BtlCharApi_GetHp(s32 objId);
extern s32 BtlCharApi_GetMaxPower(s32 objId); /* member entry + 0x1C */
extern s32 BtlCharApi_GetParamUnk14(s32 objId);
extern s32 BtlCharApi_GetParamFlags(s32 objId);
extern s32 BtlCharApi_GetOppSkillKind(s32 objId);
extern s32 BtlCharApi_IsOppSkillFlag4(s32 objId);
extern s32 BtlCharApi_TestOppSkillFlags(s32 objId);
extern s32 BtlCharApi_GetClashCountB(s32 objId);
extern s32 BtlCharApi_GetClashCountA(s32 objId);
extern s32 BtlCharApi_IsMoveSlotActive(s32 objId, u32 slot);
extern s32 BtlCharApi_GetStunTimer(s32 objId);
extern s32 BtlCharApi_GetPromptButtons(s32 objId);
extern s32 BtlCharApi_GetUnk1290(s32 objId);
extern f32 BtlCharApi_GetUnkE44B(s32 objId);
extern s32 BtlCharApi_TestPoseBit80(s32 objId, s32 arg);
extern s32 BtlCharApi_TestFlagBE(s32 objId);
extern s32 BtlCharApi_GetParamByte8F(s32 objId, s32 arg);
extern s32 BtlSide_GetKi(s32 objId); /* member entry + 0xC */
extern s32 BtlSide_GetBlast(s32 objId); /* member entry + 0x14 */
extern s32 BtlSide_IsPoweredUp(s32 objId); /* fighter flag 6 */

s32 BtlAiCond_GuardRoll(BtlAiWork *ai);
void BtlAi_ScaleByGauge(BtlAiWork *ai, s32 *lo, s32 *hi);
f32 BtlAi_GetLowGaugeFactor(BtlAiWork *ai);

/* The entry on top of the sequence stack. Written with a shift: an array index gives the other operand order. */
#define SEQ_TOP(seq) (*(BtlAiSeqEntry *)((u8 *)(seq) + ((seq)->depth << 3) + 0xC))
/* Five-step tables by level: 0..5, 6..11, 12..17, 18..23, 24..29. */
#define LEVEL_IDX(ai) ((ai)->level < 0 ? 0 : (ai)->level / 6)
#define LEVEL_STEP(ai, tbl) ((ai)->level < 0 ? 0 : (tbl)[(ai)->level / 6])
/* The character's own rate table: one signed byte per rate at level 0 (+8) and at level 29 (+0x100). */
#define RATE(ai, lo, hi, n) BtlAi_ScaleByLevel((ai)->level, lo[n], hi[n])

#endif
