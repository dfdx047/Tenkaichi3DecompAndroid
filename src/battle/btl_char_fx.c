#include "common.h"
#include "battle/btl_char_member.h"

/*
 * The one-frame effect request bits of a fighter and everything that reads them: 0x1CF578..0x1D1EC8.
 *
 * One translation unit in the original, from the request-bit helpers (which the member code's header describes)
 * through the effect requests: BtlFx_UpdateGroundFx only compiles to the original bytes when BtlChar_IsFxBitNew is
 * defined earlier in the same file. Where the object really starts is not known (0x1CF578 is the latest place; the
 * read-only data is continuous from the member code). It ends at BtlFx_FireKiBlast only because that function
 * is left in assembly and owns the float constants that follow; btl_char_fx_c.c continues it.
 *
 * The first part (to 0x1D00D8) was written against the member code's view of the fighter (BtlMemberChr), the rest
 * against the effect code's (FxChr). Both views are kept; functions declared with the first view are reached from
 * the second half through cast macros.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);
extern s32 BtlUtil_Min(s32 a, s32 b);
extern s32 BtlUtil_Max(s32 a, s32 b);
extern f32 BtlUtil_WrapAngle(f32 a);
extern s32 BtlChar_GetCount(void);
extern BtlMemberChr *BtlChar_Get(s32 i);
extern BtlMemberObj *BtlChar_GetObj(BtlMemberChr *chr);
extern BtlMemberPose *BtlChar_GetPos(BtlMemberChr *chr);
extern s32 BtlChar_IsFrozen(BtlMemberChr *chr);
extern s32 BtlChar_FrameMod(s32 n);
extern void BtlChar_Vibrate(BtlMemberChr *chr, f32 power, f32 seconds);
extern s32 BtlChar_TestFlag(BtlMemberChr *chr, s32 bit);
extern void BtlChar_ClearFlag(BtlMemberChr *chr, s32 bit);
extern void BtlEvent_Raise(s32 side, s32 ev);
extern s32 Battle_GetMode(void);
extern f32 BtlCharApi_GetChargedUnkC78(s32 objId);
extern f32 Mathf_Sin(f32 angle);
extern f32 Mathf_Cos(f32 angle);
extern s32 BtlAnim_GetId(BtlMemberChr *chr);
extern s32 BtlAnim_GetFlags(s32 state);
extern f32 BtlAnim_GetFrame(BtlMemberChr *chr);
extern s32 BtlAnim_TestAttr(BtlMemberChr *chr, u64 mask);
extern f32 BtlStat_GetScale7(BtlMemberChr *chr);
extern s32 BtlOpp_GetPlayer(BtlMemberChr *chr);
extern s32 BtlOpp_HasAbility(BtlMemberChr *chr, s32 n);
extern f32 BtlParam_GetDamageTakenScale(BtlMemberChr *chr);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);

extern f32 BtlSkill_GetShotUnk30(BtlMemberChr *chr, s32 kind);
extern f32 BtlSkill_GetShotTime(BtlMemberChr *chr, s32 kind);
extern f32 BtlSkill_GetShotSpeed(BtlMemberChr *chr, s32 kind);
extern f32 BtlSkill_GetShotTurnRate(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotUnk6C(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotTime(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotSpeed(BtlMemberChr *chr, s32 kind);
extern f32 BtlSuper_GetShotTurnRate(BtlMemberChr *chr, s32 kind);
extern void EftShot_Request(BtlMemberAuraReq *req);
extern void EftTransform_Start(BtlMemberFx0Req *req);
extern void EftTransform_Flash(void);
extern void EftTransform_End(void);
extern s32 BtlAtk_GetHitFxKind(BtlMemberChr *chr);
extern s32 BtlAtk_GetHitSoundLevel(BtlMemberChr *chr);
extern s32 BtlCharApi_HasWeaponOut(s32 objId);
extern s32 BtlAtk_GetFlags(BtlMemberChr *chr);
extern f32 BtlAtk_GetLaunchAngleA(BtlMemberChr *chr);
extern f32 BtlAtk_GetLaunchAngleB(BtlMemberChr *chr);
extern void BtlOpp_GetTargetPos(BtlMemberChr *chr, Vec4 *out);
extern f32 BtlOpp_GetRadius(BtlMemberChr *chr);
extern void EftImpact_SpawnHit(BtlMemberHitFxReq *req);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern u32 BtlObjAnim_QueryEvent(BtlMemberObj *obj, u64 a, s32 b, s32 c);
extern s32 BtlObjAnim_MaskToNode(u32 mask);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharSnd_PlayCommon(BtlMemberChr *chr, s32 line);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);

extern BtlMemberRoster *gBtlChars;

/* Sets a one-frame effect request bit. */
void BtlChar_SetFxBit(BtlMemberChr *chr, s32 n) {
    chr->fxBits[n >> 3] |= 1 << (n & 7);
}

/* Clears a one-frame effect request bit. */
void BtlChar_ClearFxBit(BtlMemberChr *chr, s32 n) {
    u8 mask = 1 << (n & 7);

    chr->fxBits[n >> 3] &= ~mask;
}

/* Tests an effect request bit of this frame. */
s32 BtlChar_TestFxBit(BtlMemberChr *chr, s32 n) {
    u8 mask = 1 << (n & 7);

    return (chr->fxBits[n >> 3] & mask) != 0;
}

/* Tests an effect request bit of the frame before. */
s32 BtlChar_TestPrevFxBit(BtlMemberChr *chr, s32 n) {
    u8 mask = 1 << (n & 7);

    return (chr->prevFxBits[n >> 3] & mask) != 0;
}

/* Whether the bit is set now and was not last frame. */
s32 BtlChar_IsFxBitNew(BtlMemberChr *chr, s32 n) {
    s32 r = 0;

    if (BtlChar_TestFxBit(chr, n)) {
        r = BtlChar_TestPrevFxBit(chr, n) == 0;
    }
    return r;
}

/* Whether the bit was set last frame and is not now. */
s32 BtlChar_IsFxBitEnded(BtlMemberChr *chr, s32 n) {
    s32 r = 0;

    if (!BtlChar_TestFxBit(chr, n)) {
        r = BtlChar_TestPrevFxBit(chr, n) != 0;
    }
    return r;
}

/* Returns the sound line for a hit of the given kind, or -1. */
s32 BtlChar_GetHitSoundLine(s32 kind) {
    switch (kind) {
    case 0:
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        return BtlChar_FrameMod(2);
    case 5:
    case 9:
    case 10:
    case 11:
    case 12:
        return BtlChar_FrameMod(3) + 6;
    case 6:
        return 0x28;
    case 7:
        return 0x29;
    case 8:
        return 0x2A;
    }
    return -1;
}

/* Spawns the effect requested by fx bits 0x3C..0x40 (the highest set bit picks the kind 0..4). */
void BtlChar_SpawnFxBits3C(BtlMemberChr *chr) {
    BtlMemberAuraReq req;
    s32 kind = -1;

    if (BtlChar_TestFxBit(chr, 0x3C)) {
        kind = 0;
    }
    if (BtlChar_TestFxBit(chr, 0x3D)) {
        kind = 1;
    }
    if (BtlChar_TestFxBit(chr, 0x3E)) {
        kind = 2;
    }
    if (BtlChar_TestFxBit(chr, 0x3F)) {
        kind = 3;
    }
    if (BtlChar_TestFxBit(chr, 0x40)) {
        kind = 4;
    }
    if (kind >= 0) {
        s32 unkA24;

        req.objId = chr->objId;
        unkA24 = BtlChar_GetObj(chr)->unkA24;
        req.kind = kind;
        req.unk8 = unkA24;
        switch (kind) {
        case 0:
        case 1:
            req.unkC = BtlSkill_GetShotUnk30(chr, kind);
            req.unk10 = BtlSkill_GetShotTime(chr, kind);
            req.unk14 = BtlSkill_GetShotSpeed(chr, kind);
            req.unk18 = BtlSkill_GetShotTurnRate(chr, kind);
            break;
        case 2:
        case 3:
        case 4:
            req.unkC = BtlSuper_GetShotUnk6C(chr, kind);
            req.unk10 = BtlSuper_GetShotTime(chr, kind);
            req.unk14 = BtlSuper_GetShotSpeed(chr, kind);
            req.unk18 = BtlSuper_GetShotTurnRate(chr, kind);
            break;
        }
        EftShot_Request(&req);
    }
}

