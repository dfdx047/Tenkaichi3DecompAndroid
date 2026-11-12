#ifndef BATTLE_BTL_ACT_F_H
#define BATTLE_BTL_ACT_F_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Blast 2 / Ultimate Blast actions, first part (src/battle/btl_act_f.c, 0x1F5460..0x1F8C00): the helpers shared
 * by all class 2..4 technique actions and the handlers of actions 0x106..0x11A. The structures are partial views
 * local to this module: only the fields this file touches. Names are guesses unless the comment gives the reason.
 */

/* Fighter flags this module sets or tests (numbers verified, meanings from how they are used here). */
#define BTL_SUPER_FLAG_FIRE     0xA7 /* held; raised by the effect when the start cinematic is over: fire */
#define BTL_SUPER_FLAG_END      0xA8 /* held; raised by the effect when the beam is over: leave the firing loop */
#define BTL_SUPER_FLAG_END2     0xA9 /* held; the same for the second firing stage */
#define BTL_SUPER_FLAG_QUICK    0xA4 /* quick form: the damage reader BtlSuper_GetDamage gives 30% */
#define BTL_SUPER_TIMEOUT       150  /* frames after which the handlers raise those three flags themselves */

/* Pose block (chr + 0x10, BtlChar_GetPos). */
typedef struct BtlSuperPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;       /* rot.y = model yaw */
    /* 0x20 */ u8 unk20[0x98 - 0x20];
    /* 0x98 */ f32 unk98;      /* zeroed with velY when a technique starts */
    /* 0x9C */ f32 velY;       /* vertical speed (btl_char_coll.h) */
} BtlSuperPose;

/* Animation data of one layer of the object. */
typedef struct BtlSuperAnimData {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 frames;      /* length in frames */
} BtlSuperAnimData;

/* Battle object of a fighter (BtlChar_GetObj). */
typedef struct BtlSuperObj {
    /* 0x000 */ u8 unk0[0xA40];
    /* 0xA40 */ u32 flags;     /* bit 0x40000: set while BtlObj_SetColorMode bit 2 is on (inferred) */
    /* 0xA44 */ u8 unkA44[0xBD8 - 0xA44];
    /* 0xBD8 */ BtlSuperAnimData *subAnim; /* animation of layer 1 (BtlAnim_PlaySub), NULL when not loaded */
} BtlSuperObj;

/* Active member's gauge block (BtlMember_GetActiveGauge). */
typedef struct BtlSuperGauge {
    /* 0x00 */ s32 health;
    /* 0x04 */ u8 unk4[0x20 - 0x4];
    /* 0x20 */ s32 variant;
    /* 0x24 */ u8 unk24[0x38 - 0x24];
    /* 0x38 */ s32 used[5];    /* [class]: times a technique of that class was started (only 2..4 written) */
    /* 0x4C */ s32 fired[5];   /* [class]: times a self-destruct technique (flag B 0x2000) was fired */
} BtlSuperGauge;

