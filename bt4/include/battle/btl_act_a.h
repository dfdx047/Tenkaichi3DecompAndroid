#ifndef BATTLE_BTL_ACT_A_H
#define BATTLE_BTL_ACT_A_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter action handlers, first group (src/battle/btl_act_a.c, 0x1E3158..0x1E6CC0).
 * The structures are partial views local to this module.
 */

/* Pose block (chr + 0x10, what BtlChar_GetPos returns). */
typedef struct BtlActAPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;
    /* 0x20 */ u8 unk20[0x80 - 0x20];
    /* 0x80 */ Vec4 dir;       /* unit travel direction */
    /* 0x90 */ f32 pitch;      /* heading pitch */
    /* 0x94 */ f32 yaw;        /* heading yaw */
    /* 0x98 */ f32 speed;
    /* 0x9C */ f32 vspeed;     /* vertical speed, positive = down */
} BtlActAPose;

/* Latched attack parameters (chr + 0xCF8); same layout as BtlActAttack in btl_char_action.h. */
typedef struct BtlActAAttack {
    /* 0x00 */ s32 flags;
    /* 0x04 */ s16 motion;
    /* 0x06 */ s16 motion2;
    /* 0x08 */ s8 parts;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ s8 cut;
    /* 0x0B */ s8 follow[2];
    /* 0x0D */ u8 padD[3];
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 cancelAt;   /* animation progress after which the queued follow-up starts */
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ s32 chained;    /* a follow-up attack (not a generic cancel) is queued */
} BtlActAAttack; /* size 0x24 */

/* Battle object (what BtlChar_GetObj returns). */
typedef struct BtlActAObj {
    /* 0x000 */ u8 unk0[0xBDC];
    /* 0xBDC */ f32 unkBDC;    /* a length in frames at 60 Hz, added to the vanish attack's lead */
    /* 0xBE0 */ u8 unkBE0[0xCAC - 0xBE0];
    /* 0xCAC */ s8 unkCAC;
    /* 0xCAD */ s8 unkCAD;
} BtlActAObj;

/* Fighter (0x1600 bytes). */
typedef struct BtlActAChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[6];       /* per-action scratch, zeroed when an action starts: [0] bit flags, [2] step, [3] frame count, [4] animation */
    /* 0x03E8 */ f32 workF[14];     /* [0] animation progress after which the queued action starts */
    /* 0x0420 */ u8 unk420[0x4A8 - 0x420];
    /* 0x04A8 */ f32 camSide;       /* ChrCam side: sign picks the left / right camera cut */
    /* 0x04AC */ u8 unk4AC[0x964 - 0x4AC];
    /* 0x0964 */ s32 actionFrame;   /* frames spent in the current action */
    /* 0x0968 */ u8 unk968[0xCF8 - 0x968];
    /* 0x0CF8 */ BtlActAAttack attack;
    /* 0x0D1C */ u8 unkD1C[0xD60 - 0xD1C];
    /* 0x0D60 */ s32 unkD60;        /* rush chain step 0..9 (animation 0x37 + step) */
    /* 0x0D64 */ s32 unkD64;        /* rapid rush step 0..4 (animation 0x3C + step) */
    /* 0x0D68 */ s32 unkD68;        /* > 0: the dash smash shows the charge effect at once */
    /* 0x0D6C */ s32 unkD6C;
    /* 0x0D70 */ s32 unkD70;        /* counts vanishing attacks */
    /* 0x0D74 */ s32 unkD74;
    /* 0x0D78 */ f32 unkD78;        /* attack charge level 0..1 */
    /* 0x0D7C */ f32 unkD7C;        /* attack charge timer 0..1 */
    /* 0x0D80 */ s32 unkD80;        /* gauge 0..100000 that blends the two charge rates */
    /* 0x0D84 */ s32 unkD84;        /* frames the charge has been full */
    /* 0x0D88 */ s32 unkD88;
    /* 0x0D8C */ s32 unkD8C;
    /* 0x0D90 */ s32 attackId;      /* set to the action id by the vanishing attack */
    /* 0x0D94 */ u8 unkD94[0x1354 - 0xD94];
    /* 0x1354 */ s32 unk1354;       /* model node for the effect of action 0x66 */
    /* 0x1358 */ u8 unk1358[0x1600 - 0x1358];
} BtlActAChr; /* size 0x1600 */

s32 BtlAct_WaitHandler(BtlActAChr *chr, s32 phase);
f32 BtlAct_GetChargeRate(BtlActAChr *chr);
s32 BtlAct_StepCharge(BtlActAChr *chr);
void BtlAct_SetChargeFromAnim(BtlActAChr *chr);
void BtlAct_SetChargeAnimDuration(BtlActAChr *chr);
void BtlAct_SetChargeAnimStep(BtlActAChr *chr);
void BtlAct_CancelOnOppC9(BtlActAChr *chr, f32 after);
void BtlAct_PlayPickedVoice(BtlActAChr *chr);
s32 BtlAct_StartSub19B(BtlActAChr *chr);
void BtlAct_DecideAttackFollow(BtlActAChr *chr, BtlActAAttack *atk);
void BtlAct_RequestAttackEnd(BtlActAChr *chr, BtlActAAttack *atk);
s32 BtlAct_RushHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_RushAutoHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_RushRapidHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_SmashChargeHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_SmashFullHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_DashSmashHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_FlyingKickHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_LiftStrikeHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_SmashVanishHandler(BtlActAChr *chr, s32 phase);
s32 BtlAct_RushFinishHandler(BtlActAChr *chr, s32 phase);

#endif
