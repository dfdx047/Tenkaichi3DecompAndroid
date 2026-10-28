#include "common.h"
#include "battle/btl_char_api.h"

/*
 * Fighter interface, 0x207020..0x208430: 58 accessors keyed by battle object id (or by nothing), used by the camera,
 * the effect scene and effect modules, the sequence, sound and the AI. See include/battle/btl_char_api.h.
 *
 * This is a slice of a larger original object, not a whole one. Evidence for the object's extent:
 *   - the same accessor style (BtlObj_Get / BtlChar_FindByObjId on the first argument, 0 for a non-fighter) starts at
 *     0x204E78, right after the last fighter state handler (0x204E28, which takes the fighter pointer), and runs to
 *     0x209EE8, where the script-control functions keyed by side (BtlCtrl_*, BtlChar_Get(side)) begin;
 *   - the float pool is one run: 0x2FE07C (BtlCharApi_ObjPushDir) .. 0x2FE0D4 (BtlCharApi_TestOppSkillFlags); 0x2FE078 belongs to
 *     BtlAct_CheckRecoveryInput before it and 0x2FE0E4 to BtlAtk_GetId after it;
 *   - calls inside it are compiled as same-file calls (BtlCharApi_IsCamShown -> BtlCharApi_IsInClashA / BtlCharApi_IsInClashBC here,
 *     BtlCharApi_GetTechniqueProgress -> 0x207C60 / 0x207C88 / 0x207CB0, BtlCharApi_FindIncomingBlast -> BtlCharApi_GetPos).
 * So the object starts at 0x204E78 and ends at 0x209EE8 or later (0x20B4A8 if the BtlCtrl functions are part of it).
 * The slice decompiled here uses no float pool entry, string or jump table, so it can be linked as its own file.
 *
 * Callees are named by address; what each does is in the comment next to its declaration (read from its code).
 */

extern BtlCharApiMgr *gBtlChars;

