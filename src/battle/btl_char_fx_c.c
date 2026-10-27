#include "common.h"
#include "battle/btl_char_fx.h"

/*
 * Fighter effect requests, part 3: 0x1D1EC8..0x1D3B40.
 * Animation events of the fighter's model (BtlAnim_TestAttr), the per-frame passes the fighter manager calls, the
 * stage effect shared by the roster, and the partner object (a second character model attached to a fighter by the
 * object load job, fighter + 0x1330).
 */

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
extern f32 BtlUtil_WrapAngle(f32 a);
extern FxObj *BtlChar_GetObj(FxChr *chr);
extern FxPose *BtlChar_GetPos(FxChr *chr);
extern s32 BtlChar_TestFlag(FxChr *chr, s32 flag);
extern void BtlChar_SetFxBit(FxChr *chr, s32 bit);          /* set */
extern s32 BtlChar_TestFxBit(FxChr *chr, s32 bit);           /* test */
extern void BtlCharApi_GetNodePos(s32 objId, s32 part, Vec4 *out); /* world position of a model part */
extern void BtlCharSnd_PlayCommon(FxChr *chr, s32 sound);
extern void BtlObj_SetColorMode(FxObj *obj, s32 bit, s32 on);
extern void EftShotFx_Start(s32 objId, s32 kind);
extern void EftImpact_SpawnHit(FxPosArg *arg);
extern s32 *BtlMember_GetActive(FxChr *chr);
extern void BtlObj_SetEyeFrame(FxObj *obj, s32 arg);
extern s32 BtlObj_GetMouthMode(FxObj *obj);
extern void BtlObj_SetSubState(FxObj *obj, s32 state, s32 arg);
extern s32 BtlAnim_TestAttr(FxChr *chr, u64 mask);           /* animation event bits raised this frame */
extern s32 BtlAnim_GetId(FxChr *chr);                      /* the current animation number */
extern s32 BtlAnim_GetFlags(s32 anim);                        /* its attribute bits */
extern s32 BtlOpp_GetPlayer(FxChr *chr);                      /* the opponent's roster index */
extern s32 BtlAct_GetCurrent(FxChr *chr);
extern s32 BtlAct_IsTechniqueId(s32 action);
extern s32 BtlAct_GetCurrentClass(FxChr *chr);
extern void BtlAct_PushAngle(FxChr *chr, f32 yaw, f32 a, f32 b);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);
extern s32 BtlCharApi_IsInSkill(s32 objId);
extern s32 BtlCharApi_HasWeaponOut(s32 objId);
extern s32 BtlAtk_GetId(FxChr *chr);
extern s32 BtlAtk_GetFlags(FxChr *chr);
extern s32 BtlAtk_GetHitSoundLevel(FxChr *chr);
extern s32 BtlSuper_GetFlagsA(FxChr *chr, s32 slot);
extern s32 BtlSkill_GetId(FxChr *chr, s32 slot);
extern s32 BtlChar_GetHitSoundLine(s32 kind);
extern s32 BtlObjAnim_GetEventArg(FxObj *obj, u64 mask);
extern s32 BtlObjAnim_MaskToNode(s32 bits);
extern s32 BtlObjAnim_QueryEvent(FxObj *obj, u64 a, s32 b, s32 c);
extern void BtlObj_GetNodeVelocity(FxObj *obj, s32 part, Vec4 *out);
extern void Mtx_MulVec4(Vec4 *out, void *mtx, Vec4 *in);
extern void EftSpdLine_SpawnPartStreaks(s32 objId, Vec4 *pos, s32 part);
extern s32 BtlChar_FrameMod(s32 n);
extern void BtlChar_Vibrate(FxChr *chr, f32 power, f32 seconds);
extern void BtlChar_SetHeldFlag(FxChr *chr, s32 flag);
extern void BtlChar_ClearFlag(FxChr *chr, s32 flag);
extern FxChr *BtlChar_Get(s32 i);
extern s32 BtlChar_IsFrozen(FxChr *chr);
extern void BtlChar_PlayVoice(FxChr *chr, s32 kind);
extern void ChrCam_AddShake(FxChr *chr, f32 strength, f32 time);
extern s32 BtlObjAnim_TestEvent(FxObj *obj, u64 mask);
extern void BtlCharSnd_RequestAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far);
extern void BtlObjXf_Update(FxObj *obj);
extern void BtlObjAnim_PlayModel(FxObj *obj, s32 anim, s32 arg);
extern void BtlObj_SaveNodePositions(FxObj *obj, s32 arg);
extern void BtlObjAnim_SamplePose(FxObj *obj);
extern void BtlObjAnim_UpdateEvents(FxObj *obj);
extern void BtlObjPose_CalcMatrices(FxObj *obj);
extern void BtlObj_UpdateChains(FxObj *obj);
extern void BtlObj_CopyLipTables(FxObj *obj, FxObj *owner);
extern void BtlObj_AddPush(FxObj *obj, Vec4 *v, f32 arg);
extern void BtlObj_AddSway(FxObj *obj, f32 a, f32 b);
extern void StgTint_Start(s32 a, s32 b, f32 time);
extern void BtlChar_SpawnFxBits3C(FxChr *chr);
extern void BtlChar_SpawnFxBits0(FxChr *chr);
extern void BtlChar_SpawnHitFx(FxChr *chr);
extern s32 BtlChar_TestPrevFxBit(FxChr *chr, s32 bit);
extern s32 BtlChar_GetCount(void);
extern FxObj *BtlObj_Get(s32 id);
extern s32 BtlObj_Destroy(s32 id);
extern void BtlRes_Free(s32 number);
extern void Vec4_Scale(Vec4 *out, Vec4 *in, f32 s);
extern FxSave *gSaveData;