/* Spawns the effects requested by fx bits 0..2, by the kind in chr->unk12DC. */
void BtlChar_SpawnFxBits0(BtlMemberChr *chr) {
    BtlMemberFx0Req req;

    if (BtlChar_TestFxBit(chr, 0)) {
        req.objId = chr->objId;
        req.kind = -1;
        switch (chr->unk12DC) {
        case 0:
            req.kind = -1;
            break;
        case 1:
            req.kind = 1;
            break;
        case 2:
            req.kind = 0;
            break;
        case 3:
            req.kind = 5;
            break;
        case 4:
            req.kind = 6;
            break;
        case 5:
            req.kind = 2;
            break;
        case 6:
            req.kind = 3;
            break;
        case 7:
            req.kind = 4;
            break;
        case 8:
            req.kind = 7;
            break;
        case 9:
            req.kind = 8;
            break;
        case 10:
            req.kind = 9;
            break;
        case 11:
            req.kind = 10;
            break;
        }
        if (req.kind >= 0) {
            EftTransform_Start(&req);
        }
    }
    if (BtlChar_TestFxBit(chr, 1) && chr->unk12DC != 0) {
        EftTransform_Flash();
    }
    if (BtlChar_TestFxBit(chr, 2) && chr->unk12DC != 0) {
        EftTransform_End();
    }
}

/* Spawns the hit effect and plays the hit sound requested by fx bits 0x1B..0x27. */
void BtlChar_SpawnHitFx(BtlMemberChr *chr) {
    BtlMemberHitFxReq req;
    s32 sound = -1;
    BtlMemberPose *pose = BtlChar_GetPos(chr);
    BtlMemberObj *obj = BtlChar_GetObj(chr);

    req.kind = -1;
    req.objId = chr->objId;
    req.unk28 = 0;
    if (BtlChar_TestFxBit(chr, 0x1B)) {
        s32 guarded = 0;
        s32 level;

        switch (BtlAtk_GetHitFxKind(chr)) {
        case 1:
            req.kind = 0;
            break;
        case 2:
            req.kind = 2;
            break;
        case 3:
            req.kind = 2;
            break;
        case 4:
            req.kind = 1;
            break;
        case 5:
            req.kind = 2;
            break;
        case 6:
            req.kind = 3;
            break;
        case 7:
            req.kind = 0xC;
            break;
        case 8:
            req.kind = 7;
            break;
        case 9:
            req.kind = 8;
            break;
        case 10:
            req.kind = 0xD;
            break;
        }
        level = BtlAtk_GetHitSoundLevel(chr);
        if (BtlCharApi_HasWeaponOut(chr->objId) && !(BtlAtk_GetFlags(chr) & 0x2000)) {
            guarded = 1;
        }
        if (guarded) {
            if (req.kind < 3) {
                req.kind = 9;
            } else {
                req.kind = 10;
            }
            sound = level < 5 ? 0x4D : 0x36;
        } else {
            switch (level) {
            case 1:
                sound = BtlChar_FrameMod(2) + 2;
                break;
            case 2:
                sound = 0x19;
                break;
            case 3:
                sound = 0x22;
                break;
            case 4:
                sound = 0x45;
                break;
            case 5:
                sound = BtlChar_FrameMod(2) + 9;
                break;
            case 6:
                sound = 0x25;
                break;
            case 7:
                sound = 0x26;
                break;
            case 8:
                sound = 0x27;
                break;
            case 9:
                sound = 0x49;
                break;
            case 10:
                sound = 0x4E;
                break;
            case 11:
                sound = 0x50;
                break;
            case 12:
                sound = 0x4F;
                break;
            case 13:
                sound = 0x4B;
                break;
            case 14:
                sound = 0xE;
                break;
            }
        }
    }
    if (BtlChar_TestFxBit(chr, 0x1C)) {
        req.kind = 6;
        if (BtlAtk_GetHitSoundLevel(chr) < 5) {
            sound = BtlChar_FrameMod(2) + 4;
        } else {
            sound = BtlChar_FrameMod(2) + 0xC;
        }
    }
    if (BtlChar_TestFxBit(chr, 0x21)) {
        sound = 0x24;
        req.kind = 6;
    }
    if (BtlChar_TestFxBit(chr, 0x24)) {
        sound = 0x42;
        req.kind = 6;
    }
    if (BtlChar_TestFxBit(chr, 0x22)) {
        if (BtlAtk_GetHitFxKind(chr) < 4) {
            req.kind = 4;
            sound = BtlChar_FrameMod(2) + 0xC;
        } else {
            sound = 0x24;
            req.kind = 5;
        }
    }
    if (req.kind >= 0) {
        f32 yaw = BtlUtil_WrapAngle(pose->yaw + BtlAtk_GetLaunchAngleA(chr));
        f32 pitch = BtlAtk_GetLaunchAngleB(chr);
        f32 sinYaw = Mathf_Sin(yaw);
        f32 cosYaw = Mathf_Cos(yaw);
        f32 sinPitch = Mathf_Sin(pitch);
        f32 cosPitch = Mathf_Cos(pitch);

        req.dir.x = cosPitch * sinYaw;
        req.dir.y = -sinPitch;
        req.dir.z = cosPitch * cosYaw;
        req.dir.w = 0.0f;
        if (BtlChar_TestFxBit(chr, 0x25)) {
            Vec4 target;

            BtlOpp_GetTargetPos(chr, &target);
            req.pos.x = target.x - Mathf_Sin(pose->yaw) * BtlOpp_GetRadius(chr);
            req.pos.y = BtlChar_GetPos(chr)->pos.y;
            req.pos.z = target.z - Mathf_Cos(pose->yaw) * BtlOpp_GetRadius(chr);
            req.pos.w = 1.0f;
            EftImpact_SpawnHit(&req);
        } else if (BtlChar_TestFxBit(chr, 0x26)) {
            Vec4 target;

            BtlOpp_GetTargetPos(chr, &target);
            req.pos.x = target.x - Mathf_Sin(pose->yaw) * BtlOpp_GetRadius(chr);
            req.pos.y = BtlChar_GetPos(chr)->pos.y;
            req.pos.z = target.z - Mathf_Cos(pose->yaw) * BtlOpp_GetRadius(chr);
            req.pos.w = 1.0f;
            req.pos.y -= BtlCharApi_GetHeight(chr->objId) * 0.5f;
            EftImpact_SpawnHit(&req);
        } else if (BtlChar_TestFxBit(chr, 0x27)) {
            Vec4 target;

            BtlOpp_GetTargetPos(chr, &target);
            req.pos.x = target.x - Mathf_Sin(pose->yaw) * BtlOpp_GetRadius(chr);
            req.pos.y = BtlChar_GetPos(chr)->pos.y;
            req.pos.z = target.z - Mathf_Cos(pose->yaw) * BtlOpp_GetRadius(chr);
            req.pos.w = 1.0f;
            req.pos.y -= BtlCharApi_GetHeight(chr->objId);
            EftImpact_SpawnHit(&req);
        } else {
            s32 masks[9] = { 0x40000, 0x800, 0x100, 0x400, 0x80, 0x20, 8, 0x10, 4 };
            u32 bits = obj->unkC9C;
            u32 hit;
            s32 i;
            Vec4 fwd;
            Vec4 point;
            Vec4 tmp;

            if (bits == 0) {
                bits = BtlObjAnim_QueryEvent(obj, 1, 0, 6);
            }
            for (i = 0; i < 9; i++) {
                hit = bits & masks[i];
                if (hit != 0) {
                    break;
                }
            }
            if (hit == 0) {
                for (i = 0; i < 0x13; i++) {
                    hit = bits & (1 << i);
                    if (hit != 0) {
                        break;
                    }
                }
            }
            if (hit != 0) {
                f32 d;

                BtlCharApi_GetNodePos(chr->objId, BtlObjAnim_MaskToNode(hit), &req.pos);
                fwd.x = Mathf_Sin(pose->unk14);
                fwd.y = 0.0f;
                fwd.z = Mathf_Cos(pose->unk14);
                Vec4_Scale(&tmp, &fwd, 5.0f);
                Vec4_Add(&point, &pose->pos, &tmp);
                fwd.w = -Vec3_Dot(&fwd, &point);
                d = Vec3_Dot(&fwd, &req.pos) + fwd.w;
                if (d < 0.0f) {
                    Vec4_Scale(&tmp, &fwd, -d);
                    Vec4_Add(&req.pos, &req.pos, &tmp);
                }
                req.pos.w = 1.0f;
                EftImpact_SpawnHit(&req);
            }
        }
    }
    if (sound >= 0) {
        BtlCharSnd_PlayCommon(chr, sound);
    }
}

