#include "common.h"
#ifdef PORT
#include "sys/common.h"
#endif
#include "battle/btl_tech_a.h"

/*
 * Fighter queries by side, the attack records and the character parameter block, 0x20BA80..0x20F0E8 (124 functions).
 * See include/battle/btl_tech_a.h for the record layouts and the attack id table.
 *
 * What the numbers are (each is the matching C below; the game terms are inferred from the callers):
 *   - damage of a hit        record +0x04 * attack curve (row 4: x0.5 at level -20, x1 at 0, x3 at 80), +-10% / x2 / x0.5
 *                            by ability, / number of hit events of the animation
 *   - ki charge              (param +0x30 or +0x34) / 30 + curve row 0 (-1000..0..+4000 per frame) + chr +0xE28 / 30,
 *                            x0.4 / 0.6 / 0.8 and x1.25 by ability, at least 200 per frame (ki maximum is 100000)
 *   - passive ki regen       param +0x38 / 30 + curve row 1 (-66..0..+266 per frame), up to param +0x28 (+ abilities)
 *   - ki-exhausted recovery  param +0x3C / 30 + curve row 2, x2 by ability, at least 10, until param +0x2C is reached
 *   - blast gauge            param +0x44 / 30 + curve row 3 (-33..0..+133 per frame), x1.2 / x0.5 / x0.8 by ability
 *   - max power              filled in param +0x50 / curve row 11 seconds, drained over param +0x54 + curve row 12
 *                            (0..8) seconds; the gauge is 30000
 *
 * Object boundary: unknown. The float pool 0x2FE0E4..0x2FE110 and the .rodata 0x2F1620..0x2F18F0 this file emits are
 * contiguous and exactly cover what the range owns; the neighbours on both sides (0x2FE0E0, 0x2FE114, jtbl_002F18F0)
 * belong to other functions, so the file links on its own whatever the original object was.
 *
 * No random draw. Non-simulation inputs, all in the BtlCtrl_* group (HUD queries, nothing they return feeds the
 * simulation here): the pad status (Pad.lastStatus), the battle mode, gProgress and the battle work flags.
 */

/* km/h to units per frame. This file's constants need this form (btl_char_move.c uses x * 1000 / 3600 * (1 / 30),
   which gives 0x3C17B424 for 1.0f where this file has 0x3C17B425). */
#define BTL_KMH(x) ((x) * (1000.0f / 3600.0f) / 30.0f)

extern BtlTechRoster *gBtlChars;
extern BtlTechProgress *gProgress;

extern BtlTechChr *BtlChar_FindBySide(s32 side);
extern BtlTechChr *BtlChar_Get(s32 player);
extern BtlTechObj *BtlChar_GetObj(BtlTechChr *chr);
extern BtlTechPad *BtlChar_GetPad(BtlTechChr *chr);
extern s32 BtlChar_IsFrozen(BtlTechChr *chr);
extern s32 BtlChar_IsDead(BtlTechChr *chr);
extern s32 BtlChar_TestMemberUnk70(BtlTechChr *chr);
extern s32 BtlChar_TestFlag(BtlTechChr *chr, s32 flag);
extern s32 BtlChar_IsFlagRaised(BtlTechChr *chr, s32 flag); /* set this frame and not the frame before */
extern s32 BtlOpp_GetPlayer(BtlTechChr *chr);
extern s32 BtlOpp_HasAbility(BtlTechChr *chr, s32 ability);
extern s32 BtlAct_GetCurrent(BtlTechChr *chr);
extern s32 BtlAct_IsAttackId(s32 action);
extern s32 BtlAct_TestPoweredSkill(BtlTechChr *chr, s32 bit);
extern s32 BtlAnim_GetId(BtlTechChr *chr);
extern f32 BtlAnim_GetFrame(BtlTechChr *chr);
extern BtlTechMember *BtlMember_Get(BtlTechChr *chr, s32 n);
extern BtlTechMember *BtlMember_GetActive(BtlTechChr *chr);
extern BtlTechGauge *BtlMember_GetActiveGauge(BtlTechChr *chr);
extern s32 BtlMember_CountAlive(BtlTechChr *chr);
extern s32 BtlMember_HasAbility(BtlTechChr *chr, s32 ability);
extern s32 BtlStat_GetMod(BtlTechChr *chr, s32 stat);
extern s32 BtlStat_GetKiChargeBonus(BtlTechChr *chr);
extern s32 BtlStat_GetKiRegenBonus(BtlTechChr *chr);
extern s32 BtlStat_GetKiRecoverBonus(BtlTechChr *chr);
extern s32 BtlStat_GetBlastGainBonus(BtlTechChr *chr);
extern f32 BtlStat_GetMeleeDamageScale(BtlTechChr *chr);
extern f32 BtlStat_GetGuardKiCostScale(BtlTechChr *chr);
extern f32 BtlStat_GetSpeedScale(BtlTechChr *chr);
extern f32 BtlStat_GetMaxPowerChargeScale(BtlTechChr *chr);
extern f32 BtlStat_GetMaxPowerExtraTime(BtlTechChr *chr);
extern s32 BtlUtil_Max(s32 a, s32 b);
extern BtlTechWork *Battle_GetWork(void);
extern BtlTechWork5A8 *Battle_GetWork5A8(void);
extern s32 Battle_GetMode(void);
extern s32 BtlSeq_GetState(void);

extern s32 BtlAct_CanUseTechnique(BtlTechChr *chr, s32 slot); /* slots 2..4 */
extern s32 BtlAct_CanUseSkill(BtlTechChr *chr, s32 slot); /* slots 0..1 */
extern s32 BtlCharApi_GetButtonIcon(BtlTechChr *chr, s32 cmd);  /* button 0..3 the key config gives command `cmd`, -1 if none */
extern s32 BtlSuper_GetPromptRowIndex(BtlTechChr *chr, s32 idx);  /* (obj + 0x92C)[0x227 + idx], signed */
/* Animation events of layer `layer` whose attribute word has a bit of `mask`: what 2 = frame of the last one,
   3 = how many there are. */
extern s32 BtlObjAnim_QueryEvent(BtlTechObj *obj, u64 mask, s32 layer, s32 what);

/* ---- queries by side -------------------------------------------------------------------------------------------- */

/* The opponent's +0xD50, 0 while the opponent is frozen or the battle flag 0x2000 is set. */
s32 BtlCtrl_GetOppUnkD50(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);
    BtlTechChr *opp;

    if (chr != NULL) {
        opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
        if (BtlChar_IsFrozen(opp)) {
            return 0;
        }
        if (Battle_GetWork()->flags & 0x2000) {
            return 0;
        }
        return opp->unkD50;
    }
    return 0;
}

/* BtlChar_GetObj behind an inline: the original calls it with jal (no tail call), which a direct `return f(chr)`
   does not give. */
static inline BtlTechObj *BtlCtrl_ObjOf(BtlTechChr *chr) {
    return BtlChar_GetObj(chr);
}

/* The battle object of the side's fighter. */
BtlTechObj *BtlCtrl_GetObj(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlCtrl_ObjOf(chr);
    }
    return NULL;
}

/* Which of flags 0xDE..0xE2 the fighter has, as 0..4; -1 for none or while frozen. */
s32 BtlCtrl_GetFlagDEtoE2(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr == NULL) {
        return -1;
    }
    if (BtlChar_IsFrozen(chr)) {
        return -1;
    }
    if (!BtlChar_IsFrozen(chr)) {
        if (BtlChar_TestFlag(chr, 0xDE)) {
            return 0;
        }
        if (BtlChar_TestFlag(chr, 0xDF)) {
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0xE0)) {
            return 2;
        }
        if (BtlChar_TestFlag(chr, 0xE1)) {
            return 3;
        }
        if (BtlChar_TestFlag(chr, 0xE2)) {
            return 4;
        }
    }
    return -1;
}

