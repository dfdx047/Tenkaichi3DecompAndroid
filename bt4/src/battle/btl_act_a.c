#include "common.h"
#include "battle/btl_act_a.h"

/*
 * Fighter action handlers, first group: 0x1E3158..0x1EA5F8 (two parts: to 0x1E6CC0, and from there; the second
 * part has its own header comment below and its own view of the fighter, include/battle/btl_act_b.h).
 *
 * Continues the object of btl_char_action.c (its float pool and jump tables run on): the handler of
 * actions 7..0xA, the attack charge helpers, the shared attack follow-up helpers, and the handlers of
 * actions 0x44..0x66 (rush chain, charged smash attacks, dash attacks, the vanishing attack, chargeable
 * rush finishers). See include/battle/btl_char_action.h for the handler protocol (phase 0 enter, 1 run,
 * 2 decide, 3 leave; int functions that fall off their end).
 *
 * Game terms in the names ("rush", "smash", "flying kick", ...) are inferred from the animations, inputs
 * and flags; the mechanics are what the C says. Flags used throughout (meanings inferred): 0xE in flight,
 * 0x11 set-flight request, 0x5B the attack connected, 0x5C / 0x5D the attack was stopped (guarded /
 * clashed), 0xD5 attacking, 0x3D follow-up window open, 0x84 fully charged lunge, 5 locked on.
 */

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)

extern f32 floorf(f32 x);

extern s32 BtlAct_GetCurrent(BtlActAChr *chr);
extern f32 BtlAct_GetHeight(BtlActAChr *chr);
extern f32 BtlAct_GetHeightRatio(BtlActAChr *chr);
extern s32 BtlAct_GetPrev(BtlActAChr *chr);
extern s32 BtlAct_GetQueued(BtlActAChr *chr);
extern s32 BtlAct_HasQueued(BtlActAChr *chr);
extern void BtlAct_Request(BtlActAChr *chr, s32 id);
extern void BtlAct_SetQueue(BtlActAChr *chr, u32 slot, s32 id);
extern s32 BtlAct_TestAttackSkill(BtlActAChr *chr, s32 attack);
extern s32 BtlAct_TestPoweredSkill(BtlActAChr *chr, u32 mask);
extern s32 BtlAnim_Advance(BtlActAChr *chr, s32 flags);
extern void BtlAnim_AdvanceLoop(BtlActAChr *chr, s32 flags);
/* The float goes before the flags: with (next, flags, blend) the argument set-up order differs at five call sites. */
extern s32 BtlAnim_AdvanceThen(BtlActAChr *chr, s32 next, f32 blend, s32 flags);
extern f32 BtlAnim_GetFrame(BtlActAChr *chr);
extern s32 BtlAnim_GetId(BtlActAChr *chr);
extern f32 BtlAnim_GetLength(BtlActAChr *chr);
extern f32 BtlAnim_GetProgress(BtlActAChr *chr);
extern s32 BtlAnim_IsNew(BtlActAChr *chr);
extern void BtlAnim_Play(BtlActAChr *chr, s32 anim, f32 blend);
extern void BtlAnim_PlaySub(BtlActAChr *chr, s32 anim);
extern void BtlAnim_Request(BtlActAChr *chr, s32 anim, f32 blend);
extern void BtlAnim_SetDuration(BtlActAChr *chr, f32 seconds);
extern void BtlAnim_SetObjRate(BtlActAChr *chr, f32 rate);
extern void BtlAnim_SetStep(BtlActAChr *chr, f32 step);
extern void BtlChar_ClearFlag(BtlActAChr *chr, u32 n);
extern BtlActAObj *BtlChar_GetObj(BtlActAChr *chr);
extern BtlActAPose *BtlChar_GetPos(BtlActAChr *chr);
extern s32 BtlChar_IsFlagDropped(BtlActAChr *chr, u32 n);
extern void BtlChar_PlayVoice(BtlActAChr *chr, s32 kind);
extern void BtlChar_SetFlag(BtlActAChr *chr, u32 n);
extern void BtlChar_SetFxBit(BtlActAChr *chr, s32 n);
extern void BtlChar_SetHeldFlag(BtlActAChr *chr, u32 n);
extern void BtlChar_SetSmallVibration(BtlActAChr *chr, f32 seconds);
extern void BtlChar_SetVibration(BtlActAChr *chr, f32 power, f32 seconds);
extern void BtlCharSnd_PlayCommon(BtlActAChr *chr, s32 id);
extern s32 BtlChar_TestFlag(BtlActAChr *chr, u32 n);
extern s32 BtlChar_TestMemberUnk70(BtlActAChr *chr);
extern void BtlChar_ToggleHeldFlag(BtlActAChr *chr, u32 n);
extern void BtlColl_AddActionBit(BtlActAChr *chr, s32 action);
extern s32 BtlInput_TestAction(BtlActAChr *chr, s32 id, s32 want);
extern s32 BtlMember_SpendKi(BtlActAChr *chr, s32 amount, s32 force);
extern void BtlMove_Advance(BtlActAChr *chr, f32 speed, f32 accel);
extern void BtlMove_ApplyGravity(BtlActAChr *chr);
extern void BtlMove_BeginRiseToOpponent(BtlActAChr *chr);
extern void BtlMove_BrakeVertical(BtlActAChr *chr);
extern void BtlMove_Fall(BtlActAChr *chr);
extern s32 BtlMove_IsBlockedByOpponent(BtlActAChr *chr);
extern void BtlMove_SetDirection(BtlActAChr *chr, s32 mode);
extern void BtlMove_SetHeading(BtlActAChr *chr, f32 yaw, f32 pitch);
extern void BtlMove_SetLeanX(BtlActAChr *chr, f32 v);
extern void BtlMove_SnapToOpponent(BtlActAChr *chr);
extern void BtlMove_SteerAtOpponent(BtlActAChr *chr, f32 closeSpeed, f32 yawAccel, f32 pitchAccel, f32 yawMax, f32 pitchMax, f32 maxStep);
extern void BtlMove_Step(BtlActAChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel);
extern void BtlMove_TurnModelYaw(BtlActAChr *chr, f32 maxStep, f32 rate);
extern void BtlMove_TurnPitch(BtlActAChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_TurnYaw(BtlActAChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_WarpAheadOfOpponent(BtlActAChr *chr, f32 lead);
extern f32 BtlOpp_GetGapXZ(BtlActAChr *chr);
extern s32 BtlOpp_GetSeenAction(BtlActAChr *chr);
extern f32 BtlOpp_GetYawFromFacing(BtlActAChr *chr);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
extern f32 BtlUtil_MaxF(f32 a, f32 b);
extern f32 BtlUtil_MinF(f32 a, f32 b);
extern void ChrCam_EndCut(BtlActAChr *chr);
extern void ChrCam_RequestCut(BtlActAChr *chr, s32 arg1, s32 arg2);

extern s32 BtlDecide_Main(BtlActAChr *chr, u32 mask);
extern s32 BtlDecide_Attack(BtlActAChr *chr, u32 mask);
extern s32 BtlDecide_QueueAttack(BtlActAChr *chr, s32 action);
extern s32 BtlAct_QueueComboFinish(BtlActAChr *chr);
extern f32 BtlAtk_GetLaunchSpeed(BtlActAChr *chr);
extern f32 BtlAtk_GetLaunchAngleBOf(BtlActAChr *chr, s32 arg);
extern s32 BtlAtk_GetVoiceKind(BtlActAChr *chr);
extern s32 BtlParam_GetFlags2(BtlActAChr *chr);
extern f32 BtlParam_GetChargeRateA(BtlActAChr *chr);
extern f32 BtlParam_GetChargeRateB(BtlActAChr *chr);
extern s32 BtlObjAnim_MaskToNode(u32 bit);                                /* event bit -> model node id */
extern s32 BtlObjAnim_QueryEvent(BtlActAObj *obj, u64 a, s32 b, s32 c);  /* motion event query (mask, layer, what) */
extern s32 BtlParam_GetDashSound(BtlActAChr *chr);
extern f32 BtlMoveParam_GetSpeed(BtlActAChr *chr, s32 kind);
extern f32 BtlMoveParam_GetAngle58(BtlActAChr *chr);
extern s32 BtlMoveParam_GetKiCost(BtlActAChr *chr, s32 kind);
extern f32 BtlMoveParam_GetTurnAccel(BtlActAChr *chr, s32 axis);
extern f32 BtlMoveParam_GetTurnMax(BtlActAChr *chr, s32 axis);
extern s32 BtlObj_GetNodeSide(s32 a);                                   /* node id -> group 0..3 */

/* Actions 7..0xA: stand (animations 0 / 0x26, 1 / 0x27, 0x196, 0x17B) facing the opponent until flag 0xAF or 0xB0 drops. */
s32 BtlAct_WaitHandler(BtlActAChr *chr, s32 phase) {
    s32 anim;
    s32 air;
    f32 turn;

    if (phase == 0) {
        anim = 0;
        air = 5.0f < BtlAct_GetHeight(chr);
        switch (BtlAct_GetCurrent(chr)) {
            case 7:
                anim = air ? 0x26 : 0;
                break;
            case 8:
                anim = air ? 0x27 : 1;
                break;
            case 9:
                anim = 0x196;
                break;
            case 10:
                anim = 0x17B;
                break;
        }
        turn = 3.14159265f;
        BtlAnim_Play(chr, anim, 0.15f);
        BtlChar_SetHeldFlag(chr, 5);
        BtlChar_SetFlag(chr, 0x29);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->vspeed = 0.0f;
        BtlMove_TurnYaw(chr, 2, turn);
        BtlMove_TurnPitch(chr, 5, turn);
        BtlMove_TurnModelYaw(chr, turn, 1.0f);
    }
    if (phase == 1) {
        BtlAnim_AdvanceLoop(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlChar_SetFlag(chr, 0x5A);
        if (5.0f < BtlAct_GetHeight(chr)) {
            BtlChar_SetFxBit(chr, 0x3A);
        } else {
            BtlChar_SetFxBit(chr, 0x3B);
        }
    }
    if (phase == 2) {
        if (BtlChar_IsFlagDropped(chr, 0xAF)) {
            BtlAct_Request(chr, 0xB);
        }
        if (BtlChar_IsFlagDropped(chr, 0xB0)) {
            BtlAct_Request(chr, 0xB);
        }
    }
}

/* Attack charge rate: the character's two rates blended by the gauge at +0xD80 (0..100000), at least 0.1. */
f32 BtlAct_GetChargeRate(BtlActAChr *chr) {
    f32 t = (f32)chr->unkD80 / 100000.0f;
    f32 a = BtlParam_GetChargeRateA(chr);
    f32 b = BtlParam_GetChargeRateB(chr);

    return BtlUtil_MaxF((1.0f - t) * a + t * b, 0.1f);
}

/* Advances the charge timer by one frame; returns 1 when the charge level reached 1. */
s32 BtlAct_StepCharge(BtlActAChr *chr) {
    f32 rate = BtlAct_GetChargeRate(chr);

    chr->unkD7C = BtlUtil_MinF(chr->unkD7C + rate * (1.0f / 22.5f), 1.0f);
    if (chr->unkD7C < 1.0f) {
        chr->unkD78 = chr->unkD7C / rate;
    } else {
        chr->unkD78 = 1.0f;
    }
    return 1.0f <= chr->unkD78;
}

/* Takes the charge level from the animation progress. */
void BtlAct_SetChargeFromAnim(BtlActAChr *chr) {
    chr->unkD78 = BtlAnim_GetProgress(chr);
}

/* Sets the charge animation's duration from the charge rate (1.5 s at rate 1, at least 0.75 s). */
void BtlAct_SetChargeAnimDuration(BtlActAChr *chr) {
    BtlAnim_SetDuration(chr, BtlUtil_MaxF(1.5f / BtlAct_GetChargeRate(chr), 0.75f));
}

/* Sets the animation step to twice the blended rate. */
void BtlAct_SetChargeAnimStep(BtlActAChr *chr) {
    f32 t = (f32)chr->unkD80 / 100000.0f;
    f32 a = BtlParam_GetChargeRateA(chr);
    f32 b = BtlParam_GetChargeRateB(chr);

    BtlAnim_SetStep(chr, ((1.0f - t) * a + t * b) * 2.0f);
}

/* Under flag 0x5D, when the opponent is seen in action 0xC9: queue a follow-up and start it past the given progress. */
void BtlAct_CancelOnOppC9(BtlActAChr *chr, f32 after) {
    if (BtlChar_TestFlag(chr, 0x5D) && BtlOpp_GetSeenAction(chr) == 0xC9) {
        if (!BtlAct_HasQueued(chr)) {
            BtlDecide_Attack(chr, 1);
        }
        if (after < BtlAnim_GetProgress(chr)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/* Plays the voice kind BtlAtk_GetVoiceKind picks, if any. */
void BtlAct_PlayPickedVoice(BtlActAChr *chr) {
    s32 kind = BtlAtk_GetVoiceKind(chr);

    if (kind >= 0) {
        BtlChar_PlayVoice(chr, kind);
    }
}

/* Starts sub animation 0x19B and returns a model node (0x2D, 0x1F or 0x15) by the group of the first node its event names. */
s32 BtlAct_StartSub19B(BtlActAChr *chr) {
    s32 i;
    BtlActAObj *obj;
    u32 mask;
    s32 kind;

    i = 0;
    obj = BtlChar_GetObj(chr);
    BtlAnim_PlaySub(chr, 0x19B);
    mask = BtlObjAnim_QueryEvent(obj, 1, 1, 6);
    for (; i < 32; i++) {
        if (mask & (1 << i)) {
            kind = BtlObj_GetNodeSide(BtlObjAnim_MaskToNode(mask & (1 << i)));
            if (kind == 0) {
                return 0x2D;
            }
            if (kind == 1) {
                return 0x1F;
            }
        }
    }
    return 0x15;
}

/* Attack decide step: under flag 0x5B read follow-up inputs by the attack's flags, and start the queued action past cancelAt. */
void BtlAct_DecideAttackFollow(BtlActAChr *chr, BtlActAAttack *atk) {
    s32 i;

    if (BtlChar_TestFlag(chr, 0x5B)) {
        for (i = 0; i < 2; i++) {
            if (atk->follow[i] >= 0) {
                if (BtlDecide_QueueAttack(chr, atk->follow[i] + 0x70)) {
                    atk->chained = 1;
                }
            }
        }
        if (atk->flags & 1) {
            if (BtlDecide_Attack(chr, 0x80000)) {
                atk->chained = 0;
            }
        }
        if (atk->flags & 2) {
            if (BtlDecide_Attack(chr, 0x100000)) {
                atk->chained = 0;
            }
        }
        if (atk->flags & 4) {
            if (BtlDecide_Attack(chr, 1)) {
                atk->chained = 0;
            }
        }
        if (atk->flags & 8) {
            if (BtlInput_TestAction(chr, 0x34, 1)) {
                BtlAct_SetQueue(chr, 0, 0x46);
                atk->chained = 0;
            }
        }
        if (atk->flags & 0x100) {
            if (BtlAct_QueueComboFinish(chr)) {
                atk->chained = 0;
            }
        }
        if (atk->flags & 0x200) {
            if (BtlAct_QueueComboFinish(chr)) {
                atk->chained = 0;
            }
        }
        if (!BtlChar_TestFlag(chr, 0x5D)) {
            if (atk->cancelAt < BtlAnim_GetProgress(chr)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
                if (atk->chained) {
                    if (atk->flags & 0x10) {
                        BtlChar_SetHeldFlag(chr, 0xDD);
                    }
                }
            }
        }
    }
}

/* Attack end: back to action 0xB, or 0xC under flag 0x5C when the attack has flag 0x400. */
void BtlAct_RequestAttackEnd(BtlActAChr *chr, BtlActAAttack *atk) {
    s32 id = 0xB;

    if (BtlChar_TestFlag(chr, 0x5C)) {
        if (atk->flags & 0x400) {
            id = 0xC;
        }
    }
    BtlAct_Request(chr, id);
}

/* Action 0x44: rush combo step. */
s32 BtlAct_RushHandler(BtlActAChr *chr, s32 phase) {
    f32 *cancelAt = &chr->workF[0];
    BtlActAPose *pose;
    s32 next;
    s32 mode;
    s32 anim;

    if (phase == 0) {
        if (BtlAct_TestPoweredSkill(chr, 0x10)) {
            if (chr->unkD60 == 4) {
                chr->unkD60 = 5;
            }
            if (chr->unkD60 == 9) {
                chr->unkD60 = 0;
            }
        }
        if (BtlChar_TestFlag(chr, 0x86)) {
            if (chr->unkD60 < 5) {
                chr->unkD60 = 5;
            }
            BtlChar_ClearFlag(chr, 0x86);
        }
        anim = chr->unkD60 + 0x37;
        chr->unkD60++;
        chr->unkD60 %= 10;
        BtlAnim_Play(chr, anim, 0.1f);
        BtlMove_BeginRiseToOpponent(chr);
        pose = BtlChar_GetPos(chr);
        pose->speed = BtlAtk_GetLaunchSpeed(chr);
    }
    if (phase == 1) {
        next = BtlChar_TestFlag(chr, 0xE) ? 0x26 : 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x37:
            case 0x38:
            case 0x39:
            case 0x3A:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
                BtlAnim_AdvanceThen(chr, next, 0.3f, 0);
                BtlChar_SetFlag(chr, 0x40);
                break;
            case 0x3B:
            case 0x40:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                if (BtlAnim_GetFrame(chr) < (f32)BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 2, 0, 2)) {
                    BtlChar_SetFlag(chr, 0x40);
                }
                break;
            case 0:
            case 0x26:
                BtlAnim_Advance(chr, 1);
                if (!BtlChar_TestFlag(chr, 0x34)) {
                    BtlAct_Request(chr, 0xB);
                }
                BtlChar_SetFlag(chr, 0x87);
                break;
        }
        mode = 6;
        if (BtlChar_TestFlag(chr, 0x5B) || BtlChar_TestFlag(chr, 0x5D)) {
            mode = 2;
        }
        BtlMove_Step(chr, mode, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x88);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x37:
            case 0x38:
            case 0x39:
            case 0x3A:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
                BtlDecide_Main(chr, 0x22000);
                BtlDecide_Attack(chr, 0x201073);
                if (BtlChar_TestFlag(chr, 0x31)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
            case 0x3B:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    if (0.4f < BtlAnim_GetProgress(chr)) {
                        if (BtlDecide_QueueAttack(chr, BtlAct_GetHeightRatio(chr) < 0.5f ? 0x7B : 0x7C)) {
                            *cancelAt = 0.6f;
                        }
                    }
                    if (BtlParam_GetFlags2(chr) & 2) {
                        if (BtlInput_TestAction(chr, 0x2A, 1)) {
                            BtlAct_SetQueue(chr, 0, 0x1F);
                            *cancelAt = 0.5f;
                        }
                    }
                    if (*cancelAt < BtlAnim_GetProgress(chr)) {
                        BtlAct_Request(chr, BtlAct_GetQueued(chr));
                    }
                }
                break;
            case 0x40:
                break;
            case 0:
            case 0x26:
                BtlDecide_Main(chr, 0x22000);
                BtlDecide_Attack(chr, 0x201073);
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
                break;
        }
    }
}

/* Action 0x45 (forced by flag 0x7E): the whole rush chain played at triple speed with a camera cut. */
s32 BtlAct_RushAutoHandler(BtlActAChr *chr, s32 phase) {
    if (phase == 0) {
        if (BtlAct_GetPrev(chr) != 0x45) {
            BtlAnim_Play(chr, 0x37, 0.1f);
        }
        ChrCam_RequestCut(chr, 1, chr->camSide < 0.0f ? 0x11 : 0x12);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
            case 0x37:
            case 0x38:
            case 0x39:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
                BtlAnim_SetStep(chr, 3.0f);
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                break;
            case 0x3A:
                BtlAnim_SetStep(chr, 3.0f);
                BtlAnim_AdvanceThen(chr, 0x3C, 0.0f, 0);
                break;
            case 0x40:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                if (!BtlAnim_IsNew(chr)) {
                    if (BtlChar_GetObj(chr)->unkCAD == ~BtlChar_GetObj(chr)->unkCAC) {
                        BtlChar_SetFlag(chr, 0x90);
                    }
                }
                break;
            case 0x3B:
                break;
        }
        BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x88);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
}

/* Action 0x46: rapid rush (animations 0x3C..0x40 at triple speed), repeated by input condition 52. */
s32 BtlAct_RushRapidHandler(BtlActAChr *chr, s32 phase) {
    BtlActAPose *pose;
    s32 next;
    s32 can;
    s32 go;

    if (phase == 0) {
        if (BtlAct_GetPrev(chr) != 0x46) {
            chr->unkD64 = 0;
            BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
            BtlChar_PlayVoice(chr, 7);
        }
        chr->unkD64 = BtlUtil_Clamp(chr->unkD64, 0, 4);
        BtlAnim_Play(chr, chr->unkD64 + 0x3C, 0.1f);
        chr->unkD64++;
        BtlMove_BeginRiseToOpponent(chr);
        BtlChar_SetHeldFlag(chr, 0xE);
        pose = BtlChar_GetPos(chr);
        pose->speed = BtlAtk_GetLaunchSpeed(chr);
        if (BtlChar_TestFlag(chr, 0x13)) {
            ChrCam_RequestCut(chr, 1, chr->camSide < 0.0f ? 0x1A : 0x1B);
        }
    }
    if (phase == 1) {
        next = BtlChar_TestFlag(chr, 0xE) ? 0x26 : 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
                BtlAnim_SetStep(chr, 3.0f);
                BtlAnim_SetObjRate(chr, 2.0f);
                BtlAnim_AdvanceThen(chr, next, 0.3f, 0);
                BtlChar_SetFxBit(chr, 9);
                break;
            case 0x40:
                if (BtlAnim_GetFrame(chr) < (f32)BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 1, 0, 0)) {
                    BtlAnim_SetStep(chr, 3.0f);
                    BtlChar_SetFxBit(chr, 9);
                } else {
                    ChrCam_EndCut(chr);
                }
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    BtlChar_GetPos(chr)->speed = 0.0f;
                }
                break;
            case 0:
            case 0x26:
                BtlAnim_Advance(chr, 1);
                if (!BtlChar_TestFlag(chr, 0x34)) {
                    BtlAct_Request(chr, 0xB);
                }
                break;
        }
        BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
    }
    if (phase == 2) {
        can = 0;
        go = 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
                can = 1;
                if (BtlChar_TestFlag(chr, 0x31)) {
                    go = 1;
                }
                break;
            case 0:
            case 0x26:
                can = 1;
                go = 1;
                break;
        }
        if (can) {
            if (BtlChar_TestFlag(chr, 6) && BtlAct_TestAttackSkill(chr, 0x3C) && !BtlChar_TestMemberUnk70(chr)) {
                BtlDecide_QueueAttack(chr, 0xAC);
            } else {
                BtlDecide_QueueAttack(chr, 0x9D);
            }
            if (BtlInput_TestAction(chr, 0x34, 1)) {
                BtlAct_SetQueue(chr, 0, 0x46);
            }
            if (go) {
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
            }
        }
    }
}

