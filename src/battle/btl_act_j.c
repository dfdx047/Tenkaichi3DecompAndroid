#include "common.h"
#include "battle/btl_act_j.h"

/*
 * End of the fighter action decision code: 0x203168..0x204E78.
 *
 * Requests that are not plain attacks or movement: the powered-up combo finisher, transformation, fusion,
 * member switch, techniques (blast 2 / ultimate, slots 2..4) and skills (blast 1, slots 0..1); then the table
 * that turns a pending hit reaction into a damage action, the story mode (mode 1) scripted actions, and small
 * selectors the handlers call to pick one of several action ids.
 *
 * The decision functions only fill the follow-up queue with BtlAct_SetQueue and return the number of slots
 * written; their callers (BtlDecide_Main, BtlAct_CheckForced, handlers) then request BtlAct_GetQueued(chr).
 */

extern s32 Battle_GetMode(void);
extern s32 BattleSide_IsCharaUsable(s32 side, s32 chara);
extern void BtlEvent_Raise(s32 player, s32 event);
extern s32 BtlStage_HasMoon(void); /* the stage's flag word has bit 0x10 */

extern f32 BtlUtil_WrapAngle(f32 a);
extern BtlActJPose *BtlChar_GetPos(BtlActJChr *chr);
extern s32 BtlChar_IsFree(BtlActJChr *chr);
extern s32 BtlChar_IsDead(BtlActJChr *chr);
extern s32 BtlChar_TestMemberUnk70(BtlActJChr *chr);
extern s32 BtlChar_TestFlag(BtlActJChr *chr, s32 bit);
extern void BtlChar_SetFlag(BtlActJChr *chr, s32 bit);
extern void BtlChar_ClearFlag(BtlActJChr *chr, s32 bit);
extern s32 BtlInput_IsHeld(BtlActJChr *chr, u32 mask);
extern s32 BtlInput_IsPressed(BtlActJChr *chr, u32 mask);
extern s32 BtlInput_TestAction(BtlActJChr *chr, s32 id, s32 want);
extern s32 BtlAnim_GetId(BtlActJChr *chr);
extern s32 BtlAnim_GetFlags(s32 anim);
extern s32 BtlOpp_GetPlayer(BtlActJChr *chr);
extern s32 BtlOpp_GetUnkD88(BtlActJChr *chr);

extern BtlActJMember *BtlMember_Get(BtlActJChr *chr, s32 member);
extern BtlActJMember *BtlMember_GetActive(BtlActJChr *chr);
extern s32 BtlMember_FindPresentIndex(BtlActJChr *chr, s32 chara);
extern BtlActJGauge *BtlMember_GetGauge(BtlActJChr *chr, s32 member);
extern BtlActJGauge *BtlMember_GetActiveGauge(BtlActJChr *chr);
extern s32 BtlMember_CountAlive(BtlActJChr *chr);
extern s32 BtlMember_HasKi(BtlActJChr *chr, s32 amount);
extern s32 BtlMember_HasBlast(BtlActJChr *chr, s32 amount);
extern s32 BtlMember_HasAbility(BtlActJChr *chr, s32 ability);

extern void BtlAct_SetQueue(BtlActJChr *chr, u32 slot, s32 id);
extern s32 BtlAct_GetCurrent(BtlActJChr *chr);
extern s32 BtlAct_GetQueued(BtlActJChr *chr);
extern s32 BtlAct_IsTechniqueId(s32 id);
extern f32 BtlAct_GetHeightRatio(BtlActJChr *chr);
extern void BtlAct_PrepareSwitch(BtlActJChr *chr);
extern s32 BtlAct_IsAttackId(s32 id);
extern void BtlAct_ResetAttack(BtlActJChr *chr);

/* Earlier parts of the decision code (not decompiled here). */
extern s32 BtlDecide_Common(BtlActJChr *chr, s32 mask);  /* status overrides: stunned 0xD3, flag 0xBE 0xEA, landing 0x11, dead 0xD2 */
extern s32 BtlDecide_Main(BtlActJChr *chr, u32 mask);  /* the input -> action decision for a free fighter */
extern s32 BtlDecide_QueueAttack(BtlActJChr *chr, s32 attack); /* builds the queue for an attack id */

/* Character parameter block (object + 0x91C) readers. */
extern s32 BtlParam_GetUnkAC(BtlActJChr *chr);                       /* s8 + 0xAC: default transformation index */
extern s32 BtlParam_GetSlotId(BtlActJChr *chr, s32 index);            /* u8 + 0x98[index]: transformation target character, 0xFF = none */
extern s32 BtlParam_GetSlotCost(BtlActJChr *chr, s32 index);            /* u8 + 0x9C[index] * 100000: transformation blast cost */
extern s32 BtlParam_GetSlotA0(BtlActJChr *chr, s32 index);            /* u8 + 0xA0[index]: transformation sequence 0..4 */
extern s32 BtlParam_GetSlotA4(BtlActJChr *chr, s32 index);            /* u8 + 0xA4[index]: transformation kind (3 needs the stage flag) */
extern s32 BtlParam_GetSlotA8(BtlActJChr *chr, s32 index);            /* u8 + 0xA8[index] */
extern s32 BtlParam_CountSlots(BtlActJChr *chr);                       /* number of transformation targets that are not 0xFF */
extern s32 BtlParam_TestUnkADBit(BtlActJChr *chr, s32 index);            /* u8 + 0xAD & (0x10 << index) */
extern s32 BtlParam_GetUnkB4(BtlActJChr *chr, s32 index);            /* u8 + 0xB4[index]: fusion result character, 0xFF = none */
extern s32 BtlParam_GetUnkB7(BtlActJChr *chr, s32 index);            /* u8 + 0xB7[index] */
extern s32 BtlParam_GetUnkBA(BtlActJChr *chr, s32 index, s32 n);     /* u8 + 0xBA[index][n]: fusion partner characters */
extern s32 BtlParam_GetCostAE(BtlActJChr *chr, s32 index);            /* u8 + 0xAE[index] * 100000: fusion blast cost */
extern s32 BtlParam_GetUnkB1(BtlActJChr *chr, s32 index);            /* u8 + 0xB1[index]: fusion sequence 1 / 2 */
extern s32 BtlParam_GetUnk8F(BtlActJChr *chr, s32 index);            /* u8 + 0x8F[index]: combo finisher mode 0..5 */
extern s32 BtlParam_CanFly(BtlActJChr *chr);                       /* parameter flag 0x1000 clear, or ability 0x36 */

