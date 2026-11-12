#ifndef BATTLE_BTL_CHAR_FX_H
#define BATTLE_BTL_CHAR_FX_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter effect requests, animation events and the partner object
 * (src/battle/btl_char_fx.c, btl_char_fx_b.c, btl_char_fx_c.c: 0x1D00D8..0x1D3B40).
 * The structures are partial views local to this module: only the fields these files touch.
 */

/* The pose block at fighter + 0x10 (BtlChar_GetPos returns it). Offsets in [] are fighter offsets. */
typedef struct FxPose {
    /* 0x00 [0x010] */ Vec4 pos;      /* world position */
    /* 0x10 [0x020] */ Vec4 rot;      /* rotation; .y is the yaw the effects are aimed along */
    /* 0x20 [0x030] */ u8 unk20[0x40 - 0x20];
    /* 0x40 [0x050] */ Vec4 vel;      /* velocity: its length is compared with speed limits, .y scales the splash */
    /* 0x50 [0x060] */ u8 unk50[0x80 - 0x50];
    /* 0x80 [0x090] */ Vec4 unk80;    /* a vector every "flash" effect gets as its second vector */
    /* 0x90 [0x0A0] */ f32 unk90;
    /* 0x94 [0x0A4] */ f32 yaw;       /* facing */
    /* 0x98 [0x0A8] */ f32 unk98;     /* reference speed: |vel| / unk98 > 0.5 starts the fast-move effect */
    /* 0x9C [0x0AC] */ u8 unk9C[0xB0 - 0x9C];
    /* 0xB0 [0x0C0] */ Vec4 unkB0;    /* ground point under the fighter; .y is compared with the water level */
    /* 0xC0 [0x0D0] */ Vec4 unkC0;    /* passed with unkB0 to the ground dust (the ground normal: inferred) */
    /* 0xD0 [0x0E0] */ u32 unkD0;     /* state bits; any of 0x30000065 cancels the ground effects */
} FxPose;

/* What BtlMember_GetActiveGauge returns: the active member's gauge block. */
typedef struct FxGauge {
    /* 0x00 */ s32 health;
    /* 0x04 */ s32 healthMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;
    /* 0x10 */ s32 kiMax;
} FxGauge;

/* An action record of a battle object (obj + 0x91C points at the current one): only the attribute word. */
typedef struct FxObjAction {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ u32 attr;   /* 0x8000 and 0x200000 are tested here */
} FxObjAction;

/* Header of the animation an object is playing. */
typedef struct FxAnimHead {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;    /* 0: the partner is hidden while this animation plays */
} FxAnimHead;

/* Animation playback of a battle object: obj + 0xB40. */
typedef struct FxObjAnim {
    /* 0x000 [0xB40] */ FxAnimHead *head;
    /* 0x004 [0xB44] */ f32 end;       /* last frame */
    /* 0x008 [0xB48] */ u8 unk8[0x130 - 0x8];
    /* 0x130 [0xC70] */ s32 unk130;    /* set to 1 when the partner starts an animation */
    /* 0x134 [0xC74] */ s32 unk134;
    /* 0x138 [0xC78] */ f32 frame;
    /* 0x13C [0xC7C] */ f32 prevFrame;
    /* 0x140 [0xC80] */ f32 step;
} FxObjAnim;

/* Bits of FxObj.flags as this module uses them. */
#define FX_OBJ_DRAWN        0x2        /* required for flag 0x80 and for the impact flash; cleared to hide the partner */
#define FX_OBJ_FLAG10       0x10
#define FX_OBJ_FLAG20       0x20       /* set / cleared by animation events 0x20 / 0x40 */
#define FX_OBJ_FLAG40       0x40       /* set / cleared by animation event bits 43 / 44 */
#define FX_OBJ_FLAG80       0x80
#define FX_OBJ_BYTE2        0xFF0000   /* request 3 writes 8 when it starts and 1 when it ends */
#define FX_OBJ_FULL_AURA    0x40000    /* forces the aura level to 1 */
#define FX_OBJ_OCCLUDED     0x2000000  /* the stage is between this fighter and its camera */

