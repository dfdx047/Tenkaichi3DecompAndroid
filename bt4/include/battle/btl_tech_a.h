#ifndef BATTLE_BTL_TECH_A_H
#define BATTLE_BTL_TECH_A_H

#include "types.h"

/*
 * Fighter queries by side, the attack records and the character parameter block
 * (src/battle/btl_tech_a.c, 0x20BA80..0x20F0E8, 124 functions).
 *
 * Three groups, in address order:
 *   0x20BA80..0x20C9F0  BtlCtrl_*   queries keyed by side / player; all but four are only called from 0x218D88
 *   0x20C9F0..0x20E090  BtlAtk_*    the current attack id and the 0x30-byte attack record it selects
 *   0x20E090..0x20F0E8  BtlParam_*  the character parameter block (battle object + 0x91C); continued in btl_tech_b.c
 *
 * All structures are partial views local to this module.
 */

/* ---- attack record --------------------------------------------------------------------------------------------- */

/* Number of attack records of a character; an id outside 0..0xA2 (including -1, "no attack") reads record 0. */
#define BTL_ATK_COUNT 0xA3

/* Charge thresholds (fighter + 0xD78, 0..1) that select the weak / medium / full record of a chargeable attack. */
#define BTL_ATK_CHARGE_MID  0.3f
#define BTL_ATK_CHARGE_FULL 0.99f

/*
 * Attack ids (BtlAtk_GetId; the mapping is verified, the game terms come from the handler names of the actions):
 *   0, 1        animation 0x37: 0 = its first hit (or a one-hit animation), 1 = a later hit
 *   2..10       animations 0x38..0x40 (the steps of the rush combo)
 *   0xB..0xD    action 0x60 by charge (weak / medium / full: charge < 0.3 / < 0.99 / else)
 *   0x11, 0x12  animation 0x93: before / from the frame of its last hit event
 *   0x13..0x21  charged smash: animations 0x58, 0x5B, 0x52, 0x55, 0x4C | 0x4F give 0x13..0x17 weak, 0x18..0x1C medium,
 *               0x1D..0x21 full
 *   0x22..0x27  animation 0x6D: charge tier * 2 + (0 before the last hit event, 1 from it)
 *   0x28..0x2D  animation 0x70: the same
 *   0x2E..0x30  action 0x61 by charge;  0x31..0x33  action 0x62 by charge
 *   0x34..0x38  actions 0x6B..0x6F, or animations 0x67, 0x6A, 0x61, 0x64, 0x5E below full charge
 *   0x39..0x3D  the same animations at full charge
 *   0x3E..0x42  the same actions / animations while fighter + 0xD68 is set
 *   0x43..0x46  actions 0x5A..0x5D
 *   0x48..0x4A  animation 0x43 by charge;  0x4B..0x4D  animation 0x46 by charge
 *   0x4E..0x52  action 0x46 (rapid rush), animations 0x3C..0x40
 *   0x55        actions 0xB4, 0xB5, 0xB7, 0xBB (grab dash, throw, lift throw);  0x56  actions 0xB6, 0xB9
 *   0x57        animation 0xF8
 *   0x59..0x5E  actions 0x64 (0x59, 0x5B, 0x5D by charge) and 0x65 (0x5A, 0x5C, 0x5E)
 *   0x66        actions 0x37 (ki charge) and 0x42
 *   0x67..0x6B  actions 0x4F, 0x50, 0x4D, 0x4E, 0x51 | 0x52 (fully charged smash), and the charged-smash
 *               animations above while the fighter has flag 0x84
 *   0x7E..0x86  animation 0x19B: (first hit, middle hits, last hit) + 3 * charge tier
 *   any         actions 0x70..0xAD: byte 3 of the action's row in the roster table + 0x2C
 * Ids never produced here: 0xE..0x10, 0x47, 0x53, 0x54, 0x58, 0x5F..0x65, 0x6C..0x7D, 0x87.. (table-driven only).
 */

/*
 * One attack record: (battle object + 0x920)[id], 0x30 bytes. For every byte: the accessor and what it does to the
 * raw value. "hits" is the number of hit events (attribute bit 0) of the playing animation, BtlObjAnim_QueryEvent(obj, 1, 0, 3).
 */
