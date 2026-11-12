#ifndef BATTLE_BTL_CAPI_B_H
#define BATTLE_BTL_CAPI_B_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Fighter interface, second part, 0x208430..0x20BA80 (src/battle/btl_capi_b.c). It continues btl_char_api.c:
 *   0x208430..0x209EE8  accessors keyed by battle object id (BtlChar_FindByObjId), nearly all for the AI
 *   0x209EE8..0x20B200  control of a fighter keyed by player index (BtlChar_Get): the sequence's poses and the
 *                       battle event script's commands (through btl_facade.c)
 *   0x20B200..0x20B418  the character-change request as the loader sees it, the partner attach
 *   0x20B418            key-config lookup for the HUD
 *   0x20B4A8..0x20BA80  accessors keyed by side (BtlChar_FindBySide) for the HUD and the AI, and by player index
 *                       for the sequence
 * Every function is safe on an id that is not a fighter and then returns 0 / -1 / does nothing.
 *
 * The structs are this module's partial views (no shared fighter header yet): only fields this file touches, or
 * that its direct callees show, are named.
 */

/* Gauge block of a team member (BtlMember_GetGauge(chr, n) / BtlMember_GetActiveGauge(chr)): member entry + 0x40. */
typedef struct BtlCapiBGauge {
    /* 0x00 */ s32 hp;        /* 10000 per bar; < 1 = down */
    /* 0x04 */ s32 hpMax;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 ki;        /* 20000 per bar */
    /* 0x10 */ s32 kiMax;
    /* 0x14 */ s32 blast;     /* blast stock, 100000 per stock */
    /* 0x18 */ s32 blastMax;
    /* 0x1C */ s32 maxPower;  /* 0..30000, the timer of the powered-up mode (fighter flag 6) */
} BtlCapiBGauge;

/* Team member entry (BtlMember_GetActive): 0xA4 bytes at fighter +0x9A4. */
typedef struct BtlCapiBMember {
    /* 0x00 */ u8 unk0[0x38];
    /* 0x38 */ s32 cpuLevel;
    /* 0x3C */ s32 aiType;
    /* 0x40 */ BtlCapiBGauge gauge;
} BtlCapiBMember;

/* Pose block (fighter +0x10, BtlChar_GetPos). */
typedef struct BtlCapiBPose {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 rot;
    /* 0x20 */ u8 unk20[0xD0 - 0x20];
    /* 0xD0 */ u8 unkD0_0 : 7;
    /* 0xD0 */ u8 unkD0_7 : 1; /* bit 0x80: read by BtlCharApi_TestPoseBit80 */
} BtlCapiBPose;