/* The fighter's clash counter (+0xE50: compared to pick the winner of clashes B and C). */
s32 BtlCtrl_GetClashCount(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return chr->clashCount;
    }
    return 0;
}

/* Flag 6 (powered-up mode) was raised this frame. */
s32 BtlCtrl_IsFlag6Raised(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_IsFrozen(chr)) {
            return 0;
        }
        return BtlChar_IsFlagRaised(chr, 6);
    }
    return 0;
}

/* Flag 0x71, 0 while frozen. */
s32 BtlCtrl_TestFlag71(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_IsFrozen(chr)) {
            return 0;
        }
        return BtlChar_TestFlag(chr, 0x71);
    }
    return 0;
}

/* Flag 5 (lock-on) was raised this frame, only in sequence state 3. */
s32 BtlCtrl_IsFlag5RaisedInFight(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_IsFrozen(chr)) {
            return 0;
        }
        if (BtlSeq_GetState() != 3) {
            return 0;
        }
        return BtlChar_IsFlagRaised(chr, 5);
    }
    return 0;
}

/* Flag 0xE3, 0 while frozen. */
s32 BtlCtrl_TestFlagE3(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_IsFrozen(chr)) {
            return 0;
        }
        return BtlChar_TestFlag(chr, 0xE3);
    }
    return 0;
}

/* Flag 0x9C was raised this frame. */
s32 BtlCtrl_IsFlag9CRaised(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_IsFrozen(chr)) {
            return 0;
        }
        return BtlChar_IsFlagRaised(chr, 0x9C);
    }
    return 0;
}

/* Flag 0x9E was raised this frame. */
s32 BtlCtrl_IsFlag9ERaised(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_IsFrozen(chr)) {
            return 0;
        }
        return BtlChar_IsFlagRaised(chr, 0x9E);
    }
    return 0;
}

/* The active member has no health left. */
s32 BtlCtrl_IsDead(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_IsDead(chr);
    }
    return 0;
}

/* Flag 0x12B. */
s32 BtlCtrl_TestFlag12B(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x12B);
    }
    return 0;
}

/* The fighter is in one of the member-switch actions 0xF3..0xF8. */
s32 BtlCtrl_IsSwitching(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        switch (BtlAct_GetCurrent(chr)) {
            case 0xF3:
            case 0xF4:
            case 0xF5:
            case 0xF6:
            case 0xF7:
            case 0xF8:
                return 1;
        }
        return 0;
    }
    return 0;
}

/* Flag 0xE4. */
s32 BtlCtrl_TestFlagE4(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xE4);
    }
    return 0;
}

/* Flag 0xE5. */
s32 BtlCtrl_TestFlagE5(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xE5);
    }
    return 0;
}

/* In battle mode 5: bit gProgress->unk7F4 of fighter 0's per-frame bit set. */
s32 BtlCtrl_TestProgressFrameBit(void) {
    BtlTechChr *chr;

    if (Battle_GetMode() != 5) {
        return 0;
    }
    if (gProgress == NULL) {
        return 0;
    }
    chr = BtlChar_Get(0);
    if (chr != NULL) {
        return (chr->frameBits >> gProgress->unk7F4) & 1;
    }
    return 0;
}

/* Flag 0xE6 when the fighter's pad status is not 1; the status is written to *padStatus. */
s32 BtlCtrl_TestFlagE6Pad(s32 side, s32 *padStatus) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_GetPad(chr)->lastStatus == 1) {
            return 0;
        }
        if (padStatus != NULL) {
            *padStatus = BtlChar_GetPad(chr)->lastStatus;
        }
        return BtlChar_TestFlag(chr, 0xE6);
    }
    return 0;
}

/* Flag 0xE6 when the fighter's pad status is 1. */
s32 BtlCtrl_TestFlagE6Pad1(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlChar_GetPad(chr)->lastStatus != 1) {
            return 0;
        }
        return BtlChar_TestFlag(chr, 0xE6);
    }
    return 0;
}

/* Pad status 1 only: the technique prompt for the class picked by the held direction (it must be usable and the
   technique delay over) or else the watched class: the technique type row, its prompt kind 0..3 and the class. */
s32 BtlCtrl_GetTechPromptPad1(s32 side, s32 *row, s32 *kind, s32 *idx) {
    s32 n = -1;
    s32 col = -1;
    BtlTechChr *chr = BtlChar_FindBySide(side);
    s32 r;
    s32 k;
    BtlTechPromptRow *rowp;

    if (chr == NULL) {
        return 0;
    }
    if (chr->injectOn) {
        return 0;
    }
    if (BtlChar_GetPad(chr)->lastStatus != 1) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xE7)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xE8)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xE9)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xEA)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xEB)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xEC)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xEE)) {
        return 0;
    }
    if (chr->dirHeld >= 2) {
        n = chr->dirHeld;
        col = 0;
        if (!BtlAct_CanUseTechnique(chr, n)) {
            return 0;
        }
        if (chr->techDelay > 0) {
            return 0;
        }
    } else if (chr->techClass >= 2) {
        n = chr->techClass;
        col = 1;
        if (BtlChar_TestFlag(chr, 0x9D)) {
            return 0;
        }
        if (BtlChar_TestFlag(chr, 0x11F)) {
            return 0;
        }
    }
    if (n < 0) {
        return 0;
    }
    if (col < 0) {
        return 0;
    }
    r = BtlSuper_GetPromptRowIndex(chr, n);
    rowp = &gBtlChars->promptRows[r];
    k = 0;
    switch (rowp->kind[col]) {
        case 0:
            break;
        case 1:
            k = 1;
            break;
        case 2:
            k = 2;
            break;
        case 3:
            k = 3;
            break;
    }
    if (row != NULL) {
        *row = r;
    }
    if (kind != NULL) {
        *kind = k;
    }
    if (idx != NULL) {
        *idx = n;
    }
    return 1;
}

/* Other pad statuses, no direction class held: the button icons (1 or 2) to show for the watched technique class,
   a kind (2 with flag 0xA2, 1 with 0xA3, else 0), the pad status and the class. Fails if an icon is missing. */
