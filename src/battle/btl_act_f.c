#include "common.h"
#include "battle/btl_act_f.h"

/*
 * Blast 2 / Ultimate Blast actions: 0x1F5460..0x1FC2B0, in two parts (to 0x1F8C00, and from there the rush
 * techniques, formerly btl_act_g.c, with their own header comment below and their own view of the fighter,
 * include/battle/btl_act_g.h). One file because BtlAct_SuperRushDashHandler (0x1F8C00) matches only with the
 * first part's functions defined above it.
 *
 * First part:
 *
 * Eighteen helpers shared by every action of a class 2..4 technique (the callers reach up to 0x1FB660), then the
 * handlers of actions 0x106..0x11A. The class (BtlAct_GetCurrentClass: 2 and 3 are the two Blast 2 moves, 4 the
 * Ultimate Blast) is the position of the action inside its triple; it selects the technique's parameters
 * (object + 0x92C, readers at 0x210D48..) and its 0x22 animations, which start at 0x105 + (class - 2) * 0x22:
 *
 *   +0 start   +1 charge loop   +2 fire   +3 firing loop   +4 second stage   +5 second loop   +6 end
 *   +7..+13 and +14..+20: the same seven aimed up and down (BtlAct_SetPitchMotion)
 *   +0x17..: the steps of a rush technique's animation chain (BtlSuper_MeasureRushStep)
 *
 *   0x106..0x108  BtlAct_SuperBeamHandler          beam: charge, fire, hold
 *   0x109..0x10B  BtlAct_SuperWarpBeamHandler      the same with a teleport in front of the opponent before firing
 *   0x10C..0x10E  BtlAct_SuperLongBeamHandler      beam with a second firing stage
 *   0x10F..0x111  BtlAct_SuperChargeHandler        chargeable: the button is held, release fires
 *   0x112..0x114  BtlAct_SuperRepeatHandler        repeated shots, each paid with health
 *   0x115..0x117  BtlAct_SuperQuickBeamHandler     beam without the charge, 30% damage (flag 0xA4)
 *   0x118..0x11A  BtlAct_SuperQuickLongBeamHandler two-stage beam without the charge, 30% damage
 *
 * These are attacker-side actions: nothing here reads the hit reaction block at fighter + 0xFB0. The handlers
 * are void (their leave phase ends in a tail call to BtlSuper_Leave). chr->work[0] is a bit set, work[1] the
 * "end animation finished" mark the decide phase waits for, work[2] a frame counter, work[3] / work[4] per
 * handler. The object continues past 0x1F8C00 (the same helpers, the float pool runs on).
 *
 * The game terms (beam, charge, quick form) are a reading of the code; what the matching C verifies is the
 * sequence of animations, flags and calls.
 */

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))

extern BtlSuperObj *BtlChar_GetObj(BtlSuperChr *chr);
extern BtlSuperPose *BtlChar_GetPos(BtlSuperChr *chr);
extern s32 BtlChar_TestFlag(BtlSuperChr *chr, s32 bit);
extern void BtlChar_SetFlag(BtlSuperChr *chr, s32 bit);
extern void BtlChar_SetHeldFlag(BtlSuperChr *chr, s32 bit);
extern void BtlChar_ClearFlag(BtlSuperChr *chr, s32 bit);
extern void BtlChar_ClearFlagRange(BtlSuperChr *chr, u32 a, u32 b);
extern s32 BtlChar_IsFlagRaised(BtlSuperChr *chr, u32 n);
extern void BtlChar_SetFxBit(BtlSuperChr *chr, s32 n);
extern void BtlCharSnd_PlayCommon(BtlSuperChr *chr, s32 id);
extern void ChrCam_RequestCut(BtlSuperChr *chr, s32 arg1, s32 arg2);
extern void ChrCam_EndCut(BtlSuperChr *chr);
extern void ChrCam_AddShake(BtlSuperChr *chr, f32 strength, f32 time);
extern void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
extern void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 arg3, f32 arg4);
extern void BtlChar_SetVibration(BtlSuperChr *chr, f32 power, f32 seconds);
extern void BtlChar_Vibrate(BtlSuperChr *chr, f32 power, f32 seconds);
extern f32 BtlUtil_WrapAngle(f32 a);
extern s32 BtlUtil_Min(s32 a, s32 b);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);
extern void BtlAct_Request(BtlSuperChr *chr, s32 id);
extern s32 BtlAct_GetRequested(BtlSuperChr *chr);
extern s32 BtlAct_IsTechniqueId(s32 id);
extern void BtlAct_PushAngle(BtlSuperChr *chr, f32 angle, f32 speed, f32 arg);
extern void BtlAct_SetObjUnk(BtlSuperChr *chr, f32 a, f32 b);
extern void BtlAnim_PlaySub(BtlSuperChr *chr, s32 anim);
extern void BtlAnim_SetStep(BtlSuperChr *chr, f32 step);
extern void BtlAnim_SetDuration(BtlSuperChr *chr, f32 seconds);
extern f32 BtlAnim_GetFrame(BtlSuperChr *chr);
extern s32 BtlAnim_TestAttr(BtlSuperChr *chr, u64 mask);
extern s32 BtlAnim_PassedFrame(BtlSuperChr *chr, f32 frame);
extern BtlSuperGauge *BtlMember_GetActiveGauge(BtlSuperChr *chr);
extern s32 BtlOpp_GetParamWord0(BtlSuperChr *chr);
extern void BtlOpp_GetTargetRot(BtlSuperChr *chr, Vec4 *out);
extern void BtlOpp_GetObjVecFA0(BtlSuperChr *chr, Vec4 *out);
extern void BtlMove_SetHeading(BtlSuperChr *chr, f32 yaw, f32 pitch);
extern void BtlChar_SetUnk1310(BtlSuperChr *chr, Vec4 *v);

extern s32 BtlAct_GetCurrentClass(BtlSuperChr *chr);
extern s32 BtlAct_GetPrev(BtlSuperChr *chr);
extern void BtlAct_SetFormCurrent(BtlSuperChr *chr);
extern void BtlAct_SetPitchMotion(BtlSuperChr *chr, s32 motionUp, s32 motionDown, s32 recalc);
extern void BtlAnim_Play(BtlSuperChr *chr, s32 anim, f32 blend);
extern void BtlAnim_Request(BtlSuperChr *chr, s32 anim, f32 blend);
extern s32 BtlAnim_GetId(BtlSuperChr *chr);
extern s32 BtlAnim_Advance(BtlSuperChr *chr, s32 flags);
/* The blend time is the third parameter: with it last (as btl_char_status.h has it) the two quick handlers
   set up $a2 and $f12 in the wrong order. Both orders pass the same registers. */
extern s32 BtlAnim_AdvanceThen(BtlSuperChr *chr, s32 next, f32 blend, s32 flags);
extern void BtlAnim_AdvanceLoop(BtlSuperChr *chr, s32 flags);
extern s32 BtlAnim_IsNew(BtlSuperChr *chr);
extern void BtlMove_TurnYaw(BtlSuperChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_TurnPitch(BtlSuperChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_TurnModelYaw(BtlSuperChr *chr, f32 maxStep, f32 rate);
extern void BtlMove_SetDirection(BtlSuperChr *chr, s32 mode);
extern void BtlMove_Step(BtlSuperChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel);
extern void BtlMove_ApplyGravity(BtlSuperChr *chr);
extern void BtlMove_WarpInFrontOfOpponent(BtlSuperChr *chr, f32 gap);
extern void BtlMove_SnapToOpponent(BtlSuperChr *chr);
extern s32 BtlMove_CountOwnBeams(BtlSuperChr *chr);
extern s32 BtlMember_SpendKi(BtlSuperChr *chr, s32 amount, s32 force);
extern s32 BtlMember_Damage(BtlSuperChr *chr, s32 amount, s32 flags);
extern s32 BtlMember_HasAbility(BtlSuperChr *chr, s32 n);
extern void BtlChar_ResetLook(BtlSuperChr *chr);
extern void BtlChar_SavePlacement(BtlSuperChr *chr);
extern s32 BtlInput_TestAction(BtlSuperChr *chr, s32 id, s32 want);
extern void BtlChange_RequestChara(s32 player, s32 chara, s32 costume, s32 variant, s32 animChara, s32 unk18,
                                   s32 voiceChara);
extern s32 BtlChange_IsLoadedFor(s32 player);
extern void BtlChange_SetReady(s32 player);
extern void BtlChange_SetDone(s32 player);

/* Technique parameter readers (0x210D48.., another module): the block at object + 0x92C, arrays by class - 2. */
extern s32 BtlSuper_GetFlagsA(BtlSuperChr *chr, s32 cls);  /* flag word A of a class 2..4 technique */
extern s32 BtlSuper_GetFlags(BtlSuperChr *chr, s32 cls);  /* flag word B */
extern s32 BtlSuper_GetId(BtlSuperChr *chr, s32 cls);  /* technique id (s16) */
extern f32 BtlSuper_GetShotTime(BtlSuperChr *chr, s32 cls);  /* a time in seconds */
extern s32 BtlSuper_GetShots(BtlSuperChr *chr, s32 cls);  /* shot limit, at least 1 */
extern s32 BtlSuper_GetRecoilDamage(BtlSuperChr *chr, s32 cls);  /* health cost per shot */
extern s32 BtlSuper_GetHitDirKind(BtlSuperChr *chr, s32 cls);  /* kind 0..9 (s8) */
extern s32 BtlSuper_GetStepCount(BtlSuperChr *chr, s32 cls);  /* number of rush steps, at least 1 */
extern s32 BtlSuper_GetDrain(BtlSuperChr *chr, s32 cls);  /* health drained */
extern s32 BtlSuper_GetCooldownFrames(BtlSuperChr *chr, s32 cls);  /* cooldown: seconds * 30 */
extern s32 BtlSuper_GetChargeLimitFrames(BtlSuperChr *chr, s32 cls);  /* charge time: seconds * 30 */
extern s32 BtlSuper_GetDamage(BtlSuperChr *chr, s32 cls, s32 a2, s32 a3); /* damage */
extern s32 BtlSuper_GetKiCost(BtlSuperChr *chr, s32 cls);  /* ki cost (halved with ability 0x2B) */
extern s32 BtlSuper_IsThrow(BtlSuperChr *chr, s32 cls);  /* one of the technique's four reactions is 0x22 (a catch) */
/* Animation event query on the object: mode 0 = frame of the first event with `mask`, 3 = how many. */
extern s32 BtlObjAnim_QueryEvent(BtlSuperObj *obj, u64 mask, s32 layer, s32 mode);
extern void BtlObj_SetColorMode(BtlSuperObj *obj, s32 bit, s32 on);

/* Requests the camera cut of a technique: cut 0, 1 or 2 for class 2, 3 or 4. */
void BtlSuper_RequestCut(BtlSuperChr *chr, s32 cls) {
    s32 cut = 0;

    switch (cls) {
    case 2:
        cut = 0;
        break;
    case 3:
        cut = 1;
        break;
    case 4:
        cut = 2;
        break;
    }
    ChrCam_RequestCut(chr, 0, cut);
}

/* Raises the start effect bit of a technique: 0x3E, 0x3F or 0x40 by class. */
void BtlSuper_SetStartFx(BtlSuperChr *chr, s32 cls) {
    s32 bit = 0;

    switch (cls) {
    case 2:
        bit = 0x3E;
        break;
    case 3:
        bit = 0x3F;
        break;
    case 4:
        bit = 0x40;
        break;
    }
    BtlChar_SetFxBit(chr, bit);
}

/* Raises flag 0xE0, 0xE1 or 0xE2 by class; class 4 only when `force` is set. */
void BtlSuper_SetClassFlag(BtlSuperChr *chr, s32 cls, s32 force) {
    s32 flag = 0;

    if (force == 0 && cls == 4) {
        return;
    }
    switch (cls) {
    case 2:
        flag = 0xE0;
        break;
    case 3:
        flag = 0xE1;
        break;
    case 4:
        flag = 0xE2;
        break;
    }
    BtlChar_SetFlag(chr, flag);
}

/* Requests hit-stop for this frame: level 2 (flag 0x125) with the full flag set, or level 1 (flag 0x126). */
void BtlSuper_RequestHitStop(BtlSuperChr *chr, s32 light) {
    if (light) {
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x126);
    } else {
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 0x125);
        BtlChar_SetFlag(chr, 0x12A);
    }
    BtlChar_SetFlag(chr, 0x128);
}

/* Per-frame flags of a technique in progress: 0xD5, 0xBB, the hit-stop exemption 0x127, and held flag 0xE. */
void BtlSuper_SetFiringFlags(BtlSuperChr *chr) {
    BtlChar_SetFlag(chr, 0xD5);
    BtlChar_SetFlag(chr, 0xBB);
    BtlChar_SetFlag(chr, 0x127);
    if (!BtlChar_TestFlag(chr, 0xE) && !BtlChar_TestFlag(chr, 0xF)) {
        BtlChar_SetHeldFlag(chr, 0xE);
    }
}

/* Enables the technique's button command for the next input frame: fighter + 0x1594 = class (BtlInput_TestSwitch
   then sets command bit 0x1000, which input condition 0x6F reads). */
void BtlSuper_SetClass(BtlSuperChr *chr, s32 cls) {
    chr->inputClass = cls;
}

/* Per-frame flags of the cinematic part of a technique. */
void BtlSuper_SetRushFlags(BtlSuperChr *chr) {
    BtlChar_SetFlag(chr, 0xD5);
    BtlChar_SetFlag(chr, 0x1A);
    BtlChar_SetFlag(chr, 0x3F);
    BtlChar_SetFlag(chr, 0xBB);
    BtlChar_SetFlag(chr, 0x128);
    BtlChar_SetFlag(chr, 0x18);
    BtlChar_SetFlag(chr, 0x12D);
}

/* Camera shake and rumble of a technique: at the animation's 0x400 event (kind 0) or every frame (kind 1). */
void BtlSuper_ShakeOnEvent(BtlSuperChr *chr, s32 cls, s32 kind) {
    BtlSuperObj *obj = BtlChar_GetObj(chr);
    u32 flags = BtlSuper_GetFlagsA(chr, cls);

    switch (kind) {
    case 0:
        if (flags & 0x4000000) {
            s32 n = BtlObjAnim_QueryEvent(obj, 0x400, 0, 3);

            if (n == 1) {
                s32 frame = BtlObjAnim_QueryEvent(obj, 0x400, 0, 0) - 4;

                if (frame < 0) {
                    frame = 0;
                }
                if (BtlAnim_PassedFrame(chr, frame)) {
                    if (flags & 0x10000000) {
                        BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 10.0f, 0.2f);
                        BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 1.0f, 0.3f);
                    } else {
                        ChrCam_AddShake(chr, 10.0f, 0.2f);
                        BtlChar_Vibrate(chr, 1.0f, 0.3f);
                    }
                }
            } else if (n >= 2) {
                if (BtlAnim_TestAttr(chr, 0x400)) {
                    if (flags & 0x10000000) {
                        BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 10.0f, 0.1f);
                        BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 1.0f, 0.2f);
                    } else {
                        ChrCam_AddShake(chr, 10.0f, 0.1f);
                        BtlChar_Vibrate(chr, 1.0f, 0.2f);
                    }
                }
            }
        }
        break;
    case 1:
        if (flags & 0x8000000) {
            if (flags & 0x10000000) {
                BtlCharApi_ShakeCamsNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 3.0f, 0.1f);
                BtlCharApi_RumbleNear(&BtlChar_GetPos(chr)->pos, 100.0f, 1000.0f, 1.0f, 0.1f);
            } else {
                ChrCam_AddShake(chr, 3.0f, 0.1f);
                BtlChar_Vibrate(chr, 1.0f, 0.1f);
            }
        }
        break;
    }
}

