#include "battle/tag_balance.h"
#include "common.h"
#include "battle/btl_char_member.h"

/*
 * Team members, parameters and gauges of a fighter: 0x1CDCA8..0x1CF578.
 *
 *   0x1CDD40..0x1CE630  member entries: parameter load, lookups, the active member, the switch candidate
 *   0x1CE630..0x1CF220  health, ki, blast stock and the +0x1C gauge of the active member; abilities
 *   0x1CF220            per-frame queued damage and drain
 *   (0x1CF578..0x1D00D8, the one-frame effect request bits and the effects / sounds they spawn, is the first part
 *   of src/battle/btl_char_fx.c: BtlFx_UpdateGroundFx needs the request-bit helpers in its translation unit)
 *
 * Units: health 10000 per bar; ki 0..100000; blast stock 100000 per stock; the +0x1C gauge 0..30000.
 * Nothing here regenerates a gauge over time: this module only offers the add / spend calls. The per-frame
 * function (BtlMembers_UpdateQueuedDamage) applies damage and drain that a fighter queued against its opponent.
 * No random draw is made by the gauge code; the effect code picks sound variants with BtlChar_FrameMod.
 *
 * The file starts at 0x1CDCA8 with BtlColl_NextPoolMember: BtlMember_Damage only compiles to the original bytes
 * when that callee is defined earlier in the same file, so the original object starts there or earlier.
 *
 * Callees outside this file, as far as this file shows what they are:
 *   BtlAnim_GetFlags(BtlAnim_GetId(chr))  flags of the current animation: 0x800 halves damage taken, 8 without 0x10
 *                                         is the window in which queued drain runs
 *   BtlAnim_TestAttr(chr, 1 << 33)        the animation's "deal the queued hit now" event
 *   BtlOpp_HasAbility(chr, n)              BtlMember_HasAbility(the opponent, n)
 *   BtlParam_GetDamageTakenScale(chr)                    float at (object + 0x91C) + 0x7C: the character's damage-taken multiplier
 *   BtlStat_GetScale7(chr)                second damage-taken multiplier, from the member's bonus level
 *   BtlColl_NextPoolMember(chr)                    side 1 in mode 3: brings in the next opponent of the pool (BtlMember_Replace)
 *   EftShot_Request / EftTransform_Start / EftTransform_Flash / EftTransform_End / EftImpact_SpawnHit   effect spawners
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);
extern s32 BtlUtil_Min(s32 a, s32 b);
extern s32 BtlUtil_Max(s32 a, s32 b);
extern f32 BtlUtil_WrapAngle(f32 a);
extern s32 BtlChar_GetCount(void);
extern BtlMemberChr *BtlChar_Get(s32 i);
extern BtlMemberObj *BtlChar_GetObj(BtlMemberChr *chr);
extern BtlMemberPose *BtlChar_GetPos(BtlMemberChr *chr);
extern s32 BtlChar_IsFrozen(BtlMemberChr *chr);
extern s32 BtlChar_FrameMod(s32 n);
extern void BtlChar_Vibrate(BtlMemberChr *chr, f32 power, f32 seconds);
extern s32 BtlChar_TestFlag(BtlMemberChr *chr, s32 bit);
extern void BtlChar_ClearFlag(BtlMemberChr *chr, s32 bit);
extern void BtlEvent_Raise(s32 side, s32 ev);
extern s32 Battle_GetMode(void);
extern f32 BtlCharApi_GetChargedUnkC78(s32 objId);
extern f32 Mathf_Sin(f32 angle);
extern f32 Mathf_Cos(f32 angle);
extern s32 BtlAnim_GetId(BtlMemberChr *chr);
extern s32 BtlAnim_GetFlags(s32 state);
extern f32 BtlAnim_GetFrame(BtlMemberChr *chr);
extern s32 BtlAnim_TestAttr(BtlMemberChr *chr, u64 mask);
extern f32 BtlStat_GetScale7(BtlMemberChr *chr);
extern s32 BtlOpp_GetPlayer(BtlMemberChr *chr);
extern s32 BtlOpp_HasAbility(BtlMemberChr *chr, s32 n);
extern f32 BtlParam_GetDamageTakenScale(BtlMemberChr *chr);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);

extern f32 BtlSkill_GetShotUnk30(BtlMemberChr *chr, s32 kind);
extern f32 BtlSkill_GetShotTime(BtlMemberChr *chr, s32 kind);
extern f32 BtlSkill_GetShotSpeed(BtlMemberChr *chr, s32 kind);
extern f32 BtlSkill_GetShotTurnRate(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotUnk6C(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotTime(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotSpeed(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotTurnRate(BtlMemberChr *chr, s32 kind);
extern void EftShot_Request(BtlMemberAuraReq *req);
extern void EftTransform_Start(BtlMemberFx0Req *req);
extern void EftTransform_Flash(void);
extern void EftTransform_End(void);
extern s32 BtlAtk_GetHitFxKind(BtlMemberChr *chr);
extern s32 BtlAtk_GetHitSoundLevel(BtlMemberChr *chr);
extern s32 BtlCharApi_HasWeaponOut(s32 objId);
extern s32 BtlAtk_GetFlags(BtlMemberChr *chr);
extern f32 BtlAtk_GetLaunchAngleA(BtlMemberChr *chr);
extern f32 BtlAtk_GetLaunchAngleB(BtlMemberChr *chr);
extern void BtlOpp_GetTargetPos(BtlMemberChr *chr, Vec4 *out);
extern f32 BtlOpp_GetRadius(BtlMemberChr *chr);
extern void EftImpact_SpawnHit(BtlMemberHitFxReq *req);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern u32 BtlObjAnim_QueryEvent(BtlMemberObj *obj, u64 a, s32 b, s32 c);
extern s32 BtlObjAnim_MaskToNode(u32 mask);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharSnd_PlayCommon(BtlMemberChr *chr, s32 line);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);

extern BtlMemberRoster *gBtlChars;

/* Opponent queue of mode 3 (battle work + 0x5A8); local view of BattleMemberPool. */
typedef struct BtlMemberPool {
    /* 0x00 */ s32 cur;
    /* 0x04 */ s32 count;
    /* 0x08 */ u8 members[50][0x64];
} BtlMemberPool;