typedef struct BtlAtkRecord {
    /* 0x00 */ u32 flags;       /* BtlAtk_GetFlags: raw. The bits are listed in btl_char_hit.c */
    /* 0x04 */ u16 damage;      /* BtlAtk_GetDamage: * curve row 4, ability changes, / hits when hits >= 2 */
    /* 0x06 */ u16 guardDamage; /* BtlAtk_GetGuardDamage: the same chain, -30% for ids 0..10 under powered skill 0x10 */
    /* 0x08 */ u16 guardKi;     /* BtlAtk_GetGuardKiCost: * curve row 5; x2 (ability 0x44) or x1.5 (0x6D) */
    /* 0x0A */ u16 kiGain;      /* BtlAtk_GetKiGain: +50% (ability 0x6E) or +25% (0x20), / hits when hits >= 1 */
    /* 0x0C */ u16 throwC;      /* BtlAtk_GetThrowParamC: raw; read by the throw setup at 0x1FC008 */
    /* 0x0E */ u16 throwE;      /* BtlAtk_GetThrowParamE: raw; the same */
    /* 0x10 */ u16 unk10;       /* BtlAtk_GetUnk10: / hits when hits >= 1; added to fighter + 0xD80 per hit */
    /* 0x12 */ u8 pushHit;      /* BtlAtk_GetPushOnHit: units of 10 km/h, returned per frame */
    /* 0x13 */ u8 pushGuard;    /* BtlAtk_GetPushOnGuard: the same */
    /* 0x14 */ u8 launchSpeed;  /* BtlAtk_GetLaunchSpeed: the same, * curve row 8; the attacker's own speed */
    /* 0x15 */ s8 launchA;      /* BtlAtk_GetLaunchAngleA: degrees, returned in radians (a yaw offset) */
    /* 0x16 */ s8 launchB;      /* BtlAtk_GetLaunchAngleB: degrees, returned in radians (a pitch) */
    /* 0x17 */ u8 shakePower;   /* BtlAtk_GetShakePower: raw as float (camera shake on hit) */
    /* 0x18 */ u8 shakeTime;    /* BtlAtk_GetShakeTime: tenths of a second, returned in seconds */
    /* 0x19 */ u8 shakePowerB;  /* BtlAtk_GetShakePowerB: raw as float (camera shake on guard) */
    /* 0x1A */ u8 shakeTimeB;   /* BtlAtk_GetShakeTimeB: tenths of a second, returned in seconds */
    /* 0x1B */ s8 react;        /* BtlAtk_GetReaction: reaction id (1..49); the fallback of the six below */
    /* 0x1C */ s8 reactB;       /* BtlAtk_GetReactionB: defender animation flag 0x800; 0 = use react */
    /* 0x1D */ s8 reactD;       /* BtlAtk_GetReactionD: defender animation flag 0x1000; 0 = use react */
    /* 0x1E */ s8 reactE;       /* BtlAtk_GetReactionE: defender animation flag 0x2000; 0 = use react */
    /* 0x1F */ s8 reactC;       /* BtlAtk_GetReactionC: defender animation flag 0x4000 or action 0xDF; 0 = use react */
    /* 0x20 */ s8 reactSub;     /* BtlAtk_GetReactionSub: raw, no fallback (HitReact.unk4) */
    /* 0x21 */ s8 reactF;       /* BtlAtk_GetReactionF: defender animation flag 0x100 from the front; 0 = use react */
    /* 0x22 */ s8 reactG;       /* BtlAtk_GetReactionG: defender animation 0xAE / 0xAF or flag 0x200; 0 = use react */
    /* 0x23 */ s8 hitFx;        /* BtlAtk_GetHitFxKind: raw; 1, 2 | 3, 4 | 10 pick the hit effect (btl_char_member.c) */
    /* 0x24 */ s8 hitSound;     /* BtlAtk_GetHitSoundLevel: raw; picks the hit sound line */
    /* 0x25 */ s8 voice;        /* BtlAtk_GetVoiceKind: 1, 2, 3 -> voice kinds 5, 6, 7; anything else -1 */
    /* 0x26 */ s8 priority;     /* BtlAtk_GetPriority: raw (the higher one wins when two attacks trade) */
    /* 0x27 */ s8 guardB;       /* BtlAtk_GetGuardKindB: guard result 0..4 against guard kind 2 */
    /* 0x28 */ s8 guardC;       /* BtlAtk_GetGuardKindC: against guard kind 3 */
    /* 0x29 */ s8 guardA;       /* BtlAtk_GetGuardKindA: against guard kind 1 */
    /* 0x2A */ s8 guardE;       /* BtlAtk_GetGuardKindE: raw; against guard kind 5 */
    /* 0x2B */ s8 guardFlagSel; /* BtlAtk_GetGuardFlagSel: raw; 0 / 1 -> defender flag 0x66 / 0x67 on a guard */
    /* 0x2C */ s8 armorIgnore;  /* BtlAtk_GetUnk2C / BtlAtk_GetUnk2COf: raw; armour levels the attack ignores */
    /* 0x2D */ u8 unk2D[3];     /* no reader in this module */
} BtlAtkRecord; /* size 0x30 */

