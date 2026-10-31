#include "battle/tag_clash.h"
#include "common.h"
#include "battle/btl_act_e.h"

/*
 * Fighter action handlers, 0x1F1930..0x1F5460: actions 0x1A..0x35 and 0xFA..0xFC.
 * Handler protocol (phase 0 enter, 1 run, 2 decide, 3 leave): include/battle/btl_char_action.h.
 *
 * Return types: most handlers are int functions without a return statement, as elsewhere. Those whose
 * leave phase ends in a tail call in the original (0x25..0x2E, 0x31, 0x32, 0x34, 0xFA) only match as void.
 *
 * "Speed kind n" below is BtlMoveParam_GetSpeed(chr, n), "ki drain kind n" is BtlMoveParam_GetKiCost(chr, n): per-character
 * parameters owned by a module that is not decompiled yet.
 */

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)
/* A second degrees macro: the two angles of BtlAct_SlideToAttackHandler only come out right in this order. */
#define BTL_DEG2RAD(x) ((x) * 3.14159265f / 180.0f)

extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
extern f32 Vec3_Length(Vec4 *v);
extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 sqrtf(f32 x);
extern void Vec3_Normalize(Vec4 *out, Vec4 *in);
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_SetZero(Vec4 *v); /* zeroes a vector */
extern void BtlEvent_Raise(s32 side, s32 event);
extern void BtlChar_AddStageTimer(f32 seconds);
extern void BtlChar_SetSmallVibration(BtlActEChr *chr, f32 seconds);
extern void BtlChar_SetFrameBits(BtlActEChr *chr, u64 bits);
extern void BtlOpp_GetTargetRot(BtlActEChr *chr, Vec4 *out);
extern void BtlOpp_GetDelta(BtlActEChr *chr, Vec4 *out);
extern void BtlOpp_GetVelocity(BtlActEChr *chr, Vec4 *out);
extern void BtlMove_WarpBehindOpponent(BtlActEChr *chr, f32 gap);
extern void BtlMove_SnapToOpponent(BtlActEChr *chr);
extern void BtlMove_WarpAheadOfOpponent(BtlActEChr *chr, f32 lead);
extern void BtlMove_CalcApproachPoint(BtlActEChr *chr, Vec4 *out, Vec4 *outTarget, f32 dist, f32 angle, f32 distScale, f32 lead);
extern void BtlMove_TurnToPoint(BtlActEChr *chr, Vec4 *target, f32 yawStep, f32 pitchStep);
extern void Vec4_Sub(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *in, f32 scale);
extern void Vec4_Copy(Vec4 *out, Vec4 *in);
extern f32 floorf(f32 x);
extern void BtlAnim_PlaySub(BtlActEChr *chr, s32 anim);
extern s32 BtlAct_IsAirMotion(BtlActEChr *chr, s32 useSaved);
extern s32 BtlParam_GetRecoverKiCost(BtlActEChr *chr);               /* character parameter: a ki cost */
extern s32 BtlObjAnim_QueryEvent(BtlActEObj *obj, u64 a, s32 b, s32 c);

extern BtlActEPose *BtlChar_GetPos(BtlActEChr *chr);
extern BtlActEObj *BtlChar_GetObj(BtlActEChr *chr);
extern s32 BtlChar_TestFlag(BtlActEChr *chr, s32 n);
extern void BtlChar_SetFlag(BtlActEChr *chr, s32 n);
extern void BtlChar_SetHeldFlag(BtlActEChr *chr, s32 n);
extern void BtlChar_ClearFlag(BtlActEChr *chr, s32 n);
extern void BtlChar_PlayVoice(BtlActEChr *chr, s32 kind);
extern void BtlChar_Vibrate(BtlActEChr *chr, f32 power, f32 seconds);
extern void BtlChar_SetVibration(BtlActEChr *chr, f32 power, f32 seconds);
extern void BtlChar_SetFxBit(BtlActEChr *chr, s32 n);
extern void BtlCharSnd_PlayCommon(BtlActEChr *chr, s32 id);
extern void BtlCharSnd_PlayBank8(BtlActEChr *chr, s32 id);
extern s32 BtlInput_IsHeld(BtlActEChr *chr, u32 mask);
extern s32 BtlInput_TestAction(BtlActEChr *chr, s32 id, s32 want);
extern BtlActEGauge *BtlMember_GetActiveGauge(BtlActEChr *chr);
extern s32 BtlMember_SpendKi(BtlActEChr *chr, s32 amount, s32 force);
extern f32 BtlOpp_GetYaw(BtlActEChr *chr);
extern f32 BtlOpp_GetClosingTime(BtlActEChr *chr);
extern s32 BtlInput_IsPressed(BtlActEChr *chr, u32 mask);
extern void BtlCharSnd_PlayCommonFar(BtlActEChr *chr, s32 id);
extern void BtlChar_RequestPlaceOnPath(BtlActEChr *chr);
extern void BtlAnim_EnableHandle(BtlActEChr *chr);
extern s32 BtlAct_GetRequested(BtlActEChr *chr);

extern BtlActERoster *gBtlChars;
extern s32 BtlOpp_GetClashCountB(BtlActEChr *chr);
extern void BtlChar_ClearFlagRange(BtlActEChr *chr, u32 a, u32 b);
extern s32 BtlMember_HasAbility(BtlActEChr *chr, s32 n);
extern s32 BtlMember_AddKi(BtlActEChr *chr, s32 amount);
extern void BtlMember_AddMaxPower(BtlActEChr *chr, s32 amount);
extern s32 BtlMember_Damage(BtlActEChr *chr, s32 amount, s32 flags);
extern s32 BtlAnim_PassedRatio(BtlActEChr *chr, f32 ratio);
extern void BtlMove_SteerAtOpponent(BtlActEChr *chr, f32 closeSpeed, f32 yawAccel, f32 pitchAccel, f32 yawMax, f32 pitchMax, f32 maxStep);
extern s32 BtlAct_IsTechniqueId(s32 id);
extern s32 BtlAct_GetPrevClass(BtlActEChr *chr);
extern void BtlAct_CountAndMarkOpponent(BtlActEChr *chr);
extern s32 BtlSuper_GetKiCost(BtlActEChr *chr, s32 cls);
extern f32 BtlOpp_GetYawFromFacing(BtlActEChr *chr);
extern s32 BtlParam_GetFlags(BtlActEChr *chr);               /* character parameter flags */
extern f32 BtlOpp_GetYawFromItsFacing(BtlActEChr *chr);
extern s32 BtlOpp_GetSeenAction(BtlActEChr *chr);
extern void BtlAnim_SetDuration(BtlActEChr *chr, f32 seconds);
extern s32 BtlMove_CircleOpponent(BtlActEChr *chr, s32 side, s32 lead, f32 baseYaw, f32 speed);
extern void BtlMove_SetLeanX(BtlActEChr *chr, f32 v);
extern void ChrCam_RequestCut(BtlActEChr *chr, s32 arg1, s32 arg2);

extern void BtlAnim_Play(BtlActEChr *chr, s32 anim, f32 blend);
extern void BtlAnim_Request(BtlActEChr *chr, s32 anim, f32 blend);
extern s32 BtlAnim_GetId(BtlActEChr *chr);
extern f32 BtlAnim_GetProgress(BtlActEChr *chr);
extern f32 BtlAnim_GetLength(BtlActEChr *chr);
extern f32 BtlAnim_GetStep(BtlActEChr *chr);
extern s32 BtlAnim_Advance(BtlActEChr *chr, s32 flags);
extern s32 BtlAnim_AdvanceThen(BtlActEChr *chr, s32 next, f32 blend, s32 flags);
extern void BtlAnim_AdvanceLoop(BtlActEChr *chr, s32 flags);
extern s32 BtlAnim_IsNew(BtlActEChr *chr);

