#ifndef BATTLE_BTL_CHAR_STATUS_H
#define BATTLE_BTL_CHAR_STATUS_H

#include "types.h"

/*
 * Fighter stat modifiers and fighter animation control:
 *   src/battle/btl_char_status.c       0x1C2FF0..0x1C3CA8  stat modifiers and the stat curves
 *   src/battle/btl_char_status_anim.c  0x1C3CA8..0x1C4BF8  animation playback of the fighter's battle object
 * The structures are partial views local to this module: only the fields these files touch.
 */

/* ---- stat modifiers ------------------------------------------------------------------------------------------ */

/* The four modified stats: index of BtlStatChr.stat[]. Meanings inferred from the curve rows each one feeds. */
#define BTL_STAT_0     0 /* with bonus[1]: rows 4..6 (multipliers 0.5..1..3) */
#define BTL_STAT_1     1 /* with bonus[2]: row 7 (multiplier 1.1875..1..0.25: lower is better, damage taken) */
#define BTL_STAT_2     2 /* with bonus[0] and bonus[3]: rows 0..2, 8, 11, 12 */
#define BTL_STAT_3     3 /* with bonus[4..6]: rows 3, 9, 10 */
#define BTL_STAT_COUNT 4

/* A character has two skill slots; every modifier is kept per slot. */
#define BTL_STAT_SLOTS 2

/* How long a skill's modifier lasts: the byte at (object +0x930) + 0x66 + slot (BtlSkill_GetStatKind). */
#define BTL_STAT_KIND_KEEP   0 /* base[]: never removed */
#define BTL_STAT_KIND_TIMED  1 /* timed[] with timer[]: removed when the timer runs out */
#define BTL_STAT_KIND_ACTION 2 /* untilA[]: removed when the action leaves 0x105..0x132 (BtlStat_Update) */
#define BTL_STAT_KIND_3      3 /* untilB[]: removed by BtlStat_EndKind3 (caller 0x1E16C0) */
#define BTL_STAT_KIND_4      4 /* untilC[]: removed by BtlStat_EndKind4 (caller 0x1E1208) */

/* Modifiers of one stat: fighter + 0xF50 + stat * 0x18. All values are levels, added up by BtlStat_GetMod. */
typedef struct BtlStatMod {
    /* 0x00 */ s8 base[BTL_STAT_SLOTS];    /* BtlStat_SetBase */
    /* 0x02 */ s8 timed[BTL_STAT_SLOTS];   /* BtlStat_SetTimed */
    /* 0x04 */ s8 untilA[BTL_STAT_SLOTS];  /* BtlStat_SetKind2 */
    /* 0x06 */ s8 untilB[BTL_STAT_SLOTS];  /* BtlStat_SetKind3 */
    /* 0x08 */ s8 untilC[BTL_STAT_SLOTS];  /* BtlStat_SetKind4 */
    /* 0x0A */ s8 penalty[BTL_STAT_SLOTS]; /* BtlStat_SetPenalty; not counted while the fighter has flag 6 */
    /* 0x0C */ s8 frame;                   /* BtlStat_SetFrameMod; cleared every frame by BtlStat_ClearFrameMods */
    /* 0x0D */ u8 unkD[3];
    /* 0x10 */ s32 timer[BTL_STAT_SLOTS];  /* frames left for timed[] */
} BtlStatMod; /* size 0x18 */

/* gBtlStatCurve[row][...]: the value of a derived quantity at level 80, 0 and -20 (f32[13][3] at 0x2EE7C0). */
#define BTL_CURVE_MAX  0 /* at level 80 */
#define BTL_CURVE_BASE 1 /* at level 0 */
#define BTL_CURVE_MIN  2 /* at level -20 */

#define BTL_STAT_CURVE_ROWS 13

/* ---- animation ----------------------------------------------------------------------------------------------- */

/* Bits of an animation's word in the roster table (gBtlChars + 0x20, 0x19E entries). */
#define BTL_ANIM_F_2        0x00000002 /* with F_80000000: BtlObjAnim_ZeroRootAxes(handle, 1, 0, 0) after BtlObjAnim_RebaseRoot */
#define BTL_ANIM_F_10       0x00000010 /* tested by the manager (btl_char_mgr.c) */
#define BTL_ANIM_F_20000000 0x20000000 /* started with BtlObjAnim_PlayFrom (needs the opponent's object) */
#define BTL_ANIM_F_40000000 0x40000000 /* BtlObjAnim_ZeroRootAxes(handle, 1, 1, 1) */
#define BTL_ANIM_F_80000000 0x80000000 /* BtlObjAnim_RebaseRoot(handle) */
#define BTL_ANIM_COUNT      0x19E