/*
 * Animation events 0x100 / 0x80 (light / heavy impact): flash at the event's model part and camera shake (only while
 * the object is drawn), sound, and voice kind 1 for the fighter that takes the blow (animation flag 0x10 picks which
 * of the two; not when the other has flag 0x13B or the speaker is frozen).
 * `work` is the alternate sound and later the fighter that speaks: only one variable for both gives the original's
 * register assignment (s1 before the flag in s2).
 */
void BtlFx_HandleImpactEvents(FxChr *chr) {
    FxPosArg arg;
    f32 time = 0.0f;
    s32 work = 0; /* the alternate sound, then the fighter that speaks: one variable in the original (see below) */
    s32 on = 0;
    s32 kind = 0;
    s32 altKind = 0;
    s32 mask = 0;
    s32 sound = 0;
    f32 strength = time;
    FxObj *obj = BtlChar_GetObj(chr);
    FxChr *other;
    s32 ok;

    if (BtlAnim_TestAttr(chr, 0x100)) {
        kind = 0xF;
        if (BtlAct_GetCurrentClass(chr) < 0) {
            kind = 0;
        }
        strength = 5.0f;
        sound = BtlChar_FrameMod(2) + 2;
        time = 0.1f;
        on = 1;
        mask = 0x100;
        altKind = 9;
        work = 0x4D;
    }
    if (BtlAnim_TestAttr(chr, 0x80)) {
        kind = 3;
        if (!(BtlAct_GetCurrentClass(chr) < 0)) {
            kind = 0x10;
        }
        strength = 10.0f;
        time = 0.2f;
        on = 1;
        mask = 0x80;
        altKind = 0xA;
        sound = 0x49;
        work = 0x36;
    }
    if (on) {
        if (!BtlCharApi_IsInRushSequence(chr->objId) && BtlCharApi_HasWeaponOut(chr->objId)) {
            sound = work;
            kind = altKind;
        }
        if (obj->flags & 2) {
            if (!BtlAnim_TestAttr(chr, 0x8000000)) {
                BtlCharApi_GetNodePos(chr->objId, BtlObjAnim_MaskToNode(BtlObjAnim_GetEventArg(obj, mask)), &arg.pos);
                arg.pos.w = 1.0f;
                arg.pos2.x = Mathf_Sin(BtlChar_GetPos(chr)->rot.y);
                arg.pos2.y = 0.0f;
                arg.pos2.z = Mathf_Cos(BtlChar_GetPos(chr)->rot.y);
                arg.kind = kind;
                arg.objId = chr->objId;
                arg.pos2.w = 0.0f;
                arg.unk28 = 0;
                EftImpact_SpawnHit(&arg);
            }
            ChrCam_AddShake(chr, strength, time);
        }
        BtlCharSnd_PlayCommon(chr, sound);
        if (BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x10) {
            work = (s32)chr;
            other = BtlChar_Get(BtlOpp_GetPlayer((FxChr *)work));
        } else {
            other = chr;
            work = (s32)BtlChar_Get(BtlOpp_GetPlayer(other));
        }
        ok = !BtlChar_TestFlag(other, 0x13B);
        if (BtlChar_IsFrozen((FxChr *)work)) {
            ok = 0;
        }
        if (ok) {
            BtlChar_PlayVoice((FxChr *)work, 1);
        }
    }
}

