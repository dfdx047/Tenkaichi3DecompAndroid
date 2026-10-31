#include "common.h"
#include "battle/btl_char_move.h"
#ifdef PORT
#include "battle/tag_revive.h"
#endif

/* Fighter movement, 0x1DC9A0-0x1E0290 (see battle/btl_char_move.h for the model and the structures).
 *
 * Called from: BtlChar_UpdateMotion (BtlMove_UpdateAction), BtlChar_UpdateStage7 (BtlMove_PushOut,
 * BtlMove_ApplyOrbit), BtlChar_UpdateStage8 (BtlMove_ClampToStage), BtlChar_UpdateLate (BtlMove_UpdateAnimVoice),
 * BtlAct_UpdateTimers (BtlMove_ApplyImpulse, BtlMove_UpdateHoverOffset, BtlMove_UpdateDefenseTimers); everything
 * else is called by the action handlers (0x1E2... onwards).
 */

/* Speeds are written in km/h and converted to metres per frame; angles in degrees. Both macros reproduce the
   original constants bit for bit (see the header). */
#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)
#define BTL_PITCH_MAX (1.5707963f - 0.01f)
#define BTL_IMPULSE_DECEL (BTL_KMH(2000.0f) * (1.0f / 30.0f))

extern f32 atan2f(f32 y, f32 x);
extern f32 sqrtf(f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec4_SetZero(Vec4 *dst);
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);
extern void Vec4_RotateEuler(Vec4 *dst, Vec4 *angles, Vec4 *src);
extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
extern f32 Mathf_Asin(f32 a);
extern f32 Mathf_Acos(f32 a);

extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_LengthXZ(Vec4 *v);
extern f32 BtlUtil_ApproachF(f32 cur, f32 goal, f32 step);
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
extern f32 BtlUtil_MinF(f32 a, f32 b);
extern f32 BtlUtil_MaxF(f32 a, f32 b);