/* Fighter (0x1600 bytes). */
typedef struct BtlCapiBChr {
    /* 0x0000 */ s32 player;
    /* 0x0004 */ s32 pad;
    /* 0x0008 */ s32 side;
    /* 0x000C */ s32 objId;
    /* 0x0010 */ BtlCapiBPose pose;
    /* 0x00E4 */ u8 unkE4[0x578 - 0xE4];
    /* 0x0578 */ u32 keyMask[8];    /* pad button of bits 0..3 of the button word (btl_input.h: the mask table) */
    /* 0x0598 */ u8 unk598[0x998 - 0x598];
    /* 0x0998 */ s32 memberCount;
    /* 0x099C */ s32 switchGauge;   /* 0..100000; full = a member switch is allowed */
    /* 0x09A0 */ u8 unk9A0[0xD40 - 0x9A0];
    /* 0x0D40 */ s32 comboDamage;   /* health this fighter lost in the combo it is receiving (BtlMemberCombo) */
    /* 0x0D44 */ s32 comboHits;     /* hits of that combo */
    /* 0x0D48 */ s32 comboTimer;    /* countdown; > 0 = the combo display is on */
    /* 0x0D4C */ s32 comboNewHit;   /* 1 on the frame a counted hit landed */
    /* 0x0D50 */ u8 unkD50[0xD70 - 0xD50];
    /* 0x0D70 */ s32 unkD70;
    /* 0x0D74 */ s32 unkD74;
    /* 0x0D78 */ u8 unkD78[0xDE0 - 0xD78];
    /* 0x0DE0 */ s32 blastShots;    /* compared with the character's limit of class-0 blasts */
    /* 0x0DE4 */ u8 unkDE4[0xE00 - 0xDE4];
    /* 0x0E00 */ s32 dodges;        /* automatic evasions left */
    /* 0x0E04 */ s32 dodgesB;
    /* 0x0E08 */ s32 unkE08;
    /* 0x0E0C */ s32 dodgeKind;     /* 0 / 1 */
    /* 0x0E10 */ s32 unkE10;
    /* 0x0E14 */ s32 unkE14;        /* timer; > 0 adds one armour level */
    /* 0x0E18 */ u8 unkE18[0xE38 - 0xE18];
    /* 0x0E38 */ s32 slotOn[2];     /* 1 while move slot n's effect is active */
    /* 0x0E40 */ s32 unkE40;        /* countdown, cleared by BtlCtrl_UseTechnique */
    /* 0x0E44 */ f32 unkE44;
    /* 0x0E48 */ s32 unkE48;
    /* 0x0E4C */ s32 clashCountA;
    /* 0x0E50 */ s32 clashCountB;
    /* 0x0E54 */ u8 unkE54[0xFB0 - 0xE54];
    /* 0x0FB0 */ s32 reactId;       /* pending hit reaction; >= 3 = cannot act */
    /* 0x0FB4 */ u8 unkFB4[0xFE0 - 0xFB4];
    /* 0x0FE0 */ s32 stunTimer;     /* countdown; > 0 = not free */
    /* 0x0FE4 */ u8 unkFE4[0x106C - 0xFE4];
    /* 0x106C */ s32 unk106C;
    /* 0x1070 */ s32 unk1070;
    /* 0x1074 */ u8 unk1074[0x1290 - 0x1074];
    /* 0x1290 */ s32 unk1290;
    /* 0x1294 */ s32 unk1294;       /* >= 0: overrides the technique progress the AI sees */
    /* 0x1298 */ u8 unk1298[0x1550 - 0x1298];
    /* 0x1550 */ s32 motion;        /* animation of action 4 (scripted motion) */
    /* 0x1554 */ f32 motionBlend;   /* its blend time */
    /* 0x1558 */ s32 motionLoop;    /* 0 play once, 1 loop */
    /* 0x155C */ s32 unk155C;
    /* 0x1560 */ Vec4 warpPos;      /* taken when held flag 0xFC is seen */
    /* 0x1570 */ Vec4 warpRot;      /* taken when held flag 0xFD is seen */
    /* 0x1580 */ u8 unk1580[0x1594 - 0x1580];
    /* 0x1594 */ s32 switchPrompt;  /* >= 2 while a switch prompt is open; -1 each frame */
    /* 0x1598 */ u8 unk1598[0x1600 - 0x1598];
} BtlCapiBChr; /* size 0x1600 */

/* Character parameter block (battle object +0x91C); read here only through BtlCharApi_GetParamByte2 / Flags /
   Unk14 / Unk18. The rest is what the callees of this file read (0x20DF80..0x20F3C8). */
typedef struct BtlCapiBParam {
    /* 0x00 */ u16 unk0;            /* bit 0x80: BtlCharApi_HasParamBit80 */
    /* 0x02 */ s8 unk2;
    /* 0x03 */ u8 unk3[0x10 - 0x3];
    /* 0x10 */ s32 flags;           /* bits 4 / 8: armour level of BtlCharApi_GetArmorBreakLevel */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ u8 unk1C[0x80 - 0x1C];
    /* 0x80 */ s16 blastLimit;      /* class-0 blasts alive at once (BtlParam_GetCount80) */
    /* 0x82 */ s16 blastLimitB;     /* class-1 (BtlParam_GetUnk82) */
    /* 0x84 */ u8 unk84[0x8F - 0x84];
    /* 0x8F */ u8 unk8F[9];         /* BtlParam_GetUnk8F(chr, n) */
    /* 0x98 */ u8 transformId[4];   /* 0xFF = none (BtlParam_GetSlotId) */
    /* 0x9C */ u8 transformCost[4]; /* blast stocks (BtlParam_GetSlotCost) */
    /* 0xA0 */ u8 unkA0[0xAC - 0xA0];
    /* 0xAC */ s8 transformSlot;    /* the slot the AI uses, -1 none (BtlParam_GetUnkAC) */
    /* 0xAD */ u8 unkAD;
    /* 0xAE */ u8 fusionCost[3];    /* blast stocks (BtlParam_GetCostAE) */
    /* 0xB1 */ u8 unkB1[3];
    /* 0xB4 */ u8 fusionId[3];      /* (BtlParam_GetUnkB4) */
} BtlCapiBParam;