/* Technique table (object + 0x92C) and skill table (object + 0x930) readers. */
extern s32 BtlSuper_GetFlags(BtlActJChr *chr, s32 slot);             /* technique flags */
extern s32 BtlSuper_GetType(BtlActJChr *chr, s32 slot);             /* technique kind (s8 + 0x13C[slot]) */
extern s32 BtlSuper_GetKiCost(BtlActJChr *chr, s32 slot);             /* technique ki cost (halved with ability 0x2B) */
extern s32 BtlSkill_GetFlags(BtlActJChr *chr, s32 slot);             /* skill flags */
extern s32 BtlSkill_GetId(BtlActJChr *chr, s32 slot);             /* skill id (s16) */
extern s32 BtlSkill_GetSequence(BtlActJChr *chr, s32 slot);             /* skill sequence 0..2 */
extern s32 BtlSkill_GetBlastCost(BtlActJChr *chr, s32 slot);             /* skill blast cost (one stock less with ability 0x15, minimum 1) */

/*
 * Powered-up mode (flag 6) only: input 80 (blast pressed) during a rush combo queues a technique as its finisher.
 * The last prepared attack id picks a column of the character's finisher table, whose value says which technique
 * (slot 2, slot 3, or the plain rush finisher 0x115) and whether a dash (0x2B, 0x2E) goes first.
 */
s32 BtlAct_QueueComboFinish(BtlActJChr *chr) {
    s32 n = 0;
    s32 col;
    s32 tech2;
    s32 tech3;

    if (!BtlChar_TestFlag(chr, 6)) {
        return 0;
    }
    if (!BtlInput_TestAction(chr, 0x50, 1)) {
        return 0;
    }
    switch (chr->attackId) {
        case 0xA6:
            col = 3;
            break;
        case 0xA7:
        case 0xA8:
            col = 6;
            break;
        case 0xA9:
            col = 5;
            break;
        case 0xAA:
            col = 4;
            break;
        case 0xAB:
            col = 7;
            break;
        case 0xAC:
            col = 8;
            break;
        case 0xAD:
            col = 2;
            break;
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
            col = 0;
            break;
        default:
            col = 1;
            break;
    }
    switch (BtlSuper_GetType(chr, 2)) {
        case 1:
            tech2 = 0x118;
            break;
        case 8:
            tech2 = 0x127;
            break;
        default:
            tech2 = 0x115;
            break;
    }
    switch (BtlSuper_GetType(chr, 3)) {
        case 1:
            tech3 = 0x119;
            break;
        case 8:
            tech3 = 0x128;
            break;
        default:
            tech3 = 0x116;
            break;
    }
    switch (BtlParam_GetUnk8F(chr, col)) {
        case 0:
            BtlAct_SetQueue(chr, n++, tech2);
            break;
        case 1:
            BtlAct_SetQueue(chr, n++, tech3);
            break;
        case 2:
            BtlAct_SetQueue(chr, n++, 0x115);
            break;
        case 3:
            BtlAct_SetQueue(chr, n++, 0x2B);
            BtlAct_SetQueue(chr, n++, 0x2E);
            BtlAct_SetQueue(chr, n++, tech2);
            break;
        case 4:
            BtlAct_SetQueue(chr, n++, 0x2B);
            BtlAct_SetQueue(chr, n++, 0x2E);
            BtlAct_SetQueue(chr, n++, tech3);
            break;
        case 5:
            BtlAct_SetQueue(chr, n++, 0x2B);
            BtlAct_SetQueue(chr, n++, 0x2E);
            BtlAct_SetQueue(chr, n++, 0x115);
            break;
    }
    BtlAct_ResetAttack(chr);
    return n;
}

/*
 * Can the fighter take transformation `index` (0..3)? Refused with ability 0x5E, flag 0xAF, the member's unk70
 * state, no target, a target the side may not use, kind 3 on a stage without flag 0x10, or (needBlast) not enough
 * blast gauge; needAllowed also requires form.allowed.
 */
s32 BtlAct_CanTransform(BtlActJChr *chr, u32 index, s32 needBlast, s32 needAllowed) {
    s32 chara;
    s32 cost;
    s32 kind;

    if (index >= 4) {
        return 0;
    }
    if (needAllowed && chr->form.allowed == 0) {
        return 0;
    }
    if (BtlMember_HasAbility(chr, 0x5E)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xAF)) {
        return 0;
    }
    if (BtlChar_TestMemberUnk70(chr)) {
        return 0;
    }
    chara = BtlParam_GetSlotId(chr, index);
    cost = BtlParam_GetSlotCost(chr, index);
    kind = BtlParam_GetSlotA4(chr, index);
    if (chara == 0xFF) {
        return 0;
    }
    if (!BattleSide_IsCharaUsable(chr->player, chara)) {
        return 0;
    }
    if (kind == 3 && !BtlStage_HasMoon()) {
        return 0;
    }
    if (needBlast &&
#ifdef PORT
        (PortGame_IsBT4() ? !BtlMember_HasKi(chr,cost) : !BtlMember_HasBlast(chr,cost))) {
#else
        !BtlMember_HasBlast(chr,cost)) {
#endif
        return 0;
    }
    return 1;
}