/* Actions 0x47..0x4C: charged smash attack (wind-up, hold, release animations). */
s32 BtlAct_SmashChargeHandler(BtlActAChr *chr, s32 phase) {
    s32 *frames = &chr->work[2];
    f32 *cancelAt = &chr->workF[0];
    BtlActAPose *pose;
    s32 anim;
    s32 yawMode;
    s32 pitchMode;
    s32 charging;
    s32 id;
    f32 speed;
    f32 accel;
    f32 turn;

    if (phase == 0) {
        anim = 0;
        switch (BtlAct_GetCurrent(chr)) {
            case 0x47:
                anim = 0x50;
                break;
            case 0x48:
                anim = 0x53;
                break;
            case 0x49:
                anim = 0x56;
                break;
            case 0x4A:
                anim = 0x59;
                break;
            case 0x4B:
                anim = 0x4A;
                break;
            case 0x4C:
                anim = 0x4D;
                break;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        BtlMove_BeginRiseToOpponent(chr);
        BtlCharSnd_PlayCommon(chr, 0x2E);
    }
    if (phase == 1) {
        speed = 0.0f;
        accel = BTL_KMH(100.0f);
        turn = speed;
        yawMode = 6;
        pitchMode = 5;
        charging = 0;
        if (BtlChar_TestFlag(chr, 0x81)) {
            speed = BTL_KMH(50.0f);
            yawMode = 2;
            turn = BTL_DEG(1.8f);
        }
        switch (BtlAnim_GetId(chr)) {
            case 0x4A:
            case 0x4D:
            case 0x50:
            case 0x53:
            case 0x56:
            case 0x59:
                BtlAct_SetChargeAnimStep(chr);
                if (!BtlChar_TestFlag(chr, 0x84)) {
                    BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                } else {
                    BtlAnim_Advance(chr, 0);
                }
                charging = 1;
                break;
            case 0x4B:
            case 0x4E:
            case 0x51:
            case 0x54:
            case 0x57:
            case 0x5A:
                BtlAct_SetChargeAnimDuration(chr);
                if (!BtlChar_TestFlag(chr, 0x84)) {
                    if (BtlAnim_Advance(chr, 0)) {
                        chr->work[0] |= 1;
                    }
                } else {
                    BtlAnim_Advance(chr, 0);
                }
                charging = 1;
                break;
            case 0x4C:
            case 0x4F:
            case 0x52:
            case 0x55:
            case 0x58:
            case 0x5B:
                if (BtlAnim_IsNew(chr)) {
                    if (0.99f < chr->unkD78) {
                        BtlCharSnd_PlayCommon(chr, 0x2F);
                        BtlChar_SetVibration(chr, 1.0f, 0.3f);
                    }
                    BtlAct_PlayPickedVoice(chr);
                    pose = BtlChar_GetPos(chr);
                    pose->speed = BtlAtk_GetLaunchSpeed(chr);
                }
                BtlAnim_SetObjRate(chr, chr->unkD78 + 1.0f);
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                speed = 0.0f;
                yawMode = 6;
                turn = speed;
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    if (BtlChar_TestFlag(chr, 0x5B) || BtlChar_TestFlag(chr, 0x5D)) {
                        turn = 3.14159265f;
                        yawMode = 2;
                    }
                }
                break;
        }
        if (charging) {
            if (!BtlChar_TestFlag(chr, 0x84)) {
                if (BtlAct_StepCharge(chr)) {
                    BtlChar_SetFxBit(chr, 0x1E);
                    if (chr->unkD84 == 0) {
                        BtlCharSnd_PlayCommon(chr, 0x13);
                        BtlCharSnd_PlayCommon(chr, 0xF);
                    }
                    chr->unkD84++;
                }
            } else {
                if (*frames == 0) {
                    BtlChar_SetFxBit(chr, 0xC);
                    BtlCharSnd_PlayCommon(chr, 0x20);
                }
                if (*frames >= 2) {
                    BtlChar_SetFlag(chr, 0xB);
                    speed = BTL_KMH(3500.0f);
                    yawMode = 2;
                    accel = 10000.0f;
                    pitchMode = 4;
                    turn = 3.14159265f;
                }
                if (*frames >= 6) {
                    BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                }
                (*frames)++;
                BtlChar_SetFlag(chr, 0x42);
                BtlChar_SetFlag(chr, 0x43);
                BtlChar_SetFlag(chr, 0x44);
                BtlChar_SetFlag(chr, 0x45);
                BtlChar_SetFlag(chr, 0x46);
            }
        }
        BtlMove_TurnYaw(chr, yawMode, turn);
        BtlMove_TurnPitch(chr, pitchMode, 3.14159265f);
        BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
        BtlMove_SetDirection(chr, 3);
        BtlMove_Advance(chr, speed, accel);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x4A:
            case 0x4B:
            case 0x4D:
            case 0x4E:
            case 0x50:
            case 0x51:
            case 0x53:
            case 0x54:
            case 0x56:
            case 0x57:
            case 0x59:
            case 0x5A:
                if (!BtlChar_TestFlag(chr, 0x84)) {
                    if (BtlInput_TestAction(chr, 0x3F, 1)) {
                        chr->work[0] |= 1;
                    }
                    if (chr->work[0] & 1) {
                        if (chr->unkD84 == 1) {
                            BtlChar_SetHeldFlag(chr, 0x84);
                        } else if (chr->unkD84 > 0 && BtlAct_TestPoweredSkill(chr, 0x200) && BtlChar_TestFlag(chr, 0x13)) {
                            id = 0x51;
                            switch (BtlAct_GetCurrent(chr)) {
                                case 0x49:
                                    id = 0x4F;
                                    break;
                                case 0x4A:
                                    id = 0x50;
                                    break;
                                case 0x47:
                                    id = 0x4D;
                                    break;
                                case 0x48:
                                    id = 0x4E;
                                    break;
                                case 0x4B:
                                    id = 0x51;
                                    break;
                                case 0x4C:
                                    id = 0x52;
                                    break;
                            }
                            BtlAct_Request(chr, id);
                        } else {
                            chr->work[0] |= 2;
                        }
                    }
                    if (chr->work[0] & 2) {
                        BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                    }
                }
                break;
            case 0x4C:
            case 0x4F:
            case 0x52:
            case 0x55:
            case 0x58:
            case 0x5B:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    BtlChar_SetFlag(chr, 0x3D);
                    switch (BtlOpp_GetSeenAction(chr)) {
                        case 0xD5:
                            if (!BtlAct_HasQueued(chr)) {
                                if (BtlDecide_Attack(chr, 0x180000)) {
                                    *cancelAt = 0.7f;
                                }
                            }
                            break;
                        case 0xD4:
                            if (!BtlAct_HasQueued(chr)) {
                                if (BtlDecide_Main(chr, 8)) {
                                    *cancelAt = 0.7f;
                                }
                            }
                            break;
                        default:
                            if (!BtlAct_HasQueued(chr)) {
                                if (BtlDecide_Attack(chr, 1)) {
                                    *cancelAt = 0.6f;
                                }
                            }
                            break;
                    }
                    if (BtlAct_HasQueued(chr)) {
                        if (*cancelAt < BtlAnim_GetProgress(chr)) {
                            BtlAct_Request(chr, BtlAct_GetQueued(chr));
                        }
                    }
                }
                BtlAct_CancelOnOppC9(chr, 0.6f);
                break;
        }
    }
}

/* Actions 0x4D..0x52: fully charged smash attack with a camera cut (hit-stop flag 0x125 during the wind-up). */
s32 BtlAct_SmashFullHandler(BtlActAChr *chr, s32 phase) {
    f32 *cancelAt = &chr->workF[0];
    BtlActAPose *pose;
    s32 cut;
    s32 anim;
    s32 mode;

    if (phase == 0) {
        cut = 0;
        anim = 0;
        switch (BtlAct_GetCurrent(chr)) {
            case 0x4D:
                anim = 0x50;
                cut = 0x1E;
                break;
            case 0x4E:
                anim = 0x53;
                cut = 0x1F;
                break;
            case 0x4F:
                anim = 0x56;
                cut = 0x1C;
                break;
            case 0x50:
                anim = 0x59;
                cut = 0x1D;
                break;
            case 0x51:
                anim = 0x4A;
                cut = 0x1E;
                break;
            case 0x52:
                anim = 0x4D;
                cut = 0x1E;
                break;
        }
        BtlAnim_Play(chr, anim, 0.0f);
        BtlMove_BeginRiseToOpponent(chr);
        BtlChar_PlayVoice(chr, 7);
        ChrCam_RequestCut(chr, 1, cut);
    }
    if (phase == 1) {
        mode = 2;
        switch (BtlAnim_GetId(chr)) {
            case 0x4B:
            case 0x4E:
            case 0x51:
            case 0x54:
            case 0x57:
            case 0x5A:
                /* Written out twice: as a fall-through into the next case, phase and mode swap registers. */
                BtlAnim_SetDuration(chr, 0.2f);
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x125);
                BtlChar_SetFlag(chr, 0x12A);
                chr->unkD78 = 1.0f;
                break;
            case 0x4A:
            case 0x4D:
            case 0x50:
            case 0x53:
            case 0x56:
            case 0x59:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x125);
                BtlChar_SetFlag(chr, 0x12A);
                chr->unkD78 = 1.0f;
                break;
            case 0x4C:
            case 0x4F:
            case 0x52:
            case 0x55:
            case 0x58:
            case 0x5B:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                if (BtlAnim_IsNew(chr)) {
                    ChrCam_EndCut(chr);
                    BtlCharSnd_PlayCommon(chr, 0x2F);
                    pose = BtlChar_GetPos(chr);
                    pose->speed = BtlAtk_GetLaunchSpeed(chr);
                    BtlChar_SetVibration(chr, 1.0f, 0.3f);
                }
                if (BtlChar_TestFlag(chr, 0x5C)) {
                    mode = 6;
                }
                break;
        }
        BtlMove_Step(chr, mode, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x4A:
            case 0x4B:
            case 0x4D:
            case 0x4E:
            case 0x50:
            case 0x51:
            case 0x53:
            case 0x54:
            case 0x56:
            case 0x57:
            case 0x59:
            case 0x5A:
                break;
            case 0x4C:
            case 0x4F:
            case 0x52:
            case 0x55:
            case 0x58:
            case 0x5B:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    BtlChar_SetFlag(chr, 0x3D);
                    if (BtlOpp_GetSeenAction(chr) == 0xD5) {
                        if (!BtlAct_HasQueued(chr)) {
                            if (BtlDecide_Attack(chr, 0x180000)) {
                                *cancelAt = 0.7f;
                            }
                        }
                    }
                    if (BtlAct_HasQueued(chr)) {
                        if (*cancelAt < BtlAnim_GetProgress(chr)) {
                            BtlAct_Request(chr, BtlAct_GetQueued(chr));
                        }
                    }
                }
                break;
        }
    }
}

