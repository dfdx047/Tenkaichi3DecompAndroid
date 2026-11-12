#ifndef BATTLE_BTL_CHAR_COLL_H
#define BATTLE_BTL_CHAR_COLL_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter side of hit resolution (0x1CA520..0x1CA6D0, the tail of src/battle/btl_char_hit.c, and
 * src/battle/btl_char_coll_b.c 0x1CA6D0..0x1CDCA8): what a fighter does when the hit detection of the effect scene (0x1AF740..0x1B10F0)
 * reports that a hit record overlaps it. The structures are partial views local to this module: only the fields
 * these files touch. Names are guesses unless the comment says how they are known.
 */

/* Kind of hit record (BtlCollHit.type). */
#define BTL_HIT_BLAST 0 /* a projectile / energy attack; its parameters are BtlCollHit.atk */
#define BTL_HIT_TECH  1 /* a technique of a fighter; BtlCollHit.owner says whose and which slot */

/* Reaction ids stored in BtlCollReact.id that these files write or compare. */
#define BTL_REACT_NOFLINCH 2    /* the hit is taken without a reaction (damage still applies) */
#define BTL_REACT_CAUGHT   0x22 /* victim of a throw (BtlColl_StartThrow) */

/* Who launched a BTL_HIT_TECH record (BtlCollHit.owner). */
typedef struct BtlCollOwner {
    /* 0x0 */ s32 side; /* roster index of the attacker */
    /* 0x4 */ s32 slot; /* technique slot: 0, 1 ("strike": readers at 0x212080..0x212760) or 2..4 ("rush": readers
                           at 0x210D48..0x211AF0); the same number BtlAct_GetCurrentClass returns */
} BtlCollOwner;

/* Parameters of a blast (BtlCollHit.atk). */
typedef struct BtlCollAtk {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s16 side;   /* roster index of the fighter that fired it */
    /* 0x12 */ u8 unk12[0x18 - 0x12];
    /* 0x18 */ s16 kind;   /* 1..3 raise event 0x36 on a hit; 0, 4, 8 need flag 0x50 to be deflected, others 0x51 */
    /* 0x1A */ u8 unk1A[0x41 - 0x1A];
    /* 0x41 */ u8 hits;    /* number of hits the damage is spread over (divisor when >= 2) */
} BtlCollAtk;

/* One hit record of the effect scene's list (the 0x190-byte "blast record" of btl_scene.h, 64 of them, emptied
   every frame). The hit detection (0x1B0030) passes the record and the target (owner ^ 1) to this module. */
typedef struct BtlCollHit {
    /* 0x00 */ s32 ownerId;    /* roster index / object id of the attacker (BtlChar_Get index) */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;       /* compared with BtlSuper_GetUnk1E(chr, class) in BtlColl_TryDodge */
    /* 0x0C */ s32 type;       /* BTL_HIT_* */
    /* 0x10 */ u8 unk10[0x10];
    /* 0x20 */ Vec4 pos;       /* where it hit: sounds, camera shake and rumble are placed here */
    /* 0x30 */ u8 unk30[0x10];
    /* 0x40 */ Vec4 vel;       /* travel direction of the hit */
    /* 0x50 */ s32 flags;      /* bit 0x10: second variant of a rush hit (other reaction table, sound and a push) */
    /* 0x54 */ s32 unk54;      /* non-zero: direction taken from the hit position instead of vel; 1: cannot be
                                  deflected, reflected or absorbed */
    /* 0x58 */ u8 unk58[0xC];
    /* 0x64 */ BtlCollOwner *owner;
    /* 0x68 */ BtlCollAtk *atk;
} BtlCollHit;

/* Active member's gauge block (BtlMember_GetActiveGauge). */
typedef struct BtlCollGauge {
    /* 0x00 */ s32 health;
    /* 0x04 */ u8 unk4[0x20 - 0x4];
    /* 0x20 */ s32 unk20;
} BtlCollGauge;