extern BtlMemberPool *Battle_GetWork5A8(void);

/* Mode 3: brings in the next opponent of the pool for side 1. 0x1CDCA8, in front of the member code proper: it is
 * the first function of this file because BtlMember_Damage only compiles to the original bytes with it defined
 * above (the functions before it are src/battle/btl_char_coll_b.c). */
void BtlColl_NextPoolMember(BtlMemberChr *chr) {
    BtlMemberPool *pool;
    s32 next;

    if (chr->side != 1) {
        return;
    }
    pool = Battle_GetWork5A8();
    if (pool == NULL) {
        return;
    }
    next = pool->cur + 1;
    if (next < pool->count) {
        if (BtlMember_Replace(chr, (BtlMemberSetup *)pool->members[next]) >= 0) {
            pool->cur = next;
        }
    }
}

/* Loads a member's gauge maxima from the character table and its abilities; with init also the starting values. */
void BtlMember_LoadParams(BtlMemberChr *chr, s32 member, s32 init, f32 healthPct, s32 variant) {
    BtlMember *m = BtlMember_Get(chr, member);
    BtlMemberGauge *g = &m->gauge;

    if (init) {
        g->unk30 = 0;
        g->fused = 0;
    }
    if (g->fused == 0) {
        g->healthMax = gBtlChars->paramTbl[m->chara].healthMax;
        if (BtlMember_HasAbilityOf(chr, member, 100)) {
            g->healthMax += 30000;
        } else if (BtlMember_HasAbilityOf(chr, member, 99)) {
            g->healthMax += 20000;
        } else if (BtlMember_HasAbilityOf(chr, member, 98)) {
            g->healthMax += 10000;
        }
        if (BtlMember_HasAbilityOf(chr, member, 103)) {
            g->healthMax -= 30000;
        } else if (BtlMember_HasAbilityOf(chr, member, 102)) {
            g->healthMax -= 20000;
        } else if (BtlMember_HasAbilityOf(chr, member, 101)) {
            g->healthMax -= 10000;
        }
        g->healthMax = BtlUtil_Max(g->healthMax, 10000);
    }
    g->kiMax = 100000;
    g->blastMax = gBtlChars->paramTbl[m->chara].blastStocks * 100000;
    if (init) {
        g->health = g->healthMax * healthPct / 100.0f;
        g->health = BtlUtil_Max(g->health, 1);
        g->healthMax = BtlUtil_Max(g->healthMax, g->health);
        g->ki = gBtlChars->paramTbl[m->chara].kiStart;
        if (BtlMember_HasAbilityOf(chr, member, 0x18)) {
            g->ki = 0;
        } else if (BtlMember_HasAbilityOf(chr, member, 0x19)) {
            g->ki = g->kiMax;
        } else if (BtlMember_HasAbilityOf(chr, member, 0x17)) {
            g->ki += 40000;
        } else if (BtlMember_HasAbilityOf(chr, member, 0x16)) {
            g->ki += 20000;
        }
        g->ki = BtlUtil_Min(g->ki, g->kiMax);
        g->blast = 0;
        if (BtlMember_HasAbilityOf(chr, member, 0x1A)) {
            g->blast = g->blastMax;
        }
        g->variant = variant;
        g->maxPower = 0;
    }
}