/* Animation event 1: raises request 0x1B when the current technique has attribute 0x80000 and the event value is 1. */
void BtlFx_HandleEvent1(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (BtlAnim_TestAttr(chr, 1)) {
        if (BtlAtk_GetId(chr) != -1) {
            if (BtlAtk_GetFlags(chr) & 0x80000) {
                if (BtlObjAnim_QueryEvent(obj, 1, 0, 5) == 1) {
                    BtlChar_SetFxBit(chr, 0x1B);
                }
            }
        }
    }
}

/* Animation events 0x2000000 / 0x4000000: set / clear fighter flag 0x30. */
void BtlFx_HandleFlag30Events(FxChr *chr) {
    if (BtlAnim_TestAttr(chr, 0x2000000)) {
        BtlChar_SetHeldFlag(chr, 0x30);
    }
    if (BtlAnim_TestAttr(chr, 0x4000000)) {
        BtlChar_ClearFlag(chr, 0x30);
    }
}

/* Animation event 2: the swing sound of the current attack (not with flag 0x91, nor with 0x60 unless 0x5C). */
void BtlFx_PlaySwingSound(FxChr *chr) {
    s32 sound;

    if (!BtlChar_TestFlag(chr, 0x91)) {
        if (BtlAnim_TestAttr(chr, 2)) {
            if (!BtlChar_TestFlag(chr, 0x60) || BtlChar_TestFlag(chr, 0x5C)) {
                sound = BtlChar_GetHitSoundLine(BtlAtk_GetHitSoundLevel(chr));
                if (sound >= 0) {
                    BtlCharSnd_PlayCommon(chr, sound);
                }
            }
        }
    }
}

/* Animation event 8: effect 0x15F3D8 at each model part named by the event's value bits (19 of them). */
void BtlFx_SpawnPartFxEvent8(FxChr *chr) {
    Vec4 pos;
    FxObj *obj = BtlChar_GetObj(chr);
    s32 bits;
    s32 i;
    s32 part;
    s32 bit;

    if (BtlAnim_TestAttr(chr, 8)) {
        bits = BtlObjAnim_GetEventArg(obj, 8);
        for (i = 0; i < 0x13; i++) {
            bit = bits & (1U << i);
            if (bit != 0) {
                part = BtlObjAnim_MaskToNode(bit);
                BtlObj_GetNodeVelocity(obj, part, &pos);
                pos.w = 0.0f;
                Mtx_MulVec4(&pos, obj->mtx, &pos);
                EftSpdLine_SpawnPartStreaks(chr->objId, &pos, part);
            }
        }
    }
}

/* Animation event 0x1000000: effect 0xC at each model part named by the event's value bits, with sound 0x4B. */
void BtlFx_SpawnPartFlashes(FxChr *chr) {
    FxPosArg arg;
    FxObj *obj = BtlChar_GetObj(chr);
    s32 bits;
    s32 i;
    s32 bit;

    if (BtlAnim_TestAttr(chr, 0x1000000)) {
        bits = BtlObjAnim_GetEventArg(obj, 0x1000000);
        if (bits != 0) {
            arg.pos2.x = Mathf_Sin(BtlChar_GetPos(chr)->rot.y);
            arg.pos2.y = 0.0f;
            arg.pos2.z = Mathf_Cos(BtlChar_GetPos(chr)->rot.y);
            arg.kind = 0xC;
            arg.objId = chr->objId;
            arg.pos2.w = 0.0f;
            arg.unk28 = 0;
            for (i = 0; i < 0x20; i++) {
                bit = bits & (1U << i);
                if (bit != 0) {
                    BtlCharApi_GetNodePos(chr->objId, BtlObjAnim_MaskToNode(bit), &arg.pos);
                    arg.pos.w = 1.0f;
                    EftImpact_SpawnHit(&arg);
                }
            }
            BtlCharSnd_PlayCommon(chr, 0x4B);
        }
    }
}

/*
 * Animation event 0x400 in a damage action (not with class attribute 0x1000000): sets flag 0x9F, and while that flag
 * lasts pushes the fighter, and its partner object, backwards (yaw + pi, speed 1, 5). Not void in the original.
 */