/* Pose block of the fighter (chr + 0x10, BtlChar_GetPos). */
typedef struct BtlCollVec {
    f32 x, y, z, w;
} __attribute__((aligned(8))) BtlCollVec;

typedef struct BtlCollPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;        /* rot.y: yaw the hit direction is compared with */
    /* 0x20 */ Vec4 unk20;      /* unk20.y: distance from the origin to the feet */
    /* 0x30 */ u8 unk30[0x50];
    /* 0x80 */ Vec4 unk80;      /* forward vector (used as the deflect direction for a motionless blast) */
    /* 0x90 */ f32 unk90;
    /* 0x94 */ f32 yaw;         /* facing yaw (fighter + 0xA4) */
    /* 0x98 */ f32 unk98;
    /* 0x9C */ f32 velY;        /* vertical speed: zeroed on landing, halved when flag 0x11 rises */
    /* 0xA0 */ u8 unkA0[0x10];
    /* 0xB0 */ BtlCollVec groundPos;  /* copy of the ground query below the fighter */
    /* 0xC0 */ BtlCollVec groundNrm;
    /* 0xD0 */ s32 groundFlags;
} BtlCollPose;

/* Result of the stage query under the fighter, inside the object's work buffer. */
typedef struct BtlCollGround {
    /* 0x00 */ BtlCollVec pos;
    /* 0x10 */ BtlCollVec nrm;
    /* 0x20 */ s32 flags;
} BtlCollGround;

/* Work buffer of a fighter's battle object (BtlObj + 0x1660, 0x1A0C0 bytes). */
typedef struct BtlCollObjWork {
    /* 0x00000 */ u8 unk0[0x18020];
    /* 0x18020 */ s32 hitFlags;    /* stage contact bits: 0x20, 0x40, 0x80, 0x100, 0x200 */
    /* 0x18024 */ u8 unk18024[0xC];
    /* 0x18030 */ BtlCollGround ground;
    /* 0x18058 */ u8 unk18058[0x8];
    /* 0x18060 */ s32 unk18060;    /* bits 0xC0 */
} BtlCollObjWork;

/* Battle object of a fighter. */
typedef struct BtlCollObj {
    /* 0x0000 */ u8 unk0[0xCAC];
    /* 0x0CAC */ s8 unkCAC;
    /* 0x0CAD */ s8 unkCAD;
    /* 0x0CAE */ u8 unkCAE[0x1660 - 0xCAE];
    /* 0x1660 */ BtlCollObjWork *work;
} BtlCollObj;

/* Throw / catch description shared by the two fighters of a throw (fighter + 0xE90). The attacker fills its own
   copy and the fields are then copied into the victim's. */
