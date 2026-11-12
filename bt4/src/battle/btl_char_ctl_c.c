#include "common.h"
extern s32 TagBattle_IsActorLoading(s32 actor);
#include "battle/btl_char_ctl.h"

/*
 * Head tracking: 0x1D6438..0x1D70E8.
 *
 * A fighter turns its head (model node 0x2E) and neck (node 0x2F) towards the other fighter's head.
 *   look.offset  eased 20% per frame towards (object position - position of node 0x2F), i.e. head -> origin.
 *   direction    (other's position - own position) + own look.offset - the OTHER fighter's look.offset
 *                (BtlOpp_GetDelta, BtlOpp_GetUnk15A0), normalised, then eased 30% per frame into look.dir
 *                (copied at once when look.snap is set).
 *   BtlChar_UpdateLook: the direction is taken into the head node's space (inverse of the node matrix, turned by
 *                the neck pitch parameter). Tracking starts when |yaw| < 1.5 and -0.6 < pitch' < 0.6 (pitch' =
 *                pitch + 0.2 |yaw|) and the target is less than 90 degrees off the fighter's heading; it is kept
 *                while |yaw| < 1.8 and -1 < pitch' < 1. While tracking, the direction is clamped to the
 *                character's limits (three bytes of the parameter block, in degrees) by rotating it, and the
 *                rotation from "forward" (0, 0, -1) to it, at half strength, goes to obj->headRot and obj->neckRot.
 *   BtlChar_UpdateLookAlt (action 0x99): the same with wider thresholds (0.8 / 1.8 to start, 1.3 / 2.5 to keep)
 *                and plain yaw / pitch clamps: headRot = yaw about y, neckRot = (head pitch - pitch) about x.
 *   The blend weights obj->headBlend / neckBlend ease 30% per frame towards 1 while tracking with flag 5 set
 *   and look.enabled, towards 0 otherwise.
 * Tracking needs fighter flag 5 (or look.enabled == 0, which keeps the pose computed but the weight at 0) and
 * is off under flag 0xAC. Everything here is driven by fighter and object state only: no pad, no random numbers.
 * BtlChar_UpdateLookOffset resets the state while a character change is loading (BtlChars_IsTimeStopped).
 */

extern f32 atan2f(f32 y, f32 x);
extern f32 asinf(f32 x);
extern f32 fabsf(f32 x);

extern void Vec4_Set(Vec4 *out, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *v, f32 scale);
extern void Vec3_Normalize(Vec4 *out, Vec4 *v);
extern void Vec3_Cross(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *out, Mtx44 *m, Vec4 *v);
extern void Mtx_Mul(Mtx44 *out, Mtx44 *a, Mtx44 *b);
extern void Mtx_Copy(Mtx44 *out, Mtx44 *in);
extern void Mtx_InverseRT(Mtx44 *out, Mtx44 *in);
extern void Mtx_RotateX(Mtx44 *out, Mtx44 *in, f32 angle);
extern void Vec4_Lerp(Vec4 *out, Vec4 *a, Vec4 *b, f32 t);
extern void Vec3_RotateAxis(Vec4 *out, Vec4 *v, Vec4 *axis, f32 angle);

extern BtlCtlObj *BtlChar_GetObj(BtlCtlChr *chr);
extern BtlCtlPose *BtlChar_GetPos(BtlCtlChr *chr);
extern s32 BtlChar_TestFlag(BtlCtlChr *chr, s32 bit);
extern f32 BtlUtil_WrapAngle(f32 angle);
extern s32 *BtlMember_GetActive(BtlCtlChr *chr);
extern void BtlOpp_GetDelta(BtlCtlChr *chr, Vec4 *out);
extern void BtlOpp_GetUnk15A0(BtlCtlChr *chr, Vec4 *out);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlParam_GetFlags(BtlCtlChr *chr);
extern BtlCtlNode *BtlObj_GetNode(BtlCtlObj *obj, s32 node);

