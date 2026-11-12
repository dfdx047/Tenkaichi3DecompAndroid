#include "common.h"
#include "battle/btl_char_status.h"

/*
 * Fighter stat modifiers, 0x1C2FF0..0x1C3CA8. See battle/btl_char_status.h for the structures.
 *
 * A fighter has four modified stats (fighter + 0xF50, four BtlStatMod records). A record holds signed
 * levels from several sources, each kept per skill slot (a character has two skills):
 *   base     kept for good              timed    with a frame timer
 *   untilA   until the action group 0x105..0x132 is left
 *   untilB / untilC   until the action code calls BtlStat_EndKind3 / BtlStat_EndKind4
 *   penalty  ignored while the fighter has flag 6        frame    lasts one frame
 * The only writer of the per-slot fields is the skill routine 0x2004C8(chr, slot): it reads the skill's kind
 * (BtlSkill_GetStatKind) and four levels from the character's parameter block and calls one setter per stat.
 *
 * BtlStat_GetMod(chr, stat) adds the levels up, adds the ability-driven extras and clamps to -20..20. The
 * seven BtlStat_GetLevelN add one of the active member's seven bonus values (BattleMember.bonus[1..7]) and
 * clamp to -20..80. BtlStat_EvalCurve turns a level into a quantity through one of 13 rows of
 * gBtlStatCurve {value at 80, value at 0, value at -20}, linear on each side of 0; the thirteen
 * BtlStat_GetRateN / BtlStat_GetScaleN are what the gauge, damage and movement code call.
 */

extern s32 BtlSkill_GetStatKind(BtlStatChr *chr, s32 slot);    /* the slot's skill kind: (obj + 0x930)[0x66 + slot] */
extern s32 BtlAct_GetCurrent(BtlStatChr *chr);              /* current action */
extern s32 BtlAct_GetPrev(BtlStatChr *chr);              /* previous action */
extern s32 BtlAct_IsTechniqueId(s32 action);                   /* action is 0x105..0x132 */
extern s32 BtlChar_TestPrevFlag(BtlStatChr *chr, s32 flag);    /* the flag in the second pair of flag arrays */
extern s32 BtlChar_TestFlag(BtlStatChr *chr, s32 flag);
extern BtlStatMember *BtlMember_GetActive(BtlStatChr *chr);   /* the active member */
extern BtlStatGauge *BtlMember_GetActiveGauge(BtlStatChr *chr);    /* its gauge block */
extern s32 BtlMember_HasAbility(BtlStatChr *chr, s32 ability); /* the active member has ability bit n */
extern s32 Battle_GetStage(void);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);

extern f32 gBtlStatCurve[BTL_STAT_CURVE_ROWS][3]; /* 0x2EE7C0, in this file's .rodata */

/* Marks the slot's skill as no longer active if its kind is `kind`. */
void BtlStat_ClearSlotOn(BtlStatChr *chr, s32 slot, s32 kind) {
    s32 i = slot != 0;

    if (BtlSkill_GetStatKind(chr, slot) == kind) {
        chr->slotOn[i] = 0;
    }
}

/* Removes the kind-2 modifiers of every stat. */
void BtlStat_EndKind2(BtlStatChr *chr) {
    s32 i;

    for (i = 0; i < BTL_STAT_COUNT; i++) {
        chr->stat[i].untilA[0] = 0;
        chr->stat[i].untilA[1] = 0;
    }
    chr->kind2On = 0;
    BtlStat_ClearSlotOn(chr, 0, BTL_STAT_KIND_ACTION);
    BtlStat_ClearSlotOn(chr, 1, BTL_STAT_KIND_ACTION);
}

/* Removes the kind-3 modifiers of every stat. */
void BtlStat_EndKind3(BtlStatChr *chr) {
    s32 i;

    for (i = 0; i < BTL_STAT_COUNT; i++) {
        chr->stat[i].untilB[0] = 0;
        chr->stat[i].untilB[1] = 0;
    }
    chr->kind3On = 0;
    BtlStat_ClearSlotOn(chr, 0, BTL_STAT_KIND_3);
    BtlStat_ClearSlotOn(chr, 1, BTL_STAT_KIND_3);
}

