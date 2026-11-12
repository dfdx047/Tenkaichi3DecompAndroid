#ifndef BATTLE_BTL_ACT_H_H
#define BATTLE_BTL_ACT_H_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter action handlers, 0x1FC2B0..0x1FFAC0 (src/battle/btl_act_h.c = 0x1FC2B0..0x1FC598,
 * src/battle/btl_act_h_b.c = 0x1FC598..0x1FFAC0):
 *   0xB4..0xBC  grab dash, throw (thrower and victim), slam throw (thrower and victim)
 *   0xBE, 0xBF  drag down (the one who drags and the one dragged)
 *   0xEC..0xF0  transformation (five presentations)
 *   0xF1, 0xF2  fusion
 *   0xF3        member switch, first action of the leaving member
 * plus the helpers the change actions share with btl_act_i.c.
 * The structures are partial views local to this module. The fighter is 0x1600 bytes.
 */

/* Pose block (BtlChar_GetPos, fighter + 0x10). */
typedef struct BtlActHPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;       /* y = yaw */
    /* 0x20 */ u8 unk20[0x60 - 0x20];
    /* 0x60 */ Vec4 rootRot;   /* animation root rotation */
    /* 0x70 */ u8 unk70[0x90 - 0x70];
    /* 0x90 */ f32 pitch;      /* second argument of BtlMove_SetHeading as kept in the pose (btl_char_ctl.h: speed) */
    /* 0x94 */ f32 facing;     /* yaw the fighter faces */
    /* 0x98 */ f32 unk98;      /* zeroed when a change action starts */
    /* 0x9C */ f32 unk9C;      /* zeroed when a change action starts */
} BtlActHPose;

/* What the last hit does to this fighter (fighter + 0xFB0; HitReact in btl_char_hit.h). */
typedef struct BtlActHReact {
    /* 0x00 [0xFB0] */ s32 reaction;
    /* 0x04 [0xFB4] */ s32 unk4;      /* passed to BtlAct_QueueReaction when the thrown fighter is released */
    /* 0x08 [0xFB8] */ s32 unk8;
    /* 0x0C [0xFBC] */ s32 back;      /* hit from behind */
    /* 0x10 [0xFC0] */ s32 unk10;     /* non-zero: the throw animations start without blending */
    /* 0x14 [0xFC4] */ s32 silent;    /* no hurt voice */
    /* 0x18 [0xFC8] */ u8 unk18[0x28 - 0x18];
    /* 0x28 [0xFD8] */ f32 launchA;   /* yaw offset of the release direction */
    /* 0x2C [0xFDC] */ f32 launchB;   /* pitch of the release direction */
} BtlActHReact; /* 0x30 */

/* Form-change request (fighter + 0x12CC; BtlActForm in btl_char_action.h, BtlActJForm in btl_act_j.h). */
typedef struct BtlActHForm {
    /* 0x00 [0x12CC] */ s32 index;
    /* 0x04 [0x12D0] */ s32 chara;       /* BtlChange_RequestChara arguments */
    /* 0x08 [0x12D4] */ s32 costume;
    /* 0x0C [0x12D8] */ s32 cost;        /* blast gauge taken when the model is in memory */
    /* 0x10 [0x12DC] */ s32 kind;        /* 3 / 4 select flag 0x133 / 0x132 in action 0xEE */
    /* 0x14 [0x12E0] */ s32 variant;
    /* 0x18 [0x12E4] */ s32 animChara;
    /* 0x1C [0x12E8] */ s32 unk1C;
    /* 0x20 [0x12EC] */ s32 voiceChara;
    /* 0x24 [0x12F0] */ s32 objId;       /* extra object (BtlChange_RequestObject): the partner / the prop */
    /* 0x28 [0x12F4] */ s32 objCostume;
    /* 0x2C [0x12F8] */ s32 partner;     /* fusion: member index of the partner */
    /* 0x30 [0x12FC] */ s32 objVariant;  /* fusion only */
} BtlActHForm; /* 0x34 */

