#include "common.h"
#include "battle/btl_act_c.h"

/*
 * Fighter action handlers, third slice: 0x1EA5F8..0x1EE058 (a slice of the original handler file: its float
 * pool 0x2FD7C8..0x2FD988 and jump tables 0x2EF920..0x2EFA34 run on from the neighbours).
 * Handler protocol (phase 0 enter, 1 run, 2 decide, 3 leave): include/battle/btl_char_action.h. The handlers
 * are int functions without a return statement.
 *
 *   0xDA..0xDE  BtlAct_DownHandler          0x36  BtlAct_SearchHandler      0x3E  BtlAct_Action3E
 *   0xE1..0xE5  BtlAct_GetUpHandler         0x37  BtlAct_ChargeHandler      0x3F  BtlAct_Action3F
 *   0xE6        BtlAct_ActionE6             0x38  BtlAct_GuardHandler       0x40  BtlAct_Action40
 *   0xE7        BtlAct_ActionE7             0x39  BtlAct_Action39           0x41  BtlAct_Action41
 *   0xE8, 0xE9  BtlAct_ActionE8E9           0x3A  BtlAct_Action3A           0x42  BtlAct_Action42
 *   0xEA        BtlAct_ActionEA             0x3B  BtlAct_Action3B           0x43  BtlAct_Action43
 *   0xEB        BtlAct_ActionEB             0x3C  BtlAct_Action3C           0xAE  BtlAct_KiBlastHandler
 *   0xD3        BtlAct_StunHandler          0x3D  BtlAct_Action3D           0xAF  BtlAct_ChargedKiBlastHandler
 *   0xE0        BtlAct_AirStunHandler                                       0xB0  BtlAct_DashKiBlastHandler
 *                                                                           0xB1  BtlAct_DashChargedKiBlastHandler
 *
 * Flags used all over this file (meanings inferred from use): 0xE held = airborne, 0xF = standing on the
 * ground, 0x11 = no ground under the feet, 5 = locked on to the opponent. Most run phases end with
 * "flag 0x11 -> hold 0xE" and some with "flag 0xF -> clear 0xE".
 */

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)

typedef BtlActCChr Chr;

extern void BtlAct_Request(Chr *chr, s32 id);
extern void BtlAct_SetQueue(Chr *chr, u32 slot, s32 id);
extern s32 BtlAct_HasQueued(Chr *chr);
extern s32 BtlAct_GetCurrent(Chr *chr);
extern s32 BtlAct_GetPrev(Chr *chr);
extern s32 BtlAct_GetQueued(Chr *chr);
extern f32 BtlAct_GetFacingRelCam(Chr *chr);
extern void BtlAct_SetPitchMotion(Chr *chr, s32 motionUp, s32 motionDown, s32 recalc);
extern s32 BtlAct_IsAirMotion(Chr *chr, s32 useSaved);
extern void BtlAct_PushDir(Chr *chr, Vec4 *dir, f32 speed, f32 arg);

extern s32 BtlAnim_Advance(Chr *chr, s32 flags);
extern void BtlAnim_AdvanceLoop(Chr *chr, s32 flags);
extern s32 BtlAnim_AdvanceThen(Chr *chr, s32 next, f32 blend, s32 flags);
extern u32 BtlAnim_GetFlags(u32 anim);
extern f32 BtlAnim_GetFrame(Chr *chr);
extern s32 BtlAnim_GetId(Chr *chr);
extern s32 BtlAnim_GetNextId(Chr *chr);
extern f32 BtlAnim_GetProgress(Chr *chr);
extern s32 BtlAnim_IsNew(Chr *chr);
extern s32 BtlAnim_PassedRatio(Chr *chr, f32 ratio);
extern void BtlAnim_Play(Chr *chr, s32 anim, f32 blend);
extern void BtlAnim_PlaySub(Chr *chr, s32 anim);
extern void BtlAnim_Request(Chr *chr, s32 anim, f32 blend);
extern void BtlAnim_SetDuration(Chr *chr, f32 seconds);

extern void BtlCharSnd_PlayBank8(Chr *chr, s32 id);
extern void BtlCharSnd_PlayCommon(Chr *chr, s32 id);
extern void BtlChar_ClearFlag(Chr *chr, s32 n);
extern s32 BtlChar_FrameMod(s32 n);
extern void *BtlChar_GetObj(Chr *chr);
extern BtlActCPose *BtlChar_GetPos(Chr *chr);
extern s32 BtlChar_IsFree(Chr *chr);
extern s32 BtlChar_IsStage4Or27(void);
extern void BtlChar_PlayVoice(Chr *chr, s32 kind);
extern void BtlChar_PlayVoiceSingle(Chr *chr, s32 kind);
extern void BtlChar_SetFlag(Chr *chr, s32 n);
extern void BtlChar_SetFxBit(Chr *chr, s32 n);
extern void BtlChar_SetHeldFlag(Chr *chr, s32 n);
extern void BtlChar_SetLookEnabled(Chr *chr, s32 enabled);
extern void BtlChar_SetVibration(Chr *chr, f32 power, f32 seconds);
extern s32 BtlChar_TestFlag(Chr *chr, s32 n);
extern s32 BtlChar_TestMemberUnk70(Chr *chr);
extern void BtlChar_Vibrate(Chr *chr, f32 power, f32 seconds);
extern s32 BtlInput_IsHeld(Chr *chr, u32 mask);
extern s32 BtlInput_IsPressed(Chr *chr, u32 mask);
extern s32 BtlInput_TestAction(Chr *chr, s32 id, s32 want);

extern s32 BtlMember_AddKi(Chr *chr, s32 amount);
extern void BtlMember_AddMaxPower(Chr *chr, s32 amount);
extern s32 BtlMember_Damage(Chr *chr, s32 amount, s32 flags);
extern BtlActCMember *BtlMember_GetActive(Chr *chr);
extern BtlActCGauge *BtlMember_GetActiveGauge(Chr *chr);
extern s32 BtlMember_HasAbility(Chr *chr, s32 n);
extern s32 BtlMember_HasBlast(Chr *chr, s32 amount);
extern s32 BtlMember_IsKiFull(Chr *chr);
extern s32 BtlMember_IsMaxPowerFull(Chr *chr);
extern s32 BtlMember_SpendKi(Chr *chr, s32 amount, s32 force);
extern void BtlMember_SubBlast(Chr *chr, s32 amount);

extern void BtlMove_Advance(Chr *chr, f32 speed, f32 accel);
extern void BtlMove_ApplyGravity(Chr *chr);
extern void BtlMove_BrakeVertical(Chr *chr);
extern void BtlMove_Fall(Chr *chr);
extern void BtlMove_MoveVertical(Chr *chr, f32 speed, f32 accel);
extern void BtlMove_RequestOrbit(Chr *chr, f32 near, f32 far);
extern void BtlMove_SetDirection(Chr *chr, s32 mode);
extern void BtlMove_SetLeanX(Chr *chr, f32 v);
extern void BtlMove_SteerAtOpponent(Chr *chr, f32 closeSpeed, f32 yawAccel, f32 pitchAccel, f32 yawMax, f32 pitchMax, f32 maxStep);
extern void BtlMove_Step(Chr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel);
extern void BtlMove_TurnModelYaw(Chr *chr, f32 maxStep, f32 rate);
extern void BtlMove_TurnPitch(Chr *chr, s32 mode, f32 maxStep);
extern void BtlMove_TurnYaw(Chr *chr, s32 mode, f32 maxStep);

