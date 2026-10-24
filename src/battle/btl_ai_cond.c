#include "common.h"
#include "battle/btl_ai_int.h"
#include "battle/btl_ai_think.h"
#include "sys/common.h"
#include "sys/heap.h"
#include "sys/rand.h"

/*
 * CPU player, second object, 0x1B6D50..0x1BB128: helpers, the rule conditions and the level-scaled rate getters
 * (to 0x1B80F8), then the weighted conditions, the rule evaluator and the thinker. BtlAi_ScaleByLevel (0x1B6D00)
 * is the last function of the object before (btl_ai_seq.c): its callers here match only with it external.
 */

/* The object's file-scope tables, 0x2EDA70..0x2EDF08: they come before all of its function-local data, so they
 * were defined at the top of the source file. D_002EDA70 maps a rule condition id to the index of its condition
 * function; the others are column tables read by the rule evaluator further on in the object (not decompiled
 * here). Kept as assembly data until that code is. */
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDA70);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDC70);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDCE0);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDD50);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDDC0);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDDD8);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDE10);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDEB0);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDEE8);

/* ---- Second object: helpers, rule conditions (table at 0x2C4768, same order), rate getters. ---- */

/* Records the opponent's current action and its class, and raises reaction bits. */
void BtlAi_NoteOpponent(BtlAiWork *ai, s32 react) {
    BtlAiActTable *act = gBtlAi->data->act;
    s32 action = BtlCharApi_GetUnk974(TagTeam_Target(ai->objId));
    BtlAiStatus *st = &ai->status;
    s32 cls = act->actClass[action];

    st->oppAction = action;
    st->oppClass = cls;
    st->react |= react;
}

/* Condition 0: never. */
s32 BtlAiCond_False(BtlAiWork *ai, u8 arg) {
    return 0;
}

/* Condition 1: always. */
s32 BtlAiCond_True(BtlAiWork *ai, u8 arg) {
    return 1;
}

/* Condition 2: arg percent chance. */
s32 BtlAiCond_Percent(BtlAiWork *ai, u8 arg) {
    BtlAiSeq *seq = &ai->seq;
    s32 roll = Rand_Range(100);

    seq->roll = roll;
    seq->threshold = arg;
    return roll < arg;
}

/* Condition 3: no sequence is running. */
s32 BtlAiCond_Idle(BtlAiWork *ai, u8 arg) {
    return ai->seq.depth == 0;
}

/* Condition 4: level >= arg. */
s32 BtlAiCond_LevelAtLeast(BtlAiWork *ai, u8 arg) {
    if (ai->level < arg) {
        return 0;
    }
    return 1;
}

/* Condition 5: level < arg. */
s32 BtlAiCond_LevelBelow(BtlAiWork *ai, u8 arg) {
    return ai->level < arg;
}

/* Condition 6: status bit 4 equals arg. */
s32 BtlAiCond_Status4(BtlAiWork *ai, u8 arg) {
    return ((s32)(ai->status.flags >> 4) & 1) == arg;
}

/* Condition 7: member word +0xC (of 100000) is at least arg percent. */
s32 BtlAiCond_GaugeAPercent(BtlAiWork *ai, u8 arg) {
    if ((f32)BtlSide_GetKi(ai->objId) / 100000.0f * 100.0f >= (f32)arg) {
        return 1;
    }
    return 0;
}

/* Condition 8: member word +0x14 holds at least arg units of 100000. */
s32 BtlAiCond_GaugeBStock(BtlAiWork *ai, u8 arg) {
    if (BtlSide_GetBlast(ai->objId) / 100000 < arg) {
        return 0;
    }
    return 1;
}

/* Fighter flag 6 equals arg. Condition 34 contains an inlined copy of condition 9, so the test was an inline
 * function in the original; a non-static `inline` BtlAiCond_Flag6 itself matches too, but this compiler emits such a
 * function at the end of the object instead of here. */
static inline s32 BtlAiCond_TestFlag6(BtlAiWork *ai, u8 arg) {
    return BtlSide_IsPoweredUp(ai->objId) == arg;
}

/* Condition 9: fighter flag 6 equals arg. */
s32 BtlAiCond_Flag6(BtlAiWork *ai, u8 arg) {
    return BtlAiCond_TestFlag6(ai, arg);
}

/* Condition 10: status bit 6 equals arg. */
s32 BtlAiCond_Status6(BtlAiWork *ai, u8 arg) {
    return ((s32)(ai->status.flags >> 6) & 1) == arg;
}

/* Condition 11: on stage 4 or 27 and BtlCharApi_TestPoseBit80(1) is zero. */
s32 BtlAiCond_Unk11(BtlAiWork *ai, u8 arg) {
    s32 r;

    if (BtlChar_IsStage4Or27() == 0) {
        r = 0;
    } else {
        r = BtlCharApi_TestPoseBit80(ai->objId, 1) == 0;
    }
    return r == arg;
}

/* Condition 12: BtlCharApi_TestOppSkillFlags equals arg. */
s32 BtlAiCond_Unk12(BtlAiWork *ai, u8 arg) {
    return BtlCharApi_TestOppSkillFlags(ai->objId) == arg;
}

/* Condition 13: plan word 8 is 2. */
s32 BtlAiCond_Plan8Is2(BtlAiWork *ai, u8 arg) {
    return (ai->plan.unk8 == 2) == arg;
}

/* Conditions 14, 15, 16: bits 0, 1, 2 of work + 0xC. */
s32 BtlAiCond_WorkBit0(BtlAiWork *ai, u8 arg) {
    return (ai->unkC & 1) == arg;
}

s32 BtlAiCond_WorkBit1(BtlAiWork *ai, u8 arg) {
    return ((ai->unkC >> 1) & 1) == arg;
}

s32 BtlAiCond_WorkBit2(BtlAiWork *ai, u8 arg) {
    return ((ai->unkC >> 2) & 1) == arg;
}

/* Condition 17: not a test, stores arg in the plan and passes. */
s32 BtlAiCond_SetPlan54(BtlAiWork *ai, u8 arg) {
    ai->plan.unk54 = arg;
    return 1;
}

/* Condition 18: flag bit 2 of the running sequence equals arg (fails when idle). */
s32 BtlAiCond_SeqFlag2(BtlAiWork *ai, u8 arg) {
    BtlAi *mgr = gBtlAi;
    BtlAiSeq *seq = &ai->seq;
    BtlAiActTable *act = mgr->data->act;

    if (seq->depth == 0) {
        return 0;
    }
    return ((act->seqFlags[SEQ_TOP(seq).id] >> 2) & 1) == arg;
}

/* Condition 19: level below -1. */
s32 BtlAiCond_LevelBelowDummy(BtlAiWork *ai, u8 arg) {
    return (ai->level < -1) == arg;
}

/* Helper of condition 20: 75% chance to react to an opponent action of class 1..7, 22 or 23. */
s32 BtlAiCond_GuardRoll(BtlAiWork *ai) {
    BtlAiStatus *st = &ai->status;
    s8 cls = gBtlAi->data->act->actClass[BtlCharApi_GetUnk974(TagTeam_Target(ai->objId))];
    s32 roll = Rand_Range(100);

    if (!((u32)(cls - 1) < 7) && cls != 0x16 && cls != 0x17) {
        return 0;
    }
    if (st->flags & 0x1000000) {
        return 0;
    }
    if (roll < 25) {
        return 0;
    }
    st->react &= ~0x10000;
    return 1;
}

/* Condition 20: reaction rolls against what the opponent is doing; the chance grows with the level. */
s32 BtlAiCond_React(BtlAiWork *ai, u8 arg) {
    s32 guard[5] = { 20, 40, 60, 80, 95 };
    s32 evade[5] = { 10, 20, 30, 40, 50 };
    s32 blastA[5] = { 0, 4, 8, 12, 15 };
    s32 blastB[5] = { 0, 3, 5, 8, 10 };
    s32 blastC[5] = { 0, 2, 3, 4, 5 };
    s32 step = LEVEL_IDX(ai);
    s32 roll = Rand_Range(100);
    s32 action = BtlCharApi_GetUnk974(TagTeam_Target(ai->objId));
    s32 chance;

    switch (arg) {
    case 0:
        BtlAi_NoteOpponent(ai, 0x10000);
        if (BtlCharApi_GetPromptButtons(ai->objId) & 4) {
            return BtlAiCond_GuardRoll(ai);
        }
    case 1:
        chance = LEVEL_STEP(ai, guard);
        break;
    case 2:
        BtlAi_NoteOpponent(ai, 0x80000);
        chance = LEVEL_STEP(ai, evade);
        break;
    case 3:
        BtlAi_NoteOpponent(ai, 0x100000);
        if (action == 0x37 || action == 0x3C) {
            chance = LEVEL_STEP(ai, blastA);
        } else if (action == 0x38 || action == 0x3D) {
            chance = LEVEL_STEP(ai, blastB);
        } else if (action == 0x39 || action == 0x3E) {
            chance = LEVEL_STEP(ai, blastC);
        } else {
            BtlAi_NoteOpponent(ai, 0x10);
            return 0;
        }
        break;
    default:
        return 0;
    }
    return roll < chance;
}

/* Condition 21: the battle sequence is in the Ready state. */
s32 BtlAiCond_SeqReady(BtlAiWork *ai, u8 arg) {
    return (BtlSeq_GetState() == 2) == arg;
}