/* ------------------------------------------------------------------------------------------------------------
 * Effect requests (0x1D00D8..), written against the effect code's view of the fighter.
 * ------------------------------------------------------------------------------------------------------------ */
#include "battle/btl_char_fx.h"

/*
 * Fighter effect requests, part 1: 0x1D00D8..0x1D0B60.
 *
 * Each function here looks at the one-frame request bits of a fighter (fighter + 0x1262, set by the action code
 * through BtlChar_SetFxBit; last frame's copy at + 0x126B) and starts, keeps or stops one effect of the effect scene.
 * They are called in a fixed order from BtlFx_UpdateAll (btl_char_fx_c.c). Nothing here draws a random number.
 *
 * This file and btl_char_fx_c.c are one run of code, cut after BtlFx_FireKiBlast (0x1D1958). Every function is C;
 * the file's float constants are 0x2FD1D8..0x2FD214 (the last five, 0x2FD200.., are BtlFx_FireKiBlast's).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 atan2f(f32 y, f32 x);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Length(Vec4 *v);
extern f32 BtlUtil_WrapAngle(f32 a);
#define BtlChar_GetObj ((FxObj *(*)(FxChr *chr))BtlChar_GetObj)
#define BtlChar_GetPos ((FxPose *(*)(FxChr *chr))BtlChar_GetPos)
extern s32 BtlChar_TestMemberUnk70(FxChr *chr);
#define BtlMember_GetActiveGauge ((FxGauge *(*)(FxChr *chr))BtlMember_GetActiveGauge)
#define BtlMember_HasAnyListedAbility ((s32 (*)(FxChr *chr))BtlMember_HasAnyListedAbility)
#define BtlChar_TestFxBit ((s32 (*)(FxChr *chr, s32 bit))BtlChar_TestFxBit)           /* test */
#define BtlChar_IsFxBitNew ((s32 (*)(FxChr *chr, s32 bit))BtlChar_IsFxBitNew)           /* set now, clear last frame */
#define BtlChar_IsFxBitEnded ((s32 (*)(FxChr *chr, s32 bit))BtlChar_IsFxBitEnded)           /* clear now, set last frame */
extern void BtlCharApi_GetNodePos(s32 objId, s32 part, Vec4 *out); /* world position of a model part */
#define BtlCharSnd_PlayCommon ((void (*)(FxChr *chr, s32 sound))BtlCharSnd_PlayCommon)
extern void BtlOpp_GetDelta(FxChr *chr, Vec4 *out);
extern s32 BtlParam_GetFlags(FxChr *chr);
extern void BtlParam_GetUnk0(FxChr *chr);
extern s32 BtlKiBlast_GetType(FxChr *chr);
extern s32 BtlKiBlast_GetUnk3(FxChr *chr);
extern void BtlObj_SetColorMode(FxObj *obj, s32 bit, s32 on);
extern s32 BtlObj_GetNodeSide(s32 kind);
extern void EftSpdLine_SpawnBodyTrails(s32 objId);
extern void EftAura_SetLevel(s32 objId, f32 level);
extern void EftAura_Command(s32 objId, s32 cmd);
extern void EftDisc_SpawnHeld(FxHitArg *arg, s32 kind);
extern void EftGlow_Request(s32 player, s32 cmd);
extern s32 EftCharge_IsActive(s32 objId);
extern void EftCharge_Start(s32 *arg);
extern void EftCharge_Burst(s32 objId);
extern void EftCharge_Stop(s32 objId);
extern void EftBlastCharge_Start(FxHitArg *arg);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftRay_CreateByValue(FxLineArg arg);
#define EftRay_CreateByValue(arg) EftRay_CreateByValue(*(FxLineArg *)(arg))
#else
extern void EftRay_CreateByValue(FxLineArg *arg);
#endif
extern void EftShotFx_Start(s32 objId, s32 kind);
#define EftImpact_SpawnHit ((void (*)(FxPosArg *arg))EftImpact_SpawnHit)
extern void EftShock_Start(s32 *arg);
extern void StgBlur_SetPasses(s32 view, s32 mode);
extern void StgBlur_SetCenter(s32 view, Vec4 *pos, s32 arg);
extern void StgBlur_SetColor0Rgba(s32 view, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor1Rgba(s32 view, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor2Rgba(s32 view, s32 r, s32 g, s32 b, s32 a);
extern void StgBlur_SetColor3Rgba(s32 view, s32 r, s32 g, s32 b, s32 a);

/* Request 0x20: effect 0xE between model part 0x2E and the fighter, with sound 0x46. */
void BtlFx_SpawnFlashReq20(FxChr *chr) {
    FxPosArg arg;

    if (BtlChar_TestFxBit(chr, 0x20)) {
        arg.kind = 0xE;
        arg.objId = chr->objId;
        arg.unk28 = 0;
        BtlCharApi_GetNodePos(chr->objId, 0x2E, &arg.pos);
        Vec4_Copy(&arg.pos2, &BtlChar_GetPos(chr)->unk80);
        EftImpact_SpawnHit(&arg);
        BtlCharSnd_PlayCommon(chr, 0x46);
    }
}

/* Request 0x2B: effect 0xD at model part 0x10. */
void BtlFx_SpawnFlashReq2B(FxChr *chr) {
    FxPosArg arg;

    if (BtlChar_TestFxBit(chr, 0x2B)) {
        arg.kind = 0xD;
        arg.objId = chr->objId;
        arg.unk28 = 0;
        Vec4_Copy(&arg.pos2, &BtlChar_GetPos(chr)->unk80);
        BtlCharApi_GetNodePos(chr->objId, 0x10, &arg.pos);
        EftImpact_SpawnHit(&arg);
    }
}

/* Request 0x17: the hit spark at the recorded hit position, by the attack's class. */
void BtlFx_SpawnHitSparkReq17(FxChr *chr) {
    FxHitArg arg;
    s32 type;

    if (BtlChar_TestFxBit(chr, 0x17)) {
        type = BtlKiBlast_GetType(chr);
        Vec4_Copy(&arg.pos, &chr->hitPos);
        arg.objId = chr->objId;
        arg.kind = chr->hitKind;
        arg.unk18 = BtlKiBlast_GetUnk3(chr);
        arg.scale = 1.0f;
        if (BtlChar_TestMemberUnk70(chr)) {
            type = 1;
        }
        if (arg.kind == 0x15) {
            arg.kind = 0x1F;
        }
        if (arg.kind == 0x23) {
            arg.kind = 0x2D;
        }
        if (type == 5) {
        } else if (type == 4) {
            EftDisc_SpawnHeld(&arg, BtlObj_GetNodeSide(arg.kind));
        } else if (type == 2) {
        } else if (type == 3) {
        } else {
            EftBlastCharge_Start(&arg);
        }
    }
}

/*
 * The aura: level = ki ratio when at least half full (1 with object flag 0x40000 or request 0x13, 0 with request 0x14
 * or when the action has attribute 0x10 without one of the listed abilities); requests 7 / 8 send its commands 2..4.
 * Not void in the original: the last call is not a tail call.
 */
s32 BtlFx_UpdateAura(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);
    FxGauge *g = BtlMember_GetActiveGauge(chr);
    f32 level;

    if (g->kiMax > 0) {
        level = (f32)g->ki / (f32)g->kiMax;
        if (level < 0.5f) {
            level = 0.0f;
        }
    } else {
        level = 0.0f;
    }
    if (BtlParam_GetFlags(chr) & 0x10) {
        if (!BtlMember_HasAnyListedAbility(chr)) {
            level = 0.0f;
        }
    }
    if (obj->flags & 0x40000) {
        level = 1.0f;
    }
    if (BtlChar_TestFxBit(chr, 0x13)) {
        level = 1.0f;
    }
    if (BtlChar_TestFxBit(chr, 0x14)) {
        level = 0.0f;
        EftAura_Command(chr->objId, 7);
    }
    if (level > 0.0f) {
        EftAura_Command(chr->objId, 0);
        EftAura_SetLevel(chr->objId, level);
    } else {
        EftAura_Command(chr->objId, 1);
    }
    if (!(BtlParam_GetFlags(chr) & 0x10)) {
        if (BtlChar_IsFxBitNew(chr, 7)) {
            EftAura_Command(chr->objId, 2);
            BtlParam_GetUnk0(chr);
        }
        if (BtlChar_IsFxBitEnded(chr, 7)) {
            EftAura_Command(chr->objId, 3);
            BtlParam_GetUnk0(chr);
        }
        if (BtlChar_TestFxBit(chr, 8)) {
            EftAura_Command(chr->objId, 4);
            BtlParam_GetUnk0(chr);
        }
    }
}

/* Requests 7 and 8: keeps effect 0x1762A8 alive while 7 is set; 8 triggers its burst. */
void BtlFx_UpdateChargeFx(FxChr *chr) {
    s32 arg[4];

    if (BtlChar_TestFxBit(chr, 7)) {
        if (!EftCharge_IsActive(chr->objId)) {
            arg[0] = chr->objId;
            EftCharge_Start(arg);
        }
    } else if (EftCharge_IsActive(chr->objId)) {
        EftCharge_Stop(chr->objId);
    }
    if (BtlChar_TestFxBit(chr, 8)) {
        EftCharge_Burst(chr->objId);
    }
}

/* Requests 4 and 5: the powered-up look (object mask bit 6) and its aura variant. Not void in the original (no tail call). */
s32 BtlFx_UpdatePowerUpLook(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (!(BtlParam_GetFlags(chr) & 0x10) || BtlMember_HasAnyListedAbility(chr)) {
        if (BtlChar_TestFxBit(chr, 5)) {
            EftGlow_Request(chr->player, 2);
            EftAura_Command(chr->player, 5);
            BtlObj_SetColorMode(obj, 6, 0);
        } else {
            if (BtlChar_IsFxBitNew(chr, 4)) {
                EftGlow_Request(chr->player, 0);
                EftAura_Command(chr->player, 6);
                BtlObj_SetColorMode(obj, 6, 1);
            }
            if (BtlChar_IsFxBitEnded(chr, 4)) {
                EftGlow_Request(chr->player, 1);
                EftAura_Command(chr->player, 5);
                BtlObj_SetColorMode(obj, 6, 0);
            }
        }
    }
}

/* Request 6: effect 0x1A6FD0. */
void BtlFx_SpawnReq6(FxChr *chr) {
    s32 arg[4];

    if (BtlChar_TestFxBit(chr, 6)) {
        arg[0] = chr->objId;
        EftShock_Start(arg);
    }
}

/*
 * Requests 9 and 10: fades the view's screen filter in (1/6 per frame) and out (1/3 per frame).
 * The compiler truncates decimal literals, so the two steps are written one digit up to give the original bits
 * (0x3E2AAAAB and 0x3EAAAAAB, the values rounded to nearest).
 */
void BtlFx_UpdateScreenFilter(FxChr *chr) {
    u8 a, b, c;

    if (BtlChar_TestFxBit(chr, 9)) {
        chr->filter += 0.16666668f;
        if (chr->filter > 1.0f) {
            chr->filter = 1.0f;
        }
    } else if (BtlChar_TestFxBit(chr, 0xA)) {
        chr->filter = 1.0f;
    } else {
        chr->filter -= 0.33333335f;
        if (chr->filter < 0.0f) {
            chr->filter = 0.0f;
        }
    }
    a = chr->filter * 8.0f;
    b = chr->filter * 32.0f;
    c = chr->filter * 128.0f;
    StgBlur_SetPasses(chr->side, 3);
    StgBlur_SetColor3Rgba(chr->side, 0x80, 0x80, 0x80, 0x40);
    StgBlur_SetColor0Rgba(chr->side, 0x80, 0x80, 0x80, a);
    StgBlur_SetColor1Rgba(chr->side, 0x80, 0x80, 0x80, b);
    StgBlur_SetColor2Rgba(chr->side, 0x80, 0x80, 0x80, c);
    if (BtlChar_TestFxBit(chr, 9)) {
        StgBlur_SetCenter(chr->side, &BtlChar_GetPos(chr)->unk80, 0);
    }
}

/* Requests 0xC..0xF: one-shot effects 0..3 of module 0x180A00. */
void BtlFx_SpawnBurstReqC(FxChr *chr) {
    if (BtlChar_TestFxBit(chr, 0xC)) {
        EftShotFx_Start(chr->objId, 0);
    }
    if (BtlChar_TestFxBit(chr, 0xD)) {
        EftShotFx_Start(chr->objId, 1);
    }
    if (BtlChar_TestFxBit(chr, 0xE)) {
        EftShotFx_Start(chr->objId, 2);
    }
    if (BtlChar_TestFxBit(chr, 0xF)) {
        EftShotFx_Start(chr->objId, 3);
    }
}

/* Request 0xB: effect 0x15EF18 while moving faster than half the reference speed. */
void BtlFx_SpawnFastMoveFx(FxChr *chr) {
    FxPose *pose;

    if (BtlChar_TestFxBit(chr, 0xB)) {
        pose = BtlChar_GetPos(chr);
        if (pose->unk98 > 0.01f) {
            if (Vec3_Length(&pose->vel) / pose->unk98 > 0.5f) {
                EftSpdLine_SpawnBodyTrails(chr->objId);
            }
        }
    }
}

/* Request 0x15: speed lines; two parameters follow the direction to the opponent relative to the camera yaw and the distance. */
/*
 * Matching notes: the defaults are an aggregate initialiser (the compiler builds it in a stack temporary, clearing
 * the vector with memset, and copies it; the temporary's slot is then reused for the direction vector). The
 * colour has to be a scalar followed by an array of three (see FxLineArg) and the 0.001 a variable assigned
 * before `angle`, or the stores and the constant loads are scheduled differently.
 */
void BtlFx_SpawnSpeedLines(FxChr *chr) {
    f32 angle;
    f32 eps;

    if (BtlChar_TestFxBit(chr, 0x15)) {
        FxLineArg arg = { { 0.0f, 0.0f, 0.0f, 1.0f }, 0x80, { 0x80, 0x80, 0x40 }, 1.0f, 800.0f, 1.5f, 0.0f, 50.0f,
                          2,                          0x30, chr->objId,           1,    1,      0,    0,    1 };
        Vec4 d;

        BtlOpp_GetDelta(chr, &d);
        eps = 0.001f;
        angle = 0.0f;
        if (__builtin_fabsf(d.x) > eps || __builtin_fabsf(d.z) > eps) {
            angle = BtlUtil_WrapAngle(atan2f(d.x, d.z) - chr->camYaw);
        }
        arg.unk20 = __builtin_fabsf(angle) / 3.14159265f * 0.5f + 0.3f;
        arg.unk2C = -20.0f - Vec3_Length(&d) * 0.1f;
        if (arg.unk2C < -50.0f) {
            arg.unk2C = -50.0f;
        }
        EftRay_CreateByValue(&arg);
    }
}

/*
 * Fighter effect requests, part 2: 0x1D0B60..0x1D1EC8 (formerly btl_char_fx_b.c).
 * More request-bit readers (ground, water, clash), the object flags that follow fighter state, and the hit sparks
 * spawned by animation event 4. BtlFx_FireKiBlast is the only function of the effect files that draws random
 * numbers (BtlChar_RandF, the fighters' own generator).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
extern f32 BtlUtil_WrapAngle(f32 a);
#define BtlChar_GetObj ((FxObj *(*)(FxChr *chr))BtlChar_GetObj)
#define BtlChar_GetPos ((FxPose *(*)(FxChr *chr))BtlChar_GetPos)
#define BtlChar_TestFlag ((s32 (*)(FxChr *chr, s32 flag))BtlChar_TestFlag)
extern s32 BtlChar_TestMemberUnk70(FxChr *chr);
#define BtlChar_SetFxBit ((void (*)(FxChr *chr, s32 bit))BtlChar_SetFxBit)          /* set */
#define BtlChar_ClearFxBit ((void (*)(FxChr *chr, s32 bit))BtlChar_ClearFxBit)          /* clear */
#define BtlChar_TestFxBit ((s32 (*)(FxChr *chr, s32 bit))BtlChar_TestFxBit)           /* test */
#define BtlChar_IsFxBitNew ((s32 (*)(FxChr *chr, s32 bit))BtlChar_IsFxBitNew)           /* set now, clear last frame */
#define BtlChar_IsFxBitEnded ((s32 (*)(FxChr *chr, s32 bit))BtlChar_IsFxBitEnded)           /* clear now, set last frame */
extern void BtlCharApi_GetNodePos(s32 objId, s32 part, Vec4 *out); /* world position of a model part */
#define BtlCharSnd_PlayCommon ((void (*)(FxChr *chr, s32 sound))BtlCharSnd_PlayCommon)
extern s32 BtlChar_TestPrevFlag(FxChr *chr, s32 flag);            /* fighter flag, last frame */
extern s32 BtlParam_GetFlags(FxChr *chr);
extern f32 BtlAtk_GetLaunchAngleAOf(FxChr *chr, s32 param);
extern f32 BtlAtk_GetLaunchAngleBOf(FxChr *chr, s32 param);
extern s32 BtlKiBlast_GetType(FxChr *chr);
extern s32 BtlKiBlast_GetUnk3(FxChr *chr);
extern s32 BtlStage_GetWaterLevel(f32 *height);
extern s32 BtlObj_GetNodeSide(s32 kind);
extern void EftBubble_StartBurst(s32 objId);
extern void EftWater_SetWake(s32 objId, s32 off);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftWater_AddSplashFor(s32 objId, Vec4 pos, s32 arg, f32 speed);
#define EftWater_AddSplashFor(objId, pos, arg, speed) EftWater_AddSplashFor(objId, *(Vec4 *)(pos), arg, speed)
#else
extern void EftWater_AddSplashFor(s32 objId, Vec4 *pos, s32 arg, f32 speed);
#endif
extern void EftAbsorb_Start(FxArg2 *arg);
extern void EftAbsorb_Stop(s32 objId);
extern void EftAbsorb_StartHands(FxArg2 *arg);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftBodyFx_Start(FxArg3 arg);
#define EftBodyFx_Start(arg) EftBodyFx_Start(*(FxArg3 *)(arg))
#else
extern void EftBodyFx_Start(FxArg3 *arg);
#endif
extern void EftBodyFx_Stop(s32 objId);
extern void EftRushBurst_Start(FxDirArg *arg);
extern void EftRushBurst_Stage2(s32 objId);
extern void EftRushBurst_Stop(s32 objId);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftRay_StartHit(s32 objId, PortVec4 pos, s32 a, s32 b, s32 c, f32 scale);
#define EftRay_StartHit(objId, pos, a, b, c, scale) EftRay_StartHit(objId, *(PortVec4 *)(pos), a, b, c, scale)
#else
extern void EftRay_StartHit(s32 objId, Vec4 *pos, s32 a, s32 b, s32 c, f32 scale);
#endif
#define EftImpact_SpawnHit ((void (*)(FxPosArg *arg))EftImpact_SpawnHit)
extern void EftGndDust_SetSlide(s32 objId, s32 off, f32 scale);
extern void EftGndDust_SetDash(s32 objId, s32 off, f32 scale);
extern void EftGndDust_SpawnBurst(s32 objId, f32 a, f32 b);
extern void EftGndDust_SpawnLanding(s32 objId, Vec4 *pos, Vec4 *normal, f32 scale);
extern void EftGndDust_Stub(s32 objId, f32 scale);
extern void EftGndDust_SpawnImpact(s32 objId, Vec4 *pos, f32 scale);
extern void EftCharaFx_Start(FxArg2 *arg);
extern void EftCharaFx_Stop(s32 objId);
extern void EftImpact_SpawnHitScaled(FxPosArg *arg, f32 size);
extern void Vec4_Lerp(Vec4 *out, Vec4 *a, Vec4 *b, f32 t);
extern s32 BtlOpp_GetObjId(FxChr *chr);
extern s32 BtlCharApi_IsModelNew(s32 objId);
#define BtlMember_GetActive ((s32 *(*)(FxChr *chr))BtlMember_GetActive)
#define BtlMember_HasAbility ((s32 (*)(FxChr *chr, s32 param))BtlMember_HasAbility)
extern void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 strength, f32 time);
extern void BtlObjFlash_Start(FxObj *obj, s32 kind);
extern void BtlObj_SetEyeFrame(FxObj *obj, s32 arg);
extern s32 BtlObj_GetMouthMode(FxObj *obj);
extern s32 BtlObj_GetEyeFrame(FxObj *obj);
extern void BtlObj_SetSubState(FxObj *obj, s32 state, s32 arg);
#define BtlAnim_TestAttr ((s32 (*)(FxChr *chr, u64 mask))BtlAnim_TestAttr)           /* animation event bits raised this frame */
extern f32 BtlOpp_GetDistance(FxChr *chr);
extern void BtlChar_ToggleHeldFlag(FxChr *chr, s32 flag);
extern f32 BtlAct_GetHeight(FxChr *chr);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlKiBlast_GetCurrentKind(FxChr *chr);
extern s32 BtlKiBlast_GetFlags(FxChr *chr);
extern f32 BtlKiBlast_GetSpeed(FxChr *chr);
extern f32 BtlKiBlast_GetTurnRate(FxChr *chr);
extern s32 BtlKiBlast_GetFrames(FxChr *chr);
extern s32 BtlKiBlast_GetUnk31(FxChr *chr);
extern s32 BtlKiBlast_GetHits(FxChr *chr);
extern f32 BtlKiBlast_GetUnk28(FxChr *chr);
extern f32 BtlKiBlast_GetUnk2C(FxChr *chr);
extern s32 BtlObjAnim_GetEventArg(FxObj *obj, u64 mask);
#define BtlObjAnim_MaskToNode ((s32 (*)(s32 bits))BtlObjAnim_MaskToNode)
#define BtlObjAnim_QueryEvent ((s32 (*)(FxObj *obj, u64 a, s32 b, s32 c))BtlObjAnim_QueryEvent)
extern void Vec3_Normalize(Vec4 *out, Vec4 *in);
extern void Vec3_RotateAxis(Vec4 *out, Vec4 *a, Vec4 *b, f32 s);
extern void Vec3_RotateY(Vec4 *out, Vec4 *in, f32 s);
extern void EftDisc_Throw(FxHitArg2 *arg);
extern void EftDisc_SpawnFromNode(FxHitArg2 *arg);
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftKiBlast_Fire(FxHitArg2 arg);
#define EftKiBlast_Fire(arg) EftKiBlast_Fire(*(FxHitArg2 *)(arg))
#else
extern void EftKiBlast_Fire(FxHitArg2 *arg);
#endif
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftKiBomb_Fire(FxHitArg2 arg);
#define EftKiBomb_Fire(arg) EftKiBomb_Fire(*(FxHitArg2 *)(arg))
#else
extern void EftKiBomb_Fire(FxHitArg2 *arg);
#endif
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftKiObj_Create(FxHitArg2 arg);
#define EftKiObj_Create(arg) EftKiObj_Create(*(FxHitArg2 *)(arg))
#else
extern void EftKiObj_Create(FxHitArg2 *arg);
#endif
extern f32 BtlChar_RandF(void);
extern s32 BtlChar_FrameMod(s32 n);
extern void BtlChar_SetSmallVibration(FxChr *chr, f32 seconds);

