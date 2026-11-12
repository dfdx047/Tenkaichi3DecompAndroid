#ifndef BATTLE_BATTLE_SETUP_H
#define BATTLE_BATTLE_SETUP_H

#include "types.h"
#include "battle/battle.h"

/*
 * Battle result finish, battle events, battle setup and replay block:
 * the second half of src/battle/battle_load.c (0x129170..0x12B570; the loader, 0x127120..0x129170, is
 * described in battle/battle_work.h).
 *
 * The battle block itself (BattleSetup, BattleSide, BattleMember, BattleResult, BattleEvents ...) is described
 * in battle/battle.h; only the replay block is declared here. Meanings come from the callers and are marked
 * as inference where they are.
 *
 * --- How a battle is set up -------------------------------------------------------------------------
 * The menu overlay (and two places in the main executable) build the setup with this sequence:
 *     Battle_ClearWork()                       -> BattleSetup_Clear(): memset + magic "btls" + version 7
 *     BattleSetup_SetRule(screenMode, mode, bgm, timeLimit, announcer, stage, unk10)
 *     BattleSetup_SetSide(side, control, pad, memberCount, unk1FC, unk200, lead, charaBits)   for side 0, 1
 *     BattleSetup_SetMember(side, idx, chara, costume, variant, cpuLevel, health, items)       per member
 *     [BattleSetup_SetPoolMember(count, idx, ...)   mode 3 only: the queue of up to 50 opponents]
 *     BattleSetup_Finish()                     -> BattleSetup_FinishEx(0)
 * Then Game_Main leaves the overlay and runs Battle_Main, which reads the setup through the getters.
 * Callers: menu 0x348710 and 0x351508 (mode 0 / 6), 0x35CFD0 (mode 5), 0x36A720 (mode 4), 0x372560 and
 * 0x388EE0 (mode 3), 0x373A68, 0x37A060 and 0x37F978 (mode 2); main 0x2617F0 (mode 7, the attract demo);
 * the battle script commands 0x25B900 / 0x25B9A0 / 0x25BA08 (mode 1), whose BattleSetup_Finish() is
 * called by the loader (BtlLoad_StepInitial) once the script has run.
 *
 * --- Battle mode (BattleRule.mode), as far as the callers show ------------------------------------------
 *   0  versus battle from the menu ("duel"): the only mode with rule options from the save (time limit
 *      rule[0], CPU strength rule[1], announcer rule[2]) and, with mode 4, the only one that may be
 *      split-screen. Equal health at time up is a draw only here (BtlSeq_CanDraw).
 *   1  scripted battle: set up by the commands of the battle script file 0x1FF + n (BattleSetup_SetScript).
 *      Side 0 is the pad, side 1 the CPU; every character is usable; CPU level is raised by the
 *      difficulty (gProgress->0x3C: +6 / +9); BattleSetup_FinishEx stores the first missing bit of
 *      SaveData.unlockFlags (1..7) in rule.unk18. Sequence table gBtlSeqTblMode1.
 *   2  three menu screens (0x373A68, 0x37A060, 0x37F978): pad against CPU team(s). Which game mode each
 *      is was not established.
 *   3  two menu screens (0x372560, 0x388EE0): one pad member against the opponent pool (BattleMemberPool,
 *      up to 50 members, bonus levels up to +60); pool entry 0 becomes side 1's member 0.
 *   4  menu 0x36A720: one member per side, split-screen allowed, rule.unk10 = 1.
 *   5  menu 0x35CFD0: no time limit, announcer 7, both sides one member; short sequence table.
 *   6  the versus menu when gProgress->0x18 == 0x2D: no time limit, one member per side, side 0 pad,
 *      side 1 CPU with cpuLevel -1 (does nothing), restart on result reason bit 2: training (inference).
 *   7  attract demo (main 0x2617F0): 45 s, both sides CPU, 9 fixed character pairs; the fight state goes
 *      straight to the end state.
 *   8  never passed to BattleSetup_SetRule by a caller found; read by the sequence (no winner scene) and
 *      by 0x23EE08, which looks at which sides are pads.
 *   9  only seen in BattleSetup_FixForMode: both sides CPU at level 29 with AI type 0, no time limit, then
 *      the mode becomes 6. Never passed to BattleSetup_SetRule by a caller found.
 *   Modes 8 and 9 can only arrive through a setup written some other way (a loaded replay block carries
 *   its own setup, BattleReplay_Load).
 *
 * --- Events ------------------------------------------------------------------------------------------
 * Each side has a set of up to 128 event bits (BattleEventSet): `now` collects BtlEvent_Raise() calls,
 * BtlEvent_Update() (once per unpaused frame, from Battle_UpdateWork) moves it to `prev` and ORs it into
 * `held`. BtlEvent_IsNew() is a rising edge (now & ~prev); BtlEvent_WasRaised() reads `held`, which is
 * what the result's event summary is built from (BattleResult_CollectEvents).
 * Events 0..18 are time events (BtlEvent_RaiseTimeEvents): 10, 15, ... 180 seconds on the sub clock.
 * Other ids seen here: 0x48 (BattleResult_CountFrame), 0x4C (raised on both sides when a requested
 * interrupt can start), 0x4D / 0x4E (clear the wait flag), 0x4F (calls 0x12BDA8 -> 0x23DE40),
 * 0x5A / 0x5B (forwarded to the script module, 0x259910).
 *
 * --- Replay ------------------------------------------------------------------------------------------
 * gBattleReplay (0x1ABA8 bytes at 0x301268) = a copy of the setup + recorded data + three words.
 * BattleSetup_FinishEx copies the finished setup into it; the memory card code saves it
 * (BattleReplay_GetBuffer, caller 0x11E608) and loads it back (BattleReplay_Load, caller 0x11DA38), which
 * also makes its setup the current one. `active` is tested by the input, camera and HUD code;
 * BattleResult_Set stores 0 / 1 in it for result reason bits 15 / 16.
 */