/* Returns member entry n. */
BtlMember *BtlMember_Get(BtlMemberChr *chr, s32 n) {
    return &chr->members[n];
}

/* Returns the entry of the member that is fighting. */
BtlMember *BtlMember_GetActive(BtlMemberChr *chr) {
    return BtlMember_Get(chr, chr->active);
}

/* Returns the present member that is the given character, or NULL. */
BtlMember *BtlMember_FindPresent(BtlMemberChr *chr, s32 chara) {
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        if (BtlMember_Get(chr, i)->present != 0 && BtlMember_Get(chr, i)->chara == chara) {
            return BtlMember_Get(chr, i);
        }
    }
    return NULL;
}

/* Returns the index of the present member that is the given character, or -1. */
s32 BtlMember_FindPresentIndex(BtlMemberChr *chr, s32 chara) {
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        if (BtlMember_Get(chr, i)->present != 0 && BtlMember_Get(chr, i)->chara == chara) {
            return i;
        }
    }
    return -1;
}

/* Returns the gauge block of member n. */
BtlMemberGauge *BtlMember_GetGauge(BtlMemberChr *chr, s32 n) {
    return &BtlMember_Get(chr, n)->gauge;
}

/* Returns the gauge block of the member that is fighting. */
BtlMemberGauge *BtlMember_GetActiveGauge(BtlMemberChr *chr) {
    return &BtlMember_GetActive(chr)->gauge;
}

/* Rebuilds the first defeated, non-active slot from a setup member; returns the slot or -1. */
s32 BtlMember_Replace(BtlMemberChr *chr, BtlMemberSetup *src) {
    s32 slot = -1;
    s32 i;
    BtlMember *m;
    f32 health;

    for (i = 0; i < BTL_MEMBER_SLOTS; i++) {
        if (chr->active != i && BtlMember_GetGauge(chr, i)->health <= 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return -1;
    }
    chr->memberCount = BtlUtil_Max(slot + 1, chr->memberCount);
    m = &chr->members[slot];
    memset(m, 0, sizeof(BtlMember));
    m->present = 1;
    m->chara = src->chara;
    m->costume = src->costume;
    m->cpuLevel = src->cpuLevel;
    m->aiType = src->aiType;
    for (i = 0; i < 4; i++) {
        m->ability[i] = src->ability[i];
    }
    BtlMember_LoadParams(chr, slot, 1, src->health, src->variant);
    m->bonus[0] = src->bonus[1];
    m->bonus[1] = src->bonus[2];
    m->bonus[2] = src->bonus[3];
    m->bonus[3] = src->bonus[4];
    m->bonus[4] = src->bonus[5];
    m->bonus[5] = src->bonus[6];
    m->bonus[6] = src->bonus[7];
    return slot;
}

/* Returns the index of the member that is the given character (present or not), or -1. */
s32 BtlMember_FindIndex(BtlMemberChr *chr, s32 chara) {
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        if (BtlMember_Get(chr, i)->chara == chara) {
            return i;
        }
    }
    return -1;
}