extern s32 BtlChar_TestFlag(BtlMoveChr *chr, s32 flag);
extern void BtlChar_SetFlag(BtlMoveChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(BtlMoveChr *chr, s32 flag);
extern void BtlChar_ClearFlag(BtlMoveChr *chr, s32 flag);
extern BtlMovePose *BtlChar_GetPos(BtlMoveChr *chr);
extern BtlMoveObj *BtlChar_GetObj(BtlMoveChr *chr);
extern void BtlChar_PlayVoice(BtlMoveChr *chr, s32 kind);
extern s32 BtlInput_IsHeld(BtlMoveChr *chr, u32 mask);

extern s32 BtlAnim_TestAttr(BtlMoveChr *chr, u64 mask);
extern void BtlCharSnd_PlayStream(BtlMoveChr *chr, s32 id);
extern void BtlAnim_SetUnkC8C(BtlMoveChr *chr, f32 v);
extern void BtlAnim_SetStep(BtlMoveChr *chr, f32 v);
extern void BtlAnim_ApplyBlend(BtlMoveChr *chr);
extern void BtlAct_Update(BtlMoveChr *chr);
extern BtlMoveBlastList *EftHit_GetList(void);
extern s32 BtlParam_GetCount80(BtlMoveChr *chr);
extern s32 BtlParam_GetUnk82(BtlMoveChr *chr);
extern f32 BtlInput_GetStickX(BtlMoveChr *chr);
extern f32 BtlInput_GetStickY(BtlMoveChr *chr);
extern f32 BtlOpp_GetYaw(BtlMoveChr *chr);
extern void BtlOpp_GetTargetRot(BtlMoveChr *chr, Vec4 *out);
extern void BtlOpp_GetDelta(BtlMoveChr *chr, Vec4 *out);
extern void BtlOpp_GetPoseVec30(BtlMoveChr *chr, Vec4 *out);
extern f32 BtlOpp_GetHalfHeightDiff(BtlMoveChr *chr);
extern f32 BtlOpp_GetRadius(BtlMoveChr *chr);
extern f32 BtlCharApi_GetRadius(s32 objId);
extern void BtlOpp_GetTargetPos(BtlMoveChr *chr, Vec4 *out);
extern void BtlOpp_GetVelocity(BtlMoveChr *chr, Vec4 *out);
extern void BtlOpp_GetObjVecFA0(BtlMoveChr *chr, Vec4 *out);
extern void BtlCharApi_GetBodyPos(s32 objId, Vec4 *out);
extern void BtlChar_SetUnk1310(BtlMoveChr *chr, Vec4 *v);
extern f32 BtlStage_GetInnerRadius(void);
extern f32 BtlStage_GetTop(void);
extern f32 BtlStage_GetBottom(void);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern f32 BtlOpp_GetHeight(BtlMoveChr *chr);
extern void BtlChar_GetSnapPos(BtlMoveChr *chr, Vec4 *out, s32 slot);
extern void BtlOpp_GetSnapPos(BtlMoveChr *chr, Vec4 *out, s32 slot);
extern void BtlChar_GetSnapDelta(BtlMoveChr *chr, Vec4 *out, s32 a, s32 b);
extern s32 BtlChars_IsTimeStopped(void);
extern s32 BtlInput_TestAction(BtlMoveChr *chr, s32 id, s32 arg);
extern f32 BtlMember_GetHealthRatio(BtlMoveChr *chr);
extern s32 BtlMember_HasAbility(BtlMoveChr *chr, s32 ability);
extern s32 BtlParam_GetFlags2(BtlMoveChr *chr);
extern s32 BtlChar_IsDead(BtlMoveChr *chr);
extern s32 BtlChar_IsFree(BtlMoveChr *chr);
extern s32 BtlChar_IsStage4Or27(void);
extern void ColSeg_Set(BtlMoveSeg *seg, Vec4 *a, Vec4 *b);
extern s32 StgCol_TraceSegment(BtlMoveSeg *seg);
extern void StgCol_GetHitPos(Vec4 *out);

/* Late per-frame voice triggers: with flags 0x12F..0x133 the current animation's attribute bits pick a voice kind
   (0xE, 0x34..0x37) or a stream (0x8D35, 0x8D37, 0x8D41, 0x8D42). Called from BtlChar_UpdateLate. */
void BtlMove_UpdateAnimVoice(BtlMoveChr *chr) {
    s32 kind = -1;

    if (BtlChar_TestFlag(chr, 0x12F)) {
        if (BtlAnim_TestAttr(chr, 0x8000)) {
            kind = 0xE;
        }
    }
    if (BtlChar_TestFlag(chr, 0x130)) {
        if (BtlAnim_TestAttr(chr, 0x8000)) {
            kind = 0x34;
        }
        if (BtlAnim_TestAttr(chr, 0x8000000000)) {
            kind = 0x35;
        }
        if (BtlAnim_TestAttr(chr, 0x20000)) {
            kind = 0x36;
        }
        if (BtlAnim_TestAttr(chr, 0x40000)) {
            BtlCharSnd_PlayStream(chr, 0x8D35);
        }
    }
    if (BtlChar_TestFlag(chr, 0x131)) {
        if (BtlAnim_TestAttr(chr, 0x8000008000)) {
            kind = 0x37;
        }
        if (BtlAnim_TestAttr(chr, 0x40000)) {
            BtlCharSnd_PlayStream(chr, 0x8D37);
        }
    }
    if (BtlChar_TestFlag(chr, 0x132)) {
        if (BtlAnim_TestAttr(chr, 0x10000)) {
            BtlCharSnd_PlayStream(chr, 0x8D42);
        }
    }
    if (BtlChar_TestFlag(chr, 0x133)) {
        if (BtlAnim_TestAttr(chr, 0x10000)) {
            BtlCharSnd_PlayStream(chr, 0x8D41);
        }
    }
    if (kind >= 0) {
        BtlChar_PlayVoice(chr, kind);
    }
}

/* Resets the object's animation rates before the action handlers run: obj+0xC8C = 0, step (obj+0xC80) = 2. */
void BtlMove_ResetAnimStep(BtlMoveChr *chr) {
    BtlAnim_SetUnkC8C(chr, 0.0f);
    BtlAnim_SetStep(chr, 2.0f);
}

/* First call of a fighter's main update (BtlChar_UpdateMotion): runs the action state machine between the
   animation rate reset and the blend update. */
void BtlMove_UpdateAction(BtlMoveChr *chr) {
    BtlMove_ResetAnimStep(chr);
    BtlAct_Update(chr);
    BtlAnim_ApplyBlend(chr);
}

/* Tests the two stage contacts of this frame: the ground under the fighter (flag 0xF, normal at pose+0xC0) and the
   object's second contact (flag 0x1F without 0x20, normal at work+0x18070). floor = 1 keeps normals within 45 degrees
   of straight up (n.y <= -0.707), floor = 0 keeps the others. Returns 1 when one is kept and the direction of travel
   points into it by more than cos(angle) (angle = -2 pi: any direction). */
s32 BtlMove_TestContact(BtlMoveChr *chr, s32 floor, f32 angle) {
    Vec4 n;
    BtlMoveObj *obj = BtlChar_GetObj(chr);
    BtlMovePose *pose = BtlChar_GetPos(chr);
    BtlMoveContact *hit = (BtlMoveContact *)(obj->work + 0x18060);
    s32 i;

    for (i = 0; i < 2; i++) {
        s32 found = 0;
        f32 d;

        switch (i) {
        case 0:
            if (BtlChar_TestFlag(chr, 0xF)) {
                found = 1;
                Vec4_Copy(&n, &BtlChar_GetPos(chr)->unkC0);
            }
            break;
        case 1:
            if (BtlChar_TestFlag(chr, 0x1F) && !BtlChar_TestFlag(chr, 0x20)) {
                found = 1;
                Vec4_Copy(&n, &hit->normal);
            }
            break;
        }
        if (found) {
            if (n.y > -0.707f) {
                if (floor == 1) {
                    continue;
                }
            } else {
                if (floor == 0) {
                    continue;
                }
            }
            if (angle == -6.2831853f) {
                return 1;
            }
            d = -Vec3_Dot(&n, &pose->dir);
            if (Mathf_Cos(angle) < d) {
                return 1;
            }
        }
    }
    return 0;
}

/* The contact test for floor-like surfaces. */
s32 BtlMove_IsHeadingIntoFloor(BtlMoveChr *chr, f32 angle) {
    return BtlMove_TestContact(chr, 1, angle);
}

/* The contact test for wall-like surfaces. */
s32 BtlMove_IsHeadingIntoWall(BtlMoveChr *chr, f32 angle) {
    return BtlMove_TestContact(chr, 0, angle);
}

/* Counts the fighter's live blasts of one class (0: definition kinds 0, 4, 8; 1: the others), stores the count and
   returns 1 while it is below the character's limit for the class (BtlParam_GetCount80 / BtlParam_GetUnk82). */
s32 BtlMove_CanFireBlast(BtlMoveChr *chr, s32 mode, s32 *outCount) {
    s32 n = 0;
    BtlMoveBlastList *list = EftHit_GetList();
    s32 i;

    for (i = 0; i < list->count; i++) {
        BtlMoveBlastRec *rec = &list->rec[i];
        BtlMoveBlastDef *def;

        if (rec->type != 0) {
            continue;
        }
        def = rec->def;
        if (def == NULL) {
            continue;
        }
        if (def->ownerId != chr->objId) {
            continue;
        }
        if (rec->unk54 == 1) {
            continue;
        }
        switch (def->kind) {
        case 0:
        case 4:
        case 8:
            if (mode == 0) {
                n++;
            }
            break;
        default:
            if (mode == 1) {
                n++;
            }
            break;
        }
    }
    if (outCount != NULL) {
        *outCount = n;
    }
    switch (mode) {
    case 0:
        if (n < BtlParam_GetCount80(chr)) {
            return 1;
        }
        break;
    case 1:
        if (n < BtlParam_GetUnk82(chr)) {
            return 1;
        }
        break;
    }
    return 0;
}

/* Returns 1 when a live blast record of an enabled class (a: kinds 0/4/8, b: other kinds, c: type-1 records) is
   moving towards the fighter and would reach it within `limit` frames at its current speed. */
s32 BtlMove_IsBlastIncoming(BtlMoveChr *chr, s32 a, s32 b, s32 c, f32 limit) {
    Vec4 step;
    Vec4 d;
    BtlMoveBlastList *list = EftHit_GetList();
    s32 i;

    for (i = 0; i < list->count; i++) {
        BtlMoveBlastRec *rec = &list->rec[i];
        f32 dist;
        f32 speed;

        if (rec->unk54 == 1) {
            continue;
        }
        if (rec->type == 0) {
            if (rec->def == NULL) {
                continue;
            }
            switch (rec->def->kind) {
            case 0:
            case 4:
            case 8:
                if (!a) {
                    continue;
                }
                break;
            default:
                if (!b) {
                    continue;
                }
                break;
            }
        } else {
            if (rec->src == NULL) {
                continue;
            }
            switch (rec->src->unk4) {
            case 0:
            case 1:
                continue;
            default:
                if (!c) {
                    continue;
                }
                break;
            }
        }
        Vec4_Sub(&step, &rec->pos, &rec->prevPos);
        Vec4_Sub(&d, &BtlChar_GetPos(chr)->pos, &rec->pos);
        if (Vec3_Dot(&d, &step) < 0.0f) {
            continue;
        }
        dist = Vec3_Length(&d);
        speed = Vec3_Length(&step);
        if (speed < 0.001f) {
            continue;
        }
        if (limit < dist / speed) {
            continue;
        }
        return 1;
    }
    return 0;
}

/* Number of type-1 blast records whose source belongs to this fighter. */
s32 BtlMove_CountOwnBeams(BtlMoveChr *chr) {
    BtlMoveBlastList *list = EftHit_GetList();
    s32 n = 0;
    s32 i;

    for (i = 0; i < list->count; i++) {
        BtlMoveBlastRec *rec = &list->rec[i];

        if (rec->type == 1 && rec->src != NULL && rec->src->ownerId == chr->objId) {
            n++;
        }
    }
    return n;
}

/* Where the segment from -> to crosses the circle of the given radius around the stage centre (XZ plane, the
   later root): writes the point and returns 1, or returns 0 when it does not cross. */
s32 BtlMove_ClipToRadius(Vec4 *out, Vec4 *from, Vec4 *to, f32 radius) {
    Vec4 p;
    Vec4 q;
    Vec4 d;
    f32 a;
    f32 b;
    f32 c;
    f32 disc;
    f32 t;

    Vec4_Copy(&p, from);
    Vec4_Copy(&q, to);
    q.y = 0.0f;
    p.y = 0.0f;
    Vec4_Sub(&d, &q, &p);
    a = Vec3_Dot(&d, &d);
    b = 2.0f * Vec3_Dot(&p, &d);
    c = Vec3_Dot(&p, &p) - radius * radius;
    disc = b * b - a * 4.0f * c;
    if (disc < 0.0f) {
        return 0;
    }
    if (a < 0.001f) {
        return 0;
    }
    t = (sqrtf(disc) - b) / (a + a);
    if (t < 0.0f || t > 1.0f) {
        return 0;
    }
    Vec4_Lerp(out, to, from, t);
    return 1;
}

/* Turns the heading yaw towards a target by at most maxStep. Modes: 0 stick relative to the camera yaw,
   1 stick x as a turn rate, 2 / 4 towards the opponent, 3 away from it, 7 towards the stage centre, 8 away from the
   centre, 9 the opponent's own yaw, 5 / 6 keep. Also clears the yaw steering offset. */
void BtlMove_TurnYaw(BtlMoveChr *chr, s32 mode, f32 maxStep) {
    Vec4 rot;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 target = pose->yaw;
    f32 d;

    switch (mode) {
    case 0:
        if (BtlInput_IsHeld(chr, 0xF0)) {
            f32 x = BtlInput_GetStickX(chr);
            f32 y = BtlInput_GetStickY(chr);
            f32 cam = chr->camYaw;

            target = BtlUtil_WrapAngle(atan2f(x, -y) + cam);
        }
        break;
    case 1:
        target = BtlUtil_WrapAngle(pose->yaw + maxStep * BtlInput_GetStickX(chr));
        break;
    case 2:
    case 4:
        if (BtlChar_TestFlag(chr, 5)) {
            target = BtlOpp_GetYaw(chr);
        }
        break;
    case 3:
        if (BtlChar_TestFlag(chr, 5)) {
            target = BtlUtil_WrapAngle(BtlOpp_GetYaw(chr) + 3.14159265f);
        }
        break;
    case 9:
        BtlOpp_GetTargetRot(chr, &rot);
        target = rot.y;
        break;
    case 7:
        target = atan2f(-pose->pos.x, -pose->pos.z);
        break;
    case 8:
        target = atan2f(pose->pos.x, pose->pos.z);
        break;
    }
    d = BtlUtil_WrapAngle(target - pose->yaw);
    if (d < -maxStep) {
        d = -maxStep;
    }
    if (d > maxStep) {
        d = maxStep;
    }
    pose->yaw = BtlUtil_WrapAngle(pose->yaw + d);
    pose->unkD4 = 0.0f;
}

/* Turns the heading pitch towards a target by at most maxStep. Modes: 0 stick y as a rate (clamped to +-(pi/2 - 0.01)),
   1..4 at the opponent (2 scaled by the stick, 3 doubled height, 4 flattened under 50 m), with the height
   difference reduced when the two are about to meet; 5 level; 6 keep. Also clears the pitch steering offset. */
void BtlMove_TurnPitch(BtlMoveChr *chr, s32 mode, f32 maxStep) {
    Vec4 to;
    Vec4 vel;
    Vec4 oppVel;
    Vec4 n;
    Vec4 nOpp;
    Vec4 nTo;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 target = pose->pitch;
    f32 d;

    switch (mode) {
    case 0:
        target = BtlUtil_ClampF(pose->pitch - maxStep * BtlInput_GetStickY(chr), -BTL_PITCH_MAX, BTL_PITCH_MAX);
        break;
    case 5:
        target = 0.0f;
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        if (BtlChar_TestFlag(chr, 5)) {
            f32 eps;
            f32 speed;
            f32 oppSpeed;

            BtlOpp_GetDelta(chr, &to);
            to.y += BtlOpp_GetHalfHeightDiff(chr);
            if (mode == 4) {
                f32 len = Vec3_Length(&to);

                if (len < 50.0f) {
                    to.y *= len / 50.0f;
                }
            }
            if (mode == 3) {
                to.y += to.y;
            }
            eps = 0.01f;
            Vec4_Copy(&vel, &BtlChar_GetPos(chr)->unk30);
            BtlOpp_GetPoseVec30(chr, &oppVel);
            speed = Vec3_Length(&vel);
            oppSpeed = Vec3_Length(&oppVel);
            if (eps < speed && eps < oppSpeed) {
                f32 dist;
                f32 closing;

                Vec3_Normalize(&n, &vel);
                Vec3_Normalize(&nOpp, &oppVel);
                Vec3_Normalize(&nTo, &to);
                dist = Vec3_Length(&to);
                dist -= (1.0f - __builtin_fabsf(nTo.y)) * BtlCharApi_GetRadius(chr->objId);
                dist -= (1.0f - __builtin_fabsf(nTo.y)) * BtlOpp_GetRadius(chr);
                if (dist < 0.0f) {
                    dist = 0.0f;
                }
                closing = speed - oppSpeed * Vec3_Dot(&n, &nOpp);
                if (eps < closing) {
                    f32 t = dist / closing;

                    if (t < 1.0f) {
                        to.y *= t;
                    }
                }
            }
            Vec3_Normalize(&n, &to);
            if (n.y < -1.0f) {
                n.y = -1.0f;
            }
            if (n.y > 1.0f) {
                n.y = 1.0f;
            }
            target = -Mathf_Asin(n.y);
            if (mode == 2) {
                f32 k = -BtlInput_GetStickY(chr);

                if (k < 0.0f) {
                    k = 0.0f;
                }
                target *= k;
            }
        }
        break;
    case 6:
        break;
    }
    d = BtlUtil_WrapAngle(target - pose->pitch);
    if (d < -maxStep) {
        d = -maxStep;
    }
    if (d > maxStep) {
        d = maxStep;
    }
    pose->pitch = BtlUtil_WrapAngle(pose->pitch + d);
    pose->unkD8 = 0.0f;
}

/* Homing dash: heads for the opponent with a stick-controlled offset. The offset goal is (pi/2 * stick), scaled down
   when under 2 frames from contact, clamped to yawMax / pitchMax and approached by yawAccel / pitchAccel per frame.
   Without lock-on only clears the offsets. */
void BtlMove_SteerAtOpponent(BtlMoveChr *chr, f32 closeSpeed, f32 yawAccel, f32 pitchAccel, f32 yawMax, f32 pitchMax, f32 maxStep) {
    Vec4 to;
    Vec4 vel;
    Vec4 oppVel;
    Vec4 n;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 dist;
    f32 speed;
    f32 oppSpeed;
    f32 t;
    f32 k;
    f32 k2;
    f32 goal2;
    f32 cur;
    f32 diff;
    f32 goal;
    f32 base;
    f32 step;

    if (!BtlChar_TestFlag(chr, 5)) {
        BtlMove_TurnYaw(chr, 6, maxStep);
        BtlMove_TurnPitch(chr, 6, maxStep);
        return;
    }
    BtlOpp_GetDelta(chr, &to);
    to.y += BtlOpp_GetHalfHeightDiff(chr);
    dist = Vec3_Length(&to);
    dist -= BtlCharApi_GetRadius(chr->objId) * 0.5f;
    dist -= BtlOpp_GetRadius(chr) * 0.5f;
    if (dist < 0.0f) {
        dist = 0.0f;
    }
    Vec4_Copy(&vel, &pose->unk30);
    BtlOpp_GetPoseVec30(chr, &oppVel);
    speed = Vec3_Length(&vel);
    oppSpeed = Vec3_Length(&oppVel);
    if (0.001f < speed && 0.001f < oppSpeed) {
        closeSpeed -= oppSpeed * (Vec3_Dot(&vel, &oppVel) / (speed * oppSpeed));
    }
    t = 1000000.0f;
    if (0.001f < closeSpeed) {
        t = dist / closeSpeed;
    }
    if (2.0f < t) {
        k = 1.0f;
    } else {
        k = t * 0.5f;
    }
    goal = BtlUtil_ClampF(k * 1.5707963f * BtlInput_GetStickX(chr), -yawMax, yawMax);
    diff = goal - pose->unkD4;
    pose->unkD4 = BtlUtil_ApproachF(pose->unkD4, goal, BtlUtil_MinF(__builtin_fabsf(diff), yawAccel));
    goal = BtlUtil_WrapAngle(atan2f(to.x, to.z) + pose->unkD4);
    pose->yaw = BtlUtil_WrapAngle(pose->yaw + BtlUtil_ClampF(BtlUtil_WrapAngle(goal - pose->yaw), -maxStep, maxStep));
    if (2.0f < t) {
        k2 = 1.0f;
    } else {
        k2 = t * 0.5f;
    }
    Vec4_Copy(&n, &to);
    Vec3_Normalize(&n, &n);
    n.y = BtlUtil_ClampF(n.y, -1.0f, 1.0f);
    base = -Mathf_Asin(n.y);
    goal2 = BtlUtil_ClampF(-k2 * 1.5707963f * BtlInput_GetStickY(chr), -pitchMax, pitchMax);
    cur = pose->unkD8;
    diff = goal2 - cur;
    step = __builtin_fabsf(diff);
    if (pitchAccel < step) {
        step = pitchAccel;
    }
    pose->unkD8 = BtlUtil_ApproachF(cur, goal2, step);
    goal2 = BtlUtil_WrapAngle(base + pose->unkD8);
    pose->pitch = BtlUtil_WrapAngle(pose->pitch + BtlUtil_ClampF(BtlUtil_WrapAngle(goal2 - pose->pitch), -maxStep, maxStep));
    if (t < 1.0f) {
        pose->pitch = pose->pitch * t;
    }
}

/* Turns the model yaw (rot.y) towards the heading yaw: step = |difference| * rate, at least 1.8 degrees, at most maxStep. */
void BtlMove_TurnModelYaw(BtlMoveChr *chr, f32 maxStep, f32 rate) {
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 d = BtlUtil_WrapAngle(pose->yaw - pose->rot.y);
    f32 step = __builtin_fabsf(d) * rate;

    if (step < BTL_DEG(1.8f)) {
        step = BTL_DEG(1.8f);
    }
    if (maxStep < step) {
        step = maxStep;
    }
    if (d < -step) {
        pose->rot.y = BtlUtil_WrapAngle(pose->rot.y - step);
    } else if (step < d) {
        pose->rot.y = BtlUtil_WrapAngle(pose->rot.y + step);
    } else {
        pose->rot.y = pose->yaw;
    }
}

/* Sets heading yaw and pitch, and the model yaw, at once. */
void BtlMove_SetHeading(BtlMoveChr *chr, f32 yaw, f32 pitch) {
    BtlMovePose *pose = BtlChar_GetPos(chr);

    pose->yaw = yaw;
    pose->rot.y = yaw;
    pose->pitch = pitch;
}

/* Turns the heading towards a world point by at most yawStep / pitchStep and clears the steering offsets. */
void BtlMove_TurnToPoint(BtlMoveChr *chr, Vec4 *target, f32 yawStep, f32 pitchStep) {
    Vec4 d;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 eps;
    f32 ang;
    f32 pitch;

    Vec4_Sub(&d, target, &pose->pos);
    eps = 0.001f;
    if (eps < Vec3_Length(&d)) {
        Vec3_Normalize(&d, &d);
    }
    ang = 0.0f;
    if (eps < __builtin_fabsf(d.x) || eps < __builtin_fabsf(d.z)) {
        ang = atan2f(d.x, d.z);
    }
    pose->yaw = BtlUtil_WrapAngle(pose->yaw + BtlUtil_ClampF(BtlUtil_WrapAngle(ang - pose->yaw), -yawStep, yawStep));
    if (d.y < -1.0f) {
        d.y = -1.0f;
    }
    if (d.y > 1.0f) {
        d.y = 1.0f;
    }
    pitch = -Mathf_Asin(d.y);
    pose->pitch = BtlUtil_WrapAngle(pose->pitch + BtlUtil_ClampF(BtlUtil_WrapAngle(pitch - pose->pitch), -pitchStep, pitchStep));
    pose->unkD4 = 0.0f;
    pose->unkD8 = 0.0f;
}

/* Builds the unit direction of travel. Modes: 0 stick rotated by the camera yaw (horizontal), 1 the same tilted by
   the heading pitch, 2 heading yaw, 3 heading yaw and pitch, 4 / 5 the reverse of 2 / 3, 6 keep (horizontal part),
   7 keep. With no stick input modes 0 / 1 keep the old direction. */
void BtlMove_SetDirection(BtlMoveChr *chr, s32 mode) {
    Vec4 v;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    Vec4 *dir = &pose->dir;
    f32 c;
    f32 s;
    f32 cp;
    f32 sp;

    if (Vec3_Length(dir) < 0.001f) {
        dir->x = Mathf_Sin(pose->yaw);
        pose->dir.y = 0.0f;
        pose->dir.z = Mathf_Cos(pose->yaw);
    }
    switch (mode) {
    case 0:
    case 1:
        if (BtlInput_IsHeld(chr, 0xF0)) {
            f32 x = BtlInput_GetStickX(chr);
            f32 y = -BtlInput_GetStickY(chr);
            f32 a = -chr->camYaw;

            c = Mathf_Cos(a);
            s = Mathf_Sin(a);
            if (mode == 1) {
                cp = Mathf_Cos(pose->pitch);
                sp = Mathf_Sin(pose->pitch);
            } else {
                cp = 1.0f;
                sp = 0.0f;
            }
            v.x = cp * (x * c - y * s);
            v.y = -sp;
            v.z = cp * (x * s + y * c);
        } else {
            Vec4_Copy(&v, dir);
        }
        break;
    case 2:
        c = Mathf_Cos(pose->yaw);
        s = Mathf_Sin(pose->yaw);
        v.x = s;
        v.z = c;
        v.y = 0.0f;
        break;
    case 3:
        cp = Mathf_Cos(pose->pitch);
        sp = Mathf_Sin(pose->pitch);
        c = Mathf_Cos(pose->yaw);
        s = Mathf_Sin(pose->yaw);
        v.x = cp * s;
        v.y = -sp;
        v.z = cp * c;
        break;
    case 4:
        c = Mathf_Cos(pose->yaw);
        s = Mathf_Sin(pose->yaw);
        v.x = -s;
        v.y = 0.0f;
        v.z = -c;
        break;
    case 5:
        cp = Mathf_Cos(pose->pitch);
        sp = Mathf_Sin(pose->pitch);
        c = Mathf_Cos(pose->yaw);
        s = Mathf_Sin(pose->yaw);
        v.x = -cp * s;
        v.y = sp;
        v.z = -cp * c;
        break;
    case 6:
        v.x = pose->dir.x;
        v.y = 0.0f;
        v.z = pose->dir.z;
        break;
    case 7:
        v.x = pose->dir.x;
        v.y = pose->dir.y;
        v.z = pose->dir.z;
        break;
    }
    Vec3_Normalize(dir, &v);
    pose->dir.w = 0.0f;
}

/* The integration step: speed approaches the target by accel, then pos += dir * speed. */
void BtlMove_Advance(BtlMoveChr *chr, f32 speed, f32 accel) {
    Vec4 step;
    BtlMovePose *pose = BtlChar_GetPos(chr);

    pose->speed = BtlUtil_ApproachF(pose->speed, speed, accel);
    Vec4_Scale(&step, &pose->dir, pose->speed);
    Vec4_Add(&pose->pos, &pose->pos, &step);
    pose->pos.w = 1.0f;
}

/* One frame of ordinary movement, used by most action handlers: turn the heading instantly, turn the model
   (36 degrees per frame at most), rebuild the direction, advance. */
void BtlMove_Step(BtlMoveChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel) {
    BtlMove_TurnYaw(chr, yawMode, 3.14159265f);
    BtlMove_TurnPitch(chr, pitchMode, 3.14159265f);
    BtlMove_TurnModelYaw(chr, BTL_DEG(36.0f), 0.3f);
    BtlMove_SetDirection(chr, dirMode);
    BtlMove_Advance(chr, speed, accel);
}

/* Lock-on only: places the fighter on the opponent's path, `lead` frames ahead plus both radii, inside the stage radius. */
void BtlMove_WarpAheadOfOpponent(BtlMoveChr *chr, f32 lead) {
    Vec4 opp;
    Vec4 oppVel;
    Vec4 old;
    Vec4 ofs;
    Vec4 rot;
    BtlMovePose *pose = BtlChar_GetPos(chr);

    if (BtlChar_TestFlag(chr, 5)) {
        f32 min;
        f32 rad;
        f32 len;
        f32 lim;

        Vec4_Copy(&old, &pose->pos);
        min = BTL_KMH(500.0f);
        BtlOpp_GetTargetPos(chr, &opp);
        opp.y += BtlOpp_GetHalfHeightDiff(chr);
        BtlOpp_GetPoseVec30(chr, &oppVel);
        if (Vec3_Length(&oppVel) < min) {
            BtlOpp_GetTargetRot(chr, &rot);
            oppVel.x = -Mathf_Sin(rot.y) * min;
            oppVel.y = 0.0f;
            oppVel.z = -Mathf_Cos(rot.y) * min;
            oppVel.w = 0.0f;
        }
        Vec4_Scale(&ofs, &oppVel, lead);
        Vec4_Add(&opp, &opp, &ofs);
        Vec4_Copy(&ofs, &oppVel);
        rad = BtlCharApi_GetRadius(chr->objId) + BtlOpp_GetRadius(chr);
        len = BtlUtil_LengthXZ(&ofs);
        if (0.0001f < len) {
            len = rad / len;
            ofs.x *= len;
            ofs.z *= len;
        }
        Vec4_Add(&pose->pos, &opp, &ofs);
        rad = BtlChar_GetObj(chr)->unkFA0->radius;
        rad = BtlStage_GetInnerRadius() - rad;
        lim = BtlUtil_LengthXZ(&pose->pos);
        if (rad < lim) {
            Vec4_Scale(&pose->pos, &pose->pos, rad / lim);
        }
    }
}

/* Lock-on only: places the fighter behind the opponent (its next position, both radii plus gap away). */
void BtlMove_WarpBehindOpponent(BtlMoveChr *chr, f32 gap) {
    Vec4 opp;
    Vec4 rot;
    Vec4 vel;
    Vec4 ofs;

    if (BtlChar_TestFlag(chr, 5)) {
        f32 d;

        BtlOpp_GetTargetPos(chr, &opp);
        BtlOpp_GetTargetRot(chr, &rot);
        BtlOpp_GetPoseVec30(chr, &vel);
        Vec4_Add(&opp, &opp, &vel);
        d = BtlCharApi_GetRadius(chr->objId) + BtlOpp_GetRadius(chr);
        d += gap;
        ofs.x = -Mathf_Sin(rot.y) * d;
        ofs.y = 0.0f;
        ofs.z = -Mathf_Cos(rot.y) * d;
        ofs.w = 0.0f;
        Vec4_Add(&BtlChar_GetPos(chr)->pos, &opp, &ofs);
    }
}

/* Lock-on only: places the fighter in front of the opponent. */
void BtlMove_WarpInFrontOfOpponent(BtlMoveChr *chr, f32 gap) {
    Vec4 opp;
    Vec4 rot;
    Vec4 vel;
    Vec4 ofs;

    if (BtlChar_TestFlag(chr, 5)) {
        f32 d;

        BtlOpp_GetTargetPos(chr, &opp);
        BtlOpp_GetTargetRot(chr, &rot);
        BtlOpp_GetPoseVec30(chr, &vel);
        Vec4_Add(&opp, &opp, &vel);
        d = BtlCharApi_GetRadius(chr->objId) + BtlOpp_GetRadius(chr);
        d += gap;
        ofs.x = Mathf_Sin(rot.y) * d;
        ofs.y = 0.0f;
        ofs.z = Mathf_Cos(rot.y) * d;
        ofs.w = 0.0f;
        Vec4_Add(&BtlChar_GetPos(chr)->pos, &opp, &ofs);
    }
}

/* Moves the fighter so that its object's body point coincides with the opponent's, stops it and raises flags 0x3F
   (no push-out), 0x24 (no velocity this frame) and 0x55 with the point saved at chr+0x1310. */
void BtlMove_SnapToOpponent(BtlMoveChr *chr) {
    Vec4 own;
    Vec4 opp;
    Vec4 d;
    BtlMovePose *pose = BtlChar_GetPos(chr);

    BtlCharApi_GetBodyPos(chr->objId, &own);
    BtlOpp_GetObjVecFA0(chr, &opp);
    Vec4_Sub(&d, &opp, &own);
    Vec4_Add(&pose->pos, &pose->pos, &d);
    pose->speed = 0.0f;
    pose->fallSpeed = 0.0f;
    Vec4_SetZero(&pose->unk70);
    BtlChar_SetUnk1310(chr, &opp);
    BtlChar_SetFlag(chr, 0x3F);
    BtlChar_SetFlag(chr, 0x24);
}

/* Flies around the opponent on a 200 m circle towards the point opposite baseYaw (side 0 / 1 picks the way round),
   clipped to the stage radius. Returns 1 on the frame the target point is reached. */
s32 BtlMove_CircleOpponent(BtlMoveChr *chr, s32 side, s32 lead, f32 baseYaw, f32 speed) {
    Vec4 tgt;
    Vec4 opp;
    Vec4 d;
    Vec4 oppVel;
    Vec4 hit;
    Vec4 step;
    s32 ret = 0;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 radius;
    f32 diff;
    f32 scale;
    f32 t;
    f32 a;

    BtlOpp_GetTargetPos(chr, &opp);
    opp.y += BtlOpp_GetHalfHeightDiff(chr);
    radius = 200.0f;
    diff = 0.0f;
    Vec4_Sub(&d, &opp, &pose->pos);
    if (0.01f < __builtin_fabsf(d.x) || 0.01f < __builtin_fabsf(d.z)) {
        diff = BtlUtil_WrapAngle(baseYaw - atan2f(d.x, d.z));
    }
    diff = __builtin_fabsf(diff);
    scale = 1.0f;
    if (diff < 1.5707963f) {
        scale = 0.0f;
        if (BTL_DEG(18.0f) < diff) {
            scale = Mathf_Sin(diff - BTL_DEG(18.0f));
        }
    }
    t = radius * diff / speed;
    if (lead) {
        BtlOpp_GetVelocity(chr, &oppVel);
        if (BTL_KMH(10.0f) < BtlUtil_LengthXZ(&oppVel)) {
            f32 k = (__builtin_fabsf(BtlUtil_WrapAngle(baseYaw - atan2f(oppVel.x, oppVel.z))) - 1.5707963f) / 1.5707963f;

            if (0.0f < k) {
                Vec4_Scale(&oppVel, &oppVel, t * k);
                Vec4_Add(&opp, &opp, &oppVel);
            }
        }
    }
    if (side == 0) {
        diff = -diff;
    }
    a = BtlUtil_WrapAngle(diff * 0.5f + BtlUtil_WrapAngle(baseYaw + 3.14159265f));
    tgt.x = Mathf_Sin(a) * radius * scale;
    tgt.y = 0.0f;
    tgt.z = Mathf_Cos(a) * radius * scale;
    Vec4_Add(&tgt, &tgt, &opp);
    if (BtlMove_ClipToRadius(&hit, &opp, &tgt, BtlStage_GetInnerRadius())) {
        Vec4_Copy(&tgt, &hit);
    }
    BtlMove_TurnToPoint(chr, &tgt, 3.14159265f, 3.14159265f);
    BtlMove_TurnModelYaw(chr, 1.5707963f, 0.5f);
    BtlMove_SetDirection(chr, 3);
    pose->speed = speed;
    Vec4_Sub(&hit, &tgt, &pose->pos);
    a = BtlUtil_MaxF(Vec3_Dot(&hit, &pose->dir), 0.0f);
    if (a < pose->speed) {
        pose->speed = a;
        ret = 1;
    }
    Vec4_Scale(&step, &pose->dir, pose->speed);
    Vec4_Add(&pose->pos, &pose->pos, &step);
    pose->pos.w = 1.0f;
    return ret;
}

/* Computes where to go to attack the opponent from a given elevation angle: the opponent's predicted position
   (lead frames ahead, stopped by the stage, kept between the stage's top and bottom), and a point `dist` away
   from it on the fighter's side. */
void BtlMove_CalcApproachPoint(BtlMoveChr *chr, Vec4 *out, Vec4 *outTarget, f32 dist, f32 angle, f32 distScale, f32 lead) {
    Vec4 opp;
    Vec4 tgt;
    Vec4 vel;
    Vec4 d;
    BtlMoveSeg seg;
    Vec4 hit;
    Vec4 e;
    f32 scale;
    f32 oppScale;
    f32 top;
    f32 bottom;
    f32 len;

    BtlOpp_GetTargetPos(chr, &opp);
    BtlOpp_GetVelocity(chr, &vel);
    Vec4_Scale(&vel, &vel, lead);
    Vec4_Add(&tgt, &opp, &vel);
    scale = BtlCharApi_GetHeight(chr->objId);
    oppScale = BtlOpp_GetHeight(chr);
    tgt.y += BtlOpp_GetHalfHeightDiff(chr);
    ColSeg_Set(&seg, &opp, &tgt);
    if (StgCol_TraceSegment(&seg)) {
        f32 len3;
        f32 lenXZ;

        StgCol_GetHitPos(&hit);
        Vec4_Sub(&e, &tgt, &hit);
        len3 = Vec3_Length(&e);
        lenXZ = BtlUtil_LengthXZ(&e);
        if (0.001f < lenXZ) {
            lenXZ = len3 / lenXZ;
            tgt.x = hit.x + e.x * lenXZ;
            tgt.z = hit.z + e.z * lenXZ;
        }
        tgt.y = hit.y;
    }
    top = BtlStage_GetTop() + oppScale * 0.5f;
    if (tgt.y < top) {
        tgt.y = top;
    }
    bottom = BtlStage_GetBottom() + oppScale * 0.5f;
    if (bottom < tgt.y) {
        f32 len3;
        f32 lenXZ;

        Vec4_Sub(&hit, &tgt, &opp);
        len3 = Vec3_Length(&hit);
        lenXZ = BtlUtil_LengthXZ(&hit);
        if (0.001f < lenXZ) {
            f32 k = hit.y;

            if (0.001f < __builtin_fabsf(k)) {
                k = (tgt.y - bottom) / k;
                if (0.0f < k) {
                    f32 m = (lenXZ * (1.0f - k) + len3 * k) / lenXZ;

                    hit.x *= m;
                    hit.z *= m;
                    tgt.x = opp.x + hit.x;
                    tgt.z = opp.z + hit.z;
                }
            }
        }
        tgt.y = bottom;
    }
    if (outTarget != NULL) {
        Vec4_Copy(outTarget, &tgt);
    }
    Vec4_Sub(&d, &tgt, &BtlChar_GetPos(chr)->pos);
    d.y = 0.0f;
    len = Vec3_Length(&d);
    if (0.01f < len) {
        len = Mathf_Cos(angle) / len;
        Vec4_Set(&d, d.x * len, -Mathf_Sin(angle), d.z * len, 0.0f);
    } else {
        Vec4_Set(&d, 0.0f, -Mathf_Sin(angle), Mathf_Cos(angle), 0.0f);
    }
    if (out != NULL) {
        len = distScale * dist;
        len += BtlCharApi_GetRadius(chr->objId);
        Vec4_Scale(&d, &d, len + BtlOpp_GetRadius(chr));
        Vec4_Sub(out, &tgt, &d);
        top = BtlStage_GetTop() + scale * 0.5f;
        if (out->y < top) {
            f32 r;

            angle = BtlUtil_LengthXZ(&d);
            len = tgt.y;
            r = Vec3_Length(&d);
            len = top - len;
            len *= len;
            r = sqrtf(r * r - len);
            if (0.01f < angle) {
                len = r / angle;
                out->x = tgt.x - d.x * len;
                out->y = top;
                out->z = tgt.z - d.z * len;
            } else {
                out->x = tgt.x;
                out->y = top;
                out->z = tgt.z - r;
            }
        }
    }
}

/* Gravity: vertical speed += 50 km/h per frame up to 3000 km/h, then y += speed (+Y is down). */
void BtlMove_Fall(BtlMoveChr *chr) {
    BtlMovePose *pose = BtlChar_GetPos(chr);

    pose->fallSpeed += BTL_KMH(50.0f);
    if (BTL_KMH(3000.0f) < pose->fallSpeed) {
        pose->fallSpeed = BTL_KMH(3000.0f);
    }
    pose->pos.y += pose->fallSpeed;
}

/* Vertical speed approaches a target by accel, then y += speed. */
void BtlMove_MoveVertical(BtlMoveChr *chr, f32 speed, f32 accel) {
    BtlMovePose *pose = BtlChar_GetPos(chr);

    pose->fallSpeed = BtlUtil_ApproachF(pose->fallSpeed, speed, accel);
    pose->pos.y += pose->fallSpeed;
}

/* Brings the vertical speed to 0 by 100 km/h per frame. */
void BtlMove_BrakeVertical(BtlMoveChr *chr) {
    BtlMove_MoveVertical(chr, 0.0f, BTL_KMH(100.0f));
}

/* Vertical part of ordinary movement: in flight (flag 0xE) the vertical speed is braked, otherwise the fighter falls. */
void BtlMove_ApplyGravity(BtlMoveChr *chr) {
    if (BtlChar_TestFlag(chr, 0xE)) {
        BtlMove_BrakeVertical(chr);
    } else {
        BtlMove_Fall(chr);
    }
}

/* Sets the model's forward lean, clamped to +-(pi/2 - 0.01). */
void BtlMove_SetLeanX(BtlMoveChr *chr, f32 v) {
    BtlMovePose *pose = BtlChar_GetPos(chr);

    if (v < -BTL_PITCH_MAX) {
        v = -BTL_PITCH_MAX;
    }
    if (BTL_PITCH_MAX < v) {
        v = BTL_PITCH_MAX;
    }
    pose->unkA0 = v;
}

/* Sets the model's sideways lean. */
void BtlMove_SetLeanZ(BtlMoveChr *chr, f32 v) {
    BtlChar_GetPos(chr)->unkA4 = v;
}

/* 1 when the push-out moved the fighter this frame (flag 0x5E) and the two overlap in height (or flag 0x5F). */
s32 BtlMove_IsBlockedByOpponent(BtlMoveChr *chr) {
    Vec4 to;

    if (BtlChar_TestFlag(chr, 0x5E)) {
        if (BtlChar_TestFlag(chr, 0x5F)) {
            return 1;
        }
        BtlOpp_GetDelta(chr, &to);
        if (0.0f < to.y) {
            if (to.y < BtlOpp_GetHeight(chr) * 0.5f) {
                return 1;
            }
        } else {
            if (-to.y < BtlCharApi_GetHeight(chr->objId) * 0.5f) {
                return 1;
            }
        }
    }
    return 0;
}

/* Stage 8: keeps the position inside the stage's limit cylinder and raises the contact flags: 0x14 radius,
   0x15 top, 0x16 bottom, 0x17 the y = -10 limit of stages 4 / 27. */
void BtlMove_ClampToStage(BtlMoveChr *chr) {
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 r = BtlUtil_LengthXZ(&pose->pos);
    f32 rad = BtlChar_GetObj(chr)->unkFA0->radius;
    f32 lim = BtlStage_GetInnerRadius() - rad;

    if (lim < r) {
        f32 k = lim / r;

        pose->pos.x *= k;
        pose->pos.z *= k;
        BtlChar_SetHeldFlag(chr, 0x14);
    } else {
        BtlChar_ClearFlag(chr, 0x14);
    }
    if (!BtlChar_TestFlag(chr, 0x18)) {
        f32 top = BtlStage_GetTop();

        top += BtlCharApi_GetHeight(chr->objId) * 0.5f;
        if (pose->pos.y < top) {
            pose->pos.y = top;
            BtlChar_SetHeldFlag(chr, 0x15);
        } else {
            BtlChar_ClearFlag(chr, 0x15);
        }
    }
    if (!BtlChar_IsDead(chr)) {
        f32 bottom = BtlStage_GetBottom();

        bottom += BtlCharApi_GetHeight(chr->objId) * 0.5f;
        if (bottom < pose->pos.y) {
            pose->pos.y = bottom;
            BtlChar_SetHeldFlag(chr, 0x16);
        } else {
            BtlChar_ClearFlag(chr, 0x16);
        }
    }
#ifdef PORT
    TagRevive_CatchFallen(chr->player);
#endif
    BtlChar_ClearFlag(chr, 0x17);
    if (BtlChar_IsStage4Or27()) {
        if (BtlChar_IsFree(chr) && !BtlChar_TestFlag(chr, 0x2A) && !BtlChar_TestFlag(chr, 7) && -10.0f < pose->pos.y) {
            pose->pos.y = -10.0f;
            BtlChar_SetHeldFlag(chr, 0x17);
        }
        if (BtlChar_TestFlag(chr, 2)) {
            f32 half = 185.0f;

            pose->pos.x = BtlUtil_ClampF(pose->pos.x, -half, half);
            pose->pos.z = BtlUtil_ClampF(pose->pos.z, -half, half);
        }
    }
}

/* Lock-on only: asks for this frame's movement to be bent into an orbit (flag 0x19) between two distances. */
void BtlMove_RequestOrbit(BtlMoveChr *chr, f32 near, f32 far) {
    BtlMovePose *pose = BtlChar_GetPos(chr);

    if (BtlChar_TestFlag(chr, 5)) {
        BtlChar_SetFlag(chr, 0x19);
        pose->orbitNear = near;
        pose->orbitFar = far;
    }
}

/* Stage 7: with flag 0x19, removes the change of distance to the opponent from this frame's movement and turns
   the heading by the angle travelled, fully beyond `far`, not at all inside `near`. */
void BtlMove_ApplyOrbit(BtlMoveChr *chr) {
    Vec4 cur;
    Vec4 base;
    Vec4 opp;
    Vec4 dBase;
    Vec4 dCur;
    Vec4 e;
    Vec4 s;
    Vec4 g;
    Vec4 ang;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 near = pose->orbitNear;
    f32 far = pose->orbitFar;

    if (BtlChar_TestFlag(chr, 5) && BtlChar_TestFlag(chr, 0x19)) {
        f32 w;
        f32 lenBase;
        f32 len;
        f32 lenE;
        f32 c;
        f32 a;

        BtlChar_GetSnapPos(chr, &base, 0);
        w = 1.0f;
        Vec4_Copy(&cur, &pose->pos);
        BtlOpp_GetTargetPos(chr, &opp);
        Vec4_Sub(&dBase, &opp, &base);
        dBase.y = 0.0f;
        Vec4_Sub(&dCur, &opp, &cur);
        dCur.y = 0.0f;
        lenBase = Vec3_Length(&dBase);
        len = Vec3_Length(&dCur);
        if (!(far < len)) {
            if (!(near < len)) {
                return;
            }
            w = (len - near) / (far - near);
        }
        if (lenBase < near) {
            return;
        }
        Vec4_Scale(&s, &dBase, 1.0f - Vec3_Dot(&dBase, &dCur) / (lenBase * lenBase));
        Vec4_Add(&e, &dCur, &s);
        lenE = Vec3_Length(&e);
        if (lenE < 0.001f) {
            return;
        }
        Vec4_Scale(&g, &e, (lenE - lenBase) / lenE * w);
        Vec4_Add(&pose->pos, &pose->pos, &g);
        c = Vec3_Dot(&dBase, &e) / (lenBase * lenE);
        if (1.0f < c) {
            c = 1.0f;
        }
        a = Mathf_Acos(c);
        if (w < 1.0f) {
            a = a * w;
            Mathf_Cos(a);
        }
        if (dBase.z * e.x - dBase.x * e.z < 0.0f) {
            a = -a;
        }
        Vec4_Set(&ang, 0.0f, a, 0.0f, 0.0f);
        Vec4_RotateEuler(&pose->dir, &ang, &pose->dir);
        pose->rot.y = BtlUtil_WrapAngle(pose->rot.y + a);
        pose->yaw = BtlUtil_WrapAngle(pose->yaw + a);
    }
}

/* Stage 7: keeps the fighter out of the opponent's body: a vertical plane between the two, placed by both radii,
   shared between them in proportion to how much each moved towards the other this frame. */
s32 BtlMove_PushOut(BtlMoveChr *chr) {
    Vec4 base;
    Vec4 d;
    Vec4 n;
    Vec4 plane;
    Vec4 own0;
    Vec4 opp0;
    f32 h[2];
    Vec4 oppMove;
    Vec4 oppA;
    Vec4 oppB;
    Vec4 move;
    Vec4 rel;
    Vec4 oppC;
    Vec4 push;
    BtlMovePose *pose = BtlChar_GetPos(chr);
    s32 checkHeight;
    f32 len;
    f32 inv;
    f32 sum;
    f32 ratio;
    f32 depth;

    if (BtlChar_TestFlag(chr, 0x3F)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xAE)) {
        return 0;
    }
    checkHeight = !BtlChar_TestFlag(chr, 5);
    if (BtlChar_TestFlag(chr, 0x1A)) {
        checkHeight = 1;
    }
    if (BtlChar_TestFlag(chr, 0xAD)) {
        checkHeight = 1;
    }
    BtlChar_GetSnapPos(chr, &own0, 0);
    BtlOpp_GetSnapPos(chr, &opp0, 0);
    Vec4_Sub(&d, &opp0, &own0);
    if (checkHeight) {
        h[0] = BtlCharApi_GetHeight(chr->objId);
        h[1] = BtlOpp_GetHeight(chr);
        if (d.y < -h[0] || h[1] < d.y) {
            return 0;
        }
    }
    Vec4_Copy(&n, &d);
    n.y = 0.0f;
    len = Vec3_Length(&n);
    if (len < 0.001f) {
        return 0;
    }
    inv = 1.0f / len;
    n.x *= inv;
    n.z *= inv;
    BtlOpp_GetSnapPos(chr, &oppA, 0);
    BtlOpp_GetSnapPos(chr, &oppB, 1);
    Vec4_Sub(&oppMove, &oppB, &oppA);
    oppMove.y = 0.0f;
    if (Vec3_Dot(&oppMove, &n) < 0.0f) {
        Vec4_Copy(&base, &oppA);
    } else {
        Vec4_Copy(&base, &oppB);
    }
    plane.x = -n.x;
    plane.y = 0.0f;
    plane.z = -n.z;
    plane.w = -Vec3_Dot(&plane, &base);
    plane.w -= BtlCharApi_GetRadius(chr->objId);
    plane.w -= BtlOpp_GetRadius(chr);
    BtlChar_GetSnapDelta(chr, &move, 0, 1);
    BtlOpp_GetSnapPos(chr, &oppC, 1);
    Vec4_Sub(&rel, &oppC, &base);
    move.y = 0.0f;
    rel.y = 0.0f;
    h[0] = Vec3_Dot(&n, &move);
    h[1] = -Vec3_Dot(&n, &rel);
    if (h[0] < 0.0f) {
        h[0] = 0.0f;
    }
    if (h[1] < 0.0f) {
        h[1] = 0.0f;
    }
    sum = h[0] + h[1];
    if (sum < 0.001f) {
        ratio = 0.5f;
    } else {
        ratio = h[0] / sum;
    }
    depth = (Vec3_Dot(&pose->pos, &plane) + plane.w + h[0]) * ratio - h[0];
    if (depth < 0.0f) {
        Vec4_Scale(&push, &n, depth);
        Vec4_Add(&pose->pos, &pose->pos, &push);
        BtlChar_SetFlag(chr, 0x5E);
        if (checkHeight) {
            BtlChar_SetFlag(chr, 0x5F);
        }
        return 1;
    }
    return 0;
}

