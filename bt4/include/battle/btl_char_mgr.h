#ifndef BATTLE_BTL_CHAR_MGR_H
#define BATTLE_BTL_CHAR_MGR_H

#include "types.h"

/*
 * Fighter manager (src/battle/btl_char_mgr.c, 0x1C0058..0x1C2FF0): the roster gBtlChars, the per-fighter
 * reset, and the whole-roster phases Battle_Loop / Battle_Update call every frame. The phase order and what
 * each phase runs per fighter is described at the top of btl_char_mgr.c.
 *
 * The structures below are this module's partial views. The fighter object (0x1600 bytes) and the fighter's
 * BtlObj are shared with many modules that are not decompiled: the fields listed are only the ones this file
 * reads or writes. All names are guesses unless the comment says how they are known.
 */

#define BTL_CHR_MEMBER_MAX 5

/* Sequence stage passed to BtlChar_SetStage (stored in the byte at fighter + 0x1084): which phase the fighter is in. */
#define BTL_CHR_STAGE_RESET    1
#define BTL_CHR_STAGE_SAMPLE   2
#define BTL_CHR_STAGE_INPUT    3
#define BTL_CHR_STAGE_6        6
#define BTL_CHR_STAGE_7        7
#define BTL_CHR_STAGE_8        8
#define BTL_CHR_STAGE_9        9
#define BTL_CHR_STAGE_10       10
#define BTL_CHR_STAGE_EVENTS   11
#define BTL_CHR_STAGE_END      12

/* BtlCharMgr.flags */
#define BTL_CHARS_STARTED 1 /* set the first frame the sequence is in Ready (2) or Fight (3) */

/* Gauges of one team member: member entry + 0x40 (BtlMember_GetActiveGauge returns this for the current member). */
typedef struct BtlMgrGauge {
    /* 0x00 */ s32 health;     /* 10000 = one bar (BtlChar_PostScene compares with 10000) */
    /* 0x04 */ s32 healthMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;         /* set to kiMax by technique bit 4 and ability 0x43 */
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ s32 blast;      /* clamped to 0..blastMax; set to blastMax by ability 0x61 */
    /* 0x18 */ s32 blastMax;   /* table value * 100000 after a fusion */
    /* 0x1C */ s32 unk1C;      /* 30000 together with held flag 6, else 0 */
    /* 0x20 */ s32 unk20;      /* non-zero: BtlObj + 0xA40 bit 30 set; copied from fighter + 0x12E0 on a change */
    /* 0x24 */ s32 lowHealth;  /* 1 while health < 10000 */
    /* 0x28 */ s32 lowHealthIdle; /* lowHealth and nothing forbids it (see BtlChar_PostScene) */
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;      /* BtlChar_TestMemberUnk70() = (unk30 != 0); set to 1 with flag 0xA6 */
    /* 0x34 */ s32 unk34;      /* 1 after a fusion */
} BtlMgrGauge;

/* One team member inside the fighter: fighter + 0x9A4 + n * 0xA4 (BtlMember_Get). */
typedef struct BtlMgrMember {
    /* 0x00 */ s32 chara;      /* BattleMember.chara */
    /* 0x04 */ s32 costume;    /* BattleMember.costume */
    /* 0x08 */ s32 present;    /* 1 at reset; 0 for the partner consumed by a fusion */
    /* 0x0C */ s32 bonus[7];   /* BattleMember.bonus[1..7]; summed and clamped to -20..40 by a fusion */
    /* 0x28 */ s32 ability[4]; /* BattleMember.ability; OR-ed by a fusion */
    /* 0x38 */ s32 cpuLevel;   /* BattleMember.cpuLevel */
    /* 0x3C */ s32 aiType;     /* BattleMember.aiType */
    /* 0x40 */ BtlMgrGauge gauge;
    /* 0x78 */ u8 unk78[0xA4 - 0x78];
} BtlMgrMember; /* 0xA4 */

/* The pose block at fighter + 0x10 (BtlChar_GetPos); copied whole to fighter + 0x100 at the start of a frame. */
typedef struct BtlMgrPose {
    /* 0x00 */ u64 unk0[6];
    /* 0x30 */ u64 unk30[2];   /* Vec4, zeroed or filled by BtlChar_GetSnapDelta at the end of the frame */
    /* 0x40 */ u64 unk40[2];   /* Vec4, zeroed or filled by BtlChar_GetMoveSince */
    /* 0x50 */ u64 unk50[10];
    /* 0xA0 */ s32 unkA0;      /* cleared each frame */
    /* 0xA4 */ s32 unkA4;      /* cleared each frame */
    /* 0xA8 */ u64 unkA8[9];
} BtlMgrPose; /* 0xF0 */