/* Stretches the animation so its 0x400 event falls `seconds` from now (0.1 s animation when it has none). */
void BtlSuper_FitAnimToEvent(BtlSuperChr *chr, f32 seconds) {
    f32 event = BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 0x400, 0, 0);
    f32 frame;
    f32 step;

    if (event < 0.0f) {
        BtlAnim_SetDuration(chr, 0.1f);
        return;
    }
    frame = BtlAnim_GetFrame(chr);
    if (event - 2.0f < frame) {
        return;
    }
    if (seconds < 0.033333335f) {
        step = event - frame;
    } else {
        f32 rest = event - frame;

        step = event / (seconds * 30.0f);
        if (rest < step) {
            step = rest;
        }
    }
    BtlAnim_SetStep(chr, step);
}

/* Forwards 0.2 and 2.0 to BtlAct_SetObjUnk. */
void BtlSuper_SetObjUnk(BtlSuperChr *chr) {
    BtlAct_SetObjUnk(chr, 0.2f, 2.0f);
}

/* Charge rumble: power 0.8 for 0.1 s. */
void BtlSuper_VibrateCharge(BtlSuperChr *chr) {
    BtlChar_SetVibration(chr, 0.8f, 0.1f);
}

/* Recoil while firing: pushes the fighter backwards, always for kinds 0 and 9, only on the fire animation for
   kinds 1..4, 6 and 8, never for 5 and 7. */
void BtlSuper_Recoil(BtlSuperChr *chr, s32 cls, s32 loop) {
    s32 push = 0;

    switch (BtlSuper_GetHitDirKind(chr, cls)) {
    case 0:
    case 9:
        push = 1;
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
    case 8:
        push = loop == 0;
        break;
    case 5:
    case 7:
        break;
    }
    if (push) {
        BtlAct_PushAngle(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y + 3.14159265f), 1.0f, 5.0f);
    }
}

/* Plays step `step` of a rush technique's animation chain on the sub layer and adds up what it contains: hits,
   total length and the frames of its 0x20000 / 0x40000 events. Called once per frame with the action frame. */
void BtlSuper_MeasureRushStep(BtlSuperChr *chr, s32 cls, s32 step) {
    s32 count = BtlSuper_GetStepCount(chr, cls);
    BtlSuperObj *obj;

    if (step < count) {
        obj = BtlChar_GetObj(chr);
        if (step == 0) {
            chr->rushHits = 0;
            chr->rushFrameA = 0;
            chr->rushFrameB = 0;
            chr->rushStepA = -1;
            chr->rushStepB = -1;
            chr->rushFrames = 0;
        }
        BtlAnim_PlaySub(chr, step + cls * 0x22 + 0xD8);
        if (obj->subAnim != NULL) {
            chr->rushHits += BtlObjAnim_QueryEvent(obj, 0x200000000, 1, 3);
            if (chr->rushStepA < 0) {
                if (BtlObjAnim_QueryEvent(obj, 0x20000, 1, 3) > 0) {
                    chr->rushFrameA += BtlObjAnim_QueryEvent(obj, 0x20000, 1, 0);
                    chr->rushStepA = step;
                } else {
                    chr->rushFrameA = chr->rushFrameA + (obj->subAnim->frames + 1.0f);
                }
            }
            if (chr->rushStepB < 0) {
                if (BtlObjAnim_QueryEvent(obj, 0x40000, 1, 3) > 0) {
                    chr->rushFrameB += BtlObjAnim_QueryEvent(obj, 0x40000, 1, 2);
                    chr->rushStepB = step;
                } else {
                    chr->rushFrameB = chr->rushFrameB + (obj->subAnim->frames + 1.0f);
                }
            }
            chr->rushFrames += obj->subAnim->frames;
            if (step < count - 1) {
                chr->rushFrames = chr->rushFrames + 1.0f;
            }
        }
    }
}

/* Start of a technique action: stops vertical motion, clears the technique flags, counts the use and marks the
   fighter (flag 0x29, held flag 0xE). Class 4 only when `force` is set. */
void BtlSuper_Begin(BtlSuperChr *chr, s32 cls, s32 force) {
    if (force == 0 && cls == 4) {
        return;
    }
    BtlChar_GetPos(chr)->unk98 = 0.0f;
    BtlChar_GetPos(chr)->velY = 0.0f;
    BtlChar_ClearFlagRange(chr, 0x9B, 0xA0);
    BtlChar_ClearFlagRange(chr, 0xA7, 0xAA);
    BtlChar_SetFlag(chr, 0x29);
    chr->charge = 0.0f;
    chr->chargeFull = 0;
    BtlMember_GetActiveGauge(chr)->used[cls]++;
    if (!BtlChar_TestFlag(chr, 0xF)) {
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if ((BtlSuper_GetFlags(chr, cls) & 0x8000) && !BtlChar_TestFlag(chr, 0x98)) {
        BtlObj_SetColorMode(BtlChar_GetObj(chr), 2, 1);
    }
}

/* Start sound of a technique (common sound 0x2B). Class 4 only when `force` is set. */
void BtlSuper_PlayStartSound(BtlSuperChr *chr, s32 cls, s32 force) {
    if (force == 0 && cls == 4) {
        return;
    }
    BtlCharSnd_PlayCommon(chr, 0x2B);
}

/* Leave phase of every technique action: unless another technique action follows, starts the cooldown at
   + 0xE40, clears the effect requests 0xA7..0xAA and the input lock 0x11F, and undoes object bit 2. */
void BtlSuper_Leave(BtlSuperChr *chr, s32 cls) {
    BtlSuperObj *obj;

    if (BtlAct_IsTechniqueId(BtlAct_GetRequested(chr))) {
        return;
    }
    chr->cooldown = BtlSuper_GetCooldownFrames(chr, cls);
    BtlChar_ClearFlagRange(chr, 0xA7, 0xAA);
    BtlChar_ClearFlag(chr, 0x11F);
    if ((BtlSuper_GetFlags(chr, cls) & 0x8000) && !BtlChar_TestFlag(chr, 0x98)) {
        obj = BtlChar_GetObj(chr);
        if (obj->flags & 0x40000) {
            BtlObj_SetColorMode(obj, 2, 0);
        }
    }
    switch (BtlSuper_GetId(chr, cls)) {
    case 0x268:
        chr->unkE5C = 0;
        break;
    case 0x2CD:
        chr->unkE60 = 0;
        break;
    }
}

/* Fills the damage queue (fighter + 0xD94) of a rush technique: total damage, damage per hit, the health drain
   and its frame window, from the technique parameters and the measured animation chain. */
void BtlSuper_SetupRushDamage(BtlSuperChr *chr, s32 cls, s32 fromAnim) {
    Vec4 v;
    Vec4 rot;
    s32 damage;
    s32 drain;
    s32 from;
    s32 to;
    s32 tmp;
    s32 rem;
    s32 immune;

    BtlChar_SetFlag(chr, 0xA1);
    chr->dmgHits = 0;
    chr->dmgUnk0 = 0;
    chr->dmgTotal = 0;
    chr->dmgPerHit = 0;
    chr->dmgFlags = 0;
    chr->dmgUnk14 = 0;
    chr->drainHealth = 0;
    chr->drainHealthStep = 0;
    chr->dmgUnk20 = 0;
    chr->drainKi = 0;
    chr->drainKiStep = 0;
    chr->drainFrom = -1;
    chr->drainTo = -1;
    if (!(BtlSuper_GetFlags(chr, cls) & 0x1000000)) {
        damage = BtlSuper_GetDamage(chr, cls, 0, 1);
        rem = damage % 10;
        if (rem) {
            damage = damage - rem + 10;
        }
        if (BtlChar_TestFlag(chr, 0xA0)) {
            damage += damage / 2;
        }
        if (chr->unkEE8) {
            damage /= 2;
        }
        if (fromAnim) {
            chr->dmgHits = BtlObjAnim_QueryEvent(BtlChar_GetObj(chr), 0x200000000, 0, 3);
        } else {
            chr->dmgHits = chr->rushHits;
        }
        chr->dmgUnk0 = damage;
        chr->dmgTotal = damage;
        chr->dmgPerHit = damage / (chr->dmgHits + 5);
        chr->dmgFlags = 0x180;
        switch (cls) {
        case 2:
            chr->dmgFlags = 0x800180;
            break;
        case 3:
            chr->dmgFlags = 0x1000180;
            break;
        case 4:
            chr->dmgFlags = 0x2000180;
            break;
        }
        chr->dmgPerHit = BtlUtil_Min(chr->dmgPerHit, 15000);
        rem = chr->dmgPerHit % 10;
        if (rem) {
            chr->dmgPerHit = chr->dmgPerHit - rem + 10;
        }
        immune = BtlOpp_GetParamWord0(chr) & 0x80;
        if (!immune) {
            drain = BtlSuper_GetDrain(chr, cls);
            if (drain > 0) {
                from = chr->rushFrameA;
                to = chr->rushFrameB;
                from = BtlUtil_Min(from, chr->rushFrames - 30);
                to = BtlUtil_Min(to, chr->rushFrames - 30);
                if (to < from) {
                    s32 swap = from;

                    from = to;
                    to = swap;
                }
                chr->dmgUnk14 = drain;
                chr->drainHealth = drain;
                tmp = (to - from) / 2;
                if (tmp <= immune) {
                    tmp = 1;
                }
                chr->drainHealthStep = drain / tmp + 1;
                chr->drainFrom = from;
                chr->drainTo = to;
            }
        }
        if (BtlSuper_GetFlags(chr, cls) & 0x400) {
            chr->dmgPerHit = 0;
            chr->dmgFlags |= 0x200;
            if (chr->unkEC4) {
                chr->dmgUnk0 = 1;
                chr->dmgTotal = 1;
                chr->dmgFlags |= 3;
            }
        }
    }
    if (BtlSuper_GetFlags(chr, cls) & 0x200000) {
        BtlOpp_GetTargetRot(chr, &rot);
        BtlMove_SetHeading(chr, rot.y, 0.0f);
        BtlChar_SetFlag(chr, 0x9B);
        BtlOpp_GetObjVecFA0(chr, &v);
        BtlChar_SetUnk1310(chr, &v);
    }
    if (BtlSuper_GetId(chr, cls) == 0x2EF) {
        if (!(BtlOpp_GetParamWord0(chr) & 0x80)) {
            chr->unkE60 = BtlUtil_Clamp(chr->unkE60 + 5, 0, 10);
        }
    }
}

/* Ends a technique: back to the neutral action 0xB, or to action 0xD9 when the technique has flag 0x4000. */
void BtlSuper_Finish(BtlSuperChr *chr, s32 cls) {
    if (BtlSuper_GetFlags(chr, cls) & 0x4000) {
        BtlChar_SetHeldFlag(chr, 0x1D);
        BtlChar_SetHeldFlag(chr, 0x1E);
        BtlChar_SetFlag(chr, 0x35);
        BtlAct_Request(chr, 0xD9);
    } else {
        BtlAct_Request(chr, 0xB);
    }
}

/* Animation ids of a class 2..4 technique: 0x22 per class, starting at 0x105. */
#define SUPER_ANIM(cls, n) ((cls) * 0x22 + 0xC1 + (n))

/*
 * Actions 0x106..0x108: a beam technique. Start animation (level 2 hit-stop) -> charge loop until the effect
 * raises flag 0xA7 (150 frames at most) -> fire -> firing loop until flag 0xA8 (150 frames at most) -> end.
 */
void BtlAct_SuperBeamHandler(BtlSuperChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 cls = BtlAct_GetCurrentClass(chr);
    u32 flagsB = BtlSuper_GetFlags(chr, cls);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(cls, 0), 0.0f);
        BtlSuper_Begin(chr, cls, 0);
        BtlSuper_PlayStartSound(chr, cls, 0);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 5, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_SetStartFx(chr, cls);
        BtlSuper_SetClassFlag(chr, cls, 0);
        BtlSuper_RequestCut(chr, cls);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, cls), 1);
        BtlChar_ResetLook(chr);
        if ((flagsB & 0x2000) && BtlMember_GetActiveGauge(chr)->variant == 0
#ifdef PORT
            && !(PortGame_IsBT4() && BtlMember_HasAbility(chr,0x7A))
#endif
            ) {
            chr->work[0] |= 1;
        }
    }
    if (phase == 1) {
        BtlSuperObj *obj = BtlChar_GetObj(chr);
        s32 aim = 0;

        if (BtlSuper_GetFlagsA(chr, cls) & 0x800) {
            aim = 1;
        }
        if (chr->actionFrame == 1 && (chr->work[0] & 1)) {
            BtlChar_SavePlacement(chr);
            BtlAct_SetFormCurrent(chr);
            BtlChange_RequestChara(chr->player, chr->formChara, chr->formCostume, chr->form14, chr->form18,
                                   chr->form1C, chr->form20);
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            aim = 1;
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 1), 0.0f, 0);
            BtlSuper_RequestHitStop(chr, 0);
            BtlSuper_SetClass(chr, cls);
            break;
        case 0x106:
        case 0x128:
        case 0x14A:
            aim = 1;
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_RequestHitStop(chr, 0);
            BtlSuper_SetClass(chr, cls);
            BtlSuper_SetObjUnk(chr);
            BtlSuper_VibrateCharge(chr);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA7);
            }
            if (BtlChar_TestFlag(chr, 0xA7)) {
                if (chr->work[0] & 1) {
                    if (BtlChange_IsLoadedFor(chr->player)) {
                        BtlChange_SetReady(chr->player);
                        BtlAnim_Request(chr, SUPER_ANIM(cls, 2), 0.0f);
                        *timer = 0;
                    }
                } else {
                    BtlAnim_Request(chr, SUPER_ANIM(cls, 2), 0.0f);
                    *timer = 0;
                }
            }
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 3), 0.0f, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 0);
            BtlSuper_Recoil(chr, cls, 0);
            BtlSuper_SetClass(chr, cls);
            if (BtlAnim_GetFrame(chr) < BtlObjAnim_QueryEvent(obj, 0x400, 0, 0)) {
                aim = 1;
                BtlSuper_RequestHitStop(chr, 1);
            }
            if (BtlAnim_IsNew(chr)) {
                ChrCam_EndCut(chr);
                if (flagsB & 0x2000) {
                    BtlMember_Damage(chr, BtlMember_GetActiveGauge(chr)->health - 1, 0x5B);
                    BtlMember_GetActiveGauge(chr)->fired[cls]++;
                }
                if (chr->work[0] & 1) {
                    BtlChange_SetDone(chr->player);
                    BtlChar_SetFlag(chr, 0xF5);
                    BtlChar_SetFlag(chr, 0x55);
                }
            }
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 1);
            BtlSuper_Recoil(chr, cls, 1);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA8);
            }
            if (BtlChar_TestFlag(chr, 0xA8)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 6), 0.0f);
                *timer = 0;
            }
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        if (BtlSuper_IsThrow(chr, cls)) {
            BtlSuper_MeasureRushStep(chr, cls, chr->actionFrame);
        }
        if (!(flagsB & 0x1000)) {
            BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 7, BtlAnim_GetId(chr) + 0xE, aim);
        }
        if (aim) {
            BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFxBit(chr, 0x10);
        BtlSuper_SetFiringFlags(chr);
        if (BtlInput_TestAction(chr, 0x6F, 1)) {
            BtlChar_SetHeldFlag(chr, 0x9E);
            BtlChar_SetHeldFlag(chr, 0x9D);
            BtlChar_SetHeldFlag(chr, 0x9C);
            if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                BtlCharSnd_PlayCommon(chr, 0x15);
                BtlCharSnd_PlayCommon(chr, 0x1C);
            }
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x73)) {
            BtlAct_Request(chr, cls + 0x128);
        }
        if (BtlChar_TestFlag(chr, 0x72)) {
            BtlAct_Request(chr, cls + 0x12B);
        }
        if (chr->work[1]) {
            BtlSuper_Finish(chr, cls);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, cls);
    }
}

/*
 * Actions 0x109..0x10B: a beam technique fired after a teleport. The charge loop runs five stages after the
 * effect's flag 0xA7: vanish, warp in front of the opponent, reappear, then fire as the plain beam does.
 */