/* Actions 0x53..0x57: charged attack out of a dash: flies at the opponent spending ki while charging. */
s32 BtlAct_DashSmashHandler(BtlActAChr *chr, s32 phase) {
    s32 anim;
    s32 cost;
    s32 steer;
    f32 speed;
    f32 accel;
    f32 closeSpeed;
    f32 yawAccel;
    f32 pitchAccel;
    f32 yawMax;
    f32 pitchMax;
    f32 scale;

    if (phase == 0) {
        anim = 0;
        switch (BtlAct_GetCurrent(chr)) {
            case 0x53:
                anim = 0x65;
                break;
            case 0x54:
                anim = 0x68;
                break;
            case 0x55:
                anim = 0x5F;
                break;
            case 0x56:
                anim = 0x62;
                break;
            case 0x57:
                anim = 0x5C;
                break;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x2E);
    }
    if (phase == 1) {
        speed = BtlMoveParam_GetSpeed(chr, 6);
        closeSpeed = speed;
        cost = BtlMoveParam_GetKiCost(chr, 0);
        accel = 10000.0f;
        steer = 1;
        switch (BtlAnim_GetId(chr)) {
            case 0x5C:
            case 0x5F:
            case 0x62:
            case 0x65:
            case 0x68:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlChar_SetFxBit(chr, 4);
                BtlChar_SetFxBit(chr, 9);
                BtlChar_SetVibration(chr, 0.7f, 0.1f);
                BtlMember_SpendKi(chr, cost, 0);
                BtlChar_SetFlag(chr, 0x47);
                break;
            case 0x5D:
            case 0x60:
            case 0x63:
            case 0x66:
            case 0x69:
                BtlAct_SetChargeAnimDuration(chr);
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                if (chr->unkD68 > 0) {
                    BtlAct_StepCharge(chr);
                    BtlChar_SetFxBit(chr, 0x1E);
                } else if (BtlAct_StepCharge(chr)) {
                    if (chr->unkD84 == 0) {
                        BtlCharSnd_PlayCommon(chr, 0x13);
                        BtlCharSnd_PlayCommon(chr, 0xF);
                    }
                    chr->unkD84++;
                }
                BtlChar_SetFxBit(chr, 4);
                BtlChar_SetFxBit(chr, 9);
                BtlChar_SetVibration(chr, 0.7f, 0.1f);
                BtlMember_SpendKi(chr, cost, 0);
                BtlChar_SetFlag(chr, 0x47);
                break;
            case 0x5E:
            case 0x61:
            case 0x64:
            case 0x67:
            case 0x6A:
                if (BtlAnim_IsNew(chr)) {
                    BtlAct_PlayPickedVoice(chr);
                    BtlChar_SetVibration(chr, 1.0f, 0.3f);
                }
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                BtlAnim_SetObjRate(chr, 2.0f);
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    speed = 0.0f;
                    BtlChar_GetPos(chr)->speed = speed;
                }
                if (BtlChar_TestFlag(chr, 0x6C)) {
                    chr->work[0] |= 1;
                }
                if (chr->work[0] & 1) {
                    speed = 0.0f;
                    BtlChar_GetPos(chr)->speed = speed;
                }
                if (BtlChar_TestFlag(chr, 0x5C) || BtlChar_TestFlag(chr, 0x5D)) {
                    speed = 0.0f;
                    accel = BTL_KMH(100.0f);
                    steer = 0;
                    if (BtlMove_IsBlockedByOpponent(chr)) {
                        BtlChar_GetPos(chr)->speed = speed;
                    }
                }
                break;
        }
        if (BtlChar_TestFlag(chr, 5) && steer) {
            yawAccel = BtlMoveParam_GetTurnAccel(chr, 0);
            pitchAccel = BtlMoveParam_GetTurnAccel(chr, 1);
            yawMax = BtlMoveParam_GetTurnMax(chr, 0);
            pitchMax = BtlMoveParam_GetTurnMax(chr, 1);
            scale = BtlUtil_MinF(BtlOpp_GetGapXZ(chr) * 0.01f, 1.0f);
            BtlMove_SteerAtOpponent(chr, closeSpeed, yawAccel, pitchAccel, yawMax * scale, pitchMax * scale, 3.14159265f);
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 1.0f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, accel);
            BtlMove_ApplyGravity(chr);
        } else {
            BtlMove_Step(chr, 6, 6, 3, speed, accel);
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFlag(chr, 0x89);
        BtlChar_SetFlag(chr, 9);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x5C:
            case 0x5F:
            case 0x62:
            case 0x65:
            case 0x68:
                if (BtlInput_TestAction(chr, 0x3F, 1)) {
                    chr->work[0] |= 2;
                }
                break;
            case 0x5D:
            case 0x60:
            case 0x63:
            case 0x66:
            case 0x69:
                if (BtlInput_TestAction(chr, 0x3F, 1)) {
                    chr->work[0] |= 2;
                }
                if (!BtlChar_TestFlag(chr, 5)) {
                    chr->work[0] |= 2;
                }
                if (chr->work[0] & 2) {
                    BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                }
                break;
            case 0x5E:
            case 0x61:
            case 0x64:
            case 0x67:
            case 0x6A:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    BtlChar_SetFlag(chr, 0x3D);
                    switch (BtlOpp_GetSeenAction(chr)) {
                        case 0xD5:
                            if (!BtlAct_HasQueued(chr)) {
                                BtlDecide_Attack(chr, 0x180000);
                            }
                            if (0.7f < BtlAnim_GetProgress(chr)) {
                                BtlAct_Request(chr, BtlAct_GetQueued(chr));
                            }
                            break;
                        case 0xC6:
                            if (!BtlAct_HasQueued(chr)) {
                                BtlDecide_Main(chr, 0x8000);
                                BtlDecide_Attack(chr, 1);
                            }
                            if (0.6f < BtlAnim_GetProgress(chr)) {
                                BtlAct_Request(chr, BtlAct_GetQueued(chr));
                            }
                            break;
                    }
                }
                break;
        }
    }
}

/* Action 0x58: flying kick out of a dash (animations 0x6B wind-up, 0x6C hold, 0x6D strike). */
s32 BtlAct_FlyingKickHandler(BtlActAChr *chr, s32 phase) {
    f32 *cancelAt = &chr->workF[0];
    s32 steer;
    s32 i;
    f32 lean;
    f32 speed;
    f32 closeSpeed;
    f32 yawAccel;
    f32 scale;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x6B, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x2E);
        if (BtlChar_TestFlag(chr, 5)) {
            if (__builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) < BtlMoveParam_GetAngle58(chr)) {
                chr->work[0] |= 1;
            }
        }
    }
    if (phase == 1) {
        lean = 0.0f;
        speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) ? 5 : 4);
        closeSpeed = speed;
        steer = 0;
        if (chr->work[0] & 1) {
            steer = 1;
        }
        switch (BtlAnim_GetId(chr)) {
            case 0x6B:
                BtlAnim_AdvanceThen(chr, 0x6C, 0.0f, 0);
                BtlChar_SetFxBit(chr, 0xB);
                lean = 1.0f;
                break;
            case 0x6C:
                if (BtlAnim_AdvanceThen(chr, 0x6D, 0.0f, 0)) {
                    BtlCharSnd_PlayCommon(chr, 0x2F);
                    BtlChar_SetVibration(chr, 1.0f, 0.3f);
                }
                BtlAct_SetChargeFromAnim(chr);
                BtlChar_SetFxBit(chr, 0xB);
                lean = 1.0f;
                break;
            case 0x6D:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                if (BtlAnim_IsNew(chr)) {
                    BtlAct_PlayPickedVoice(chr);
                }
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    speed = 0.0f;
                    BtlChar_GetPos(chr)->speed = speed;
                } else if (BtlChar_TestFlag(chr, 0x5C) || BtlChar_TestFlag(chr, 0x5D)) {
                    speed = 0.0f;
                    steer = 0;
                    if (BtlMove_IsBlockedByOpponent(chr)) {
                        BtlChar_GetPos(chr)->speed = 0.0f;
                    }
                } else {
                    BtlChar_SetFxBit(chr, 0xB);
                }
                lean = 1.0f - BtlAnim_GetProgress(chr);
                break;
        }
        if (steer && BtlChar_TestFlag(chr, 5)) {
            yawAccel = BTL_DEG(9.0f);
            if (chr->actionFrame == 0) {
                yawAccel = 3.14159265f;
            }
            scale = BtlUtil_MinF(BtlOpp_GetGapXZ(chr) * 0.01f, 1.0f);
            BtlMove_SteerAtOpponent(chr, closeSpeed, yawAccel, 0.0f, scale * BTL_DEG(54.0f), 0.0f, 3.14159265f);
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.5f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, BTL_KMH(100.0f));
        } else {
            BtlMove_Step(chr, 6, 6, 3, speed, BTL_KMH(100.0f));
        }
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch * lean);
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x6B:
                break;
            case 0x6C:
                if (BtlInput_TestAction(chr, 0x41, 1)) {
                    BtlAnim_Request(chr, 0x6D, 0.0f);
                }
                break;
            case 0x6D:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    s32 follow[2] = { 0x87, 0x77 }; /* attack ids; the initialiser is D_002FEB20 (.sdata) */

                    for (i = 0; i < 2; i++) {
                        if (BtlDecide_QueueAttack(chr, follow[i])) {
                            *cancelAt = 0.4f;
                            break;
                        }
                    }
                    if (BtlDecide_Attack(chr, 1)) {
                        *cancelAt = 0.6f;
                    }
                    if (BtlAct_HasQueued(chr)) {
                        if (*cancelAt < BtlAnim_GetProgress(chr)) {
                            BtlAct_Request(chr, BtlAct_GetQueued(chr));
                        }
                    }
                }
                BtlAct_CancelOnOppC9(chr, 0.6f);
                break;
        }
    }
}

/* Action 0x59: rising attack that leaves flight (animations 0x6E wind-up, 0x6F hold, 0x70 strike). */
s32 BtlAct_LiftStrikeHandler(BtlActAChr *chr, s32 phase) {
    f32 *cancelAt = &chr->workF[0];
    BtlActAPose *pose;
    s32 falling;
    s32 steer;
    f32 speed;
    f32 turn;

    if (phase == 0) {
        pose = BtlChar_GetPos(chr);
        BtlAnim_Play(chr, 0x6E, 0.15f);
        BtlChar_ClearFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x2E);
        pose->vspeed = 0.0f;
        if (BtlChar_TestFlag(chr, 5) && __builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) < BtlMoveParam_GetAngle58(chr)) {
            turn = 3.14159265f;
            BtlMove_TurnYaw(chr, 2, turn);
            BtlMove_TurnPitch(chr, 1, turn);
            pose->pitch = BtlUtil_ClampF(pose->pitch, BTL_DEG(-81.0f), BTL_DEG(-27.0f));
            chr->work[0] |= 1;
        } else {
            BtlMove_SetHeading(chr, pose->yaw, BTL_DEG(-36.0f));
        }
    }
    if (phase == 1) {
        speed = BtlMoveParam_GetSpeed(chr, 4);
        falling = 0;
        steer = 1;
        switch (BtlAnim_GetId(chr)) {
            case 0x6E:
                BtlAnim_AdvanceThen(chr, 0x6F, 0.0f, 0);
                break;
            case 0x6F:
                if (BtlAnim_AdvanceThen(chr, 0x70, 0.0f, 0)) {
                    BtlCharSnd_PlayCommon(chr, 0x2F);
                    BtlChar_SetVibration(chr, 1.0f, 0.3f);
                }
                BtlAct_SetChargeFromAnim(chr);
                break;
            case 0x70:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                if (BtlAnim_IsNew(chr)) {
                    BtlAct_PlayPickedVoice(chr);
                }
                speed = 0.0f;
                falling = 1;
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    BtlChar_GetPos(chr)->speed = speed;
                } else if (BtlChar_TestFlag(chr, 0x5C) || BtlChar_TestFlag(chr, 0x5D)) {
                    steer = 0;
                    if (BtlMove_IsBlockedByOpponent(chr)) {
                        BtlChar_GetPos(chr)->speed = speed;
                    }
                }
                break;
        }
        BtlMove_Step(chr, ((chr->work[0] & 1) && steer) ? 2 : 6, 6, 3, speed, BTL_KMH(100.0f));
        if (falling && !BtlChar_TestFlag(chr, 0xE)) {
            BtlMove_Fall(chr);
        } else {
            BtlMove_BrakeVertical(chr);
        }
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x6E:
                break;
            case 0x6F:
                if (BtlInput_TestAction(chr, 0x41, 1)) {
                    BtlAnim_Request(chr, 0x70, 0.0f);
                }
                break;
            case 0x70:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    if (BtlParam_GetFlags2(chr) & 0x400000) {
                        BtlColl_AddActionBit(chr, 0x5F);
                        if (BtlInput_TestAction(chr, 0x50, 1)) {
                            BtlAct_SetQueue(chr, 0, 0x5F);
                            *cancelAt = 0.3f;
                        }
                    }
                    if (BtlDecide_Attack(chr, 1)) {
                        *cancelAt = 0.6f;
                    }
                    if (BtlAct_HasQueued(chr)) {
                        if (*cancelAt < BtlAnim_GetProgress(chr)) {
                            BtlAct_Request(chr, BtlAct_GetQueued(chr));
                        }
                    }
                }
                BtlAct_CancelOnOppC9(chr, 0.6f);
                break;
        }
    }
}

/* Actions 0x5A..0x5D: vanishing attack: disappear, reappear ahead of the opponent and strike. */
s32 BtlAct_SmashVanishHandler(BtlActAChr *chr, s32 phase) {
    s32 *anim = &chr->work[4];
    s32 *step = &chr->work[2];
    f32 *lead = &chr->workF[0];
    s32 *count = &chr->work[3];
    BtlActAObj *obj;
    s32 move;
    s32 a;
    f32 turn;

    if (phase == 0) {
        switch (BtlAct_GetCurrent(chr)) {
            case 0x5A:
                *anim = 0x65;
                break;
            case 0x5B:
                *anim = 0x68;
                break;
            case 0x5C:
                *anim = 0x5F;
                break;
            case 0x5D:
                *anim = 0x62;
                break;
        }
        switch (BtlAct_GetPrev(chr)) {
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
                BtlChar_PlayVoice(chr, 0x1D);
                break;
            default:
                BtlChar_PlayVoice(chr, 5);
                break;
        }
        BtlChar_SetFxBit(chr, 0xC);
        BtlCharSnd_PlayCommon(chr, 0x20);
        BtlChar_SetSmallVibration(chr, 0.2f);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->vspeed = 0.0f;
        BtlChar_SetHeldFlag(chr, 0xE);
        chr->unkD70++;
        chr->attackId = BtlAct_GetCurrent(chr);
    }
    if (phase == 1) {
        obj = BtlChar_GetObj(chr);
        switch (*step) {
            case 0:
                BtlAnim_Advance(chr, 0);
                (*count)++;
                if (*count == 1) {
                    BtlAnim_PlaySub(chr, *anim);
                    *lead += obj->unkBDC * 30.0f / 60.0f;
                }
                if (*count == 2) {
                    BtlAnim_PlaySub(chr, *anim + 2);
                    a = BtlObjAnim_QueryEvent(obj, 1, 1, 0);
                    a += BtlObjAnim_QueryEvent(obj, 2, 1, 0);
                    *lead += (f32)a * 0.5f * 30.0f / 60.0f;
                    *lead = floorf(*lead + 0.5f);
                }
                if (*count >= 4) {
                    BtlChar_SetFlag(chr, 0xB);
                }
                if (*count >= 8) {
                    *count = 0;
                    (*step)++;
                }
                break;
            case 1:
                BtlMove_SnapToOpponent(chr);
                BtlChar_SetFlag(chr, 0xB);
                BtlChar_SetFlag(chr, 0xCC);
                (*step)++;
                break;
            case 2:
                turn = 3.14159265f;
                BtlMove_WarpAheadOfOpponent(chr, *lead);
                BtlMove_TurnYaw(chr, 2, turn);
                BtlMove_TurnPitch(chr, 1, turn);
                BtlMove_TurnModelYaw(chr, turn, 1.0f);
                BtlMove_SetDirection(chr, 3);
                BtlChar_SetFlag(chr, 0x3F);
                BtlChar_SetFlag(chr, 0xB);
                BtlAnim_Request(chr, *anim, 0.0f);
                BtlChar_SetFxBit(chr, 0xD);
                BtlChar_SetFlag(chr, 0xCC);
                BtlChar_SetFlag(chr, 0x24);
                (*step)++;
                break;
            case 3:
                move = 1;
                switch (BtlAnim_GetId(chr)) {
                    case 0x5F:
                    case 0x62:
                    case 0x65:
                    case 0x68:
                        BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 2, 0.0f, 0);
                        chr->unkD78 = 1.0f;
                        if (BtlAnim_GetProgress(chr) < 0.7f) {
                            BtlChar_SetFlag(chr, 0xB);
                        }
                        if (BtlAnim_IsNew(chr)) {
                            BtlChar_SetFlag(chr, 0xCD);
                        }
                        break;
                    case 0x61:
                    case 0x64:
                    case 0x67:
                    case 0x6A:
                        if (BtlAnim_Advance(chr, 0)) {
                            switch (BtlOpp_GetSeenAction(chr)) {
                                case 0x89:
                                case 0x8A:
                                case 0x8B:
                                case 0x8C:
                                    BtlAct_Request(chr, 0xC);
                                    break;
                                default:
                                    BtlAct_Request(chr, 0xB);
                                    break;
                            }
                        }
                        BtlAnim_SetObjRate(chr, 3.0f);
                        if (BtlChar_TestFlag(chr, 0x5B)) {
                            chr->work[0] |= 1;
                        }
                        move = !BtlChar_TestFlag(chr, 0x5C);
                        break;
                    case 0x60:
                    case 0x63:
                    case 0x66:
                    case 0x69:
                        break;
                }
                if (move) {
                    BtlMove_Step(chr, 2, 1, 5, 0.0f, BTL_KMH(100.0f));
                } else {
                    BtlMove_Step(chr, 6, 6, 5, 0.0f, BTL_KMH(100.0f));
                }
                break;
        }
        BtlChar_SetFxBit(chr, 0x3A);
        BtlChar_SetFlag(chr, 0x89);
    }
    if (phase == 2) {
        if (chr->work[0] & 1) {
            if (BtlChar_TestFlag(chr, 0x5B)) {
                BtlChar_SetFlag(chr, 0x3D);
                if (BtlOpp_GetSeenAction(chr) == 0xD5) {
                    if (!BtlAct_HasQueued(chr)) {
                        BtlDecide_Attack(chr, 0x180000);
                    }
                    if (0.7f < BtlAnim_GetProgress(chr)) {
                        BtlAct_Request(chr, BtlAct_GetQueued(chr));
                    }
                }
            }
        }
    }
}

