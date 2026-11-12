#ifndef BATTLE_BTL_ACT_I_H
#define BATTLE_BTL_ACT_I_H

#include "types.h"
#include "sys/math3d.h"

/*
 * src/battle/btl_act_i.c (0x1FFAC0..0x203168): the tail of the member switch chains, the skill actions, and the
 * input decision functions of the fighter. The structures are partial views local to this module.
 *
 * ---- Member switch chains (verified by the matching C; "why" is inferred) ------------------------------------
 *
 *   by input (107):  0xF3 leaving member (btl_act_h.c) -> 0xF4 -> 0xF5 -> 0xB
 *   after a KO:      0xD2 ... 0xEB (btl_act_c.c)       -> 0xF6 -> 0xF7 -> 0xF8 -> fighter flag 0xF9 (round reset)
 *
 *   0xF4  first action on the new model: one hidden frame (flags 0xB, 0xCC), BtlChange_SetDone.
 *   0xF5  the new member lands: animation 0x2D (held 8 frames, effect 0xD, sound 0x20, voice 0xF), then 0x26 / 0;
 *         at 20% of that animation it raises event 0x5B and may leave. Leaving: flags 0x37, 0xF5 + 0x55 (unless
 *         0xFC / 0xFD are pending), event 0x4E, +2 seconds on the stage timer.
 *   0xF6  the defeated member (animation 0xD8 in water, else 0xD6) waits: frame 1 BtlChange_RequestChara with
 *         the six values at +0xCDC, then, once BtlChange_IsLoadedFor, BtlChange_SetReady and action 0xF7.
 *   0xF7  first action on the new model: BtlChange_SetDone, entry point saved as the placement, flags 0xF5 + 0x55
 *         (placement requests), one hidden frame.
 *   0xF8  flies in (animation 0x19, 2500 km/h, facing the start placement), on touching the ground (flag 0xF, or
 *         0x1F without 0x20) lands (0x2D: effect 0x35, sound 0x25, voice 0xF, camera shake, vibration), stands
 *         (animation 0) 30 frames (event 0x5B on the 30th), then sets fighter flag 0xF9 and raises event 0x4E.
 *   0xF9  the action a fighter is in during the round reset: leaves through BtlActChange_Finish; on leaving sets
 *         effect bit 5 and flags 0xD5, 0xCD, 0x37 and adds 2 seconds to the stage timer.
 *
 * ---- Skills (actions 0xFD..0x102; class 0 / 1 = skill slot) ---------------------------------------------------
 *
 *   0xFD / 0xFE   BtlAct_SkillBasic: animation 0x103 / 0x104, effect bit 0x3C / 0x3D, flag 0xDE / 0xDF; the
 *                 skill is applied on entry, or when the animation ends if attribute 0x80 is set. Attribute
 *                 0x40000 makes it a cinematic: camera cut (1, 5) and every frame flags 0x125 (hit-stop level 2),
 *                 0x12A, 0x42..0x46, 0x128, 0xBB. Attributes 0x1000000 / 0x2000000 / 0x4000000 / 0x8000000 set
 *                 flags 0x42 + 0x43 / 0x44 / 0x46 / 0x45 each frame. Skill id 1 sets flag 0x137 each frame and on
 *                 leaving holds flag 0x4C + slot. Skill id 0x18 on leaving calls BtlObj_SetColorMode(obj, 2, 0) when
 *                 flag 0x98 is clear and object flag 0x40000 is set.
 *   0xFF / 0x100  BtlAct_SkillTeleport: applies at once; when the animation is over, nine frames of vanish; with
 *                 lock-on the fighter reappears behind the opponent on the 7th.
 *   0x101 / 0x102 BtlAct_SkillInstant: applies and leaves at once (guard input may follow).
 *
 *   BtlSkill_Apply(chr, slot) is the only writer of the per-slot stat modifiers:
 *     kind (BtlSkill_GetStatKind) 0..4 -> BtlStat_SetBase / SetTimed / SetKind2 / SetKind3 / SetKind4 for stats
 *     0..3 with the four levels, `add` = attribute 0x100 (stacks instead of replacing), time = BtlSkill_GetFrames.
 *     Attribute 0x200: full ki, BtlMember_AddMaxPower(30000), held flag 6 (powered-up mode), flag 0xBB.
 *     Attribute 0x800000: BtlStat_SetPenalty(stat 2, -10). Attribute 0x800: kind 1 -> skillTimerC, kind 2 ->
 *     kind2On, kind 3 -> kind3On. Attribute 0x400: skillTimerD. Health / ki changes are applied with
 *     BtlMember_AddHealth / Damage(flags 0x41B) / AddKi / SpendKi(force). Skill ids: 0xC, 0x32 skillStackA;
 *     0x33 skillTimer; 0x37 skillStackB; 0x17 costs 3000 health; 0x18 held flag 0x98 + flag 0x99, full ki and
 *     skillKiRate = ki maximum / frames; 0x41 skillCount3. Then the use counter of the slot, slotOn (kind != 0)
 *     and the blast stock cost.
 *
 * ---- Decision functions ----------------------------------------------------------------------------------------
 *
 * Called from the decide phase of the handlers that accept input, with a mask that enables groups. Each returns the
 * number of actions it queued (BtlAct_SetQueue, slot 0 first); the caller then requests queue[0]. The neutral
 * action 0xB calls BtlDecide_Main(0x22FCBFEB), BtlDecide_Attack(0x0800054F), BtlDecide_Common(0xF) in this order:
 * each later call overwrites slot 0, so attacks win over movement and the forced transitions win over both.
 * Both tables return 0 at once unless BtlChar_IsFree(chr).
 *
 * Input ids are those of BtlInput_TestAction (decimal, table in btl_char_ctl.h), tested with want = 1. Fighter
 * flags: 5 lock-on, 6 powered-up mode, 0xE in flight, 0xF on the ground, 0x11 in water (inferred), 0x13 close to
 * the opponent. "mark N" = BtlColl_AddActionBit(chr, N) is called whenever the group is reached, input or not.
 *
 * BtlDecide_Common(chr, mask), in this order:
 *   mask 8   chr+0xFE0 > 0 (stun countdown)                                       -> 0xD3 stun
 *   mask 4   flag 0xBE                                                             -> 0xEA
 *   mask 1   flags 0xE and 0xF both clear, and 0xF clear last frame                -> 0x11 fall
 *   mask 2   BtlChar_IsDead                                                        -> 0xD2
 *
 * BtlDecide_Main(chr, mask), in this order (first hit wins):
 *   mask         state conditions                                   input                      -> action
 *   0x10000000   health >= 5000; mark 0x42                          46 lock-on double tap      -> 0x42 (costs 5000 health)
 *   0x400        ki >= 1000                                         21 dir + dash/charge press -> 0x1A free dash
 *                ... and lock-on                                    20 no dir + the same       -> 0x19 homing dash
 *   0x8000000    ki >= 10000, lock-on, not close (or the opponent
 *                is seen in action 0xD5)                            20                         -> 0x33 circle dash
 *   0x800                                                           15 ascend double tap       -> 0x17 fast ascend
 *   0x1000       not on the ground                                  17 descend double tap      -> 0x18 fast descend
 *   0x2 / 0x4    on the ground, not in water, not in flight         8 ascend pressed           -> 0x10 (mask 2), else 0x12
 *   0x8 / 0x10   not close, or mask 0x10                            3 dash held (no R3)        -> 0xF dash move
 *   0x2000/0x4000 lock-on; close, or mask 0x4000                    25 / 26 / 27 dash press +
 *                                                                   down / left / right        -> 0x1B / 0x1C / 0x1D step
 *   0x8000/0x10000 parameter flags2 & 1, lock-on; close, or
 *                mask 0x10000                                       28 dash press + up         -> 0x1E step in
 *   0x20000      flags2 & 8, lock-on, ki >= BtlParam_GetAmountA     32 / 29 / 30 / 31 guard
 *                                                                   press + up/down/left/right -> 0x23 / 0x20 / 0x21 / 0x22 vanishing step
 *   0x40000      lock-on, close, ki >= BtlParam_GetAmountB,
 *                flag 0xB3, chr+0x1080 > 0; mark 0x24               33 guard press             -> 0x24 vanish behind the opponent
 *   0x20000000   BtlAct_TestPoweredSkill(chr, 8)                    49 dir + guard press,
 *                                                                   charge not held            -> 0x35 vanished dash
 *   0x100000     BtlAct_CheckTechniqueInput (inputs 108..110: techniques, btl_act_j.c)
 *   0x800000     BtlAct_CheckSkillInput (inputs 113, 114: skills -> actions 0xFD..0x102)
 *   0x80         flags 6 and 0x98 clear; if parameter word 0 & 0x80:
 *                ki full and one blast stock (100000)               19 charge held             -> 0x37 ki charge
 *   0x1000000    flags2 & 0x20, BtlMove_IsBlastIncoming(1, 1, f, 90)
 *                with f = parameter flags & 2                       43 guard + side press      -> 0x3D (f set), else 0x3B
 *   0x2000000    flags2 & 0x20, BtlMove_IsBlastIncoming(0, 1, 0, 5) 39 guard press             -> 0x3C
 *   0x80000                                                         34 guard held              -> 0x38 guard; with lock-on and the
 *                                                                                                 opponent more than 1.7 rad off the
 *                                                                                                 facing: 0x3A (turn) then 0x38
 *   0x20         in flight or in water                              10 ascend held             -> BtlAct_GetAction14or16 (0x14 / 0x16)
 *   0x40         (in flight or in water) and not on the ground:
 *                cannot fly and not in water                        14 descend pressed         -> 0x11 fall
 *                otherwise                                          12 descend held            -> 0x15 descend
 *   0x400000     BtlAct_CheckSwitchInput (input 107: member switch -> 0xF3)
 *   0x100        BtlAct_CheckTransformInput (inputs 99..103 -> 0xEC..0xF0)
 *   0x200        BtlAct_CheckFusionInput (inputs 104..106 -> 0xF1 / 0xF2)
 *   0x1                                                             1 direction held (no R3)   -> 0xE close move if close, else
 *                                                                                                 0xD move (not when already in it)
 *   0x200000     flags 5 and 0x93 clear, flag 3 set                 none                       -> 0x36 search (lock-on lost)
 *
 * BtlDecide_Attack(chr, mask), in this order:
 *   0x1          mark 0x44                                          52 rush tap                -> 0x44 rush; with lock-on and the
 *                                                                                                 opponent more than 1.7 rad off: 0x3A, 0x44
 *   0x2          mark 0x47                                          53 / 54 / 55 / 56 rush held
 *                                                                   6 frames + up/down/left/
 *                                                                   right                      -> 0x47 / 0x48 / 0x49 / 0x4A charged smash
 *                                                                   57 the same, no direction  -> 0x4B if chr+0xD60 < 5, else 0x4C
 *   0x2000       mark 0x55                                          58..62 rush held + up /
 *                                                                   down / left / right / none -> 0x55 / 0x56 / 0x53 / 0x54 / 0x57 dash smash
 *   0x4000000    flags2 & 0x10000; mark 0x6D                        74, 75, 72, 73, 76 rush
 *                                                                   press + up / down / left /
 *                                                                   right / none               -> 0x6D / 0x6E / 0x6B / 0x6C / 0x6E
 *   0x4          ki >= BtlKiBlast_GetKiCostOf(0), chr+0xDE0 <
 *                BtlParam_GetCount80, BtlMove_CanFireBlast(0);
 *                mark 0xAE                                          89 blast tap               -> 0xAE ki blast (0x3A first as for 0x44)
 *   0x8          the same with cost kind 3, CanFireBlast(1);
 *                mark 0xAF                                          90 blast held 6 frames     -> 0xAF charged ki blast
 *   0x4000       flags2 & 0x10, cost kind 4, CanFireBlast(0);
 *                mark 0xB0                                          89                         -> 0xB0 ki blast while dashing
 *   0x8000       flags2 & 0x10, cost kind 7, CanFireBlast(1);
 *                mark 0xB1                                          90                         -> 0xB1 charged, while dashing
 *   0x10000                                                         64 rush press, no blast    -> 0x59 rising attack
 *   0x20000      flags2 & 0x10, cost kind 8, CanFireBlast(0);
 *                mark 0xB2                                          89                         -> 0xB2
 *   0x40000      flags2 & 0x10, cost kind 0xB, CanFireBlast(1);
 *                mark 0xB3                                          90                         -> 0xB3
 *   0x10         flags2 & 0x800; mark 0x5E                          77 up + blast press        -> 0x5E
 *   0x800000     the same                                           77                         -> by BtlParam_GetUnk8A(1): 0 -> 0x5E;
 *                                                                                                 1 -> 0x40 then 0x5E; else nothing
 *   0x20         flags2 & 0x1000; mark 0x5F                         78 down + blast press      -> 0x5F
 *   0x1000000    the same                                           78                         -> by BtlParam_GetUnk8A(2): 0x5F / 0x40, 0x5F
 *   0x200000     F = finisher BtlParam_GetUnk84((chr+0xD60 + 4) % 5)
 *                names: 0..4 -> 0x60, 0x61, 0x62, 0x63, 0x66;
 *                5 -> 0x64 with flag 0x8F, else 0x65; mark F        66 blast press             -> F (rush chain finisher)
 *   0x400000 / 0x2000000 / 0x8000000
 *                kind 0 (mask 0x400000), 3 (0x2000000), or 4 when
 *                BtlParam_GetCount80 <= 0; v = BtlParam_GetUnk8A(kind):
 *                v / 2 picks the finisher as above (0..5), odd v
 *                puts action 0x40 in front; mark the finisher       66 (kind 0, 3) or
 *                                                                   88 blast tap-or-hold (4)   -> [0x40,] finisher
 *   0x40 / 0x80  close, or mask 0x80; mark 0xB4                     92 dash double tap         -> 0xB4 grab
 *   0x100/0x200  close, or mask 0x200; flags2 & 0x80000; mark 0xB6  94 up + blast press        -> 0xB6 grab
 *   0x80000      chr+0xD68 < chr+0xD6C, ki >= 5000; mark 0x19       42 dash press              -> 0x19 homing dash
 *   0x100000     BtlDecide_QueueAttack(chr, 0x9B) first; then, if chr+0xD70 < chr+0xD74:
 *                flags2 & 0x2000; mark 0x5C                         68 / 69 / 70 / 71 blast
 *                                                                   press + up/down/left/right -> 0x5C / 0x5D / 0x5A / 0x5B vanish strike
 *                else, flags3 & 0x1000 and not BtlChar_TestMemberUnk70: BtlAct_QueueComboFinish
 *   0x400                                                           95 guard held + blast      -> 0x67
 *   0x1000                                                          96 guard press, no dir     -> 0x68
 *   0x800                                                           96                         -> 0x40 if BtlParam_GetUnk88(1) == 5, else 0x69
 *
 * BtlDecide_QueueAttack(chr, id): `id` is an attack action 0x70.. with a record in the roster's attack table.
 *   Needs BtlAct_TestAttackSkill. record +5 (kind): 0 always; 1..8 only when the opponent is seen in action
 *   0xD4 / 0xD5 / 0xD0 / 0xCD / 0xCB / 0xC6 / 0xC7 or 0xC8 / 0xCE, and never on the frame flag 0x5B is raised.
 *   record +2 (1..9): input 79..87 must be satisfied (rush / blast / guard press, plain, with up, with down).
 *   record +1 (lead-in): 0 none; 1: 0x2B; 2: 0x2B, 0x2D; 3: 0x2B, 0x2E; 4: 0x21 or 0x22 by BtlChar_FrameMod(2);
 *   5: 0x2B, 0x2F; 6: 0x31; 7: 0x32; 8: 0x2B, 0x30; 9: 0x2C, 0x2D. Then the attack itself and BtlAct_PrepareAttack.
 */

