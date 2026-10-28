#include "battle/tag_balance.h"
#include "battle/tag_clash.h"
#include "common.h"
#include "battle/btl_char_hit.h"

/* Fighter hit resolution, 0x1C7B30-0x1CA6D0. The frame flow and the structures are in battle/btl_char_hit.h.
 *
 * Reading guide (everything below is what the matching C does; what the numbers mean in game terms is inferred):
 *
 * Attack record (0x30 bytes, fighter object + 0x920, indexed by the attack id BtlAtk_GetId; accessors at
 * 0x20D210..0x20DF60 are not decompiled, the offsets were read from their disassembly):
 *   +0x00 u32 flags (BtlAtk_GetFlags)      +0x04 u16 damage (BtlAtk_GetDamage)   +0x06 u16 guard damage (BtlAtk_GetGuardDamage)
 *   +0x08 u16 ki the defender pays on guard result 1 (BtlAtk_GetGuardKiCost)        +0x0A u16 ki gain (BtlAtk_GetKiGain)
 *   +0x10 u16 gain of fighter + 0xD80 (BtlAtk_GetUnk10)
 *   +0x12 u8 push speed on hit (BtlAtk_GetPushOnHit)    +0x13 u8 push speed on guard (BtlAtk_GetPushOnGuard)
 *   +0x15, +0x16 s8 launch angles (BtlAtk_GetLaunchAngleA, BtlAtk_GetLaunchAngleB)
 *   +0x17, +0x18 u8 shake power / time on hit (BtlAtk_GetShakePower, BtlAtk_GetShakeTime)
 *   +0x19, +0x1A u8 shake power / time on guard (BtlAtk_GetShakePowerB, BtlAtk_GetShakeTimeB)
 *   +0x1B..+0x1F, +0x21, +0x22 s8 reaction ids by defender state (BtlAtk_GetReaction..BtlAtk_GetReactionG)
 *   +0x20 s8 -> HitReact.unk4 (BtlAtk_GetReactionSub)     +0x23 s8 -> effect bit 0x1D on the defender (BtlAtk_GetHitFxKind)
 *   +0x26 s8 priority for trades (BtlAtk_GetPriority)
 *   +0x27..+0x2A s8 guard results (BtlAtk_GetGuardKindB, BtlAtk_GetGuardKindC, BtlAtk_GetGuardKindA, BtlAtk_GetGuardKindE; BtlAtk_GetBestGuardKind mixes)
 *   +0x2B s8 0 / 1 -> defender flag 0x66 / 0x67 on guard (BtlAtk_GetGuardFlagSel)
 *   +0x2C s8 armour levels the attack ignores (BtlAtk_GetUnk2C)
 *
 * Attack flags tested here (BtlAtk_GetFlags): 1 first hit can be evaded by level, 2 ignored by animation flag 0x200,
 * 4 can be absorbed by armour, 8 hits without facing the opponent, 0x10 can be timed-guarded, 0x20 BtlMove_AimVerticalAtOpponent,
 * 0x40 / 0x400000 defender held flags 0xE / 0x1E, 0x100 turns the defender, 0x200 attacker held flag 0x86,
 * 0x400 no hurt voice, 0x800 shakes the attacker's camera too, 0x1000 launch "back" value, 0x4000 throw that
 * parameter bit 0x4000 refuses, 0x10000..0x40000 effect bits 0x25..0x27, 0x80000 no effect bit 0x1B,
 * 0x100000 the level evasion costs 20000 ki, 0x200000 turn uses the attacker's pitch, 0x800000 two-way side step,
 * 0x1000000 defender flag 0x78, 0x2000000 no defender scale, 0x4000000 needs one more level to evade,
 * 0x8000000 no back-hit bonus, 0x10000000 push direction is towards the opponent instead of the facing.
 *
 * Random draws: only BtlChar_FrameMod (frame counter % n), four sites: the side-step direction in
 * BtlHit_CheckDodge (% 2 or % 3) and in BtlHit_CheckRush (% 2, twice). No BtlChar_Rand, rand() or Rand_* call.
 *
 * Health is changed in two places only, both through BtlMember_Damage (0x1CE630): guard damage in BtlHit_TestHit
 * (flags 0xC) and hit damage in BtlHit_ApplyHit.
 */

