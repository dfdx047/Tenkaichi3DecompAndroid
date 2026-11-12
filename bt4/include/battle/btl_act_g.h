#ifndef BATTLE_BTL_ACT_G_H
#define BATTLE_BTL_ACT_G_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Blast 2 / Ultimate Blast actions, second part (src/battle/btl_act_g.c, 0x1F8C00..0x1FC2B0): the rush techniques
 * (dash, strike, scripted sequence, both sides), the clash struggle, and two helpers of the throw handlers.
 * The structures are partial views local to this module: only the fields this file touches. Blocks documented
 * elsewhere keep their layout (BtlCollThrow, HitReact, BtlMemberQueue, BtlActForm, BtlMovePose).
 */

/* Pose block of a fighter (chr + 0x10, BtlChar_GetPos). Same layout as BtlMovePose; only what this file reads. */
typedef struct ActGPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 unk30;
    /* 0x40 */ Vec4 moved;      /* whole movement of the previous frame */
    /* 0x50 */ Vec4 unk50;
    /* 0x60 */ Vec4 unk60;
    /* 0x70 */ Vec4 impulse;
    /* 0x80 */ Vec4 dir;
    /* 0x90 */ f32 pitch;       /* heading pitch */
    /* 0x94 */ f32 yaw;         /* heading yaw */
    /* 0x98 */ f32 speed;
    /* 0x9C */ f32 fallSpeed;
} ActGPose;

/* Rush / throw description shared by the two fighters (chr + 0xE90). Same layout as BtlCollThrow. */
typedef struct ActGThrow {
    /* 0x00 */ s32 tech;
    /* 0x04 */ s32 atkSide;
    /* 0x08 */ s32 defSide;
    /* 0x0C */ s32 slot;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 unk44;
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ s32 unk58;
    /* 0x5C */ s32 unk5C;
    /* 0x60 */ f32 unk60;
    /* 0x64 */ f32 unk64;
} ActGThrow; /* size 0x68 */

/* Hit reaction block (chr + 0xFB0). Same layout as HitReact in btl_char_hit.h. */
typedef struct ActGReact {
    /* 0x00 */ s32 reaction;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 back;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 silent;
    /* 0x18 */ f32 scale;
    /* 0x1C */ f32 yaw;
    /* 0x20 */ f32 turnYaw;     /* yaw the fighter is turned to under flag 0x94 */
    /* 0x24 */ f32 turnPitch;
    /* 0x28 */ f32 launchA;
    /* 0x2C */ f32 launchB;
    /* 0x30 */ s32 stun;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 damage;      /* deferred damage */
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 slot;
    /* 0x48 */ s32 unk48;
} ActGReact; /* size 0x4C */

/* Form-change request (chr + 0x12CC). Same layout as BtlActForm. */
typedef struct ActGForm {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 chara;
    /* 0x08 */ s32 costume;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;       /* model variant requested (1 from BtlAct_SetFormCurrent) */
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} ActGForm;

/* Gauge block of the active member (BtlMember_GetActiveGauge). */
typedef struct ActGGauge {
    /* 0x00 */ s32 health;
    /* 0x04 */ s32 healthMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ s32 blast;
    /* 0x18 */ s32 blastMax;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 variant;     /* model variant in use; 0 = undamaged */
    /* 0x24 */ u8 unk24[0x4C - 0x24];
    /* 0x4C */ s32 unk4C[5];    /* by technique slot: counts self-destruct techniques (flag B 0x2000) used */
} ActGGauge;

/* Damage queue (chr + 0xD94). Same layout as BtlMemberQueue in btl_char_member.h, plus the word after it. */
typedef struct ActGQueue {
    /* 0x00 */ s32 unk0;            /* the total again */
    /* 0x04 */ s32 total;           /* health damage still to deal */
    /* 0x08 */ s32 perHit;          /* amount per trigger while more than one hit is left */
    /* 0x0C */ s32 hits;            /* triggers left */
    /* 0x10 */ s32 flags;           /* BTL_DMG_* */
    /* 0x14 */ s32 unk14;           /* the health drain again */
    /* 0x18 */ s32 drainHealth;     /* health still to drain */
    /* 0x1C */ s32 drainHealthStep; /* per frame */
    /* 0x20 */ s32 unk20;           /* the ki drain again */
    /* 0x24 */ s32 drainKi;         /* ki still to drain */
    /* 0x28 */ s32 drainKiStep;     /* per frame */
    /* 0x2C */ s32 drainFrom;       /* animation frame of the 0x20000 event, -1 = no drain */
    /* 0x30 */ s32 drainTo;         /* animation frame of the 0x40000 event */
} ActGQueue; /* size 0x34 */