/* Partial view of a fighter (0x1600 bytes). */
typedef struct BtlActIChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ u8 unk4[0x3D0 - 0x4];
    /* 0x03D0 */ s32 work[0x14];     /* per-action scratch, zeroed on every action switch */
    /* 0x0420 */ u8 unk420[0x964 - 0x420];
    /* 0x0964 */ s32 actionFrame;    /* frames spent in the current action */
    /* 0x0968 */ u8 unk968[0xCDC - 0x968];
    /* 0x0CDC */ s32 switchChara;    /* member switch target: the arguments of BtlChange_RequestChara */
    /* 0x0CE0 */ s32 switchCostume;
    /* 0x0CE4 */ s32 switchVariant;
    /* 0x0CE8 */ s32 switchAnimChara;
    /* 0x0CEC */ s32 switchUnk18;
    /* 0x0CF0 */ s32 switchVoiceChara;
    /* 0x0CF4 */ u8 unkCF4[0xD60 - 0xCF4];
    /* 0x0D60 */ s32 unkD60;         /* rush combo step: < 5 picks action 0x4B over 0x4C; (unkD60 + 4) % 5 indexes parameter +0x84 */
    /* 0x0D64 */ s32 unkD64;
    /* 0x0D68 */ s32 unkD68;         /* counter with limit unkD6C: action 0x19 from the attack table needs unkD68 < unkD6C */
    /* 0x0D6C */ s32 unkD6C;
    /* 0x0D70 */ s32 unkD70;         /* counter with limit unkD74: below it actions 0x5A..0x5D, else the combo finisher */
    /* 0x0D74 */ s32 unkD74;
    /* 0x0D78 */ u8 unkD78[0xDE0 - 0xD78];
    /* 0x0DE0 */ s32 unkDE0;         /* ki blasts fired in a row: must be below BtlParam_GetCount80 to fire another */
    /* 0x0DE4 */ u8 unkDE4[0xE00 - 0xDE4];
    /* 0x0E00 */ s32 skillStackA;    /* skill ids 0x0C / 0x32: 1 on use, +1 up to 3 when the skill stacks */
    /* 0x0E04 */ s32 skillStackB;    /* skill id 0x37: the same */
    /* 0x0E08 */ s32 skillTimer;     /* skill id 0x33: frames left; flag 0x56 every frame while > 0 */
    /* 0x0E0C */ s32 skillSlot;      /* slot of the skill that set one of the three fields above */
    /* 0x0E10 */ s32 skillKiRate;    /* skill id 0x18: ki maximum / duration */
    /* 0x0E14 */ s32 skillTimerC;    /* frames left of a kind 1 skill with attribute 0x800; held at 1 while kind2On / kind3On */
    /* 0x0E18 */ s32 skillTimerD;    /* frames left of a skill with attribute 0x400 */
    /* 0x0E1C */ s32 skillTimerE;    /* frames left for skillValE */
    /* 0x0E20 */ s32 skillValE;      /* BtlSkill_GetValE when positive; 0 when its timer runs out */
    /* 0x0E24 */ s32 skillTimerF;    /* frames left for skillValF */
    /* 0x0E28 */ s32 skillValF;      /* BtlSkill_GetValF when positive */
    /* 0x0E2C */ s32 unkE2C;
    /* 0x0E30 */ s32 kind2On;        /* set by a kind 2 skill with attribute 0x800 (cleared by BtlStat_EndKind2) */
    /* 0x0E34 */ s32 kind3On;        /* the same for kind 3 */
    /* 0x0E38 */ s32 slotOn[2];      /* 1 once a skill of kind != 0 was applied from the slot */
    /* 0x0E40 */ u8 unkE40[0xE5C - 0xE40];
    /* 0x0E5C */ s32 skillCount3;    /* skill id 0x41: +1, clamped to 0..3 */
    /* 0x0E60 */ u8 unkE60[0xFE0 - 0xE60];
    /* 0x0FE0 */ s32 stunTimer;      /* > 0: BtlDecide_Common(mask 8) queues action 0xD3 */
    /* 0x0FE4 */ u8 unkFE4[0x1080 - 0xFE4];
    /* 0x1080 */ s32 unk1080;        /* > 0 needed (with flag 0xB3) for action 0x24 */
    /* 0x1084 */ u8 unk1084[0x1600 - 0x1084];
} BtlActIChr; /* 0x1600 */