extern s32 BtlChar_GetCount(void);
extern BtlCharApiChr *BtlChar_Get(s32 idx);
extern BtlCharApiChr *BtlChar_FindByObjId(s32 objId);
extern BtlCharApiObj *BtlObj_Get(s32 objId);
extern s32 BtlChar_TestFlag(BtlCharApiChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(BtlCharApiChr *chr, s32 flag);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec4_SetZeroW1(Vec4 *dst);                    /* dst = 0 */
extern void Vec4_SetZero(Vec4 *dst);                    /* dst = 0 */
extern f32 Vec3_Dist(Vec4 *a, Vec4 *b);              /* distance between two points */

extern f32 BtlAnim_GetFrame(BtlCharApiChr *chr);            /* BtlObj_Get(chr->objId)->unkC78 */
extern s32 BtlAnim_GetId(BtlCharApiChr *chr);            /* chr->unk974 */
extern void *BtlAnim_GetFlags(s32 idx);                     /* gBtlChars->unk20[idx], idx < 0x19E */
extern s32 BtlAnim_TestAttr(BtlCharApiChr *chr, u64 mask);  /* 0 while chr->unk990 > 0, else BtlObjAnim_TestEvent on its object */
extern void ChrCam_AddShake(BtlCharApiChr *chr, f32 a, f32 b); /* CamShake_Add(&chr->camShake, a, b) if chr->camShakeOn */
extern s32 ChrCam_IsCutActive(BtlCharApiChr *chr);
extern BtlCharApiVitals *BtlMember_GetActiveGauge(BtlCharApiChr *chr); /* the active member's vitals */
extern s32 BtlReplay_GetViewSide(void);
extern void BtlCharSnd_RequestAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far); /* sound request, owner -1 */
extern void BtlCharSnd_PlayOwn(BtlCharApiChr *chr, s32 id);  /* sound request at the fighter: kind side + 2, 200 / 1500 */
extern s32 BtlCharSnd_GetBankMask(s32 idx);                       /* table of words at 0x2EF290 */
extern s32 BtlOpp_GetObjId(BtlCharApiChr *chr);            /* object id of the other side's fighter */
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);         /* clamp */
extern BtlCharApiObj *BtlChar_GetObj(BtlCharApiChr *chr); /* BtlObj_Get(chr->objId) */
extern Vec4 *BtlChar_GetPos(BtlCharApiChr *chr);          /* &chr->pos */
extern s32 BtlChar_IsFrozen(BtlCharApiChr *chr);            /* chr->unk1320 > 0 */
extern s32 BtlChar_IsDead(BtlCharApiChr *chr);            /* vitals->hp < 1 */
extern s32 BtlChar_TestMemberUnk70(BtlCharApiChr *chr);            /* vitals->unk30 != 0 */
extern void BtlChar_SetVibration(BtlCharApiChr *chr, f32 power, f32 time);
extern void BtlChar_SetSmallVibration(BtlCharApiChr *chr, f32 time);
extern s32 BtlAct_GetCurrent(BtlCharApiChr *chr);            /* chr->action */
extern s32 BtlAct_IsTechniqueId(s32 action);                    /* action == 0x105 or in 0x106..0x132 */
extern s32 BtlAct_GetCurrentClass(BtlCharApiChr *chr);            /* technique slot of the current action, or -1 */
extern s32 BtlAct_GetMotionLevel(BtlCharApiChr *chr, s32 action);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);                     /* action id in 0x12D..0x12F or 0x139..0x13B */
extern s32 BtlCharApi_IsInClashA(s32 objId);                     /* action id in 0x130..0x132 */
extern s32 BtlCharApi_IsInClashBC(s32 objId);                     /* action id 0xFA or 0xFC */
extern s32 BtlParam_GetUnk84(BtlCharApiChr *chr, u32 n);
extern s32 BtlParam_GetUnk8A(BtlCharApiChr *chr, u32 n);
extern s32 BtlSuper_GetFlags(BtlCharApiChr *chr, s32 slot);  /* attribute word of technique `slot` */
extern s32 BtlSuper_IsThrow(BtlCharApiChr *chr, s32 slot);
extern s32 BtlObjAnim_TestEvent(BtlCharApiObj *obj, u64 mask);
extern s32 BtlObjAnim_GetEventArg(BtlCharApiObj *obj, u64 mask);
extern s32 BtlObjAnim_MaskToNode(s32 bits);
extern s32 BtlObjAnim_QueryEvent(BtlCharApiObj *obj, u64 arg1, s32 arg2, s32 arg3);
extern void BtlObj_SetColorMode(BtlCharApiObj *obj, s32 bit, s32 on);

extern s32 DemoCam_IsActive(void);
extern s32 Battle_IsSplitScreen(void);
extern s32 BattleReplay_IsActive(void);
extern s32 BtlCam_GetDefaultView(void);

/* Whether the object has both bits 0x02 and 0x20 of its flag word. */
s32 BtlCharApi_ObjHasFlags22(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return (obj->u.flags64 & 0x22) == 0x22;
    }
    return 0;
}

/* Whether the object has both bits 0x02 and 0x40 of its flag word. */
s32 BtlCharApi_ObjHasFlags42(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return (obj->u.flags64 & 0x42) == 0x42;
    }
    return 0;
}

/* Whether any fighter has flag 0x128 (the battle-end check waits while one does). */
s32 BtlCharApi_AnyHasFlag128(void) {
    s32 i;

    if (gBtlChars == NULL) {
        return 0;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        if (BtlChar_TestFlag(BtlChar_Get(i), 0x128)) {
            return 1;
        }
    }
    return 0;
}

/* Whether the fighter's active member has no health left. */
s32 BtlCharApi_IsHpEmpty(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_IsDead(chr);
    }
    return 0;
}

/* Fighter flag 0xA4. */
s32 BtlCharApi_TestFlagA4(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xA4);
    }
    return 0;
}

/* Word +0x60 of the fighter's active member block. */
s32 BtlCharApi_GetMemberUnk60(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->unk20;
    }
    return 0;
}