/* A battle object (BtlChar_GetObj / BtlObj_Get): only the fields used here. */
typedef struct FxObj {
    /* 0x000 */ u8 unk0[0xC];
    /* 0x00C */ s32 chara;       /* character number (voice lines are chara * 100 + line) */
    /* 0x010 */ s32 id;          /* object id */
    /* 0x014 */ u8 unk14[0x91C - 0x14];
    /* 0x91C */ FxObjAction *action;
    /* 0x920 */ u8 unk920[0x950 - 0x920];
    /* 0x950 */ Vec4 pos;
    /* 0x960 */ Vec4 rot;
    /* 0x970 */ u8 unk970[0x9A0 - 0x970];
    /* 0x9A0 */ u8 mtx[0x40];    /* matrix the model part offsets of event 8 are multiplied by */
    /* 0x9E0 */ u8 unk9E0[0xA24 - 0x9E0];
    /* 0xA24 */ s32 unkA24;      /* copied into the damage spark argument */
    /* 0xA28 */ u8 unkA28[0xA40 - 0xA28];
    /* 0xA40 */ u32 flags;       /* FX_OBJ_ bits. The original tests "0x2 and 0x80" with one 64-bit load, which is
                                    what two adjacent bit-field tests compile to. */
    /* 0xA44 */ u8 unkA44[0xB40 - 0xA44];
    /* 0xB40 */ FxObjAnim anim;
} FxObj;

/* A second character model attached to a fighter: fighter + 0x1330. */
typedef struct FxPartner {
    /* 0x0 */ s32 active;   /* 1 once the object load job has attached it */
    /* 0x4 */ s32 res;      /* resource slot, released with BtlRes_Free */
    /* 0x8 */ s32 objId;    /* battle object, released with BtlObj_Destroy */
} FxPartner;

/* Partial view of a fighter (0x1600 bytes). */
typedef struct FxChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;       /* the view whose screen filter BtlFx_UpdateScreenFilter drives */
    /* 0x000C */ s32 objId;
    /* 0x0010 */ u8 unk10[0x494 - 0x10];
    /* 0x0494 */ s32 camTrace;   /* fighter camera: the stage trace hit something */
    /* 0x0498 */ u8 unk498[0x4A0 - 0x498];
    /* 0x04A0 */ f32 camYaw;     /* fighter camera yaw */
    /* 0x04A4 */ u8 unk4A4[0xDD0 - 0x4A4];
    /* 0x0DD0 */ Vec4 hitPos;    /* where the last hit landed (written by the hit code) */
    /* 0x0DE0 */ u8 unkDE0[0xDF0 - 0xDE0];
    /* 0x0DF0 */ s32 hitKind;    /* effect kind of the hit spark of request 0x17 */
    /* 0x0DF4 */ u8 unkDF4[0x1330 - 0xDF4];
    /* 0x1330 */ FxPartner partner;
    /* 0x133C */ s32 unk133C;
    /* 0x1340 */ f32 filter;     /* screen filter strength 0..1 */
    /* 0x1344 */ s32 unk1344;
    /* 0x1348 */ s32 charaFxOn;  /* BtlFx_UpdateCharaFx: its effect is running */
    /* 0x134C */ s32 trigger1E;  /* frame counter 0..3 of request 0x1E */
    /* 0x1350 */ s32 trigger1F;  /* frame counter 0..3 of request 0x1F */
    /* 0x1354 */ s32 unk1354;    /* passed to the effect of request 0x28 */
    /* 0x1358 */ u8 unk1358[0x1360 - 0x1358];
    /* 0x1360 */ Vec4 unk1360;   /* second vector of the flash of request 0x2E */
} FxChr;

/* Argument of EftImpact_SpawnHit / EftImpact_SpawnHitScaled (the "flash" effects). */
typedef struct FxPosArg {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 pos2;
    /* 0x20 */ s32 kind;
    /* 0x24 */ s32 objId;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} FxPosArg;

