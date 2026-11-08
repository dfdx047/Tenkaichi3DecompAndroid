#include "battle/tag_balance.h"
#include "battle/tag_lobby.h"
#include "common.h"
#include "battle/btl_char_coll.h"

/*
 * Fighter hit reactions, 0x1CA6D0..0x1CDCA8: what a fighter does when the hit detection (0x1AF740..0x1B10F0)
 * delivers a hit record to it, the queries that detection makes first (dodge, deflect, reflect, absorb), throws,
 * the ground / water / ring-out step, and a few small helpers. See include/battle/btl_char_coll.h.
 *
 * The object boundary at 0x1CA6D0 is inferred: the functions from 0x1CAEF0 on match only when
 * BtlColl_GetGuardKind (0x1CA628) is an external function, so it was not defined in this translation unit. The
 * exact start may be anywhere in 0x1CA6D0..0x1CAEF0 (the four functions in between match either way).
 * The function after this file, BtlColl_NextPoolMember (0x1CDCA8), is the first one of btl_char_member.c.
 */

extern Vec4 D_002EC2A0; /* zero vector */

extern s32 BtlChar_GetCount(void);
extern BtlCollChr *BtlChar_Get(s32 i);
extern BtlCollChr *BtlChar_FindByObjId(s32 objId);
extern BtlCollObj *BtlChar_GetObj(BtlCollChr *chr);
extern BtlCollPose *BtlChar_GetPos(BtlCollChr *chr);
extern s32 BtlChar_IsFrozen(BtlCollChr *chr);
extern s32 BtlChar_IsFree(BtlCollChr *chr);
extern s32 BtlChar_IsDead(BtlCollChr *chr);
extern s32 BtlChar_FrameMod(s32 n);
extern s32 BtlChar_IsStage4Or27(void);
extern void BtlChar_AddStageTimer(f32 seconds);
extern void BtlChar_Vibrate(BtlCollChr *chr, f32 power, f32 seconds);
extern void BtlChar_PlayVoice(BtlCollChr *chr, s32 kind);
extern s32 BtlChar_TestFlag(BtlCollChr *chr, s32 flag);
extern void BtlChar_SetFlag(BtlCollChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(BtlCollChr *chr, s32 flag);
extern void BtlChar_ClearFlag(BtlCollChr *chr, s32 flag);
extern s32 BtlInput_IsHeld(BtlCollChr *chr, u32 mask);
extern void ChrCam_AddShake(BtlCollChr *chr, f32 strength, f32 time);
extern void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 strength, f32 time);
extern void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
extern void BtlEvent_Raise(s32 side, s32 ev);
extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_LengthXZ(Vec4 *v);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 k);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 atan2f(f32 y, f32 x);

extern s32 BtlColl_GetGuardKind(BtlCollChr *chr); /* guard kind; btl_char_hit.c; must stay external, see above */

extern s32 BtlAnim_GetId(BtlCollChr *chr);            /* chr->unk974: state id */
extern u32 BtlAnim_GetFlags(s32 state);                  /* roster tbl[0][state]: flag word of the state */
extern f32 BtlAnim_GetFrame(BtlCollChr *chr);
extern f32 BtlAnim_GetLength(BtlCollChr *chr);
extern f32 BtlAnim_GetStep(BtlCollChr *chr);
extern BtlCollGauge *BtlMember_GetActiveGauge(BtlCollChr *chr);  /* active member's gauge block */
extern s32 BtlMember_Damage(BtlCollChr *chr, s32 damage, s32 flags); /* applies damage */
extern s32 BtlMember_AddKi(BtlCollChr *chr, s32 n);     /* gauge +0xC += n, returns the overflow */
extern void BtlMember_AddBlast(BtlCollChr *chr, s32 n);    /* gauge +0x14 += n, clamped */
extern void BtlMember_AddMaxPower(BtlCollChr *chr, s32 n);    /* gauge +0x1C += n, clamped to 30000 */
extern s32 BtlMember_HasAbility(BtlCollChr *chr, s32 n);     /* active member has ability n */
extern void BtlChar_SetFxBit(BtlCollChr *chr, s32 n);    /* sets bit n of the byte set at chr + 0x1262 */
extern void BtlChar_GetSnapDelta(BtlCollChr *chr, Vec4 *out, s32 arg2, s32 arg3);
extern void BtlCharSnd_RequestAt(Vec4 *pos, s32 arg1, s32 sound, f32 near, f32 far); /* positional sound */
extern void BtlCharSnd_PlayCommon(BtlCollChr *chr, s32 sound); /* sound at the fighter */
extern s32 BtlChar_IsFlagRaised(BtlCollChr *chr, s32 flag);  /* flag rose this frame */
extern void BtlChar_RaiseFirstClash(BtlCollChr *chr);           /* first-hit bookkeeping (flag 0xE3, event 0x3C) */
extern void BtlOpp_GetDelta(BtlCollChr *chr, Vec4 *out);
extern f32 BtlOpp_GetYawFromFacing(BtlCollChr *chr);
extern f32 BtlOpp_GetHalfHeightDiff(BtlCollChr *chr);
extern void BtlMove_SetImpulseDir(BtlCollChr *chr, Vec4 *dir, f32 power); /* knock-back push */
extern s32 BtlAct_GetCurrent(BtlCollChr *chr);            /* chr->action */
extern s32 BtlAct_IsTechniqueId(s32 action);
extern s32 BtlAct_GetCurrentClass(BtlCollChr *chr);            /* class number of the action */
extern s32 BtlAct_TestPoweredSkill(BtlCollChr *chr, s32 mask);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out); /* world position of a model node */
extern s32 BtlParam_GetFlags(BtlCollChr *chr);            /* parameter word +0x10 of the fighter's object */
extern s32 BtlParam_GetUnk2(BtlCollChr *chr);
extern f32 BtlParam_GetUnkC7Scale(BtlCollChr *chr);
extern s32 BtlObjAnim_QueryEvent(BtlCollObj *obj, u64 arg1, s32 arg2, s32 arg3);
extern s32 BtlStage_GetWaterLevel(f32 *height);
extern s32 EftHit_GetHitCount(BtlCollHit *hit);

/* Blast parameter readers (hit record). */
extern s32 BtlKiBlast_GetFlagsOfHit(BtlCollHit *hit);            /* blast flag word */
extern s32 BtlKiBlast_GetDamageOfHit(BtlCollHit *hit);            /* damage */
extern s32 BtlKiBlast_GetGuardDamageOfHit(BtlCollHit *hit);            /* damage against a changing fighter */
extern s32 BtlKiBlast_GetKiCostOfHit(BtlCollHit *hit);
extern s32 BtlKiBlast_GetReactOfHit(BtlCollHit *hit);            /* reaction ids */
extern s32 BtlKiBlast_GetReactS800OfHit(BtlCollHit *hit);
extern s32 BtlKiBlast_GetReactS4000OfHit(BtlCollHit *hit);
extern s32 BtlKiBlast_GetReactS200OfHit(BtlCollHit *hit);
extern s32 BtlKiBlast_GetTypeOfHit(BtlCollHit *hit);            /* sound class */
extern f32 BtlKiBlast_GetPushOfHit(BtlCollHit *hit);            /* push */
extern f32 BtlKiBlast_GetGuardPushOfHit(BtlCollHit *hit);