extern f32 BtlOpp_GetDistanceXZ(Chr *chr);
extern f32 BtlOpp_GetGapXZ(Chr *chr);
extern s32 BtlOpp_GetParamWord0(Chr *chr);
extern s32 BtlOpp_GetSeenAction(Chr *chr);
extern f32 BtlOpp_GetYawFromFacing(Chr *chr);
extern void BtlStat_ClearPenalty(Chr *chr);
extern s32 BtlUtil_AngleToSector(f32 a);
extern f32 BtlUtil_MinF(f32 a, f32 b);
extern f32 BtlUtil_WrapAngle(f32 a);
extern void ChrCam_AddShake(Chr *chr, f32 strength, f32 time);
extern void ChrCam_RequestCut(Chr *chr, s32 kind, s32 cut);
extern f32 Mathf_Cos(f32 a);
extern f32 Mathf_Sin(f32 a);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);

extern void BtlActB_SetReactionFlags(Chr *chr);
extern s32 BtlActB_TickMemberChange(Chr *chr, s32 *work);
extern s32 BtlObjAnim_MaskToNode(s32 bits);
extern s32 BtlObjAnim_QueryEvent(void *obj, u64 a, s32 b, s32 c);
extern f32 BtlMoveParam_GetSpeed(Chr *chr, s32 n);
extern f32 BtlMoveParam_GetTurnRate(Chr *chr, s32 n);
extern s32 BtlChar_ClearFlagRet(Chr *chr, s32 n) __asm__("BtlChar_ClearFlag");
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_CalcAimDir(s32 objId, s32 node, Vec4 *pos, Vec4 *out, f32 min, f32 max);
extern s32 BtlKiBlast_GetKiCost(Chr *chr);
extern s32 BtlKiBlast_GetHitsOf(Chr *chr, s32 n);
extern u32 BtlParam_GetFlags(Chr *chr);
extern s32 BtlParam_GetUnk0(Chr *chr);
extern f32 BtlParam_GetTypeValueA(Chr *chr);
extern s32 BtlParam_IsType2to4(Chr *chr);
extern s32 BtlParam_GetRateA(Chr *chr);
extern s32 BtlParam_GetRateB(Chr *chr);
extern s32 BtlParam_GetStepA(Chr *chr);
extern s32 BtlParam_GetChargeStartSound(Chr *chr);
extern s32 BtlParam_GetChargeLoopSound(Chr *chr);
extern s32 BtlParam_GetMaxPowerSound(Chr *chr);
extern s32 BtlParam_CanFly(Chr *chr);
extern f32 BtlParam_GetTypeValueB(Chr *chr);
extern s32 BtlDecide_Common(Chr *chr, s32 arg);
extern void BtlDecide_Main(Chr *chr, s32 mask);
extern s32 BtlDecide_Attack(Chr *chr, s32 arg);
extern void BtlDecide_QueueAttack(Chr *chr, s32 attack);
extern s32 BtlAct_CheckRecoveryInput(Chr *chr, s32 arg);
extern s32 BtlAct_GetDownAction(Chr *chr);
extern s32 BtlAct_GetEvasionAttack(Chr *chr);
extern s32 BtlAct_GetIdleFollowUp(Chr *chr);

/* BtlChar_SetFlag as the tail call of an int function (`return f();`). */
extern s32 BtlChar_SetFlagRet(Chr *chr, s32 n) __asm__("BtlChar_SetFlag");

/*
 * Actions 0xDA..0xDE: lying / hitting the ground after a knock-down (name from BtlAct_GetDownAction and the
 * recovery check; the animation meanings are inferred). 0xDA plays 0xB9 / 0xBA (second of each pair = the
 * "air" variant of BtlAct_IsAirMotion) and keeps the airborne flag; the others clear it and add a voice, a
 * sound and an effect bit. At the end of the animation 0xDC / 0xDD go on to 0xDB, the rest to the down action;
 * on stages 4 / 27 a free fighter with flag 0x17 goes to 0xE6 instead. While gauge +0x28 is set, any attack
 * button press (button bit 20) takes one off chr +0x1000. Decide: once free, with that counter used up (or
 * gauge +0x28 clear) and chr +0xD44 >= 10, the recovery input check queues the next action.
 */
s32 BtlAct_DownHandler(Chr *chr, s32 phase) {
    s32 air;
    s32 anim;
    s32 common;
    s32 bank8;
    s32 voice;
    s32 fx;
    s32 clear;

    if (phase == 0) {
        common = -1;
        air = BtlAct_IsAirMotion(chr, 0);
        anim = 0;
        bank8 = -1;
        voice = -1;
        fx = -1;
        clear = 1;
        switch (BtlAct_GetCurrent(chr)) {
            case 0xDA:
                anim = air ? 0xBA : 0xB9;
                clear = 0;
                break;
            case 0xDB:
                anim = air ? 0xCB : 0xCA;
                bank8 = 2;
                fx = 0x34;
                break;
            case 0xDC:
                anim = air ? 0xCD : 0xCC;
                bank8 = 3;
                voice = 0;
                fx = 0x34;
                break;
            case 0xDD:
                anim = air ? 0xCF : 0xCE;
                common = 0x25;
                voice = 1;
                fx = 0x35;
                break;
            case 0xDE:
                anim = air ? 0xCB : 0xCA;
                common = 0x25;
                voice = 1;
                fx = 0x35;
                break;
        }
        BtlAnim_Play(chr, anim, 0.0f);
        if (clear) {
            BtlChar_ClearFlag(chr, 0xE);
        }
        if (voice >= 0) {
            BtlChar_PlayVoice(chr, voice);
        }
        if (common >= 0) {
            BtlCharSnd_PlayCommon(chr, common);
        }
        if (bank8 >= 0) {
            BtlCharSnd_PlayBank8(chr, bank8);
        }
        if (fx >= 0) {
            BtlChar_SetFxBit(chr, fx);
        }
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            switch (BtlAct_GetCurrent(chr)) {
                case 0xDA:
                case 0xDB:
                case 0xDE:
                    BtlAct_Request(chr, BtlAct_GetDownAction(chr));
                    break;
                case 0xDC:
                case 0xDD:
                    BtlAct_Request(chr, 0xDB);
                    break;
            }
            if (BtlChar_IsStage4Or27() && BtlChar_IsFree(chr) && BtlChar_TestFlag(chr, 0x17)) {
                BtlAct_Request(chr, 0xE6);
            }
        }
        BtlMove_Step(chr, 6, 5, 6, 0.0f, BTL_KMH(50.0f) / 3.0f);
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0 && BtlChar_IsFree(chr)) {
            BtlChar_SetFlag(chr, 0xE7);
            if (BtlInput_IsPressed(chr, 0x100000)) {
                chr->unk1000--;
            }
        }
    }
    if (phase == 2) {
        if (BtlChar_IsFree(chr)) {
            if (BtlMember_GetActiveGauge(chr)->unk28 == 0 || chr->unk1000 <= 0) {
                if (chr->unkD44 >= 10) {
                    if (BtlAct_CheckRecoveryInput(chr, 0)) {
                        BtlAct_Request(chr, BtlAct_GetQueued(chr));
                    }
                }
            }
        }
    }
}