/* Fighter accessors and flags. */
extern s32 BtlChar_GetCount(void);
extern HitChr *BtlChar_Get(s32 i);
extern HitPose *BtlChar_GetPos(HitChr *chr);
extern HitObj *BtlChar_GetObj(HitChr *chr);
extern s32 BtlChar_IsFree(HitChr *chr);
extern s32 BtlChar_TestMemberUnk70(HitChr *chr);                 /* active member's gauge + 0x30 != 0 */
extern s32 BtlChar_TestFlag(HitChr *chr, s32 flag);
extern void BtlChar_SetFlag(HitChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(HitChr *chr, s32 flag);
extern void BtlChar_ClearFlag(HitChr *chr, s32 flag);
extern void BtlChar_SetFxBit(HitChr *chr, s32 bit);              /* one-frame effect / sound request bit */
extern s32 BtlChar_FrameMod(s32 n);                              /* frame counter % n */
extern void BtlChar_PlayVoice(HitChr *chr, s32 kind);
extern void BtlChar_Vibrate(HitChr *chr, f32 power, f32 seconds);
extern void BtlChar_AddStageTimer(f32 seconds);
extern void BtlChar_RaiseFirstClash(HitChr *chr);
extern void BtlCharSnd_PlayCommon(HitChr *chr, s32 id);
extern void ChrCam_AddShake(HitChr *chr, f32 power, f32 seconds);
extern s32 BtlInput_IsHeld(HitChr *chr, u32 mask);
extern s32 BtlInput_IsCmdHeld(HitChr *chr, u32 mask);
extern s32 BtlOpp_GetPlayer(HitChr *chr);                        /* the opponent's player index */
extern f32 BtlOpp_GetYaw(HitChr *chr);                           /* yaw of the direction to the opponent */
extern f32 BtlOpp_GetYawFromFacing(HitChr *chr);                 /* the same relative to the own facing */
extern s32 BtlMove_IsBlockedByOpponent(HitChr *chr);                           /* flags 0x5E / 0x5F and a height test: the bodies touch */
extern void BtlMove_SetImpulseYaw(HitChr *chr, f32 yaw, f32 speed);      /* push: velocity (sin yaw, 0, cos yaw) * speed */
extern void BtlMove_AimVerticalAtOpponent(HitChr *chr, s32 arg);

/* Animation state, action, team member. */
extern s32 BtlAnim_GetId(HitChr *chr);                           /* fighter + 0x974 */
extern u32 BtlAnim_GetFlags(s32 id);                             /* roster table 0, 0 when out of range */
extern s32 BtlAct_GetCurrent(HitChr *chr);
extern s32 BtlAct_GetCurrentClass(HitChr *chr);
extern s32 BtlAct_TestPoweredSkill(HitChr *chr, s32 mask);       /* flag 6 and parameter word & mask */
extern s32 BtlColl_GetGuardKind(HitChr *chr);                   /* animation ids 0xEC..0xF7 -> 1..5: used here as the guard kind */
extern s32 BtlColl_IsGuarding(HitChr *chr);                      /* that kind is 1..4 */
extern s32 BtlColl_StartThrow(HitChr *a, HitChr *b, s32 tech, u32 flags);
extern HitMember *BtlMember_GetActive(HitChr *chr);
extern HitGauge *BtlMember_GetActiveGauge(HitChr *chr);
extern void BtlMember_Damage(HitChr *chr, s32 amount, s32 flags);
extern void BtlMember_AddKi(HitChr *chr, s32 amount);
extern s32 BtlMember_HasKi(HitChr *chr, s32 amount);
extern s32 BtlMember_SpendKi(HitChr *chr, s32 amount, s32 force);
extern s32 BtlMember_IsKiEmpty(HitChr *chr);
extern void BtlMember_AddBlast(HitChr *chr, s32 amount);
extern s32 BtlMember_HasAbility(HitChr *chr, s32 ability);

/* The attacker's current attack and its record (see the table above). */
extern s32 BtlAtk_GetId(HitChr *chr);
extern s32 BtlAtk_GetFlags(HitChr *chr);
extern s32 BtlAtk_GetPriority(HitChr *chr);
extern s32 BtlAtk_GetDamage(HitChr *chr);
extern s32 BtlAtk_GetGuardDamage(HitChr *chr);
extern s32 BtlAtk_GetKiGain(HitChr *chr);
extern s32 BtlAtk_GetGuardKiCost(HitChr *chr);
extern s32 BtlAtk_GetUnk10(HitChr *chr);
extern s32 BtlAtk_GetReaction(HitChr *chr);
extern s32 BtlAtk_GetReactionB(HitChr *chr);
extern s32 BtlAtk_GetReactionC(HitChr *chr);
extern s32 BtlAtk_GetReactionD(HitChr *chr);
extern s32 BtlAtk_GetReactionE(HitChr *chr);
extern s32 BtlAtk_GetReactionSub(HitChr *chr);
extern s32 BtlAtk_GetReactionF(HitChr *chr);
extern s32 BtlAtk_GetReactionG(HitChr *chr);
extern f32 BtlAtk_GetPushOnHit(HitChr *chr);
extern f32 BtlAtk_GetPushOnGuard(HitChr *chr);
extern f32 BtlAtk_GetLaunchAngleA(HitChr *chr);
extern f32 BtlAtk_GetLaunchAngleB(HitChr *chr);
extern f32 BtlAtk_GetShakePower(HitChr *chr);
extern f32 BtlAtk_GetShakeTime(HitChr *chr);
extern f32 BtlAtk_GetShakePowerB(HitChr *chr);
extern f32 BtlAtk_GetShakeTimeB(HitChr *chr);
extern s32 BtlAtk_GetHitFxKind(HitChr *chr);
extern s32 BtlAtk_GetGuardKindA(HitChr *chr);
extern s32 BtlAtk_GetGuardKindB(HitChr *chr);
extern s32 BtlAtk_GetGuardKindC(HitChr *chr);
extern s32 BtlAtk_GetBestGuardKind(HitChr *chr);
extern s32 BtlAtk_GetGuardKindE(HitChr *chr);
extern s32 BtlAtk_GetGuardFlagSel(HitChr *chr);
extern s32 BtlAtk_GetUnk2C(HitChr *chr);

/* The fighter's parameter block (object + 0x91C). */
extern s32 BtlParam_GetFlags(HitChr *chr);                           /* flags, + 0x10 */
extern u32 BtlParam_GetFlags2(HitChr *chr);                           /* flags, + 0x14 */
extern s32 BtlParam_GetUnk2(HitChr *chr);                           /* byte + 2 */
extern f32 BtlParam_GetHitReactScale(HitChr *chr);                           /* float + 0x60 */
extern u32 BtlSuper_GetFlags(HitChr *chr, s32 tech);                 /* technique flags */
extern s32 BtlSkill_GetFrames(HitChr *chr, s32 dir);

/* Model objects and maths. */
extern f32 BtlCharApi_GetHeight(s32 objId);                             /* body size, obj + 0xFF4 */
extern f32 BtlCharApi_GetRadius(s32 objId);                             /* body radius */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);       /* world position of a model node */
extern s32 BtlObjAnim_QueryEvent(HitObj *obj, u64 a, s32 b, s32 c);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_LengthXZ(Vec4 *v);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);

/* 1 when the attacker's facing yaw is within 90 degrees of the defender's model yaw (rot.y + unk64): both look the
 * same way, so an attacker that is facing the defender stands behind it. */
s32 BtlHit_IsFromBehind(HitChr *atk, HitChr *def) {
    HitPose *pd = BtlChar_GetPos(def);
    f32 ang = BtlUtil_WrapAngle(pd->rot.y + BtlChar_GetPos(def)->unk64);

    ang = BtlUtil_WrapAngle(ang - BtlChar_GetPos(atk)->yaw);
    if (__builtin_fabsf(ang) < 1.5707963f) {
        return 1;
    }
    return 0;
}

/* Voice kind 0..4 (BtlChar_PlayVoice) for the reaction id 1..49 a hit gives the defender; 0 for anything else. */
s32 BtlHit_GetVoiceKind(s32 reaction) {
    switch (reaction) {
    case 1: case 2: case 3: case 4: case 11: case 14: case 20: case 21: case 22:
    case 31: case 32: case 33: case 34: case 37: case 43: case 44: case 45: case 46: case 49:
        return 0;
    case 5: case 6: case 7: case 8: case 9: case 10: case 29: case 30: case 35: case 36:
    case 42: case 47: case 48:
        return 1;
    case 15: case 16: case 17: case 19:
        return 2;
    case 18: case 40: case 41:
        return 3;
    case 12: case 13: case 23: case 24: case 25: case 26: case 27: case 28: case 38: case 39:
        return 4;
    }
    return 0;
}

/* Number of the current hit of the attack: the model's hit counter (obj + 0xCAD) while the model has active hit
 * volumes (work + 0x18020 & 0xF000000), else the count of frames flag 0x65 ("contact by proximity") has been up,
 * else -1. */
s32 BtlHit_GetHitNo(HitChr *chr) {
    HitObj *obj = BtlChar_GetObj(chr);

    if (TagContact_Has(chr->player,-1)) {
        return obj->hitNo;
    }
    if (BtlChar_TestFlag(chr, 0x65)) {
        return chr->status.contact;
    }
    return -1;
}

/* The effects of a hit that was guarded or deflected. result: 0 plain guard, 1 flag-0x40 guard, 2..4 the three timed
 * guards (flags 0x68..0x6A), 5 guard that costs the defender ki, 6 guard that pushes the attacker back and gives the
 * defender half a blast stock, 7 / 8 the same with hit-stop and a voice / flag 0x7B. No health is touched here. */
