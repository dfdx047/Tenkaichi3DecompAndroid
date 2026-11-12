#ifndef BATTLE_BTL_CHAR_API_H
#define BATTLE_BTL_CHAR_API_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter interface, 0x207020..0x208430 (src/battle/btl_char_api.c): what the rest of the battle code (camera, effect
 * scene, effect modules, HUD, sound, AI) calls to read or poke a fighter without knowing its layout. Arguments named
 * `objId` are battle object ids (BtlObj_Get index; a fighter's own id is at fighter +0xC); every function is safe on
 * an id that is not a fighter and then returns 0 / 0.0f / does nothing.
 *
 * The structs below are this module's partial views (rule: no shared fighter header yet). Only the fields this file
 * touches, or that its direct callees show, are named.
 */

/* Active member's vitals, the block BtlMember_GetActiveGauge(chr) returns: chr + 0x9A4 + chr->member * 0xA4 + 0x40. */
typedef struct BtlCharApiVitals {
    /* 0x00 */ s32 hp;     /* < 1 = down (BtlChar_IsDead) */
    /* 0x04 */ s32 hpMax;
    /* 0x08 */ u8 unk8[0x20 - 0x8];
    /* 0x20 */ s32 unk20;  /* member +0x60 */
    /* 0x24 */ u8 unk24[0x30 - 0x24];
    /* 0x30 */ s32 unk30;  /* member +0x70; non-zero makes the win / lose talk use character 0x56 (btl_seq.c) */
} BtlCharApiVitals;

/* Fighter (0x1600 bytes, BtlChar_Get / BtlChar_FindByObjId). */
typedef struct BtlCharApiChr {
    /* 0x0000 */ s32 side;          /* 0 / 1; compared with BtlCam_GetDefaultView() */
    /* 0x0004 */ s32 pad;           /* pad index (btl_input.h) */
    /* 0x0008 */ u8 unk8[0xC - 0x8];
    /* 0x000C */ s32 objId;         /* BtlObj_Get index of this fighter's object */
    /* 0x0010 */ Vec4 pos;          /* world position (BtlChar_GetPos returns its address) */
    /* 0x0020 */ u8 unk20[0x420 - 0x20];
    /* 0x0420 */ Vec4 camUnk420;    /* point the camera-shake distance is measured from */
    /* 0x0430 */ Vec4 camPos;       /* fighter camera position */
    /* 0x0440 */ Vec4 camRot;       /* fighter camera rotation */
    /* 0x0450 */ u8 unk450[0x460 - 0x450];
    /* 0x0460 */ Vec4 camUnk460;    /* end point of the demo camera's stage trace */
    /* 0x0470 */ u8 camShake[0x494 - 0x470]; /* CamShake_Add target (ChrCam_AddShake) */
    /* 0x0494 */ s32 camUnk494;     /* returned by BtlCharApi_GetCamPose */
    /* 0x0498 */ u8 unk498[0x4A0 - 0x498];
    /* 0x04A0 */ f32 camUnk4A0;
    /* 0x04A4 */ u8 unk4A4[0x4B8 - 0x4A4];
    /* 0x04B8 */ s32 camShakeOn;    /* ChrCam_AddShake only shakes when non-zero */
    /* 0x04BC */ u8 unk4BC[0x948 - 0x4BC];
    /* 0x0948 */ s32 action;        /* action id (BtlAct_GetCurrent) */
    /* 0x094C */ u8 unk94C[0x974 - 0x94C];
    /* 0x0974 */ s32 unk974;        /* index into the manager's table at +0x20 (BtlAnim_GetId) */
    /* 0x0978 */ u8 unk978[0x990 - 0x978];
    /* 0x0990 */ s32 unk990;        /* > 0 makes BtlCharApi_ObjTestAttr answer 0 (BtlAnim_TestAttr) */
    /* 0x0994 */ s32 member;        /* active member index (BtlMember_GetActive) */
    /* 0x0998 */ u8 unk998[0xE44 - 0x998]; /* member blocks of 0xA4 bytes start at 0x9A4 */
    /* 0x0E44 */ f32 unkE44;
    /* 0x0E48 */ u8 unkE48[0xE5C - 0xE48];
    /* 0x0E5C */ s32 unkE5C;        /* counter, full at 3 */
    /* 0x0E60 */ s32 unkE60;        /* counter, full at 5 */
    /* 0x0E64 */ u8 unkE64[0xEFC - 0xE64];
    /* 0x0EFC */ f32 unkEFC[4];     /* length not known; summed by BtlCharApi_GetChargedUnkC78 */
    /* 0x0F0C */ u8 unkF0C[0x1278 - 0xF0C];
    /* 0x1278 */ s32 injectOn;      /* input comes from the three fields below (btl_input.h) */
    /* 0x127C */ u32 injectButtons;
    /* 0x1280 */ f32 injectStickX;
    /* 0x1284 */ f32 injectStickY;
    /* 0x1288 */ u64 unk1288;
    /* 0x1290 */ u8 unk1290[0x1320 - 0x1290];
    /* 0x1320 */ s32 unk1320;       /* > 0: no camera shake from BtlCharApi_ShakeCamsNear (BtlChar_IsFrozen) */
    /* 0x1324 */ u8 unk1324[0x15D0 - 0x1324];
    /* 0x15D0 */ s32 rumbleOn;      /* pad vibration block, written by BtlChar_SetVibration / BtlChar_SetSmallVibration, run by BtlChar_UpdateVibration */
    /* 0x15D4 */ f32 rumblePower;   /* 0..1 */
    /* 0x15D8 */ s32 rumbleFrames;  /* time * 30 */
    /* 0x15DC */ s32 rumbleFrames2; /* second motor, time * 30 */
    /* 0x15E0 */ u8 unk15E0[0x1600 - 0x15E0];
} BtlCharApiChr; /* size 0x1600 */

/* Battle object (BtlObj_Get): only the fields read here. */
typedef struct BtlCharApiObj {
    /* 0x000 */ u8 unk0[0xA40];
    /* 0xA40 */ union {
        u64 flags64;                /* the tests of two bits read all 64 bits */
        s32 flags;                  /* the single-bit accessors read and write the low word */
    } u;
    /* 0xA48 */ u8 unkA48[0xC78 - 0xA48];
    /* 0xC78 */ f32 unkC78;
    /* 0xC7C */ u8 unkC7C[0xC80 - 0xC7C];
    /* 0xC80 */ f32 unkC80;
    /* 0xC84 */ u8 unkC84[0xCAC - 0xC84];
    /* 0xCAC */ s8 unkCAC;
    /* 0xCAD */ s8 unkCAD;          /* expected to be ~unkCAC */
    /* 0xCAE */ s8 attrCount;       /* number of attribute words below (BtlObjAnim_TestEvent) */
} BtlCharApiObj;

/* One playing sound of a side (0xC bytes). BtlCharSnd_StoreHandle fills a slot, BtlCharSnd_StopUnrequestedLoops stops it with Snd_StopHandle. */
typedef struct BtlCharApiSound {
    /* 0x0 */ s32 handle;           /* sound handle, < 0 = free slot */
    /* 0x4 */ s32 unk4;
    /* 0x8 */ u8 unk8;              /* index into the table at 0x2EF290 (BtlCharSnd_GetBankMask) */
} BtlCharApiSound;

typedef struct BtlCharApiSoundSet {
    /* 0x00 */ BtlCharApiSound slot[4];
    /* 0x30 */ s32 next;            /* ring index of the next slot to try (BtlCharSnd_StoreHandle) */
} BtlCharApiSoundSet; /* size 0x34 */

/* Fighter manager (gBtlChars, 0x280 bytes). */
typedef struct BtlCharApiMgr {
    /* 0x00 */ s32 count;
    /* 0x04 */ BtlCharApiChr *chars;
    /* 0x08 */ void *unk8;
    /* 0x0C */ BtlCharApiSoundSet *sounds; /* one per side */
    /* 0x10 */ u8 unk10[0x1C - 0x10];
    /* 0x1C */ s32 unk1C;               /* counter, full at 90 */
    /* 0x20 */ void **unk20;            /* 0x19E pointers, indexed by fighter +0x974 */
    /* 0x24 .. 0x280: not touched here. Seen in callees: +0xA0 four 0x20-byte sound requests {Vec4 pos, f32 near,
       f32 far, s32 id, s8 owner, s8 kind} with their count at +0x120 (BtlCharSnd_Request); +0x130 / +0x134 (BtlReplay_GetViewSide). */
} BtlCharApiMgr;

s32 BtlCharApi_ObjHasFlags22(s32 objId);
s32 BtlCharApi_ObjHasFlags42(s32 objId);
s32 BtlCharApi_AnyHasFlag128(void);
s32 BtlCharApi_IsHpEmpty(s32 objId);
s32 BtlCharApi_TestFlagA4(s32 objId);
s32 BtlCharApi_GetMemberUnk60(s32 objId);
s32 BtlCharApi_IsFlag8Action104(s32 objId);
s32 BtlCharApi_IsAction103OrFlagA6(s32 objId);
s32 BtlCharApi_HasMemberUnk70(s32 objId);
void BtlCharApi_SetHeldFlagA7(s32 objId);
void BtlCharApi_SetHeldFlagA8(s32 objId);
void BtlCharApi_SetHeldFlagA9(s32 objId);
void BtlCharApi_SetHeldFlagAA(s32 objId);
void BtlCharApi_SetHeldFlagAB(s32 objId);
void BtlCharApi_PlaySoundAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far);
s32 BtlCharApi_PlayTechniqueSound(s32 objId, u32 n); /* result undefined, see the source */
s32 BtlCharApi_GetSoundCount(s32 side);
void BtlCharApi_GetSound(s32 side, s32 n, s32 *handle, s32 *out3, s32 *out4);
void BtlCharApi_ObjClearMaskBit3(s32 objId);
void BtlCharApi_ObjSetMaskBit3(s32 objId);
s32 BtlCharApi_ObjTestFlagBit21(s32 objId);
f32 BtlCharApi_GetUnkE44(s32 objId);
f32 BtlCharApi_GetUnkE5CRatio(s32 objId);
f32 BtlCharApi_GetUnkE60Ratio(s32 objId);
void BtlCharApi_ObjSetFlag100(s32 objId, s32 on);
s32 BtlCharApi_ObjHasFlags102(s32 objId);
s32 BtlCharApi_IsTargetBelowHalfHp(s32 objId, s32 targetId);
s32 BtlCharApi_CanTechniqueFinish(s32 objId, s32 targetId);
f32 BtlCharApi_GetMgrTimerRatio(void);
void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
s32 BtlCharApi_ObjTestAttr(s32 objId, u64 mask);
s32 BtlCharApi_ObjGetAttrValue(s32 objId, u64 mask);
s32 BtlCharApi_ObjGetAttrKind(s32 objId, u64 mask);
f32 BtlCharApi_GetChargedUnkC78(s32 objId);
f32 BtlCharApi_ObjGetUnkC78(s32 objId);
f32 BtlCharApi_ObjGetUnkC80(s32 objId);
s32 BtlCharApi_ObjQuery24D610(s32 objId, s32 arg1, s32 arg2);
s32 BtlCharApi_GetUnk974(s32 objId);
void *BtlCharApi_GetUnk974Data(s32 objId);
s32 BtlCharApi_TestFlag2B(s32 objId);
s32 BtlCharApi_GetCamPose(s32 objId, Vec4 *pos, Vec4 *rot);
f32 BtlCharApi_GetCamUnk4A0(s32 objId);
s32 BtlCharApi_HasCamPriority(s32 objId);
void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 arg3, f32 arg4);
s32 BtlCharApi_IsCamShown(s32 objId);
void BtlCharApi_GetCamUnk460(s32 objId, Vec4 *out);
s32 BtlCharApi_GetMgrUnk134(void);
s32 BtlCharApi_AnyCamPriority(void);
s32 BtlCharApi_IsInputInjected(s32 objId);
void BtlCharApi_SetInjectedInput(s32 objId, u32 buttons, f32 stickX, f32 stickY);
s32 BtlCharApi_TestFlag0F(s32 objId);
s32 BtlCharApi_TestFlag05(s32 objId);
s32 BtlCharApi_GetParamByte84(s32 objId, u32 n);
s32 BtlCharApi_GetParamByte8A(s32 objId);
s32 BtlCharApi_GetParamByte8D(s32 objId);
u64 BtlCharApi_GetUnk1288(s32 objId);
s32 BtlCharApi_CheckUnkCAC(s32 objId);
s32 BtlCharApi_TestFlag60(s32 objId);

#endif