/*
 * Transformation inputs: 99 (R3 tapped, no direction) = the character's default index, 100..103 (R3 tapped with
 * left / up / right / down) = index 0..3. A character with a single transformation always uses its default.
 */
s32 BtlAct_CheckTransformInput(BtlActJChr *chr) {
    s32 index = -1;
    s32 def = BtlParam_GetUnkAC(chr);

    if (BtlInput_TestAction(chr, 0x63, 1)) {
        index = def;
    }
    if (BtlInput_TestAction(chr, 0x64, 1)) {
        index = 0;
    }
    if (BtlInput_TestAction(chr, 0x65, 1)) {
        index = 1;
    }
    if (BtlInput_TestAction(chr, 0x66, 1)) {
        index = 2;
    }
    if (BtlInput_TestAction(chr, 0x67, 1)) {
        index = 3;
    }
    if (index < 0) {
        return 0;
    }
    if (BtlParam_CountSlots(chr) == 1 && def >= 0) {
        index = def;
    }
    if (BtlAct_CanTransform(chr, index, 1, !BtlChar_TestFlag(chr, 0x10F))) {
        return BtlAct_QueueTransform(chr, index);
    }
    return 0;
}

/* Fills the form-change request for transformation `index` and queues its action (0xEC..0xF0 by sequence). */
s32 BtlAct_QueueTransform(BtlActJChr *chr, s32 index) {
    s32 chara = BtlParam_GetSlotId(chr, index);
    s32 cost = BtlParam_GetSlotCost(chr, index);
    s32 seq = BtlParam_GetSlotA0(chr, index);
    s32 kind = BtlParam_GetSlotA4(chr, index);
    s32 unk24 = BtlParam_GetSlotA8(chr, index);
    s32 action;

    chr->form.index = index;
    chr->form.chara = chara;
    chr->form.costume = BtlMember_GetActive(chr)->costume;
    chr->form.cost = cost;
    chr->form.partner = -1;
    chr->form.kind = kind;
    chr->form.unk24 = unk24;
    chr->form.unk28 = 0;
    chr->form.variant = BtlMember_GetActiveGauge(chr)->variant;
    chr->form.animChara = chara;
    chr->form.unk1C = chara;
    chr->form.voiceChara = chara;
    if (BtlMember_GetActive(chr)->chara == 0x6A) {
        chr->form.unk28 = 2;
    }
#ifdef PORT
    if (PortGame_IsBT4()) {
        extern s32 BtlParam_GetTransformPropCostume(BtlActJChr *,s32);
        chr->form.unk28 = BtlParam_GetTransformPropCostume(chr,index);
        if (chr->form.unk24 >= 0x100) chr->form.unk24 += chr->form.unk28;
    }
#endif
    if (BtlParam_TestUnkADBit(chr, index)) {
        chr->form.variant = 0;
    }
    switch (seq) {
        case 1:
            action = 0xED;
            break;
        case 2:
            action = 0xEE;
            break;
        case 3:
            action = 0xEF;
            break;
        case 4:
            action = 0xF0;
            break;
        case 0:
            action = 0xEC;
            break;
        default:
            action = 0xEC;
            break;
    }
    BtlAct_SetQueue(chr, 0, action);
    return 1;
}

/*
 * Can the fighter perform fusion `index` (0..2)? Same refusals as a transformation, plus: one of the four partner
 * characters must be a present member that is alive and not excluded (gauge unk30). The partner's member index
 * goes to *partner.
 */
s32 BtlAct_CanFuse(BtlActJChr *chr, s32 index, s32 needBlast, s32 needAllowed, s32 *partner) {
    s32 chara;
    s32 cost;
    s32 member;
    s32 i;
#ifdef PORT
    extern s32 TagTeam_FusionsEnabled(void);
    if (TagTeam_Active() && !TagTeam_FusionsEnabled()) return 0;
#endif

    if (needAllowed && chr->form.allowed == 0) {
        return 0;
    }
    if (BtlMember_HasAbility(chr, 0x5E)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xAF)) {
        return 0;
    }
    if (BtlChar_TestMemberUnk70(chr)) {
        return 0;
    }
    chara = BtlParam_GetUnkB4(chr, index);
    cost = BtlParam_GetCostAE(chr, index);
    if (chara == 0xFF) {
        return 0;
    }
    if (!BattleSide_IsCharaUsable(chr->player, chara)) {
        return 0;
    }
    if (needBlast && !BtlMember_HasBlast(chr, cost)) {
        return 0;
    }
#ifdef PORT
    if(TagTeam_Active()) {
        extern s32 TagFusion_Find(s32,s32);
        member=TagFusion_Find(chr->player,index);
        if(member<0) return 0;
        if(partner) *partner=member;
        return 1;
    }
#endif
    member = -1;
    i = 0;
    while (i < 4) {
        member = BtlMember_FindPresentIndex(chr, BtlParam_GetUnkBA(chr, index, i++));
        if (member >= 0) {
            break;
        }
    }
    if (member < 0) {
        return 0;
    }
    if (BtlMember_GetGauge(chr, member)->unk30 != 0) {
        return 0;
    }
    if (BtlMember_GetGauge(chr, member)->health <= 0) {
        return 0;
    }
    if (partner != NULL) {
        *partner = member;
    }
    return 1;
}