/* Argument of the hit sparks of request 0x17. */
typedef struct FxHitArg {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ s32 objId;
    /* 0x14 */ s32 kind;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ f32 scale;
} FxHitArg;

/* Argument of the damage hit sparks (EftKiBlast_Fire and its four variants). */
typedef struct FxHitArg2 {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ u16 objId;
    /* 0x12 */ u16 objId2;
    /* 0x14 */ u16 code;     /* model part code of the animation event */
    /* 0x16 */ u16 unk16;    /* obj->unkA24 */
    /* 0x18 */ s16 level;    /* BtlKiBlast_GetCurrentKind: 0..11, its low two bits pick the size */
    /* 0x1A */ u8 type;      /* BtlKiBlast_GetType: picks the spark module (5, 4, 2, 3, other) */
    /* 0x1B */ u8 unk1B;
    /* 0x1C */ s16 unk1C;
    /* 0x20 */ f32 unk20;
    /* 0x24 */ f32 unk24;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;
    /* 0x30 */ s32 size;     /* 0..3 */
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ u8 unk40;
    /* 0x41 */ u8 unk41;
    /* 0x42 */ u8 unk42[0xE];
} FxHitArg2; /* size 0x50 */

/* Argument of EftBodyFx_Start. */
typedef struct FxArg3 {
    /* 0x0 */ s32 objId;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ f32 scale;
    /* 0xC */ s32 unkC;
} FxArg3;

/* Argument of EftAbsorb_Start / EftAbsorb_StartHands / EftCharaFx_Start. */
typedef struct FxArg2 {
    /* 0x0 */ s32 objId;
    /* 0x4 */ f32 scale;
    /* 0x8 */ s32 unk8[2];
} FxArg2;

/* Argument of EftRushBurst_Start. */
typedef struct FxDirArg {
    /* 0x00 */ Vec4 dir;
    /* 0x10 */ s32 objId;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ f32 scale;
    /* 0x1C */ s32 unk1C;
} FxDirArg;

/* Argument of EftRay_CreateByValue (the speed lines of request 0x15). The original's block is 0x60 bytes. */
typedef struct FxLineArg {
    /* 0x00 */ Vec4 pos;      /* (0, 0, 0, 1) */
    /* 0x10 */ s32 r;         /* colour: r, then g, b, alpha. The callee reads s32 color[4]; the split (a scalar and
                                 an array of three) is what the initialiser's store order needs to match */
    /* 0x14 */ s32 gba[3];
    /* 0x20 */ f32 unk20;     /* 0.3 + 0.5 * |angle to the opponent, relative to the camera yaw| / pi */
    /* 0x24 */ f32 unk24;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;     /* -20 - distance * 0.1, not below -50 */
    /* 0x30 */ f32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 objId;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 unk44;
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ s32 unk50;
} __attribute__((aligned(16))) FxLineArg; /* size 0x60 */

/* The part of the save block read here. */
typedef struct FxSave {
    /* 0x0000 */ u8 unk0[0x1608];
    /* 0x1608 */ s32 flags;   /* bit 0: alternate voices (voice line base 0xCC32 instead of 0x8D4E) */
} FxSave;

/* btl_char_fx.c */
void BtlFx_SpawnFlashReq20(FxChr *chr);
void BtlFx_SpawnFlashReq2B(FxChr *chr);
void BtlFx_SpawnHitSparkReq17(FxChr *chr);
s32 BtlFx_UpdateAura(FxChr *chr);
void BtlFx_UpdateChargeFx(FxChr *chr);
s32 BtlFx_UpdatePowerUpLook(FxChr *chr);
void BtlFx_SpawnReq6(FxChr *chr);
void BtlFx_UpdateScreenFilter(FxChr *chr);
void BtlFx_SpawnBurstReqC(FxChr *chr);
void BtlFx_SpawnFastMoveFx(FxChr *chr);
void BtlFx_SpawnSpeedLines(FxChr *chr);