void BtlHit_ApplyGuard(HitChr *atk, HitChr *def, u32 result) {
    f32 scale;
    f32 power;
    f32 time;
    HitPose *pose;

    if (BtlAtk_GetFlags(atk) & 0x10000) {
        BtlChar_SetFxBit(atk, 0x25);
    }
    if (BtlAtk_GetFlags(atk) & 0x20000) {
        BtlChar_SetFxBit(atk, 0x26);
    }
    if (BtlAtk_GetFlags(atk) & 0x40000) {
        BtlChar_SetFxBit(atk, 0x27);
    }
    BtlChar_ClearFlag(atk, 0x8A);
    BtlChar_SetHeldFlag(atk, 0x5D);
    switch (result) {
    case 1:
        BtlChar_SetFxBit(atk, 0x22);
        break;
    case 0:
    case 2:
    case 3:
    case 4:
        BtlChar_SetFxBit(atk, 0x1C);
        break;
    case 5:
        BtlChar_SetFlag(atk, 0x6C);
        BtlChar_SetHeldFlag(atk, 0x86);
        atk->unkD80 += 50000;
        if (atk->unkD80 > 100000) {
            atk->unkD80 = 100000;
        }
        break;
    case 6:
        BtlChar_SetFxBit(atk, 0x21);
        BtlChar_SetFlag(atk, 0x6F);
        break;
    case 7:
        BtlChar_SetFxBit(atk, 0x24);
        BtlChar_SetFlag(atk, 0x6F);
        atk->stopLen = 2;
        atk->stopDelay = 1;
        break;
    case 8:
        BtlChar_SetFxBit(atk, 0x21);
        BtlChar_SetFlag(atk, 0x6F);
        atk->stopLen = 2;
        atk->stopDelay = 1;
        break;
    }
    scale = 1.0f;
    switch (result) {
    case 0:
    case 1:
        break;
    case 2:
        BtlChar_SetFlag(def, 0x68);
        break;
    case 3:
        BtlChar_SetFlag(def, 0x69);
        break;
    case 4:
        BtlChar_SetFlag(def, 0x6A);
        break;
    case 5:
        BtlChar_SetFxBit(def, 0x20);
        BtlChar_SetFlag(def, 0x6D);
        BtlMember_SpendKi(def, BtlAtk_GetGuardKiCost(atk), 0);
        if (BtlMember_IsKiEmpty(def)) {
            BtlChar_SetHeldFlag(def, 0xBE);
        }
        scale = 0.5f;
        break;
    case 6:
        BtlChar_SetFlag(def, 0x6E);
        BtlMember_AddBlast(def, 50000);
        scale = 0.5f;
        break;
    case 7:
        BtlChar_SetFlag(def, 0x70);
        BtlChar_PlayVoice(def, 0x12);
        def->stopLen = 3;
        def->stopDelay = 0;
        break;
    case 8:
        BtlChar_SetFlag(def, 0x7B);
        def->stopLen = 3;
        def->stopDelay = 0;
        break;
    }
    pose = BtlChar_GetPos(atk);
    BtlMove_SetImpulseYaw(def, pose->yaw, BtlAtk_GetPushOnGuard(atk) * scale);
    switch (BtlAtk_GetGuardFlagSel(atk)) {
    case 0:
        BtlChar_SetFlag(def, 0x66);
        break;
    case 1:
        BtlChar_SetFlag(def, 0x67);
        break;
    }
    power = BtlAtk_GetShakePowerB(atk);
    time = BtlAtk_GetShakeTimeB(atk);
    if (BtlAtk_GetFlags(atk) & 0x800) {
        ChrCam_AddShake(atk, power, time);
    }
    ChrCam_AddShake(def, power, time);
    BtlChar_Vibrate(atk, power * 0.5f, time * 0.5f);
    BtlChar_Vibrate(def, power * 0.5f, time * 0.5f);
}

/* Attacker flags 0x4A / 0x4B: raise "contact" (0x65) when the opponent's node 3 is within 5 (0x4A) or 2 (0x4B)
 * body sizes, clamped to 20..150. Always returns 0. */
s32 BtlHit_CheckNear(HitChr *a, HitChr *b) {
    Vec4 pa;
    Vec4 pb;
    Vec4 d;
    f32 range;

    if (BtlChar_TestFlag(b, 0x42)) {
        return 0;
    }
    if (BtlChar_TestFlag(a, 0x4A)) {
        range = BtlCharApi_GetHeight(a->objId) * 5.0f;
    } else if (BtlChar_TestFlag(a, 0x4B)) {
        range = BtlCharApi_GetHeight(a->objId) * 2.0f;
    } else {
        return 0;
    }
    if (range < 20.0f) {
        range = 20.0f;
    }
    if (150.0f < range) {
        range = 150.0f;
    }
    BtlCharApi_GetNodePos(a->objId, 3, &pa);
    BtlCharApi_GetNodePos(b->objId, 3, &pb);
    Vec4_Sub(&d, &pa, &pb);
    if (Vec3_Length(&d) < range) {
        BtlChar_SetFlag(a, 0x65);
        if (BtlChar_TestFlag(a, 0x4B)) {
            BtlChar_ClearFlag(a, 0x4B);
        }
    }
    return 0;
}

/* Bit index of the active member's model form: costume * 2 + (variant != 0). The original inlined a helper here. */
static inline s32 BtlHit_FormIndex(s32 n, HitGauge *g) {
    return n * 2 + (g->variant != 0);
}

/* Attacker flags 0x4C / 0x4D (direction 0 / 1): a grab attempt within 100 units. Fails against flag 0x42, a form
 * whose parameter bit (0x1000000 << form) is set, flag 0x137, or from behind (animation flag 0x800: when 0x8000 is
 * set). On success fills the defender's reaction block (reaction 9, or 0xE with animation flag 0x800). */
s32 BtlHit_CheckGrab(HitChr *a, HitChr *b) {
    Vec4 pa;
    Vec4 pb;
    Vec4 d;
    s32 dir;
    s32 n;
    f32 range;
    u32 flags;
    s32 t;
    HitGauge *g;
    s32 is800;

    if (BtlChar_TestFlag(b, 0x42)) {
        return 0;
    }
    dir = -1;
    if (BtlChar_TestFlag(a, 0x4C)) {
        dir = 0;
    }
    if (BtlChar_TestFlag(a, 0x4D)) {
        dir = 1;
    }
    if (dir < 0) {
        return 0;
    }
    BtlChar_ClearFlag(a, 0x4C);
    BtlChar_ClearFlag(a, 0x4D);
    n = BtlMember_GetActive(b)->costume;
    n = BtlHit_FormIndex(n, BtlMember_GetActiveGauge(b));
    if (BtlParam_GetFlags(b) & (0x1000000 << n)) {
        return 0;
    }
    if (BtlChar_TestFlag(b, 0x137)) {
        return 0;
    }
    range = 100.0f;
    BtlCharApi_GetNodePos(a->objId, 3, &pa);
    BtlCharApi_GetNodePos(b->objId, 3, &pb);
    Vec4_Sub(&d, &pa, &pb);
    if (range < Vec3_Length(&d)) {
        return 0;
    }
    flags = BtlAnim_GetFlags(BtlAnim_GetId(b));
    is800 = flags & 0x800;
    t = flags & 0x8000;
    if (!is800) {
        t = BtlHit_IsFromBehind(a, b);
    }
    if (t) {
        t = 0;
        goto end;
    }
    t = BtlSkill_GetFrames(a, dir);
    b->react.back = 0;
    b->react.unk48 = t;
    b->react.yaw = BtlChar_GetPos(a)->yaw;
    BtlChar_PlayVoice(b, 0);
    if (is800) {
        b->react.reaction = 0xE;
    } else {
        b->react.reaction = 9;
    }
    BtlChar_ClearFlag(b, 5);
    t = 1;
end:
    return t;
}

/* Returns 1. No caller found. */
s32 BtlHit_Stub(void) {
    return 1;
}

/* Attacker flag 0x4F: raise "contact" (0x65) when BtlMove_IsBlockedByOpponent says so or the two bodies overlap (flat distance
 * below 1.2 * both radii and the height difference inside 1.2 * both body sizes). Returns whether it did. */
s32 BtlHit_CheckTouch(HitChr *a, HitChr *b) {
    Vec4 d;
    f32 h[2];
    f32 k;
    s32 hit;
    f32 dist;
    HitPose *pb;

    if (BtlChar_TestFlag(b, 0x42)) {
        return 0;
    }
    if (!BtlChar_TestFlag(a, 0x4F)) {
        return 0;
    }
    k = 1.2f;
    hit = BtlMove_IsBlockedByOpponent(a) != 0;
    pb = BtlChar_GetPos(b);
    Vec4_Sub(&d, &pb->pos, &BtlChar_GetPos(a)->pos);
    dist = BtlUtil_LengthXZ(&d);
    dist -= BtlCharApi_GetRadius(a->objId) * k;
    dist -= BtlCharApi_GetRadius(b->objId) * k;
    if (dist < 0.0f) {
        h[0] = BtlCharApi_GetHeight(a->objId) * k;
        h[1] = BtlCharApi_GetHeight(b->objId) * k;
        if (-h[0] < d.y && d.y < h[1]) {
            hit = 1;
        }
    }
    if (hit) {
        BtlChar_SetFlag(a, 0x65);
        BtlChar_ClearFlag(a, 0x4F);
    }
    return hit;
}

