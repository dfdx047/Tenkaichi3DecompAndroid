#ifndef BATTLE_BTL_ACT_J_H
#define BATTLE_BTL_ACT_J_H

#include "types.h"
#include "sys/math3d.h"

/*
 * End of the fighter action decision code (src/battle/btl_act_j.c, 0x203168..0x204E78): transformation, fusion,
 * member switch, technique (blast 2 / ultimate) and skill (blast 1) requests, the hit reaction -> action table,
 * the story mode forced actions, and a handful of "which action id" selectors used by the handlers.
 *
 * Every "Check..." / "Queue..." function here only writes the follow-up queue (BtlAct_SetQueue) and returns how
 * many slots it wrote (0 = nothing); the caller makes the request with BtlAct_Request(chr, BtlAct_GetQueued(chr)).
 *
 * All structures are partial views local to this module.
 */

/* Pose block (chr + 0x10, BtlChar_GetPos). */
typedef struct BtlActJPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;       /* y = model yaw */
} BtlActJPose;

/* First words of a member entry (BtlMember_Get / BtlMember_GetActive). */
typedef struct BtlActJMember {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 costume;
} BtlActJMember;

/* Gauge block of a member (BtlMember_GetGauge / BtlMember_GetActiveGauge). */
typedef struct BtlActJGauge {
    /* 0x00 */ s32 health;
    /* 0x04 */ u8 unk4[0x20 - 0x4];
    /* 0x20 */ s32 variant;
    /* 0x24 */ u8 unk24[0x30 - 0x24];
    /* 0x30 */ s32 unk30;        /* non-zero: this member cannot be fused with */
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 skillLock[2]; /* > 0: skill slot 0 / 1 (blast 1) with flag 0x10 is locked */
    /* 0x40 */ u8 unk40[0x4C - 0x40];
    /* 0x4C */ s32 techLock[5];  /* indexed by technique slot (2..4 -> + 0x54..0x5C): > 0 = a flag-0x2000 technique is locked */
} BtlActJGauge;

/* Form-change request (chr + 0x12CC), the same block btl_char_action.h calls BtlActForm. */
typedef struct BtlActJForm {
    /* 0x00 [0x12CC] */ s32 index;      /* transformation index 0..3 / fusion index 0..2 */
    /* 0x04 [0x12D0] */ s32 chara;      /* character to become */
    /* 0x08 [0x12D4] */ s32 costume;    /* transformation: the active member's costume; fusion: 0 */
    /* 0x0C [0x12D8] */ s32 cost;       /* blast gauge cost (stocks * 100000) */
    /* 0x10 [0x12DC] */ s32 kind;       /* transformation: parameter byte + 0xA4; fusion: 5 */
    /* 0x14 [0x12E0] */ s32 variant;    /* active gauge + 0x20, or 0 when parameter flag (0x10 << index) is set */
    /* 0x18 [0x12E4] */ s32 animChara;  /* = chara */
    /* 0x1C [0x12E8] */ s32 unk1C;      /* = chara */
    /* 0x20 [0x12EC] */ s32 voiceChara; /* = chara */
    /* 0x24 [0x12F0] */ s32 unk24;      /* parameter byte + 0xA8 (transformation) / + 0xB7 (fusion) */
    /* 0x28 [0x12F4] */ s32 unk28;      /* transformation: 0, or 2 when the active character is 0x6A; fusion: the partner's costume */
    /* 0x2C [0x12F8] */ s32 partner;    /* fusion: member index of the partner; transformation: -1 */
    /* 0x30 [0x12FC] */ s32 unk30;
    /* 0x34 [0x1300] */ s32 allowed;    /* 0 = transformations, fusions and switches are refused (unless flag 0x10F) */
} BtlActJForm;