/* Fighter (0x1600 bytes). */
typedef struct BtlActHChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[0x14];     /* per-action scratch, zeroed on every action switch */
    /* 0x0420 */ u8 unk420[0x4A8 - 0x420];
    /* 0x04A8 */ f32 camSide;        /* fighter camera: sign = side of the fighter the camera sits on */
    /* 0x04AC */ u8 unk4AC[0x964 - 0x4AC];
    /* 0x0964 */ s32 actionFrame;    /* frames spent in the current action; does not advance while time is stopped */
    /* 0x0968 */ u8 unk968[0x970 - 0x968];
    /* 0x0970 */ s32 holdAction;     /* 1 = skip next frame's forced-action check */
    /* 0x0974 */ u8 unk974[0xCDC - 0x974];
    /* 0x0CDC */ s32 switchChara;    /* member switch target: the arguments of BtlChange_RequestChara */
    /* 0x0CE0 */ s32 switchCostume;
    /* 0x0CE4 */ s32 switchVariant;
    /* 0x0CE8 */ s32 switchAnimChara;
    /* 0x0CEC */ s32 switchUnk18;
    /* 0x0CF0 */ s32 switchVoiceChara;
    /* 0x0CF4 */ u8 unkCF4[0xDC0 - 0xCF4];
    /* 0x0DC0 */ s32 drainFrom;      /* first animation frame of the throw's drain window, < 0 = none */
    /* 0x0DC4 */ s32 drainTo;        /* last frame of it */
    /* 0x0DC8 */ u8 unkDC8[0xE60 - 0xDC8];
    /* 0x0E60 */ s32 unkE60;         /* counter 0..10, +1 per throw by character 0x6E */
    /* 0x0E64 */ u8 unkE64[0xFB0 - 0xE64];
    /* 0x0FB0 */ BtlActHReact react;
    /* 0x0FE0 */ u8 unkFE0[0x12CC - 0xFE0];
    /* 0x12CC */ BtlActHForm form;
    /* 0x1300 */ u8 unk1300[0x1600 - 0x1300];
} BtlActHChr; /* 0x1600 */

/* Animation data of a battle object (object + 0xB40 points at it). */
typedef struct BtlActHTrack {
    /* 0x00 */ u16 flags;      /* bit 0: the first key is at +4, else at +0x14 */
    /* 0x02 */ u16 count;
    /* 0x04 */ u32 key[6];     /* packed quaternions, two words each */
} BtlActHTrack;

typedef struct BtlActHAnim {
    /* 0x00 */ u8 unk0[6];
    /* 0x06 */ u16 rootTrack;  /* offset of the root rotation track in words, 0 = none */
} BtlActHAnim;

/* Battle object (BtlChar_GetObj). */
typedef struct BtlActHObj {
    /* 0x000 */ u8 unk0[0xB40];
    /* 0xB40 */ BtlActHAnim *anim;
} BtlActHObj;

/* Member entry (BtlMember_GetActive). */
typedef struct BtlActHMember {
    /* 0x00 */ s32 chara;
} BtlActHMember;

#define BTL_KMH(x) ((x) * 1000.0f / 3600.0f * (1.0f / 30.0f))
#define BTL_DEG(x) ((x) / 180.0f * 3.14159265f)

extern f32 sqrtf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Sin(f32 angle);
extern f32 Mathf_Cos(f32 angle);
extern f32 Mathf_Tan(f32 angle);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);

extern Vec4 D_002EC2A0; /* (0, 0, 0, 0) */

extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_LengthXZ(Vec4 *v);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);

extern BtlActHChr *BtlChar_Get(s32 player);
extern BtlActHPose *BtlChar_GetPos(BtlActHChr *chr);
extern BtlActHObj *BtlChar_GetObj(BtlActHChr *chr);
extern s32 BtlChar_TestFlag(BtlActHChr *chr, s32 n);
extern s32 BtlChar_TestPrevFlag(BtlActHChr *chr, s32 n);
extern void BtlChar_SetFlag(BtlActHChr *chr, s32 n);
extern void BtlChar_SetHeldFlag(BtlActHChr *chr, s32 n);
extern void BtlChar_SetFxBit(BtlActHChr *chr, s32 n);
extern void BtlChar_PlayVoice(BtlActHChr *chr, s32 kind);
extern void BtlChar_Vibrate(BtlActHChr *chr, f32 power, f32 seconds);
extern void BtlChar_SetVibration(BtlActHChr *chr, f32 power, f32 seconds);
extern void BtlChar_AddStageTimer(f32 seconds);
extern s32 BtlChar_IsDead(BtlActHChr *chr);
extern s32 BtlChar_IsStage4Or27(void);
extern s32 BtlChar_FrameMod(s32 n);
extern void BtlChar_SavePlacement(BtlActHChr *chr);
extern void BtlChar_SetSavedPlacement(BtlActHChr *chr, Vec4 *pos, Vec4 *rot, Vec4 *unk1310, s32 flagF, s32 flagE);
extern void BtlChar_ResetLook(BtlActHChr *chr);
extern void BtlCharSnd_PlayCommon(BtlActHChr *chr, s32 id);
extern void BtlCharSnd_PlayStream(BtlActHChr *chr, s32 id);
extern void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 arg3, f32 arg4);
extern void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
extern void BtlEvent_Raise(s32 side, s32 ev);