typedef struct BtlCollThrow {
    /* 0x00 */ s32 tech;      /* BtlSuper_GetId(attacker, slot) */
    /* 0x04 */ s32 atkSide;
    /* 0x08 */ s32 defSide;
    /* 0x0C */ s32 slot;
    /* 0x10 */ s32 unk10;     /* BtlSuper_GetStepCount */
    /* 0x14 */ s32 unk14;     /* BtlSuper_GetUnk14D */
    /* 0x18 */ s32 unk18;     /* BtlSuper_GetUnk220, or -1 */
    /* 0x1C */ s32 unk1C;     /* BtlSuper_GetLastStep, or -1 */
    /* 0x20 */ s32 unk20;     /* BtlSuper_GetThrowChara; -1 in the victim */
    /* 0x24 */ s32 unk24;     /* BtlSuper_GetThrowCostume; 0 in the victim */
    /* 0x28 */ s32 unk28;     /* BtlSuper_GetThrowGauge20; 0 in the victim */
    /* 0x2C */ s32 unk2C;     /* BtlSuper_GetUnk226; 1 in the victim */
    /* 0x30 */ s32 unk30;     /* technique bit 0x400000 */
    /* 0x34 */ s32 unk34;     /* technique bit 0x400: BtlChar_FrameMod(2) != 0 */
    /* 0x38 */ s32 unk38;     /* technique bit 0x100000 */
    /* 0x3C */ s32 unk3C;     /* technique bit 0x800000 */
    /* 0x40 */ s32 unk40;     /* BtlSuper_GetFlagsA bit 0x2000000 */
    /* 0x44 */ s32 unk44;     /* 1 when the catch reaction is 0x1E */
    /* 0x48 */ s32 unk48;     /* technique bit 0x2000 and attacker gauge unk20 == 0 */
    /* 0x4C */ s32 unk4C;     /* technique bit 0x80000 and victim gauge unk20 == 0 */
    /* 0x50 */ s32 unk50;     /* technique bit 0x800 */
    /* 0x54 */ s32 unk54;     /* technique bit 0x10000 */
    /* 0x58 */ s32 unk58;     /* victim's state has table bit 0x800 */
    /* 0x5C */ s32 unk5C;     /* technique bit 0x4000000 */
    /* 0x60 */ f32 unk60;     /* BtlSuper_GetThrowAngleA */
    /* 0x64 */ f32 unk64;     /* BtlSuper_GetThrowAngleB */
} BtlCollThrow; /* size 0x68 */

/* Pending hit reaction of a fighter (fighter + 0xFB0). */
typedef struct BtlCollReact {
    /* 0x00 */ s32 id;        /* reaction to start; 0 = none, 2 = no flinch, 0x22 = caught; a throw needs the
                                 attacker's id < 3 */
    /* 0x04 */ s32 unk4[2];
    /* 0x0C */ s32 back;      /* BtlColl_IsFacing(chr, dirYaw): the hit travels the way the fighter faces, i.e. it
                                 comes from behind; for a throw: technique bit 0x10000000 */
    /* 0x10 */ s32 unk10[2];
    /* 0x18 */ s32 unk18;
    /* 0x1C */ f32 dirYaw;    /* yaw of the hit direction */
    /* 0x20 */ f32 faceYaw;   /* yaw to turn to (with flag 0x94) */
    /* 0x24 */ f32 unk24;
    /* 0x28 */ s32 unk28[2];
    /* 0x30 */ s32 stun;      /* > 0: not free (BtlChar_IsFree); set from BtlSkill_GetFrames for reactions 0x17..0x1C */
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 unk3C[2];
    /* 0x44 */ s32 slot;      /* slot of the rush attack that hit */
} BtlCollReact;

/* Fighter (0x1600 bytes). */
typedef struct BtlCollChr {
    /* 0x0000 */ s32 side;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 unk8;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ BtlCollPose pose;
    /* 0x00E8 */ u8 unkE8[0xE00 - 0xE8];
    /* 0x0E00 */ s32 dodgeA;       /* counters consumed by an automatic dodge */
    /* 0x0E04 */ s32 dodgeB;
    /* 0x0E08 */ s32 unkE08;
    /* 0x0E0C */ s32 unkE0C;       /* 0 / 1: which of flags 0xDE / 0xDF the dodge sets */
    /* 0x0E10 */ s32 unkE10;
    /* 0x0E14 */ s32 unkE14;
    /* 0x0E18 */ s32 noFlinch;     /* > 0: every hit gets reaction 2 (no flinch) */
    /* 0x0E1C */ u8 unkE1C[0xE44 - 0xE1C];
    /* 0x0E44 */ f32 unkE44;
    /* 0x0E48 */ u8 unkE48[0xE90 - 0xE48];
    /* 0x0E90 */ BtlCollThrow thr;
    /* 0x0EF8 */ u8 unkEF8[0xF40 - 0xEF8];
    /* 0x0F40 */ s32 unkF40;
    /* 0x0F44 */ u8 unkF44[0xFB0 - 0xF44];
    /* 0x0FB0 */ BtlCollReact react;
    /* 0x0FF8 */ u8 unkFF8[0x106C - 0xFF8];
    /* 0x106C */ s32 unk106C;      /* > 0: dodge allowed without using a counter; zeroed by a rush hit */
    /* 0x1070 */ s32 unk1070;
    /* 0x1074 */ s32 unk1074;
    /* 0x1078 */ u8 unk1078[0x1288 - 0x1078];
    /* 0x1288 */ u64 actBits;      /* classes of the actions started this frame (BtlColl_AddActionBit) */
    /* 0x1290 */ s32 unk1290;
    /* 0x1294 */ s32 unk1294;
    /* 0x1298 */ u8 unk1298[0x1360 - 0x1298];
    /* 0x1360 */ Vec4 deflectDir;  /* direction a deflected blast is sent in */
    /* 0x1370 */ u8 unk1370[0x1600 - 0x1370];
} BtlCollChr; /* size 0x1600 */