/* Request 0x18: effect 0x17D500 at model part 0x11. */
void BtlFx_SpawnReq18(FxChr *chr) {
    Vec4 pos;

    if (BtlChar_TestFxBit(chr, 0x18)) {
        BtlCharApi_GetNodePos(chr->objId, 0x11, &pos);
        EftRay_StartHit(chr->objId, &pos, 0, 0, 1, 0.5f);
    }
}

/* Requests 0x2F..0x36: ground effects (dust, or splashes when the ground point is under water). */
/*
 * Matches only with BtlChar_IsFxBitNew (0x1CF620) defined earlier in the same translation unit: then the first
 * branch takes "li a1,0x2F" into its delay slot as the original does. Compiled on its own the branch becomes a
 * branch-likely with "move a0,s1". This is the evidence that the request-bit helpers and the effect requests were
 * one source file.
 */
/*
 * The effect calls that take a float go through inline wrappers: without them the compiler keeps a repeated
 * constant in a saved register across the calls, which the original does not.
 */
static inline void Fx_FootDust(s32 objId, Vec4 *pos, f32 scale) {
    EftGndDust_SpawnImpact(objId, pos, scale);
}

void BtlFx_UpdateGroundFx(FxChr *chr) {
    Vec4 a;
    Vec4 b;
    f32 water;
    s32 under;

    if (BtlChar_GetPos(chr)->unkD0 & 0x30000065) {
        BtlChar_ClearFxBit(chr, 0x2F);
        BtlChar_ClearFxBit(chr, 0x30);
        BtlChar_ClearFxBit(chr, 0x31);
        BtlChar_ClearFxBit(chr, 0x32);
        BtlChar_ClearFxBit(chr, 0x33);
        BtlChar_ClearFxBit(chr, 0x34);
        BtlChar_ClearFxBit(chr, 0x35);
        BtlChar_ClearFxBit(chr, 0x36);
    }
    if (BtlChar_IsFxBitNew(chr, 0x30)) {
        EftGndDust_SetSlide(chr->objId, 0, 1.0f);
    }
    if (BtlChar_IsFxBitEnded(chr, 0x30)) {
        EftGndDust_SetSlide(chr->objId, 1, 1.0f);
    }
    if (BtlChar_IsFxBitNew(chr, 0x33)) {
        EftGndDust_SetDash(chr->objId, 0, 1.0f);
    }
    if (BtlChar_IsFxBitEnded(chr, 0x33)) {
        EftGndDust_SetDash(chr->objId, 1, 1.0f);
    }
    if (BtlChar_TestFxBit(chr, 0x34)) {
        EftGndDust_SpawnBurst(chr->objId, 1.0f, 1.0f);
    }
    if (BtlChar_TestFxBit(chr, 0x35)) {
        water = 0.0f;
        under = 0;
        if (BtlStage_GetWaterLevel(&water)) {
            if (BtlChar_GetPos(chr)->unkB0.y > water) {
                under = 1;
            }
        }
        if (under) {
            EftGndDust_SpawnBurst(chr->objId, 1.0f, 1.0f);
        } else {
            EftGndDust_SpawnLanding(chr->objId, &BtlChar_GetPos(chr)->unkB0, &BtlChar_GetPos(chr)->unkC0, 1.0f);
        }
    }
    if (BtlChar_TestFxBit(chr, 0x31)) {
        EftGndDust_Stub(chr->objId, 1.0f);
    }
    if (BtlChar_TestFxBit(chr, 0x32)) {
        BtlCharApi_GetNodePos(chr->objId, 0xF, &a);
        a.y = BtlChar_GetPos(chr)->unkB0.y;
        BtlCharApi_GetNodePos(chr->objId, 0xB, &b);
        b.y = BtlChar_GetPos(chr)->unkB0.y;
        Fx_FootDust(chr->objId, &a, 1.0f);
        Fx_FootDust(chr->objId, &b, 1.0f);
    }
}

