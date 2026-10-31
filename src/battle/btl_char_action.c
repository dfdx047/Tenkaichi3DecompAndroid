#include "common.h"
#include "battle/btl_char_action.h"

/*
 * Fighter action state machine, core part: 0x1E0290..0x1E3158.
 *
 * Action bookkeeping and small queries, movement helpers shared by the handlers, the attack
 * parameter block, the per-frame dispatcher (BtlAct_Update) with its three helpers, and the first
 * five handlers of the table (actions 1..6). The original object continues past 0x1E3158 with the
 * other handlers (its float constants run on), so this is a slice of a larger source file.
 *
 * See include/battle/btl_char_action.h for the handler protocol.
 */

extern f32 atan2f(f32 y, f32 x);
extern void *memset(void *dst, s32 c, u32 n);
extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
extern f32 Mathf_Asin(f32 x);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Normalize(Vec4 *out, Vec4 *in);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *in, f32 scale);
extern void Vec4_SetZero(Vec4 *v); /* zeroes a vector */

extern s32 Battle_GetMode(void);
extern s32 BattleSide_GetUnk200(s32 side);

extern f32 BtlUtil_WrapAngle(f32 a);
extern s32 BtlUtil_Approach(s32 cur, s32 target, s32 step);
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
extern f32 BtlUtil_MaxF(f32 a, f32 b);
extern s32 BtlUtil_Min(s32 a, s32 b);
extern s32 BtlUtil_Max(s32 a, s32 b);
extern BtlActChr *BtlChar_Get(s32 i);
extern void *BtlChar_GetObj(BtlActChr *chr);
extern BtlActPose *BtlChar_GetPos(BtlActChr *chr);
extern s32 BtlChar_IsFree(BtlActChr *chr);
extern s32 BtlChar_IsDead(BtlActChr *chr);
extern s32 BtlChar_TestMemberUnk70(BtlActChr *chr);
extern s32 BtlChar_FrameMod(s32 n);
extern s32 BtlChar_IsStage4Or27(void);
extern void BtlChar_SetSmallVibration(BtlActChr *chr, f32 seconds);
extern void BtlChar_PlayVoice(BtlActChr *chr, s32 kind);
extern s32 BtlChar_TestFlag(BtlActChr *chr, s32 bit);
extern void BtlChar_SetFlag(BtlActChr *chr, s32 bit);
extern void BtlChar_SetHeldFlag(BtlActChr *chr, s32 bit);
extern void BtlChar_ClearFlag(BtlActChr *chr, s32 bit);
extern s32 BtlInput_IsHeld(BtlActChr *chr, u32 mask);
extern s32 BtlInput_IsPressed(BtlActChr *chr, u32 mask);
extern void ChrCam_EndCut(BtlActChr *chr);

/* Motion layer (0x1C3xxx..0x1C4xxx): purposes inferred from their bodies. */
extern void BtlStat_EndKind3(BtlActChr *chr);
extern void BtlStat_EndKind4(BtlActChr *chr);
extern void BtlStat_Update(BtlActChr *chr);
extern void BtlAnim_Play(BtlActChr *chr, s32 motion, f32 blend); /* start a motion */
extern void BtlAnim_FlushRequest(BtlActChr *chr);
extern void BtlAnim_FlushRequestKeep(BtlActChr *chr);
extern void BtlAnim_FlushSubToMain(BtlActChr *chr);
extern void BtlAnim_PlaySub(BtlActChr *chr, s32 motion);
extern void BtlAnim_SetUnkC8C(BtlActChr *chr, f32 weight);
extern void BtlAnim_SetObjRate(BtlActChr *chr, f32 v);              /* obj + 0xCB8 = v */
extern s32 BtlAnim_GetId(BtlActChr *chr);                      /* chr->motion */
extern s32 BtlAnim_GetPrevId(BtlActChr *chr);                      /* chr + 0x97C */
extern u32 BtlAnim_GetFlags(s32 motion);                          /* roster motion flags */
extern f32 BtlAnim_GetFrame(BtlActChr *chr);                      /* obj + 0xC78 */
extern f32 BtlAnim_GetProgress(BtlActChr *chr);                      /* motion progress 0..1 */
extern f32 BtlAnim_GetObjRate(BtlActChr *chr);                      /* obj + 0xCB8 */
extern s32 BtlAnim_Advance(BtlActChr *chr, s32 arg);             /* motion finished */
extern s32 BtlAnim_AdvanceThen(BtlActChr *chr, s32 motion, f32 blend, s32 arg); /* chain to motion when finished */
extern void BtlAnim_AdvanceLoop(BtlActChr *chr, s32 arg);
extern s32 BtlAnim_PassedFrame(BtlActChr *chr, f32 frame);
extern s32 BtlAnim_IsNew(BtlActChr *chr);

/* Members and gauges (0x1CExxx..0x1CFxxx). */
extern BtlActMember *BtlMember_Get(BtlActChr *chr, s32 member);
extern BtlActMember *BtlMember_GetActive(BtlActChr *chr);            /* active member */
extern BtlActVitals *BtlMember_GetGauge(BtlActChr *chr, s32 member);
extern BtlActVitals *BtlMember_GetActiveGauge(BtlActChr *chr);            /* active member's gauge block */
extern s32 BtlMember_CountAlive(BtlActChr *chr);
extern s32 BtlMember_GetSwitchTarget(BtlActChr *chr);
extern void BtlMember_NextSwitchTarget(BtlActChr *chr);
extern void BtlMember_PrevSwitchTarget(BtlActChr *chr);
extern void BtlMember_AddHealth(BtlActChr *chr, s32 amount);
extern void BtlMember_AddKi(BtlActChr *chr, s32 amount);
extern s32 BtlMember_HasKi(BtlActChr *chr, s32 amount);
extern void BtlMember_SpendKi(BtlActChr *chr, s32 amount, s32 arg);
extern void BtlMember_AddBlast(BtlActChr *chr, s32 amount);
extern void BtlMember_SubMaxPower(BtlActChr *chr, s32 amount);
extern s32 BtlMember_IsKiEmpty(BtlActChr *chr);
extern s32 BtlMember_IsMaxPowerEmpty(BtlActChr *chr);
extern s32 BtlMember_HasAbility(BtlActChr *chr, s32 ability);          /* active member has the ability */
extern s32 BtlMember_HasAbilityOf(BtlActChr *chr, s32 member, s32 ability);
extern void BtlChar_SetFxBit(BtlActChr *chr, s32 bit);             /* request bit at chr + 0x1262 */

extern s32 BtlChars_IsTimeStopped(void);                                 /* roster "time stopped" word */
extern void BtlChar_ApplyRootYaw(BtlActChr *chr);
extern void BtlCharSnd_PlayCommon(BtlActChr *chr, s32 sound);
extern void BtlChar_SetStage(BtlActChr *chr, s32 stage);           /* per-frame stage number */
extern void BtlChar_ClearFlagRange(BtlActChr *chr, s32 first, s32 last); /* clear a range of flags */
extern s32 BtlChar_TestPrevFlag(BtlActChr *chr, s32 bit);
extern void BtlOpp_GetDelta(BtlActChr *chr, Vec4 *out);
extern f32 BtlOpp_GetDistanceXZ(BtlActChr *chr);
extern s32 BtlOpp_GetParamByte2(BtlActChr *chr);
extern s32 BtlOpp_GetPlayer(BtlActChr *chr);
extern void BtlOpp_GetNodePos(BtlActChr *chr, s32 node, Vec4 *out);
extern void BtlMove_SetHeading(BtlActChr *chr, f32 a, f32 b);
extern void BtlMove_Step(BtlActChr *chr, s32 a, s32 b, s32 c, f32 x, f32 y);
extern void BtlMove_ApplyGravity(BtlActChr *chr);
extern void BtlMove_SetLeanX(BtlActChr *chr, f32 speed);
extern void BtlMove_ApplyImpulse(BtlActChr *chr);
extern void BtlMove_UpdateHoverOffset(BtlActChr *chr);
extern void BtlMove_UpdateDefenseTimers(BtlActChr *chr);

