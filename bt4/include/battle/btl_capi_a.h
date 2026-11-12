#ifndef BATTLE_BTL_CAPI_A_H
#define BATTLE_BTL_CAPI_A_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter interface, first part: 0x204E78..0x207020 (src/battle/btl_capi_a.c), 81 functions. The same original
 * source file continues in btl_char_api.c (0x207020) and btl_capi_b.c: see the header of btl_char_api.c for the
 * evidence. This is what the AI, the effect scene and its modules, the camera and the HUD call to read a fighter
 * without knowing its layout.
 *
 * Arguments named `objId` are battle object ids (BtlObj_Get index; a fighter's own id is at fighter +0xC, and in a
 * battle the two fighters are objects 0 and 1); arguments named `player` are roster indices (BtlChar_Get). Every
 * function is safe on an id that is not a fighter, or not an object, and then returns 0 / 0.0f / a zero vector /
 * an identity, except where the comment in the source gives another default.
 *
 * The structs are this module's partial views (no shared fighter header yet). Fields are named where another
 * module's matching code or this file's callers say what they are; the rest are unkXX.
 */

/* Pose block (fighter +0x10, what BtlChar_GetPos returns). Same layout as BtlMovePose in btl_char_move.h, where
   the writers are. */
typedef struct BtlCapiPose {
    /* 0x00 */ Vec4 pos;       /* world position */
    /* 0x10 */ Vec4 rot;       /* model rotation, y = model yaw */
    /* 0x20 */ Vec4 dispOfs;   /* display offset rebuilt every frame: hover bob (y) and shake (x, z) */
    /* 0x30 */ Vec4 vel;       /* movement of the previous frame between snapshots 0 and 1 */
    /* 0x40 */ Vec4 move;      /* whole movement of the previous frame */
    /* 0x50 */ Vec4 rootPos;   /* root motion position */
    /* 0x60 */ Vec4 rootRot;   /* root motion rotation */
    /* 0x70 */ Vec4 impulse;   /* knock-back velocity */
    /* 0x80 */ Vec4 dir;       /* unit direction of travel */
    /* 0x90 */ f32 pitch;      /* heading pitch */
    /* 0x94 */ f32 yaw;        /* heading yaw */
    /* 0x98 */ f32 speed;      /* per frame along dir */
    /* 0x9C */ f32 fallSpeed;  /* vertical speed, positive = down */
    /* 0xA0 */ u8 unkA0[0xB0 - 0xA0];
    /* 0xB0 */ Vec4 ground;    /* ground point below the fighter */
    /* 0xC0 */ u8 unkC0[0xF0 - 0xC0];
} BtlCapiPose; /* size 0xF0 */

/* Fighter (0x1600 bytes). */
typedef struct BtlCapiChr {
    /* 0x0000 */ s32 player;        /* roster index */
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;         /* battle object id */
    /* 0x0010 */ BtlCapiPose pose;
    /* 0x0100 */ u8 unk100[0xD78 - 0x100];
    /* 0x0D78 */ f32 charge;        /* 0..1: how far the current attack was charged (btl_tech_a.h) */
    /* 0x0D7C */ u8 unkD7C[0xDEC - 0xD7C];
    /* 0x0DEC */ f32 blastCharge;   /* 0..1: progress of the charged ki blast (btl_act_d.h) */
    /* 0x0DF0 */ u8 unkDF0[0xE90 - 0xDF0];
    /* 0x0E90 */ s32 thrTech;       /* +0xE90 is the throw / rush description (BtlCollThrow in btl_char_coll.h) */
    /* 0x0E94 */ s32 thrAtkSide;    /* player index of the attacker */
    /* 0x0E98 */ u8 unkE98[0xEA0 - 0xE98];
    /* 0x0EA0 */ s32 thrUnk10;      /* attacker: the marked rush step + 1 */
    /* 0x0EA4 */ u8 unkEA4[0xEAC - 0xEA4];
    /* 0x0EAC */ s32 thrUnk1C;      /* victim: the marked rush step, or -1 */
    /* 0x0EB0 */ u8 unkEB0[0xED8 - 0xEB0];
    /* 0x0ED8 */ s32 thrUnk48;      /* technique bit 0x2000 and attacker gauge word +0x20 == 0 */
    /* 0x0EDC */ s32 thrUnk4C;      /* technique bit 0x80000 and victim gauge word +0x20 == 0 */
    /* 0x0EE0 */ u8 unkEE0[0xFFC - 0xEE0];
    /* 0x0FFC */ s32 unkFFC;        /* follows twice the countdown at +0xFF8 by 15 per frame (BtlAct_UpdateTimers);
                                       BtlCharApi_GetBlindRatio gives it as a ratio of 90 */
    /* 0x1000 */ u8 unk1000[0x1330 - 0x1000];
    /* 0x1330 */ s32 partnerOn;     /* the fighter has a partner object (a second character model) */
    /* 0x1334 */ u8 unk1334[0x1338 - 0x1334];
    /* 0x1338 */ s32 partnerObjId;  /* its battle object id */
    /* 0x133C */ u8 unk133C[0x1600 - 0x133C];
} BtlCapiChr; /* size 0x1600 */

