#ifndef BATTLE_BTL_CHAR_GET_H
#define BATTLE_BTL_CHAR_GET_H

#include "types.h"
#include "sys/math3d.h"
#include "sys/pad.h"

/*
 * Small battle helpers and fighter accessors (src/battle/btl_char_get.c, 0x1DBF20..0x1DC9A0).
 * The structures are partial views local to this module: only the fields this file touches.
 */

#define BTL_VOICE_KINDS 57

/* Pad vibration request of a fighter (chr + 0x15D0). */
typedef struct BtlCharVib {
    /* 0x00 */ s32 enabled;   /* set at fighter reset from gSaveData->flags & (2 << chr->pad) */
    /* 0x04 */ f32 power;     /* 0..1, big motor */
    /* 0x08 */ s32 time;      /* frames left for the big motor */
    /* 0x0C */ s32 smallTime; /* frames left for the small motor */
    /* 0x10 */ s32 phase;     /* frames the big motor has run (modulates the power) */
    /* 0x14 */ s32 toggle;    /* small motor is on every other frame */
} BtlCharVib; /* size 0x18 */

/* Voice cooldowns of a fighter (chr + 0x1370). */
typedef struct BtlCharVoice {
    /* 0x00 */ s32 last[BTL_VOICE_KINDS];  /* line played last for each kind */
    /* 0xE4 */ s32 timer[BTL_VOICE_KINDS]; /* frames until the kind may play again */
} BtlCharVoice; /* size 0x1C8 */

/* Entry of the voice table (roster + 0x28). */
typedef struct BtlVoiceEntry {
    /* 0x0 */ s16 first;    /* first line */
    /* 0x2 */ s16 count;    /* number of lines */
    /* 0x4 */ f32 interval; /* cooldown in seconds */
} BtlVoiceEntry; /* size 0x8 */

/* What BtlMember_GetActiveGauge returns: the active member's block + 0x40. */
typedef struct BtlCharGetVitals {
    /* 0x00 */ s32 hp;
    /* 0x04 */ u8 unk4[0x30 - 0x4];
    /* 0x30 */ s32 unk30;
} BtlCharGetVitals;

/* Fighter (0x1600 bytes). */
typedef struct BtlCharGetChr {
    /* 0x0000 */ s32 player;  /* roster index; sound channel base, voice handle, replay track */
    /* 0x0004 */ s32 pad;     /* index into gPad */
    /* 0x0008 */ s32 side;    /* roster index again (BtlChar_FindBySide) */
    /* 0x000C */ s32 objId;   /* BtlObj_Get index */
    /* 0x0010 */ Vec4 pos;
    /* 0x0020 */ u8 unk20[0xFE0 - 0x20];
    /* 0x0FE0 */ s32 unkFE0;  /* > 0: not free */
    /* 0x0FE4 */ u8 unkFE4[0x1278 - 0xFE4];
    /* 0x1278 */ s32 injectOn; /* CPU-controlled: no vibration */
    /* 0x127C */ u8 unk127C[0x1320 - 0x127C];
    /* 0x1320 */ s32 freeze;  /* > 0: BtlChar_IsFrozen */
    /* 0x1324 */ u8 unk1324[0x1370 - 0x1324];
    /* 0x1370 */ BtlCharVoice voice;
    /* 0x1538 */ u8 unk1538[0x15D0 - 0x1538];
    /* 0x15D0 */ BtlCharVib vib;
    /* 0x15E8 */ u8 unk15E8[0x1600 - 0x15E8];
} BtlCharGetChr; /* size 0x1600 */

/* Fighter roster (gBtlChars, 0x280 bytes). */
typedef struct BtlCharGetRoster {
    /* 0x00 */ s32 count;
    /* 0x04 */ BtlCharGetChr *chars;
    /* 0x08 */ u8 unk8[0x14 - 0x8];
    /* 0x14 */ s32 frame;       /* frames simulated since BtlChar_ResetAll, 30 bits */
    /* 0x18 */ u32 randState;   /* BtlChar_Rand; 0 at BtlChar_ResetAll */
    /* 0x1C */ s32 stageTimer;  /* BtlChar_AddStageTimer; counts down to 0 */
    /* 0x20 */ u8 unk20[0x28 - 0x20];
    /* 0x28 */ BtlVoiceEntry *voiceTbl;
} BtlCharGetRoster;

f32 BtlUtil_WrapAngle(f32 a);
void BtlUtil_WrapAngles(Vec4 *dst, Vec4 *src);
f32 BtlUtil_LengthXZ(Vec4 *v);
f32 BtlUtil_ApproachF(f32 cur, f32 target, f32 step);
s32 BtlUtil_Approach(s32 cur, s32 target, s32 step);
f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
f32 BtlUtil_MinF(f32 a, f32 b);
f32 BtlUtil_MaxF(f32 a, f32 b);
s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);
s32 BtlUtil_Min(s32 a, s32 b);
s32 BtlUtil_Max(s32 a, s32 b);
s32 BtlUtil_RoundUp(s32 n, s32 unit);
s32 BtlUtil_AngleToSector(f32 a);
s32 BtlChar_GetCount(void);
BtlCharGetChr *BtlChar_Get(s32 i);
BtlCharGetChr *BtlChar_FindByObjId(s32 objId);
BtlCharGetChr *BtlChar_FindBySide(s32 side);
void *BtlChar_GetObj(BtlCharGetChr *chr);
Vec4 *BtlChar_GetPos(BtlCharGetChr *chr);
Pad *BtlChar_GetPad(BtlCharGetChr *chr);
s32 BtlChar_IsFrozen(BtlCharGetChr *chr);
s32 BtlChar_IsFree(BtlCharGetChr *chr);
s32 BtlChar_IsDead(BtlCharGetChr *chr);
s32 BtlChar_TestMemberUnk70(BtlCharGetChr *chr);
s32 BtlChar_GetFrame(void);
s32 BtlChar_FrameMod(s32 n);
s32 BtlChar_Rand(void);
f32 BtlChar_RandF(void);
s32 BtlChar_IsStage4Or27(void);
void BtlChar_AddStageTimer(f32 seconds);
void BtlChar_SetVibration(BtlCharGetChr *chr, f32 power, f32 seconds);
void BtlChar_SetSmallVibration(BtlCharGetChr *chr, f32 seconds);
void BtlChar_Vibrate(BtlCharGetChr *chr, f32 power, f32 seconds);
void BtlChar_UpdateVibration(BtlCharGetChr *chr);
BtlVoiceEntry *BtlChar_GetVoiceEntry(s32 kind);
void BtlChar_PlayVoice(BtlCharGetChr *chr, s32 kind);
void BtlChar_StopVoiceOnFlag(BtlCharGetChr *chr);
void BtlChar_TickVoiceTimers(BtlCharGetChr *chr);
void BtlChar_PlayVoiceSingle(BtlCharGetChr *chr, s32 kind);

#endif
