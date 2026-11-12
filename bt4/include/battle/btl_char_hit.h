#ifndef BATTLE_BTL_CHAR_HIT_H
#define BATTLE_BTL_CHAR_HIT_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter hit resolution (src/battle/btl_char_hit.c, 0x1C7B30..0x1CA520): does an attack connect, is it avoided,
 * guarded or traded, and what it does to the defender. Called once per frame from the collision step at 0x1CA520
 * (the next module), in this order:
 *
 *   for each fighter: BtlHit_BeginFrame
 *   BtlHit_CheckClash        (returns 1: stop)
 *   BtlHit_CheckRush         (returns 1: stop)
 *   BtlHit_CheckProximityAll
 *   for each fighter: BtlColl_UpdateFrozen, BtlHit_TestHit
 *   BtlHit_ResolveTrades
 *   for each fighter: BtlHit_ApplyHit
 *
 * A hit is not an object. It is: the attacker's current attack id (BtlAtk_GetId, -1 = none), the attack record that
 * id selects (0x30 bytes at fighter object + 0x920, read through the func_0020Dxxx accessors, not decompiled), the
 * pending target HitStatus.target of the attacker, and the HitReact block that BtlHit_ApplyHit fills on the defender.
 *
 * All structures are partial views local to this module. Names are guesses unless the comment gives the evidence.
 */

/* Results passed to BtlHit_ApplyGuard. */
#define BTL_GUARD_PLAIN      0 /* attack record guard result 0 */
#define BTL_GUARD_FLAG40     1 /* defender flag 0x40, attack id below 11 */
#define BTL_GUARD_TIMED_A    2 /* defender flag 0x41, input 0x10 held: defender flag 0x68 */
#define BTL_GUARD_TIMED_B    3 /* input 0x20 held: defender flag 0x69 */
#define BTL_GUARD_TIMED_C    4 /* neither: defender flag 0x6A */
#define BTL_GUARD_KI         5 /* guard result 1: defender pays ki, flag 0xBE when it runs out */
#define BTL_GUARD_PUSH       6 /* guard result 2: attacker flag 0x6F, defender + 50000 blast */
#define BTL_GUARD_PUSH_STOP  7 /* guard result 4: the same with hit-stop and voice 0x12 */
#define BTL_GUARD_COUNTER    8 /* defender level at + 0x1070: flag 0x7B, hit-stop, no chip damage */

/* Reaction ids (HitReact.reaction) this module tests by value. */
#define BTL_REACT_NONE       1    /* nothing happens; also the value BtlHit_BeginFrame writes every frame */
#define BTL_REACT_NO_FLINCH  2    /* the hit lands and hurts but the defender keeps its action */
#define BTL_REACT_GRAB       9    /* BtlHit_CheckGrab */
#define BTL_REACT_GRAB_B     0xE  /* BtlHit_CheckGrab, defender animation flag 0x800 */

/* The pose block, fighter + 0x10 (BtlChar_GetPos). */
typedef struct HitPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;       /* rot.y is the model yaw */
    /* 0x20 */ u8 unk20[0x64 - 0x20];
    /* 0x64 */ f32 unk64;      /* added to rot.y by BtlHit_IsFromBehind */
    /* 0x68 */ u8 unk68[0x90 - 0x68];
    /* 0x90 */ f32 unk90;      /* used as the launch pitch with attack flag 0x200000 */
    /* 0x94 */ f32 yaw;        /* facing yaw (fighter + 0xA4) */
} HitPose;

/* The fighter object's work buffer (BtlObj + 0x1660). */
typedef struct HitObjWork {
    /* 0x00000 */ u8 unk0[0x18020];
    /* 0x18020 */ u32 unk18020; /* & 0xF000000: the model has active hit volumes this frame */
} HitObjWork;