s32 BtlFx_PushBackOnEvent400(FxChr *chr) {
    if (BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr))) {
        if (BtlCharApi_IsInRushSequence(chr->objId)) {
            if (!(BtlSuper_GetFlagsA(chr, BtlAct_GetCurrentClass(chr)) & 0x1000000)) {
                if (BtlAnim_TestAttr(chr, 0x400)) {
                    BtlChar_SetHeldFlag(chr, 0x9F);
                }
                if (BtlChar_TestFlag(chr, 0x9F)) {
                    BtlAct_PushAngle(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y + 3.14159265f), 1.0f, 5.0f);
                    if (BtlPartner_IsActive(chr)) {
                        BtlPartner_PushAngle(chr, BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y + 3.14159265f), 1.0f, 5.0f);
                    }
                }
            }
        }
    }
}

/* Animation event 0x400 in a technique of class 0x18: sets object mask bit 2. Not void in the original (no tail call). */
s32 BtlFx_SetObjMaskOnEvent400(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (BtlCharApi_IsInSkill(chr->objId)) {
        if (BtlSkill_GetId(chr, BtlAct_GetCurrentClass(chr)) == 0x18) {
            if (BtlAnim_TestAttr(chr, 0x400)) {
                BtlObj_SetColorMode(obj, 2, 1);
            }
        }
    }
}

static inline void Fx_ShakeCam(FxChr *chr, f32 strength) {
    ChrCam_AddShake(chr, strength, 0.3f);
}

/* Animation event bit 46: camera shake 3.0 for 0.3 s. */
void BtlFx_ShakeCamOnEvent(FxChr *chr) {
    if (BtlAnim_TestAttr(chr, 0x400000000000)) {
        Fx_ShakeCam(chr, 3.0f);
    }
}

static inline void Fx_Vibrate(FxChr *chr, f32 power) {
    BtlChar_Vibrate(chr, power, 0.2f);
}

/* Animation event bit 45: pad vibration 1.0 for 0.2 s. */
void BtlFx_VibrateOnEvent(FxChr *chr) {
    if (BtlAnim_TestAttr(chr, 0x200000000000)) {
        Fx_Vibrate(chr, 1.0f);
    }
}

/* Object animation events: bit 42 spawns effect 0 of module 0x180A00; 0x20 / 0x40 and bits 43 / 44 set / clear object flags 0x20 and 0x40. */
void BtlFxObj_HandleFlagEvents(FxObj *obj) {
    if (BtlObjAnim_TestEvent(obj, 0x40000000000)) {
        EftShotFx_Start(obj->id, 0);
    }
    if (BtlObjAnim_TestEvent(obj, 0x20)) {
        obj->flags |= 0x20;
    }
    if (BtlObjAnim_TestEvent(obj, 0x40)) {
        obj->flags &= ~0x20;
    }
    if (BtlObjAnim_TestEvent(obj, 0x80000000000)) {
        obj->flags |= 0x40;
    }
    if (BtlObjAnim_TestEvent(obj, 0x100000000000)) {
        obj->flags &= ~0x40;
    }
}

/* Object animation events that are sounds, played at model part 3 (audible from 200 to 1500 units). */
void BtlFxObj_PlayEventSounds(FxObj *obj) {
    Vec4 pos;
    s32 id;

    BtlCharApi_GetNodePos(obj->id, 3, &pos);
    if (BtlObjAnim_TestEvent(obj, 0x10)) {
        BtlCharSnd_RequestAt(&pos, 0, BtlChar_FrameMod(3) + 0x28, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x10000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x1E, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x20000000)) {
        BtlCharSnd_RequestAt(&pos, 0, BtlChar_FrameMod(2), 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x40000000)) {
        BtlCharSnd_RequestAt(&pos, 0, BtlChar_FrameMod(3) + 6, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x800000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x20, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x1000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x4D, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x2000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x25, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x4000000000)) {
        id = 0x33;
        if (obj->action != NULL) {
            if (obj->action->attr & 0x8000) {
                id = 0xB;
            }
        }
        BtlCharSnd_RequestAt(&pos, 0, id, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x10000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x30, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x20000000000)) {
        BtlCharSnd_RequestAt(&pos, 4, 0x8D3B, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x80000000000000)) {
        BtlCharSnd_RequestAt(&pos, 4, 0x8D3D, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x800000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x48, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x1000000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x12, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x2000000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x18, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x4000000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x2C, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x8000000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x1F, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x10000000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, 0x31, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x20000000000000)) {
        BtlCharSnd_RequestAt(&pos, 0, BtlChar_FrameMod(2) + 4, 200.0f, 1500.0f);
    }
    if (BtlObjAnim_TestEvent(obj, 0x80000000)) {
        BtlCharSnd_RequestAt(&pos, 1, BtlChar_FrameMod(2), 200.0f, 1500.0f);
    }
}

