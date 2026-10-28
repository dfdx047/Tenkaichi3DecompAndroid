#include "common.h"
#include "battle/btl_act_d.h"

/*
 * Fighter action handlers: 0x1EE058..0x1F1930. A slice of the original handler source file (its float pool
 * runs on from the previous slice and into the next).
 *
 *   0xB2, 0xB3   ki blast fired at an aim direction, plain and chargeable
 *   0x95         ki volley attack (motions 0x71 / 0x72, up to ten shots)
 *   0x0B         the neutral state
 *   0x0C         the idle motion played once
 *   0x0D, 0x0E   moving with the stick: free, and close to the opponent (flag 0x13)
 *   0x0F         dash
 *   0x10..0x13   jump: take-off (0x10 plain, 0x12 from a dash) and airborne part (0x11, 0x13)
 *   0x14..0x16   ascend, descend, and the hop of a fighter that cannot fly
 *   0x17, 0x18   fast ascend / descend (ki drain)
 *   0x19         homing dash at the opponent (ki drain)
 *
 * Handler protocol (btl_char_action.h): s32 handler(chr, phase), phase 0 enter, 1 run, 2 decide, 3 leave; the
 * value is ignored and the handlers fall off their end.
 *
 * Flags used here (meanings inferred from these handlers): 0xE in flight mode (held), 0xF standing on the
 * ground, 0x11 in water, 0x12 another medium (dash sound 0x23), 0x13 close to the opponent, 5 locked on,
 * 0x16 / 0x17 force flight (0x17 only on stages 4 and 27), 0x15 / 0x16 stop a fast ascend / descend.
 */

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)

#define PHASE_ENTER  0
#define PHASE_RUN    1
#define PHASE_DECIDE 2
#define PHASE_LEAVE  3

extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
extern f32 Vec3_Length(Vec4 *v);
extern s32 Battle_GetMode(void);

extern void *BtlChar_GetObj(BtlActDChr *chr);
extern BtlActDPose *BtlChar_GetPos(BtlActDChr *chr);
extern s32 BtlChar_TestMemberUnk70(BtlActDChr *chr);
extern s32 BtlChar_IsStage4Or27(void);
extern void BtlChar_PlayVoice(BtlActDChr *chr, s32 kind);
extern void BtlChar_SetVibration(BtlActDChr *chr, f32 power, f32 seconds);
extern void BtlChar_Vibrate(BtlActDChr *chr, f32 power, f32 seconds);
extern s32 BtlChar_TestFlag(BtlActDChr *chr, s32 n);
extern s32 BtlChar_IsFlagRaised(BtlActDChr *chr, s32 n);
extern void BtlChar_SetFlag(BtlActDChr *chr, s32 n);
extern void BtlChar_SetHeldFlag(BtlActDChr *chr, s32 n);
extern void BtlChar_ClearFlag(BtlActDChr *chr, s32 n);
extern void BtlChar_SetFxBit(BtlActDChr *chr, s32 n);
extern void BtlChar_SetLookEnabled(BtlActDChr *chr, s32 enabled);
extern void BtlCharSnd_PlayCommon(BtlActDChr *chr, s32 id);
extern void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
extern void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 arg3, f32 arg4);
extern void ChrCam_RequestCut(BtlActDChr *chr, s32 table, s32 index);
extern void ChrCam_EndCut(BtlActDChr *chr);

extern s32 BtlInput_TestAction(BtlActDChr *chr, s32 id, s32 want);
extern s32 BtlInput_IsHeld(BtlActDChr *chr, u32 mask);
extern f32 BtlInput_GetStickY(BtlActDChr *chr);
extern f32 BtlInput_GetStickLength(BtlActDChr *chr);

extern BtlActDGauge *BtlMember_GetActiveGauge(BtlActDChr *chr);
extern void *BtlMember_GetActive(BtlActDChr *chr);
extern s32 BtlMember_SpendKi(BtlActDChr *chr, s32 amount, s32 force);

extern void BtlAct_Request(BtlActDChr *chr, s32 id);
extern s32 BtlAct_HasQueued(BtlActDChr *chr);
extern s32 BtlAct_GetCurrent(BtlActDChr *chr);
extern s32 BtlAct_GetRequested(BtlActDChr *chr);
extern s32 BtlAct_GetPrev(BtlActDChr *chr);
extern s32 BtlAct_GetQueued(BtlActDChr *chr);
extern s32 BtlAct_IsAttackId(s32 id);
extern void BtlAct_LatchAttack(BtlActDChr *chr);
extern f32 BtlAct_GetGroundY(BtlActDChr *chr);
extern f32 BtlAct_GetHeight(BtlActDChr *chr);
extern f32 BtlAct_GetFramesToGround(BtlActDChr *chr);
extern f32 BtlAct_GetRotYRelCam(BtlActDChr *chr);
extern f32 BtlAct_GetVelDirRelCam(BtlActDChr *chr);
extern f32 BtlAct_ScaleSpeedByApproach(BtlActDChr *chr, f32 speed, f32 range);
extern void BtlAct_SetPitchMotion(BtlActDChr *chr, s32 motionUp, s32 motionDown, s32 recalc);

extern void BtlAnim_Play(BtlActDChr *chr, s32 anim, f32 blend);
extern void BtlAnim_Request(BtlActDChr *chr, s32 anim, f32 blend);
extern void BtlAnim_RequestKeep(BtlActDChr *chr, s32 anim, f32 blend);
extern void BtlAnim_PlaySub(BtlActDChr *chr, s32 anim);
extern void BtlAnim_SetUnkC8C(BtlActDChr *chr, f32 v);
extern void BtlAnim_SetDuration(BtlActDChr *chr, f32 seconds);
extern s32 BtlAnim_GetId(BtlActDChr *chr);
extern f32 BtlAnim_GetFrame(BtlActDChr *chr);
extern f32 BtlAnim_GetProgress(BtlActDChr *chr);
extern s32 BtlAnim_Advance(BtlActDChr *chr, s32 flags);
extern s32 BtlAnim_AdvanceThen(BtlActDChr *chr, s32 next, f32 blend, s32 flags);
extern void BtlAnim_AdvanceLoop(BtlActDChr *chr, s32 flags);
extern s32 BtlAnim_PassedRatio(BtlActDChr *chr, f32 ratio);
extern s32 BtlAnim_IsNew(BtlActDChr *chr);