/* Actions 0x5E..0x66 (not 0x63): chargeable rush finishers (wind-up, hold, release animation triples). */
s32 BtlAct_RushFinishHandler(BtlActAChr *chr, s32 phase) {
    f32 *cancelAt = &chr->workF[0];
    BtlActAObj *obj = BtlChar_GetObj(chr);
    BtlActAPose *pose;
    s32 follow[8];
    s32 anim;
    s32 mode;
    s32 count;
    s32 i;
    f32 speed;
    f32 turn;

    if (phase == 0) {
        anim = 0;
        switch (BtlAct_GetCurrent(chr)) {
            case 0x5E:
                anim = 0x41;
                break;
            case 0x5F:
                anim = 0x44;
                break;
            case 0x60:
                anim = 0x47;
                break;
            case 0x61:
            case 0x62:
                anim = 0x90;
                break;
            case 0x66:
                anim = 0x199;
                break;
            case 0x64:
                anim = 0x56;
                BtlChar_ToggleHeldFlag(chr, 0x8F);
                break;
            case 0x65:
                anim = 0x59;
                BtlChar_ToggleHeldFlag(chr, 0x8F);
                break;
        }
        BtlAnim_Play(chr, anim, 0.1f);
        BtlMove_BeginRiseToOpponent(chr);
        BtlCharSnd_PlayCommon(chr, 0x2E);
        if (BtlAct_GetCurrent(chr) == 0x66) {
            chr->unk1354 = BtlAct_StartSub19B(chr);
        }
    }
    if (phase == 1) {
        speed = 0.0f;
        mode = 6;
        turn = speed;
        if (BtlChar_TestFlag(chr, 0x81)) {
            speed = BTL_KMH(50.0f);
            mode = 2;
            turn = BTL_DEG(1.8f);
        }
        switch (BtlAnim_GetId(chr)) {
            case 0x41:
            case 0x44:
            case 0x47:
            case 0x56:
            case 0x59:
            case 0x90:
            case 0x199:
                BtlAct_SetChargeAnimStep(chr);
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                break;
            case 0x42:
            case 0x45:
            case 0x48:
            case 0x57:
            case 0x5A:
            case 0x91:
            case 0x19A:
                BtlAct_SetChargeAnimStep(chr);
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlAct_SetChargeFromAnim(chr);
                if (BtlAct_GetCurrent(chr) == 0x66) {
                    if (BtlAnim_IsNew(chr)) {
                        BtlChar_SetFxBit(chr, 0x28);
                    }
                }
                break;
            case 0x43:
            case 0x46:
            case 0x49:
            case 0x58:
            case 0x5B:
            case 0x92:
            case 0x19B:
                speed = 0.0f;
                turn = speed;
                mode = 6;
                if (BtlAct_GetCurrent(chr) == 0x66) {
                    if (BtlAnim_IsNew(chr)) {
                        BtlAct_PlayPickedVoice(chr);
                        BtlChar_SetFxBit(chr, 0x29);
                    }
                    BtlChar_SetVibration(chr, 1.0f, 0.1f);
                    BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                    speed = BtlAtk_GetLaunchSpeed(chr);
                    if (BtlChar_TestFlag(chr, 0x13)) {
                        BtlChar_SetFlag(chr, 0xDC);
                    }
                } else {
                    if (BtlAnim_IsNew(chr)) {
                        BtlAct_PlayPickedVoice(chr);
                        pose = BtlChar_GetPos(chr);
                        pose->speed = BtlAtk_GetLaunchSpeed(chr);
                        if (0.99f < chr->unkD78) {
                            BtlChar_SetVibration(chr, 1.0f, 0.3f);
                        }
                    }
                    if (BtlAnim_Advance(chr, 0)) {
                        BtlAct_Request(chr, 0xB);
                    }
                }
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    if (BtlChar_TestFlag(chr, 0x5B) || BtlChar_TestFlag(chr, 0x5D)) {
                        turn = 3.14159265f;
                        mode = 2;
                    }
                }
                break;
            case 0x19C:
                if (BtlAnim_IsNew(chr)) {
                    BtlChar_SetFxBit(chr, 0x2A);
                }
                BtlAnim_SetDuration(chr, 0.5f);
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                speed = 0.0f;
                mode = 6;
                turn = speed;
                break;
        }
        BtlMove_TurnYaw(chr, mode, turn);
        BtlMove_TurnPitch(chr, 5, 3.14159265f);
        BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
        BtlMove_SetDirection(chr, 2);
        BtlMove_Advance(chr, speed, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x41:
            case 0x44:
            case 0x47:
            case 0x56:
            case 0x59:
            case 0x90:
            case 0x199:
                break;
            case 0x42:
            case 0x45:
            case 0x48:
            case 0x57:
            case 0x5A:
            case 0x91:
            case 0x19A:
                if (BtlInput_TestAction(chr, 0x43, 1)) {
                    BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                }
                break;
            case 0x43:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    if (BtlOpp_GetSeenAction(chr) == 0xCE) {
                        BtlColl_AddActionBit(chr, 0x34);
                        if (BtlInput_TestAction(chr, 0x2A, 1)) {
                            speed = (f32)BtlObjAnim_QueryEvent(obj, 1, 0, 0);
                            turn = BtlAnim_GetLength(chr);
                            speed += 20.0f;
                            speed /= turn;
                            *cancelAt = speed;
                            *cancelAt = BtlUtil_ClampF(*cancelAt, 0.2f, 0.8f);
                            BtlAct_SetQueue(chr, 0, 0x34);
                        }
                    }
                    if (BtlAct_HasQueued(chr)) {
                        if (*cancelAt < BtlAnim_GetProgress(chr)) {
                            BtlAct_Request(chr, BtlAct_GetQueued(chr));
                        }
                    }
                }
                BtlAct_CancelOnOppC9(chr, 0.6f);
                break;
            case 0x46:
            case 0x49:
            case 0x58:
            case 0x5B:
            case 0x92:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    count = 0;
                    switch (BtlAct_GetCurrent(chr)) {
                        case 0x5E:
                            break;
                        case 0x5F:
                            follow[0] = 0x80;
                            follow[1] = 0x70;
                            count = 2;
                            break;
                        case 0x60:
                            follow[0] = 0x7E;
                            follow[1] = 0x74;
                            count = 2;
                            break;
                        case 0x61:
                            if (0.99f < chr->unkD78) {
                                follow[0] = 0x73;
                                follow[1] = 0x96;
                                count = 2;
                            }
                            break;
                        case 0x62:
                            follow[0] = 0x82;
                            count = 1;
                            break;
                        case 0x64:
                            follow[0] = 0x99;
                            follow[1] = 0x85;
                            count = 2;
                            break;
                        case 0x65:
                            follow[0] = 0x99;
                            follow[1] = 0x84;
                            count = 2;
                            break;
                    }
                    for (i = 0; i < count; i++) {
                        if (BtlDecide_QueueAttack(chr, follow[i])) {
                            *cancelAt = 0.4f;
                            break;
                        }
                    }
                    if (BtlDecide_Attack(chr, 3)) {
                        *cancelAt = 0.6f;
                    }
                    if (BtlAct_HasQueued(chr)) {
                        if (*cancelAt < BtlAnim_GetProgress(chr)) {
                            BtlAct_Request(chr, BtlAct_GetQueued(chr));
                        }
                    }
                }
                BtlAct_CancelOnOppC9(chr, 0.6f);
                break;
            case 0x19B:
                break;
            case 0x19C:
                if (BtlChar_TestFlag(chr, 0x5B)) {
                    if (BtlDecide_QueueAttack(chr, !(BtlAtk_GetLaunchAngleBOf(chr, 0x86) < BTL_DEG(10.0f)) ? 0x93 : 0xA3)) {
                        *cancelAt = 0.9f;
                    }
                    if (BtlOpp_GetSeenAction(chr) != 0xD5) {
                        if (BtlDecide_Attack(chr, 3)) {
                            *cancelAt = 0.6f;
                        }
                    }
                    if (BtlAct_HasQueued(chr)) {
                        if (*cancelAt < BtlAnim_GetProgress(chr)) {
                            BtlAct_Request(chr, BtlAct_GetQueued(chr));
                        }
                    }
                }
                BtlAct_CancelOnOppC9(chr, 0.6f);
                break;
        }
    }
    if (phase == 3) {
        if (BtlAct_GetCurrent(chr) == 0x66) {
            BtlChar_SetFxBit(chr, 0x2A);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly btl_act_b.c), written against its own view of the fighter. Functions already declared above with
 * the first part's types are reached through cast macros.
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/btl_act_b.h"

/*
 * Fighter action handlers, second slice: 0x1E6CC0..0x1EA5F8.
 *
 * A slice of the same original source file as btl_char_action.c and btl_act_a.c (the float pool and the jump
 * tables run on, and BtlAct_AttackDashHandler only matches when BtlAct_RequestAttackEnd of the previous slice is
 * defined in the same translation unit). Handler protocol: include/battle/btl_char_action.h.
 *
 *   action            handler                         what it is (game terms are guesses)
 *   0x63              BtlAct_Action63                 animation 0x93, a rush finisher that is not chargeable
 *   0x67..0x69        BtlAct_Action67to69             held ki-spending pose (animation 0xF7) that chains into 0x6A / 0x9E..0xA1
 *   0x6A              BtlAct_Action6A                 1000 km/h lunge (animation 0xF8); on contact queues attack 0xA2
 *   0x6B..0x6F        BtlAct_Action6Bto6F             1500 km/h homing strikes (animations 0x67, 0x6A, 0x61, 0x64, 0x5E)
 *   0x70..0xAD        BtlAct_AttackHandler            the generic attack driven by the attack table
 *     except 0x92, 0x9B, 0xAB  BtlAct_AttackDashHandler        dash at the opponent (0x197 loop), then strike (0x198)
 *            0x99              BtlAct_AttackVolleyDashHandler  dash (0x2D, 0x19 loop) with flag 0x4F pulsed, then 0x1A
 *            0x95              (next slices)
 *   0xBD, 0xC0..0xCA  BtlAct_HitStaggerHandler        hit reactions that stay in place
 *   0xCB, 0xCC        BtlAct_ActionCBtoCC             long stagger shortened by mashing
 *   0xCD              BtlAct_GroundBounceHandler      on the ground after a fall (0xDC, 0xDD), then get up / roll
 *   0xCE              BtlAct_LaunchedHandler          knocked upward, then falling
 *   0xCF..0xD1        BtlAct_ActionCFtoD1             pushed back 500 km/h (0xD1: in place)
 *   0xD2              BtlAct_ActionD2                 animation 0xD9, then the down action
 *   0xD4              BtlAct_KnockBackHandler         knocked back 700 km/h for a time, then recovers
 *   0xD5, 0xD6, 0xDF  BtlAct_BlowAwayHandler          blown away 1000 / 2000 km/h until something is hit
 *   0xD7              BtlAct_BlowRecoverHandler       mashed out of being blown away
 *   0xD8              BtlAct_LieDownHandler           lying down / floating knocked out; the member change on death
 *   0xD9              BtlAct_FallHandler              falling helplessly to the ground
 */

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)

extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Normalize(Vec4 *out, Vec4 *in);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern s32 Battle_GetMode(void);
extern void BtlEvent_Raise(s32 side, s32 event);
extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
extern f32 BtlUtil_ApproachF(f32 cur, f32 target, f32 step);

#define BtlChar_GetObj ((void *(*)(BtlActBChr *chr))BtlChar_GetObj)
#define BtlChar_GetPos ((BtlActBPose *(*)(BtlActBChr *chr))BtlChar_GetPos)
extern s32 BtlChar_IsFree(BtlActBChr *chr);
extern s32 BtlChar_IsDead(BtlActBChr *chr);
extern s32 BtlChar_IsStage4Or27(void);
#define BtlChar_SetVibration ((void (*)(BtlActBChr *chr, f32 power, f32 seconds))BtlChar_SetVibration)
#define BtlChar_PlayVoice ((void (*)(BtlActBChr *chr, s32 kind))BtlChar_PlayVoice)
#define BtlChar_TestFlag ((s32 (*)(BtlActBChr *chr, s32 bit))BtlChar_TestFlag)
extern s32 BtlChar_TestPrevFlag(BtlActBChr *chr, s32 bit);
extern s32 BtlChar_IsFlagRaised(BtlActBChr *chr, s32 bit);
#define BtlChar_SetFlag ((void (*)(BtlActBChr *chr, s32 bit))BtlChar_SetFlag)
#define BtlChar_SetHeldFlag ((void (*)(BtlActBChr *chr, s32 bit))BtlChar_SetHeldFlag)
#define BtlChar_ClearFlag ((void (*)(BtlActBChr *chr, s32 bit))BtlChar_ClearFlag)
#define BtlChar_SetFxBit ((void (*)(BtlActBChr *chr, s32 bit))BtlChar_SetFxBit)
extern s32 BtlInput_IsHeld(BtlActBChr *chr, u32 mask);
extern s32 BtlInput_IsPressed(BtlActBChr *chr, u32 mask);
#define BtlInput_TestAction ((s32 (*)(BtlActBChr *chr, s32 id, s32 want))BtlInput_TestAction)
#define ChrCam_RequestCut ((void (*)(BtlActBChr *chr, s32 table, s32 index))ChrCam_RequestCut)
#define ChrCam_EndCut ((void (*)(BtlActBChr *chr))ChrCam_EndCut)

#define BtlAnim_Play ((void (*)(BtlActBChr *chr, s32 anim, f32 blend))BtlAnim_Play)
#define BtlAnim_Request ((void (*)(BtlActBChr *chr, s32 anim, f32 blend))BtlAnim_Request)
#define BtlAnim_SetStep ((void (*)(BtlActBChr *chr, f32 step))BtlAnim_SetStep)
#define BtlAnim_SetDuration ((void (*)(BtlActBChr *chr, f32 seconds))BtlAnim_SetDuration)
#define BtlAnim_SetObjRate ((void (*)(BtlActBChr *chr, f32 rate))BtlAnim_SetObjRate)
#define BtlAnim_GetId ((s32 (*)(BtlActBChr *chr))BtlAnim_GetId)
#define BtlAnim_GetProgress ((f32 (*)(BtlActBChr *chr))BtlAnim_GetProgress)
#define BtlAnim_Advance ((s32 (*)(BtlActBChr *chr, s32 flags))BtlAnim_Advance)
/* The float comes before the flags here: only this order reproduces the argument set-up at 0x1E9378 / 0x1E93C8
   (btl_char_status.h declares it (chr, next, flags, blend); the registers are the same either way). */