extern void BtlMove_TurnYaw(BtlActEChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_TurnPitch(BtlActEChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_TurnModelYaw(BtlActEChr *chr, f32 maxStep, f32 rate);
extern void BtlMove_SetDirection(BtlActEChr *chr, s32 mode);
extern void BtlMove_Advance(BtlActEChr *chr, f32 speed, f32 accel);
extern void BtlMove_Step(BtlActEChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel);
extern void BtlMove_MoveVertical(BtlActEChr *chr, f32 speed, f32 accel);
extern void BtlMove_ApplyGravity(BtlActEChr *chr);
extern s32 BtlMove_IsBlockedByOpponent(BtlActEChr *chr);
extern void BtlMove_RequestOrbit(BtlActEChr *chr, f32 near, f32 far);

extern void BtlAct_Request(BtlActEChr *chr, s32 id);
extern s32 BtlAct_HasQueued(BtlActEChr *chr);
extern s32 BtlAct_GetCurrent(BtlActEChr *chr);
extern s32 BtlAct_GetPrev(BtlActEChr *chr);
extern s32 BtlAct_GetQueued(BtlActEChr *chr);
extern f32 BtlAct_GetHeight(BtlActEChr *chr);
extern f32 BtlAct_ScaleSpeedByDist(BtlActEChr *chr, f32 speed, f32 range, f32 frames);

/* Not named yet (other modules). Purposes read from how they are used here. */
extern void BtlDecide_Main(BtlActEChr *chr, s32 mask);    /* fill the follow-up queue from the input, by class mask */
extern s32 BtlDecide_Attack(BtlActEChr *chr, s32 mask);     /* the same for attack inputs; 1 = queued */
extern s32 BtlDecide_QueueAttack(BtlActEChr *chr, s32 attack);   /* queue an attack; 1 = queued */
extern s32 BtlParam_GetAmountA(BtlActEChr *chr);               /* character parameter: a ki cost */
extern s32 BtlParam_GetAmountB(BtlActEChr *chr);               /* character parameter: a ki cost */
extern f32 BtlParam_GetUnk68(BtlActEChr *chr);               /* character parameter: progress ratio */
extern f32 BtlParam_GetUnk6C(BtlActEChr *chr);
extern s32 BtlParam_GetDashSound(BtlActEChr *chr);               /* character parameter: a common sound id */
extern f32 BtlMoveParam_GetSpeed(BtlActEChr *chr, s32 n);        /* character speed parameter n */
extern s32 BtlMoveParam_GetKiCost(BtlActEChr *chr, s32 n);        /* character ki cost n */
extern f32 BtlMoveParam_GetTurnRate(BtlActEChr *chr, s32 n);        /* character turn-rate parameter n */

/*
 * Action 0x1A: dash in the direction held. Animations 0x18 / 0x2D (start) -> 0x19 (loop) -> 0x1A or 0x15 (stop).
 * Speed kind 7, ASCEND / DESCEND (0x800 / 0x1000) move up / down at speed kind 8, ki drain kind 1 per frame. Stops when
 * blocked by the opponent, slower than 100 km/h for 16 frames, or out of ki. Inputs 0x14 / 0x15 (through
 * animation 0x15) chain into action 0x19 (with flag 5) or 0x1A again.
 */
s32 BtlAct_DragonDashHandler(BtlActEChr *chr, s32 phase) {
    s32 *counter = &chr->work[2];
    s32 flag;
    f32 speed;
    f32 turn;
    f32 vspeed;

    if (phase == 0) {
        s32 anim;

        switch (BtlAct_GetPrev(chr)) {
            case 0x19:
            case 0x1A:
                anim = 0x19;
                break;
            default:
                anim = 0x2D;
                break;
        }
        if (chr->unkFF0 > 0) {
            anim = 0x18;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlMove_TurnYaw(chr, 0, 3.14159265f);
    }
    if (phase == 1) {
        flag = 1;
        switch (BtlAnim_GetId(chr)) {
            case 0x18:
            case 0x2D:
                BtlAnim_AdvanceThen(chr, 0x19, 0.2f, 0);
                flag = 0;
                break;
            case 0x19:
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlAnim_IsNew(chr)) {
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                    BtlChar_Vibrate(chr, 0.8f, 0.3f);
                } else {
                    BtlChar_SetVibration(chr, 0.7f, 0.1f);
                }
                flag = 1;
                break;
            case 0x15:
            case 0x1A:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                    BtlChar_GetPos(chr)->unk98 = 0.0f;
                }
                flag = 0;
                break;
        }
        if (flag) {
            speed = BtlMoveParam_GetSpeed(chr, 7);
            turn = BtlMoveParam_GetTurnRate(chr, 2);
            vspeed = BtlMoveParam_GetSpeed(chr, 8);
            BtlMove_TurnYaw(chr, 0, turn);
            BtlMove_TurnPitch(chr, 5, 3.14159265f);
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, 10000.0f);
            if (BtlInput_IsHeld(chr, 0x800)) {
                BtlMove_MoveVertical(chr, -vspeed, BTL_KMH(100.0f));
            } else if (BtlInput_IsHeld(chr, 0x1000)) {
                BtlMove_MoveVertical(chr, vspeed, BTL_KMH(100.0f));
            } else {
                BtlMove_ApplyGravity(chr);
            }
            BtlChar_SetFlag(chr, 9);
            BtlChar_SetFlag(chr, 0x50);
            BtlChar_SetFlag(chr, 0x51);
            BtlChar_SetFxBit(chr, 4);
        } else {
            BtlMove_Step(chr, 6, 5, 3, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlMove_RequestOrbit(chr, 50.0f, 100.0f);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x18:
            case 0x2D:
                break;
            case 0x19:
                flag = BtlMove_IsBlockedByOpponent(chr) != 0;
                if (Vec3_Length(&BtlChar_GetPos(chr)->unk40) < BTL_KMH(100.0f)) {
                    if (++*counter >= 0x10) {
                        flag = 1;
                    }
                } else {
                    *counter = 0;
                }
                if (BtlMember_SpendKi(chr, BtlMoveParam_GetKiCost(chr, 1), 0)) {
                    flag = 1;
                }
                if (BtlInput_TestAction(chr, 0x16, 1)) {
                    BtlAnim_Request(chr, 0x15, 0.0f);
                }
                if (BtlInput_TestAction(chr, 0x14, 1)) {
                    BtlAnim_Request(chr, 0x15, 0.0f);
                    chr->work[0] |= 1;
                }
                if (BtlInput_TestAction(chr, 0x15, 1)) {
                    BtlAnim_Request(chr, 0x15, 0.0f);
                    chr->work[0] |= 2;
                }
                if (flag) {
                    BtlAnim_Request(chr, 0x1A, 0.0f);
                    BtlChar_GetPos(chr)->unk98 *= 0.5f;
                }
                break;
            case 0x1A:
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    BtlChar_GetPos(chr)->unk98 = 0.0f;
                }
                break;
            case 0x15:
                if (BtlInput_TestAction(chr, 0x17, 1)) {
                    chr->work[0] |= 1;
                }
                if (BtlInput_TestAction(chr, 0x18, 1)) {
                    chr->work[0] |= 2;
                }
                if (BtlAnim_GetProgress(chr) > 0.5f) {
                    if (chr->work[0] & 1) {
                        if (BtlChar_TestFlag(chr, 5)) {
                            BtlAct_Request(chr, 0x19);
                        }
                    }
                    if (chr->work[0] & 2) {
                        BtlAct_Request(chr, 0x1A);
                    }
                }
                break;
        }
        BtlDecide_Main(chr, 0x1800);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/*
 * Actions 0x1B (back), 0x1C, 0x1D (left / right of the line to the opponent): a step. Animations 0x1C..0x1E on
 * the ground, 0x28..0x2A in the air (flag 0xE). Moves between 10% and 60% of the animation.
 */
s32 BtlAct_StepHandler(BtlActEChr *chr, s32 phase) {
    s32 voice;
    s32 anim;
    f32 yaw;
    f32 speed;

    if (phase == 0) {
        anim = 0;
        voice = -1;
        if (BtlAct_GetHeight(chr) < 5.0f) {
            BtlChar_ClearFlag(chr, 0xE);
        }
        yaw = BtlOpp_GetYaw(chr);
        switch (BtlAct_GetCurrent(chr)) {
            case 0x1B:
                anim = BtlChar_TestFlag(chr, 0xE) ? 0x28 : 0x1C;
                voice = 9;
                yaw = BtlUtil_WrapAngle(yaw + 3.14159265f);
                break;
            case 0x1C:
                anim = BtlChar_TestFlag(chr, 0xE) ? 0x29 : 0x1D;
                voice = 8;
                yaw = BtlUtil_WrapAngle(yaw - 3.14159265f / 2.0f);
                break;
            case 0x1D:
                anim = BtlChar_TestFlag(chr, 0xE) ? 0x2A : 0x1E;
                voice = 8;
                yaw = BtlUtil_WrapAngle(yaw + 3.14159265f / 2.0f);
                break;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        BtlChar_GetPos(chr)->vel.x = Mathf_Sin(yaw);
        BtlChar_GetPos(chr)->vel.y = 0.0f;
        BtlChar_GetPos(chr)->vel.z = Mathf_Cos(yaw);
        if (BtlChar_TestFlag(chr, 0xE)) {
            BtlCharSnd_PlayCommon(chr, 0x1F);
        } else if (BtlAct_GetCurrent(chr) == 0x1B) {
            BtlCharSnd_PlayCommon(chr, 0x1F);
        } else {
            BtlCharSnd_PlayBank8(chr, 1);
        }
        if (voice >= 0) {
            BtlChar_PlayVoice(chr, voice);
        }
    }
    if (phase == 1) {
        speed = 0.0f;
        yaw = BtlAnim_GetProgress(chr);

        if (BtlAnim_Advance(chr, 0)) {
            chr->work[1] = 1;
        }
        if (BtlAnim_GetProgress(chr) < 0.2f) {
            BtlChar_SetFlag(chr, 0x42);
        }
        switch (BtlAnim_GetId(chr)) {
            case 0x1C:
            case 0x28:
                if (0.1f < yaw && yaw < 0.6f) {
                    speed = BtlMoveParam_GetSpeed(chr, 0xB);
                } else {
                    speed = 0.0f;
                }
                break;
            case 0x1D:
            case 0x1E:
            case 0x29:
            case 0x2A:
                if (0.1f < yaw && yaw < 0.6f) {
                    yaw = BtlMoveParam_GetSpeed(chr, 0xC);
                    speed = BtlAnim_GetLength(chr);
                    speed /= BtlAnim_GetStep(chr);
                    speed = BtlAct_ScaleSpeedByDist(chr, yaw, 3.14159265f / 2.0f, speed * (0.6f - 0.1f));
                } else {
                    speed = 0.0f;
                }
                break;
        }
        BtlMove_Step(chr, 2, 5, 6, speed, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x88);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlMember_GetActiveGauge(chr)->unk28) {
            BtlChar_SetFlag(chr, 0x96);
        }
        BtlMove_RequestOrbit(chr, 5.0f, 10.0f);
    }
    if (phase == 2) {
        if (BtlAnim_GetId(chr) == 0x1C) {
            yaw = BtlParam_GetUnk6C(chr);
        } else {
            yaw = BtlParam_GetUnk68(chr);
        }
        if (chr->work[1]) {
            BtlAct_Request(chr, 0xB);
            anim = 1;
            if (BtlInput_IsHeld(chr, 0x20)) {
                anim = 0x11;
            }
            BtlDecide_Main(chr, anim);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (!BtlAct_HasQueued(chr) && BtlAnim_GetProgress(chr) > 0.4f) {
            BtlDecide_Main(chr, 0x14000);
            BtlDecide_Attack(chr, 0x83);
        }
        if (yaw < BtlAnim_GetProgress(chr)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/* Actions 0x1E, 0x1F: short forward dash (animation 0x1B). 0x1E takes attack inputs; 0x1F queues attacks 0x8F, 0x8D, 0x8E. */
s32 BtlAct_StepInHandler(BtlActEChr *chr, s32 phase) {
    f32 speed;
    f32 accel;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x1B, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x1F);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        if (BtlAnim_GetProgress(chr) < 0.3f) {
            BtlChar_SetFlag(chr, 0x42);
        }
        if (BtlAnim_GetProgress(chr) < 0.7f) {
            speed = BtlMoveParam_GetSpeed(chr, 0xA);
            accel = 10000.0f;
        } else {
            speed = 0.0f;
            accel = BTL_KMH(100.0f);
        }
        BtlMove_Step(chr, 2, 1, 3, speed, accel);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x88);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlMember_GetActiveGauge(chr)->unk28) {
            BtlChar_SetFlag(chr, 0x96);
        }
    }
    if (phase == 2) {
        switch (BtlAct_GetCurrent(chr)) {
            case 0x1E:
                BtlDecide_Attack(chr, 0x1C00803);
                if (BtlAnim_GetProgress(chr) > 0.5f) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                if (BtlDecide_Attack(chr, 0x40)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
            case 0x1F:
                BtlDecide_Attack(chr, 0x2000030);
                BtlDecide_QueueAttack(chr, 0x8F);
                BtlDecide_QueueAttack(chr, 0x8D);
                BtlDecide_QueueAttack(chr, 0x8E);
                if (BtlAnim_GetProgress(chr) > 0.3f) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
        }
    }
}

/*
 * Actions 0x20..0x23: vanishing side-step (animation 0xF6): seven frames of fast movement back (0x20),
 * left (0x21), right (0x22) or up along the line to the opponent (0x23), invulnerable (flags 0x42..0x46).
 */
s32 BtlAct_VanishStepHandler(BtlActEChr *chr, s32 phase) {
    Vec4 dir;
    Vec4 delta;
    Vec4 oppVel;
    s32 *counter = &chr->work[2];
    BtlActEPose *pose = BtlChar_GetPos(chr);
    f32 yaw;
    f32 c;
    f32 sn;

    if (phase == 0) {
        BtlAnim_Play(chr, 0xF6, 0.15f);
        if (BtlAct_GetPrev(chr) == 0x44) {
            BtlChar_SetHeldFlag(chr, 0x83);
            BtlMember_SpendKi(chr, BtlParam_GetAmountA(chr), 0);
        }
        if (BtlChar_TestFlag(chr, 0x74) || BtlChar_TestFlag(chr, 0x75) || BtlChar_TestFlag(chr, 0x76)) {
            chr->work[0] |= 1;
            BtlChar_SetFlag(chr, 0x12C);
            BtlEvent_Raise(chr->player, 0x41);
            BtlChar_SetFrameBits(chr, 0x8000);
            BtlChar_AddStageTimer(2.0f);
        }
        *counter = 7;
        BtlChar_SetFxBit(chr, 0xC);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x20);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        switch (BtlAct_GetCurrent(chr)) {
            case 0x21:
            case 0x22:
                if (BtlChar_TestFlag(chr, 0x13)) {
                    BtlOpp_GetTargetRot(chr, &dir);
                    if (__builtin_fabsf(BtlUtil_WrapAngle(pose->facing - dir.y)) > 3.14159265f / 2.0f) {
                        BtlChar_PlayVoice(chr, 0x1D);
                    }
                }
                break;
        }
        if (BtlChar_TestFlag(chr, 0x78)) {
            chr->work[0] |= 2;
        }
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            chr->work[0] |= 1;
        }
        if (!(chr->work[0] & 1)) {
            BtlMove_Step(chr, 2, 5, 7, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            if (*counter == 7) {
                Vec4_SetZero(&dir);
                BtlOpp_GetDelta(chr, &delta);
                Vec3_Normalize(&delta, &delta);
                yaw = -BtlOpp_GetYaw(chr);
                switch (BtlAct_GetCurrent(chr)) {
                    case 0x21:
                        dir.x = -1.0f;
                        break;
                    case 0x22:
                        dir.x = 1.0f;
                        break;
                    case 0x23:
                        dir.y = delta.y;
                        dir.z = sqrtf(1.0f - delta.y * delta.y);
                        break;
                    case 0x20:
                        dir.z = -1.0f;
                        break;
                }
                Vec3_Normalize(&dir, &dir);
                c = Mathf_Cos(yaw);
                sn = Mathf_Sin(yaw);
                pose->vel.x = dir.x * c - dir.z * sn;
                pose->vel.y = dir.y;
                pose->vel.z = dir.x * sn + dir.z * c;
            }
            --*counter;
            if (*counter >= 2) {
                c = BtlAct_ScaleSpeedByDist(chr, BtlMoveParam_GetSpeed(chr, 0x12), 3.14159265f, 6.0f);
            } else if (*counter == 1) {
                c = 0.0f;
                BtlChar_SetFxBit(chr, 0xD);
            } else {
                c = 0.0f;
            }
            BtlMove_Step(chr, 2, 5, 7, c, 10000.0f);
            BtlMove_ApplyGravity(chr);
            if (!(chr->work[0] & 2)) {
                if (BtlChar_TestFlag(chr, 5)) {
                    BtlOpp_GetVelocity(chr, &oppVel);
                    Vec4_Add(&pose->pos, &pose->pos, &oppVel);
                }
            }
            BtlChar_SetFlag(chr, 0xB);
        }
        BtlMove_RequestOrbit(chr, 5.0f, 10.0f);
        BtlChar_SetFlag(chr, 0x88);
        BtlChar_SetFxBit(chr, 0x3A);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        if (BtlChar_TestFlag(chr, 0x83)) {
            BtlChar_SetFlag(chr, 0x85);
        }
    }
    if (phase == 2) {
        if (!BtlAct_HasQueued(chr)) {
            BtlDecide_Attack(chr, 3);
        }
        if (*counter <= 0) {
            BtlAct_Request(chr, 0xB);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/* Action 0x24: vanish and reappear behind the opponent (animation 0xF6), costs ki. */
s32 BtlAct_VanishBehindHandler(BtlActEChr *chr, s32 phase) {
    s32 *counter = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0xF6, 0.15f);
        BtlMember_SpendKi(chr, BtlParam_GetAmountB(chr), 0);
        BtlChar_SetFxBit(chr, 0xC);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x20);
        BtlChar_PlayVoice(chr, 0x1D);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        *counter = 7;
        BtlChar_SetSmallVibration(chr, 0.2f);
    }
    if (phase == 1) {
        if (!BtlAnim_Advance(chr, 0)) {
            BtlMove_Step(chr, 2, 5, 7, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            --*counter;
            if (*counter >= 2) {
                BtlChar_GetPos(chr)->unk98 = 0.0f;
                BtlChar_GetPos(chr)->unk9C = 0.0f;
            } else if (*counter == 1) {
                BtlChar_SetFxBit(chr, 0xD);
                if (!BtlChar_TestFlag(chr, 0xBA)) {
                    BtlMove_WarpBehindOpponent(chr, 0.0f);
                    BtlMove_TurnYaw(chr, 2, 3.14159265f);
                    BtlMove_TurnPitch(chr, 1, 3.14159265f);
                    BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
                    BtlMove_SetDirection(chr, 3);
                    BtlChar_SetFlag(chr, 0x3F);
                    BtlChar_SetFlag(chr, 0xB);
                    BtlChar_SetFlag(chr, 0xCC);
                    BtlChar_SetFlag(chr, 0x24);
                }
            }
            BtlChar_SetFlag(chr, 0xB);
        }
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
    }
    if (phase == 2) {
        if (!BtlAct_HasQueued(chr)) {
            BtlDecide_Attack(chr, 3);
        }
        if (*counter <= 0) {
            BtlAct_Request(chr, 0xB);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/*
 * Actions 0x25..0x2A: recover in the air (animations 0xFE..0x102), costs ki. 0x29 and 0x2A turn around;
 * 0x26 goes on to action 0xF, 0x28 and 0x2A to action 0x19, the others to 0xB.
 */
void BtlAct_RecoverHandler(BtlActEChr *chr, s32 phase) {
    if (phase == 0) {
        BtlActEPose *pose = BtlChar_GetPos(chr);
        s32 anim = 0xFE;

        if (BtlAct_IsAirMotion(chr, 0)) {
            pose->facing = BtlUtil_WrapAngle(pose->facing + 3.14159265f);
        }
        switch (BtlAct_GetCurrent(chr)) {
            case 0x25:
            case 0x26:
                anim = 0xFE;
                break;
            case 0x27:
                anim = 0xFF;
                break;
            case 0x29:
                anim = 0x100;
                pose->unk98 = 0.0f;
                pose->rot.y = pose->facing = BtlUtil_WrapAngle(pose->facing + 3.14159265f);
                break;
            case 0x28:
                anim = 0x101;
                break;
            case 0x2A:
                anim = 0x102;
                pose->unk98 = 0.0f;
                pose->rot.y = pose->facing = BtlUtil_WrapAngle(pose->facing + 3.14159265f);
                break;
        }
        BtlAnim_Play(chr, anim, 0.0f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlMember_SpendKi(chr, BtlParam_GetRecoverKiCost(chr), 0);
        BtlCharSnd_PlayCommon(chr, 0x21);
    }
    if (phase == 1) {
        switch (BtlAct_GetCurrent(chr)) {
            case 0x26:
                BtlAnim_Advance(chr, 0);
                if (BtlAnim_GetProgress(chr) > 0.4f) {
                    BtlAct_Request(chr, 0xF);
                }
                break;
            case 0x25:
            case 0x27:
            case 0x29:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                break;
            case 0x28:
            case 0x2A:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0x19);
                }
                break;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x33);
    }
}

/*
 * Plays part `frame` of a multi-part attack motion as the sub-animation and returns the updated
 * lead time: each part before the last adds the object's value at +0xBDC (in 60ths) / atk->unk10
 * frames, the last part adds half of two animation marker frames and rounds.
 */
f32 BtlAct_PlayAttackPart(BtlActEChr *chr, BtlActEAttack *atk, s32 frame, f32 time) {
    BtlActEObj *obj = BtlChar_GetObj(chr);

    if (frame < atk->parts - 1) {
        BtlAnim_PlaySub(chr, atk->motion[frame]);
        time += obj->unkBDC * 30.0f / 60.0f / atk->unk10;
    } else if (frame == atk->parts - 1) {
        s32 n;

        BtlAnim_PlaySub(chr, atk->motion[frame]);
        n = BtlObjAnim_QueryEvent(obj, 1, 1, 0);
        n += BtlObjAnim_QueryEvent(obj, 2, 1, 0);
        time += (f32)n * 0.5f * 30.0f / 60.0f;
        time = floorf(time + 0.5f);
    }
    return time;
}

/* Action 0x2B: vanish (animation 0x26), then snap to the opponent and start the queued attack. */
void BtlAct_VanishAttackHandler(BtlActEChr *chr, s32 phase) {
    s32 *timer = &chr->work[3];
    s32 *step = &chr->work[2];

    if (phase == 0) {
        BtlChar_SetFxBit(chr, 0xC);
        BtlCharSnd_PlayCommon(chr, 0x20);
        BtlChar_SetSmallVibration(chr, 0.2f);
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if (phase == 1) {
        BtlAnim_AdvanceThen(chr, 0x26, 0.15f, 0);
        chr->nextAttack.unk18 = BtlAct_PlayAttackPart(chr, &chr->nextAttack, chr->actionFrame, chr->nextAttack.unk18);
        switch (*step) {
            case 0:
                if (++*timer >= 4) {
                    BtlChar_SetFlag(chr, 0xB);
                }
                if (*timer >= 6) {
                    *timer = 0;
                    ++*step;
                }
                break;
            case 1:
                BtlMove_SnapToOpponent(chr);
                BtlChar_SetFlag(chr, 0xB);
                BtlChar_SetFlag(chr, 0xCC);
                if (BtlAct_HasQueued(chr)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                } else {
                    BtlAct_Request(chr, 0xB);
                }
                break;
        }
    }
    if (phase == 3) {
        BtlChar_SetHeldFlag(chr, 0xDD);
    }
}

/* Action 0x2C: the same as 0x2B with a one-frame wait. */
void BtlAct_VanishAttackQuickHandler(BtlActEChr *chr, s32 phase) {
    s32 *step = &chr->work[2];

    if (phase == 0) {
        BtlChar_SetFxBit(chr, 0xC);
        BtlCharSnd_PlayCommon(chr, 0x20);
        BtlChar_SetSmallVibration(chr, 0.2f);
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if (phase == 1) {
        BtlAnim_AdvanceThen(chr, 0x26, 0.15f, 0);
        chr->nextAttack.unk18 = BtlAct_PlayAttackPart(chr, &chr->nextAttack, chr->actionFrame, chr->nextAttack.unk18);
        switch (*step) {
            case 0:
                BtlChar_SetFlag(chr, 0xB);
                ++*step;
                break;
            case 1:
                BtlMove_SnapToOpponent(chr);
                BtlChar_SetFlag(chr, 0xB);
                BtlChar_SetFlag(chr, 0xCC);
                if (BtlAct_HasQueued(chr)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                } else {
                    BtlAct_Request(chr, 0xB);
                }
                break;
        }
    }
    if (phase == 3) {
        BtlChar_SetHeldFlag(chr, 0xDD);
    }
}

/* Action 0x2D: appear behind the opponent on the first frame and start the queued attack. */
void BtlAct_WarpBehindAttackHandler(BtlActEChr *chr, s32 phase) {
    if (phase == 1) {
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        if (!BtlChar_TestFlag(chr, 0xBA)) {
            BtlMove_WarpBehindOpponent(chr, 0.0f);
            BtlMove_TurnYaw(chr, 2, 3.14159265f);
            BtlMove_TurnPitch(chr, 1, 3.14159265f);
            BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
            BtlMove_SetDirection(chr, 3);
            BtlChar_SetFlag(chr, 0x3F);
            BtlChar_SetFlag(chr, 0xB);
            BtlChar_SetFlag(chr, 0xCC);
            BtlChar_SetFlag(chr, 0x24);
        }
        if (BtlAct_HasQueued(chr)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        } else {
            BtlAct_Request(chr, 0xB);
        }
    }
    if (phase == 3) {
        BtlChar_SetHeldFlag(chr, 0xDD);
    }
}

/* Action 0x2E: appear ahead of the (moving) opponent, led by the attack's lead time, and start the queued attack. */
void BtlAct_WarpAheadAttackHandler(BtlActEChr *chr, s32 phase) {
    if (phase == 1) {
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        if (!BtlChar_TestFlag(chr, 0xBA)) {
            BtlMove_WarpAheadOfOpponent(chr, chr->nextAttack.unk18);
            BtlMove_TurnYaw(chr, 2, 3.14159265f);
            BtlMove_TurnPitch(chr, 1, 3.14159265f);
            BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
            BtlMove_SetDirection(chr, 3);
            BtlChar_SetFlag(chr, 0x3F);
            BtlChar_SetFlag(chr, 0xB);
            BtlChar_SetFlag(chr, 0xCC);
            BtlChar_SetFlag(chr, 0x24);
        }
        if (BtlAct_HasQueued(chr)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        } else {
            BtlAct_Request(chr, 0xB);
        }
    }
    if (phase == 3) {
        BtlChar_SetHeldFlag(chr, 0xDD);
    }
}

/*
 * Actions 0x2F, 0x30: slide in four frames to a point beside the opponent's predicted position
 * (35 degrees off at distance 11, or 70 degrees off at distance 8), then face the predicted point.
 */
s32 BtlAct_SlideToAttackHandler(BtlActEChr *chr, s32 phase) {
    Vec4 point;
    Vec4 target;
    Vec4 *step = (Vec4 *)&chr->work[12];
    s32 *counter = &chr->work[2];
    Vec4 *face = (Vec4 *)&chr->work[16];
    BtlActEPose *pose = BtlChar_GetPos(chr);

    if (phase == 0) {
        f32 dist = 0.0f;
        f32 angle = 0.0f;
        f32 extra = 0.0f;
        s32 frames = 4;

        switch (BtlAct_GetCurrent(chr)) {
            case 0x2F:
                angle = BTL_DEG2RAD(-35.0f);
                dist = 11.0f;
                extra = 1.0f;
                frames = 4;
                break;
            case 0x30:
                angle = BTL_DEG2RAD(-70.0f);
                dist = 8.0f;
                break;
        }
        BtlMove_CalcApproachPoint(chr, &point, &target, chr->nextAttack.unk1C, angle, dist, dist + frames + extra);
        Vec4_Sub(step, &point, &pose->pos);
        Vec4_Scale(step, step, 0.25f);
        Vec4_Copy(face, &target);
    }
    if (phase == 1) {
        pose->unk98 = 0.0f;
        pose->unk9C = 0.0f;
        Vec4_Add(&pose->pos, &pose->pos, step);
        if (++*counter == 4) {
            if (BtlAct_HasQueued(chr)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            } else {
                BtlAct_Request(chr, 0xB);
            }
        }
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0xB);
        BtlChar_SetFlag(chr, 0xCC);
        BtlChar_SetFlag(chr, 0x24);
    }
    if (phase == 3) {
        BtlMove_TurnToPoint(chr, face, 3.14159265f, 3.14159265f);
        BtlChar_SetHeldFlag(chr, 0xDD);
    }
}

/*
 * Action 0x31: rush at the opponent (animation 0xD, the dash loop) at the attack's speed until the
 * closing time drops below the attack's lead time, then start the queued attack; 0xE is the stop animation.
 */
void BtlAct_RushToAttackHandler(BtlActEChr *chr, s32 phase) {
    f32 *lead = (f32 *)&chr->work[6];
    s32 *counter = &chr->work[2];
    s32 reach;
    s32 stuck;
    f32 speed;
    f32 accel;

    if (phase == 0) {
        BtlAnim_Play(chr, 0xD, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_SetFlag(chr, 0xD1);
        BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
        BtlChar_SetVibration(chr, 0.8f, 0.2f);
    }
    if (phase == 1) {
        speed = 0.0f;
        *lead = BtlAct_PlayAttackPart(chr, &chr->nextAttack, chr->actionFrame, *lead);
        accel = BTL_KMH(100.0f);
        switch (BtlAnim_GetId(chr)) {
            case 0xD:
                BtlAnim_AdvanceLoop(chr, 0);
                speed = chr->nextAttack.unk1C;
                accel = 10000.0f;
                BtlChar_SetFxBit(chr, 4);
                if (BtlChar_TestFlag(chr, 0xF)) {
                    BtlChar_SetFxBit(chr, 0x30);
                }
                BtlChar_SetFlag(chr, 9);
                BtlChar_SetFlag(chr, 0x50);
                BtlChar_SetFlag(chr, 0x51);
                BtlChar_SetFxBit(chr, 9);
                break;
            case 0xE:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                break;
        }
        BtlMove_Step(chr, 2, 1, 3, speed, accel);
        BtlMove_ApplyGravity(chr);
    }
    if (phase == 2) {
        if (BtlAnim_GetId(chr) == 0xD) {
            reach = 0;
            stuck = 0;
            if (BtlOpp_GetClosingTime(chr) < *lead) {
                reach = 1;
            }
            if (BtlMove_IsBlockedByOpponent(chr)) {
                reach = 1;
            }
            if (Vec3_Length(&BtlChar_GetPos(chr)->unk40) < BTL_KMH(100.0f)) {
                if (++*counter >= 8) {
                    stuck = 1;
                }
            } else {
                *counter = 0;
            }
            if (!BtlChar_TestFlag(chr, 5)) {
                reach = 1;
            }
            if (reach || stuck) {
                BtlChar_GetPos(chr)->unk98 = 0.0f;
                if (BtlAct_HasQueued(chr) && !stuck) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                } else {
                    BtlAnim_Request(chr, 0xE, 0.0f);
                }
            }
        }
    }
    if (phase == 3) {
        BtlChar_SetHeldFlag(chr, 0xDD);
    }
}

/* Action 0x32: hop back (animation 0x28) for half the animation, then start the queued attack. */
void BtlAct_HopBackAttackHandler(BtlActEChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0x28, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x1F);
    }
    if (phase == 1) {
        f32 speed = BtlMoveParam_GetSpeed(chr, 0xB);

        BtlAnim_Advance(chr, 0);
        if (BtlAnim_GetProgress(chr) > 0.5f) {
            if (BtlAct_HasQueued(chr)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            } else {
                BtlAct_Request(chr, 0xB);
            }
        }
        BtlMove_Step(chr, 2, 5, 4, speed, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
    }
    if (phase == 3) {
        BtlChar_SetHeldFlag(chr, 0xDD);
    }
}

/*
 * Action 0x33: dash in a circle around the opponent to get behind it (animations 0x2D -> 0x19 loop ->
 * 0x1A stop), spending ki every frame. Once round far enough, attack 0x90 or an attack input follows.
 */
s32 BtlAct_CircleDashHandler(BtlActEChr *chr, s32 phase) {
    s32 *counter = &chr->work[2];
    f32 *baseYaw = (f32 *)&chr->work[6];
    s32 flag;
    f32 lean;
    f32 speed;

    if (phase == 0) {
        BtlActEPose *pose = BtlChar_GetPos(chr);

        BtlAnim_Play(chr, 0x2D, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_PlayVoice(chr, 5);
        BtlChar_SetFxBit(chr, 6);
        pose->unk98 = 0.0f;
        pose->unk9C = 0.0f;
        if (0.0f < BtlOpp_GetYawFromItsFacing(chr)) {
            chr->work[0] |= 1;
        }
        *baseYaw = BtlUtil_WrapAngle(pose->rot.y + 3.14159265f);
    }
    if (phase == 1) {
        flag = 0;
        lean = 0.0f;
        switch (BtlAnim_GetId(chr)) {
            case 0x2D:
                BtlAnim_AdvanceThen(chr, 0x19, 0.15f, 0);
                lean = 1.0f;
                break;
            case 0x19:
                BtlAnim_AdvanceLoop(chr, 0);
                lean = 1.0f;
                flag = 1;
                if (BtlAnim_IsNew(chr)) {
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                    BtlChar_Vibrate(chr, lean, 0.3f);
                    ChrCam_RequestCut(chr, 1, 4);
                }
                break;
            case 0x1A:
                BtlAnim_SetDuration(chr, 0.3f);
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                lean = 1.0f - BtlAnim_GetProgress(chr);
                break;
        }
        if (flag) {
            speed = BtlMoveParam_GetSpeed(chr, 0x13);
            if (BtlMove_CircleOpponent(chr, chr->work[0] & 1, BtlOpp_GetSeenAction(chr) == 0xD5, *baseYaw, speed)) {
                chr->work[0] |= 2;
            }
            BtlChar_SetFlag(chr, 9);
            BtlChar_SetFlag(chr, 0x50);
            BtlChar_SetFlag(chr, 0x51);
            BtlChar_SetFxBit(chr, 4);
            BtlChar_SetFlag(chr, 0x47);
        } else {
            BtlMove_Step(chr, (chr->work[0] & 2) ? 2 : 6, 6, 7, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->speed * lean);
        BtlChar_SetFlag(chr, 0x89);
    }
    if (phase == 2) {
        flag = 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x2D:
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    flag = 1;
                    BtlChar_GetPos(chr)->unk98 = 0.0f;
                }
                break;
            case 0x19:
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    flag = 1;
                    BtlChar_GetPos(chr)->unk98 = 0.0f;
                }
                if (chr->work[0] & 2) {
                    flag = 1;
                    BtlChar_GetPos(chr)->unk98 = 0.0f;
                }
                if (BtlMember_SpendKi(chr, BtlMoveParam_GetKiCost(chr, 3), 0)) {
                    flag = 1;
                    BtlChar_GetPos(chr)->unk98 *= 0.2f;
                }
                if (!BtlChar_TestFlag(chr, 5)) {
                    flag = 1;
                }
                if (Vec3_Length(&BtlChar_GetPos(chr)->unk40) < BTL_KMH(1000.0f)) {
                    if (++*counter >= 6) {
                        flag = 1;
                        BtlChar_GetPos(chr)->unk98 = 0.0f;
                    }
                } else {
                    *counter = 0;
                }
                break;
            case 0x1A:
                break;
        }
        if (flag) {
            BtlAnim_Request(chr, 0x1A, 0.15f);
        }
        if (BtlChar_TestFlag(chr, 0x13)) {
            speed = 3.14159265f / 4.0f;
            if (BtlAnim_GetId(chr) == 0x1A) {
                speed = 3.14159265f / 2.0f;
            }
            if (__builtin_fabsf(BtlUtil_WrapAngle(BtlOpp_GetYaw(chr) - *baseYaw)) < speed) {
                if (BtlDecide_QueueAttack(chr, 0x90)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                if (BtlDecide_Attack(chr, 0x4000000)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
            }
        }
    }
}

/*
 * Action 0x34: rush dash at the opponent (animations 0x23 -> 0x24 loop -> 0x25 stop) at 1800 km/h.
 * Characters with parameter flag bit 20 do it vanished and invulnerable at 2200 km/h.
 */
void BtlAct_RushDashHandler(BtlActEChr *chr, s32 phase) {
    s32 fast = (BtlParam_GetFlags(chr) >> 20) & 1;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x23, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        if (fast) {
            BtlChar_SetFxBit(chr, 0xE);
            BtlCharSnd_PlayCommon(chr, 0x20);
        } else {
            BtlCharSnd_PlayCommon(chr, 0x31);
        }
        BtlChar_SetSmallVibration(chr, 0.2f);
    }
    if (phase == 1) {
        s32 yawMode = 2;
        f32 accel = 10000.0f;
        s32 pitchMode = 1;
        f32 speed;

        if (fast) {
            speed = 0.0f;
            BtlChar_SetFlag(chr, 0xB);
            BtlChar_SetFlag(chr, 0x42);
            BtlChar_SetFlag(chr, 0x43);
            BtlChar_SetFlag(chr, 0x44);
            BtlChar_SetFlag(chr, 0x45);
            BtlChar_SetFlag(chr, 0x46);
            if (chr->actionFrame != 0) {
                speed = BTL_KMH(2200.0f);
            }
        } else {
            speed = BTL_KMH(1800.0f);
        }
        switch (BtlAnim_GetId(chr)) {
            case 0x23:
                BtlAnim_AdvanceThen(chr, 0x24, 0.15f, 0);
                break;
            case 0x24:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
            case 0x25:
                BtlAnim_SetDuration(chr, 0.1f);
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                    BtlChar_GetPos(chr)->unk98 = 0.0f;
                    BtlChar_GetPos(chr)->unk9C = 0.0f;
                }
                speed = 0.0f;
                yawMode = 6;
                accel = BTL_KMH(100.0f);
                pitchMode = 6;
                break;
        }
        BtlMove_Step(chr, yawMode, pitchMode, 3, speed, accel);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFxBit(chr, 9);
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == 2) {
        if (BtlAnim_GetId(chr) == 0x24) {
            if (BtlMove_IsBlockedByOpponent(chr)) {
                BtlAnim_Request(chr, 0x25, 0.0f);
            }
        }
        if (BtlDecide_QueueAttack(chr, 0x9C)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlInput_TestAction(chr, 0x34, 1)) {
            BtlAct_Request(chr, 0x46);
        }
        if (BtlInput_TestAction(chr, 0x5D, 1)) {
            BtlAct_Request(chr, 0xB5);
        }
    }
    if (phase == 3) {
        if (fast) {
            BtlChar_SetFxBit(chr, 0xD);
        }
    }
}

/*
 * Action 0x35: vanished, invulnerable dash at 2000 km/h in the direction held (animations 0xC -> 0xD
 * loop -> 0xE stop); stops after 13 frames or when it runs into the opponent head on.
 */
s32 BtlAct_VanishDashHandler(BtlActEChr *chr, s32 phase) {
    s32 flag;

    if (phase == 0) {
        BtlAnim_Play(chr, 0xC, 0.15f);
        BtlChar_SetFxBit(chr, 0xE);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x20);
        BtlMove_TurnYaw(chr, 0, 3.14159265f);
        BtlMove_TurnPitch(chr, 2, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
    }
    if (phase == 1) {
        flag = 1;
        switch (BtlAnim_GetId(chr)) {
            case 0xC:
                BtlAnim_SetDuration(chr, 0.05f);
                BtlAnim_AdvanceThen(chr, 0xD, 0.15f, 0);
                if (BtlAnim_IsNew(chr)) {
                    flag = 0;
                }
                break;
            case 0xD:
                BtlAnim_AdvanceLoop(chr, 0);
                BtlChar_SetFlag(chr, 0xB);
                break;
            case 0xE:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                if (BtlAnim_IsNew(chr)) {
                    BtlChar_SetFxBit(chr, 0xD);
                    BtlChar_GetPos(chr)->unk98 = BTL_KMH(1000.0f);
                }
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    BtlChar_GetPos(chr)->unk98 = 0.0f;
                }
                flag = 0;
                break;
        }
        if (flag) {
            BtlMove_Step(chr, 6, 6, 3, BTL_KMH(2000.0f), 10000.0f);
            BtlChar_SetFlag(chr, 9);
            BtlChar_SetFlag(chr, 0x42);
            BtlChar_SetFlag(chr, 0x43);
            BtlChar_SetFlag(chr, 0x44);
            BtlChar_SetFlag(chr, 0x45);
            BtlChar_SetFlag(chr, 0x46);
            BtlChar_SetFlag(chr, 0x47);
        } else {
            BtlMove_Step(chr, 6, 6, 3, 0.0f, BTL_KMH(100.0f));
        }
        BtlMove_ApplyGravity(chr);
        BtlMove_RequestOrbit(chr, 50.0f, 100.0f);
        BtlChar_SetFlag(chr, 0xBC);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0xC:
            case 0xD:
                flag = 1; if (chr->actionFrame < 0xD) { flag = 0; }
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    if (__builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) < 3.14159265f / 4.0f) {
                        flag = 1;
                    }
                }
                if (flag) {
                    BtlAnim_Request(chr, 0xE, 0.15f);
                }
                break;
            case 0xE:
                BtlDecide_Attack(chr, 3);
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
                break;
        }
        if (chr->actionFrame >= 7) {
            BtlDecide_Main(chr, 0x20000000);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/*
 * Action 0xFA (forced by flag 0x61): clash B, the two fighters trading blows. Every accepted input
 * (input condition 0x33) adds to clashCountB; abilities and a technique in progress add more on a
 * timer. The clash sequence raises 0xBF on the winner (animation 0x175) and 0xC0 on the loser (0x176).
 */
void BtlAct_ClashBlowsHandler(BtlActEChr *chr, s32 phase) {
    s32 *bonus = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, BtlClash_Side(chr->player) == 0 ? 0x173 : 0x174, 0.15f);
        chr->clashCountB = 0;
        if (BtlAct_IsTechniqueId(BtlAct_GetPrev(chr))) {
            chr->work[0] |= 1;
            switch (BtlAct_GetPrevClass(chr)) {
                case 2:
                    *bonus = BtlSuper_GetKiCost(chr, 2) / 2;
                    break;
                case 3:
                    *bonus = BtlSuper_GetKiCost(chr, 3) / 2;
                    break;
                case 4:
                    *bonus = 100000;
                    break;
            }
        }
        BtlChar_ClearFlagRange(chr, 0xBF, 0xC3);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
            case 0x173:
            case 0x174:
                BtlAnim_AdvanceLoop(chr, 0);
                BtlChar_SetFlag(chr, 0xE6);
                BtlChar_SetFlag(chr, 0xED);
                if (BtlInput_TestAction(chr, 0x33, 1)) {
                    BtlAct_CountAndMarkOpponent(chr);
#ifdef PORT /* PC build: BT3_CLASH_LOG=1 */
                    Port_ClashLog(chr->player, "blow clash", 1, chr->clashCountB, chr->actionFrame);
#endif
                } else if (BtlClash_SequenceActive(chr->player)) {
                    /* No passive mash credit while waiting for the next button. */
                } else if (BtlMember_HasAbility(chr, 0x6A)) {
                    if (chr->actionFrame % 28 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                } else if (BtlMember_HasAbility(chr, 0x69)) {
                    if (chr->actionFrame % 8 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                } else if (BtlMember_HasAbility(chr, 0x68)) {
                    if (chr->actionFrame % 5 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                } else {
                    if (chr->actionFrame % 4 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                }
                if (!BtlClash_SequenceActive(chr->player) && (chr->work[0] & 1)) {
                    if (chr->actionFrame % 15 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                }
                if (BtlClash_SequenceActive(chr->player)) {
                    /* Sequence scores come only from accepted button presses. */
                } else if (BtlMember_HasAbility(chr, 0x14)) {
                    if (chr->actionFrame % 5 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                } else if (BtlMember_HasAbility(chr, 0x13)) {
                    if (chr->actionFrame % 11 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                } else if (BtlMember_HasAbility(chr, 0x12)) {
                    if (chr->actionFrame % 17 == 0) {
                        BtlAct_CountAndMarkOpponent(chr);
                    }
                }
                chr->unkD48 = 30;
                if (BtlChar_TestFlag(chr, 0xBF)) {
                    BtlAnim_Request(chr, 0x175, 0.15f);
                    BtlAct_CountAndMarkOpponent(chr);
                    chr->unkD48 = 0;
                    if (*bonus > 0) {
                        BtlMember_AddKi(chr, *bonus);
                        if (BtlChar_TestFlag(chr, 6)) {
                            BtlMember_AddMaxPower(chr, 30000);
                        }
                    }
                }
                if (BtlChar_TestFlag(chr, 0xC0)) {
                    BtlAnim_Request(chr, 0x176, 0.15f);
                }
                break;
            case 0x175:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                break;
            case 0x176:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xD5);
                }
                if (BtlAnim_PassedRatio(chr, 0.5f)) {
                    BtlMember_Damage(chr, BtlOpp_GetClashCountB(chr) * 60, 8);
                    BtlChar_PlayVoice(chr, 0x1A);
                }
                BtlChar_SetFlag(chr, 0x95);
                BtlChar_SetFlag(chr, 0x96);
                break;
        }
        BtlMove_SteerAtOpponent(chr, BTL_KMH(100.0f), 0.0f, 0.0f, 0.0f, 0.0f, 3.14159265f / 4.0f);
        BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
        BtlChar_GetPos(chr)->facing = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->facing + 0.5f);
        BtlMove_SetDirection(chr, 3);
        BtlMove_Advance(chr, BTL_KMH(100.0f), 10000.0f);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x128);
        BtlChar_Vibrate(chr, 1.0f, 0.1f);
        BtlChar_SetFlag(chr, 0xD2);
        BtlChar_SetFlag(chr, 0x134);
        BtlChar_SetFlag(chr, 0xEE);
        BtlChar_SetFlag(chr, 0x13B);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
    }
    if (phase == 3) {
        chr->clashCountB = 0;
        BtlChar_ClearFlagRange(chr, 0xBF, 0xC3);
    }
}

/* Action 0xFB (forced by flag 0x62): start of clash C. Vanish (animation 0xF6), then seven frames later action 0xFC. */
s32 BtlAct_ClashVanishHandler(BtlActEChr *chr, s32 phase) {
    s32 *counter = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0xF6, 0.1f);
        chr->clashCountB = 0;
        BtlChar_SetFxBit(chr, 0xC);
        BtlCharSnd_PlayCommon(chr, 0x20);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlChar_SetFlag(chr, 0xB);
            if (++*counter >= 7) {
                BtlAct_Request(chr, 0xFC);
            }
        }
        BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x128);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
    }
}