extern void BtlMove_TurnYaw(BtlActDChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_TurnPitch(BtlActDChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_SteerAtOpponent(BtlActDChr *chr, f32 closeSpeed, f32 yawAccel, f32 pitchAccel, f32 yawMax, f32 pitchMax, f32 maxStep);
extern void BtlMove_TurnModelYaw(BtlActDChr *chr, f32 maxStep, f32 rate);
extern void BtlMove_SetDirection(BtlActDChr *chr, s32 mode);
extern void BtlMove_Advance(BtlActDChr *chr, f32 speed, f32 accel);
extern void BtlMove_Step(BtlActDChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel);
extern void BtlMove_MoveVertical(BtlActDChr *chr, f32 speed, f32 accel);
extern void BtlMove_BrakeVertical(BtlActDChr *chr);
extern void BtlMove_ApplyGravity(BtlActDChr *chr);
extern void BtlMove_SetLeanX(BtlActDChr *chr, f32 v);
extern s32 BtlMove_IsBlockedByOpponent(BtlActDChr *chr);
extern void BtlMove_RequestOrbit(BtlActDChr *chr, f32 near, f32 far);
extern f32 BtlMove_CalcJumpSpeed(f32 height);

extern f32 BtlOpp_GetGapXZ(BtlActDChr *chr);
extern f32 BtlOpp_GetYawFromFacing(BtlActDChr *chr);
extern s32 BtlOpp_GetSeenAction(BtlActDChr *chr);
extern f32 BtlUtil_ApproachF(f32 cur, f32 target, f32 step);
extern f32 BtlUtil_MinF(f32 a, f32 b);

extern s32 BtlAct_PlaySubAnim(BtlActDChr *chr, s32 anim);
extern s32 BtlParam_GetFlags(BtlActDChr *chr);
extern s32 BtlParam_GetUnk2(BtlActDChr *chr);
extern s32 BtlParam_CanFly(BtlActDChr *chr);   /* can fly: parameter flag 0x1000 clear, or ability 0x36 */
extern s32 BtlKiBlast_GetKiCost(BtlActDChr *chr);   /* ki cost of the current attack */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_CalcAimDir(s32 objId, s32 node, Vec4 *pos, Vec4 *out, f32 a, f32 b);
extern s32 BtlObjAnim_QueryEvent(void *obj, u64 mask, s32 layer, s32 what); /* motion event query: what 0 = first frame, 3 = count */

/*
 * Action 0xB2: a ki blast (motion 0x7A) paid on entry, aimed 18..45 degrees below the horizon at the opponent when
 * locked on, else 45 degrees down along the facing. Ends in action 0xB in flight, else 0x11 (fall).
 */
s32 BtlAct_KiBlastB2(BtlActDChr *chr, s32 phase) {
    Vec4 pos;
    f32 r;

    if (phase == PHASE_ENTER) {
        BtlAnim_Play(chr, 0x7A, 0.15f);
        BtlMember_SpendKi(chr, BtlKiBlast_GetKiCost(chr), 0);
        BtlChar_PlayVoice(chr, 0xC);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        chr->unkDE0 += BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 4, 0, 3);
        chr->unkDE4 = 0x1E;
    }
    if (phase == PHASE_RUN) {
        if (BtlAnim_Advance(chr, 0)) {
            if (BtlChar_TestFlag(chr, 0xE)) {
                BtlAct_Request(chr, 0xB);
            } else {
                BtlAct_Request(chr, 0x11);
            }
        }
        BtlMove_Step(chr, 2, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 5)) {
            BtlCharApi_GetNodePos(chr->objId, 0x11, &pos);
            BtlCharApi_CalcAimDir(chr->objId, 0x11, &pos, &chr->aimDir, BTL_DEG(18.0f), BTL_DEG(45.0f));
        } else {
            r = 0.7071f;
            chr->aimDir.x = Mathf_Sin(BtlChar_GetPos(chr)->yaw) * r;
            chr->aimDir.y = r;
            chr->aimDir.z = Mathf_Cos(BtlChar_GetPos(chr)->yaw) * r;
            chr->aimDir.w = 0.0f;
        }
    }
}

/*
 * Action 0xB3: the chargeable version: motions 0x8D wind-up, 0x8E charge (left when input 0x5B, BLAST released, is
 * seen; its progress is kept in +0xDEC), 0x8F release (paid when it starts). Flag 0x8E is raised while charging and
 * until the release motion reaches its first event of kind 4.
 */
s32 BtlAct_KiBlastChargeB3(BtlActDChr *chr, s32 phase) {
    Vec4 pos;
    f32 r;
    f32 frame;

    if (phase == PHASE_ENTER) {
        BtlAnim_Play(chr, 0x8D, 0.15f);
        chr->unkDF0 = BtlAct_PlaySubAnim(chr, 0x8F);
        BtlChar_SetFxBit(chr, 0x17);
        BtlCharSnd_PlayCommon(chr, 0x2E);
        chr->unkDE0++;
        chr->unkDE4 = 0x1E;
    }
    if (phase == PHASE_RUN) {
        switch (BtlAnim_GetId(chr)) {
            case 0x8D:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x8E);
                break;
            case 0x8E:
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
                BtlChar_SetFlag(chr, 0x8E);
                chr->unkDEC = BtlAnim_GetProgress(chr);
                break;
            case 0x8F:
                if (BtlAnim_Advance(chr, 0)) {
                    if (BtlChar_TestFlag(chr, 0xE)) {
                        BtlAct_Request(chr, 0xB);
                    } else {
                        BtlAct_Request(chr, 0x11);
                    }
                }
                if (BtlAnim_IsNew(chr)) {
                    BtlChar_PlayVoice(chr, 0x25);
                    BtlMember_SpendKi(chr, BtlKiBlast_GetKiCost(chr), 0);
                }
                frame = BtlAnim_GetFrame(chr);
                if (frame < BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 4, 0, 0)) {
                    BtlChar_SetFlag(chr, 0x8E);
                }
                break;
        }
        BtlMove_Step(chr, 2, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlChar_SetFlag(chr, 0xD5);
        if (BtlChar_TestFlag(chr, 5)) {
            BtlCharApi_GetNodePos(chr->objId, 0x11, &pos);
            BtlCharApi_CalcAimDir(chr->objId, 0x11, &pos, &chr->aimDir, BTL_DEG(18.0f), BTL_DEG(45.0f));
        } else {
            r = 0.7071f;
            chr->aimDir.x = Mathf_Sin(BtlChar_GetPos(chr)->yaw) * r;
            chr->aimDir.y = r;
            chr->aimDir.z = Mathf_Cos(BtlChar_GetPos(chr)->yaw) * r;
            chr->aimDir.w = 0.0f;
        }
    }
    if (phase == PHASE_DECIDE) {
        if (BtlAnim_GetId(chr) == 0x8E) {
            if (BtlInput_TestAction(chr, 0x5B, 1)) {
                BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
            }
        }
    }
}

/*
 * Action 0x95: attack record 0x25. Motions 0x71 / 0x72 of 0.1 s, each paid when it starts, alternating (always 0x72
 * with parameter flag 0x100 or the member's +0x70 word set), ten in all; the pitch variants are +2 / +4.
 */
s32 BtlAct_KiVolley95(BtlActDChr *chr, s32 phase) {
    Vec4 pos;
    s32 *count = &chr->work[2];
    BtlActDAttack *atk = &chr->attack;
    s32 next;

    if (phase == PHASE_ENTER) {
        BtlAct_LatchAttack(chr);
        BtlAnim_Play(chr, 0x71, 0.1f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_PlayVoice(chr, 0x14);
        if (atk->flags & 0x80) {
            BtlChar_SetFlag(chr, 0xCD);
        }
        if (atk->cut >= 0) {
            ChrCam_RequestCut(chr, 1, atk->cut);
        }
        if (atk->unk9 < 4) {
            if (atk->unk9 > 0) {
                BtlChar_SetFxBit(chr, 0xD);
            }
        }
    }
    if (phase == PHASE_RUN) {
        switch (BtlAnim_GetId(chr)) {
            case 0x71:
            case 0x72:
                if (BtlAnim_IsNew(chr)) {
                    (*count)++;
                    BtlMember_SpendKi(chr, BtlKiBlast_GetKiCost(chr), 0);
                }
                BtlAnim_SetDuration(chr, 0.1f);
                if (BtlAnim_Advance(chr, 0)) {
                    if (*count >= 10) {
                        BtlAct_Request(chr, 0xB);
                    } else {
                        if (BtlParam_GetFlags(chr) & 0x100) {
                            next = 0x72;
                        } else if (BtlChar_TestMemberUnk70(chr)) {
                            next = 0x72;
                        } else if (BtlAnim_GetId(chr) == 0x71) {
                            next = 0x72;
                        } else {
                            next = 0x71;
                        }
                        BtlAnim_Request(chr, next, 0.0f);
                    }
                }
                BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 2, BtlAnim_GetId(chr) + 4, 1);
                break;
        }
        BtlMove_Step(chr, 2, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        if (BtlChar_TestFlag(chr, 5)) {
            BtlCharApi_GetNodePos(chr->objId, 0x11, &pos);
            BtlCharApi_CalcAimDir(chr->objId, 0x11, &pos, &chr->aimDir, -BTL_DEG(36.0f), BTL_DEG(36.0f));
        } else {
            chr->aimDir.x = Mathf_Sin(BtlChar_GetPos(chr)->yaw);
            chr->aimDir.y = 0.0f;
            chr->aimDir.z = Mathf_Cos(BtlChar_GetPos(chr)->yaw);
            chr->aimDir.w = 0.0f;
        }
        if (atk->flags & 0x40) {
            if (BtlChar_IsFlagRaised(chr, 0x5B)) {
                if (atk->cut >= 0) {
                    ChrCam_EndCut(chr);
                }
            }
        }
    }
}

/*
 * Landing feedback. Parameter +2 == 4: rumble and camera shake around the fighter, sound 0x25, effect 0x35.
 * Otherwise effect 0x32 and sound 0xB (parameter flag 0x8000), 0x33 (soft) or 0x30 (hard).
 */
void BtlAct_PlayLandFx(BtlActDChr *chr, s32 hard) {
    s32 snd;

    if (BtlParam_GetUnk2(chr) == 4) {
        BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 0.8f, 0.1f);
        BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 3.0f, 0.3f);
        BtlCharSnd_PlayCommon(chr, 0x25);
        BtlChar_SetFxBit(chr, 0x35);
        return;
    }
    if (BtlParam_GetFlags(chr) & 0x8000) {
        snd = 0xB;
    } else {
        snd = (hard != 0) ? 0x30 : 0x33;
    }
    BtlCharSnd_PlayCommon(chr, snd);
    BtlChar_SetFxBit(chr, 0x32);
}