#define BtlAnim_AdvanceThen ((s32 (*)(BtlActBChr *chr, s32 next, f32 blend, s32 flags))BtlAnim_AdvanceThen)
#define BtlAnim_AdvanceLoop ((void (*)(BtlActBChr *chr, s32 flags))BtlAnim_AdvanceLoop)
#define BtlAnim_IsNew ((s32 (*)(BtlActBChr *chr))BtlAnim_IsNew)

#define BtlMove_Step ((void (*)(BtlActBChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel))BtlMove_Step)
#define BtlMove_ApplyGravity ((void (*)(BtlActBChr *chr))BtlMove_ApplyGravity)
#define BtlMove_Fall ((void (*)(BtlActBChr *chr))BtlMove_Fall)
extern void BtlMove_MoveVertical(BtlActBChr *chr, f32 speed, f32 accel);
#define BtlMove_BrakeVertical ((void (*)(BtlActBChr *chr))BtlMove_BrakeVertical)
#define BtlMove_SetLeanX ((void (*)(BtlActBChr *chr, f32 v))BtlMove_SetLeanX)
extern void BtlMove_SetLeanZ(BtlActBChr *chr, f32 v);
#define BtlMove_IsBlockedByOpponent ((s32 (*)(BtlActBChr *chr))BtlMove_IsBlockedByOpponent)
#define BtlMove_BeginRiseToOpponent ((void (*)(BtlActBChr *chr))BtlMove_BeginRiseToOpponent)
extern void BtlMove_DeflectAtStageLimit(BtlActBChr *chr, s32 back);
extern s32 BtlMove_IsHeadingIntoFloor(BtlActBChr *chr, f32 angle);
extern s32 BtlMove_IsHeadingIntoWall(BtlActBChr *chr, f32 angle);

extern s32 BtlMember_CountAlive(BtlActBChr *chr);
extern s32 BtlMember_HasKi(BtlActBChr *chr, s32 amount);
#define BtlMember_SpendKi ((s32 (*)(BtlActBChr *chr, s32 amount, s32 force))BtlMember_SpendKi)
extern s32 BtlMember_IsKiEmpty(BtlActBChr *chr);
extern f32 BtlMember_GetHealthRatio(BtlActBChr *chr);

#define BtlCharSnd_PlayCommon ((void (*)(BtlActBChr *chr, s32 sound))BtlCharSnd_PlayCommon)
extern void BtlCharSnd_PlayBank8(BtlActBChr *chr, s32 id);
extern void BtlCharSnd_StopCommon(BtlActBChr *chr, s32 id);
#define BtlOpp_GetSeenAction ((s32 (*)(BtlActBChr *chr))BtlOpp_GetSeenAction)
extern s32 BtlOpp_GetSeenUnk964(BtlActBChr *chr);
extern f32 BtlOpp_GetPitchAdjusted(BtlActBChr *chr);
#define BtlOpp_GetYawFromFacing ((f32 (*)(BtlActBChr *chr))BtlOpp_GetYawFromFacing)
#define BtlColl_AddActionBit ((void (*)(BtlActBChr *chr, s32 action))BtlColl_AddActionBit)

#define BtlAct_Request ((void (*)(BtlActBChr *chr, s32 id))BtlAct_Request)
#define BtlAct_HasQueued ((s32 (*)(BtlActBChr *chr))BtlAct_HasQueued)
#define BtlAct_GetCurrent ((s32 (*)(BtlActBChr *chr))BtlAct_GetCurrent)
#define BtlAct_GetPrev ((s32 (*)(BtlActBChr *chr))BtlAct_GetPrev)
#define BtlAct_GetQueued ((s32 (*)(BtlActBChr *chr))BtlAct_GetQueued)
extern f32 BtlAct_GetGroundY(BtlActBChr *chr);
#define BtlAct_GetHeight ((f32 (*)(BtlActBChr *chr))BtlAct_GetHeight)
extern f32 BtlAct_GetFramesToGround(BtlActBChr *chr);
extern s32 BtlAct_IsAirMotion(BtlActBChr *chr, s32 useSaved);
extern s32 BtlAct_IsAttackId(s32 id);
extern void BtlAct_LatchAttack(BtlActBChr *chr);

/* Handlers' shared helpers in the previous slice (0x1E3158..0x1E6CC0). */
#define BtlAct_PlayPickedVoice ((void (*)(BtlActBChr *chr))BtlAct_PlayPickedVoice)                          /* voice kind BtlAtk_GetVoiceKind picks, if any */
#define BtlAct_DecideAttackFollow ((void (*)(BtlActBChr *chr, BtlActBAttack *atk))BtlAct_DecideAttackFollow)   /* decide phase of an attack: follow-up inputs */
/* Ends an attack: action 0xB, or 0xC under flag 0x5C when the attack has flag 0x400. */
#define BtlAct_RequestAttackEnd ((void (*)(BtlActBChr *chr, BtlActBAttack *atk))BtlAct_RequestAttackEnd)

/* Decision functions and parameter readers (0x2013E0..0x20F4E8). */
#define BtlDecide_Main ((s32 (*)(BtlActBChr *chr, u32 mask))BtlDecide_Main)          /* main input table; 1 = something was queued */
#define BtlDecide_Attack ((s32 (*)(BtlActBChr *chr, u32 mask))BtlDecide_Attack)        /* attack input table */
#define BtlDecide_QueueAttack ((s32 (*)(BtlActBChr *chr, s32 attack))BtlDecide_QueueAttack) /* queue an attack table record */
extern s32 BtlAct_GetChainAction(BtlActBChr *chr, s32 kind);
#define BtlAtk_GetLaunchSpeed ((f32 (*)(BtlActBChr *chr))BtlAtk_GetLaunchSpeed)
extern s32 BtlParam_GetUnk88(BtlActBChr *chr, s32 which);
extern s32 BtlParam_GetKiDrain67(BtlActBChr *chr);
#define BtlParam_GetDashSound ((s32 (*)(BtlActBChr *chr))BtlParam_GetDashSound)

extern s32 BtlAct_GetIdleFollowUp(BtlActBChr *chr); /* 0xB unless a decision queues something: how the damage actions end */
extern s32 BtlAct_GetDownAction(BtlActBChr *chr); /* 0xE0 while chr + 0xFE0 counts down, else 0xD8 */
extern s32 BtlParam_CanFly(BtlActBChr *chr); /* 1 unless parameter flag 0x1000 without ability 0x36 */
extern f32 BtlCharApi_GetHeight(s32 objId); /* body size of the object */
extern s32 BtlAct_QueueFlag21Action(BtlActBChr *chr); /* flags 0x21 / 0x22: queue air recovery 0x25 / 0x27 / 0x28; 1 = queued */
extern s32 BtlAct_QueueFlag21ActionB(BtlActBChr *chr); /* the same against a wall: 0x29 / 0x2A */
#define BtlParam_GetFlags2 ((s32 (*)(BtlActBChr *chr))BtlParam_GetFlags2) /* parameter +0x14, skill bits */
extern s32 BtlParam_GetRecoverKiCost(BtlActBChr *chr); /* ki an air recovery costs */
extern s32 BtlMember_Damage(BtlActBChr *chr, s32 amount, s32 flags);
extern void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 arg3, f32 arg4);
extern void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
extern BtlActBGauge *BtlMember_GetActiveGauge(BtlActBChr *chr);
extern s32 BtlAct_CheckRecoveryInput(BtlActBChr *chr, s32 arg); /* get-up input of a downed fighter: queues 0xE1..0xE6 */
extern f32 BtlStage_GetBottom(void); /* stage: height below which a fighter is out of the arena (inferred) */

/* Action 0x63: animation 0x93 after taking off toward the opponent, with the attack voice; attack inputs (mask 3)
   are read all through and the queued action starts once the animation is past 90%. Ends in 0xB. */
s32 BtlAct_Action63(BtlActBChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0x93, 0.1f);
        BtlMove_BeginRiseToOpponent(chr);
        BtlAct_PlayPickedVoice(chr);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        BtlDecide_Attack(chr, 3);
        if (BtlAnim_GetProgress(chr) > 0.9f) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/*
 * Actions 0x70..0xAD except 0x92, 0x95, 0x99, 0x9B, 0xAB: the generic attack, driven by the attack table record that
 * BtlAct_PrepareAttack turned into the block at chr + 0xD1C when the attack was queued.
 *
 * Enter: latch that block to chr + 0xCF8 (BtlAct_LatchAttack), play its first animation (blend 0.15 s), take off toward
 * the opponent, set the advance speed from the record (BtlAtk_GetLaunchSpeed), play the attack voice, raise flag 0xCD
 * for attack flag 0x80. A record with a camera cut (cut >= 0) requests it with ChrCam_RequestCut(chr, 1, cut) and
 * raises flag 0x27, but only when the opponent is not seen in an attack action itself, or is seen in one that it
 * entered after this fighter did (its action frame count is greater): of two attackers the earlier one gets the cut.
 * Levels 1..3 request effect bit 0xD.
 *
 * Run: chr + 0xD78 = 1.0. work[2] counts the parts played: while a further part follows, the animation runs at twice
 * the record's rate and chains to the next part's animation; the last part ends the attack through
 * BtlAct_RequestAttackEnd. Movement: with a record speed the fighter homes on the opponent (yaw mode 2, pitch mode 1)
 * at that speed with unlimited acceleration; without one, or once the attack is guarded / clashes (flags 0x5C,
 * 0x5D), it brakes at 100 km/h per frame (modes 6, 6). A landed hit (flag 0x5B) zeroes the speed at once, and so does
 * being blocked by the opponent's body. Every frame: gravity, object rate 2.0, flag 0xD5, effect bit 0x3A. With attack
 * flag 0x40 the camera cut ends on the frame the hit lands (rising edge of 0x5B).
 *
 * Decide: BtlAct_DecideAttackFollow (follow-up inputs of the record).
 */
s32 BtlAct_AttackHandler(BtlActBChr *chr, s32 phase) {
    BtlActBAttack *atk = &chr->attack;
    s32 *part = &chr->work[2];
    BtlActBPose *pos;
    s32 cut;
    s32 homing;
    f32 speed;
    f32 accel;

    if (phase == 0) {
        BtlAct_LatchAttack(chr);
        BtlAnim_Play(chr, atk->motion[0], 0.15f);
        BtlMove_BeginRiseToOpponent(chr);
        pos = BtlChar_GetPos(chr);
        pos->speed = BtlAtk_GetLaunchSpeed(chr);
        BtlAct_PlayPickedVoice(chr);
        if (atk->flags & 0x80) {
            BtlChar_SetFlag(chr, 0xCD);
        }
        if (atk->cut >= 0) {
            cut = 0;
            if (!BtlAct_IsAttackId(BtlOpp_GetSeenAction(chr)) || chr->actionFrame < BtlOpp_GetSeenUnk964(chr)) {
                cut = 1;
            }
            if (cut) {
                ChrCam_RequestCut(chr, 1, atk->cut);
                BtlChar_SetFlag(chr, 0x27);
            }
        }
        switch (atk->level) {
            case 1:
            case 2:
            case 3:
                BtlChar_SetFxBit(chr, 0xD);
                break;
        }
    }
    if (phase == 1) {
        chr->unkD78 = 1.0f;
        if (*part < atk->parts - 1) {
            BtlAnim_SetStep(chr, atk->rate * 2.0f);
            if (BtlAnim_AdvanceThen(chr, atk->motion[*part + 1], 0.0f, 0)) {
                (*part)++;
            }
        } else if (BtlAnim_Advance(chr, 0)) {
            BtlAct_RequestAttackEnd(chr, atk);
        }
        if (atk->speed < 0.0001f) {
            speed = 0.0f;
            accel = BTL_KMH(100.0f);
            homing = 0;
        } else {
            speed = atk->speed;
            accel = 10000.0f;
            homing = 1;
        }
        if (BtlChar_TestFlag(chr, 0x5B)) {
            speed = 0.0f;
            BtlChar_GetPos(chr)->speed = speed;
        }
        if (BtlChar_TestFlag(chr, 0x5C) || BtlChar_TestFlag(chr, 0x5D)) {
            speed = 0.0f;
            accel = BTL_KMH(100.0f);
            homing = 0;
            if (BtlMove_IsBlockedByOpponent(chr)) {
                BtlChar_GetPos(chr)->speed = speed;
            }
        }
        if (homing) {
            BtlMove_Step(chr, 2, 1, 3, speed, accel);
        } else {
            BtlMove_Step(chr, 6, 6, 3, speed, accel);
        }
        BtlMove_ApplyGravity(chr);
        BtlAnim_SetObjRate(chr, 2.0f);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFxBit(chr, 0x3A);
        if (atk->flags & 0x40) {
            if (BtlChar_IsFlagRaised(chr, 0x5B)) {
                if (atk->cut >= 0) {
                    ChrCam_EndCut(chr);
                }
            }
        }
    }
    if (phase == 2) {
        BtlAct_DecideAttackFollow(chr, atk);
    }
}

/* Actions 0x67..0x69: animation 0xF7 with voice 5, spending ki every frame (BtlParam_GetKiDrain67) and rumbling the
   pad. When the animation ends: guard (0x38) if the ki ran out, idle (0xB) if input condition 0x22 no longer holds.
   Flag 0x70 chains into the action BtlAct_GetChainAction picks from the character parameter (entry 0 for 0x67 / 0x68,
   entry 1 for 0x69): 0x6A or attack 0x9E..0xA1. Action 0x67 raises flag 0x4E in its first ten frames. */
s32 BtlAct_Action67to69(BtlActBChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xF7, 0.15f);
        BtlChar_PlayVoice(chr, 5);
    }
    if (phase == 1) {
        BtlMember_SpendKi(chr, BtlParam_GetKiDrain67(chr), 0);
        if (BtlAnim_Advance(chr, 0)) {
            if (BtlMember_IsKiEmpty(chr)) {
                BtlAct_Request(chr, 0x38);
            }
            if (BtlInput_TestAction(chr, 0x22, 0)) {
                BtlAct_Request(chr, 0xB);
            }
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetVibration(chr, 0.7f, 0.1f);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlAct_GetCurrent(chr) == 0x67 && chr->actionFrame < 10) {
            BtlChar_SetFlag(chr, 0x4E);
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x70)) {
            switch (BtlAct_GetCurrent(chr)) {
                case 0x67:
                case 0x68:
                    BtlAct_Request(chr, BtlAct_GetChainAction(chr, BtlParam_GetUnk88(chr, 0)));
                    break;
                case 0x69:
                    BtlAct_Request(chr, BtlAct_GetChainAction(chr, BtlParam_GetUnk88(chr, 1)));
                    break;
            }
        }
    }
}

/* Action 0x6A: animation 0xF8 starting at 1000 km/h toward the opponent and braking; once the hit lands (flag 0x5B)
   attack 0xA2 is queued and starts when the animation is past 60%. Ends in 0xB. */
s32 BtlAct_Action6A(BtlActBChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xF8, 0.15f);
        BtlChar_GetPos(chr)->speed = BTL_KMH(1000.0f);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        chr->unkD78 = 1.0f;
        BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x5B)) {
            BtlDecide_QueueAttack(chr, 0xA2);
            if (BtlAnim_GetProgress(chr) > 0.6f) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            }
        }
    }
}

/*
 * Actions 0x92, 0x9B, 0xAB: attacks that dash first. Enter: latch the attack, animation 0x197, voice, flight mode,
 * the dash sound, held flag 0x4F, camera cut (not conditional here). Run: 0x197 loops while the fighter homes at the
 * record's speed (effect bit 9); 0x198 is the strike, played in 0.3 s with flag 0x3F, braking. The lean follows the
 * pitch to the opponent within 45 degrees of the heading. Decide: the dash ends after 21 frames (speed halved) or on
 * contact (flags 0x5B / 0x5D, speed zeroed): animation 0x198, flag 0x4F cleared; the strike then takes the follow-up
 * inputs. Leave: flag 0x4F cleared. This handler and the next one are void in the original (their last call is a
 * tail call).
 *
 * Matches only with BtlAct_RequestAttackEnd (0x1E38F0, first part of this file) DEFINED earlier in the same
 * translation unit: with a declaration alone the beqz at 0x1E7568 comes out as beqzl. That is why the two parts
 * are one file.
 */
