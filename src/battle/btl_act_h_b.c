#include "common.h"
#include "battle/btl_act_h.h"
#ifdef PORT
struct BtlActIChr;
extern void BtlChar_ClearFlag(struct BtlActIChr *,s32);
#endif

static void spend_transform_cost(BtlActHChr *chr) {
#ifdef PORT
    if (PortGame_IsBT4()) {
        extern s32 BtlMember_DrainKi(BtlActHChr *,s32);
        extern void BtlMember_SubMaxPower(BtlActHChr *,s32);
        if (BtlChar_TestFlag(chr,6))
            BtlMember_SubMaxPower(chr,chr->form.cost/2-chr->form.cost/5);
        else BtlMember_DrainKi(chr,chr->form.cost);
        return;
    }
#endif
    BtlMember_SubBlast(chr,chr->form.cost);
}

/*
 * Fighter action handlers, second file: 0x1FC598..0x203168 (see btl_act_h.c for the overview and for how a
 * character change runs). Two parts: to 0x1FFAC0, and from there the member switch tail, the skills and the
 * decision functions (formerly btl_act_i.c, with its own header comment below and its own view of the fighter,
 * include/battle/btl_act_i.h). They are one file because two handlers of the second part match only with
 * BtlActChange_SetFlags and BtlActChange_Finish of the first part defined above them.
 */

/*
 * 0xB7, 0xB9: the thrower (animation 0x96 -> 0x98; 0xB9: 0x188 -> 0x18A). 0xB7 picks camera cut 7 or 8 by
 * BtlChar_FrameMod(2) unless attack flag 0x8000 is set. Near the stage edge and facing outward the pair is turned
 * around. Between the animation frames drainFrom..drainTo sound 0x51 and fx bit 0x38 run (a drain).
 */
s32 BtlAct_Throw(BtlActHChr *chr, s32 phase) {
    s32 alt;
    f32 radius;
    f32 dist;

    if (phase == 0) {
        alt = BtlAct_GetCurrent(chr) == 0xB9;
        BtlAnim_Play(chr, alt ? 0x188 : 0x96, chr->react.unk10 != 0 ? 0.0f : 0.3f);
        BtlChar_PlayVoice(chr, 7);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlActThrow_SetupDamage(chr, 0);
        if (!alt && !(BtlAtk_GetFlags(chr) & 0x8000)) {
            ChrCam_RequestCut(chr, 0, BtlChar_FrameMod(2) ? 7 : 8);
        }
        if (__builtin_fabsf(BtlAtk_GetLaunchAngleA(chr)) < BTL_DEG(90.0f)) {
            radius = BtlStage_GetInnerRadius();
            dist = BtlUtil_LengthXZ(&BtlChar_GetPos(chr)->pos);
            if (radius - 200.0f < dist) {
                BtlMove_TurnYaw(chr, alt ? 8 : 7, BTL_DEG(180.0f));
            }
        }
        if (BtlMember_GetActive(chr)->chara ==
#ifdef PORT
            (PortGame_IsBT4() ? 0xC1 : 0x6E)
#else
            0x6E
#endif
            && !(BtlOpp_GetParamWord0(chr) & 0x80)) {
            chr->unkE60 = BtlUtil_Clamp(chr->unkE60 + 1, 0, 10);
        }
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
        case 0x96:
            BtlAnim_AdvanceThen(chr, 0x98, 0.0f, 1);
            BtlChar_SetFlag(chr, 0x8B);
            break;
        case 0x188:
            BtlAnim_AdvanceThen(chr, 0x18A, 0.0f, 1);
            BtlChar_SetFlag(chr, 0x8B);
            break;
        case 0x98:
        case 0x18A:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAct_Request(chr, 0xB);
            }
            break;
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        if (BtlAtk_GetFlags(chr) & 0x400) {
            BtlChar_SetFlag(chr, 0x13B);
        }
        if (chr->drainFrom >= 0 && chr->drainTo >= 0) {
            if (BtlAnim_InFrameRange(chr, chr->drainFrom, chr->drainTo)) {
                BtlCharSnd_PlayCommon(chr, 0x51);
                BtlChar_SetFxBit(chr, 0x38);
            }
            if (BtlAnim_PassedFrame(chr, chr->drainTo)) {
                BtlCharSnd_PlayCommon(chr, 0x52);
            }
            if (BtlAtk_GetFlags(chr) & 0x20000000) {
                BtlChar_SetFlag(chr, 0x13C);
            }
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x8C);
        BtlChar_SetFlag(chr, 0x33);
        return;
    }
}

/*
 * 0xB8, 0xBA: the thrown fighter (animation 0x97; 0xBA: 0x189). At the end of the animation: hurt voice, the
 * action queue is filled from the reaction and the queued action starts. When flag 0x23 drops the fighter is released
 * (BtlActThrow_SetReleaseHeading).
 */
s32 BtlAct_Thrown(BtlActHChr *chr, s32 phase) {
    if (phase == 0) {
#ifdef PORT
        if (PortGame_IsBT4()) BtlChar_ClearFlag((struct BtlActIChr *)chr,0x1D);
#endif
        BtlAnim_Play(chr, BtlAct_GetCurrent(chr) == 0xBA ? 0x189 : 0x97, chr->react.unk10 != 0 ? 0.0f : 0.3f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlActThrow_TurnByRootYaw(chr);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 1)) {
            if (chr->react.silent == 0) {
                BtlChar_PlayVoice(chr, 2);
            }
            if (chr->react.back != 0) {
                BtlChar_SetFlag(chr, 0x35);
            }
            BtlAct_QueueReaction(chr, chr->react.unk4);
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
        BtlMove_Step(chr, 9, 6, 3, BTL_KMH(1000.0f), BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x97);
        BtlChar_SetFlag(chr, 0x95);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0xA);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        if (chr->react.silent == 0) {
            BtlChar_SetFlag(chr, 0x96);
        }
        BtlChar_SetFlag(chr, 0x1B);
        BtlChar_SetFxBit(chr, 0x3B);
        if (!BtlChar_TestFlag(chr, 0x23) && BtlChar_TestPrevFlag(chr, 0x23)) {
            BtlActThrow_SetReleaseHeading(chr);
            chr->work[0] |= 1;
        }
        if (chr->work[0] & 1) {
            BtlMove_SetLeanX(chr, -BtlChar_GetPos(chr)->pitch);
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x33);
        if (!(chr->work[0] & 1)) {
            BtlActThrow_SetReleaseHeading(chr);
            return;
        }
    }
}

/*
 * 0xBB: slam throw, the thrower. Animation 0x96, then the loop 0x99 while diving at 1500 km/h (+Y is down) with the
 * victim, then 0x98. The loop ends (decide phase) on flag 0xF (on the ground) or 0x16 (stage bound), when the opponent
 * is no longer in 0xBC, on flag 0x17 on stages 4 / 27, on the 91st loop frame, or from the 11th on when a stage level
 * (BtlStage_GetWaterLevel) is less than three body heights below; only characters with parameter flag 0x400 test that level.
 * Void in the original (the delay slot of the last switch only matches that way).
 */
void BtlAct_SlamThrow(BtlActHChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 rise;
    s32 end;
    f32 top;
    f32 room;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x96, chr->react.unk10 != 0 ? 0.0f : 0.3f);
        BtlActThrow_SetupDamage(chr, 1);
        BtlChar_PlayVoice(chr, 7);
        BtlChar_SetHeldFlag(chr, 0xE);
        ChrCam_RequestCut(chr, 0, 7);
    }
    if (phase == 1) {
        rise = 0;
        switch (BtlAnim_GetId(chr)) {
        case 0x96:
            BtlAnim_AdvanceThen(chr, 0x99, 0.0f, 1);
            if (BtlAnim_GetProgress(chr) > 0.9f) {
                rise = 1;
            }
            BtlChar_SetFlag(chr, 0x8B);
            break;
        case 0x99:
            BtlAnim_AdvanceLoop(chr, 0);
            if (BtlAnim_IsNew(chr)) {
                BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                BtlChar_PlayVoice(chr, 7);
                ChrCam_RequestCut(chr, 0, 8);
                BtlChar_Vibrate(chr, 0.8f, 0.3f);
            } else {
                BtlChar_SetVibration(chr, 0.7f, 0.1f);
            }
            BtlChar_SetFlag(chr, 0x8B);
            BtlChar_SetFxBit(chr, 4);
            rise = 1;
            break;
        case 0x98:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAct_Request(chr, 0xB);
            }
            break;
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        if (rise) {
            BtlMove_MoveVertical(chr, BTL_KMH(1500.0f), BTL_KMH(100.0f));
        } else {
            BtlMove_BrakeVertical(chr);
        }
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
        case 0x96:
        case 0x98:
            break;
        case 0x99:
            end = BtlChar_TestFlag(chr, 0xF) != 0;
            if (BtlChar_TestFlag(chr, 0x16)) {
                end = 1;
            }
            if (BtlOpp_GetSeenAction(chr) != 0xBC) {
                end = 1;
            }
            if (BtlChar_IsStage4Or27() && BtlChar_TestFlag(chr, 0x17)) {
                end = 1;
            }
            if (++*timer > 10) {
                if (*timer > 90) {
                    end = 1;
                }
                if (BtlParam_GetFlags(chr) & 0x400) {
                    if (BtlStage_GetWaterLevel(&top)) {
                        room = top - BtlChar_GetPos(chr)->pos.y;
                        if (room < BtlCharApi_GetHeight(chr->objId) * 3.0f) {
                            end = 1;
                        }
                    }
                }
            }
            if (end) {
                BtlAnim_Request(chr, 0x98, 0.0f);
                BtlChar_GetPos(chr)->unk9C = 0.0f;
            }
            break;
        }
    }
}

/*
 * 0xBC: slam throw, the victim. Enter: the heading is turned by the yaw of the first key of the animation's root
 * rotation track. Animation 0x97, then the loop 0x9A while diving at 1500 km/h with the thrower, then 0x9B once flag
 * 0xF (on the ground) is set without flag 0x23; at its end the cameras shake and action 0xDE starts. Leaves through
 * 0xE6 on flag 0x16, on flag 0x17 on stages 4 / 27, or when the opponent is no longer in 0xBB.
 */
s32 BtlAct_SlamThrown(BtlActHChr *chr, s32 phase) {
    BtlActHAnim *anim;
    BtlActHTrack *track;
    Quat q;
    Vec4 euler;
    s32 rise;
    f32 yaw;
    s32 count;
    u64 packed;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x97, chr->react.unk10 != 0 ? 0.0f : 0.3f);
        BtlChar_SetHeldFlag(chr, 0xE);
        anim = BtlChar_GetObj(chr)->anim;
        if (anim != NULL && anim->rootTrack != 0) {
            yaw = 0.0f;
            track = (BtlActHTrack *)((u32 *)anim + anim->rootTrack);
            count = track->count;
            if (count > 0) {
                if ((u16)(track->flags & 1)) {
                    u32 *key = track->key;

                    packed = (u64)key[0] + ((u64)key[1] << 32);
                } else {
                    u32 *key = track->key;

                    packed = (u64)key[4] + ((u64)key[5] << 32);
                }
                Quat_Unpack(&q, packed);
                Quat_ToEuler(&euler, &q);
                yaw = euler.y;
            }
            BtlMove_SetHeading(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->facing - yaw), 0.0f);
        }
    }
    if (phase == 1) {
        rise = 0;
        switch (BtlAnim_GetId(chr)) {
        case 0x97:
            BtlAnim_AdvanceThen(chr, 0x9A, 0.0f, 1);
            BtlChar_SetFlag(chr, 0x97);
            break;
        case 0x9A:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlChar_SetFlag(chr, 0x97);
            BtlChar_SetFlag(chr, 0xA);
            rise = 1;
            break;
        case 0x9B:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        BtlMove_Step(chr, 9, 6, 3, 0.0f, BTL_KMH(100.0f));
        if (rise) {
            BtlMove_MoveVertical(chr, BTL_KMH(1500.0f), 10000.0f);
        } else {
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFlag(chr, 0x95);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 0x96);
        BtlChar_SetFlag(chr, 0x1B);
        BtlChar_SetFxBit(chr, 0x3B);
    }
    if (phase == 2) {
        if (BtlAnim_GetId(chr) == 0x9A) {
            if (BtlChar_TestFlag(chr, 0x16)) {
                BtlAct_Request(chr, 0xE6);
            }
            if (BtlChar_IsStage4Or27() && BtlChar_TestFlag(chr, 0x17)) {
                BtlAct_Request(chr, 0xE6);
            }
            if (BtlOpp_GetSeenAction(chr) != 0xBB) {
                BtlAct_Request(chr, 0xE6);
            }
            if (!BtlChar_TestFlag(chr, 0x23) && BtlChar_TestFlag(chr, 0xF)) {
                BtlAnim_Request(chr, 0x9B, 0.0f);
                BtlChar_SetFlag(BtlChar_Get(BtlOpp_GetPlayer(chr)), 0x8C);
            }
        }
        if (chr->work[1] != 0) {
            if (chr->react.back != 0) {
                BtlChar_SetFlag(chr, 0x35);
            }
            BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 3.0f, 0.4f);
            BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 0.8f, 0.2f);
            BtlAct_Request(chr, 0xDE);
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x33);
    }
}

/*
 * 0xBE: drags the opponent (who is in 0xBF) down at 2000 km/h (+Y is down) in the loop 0x197, head first, with
 * camera cut 0x18 / 0x19 picked by the side the fighter camera sits on (camSide < 0: 0x18); flag 0xD3 (this camera
 * takes the whole screen) while the cut runs. When the opponent leaves 0xBF: animation 0x198, then 0x28, then 0xB.
 */