/* Nine bytes at fighter + 0x1262, copied to + 0x126B and cleared at the start of a frame. */
typedef struct BtlMgrBytes9 {
    s8 b[9];
} BtlMgrBytes9;

/* The fighter's display / collision object (BtlObj_Get(objId)): only the fields used here. */
typedef struct BtlMgrObj {
    /* 0x0000 */ u8 unk0[0xA4];
    /* 0x00A4 */ u32 *file[3];   /* three loaded data files; word 3 of each is the byte offset of a table */
    /* 0x00B0 */ u8 unkB0[0xA40 - 0xB0];
    /* 0x0A40 */ u32 flags;      /* bit 1: drawn in front / highlighted (see BtlChars_UpdateObjFlag2),
                                    bit 30: BtlMgrGauge.unk20 */
    /* 0x0A44 */ u8 unkA44[0x1660 - 0xA44];
    /* 0x1660 */ u8 *unk1660;    /* block whose word at + 0x18028 mirrors BtlChar_TestMemberUnk70() */
} BtlMgrObj;

/* Partial view of a fighter (0x1600 bytes, BtlChar_Get). */
typedef struct BtlMgrChr {
    /* 0x0000 */ s32 side;       /* roster index = battle side; BtlInputChr.player */
    /* 0x0004 */ s32 pad;        /* BattleSide.pad */
    /* 0x0008 */ s32 index;      /* roster index again */
    /* 0x000C */ s32 objId;      /* BattleSide.objId: BtlObj_Get() argument */
    /* 0x0010 */ BtlMgrPose pose;
    /* 0x0100 */ BtlMgrPose prevPose;
    /* 0x01F0 */ u8 unk1F0[0x2E0 - 0x1F0]; /* 0x10..0x2E0 is cleared when the character is swapped */
    /* 0x02E0 */ u8 unk2E0[0x49C - 0x2E0];
    /* 0x049C */ s32 optA;       /* BattleOption.optA[side] */
    /* 0x04A0 */ u8 unk4A0[0x4B8 - 0x4A0];
    /* 0x04B8 */ s32 optBOff;    /* BattleOption.optB[side] == 0 */
    /* 0x04BC */ u8 unk4BC[0x948 - 0x4BC]; /* 0x570: BtlCharInput (battle/btl_input.h) */
    /* 0x0948 */ s32 action;     /* current action id (BtlAct_GetCurrent) */
    /* 0x094C */ s32 unk94C;
    /* 0x0950 */ s32 prevAction; /* action of the frame before */
    /* 0x0954 */ u8 unk954[0x964 - 0x954];
    /* 0x0964 */ s32 unk964;
    /* 0x0968 */ s32 prev964;
    /* 0x096C */ u8 unk96C[0x974 - 0x96C];
    /* 0x0974 */ s32 unk974;     /* index into the table gBtlChars->tbl[0] (BtlAnim_GetId / BtlAnim_GetFlags) */
    /* 0x0978 */ s32 unk978;
    /* 0x097C */ s32 prev974;
    /* 0x0980 */ s32 unk980;     /* -1 when the object is bound */
    /* 0x0984 */ u8 unk984[0x994 - 0x984];
    /* 0x0994 */ s32 curMember;  /* index of the member that is fighting (BtlMember_GetActiveIndex) */
    /* 0x0998 */ s32 memberCount; /* BattleSide.memberCount */
    /* 0x099C */ s32 unk99C;     /* 0, or 100000 when the current member has ability 0x1B */
    /* 0x09A0 */ s32 unk9A0;
    /* 0x09A4 */ BtlMgrMember members[BTL_CHR_MEMBER_MAX];
    /* 0x0CD8 */ s32 nextMember; /* member to switch to (actions 0xF3..0xF8) */
    /* 0x0CDC */ u8 unkCDC[0xCF4 - 0xCDC];
    /* 0x0CF4 */ s32 unkCF4;     /* BattleSide.unk200 */
    /* 0x0CF8 */ u8 unkCF8[0xD40 - 0xCF8];
    /* 0x0D40 */ s32 unkD40;     /* maximum kept in BattleResult.unk1C[opponent] */
    /* 0x0D44 */ s32 unkD44;     /* maximum kept in BattleResult.unk24[opponent] */
    /* 0x0D48 */ s32 unkD48;
    /* 0x0D4C */ s32 unkD4C;     /* cleared each frame */
    /* 0x0D50 */ s32 unkD50;     /* cleared each frame */
    /* 0x0D54 */ s32 unkD54;
    /* 0x0D58 */ f32 unkD58;     /* 0.5 after a round reset */
    /* 0x0D5C */ f32 unkD5C;     /* 500.0 after a round reset */
    /* 0x0D60 */ u8 unkD60[0xD68 - 0xD60];
    /* 0x0D68 */ s32 unkD68;
    /* 0x0D6C */ s32 unkD6C;     /* per-frame level: 1 (or BtlParam_GetUnk70 with flag 6) + ability 1 / 0 bonus */
    /* 0x0D70 */ s32 unkD70;
    /* 0x0D74 */ s32 unkD74;     /* per-frame level: 1 (or BtlParam_GetUnk72 with flag 6) + ability 3 / 2 bonus */
    /* 0x0D78 */ u8 unkD78[0xE00 - 0xD78];
    /* 0x0E00 */ s32 unkE00[5];  /* 0xE00..0xE40 is cleared when the object is bound */
    /* 0x0E14 */ s32 unkE14;     /* a timer: > 0 forbids lowHealthIdle */
    /* 0x0E18 */ s32 unkE18;     /* same */
    /* 0x0E1C */ u8 unkE1C[0xF30 - 0xE1C];
    /* 0x0F30 */ u64 unkF30[2];  /* Vec4 written on the OPPONENT's object by BtlChar_PostScene */
    /* 0x0F40 */ u8 unkF40[0xFB0 - 0xF40];
    /* 0x0FB0 */ s32 unkFB0;     /* 1 after a reset; >= 3 selects flag 0x13A on a character swap */
    /* 0x0FB4 */ u8 unkFB4[0x1262 - 0xFB4];
    /* 0x1262 */ BtlMgrBytes9 unk1262;
    /* 0x126B */ BtlMgrBytes9 prev1262;
    /* 0x1274 */ u8 unk1274[0x1278 - 0x1274];
    /* 0x1278 */ s32 injectOn;   /* BattleSide_IsCpu(side): input comes from the AI (battle/btl_input.h) */
    /* 0x127C */ u8 unk127C[0x12D0 - 0x127C];
    /* 0x12D0 */ s32 newChara;   /* character / costume / gauge flag of the form being changed into */
    /* 0x12D4 */ s32 newCostume;
    /* 0x12D8 */ u8 unk12D8[0x12E0 - 0x12D8];
    /* 0x12E0 */ s32 new20;
    /* 0x12E4 */ u8 unk12E4[0x12F8 - 0x12E4];
    /* 0x12F8 */ s32 fusionMember; /* member index of the fusion partner */
    /* 0x12FC */ s32 unk12FC;
    /* 0x1300 */ s32 unk1300;    /* BattleSide.unk1FC */
    /* 0x1304 */ u8 unk1304[0x1310 - 0x1304];
    /* 0x1310 */ u64 unk1310[2]; /* passed to BtlObjBody_Warp with flag 0x55 */
    /* 0x1320 */ s32 freeze;     /* > 0: every phase skips this fighter (BtlChar_IsFrozen); counts down per frame */
    /* 0x1324 */ s32 freezeNext; /* freeze to start once freezeDelay has run out */
    /* 0x1328 */ s32 freezeDelay;
    /* 0x132C */ s32 unk132C;
    /* 0x1330 */ s32 unk1330;    /* non-zero: BtlPartner_Release releases a loaded resource */
    /* 0x1334 */ u8 unk1334[0x1538 - 0x1334];
    /* 0x1538 */ u64 frameBits;  /* cleared each frame; BtlChar_SetFrameBits ORs bits in */
    /* 0x1540 */ u8 unk1540[0x1590 - 0x1540];
    /* 0x1590 */ s32 unk1590;    /* -1 each frame */
    /* 0x1594 */ s32 unk1594;    /* -1 each frame (BtlInputChr.unk1594) */
    /* 0x1598 */ u8 unk1598[0x15D0 - 0x1598];
    /* 0x15D0 */ s32 padFlagA;   /* (SaveData.flags & (2 << pad)) != 0 */
    /* 0x15D4 */ s32 unk15D4[5]; /* cleared on a character swap */
    /* 0x15E8 */ u32 *objTbl[3]; /* table inside each of the object's three files */
    /* 0x15F4 */ u8 unk15F4[0x1600 - 0x15F4];
} BtlMgrChr; /* 0x1600 */