/* Object animation events bits 56..60: face sub-states 9..11, command 2, and "back to sub-state 0, then command 9". */
void BtlFxObj_HandleSubStateEvents(FxObj *obj) {
    if (BtlObjAnim_TestEvent(obj, 0x100000000000000)) {
        BtlObj_SetSubState(obj, 9, 0);
    }
    if (BtlObjAnim_TestEvent(obj, 0x200000000000000)) {
        BtlObj_SetSubState(obj, 0xA, 0);
    }
    if (BtlObjAnim_TestEvent(obj, 0x400000000000000)) {
        BtlObj_SetSubState(obj, 0xB, 0);
    }
    if (BtlObjAnim_TestEvent(obj, 0x800000000000000)) {
        BtlObj_SetEyeFrame(obj, 2);
    }
    if (BtlObjAnim_TestEvent(obj, 0x1000000000000000)) {
        switch (BtlObj_GetMouthMode(obj)) {
        case 9:
        case 10:
        case 11:
        case 12:
            BtlObj_SetSubState(obj, 0, 0);
            break;
        }
        BtlObj_SetEyeFrame(obj, 9);
    }
}

/* Manager pass (action stage): the object events of the fighter's own model. */
void BtlFx_UpdateObjEvents(FxChr *chr) {
    BtlFxObj_HandleSubStateEvents(BtlChar_GetObj(chr));
}

/* Manager pass (camera stage): every effect request and animation event of the fighter, in a fixed order. */
void BtlFx_UpdateAll(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    BtlFx_UpdateAura(chr);
    BtlFx_UpdateChargeFx(chr);
    BtlChar_SpawnFxBits3C(chr);
    BtlFx_FireKiBlast(chr);
    BtlFx_SpawnHitSparkReq17(chr);
    BtlChar_SpawnFxBits0(chr);
    BtlFx_UpdateObjFlagReq3(chr);
    BtlFx_HandleImpactEvents(chr);
    BtlFx_HandleFlag30Events(chr);
    BtlFx_PlaySwingSound(chr);
    BtlFxObj_HandleFlagEvents(obj);
    BtlFxObj_PlayEventSounds(obj);
    BtlFx_UpdatePowerUpLook(chr);
    BtlFx_SpawnReq6(chr);
    BtlFx_UpdateScreenFilter(chr);
    BtlFx_SpawnBurstReqC(chr);
    BtlFx_SpawnPartFxEvent8(chr);
    BtlFx_SpawnFastMoveFx(chr);
    BtlFx_SpawnSpeedLines(chr);
    BtlFx_SpawnReq18(chr);
    BtlFx_UpdateGroundFx(chr);
    BtlFx_UpdateWaterFx(chr);
    BtlFx_UpdateAimedFxReq28(chr);
    BtlFx_UpdateReq1A(chr);
    BtlFx_UpdateReq38(chr);
    BtlFx_TriggerObjReq1E(chr);
    BtlFx_TriggerObjReq1F(chr);
    BtlFx_SpawnFlashReq2B(chr);
    BtlFx_UpdateObjCmdFlag137(chr);
    BtlFx_UpdateSubStateFlag96(chr);
    BtlFx_UpdateObjFlag80(chr);
    BtlFx_SpawnPartFlashes(chr);
    BtlFx_PushBackOnEvent400(chr);
    BtlFx_SetObjMaskOnEvent400(chr);
    BtlFx_ShakeCamOnEvent(chr);
    BtlFx_VibrateOnEvent(chr);
    BtlFx_UpdateCharaFx(chr);
}

/* Manager pass (stage 10, after hit detection): the requests raised by hits. */
void BtlFx_UpdateAfterHits(FxChr *chr) {
    BtlFx_TriggerObjReq1D(chr);
    BtlFx_HandleEvent1(chr);
    BtlChar_SpawnHitFx(chr);
    BtlFx_SpawnFlashReq20(chr);
    BtlFx_SpawnClashFlash(chr);
}

/* Manager pass (after the effect scene and the fighter camera): requests 0x37 and 0x2E. */
void BtlFx_UpdatePostScene(FxChr *chr) {
    BtlFx_SpawnReq37(chr);
    BtlFx_SpawnFlashReq2E(chr);
}

/* Manager pass (end of frame): the camera-trace object flag. */
void BtlFx_UpdateLate(FxChr *chr) {
    BtlFx_UpdateOccludedFlag(chr);
}