/* Sets which member is fighting. */
void BtlMember_SetActiveIndex(BtlMemberChr *chr, s32 n) {
    chr->active = n;
}

/* Counts the present members that still have health. */
s32 BtlMember_CountAlive(BtlMemberChr *chr) {
    s32 n = 0;
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        if (BtlMember_Get(chr, i)->present != 0) {
            if (BtlMember_GetGauge(chr, i)->health > 0) {
                n++;
            }
        }
    }
    return n;
}

/* Returns which member is fighting. */
s32 BtlMember_GetActiveIndex(BtlMemberChr *chr) {
    return chr->active;
}

/* Returns the switch candidate after moving it off a defeated member and off the active one. */
s32 BtlMember_GetSwitchTarget(BtlMemberChr *chr) {
    if (BtlMember_GetGauge(chr, chr->switchTarget)->health <= 0) {
        BtlMember_NextSwitchTarget(chr);
    }
    if (chr->switchTarget == chr->active) {
        BtlMember_NextSwitchTarget(chr);
    }
    return chr->switchTarget;
}

/* Sets the switch candidate; returns 1 if it was acceptable as given. */
s32 BtlMember_SetSwitchTarget(BtlMemberChr *chr, s32 n) {
    chr->switchTarget = n;
    return n == BtlMember_GetSwitchTarget(chr);
}

/* Moves the switch candidate to the next living member that is not fighting. */
void BtlMember_NextSwitchTarget(BtlMemberChr *chr) {
    s32 i;
    s32 n;

    for (i = 0; i < chr->memberCount; i++) {
        n = (chr->switchTarget + i + 1) % chr->memberCount;
        if (n != chr->active && BtlMember_GetGauge(chr, n)->health > 0) {
            chr->switchTarget = n;
            return;
        }
    }
}

/* Moves the switch candidate to the previous living member that is not fighting. */
void BtlMember_PrevSwitchTarget(BtlMemberChr *chr) {
    s32 i;
    s32 n;

    for (i = chr->memberCount - 1; i >= 0; i--) {
        n = (chr->switchTarget + i) % chr->memberCount;
        if (n != chr->active && BtlMember_GetGauge(chr, n)->health > 0) {
            chr->switchTarget = n;
            return;
        }
    }
}

/* Takes health from the active member after every modifier; returns the health actually lost.
 *
 * In order: halved when the current animation has flag 0x800; combo scaling from the 5th hit (3% less per hit down to
 * 50%, or 2% down to 70% when the attacker has ability 0x5D); the two defense multipliers (a non-zero amount never
 * drops below 1); drain is cancelled by ability 0x3F and halved by 0x3E; ability 0x76 takes no damage at all; nothing
 * happens outside fighter flag 3 unless BTL_DMG_ANY_STATE; amounts of 10 or more are rounded up to a multiple of 10;
 * modes 5 and 6, drain and BTL_DMG_NO_KILL leave at least 1 health. Then the combo counters, the KO events
 * (0x4A for the side, 0x48 and 0x51..0x53 for the opponent), event 0x59 for any loss, and on reaching 0 the ki, blast
 * and +0x1C gauges are emptied.
 */