/* Condition 22: the stage is 4 or 27. */
s32 BtlAiCond_Stage4Or27(BtlAiWork *ai, u8 arg) {
    return BtlChar_IsStage4Or27() == arg;
}

/* Condition 23: once per reaction bit 0x40000, a level-stepped chance. */
s32 BtlAiCond_React40000(BtlAiWork *ai, u8 arg) {
    s32 tbl[5] = { 40, 55, 70, 85, 95 };
    s32 chance = LEVEL_STEP(ai, tbl);
    s32 roll = Rand_Range(100);

    if (ai->status.react & 0x40000) {
        return 0;
    }
    BtlAi_NoteOpponent(ai, 0x40000);
    return roll < chance;
}

/* Condition 24: BtlCharApi_HasBlastLimit equals arg. */
s32 BtlAiCond_Unk24(BtlAiWork *ai, u8 arg) {
    return BtlCharApi_HasBlastLimit(ai->objId) == arg;
}

/* Condition 25: character rate 0. */
s32 BtlAiCond_Rate0(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 0);
    BtlAiSeq *seq = &ai->seq;

    seq->roll = roll;
    seq->threshold = chance;
    if (ai->status.react & 2) {
        BtlAi_NoteOpponent(ai, 0x20);
    }
    return roll < chance;
}

/* Condition 26: character rate 5, only while one of two fighter flags is up. */
s32 BtlAiCond_Rate5(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 5);
    s32 a = BtlCharApi_GetParamUnk14(ai->objId);
    s32 b = BtlCharApi_GetParamFlags(ai->objId);

    if (!(a & 0x20) && !(b & 2)) {
        return 0;
    }
    return roll < chance;
}

/* Condition 27: BtlCharApi_FindIncomingBlast(1) is 2 (only with arg 1). */
s32 BtlAiCond_Unk27(BtlAiWork *ai, u8 arg) {
    if (BtlCharApi_FindIncomingBlast(ai->objId, 1) == 2 && arg == 1) {
        return 1;
    }
    return 0;
}

/* Condition 28: character rate 6; raises reaction bit 0x80. */
s32 BtlAiCond_Rate6(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 6);

    BtlAi_NoteOpponent(ai, 0x80);
    return roll < chance;
}

/* Condition 29: character rate 1; raises reaction bit 0x40 when a fighter query returns -1. */
s32 BtlAiCond_Rate1(BtlAiWork *ai, u8 arg) {
    BtlAiSeq *seq = &ai->seq;
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 1);

    if (BtlCharApi_GetOppSkillKind(ai->objId) == -1) {
        BtlAi_NoteOpponent(ai, 0x40);
    }
    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition 30: character rate 4; raises reaction bit 0x100. */
s32 BtlAiCond_Rate4(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 4);

    BtlAi_NoteOpponent(ai, 0x100);
    return roll < chance;
}

/* Condition 31: AI-type rate chosen by which of the opponent actions 0x37..0x3A / 0x3C..0x3F is running. */
s32 BtlAiCond_TypeRateByOppAction(BtlAiWork *ai, u8 arg) {
    BtlAiSeq *seq = &ai->seq;
    s32 action = BtlCharApi_GetUnk974(TagTeam_Target(ai->objId));
    u8 *prof = gBtlAi->data->profile[ai->type];
    s8 *lo = (s8 *)prof + 0x2A8;
    s8 *hi = (s8 *)prof + 0x568;
    s32 idx = action < 0x3C ? action - 0x37 : action - 0x3C;
    s32 roll = Rand_Range(100);
    s32 chance;

    if ((u32)idx >= 4) {
        return 0;
    }
    chance = RATE(ai, lo, hi, idx);
    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition 32: character rate 2; raises reaction bit 0x400. */
s32 BtlAiCond_Rate2(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 2);
    BtlAiSeq *seq = &ai->seq;
    BtlAiStatus *st;

    seq->roll = roll;
    seq->threshold = chance;
    st = &ai->status;
    st->react |= 0x400;
    return roll < chance;
}

/* Condition 33: character rate 8 (+20 for arg 1), or a fighter query; arms a 90-frame timer. */
s32 BtlAiCond_Rate8(BtlAiWork *ai, u8 arg) {
    BtlAiStatus *st = &ai->status;
    BtlAiSeq *seq = &ai->seq;
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 8);
    s32 force = BtlCharApi_GetUnk1290(ai->objId);

    if (arg == 0) {
        st->timer18 = 90;
    } else if (arg == 1) {
        st->timer1C = 90;
        chance += 20;
    }
    seq->roll = roll;
    seq->threshold = chance;
    if (roll < chance || force) {
        return 1;
    }
    return 0;
}

/* Condition 34: 10% chance, never while fighter flag 6 is set; arms a 90-frame timer. */
s32 BtlAiCond_TenPercent(BtlAiWork *ai, u8 arg) {
    s32 roll = Rand_Range(100);

    ai->status.timer20 = 90;
    if (BtlAiCond_TestFlag6(ai, 0)) {
        return roll < 10;
    }
    return 0;
}

/* Scales a lo/hi pair down as member word +0x1C (of 30000) rises above a level-dependent percentage. */
void BtlAi_ScaleByGauge(BtlAiWork *ai, s32 *lo, s32 *hi) {
    u8 *prof = gBtlAi->data->profile[ai->type];
    f32 limit = (f32)BtlAi_ScaleByLevel(ai->level, prof[0x2AC], prof[0x56C]);
    f32 pct = (f32)BtlCharApi_GetMaxPower(ai->objId) / 30000.0f * 100.0f;

    s32 a = *lo;
    s32 b = *hi;

    if (limit < pct) {
        f32 k = 1.0f - (pct - limit) / (100.0f - limit);

        *lo = (s32)((f32)a * k);
        *hi = (s32)((f32)b * k);
    }
}

/* Multiplier applied when member word +0xC is under 20% / 40% of 100000. */
f32 BtlAi_GetLowGaugeFactor(BtlAiWork *ai) {
    f32 pct = (f32)BtlSide_GetKi(ai->objId) / 100000.0f * 100.0f;
    u8 *prof = gBtlAi->data->profile[ai->type];
    f32 r = 1.0f;
    s32 lo;
    s32 hi;

    if (pct < 20.0f) {
        lo = prof[0x2B7];
        hi = prof[0x577];
    } else if (pct < 40.0f) {
        lo = prof[0x2B6];
        hi = prof[0x576];
    } else {
        return r;
    }
    return (f32)BtlAi_ScaleByLevel(ai->level, lo, hi) * 0.1f;
}

/* A 16-byte row of a rate table as the two functions below read it: four byte columns at 0, 4, 8 and 12. They
 * match only when the columns are read as FIELDS of a row at (table + offset); with plain `table[i + 8]` the loads
 * and the stores to lo / hi come out interleaved. Each case also needs its own block-local index variable. */
typedef struct AiRateRow {
    /* 0x0 */ s8 c0;
    /* 0x1 */ s8 pad1[3];
    /* 0x4 */ s8 c4;
    /* 0x5 */ s8 pad5[3];
    /* 0x8 */ s8 c8;
    /* 0x9 */ s8 pad9[3];
    /* 0xC */ s8 c12;
    /* 0xD */ s8 padD[3];
} AiRateRow;

/* Picks one of 14 lo/hi byte pairs (rows of 16, columns 0 and 8), scales them by gauge and returns the value for
 * the level. */