/* Fighter (0x1600 bytes): the fields this module touches. */
typedef struct BtlSuperChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ u8 unk4[0x3D0 - 0x4];
    /* 0x03D0 */ s32 work[0x14];   /* per-action scratch, zeroed on every action switch */
    /* 0x0420 */ u8 unk420[0x964 - 0x420];
    /* 0x0964 */ s32 actionFrame;  /* frames in the current action */
    /* 0x0968 */ u8 unk968[0xD94 - 0x968];
    /* The damage queue against the opponent (BtlMemberQueue in btl_char_member.h), filled by
       BtlSuper_SetupRushDamage. */
    /* 0x0D94 */ s32 dmgUnk0;      /* the total again (1 with technique flag B 0x400 and + 0xEC4 set) */
    /* 0x0D98 */ s32 dmgTotal;     /* total damage, rounded up to 10 */
    /* 0x0D9C */ s32 dmgPerHit;    /* total / (hits + 5), at most 15000, rounded up to 10 */
    /* 0x0DA0 */ s32 dmgHits;      /* number of 0x200000000 events in the animation(s) */
    /* 0x0DA4 */ s32 dmgFlags;     /* BTL_DMG_*: 0x180 plus 0x800000 / 0x1000000 / 0x2000000 by class */
    /* 0x0DA8 */ s32 dmgUnk14;     /* the health drain again */
    /* 0x0DAC */ s32 drainHealth;  /* health still to drain */
    /* 0x0DB0 */ s32 drainHealthStep; /* per frame: drain / max((to - from) / 2, 1) + 1 */
    /* 0x0DB4 */ s32 dmgUnk20;
    /* 0x0DB8 */ s32 drainKi;
    /* 0x0DBC */ s32 drainKiStep;
    /* 0x0DC0 */ s32 drainFrom;    /* first frame of the drain window, -1 = none */
    /* 0x0DC4 */ s32 drainTo;      /* last frame of the drain window */
    /* 0x0DC8 */ u8 unkDC8[0xE40 - 0xDC8];
    /* 0x0E40 */ s32 cooldown;     /* frames, from the technique (BtlSuper_GetCooldownFrames) when its action is left */
    /* 0x0E44 */ f32 charge;       /* charge level of a chargeable technique, 0..1 */
    /* 0x0E48 */ s32 chargeFull;   /* frames the charge has been full (the sounds play on the first) */
    /* 0x0E4C */ u8 unkE4C[0xE5C - 0xE4C];
    /* 0x0E5C */ s32 unkE5C;       /* counter (full at 3, fighter.md); zeroed when technique 0x268 ends */
    /* 0x0E60 */ s32 unkE60;       /* counter 0..10: + 5 per technique 0x2EF, zeroed when technique 0x2CD ends */
    /* 0x0E64 */ u8 unkE64[0xEC4 - 0xE64];
    /* 0x0EC4 */ s32 unkEC4;       /* non-zero: a flag B 0x400 technique deals 1 damage, undefended and exact */
    /* 0x0EC8 */ u8 unkEC8[0xEE8 - 0xEC8];
    /* 0x0EE8 */ s32 unkEE8;       /* non-zero: rush damage is halved */
    /* 0x0EEC */ u8 unkEEC[0xEF8 - 0xEEC];
    /* Measured from a rush technique's animation chain by BtlSuper_MeasureRushStep. */
    /* 0x0EF8 */ s32 rushHits;     /* 0x200000000 events in all steps */
    /* 0x0EFC */ u8 unkEFC[0xF10 - 0xEFC];
    /* 0x0F10 */ s32 rushFrames;   /* total length: frames of every step, + 1 between steps */
    /* 0x0F14 */ s32 rushFrameA;   /* frame, counted over the chain, of the first 0x20000 event */
    /* 0x0F18 */ s32 rushFrameB;   /* the same for the 0x40000 event (its last frame) */
    /* 0x0F1C */ s32 rushStepA;    /* step that holds the 0x20000 event, -1 = not found yet */
    /* 0x0F20 */ s32 rushStepB;    /* step that holds the 0x40000 event */
    /* 0x0F24 */ u8 unkF24[0x12D0 - 0xF24];
    /* The form-change request (BtlActForm at + 0x12CC), filled by BtlAct_SetFormCurrent. */
    /* 0x12D0 */ s32 formChara;
    /* 0x12D4 */ s32 formCostume;
    /* 0x12D8 */ u8 unk12D8[0x12E0 - 0x12D8];
    /* 0x12E0 */ s32 form14;       /* passed to BtlChange_RequestChara as variant, animChara, unk18, voiceChara */
    /* 0x12E4 */ s32 form18;
    /* 0x12E8 */ s32 form1C;
    /* 0x12EC */ s32 form20;
    /* 0x12F0 */ u8 unk12F0[0x1594 - 0x12F0];
    /* 0x1594 */ s32 inputClass;   /* class whose button command is watched; -1 again every frame */
    /* 0x1598 */ u8 unk1598[0x1600 - 0x1598];
} BtlSuperChr; /* size 0x1600 */

void BtlSuper_RequestCut(BtlSuperChr *chr, s32 cls);
void BtlSuper_SetStartFx(BtlSuperChr *chr, s32 cls);
void BtlSuper_SetClassFlag(BtlSuperChr *chr, s32 cls, s32 force);
void BtlSuper_RequestHitStop(BtlSuperChr *chr, s32 light);
void BtlSuper_SetFiringFlags(BtlSuperChr *chr);
void BtlSuper_SetClass(BtlSuperChr *chr, s32 cls);
void BtlSuper_SetRushFlags(BtlSuperChr *chr);
void BtlSuper_ShakeOnEvent(BtlSuperChr *chr, s32 cls, s32 kind);
void BtlSuper_FitAnimToEvent(BtlSuperChr *chr, f32 seconds);
void BtlSuper_SetObjUnk(BtlSuperChr *chr);
void BtlSuper_VibrateCharge(BtlSuperChr *chr);
void BtlSuper_Recoil(BtlSuperChr *chr, s32 cls, s32 loop);
void BtlSuper_MeasureRushStep(BtlSuperChr *chr, s32 cls, s32 step);
void BtlSuper_Begin(BtlSuperChr *chr, s32 cls, s32 force);
void BtlSuper_PlayStartSound(BtlSuperChr *chr, s32 cls, s32 force);
void BtlSuper_Leave(BtlSuperChr *chr, s32 cls);
void BtlSuper_SetupRushDamage(BtlSuperChr *chr, s32 cls, s32 fromAnim);
void BtlSuper_Finish(BtlSuperChr *chr, s32 cls);
void BtlAct_SuperBeamHandler(BtlSuperChr *chr, s32 phase);
void BtlAct_SuperWarpBeamHandler(BtlSuperChr *chr, s32 phase);
void BtlAct_SuperLongBeamHandler(BtlSuperChr *chr, s32 phase);
void BtlAct_SuperChargeHandler(BtlSuperChr *chr, s32 phase);
void BtlAct_SuperRepeatHandler(BtlSuperChr *chr, s32 phase);
void BtlAct_SuperQuickBeamHandler(BtlSuperChr *chr, s32 phase);
void BtlAct_SuperQuickLongBeamHandler(BtlSuperChr *chr, s32 phase);

#endif