/* Only matches with BtlColl_NextPoolMember defined above in this file (bgtz, not bgtzl, after BtlMember_CountAlive). */
s32 BtlMember_Damage(BtlMemberChr *chr, s32 amount, s32 flags) {
    BtlMemberCombo *combo = &chr->combo;
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);
    s32 before = g->health;
    s32 health;

    if (!(flags & BTL_DMG_NO_HALVE)) {
        if (BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x800) {
            amount /= 2;
        }
    }
    if (!(flags & BTL_DMG_NO_SCALING)) {
        s32 n = combo->hits - 4;

        if (n > 0) {
            s32 pct = BtlUtil_Max(100 - n * 3, 50);

            if (BtlOpp_HasAbility(chr, 0x5D)) {
                pct = BtlUtil_Max(100 - n * 2, 70);
            }
            amount = amount * pct / 100;
        }
    }
    if (!(flags & BTL_DMG_NO_DEFENSE)) {
        s32 second = 1;
        s32 v = amount * BtlParam_GetDamageTakenScale(chr);

        if (v <= 0 && amount > 0) {
            amount = 1;
        } else {
            amount = v;
        }
        if (flags & BTL_DMG_GUARD_MASK) {
            second = BtlOpp_HasAbility(chr, 0x4C) == 0;
        }
        if (second) {
            v = amount * BtlStat_GetScale7(chr);
            if (v <= 0 && amount > 0) {
                amount = 1;
            } else {
                amount = v;
            }
        }
    }
    if (flags & BTL_DMG_DRAIN) {
        if (BtlMember_HasAbility(chr, 0x3F)) {
            amount = 0;
        } else if (BtlMember_HasAbility(chr, 0x3E)) {
            amount /= 2;
        }
    }
    if (BtlMember_HasAbility(chr, 0x76)) {
        amount = 0;
        flags |= BTL_DMG_FORCE;
    }
    if (amount <= 0) {
        if (!(flags & BTL_DMG_FORCE)) {
            return 0;
        }
        amount = 0;
    }
    if (!BtlChar_TestFlag(chr, 3) && !(flags & BTL_DMG_ANY_STATE)) {
        return 0;
    }
    if (!(flags & BTL_DMG_EXACT) && amount >= 10) {
        s32 r = amount % 10;

        if (r != 0) {
            amount = amount - r + 10;
        }
    }
    if (Battle_GetMode() == 5 || Battle_GetMode() == 6) {
        health = g->health;
        if (amount > health - 1) {
            amount = health - 1;
        }
    } else {
        health = g->health;
    }
    if (flags & BTL_DMG_DRAIN) {
        if (amount > health - 1) {
            amount = health - 1;
        }
    }
    if (flags & BTL_DMG_NO_KILL) {
        if (amount > health - 1) {
            amount = health - 1;
        }
    }
    g->health = health - amount;
    if (amount > 0 || (flags & BTL_DMG_FORCE)) {
        if (flags & BTL_DMG_NO_HIT) {
            if (!(flags & BTL_DMG_NO_TOTAL)) {
                combo->damage += amount;
                if (!(flags & BTL_DMG_NO_TIMER)) {
                    combo->timer = (s32)(combo->damage * 30.0f * 0.0001f) + 30;
                    if (combo->timer > 90) {
                        combo->timer = 90;
                    }
                    combo->changed = 1;
                }
            }
        } else {
            combo->hits++;
            if (combo->hits == 1) {
                combo->damage = amount;
            } else {
                combo->damage += amount;
            }
            combo->timer = (s32)(combo->damage * 30.0f * 0.0001f) + 30;
            if (combo->timer > 90) {
                combo->timer = 90;
            }
            combo->newHit = 1;
            combo->changed = 1;
        }
    }
    combo->drain = (flags >> 2) & 1;
    health = g->health;
    if (health < 0) {
        g->health = 0;
        health = 0;
    }
    if (before > 0 && health == 0) {
        s32 other = BtlOpp_GetPlayer(chr);

        BtlEvent_Raise(chr->side, 0x4A);
        BtlEvent_Raise(other, 0x48);
        if (flags & BTL_DMG_KO_EV53_A) {
            BtlEvent_Raise(other, 0x53);
        }
        if (flags & BTL_DMG_KO_EV53_B) {
            BtlEvent_Raise(other, 0x53);
        }
        if (flags & BTL_DMG_KO_EV52_A) {
            BtlEvent_Raise(other, 0x52);
        }
        if (flags & BTL_DMG_KO_EV52_B) {
            BtlEvent_Raise(other, 0x52);
        }
        if (flags & BTL_DMG_KO_EV51) {
            BtlEvent_Raise(other, 0x51);
        }
    }
    if (amount > 0) {
        BtlEvent_Raise(chr->side, 0x59);
    }
    if (g->health == 0) {
        g->ki = 0;
        g->blast = 0;
        g->maxPower = 0;
        chr->unkFE0 = 0;
        BtlChar_ClearFlag(chr, 0xBE);
        if (Battle_GetMode() == 3) {
            if (BtlMember_CountAlive(chr) <= 0) {
                BtlColl_NextPoolMember(chr);
            }
        }
    }
    {extern void TagChallenge_Damage(s32,s32);TagChallenge_Damage(chr->side,before-health);}
    return before - health;
}