/* Fusion inputs: 104..106 (R3 held 12 frames with left / up / right) = fusion index 0..2. */
s32 BtlAct_CheckFusionInput(BtlActJChr *chr) {
    s32 partner;
    s32 index = -1;

    if (BtlInput_TestAction(chr, 0x68, 1)) {
        index = 0;
    }
    if (BtlInput_TestAction(chr, 0x69, 1)) {
        index = 1;
    }
    if (BtlInput_TestAction(chr, 0x6A, 1)) {
        index = 2;
    }
    if (index < 0) {
        return 0;
    }
    if (BtlAct_CanFuse(chr, index, 1, !BtlChar_TestFlag(chr, 0x10F), &partner)) {
        return BtlAct_QueueFusion(chr, index, partner);
    }
    return 0;
}

/*
 * Fills the form-change request for fusion `index` with member `partner` (searched for when negative) and queues
 * action 0xF1 or 0xF2 by the fusion's sequence.
 */
s32 BtlAct_QueueFusion(BtlActJChr *chr, s32 index, s32 partner) {
    s32 chara = BtlParam_GetUnkB4(chr, index);
    s32 cost = BtlParam_GetCostAE(chr, index);
    s32 seq = BtlParam_GetUnkB1(chr, index);
    s32 unk24 = BtlParam_GetUnkB7(chr, index);
    s32 i;
    s32 action;

#ifdef PORT
    if(TagTeam_Active()) {
        extern s32 TagFusion_Prepare(s32,s32,s32);
        if(seq!=1 && seq!=2) return 0;
        partner=TagFusion_Prepare(chr->player,index,partner);
        if(partner<0) return 0;
    }
#endif
    if (partner < 0) {
        i = 0;
        while (i < 4) {
            partner = BtlMember_FindPresentIndex(chr, BtlParam_GetUnkBA(chr, index, i++));
            if (partner >= 0) {
                break;
            }
        }
        if (partner < 0) {
            return 0;
        }
    }
    chr->form.index = index;
    chr->form.cost = cost;
    chr->form.kind = 5;
    chr->form.unk24 = unk24;
    chr->form.chara = chara;
    chr->form.costume = 0;
    chr->form.partner = partner;
    chr->form.unk28 = BtlMember_Get(chr, partner)->costume;
    chr->form.variant = BtlMember_GetActiveGauge(chr)->variant;
#ifdef PORT
    if (PortGame_IsBT4()) {
        chr->form.unk30 = BtlMember_GetGauge(chr,partner)->variant;
        chr->form.variant = 0;
    }
#endif
    chr->form.animChara = chara;
    chr->form.unk1C = chara;
    chr->form.voiceChara = chara;
    switch (seq) {
        case 1:
            action = 0xF1;
            break;
        case 2:
            action = 0xF2;
            break;
        default:
            return 0;
    }
    BtlAct_SetQueue(chr, 0, action);
    return 1;
}

/* Can the fighter switch member? Needs two living members, no flag 0xAF, and (needGauge) a full switch gauge. */
s32 BtlAct_CanSwitch(BtlActJChr *chr, s32 needGauge, s32 needAllowed) {
    if (needAllowed && chr->form.allowed == 0) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xAF)) {
        return 0;
    } else {
        if (needGauge && chr->switchGauge < 100000) {
            return 0;
        }
        if (BtlMember_CountAlive(chr) < 2) {
            return 0;
        }
        return 1;
    }
}

/* Member switch input: 107 (L3 + R3 pressed). */
s32 BtlAct_CheckSwitchInput(BtlActJChr *chr) {
    if (!BtlInput_TestAction(chr, 0x6B, 1)) {
        return 0;
    }
    if (!BtlAct_CanSwitch(chr, 1, !BtlChar_TestFlag(chr, 0x10F))) {
        return 0;
    }
    return BtlAct_QueueSwitch(chr);
}

/* Picks the member to switch to and queues the switch action 0xF3. */
s32 BtlAct_QueueSwitch(BtlActJChr *chr) {
    BtlAct_PrepareSwitch(chr);
    BtlAct_SetQueue(chr, 0, 0xF3);
    return 1;
}

/* Action id of technique slot 2..4 from the technique's kind: a block of three ids per kind. */
s32 BtlAct_GetTechniqueAction(BtlActJChr *chr, s32 slot) {
    s32 n = slot - 2;

    switch (BtlSuper_GetType(chr, slot)) {
        case 0:
        case 7:
            return n + 0x106;
        case 1:
            return n + 0x10C;
        case 2:
            return n + 0x10F;
        case 3:
            return n + 0x112;
        case 6:
            return n + 0x11E;
        case 4:
        case 5:
            return n + 0x11B;
        case 9:
            return n + 0x109;
        case 8:
            return n + 0x124;
        case 10:
            return n + 0x121;
        default:
            return slot + 0x104;
    }
}

/*
 * Can technique slot 2..4 be used? Needs lock-on (flag 5), the powered-up mode (flag 6) for slot 4, and the ki
 * cost. A technique with flag 0x2000 is refused while its lock counter is positive, except in modes 5 and 6 and
 * in story mode under flag 0x11F.
 */
