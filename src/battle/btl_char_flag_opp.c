#include "common.h"
#include "battle/btl_char_flag.h"
#ifdef PORT
#include "battle/tag_challenge.h"
#endif

/*
 * Opponent helpers: 0x1DB048..0x1DBF20. Each takes a fighter and answers about the other one
 * (fighter 1 for player 0, fighter 0 otherwise).
 */

extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_LengthXZ(Vec4 *v);
extern f32 BtlUtil_MaxF(f32 a, f32 b);
extern BtlFlagChr *BtlChar_Get(s32 i);
extern BtlFlagPose *BtlChar_GetPos(BtlFlagChr *chr);
extern void *BtlChar_GetObj(BtlFlagChr *chr);
extern s32 BtlChar_IsDead(BtlFlagChr *chr);
extern void *BtlObj_Get(s32 id);
extern void ChrCam_EndCut(BtlFlagChr *chr);
extern void BtlChar_GetSnapPos(BtlFlagChr *chr, Vec4 *out, s32 slot); /* override position `slot` when its bit of +0x3C0 is set, else the position */
extern void BtlChar_GetSnapRot(BtlFlagChr *chr, Vec4 *out, s32 slot); /* the same for the rotation */
extern s32 BtlMember_HasAbility(BtlFlagChr *chr, s32 arg);
extern s32 BtlAct_GetCurrent(BtlFlagChr *chr);                        /* action id */
extern s32 BtlAct_IsTechniqueId(s32 action);                             /* action is 0x105..0x132 */
extern f32 BtlCharApi_GetHeight(s32 objId);                              /* object +0xFF4: height (10 without object) */
extern f32 BtlCharApi_GetBodyUnkFF8(s32 objId);                              /* object +0xFF8 (10 without object) */
extern f32 BtlCharApi_GetCenterHeight(s32 objId);                              /* object +0xFFC (5 without object) */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);        /* world position of a model node */
extern f32 BtlCharApi_GetRadius(s32 objId);                              /* body radius */
extern s32 BtlCharApi_IsInSkill(s32 objId);                              /* action is 0xFD..0x102 */
extern s32 BtlParam_GetUnk0(BtlFlagChr *chr);                        /* u16 at +0 of the object's parameter block */
extern s32 BtlParam_GetUnk2(BtlFlagChr *chr);                        /* s8 at +2 of the object's parameter block */
extern f32 BtlParam_GetUnkC7Scale(BtlFlagChr *chr);

#define OPPONENT(chr) BtlChar_Get(TagTeam_Target((chr)->player))

/* Where to aim at the opponent: its override position 6 when set, else its position. */
void BtlOpp_GetTargetPos(BtlFlagChr *chr, Vec4 *out) {
    BtlChar_GetSnapPos(OPPONENT(chr), out, 6);
}

/* The opponent's override position `slot` when set, else its position. */
void BtlOpp_GetSnapPos(BtlFlagChr *chr, Vec4 *out, s32 slot) {
    BtlChar_GetSnapPos(OPPONENT(chr), out, slot);
}

/* The opponent's override rotation 6 when set, else its rotation. */
void BtlOpp_GetTargetRot(BtlFlagChr *chr, Vec4 *out) {
    BtlChar_GetSnapRot(OPPONENT(chr), out, 6);
}

/* The opponent's override rotation `slot` when set, else its rotation. */
void BtlOpp_GetSnapRot(BtlFlagChr *chr, Vec4 *out, s32 slot) {
    BtlChar_GetSnapRot(OPPONENT(chr), out, slot);
}

/* Y angle of BtlOpp_GetTargetRot. */
f32 BtlOpp_GetTargetYaw(BtlFlagChr *chr) {
    Vec4 rot;

    BtlChar_GetSnapRot(OPPONENT(chr), &rot, 6);
    return rot.y;
}

/* Vector from the fighter to the opponent's target position. */
void BtlOpp_GetDelta(BtlFlagChr *chr, Vec4 *out) {
    Vec4 pos;

    BtlOpp_GetTargetPos(chr, &pos);
    Vec4_Sub(out, &pos, &BtlChar_GetPos(chr)->pos);
}