/* ---- character parameter block --------------------------------------------------------------------------------- */

/* Number of search types (BtlParam.searchType[], after BtlParam_GetType). */
#define BTL_TYPE_COUNT 10

/*
 * The block at (battle object + 0x91C): one per character, read-only. Offsets verified by the accessors; the
 * meanings come from the callers (see the report). +0x5C..+0x7F and +0x8F.. are documented in btl_tech_b.h.
 */
typedef struct BtlParam {
    /* 0x00 */ u16 unk0;          /* BtlParam_GetUnk0: bit 0x80 is tested by seven callers */
    /* 0x02 */ s8 unk2;           /* BtlParam_GetUnk2: 4 exempts the character from throws, rushes and some dodges */
    /* 0x03 */ s8 auraKind;       /* BtlParam_GetAuraKind: replaced by 0..10 by abilities 0x4D..0x56, 0x74 */
    /* 0x04 */ u8 unk4[0x10 - 0x4];
    /* 0x10 */ u32 flags;         /* BtlParam_GetFlags */
    /* 0x14 */ u32 flags2;        /* BtlParam_GetFlags2: skill bits (BtlAct_TestAttackSkill) */
    /* 0x18 */ u32 flags3;        /* BtlParam_GetFlags3: skill bits that need the powered-up mode (BtlAct_TestPoweredSkill) */
    /* 0x1C */ s32 unk1C;         /* BtlParam_GetUnk1C (no caller found) */
    /* 0x20 */ s32 unk20;         /* BtlParam_GetUnk20 (no caller found) */
    /* 0x24 */ s32 unk24;         /* BtlParam_GetUnk24 (no caller found) */
    /* 0x28 */ s32 kiRegenLimit;  /* BtlParam_GetKiRegenLimit: ki level the passive regeneration stops at */
    /* 0x2C */ s32 kiRecoverGoal; /* BtlParam_GetGaugeB: ki level that ends the ki-exhausted state (flag 0xBE) */
    /* 0x30 */ s32 kiCharge;      /* per second; BtlParam_GetRateA */
    /* 0x34 */ s32 kiChargeWater; /* per second, with fighter flag 0x11; BtlParam_GetRateB */
    /* 0x38 */ s32 kiRegen;       /* per second; BtlParam_GetKiRegenRate */
    /* 0x3C */ s32 kiRecover;     /* per second; BtlParam_GetKiRecoverRate */
    /* 0x40 */ s32 unk40;         /* BtlParam_GetUnk40 (no caller found) */
    /* 0x44 */ s32 blastGain;     /* per second; BtlParam_GetBlastGainRate */
    /* 0x48 */ s32 recoverKiCost; /* BtlParam_GetRecoverKiCost: ki an air recovery costs */
    /* 0x4C */ s32 kiDrain67;     /* per second; BtlParam_GetKiDrain67: ki actions 0x67..0x69 spend */
    /* 0x50 */ f32 maxPowerChargeTime; /* seconds to fill the max power gauge; BtlParam_GetStepA */
    /* 0x54 */ f32 maxPowerTime;  /* seconds the max power gauge lasts; BtlParam_GetMaxPowerDrain */
    /* 0x58 */ s8 searchType[4];  /* per costume; BtlParam_GetType */
    /* 0x5C */ u8 unk5C[0x80 - 0x5C]; /* btl_tech_b.h */
    /* 0x80 */ s16 blastLimitA;   /* BtlParam_GetCount80: live ki blasts of class 0 allowed at once */
    /* 0x82 */ s16 blastLimitB;   /* BtlParam_GetUnk82: the same for class 1 */
    /* 0x84 */ u8 unk84[4];       /* BtlParam_GetUnk84 */
    /* 0x88 */ u8 unk88[2];       /* BtlParam_GetUnk88: argument of BtlAct_GetChainAction when actions 0x67..0x69 end by flag 0x70 */
    /* 0x8A */ u8 unk8A[5];       /* BtlParam_GetUnk8A */
    /* 0x8F */ u8 unk8F[0x98 - 0x8F];
    /* 0x98 */ u8 transTarget[4]; /* BtlParam_GetSlotId: transformation targets; 0xFF = none (BtlParam_CountSlots) */
    /* 0x9C */ u8 transCost[4];   /* BtlParam_GetSlotCost: blast stocks, returned * 100000 */
    /* 0xA0 */ u8 transSeq[4];    /* BtlParam_GetSlotA0 */
    /* 0xA4 */ u8 transKind[4];   /* BtlParam_GetSlotA4 */
    /* 0xA8 */ u8 transA8[4];     /* BtlParam_GetSlotA8 */
    /* 0xAC */ s8 transDefault;   /* BtlParam_GetUnkAC: the target a neutral input picks */
    /* 0xAD */ u8 unkAD;          /* BtlParam_GetUnkAD; bit 4 + n: BtlParam_TestUnkADBit(n) (caller: BtlAct_QueueTransform) */
    /* 0xAE */ u8 fusionCost[3];  /* BtlParam_GetCostAE: blast stocks, returned * 100000 */
    /* 0xB1 */ u8 fusionSeq[3];   /* BtlParam_GetUnkB1 */
    /* 0xB4 */ u8 fusionResult[3]; /* BtlParam_GetUnkB4 */
    /* 0xB7 */ u8 fusionB7[3];    /* BtlParam_GetUnkB7 */
    /* 0xBA */ u8 fusionPartner[3][4]; /* BtlParam_GetUnkBA */
    /* 0xC6 */ u8 unkC6;          /* BtlParam_GetUnkC6 (no caller found) */
} BtlParam;