/* Vertical speed that covers `height` when it is afterwards braked by 100 km/h per frame, plus extra, limited to
   +-800 km/h. */
f32 BtlMove_CalcVerticalSpeed(f32 height, f32 extra) {
    f32 v = 0.0f;

    for (;;) {
        if (0.0f < height) {
            height -= v;
            v += BTL_KMH(100.0f);
            if (height < 0.0f) {
                v += (1.0f - -height / v) * BTL_KMH(100.0f) - BTL_KMH(100.0f);
                break;
            }
        } else {
            height -= v;
            v -= BTL_KMH(100.0f);
            if (0.0f < height) {
                v -= (1.0f - -height / v) * BTL_KMH(100.0f) - BTL_KMH(100.0f);
                break;
            }
        }
    }
    v += extra;
    if (BTL_KMH(800.0f) < v) {
        v = BTL_KMH(800.0f);
    }
    if (v < -BTL_KMH(800.0f)) {
        v = -BTL_KMH(800.0f);
    }
    return v;
}

/* Sets the vertical speed so that the fighter ends at the opponent's height. */
void BtlMove_AimVerticalAtOpponent(BtlMoveChr *chr, s32 lead) {
    Vec4 to;
    Vec4 vel;
    f32 v;

    BtlOpp_GetDelta(chr, &to);
    if (lead) {
        BtlOpp_GetVelocity(chr, &vel);
        v = BtlMove_CalcVerticalSpeed(to.y + BtlOpp_GetHalfHeightDiff(chr), vel.y * 0.5f);
    } else {
        v = BtlMove_CalcVerticalSpeed(to.y + BtlOpp_GetHalfHeightDiff(chr), 0.0f);
    }
    BtlChar_GetPos(chr)->fallSpeed = v;
}