extern void ChrCam_RequestCut(BtlActHChr *chr, s32 arg1, s32 arg2);
extern void ChrCam_EndCut(BtlActHChr *chr);
extern s32 ChrCam_IsCutActive(BtlActHChr *chr);
extern void ChrCam_AddShake(BtlActHChr *chr, f32 strength, f32 time);
extern void ChrCam_SetCut(BtlActHChr *chr, Vec4 *vecA, Vec4 *vecADelta, Vec4 *vecB, Vec4 *vecBDelta, Vec4 *vecC,
                          Vec4 *vecCDelta, s32 unk88, f32 valA, f32 valADelta, f32 valB, f32 valBDelta, f32 valC,
                          f32 valCDelta, s32 unk8C, s32 unk90, s32 unk94, s32 time, s32 flags);

extern void BtlAnim_Play(BtlActHChr *chr, s32 anim, f32 blend);
extern void BtlAnim_PlaySub(BtlActHChr *chr, s32 anim);
extern void BtlAnim_Request(BtlActHChr *chr, s32 anim, f32 blend);
extern void BtlAnim_RequestSubToMain(BtlActHChr *chr);
extern void BtlAnim_SetBlend(BtlActHChr *chr, f32 blend);
extern void BtlAnim_SetObjRate(BtlActHChr *chr, f32 rate);
extern s32 BtlAnim_GetId(BtlActHChr *chr);
extern f32 BtlAnim_GetFrame(BtlActHChr *chr);
extern f32 BtlAnim_GetProgress(BtlActHChr *chr);
extern s32 BtlAnim_Advance(BtlActHChr *chr, s32 flags);
extern s32 BtlAnim_AdvanceThen(BtlActHChr *chr, s32 next, f32 blend, s32 flags);
extern void BtlAnim_AdvanceLoop(BtlActHChr *chr, s32 flags);
extern s32 BtlAnim_PassedRatio(BtlActHChr *chr, f32 ratio);
extern s32 BtlAnim_PassedFrame(BtlActHChr *chr, f32 frame);
extern s32 BtlAnim_InFrameRange(BtlActHChr *chr, f32 lo, f32 hi);
extern s32 BtlAnim_IsNew(BtlActHChr *chr);
extern s32 BtlAnim_WasNew(BtlActHChr *chr);

extern void BtlMove_Step(BtlActHChr *chr, s32 yawMode, s32 pitchMode, s32 dirMode, f32 speed, f32 accel);
extern void BtlMove_TurnYaw(BtlActHChr *chr, s32 mode, f32 maxStep);
extern void BtlMove_SetHeading(BtlActHChr *chr, f32 yaw, f32 pitch);
extern void BtlMove_Fall(BtlActHChr *chr);
extern void BtlMove_MoveVertical(BtlActHChr *chr, f32 speed, f32 accel);
extern void BtlMove_BrakeVertical(BtlActHChr *chr);
extern void BtlMove_ApplyGravity(BtlActHChr *chr);
extern void BtlMove_SetLeanX(BtlActHChr *chr, f32 v);
extern void BtlMove_BeginRiseToOpponent(BtlActHChr *chr);

extern void BtlAct_Request(BtlActHChr *chr, s32 id);
extern s32 BtlAct_GetCurrent(BtlActHChr *chr);
extern s32 BtlAct_GetPrev(BtlActHChr *chr);
extern s32 BtlAct_GetQueued(BtlActHChr *chr);
extern f32 BtlAct_GetHeight(BtlActHChr *chr);
extern s32 BtlAct_IsAirMotion(BtlActHChr *chr, s32 useSaved);
extern void BtlAct_PushAngle(BtlActHChr *chr, f32 angle, f32 speed, f32 arg);

extern BtlActHMember *BtlMember_GetActive(BtlActHChr *chr);
extern s32 BtlMember_Damage(BtlActHChr *chr, s32 amount, s32 flags);
extern void BtlMember_SubBlast(BtlActHChr *chr, s32 amount);
extern s32 BtlMember_HasAbilityOf(BtlActHChr *chr, s32 member, s32 n);

extern s32 BtlOpp_GetPlayer(BtlActHChr *chr);
extern s32 BtlOpp_GetSeenAction(BtlActHChr *chr);
extern s32 BtlOpp_GetParamWord0(BtlActHChr *chr);
extern f32 BtlOpp_GetHeight(BtlActHChr *chr);
extern void BtlOpp_GetTargetPos(BtlActHChr *chr, Vec4 *out);
extern void BtlOpp_GetPoseVec30(BtlActHChr *chr, Vec4 *out);