/* Technique table (battle object +0x92C, BtlCharApi_GetSkillTable). Arrays are indexed by technique class
   (BtlAct_GetCurrentClass: 0 / 1 = the two move slots, 2..4 = the three skill slots); the AI's view
   (AiThChrSkills) indexes the same arrays by skill slot = class - 2. */
typedef struct BtlCapiBSkills {
    /* 0x000 */ u32 state[3];       /* by skill slot */
    /* 0x00C */ u32 flags[3];       /* by skill slot (BtlSuper_GetFlags(chr, cls)): 1, 2, 4 tested here */
    /* 0x018 */ u8 unk18[0x13C - 0x18];
    /* 0x13C */ s8 unk13C[5];       /* by class (BtlSuper_GetType) */
    /* 0x141 */ u8 unk141[0x163 - 0x141];
    /* 0x163 */ s8 kind[5];         /* by class (BtlSuper_GetUnk165) */
    /* 0x168 */ u8 unk168[0x194 - 0x168];
    /* 0x194 */ s32 cost[5];        /* by class: ki cost (BtlSuper_GetKiCost) */
    /* 0x1A8 */ u8 unk1A8[0x227 - 0x1A8];
    /* 0x227 */ s8 unk227[5];       /* BtlSuper_GetPromptRowIndex(chr, n): index into the roster table +0x34 */
} BtlCapiBSkills;

/* Move table (battle object +0x930, BtlCharApi_GetMoveTable): the two move slots. */
typedef struct BtlCapiBMoves {
    /* 0x00 */ u8 unk0[0x8];
    /* 0x08 */ u32 flags[2];        /* BtlSkill_GetFlags: bit 1 tested by BtlCtrl_UseTechnique, 0x100 by IsMoveFlag100 */
    /* 0x10 */ s16 id[2];
    /* 0x14 */ u8 unk14[0x94 - 0x14];
    /* 0x94 */ s8 stock[2];         /* blast stocks the move costs (BtlSkill_GetBlastCost) */
    /* 0x96 */ u8 unk96[0x9E - 0x96];
    /* 0x9E */ s8 kind[2];          /* BtlSkill_GetUnk9E */
} BtlCapiBMoves;

/* Battle object of a fighter (BtlChar_GetObj). */
typedef struct BtlCapiBObj {
    /* 0x000 */ u8 unk0[0x91C];
    /* 0x91C */ BtlCapiBParam *param;
    /* 0x920 */ u8 unk920[0x92C - 0x920];
    /* 0x92C */ BtlCapiBSkills *skills;
    /* 0x930 */ BtlCapiBMoves *moves;
    /* 0x934 */ u8 unk934[0xCAC - 0x934];
    /* 0xCAC */ s8 unkCAC;
    /* 0xCAD */ s8 unkCAD;          /* expected to be ~unkCAC */
} BtlCapiBObj;

/* Blast list (EftHit_GetList): 64 records of 0x190 bytes and a count. */
typedef struct BtlCapiBBlastDef {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s16 ownerId;
    /* 0x12 */ u8 unk12[0x18 - 0x12];
    /* 0x18 */ s16 kind;
} BtlCapiBBlastDef;

typedef struct BtlCapiBBlastSrc {
    /* 0x0 */ s32 ownerId;
} BtlCapiBBlastSrc;