s32 BtlCtrl_GetSwitchPrompt(s32 side, s32 *buttons, s32 *count, s32 *kind, s32 *padStatus, s32 *idx) {
    s32 btn[8];
    s32 n = -1;
    BtlTechChr *chr = BtlChar_FindBySide(side);
    BtlTechPromptRow *row;
    s32 cnt;
    s32 k;
    s32 i;

    if (chr == NULL) {
        return 0;
    }
    if (chr->injectOn) {
        return 0;
    }
    if (BtlChar_GetPad(chr)->lastStatus == 1) {
        return 0;
    }
    if (chr->dirHeld >= 2) {
        return 0;
    }
    if (chr->techClass >= 2) {
        n = chr->techClass;
        if (BtlChar_TestFlag(chr, 0x9D)) {
            return 0;
        }
        if (BtlChar_TestFlag(chr, 0x11F)) {
            return 0;
        }
    }
    if (n < 0) {
        return 0;
    }
    row = &gBtlChars->promptRows[BtlSuper_GetPromptRowIndex(chr, n)];
    cnt = 0;
    k = 0;
    if (BtlChar_TestFlag(chr, 0xA2)) {
        btn[0] = BtlCharApi_GetButtonIcon(chr, 2);
        cnt = 1;
        k = 2;
    } else if (BtlChar_TestFlag(chr, 0xA3)) {
        btn[0] = BtlCharApi_GetButtonIcon(chr, 2);
        cnt = 1;
        k = 1;
    } else {
        switch (row->buttons) {
            case 0:
                btn[0] = BtlCharApi_GetButtonIcon(chr, 4);
                cnt = 2;
                btn[1] = BtlCharApi_GetButtonIcon(chr, 3);
                break;
            case 1:
                btn[0] = BtlCharApi_GetButtonIcon(chr, 5);
                cnt = 2;
                btn[1] = BtlCharApi_GetButtonIcon(chr, 3);
                break;
            case 2:
                btn[0] = BtlCharApi_GetButtonIcon(chr, 6);
                cnt = 2;
                btn[1] = BtlCharApi_GetButtonIcon(chr, 3);
                break;
            case 3:
                btn[0] = BtlCharApi_GetButtonIcon(chr, 7);
                cnt = 2;
                btn[1] = BtlCharApi_GetButtonIcon(chr, 3);
                break;
        }
    }
    for (i = 0; i < cnt; i++) {
        if (btn[i] < 0) {
            return 0;
        }
    }
    if (count != NULL) {
        *count = cnt;
    }
    if (kind != NULL) {
        *kind = k;
    }
    if (padStatus != NULL) {
        *padStatus = BtlChar_GetPad(chr)->lastStatus;
    }
    if (idx != NULL) {
        *idx = n;
    }
    if (buttons != NULL) {
        for (i = 0; i < cnt; i++) {
            buttons[i] = btn[i];
        }
    }
    return 1;
}

/* The button icon to show while one of flags 0xE7..0xEC is set (0xE7 is the ki-exhausted mash prompt), with kind 1
   (0xE7, 0xE8: buttons 0 / 2) or 2 (0xE9..0xEC: buttons 0x1B..0x1E). The last flag set wins. */
s32 BtlCtrl_GetChangePrompt(s32 side, s32 *button, s32 *kind, s32 *padStatus) {
    s32 k = -1;
    s32 btn = -1;
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr == NULL) {
        return 0;
    }
    if (chr->injectOn) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xE7)) {
        btn = BtlCharApi_GetButtonIcon(chr, 0);
        k = 1;
    }
    if (BtlChar_TestFlag(chr, 0xE8)) {
        btn = BtlCharApi_GetButtonIcon(chr, 2);
        k = 1;
    }
    if (BtlChar_TestFlag(chr, 0xE9)) {
        btn = BtlCharApi_GetButtonIcon(chr, 0x1B);
        k = 2;
    }
    if (BtlChar_TestFlag(chr, 0xEA)) {
        btn = BtlCharApi_GetButtonIcon(chr, 0x1C);
        k = 2;
    }
    if (BtlChar_TestFlag(chr, 0xEB)) {
        btn = BtlCharApi_GetButtonIcon(chr, 0x1D);
        k = 2;
    }
    if (BtlChar_TestFlag(chr, 0xEC)) {
        btn = BtlCharApi_GetButtonIcon(chr, 0x1E);
        k = 2;
    }
    if (btn < 0 || k < 0) {
        return 0;
    }
    if (button != NULL) {
        *button = btn;
    }
    if (kind != NULL) {
        *kind = k;
    }
    if (padStatus != NULL) {
        *padStatus = BtlChar_GetPad(chr)->lastStatus;
    }
    return 1;
}

/* Sum of the modifiers of stat 0. */
s32 BtlCtrl_GetStatMod0(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlStat_GetMod(chr, 0);
    }
    return 0;
}

/* Sum of the modifiers of stat 2. */
s32 BtlCtrl_GetStatMod2(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlStat_GetMod(chr, 2);
    }
    return 0;
}

/* Sum of the modifiers of stat 1. */
s32 BtlCtrl_GetStatMod1(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlStat_GetMod(chr, 1);
    }
    return 0;
}

/* Sum of the modifiers of stat 3. */
s32 BtlCtrl_GetStatMod3(s32 side) {
    BtlTechChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlStat_GetMod(chr, 3);
    }
    return 0;
}

/* Whether skill slot 0..1 (BtlAct_CanUseSkill) or technique class 2..4 (BtlAct_CanUseTechnique) can be used now. */
s32 BtlCtrl_CanUseTechnique(s32 player, s32 slot) {
    BtlTechChr *chr = BtlChar_Get(player);

    if (chr == NULL) {
        return 0;
    }
    switch (slot) {
        case 0:
        case 1:
            return BtlAct_CanUseSkill(chr, slot);
        case 2:
        case 3:
        case 4:
            return BtlAct_CanUseTechnique(chr, slot);
    }
    return 0;
}

/* Battle mode 3: work5A8.unk0, plus 1 once fighter 1 has nobody left, capped at work5A8.unk4. */
s32 BtlCtrl_GetWork5A8Count(void) {
    BtlTechWork5A8 *work;
    s32 n;

    if (Battle_GetMode() != 3) {
        return 0;
    }
    work = Battle_GetWork5A8();
    if (work == NULL) {
        return 0;
    }
    n = work->unk0;
    if (BtlMember_CountAlive(BtlChar_Get(1)) < 1) {
        n++;
    }
    if (work->unk4 < n) {
        return work->unk4;
    }
    return n;
}

/* Sets the CPU level of every member of a fighter. */
void BtlCtrl_SetCpuLevel(s32 player, s32 level) {
    BtlTechChr *chr = BtlChar_Get(player);
    s32 i;

    if (chr != NULL && chr->memberCount > 0) {
        for (i = 0; i < chr->memberCount; i++) {
            BtlMember_Get(chr, i)->cpuLevel = level;
        }
    }
}

/* The active member's gauge word +0x30 is set. */
s32 BtlCtrl_TestMemberUnk70(s32 player) {
    BtlTechChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        return BtlChar_TestMemberUnk70(chr);
    }
    return 0;
}

/* ---- attack records --------------------------------------------------------------------------------------------- */

/* Row of an attack action in the roster's table. */
static inline BtlTechAtkAction *BtlAtk_GetActionRow(s32 n) {
    return &gBtlChars->atkActions[n];
}

/* weak / medium / full record by charge */
#define TIER3(a, b, c)      \
    if (charge < mid) {     \
        return a;           \
    }                       \
    if (charge < full) {    \
        return b;           \
    }                       \
    return c