/* Fighter (0x1600 bytes). */
typedef struct BtlActJChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x4A0 - 0x10];
    /* 0x04A0 */ f32 camYaw;         /* ChrCam yaw */
    /* 0x04A4 */ u8 unk4A4[0x99C - 0x4A4];
    /* 0x099C */ s32 switchGauge;    /* 0..100000: a member switch needs it full */
    /* 0x09A0 */ u8 unk9A0[0xD88 - 0x9A0];
    /* 0x0D88 */ s32 unkD88;
    /* 0x0D8C */ s32 unkD8C;
    /* 0x0D90 */ s32 attackId;       /* id BtlAct_PrepareAttack was last called with */
    /* 0x0D94 */ u8 unkD94[0xE00 - 0xD94];
    /* 0x0E00 */ s32 dodges;         /* automatic evasions left (first kind) */
    /* 0x0E04 */ s32 dodgesB;        /* automatic evasions left (second kind) */
    /* 0x0E08 */ u8 unkE08[0xE40 - 0xE08];
    /* 0x0E40 */ s32 techDelay;      /* countdown: no technique input while > 0 */
    /* 0x0E44 */ u8 unkE44[0xE5C - 0xE44];
    /* 0x0E5C */ s32 unkE5C;         /* counter, full at 3 */
    /* 0x0E60 */ u8 unkE60[0xE9C - 0xE60];
    /* 0x0E9C */ s32 unkE9C;         /* 0..2: variant added to the actions of reactions 29, 30 and 34 */
    /* 0x0EA0 */ u8 unkEA0[0xFB0 - 0xEA0];
    /* 0x0FB0 */ s32 reaction;       /* HitReact.reaction */
    /* 0x0FB4 */ s32 unkFB4;
    /* 0x0FB8 */ s32 unkFB8;
    /* 0x0FBC */ s32 hitBack;        /* HitReact.back: hit from behind */
    /* 0x0FC0 */ u8 unkFC0[0xFCC - 0xFC0];
    /* 0x0FCC */ f32 hitYaw;         /* HitReact.yaw: direction the hit pushes */
    /* 0x0FD0 */ f32 turnYaw;        /* HitReact.turnYaw */
    /* 0x0FD4 */ s32 turnPitch;      /* HitReact.turnPitch (cleared with an integer store) */
    /* 0x0FD8 */ u8 unkFD8[0xFE0 - 0xFD8];
    /* 0x0FE0 */ s32 stunTime;       /* countdown shortened by mashing; > 0 = not free */
    /* 0x0FE4 */ u8 unkFE4[0xFF4 - 0xFE4];
    /* 0x0FF4 */ s32 unkFF4;         /* 0..2: variant added to the action of reaction 41 */
    /* 0x0FF8 */ u8 unkFF8[0x1004 - 0xFF8];
    /* 0x1004 */ s8 reactCount[0x32]; /* per reaction id: times it was taken, saturating at 100 */
    /* 0x1036 */ s8 reactTimer[0x32]; /* per reaction id: zeroed when the reaction is taken */
    /* 0x1068 */ u8 unk1068[0x12CC - 0x1068];
    /* 0x12CC */ BtlActJForm form;
    /* 0x1304 */ u8 unk1304[0x1600 - 0x1304];
} BtlActJChr; /* size 0x1600 */

s32 BtlAct_QueueComboFinish(BtlActJChr *chr);
s32 BtlAct_CanTransform(BtlActJChr *chr, u32 index, s32 needBlast, s32 needAllowed);
s32 BtlAct_CheckTransformInput(BtlActJChr *chr);
s32 BtlAct_QueueTransform(BtlActJChr *chr, s32 index);
s32 BtlAct_CanFuse(BtlActJChr *chr, s32 index, s32 needBlast, s32 needAllowed, s32 *partner);
s32 BtlAct_CheckFusionInput(BtlActJChr *chr);
s32 BtlAct_QueueFusion(BtlActJChr *chr, s32 index, s32 partner);
s32 BtlAct_CanSwitch(BtlActJChr *chr, s32 needGauge, s32 needAllowed);
s32 BtlAct_CheckSwitchInput(BtlActJChr *chr);
s32 BtlAct_QueueSwitch(BtlActJChr *chr);
s32 BtlAct_GetTechniqueAction(BtlActJChr *chr, s32 slot);
s32 BtlAct_CanUseTechnique(BtlActJChr *chr, s32 slot);
s32 BtlAct_CheckTechniqueInput(BtlActJChr *chr);
s32 BtlAct_GetSkillAction(BtlActJChr *chr, s32 slot);
s32 BtlAct_CanUseSkill(BtlActJChr *chr, u32 slot);
s32 BtlAct_CheckSkillInput(BtlActJChr *chr);
s32 BtlAct_QueueReaction(BtlActJChr *chr, u32 reaction);
s32 BtlAct_GetLandingAction(BtlActJChr *chr, s32 arg1, u32 kind);
s32 BtlAct_QueueFlag21Action(BtlActJChr *chr);
s32 BtlAct_QueueFlag21ActionB(BtlActJChr *chr);
s32 BtlAct_CheckRecoveryInput(BtlActJChr *chr, s32 held);
s32 BtlAct_CheckStoryForced(BtlActJChr *chr);
s32 BtlAct_GetDownAction(BtlActJChr *chr);
s32 BtlAct_GetEvasionAttack(BtlActJChr *chr);
s32 BtlAct_GetChainAction(BtlActJChr *chr, u32 kind);
s32 BtlAct_GetIdleFollowUp(BtlActJChr *chr);
s32 BtlAct_GetAction14or16(BtlActJChr *chr);
s32 BtlAct_GetAction11or15(BtlActJChr *chr);

#endif