typedef struct BtlCapiBBlastRec {
    /* 0x000 */ s32 objId;          /* fighter that fired it */
    /* 0x004 */ u8 unk4[0xC - 0x4];
    /* 0x00C */ s32 type;           /* 0: described by def, 1: by src */
    /* 0x010 */ u8 unk10[0x20 - 0x10];
    /* 0x020 */ u64 pos[2];         /* a Vec4; copied by value as two doublewords */
    /* 0x030 */ Vec4 prevPos;
    /* 0x040 */ u8 unk40[0x54 - 0x40];
    /* 0x054 */ s32 unk54;          /* 1: ignored */
    /* 0x058 */ u8 unk58[0x64 - 0x58];
    /* 0x064 */ BtlCapiBBlastSrc *src;
    /* 0x068 */ BtlCapiBBlastDef *def;
    /* 0x06C */ u8 unk6C[0x180 - 0x6C];
    /* 0x180 */ s32 seen;           /* set by BtlCharApi_MarkIncomingBlast: the AI has reacted to it */
    /* 0x184 */ u8 unk184[0x190 - 0x184];
} BtlCapiBBlastRec; /* size 0x190 */

typedef struct BtlCapiBBlastList {
    /* 0x0000 */ BtlCapiBBlastRec rec[64];
    /* 0x6400 */ s32 count;
} BtlCapiBBlastList;

/* Character-change request (roster +0x258 points at the one being served; btl_char_ctl.h has the queue). */
typedef struct BtlCapiBChange {
    /* 0x00 */ s32 player;
    /* 0x04 */ s32 type;            /* 0 / 1 */
    /* 0x08 */ s32 arg[7];          /* the words BtlChange_GetArgs copies out */
} BtlCapiBChange;

typedef struct BtlCapiBChangeQueue {
    /* 0x000 */ u8 unk0[0x120];
    /* 0x120 */ BtlCapiBChange *cur; /* roster +0x258 */
    /* 0x124 */ u8 unk124[0x12C - 0x124];
    /* 0x12C */ s32 state;           /* roster +0x264: 1 = waiting for the loader, 4 = ready */
} BtlCapiBChangeQueue;

/* One entry of the roster table +0x34 (0x14 bytes), indexed by BtlCapiBSkills.unk227[]. */
typedef struct BtlCapiBPrompt {
    /* 0x0 */ u8 unk0[2];
    /* 0x2 */ s8 button;            /* 0..3: which button the prompt wants */
    /* 0x3 */ u8 unk3[0x14 - 0x3];
} BtlCapiBPrompt;

/* Fighter roster (gBtlChars). */
typedef struct BtlCapiBMgr {
    /* 0x000 */ u8 unk0[0x34];
    /* 0x034 */ BtlCapiBPrompt *prompts;
    /* 0x038 */ u8 unk38[0x138 - 0x38];
    /* 0x138 */ BtlCapiBChangeQueue change;
} BtlCapiBMgr;

/* Battle work: only the flag word. */
typedef struct BtlCapiBWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags;         /* 0x100 paused, 0x2000 loading */
} BtlCapiBWork;