void BtlAct_AttackDashHandler(BtlActBChr *chr, s32 phase) {
    BtlActBAttack *atk = &chr->attack;
    s32 *frames = &chr->work[2];
    f32 lean;
    f32 accel;
    f32 speed;
    f32 pitch;
    s32 end;

    if (phase == 0) {
        BtlAct_LatchAttack(chr);
        BtlAnim_Play(chr, 0x197, 0.0f);
        BtlAct_PlayPickedVoice(chr);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
        BtlChar_SetHeldFlag(chr, 0x4F);
        if (atk->flags & 0x80) {
            BtlChar_SetFlag(chr, 0xCD);
        }
        if (atk->cut >= 0) {
            ChrCam_RequestCut(chr, 1, atk->cut);
        }
        switch (atk->level) {
            case 1:
            case 2:
            case 3:
                BtlChar_SetFxBit(chr, 0xD);
                break;
        }
    }
    if (phase == 1) {
        lean = 0.0f;
        accel = BTL_KMH(100.0f);
        speed = 0.0f;
        switch (BtlAnim_GetId(chr)) {
            case 0x197:
                BtlAnim_AdvanceLoop(chr, 0);
                speed = atk->speed;
                lean = 1.0f;
                accel = 10000.0f;
                BtlChar_SetFxBit(chr, 9);
                break;
            case 0x198:
                BtlAnim_SetDuration(chr, 0.3f);
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_RequestAttackEnd(chr, atk);
                }
                BtlChar_SetFlag(chr, 0x3F);
                lean = 1.0f - BtlAnim_GetProgress(chr);
                break;
        }
        BtlMove_Step(chr, 6, 6, 3, speed, accel);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0xD5);
        pitch = BtlChar_GetPos(chr)->pitch;
        BtlMove_SetLeanX(chr, BtlUtil_ClampF(BtlOpp_GetPitchAdjusted(chr), pitch - BTL_DEG(45.0f), pitch + BTL_DEG(45.0f)) * lean);
        BtlChar_SetFlag(chr, 0x28);
        BtlChar_SetFxBit(chr, 0x3A);
        if (atk->flags & 0x40) {
            if (BtlChar_IsFlagRaised(chr, 0x5B)) {
                if (atk->cut >= 0) {
                    ChrCam_EndCut(chr);
                }
            }
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x197:
                end = 0;
                if (++*frames > 20) {
                    end = 1;
                    BtlChar_GetPos(chr)->speed *= 0.5f;
                }
                if (BtlChar_TestFlag(chr, 0x5B) || BtlChar_TestFlag(chr, 0x5D)) {
                    end = 1;
                    BtlChar_GetPos(chr)->speed = 0.0f;
                }
                if (end) {
                    BtlAnim_Request(chr, 0x198, 0.0f);
                    BtlChar_ClearFlag(chr, 0x4F);
                }
                break;
            case 0x198:
                BtlAct_DecideAttackFollow(chr, atk);
                break;
        }
    }
    if (phase == 3) {
        BtlChar_ClearFlag(chr, 0x4F);
    }
}

/* Action 0x99: an attack with a longer dash. Animation 0x2D leads into the dash loop 0x19 (homing at the record's
   speed, effect bits 9 and 4, the dash sound and held flag 0x4F when the loop starts), 0x1A is the strike (0.3 s).
   Decide, in the loop: whenever flag 0x4F is found cleared (nothing in this handler clears it during the loop) it is set again two frames later, at most
   three times; the fourth time, after 24 frames, or when guarded (flag 0x5C) the strike starts. The strike then takes
   the follow-up inputs. Leave: flag 0x4F cleared. */
void BtlAct_AttackVolleyDashHandler(BtlActBChr *chr, s32 phase) {
    BtlActBAttack *atk = &chr->attack;
    s32 *pause = &chr->work[3];
    s32 *bursts = &chr->work[4];
    s32 *frames = &chr->work[2];
    f32 speed;
    f32 accel;
    f32 lean;
    s32 end;
    s32 more;

    if (phase == 0) {
        BtlAct_LatchAttack(chr);
        BtlAnim_Play(chr, 0x2D, 0.15f);
        BtlAct_PlayPickedVoice(chr);
        BtlChar_SetVibration(chr, 1.0f, 0.3f);
        BtlChar_SetHeldFlag(chr, 0xE);
        if (atk->flags & 0x80) {
            BtlChar_SetFlag(chr, 0xCD);
        }
        if (atk->cut >= 0) {
            ChrCam_RequestCut(chr, 1, atk->cut);
        }
        switch (atk->level) {
            case 1:
            case 2:
            case 3:
                BtlChar_SetFxBit(chr, 0xD);
                break;
        }
    }
    if (phase == 1) {
        speed = 0.0f;
        accel = BTL_KMH(100.0f);
        lean = 0.0f;
        switch (BtlAnim_GetId(chr)) {
            case 0x2D:
                BtlAnim_AdvanceThen(chr, 0x19, 0.1f, 0);
                speed = 0.0f;
                break;
            case 0x19:
                BtlAnim_AdvanceLoop(chr, 0);
                speed = atk->speed;
                accel = 10000.0f;
                lean = 1.0f;
                if (BtlAnim_IsNew(chr)) {
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                    BtlChar_SetHeldFlag(chr, 0x4F);
                }
                BtlChar_SetFxBit(chr, 9);
                BtlChar_SetFxBit(chr, 4);
                break;
            case 0x1A:
                BtlAnim_SetDuration(chr, 0.3f);
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_RequestAttackEnd(chr, atk);
                }
                speed = 0.0f;
                lean = 1.0f - BtlAnim_GetProgress(chr);
                BtlChar_ClearFlag(chr, 0x4F);
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    BtlChar_GetPos(chr)->speed = speed;
                }
                break;
        }
        BtlMove_Step(chr, 2, 1, 3, speed, accel);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0xD5);
        BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch * lean);
        if (atk->flags & 0x40) {
            if (BtlChar_IsFlagRaised(chr, 0x5B)) {
                if (atk->cut >= 0) {
                    ChrCam_EndCut(chr);
                }
            }
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x19:
                end = 0;
                if (!BtlChar_TestFlag(chr, 0x4F)) {
                    if (*pause > 0) {
                        if (--*pause <= 0) {
                            BtlChar_SetHeldFlag(chr, 0x4F);
                        }
                    } else {
                        *pause = 2;
                        more = ++*bursts < 4;
                        end = !more;
                    }
                }
                if (++*frames > 23) {
                    end = 1;
                }
                if (BtlChar_TestFlag(chr, 0x5C)) {
                    end = 1;
                }
                if (end) {
                    BtlAnim_Request(chr, 0x1A, 0.15f);
                }
                break;
            case 0x1A:
                BtlAct_DecideAttackFollow(chr, atk);
                break;
            case 0x2D:
                break;
        }
    }
    if (phase == 3) {
        BtlChar_ClearFlag(chr, 0x4F);
    }
}

/* Actions 0x6B..0x6F: one strike each (animations 0x67, 0x6A, 0x61, 0x64, 0x5E) homing on the opponent at 1500 km/h
   with flags 0x89 and 9, camera cut 0x20 when chr + 0xD68 is positive. Once the hit lands, flag 0x3D, and by the
   action the opponent is seen in: 0xD5 (blown away) takes attack inputs 0x180000 and follows up past 70%; 0xC6 takes
   main input 0x8000 and attack input 1 and follows up past 60%. Ends in 0xB. */
s32 BtlAct_Action6Bto6F(BtlActBChr *chr, s32 phase) {
    s32 anim;
    s32 homing;
    f32 speed;
    f32 accel;

    if (phase == 0) {
        anim = 0;
        switch (BtlAct_GetCurrent(chr)) {
            case 0x6B:
                anim = 0x67;
                break;
            case 0x6C:
                anim = 0x6A;
                break;
            case 0x6D:
                anim = 0x61;
                break;
            case 0x6E:
                anim = 0x64;
                break;
            case 0x6F:
                anim = 0x5E;
                break;
        }
        BtlAnim_Play(chr, anim, 0.1f);
        if (chr->unkD68 > 0) {
            ChrCam_RequestCut(chr, 1, 0x20);
        }
        BtlChar_SetVibration(chr, 1.0f, 0.3f);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        speed = BTL_KMH(1500.0f);
        accel = 10000.0f;
        homing = 1;
        if (BtlChar_TestFlag(chr, 0x5B)) {
            speed = 0.0f;
            BtlChar_GetPos(chr)->speed = speed;
        }
        if (BtlChar_TestFlag(chr, 0x5C) || BtlChar_TestFlag(chr, 0x5D)) {
            speed = 0.0f;
            accel = BTL_KMH(100.0f);
            homing = 0;
            if (BtlMove_IsBlockedByOpponent(chr)) {
                BtlChar_GetPos(chr)->speed = speed;
            }
        }
        if (homing) {
            BtlMove_Step(chr, 2, 1, 3, speed, accel);
        } else {
            BtlMove_Step(chr, 6, 6, 3, speed, accel);
        }
        BtlMove_ApplyGravity(chr);
        BtlAnim_SetObjRate(chr, 2.0f);
        BtlChar_SetFlag(chr, 0x89);
        BtlChar_SetFlag(chr, 9);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x5B)) {
            BtlChar_SetFlag(chr, 0x3D);
            switch (BtlOpp_GetSeenAction(chr)) {
                case 0xD5:
                    if (!BtlAct_HasQueued(chr)) {
                        BtlDecide_Attack(chr, 0x180000);
                    }
                    if (BtlAnim_GetProgress(chr) > 0.7f) {
                        BtlAct_Request(chr, BtlAct_GetQueued(chr));
                    }
                    break;
                case 0xC6:
                    if (!BtlAct_HasQueued(chr)) {
                        BtlDecide_Main(chr, 0x8000);
                        BtlDecide_Attack(chr, 1);
                    }
                    if (BtlAnim_GetProgress(chr) > 0.6f) {
                        BtlAct_Request(chr, BtlAct_GetQueued(chr));
                    }
                    break;
            }
        }
    }
}

/* Steps the fighter's variant counter (chr + 0xFB8) and returns it modulo n; 0 when n < 2. Cycles the stagger
   animations so the same one does not play twice in a row; not random. */
s32 BtlActB_NextVariant(BtlActBChr *chr, s32 n) {
    if (n < 2) {
        return 0;
    }
    chr->animCycle++;
    chr->animCycle %= n;
    return chr->animCycle;
}

/* Flags every damage action raises each frame: 0xD5, 0x95, 0x96, and flight mode (0xE) while in water (flag 0x11). */
void BtlActB_SetReactionFlags(BtlActBChr *chr) {
    BtlChar_SetFlag(chr, 0xD5);
    BtlChar_SetFlag(chr, 0x95);
    BtlChar_SetFlag(chr, 0x96);
    if (BtlChar_TestFlag(chr, 0x11)) {
        BtlChar_SetHeldFlag(chr, 0xE);
    }
}

/* Picks the animation of a downed fighter: 0xD7 / 0xD8 floating in water (flag 0x11), 0xD2 / 0xD3 while still moving
   faster than 300 km/h, else 0xD5 / 0xD6; the second of each pair for an "air" animation set. The second parameter
   (the current animation at both call sites) is not used. */
s32 BtlActB_PickDownAnim(BtlActBChr *chr, s32 cur) {
    s32 air = BtlAct_IsAirMotion(chr, 0);
    s32 id;

    if (BtlChar_TestFlag(chr, 0x11)) {
        id = air ? 0xD8 : 0xD7;
    } else if (Vec3_Length(&BtlChar_GetPos(chr)->moved) > BTL_KMH(300.0f)) {
        id = air ? 0xD3 : 0xD2;
    } else {
        id = air ? 0xD6 : 0xD5;
    }
    return id;
}

/* Animation step of a stagger the player can mash out of: each press of any of the four action buttons (bit 20)
   adds `add` to the step, kept in 1..4, and the step drifts toward 2 * add by 4 per second. add is 1, or with
   `scaled` past 20% of the animation 0.5 + 0.5 * (1 - chr + 0xFC8). Raises flag 0xE7 while the fighter is free. */
void BtlActB_UpdateMashRate(BtlActBChr *chr, f32 *rate, s32 scaled) {
    f32 add = 1.0f;
    f32 target;

    if (scaled) {
        if (BtlAnim_GetProgress(chr) > 0.2f) {
            add = (1.0f - chr->unkFC8) * 0.5f + 0.5f;
        }
    }
    target = add * 2.0f;
    if (BtlChar_IsFree(chr)) {
        BtlChar_SetFlag(chr, 0xE7);
        if (BtlInput_IsPressed(chr, 0x100000)) {
            *rate += add;
        }
    }
    *rate = BtlUtil_ClampF(*rate, 1.0f, 4.0f);
    BtlAnim_SetStep(chr, *rate);
    *rate = BtlUtil_ApproachF(*rate, target, 4.0f / 30.0f);
}

/* Whether a knocked-out fighter is to be replaced by its next member: under the fight state (flag 3), dead, flag 0xAF
   clear, a member left alive (and in mode 1 only with flag 0x10E). With a counter it answers 1 from the 31st call.
   The last two tests have to share the final `return 0` (written as early returns the compiler turns the counter
   test into slti / sltiu; with one `return 0` reached from two places it keeps the branches). */
s32 BtlActB_TickMemberChange(BtlActBChr *chr, s32 *frames) {
    if (Battle_GetMode() == 1 && !BtlChar_TestFlag(chr, 0x10E)) {
        return 0;
    }
    if (!BtlChar_TestFlag(chr, 3)) {
        return 0;
    }
    if (!BtlChar_IsDead(chr)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xAF)) {
        return 0;
    }
    if (BtlMember_CountAlive(chr) > 0) {
        if (frames == NULL || ++*frames > 30) {
            return 1;
        }
    }
    return 0;
}

/* Actions 0xBD, 0xC0..0xCA: hit reactions that stay where they are. One animation per action, picked on enter with
   the saved ground / air kind: 0xC0 0x9E..0xA1 (air 0xA2..0xA5) and 0xC1 0xA6..0xA9 (air 0xAA..0xAD) cycling through
   four variants, 0xC2..0xC5 0xB3 / 0xB4 / 0xB0 / 0xB2 (air 0xB1), 0xC6 0xB5 / 0xB6 alternating (air 0xB1), 0xC7 0xE0,
   0xC8 0xE1, 0xC9 0xB8, 0xCA 0xDF, 0xBD 0x9C. Ends in BtlAct_GetIdleFollowUp; flag 0x31, or main inputs 0x10000000
   (close to the opponent) and 0x840000, cut it short. On the ground above 200 km/h it requests effect bit 0x32.
   Leaves with flag 0x33 and chr + 0xFF0 = 15. */
s32 BtlAct_HitStaggerHandler(BtlActBChr *chr, s32 phase) {
    s32 anim;
    s32 air;

    if (phase == 0) {
        anim = 0;
        air = BtlAct_IsAirMotion(chr, 1);
        switch (BtlAct_GetCurrent(chr)) {
            case 0xC0:
                anim = BtlActB_NextVariant(chr, 4) + (air ? 0xA2 : 0x9E);
                break;
            case 0xC1:
                anim = BtlActB_NextVariant(chr, 4) + (air ? 0xAA : 0xA6);
                break;
            case 0xC2:
                anim = air ? 0xB1 : 0xB3;
                break;
            case 0xC3:
                anim = air ? 0xB1 : 0xB4;
                break;
            case 0xC4:
                anim = air ? 0xB1 : 0xB0;
                break;
            case 0xC5:
                anim = air ? 0xB1 : 0xB2;
                break;
            case 0xC6:
                anim = air ? 0xB1 : BtlActB_NextVariant(chr, 2) + 0xB5;
                break;
            case 0xC7:
                anim = 0xE0;
                break;
            case 0xC8:
                anim = 0xE1;
                break;
            case 0xC9:
                anim = 0xB8;
                break;
            case 0xCA:
                anim = 0xDF;
                break;
            case 0xBD:
                anim = 0x9C;
                break;
        }
        BtlAnim_Play(chr, anim, 0.0f);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, BtlAct_GetIdleFollowUp(chr));
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFlag(chr, 0x41);
        if (BtlChar_TestFlag(chr, 0xF)) {
            if (Vec3_Length(&BtlChar_GetPos(chr)->moved) > BTL_KMH(200.0f)) {
                BtlChar_SetFxBit(chr, 0x32);
            }
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x31)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlChar_TestFlag(chr, 0x13)) {
            if (BtlDecide_Main(chr, 0x10000000)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            }
        }
        if (BtlDecide_Main(chr, 0x840000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x33);
        chr->unkFF0 = 15;
    }
}

/* Actions 0xCB, 0xCC: animation 0xAE (air 0xAF) played at the mash rate (BtlActB_UpdateMashRate, scaled for 0xCB),
   starting at 2; 0xCC leans the model back and straightens it as the animation goes. Ends in BtlAct_GetIdleFollowUp;
   main inputs 0x10000000 / 0x800000 cut it short. Leaves with chr + 0xFF0 = 15. */