extern void BtlSkill_UpdateTimers(BtlActChr *chr);
extern void BtlDecide_QueueAttack(BtlActChr *chr, s32 attack);          /* queue an attack */
extern s32 BtlAct_QueueReaction(BtlActChr *chr, s32 arg);
extern s32 BtlAct_CheckStoryForced(BtlActChr *chr);
extern s32 BtlAct_GetEvasionAttack(BtlActChr *chr);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern f32 BtlCharApi_GetGroundY(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern s32 BtlCharApi_IsInTechnique(s32 objId);
extern u32 BtlParam_GetFlags(BtlActChr *chr);
extern u32 BtlParam_GetFlags2(BtlActChr *chr);                       /* obj->unk91C->unk14 */
extern u32 BtlParam_GetFlags3(BtlActChr *chr);                       /* obj->unk91C->unk18 */
extern s32 BtlParam_GetUnk2(BtlActChr *chr);
extern s32 BtlParam_GetKiRegenLimit(BtlActChr *chr);
extern s32 BtlParam_GetGaugeB(BtlActChr *chr);
extern s32 BtlParam_GetKiRegenRate(BtlActChr *chr);
extern s32 BtlParam_GetKiRecoverRate(BtlActChr *chr);
extern s32 BtlParam_GetBlastGainRate(BtlActChr *chr);
extern s32 BtlParam_GetMaxPowerDrain(BtlActChr *chr);
extern f32 BtlParam_GetGauge99CTime(BtlActChr *chr);
extern s32 BtlParam_GetDashSound(BtlActChr *chr);
extern s32 BtlParam_GetChargeLoopSound(BtlActChr *chr);
extern s32 BtlParam_GetMaxPowerSound(BtlActChr *chr);
extern f32 BtlMoveParam_GetSpeed(BtlActChr *chr, s32 arg);
extern f32 BtlStage_GetTop(void);
extern f32 BtlStage_GetBottom(void);
extern s32 BtlObjAnim_QueryEvent(void *obj, u64 a, s32 b, s32 c);
extern void BtlObj_SetColorMode(void *obj, s32 a, s32 b);
extern void BtlObj_AddPush(void *obj, Vec4 *v, f32 arg);
extern void BtlObj_AddSway(void *obj, f32 a, f32 b);

extern BtlActRoster *gBtlChars;
extern BtlActHandler gBtlActHandlers[BTLACT_COUNT];

/* Requests the action to switch to at the next dispatch (-1 is ignored). */
void BtlAct_Request(BtlActChr *chr, s32 id) {
    if (id != -1) {
        chr->act.request = id;
    }
}

/* Puts an action in a queue slot and empties the slots after it. */
void BtlAct_SetQueue(BtlActChr *chr, u32 slot, s32 id) {
    s32 i;

    if (slot < 4 && id != -1) {
        chr->act.queue[slot] = id;
        for (i = slot + 1; i < 4; i++) {
            chr->act.queue[i] = -1;
        }
    }
}

/* Empties the action queue. */
void BtlAct_ClearQueue(BtlActChr *chr) {
    s32 i;

    for (i = 0; i < 4; i++) {
        chr->act.queue[i] = -1;
    }
}

/* Whether an action is queued. */
s32 BtlAct_HasQueued(BtlActChr *chr) {
    return chr->act.queue[0] != -1;
}

/* Returns the current action id. */
s32 BtlAct_GetCurrent(BtlActChr *chr) {
    return chr->act.current;
}

/* Returns the requested action (-1 = none). */
s32 BtlAct_GetRequested(BtlActChr *chr) {
    return chr->act.request;
}

/* Returns chr + 0x950, a field nothing stores to. */
s32 BtlAct_GetPrev(BtlActChr *chr) {
    return chr->act.prev;
}

/* Returns the first queued action (-1 = none). */
s32 BtlAct_GetQueued(BtlActChr *chr) {
    return chr->act.queue[0];
}

/* Drops the first queued action. */
void BtlAct_PopQueue(BtlActChr *chr) {
    s32 i;

    for (i = 0; i < 3; i++) {
        chr->act.queue[i] = chr->act.queue[i + 1];
    }
    chr->act.queue[3] = -1;
}

/* Whether an action id is a hit reaction (0x105..0x132). */
s32 BtlAct_IsTechniqueId(s32 id) {
    if ((u32)(id - 0x106) < 0x2D) {
        return 1;
    }
    return id == 0x105;
}

/* Class of an action id: 0/1 for 0xFD..0x102, 2..4 for 0x106..0x13B, 4 for 0x105, else -1. */
s32 BtlAct_GetIdClass(BtlActChr *chr, s32 id) {
    if (id == 0x105) {
        return 4;
    }
    if ((u32)(id - 0xFD) < 6) {
        return (id - 0xFD) % 2;
    }
    if ((u32)(id - 0x106) < 0x36) {
        return (id - 0x106) % 3 + 2;
    }
    return -1;
}

/* Class of the current action. */
s32 BtlAct_GetCurrentClass(BtlActChr *chr) {
    return BtlAct_GetIdClass(chr, BtlAct_GetCurrent(chr));
}

/* Class of chr + 0x950. */
s32 BtlAct_GetPrevClass(BtlActChr *chr) {
    return BtlAct_GetIdClass(chr, BtlAct_GetPrev(chr));
}

/* Position 0..4 of a motion inside one of six runs of five motions; 0 for any other motion. */
s32 BtlAct_GetMotionLevel(BtlActChr *chr, s32 motion) {
    if ((u32)(motion - 0x11C) < 5) {
        return motion - 0x11C;
    }
    if ((u32)(motion - 0x13E) < 5) {
        return motion - 0x13E;
    }
    if ((u32)(motion - 0x160) < 5) {
        return motion - 0x160;
    }
    if ((u32)(motion - 0x121) < 5) {
        return motion - 0x121;
    }
    if ((u32)(motion - 0x143) < 5) {
        return motion - 0x143;
    }
    if ((u32)(motion - 0x165) < 5) {
        return motion - 0x165;
    }
    return 0;
}

/* Returns pose + 0xB4 (ground height below the fighter). */
f32 BtlAct_GetGroundY(BtlActChr *chr) {
    return BtlChar_GetPos(chr)->groundY;
}

/* Height above the ground (+Y is down). */
f32 BtlAct_GetHeight(BtlActChr *chr) {
    f32 ground = BtlAct_GetGroundY(chr);

    return ground - BtlChar_GetPos(chr)->pos.y;
}

/* Height as a fraction of the room between the ground (or the stage floor) and the stage ceiling. */
f32 BtlAct_GetHeightRatio(BtlActChr *chr) {
    f32 half;
    f32 top;
    f32 bottom;
    f32 ground;

    BtlActPose *pose;
    f32 y;

    half = BtlCharApi_GetHeight(chr->objId);
    top = BtlStage_GetTop() + half * 0.5f;
    bottom = BtlStage_GetBottom() + half * 0.5f;
    ground = BtlAct_GetGroundY(chr);
    pose = BtlChar_GetPos(chr);
    y = pose->pos.y;
    if (bottom < ground) {
        ground = bottom;
    }
    return (ground - y) / (ground - top);
}

/* Height divided by pose + 0x44: frames until the ground is reached (0 with flag 0xF, 1e6 when not falling). */
f32 BtlAct_GetFramesToGround(BtlActChr *chr) {
    f32 fall;
    f32 height;

    if (BtlChar_TestFlag(chr, 0xF)) {
        return 0.0f;
    }
    fall = BtlChar_GetPos(chr)->unk44;
    if (fall < 0.001f) {
        return 1000000.0f;
    }
    height = BtlAct_GetHeight(chr);
    if (height < 0.0f) {
        return 0.0f;
    }
    return height / fall;
}

/* Yaw of the fighter relative to its camera yaw. */
f32 BtlAct_GetRotYRelCam(BtlActChr *chr) {
    return BtlUtil_WrapAngle(BtlChar_GetPos(chr)->rot.y - chr->camYaw);
}

/* Facing of the fighter relative to its camera yaw. */
f32 BtlAct_GetFacingRelCam(BtlActChr *chr) {
    return BtlUtil_WrapAngle(BtlChar_GetPos(chr)->facing - chr->camYaw);
}

/* Direction of travel relative to the camera yaw. */
f32 BtlAct_GetVelDirRelCam(BtlActChr *chr) {
    BtlActPose *pose = BtlChar_GetPos(chr);

    return BtlUtil_WrapAngle(atan2f(pose->vel.x, pose->vel.z) - chr->camYaw);
}

/* In lock-on, within range of the opponent: scales the speed down by distance unless moving along the line to it. */
f32 BtlAct_ScaleSpeedByApproach(BtlActChr *chr, f32 speed, f32 range) {
    Vec4 to;
    Vec4 dir;
    f32 dist;
    f32 along;

    if (!BtlChar_TestFlag(chr, 5)) {
        return speed;
    }
    BtlOpp_GetDelta(chr, &to);
    to.y = 0.0f;
    dist = Vec3_Length(&to);
    if (dist < 0.01f) {
        dist = 0.01f;
    }
    if (dist < range) {
        Vec3_Normalize(&dir, &BtlChar_GetPos(chr)->vel);
        dir.y = 0.0f;
        along = __builtin_fabsf(Vec3_Dot(&dir, &to) / dist);
        speed = speed * along + speed * (1.0f - along) * (dist / range);
    }
    return speed;
}

/* In lock-on: limits the speed so that `frames` frames of it do not exceed range * distance to the opponent. */
f32 BtlAct_ScaleSpeedByDist(BtlActChr *chr, f32 speed, f32 range, f32 frames) {
    f32 limit;
    f32 travel;
    f32 scale;

    if (!BtlChar_TestFlag(chr, 5)) {
        return speed;
    }
    travel = speed * frames;
    limit = range * BtlOpp_GetDistanceXZ(chr);
    scale = 1.0f;
    if (limit < travel && 0.01f < travel) {
        scale = limit / travel;
    }
    return speed * scale;
}

/* Blends in an up or down motion by the pitch toward the opponent (lock-on only); recalc measures the pitch again. */
void BtlAct_SetPitchMotion(BtlActChr *chr, s32 motionUp, s32 motionDown, s32 recalc) {
    Vec4 own;
    Vec4 other;
    Vec4 dir;
    f32 len;
    f32 t;

    if (!BtlChar_TestFlag(chr, 5)) {
        chr->pitch = 0.0f;
        return;
    }
    if (recalc) {
        BtlCharApi_GetNodePos(chr->objId, 0x11, &own);
        BtlOpp_GetNodePos(chr, 0x11, &other);
        Vec4_Sub(&dir, &other, &own);
        len = Vec3_Length(&dir);
        if (0.0001f < len) {
            Vec4_Scale(&dir, &dir, 1.0f / len);
        }
        dir.y = BtlUtil_ClampF(dir.y, -1.0f, 1.0f);
        chr->pitch = -Mathf_Asin(dir.y);
    }
    t = chr->pitch / 0.78539816f;
    if (t < -1.0f) {
        t = -1.0f;
    }
    if (1.0f < t) {
        t = 1.0f;
    }
    if (t < 0.0f) {
        BtlAnim_SetUnkC8C(chr, -t);
        BtlAnim_PlaySub(chr, motionDown);
    } else {
        BtlAnim_SetUnkC8C(chr, t);
        BtlAnim_PlaySub(chr, motionUp);
    }
}

/* Flag 0x35 -> 1, flag 0x36 -> 0, else the saved value (when asked and valid) or motion flag 0x8000. */
s32 BtlAct_IsAirMotion(BtlActChr *chr, s32 useSaved) {
    if (BtlChar_TestFlag(chr, 0x35)) {
        return 1;
    }
    if (BtlChar_TestFlag(chr, 0x36)) {
        return 0;
    }
    if (useSaved && chr->unkFB0 != 1) {
        return chr->unkFBC;
    }
    if (BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x8000) {
        return 1;
    }
    return 0;
}

/* Picks the member to switch to and records its chara, costume and gauge word. */
void BtlAct_PrepareSwitch(BtlActChr *chr) {
    chr->switchMember = BtlMember_GetSwitchTarget(chr);
    chr->switchChara = BtlMember_Get(chr, chr->switchMember)->chara;
    chr->switchCostume = BtlMember_Get(chr, chr->switchMember)->costume;
    chr->switchUnk20 = BtlMember_GetGauge(chr, chr->switchMember)->unk20;
    if (BtlMember_GetGauge(chr, chr->switchMember)->unk30 != 0) {
        chr->unkCE8 = 0x56;
        chr->unkCEC = 0x56;
        chr->unkCF0 = 0x56;
    } else {
        chr->unkCE8 = chr->switchChara;
        chr->unkCEC = chr->switchChara;
        chr->unkCF0 = chr->switchChara;
    }
}

/* Gives the fighter's object a horizontal push of `speed` along a yaw. */
void BtlAct_PushAngle(BtlActChr *chr, f32 angle, f32 speed, f32 arg) {
    Vec4 v;

    v.x = Mathf_Sin(angle) * speed;
    v.y = 0.0f;
    v.z = Mathf_Cos(angle) * speed;
    v.w = 0.0f;
    BtlObj_AddPush(BtlChar_GetObj(chr), &v, arg);
}

/* Gives the fighter's object a push of `speed` along a direction (ignored when the direction is null). */
void BtlAct_PushDir(BtlActChr *chr, Vec4 *dir, f32 speed, f32 arg) {
    Vec4 v;
    f32 len;

    len = Vec3_Length(dir);
    if (!(len < 0.0001f)) {
        Vec4_Scale(&v, dir, 1.0f / len);
        Vec4_Scale(&v, &v, speed);
        BtlObj_AddPush(BtlChar_GetObj(chr), &v, arg);
    }
}

/* Forwards two floats to BtlObj_AddSway on the fighter's object. */
void BtlAct_SetObjUnk(BtlActChr *chr, f32 a, f32 b) {
    BtlObj_AddSway(BtlChar_GetObj(chr), a, b);
}

/* Whether an action id is one of the table-driven attacks (0x70..0xAD). */
s32 BtlAct_IsAttackId(s32 id) {
    return (u32)(id - 0x70) < 0x3E;
}

/* Whether the skill bit an attack record needs is set in the object's skill masks (no bit needed = yes). */
static inline BtlActAttackRec *BtlAct_GetAttackRec(s32 attack) {
    return &gBtlChars->attackTbl[attack];
}

static inline s32 BtlAct_TestSkillLo(BtlActChr *chr, s32 bit) {
    return BtlParam_GetFlags2(chr) & (1 << bit);
}

static inline s32 BtlAct_TestSkillHi(BtlActChr *chr, s32 bit) {
    return BtlParam_GetFlags3(chr) & (1 << bit);
}

s32 BtlAct_TestAttackSkill(BtlActChr *chr, s32 attack) {
    s32 bit = BtlAct_GetAttackRec(attack)->needSkill;

    if (bit >= 0) {
        if ((u32)bit >= 0x20) {
            if (BtlAct_TestSkillHi(chr, bit - 0x20)) {
                return 1;
            }
        } else if (BtlAct_TestSkillLo(chr, bit)) {
            return 1;
        }
        return 0;
    }
    return 1;
}

/* Builds the pending attack parameters from the attack table record of an attack action (0x70..0xAD). */
void BtlAct_PrepareAttack(BtlActChr *chr, s32 id) {
    BtlActAttackRec *rec;
    s32 i;
    f32 v;

    if (BtlAct_IsAttackId(id)) {
        rec = BtlAct_GetAttackRec(id - 0x70);
        if (rec->parts >= 2) {
            chr->nextAttack.motion = rec->motion;
            chr->nextAttack.motion2 = rec->motion + 2;
        } else {
            chr->nextAttack.motion = rec->motion;
            chr->nextAttack.motion2 = -1;
        }
        chr->nextAttack.parts = BtlUtil_Max(rec->parts, 1);
        chr->nextAttack.flags = rec->flags;
        chr->nextAttack.unk1C = rec->unk0 * 10.0f * 0.0092592593f;
        chr->nextAttack.unk9 = rec->unk1;
        if (rec->unkC > 0) {
            v = rec->unkC * 0.1f;
        } else {
            v = 1.0f;
        }
        chr->nextAttack.unk10 = v;
        chr->nextAttack.unk14 = rec->unk8;
        chr->nextAttack.unk18 = 0.0f;
        chr->nextAttack.unk20 = 0;
        /* Camera cut of the attack. With record flag 0x20 the choice depends on which side of the
           fighter its camera sits (camSide < 0, since unk18 was just zeroed): the one place the
           camera pose is read by fighter logic. Otherwise it alternates with the frame counter. */
        if (rec->cutAlt < 0) {
            chr->nextAttack.cut = rec->cut;
        } else {
            if (rec->flags & 0x20) {
                if (chr->camSide < chr->nextAttack.unk18) {
                    chr->nextAttack.cut = rec->cut;
                } else {
                    chr->nextAttack.cut = rec->cutAlt;
                }
            } else {
                chr->nextAttack.cut = BtlChar_FrameMod(2) ? rec->cut : rec->cutAlt;
            }
        }
        if (BtlChar_TestFlag(chr, 6) && !BtlChar_TestMemberUnk70(chr)) {
            for (i = 0; i < 2; i++) {
                if (BtlAct_TestAttackSkill(chr, rec->followAlt[i])) {
                    chr->nextAttack.follow[i] = rec->followAlt[i];
                } else {
                    chr->nextAttack.follow[i] = rec->follow[i];
                }
            }
            chr->attackId = id;
        } else {
            for (i = 0; i < 2; i++) {
                chr->nextAttack.follow[i] = rec->follow[i];
            }
            chr->attackId = id;
        }
    }
}

/* Resets the pending attack parameters to "no attack". */
void BtlAct_ResetAttack(BtlActChr *chr) {
    s32 i;
    chr->nextAttack.motion = -1;
    chr->nextAttack.motion2 = -1;
    chr->nextAttack.parts = 0;
    chr->nextAttack.flags = 0;
    chr->nextAttack.unk1C = 0.0f;
    chr->nextAttack.unk9 = 0;
    chr->nextAttack.unk10 = 1.0f;
    chr->nextAttack.unk14 = 1.0f;
    chr->nextAttack.unk18 = 15.0f;
    chr->nextAttack.unk20 = 0;
    chr->nextAttack.cut = -1;
    for (i = 0; i < 2; i++) {
        chr->nextAttack.follow[i] = -1;
    }
}

/* Makes the pending attack parameters current (called by the attack handlers when they start). */
void BtlAct_LatchAttack(BtlActChr *chr) {
    chr->attack = chr->nextAttack;
}

/* In the powered-up mode (flag 6): whether any of the given skill bits is set in the object's second mask. */
s32 BtlAct_TestPoweredSkill(BtlActChr *chr, u32 mask) {
    if (BtlChar_TestFlag(chr, 6)) {
        return (BtlParam_GetFlags3(chr) & mask) != 0;
    }
    return 0;
}

/* Fills the form-change request with the active member's own chara and costume. */
void BtlAct_SetFormCurrent(BtlActChr *chr) {
    chr->form.unk0 = -1;
    chr->form.chara = BtlMember_GetActive(chr)->chara;
    chr->form.costume = BtlMember_GetActive(chr)->costume;
    chr->form.unk14 = 1;
    chr->form.unkC = 0;
    chr->form.unk10 = 0;
    chr->form.unk24 = -1;
    chr->form.unk2C = -1;
    chr->form.unk28 = 0;
    chr->form.unk18 = -1;
    chr->form.unk1C = -1;
    chr->form.unk20 = -1;
}

/* Fills the form-change request with one of six fixed {chara, costume} pairs, picked by the frame counter. */
void BtlAct_SetFormRandom(BtlActChr *chr) {
    s32 forms[6][2] = { { 1, 0 }, { 0xD, 2 }, { 0x19, 1 }, { 0x1D, 1 }, { 0x18, 0 }, { 0x59, 0 } };
    s32 i = BtlChar_FrameMod(6);

    chr->form.chara = forms[i][0];
    chr->form.costume = forms[i][1];
    chr->form.unk0 = -1;
    chr->form.unkC = 0;
    chr->form.unk10 = 0;
    chr->form.unk24 = -1;
    chr->form.unk2C = -1;
    chr->form.unk28 = 0;
    chr->form.unk14 = 0;
    chr->form.unk18 = -1;
    chr->form.unk1C = -1;
    chr->form.unk20 = -1;
}

/* Counts one at chr + 0xE50 and sets the opponent's + 0xD4C. */
void BtlAct_CountAndMarkOpponent(BtlActChr *chr) {
    chr->unkE50++;
    BtlChar_Get(BtlOpp_GetPlayer(chr))->unkD4C = 1;
}

/* Ends the state held by flag 0x98: clears it, resets the object's sub-state and calls BtlStat_EndKind4. */
void BtlAct_EndFlag98(BtlActChr *chr) {
    void *obj;

    if (BtlChar_TestFlag(chr, 0x98)) {
        obj = BtlChar_GetObj(chr);
        BtlChar_ClearFlag(chr, 0x98);
        BtlObj_SetColorMode(obj, 2, 0);
        BtlStat_EndKind4(chr);
    }
}

/* With flag 0x94: applies the two floats saved at chr + 0xFD0 through BtlMove_SetHeading. */
void BtlAct_ApplyFlag94(BtlActChr *chr) {
    if (BtlChar_TestFlag(chr, 0x94)) {
        BtlMove_SetHeading(chr, chr->unkFD0, chr->unkFD4);
    }
}

/*
 * From here on the code calls the small accessors above as if they were not visible to the compiler
 * (delay slots are filled differently around a call to a function already compiled in the same
 * file). That is evidence of a source file boundary somewhere between BtlAct_GetQueued and
 * BtlAct_CheckForced; until the files are split, the calls go through aliased declarations.
 */
extern s32 BtlAct_GetCurrent_(BtlActChr *chr) __asm__("BtlAct_GetCurrent");
extern s32 BtlAct_GetRequested_(BtlActChr *chr) __asm__("BtlAct_GetRequested");
extern s32 BtlAct_GetPrev_(BtlActChr *chr) __asm__("BtlAct_GetPrev");
extern s32 BtlAct_GetQueued_(BtlActChr *chr) __asm__("BtlAct_GetQueued");
extern void BtlAct_PopQueue_(BtlActChr *chr) __asm__("BtlAct_PopQueue");
extern s32 BtlAct_IsTechniqueId_(s32 id) __asm__("BtlAct_IsTechniqueId");
extern s32 BtlAct_GetCurrentClass_(BtlActChr *chr) __asm__("BtlAct_GetCurrentClass");
extern f32 BtlAct_GetHeight_(BtlActChr *chr) __asm__("BtlAct_GetHeight");
/* BtlAct_Action04 ends on a tail call to BtlChar_SetFlag, which an int function only gets from `return f();`. */
extern s32 BtlChar_SetFlagRet(BtlActChr *chr, s32 bit) __asm__("BtlChar_SetFlag");

/*
 * Forced actions: turns fighter flags raised elsewhere (hits, grabs, scripted events) into action
 * requests. Runs every frame after the current handler's decide phase unless the handler set
 * holdAction on the previous frame. Later tests overwrite earlier requests, so the order below is
 * the priority order, lowest first.
 */
void BtlAct_CheckForced(BtlActChr *chr) {
    BtlActState *act = &chr->act;

    if (chr->holdAction != 0) {
        return;
    }
    if (act->current == 0) {
        BtlAct_Request(chr, 0xB);
    }
    if (BtlChar_IsFree(chr)) {
        if (BtlChar_TestFlag(chr, 0x74)) {
            BtlAct_Request(chr, 0x21);
        }
        if (BtlChar_TestFlag(chr, 0x75)) {
            BtlAct_Request(chr, 0x22);
        }
        if (BtlChar_TestFlag(chr, 0x76)) {
            BtlAct_Request(chr, 0x20);
        }
        if (BtlChar_TestFlag(chr, 0x77)) {
            BtlDecide_QueueAttack(chr, BtlChar_IsStage4Or27() ? 0x7A : 0x79);
            BtlAct_Request(chr, BtlAct_GetQueued_(chr));
        }
    }
    if (BtlChar_TestFlag(chr, 0x7C)) {
        BtlDecide_QueueAttack(chr, BtlAct_GetEvasionAttack(chr));
        BtlAct_Request(chr, BtlAct_GetQueued_(chr));
    }
    if (BtlChar_TestFlag(chr, 0x6D)) {
        BtlAct_Request(chr, 0xC9);
    }
    if (BtlChar_TestFlag(chr, 0x6F)) {
        BtlAct_Request(chr, 0xCA);
    }
    if (BtlChar_TestFlag(chr, 0x68) || BtlChar_TestFlag(chr, 0x69) || BtlChar_TestFlag(chr, 0x6A)) {
        BtlAct_Request(chr, 0x38);
    }
    if (BtlChar_TestFlag(chr, 0x7B)) {
        BtlAct_Request(chr, 0x39);
    }
    if (BtlChar_TestFlag(chr, 0x6B)) {
        if (BtlParam_GetFlags(chr) & 2) {
            BtlAct_Request(chr, 0x3D);
        } else {
            BtlAct_Request(chr, 0x3B);
        }
    }
    if (BtlChar_TestFlag(chr, 0x79)) {
        BtlAct_Request(chr, 0xBD);
    }
    if (BtlAct_QueueReaction(chr, chr->unkFB0)) {
        BtlAct_Request(chr, BtlAct_GetQueued_(chr));
    }
    if (BtlChar_TestFlag(chr, 0x7F)) {
        BtlAct_Request(chr, 0xBE);
    }
    if (BtlChar_TestFlag(chr, 0x61)) {
        BtlAct_Request(chr, 0xFA);
    }
    if (BtlChar_TestFlag(chr, 0x62)) {
        BtlAct_Request(chr, 0xFB);
    }
    if (BtlChar_TestFlag(chr, 0xAA)) {
        BtlAct_Request(chr, BtlAct_GetCurrentClass_(chr) + 0x12E);
    }
    if (BtlChar_TestFlag(chr, 0xAB)) {
        BtlChar_ClearFlag(chr, 0xAB);
        BtlAct_Request(chr, 0x104);
    }
    if (BtlChar_TestFlag(chr, 0x7D)) {
        BtlAct_Request(chr, 0x41);
    }
    if (BtlChar_TestFlag(chr, 0x7E)) {
        BtlAct_Request(chr, 0x45);
    }
    if (BtlChar_TestFlag(chr, 0x139)) {
        BtlChar_ClearFlag(chr, 0x139);
        BtlAct_Request(chr, 0xB);
    }
    if (BtlChar_TestFlag(chr, 0x13A)) {
        BtlChar_ClearFlag(chr, 0x13A);
        BtlAct_Request(chr, 0xD8);
    }
    if (BtlChar_TestFlag(chr, 0xEF)) {
        BtlAct_Request(chr, 1);
    }
    if (BtlChar_TestFlag(chr, 0xF1)) {
        BtlAct_Request(chr, 2);
    }
    if (BtlChar_TestFlag(chr, 0xF2)) {
        BtlAct_Request(chr, 3);
    }
    if (BtlAct_CheckStoryForced(chr)) {
        BtlAct_Request(chr, BtlAct_GetQueued_(chr));
    }
}

/* What BtlChar_GetObj returns, as far as this file reads it. */
typedef struct BtlActObj {
    /* 0x000 */ u8 unk0[0xA40];
    /* 0xA40 */ u32 flags;
} BtlActObj;

/*
 * Gauges, once per frame after the action's run phase. Nothing happens while time is stopped,
 * outside the fight state (flag 3), during hit-stop (flags 0x125 / 0x126) or under flag 0x128.
 * Uses the motion flags of the current motion: 0x10000 = the motion charges, 0x20000 = second charge.
 */
void BtlAct_UpdateGauges(BtlActChr *chr) {
    s32 tick = 0;
    u32 mflags = BtlAnim_GetFlags(BtlAnim_GetId(chr));
    s32 amount;
    s32 floor;
    s32 i;
    BtlActMember *member;
    BtlActVitals *v;
    s32 cap;

    if (BtlChars_IsTimeStopped()) {
        return;
    }
    if (!BtlChar_TestFlag(chr, 3)) {
        return;
    }
    if (BtlChar_TestFlag(chr, 0x125)) {
        return;
    }
    if (BtlChar_TestFlag(chr, 0x126)) {
        return;
    }
    if (BtlChar_TestFlag(chr, 0x128)) {
        return;
    }
    if (!BtlChar_TestFlag(chr, 0xBB)) {
        amount = BtlParam_GetMaxPowerDrain(chr);
        if (BtlChar_TestFlag(chr, 0xBC)) {
            amount *= 3;
        } else if (BtlChar_TestFlag(chr, 0xBD)) {
            amount *= 2;
        } else if (BtlMember_HasAbility(chr, 0x42)) {
            amount *= 2;
        } else if (BtlMember_HasAbility(chr, 0x41)) {
            amount = amount * 2 / 3;
        }
        BtlMember_SubMaxPower(chr, amount);
        if (BtlMember_IsMaxPowerEmpty(chr)) {
            BtlChar_ClearFlag(chr, 6);
            BtlStat_EndKind3(chr);
        }
    }
    if (BtlChar_TestFlag(chr, 0x98) && !BtlChar_TestFlag(chr, 0x99) && !BtlCharApi_IsInTechnique(chr->objId)) {
        BtlMember_SpendKi(chr, chr->unkE10, 0);
        if (BtlMember_IsKiEmpty(chr)) {
            BtlAct_EndFlag98(chr);
        }
    }
    if (BtlChar_IsDead(chr)) {
        return;
    }
    if (++chr->secTimer >= 30) {
        chr->secTimer = 0;
        tick = 1;
    }
    if (mflags & 0x10000) {
        if (BtlChar_TestFlag(chr, 0xBE)) {
            amount = BtlParam_GetKiRecoverRate(chr);
            BtlChar_SetFlag(chr, 0xE7);
            if (BtlInput_IsPressed(chr, 0x100000)) {
                amount *= 4;
            }
            BtlMember_AddKi(chr, amount);
            if (BtlMember_HasKi(chr, BtlParam_GetGaugeB(chr))) {
                BtlChar_ClearFlag(chr, 0xBE);
            }
        } else if (!BtlChar_TestFlag(chr, 0x98)) {
            if (!BtlMember_HasKi(chr, BtlParam_GetKiRegenLimit(chr))) {
                BtlMember_AddKi(chr, BtlParam_GetKiRegenRate(chr));
            }
            if (Battle_GetMode() == 5 || Battle_GetMode() == 6) {
                BtlMember_AddKi(chr, 1000);
            }
            if (tick) {
                if (BtlMember_HasAbility(chr, 0x78)) {
                    BtlMember_AddKi(chr, 20000);
                } else if (BtlMember_HasAbility(chr, 0x1F)) {
                    BtlMember_AddKi(chr, 900);
                } else if (BtlMember_HasAbility(chr, 0x1E)) {
                    BtlMember_AddKi(chr, 600);
                }
                if (BtlMember_HasAbility(chr, 0x57)) {
                    BtlMember_AddKi(chr, 300);
                }
                if (BtlChar_TestFlag(chr, 0x11) && BtlMember_HasAbility(chr, 0x5C)) {
                    BtlMember_AddKi(chr, 900);
                }
            }
        }
    }
    if (mflags & 0x20000) {
        BtlMember_AddBlast(chr, BtlParam_GetBlastGainRate(chr));
        if (Battle_GetMode() == 5 || Battle_GetMode() == 6) {
            BtlMember_AddBlast(chr, 0xD05);
        }
    }
    BtlStat_Update(chr);
    BtlSkill_UpdateTimers(chr);
    chr->unkD80 -= 400;
    floor = BtlAct_TestPoweredSkill(chr, 1) ? 50000 : 0;
    if (chr->unkE20 > 0) {
        if (floor < chr->unkE20) {
            floor = chr->unkE20;
        }
    }
    if (chr->unkD80 < floor) {
        chr->unkD80 = floor;
    }
    if (BattleSide_GetUnk200(chr->player)) {
        chr->unk99C += (s32)(100000.0f / (BtlParam_GetGauge99CTime(chr) * 30.0f));
        if (Battle_GetMode() == 5 || Battle_GetMode() == 6) {
            chr->unk99C += 0x457;
        }
        if (chr->unk99C > 100000) {
            chr->unk99C = 100000;
        }
    }
    if (Battle_GetMode() == 5 || Battle_GetMode() == 6) {
        if (!BtlChar_TestFlag(chr, 0x95)) {
            BtlMember_AddHealth(chr, 200);
        }
    }
    if (tick) {
        if (BtlMember_HasAbility(chr, 0x1D)) {
            BtlMember_AddHealth(chr, 100);
        } else if (BtlMember_HasAbility(chr, 0x1C)) {
            BtlMember_AddHealth(chr, 50);
        }
        if (BtlChar_TestFlag(chr, 0x11) && BtlMember_HasAbility(chr, 0x5C)) {
            BtlMember_AddHealth(chr, 100);
        }
        /* Members waiting on the bench recover once per second. */
        for (i = 0; i < chr->memberCount; i++) {
            if (i != chr->active) {
                member = BtlMember_Get(chr, i);
                v = &member->vitals;
                if (member->present) {
                    if (v->hp > 0) {
                        cap = (v->hp + 9999) / 10000 * 10000;
                        v->hp = BtlUtil_Min(v->hp + 100, cap);
                        if (BtlMember_HasAbilityOf(chr, i, 0x5F)) {
                            v->hp = BtlUtil_Min(v->hp + 200, cap);
                        }
                        v->gaugeB = BtlUtil_Min(v->gaugeB + 1000, v->gaugeBMax);
                        if (BtlMember_HasAbilityOf(chr, i, 0x60)) {
                            v->gaugeB = BtlUtil_Min(v->gaugeB + 2000, v->gaugeBMax);
                        }
                    }
                }
            }
        }
    }
}

/*
 * Timers and bookkeeping that run every frame after the gauges: flag-held state that lapses when
 * its flag is gone, stun and status countdowns, the last held direction, the action frame counter.
 * Everything after the first block is skipped while time is stopped.
 */
void BtlAct_UpdateTimers(BtlActChr *chr) {
    BtlActObj *obj = BtlChar_GetObj(chr);
    u32 mflags = BtlAnim_GetFlags(BtlAnim_GetId(chr));
    BtlActPose *pose;
    s32 i;
    s32 dir;
    s32 kind;

    BtlMove_ApplyImpulse(chr);
    BtlMove_UpdateHoverOffset(chr);
    BtlMove_UpdateDefenseTimers(chr);
    if (BtlChar_TestFlag(chr, 0x8B)) {
        Vec4_Add(&BtlChar_GetPos(chr)->pos, &BtlChar_GetPos(chr)->pos, &chr->unkF30);
    }
    if (BtlChars_IsTimeStopped()) {
        return;
    }
    if (!BtlChar_TestFlag(chr, 0x82)) {
        BtlChar_ClearFlag(chr, 0x8A);
    }
    if (!BtlChar_TestFlag(chr, 0x88)) {
        chr->unkD60 = 0;
        if (!(mflags & 2)) {
            BtlChar_ClearFlag(chr, 0x86);
        }
    }
    if (!BtlChar_TestFlag(chr, 0x89)) {
        chr->unkD68 = 0;
        chr->unkD70 = 0;
    }
    if (!BtlChar_TestFlag(chr, 0x95)) {
        chr->unkD40 = 0;
        chr->unkD44 = 0;
        if (chr->unkD48 > 0) {
            chr->unkD48--;
        }
        if (BtlChar_TestPrevFlag(chr, 0x95)) {
            s32 j;

            for (j = 0; j < 0x32; j++) {
                chr->unk1004[j] = 0;
            }
        }
    }
    for (i = 0; i < 0x32; i++) {
        if (chr->unk1036[i] < 100) {
            chr->unk1036[i]++;
        }
    }
    /* Stun: counts down only in a motion with flag 0x10000; mashing (input bit 20) takes 3 more per press. */
    if (chr->unkFE0 > 0) {
        BtlChar_SetFxBit(chr, 0x1A);
        if (mflags & 0x10000) {
            chr->unkFE0--;
            BtlChar_SetSmallVibration(chr, 0.1f);
            BtlChar_SetFlag(chr, 0xE7);
            if (BtlInput_IsPressed(chr, 0x100000)) {
                chr->unkFE0 -= 3;
                if (chr->unkFE0 < 0) {
                    chr->unkFE0 = 0;
                }
            }
        }
        if (BtlChar_IsDead(chr)) {
            chr->unkFE0 = 0;
        }
    }
    if (chr->unkFF8 > 0) {
        chr->unkFF8--;
        BtlChar_SetFlag(chr, 0x93);
        BtlChar_SetFlag(chr, 0x137);
        BtlChar_SetFlag(chr, 0x96);
    }
    chr->unkFFC = BtlUtil_Approach(chr->unkFFC, chr->unkFF8 * 2, 15);
    if (chr->unkFE8 > 0) {
        chr->unkFE8--;
        if (BtlChar_IsDead(chr)) {
            chr->unkFE8 = 0;
        }
    }
    if (chr->unkFE4 > 0) {
        chr->unkFE4--;
        BtlChar_SetFlag(chr, 0x96);
    }
    if (chr->unkDE4 > 0) {
        if (!(mflags & 4)) {
            chr->unkDE4--;
        }
    } else {
        chr->unkDE0 = 0;
    }
    if (!BtlCharApi_IsInTechnique(chr->objId)) {
        if (chr->unkE40 > 0) {
            chr->unkE40--;
        }
    }
    if (chr->unkFF0 > 0) {
        chr->unkFF0--;
        if (!BtlChar_TestFlag(chr, 0x13)) {
            chr->unkFF0 = 0;
        }
    }
    pose = BtlChar_GetPos(chr);
    switch (BtlAct_GetCurrent_(chr)) {
        case 0xF:
        case 0x3E:
        case 0x58:
        case 0xB0:
        case 0xB1:
            pose->unkAC = BtlUtil_MaxF(pose->unkAC, 1.0f);
            break;
        default:
            pose->unkAC = 1.0f;
            break;
    }
    if (!(mflags & 0x810)) {
        BtlChar_ClearFlag(chr, 0x1D);
        BtlChar_ClearFlag(chr, 0x1E);
    }
    /* Member switch on the right stick (input bits 25 / 26) with three or more members. */
    if (BattleSide_GetUnk200(chr->player) && Battle_GetMode() != 1 && !BtlChar_TestFlag(chr, 0x134) &&
        BtlMember_CountAlive(chr) >= 3) {
        if (BtlInput_IsPressed(chr, 0x2000000)) {
            BtlMember_PrevSwitchTarget(chr);
            BtlChar_SetFlag(chr, 0xE4);
        }
        if (BtlInput_IsPressed(chr, 0x4000000)) {
            BtlMember_NextSwitchTarget(chr);
            BtlChar_SetFlag(chr, 0xE5);
        }
    }
    switch (BtlAct_GetCurrent_(chr)) {
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
            if (BtlChar_TestFlag(chr, 8)) {
                chr->unkD88++;
            }
            break;
        case 0x2B:
        case 0x2D:
        case 0x2E:
            break;
        default:
            chr->unkD88 = 0;
            break;
    }
    kind = BtlOpp_GetParamByte2(chr);
    if (kind == 4) {
        if (BtlParam_GetUnk2(chr) == kind) {
            BtlAnim_SetObjRate(chr, BtlAnim_GetObjRate(chr) * 2.0f);
        } else {
            BtlAnim_SetObjRate(chr, BtlAnim_GetObjRate(chr) * 3.0f);
        }
    }
    if (!BtlAct_IsTechniqueId_(BtlAct_GetCurrent_(chr))) {
        dir = -1;
        if (!BtlInput_IsHeld(chr, 0x10000)) {
            if (BtlInput_IsHeld(chr, 0x40000)) {
                dir = 2;
            } else if (BtlInput_IsHeld(chr, 0x80000)) {
                dir = 3;
            } else if (BtlInput_IsHeld(chr, 0x20000)) {
                dir = 4;
            }
        }
        chr->unk1590 = dir;
    }
    if (BtlChar_TestFlag(chr, 6) && BtlMember_HasAbility(chr, 0x42)) {
        BtlChar_SetFlag(chr, 0x57);
    }
    if (BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x800) {
        if (BtlMember_HasAbility(chr, 0x39)) {
            BtlChar_SetFlag(chr, 0x57);
        }
        if (BtlMember_GetActiveGauge(chr)->unk28 == 0) {
            chr->unk1000 = 0;
        } else if (!(BtlAnim_GetFlags(BtlAnim_GetPrevId(chr)) & 0x800)) {
            chr->unk1000 = 4;
        }
    } else {
        chr->unk1000 = 0;
    }
    if (BtlChar_IsDead(chr) && (obj->flags & 0x40000)) {
        BtlObj_SetColorMode(obj, 2, 0);
    }
    if (Battle_GetMode() == 1 && chr->unk1580 > 0) {
        chr->unk1580--;
        BtlChar_SetFlag(chr, 0x11E);
    }
    { extern s32 TagBalance_AgeStep(s32); chr->actionFrame+=TagBalance_AgeStep(chr->player); }
}

/*
 * The action dispatcher: one call per fighter per frame (first pass of the fighter update).
 *   stage 4:  decide phase of the current action, then the forced-action check;
 *             if an action was requested: leave the old one, switch, reset the per-action state,
 *             pop the queue, enter the new one; otherwise three motion-layer steps;
 *   stage 5:  run phase of the (possibly new) action, then gauges and timers.
 */
void BtlAct_Update(BtlActChr *chr) {
    BtlActState *act = &chr->act;
    BtlActHandler *tbl = gBtlActHandlers;

    BtlChar_SetStage(chr, 4);
    if (tbl[act->current] != NULL) {
        tbl[act->current](chr, BTLACT_PHASE_DECIDE);
    }
    {
        extern s32 TagAssist_PendingAction(s32);
        s32 pending=TagAssist_PendingAction(chr->player);
        if(pending>=0) BtlAct_Request(chr,pending);
    }
    BtlAct_CheckForced(chr);
#ifdef PORT
    /* The original duel ends immediately on ring-out. A team fight can keep
       going, so put the eliminated body down and reject recovery/attack
       requests while its teammates finish the round. Let the native falling,
       landing impact and lying-down chain finish; blocking DC/DB leaves a
       fighter looping the falling animation even after ground collision. */
    if (TagTeam_Active() && BtlChar_TestFlag(chr, 3) && BtlChar_TestFlag(chr, 7)) {
        BtlAct_ClearQueue(chr);
        if (act->current < 0xD8 || act->current > 0xDE) BtlAct_Request(chr, 0xD8);
        else if (act->request < 0xD8 || act->request > 0xDE) act->request = -1;
    }
#endif
    if (act->request != -1) {
        if (tbl[act->current] != NULL) {
            tbl[act->current](chr, BTLACT_PHASE_LEAVE);
        }
        BtlChar_ApplyRootYaw(chr);
        act->current = act->request;
        act->request = -1;
        memset(chr->work, 0, sizeof(chr->work));
        chr->actionFrame = 0;
        chr->unkD78 = 0;
        chr->unkD7C = 0;
        chr->unkD84 = 0;
        chr->unkDEC = 0;
        if (!BtlChar_TestFlag(chr, 0xDD)) {
            ChrCam_EndCut(chr);
        }
        BtlAct_PopQueue_(chr);
        BtlChar_ClearFlagRange(chr, 0x81, 0x84);
        BtlChar_ClearFlagRange(chr, 0x5B, 0x5D);
        BtlChar_ClearFlagRange(chr, 0xDD, 0xDD);
        chr->unkF48 = 0;
        BtlChar_SetFlag(chr, 8);
        BtlAct_ApplyFlag94(chr);
        if (tbl[act->current] != NULL) {
            tbl[act->current](chr, BTLACT_PHASE_ENTER);
        }
    } else {
        BtlAnim_FlushRequest(chr);
        BtlAnim_FlushRequestKeep(chr);
        BtlAnim_FlushSubToMain(chr);
        BtlChar_ApplyRootYaw(chr);
    }
    BtlChar_SetStage(chr, 5);
    chr->holdAction = 0;
    if (gBtlActHandlers[act->current] != NULL) {
        gBtlActHandlers[act->current](chr, BTLACT_PHASE_RUN);
    }
    BtlAct_UpdateGauges(chr);
    BtlAct_UpdateTimers(chr);
}

/* Action 1 (forced by flag 0xEF): plays motions 0x180 -> 0x181 in place; flag 0xF0 ends it (to action 0xB). */
s32 BtlAct_Action01(BtlActChr *chr, s32 phase) {
    void *obj;
    f32 frameA;
    f32 frameB;
    f32 frame;

    if (phase == BTLACT_PHASE_ENTER) {
        BtlAnim_Play(chr, 0x180, 0.0f);
    }
    if (phase == BTLACT_PHASE_RUN) {
        switch (BtlAnim_GetId(chr)) {
            case 0x180:
                BtlAnim_AdvanceThen(chr, 0x181, 0.0f, 0);
                break;
            case 0x181:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, 0.9259258f);
        BtlMove_ApplyGravity(chr);
        obj = BtlChar_GetObj(chr);
        frameA = BtlObjAnim_QueryEvent(obj, 0x200, 0, 0);
        frameB = BtlObjAnim_QueryEvent(obj, 0x400, 0, 0);
        frame = BtlAnim_GetFrame(chr);
        if (0.0f <= frameA && frameA < frame && (frameB < 0.0f || frame < frameB)) {
            BtlChar_SetFxBit(chr, 7);
            BtlCharSnd_PlayCommon(chr, BtlParam_GetChargeLoopSound(chr));
        }
        if (0.0f <= frameB && BtlAnim_PassedFrame(chr, frameB)) {
            BtlChar_SetFxBit(chr, 7);
            BtlChar_SetFxBit(chr, 0x41);
            BtlCharSnd_PlayCommon(chr, BtlParam_GetMaxPowerSound(chr));
        }
    }
    if (phase == BTLACT_PHASE_DECIDE) {
        if (BtlChar_TestFlag(chr, 0xF0)) {
            BtlAct_Request(chr, 0xB);
        }
    }
}

/* Action 2 (forced by flag 0xF1): motions 0x182 -> 0x183, hit-stop level 2 (flag 0x125) held every frame. */
s32 BtlAct_Action02(BtlActChr *chr, s32 phase) {
    BtlActObj *obj;

    if (phase == BTLACT_PHASE_ENTER) {
        obj = BtlChar_GetObj(chr);
        BtlAnim_Play(chr, 0x182, 0.15f);
        BtlChar_SetFlag(chr, 0xF3);
        BtlChar_SetFlag(chr, 0x24);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        chr->unkFE0 = 0;
        if (obj->flags & 0x40000) {
            BtlObj_SetColorMode(obj, 2, 0);
        }
    }
    if (phase == BTLACT_PHASE_RUN) {
        switch (BtlAnim_GetId(chr)) {
            case 0x182:
                BtlAnim_AdvanceThen(chr, 0x183, 0.0f, 0);
                break;
            case 0x183:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
        }
        BtlMove_Step(chr, 6, 6, 7, 0.0f, 0.9259258f);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 0x125);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x12A);
        BtlChar_SetFxBit(chr, 0x3B);
    }
}