/* The fighter's battle object (BtlChar_GetObj). */
typedef struct HitObj {
    /* 0x0000 */ u8 unk0[0xCAC];
    /* 0x0CAC */ s8 unkCAC;    /* when hitNo == ~unkCAC the animation's last hit has been reached (inferred) */
    /* 0x0CAD */ s8 hitNo;     /* number of the animation's current hit, negative when none */
    /* 0x0CAE */ u8 unkCAE[0x1660 - 0xCAE];
    /* 0x1660 */ HitObjWork *work;
} HitObj;

/* First words of a member entry (BtlMember_GetActive). */
typedef struct HitMember {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 costume;
} HitMember;

/* Gauge block of the active member (BtlMember_GetActiveGauge). */
typedef struct HitGauge {
    /* 0x00 */ s32 health;
    /* 0x04 */ s32 healthMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ s32 blast;
    /* 0x18 */ s32 blastMax;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 variant;
} HitGauge;

/* fighter + 0xF30 */
typedef struct HitStatus {
    /* 0x00 */ Vec4 unk0;      /* not used here */
    /* 0x10 */ s32 target;     /* fighter + 0xF40: player index this fighter's attack hits this frame, -1 = none */
    /* 0x14 */ s32 lastHitNo;  /* fighter + 0xF44: HitObj.hitNo of the previous frame */
    /* 0x18 */ s32 contact;    /* fighter + 0xF48: frames flag 0x65 has been seen by BtlHit_TestHit */
} HitStatus;

/* fighter + 0xFB0: what the last hit does to this fighter. Read by the action code (not decompiled). */
typedef struct HitReact {
    /* 0x00 */ s32 reaction;   /* reaction id from the attack record (1..49), 1 = none */
    /* 0x04 */ s32 unk4;       /* attack record + 0x20, or 0x13 when the attacker's member flag unk30 is set */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 back;       /* hit from behind; for launches attack flag 0x1000 instead; 0 for a grab */
    /* 0x10 */ s32 unk10;      /* attack 0x55: bit 19 of the attacker's parameter flags; written on both fighters */
    /* 0x14 */ s32 silent;     /* attack flag 0x400: no hurt voice */
    /* 0x18 */ f32 scale;      /* attacker + 0xD78, times the defender's parameter float + 0x60 unless flag 0x2000000 */
    /* 0x1C */ f32 yaw;        /* direction the hit pushes */
    /* 0x20 */ f32 turnYaw;    /* attack flag 0x100: yaw the defender is turned to */
    /* 0x24 */ f32 turnPitch;
    /* 0x28 */ f32 launchA;    /* launch reactions: attack record + 0x15 as an angle */
    /* 0x2C */ f32 launchB;    /* attack record + 0x16 as an angle */
    /* 0x30 */ u8 unk30[0x38 - 0x30];
    /* 0x38 */ s32 unk38;      /* 3 with reaction 2 */
    /* 0x3C */ s32 damage;     /* reactions 0x14, 0x16, 0x25: the damage, applied later by other code */
    /* 0x40 */ u8 unk40[0x48 - 0x40];
    /* 0x48 */ s32 unk48;      /* grab: BtlSkill_GetFrames(attacker, direction) */
} HitReact;

