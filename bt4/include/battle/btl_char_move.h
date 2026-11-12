#ifndef BATTLE_BTL_CHAR_MOVE_H
#define BATTLE_BTL_CHAR_MOVE_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter movement (src/battle/btl_char_move.c, 0x1DC9A0..0x1E0290).
 * The structures are partial views local to this module: only the fields this file touches.
 *
 * The movement model (everything here is verified by the matching C unless marked):
 *
 *   heading   yaw (+0x94) and pitch (+0x90) of the direction the fighter wants to travel. Yaw 0 is +Z, yaw grows
 *             towards +X (dir = (sin yaw, 0, cos yaw)); a positive pitch points up (dir.y = -sin pitch, +Y is down).
 *   dir       unit vector (+0x80) built from the heading, or straight from the stick, by BtlMove_SetDirection.
 *   speed     scalar (+0x98), metres per frame along dir. BtlMove_Advance: speed approaches its target by `accel`
 *             per frame, then pos += dir * speed. There is no separate velocity vector and no time step: one call
 *             is one 30 Hz frame.
 *   vertical  a second scalar (+0x9C, positive = down) added to pos.y: gravity adds 50 km/h per frame up to
 *             3000 km/h; in flight (flag 0xE) it is braked to 0 by 100 km/h per frame instead.
 *   impulse   a vector (+0x70) added to pos every frame and shortened by a fixed amount (knock-back).
 *   model     rot.y (+0x14) is the yaw the model is drawn with; it follows the heading yaw at a limited rate.
 *
 *   Stick to heading: target yaw = atan2f(stickX, -stickY) + chr->camYaw (BtlMove_TurnYaw mode 0), or the same
 *   rotation written out with sin / cos of -camYaw (BtlMove_SetDirection modes 0 / 1). The stick is the smoothed
 *   pair at chr+0x75C / +0x760 (BtlInput_GetStickX / Y) and is only used while a direction bit (0xF0) is held.
 *
 *   Units (inferred from the constants): one macro form, km/h * 1000 / 3600 * (1 / 30), reproduces six different
 *   constants bit for bit (10, 50, 100, 500, 800 and 3000 km/h), so lengths are metres and speeds metres per frame.
 *   Angles written in degrees and converted (1.8, 6, 18, 36 degrees).
 */

/* Pose block of a fighter (chr + 0x10, 0xF0 bytes; BtlChar_GetPos returns it). Offsets in brackets are fighter
   offsets. Meanings marked (other) come from other modules' headers, not from this file. */
typedef struct BtlMovePose {
    /* 0x00 [0x010] */ Vec4 pos;       /* world position, w forced to 1 after a move */
    /* 0x10 [0x020] */ Vec4 rot;       /* y = model yaw */
    /* 0x20 [0x030] */ Vec4 unk20;     /* display offset rebuilt every frame: hover bob (y) and shake (x, z) */
    /* 0x30 [0x040] */ Vec4 unk30;     /* movement of the previous frame between snapshots 0 and 1 (written by
                                          BtlChar_EndFrame); used here as "my velocity" */
    /* 0x40 [0x050] */ Vec4 unk40;     /* whole movement of the previous frame (other); read here through BtlOpp_GetVelocity */
    /* 0x50 [0x060] */ Vec4 unk50;     /* root motion position (other) */
    /* 0x60 [0x070] */ Vec4 unk60;     /* root motion rotation (other) */
    /* 0x70 [0x080] */ Vec4 unk70;     /* impulse velocity (knock-back), metres per frame */
    /* 0x80 [0x090] */ Vec4 dir;       /* unit direction of travel, w = 0 */
    /* 0x90 [0x0A0] */ f32 pitch;      /* heading pitch, positive = up */
    /* 0x94 [0x0A4] */ f32 yaw;        /* heading yaw */
    /* 0x98 [0x0A8] */ f32 speed;      /* metres per frame along dir */
    /* 0x9C [0x0AC] */ f32 fallSpeed;  /* vertical speed, positive = down */
    /* 0xA0 [0x0B0] */ f32 unkA0;      /* model lean about x (BtlMove_SetLeanX) */
    /* 0xA4 [0x0B4] */ f32 unkA4;      /* model lean about z (BtlMove_SetLeanZ) */
    /* 0xA8 [0x0B8] */ f32 unkA8;      /* hover bob phase */
    /* 0xAC [0x0BC] */ f32 unkAC;
    /* 0xB0 [0x0C0] */ Vec4 unkB0;     /* ground point below the fighter (other) */
    /* 0xC0 [0x0D0] */ Vec4 unkC0;     /* ground normal, valid with flag 0xF */
    /* 0xD0 [0x0E0] */ f32 unkD0;
    /* 0xD4 [0x0E4] */ f32 unkD4;      /* yaw steering offset of the homing dash */
    /* 0xD8 [0x0E8] */ f32 unkD8;      /* pitch steering offset of the homing dash */
    /* 0xDC [0x0EC] */ f32 orbitNear;  /* BtlMove_RequestOrbit */
    /* 0xE0 [0x0F0] */ f32 orbitFar;
    /* 0xE4 [0x0F4] */ u8 unkE4[0xF0 - 0xE4];
} BtlMovePose; /* size 0xF0 */