/* Removes the kind-4 modifiers of every stat. */
void BtlStat_EndKind4(BtlStatChr *chr) {
    s32 i;

    for (i = 0; i < BTL_STAT_COUNT; i++) {
        chr->stat[i].untilC[0] = 0;
        chr->stat[i].untilC[1] = 0;
    }
    BtlStat_ClearSlotOn(chr, 0, BTL_STAT_KIND_4);
    BtlStat_ClearSlotOn(chr, 1, BTL_STAT_KIND_4);
}

/* Removes the penalties of every stat. */
void BtlStat_ClearPenalty(BtlStatChr *chr) {
    s32 i;

    for (i = 0; i < BTL_STAT_COUNT; i++) {
        chr->stat[i].penalty[0] = 0;
        chr->stat[i].penalty[1] = 0;
    }
}

/* Clears the one-frame modifier of every stat (start of each frame). */
void BtlStat_ClearFrameMods(BtlStatChr *chr) {
    s32 i;

    for (i = BTL_STAT_COUNT - 1; i >= 0; i--) {
        chr->stat[i].frame = 0;
    }
}

/* Clears the whole modifier table. */
void BtlStat_Reset(BtlStatChr *chr) {
    s32 i;
    s32 j;

    for (i = 0; i < BTL_STAT_COUNT; i++) {
        for (j = 0; j < BTL_STAT_SLOTS; j++) {
            chr->stat[i].base[j] = 0;
            chr->stat[i].timed[j] = 0;
            chr->stat[i].untilA[j] = 0;
            chr->stat[i].untilB[j] = 0;
            chr->stat[i].untilC[j] = 0;
            chr->stat[i].penalty[j] = 0;
            chr->stat[i].timer[j] = 0;
        }
        chr->stat[i].frame = 0;
    }
}

/* Per frame: runs the timers of the timed modifiers and ends the kind-2 ones when action group 0x105..0x132 is left. */
void BtlStat_Update(BtlStatChr *chr) {
    s32 i;
    s32 j;

    for (i = 0; i < BTL_STAT_COUNT; i++) {
        for (j = 0; j < BTL_STAT_SLOTS; j++) {
            chr->stat[i].timer[j]--;
            if (chr->stat[i].timer[j] <= 0) {
                chr->stat[i].timer[j] = 0;
                chr->stat[i].timed[j] = 0;
                BtlStat_ClearSlotOn(chr, j, BTL_STAT_KIND_TIMED);
            }
        }
    }
    if (!BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr)) && BtlAct_IsTechniqueId(BtlAct_GetPrev(chr)) && !BtlChar_TestPrevFlag(chr, 0xA4)) {
        BtlStat_EndKind2(chr);
    }
}

/* Sets (or adds to) the kept modifier of a stat for a skill slot. */
void BtlStat_SetBase(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add) {
    if (add) {
        chr->stat[stat].base[slot] += val;
    } else {
        chr->stat[stat].base[slot] = val;
    }
}

/* Sets (or adds to) the one-frame modifier of a stat. */
void BtlStat_SetFrameMod(BtlStatChr *chr, s32 stat, s32 val, s32 add) {
    if (add) {
        chr->stat[stat].frame += val;
    } else {
        chr->stat[stat].frame = val;
    }
}

/* Sets (or adds to) the timed modifier of a stat for a skill slot and starts its timer. */
void BtlStat_SetTimed(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add, s32 time) {
    if (add) {
        chr->stat[stat].timed[slot] += val;
    } else {
        chr->stat[stat].timed[slot] = val;
    }
    chr->stat[stat].timer[slot] = time;
}

