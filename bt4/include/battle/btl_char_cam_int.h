#ifndef BATTLE_BTL_CHAR_CAM_INT_H
#define BATTLE_BTL_CHAR_CAM_INT_H

/* What the three fighter-camera files (btl_char_cam.c, btl_char_cam_cut.c, btl_char_cam_modes.c) use from other
 * modules, declared with this module's view of the fighter (battle/btl_char_cam.h). */

#include "battle/btl_char_cam.h"
#include "battle/btl_demo_cam.h"

extern f32 atan2f(f32 y, f32 x);

/* Vector maths (VU0 routines). */
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec4_SetZeroW1(Vec4 *dst);                            /* dst = (0, 0, 0, 1) */
extern void Vec4_SetZero(Vec4 *dst);                            /* dst = (0, 0, 0, 0) */
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);   /* dst = a * t + b * (1 - t) */
extern void Vec4_RotateEuler(Vec4 *dst, Vec4 *angles, Vec4 *src);   /* dst = src rotated by Euler angles */
extern void Vec3_Clamp(Vec4 *dst, Vec4 *src, f32 lo, f32 hi); /* clamp x, y, z */
extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
extern f32 Mathf_Tan(f32 a);
extern f32 Mathf_Asin(f32 a); /* argument clamped to -1..1 */
extern f32 Mathf_Atan(f32 a);
extern Vec4 D_002EC2A0[2];    /* shared constants: (0, 0, 0, 0) and (1, 1, 1, 1) */

/* Small float helpers of the fighter code. */
extern f32 BtlUtil_WrapAngle(f32 a);                       /* wrap to -pi..pi (one turn at most) */
extern void BtlUtil_WrapAngles(Vec4 *dst, Vec4 *src);      /* the same for x, y, z */
extern f32 BtlUtil_LengthXZ(Vec4 *v);                      /* sqrt(x * x + z * z) */
extern f32 BtlUtil_ApproachF(f32 cur, f32 goal, f32 step); /* move cur towards goal by at most |step| */
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
extern f32 BtlUtil_MinF(f32 a, f32 b);
extern f32 BtlUtil_MaxF(f32 a, f32 b);

/* Fighter helpers (0x1C.... / 0x1D....: not decompiled). */
extern ChrCamMgr *gBtlChars;
extern ChrCamChr *BtlChar_Get(s32 side);
extern s32 BtlChar_TestFlag(ChrCamChr *chr, s32 flag);          /* the flag, raised this frame or held */
extern void BtlChar_SetFlag(ChrCamChr *chr, s32 flag);          /* raise a one-frame flag */
extern s32 BtlChar_TestPrevFlag(ChrCamChr *chr, s32 flag);             /* the flag in the second pair of flag arrays (+0x10D5 / +0x10FD): the previous frame, inferred */
extern s32 BtlChar_IsFlagRaised(ChrCamChr *chr, s32 flag);             /* flag set in the first pair and not in the second: newly raised, inferred */
extern ChrCamBody *BtlChar_GetPos(ChrCamChr *chr);              /* &chr->body */
extern void *BtlChar_GetObj(ChrCamChr *chr);                    /* BtlObj_Get(chr->objId) */
extern s32 BtlAnim_GetId(ChrCamChr *chr);                       /* chr->unk974 */
extern s32 BtlChars_IsTimeStopped(void);                                 /* gBtlChars->unk274 */
extern void BtlOpp_GetTargetPos(ChrCamChr *chr, Vec4 *out);           /* the opponent's override position 6 (+0x340 when bit 6 of +0x3C0 is set), else its position */
extern void BtlOpp_GetTargetRot(ChrCamChr *chr, Vec4 *out);           /* the same for the opponent's rotation (BtlChar_GetSnapRot) */
extern f32 BtlOpp_GetDistance(ChrCamChr *chr);                       /* distance to the opponent */
extern f32 BtlOpp_GetYaw(ChrCamChr *chr);                       /* yaw of the direction to the opponent (0 when on top of it) */
extern f32 BtlOpp_GetHalfHeightDiff(ChrCamChr *chr);                       /* (own body scale - the opponent's) / 2 */
extern f32 BtlOpp_GetHeight(ChrCamChr *chr);                       /* the opponent's body scale */
extern s32 BtlOpp_GetObjId(ChrCamChr *chr);                       /* the opponent's objId */
extern void *BtlOpp_GetObj(ChrCamChr *chr);                     /* the opponent's model object */
extern void BtlOpp_GetNodePos(ChrCamChr *chr, s32 node, Vec4 *out); /* world position of a node of the opponent's model */

/* Fighter input (battle/btl_input.h; declared here for this file's view of the fighter). */
extern s32 BtlInput_IsPressed(ChrCamChr *chr, u32 mask);
extern s32 BtlInput_IsHeld(ChrCamChr *chr, u32 mask);
extern f32 BtlInput_GetStickX(ChrCamChr *chr);                       /* smoothed stick x (BtlCharInput.stick[0]) */
extern s32 BtlInput_TestAction(ChrCamChr *chr, s32 action, s32 arg);  /* action input test, built on BtlInput_* */

/* Model objects and stage. */
extern f32 BtlCharApi_GetHeight(s32 objId);                            /* body scale, obj + 0xFF4 (10 when no object) */
extern f32 BtlCharApi_GetBodyUnk1000(s32 objId);                            /* obj + 0x1000 (5 when no object) */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);      /* world position of a model node (obj + 0x970 when the node is missing) */
extern f32 BtlStage_GetRadius(void);                                 /* stage: first float of the stage's limit block (horizontal radius, inferred) */
extern f32 BtlStage_GetBottom(void);                                 /* stage: third float of the same block (a height, inferred) */
extern s32 Battle_IsSplitScreen(void);

#define PI 3.14159265f
#define HALF_PI 1.5707963f

#endif