typedef struct BtlReplayData {
    /* 0x00000 */ u64 unk0[0x1A5F0 / 8]; /* 8-byte aligned: the block copy uses ld/sd without an alignment test */
} BtlReplayData; /* size 0x1A5F0 */

/* What BattleReplay_GetData() points at: the data and the flag word behind it. */
typedef struct BtlReplayRec {
    /* 0x00000 */ BtlReplayData data;
    /* 0x1A5F0 */ s32 flags;   /* bit 0 */
} BtlReplayRec;

typedef struct BtlReplay {
    /* 0x00000 */ BattleSetup setup;
    /* 0x005A8 */ BtlReplayData data;
    /* 0x1AB98 */ s32 flags;
    /* 0x1AB9C */ s32 active;  /* D_0031BE04 */
    /* 0x1ABA0 */ s32 loaded;  /* D_0031BE08 */
    /* 0x1ABA4 */ s32 unk1ABA4;
} BtlReplay; /* size 0x1ABA8 */

extern BtlReplay gBattleReplay;
extern s16 gBattleTimeLimitTbl[8];

void BattleResult_CountFrame(void);
void BattleResult_Stub1291B8(void);
void BattleResult_Finish(void);
s32 BattleResult_GetFlags(void);
s32 BattleResult_GetReason(void);
u64 BattleResult_GetEventSummary(void);
BattleResult *BattleResult_GetPtr(void);
s32 BtlClock_ToSeconds(BtlClock *clock);

void BtlEvent_RaiseTimeEvents(void);
void BtlEvent_UpdateRequests(void);
void BtlEventSet_Rotate(BattleEventSet *set);
void BtlEvent_SetBit(u64 *bits, s32 n);
s32 BtlEventSet_IsNew(BattleEventSet *set, s32 n);
s32 BtlEventSet_WasRaised(BattleEventSet *set, s32 n);
void BtlEvent_ClearAll(void);
void BtlEvent_Reset(void);
void BtlEvent_Raise(s32 side, s32 ev);
s32 BtlEvent_IsNew(s32 side, s32 ev);
s32 BtlEvent_WasRaised(s32 side, s32 ev);
void BtlEvent_Update(void);
void BtlEvent_BeginInterrupt(void);
void BtlEvent_EndInterrupt(void);
void BtlEvent_SetWaitOff(s32 off);
s32 BtlEvent_IsWaitOff(void);