/*
 * Actions 0xE1..0xE5: getting up (inferred: they end in BtlAct_GetIdleFollowUp). 0xE1..0xE3 play 0xE2/0xE3,
 * 0xE4/0xE5, 0xE6/0xE7 (ground / air variant); 0xE4 and 0xE5 play 0xFE, 0xE5 after turning the facing by pi.
 * Random draw: in 0xE1, when gauge +0x20 is set and gauge +0x2C is not, BtlChar_FrameMod(2) picks voice 0x20
 * or 0x21, once (gauge +0x2C is then set).
 */
s32 BtlAct_GetUpHandler(Chr *chr, s32 phase) {
    s32 anim;
    s32 air;
    BtlActCPose *pose;

    if (phase == 0) {
        anim = 0;
        air = BtlAct_IsAirMotion(chr, 1);
        switch (BtlAct_GetCurrent(chr)) {
            case 0xE1:
                anim = air ? 0xE3 : 0xE2;
                break;
            case 0xE2:
                anim = air ? 0xE5 : 0xE4;
                break;
            case 0xE3:
                anim = air ? 0xE7 : 0xE6;
                break;
            case 0xE4:
                anim = 0xFE;
                break;
            case 0xE5:
                anim = 0xFE;
                pose = BtlChar_GetPos(chr);
                pose->facing = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->facing + 3.14159265f);
                break;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        if (BtlAct_GetCurrent(chr) != 0xE1) {
            BtlCharSnd_PlayCommon(chr, 0x1F);
        }
        if (BtlAct_GetCurrent(chr) == 0xE1) {
            if (BtlMember_GetActiveGauge(chr)->unk20 != 0 && BtlMember_GetActiveGauge(chr)->unk2C == 0) {
                BtlChar_PlayVoice(chr, BtlChar_FrameMod(2) ? 0x20 : 0x21);
                BtlMember_GetActiveGauge(chr)->unk2C = 1;
            }
        }
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, BtlAct_GetIdleFollowUp(chr));
        }
        BtlMove_Step(chr, 6, 5, 6, 0.0f, BTL_KMH(50.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x41);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
}

/*
 * Action 0xE6: animation 0xEA / 0xEB, turning to the opponent under lock-on. Past 40% it runs the common
 * decide helpers (when nothing is queued) and takes the queued action; leaving raises flag 0x33.
 */
s32 BtlAct_ActionE6(Chr *chr, s32 phase) {
    f32 blend;
    s32 air;
    s32 anim;

    if (phase == 0) {
        blend = 0.15f;
        air = BtlAct_IsAirMotion(chr, 1);
        if (BtlAct_GetPrev(chr) == 0xD4) {
            blend = 0.0f;
        }
        anim = air ? 0xEB : 0xEA;
        BtlAnim_Play(chr, anim, blend);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x21);
        if (BtlChar_TestFlag(chr, 5)) {
            if (BtlAnim_GetFlags(anim) & 0x8000) {
                BtlMove_TurnYaw(chr, 3, 6.2831853f);
            } else {
                BtlMove_TurnYaw(chr, 2, 6.2831853f);
            }
        }
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, BtlAct_GetIdleFollowUp(chr));
        }
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(50.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x41);
        BtlChar_SetFlag(chr, 0x95);
    }
    if (phase == 2) {
        if (0.4f < BtlAnim_GetProgress(chr)) {
            if (!BtlAct_HasQueued(chr)) {
                BtlDecide_Main(chr, 0x81C68);
                BtlDecide_Attack(chr, 0xF);
                BtlDecide_Common(chr, 6);
            }
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
    if (phase == 3) {
        return BtlChar_SetFlagRet(chr, 0x33);
    }
}

/* Action 0xE7: animation 0xC8 / 0xC9, voice 0; then 0xDB on the ground, the down action without ground, else 0xD9. */
s32 BtlAct_ActionE7(Chr *chr, s32 phase) {
    f32 zero;

    if (phase == 0) {
        zero = 0.0f;
        BtlAnim_Play(chr, BtlAct_IsAirMotion(chr, 0) ? 0xC9 : 0xC8, zero);
        BtlChar_ClearFlag(chr, 0xE);
        BtlChar_GetPos(chr)->unk98 = zero;
        BtlChar_PlayVoice(chr, 0);
        BtlCharSnd_PlayCommon(chr, 0x32);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            if (BtlChar_TestFlag(chr, 0xF)) {
                BtlAct_Request(chr, 0xDB);
            } else if (BtlChar_TestFlag(chr, 0x11)) {
                BtlAct_Request(chr, BtlAct_GetDownAction(chr));
            } else {
                BtlAct_Request(chr, 0xD9);
            }
        }
        BtlMove_Step(chr, 6, 5, 6, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
    }
}

/* Actions 0xE8, 0xE9: animation 0xD0 / 0xD1 then action 0xB; 0xE8 starts with a 700 km/h move (yaw mode 7 / 8). */
s32 BtlAct_ActionE8E9(Chr *chr, s32 phase) {
    s32 air;
    f32 zero;

    if (phase == 0) {
        air = BtlAct_IsAirMotion(chr, 0);
        zero = 0.0f;
        BtlAnim_Play(chr, air ? 0xD1 : 0xD0, zero);
        switch (BtlAct_GetCurrent(chr)) {
            case 0xE8:
                BtlMove_TurnYaw(chr, air ? 8 : 7, 3.14159265f);
                BtlMove_SetDirection(chr, 2);
                BtlChar_GetPos(chr)->unk98 = BTL_KMH(700.0f);
                break;
            case 0xE9:
                BtlChar_GetPos(chr)->unk98 = zero;
                break;
        }
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_PlayVoice(chr, 0);
        BtlCharSnd_PlayBank8(chr, 9);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
    }
}

/* Action 0xEA: loops animation 0xD4 while flag 0xBE is raised, then action 0xB. */
s32 BtlAct_ActionEA(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xD4, 0.15f);
    }
    if (phase == 1) {
        BtlAnim_AdvanceLoop(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
    }
    if (phase == 2) {
        if (!BtlChar_TestFlag(chr, 0xBE)) {
            BtlAct_Request(chr, 0xB);
        }
    }
}

/* Action 0xEB: stands in animation 0 (flag 0xB) until BtlActB_TickMemberChange says go, then action 0xF6. */
s32 BtlAct_ActionEB(Chr *chr, s32 phase) {
    s32 *work = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0, 0.0f);
    }
    if (phase == 1) {
        BtlAnim_AdvanceLoop(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlChar_SetFlag(chr, 0xB);
        if (BtlActB_TickMemberChange(chr, work)) {
            BtlAct_Request(chr, 0xF6);
        }
    }
}

/* Action 0xD3: loops animation 0xB7 until the countdown chr +0xFE0 runs out, then the idle follow-up. */
s32 BtlAct_StunHandler(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xB7, 0.0f);
    }
    if (phase == 1) {
        BtlAnim_AdvanceLoop(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActB_SetReactionFlags(chr);
    }
    if (phase == 2) {
        if (chr->unkFE0 <= 0) {
            BtlAct_Request(chr, BtlAct_GetIdleFollowUp(chr));
        }
    }
}