/* btl_char_fx_b.c */
void BtlFx_SpawnReq18(FxChr *chr);
void BtlFx_UpdateGroundFx(FxChr *chr);
void BtlFx_UpdateWaterFx(FxChr *chr);
void BtlFx_UpdateAimedFxReq28(FxChr *chr);
void BtlFx_UpdateReq1A(FxChr *chr);
void BtlFx_UpdateReq38(FxChr *chr);
void BtlFx_SpawnReq37(FxChr *chr);
void BtlFx_SpawnFlashReq2E(FxChr *chr);
void BtlFx_UpdateCharaFx(FxChr *chr);
void BtlFx_SpawnClashFlash(FxChr *chr);
void BtlFx_TriggerObjReq1D(FxChr *chr);
void BtlFx_TriggerObjReq1E(FxChr *chr);
void BtlFx_TriggerObjReq1F(FxChr *chr);
void BtlFx_UpdateObjFlagReq3(FxChr *chr);
void BtlFx_UpdateOccludedFlag(FxChr *chr);
void BtlFx_UpdateObjCmdFlag137(FxChr *chr);
void BtlFx_UpdateSubStateFlag96(FxChr *chr);
void BtlFx_UpdateObjFlag80(FxChr *chr);
void BtlFx_FireKiBlast(FxChr *chr);

/* btl_char_fx_c.c */
void BtlFx_HandleImpactEvents(FxChr *chr);
void BtlFx_HandleEvent1(FxChr *chr);
void BtlFx_HandleFlag30Events(FxChr *chr);
void BtlFx_PlaySwingSound(FxChr *chr);
void BtlFx_SpawnPartFxEvent8(FxChr *chr);
void BtlFx_SpawnPartFlashes(FxChr *chr);
s32 BtlFx_PushBackOnEvent400(FxChr *chr);
s32 BtlFx_SetObjMaskOnEvent400(FxChr *chr);
void BtlFx_ShakeCamOnEvent(FxChr *chr);
void BtlFx_VibrateOnEvent(FxChr *chr);
void BtlFxObj_HandleFlagEvents(FxObj *obj);
void BtlFxObj_PlayEventSounds(FxObj *obj);
void BtlFxObj_HandleSubStateEvents(FxObj *obj);
void BtlFx_UpdateObjEvents(FxChr *chr);
void BtlFx_UpdateAll(FxChr *chr);
void BtlFx_UpdateAfterHits(FxChr *chr);
void BtlFx_UpdatePostScene(FxChr *chr);
void BtlFx_UpdateLate(FxChr *chr);
void BtlFx_UpdateStageFxRequest(FxChr *chr);
void BtlFx_UpdateStageFx(void);
void BtlFx_UpdateRoster(void);
void BtlFxObj_Place(FxObj *obj, Vec4 *pos, Vec4 *rot);
void BtlPartner_HandleEvents(FxObj *obj, FxChr *chr);
s32 BtlPartner_IsActive(FxChr *chr);
void BtlPartner_Attach(FxChr *chr, s32 res, s32 objId);
void BtlPartner_Release(FxChr *chr);
void BtlPartner_Update(FxChr *chr);
void BtlPartner_UpdateEvents(FxChr *chr);
void BtlPartner_PlayAnim(FxChr *chr, s32 anim);
s32 BtlPartner_StepAnim(FxChr *chr);
void BtlPartner_SetFlag10(FxChr *chr, s32 on);
void BtlPartner_LinkToOwner(FxChr *chr);
FxObj *BtlPartner_GetObj(FxChr *chr);
void BtlPartner_PushAngle(FxChr *chr, f32 yaw, f32 speed, f32 arg);
void BtlPartner_PushDir(FxChr *chr, Vec4 *dir, f32 speed, f32 arg);
void BtlPartner_SetObjFloats(FxChr *chr, f32 a, f32 b);
void BtlPartner_SetFlag80(FxChr *chr, s32 on);

#endif
