#include "common.h"
#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "battle/btl_facade.h"
#include "battle/btl_seq.h"

/*
 * Facade between the battle event script (the command interpreter at 0x259070..0x25BE58) and the battle
 * modules, 0x12BD58..0x12C9F0. See include/battle/btl_facade.h for who calls what.
 *
 * Nearly every function is a tail call, so what a wrapper passes through is not visible in its code: the
 * parameter lists below come from reading the targets. The targets are other modules, not decompiled; the
 * names given to them here (BtlCtrl_*, BtlObj_*SubState) are guesses recorded in
 * config/symbols/btl_scene.txt. BattleSide_*, BattleResult_Set and BtlEvent_* come from battle/battle_setup.h and
 * battle/battle_work.h; DemoCam_* are btl_cam's names at the time of writing, declared here as a local view.
 */

typedef struct Vec4 Vec4;

extern void *memset(void *dst, s32 c, u32 n);
extern void *BtlObj_Get(s32 id);

/* scripted camera, 0x23DD30..0x23DE58 (state at gp 0x2FEBCC); btl_cam's names (local view) */
extern void DemoCam_AddShake(f32 time);
extern void DemoCam_SetFixedPose(Vec4 *pos, Vec4 *rot);
extern void DemoCam_GetFixedPose(Vec4 *pos, Vec4 *rot);
extern void DemoCam_ClearFixed(void);

/* fighter object sub-state (object + 0x1664) */
extern void BtlObj_SetSubState(void *obj, s32 state, s32 arg);
extern s32 BtlObj_GetSubState(void *obj);

/* per-side controller block at gp 0x2FEB10 (two 0x520-byte entries) */
extern void BtlAiMgr_SetType(s32 side, s32 value);
extern void BtlAiMgr_SetLevel(s32 side, s32 value);

/* script control of a fighter, 0x20A050..0x20B200; `side` is the BtlChar_Get index */
extern void BtlCtrl_PlayMotion(s32 side, s32 type, s32 mode, f32 value);
extern void BtlCtrl_SetPos(s32 side, Vec4 *pos);
extern void BtlCtrl_SetRot(s32 side, Vec4 *dir);
extern s32 BtlCtrl_GetActiveMember(s32 side);
extern s32 BtlCtrl_IsAction4(s32 side);
extern s32 BtlCtrl_IsMotionPlaying(s32 side);
extern void BtlCtrl_StopMotion(s32 side);
extern void BtlCtrl_SetAuraOn(s32 side);
extern void BtlCtrl_SetAuraOff(s32 side);
extern void BtlCtrl_SetChargeFx(s32 side, s32 on);
extern void BtlCtrl_BurstChargeFx(s32 side);
extern void BtlCtrl_ClearHidden(s32 side);
extern void BtlCtrl_SetHidden(s32 side);
extern void BtlCtrl_ReloadMemberBonus(s32 side, s32 member);
extern void BtlCtrl_ReloadMemberAbilities(s32 side, s32 member);
extern void BtlCtrl_SetFlag10D(s32 side);
extern void BtlCtrl_ClearFlag10D(s32 side);
extern void BtlCtrl_AddHp(s32 side, s32 member, f32 ratio);
extern void BtlCtrl_RaiseHp(s32 side, s32 member, f32 ratio);
extern void BtlCtrl_LowerHp(s32 side, s32 member, f32 ratio);
extern void BtlCtrl_AddKi(s32 side, s32 member, s32 value);
extern void BtlCtrl_RaiseKi(s32 side, s32 member, s32 value);
extern void BtlCtrl_LowerKi(s32 side, s32 member, s32 value);
extern void BtlCtrl_AddBlast(s32 side, s32 member, s32 value);
extern void BtlCtrl_RaiseBlast(s32 side, s32 member, s32 value);
extern void BtlCtrl_LowerBlast(s32 side, s32 member, s32 value);
extern void BtlCtrl_SetMaxPower(s32 side, s32 on);
extern s32 BtlCtrl_Transform(s32 side, s32 id);
extern s32 BtlCtrl_Fuse(s32 side, s32 id);
extern s32 BtlCtrl_ChangeMember(s32 side, s32 member);
extern s32 BtlCtrl_ForceFlag11x(s32 side, s32 kind);
extern s32 BtlCtrl_UseTechnique(s32 side, s32 kind);
extern s32 BtlCtrl_ForceReaction(s32 side, s32 kind);
extern s32 BtlCtrl_IsInterruptible(s32 side);
extern s32 BtlCtrl_CanAct(s32 side);

/* BtlLoad_RequestStageChange(0x243470()): asks the loader for the destroyed version of the stage */
extern void BtlStage_RequestChange(void);