/* Action 0xE0: the same in the air (0xD2 / 0xD3), sinking at 30 km/h without ground; ends in the down action. */
s32 BtlAct_AirStunHandler(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, BtlAct_IsAirMotion(chr, 0) ? 0xD3 : 0xD2, 0.0f);
    }
    if (phase == 1) {
        f32 accel;

        BtlAnim_AdvanceLoop(chr, 0);
        accel = BTL_KMH(100.0f);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, accel);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlMove_MoveVertical(chr, BTL_KMH(30.0f), accel);
        } else {
            BtlMove_Fall(chr);
        }
        BtlActB_SetReactionFlags(chr);
        BtlChar_SetFlag(chr, 0x2A);
    }
    if (phase == 2) {
        if (chr->unkFE0 <= 0) {
            BtlAct_Request(chr, BtlAct_GetDownAction(chr));
        }
    }
}

/*
 * Search (action 0x36): widens the search window, a half angle (chr +0xD58, up to pi) and a range (chr +0xD5C,
 * up to 1000000) by the fighter's two per-type values, 60% of them when bit 7 of the opponent's parameter
 * word is set. True on the frame the angle passes `limit`.
 */
s32 BtlAct_GrowSearchWindow(Chr *chr, f32 limit) {
    s32 slow;
    f32 dAngle;
    f32 dRange;
    f32 old;

    slow = (BtlOpp_GetParamWord0(chr) >> 7) & 1;
    dAngle = BtlParam_GetTypeValueA(chr);
    dRange = BtlParam_GetTypeValueB(chr);
    if (slow) {
        dAngle *= 0.6f;
        dRange *= 0.6f;
    }
    old = chr->unkD58;
    chr->unkD58 = old + dAngle;
    if (3.14159265f < chr->unkD58) {
        chr->unkD58 = 3.14159265f;
    }
    chr->unkD5C += dRange;
    if (1000000.0f < chr->unkD5C) {
        chr->unkD5C = 1000000.0f;
    }
    if (old < limit && limit <= chr->unkD58) {
        return 1;
    }
    return 0;
}

/*
 * True when the opponent is inside the search window: within the half angle of the facing and nearer than
 * range * (0.15 + 0.85 * (1 - angle / pi)). Never with flag 0xB2; flag 0xBA hides the opponent unless the
 * fighter has ability 0x6C and is in animation 0x17E.
 */
s32 BtlAct_IsOppInSearchWindow(Chr *chr) {
    s32 ret = 0;
    s32 flag;
    f32 dist;
    f32 yaw;

    if (BtlChar_TestFlag(chr, 0xB2)) {
        return 0;
    }
    BtlOpp_GetParamWord0(chr);
    flag = BtlChar_TestFlag(chr, 0xBA);
    dist = BtlOpp_GetDistanceXZ(chr);
    yaw = __builtin_fabsf(BtlOpp_GetYawFromFacing(chr));
    if (yaw <= chr->unkD58) {
        if (dist < chr->unkD5C * ((1.0f - yaw / 3.14159265f) * 0.85f + 0.15f)) {
            ret = flag == 0;
            if (BtlMember_HasAbility(chr, 0x6C)) {
                if (BtlAnim_GetId(chr) == 0x17E) {
                    ret = 1;
                }
            }
        }
    }
    return ret;
}

/*
 * Action 0x36: searching for an opponent that was lost from sight (lock-on flag 5 clear). Animation 0x17F
 * (types 2..4) into the loop 0x17E; the window grows every frame (voice 0x26 when the angle passes 90 degrees)
 * and when the opponent is inside it the lock-on is restored (voice 0x27, sound 0x2C) and action 0xB follows.
 * Leaving resets the window to 0.5 rad / 500.
 */
s32 BtlAct_SearchHandler(Chr *chr, s32 phase) {
    s32 grow;

#ifdef PORT
    /* A search may already be queued when the last enemy falls. Leave it
       without playing the found-target voice, sound or streak effect. */
    if ((phase == 1 || phase == 2) && TagTeam_LockedOff(chr->player)) {
        BtlAct_Request(chr, 0xB);
        return 0;
    }
#endif

    if (phase == 0) {
        if (BtlParam_IsType2to4(chr)) {
            BtlAnim_Play(chr, 0x17F, 0.15f);
        } else {
            BtlAnim_Play(chr, 0x17E, 0.15f);
        }
    }
    if (phase == 1) {
        grow = 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x17F:
                BtlAnim_AdvanceThen(chr, 0x17E, 0.15f, 0);
                if (0.5f < BtlAnim_GetProgress(chr)) {
                    grow = 1;
                }
                break;
            case 0x17E:
                BtlAnim_AdvanceLoop(chr, 0);
                grow = 1;
                break;
        }
        if (grow) {
            if (BtlAct_GrowSearchWindow(chr, 1.5707963f)) {
                BtlChar_PlayVoice(chr, 0x26);
            }
        }
        if (BtlAct_IsOppInSearchWindow(chr)) {
            BtlChar_SetHeldFlag(chr, 5);
            BtlMove_TurnYaw(chr, 2, 3.14159265f);
            BtlChar_PlayVoice(chr, 0x27);
            BtlCharSnd_PlayCommon(chr, 0x2C);
            if (!BtlChar_TestFlag(chr, 0xBA)) {
                BtlChar_SetFxBit(chr, 0x15);
            }
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xC9);
        BtlChar_SetFlag(chr, 0x1B);
        BtlChar_SetFlag(chr, 0x1C);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_ClearFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 5)) {
            BtlAct_Request(chr, 0xB);
        }
    }
    if (phase == 2) {
        BtlDecide_Main(chr, 0x481FEB);
        BtlDecide_Attack(chr, 3);
        BtlDecide_Common(chr, 6);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
    if (phase == 3) {
        chr->unkD58 = 0.5f;
        chr->unkD5C = 500.0f;
    }
}

/*
 * Action 0x37: ki charge. 0x34 start -> 0x35 loop -> 0x36 when the +0x1C gauge fills. The loop adds ki every
 * frame (a different rate without ground), pushes the fighter down along its yaw unless parameter word 0 is
 * 0x80, and with full ki and a blast stock feeds the +0x1C gauge; full ki also clears the stat penalty. A full
 * +0x1C gauge spends one blast stock and holds flag 6 (the powered-up mode). Decide: outside 0x36, releasing
 * the charge input (action input 19) goes to action 0xB. A fighter that cannot fly sinks while airborne.
 */