void BtlAct_SuperWarpBeamHandler(BtlSuperChr *chr, s32 phase) {
    s32 *sub = &chr->work[3];
    s32 *stage = &chr->work[4];
    s32 *timer = &chr->work[2];
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(cls, 0), 0.0f);
        BtlSuper_Begin(chr, cls, 0);
        BtlSuper_PlayStartSound(chr, cls, 0);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 5, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_SetStartFx(chr, cls);
        BtlSuper_SetClassFlag(chr, cls, 0);
        BtlSuper_RequestCut(chr, cls);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, cls), 1);
        BtlChar_ResetLook(chr);
    }
    if (phase == 1) {
        s32 warped = 0;
        BtlSuperObj *obj = BtlChar_GetObj(chr);
        s32 aim = 0;
        f32 event;
        f32 frame;

        if (BtlSuper_GetFlagsA(chr, cls) & 0x800) {
            aim = 1;
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            aim = 1;
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 1), 0.0f, 0);
            BtlSuper_RequestHitStop(chr, 0);
            BtlSuper_SetClass(chr, cls);
            break;
        case 0x106:
        case 0x128:
        case 0x14A:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_SetObjUnk(chr);
            BtlSuper_VibrateCharge(chr);
            aim = 1;
            switch (*stage) {
            case 0:
                BtlSuper_RequestHitStop(chr, 0);
                BtlSuper_SetClass(chr, cls);
                if (++*timer > BTL_SUPER_TIMEOUT) {
                    BtlChar_SetHeldFlag(chr, 0xA7);
                }
                if (BtlChar_TestFlag(chr, 0xA7)) {
                    ++*stage;
                    *sub = 0;
                    *timer = 0;
                    BtlChar_SetFxBit(chr, 0xC);
                    BtlCharSnd_PlayCommon(chr, 0x20);
                }
                break;
            case 1:
                BtlSuper_RequestHitStop(chr, 0);
                if (++*sub > 3) {
                    BtlChar_SetFlag(chr, 0xB);
                }
                if (*sub >= 7) {
                    ++*stage;
                    *sub = 0;
                }
                break;
            case 2:
                BtlSuper_RequestHitStop(chr, 0);
                BtlMove_SnapToOpponent(chr);
                BtlChar_SetFlag(chr, 0xB);
                warped = 1;
                BtlChar_SetFlag(chr, 0xCC);
                ++*stage;
                *sub = 0;
                break;
            case 3:
                BtlSuper_RequestHitStop(chr, 0);
                BtlMove_WarpInFrontOfOpponent(chr, 40.0f);
                warped = 1;
                BtlChar_GetPos(chr)->pos.y += 20.0f;
                BtlMove_TurnYaw(chr, 2, 3.14159265f);
                BtlMove_TurnPitch(chr, 1, 3.14159265f);
                BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
                BtlMove_SetDirection(chr, 3);
                BtlChar_SetFlag(chr, 0x3F);
                BtlChar_SetFlag(chr, 0xB);
                BtlChar_SetFxBit(chr, 0xD);
                BtlChar_SetFlag(chr, 0xCC);
                BtlChar_SetFlag(chr, 0xCE);
                BtlChar_SetFlag(chr, 0x24);
                ++*stage;
                *sub = 0;
                break;
            case 4:
                BtlSuper_RequestHitStop(chr, 1);
                if (++*sub == 1) {
                    ChrCam_EndCut(chr);
                }
                if (*sub < 3) {
                    BtlChar_SetFlag(chr, 0xB);
                }
                if (*sub >= 7) {
                    BtlAnim_Request(chr, SUPER_ANIM(cls, 2), 0.0f);
                }
                break;
            }
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 3), 0.0f, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 0);
            BtlSuper_Recoil(chr, cls, 0);
            event = BtlObjAnim_QueryEvent(obj, 0x400, 0, 0);
            frame = BtlAnim_GetFrame(chr);
            if (frame < event - 4.0f) {
                BtlSuper_RequestHitStop(chr, 1);
            }
            if (frame < event) {
                aim = 1;
            }
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 1);
            BtlSuper_Recoil(chr, cls, 1);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA8);
            }
            if (BtlChar_TestFlag(chr, 0xA8)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 6), 0.0f);
                *timer = 0;
            }
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        if (!(BtlSuper_GetFlags(chr, cls) & 0x1000)) {
            BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 7, BtlAnim_GetId(chr) + 0xE, aim);
        }
        if (!warped) {
            if (aim) {
                BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
                BtlMove_ApplyGravity(chr);
            } else {
                BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
                BtlMove_ApplyGravity(chr);
            }
        }
        BtlChar_SetFxBit(chr, 0x10);
        BtlSuper_SetFiringFlags(chr);
        if (BtlInput_TestAction(chr, 0x6F, 1)) {
            BtlChar_SetHeldFlag(chr, 0x9E);
            BtlChar_SetHeldFlag(chr, 0x9D);
            BtlChar_SetHeldFlag(chr, 0x9C);
            if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                BtlCharSnd_PlayCommon(chr, 0x15);
                BtlCharSnd_PlayCommon(chr, 0x1C);
            }
        }
    }
    if (phase == 2) {
        if (chr->work[1]) {
            BtlSuper_Finish(chr, cls);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, cls);
    }
}

/*
 * Actions 0x10C..0x10E: a beam technique with a second firing stage. As the plain beam, but the firing loop
 * (until flag 0xA8) is followed by another animation and loop (until flag 0xA9) before the end animation.
 */
void BtlAct_SuperLongBeamHandler(BtlSuperChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(cls, 0), 0.0f);
        BtlSuper_Begin(chr, cls, 0);
        BtlSuper_PlayStartSound(chr, cls, 0);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 5, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_SetStartFx(chr, cls);
        BtlSuper_SetClassFlag(chr, cls, 0);
        BtlSuper_RequestCut(chr, cls);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, cls), 1);
        BtlChar_ResetLook(chr);
    }
    if (phase == 1) {
        s32 aim = 0;

        if (BtlSuper_GetFlagsA(chr, cls) & 0x800) {
            aim = 1;
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            aim = 1;
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 1), 0.0f, 0);
            BtlSuper_RequestHitStop(chr, 0);
            BtlSuper_SetClass(chr, cls);
            break;
        case 0x106:
        case 0x128:
        case 0x14A:
            aim = 1;
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_RequestHitStop(chr, 0);
            BtlSuper_SetClass(chr, cls);
            BtlSuper_SetObjUnk(chr);
            BtlSuper_VibrateCharge(chr);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA7);
            }
            if (BtlChar_TestFlag(chr, 0xA7)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 2), 0.0f);
                *timer = 0;
            }
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 3), 0.0f, 0);
            BtlSuper_RequestHitStop(chr, 1);
            BtlSuper_ShakeOnEvent(chr, cls, 0);
            BtlSuper_Recoil(chr, cls, 0);
            BtlSuper_SetClass(chr, cls);
            if (BtlAnim_IsNew(chr)) {
                ChrCam_EndCut(chr);
            }
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_RequestHitStop(chr, 1);
            BtlSuper_ShakeOnEvent(chr, cls, 1);
            BtlSuper_Recoil(chr, cls, 1);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA8);
            }
            if (BtlChar_TestFlag(chr, 0xA8)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 4), 0.0f);
                *timer = 0;
            }
            break;
        case 0x109:
        case 0x12B:
        case 0x14D:
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 5), 0.0f, 0);
            break;
        case 0x10A:
        case 0x12C:
        case 0x14E:
            BtlAnim_AdvanceLoop(chr, 0);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA9);
            }
            if (BtlChar_TestFlag(chr, 0xA9)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 6), 0.0f);
                *timer = 0;
            }
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        if (!(BtlSuper_GetFlags(chr, cls) & 0x1000)) {
            BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 7, BtlAnim_GetId(chr) + 0xE, aim);
        }
        if (aim) {
            BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFxBit(chr, 0x10);
        BtlSuper_SetFiringFlags(chr);
        if (BtlInput_TestAction(chr, 0x6F, 1)) {
            BtlChar_SetHeldFlag(chr, 0x9E);
            BtlChar_SetHeldFlag(chr, 0x9D);
            BtlChar_SetHeldFlag(chr, 0x9C);
            if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                BtlCharSnd_PlayCommon(chr, 0x15);
                BtlCharSnd_PlayCommon(chr, 0x1C);
            }
        }
    }
    if (phase == 2) {
        if (chr->work[1]) {
            BtlSuper_Finish(chr, cls);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, cls);
    }
}

/*
 * Actions 0x10F..0x111: a chargeable technique. The charge loop lasts until the button is released (input
 * condition 0x6F) or the technique's charge time is over; fighter + 0xE44 is the charge level, full after half
 * the charge time (a third with ability 0x4A).
 */
void BtlAct_SuperChargeHandler(BtlSuperChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(cls, 0), 0.15f);
        BtlSuper_Begin(chr, cls, 0);
        BtlSuper_PlayStartSound(chr, cls, 0);
        BtlSuper_SetStartFx(chr, cls);
        BtlSuper_SetClassFlag(chr, cls, 0);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, cls), 1);
    }
    if (phase == 1) {
        BtlSuperObj *obj = BtlChar_GetObj(chr);
        s32 limit = BtlSuper_GetChargeLimitFrames(chr, cls);
        s32 aim = 0;

        if (BtlSuper_GetFlagsA(chr, cls) & 0x800) {
            aim = 1;
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            aim = 1;
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 1), 0.0f, 0);
            BtlSuper_SetClass(chr, cls);
            BtlChar_SetFlag(chr, 0x53);
            break;
        case 0x106:
        case 0x128:
        case 0x14A:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_SetObjUnk(chr);
            BtlSuper_VibrateCharge(chr);
            aim = 1;
            BtlSuper_SetClass(chr, cls);
            BtlChar_SetFlag(chr, 0x53);
            if (++*timer >= limit) {
                chr->work[0] |= 1;
            }
            chr->charge = *timer * 2.0f / limit;
            if (BtlMember_HasAbility(chr, 0x4A)) {
                chr->charge *= 1.5f;
            }
            if (chr->charge >= 1.0f) {
                chr->charge = 1.0f;
                BtlChar_SetFxBit(chr, 0x1E);
                if (chr->chargeFull == 0) {
                    BtlCharSnd_PlayCommon(chr, 0x14);
                    BtlCharSnd_PlayCommon(chr, 0x15);
                    BtlCharSnd_PlayCommon(chr, 0x1C);
                }
                chr->chargeFull++;
            }
            if (chr->work[0] & 1) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 2), 0.0f);
                *timer = 0;
            }
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 3), 0.0f, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 0);
            BtlSuper_Recoil(chr, cls, 0);
            BtlChar_SetFlag(chr, 0x53);
            if (BtlAnim_GetFrame(chr) < BtlObjAnim_QueryEvent(obj, 0x400, 0, 0)) {
                aim = 1;
            }
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 1);
            BtlSuper_Recoil(chr, cls, 1);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA8);
            }
            if (BtlChar_TestFlag(chr, 0xA8)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 6), 0.0f);
                *timer = 0;
            }
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        if (!(BtlSuper_GetFlags(chr, cls) & 0x1000)) {
            BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 7, BtlAnim_GetId(chr) + 0xE, aim);
        }
        if (aim) {
            BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFxBit(chr, 0x10);
        BtlSuper_SetFiringFlags(chr);
        BtlChar_SetFlag(chr, 0xA2);
    }
    if (phase == 2) {
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            break;
        case 0x106:
        case 0x128:
        case 0x14A:
            if (BtlInput_TestAction(chr, 0x6F, 1)) {
                BtlChar_SetHeldFlag(chr, 0x9E);
                BtlChar_SetHeldFlag(chr, 0x9D);
                chr->work[0] |= 1;
                if (chr->charge >= 1.0f) {
                    BtlChar_SetHeldFlag(chr, 0x9C);
                }
            }
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            break;
        }
        if (chr->work[1]) {
            BtlSuper_Finish(chr, cls);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, cls);
    }
}

/* Whether no further shot may be fired: 1 health left, or the technique's shot limit reached. */
static inline s32 BtlSuper_IsLastShot(BtlSuperChr *chr, s32 cls, s32 *shots) {
    if (BtlMember_GetActiveGauge(chr)->health == 1) {
        return 1;
    }
    if (*shots < BtlSuper_GetShots(chr, cls)) {
        return 0;
    }
    return 1;
}

/*
 * Actions 0x112..0x114: a technique fired in repeated shots. Every shot costs health (not ki); while the fire
 * loop runs, the button fires the next shot until the technique's shot limit, its time limit or 1 health.
 */
void BtlAct_SuperRepeatHandler(BtlSuperChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 *shots = &chr->work[3];
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(cls, 0), 0.0f);
        BtlSuper_Begin(chr, cls, 0);
        BtlSuper_PlayStartSound(chr, cls, 0);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 5, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_SetStartFx(chr, cls);
        BtlSuper_SetClassFlag(chr, cls, 0);
        BtlSuper_RequestCut(chr, cls);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, cls), 1);
        BtlChar_ResetLook(chr);
    }
    if (phase == 1) {
        BtlSuperObj *obj = BtlChar_GetObj(chr);
        s32 aim = 0;

        if (BtlSuper_GetFlagsA(chr, cls) & 0x800) {
            aim = 1;
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 1), 0.0f, 0);
            BtlSuper_RequestHitStop(chr, 0);
            BtlSuper_SetClass(chr, cls);
            if (BtlInput_TestAction(chr, 0x6F, 1)) {
                BtlChar_SetHeldFlag(chr, 0x9E);
                BtlChar_SetHeldFlag(chr, 0x9D);
                BtlChar_SetHeldFlag(chr, 0x9C);
                if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                    BtlCharSnd_PlayCommon(chr, 0x14);
                    BtlCharSnd_PlayCommon(chr, 0x15);
                    BtlCharSnd_PlayCommon(chr, 0x1C);
                }
            }
            aim = 1;
            break;
        case 0x106:
        case 0x128:
        case 0x14A:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_RequestHitStop(chr, 0);
            BtlSuper_SetObjUnk(chr);
            BtlSuper_VibrateCharge(chr);
            BtlSuper_SetClass(chr, cls);
            if (BtlInput_TestAction(chr, 0x6F, 1)) {
                BtlChar_SetHeldFlag(chr, 0x9E);
                BtlChar_SetHeldFlag(chr, 0x9D);
                BtlChar_SetHeldFlag(chr, 0x9C);
                if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                    BtlCharSnd_PlayCommon(chr, 0x14);
                    BtlCharSnd_PlayCommon(chr, 0x15);
                    BtlCharSnd_PlayCommon(chr, 0x1C);
                }
            }
            aim = 1;
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA7);
            }
            if (BtlChar_TestFlag(chr, 0xA7)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 2), 0.0f);
                *timer = 0;
            }
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 3), 0.0f, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 0);
            BtlSuper_Recoil(chr, cls, 0);
            if (BtlAnim_GetFrame(chr) < BtlObjAnim_QueryEvent(obj, 0x400, 0, 0)) {
                aim = 1;
                BtlSuper_RequestHitStop(chr, 1);
            }
            if (BtlAnim_IsNew(chr)) {
                BtlMember_Damage(chr, BtlSuper_GetRecoilDamage(chr, cls), 0x42B);
                ++*shots;
                *timer = 0;
                ChrCam_EndCut(chr);
                if (*shots == 1) {
                    BtlChar_ClearFlag(chr, 0x9E);
                    BtlChar_ClearFlag(chr, 0x9D);
                }
            }
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 1);
            BtlSuper_Recoil(chr, cls, 1);
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        if (!(BtlSuper_GetFlags(chr, cls) & 0x1000)) {
            BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 7, BtlAnim_GetId(chr) + 0xE, aim);
        }
        if (aim) {
            BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFxBit(chr, 0x10);
        BtlSuper_SetFiringFlags(chr);
        BtlChar_SetFlag(chr, 0xA3);
    }
    if (phase == 2) {
        s32 stop;

        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            break;
        case 0x106:
        case 0x128:
        case 0x14A:
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            if (!BtlSuper_IsLastShot(chr, cls, shots)) {
                BtlSuper_SetClass(chr, cls);
            }
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            ++*timer;
            if (*timer >= (s32)(BtlSuper_GetShotTime(chr, cls) * 30.0f) - 1) {
                stop = 1;
            } else if (BtlMember_GetActiveGauge(chr)->health == 1) {
                stop = 1;
            } else if (*shots < BtlSuper_GetShots(chr, cls)) {
                stop = 0;
            } else {
                stop = 1;
            }
            if (stop) {
                if (BtlMove_CountOwnBeams(chr) <= 0) {
                    BtlAnim_Request(chr, SUPER_ANIM(cls, 6), 0.0f);
                    *timer = 0;
                }
            } else {
                BtlSuper_SetClass(chr, cls);
                if (*timer >= 16) {
                    if (BtlInput_TestAction(chr, 0x6F, 1)) {
                        BtlAnim_Request(chr, SUPER_ANIM(cls, 2), 0.0f);
                    }
                }
            }
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            break;
        }
        if (chr->work[1]) {
            BtlSuper_Finish(chr, cls);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, cls);
    }
}

