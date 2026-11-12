#ifndef BATTLE_BTL_CHAR_ACTION_H
#define BATTLE_BTL_CHAR_ACTION_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter action state machine, core part (src/battle/btl_char_action.c, 0x1E0290..0x1E3158).
 * The structures are partial views local to this module: only the fields this file touches.
 *
 * An action is a number 0..0x13B. gBtlActHandlers[id] is `s32 handler(chr, phase)` (NULL for 0):
 *   phase 0  enter      called once, on the frame the action becomes current
 *   phase 1  run        called every frame (also on the enter frame, after phase 0)
 *   phase 2  decide     called every frame before the forced-action check; requests the next action
 *   phase 3  leave      called once on the old action, when a request is accepted
 * The handlers are int functions that fall off their end (they are not compiled as void: their last
 * call is not a tail call), and the dispatcher ignores the value. A handler ends its action by
 * calling BtlAct_Request(chr, next); BtlAct_Update makes the switch on the next frame's dispatch.
 */

#define BTLACT_COUNT 0x13C

#define BTLACT_PHASE_ENTER  0
#define BTLACT_PHASE_RUN    1
#define BTLACT_PHASE_DECIDE 2
#define BTLACT_PHASE_LEAVE  3

struct BtlActChr;
typedef s32 (*BtlActHandler)(struct BtlActChr *chr, s32 phase);

/* Pose block (chr + 0x10, what BtlChar_GetPos returns). */
typedef struct BtlActPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;       /* y = yaw */
    /* 0x20 */ u8 unk20[0x44 - 0x20];
    /* 0x44 */ f32 unk44;      /* divides the height in BtlAct_GetFramesToGround: a fall speed */
    /* 0x48 */ u8 unk48[0x70 - 0x48];
    /* 0x70 */ Vec4 unk70;     /* zeroed by action 4 */
    /* 0x80 */ Vec4 vel;       /* direction of travel (x, z used with atan2f) */
    /* 0x90 */ f32 speed;
    /* 0x94 */ f32 facing;     /* yaw the fighter faces */
    /* 0x98 */ f32 unk98;
    /* 0x9C */ f32 unk9C;
    /* 0xA0 */ u8 unkA0[0xAC - 0xA0];
    /* 0xAC */ f32 unkAC;      /* forced to 1.0 except in actions 0xF, 0x3E, 0x58, 0xB0, 0xB1 (kept >= 1.0) */
    /* 0xB0 */ f32 unkB0;
    /* 0xB4 */ f32 groundY;    /* height of the ground below (inferred) */
} BtlActPose;

/* Attack parameters built from an attack table record (0x24 bytes; pending at chr + 0xD1C, latched at + 0xCF8). */
typedef struct BtlActAttack {
    /* 0x00 */ s32 flags;      /* record +0x14 (u16) */
    /* 0x04 */ s16 motion;     /* record +0x06 */
    /* 0x06 */ s16 motion2;    /* motion + 2 when the record has 2 or more parts, else -1 */
    /* 0x08 */ s8 parts;       /* max(record +0x04, 1) */
    /* 0x09 */ u8 unk9;        /* record +0x01 */
    /* 0x0A */ s8 cut;         /* camera cut passed to ChrCam_RequestCut(chr, 1, cut); < 0 = none */
    /* 0x0B */ s8 follow[2];   /* attack record indices: record +0x0D.. or +0x0F.. */
    /* 0x0D */ u8 padD[3];
    /* 0x10 */ f32 unk10;      /* record +0x0C * 0.1, or 1.0 */
    /* 0x14 */ f32 unk14;      /* record +0x08 */
    /* 0x18 */ f32 unk18;      /* 0 from BtlAct_PrepareAttack, 15.0 from BtlAct_ResetAttack */
    /* 0x1C */ f32 unk1C;      /* record +0x00 * 10 / 1080 */
    /* 0x20 */ s32 unk20;
} BtlActAttack; /* size 0x24 */

