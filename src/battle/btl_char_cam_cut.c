#include "common.h"
#include "battle/btl_char_cam_int.h"

/* The fighter camera's cut evaluator, 0x1C4F68-0x1C5840: the middle of btl_char_cam.c, still a file of its own (see the
 * note at the top of btl_char_cam.c; the three can be merged now that it matches). It emits two .lit4 words, both 0.001f
 * (0x2FD08C, 0x2FD090). */

/* Evaluates the running camera cut: refreshes the node positions, interpolates eye / target / look-at and the
   three angles by the cut's progress, derives the rotation, raises the cut's flags and counts its timer down.
   The masked node id is written out at each of the two calls (`node & CHRCUT_NODE_MASK`): a variable for it
   shares the node's register and frees one for a hoisted &tmp0, which the original does not have. */
void ChrCam_CalcCut(ChrCamChr *chr, Vec4 *eye, Vec4 *rot, Vec4 *target) {
    Vec4 look;
    Vec4 dir;
    Vec4 tmp0;
    Vec4 tmp1;
    Vec4 tmp2;
    Vec4 tmp3;
    ChrCamCut *cut = &chr->cut;
    s32 frozen;
    ChrCam *cam;
    s32 refreshed;
    s32 usedOpp;
    s32 usedMid;
    f32 t;
    s32 node;
    s32 other;
    s32 id;
    f32 eps;
    f32 yaw;
    f32 pitch;
    f32 dist;
    f32 len;
    f32 limit;
    f32 a;

    frozen = BtlChar_TestFlag(chr, 0xB4);
    cam = &chr->cam;
    t = 0.0f;
    refreshed = 0;
    usedOpp = 0;
    usedMid = 0;
    if (cut->total > 0) {
        t = 1.0f - (f32)cut->timer / (f32)cut->total;
    }

    node = cut->unk88;
    if (node >= 0) {
        other = 0;
        if (node & CHRCUT_NODE_OPP) {
            usedOpp = 1;
            id = BtlOpp_GetObjId(chr);
            other = 1;
        } else if (node & CHRCUT_NODE_MID) {
            id = -1;
            usedMid = 1;
        } else {
            id = chr->objId;
        }
        if (id < 0) {
            BtlCharApi_GetNodePos(chr->objId, node & CHRCUT_NODE_MASK, &tmp0);
            BtlCharApi_GetNodePos(BtlOpp_GetObjId(chr), node & CHRCUT_NODE_MASK, &tmp1);
            Vec4_Lerp(&cut->vecA, &tmp0, &tmp1, 0.5f);
        } else if (frozen == 0 || other == 0) {
            BtlCharApi_GetNodePos(id, cut->unk88 & CHRCUT_NODE_MASK, &cut->vecA);
        }
        refreshed = 1;
    }

    node = cut->unk8C;
    if (node >= 0) {
        if (cut->unk88 == node) {
            Vec4_SetZeroW1(&cut->vecADelta);
        } else {
            other = 0;
            if (node & CHRCUT_NODE_OPP) {
                usedOpp = 1;
                id = BtlOpp_GetObjId(chr);
                other = 1;
            } else if (node & CHRCUT_NODE_MID) {
                id = -1;
                usedMid = 1;
            } else {
                id = chr->objId;
            }
            if (id < 0) {
                BtlCharApi_GetNodePos(chr->objId, node & CHRCUT_NODE_MASK, &tmp2);
                BtlCharApi_GetNodePos(BtlOpp_GetObjId(chr), node & CHRCUT_NODE_MASK, &tmp3);
                Vec4_Lerp(&tmp0, &tmp2, &tmp3, 0.5f);
            } else if (frozen == 0 || other == 0) {
                BtlCharApi_GetNodePos(id, cut->unk8C & CHRCUT_NODE_MASK, &tmp0);
            }
            Vec4_Sub(&cut->vecADelta, &tmp0, &cut->vecA);
        }
        refreshed = 1;
    }

    node = cut->unk90;
    if (node >= 0) {
        other = 0;
        if (node & CHRCUT_NODE_OPP) {
            id = BtlOpp_GetObjId(chr);
            other = 1;
        } else if (node & CHRCUT_NODE_MID) {
            id = -1;
        } else {
            id = chr->objId;
        }
        if (id < 0) {
            BtlCharApi_GetNodePos(chr->objId, node & CHRCUT_NODE_MASK, &tmp0);
            BtlCharApi_GetNodePos(BtlOpp_GetObjId(chr), node & CHRCUT_NODE_MASK, &tmp1);
            Vec4_Lerp(&cut->vecC, &tmp0, &tmp1, 0.5f);
        } else if (frozen == 0 || other == 0) {
            BtlCharApi_GetNodePos(id, node & CHRCUT_NODE_MASK, &cut->vecC);
        }
    }

    node = cut->unk94;
    if (node >= 0) {
        if (cut->unk90 == node) {
            Vec4_SetZeroW1(&cut->vecCDelta);
        } else {
            other = 0;
            if (node & CHRCUT_NODE_OPP) {
                id = BtlOpp_GetObjId(chr);
                other = 1;
            } else if (node & CHRCUT_NODE_MID) {
                id = -1;
            } else {
                id = chr->objId;
            }
            if (id < 0) {
                BtlCharApi_GetNodePos(chr->objId, node & CHRCUT_NODE_MASK, &tmp1);
                BtlCharApi_GetNodePos(BtlOpp_GetObjId(chr), node & CHRCUT_NODE_MASK, &tmp2);
                Vec4_Lerp(&tmp0, &tmp1, &tmp2, 0.5f);
            } else if (frozen == 0 || other == 0) {
                BtlCharApi_GetNodePos(id, node & CHRCUT_NODE_MASK, &tmp0);
            }
            Vec4_Sub(&cut->vecCDelta, &tmp0, &cut->vecC);
        }
    }
    if (refreshed) {
        if (usedOpp) {
            Vec4_Copy(&cut->vecB, *(Vec4 **)((u8 *)BtlOpp_GetObj(chr) + 0xFA0));
        } else if (usedMid) {
            Vec4_Copy(&tmp0, *(Vec4 **)((u8 *)BtlOpp_GetObj(chr) + 0xFA0));
            Vec4_Copy(&tmp1, *(Vec4 **)((u8 *)BtlChar_GetObj(chr) + 0xFA0));
            Vec4_Lerp(&cut->vecB, &tmp0, &tmp1, 0.5f);
        } else {
            Vec4_Copy(&cut->vecB, *(Vec4 **)((u8 *)BtlChar_GetObj(chr) + 0xFA0));
        }
        Vec4_SetZeroW1(&cut->vecBDelta);
    }

    eps = 0.001f;
    Vec4_Scale(eye, &cut->vecADelta, t);
    Vec4_Add(eye, &cut->vecA, eye);
    Vec4_Scale(target, &cut->vecBDelta, t);
    Vec4_Add(target, &cut->vecB, target);
    Vec4_Scale(&look, &cut->vecCDelta, t);
    Vec4_Add(&look, &cut->vecC, &look);
    yaw = cut->valA + cut->valADelta * t;
    pitch = -(cut->valB + cut->valBDelta * t);
    dist = cut->valC + cut->valCDelta * t;
    dir.x = Mathf_Cos(pitch) * Mathf_Sin(yaw);
    dir.y = Mathf_Sin(pitch);
    dir.z = Mathf_Cos(pitch) * Mathf_Cos(yaw);
    Vec4_Scale(&dir, &dir, dist);
    Vec4_Sub(eye, eye, &dir);
    Vec4_SetZero(rot);
    Vec4_Sub(&dir, &look, eye);
    len = Vec3_Length(&dir);
    if (eps < len) {
        Vec4_Scale(&dir, &dir, 1.0f / len);
        Vec3_Clamp(&dir, &dir, -1.0f, 1.0f);
        rot->x = Mathf_Asin(-dir.y);
        if (__builtin_fabsf(dir.x) < eps && __builtin_fabsf(dir.z) < eps) {
            rot->y = 0.0f;
        } else {
            rot->y = atan2f(dir.x, dir.z);
        }
    }

    if (cut->flags & CHRCUT_F_SNAP_RUN) {
        BtlChar_SetFlag(chr, 0xCD);
    }
    if (cut->flags & CHRCUT_F_RATE_RUN) {
        BtlChar_SetFlag(chr, 0xD1);
    }
    if (cut->flags & CHRCUT_F_PRIORITY) {
        BtlChar_SetFlag(chr, 0xD3);
    }
    if (cut->timer > 0) {
        cut->timer--;
        if (cut->timer == 0 && !(cut->flags & CHRCUT_F_HOLD)) {
            if (cut->flags & CHRCUT_F_SNAP_END) {
                BtlChar_SetFlag(chr, 0xCE);
            }
            ChrCam_EndCut(chr);
        }
    }
    if (cut->flags & CHRCUT_F_SET_SIDE) {
        Vec4_Sub(&tmp2, &BtlChar_GetPos(chr)->pos, &cam->eye);
        if (0.001f < __builtin_fabsf(tmp2.x) || 0.001f < __builtin_fabsf(tmp2.z)) {
            limit = ChrCam_GetSideLimit(chr);
            a = atan2f(tmp2.x, tmp2.z);
            chr->cam.side = BtlUtil_ClampF(-BtlUtil_WrapAngle(a - BtlChar_GetPos(chr)->yaw), -limit, limit);
        }
    }
    BtlChar_SetFlag(chr, 0xD4);
}