s32 BtlAct_CanUseTechnique(BtlActJChr *chr, s32 slot) {
    s32 check;

    if ((u32)(slot - 2) >= 3) {
        return 0;
    }
    if (BtlChar_TestMemberUnk70(chr)
#ifdef PORT
        && (!PortGame_IsBT4() || slot >= 4)
#endif
        ) {
        return 0;
    }
    if (!BtlChar_TestFlag(chr, 5)) {
        return 0;
    }
    if (slot == 4 && !BtlChar_TestFlag(chr, 6)) {
        return 0;
    }
    if (!BtlMember_HasKi(chr, BtlSuper_GetKiCost(chr, slot))) {
        return 0;
    }
    if (BtlSuper_GetFlags(chr, slot) & 0x2000) {
        if (Battle_GetMode() == 5) {
            return 1;
        }
        if (Battle_GetMode() == 6) {
            return 1;
        }
        check = 1;
        if (Battle_GetMode() == 1) {
            check = !BtlChar_TestFlag(chr, 0x11F);
        }
        if (check) {
            if (BtlMember_GetActiveGauge(chr)->techLock[slot] > 0) {
                return 0;
            }
        }
    }
    return 1;
}

/*
 * Technique inputs: 108 / 109 / 110 (charge + blast, neutral / up / down, or flags 0x122..0x124) = slot 2 / 3 / 4.
 * Queues [0x1B first when flag 0x13 is set and the technique allows it], [0x105 for slot 4], the technique action.
 */
s32 BtlAct_CheckTechniqueInput(BtlActJChr *chr) {
    s32 slot = -1;
    s32 n = 0;
    s32 pre;
    s32 flags;
    s32 kind;

    if (chr->techDelay > 0) {
        return 0;
    }
    if (BtlInput_TestAction(chr, 0x6C, 1)) {
        slot = 2;
    }
    if (BtlInput_TestAction(chr, 0x6D, 1)) {
        slot = 3;
    }
    if (BtlInput_TestAction(chr, 0x6E, 1)) {
        slot = 4;
    }
    if (slot < 0) {
        return 0;
    }
    if (!BtlAct_CanUseTechnique(chr, slot)) {
        return 0;
    }
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0x13)) {
        flags = BtlSuper_GetFlags(chr, slot) & 0x200;
        kind = BtlSuper_GetType(chr, slot);
        pre = flags == 0;
        if (kind == 10) {
            pre = 0;
        }
        if (pre) {
            BtlAct_SetQueue(chr, 0, 0x1B);
            n = 1;
        }
    }
    if (slot == 4) {
        BtlAct_SetQueue(chr, n++, 0x105);
    }
    BtlAct_SetQueue(chr, n++, BtlAct_GetTechniqueAction(chr, slot));
    return n;
}

/* Action id of skill slot 0..1 from the skill's sequence: 0xFD.., 0xFF.. or 0x101... */
s32 BtlAct_GetSkillAction(BtlActJChr *chr, s32 slot) {
    switch (BtlSkill_GetSequence(chr, slot)) {
        case 1:
            return slot + 0xFF;
        case 0:
            return slot + 0xFD;
        case 2:
            return slot + 0x101;
        default:
            return slot + 0xFD;
    }
}

/*
 * Can skill slot 0..1 be used? Needs lock-on when the skill has flag 1, the blast cost, no lock counter when it
 * has flag 0x10, and room for what it gives: skills 0xC / 0x32 refill the evasions (not while any are left, or
 * with flag 0x100 while 3 are left), 0x37 the second kind, 0x41 the counter at + 0xE5C.
 */
s32 BtlAct_CanUseSkill(BtlActJChr *chr, u32 slot) {
    s32 flags;
    s32 stack;

    if (slot >= 2) {
        return 0;
    }
    flags = BtlSkill_GetFlags(chr, slot);
    stack = (flags >> 8) & 1;
    if (BtlChar_TestMemberUnk70(chr)
#ifdef PORT
        && !PortGame_IsBT4()
#endif
        ) {
        return 0;
    }
    if (!BtlChar_TestFlag(chr, 5) && (flags & 1)) {
        return 0;
    }
    if (!BtlMember_HasBlast(chr, BtlSkill_GetBlastCost(chr, slot))) {
        return 0;
    }
    if ((flags & 0x10) && BtlMember_GetActiveGauge(chr)->skillLock[slot] > 0) {
        return 0;
    }
    switch (BtlSkill_GetId(chr, slot)) {
        case 0xC:
        case 0x32:
            if (stack) {
                if (chr->dodges >= 3) {
                    return 0;
                }
            } else {
                if (chr->dodges > 0) {
                    return 0;
                }
            }
            break;
        case 0x37:
            if (stack) {
                if (chr->dodgesB >= 3) {
                    return 0;
                }
            } else {
                if (chr->dodgesB > 0) {
                    return 0;
                }
            }
            break;
        case 0x41:
            if (chr->unkE5C >= 3) {
                return 0;
            }
            break;
    }
    return 1;
}

/*
 * Skill inputs: 113 / 114 (charge + guard, neutral / up, or flags 0x120 / 0x121) = slot 0 / 1. During an
 * animation with flag 0x10 only skills with flag 8 are accepted.
 */
s32 BtlAct_CheckSkillInput(BtlActJChr *chr) {
    s32 slot = -1;

    if (BtlInput_TestAction(chr, 0x71, 1)) {
        slot = 0;
    }
    if (BtlInput_TestAction(chr, 0x72, 1)) {
        slot = 1;
    }
    if (slot < 0) {
        return 0;
    }
    if (!BtlAct_CanUseSkill(chr, slot)) {
        return 0;
    }
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if ((BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x10) && !(BtlSkill_GetFlags(chr, slot) & 8)) {
        return 0;
    }
    BtlAct_SetQueue(chr, 0, BtlAct_GetSkillAction(chr, slot));
    return 1;
}

