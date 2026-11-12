#ifndef BATTLE_BTL_FACADE_H
#define BATTLE_BTL_FACADE_H

#include "types.h"

/*
 * Battle facade: src/battle/btl_facade.c = 0x12BD58..0x12C9F0 (63 functions, no data, no rodata).
 *
 * Who uses it. Every caller but three is in the battle event script, the block 0x259070..0x25BE58 that
 * battle.h calls "menu side" (Gsc_Update / BtlScript_Update in the frame loop, work at 0x333BC0). That code is
 * a command interpreter: Gsc_GetInt fetches the next operand, Gsc_FindOption(letter) tests for an option
 * letter, and each command handler is called with a phase (0 or 4). The handlers never touch a fighter, the
 * camera, the sequence or the result block themselves; they go through this file:
 *
 *   BtlScript_UpdateView / BtlScriptCmd_SetCamera / BtlScriptCmd_ClearCamera / BtlScriptCmd_ShakeCamera / BtlScriptCmd_StopShake / BtlScriptCmd_MoveCamera
 *                    camera commands: ShakeCamera, Set/Get/ClearFixedCamera
 *   BtlScriptCmd_PlaceChar..BtlScriptCmd_StopCharMoves   one handler per fighter command: SetCharPos, SetCharDir, StartCharMove,
 *                    IsCharMoveDone, StopCharMove and the SetCtrl / ClearCtrl flags (0xFE..0x102)
 *   BtlScriptCmd_Talk    SetObjSubState14, EndObjSubState3, GetActiveMember
 *   BtlScriptCmd_EndEvent / BtlScriptCmd_BeginScene / BtlScriptCmd_EndScene   script start / reset / end: put everything back
 *                    (ClearFixedCamera, StopCharMove, SetCtrlFF, ClearCtrl101, BeginInterrupt, EndInterrupt,
 *                    and BtlScene_Reset(0) of the next module)
 *   BtlScriptCmd_Battle    30-way command switch (jump table 0x2F3040): BeginWait, then one of CallSeqExtra,
 *                    RequestStageChange, ForceBoth110/111, ForceAction01/23/4, ForceActions, UseSkillA/B,
 *                    ChangeMember, SetPowerUp, ResetSubClock
 *   BtlScriptCmd_SetResult    SetResult, IsNotFinished
 *   BtlScriptCmd_SetStatus0 / BtlScriptCmd_SetStatus1   the "set fighter status" command for side 0 / side 1: option letters pick
 *                    SetMemberItems, AddHp / RaiseHp / LowerHp and the two gauges
 *   BtlScript_IsTriggered, BtlScript_StartPending, BtlScript_UpdateHud   IsWaitOff, AreBothInterruptible, CanCharAct
 *
 * The three callers outside the script are in battle_load.c (BtlEvent code, 0x129450 and 0x129808):
 * ClearFixedCamera, AreBothInterruptible, SetFlag200 and ClearFlag200.
 *
 * Seven functions have no caller at all: SetCpuParam4, IsCharAction4, AddGauge14, PulseCtrl10D, Nop,
 * SetEndCheckOff, GetEndCheckOff.
 *
 * Conventions: `side` is 0 or 1 (the BtlChar_Get index). `member` is a team member index, or -1 for the
 * member that is fighting. A fighter is driven by "control flags": bits in its held flag array (+0x1085)
 * set with 0x1DA9D0 and cleared with 0x1DAA50; the fighter code reads them with BtlChar_TestFlag.
 */

/* One entry of the item list handed to BtlFacade_SetMemberItems; only `id` is read. */
typedef struct BtlFacadeItem {
    /* 0x00 */ u16 id;
    /* 0x02 */ u16 unk2;
} BtlFacadeItem; /* size 4 */

/* Argument of BtlFacade_SetResult. */
#define BTL_FACADE_RESULT_0 0 /* BattleResult_Set(2, 1) */
#define BTL_FACADE_RESULT_1 1 /* BattleResult_Set(1, 1) */
#define BTL_FACADE_RESULT_2 2 /* BattleResult_Set(2, 4) */
#define BTL_FACADE_RESULT_3 3 /* BattleResult_Set(1, 4) */

struct Vec4;