/* 1 when the attack cannot touch the defender at all: attacker flag 0xBA, defender flag 0x42 (0x43 for attack 0x55),
 * or attack 0x56 from behind (or against animation flags 0x800 + 0x8000). */
s32 BtlHit_IsVoid(HitChr *atk, HitChr *def) {
    s32 k = BtlAtk_GetId(atk);
    s32 behind;
    u32 flags;

    if (BtlChar_TestFlag(atk, 0xBA)) {
        return 1;
    }
    if (k == 0x55) {
        if (BtlChar_TestFlag(def, 0x43)) {
            return 1;
        }
    } else {
        if (BtlChar_TestFlag(def, 0x42)) {
            return 1;
        }
    }
    behind = BtlHit_IsFromBehind(atk, def);
    flags = BtlAnim_GetFlags(BtlAnim_GetId(def));
    if (BtlAtk_GetId(atk) == 0x56) {
        if (flags & 0x800) {
            if (flags & 0x8000) {
                return 1;
            }
        }
        if (behind) {
            return 1;
        }
    }
    return 0;
}

/* Automatic evasion. With the defender free, not flagged 0x5A and in an animation with flags 0xB1 (or flag 0x200000
 * with the hit counter condition / flag 0x60): the attack is avoided when the defender's level (+0x1068) beats the
 * attack and it is its first hit, or a counter at +0xE00 / +0xE04 is left (one is used up), or flags 0x56 / 0x57 are
 * up. The side step is chosen by held direction, else by BtlChar_FrameMod. A second path (flag 0x4E and parameter
 * bit 0x80, frontal, attack id below 11) answers with flags 0x7D / 0x7E and costs the attacker 2000 ki. */
s32 BtlHit_CheckDodge(HitChr *atk, HitChr *def) {
    u32 flags;
    u32 st;
    s32 lvl;
    s32 half;
    s32 on;
    s32 done;
    s32 strike;
    s32 free;
    s32 counted;
    s32 paid;
    s32 dir;

    if (!BtlChar_IsFree(def)) {
        return 0;
    }
    if (BtlChar_TestFlag(def, 0x5A)) {
        return 0;
    }
    st = BtlAnim_GetFlags(BtlAnim_GetId(def));
    on = 0;
    if (st & 0xB1) {
        on = 1;
    }
    if (st & 0x200000) {
        s32 d = BtlChar_GetObj(def)->hitNo;
        d ^= ~BtlChar_GetObj(def)->unkCAC;
        if (d == 0) {
            on = 1;
        }
        if (BtlChar_TestFlag(def, 0x60)) {
            on = 1;
        }
    }
    if (on) {
        flags = BtlAtk_GetFlags(atk);
        done = 0;
        strike = 0;
        free = 0;
        counted = 0;
        paid = 0;
        lvl = def->unk1068;
        half = 0;
        if (flags & 0x4000000) {
            half = 1;
        }
        lvl -= half;
        if (lvl > 0 && (flags & 1) && BtlHit_GetHitNo(atk) == 0) {
            BtlChar_SetFlag(def, 0x12C);
            done = 1;
            free = 1;
            if (flags & 0x100000) {
                paid = BtlMember_HasKi(def, 20000) != 0;
            }
        }
        if (!done) {
            if (def->dodges > 0) {
                def->dodges--;
                done = 1;
                counted = 1;
            }
            if (def->dodgesB > 0) {
                def->dodgesB--;
                done = 1;
                strike = 1;
                counted = 1;
            }
            if (BtlChar_TestFlag(def, 0x56)) {
                done = 1;
                counted = 1;
            }
            if (BtlChar_TestFlag(def, 0x57)) {
                done = 1;
            }
        }
        if (done) {
            if (paid) {
                BtlChar_SetFlag(def, 0x7C);
                BtlMember_SpendKi(def, 20000, 0);
            } else if (strike) {
                BtlChar_SetFlag(def, 0x77);
            } else {
                if (flags & 0x800000) {
                    dir = BtlChar_FrameMod(2);
                    if (BtlInput_IsHeld(def, 0x40)) {
                        dir = 0;
                    }
                    if (BtlInput_IsHeld(def, 0x80)) {
                        dir = 1;
                    }
                } else {
                    dir = BtlChar_FrameMod(3);
                    if (BtlInput_IsHeld(def, 0x40)) {
                        dir = 0;
                    }
                    if (BtlInput_IsHeld(def, 0x80)) {
                        dir = 1;
                    }
                    if (BtlInput_IsHeld(def, 0x20)) {
                        dir = 2;
                    }
                }
                switch (dir) {
                case 0:
                    BtlChar_SetFlag(def, 0x74);
                    break;
                case 1:
                    BtlChar_SetFlag(def, 0x75);
                    break;
                case 2:
                    BtlChar_SetFlag(def, 0x76);
                    break;
                }
                if (flags & 0x1000000) {
                    BtlChar_SetFlag(def, 0x78);
                }
            }
            if (free && BtlMember_HasAbility(def, 0x21)) {
                BtlMember_AddBlast(def, 100000);
            }
            if (counted) {
                switch (def->unkE0C) {
                case 0:
                    BtlChar_SetFlag(def, 0xDE);
                    break;
                case 1:
                    BtlChar_SetFlag(def, 0xDF);
                    break;
                }
            }
            return 1;
        }
    }
    if (BtlChar_TestFlag(def, 0x4E) && (BtlParam_GetFlags2(def) & 0x80) && BtlParam_GetUnk2(atk) != 4 &&
        !BtlHit_IsFromBehind(atk, def) && (u32)BtlAtk_GetId(atk) < 0xB) {
        BtlChar_SetFlag(def, 0x7D);
        BtlChar_SetFlag(atk, 0x7E);
        if (BtlChar_GetObj(atk)->hitNo == 0) {
            BtlMember_SpendKi(atk, 2000, 0);
        }
        return 1;
    }
    return 0;
}

/* 1 when the defender's animation has flag 0x200 and the attack has flag 2: the hit is ignored. */
s32 BtlHit_CheckArmor(HitChr *atk, HitChr *def) {
    s32 ret;

    if (!(BtlAnim_GetFlags(BtlAnim_GetId(def)) & 0x200)) {
        return 0;
    }
    ret = 0;
    if (BtlAtk_GetFlags(atk) & 2) {
        ret = 1;
    }
    return ret;
}

/* Defender flag 0x59 or 0x53: the attacker gets flag 0x6F (pushed back) and the hit is ignored. */
s32 BtlHit_CheckRepel(HitChr *atk, HitChr *def) {
    if (BtlChar_TestFlag(def, 0x59) || BtlChar_TestFlag(def, 0x53)) {
        BtlChar_SetFlag(atk, 0x6F);
        return 1;
    }
    return 0;
}

/* Guard resolution. *full is 0 only for the level-based counter (reaction 8), which takes no chip damage. Returns 1
 * when the hit was guarded in some way (BtlHit_ApplyGuard has run), 0 when it goes through. */
