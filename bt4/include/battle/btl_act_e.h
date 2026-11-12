#ifndef BATTLE_BTL_ACT_E_H
#define BATTLE_BTL_ACT_E_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter action handlers, actions 0x1A..0x35 and 0xFA..0xFC (src/battle/btl_act_e.c,
 * 0x1F1930..0x1F5460). The structures are partial views local to this module.
 * Handler protocol: see include/battle/btl_char_action.h.
 */

/* Pose block (chr + 0x10, what BtlChar_GetPos returns). */
typedef struct BtlActEPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;
    /* 0x20 */ u8 unk20[0x40 - 0x20];
    /* 0x40 */ Vec4 unk40;     /* its length is compared with a speed: the movement of this frame */
    /* 0x50 */ u8 unk50[0x80 - 0x50];
    /* 0x80 */ Vec4 vel;       /* direction of travel */
    /* 0x90 */ f32 speed;
    /* 0x94 */ f32 facing;
    /* 0x98 */ f32 unk98;      /* forward speed */
    /* 0x9C */ f32 unk9C;      /* vertical speed */
} BtlActEPose;

/* Active member's gauge block (BtlMember_GetActiveGauge). */
typedef struct BtlActEGauge {
    /* 0x00 */ s32 hp;
    /* 0x04 */ s32 hpMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ u8 unk14[0x28 - 0x14];
    /* 0x28 */ s32 unk28;
} BtlActEGauge;

/* Attack parameter block (see BtlActAttack in btl_char_action.h). */
typedef struct BtlActEAttack {
    /* 0x00 */ s32 flags;
    /* 0x04 */ s16 motion[2];  /* one motion per part */
    /* 0x08 */ s8 parts;
    /* 0x09 */ u8 unk9[7];
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;      /* lead time in frames, built up by BtlAct_PlayAttackPart */
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ s32 unk20;
} BtlActEAttack; /* size 0x24 */

/* Battle object of a fighter (BtlChar_GetObj). */
typedef struct BtlActEObj {
    /* 0x000 */ u8 unk0[0xBDC];
    /* 0xBDC */ f32 unkBDC;
} BtlActEObj;

/* Fighter (0x1600 bytes). */
typedef struct BtlActEChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[0x14];    /* per-action scratch, zeroed when an action starts */
    /* 0x0420 */ u8 unk420[0x964 - 0x420];
    /* 0x0964 */ s32 actionFrame;   /* frames spent in the current action */
    /* 0x0968 */ u8 unk968[0xCF8 - 0x968];
    /* 0x0CF8 */ BtlActEAttack attack;     /* latched when the attack action starts */
    /* 0x0D1C */ BtlActEAttack nextAttack; /* prepared when the attack is queued */
    /* 0x0D40 */ u8 unkD40[0xD48 - 0xD40];
    /* 0x0D48 */ s32 unkD48;        /* countdown (see btl_char_action.h); held at 30 during clash B */
    /* 0x0D4C */ u8 unkD4C[0xE50 - 0xD4C];
    /* 0x0E50 */ s32 clashCountB;   /* inputs counted in clashes B and C */
    /* 0x0E54 */ s32 unkE54;        /* consecutive clash C exchanges: picks the strike animation */
    /* 0x0E58 */ s32 unkE58;        /* clash C answer: -1 none yet, -2 wrong button, else the frame it was pressed */
    /* 0x0E5C */ u8 unkE5C[0xFF0 - 0xE5C];
    /* 0x0FF0 */ s32 unkFF0;
    /* 0x0FF4 */ u8 unkFF4[0x1278 - 0xFF4];
    /* 0x1278 */ s32 injected;      /* input is injected (CPU) */
    /* 0x127C */ u8 unk127C[0x1600 - 0x127C];
} BtlActEChr; /* size 0x1600 */

/* Clash C state (roster + 0x7C; see BtlClashC in btl_char_flag.h). */
typedef struct BtlActEClashC {
    /* 0x00 */ s32 level;
    /* 0x04 */ s32 count;
    /* 0x08 */ s32 path;
    /* 0x0C */ s32 point;
    /* 0x10 */ s32 hold;
    /* 0x14 */ s32 pick;      /* 0..3: which of the four buttons (bits 27..30) is asked for */
    /* 0x18 */ s32 prevPick;
} BtlActEClashC;

/* Fighter roster (gBtlChars). */
typedef struct BtlActERoster {
    /* 0x00 */ s32 count;
    /* 0x04 */ BtlActEChr *chars;
    /* 0x08 */ u8 unk8[0x7C - 0x8];
    /* 0x7C */ BtlActEClashC clashC;
} BtlActERoster;

s32 BtlAct_DragonDashHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_StepHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_StepInHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_VanishStepHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_VanishBehindHandler(BtlActEChr *chr, s32 phase);
void BtlAct_RecoverHandler(BtlActEChr *chr, s32 phase);
f32 BtlAct_PlayAttackPart(BtlActEChr *chr, BtlActEAttack *atk, s32 frame, f32 time);
void BtlAct_VanishAttackHandler(BtlActEChr *chr, s32 phase);
void BtlAct_VanishAttackQuickHandler(BtlActEChr *chr, s32 phase);
void BtlAct_WarpBehindAttackHandler(BtlActEChr *chr, s32 phase);
void BtlAct_WarpAheadAttackHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_SlideToAttackHandler(BtlActEChr *chr, s32 phase);
void BtlAct_RushToAttackHandler(BtlActEChr *chr, s32 phase);
void BtlAct_HopBackAttackHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_CircleDashHandler(BtlActEChr *chr, s32 phase);
void BtlAct_RushDashHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_VanishDashHandler(BtlActEChr *chr, s32 phase);
void BtlAct_ClashBlowsHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_ClashVanishHandler(BtlActEChr *chr, s32 phase);
s32 BtlAct_ClashExchangeHandler(BtlActEChr *chr, s32 phase);

#endif