/* Water: the splash when flag 0x11 changes, and the wake (request 0x19) while moving fast with flag 0x12. */
void BtlFx_UpdateWaterFx(FxChr *chr) {
    Vec4 pos;
    f32 water;
    FxPose *pose = BtlChar_GetPos(chr);

    if (BtlChar_TestFlag(chr, 0x11) != BtlChar_TestPrevFlag(chr, 0x11)) {
        if (!BtlChar_TestFlag(chr, 0x24)) {
            if (BtlStage_GetWaterLevel(&water)) {
                pos.x = pose->pos.x;
                pos.y = water;
                pos.z = pose->pos.z;
                pos.w = 1.0f;
                EftWater_AddSplashFor(chr->objId, &pos, 0, BtlChar_GetPos(chr)->vel.y);
                if (BtlChar_TestFlag(chr, 0x11)) {
                    BtlCharSnd_PlayCommon(chr, 0x3A);
                } else {
                    BtlCharSnd_PlayCommon(chr, 0x3B);
                }
                if (BtlChar_TestFlag(chr, 0x11)) {
                    EftBubble_StartBurst(chr->objId);
                }
            }
        }
    }
    if (BtlChar_TestFlag(chr, 0x12)) {
        if (Vec3_Length(&pose->vel) > 6.4814806f) {
            BtlChar_SetFxBit(chr, 0x19);
        }
    }
    if (BtlChar_IsFxBitNew(chr, 0x19)) {
        EftWater_SetWake(chr->objId, 0);
    }
    if (BtlChar_IsFxBitEnded(chr, 0x19)) {
        EftWater_SetWake(chr->objId, 1);
    }
}