/* The id of the attack record the fighter's current action / animation uses, -1 when it is not attacking. */
s32 BtlAtk_GetId(BtlTechChr *chr) {
    BtlTechObj *obj = BtlChar_GetObj(chr);
    f32 charge = chr->charge;
    s8 *hit = (s8 *)obj->unkC90;
    s32 alt = chr->unkD68;
    s32 action = BtlAct_GetCurrent(chr);
    s32 anim = BtlAnim_GetId(chr);
    f32 mid = BTL_ATK_CHARGE_MID;
    f32 full = BTL_ATK_CHARGE_FULL;
    f32 frame;
    f32 last;
    s32 b;

    if (BtlAct_IsAttackId(action)) {
        return BtlAtk_GetActionRow(action - 0x70)->atkId;
    }
    frame = BtlAnim_GetFrame(chr);
    last = BtlObjAnim_QueryEvent(obj, 1, 0, 2);
    switch (action) {
        case 0x46:
            switch (anim) {
                case 0x3C:
                    return 0x4E;
                case 0x3D:
                    return 0x4F;
                case 0x3E:
                    return 0x50;
                case 0x3F:
                    return 0x51;
                case 0x40:
                    return 0x52;
            }
            return -1;
        case 0xB4:
        case 0xB5:
        case 0xB7:
        case 0xBB:
            return 0x55;
        case 0xB6:
        case 0xB9:
            return 0x56;
        case 0x60:
            TIER3(0xB, 0xC, 0xD);
        case 0x5A:
            return 0x43;
        case 0x5B:
            return 0x44;
        case 0x5C:
            return 0x45;
        case 0x5D:
            return 0x46;
        case 0x6B:
            return alt ? 0x3E : 0x34;
        case 0x6C:
            return alt ? 0x3F : 0x35;
        case 0x6D:
            return alt ? 0x40 : 0x36;
        case 0x6E:
            return alt ? 0x41 : 0x37;
        case 0x6F:
            return alt ? 0x42 : 0x38;
        case 0x64:
            TIER3(0x59, 0x5B, 0x5D);
        case 0x65:
            TIER3(0x5A, 0x5C, 0x5E);
        case 0x61:
            TIER3(0x2E, 0x2F, 0x30);
        case 0x62:
            TIER3(0x31, 0x32, 0x33);
        case 0x37:
        case 0x42:
            return 0x66;
        case 0x4F:
            return 0x67;
        case 0x50:
            return 0x68;
        case 0x4D:
            return 0x69;
        case 0x4E:
            return 0x6A;
        case 0x51:
        case 0x52:
            return 0x6B;
    }
    switch (anim) {
        case 0x37:
            if (hit[0x1C] >= 2 && (b = hit[0x1D]) != 0 && b != -1) {
                return 1;
            }
            return 0;
        case 0x38:
            return 2;
        case 0x39:
            return 3;
        case 0x3A:
            return 4;
        case 0x3B:
            return 5;
        case 0x3C:
            return 6;
        case 0x3D:
            return 7;
        case 0x3E:
            return 8;
        case 0x3F:
            return 9;
        case 0x40:
            return 0xA;
        case 0x93:
            if (frame < last) {
                return 0x11;
            }
            return 0x12;
        case 0x58:
            if (BtlChar_TestFlag(chr, 0x84)) {
                return 0x67;
            }
            TIER3(0x13, 0x18, 0x1D);
        case 0x5B:
            if (BtlChar_TestFlag(chr, 0x84)) {
                return 0x68;
            }
            TIER3(0x14, 0x19, 0x1E);
        case 0x52:
            if (BtlChar_TestFlag(chr, 0x84)) {
                return 0x69;
            }
            TIER3(0x15, 0x1A, 0x1F);
        case 0x55:
            if (BtlChar_TestFlag(chr, 0x84)) {
                return 0x6A;
            }
            TIER3(0x16, 0x1B, 0x20);
        case 0x4C:
        case 0x4F:
            if (BtlChar_TestFlag(chr, 0x84)) {
                return 0x6B;
            }
            TIER3(0x17, 0x1C, 0x21);
        case 0x67:
            if (alt) {
                return 0x3E;
            }
            if (charge < full) {
                return 0x34;
            }
            return 0x39;
        case 0x6A:
            if (alt) {
                return 0x3F;
            }
            if (charge < full) {
                return 0x35;
            }
            return 0x3A;
        case 0x61:
            if (alt) {
                return 0x40;
            }
            if (charge < full) {
                return 0x36;
            }
            return 0x3B;
        case 0x64:
            if (alt) {
                return 0x41;
            }
            if (charge < full) {
                return 0x37;
            }
            return 0x3C;
        case 0x5E:
            if (alt) {
                return 0x42;
            }
            if (charge < full) {
                return 0x38;
            }
            return 0x3D;
        case 0x43:
            TIER3(0x48, 0x49, 0x4A);
        case 0x46:
            TIER3(0x4B, 0x4C, 0x4D);
        case 0x6D:
            if (charge < mid) {
                return frame < last ? 0x22 : 0x23;
            }
            if (charge < full) {
                return frame < last ? 0x24 : 0x25;
            }
            return frame < last ? 0x26 : 0x27;
        case 0x70:
            if (charge < mid) {
                return frame < last ? 0x28 : 0x29;
            }
            if (charge < full) {
                return frame < last ? 0x2A : 0x2B;
            }
            return frame < last ? 0x2C : 0x2D;
        case 0xF8:
            return 0x57;
        case 0x19B:
            if (obj->hitNo == 0) {
                TIER3(0x7E, 0x81, 0x84);
            }
            if (obj->hitNo < obj->hitCount - 1) {
                TIER3(0x7F, 0x82, 0x85);
            }
            TIER3(0x80, 0x83, 0x86);
    }
    return -1;
}

/* The fighter's current attack record; ids outside 0..0xA2 (and -1) give record 0. The id used goes to *id. */
BtlAtkRecord *BtlAtk_GetRecord(BtlTechChr *chr, s32 *id) {
    u32 n = BtlAtk_GetId(chr);

    if (n >= BTL_ATK_COUNT) {
        n = 0;
    }
    if (id != NULL) {
        *id = n;
    }
    return &BtlChar_GetObj(chr)->atk[n];
}

/* Record `id` of the fighter's character. */
BtlAtkRecord *BtlAtk_GetRecordOf(BtlTechChr *chr, u32 id) {
    if (id >= BTL_ATK_COUNT) {
        id = 0;
    }
    return &BtlChar_GetObj(chr)->atk[id];
}

/* Damage changes by ability: ids 0..9 +10% (4) / -10% (5); ids 0x13..0x21 +10% (6) / -10% (7); ids 0x55, 0x56
   x2 (8) / halved (9). */
s32 BtlAtk_ApplyAbilities(BtlTechChr *chr, s32 val, u32 id) {
    extern s32 TagGiant_Damage(s32,s32);
    val=TagGiant_Damage(chr->player,val);
    if (id < 10) {
        if (BtlMember_HasAbility(chr, 4)) {
            val += val / 10;
        }
        if (BtlMember_HasAbility(chr, 5)) {
            val -= val / 10;
        }
    }
    if (id - 0x13 < 0xF) {
        if (BtlMember_HasAbility(chr, 6)) {
            val += val / 10;
        }
        if (BtlMember_HasAbility(chr, 7)) {
            val -= val / 10;
        }
    }
    if (id - 0x55 < 2) {
        if (BtlMember_HasAbility(chr, 8)) {
            val *= 2;
        }
        if (BtlMember_HasAbility(chr, 9)) {
            val -= val / 2;
        }
    }
    return val;
}

/* Whether an attack id is one of 0x13..0x21. */
static inline s32 BtlAtk_IsId13to21(s32 id) {
    return (u32)(id - 0x13) < 0xF;
}

/* Record +0x26: which attack wins when two connect on the same frame. */
s32 BtlAtk_GetPriority(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->priority;
}

/* Damage of one hit: record damage * curve row 4, ability changes, divided by the animation's hit count. */
s32 BtlAtk_GetDamage(BtlTechChr *chr) {
    s32 id;
    BtlTechObj *obj = BtlChar_GetObj(chr);
    s32 val = BtlAtk_GetRecord(chr, &id)->damage;
    s32 hits;

    val = val * BtlStat_GetMeleeDamageScale(chr);
    val = BtlAtk_ApplyAbilities(chr, val, id);
    hits = BtlObjAnim_QueryEvent(obj, 1, 0, 3);
    if (hits >= 2) {
        val /= hits;
    }
    return val;
}