s32 BtlAct_DragDown(BtlActHChr *chr, s32 phase) {
    f32 lean;
    f32 speed;

    if (phase == 0) {
        BtlAnim_Play(chr, 0x197, 0.0f);
        ChrCam_RequestCut(chr, 1, chr->camSide < 0.0f ? 0x18 : 0x19);
    }
    if (phase == 1) {
        lean = 0.0f;
        speed = 0.0f;
        switch (BtlAnim_GetId(chr)) {
        case 0x197:
            BtlAnim_AdvanceLoop(chr, 0);
            speed = 0.0f;
            lean = 1.0f;
            BtlMove_MoveVertical(chr, BTL_KMH(2000.0f), 10000.0f);
            break;
        case 0x198:
            BtlAnim_AdvanceThen(chr, 0x28, 0.15f, 0);
            speed = 0.0f;
            lean = 1.0f - BtlAnim_GetProgress(chr);
            BtlMove_MoveVertical(chr, 0.0f, 10000.0f);
            break;
        case 0x28:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAct_Request(chr, 0xB);
            }
            speed = 0.0f;
            if (BtlAnim_GetProgress(chr) < 0.6f) {
                speed = BtlMoveParam_GetSpeed(chr, 0xB);
            }
            BtlMove_MoveVertical(chr, 0.0f, 10000.0f);
            break;
        }
        BtlMove_Step(chr, 6, 5, 4, speed, BTL_KMH(100.0f));
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlMove_SetLeanX(chr, lean * BTL_DEG(-90.0f));
        if (ChrCam_IsCutActive(chr)) {
            BtlChar_SetFlag(chr, 0xD3);
        }
    }
    if (phase == 2) {
        if (BtlAnim_GetId(chr) == 0x197 && chr->actionFrame > 0) {
            s32 dropped = BtlOpp_GetSeenAction(chr) != 0xBF;

            if (dropped) {
                BtlAnim_Request(chr, 0x198, 0.0f);
                ChrCam_EndCut(chr);
            }
        }
    }
}

/*
 * 0xBF: dragged down by the opponent (who is in 0xBE): the position follows the opponent's (target position + its
 * frame movement + 0.3 of its height), offset by this model's node 0 - node 0x10. Flag 0x1F or 0xF (on the ground)
 * ends it with 500 damage, a camera shake and action 0xDE; flags 0x16, 0xB6 (and 0x17, 0xB7 on stages 4 / 27) lead to
 * 0xE6, or 0xD9 when dead.
 */
s32 BtlAct_DraggedDown(BtlActHChr *chr, s32 phase) {
    BtlActHPose *pose = BtlChar_GetPos(chr);
    Vec4 pos;
    Vec4 node0;
    Vec4 node16;
    Vec4 offset;
    Vec4 target;
    Vec4 move;
    s32 air;
    s32 startAir;

    if (phase == 0) {
        startAir = BtlAct_IsAirMotion(chr, 0);
        if (BtlAct_GetPrev(chr) == 0xD5) {
            startAir = 1;
            if (!(BtlChar_GetPos(chr)->pitch < BTL_DEG(10.0f))) {
                startAir = 0;
            }
        }
        BtlAnim_Play(chr, startAir ? 0xC4 : 0xC3, 0.0f);
        pose->unk98 = 0.0f;
        pose->unk9C = 0.0f;
    }
    if (phase == 1) {
        air = BtlAct_IsAirMotion(chr, 0);
        BtlAnim_AdvanceLoop(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_MoveVertical(chr, BTL_KMH(2000.0f), 10000.0f);
        if (BtlOpp_GetSeenAction(chr) == 0xBE || chr->actionFrame == 0) {
            BtlOpp_GetTargetPos(chr, &target);
            BtlOpp_GetPoseVec30(chr, &move);
            Vec4_Add(&pos, &target, &move);
            pos.y += BtlOpp_GetHeight(chr) * 0.3f;
            BtlCharApi_GetNodePos(chr->objId, 0, &node0);
            BtlCharApi_GetNodePos(chr->objId, 0x10, &node16);
            Vec4_Sub(&offset, &node0, &node16);
            Vec4_Add(&pos, &pos, &offset);
            Vec4_Copy(&pose->pos, &pos);
        }
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0xA);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 0x95);
        BtlChar_SetFlag(chr, 0x96);
        BtlChar_SetFlag(chr, 0x1B);
        BtlChar_SetFxBit(chr, 0x3B);
        BtlMove_SetLeanX(chr, air ? BTL_DEG(-90.0f) : BTL_DEG(90.0f));
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x1F) || BtlChar_TestFlag(chr, 0xF)) {
            BtlAct_Request(chr, 0xDE);
            BtlMember_Damage(chr, 500, 0);
            BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 5.0f, 0.5f);
            BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 0.8f, 0.2f);
        }
        if (BtlChar_TestFlag(chr, 0x16)) {
            BtlAct_Request(chr, 0xE6);
        }
        if (BtlChar_TestFlag(chr, 0xB6)) {
            if (BtlChar_IsDead(chr)) {
                BtlAct_Request(chr, 0xD9);
            } else {
                BtlAct_Request(chr, 0xE6);
            }
        }
        if (BtlChar_IsStage4Or27()) {
            if (BtlChar_TestFlag(chr, 0x17)) {
                BtlAct_Request(chr, 0xE6);
            }
            if (BtlChar_TestFlag(chr, 0xB7)) {
                if (BtlChar_IsDead(chr)) {
                    BtlAct_Request(chr, 0xD9);
                } else {
                    BtlAct_Request(chr, 0xE6);
                }
            }
        }
    }
}

/*
 * Raised on every run frame of a character-change action (0xEC..0xF8): hit-stop level 2 (0x125, the opponent
 * freezes), 0x128 (no end-of-battle check) and the rest of the set; holds the action against forced actions.
 */
void BtlActChange_SetFlags(BtlActHChr *chr) {
    BtlChar_SetFlag(chr, 0x125);
    BtlChar_SetFlag(chr, 0x12A);
    BtlChar_SetFlag(chr, 0x42);
    BtlChar_SetFlag(chr, 0x43);
    BtlChar_SetFlag(chr, 0x44);
    BtlChar_SetFlag(chr, 0x45);
    BtlChar_SetFlag(chr, 0x46);
    BtlChar_SetFlag(chr, 0xBB);
    BtlChar_SetFlag(chr, 0x3F);
    BtlChar_SetFlag(chr, 0x128);
    BtlChar_SetFlag(chr, 0x1B);
    BtlChar_SetFlag(chr, 0x1A);
    BtlChar_SetFlag(chr, 0x136);
    chr->holdAction = 1;
}

/* Transformations 0xEC / 0xF0: a push straight behind the fighter (BtlAct_PushAngle, speed 0.4). */
void BtlActChange_PushBack(BtlActHChr *chr) {
    BtlAct_PushAngle(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y + BTL_DEG(180.0f)), 0.4f, 3.0f);
}

/*
 * Member switch: where the arriving member appears. From the player's start placement, 40 degrees up and away from
 * the opponent's, up to the stage height limit or 300 units, pulled inside the stage radius; saved as the placement
 * (taken by flag 0xF5).
 */
void BtlActSwitch_SaveEntryPlacement(BtlActHChr *chr) {
    Vec4 own;
    Vec4 opp;
    Vec4 pos;
    Vec4 rot;
    Vec4 dir;
    f32 limit;
    f32 t;
    f32 len;

    BtlStage_GetStartPlace(chr->player, &own, &dir, 0);
    BtlStage_GetStartPlace(BtlOpp_GetPlayer(chr), &opp, &dir, 0);
    limit = BtlStage_GetTop();
    dir.x = own.x - opp.x;
    dir.z = own.z - opp.z;
    t = -Mathf_Tan(BTL_DEG(40.0f));
    dir.y = t * sqrtf(dir.x * dir.x + dir.z * dir.z);
    if (Vec3_Length(&dir) < 0.001f) {
        dir.x = 0.0f;
        dir.y = -Mathf_Sin(BTL_DEG(20.0f));
        dir.z = Mathf_Cos(BTL_DEG(20.0f));
    } else {
        Vec3_Normalize(&dir, &dir);
    }
    t = (limit - own.y) / dir.y;
    if (t > 300.0f) {
        t = 300.0f;
    }
    pos.x = own.x + dir.x * t;
    pos.y = own.y + dir.y * t;
    pos.z = own.z + dir.z * t;
    rot.x = 0.0f;
    rot.y = atan2f(dir.x, dir.z);
    rot.z = 0.0f;
    t = BtlStage_GetInnerRadius() - BtlCharApi_GetRadius(chr->objId);
    len = sqrtf(pos.x * pos.x + pos.z * pos.z);
    if (t < len) {
        t /= len;
        pos.x *= t;
        pos.z *= t;
    }
    BtlChar_SetSavedPlacement(chr, &pos, &rot, &pos, 0, 0);
}

/*
 * Member switch: camera cut on the leaving member. Returns 1 (and uses a cut on a stage point instead of the
 * fighter) when the fighter is below the stage floor level or the previous action was 0xEB.
 */
s32 BtlActSwitch_SetLeaveCut(BtlActHChr *chr) {
    Vec4 pos;
    Vec4 rot;
    s32 hidden = 0;
    BtlActHPose *pose = BtlChar_GetPos(chr);
    s32 onFighter;

    onFighter = 1;
    if (BtlStage_GetBottom() < pose->pos.y) {
        onFighter = 0;
        hidden = 1;
    }
    if (BtlAct_GetPrev(chr) == 0xEB) {
        onFighter = 0;
        hidden = 1;
    }
    if (onFighter) {
        ChrCam_SetCut(chr, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, 3, 0.0f,
                      BTL_DEG(90.0f), -0.9f, -0.4f, BtlCharApi_GetHeight(chr->objId) * 1.5f, BtlCharApi_GetHeight(chr->objId) * 0.5f,
                      3, 3, 3, 0x3C, 0xC5);
    } else {
        BtlStage_GetPlace(&pos, &rot);
        pos.y -= 5.0f;
        ChrCam_SetCut(chr, &pos, &D_002EC2A0, &pos, &D_002EC2A0, &pos, &D_002EC2A0, -1, 0.0f, BTL_DEG(-45.0f), -0.2f,
                      0.0f, 100.0f, 0.0f, -1, -1, -1, 0x3C, 0xC5);
    }
    return hidden;
}

/* Member switch: camera cut on the arriving member, placed from the player's start placement. */
void BtlActSwitch_SetEnterCut(BtlActHChr *chr) {
    Vec4 pos;
    Vec4 rot;
    Vec4 a;
    Vec4 b;

    BtlStage_GetStartPlace(chr->player, &pos, &rot, 0);
    a.x = pos.x;
    a.y = pos.y - BtlCharApi_GetHeight(chr->objId) * 0.3f;
    a.z = pos.z;
    b.x = a.x;
    b.y = a.y - BtlCharApi_GetHeight(chr->objId) * 0.5f;
    b.z = a.z;
    ChrCam_SetCut(chr, &a, &D_002EC2A0, &a, &D_002EC2A0, &b, &D_002EC2A0, -1,
                  BtlUtil_WrapAngle(BtlChar_GetPos(chr)->facing + BTL_DEG(180.0f)), 0.0f, 0.0f, 0.0f,
                  BtlCharApi_GetHeight(chr->objId) + 5.0f, 0.0f, -1, -1, 0x11, 0xF, 0xC5);
}

/* Ends a change action: action 0xB, or the queued action when BtlAct_CheckStoryForced queued one. */
void BtlActChange_Finish(BtlActHChr *chr) {
    BtlAct_Request(chr, 0xB);
    if (BtlAct_CheckStoryForced(chr)) {
        BtlAct_Request(chr, BtlAct_GetQueued(chr));
    }
}

/*
 * 0xEC: transformation. Lead-in 0x177 (camera cut 4), loop 0x178 until the model is in memory, closing 0x179
 * (camera cut 6, voice 0x22). Pushes back with BtlActChange_PushBack during the lead-in and the loop.
 */
s32 BtlAct_TransformA(BtlActHChr *chr, s32 phase) {
    BtlActHPose *pose;
    s32 waiting;

    if (phase == 0) {
        pose = BtlChar_GetPos(chr);
        BtlAnim_Play(chr, 0x177, 0.0f);
        BtlAnim_PlaySub(chr, 0x178);
        pose->unk98 = 0.0f;
        pose->unk9C = 0.0f;
        BtlMove_SetHeading(chr, pose->rot.y, 0.0f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_SetFlag(chr, 0x29);
        ChrCam_RequestCut(chr, 0, 4);
        BtlChar_SavePlacement(chr);
        BtlChar_ResetLook(chr);
        BtlEvent_Raise(chr->player, 0x20);
    }
    if (phase == 1) {
        waiting = 0;
        if (chr->actionFrame == 1) {
            BtlCharSnd_PlayCommon(chr, 0x54);
            BtlChar_PlayVoice(chr, 0xE);
            BtlChar_SetFxBit(chr, 0);
            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.variant,
                                   chr->form.animChara, chr->form.unk1C, chr->form.voiceChara);
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x177:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAnim_RequestSubToMain(chr);
            }
            BtlActChange_PushBack(chr);
            waiting = 1;
            break;
        case 0x178:
            BtlAnim_AdvanceLoop(chr, 0);
            waiting = 1;
            BtlActChange_PushBack(chr);
            if (BtlChange_IsLoadedFor(chr->player)) {
                BtlAnim_Request(chr, 0x179, 0.0f);
                spend_transform_cost(chr);
                BtlChange_SetReady(chr->player);
            }
            break;
        case 0x179:
            if (BtlAnim_IsNew(chr)) {
                BtlChange_SetDone(chr->player);
                BtlChar_SetFlag(chr, 0xF5);
                BtlChar_SetFlag(chr, 0x55);
                BtlChar_SetFxBit(chr, 1);
                BtlChar_PlayVoice(chr, 0x22);
                ChrCam_RequestCut(chr, 0, 6);
                if (BtlParam_GetFlags(chr) & 0x40000) {
                    BtlCharSnd_PlayCommon(chr, 0x4C);
                } else {
                    BtlCharSnd_PlayCommon(chr, 0x53);
                }
            }
            if (BtlAnim_WasNew(chr)) {
                ChrCam_AddShake(chr, 5.0f, 0.4f);
                BtlChar_Vibrate(chr, 1.0f, 0.5f);
            }
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
                BtlEvent_Raise(chr->player, 0x5A);
                BtlChar_SetFxBit(chr, 2);
            }
            break;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlActChange_SetFlags(chr);
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFxBit(chr, 0x14);
        if (waiting && !ChrCam_IsCutActive(chr)) {
            ChrCam_RequestCut(chr, 0, 5);
        }
    }
    if (phase == 2 && chr->work[1] != 0) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        BtlEvent_Raise(chr->player, 0x4D);
        BtlChar_AddStageTimer(2.0f);
        return;
    }
}

/*
 * 0xED: transformation. The request is pushed in the enter phase. Lead-in 0x18F (camera cut 1/0), loop 0x190 until
 * the model is in memory, closing 0x191 (voice 0x22, stream sound 0x8D34).
 */