/* ---- other views ----------------------------------------------------------------------------------------------- */

/* The fighter's battle object (BtlChar_GetObj). */
typedef struct BtlTechObj {
    /* 0x000 */ u8 unk0[0x91C];
    /* 0x91C */ BtlParam *param;
    /* 0x920 */ BtlAtkRecord *atk;  /* BTL_ATK_COUNT records (a pointer, not an inline array) */
    /* 0x924 */ u8 unk924[0xC90 - 0x924];
    /* 0xC90 */ u8 unkC90[0x1C];    /* BtlAtk_GetId reads the two bytes below through a pointer to here */
    /* 0xCAC */ s8 hitCount;        /* hits of the current animation (inferred from the first / middle / last test) */
    /* 0xCAD */ s8 hitNo;           /* number of the current hit: 0 = first, 0 or -1 = "first" for animation 0x37 */
} BtlTechObj;

/* Member entry (BtlMember_Get / BtlMember_GetActive). */
typedef struct BtlTechMember {
    /* 0x00 */ s32 chara;
    /* 0x04 */ u32 costume;
    /* 0x08 */ u8 unk8[0x38 - 0x8];
    /* 0x38 */ s32 cpuLevel;
} BtlTechMember;

/* Gauge block of the active member (BtlMember_GetActiveGauge). */
typedef struct BtlTechGauge {
    /* 0x00 */ u8 unk0[0x20];
    /* 0x20 */ s32 variant;         /* non-zero: BtlParam_GetType turns types 2..4 into 7 / 8 */
} BtlTechGauge;

/* The pad the fighter reads (BtlChar_GetPad). */
typedef struct BtlTechPad {
    /* 0x000 */ u8 unk0[0x184];
    /* 0x184 */ s32 lastStatus;     /* Pad.lastStatus (sys/pad.h) */
} BtlTechPad;