/* Turns requests 0x10 / 0x11 into request 0x12 (the stage effect of BtlFx_UpdateStageFx), and drops flag 0x12E when 0x11 ends. */
void BtlFx_UpdateStageFxRequest(FxChr *chr) {
    s32 on;

    if (BtlChar_TestFxBit(chr, 0x10)) {
        on = 0;
        if (!BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr)) || BtlSuper_GetFlagsA(chr, BtlAct_GetCurrentClass(chr)) >= 0) {
            on = 1;
        }
        if (on) {
            BtlChar_SetFxBit(chr, 0x12);
        }
    }
    if (BtlChar_TestFxBit(chr, 0x11)) {
        if (BtlAnim_TestAttr(chr, 0x200)) {
            BtlChar_SetHeldFlag(chr, 0x12E);
        }
        if (BtlChar_TestFlag(chr, 0x12E)) {
            BtlChar_SetFxBit(chr, 0x12);
        }
    } else {
        BtlChar_ClearFlag(chr, 0x12E);
    }
}

/* Roster pass: starts the stage effect 0x247AF8 when any fighter raises request 0x12, and ends it when none does. */
void BtlFx_UpdateStageFx(void) {
    s32 i;
    s32 prev = 0;
    s32 now = 0;
    FxChr *chr;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlFx_UpdateStageFxRequest(BtlChar_Get(i));
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        now |= BtlChar_TestFxBit(chr, 0x12);
        prev |= BtlChar_TestPrevFxBit(chr, 0x12);
    }
    if (now && !prev) {
        StgTint_Start(0, 0, 0.2f);
    }
    if (!now && prev) {
        StgTint_Start(0, 1, 0.2f);
    }
}

/* Manager step between the roster passes. */
void BtlFx_UpdateRoster(void) {
    BtlFx_UpdateStageFx();
}

/* Places an object at a position and rotation. */
void BtlFxObj_Place(FxObj *obj, Vec4 *pos, Vec4 *rot) {
    Vec4_Copy(&obj->pos, pos);
    Vec4_Copy(&obj->rot, rot);
    BtlObjXf_Update(obj);
}

/*
 * The partner object's animation events: shown / hidden (0x4000000 / 0x2000000), impact flash with the opponent's
 * voice kind 1, the object sounds, and voice lines (sound kind 7, id = base + chara * 100 + line, base 0x8D4E or
 * 0xCC32 with the alternate voices): lines 0x5F..0x62 of the partner's character under fighter flags 0x130 / 0x131,
 * lines 0x57 / 0x59 / 0x5B (+1) of the fighter's character by technique class 2 / 3 / 4. Each line also sets the
 * partner's sub-state 2 to the line number.
 */