/* Attack table record (roster + 0x2C, index = action id - 0x70). */
typedef struct BtlActAttackRec {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 unk1;
    /* 0x02 */ u8 unk2[2];
    /* 0x04 */ s8 parts;
    /* 0x05 */ s8 kind;        /* 0..8, switched on by the queue builder BtlDecide_QueueAttack */
    /* 0x06 */ u16 motion;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ s8 unkC;
    /* 0x0D */ s8 follow[2];   /* attack record indices (follow-ups, inferred) */
    /* 0x0F */ s8 followAlt[2]; /* used instead in the powered-up mode (flag 6) when that record is available */
    /* 0x11 */ s8 cut;         /* camera cut when the camera sits on the negative side (or by frame parity) */
    /* 0x12 */ s8 cutAlt;      /* camera cut for the other side; < 0 = always use cut */
    /* 0x13 */ s8 needSkill;   /* bit number tested by BtlAct_TestAttackSkill; < 0 = always available */
    /* 0x14 */ u16 flags;      /* 0x20 = pick the cut from the camera side instead of the frame parity */
    /* 0x16 */ u8 unk16[2];
} BtlActAttackRec; /* size 0x18 */

/* Member entry (chr + 0x9A4 + i * 0xA4). */
typedef struct BtlActVitals {
    /* 0x00 */ s32 hp;
    /* 0x04 */ s32 hpMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 gaugeB;
    /* 0x10 */ s32 gaugeBMax;
    /* 0x14 */ u8 unk14[0x20 - 0x14];
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
} BtlActVitals;

typedef struct BtlActMember {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 costume;
    /* 0x08 */ s32 present;
    /* 0x0C */ u8 unkC[0x40 - 0xC];
    /* 0x40 */ BtlActVitals vitals;
} BtlActMember;

/* Form-change request (chr + 0x12CC). */
typedef struct BtlActForm {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 chara;
    /* 0x08 */ s32 costume;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} BtlActForm;

/* Action bookkeeping (chr + 0x948). The nesting is needed for BtlAct_SetQueue to match. */
typedef struct BtlActState {
    /* 0x00 */ s32 current;   /* current action id */
    /* 0x04 */ s32 request;   /* action to switch to at the next dispatch, -1 = none */
    /* 0x08 */ s32 prev;      /* read by BtlAct_GetPrev; nothing stores it */
    /* 0x0C */ s32 queue[4];  /* follow-up actions, -1 = empty */
} BtlActState; /* size 0x1C */

