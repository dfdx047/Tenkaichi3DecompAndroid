#ifndef BATTLE_BTL_ACT_C_H
#define BATTLE_BTL_ACT_C_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter action handlers, third slice (src/battle/btl_act_c.c, 0x1EA5F8..0x1EE058).
 * Handler protocol: include/battle/btl_char_action.h. The structures below are partial views
 * local to this module.
 */

/* Pose block (chr + 0x10, what BtlChar_GetPos returns). */
typedef struct BtlActCPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;
    /* 0x20 */ u8 unk20[0x40 - 0x20];
    /* 0x40 */ Vec4 move;      /* y = fall speed */
    /* 0x50 */ u8 unk50[0x70 - 0x50];
    /* 0x70 */ Vec4 unk70;
    /* 0x80 */ Vec4 vel;
    /* 0x90 */ f32 speed;
    /* 0x94 */ f32 facing;
    /* 0x98 */ f32 unk98;
    /* 0x9C */ f32 unk9C;
    /* 0xA0 */ f32 unkA0;
    /* 0xA4 */ f32 unkA4;
    /* 0xA8 */ f32 unkA8;
    /* 0xAC */ f32 unkAC;
    /* 0xB0 */ f32 unkB0;
    /* 0xB4 */ f32 groundY;
} BtlActCPose;

/* Active member's gauge block (what BtlMember_GetActiveGauge returns). */
typedef struct BtlActCGauge {
    /* 0x00 */ s32 hp;
    /* 0x04 */ s32 hpMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ s32 blast;
    /* 0x18 */ s32 blastMax;
    /* 0x1C */ s32 maxPower;
    /* 0x20 */ s32 unk20;     /* non-zero: getting up (0xE1) plays voice 0x20 / 0x21 once */
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;     /* non-zero: a downed fighter must mash to recover */
    /* 0x2C */ s32 unk2C;     /* that voice was played */
    /* 0x30 */ s32 unk30;
} BtlActCGauge;

/* Member entry (what BtlMember_GetActive returns). */
typedef struct BtlActCMember {
    /* 0x00 */ u8 unk0[0xA0];
    /* 0xA0 */ s32 unkA0;     /* counts action 0x43 */
} BtlActCMember;

/* Fighter (0x1600 bytes). */
typedef struct BtlActCChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[0x14];   /* per-action scratch: [0] bit 0 = dash blast steers at the opponent; [2] = counters of 0x38 / 0x3B, block of 0xEB */
    /* 0x0420 */ u8 unk420[0x4A8 - 0x420];
    /* 0x04A8 */ f32 camSide;       /* ChrCam side: its sign picks the camera cut of action 0x41 */
    /* 0x04AC */ u8 unk4AC[0x964 - 0x4AC];
    /* 0x0964 */ s32 actionFrame;   /* frames in the current action */
    /* 0x0968 */ u8 unk968[0xD44 - 0x968];
    /* 0x0D44 */ s32 unkD44;        /* must reach 10 before a downed fighter may recover */
    /* 0x0D48 */ u8 unkD48[0xD58 - 0xD48];
    /* 0x0D58 */ f32 unkD58;        /* search window half angle (0.5 .. pi) */
    /* 0x0D5C */ f32 unkD5C;        /* search window range (500 .. 1000000) */
    /* 0x0D60 */ u8 unkD60[0xD8C - 0xD60];
    /* 0x0D8C */ s32 unkD8C;        /* action 0x41: which of the three animations is next */
    /* 0x0D90 */ u8 unkD90[0xDD0 - 0xD90];
    /* 0x0DD0 */ Vec4 unkDD0;       /* ki blast aim direction */
    /* 0x0DE0 */ s32 unkDE0;        /* ki blast counter */
    /* 0x0DE4 */ s32 unkDE4;        /* set to 30 by every ki blast (a countdown, see btl_char_action.h) */
    /* 0x0DE8 */ s32 unkDE8;        /* ki blast: 1 = the next one uses the other hand */
    /* 0x0DEC */ f32 unkDEC;        /* charged ki blast: progress of the hold animation */
    /* 0x0DF0 */ s32 unkDF0;        /* charged ki blast: model node it leaves from */
    /* 0x0DF4 */ u8 unkDF4[0xFE0 - 0xDF4];
    /* 0x0FE0 */ s32 unkFE0;        /* stun countdown */
    /* 0x0FE4 */ u8 unkFE4[0x1000 - 0xFE4];
    /* 0x1000 */ s32 unk1000;       /* downed: button presses still needed before recovering */
    /* 0x1004 */ u8 unk1004[0x1600 - 0x1004];
} BtlActCChr; /* size 0x1600 */

s32 BtlAct_DownHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_GetUpHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_ActionE6(BtlActCChr *chr, s32 phase);
s32 BtlAct_ActionE7(BtlActCChr *chr, s32 phase);
s32 BtlAct_ActionE8E9(BtlActCChr *chr, s32 phase);
s32 BtlAct_ActionEA(BtlActCChr *chr, s32 phase);
s32 BtlAct_ActionEB(BtlActCChr *chr, s32 phase);
s32 BtlAct_StunHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_AirStunHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_GrowSearchWindow(BtlActCChr *chr, f32 limit);
s32 BtlAct_IsOppInSearchWindow(BtlActCChr *chr);
s32 BtlAct_SearchHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_ChargeHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_GuardHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action39(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action3A(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action3B(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action3C(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action3D(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action3E(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action3F(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action40(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action41(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action42(BtlActCChr *chr, s32 phase);
s32 BtlAct_Action43(BtlActCChr *chr, s32 phase);
s32 BtlAct_PlaySubAnim(BtlActCChr *chr, s32 anim);
s32 BtlAct_KiBlastHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_ChargedKiBlastHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_DashKiBlastHandler(BtlActCChr *chr, s32 phase);
s32 BtlAct_DashChargedKiBlastHandler(BtlActCChr *chr, s32 phase);

#endif