s32 BtlHit_CheckGuard(HitChr *atk, HitChr *def, s32 *full) {
    s32 guard;
    s32 res;

    *full = 1;
    if (!BtlChar_IsFree(def)) {
        return 0;
    }
    guard = BtlColl_GetGuardKind(def);
    if (BtlChar_GetObj(atk)->hitNo == 0 && (BtlAnim_GetFlags(BtlAnim_GetId(def)) & 0x31) && def->unk1070 > 0) {
        BtlHit_ApplyGuard(atk, def, 8);
        *full = 0;
        return 1;
    }
    if (guard != 6) {
        if (BtlHit_IsFromBehind(atk, def)) {
            return 0;
        }
    }
    if (BtlChar_TestFlag(def, 0x40) && (u32)BtlAtk_GetId(atk) < 0xB) {
        BtlHit_ApplyGuard(atk, def, 1);
        return 1;
    }
    if (BtlChar_TestFlag(def, 0x41) && (BtlAtk_GetFlags(atk) & 0x10) && BtlInput_IsCmdHeld(def, 0x10)) {
        if (BtlInput_IsHeld(def, 0x10)) {
            switch (BtlAtk_GetGuardKindB(atk)) {
            case 0:
            case 2:
                BtlHit_ApplyGuard(atk, def, 2);
                return 1;
            }
        } else if (BtlInput_IsHeld(def, 0x20)) {
            switch (BtlAtk_GetGuardKindC(atk)) {
            case 0:
            case 2:
                BtlHit_ApplyGuard(atk, def, 3);
                return 1;
            }
        } else {
            switch (BtlAtk_GetGuardKindA(atk)) {
            case 0:
            case 2:
                BtlHit_ApplyGuard(atk, def, 4);
                return 1;
            }
        }
    }
    if (guard == 0) {
        return 0;
    }
    res = 0;
    switch (guard) {
    case 1:
        res = BtlAtk_GetGuardKindA(atk);
        break;
    case 2:
        res = BtlAtk_GetGuardKindB(atk);
        break;
    case 3:
        res = BtlAtk_GetGuardKindC(atk);
        break;
    case 5:
        res = BtlAtk_GetGuardKindE(atk);
        break;
    case 4:
        res = BtlAtk_GetBestGuardKind(atk);
        break;
    case 6:
        res = 0;
        break;
    }
    if (res == 3) {
        return 0;
    }
    switch (res) {
    case 0:
        BtlHit_ApplyGuard(atk, def, 0);
        break;
    case 1:
        BtlHit_ApplyGuard(atk, def, 5);
        break;
    case 2:
        BtlHit_ApplyGuard(atk, def, 6);
        break;
    case 4:
        BtlHit_ApplyGuard(atk, def, 7);
        break;
    }
    return 1;
}

/* Attack 0x55 only (a throw): 1 when it fails. Against parameter byte 4, or parameter bit 0x4000 with attack flag
 * 0x4000, the attacker gets flag 0x7A; a free defender in a 0x31 animation that is facing the attacker and has a
 * level at +0x1078 also gets 0x79 (the break). */
s32 BtlHit_CheckThrowBreak(HitChr *atk, HitChr *def) {
    u32 st;

    if (BtlAtk_GetId(atk) != 0x55) {
        return 0;
    }
    if (BtlParam_GetUnk2(def) == 4) {
        BtlChar_SetFlag(atk, 0x7A);
        BtlCharSnd_PlayCommon(def, 0x44);
        BtlChar_PlayVoice(atk, 0);
        return 1;
    }
    if ((BtlParam_GetFlags(def) & 0x4000) && (BtlAtk_GetFlags(atk) & 0x4000)) {
        BtlChar_SetFlag(atk, 0x7A);
        BtlCharSnd_PlayCommon(def, 0x44);
        BtlChar_PlayVoice(atk, 0);
        return 1;
    }
    if (!BtlChar_IsFree(def)) {
        return 0;
    }
    st = BtlAnim_GetFlags(BtlAnim_GetId(def));
    if ((st & 0x31) && !(st & 0x800) && !BtlHit_IsFromBehind(atk, def) && def->unk1078 > 0) {
        BtlChar_SetFlag(atk, 0x7A);
        BtlChar_SetFlag(def, 0x79);
        BtlCharSnd_PlayCommon(def, 0x44);
        BtlChar_PlayVoice(atk, 0);
        BtlChar_PlayVoice(def, 0x11);
        return 1;
    }
    return 0;
}

/* Start of the hit step for one fighter: reaction id back to 1, and "this hit was handled" (0x60) is dropped when
 * the model's hit counter changed. A counter that ends without having been handled raises held flag 0x5C. */
void BtlHit_BeginFrame(HitChr *chr) {
    HitStatus *st = &chr->status;
    HitReact *react = &chr->react;
    HitObj *obj = BtlChar_GetObj(chr);

    react->reaction = 1;
    if (BtlChar_TestFlag(chr, 0x2B)) {
        BtlChar_ClearFlag(chr, 0x60);
    }
    if (obj->hitNo >= 0) {
        if (st->lastHitNo != obj->hitNo) {
            BtlChar_ClearFlag(chr, 0x60);
        }
    } else {
        if (st->lastHitNo >= 0) {
            if (!BtlChar_TestFlag(chr, 0x60) && !BtlChar_TestFlag(chr, 0x2B) && obj->hitNo == ~obj->unkCAC) {
                BtlChar_SetHeldFlag(chr, 0x5C);
            }
        }
        BtlChar_ClearFlag(chr, 0x60);
    }
    st->lastHitNo = obj->hitNo;
}

/* Two fighters that both carry flag 0x47 (or a 0x47 / 0x48 mix) and touch: both get flag 0x61 (0x62 for a mix),
 * hit-stop and the "struck" flags 0x44..0x46. Returns 1, which ends the hit step for this frame. */
s32 BtlHit_CheckClash(void) {
    s32 i,j,result=0;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        HitChr *a = BtlChar_Get(i);
        for (j=0;j<BtlChar_GetCount();j++) {
        HitChr *b = BtlChar_Get(j);
        if (TagTeam_Active()) {
            extern s32 TagTeam_BodiesTouch(s32,s32);
            if (j<=i || TagTeam_CinematicPaused(i) || TagTeam_CinematicPaused(j) || !TagTeam_BodiesTouch(i,j) || BtlClash_Opponent(i)>=0 || BtlClash_Opponent(j)>=0) continue;
        } else if (j!=BtlOpp_GetPlayer(a)) continue;

        if (BtlChar_TestFlag(a, 3)) {
            s32 same = 0;
            s32 mixed;

            if (BtlChar_TestFlag(a, 0x47) && BtlChar_TestFlag(b, 0x47)) {
                same = 1;
            }
            mixed = 0;
            if (BtlChar_TestFlag(a, 0x48) && BtlChar_TestFlag(b, 0x48)) {
                mixed = 1;
            }
            if (BtlChar_TestFlag(a, 0x47) && BtlChar_TestFlag(b, 0x48)) {
                mixed = 1;
            }
            if (BtlChar_TestFlag(a, 0x48) && BtlChar_TestFlag(b, 0x47)) {
                mixed = 1;
            }
            if ((same || mixed) && (TagTeam_Active() || BtlMove_IsBlockedByOpponent(a) || BtlMove_IsBlockedByOpponent(b))) {
                if (!BtlClash_Join(a->player, b->player)) continue;
                if (same) {
                    BtlChar_SetFlag(a, 0x61);
                    BtlChar_SetFlag(b, 0x61);
                    a->stopLen = 6;
                    b->stopLen = 6;
                    a->stopDelay = 0;
                    b->stopDelay = 0;
                    BtlCharSnd_PlayCommon(a, 0x4F);
                    BtlCharSnd_PlayCommon(b, 0x4B);
                } else {
                    BtlChar_SetFlag(a, 0x62);
                    BtlChar_SetFlag(b, 0x62);
                    a->stopLen = 2;
                    b->stopLen = 2;
                    a->stopDelay = 0;
                    b->stopDelay = 0;
                    BtlCharSnd_PlayCommon(a, 0x4F);
                    BtlCharSnd_PlayCommon(b, 0x4B);
                    BtlChar_SetFxBit(a, 0x2C);
                }
                BtlChar_SetFlag(a, 0x44);
                BtlChar_SetFlag(a, 0x45);
                BtlChar_SetFlag(a, 0x46);
                BtlChar_SetFlag(b, 0x44);
                BtlChar_SetFlag(b, 0x45);
                BtlChar_SetFlag(b, 0x46);
                result=1;
            }
        }
        }
    }
    return result;
}