void BtlColl_Update(void);
void BtlColl_UpdateFrozen(BtlCollChr *chr);
s32 BtlColl_GetGuardKind(BtlCollChr *chr);
s32 BtlColl_IsGuarding(BtlCollChr *chr);
s32 BtlColl_StartThrow(BtlCollChr *atk, BtlCollChr *def, s32 slot, u32 techFlags);
s32 BtlColl_IsFacing(BtlCollChr *chr, f32 yaw);
f32 BtlColl_GetHitDir(BtlCollChr *chr, BtlCollHit *hit, Vec4 *outDir);
void BtlColl_PlayHitSound(BtlCollChr *chr, BtlCollHit *hit, u32 kind);
s32 BtlColl_GuardBlast(BtlCollChr *chr, BtlCollHit *hit);
s32 BtlColl_GuardStrike(BtlCollChr *chr, BtlCollHit *hit);
s32 BtlColl_GuardRush(BtlCollChr *chr, BtlCollHit *hit);
s32 BtlColl_HitByBlast(BtlCollChr *chr, BtlCollHit *hit);
s32 BtlColl_HitByStrike(BtlCollChr *chr, BtlCollHit *hit);
void BtlColl_ApplyRushHit(BtlCollChr *chr, BtlCollChr *atk, BtlCollHit *hit, s32 react, f32 yaw, s32 front, Vec4 *dir);
void BtlColl_ApplyRushNoFlinch(BtlCollChr *chr, BtlCollChr *atk, BtlCollHit *hit, f32 yaw, s32 front);
s32 BtlColl_StartRushCatch(BtlCollChr *chr, BtlCollChr *atk, BtlCollHit *hit, s32 react, f32 yaw);
s32 BtlColl_HitByRush(BtlCollChr *chr, BtlCollHit *hit);
s32 BtlColl_TryDodge(s32 objId, BtlCollHit *hit);
s32 BtlColl_TryDeflect(s32 objId, BtlCollHit *hit);
s32 BtlColl_TryReflect(s32 objId, BtlCollHit *hit);
s32 BtlColl_TryAbsorb(s32 objId, BtlCollHit *hit);
s32 BtlColl_TryGuard(s32 objId, BtlCollHit *hit);
s32 BtlColl_Hit(s32 objId, BtlCollHit *hit);
void BtlColl_PlayContactSound(BtlCollHit *hit);
void BtlColl_UpdateGround(BtlCollChr *chr);
void BtlColl_ClearActionBits(BtlCollChr *chr);
void BtlColl_AddActionBit(BtlCollChr *chr, s32 action);
void BtlColl_SetUnk1294(BtlCollChr *chr, s32 v);
void BtlColl_SetFramesToReach(BtlCollChr *chr, f32 scale);
void BtlColl_NextPoolMember(BtlCollChr *chr); /* 0x1CDCA8: first function of src/battle/btl_char_member.c */

#endif