/*
 * Actions 0x115..0x117: the quick form of the beam technique (flag 0xA4: 30% damage). No ki cost, no hit-stop
 * and no charge: the start animation is cut to 0.1 s and leads straight to the fire animation.
 */
void BtlAct_SuperQuickBeamHandler(BtlSuperChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(cls, 0), 0.0f);
        BtlSuper_Begin(chr, cls, 1);
        BtlSuper_SetStartFx(chr, cls);
        BtlChar_SetHeldFlag(chr, 0xE);
        if (BtlAct_GetPrev(chr) == 0x2E) {
            BtlChar_SetFlag(chr, 0xCD);
        }
    }
    if (phase == 1) {
        BtlSuperObj *obj = BtlChar_GetObj(chr);
        s32 aim = 0;

        if (BtlSuper_GetFlagsA(chr, cls) & 0x800) {
            aim = 1;
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            BtlAnim_SetDuration(chr, 0.1f);
            aim = 1;
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 2), 0.0f, 0);
            chr->charge = 0.0f;
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            BtlSuper_FitAnimToEvent(chr, 0.1f);
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 3), 0.0f, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 0);
            BtlSuper_Recoil(chr, cls, 0);
            if (BtlAnim_GetFrame(chr) < BtlObjAnim_QueryEvent(obj, 0x400, 0, 0)) {
                aim = 1;
            }
            break;
        case 0x108:
        case 0x12A:
        case 0x14C:
            BtlAnim_AdvanceLoop(chr, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 1);
            BtlSuper_Recoil(chr, cls, 1);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA8);
            }
            if (BtlChar_TestFlag(chr, 0xA8)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 6), 0.0f);
                *timer = 0;
            }
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        if (!(BtlSuper_GetFlags(chr, cls) & 0x1000)) {
            BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 7, BtlAnim_GetId(chr) + 0xE, aim);
        }
        if (aim) {
            BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0xA4);
    }
    if (phase == 2) {
        if (chr->work[1]) {
            BtlSuper_Finish(chr, cls);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, cls);
    }
}

/*
 * Actions 0x118..0x11A: the quick form of the two-stage beam technique (flag 0xA4: 30% damage): start cut to
 * 0.1 s -> fire -> second stage and its loop until flag 0xA9 (150 frames at most) -> end.
 */
void BtlAct_SuperQuickLongBeamHandler(BtlSuperChr *chr, s32 phase) {
    s32 *timer = &chr->work[2];
    s32 cls = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(cls, 0), 0.0f);
        BtlSuper_Begin(chr, cls, 0);
        BtlSuper_SetStartFx(chr, cls);
        BtlChar_SetHeldFlag(chr, 0xE);
        if (BtlAct_GetPrev(chr) == 0x2E) {
            BtlChar_SetFlag(chr, 0xCD);
        }
    }
    if (phase == 1) {
        s32 aim = 0;

        if (BtlSuper_GetFlagsA(chr, cls) & 0x800) {
            aim = 1;
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x105:
        case 0x127:
        case 0x149:
            BtlAnim_SetDuration(chr, 0.1f);
            aim = 1;
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 2), 0.0f, 0);
            chr->charge = 0.0f;
            break;
        case 0x107:
        case 0x129:
        case 0x14B:
            BtlSuper_FitAnimToEvent(chr, 0.1f);
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 4), 0.0f, 0);
            BtlSuper_ShakeOnEvent(chr, cls, 0);
            BtlSuper_Recoil(chr, cls, 0);
            if (BtlAnim_IsNew(chr)) {
                ChrCam_EndCut(chr);
            }
            break;
        case 0x109:
        case 0x12B:
        case 0x14D:
            BtlSuper_FitAnimToEvent(chr, 0.1f);
            BtlAnim_AdvanceThen(chr, SUPER_ANIM(cls, 5), 0.0f, 0);
            break;
        case 0x10A:
        case 0x12C:
        case 0x14E:
            BtlAnim_AdvanceLoop(chr, 0);
            if (++*timer > BTL_SUPER_TIMEOUT) {
                BtlChar_SetHeldFlag(chr, 0xA9);
            }
            if (BtlChar_TestFlag(chr, 0xA9)) {
                BtlAnim_Request(chr, SUPER_ANIM(cls, 6), 0.0f);
                *timer = 0;
            }
            break;
        case 0x10B:
        case 0x12D:
        case 0x14F:
            if (BtlAnim_Advance(chr, 0)) {
                chr->work[1] = 1;
            }
            break;
        }
        if (!(BtlSuper_GetFlags(chr, cls) & 0x1000)) {
            BtlAct_SetPitchMotion(chr, BtlAnim_GetId(chr) + 7, BtlAnim_GetId(chr) + 0xE, aim);
        }
        if (aim) {
            BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        } else {
            BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
            BtlMove_ApplyGravity(chr);
        }
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0xA4);
    }
    if (phase == 2) {
        if (chr->work[1]) {
            BtlSuper_Finish(chr, cls);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, cls);
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly btl_act_g.c), written against its own view of the fighter. Functions already declared above with
 * the first part's types are reached through cast macros.
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/btl_act_g.h"

/*
 * Blast 2 / Ultimate Blast actions, second part: 0x1F8C00..0x1FC2B0 (same helpers as the first part, the float
 * pool runs on from 0x2FDDAC).
 *
 * `slot` (BtlAct_GetCurrentClass: 2 and 3 are the two Blast 2 moves, 4 the Ultimate Blast) selects the technique's
 * parameters and its block of 0x22 animations starting at 0x105 + (slot - 2) * 0x22 (SUPER_ANIM below).
 *
 *   0x11B..0x11D  BtlAct_SuperRushDashHandler      rush technique: start-up, then a homing dash at the opponent
 *   0x11E..0x120  (the same handler)               the same, dashing straight ahead (technique kind 6)
 *   0x121..0x123  BtlAct_SuperRushStrikeHandler    rush technique struck from where the fighter stands (kind 10)
 *   0x124..0x126  BtlAct_SuperRushFlyHandler       a dash the player steers; hits on the way, no sequence (kind 8)
 *   0x127..0x129  BtlAct_SuperRushFollowHandler    the same dash without start-up, ki cost or camera cut
 *   0x12A..0x12C  BtlAct_SuperRushFinishHandler    closing animation of the attacker
 *   0x12D..0x12F  BtlAct_SuperRushSequenceHandler  the scripted hit sequence, attacker
 *   0x139..0x13B  (the same handler)               the scripted hit sequence, victim (hit reaction 0x22)
 *   0x133..0x135  BtlAct_SuperRushCaughtHandler    victim of a catch that has no sequence (hit reactions 0x1D, 0x1E)
 *   0x136..0x138  BtlAct_SuperLaunchedUpHandler    victim thrown straight up (hit reaction 0x29)
 *   0x130..0x132  BtlAct_ClashStruggleHandler      both fighters during clash A (beam against beam, rush against rush)
 *   0x104         BtlAct_ClashLostHandler          the loser of the struggle / "hit by a finishing technique" (flag 0xAB)
 *   0x105         BtlAct_UltimateStartHandler      pose in front of an Ultimate Blast (queued before the technique)
 *   0x103         BtlAct_ChangeRandomCharaHandler  the fighter is replaced by one of six fixed characters
 *
 * chr->work[0] is a bit set, work[1] the "finished" mark the decide phase waits for, work[2] / work[3] counters or
 * step numbers. The game terms are a reading of the code; what the matching C verifies is the sequence of
 * animations, flags and calls.
 *
 * Return types: a handler whose last call is a tail call in the original is written `void`; one whose last call
 * is followed by the epilogue is written `s32` without a return statement (see btl_char_action.h).
 *
 * The last two functions are helpers of the throw handlers in btl_act_h.c (their only callers); the original
 * object boundary is therefore probably at 0x1FC008, not 0x1FC2B0.
 */

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)

/* Animation n of a slot 2..4 technique: 0 start, 1 second start, 2 dash start / strike, 3 dash loop, 6 end. */
#define SUPER_ANIM(slot, n) ((slot) * 0x22 + 0xC1 + (n))

extern f32 Vec3_Length(Vec4 *v);
extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_MinF(f32 a, f32 b);
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);

#define BtlChar_GetObj ((ActGObj *(*)(ActGChr *chr))BtlChar_GetObj)
#define BtlChar_GetPos ((ActGPose *(*)(ActGChr *chr))BtlChar_GetPos)
#define BtlChar_TestFlag ((s32 (*)(ActGChr *chr, s32 bit))BtlChar_TestFlag)
#define BtlChar_SetFlag ((void (*)(ActGChr *chr, s32 bit))BtlChar_SetFlag)
#define BtlChar_SetHeldFlag ((void (*)(ActGChr *chr, s32 bit))BtlChar_SetHeldFlag)
#define BtlChar_ClearFlag ((void (*)(ActGChr *chr, s32 bit))BtlChar_ClearFlag)
#define BtlChar_IsFlagRaised ((s32 (*)(ActGChr *chr, s32 bit))BtlChar_IsFlagRaised)
#define BtlChar_SetFxBit ((void (*)(ActGChr *chr, s32 bit))BtlChar_SetFxBit)
#define BtlCharSnd_PlayCommon ((void (*)(ActGChr *chr, s32 id))BtlCharSnd_PlayCommon)
#define BtlChar_Vibrate ((void (*)(ActGChr *chr, f32 power, f32 seconds))BtlChar_Vibrate)
extern void BtlChar_SetSmallVibration(ActGChr *chr, f32 seconds);
#define BtlChar_ResetLook ((void (*)(ActGChr *chr))BtlChar_ResetLook)
#define ChrCam_EndCut ((void (*)(ActGChr *chr))ChrCam_EndCut)
#define ChrCam_RequestCut ((void (*)(ActGChr *chr, s32 arg1, s32 arg2))ChrCam_RequestCut)
#define BtlInput_TestAction ((s32 (*)(ActGChr *chr, s32 id, s32 want))BtlInput_TestAction)
#define BtlMember_SpendKi ((void (*)(ActGChr *chr, s32 amount, s32 arg2))BtlMember_SpendKi)
extern f32 BtlOpp_GetGapXZ(ActGChr *chr);
extern void BtlColl_SetFramesToReach(ActGChr *chr, f32 scale);

#define BtlAct_Request ((void (*)(ActGChr *chr, s32 id))BtlAct_Request)
extern s32 BtlAct_GetCurrent(ActGChr *chr);
#define BtlAct_GetCurrentClass ((s32 (*)(ActGChr *chr))BtlAct_GetCurrentClass)
extern s32 BtlAct_GetQueued(ActGChr *chr);
#define BtlAct_GetPrev ((s32 (*)(ActGChr *chr))BtlAct_GetPrev)

#define BtlAnim_Play ((void (*)(ActGChr *chr, s32 anim, f32 blend))BtlAnim_Play)
#define BtlAnim_Request ((void (*)(ActGChr *chr, s32 anim, f32 blend))BtlAnim_Request)
#define BtlAnim_Advance ((s32 (*)(ActGChr *chr, s32 flags))BtlAnim_Advance)
#define BtlAnim_AdvanceThen ((s32 (*)(ActGChr *chr, s32 next, f32 blend, s32 flags))BtlAnim_AdvanceThen)
#define BtlAnim_AdvanceLoop ((void (*)(ActGChr *chr, s32 flags))BtlAnim_AdvanceLoop)
#define BtlAnim_GetId ((s32 (*)(ActGChr *chr))BtlAnim_GetId)
#define BtlAnim_IsNew ((s32 (*)(ActGChr *chr))BtlAnim_IsNew)
extern s32 BtlAnim_WasNew(ActGChr *chr);
extern f32 BtlAnim_GetProgress(ActGChr *chr);

#define BtlMove_Step ((void (*)(ActGChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel))BtlMove_Step)
#define BtlMove_TurnYaw ((void (*)(ActGChr *chr, s32 mode, f32 maxStep))BtlMove_TurnYaw)
#define BtlMove_TurnPitch ((void (*)(ActGChr *chr, s32 mode, f32 maxStep))BtlMove_TurnPitch)
#define BtlMove_TurnModelYaw ((void (*)(ActGChr *chr, f32 maxStep, f32 rate))BtlMove_TurnModelYaw)
#define BtlMove_ApplyGravity ((void (*)(ActGChr *chr))BtlMove_ApplyGravity)
#define BtlMove_SetDirection ((void (*)(ActGChr *chr, s32 mode))BtlMove_SetDirection)
extern void BtlMove_Advance(ActGChr *chr, f32 speed, f32 accel);
extern void BtlMove_SetLeanX(ActGChr *chr, f32 v);
extern void BtlMove_SteerAtOpponent(ActGChr *chr, f32 closeSpeed, f32 yawAccel, f32 pitchAccel, f32 yawMax, f32 pitchMax, f32 maxStep);