/*
 * Queues the damage action for a pending hit reaction (HitReact.reaction, fighter + 0xFB0); called every frame by
 * BtlAct_CheckForced and by the handler of actions 0xB8 / 0xBA.
 *   - reactions 0 and 1: nothing (returns 0). Reaction 2 (no flinch): nothing unless the fighter is dead.
 *   - reactCount[reaction] is incremented (saturating at 100); 0x23 and 0x24 also increment each other's count.
 *   - reactions 5..13, 35, 36, 42, 45..47 taken for the second time or more (count >= 2) become reaction 0x10,
 *     with flag 0x94 and the fighter turned along the hit (hitYaw, or hitYaw + pi when not hit from behind).
 *   - on a dead fighter, reactions 2..9, 23..25, 42..48 become reaction 0xF the same way.
 *   - reactTimer[reaction] = 0, then reaction -> action:
 *        4 0xC1    5 0xC2    6 0xC3    7 0xC4    8 0xC5    9 0xC6   10 0xCE   11 0xCD   12 0xCB   13 0xCC
 *       14 0xDA   15 0xCF   16 0xD0   17 0xD5   18 0xD6   19 0xD4   20 0xB8   22 0xBA
 *       23 / 24 / 25  0xD3, and stunTime = 15 / 30 / 45 frames unless it is already running
 *       26 / 27 / 28  0xE0, and stunTime = 15 / 30 / 45 the same way
 *       29, 30  0x131 + unkE9C      31 0xDB   32 0xDC   33 0xDD   34 0x137 + unkE9C
 *       35 0xC7   36 0xC8   37 0xBC   38 0xD1   39 0xCB   40 0xDF   41 0x134 + unkFF4
 *       42 0xC6   43 0xC4   44 0xC5   45 0xC6   46 0xC4   47 0xC6   48 0xC6   49 0xBF
 *       2, 3, 21 and anything else  0xC0
 *   - event 0x55 is raised for the opponent's player when the current animation has any flag; flag 0x129 is set.
 */
s32 BtlAct_QueueReaction(BtlActJChr *chr, u32 reaction) {
    s32 action;

    if (reaction < 2) {
        return 0;
    }
    if (reaction == 2 && !BtlChar_IsDead(chr)) {
        return 0;
    }
    if (chr->reactCount[reaction] < 100) {
        chr->reactCount[reaction]++;
    }
    switch (reaction) {
        case 0x23:
            chr->reactCount[0x24]++;
            break;
        case 0x24:
            chr->reactCount[0x23]++;
            break;
    }
    switch (reaction) {
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 35:
        case 36:
        case 42:
        case 45:
        case 46:
        case 47:
            if (chr->reactCount[reaction] >= 2) {
                BtlChar_SetFlag(chr, 0x94);
                reaction = 0x10;
                if (chr->hitBack != 0) {
                    chr->turnYaw = chr->hitYaw;
                    chr->turnPitch = 0;
                } else {
                    chr->turnYaw = BtlUtil_WrapAngle(chr->hitYaw + 3.14159265f);
                    chr->turnPitch = 0;
                }
            }
            break;
    }
    if (BtlChar_IsDead(chr)) {
        switch (reaction) {
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 23:
            case 24:
            case 25:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
            case 48:
                BtlChar_SetFlag(chr, 0x94);
                reaction = 0xF;
                if (chr->hitBack != 0) {
                    chr->turnYaw = chr->hitYaw;
                    chr->turnPitch = 0;
                } else {
                    chr->turnYaw = BtlUtil_WrapAngle(chr->hitYaw + 3.14159265f);
                    chr->turnPitch = 0;
                }
                break;
        }
    }
    chr->reactTimer[reaction] = 0;
    switch (reaction) {
        case 4:
            action = 0xC1;
            break;
        case 5:
            action = 0xC2;
            break;
        case 6:
            action = 0xC3;
            break;
        case 7:
        case 43:
        case 46:
            action = 0xC4;
            break;
        case 8:
        case 44:
            action = 0xC5;
            break;
        case 9:
        case 42:
        case 45:
        case 47:
        case 48:
            action = 0xC6;
            break;
        case 10:
            action = 0xCE;
            break;
        case 11:
            action = 0xCD;
            break;
        case 14:
            action = 0xDA;
            break;
        case 15:
            action = 0xCF;
            break;
        case 16:
            action = 0xD0;
            break;
        case 38:
            action = 0xD1;
            break;
        case 17:
            action = 0xD5;
            break;
        case 18:
            action = 0xD6;
            break;
        case 19:
            action = 0xD4;
            break;
        case 20:
            action = 0xB8;
            break;
        case 37:
            action = 0xBC;
            break;
        case 22:
            action = 0xBA;
            break;
        case 49:
            action = 0xBF;
            break;
        case 12:
        case 39:
            action = 0xCB;
            break;
        case 13:
            action = 0xCC;
            break;
        case 31:
            action = 0xDB;
            break;
        case 32:
            action = 0xDC;
            break;
        case 33:
            action = 0xDD;
            break;
        case 35:
            action = 0xC7;
            break;
        case 36:
            action = 0xC8;
            break;
        case 40:
            action = 0xDF;
            break;
        case 23:
            action = 0xD3;
            if (chr->stunTime <= 0) {
                chr->stunTime = 15;
            }
            break;
        case 24:
            action = 0xD3;
            if (chr->stunTime <= 0) {
                chr->stunTime = 30;
            }
            break;
        case 25:
            action = 0xD3;
            if (chr->stunTime <= 0) {
                chr->stunTime = 45;
            }
            break;
        case 26:
            action = 0xE0;
            if (chr->stunTime <= 0) {
                chr->stunTime = 15;
            }
            break;
        case 27:
            action = 0xE0;
            if (chr->stunTime <= 0) {
                chr->stunTime = 30;
            }
            break;
        case 28:
            action = 0xE0;
            if (chr->stunTime <= 0) {
                chr->stunTime = 45;
            }
            break;
        case 34:
            action = chr->unkE9C + 0x137;
            break;
        case 29:
        case 30:
            action = chr->unkE9C + 0x131;
            break;
        case 41:
            action = chr->unkFF4 + 0x134;
            break;
        case 3:
        default:
            action = 0xC0;
            break;
    }
    if (BtlAnim_GetFlags(BtlAnim_GetId(chr))) {
        BtlEvent_Raise(BtlOpp_GetPlayer(chr), 0x55);
    }
    BtlChar_SetFlag(chr, 0x129);
    BtlAct_SetQueue(chr, 0, action);
    return 1;
}