/* Clears the head tracking: identity rotations, zero weights, and the height offset. */
void BtlChar_ResetLook(BtlCtlChr *chr) {
    BtlCtlLook *look = &chr->look;
    BtlCtlObj *obj = BtlChar_GetObj(chr);

    Quat_SetIdentity(&obj->headRot);
    Quat_SetIdentity(&obj->neckRot);
    obj->headBlend = 0.0f;
    obj->neckBlend = 0.0f;
    look->height = BtlCharApi_GetHeight(chr->objId) * 0.7f;
}

/* Per frame: eases the look offset towards the head node's offset from the body (reset while time is stopped). */
void BtlChar_UpdateLookOffset(BtlCtlChr *chr) {
    Vec4 delta;
    Vec4 offset;
    BtlCtlLook *look = &chr->look;
    BtlCtlObj *obj = BtlChar_GetObj(chr);

    if (TagBattle_IsActorLoading(chr->player)) {
        BtlChar_ResetLook(chr);
    } else {
        BtlCtlNode *node = BtlObj_GetNode(obj, 0x2F);

        look->height += (obj->outPos.y - node->unk40.y - look->height) * 0.2f;
        Vec4_Sub(&offset, &obj->outPos, &node->unk40);
        Vec4_Sub(&delta, &offset, &look->offset);
        Vec4_Scale(&delta, &delta, 0.2f);
        Vec4_Add(&look->offset, &look->offset, &delta);
    }
}

/* Turns the head tracking on or off. */
void BtlChar_SetLookEnabled(BtlCtlChr *chr, s32 enabled) {
    chr->look.enabled = enabled;
}