/* Contact of a rushing attack (attacker flag 0x49, touching). In order: miss (0x63) against flag 0x42 or animation
 * flag 0x800; bounce (0x64 + 0x129); throw break; automatic evasion (level or counters); guard from the front
 * (attacker 0x6F, defender loses 20000 ki); else BtlColl_StartThrow, whose non-zero result ends the hit step. */
s32 BtlHit_CheckRush(void) {
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        HitChr *a = BtlChar_Get(i);
        HitChr *b = BtlChar_Get(BtlOpp_GetPlayer(a));
        u32 st;
        s32 tech;
        u32 tf;
        s32 flag;

        if(TagTeam_CinematicPaused(a->player) || TagTeam_CinematicPaused(b->player)) continue;
        if (!BtlChar_TestFlag(a, 0x49)) {
            continue;
        }
        if (TagTeam_Active() ? !TagTeam_BodiesTouch(a->player,b->player) : !BtlMove_IsBlockedByOpponent(a)) {
            continue;
        }
        BtlAnim_GetFlags(BtlAnim_GetId(a));
        st = BtlAnim_GetFlags(BtlAnim_GetId(b));
        tech = BtlAct_GetCurrentClass(a);
        tf = BtlSuper_GetFlags(a, tech);
        if (BtlChar_TestFlag(b, 0x42) || (st & 0x800)) {
            goto miss;
        }
        if (BtlChar_TestFlag(b, 0x49) && BtlChar_TestFlag(b, 0xA5) && !BtlChar_TestFlag(a, 0xA5)) {
            goto miss;
        }
        if ((BtlParam_GetUnk2(b) == 4 && !(tf & 0x8000000)) || BtlChar_TestFlag(b, 0x59)) {
            BtlChar_SetFlag(a, 0x64);
            BtlChar_SetFlag(a, 0x129);
            continue;
        }
        if (BtlChar_IsFree(b) && b->unk107C > 0) {
            if (BtlChar_TestFlag(a, 0xA5)) {
                if ((st & 0x31) && !BtlHit_IsFromBehind(a, b)) {
                    BtlChar_SetFlag(a, 0x7A);
                    BtlChar_SetFlag(b, 0x79);
                    BtlCharSnd_PlayCommon(b, 0x44);
                    BtlChar_PlayVoice(a, 0);
                    BtlChar_PlayVoice(b, 0x11);
                    continue;
                }
            } else {
                if ((st & 0xB1) && !BtlChar_TestFlag(b, 0x5A)) {
                    BtlChar_SetFlag(a, 0x63);
                    if (BtlChar_FrameMod(2)) {
                        flag = 0x74;
                    } else {
                        flag = 0x75;
                    }
                    BtlChar_SetFlag(b, flag);
                    if (BtlMember_HasAbility(b, 0x22)) {
                        BtlMember_AddBlast(b, 100000);
                    }
                    continue;
                }
            }
        }
        if (BtlChar_IsFree(b) && !BtlChar_TestFlag(b, 0x5A)) {
            s32 n0 = 0;
            s32 n1 = 0;

            if (b->dodges > 0) {
                n0 = 1;
            }
            if (b->dodgesB > 0) {
                n1 = 1;
            }

            if ((n0 || n1) && (st & 0xB1)) {
                if (n0) {
                    b->dodges--;
                }
                if (n1) {
                    b->dodgesB--;
                }
                BtlChar_SetFlag(a, 0x63);
                if (n0) {
                    if (BtlChar_FrameMod(2)) {
                        flag = 0x74;
                    } else {
                        flag = 0x75;
                    }
                    BtlChar_SetFlag(b, flag);
                }
                if (n1) {
                    BtlChar_SetFlag(b, 0x77);
                }
                switch (b->unkE0C) {
                case 0:
                    BtlChar_SetFlag(b, 0xDE);
                    break;
                case 1:
                    BtlChar_SetFlag(b, 0xDF);
                    break;
                }
                continue;
            }
        }
        if (BtlColl_IsGuarding(b) && !BtlHit_IsFromBehind(a, b) && !(tf & 1) && !BtlChar_TestFlag(a, 0xA5)) {
            BtlChar_SetFlag(a, 0x6F);
            BtlChar_SetFlag(a, 0x129);
            BtlChar_SetFxBit(b, 0x20);
            BtlChar_SetFlag(b, 0x6D);
            BtlCharSnd_PlayCommon(a, 0x42);
            BtlChar_SetFlag(b, 0x94);
            b->react.turnYaw = BtlUtil_WrapAngle(BtlChar_GetPos(a)->yaw + 3.14159265f);
            b->react.turnPitch = 0;
            BtlMember_SpendKi(b, 20000, 0);
            if (BtlMember_IsKiEmpty(b)) {
                BtlChar_SetHeldFlag(b, 0xBE);
            }
            continue;
        }
        if (!BtlChar_TestFlag(a, 3)) {
        miss:
            BtlChar_SetFlag(a, 0x63);
            continue;
        }
        if (BtlColl_StartThrow(a, b, tech, tf)) {
            return 1;
        }
    }
    return 0;
}

/* Every ordered pair of fighters: the three proximity tests. */
void BtlHit_CheckProximityAll(void) {
    s32 i;
    s32 j;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        for (j = 0; j < BtlChar_GetCount(); j++) {
            if (TagTeam_IsEnemy(i, j) && TagTeam_CanContact(i,j)) {
                HitChr *a = BtlChar_Get(i);
                HitChr *b = BtlChar_Get(j);

                BtlHit_CheckNear(a, b);
                BtlHit_CheckGrab(a, b);
                BtlHit_CheckTouch(a, b);
            }
        }
    }
}

/* Does this fighter's attack connect this frame? Needs an attack id, active hit volumes or "contact" (0x65), and
 * the opponent within 90 degrees of the facing (or attack flag 8). Marks the hit handled (0x60), then runs the
 * avoid / guard / throw-break tests in order; if none applies the opponent is stored as the pending target. */