/* Action that follows actions 0x12D..0x12F, 0x133..0x135 and 0x139..0x13B, by `kind` 0..7 (5 and others: 0xCF). */
s32 BtlAct_GetLandingAction(BtlActJChr *chr, s32 arg1, u32 kind) {
    s32 action = 0xCF;

    switch (kind) {
        case 0:
            action = 0xD8;
            break;
        case 1:
            action = 0xD9;
            break;
        case 2:
            action = 0xCF;
            break;
        case 3:
            action = 0xD0;
            break;
        case 4:
            action = 0xD5;
            break;
        case 6:
            action = 0xD4;
            break;
        case 7:
            action = 0xDB;
            break;
    }
    return action;
}

/* Flag 0x21, or flag 0x22: queues 0x25 / 0x27 (by BtlParam_CanFly), or 0x28 for flag 0x22 under lock-on. */
s32 BtlAct_QueueFlag21Action(BtlActJChr *chr) {
    if (BtlChar_TestFlag(chr, 0x21)) {
        if (!BtlParam_CanFly(chr)) {
            BtlAct_SetQueue(chr, 0, 0x25);
            return 1;
        } else {
            BtlAct_SetQueue(chr, 0, 0x27);
            return 1;
        }
    }
    if (BtlChar_TestFlag(chr, 0x22)) {
        if (!BtlChar_TestFlag(chr, 5)) {
            if (!BtlParam_CanFly(chr)) {
                BtlAct_SetQueue(chr, 0, 0x25);
                return 1;
            } else {
                BtlAct_SetQueue(chr, 0, 0x27);
                return 1;
            }
        } else {
            BtlAct_SetQueue(chr, 0, 0x28);
            return 1;
        }
    }
    return 0;
}

/* Flag 0x21: queues 0x29. Flag 0x22: queues 0x2A under lock-on, else 0x29. */
s32 BtlAct_QueueFlag21ActionB(BtlActJChr *chr) {
    if (BtlChar_TestFlag(chr, 0x21)) {
        BtlAct_SetQueue(chr, 0, 0x29);
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x22)) {
        if (!BtlChar_TestFlag(chr, 5)) {
            BtlAct_SetQueue(chr, 0, 0x29);
            return 1;
        } else {
            BtlAct_SetQueue(chr, 0, 0x2A);
            return 1;
        }
    }
    return 0;
}

/*
 * Recovery input of a fighter that is down or tumbling (actions 0xD8, 0xDA..0xDE). With flag 0x11: any direction
 * or any face button queues 0xE6. Otherwise a direction queues 0xE2 / 0xE3 (left / right) or 0xE5 / 0xE4 (up /
 * down); each pair is swapped when the model does not face the same way as the fighter camera (model yaw 90
 * degrees or more off the camera yaw). A face button alone queues 0xE1. `held` tests held directions instead of
 * pressed ones; a face button press always makes the directions "held".
 */
s32 BtlAct_CheckRecoveryInput(BtlActJChr *chr, s32 held) {
    BtlActJPose *pose = BtlChar_GetPos(chr);
    s32 (*test)(BtlActJChr *chr, u32 mask);
    s32 button;
    s32 front;

    if (held) {
        test = BtlInput_IsHeld;
    } else {
        test = BtlInput_IsPressed;
    }
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0x11)) {
        if (test(chr, 0xF0)) {
            BtlAct_SetQueue(chr, 0, 0xE6);
            return 1;
        }
        if (test(chr, 0x100000)) {
            BtlAct_SetQueue(chr, 0, 0xE6);
            return 1;
        }
    } else {
        button = BtlInput_IsPressed(chr, 0x100000);
        front = 0;
        if (button) {
            test = BtlInput_IsHeld;
        }
        if (__builtin_fabsf(BtlUtil_WrapAngle(pose->rot.y - chr->camYaw)) < 1.5707963f) {
            front = 1;
        }
        if (test(chr, 0x40)) {
            BtlAct_SetQueue(chr, 0, front ? 0xE2 : 0xE3);
            return 1;
        }
        if (test(chr, 0x80)) {
            BtlAct_SetQueue(chr, 0, front ? 0xE3 : 0xE2);
            return 1;
        }
        if (test(chr, 0x10)) {
            BtlAct_SetQueue(chr, 0, front ? 0xE5 : 0xE4);
            return 1;
        }
        if (test(chr, 0x20)) {
            BtlAct_SetQueue(chr, 0, front ? 0xE4 : 0xE5);
            return 1;
        }
        if (button) {
            BtlAct_SetQueue(chr, 0, 0xE1);
            return 1;
        }
    }
    return 0;
}