/* Copies the vector at +0x30 of the opponent's pose. */
void BtlOpp_GetPoseVec30(BtlFlagChr *chr, Vec4 *out) {
    Vec4_Copy(out, &BtlChar_GetPos(OPPONENT(chr))->unk30);
}

/* Copies the opponent's velocity (pose +0x40). */
void BtlOpp_GetVelocity(BtlFlagChr *chr, Vec4 *out) {
    Vec4_Copy(out, &BtlChar_GetPos(OPPONENT(chr))->vel);
}

/* Distance to the opponent. */
f32 BtlOpp_GetDistance(BtlFlagChr *chr) {
    Vec4 d;

    BtlOpp_GetDelta(chr, &d);
    return Vec3_Length(&d);
}

/* Horizontal distance to the opponent. */
f32 BtlOpp_GetDistanceXZ(BtlFlagChr *chr) {
    Vec4 d;

    BtlOpp_GetDelta(chr, &d);
    return BtlUtil_LengthXZ(&d);
}

/* Horizontal distance between the two bodies: distance minus both radii, never negative. */
f32 BtlOpp_GetGapXZ(BtlFlagChr *chr) {
    f32 d = BtlOpp_GetDistanceXZ(chr);

    d -= BtlCharApi_GetRadius(chr->objId);
    d -= BtlOpp_GetRadius(chr);
    return BtlUtil_MaxF(d, 0.0f);
}

/* Yaw of the direction to the opponent; 0 when it is straight above or below. */
f32 BtlOpp_GetYaw(BtlFlagChr *chr) {
    Vec4 d;

    BtlOpp_GetDelta(chr, &d);
    if (__builtin_fabsf(d.x) < 0.0001f && __builtin_fabsf(d.z) < 0.0001f) {
        return 0.0f;
    }
    return atan2f(d.x, d.z);
}

/* Yaw to the opponent relative to the fighter's own rotation. */
f32 BtlOpp_GetYawFromFacing(BtlFlagChr *chr) {
    f32 yaw = BtlOpp_GetYaw(chr);
    BtlFlagPose *pose = BtlChar_GetPos(chr);

    return BtlUtil_WrapAngle(yaw - BtlUtil_WrapAngle(pose->rot.y + BtlChar_GetPos(chr)->unk64));
}

/* Yaw to the opponent relative to the opponent's rotation. */
f32 BtlOpp_GetYawFromItsFacing(BtlFlagChr *chr) {
    f32 yaw = BtlOpp_GetYaw(chr);

    return BtlUtil_WrapAngle(yaw - BtlOpp_GetTargetYaw(chr));
}

/* Pitch of the direction to the opponent (positive when it is higher: +Y is down). */
f32 BtlOpp_GetPitch(BtlFlagChr *chr) {
    Vec4 d;
    f32 len;
    f32 pitch;

    BtlOpp_GetDelta(chr, &d);
    len = Vec3_Length(&d);
    pitch = 0.0f;
    if (0.001f < len) {
        Vec4_Scale(&d, &d, 1.0f / len);
        pitch = -Mathf_Asin(d.y);
    }
    return pitch;
}

/* The same after adding half the height difference to the vertical offset. */
f32 BtlOpp_GetPitchAdjusted(BtlFlagChr *chr) {
    Vec4 d;
    f32 len;
    f32 pitch;

    BtlOpp_GetDelta(chr, &d);
    d.y += BtlOpp_GetHalfHeightDiff(chr);
    len = Vec3_Length(&d);
    pitch = 0.0f;
    if (0.001f < len) {
        Vec4_Scale(&d, &d, 1.0f / len);
        pitch = -Mathf_Asin(d.y);
    }
    return pitch;
}

/* Half the fighter's height minus half the opponent's. */
f32 BtlOpp_GetHalfHeightDiff(BtlFlagChr *chr) {
    BtlFlagChr *opp = OPPONENT(chr);
    f32 own = BtlCharApi_GetHeight(chr->objId);

    return own * 0.5f - BtlCharApi_GetHeight(opp->objId) * 0.5f;
}