s32 BtlAct_TransformB(BtlActHChr *chr, s32 phase) {
    BtlActHPose *pose;

    if (phase == 0) {
        pose = BtlChar_GetPos(chr);
        BtlAnim_Play(chr, 0x18F, 0.0f);
        BtlAnim_PlaySub(chr, 0x190);
        pose->unk98 = 0.0f;
        pose->unk9C = 0.0f;
        BtlMove_SetHeading(chr, pose->rot.y, 0.0f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_SetFlag(chr, 0x29);
        ChrCam_RequestCut(chr, 1, 0);
        BtlChar_SavePlacement(chr);
        BtlCharSnd_PlayCommon(chr, 0x54);
        BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.variant, chr->form.animChara,
                               chr->form.unk1C, chr->form.voiceChara);
        BtlChar_ResetLook(chr);
        BtlEvent_Raise(chr->player, 0x20);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
        case 0x18F:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAnim_RequestSubToMain(chr);
            }
            break;
        case 0x190:
            BtlAnim_AdvanceLoop(chr, 0);
            if (BtlChange_IsLoadedFor(chr->player)) {
                BtlAnim_Request(chr, 0x191, 0.0f);
#ifdef PORT
                if (!PortGame_IsBT4())
#endif
                BtlMember_SubBlast(chr, chr->form.cost);
                BtlChange_SetReady(chr->player);
            }
            break;
        case 0x191:
            if (BtlAnim_IsNew(chr)) {
                BtlChange_SetDone(chr->player);
                BtlCharSnd_PlayStream(chr, 0x8D34);
                BtlChar_PlayVoice(chr, 0x22);
                BtlChar_SetFlag(chr, 0xF5);
                BtlChar_SetFlag(chr, 0x55);
            }
            if (BtlAnim_GetFrame(chr) < 3.0f) {
                BtlChar_SetFxBit(chr, 3);
            }
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
                BtlEvent_Raise(chr->player, 0x5A);
            }
            break;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlActChange_SetFlags(chr);
        BtlChar_SetFxBit(chr, 0x14);
    }
    if (phase == 2 && chr->work[1] != 0) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        BtlEvent_Raise(chr->player, 0x4D);
        return;
    }
}

/*
 * 0xEE: transformation without a waiting loop. Lead-in 0x177 holds on its last frame until the model is in memory,
 * then the closing 0x179 (voice 0x22, stream sound 0x8D36). Gravity applies; the placement is taken (flag 0xF5) on
 * leaving unless a warp flag (0xFC / 0xFD) is set.
 */
s32 BtlAct_TransformC(BtlActHChr *chr, s32 phase) {
    s32 lead = 0x177;
#ifdef PORT
    if (PortGame_IsBT4()) lead=0x1A4;
#endif
    if (phase == 0) {
        BtlAnim_Play(chr, lead, 0.0f);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        BtlMove_SetHeading(chr, BtlChar_GetPos(chr)->rot.y, 0.0f);
        BtlChar_SavePlacement(chr);
        BtlChar_SetFlag(chr, 0xF4);
        BtlChar_SetFlag(chr, 0xD7);
        BtlChar_ResetLook(chr);
        BtlEvent_Raise(chr->player, 0x20);
    }
    if (phase == 1) {
        if (chr->actionFrame == 1) {
            BtlCharSnd_PlayCommon(chr, 0x54);
            BtlChar_SetFxBit(chr, 0);
            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.variant,
                                   chr->form.animChara, chr->form.unk1C, chr->form.voiceChara);
        }
        switch (BtlAnim_GetId(chr)==lead ? 0x177 : BtlAnim_GetId(chr)) {
        case 0x177:
            if (BtlAnim_Advance(chr, 0)) {
                if (BtlChange_IsLoadedFor(chr->player)) {
                    BtlAnim_Request(chr, 0x179, 0.0f);
                    spend_transform_cost(chr);
                    BtlChange_SetReady(chr->player);
                }
            }
            BtlChar_SetFxBit(chr, 0x11);
            BtlChar_SetFlag(chr, 0x12F);
            break;
        case 0x179:
            if (BtlAnim_IsNew(chr)) {
                BtlChange_SetDone(chr->player);
                BtlChar_SetFxBit(chr, 1);
                BtlChar_PlayVoice(chr, 0x22);
                BtlChar_SetFlag(chr, 0xD9);
                BtlChar_Vibrate(chr, 1.0f, 0.5f);
                BtlCharSnd_PlayStream(chr, 0x8D36);
            }
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
                BtlEvent_Raise(chr->player, 0x5A);
                BtlChar_SetFxBit(chr, 2);
            }
            break;
        }
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
        BtlChar_SetFxBit(chr, 0x14);
        if (chr->form.kind == 4) {
            BtlChar_SetFlag(chr, 0x132);
        }
        if (chr->form.kind == 3) {
            BtlChar_SetFlag(chr, 0x133);
        }
    }
    if (phase == 2 && chr->work[1] != 0) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        if (!BtlChar_TestFlag(chr, 0xFC) && !BtlChar_TestFlag(chr, 0xFD)) {
            BtlChar_SetFlag(chr, 0xF5);
            BtlChar_SetFlag(chr, 0x55);
        }
        BtlChar_SetFlag(chr, 0xDA);
        BtlEvent_Raise(chr->player, 0x4D);
        BtlChar_AddStageTimer(2.0f);
        return;
    }
}

/*
 * 0xEF: transformation with an extra object (two requests: the object, then the character). Lead-in 0x2B while
 * rising at 800 km/h, loop 0x17E for at least 61 frames and until the object is in memory, then 0x177 with the object's
 * animation 0x1A2 until the character is in memory, then the closing 0x179.
 */
s32 BtlAct_TransformD(BtlActHChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    BtlActHPose *pose;

    if (phase == 0) {
        pose = BtlChar_GetPos(chr);
        BtlAnim_Play(chr, 0x2B, 0.0f);
        BtlAnim_PlaySub(chr, 0x17E);
        pose->unk98 = 0.0f;
        pose->unk9C = 0.0f;
        BtlMove_SetHeading(chr, pose->rot.y, 0.0f);
        ChrCam_RequestCut(chr, 0, 4);
        BtlChar_SavePlacement(chr);
        BtlChar_ResetLook(chr);
        BtlEvent_Raise(chr->player, 0x20);
    }
    if (phase == 1) {
        if (chr->actionFrame == 1) {
            BtlCharSnd_PlayCommon(chr, 0x18);
            BtlChange_RequestObject(chr->player, chr->form.objId, chr->form.objCostume, 0, 2);
            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.variant,
                                   chr->form.animChara, chr->form.unk1C, chr->form.voiceChara);
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x2B:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAnim_RequestSubToMain(chr);
            }
            BtlMove_MoveVertical(chr, -BTL_KMH(800.0f), BTL_KMH(100.0f));
            BtlChar_SetFlag(chr, 0x18);
            break;
        case 0x17E:
            if (BtlAnim_IsNew(chr)) {
                ChrCam_RequestCut(chr, 0, 5);
                BtlChar_PlayVoice(chr, 0x26);
                BtlChar_SetFlag(chr, 0xF7);
            }
            BtlAnim_AdvanceLoop(chr, 0);
            BtlMove_MoveVertical(chr, 0.0f, 10000.0f);
            if (++*timer > 60 && BtlChange_IsLoadedFor(chr->player)) {
                BtlAnim_Request(chr, 0x177, 0.0f);
                BtlChange_SetReady(chr->player);
            }
            BtlChar_SetFxBit(chr, 0x14);
            break;
        case 0x177:
            if (BtlAnim_IsNew(chr)) {
                BtlChange_SetDone(chr->player);
                BtlPartner_PlayAnim(chr, 0x1A2);
                BtlChar_SetFlag(chr, 0xF4);
                BtlChar_SetFlag(chr, 0xD7);
                BtlChar_SetFxBit(chr, 0);
            }
            if (BtlAnim_Advance(chr, 0) && BtlChange_IsLoadedFor(chr->player)) {
                BtlAnim_Request(chr, 0x179, 0.0f);
                spend_transform_cost(chr);
                BtlChange_SetReady(chr->player);
            }
            BtlPartner_StepAnim(chr);
            BtlMove_BrakeVertical(chr);
            BtlChar_SetFxBit(chr, 0x14);
            BtlChar_SetFlag(chr, 0x12F);
            break;
        case 0x179:
            if (BtlAnim_IsNew(chr)) {
                BtlChar_SetFlag(chr, 0xF4);
                BtlPartner_Release(chr);
                BtlChar_SetFlag(chr, 0xD9);
                BtlCharSnd_PlayStream(chr, 0x8D36);
                BtlChar_SetFxBit(chr, 1);
            }
            if (BtlAnim_PassedRatio(chr, 0.3f)) {
                BtlChar_PlayVoice(chr, 0x22);
            }
            if (BtlAnim_Advance(chr, 0)) {
                BtlChange_SetDone(chr->player);
                BtlChar_SetFxBit(chr, 2);
                chr->work[1] = 1;
                BtlEvent_Raise(chr->player, 0x5A);
            }
            BtlMove_BrakeVertical(chr);
            break;
        }
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
    }
    if (phase == 2 && chr->work[1] != 0) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        if (!BtlChar_TestFlag(chr, 0xFC) && !BtlChar_TestFlag(chr, 0xFD)) {
            BtlChar_SetFlag(chr, 0xF5);
            BtlChar_SetFlag(chr, 0x55);
        }
        BtlChar_SetFlag(chr, 0xDA);
        ChrCam_EndCut(chr);
        BtlChar_SetFlag(chr, 0xCD);
        BtlEvent_Raise(chr->player, 0x4D);
        BtlChar_AddStageTimer(2.0f);
        return;
    }
}

/*
 * 0xF0: transformation, the long presentation. Lead-in 0x177 with three sounds, loop 0x178 until the model is in
 * memory, closing 0x179 with sounds, camera shakes and vibration at fixed points of the animation.
 */
s32 BtlAct_TransformE(BtlActHChr *chr, s32 phase) {
    BtlActHPose *pose;
    s32 waiting;

    if (phase == 0) {
        pose = BtlChar_GetPos(chr);
        BtlAnim_Play(chr, 0x177, 0.0f);
        BtlAnim_PlaySub(chr, 0x178);
        pose->unk98 = 0.0f;
        pose->unk9C = 0.0f;
        BtlMove_SetHeading(chr, pose->rot.y, 0.0f);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_SetFlag(chr, 0x29);
        ChrCam_RequestCut(chr, 0, 4);
        BtlChar_SavePlacement(chr);
        BtlChar_ResetLook(chr);
        BtlEvent_Raise(chr->player, 0x20);
    }
    if (phase == 1) {
        waiting = 0;
        if (chr->actionFrame == 1) {
            BtlChar_PlayVoice(chr, 0xE);
            BtlChar_SetFxBit(chr, 0);
            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.variant,
                                   chr->form.animChara, chr->form.unk1C, chr->form.voiceChara);
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x177:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAnim_RequestSubToMain(chr);
            }
            waiting = 1;
            if (BtlAnim_PassedRatio(chr, 0.4f)) {
                BtlCharSnd_PlayCommon(chr, 0x44);
            }
            if (BtlAnim_PassedRatio(chr, 0.5f)) {
                BtlCharSnd_PlayCommon(chr, 0x22);
            }
            if (BtlAnim_PassedRatio(chr, 0.8f)) {
                BtlCharSnd_PlayCommon(chr, 0x18);
            }
            break;
        case 0x178:
            BtlAnim_AdvanceLoop(chr, 0);
            waiting = 1;
            if (BtlChange_IsLoadedFor(chr->player)) {
                BtlAnim_Request(chr, 0x179, 0.0f);
                spend_transform_cost(chr);
                BtlChange_SetReady(chr->player);
            }
            break;
        case 0x179:
            if (BtlAnim_IsNew(chr)) {
                BtlChange_SetDone(chr->player);
                BtlChar_SetFlag(chr, 0xF5);
                BtlChar_SetFlag(chr, 0x55);
                BtlChar_SetFxBit(chr, 1);
                BtlChar_PlayVoice(chr, 0x22);
                ChrCam_RequestCut(chr, 0, 6);
            }
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
                BtlEvent_Raise(chr->player, 0x5A);
                BtlChar_SetFxBit(chr, 2);
            }
            if (BtlAnim_PassedRatio(chr, 0.12f)) {
                if (BtlAct_GetHeight(chr) < 5.0f) {
                    BtlCharSnd_PlayCommon(chr, 0x25);
                    ChrCam_AddShake(chr, 5.0f, 0.2f);
                    BtlChar_Vibrate(chr, 1.0f, 0.15f);
                }
            }
            if (BtlAnim_PassedRatio(chr, 0.35f)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                ChrCam_AddShake(chr, 5.0f, 0.2f);
                BtlChar_Vibrate(chr, 1.0f, 0.15f);
            }
            if (BtlAnim_PassedRatio(chr, 0.63f)) {
                BtlCharSnd_PlayCommon(chr, 0x45);
                ChrCam_AddShake(chr, 5.0f, 0.1f);
                BtlChar_Vibrate(chr, 1.0f, 0.1f);
            }
            if (BtlAnim_PassedRatio(chr, 0.75f)) {
                BtlCharSnd_PlayCommon(chr, 0x2C);
                BtlChar_SetFxBit(chr, 0x18);
            }
            break;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlActChange_SetFlags(chr);
        BtlChar_SetFxBit(chr, 0x14);
        BtlChar_SetFxBit(chr, 0x3A);
        if (waiting && !ChrCam_IsCutActive(chr)) {
            ChrCam_RequestCut(chr, 0, 5);
        }
    }
    if (phase == 2 && chr->work[1] != 0) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        BtlEvent_Raise(chr->player, 0x4D);
        BtlChar_AddStageTimer(2.0f);
        return;
    }
}

/*
 * 0xF1, 0xF2: fusion (two requests: the partner as an extra object, then the fused character). Animation 0x1F -> 0x20
 * while rising at 1000 km/h (from the middle of 0x1F); 0x20 holds on its last frame until the partner object is in memory; then the two-fighter
 * animation 0x1A2 (0xF1) / 0x1A3 (0xF2), which holds on its last frame until the fused character is in memory; then
 * the closing 0x1A4 (voice 0x22 at 30%).
 */