void BtlHit_TestHit(HitChr *chr) {
    HitStatus *st = &chr->status;
    HitObj *obj = BtlChar_GetObj(chr);
    s32 full;
    if(!TagTeam_CanContact(chr->player,TagTeam_Target(chr->player))) return;

    if (BtlChar_TestFlag(chr, 0x60)) {
        return;
    }
    if (BtlAtk_GetId(chr) == -1) {
        return;
    }
    if (TagContact_Has(chr->player,TagTeam_Target(chr->player)) || BtlChar_TestFlag(chr, 0x65)) {
        s32 ok = 0;
        s32 oppIdx = BtlOpp_GetPlayer(chr);
        HitChr *opp = BtlChar_Get(oppIdx);

        if (__builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) < 1.5707963f) {
            ok = 1;
        }
        if (BtlAtk_GetFlags(chr) & 8) {
            ok = 1;
        }
        if (ok) {
            BtlChar_SetHeldFlag(chr, 0x60);
            if (BtlHit_IsVoid(chr, opp) || BtlHit_CheckDodge(chr, opp) || BtlHit_CheckArmor(chr, opp) ||
                BtlHit_CheckRepel(chr, opp)) {
                BtlChar_SetHeldFlag(chr, 0x5C);
            } else if (BtlHit_CheckGuard(chr, opp, &full)) {
                if (full) {
                    TagBalance_Damage(opp, BtlAtk_GetGuardDamage(chr), 0xC, *(s32 *)chr);
                }
            } else if (!BtlHit_CheckThrowBreak(chr, opp)) {
                st->target = oppIdx;
            }
        }
    }
    if (BtlChar_TestFlag(chr, 0x65)) {
        chr->status.contact++;
    }
}

/* Two fighters that hit each other in the same frame: the higher attack priority wins and the other hit is
 * dropped; equal priorities cancel both (spark effect, camera shake, vibration, held flag 0x5D). */
void BtlHit_ResolveTrades(void) {
    s32 i;
    s32 j;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        for (j = i + 1; j < BtlChar_GetCount(); j++) {
            HitChr *a = BtlChar_Get(i);
            HitChr *b = BtlChar_Get(j);
            HitStatus *sa = &a->status;
            HitStatus *sb = &b->status;

            if (sa->target == b->player && sb->target == a->player) {
                s32 pa = BtlAtk_GetPriority(a);
                s32 pb = BtlAtk_GetPriority(b);

                if (pa > pb) {
                    sb->target = -1;
                } else if (pa < pb) {
                    sa->target = -1;
                } else {
                    sa->target = -1;
                    sb->target = -1;
                    BtlChar_SetFxBit(a, 0x22);
                    BtlChar_SetFxBit(b, 0x22);
                    ChrCam_AddShake(a, 2.0f, 0.3f);
                    ChrCam_AddShake(b, 2.0f, 0.3f);
                    BtlChar_Vibrate(a, 1.0f, 0.1f);
                    BtlChar_Vibrate(b, 1.0f, 0.1f);
                    BtlChar_SetHeldFlag(a, 0x5D);
                    BtlChar_SetHeldFlag(b, 0x5D);
                }
            }
        }
    }
}

/* Applies the pending hit of one attacker to its target: picks the reaction from the attack record by the
 * defender's animation flags (or a "does not flinch" reaction 2 from the armour level comparison), fills the
 * defender's reaction block, shakes cameras and pads, then takes health with BtlMember_Damage and gives the attacker
 * its ki and gauge gains. */