s32 BtlAct_ChargeHandler(Chr *chr, s32 phase) {
    Vec4 dir;
    f32 yaw;
    f32 accel;
    s32 amount;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x34, 0.15f);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
            case 0x34:
                if (BtlAnim_AdvanceThen(chr, 0x35, 0.15f, 1)) {
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetChargeStartSound(chr));
                    BtlChar_PlayVoice(chr, 0xD);
                }
                break;
            case 0x35:
                BtlAnim_AdvanceLoop(chr, 0);
                BtlCharSnd_PlayCommon(chr, BtlParam_GetChargeLoopSound(chr));
                BtlChar_SetFxBit(chr, 7);
                BtlChar_SetFlag(chr, 0xC);
                if (BtlParam_GetUnk0(chr) != 0x80) {
                    yaw = BtlChar_GetPos(chr)->rot.y;
                    dir.x = -Mathf_Sin(yaw);
                    dir.y = -1.0f;
                    dir.z = -Mathf_Cos(yaw);
                    BtlAct_PushDir(chr, &dir, 0.3f, 3.0f);
                }
                BtlChar_SetVibration(chr, 0.8f, 0.1f);
                if (BtlChar_TestFlag(chr, 0xF)) {
                    BtlChar_SetFxBit(chr, 0x36);
                }
                if (BtlChar_TestFlag(chr, 0x11)) {
                    amount = BtlParam_GetRateB(chr);
                } else {
                    amount = BtlParam_GetRateA(chr);
                }
                BtlMember_AddKi(chr, amount);
                if (BtlMember_IsKiFull(chr)) {
                    if (BtlMember_HasBlast(chr, 100000)) {
                        BtlMember_AddMaxPower(chr, BtlParam_GetStepA(chr));
                        BtlChar_SetFlag(chr, 0xBB);
                    }
                    BtlStat_ClearPenalty(chr);
                }
                if (BtlMember_IsMaxPowerFull(chr)) {
                    BtlAnim_Request(chr, 0x36, 0.15f);
                    BtlChar_SetFxBit(chr, 8);
                    BtlChar_SetFlag(chr, 0xD);
                    BtlMember_SubBlast(chr, 100000);
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetMaxPowerSound(chr));
                    BtlChar_PlayVoice(chr, 0x13);
                    BtlChar_SetHeldFlag(chr, 6);
                    ChrCam_AddShake(chr, 4.0f, 0.3f);
                    BtlChar_Vibrate(chr, 0.8f, 0.4f);
                }
                break;
            case 0x36:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, BtlAct_GetIdleFollowUp(chr));
                }
                if (!(BtlParam_GetFlags(chr) & 0x10)) {
                    if (BtlAnim_PassedRatio(chr, 0.25f)) {
                        BtlChar_SetFlag(chr, 0x4A);
                    }
                }
                BtlChar_SetFlag(chr, 0xBB);
                BtlChar_SetFlag(chr, 0x59);
                break;
        }
        accel = BTL_KMH(100.0f);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, accel);
        if (!BtlParam_CanFly(chr) && BtlChar_TestFlag(chr, 0xE) && !BtlChar_TestFlag(chr, 0x11)) {
            BtlMove_MoveVertical(chr, accel, accel);
        } else {
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFlag(chr, 0xC9);
        BtlChar_SetFlag(chr, 0x1C);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_ClearFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (BtlAnim_GetId(chr) != 0x36 && BtlAnim_GetNextId(chr) != 0x36) {
            if (BtlInput_TestAction(chr, 0x13, 0)) {
                BtlAct_Request(chr, 0xB);
            }
            BtlDecide_Main(chr, 0xD01C00);
            BtlDecide_Common(chr, 6);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
    if (phase == 3) {
        BtlChar_PlayVoiceSingle(chr, 0xD);
    }
}

/*
 * Action 0x38: guard (forced by flags 0x68 / 0x69 / 0x6A when a hit is guarded: recoil 0xF2 / 0xF4 / 0xF0).
 * Poses 0xEF (middle), 0xF1 (action input 36) and 0xF3 (action input 37), each with a recoil animation at +1;
 * 0xF5 after flag 0x67. A pose change needs three frames of the new input (work[2]); releasing the guard
 * (action input 34) ends the action.
 *
 * The recoil case tests `phase` (always 1 there) around its second BtlChar_SetFlag with the same call in both
 * arms: the compiler merges the arms only after its second scheduling pass, and the block boundary that exists
 * until then is what gives the original order of the two argument loads in front of BtlAnim_AdvanceThen
 * (`addiu a1,v0,-1 / jal / move a2,zero`; with a plain second call the two come out exchanged). The original
 * source had some such boundary behind SetFlag(0x95); what it looked like is not known.
 */
s32 BtlAct_GuardHandler(Chr *chr, s32 phase) {
    s32 *work = &chr->work[2];
    f32 blend;
    f32 accel;
    s32 anim;
    s32 want;

    if (phase == 0) {
        blend = 0.0f;
        if (BtlChar_TestFlag(chr, 0x68)) {
            anim = 0xF2;
        } else if (BtlChar_TestFlag(chr, 0x69)) {
            anim = 0xF4;
        } else if (BtlChar_TestFlag(chr, 0x6A)) {
            anim = 0xF0;
        } else {
            blend = 0.15f;
            if (BtlInput_TestAction(chr, 0x24, 1)) {
                anim = 0xF1;
            } else {
                anim = BtlInput_TestAction(chr, 0x25, 1) ? 0xF3 : 0xEF;
            }
        }
        BtlAnim_Play(chr, anim, blend);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
            case 0xEF:
            case 0xF1:
            case 0xF3:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
            case 0xF0:
            case 0xF2:
            case 0xF4:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) - 1, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x95);
                if (phase) {
                    BtlChar_SetFlag(chr, 0x41);
                } else {
                    BtlChar_SetFlag(chr, 0x41);
                }
                break;
            case 0xF5:
                BtlAnim_AdvanceThen(chr, 0xEF, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x95);
                BtlChar_SetFlag(chr, 0x41);
                if (BtlAnim_IsNew(chr)) {
                    BtlChar_PlayVoice(chr, 0x1B);
                }
                break;
        }
        accel = BTL_KMH(100.0f);
        BtlMove_Step(chr, 6, 5, 2, 0.0f, accel);
        if (!BtlParam_CanFly(chr) && BtlChar_TestFlag(chr, 0xE) && !BtlChar_TestFlag(chr, 0x11)) {
            BtlMove_MoveVertical(chr, accel, accel);
        } else {
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFlag(chr, 0xC9);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x1C);
        BtlChar_SetLookEnabled(chr, 1);
        if (BtlChar_TestFlag(chr, 0xF)) {
            if (BTL_KMH(200.0f) < Vec3_Length(&BtlChar_GetPos(chr)->move)) {
                BtlChar_SetFxBit(chr, 0x32);
            }
        }
        if (BtlChar_TestFlag(chr, 0x11) && !BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_ClearFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x66)) {
            switch (BtlAnim_GetId(chr)) {
                case 0xEF:
                case 0xF1:
                case 0xF3:
                    BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                    break;
                case 0xF0:
                case 0xF2:
                case 0xF4:
                    BtlAnim_Request(chr, BtlAnim_GetId(chr), 0.0f);
                    break;
                case 0xF5:
                    BtlAnim_Request(chr, 0xF0, 0.0f);
                    break;
            }
        } else if (BtlChar_TestFlag(chr, 0x67)) {
            BtlAnim_Request(chr, 0xF5, 0.0f);
        } else {
            switch (BtlAnim_GetId(chr)) {
                case 0xEF:
                case 0xF1:
                case 0xF3:
                    if (BtlInput_TestAction(chr, 0x22, 0)) {
                        BtlAct_Request(chr, 0xB);
                    }
                    want = 0xEF;
                    if (BtlInput_TestAction(chr, 0x24, 1)) {
                        want = 0xF1;
                    }
                    if (BtlInput_TestAction(chr, 0x25, 1)) {
                        want = 0xF3;
                    }
                    if (BtlAnim_GetId(chr) == want) {
                        *work = 0;
                    } else {
                        (*work)++;
                    }
                    if (*work >= 3) {
                        BtlAnim_Request(chr, want, 0.15f);
                        *work = 0;
                    }
                    break;
                case 0xF0:
                case 0xF2:
                case 0xF4:
                case 0xF5:
                    break;
            }
            BtlDecide_Main(chr, 0x1000000);
            BtlDecide_Attack(chr, 0x400);
            BtlDecide_Common(chr, 6);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/* Action 0x39 (forced by flag 0x7B): animation 0xEC, then the evasion attack BtlAct_GetEvasionAttack picks. */
s32 BtlAct_Action39(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xEC, 0.0f);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlDecide_QueueAttack(chr, BtlAct_GetEvasionAttack(chr));
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xC9);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0xF)) {
            if (BTL_KMH(200.0f) < Vec3_Length(&BtlChar_GetPos(chr)->move)) {
                BtlChar_SetFxBit(chr, 0x32);
            }
        }
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
}