s32 BtlAct_Fusion(BtlActHChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0x1F, 0.0f);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        BtlMove_SetHeading(chr, BtlChar_GetPos(chr)->rot.y, 0.0f);
        BtlChar_SavePlacement(chr);
        ChrCam_RequestCut(chr, 1, 1);
        BtlChar_ResetLook(chr);
        BtlEvent_Raise(chr->player, 0x20);
    }
    if (phase == 1) {
        if (chr->actionFrame == 1) {
            BtlCharSnd_PlayCommon(chr, 0x31);
            BtlChange_RequestObject(chr->player, chr->form.objId, chr->form.objCostume, chr->form.objVariant, 2);
            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.variant,
                                   chr->form.animChara, chr->form.unk1C, chr->form.voiceChara);
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x1F:
            BtlAnim_AdvanceThen(chr, 0x20, 0.0f, 0);
            if (BtlAnim_GetProgress(chr) < 0.5f) {
                BtlMove_MoveVertical(chr, 0.0f, 10000.0f);
            } else {
                BtlMove_MoveVertical(chr, -BTL_KMH(1000.0f), 10000.0f);
            }
            break;
        case 0x20:
            if (BtlAnim_Advance(chr, 0)) {
                BtlMove_MoveVertical(chr, 0.0f, 10000.0f);
                if (BtlChange_IsLoadedFor(chr->player)) {
                    if (BtlAct_GetCurrent(chr) == 0xF1) {
                        BtlAnim_Request(chr, 0x1A2, 0.0f);
                    } else {
                        BtlAnim_Request(chr, 0x1A3, 0.0f);
                    }
                    BtlChar_SetFlag(chr, 0xB);
                    BtlChange_SetReady(chr->player);
                }
            } else {
                BtlMove_MoveVertical(chr, -BTL_KMH(1000.0f), 10000.0f);
            }
            BtlChar_SetFlag(chr, 0x18);
            break;
        case 0x1A2:
        case 0x1A3:
            if (BtlAnim_IsNew(chr)) {
                BtlChange_SetDone(chr->player);
                BtlPartner_PlayAnim(chr, BtlAnim_GetId(chr));
                if (BtlMember_HasAbilityOf(chr, chr->form.partner, 0x79)) {
                    BtlPartner_SetFlag80(chr, 1);
                }
                BtlChar_SetFxBit(chr, 0);
                BtlChar_SetFlag(chr, 0xF4);
                if (BtlAct_GetCurrent(chr) == 0xF1) {
                    BtlChar_SetFlag(chr, 0xD7);
                } else {
                    BtlChar_SetFlag(chr, 0xD8);
                }
            }
            if (BtlAct_GetCurrent(chr) == 0xF1) {
                BtlChar_SetFlag(chr, 0x130);
            } else {
                BtlChar_SetFlag(chr, 0x131);
            }
            if (BtlAnim_Advance(chr, 0) && BtlChange_IsLoadedFor(chr->player)) {
                BtlAnim_Request(chr, 0x1A4, 0.0f);
                BtlMember_SubBlast(chr, chr->form.cost);
                BtlChange_SetReady(chr->player);
            }
            BtlPartner_StepAnim(chr);
            BtlMove_BrakeVertical(chr);
            BtlChar_SetFxBit(chr, 0x14);
            break;
        case 0x1A4:
            if (BtlAnim_IsNew(chr)) {
                BtlChar_SetFlag(chr, 0xF4);
                BtlPartner_Release(chr);
                BtlChar_SetFlag(chr, 0xD9);
                BtlChar_SetFxBit(chr, 1);
                BtlChar_Vibrate(chr, 1.0f, 0.5f);
                if (BtlAct_GetCurrent(chr) == 0xF1) {
                    BtlCharSnd_PlayStream(chr, 0x8D36);
                } else {
                    BtlCharSnd_PlayStream(chr, 0x8D38);
                }
            }
            if (BtlAnim_PassedRatio(chr, 0.3f)) {
                BtlChar_PlayVoice(chr, 0x22);
            }
            if (BtlAnim_Advance(chr, 0)) {
                BtlChange_SetDone(chr->player);
                BtlChar_SetFxBit(chr, 2);
                chr->work[1] = 1;
                BtlEvent_Raise(chr->player, 0x5A);
            }
            BtlMove_BrakeVertical(chr);
            break;
        }
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
    }
    if (phase == 2 && chr->work[1] != 0) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        if (!BtlChar_TestFlag(chr, 0xFC) && !BtlChar_TestFlag(chr, 0xFD)) {
            BtlChar_SetFlag(chr, 0xF5);
            BtlChar_SetFlag(chr, 0x55);
        }
        BtlChar_SetFlag(chr, 0xDA);
        ChrCam_EndCut(chr);
        BtlChar_SetFlag(chr, 0xCD);
        BtlEvent_Raise(chr->player, 0x4D);
        BtlChar_AddStageTimer(4.0f);
        return;
    }
}

/*
 * 0xF3: member switch, the leaving member. Animation 0xEE (falling), then the loop 0x2F: rises at 1000 km/h for 14
 * frames, fx bit 0xC and sound 0x20 on frame 15, hidden (flag 0xB) from frame 15; from frame 46 on, as soon as the
 * next member's files are in memory: BtlChange_SetReady and action 0xF4.
 */
s32 BtlAct_SwitchLeave(BtlActHChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0xEE, 0.0f);
        BtlAnim_PlaySub(chr, 0x2F);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        BtlMove_SetHeading(chr, BtlChar_GetPos(chr)->rot.y, 0.0f);
        BtlChar_SavePlacement(chr);
        BtlChar_SetFlag(chr, 0xF4);
        BtlChar_ResetLook(chr);
        ChrCam_RequestCut(chr, 1, 2);
    }
    if (phase == 1) {
        if (chr->actionFrame == 1) {
            BtlChar_PlayVoice(chr, 0x10);
            BtlChange_RequestChara(chr->player, chr->switchChara, chr->switchCostume, chr->switchVariant,
                                   chr->switchAnimChara, chr->switchUnk18, chr->switchVoiceChara);
        }
        switch (BtlAnim_GetId(chr)) {
        case 0xEE:
            if (BtlAnim_Advance(chr, 0)) {
                BtlChar_SetFlag(chr, 0x33);
                BtlAnim_RequestSubToMain(chr);
                BtlAnim_SetBlend(chr, 0.15f);
                BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
            }
            BtlMove_Fall(chr);
            break;
        case 0x2F:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlChar_SetFlag(chr, 0x18);
            BtlChar_SetFxBit(chr, 4);
            BtlChar_SetFxBit(chr, 0x3A);
            if (++*timer < 15) {
                BtlMove_MoveVertical(chr, -BTL_KMH(1000.0f), 10000.0f);
            } else {
                BtlMove_BrakeVertical(chr);
            }
            if (*timer == 15) {
                BtlChar_SetFxBit(chr, 0xC);
                BtlCharSnd_PlayCommon(chr, 0x20);
            }
            if (*timer >= 16) {
                BtlChar_SetFlag(chr, 0xB);
            }
            if (*timer >= 46 && BtlChange_IsLoadedFor(chr->player)) {
                BtlAct_Request(chr, 0xF4);
                BtlChange_SetReady(chr->player);
                BtlChar_SetFxBit(chr, 5);
                *timer = 0;
            }
            break;
        }
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
        BtlChar_SetFlag(chr, 0x134);
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly btl_act_i.c), written against its own view of the fighter. Functions already declared above with
 * the first part's types are reached through cast macros.
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/btl_act_i.h"

/*
 * Fighter action handlers and input decision functions: 0x1FFAC0..0x203168.
 *
 *   0x1FFAC0..0x200408  the tail of the member switch chains (actions 0xF4..0xF8) and the round reset action 0xF9
 *   0x200408..0x200B60  skills ("blast 1", the two skill slots of a character): BtlSkill_Apply and its helpers
 *   0x200B60..0x2012D8  the three skill actions 0xFD..0x102
 *   0x2012D8..0x203168  the DECISION functions: what a free fighter does for each input
 *
 * This is a slice of a larger original source file: BtlAct_SwitchArriveLand and BtlAct_KoSwitchFlyIn match only when
 * BtlActChange_SetFlags (0x1FD958) and BtlActChange_Finish (0x1FDF50), in the first part of this file, are defined
 * earlier in the same translation unit (see the notes at those two functions). The float pool of this slice is
 * 0x2FE008..0x2FE070 and its jump tables are 0x2F0FD0..0x2F11C8.
 *
 * Handlers are `s32 handler(chr, phase)` that fall off their end; where the original ends a leave phase with a
 * tail call, the source has an explicit `return;` (the only form found that reproduces it).
 *
 * The decision tables are written out in include/battle/btl_act_i.h.
 */

/* Speeds are km/h converted to units per frame (see btl_char_move.c). */
#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))

/* Not the libc prototype on purpose: with the real one the compiler expands the 8-byte clears inline, and the
 * original calls memset. */
extern void *memset(void *dst, s32 c, s32 n);

extern BtlActIRoster *gBtlChars;

/* Fighter accessors, flags, effects, sound. */
#define BtlChar_GetPos ((BtlActIPose *(*)(BtlActIChr *chr))BtlChar_GetPos)
#define BtlChar_GetObj ((BtlActIObj *(*)(BtlActIChr *chr))BtlChar_GetObj)
extern s32 BtlChar_IsFree(BtlActIChr *chr);
#define BtlChar_IsDead ((s32 (*)(BtlActIChr *chr))BtlChar_IsDead)
extern s32 BtlChar_TestMemberUnk70(BtlActIChr *chr);
extern s32 BtlChar_FrameMod(s32 n);
#define BtlChar_TestFlag ((s32 (*)(BtlActIChr *chr, s32 n))BtlChar_TestFlag)
#define BtlChar_TestPrevFlag ((s32 (*)(BtlActIChr *chr, s32 n))BtlChar_TestPrevFlag)
extern s32 BtlChar_IsFlagRaised(BtlActIChr *chr, s32 n);
#define BtlChar_SetFlag ((void (*)(BtlActIChr *chr, s32 n))BtlChar_SetFlag)
#define BtlChar_SetHeldFlag ((void (*)(BtlActIChr *chr, s32 n))BtlChar_SetHeldFlag)
extern void BtlChar_ClearFlag(BtlActIChr *chr, s32 n);
#define BtlChar_SetFxBit ((void (*)(BtlActIChr *chr, s32 n))BtlChar_SetFxBit)
#define BtlChar_PlayVoice ((void (*)(BtlActIChr *chr, s32 kind))BtlChar_PlayVoice)
#define BtlChar_Vibrate ((void (*)(BtlActIChr *chr, f32 power, f32 seconds))BtlChar_Vibrate)
extern void BtlChar_SetSmallVibration(BtlActIChr *chr, f32 seconds);
extern void BtlChar_AddStageTimer(f32 seconds);
#define BtlCharSnd_PlayCommon ((void (*)(BtlActIChr *chr, s32 id))BtlCharSnd_PlayCommon)
extern void BtlEvent_Raise(s32 side, s32 ev);
#define ChrCam_RequestCut ((void (*)(BtlActIChr *chr, s32 arg1, s32 arg2))ChrCam_RequestCut)
#define ChrCam_AddShake ((void (*)(BtlActIChr *chr, f32 strength, f32 time))ChrCam_AddShake)
extern void BtlColl_AddActionBit(BtlActIChr *chr, s32 action);
#define BtlOpp_GetSeenAction ((s32 (*)(BtlActIChr *chr))BtlOpp_GetSeenAction)
extern f32 BtlOpp_GetYawFromFacing(BtlActIChr *chr);
extern s32 BtlInput_TestAction(BtlActIChr *chr, s32 id, s32 want);
extern s32 BtlUtil_Min(s32 a, s32 b);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);

/* Animation. */
#define BtlAnim_Play ((void (*)(BtlActIChr *chr, s32 anim, f32 blend))BtlAnim_Play)
#define BtlAnim_Request ((void (*)(BtlActIChr *chr, s32 anim, f32 blend))BtlAnim_Request)
#define BtlAnim_GetId ((s32 (*)(BtlActIChr *chr))BtlAnim_GetId)
#define BtlAnim_GetProgress ((f32 (*)(BtlActIChr *chr))BtlAnim_GetProgress)
#define BtlAnim_Advance ((s32 (*)(BtlActIChr *chr, s32 flags))BtlAnim_Advance)
#define BtlAnim_AdvanceThen ((s32 (*)(BtlActIChr *chr, s32 next, f32 blend, s32 flags))BtlAnim_AdvanceThen)
#define BtlAnim_AdvanceLoop ((void (*)(BtlActIChr *chr, s32 flags))BtlAnim_AdvanceLoop)
#define BtlAnim_PassedRatio ((s32 (*)(BtlActIChr *chr, f32 ratio))BtlAnim_PassedRatio)