/* The roster: gBtlChars -> 0x280 bytes. */
typedef struct BtlCharMgr {
    /* 0x000 */ s32 count;       /* number of fighters (Battle_Init passes 2) */
    /* 0x004 */ BtlMgrChr *chars; /* count * 0x1600 */
    /* 0x008 */ void *unk8;      /* count * 0x34 bytes: per side, four 0xC-byte slots (BtlCharSnd_GetSet) */
    /* 0x00C */ void *unkC;      /* count * 0x34 bytes: per side, four sound handle slots (BtlCharSnd_GetLoopSet) */
    /* 0x010 */ s32 flags;       /* BTL_CHARS_* */
    /* 0x014 */ s32 frame;       /* frames simulated since the start, 30 bits */
    /* 0x018 */ s32 unk18;
    /* 0x01C */ s32 unk1C;       /* counts down to 0 on every simulated frame */
    /* 0x020 */ u32 *tbl[6];     /* tables inside common file 2: header words 3, 5, 6, 7, 8, 9 */
    /* 0x038 */ u8 unk38[8];
    /* 0x040 */ u8 unk40[0x60];  /* cleared by a reset */
    /* 0x0A0 */ u8 unkA0[0x90];  /* cleared by a reset; four 0x20-byte records at +0xB0, count at +0x120 */
    /* 0x130 */ u64 unk130;      /* cleared by a reset */
    /* 0x138 */ u8 unk138[0x140]; /* cleared by a reset; +0x274 is BtlChars_IsTimeStopped() */
    /* 0x278 */ u8 unk278[8];
} BtlCharMgr; /* 0x280 */