/* Take-off: once (flag 0x8A) faces the opponent, enters flight (0xE) and aims for its height; raises 0x81, 0x82, 0x8A. */
void BtlMove_BeginRiseToOpponent(BtlMoveChr *chr) {
    if (!BtlChar_TestFlag(chr, 0x8A)) {
        if (BtlChar_TestFlag(chr, 5)) {
            BtlMove_TurnYaw(chr, 2, 3.14159265f);
            BtlMove_TurnPitch(chr, 5, 3.14159265f);
            BtlMove_SetDirection(chr, 2);
            BtlChar_SetHeldFlag(chr, 0xE);
            BtlMove_AimVerticalAtOpponent(chr, 1);
            BtlChar_SetHeldFlag(chr, 0x81);
        }
    }
    BtlChar_SetHeldFlag(chr, 0x82);
    BtlChar_SetHeldFlag(chr, 0x8A);
}

/* Upward launch speed (negative) that rises `height` against gravity. */
f32 BtlMove_CalcJumpSpeed(f32 height) {
    f32 v = 0.0f;

    for (;;) {
        height -= v;
        v += BTL_KMH(50.0f);
        if (height < 0.0f) {
            v += (1.0f - -height / v) * BTL_KMH(50.0f) - BTL_KMH(50.0f);
            break;
        }
    }
    return -v;
}