/* Head tracking: turns the head and neck nodes towards the look target within the character's limits. */
void BtlChar_UpdateLook(BtlCtlChr *chr) {
    Vec4 q0;
    Vec4 q1;
    Mtx44 m;
    Mtx44 inv;
    Vec4 other;
    Mtx44 rotm;
    Vec4 target;
    Vec4 local;
    Vec4 fwd;
    Vec4 axis;
    Vec4 up;
    Vec4 axes[2];
    BtlCtlNode *head;
    BtlCtlNode *neck;
    s32 full;
    BtlCtlObj *obj;
    BtlCtlLook *look = &chr->look;
    s32 active;
    f32 headPitch;
    f32 neckPitch;
    f32 blend;
    f32 angle;
    f32 limA;
    f32 limB;
    f32 limC;
    f32 down;
    s32 i;

    obj = BtlChar_GetObj(chr);
    neckPitch = obj->param->neckPitch * 3.14159265f / 180.0f;
    headPitch = obj->param->headPitch * 3.14159265f / 180.0f;
    full = 1;
    active = 1;
    if (!BtlChar_TestFlag(chr, 5)) {
        full = 0;
        active = 0;
    }
    if (look->enabled == 0) {
        full = 0;
        active = 1;
    }
    if (BtlChar_TestFlag(chr, 0xAC)) {
        full = 0;
        active = 0;
    }
    head = BtlObj_GetNode(obj, 0x2E);
    neck = BtlObj_GetNode(obj, 0x2F);
    blend = 0.0f;
    Vec4_Copy(&q0, (Vec4 *)&head->rot);
    Vec4_Copy(&q1, (Vec4 *)&neck->rot);
    Mtx_Copy(&m, &head->mtx);
    Mtx_StoreIdentity(&rotm);
    Mtx_RotateX(&rotm, &rotm, neckPitch);
    Mtx_Mul(&m, &m, &rotm);
    Mtx_InverseRT(&inv, &m);
    BtlOpp_GetDelta(chr, &target);
    Vec4_Add(&target, &target, &look->offset);
    BtlOpp_GetUnk15A0(chr, &other);
    Vec4_Sub(&target, &target, &other);
    Vec3_Normalize(&target, &target);
    target.w = blend;
    if (look->snap) {
        Vec4_Copy(&look->dir, &target);
    } else {
        Vec4_Lerp(&look->dir, &target, &look->dir, 0.3f);
    }
    look->dir.w = 0.0f;
    if (active) {
        f32 pitch;

        Vec4_Set(&fwd, look->dir.w, look->dir.w, -1.0f, look->dir.w);
        Mtx_MulVec4(&local, &inv, &look->dir);
        Vec3_Normalize(&local, &local);
        if (look->tracking) {
            limB = -1.0f;
            limA = 1.0f;
            limC = 1.8f;
        } else {
            limA = 0.6f;
            limB = -0.6f;
            limC = 1.5f;
        }
        angle = atan2f(-local.x, -local.z);
        pitch = -asinf(local.y);
        pitch += fabsf(angle) * 0.2f;
        if (-limC < angle && angle < limC && limB < pitch && pitch < limA) {
            if (!look->tracking) {
                BtlOpp_GetDelta(chr, &axis);
                angle = atan2f(axis.x, axis.z);
                if (fabsf(BtlUtil_WrapAngle(angle - BtlChar_GetPos(chr)->heading)) < 1.5707963f) {
                    look->tracking = 1;
                }
            }
        } else {
            look->tracking = 0;
        }
        if (look->tracking) {
            down = -(obj->param->headDownMax * 3.14159265f / 180.0f);
            limC = obj->param->headUpMax * 3.14159265f / 180.0f;
            limB = obj->param->headYawMax * 3.14159265f / 180.0f;
            Vec4_Set(&up, 0.0f, 1.0f, 0.0f, 0.0f);
            Vec3_Cross(&axis, &local, &up);
            Vec3_Normalize(&axis, &axis);
            angle = -asinf(local.y);
            angle -= fabsf(atan2f(-local.x, -local.z)) * local.y * 0.2f;
            if (limC < angle) {
                Vec3_RotateAxis(&local, &local, &axis, angle - limC);
            }
            if (angle < down) {
                Vec3_RotateAxis(&local, &local, &axis, angle - down);
            }
            Vec4_Copy(&axes[0], (Vec4 *)inv.m[1]);
            Vec4_Set(&axes[1], 0.0f, 1.0f, 0.0f, 0.0f);
            for (i = 0; i < 2; i++) {
                angle = atan2f(-local.x, -local.z);
                if (angle < -limB) {
                    Vec3_RotateAxis(&local, &local, &axes[i], -limB - angle);
                }
                if (limB < angle) {
                    Vec3_RotateAxis(&local, &local, &axes[i], limB - angle);
                }
            }
            Quat_FromVectors((Quat *)&axis, &fwd, &local, 0.5f);
            Quat_FromAxisAngle((Quat *)&up, 1.0f, 0.0f, 0.0f, headPitch * 0.5f);
            Quat_Mul((Quat *)&axis, (Quat *)&axis, (Quat *)&up);
            Quat_FromAxisAngle((Quat *)&up, 1.0f, 0.0f, 0.0f, neckPitch * 0.5f);
            Quat_Mul((Quat *)&axis, (Quat *)&up, (Quat *)&axis);
            Vec4_Copy((Vec4 *)&obj->headRot, &axis);
            Vec4_Copy((Vec4 *)&obj->neckRot, &axis);
            head->flags &= ~1;
            neck->flags &= ~1;
            blend = 1.0f;
            if (!full) {
                blend = 0.0f;
            }
        }
    } else {
        look->tracking = 0;
    }
    if (look->snap) {
        obj->headBlend = blend;
        obj->neckBlend = blend;
    } else {
        obj->headBlend += (blend - obj->headBlend) * 0.3f;
        obj->neckBlend += (blend - obj->neckBlend) * 0.3f;
    }
}