extern BtlCharMgr *gBtlChars;

void BtlChar_BindObject(BtlMgrChr *chr);
void BtlChar_CopyMemberBonus(BtlMgrChr *chr, s32 idx);
void BtlChar_CopyAllBonus(BtlMgrChr *chr);
void BtlChar_CopyMemberAbility(BtlMgrChr *chr, s32 idx);
void BtlChar_CopyAllAbility(BtlMgrChr *chr);
void BtlChar_Reset(BtlMgrChr *chr);
void BtlChar_OnModelLoaded(BtlMgrChr *chr);
void BtlChar_OnStageLoaded(BtlMgrChr *chr);
void BtlChar_ResetRound(BtlMgrChr *chr);
void BtlChar_ResetObjWork(BtlMgrChr *chr);
void BtlChars_UpdateFreeze(void);
void BtlChars_UpdateObjFlag2(void);
void BtlChars_ClearObjFlag2(void);
void BtlChars_UpdateCollision(void);
void BtlChar_RaiseEvents(BtlMgrChr *chr);
void BtlChars_CheckRoundReset(void);
void BtlChar_ApplyBonusRequests(BtlMgrChr *chr);
void BtlChar_ApplyAbilityRequests(BtlMgrChr *chr);
void BtlChar_SampleInput(BtlMgrChr *chr);
void BtlChar_BeginFrame(BtlMgrChr *chr);
void BtlChar_UpdateSeqFlags(BtlMgrChr *chr);
void BtlChar_UpdateMotion(BtlMgrChr *chr);
void BtlChar_UpdateStage6(BtlMgrChr *chr);
void BtlChar_UpdateStage7(BtlMgrChr *chr);
void BtlChar_UpdateStage8(BtlMgrChr *chr);
void BtlChar_UpdateStage9(BtlMgrChr *chr);
void BtlChar_UpdateCamera(BtlMgrChr *chr);
void BtlChar_UpdateStage10(BtlMgrChr *chr);
void BtlChar_PostScene(BtlMgrChr *chr);
void BtlChar_UpdateLate(BtlMgrChr *chr);
void BtlChar_EndFrame(BtlMgrChr *chr);
void BtlChar_AllocAll(s32 count);
void BtlChar_FreeAll(void);
void BtlChar_ResetAll(void);
void BtlChars_OnModelLoaded(s32 side);
void BtlChars_OnStageLoaded(void);
void BtlChars_SampleInput(void);
s32 BtlChars_CheckStart(void);
void BtlChars_UpdateInput(void);
void BtlChars_UpdateMain(void);
void BtlChars_PostScene(void);
void BtlChars_EndFrame(void);

#endif