/* Shakes the scripted camera at full strength for `time` seconds. */
s32 BtlFacade_ShakeCamera(f32 time) {
    DemoCam_AddShake(time);
    return 1;
}

/* Switches the scripted camera to a fixed pose (position, rotation). */
void BtlFacade_SetFixedCamera(Vec4 *pos, Vec4 *rot) {
    DemoCam_SetFixedPose(pos, rot);
}

/* Reads back the fixed camera pose. */
void BtlFacade_GetFixedCamera(Vec4 *pos, Vec4 *rot) {
    DemoCam_GetFixedPose(pos, rot);
}

/* Turns the fixed camera off. */
void BtlFacade_ClearFixedCamera(void) {
    DemoCam_ClearFixed();
}

/* Puts the side's fighter object into sub-state 14 with an argument (0x24F5D8). */
void BtlFacade_SetObjSubState14(s32 side, s32 arg) {
    BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(side)), 0xE, arg);
}

/* Puts the side's fighter object back to sub-state 0 if it is in sub-state 3. */
void BtlFacade_EndObjSubState3(s32 side) {
    void *obj = BtlObj_Get(BattleSide_GetObjId(side));

    if (BtlObj_GetSubState(obj) == 3) {
        BtlObj_SetSubState(obj, 0, -1);
    }
}

/* Stores a value in word +8 of both sides' 0x1BBxxx controller and restarts it. */
void BtlFacade_SetCpuParam8(s32 arg) {
    BtlAiMgr_SetLevel(0, arg);
    BtlAiMgr_SetLevel(1, arg);
}

/* Stores a value in word +4 of both sides' 0x1BBxxx controller and restarts it (no caller). */
void BtlFacade_SetCpuParam4(s32 arg) {
    BtlAiMgr_SetType(0, arg);
    BtlAiMgr_SetType(1, arg);
}

/* Requests a fighter position (BtlCtrl_SetPos). */
void BtlFacade_SetCharPos(s32 side, Vec4 *pos) {
    BtlCtrl_SetPos(side, pos);
}

/* Requests the fighter's second vector, +0x1570 (BtlCtrl_SetRot). */
void BtlFacade_SetCharRot(s32 side, Vec4 *dir) {
    BtlCtrl_SetRot(side, dir);
}

/* Starts a scripted move of a fighter (BtlCtrl_PlayMotion). */
void BtlFacade_PlayCharMotion(s32 side, s32 type, s32 mode, f32 value) {
    BtlCtrl_PlayMotion(side, type, mode, value);
}

/* Returns 1 when the scripted move has ended. */
s32 BtlFacade_IsCharMotionPlaying(s32 side) {
    return BtlCtrl_IsMotionPlaying(side);
}

/* Returns 1 when the fighter's action id is 4 (no caller). */
s32 BtlFacade_IsCharAction4(s32 side) {
    return BtlCtrl_IsAction4(side);
}

/* Cancels the scripted move. */
void BtlFacade_StopCharMotion(s32 side) {
    BtlCtrl_StopMotion(side);
}

/* Sets control flag 0xFE and clears 0xFF. */
void BtlFacade_SetCtrlFE(s32 side) {
    BtlCtrl_SetAuraOn(side);
}

/* Clears control flag 0xFE and sets 0xFF. */
void BtlFacade_SetCtrlFF(s32 side) {
    BtlCtrl_SetAuraOff(side);
}

/* Sets control flag 0x101. */
void BtlFacade_SetCtrl101(s32 side) {
    BtlCtrl_SetChargeFx(side, 1);
}

/* Clears control flag 0x101. */
void BtlFacade_ClearCtrl101(s32 side) {
    BtlCtrl_SetChargeFx(side, 0);
}

/* Sets control flag 0x102. */
void BtlFacade_SetCtrl102(s32 side) {
    BtlCtrl_BurstChargeFx(side);
}

/* Clears control flag 0x100. */
void BtlFacade_ClearCtrl100(s32 side) {
    BtlCtrl_ClearHidden(side);
}

/* Sets control flag 0x100. */
void BtlFacade_SetCtrl100(s32 side) {
    BtlCtrl_SetHidden(side);
}

/* Sets control flag 0x10D. */
void BtlFacade_SetCtrl10D(s32 side) {
    BtlCtrl_SetFlag10D(side);
}

/* Clears control flag 0x10D. */
void BtlFacade_ClearCtrl10D(s32 side) {
    BtlCtrl_ClearFlag10D(side);
}

/* Returns the index of the side's team member that is fighting. */
s32 BtlFacade_GetActiveMember(s32 side) {
    return BtlCtrl_GetActiveMember(side);
}