/* Requests 0x28..0x2A: effect 0x171C78 along a direction from two action parameters, and its two follow-ups. */
void BtlFx_UpdateAimedFxReq28(FxChr *chr) {
    FxDirArg arg;
    f32 pitch;
    f32 yaw;

    if (BtlChar_IsFxBitNew(chr, 0x28)) {
        pitch = BtlAtk_GetLaunchAngleBOf(chr, 0x86);
        yaw = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw + BtlAtk_GetLaunchAngleAOf(chr, 0x86));
        memset(&arg, 0, sizeof(arg));
        arg.objId = chr->objId;
        arg.unk14 = chr->unk1354;
        arg.scale = 1.0f;
        arg.dir.x = Mathf_Cos(pitch) * Mathf_Sin(yaw);
        arg.dir.y = -Mathf_Sin(pitch);
        arg.dir.z = Mathf_Cos(pitch) * Mathf_Cos(yaw);
        EftRushBurst_Start(&arg);
    }
    if (BtlChar_IsFxBitNew(chr, 0x29)) {
        EftRushBurst_Stage2(chr->objId);
    }
    if (BtlChar_IsFxBitNew(chr, 0x2A)) {
        EftRushBurst_Stop(chr->objId);
    }
}

/* Request 0x1A: effect 0x16B418 while set. */
void BtlFx_UpdateReq1A(FxChr *chr) {
    FxArg3 arg;

    if (BtlChar_IsFxBitNew(chr, 0x1A)) {
        arg.objId = chr->objId;
        arg.scale = 1.0f;
        arg.unk4 = 0;
        EftBodyFx_Start(&arg);
    }
    if (BtlChar_IsFxBitEnded(chr, 0x1A)) {
        EftBodyFx_Stop(chr->objId);
    }
}