/* Action 0x3A: animation 0xEE (voice 0x1E under lock-on with flag 0x13), then action 0xB or the queue; leaving raises flag 0x33. */
s32 BtlAct_Action3A(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xEE, 0.15f);
        if (BtlChar_TestFlag(chr, 5) && BtlChar_TestFlag(chr, 0x13)) {
            BtlChar_PlayVoice(chr, 0x1E);
        }
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        BtlMove_Step(chr, 3, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 3) {
        return BtlChar_SetFlagRet(chr, 0x33);
    }
}

/*
 * Action 0x3B (forced by flag 0x6B): animation 0xF9, flags 0x50 / 0x51. A counter of 4 (work[2]) is refilled
 * by action input 43 and zeroed when the guard is released; each flag 0x31 takes one off and alternates
 * 0xF9 / 0xFA, and at 0 the action ends.
 */
s32 BtlAct_Action3B(Chr *chr, s32 phase) {
    s32 *work = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0xF9, 0.15f);
        *work = 4;
    }
    if (phase == 1) {
        BtlAnim_Advance(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1C);
        BtlChar_SetFlag(chr, 0x50);
        BtlChar_SetFlag(chr, 0x51);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_ClearFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (BtlInput_TestAction(chr, 0x2B, 1)) {
            *work = 4;
        }
        if (BtlInput_TestAction(chr, 0x22, 0)) {
            *work = 0;
        }
        if (BtlChar_TestFlag(chr, 0x31)) {
            if (--*work <= 0) {
                BtlAct_Request(chr, 0xB);
            } else {
                BtlAnim_Request(chr, BtlAnim_GetId(chr) == 0xF9 ? 0xFA : 0xF9, 0.0f);
            }
        }
        BtlDecide_Common(chr, 6);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/* Action 0x3C: animation 0xFD with voice 5 and flag 0x52, then action 0xB. */
s32 BtlAct_Action3C(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xFD, 0.15f);
        BtlChar_PlayVoice(chr, 5);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1C);
        BtlChar_SetFlag(chr, 0x52);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_ClearFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        BtlDecide_Main(chr, 0x1C6A);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/* Action 0x3D (forced by flag 0x6B): 0xF9 -> loop 0xFA with flag 0x54 until the guard is released. */
s32 BtlAct_Action3D(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xF9, 0.15f);
        BtlCharSnd_PlayCommon(chr, 0x1F);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
            case 0xF9:
                BtlAnim_AdvanceThen(chr, 0xFA, 0.0f, 0);
                break;
            case 0xFA:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1C);
        BtlChar_SetFlag(chr, 0x54);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_ClearFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (BtlInput_TestAction(chr, 0x22, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlDecide_Common(chr, 6);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/*
 * Action 0x3E: an airborne step (animation 0xFB, voice 5) moving at the fighter's speed 4 / 5 around the
 * opponent, then action 0xF. Action input 44 past 40% queues another; 70% takes the queue; action input 64
 * goes to 0x58.
 */
s32 BtlAct_Action3E(Chr *chr, s32 phase) {
    f32 speed;
    f32 c;

    if (phase == 0) {
        BtlAnim_Play(chr, 0xFB, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_PlayVoice(chr, 5);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xF);
        }
        speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) + 4);
        BtlMove_TurnYaw(chr, 0, BtlMoveParam_GetTurnRate(chr, 0));
        BtlMove_TurnPitch(chr, 2, BTL_DEG(9.0f));
        BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
        BtlMove_SetDirection(chr, 3);
        BtlMove_Advance(chr, speed, 10000.0f);
        BtlMove_ApplyGravity(chr);
        if (BtlChar_TestFlag(chr, 5) && !BtlInput_IsHeld(chr, 0xF0)) {
            BtlChar_SetFlag(chr, 0x1A);
        }
        c = Mathf_Cos(BtlOpp_GetYawFromFacing(chr));
        if (0.0f < c) {
            BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->speed * c);
        }
        BtlChar_SetFlag(chr, 0xCA);
        BtlMove_RequestOrbit(chr, 50.0f, 100.0f);
        BtlChar_SetFlag(chr, 0x50);
        BtlChar_SetFlag(chr, 0x51);
        BtlChar_SetFxBit(chr, 0xB);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (0.4f < BtlAnim_GetProgress(chr) && BtlInput_TestAction(chr, 0x2C, 1)) {
            BtlAct_SetQueue(chr, 0, 0x3E);
        }
        if (BtlAnim_PassedRatio(chr, 0.7f)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlInput_TestAction(chr, 0x40, 1)) {
            BtlAct_Request(chr, 0x58);
        }
        if (BtlDecide_Attack(chr, 0xC000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlDecide_Common(chr, 6)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/* Action 0x3F: the ground version (animation 0xFC, then 0x11; input 64 goes to 0x59; landing ends it). */
s32 BtlAct_Action3F(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xFC, 0.15f);
        BtlChar_ClearFlag(chr, 0xE);
        BtlChar_PlayVoice(chr, 5);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0x11);
        }
        BtlMove_Step(chr, 2, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x50);
        BtlChar_SetFlag(chr, 0x51);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (0.4f < BtlAnim_GetProgress(chr) && BtlInput_TestAction(chr, 0x2C, 1)) {
            BtlAct_SetQueue(chr, 0, 0x3F);
        }
        if (BtlAnim_PassedRatio(chr, 0.7f)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlInput_TestAction(chr, 0x40, 1)) {
            BtlAct_Request(chr, 0x59);
        }
        if (BtlDecide_Attack(chr, 0x60000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        if (BtlChar_TestFlag(chr, 0xF)) {
            BtlAct_Request(chr, 0xB);
        }
    }
}

/* Action 0x40: animation 0xDE with flag 0x42, then action 0xB; past 60% a queued action takes over. */
s32 BtlAct_Action40(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0xDE, 0.15f);
        BtlCharSnd_PlayCommon(chr, 0x1F);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlMove_Step(chr, 6, 5, 3, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x42);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        if (0.6f < BtlAnim_GetProgress(chr) && BtlAct_HasQueued(chr)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/*
 * Action 0x41 (forced by flag 0x7D): plays 0x193, 0x194, 0x195 in turn (chr +0xD8C, reset unless the previous
 * action was 0x41) with camera cut 0x11 / 0x12 by the sign of the fighter camera's `side`, then idles (0 / 0x26)
 * until flag 0xB5 or the opponent's seen action is no longer 0x45, and requests 0x6A.
 */
s32 BtlAct_Action41(Chr *chr, s32 phase) {
    s32 idle;

    if (phase == 0) {
        if (BtlAct_GetPrev(chr) != 0x41) {
            chr->unkD8C = 0;
        }
        BtlAnim_Play(chr, chr->unkD8C + 0x193, 0.15f);
        chr->unkD8C++;
        chr->unkD8C %= 3;
        BtlCharSnd_PlayCommon(chr, 0x1F);
        ChrCam_RequestCut(chr, 1, chr->camSide < 0.0f ? 0x11 : 0x12);
    }
    if (phase == 1) {
        idle = BtlChar_TestFlag(chr, 0xE) ? 0x26 : 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x193:
            case 0x194:
            case 0x195:
                BtlAnim_AdvanceThen(chr, idle, 0.15f, 0);
                break;
            case 0:
            case 0x26:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
        }
        BtlMove_Step(chr, 2, 5, 3, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x4E);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0:
            case 0x26:
                if (BtlChar_TestFlag(chr, 0xB5) || BtlOpp_GetSeenAction(chr) != 0x45) {
                    BtlAct_Request(chr, 0x6A);
                }
                break;
            case 0x193:
            case 0x194:
            case 0x195:
                break;
        }
    }
}