/* camera */
s32 BtlFacade_ShakeCamera(f32 time);
void BtlFacade_SetFixedCamera(struct Vec4 *pos, struct Vec4 *rot);
void BtlFacade_GetFixedCamera(struct Vec4 *pos, struct Vec4 *rot);
void BtlFacade_ClearFixedCamera(void);

/* fighter object and controller */
void BtlFacade_SetObjSubState14(s32 side, s32 arg);
void BtlFacade_EndObjSubState3(s32 side);
void BtlFacade_SetCpuParam8(s32 value);
void BtlFacade_SetCpuParam4(s32 value);

/* scripted movement and control flags */
void BtlFacade_SetCharPos(s32 side, struct Vec4 *pos);
void BtlFacade_SetCharRot(s32 side, struct Vec4 *dir);
void BtlFacade_PlayCharMotion(s32 side, s32 type, s32 mode, f32 value);
s32 BtlFacade_IsCharMotionPlaying(s32 side);
s32 BtlFacade_IsCharAction4(s32 side);
void BtlFacade_StopCharMotion(s32 side);
void BtlFacade_SetCtrlFE(s32 side);
void BtlFacade_SetCtrlFF(s32 side);
void BtlFacade_SetCtrl101(s32 side);
void BtlFacade_ClearCtrl101(s32 side);
void BtlFacade_SetCtrl102(s32 side);
void BtlFacade_ClearCtrl100(s32 side);
void BtlFacade_SetCtrl100(s32 side);
void BtlFacade_SetCtrl10D(s32 side);
void BtlFacade_ClearCtrl10D(s32 side);

/* team member status */
s32 BtlFacade_GetActiveMember(s32 side);
void BtlFacade_NotifyMemberItems(s32 side, s32 member);
void BtlFacade_SetMemberItems(s32 side, s32 member, BtlFacadeItem *src);
void BtlFacade_AddHp(s32 side, s32 member, f32 ratio);
void BtlFacade_RaiseHp(s32 side, s32 member, f32 ratio);
void BtlFacade_LowerHp(s32 side, s32 member, f32 ratio);
void BtlFacade_AddGaugeC(s32 side, s32 member, s32 value);
void BtlFacade_RaiseGaugeC(s32 side, s32 member, s32 value);
void BtlFacade_LowerGaugeC(s32 side, s32 member, s32 value);
void BtlFacade_AddGauge14(s32 side, s32 member, s32 value);
void BtlFacade_RaiseGauge14(s32 side, s32 member, s32 value);
void BtlFacade_LowerGauge14(s32 side, s32 member, s32 value);
void BtlFacade_SetMaxPower(s32 side, s32 on);

/* forced actions */
void BtlFacade_Transform(s32 side, s32 id);
void BtlFacade_Fuse(s32 side, s32 id);
void BtlFacade_ChangeMember(s32 side, s32 member);
void BtlFacade_PulseCtrl10D(s32 side);
s32 BtlFacade_AreBothInterruptible(void);
s32 BtlFacade_CanCharAct(s32 side);
void BtlFacade_ForceAction01(s32 side, s32 second);
void BtlFacade_ForceAction23(s32 side, s32 second);
void BtlFacade_ForceAction4(s32 side);
void BtlFacade_Nop(void);
void BtlFacade_ForceBoth111(void);
void BtlFacade_ForceBoth110(void);
void BtlFacade_ForceActions(s32 kind0, s32 kind1);

/* sequence, flags, result, script wait */
s32 BtlFacade_CallSeqExtra(void);
s32 BtlFacade_CallSeqExtra2(void);
s32 BtlFacade_ResetSubClock(void);
void BtlFacade_SetEndCheckOff(s32 off);
s32 BtlFacade_GetEndCheckOff(void);
void BtlFacade_RequestStageChange(void);
void BtlFacade_SetFlag200(void);
void BtlFacade_ClearFlag200(void);
void BtlFacade_SetResult(s32 kind);
s32 BtlFacade_IsNotFinished(void);
void BtlFacade_BeginInterrupt(void);
void BtlFacade_EndInterrupt(void);
void BtlFacade_BeginWait(void);
s32 BtlFacade_IsWaitOff(void);

#endif
