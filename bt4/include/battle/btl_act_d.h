#ifndef BATTLE_BTL_ACT_D_H
#define BATTLE_BTL_ACT_D_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter action handlers, 0x1EE058..0x1F1930 (src/battle/btl_act_d.c).
 * The structures are partial views local to this module.
 */

/* Pose block (chr + 0x10, what BtlChar_GetPos returns); names as in btl_char_move.h. */
typedef struct BtlActDPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;
    /* 0x20 */ u8 unk20[0x40 - 0x20];
    /* 0x40 */ Vec4 unk40;     /* whole movement of the previous frame (btl_char_move.h); its length is the real speed */
    /* 0x50 */ u8 unk50[0x80 - 0x50];
    /* 0x80 */ Vec4 dir;       /* unit direction of travel */
    /* 0x90 */ f32 pitch;      /* heading pitch */
    /* 0x94 */ f32 yaw;        /* heading yaw */
    /* 0x98 */ f32 speed;      /* along dir, per frame */
    /* 0x9C */ f32 fallSpeed;  /* vertical speed, positive = down */
    /* 0xA0 */ f32 leanX;
    /* 0xA4 */ f32 unkA4;
    /* 0xA8 */ f32 unkA8;
    /* 0xAC */ f32 unkAC;
    /* 0xB0 */ f32 unkB0;
    /* 0xB4 */ f32 groundY;
    /* 0xB8 */ u8 unkB8[0xD0 - 0xB8];
    /* 0xD0 */ s32 unkD0;      /* bit word: 0x40 blocks the landing of action 0x15; 0x40000000 without 0x01000000
                                  keeps the fast descend going near the ground */
} BtlActDPose;

/* Latched attack parameters (chr + 0xCF8), see BtlActAttack in btl_char_action.h. */
typedef struct BtlActDAttack {
    /* 0x00 */ s32 flags;
    /* 0x04 */ s16 motion;
    /* 0x06 */ s16 motion2;
    /* 0x08 */ s8 parts;
    /* 0x09 */ s8 unk9;
    /* 0x0A */ s8 cut;
    /* 0x0B */ s8 follow[2];
    /* 0x0D */ u8 padD[3];
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ s32 unk20;
} BtlActDAttack; /* size 0x24 */

/* Active member's gauge block (BtlMember_GetActiveGauge). */
typedef struct BtlActDGauge {
    /* 0x00 */ s32 hp;
    /* 0x04 */ s32 hpMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ u8 unk14[0x28 - 0x14];
    /* 0x28 */ s32 unk28;      /* non-zero: second idle motion, flag 0x96 every frame */
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
} BtlActDGauge;

/* Member entry (BtlMember_GetActive). */
typedef struct BtlActDMember {
    /* 0x00 */ u8 unk0[0xA0];
    /* 0xA0 */ s32 unkA0;      /* <= 0 lets the neutral state time out into action 0x43 */
} BtlActDMember;

/* Fighter (0x1600 bytes). */
typedef struct BtlActDChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[0x14];    /* per-action scratch, zeroed when an action starts */
    /* 0x0420 */ u8 unk420[0x948 - 0x420];
    /* 0x0948 */ s32 action;
    /* 0x094C */ s32 request;
    /* 0x0950 */ s32 prev;
    /* 0x0954 */ s32 queue[4];
    /* 0x0964 */ s32 actionFrame;
    /* 0x0968 */ u8 unk968[0xCF8 - 0x968];
    /* 0x0CF8 */ BtlActDAttack attack;
    /* 0x0D1C */ u8 unkD1C[0xD68 - 0xD1C];
    /* 0x0D68 */ s32 unkD68;       /* counter: homing dashes started out of an attack or actions 0x47..0x57, 0x5A..0x5D, 0x6B..0x6F */
    /* 0x0D6C */ u8 unkD6C[0xDD0 - 0xD6C];
    /* 0x0DD0 */ Vec4 aimDir;      /* direction a ki blast is fired in */
    /* 0x0DE0 */ s32 unkDE0;       /* ki blasts fired: += the shots of motion 0x7A, or 1 */
    /* 0x0DE4 */ s32 unkDE4;       /* set to 30 with it (countdown in BtlAct_UpdateTimers) */
    /* 0x0DE8 */ s32 unkDE8;
    /* 0x0DEC */ f32 unkDEC;       /* progress of the charge motion 0x8E */
    /* 0x0DF0 */ s32 unkDF0;       /* result of BtlAct_PlaySubAnim */
    /* 0x0DF4 */ u8 unkDF4[0xFF0 - 0xDF4];
    /* 0x0FF0 */ s32 unkFF0;       /* countdown (BtlAct_UpdateTimers); > 0 selects the quick start of actions 0x14, 0x15, 0x17, 0x18; cleared by a jump */
    /* 0x0FF4 */ u8 unkFF4[0x1600 - 0xFF4];
} BtlActDChr; /* size 0x1600 */

s32 BtlAct_KiBlastB2(BtlActDChr *chr, s32 phase);
s32 BtlAct_KiBlastChargeB3(BtlActDChr *chr, s32 phase);
s32 BtlAct_KiVolley95(BtlActDChr *chr, s32 phase);
void BtlAct_PlayLandFx(BtlActDChr *chr, s32 hard);
s32 BtlAct_GetIdleMotion(BtlActDChr *chr);
s32 BtlAct_NeutralHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_IdleOnceHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_MoveHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_CloseMoveHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_DashMoveHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_JumpStartHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_JumpAirHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_AscendHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_DescendHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_HopHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_FastAscendHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_FastDescendHandler(BtlActDChr *chr, s32 phase);
s32 BtlAct_HomingDashHandler(BtlActDChr *chr, s32 phase);

#endif