/* Whether the fighter has flag 8 and is in action 0x104. */
s32 BtlCharApi_IsFlag8Action104(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (BtlChar_TestFlag(chr, 8) && BtlAct_GetCurrent(chr) == 0x104) {
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Whether the fighter is in action 0x103 or has flag 0xA6. */
s32 BtlCharApi_IsAction103OrFlagA6(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (BtlAct_GetCurrent(chr) == 0x103) {
            return 1;
        }
        return BtlChar_TestFlag(chr, 0xA6) != 0;
    }
    return 0;
}

/* Whether word +0x70 of the fighter's active member block is set. */
s32 BtlCharApi_HasMemberUnk70(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestMemberUnk70(chr);
    }
    return 0;
}

/* Sets held flag 0xA7 on the fighter. */
void BtlCharApi_SetHeldFlagA7(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0xA7);
    }
}

/* Sets held flag 0xA8 on the fighter. */
void BtlCharApi_SetHeldFlagA8(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0xA8);
    }
}

/* Sets held flag 0xA9 on the fighter. */
void BtlCharApi_SetHeldFlagA9(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0xA9);
    }
}

/* Sets held flag 0xAA on the fighter. */
void BtlCharApi_SetHeldFlagAA(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0xAA);
    }
}

/* Sets held flag 0xAB on the fighter (the effect scene does it to the target of a finishing technique). */
void BtlCharApi_SetHeldFlagAB(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0xAB);
    }
}

/* Queues a positioned sound with no owner in the fighter manager's 4-entry request list (played by BtlCharSnd_PlayRequests). */
void BtlCharApi_PlaySoundAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far) {
    BtlCharSnd_RequestAt(pos, kind, id, near, far);
}

/* Queues sound 0x25 + n or 0x29 + n at the fighter, by the class (0 / 1) of the technique it is performing.
   Declared int with no return statement: the original calls BtlCharSnd_PlayOwn with jal and falls into the epilogue
   instead of tail-calling it, which a void function would not do. No caller uses a result. */
s32 BtlCharApi_PlayTechniqueSound(s32 objId, u32 n) {
    s32 base = -1;
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL && n < 4) {
        switch (BtlAct_GetCurrentClass(chr)) {
        case 0:
            base = 0x25;
            break;
        case 1:
            base = 0x29;
            break;
        }
        if (base >= 0) {
            BtlCharSnd_PlayOwn(chr, base + n);
        }
    }
}

/* Number of live sound handles of a side (4 slots). */
s32 BtlCharApi_GetSoundCount(s32 side) {
    s32 count = 0;
    s32 i;

    BtlCharApiSound *e;

    if (gBtlChars == NULL) {
        return 0;
    }
    e = gBtlChars->sounds[side].slot;
    for (i = 0; i < 4; i++, e++) {
        if (e->handle >= 0) {
            count++;
        }
    }
    return count;
}

/* The n-th live sound of a side: its handle, the table word for its byte +8, its word +4. */
void BtlCharApi_GetSound(s32 side, s32 n, s32 *handle, s32 *out3, s32 *out4) {
    s32 count = 0;
    s32 i;
    BtlCharApiSound *e;

    if (gBtlChars == NULL) {
        return;
    }
    e = gBtlChars->sounds[side].slot;
    for (i = 0; i < 4; i++, e++) {
        if (e->handle >= 0) {
            if (count == n) {
                if (handle != NULL) {
                    *handle = e->handle;
                }
                if (out3 != NULL) {
                    *out3 = BtlCharSnd_GetBankMask(e->unk8);
                }
                if (out4 != NULL) {
                    *out4 = e->unk4;
                }
                return;
            }
            count++;
        }
    }
}

/* Clears bit 3 of the object's mask at +0xB34. */
void BtlCharApi_ObjClearMaskBit3(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        BtlObj_SetColorMode(obj, 3, 0);
    }
}

/* Sets bit 3 of the object's mask at +0xB34. */
void BtlCharApi_ObjSetMaskBit3(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        BtlObj_SetColorMode(obj, 3, 1);
    }
}

/* Bit 21 of the object's flag word. */
s32 BtlCharApi_ObjTestFlagBit21(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return (obj->u.flags >> 21) & 1;
    }
    return 0;
}

/* Float at fighter +0xE44. */
f32 BtlCharApi_GetUnkE44(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->unkE44;
    }
    return 0.0f;
}