/* Movement. */
#define BtlMove_Step ((void (*)(BtlActIChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel))BtlMove_Step)
#define BtlMove_BrakeVertical ((void (*)(BtlActIChr *chr))BtlMove_BrakeVertical)
#define BtlMove_ApplyGravity ((void (*)(BtlActIChr *chr))BtlMove_ApplyGravity)
extern void BtlMove_TurnToPoint(BtlActIChr *chr, Vec4 *target, f32 yawStep, f32 pitchStep);
extern void BtlMove_TurnModelYaw(BtlActIChr *chr, f32 maxStep, f32 rate);
#define BtlMove_TurnYaw ((void (*)(BtlActIChr *chr, s32 mode, f32 maxStep))BtlMove_TurnYaw)
extern void BtlMove_TurnPitch(BtlActIChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_SnapToOpponent(BtlActIChr *chr);
extern void BtlMove_WarpBehindOpponent(BtlActIChr *chr, f32 gap);
extern void BtlMove_SetDirection(BtlActIChr *chr, s32 mode);
extern s32 BtlMove_IsBlastIncoming(BtlActIChr *chr, s32 a, s32 b, s32 c, f32 limit);
extern s32 BtlMove_CanFireBlast(BtlActIChr *chr, s32 mode, s32 *outCount);

/* Action state machine core (btl_char_action.c). */
#define BtlAct_Request ((void (*)(BtlActIChr *chr, s32 id))BtlAct_Request)
extern void BtlAct_SetQueue(BtlActIChr *chr, u32 slot, s32 id);
#define BtlAct_GetQueued ((s32 (*)(BtlActIChr *chr))BtlAct_GetQueued)
#define BtlAct_GetCurrent ((s32 (*)(BtlActIChr *chr))BtlAct_GetCurrent)
extern s32 BtlAct_GetCurrentClass(BtlActIChr *chr);
extern void BtlAct_PrepareSwitch(BtlActIChr *chr);
extern s32 BtlAct_IsAttackId(s32 id);
extern s32 BtlAct_TestAttackSkill(BtlActIChr *chr, s32 attack);
extern void BtlAct_PrepareAttack(BtlActIChr *chr, s32 id);
extern s32 BtlAct_TestPoweredSkill(BtlActIChr *chr, u32 mask);

/* Character change (btl_act_h.c, btl_char_ctl_b.c). */
#define BtlActChange_SetFlags ((void (*)(BtlActIChr *chr))BtlActChange_SetFlags)          /* the flag set of a change action; holds the action */
#define BtlActChange_Finish ((void (*)(BtlActIChr *chr))BtlActChange_Finish)            /* request 0xB, or the action a story script forces */
#define BtlActSwitch_SaveEntryPlacement ((void (*)(BtlActIChr *chr))BtlActSwitch_SaveEntryPlacement) /* entry point of the arriving member -> saved placement */
#define BtlActSwitch_SetLeaveCut ((s32 (*)(BtlActIChr *chr))BtlActSwitch_SetLeaveCut)        /* camera cut; 1 = below the stage floor or after action 0xEB */
#define BtlActSwitch_SetEnterCut ((void (*)(BtlActIChr *chr))BtlActSwitch_SetEnterCut)
extern void BtlChange_RequestChara(s32 player, s32 chara, s32 costume, s32 variant, s32 animChara, s32 unk18, s32 voiceChara);
extern s32 BtlChange_IsLoadedFor(s32 player);
extern void BtlChange_SetReady(s32 player);
extern void BtlChange_SetDone(s32 player);
extern s32 BtlChars_IsTimeStopped(void);
extern void BtlStage_GetStartPlace(s32 player, Vec4 *pos, Vec4 *rot, s32 arg3); /* start placement of a player (stage side) */
extern void BtlObj_SetColorMode(BtlActIObj *obj, s32 arg1, s32 arg2);        /* battle object: called for object flag 0x40000 */

/* Gauges and stat modifiers. */
extern BtlActIGauge *BtlMember_GetActiveGauge(BtlActIChr *chr);
extern s32 BtlMember_HasHealth(BtlActIChr *chr, s32 amount);
extern s32 BtlMember_HasKi(BtlActIChr *chr, s32 amount);
extern s32 BtlMember_HasBlast(BtlActIChr *chr, s32 amount);
extern s32 BtlMember_IsKiFull(BtlActIChr *chr);
extern void BtlMember_AddMaxPower(BtlActIChr *chr, s32 amount);
extern void BtlMember_AddHealth(BtlActIChr *chr, s32 amount);
#define BtlMember_Damage ((void (*)(BtlActIChr *chr, s32 damage, s32 flags))BtlMember_Damage)
extern s32 BtlMember_AddKi(BtlActIChr *chr, s32 amount);
extern s32 BtlMember_SpendKi(BtlActIChr *chr, s32 amount, s32 force);
#define BtlMember_SubBlast ((void (*)(BtlActIChr *chr, s32 amount))BtlMember_SubBlast)
extern void BtlStat_SetBase(BtlActIChr *chr, s32 stat, s32 slot, s32 val, s32 add);
extern void BtlStat_SetTimed(BtlActIChr *chr, s32 stat, s32 slot, s32 val, s32 add, s32 time);
extern void BtlStat_SetKind2(BtlActIChr *chr, s32 stat, s32 slot, s32 val, s32 add);
extern void BtlStat_SetKind3(BtlActIChr *chr, s32 stat, s32 slot, s32 val, s32 add);
extern void BtlStat_SetKind4(BtlActIChr *chr, s32 stat, s32 slot, s32 val, s32 add);
extern void BtlStat_SetPenalty(BtlActIChr *chr, s32 stat, s32 slot, s32 val);

/* Character parameters (btl_tech_a.c / btl_tech_b.c). */
extern s32 BtlParam_GetUnk0(BtlActIChr *chr);
#define BtlParam_GetFlags ((s32 (*)(BtlActIChr *chr))BtlParam_GetFlags)
extern s32 BtlParam_GetFlags2(BtlActIChr *chr);
extern s32 BtlParam_GetFlags3(BtlActIChr *chr);
extern s32 BtlParam_GetAmountA(BtlActIChr *chr);   /* ki cost: 10000; 5000 with ability 0x23; 0 with ability 0x24 */
extern s32 BtlParam_GetAmountB(BtlActIChr *chr);   /* ki cost: 20000; 10000; 0 */
extern s32 BtlParam_GetCount80(BtlActIChr *chr);   /* how many ki blasts can be fired in a row */
extern s32 BtlParam_GetUnk84(BtlActIChr *chr, s32 n);
extern s32 BtlParam_GetUnk88(BtlActIChr *chr, s32 n);
extern s32 BtlParam_GetUnk8A(BtlActIChr *chr, s32 n);
#define BtlParam_GetDashSound ((s32 (*)(BtlActIChr *chr))BtlParam_GetDashSound)
extern s32 BtlParam_CanFly(BtlActIChr *chr);
extern s32 BtlKiBlast_GetKiCostOf(BtlActIChr *chr, s32 kind);

/* Skill slot parameters: the character parameter block (object +0x930), by slot 0 / 1. */
extern s32 BtlSkill_GetFlags(BtlActIChr *chr, s32 slot);        /* +0x08 + slot * 4: attribute bits */
extern s32 BtlSkill_GetId(BtlActIChr *chr, s32 slot);           /* +0x10 + slot * 2 (s16): skill id */
extern s32 BtlSkill_GetStatKind(BtlActIChr *chr, s32 slot);     /* +0x66 + slot (s8): BTL_STAT_KIND_* */
extern s32 BtlSkill_GetBlastCost(BtlActIChr *chr, s32 slot);    /* max(+0x94[slot] (- 1 with ability 0x15), 1) * 100000 */
extern s32 BtlSkill_GetStatLevel0(BtlActIChr *chr, s32 slot);   /* +0x96 + slot (s8) */
extern s32 BtlSkill_GetStatLevel1(BtlActIChr *chr, s32 slot);   /* +0x9A + slot (s8) */
extern s32 BtlSkill_GetStatLevel2(BtlActIChr *chr, s32 slot);   /* +0x98 + slot (s8) */
extern s32 BtlSkill_GetStatLevel3(BtlActIChr *chr, s32 slot);   /* +0x9C + slot (s8) */
extern s32 BtlSkill_GetFrames(BtlActIChr *chr, s32 slot);       /* +0xA0 + slot * 4 (f32 seconds) * 30 */
extern s32 BtlSkill_GetHealthChange(BtlActIChr *chr, s32 slot); /* health maximum * (+0xA8 + slot * 4) / 100 */
extern s32 BtlSkill_GetKiChange(BtlActIChr *chr, s32 slot);     /* +0xB0 + slot * 4 */
extern s32 BtlSkill_GetValF(BtlActIChr *chr, s32 slot);         /* +0xB8 + slot * 4 */
extern s32 BtlSkill_GetValE(BtlActIChr *chr, s32 slot);         /* +0xC0 + slot * 4 */

/* The rest of the decision code (btl_act_j.c). */
extern s32 BtlAct_QueueComboFinish(BtlActIChr *chr);
extern s32 BtlAct_CheckTransformInput(BtlActIChr *chr);
extern s32 BtlAct_CheckFusionInput(BtlActIChr *chr);
extern s32 BtlAct_CheckSwitchInput(BtlActIChr *chr);
extern s32 BtlAct_CheckTechniqueInput(BtlActIChr *chr);
extern s32 BtlAct_CheckSkillInput(BtlActIChr *chr);
extern s32 BtlAct_GetAction14or16(BtlActIChr *chr);

/* 0xF4 (after 0xF3, the member switch by input): first action on the new member's model, one hidden frame; then 0xF5. */
s32 BtlAct_SwitchArriveWait(BtlActIChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0, 0.0f);
        BtlChange_SetDone(chr->player);
    }
    if (phase == 1) {
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
        BtlChar_SetFlag(chr, 0x134);
        BtlChar_SetFlag(chr, 0xB);
        BtlChar_SetFlag(chr, 0xCC);
    }
    if (phase == 2) {
        BtlAct_Request(chr, 0xF5);
    }
}

/*
 * 0xF5: the new member lands (animation 0x2D, held for 8 frames, then 0) and control returns once work[1] is set.
 *
 * Matches only with BtlActChange_SetFlags and BtlActChange_Finish DEFINED earlier in the same translation unit:
 * with declarations alone one instruction differs (0x1FFD2C `beqz` comes out as `beqzl`).
 */
s32 BtlAct_SwitchArriveLand(BtlActIChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0x2D, 0.0f);
        BtlChar_SetFlag(chr, 0xF4);
        ChrCam_RequestCut(chr, 1, 3);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
        case 0x2D:
            if (++*timer >= 8) {
                if (BtlAnim_AdvanceThen(chr, 0, 0.15f, 0)) {
                    BtlChar_SetFxBit(chr, 0xD);
                    BtlCharSnd_PlayCommon(chr, 0x20);
                    BtlChar_PlayVoice(chr, 0xF);
                }
            }
            if (BtlAnim_GetProgress(chr) < 0.5f) {
                BtlChar_SetFlag(chr, 0xB);
            }
            break;
        case 0x26:
        case 0:
            BtlAnim_Advance(chr, 0);
            if (BtlAnim_GetProgress(chr) > 0.2f) {
                chr->work[1] = 1;
                BtlEvent_Raise(chr->player, 0x5B);
                BtlChar_SetFlag(chr, 0xCD);
            }
            break;
        }
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
        BtlChar_SetFlag(chr, 0x134);
    }
    if (phase == 2 && chr->work[1] != 0) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        if (!BtlChar_TestFlag(chr, 0xFC) && !BtlChar_TestFlag(chr, 0xFD)) {
            BtlChar_SetFlag(chr, 0xF5);
            BtlChar_SetFlag(chr, 0x55);
        }
        BtlEvent_Raise(chr->player, 0x4E);
        BtlChar_AddStageTimer(2.0f);
        return;
    }
}

/* 0xF6 (after 0xEB, a member was defeated): the defeated member stays put while the next member's files load; then 0xF7. */
s32 BtlAct_KoSwitchLoad(BtlActIChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, BtlChar_TestFlag(chr, 0x11) ? 0xD8 : 0xD6, 0.0f);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        BtlAct_PrepareSwitch(chr);
        if (BtlActSwitch_SetLeaveCut(chr)) {
            chr->work[0] |= 1;
        }
        BtlChar_ClearFlag(chr, 0x10E);
    }
    if (phase == 1) {
        if (chr->actionFrame == 1) {
            BtlChange_RequestChara(chr->player, chr->switchChara, chr->switchCostume, chr->switchVariant,
                                   chr->switchAnimChara, chr->switchUnk18, chr->switchVoiceChara);
        } else if (chr->actionFrame >= 2 && BtlChange_IsLoadedFor(chr->player)) {
            BtlAct_Request(chr, 0xF7);
            BtlChange_SetReady(chr->player);
        }
        BtlAnim_AdvanceLoop(chr, 0);
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
        BtlChar_SetFlag(chr, 0x134);
        if (chr->work[0] & 1) {
            BtlChar_SetFlag(chr, 0xB);
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        return;
    }
}

/* 0xF7: first action on the next member's model: placement at its entry point requested, one hidden frame; then 0xF8. */
s32 BtlAct_KoSwitchEnterWait(BtlActIChr *chr, s32 phase) {
    if (phase == 0) {
        BtlAnim_Play(chr, 0, 0.0f);
        BtlChange_SetDone(chr->player);
        BtlActSwitch_SaveEntryPlacement(chr);
        BtlChar_SetFlag(chr, 0xF5);
        BtlChar_SetFlag(chr, 0x55);
    }
    if (phase == 1) {
        BtlMove_Step(chr, 6, 5, 7, 0.0f, BTL_KMH(100.0f));
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
        BtlChar_SetFlag(chr, 0x134);
        BtlChar_SetFlag(chr, 0xB);
        BtlChar_SetFlag(chr, 0xCC);
    }
    if (phase == 2) {
        BtlAct_Request(chr, 0xF8);
    }
}

/*
 * 0xF8: the next member flies in (animation 0x19), lands (0x2D), stands (0) for 30 frames and asks for the round
 * reset (fighter flag 0xF9).
 *
 * Matches only with BtlActChange_SetFlags DEFINED earlier in the same translation unit: with a declaration alone
 * the "likely" bit of three branches of the run-phase switch differs (0x20010C, 0x200114, 0x200128). See
 * BtlAct_SwitchArriveLand.
 * BTL_KMH(2500.0) has a double literal on purpose: with 2500.0f the constant folds to 0x41B92F66, the original is
 * 0x41B92F67 (other spellings give that too, e.g. BTL_KMH(500.0f) * 5.0f; which one the source had is not known).
 */
s32 BtlAct_KoSwitchFlyIn(BtlActIChr *chr, s32 phase) {
    Vec4 pos;
    Vec4 rot;
    s32 *timer = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0x19, 0.0f);
        BtlStage_GetStartPlace(chr->player, &pos, &rot, 0);
        BtlMove_TurnToPoint(chr, &pos, 3.14159265f, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
        BtlActSwitch_SetEnterCut(chr);
    }
    if (phase == 1) {
        switch (BtlAnim_GetId(chr)) {
        case 0x19:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlMove_Step(chr, 6, 6, 3, BTL_KMH(2500.0), BTL_KMH(100.0f));
            BtlMove_BrakeVertical(chr);
            BtlChar_SetFlag(chr, 9);
            BtlChar_SetFxBit(chr, 4);
            BtlChar_SetFxBit(chr, 0xA);
            BtlChar_SetFxBit(chr, 0x3A);
            break;
        case 0x2D:
            BtlAnim_AdvanceThen(chr, 0, 0.15f, 1);
            BtlMove_Step(chr, 6, 5, 3, 0.0f, 10000.0f);
            BtlMove_ApplyGravity(chr);
            break;
        case 0:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlMove_Step(chr, 6, 5, 3, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
            break;
        }
        BtlActChange_SetFlags(chr);
        BtlChar_SetFlag(chr, 0x12D);
        BtlChar_SetFlag(chr, 0x134);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
        case 0x19: {
            s32 landed = BtlChar_TestFlag(chr, 0xF) != 0;

            if (BtlChar_TestFlag(chr, 0x1F) && !BtlChar_TestFlag(chr, 0x20)) {
                landed = 1;
            }
            if (landed) {
                BtlAnim_Request(chr, 0x2D, 0.15f);
                BtlChar_SetFxBit(chr, 0x35);
                BtlCharSnd_PlayCommon(chr, 0x25);
                BtlChar_PlayVoice(chr, 0xF);
                BtlChar_GetPos(chr)->unk98 = 0.0f;
                ChrCam_AddShake(chr, 3.0f, 0.7f);
                BtlChar_Vibrate(chr, 1.0f, 0.5f);
            }
            break;
        }
        case 0x2D:
            break;
        case 0:
            if (++*timer == 0x1E) {
                BtlEvent_Raise(chr->player, 0x5B);
            }
            if (*timer > 0x1E) {
                BtlChar_SetFlag(chr, 0xF9);
                BtlEvent_Raise(chr->player, 0x4E);
            }
            break;
        }
    }
}