/*
 * Action 0xFC: one exchange of clash C (the pair vanishing and reappearing along a stage path).
 * After a wait set by the clash level the fighter strikes (animation 0x4C, 0x3B, 0x4F or 0x40 in turn);
 * the frame on which the button the sequence picked is pressed goes to chr +0xE58 (-2 = wrong button).
 * The sequence answers with flags 0xC5 (again), 0xC6 (over), 0xC7 (won) and 0xC8 (lost).
 */
s32 BtlAct_ClashExchangeHandler(BtlActEChr *chr, s32 phase) {
    BtlActEClashC *c = BtlClash_Exchange(chr->player);
    s32 *counter = &chr->work[2];
    s32 wait;
    f32 duration;
    f32 speed;

    if (phase == 0) {
        s32 anim = 0;

        if (BtlAct_GetPrev(chr) != 0xFC) {
            chr->unkE54 = 0;
        } else {
            chr->unkE54++;
        }
        switch (chr->unkE54 % 4) {
            case 0:
                anim = 0x4C;
                break;
            case 1:
                anim = 0x3B;
                break;
            case 2:
                anim = 0x4F;
                break;
            case 3:
                anim = 0x40;
                break;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        BtlAnim_EnableHandle(chr);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_RequestPlaceOnPath(chr);
        chr->unkE58 = -1;
    }
    if (phase == 1) {
        duration = 0.4f;
        speed = 0.0f;
        wait = 15;
        switch (c->level) {
            case 0:
                duration = 0.5f;
                wait = 18;
                break;
            case 1:
                break;
            case 2:
                wait = 12;
                duration = 0.3f;
                break;
        }
        switch (BtlAnim_GetId(chr)) {
            case 0x3B:
            case 0x40:
            case 0x4C:
            case 0x4F:
                if (*counter < wait) {
                    ++*counter;
                    BtlChar_SetFlag(chr, 0xB);
                } else {
                    BtlAnim_SetDuration(chr, duration);
                    BtlAnim_Advance(chr, 0);
                    if (BtlAnim_GetProgress(chr) < 0.1f) {
                        BtlChar_SetFlag(chr, 0xB);
                    } else {
                        speed = BTL_KMH(2000.0f);
                    }
                    if (BtlAnim_PassedRatio(chr, 0.1f)) {
                        BtlChar_SetFxBit(chr, 0xC);
                        BtlChar_Vibrate(chr, 1.0f, 0.3f);
                        if (BtlClash_Side(chr->player) == 0) {
                            BtlChar_SetFxBit(chr, 0x2D);
                            BtlCharSnd_PlayCommon(chr, 0x4F);
                            BtlCharSnd_PlayCommon(chr, 0x4B);
                        }
                    }
                    if (BtlAnim_GetProgress(chr) > 0.7f) {
                        BtlChar_SetFlag(chr, 0xB);
                        BtlChar_SetFlag(chr, 0xC4);
                    }
                }
                if (BtlClash_SequenceActive(chr->player)) {
                    if(BtlClash_SequenceInput(chr->player)) {
                        BtlAct_CountAndMarkOpponent(chr);
                        if(!chr->injected) BtlCharSnd_PlayCommonFar(chr,0x14);
                    }
                } else if (chr->actionFrame > 0) {
                    BtlChar_SetFlag(chr, c->pick + 0xE9);
                    if (chr->unkE58 == -1) {
                        if (BtlInput_IsPressed(chr, 0x78000000)) {
                            if (BtlInput_IsPressed(chr, 0x8000000 << c->pick)) {
                                chr->unkE58 = chr->actionFrame;
                                if (!chr->injected) {
                                    BtlCharSnd_PlayCommonFar(chr, 0x14);
                                }
                            } else if (c->prevPick < 0 || !BtlInput_IsPressed(chr, 0x8000000 << c->prevPick)) {
                                chr->unkE58 = -2;
                            }
                        }
                    }
                }
                BtlChar_SetFlag(chr, 0x18);
                chr->unkD48 = 30;
                break;
            case 0x175:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                break;
            case 0x176:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xD5);
                }
                if (BtlAnim_PassedRatio(chr, 0.5f)) {
                    BtlMember_Damage(chr, BtlOpp_GetClashCountB(chr) * 600, 8);
                    BtlChar_PlayVoice(chr, 0x1A);
                }
                BtlChar_SetFlag(chr, 0x95);
                BtlChar_SetFlag(chr, 0x96);
                break;
        }
        BtlMove_Step(chr, 2, 5, 2, speed, 10000.0f);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x128);
        BtlChar_SetFlag(chr, 0x134);
        BtlChar_SetFlag(chr, 0xEE);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 0x91);
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0xC5)) {
            BtlAct_Request(chr, 0xFC);
        }
        if (BtlChar_TestFlag(chr, 0xC6)) {
            BtlAct_Request(chr, 0xB);
        }
        if (BtlChar_TestFlag(chr, 0xC7)) {
            BtlAnim_Request(chr, 0x175, 0.0f);
            chr->unkD48 = 0;
        }
        if (BtlChar_TestFlag(chr, 0xC8)) {
            BtlAnim_Request(chr, 0x176, 0.0f);
        }
    }
    if (phase == 3) {
        if (BtlAct_GetRequested(chr) != 0xFC) {
            chr->clashCountB = 0;
        }
    }
}