/* Action 0x42: animation 0x192 over 25 frames; costs 5000 health (flags 0x42B), holds flag 0x4B for the first half. */
s32 BtlAct_Action42(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0x192, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        BtlMember_Damage(chr, 5000, 0x42B);
        BtlChar_SetHeldFlag(chr, 0x4B);
        BtlCharSnd_PlayCommon(chr, 0x24);
        BtlChar_PlayVoice(chr, 6);
        BtlChar_SetFxBit(chr, 0x2B);
        BtlChar_Vibrate(chr, 0.8f, 0.4f);
    }
    if (phase == 1) {
        BtlAnim_SetDuration(chr, 25.0f / 30.0f);
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        if (BtlAnim_PassedRatio(chr, 0.5f)) {
            BtlChar_ClearFlag(chr, 0x4B);
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xC9);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
    if (phase == 3) {
        return BtlChar_ClearFlagRet(chr, 0x4B);
    }
}

/* Action 0x43: animation 0x185 with voice 0x23; counts in the active member's +0xA0. */
s32 BtlAct_Action43(Chr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0x185, 0.15f);
        BtlChar_PlayVoice(chr, 0x23);
        BtlMember_GetActive(chr)->unkA0++;
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
}

/* Starts sub-animation `anim` and returns the model node of its event 4 (the hand the blast leaves from). */
s32 BtlAct_PlaySubAnim(Chr *chr, s32 anim) {
    void *obj = BtlChar_GetObj(chr);

    BtlAnim_PlaySub(chr, anim);
    return BtlObjAnim_MaskToNode(BtlObjAnim_QueryEvent(obj, 4, 1, 6));
}

/*
 * Action 0xAE: ki blast. Alternates animations 0x71 / 0x72 (chr +0xDE8; always 0x71 unless the member word or
 * parameter flag 0x100 allows the second hand), spends the ki cost, voice 0xC, counts the shots (chr +0xDE0)
 * and restarts a 30 frame timer (chr +0xDE4). The aim direction (chr +0xDD0) is the facing, or toward the
 * opponent from node 0x11 within 36 degrees under lock-on.
 */
s32 BtlAct_KiBlastHandler(Chr *chr, s32 phase) {
    Vec4 pos;
    s32 idle;
    f32 zero;

    if (phase == 0) {
        if (BtlAct_GetPrev(chr) != 0xAE) {
            chr->unkDE8 = 0;
        }
        if (chr->unkDE8 == 0) {
            BtlAnim_Play(chr, 0x71, 0.1f);
            chr->unkDE8 = 1;
        } else {
            BtlAnim_Play(chr, 0x72, 0.1f);
            if (!BtlChar_TestMemberUnk70(chr) && !(BtlParam_GetFlags(chr) & 0x100)) {
                chr->unkDE8 = 0;
            }
        }
        BtlMember_SpendKi(chr, BtlKiBlast_GetKiCost(chr), 0);
        BtlChar_PlayVoice(chr, 0xC);
        chr->unkDE0 += BtlKiBlast_GetHitsOf(chr, 0);
        chr->unkDE4 = 0x1E;
    }
    if (phase == 1) {
        idle = BtlChar_TestFlag(chr, 0xE) ? 0x26 : 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x71:
            case 0x72:
                BtlAnim_AdvanceThen(chr, idle, 0.2f, 0);
                BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 2, BtlAnim_GetId(chr) + 4, 1);
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
        zero = 0.0f;
        BtlMove_Step(chr, 2, 5, 7, zero, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 5)) {
            BtlCharApi_GetNodePos(chr->objId, 0x11, &pos);
            BtlCharApi_CalcAimDir(chr->objId, 0x11, &pos, &chr->unkDD0, -BTL_DEG(36.0f), BTL_DEG(36.0f));
        } else {
            chr->unkDD0.x = Mathf_Sin(BtlChar_GetPos(chr)->facing);
            chr->unkDD0.y = zero;
            chr->unkDD0.z = Mathf_Cos(BtlChar_GetPos(chr)->facing);
            chr->unkDD0.w = zero;
        }
    }
    if (phase == 2) {
        BtlDecide_Attack(chr, 0xC);
        switch (BtlAnim_GetId(chr)) {
            case 0x71:
            case 0x72:
                if (BtlChar_TestFlag(chr, 0x31)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
            case 0:
            case 0x26:
                BtlDecide_Main(chr, 0x81CEA);
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
                break;
        }
    }
}

/* Action 0xAF: charged ki blast: 0x7B wind-up -> 0x7C hold (until action input 91) -> 0x7D release (ki cost, voice 0x25). */
s32 BtlAct_ChargedKiBlastHandler(Chr *chr, s32 phase) {
    Vec4 pos;
    f32 frame;
    f32 zero;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x7B, 0.15f);
        chr->unkDF0 = BtlAct_PlaySubAnim(chr, 0x7D);
        BtlChar_SetFxBit(chr, 0x17);
        BtlCharSnd_PlayCommon(chr, 0x2E);
        chr->unkDE0++;
        chr->unkDE4 = 0x1E;
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
            case 0x7B:
                BtlAnim_AdvanceThen(chr, 0x7C, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x8E);
                break;
            case 0x7C:
                BtlAnim_AdvanceThen(chr, 0x7D, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x8E);
                chr->unkDEC = BtlAnim_GetProgress(chr);
                break;
            case 0x7D:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                frame = BtlAnim_GetFrame(chr);
                if (frame < (f32)BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 4, 0, 0)) {
                    BtlChar_SetFlag(chr, 0x8E);
                }
                if (BtlAnim_IsNew(chr)) {
                    BtlMember_SpendKi(chr, BtlKiBlast_GetKiCost(chr), 0);
                    BtlChar_PlayVoice(chr, 0x25);
                }
                break;
        }
        zero = 0.0f;
        BtlMove_Step(chr, 2, 5, 7, zero, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 3, BtlAnim_GetId(chr) + 6, 1);
        if (BtlChar_TestFlag(chr, 5)) {
            BtlCharApi_GetNodePos(chr->objId, 0x11, &pos);
            BtlCharApi_CalcAimDir(chr->objId, chr->unkDF0, &pos, &chr->unkDD0, -BTL_DEG(36.0f), BTL_DEG(36.0f));
        } else {
            chr->unkDD0.x = Mathf_Sin(BtlChar_GetPos(chr)->facing);
            chr->unkDD0.y = zero;
            chr->unkDD0.z = Mathf_Cos(BtlChar_GetPos(chr)->facing);
            chr->unkDD0.w = zero;
        }
    }
    if (phase == 2) {
        if (BtlAnim_GetId(chr) == 0x7C) {
            if (BtlInput_TestAction(chr, 0x5B, 1)) {
                BtlAnim_Request(chr, 0x7D, 0.0f);
            }
        }
    }
}