/* Rush-technique helpers of the previous file (0x1F5460..0x1F6510). */
#define BtlSuper_RequestCut ((void (*)(ActGChr *chr, s32 slot))BtlSuper_RequestCut)
#define BtlSuper_SetStartFx ((void (*)(ActGChr *chr, s32 slot))BtlSuper_SetStartFx)
#define BtlSuper_SetClassFlag ((void (*)(ActGChr *chr, s32 slot, s32 always))BtlSuper_SetClassFlag)
#define BtlSuper_RequestHitStop ((void (*)(ActGChr *chr, s32 light))BtlSuper_RequestHitStop)
#define BtlSuper_SetClass ((void (*)(ActGChr *chr, s32 slot))BtlSuper_SetClass)
#define BtlSuper_MeasureRushStep ((void (*)(ActGChr *chr, s32 slot, s32 frame))BtlSuper_MeasureRushStep)
#define BtlSuper_Begin ((void (*)(ActGChr *chr, s32 slot, s32 always))BtlSuper_Begin)
#define BtlSuper_PlayStartSound ((void (*)(ActGChr *chr, s32 slot, s32 always))BtlSuper_PlayStartSound)
#define BtlSuper_Leave ((void (*)(ActGChr *chr, s32 slot))BtlSuper_Leave)
#define BtlSuper_Finish ((void (*)(ActGChr *chr, s32 slot))BtlSuper_Finish)
#define BtlSuper_SetRushFlags ((void (*)(ActGChr *chr))BtlSuper_SetRushFlags)
extern s32 BtlAct_GetLandingAction(ActGChr *chr, s32 slot, s32 arg2);
extern f32 BtlOpp_GetTargetYaw(ActGChr *chr);
#define BtlMove_SetHeading ((void (*)(ActGChr *chr, f32 yaw, f32 pitch))BtlMove_SetHeading)
extern void BtlMove_MoveVertical(ActGChr *chr, f32 speed, f32 accel);
extern s32 BtlSuper_GetUnk1E(ActGChr *chr, s32 slot);
#define BtlSuper_GetHitDirKind ((s32 (*)(ActGChr *chr, s32 slot))BtlSuper_GetHitDirKind)
#define BtlMember_HasAbility ((s32 (*)(ActGChr *chr, s32 n))BtlMember_HasAbility)
#define BtlMember_GetActiveGauge ((ActGGauge *(*)(ActGChr *chr))BtlMember_GetActiveGauge)
#define BtlAct_SetPitchMotion ((void (*)(ActGChr *chr, s32 motionUp, s32 motionDown, s32 recalc))BtlAct_SetPitchMotion)
#define BtlAct_PushAngle ((void (*)(ActGChr *chr, f32 angle, f32 speed, f32 arg))BtlAct_PushAngle)
#define BtlAct_SetFormCurrent ((void (*)(ActGChr *chr))BtlAct_SetFormCurrent)
extern void BtlAct_SetFormRandom(ActGChr *chr);
#define ChrCam_AddShake ((void (*)(ActGChr *chr, f32 strength, f32 time))ChrCam_AddShake)
#define BtlChar_ClearFlagRange ((void (*)(ActGChr *chr, u32 a, u32 b))BtlChar_ClearFlagRange)
#define BtlChar_SavePlacement ((void (*)(ActGChr *chr))BtlChar_SavePlacement)
extern void BtlChange_RequestChara(s32 player, s32 chara, s32 costume, s32 variant, s32 animChara, s32 unk18, s32 voiceChara);
extern s32 BtlChange_IsLoadedFor(s32 player);
extern void BtlChange_SetReady(s32 player);
extern void BtlChange_SetDone(s32 player);
extern void BtlAnim_JumpToEnd(ActGChr *chr);
extern void BtlAnim_EnableHandle(ActGChr *chr);
#define BtlObjAnim_QueryEvent ((s32 (*)(ActGObj *obj, u64 mask, s32 layer, s32 mode))BtlObjAnim_QueryEvent)
extern s32 BtlAtk_GetDamage(ActGChr *chr);
extern s32 BtlAtk_GetThrowParamC(ActGChr *chr);
extern s32 BtlAtk_GetThrowParamE(ActGChr *chr);
#define BtlOpp_GetParamWord0 ((s32 (*)(ActGChr *chr))BtlOpp_GetParamWord0)
extern s32 BtlOpp_GetPlayer(ActGChr *chr);
extern ActGChr *BtlChar_Get(s32 i);
extern s32 BtlChar_IsDead(ActGChr *chr);
extern void BtlChar_RequestPlaceRelative(ActGChr *chr, s32 slot, s32 step, s32 self);
extern void BtlChar_ResetUnk1310(ActGChr *chr);
extern void BtlChange_RequestObject(s32 player, s32 id, s32 costume, s32 variant, s32 slot);
extern void BtlPartner_LinkToOwner(ActGChr *chr);
extern void BtlPartner_SetFlag80(ActGChr *chr, s32 on);
extern void BtlPartner_PlayAnim(ActGChr *chr, s32 anim);
extern s32 BtlPartner_StepAnim(ActGChr *chr);
extern void BtlPartner_Release(ActGChr *chr);
#define BtlMember_Damage ((void (*)(ActGChr *chr, s32 amount, s32 flags))BtlMember_Damage)
#define BtlAnim_GetFrame ((f32 (*)(ActGChr *chr))BtlAnim_GetFrame)
extern s32 BtlAct_GetMotionLevel(ActGChr *chr, s32 motion);
#define BtlSuper_GetFlagsA ((s32 (*)(ActGChr *chr, s32 slot))BtlSuper_GetFlagsA)
extern s32 BtlSuper_GetUnk223(ActGChr *chr, s32 slot);
#define BtlSuper_SetupRushDamage ((void (*)(ActGChr *chr, s32 slot, s32 fromAnim))BtlSuper_SetupRushDamage)
extern s32 BtlSuper_GetFrames22C(ActGChr *chr, s32 slot);
extern s32 BtlSuper_GetFrames238(ActGChr *chr, s32 slot);
extern f32 BtlCharApi_GetGroundY(s32 objId);
extern f32 BtlAct_GetHeight(ActGChr *chr);
extern void BtlChar_AddStageTimer(f32 seconds);
extern void BtlMove_BrakeVertical(ActGChr *chr);
#define BtlAnim_SetDuration ((void (*)(ActGChr *chr, f32 seconds))BtlAnim_SetDuration)

/* Technique parameter readers (0x210xxx..0x211xxx, not decompiled). */
extern f32 BtlMoveParam_GetTurnAccel(ActGChr *chr, s32 n);
extern f32 BtlMoveParam_GetTurnMax(ActGChr *chr, s32 n);
#define BtlSuper_GetFlags ((s32 (*)(ActGChr *chr, s32 slot))BtlSuper_GetFlags)
#define BtlSuper_GetShotTime ((f32 (*)(ActGChr *chr, s32 slot))BtlSuper_GetShotTime)
#define BtlSuper_GetKiCost ((s32 (*)(ActGChr *chr, s32 slot))BtlSuper_GetKiCost)
extern f32 BtlSuper_GetRushSpeed(ActGChr *chr, s32 slot);

/*
 * Actions 0x11B..0x11D (technique kinds 4, 5) and 0x11E..0x120 (kind 6: work[0] bit 0, no homing): a rush technique.
 *
 * enter   animation 0, turn to the opponent, BtlSuper_Begin, start sound, start effect and flag, camera cut, half of
 *         the ki cost (the other half is paid when the sequence starts).
 * run     animations 0 and 1: stand still under level 2 hit-stop (BtlSuper_RequestHitStop(chr, 0), flag 0x125).
 *         animation 2 (dash start): vibration and flag 0xD0 on its first frame, the camera cut ends; the dash begins.
 *         animation 3 (dash loop): BtlMove_SteerAtOpponent at the technique's speed (BtlSuper_GetRushSpeed) with
 *         the fighter's turn rates scaled by min(gap / 100, 1); the straight variant just flies along its heading.
 *         Flag 0x47 while dashing, flag 0x49 (the attack is live) in the loop; the body leans by the heading
 *         pitch (clamped to +-63 degrees) times a 0..1 factor that follows the dash animations.
 *         animation 6 (end): brake, work[1] = 1 at its end.
 *         BtlSuper_MeasureRushStep(chr, slot, actionFrame) plays one step of the sequence's animation chain per
 *         frame on the sub layer to measure it. Input 0x6F holds flags 0x9C..0x9E (sounds 0x14, 0x15, 0x1C once).
 * decide  the loop ends (animation 6, speed 0) after 16 frames in a row moving slower than 100 km/h, after
 *         BtlSuper_GetShotTime seconds, when lock-on (flag 5) is lost, or on flag 0x63.
 *         work[1]: action 0xB.  flag 0x64: flag 0x36 and action 0xCF.  flag 0x72 (the hit connected): effect
 *         bit 5 and action 0x12B + slot (the sequence, 0x12D..0x12F).
 * leave   BtlSuper_Leave.
 *
 * Matches only in one translation unit with the first part of this file: compiled as a file of its own (as
 * btl_act_g.c was) the `beqz` after BtlChar_TestFlag(chr, 5) comes out as `beqzl` with the target's
 * `lw a2,0x964(s0)` in its delay slot instead of the fall-through's `move a0,s0`.
 */
void BtlAct_SuperRushDashHandler(ActGChr *chr, s32 phase) {
    s32 *still = &chr->work[2];
    s32 *frames = &chr->work[3];
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(slot, 0), 0.15f);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 1, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_Begin(chr, slot, 0);
        BtlSuper_PlayStartSound(chr, slot, 0);
        BtlSuper_SetStartFx(chr, slot);
        BtlSuper_SetClassFlag(chr, slot, 0);
        BtlSuper_RequestCut(chr, slot);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, slot) / 2, 1);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_ResetLook(chr);
        switch (BtlAct_GetCurrent(chr)) {
            case 0x11E:
            case 0x11F:
            case 0x120:
                chr->work[0] |= 1;
                break;
        }
    }
    if (phase == 1) {
        ActGPose *pose = BtlChar_GetPos(chr);
        s32 moving = 0;
        f32 lean = 0.0f;
        f32 blend;

        switch (BtlAnim_GetId(chr)) {
            case 0x105:
            case 0x127:
            case 0x149:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 1), 0.0f, 0);
                BtlSuper_RequestHitStop(chr, 0);
                lean = 0.0f;
                BtlSuper_SetClass(chr, slot);
                break;
            case 0x106:
            case 0x128:
            case 0x14A:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 2), 0.0f, 0);
                BtlSuper_RequestHitStop(chr, 0);
                lean = 0.0f;
                BtlSuper_SetClass(chr, slot);
                break;
            case 0x107:
            case 0x129:
            case 0x14B:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 3), 0.0f, 0);
                BtlSuper_SetClass(chr, slot);
                if (BtlAnim_IsNew(chr)) {
                    BtlChar_Vibrate(chr, 0.8f, 0.3f);
                    BtlChar_SetFlag(chr, 0xD0);
                    ChrCam_EndCut(chr);
                }
                BtlChar_SetFxBit(chr, 4);
                BtlChar_SetFlag(chr, 0x47);
                moving = 1;
                if ((chr->work[0] & 1) && !BtlAnim_IsNew(chr) && !BtlAnim_WasNew(chr)) {
                    BtlChar_SetFlag(chr, 0x49);
                }
                lean = BtlAnim_GetProgress(chr);
                break;
            case 0x108:
            case 0x12A:
            case 0x14C:
                moving = 1;
                BtlAnim_AdvanceLoop(chr, 0);
                lean = 1.0f;
                BtlChar_SetFlag(chr, 0x49);
                BtlChar_SetFlag(chr, 0x47);
                BtlChar_SetFxBit(chr, 4);
                BtlChar_SetSmallVibration(chr, 0.1f);
                break;
            case 0x10B:
            case 0x12D:
            case 0x14F:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                lean = 1.0f - BtlAnim_GetProgress(chr);
                break;
        }
        if (moving) {
            f32 speed = BtlSuper_GetRushSpeed(chr, slot);

            if (chr->work[0] & 1) {
                BtlMove_Step(chr, 6, 6, 3, speed, 10000.0f);
            } else {
                f32 yawAccel = BtlMoveParam_GetTurnAccel(chr, 2);
                f32 pitchAccel = BtlMoveParam_GetTurnAccel(chr, 3);
                f32 yawMax = BtlMoveParam_GetTurnMax(chr, 2);
                f32 pitchMax = BtlMoveParam_GetTurnMax(chr, 3);
                f32 ratio = BtlUtil_MinF(BtlOpp_GetGapXZ(chr) * 0.01f, 1.0f);

                BtlMove_SteerAtOpponent(chr, speed, yawAccel, pitchAccel, yawMax * ratio, pitchMax * ratio, 3.14159265f);
                BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.5f);
                BtlMove_SetDirection(chr, 3);
                BtlMove_Advance(chr, speed, 10000.0f);
                BtlMove_ApplyGravity(chr);
            }
            BtlChar_SetFlag(chr, 9);
        } else {
            BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(200.0f));
        }
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFxBit(chr, 0x3A);
        if (chr->work[0] & 1) {
            BtlChar_SetFlag(chr, 0x1A);
        }
        if (BtlChar_TestFlag(chr, 5) && BtlSuper_GetFlags(chr, slot) >= 0) {
            BtlMove_SetLeanX(chr, BtlUtil_ClampF(pose->pitch, BTL_DEG(-63.0f), BTL_DEG(63.0f)) * lean);
        }
        BtlSuper_MeasureRushStep(chr, slot, chr->actionFrame);
        if (BtlInput_TestAction(chr, 0x6F, 1)) {
            BtlChar_SetHeldFlag(chr, 0x9E);
            BtlChar_SetHeldFlag(chr, 0x9D);
            BtlChar_SetHeldFlag(chr, 0x9C);
            if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                BtlCharSnd_PlayCommon(chr, 0x15);
                BtlCharSnd_PlayCommon(chr, 0x1C);
            }
        }
    }
    if (phase == 2) {
        ActGPose *pose = BtlChar_GetPos(chr);

        switch (BtlAnim_GetId(chr)) {
            case 0x105:
            case 0x127:
            case 0x149:
            case 0x106:
            case 0x128:
            case 0x14A:
            case 0x10B:
            case 0x12D:
            case 0x14F:
                break;
            case 0x107:
            case 0x129:
            case 0x14B:
                if ((chr->work[0] & 1) && BtlChar_TestFlag(chr, 0x63)) {
                    BtlAnim_Request(chr, SUPER_ANIM(slot, 6), 0.15f);
                    pose->speed = 0.0f;
                }
                break;
            case 0x108:
            case 0x12A:
            case 0x14C: {
                s32 stop = 0;

                if (Vec3_Length(&pose->moved) < BTL_KMH(100.0f)) {
                    if (++*still >= 16) {
                        pose->speed = 0.0f;
                        stop = 1;
                    }
                } else {
                    *still = 0;
                }
                if ((f32)++*frames > BtlSuper_GetShotTime(chr, slot) * 30.0f) {
                    pose->speed = 0.0f;
                    stop = 1;
                }
                if (!BtlChar_TestFlag(chr, 5)) {
                    stop = 1;
                }
                if (BtlChar_TestFlag(chr, 0x63)) {
                    pose->speed = 0.0f;
                    stop = 1;
                }
                if (stop) {
                    BtlAnim_Request(chr, SUPER_ANIM(slot, 6), 0.15f);
                }
                break;
            }
        }
        if (chr->work[1] != 0) {
            BtlAct_Request(chr, 0xB);
        }
        if (BtlChar_TestFlag(chr, 0x64)) {
            BtlChar_SetFlag(chr, 0x36);
            BtlAct_Request(chr, 0xCF);
        }
        if (BtlChar_TestFlag(chr, 0x72)) {
            BtlChar_SetFxBit(chr, 5);
            BtlAct_Request(chr, slot + 0x12B);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, slot);
    }
}

/*
 * Actions 0x121..0x123 (technique kind 10): a rush technique struck from where the fighter stands.
 * Animations 0 and 1 under level 2 hit-stop, then animation 2 followed directly by the end animation 6: during
 * animation 2 the fighter moves at 100 km/h (BtlMove_Step dir mode 2), BtlColl_SetFramesToReach(chr, 0.5) is
 * called and the attack is live (flag 0x49) in its second half. Flags 0xA5 and 0x53 every frame.
 * decide: work[1] -> 0xB; flag 0x7A -> 0xCA; flag 0x64 -> flag 0x36 and 0xCF; flag 0x72 -> the sequence.
 */