/* Damage of one guarded hit: the same chain on record +0x06, with -30% for ids 0..10 under skill effect 0x10. */
s32 BtlAtk_GetGuardDamage(BtlTechChr *chr) {
    s32 id;
    BtlTechObj *obj = BtlChar_GetObj(chr);
    s32 val = BtlAtk_GetRecord(chr, &id)->guardDamage;
    s32 hits;

    val = val * BtlStat_GetMeleeDamageScale(chr);
    if (BtlAct_TestPoweredSkill(chr, 0x10)) {
        if ((u32)id < 0xB) {
            val -= val * 3 / 10;
        }
    }
    val = BtlAtk_ApplyAbilities(chr, val, id);
    hits = BtlObjAnim_QueryEvent(obj, 1, 0, 3);
    if (hits >= 2) {
        val /= hits;
    }
    return val;
}

/* Record +0x0C: first of two values the throw setup (0x1FC008) reads. */
s32 BtlAtk_GetThrowParamC(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->throwC;
}

/* Record +0x0E: the second. */
s32 BtlAtk_GetThrowParamE(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->throwE;
}

/* Ki the attacker gains per hit: record +0x0A, +50% (ability 0x6E) or +25% (0x20), divided by the hit count. */
s32 BtlAtk_GetKiGain(BtlTechChr *chr) {
    BtlTechObj *obj = BtlChar_GetObj(chr);
    s32 val = BtlAtk_GetRecord(chr, NULL)->kiGain;
    s32 hits;

    if (BtlMember_HasAbility(chr, 0x6E)) {
        val += (u32)val / 2;
    } else if (BtlMember_HasAbility(chr, 0x20)) {
        val += val / 4;
    }
    hits = BtlObjAnim_QueryEvent(obj, 1, 0, 3);
    if (hits > 0) {
        val /= hits;
    }
    return val;
}

/* Ki the defender pays when it guards: record +0x08 * curve row 5, x2 (ability 0x44) or x1.5 (0x6D). */
s32 BtlAtk_GetGuardKiCost(BtlTechChr *chr) {
    s32 val = BtlAtk_GetRecord(chr, NULL)->guardKi;

    val = val * BtlStat_GetGuardKiCostScale(chr);
    if (BtlMember_HasAbility(chr, 0x44)) {
        val *= 2;
    } else if (BtlMember_HasAbility(chr, 0x6D)) {
        val += val / 2;
    }
    return val;
}

/* Record +0x10 divided by the hit count: what a hit adds to the fighter's +0xD80 gauge (decays 400 per frame). */
s32 BtlAtk_GetUnk10(BtlTechChr *chr) {
    BtlTechObj *obj = BtlChar_GetObj(chr);
    s32 val = BtlAtk_GetRecord(chr, NULL)->unk10;
    s32 hits = BtlObjAnim_QueryEvent(obj, 1, 0, 3);

    if (hits > 0) {
        val /= hits;
    }
    return val;
}

/* Reaction id, record +0x1B. */
s32 BtlAtk_GetReaction(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->react;
}

/* Reaction id +0x1C (defender animation flag 0x800), or +0x1B when it is 0. */
s32 BtlAtk_GetReactionB(BtlTechChr *chr) {
    BtlAtkRecord *rec = BtlAtk_GetRecord(chr, NULL);

    return rec->reactB != 0 ? rec->reactB : rec->react;
}

/* Reaction id +0x1F (defender animation flag 0x4000 or action 0xDF), or +0x1B when it is 0. */
s32 BtlAtk_GetReactionC(BtlTechChr *chr) {
    BtlAtkRecord *rec = BtlAtk_GetRecord(chr, NULL);

    return rec->reactC != 0 ? rec->reactC : rec->react;
}

/* Reaction id +0x1D (defender animation flag 0x1000), or +0x1B when it is 0. */
s32 BtlAtk_GetReactionD(BtlTechChr *chr) {
    BtlAtkRecord *rec = BtlAtk_GetRecord(chr, NULL);

    return rec->reactD != 0 ? rec->reactD : rec->react;
}

/* Reaction id +0x1E (defender animation flag 0x2000), or +0x1B when it is 0. */
s32 BtlAtk_GetReactionE(BtlTechChr *chr) {
    BtlAtkRecord *rec = BtlAtk_GetRecord(chr, NULL);

    return rec->reactE != 0 ? rec->reactE : rec->react;
}

/* Record +0x20: stored in the defender's reaction block (+0xFB4). */
s32 BtlAtk_GetReactionSub(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->reactSub;
}

/* Reaction id +0x21 (defender animation flag 0x100, hit from the front), or +0x1B when it is 0. */
s32 BtlAtk_GetReactionF(BtlTechChr *chr) {
    BtlAtkRecord *rec = BtlAtk_GetRecord(chr, NULL);

    return rec->reactF != 0 ? rec->reactF : rec->react;
}

/* Reaction id +0x22 (defender animation 0xAE / 0xAF or flag 0x200), or +0x1B when it is 0. */
s32 BtlAtk_GetReactionG(BtlTechChr *chr) {
    BtlAtkRecord *rec = BtlAtk_GetRecord(chr, NULL);

    return rec->reactG != 0 ? rec->reactG : rec->react;
}

/* Record +0x00: the attack's flag word. */
u32 BtlAtk_GetFlags(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->flags;
}

/* Push speed on hit: record +0x12 in units of 10 km/h, per frame. */
f32 BtlAtk_GetPushOnHit(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->pushHit * 10.0f * BTL_KMH(1.0f);
}

/* Push speed on guard: record +0x13 in units of 10 km/h, per frame. */
f32 BtlAtk_GetPushOnGuard(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->pushGuard * 10.0f * BTL_KMH(1.0f);
}

/* Record +0x14 in units of 10 km/h, per frame, times curve row 8: the speed the attack handlers give the attacker. */
f32 BtlAtk_GetLaunchSpeed(BtlTechChr *chr) {
    f32 v = BtlAtk_GetRecord(chr, NULL)->launchSpeed * 10.0f * BTL_KMH(1.0f);

    return v * BtlStat_GetSpeedScale(chr);
}

/* Record +0x15 (degrees) in radians: yaw offset of the launch direction. */
f32 BtlAtk_GetLaunchAngleA(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->launchA * 3.14159265f / 180.0f;
}

/* The same of record `id`. */
f32 BtlAtk_GetLaunchAngleAOf(BtlTechChr *chr, u32 id) {
    return BtlAtk_GetRecordOf(chr, id)->launchA * 3.14159265f / 180.0f;
}

/* Record +0x16 (degrees) in radians: pitch of the launch direction. */
f32 BtlAtk_GetLaunchAngleB(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->launchB * 3.14159265f / 180.0f;
}

/* The same of record `id`. */
f32 BtlAtk_GetLaunchAngleBOf(BtlTechChr *chr, u32 id) {
    return BtlAtk_GetRecordOf(chr, id)->launchB * 3.14159265f / 180.0f;
}

/* Record +0x17 as a float: camera shake power on a hit. */
f32 BtlAtk_GetShakePower(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->shakePower;
}

/* Record +0x18 (tenths of a second) in seconds: camera shake time on a hit. */
f32 BtlAtk_GetShakeTime(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->shakeTime * 0.1f;
}

/* Record +0x19 as a float: camera shake power on a guard. */
f32 BtlAtk_GetShakePowerB(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->shakePowerB;
}