/* The opponent's body radius. */
f32 BtlOpp_GetRadius(BtlFlagChr *chr) {
    return BtlCharApi_GetRadius(OPPONENT(chr)->objId);
}

/* Signed byte +2 of the opponent's parameter block. */
s32 BtlOpp_GetParamByte2(BtlFlagChr *chr) {
    return BtlParam_GetUnk2(OPPONENT(chr));
}

/* The opponent's height (object +0xFF4). */
f32 BtlOpp_GetHeight(BtlFlagChr *chr) {
    return BtlCharApi_GetHeight(OPPONENT(chr)->objId);
}

/* The opponent's object +0xFFC. */
f32 BtlOpp_GetObjUnkFFC(BtlFlagChr *chr) {
    return BtlCharApi_GetCenterHeight(OPPONENT(chr)->objId);
}

/* Word +0 of the opponent's parameter block. */
s32 BtlOpp_GetParamWord0(BtlFlagChr *chr) {
    return BtlParam_GetUnk0(OPPONENT(chr));
}

/* The opponent's player index. */
s32 BtlOpp_GetPlayer(BtlFlagChr *chr) {
    return OPPONENT(chr)->player;
}

/* The opponent's battle object id. */
s32 BtlOpp_GetObjId(BtlFlagChr *chr) {
    return OPPONENT(chr)->objId;
}

/* The opponent's battle object. */
void *BtlOpp_GetObj(BtlFlagChr *chr) {
    return BtlChar_GetObj(OPPONENT(chr));
}

/* The opponent's action as this frame should see it: last frame's until both fighters have reached stage 6. */
s32 BtlOpp_GetSeenAction(BtlFlagChr *chr) {
    BtlFlagChr *opp = OPPONENT(chr);

    if (BtlChar_GetStage(chr) < 6 || BtlChar_GetStage(opp) < 6) {
        return opp->prevAction;
    }
    return BtlAct_GetCurrent(opp);
}

/* The same choice for the opponent's +0x964 / +0x968. */
s32 BtlOpp_GetSeenUnk964(BtlFlagChr *chr) {
    BtlFlagChr *opp = OPPONENT(chr);

    if (BtlChar_GetStage(chr) < 6 || BtlChar_GetStage(opp) < 6) {
        return opp->unk968;
    }
    return opp->unk964;
}

/* The opponent's object +0xFF8. */
f32 BtlOpp_GetObjUnkFF8(BtlFlagChr *chr) {
    return BtlCharApi_GetBodyUnkFF8(OPPONENT(chr)->objId);
}

/* Copies the opponent's vector at +0x15A0. */
void BtlOpp_GetUnk15A0(BtlFlagChr *chr, Vec4 *out) {
    BtlFlagChr *opp = OPPONENT(chr);

    Vec4_Copy(out, &opp->unk15A0);
}

/* World position of a node of the opponent's model. */
void BtlOpp_GetNodePos(BtlFlagChr *chr, s32 node, Vec4 *out) {
    BtlCharApi_GetNodePos(OPPONENT(chr)->objId, node, out);
}

/* Copies the vector the opponent's object points at with +0xFA0. */
void BtlOpp_GetObjVecFA0(BtlFlagChr *chr, Vec4 *out) {
    Vec4_Copy(out, *(Vec4 **)((u8 *)BtlObj_Get(OPPONENT(chr)->objId) + 0xFA0));
}

/* The opponent's +0xD88. */
s32 BtlOpp_GetUnkD88(BtlFlagChr *chr) {
    return OPPONENT(chr)->unkD88;
}