void BtlAct_SuperRushStrikeHandler(ActGChr *chr, s32 phase) {
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(slot, 0), 0.15f);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 1, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_Begin(chr, slot, 0);
        BtlSuper_PlayStartSound(chr, slot, 0);
        BtlSuper_SetStartFx(chr, slot);
        BtlSuper_SetClassFlag(chr, slot, 0);
        BtlSuper_RequestCut(chr, slot);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, slot) / 2, 1);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_ResetLook(chr);
    }
    if (phase == 1) {
        s32 moving = 0;

        switch (BtlAnim_GetId(chr)) {
            case 0x105:
            case 0x127:
            case 0x149:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 1), 0.0f, 0);
                BtlSuper_RequestHitStop(chr, 0);
                BtlSuper_SetClass(chr, slot);
                break;
            case 0x106:
            case 0x128:
            case 0x14A:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 2), 0.0f, 0);
                BtlSuper_RequestHitStop(chr, 0);
                BtlSuper_SetClass(chr, slot);
                break;
            case 0x107:
            case 0x129:
            case 0x14B:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 6), 0.0f, 0);
                BtlSuper_SetClass(chr, slot);
                if (BtlAnim_IsNew(chr)) {
                    ChrCam_EndCut(chr);
                }
                if (0.5f < BtlAnim_GetProgress(chr)) {
                    BtlChar_SetFlag(chr, 0x49);
                }
                BtlColl_SetFramesToReach(chr, 0.5f);
                moving = 1;
                break;
            case 0x10B:
            case 0x12D:
            case 0x14F:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                break;
        }
        if (moving) {
            BtlMove_Step(chr, 6, 5, 2, BTL_KMH(100.0f), BTL_KMH(100.0f));
        } else {
            BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        }
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFxBit(chr, 0x3A);
        BtlChar_SetFlag(chr, 0xA5);
        BtlChar_SetFlag(chr, 0x53);
        BtlSuper_MeasureRushStep(chr, slot, chr->actionFrame);
        if (BtlInput_TestAction(chr, 0x6F, 1)) {
            BtlChar_SetHeldFlag(chr, 0x9E);
            BtlChar_SetHeldFlag(chr, 0x9D);
            BtlChar_SetHeldFlag(chr, 0x9C);
            if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                BtlCharSnd_PlayCommon(chr, 0x15);
                BtlCharSnd_PlayCommon(chr, 0x1C);
            }
        }
    }
    if (phase == 2) {
        BtlAnim_GetId(chr);
        if (chr->work[1] != 0) {
            BtlAct_Request(chr, 0xB);
        }
        if (BtlChar_TestFlag(chr, 0x7A)) {
            BtlAct_Request(chr, 0xCA);
        }
        if (BtlChar_TestFlag(chr, 0x64)) {
            BtlChar_SetFlag(chr, 0x36);
            BtlAct_Request(chr, 0xCF);
        }
        if (BtlChar_TestFlag(chr, 0x72)) {
            BtlAct_Request(chr, slot + 0x12B);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, slot);
    }
}

/*
 * Actions 0x12D..0x12F (attacker, work[0] bit 0) and 0x139..0x13B (victim, queued for hit reaction 0x22): the
 * scripted sequence of a rush technique. Both fighters play a chain of up to five animations (attacker 0x17..,
 * victim 0x1C..; BtlAct_GetMotionLevel gives the step) while BtlChar_RequestPlaceRelative keeps them placed
 * relative to each other. Everything is driven by the throw block at chr + 0xE90, which BtlColl_StartThrow /
 * BtlColl_StartRushCatch filled on the attacker and copied to the victim:
 *
 *   tech      technique id: 0x280 sets work[0] bit 2 (the attacker turns into a random character at the end, flag
 *             0xA6 while it runs), 0x2E8 links the partner object to its owner, 0x2E4 raises flag 0x13B in step 0
 *   unk10     number of steps; unk50: place the pair in step 0 as well
 *   unk18     step that waits for a partner object: unk20..unk2C is the BtlChange_RequestObject request made on
 *             entry (work[0] bit 1); from the next step on the partner plays animation 0x19D + n
 *   unk1C     step in which the victim's model is reloaded when unk4C is set (BtlAct_SetFormCurrent: the damaged
 *             variant); unk48: the attacker's model is reloaded in the last step
 *   unk14     landing kind passed to BtlAct_GetLandingAction for the victim's next action
 *   unk30, unk60, unk64   turn the victim (flag 0x94) to the attacker-relative yaw / pitch, mirrored when hit from
 *             the front; otherwise a victim hit from behind is simply turned around
 *   unk34     the victim ends in the neutral action 0xB;  unk40: a dead victim goes to action 0xEB
 *   unk5C     the attacker is put on the ground when the action ends
 *
 * A step ends on the animation event flag 0x31; steps that wait for a model load wait for BtlChange_IsLoadedFor,
 * then BtlChange_SetReady / BtlChange_SetDone (flag 0x55 marks the swap). The frame reached in each step is kept
 * in stepFrame[]. Both fighters are invulnerable to everything else for the duration (flags 0x42..0x46, 0x136,
 * BtlSuper_SetRushFlags) and hold position (speed 0, vertical speed braked).
 *
 * Damage is not applied here: the attacker's enter phase fills the damage queue (BtlSuper_SetupRushDamage) and the
 * member code deals it on the animation's hit events. The only direct damage is the self-destruct case: at the end
 * an attacker whose technique has flag B 0x2000 loses all health but 1 (BtlMember_Damage(chr, health - 1, 0x5B)).
 *
 * decide (work[1]): attacker -> flags 0x8C, 0x33, 3 s on the stage timer, BtlSuper_Finish. Victim -> flag 0x33
 * and one of: 0xB (unk34), 0xEB (dead and unk40), or the landing action with flags 0x1D / 0x1E held and flag
 * 0x35 when hit from behind.
 * leave: flags 0x37 and 0xCD; the attacker also runs BtlSuper_Leave, lands or goes airborne (held flag 0xE),
 * releases the partner object and adds 3 s to the stage timer.
 */
void BtlAct_SuperRushSequenceHandler(ActGChr *chr, s32 phase) {
    s32 *stepA = &chr->work[3];
    s32 *stepB = &chr->work[2];
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        s32 anim = 0;

        switch (BtlAct_GetCurrent(chr)) {
            case 0x12D:
            case 0x12E:
            case 0x12F:
                anim = SUPER_ANIM(slot, 0x17);
                chr->work[0] |= 1;
                break;
            case 0x139:
            case 0x13A:
            case 0x13B:
                anim = SUPER_ANIM(slot, 0x1C);
                break;
        }
        BtlAnim_Play(chr, anim, 0.0f);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        chr->stepFrame[0] = 0.0f;
        chr->stepFrame[1] = 0.0f;
        chr->stepFrame[2] = 0.0f;
        chr->stepFrame[3] = 0.0f;
        chr->stepFrame[4] = 0.0f;
        if (chr->thr.tech == 0x280) {
            chr->work[0] |= 4;
        }
        if (chr->work[0] & 1) {
            BtlSuper_SetupRushDamage(chr, slot, 0);
            switch (BtlAct_GetPrev(chr)) {
                case 0x11B:
                case 0x11C:
                case 0x11D:
                case 0x11E:
                case 0x11F:
                case 0x120:
                case 0x121:
                case 0x122:
                case 0x123:
                    BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, slot) / 2, 1);
                    break;
            }
            if (chr->thr.unk20 >= 0) {
                BtlChange_RequestObject(chr->player, chr->thr.unk20, chr->thr.unk24, chr->thr.unk28, chr->thr.unk2C);
                chr->work[0] |= 2;
            }
        }
    }
    if (phase == 1) {
        s32 level = BtlAct_GetMotionLevel(chr, BtlAnim_GetId(chr));

        if (BtlAnim_IsNew(chr)) {
            if (level != 0 || chr->thr.unk50 != 0) {
                BtlChar_RequestPlaceRelative(chr, slot, level, chr->work[0] & 1);
            }
        }
        if (level == chr->thr.unk18) {
            if (BtlAnim_Advance(chr, 0)) {
                if (BtlChange_IsLoadedFor(chr->thr.atkSide)) {
                    if (chr->work[0] & 2) {
                        BtlChange_SetReady(chr->player);
                    }
                    BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                }
            }
        } else if (chr->work[0] & 4) {
            if (level < chr->thr.unk10 - 1) {
                BtlAnim_AdvanceThen(chr, BtlAnim_GetId(chr) + 1, 0.0f, 0);
            } else {
                switch (*stepA) {
                    case 0:
                        if ((chr->work[0] & 1) && BtlAnim_IsNew(chr)) {
                            BtlAct_SetFormRandom(chr);
                            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.unk14,
                                                   chr->form.unk18, chr->form.unk1C, chr->form.unk20);
                        }
                        if (BtlAnim_Advance(chr, 0)) {
                            ++*stepA;
                        }
                        break;
                    case 1:
                        if (BtlChange_IsLoadedFor(chr->thr.atkSide)) {
                            if (chr->work[0] & 1) {
                                BtlChange_SetReady(chr->thr.atkSide);
                                BtlAnim_Request(chr, BtlAnim_GetId(chr), 0.0f);
                            }
                            ++*stepA;
                        }
                        break;
                    case 2:
                        if (chr->work[0] & 1) {
                            BtlChange_SetDone(chr->thr.atkSide);
                            BtlChar_RequestPlaceRelative(chr, slot, level, chr->work[0] & 1);
                            BtlAnim_JumpToEnd(chr);
                        }
                        chr->work[1] = 1;
                        break;
                }
            }
        } else {
            s32 okA = 0;
            s32 okB = 0;

            if (chr->work[0] & 2) {
                if (chr->thr.unk18 < level) {
                    if (BtlAnim_IsNew(chr)) {
                        if (level == chr->thr.unk18 + 1) {
                            BtlChange_SetDone(chr->player);
                            if (chr->thr.tech == 0x2E8) {
                                BtlPartner_LinkToOwner(chr);
                                if (BtlMember_HasAbility(chr, 0x79)) {
                                    BtlPartner_SetFlag80(chr, 1);
                                }
                            }
                        }
                        BtlPartner_PlayAnim(chr, level - chr->thr.unk18 + 0x19D);
                    }
                    BtlPartner_StepAnim(chr);
                }
            }
            BtlAnim_Advance(chr, 0);
            if (chr->thr.unk4C != 0 && chr->thr.unk1C == level) {
                switch (*stepB) {
                    case 0:
                        if (!(chr->work[0] & 1) && BtlAnim_IsNew(chr)) {
                            BtlAct_SetFormCurrent(chr);
                            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.unk14,
                                                   chr->form.unk18, chr->form.unk1C, chr->form.unk20);
                        }
                        if (BtlChar_TestFlag(chr, 0x31)) {
                            ++*stepB;
                        }
                        break;
                    case 1:
                        if (BtlChange_IsLoadedFor(chr->thr.defSide)) {
                            if (!(chr->work[0] & 1)) {
                                BtlChar_ResetUnk1310(chr);
                                BtlChange_SetReady(chr->thr.defSide);
                                BtlAnim_Request(chr, BtlAnim_GetId(chr), 0.0f);
                            }
                            ++*stepB;
                        }
                        break;
                    case 2:
                        if (!(chr->work[0] & 1)) {
                            BtlChange_SetDone(chr->thr.defSide);
                            BtlChar_RequestPlaceRelative(chr, slot, level, chr->work[0] & 1);
                            BtlChar_SetFlag(chr, 0x55);
                            BtlAnim_JumpToEnd(chr);
                        }
                        ++*stepB;
                        break;
                    case 3:
                        okB = 1;
                        break;
                }
            } else {
                okB = 1;
            }
            if (chr->thr.unk48 != 0 && chr->thr.unk10 - 1 == level) {
                switch (*stepA) {
                    case 0:
                        if ((chr->work[0] & 1) && BtlAnim_IsNew(chr)) {
                            BtlAct_SetFormCurrent(chr);
                            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.unk14,
                                                   chr->form.unk18, chr->form.unk1C, chr->form.unk20);
                        }
                        if (BtlChar_TestFlag(chr, 0x31)) {
                            ++*stepA;
                        }
                        break;
                    case 1:
                        if (BtlChange_IsLoadedFor(chr->thr.atkSide)) {
                            if (chr->work[0] & 1) {
                                BtlChar_ResetUnk1310(chr);
                                BtlChange_SetReady(chr->thr.atkSide);
                                BtlAnim_Request(chr, BtlAnim_GetId(chr), 0.0f);
                            }
                            ++*stepA;
                        }
                        break;
                    case 2:
                        if (chr->work[0] & 1) {
                            BtlChange_SetDone(chr->thr.atkSide);
                            BtlChar_RequestPlaceRelative(chr, slot, level, chr->work[0] & 1);
                            BtlChar_SetFlag(chr, 0x55);
                            BtlAnim_JumpToEnd(chr);
                        }
                        ++*stepA;
                        break;
                    case 3:
                        okA = 1;
                        break;
                }
            } else {
                okA = 1;
            }
            if (BtlChar_TestFlag(chr, 0x31) && okA && okB) {
                if (level < chr->thr.unk10 - 1) {
                    BtlAnim_Request(chr, BtlAnim_GetId(chr) + 1, 0.0f);
                } else {
                    chr->work[1] = 1;
                }
            }
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlSuper_SetRushFlags(chr);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 9);
        BtlChar_SetFlag(chr, 0x136);
        BtlChar_SetFlag(chr, 0x58);
        BtlChar_SetFxBit(chr, 0x3B);
        BtlChar_SetFlag(chr, 0x1B);
        if (chr->thr.tech == 0x2E4 && level == 0) {
            BtlChar_SetFlag(chr, 0x13B);
        }
        if (chr->work[0] & 1) {
            BtlChar_SetFlag(chr, 0x8B);
            if (level < BtlSuper_GetUnk223(chr, slot)) {
                BtlChar_SetFxBit(chr, 0x11);
            }
            if (BtlSuper_GetFlagsA(chr, slot) & 0x20000000) {
                BtlChar_SetFxBit(chr, 0x14);
            }
            if (BtlSuper_GetFlags(chr, slot) & 0x20000000) {
                BtlChar_SetFlag(chr, 0x13B);
            }
            if (BtlSuper_GetFlags(chr, slot) & 8) {
                BtlChar_SetFlag(chr, 0x13C);
            }
            if (chr->work[0] & 4) {
                BtlChar_SetFlag(chr, 0xA6);
            }
        } else {
            BtlChar_SetFlag(chr, 0x97);
            BtlChar_SetFlag(chr, 0x95);
            if (BtlSuper_GetFlagsA(BtlChar_Get(BtlOpp_GetPlayer(chr)), slot) & 0x40000000) {
                BtlChar_SetFxBit(chr, 0x14);
            }
        }
        chr->stepFrame[level] = BtlAnim_GetFrame(chr);
        if (chr->work[1] != 0 && (chr->work[0] & 1) && (BtlSuper_GetFlags(chr, slot) & 0x2000)) {
            BtlMember_Damage(chr, BtlMember_GetActiveGauge(chr)->health - 1, 0x5B);
            BtlMember_GetActiveGauge(chr)->unk4C[slot]++;
        }
    }
    if (phase == 2) {
        if (chr->work[1] != 0) {
            if (chr->work[0] & 1) {
                BtlChar_SetFlag(chr, 0x8C);
                BtlChar_SetFlag(chr, 0x33);
                BtlChar_AddStageTimer(3.0f);
                BtlSuper_Finish(chr, slot);
            } else {
                BtlChar_SetFlag(chr, 0x33);
                if (chr->thr.unk34 != 0) {
                    BtlChar_SetHeldFlag(chr, 0xE);
                    BtlAct_Request(chr, 0xB);
                } else if (chr->thr.unk40 != 0 && BtlChar_IsDead(chr)) {
                    BtlAct_Request(chr, 0xEB);
                } else {
                    s32 next;

                    BtlChar_SetHeldFlag(chr, 0x1D);
                    BtlChar_SetHeldFlag(chr, 0x1E);
                    next = BtlAct_GetLandingAction(chr, slot, chr->thr.unk14);
                    if (chr->thr.unk30 != 0) {
                        f32 yaw;
                        f32 addYaw;
                        f32 pitch;

                        BtlChar_SetFlag(chr, 0x94);
                        yaw = BtlChar_GetPos(chr)->rot.y;
                        addYaw = chr->thr.unk60;
                        pitch = chr->thr.unk64;
                        if (chr->react.back != 0) {
                            chr->react.turnYaw = BtlUtil_WrapAngle(yaw + addYaw);
                            chr->react.turnPitch = pitch;
                        } else {
                            chr->react.turnYaw = BtlUtil_WrapAngle(BtlUtil_WrapAngle(yaw + addYaw) + 3.14159265f);
                            chr->react.turnPitch = -pitch;
                        }
                    } else {
                        if (chr->react.back != 0) {
                            f32 yaw = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw + 3.14159265f);

                            BtlMove_SetHeading(chr, yaw, BtlChar_GetPos(chr)->pitch);
                        }
                    }
                    if (chr->react.back != 0) {
                        BtlChar_SetFlag(chr, 0x35);
                    }
                    BtlAct_Request(chr, next);
                }
            }
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x37);
        BtlChar_SetFlag(chr, 0xCD);
        if (chr->work[0] & 1) {
            BtlSuper_Leave(chr, slot);
            if (chr->thr.unk5C != 0) {
                ActGPose *pose = BtlChar_GetPos(chr);

                pose->pos.y += BtlAct_GetHeight(chr);
            } else {
                BtlChar_SetHeldFlag(chr, 0xE);
            }
            if (chr->work[0] & 2) {
                BtlPartner_Release(chr);
            }
            BtlChar_AddStageTimer(3.0f);
        }
    }
}