/* Fighter (0x1600 bytes). */
typedef struct BtlMoveChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ BtlMovePose pose;
    /* 0x0100 */ BtlMovePose prev;
    /* 0x01F0 */ u8 unk1F0[0x4A0 - 0x1F0];
    /* 0x04A0 */ f32 camYaw;     /* yaw of the fighter camera: the only camera value this file reads */
    /* 0x04A4 */ u8 unk4A4[0xFB0 - 0x4A4];
    /* 0x0FB0 */ s32 unkFB0;     /* pending hit reaction id; != 1 closes the first defence window */
    /* 0x0FB4 */ u8 unkFB4[0xFE8 - 0xFB4];
    /* 0x0FE8 */ s32 unkFE8;     /* > 0: the model shakes sideways, the side alternating with bit 0 */
    /* 0x0FEC */ u8 unkFEC[0x1068 - 0xFEC];
    /* 0x1068 */ s32 unk1068;    /* defence windows (see BtlMove_UpdateDefenseTimers): > 0 = open */
    /* 0x106C */ s32 unk106C;
    /* 0x1070 */ s32 unk1070;
    /* 0x1074 */ s32 unk1074;
    /* 0x1078 */ s32 unk1078;
    /* 0x107C */ s32 unk107C;
    /* 0x1080 */ s32 unk1080;
    /* 0x1084 */ u8 unk1084[0x1600 - 0x1084];
} BtlMoveChr; /* size 0x1600 */

/* What obj + 0xFA0 points to. */
typedef struct BtlMoveObjBody {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ f32 radius;  /* subtracted from the stage radius */
} BtlMoveObjBody;

/* Battle object of a fighter (BtlChar_GetObj). */
typedef struct BtlMoveObj {
    /* 0x0000 */ u8 unk0[0xFA0];
    /* 0x0FA0 */ BtlMoveObjBody *unkFA0;
    /* 0x0FA4 */ u8 unkFA4[0x1660 - 0xFA4];
    /* 0x1660 */ u8 *work;   /* stage contact work buffer */
} BtlMoveObj;

/* Second stage contact of the object, at obj->work + 0x18060. */
typedef struct BtlMoveContact {
    /* 0x00 */ Vec4 unk0;
    /* 0x10 */ Vec4 normal;
} BtlMoveContact;

/* Segment for the stage line test (ColSeg_Set builds it, StgCol_TraceSegment tests it, StgCol_GetHitPos gives the hit). */
typedef struct BtlMoveSeg {
    /* 0x00 */ Vec4 a;
    /* 0x10 */ Vec4 b;
} BtlMoveSeg;

/* One record of the blast list (EftHit_GetList: 64 records and a count at +0x6400). */
typedef struct BtlMoveBlastDef {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s16 ownerId;  /* object id of the fighter that fired it */
    /* 0x12 */ u8 unk12[0x18 - 0x12];
    /* 0x18 */ s16 kind;     /* 0, 4, 8 form one class, everything else the other */
} BtlMoveBlastDef;

typedef struct BtlMoveBlastSrc {
    /* 0x0 */ s32 ownerId;
    /* 0x4 */ s32 unk4;      /* 0 / 1: not counted as incoming */
} BtlMoveBlastSrc;

typedef struct BtlMoveBlastRec {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ s32 type;     /* 0: described by def, otherwise by src */
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ Vec4 pos;
    /* 0x30 */ Vec4 prevPos;
    /* 0x40 */ u8 unk40[0x14];
    /* 0x54 */ s32 unk54;    /* 1: ignored */
    /* 0x58 */ u8 unk58[0xC];
    /* 0x64 */ BtlMoveBlastSrc *src;
    /* 0x68 */ BtlMoveBlastDef *def;
    /* 0x6C */ u8 unk6C[0x190 - 0x6C];
} BtlMoveBlastRec; /* size 0x190 */

typedef struct BtlMoveBlastList {
    /* 0x0000 */ BtlMoveBlastRec rec[64];
    /* 0x6400 */ s32 count;
} BtlMoveBlastList;

/* Modes of BtlMove_TurnYaw. */
#define BTL_YAW_STICK        0 /* atan2f(stickX, -stickY) + camYaw, while a direction is held */
#define BTL_YAW_STICK_RATE   1 /* yaw + maxStep * stickX */
#define BTL_YAW_OPPONENT     2 /* lock-on: towards the opponent */
#define BTL_YAW_AWAY         3 /* lock-on: away from the opponent */
#define BTL_YAW_OPPONENT2    4 /* same as 2 */
#define BTL_YAW_KEEP         6 /* 5 and 6: unchanged */
#define BTL_YAW_CENTRE       7 /* towards the stage centre */
#define BTL_YAW_FROM_CENTRE  8 /* away from the stage centre */
#define BTL_YAW_AS_OPPONENT  9 /* the opponent's yaw */