/*
 * The idle motion: on the ground 0 or 1; in flight 0x26 or 0x27, or 0x18B for a fighter that cannot fly and is not
 * in water. The second of each pair is used while the gauge block's +0x28 word is set.
 */
s32 BtlAct_GetIdleMotion(BtlActDChr *chr) {
    s32 motion;

    if (BtlChar_TestFlag(chr, 0xE)) {
        if (!BtlParam_CanFly(chr)) {
            if (!BtlChar_TestFlag(chr, 0x11)) {
                motion = 0x18B;
            } else {
                motion = (BtlMember_GetActiveGauge(chr)->unk28 != 0) ? 0x27 : 0x26;
            }
        } else {
            motion = (BtlMember_GetActiveGauge(chr)->unk28 != 0) ? 0x27 : 0x26;
        }
    } else {
        motion = BtlMember_GetActiveGauge(chr)->unk28 != 0;
    }
    return motion;
}

/* Decision functions: fill the action queue from the input; mask bits enable groups; non-zero when queued. */
extern s32 BtlDecide_Common(BtlActDChr *chr, s32 mask); /* forced transitions: stun, flag 0xBE, lost footing, death */
extern s32 BtlDecide_Main(BtlActDChr *chr, u32 mask);   /* movement, guard, techniques, changes */
extern s32 BtlDecide_Attack(BtlActDChr *chr, u32 mask);    /* attacks: rush, ki blasts, throws */

/*
 * Action 0xB: the neutral state. Every other action returns here.
 * enter: leaves flight mode when lower than BTL_KMH(50) (0.46) above the ground, plays the idle motion.
 * run: flight mode follows the ground (0xF clears it; water sets it); faces the opponent (step modes 6, 6, 7) with
 *   no speed; flags 0xC9, 0x1C, 0x25; with gauge +0x28 set also 0x96, and 0x137 for parameter flag 0x10000.
 * decide: outside mode 1, a fighter with flag 3 whose member +0xA0 is <= 0 and without flag 0xB1 requests action
 *   0x43 after more than 90 frames; then BtlDecide_Main(0x22FCBFEB), BtlDecide_Attack(0x0800054F),
 *   BtlDecide_Common(0xF), and queue[0] is requested (the later calls overwrite slot 0 of the earlier ones).
 */
s32 BtlAct_NeutralHandler(BtlActDChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 look;
    s32 motion;
    f32 speed;

    if (phase == PHASE_ENTER) {
        if (BtlAct_GetHeight(chr) < BTL_KMH(50.0f)) {
            BtlChar_ClearFlag(chr, 0xE);
        }
        BtlAnim_Play(chr, BtlAct_GetIdleMotion(chr), 0.15f);
    }
    if (phase == PHASE_RUN) {
        look = 1;
        if (BtlChar_TestFlag(chr, 0xE)) {
            if (BtlChar_TestFlag(chr, 0xF)) {
                BtlChar_ClearFlag(chr, 0xE);
            }
        } else if (!BtlChar_TestFlag(chr, 0xF)) {
            if (BtlChar_TestFlag(chr, 0x11)) {
                BtlChar_SetHeldFlag(chr, 0xE);
            }
        }
        BtlAnim_AdvanceLoop(chr, 0);
        if (BtlMove_IsBlockedByOpponent(chr)) {
            BtlChar_GetPos(chr)->speed = 0.0f;
        }
        speed = BTL_KMH(100.0f);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, speed);
        if (BtlAnim_GetId(chr) == 0x18B) {
            BtlMove_MoveVertical(chr, speed, speed);
        } else {
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFlag(chr, 0xC9);
        BtlChar_SetFlag(chr, 0x1C);
        BtlChar_SetFlag(chr, 0x25);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
            if (BtlParam_GetFlags(chr) & 0x10000) {
                BtlChar_SetFlag(chr, 0x137);
                look = 0;
            }
        }
        motion = BtlAct_GetIdleMotion(chr);
        if (BtlAnim_GetId(chr) != motion) {
            BtlAnim_Request(chr, motion, 0.15f);
        }
        if (look) {
            BtlChar_SetLookEnabled(chr, 1);
        }
    }
    if (phase == PHASE_DECIDE) {
        if (Battle_GetMode() != 1) {
            if (BtlChar_TestFlag(chr, 3)) {
                if (((BtlActDMember *)BtlMember_GetActive(chr))->unkA0 <= 0) {
                    if (!BtlChar_TestFlag(chr, 0xB1)) {
                        if (++(*timer) > 90) {
                            BtlAct_Request(chr, 0x43);
                        }
                    }
                }
            }
        }
        BtlDecide_Main(chr, 0x22FCBFEB);
        BtlDecide_Attack(chr, 0x0800054F);
        BtlDecide_Common(chr, 0xF);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/* Action 0xC: the idle motion played once over one second without any input check, then action 0xB. */
s32 BtlAct_IdleOnceHandler(BtlActDChr *chr, s32 phase) {
    if (phase == PHASE_ENTER) {
        BtlAnim_Play(chr, BtlAct_GetIdleMotion(chr), 0.15f);
    }
    if (phase == PHASE_RUN) {
        BtlAnim_SetDuration(chr, 1.0f);
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1C);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
            if (BtlParam_GetFlags(chr) & 0x10000) {
                BtlChar_SetFlag(chr, 0x137);
            }
        }
    }
}

extern f32 BtlMoveParam_GetSpeed(BtlActDChr *chr, s32 kind); /* speed of the character table (obj +0x928)[kind], scaled; kinds 4, 5 also by pose +0xAC */
extern s32 BtlMove_TurnYawRet(BtlActDChr *chr, s32 mode, f32 maxStep) __asm__("BtlMove_TurnYaw");

/*
 * Action 0xD: free movement (motions 2 -> 3 with side layers 4..7 weighted by the sine of the heading relative to
 * the camera; 0x18B for a non-flyer in the air). Speed kind 0, or 1 in water. Ends when input 1 (a direction held)
 * is no longer true.
 */