/*
 * Actions 0x124..0x126 (technique kind 8): a dash the player steers. Start animations as in the rush dash (full ki
 * cost on entry), then the loop animation 3 flying at BtlSuper_GetRushSpeed with BtlMove_TurnYaw mode 1 (7.2
 * degrees per frame) and BtlMove_TurnPitch mode 0 (5.4 degrees per frame). Flag 0x1A every frame. The loop ends
 * (animation 6) on flag 0xA8, raised by the technique's effect, or when lock-on is lost; then BtlSuper_Finish.
 */
void BtlAct_SuperRushFlyHandler(ActGChr *chr, s32 phase) {
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(slot, 0), 0.15f);
        BtlSuper_Begin(chr, slot, 0);
        BtlSuper_PlayStartSound(chr, slot, 0);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 1, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_SetStartFx(chr, slot);
        BtlSuper_SetClassFlag(chr, slot, 0);
        BtlSuper_RequestCut(chr, slot);
        BtlMember_SpendKi(chr, BtlSuper_GetKiCost(chr, slot), 1);
        BtlChar_SetHeldFlag(chr, 0xE);
        BtlChar_ResetLook(chr);
    }
    if (phase == 1) {
        f32 lean = 0.0f;
        s32 moving = 0;

        switch (BtlAnim_GetId(chr)) {
            case 0x105:
            case 0x127:
            case 0x149:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 1), 0.0f, 0);
                BtlSuper_RequestHitStop(chr, 0);
                lean = 0.0f;
                BtlSuper_SetClass(chr, slot);
                break;
            case 0x106:
            case 0x128:
            case 0x14A:
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 2), 0.0f, 0);
                BtlSuper_RequestHitStop(chr, 0);
                lean = 0.0f;
                BtlSuper_SetClass(chr, slot);
                break;
            case 0x107:
            case 0x129:
            case 0x14B:
                if (BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 3), 0.0f, 0)) {
                    BtlChar_Vibrate(chr, 0.8f, 0.3f);
                    BtlChar_SetFlag(chr, 0xD0);
                }
                BtlSuper_SetClass(chr, slot);
                if (BtlAnim_IsNew(chr)) {
                    ChrCam_EndCut(chr);
                }
                lean = BtlAnim_GetProgress(chr);
                break;
            case 0x108:
            case 0x12A:
            case 0x14C:
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlAnim_IsNew(chr)) {
                    BtlMove_TurnYaw(chr, 2, 3.14159265f);
                    BtlMove_TurnPitch(chr, 1, 3.14159265f);
                    BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
                }
                moving = 1;
                lean = 1.0f;
                BtlChar_SetSmallVibration(chr, 0.1f);
                break;
            case 0x10B:
            case 0x12D:
            case 0x14F:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                lean = 1.0f - BtlAnim_GetProgress(chr);
                break;
        }
        if (moving) {
            f32 speed = BtlSuper_GetRushSpeed(chr, slot);

            BtlMove_TurnYaw(chr, 1, BTL_DEG(7.2f));
            BtlMove_TurnPitch(chr, 0, BTL_DEG(5.4f));
            BtlMove_TurnModelYaw(chr, 3.14159265f, 0.5f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, 10000.0f);
            BtlMove_ApplyGravity(chr);
            BtlChar_SetFlag(chr, 9);
        } else {
            BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(200.0f));
        }
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFlag(chr, 0x1A);
        if (BtlChar_TestFlag(chr, 5)) {
            BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch * lean);
        }
        if (BtlInput_TestAction(chr, 0x6F, 1)) {
            BtlChar_SetHeldFlag(chr, 0x9E);
            BtlChar_SetHeldFlag(chr, 0x9D);
            BtlChar_SetHeldFlag(chr, 0x9C);
            if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                BtlCharSnd_PlayCommon(chr, 0x15);
                BtlCharSnd_PlayCommon(chr, 0x1C);
            }
        }
    }
    if (phase == 2) {
        ActGPose *pose = BtlChar_GetPos(chr);

        switch (BtlAnim_GetId(chr)) {
            case 0x105:
            case 0x127:
            case 0x149:
            case 0x106:
            case 0x128:
            case 0x14A:
            case 0x107:
            case 0x129:
            case 0x14B:
            case 0x10B:
            case 0x12D:
            case 0x14F:
                break;
            case 0x108:
            case 0x12A:
            case 0x14C: {
                s32 stop = 0;

                if (BtlChar_TestFlag(chr, 0xA8)) {
                    pose->speed = 0.0f;
                    stop = 1;
                }
                if (!BtlChar_TestFlag(chr, 5)) {
                    stop = 1;
                }
                if (stop) {
                    BtlAnim_Request(chr, SUPER_ANIM(slot, 6), 0.15f);
                }
                break;
            }
        }
        if (chr->work[1] != 0) {
            BtlSuper_Finish(chr, slot);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, slot);
    }
}

/*
 * Actions 0x127..0x129: the same steered dash started as a follow-up: no hit-stop, no camera cut, no ki cost,
 * the two start animations hurried to 0.1 s each, flag 0xA4 (30% damage) every frame. Coming from action 0x2E it
 * raises flag 0xCD.
 */
/* The redundant prototype below is a matching aid, not original source: this function's jump-table dispatch
 * (`lw` with the table's address as displacement, against a separate `la`) flips with unrelated changes earlier in
 * the translation unit (a declaration added or removed anywhere above is enough), so it is the compiler's state
 * that decides, not the code. With the file as it stands the function matches only with one more declaration in
 * front of it. Re-check with fdiff after any change to this file or its headers; remove the line if it flips. */
void BtlAct_SuperRushFollowHandler(ActGChr *chr, s32 phase);
void BtlAct_SuperRushFollowHandler(ActGChr *chr, s32 phase) {
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(slot, 0), 0.15f);
        BtlSuper_Begin(chr, slot, 0);
        BtlSuper_PlayStartSound(chr, slot, 0);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 1, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_SetStartFx(chr, slot);
        BtlChar_SetHeldFlag(chr, 0xE);
        if (BtlAct_GetPrev(chr) == 0x2E) {
            BtlChar_SetFlag(chr, 0xCD);
        }
    }
    if (phase == 1) {
        f32 lean = 0.0f;
        s32 moving = 0;

        switch (BtlAnim_GetId(chr)) {
            case 0x105:
            case 0x127:
            case 0x149:
                BtlAnim_SetDuration(chr, 0.1f);
                BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 2), 0.0f, 0);
                lean = 0.0f;
                break;
            case 0x107:
            case 0x129:
            case 0x14B:
                BtlAnim_SetDuration(chr, 0.1f);
                if (BtlAnim_AdvanceThen(chr, SUPER_ANIM(slot, 3), 0.0f, 0)) {
                    BtlChar_Vibrate(chr, 0.8f, 0.3f);
                    BtlChar_SetFlag(chr, 0xD0);
                }
                lean = BtlAnim_GetProgress(chr);
                break;
            case 0x108:
            case 0x12A:
            case 0x14C:
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlAnim_IsNew(chr)) {
                    BtlMove_TurnYaw(chr, 2, 3.14159265f);
                    BtlMove_TurnPitch(chr, 1, 3.14159265f);
                    BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
                }
                moving = 1;
                lean = 1.0f;
                BtlChar_SetSmallVibration(chr, 0.1f);
                break;
            case 0x10B:
            case 0x12D:
            case 0x14F:
                if (BtlAnim_Advance(chr, 0)) {
                    chr->work[1] = 1;
                }
                lean = 1.0f - BtlAnim_GetProgress(chr);
                break;
        }
        if (moving) {
            f32 speed = BtlSuper_GetRushSpeed(chr, slot);

            BtlMove_TurnYaw(chr, 1, BTL_DEG(7.2f));
            BtlMove_TurnPitch(chr, 0, BTL_DEG(5.4f));
            BtlMove_TurnModelYaw(chr, 3.14159265f, 0.5f);
            BtlMove_SetDirection(chr, 3);
            BtlMove_Advance(chr, speed, 10000.0f);
            BtlMove_ApplyGravity(chr);
            BtlChar_SetFlag(chr, 9);
        } else {
            BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(200.0f));
        }
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFlag(chr, 0x1A);
        BtlChar_SetFlag(chr, 0xA4);
        if (BtlChar_TestFlag(chr, 5)) {
            BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->pitch * lean);
        }
    }
    if (phase == 2) {
        ActGPose *pose = BtlChar_GetPos(chr);

        switch (BtlAnim_GetId(chr)) {
            case 0x105:
            case 0x127:
            case 0x149:
            case 0x106:
            case 0x128:
            case 0x14A:
            case 0x107:
            case 0x129:
            case 0x14B:
            case 0x10B:
            case 0x12D:
            case 0x14F:
                break;
            case 0x108:
            case 0x12A:
            case 0x14C: {
                s32 stop = 0;

                if (BtlChar_TestFlag(chr, 0xA8)) {
                    pose->speed = 0.0f;
                    stop = 1;
                }
                if (!BtlChar_TestFlag(chr, 5)) {
                    stop = 1;
                }
                if (stop) {
                    BtlAnim_Request(chr, SUPER_ANIM(slot, 6), 0.15f);
                }
                break;
            }
        }
        if (chr->work[1] != 0) {
            BtlSuper_Finish(chr, slot);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, slot);
    }
}

/*
 * Actions 0x12A..0x12C: closing animation 0x15 of a rush technique. Fills the damage queue from the animation's own
 * hit events (BtlSuper_SetupRushDamage(chr, slot, 1)), holds position, sets the rush flags and 0x42..0x46 / 0x136,
 * and runs camera cut 9 between the two frames the technique gives (BtlSuper_GetFrames22C / 238). With throw
 * block unk38: flags 0x8B and 9. At the end: flags 0x8C and 0x33, BtlSuper_Finish. Leaving adds 3 s to the stage
 * timer and lands the fighter when it is less than 5 m above the ground.
 */
void BtlAct_SuperRushFinishHandler(ActGChr *chr, s32 phase) {
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(slot, 0x15), 0.0f);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        BtlSuper_SetupRushDamage(chr, slot, 1);
    }
    if (phase == 1) {
        s32 cutOn;
        s32 cutOff;

        if (BtlAnim_Advance(chr, 0)) {
            chr->work[1] = 1;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlSuper_SetRushFlags(chr);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFxBit(chr, 0x10);
        BtlChar_SetFlag(chr, 0x136);
        if (chr->thr.unk38 != 0) {
            BtlChar_SetFlag(chr, 0x8B);
            BtlChar_SetFlag(chr, 9);
        }
        cutOn = BtlSuper_GetFrames22C(chr, slot);
        cutOff = BtlSuper_GetFrames238(chr, slot);
        if (cutOn >= 0 && cutOff >= 0) {
            if (chr->actionFrame == cutOn) {
                ChrCam_RequestCut(chr, 0, 9);
            }
            if (chr->actionFrame == cutOff) {
                ChrCam_EndCut(chr);
            }
        }
    }
    if (phase == 2) {
        if (chr->work[1] != 0) {
            BtlChar_SetFlag(chr, 0x8C);
            BtlChar_SetFlag(chr, 0x33);
            BtlSuper_Finish(chr, slot);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, slot);
        BtlChar_AddStageTimer(3.0f);
        if (BtlAct_GetHeight(chr) < 5.0f) {
            ActGPose *pose;

            BtlChar_SetHeldFlag(chr, 0xF);
            pose = BtlChar_GetPos(chr);
            pose->pos.y = BtlCharApi_GetGroundY(chr->objId);
            BtlChar_ClearFlag(chr, 0xE);
        } else {
            BtlChar_SetHeldFlag(chr, 0xE);
        }
    }
}

/*
 * Actions 0x133..0x135 (queued for hit reactions 0x1D and 0x1E, slot from the throw block): the victim of a rush
 * technique that catches without a sequence. Animation 0x16; with throw block unk44 (reaction 0x1E) it loops, with
 * common sound 0x12, until flag 0xAF is no longer set; otherwise it plays once. Flags 0x95, 0x96, 0x136 and the
 * rush flags every frame. Then, exactly as the victim side of the sequence: flags 0x1D / 0x1E held, turn by the
 * throw block (unk30, unk60, unk64) around the direction to the opponent, flag 0x35 when hit from behind, and
 * the landing action BtlAct_GetLandingAction(chr, slot, thr.unk14). Leaving raises flag 0x33.
 */
void BtlAct_SuperRushCaughtHandler(ActGChr *chr, s32 phase) {
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(slot, 0x16), 0.0f);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        if (chr->thr.unk44 != 0) {
            BtlCharSnd_PlayCommon(chr, 0x12);
        }
        if (chr->thr.unk3C != 0) {
            BtlMove_TurnYaw(chr, 2, 3.14159265f);
            BtlMove_TurnPitch(chr, 5, 3.14159265f);
        }
    }
    if (phase == 1) {
        if (chr->thr.unk44 != 0) {
            BtlAnim_AdvanceLoop(chr, 0);
        } else if (BtlAnim_Advance(chr, 0)) {
            chr->work[1] = 1;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, BTL_KMH(100.0f));
        BtlMove_BrakeVertical(chr);
        BtlSuper_SetRushFlags(chr);
        BtlChar_SetFlag(chr, 0x95);
        BtlChar_SetFlag(chr, 0x96);
        BtlChar_SetFlag(chr, 0x136);
        if (chr->thr.unk44 != 0) {
            BtlChar_SetFlag(chr, 0x42);
            BtlChar_SetFlag(chr, 0x43);
            BtlChar_SetFlag(chr, 0x44);
            BtlChar_SetFlag(chr, 0x45);
        } else {
            BtlChar_SetFlag(chr, 0x42);
            BtlChar_SetFlag(chr, 0x43);
            BtlChar_SetFlag(chr, 0x44);
            BtlChar_SetFlag(chr, 0x45);
            BtlChar_SetFlag(chr, 0x46);
        }
        if (chr->thr.unk38 != 0) {
            BtlChar_SetFlag(chr, 0x97);
            BtlChar_SetFlag(chr, 0xA);
        }
    }
    if (phase == 2) {
        if (chr->thr.unk44 != 0) {
            if (!BtlChar_TestFlag(chr, 0xAF)) {
                chr->work[1] = 1;
            }
        }
        if (chr->work[1] != 0) {
            s32 next;

            BtlChar_SetHeldFlag(chr, 0x1D);
            BtlChar_SetHeldFlag(chr, 0x1E);
            next = BtlAct_GetLandingAction(chr, slot, chr->thr.unk14);
            if (chr->thr.unk30 != 0) {
                f32 yaw;
                f32 addYaw;
                f32 pitch;

                BtlChar_SetFlag(chr, 0x94);
                yaw = BtlOpp_GetTargetYaw(chr);
                addYaw = chr->thr.unk60;
                pitch = chr->thr.unk64;
                if (chr->react.back != 0) {
                    chr->react.turnYaw = BtlUtil_WrapAngle(yaw + addYaw);
                    chr->react.turnPitch = pitch;
                } else {
                    chr->react.turnYaw = BtlUtil_WrapAngle(BtlUtil_WrapAngle(yaw + addYaw) + 3.14159265f);
                    chr->react.turnPitch = -pitch;
                }
            } else {
                if (chr->react.back != 0) {
                    f32 yaw = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw + 3.14159265f);

                    BtlMove_SetHeading(chr, yaw, BtlChar_GetPos(chr)->pitch);
                }
            }
            if (chr->react.back != 0) {
                BtlChar_SetFlag(chr, 0x35);
            }
            BtlAct_Request(chr, next);
        }
    }
    if (phase == 3) {
        BtlChar_SetFlag(chr, 0x33);
    }
}