/* Sets the impulse velocity (knock-back) unless the current one is larger. */
void BtlMove_SetImpulse(BtlMoveChr *chr, Vec4 *vel) {
    Vec4 *cur = &BtlChar_GetPos(chr)->unk70;

    if (Vec3_Length(cur) < Vec3_Length(vel)) {
        Vec4_Copy(cur, vel);
    }
}

/* Impulse along a direction. */
void BtlMove_SetImpulseDir(BtlMoveChr *chr, Vec4 *dir, f32 speed) {
    Vec4 vel;

    Vec4_Scale(&vel, dir, speed);
    BtlMove_SetImpulse(chr, &vel);
}

/* Horizontal impulse along a yaw. */
void BtlMove_SetImpulseYaw(BtlMoveChr *chr, f32 yaw, f32 speed) {
    Vec4 dir;

    dir.x = Mathf_Sin(yaw);
    dir.y = 0.0f;
    dir.z = Mathf_Cos(yaw);
    dir.w = 0.0f;
    BtlMove_SetImpulseDir(chr, &dir, speed);
}

/* After touching the stage limit: heads along the wall (or the reverse), and removes the pitch into the top / bottom. */
void BtlMove_DeflectAtStageLimit(BtlMoveChr *chr, s32 back) {
    BtlMovePose *pose = BtlChar_GetPos(chr);

    if (BtlChar_TestFlag(chr, 0x14) && 0.0f < BtlUtil_LengthXZ(&pose->pos)) {
        f32 a;

        if (0.0f < pose->pos.x * pose->unk30.z - pose->pos.z * pose->unk30.x) {
            a = atan2f(pose->pos.z, -pose->pos.x);
        } else {
            a = atan2f(-pose->pos.z, pose->pos.x);
        }
        if (back) {
            a = BtlUtil_WrapAngle(a + 3.14159265f);
        }
        pose->yaw = a;
    }
    if (BtlChar_TestFlag(chr, 0x15)) {
        if (back) {
            if (0.0f < pose->pitch) {
                pose->pitch = 0.0f;
            }
        } else {
            if (pose->pitch < 0.0f) {
                pose->pitch = 0.0f;
            }
        }
    }
    if (BtlChar_TestFlag(chr, 0x16)) {
        if (back) {
            if (pose->pitch < 0.0f) {
                pose->pitch = 0.0f;
            }
        } else {
            if (0.0f < pose->pitch) {
                pose->pitch = 0.0f;
            }
        }
    }
}