/* Fighter flags owned by the animation code. */
#define BTL_ANIM_FLAG_NEW        0x2B /* one frame: an animation was started on the main layer (BtlAnim_Play) */
#define BTL_ANIM_FLAG_NEW_KEEP   0x2C /* one frame: started by BtlAnim_PlayKeep */
#define BTL_ANIM_FLAG_REQ        0x2D /* held: BtlAnim_Request is pending */
#define BTL_ANIM_FLAG_REQ_KEEP   0x2E /* held: BtlAnim_RequestKeep is pending */
#define BTL_ANIM_FLAG_REQ_SUB    0x2F /* held: BtlAnim_RequestSubToMain is pending */
#define BTL_ANIM_FLAG_30         0x30 /* cleared by BtlAnim_Play */
#define BTL_ANIM_FLAG_END        0x31 /* held: the animation reached its last frame */
#define BTL_ANIM_FLAG_BLEND_REQ  0x32 /* held: fighter +0x984 holds a blend time to apply */
#define BTL_ANIM_FLAG_BLENDING   0x34 /* held: the blend counter of the object is still running */
#define BTL_ANIM_FLAG_NO_BLEND   0x37 /* tested: forces the blend time to 0 */

/* The animation ids of a fighter: fighter + 0x974. */
typedef struct BtlAnimIds {
    /* 0x0 */ s32 cur;  /* animation playing on the main layer: index into the roster's flag table */
    /* 0x4 */ s32 next; /* requested animation, started by BtlAnim_FlushRequest; -1 once started */
    /* 0x8 */ s32 prev; /* last frame's cur (written by the manager) */
    /* 0xC */ s32 sub;  /* animation on the second layer; -1 when the object is bound */
} BtlAnimIds; /* size 0x10 */

/* Animation player inside the battle object: object + 0xB40. Layer handles are 0x98 bytes apart. */
typedef struct BtlAnimObj {
    /* 0x000 */ void *handle;    /* main layer (what BtlObjAnim_ZeroRootAxes / BtlObjAnim_RebaseRoot / BtlObjAnim_ClearNodeRot take) */
    /* 0x004 */ f32 length;      /* last frame of the current animation */
    /* 0x008 */ u8 unk8[0x98 - 0x8];
    /* 0x098 */ void *subHandle; /* second layer (object + 0xBD8) */
    /* 0x09C */ u8 unk9C[0x138 - 0x9C];
    /* 0x138 */ f32 frame;       /* object + 0xC78: current frame */
    /* 0x13C */ f32 prevFrame;   /* object + 0xC7C: frame before the last advance */
    /* 0x140 */ f32 step;        /* object + 0xC80: frames added per advance */
    /* 0x144 */ f32 blend;       /* object + 0xC84: blend counter, runs down to 0 */
    /* 0x148 */ f32 blendStep;   /* object + 0xC88 */
    /* 0x14C */ f32 unk14C;      /* object + 0xC8C: BtlAnim_SetUnkC8C */
    /* 0x150 */ u8 unk150[0x178 - 0x150];
    /* 0x178 */ f32 rate;        /* object + 0xCB8: BtlAnim_SetObjRate, 1.0 every frame */
} BtlAnimObj;

/* The fighter's battle object (BtlChar_GetObj): only the fields used here. */
typedef struct BtlStatObj {
    /* 0x000 */ u8 unk0[0xC];
    /* 0x00C */ s32 model;       /* 0x78, 0x98, 0x99 select nodes to switch off (BtlAnim_HideModelNodes) */
    /* 0x010 */ u8 unk10[0xA40 - 0x10];
    /* 0xA40 */ u32 flags;       /* bits 0x20 and 0x40 are cleared when an animation starts */
    /* 0xA44 */ u8 unkA44[0xB40 - 0xA44];
    /* 0xB40 */ BtlAnimObj anim;
} BtlStatObj;

/* ---- fighter ------------------------------------------------------------------------------------------------- */