/* Partial view of a fighter (0x1600 bytes). */
typedef struct HitChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ HitPose pose;
    /* 0x00A8 */ u8 unkA8[0xD78 - 0xA8];
    /* 0x0D78 */ f32 unkD78;   /* copied into HitReact.scale of whoever this fighter hits */
    /* 0x0D7C */ s32 unkD7C;
    /* 0x0D80 */ s32 unkD80;   /* a gauge capped at 100000: + attack record + 0x10 per hit, + 50000 per guard result 1 */
    /* 0x0D84 */ s32 unkD84;
    /* 0x0D88 */ s32 unkD88;   /* 1..: attacks 0x75..0x78 add (own + target's - 1, clamped to 0..5) fifths of damage */
    /* 0x0D8C */ u8 unkD8C[0xE00 - 0xD8C];
    /* 0x0E00 */ s32 dodges;   /* automatic evasions left (side step, flags 0x74..0x76) */
    /* 0x0E04 */ s32 dodgesB;  /* automatic evasions left of the second kind (flag 0x77) */
    /* 0x0E08 */ s32 unkE08;
    /* 0x0E0C */ s32 unkE0C;   /* 0 / 1: which of flags 0xDE / 0xDF a used evasion raises; added to the damage flags
                                  of attacks 0x61 / 0x62 */
    /* 0x0E10 */ s32 unkE10;
    /* 0x0E14 */ s32 unkE14;   /* > 0: + 1 armour level */
    /* 0x0E18 */ s32 unkE18;   /* > 0: every non-launch reaction becomes 2 (no flinch) */
    /* 0x0E1C */ u8 unkE1C[0xF30 - 0xE1C];
    /* 0x0F30 */ HitStatus status;
    /* 0x0F4C */ u8 unkF4C[0xFB0 - 0xF4C];
    /* 0x0FB0 */ HitReact react;
    /* 0x0FFC */ u8 unkFFC[0x1068 - 0xFFC];
    /* 0x1068 */ s32 unk1068;  /* level: > 0 (1 with attack flag 0x4000000) avoids the first hit of a flag-1 attack */
    /* 0x106C */ s32 unk106C;
    /* 0x1070 */ s32 unk1070;  /* level: > 0 counters (guard result 8) from a 0x31 animation */
    /* 0x1074 */ s32 unk1074;
    /* 0x1078 */ s32 unk1078;  /* level: > 0 breaks a throw (attack 0x55) */
    /* 0x107C */ s32 unk107C;  /* level: > 0 breaks / evades a rush */
    /* 0x1080 */ u8 unk1080[0x1324 - 0x1080];
    /* 0x1324 */ s32 stopLen;  /* hit-stop: pending length */
    /* 0x1328 */ s32 stopDelay; /* hit-stop: delay */
    /* 0x132C */ u8 unk132C[0x1600 - 0x132C];
} HitChr;

s32 BtlHit_IsFromBehind(HitChr *atk, HitChr *def);
s32 BtlHit_GetVoiceKind(s32 reaction);
s32 BtlHit_GetHitNo(HitChr *chr);
void BtlHit_ApplyGuard(HitChr *atk, HitChr *def, u32 result);
s32 BtlHit_CheckNear(HitChr *a, HitChr *b);
s32 BtlHit_CheckGrab(HitChr *a, HitChr *b);
s32 BtlHit_Stub(void);
s32 BtlHit_CheckTouch(HitChr *a, HitChr *b);
s32 BtlHit_IsVoid(HitChr *atk, HitChr *def);
s32 BtlHit_CheckDodge(HitChr *atk, HitChr *def);
s32 BtlHit_CheckArmor(HitChr *atk, HitChr *def);
s32 BtlHit_CheckRepel(HitChr *atk, HitChr *def);
s32 BtlHit_CheckGuard(HitChr *atk, HitChr *def, s32 *full);
s32 BtlHit_CheckThrowBreak(HitChr *atk, HitChr *def);
void BtlHit_BeginFrame(HitChr *chr);
s32 BtlHit_CheckClash(void);
s32 BtlHit_CheckRush(void);
void BtlHit_CheckProximityAll(void);
void BtlHit_TestHit(HitChr *chr);
void BtlHit_ResolveTrades(void);
void BtlHit_ApplyHit(HitChr *atk);

/* 0x1CA520..0x1CA6D0, the tail of the same object (include/battle/btl_char_coll.h declares them with its own view). */
void BtlColl_Update(void);
void BtlColl_UpdateFrozen(HitChr *chr);
s32 BtlColl_GetGuardKind(HitChr *chr);
s32 BtlColl_IsGuarding(HitChr *chr);

#endif