/* Rush-attack parameter readers (attacker, slot 2..4). */
extern s32 BtlSuper_GetFlagsA(BtlCollChr *chr, s32 slot);
extern u32 BtlSuper_GetFlags(BtlCollChr *chr, s32 slot);  /* technique flag word */
extern s32 BtlSuper_GetId(BtlCollChr *chr, s32 slot);  /* technique id */
extern s32 BtlSuper_GetUnk1E(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetShots(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetHitsB(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetHitDirKind(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetType(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetReact(BtlCollChr *chr, s32 slot);  /* reaction ids */
extern s32 BtlSuper_GetReactAlt(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetReactS800(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetReactAltS800(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetUnk14D(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetGuardKind(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetHitSound(BtlCollChr *chr, s32 slot, s32 alt); /* hit sound kind */
extern s32 BtlSuper_GetStepCount(BtlCollChr *chr, s32 slot);
extern f32 BtlSuper_GetPush(BtlCollChr *chr, s32 slot);
extern f32 BtlSuper_GetGuardPush(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetKiCost(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetRecoilDamage(BtlCollChr *chr, s32 slot);  /* recoil damage to the attacker */
extern f32 BtlSuper_GetThrowAngleA(BtlCollChr *chr, s32 slot);
extern f32 BtlSuper_GetThrowAngleB(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetThrowChara(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetThrowCostume(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetThrowGauge20(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetUnk220(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetUnk226(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetLastStep(BtlCollChr *chr, s32 slot);
extern s32 BtlSuper_GetDamage(BtlCollChr *chr, s32 slot, s32 arg2, s32 arg3); /* damage */

/* Strike parameter readers (attacker, slot 0..1). */
extern u32 BtlSkill_GetFlags(BtlCollChr *chr, s32 slot);  /* flag word */
extern s32 BtlSkill_GetHitDirKind(BtlCollChr *chr, s32 slot);
extern s32 BtlSkill_GetReact(BtlCollChr *chr, s32 slot);  /* reaction ids */
extern s32 BtlSkill_GetReactS800(BtlCollChr *chr, s32 slot);
extern s32 BtlSkill_GetHitSound(BtlCollChr *chr, s32 slot);  /* hit sound kind */
extern f32 BtlSkill_GetPush(BtlCollChr *chr, s32 slot);  /* push */
extern f32 BtlSkill_GetGuardPush(BtlCollChr *chr, s32 slot);
extern s32 BtlSkill_GetDamage(BtlCollChr *chr, s32 slot, s32 arg2); /* damage */
extern s32 BtlSkill_GetGuardDamage(BtlCollChr *chr, s32 slot, s32 arg2);
extern s32 BtlSkill_GetFrames(BtlCollChr *chr, s32 slot);  /* stun frames */

/* Starts a throw: fills the attacker's throw block, copies it to the victim and marks both. */
s32 BtlColl_StartThrow(BtlCollChr *atk, BtlCollChr *def, s32 slot, u32 techFlags) {
    s32 pick = 0;
    s32 back;
    f32 yaw;

    if (!TagTeam_CanContact(atk->side, def->side)) return 0;
    if (atk->react.id >= 3) {
        return 0;
    }
    if (BtlChar_IsDead(atk)) {
        return 0;
    }
    if (BtlChar_IsDead(def)) {
        return 0;
    }
    if (!BtlChar_TestFlag(atk, 3)) {
        return 0;
    }
    BtlChar_SetFlag(atk, 0x72);
    atk->thr.tech = BtlSuper_GetId(atk, slot);
    atk->thr.atkSide = atk->side;
    atk->thr.defSide = def->side;
    atk->thr.slot = slot;
    atk->thr.unk10 = BtlSuper_GetStepCount(atk, slot);
    atk->thr.unk20 = BtlSuper_GetThrowChara(atk, slot);
    atk->thr.unk24 = BtlSuper_GetThrowCostume(atk, slot);
    atk->thr.unk28 = BtlSuper_GetThrowGauge20(atk, slot);
    atk->thr.unk2C = BtlSuper_GetUnk226(atk, slot);
    atk->thr.unk18 = BtlSuper_GetUnk220(atk, slot);
    atk->thr.unk1C = BtlSuper_GetLastStep(atk, slot);
    BtlChar_SetFlag(atk, 0x94);
    atk->react.faceYaw = BtlChar_GetPos(atk)->yaw;
    atk->react.unk24 = 0.0f;
    back = 0;
    if (techFlags & 0x10000000) {
        back = 1;
    }
    if (techFlags & 0x400) {
        pick = BtlChar_FrameMod(2) != 0;
    }
    atk->thr.unk34 = pick;
    atk->thr.unk14 = BtlSuper_GetUnk14D(atk, slot);
    if (techFlags & 0x400000) {
        atk->thr.unk30 = 1;
        atk->thr.unk60 = BtlSuper_GetThrowAngleA(atk, slot);
        atk->thr.unk64 = BtlSuper_GetThrowAngleB(atk, slot);
    } else {
        atk->thr.unk30 = 0;
        atk->thr.unk60 = 0.0f;
        atk->thr.unk64 = 0.0f;
    }
    atk->thr.unk40 = 0;
    if (BtlSuper_GetFlagsA(atk, slot) & 0x2000000) {
        atk->thr.unk40 = 1;
    }
    atk->thr.unk44 = 0;
    atk->thr.unk48 = 0;
    if (techFlags & 0x2000) {
        if (BtlMember_GetActiveGauge(atk)->unk20 == 0) {
            atk->thr.unk48 = 1;
        }
    }
    atk->thr.unk4C = 0;
    if (techFlags & 0x80000) {
        if (BtlMember_GetActiveGauge(def)->unk20 == 0) {
            atk->thr.unk4C = 1;
        }
    }
    atk->thr.unk50 = 0;
    if (techFlags & 0x800) {
        atk->thr.unk50 = 1;
    }
    atk->thr.unk54 = 0;
    if (techFlags & 0x10000) {
        atk->thr.unk54 = 1;
    }
    atk->thr.unk58 = 0;
    if (BtlAnim_GetFlags(BtlAnim_GetId(def)) & 0x800) {
        atk->thr.unk58 = 1;
    }
    atk->thr.unk5C = 0;
    if (techFlags & 0x4000000) {
        atk->thr.unk5C = 1;
    }
    def->react.id = BTL_REACT_CAUGHT;
    def->thr.tech = atk->thr.tech;
    def->thr.atkSide = atk->thr.atkSide;
    def->thr.defSide = atk->thr.defSide;
    def->thr.slot = atk->thr.slot;
    def->thr.unk10 = atk->thr.unk10;
    def->thr.unk20 = -1;
    def->thr.unk24 = 0;
    def->thr.unk28 = 0;
    def->thr.unk2C = 1;
    def->thr.unk18 = atk->thr.unk18;
    def->thr.unk1C = atk->thr.unk1C;
    def->thr.unk14 = atk->thr.unk14;
    def->thr.unk30 = atk->thr.unk30;
    def->thr.unk60 = atk->thr.unk60;
    def->thr.unk64 = atk->thr.unk64;
    def->thr.unk34 = atk->thr.unk34;
    def->thr.unk40 = atk->thr.unk40;
    def->thr.unk44 = atk->thr.unk44;
    def->thr.unk48 = atk->thr.unk48;
    def->thr.unk4C = atk->thr.unk4C;
    def->thr.unk50 = atk->thr.unk50;
    def->thr.unk54 = atk->thr.unk54;
    def->thr.unk58 = atk->thr.unk58;
    def->thr.unk5C = atk->thr.unk5C;
    BtlChar_SetFlag(def, 0x94);
    yaw = atk->react.faceYaw;
    def->react.faceYaw = yaw;
    def->react.unk24 = atk->react.unk24;
    def->react.dirYaw = yaw;
    def->react.back = back;
    BtlChar_SetFlag(atk, 0x44);
    BtlChar_SetFlag(atk, 0x45);
    BtlChar_SetFlag(atk, 0x46);
    BtlChar_SetFlag(def, 0x44);
    BtlChar_SetFlag(def, 0x45);
    BtlChar_SetFlag(def, 0x46);
    BtlChar_RaiseFirstClash(atk);
    BtlChar_SetFlag(atk, 0x128);
    switch (slot) {
    case 2:
        BtlEvent_Raise(atk->side, 0x2B);
        break;
    case 3:
        BtlEvent_Raise(atk->side, 0x2C);
        break;
    case 4:
        BtlEvent_Raise(atk->side, 0x2D);
        break;
    }
#ifdef PORT
    {extern void TagCamera_TestContact(s32,s32,s32);TagCamera_TestContact(atk->side,def->side,1);}
#endif
    return 1;
}

/* 1 when `yaw` (the direction a hit travels in) is within 90 degrees of the fighter's model yaw: the hit comes
 * from behind. Guards, deflects, reflects and absorbs all fail in that case. */
s32 BtlColl_IsFacing(BtlCollChr *chr, f32 yaw) {
    if (__builtin_fabsf(BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y - yaw)) < 1.5707963f) {
        return 1;
    }
    return 0;
}

/* Direction a hit pushes the fighter in: returns its yaw and optionally the unit vector. */
f32 BtlColl_GetHitDir(BtlCollChr *chr, BtlCollHit *hit, Vec4 *outDir) {
    Vec4 dir;
    Vec4 p;
    s32 fromPos = 0;
    s32 fromAtk = 0;
    BtlCollChr *atk;
    s32 slot;
    f32 yaw;

    if (hit->unk54 != 0 || Vec3_Length(&hit->vel) < 0.01f) {
        fromPos = 1;
    }
    if (hit->type == BTL_HIT_TECH && hit->owner != NULL) {
        slot = hit->owner->slot;
        atk = BtlChar_Get(hit->owner->side);
        switch (slot) {
        case 0 ... 1:
            fromPos = 0;
            fromAtk = 1;
            break;
        case 2 ... 4:
            if (BtlSuper_GetHitDirKind(atk, slot) == 3) {
                fromPos = 0;
                fromAtk = 1;
            }
            break;
        }
    }
    if (fromPos) {
        BtlCharApi_GetNodePos(chr->objId, 3, &p);
        Vec4_Sub(&dir, &p, &hit->pos);
    } else if (fromAtk) {
        BtlOpp_GetDelta(chr, &dir);
        dir.y += BtlOpp_GetHalfHeightDiff(chr);
        Vec4_Sub(&dir, &D_002EC2A0, &dir);
    } else {
        Vec4_Copy(&dir, &hit->vel);
    }
    if (__builtin_fabsf(dir.x) > 0.001f || __builtin_fabsf(dir.z) > 0.001f) {
        Vec3_Normalize(&dir, &dir);
        yaw = atan2f(dir.x, dir.z);
    } else {
        if (__builtin_fabsf(dir.y) > 0.001f) {
            Vec3_Normalize(&dir, &dir);
        }
        yaw = 0.0f;
    }
    if (outDir != NULL) {
        Vec4_Copy(outDir, &dir);
    }
    return yaw;
}

/* Plays the sound of a technique hit at the hit position and the victim's voice. */
void BtlColl_PlayHitSound(BtlCollChr *chr, BtlCollHit *hit, u32 kind) {
    switch (kind) {
    case 0:
        if (chr != NULL) {
            BtlChar_PlayVoice(chr, 0);
        }
        break;
    case 1:
        if (hit != NULL) {
            BtlCharSnd_RequestAt(&hit->pos, 0, BtlChar_FrameMod(2) ? 0x1B : 0x11, 200.0f, 1500.0f);
        }
        if (chr != NULL) {
            BtlChar_PlayVoice(chr, 0);
        }
        break;
    case 2:
        if (hit != NULL) {
            BtlCharSnd_RequestAt(&hit->pos, 0, 0x47, 200.0f, 1500.0f);
        }
        if (chr != NULL) {
            BtlChar_PlayVoice(chr, 1);
        }
        break;
    case 3:
        if (hit != NULL) {
            BtlCharSnd_RequestAt(&hit->pos, 0, 0x4A, 200.0f, 1500.0f);
        }
        if (chr != NULL) {
            BtlChar_PlayVoice(chr, 3);
        }
        break;
    case 4:
        if (hit != NULL) {
            BtlCharSnd_RequestAt(&hit->pos, 0, 0x12, 200.0f, 1500.0f);
        }
        if (chr != NULL) {
            BtlChar_PlayVoice(chr, 3);
        }
        break;
    }
}

/* A blast hits a fighter that is guarding (guard kind 1..4) from the front: chip damage (BtlMember_Damage flag 0xC),
 * flag 0x66 and a push; no reaction. Returns 0 when the guard does not apply and the hit goes through. */
s32 BtlColl_GuardBlast(BtlCollChr *chr, BtlCollHit *hit) {
    Vec4 dir;
    s32 kind;
    f32 yaw;
    s32 dmg;
    f32 time;

    if (BtlChar_TestFlag(chr, 0x44)) {
        return 0;
    }
    kind = BtlColl_GetGuardKind(chr);
    yaw = BtlColl_GetHitDir(chr, hit, &dir);
    if (kind != 6) {
        if (BtlKiBlast_GetFlagsOfHit(hit) & 1) {
            return 0;
        }
        if (BtlColl_IsFacing(chr, yaw)) {
            return 0;
        }
    }
    dmg = BtlKiBlast_GetGuardDamageOfHit(hit);
    if (hit->atk->hits >= 2) {
        dmg /= hit->atk->hits;
    }
    if (kind == 0 || kind == 5) {
        return 0;
    }
    BtlChar_SetFlag(chr, 0x66);
    TagBalance_Damage(chr, dmg, 0xC, hit->ownerId);
    switch (BtlKiBlast_GetTypeOfHit(hit)) {
    case 2:
        BtlCharSnd_RequestAt(&hit->pos, 0, 0x42, 200.0f, 1500.0f);
        break;
    case 3:
        BtlCharSnd_RequestAt(&hit->pos, 0, BtlChar_FrameMod(2) ? 0xC : 0xD, 200.0f, 1500.0f);
        break;
    default:
        BtlCharSnd_RequestAt(&hit->pos, 0, BtlChar_FrameMod(2) ? 0x1B : 0x11, 200.0f, 1500.0f);
        break;
    }
    time = 0.1f;
    BtlMove_SetImpulseDir(chr, &dir, BtlKiBlast_GetGuardPushOfHit(hit));
    ChrCam_AddShake(chr, 3.0f, time);
    BtlChar_Vibrate(chr, 1.0f, time);
    return 1;
}

/* A strike (slot 0, 1) hits a guarding fighter: same as BtlColl_GuardBlast, unless the strike has flag 0x200000. */
s32 BtlColl_GuardStrike(BtlCollChr *chr, BtlCollHit *hit) {
    Vec4 dir;
    s32 kind;
    BtlCollChr *atk;
    s32 slot;
    s32 dmg;
    f32 yaw;
    f32 time;

    if (BtlChar_TestFlag(chr, 0x45)) {
        return 0;
    }
    if (hit->owner == NULL) {
        return 0;
    }
    kind = BtlColl_GetGuardKind(chr);
    yaw = BtlColl_GetHitDir(chr, hit, &dir);
    if (kind != 6) {
        if (BtlColl_IsFacing(chr, yaw)) {
            return 0;
        }
    }
    slot = hit->owner->slot;
    atk = BtlChar_Get(hit->owner->side);
    if (BtlSkill_GetFlags(atk, slot) & 0x200000) {
        return 0;
    }
    dmg = BtlSkill_GetGuardDamage(atk, slot, 0);
    if (kind == 0 || kind == 5) {
        return 0;
    }
    BtlChar_SetFlag(chr, 0x66);
    TagBalance_Damage(chr, dmg, 0xC, hit->ownerId);
    time = 0.2f;
    BtlColl_PlayHitSound(NULL, hit, BtlSkill_GetHitSound(atk, slot));
    BtlMove_SetImpulseDir(chr, &dir, BtlSkill_GetGuardPush(atk, slot));
    ChrCam_AddShake(chr, 3.0f, time);
    BtlChar_Vibrate(chr, 1.0f, time);
    return 1;
}

/* A rush attack (slot 2..4) hits a guarding fighter. Catch reactions (0x1D, 0x1E, 0x22) are swallowed with only a
 * sound when BtlParam_GetUnk2 is 4 and the technique lacks bit 0x8000000; unblockable techniques (bit 1) go through. */
s32 BtlColl_GuardRush(BtlCollChr *chr, BtlCollHit *hit) {
    Vec4 dir;
    s32 weak = 0;
    BtlCollChr *atk;
    s32 slot;
    s32 kind;
    s32 alt;
    s32 n;
    f32 yaw;
    f32 time;

    if (BtlChar_TestFlag(chr, 0x46)) {
        return 0;
    }
    if (hit->owner == NULL) {
        return 0;
    }
    slot = hit->owner->slot;
    atk = BtlChar_Get(hit->owner->side);
    kind = BtlColl_GetGuardKind(chr);
    alt = (hit->flags >> 4) & 1;
    switch (BtlSuper_GetReact(atk, slot)) {
    case 0x1D:
    case 0x1E:
    case 0x22:
        if (BtlParam_GetUnk2(chr) == 4 && !(BtlSuper_GetFlags(atk, slot) & 0x8000000)) {
            BtlColl_PlayHitSound(NULL, hit, BtlSuper_GetHitSound(atk, slot, alt));
            return 1;
        }
        weak = 1;
        break;
    }
    if (BtlSuper_GetId(atk, slot) == 0x2D5) {
        if (BtlParam_GetUnkC7Scale(chr) < 0.001f) {
            BtlColl_PlayHitSound(NULL, hit, BtlSuper_GetHitSound(atk, slot, alt));
            return 1;
        }
    }
    yaw = BtlColl_GetHitDir(chr, hit, &dir);
    if (kind != 6) {
        if (BtlColl_IsFacing(chr, yaw)) {
            return 0;
        }
    }
    if (BtlSuper_GetFlags(atk, slot) & 1) {
        if (weak || kind != 6) {
            return 0;
        }
    }
    if ((BtlSuper_GetType(atk, slot) & 2) && atk->unkE44 >= 0.9999f && (BtlSuper_GetFlags(atk, slot) & 2)) {
        if (kind != 6) {
            return 0;
        }
    }
    if (kind == 0 || kind == 5) {
        return 0;
    }
    switch (BtlSuper_GetGuardKind(atk, slot)) {
    case 0:
        BtlChar_SetFlag(chr, 0x66);
        break;
    case 1:
        BtlChar_SetFlag(chr, 0x67);
        break;
    }
    TagBalance_Damage(chr, BtlSuper_GetDamage(atk, slot, 1, 0), 0xC, hit->ownerId);
    BtlColl_PlayHitSound(NULL, hit, BtlSuper_GetHitSound(atk, slot, alt));
    if (alt) {
        BtlMove_SetImpulseDir(chr, &dir, BtlSuper_GetGuardPush(atk, slot));
    }
    time = 0.3f;
    ChrCam_AddShake(chr, 5.0f, time);
    BtlChar_Vibrate(chr, 1.0f, time);
    n = BtlSuper_GetRecoilDamage(atk, slot);
    if (n > 0) {
        if (BtlMember_Damage(atk, n, 0x42B)) {
            BtlChar_SetFlag(atk, 0x95);
        }
    }
    switch (slot) {
    case 2:
        BtlEvent_Raise(chr->side, 0x2E);
        break;
    case 3:
        BtlEvent_Raise(chr->side, 0x2F);
        break;
    case 4:
        BtlEvent_Raise(chr->side, 0x30);
        break;
    }
    return 1;
}

/* A blast hits a fighter: chooses "no flinch" (reaction 2) or a reaction from the blast's tables, then applies the
 * push, sound, damage, shake and rumble. Returns 1 when the record was consumed. */
s32 BtlColl_HitByBlast(BtlCollChr *chr, BtlCollHit *hit) {
    Vec4 dir;
    BtlCollObj *obj = BtlChar_GetObj(chr);
    u32 stFlags;
    BtlCollChr *atk;
    f32 yaw;
    s32 front;
    s32 react;
    s32 n;
    s32 dmg;
    f32 time;

    if (hit->atk == NULL) {
        return 0;
    }
    stFlags = BtlAnim_GetFlags(BtlAnim_GetId(chr));
    if (BtlChar_TestFlag(chr, 0x44)) {
        return 0;
    }
    if (BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr))) {
        return 1;
    }
    atk = BtlChar_Get(hit->atk->side);
    yaw = BtlColl_GetHitDir(chr, hit, &dir);
    front = BtlColl_IsFacing(chr, yaw);
    if (chr->noFlinch > 0) {
        react = BTL_REACT_NOFLINCH;
    } else if ((stFlags & 0x4000) || BtlAct_GetCurrent(chr) == 0xDF) {
        react = BtlKiBlast_GetReactS4000OfHit(hit);
    } else if (stFlags & 0x800) {
        react = BtlKiBlast_GetReactS800OfHit(hit);
    } else if (stFlags & 0x200) {
        react = BtlKiBlast_GetReactS200OfHit(hit);
    } else {
        n = 0;
        if (BtlParam_GetFlags(chr) & 4) {
            n = 1;
        }
        if (BtlParam_GetFlags(chr) & 8) {
            n--;
        }
        if (BtlParam_GetFlags(atk) & 4) {
            n--;
        }
        if (BtlParam_GetFlags(atk) & 8) {
            n++;
        }
        if (BtlAct_TestPoweredSkill(chr, 0x40)) {
            n++;
        }
        if (chr->unkE14 > 0) {
            n++;
        }
        if (BtlMember_HasAbility(chr, 0x45)) {
            n++;
        }
        if (BtlAct_TestPoweredSkill(atk, 0x100)) {
            n--;
        }
        if (BtlMember_HasAbility(atk, 0x47)) {
            n--;
        } else if (BtlMember_HasAbility(atk, 0x70)) {
            if (BtlChar_TestFlag(atk, 6)) {
                n--;
            }
        }
        if (n > 0 && (BtlKiBlast_GetFlagsOfHit(hit) & 0x10)) {
            react = BTL_REACT_NOFLINCH;
        } else if ((stFlags & 0x100) && (BtlKiBlast_GetFlagsOfHit(hit) & 0x10)) {
            if (BtlObjAnim_QueryEvent(obj, 1, 0, 3) == 0) {
                react = BTL_REACT_NOFLINCH;
            } else if (obj->unkCAD != ~obj->unkCAC) {
                react = BTL_REACT_NOFLINCH;
            } else {
                react = BtlKiBlast_GetReactOfHit(hit);
            }
        } else {
            react = BtlKiBlast_GetReactOfHit(hit);
        }
    }
    if (react == BTL_REACT_NOFLINCH) {
        BtlCollReact *g = &chr->react;

        g->id = react;
        g->unk38 = 3;
    } else {
        BtlCollReact *r = &chr->react;

        r->id = react;
        r->unk18 = 0;
        chr->react.faceYaw = 0.0f;
        chr->react.unk24 = 0.0f;
        r->dirYaw = yaw;
        r->back = front;
        BtlMove_SetImpulseDir(chr, &dir, BtlKiBlast_GetPushOfHit(hit));
        BtlChar_PlayVoice(chr, 0);
    }
    BtlChar_SetFxBit(chr, 0x1D);
    if (BtlKiBlast_GetTypeOfHit(hit) == 2) {
        BtlCharSnd_RequestAt(&hit->pos, 0, 0x42, 200.0f, 1500.0f);
    } else {
        BtlCharSnd_RequestAt(&hit->pos, 0, BtlChar_FrameMod(2) ? 0x1B : 0x11, 200.0f, 1500.0f);
    }
    dmg = BtlKiBlast_GetDamageOfHit(hit);
    if (hit->atk->hits >= 2) {
        dmg /= hit->atk->hits;
    }
    time = 0.1f;
    TagBalance_Damage(chr, dmg, 0, hit->ownerId);
    ChrCam_AddShake(chr, 3.0f, time);
    BtlChar_Vibrate(chr, 1.0f, time);
    switch (hit->atk->kind) {
    case 1 ... 3:
        BtlEvent_Raise(hit->atk->side, 0x36);
        break;
    }
    if (!BtlChar_TestFlag(chr, 0x93)) {
        BtlChar_SetHeldFlag(chr, 5);
    }
    return 1;
}

/* A strike (slot 0, 1) hits a fighter. */
s32 BtlColl_HitByStrike(BtlCollChr *chr, BtlCollHit *hit) {
    Vec4 dir;
    f32 yaw;
    s32 front;
    BtlCollChr *atk;
    s32 slot;
    s32 react;
    BtlCollReact *r;
    s32 fl;
    s32 dmg;
    f32 time;

    if (BtlChar_TestFlag(chr, 0x45)) {
        return 0;
    }
    if (hit->owner == NULL) {
        return 0;
    }
    yaw = BtlColl_GetHitDir(chr, hit, &dir);
    front = BtlColl_IsFacing(chr, yaw);
    slot = hit->owner->slot;
    atk = BtlChar_Get(hit->owner->side);
    if (BtlSkill_GetHitDirKind(atk, slot) != 4) {
        if (__builtin_fabsf(BtlOpp_GetYawFromFacing(atk)) > 1.5707963f) {
            return 0;
        }
    }
    if (BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x800) {
        react = BtlSkill_GetReactS800(atk, slot);
    } else {
        react = BtlSkill_GetReact(atk, slot);
    }
    if (react == 0) {
        react = 3;
    }
    if (react == 1) {
        return 0;
    }
    r = &chr->react;
    if (chr->noFlinch > 0) {
        react = BTL_REACT_NOFLINCH;
    }
    r->dirYaw = yaw;
    r->back = front;
    r->id = react;
    if (react != BTL_REACT_NOFLINCH) {
        r->unk18 = 0;
        chr->react.faceYaw = 0.0f;
        chr->react.unk24 = 0.0f;
        BtlChar_SetFlag(chr, 0x94);
        if (front) {
            chr->react.faceYaw = yaw;
            chr->react.unk24 = 0.0f;
        } else {
            chr->react.faceYaw = BtlUtil_WrapAngle(yaw + 3.1415927f);
            chr->react.unk24 = 0.0f;
        }
        BtlMove_SetImpulseDir(chr, &dir, BtlSkill_GetPush(atk, slot));
        switch (react) {
        case 0x17 ... 0x1C:
            chr->react.stun = BtlSkill_GetFrames(atk, slot);
            if (BtlMember_HasAbility(chr, 0x3A)) {
                chr->react.stun /= 2;
            }
            if (BtlMember_HasAbility(atk, 0x3B)) {
                chr->react.stun += chr->react.stun / 2;
            }
            break;
        }
    }
    BtlChar_SetFxBit(chr, 0x1D);
    fl = 0;
    BtlColl_PlayHitSound(chr, hit, BtlSkill_GetHitSound(atk, slot));
    dmg = BtlSkill_GetDamage(atk, slot, 0);
    switch (slot) {
    case 0:
        fl = 0x200000;
        break;
    case 1:
        fl = 0x400000;
        break;
    }
    TagBalance_Damage(chr, dmg, fl, hit->ownerId);
    ChrCam_AddShake(chr, 3.0f, 0.2f);
    BtlChar_Vibrate(chr, 1.0f, 0.2f);
    switch (slot) {
    case 0:
        BtlEvent_Raise(atk->side, 0x29);
        break;
    case 1:
        BtlEvent_Raise(atk->side, 0x2A);
        break;
    }
    return 1;
}

/* A rush attack lands: reaction, facing, damage, shake, recoil, and the stage timer. */
void BtlColl_ApplyRushHit(BtlCollChr *chr, BtlCollChr *atk, BtlCollHit *hit, s32 react, f32 yaw, s32 front, Vec4 *dir) {
    BtlCollReact *r = &chr->react;
    s32 slot = hit->owner->slot;
    s32 alt;
    s32 fl;
    s32 dmg;
    s32 n;

    r->id = react;
    r->unk18 = 0;
    r->slot = slot;
    alt = (hit->flags >> 4) & 1;
    BtlChar_SetFlag(chr, 0x94);
    if (front) {
        chr->react.faceYaw = yaw;
        chr->react.unk24 = 0.0f;
    } else {
        chr->react.faceYaw = BtlUtil_WrapAngle(yaw + 3.1415927f);
        chr->react.unk24 = 0.0f;
    }
    r->dirYaw = yaw;
    r->back = front;
    if (alt) {
        BtlMove_SetImpulseDir(chr, dir, BtlSuper_GetPush(atk, slot));
    }
    BtlChar_SetFxBit(chr, 0x1D);
    BtlColl_PlayHitSound(chr, hit, BtlSuper_GetHitSound(atk, slot, alt));
    fl = 0x100;
    dmg = BtlSuper_GetDamage(atk, slot, 0, 0);
    switch (slot) {
    case 2:
        fl = 0x800100;
        break;
    case 3:
        fl = 0x1000100;
        break;
    case 4:
        fl = 0x2000100;
        break;
    }
    TagBalance_Damage(chr, dmg, fl, hit->ownerId);
    BtlCharApi_ShakeCamsNear(&hit->pos, 100.0f, 1500.0f, 5.0f, 0.3f);
    BtlCharApi_RumbleNear(&hit->pos, 100.0f, 1000.0f, 1.0f, 0.3f);
    BtlChar_SetHeldFlag(chr, 0x1D);
    BtlChar_SetHeldFlag(chr, 0x1E);
    if (BtlSuper_GetType(atk, slot) == 7) {
        BtlChar_SetFlag(atk, 0x73);
    }
    if (BtlSuper_GetType(atk, slot) == 8) {
        n = BtlSuper_GetRecoilDamage(atk, slot);
        if (n > 0) {
            if (BtlMember_Damage(atk, n, 0x42B)) {
                BtlChar_SetFlag(atk, 0x95);
            }
        }
    }
    chr->unk106C = 0;
    switch (slot) {
    case 2:
    case 3:
        BtlChar_AddStageTimer(2.0f);
        break;
    case 4:
        BtlChar_AddStageTimer(5.0f);
        break;
    }
    if (slot == 4) {
        if (!BtlChar_TestFlag(chr, 0x13) && react != 0x22) {
            BtlChar_ClearFlag(chr, 5);
        }
    } else {
        if (!BtlChar_TestFlag(chr, 0x93)) {
            BtlChar_SetHeldFlag(chr, 5);
        }
    }
}

/* A rush attack lands on a fighter that does not flinch (reaction 2): damage, recoil, shake; no facing change. */
void BtlColl_ApplyRushNoFlinch(BtlCollChr *chr, BtlCollChr *atk, BtlCollHit *hit, f32 yaw, s32 front) {
    s32 fl = 0x100;
    BtlCollReact *r = &chr->react;
    s32 slot = hit->owner->slot;
    s32 alt;
    s32 dmg;
    s32 n;

    r->back = front;
    r->dirYaw = yaw;
    r->id = BTL_REACT_NOFLINCH;
    alt = (hit->flags >> 4) & 1;
    BtlChar_SetFxBit(chr, 0x1D);
    BtlColl_PlayHitSound(chr, hit, BtlSuper_GetHitSound(atk, slot, alt));
    dmg = BtlSuper_GetDamage(atk, slot, 0, 0);
    switch (slot) {
    case 2:
        fl = 0x800100;
        break;
    case 3:
        fl = 0x1000100;
        break;
    case 4:
        fl = 0x2000100;
        break;
    }
    TagBalance_Damage(chr, dmg, fl, hit->ownerId);
    n = BtlSuper_GetRecoilDamage(atk, slot);
    if (n > 0) {
        if (BtlMember_Damage(atk, n, 0x42B)) {
            BtlChar_SetFlag(atk, 0x95);
        }
    }
    BtlCharApi_ShakeCamsNear(&hit->pos, 100.0f, 1500.0f, 5.0f, 0.3f);
    BtlCharApi_RumbleNear(&hit->pos, 100.0f, 1000.0f, 1.0f, 0.3f);
}

/* A rush attack with a catch reaction (0x1D, 0x1E) lands: fills the throw block of both fighters. */
s32 BtlColl_StartRushCatch(BtlCollChr *chr, BtlCollChr *atk, BtlCollHit *hit, s32 react, f32 yaw) {
    BtlCollReact *r = &chr->react;
    s32 slot = hit->owner->slot;
    s32 back;
    u32 tf;
    s32 alt;

    if (!TagTeam_CanContact(atk->side, chr->side)) return 0;
    if (atk->react.id >= 3) {
        return 0;
    }
    if (BtlChar_IsDead(atk)) {
        return 0;
    }
    if (BtlChar_IsDead(chr)) {
        return 0;
    }
    if (!BtlChar_TestFlag(atk, 3)) {
        return 0;
    }
    alt = (hit->flags >> 4) & 1;
    r->id = react;
    r->unk18 = 0;
    chr->react.faceYaw = 0.0f;
    chr->react.unk24 = 0.0f;
    atk->thr.tech = BtlSuper_GetId(atk, slot);
    atk->thr.atkSide = atk->side;
    atk->thr.defSide = chr->side;
    atk->thr.slot = slot;
    atk->thr.unk14 = BtlSuper_GetUnk14D(atk, slot);
    tf = BtlSuper_GetFlags(atk, slot);
    back = 0;
    if (tf & 0x10000000) {
        back = 1;
    }
    if (tf & 0x400000) {
        atk->thr.unk30 = 1;
        atk->thr.unk60 = BtlSuper_GetThrowAngleA(atk, slot);
        atk->thr.unk64 = BtlSuper_GetThrowAngleB(atk, slot);
    } else {
        atk->thr.unk30 = 0;
        atk->thr.unk60 = 0.0f;
        atk->thr.unk64 = 0.0f;
    }
    if (tf & 0x100000) {
        atk->thr.unk38 = 1;
    } else {
        atk->thr.unk38 = 0;
    }
    if (tf & 0x800000) {
        atk->thr.unk3C = 1;
    } else {
        atk->thr.unk3C = 0;
    }
    atk->thr.unk44 = 0;
    if (react == 0x1E) {
        atk->thr.unk44 = 1;
    }
    atk->thr.unk54 = 0;
    if (tf & 0x10000) {
        atk->thr.unk54 = 1;
    }
    atk->thr.unk58 = 0;
    if (BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x800) {
        atk->thr.unk58 = 1;
    }
    atk->thr.unk48 = 0;
    atk->thr.unk18 = -1;
    atk->thr.unk1C = -1;
    atk->thr.unk4C = 0;
    atk->thr.unk50 = 0;
    atk->thr.unk5C = 0;
    BtlChar_SetFlag(atk, 0x73);
    chr->thr.tech = atk->thr.tech;
    chr->thr.atkSide = atk->thr.atkSide;
    chr->thr.defSide = atk->thr.defSide;
    chr->thr.slot = atk->thr.slot;
    chr->thr.unk14 = atk->thr.unk14;
    chr->thr.unk30 = atk->thr.unk30;
    chr->thr.unk38 = atk->thr.unk38;
    chr->thr.unk3C = atk->thr.unk3C;
    chr->thr.unk60 = atk->thr.unk60;
    chr->thr.unk64 = atk->thr.unk64;
    chr->react.dirYaw = yaw;
    chr->react.back = back;
    chr->thr.unk44 = atk->thr.unk44;
    chr->thr.unk48 = atk->thr.unk48;
    chr->thr.unk4C = atk->thr.unk4C;
    chr->thr.unk18 = atk->thr.unk18;
    chr->thr.unk1C = atk->thr.unk1C;
    chr->thr.unk50 = atk->thr.unk50;
    chr->thr.unk54 = atk->thr.unk54;
    chr->thr.unk58 = atk->thr.unk58;
    chr->thr.unk5C = atk->thr.unk5C;
    BtlColl_PlayHitSound(NULL, hit, BtlSuper_GetHitSound(atk, slot, alt));
#ifdef PORT
    {extern void TagCamera_TestContact(s32,s32,s32);TagCamera_TestContact(atk->side,chr->side,2);}
#endif
    return 1;
}

/* A rush attack (slot 2..4) hits a fighter: no-flinch, catch, throw or a plain hit. */
s32 BtlColl_HitByRush(BtlCollChr *chr, BtlCollHit *hit) {
    Vec4 dir;
    f32 yaw;
    s32 front;
    BtlCollChr *atk;
    s32 slot;
    s32 react;

    if (BtlChar_TestFlag(chr, 0x46)) {
        return 0;
    }
    if (hit->owner == NULL) {
        return 0;
    }
    yaw = BtlColl_GetHitDir(chr, hit, &dir);
    front = BtlColl_IsFacing(chr, yaw);
    slot = hit->owner->slot;
    atk = BtlChar_Get(hit->owner->side);
    switch (BtlSuper_GetHitDirKind(atk, slot)) {
    case 0:
    case 2:
        if (__builtin_fabsf(BtlOpp_GetYawFromFacing(atk)) > 1.5707963f) {
            return 0;
        }
        break;
    }
    if (BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x800) {
        if (hit->flags & 0x10) {
            react = BtlSuper_GetReactAltS800(atk, slot);
        } else {
            react = BtlSuper_GetReactS800(atk, slot);
        }
    } else {
        if (hit->flags & 0x10) {
            react = BtlSuper_GetReactAlt(atk, slot);
        } else {
            react = BtlSuper_GetReact(atk, slot);
        }
    }
    if (react == 0) {
        react = 0xF;
    }
    if (react == 1) {
        return 0;
    }
    if (react == BTL_REACT_NOFLINCH) {
        BtlColl_ApplyRushNoFlinch(chr, atk, hit, yaw, front);
    } else if (react == 0x1D || react == 0x1E) {
        if (!BtlColl_StartRushCatch(chr, atk, hit, react, yaw)) {
            return 0;
        }
    } else if (react == BTL_REACT_CAUGHT) {
        if (!BtlColl_StartThrow(atk, chr, slot, BtlSuper_GetFlags(atk, slot))) {
            return 0;
        }
    } else if (chr->noFlinch > 0) {
        BtlColl_ApplyRushNoFlinch(chr, atk, hit, yaw, front);
    } else {
        BtlColl_ApplyRushHit(chr, atk, hit, react, yaw, front, &dir);
    }
    switch (slot) {
    case 2:
        BtlEvent_Raise(atk->side, 0x2B);
        break;
    case 3:
        BtlEvent_Raise(atk->side, 0x2C);
        break;
    case 4:
        BtlEvent_Raise(atk->side, 0x2D);
        break;
    }
    return 1;
}

/* Asks the fighter whether a hit about to land is avoided: invulnerability, or an automatic dodge. */
s32 BtlColl_TryDodge(s32 objId, BtlCollHit *hit) {
    BtlCollChr *chr = BtlChar_FindByObjId(objId);
    BtlCollChr *atk;
    s32 slot;
    s32 canDodge;
    s32 rush;
    s32 ok;
    s32 free;
    s32 used;
    s32 side;

    if (chr == NULL) {
        return 0;
    }
    if (!TagTeam_CanContact(hit->ownerId, chr->side)) return 1;
    if (BtlChar_IsFrozen(chr)) {
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0xAA)) {
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x5A)) {
        return 0;
    }
    if (BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr))) {
        slot = BtlAct_GetCurrentClass(chr);
        if (slot >= 2 && slot < 5) {
            if (BtlSuper_GetType(chr, slot) == 8) {
                if (BtlSuper_GetUnk1E(chr, slot) >= hit->unk8) {
                    return 1;
                }
            }
        }
    }
    if (hit->type == BTL_HIT_BLAST) {
        return BtlChar_TestFlag(chr, 0x44) != 0;
    }
    canDodge = 0;
    rush = 0;
    if (hit->owner == NULL) {
        return 0;
    }
    slot = hit->owner->slot;
    atk = BtlChar_Get(hit->owner->side);
    if (hit->owner != NULL) {
        switch (slot) {
        case 0 ... 1:
            if (BtlChar_TestFlag(chr, 0x45)) {
                return 1;
            }
            if (BtlSkill_GetFlags(atk, slot) & 0x400000) {
                canDodge = 1;
            }
            break;
        case 2 ... 4:
            if (BtlChar_TestFlag(chr, 0x46)) {
                return 1;
            }
            canDodge = 1;
            rush = 1;
            break;
        }
    }
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if (!(BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0xB1) || EftHit_GetHitCount(hit)) {
        return 0;
    }
    if (canDodge) {
    ok = 0;
    free = 0;
    used = 0;
    if (chr->unk106C > 0) {
        ok = 1;
        free = 1;
    } else {
        if (chr->dodgeA > 0) {
            chr->dodgeA--;
            ok = 1;
            used = 1;
        }
        if (chr->dodgeB > 0) {
            chr->dodgeB--;
            ok = 1;
            used = 1;
        }
        if (BtlChar_TestFlag(chr, 0x56)) {
            ok = 1;
            used = 1;
        }
        if (BtlChar_TestFlag(chr, 0x57)) {
            ok = 1;
        }
    }
    if (ok) {
    side = BtlChar_FrameMod(2);
    if (BtlInput_IsHeld(chr, 0x40)) {
        side = 0;
    }
    if (BtlInput_IsHeld(chr, 0x80)) {
        side = 1;
    }
    switch (side) {
    case 0:
        BtlChar_SetFlag(chr, 0x74);
        break;
    case 1:
        BtlChar_SetFlag(chr, 0x75);
        break;
    }
    BtlChar_PlayVoice(chr, 0x1F);
    if (rush) {
        if (!BtlChar_TestFlag(chr, 0x80)) {
            if (free) {
                if (BtlMember_HasAbility(chr, 0x22)) {
                    BtlMember_AddBlast(chr, 100000);
                }
            }
        }
        BtlChar_SetHeldFlag(chr, 0x80);
    }
    if (used) {
        switch (chr->unkE0C) {
        case 0:
            BtlChar_SetFlag(chr, 0xDE);
            break;
        case 1:
            BtlChar_SetFlag(chr, 0xDF);
            break;
        }
    }
        return 1;
    }
    }
    return 0;
}

/* Asks the fighter whether it knocks a blast away (deflect); sets the direction the blast leaves in. */
s32 BtlColl_TryDeflect(s32 objId, BtlCollHit *hit) {
    BtlCollChr *chr;
    s32 perfect = 0;
    Vec4 *vel;
    f32 len;

    chr = BtlChar_FindByObjId(objId);
    if (chr == NULL) {
        return 0;
    }
    if (hit->type != BTL_HIT_BLAST) {
        return 0;
    }
    if (hit->unk54 == 1) {
        return 0;
    }
    if (hit->atk == NULL) {
        return 0;
    }
    if (BtlKiBlast_GetFlagsOfHit(hit) & 2) {
        return 0;
    }
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if (!BtlChar_TestFlag(chr, 0x52) && !BtlChar_TestFlag(chr, 0x54) && BtlAct_TestPoweredSkill(chr, 0x80)) {
        switch (BtlKiBlast_GetTypeOfHit(hit)) {
        case 2 ... 3:
            BtlCharSnd_PlayCommon(chr, BtlChar_FrameMod(2) ? 0xC : 0xD);
            break;
        default:
            BtlCharSnd_PlayCommon(chr, 0x13);
            break;
        }
        return 1;
    }
    if (BtlColl_IsFacing(chr, BtlColl_GetHitDir(chr, hit, NULL))) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0x41)) {
        if (chr->unk1074 > 0) {
            if (!(BtlParam_GetFlags(chr) & 2)) {
                perfect = 1;
            }
        }
    }
    if (!perfect) {
        switch (hit->atk->kind) {
        case 0:
        case 4:
        case 8:
            if (!BtlChar_TestFlag(chr, 0x50)) {
                return 0;
            }
            break;
        default:
            if (!BtlChar_TestFlag(chr, 0x51)) {
                return 0;
            }
            break;
        }
    }
    switch (BtlKiBlast_GetTypeOfHit(hit)) {
    case 2 ... 3:
        BtlCharSnd_PlayCommon(chr, BtlChar_FrameMod(2) ? 0xC : 0xD);
        break;
    default:
        BtlCharSnd_PlayCommon(chr, 0x13);
        break;
    }
    BtlChar_PlayVoice(chr, 0xB);
    BtlMember_AddBlast(chr, 5000);
    if (perfect) {
        BtlChar_SetFlag(chr, 0x6B);
    }
    vel = &hit->vel;
    len = Vec3_Length(vel);
    if (len < 0.001f) {
        Vec4_Copy(&chr->deflectDir, &BtlChar_GetPos(chr)->unk80);
    } else {
        Vec4_Scale(&chr->deflectDir, vel, -1.0f / len);
    }
    BtlChar_SetFxBit(chr, 0x2E);
    BtlEvent_Raise(chr->side, 0x58);
    return 1;
}

/* Asks the fighter whether it sends a blast back (flag 0x52); sets the direction the blast leaves in. */
s32 BtlColl_TryReflect(s32 objId, BtlCollHit *hit) {
    BtlCollChr *chr;
    Vec4 *vel;
    f32 len;

    chr = BtlChar_FindByObjId(objId);
    if (chr == NULL) {
        return 0;
    }
    if (hit->type != BTL_HIT_BLAST) {
        return 0;
    }
    if (hit->unk54 == 1) {
        return 0;
    }
    if (hit->atk == NULL) {
        return 0;
    }
    if (BtlKiBlast_GetFlagsOfHit(hit) & 2) {
        return 0;
    }
    if (!BtlChar_TestFlag(chr, 0x52)) {
        return 0;
    }
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if (BtlColl_IsFacing(chr, BtlColl_GetHitDir(chr, hit, NULL))) {
        return 0;
    }
    switch (BtlKiBlast_GetTypeOfHit(hit)) {
    case 2 ... 3:
        BtlCharSnd_PlayCommon(chr, BtlChar_FrameMod(2) ? 0xC : 0xD);
        break;
    default:
        BtlCharSnd_PlayCommon(chr, 0x14);
        break;
    }
    BtlChar_PlayVoice(chr, 0xB);
    vel = &hit->vel;
    len = Vec3_Length(vel);
    if (len < 0.001f) {
        Vec4_Copy(&chr->deflectDir, &BtlChar_GetPos(chr)->unk80);
    } else {
        Vec4_Scale(&chr->deflectDir, vel, -1.0f / len);
    }
    BtlChar_SetFxBit(chr, 0x2E);
    BtlEvent_Raise(chr->side, 0x58);
    return 1;
}

/* Asks the fighter whether it absorbs a hit (flag 0x54): the hit's energy is added to a gauge. */
s32 BtlColl_TryAbsorb(s32 objId, BtlCollHit *hit) {
    BtlCollChr *chr;
    BtlCollChr *atk;
    s32 slot;
    s32 amount = 0;
    s32 alt = 0;
    s32 n;

    chr = BtlChar_FindByObjId(objId);
    if (chr == NULL) {
        return 0;
    }
    if (hit->unk54 == 1) {
        return 0;
    }
    if (!BtlChar_TestFlag(chr, 0x54)) {
        if (BtlChar_TestFlag(chr, 0x41)) {
            if (chr->unk1074 > 0) {
                if (BtlParam_GetFlags(chr) & 2) {
                    alt = hit->type == BTL_HIT_BLAST;
                }
            }
        }
        if (!alt) {
            return 0;
        }
    }
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if (BtlColl_IsFacing(chr, BtlColl_GetHitDir(chr, hit, NULL))) {
        return 0;
    }
    if (hit->type == BTL_HIT_BLAST) {
        if (BtlKiBlast_GetFlagsOfHit(hit) & 2) {
            return 0;
        }
        switch (BtlKiBlast_GetTypeOfHit(hit)) {
        case 2 ... 3:
            return 0;
        }
        amount = BtlKiBlast_GetKiCostOfHit(hit) * 3;
    } else {
        if (hit->owner == NULL) {
            return 0;
        }
        slot = hit->owner->slot;
        atk = BtlChar_Get(hit->owner->side);
        switch (slot) {
        case 0 ... 1:
            return 0;
        case 2 ... 4:
            if (!(BtlSuper_GetFlags(atk, slot) & 4)) {
                return 0;
            }
            amount = BtlSuper_GetKiCost(atk, slot);
            n = BtlSuper_GetShots(atk, slot);
            n *= BtlSuper_GetHitsB(atk, slot);
            if (n >= 2) {
                amount /= n;
            }
            break;
        }
    }
    BtlCharSnd_PlayCommon(chr, 0x37);
    if (BtlMember_AddKi(chr, amount) > 0.0f) {
        if (!BtlChar_TestFlag(chr, 6)) {
            BtlMember_AddMaxPower(chr, amount * 30000 / 100000);
        }
    }
    BtlChar_SetFxBit(chr, 0x37);
    if (alt) {
        BtlChar_SetFlag(chr, 0x6B);
    }
    return 1;
}

/* First attempt of the hit detection for a landed hit: the guard. Returns 1 when the hit was guarded (or the
 * fighter has flag 0x59), 0 when BtlColl_Hit must handle it. */
s32 BtlColl_TryGuard(s32 objId, BtlCollHit *hit) {
    BtlCollChr *chr;
    s32 ret = 0;

    chr = BtlChar_FindByObjId(objId);
    if (chr == NULL) {
        return 0;
    }
    if (!TagTeam_CanContact(hit->ownerId, chr->side)) return 1;
    if (BtlChar_TestFlag(chr, 0x59)) {
        return 1;
    }
    if (hit->type == BTL_HIT_BLAST) {
        ret = BtlColl_GuardBlast(chr, hit);
    } else if (hit->owner != NULL) {
        switch (hit->owner->slot) {
        case 0 ... 1:
            ret = BtlColl_GuardStrike(chr, hit);
            break;
        case 2 ... 4:
            ret = BtlColl_GuardRush(chr, hit);
            break;
        }
    }
    return ret;
}

/* A hit lands on a fighter: dispatch by kind of hit, unless the pending reaction is one that cannot be replaced
 * (then the hit is consumed without effect). A hit that lands runs BtlChar_RaiseFirstClash on the attacker. */
s32 BtlColl_Hit(s32 objId, BtlCollHit *hit) {
    BtlCollChr *chr;
    s32 ret = 0;

    chr = BtlChar_FindByObjId(objId);
    if (chr == NULL) {
        return 0;
    }
    if (!TagTeam_CanContact(hit->ownerId, chr->side)) return 1;
    switch (chr->react.id) {
    case 9:
    case 10:
    case 11:
    case 12:
    case 17:
    case 19:
    case 20:
    case 21:
    case 22:
    case 29:
    case 30:
    case 34:
    case 37:
    case 39:
    case 40:
    case 41:
        return 1;
    }
    if (hit->type == BTL_HIT_BLAST) {
        ret = BtlColl_HitByBlast(chr, hit);
    } else if (hit->owner != NULL) {
        switch (hit->owner->slot) {
        case 0:
        case 1:
            ret = BtlColl_HitByStrike(chr, hit);
            break;
        case 2:
        case 3:
            ret = BtlColl_HitByRush(chr, hit);
            break;
        case 4:
            ret = BtlColl_HitByRush(chr, hit);
            break;
        }
    }
    if (ret) {
        TagLobby_Contact(hit->ownerId,chr->side);
        BtlChar_RaiseFirstClash(BtlChar_Get(hit->ownerId));
    }
    return ret;
}

/* Plays only the contact sound of a hit (no fighter involved). */
void BtlColl_PlayContactSound(BtlCollHit *hit) {
    BtlCollChr *atk;
    s32 slot;
    s32 sound;

    switch (hit->type) {
    case BTL_HIT_BLAST:
        switch (BtlKiBlast_GetTypeOfHit(hit)) {
        case 2:
            sound = 0x42;
            break;
        case 3:
            sound = BtlChar_FrameMod(2) ? 0xC : 0xD;
            break;
        default:
            sound = BtlChar_FrameMod(2) ? 0x1B : 0x11;
            break;
        }
        if (sound >= 0) {
            BtlCharSnd_RequestAt(&hit->pos, 0, sound, 200.0f, 1500.0f);
        }
        break;
    case BTL_HIT_TECH:
        if (hit->owner != NULL) {
            atk = BtlChar_Get(hit->owner->side);
            slot = hit->owner->slot;
            switch (slot) {
            case 0 ... 1:
                BtlColl_PlayHitSound(NULL, hit, BtlSkill_GetHitSound(atk, slot));
                break;
            case 2 ... 4:
                BtlColl_PlayHitSound(NULL, hit, BtlSuper_GetHitSound(atk, slot, 1));
                break;
            }
        }
        break;
    }
}

/* Keeps the fighter on the ground query result and derives the ground / water / ring-out flags. */
void BtlColl_UpdateGround(BtlCollChr *chr) {
    Vec4 vel;
    f32 water;
    s32 landed = 0;
    BtlCollObj *obj = BtlChar_GetObj(chr);
    BtlCollPose *pose = BtlChar_GetPos(chr);
    BtlCollGround *g = &obj->work->ground;
    f32 margin;
    f32 floorY;
    f32 size;
    f32 lo;
    f32 hi;

    BtlChar_GetSnapDelta(chr, &vel, 0, 1);
    if (g->pos.y > 100000.0f) {
    } else {
        pose->groundPos = g->pos;
        pose->groundNrm = g->nrm;
        pose->groundFlags = g->flags;
    }
    margin = BtlUtil_LengthXZ(&vel) + 1.0f;
    if (!BtlChar_TestFlag(chr, 0xF)) {
        margin = 0.0f;
    }
    if (BtlChar_IsStage4Or27()) {
        if (!(g->flags & 0x80)) {
            margin = 0.0f;
        }
    }
    if (BtlChar_TestFlag(chr, 0xE)) {
        margin = 0.0f;
    }
    if (BtlChar_TestFlag(chr, 0x58)) {
        margin = 0.0f;
    }
    if (BtlChar_TestFlag(chr, 0x24)) {
        margin = 0.0f;
    }
    floorY = pose->groundPos.y - pose->unk20.y;
    if (floorY - margin < pose->pos.y) {
        pose->pos.y = floorY;
        if (vel.y > 0.0f) {
            pose->velY = 0.0f;
            landed = 1;
        }
    }
    if (BtlChar_TestFlag(chr, 0x10)) {
        landed = 1;
        pose->velY = 0.0f;
        pose->pos.y = pose->groundPos.y - pose->unk20.y;
    }
    if (obj->work->hitFlags & 0x40) {
        BtlChar_SetFlag(chr, 0x20);
        BtlChar_ClearFlag(chr, 0xF);
        if (BtlChar_TestFlag(chr, 0xA)) {
            if (obj->work->hitFlags & 0x80) {
                BtlMember_Damage(chr, 200, 0);
            }
            if (obj->work->hitFlags & 0x100) {
                BtlMember_Damage(chr, 600, 0);
            }
            if (obj->work->hitFlags & 0x200) {
                BtlMember_Damage(chr, 1000, 0);
            }
        }
    } else if (landed) {
        BtlChar_SetHeldFlag(chr, 0xF);
    } else {
        BtlChar_ClearFlag(chr, 0xF);
    }
    BtlChar_ClearFlag(chr, 0x11);
    BtlChar_ClearFlag(chr, 0x12);
    if (BtlStage_GetWaterLevel(&water)) {
        if (water < g->pos.y) {
            size = BtlCharApi_GetHeight(chr->objId);
            if (water < pose->pos.y - size * 0.5f) {
                BtlChar_SetHeldFlag(chr, 0x11);
            }
            lo = pose->pos.y - size;
            hi = pose->pos.y + pose->unk98 * 30.0f * 0.02f;
            if (lo < water && water < hi) {
                BtlChar_SetHeldFlag(chr, 0x12);
            }
        }
    }
    if (BtlChar_IsFlagRaised(chr, 0x11)) {
        BtlChar_GetPos(chr)->velY *= 0.5f;
    }
    if (BtlChar_IsStage4Or27()) {
        if (pose->groundPos.y < -10.0f) {
            BtlChar_ClearFlag(chr, 0x17);
        }
        if (obj->work->hitFlags & 0x20) {
            if (!(obj->work->unk18060 & 0xC0)) {
                BtlChar_SetHeldFlag(chr, 7);
            }
        }
        if (obj->work->hitFlags & 0x40) {
            BtlChar_SetHeldFlag(chr, 7);
        }
        if (landed) {
            if (!(g->flags & 0xC0)) {
                BtlChar_SetHeldFlag(chr, 7);
            }
        }
    }
    if (obj->work->hitFlags & 0x20) {
        BtlChar_SetHeldFlag(chr, 0x1F);
    } else {
        BtlChar_ClearFlag(chr, 0x1F);
    }
}

/* Begin-frame reset of the action-class bits. */
void BtlColl_ClearActionBits(BtlCollChr *chr) {
    chr->actBits = 0;
    BtlColl_SetUnk1294(chr, -1);
}

/* Records the class of an action started this frame as one bit of chr->actBits. */
void BtlColl_AddActionBit(BtlCollChr *chr, s32 action) {
    u64 bit = 0;

    switch (action) {
    case 0x70:
        bit = 1;
        break;
    case 0x73:
        bit = 2;
        break;
    case 0x74:
        bit = 4;
        break;
    case 0x77:
        bit = 8;
        break;
    case 0x7E:
        bit = 0x10;
        break;
    case 0x80:
        bit = 0x20;
        break;
    case 0x82:
        bit = 0x40;
        break;
    case 0x7B: case 0x7C:
        bit = 0x8000000;
        break;
    case 0x19:
        bit = 0x80;
        break;
    case 0x5A: case 0x5B: case 0x5C: case 0x5D:
        bit = 0x100;
        break;
    case 0x44:
        bit = 0x200;
        break;
    case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C:
        bit = 0x400;
        break;
    case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        bit = 0x800;
        break;
    case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
        bit = 0x2000000000ULL;
        break;
    case 0xAE:
        bit = 0x1000;
        break;
    case 0xAF:
        bit = 0x2000;
        break;
    case 0xB0:
        bit = 0x4000;
        break;
    case 0xB1:
        bit = 0x8000;
        break;
    case 0xB2:
        bit = 0x10000;
        break;
    case 0xB3:
        bit = 0x20000;
        break;
    case 0x5E:
        bit = 0x40000;
        break;
    case 0x5F:
        bit = 0x80000;
        break;
    case 0x60:
        bit = 0x100000;
        break;
    case 0x61:
        bit = 0x200000;
        break;
    case 0x62:
        bit = 0x400000;
        break;
    case 0x63:
        bit = 0x800000;
        break;
    case 0x66:
        bit = 0x1000000;
        break;
    case 0x64: case 0x65:
        bit = 0x2000000;
        break;
    case 0x84: case 0x85:
        bit = 0x80000000ULL;
        break;
    case 0xB4:
        bit = 0x4000000;
        break;
    case 0xB6:
        bit = 0x40000000;
        break;
    case 0x90:
        bit = 0x20000000;
        break;
    case 0x9B:
        bit = 0x10000000;
        break;
    case 0x99:
        bit = 0x100000000ULL;
        break;
    case 0x93:
        bit = 0x200000000ULL;
        break;
    case 0x96:
        bit = 0x400000000ULL;
        break;
    case 0x34:
        bit = 0x800000000ULL;
        break;
    case 0xA2:
        bit = 0x1000000000ULL;
        break;
    case 0x25:
        bit = 0x20000000000ULL;
        break;
    case 0x26:
        bit = 0x40000000000ULL;
        break;
    case 0x28: case 0x2A:
        bit = 0x80000000000ULL;
        break;
    case 0xD7:
        bit = 0x100000000000ULL;
        break;
    case 0x24:
        bit = 0x200000000000ULL;
        break;
    case 0x42:
        bit = 0x400000000000ULL;
        break;
    default:
        break;
    }
    chr->actBits |= bit;
}

/* Sets chr->unk1294. */
void BtlColl_SetUnk1294(BtlCollChr *chr, s32 v) {
    chr->unk1294 = v;
}

/* Sets unk1294 to the number of frames the animation needs to reach `scale` times its length (rounded up). */
void BtlColl_SetFramesToReach(BtlCollChr *chr, f32 scale) {
    f32 len;
    f32 have;
    f32 rate;
    f32 need;

    len = BtlAnim_GetLength(chr);
    have = BtlAnim_GetFrame(chr);
    rate = BtlAnim_GetStep(chr);
    need = len * scale;
    if (need <= have) {
        return;
    }
    if (rate < 0.01f) {
        return;
    }
    BtlColl_SetUnk1294(chr, (s32)((need - have) / rate + 0.99f));
}