/*
 * Action 0x105 (slot 4; the technique queue builder in btl_act_j.c puts it in front of an Ultimate Blast): motion 0x16B with camera
 * cut 3 and level 2 hit-stop on every frame, so the whole battle is frozen around the pose. Ends on the animation
 * event flag 0x31 by starting the queued technique action. Raises flag 0xA3 when that action is 0x114.
 */
s32 BtlAct_UltimateStartHandler(ActGChr *chr, s32 phase) {
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, 0x16B, 0.0f);
        BtlSuper_Begin(chr, slot, 1);
        BtlSuper_PlayStartSound(chr, slot, 1);
        BtlMove_TurnYaw(chr, 2, 3.14159265f);
        BtlMove_TurnPitch(chr, 5, 3.14159265f);
        BtlMove_TurnModelYaw(chr, 3.14159265f, 1.0f);
        BtlSuper_SetClassFlag(chr, slot, 1);
        ChrCam_RequestCut(chr, 0, 3);
        BtlChar_GetPos(chr)->speed = 0.0f;
        BtlChar_GetPos(chr)->fallSpeed = 0.0f;
        BtlChar_ResetLook(chr);
    }
    if (phase == 1) {
        BtlAnim_Advance(chr, 0);
        BtlSuper_SetClass(chr, slot);
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFxBit(chr, 0x10);
        BtlSuper_RequestHitStop(chr, 0);
        if (BtlAct_GetQueued(chr) == 0x114) {
            BtlChar_SetFlag(chr, 0xA3);
        }
        if (BtlInput_TestAction(chr, 0x6F, 1)) {
            BtlChar_SetHeldFlag(chr, 0x9E);
            BtlChar_SetHeldFlag(chr, 0x9D);
            BtlChar_SetHeldFlag(chr, 0x9C);
            if (BtlChar_IsFlagRaised(chr, 0x9C)) {
                BtlCharSnd_PlayCommon(chr, 0x14);
                BtlCharSnd_PlayCommon(chr, 0x15);
                BtlCharSnd_PlayCommon(chr, 0x1C);
            }
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x31)) {
            BtlAct_Request(chr, BtlAct_GetQueued(chr));
        }
    }
}

/*
 * Actions 0x130..0x132 (forced by flag 0xAA: BtlAct_CheckForced requests 0x12E + slot): both fighters during
 * clash A. A technique of kind 1 (BtlSuper_GetHitDirKind, a beam) holds motion 0x16C, anything else the dash loop;
 * both are aimed up / down with BtlAct_SetPitchMotion unless technique flag B 0x1000 is set.
 *
 * The score is chr->clashPower (fighter + 0xE4C, "clashCountA" in btl_char_flag.h): BtlSuper_GetUnk1E(chr, slot) / 20,
 * + 10 / + 6 / + 3 for ability 0x14 / 0x13 / 0x12, - 10 / - 6 / - 3 for ability 0x6A / 0x69 / 0x68, and + 1 for
 * every frame from frame 16 on in which input 51 (a direction command) tests true. BtlClash_Update
 * (btl_char_flag_clash.c) compares the two scores every frame and raises the result flags read here:
 * 0xC3 (winner) -> action 0xB, 0xC2 (loser) -> action 0x104, 0x72 (the winner's catch connected) -> the rush
 * sequence.
 * Every frame: camera shake 5.0, vibration, recoil backwards, flags 0x42..0x46, 0x134, 0x128.
 */
void BtlAct_ClashStruggleHandler(ActGChr *chr, s32 phase) {
    extern s32 TagAssist_Handle(s32,s32);
    s32 slot = BtlAct_GetCurrentClass(chr);
    if(TagAssist_Handle(chr->player,phase)) return;

    if (phase == 0) {
        s32 anim = SUPER_ANIM(slot, 3);

        if (BtlSuper_GetHitDirKind(chr, slot) == 1) {
            anim = 0x16C;
        }
        BtlAnim_Play(chr, anim, 0.15f);
        BtlChar_ClearFlag(chr, 0xAA);
        BtlChar_ClearFlagRange(chr, 0xBF, 0xC3);
        BtlChar_ClearFlag(chr, 0x11F);
        chr->clashPower = BtlSuper_GetUnk1E(chr, slot) / 20;
        if (BtlMember_HasAbility(chr, 0x14)) {
            chr->clashPower += 10;
        } else if (BtlMember_HasAbility(chr, 0x13)) {
            chr->clashPower += 6;
        } else if (BtlMember_HasAbility(chr, 0x12)) {
            chr->clashPower += 3;
        }
        if (BtlMember_HasAbility(chr, 0x6A)) {
            chr->clashPower -= 10;
        } else if (BtlMember_HasAbility(chr, 0x69)) {
            chr->clashPower -= 6;
        } else if (BtlMember_HasAbility(chr, 0x68)) {
            chr->clashPower -= 3;
        }
    }
    if (phase == 1) {
        s32 up = -1;
        s32 down = -1;

        BtlAnim_AdvanceLoop(chr, 0);
        switch (BtlAnim_GetId(chr)) {
            case 0x108:
            case 0x12A:
            case 0x14C:
                up = SUPER_ANIM(slot, 0xA);
                down = SUPER_ANIM(slot, 0x11);
                break;
            case 0x16C:
                up = 0x16E;
                down = 0x170;
                break;
        }
        if (up >= 0 && down >= 0 && !(BtlSuper_GetFlags(chr, slot) & 0x1000)) {
            BtlAct_SetPitchMotion(chr, up, down, 1);
        }
        BtlMove_Step(chr, 2, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x128);
        BtlChar_SetFxBit(chr, 0x10);
        ChrCam_AddShake(chr, 5.0f, 0.1f);
        BtlChar_SetFlag(chr, 0xE6);
        BtlChar_Vibrate(chr, 1.0f, 0.1f);
        BtlChar_SetFlag(chr, 0xD2);
        BtlAct_PushAngle(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y + 3.14159265f), 1.0f, 5.0f);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 0x134);
        if (chr->actionFrame >= 0x10 && BtlInput_TestAction(chr, 0x33, 1)) {
            chr->clashPower++;
#ifdef PORT /* PC build: BT3_CLASH_LOG=1 says what a clash counts (testing the Square option) */
            Port_ClashLog(chr->player, "beam clash", 1, chr->clashPower, chr->actionFrame);
#endif
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0xC3)) {
            BtlAct_Request(chr, 0xB);
        }
        if (BtlChar_TestFlag(chr, 0xC2)) {
            BtlAct_Request(chr, 0x104);
        }
        if (BtlChar_TestFlag(chr, 0x72)) {
            BtlAct_Request(chr, slot + 0x12B);
        }
    }
    if (phase == 3) {
        BtlSuper_Leave(chr, slot);
        BtlChar_ClearFlagRange(chr, 0xBF, 0xC3);
#ifdef PORT
        Port_ClashLog(chr->player, "beam clash", 0, chr->clashPower, chr->actionFrame);
#endif
        chr->clashPower = 0;
    }
}

/*
 * Action 0x104 (forced by flag 0xAB "hit by a finishing technique", or requested by the struggle on flag 0xC2):
 * motion 0x172 with camera cut 0x21 + player, pushed backwards, under level 2 hit-stop (flag 0x125) on every
 * frame and with holdAction set, so nothing can interrupt it. If the active member still wears the undamaged
 * model (gauge variant 0) the model is reloaded as variant 1 (BtlAct_SetFormCurrent, request on frame 1): the
 * action then also waits for BtlChange_IsLoadedFor before it ends. Next action 0xD9; leaving adds 4 s to the
 * stage timer and clears the clash flags 0xBF..0xC3.
 */
void BtlAct_ClashLostHandler(ActGChr *chr, s32 phase) {
    s32 *step = &chr->work[2];

    if (phase == 0) {
        BtlAnim_Play(chr, 0x172, 0.0f);
        ChrCam_RequestCut(chr, 1, (chr->player & 1) + 0x21);
        if (BtlMember_GetActiveGauge(chr)->variant == 0) {
            chr->work[0] |= 1;
            BtlAct_SetFormCurrent(chr);
        }
    }
    if (phase == 1) {
        if ((chr->work[0] & 1) && chr->actionFrame == 1) {
            BtlChar_SavePlacement(chr);
            BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.unk14, chr->form.unk18,
                                   chr->form.unk1C, chr->form.unk20);
        }
        if (BtlAnim_Advance(chr, 0)) {
            chr->work[0] |= 2;
        }
        if (chr->work[0] & 2) {
            if (chr->work[0] & 1) {
                switch (*step) {
                    case 0:
                        if (BtlChange_IsLoadedFor(chr->player)) {
                            BtlAnim_Request(chr, 0x172, 0.0f);
                            BtlChange_SetReady(chr->player);
                            ++*step;
                        }
                        break;
                    case 1:
                        BtlAnim_JumpToEnd(chr);
                        BtlAnim_EnableHandle(chr);
                        BtlChange_SetDone(chr->player);
                        BtlChar_SetFlag(chr, 0xF5);
                        BtlChar_SetFlag(chr, 0x55);
                        chr->work[1] = 1;
                        break;
                }
            } else {
                chr->work[1] = 1;
            }
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x125);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x128);
        BtlChar_SetFlag(chr, 0xD2);
        BtlChar_SetFlag(chr, 0x95);
        BtlChar_SetFlag(chr, 0x96);
        BtlAct_PushAngle(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y + 3.14159265f), 1.0f, 5.0f);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        chr->holdAction = 1;
    }
    if (phase == 2) {
        if (chr->work[1] != 0) {
            BtlChar_SetHeldFlag(chr, 0x1D);
            BtlChar_SetHeldFlag(chr, 0x1E);
            BtlAct_Request(chr, 0xD9);
        }
    }
    if (phase == 3) {
        BtlChar_ClearFlagRange(chr, 0xBF, 0xC3);
        BtlChar_AddStageTimer(4.0f);
    }
}

/*
 * Actions 0x136..0x138 (queued for hit reaction 0x29, slot from the reaction block): motion 0x21 of the slot while
 * moving straight up at 1500 km/h; flags 0xD5, 0x95, 0x96. Action 0xD0 when the motion ends.
 */
s32 BtlAct_SuperLaunchedUpHandler(ActGChr *chr, s32 phase) {
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (phase == 0) {
        BtlAnim_Play(chr, SUPER_ANIM(slot, 0x21), 0.0f);
    }
    if (phase == 1) {
        if (BtlAnim_Advance(chr, 0)) {
            BtlAct_Request(chr, 0xD0);
        }
        BtlMove_Step(chr, 6, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_MoveVertical(chr, BTL_KMH(-1500.0f), 10000.0f);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x95);
        BtlChar_SetFlag(chr, 0x96);
        BtlChar_SetFxBit(chr, 0x3B);
    }
}

/*
 * Action 0x103: saves the placement, requests one of six fixed characters (BtlAct_SetFormRandom, picked with
 * BtlChar_FrameMod(6)), waits for the load, swaps (flags 0xF5, 0x55) and returns to action 0xB. Level 2 hit-stop
 * on every frame: the battle is frozen until the model has loaded.
 */
s32 BtlAct_ChangeRandomCharaHandler(ActGChr *chr, s32 phase) {
    s32 *step = &chr->work[2];

    if (phase == 1) {
        switch (*step) {
            case 0:
                BtlChar_SavePlacement(chr);
                BtlAct_SetFormRandom(chr);
                BtlChange_RequestChara(chr->player, chr->form.chara, chr->form.costume, chr->form.unk14,
                                       chr->form.unk18, chr->form.unk1C, chr->form.unk20);
                ++*step;
                break;
            case 1:
                if (BtlChange_IsLoadedFor(chr->player)) {
                    BtlChange_SetReady(chr->player);
                    BtlAnim_Request(chr, 0, 0.0f);
                    ++*step;
                }
                break;
            case 2:
                BtlChange_SetDone(chr->player);
                BtlChar_SetFlag(chr, 0xF5);
                BtlChar_SetFlag(chr, 0x55);
                BtlAct_Request(chr, 0xB);
                break;
        }
        BtlChar_SetFlag(chr, 0x125);
        BtlChar_SetFlag(chr, 0x136);
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
    }
}

/*
 * Fills the damage queue (chr + 0xD94) for a throw from the current attack record: total = BtlAtk_GetDamage,
 * spread over the animation's hit events (mask 0x200000000, + 1 with extraHit) as total / (hits + 5) rounded up to
 * 10 when that does not exceed the total. Unless the opponent's parameter word has bit 0x80, health and ki drains
 * (BtlAtk_GetThrowParamC / E) run between the animation's 0x20000 and 0x40000 events.
 */
void BtlActThrow_SetupDamage(ActGChr *chr, s32 extraHit) {
    ActGObj *obj = BtlChar_GetObj(chr);
    s32 damage;
    s32 rem;
    s32 immune;
    s32 drainKi;
    s32 drainHealth;
    s32 from;
    s32 to;
    s32 span;

    chr->queue.hits = BtlObjAnim_QueryEvent(obj, 0x200000000, 0, 3);
    if (extraHit) {
        chr->queue.hits++;
    }
    damage = BtlAtk_GetDamage(chr);
    chr->queue.unk0 = damage;
    chr->queue.total = damage;
    chr->queue.perHit = damage / (chr->queue.hits + 5);
    chr->queue.flags = 0x10000000;
    if (damage <= 0) {
        chr->queue.flags = 0x10000200;
    }
    rem = chr->queue.perHit % 10;
    if (rem) {
        s32 up = chr->queue.perHit - rem + 10;

        if (up * chr->queue.hits < chr->queue.unk0) {
            chr->queue.perHit = up;
        }
    }
    chr->queue.unk14 = 0;
    chr->queue.drainHealth = 0;
    chr->queue.drainHealthStep = 0;
    chr->queue.unk20 = 0;
    chr->queue.drainKi = 0;
    chr->queue.drainKiStep = 0;
    chr->queue.drainFrom = -1;
    chr->queue.drainTo = -1;
    immune = BtlOpp_GetParamWord0(chr) & 0x80;
    if (!immune) {
        drainHealth = BtlAtk_GetThrowParamC(chr);
        drainKi = BtlAtk_GetThrowParamE(chr);
        if (drainHealth > 0 || drainKi > 0) {
            from = BtlObjAnim_QueryEvent(obj, 0x20000, 0, 0);
            to = BtlObjAnim_QueryEvent(obj, 0x40000, 0, 2);
            span = (to - from) / 2;
            if (span <= immune) {
                span = 1;
            }
            chr->queue.unk14 = drainHealth;
            chr->queue.drainHealth = drainHealth;
            chr->queue.drainHealthStep = drainHealth / span + 1;
            chr->queue.unk20 = drainKi;
            chr->queue.drainKi = drainKi;
            chr->queue.drainKiStep = drainKi / span + 1;
            chr->queue.drainFrom = from;
            chr->queue.drainTo = to;
        }
    }
}

/* Turns the heading by minus the yaw of the first root rotation key of the current animation. */
void BtlActThrow_TurnByRootYaw(ActGChr *chr) {
    ActGAnim *anim;
    ActGTrack *track;
    Quat q;
    Vec4 euler;
    f32 yaw;
    s32 count;
    u64 packed;

    anim = BtlChar_GetObj(chr)->anim;
    if (anim != NULL && anim->rootTrack != 0) {
        yaw = 0.0f;
        track = (ActGTrack *)((u32 *)anim + anim->rootTrack);
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
        BtlMove_SetHeading(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw - yaw), 0.0f);
    }
}