/* Record +0x1A (tenths of a second) in seconds: camera shake time on a guard. */
f32 BtlAtk_GetShakeTimeB(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->shakeTimeB * 0.1f;
}

/* Record +0x23: hit effect kind (1, 2 | 3, 4 | 10 pick the effect; non-zero sets effect bit 0x1D on the defender). */
s32 BtlAtk_GetHitFxKind(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->hitFx;
}

/* Record +0x24: level that picks the hit sound line. */
s32 BtlAtk_GetHitSoundLevel(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->hitSound;
}

/* Voice kind of the attack: record +0x25 mapped 1, 2, 3 -> 5, 6, 7; -1 (none) otherwise. */
s32 BtlAtk_GetVoiceKind(BtlTechChr *chr) {
    switch (BtlAtk_GetRecord(chr, NULL)->voice) {
        case 0:
            return -1;
        case 1:
            return 5;
        case 2:
            return 6;
        case 3:
            return 7;
    }
    return -1;
}

/* Guard result against guard kind 1 (record +0x29); result 1 becomes 0 when the opponent has ability 0x40. */
s32 BtlAtk_GetGuardKindA(BtlTechChr *chr) {
    s32 id;
    s32 v = BtlAtk_GetRecord(chr, &id)->guardA;

    if (v == 1) {
        v = !BtlOpp_HasAbility(chr, 0x40);
    }
    return v;
}

/* Guard result against guard kind 2 (record +0x27); result 1 becomes 0 for ids 0x13..0x21 when the opponent has
   ability 0x40. */
s32 BtlAtk_GetGuardKindB(BtlTechChr *chr) {
    s32 id;
    s32 v = BtlAtk_GetRecord(chr, &id)->guardB;

    if (v == 1 && BtlOpp_HasAbility(chr, 0x40)) {
        v = !BtlAtk_IsId13to21(id);
    }
    return v;
}

/* The same against guard kind 3 (record +0x28). */
s32 BtlAtk_GetGuardKindC(BtlTechChr *chr) {
    s32 id;
    s32 v = BtlAtk_GetRecord(chr, &id)->guardC;

    if (v == 1 && BtlOpp_HasAbility(chr, 0x40)) {
        v = !BtlAtk_IsId13to21(id);
    }
    return v;
}

/* Rank of a guard result: 4 > 0 > 1 > 2 > 3. */
s32 BtlAtk_RankGuardKind(u32 kind) {
    switch (kind) {
        case 4:
            return 5;
        case 0:
            return 4;
        case 1:
            return 3;
        case 2:
            return 2;
        case 3:
            return 1;
    }
    return 0;
}

/* Against guard kind 4: the highest-ranked of the record's three raw guard results (+0x29, +0x27, +0x28). */
s32 BtlAtk_GetBestGuardKind(BtlTechChr *chr) {
    s32 i = 0;
    s32 v = 0;
    s32 bestRank = -1;
    s32 best = -1;
    BtlAtkRecord *rec = BtlAtk_GetRecord(chr, NULL);
    s32 rank;

    for (; i < 3; i++) {
        switch (i) {
            case 0:
                v = rec->guardA;
                break;
            case 1:
                v = rec->guardB;
                break;
            case 2:
                v = rec->guardC;
                break;
        }
        rank = BtlAtk_RankGuardKind(v);
        if (bestRank < rank) {
            bestRank = rank;
            best = v;
        }
    }
    return best;
}

/* Record +0x2A: guard result against guard kind 5. */
s32 BtlAtk_GetGuardKindE(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->guardE;
}

/* Record +0x2B: 0 / 1 selects defender flag 0x66 / 0x67 on a guard. */
s32 BtlAtk_GetGuardFlagSel(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->guardFlagSel;
}

/* Record +0x2C: armour levels the attack ignores. */
s32 BtlAtk_GetUnk2C(BtlTechChr *chr) {
    return BtlAtk_GetRecord(chr, NULL)->armorIgnore;
}

/* +0x2C of record `id`. */
s32 BtlAtk_GetUnk2COf(BtlTechChr *chr, u32 id) {
    return BtlAtk_GetRecordOf(chr, id)->armorIgnore;
}

/* The ki charge rate scaled by ability: x0.4 (0x31), x0.6 (0x30) or x0.8 (0x2F), then x1.25 (0x75). */
s32 BtlParam_ScaleKiCharge(BtlTechChr *chr, s32 val) {
    if (BtlMember_HasAbility(chr, 0x31)) {
        val = val * 4 / 10;
    } else if (BtlMember_HasAbility(chr, 0x30)) {
        val = val * 6 / 10;
    } else if (BtlMember_HasAbility(chr, 0x2F)) {
        val = val * 8 / 10;
    }
    if (BtlMember_HasAbility(chr, 0x75)) {
        val = val * 125 / 100;
    }
    return val;
}

/* ---- character parameter block ---------------------------------------------------------------------------------- */

/* Parameter +0x10: flag word. */
u32 BtlParam_GetFlags(BtlTechChr *chr) {
#ifdef PORT
    if (PortGame_IsBT4()) {
        /* BT4 moved the four character flag words to common file 2, part 12. */
        u32 *pack = gCommonRes->data[0];
        if (pack[12] == pack[13]) return 0;
        return ((u32 *)((u8 *)pack + (pack[12] & ~3u)))[BtlChar_GetObj(chr)->chara * 4 + 1];
    }
#endif
    return BtlChar_GetObj(chr)->param->flags;
}

/* Parameter +0x14: skill bits. */
u32 BtlParam_GetFlags2(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->flags2;
}

/* Parameter +0x18: skill bits that also need the powered-up mode. */
u32 BtlParam_GetFlags3(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->flags3;
}

/* Parameter +0x00 (u16). */
s32 BtlParam_GetUnk0(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unk0;
}

/* Parameter +0x02: 4 is tested by the hit code (not thrown, not rushed). */
s32 BtlParam_GetUnk2(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unk2;
}

/* Parameter +0xAC: the transformation slot a neutral input picks. */
s32 BtlParam_GetUnkAC(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->transDefault;
}

/* Parameter +0x98 + slot: transformation target, 0xFF = none. */
s32 BtlParam_GetSlotId(BtlTechChr *chr, s32 slot) {
    return BtlChar_GetObj(chr)->param->transTarget[slot];
}

/* Parameter +0x9C + slot, times 100000: blast gauge a transformation costs. */
s32 BtlParam_GetSlotCost(BtlTechChr *chr, s32 slot) {
#ifdef PORT
    if (PortGame_IsBT4()) return BtlChar_GetObj(chr)->param->transCost[slot] * 20000;
#endif
    return BtlChar_GetObj(chr)->param->transCost[slot] * 100000;
}

/* Parameter +0xA0 + slot. */
s32 BtlParam_GetSlotA0(BtlTechChr *chr, s32 slot) {
    return BtlChar_GetObj(chr)->param->transSeq[slot];
}

/* Parameter +0xA4 + slot. */
s32 BtlParam_GetSlotA4(BtlTechChr *chr, s32 slot) {
    return BtlChar_GetObj(chr)->param->transKind[slot];
}

/* Parameter +0xA8 + slot. */
s32 BtlParam_GetSlotA8(BtlTechChr *chr, s32 slot) {
#ifdef PORT
    if (PortGame_IsBT4()) return ((s16 *)((u8 *)BtlChar_GetObj(chr)->param + 0xCC))[slot];
#endif
    return BtlChar_GetObj(chr)->param->transA8[slot];
}