s32 BtlAct_MoveHandler(BtlActDChr *chr, s32 phase) {
    s32 motion;
    s32 subNeg;
    s32 subPos;
    s32 pitchMode;
    f32 speed;
    f32 side;

    if (phase == PHASE_ENTER) {
        motion = 2;
        if (!BtlParam_CanFly(chr)) {
            if (BtlChar_TestFlag(chr, 0xE)) {
                if (!BtlChar_TestFlag(chr, 0x11)) {
                    motion = 0x18B;
                }
            }
        }
        BtlAnim_Play(chr, motion, 0.15f);
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlCharSnd_PlayCommon(chr, 0x38);
        }
    }
    if (phase == PHASE_RUN) {
        speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11));
        subNeg = 0;
        subPos = 0;
        switch (BtlAnim_GetId(chr)) {
            case 2:
                BtlAnim_AdvanceThen(chr, 3, 0.0f, 0);
                subNeg = 4;
                subPos = 6;
                break;
            case 3:
                BtlAnim_AdvanceLoop(chr, 0);
                subNeg = 5;
                subPos = 7;
                break;
            case 0x18B:
                BtlAnim_AdvanceLoop(chr, 0);
                subNeg = -1;
                subPos = -1;
                break;
        }
        if (subNeg >= 0 && subPos >= 0) {
            side = Mathf_Sin(BtlAct_GetRotYRelCam(chr));
            if (0.0f < side) {
                BtlAnim_SetUnkC8C(chr, side);
                BtlAnim_PlaySub(chr, subPos);
            } else {
                BtlAnim_SetUnkC8C(chr, -side);
                BtlAnim_PlaySub(chr, subNeg);
            }
        }
        pitchMode = 5;
        if (BtlChar_TestFlag(chr, 0xE)) {
            if (BtlChar_TestFlag(chr, 5)) {
                pitchMode = 2;
            }
        }
        BtlMove_Step(chr, 0, pitchMode, 3, speed, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xC9);
        BtlChar_SetFlag(chr, 0x1C);
        BtlChar_SetFlag(chr, 0x25);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        BtlChar_SetLookEnabled(chr, 1);
        if (BtlChar_TestFlag(chr, 0xE)) {
            if (BtlChar_TestFlag(chr, 0xF)) {
                BtlChar_ClearFlag(chr, 0xE);
            }
        }
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
        if (BtlAnim_GetId(chr) == 0x18B) {
            if (BtlChar_TestFlag(chr, 0x11)) {
                BtlAnim_Request(chr, 3, 0.15f);
            }
        } else if (!BtlParam_CanFly(chr)) {
            if (!BtlChar_TestFlag(chr, 0x11)) {
                if (!BtlChar_TestFlag(chr, 0xF)) {
                    BtlAnim_Request(chr, 0x18B, 0.15f);
                }
            }
        }
    }
    if (phase == PHASE_DECIDE) {
        if (BtlInput_TestAction(chr, 1, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlDecide_Main(chr, 0x22981FEB);
        BtlDecide_Attack(chr, 0x0800040F);
        BtlDecide_Common(chr, 0xF);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/*
 * Action 0xE: movement close to the opponent (flag 0x13), always facing them: motions 8 (toward) / 9 (away) by the
 * direction of travel, side layers 0xA / 0xB. Speed kind 2, or 3 in water, scaled down on approach within 30.
 * Ends when input 2 (no direction for 3 frames) is true. Leaving to action 0x11 snaps the yaw.
 */
s32 BtlAct_CloseMoveHandler(BtlActDChr *chr, s32 phase) {
    f32 *lean = (f32 *)&chr->work[6];
    s32 motion;
    f32 dir;
    f32 speed;

    if (phase == PHASE_ENTER) {
        motion = 8;
        if (!(BtlInput_GetStickY(chr) < 0.0f)) {
            motion = 9;
        }
        if (!BtlParam_CanFly(chr)) {
            if (BtlChar_TestFlag(chr, 0xE)) {
                if (!BtlChar_TestFlag(chr, 0x11)) {
                    motion = 0x18B;
                }
            }
        }
        BtlAnim_Play(chr, motion, 0.15f);
        BtlMove_SetDirection(chr, 0);
        *lean = Mathf_Sin(BtlAct_GetVelDirRelCam(chr));
    }
    if (phase == PHASE_RUN) {
        if (BtlChar_TestFlag(chr, 0xE)) {
            if (BtlChar_TestFlag(chr, 0xF)) {
                BtlChar_ClearFlag(chr, 0xE);
            }
        }
        if (BtlAnim_GetId(chr) != 0x18B) {
            dir = BtlAct_GetVelDirRelCam(chr);
            if (-BTL_DEG(90.0f) < dir && dir < BTL_DEG(90.0f)) {
                if (BtlAnim_GetId(chr) == 9) {
                    BtlAnim_RequestKeep(chr, 8, 0.15f);
                }
            } else {
                if (BtlAnim_GetId(chr) == 8) {
                    BtlAnim_RequestKeep(chr, 9, 0.15f);
                }
            }
            *lean = BtlUtil_ApproachF(*lean, Mathf_Sin(dir), 8.0f / 30.0f);
            if (0.0f < *lean) {
                BtlAnim_SetUnkC8C(chr, *lean);
                BtlAnim_PlaySub(chr, 0xB);
            } else {
                BtlAnim_SetUnkC8C(chr, -*lean);
                BtlAnim_PlaySub(chr, 0xA);
            }
        }
        BtlAnim_AdvanceLoop(chr, 0);
        speed = BtlAct_ScaleSpeedByApproach(chr, BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) + 2), 30.0f);
        BtlMove_Step(chr, 2, 2, BtlChar_TestFlag(chr, 0xE) != 0, speed, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x1C);
        BtlChar_SetLookEnabled(chr, 1);
        BtlMove_RequestOrbit(chr, 10.0f, 20.0f);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
        if (BtlAnim_GetId(chr) == 0x18B) {
            if (BtlChar_TestFlag(chr, 0x11)) {
                BtlAnim_Request(chr, (BtlInput_GetStickY(chr) < 0.0f) ? 8 : 9, 0.15f);
            }
        } else if (!BtlParam_CanFly(chr)) {
            if (!BtlChar_TestFlag(chr, 0x11)) {
                if (!BtlChar_TestFlag(chr, 0xF)) {
                    BtlAnim_Request(chr, 0x18B, 0.15f);
                }
            }
        }
    }
    if (phase == PHASE_DECIDE) {
        if (BtlInput_TestAction(chr, 2, 1)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlDecide_Main(chr, 0x229CBFE3);
        BtlDecide_Attack(chr, 0x0800054F);
        BtlDecide_Common(chr, 0xF);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
    if (phase == PHASE_LEAVE) {
        if (BtlAct_GetRequested(chr) == 0x11) {
            return BtlMove_TurnYawRet(chr, 0, 3.14159265f);
        }
    }
}

extern f32 BtlMoveParam_GetTurnRate(BtlActDChr *chr, s32 kind); /* turn rate, table +0x74[kind] in degrees; kind 0 scaled by pose +0xAC */

/*
 * Action 0xF: dash (motions 0xC start, 0xD loop, 0xE stop, 0x15 turn-around; side layers 0xF..0x14, 0x16, 0x17).
 * Speed kind 4, or 5 in water. work[0] bit 0: a turn was asked for during the start; work[1]: finished.
 * Entered from actions 0x3E, 0xB0, 0xB1 it resumes in the loop.
 */
s32 BtlAct_DashMoveHandler(BtlActDChr *chr, s32 phase) {
    extern s32 TagAssist_Handle(s32,s32);
    if(TagAssist_Handle(chr->player,phase)) return 0;
    s32 subNeg;
    s32 subPos;
    s32 moving;
    s32 pitchMode;
    s32 resume;
    s32 queued;
    f32 ratio;
    f32 speed;
    f32 side;
    f32 toward;

    if (phase == PHASE_ENTER) {
        resume = 0;
        switch (BtlAct_GetPrev(chr)) {
            case 0x3E:
            case 0xB0:
            case 0xB1:
                resume = 1;
                break;
        }
        if (resume) {
            BtlAnim_Play(chr, 0xD, 0.25f);
        } else {
            BtlAnim_Play(chr, 0xC, 0.15f);
            BtlChar_SetFlag(chr, 0xD0);
            BtlMove_TurnYaw(chr, 0, 3.14159265f);
            if (BtlChar_TestFlag(chr, 0x12)) {
                BtlCharSnd_PlayCommon(chr, 0x23);
            } else if (BtlChar_TestFlag(chr, 0x11)) {
                BtlCharSnd_PlayCommon(chr, 0x39);
            } else {
                BtlCharSnd_PlayCommon(chr, 0x1E);
            }
        }
    }
    if (phase == PHASE_RUN) {
        subNeg = 0;
        subPos = 0;
        ratio = 0.0f;
        moving = 1;
        switch (BtlAnim_GetId(chr)) {
            case 0xC:
                BtlAnim_AdvanceThen(chr, 0xD, 0.15f, 0);
                subNeg = 0xF;
                ratio = BtlAnim_GetProgress(chr);
                subPos = 0x12;
                break;
            case 0xD:
                BtlAnim_AdvanceLoop(chr, 0);
                subNeg = 0x10;
                ratio = 1.0f;
                subPos = 0x13;
                break;
            case 0xE:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                subNeg = 0x11;
                ratio = 1.0f - BtlAnim_GetProgress(chr);
                subPos = 0x14;
                moving = 0;
                break;
            case 0x15:
                if (BtlAnim_IsNew(chr)) {
                    if (BtlChar_TestFlag(chr, 0xF)) {
                        BtlChar_SetFxBit(chr, 0x31);
                    }
                }
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                subNeg = 0x16;
                ratio = 1.0f - BtlAnim_GetProgress(chr);
                subPos = 0x17;
                moving = 0;
                chr->work[0] &= ~1;
                break;
        }
        if (!(BtlParam_GetFlags(chr) & 0x20000)) {
            side = Mathf_Sin(BtlAct_GetRotYRelCam(chr));
            if (0.0f < side) {
                BtlAnim_SetUnkC8C(chr, side);
                BtlAnim_PlaySub(chr, subPos);
            } else {
                BtlAnim_SetUnkC8C(chr, -side);
                BtlAnim_PlaySub(chr, subNeg);
            }
        }
        if (BtlChar_TestFlag(chr, 5)) {
            if (moving) {
                if (!BtlChar_TestFlag(chr, 0xE) || !BtlInput_IsHeld(chr, 0xF0)) {
                    BtlChar_SetFlag(chr, 0x1A);
                }
            }
        }
        pitchMode = 5;
        if (BtlChar_TestFlag(chr, 0xE)) {
            if (BtlChar_TestFlag(chr, 5)) {
                pitchMode = 2;
            }
        }
        if (moving) {
            speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11) + 4);
            BtlMove_TurnYaw(chr, 0, BtlMoveParam_GetTurnRate(chr, 0));
            BtlMove_TurnPitch(chr, pitchMode, BTL_DEG(9.0f));
            BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, 10000.0f);
            BtlMove_ApplyGravity(chr);
            BtlChar_SetFxBit(chr, 0xB);
            if (BtlChar_TestFlag(chr, 0xF)) {
                BtlChar_SetFxBit(chr, 0x30);
            }
        } else {
            BtlMove_Step(chr, 6, 5, 3, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        toward = Mathf_Cos(BtlOpp_GetYawFromFacing(chr));
        if (0.0f < toward) {
            BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch * toward * ratio);
        }
        BtlChar_SetFlag(chr, 0xCA);
        BtlChar_SetFlag(chr, 0x25);
        BtlMove_RequestOrbit(chr, 50.0f, 100.0f);
        BtlChar_SetFxBit(chr, 0x3A);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlChar_IsStage4Or27()) {
            if (BtlChar_TestFlag(chr, 0x17)) {
                BtlChar_SetHeldFlag(chr, 0xE);
            }
        }
    }
    if (phase == PHASE_DECIDE) {
        if (chr->work[1] != 0) {
            BtlAct_Request(chr, 0xB);
            if (!BtlAct_HasQueued(chr)) {
                BtlDecide_Main(chr, 1);
                BtlDecide_Common(chr, 1);
            }
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        switch (BtlAnim_GetId(chr)) {
            case 0xC:
                if (BtlInput_TestAction(chr, 4, 1)) {
                    chr->work[0] |= 1;
                }
                break;
            case 0xD:
                if (BtlInput_TestAction(chr, 6, 1)) {
                    BtlAnim_Request(chr, 0xE, 0.15f);
                }
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    BtlAnim_Request(chr, 0xE, 0.15f);
                    BtlChar_GetPos(chr)->speed = 0.0f;
                }
                if (BtlInput_TestAction(chr, 4, 1) || (chr->work[0] & 1)) {
                    BtlAnim_Request(chr, 0x15, 0.15f);
                }
                break;
            case 0xE:
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    BtlChar_GetPos(chr)->speed = 0.0f;
                }
                BtlDecide_Main(chr, 0x300);
                break;
            case 0x15:
                if (BtlAnim_PassedRatio(chr, 0.5f)) {
                    if (BtlInput_TestAction(chr, 5, 1)) {
                        BtlAct_Request(chr, 0xF);
                        BtlChar_GetPos(chr)->unkAC = BtlUtil_MinF(BtlChar_GetPos(chr)->unkAC + 0.1f, 1.5f);
                    }
                }
                break;
        }
        queued = BtlDecide_Main(chr, 0x20001C64);
        if (BtlAnim_GetId(chr) != 0xE) {
            if (BtlInput_TestAction(chr, 0x40, 1)) {
                BtlAct_Request(chr, 0x58);
            }
            queued |= BtlDecide_Attack(chr, 0xC000);
            if (BtlInput_TestAction(chr, 0x2C, 1)) {
                BtlAct_Request(chr, 0x3E);
            }
        }
        queued |= BtlDecide_Common(chr, 0xE);
        if (queued) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

extern f32 BtlMoveParam_GetHeight(BtlActDChr *chr, s32 kind); /* jump height, table +0x5C[kind] * 10 */
extern s32 BtlAct_GetAction14or16(BtlActDChr *chr);
extern s32 BtlAct_GetAction11or15(BtlActDChr *chr);

/* Actions 0x10 and 0x12: jump take-off (motion 0x1F), then action 0x11 or 0x13. */
s32 BtlAct_JumpStartHandler(BtlActDChr *chr, s32 phase) {
    f32 height;
    f32 speed;
    BtlActDPose *pose;

    if (phase == PHASE_ENTER) {
        BtlAnim_Play(chr, 0x1F, 0.1f);
        BtlChar_ClearFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x31);
        chr->unkFF0 = 0;
        if (BtlAct_GetCurrent(chr) == 0x12) {
            chr->work[0] |= 1;
        }
    }
    if (phase == PHASE_RUN) {
        if (BtlAnim_Advance(chr, 1)) {
            BtlAct_Request(chr, (chr->work[0] & 1) ? 0x13 : 0x11);
        }
        if (BtlAnim_PassedRatio(chr, 0.3f)) {
            height = BtlMoveParam_GetHeight(chr, chr->work[0] & 1);
            pose = BtlChar_GetPos(chr);
            pose->fallSpeed = BtlMove_CalcJumpSpeed(height);
        }
        speed = BtlMoveParam_GetSpeed(chr, (chr->work[0] & 1) ? 4 : 0xD);
        BtlMove_Step(chr, 0, 5, 3, speed * BtlInput_GetStickLength(chr), BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xC9);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFxBit(chr, 0x3A);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
    }
    if (phase == PHASE_DECIDE) {
        if (BtlInput_TestAction(chr, 9, 1)) {
            BtlAct_Request(chr, BtlAct_GetAction14or16(chr));
        }
        if (BtlParam_CanFly(chr)) {
            if (BtlInput_TestAction(chr, 0xC, 1)) {
                BtlAct_Request(chr, BtlAct_GetAction11or15(chr));
            }
        }
        if (BtlInput_TestAction(chr, 7, 1)) {
            BtlAct_Request(chr, 0xF);
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        BtlDecide_Main(chr, 0x1C00);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
        if (BtlInput_TestAction(chr, 0x2C, 1)) {
            BtlAct_Request(chr, 0x3F);
        }
        if (BtlDecide_Attack(chr, 0x70000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/* Actions 0x11 and 0x13: jump in the air (motions 0x20 rising, 0x21 falling, 0x22 landing), then action 0xB. */
s32 BtlAct_JumpAirHandler(BtlActDChr *chr, s32 phase) {
    f32 turn;
    f32 speed;
    f32 accel;

    if (phase == PHASE_ENTER) {
        if (0.0f <= BtlChar_GetPos(chr)->fallSpeed) {
            BtlAnim_Play(chr, 0x21, 0.15f);
        } else {
            BtlAnim_Play(chr, 0x20, 0.15f);
        }
        BtlChar_ClearFlag(chr, 0xE);
    }
    if (phase == PHASE_RUN) {
        speed = 0.0f;
        accel = BTL_KMH(50.0f);
        turn = BtlMoveParam_GetTurnRate(chr, 1);
        switch (BtlAct_GetCurrent(chr)) {
            case 0x11:
                speed = BtlMoveParam_GetSpeed(chr, 0xD);
                break;
            case 0x13:
                speed = BtlMoveParam_GetSpeed(chr, 4);
                break;
        }
        switch (BtlAnim_GetId(chr)) {
            case 0x20:
                BtlAnim_AdvanceLoop(chr, 0);
                if (0.0f <= BtlChar_GetPos(chr)->fallSpeed) {
                    BtlAnim_Request(chr, 0x21, 0.15f);
                }
                break;
            case 0x21:
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlAct_GetFramesToGround(chr) < 3.0f) {
                    if (!BtlChar_IsStage4Or27() || BtlChar_TestFlag(chr, 7) || BtlAct_GetGroundY(chr) < -10.0f) {
                        BtlAnim_Request(chr, 0x22, 0.1f);
                    }
                }
                break;
            case 0x22:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                }
                speed = 0.0f;
                accel = BTL_KMH(100.0f);
                if (BtlAnim_PassedRatio(chr, 0.1f)) {
                    BtlAct_PlayLandFx(chr, 0);
                }
                break;
        }
        speed *= BtlInput_GetStickLength(chr);
        BtlMove_TurnYaw(chr, 0, turn);
        BtlMove_TurnPitch(chr, 5, 3.14159265f);
        BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
        BtlMove_SetDirection(chr, 3);
        BtlMove_Advance(chr, speed, accel);
        BtlMove_ApplyGravity(chr);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFxBit(chr, 0x3A);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
    }
    if (phase == PHASE_DECIDE) {
        if (BtlChar_TestFlag(chr, 0x16)) {
            BtlChar_SetHeldFlag(chr, 0xE);
            BtlAct_Request(chr, 0xB);
        }
        if (BtlChar_IsStage4Or27()) {
            if (BtlChar_TestFlag(chr, 0x17)) {
                BtlChar_SetHeldFlag(chr, 0xE);
                BtlAct_Request(chr, 0xB);
            }
        }
        if (BtlChar_TestFlag(chr, 0x11)) {
            BtlChar_SetHeldFlag(chr, 0xE);
            BtlAct_Request(chr, 0xB);
        }
        if (BtlInput_TestAction(chr, 0xA, 1)) {
            BtlAct_Request(chr, BtlAct_GetAction14or16(chr));
        }
        if (BtlParam_CanFly(chr)) {
            if (BtlInput_TestAction(chr, 0xC, 1)) {
                BtlAct_Request(chr, BtlAct_GetAction11or15(chr));
            }
        }
        BtlDecide_Main(chr, (BtlAnim_GetId(chr) != 0x22) ? 0x1C00 : 0x1C02);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
        if (BtlInput_TestAction(chr, 7, 1)) {
            BtlAct_Request(chr, 0xF);
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlAnim_GetId(chr) != 0x22) {
            if (BtlInput_TestAction(chr, 0x2C, 1)) {
                BtlAct_Request(chr, 0x3F);
            }
            if (BtlDecide_Attack(chr, 0x70000)) {
                BtlAct_Request(chr, BtlAct_GetQueued(chr));
            }
        }
    }
}

extern f32 BtlMoveParam_GetAccel(BtlActDChr *chr, s32 kind); /* vertical acceleration, table +0x50[kind] */

/* Action 0x14: ascend (motion 0x2B, entered through 0x2E after a jump). */
s32 BtlAct_AscendHandler(BtlActDChr *chr, s32 phase) {
    s32 motion;
    f32 speed;
    f32 vspeed;
    f32 vaccel;

    if (phase == PHASE_ENTER) {
        motion = 0x2B;
        if (chr->unkFF0 > 0) {
            motion = 0x2E;
            BtlCharSnd_PlayCommon(chr, 0x1F);
        }
        BtlAnim_Play(chr, motion, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if (phase == PHASE_RUN) {
        switch (BtlAnim_GetId(chr)) {
            case 0x2E:
                BtlAnim_SetDuration(chr, 0.35f);
                BtlAnim_AdvanceThen(chr, 0x2B, 0.15f, 0);
                BtlMove_Step(chr, 6, 5, 3, 0.0f, BTL_KMH(50.0f));
                BtlMove_ApplyGravity(chr);
                break;
            case 0x2B:
                if (BtlAnim_IsNew(chr)) {
                    BtlCharSnd_PlayCommon(chr, 0x18);
                }
                BtlAnim_AdvanceLoop(chr, 0);
                speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11)) * BtlInput_GetStickLength(chr);
                vspeed = -BtlMoveParam_GetSpeed(chr, 0x10);
                vaccel = BtlMoveParam_GetAccel(chr, 0);
                BtlMove_Step(chr, 0, 5, 3, speed, BTL_KMH(50.0f));
                BtlMove_MoveVertical(chr, vspeed, vaccel);
                break;
        }
        BtlChar_SetFlag(chr, 0xC9);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == PHASE_DECIDE) {
        if (BtlInput_TestAction(chr, 0xB, 1)) {
            BtlAct_Request(chr, 0xB);
            BtlDecide_Main(chr, 0x41);
        }
        BtlDecide_Main(chr, 0x1800);
        BtlDecide_Attack(chr, 0x70000);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/* Action 0x15: descend (motion 0x2C, entered through 0x1F after a jump), landing with motion 0x2D. */
s32 BtlAct_DescendHandler(BtlActDChr *chr, s32 phase) {
    s32 motion;
    f32 speed;
    f32 vspeed;
    f32 vaccel;

    if (phase == PHASE_ENTER) {
        motion = 0x2C;
        if (chr->unkFF0 > 0) {
            motion = 0x1F;
            BtlCharSnd_PlayCommon(chr, 0x1F);
        }
        BtlAnim_Play(chr, motion, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if (phase == PHASE_RUN) {
        speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11)) * BtlInput_GetStickLength(chr);
        vspeed = BtlMoveParam_GetSpeed(chr, 0x11);
        vaccel = BtlMoveParam_GetAccel(chr, 1);
        switch (BtlAnim_GetId(chr)) {
            case 0x1F:
                BtlAnim_SetDuration(chr, 0.35f);
                BtlAnim_AdvanceThen(chr, 0x2C, 0.15f, 0);
                break;
            case 0x2C:
                if (BtlAnim_IsNew(chr)) {
                    BtlCharSnd_PlayCommon(chr, 0x1A);
                }
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlAct_GetFramesToGround(chr) < 1.0f) {
                    if (!BtlChar_IsStage4Or27() || BtlChar_TestFlag(chr, 7) || BtlAct_GetGroundY(chr) < -10.0f) {
                        if (!(BtlChar_GetPos(chr)->unkD0 & 0x40)) {
                            BtlAnim_Request(chr, 0x2D, 0.0f);
                            BtlAct_PlayLandFx(chr, 1);
                        }
                    }
                }
                BtlMove_Step(chr, 0, 5, 3, speed, BTL_KMH(50.0f));
                BtlMove_MoveVertical(chr, vspeed, vaccel);
                BtlChar_SetFlag(chr, 0xC9);
                break;
            case 0x2D:
                BtlChar_ClearFlag(chr, 0xE);
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                BtlMove_Step(chr, 0, 5, 3, 0.0f, BTL_KMH(100.0f));
                BtlMove_ApplyGravity(chr);
                break;
        }
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        if (BtlMember_GetActiveGauge(chr)->unk28 != 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == PHASE_DECIDE) {
        switch (BtlAnim_GetId(chr)) {
            case 0x1F:
            case 0x2C:
                if (BtlInput_TestAction(chr, 0xD, 1)) {
                    BtlAct_Request(chr, 0xB);
                    BtlDecide_Main(chr, 0x21);
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
            case 0x2D:
                if (chr->work[1] != 0) {
                    if (BtlInput_TestAction(chr, 1, 1)) {
                        BtlAct_Request(chr, 0xD);
                    } else {
                        BtlAct_Request(chr, 0xB);
                    }
                }
                break;
        }
        BtlDecide_Main(chr, 0x1800);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

extern s32 BtlParam_GetDashSound(BtlActDChr *chr);            /* burst sound: 0x1D with parameter flag 0x10, else 0x1C */
extern s32 BtlMoveParam_GetKiCost(BtlActDChr *chr, s32 kind);  /* ki drain per frame, table +0x64[kind] / 30 */

/* Action 0x16: a short rise that fades out over motion 0x18C, then action 0xB. */
s32 BtlAct_HopHandler(BtlActDChr *chr, s32 phase) {
    f32 fade;
    f32 speed;
    f32 vspeed;
    f32 vaccel;

    if (phase == PHASE_ENTER) {
        BtlAnim_Play(chr, 0x18C, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlCharSnd_PlayCommon(chr, 0x1F);
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
    }
    if (phase == PHASE_RUN) {
        fade = 1.0f;
        speed = BtlMoveParam_GetSpeed(chr, BtlChar_TestFlag(chr, 0x11));
        vspeed = BtlMoveParam_GetSpeed(chr, 0x10);
        vaccel = BtlMoveParam_GetAccel(chr, 0);
        fade -= BtlAnim_GetProgress(chr);
        speed *= BtlInput_GetStickLength(chr) * fade;
        vspeed *= fade;
        vaccel *= 0.5f;
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
        }
        BtlMove_Step(chr, 0, 5, 2, speed, BTL_KMH(50.0f));
        BtlMove_MoveVertical(chr, -vspeed, vaccel);
        BtlChar_SetFlag(chr, 0xC9);
        BtlMove_RequestOrbit(chr, 20.0f, 50.0f);
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == PHASE_DECIDE) {
        BtlDecide_Main(chr, 0x1800);
        BtlDecide_Attack(chr, 0x70000);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/* Action 0x17: fast ascend (motions 0x2E start, 0x2F loop, 0x30 stop); drains ki while it lasts. */
s32 BtlAct_FastAscendHandler(BtlActDChr *chr, s32 phase) {
    f32 vspeed;

    if (phase == PHASE_ENTER) {
        BtlAnim_Play(chr, 0x2E, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        if (chr->unkFF0 > 0) {
            chr->work[0] |= 1;
        }
    }
    if (phase == PHASE_RUN) {
        vspeed = -BtlMoveParam_GetSpeed(chr, 9);
        switch (BtlAnim_GetId(chr)) {
            case 0x2E:
                if (chr->work[0] & 1) {
                    BtlAnim_SetDuration(chr, 0.5f);
                }
                if (BtlAnim_AdvanceThen(chr, 0x2F, 0.15f, 0)) {
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                    BtlChar_Vibrate(chr, 0.8f, 0.3f);
                }
                BtlMove_MoveVertical(chr, 0.0f, BTL_KMH(100.0f));
                break;
            case 0x2F:
                BtlAnim_AdvanceLoop(chr, 0);
                BtlMove_MoveVertical(chr, vspeed, 10000.0f);
                BtlChar_SetFxBit(chr, 4);
                BtlChar_SetFlag(chr, 9);
                BtlChar_SetVibration(chr, 0.7f, 0.1f);
                break;
            case 0x30:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                BtlMove_MoveVertical(chr, 0.0f, BTL_KMH(100.0f));
                break;
        }
        BtlMove_Step(chr, 6, 5, 3, 0.0f, BTL_KMH(100.0f));
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == PHASE_DECIDE) {
        switch (BtlAnim_GetId(chr)) {
            case 0x2E:
                break;
            case 0x2F: {
                s32 stop = 0;

                if (BtlInput_TestAction(chr, 0x10, 1)) {
                    stop = 1;
                }
                if (BtlMember_SpendKi(chr, BtlMoveParam_GetKiCost(chr, 2), 0)) {
                    stop = 1;
                }
                if (BtlChar_TestFlag(chr, 0x15)) {
                    stop = 1;
                }
                if (stop) {
                    BtlAnim_Request(chr, 0x30, 0.15f);
                }
                break;
            }
            case 0x30:
                if (chr->work[1] != 0) {
                    BtlAct_Request(chr, 0xB);
                    BtlDecide_Main(chr, 1);
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
        }
        BtlDecide_Main(chr, 0x1400);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/* Action 0x18: fast descend (motions 0x31 start, 0x32 loop, 0x33 stop); drains ki while it lasts. */
s32 BtlAct_FastDescendHandler(BtlActDChr *chr, s32 phase) {
    f32 vspeed;

    if (phase == PHASE_ENTER) {
        BtlAnim_Play(chr, 0x31, 0.15f);
        BtlChar_SetHeldFlag(chr, 0xE);
        if (chr->unkFF0 > 0) {
            chr->work[0] |= 1;
        }
    }
    if (phase == PHASE_RUN) {
        vspeed = BtlMoveParam_GetSpeed(chr, 9);
        switch (BtlAnim_GetId(chr)) {
            case 0x31:
                if (chr->work[0] & 1) {
                    BtlAnim_SetDuration(chr, 0.5f);
                }
                if (BtlAnim_AdvanceThen(chr, 0x32, 0.15f, 0)) {
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                    BtlChar_Vibrate(chr, 0.8f, 0.3f);
                }
                BtlMove_MoveVertical(chr, 0.0f, BTL_KMH(100.0f));
                break;
            case 0x32:
                BtlAnim_AdvanceLoop(chr, 0);
                BtlMove_MoveVertical(chr, vspeed, 10000.0f);
                BtlChar_SetFxBit(chr, 4);
                BtlChar_SetFlag(chr, 9);
                BtlChar_SetVibration(chr, 0.7f, 0.1f);
                break;
            case 0x33:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                BtlMove_MoveVertical(chr, 0.0f, BTL_KMH(100.0f));
                break;
        }
        BtlMove_Step(chr, 6, 5, 3, 0.0f, BTL_KMH(100.0f));
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == PHASE_DECIDE) {
        switch (BtlAnim_GetId(chr)) {
            case 0x31:
                break;
            case 0x32: {
                s32 stop = 0;

                if (BtlInput_TestAction(chr, 0x12, 1)) {
                    stop = 1;
                }
                if (!(BtlChar_GetPos(chr)->unkD0 & 0x40000000) || (BtlChar_GetPos(chr)->unkD0 & 0x01000000)) {
                    if (BtlAct_GetFramesToGround(chr) < 0.3f * 30.0f) {
                        stop = 1;
                    }
                }
                if (BtlChar_TestFlag(chr, 0x16)) {
                    stop = 1;
                }
                if (BtlMember_SpendKi(chr, BtlMoveParam_GetKiCost(chr, 2), 0)) {
                    stop = 1;
                }
                if (stop) {
                    BtlAnim_Request(chr, 0x33, 0.15f);
                }
                break;
            }
            case 0x33:
                if (chr->work[1] != 0) {
                    BtlAct_Request(chr, 0xB);
                    BtlDecide_Main(chr, 1);
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
        }
        BtlDecide_Main(chr, 0xC00);
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

extern f32 BtlMoveParam_GetTurnAccel(BtlActDChr *chr, s32 axis); /* steering acceleration, table +0x80[axis] in degrees */
extern f32 BtlMoveParam_GetTurnMax(BtlActDChr *chr, s32 axis); /* steering speed limit */

/* Action 0x19: ki-draining dash that homes on the opponent (motions 0x18 / 0x2D start, 0x19 loop, 0x1A / 0x15 stop). */
s32 BtlAct_HomingDashHandler(BtlActDChr *chr, s32 phase) {
    s32 *slowFrames = &chr->work[2];
    s32 prev;
    s32 motion;
    s32 voice;
    s32 moving;
    s32 faceOpp;
    s32 first;
    s32 yawMode;
    f32 lean;
    f32 speed;
    f32 yawAccel;
    f32 pitchAccel;
    f32 yawMax;
    f32 pitchMax;
    f32 modelTurn;
    f32 near;
    BtlActDPose *pose;

    if (phase == PHASE_ENTER) {
        voice = -1;
        prev = BtlAct_GetPrev(chr);
        if (BtlAct_IsAttackId(prev)) {
            motion = 0x19;
            chr->unkD68++;
        } else {
            switch (prev) {
                case 0x47 ... 0x52:
                    voice = 0x1C;
                case 0x53 ... 0x57:
                case 0x5A ... 0x5D:
                case 0x6B ... 0x6F:
                    chr->unkD68++;
                case 0x1A:
                case 0x28:
                    motion = 0x19;
                    break;
                case 0x2A:
                case 0xD7:
                    motion = 0x19;
                    break;
                case 0x17:
                case 0x18:
                    motion = 0x2D;
                    break;
                default:
                    motion = 0x18;
                    voice = 0x24;
                    break;
            }
        }
        BtlAnim_Play(chr, motion, 0.15f);
        if (voice >= 0) {
            BtlChar_PlayVoice(chr, voice);
        }
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if (phase == PHASE_RUN) {
        lean = 0.0f;
        moving = 1;
        faceOpp = 1;
        first = 0;
        switch (BtlAnim_GetId(chr)) {
            case 0x18:
            case 0x2D:
                BtlAnim_AdvanceThen(chr, 0x19, 0.2f, 0);
                lean = BtlAnim_GetProgress(chr);
                moving = 0;
                if (0.5f < BtlAnim_GetProgress(chr)) {
                    BtlChar_SetFlag(chr, 0x47);
                }
                break;
            case 0x19:
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlAnim_IsNew(chr)) {
                    first = 1;
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                    BtlChar_SetFlag(chr, 0xD1);
                    BtlChar_Vibrate(chr, 0.8f, 0.3f);
                    if (BtlAct_GetHeight(chr) < 20.0f) {
                        BtlChar_SetFxBit(chr, 0x35);
                    }
                }
                BtlChar_SetFlag(chr, 0x47);
                lean = 1.0f;
                moving = 1;
                BtlChar_SetVibration(chr, 0.7f, 0.1f);
                break;
            case 0x15:
            case 0x1A:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                    BtlChar_GetPos(chr)->speed = 0.0f;
                }
                moving = 0;
                faceOpp = 0;
                lean = 1.0f - BtlAnim_GetProgress(chr);
                break;
        }
        if (moving) {
            speed = BtlMoveParam_GetSpeed(chr, 6);
            yawAccel = BtlMoveParam_GetTurnAccel(chr, 0);
            pitchAccel = BtlMoveParam_GetTurnAccel(chr, 1);
            yawMax = BtlMoveParam_GetTurnMax(chr, 0);
            pitchMax = BtlMoveParam_GetTurnMax(chr, 1);
            modelTurn = BTL_DEG(36.0f);
            if (first) {
                pitchAccel = 3.14159265f;
                yawAccel = pitchAccel;
                modelTurn = pitchAccel;
            }
            near = BtlUtil_MinF(BtlOpp_GetGapXZ(chr) * 0.01f, 1.0f);
            BtlMove_SteerAtOpponent(chr, speed, yawAccel, pitchAccel, yawMax * near, pitchMax * near, 3.14159265f);
            BtlMove_TurnModelYaw(chr, modelTurn, 0.5f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, 10000.0f);
            BtlMove_ApplyGravity(chr);
            BtlChar_SetFlag(chr, 9);
            BtlChar_SetFlag(chr, 0x50);
            BtlChar_SetFlag(chr, 0x51);
            BtlChar_SetFxBit(chr, 4);
            BtlChar_SetFxBit(chr, 9);
        } else {
            yawMode = faceOpp ? 2 : 6;
            if (BtlMove_IsBlockedByOpponent(chr)) {
                BtlChar_GetPos(chr)->speed = 0.0f;
            }
            BtlMove_Step(chr, yawMode, 6, 7, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFlag(chr, 0x89);
        BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch * lean);
        BtlMove_RequestOrbit(chr, 50.0f, 100.0f);
        BtlChar_SetFxBit(chr, 0x3A);
    }
    if (phase == PHASE_DECIDE) {
        switch (BtlAnim_GetId(chr)) {
            case 0x18:
            case 0x2D:
                break;
            case 0x19: {
                s32 stop = 0;

                if (BtlMove_IsBlockedByOpponent(chr)) {
                    if (BtlOpp_GetSeenAction(chr) != 0xD5) {
                        stop = 1;
                    }
                }
                if (Vec3_Length(&BtlChar_GetPos(chr)->unk40) < BTL_KMH(100.0f)) {
                    if (++(*slowFrames) >= 16) {
                        stop = 1;
                    }
                } else {
                    *slowFrames = 0;
                }
                if (!BtlChar_TestFlag(chr, 5)) {
                    stop = 1;
                }
                if (BtlMember_SpendKi(chr, BtlMoveParam_GetKiCost(chr, 0), 0)) {
                    stop = 1;
                }
                if (BtlInput_TestAction(chr, 0x16, 1)) {
                    BtlAnim_Request(chr, 0x15, 0.0f);
                }
                if (BtlInput_TestAction(chr, 0x15, 1)) {
                    BtlAnim_Request(chr, 0x15, 0.0f);
                    chr->work[0] |= 1;
                }
                if (BtlDecide_Main(chr, 0x08000000)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                if (stop) {
                    BtlAnim_Request(chr, 0x1A, 0.0f);
                    pose = BtlChar_GetPos(chr);
                    pose->speed *= 0.5f;
                }
                break;
            }
            case 0x1A:
                if (BtlMove_IsBlockedByOpponent(chr)) {
                    BtlChar_GetPos(chr)->speed = 0.0f;
                }
                break;
            case 0x15:
                if (BtlInput_TestAction(chr, 0x18, 1)) {
                    chr->work[0] |= 1;
                }
                if (0.5f < BtlAnim_GetProgress(chr)) {
                    if (chr->work[0] & 1) {
                        BtlAct_Request(chr, 0x1A);
                    }
                }
                if (BtlDecide_Main(chr, 0x08000000)) {
                    BtlAct_Request(chr, BtlAct_GetQueued(chr));
                }
                break;
        }
        BtlDecide_Main(chr, 0x1800);
        switch (BtlAnim_GetId(chr)) {
            case 0x19:
                BtlDecide_Attack(chr, 0x2000);
                break;
            case 0x1A:
                BtlDecide_Attack(chr, 3);
                break;
        }
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}