/* Action 3 (forced by flag 0xF2): motion 0x184 with the same held flags as action 2. */
s32 BtlAct_Action03(BtlActChr *chr, s32 phase) {
    if (phase == BTLACT_PHASE_ENTER) {
        BtlAnim_Play(chr, 0x184, 0.15f);
        BtlChar_SetFlag(chr, 0xF3);
        BtlChar_SetFlag(chr, 0x24);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        chr->unkFF8 = 0;
        chr->unkFFC = 0;
    }
    if (phase == BTLACT_PHASE_RUN) {
        BtlAnim_Advance(chr, 0);
        BtlMove_Step(chr, 6, 6, 7, 0.0f, 0.9259258f);
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        BtlChar_SetFlag(chr, 0x125);
        BtlChar_SetFlag(chr, 0xBB);
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x12A);
        BtlChar_SetFxBit(chr, 0x14);
        BtlChar_SetFxBit(chr, 0x3B);
    }
}

/*
 * Action 4: a motion played on request (motion, blend and end mode at chr + 0x1550). It sets holdAction
 * every frame, so forced actions cannot interrupt it; flag 0xFA restarts it and flag 0xFB ends it.
 */
s32 BtlAct_Action04(BtlActChr *chr, s32 phase) {
    s32 motion;
    f32 blend;
    BtlActPose *pose;

    if (phase == BTLACT_PHASE_ENTER) {
        motion = chr->unk1550;
        blend = chr->unk1554;
        if (BtlAct_GetPrev_(chr) != 4) {
            blend = 0.0f;
        }
        BtlAnim_Play(chr, motion, blend);
        BtlChar_GetPos(chr)->unk98 = 0.0f;
        BtlChar_GetPos(chr)->unk9C = 0.0f;
        BtlChar_SetFlag(chr, 0x29);
        BtlAct_EndFlag98(chr);
        BtlChar_ClearFlag(chr, 0xBE);
        chr->unkFE0 = 0;
        BtlChar_ClearFlag(chr, 0xFA);
    }
    if (phase == BTLACT_PHASE_RUN) {
        switch (chr->unk1558) {
            case 0:
                BtlAnim_Advance(chr, 0);
                break;
            case 1:
                BtlAnim_AdvanceLoop(chr, 0);
                break;
        }
        Vec4_SetZero(&BtlChar_GetPos(chr)->unk70);
        chr->holdAction = 1;
        BtlChar_SetFlag(chr, 0x3F);
        BtlChar_SetFlag(chr, 0x128);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        if (BtlChar_TestFlag(chr, 0xFE)) {
            BtlChar_SetFxBit(chr, 0x13);
        }
        if (BtlChar_TestFlag(chr, 0xFF)) {
            BtlChar_SetFxBit(chr, 0x14);
        }
        if (BtlChar_TestFlag(chr, 0x100)) {
            BtlChar_SetFlag(chr, 0x30);
        }
        if (BtlChar_TestFlag(chr, 0x101)) {
            BtlChar_SetFxBit(chr, 7);
            BtlCharSnd_PlayCommon(chr, BtlParam_GetChargeLoopSound(chr));
        }
        if (BtlChar_TestFlag(chr, 0x102)) {
            BtlChar_ClearFlag(chr, 0x102);
            BtlChar_ClearFlag(chr, 0x101);
            BtlChar_SetFxBit(chr, 7);
            BtlChar_SetFxBit(chr, 8);
            BtlCharSnd_PlayCommon(chr, BtlParam_GetMaxPowerSound(chr));
        }
        if (5.0f < BtlAct_GetHeight_(chr)) {
            BtlChar_SetFxBit(chr, 0x3A);
        } else {
            BtlChar_SetFxBit(chr, 0x3B);
        }
        chr->unk1580 = 3;
    }
    if (phase == BTLACT_PHASE_DECIDE) {
        if (BtlChar_TestFlag(chr, 0xFA)) {
            BtlAct_Request(chr, 4);
        }
        if (BtlChar_TestFlag(chr, 0xFB)) {
            if (BtlAnim_GetFlags(chr->unk1550) & 0x800) {
                BtlAct_Request(chr, 0xD8);
            } else {
                BtlAct_Request(chr, 0xB);
            }
            if (BtlAct_CheckStoryForced(chr)) {
                BtlAct_Request(chr, BtlAct_GetQueued_(chr));
            }
        }
    }
    if (phase == BTLACT_PHASE_LEAVE) {
        if (BtlAct_GetRequested_(chr) != 4) {
            if (BtlAct_GetHeight_(chr) < 5.0f) {
                BtlChar_SetHeldFlag(chr, 0xF);
                pose = BtlChar_GetPos(chr);
                pose->pos.y = BtlCharApi_GetGroundY(chr->objId);
                BtlChar_ClearFlag(chr, 0xE);
            } else {
                BtlChar_SetHeldFlag(chr, 0xE);
            }
            BtlChar_ClearFlag(chr, 0xFE);
            BtlChar_ClearFlag(chr, 0xFF);
            BtlChar_ClearFlag(chr, 0x101);
            BtlChar_ClearFlag(chr, 0x102);
            BtlChar_ClearFlag(chr, 0x100);
            return BtlChar_SetFlagRet(chr, 0x37);
        }
    }
}