/* Gives health to the active member, up to its maximum. */
void BtlMember_AddHealth(BtlMemberChr *chr, s32 amount) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    if (amount > 0) {
        g->health += amount;
        if (g->health > g->healthMax) {
            g->health = g->healthMax;
        }
    }
}

/* Whether the active member has at least this much health. */
s32 BtlMember_HasHealth(BtlMemberChr *chr, s32 amount) {
    if (BtlMember_GetActiveGauge(chr)->health < amount) {
        return 0;
    }
    return 1;
}

/* Gives ki to the active member; returns what did not fit. */
s32 BtlMember_AddKi(BtlMemberChr *chr, s32 amount) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);
    s32 over = 0;

    if (amount <= 0) {
        return 0;
    }
    g->ki += amount;
    if (g->ki > g->kiMax) {
        over = g->ki - g->kiMax;
        g->ki = g->kiMax;
    }
    return over;
}

/* Whether the active member has at least this much ki. */
s32 BtlMember_HasKi(BtlMemberChr *chr, s32 amount) {
    if (BtlMember_GetActiveGauge(chr)->ki < amount) {
        return 0;
    }
    return 1;
}

/* Spends ki (nothing is spent under flag 6 unless forced); returns 1 if the gauge ran out. */
s32 BtlMember_SpendKi(BtlMemberChr *chr, s32 amount, s32 force) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);
    s32 ranOut = 0;

    if (force == 0 && BtlChar_TestFlag(chr, 6)) {
        return 0;
    }
    g->ki -= amount;
    if (g->ki < 0) {
        g->ki = 0;
        ranOut = 1;
    }
    g->maxPower = 0;
    return ranOut;
}

/* Takes ki away from the active member; returns how much was taken. */
s32 BtlMember_DrainKi(BtlMemberChr *chr, s32 amount) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);
    s32 before;

    if (amount <= 0) {
        return 0;
    }
    before = g->ki;
    g->maxPower = 0;
    g->ki = before - amount;
    if (g->ki < 0) {
        g->ki = 0;
    }
    return before - g->ki;
}

/* Adds to the blast stock gauge, up to its maximum. */
void BtlMember_AddBlast(BtlMemberChr *chr, s32 amount) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    g->blast += amount;
    if (g->blast > g->blastMax) {
        g->blast = g->blastMax;
    }
}

/* Whether the blast stock gauge holds at least this much. */
s32 BtlMember_HasBlast(BtlMemberChr *chr, s32 amount) {
    if (BtlMember_GetActiveGauge(chr)->blast < amount) {
        return 0;
    }
    return 1;
}

/* Spends blast stock, down to 0. */
void BtlMember_SubBlast(BtlMemberChr *chr, s32 amount) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    g->blast -= amount;
    if (g->blast < 0) {
        g->blast = 0;
    }
}