/* Fighter counter +0xE5C as a 0..1 ratio of 3. */
f32 BtlCharApi_GetUnkE5CRatio(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);


    if (chr == NULL) {
        return 0.0f;
    }
    return BtlUtil_ClampF((f32)chr->unkE5C / 3.0f, 0.0f, 1.0f);
}

/* Fighter counter +0xE60 as a 0..1 ratio of 5. */
f32 BtlCharApi_GetUnkE60Ratio(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr == NULL) {
        return 0.0f;
    }
    return BtlUtil_ClampF((f32)chr->unkE60 / 5.0f, 0.0f, 1.0f);
}

/* Sets or clears bit 0x100 of the object's flag word. */
void BtlCharApi_ObjSetFlag100(s32 objId, s32 on) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        if (on) {
            obj->u.flags |= 0x100;
        } else {
            obj->u.flags &= ~0x100;
        }
    }
}

/* Whether the object has both bits 0x02 and 0x100 of its flag word. */
s32 BtlCharApi_ObjHasFlags102(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return (obj->u.flags64 & 0x102) == 0x102;
    }
    return 0;
}

/* Whether both fighters still have health and the target is below half of its maximum. */
s32 BtlCharApi_IsTargetBelowHalfHp(s32 objId, s32 targetId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);
    BtlCharApiChr *target = BtlChar_FindByObjId(targetId);

    s32 result = 0;

    if (chr != NULL && target != NULL) {
        if (BtlChar_IsDead(chr)) {
            return 0;
        }
        if (BtlChar_IsDead(target)) {
            return 0;
        }
        return BtlMember_GetActiveGauge(target)->hp < BtlMember_GetActiveGauge(target)->hpMax / 2;
    }
    return result;
}

/* Whether the fighter's current technique has attribute 0x80000, passes BtlSuper_IsThrow and the target's member word +0x60 is 0. */
s32 BtlCharApi_CanTechniqueFinish(s32 objId, s32 targetId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);
    BtlCharApiChr *target = BtlChar_FindByObjId(targetId);
    s32 slot;
    s32 result = 0;

    if (chr != NULL && target != NULL) {
        if (!BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr))) {
            return 0;
        }
        slot = BtlAct_GetCurrentClass(chr);
        if (!(BtlSuper_GetFlags(chr, slot) & 0x80000)) {
            return 0;
        }
        if (BtlSuper_IsThrow(chr, slot)) {
            return 0;
        }
        return BtlMember_GetActiveGauge(target)->unk20 == 0;
    }
    return result;
}

/* Fighter manager counter +0x1C as a 0..1 ratio of 90. */
f32 BtlCharApi_GetMgrTimerRatio(void) {
    if (gBtlChars == NULL) {
        return 0.0f;
    }
    return BtlUtil_ClampF((f32)gBtlChars->unk1C / 90.0f, 0.0f, 1.0f);
}

/* Starts the block at +0x15D0 (strength, frames) on every fighter within `far` of pos, scaled by closeness. */
void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time) {
    Vec4 diff;
    s32 i;
    BtlCharApiChr *chr;
    f32 rate;
    f32 t;

    if (gBtlChars == NULL) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        Vec4_Sub(&diff, BtlChar_GetPos(chr), pos);
        rate = 1.0f - (Vec3_Length(&diff) - near) / (far - near);
        if (rate < 0.0f) {
            continue;
        }
        if (rate > 1.0f) {
            rate = 1.0f;
        }
        t = time * rate;
        BtlChar_SetVibration(chr, power * rate, t);
        if (rate > 0.5f) {
            BtlChar_SetSmallVibration(chr, t);
        }
    }
}

/* Whether any attribute word of the object (for a fighter: unless +0x990 > 0) has a bit of mask. */
s32 BtlCharApi_ObjTestAttr(s32 objId, u64 mask) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);
    BtlCharApiObj *obj;

    if (chr != NULL) {
        return BtlAnim_TestAttr(chr, mask);
    }
    obj = BtlObj_Get(objId);
    if (obj != NULL) {
        return BtlObjAnim_TestEvent(obj, mask);
    }
    return 0;
}