#ifdef PORT
s32 BtlParam_GetTransformPropCostume(BtlTechChr *chr,s32 slot) {
    return ((u8 *)BtlChar_GetObj(chr)->param)[0xD4 + slot * 4 + BtlMember_GetActive(chr)->costume];
}
#endif

/* Parameter +0xAD. */
s32 BtlParam_GetUnkAD(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unkAD;
}

/* How many of the four transformation targets at +0x98 are not 0xFF. */
s32 BtlParam_CountSlots(BtlTechChr *chr) {
    BtlParam *param = BtlChar_GetObj(chr)->param;
    s32 n = 0;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (param->transTarget[i] != 0xFF) {
            n++;
        }
    }
    return n;
}

/* Bit 4 + n of parameter +0xAD. */
s32 BtlParam_TestUnkADBit(BtlTechChr *chr, s32 n) {
    return (BtlChar_GetObj(chr)->param->unkAD & (0x10 << n)) != 0;
}

/* Parameter +0xB4 + n: fusion n. */
s32 BtlParam_GetUnkB4(BtlTechChr *chr, s32 n) {
    return BtlChar_GetObj(chr)->param->fusionResult[n];
}

/* Parameter +0xB7 + n: fusion n. */
s32 BtlParam_GetUnkB7(BtlTechChr *chr, s32 n) {
    return BtlChar_GetObj(chr)->param->fusionB7[n];
}

/* Parameter +0xBA + n * 4 + m: partner m of fusion n. */
s32 BtlParam_GetUnkBA(BtlTechChr *chr, s32 n, s32 m) {
    return BtlChar_GetObj(chr)->param->fusionPartner[n][m];
}

/* Parameter +0xAE + n, times 100000: blast gauge fusion n costs. */
s32 BtlParam_GetCostAE(BtlTechChr *chr, s32 n) {
    return BtlChar_GetObj(chr)->param->fusionCost[n] * 100000;
}

/* Parameter +0xC6. */
s32 BtlParam_GetUnkC6(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unkC6;
}

/* Parameter +0xB1 + n: fusion n. */
s32 BtlParam_GetUnkB1(BtlTechChr *chr, s32 n) {
    return BtlChar_GetObj(chr)->param->fusionSeq[n];
}

/* Aura kind (passed to BtlObj_SetColorPreset on the object): parameter +0x03, replaced by 0..10 when the member has one of
   abilities 0x4D..0x53, 0x74, 0x54..0x56 (first match in that order). */
s32 BtlParam_GetAuraKind(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->auraKind;

    if (BtlMember_HasAbility(chr, 0x4D)) {
        v = 0;
    } else if (BtlMember_HasAbility(chr, 0x4E)) {
        v = 1;
    } else if (BtlMember_HasAbility(chr, 0x4F)) {
        v = 2;
    } else if (BtlMember_HasAbility(chr, 0x50)) {
        v = 3;
    } else if (BtlMember_HasAbility(chr, 0x51)) {
        v = 4;
    } else if (BtlMember_HasAbility(chr, 0x52)) {
        v = 5;
    } else if (BtlMember_HasAbility(chr, 0x53)) {
        v = 6;
    } else if (BtlMember_HasAbility(chr, 0x74)) {
        v = 7;
    } else if (BtlMember_HasAbility(chr, 0x54)) {
        v = 8;
    } else if (BtlMember_HasAbility(chr, 0x55)) {
        v = 9;
    } else if (BtlMember_HasAbility(chr, 0x56)) {
        v = 10;
    }
    return v;
}

/* Search type 0..9 of the active member's costume (parameter +0x58 + costume, costume above 3 reads 0); types 2..4
   become 7 (8 for type 4) while the member's gauge word +0x20 is set. */
s32 BtlParam_GetType(BtlTechChr *chr) {
    BtlParam *param = BtlChar_GetObj(chr)->param;
    u32 costume = BtlMember_GetActive(chr)->costume;
    s32 type;

    if (costume >= 4) {
        costume = 0;
    }
    type = param->searchType[costume];
#ifdef PORT
    if (PortGame_IsBT4()) type = (s8)BtlChar_GetObj(chr)->modelInfo[0x355];
#endif
    switch (type) {
        case 2:
        case 3:
        case 4:
            if (BtlMember_GetActiveGauge(chr)->variant != 0) {
                if (type == 4) {
                    type = 8;
                } else {
                    type = 7;
                }
            }
            break;
    }
    return type;
}

/* Radians per frame the search window widens, by search type; doubled by ability 0x6B (BtlAct_GrowWindow).
   The ten constants are written as literals: no expression was found for them (1.29, 1.58, 3.60, 6.31, 16.8, 1.26,
   1.74, 0.97, 1.12, 1.44 degrees per frame). The bits were compared with 0x2F1878. */
f32 BtlParam_GetTypeValueA(BtlTechChr *chr) {
    f32 tbl[BTL_TYPE_COUNT] = {
        0.02257771464f, 0.0275165909f, 0.06289506145f, 0.1100663636f, 0.2935102955f,
        0.02201326983f, 0.03036313364f, 0.01693328703f, 0.01956735225f, 0.02515802393f,
    };
    f32 v = tbl[BtlParam_GetType(chr)];

    if (BtlMember_HasAbility(chr, 0x6B)) {
        v += v;
    }
    return v;
}

/* Distance per frame the search window grows, by search type (3000..40000 km/h); doubled by ability 0x6B. */
f32 BtlParam_GetTypeValueB(BtlTechChr *chr) {
    f32 tbl[BTL_TYPE_COUNT] = {
        BTL_KMH(3000.0f), BTL_KMH(3500.0f), BTL_KMH(20000.0f), BTL_KMH(30000.0f), BTL_KMH(40000.0f),
        BTL_KMH(3000.0f), BTL_KMH(4000.0f), BTL_KMH(2000.0f), BTL_KMH(2500.0f), BTL_KMH(3000.0f),
    };
    f32 v = tbl[BtlParam_GetType(chr)];

    if (BtlMember_HasAbility(chr, 0x6B)) {
        v += v;
    }
    return v;
}

/* A third value by search type (0.1..0.5); no caller found. */
f32 BtlParam_GetTypeValueC(BtlTechChr *chr) {
    f32 tbl[BTL_TYPE_COUNT] = {
        0.15f, 0.2f, 0.4f, 0.45f, 0.5f, 0.2f, 0.35f, 0.1f, 0.15f, 0.2f,
    };

    return tbl[BtlParam_GetType(chr)];
}

/* The search type is 2..4 (the search action then plays animation 0x17F instead of 0x17E). */
s32 BtlParam_IsType2to4(BtlTechChr *chr) {
    switch (BtlParam_GetType(chr)) {
        case 2:
        case 3:
        case 4:
            return 1;
    }
    return 0;
}

/* The search type is 2, 3, 4, 6 or 9 (no caller found). */
s32 BtlParam_IsTypeSetA(BtlTechChr *chr) {
    switch (BtlParam_GetType(chr)) {
        case 2:
        case 3:
        case 4:
        case 6:
        case 9:
            return 1;
    }
    return 0;
}

/* The search type is 0..4, 6 or 9 (no caller found). */
s32 BtlParam_IsTypeSetB(BtlTechChr *chr) {
    switch (BtlParam_GetType(chr)) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 9:
            return 1;
    }
    return 0;
}

/* How many class-0 ki blasts may be alive at once: parameter +0x80 less 3 / 2 / 1 by ability 0x11 / 0x10 / 0xF, but
   at least 1 when the raw value is positive. */