/* Request 0x38: effect 0x15EDF0 while set. */
void BtlFx_UpdateReq38(FxChr *chr) {
    FxArg2 arg;

    if (BtlChar_IsFxBitNew(chr, 0x38)) {
        arg.objId = chr->objId;
        arg.scale = 1.0f;
        EftAbsorb_Start(&arg);
    }
    if (BtlChar_IsFxBitEnded(chr, 0x38)) {
        EftAbsorb_Stop(chr->objId);
    }
}

/* Request 0x37: effect 0x15EEC0. */
void BtlFx_SpawnReq37(FxChr *chr) {
    FxArg2 arg;

    if (BtlChar_TestFxBit(chr, 0x37)) {
        arg.objId = chr->objId;
        arg.scale = 1.0f;
        EftAbsorb_StartHands(&arg);
    }
}

/* Request 0x2E: effect 0xB between model part 0x11 and the point stored at chr + 0x1360. */
void BtlFx_SpawnFlashReq2E(FxChr *chr) {
    FxPosArg arg;

    if (BtlChar_TestFxBit(chr, 0x2E)) {
        arg.kind = 0xB;
        arg.objId = chr->objId;
        arg.unk28 = 0;
        Vec4_Copy(&arg.pos2, &chr->unk1360);
        BtlCharApi_GetNodePos(chr->objId, 0x11, &arg.pos);
        EftImpact_SpawnHit(&arg);
    }
}

/* Characters 0x37, 0x98, 0x99: keeps effect 0x1749F0 running while request 0x3A is set, or with flag 0xE out of water. */
void BtlFx_UpdateCharaFx(FxChr *chr) {
    FxArg2 arg;
    s32 on = 0;

    if (!BtlCharApi_IsModelNew(chr->objId)) {
        switch (*BtlMember_GetActive(chr)) {
        case 0x37:
        case 0x98:
        case 0x99:
            break;
        default:
            return;
        }
        if (BtlChar_TestFxBit(chr, 0x3A)) {
            on = 1;
        } else if (!BtlChar_TestFxBit(chr, 0x3B)) {
            if (BtlChar_TestFlag(chr, 0xE)) {
                on = !BtlChar_TestFlag(chr, 0x11);
            }
        }
        if (on) {
            if (chr->charaFxOn == 0) {
                arg.objId = chr->objId;
                arg.scale = 1.0f;
                EftCharaFx_Start(&arg);
            }
            chr->charaFxOn = 1;
            return;
        }
    }
    if (chr->charaFxOn == 1) {
        EftCharaFx_Stop(chr->objId);
    }
    chr->charaFxOn = 0;
}

/* Requests 0x2C / 0x2D: a clash flash halfway to the opponent (size 3 or 6); the big one shakes nearby cameras. */
void BtlFx_SpawnClashFlash(FxChr *chr) {
    FxPosArg arg;
    Vec4 a;
    Vec4 b;
    s32 shake = 0;
    f32 size = -1.0f;

    if (BtlChar_TestFxBit(chr, 0x2C)) {
        size = 3.0f;
    }
    if (BtlChar_TestFxBit(chr, 0x2D)) {
        size = 6.0f;
        shake = 1;
    }
    if (size > 0.0f) {
        BtlCharApi_GetNodePos(chr->objId, 3, &a);
        BtlCharApi_GetNodePos(BtlOpp_GetObjId(chr), 3, &b);
        Vec4_Lerp(&arg.pos, &a, &b, 0.5f);
        arg.objId = chr->objId;
        arg.unk28 = 0;
        Vec4_Copy(&arg.pos2, &BtlChar_GetPos(chr)->unk80);
        arg.kind = 6;
        EftImpact_SpawnHitScaled(&arg, size);
        arg.kind = 0xD;
        EftImpact_SpawnHitScaled(&arg, size);
        {
            f32 near = 200.0f;
            f32 far = 1500.0f;

            if (shake) {
                BtlCharApi_ShakeCamsNear(&arg.pos, near, far, 6.0f, 0.2f);
            }
        }
    }
}

/* Request 0x1D: object trigger 0. */
void BtlFx_TriggerObjReq1D(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_TestFxBit(chr, 0x1D)) {
        BtlObjFlash_Start(obj, 0);
    }
}

/* Request 0x1E: object trigger 1, repeated every 4 frames while set. */
void BtlFx_TriggerObjReq1E(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_TestFxBit(chr, 0x1E)) {
        if (chr->trigger1E == 0) {
            BtlObjFlash_Start(obj, 1);
        }
        chr->trigger1E = (chr->trigger1E + 1) % 4;
    } else {
        chr->trigger1E = 0;
    }
}

/* Request 0x1F: object trigger 2, repeated every 4 frames while set. */
void BtlFx_TriggerObjReq1F(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_TestFxBit(chr, 0x1F)) {
        if (chr->trigger1F == 0) {
            BtlObjFlash_Start(obj, 2);
        }
        chr->trigger1F = (chr->trigger1F + 1) % 4;
    } else {
        chr->trigger1F = 0;
    }
}

/* Request 3: object flag byte 2 becomes 8 when it starts and 1 when it ends. */
void BtlFx_UpdateObjFlagReq3(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_IsFxBitNew(chr, 3)) {
        obj->flags = (obj->flags & 0xFF00FFFF) | 0x80000;
    }
    if (BtlChar_IsFxBitEnded(chr, 3)) {
        obj->flags = (obj->flags & 0xFF00FFFF) | 0x10000;
    }
}

/* Object flag 0x2000000 = the camera trace hit something and flag 0x12D is clear. */
void BtlFx_UpdateOccludedFlag(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);
    s32 on = 0;

    if (chr->camTrace != 0) {
        on = !BtlChar_TestFlag(chr, 0x12D);
    }
    if (on) {
        obj->flags |= 0x2000000;
    } else {
        obj->flags &= ~0x2000000;
    }
}