/* 0xF9: the round reset action. Leaves through BtlActChange_Finish; on leaving sets effect bit 5, flags 0xD5, 0xCD, 0x37 and adds 2 seconds to the stage timer. */
s32 BtlAct_RoundReset(BtlActIChr *chr, s32 phase) {
    if (phase == 2) {
        BtlActChange_Finish(chr);
    }
    if (phase == 3) {
        BtlChar_SetFxBit(chr, 5);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0xCD);
        BtlChar_SetFlag(chr, 0x37);
        BtlChar_AddStageTimer(2.0f);
        return;
    }
}

/* Starts the skill animation of a class: 0x103 for class 0 (actions 0xFD / 0xFF / 0x101), 0x104 for class 1. */
void BtlSkill_PlayAnim(BtlActIChr *chr, s32 cls) {
    s32 anim = 0;

    switch (cls) {
    case 0:
        anim = 0x103;
        break;
    case 1:
        anim = 0x104;
        break;
    }
    BtlAnim_Play(chr, anim, 0.15f);
}

/* Sets the effect bit of a skill class: 0x3C / 0x3D. */
void BtlSkill_SetFxBit(BtlActIChr *chr, s32 cls) {
    s32 bit = 0;

    switch (cls) {
    case 0:
        bit = 0x3C;
        break;
    case 1:
        bit = 0x3D;
        break;
    }
    BtlChar_SetFxBit(chr, bit);
}

/* Sets the fighter flag of a skill class: 0xDE / 0xDF. */
void BtlSkill_SetFlag(BtlActIChr *chr, s32 cls) {
    s32 flag = 0;

    switch (cls) {
    case 0:
        flag = 0xDE;
        break;
    case 1:
        flag = 0xDF;
        break;
    }
    BtlChar_SetFlag(chr, flag);
}

/*
 * Applies the skill of a slot (0 / 1): the special cases by skill id, health and ki changes, the four stat
 * modifiers by the skill's kind, the two timed values, the use counter, and the blast stock cost.
 */
void BtlSkill_Apply(BtlActIChr *chr, s32 slot) {
    s32 idx = slot != 0;
    s32 id = BtlSkill_GetId(chr, slot);
    s32 attr = BtlSkill_GetFlags(chr, slot);
    s32 health;
    s32 ki;
    s32 time;
    s32 kind;
    s32 lv0;
    s32 lv1;
    s32 lv2;
    s32 lv3;
    s32 valE;
    s32 valF;
    s32 add;

    BtlSkill_GetBlastCost(chr, slot); /* result unused here; read again at the end */
    health = BtlSkill_GetHealthChange(chr, slot);
    ki = BtlSkill_GetKiChange(chr, slot);
    time = BtlSkill_GetFrames(chr, slot);
    kind = BtlSkill_GetStatKind(chr, slot);
    lv0 = BtlSkill_GetStatLevel0(chr, slot);
    lv1 = BtlSkill_GetStatLevel1(chr, slot);
    lv2 = BtlSkill_GetStatLevel2(chr, slot);
    lv3 = BtlSkill_GetStatLevel3(chr, slot);
    valE = BtlSkill_GetValE(chr, slot);
    valF = BtlSkill_GetValF(chr, slot);
    add = (attr >> 8) & 1;

    switch (id) {
    case 0xC:
    case 0x32:
        if (add) {
            chr->skillStackA = BtlUtil_Min(chr->skillStackA + 1, 3);
        } else {
            chr->skillStackA = 1;
        }
        chr->skillSlot = slot;
        break;
    case 0x33:
        chr->skillTimer = time;
        chr->skillSlot = slot;
        break;
    case 0x37:
        if (add) {
            chr->skillStackB = BtlUtil_Min(chr->skillStackB + 1, 3);
        } else {
            chr->skillStackB = 1;
        }
        chr->skillSlot = slot;
        break;
    case 0x17:
        health = -3000;
        break;
    case 0x18:
        BtlChar_SetHeldFlag(chr, 0x98);
        BtlChar_SetFlag(chr, 0x99);
        ki = BtlMember_GetActiveGauge(chr)->kiMax;
        if (time > 0) {
            chr->skillKiRate = ki / time;
        }
        break;
    case 0x41:
        chr->skillCount3 = BtlUtil_Clamp(chr->skillCount3 + 1, 0, 3);
        break;
    }

    if (attr & 0x200) {
        ki = BtlMember_GetActiveGauge(chr)->kiMax;
        BtlMember_AddMaxPower(chr, 30000);
        BtlChar_SetHeldFlag(chr, 6);
        BtlChar_SetFlag(chr, 0xBB);
    }
    if (attr & 0x800000) {
        BtlStat_SetPenalty(chr, 2, idx, -10);
    }
    if (attr & 0x800) {
        switch (kind) {
        case 0:
            break;
        case 1:
            chr->skillTimerC = time;
            break;
        case 2:
            chr->kind2On = 1;
            break;
        case 3:
            chr->kind3On = 1;
            break;
        case 4:
            break;
        }
    }
    if (attr & 0x400) {
        chr->skillTimerD = time;
    }
    if (health > 0) {
        BtlMember_AddHealth(chr, health);
    }
    if (health < 0) {
        BtlMember_Damage(chr, -health, 0x41B);
    }
    if (ki > 0) {
        BtlMember_AddKi(chr, ki);
    }
    if (ki < 0) {
        BtlMember_SpendKi(chr, -ki, 1);
    }

    switch (kind) {
    case 0:
        BtlStat_SetBase(chr, 0, idx, lv0, add);
        BtlStat_SetBase(chr, 1, idx, lv1, add);
        BtlStat_SetBase(chr, 2, idx, lv2, add);
        BtlStat_SetBase(chr, 3, idx, lv3, add);
        break;
    case 1:
        BtlStat_SetTimed(chr, 0, idx, lv0, add, time);
        BtlStat_SetTimed(chr, 1, idx, lv1, add, time);
        BtlStat_SetTimed(chr, 2, idx, lv2, add, time);
        BtlStat_SetTimed(chr, 3, idx, lv3, add, time);
        break;
    case 2:
        BtlStat_SetKind2(chr, 0, idx, lv0, add);
        BtlStat_SetKind2(chr, 1, idx, lv1, add);
        BtlStat_SetKind2(chr, 2, idx, lv2, add);
        BtlStat_SetKind2(chr, 3, idx, lv3, add);
        break;
    case 3:
        BtlStat_SetKind3(chr, 0, idx, lv0, add);
        BtlStat_SetKind3(chr, 1, idx, lv1, add);
        BtlStat_SetKind3(chr, 2, idx, lv2, add);
        BtlStat_SetKind3(chr, 3, idx, lv3, add);
        break;
    case 4:
        BtlStat_SetKind4(chr, 0, idx, lv0, add);
        BtlStat_SetKind4(chr, 1, idx, lv1, add);
        BtlStat_SetKind4(chr, 2, idx, lv2, add);
        BtlStat_SetKind4(chr, 3, idx, lv3, add);
        break;
    }

    if (valE > 0) {
        chr->skillValE = valE;
        chr->skillTimerE = time;
    }
    if (valF > 0) {
        chr->skillValF = valF;
        chr->skillTimerF = time;
    }
    BtlMember_GetActiveGauge(chr)->skillUses[slot]++;
    if (kind != 0) {
        chr->slotOn[idx] = 1;
    }
    BtlMember_SubBlast(chr, BtlSkill_GetBlastCost(chr, slot));
}

/* Per-frame countdown of the skill timers (not while time is stopped). */
void BtlSkill_UpdateTimers(BtlActIChr *chr) {
    if (BtlChars_IsTimeStopped()) {
        return;
    }
    if (--chr->skillTimerE < 0) {
        chr->skillTimerE = 0;
        chr->skillValE = 0;
    }
    if (--chr->skillTimerF < 0) {
        chr->skillTimerF = 0;
        chr->skillValF = 0;
    }
    chr->skillTimerC--;
    if (chr->skillTimerC < 0) {
        chr->skillTimerC = 0;
    }
    if (chr->kind2On != 0) {
        chr->skillTimerC = 1;
    }
    if (chr->kind3On != 0) {
        chr->skillTimerC = 1;
    }
    chr->skillTimerD--;
    if (chr->skillTimerD < 0) {
        chr->skillTimerD = 0;
    }
    if (chr->skillTimer > 0) {
        chr->skillTimer--;
        BtlChar_SetFlag(chr, 0x56);
    }
}

/*
 * 0xFD / 0xFE: skill of slot 0 / 1, the plain kind: plays the skill animation and applies the skill at the start
 * (or at the end with attribute 0x80), then returns to action 0xB.
 */
s32 BtlAct_SkillBasic(BtlActIChr *chr, s32 phase) {
    s32 cls = BtlAct_GetCurrentClass(chr);
    s32 attr = BtlSkill_GetFlags(chr, cls);

    if (phase == 0) {
        BtlSkill_PlayAnim(chr, cls);
        BtlSkill_SetFxBit(chr, cls);
        BtlSkill_SetFlag(chr, cls);
        if (!(attr & 0x80)) {
            BtlSkill_Apply(chr, cls);
        }
        if (attr & 2) {
            BtlMove_TurnYaw(chr, 2, 3.14159265f);
            BtlMove_TurnPitch(chr, 5, 3.14159265f);
        }
        if (attr & 4) {
            BtlMove_TurnYaw(chr, 2, 3.14159265f);
            BtlMove_TurnPitch(chr, 5, 3.14159265f);
            BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        }
        if (attr & 0x40000) {
            ChrCam_RequestCut(chr, 1, 5);
        }
    }
    if (phase == 1) {
        if (!BtlChar_TestFlag(chr, 0xE) && !BtlChar_TestFlag(chr, 0xF)) {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xB);
            if (attr & 0x80) {
                BtlSkill_Apply(chr, cls);
            }
            BtlChar_SetFlag(chr, 0x9A);
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        if (BtlSkill_GetFlags(chr, cls) & 0x1000000) {
            BtlChar_SetFlag(chr, 0x42);
            BtlChar_SetFlag(chr, 0x43);
        }
        if (BtlSkill_GetFlags(chr, cls) & 0x2000000) {
            BtlChar_SetFlag(chr, 0x44);
        }
        if (BtlSkill_GetFlags(chr, cls) & 0x4000000) {
            BtlChar_SetFlag(chr, 0x46);
        }
        if (BtlSkill_GetFlags(chr, cls) & 0x8000000) {
            BtlChar_SetFlag(chr, 0x45);
        }
        BtlSkill_GetFlags(chr, cls);
        if (attr & 0x40000) {
            BtlChar_SetFlag(chr, 0x125);
            BtlChar_SetFlag(chr, 0x12A);
            BtlChar_SetFlag(chr, 0x42);
            BtlChar_SetFlag(chr, 0x43);
            BtlChar_SetFlag(chr, 0x44);
            BtlChar_SetFlag(chr, 0x45);
            BtlChar_SetFlag(chr, 0x46);
            BtlChar_SetFlag(chr, 0x128);
            BtlChar_SetFlag(chr, 0xBB);
        }
        if (BtlSkill_GetId(chr, cls) == 1) {
            BtlChar_SetFlag(chr, 0x137);
        }
    }
    if (phase == 3) {
        switch (BtlSkill_GetId(chr, cls)) {
        case 0x18:
            if (!BtlChar_TestFlag(chr, 0x98)) {
                BtlActIObj *obj = BtlChar_GetObj(chr);

                if (obj->flags & 0x40000) {
                    BtlObj_SetColorMode(obj, 2, 0);
                    return;
                }
            }
            break;
        case 1:
            BtlChar_SetHeldFlag(chr, cls + 0x4C);
            return;
        }
    }
}

/*
 * 0xFF / 0x100: skill of slot 0 / 1, the teleport kind: applies the skill at once, plays the animation, and when it
 * ends (flag 0x31) vanishes and, with lock-on (flag 5), reappears behind the opponent over nine frames.
 */
s32 BtlAct_SkillTeleport(BtlActIChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlSkill_PlayAnim(chr, cls);
        BtlSkill_SetFlag(chr, cls);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlSkill_Apply(chr, cls);
    }
    if (phase == 1) {
        if (!BtlChar_TestFlag(chr, 0x31)) {
            BtlAnim_Advance(chr, 0);
            if (BtlAnim_PassedRatio(chr, 0.8f)) {
                BtlChar_SetFxBit(chr, 0xC);
                BtlCharSnd_PlayCommon(chr, 0x20);
                BtlChar_SetSmallVibration(chr, 0.2f);
            }
            BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            BtlChar_SetFlag(chr, 0x42);
            BtlChar_SetFlag(chr, 0x43);
            BtlChar_SetFlag(chr, 0x44);
            BtlChar_SetFlag(chr, 0x45);
            BtlChar_SetFlag(chr, 0x46);
            BtlChar_GetPos(chr)->unk98 = 0.0f;
            BtlChar_GetPos(chr)->unk9C = 0.0f;
            switch ((*timer)++) {
            case 5:
                if (BtlChar_TestFlag(chr, 5)) {
                    BtlMove_SnapToOpponent(chr);
                    BtlChar_SetFlag(chr, 0xCC);
                }
                break;
            case 6:
                if (BtlChar_TestFlag(chr, 5)) {
                    BtlMove_WarpBehindOpponent(chr, 0.0f);
                    BtlMove_TurnYaw(chr, 2, 3.14159265f);
                    BtlMove_TurnPitch(chr, 1, 3.14159265f);
                    BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
                    BtlMove_SetDirection(chr, 3);
                    BtlChar_SetFlag(chr, 0x3F);
                    BtlChar_SetFlag(chr, 0xCC);
                    BtlChar_SetFlag(chr, 0x24);
                    BtlChar_SetFxBit(chr, 0xD);
                }
                break;
            case 7:
                if (BtlChar_TestFlag(chr, 5)) {
                    BtlChar_SetFlag(chr, 0xCD);
                    BtlChar_SetFlag(chr, 0xD5);
                }
                break;
            case 9:
                BtlAct_Request(chr, 0xB);
                break;
            }
            if (*timer >= 4) {
                BtlChar_SetFlag(chr, 0x26);
            }
            if (*timer < 9) {
                BtlChar_SetFlag(chr, 0xB);
            }
        }
    }
}