/* What BtlMember_GetActiveGauge returns: the active member's gauge block. */
typedef struct BtlStatGauge {
    /* 0x0 */ s32 health; /* 10000 per bar */
    /* 0x4 */ s32 healthMax;
} BtlStatGauge;

/* What BtlMember_GetActive returns: the active member entry. */
typedef struct BtlStatMember {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ s32 bonus[7]; /* BattleMember.bonus[1..7] */
} BtlStatMember;

/* Partial view of a fighter (0x1600 bytes). */
typedef struct BtlStatChr {
    /* 0x0000 */ u8 unk0[0xC];
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x974 - 0x10];
    /* 0x0974 */ BtlAnimIds anim;
    /* 0x0984 */ f32 blendTime;   /* seconds; valid with flag 0x32 */
    /* 0x0988 */ f32 animRate;    /* BtlAnim_SetRate, 1.0 every frame */
    /* 0x098C */ s32 stall;       /* advances in a row that left the frame unchanged */
    /* 0x0990 */ s32 prevStall;   /* stall before the last advance; > 0 turns the attribute test off */
    /* 0x0994 */ u8 unk994[0xE30 - 0x994];
    /* 0x0E30 */ s32 kind2On;     /* 1 when a kind-2 skill was used; 0 when its modifiers end */
    /* 0x0E34 */ s32 kind3On;     /* the same for kind 3 */
    /* 0x0E38 */ s32 slotOn[BTL_STAT_SLOTS]; /* 1 while the slot's (non-kept) modifier is active */
    /* 0x0E40 */ u8 unkE40[0xEE4 - 0xE40];
    /* 0x0EE4 */ s32 unkEE4;      /* passed to BtlObjAnim_PlayFrom when BtlCharApi_IsInRushSequence(objId) is set */
    /* 0x0EE8 */ u8 unkEE8[0xF50 - 0xEE8];
    /* 0x0F50 */ BtlStatMod stat[BTL_STAT_COUNT];
    /* 0x0FB0 */ u8 unkFB0[0x1600 - 0xFB0];
} BtlStatChr; /* size 0x1600 */

/* Fighter roster (gBtlChars): only the animation flag table. */
typedef struct BtlStatRoster {
    /* 0x00 */ u8 unk0[0x20];
    /* 0x20 */ u32 *animFlags; /* tbl[0]: one word per animation id, BTL_ANIM_COUNT entries */
} BtlStatRoster;

/* btl_char_status.c */
void BtlStat_ClearSlotOn(BtlStatChr *chr, s32 slot, s32 kind);
void BtlStat_EndKind2(BtlStatChr *chr);
void BtlStat_EndKind3(BtlStatChr *chr);
void BtlStat_EndKind4(BtlStatChr *chr);
void BtlStat_ClearPenalty(BtlStatChr *chr);
void BtlStat_ClearFrameMods(BtlStatChr *chr);
void BtlStat_Reset(BtlStatChr *chr);
void BtlStat_Update(BtlStatChr *chr);
void BtlStat_SetBase(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add);
void BtlStat_SetFrameMod(BtlStatChr *chr, s32 stat, s32 val, s32 add);
void BtlStat_SetTimed(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add, s32 time);
void BtlStat_SetKind2(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add);
void BtlStat_SetKind3(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add);
void BtlStat_SetKind4(BtlStatChr *chr, s32 stat, s32 slot, s32 val, s32 add);
void BtlStat_SetPenalty(BtlStatChr *chr, s32 stat, s32 slot, s32 val);
s32 BtlStat_GetMod(BtlStatChr *chr, s32 stat);
s32 BtlStat_GetLevel0(BtlStatChr *chr);
s32 BtlStat_GetLevel1(BtlStatChr *chr);
s32 BtlStat_GetLevel2(BtlStatChr *chr);
s32 BtlStat_GetLevel3(BtlStatChr *chr);
s32 BtlStat_GetLevel4(BtlStatChr *chr);
s32 BtlStat_GetLevel5(BtlStatChr *chr);
s32 BtlStat_GetLevel6(BtlStatChr *chr);
f32 BtlStat_EvalCurve(s32 level, s32 row);
s32 BtlStat_GetKiChargeBonus(BtlStatChr *chr);
s32 BtlStat_GetKiRegenBonus(BtlStatChr *chr);
s32 BtlStat_GetKiRecoverBonus(BtlStatChr *chr);
s32 BtlStat_GetBlastGainBonus(BtlStatChr *chr);
f32 BtlStat_GetMeleeDamageScale(BtlStatChr *chr);
f32 BtlStat_GetGuardKiCostScale(BtlStatChr *chr);
f32 BtlStat_GetKiBlastDamageScale(BtlStatChr *chr);
f32 BtlStat_GetScale7(BtlStatChr *chr);
f32 BtlStat_GetSpeedScale(BtlStatChr *chr);
f32 BtlStat_GetBlast2DamageScale(BtlStatChr *chr);
f32 BtlStat_GetUltimateDamageScale(BtlStatChr *chr);
f32 BtlStat_GetMaxPowerChargeScale(BtlStatChr *chr);
f32 BtlStat_GetMaxPowerExtraTime(BtlStatChr *chr);