void BtlPartner_HandleEvents(FxObj *obj, FxChr *chr) {
    Vec4 pos;
    FxPosArg arg;
    s32 on;
    s32 mask;
    s32 kind;
    s32 sound;

    BtlCharApi_GetNodePos(obj->id, 3, &pos);
    if (BtlObjAnim_TestEvent(obj, 0x2000000)) {
        obj->flags &= ~2;
    }
    if (BtlObjAnim_TestEvent(obj, 0x4000000)) {
        obj->flags |= 2;
    }
    on = 0;
    mask = 0;
    kind = 0;
    sound = 0;
    if (BtlObjAnim_TestEvent(obj, 0x100)) {
        on = 1;
        mask = 0x100;
        sound = BtlChar_FrameMod(2) + 2;
        kind = 0xF;
    }
    if (BtlObjAnim_TestEvent(obj, 0x80)) {
        on = 1;
        mask = 0x80;
        kind = 0x10;
        sound = 0x49;
    }
    if (on) {
        BtlCharApi_GetNodePos(obj->id, BtlObjAnim_MaskToNode(BtlObjAnim_GetEventArg(obj, mask)), &arg.pos);
        arg.pos.w = 1.0f;
        arg.pos2.x = Mathf_Sin(obj->rot.y);
        arg.pos2.y = 0.0f;
        arg.pos2.z = Mathf_Cos(obj->rot.y);
        arg.kind = kind;
        arg.objId = obj->id;
        arg.pos2.w = 0.0f;
        arg.unk28 = 0;
        EftImpact_SpawnHit(&arg);
        BtlCharSnd_RequestAt(&pos, 0, sound, 200.0f, 1500.0f);
        BtlChar_PlayVoice(BtlChar_Get(BtlOpp_GetPlayer(chr)), 1);
    }
    BtlFxObj_HandleFlagEvents(obj);
    BtlFxObj_PlayEventSounds(obj);
    sound = DISC_RESOURCE(0xCC32, 0x16E6A);
    if (!(gSaveData->flags & 1)) {
        sound = DISC_RESOURCE(0x8D4E, 0x10CC2);
    }
    kind = -1;
    mask = -1;
    on = -1;
    if (BtlChar_TestFlag(chr, 0x130)) {
        mask = 0x5F;
        if (!BtlObjAnim_TestEvent(obj, 0x8000)) {
            mask = on;
        }
        if (BtlObjAnim_TestEvent(obj, 0x8000000000)) {
            mask = 0x60;
        }
        if (BtlObjAnim_TestEvent(obj, 0x20000)) {
            mask = 0x61;
        }
    }
    if (BtlChar_TestFlag(chr, 0x131)) {
        if (BtlObjAnim_TestEvent(obj, 0x8000008000)) {
            kind = 0x62;
            mask = 0x62;
        }
    }
    if (kind >= 0) {
        BtlCharSnd_RequestAt(&pos, 7, sound + obj->chara * 100 + kind, 200.0f, 1500.0f);
    }
    if (mask >= 0) {
        BtlObj_SetSubState(obj, 2, mask);
    }
    switch (BtlAct_GetCurrentClass(chr)) {
    case 2:
        on = 0x57;
        break;
    case 3:
        on = 0x59;
        break;
    case 4:
        on = 0x5B;
        break;
    }
    if (on >= 0) {
        if (BtlObjAnim_TestEvent(obj, 0x8000)) {
            BtlCharSnd_RequestAt(&pos, 7, sound + *BtlMember_GetActive(chr) * 100 + on, 200.0f, 1500.0f);
            BtlObj_SetSubState(obj, 2, on);
        }
        if (BtlObjAnim_TestEvent(obj, 0x8000000000)) {
            BtlCharSnd_RequestAt(&pos, 7, sound + (on + *BtlMember_GetActive(chr) * 100) + 1, 200.0f, 1500.0f);
            BtlObj_SetSubState(obj, 2, on + 1);
        }
    }
}

/* Whether the fighter has a partner object. */
s32 BtlPartner_IsActive(FxChr *chr) {
    return chr->partner.active;
}

/* Attaches a partner object (resource slot and battle object) unless one is attached already. */
void BtlPartner_Attach(FxChr *chr, s32 res, s32 objId) {
    FxPartner *p = &chr->partner;

    if (!p->active) {
        p->res = res;
        p->objId = objId;
        p->active = 1;
    }
}

/* Destroys the partner object and frees its resource slot. */
void BtlPartner_Release(FxChr *chr) {
    FxPartner *p = &chr->partner;

    if (p->active) {
        BtlObj_Destroy(p->objId);
        BtlRes_Free(p->res);
        p->res = 0;
        p->objId = 0;
        p->active = 0;
    }
}

/* Manager pass (stage 6): puts the partner on the fighter and runs its animation and pose passes. */
void BtlPartner_Update(FxChr *chr) {
    FxPartner *p = &chr->partner;
    FxObj *obj;
    FxObj *partner;
    Vec4 *pos;

    if (p->active) {
        obj = BtlChar_GetObj(chr);
        pos = &obj->pos;
        partner = BtlObj_Get(p->objId);
        BtlFxObj_Place(partner, pos, &obj->rot);
        BtlObj_SaveNodePositions(partner, 0);
        BtlObjAnim_SamplePose(partner);
        BtlObjAnim_UpdateEvents(partner);
        BtlFxObj_HandleSubStateEvents(partner);
        BtlObjPose_CalcMatrices(partner);
        BtlObj_UpdateChains(partner);
        BtlObjPose_CalcMatrices(partner);
        partner->flags &= ~0x2000000;
    }
}

/* Manager pass (stage 10): the partner's animation events. */
void BtlPartner_UpdateEvents(FxChr *chr) {
    FxPartner *p = &chr->partner;

    if (p->active) {
        BtlPartner_HandleEvents(BtlObj_Get(p->objId), chr);
    }
}