/* Adds to the +0x1C gauge, up to 30000. */
void BtlMember_AddMaxPower(BtlMemberChr *chr, s32 amount) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    g->maxPower += amount;
    if (g->maxPower > 30000) {
        g->maxPower = 30000;
    }
}

/* Subtracts from the +0x1C gauge, down to 0. */
void BtlMember_SubMaxPower(BtlMemberChr *chr, s32 amount) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    g->maxPower -= amount;
    if (g->maxPower < 0) {
        g->maxPower = 0;
    }
}

/* Sets the +0x1C gauge, clamped to 0..30000. */
void BtlMember_SetMaxPower(BtlMemberChr *chr, s32 value) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    g->maxPower = BtlUtil_Clamp(value, 0, 30000);
}

/* Health of the active member as a fraction of its maximum. */
f32 BtlMember_GetHealthRatio(BtlMemberChr *chr) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    if (g->healthMax > 0) {
        return (f32)g->health / (f32)g->healthMax;
    }
    return 0.0f;
}

/* Health of the whole team as a fraction of the summed maxima. */
f32 BtlMember_GetTeamHealthRatio(BtlMemberChr *chr) {
    s32 max = 0;
    s32 cur = 0;
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        BtlMemberGauge *g = BtlMember_GetGauge(chr, i);

        cur += g->health;
        max += g->healthMax;
    }
    if (max > 0) {
        return (f32)cur / (f32)max;
    }
    return 0.0f;
}

/* Ki of the active member as a fraction of its maximum. */
f32 BtlMember_GetKiRatio(BtlMemberChr *chr) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    if (g->kiMax > 0) {
        return (f32)g->ki / (f32)g->kiMax;
    }
    return 0.0f;
}

/* Whether the active member's health is at its maximum. */
s32 BtlMember_IsHealthFull(BtlMemberChr *chr) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    if (g->health < g->healthMax) {
        return 0;
    }
    return 1;
}

/* Whether the active member's ki is at its maximum. */
s32 BtlMember_IsKiFull(BtlMemberChr *chr) {
    BtlMemberGauge *g = BtlMember_GetActiveGauge(chr);

    if (g->ki < g->kiMax) {
        return 0;
    }
    return 1;
}

/* Whether the +0x1C gauge is at 30000. */
s32 BtlMember_IsMaxPowerFull(BtlMemberChr *chr) {
    if (BtlMember_GetActiveGauge(chr)->maxPower < 30000) {
        return 0;
    }
    return 1;
}

/* Whether the active member has no health left. */
s32 BtlMember_IsHealthEmpty(BtlMemberChr *chr) {
    return BtlMember_GetActiveGauge(chr)->health <= 0;
}

/* Whether the active member has no ki left. */
s32 BtlMember_IsKiEmpty(BtlMemberChr *chr) {
    return BtlMember_GetActiveGauge(chr)->ki <= 0;
}

/* Whether the +0x1C gauge is empty. */
s32 BtlMember_IsMaxPowerEmpty(BtlMemberChr *chr) {
    return BtlMember_GetActiveGauge(chr)->maxPower <= 0;
}

/* Whether the active member has ability bit n. */
s32 BtlMember_HasAbility(BtlMemberChr *chr, s32 n) {
    return BtlMember_HasAbilityOf(chr, BtlMember_GetActiveIndex(chr), n);
}

/* Whether the given member has ability bit n. */
s32 BtlMember_HasAbilityOf(BtlMemberChr *chr, s32 member, s32 n) {
    BtlMember *m = BtlMember_Get(chr, member);
    u32 mask = 1 << (n & 0x1F);

    return (m->ability[n >> 5] & mask) != 0;
}

/* Whether the active member has any of eleven listed abilities. */
s32 BtlMember_HasAnyListedAbility(BtlMemberChr *chr) {
    s32 list[11] = { 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x74 };
    s32 i;

    for (i = 0; i < 11; i++) {
        if (BtlMember_HasAbility(chr, list[i])) {
            return 1;
        }
    }
    return 0;
}