/*
 * Story mode (mode 1) only: actions forced by the battle script through fighter flags. Flag 0xFA queues action 4.
 * Outside a damage action: 0x116..0x119 transformation 0..3, 0x11A..0x11C fusion 0..2, 0x11D member switch (no
 * cost or availability check). Always: 0x110..0x115 queue actions 5..10. The flag is consumed (except 0xFA).
 */
s32 BtlAct_CheckStoryForced(BtlActJChr *chr) {
    if (Battle_GetMode() != 1) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xFA)) {
        BtlAct_SetQueue(chr, 0, 4);
        return 1;
    }
    if (!BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr))) {
        if (BtlChar_TestFlag(chr, 0x116)) {
            BtlChar_ClearFlag(chr, 0x116);
            BtlAct_QueueTransform(chr, 0);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0x117)) {
            BtlChar_ClearFlag(chr, 0x117);
            BtlAct_QueueTransform(chr, 1);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0x118)) {
            BtlChar_ClearFlag(chr, 0x118);
            BtlAct_QueueTransform(chr, 2);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0x119)) {
            BtlChar_ClearFlag(chr, 0x119);
            BtlAct_QueueTransform(chr, 3);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0x11A)) {
            BtlChar_ClearFlag(chr, 0x11A);
            BtlAct_QueueFusion(chr, 0, -1);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0x11B)) {
            BtlChar_ClearFlag(chr, 0x11B);
            BtlAct_QueueFusion(chr, 1, -1);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0x11C)) {
            BtlChar_ClearFlag(chr, 0x11C);
            BtlAct_QueueFusion(chr, 2, -1);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 0x11D)) {
            BtlChar_ClearFlag(chr, 0x11D);
            BtlAct_QueueSwitch(chr);
            return 1;
        }
    }
    if (BtlChar_TestFlag(chr, 0x110)) {
        BtlChar_ClearFlag(chr, 0x110);
        BtlAct_SetQueue(chr, 0, 5);
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x111)) {
        BtlChar_ClearFlag(chr, 0x111);
        BtlAct_SetQueue(chr, 0, 6);
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x112)) {
        BtlChar_ClearFlag(chr, 0x112);
        BtlAct_SetQueue(chr, 0, 7);
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x113)) {
        BtlChar_ClearFlag(chr, 0x113);
        BtlAct_SetQueue(chr, 0, 8);
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x114)) {
        BtlChar_ClearFlag(chr, 0x114);
        BtlAct_SetQueue(chr, 0, 9);
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x115)) {
        BtlChar_ClearFlag(chr, 0x115);
        BtlAct_SetQueue(chr, 0, 10);
        return 1;
    }
    return 0;
}

/* Action a fighter on the ground goes to: 0xE0 while the stun countdown runs, else 0xD8. */
s32 BtlAct_GetDownAction(BtlActJChr *chr) {
    return chr->stunTime > 0 ? 0xE0 : 0xD8;
}

/*
 * Attack id of a paid evasion (flag 0x7C) and of action 0x39: (own + opponent's counter at + 0xD88) % 3 picks
 * 0x89, 0x8A, or by height 0x8B (low) / 0x8C (above half of the height range).
 */
s32 BtlAct_GetEvasionAttack(BtlActJChr *chr) {
    s32 n = chr->unkD88;

    n += BtlOpp_GetUnkD88(chr);
    switch (n % 3) {
        case 1:
            return 0x89;
        case 0:
            return BtlAct_GetHeightRatio(chr) > 0.5f ? 0x8C : 0x8B;
        case 2:
            return 0x8A;
    }
    return 0x8C;
}

/*
 * Follow-up of actions 0x67..0x69 by `kind`: 0 = 0x6A, 1..4 = 0x9E..0xA1 (attack ids: the queue is built with
 * BtlDecide_QueueAttack and its first action returned), anything else 0x6A.
 */
s32 BtlAct_GetChainAction(BtlActJChr *chr, u32 kind) {
    s32 id;

    switch (kind) {
        case 0:
            id = 0x6A;
            break;
        case 1:
            id = 0x9E;
            break;
        case 2:
            id = 0x9F;
            break;
        case 3:
            id = 0xA0;
            break;
        case 4:
            id = 0xA1;
            break;
        case 5:
        default:
            return 0x6A;
    }
    if (!BtlAct_IsAttackId(id)) {
        return id;
    }
    if (BtlDecide_QueueAttack(chr, id)) {
        return BtlAct_GetQueued(chr);
    }
    return 0x6A;
}

/*
 * What a fighter does when a damage action ends: idle (0xB), unless an input (BtlDecide_Main with mask 0x80000)
 * or a status override (BtlDecide_Common with mask 0xF) queues something else.
 */
s32 BtlAct_GetIdleFollowUp(BtlActJChr *chr) {
    BtlAct_SetQueue(chr, 0, 0xB);
    BtlDecide_Main(chr, 0x80000);
    BtlDecide_Common(chr, 0xF);
    return BtlAct_GetQueued(chr);
}

/* 0x14 when BtlParam_CanFly holds for the character, else 0x16. */
s32 BtlAct_GetAction14or16(BtlActJChr *chr) {
    return !BtlParam_CanFly(chr) ? 0x16 : 0x14;
}

/* 0x15 when BtlParam_CanFly holds or flag 0x11 is set, else 0x11. */
s32 BtlAct_GetAction11or15(BtlActJChr *chr) {
    if (!BtlParam_CanFly(chr)) {
        if (!BtlChar_TestFlag(chr, 0x11)) {
            return 0x11;
        }
    }
    return 0x15;
}