/* Per frame: pos += impulse, then the impulse loses BTL_IMPULSE_DECEL of length. Declared int with no return value:
   the original does not tail-call. */
s32 BtlMove_ApplyImpulse(BtlMoveChr *chr) {
    BtlMovePose *pose = BtlChar_GetPos(chr);

    if (!BtlChars_IsTimeStopped()) {
        Vec4 *vel = &pose->unk70;
        f32 len;

        Vec4_Add(&pose->pos, &pose->pos, vel);
        pose->pos.w = 1.0f;
        len = Vec3_Length(vel);
        if (len < BTL_IMPULSE_DECEL) {
            Vec4_SetZero(vel);
        } else {
            Vec4_Scale(vel, vel, (len - BTL_IMPULSE_DECEL) / len);
        }
    }
}

/* Per frame: the display offset (pose+0x20): hover bob 2 * sin(phase) while flying idle, and a sideways shake. */
void BtlMove_UpdateHoverOffset(BtlMoveChr *chr) {
    BtlMovePose *pose = BtlChar_GetPos(chr);
    f32 a;

    Vec4_SetZero(&pose->unk20);
    if (BtlChar_TestFlag(chr, 0x29)) {
        pose->unkA8 = 0.0f;
    }
    if (BtlChars_IsTimeStopped()) {
        return;
    }
    if (BtlChar_TestFlag(chr, 0xE) && BtlChar_TestFlag(chr, 0x1C)) {
        pose->unkA8 = BtlUtil_WrapAngle(pose->unkA8 + BTL_DEG(6.0f));
    } else {
        if (1.5707963f < pose->unkA8) {
            pose->unkA8 = 3.14159265f - pose->unkA8;
        }
        if (pose->unkA8 < -1.5707963f) {
            pose->unkA8 = -3.14159265f - pose->unkA8;
        }
        if (0.0f < pose->unkA8) {
            pose->unkA8 -= BTL_DEG(6.0f);
            if (pose->unkA8 < 0.0f) {
                pose->unkA8 = 0.0f;
            }
        }
        if (pose->unkA8 < 0.0f) {
            pose->unkA8 += BTL_DEG(6.0f);
            if (0.0f < pose->unkA8) {
                pose->unkA8 = 0.0f;
            }
        }
    }
    pose->unk20.y = Mathf_Sin(pose->unkA8) * 2.0f;
    if (chr->unkFE8 > 0) {
        f32 k = ((f32)(chr->unkFE8 & 1) - 0.5f) * 0.5f;

        pose->unk20.x = -Mathf_Cos(pose->yaw) * k;
        pose->unk20.z = Mathf_Sin(pose->yaw) * k;
    }
}