/* General parameters of the character (object +0x91C); BtlParam / BtlTechBParam in btl_tech_a.h / btl_tech_b.h. */
typedef struct BtlCapiParam {
    /* 0x00 */ u16 flags0;     /* bits tested by callers: 0x4, 0x8, 0x80 */
    /* 0x02 */ u8 unk2;
    /* 0x03 */ s8 auraType;
    /* 0x04 */ u8 unk4[0xC - 0x4];
    /* 0x0C */ f32 radius;     /* body radius; <= 0: object +0xFF0 * 1.8 is used */
    /* 0x10 */ s32 flags;      /* 0x40 / 0x80: see BtlCharApi_HasWeaponOut; 0x800: see BtlCharApi_TestStageBreakA */
    /* 0x14 */ u8 unk14[0xC8 - 0x14];
    /* 0xC8 */ f32 unkC8;
} BtlCapiParam;

/* One ki blast record (object +0x924, 13 of them); BtlKiBlastData in btl_tech_b.h. */
typedef struct BtlCapiKiBlast {
    /* 0x00 */ u8 unk0[2];
    /* 0x02 */ s8 type;
    /* 0x03 */ u8 unk3[0x34 - 3];
} BtlCapiKiBlast; /* size 0x34 */

/* Model node, as returned by BtlObj_GetNode(obj, node). */
typedef struct BtlCapiNode {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ Mtx44 mtx;      /* world matrix; its translation (+0x40) is the node position */
    /* 0x50 */ Mtx44 unk50;
    /* 0x90 */ Vec4 unk90;
    /* 0xA0 */ Quat rot;
} BtlCapiNode;

/* What BtlObj_FindBound(obj, n) returns. */
typedef struct BtlCapiPart {
    /* 0x00 */ u8 unk0[0x5C];
    /* 0x5C */ f32 unk5C;
} BtlCapiPart;

/* Battle object (BtlObj in btl_obj.h): only the fields read here. */
typedef struct BtlCapiObj {
    /* 0x000 */ u8 unk0[0xC];
    /* 0x00C */ s32 chara;          /* character id */
    /* 0x010 */ u8 unk10[0x58 - 0x10];
    /* 0x058 */ s32 unk58;
    /* 0x05C */ u8 unk5C[0x9C - 0x5C];
    /* 0x09C */ s32 unk9C[5];       /* five pointers of the model data block */
    /* 0x0B0 */ u8 unkB0[0x91C - 0xB0];
    /* 0x91C */ BtlCapiParam *param;
    /* 0x920 */ u8 unk920[4];
    /* 0x924 */ BtlCapiKiBlast *kiBlasts;
    /* 0x928 */ u8 unk928[4];
    /* 0x92C */ void *supers;       /* BtlSuperData (btl_tech_b.h) */
    /* 0x930 */ void *skills;       /* BtlSkillData */
    /* 0x934 */ u8 unk934[0x960 - 0x934];
    /* 0x960 */ Vec4 rot;
    /* 0x970 */ Vec4 pos;
    /* 0x980 */ u8 unk980[0xA40 - 0x980];
    /* 0xA40 */ union {
        u64 flags64;                /* the tests of two bits read all 64 bits */
        s32 flags;                  /* 2 drawn; 0x20, 0x40 see BtlCharApi_HasWeaponOut; 0x40000 forces aura type 3 */
    } u;
    /* 0xA48 */ u8 unkA48[0xFA0 - 0xA48];
    /* 0xFA0 */ Vec4 *bodyPos;      /* -> {Vec4 pos; f32 radius} (BtlMoveObjBody in btl_char_move.h) */
    /* 0xFA4 */ u8 unkFA4[0xFF0 - 0xFA4];
    /* 0xFF0 */ f32 unkFF0;         /* from the model's body data; * 1.8 = default radius */
    /* 0xFF4 */ f32 height;         /* model header +0x18 (BtlObjBody_Init copies the four) */
    /* 0xFF8 */ f32 unkFF8;         /* model header +0x1C */
    /* 0xFFC */ f32 centerHeight;   /* model header +0x20 */
    /* 0x1000 */ f32 unk1000;       /* model header +0x24 */
} BtlCapiObj;

/* Roster (gBtlChars): only the field read here. */
typedef struct BtlCapiMgr {
    /* 0x00 */ u8 unk0[0x5C];
    /* 0x5C */ f32 clashBias;       /* BtlClashA.bias (clash A state is at roster +0x50) */
} BtlCapiMgr;