/* Per frame: deals the damage each fighter has queued against its opponent, and runs its health / ki drain. */
void BtlMembers_UpdateQueuedDamage(void) {
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlMemberChr *chr = BtlChar_Get(i);
        BtlMemberChr *other;
        s32 active;
        s32 frame;

        if (BtlChar_IsFrozen(chr)) {
            continue;
        }
        other = BtlChar_Get(BtlOpp_GetPlayer(chr));
        if (BtlAnim_TestAttr(chr, 0x200000000)) {
            if (chr->queue.hits >= 2) {
                TagBalance_Damage(other, chr->queue.perHit, chr->queue.flags, *(s32 *)chr);
                chr->queue.total -= chr->queue.perHit;
                BtlChar_Vibrate(chr, 1.0f, 0.1f);
                BtlChar_Vibrate(other, 1.0f, 0.1f);
            } else {
                TagBalance_Damage(other, chr->queue.total, chr->queue.flags, *(s32 *)chr);
                chr->queue.total = 0;
                BtlChar_Vibrate(chr, 1.0f, 0.3f);
                BtlChar_Vibrate(other, 1.0f, 0.3f);
            }
            chr->queue.hits--;
            other->unkFE4 = 30;
            if (BtlChar_TestFlag(chr, 0x13B)) {
                other->unkFE4 = 0;
            }
        }
        active = 0;
        frame = 0;
        if ((BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 8) && !(BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x10)) {
            active = 1;
            frame = BtlAnim_GetFrame(chr);
        }
        if (BtlCharApi_IsInTechnique(chr->objId) && BtlCharApi_IsInRushSequence(chr->objId)) {
            active = 1;
            frame = BtlCharApi_GetChargedUnkC78(chr->objId);
        }
        if (active && chr->queue.drainFrom < frame) {
            if (chr->queue.drainHealth > 0) {
                s32 amount;
                s32 flags;
                s32 lost;

                if (chr->queue.drainHealthStep < chr->queue.drainHealth) {
                    amount = chr->queue.drainHealthStep;
                    chr->queue.drainHealth -= amount;
                } else {
                    amount = chr->queue.drainHealth;
                    chr->queue.drainHealth = 0;
                }
                flags = chr->queue.flags;
                if (other->combo.hits <= 0) {
                    flags |= BTL_DMG_NO_DEFENSE | BTL_DMG_EXACT;
                } else {
                    flags |= BTL_DMG_NO_DEFENSE | BTL_DMG_EXACT | BTL_DMG_NO_HIT;
                }
                lost = TagBalance_Damage(other, amount, flags, *(s32 *)chr);
                if (!BtlChar_TestFlag(chr, 0x13C)) {
                    BtlMember_AddHealth(chr, lost);
                }
                other->unkFE4 = 30;
            }
            if (chr->queue.drainKi > 0) {
                s32 amount;
                s32 lost;

                if (chr->queue.drainKiStep < chr->queue.drainKi) {
                    amount = chr->queue.drainKiStep;
                    chr->queue.drainKi -= amount;
                } else {
                    amount = chr->queue.drainKi;
                    chr->queue.drainKi = 0;
                }
                lost = BtlMember_DrainKi(other, amount);
                if (!BtlChar_TestFlag(chr, 0x13C)) {
                    BtlMember_AddKi(chr, lost);
                }
                other->unkFE4 = 30;
            }
        }
        if (BtlChar_TestFlag(chr, 0x8C)) {
            if (chr->queue.total > 0) {
                TagBalance_Damage(other, chr->queue.total, chr->queue.flags, *(s32 *)chr);
            }
            chr->queue.unk0 = 0;
            chr->queue.total = 0;
            chr->queue.hits = 0;
            chr->queue.perHit = 0;
            chr->queue.flags = 0;
            chr->queue.unk14 = 0;
            chr->queue.drainHealth = 0;
            chr->queue.drainHealthStep = 0;
            chr->queue.unk20 = 0;
            chr->queue.drainKi = 0;
            chr->queue.drainKiStep = 0;
            chr->queue.drainFrom = 0;
        }
    }
}