/* --- by object id --- */
s32 BtlCharApi_IsUnkCACPaired(s32 objId);
s32 BtlCharApi_GetAction(s32 objId);
s32 BtlCharApi_GetBlastShots(s32 objId);
s32 BtlCharApi_GetBlastRoom(s32 objId);
s32 BtlCharApi_HasBlastLimit(s32 objId);
s32 BtlCharApi_GetBlastRoomB(s32 objId);
s32 BtlCharApi_TestFlag66(s32 objId);
s32 BtlCharApi_SkipBlastRec(s32 objId, BtlCapiBBlastRec *rec, s32 mode);
s32 BtlCharApi_FindIncomingBlast(s32 objId, s32 mode);
s32 BtlCharApi_IsBlastPassing(s32 objId);
void BtlCharApi_MarkIncomingBlast(s32 objId);
f32 BtlCharApi_GetTechniqueProgress(s32 objId);
BtlCapiBSkills *BtlCharApi_GetSkillTable(s32 objId);
BtlCapiBMoves *BtlCharApi_GetMoveTable(s32 objId);
s32 BtlCharApi_GetAttackAttr(s32 objId);
s32 BtlCharApi_GetHp(s32 objId);
s32 BtlCharApi_GetHpMax(s32 objId);
s32 BtlCharApi_GetKi(s32 objId);
s32 BtlCharApi_GetBlast(s32 objId);
s32 BtlCharApi_GetBlastMax(s32 objId);
s32 BtlCharApi_GetMaxPower(s32 objId);
s32 BtlCharApi_GetSwitchGauge(s32 objId);
s32 BtlCharApi_IsBlockedByOpponent(s32 objId);
s32 BtlCharApi_HasParamBit80(s32 objId);
s32 BtlCharApi_CanTransform(s32 objId);
s32 BtlCharApi_CanFuse(s32 objId);
s32 BtlCharApi_CanSwitch(s32 objId);
s32 BtlCharApi_GetCpuLevel(s32 objId);
s32 BtlCharApi_GetAiType(s32 objId);
s32 BtlCharApi_IsUnk1070Set(s32 objId);
s32 BtlCharApi_GetTransformCost(s32 objId);
s32 BtlCharApi_PickFusionSlot(s32 objId);
s32 BtlCharApi_PickTransformSlot(s32 objId);
s32 BtlCharApi_IsChangingForm(s32 objId);
s32 BtlCharApi_GetParamUnk14(s32 objId);
s32 BtlCharApi_GetParamFlags(s32 objId);
s32 BtlCharApi_GetParamUnk18(s32 objId);
s32 BtlCharApi_GetMemberCount(s32 objId);
s32 BtlCharApi_GetActiveMember(s32 objId);
s32 BtlCharApi_GetSwitchTarget(s32 objId);
s32 BtlCharApi_GetMemberHpPercent(s32 objId, s32 member);
s32 BtlCharApi_GetMemberKiPercent(s32 objId, s32 member);
s32 BtlCharApi_GetOppSkillKind(s32 objId);
s32 BtlCharApi_IsOppSkillFlag4(s32 objId);
s32 BtlCharApi_TestOppSkillFlags(s32 objId);
s32 BtlCharApi_GetOppSkillClass(s32 objId);
s32 BtlCharApi_GetOppMoveKind(s32 objId);
s32 BtlCharApi_GetClashCountB(s32 objId);
s32 BtlCharApi_GetClashCountA(s32 objId);
s32 BtlCharApi_IsMoveSlotActive(s32 objId, u32 slot);
s32 BtlCharApi_IsMoveFlag100(s32 objId, u32 slot);
s32 BtlCharApi_GetStunTimer(s32 objId);
s32 BtlCharApi_GetPromptButtons(s32 objId);
s32 BtlCharApi_GetUnk1290(s32 objId);
s32 BtlCharApi_IsUnk106CLow(s32 objId);
s32 BtlCharApi_GetArmorBreakLevel(s32 objId);
s32 BtlCharApi_GetParamByte2(s32 objId);
f32 BtlCharApi_GetUnkE44B(s32 objId);
s32 BtlCharApi_GetUnkD74Diff(s32 objId);
s32 BtlCharApi_IsAnimFlag2800(s32 objId);
s32 BtlCharApi_TestPoseBit80(s32 objId, s32 always);
s32 BtlCharApi_TestFlag98(s32 objId);
s32 BtlCharApi_TestFlagBE(s32 objId);
s32 BtlCharApi_GetUnkE40(s32 objId);
s32 BtlCharApi_GetParamByte8F(s32 objId, s32 n);

/* --- by player index: sequence poses --- */
void BtlCtrl_StartEntrance(s32 player);
void BtlCtrl_EndEntrance(s32 player);
void BtlCtrl_StartWinPose(s32 player);
void BtlCtrl_StartLosePose(s32 player);
s32 BtlCtrl_IsPoseReached(s32 player);