/* OR of the values of the object's attribute words that have a bit of mask. */
s32 BtlCharApi_ObjGetAttrValue(s32 objId, u64 mask) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return BtlObjAnim_GetEventArg(obj, mask);
    }
    return 0;
}

/* The same value mapped to a kind code by BtlObjAnim_MaskToNode, 0 when there is none. */
s32 BtlCharApi_ObjGetAttrKind(s32 objId, u64 mask) {
    BtlCharApiObj *obj = BtlObj_Get(objId);
    s32 bits;
    s32 kind;

    if (obj != NULL) {
        bits = BtlObjAnim_GetEventArg(obj, mask);
        kind = 0;
        if (bits != 0) {
            kind = BtlObjAnim_MaskToNode(bits);
        }
        return kind;
    }
    return 0;
}

/* Object float +0xC78, raised by the fighter's +0xEFC entries while it is in actions 0x12D..0x12F / 0x139..0x13B. */
f32 BtlCharApi_GetChargedUnkC78(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);
    f32 value;
    s32 n;
    s32 i;

    if (chr == NULL) {
        return 0.0f;
    }
    value = BtlAnim_GetFrame(chr);
    if (BtlCharApi_IsInRushSequence(objId)) {
        n = BtlAct_GetMotionLevel(chr, BtlAnim_GetId(chr));
        for (i = 0; i < n; i++) {
            value += chr->unkEFC[i] + 1.0f;
        }
    }
    return value;
}

/* Object float +0xC78. */
f32 BtlCharApi_ObjGetUnkC78(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->unkC78;
    }
    return 0.0f;
}

/* Object float +0xC80. */
f32 BtlCharApi_ObjGetUnkC80(s32 objId) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->unkC80;
    }
    return 0.0f;
}

/* BtlObjAnim_QueryEvent(obj, arg1, 0, arg2) on the object. */
s32 BtlCharApi_ObjQuery24D610(s32 objId, s32 arg1, s32 arg2) {
    BtlCharApiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return BtlObjAnim_QueryEvent(obj, arg1, 0, arg2);
    }
    return 0;
}

/* Fighter word +0x974. */
s32 BtlCharApi_GetUnk974(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlAnim_GetId(chr);
    }
    return 0;
}

/* The manager's table entry for fighter word +0x974. */
void *BtlCharApi_GetUnk974Data(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlAnim_GetFlags(BtlAnim_GetId(chr));
    }
    return NULL;
}

/* Fighter flag 0x2B. */
s32 BtlCharApi_TestFlag2B(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x2B);
    }
    return 0;
}

/* Copies the fighter camera's position and rotation (zeroes them for a non-fighter); returns fighter +0x494. */
s32 BtlCharApi_GetCamPose(s32 objId, Vec4 *pos, Vec4 *rot) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        Vec4_Copy(pos, &chr->camPos);
        Vec4_Copy(rot, &chr->camRot);
        return chr->camUnk494;
    }
    Vec4_SetZeroW1(pos);
    Vec4_SetZero(rot);
    return 0;
}

/* Fighter flag 0xE: locomotion, including ground and air movement actions. */
s32 BtlCharApi_IsMoving(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    return chr != NULL ? BtlChar_TestFlag(chr, 0xE) : 0;
}

/* Float at fighter +0x4A0. */
f32 BtlCharApi_GetCamUnk4A0(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->camUnk4A0;
    }
    return 0.0f;
}

/* Fighter flag 0xD3 as 0 / 1: this fighter's camera wants the whole screen. */
s32 BtlCharApi_HasCamPriority(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xD3) != 0;
    }
    return 0;
}

/* Shakes the camera of every fighter whose +0x420 point is within `far` of pos, scaled by closeness. */
void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 arg3, f32 arg4) {
    s32 i;
    BtlCharApiChr *chr;
    f32 dist;
    f32 rate;

    if (gBtlChars == NULL) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        if (BtlChar_IsFrozen(chr)) {
            continue;
        }
        dist = Vec3_Dist(&chr->camUnk420, pos);
        if (dist < far) {
            rate = BtlUtil_ClampF(1.0f - (dist - near) / (far - near), 0.0f, 1.0f);
            ChrCam_AddShake(chr, arg3 * rate, arg4 * rate);
        }
    }
}