/* Sets (or adds to) the kind-2 modifier of a stat for a skill slot. */
void BtlStat_SetKind2(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add) {
    if (add) {
        chr->stat[stat].untilA[slot] += val;
    } else {
        chr->stat[stat].untilA[slot] = val;
    }
}

/* Sets (or adds to) the kind-3 modifier of a stat for a skill slot. */
void BtlStat_SetKind3(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add) {
    if (add) {
        chr->stat[stat].untilB[slot] += val;
    } else {
        chr->stat[stat].untilB[slot] = val;
    }
}

/* Sets (or adds to) the kind-4 modifier of a stat for a skill slot. */
void BtlStat_SetKind4(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add) {
    if (add) {
        chr->stat[stat].untilC[slot] += val;
    } else {
        chr->stat[stat].untilC[slot] = val;
    }
}

/* Sets the penalty of a stat for a skill slot. */
void BtlStat_SetPenalty(BtlStatChr *chr, s32 stat, s32 slot, s32 val) {
    chr->stat[stat].penalty[slot] = val;
}

/* Total modifier level of a stat, -20..20: the table, plus 3 per lost health bar and a stage bonus with the matching abilities. */
s32 BtlStat_GetMod(BtlStatChr *chr, s32 stat) {
    BtlStatMod *m;
    s32 i;
    s32 sum;
    s32 lost;

    m = &chr->stat[stat];
    sum = 0;
    for (i = 0; i < BTL_STAT_SLOTS; i++) {
        sum += m->base[i];
        sum += m->timed[i];
        sum += m->untilA[i];
        sum += m->untilB[i];
        sum += m->untilC[i];
    }
    if (!BtlChar_TestFlag(chr, 6)) {
        for (i = 0; i < BTL_STAT_SLOTS; i++) {
            sum += m->penalty[i];
        }
    }
    sum += m->frame;
    lost = (BtlMember_GetActiveGauge(chr)->healthMax - BtlMember_GetActiveGauge(chr)->health) / 10000 * 3;
    switch (stat) {
        case 0:
            if (BtlMember_HasAbility(chr, 0x58)) {
                sum += lost;
            }
            break;
        case 1:
            if (BtlMember_HasAbility(chr, 0x71)) {
                sum += lost;
            }
            break;
        case 2:
            if (BtlMember_HasAbility(chr, 0x72)) {
                sum += lost;
            }
            break;
        case 3:
            if (BtlMember_HasAbility(chr, 0x73)) {
                sum += lost;
            }
            break;
    }
    switch (Battle_GetStage()) {
        case 2:
        case 3:
            if (BtlMember_HasAbility(chr, 0x59)) {
                sum += 15;
            }
            break;
        case 7:
        case 8:
        case 13:
        case 16:
        case 18:
        case 34:
            if (BtlMember_HasAbility(chr, 0x5B)) {
                sum += 10;
            }
            break;
        default:
            if (BtlMember_HasAbility(chr, 0x5A)) {
                sum += 5;
            }
            break;
    }
    return BtlUtil_Clamp(sum, -20, 20);
}

/* Level -20..80 of bonus 0 with stat 2. */
s32 BtlStat_GetLevel0(BtlStatChr *chr) {
    s32 bonus = BtlMember_GetActive(chr)->bonus[0];

    return BtlUtil_Clamp(bonus + BtlStat_GetMod(chr, BTL_STAT_2), -20, 80);
}

/* Level -20..80 of bonus 1 with stat 0. */
s32 BtlStat_GetLevel1(BtlStatChr *chr) {
    s32 bonus = BtlMember_GetActive(chr)->bonus[1];

    return BtlUtil_Clamp(bonus + BtlStat_GetMod(chr, BTL_STAT_0), -20, 80);
}

/* Level -20..80 of bonus 2 with stat 1. */
s32 BtlStat_GetLevel2(BtlStatChr *chr) {
    s32 bonus = BtlMember_GetActive(chr)->bonus[2];

    return BtlUtil_Clamp(bonus + BtlStat_GetMod(chr, BTL_STAT_1), -20, 80);
}