/* Raises the two per-member control flags (0x103 + member, 0x108 + member). */
void BtlFacade_NotifyMemberItems(s32 side, s32 member) {
    BtlCtrl_ReloadMemberBonus(side, member);
    BtlCtrl_ReloadMemberAbilities(side, member);
}

/* Stores a member's eight item ids (+1) in the setup and raises the per-member flags. */
void BtlFacade_SetMemberItems(s32 side, s32 member, BtlFacadeItem *src) {
    u16 buf[8];
    s32 i;

    memset(buf, 0, sizeof(buf));
    for (i = 0; i < 8; i++) {
        buf[i] = src[i].id + 1;
    }
    BattleSide_SetMemberItems(side, member, (BattleItemSet *)buf);
    if (member == -1) {
        BtlFacade_NotifyMemberItems(side, BtlCtrl_GetActiveMember(side));
    } else {
        BtlFacade_NotifyMemberItems(side, member);
    }
}

/* Adds ratio * max to a member's HP (damage or heal through the fighter when it is the active one). */
void BtlFacade_AddHp(s32 side, s32 member, f32 value) {
    if (member == -1) {
        BtlCtrl_AddHp(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_AddHp(side, member, value);
    }
}

/* Raises a member's HP to at least ratio * max. */
void BtlFacade_RaiseHp(s32 side, s32 member, f32 value) {
    if (member == -1) {
        BtlCtrl_RaiseHp(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_RaiseHp(side, member, value);
    }
}

/* Lowers a member's HP to at most ratio * max; a ratio <= 0 also raises the KO events. */
void BtlFacade_LowerHp(s32 side, s32 member, f32 value) {
    if (value <= 0.0f) {
        if (side == 0) {
            BtlEvent_Raise(side, 0x4A);
            BtlEvent_Raise(1, 0x48);
        } else {
            BtlEvent_Raise(0, 0x48);
            BtlEvent_Raise(1, 0x4A);
        }
    }
    if (member == -1) {
        BtlCtrl_LowerHp(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_LowerHp(side, member, value);
    }
}

/* Adds n * 20000 to a member's gauge +0xC. */
void BtlFacade_AddGaugeC(s32 side, s32 member, s32 value) {
    if (member == -1) {
        BtlCtrl_AddKi(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_AddKi(side, member, value);
    }
}

/* Raises a member's gauge +0xC to at least n * 20000. */
void BtlFacade_RaiseGaugeC(s32 side, s32 member, s32 value) {
    if (member == -1) {
        BtlCtrl_RaiseKi(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_RaiseKi(side, member, value);
    }
}

/* Lowers a member's gauge +0xC to at most n * 20000. */
void BtlFacade_LowerGaugeC(s32 side, s32 member, s32 value) {
    if (member == -1) {
        BtlCtrl_LowerKi(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_LowerKi(side, member, value);
    }
}

/* Adds n * 100000 to a member's gauge +0x14 (no caller). */
void BtlFacade_AddGauge14(s32 side, s32 member, s32 value) {
    if (member == -1) {
        BtlCtrl_AddBlast(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_AddBlast(side, member, value);
    }
}

/* Raises a member's gauge +0x14 to at least n * 100000. */
void BtlFacade_RaiseGauge14(s32 side, s32 member, s32 value) {
    if (member == -1) {
        BtlCtrl_RaiseBlast(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_RaiseBlast(side, member, value);
    }
}

/* Lowers a member's gauge +0x14 to at most n * 100000. */
void BtlFacade_LowerGauge14(s32 side, s32 member, s32 value) {
    if (member == -1) {
        BtlCtrl_LowerBlast(side, BtlCtrl_GetActiveMember(side), value);
    } else {
        BtlCtrl_LowerBlast(side, member, value);
    }
}

/* Fills gauge +0xC, sets the 30000 timer and control flag 6, or clears flag 6. */
void BtlFacade_SetMaxPower(s32 side, s32 on) {
    BtlCtrl_SetMaxPower(side, on);
}

/* Makes the fighter use the technique with this id from its 4-slot list; BtlEvent_SetWaitOff(0) on success. */
void BtlFacade_Transform(s32 side, s32 id) {
    if (BtlCtrl_Transform(side, id)) {
        BtlEvent_SetWaitOff(0);
    }
}

/* Makes the fighter use the technique with this id from its 3-slot list; BtlEvent_SetWaitOff(0) on success. */
void BtlFacade_Fuse(s32 side, s32 id) {
    if (BtlCtrl_Fuse(side, id)) {
        BtlEvent_SetWaitOff(0);
    }
}

/* Makes the side switch to another team member; BtlEvent_SetWaitOff(0) on success. */
void BtlFacade_ChangeMember(s32 side, s32 member) {
    if (BtlCtrl_ChangeMember(side, member)) {
        BtlEvent_SetWaitOff(0);
    }
}

/* Clears then sets control flag 0x10D (no caller). */
void BtlFacade_PulseCtrl10D(s32 side) {
    BtlFacade_ClearCtrl10D(side);
    BtlFacade_SetCtrl10D(side);
}

/* Returns 1 when both fighters are in an action the script may interrupt. */
s32 BtlFacade_AreBothInterruptible(void) {
    if (BtlCtrl_IsInterruptible(0) && BtlCtrl_IsInterruptible(1)) {
        return 1;
    }
    return 0;
}

/* Returns 1 when the fighter can take a scripted action now. */
s32 BtlFacade_CanCharAct(s32 side) {
    return BtlCtrl_CanAct(side) != 0;
}

/* Forces action 0 or 1 on a side and the matching reaction on the other. */
void BtlFacade_ForceAction01(s32 side, s32 arg) {
    BtlCtrl_ForceReaction(side == 0, 0);
    BtlCtrl_UseTechnique(side, arg != 0);
}

/* Forces action 2 or 3 on a side and the matching reaction on the other. */
void BtlFacade_ForceAction23(s32 side, s32 arg) {
    BtlCtrl_ForceReaction(side == 0, 0);
    BtlCtrl_UseTechnique(side, arg == 0 ? 2 : 3);
}

/* Forces action 4 on a side and the matching reaction on the other. */
void BtlFacade_ForceAction4(s32 side) {
    BtlCtrl_ForceReaction(side == 0, 0);
    BtlCtrl_UseTechnique(side, 4);
}

/* Empty. */
void BtlFacade_Nop(void) {
}

/* Raises forced-action flag 0x111 on both sides. */
void BtlFacade_ForceBoth111(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        BtlCtrl_ForceFlag11x(i, 1);
    }
}

/* Raises forced-action flag 0x110 on both sides. */
void BtlFacade_ForceBoth110(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        BtlCtrl_ForceFlag11x(i, 0);
    }
}

/* Forces one action per side. */
void BtlFacade_ForceActions(s32 arg0, s32 arg1) {
    s32 i;

    for (i = 0; i < 2; i++) {
        BtlCtrl_UseTechnique(i, i == 0 ? arg0 : arg1);
    }
}

/* Runs the sequence state's extra handler. */
s32 BtlFacade_CallSeqExtra(void) {
    BtlSeq_CallExtra();
    return 1;
}

/* Same as BtlFacade_CallSeqExtra (second script command). */
s32 BtlFacade_CallSeqExtra2(void) {
    BtlSeq_CallExtra();
    return 1;
}

/* Restarts the sequence's second clock. */
s32 BtlFacade_ResetSubClock(void) {
    BtlSeq_ResetSubClock();
    return 1;
}

/* Turns the end-of-battle check off or on (no caller). */
void BtlFacade_SetEndCheckOff(s32 off) {
    BtlSeq_SetEndCheckOff(off);
}

/* Returns whether the end-of-battle check is off (no caller). */
s32 BtlFacade_GetEndCheckOff(void) {
    return BtlSeq_GetEndCheckOff();
}

/* Requests the stage change of the current stage. */
void BtlFacade_RequestStageChange(void) {
    BtlStage_RequestChange();
}

/* Sets battle flag 0x200. */
void BtlFacade_SetFlag200(void) {
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
}

/* Clears battle flag 0x200. */
void BtlFacade_ClearFlag200(void) {
    Battle_GetWork()->flags &= ~BATTLE_FLAG_DEMO;
}

/* Stores one of four battle results. */
void BtlFacade_SetResult(s32 kind) {
    switch (kind) {
    case 0:
        BattleResult_Set(2, 1);
        break;
    case 1:
        BattleResult_Set(1, 1);
        break;
    case 2:
        BattleResult_Set(2, 4);
        break;
    case 3:
        BattleResult_Set(1, 4);
        break;
    }
}

/* Returns 1 while the sequence is not in the finish state. */
s32 BtlFacade_IsNotFinished(void) {
    return BtlSeq_IsFinish() == 0;
}

/* Starts a script interrupt of the fight. */
void BtlFacade_BeginInterrupt(void) {
    BtlEvent_BeginInterrupt();
}

/* Ends a script interrupt. */
void BtlFacade_EndInterrupt(void) {
    BtlEvent_EndInterrupt();
}

/* Calls BtlEvent_SetWaitOff(1): the script starts waiting for a forced action. */
void BtlFacade_BeginWait(void) {
    BtlEvent_SetWaitOff(1);
}

/* Returns the script wait state. */
s32 BtlFacade_IsWaitOff(void) {
    return BtlEvent_IsWaitOff();
}