/* Pose block (BtlChar_GetPos). */
typedef struct BtlActIPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ u8 unk10[0x98 - 0x10];
    /* 0x98 */ f32 unk98;
    /* 0x9C */ f32 unk9C;
} BtlActIPose;

/* Battle object (BtlChar_GetObj). */
typedef struct BtlActIObj {
    /* 0x000 */ u8 unk0[0xA40];
    /* 0xA40 */ u32 flags;
} BtlActIObj;

/* Attack table record (roster + 0x2C, index = attack id - 0x70): the fields BtlDecide_QueueAttack reads. */
typedef struct BtlActIAttackRec {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ s8 leadIn;    /* 0..9: action(s) queued in front of the attack */
    /* 0x02 */ s8 inputKind; /* 1..9: input id 79..87 that must be satisfied; else none */
    /* 0x03 */ u8 unk3;
    /* 0x04 */ s8 parts;
    /* 0x05 */ s8 kind;      /* 0: always; 1..8: the opponent must be seen in a given action */
    /* 0x06 */ u8 unk6[0x18 - 0x6];
} BtlActIAttackRec; /* 0x18 */

/* Fighter roster (gBtlChars). */
typedef struct BtlActIRoster {
    /* 0x00 */ u8 unk0[0x2C];
    /* 0x2C */ BtlActIAttackRec *attackTbl;
} BtlActIRoster;