/* Head tracking used during action 0x99: plain yaw and pitch clamps instead of the rotated direction. */
void BtlChar_UpdateLookAlt(BtlCtlChr *chr) {
    Vec4 q0;
    Vec4 q1;
    Mtx44 m;
    Mtx44 inv;
    Vec4 other;
    Vec4 target;
    Vec4 local;
    Vec4 tmp;
    s32 full;
    BtlCtlObj *obj;
    BtlCtlLook *look = &chr->look;
    s32 active;
    BtlCtlNode *head;
    BtlCtlNode *neck;
    f32 headPitch;
    f32 blend;
    f32 yaw;
    f32 pitch;
    f32 yawMax;
    f32 upMax;
    f32 downMax;

    obj = BtlChar_GetObj(chr);
    headPitch = obj->param->headPitch * 3.14159265f / 180.0f;
    full = 1;
    active = 1;
    if (!BtlChar_TestFlag(chr, 5)) {
        full = 0;
        active = 0;
    }
    if (look->enabled == 0) {
        full = 0;
        active = 1;
    }
    if (BtlChar_TestFlag(chr, 0xAC)) {
        full = 0;
        active = 0;
    }
    head = BtlObj_GetNode(obj, 0x2E);
    neck = BtlObj_GetNode(obj, 0x2F);
    Vec4_Copy(&q0, (Vec4 *)&head->rot);
    blend = 0.0f;
    Vec4_Copy(&q1, (Vec4 *)&neck->rot);
    Mtx_Copy(&m, &head->mtx);
    Mtx_InverseRT(&inv, &m);
    BtlOpp_GetDelta(chr, &target);
    Vec4_Add(&target, &target, &look->offset);
    BtlOpp_GetUnk15A0(chr, &other);
    Vec4_Sub(&target, &target, &other);
    Vec3_Normalize(&target, &target);
    target.w = blend;
    if (look->snap) {
        Vec4_Copy(&look->dir, &target);
    } else {
        Vec4_Lerp(&look->dir, &target, &look->dir, 0.3f);
    }
    look->dir.w = 0.0f;
    if (active) {
        Mtx_MulVec4(&local, &inv, &look->dir);
        Vec3_Normalize(&local, &local);
        if (look->tracking) {
            upMax = 1.3f;
            downMax = -1.3f;
            yawMax = 2.5f;
        } else {
            upMax = 0.8f;
            downMax = -0.8f;
            yawMax = 1.8f;
        }
        yaw = atan2f(-local.x, -local.z);
        pitch = -asinf(local.y);
        pitch += fabsf(yaw) * 0.2f;
        if (-yawMax < yaw && yaw < yawMax && downMax < pitch && pitch < upMax) {
            if (!look->tracking) {
                f32 dir;

                BtlOpp_GetDelta(chr, &tmp);
                dir = atan2f(tmp.x, tmp.z);
                if (fabsf(BtlUtil_WrapAngle(dir - BtlChar_GetPos(chr)->heading)) < 1.5707963f) {
                    look->tracking = 1;
                }
            }
        } else {
            look->tracking = 0;
        }
        if (look->tracking) {
            f32 up = obj->param->headUpMax * 3.14159265f / 180.0f;
            f32 down = -(obj->param->headDownMax * 3.14159265f / 180.0f);
            f32 side = obj->param->headYawMax * 3.14159265f / 180.0f;

            if (up < pitch) {
                pitch = up;
            }
            if (pitch < down) {
                pitch = down;
            }
            if (yaw < -side) {
                yaw = -side;
            }
            if (side < yaw) {
                yaw = side;
            }
            Quat_FromAxisAngle(&obj->headRot, 0.0f, 1.0f, 0.0f, yaw);
            Quat_FromAxisAngle(&obj->neckRot, 1.0f, 0.0f, 0.0f, headPitch - pitch);
            head->flags &= ~1;
            neck->flags &= ~1;
            blend = 1.0f;
            if (!full) {
                blend = 0.0f;
            }
        }
    } else {
        look->tracking = 0;
    }
    if (look->snap) {
        obj->headBlend = blend;
        obj->neckBlend = blend;
    } else {
        obj->headBlend += (blend - obj->headBlend) * 0.3f;
        obj->neckBlend += (blend - obj->neckBlend) * 0.3f;
    }
}

/* Runs the head tracking unless BtlParam_GetFlags has bit 0x20; action 0x99 uses the second variant. */
s32 BtlChar_UpdateHead(BtlCtlChr *chr) {
    if (!(BtlParam_GetFlags(chr) & 0x20)) {
        if (*BtlMember_GetActive(chr) == 0x99) {
            BtlChar_UpdateLookAlt(chr);
        } else {
            BtlChar_UpdateLook(chr);
        }
    }
}