/* Actions 5 and 6: motions 0x18 -> 0x19 (moving) -> 0x1A (stopping, then action 0xB); flag 0x47 or 0x48 by action. */
s32 BtlAct_Action05(BtlActChr *chr, s32 phase) {
    f32 rate;
    s32 moving;
    s32 flag;

    if (phase == BTLACT_PHASE_ENTER) {
        BtlAnim_Play(chr, 0x18, 0.15f);
        BtlChar_PlayVoice(chr, 0x24);
        BtlChar_SetHeldFlag(chr, 0xE);
    }
    if (phase == BTLACT_PHASE_RUN) {
        rate = 0.0f;
        moving = 1;
        flag = 1;
        switch (BtlAnim_GetId(chr)) {
            case 0x18:
                BtlAnim_AdvanceThen(chr, 0x19, 0.2f, 0);
                rate = BtlAnim_GetProgress(chr);
                moving = 0;
                break;
            case 0x19:
                BtlAnim_AdvanceLoop(chr, 0);
                if (BtlAnim_IsNew(chr)) {
                    BtlCharSnd_PlayCommon(chr, BtlParam_GetDashSound(chr));
                    BtlChar_SetFlag(chr, 0xD1);
                    if (BtlAct_GetHeight_(chr) < 20.0f) {
                        BtlChar_SetFxBit(chr, 0x35);
                    }
                }
                rate = 1.0f;
                moving = 1;
                break;
            case 0x1A:
                if (BtlAnim_Advance(chr, 0)) {
                    BtlAct_Request(chr, 0xB);
                    BtlChar_GetPos(chr)->unk98 = rate;
                }
                moving = 0;
                rate = 1.0f - BtlAnim_GetProgress(chr);
                flag = 0;
                break;
        }
        if (moving) {
            BtlMove_Step(chr, 2, 1, 3, BtlMoveParam_GetSpeed(chr, 6), 10000.0f);
            BtlMove_ApplyGravity(chr);
            BtlChar_SetFlag(chr, 9);
            BtlChar_SetFlag(chr, 0x50);
            BtlChar_SetFlag(chr, 0x51);
            BtlChar_SetFxBit(chr, 4);
            BtlChar_SetFxBit(chr, 9);
            BtlChar_SetFxBit(chr, 0x3A);
        } else {
            BtlMove_Step(chr, flag ? 2 : 6, 6, 7, 0.0f, 0.9259258f);
            BtlMove_ApplyGravity(chr);
        }
        BtlMove_SetLeanX(chr, BtlChar_GetPos(chr)->speed * rate);
        BtlChar_SetFlag(chr, 0x42);
        BtlChar_SetFlag(chr, 0x43);
        BtlChar_SetFlag(chr, 0x44);
        BtlChar_SetFlag(chr, 0x45);
        BtlChar_SetFlag(chr, 0x46);
        switch (BtlAct_GetCurrent_(chr)) {
            case 5:
                BtlChar_SetFlag(chr, 0x47);
                break;
            case 6:
                BtlChar_SetFlag(chr, 0x48);
                break;
        }
    }
}