/* Active member's gauge block (BtlMember_GetActiveGauge). */
typedef struct BtlActIGauge {
    /* 0x00 */ s32 health;
    /* 0x04 */ s32 healthMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ u8 unk14[0x38 - 0x14];
    /* 0x38 */ s32 skillUses[2];     /* times each skill slot was used */
} BtlActIGauge;

s32 BtlAct_SwitchArriveWait(BtlActIChr *chr, s32 phase);
s32 BtlAct_SwitchArriveLand(BtlActIChr *chr, s32 phase);
s32 BtlAct_KoSwitchLoad(BtlActIChr *chr, s32 phase);
s32 BtlAct_KoSwitchEnterWait(BtlActIChr *chr, s32 phase);
s32 BtlAct_KoSwitchFlyIn(BtlActIChr *chr, s32 phase);
s32 BtlAct_RoundReset(BtlActIChr *chr, s32 phase);
void BtlSkill_PlayAnim(BtlActIChr *chr, s32 cls);
void BtlSkill_SetFxBit(BtlActIChr *chr, s32 cls);
void BtlSkill_SetFlag(BtlActIChr *chr, s32 cls);
void BtlSkill_Apply(BtlActIChr *chr, s32 slot);
void BtlSkill_UpdateTimers(BtlActIChr *chr);
s32 BtlAct_SkillBasic(BtlActIChr *chr, s32 phase);
s32 BtlAct_SkillTeleport(BtlActIChr *chr, s32 phase);
s32 BtlAct_SkillInstant(BtlActIChr *chr, s32 phase);
s32 BtlDecide_Common(BtlActIChr *chr, s32 mask);
s32 BtlDecide_Main(BtlActIChr *chr, s32 mask);
s32 BtlDecide_Attack(BtlActIChr *chr, s32 mask);
s32 BtlDecide_QueueAttack(BtlActIChr *chr, s32 id);

#endif