/* 0x101 / 0x102: skill of slot 0 / 1 with no animation of its own: applies the skill and leaves at once. */
s32 BtlAct_SkillInstant(BtlActIChr *chr, s32 phase) {
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlSkill_Apply(chr, cls);
    }
    if (phase == 1) {
        BtlAnim_Advance(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
    }
    if (phase == 2) {
        BtlAct_Request(chr, 0xB);
        if (BtlDecide_Main(chr, 0x80000)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/*
 * Forced transitions tested before any input. mask bit 3: stunned (stunTimer) -> 0xD3; bit 2: flag 0xBE -> 0xEA;
 * bit 0: neither airborne (0xE) nor grounded (0xF) now, nor grounded last frame -> 0x11; bit 1: dead -> 0xD2.
 * Queues the action in slot 0 and returns 1.
 */
s32 BtlDecide_Common(BtlActIChr *chr, s32 mask) {
    if ((mask & 8) && chr->stunTimer > 0) {
        BtlAct_SetQueue(chr, 0, 0xD3);
        return 1;
    }
    if ((mask & 4) && BtlChar_TestFlag(chr, 0xBE)) {
        BtlAct_SetQueue(chr, 0, 0xEA);
        return 1;
    }
    if ((mask & 1) && !BtlChar_TestFlag(chr, 0xE) && !BtlChar_TestFlag(chr, 0xF) && !BtlChar_TestPrevFlag(chr, 0xF)) {
        BtlAct_SetQueue(chr, 0, 0x11);
        return 1;
    }
    if ((mask & 2) && BtlChar_IsDead(chr)) {
        BtlAct_SetQueue(chr, 0, 0xD2);
        return 1;
    }
    return 0;
}

#define TEST(id) BtlInput_TestAction(chr, id, 1)

/*
 * The main input table of a free fighter. Each group is enabled by a bit of `mask` (BTLDEC_*); the first input
 * that is satisfied queues its action in slot 0. Returns the number of actions queued (0, 1 or 2).
 * The table is written out in the header.
 */
s32 BtlDecide_Main(BtlActIChr *chr, s32 mask) {
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if ((mask & 0x10000000) && BtlMember_HasHealth(chr, 5000)) {
        BtlColl_AddActionBit(chr, 0x42);
        if (TEST(0x2E)) {
            BtlAct_SetQueue(chr, 0, 0x42);
            return 1;
        }
    }
    if ((mask & 0x400) && BtlMember_HasKi(chr, 1000)) {
        if (TEST(0x15)) {
            BtlAct_SetQueue(chr, 0, 0x1A);
            return 1;
        }
        if (BtlChar_TestFlag(chr, 5) && TEST(0x14)) {
            BtlAct_SetQueue(chr, 0, 0x19);
            return 1;
        }
    }
    if ((mask & 0x8000000) && BtlMember_HasKi(chr, 10000) && BtlChar_TestFlag(chr, 5)) {
        s32 ok = 0;

        if (!BtlChar_TestFlag(chr, 0x13)) {
            ok = 1;
        }
        if (BtlOpp_GetSeenAction(chr) == 0xD5) {
            ok = 1;
        }
        if (ok && TEST(0x14)) {
            BtlAct_SetQueue(chr, 0, 0x33);
            return 1;
        }
    }
    if ((mask & 0x800) && TEST(0xF)) {
        BtlAct_SetQueue(chr, 0, 0x17);
        return 1;
    }
    if ((mask & 0x1000) && !BtlChar_TestFlag(chr, 0xF) && TEST(0x11)) {
        BtlAct_SetQueue(chr, 0, 0x18);
        return 1;
    }
    if ((mask & 6) && BtlChar_TestFlag(chr, 0xF) && !BtlChar_TestFlag(chr, 0x11) && !BtlChar_TestFlag(chr, 0xE)
        && TEST(8)) {
        if (mask & 2) {
            BtlAct_SetQueue(chr, 0, 0x10);
        } else {
            BtlAct_SetQueue(chr, 0, 0x12);
        }
        return 1;
    }
    if ((mask & 0x18) && (!BtlChar_TestFlag(chr, 0x13) || (mask & 0x10)) && TEST(3)) {
        BtlAct_SetQueue(chr, 0, 0xF);
        return 1;
    }
    if ((mask & 0x6000) && BtlChar_TestFlag(chr, 5) && (BtlChar_TestFlag(chr, 0x13) || (mask & 0x4000))) {
        if (TEST(0x19)) {
            BtlAct_SetQueue(chr, 0, 0x1B);
            return 1;
        }
        if (TEST(0x1A)) {
            BtlAct_SetQueue(chr, 0, 0x1C);
            return 1;
        }
        if (TEST(0x1B)) {
            BtlAct_SetQueue(chr, 0, 0x1D);
            return 1;
        }
    }
    if ((mask & 0x18000) && (BtlParam_GetFlags2(chr) & 1) && BtlChar_TestFlag(chr, 5)
        && (BtlChar_TestFlag(chr, 0x13) || (mask & 0x10000)) && TEST(0x1C)) {
        BtlAct_SetQueue(chr, 0, 0x1E);
        return 1;
    }
    if ((mask & 0x20000) && (BtlParam_GetFlags2(chr) & 8) && BtlChar_TestFlag(chr, 5)
        && BtlMember_HasKi(chr, BtlParam_GetAmountA(chr))) {
        if (TEST(0x20)) {
            BtlAct_SetQueue(chr, 0, 0x23);
            return 1;
        }
        if (TEST(0x1D)) {
            BtlAct_SetQueue(chr, 0, 0x20);
            return 1;
        }
        if (TEST(0x1E)) {
            BtlAct_SetQueue(chr, 0, 0x21);
            return 1;
        }
        if (TEST(0x1F)) {
            BtlAct_SetQueue(chr, 0, 0x22);
            return 1;
        }
    }
    if ((mask & 0x40000) && BtlChar_TestFlag(chr, 5) && BtlChar_TestFlag(chr, 0x13)
        && BtlMember_HasKi(chr, BtlParam_GetAmountB(chr)) && BtlChar_TestFlag(chr, 0xB3) && chr->unk1080 > 0) {
        BtlColl_AddActionBit(chr, 0x24);
        if (TEST(0x21)) {
            BtlAct_SetQueue(chr, 0, 0x24);
            return 1;
        }
    }
    if ((mask & 0x20000000) && BtlAct_TestPoweredSkill(chr, 8) && TEST(0x31)) {
        BtlAct_SetQueue(chr, 0, 0x35);
        return 1;
    }
    if ((mask & 0x100000) && BtlAct_CheckTechniqueInput(chr)) {
        return 1;
    }
    if ((mask & 0x800000) && BtlAct_CheckSkillInput(chr)) {
        return 1;
    }
    if ((mask & 0x80) && !BtlChar_TestFlag(chr, 6) && !BtlChar_TestFlag(chr, 0x98)) {
        if (BtlParam_GetUnk0(chr) & 0x80) {
            if (BtlMember_IsKiFull(chr) && BtlMember_HasBlast(chr, 100000) && TEST(0x13)) {
                BtlAct_SetQueue(chr, 0, 0x37);
                return 1;
            }
        } else if (TEST(0x13)) {
            BtlAct_SetQueue(chr, 0, 0x37);
            return 1;
        }
    }
    if ((mask & 0x1000000) && (BtlParam_GetFlags2(chr) & 0x20)) {
        if (BtlParam_GetFlags(chr) & 2) {
            if (BtlMove_IsBlastIncoming(chr, 1, 1, 1, 90.0f) && TEST(0x2B)) {
                BtlAct_SetQueue(chr, 0, 0x3D);
                return 1;
            }
        } else if (BtlMove_IsBlastIncoming(chr, 1, 1, 0, 90.0f) && TEST(0x2B)) {
            BtlAct_SetQueue(chr, 0, 0x3B);
            return 1;
        }
    }
    if ((mask & 0x2000000) && (BtlParam_GetFlags2(chr) & 0x20) && BtlMove_IsBlastIncoming(chr, 0, 1, 0, 5.0f)
        && TEST(0x27)) {
        BtlAct_SetQueue(chr, 0, 0x3C);
        return 1;
    }
    if ((mask & 0x80000) && TEST(0x22)) {
        if (BtlChar_TestFlag(chr, 5)) {
            if (__builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) > 1.7f) {
                BtlAct_SetQueue(chr, 0, 0x3A);
                BtlAct_SetQueue(chr, 1, 0x38);
                return 2;
            }
            BtlAct_SetQueue(chr, 0, 0x38);
            return 1;
        }
        BtlAct_SetQueue(chr, 0, 0x38);
        return 1;
    }
    if ((mask & 0x20) && (BtlChar_TestFlag(chr, 0xE) || BtlChar_TestFlag(chr, 0x11)) && TEST(0xA)) {
        BtlAct_SetQueue(chr, 0, BtlAct_GetAction14or16(chr));
        return 1;
    }
    if ((mask & 0x40) && (BtlChar_TestFlag(chr, 0xE) || BtlChar_TestFlag(chr, 0x11)) && !BtlChar_TestFlag(chr, 0xF)) {
        if (!BtlParam_CanFly(chr) && !BtlChar_TestFlag(chr, 0x11)) {
            if (TEST(0xE)) {
                BtlAct_SetQueue(chr, 0, 0x11);
                return 1;
            }
        } else if (TEST(0xC)) {
            BtlAct_SetQueue(chr, 0, 0x15);
            return 1;
        }
    }
    if ((mask & 0x400000) && BtlAct_CheckSwitchInput(chr)) {
        return 1;
    }
    if ((mask & 0x100) && BtlAct_CheckTransformInput(chr)) {
        return 1;
    }
    if ((mask & 0x200) && BtlAct_CheckFusionInput(chr)) {
        return 1;
    }
    if ((mask & 1) && TEST(1)) {
        if (BtlChar_TestFlag(chr, 0x13)) {
            if (BtlAct_GetCurrent(chr) != 0xE) {
                BtlAct_SetQueue(chr, 0, 0xE);
                return 1;
            }
        } else if (BtlAct_GetCurrent(chr) != 0xD) {
            BtlAct_SetQueue(chr, 0, 0xD);
            return 1;
        }
    }
    if (mask & 0x200000) {
#ifdef PORT
        /* A deliberate lock-off (including a gauntlet rest) is not a lost
           opponent. Do not automatically search the hidden roster slot. */
        if (TagTeam_LockedOff(chr->player)) return 0;
#endif
        if (BtlChar_TestFlag(chr, 5)) {
            return 0;
        }
        if (BtlChar_TestFlag(chr, 0x93)) {
            return 0;
        }
        if (BtlChar_TestFlag(chr, 3)) {
            BtlAct_SetQueue(chr, 0, 0x36);
            return 1;
        }
    }
    return 0;
}

/*
 * The attack input table of a free fighter (rush attacks, ki blasts, charged attacks, throws ...). Same protocol as
 * BtlDecide_Main: groups enabled by `mask`, the first satisfied input queues its action(s); returns the count.
 */
s32 BtlDecide_Attack(BtlActIChr *chr, s32 mask) {
    if (!BtlChar_IsFree(chr)) {
        return 0;
    }
    if (mask & 1) {
        BtlColl_AddActionBit(chr, 0x44);
        if (TEST(0x34)) {
            if (BtlChar_TestFlag(chr, 5)) {
                if (__builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) > 1.7f) {
                    BtlAct_SetQueue(chr, 0, 0x3A);
                    BtlAct_SetQueue(chr, 1, 0x44);
                    return 2;
                }
                BtlAct_SetQueue(chr, 0, 0x44);
                return 1;
            }
            BtlAct_SetQueue(chr, 0, 0x44);
            return 1;
        }
    }
    if (mask & 2) {
        BtlColl_AddActionBit(chr, 0x47);
        if (TEST(0x35)) {
            BtlAct_SetQueue(chr, 0, 0x47);
            return 1;
        }
        if (TEST(0x36)) {
            BtlAct_SetQueue(chr, 0, 0x48);
            return 1;
        }
        if (TEST(0x37)) {
            BtlAct_SetQueue(chr, 0, 0x49);
            return 1;
        }
        if (TEST(0x38)) {
            BtlAct_SetQueue(chr, 0, 0x4A);
            return 1;
        }
        if (TEST(0x39)) {
            if (chr->unkD60 < 5) {
                BtlAct_SetQueue(chr, 0, 0x4B);
            } else {
                BtlAct_SetQueue(chr, 0, 0x4C);
            }
            return 1;
        }
    }
    if (mask & 0x2000) {
        BtlColl_AddActionBit(chr, 0x55);
        if (TEST(0x3A)) {
            BtlAct_SetQueue(chr, 0, 0x55);
            return 1;
        }
        if (TEST(0x3B)) {
            BtlAct_SetQueue(chr, 0, 0x56);
            return 1;
        }
        if (TEST(0x3C)) {
            BtlAct_SetQueue(chr, 0, 0x53);
            return 1;
        }
        if (TEST(0x3D)) {
            BtlAct_SetQueue(chr, 0, 0x54);
            return 1;
        }
        if (TEST(0x3E)) {
            BtlAct_SetQueue(chr, 0, 0x57);
            return 1;
        }
    }
    if ((mask & 0x4000000) && (BtlParam_GetFlags2(chr) & 0x10000)) {
        BtlColl_AddActionBit(chr, 0x6D);
        if (TEST(0x4A)) {
            BtlAct_SetQueue(chr, 0, 0x6D);
            return 1;
        }
        if (TEST(0x4B)) {
            BtlAct_SetQueue(chr, 0, 0x6E);
            return 1;
        }
        if (TEST(0x48)) {
            BtlAct_SetQueue(chr, 0, 0x6B);
            return 1;
        }
        if (TEST(0x49)) {
            BtlAct_SetQueue(chr, 0, 0x6C);
            return 1;
        }
        if (TEST(0x4C)) {
            BtlAct_SetQueue(chr, 0, 0x6E);
            return 1;
        }
    }
    if ((mask & 4) && BtlMember_HasKi(chr, BtlKiBlast_GetKiCostOf(chr, 0)) && chr->unkDE0 < BtlParam_GetCount80(chr)
        && BtlMove_CanFireBlast(chr, 0, NULL)) {
        BtlColl_AddActionBit(chr, 0xAE);
        if (TEST(0x59)) {
            if (BtlChar_TestFlag(chr, 5)) {
                if (__builtin_fabsf(BtlOpp_GetYawFromFacing(chr)) > 1.7f) {
                    BtlAct_SetQueue(chr, 0, 0x3A);
                    BtlAct_SetQueue(chr, 1, 0xAE);
                    return 2;
                }
                BtlAct_SetQueue(chr, 0, 0xAE);
                return 1;
            }
            BtlAct_SetQueue(chr, 0, 0xAE);
            return 1;
        }
    }
    if ((mask & 8) && BtlMember_HasKi(chr, BtlKiBlast_GetKiCostOf(chr, 3)) && chr->unkDE0 < BtlParam_GetCount80(chr)
        && BtlMove_CanFireBlast(chr, 1, NULL)) {
        BtlColl_AddActionBit(chr, 0xAF);
        if (TEST(0x5A)) {
            BtlAct_SetQueue(chr, 0, 0xAF);
            return 1;
        }
    }
    if ((mask & 0x4000) && (BtlParam_GetFlags2(chr) & 0x10) && BtlMember_HasKi(chr, BtlKiBlast_GetKiCostOf(chr, 4))
        && chr->unkDE0 < BtlParam_GetCount80(chr) && BtlMove_CanFireBlast(chr, 0, NULL)) {
        BtlColl_AddActionBit(chr, 0xB0);
        if (TEST(0x59)) {
            BtlAct_SetQueue(chr, 0, 0xB0);
            return 1;
        }
    }
    if ((mask & 0x8000) && (BtlParam_GetFlags2(chr) & 0x10) && BtlMember_HasKi(chr, BtlKiBlast_GetKiCostOf(chr, 7))
        && chr->unkDE0 < BtlParam_GetCount80(chr) && BtlMove_CanFireBlast(chr, 1, NULL)) {
        BtlColl_AddActionBit(chr, 0xB1);
        if (TEST(0x5A)) {
            BtlAct_SetQueue(chr, 0, 0xB1);
            return 1;
        }
    }
    if ((mask & 0x10000) && TEST(0x40)) {
        BtlAct_SetQueue(chr, 0, 0x59);
        return 1;
    }
    if ((mask & 0x20000) && (BtlParam_GetFlags2(chr) & 0x10) && BtlMember_HasKi(chr, BtlKiBlast_GetKiCostOf(chr, 8))
        && chr->unkDE0 < BtlParam_GetCount80(chr) && BtlMove_CanFireBlast(chr, 0, NULL)) {
        BtlColl_AddActionBit(chr, 0xB2);
        if (TEST(0x59)) {
            BtlAct_SetQueue(chr, 0, 0xB2);
            return 1;
        }
    }
    if ((mask & 0x40000) && (BtlParam_GetFlags2(chr) & 0x10) && BtlMember_HasKi(chr, BtlKiBlast_GetKiCostOf(chr, 0xB))
        && chr->unkDE0 < BtlParam_GetCount80(chr) && BtlMove_CanFireBlast(chr, 1, NULL)) {
        BtlColl_AddActionBit(chr, 0xB3);
        if (TEST(0x5A)) {
            BtlAct_SetQueue(chr, 0, 0xB3);
            return 1;
        }
    }
    if ((mask & 0x10) && (BtlParam_GetFlags2(chr) & 0x800)) {
        BtlColl_AddActionBit(chr, 0x5E);
        if (TEST(0x4D)) {
            BtlAct_SetQueue(chr, 0, 0x5E);
            return 1;
        }
    }
    if ((mask & 0x800000) && (BtlParam_GetFlags2(chr) & 0x800)) {
        BtlColl_AddActionBit(chr, 0x5E);
        if (TEST(0x4D)) {
            s32 list[2];
            s32 n;
            s32 i;

            memset(list, 0, sizeof(list));
            n = 0;
            switch (BtlParam_GetUnk8A(chr, 1)) {
            case 0:
                list[0] = 0x5E;
                n = 1;
                break;
            case 1:
                list[0] = 0x40;
                list[1] = 0x5E;
                n = 2;
                break;
            }
            if (n > 0) {
                for (i = 0; i < n; i++) {
                    BtlAct_SetQueue(chr, i, list[i]);
                }
                return n;
            }
        }
    }
    if ((mask & 0x20) && (BtlParam_GetFlags2(chr) & 0x1000)) {
        BtlColl_AddActionBit(chr, 0x5F);
        if (TEST(0x4E)) {
            BtlAct_SetQueue(chr, 0, 0x5F);
            return 1;
        }
    }
    if ((mask & 0x1000000) && (BtlParam_GetFlags2(chr) & 0x1000)) {
        BtlColl_AddActionBit(chr, 0x5F);
        if (TEST(0x4E)) {
            s32 list[2];
            s32 n;
            s32 i;

            memset(list, 0, sizeof(list));
            n = 0;
            switch (BtlParam_GetUnk8A(chr, 2)) {
            case 0:
                list[0] = 0x5F;
                n = 1;
                break;
            case 1:
                list[0] = 0x40;
                list[1] = 0x5F;
                n = 2;
                break;
            }
            if (n > 0) {
                for (i = 0; i < n; i++) {
                    BtlAct_SetQueue(chr, i, list[i]);
                }
                return n;
            }
        }
    }
    if (mask & 0x200000) {
        s32 act = 0;

        switch (BtlParam_GetUnk84(chr, (chr->unkD60 + 4) % 5)) {
        case 0:
            act = 0x60;
            break;
        case 1:
            act = 0x61;
            break;
        case 2:
            act = 0x62;
            break;
        case 3:
            act = 0x63;
            break;
        case 4:
            act = 0x66;
            break;
        case 5:
            act = BtlChar_TestFlag(chr, 0x8F) ? 0x64 : 0x65;
            break;
        }
        BtlColl_AddActionBit(chr, act);
        if (TEST(0x42)) {
            BtlAct_SetQueue(chr, 0, act);
            return 1;
        }
    }
    if (mask & 0xA400000) {
        s32 list[2];
        s32 n;
        s32 kind;
        s32 input;
        s32 i;

        memset(list, 0, sizeof(list));
        n = 0;
        kind = -1;
        input = -1;
        if (mask & 0x400000) {
            kind = 0;
            input = 0x42;
        } else if (mask & 0x2000000) {
            kind = 3;
            input = 0x42;
        } else if (BtlParam_GetCount80(chr) <= 0) {
            kind = 4;
            input = 0x58;
        }
        if (kind >= 0) {
            switch (BtlParam_GetUnk8A(chr, kind)) {
            case 1:
                list[0] = 0x40;
                n = 1;
            case 0:
                list[n++] = 0x60;
                break;
            case 3:
                list[0] = 0x40;
                n = 1;
            case 2:
                list[n++] = 0x61;
                break;
            case 5:
                list[0] = 0x40;
                n = 1;
            case 4:
                list[n++] = 0x62;
                break;
            case 7:
                list[0] = 0x40;
                n = 1;
            case 6:
                list[n++] = 0x63;
                break;
            case 9:
                list[0] = 0x40;
                n = 1;
            case 8:
                list[n++] = 0x66;
                break;
            case 11:
                list[0] = 0x40;
                n = 1;
            case 10:
                list[n++] = BtlChar_TestFlag(chr, 0x8F) ? 0x64 : 0x65;
                break;
            }
            if (n > 0) {
                BtlColl_AddActionBit(chr, list[n - 1]);
                if (TEST(input)) {
                    for (i = 0; i < n; i++) {
                        BtlAct_SetQueue(chr, i, list[i]);
                    }
                    return n;
                }
            }
        }
    }
    if ((mask & 0xC0) && (BtlChar_TestFlag(chr, 0x13) || (mask & 0x80))) {
        BtlColl_AddActionBit(chr, 0xB4);
        if (TEST(0x5C)) {
            BtlAct_SetQueue(chr, 0, 0xB4);
            return 1;
        }
    }
    if ((mask & 0x300) && (BtlChar_TestFlag(chr, 0x13) || (mask & 0x200)) && (BtlParam_GetFlags2(chr) & 0x80000)) {
        BtlColl_AddActionBit(chr, 0xB6);
        if (TEST(0x5E)) {
            BtlAct_SetQueue(chr, 0, 0xB6);
            return 1;
        }
    }
    if ((mask & 0x80000) && chr->unkD68 < chr->unkD6C && BtlMember_HasKi(chr, 5000)) {
        BtlColl_AddActionBit(chr, 0x19);
        if (TEST(0x2A)) {
            BtlAct_SetQueue(chr, 0, 0x19);
            return 1;
        }
    }
    if (mask & 0x100000) {
        s32 ret = BtlDecide_QueueAttack(chr, 0x9B);

        if (ret > 0) {
            return ret;
        }
        if (chr->unkD70 < chr->unkD74) {
            if (BtlParam_GetFlags2(chr) & 0x2000) {
                BtlColl_AddActionBit(chr, 0x5C);
                if (TEST(0x44)) {
                    BtlAct_SetQueue(chr, 0, 0x5C);
                    return 1;
                }
                if (TEST(0x45)) {
                    BtlAct_SetQueue(chr, 0, 0x5D);
                    return 1;
                }
                if (TEST(0x46)) {
                    BtlAct_SetQueue(chr, 0, 0x5A);
                    return 1;
                }
                if (TEST(0x47)) {
                    BtlAct_SetQueue(chr, 0, 0x5B);
                    return 1;
                }
            }
        } else if ((BtlParam_GetFlags3(chr) & 0x1000) && !BtlChar_TestMemberUnk70(chr)) {
            ret = BtlAct_QueueComboFinish(chr);
            if (ret > 0) {
                return ret;
            }
        }
    }
    if ((mask & 0x400) && TEST(0x5F)) {
        BtlAct_SetQueue(chr, 0, 0x67);
        return 1;
    }
    if ((mask & 0x1000) && TEST(0x60)) {
        BtlAct_SetQueue(chr, 0, 0x68);
        return 1;
    }
    if ((mask & 0x800) && TEST(0x60)) {
        if (BtlParam_GetUnk88(chr, 1) == 5) {
            BtlAct_SetQueue(chr, 0, 0x40);
            return 1;
        }
        BtlAct_SetQueue(chr, 0, 0x69);
        return 1;
    }
    return 0;
}

/*
 * Queues attack `id` (an attack table record, 0x70..) if it is available: the record's skill bit, the opponent state
 * its kind needs, and its input. Queues the record's lead-in action(s) first, then the attack, and prepares the
 * attack parameters. Returns the number of actions queued. The only caller of BtlAct_PrepareAttack.
 */
s32 BtlDecide_QueueAttack(BtlActIChr *chr, s32 id) {
    s32 list[8];
    s32 queued = 0;
    BtlActIAttackRec *rec;
    s32 idx;
    s32 n;
    s32 ok;
    s32 i;
    s32 input;

    if (!BtlAct_IsAttackId(id)) {
        return 0;
    }
    idx = id - 0x70;
    rec = &gBtlChars->attackTbl[idx];
    if (!BtlAct_TestAttackSkill(chr, idx)) {
        return 0;
    }
    n = 0;
    ok = 0;
    switch (rec->kind) {
    case 0:
        ok = 1;
        break;
    case 1:
        list[0] = 0xD4;
        n = 1;
        break;
    case 2:
        list[0] = 0xD5;
        n = 1;
        break;
    case 3:
        list[0] = 0xD0;
        n = 1;
        break;
    case 4:
        list[0] = 0xCD;
        n = 1;
        break;
    case 5:
        list[0] = 0xCB;
        n = 1;
        break;
    case 6:
        list[0] = 0xC6;
        n = 1;
        break;
    case 7:
        list[0] = 0xC7;
        list[1] = 0xC8;
        n = 2;
        break;
    case 8:
        list[0] = 0xCE;
        n = 1;
        break;
    }
    if (n > 0 && BtlChar_IsFlagRaised(chr, 0x5B)) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (BtlOpp_GetSeenAction(chr) == list[i]) {
            ok = 1;
            break;
        }
    }
    if (!ok) {
        return 0;
    }
    BtlColl_AddActionBit(chr, id);
    input = -1;
    switch (rec->inputKind) {
    case 1:
        input = 0x4F;
        break;
    case 2:
        input = 0x50;
        break;
    case 3:
        input = 0x51;
        break;
    case 4:
        input = 0x52;
        break;
    case 5:
        input = 0x53;
        break;
    case 6:
        input = 0x54;
        break;
    case 7:
        input = 0x55;
        break;
    case 8:
        input = 0x56;
        break;
    case 9:
        input = 0x57;
        break;
    }
    if (input >= 0 && !TEST(input)) {
        return 0;
    }
    switch (rec->leadIn) {
    case 0:
        break;
    case 1:
        BtlAct_SetQueue(chr, queued++, 0x2B);
        break;
    case 2:
        BtlAct_SetQueue(chr, queued++, 0x2B);
        BtlAct_SetQueue(chr, queued++, 0x2D);
        break;
    case 3:
        BtlAct_SetQueue(chr, queued++, 0x2B);
        BtlAct_SetQueue(chr, queued++, 0x2E);
        break;
    case 4:
        BtlAct_SetQueue(chr, queued++, BtlChar_FrameMod(2) ? 0x21 : 0x22);
        break;
    case 5:
        BtlAct_SetQueue(chr, queued++, 0x2B);
        BtlAct_SetQueue(chr, queued++, 0x2F);
        break;
    case 6:
        BtlAct_SetQueue(chr, queued++, 0x31);
        break;
    case 7:
        BtlAct_SetQueue(chr, queued++, 0x32);
        break;
    case 8:
        BtlAct_SetQueue(chr, queued++, 0x2B);
        BtlAct_SetQueue(chr, queued++, 0x30);
        break;
    case 9:
        BtlAct_SetQueue(chr, queued++, 0x2C);
        BtlAct_SetQueue(chr, queued++, 0x2D);
        break;
    }
    BtlAct_SetQueue(chr, queued++, id);
    BtlAct_PrepareAttack(chr, id);
    return queued;
}