/* btl_char_status_anim.c */
void BtlAnim_ApplyTableFlags(void *handle, s32 anim);
void BtlAnim_HideModelNodes(void *handle, s32 model);
void BtlAnim_HideModelNode10(void *handle, s32 model);
void BtlAnim_Play(BtlStatChr *chr, s32 anim, f32 blend);
void BtlAnim_PlayKeep(BtlStatChr *chr, s32 anim, f32 blend);
void BtlAnim_SubToMain(BtlStatChr *chr);
void BtlAnim_Request(BtlStatChr *chr, s32 anim, f32 blend);
void BtlAnim_RequestKeep(BtlStatChr *chr, s32 anim, f32 blend);
void BtlAnim_RequestSubToMain(BtlStatChr *chr);
void BtlAnim_SetBlend(BtlStatChr *chr, f32 blend);
void BtlAnim_SetNoBlend(BtlStatChr *chr);
void BtlAnim_FlushRequest(BtlStatChr *chr);
void BtlAnim_FlushRequestKeep(BtlStatChr *chr);
void BtlAnim_FlushSubToMain(BtlStatChr *chr);
void BtlAnim_ApplyBlend(BtlStatChr *chr);
void BtlAnim_PlaySub(BtlStatChr *chr, s32 anim);
void BtlAnim_SetUnkC8C(BtlStatChr *chr, f32 v);
void BtlAnim_SetStep(BtlStatChr *chr, f32 step);
void BtlAnim_SetDuration(BtlStatChr *chr, f32 seconds);
void BtlAnim_SetObjRate(BtlStatChr *chr, f32 rate);
void BtlAnim_SetRate(BtlStatChr *chr, f32 rate);
void BtlAnim_JumpToEnd(BtlStatChr *chr);
void BtlAnim_EnableHandle(BtlStatChr *chr);
s32 BtlAnim_GetId(BtlStatChr *chr);
s32 BtlAnim_GetNextId(BtlStatChr *chr);
s32 BtlAnim_GetPrevId(BtlStatChr *chr);
u32 BtlAnim_GetFlags(u32 anim);
f32 BtlAnim_GetFrame(BtlStatChr *chr);
f32 BtlAnim_GetProgress(BtlStatChr *chr);
f32 BtlAnim_GetLength(BtlStatChr *chr);
f32 BtlAnim_GetStep(BtlStatChr *chr);
f32 BtlAnim_GetObjRate(BtlStatChr *chr);
s32 BtlAnim_TestAttr(BtlStatChr *chr, u64 mask);
s32 BtlAnim_Advance(BtlStatChr *chr, s32 flags);
s32 BtlAnim_AdvanceThen(BtlStatChr *chr, s32 next, f32 blend, s32 flags);
void BtlAnim_AdvanceLoop(BtlStatChr *chr, s32 flags);
s32 BtlAnim_PassedRatio(BtlStatChr *chr, f32 ratio);
s32 BtlAnim_PassedFrame(BtlStatChr *chr, f32 frame);
s32 BtlAnim_InRatioRange(BtlStatChr *chr, f32 lo, f32 hi);
s32 BtlAnim_InFrameRange(BtlStatChr *chr, f32 lo, f32 hi);
s32 BtlAnim_IsNew(BtlStatChr *chr);
s32 BtlAnim_WasNew(BtlStatChr *chr);

#endif