s32 BtlCharApi_GetChara(s32 objId);
f32 BtlCharApi_GetHeight(s32 objId);
f32 BtlCharApi_GetBodyUnkFF8(s32 objId);
f32 BtlCharApi_GetCenterHeight(s32 objId);
f32 BtlCharApi_GetBodyUnk1000(s32 objId);
s32 BtlCharApi_ObjGetParamFlags0(s32 objId);
s32 BtlCharApi_ObjGetParamFlags(s32 objId);
s32 BtlCharApi_GetAuraType(s32 objId);
f32 BtlCharApi_GetParamUnkC8(s32 objId);
s32 BtlCharApi_GetCostume(s32 objId);
s32 BtlCharApi_HasKiBlastType2(s32 objId);
s32 BtlCharApi_HasKiBlastType3(s32 objId);
s32 BtlCharApi_IsFighter(s32 objId);
s32 BtlCharApi_GetObjIdOfPlayer(s32 player);
s32 BtlCharApi_GetPlayer(s32 objId);
s32 BtlCharApi_GetOpponentObjId(s32 objId);
s32 BtlCharApi_GetPartnerObjId(s32 objId);
s32 BtlCharApi_GetPlayerObjUnk9C(s32 player, u32 n);
void *BtlCharApi_GetPlayerSuperData(s32 player);
void *BtlCharApi_GetPlayerSkillData(s32 player);
s32 BtlCharApi_GetPlayerObjUnk58(s32 player);
void BtlCharApi_GetPos(s32 objId, Vec4 *out);
void BtlCharApi_GetBasePos(s32 objId, Vec4 *out);
void BtlCharApi_GetRot(s32 objId, Vec4 *out);
void BtlCharApi_GetModelRot(s32 objId, Vec4 *out);
f32 BtlCharApi_GetYaw(s32 objId);
void BtlCharApi_GetVelocity(s32 objId, Vec4 *out);
void BtlCharApi_GetFrameMove(s32 objId, Vec4 *out);
void BtlCharApi_GetDir(s32 objId, Vec4 *out);
f32 BtlCharApi_GetSpeed(s32 objId);
f32 BtlCharApi_GetFallSpeed(s32 objId);
f32 BtlCharApi_GetGroundY(s32 objId);
f32 BtlCharApi_GetAltitude(s32 objId);
void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
void BtlCharApi_GetNodeUnk90(s32 objId, s32 node, Vec4 *out);
void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out);
void BtlCharApi_GetNodeMtx50(s32 objId, s32 node, Mtx44 *out);
void BtlCharApi_GetNodeRot(s32 objId, s32 node, Vec4 *out);
void BtlCharApi_GetNodeQuat(s32 objId, s32 node, Quat *out);
f32 BtlCharApi_GetPartUnk5C(s32 objId, s32 n);
f32 BtlCharApi_GetCloseRange(s32 objId);
f32 BtlCharApi_ObjExistsF(s32 objId);
s32 BtlCharApi_IsClose(s32 objId);
s32 BtlCharApi_IsSightBlocked(s32 objId);
s32 BtlCharApi_IsOnGround(s32 objId);
s32 BtlCharApi_IsInWater(s32 objId);
s32 BtlCharApi_IsHidden(s32 objId);
s32 BtlCharApi_IsFrozen(s32 objId);
s32 BtlCharApi_AnyFrozen(void);
s32 BtlCharApi_TestStageBreakA(s32 objId);
s32 BtlCharApi_TestStageBreakB(s32 objId);
f32 BtlCharApi_GetChargeRate(s32 objId);
s32 BtlCharApi_GetActionFxKind(s32 objId);
f32 BtlCharApi_GetClashBias(void);
f32 BtlCharApi_GetBlindRatio(s32 objId);
s32 BtlCharApi_GetRushFinishPhase(s32 objId);
void BtlCharApi_ObjPushYaw(s32 objId, f32 yaw, f32 len, f32 arg);
void BtlCharApi_ObjPushDir(s32 objId, Vec4 *dir, f32 len, f32 arg);
f32 BtlCharApi_GetRadius(s32 objId);
void BtlCharApi_GetBodyPos(s32 objId, Vec4 *out);
void BtlCharApi_CalcAimDir(s32 objId, s32 node, Vec4 *from, Vec4 *out, f32 minPitch, f32 maxPitch);
void BtlCharApi_CalcAimDir45(s32 objId, Vec4 *from, Vec4 *out);
void BtlCharApi_GetDeflectDir(s32 objId, Vec4 *out);
s32 BtlCharApi_IsLockedOn(s32 objId);
s32 BtlCharApi_IsChargingKiBlast(s32 objId);
s32 BtlCharApi_IsInTechnique(s32 objId);
s32 BtlCharApi_EnteredTechnique(s32 objId);
s32 BtlCharApi_IsRushConnected(s32 objId);
s32 BtlCharApi_IsInRushSequence(s32 objId);
s32 BtlCharApi_IsInSkill(s32 objId);
s32 BtlCharApi_IsSkillStart(s32 objId);
s32 BtlCharApi_IsSkillApplied(s32 objId);
s32 BtlCharApi_IsInClashA(s32 objId);
s32 BtlCharApi_IsInClashBC(s32 objId);
s32 BtlCharApi_IsChargingKi(s32 objId);
s32 BtlCharApi_IsMaxPowerStart(s32 objId);
s32 BtlCharApi_IsChanging(s32 objId);
s32 BtlCharApi_TestAnimFlag10(s32 objId);
s32 BtlCharApi_IsCircleDash(s32 objId);
s32 BtlCharApi_IsModelNew(s32 objId);
s32 BtlCharApi_HasWeaponOut(s32 objId);

#endif