/* Fighter flag 0x137 sends the object command 1; otherwise command 9 once BtlObj_GetEyeFrame answers 1. */
void BtlFx_UpdateObjCmdFlag137(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_TestFlag(chr, 0x137)) {
        BtlObj_SetEyeFrame(obj, 1);
    } else if (BtlObj_GetEyeFrame(obj) == 1) {
        BtlObj_SetEyeFrame(obj, 9);
    }
}

/* Fighter flag 0x96 holds the object in sub-state 5 (from 0 or 4); clearing it returns to 0. */
void BtlFx_UpdateSubStateFlag96(FxChr *chr) {
    FxObj *obj;
    s32 state;

    if (BtlChar_TestFlag(chr, 0x96)) {
        obj = BtlChar_GetObj(chr);
        state = BtlObj_GetMouthMode(obj);
        if (state == 0 || state == 4) {
            BtlObj_SetSubState(obj, 5, 0);
        }
    } else {
        obj = BtlChar_GetObj(chr);
        if (BtlObj_GetMouthMode(obj) == 5) {
            BtlObj_SetSubState(obj, 0, 0);
        }
    }
}

/* Object flag 0x80 follows action parameter 0x79 while the action has attribute 0x200000 (only with flag 2 set). */
void BtlFx_UpdateObjFlag80(FxChr *chr) {
    FxObj *obj = BtlChar_GetObj(chr);
    s32 on = 0;

    if (BtlParam_GetFlags(chr) & 0x200000) {
        on = BtlMember_HasAbility(chr, 0x79) != 0;
    }
    if (on) {
        if ((*(u64 *)&obj->flags & 0x82) != 0x82) {
            obj->flags |= 0x80;
        }
    } else {
        if ((*(u64 *)&obj->flags & 0x82) == 0x82) {
            obj->flags &= ~0x80;
        }
    }
}

/*
 * Animation event 4 (a hit landed on this fighter): one to n sparks at the hit position, the hit sound and a 0.1 s
 * buzz of the small motor. Reaction kind 0 jitters every spark with three BtlChar_RandF draws (x, y, z; +-0.2, or
 * -0.2..0 on y when the fighter is not above three times its body height); kinds 2..6 with flag 5 push the spark
 * sideways (2) or along the hit vector by the opponent distance.
 */
/*
 * Matching notes: every case of the size switch has its own body (the compiler merges the identical ones after
 * register allocation; written with shared bodies the function has fewer instructions in front of the loop, the
 * hit count and `code` then outrank the object pointer and the hoisted constant 2 for a saved register, and the
 * registers come out differently). The two owner stores are in the order objId2, objId.
 */
void BtlFx_FireKiBlast(FxChr *chr) {
    FxHitArg2 arg;
    Vec4 hitPos;
    Vec4 dir;
    s32 code;
    FxObj *obj = BtlChar_GetObj(chr);
    s32 react;
    s32 count;
    s32 i;
    f32 speed;

    if (BtlAnim_TestAttr(chr, 4)) {
        react = BtlKiBlast_GetUnk31(chr);
        code = BtlObjAnim_MaskToNode(BtlObjAnim_GetEventArg(obj, 4));
        Vec4_Copy(&hitPos, &chr->hitPos);
        count = BtlKiBlast_GetHits(chr);
        if (count <= 0) {
            count = 1;
        }
        arg.code = code;
        arg.objId2 = chr->objId;
        arg.objId = chr->objId;
        arg.unk16 = obj->unkA24;
        arg.type = BtlKiBlast_GetType(chr);
        arg.unk1B = BtlKiBlast_GetUnk3(chr);
        arg.level = BtlKiBlast_GetCurrentKind(chr);
        arg.unk1C = BtlKiBlast_GetFrames(chr);
        arg.unk20 = BtlKiBlast_GetSpeed(chr);
        arg.unk24 = BtlKiBlast_GetTurnRate(chr);
        arg.unk28 = BtlKiBlast_GetUnk28(chr);
        arg.unk2C = BtlKiBlast_GetUnk2C(chr);
        arg.unk34 = 1;
        arg.unk38 = 1;
        arg.unk3C = (BtlKiBlast_GetFlags(chr) >> 5) & 1;
        arg.unk41 = BtlObjAnim_QueryEvent(obj, 4, 0, 3);
        switch (arg.level) {
        case 0:
            arg.size = 0;
            break;
        case 1:
            arg.size = 1;
            break;
        case 2:
            arg.size = 2;
            break;
        case 3:
            arg.size = 3;
            break;
        case 4:
            arg.size = 0;
            break;
        case 5:
            arg.size = 1;
            break;
        case 6:
            arg.size = 2;
            break;
        case 7:
            arg.size = 3;
            break;
        case 8:
            arg.size = 0;
            break;
        case 9:
            arg.size = 1;
            break;
        case 10:
            arg.size = 2;
            break;
        case 11:
            arg.size = 3;
            break;
        default:
            arg.size = 0;
            break;
        }
        if (BtlChar_TestMemberUnk70(chr)) {
            switch (arg.level) {
            case 0:
            case 4:
            case 8:
                arg.type = 0;
                break;
            default:
                arg.type = 1;
                break;
            }
        }
        for (i = 0; i < count; i++) {
            Vec4_Copy(&arg.pos, &hitPos);
            switch (react) {
            case 1:
                break;
            case 0:
                speed = BtlAct_GetHeight(chr);
                if (!(BtlCharApi_GetHeight(chr->objId) * 3.0f < speed)) {
                    arg.pos.x += (BtlChar_RandF() - 0.5f) * 0.4f;
                    arg.pos.y += (BtlChar_RandF() - 1.0f) * 0.2f;
                } else {
                    arg.pos.x += (BtlChar_RandF() - 0.5f) * 0.4f;
                    arg.pos.y += (BtlChar_RandF() - 0.5f) * 0.4f;
                }
                arg.pos.z += (BtlChar_RandF() - 0.5f) * 0.4f;
                break;
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
                if (BtlChar_TestFlag(chr, 5)) {
                    f32 d = BtlOpp_GetDistance(chr) * 0.8f / arg.unk20 * arg.unk24;

                    d *= 0.5f;
                    if (d > 0.75f) {
                        d = 0.75f;
                    }
                    if (d < 0.2f) {
                        d = 0.2f;
                    }
                    if (react == 2) {
                        dir.x = hitPos.z;
                        dir.y = 0.0f;
                        dir.z = -hitPos.x;
                        Vec3_Normalize(&dir, &dir);
                        Vec3_RotateAxis(&arg.pos, &arg.pos, &dir, d);
                    } else {
                        s32 back = 0;

                        switch (react) {
                        case 3:
                            back = BtlChar_TestFlag(chr, 0x8D) != 0;
                            BtlChar_ToggleHeldFlag(chr, 0x8D);
                            break;
                        case 4:
                            back = 1;
                            break;
                        case 5:
                            break;
                        case 6:
                            back = BtlObj_GetNodeSide(code) == 0;
                            break;
                        }
                        if (back) {
                            Vec3_RotateY(&arg.pos, &arg.pos, -d);
                        } else {
                            Vec3_RotateY(&arg.pos, &arg.pos, d);
                        }
                    }
                }
                break;
            }
            if (arg.type == 5) {
                EftDisc_SpawnFromNode(&arg);
            } else if (arg.type == 4) {
                EftDisc_Throw(&arg);
            } else if (arg.type == 2) {
                EftKiObj_Create(&arg);
            } else if (arg.type == 3) {
                EftKiBomb_Fire(&arg);
            } else {
                EftKiBlast_Fire(&arg);
            }
        }
        switch (arg.type) {
        case 2:
        case 3:
            BtlCharSnd_PlayCommon(chr, 1);
            break;
        default:
            BtlCharSnd_PlayCommon(chr, BtlChar_FrameMod(2) + 0xF);
            break;
        }
        BtlChar_SetSmallVibration(chr, 0.1f);
    }
}