void BtlMember_ClampBonus(BattleMember *m, s32 wide);
void BtlMember_ApplyItems(BattleMember *m);
void BtlMember_Init(BattleMember *m, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health, BattleItemSet *items);

void BattleSetup_FixForMode(void);
void BattleSetup_InitCharaBits(BattleCharaBits *dst, BattleCharaBits *src);
void BattleSetup_DefaultOptions(void);
void BattleSetup_Clear(void);
void BattleReplay_ClearDataFlag(void);
void BattleSetup_SetScript(s32 script);
void BattleSetup_SetOptions(s32 optA0, s32 optA1, s32 optB0, s32 optB1);
void BattleSetup_SetOption14(s32 val);
void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                         BattleCharaBits *bits);
void BattleSetup_SetMemberByItemIds(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel,
                                    f32 health, u32 *itemIds);
void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                           BattleItemSet *items);
void BattleSetup_SetPoolMember(s32 count, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                               BattleItemSet *items);
void BattleSetup_Finish(void);
void BattleSetup_FinishEx(s32 wide);

BtlReplay *BattleReplay_GetBuffer(s32 *size);
s32 BattleReplay_Load(BtlReplay *buf, s32 size);
s32 BattleReplay_IsActive(void);
s32 BattleReplay_IsLoaded(void);
BtlReplayRec *BattleReplay_GetData(void);
s32 BattleReplay_TestDataFlag(void);
void BattleReplay_SetActive(s32 active);

s32 Battle_GetHumanSide(void);
s32 Battle_GetOption14(void);
s32 Battle_GetScript(void);
s32 Battle_GetTimeLimit(void);
s32 Battle_IsTimeLimitOff(void);
s32 Battle_GetAnnouncer(void);
s32 Battle_GetBgm(void);
s32 Battle_GetStartStage(void);
s32 Battle_GetStage(void);
void Battle_SetStage(s32 stage);
s32 Battle_IsStageChanged(void);
void Battle_ResetStage(void);
s32 Battle_GetRuleUnk10(void);

s32 BattleSide_GetOptionA(s32 side);
s32 BattleSide_GetOptionB(s32 side);
s32 BattleSide_GetPad(s32 side);
s32 BattleSide_GetControl(s32 side);
s32 BattleSide_IsCpu(s32 side);
s32 BattleSide_GetUnk200(s32 side);
s32 BattleSide_GetUnk1FC(s32 side);
s32 BattleSide_GetStartChara(s32 side);
s32 BattleSide_GetStartCostume(s32 side);
s32 BattleSide_GetStartVariant(s32 side);
s32 BattleSide_GetChara(s32 side);
void BattleSide_SetChara(s32 side, s32 chara);
s32 BattleSide_IsCharaChanged(s32 side);
void BattleSide_ResetChara(s32 side);
s32 BattleSide_GetMemberChara(s32 side, s32 idx);
s32 BattleSide_GetMemberCostume(s32 side, s32 idx);
s32 BattleSide_GetMemberVariant(s32 side, s32 idx);
s32 BattleSide_GetMemberCount(s32 side);
s32 BattleSide_GetObjId(s32 side);
s32 BattleSide_GetModelSlot(s32 side);
void BattleSide_SetObjId(s32 side, s32 objId);
void BattleSide_SetModelSlot(s32 side, s32 slot);
void BattleSide_SetForm(s32 side, s32 chara, s32 costume, s32 variant);
s32 BattleSide_IsFormChanged(s32 side);
BattleMember *BattleSide_GetMember(s32 side, s32 idx);
void BattleSide_SetMemberItems(s32 side, s32 idx, BattleItemSet *items);
s32 BattleSide_IsCharaUsable(s32 side, s32 chara);

#endif