/* Modes of BtlMove_SetDirection. */
#define BTL_DIR_STICK        0 /* stick rotated by camYaw, horizontal */
#define BTL_DIR_STICK_PITCH  1 /* the same, tilted by the heading pitch */
#define BTL_DIR_YAW          2 /* heading yaw, horizontal */
#define BTL_DIR_HEADING      3 /* heading yaw and pitch */
#define BTL_DIR_BACK_YAW     4 /* reverse of 2 */
#define BTL_DIR_BACK_HEADING 5 /* reverse of 3 */
#define BTL_DIR_KEEP_FLAT    6 /* keep, y removed */
#define BTL_DIR_KEEP         7 /* keep */

void BtlMove_UpdateAnimVoice(BtlMoveChr *chr);
void BtlMove_ResetAnimStep(BtlMoveChr *chr);
void BtlMove_UpdateAction(BtlMoveChr *chr);
s32 BtlMove_TestContact(BtlMoveChr *chr, s32 floor, f32 angle);
s32 BtlMove_IsHeadingIntoFloor(BtlMoveChr *chr, f32 angle);
s32 BtlMove_IsHeadingIntoWall(BtlMoveChr *chr, f32 angle);
s32 BtlMove_CanFireBlast(BtlMoveChr *chr, s32 mode, s32 *outCount);
s32 BtlMove_IsBlastIncoming(BtlMoveChr *chr, s32 a, s32 b, s32 c, f32 limit);
s32 BtlMove_CountOwnBeams(BtlMoveChr *chr);
s32 BtlMove_ClipToRadius(Vec4 *out, Vec4 *from, Vec4 *to, f32 radius);
void BtlMove_TurnYaw(BtlMoveChr *chr, s32 mode, f32 maxStep);
void BtlMove_TurnPitch(BtlMoveChr *chr, s32 mode, f32 maxStep);
void BtlMove_SteerAtOpponent(BtlMoveChr *chr, f32 closeSpeed, f32 yawAccel, f32 pitchAccel, f32 yawMax, f32 pitchMax, f32 maxStep);
void BtlMove_TurnModelYaw(BtlMoveChr *chr, f32 maxStep, f32 rate);
void BtlMove_SetHeading(BtlMoveChr *chr, f32 yaw, f32 pitch);
void BtlMove_TurnToPoint(BtlMoveChr *chr, Vec4 *target, f32 yawStep, f32 pitchStep);
void BtlMove_SetDirection(BtlMoveChr *chr, s32 mode);
void BtlMove_Advance(BtlMoveChr *chr, f32 speed, f32 accel);
void BtlMove_Step(BtlMoveChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel);
void BtlMove_WarpAheadOfOpponent(BtlMoveChr *chr, f32 lead);
void BtlMove_WarpBehindOpponent(BtlMoveChr *chr, f32 gap);
void BtlMove_WarpInFrontOfOpponent(BtlMoveChr *chr, f32 gap);
void BtlMove_SnapToOpponent(BtlMoveChr *chr);
s32 BtlMove_CircleOpponent(BtlMoveChr *chr, s32 side, s32 lead, f32 baseYaw, f32 speed);
void BtlMove_CalcApproachPoint(BtlMoveChr *chr, Vec4 *out, Vec4 *outTarget, f32 dist, f32 angle, f32 distScale, f32 lead);
void BtlMove_Fall(BtlMoveChr *chr);
void BtlMove_MoveVertical(BtlMoveChr *chr, f32 speed, f32 accel);
void BtlMove_BrakeVertical(BtlMoveChr *chr);
void BtlMove_ApplyGravity(BtlMoveChr *chr);
void BtlMove_SetLeanX(BtlMoveChr *chr, f32 v);
void BtlMove_SetLeanZ(BtlMoveChr *chr, f32 v);
s32 BtlMove_IsBlockedByOpponent(BtlMoveChr *chr);
void BtlMove_ClampToStage(BtlMoveChr *chr);
void BtlMove_RequestOrbit(BtlMoveChr *chr, f32 near, f32 far);
void BtlMove_ApplyOrbit(BtlMoveChr *chr);
s32 BtlMove_PushOut(BtlMoveChr *chr);
f32 BtlMove_CalcVerticalSpeed(f32 height, f32 extra);
void BtlMove_AimVerticalAtOpponent(BtlMoveChr *chr, s32 lead);
void BtlMove_BeginRiseToOpponent(BtlMoveChr *chr);
f32 BtlMove_CalcJumpSpeed(f32 height);
void BtlMove_SetImpulse(BtlMoveChr *chr, Vec4 *vel);
void BtlMove_SetImpulseDir(BtlMoveChr *chr, Vec4 *dir, f32 speed);
void BtlMove_SetImpulseYaw(BtlMoveChr *chr, f32 yaw, f32 speed);
void BtlMove_DeflectAtStageLimit(BtlMoveChr *chr, s32 back);
s32 BtlMove_ApplyImpulse(BtlMoveChr *chr);
void BtlMove_UpdateHoverOffset(BtlMoveChr *chr);
void BtlMove_UpdateDefenseTimers(BtlMoveChr *chr);

#endif