void BtlHit_ApplyHit(HitChr *atk) {
    s32 drain = 0;
    s32 launch = 0;
    HitChr *def;
    HitObj *dobj;
    s32 kind;
    s32 af;
    s32 state;
    u32 st;
    f32 yaw;
    s32 back;
    s32 react;
    f32 power;
    f32 time;
    f32 a;
    f32 b;

    if (atk->status.target < 0 || !TagTeam_CanContact(atk->player,atk->status.target)) {
        return;
    }
    def = BtlChar_Get(atk->status.target);
    dobj = BtlChar_GetObj(def);
    kind = BtlAtk_GetId(atk);
    /* A third fighter may have landed a hit earlier in this contact pass.
       Do not start half a grab when its attacker is already being knocked out
       of the catch. Successful grabs are protected from subsequent contacts. */
    if (TagTeam_Active() && kind == 0x55 && atk->react.reaction >= 3) return;
    af = BtlAtk_GetFlags(atk);
    state = BtlAnim_GetId(def);
    st = BtlAnim_GetFlags(state);
    if (af & 0x10000000) {
        yaw = BtlOpp_GetYaw(atk);
    } else {
        yaw = BtlChar_GetPos(atk)->yaw;
    }
    back = BtlHit_IsFromBehind(atk, def);
    if (st & 0x1000) {
        react = BtlAtk_GetReactionD(atk);
    } else if (st & 0x2000) {
        react = BtlAtk_GetReactionE(atk);
    } else if ((st & 0x4000) || BtlAct_GetCurrent(def) == 0xDF) {
        react = BtlAtk_GetReactionC(atk);
    } else if ((u32)(state - 0xAE) < 2 || (st & 0x200)) {
        react = BtlAtk_GetReactionG(atk);
    } else {
        s32 lvl = 0;

        if (BtlParam_GetFlags(def) & 4) {
            lvl = 1;
        }
        if (BtlParam_GetFlags(def) & 8) {
            lvl--;
        }
        if (BtlParam_GetFlags(atk) & 4) {
            lvl--;
        }
        if (BtlParam_GetFlags(atk) & 8) {
            lvl++;
        }
        if (BtlAct_TestPoweredSkill(def, 0x40)) {
            lvl++;
        }
        if (def->unkE14 > 0) {
            lvl++;
        }
        if (BtlMember_HasAbility(def, 0x46)) {
            lvl++;
        }
        if (BtlAct_TestPoweredSkill(atk, 0x100)) {
            lvl--;
        }
        if (BtlMember_HasAbility(atk, 0x47)) {
            lvl--;
        } else if (BtlMember_HasAbility(atk, 0x70)) {
            if (BtlChar_TestFlag(atk, 6)) {
                lvl--;
            }
        }
        lvl -= BtlAtk_GetUnk2C(atk);
        {
            extern s32 TagGiant_Armor(s32);
            lvl += TagGiant_Armor(def->player);
        }
        if (lvl > 0 && (af & 4)) {
            react = 2;
            if (BtlParam_GetFlags(def) & 4) {
                drain = 1;
            }
        } else if (st & 0x800) {
            react = BtlAtk_GetReactionB(atk);
        } else if (st & 0x100) {
            if (back || (BtlObjAnim_QueryEvent(dobj, 1, 0, 3) && dobj->hitNo == ~dobj->unkCAC)) {
                react = BtlAtk_GetReaction(atk);
            } else {
                react = BtlAtk_GetReactionF(atk);
            }
        } else {
            react = BtlAtk_GetReaction(atk);
        }
    }
    if (react == 1) {
        BtlChar_SetHeldFlag(atk, 0x5C);
        return;
    }
    if (react == 0x14) {
        if (BtlParam_GetFlags(atk) & 0x200) {
            react = 0x25;
        }
    }
    switch (react) {
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x25:
    case 0x31:
        launch = 1;
        break;
    default:
        if (def->unkE18 > 0) {
            react = 2;
        }
        break;
    }
    def->react.reaction = react;
    if (react != 2) {
        def->react.unk4 = BtlAtk_GetReactionSub(atk);
        if (BtlChar_TestMemberUnk70(atk)) {
            def->react.unk4 = 0x13;
        }
        def->react.silent = (af >> 10) & 1;
        if (react == 0x14) {
            def->react.scale = 1.0f;
        } else if (af & 0x2000000) {
            def->react.scale = atk->unkD78;
        } else {
            def->react.scale = atk->unkD78 * BtlParam_GetHitReactScale(def);
        }
        if (af & 0x40) {
            BtlChar_SetHeldFlag(def, 0xE);
        }
        if (af & 0x400000) {
            BtlChar_SetHeldFlag(def, 0x1E);
        }
        if (def->react.silent == 0) {
            BtlChar_PlayVoice(def, BtlHit_GetVoiceKind(def->react.reaction));
        }
    } else {
        def->react.unk38 = 3;
        if (drain) {
            BtlMember_SpendKi(def, 2000, 0);
        }
    }
    if (BtlAtk_GetHitFxKind(atk)) {
        BtlChar_SetFxBit(def, 0x1D);
    }
    power = BtlAtk_GetShakePower(atk);
    time = BtlAtk_GetShakeTime(atk);
    if (af & 0x800) {
        ChrCam_AddShake(atk, power, time);
    }
    ChrCam_AddShake(def, power, time);
    BtlChar_Vibrate(atk, power * 0.5f, time * 0.5f);
    BtlChar_Vibrate(def, power * 0.5f, time * 0.5f);
    if (!(af & 0x80000)) {
        BtlChar_SetFxBit(atk, 0x1B);
    }
    if (af & 0x10000) {
        BtlChar_SetFxBit(atk, 0x25);
    }
    if (af & 0x20000) {
        BtlChar_SetFxBit(atk, 0x26);
    }
    if (af & 0x40000) {
        BtlChar_SetFxBit(atk, 0x27);
    }
    BtlChar_ClearFlag(atk, 0x8A);
    BtlChar_SetHeldFlag(atk, 0x5B);
    if (st & 2) {
        BtlChar_SetFlag(atk, 0x71);
    }
    if (af & 0x200) {
        BtlChar_SetHeldFlag(atk, 0x86);
    }
    if (af & 0x20) {
        BtlMove_AimVerticalAtOpponent(atk, 0);
    }
    if (react == 0x31) {
        BtlChar_SetFlag(atk, 0x7F);
    }
    if (react != 2) {
        if (launch) {
            f32 la = BtlAtk_GetLaunchAngleA(atk);
            f32 lb = BtlAtk_GetLaunchAngleB(atk);

            def->react.back = (af >> 12) & 1;
            if (BtlChar_TestMemberUnk70(atk) && kind == 0x55) {
                def->react.back = 0;
                la = 0.0f;
                lb = -0.78539807f;
            }
            def->react.yaw = yaw;
            def->react.launchA = la;
            def->react.launchB = lb;
            def->react.turnYaw = 0.0f;
            def->react.turnPitch = 0.0f;
            BtlChar_SetFlag(atk, 0x44);
            BtlChar_SetFlag(def, 0x44);
            BtlChar_SetFlag(atk, 0x45);
            BtlChar_SetFlag(def, 0x45);
            BtlChar_SetFlag(atk, 0x46);
            BtlChar_SetFlag(def, 0x46);
        } else {
            def->react.back = back;
            def->react.yaw = yaw;
            BtlMove_SetImpulseYaw(def, def->react.yaw, BtlAtk_GetPushOnHit(atk));
            if (af & 0x100) {
                BtlChar_SetFlag(def, 0x94);
                a = BtlAtk_GetLaunchAngleA(atk);
                b = BtlAtk_GetLaunchAngleB(atk);
                if (af & 0x200000) {
                    a = 0.0f;
                    b = BtlChar_GetPos(atk)->unk90;
                }
                if (def->react.back) {
                    def->react.turnYaw = BtlUtil_WrapAngle(def->react.yaw + a);
                    def->react.turnPitch = b;
                } else {
                    def->react.turnYaw = BtlUtil_WrapAngle(BtlUtil_WrapAngle(def->react.yaw + a) + 3.14159265f);
                    def->react.turnPitch = -b;
                }
            }
            def->react.launchA = 0.0f;
            def->react.launchB = 0.0f;
        }
    }
    if (kind == 0x55) {
        s32 v = (BtlParam_GetFlags(atk) >> 19) & 1;

        if(react!=2) TagTeam_GrabContact(atk->player,def->player);
        atk->react.unk10 = v;
        def->react.unk10 = v;
    } else {
        atk->react.unk10 = 0;
        def->react.unk10 = 0;
    }
    switch (react) {
    case 0x14:
    case 0x16:
    case 0x25:
        def->react.damage = BtlAtk_GetDamage(atk);
        break;
    default: {
        s32 type = 0;
        s32 dmg = BtlAtk_GetDamage(atk);

        switch (kind) {
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
            dmg += dmg * BtlUtil_Clamp(atk->unkD88 + def->unkD88 - 1, 0, 5) / 5;
            break;
        default:
            if (def->react.back) {
                if (!(af & 0x8000000) && !(st & 0x800)) {
                    dmg += dmg / 5;
                }
            }
            break;
        }
        switch (kind) {
        case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            type = 0x8000000;
            break;
        case 0x3E: case 0x3F: case 0x40: case 0x41: case 0x42:
            type = 0x4000000;
            break;
        case 0x61: case 0x62:
            type = 0x200000 + atk->unkE0C;
            break;
        }
        TagBalance_Damage(def, dmg, type, *(s32 *)atk);
        break;
    }
    }
    if (!BtlChar_TestFlag(atk, 0x98)) {
        BtlMember_AddKi(atk, BtlAtk_GetKiGain(atk));
    }
    atk->unkD80 += BtlAtk_GetUnk10(atk);
    if (atk->unkD80 > 100000) {
        atk->unkD80 = 100000;
    }
    BtlChar_RaiseFirstClash(atk);
    BtlChar_AddStageTimer(BtlAtk_GetShakeTime(atk) * 2.0f);
}

/*
 * Tail of the object, 0x1CA520..0x1CA6D0: the per-frame collision step and the guard kind of the current
 * animation. These four functions are part of this translation unit in the original: BtlColl_Update only compiles
 * to the original bytes with BtlHit_CheckProximityAll defined above it, and the functions from 0x1CAEF0 on
 * (src/battle/btl_char_coll_b.c) only match when BtlColl_GetGuardKind is NOT defined in theirs.
 */

/* The per-frame fighter-against-fighter step, run by BtlChars_UpdateCollision when nobody is frozen (after the
 * effect scene's hit detection, BtlBodyHit_Update): guard-direction bookkeeping per fighter, then the clash test, the
 * rush test (each ends the step when it fires), the proximity test, per fighter the hit test, the trade
 * resolution, and per fighter the hit application. */
void BtlColl_Update(void) {
    s32 i;
    HitChr *chr;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlHit_BeginFrame(BtlChar_Get(i));
    }
    if (BtlHit_CheckClash()) {
        return;
    }
    if (!BtlHit_CheckRush()) {
        BtlHit_CheckProximityAll();
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            BtlColl_UpdateFrozen(chr);
            BtlHit_TestHit(chr);
        }
        BtlHit_ResolveTrades();
        for (i = 0; i < BtlChar_GetCount(); i++) {
            BtlHit_ApplyHit(BtlChar_Get(i));
        }
    }
}

/* All the collision step does for a fighter while somebody is frozen (also run before each hit test): no target. */
void BtlColl_UpdateFrozen(HitChr *chr) {
    chr->status.target = -1;
}

/* Guard kind of the fighter's current animation (ids 0xEC..0xF7): 1..5, or 0 when it is not a guard animation. */
s32 BtlColl_GetGuardKind(HitChr *chr) {
    switch (BtlAnim_GetId(chr)) {
    case 0xEF:
    case 0xF0:
        return 1;
    case 0xF1:
    case 0xF2:
        return 2;
    case 0xF3:
    case 0xF4:
        return 3;
    case 0xEC:
    case 0xF5:
        return 4;
    case 0xF7:
        return 5;
    case 0xED:
    case 0xEE:
    case 0xF6:
        return 0;
    }
    return 0;
}

/* Whether the guard kind is 1..4. */
s32 BtlColl_IsGuarding(HitChr *chr) {
    s32 kind = BtlColl_GetGuardKind(chr);

    switch (kind) {
    case 1 ... 4:
        return 1;
    }
    return 0;
}