s32 BtlAct_ActionCBtoCC(BtlActBChr *chr, s32 phase) {
    f32 *rate = (f32 *)&chr->work[6];
    s32 anim;
    s32 airSaved;
    s32 air;
    f32 t;
    f32 lean;

    if (phase == 0) {
        anim = 0;
        airSaved = BtlAct_IsAirMotion(chr, 1);
        switch (BtlAct_GetCurrent(chr)) {
            case 0xCB:
            case 0xCC:
                anim = airSaved ? 0xAF : 0xAE;
                break;
        }
        BtlAnim_Play(chr, anim, 0.0f);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        *rate = 2.0f;
    }
    if (phase == 1) {
        air = BtlAct_IsAirMotion(chr, 0);
        BtlActB_UpdateMashRate(chr, rate, BtlAct_GetCurrent(chr) == 0xCB);
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, BtlAct_GetIdleFollowUp(chr));
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFlag(chr, 0x41);
        if (BtlAct_GetCurrent(chr) == 0xCC) {
            t = 1.0f - BtlAnim_GetProgress(chr);
            lean = t * -BTL_DEG(90.0f);
            if (air) {
                lean = t * BTL_DEG(45.0f);
            }
            BtlMove_SetLeanX(chr, lean);
        }
    }
    if (phase == 2) {
        if (BtlDecide_Main(chr, 0x10000000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlDecide_Main(chr, 0x800000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
    if (phase == 3) {
        chr->unkFF0 = 15;
    }
}

/* Action 0xCD: hits the ground: animations 0xDC then 0xDD at the mash rate, sliding on at 50 km/h. When 0xDD ends:
   in flight mode 0xE6 (free) or 0xD9; otherwise the down action 0xDB, or with a direction held by a free fighter a
   recovery (0x25 for up / down, 0xE2 left, 0xE3 right; an "air" set turns the heading round first). */
s32 BtlAct_GroundBounceHandler(BtlActBChr *chr, s32 phase) {
    f32 *rate = (f32 *)&chr->work[6];
    s32 next;
    s32 id;

    if (phase == 0) {
        if (BtlAct_IsAirMotion(chr, 1)) {
            chr->work[0] |= 1;
        }
        BtlAnim_Play(chr, 0xDC, 0.0f);
        *rate = 2.0f;
    }
    if (phase == 1) {
        BtlActB_UpdateMashRate(chr, rate, 1);
        switch (BtlAnim_GetId(chr)) {
            case 0xDC:
                BtlAnim_AdvanceThen(chr, 0xDD, 0.0f, 0);
                break;
            case 0xDD:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                break;
        }
        BtlMove_Step(chr, 6, 5, (chr->work[0] & 1) ? 2 : 4, BTL_KMH(50.0f), BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFlag(chr, 0x41);
        if (BtlChar_IsStage4Or27()) {
            if (BtlChar_TestFlag(chr, 0x17)) {
                BtlChar_SetHeldFlag(chr, 0xE);
            }
        }
    }
    if (phase == 2) {
        if (BtlDecide_Main(chr, 0x10000000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (chr->work[1]) {
            next = 0xDB;
            if (BtlChar_TestFlag(chr, 0xE)) {
                next = BtlChar_IsFree(chr) ? 0xE6 : 0xD9;
            } else if (BtlChar_IsFree(chr)) {
                if (!BtlChar_TestFlag(chr, 0x1E)) {
                    id = -1;
                    if (BtlInput_IsHeld(chr, 0x20)) {
                        id = 0x25;
                    }
                    if (BtlInput_IsHeld(chr, 0x10)) {
                        id = 0x25;
                    }
                    if (BtlInput_IsHeld(chr, 0x40)) {
                        id = 0xE2;
                    }
                    if (BtlInput_IsHeld(chr, 0x80)) {
                        id = 0xE3;
                    }
                    if (id >= 0) {
                        if (chr->work[0] & 1) {
                            BtlChar_GetPos(chr)->yaw = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw + 3.14159265f);
                        }
                        next = id;
                    }
                }
            }
            BtlAct_Request(chr, next);
        }
    }
}

/* Action 0xCE: knocked upward at up to 1200 km/h (70% plus 30% of chr + 0xFC8): animation 0xDA rising, 0xDB once
   falling, travelling at 1000 km/h. While falling: lands (under two frames from the ground, or heading into the floor)
   into 0xDC, or straight into an air recovery (0x25 / 0x27) with a direction held; meets a wall into 0xE7 / 0xE8 /
   0xDC; water (flag 0x11) gives the down action; flags 0x16 / 0x17 give 0xE6. From the ninth frame a free fighter can
   recover with input condition 0x2F (0xE6) or 0x30 (flight mode and 0x68). */
s32 BtlAct_LaunchedHandler(BtlActBChr *chr, s32 phase) {
    if (phase == 0) {
        if (BtlAct_IsAirMotion(chr, 1)) {
            chr->work[0] |= 1;
        }
        BtlAnim_Play(chr, 0xDA, 0.0f);
        BtlChar_GetPos(chr)->fallSpeed = (chr->unkFC8 * 0.3f + 0.7f) * -BTL_KMH(1200.0f);
        BtlChar_ClearFlag(chr, 0xE);
    }
    if (phase == 1) {
        BtlMove_Fall(chr);
        switch (BtlAnim_GetId(chr)) {
            case 0xDA:
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlChar_GetPos(chr)->fallSpeed > 0.0f) {
                    BtlAnim_Request(chr, 0xDB, 0.4f);
                }
                break;
            case 0xDB:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
        }
        BtlMove_Step(chr, 6, 5, (chr->work[0] & 1) ? 2 : 4, BTL_KMH(1000.0f), BTL_KMH(100.0f));
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFxBit(chr, 0x3B);
    }
    if (phase == 2) {
        if (BtlDecide_Main(chr, 0x10000000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlAnim_GetId(chr) == 0xDB) {
            if (BtlAct_GetFramesToGround(chr) < 2.0f || BtlMove_IsHeadingIntoFloor(chr, -6.2831853f)) {
                if (!BtlChar_IsStage4Or27() || BtlAct_GetGroundY(chr) < -10.0f) {
                    BtlAct_Request(chr, 0xDC);
                    if (BtlChar_IsFree(chr) && !BtlChar_TestFlag(chr, 0x1E) && BtlInput_IsHeld(chr, 0xF0)) {
                        if (!BtlParam_CanFly(chr)) {
                            BtlAct_Request(chr, 0x25);
                        } else {
                            BtlAct_Request(chr, 0x27);
                        }
                    }
                } else if (BtlChar_TestFlag(chr, 7)) {
                    BtlAct_Request(chr, 0xDC);
                }
            }
            if (BtlMove_IsHeadingIntoWall(chr, -6.2831853f)) {
                if (((BtlActBObj *)BtlChar_GetObj(chr))->unk1660->unk18060 & 0x40) {
                    if (BtlChar_IsFree(chr)) {
                        BtlAct_Request(chr, 0xE8);
                    } else {
                        BtlAct_Request(chr, 0xE7);
                    }
                } else if (chr->work[0] & 1) {
                    BtlAct_Request(chr, 0xDC);
                    if (BtlChar_IsFree(chr) && !BtlChar_TestFlag(chr, 0x1E) && BtlInput_IsHeld(chr, 0xF0)) {
                        if (!BtlParam_CanFly(chr)) {
                            BtlAct_Request(chr, 0x25);
                        } else {
                            BtlAct_Request(chr, 0x27);
                        }
                    }
                } else {
                    BtlAct_Request(chr, 0xE7);
                    if (BtlChar_IsFree(chr) && !BtlChar_TestFlag(chr, 0x1E) && BtlInput_IsHeld(chr, 0xF0)) {
                        BtlAct_Request(chr, 0x29);
                    }
                }
            }
            if (BtlChar_TestFlag(chr, 0x11)) {
                BtlAct_Request(chr, BtlAct_GetDownAction(chr));
            }
            if (BtlChar_TestFlag(chr, 0x16)) {
                if (BtlChar_IsFree(chr)) {
                    BtlAct_Request(chr, 0xE6);
                }
            }
            if (BtlChar_IsStage4Or27()) {
                if (BtlChar_TestFlag(chr, 0x17)) {
                    if (BtlChar_IsFree(chr)) {
                        BtlAct_Request(chr, 0xE6);
                    }
                }
            }
        }
        if (chr->actionFrame > 8) {
            if (BtlChar_IsFree(chr)) {
                if (BtlInput_TestAction(chr, 0x2F, 1)) {
                    BtlAct_Request(chr, 0xE6);
                }
                if (BtlInput_TestAction(chr, 0x30, 1)) {
                    BtlChar_SetHeldFlag(chr, 0xE);
                    BtlAct_Request(chr, 0x68);
                }
            }
        }
    }
}

/* Actions 0xCF..0xD1: thrown back: 0xCF animation 0xBB / 0xBC and 0xD0 animation 0xC5 at 500 km/h out of flight mode
   (0xD0 turns an "air" set round first), 0xD1 animation 0x17C / 0x17D in place. Above 10 units the vertical speed is
   braked, below it the fighter falls. Ends on flag 0x31, or past 70% when off the ground: 0xDB on the ground, else
   0xD9 (0xE6 on stages 4 / 27), or a recovery with up (0x26) / down (0x25, 0xE6) held. */
s32 BtlAct_ActionCFtoD1(BtlActBChr *chr, s32 phase) {
    s32 anim;
    s32 air;
    BtlActBPose *pos;
    f32 speed;
    s32 end;

    if (phase == 0) {
        anim = 0xC5;
        air = BtlAct_IsAirMotion(chr, 1);
        pos = BtlChar_GetPos(chr);
        switch (BtlAct_GetCurrent(chr)) {
            case 0xCF:
                anim = air ? 0xBC : 0xBB;
                BtlChar_ClearFlag(chr, 0xE);
                break;
            case 0xD0:
                if (air) {
                    pos->rot.y = BtlUtil_WrapAngle(pos->rot.y + 3.14159265f);
                    pos->yaw = BtlUtil_WrapAngle(pos->yaw + 3.14159265f);
                    pos->pitch = -pos->pitch;
                }
                BtlChar_ClearFlag(chr, 0xE);
                break;
            case 0xD1:
                anim = air ? 0x17D : 0x17C;
                break;
        }
        BtlAnim_Play(chr, anim, 0.0f);
    }
    if (phase == 1) {
        speed = 0.0f;
        BtlAnim_Advance(chr, 0);
        switch (BtlAct_GetCurrent(chr)) {
            case 0xCF:
                speed = BTL_KMH(500.0f);
                break;
            case 0xD0:
                speed = BTL_KMH(500.0f);
                break;
            case 0xD1:
                speed = 0.0f;
                break;
        }
        BtlMove_Step(chr, 6, 6, BtlAct_IsAirMotion(chr, 0) ? 3 : 5, speed, BTL_KMH(100.0f));
        if (BtlAct_GetHeight(chr) > 10.0f) {
            BtlMove_BrakeVertical(chr);
        } else {
            BtlMove_Fall(chr);
        }
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFxBit(chr, 0x3B);
    }
    if (phase == 2) {
        if (BtlChar_IsFree(chr)) {
            if (!BtlChar_TestFlag(chr, 0x1E)) {
                BtlColl_AddActionBit(chr, 0x25);
                BtlColl_AddActionBit(chr, 0x26);
            }
            if (BtlDecide_Main(chr, 0x10000000)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            }
        }
        end = BtlChar_TestFlag(chr, 0x31) != 0;
        if (BtlAnim_GetProgress(chr) > 0.7f) {
            if (!BtlChar_TestFlag(chr, 0xF)) {
                end = 1;
            }
        }
        if (end) {
            if (BtlChar_TestFlag(chr, 0xF)) {
                BtlAct_Request(chr, 0xDB);
            } else {
                BtlAct_Request(chr, 0xD9);
                if (BtlChar_IsStage4Or27()) {
                    if (BtlChar_IsFree(chr)) {
                        BtlAct_Request(chr, 0xE6);
                    }
                }
            }
            if (BtlChar_IsFree(chr)) {
                if (!BtlChar_TestFlag(chr, 0x1E)) {
                    if (BtlInput_IsHeld(chr, 0x10)) {
                        BtlChar_SetHeldFlag(chr, 0xE);
                        BtlAct_Request(chr, 0x26);
                    } else if (BtlInput_IsHeld(chr, 0x20)) {
                        if (BtlChar_TestFlag(chr, 0xF)) {
                            BtlAct_Request(chr, 0x25);
                        } else {
                            BtlAct_Request(chr, 0xE6);
                        }
                    }
                }
            }
        }
    }
}

/* Action 0xD2: animation 0xD9 in place, then the down action; off the ground past 40% it falls (0xD9) instead. */
s32 BtlAct_ActionD2(BtlActBChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xD9, 0.0f);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlChar_ClearFlag(chr, 0xE);
            BtlAct_Request(chr, BtlAct_GetDownAction(chr));
        }
        if (!BtlChar_TestFlag(chr, 0xF)) {
            if (BtlAnim_GetProgress(chr) > 0.4f) {
                BtlAct_Request(chr, 0xD9);
            }
        }
        BtlMove_Step(chr, 6, 6, 6, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
    }
}

/* Action 0xD4: knocked back at 700 km/h (on stages 4 / 27 scaled by 0.5 + 0.5 * health lost): animations 0xBD start,
   0xBE loop, 0xBF end (0xC0..0xC2 for an "air" set). The loop lasts chr + 0xFC8 * 15 frames (times the health lost on
   stages 4 / 27) and deflects at the stage limit; when it ends a free fighter holding a direction recovers (0xE6).
   The end animation gives the down action, or 0xD9 when higher than the body size past 60%. Work bit 1 (previous
   action 0xB8 / 0xBA, a throw) raises flag 0x138 every frame. */
s32 BtlAct_KnockBackHandler(BtlActBChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    f32 speed;
    f32 height;

    if (phase == 0) {
        BtlAnim_Play(chr, BtlAct_IsAirMotion(chr, 1) ? 0xC0 : 0xBD, 0.0f);
        *timer = chr->unkFC8 * 15.0f;
        if (BtlChar_IsStage4Or27()) {
            *timer = *timer * (1.0f - BtlMember_GetHealthRatio(chr));
        }
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        BtlChar_ClearFlag(chr, 0xE);
        switch (BtlAct_GetPrev(chr)) {
            case 0xB8:
            case 0xBA:
                chr->work[0] |= 1;
                break;
        }
    }
    if (phase == 1) {
        speed = BTL_KMH(700.0f);
        if (BtlChar_IsStage4Or27()) {
            speed = ((1.0f - BtlMember_GetHealthRatio(chr)) * 0.5f + 0.5f) * speed;
        }
        switch (BtlAnim_GetId(chr)) {
            case 0xBD:
            case 0xC0:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                break;
            case 0xBE:
            case 0xC1:
                if (*timer > 0) {
                    BtlAnim_AdvanceLoop(chr, 0);
                    (*timer)--;
                } else if (BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0)) {
                    if (BtlChar_IsFree(chr)) {
                        if (!BtlChar_TestFlag(chr, 0x1D)) {
                            if (BtlInput_IsHeld(chr, 0xF0) || BtlChar_IsStage4Or27()) {
                                BtlAct_Request(chr, 0xE6);
                            }
                        }
                    }
                }
                if (chr->actionFrame > 0) {
                    BtlMove_DeflectAtStageLimit(chr, BtlAct_IsAirMotion(chr, 0));
                }
                break;
            case 0xBF:
            case 0xC2:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, BtlAct_GetDownAction(chr));
                    BtlCharSnd_PlayBank8(chr, 2);
                }
                height = BtlAct_GetHeight(chr);
                if (BtlCharApi_GetHeight(chr->objId) < height) {
                    if (BtlAnim_GetProgress(chr) > 0.6f) {
                        BtlAct_Request(chr, 0xD9);
                    }
                }
                break;
        }
        BtlMove_Step(chr, 6, 6, BtlAct_IsAirMotion(chr, 0) ? 3 : 5, speed, 10000.0f);
        BtlMove_BrakeVertical(chr);
        BtlChar_SetFlag(chr, 0x1B);
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFxBit(chr, 0x3B);
        if (chr->work[0] & 1) {
            BtlChar_SetFlag(chr, 0x138);
        }
    }
    if (phase == 2) {
        if (BtlDecide_Main(chr, 0x10000000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/* Actions 0xD5, 0xD6, 0xDF: blown away in flight mode for 50 / 15 / 32 frames (times the health lost on stages
   4 / 27) at 1000 / 2000 / 2000 km/h (1000 on stages 4 / 27), looping animation 0xC3 (0xD5, 0xD6) or 0xC6 (0xDF), the
   next one for an "air" set. 0xD5 plays common sound 0x48 and follows the heading pitch, 0xD6 spins about z by 48
   degrees a frame. Decide, for a free fighter: main input 0x10000000; from the ninth frame, with the ki for it, the air
   recoveries (flags 0x21 / 0x22 from input conditions 0x28 / 0x29); with skill bit 0x40, mashing 4 + 12 * health lost
   times gives 0xD7. Then what it runs into: the floor (within 70 degrees) an air recovery or 500 damage, camera shake,
   rumble and 0xDD; a wall the same with 0xE7, or 0xE8 / 0xE7 when the object's word (+0x1660)->+0x18060 has bit 0x40; flag 0x14 with the movement pointing
   away from the centre 0xE8 / 0xE7; the opponent 0xE9 / 0xE7. When the time runs out: 0xD9 (0xE6 on stages 4 / 27).
   `sel` is one register in the original for the animation, the direction mode and the phase 2 air test. */
s32 BtlAct_BlowAwayHandler(BtlActBChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 *mash = &chr->work[3];
    f32 *spin = (f32 *)&chr->work[6];
    s32 sel;
    s32 air;
    s32 hit;
    f32 blend;
    f32 speed;
    Vec4 a;
    Vec4 b;

    if (phase == 0) {
        sel = 0;
        air = BtlAct_IsAirMotion(chr, 1);
        blend = 0.0f;
        if (BtlAct_GetPrev(chr) == 0xB8) {
            blend = 0.15f;
        }
        switch (BtlAct_GetCurrent(chr)) {
            case 0xD5:
                *timer = 50;
                sel = air ? 0xC4 : 0xC3;
                BtlCharSnd_PlayCommon(chr, 0x48);
                chr->work[0] |= 3;
                break;
            case 0xD6:
                *timer = 15;
                sel = air ? 0xC4 : 0xC3;
                chr->work[0] |= 6;
                break;
            case 0xDF:
                *timer = 32;
                sel = air ? 0xC7 : 0xC6;
                chr->work[0] |= 8;
                break;
        }
        if (BtlChar_IsStage4Or27()) {
            *timer = *timer * (1.0f - BtlMember_GetHealthRatio(chr));
        }
        BtlAnim_Play(chr, sel, blend);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        switch (BtlAct_GetPrev(chr)) {
            case 0xB8:
            case 0xBA:
                chr->work[0] |= 1;
                break;
        }
    }
    if (phase == 1) {
        air = BtlAct_IsAirMotion(chr, 0);
        BtlAnim_AdvanceLoop(chr, 0);
        if (--*timer < 0) {
            if (BtlChar_IsStage4Or27() && BtlChar_IsFree(chr)) {
                BtlAct_Request(chr, 0xE6);
            } else {
                BtlAct_Request(chr, 0xD9);
            }
        }
        if (chr->actionFrame > 0) {
            BtlMove_DeflectAtStageLimit(chr, BtlAct_IsAirMotion(chr, 0));
        }
        sel = air ? 3 : 5;
        speed = 0.0f;
        if (BtlChar_IsStage4Or27()) {
            switch (BtlAct_GetCurrent(chr)) {
                case 0xD5:
                    speed = BTL_KMH(1000.0f);
                    break;
                case 0xD6:
                    speed = BTL_KMH(1000.0f);
                    break;
                case 0xDF:
                    speed = BTL_KMH(1000.0f);
                    break;
            }
        } else {
            switch (BtlAct_GetCurrent(chr)) {
                case 0xD5:
                    speed = BTL_KMH(1000.0f);
                    break;
                case 0xD6:
                    speed = BTL_KMH(2000.0f);
                    break;
                case 0xDF:
                    speed = BTL_KMH(2000.0f);
                    break;
            }
        }
        BtlMove_Step(chr, 6, 6, sel, speed, 10000.0f);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1B);
        BtlChar_SetFlag(chr, 0xA);
        BtlChar_SetFlag(chr, 0x2A);
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFxBit(chr, 0x3B);
        if (chr->work[0] & 2) {
            BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch);
        }
        if (chr->work[0] & 4) {
            *spin = BtlUtil_WrapAngle(*spin + BTL_DEG(48.0f));
            BtlMove_SetLeanZ(chr, *spin);
        }
        if (chr->work[0] & 1) {
            BtlChar_SetFlag(chr, 0x138);
        }
    }
    if (phase == 2) {
        if (BtlChar_IsFree(chr)) {
            if (BtlDecide_Main(chr, 0x10000000)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            }
            if (!BtlChar_TestFlag(chr, 0x1E) && chr->actionFrame > 8) {
                if (BtlMember_HasKi(chr, BtlParam_GetRecoverKiCost(chr))) {
                    BtlColl_AddActionBit(chr, 0x25);
                    BtlColl_AddActionBit(chr, 0x2A);
                    if (BtlInput_TestAction(chr, 0x28, 1)) {
                        BtlChar_SetFlag(chr, 0x21);
                    }
                    if (BtlInput_TestAction(chr, 0x29, 1)) {
                        BtlChar_SetFlag(chr, 0x22);
                    }
                }
            }
            if (!BtlChar_TestFlag(chr, 0x1D)) {
                if (BtlParam_GetFlags2(chr) & 0x40) {
                    BtlColl_AddActionBit(chr, 0xD7);
                    BtlChar_SetFlag(chr, 0xE7);
                    if (BtlInput_IsPressed(chr, 0x100000)) {
                        (*mash)++;
                        if (*mash >= (s32)((1.0f - BtlMember_GetHealthRatio(chr)) * 12.0f) + 4) {
                            BtlAct_Request(chr, 0xD7);
                        }
                    }
                }
            }
        }
        if (BtlMove_IsHeadingIntoFloor(chr, 3.14159265f * 70.0f / 180.0f)) {
            if (BtlAct_QueueFlag21Action(chr)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            } else {
                BtlMember_Damage(chr, 500, 0);
                BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 3.0f, 0.4f);
                BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 0.8f, 0.2f);
                if (chr->work[0] & 8) {
                    BtlChar_GetPos(chr)->speed = 0.0f;
                }
                BtlAct_Request(chr, 0xDD);
            }
        } else if (BtlMove_IsHeadingIntoWall(chr, 3.14159265f * 70.0f / 180.0f)) {
            if (((BtlActBObj *)BtlChar_GetObj(chr))->unk1660->unk18060 & 0x40) {
                if (BtlChar_IsFree(chr)) {
                    BtlAct_Request(chr, 0xE8);
                } else {
                    BtlAct_Request(chr, 0xE7);
                }
            } else if (BtlAct_QueueFlag21ActionB(chr)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            } else {
                BtlMember_Damage(chr, 500, 0);
                BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 3.0f, 0.4f);
                BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 0.8f, 0.2f);
                BtlAct_Request(chr, 0xE7);
            }
        } else if (BtlChar_TestFlag(chr, 0x14)) {
            Vec3_Normalize(&a, &BtlChar_GetPos(chr)->pos);
            Vec3_Normalize(&b, &BtlChar_GetPos(chr)->unk30);
            if (Vec3_Dot(&a, &b) > 0.7f) {
                if (BtlChar_IsFree(chr)) {
                    BtlAct_Request(chr, 0xE8);
                } else {
                    BtlAct_Request(chr, 0xE7);
                }
            }
        } else if (BtlMove_IsBlockedByOpponent(chr)) {
            sel = BtlAct_IsAirMotion(chr, 0);
            if (__builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) < BTL_DEG(90.0f)) {
                hit = sel;
            } else {
                hit = !sel;
            }
            if (hit) {
                if (BtlChar_IsFree(chr)) {
                    BtlAct_Request(chr, 0xE9);
                } else {
                    BtlAct_Request(chr, 0xE7);
                }
            }
        }
    }
    if (phase == 3) {
        if (chr->work[0] & 1) {
            BtlCharSnd_StopCommon(chr, 0x48);
        }
    }
}