/*
 * Action 0xB0: ki blast while dashing. The animation is picked from the facing relative to the camera yaw
 * (0x77 forward, 0x78 / 0x79 sideways, firing 90 degrees off the direction of travel); forward under lock-on
 * the dash steers at the opponent. Then action 0xF.
 */
s32 BtlAct_DashKiBlastHandler(Chr *chr, s32 phase) {
    s32 anim;
    Vec4 *dir;
    f32 speed;
    f32 yawAccel;
    f32 max;
    f32 tmp;
    f32 tmp2;

    if (phase == 0) {
        anim = 0;
        switch (BtlUtil_AngleToSector(BtlAct_GetFacingRelCam(chr))) {
            case 0:
                anim = 0x77;
                if (BtlChar_TestFlag(chr, 5)) {
                    chr->work[0] |= 1;
                    BtlChar_SetHeldFlag(chr, 0xE);
                }
                break;
            case 2:
                anim = 0x78;
                break;
            case 1:
                anim = 0x77;
                break;
            case 3:
                anim = 0x79;
                break;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        BtlMember_SpendKi(chr, BtlKiBlast_GetKiCost(chr), 0);
        BtlChar_PlayVoice(chr, 0xC);
        chr->unkDE0 += BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 4, 0, 3);
        chr->unkDE4 = 0x1E;
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xF);
        }
        dir = &chr->unkDD0;
        Vec4_Copy(dir, &BtlChar_GetPos(chr)->vel);
        switch (BtlAnim_GetId(chr)) {
            case 0x77:
                break;
            case 0x78:
                tmp = -dir->x;
                tmp2 = chr->unkDD0.z;
                chr->unkDD0.z = tmp;
                dir->x = tmp2;
                break;
            case 0x79:
                tmp = -chr->unkDD0.z;
                tmp2 = dir->x;
                dir->x = tmp;
                chr->unkDD0.z = tmp2;
                break;
        }
        if ((chr->work[0] & 1) && BtlChar_TestFlag(chr, 5)) {
            yawAccel = BTL_DEG(9.0f);
            speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) + 4);
            max = BTL_DEG(54.0f);
            if (chr->actionFrame == 0) {
                yawAccel = 3.14159265f;
            }
            BtlMove_SteerAtOpponent(chr, speed, yawAccel, 0.0f, BtlUtil_MinF(BtlOpp_GetGapXZ(chr) * 0.01f, 1.0f) * max, 0.0f, max);
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.5f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, BTL_KMH(200.0f));
        } else {
            speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) + 4);
            BtlMove_TurnYaw(chr, 0, BtlMoveParam_GetTurnRate(chr, 0));
            BtlMove_TurnPitch(chr, 5, BTL_DEG(9.0f));
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, 10000.0f);
        }
        BtlMove_ApplyGravity(chr);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        BtlChar_SetFlag(chr, 0xCA);
        BtlChar_SetFxBit(chr, 0xB);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 5) && (chr->work[0] & 1)) {
            BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->speed);
        }
    }
    if (phase == 2) {
        if (0.5f < BtlAnim_GetProgress(chr)) {
            BtlDecide_Attack(chr, 0xC000);
            if (BtlInput_TestAction(chr, 0x2C, 1)) {
                BtlAct_SetQueue(chr, 0, 0x3E);
            }
            if (BtlChar_TestFlag(chr, 0x31)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            }
        }
    }
}

/* Action 0xB1: charged ki blast while dashing (0x84 / 0x87 / 0x8A wind-up, +1 hold, +2 release). */
s32 BtlAct_DashChargedKiBlastHandler(Chr *chr, s32 phase) {
    s32 anim;
    Vec4 *dir;
    f32 speed;
    f32 yawAccel;
    f32 max;
    f32 tmp;
    f32 tmp2;
    f32 frame;

    if (phase == 0) {
        anim = 0;
        switch (BtlUtil_AngleToSector(BtlAct_GetFacingRelCam(chr))) {
            case 0:
                anim = 0x84;
                if (BtlChar_TestFlag(chr, 5)) {
                    chr->work[0] |= 1;
                    BtlChar_SetHeldFlag(chr, 0xE);
                }
                break;
            case 2:
                anim = 0x87;
                break;
            case 1:
                anim = 0x84;
                break;
            case 3:
                anim = 0x8A;
                break;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        chr->unkDF0 = BtlAct_PlaySubAnim(chr, anim + 2);
        BtlChar_SetFxBit(chr, 0x17);
        BtlCharSnd_PlayCommon(chr, 0x2E);
        chr->unkDE0++;
        chr->unkDE4 = 0x1E;
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
            case 0x84:
            case 0x87:
            case 0x8A:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x8E);
                break;
            case 0x85:
            case 0x88:
            case 0x8B:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x8E);
                chr->unkDEC = BtlAnim_GetProgress(chr);
                break;
            case 0x86:
            case 0x89:
            case 0x8C:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xF);
                }
                if (BtlAnim_IsNew(chr)) {
                    BtlChar_PlayVoice(chr, 0x25);
                    BtlMember_SpendKi(chr, BtlKiBlast_GetKiCost(chr), 0);
                }
                frame = BtlAnim_GetFrame(chr);
                if (frame < (f32)BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 4, 0, 0)) {
                    BtlChar_SetFlag(chr, 0x8E);
                }
                break;
        }
        dir = &chr->unkDD0;
        Vec4_Copy(dir, &BtlChar_GetPos(chr)->vel);
        switch (BtlAnim_GetId(chr)) {
            case 0x89:
                tmp = -dir->x;
                tmp2 = chr->unkDD0.z;
                chr->unkDD0.z = tmp;
                dir->x = tmp2;
                break;
            case 0x8C:
                tmp = -chr->unkDD0.z;
                tmp2 = dir->x;
                dir->x = tmp;
                chr->unkDD0.z = tmp2;
                break;
        }
        if ((chr->work[0] & 1) && BtlChar_TestFlag(chr, 5)) {
            yawAccel = BTL_DEG(9.0f);
            speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) + 4);
            max = BTL_DEG(54.0f);
            if (chr->actionFrame == 0) {
                yawAccel = 3.14159265f;
            }
            BtlMove_SteerAtOpponent(chr, speed, yawAccel, 0.0f, BtlUtil_MinF(BtlOpp_GetGapXZ(chr) * 0.01f, 1.0f) * max, 0.0f, max);
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.5f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, BTL_KMH(200.0f));
        } else {
            speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) + 4);
            BtlMove_TurnYaw(chr, 0, BtlMoveParam_GetTurnRate(chr, 0));
            BtlMove_TurnPitch(chr, 5, BTL_DEG(9.0f));
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, 10000.0f);
        }
        BtlMove_ApplyGravity(chr);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        BtlChar_SetFlag(chr, 0xCA);
        BtlChar_SetFxBit(chr, 0xB);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_TestFlag(chr, 5) && (chr->work[0] & 1)) {
            BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->speed);
        }
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
            case 0x84:
            case 0x87:
            case 0x8A:
                break;
            case 0x85:
            case 0x88:
            case 0x8B:
                if (BtlInput_TestAction(chr, 0x5B, 1)) {
                    BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                }
                break;
            case 0x86:
            case 0x89:
            case 0x8C:
                break;
        }
    }
}