/* Partial view of a fighter (0x1600 bytes). */
typedef struct BtlTechChr {
    /* 0x0000 */ u8 unk0[0x998];
    /* 0x0998 */ s32 memberCount;
    /* 0x099C */ u8 unk99C[0xD50 - 0x99C];
    /* 0x0D50 */ s32 unkD50;        /* BtlCtrl_GetOppUnkD50 reads the opponent's */
    /* 0x0D54 */ u8 unkD54[0xD68 - 0xD54];
    /* 0x0D68 */ s32 unkD68;        /* non-zero selects attack ids 0x3E..0x42 instead of 0x34..0x3D */
    /* 0x0D6C */ u8 unkD6C[0xD78 - 0xD6C];
    /* 0x0D78 */ f32 charge;        /* 0..1: how far the current attack was charged */
    /* 0x0D7C */ u8 unkD7C[0xE28 - 0xD7C];
    /* 0x0E28 */ s32 rateBonus;     /* per second, added to the ki charge rate (BtlParam_GetRateA / B) */
    /* 0x0E2C */ u8 unkE2C[0xE40 - 0xE2C];
    /* 0x0E40 */ s32 techDelay;     /* > 0: no technique input (btl_act_j.h); hides the technique prompt */
    /* 0x0E44 */ u8 unkE44[0xE50 - 0xE44];
    /* 0x0E50 */ s32 clashCount;    /* the clash counter of clashes B and C (btl_char_flag.h) */
    /* 0x0E54 */ u8 unkE54[0x1278 - 0xE54];
    /* 0x1278 */ s32 injectOn;      /* input is injected (CPU) */
    /* 0x127C */ u8 unk127C[0x1538 - 0x127C];
    /* 0x1538 */ u64 frameBits;     /* per-frame bit set */
    /* 0x1540 */ u8 unk1540[0x1590 - 0x1540];
    /* 0x1590 */ s32 dirHeld;       /* -1, or technique class 2..4 by the direction held (btl_char_action.c) */
    /* 0x1594 */ s32 techClass;     /* -1, or the technique class 2..4 whose button is watched this frame */
    /* 0x1598 */ u8 unk1598[0x1600 - 0x1598];
} BtlTechChr; /* size 0x1600 */

/* roster + 0x2C: one entry per attack action 0x70..0xAD (BtlAct_IsAttackId). */
typedef struct BtlTechAtkAction {
    /* 0x00 */ u8 unk0[3];
    /* 0x03 */ u8 atkId;            /* attack record the action uses */
    /* 0x04 */ u8 unk4[0x14];
} BtlTechAtkAction; /* size 0x18 */

/* roster + 0x34: one entry per technique type (BtlSuper_GetPromptRowIndex). */
typedef struct BtlTechPromptRow {
    /* 0x00 */ s8 kind[2];          /* prompt kind 0..3: [0] by held direction, [1] by watched class */
    /* 0x02 */ s8 buttons;          /* 0..3: button icon 4..7 shown first by BtlCtrl_GetSwitchPrompt */
    /* 0x03 */ u8 unk3[0x11];
} BtlTechPromptRow; /* size 0x14 */

/* Fighter roster (gBtlChars). */
typedef struct BtlTechRoster {
    /* 0x00 */ u8 unk0[0x2C];
    /* 0x2C */ BtlTechAtkAction *atkActions;
    /* 0x30 */ void *unk30;
    /* 0x34 */ BtlTechPromptRow *promptRows;
} BtlTechRoster;

/* Battle work: only the flag word. */
typedef struct BtlTechWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags;
} BtlTechWork;

/* Battle work + 0x5A8 (Battle_GetWork5A8). */
typedef struct BtlTechWork5A8 {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
} BtlTechWork5A8;

/* gProgress: only the field read here. */
typedef struct BtlTechProgress {
    /* 0x000 */ u8 unk0[0x7F4];
    /* 0x7F4 */ s32 unk7F4;
} BtlTechProgress;