/* Starts the partner animation that belongs to a fighter animation number (those from 0x19E up). */
void BtlPartner_PlayAnim(FxChr *chr, s32 anim) {
    FxPartner *p = &chr->partner;
    FxObj *obj;

    if (!p->active) {
        return;
    }
    if (anim < 0x19E) {
        return;
    }
    obj = BtlObj_Get(p->objId);
    BtlObjAnim_PlayModel(obj, anim - 0x19E, 0);
    if (obj->anim.head == NULL) {
        obj->flags &= ~2;
    } else if (obj->anim.head->unk2 == 0) {
        obj->flags &= ~2;
    }
    obj->flags &= ~0x20;
    obj->flags &= ~0x40;
    switch (BtlObj_GetMouthMode(obj)) {
    case 9:
    case 10:
    case 11:
    case 12:
        BtlObj_SetSubState(obj, 0, 0);
        break;
    }
    obj->anim.unk130 = 1;
}

/* Advances the partner animation by its step; 1 when it has reached its end (or there is no partner). */
s32 BtlPartner_StepAnim(FxChr *chr) {
    FxPartner *p = &chr->partner;
    s32 done = 0;
    FxObjAnim *anim;

    if (!p->active) {
        return 1;
    }
    anim = &BtlObj_Get(p->objId)->anim;
    anim->prevFrame = anim->frame;
    anim->frame += anim->step;
    if (anim->frame >= anim->end) {
        anim->frame = anim->end;
        done = 1;
    }
    return done;
}

/* Sets or clears partner object flag 0x10. */
void BtlPartner_SetFlag10(FxChr *chr, s32 on) {
    FxPartner *p = &chr->partner;
    FxObj *obj;

    if (p->active) {
        obj = BtlObj_Get(p->objId);
        if (on) {
            obj->flags |= 0x10;
        } else {
            obj->flags &= ~0x10;
        }
    }
}

/* Links the partner object to the fighter's object (BtlObj_CopyLipTables). */
void BtlPartner_LinkToOwner(FxChr *chr) {
    FxPartner *p = &chr->partner;

    if (p->active) {
        BtlObj_CopyLipTables(BtlObj_Get(p->objId), BtlChar_GetObj(chr));
    }
}

/* The partner object. Without one the original falls off the end (the caller gets the 0 left in the result register). */
FxObj *BtlPartner_GetObj(FxChr *chr) {
    FxPartner *p = &chr->partner;

    if (p->active) {
        return BtlObj_Get(p->objId);
    }
}

/* Gives the partner a horizontal velocity along a yaw. */
void BtlPartner_PushAngle(FxChr *chr, f32 yaw, f32 speed, f32 arg) {
    FxPartner *p = &chr->partner;
    Vec4 v;
    FxObj *obj;

    if (p->active) {
        obj = BtlObj_Get(p->objId);
        v.x = Mathf_Sin(yaw) * speed;
        v.y = 0.0f;
        v.z = Mathf_Cos(yaw) * speed;
        v.w = 0.0f;
        BtlObj_AddPush(obj, &v, arg);
    }
}

/* Gives the partner a velocity of a given speed along a direction (ignored when the direction is shorter than 0.0001). */
void BtlPartner_PushDir(FxChr *chr, Vec4 *dir, f32 speed, f32 arg) {
    FxPartner *p = &chr->partner;
    Vec4 v;
    FxObj *obj;
    f32 len;

    if (p->active) {
        obj = BtlObj_Get(p->objId);
        len = Vec3_Length(dir);
        if (!(len < 0.0001f)) {
            Vec4_Scale(&v, dir, 1.0f / len);
            Vec4_Scale(&v, &v, speed);
            BtlObj_AddPush(obj, &v, arg);
        }
    }
}

/* Forwards two floats to BtlObj_AddSway on the partner. */
void BtlPartner_SetObjFloats(FxChr *chr, f32 a, f32 b) {
    FxPartner *p = &chr->partner;

    if (p->active) {
        BtlObj_AddSway(BtlObj_Get(p->objId), a, b);
    }
}

/* Partner object flag 0x80 = on, while its action has attribute 0x200000 (only with flag 2 set). */
void BtlPartner_SetFlag80(FxChr *chr, s32 on) {
    FxPartner *p = &chr->partner;
    s32 set = 0;
    FxObj *obj;

    if (p->active) {
        obj = BtlObj_Get(p->objId);
        if (obj != NULL && obj->action != NULL) {
            if (obj->action->attr & 0x200000) {
                set = on != 0;
            }
        }
        if (set) {
            if ((*(u64 *)&obj->flags & 0x82) != 0x82) {
                obj->flags |= 0x80;
            }
        } else {
            if ((*(u64 *)&obj->flags & 0x82) == 0x82) {
                obj->flags &= ~0x80;
            }
        }
    }
}