/* --- by player index: the battle event script (BtlCtrl_* names already in btl_scene.txt are kept) --- */
void BtlCtrl_PlayMotion(s32 player, s32 motion, s32 loop, f32 blend);
void BtlCtrl_SetPos(s32 player, Vec4 *pos);
void BtlCtrl_GetPos(s32 player, Vec4 *out);
void BtlCtrl_SetRot(s32 player, Vec4 *rot);
void BtlCtrl_GetRot(s32 player, Vec4 *out);
s32 BtlCtrl_GetActiveMember(s32 player);
s32 BtlCtrl_IsAction4(s32 player);
s32 BtlCtrl_IsMotionPlaying(s32 player);
void BtlCtrl_StopMotion(s32 player);
void BtlCtrl_SetAuraOn(s32 player);
void BtlCtrl_SetAuraOff(s32 player);
s32 BtlCtrl_SetChargeFx(s32 player, s32 on);  /* no result: see the source */
void BtlCtrl_BurstChargeFx(s32 player);
void BtlCtrl_ClearHidden(s32 player);
void BtlCtrl_SetHidden(s32 player);
void BtlCtrl_ReloadMemberBonus(s32 player, u32 member);
void BtlCtrl_ReloadMemberAbilities(s32 player, u32 member);
void BtlCtrl_SetFlag10D(s32 player);
void BtlCtrl_ClearFlag10D(s32 player);
s32 BtlCtrl_AddHp(s32 player, s32 member, f32 percent);  /* no result: see the source */
void BtlCtrl_RaiseHp(s32 player, s32 member, f32 percent);
void BtlCtrl_LowerHp(s32 player, s32 member, f32 percent);
s32 BtlCtrl_AddKi(s32 player, s32 member, s32 bars);  /* no result: see the source */
void BtlCtrl_RaiseKi(s32 player, s32 member, s32 bars);
void BtlCtrl_LowerKi(s32 player, s32 member, s32 bars);
s32 BtlCtrl_AddBlast(s32 player, s32 member, s32 stocks);  /* no result: see the source */
void BtlCtrl_RaiseBlast(s32 player, s32 member, s32 stocks);
void BtlCtrl_LowerBlast(s32 player, s32 member, s32 stocks);
s32 BtlCtrl_SetMaxPower(s32 player, s32 on);  /* no result: see the source */
s32 BtlCtrl_Transform(s32 player, s32 id);
s32 BtlCtrl_Fuse(s32 player, s32 id);
s32 BtlCtrl_ChangeMember(s32 player, s32 member);
s32 BtlCtrl_ForceFlag11x(s32 player, s32 kind);
s32 BtlCtrl_UseTechnique(s32 player, s32 kind);
s32 BtlCtrl_ForceReaction(s32 player, u32 kind);
s32 BtlCtrl_IsInterruptible(s32 player);
s32 BtlCtrl_CanAct(s32 player);

/* --- the character-change request, for the loader --- */
s32 BtlChange_IsPendingType0(s32 player);
s32 BtlChange_IsPendingType1(s32 player);
void BtlChange_GetArgs(s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 *f, s32 *g);
void BtlChange_NotifyTaken(void);
void BtlChange_NotifyLoaded(void);
s32 BtlChange_IsReady(void);
void BtlCtrl_AttachPartner(s32 player, s32 slot, s32 objId);
s32 BtlCharApi_GetButtonIcon(BtlCapiBChr *chr, u32 bit);

/* --- by side --- */
s32 BtlSide_GetHp(s32 side);
s32 BtlSide_GetKi(s32 side);
s32 BtlSide_GetBlast(s32 side);
s32 BtlSide_GetBlastMax(s32 side);
s32 BtlSide_GetMaxPower(s32 side);
s32 BtlSide_GetSwitchGauge(s32 side);
s32 BtlSide_GetActiveMember(s32 side);
s32 BtlSide_GetSwitchTarget(s32 side);
s32 BtlSide_CountAlive(s32 side);
s32 BtlSide_GetSwitchTargetHp(s32 side);
s32 BtlSide_GetSwitchTargetHpMax(s32 side);
s32 BtlSide_GetParamUnk2C(s32 side);
s32 BtlSide_IsPoweredUp(s32 side);
s32 BtlSide_TestFlagBE(s32 side);
s32 BtlCtrl_IsActiveDead(s32 player);
s32 BtlCtrl_IsTeamDead(s32 player);
s32 BtlCtrl_TestFlag7(s32 player);
s32 BtlSide_GetComboHits(s32 side);
s32 BtlSide_GetComboDamage(s32 side);
s32 BtlSide_IsComboShown(s32 side);
s32 BtlSide_IsComboHitNew(s32 side);

#endif