/* Fighter (0x1600 bytes). */
typedef struct BtlActChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x3D0 - 0x10];
    /* 0x03D0 */ s32 work[0x14];    /* per-action scratch, zeroed when an action starts */
    /* 0x0420 */ u8 unk420[0x4A0 - 0x420];
    /* 0x04A0 */ f32 camYaw;        /* ChrCam yaw: movement reference */
    /* 0x04A4 */ f32 unk4A4;
    /* 0x04A8 */ f32 camSide;       /* ChrCam side: sign = which side of the fighter the camera sits */
    /* 0x04AC */ u8 unk4AC[0x948 - 0x4AC];
    /* 0x0948 */ BtlActState act;
    /* 0x0964 */ s32 actionFrame;   /* frames spent in the current action */
    /* 0x0968 */ s32 unk968;
    /* 0x096C */ f32 pitch;         /* pitch toward the opponent, radians (BtlAct_SetPitchMotion) */
    /* 0x0970 */ s32 holdAction;    /* set by a handler's run phase: skip next frame's forced-action check */
    /* 0x0974 */ s32 motion;        /* motion id, index of the roster's motion flag table */
    /* 0x0978 */ u8 unk978[0x994 - 0x978];
    /* 0x0994 */ s32 active;        /* active member */
    /* 0x0998 */ s32 memberCount;
    /* 0x099C */ s32 unk99C;        /* gauge, 0..100000 */
    /* 0x09A0 */ s32 unk9A0;
    /* 0x09A4 */ u8 members[0xCD8 - 0x9A4];
    /* 0x0CD8 */ s32 switchMember;  /* member being switched to */
    /* 0x0CDC */ s32 switchChara;
    /* 0x0CE0 */ s32 switchCostume;
    /* 0x0CE4 */ s32 switchUnk20;
    /* 0x0CE8 */ s32 unkCE8;
    /* 0x0CEC */ s32 unkCEC;
    /* 0x0CF0 */ s32 unkCF0;
    /* 0x0CF4 */ s32 unkCF4;
    /* 0x0CF8 */ BtlActAttack attack;     /* latched when the attack action starts */
    /* 0x0D1C */ BtlActAttack nextAttack; /* prepared when the attack is queued */
    /* 0x0D40 */ s32 unkD40;
    /* 0x0D44 */ s32 unkD44;
    /* 0x0D48 */ s32 unkD48;        /* countdown */
    /* 0x0D4C */ s32 unkD4C;
    /* 0x0D50 */ u8 unkD50[0xD60 - 0xD50];
    /* 0x0D60 */ s32 unkD60;
    /* 0x0D64 */ s32 unkD64;
    /* 0x0D68 */ s32 unkD68;
    /* 0x0D6C */ s32 unkD6C;
    /* 0x0D70 */ s32 unkD70;
    /* 0x0D74 */ s32 unkD74;
    /* 0x0D78 */ s32 unkD78;
    /* 0x0D7C */ s32 unkD7C;
    /* 0x0D80 */ s32 unkD80;        /* gauge that drains 400 per frame down to a floor */
    /* 0x0D84 */ s32 unkD84;
    /* 0x0D88 */ s32 unkD88;        /* frames with flag 8 in actions 0x89..0x8C */
    /* 0x0D8C */ s32 unkD8C;
    /* 0x0D90 */ s32 attackId;      /* id BtlAct_PrepareAttack was last called with */
    /* 0x0D94 */ u8 unkD94[0xDE0 - 0xD94];
    /* 0x0DE0 */ s32 unkDE0;
    /* 0x0DE4 */ s32 unkDE4;        /* countdown */
    /* 0x0DE8 */ s32 unkDE8;
    /* 0x0DEC */ s32 unkDEC;
    /* 0x0DF0 */ u8 unkDF0[0xE10 - 0xDF0];
    /* 0x0E10 */ s32 unkE10;
    /* 0x0E14 */ u8 unkE14[0xE20 - 0xE14];
    /* 0x0E20 */ s32 unkE20;
    /* 0x0E24 */ u8 unkE24[0xE40 - 0xE24];
    /* 0x0E40 */ s32 unkE40;        /* countdown */
    /* 0x0E44 */ u8 unkE44[0xE50 - 0xE44];
    /* 0x0E50 */ s32 unkE50;        /* counter */
    /* 0x0E54 */ u8 unkE54[0xF30 - 0xE54];
    /* 0x0F30 */ Vec4 unkF30;       /* added to the position under flag 0x8B */
    /* 0x0F40 */ u8 unkF40[0xF48 - 0xF40];
    /* 0x0F48 */ s32 unkF48;
    /* 0x0F4C */ u8 unkF4C[0xFB0 - 0xF4C];
    /* 0x0FB0 */ s32 unkFB0;
    /* 0x0FB4 */ u8 unkFB4[0xFBC - 0xFB4];
    /* 0x0FBC */ s32 unkFBC;
    /* 0x0FC0 */ u8 unkFC0[0xFD0 - 0xFC0];
    /* 0x0FD0 */ f32 unkFD0;
    /* 0x0FD4 */ f32 unkFD4;
    /* 0x0FD8 */ u8 unkFD8[0xFE0 - 0xFD8];
    /* 0x0FE0 */ s32 unkFE0;        /* countdown shortened by mashing (bit 20); > 0 = not free */
    /* 0x0FE4 */ s32 unkFE4;        /* countdown: flag 0x96 */
    /* 0x0FE8 */ s32 unkFE8;        /* countdown */
    /* 0x0FEC */ s32 unkFEC;
    /* 0x0FF0 */ s32 unkFF0;        /* countdown, cleared without flag 0x13 */
    /* 0x0FF4 */ s32 unkFF4;
    /* 0x0FF8 */ s32 unkFF8;        /* countdown: flags 0x93, 0x137, 0x96 */
    /* 0x0FFC */ s32 unkFFC;        /* follows unkFF8 * 2 by 15 per frame */
    /* 0x1000 */ s32 unk1000;
    /* 0x1004 */ u8 unk1004[0x32];
    /* 0x1036 */ s8 unk1036[0x32];  /* counters saturating at 100 */
    /* 0x1068 */ u8 unk1068[0x12CC - 0x1068];
    /* 0x12CC */ BtlActForm form;
    /* 0x12FC */ u8 unk12FC[0x132C - 0x12FC];
    /* 0x132C */ s32 secTimer;      /* 0..29: one "second" tick every 30 frames */
    /* 0x1330 */ u8 unk1330[0x1550 - 0x1330];
    /* 0x1550 */ s32 unk1550;       /* motion of action 4 */
    /* 0x1554 */ f32 unk1554;       /* its blend time */
    /* 0x1558 */ s32 unk1558;       /* its end mode */
    /* 0x155C */ u8 unk155C[0x1580 - 0x155C];
    /* 0x1580 */ s32 unk1580;       /* mode 1 only: frames of flag 0x11E (input off) */
    /* 0x1584 */ u8 unk1584[0x1590 - 0x1584];
    /* 0x1590 */ s32 unk1590;       /* last direction held: -1 (bit 16), 2 (bit 18), 3 (bit 19), 4 (bit 17) */
    /* 0x1594 */ u8 unk1594[0x1600 - 0x1594];
} BtlActChr; /* size 0x1600 */