/* by side */
s32 BtlCtrl_GetOppUnkD50(s32 side);
BtlTechObj *BtlCtrl_GetObj(s32 side);
s32 BtlCtrl_GetFlagDEtoE2(s32 side);
s32 BtlCtrl_GetClashCount(s32 side);
s32 BtlCtrl_IsFlag6Raised(s32 side);
s32 BtlCtrl_TestFlag71(s32 side);
s32 BtlCtrl_IsFlag5RaisedInFight(s32 side);
s32 BtlCtrl_TestFlagE3(s32 side);
s32 BtlCtrl_IsFlag9CRaised(s32 side);
s32 BtlCtrl_IsFlag9ERaised(s32 side);
s32 BtlCtrl_IsDead(s32 side);
s32 BtlCtrl_TestFlag12B(s32 side);
s32 BtlCtrl_IsSwitching(s32 side);
s32 BtlCtrl_TestFlagE4(s32 side);
s32 BtlCtrl_TestFlagE5(s32 side);
s32 BtlCtrl_TestProgressFrameBit(void);
s32 BtlCtrl_TestFlagE6Pad(s32 side, s32 *padStatus);
s32 BtlCtrl_TestFlagE6Pad1(s32 side);
s32 BtlCtrl_GetTechPromptPad1(s32 side, s32 *row, s32 *kind, s32 *idx);
s32 BtlCtrl_GetSwitchPrompt(s32 side, s32 *buttons, s32 *count, s32 *kind, s32 *padStatus, s32 *idx);
s32 BtlCtrl_GetChangePrompt(s32 side, s32 *button, s32 *kind, s32 *padStatus);
s32 BtlCtrl_GetStatMod0(s32 side);
s32 BtlCtrl_GetStatMod2(s32 side);
s32 BtlCtrl_GetStatMod1(s32 side);
s32 BtlCtrl_GetStatMod3(s32 side);
s32 BtlCtrl_CanUseTechnique(s32 player, s32 slot);
s32 BtlCtrl_GetWork5A8Count(void);
void BtlCtrl_SetCpuLevel(s32 player, s32 level);
s32 BtlCtrl_TestMemberUnk70(s32 player);

/* attack */
s32 BtlAtk_GetId(BtlTechChr *chr);
BtlAtkRecord *BtlAtk_GetRecord(BtlTechChr *chr, s32 *id);
BtlAtkRecord *BtlAtk_GetRecordOf(BtlTechChr *chr, u32 id);
s32 BtlAtk_ApplyAbilities(BtlTechChr *chr, s32 val, u32 id);
s32 BtlAtk_GetPriority(BtlTechChr *chr);
s32 BtlAtk_GetDamage(BtlTechChr *chr);
s32 BtlAtk_GetGuardDamage(BtlTechChr *chr);
s32 BtlAtk_GetThrowParamC(BtlTechChr *chr);
s32 BtlAtk_GetThrowParamE(BtlTechChr *chr);
s32 BtlAtk_GetKiGain(BtlTechChr *chr);
s32 BtlAtk_GetGuardKiCost(BtlTechChr *chr);
s32 BtlAtk_GetUnk10(BtlTechChr *chr);
s32 BtlAtk_GetReaction(BtlTechChr *chr);
s32 BtlAtk_GetReactionB(BtlTechChr *chr);
s32 BtlAtk_GetReactionC(BtlTechChr *chr);
s32 BtlAtk_GetReactionD(BtlTechChr *chr);
s32 BtlAtk_GetReactionE(BtlTechChr *chr);
s32 BtlAtk_GetReactionSub(BtlTechChr *chr);
s32 BtlAtk_GetReactionF(BtlTechChr *chr);
s32 BtlAtk_GetReactionG(BtlTechChr *chr);
u32 BtlAtk_GetFlags(BtlTechChr *chr);
f32 BtlAtk_GetPushOnHit(BtlTechChr *chr);
f32 BtlAtk_GetPushOnGuard(BtlTechChr *chr);
f32 BtlAtk_GetLaunchSpeed(BtlTechChr *chr);
f32 BtlAtk_GetLaunchAngleA(BtlTechChr *chr);
f32 BtlAtk_GetLaunchAngleAOf(BtlTechChr *chr, u32 id);
f32 BtlAtk_GetLaunchAngleB(BtlTechChr *chr);
f32 BtlAtk_GetLaunchAngleBOf(BtlTechChr *chr, u32 id);
f32 BtlAtk_GetShakePower(BtlTechChr *chr);
f32 BtlAtk_GetShakeTime(BtlTechChr *chr);
f32 BtlAtk_GetShakePowerB(BtlTechChr *chr);
f32 BtlAtk_GetShakeTimeB(BtlTechChr *chr);
s32 BtlAtk_GetHitFxKind(BtlTechChr *chr);
s32 BtlAtk_GetHitSoundLevel(BtlTechChr *chr);
s32 BtlAtk_GetVoiceKind(BtlTechChr *chr);
s32 BtlAtk_GetGuardKindA(BtlTechChr *chr);
s32 BtlAtk_GetGuardKindB(BtlTechChr *chr);
s32 BtlAtk_GetGuardKindC(BtlTechChr *chr);
s32 BtlAtk_RankGuardKind(u32 kind);
s32 BtlAtk_GetBestGuardKind(BtlTechChr *chr);
s32 BtlAtk_GetGuardKindE(BtlTechChr *chr);
s32 BtlAtk_GetGuardFlagSel(BtlTechChr *chr);
s32 BtlAtk_GetUnk2C(BtlTechChr *chr);
s32 BtlAtk_GetUnk2COf(BtlTechChr *chr, u32 id);
s32 BtlParam_ScaleKiCharge(BtlTechChr *chr, s32 val);