/* Whether this fighter's camera is the one on screen. */
s32 BtlCharApi_IsCamShown(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);
    s32 mine;
    s32 other;

    if (chr == NULL) {
        return 0;
    }
    if (BtlCharApi_IsInClashA(objId)) {
        return 0;
    }
    if (BtlCharApi_IsInClashBC(objId)) {
        return 0;
    }
    if (DemoCam_IsActive()) {
        return 1;
    }
    mine = BtlCharApi_HasCamPriority(objId);
    other = BtlCharApi_HasCamPriority(BtlOpp_GetObjId(chr));
    if (other < mine) {
        return 1;
    }
    if (mine < other) {
        return 0;
    }
    if (Battle_IsSplitScreen()) {
        if (!BattleReplay_IsActive()) {
            return 1;
        }
    }
    return chr->side == BtlCam_GetDefaultView();
}

/* Copies the vector at fighter +0x460. */
void BtlCharApi_GetCamUnk460(s32 objId, Vec4 *out) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        Vec4_Copy(out, &chr->camUnk460);
    }
}

/* Manager word +0x134 while +0x130 is 0, else 0 (the default view during a replay). */
s32 BtlCharApi_GetMgrUnk134(void) {
    return BtlReplay_GetViewSide();
}

/* Whether any fighter passing ChrCam_IsCutActive has flag 0xD3. */
s32 BtlCharApi_AnyCamPriority(void) {
    s32 i;
    BtlCharApiChr *chr;

    if (gBtlChars == NULL) {
        return 0;
    }
    for (i = 0; i < gBtlChars->count; i++) {
        chr = BtlChar_Get(i);
        if (ChrCam_IsCutActive(chr) && BtlChar_TestFlag(chr, 0xD3)) {
            return 1;
        }
    }
    return 0;
}

/* Whether the fighter's input is injected (CPU) instead of read from a pad. */
s32 BtlCharApi_IsInputInjected(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->injectOn;
    }
    return 0;
}

/* Stores the buttons and stick the fighter's input is built from when it is injected (the AI calls this). */
void BtlCharApi_SetInjectedInput(s32 objId, u32 buttons, f32 stickX, f32 stickY) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        chr->injectButtons = buttons;
        chr->injectStickX = stickX;
        chr->injectStickY = stickY;
    }
}

/* Fighter flag 0xF. */
s32 BtlCharApi_TestFlag0F(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xF);
    }
    return 0;
}

/* Fighter flag 5. */
s32 BtlCharApi_TestFlag05(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 5);
    }
    return 0;
}

/* Byte 0x84 + n (n < 4) of the fighter's parameter block. */
s32 BtlCharApi_GetParamByte84(s32 objId, u32 n) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlParam_GetUnk84(chr, n);
    }
    return 0;
}

/* Byte 0x8A of the fighter's parameter block. */
s32 BtlCharApi_GetParamByte8A(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlParam_GetUnk8A(chr, 0);
    }
    return 0;
}

/* Byte 0x8D of the fighter's parameter block. */
s32 BtlCharApi_GetParamByte8D(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlParam_GetUnk8A(chr, 3);
    }
    return 0;
}

/* The 64-bit word at fighter +0x1288. */
u64 BtlCharApi_GetUnk1288(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->unk1288;
    }
    return 0;
}

/* 1 if object byte +0xCAD is not the complement of +0xCAC; else 0 with flag 0x60; else whether +0xCAD is >= 0. */
s32 BtlCharApi_CheckUnkCAC(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);
    BtlCharApiObj *obj;

    if (chr == NULL) {
        return 0;
    }
    obj = BtlChar_GetObj(chr);
    if (obj == NULL) {
        return 0;
    }
    if (obj->unkCAD != ~obj->unkCAC) {
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x60) == 1) {
        return 0;
    }
    return obj->unkCAD >= 0;
}

/* Fighter flag 0x60. */
s32 BtlCharApi_TestFlag60(s32 objId) {
    BtlCharApiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x60);
    }
    return 0;
}