/* Seconds until the two fighters meet at their current velocities; 10000000 when they are not closing. */
f32 BtlOpp_GetClosingTime(BtlFlagChr *chr) {
    Vec4 pos;
    Vec4 vel;
    Vec4 oppPos;
    Vec4 oppVel;
    Vec4 d;
    Vec4 dir;
    f32 dist;
    f32 speed;
    f32 rel;
    f32 eps;

    Vec4_Copy(&pos, &BtlChar_GetPos(chr)->pos);
    eps = 0.001f;
    BtlOpp_GetTargetPos(chr, &oppPos);
    Vec4_Sub(&d, &oppPos, &pos);
    dist = Vec3_Length(&d);
    if (dist < eps) {
        return 0.0f;
    }
    Vec4_Scale(&dir, &d, 1.0f / dist);
    Vec4_Copy(&vel, &BtlChar_GetPos(chr)->vel);
    BtlOpp_GetVelocity(chr, &oppVel);
    speed = Vec3_Dot(&vel, &dir);
    if (speed < 0.0f) {
        return 10000000.0f;
    }
    rel = speed - Vec3_Dot(&oppVel, &dir);
    if (rel < eps) {
        return 10000000.0f;
    }
    return dist / rel;
}

/* BtlMember_HasAbility on the opponent. */
s32 BtlOpp_HasAbility(BtlFlagChr *chr, s32 arg) {
    return BtlMember_HasAbility(OPPONENT(chr), arg);
}

/* BtlParam_GetUnkC7Scale on the opponent. */
f32 BtlOpp_GetUnk20F318(BtlFlagChr *chr) {
    return BtlParam_GetUnkC7Scale(OPPONENT(chr));
}

/* The opponent's clash counter (+0xE50). */
s32 BtlOpp_GetClashCountB(BtlFlagChr *chr) {
    return OPPONENT(chr)->clashCountB;
}

/* Copies what the opponent is doing into this fighter's flags 0xAC..0xB9 and 0xD6. */
void BtlOpp_MirrorFlags(BtlFlagChr *chr) {
    BtlFlagChr *opp;

    opp = OPPONENT(chr);
    BtlChar_ClearFlagRange(chr, 0xAC, 0xB9);
#ifdef PORT
    /* Between gauntlet waves the target slot contains a hidden, defeated
       fighter. Mirroring its death flag would block the survivor's input. */
    if (TagChallenge_Hidden(TagTeam_Target(chr->player))) return;
#endif
    if (BtlChar_TestFlag(opp, 0xB)) {
        BtlChar_SetHeldFlag(chr, 0xAC);
    }
    if (BtlChar_TestFlag(opp, 0x1A)) {
        BtlChar_SetHeldFlag(chr, 0xAD);
    }
    if (BtlChar_TestFlag(opp, 0x3F)) {
        BtlChar_SetHeldFlag(chr, 0xAE);
    }
    if (BtlChar_TestFlag(opp, 0x85)) {
        BtlChar_SetHeldFlag(chr, 0xB3);
    }
    if (BtlAct_IsTechniqueId(BtlAct_GetCurrent(opp))) {
        BtlChar_SetHeldFlag(chr, 0xAF);
    }
    if (BtlCharApi_IsInSkill(opp->objId)) {
        BtlChar_SetHeldFlag(chr, 0xB0);
    }
    if (BtlChar_IsDead(opp)) {
        BtlChar_SetHeldFlag(chr, 0xB1);
    }
    if (BtlChar_TestFlag(opp, 0xCC)) {
        BtlChar_SetHeldFlag(chr, 0xB4);
    }
    if (BtlChar_TestFlag(opp, 0x26)) {
        BtlChar_SetHeldFlag(chr, 0xB2);
    }
    if (BtlChar_TestFlag(opp, 0x27)) {
        ChrCam_EndCut(chr);
    }
    if (BtlChar_TestFlag(opp, 0x90)) {
        BtlChar_SetFlag(chr, 0xB5);
    }
    if (BtlChar_TestFlag(opp, 0x16)) {
        BtlChar_SetFlag(chr, 0xB6);
    }
    if (BtlChar_TestFlag(opp, 0x17)) {
        BtlChar_SetFlag(chr, 0xB7);
    }
    if (BtlChar_TestFlag(opp, 0x28)) {
        BtlChar_SetFlag(chr, 0xD6);
    }
    if (BtlChar_TestFlag(opp, 0xDC)) {
        BtlChar_SetFlag(chr, 0xB8);
    }
    if (BtlChar_TestFlag(opp, 0x92)) {
        BtlChar_SetFlag(chr, 0xB9);
    }
}