/* parameter block */
u32 BtlParam_GetFlags(BtlTechChr *chr);
u32 BtlParam_GetFlags2(BtlTechChr *chr);
u32 BtlParam_GetFlags3(BtlTechChr *chr);
s32 BtlParam_GetUnk0(BtlTechChr *chr);
s32 BtlParam_GetUnk2(BtlTechChr *chr);
s32 BtlParam_GetUnkAC(BtlTechChr *chr);
s32 BtlParam_GetSlotId(BtlTechChr *chr, s32 slot);
s32 BtlParam_GetSlotCost(BtlTechChr *chr, s32 slot);
s32 BtlParam_GetSlotA0(BtlTechChr *chr, s32 slot);
s32 BtlParam_GetSlotA4(BtlTechChr *chr, s32 slot);
s32 BtlParam_GetSlotA8(BtlTechChr *chr, s32 slot);
s32 BtlParam_GetUnkAD(BtlTechChr *chr);
s32 BtlParam_CountSlots(BtlTechChr *chr);
s32 BtlParam_TestUnkADBit(BtlTechChr *chr, s32 n);
s32 BtlParam_GetUnkB4(BtlTechChr *chr, s32 n);
s32 BtlParam_GetUnkB7(BtlTechChr *chr, s32 n);
s32 BtlParam_GetUnkBA(BtlTechChr *chr, s32 n, s32 m);
s32 BtlParam_GetCostAE(BtlTechChr *chr, s32 n);
s32 BtlParam_GetUnkC6(BtlTechChr *chr);
s32 BtlParam_GetUnkB1(BtlTechChr *chr, s32 n);
s32 BtlParam_GetAuraKind(BtlTechChr *chr);
s32 BtlParam_GetType(BtlTechChr *chr);
f32 BtlParam_GetTypeValueA(BtlTechChr *chr);
f32 BtlParam_GetTypeValueB(BtlTechChr *chr);
f32 BtlParam_GetTypeValueC(BtlTechChr *chr);
s32 BtlParam_IsType2to4(BtlTechChr *chr);
s32 BtlParam_IsTypeSetA(BtlTechChr *chr);
s32 BtlParam_IsTypeSetB(BtlTechChr *chr);
s32 BtlParam_GetCount80(BtlTechChr *chr);
s32 BtlParam_GetUnk82(BtlTechChr *chr);
s32 BtlParam_GetUnk84(BtlTechChr *chr, u32 n);
s32 BtlParam_GetUnk88(BtlTechChr *chr, u32 n);
s32 BtlParam_GetUnk8A(BtlTechChr *chr, u32 n);
s32 BtlParam_GetUnk1C(BtlTechChr *chr);
s32 BtlParam_GetUnk20(BtlTechChr *chr);
s32 BtlParam_GetUnk24(BtlTechChr *chr);
s32 BtlParam_GetKiRegenLimit(BtlTechChr *chr);
s32 BtlParam_GetGaugeB(BtlTechChr *chr);
s32 BtlParam_GetUnk40(BtlTechChr *chr);
s32 BtlParam_GetRateA(BtlTechChr *chr);
s32 BtlParam_GetRateB(BtlTechChr *chr);
s32 BtlParam_GetKiRegenRate(BtlTechChr *chr);
s32 BtlParam_GetKiRecoverRate(BtlTechChr *chr);
s32 BtlParam_GetKiDrain67(BtlTechChr *chr);
s32 BtlParam_GetRecoverKiCost(BtlTechChr *chr);
s32 BtlParam_GetAmountA(BtlTechChr *chr);
s32 BtlParam_GetAmountB(BtlTechChr *chr);
s32 BtlParam_GetBlastGainRate(BtlTechChr *chr);
s32 BtlParam_GetStepA(BtlTechChr *chr);
s32 BtlParam_GetMaxPowerDrain(BtlTechChr *chr);

#endif