/* Per frame: the seven defence windows at chr+0x1068: each counts down to a floor and is re-opened by its input. */
void BtlMove_UpdateDefenseTimers(BtlMoveChr *chr) {
    s32 on = BtlInput_TestAction(chr, 0x27, 1);
    f32 hp = BtlMember_GetHealthRatio(chr);

    if (!BtlChar_TestFlag(chr, 5)) {
        on = 0;
    }
    if (BtlChar_TestFlag(chr, 0x12C)) {
        chr->unk1068 = -30;
        chr->unk106C = -30;
        chr->unk107C = -30;
    }
    chr->unk1068--;
    if (chr->unk1068 < -30) {
        chr->unk1068 = -30;
        if (on) {
            if (hp < 0.3f) {
                chr->unk1068 = 2;
            } else if (hp < 0.7f) {
                chr->unk1068 = 3;
            } else {
                chr->unk1068 = 4;
            }
        }
    } else {
        if (on) {
            chr->unk1068 = -1;
        }
        if (chr->unkFB0 != 1) {
            chr->unk1068 = -30;
        }
    }
    chr->unk106C--;
    if (chr->unk106C < -30) {
        chr->unk106C = -30;
        if (on) {
            if (BtlChar_TestFlag(chr, 0x138)) {
                chr->unk106C = 4;
            } else {
                chr->unk106C = 2;
            }
        }
    } else {
        if (on) {
            chr->unk106C = -1;
        }
    }
    chr->unk1070--;
    if (chr->unk1070 < -30) {
        chr->unk1070 = -30;
        if (BtlInput_TestAction(chr, 0x26, 1)) {
            chr->unk1070 = 1;
        }
    } else {
        if (BtlInput_TestAction(chr, 0x26, 1)) {
            chr->unk1070 = -1;
        }
    }
    chr->unk1074--;
    if (chr->unk1074 < -15) {
        chr->unk1074 = -15;
        if (BtlInput_TestAction(chr, 0x2B, 1) && (BtlParam_GetFlags2(chr) & 0x20)) {
            chr->unk1074 = 5;
        }
    }
    chr->unk1078--;
    if (chr->unk1078 < -30) {
        chr->unk1078 = -30;
        if (on) {
            chr->unk1078 = 5;
        }
    }
    chr->unk107C--;
    if (chr->unk107C < -30) {
        chr->unk107C = -30;
        if (on) {
            if (BtlChar_TestFlag(chr, 0x138)) {
                chr->unk107C = 4;
            } else {
                chr->unk107C = 2;
            }
        }
    } else {
        if (on) {
            chr->unk107C = -1;
        }
    }
    if (BtlParam_GetFlags2(chr) & 8) {
        chr->unk1080++;
        if (chr->unk1080 >= 2) {
            chr->unk1080 = 1;
        }
        if (on) {
            chr->unk1080 = -15;
        }
    } else {
        chr->unk1080 = -1;
    }
    if (BtlChar_TestFlag(chr, 0x80) && BtlMember_HasAbility(chr, 0x38)) {
        chr->unk106C = 1;
    }
}