/* Fighter roster (gBtlChars). */
typedef struct BtlActRoster {
    /* 0x00 */ s32 count;
    /* 0x04 */ BtlActChr *chars;
    /* 0x08 */ u8 unk8[0x20 - 0x8];
    /* 0x20 */ u32 *motionFlags;         /* 0x19E words, by motion id */
    /* 0x24 */ void *cutTbl;
    /* 0x28 */ void *voiceTbl;
    /* 0x2C */ BtlActAttackRec *attackTbl; /* by action id - 0x70 */
} BtlActRoster;

void BtlAct_Request(BtlActChr *chr, s32 id);
void BtlAct_SetQueue(BtlActChr *chr, u32 slot, s32 id);
void BtlAct_ClearQueue(BtlActChr *chr);
s32 BtlAct_HasQueued(BtlActChr *chr);
s32 BtlAct_GetCurrent(BtlActChr *chr);
s32 BtlAct_GetRequested(BtlActChr *chr);
s32 BtlAct_GetPrev(BtlActChr *chr);
s32 BtlAct_GetQueued(BtlActChr *chr);
void BtlAct_PopQueue(BtlActChr *chr);
s32 BtlAct_IsTechniqueId(s32 id);
s32 BtlAct_GetIdClass(BtlActChr *chr, s32 id);
s32 BtlAct_GetCurrentClass(BtlActChr *chr);
s32 BtlAct_GetPrevClass(BtlActChr *chr);
s32 BtlAct_GetMotionLevel(BtlActChr *chr, s32 motion);
f32 BtlAct_GetGroundY(BtlActChr *chr);
f32 BtlAct_GetHeight(BtlActChr *chr);
f32 BtlAct_GetHeightRatio(BtlActChr *chr);
f32 BtlAct_GetFramesToGround(BtlActChr *chr);
f32 BtlAct_GetRotYRelCam(BtlActChr *chr);
f32 BtlAct_GetFacingRelCam(BtlActChr *chr);
f32 BtlAct_GetVelDirRelCam(BtlActChr *chr);
f32 BtlAct_ScaleSpeedByApproach(BtlActChr *chr, f32 speed, f32 range);
f32 BtlAct_ScaleSpeedByDist(BtlActChr *chr, f32 speed, f32 range, f32 frames);
void BtlAct_SetPitchMotion(BtlActChr *chr, s32 motionUp, s32 motionDown, s32 recalc);
s32 BtlAct_IsAirMotion(BtlActChr *chr, s32 useSaved);
void BtlAct_PrepareSwitch(BtlActChr *chr);
void BtlAct_PushAngle(BtlActChr *chr, f32 angle, f32 speed, f32 arg);
void BtlAct_PushDir(BtlActChr *chr, Vec4 *dir, f32 speed, f32 arg);
void BtlAct_SetObjUnk(BtlActChr *chr, f32 a, f32 b);
s32 BtlAct_IsAttackId(s32 id);
s32 BtlAct_TestAttackSkill(BtlActChr *chr, s32 attack);
void BtlAct_PrepareAttack(BtlActChr *chr, s32 id);
void BtlAct_ResetAttack(BtlActChr *chr);
void BtlAct_LatchAttack(BtlActChr *chr);
s32 BtlAct_TestPoweredSkill(BtlActChr *chr, u32 mask);
void BtlAct_SetFormCurrent(BtlActChr *chr);
void BtlAct_SetFormRandom(BtlActChr *chr);
void BtlAct_CountAndMarkOpponent(BtlActChr *chr);
void BtlAct_EndFlag98(BtlActChr *chr);
void BtlAct_ApplyFlag94(BtlActChr *chr);
void BtlAct_CheckForced(BtlActChr *chr);
void BtlAct_UpdateGauges(BtlActChr *chr);
void BtlAct_UpdateTimers(BtlActChr *chr);
void BtlAct_Update(BtlActChr *chr);
s32 BtlAct_Action01(BtlActChr *chr, s32 phase);
s32 BtlAct_Action02(BtlActChr *chr, s32 phase);
s32 BtlAct_Action03(BtlActChr *chr, s32 phase);
s32 BtlAct_Action04(BtlActChr *chr, s32 phase);
s32 BtlAct_Action05(BtlActChr *chr, s32 phase);

#endif
