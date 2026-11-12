#ifndef BATTLE_BTL_ACT_B_H
#define BATTLE_BTL_ACT_B_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter action handlers, second slice (src/battle/btl_act_b.c, 0x1E6CC0..0x1EA5F8).
 * The handler protocol is described in include/battle/btl_char_action.h. The structures are
 * partial views local to this module: only the fields this file touches.
 */

/* Pose block (chr + 0x10, what BtlChar_GetPos returns); the same layout as BtlMovePose. */
typedef struct BtlActBPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;       /* y = model yaw */
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 unk30;     /* movement of the previous frame ("my velocity" in btl_char_move.h) */
    /* 0x40 */ Vec4 moved;     /* whole movement of the previous frame: its length is the real speed */
    /* 0x50 */ u8 unk50[0x70 - 0x50];
    /* 0x70 */ Vec4 impulse;   /* knock-back velocity, metres per frame */
    /* 0x80 */ Vec4 dir;       /* unit direction of travel */
    /* 0x90 */ f32 pitch;      /* heading pitch, positive = up */
    /* 0x94 */ f32 yaw;        /* heading yaw */
    /* 0x98 */ f32 speed;      /* metres per frame along dir */
    /* 0x9C */ f32 fallSpeed;  /* vertical speed, positive = down */
    /* 0xA0 */ f32 leanX;
    /* 0xA4 */ f32 leanZ;
    /* 0xA8 */ u8 unkA8[0xB0 - 0xA8];
    /* 0xB0 */ Vec4 ground;    /* ground point below the fighter */
    /* 0xC0 */ Vec4 groundNormal;
    /* 0xD0 */ s32 groundFlags; /* bit 0x40 with pos.y below BtlStage_GetBottom(): landed out of the arena (action 0xD9) */
} BtlActBPose;

/* Active member's gauge block (BtlMember_GetActiveGauge). */
typedef struct BtlActBGauge {
    /* 0x00 */ u8 unk0[0x28];
    /* 0x28 */ s32 unk28;      /* non-zero: a downed fighter has to mash before it may act (chr + 0x1000) */
} BtlActBGauge;

/* Latched attack parameters (chr + 0xCF8); BtlActAttack in btl_char_action.h. */
typedef struct BtlActBAttack {
    /* 0x00 */ s32 flags;      /* 0x40 end the camera cut on contact, 0x80 raise flag 0xCD, 0x400 end in action 0xC */
    /* 0x04 */ s16 motion[2];  /* one animation per part */
    /* 0x08 */ s8 parts;
    /* 0x09 */ s8 level;       /* 1..3: effect request 0xD at the start */
    /* 0x0A */ s8 cut;         /* camera cut, < 0 = none */
    /* 0x0B */ s8 follow[2];
    /* 0x0D */ u8 padD[3];
    /* 0x10 */ f32 rate;       /* animation step = rate * 2 while a further part follows */
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 speed;      /* advance speed, metres per frame (0 = stand still) */
    /* 0x20 */ s32 unk20;
} BtlActBAttack; /* size 0x24 */

/* The fighter's battle object (BtlChar_GetObj). */
typedef struct BtlActBObjSub {
    /* 0x00000 */ u8 unk0[0x18060];
    /* 0x18060 */ s32 unk18060;   /* bit 0x40 tested when a launched / blown away fighter meets a wall */
} BtlActBObjSub;

typedef struct BtlActBObj {
    /* 0x0000 */ u8 unk0[0x1660];
    /* 0x1660 */ BtlActBObjSub *unk1660;
} BtlActBObj;

/* Fighter (0x1600 bytes). */
typedef struct BtlActBChr {
    /* 0x0000 */ s32 player;        /* passed to BtlEvent_Raise by action 0xD8 */
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[0x14];    /* per-action scratch, zeroed when an action starts; uses are listed per handler
                                       in btl_act_b.c ([0] bits, [1] done, [2] counter, [3] / [4] counters, [6] float) */
    /* 0x0420 */ u8 unk420[0x964 - 0x420];
    /* 0x0964 */ s32 actionFrame;   /* frames spent in the current action */
    /* 0x0968 */ u8 unk968[0xCF8 - 0x968];
    /* 0x0CF8 */ BtlActBAttack attack;
    /* 0x0D1C */ u8 unkD1C[0xD68 - 0xD1C];
    /* 0x0D68 */ s32 unkD68;        /* > 0: actions 0x6B..0x6F request camera cut 0x20 */
    /* 0x0D6C */ u8 unkD6C[0xD78 - 0xD6C];
    /* 0x0D78 */ f32 unkD78;        /* set to 1.0 every frame by the attack handler and by action 0x6A */
    /* 0x0D7C */ u8 unkD7C[0xFB8 - 0xD7C];
    /* 0x0FB8 */ s32 animCycle;     /* stagger animation variant counter (BtlActB_NextVariant) */
    /* 0x0FBC */ u8 unkFBC[0xFC8 - 0xFBC];
    /* 0x0FC8 */ f32 unkFC8;        /* reaction strength 0..1 (inferred): scales launch speed, knock-back time and
                                       how much mashing helps */
    /* 0x0FCC */ u8 unkFCC[0xFE0 - 0xFCC];
    /* 0x0FE0 */ s32 unkFE0;        /* countdown; > 0 = action 0xE0 */
    /* 0x0FE4 */ u8 unkFE4[0xFF0 - 0xFE4];
    /* 0x0FF0 */ s32 unkFF0;        /* countdown, set to 15 when a stagger action is left */
    /* 0x0FF4 */ u8 unkFF4[0x1000 - 0xFF4];
    /* 0x1000 */ s32 unk1000;       /* presses still owed before a downed fighter may act, while gauge +0x28 is set */
    /* 0x1004 */ u8 unk1004[0x1600 - 0x1004];
} BtlActBChr; /* size 0x1600 */

/* Helpers other slices call. */
s32 BtlActB_NextVariant(BtlActBChr *chr, s32 n);
void BtlActB_SetReactionFlags(BtlActBChr *chr);
s32 BtlActB_PickDownAnim(BtlActBChr *chr, s32 cur);
void BtlActB_UpdateMashRate(BtlActBChr *chr, f32 *rate, s32 scaled);
s32 BtlActB_TickMemberChange(BtlActBChr *chr, s32 *frames);

#endif