/* Action 0xD7: stops being blown away: animation 0x18D / 0x18E, speed halved, common sound 0x1F, flight mode, braking
   at 50 km/h per frame while the lean follows the heading pitch; then 0x19 with up held, else 0xB. */
s32 BtlAct_BlowRecoverHandler(BtlActBChr *chr, s32 phase) {
    BtlActBPose *pos;
    s32 air;
    f32 t;

    if (phase == 0) {
        BtlAnim_Play(chr, BtlAct_IsAirMotion(chr, 0) ? 0x18E : 0x18D, 0.15f);
        pos = BtlChar_GetPos(chr);
        pos->speed *= 0.5f;
        BtlCharSnd_PlayCommon(chr, 0x1F);
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if (phase == 1) {
        air = BtlAct_IsAirMotion(chr, 0);
        if (BtlAnim_Advance(chr, 0)) {
            if (BtlInput_IsHeld(chr, 0x10)) {
                BtlAct_Request(chr, 0x19);
            } else {
                BtlAct_Request(chr, 0xB);
            }
        }
        BtlMove_Step(chr, 6, 6, air ? 3 : 5, 0.0f, BTL_KMH(50.0f));
        BtlMove_ApplyGravity(chr);
        t = 1.0f - BtlAnim_GetProgress(chr);
        BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch * t);
    }
}

/* Action 0xD8: lying down (raises battle event 0x22 for the fighter's player). The animation follows
   BtlActB_PickDownAnim every frame unless flag 0x34. With the gauge word +0x28 set the fighter has to mash (chr + 0x1000
   counts the presses down) before it may get up; a dead fighter raises flag 0x137, and once at rest the member change
   0xF6 after 31 frames. Decide: off the ground 0xD9; after 90 frames 0xE6 / 0xE1; the get-up inputs
   (BtlAct_CheckRecoveryInput); 0xE0 while chr + 0xFE0 counts. */
s32 BtlAct_LieDownHandler(BtlActBChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 cur;
    s32 next;
    s32 down;
    f32 accel;
    f32 height;

    if (phase == 0) {
        BtlAnim_Play(chr, BtlActB_PickDownAnim(chr, BtlAnim_GetId(chr)), 0.15f);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        } else {
            BtlChar_ClearFlag(chr, 0xE);
        }
        BtlEvent_Raise(chr->player, 0x22);
    }
    if (phase == 1) {
        accel = BTL_KMH(100.0f);
        down = 0;
        BtlAnim_AdvanceLoop(chr, 0);
        if (!BtlChar_TestFlag(chr, 0x34)) {
            cur = BtlAnim_GetId(chr);
            next = BtlActB_PickDownAnim(chr, cur);
            if (cur != next) {
                BtlAnim_Request(chr, next, 0.15f);
            }
        }
        switch (BtlAnim_GetId(chr)) {
            case 0xD5:
            case 0xD6:
                BtlMove_Fall(chr);
                down = 1;
                break;
            case 0xD2:
            case 0xD3:
                accel = BTL_KMH(500.0f) * (1.0f / 30.0f);
                BtlMove_Fall(chr);
                if (BtlChar_TestFlag(chr, 0xF)) {
                    BtlChar_SetFxBit(chr, 0x33);
                }
                break;
            case 0xD7:
            case 0xD8:
                BtlMove_MoveVertical(chr, BTL_KMH(30.0f), BTL_KMH(100.0f));
                down = 1;
                break;
        }
        BtlMove_Step(chr, 6, 5, 6, 0.0f, accel);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlMember_GetActiveGauge(chr)->unk28) {
            BtlChar_SetFlag(chr, 0x96);
            if (BtlChar_IsFree(chr)) {
                BtlChar_SetFlag(chr, 0xE7);
                if (BtlInput_IsPressed(chr, 0x100000)) {
                    chr->unk1000--;
                }
            }
        }
        if (BtlChar_IsDead(chr)) {
            BtlChar_SetFlag(chr, 0x137);
        }
        if (down) {
            if (BtlActB_TickMemberChange(chr, timer)) {
                BtlAct_Request(chr, 0xF6);
            }
        }
    }
    if (phase == 2) {
        if (!BtlChar_TestFlag(chr, 0xF) && !BtlChar_TestPrevFlag(chr, 0xF) && !BtlChar_TestFlag(chr, 0x1F) &&
            !BtlChar_TestFlag(chr, 0x11)) {
            height = BtlAct_GetHeight(chr);
            if (BtlCharApi_GetHeight(chr->objId) * 0.3f < height) {
                BtlAct_Request(chr, 0xD9);
            }
        }
        if (BtlChar_IsFree(chr)) {
            if (chr->actionFrame > 90) {
                if (BtlChar_TestFlag(chr, 0x11)) {
                    BtlAct_Request(chr, 0xE6);
                } else {
                    BtlAct_Request(chr, 0xE1);
                }
            }
            if (!BtlMember_GetActiveGauge(chr)->unk28 || chr->unk1000 <= 0) {
                if (BtlAct_CheckRecoveryInput(chr, 0)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
            }
            if (BtlChar_IsStage4Or27()) {
                if (BtlChar_TestFlag(chr, 0x17)) {
                    BtlAct_Request(chr, 0xE6);
                }
            }
        }
        if (chr->unkFE0 > 0) {
            BtlAct_Request(chr, 0xE0);
        }
    }
}

/* Action 0xD9: falling helplessly (animation 0xC6 / 0xC7). Lands (two frames from the ground, on it, or flag 0x1F;
   on stages 4 / 27 only on the ground) into 0xDC or an air recovery with a direction held; landing in a pit below
   the stage height gives the member change 0xF6 at once. Water gives the down action, flag 0x16 gives 0xE6, and a
   press of an action button gives 0xE6 when no mashing is owed. */
s32 BtlAct_FallHandler(BtlActBChr *chr, s32 phase) {
    BtlActBPose *pos;
    s32 air;
    s32 land;
    f32 blend;

    if (phase == 0) {
        blend = 0.15f;
        air = BtlAct_IsAirMotion(chr, 1);
        if (BtlAct_GetPrev(chr) == 0xD5) {
            blend = 0.3f;
        }
        BtlAnim_Play(chr, air ? 0xC7 : 0xC6, blend);
        BtlChar_ClearFlag(chr, 0xE);
    }
    if (phase == 1) {
        BtlAnim_AdvanceLoop(chr, 0);
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(500.0f) * (1.0f / 30.0f));
        BtlMove_Fall(chr);
        BtlActB_SetReactionFlags(chr);
        if (BtlMember_GetActiveGauge(chr)->unk28) {
            BtlChar_SetFlag(chr, 0x96);
            if (BtlChar_IsFree(chr)) {
                if (!BtlChar_TestFlag(chr, 0x1D)) {
                    BtlChar_SetFlag(chr, 0xE7);
                    if (BtlInput_IsPressed(chr, 0x100000)) {
                        chr->unk1000--;
                    }
                }
            }
        }
    }
    if (phase == 2) {
        land = 0;
        if (BtlChar_IsStage4Or27()) {
            if (BtlChar_TestFlag(chr, 0xF)) {
                land = 1;
            } else if (BtlChar_TestFlag(chr, 0x17)) {
                if (BtlChar_IsFree(chr)) {
                    BtlAct_Request(chr, 0xE6);
                }
            }
        } else {
            if (BtlAct_GetFramesToGround(chr) < 2.0f || BtlChar_TestFlag(chr, 0xF) || BtlChar_TestFlag(chr, 0x1F)) {
                land = 1;
            }
        }
        if (land) {
            BtlAct_Request(chr, 0xDC);
            if (BtlChar_IsFree(chr) && !BtlChar_TestFlag(chr, 0x1E)) {
                BtlColl_AddActionBit(chr, 0x25);
                if (BtlInput_IsHeld(chr, 0xF0)) {
                    if (!BtlParam_CanFly(chr)) {
                        BtlAct_Request(chr, 0x25);
                    } else {
                        BtlAct_Request(chr, 0x27);
                    }
                }
            }
            if (BtlChar_GetPos(chr)->groundFlags & 0x40) {
                pos = BtlChar_GetPos(chr);
                if (BtlStage_GetBottom() < pos->pos.y) {
                    if (BtlActB_TickMemberChange(chr, NULL)) {
                        BtlAct_Request(chr, 0xF6);
                    }
                }
            }
        }
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlAct_Request(chr, BtlAct_GetDownAction(chr));
        }
        if (BtlChar_TestFlag(chr, 0x16)) {
            if (BtlChar_IsFree(chr)) {
                BtlAct_Request(chr, 0xE6);
            }
        }
        if (BtlInput_IsPressed(chr, 0x100000)) {
            if (BtlChar_IsFree(chr)) {
                if (!BtlChar_TestFlag(chr, 0x1D)) {
                    if (!BtlMember_GetActiveGauge(chr)->unk28 || chr->unk1000 <= 0) {
                        BtlAct_Request(chr, 0xE6);
                    }
                }
            }
        }
    }
}