extern void BtlPartner_PlayAnim(BtlActHChr *chr, s32 anim);
extern s32 BtlPartner_StepAnim(BtlActHChr *chr);
extern void BtlPartner_Release(BtlActHChr *chr);
extern void BtlPartner_SetFlag80(BtlActHChr *chr, s32 on);

extern void BtlChange_RequestChara(s32 player, s32 chara, s32 costume, s32 variant, s32 animChara, s32 unk18, s32 voiceChara);
extern void BtlChange_RequestObject(s32 player, s32 id, s32 costume, s32 variant, s32 slot);
extern s32 BtlChange_IsLoadedFor(s32 player);
extern void BtlChange_SetReady(s32 player);
extern void BtlChange_SetDone(s32 player);

/* Not named yet (purposes read from their disassembly or from the call sites here). */
extern void BtlActThrow_SetupDamage(BtlActHChr *chr, s32 arg1);  /* thrower setup (previous file) */
extern void BtlActThrow_TurnByRootYaw(BtlActHChr *chr);            /* thrown fighter setup (previous file) */
extern void BtlAct_QueueReaction(BtlActHChr *chr, s32 arg1);  /* fills the action queue from a reaction word */
extern s32 BtlAct_CheckStoryForced(BtlActHChr *chr);             /* fills the action queue; 1 when it queued something */
extern f32 BtlCharApi_GetHeight(s32 objId);                   /* a size of the object (height) */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out); /* world position of a model node */
extern f32 BtlCharApi_GetRadius(s32 objId);                   /* a radius of the object */
extern u32 BtlAtk_GetFlags(BtlActHChr *chr);             /* flags of the current attack / motion */
extern f32 BtlAtk_GetLaunchAngleA(BtlActHChr *chr);             /* an angle to the opponent */
extern u32 BtlParam_GetFlags(BtlActHChr *chr);             /* character parameter flags */
extern s32 BtlParam_GetDashSound(BtlActHChr *chr);             /* common sound id */
extern f32 BtlMoveParam_GetSpeed(BtlActHChr *chr, s32 action); /* a speed for an action id */
extern f32 BtlStage_GetInnerRadius(void);                        /* stage radius */
extern f32 BtlStage_GetTop(void);                        /* stage height limit */
extern f32 BtlStage_GetBottom(void);                        /* stage floor level */
extern s32 BtlStage_GetWaterLevel(f32 *out);                    /* a stage height, 0 when the stage has none */
extern void BtlStage_GetStartPlace(s32 player, Vec4 *pos, Vec4 *rot, s32 arg3); /* start placement of a player */
extern void BtlStage_GetPlace(Vec4 *pos, Vec4 *rot);       /* a stage reference point */

void BtlActThrow_SetReleaseHeading(BtlActHChr *chr);
s32 BtlAct_GrabDash(BtlActHChr *chr, s32 phase);
s32 BtlAct_Throw(BtlActHChr *chr, s32 phase);
s32 BtlAct_Thrown(BtlActHChr *chr, s32 phase);
void BtlAct_SlamThrow(BtlActHChr *chr, s32 phase);
s32 BtlAct_SlamThrown(BtlActHChr *chr, s32 phase);
s32 BtlAct_DragDown(BtlActHChr *chr, s32 phase);
s32 BtlAct_DraggedDown(BtlActHChr *chr, s32 phase);
void BtlActChange_SetFlags(BtlActHChr *chr);
void BtlActChange_PushBack(BtlActHChr *chr);
void BtlActSwitch_SaveEntryPlacement(BtlActHChr *chr);
s32 BtlActSwitch_SetLeaveCut(BtlActHChr *chr);
void BtlActSwitch_SetEnterCut(BtlActHChr *chr);
void BtlActChange_Finish(BtlActHChr *chr);
s32 BtlAct_TransformA(BtlActHChr *chr, s32 phase);
s32 BtlAct_TransformB(BtlActHChr *chr, s32 phase);
s32 BtlAct_TransformC(BtlActHChr *chr, s32 phase);
s32 BtlAct_TransformD(BtlActHChr *chr, s32 phase);
s32 BtlAct_TransformE(BtlActHChr *chr, s32 phase);
s32 BtlAct_Fusion(BtlActHChr *chr, s32 phase);
s32 BtlAct_SwitchLeave(BtlActHChr *chr, s32 phase);

#endif