/* Root rotation track of an animation. */
typedef struct ActGTrack {
    /* 0x00 */ u16 flags;      /* bit 0: the first key is stored at +4, else at +0x14 */
    /* 0x02 */ u16 count;
    /* 0x04 */ u32 key[6];     /* packed quaternions, 64 bits each */
} ActGTrack;

typedef struct ActGAnim {
    /* 0x00 */ u8 unk0[6];
    /* 0x06 */ u16 rootTrack;  /* offset of the root rotation track in words, 0 = none */
} ActGAnim;

/* Battle object (BtlChar_GetObj). */
typedef struct ActGObj {
    /* 0x000 */ u8 unk0[0xB40];
    /* 0xB40 */ ActGAnim *anim;
} ActGObj;

/* Fighter (0x1600 bytes). */
typedef struct ActGChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[0x14];    /* per-action scratch, zeroed when an action starts */
    /* 0x0420 */ u8 unk420[0x964 - 0x420];
    /* 0x0964 */ s32 actionFrame;   /* frames spent in the current action */
    /* 0x0968 */ u8 unk968[0x970 - 0x968];
    /* 0x0970 */ s32 holdAction;    /* set in a run phase: skip the next forced-action check */
    /* 0x0974 */ u8 unk974[0xD94 - 0x974];
    /* 0x0D94 */ ActGQueue queue;   /* damage queue (BtlMemberQueue) */
    /* 0x0DC8 */ u8 unkDC8[0xE4C - 0xDC8];
    /* 0x0E4C */ s32 clashPower;    /* clash struggle score: technique value / 20, ability bonus, + 1 per frame of input 51 */
    /* 0x0E50 */ u8 unkE50[0xE90 - 0xE50];
    /* 0x0E90 */ ActGThrow thr;     /* rush / throw description (BtlCollThrow) */
    /* 0x0EF8 */ s32 rushHits;      /* written by BtlSuper_MeasureRushStep */
    /* 0x0EFC */ f32 stepFrame[5];  /* animation frame reached in each step of the rush sequence */
    /* 0x0F10 */ u8 unkF10[0xFB0 - 0xF10];
    /* 0x0FB0 */ ActGReact react;   /* hit reaction block (HitReact) */
    /* 0x0FFC */ u8 unkFFC[0x12CC - 0xFFC];
    /* 0x12CC */ ActGForm form;     /* form-change request (BtlActForm) */
    /* 0x12FC */ u8 unk12FC[0x1600 - 0x12FC];
} ActGChr;

void BtlAct_SuperRushDashHandler(ActGChr *chr, s32 phase);
void BtlAct_SuperRushStrikeHandler(ActGChr *chr, s32 phase);
void BtlAct_SuperRushSequenceHandler(ActGChr *chr, s32 phase);
void BtlAct_SuperRushFlyHandler(ActGChr *chr, s32 phase);
void BtlAct_SuperRushFollowHandler(ActGChr *chr, s32 phase);
void BtlAct_SuperRushFinishHandler(ActGChr *chr, s32 phase);
void BtlAct_SuperRushCaughtHandler(ActGChr *chr, s32 phase);
s32 BtlAct_UltimateStartHandler(ActGChr *chr, s32 phase);
void BtlAct_ClashStruggleHandler(ActGChr *chr, s32 phase);
void BtlAct_ClashLostHandler(ActGChr *chr, s32 phase);
s32 BtlAct_SuperLaunchedUpHandler(ActGChr *chr, s32 phase);
s32 BtlAct_ChangeRandomCharaHandler(ActGChr *chr, s32 phase);
void BtlActThrow_SetupDamage(ActGChr *chr, s32 extraHit);
void BtlActThrow_TurnByRootYaw(ActGChr *chr);

#endif