s32 BtlAi_GetPairRate(BtlAiWork *ai, u32 kind, s32 off, s8 *base_lo, s8 *base_hi, s32 byGauge) {
    BtlAiPlan *plan = &ai->plan;
    s32 lo = 0;
    s32 hi = 0;

    switch (kind) {
    case 0:
        lo = ((AiRateRow *)(base_lo + off))->c0;
        hi = ((AiRateRow *)(base_hi + off))->c0;
        break;
    case 1:
        lo = ((AiRateRow *)(base_lo + off))->c8;
        hi = ((AiRateRow *)(base_hi + off))->c8;
        break;
    case 2: {
        s32 i = off + 0x10;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 3: {
        s32 i = off + 0x10;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    case 4: {
        s32 i = off + 0x20;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 5: {
        s32 i = off + 0x20;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    case 6: {
        s32 i = off + 0x30;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 7: {
        s32 i = off + 0x30;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    case 8: {
        s32 i = off + 0x40;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 9: {
        s32 i = off + 0x40;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    case 10: {
        s32 i = off + 0x50;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 11: {
        s32 i = off + 0x50;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    case 12: {
        s32 i = off + 0x60;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 13: {
        s32 i = off + 0x60;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    default:
        return 0;
    }
    if (byGauge == 1) {
        BtlAi_ScaleByGauge(ai, &lo, &hi);
    }
    if (D_002EDA70[plan->cond] == 0x26) {
        f32 k = BtlAi_GetLowGaugeFactor(ai);

        lo = (s32)((f32)lo * k);
        hi = (s32)((f32)hi * k);
    }
    return BtlAi_ScaleByLevel(ai->level, lo, hi);
}

/* Same with rows of four pairs (columns 0, 4, 8, 12) and no low-gauge factor. */
s32 BtlAi_GetQuadRate(BtlAiWork *ai, u32 kind, s32 off, s8 *base_lo, s8 *base_hi, s32 byGauge) {
    s32 lo = 0;
    s32 hi = 0;

    switch (kind) {
    case 0:
        lo = ((AiRateRow *)(base_lo + off))->c0;
        hi = ((AiRateRow *)(base_hi + off))->c0;
        break;
    case 1:
        lo = ((AiRateRow *)(base_lo + off))->c4;
        hi = ((AiRateRow *)(base_hi + off))->c4;
        break;
    case 2:
        lo = ((AiRateRow *)(base_lo + off))->c8;
        hi = ((AiRateRow *)(base_hi + off))->c8;
        break;
    case 3:
        lo = ((AiRateRow *)(base_lo + off))->c12;
        hi = ((AiRateRow *)(base_hi + off))->c12;
        break;
    case 4: {
        s32 i = off + 0x10;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 5: {
        s32 i = off + 0x10;

        lo = ((AiRateRow *)(base_lo + i))->c4;
        hi = ((AiRateRow *)(base_hi + i))->c4;
        break;
    }
    case 6: {
        s32 i = off + 0x10;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    case 7: {
        s32 i = off + 0x10;

        lo = ((AiRateRow *)(base_lo + i))->c12;
        hi = ((AiRateRow *)(base_hi + i))->c12;
        break;
    }
    case 8: {
        s32 i = off + 0x20;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 9: {
        s32 i = off + 0x20;

        lo = ((AiRateRow *)(base_lo + i))->c4;
        hi = ((AiRateRow *)(base_hi + i))->c4;
        break;
    }
    case 10: {
        s32 i = off + 0x20;

        lo = ((AiRateRow *)(base_lo + i))->c8;
        hi = ((AiRateRow *)(base_hi + i))->c8;
        break;
    }
    case 11: {
        s32 i = off + 0x20;

        lo = ((AiRateRow *)(base_lo + i))->c12;
        hi = ((AiRateRow *)(base_hi + i))->c12;
        break;
    }
    case 12: {
        s32 i = off + 0x30;

        lo = ((AiRateRow *)(base_lo + i))->c0;
        hi = ((AiRateRow *)(base_hi + i))->c0;
        break;
    }
    case 13: {
        s32 i = off + 0x30;

        lo = ((AiRateRow *)(base_lo + i))->c4;
        hi = ((AiRateRow *)(base_hi + i))->c4;
        break;
    }
    default:
        return 0;
    }
    if (byGauge == 1) {
        BtlAi_ScaleByGauge(ai, &lo, &hi);
    }
    return BtlAi_ScaleByLevel(ai->level, lo, hi);
}

/* ---- 0x1B80F8..0x1BB128 ---- */
/*
 * CPU opponent: weighted rule conditions (functions 35..117 of the condition table at 0x2C4768), the rule
 * evaluator, the thinker, and the binding of the AI data and of one side. 0x1B80F8..0x1BB128.
 *
 * Same object as the code above: it uses the file-scope tables at the top of the file (D_002EDA70 ..
 * D_002EDEE8; AiThink_FindWeightColumn only assembles as in the original when they are defined in its file) and
 * its function-local constants continue the same .rodata (0x2EE020 .. 0x2EE248).
 *
 * One frame of thinking (AiThink_Think, called by BtlAiMgr_Update after sense 0x1BFF70):
 *   1. the plan cooldown is counted down;
 *   2. rule list 7 is evaluated; if that started an action (seq.depth != 0), or the level is negative, stop;
 *   3. rule list 4; if that started an action, stop;
 *   4. rule list 0 (its rules of kind 4 choose plan.next);
 *   5. rule list {1, 2, 3, 5, 6, 7}[plan.next] (plan.next starts every frame as 5, i.e. list 7 again).
 *
 * Evaluating a list (AiThink_EvalRules): rules are taken in file order. A rule is skipped unless the status
 * bit of its situation group is set (AiThStatus.flags, written by sense). The first time a group is met (it
 * differs from the group of the previous tested rule) eight rolls are drawn, one per condition position, each
 * Rand_Range(range) with an empty running sum. Conditions are tested left to right and stop at the first that
 * fails. "Weighted" conditions add their rate to the running sum of their position and pass when the roll is
 * below the sum: consecutive rules of one group therefore form a weighted choice with the rates as weights,
 * and `range` is the total of the weights the fighter can actually use (AiThink_BuildTotals), so that one of
 * them is always taken. The first rule that passes ends the evaluation: kind 0 does nothing, kind 4 sets
 * plan.next, any other kind pushes up to four actions (BtlAiSeq_PushRule).
 *
 * Condition ids: the byte in the rule is an id; D_002EDA70[id] is the index into the function table. The
 * weighted conditions do not look at their argument byte: they identify themselves by plan.cond (the id) and
 * look up their column in the weight table of the list in hand.
 */

extern s32 D_002EDA70[];       /* rule condition id -> index into the condition function table */
extern AiThCondFunc D_002C4768[]; /* condition function table */
extern u8 D_002EDC70[];        /* [14][8] condition code (id - 35) of each column of AiThRates.basic */
extern u8 D_002EDCE0[];        /* [14][8] condition code (id - 40) of each column of tblA */
extern u8 D_002EDD50[];        /* [14][8] ... of tblB */
extern u8 D_002EDDC0[];        /* [24] condition code (id - 40) of each byte of AiThRates.act */
extern u8 D_002EDDD8[];        /* [14][4] ... of tblC */
extern u8 D_002EDE10[];        /* [20][8] ... of tblD */
extern u8 D_002EDEB0[];        /* [14][4] ... of tblE */
extern u8 D_002EDEE8[];        /* [4][8] ... of tblF */

extern void *memset(void *dst, s32 c, u32 n);
extern char *strcpy(char *dst, const char *src);
extern AiThObj *BtlObj_Get(s32 id);
extern s32 BtlChar_IsStage4Or27(void);
extern s32 BtlCharApi_GetUnk974(s32 objId);
extern s32 BtlCharApi_TestFlag05(s32 objId);
extern u64 BtlCharApi_GetUnk1288(s32 objId);
extern s32 BtlAi_ScaleByLevel(s32 level, s32 lo, s32 hi);
extern void BtlAiMgr_SetType(s32 side, s32 aiType);
extern void BtlAiMgr_SetLevel(s32 side, s32 cpuLevel);
extern void BtlAiSeq_Reset(AiThSeq *seq);
extern void BtlAiSeq_PushRule(AiThWork *ai, AiThRule *rule);
extern s32 BtlAiSense_IsBehindOpponent(AiThWork *ai);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetPos(s32 objId, AiThVec *out);
extern f32 BtlCharApi_GetCloseRange(s32 objId);
extern f32 BtlCharApi_GetRadius(s32 objId);
extern s32 BtlCharApi_GetHp(s32 objId);
extern s32 BtlCharApi_HasParamBit80(s32 objId);
extern s32 BtlCharApi_GetCpuLevel(s32 objId);
extern s32 BtlCharApi_GetAiType(s32 objId);
extern s32 BtlCharApi_GetParamUnk14(s32 objId);
extern s32 BtlCharApi_GetParamUnk18(s32 objId);
extern s32 BtlCharApi_GetOppSkillClass(s32 objId);
extern s32 BtlCharApi_IsMoveFlag100(s32 objId, s32 slot);
extern s32 BtlCharApi_GetStunTimer(s32 objId);
extern s32 BtlCharApi_IsAnimFlag2800(s32 objId);
extern s32 BtlCharApi_TestPoseBit80(s32 objId, s32 arg);
extern s32 BtlCharApi_TestFlag98(s32 objId);
extern s32 BtlCharApi_GetUnkE40(s32 objId);
extern s32 BtlSide_GetKi(s32 objId);
extern s32 BtlSide_GetBlast(s32 objId);
extern s32 BtlSide_IsPoweredUp(s32 objId);

s32 AiThink_TestMove(AiThWork *ai);
s32 AiThink_TestSkill(AiThWork *ai);

/* The first half of the file uses the views of battle/btl_ai_int.h, this half those of battle/btl_ai_think.h
 * (the same memory described twice; the headers are not merged yet). Where both halves use a symbol, this half
 * goes through a cast. */
#define gBtlAi ((AiThMgr *)gBtlAi)
#define BtlAi_NoteOpponent(ai, react) BtlAi_NoteOpponent((BtlAiWork *)(ai), react)
#define BtlAi_GetLowGaugeFactor(ai) BtlAi_GetLowGaugeFactor((BtlAiWork *)(ai))
#define BtlAi_GetPairRate(ai, kind, off, lo, hi, byGauge) BtlAi_GetPairRate((BtlAiWork *)(ai), kind, off, lo, hi, byGauge)
#define BtlAi_GetQuadRate(ai, kind, off, lo, hi, byGauge) BtlAi_GetQuadRate((BtlAiWork *)(ai), kind, off, lo, hi, byGauge)
#define BtlCharApi_GetSkillTable(objId) ((AiThChrSkills *)BtlCharApi_GetSkillTable(objId))
#define BtlCharApi_GetMoveTable(objId) ((AiThChrMoves *)BtlCharApi_GetMoveTable(objId))
#define BtlCharApi_IsMoveSlotActive(objId, slot) (((s32 (*)(s32, s32))BtlCharApi_IsMoveSlotActive)(objId, slot))

#define PROFILE(ai) (gBtlAi->data->profile[(ai)->type])

/* Rate of column col of weight table 3 (20 rows of 8, row = plan.sub) for the level. */
s32 AiThink_GetSubRate(AiThWork *ai, s32 col, s8 *base_lo, s8 *base_hi) {
    s32 lo;
    s32 hi;
    s32 i;

    switch (ai->plan.sub) {
    case 0:
        lo = base_lo[col];
        hi = base_hi[col];
        break;
    case 1:
        lo = base_lo[col + 8];
        hi = base_hi[col + 8];
        break;
    case 2:
        i = col + 0x10;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 3:
        i = col + 0x10;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 4:
        i = col + 0x20;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 5:
        i = col + 0x20;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 6:
        i = col + 0x30;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 7:
        i = col + 0x30;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 8:
        i = col + 0x40;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 9:
        i = col + 0x40;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 10:
        i = col + 0x50;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 11:
        i = col + 0x50;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 12:
        i = col + 0x60;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 13:
        i = col + 0x60;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 14:
        i = col + 0x70;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 15:
        i = col + 0x70;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 16:
        i = col + 0x80;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 17:
        i = col + 0x80;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 18:
        i = col + 0x90;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 19:
        i = col + 0x90;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    default:
        return 0;
    }
    return BtlAi_ScaleByLevel(ai->level, lo, hi);
}

/* The same for weight table 5 (4 rows of 8). */
s32 AiThink_GetSubRate4(AiThWork *ai, s32 col, s8 *base_lo, s8 *base_hi) {
    s32 lo;
    s32 hi;
    s32 i;

    switch (ai->plan.sub) {
    case 0:
        lo = base_lo[col];
        hi = base_hi[col];
        break;
    case 1:
        lo = base_lo[col + 8];
        hi = base_hi[col + 8];
        break;
    case 2:
        i = col + 0x10;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 3:
        i = col + 0x10;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    default:
        return 0;
    }
    return BtlAi_ScaleByLevel(ai->level, lo, hi);
}

/* Column of the basic table that holds condition code (id - 35) for class cls: -1 none, -2 no class. */
s32 AiThink_FindBasicColumn(s32 cls, s32 code) {
    u8 *p;
    s32 i;

    if (cls == 14) {
        return -2;
    }
    p = &D_002EDC70[cls * 8];
    if (code < 0) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        if (code == *p++) {
            return i;
        }
    }
    return -1;
}

/* Basic weighted test: adds the rate of this condition's column to the position's sum and compares the roll. */
s32 AiThink_TestBasic(AiThWork *ai) {
    AiThPlan *plan = &ai->plan;
    AiThSeq *seq = &ai->seq;
    u8 *prof = PROFILE(ai);
    AiThRoll *r = &plan->rolls[plan->condNo];
    s32 col = AiThink_FindBasicColumn(plan->cls, plan->cond - 35);

    if (col == -2) {
        return 0;
    }
    if (col == -1) {
        return 0;
    }
    r->acc += BtlAi_GetPairRate(ai, plan->cls, col, (s8 *)prof + 4, (s8 *)prof + 0x2C4, 0);
    seq->roll = r->roll;
    seq->threshold = r->acc;
    if (r->roll < r->acc) {
        plan->unkA9 = r->roll;
        return 1;
    }
    return 0;
}

/* Condition function 38: the basic test, never with fighter flag 6 or while BtlCharApi_TestFlag98 is set. */
s32 AiThCond_BasicNoFlag6(AiThWork *ai, u8 arg) {
    if (BtlSide_IsPoweredUp(ai->objId) != 0) {
        return 0;
    }
    if (BtlCharApi_TestFlag98(ai->objId) != 0) {
        return 0;
    }
    return AiThink_TestBasic(ai);
}

/* Column of the current weight table (plan.next) that holds the condition code (id - 40); -1 = none.
 * (Each `lbu v, table(v)` only assembles as in the original, with its last instruction in the delay slot of the
 * following branch, because the tables are defined in this file, above.) */
s32 AiThink_FindWeightColumn(AiThPlan *plan) {
    s32 count[6] = { 8, 8, 4, 8, 4, 8 };
    s32 code = plan->cond - 40;
    s32 i;

    if (code < 0) {
        return -1;
    }
    for (i = 0; i < count[plan->next]; i++) {
        u8 v;

        switch (plan->next) {
        case 0:
            v = D_002EDCE0[i + plan->cls * 8];
            break;
        case 1:
            v = D_002EDD50[i + plan->cls * 8];
            break;
        case 2:
            v = D_002EDDD8[i + plan->cls * 4];
            break;
        case 3:
            v = D_002EDE10[i + plan->sub * 8];
            break;
        case 4:
            v = D_002EDEB0[i + plan->cls * 4];
            break;
        case 5:
            v = D_002EDEE8[i + plan->sub * 8];
            break;
        default:
            return -1;
        }
        if (code == v) {
            return i;
        }
    }
    return -1;
}

/* Weighted test: the rate of this condition's column in the current weight table, adjusted by the situation,
 * is added to the position's sum; passes when the pre-drawn roll is below the sum. */
s32 AiThink_TestWeighted(AiThWork *ai, s32 byGauge) {
    AiThPlan *plan = &ai->plan;
    AiThStatus *st = &ai->status;
    AiThSeq *seq = &ai->seq;
    u8 *prof = PROFILE(ai);
    AiThRoll *r = &plan->rolls[plan->condNo];
    s32 fn = D_002EDA70[plan->cond];
    AiThChrSkills *skills = BtlCharApi_GetSkillTable(ai->objId);
    s32 dist = BtlCharApi_GetHp(ai->objId);
    s32 near = BtlAi_ScaleByLevel(ai->level, prof[0x2AE], prof[0x56E]);
    s32 col = AiThink_FindWeightColumn(plan);
    s32 rate;

    if (col == -1) {
        return 0;
    }
    switch (plan->next) {
    case 0:
        rate = BtlAi_GetPairRate(ai, plan->cls, col, (s8 *)prof + 0x74, (s8 *)prof + 0x334, byGauge);
        break;
    case 1:
        rate = BtlAi_GetPairRate(ai, plan->cls, col, (s8 *)prof + 0xE4, (s8 *)prof + 0x3A4, byGauge);
        break;
    case 2:
        rate = BtlAi_GetQuadRate(ai, plan->cls, col, (s8 *)prof + 0x178, (s8 *)prof + 0x438, byGauge);
        break;
    case 3:
        rate = AiThink_GetSubRate(ai, col, (s8 *)prof + 0x1B0, (s8 *)prof + 0x470);
        break;
    case 4:
        rate = BtlAi_GetQuadRate(ai, plan->cls, col, (s8 *)prof + 0x250, (s8 *)prof + 0x510, byGauge);
        break;
    case 5:
        rate = AiThink_GetSubRate4(ai, col, (s8 *)prof + 0x288, (s8 *)prof + 0x548);
        break;
    default:
        return 0;
    }
    if (st->flags & 0x3000000000000000) {
        if (fn == 0x31 || fn == 0x51) {
            rate = (s32)((f32)rate * 0.5f);
        }
        if (st->flags & 0x2000000000000000) {
            if (fn == 0x33 || fn == 0x5E) {
                rate = 0;
            }
            if ((u32)(fn - 0x5F) < 2) {
                if (plan->slot == -1) {
                    return 0;
                }
                if (!(skills->flags[plan->slot] & 0x8000000)) {
                    rate = (s32)((f32)rate * 0.5f);
                }
            }
        }
    }
    if (dist < 10000) {
        if (near != 0 && fn == 0x5D) {
            rate = near;
        }
    }
    r->acc += rate;
    seq->roll = r->roll;
    seq->threshold = r->acc;
    return r->roll < r->acc;
}

/* Conditions 92..100 (functions 85..93): picks a usable move slot of kind (id - 92). The weighted test decides
 * whether to act; with both slots usable one is drawn at random. */
s32 AiThink_TestMove(AiThWork *ai) {
    s32 list[4];
    AiThPlan *plan = &ai->plan;
    s32 count = 0;
    AiThChrMoves *moves = BtlCharApi_GetMoveTable(ai->objId);
    s32 stock = BtlSide_GetBlast(ai->objId) / 100000;
    s32 kind = plan->cond - 0x5C;
    s32 dist = BtlCharApi_GetHp(ai->objId);
    s32 oppVal = BtlCharApi_GetStunTimer(TagTeam_Target(ai->objId));
    s32 i;
    s32 blocked;

    if ((u32)kind >= 9) {
        return 0;
    }
    plan->slot = -1;
    for (i = 0; i < 2; i++) {
        if (kind != moves->kind[i]) {
            continue;
        }
        if (plan->cooldown > 0) {
            continue;
        }
        blocked = 0;
        switch (kind) {
        case 0:
            if (moves->id[i] == 1) {
                if (BtlCharApi_TestFlag05(TagTeam_Target(ai->objId)) == 0) {
                    blocked = 1;
                }
            } else if (!(oppVal < 0x1F)) {
                blocked = 1;
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            break;
        case 5:
            if (BtlCharApi_IsMoveFlag100(ai->objId, i) == 0) {
                if (BtlCharApi_IsMoveSlotActive(ai->objId, i) != 0) {
                    blocked = 1;
                }
            }
            break;
        case 6:
            blocked = BtlSide_IsPoweredUp(ai->objId) != 0;
            break;
        case 7:
        case 8:
            break;
        }
        if (blocked) {
            continue;
        }
        if (moves->id[i] == 0x17 && dist < 10000) {
            continue;
        }
        if (stock < moves->stock[i]) {
            continue;
        }
        list[count++] = i;
        plan->slot = i;
    }
    if (count == 0) {
        return 0;
    }
    if (AiThink_TestWeighted(ai, 0) == 0) {
        return 0;
    }
    if (count == 1) {
        plan->slot = list[0];
    } else {
        plan->slot = list[Rand_Range(count)];
    }
    return 1;
}

/* Conditions 101..107 (functions 94..100) and 113, 114 (106, 107): picks a usable skill slot, then the weighted
 * test. Slots are scanned from the last (2 only with fighter flag 6) to the first. */
/* Matching notes: the slot test for condition 114 is written as two compares (`!= 1 && != 2`): the range form
 * `(u8)kind - 1 >= 2` is one RTL instruction shorter, which makes the SECOND loop pass (133 against 132 insns)
 * strength-reduce `&list[count]` and changes every reload register. The opponent's power is read first in both
 * power compares. Dead code kept from the original: `strict == 0` implies `oppSlot != -1`, so the
 * `skills->kind[i] != 3` arm is never reached, and `n - 1` is never negative. Differential test against the
 * original bytes: build/scratch_cleanup3_S/t_testskill.py. */
s32 AiThink_TestSkill(AiThWork *ai) {
    s32 list[4];
    s32 gauge;
    AiThPlan *plan;
    s32 kind;
    s32 count;
    s32 anyBasic;
    s32 beatOpp;
    s32 dist;
    s32 strict;
    AiThChrSkills *skills;
    AiThChrSkills *opp;
    s32 oppSlot;
    s32 n;
    s32 i;

    n = 2;
    plan = &ai->plan;
    count = 0;
    anyBasic = 0;
    skills = BtlCharApi_GetSkillTable(ai->objId);
    beatOpp = 0;
    strict = 1;
    gauge = BtlSide_GetKi(ai->objId);
    kind = plan->cond - 0x65;
    dist = BtlCharApi_GetHp(ai->objId);
    oppSlot = BtlCharApi_GetOppSkillClass(ai->objId);
    opp = BtlCharApi_GetSkillTable(TagTeam_Target(ai->objId));
    if (BtlCharApi_GetUnkE40(ai->objId) != 0) {
        return 0;
    }
    if (D_002EDA70[plan->cond] == 0x6B) {
        anyBasic = 1;
    } else if (D_002EDA70[plan->cond] == 0x6A) {
        beatOpp = 1;
        if (oppSlot != -1) {
            if (opp->state[oppSlot] & 0x100) {
                strict = 0;
            } else {
                strict = 1;
            }
        }
    }
    if ((u32)kind >= 7 && anyBasic == 0 && beatOpp == 0) {
        return 0;
    }
    plan->slot = -1;
    if (D_002EDA70[plan->cond] == 0x5E) {
        if (ai->range.dist[0] * 1.5f < gBtlAi->distance) {
            return 0;
        }
    }
    if (BtlSide_IsPoweredUp(ai->objId) != 0) {
        n = 3;
    }
    for (i = n - 1; i >= 0; i--) {
        if (anyBasic != 0) {
            if (skills->kind[i] != 1 && skills->kind[i] != 2) {
                continue;
            }
        } else if (beatOpp != 0) {
            if (strict == 1) {
                if (skills->state[i] & 0x100) {
                    continue;
                }
                if (oppSlot != -1) {
                    if (opp->power[oppSlot] > skills->power[i]) {
                        continue;
                    }
                    if (opp->power[oppSlot] == skills->power[i] && skills->rank[i] < opp->rank[oppSlot]) {
                        continue;
                    }
                }
            } else if (oppSlot != -1) {
                if (opp->power[oppSlot] > skills->power[i]) {
                    continue;
                }
            } else if (skills->kind[i] != 3) {
                continue;
            }
        } else if (kind != skills->kind[i]) {
            continue;
        }
        if (skills->kind[i] == 5 && BtlChar_IsStage4Or27() != 0) {
            if (BtlCharApi_TestPoseBit80(ai->objId, 1) == 0) {
                continue;
            }
            if (BtlCharApi_TestPoseBit80(TagTeam_Target(ai->objId), 1) == 0) {
                continue;
            }
        }
        if ((skills->flags[i] & 0x2000) || skills->unk18[i] == 0x280) {
            if (dist > 10000) {
                continue;
            }
        }
        if (gauge < skills->cost[i]) {
            continue;
        }
        list[count++] = i;
    }
    if (count == 0) {
        return 0;
    }
    if (count == 1) {
        plan->slot = list[0];
    }
    if (count >= 2) {
        if (list[0] == 2 && (s32)Rand_Range(100) < 50) {
            plan->slot = list[0];
        } else {
            plan->slot = list[Rand_Range(count)];
        }
    }
    if (AiThink_TestWeighted(ai, BtlSide_IsPoweredUp(ai->objId)) == 0) {
        plan->slot = -1;
        return 0;
    }
    return 1;
}

/* Condition function 40: the weighted test while bit 0 of BtlCharApi_GetParamUnk14 is set. */
s32 AiThCond_WeightedIfBit0(AiThWork *ai, u8 arg) {
    if (BtlCharApi_GetParamUnk14(ai->objId) & 1) {
        return AiThink_TestWeighted(ai, 0);
    }
    return 0;
}

/* Condition function 77: misc rate 0x10 (arg 0) or 0x13 (arg 1); on stage 4 / 27 only when the opponent's
 * BtlCharApi_TestPoseBit80(1) is set. */
s32 AiThCond_MiscRate0Or3(AiThWork *ai, u8 arg) {
    AiThSeq *seq = &ai->seq;
    u8 *prof = PROFILE(ai);
    u8 *lo = prof + 0x2B8;
    u8 *hi = prof + 0x578;
    s32 roll = Rand_Range(100);
    s32 stage = BtlChar_IsStage4Or27();
    s32 chance;

    switch (arg) {
    case 0:
        chance = BtlAi_ScaleByLevel(ai->level, lo[0], hi[0]);
        break;
    case 1:
        chance = BtlAi_ScaleByLevel(ai->level, lo[3], hi[3]);
        break;
    default:
        chance = 0;
        break;
    }
    if (stage != 0 && BtlCharApi_TestPoseBit80(TagTeam_Target(ai->objId), 1) == 0) {
        return 0;
    }
    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition function 78: misc rate 0x11. */
s32 AiThCond_MiscRate1(AiThWork *ai, u8 arg) {
    u8 *prof = PROFILE(ai);
    s32 chance = BtlAi_ScaleByLevel(ai->level, prof[0x2B9], prof[0x579]);
    s32 roll = Rand_Range(100);
    AiThSeq *seq = &ai->seq;

    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition function 79: misc rate 0x12. */
s32 AiThCond_MiscRate2(AiThWork *ai, u8 arg) {
    u8 *prof = PROFILE(ai);
    s32 chance = BtlAi_ScaleByLevel(ai->level, prof[0x2BA], prof[0x57A]);
    s32 roll = Rand_Range(100);
    AiThSeq *seq = &ai->seq;

    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition function 80: misc rate 0x14. */
s32 AiThCond_MiscRate4(AiThWork *ai, u8 arg) {
    u8 *prof = PROFILE(ai);
    s32 chance = BtlAi_ScaleByLevel(ai->level, prof[0x2BC], prof[0x57C]);
    s32 roll = Rand_Range(100);
    AiThSeq *seq = &ai->seq;

    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition function 108: misc rate 0xC; raises reaction bit 0x4000. */
s32 AiThCond_React4000(AiThWork *ai, u8 arg) {
    AiThSeq *seq = &ai->seq;
    u8 *prof = PROFILE(ai);
    s32 chance = BtlAi_ScaleByLevel(ai->level, prof[0x2B4], prof[0x574]);
    s32 roll = Rand_Range(100);

    BtlAi_NoteOpponent(ai, 0x4000);
    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition function 109: misc rate 0xD; raises reaction bit 0x8000. */
s32 AiThCond_React8000(AiThWork *ai, u8 arg) {
    AiThSeq *seq = &ai->seq;
    u8 *prof = PROFILE(ai);
    s32 chance = BtlAi_ScaleByLevel(ai->level, prof[0x2B5], prof[0x575]);
    s32 roll = Rand_Range(100);

    BtlAi_NoteOpponent(ai, 0x8000);
    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition functions 35, 36, 37, 39: the basic test. */
s32 AiThCond_Basic(AiThWork *ai, u8 arg) {
    return AiThink_TestBasic(ai);
}

/* Condition functions 41..53, 81, 82, 101..105, 110..113, 116: the weighted test. */
s32 AiThCond_Weighted(AiThWork *ai, u8 arg) {
    return AiThink_TestWeighted(ai, 0);
}

/* Condition functions 54, 55, 83, 84: the weighted test, gated by the height difference to the opponent. */
s32 AiThCond_WeightedByHeight(AiThWork *ai, u8 arg) {
    AiThStatus *st = &ai->status;
    AiThPlan *plan = &ai->plan;
    f32 height = BtlCharApi_GetHeight(ai->objId);
    AiThVec pos;
    AiThVec opp;
    f32 dy;

    BtlCharApi_GetPos(ai->objId, &pos);
    BtlCharApi_GetPos(TagTeam_Target(ai->objId), &opp);
    dy = opp.y - pos.y;
    switch (D_002EDA70[plan->cond]) {
    case 0x36:
    case 0x53:
        if (opp.y < pos.y - height) {
            return 0;
        }
        if (!(st->flags & 0x30)) {
            return 0;
        }
        break;
    case 0x54:
        if (opp.y < pos.y - height) {
            return 0;
        }
        if (!(st->flags & 0x20)) {
            return 0;
        }
        break;
    case 0x37:
        if (BtlCharApi_IsAnimFlag2800(TagTeam_Target(ai->objId)) == 0) {
            return 0;
        }
        if (gBtlAi->distance > gBtlAi->radiusSum + BtlCharApi_GetRadius(ai->objId)) {
            return 0;
        }
        if ((dy < 0.0f ? -dy : dy) > height) {
            return 0;
        }
        if (BtlAiSense_IsBehindOpponent(ai) != 0) {
            return 0;
        }
        break;
    }
    return AiThink_TestWeighted(ai, 0);
}

/* Condition functions 56..76: a level-scaled rate (AiThRates.act), then a test of the fighter's own state word
 * (fighter +0x1288) that depends on the condition. */
s32 AiThCond_ActRateByFlags(AiThWork *ai, u8 arg) {
    s32 idx = -1;
    AiThSeq *seq = &ai->seq;
    AiThPlan *plan = &ai->plan;
    s32 roll = Rand_Range(100);
    AiThProfile *prof = (AiThProfile *)PROFILE(ai);
    s32 code = plan->cond - 40;
    u64 f = BtlCharApi_GetUnk1288(ai->objId);
    AiThActTable *act = gBtlAi->data->act;
    s32 action[2];
    s8 cls[2];
    s32 chance;
    s32 i;

    if (code < 0) {
        return 0;
    }
    for (i = 0; i < 24; i++) {
        if (code == D_002EDDC0[i]) {
            idx = i;
        }
    }
    if (idx == -1) {
        return 0;
    }
    chance = BtlAi_ScaleByLevel(ai->level, prof->lo.act[idx], prof->hi.act[idx]);
    seq->roll = roll;
    seq->threshold = chance;
    if (chance < roll) {
        return 0;
    }
    action[0] = BtlCharApi_GetUnk974(ai->objId);
    action[1] = BtlCharApi_GetUnk974(TagTeam_Target(ai->objId));
    cls[0] = ((AiThActBody *)((u8 *)act + 8))->actClass[action[0]];
    cls[1] = ((AiThActBody *)((u8 *)act + 8))->actClass[action[1]];
    switch (D_002EDA70[plan->cond]) {
    case 0x38:
        if (!(f & 0x80)) {
            return 0;
        }
        break;
    case 0x39:
        if (!(f & 0x100)) {
            return 0;
        }
        break;
    case 0x3A:
        if (!(f & 0x10000000)) {
            return 0;
        }
        break;
    case 0x3B:
        if (!(f & 4)) {
            return 0;
        }
        break;
    case 0x3C:
        if (!(f & 0x10)) {
            return 0;
        }
        break;
    case 0x3D:
        if (!(f & 0x20000000)) {
            return 0;
        }
        break;
    case 0x3F:
        if (!(f & 0x80000000)) {
            return 0;
        }
        break;
    case 0x49:
        if (!(f & 0x100000000)) {
            return 0;
        }
        break;
    case 0x40:
    case 0x41:
        if (!(f & 8)) {
            return 0;
        }
        break;
    case 0x42:
        if (!(f & 0x200000000)) {
            return 0;
        }
        break;
    case 0x43:
        if (!(f & 0x400000000)) {
            return 0;
        }
        break;
    case 0x44:
        if (!(f & 2)) {
            return 0;
        }
        break;
    case 0x45:
        if (!(f & 1)) {
            return 0;
        }
        break;
    case 0x46:
        if (!(f & 0x20)) {
            return 0;
        }
        break;
    case 0x47:
        if ((f & 0x80000) && cls[0] == 0x10) {
            break;
        }
        return 0;
    case 0x48:
        if (!(f & 0x40)) {
            return 0;
        }
        break;
    case 0x4A:
        if (!(f & 0x1000000000)) {
            return 0;
        }
        break;
    case 0x4B:
        if (!(f & 0x800000000)) {
            return 0;
        }
        break;
    default:
        return 0;
    case 0x3E:
        if ((f & 0x80) && cls[1] == 3) {
            break;
        }
        return 0;
    }
    return 1;
}

/* The rate part of the same for condition id cond, without the trace and the state test. Called from
 * BtlAiSeq_RollPowerUpChain (at 0x1B4ACC). */
s32 AiThink_RollActRate(AiThWork *ai, s32 cond) {
    s32 code = cond - 40;
    s32 roll = Rand_Range(100);
    AiThProfile *prof = (AiThProfile *)PROFILE(ai);
    s32 idx = -1;
    s32 i;

    for (i = 0; i < 24; i++) {
        if (code == D_002EDDC0[i]) {
            idx = i;
        }
    }
    if (idx == -1) {
        return 0;
    }
    return roll < BtlAi_ScaleByLevel(ai->level, prof->lo.act[idx], prof->hi.act[idx]);
}

/* Condition functions 114, 115, 117: the weighted test, only with fighter flag 6 and one ability bit. */
s32 AiThCond_WeightedIfAbility(AiThWork *ai, u8 arg) {
    s32 ab = BtlCharApi_GetParamUnk18(ai->objId);
    s32 fn = D_002EDA70[ai->plan.cond];

    if (BtlSide_IsPoweredUp(ai->objId) == 0) {
        return 0;
    }
    switch (fn) {
    case 0x73:
        if (ab & 0x200) {
            break;
        }
        return 0;
    case 0x72:
        if (ab & 0x10) {
            break;
        }
        return 0;
    case 0x75:
        if (!(ab & 8)) {
            return 0;
        }
        break;
    }
    return AiThink_TestWeighted(ai, 0);
}

/* Condition functions 85..93: pick a usable move slot. */
s32 AiThCond_Move(AiThWork *ai, u8 arg) {
    return AiThink_TestMove(ai);
}

/* Condition functions 94..100, 106, 107: pick a usable skill slot. */
s32 AiThCond_Skill(AiThWork *ai, u8 arg) {
    return AiThink_TestSkill(ai);
}

/* Class (row of the per-class tables) of a situation group; 14 = the group has none. */
s32 AiThink_GetGroupClass(s32 group) {
    switch (group) {
    case 7:
        return 0;
    case 8:
        return 1;
    case 9:
        return 2;
    case 10:
        return 3;
    case 11:
        return 4;
    case 12:
        return 5;
    case 14:
        return 6;
    case 16:
        return 7;
    case 17:
        return 8;
    case 18:
        return 9;
    case 19:
        return 10;
    case 20:
        return 11;
    case 23:
        return 12;
    case 24:
        return 13;
    default:
        return 14;
    }
}

/* Whether one of the fighter's two move slots has this kind. */
s32 AiThink_HasMoveKind(AiThWork *ai, s32 kind) {
    AiThChrMoves *moves = BtlCharApi_GetMoveTable(ai->objId);
    s32 i;
    s8 *p;

    p = moves->kind;
    for (i = 0; i < 2; i++) {
        if (kind == *p++) {
            return 1;
        }
    }
    return 0;
}

/* Whether one of the fighter's three skill slots has this kind. */
s32 AiThink_HasSkillKind(AiThWork *ai, s32 kind) {
    AiThChrSkills *skills = BtlCharApi_GetSkillTable(ai->objId);
    s32 i;
    s8 *p;

    p = skills->kind;
    for (i = 0; i < 3; i++) {
        if (kind == *p++) {
            return 1;
        }
    }
    return 0;
}

/* Whether the fighter's ability word (BtlCharApi_GetParamUnk18) has one of the bits. */
s32 AiThink_HasAbility(AiThWork *ai, s32 mask) {
    return (BtlCharApi_GetParamUnk18(ai->objId) & mask) != 0;
}

/* Whether this fighter can ever do what a condition code (id - 40) stands for. */
s32 AiThink_IsCodeUsable(AiThWork *ai, s32 code) {
    s32 r = BtlCharApi_GetParamUnk14(ai->objId) & 1;

    if (code == 0) {
        return r;
    }
    if ((u32)(code - 0x34) < 9) {
        return AiThink_HasMoveKind(ai, code - 0x34);
    }
    if ((u32)(code - 0x3D) < 7) {
        return AiThink_HasSkillKind(ai, code - 0x3D);
    }
    if (code == 0x49) {
        return AiThink_HasSkillKind(ai, 3);
    }
    if (code == 0x4A) {
        return AiThink_HasSkillKind(ai, 1) | AiThink_HasSkillKind(ai, 2);
    }
    switch (code) {
    case 0x52:
        return AiThink_HasAbility(ai, 0x10);
    case 0x53:
        return AiThink_HasAbility(ai, 0x200);
    case 0x54:
        return AiThink_HasAbility(ai, 0x20);
    case 0x55:
        return AiThink_HasAbility(ai, 8);
    }
    return 1;
}

/* Sum over the usable columns of row cls of a pair table (8 columns): the range of the roll for that class. */
s32 AiThink_SumPairRates(AiThWork *ai, u32 cls, s8 *lo, s8 *hi, u8 *codes) {
    s32 sum = 0;
    s32 c;
    s32 i;

    for (c = 0; c < 8; c++) {
        if (AiThink_IsCodeUsable(ai, (&codes[cls * 8])[c]) != 0) {
            switch (cls) {
            case 0:
                sum += BtlAi_ScaleByLevel(ai->level, lo[c], hi[c]);
                break;
            case 1:
                sum += BtlAi_ScaleByLevel(ai->level, lo[c + 8], hi[c + 8]);
                break;
            case 2:
                i = c + 0x10;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 3:
                i = c + 0x10;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            case 4:
                i = c + 0x20;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 5:
                i = c + 0x20;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            case 6:
                i = c + 0x30;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 7:
                i = c + 0x30;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            case 8:
                i = c + 0x40;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 9:
                i = c + 0x40;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            case 10:
                i = c + 0x50;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 11:
                i = c + 0x50;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            case 12:
                i = c + 0x60;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 13:
                i = c + 0x60;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            }
        }
    }
    return sum;
}

/* The same for a quad table (4 columns). */
s32 AiThink_SumQuadRates(AiThWork *ai, u32 cls, s8 *lo, s8 *hi, u8 *codes) {
    s32 sum = 0;
    s32 c;
    s32 i;

    for (c = 0; c < 4; c++) {
        if (AiThink_IsCodeUsable(ai, (&codes[cls * 4])[c]) != 0) {
            switch (cls) {
            case 0:
                sum += BtlAi_ScaleByLevel(ai->level, lo[c], hi[c]);
                break;
            case 1:
                sum += BtlAi_ScaleByLevel(ai->level, lo[c + 4], hi[c + 4]);
                break;
            case 2:
                sum += BtlAi_ScaleByLevel(ai->level, lo[c + 8], hi[c + 8]);
                break;
            case 3:
                sum += BtlAi_ScaleByLevel(ai->level, lo[c + 12], hi[c + 12]);
                break;
            case 4:
                i = c + 0x10;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 5:
                i = c + 0x10;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 4], hi[i + 4]);
                break;
            case 6:
                i = c + 0x10;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            case 7:
                i = c + 0x10;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 12], hi[i + 12]);
                break;
            case 8:
                i = c + 0x20;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 9:
                i = c + 0x20;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 4], hi[i + 4]);
                break;
            case 10:
                i = c + 0x20;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 8], hi[i + 8]);
                break;
            case 11:
                i = c + 0x20;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 12], hi[i + 12]);
                break;
            case 12:
                i = c + 0x30;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i], hi[i]);
                break;
            case 13:
                i = c + 0x30;
                sum += BtlAi_ScaleByLevel(ai->level, lo[i + 4], hi[i + 4]);
                break;
            }
        }
    }
    return sum;
}

/* The same for row sub of weight table 5. */
s32 AiThink_SumSubRates(AiThWork *ai, s32 sub) {
    s32 sum = 0;
    s32 c;
    s32 i;
    u8 *codes = &D_002EDEE8[sub * 8];
    u8 *prof = PROFILE(ai);
    s8 *lo0 = (s8 *)prof + 0x288;
    s8 *hi0 = (s8 *)prof + 0x548;
    s8 *lo1 = (s8 *)prof + 0x290;
    s8 *hi1 = (s8 *)prof + 0x550;

    i = 0x10;
    for (c = 0; c < 8; c++, i++, codes++) {
        if (AiThink_IsCodeUsable(ai, *codes) != 0) {
            switch (sub) {
            case 0:
                sum += BtlAi_ScaleByLevel(ai->level, lo0[c], hi0[c]);
                break;
            case 1:
                sum += BtlAi_ScaleByLevel(ai->level, lo1[c], hi1[c]);
                break;
            case 2:
                sum += BtlAi_ScaleByLevel(ai->level, lo0[i], hi0[i]);
                break;
            case 3:
                sum += BtlAi_ScaleByLevel(ai->level, lo1[i], hi1[i]);
                break;
            }
        }
    }
    return sum;
}

/* Fills plan.total (rows 0..2 by class, row 3 by sub) for the fighter's kit and level. Called when the type or
 * level is set. The table is reached through a `plan` pointer: this compiler splits every field offset into a
 * multiple of 16 and a rest, so `ai->plan.total[..]` and `plan->total[..]` give different base constants (the
 * original's work + 0x2F1 is plan + 9, the rest of total's offset 0x69). */
void AiThink_BuildTotals(AiThWork *ai) {
    AiThPlan *plan = &ai->plan;
    u8 *prof = PROFILE(ai);
    s32 i;
    s32 j;

    for (i = 0; i < 14; i++) {
        plan->total[0][i] = AiThink_SumPairRates(ai, i, (s8 *)prof + 0x74, (s8 *)prof + 0x334, D_002EDCE0);
        plan->total[1][i] = AiThink_SumPairRates(ai, i, (s8 *)prof + 0xE4, (s8 *)prof + 0x3A4, D_002EDD50);
        plan->total[2][i] = AiThink_SumQuadRates(ai, i, (s8 *)prof + 0x178, (s8 *)prof + 0x438, D_002EDDD8);
    }
    for (j = 0; j < 4; j++) {
        plan->total[3][j] = AiThink_SumSubRates(ai, j);
    }
}

/* Range of the rolls for the rule list in hand (tbl = list - 1, or 3 for list 7): the total of the usable rates,
 * 100 when that is zero. */
s32 AiThink_GetRollRange(AiThWork *ai, s32 tbl) {
    AiThPlan *plan = &ai->plan;
    s32 idx;
    s8 total;
    s32 dist;
    s32 near;

    if (tbl != 3) {
        idx = plan->cls;
    } else {
        idx = plan->sub;
    }
    total = plan->total[tbl][idx];
    dist = BtlCharApi_GetHp(ai->objId);
    near = BtlAi_ScaleByLevel(ai->level, PROFILE(ai)[0x2AE], PROFILE(ai)[0x56E]);
    if (total == 0) {
        return 100;
    }
    if (near == 0) {
        return total;
    }
    if (tbl != 1) {
        return total;
    }
    switch ((u32)plan->cls) {
    case 8:
    case 9:
    case 13:
        break;
    default:
        return total;
    }
    if (dist < 10001) {
        return total + near;
    }
    return total;
}

/* Range of the rolls for a group whose first condition is function 38: 100 stretched by the low-gauge factor. */
s32 AiThink_GetCond38Range(AiThWork *ai) {
    f32 k = BtlAi_GetLowGaugeFactor(ai);
    AiThPlan *plan = &ai->plan;
    f32 rate;

    if (k == 1.0f) {
        return 100;
    }
    rate = (f32)ai->plan.rate[plan->cls];
    return (s32)(rate * k - rate) + 100;
}

/* Gate of the groups 0x1D, 0x24 and 0x25: 1 (skip the group this frame) unless a roll is under the summed
 * rates of row plan.sub of weight table 5. */
/* plan.sub is read into one function-scope variable for both switches (two separate `switch (plan->sub)` give
 * the other register in the first one). AiThink_EvalRules below needs this definition in front of it. */
s32 AiThink_RollGroupGate(AiThWork *ai) {
    AiThPlan *plan = &ai->plan;
    s32 sum = 0;
    s32 c = 0;
    s32 i;
    s32 sub;
    s32 roll = Rand_Range(100);
    u8 *prof = PROFILE(ai);
    s8 *lo0 = (s8 *)prof + 0x288;
    s8 *hi0 = (s8 *)prof + 0x548;
    s8 *lo1 = (s8 *)prof + 0x290;
    s8 *hi1 = (s8 *)prof + 0x550;

    for (; c < 8; c++) {
        sub = plan->sub;
        switch (sub) {
        case 0:
            sum += BtlAi_ScaleByLevel(ai->level, lo0[c], hi0[c]);
            break;
        case 1:
            sum += BtlAi_ScaleByLevel(ai->level, lo1[c], hi1[c]);
            break;
        case 3:
            i = c + 0x10;
            sum += BtlAi_ScaleByLevel(ai->level, lo1[i], hi1[i]);
            break;
        default:
            return 0;
        }
    }
    if (roll < sum) {
        return 0;
    }
    sub = plan->sub;
    switch (sub) {
    case 0:
        BtlAi_NoteOpponent(ai, 0x200);
        break;
    case 1:
        BtlAi_NoteOpponent(ai, 0x800);
        break;
    case 3:
        BtlAi_NoteOpponent(ai, 0x1000);
        break;
    }
    return 1;
}

/* Evaluates one rule list: the first rule whose situation group is active and whose conditions all pass is
 * executed, and evaluation stops there (except that a rule with no conditions never passes). */
/* The order of the three stores in front of BtlAiSeq_PushRule (firedRule, firedGroup, firedSet) decides whether
 * the argument registers are already loaded when the store of `n` is reloaded, and with that which registers
 * reload uses for spilled values in the WHOLE function (a2 instead of a1). */
void AiThink_EvalRules(AiThWork *ai, AiThRuleList *list) {
    s32 res[8];
    AiThSeq *seq = &ai->seq;
    s32 n;
    s32 lastGroup = -1;
    AiThStatus *st = &ai->status;
    AiThPlan *plan = &ai->plan;
    s32 skipGroup = -1;
    AiThRule *rule;
    AiThRoll *r;
    s32 i;
    s32 pass;
    s32 range;

    for (n = 0; n < list->count; n++) {
        plan->ruleNo = n;
        rule = &list->rules[n];
        if (skipGroup == rule->group) {
            continue;
        }
        if (!((st->flags >> rule->group) & 1)) {
            continue;
        }
        plan->cls = AiThink_GetGroupClass(rule->group);
        if (rule->group == 0x1D) {
            plan->sub = 0;
        } else if (rule->group == 0x24) {
            plan->sub = 1;
        } else if (rule->group == 0x26) {
            plan->sub = 2;
        } else if (rule->group == 0x25) {
            plan->sub = 3;
        }
        if (lastGroup != rule->group) {
            if (rule->group == 0x1D || rule->group == 0x24 || rule->group == 0x25) {
                if (AiThink_RollGroupGate(ai) != 0) {
                    skipGroup = rule->group;
                    continue;
                }
            }
            for (i = 0; i < 8; i++) {
                range = 100;
                res[i] = 0;
                if ((u32)(seq->ruleSet - 1) < 3) {
                    range = AiThink_GetRollRange(ai, seq->ruleSet - 1);
                } else if (seq->ruleSet == 7) {
                    if (plan->sub >= 0) {
                        if (plan->sub < 4) {
                            range = AiThink_GetRollRange(ai, 3);
                        }
                    }
                }
                if (D_002EDA70[rule->cond[0]] == 0x26) {
                    range = AiThink_GetCond38Range(ai);
                }
                plan->range = range;
                r = &plan->rolls[i];
                r->roll = Rand_Range(range);
                r->acc = 0;
            }
        }
        lastGroup = rule->group;
        pass = 0;
        for (i = 0; i < 8; i++) {
            if (rule->cond[i] == AITH_COND_SAME) {
                pass = res[i];
                if (pass == 0) {
                    break;
                }
                continue;
            }
            if (rule->cond[i] == AITH_COND_END) {
                break;
            }
            pass = rule->cond[i];
            plan->condNo = i;
            plan->cond = pass;
            pass = D_002C4768[D_002EDA70[pass]](ai, rule->arg[i]);
            res[i] = pass;
            if (pass == 0) {
                goto next;
            }
        }
        if (pass == 0) {
            continue;
        }
        if (rule->kind == 0) {
            return;
        }
        if (rule->kind == 4) {
            s32 cls = plan->cls;
            s32 next = rule->param - 0x7F;

            plan->next = next;
            plan->unkAA = rule->group;
            if (cls == 14) {
                return;
            }
            plan->nextCount[cls][next]++;
            plan->clsCount[plan->cls]++;
            return;
        }
        if (rule->group == 0x24) {
            BtlAi_NoteOpponent(ai, 0x800);
        } else if (rule->group == 0x25) {
            BtlAi_NoteOpponent(ai, 0x1000);
        } else if (rule->group == 0x26) {
            BtlAi_NoteOpponent(ai, 0x2000);
        }
        seq->firedRule = n;
        seq->firedGroup = rule->group;
        seq->firedSet = seq->ruleSet;
        BtlAiSeq_PushRule(ai, rule);
        return;
    next:;
    }
}

/* For the fighter's own action: 1 when its class is 15; otherwise, for the actions 0x3C..0x3F, the step to
 * continue at (action - 0x3A with sequence flag 0x100, else action - 0x3B); 0 for anything else. Called by step
 * handler BtlAiPick_ComboBranch (the call is at 0x1B5098). */
/* The biased action id is a local set before the class test (as written, the range test's subtraction is
 * computed in front of the first branch and its compare lands in that branch's delay slot). */
s32 AiThink_GetBlastStep(AiThWork *ai) {
    AiThActTable *act = gBtlAi->data->act;
    s32 action = BtlCharApi_GetUnk974(ai->objId);
    AiThSeq *seq = &ai->seq;
    s32 k = action - 0x3C;

    if (act->actClass[action] == 15) {
        return 1;
    }
    if ((u32)k >= 4) {
        return 0;
    }
    if (seq->flags & 0x100) {
        return action - 0x3A;
    }
    return action - 0x3B;
}

/* Counts the plan cooldown down. */
void AiThink_TickCooldown(AiThWork *ai) {
    AiThPlan *plan = &ai->plan;

    if (plan->cooldown > 0) {
        plan->cooldown--;
    }
}

/* Once per frame per CPU fighter, after sense: runs the rule lists until one starts an action. */
void AiThink_Think(AiThWork *ai) {
    AiThSeq *seq = &ai->seq;
    AiThData *data = gBtlAi->data;
    s32 next[6] = { 1, 2, 3, 5, 6, 7 };
    AiThPlan *plan;

    AiThink_TickCooldown(ai);
    plan = &ai->plan;
    seq->ruleSet = 7;
    plan->next = 5;
    AiThink_EvalRules(ai, data->rules[seq->ruleSet]);
    if (seq->depth != 0) {
        return;
    }
    if (ai->level < 0) {
        return;
    }
    seq->ruleSet = 4;
    AiThink_EvalRules(ai, data->rules[4]);
    if (seq->depth != 0) {
        return;
    }
    seq->ruleSet = 0;
    AiThink_EvalRules(ai, data->rules[0]);
    seq->ruleSet = next[plan->next];
    AiThink_EvalRules(ai, data->rules[seq->ruleSet]);
}

/* Points the manager at the AI data (member 4 of common file 2) and fills in its section pointers. The owned
 * variant (a separately loaded copy) has no loader left in this build and does nothing. */
void AiThink_BindData(s32 owned) {
    u8 *p = NULL;
    AiThData *data;
    s32 i;

    if (owned != 1) {
        u32 *file = gCommonRes->data[0];

        p = (u8 *)file + ((file[4] >> 2) << 2);
    }
    if (p == NULL) {
        return;
    }
    data = (AiThData *)p;
    gBtlAi->frame = 0;
    gBtlAi->own &= ~1;
    p = (u8 *)data + 0x148;
    data->act = (AiThActTable *)p;
    ((AiThActTable *)p)->unk4 = (u8 *)data + 0x8D0;
    p += data->size[0];
    for (i = 0; i < 8; i++) {
        data->rules[i] = (AiThRuleList *)p;
        ((AiThRuleList *)p)->rules = (AiThRule *)(p + 8);
        p += data->size[1 + i];
    }
    for (i = 0; i < 32; i++) {
        data->profile[i] = p;
        if (data->size[9 + i] == 0) {
            data->profile[i] = NULL;
        }
        p += data->size[9 + i];
    }
    gBtlAi->data = data;
    if (owned == 1) {
        gBtlAi->own |= 1;
    }
}

/* Reads what the fighter's kit offers: move kinds 6 / 7, BtlCharApi_HasParamBit80, and the cheapest / dearest of the first
 * two skills in thousands. */
void AiThink_ReadKit(AiThWork *ai) {
    AiThChrMoves *moves = BtlCharApi_GetMoveTable(ai->objId);
    AiThChrSkills *skills = BtlCharApi_GetSkillTable(ai->objId);
    s32 i;

    ai->kit = 0;
    for (i = 0; i < 2; i++) {
        if (moves->kind[i] == 6) {
            ai->kit |= AITH_KIT_MOVE6;
        }
        if (moves->kind[i] == 7) {
            ai->kit |= AITH_KIT_MOVE7;
        }
    }
    if ((ai->kit & 3) == 3) {
        ai->kit &= ~AITH_KIT_MOVE7;
    }
    if (BtlCharApi_HasParamBit80(ai->objId) != 0) {
        ai->kit |= AITH_KIT_UNK4;
    }
    ai->costMin = (skills->cost[0] < skills->cost[1] ? skills->cost[0] : skills->cost[1]) / 1000;
    ai->costMax = (skills->cost[0] < skills->cost[1] ? skills->cost[1] : skills->cost[0]) / 1000;
}

/* Resets one side's AI work from its fighter: parameters, level, type, keep-distances, kit. owned selects a
 * separately loaded parameter block; that path has no loader left and leaves the side without AI. The name
 * buffer is dead: the original copied the debug label into it and its user was compiled out. */
void AiThink_ResetSide(s32 side, s32 owned) {
    char name[256];
    AiThWork *ai = &gBtlAi->work[side];
    AiThRange *range = &ai->range;
    void *param = NULL;
    f32 d;

    if (ai->own & 1) {
        if (ai->param != NULL) {
            Heap_Free(ai->param);
            ai->param = NULL;
        }
    }
    memset(ai, 0, sizeof(AiThWork));
    if (owned == 0) {
        param = BtlObj_Get(side)->param;
        strcpy(name, "tpChar->data.pparam_com");
    }
    if (param == NULL) {
        ai->param = NULL;
        return;
    }
    ai->param = param;
    ai->objId = side;
    ai->own &= ~1;
    BtlAiMgr_SetLevel(ai->objId, BtlCharApi_GetCpuLevel(side));
    BtlAiMgr_SetType(ai->objId, BtlCharApi_GetAiType(ai->objId));
    BtlAiSeq_Reset(&ai->seq);
    gBtlAi->radiusSum = BtlCharApi_GetRadius(ai->objId) + BtlCharApi_GetRadius(TagTeam_Target(ai->objId));
    range->dist[0] = gBtlAi->radiusSum;
    d = BtlCharApi_GetCloseRange(side) - BtlCharApi_GetRadius(side);
    range->dist[1] = d;
    range->dist[2] = d * 5.0f;
    range->dist[4] = range->dist[3] = d * 10.0f;
    AiThink_ReadKit(ai);
    if (owned != 0) {
        ai->own |= 1;
    }
}