/* Level -20..80 of bonus 3 with stat 2. */
s32 BtlStat_GetLevel3(BtlStatChr *chr) {
    s32 bonus = BtlMember_GetActive(chr)->bonus[3];

    return BtlUtil_Clamp(bonus + BtlStat_GetMod(chr, BTL_STAT_2), -20, 80);
}

/* Level -20..80 of bonus 4 with stat 3. */
s32 BtlStat_GetLevel4(BtlStatChr *chr) {
    s32 bonus = BtlMember_GetActive(chr)->bonus[4];

    return BtlUtil_Clamp(bonus + BtlStat_GetMod(chr, BTL_STAT_3), -20, 80);
}

/* Level -20..80 of bonus 5 with stat 3. */
s32 BtlStat_GetLevel5(BtlStatChr *chr) {
    s32 bonus = BtlMember_GetActive(chr)->bonus[5];

    return BtlUtil_Clamp(bonus + BtlStat_GetMod(chr, BTL_STAT_3), -20, 80);
}

/* Level -20..80 of bonus 6 with stat 3. */
s32 BtlStat_GetLevel6(BtlStatChr *chr) {
    s32 bonus = BtlMember_GetActive(chr)->bonus[6];

    return BtlUtil_Clamp(bonus + BtlStat_GetMod(chr, BTL_STAT_3), -20, 80);
}

/* Value of curve row `row` at a level: linear from base to max over 0..80 and from base to min over 0..-20. */
f32 BtlStat_EvalCurve(s32 level, s32 row) {
    if (level >= 0) {
        return (gBtlStatCurve[row][BTL_CURVE_MAX] - gBtlStatCurve[row][BTL_CURVE_BASE]) * ((f32)level / 80.0f) +
               gBtlStatCurve[row][BTL_CURVE_BASE];
    }
    return (gBtlStatCurve[row][BTL_CURVE_MIN] - gBtlStatCurve[row][BTL_CURVE_BASE]) * ((f32)level / -20.0f) +
           gBtlStatCurve[row][BTL_CURVE_BASE];
}

/* Curve row 0 at level 0, per frame (per-second value / 30). */
s32 BtlStat_GetKiChargeBonus(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel0(chr), 0) / 30.0f;
}

/* Curve row 1 at level 0, per frame. */
s32 BtlStat_GetKiRegenBonus(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel0(chr), 1) / 30.0f;
}

/* Curve row 2 at level 0, per frame. */
s32 BtlStat_GetKiRecoverBonus(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel0(chr), 2) / 30.0f;
}

/* Curve row 3 at level 4, per frame. */
s32 BtlStat_GetBlastGainBonus(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel4(chr), 3) / 30.0f;
}

/* Curve row 4 at level 1. */
f32 BtlStat_GetMeleeDamageScale(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel1(chr), 4);
}

/* Curve row 5 at level 1. */
f32 BtlStat_GetGuardKiCostScale(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel1(chr), 5);
}

/* Curve row 6 at level 1. */
f32 BtlStat_GetKiBlastDamageScale(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel1(chr), 6);
}

/* Curve row 7 at level 2. */
f32 BtlStat_GetScale7(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel2(chr), 7);
}

/* Curve row 8 at level 3. */
f32 BtlStat_GetSpeedScale(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel3(chr), 8);
}

/* Curve row 9 at level 5. */
f32 BtlStat_GetBlast2DamageScale(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel5(chr), 9);
}

/* Curve row 10 at level 6. */
f32 BtlStat_GetUltimateDamageScale(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel6(chr), 10);
}

/* Curve row 11 at level 0. */
f32 BtlStat_GetMaxPowerChargeScale(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel0(chr), 11);
}

/* Curve row 12 at level 0. */
f32 BtlStat_GetMaxPowerExtraTime(BtlStatChr *chr) {
    return BtlStat_EvalCurve(BtlStat_GetLevel0(chr), 12);
}