s32 BtlParam_GetCount80(BtlTechChr *chr) {
    BtlParam *param = BtlChar_GetObj(chr)->param;
    s32 v = param->blastLimitA;

    if (BtlMember_HasAbility(chr, 0x11)) {
        v -= 3;
    } else if (BtlMember_HasAbility(chr, 0x10)) {
        v -= 2;
    } else if (BtlMember_HasAbility(chr, 0xF)) {
        v -= 1;
    }
    if (param->blastLimitA > 0) {
        if (v <= 0) {
            v = 1;
        }
    }
    return v;
}

/* Parameter +0x82: the same limit for class-1 blasts. */
s32 BtlParam_GetUnk82(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->blastLimitB;
}

/* Parameter +0x84 + n (n above 3 reads entry 0). */
s32 BtlParam_GetUnk84(BtlTechChr *chr, u32 n) {
    BtlParam *param = BtlChar_GetObj(chr)->param;

    if (n >= 4) {
        n = 0;
    }
    return param->unk84[n];
}

/* Parameter +0x88 + n (n above 1 reads entry 0). */
s32 BtlParam_GetUnk88(BtlTechChr *chr, u32 n) {
    BtlParam *param = BtlChar_GetObj(chr)->param;

    if (n >= 2) {
        n = 0;
    }
    return param->unk88[n];
}

/* Parameter +0x8A + n (n above 4 reads entry 0). */
s32 BtlParam_GetUnk8A(BtlTechChr *chr, u32 n) {
    BtlParam *param = BtlChar_GetObj(chr)->param;

    if (n >= 5) {
        n = 0;
    }
    return param->unk8A[n];
}

/* Parameter +0x1C. */
s32 BtlParam_GetUnk1C(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unk1C;
}

/* Parameter +0x20. */
s32 BtlParam_GetUnk20(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unk20;
}

/* Parameter +0x24. */
s32 BtlParam_GetUnk24(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unk24;
}

/* Ki level below which ki regenerates by itself: parameter +0x28 plus 60000 / 40000 / 20000 by ability 0x2E / 0x2D /
   0x2C, at most 100000. */
s32 BtlParam_GetKiRegenLimit(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->kiRegenLimit;

    if (BtlMember_HasAbility(chr, 0x2E)) {
        v += 60000;
    } else if (BtlMember_HasAbility(chr, 0x2D)) {
        v += 40000;
    } else if (BtlMember_HasAbility(chr, 0x2C)) {
        v += 20000;
    }
    if (v > 100000) {
        v = 100000;
    }
    return v;
}

/* Parameter +0x2C: ki level that ends the ki-exhausted state (flag 0xBE). */
s32 BtlParam_GetGaugeB(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->kiRecoverGoal;
}

/* Parameter +0x40. */
s32 BtlParam_GetUnk40(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->unk40;
}

/* Ki gained per frame of charging: ((parameter +0x30) / 30 + curve row 0 + (fighter +0xE28) / 30) scaled by
   BtlParam_ScaleKiCharge, at least 200. */
s32 BtlParam_GetRateA(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->kiCharge / 30;

    v += BtlStat_GetKiChargeBonus(chr);
    return BtlUtil_Max(BtlParam_ScaleKiCharge(chr, v + chr->rateBonus / 30), 200);
}

/* The same from parameter +0x34: used while the fighter has flag 0x11. */
s32 BtlParam_GetRateB(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->kiChargeWater / 30;

    v += BtlStat_GetKiChargeBonus(chr);
    return BtlUtil_Max(BtlParam_ScaleKiCharge(chr, v + chr->rateBonus / 30), 200);
}

/* Ki regenerated per frame below BtlParam_GetKiRegenLimit: (parameter +0x38) / 30 + curve row 1, at least 0. */
s32 BtlParam_GetKiRegenRate(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->kiRegen / 30;

    return BtlUtil_Max(v + BtlStat_GetKiRegenBonus(chr), 0);
}

/* Ki recovered per frame in the ki-exhausted state (x4 by the caller on a button press): (parameter +0x3C) / 30 +
   curve row 2, doubled by ability 0x3C, at least 10. */
s32 BtlParam_GetKiRecoverRate(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->kiRecover / 30;

    v += BtlStat_GetKiRecoverBonus(chr);
    if (BtlMember_HasAbility(chr, 0x3C)) {
        v *= 2;
    }
    return BtlUtil_Max(v, 10);
}

/* Ki actions 0x67..0x69 spend per frame: (parameter +0x4C) / 30, halved by ability 0x28. */
s32 BtlParam_GetKiDrain67(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->kiDrain67 / 30;

    if (BtlMember_HasAbility(chr, 0x28)) {
        v /= 2;
    }
    return v;
}

/* Parameter +0x48: ki an air recovery (actions 0x25..0x2A) costs; also tested by action 0xD5 / 0xD6 / 0xDF. */
s32 BtlParam_GetRecoverKiCost(BtlTechChr *chr) {
    return BtlChar_GetObj(chr)->param->recoverKiCost;
}

/* Ki a vanishing step costs: 10000; 5000 with ability 0x23; 0 with ability 0x24. */
s32 BtlParam_GetAmountA(BtlTechChr *chr) {
    s32 v = 10000;

    if (BtlMember_HasAbility(chr, 0x24)) {
        v = 0;
    } else if (BtlMember_HasAbility(chr, 0x23)) {
        v = 5000;
    }
    return v;
}

/* Ki vanishing behind the opponent costs: 20000; 10000 with ability 0x23; 0 with ability 0x24. */
s32 BtlParam_GetAmountB(BtlTechChr *chr) {
    s32 v = 20000;

    if (BtlMember_HasAbility(chr, 0x24)) {
        v = 0;
    } else if (BtlMember_HasAbility(chr, 0x23)) {
        v = 10000;
    }
    return v;
}

/* Blast gauge gained per frame: (parameter +0x44) / 30 + curve row 3; x1.2 (ability 0x32), halved (0x1A), x0.8
   (0x33); at least 20. */
s32 BtlParam_GetBlastGainRate(BtlTechChr *chr) {
    s32 v = BtlChar_GetObj(chr)->param->blastGain / 30;

    v += BtlStat_GetBlastGainBonus(chr);
    if (BtlMember_HasAbility(chr, 0x32)) {
        v = v * 12 / 10;
    }
    if (BtlMember_HasAbility(chr, 0x1A)) {
        v /= 2;
    }
    if (BtlMember_HasAbility(chr, 0x33)) {
        v = v * 8 / 10;
    }
    return BtlUtil_Max(v, 20);
}

/* Max power gained per frame of charging at full ki: 30000 (the whole gauge) divided by the frames in
   (parameter +0x50 seconds / curve row 11). */
s32 BtlParam_GetStepA(BtlTechChr *chr) {
    f32 t = BtlChar_GetObj(chr)->param->maxPowerChargeTime;

    t /= BtlStat_GetMaxPowerChargeScale(chr);
    return 30000 / (s32)(t * 30.0f);
}

/* Max power lost per frame: 30000 divided by the frames in (parameter +0x54 + curve row 12) seconds, the time
   being at least one frame. */
s32 BtlParam_GetMaxPowerDrain(BtlTechChr *chr) {
    f32 t = BtlChar_GetObj(chr)->param->maxPowerTime;

    t += BtlStat_GetMaxPowerExtraTime(chr);
    if (t < 1.0f / 30.0f) {
        t = 1.0f / 30.0f;
    }
    return 30000 / (s32)(t * 30.0f);
}
