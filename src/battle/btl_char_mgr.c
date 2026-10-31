#include "battle/tag_clash.h"
#include "common.h"
#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_scene.h"
#include "battle/btl_seq.h"
#include "battle/tag_team.h"
#include "sys/common.h"
#include "sys/heap.h"
#include "sys/save.h"

/*
 * Fighter manager, 0x1C0058..0x1C2FF0: the roster (gBtlChars), the per-fighter reset and the whole-roster
 * phases of a battle frame. Layouts: include/battle/btl_char_mgr.h.
 *
 * Lifecycle
 *   BtlChar_AllocAll(n)   Battle_Init, n = 2: the manager, n fighters of 0x1600 bytes and two side arrays, all
 *                         zeroed; caches six table pointers of common file 2; BtlChar_ResetAll(); BtlCharSnd_Init().
 *   BtlChar_ResetAll()    Battle_Restart, and BtlChars_CheckStart when the fight starts: releases what the
 *                         fighters hold, zeroes the manager's counters and every fighter, then per fighter
 *                         side / pad / index, BtlChar_BindObject, BtlChar_Reset.
 *   BtlChar_FreeAll()     Battle_Term.
 *
 * One frame (the callers are Battle_Loop and Battle_Update, src/battle/battle.c). "per fighter" means a loop
 * over the roster in index order, and every per-fighter body does nothing while the fighter is frozen
 * (chr->freeze > 0, BtlChar_IsFrozen).
 *
 *   1. BtlChars_CheckStart      before Pad_Update, paused or not. The first frame the sequence is in Ready or
 *                               Fight: BtlChar_ResetAll() (except in mode 1) and set BTL_CHARS_STARTED.
 *   2. BtlChars_SampleInput     skipped under PAUSE / LOADING. per fighter: stage 2, BtlInput_Sample.
 *   3. BtlChars_UpdateInput     skipped under PAUSE; under LOADING only BtlChars_ClearObjFlag2.
 *        frame counter          if started and BtlChars_IsTimeStopped() == 0: frame++, unk1C counts down
 *        BtlChars_CheckRoundReset
 *        BtlChars_UpdateFreeze  decides who is frozen this frame
 *        BtlCharSnd_ClearRequests          empties the manager's sound request list at +0xA0
 *        per fighter            BtlChar_BeginFrame: stage 3, previous-frame copies, per-frame clears,
 *                               BtlInput_Update, re-reads of member bonus / ability
 *        BtlChars_Snapshot(0)       snapshot 0 of every fighter's position
 *        BtlChars_UpdateSightFlag          two-fighter test that sets held flag 0xBA on both
 *        per fighter            BtlChar_UpdateSeqFlags: held flags 1..4 from the sequence state, 0x135
 *   4. BtlChars_UpdateMain      skipped under PAUSE / LOADING.
 *        per fighter            BtlChar_UpdateMotion: BtlMove_UpdateAction (the action state machine: BtlAct_Update
 *                               sets stages 4 and 5 itself), seven placement requests (BtlChar_PlaceRestart..), then
 *                               pose -> object, animation (BtlObjAnim_SamplePose, BtlObjAnim_UpdateEvents), matrices
 *                               (BtlObjPose_CalcMatrices), object -> pose
 *        per fighter            BtlChar_UpdateStage6: stage 6, BtlOpp_MirrorFlags, BtlPartner_Update
 *        BtlChars_UpdateHold          fighter against fighter push-out
 *        BtlChars_Snapshot(1)
 *        per fighter            BtlChar_UpdateStage7: stage 7, BtlMove_PushOut, BtlMove_ApplyOrbit, matrices
 *        BtlChars_Snapshot(2)
 *        per fighter            BtlChar_UpdateStage8: stage 8, BtlMove_ClampToStage (stage queries), matrices,
 *                               StgCol_UpdateFighter / StgGround_UpdateFighter on the object
 *        BtlChars_Snapshot(3)
 *        per fighter            BtlChar_UpdateStage9: stage 9, BtlColl_UpdateGround, matrices
 *        BtlChars_Snapshot(4)
 *        per fighter            BtlChar_UpdateCamera: ChrCam_StartCut, ChrCam_UpdateDemo, ChrCam_UpdateInput,
 *                               then the object's final placement, BtlChar_UpdateHead, matrices, BtlFx_UpdateAll
 *        BtlReplay_UpdateViewer, BtlFx_UpdateRoster, BtlMembers_UpdateQueuedDamage (gauges), BtlChars_UpdateCollision (hits)
 *        per fighter            BtlChar_UpdateStage10: stage 10, BtlPartner_UpdateEvents, BtlFx_UpdateAfterHits
 *        BtlClash_Update
 *      (Battle_Update then runs the effect scene: BtlScene_Update, EftDet_Update, BtlScene_PostUpdate)
 *   5. BtlChars_PostScene       skipped under PAUSE / LOADING. per fighter: BtlChar_PostScene (ChrCam_Update,
 *                               low-health state, the vector pushed onto the opponent).
 *      (Battle_Update: BtlScene_CheckStageChange, loader polls, stage, both cameras, BtlCam_UpdateOverride)
 *   6. BtlChars_EndFrame        skipped under LOADING. BtlChange_Update() runs even when paused; the rest is
 *                               skipped under PAUSE:
 *        per fighter            BtlChar_UpdateLate
 *        BtlChars_UpdateObjFlag2
 *        BtlCharSnd_PlayRequests          plays the sound request list at +0xA0
 *        per fighter            BtlChar_EndFrame: stage 11, BtlChar_RaiseEvents, BtlChar_UpdateVibration,
 *                               stage 12
 *        BtlChars_Snapshot(5)
 *
 * What the callees named func_XXXXXXXX do is from a first read of their code, not from matching C:
 *   BtlChar_SetStage(chr, n) sets the stage byte chr+0x1084 = n and moves the flag bits queued "for stage n"
 *                        from the pending arrays into the live ones (BtlChar_PromoteFlags)
 *   BtlChar_IsFlagRaised(chr, f) flag f went 0 -> 1 this frame;   BtlChar_IsFlagDropped(chr, f) went 1 -> 0
 *   BtlChar_SetFrameBits(chr, m) chr->frameBits |= m
 *   BtlAct_GetCurrent(chr)   chr->action;  BtlAct_GetCurrentClass(chr) a class number of the action
 *   BtlMember_Get(chr, n) &chr->members[n]; BtlMember_GetActive(chr) the current member; BtlMember_GetActiveGauge(chr) its gauge
 *   BtlMember_GetActiveIndex(chr)   chr->curMember; BtlMember_SetActiveIndex(chr, n) sets it; BtlMember_CountAlive(chr) members with health
 *   BtlMember_GetHealthRatio(chr)   health / healthMax of the current member; BtlMember_GetTeamHealthRatio(chr) the same over the team
 *   BtlMember_HasAbility(chr, n) the current member has ability bit n
 *   BtlMember_LoadParams(chr, member, init, variant, health)  loads a member's parameters
 *   BtlMember_AddHealth(chr, n) adds n health, clamped to the maximum
 *   BtlOpp_GetPlayer(chr)   the opponent's roster index (1 for side 0, else 0)
 *   BtlChars_IsTimeStopped()      gBtlChars + 0x274: non-zero while time is stopped
 *   BtlChar_PoseToObj(chr, f) writes the pose (position, rotation matrix) into the BtlObj
 *   BtlChar_ObjToPose(chr)   reads position / rotation back from the BtlObj into the pose
 *   BtlObjAnim_SamplePose, BtlObjAnim_UpdateEvents, BtlObjPose_CalcMatrices, BtlObjBody_Warp, BtlObjHit_BuildVolumes, BtlObj_SetColorPreset, BtlObj_BindCommonTables,
 *   BtlObj_SaveNodePositions, BtlObj_SetMoveVec, BtlObj_InitChains, BtlObj_UpdateChains   BtlObj (model / skeleton) updates
 *   BtlChars_Snapshot(n)     per fighter BtlChar_Snapshot(chr, &pose, &pose + 0x10, n): position snapshot n
 *   BtlParam_GetUnkAD(chr)   byte 0xAD of the object's parameter block (BtlObj + 0x91C)
 *   BtlAtk_GetId(chr)   id of the technique in use
 *   Vec4_SetZero(v)     zeroes a Vec4
 * BtlChar_GetObj / BtlChar_GetPos / BtlChar_IsFrozen / BtlUtil_Clamp / BtlUtil_Max and the ChrCam_ / BtlReplay_
 * names come from config/symbols (btl_replay.txt, btl_char_cam.txt); they are declared here with this
 * module's own view of the fighter.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern s32 BtlChar_GetCount(void);
extern BtlMgrChr *BtlChar_Get(s32 idx);
extern void BtlChar_SetFlag(BtlMgrChr *chr, s32 flag);
extern s32 BtlChar_TestFlag(BtlMgrChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(BtlMgrChr *chr, s32 flag);
extern void BtlChar_ClearFlag(BtlMgrChr *chr, s32 flag);
extern void BtlInput_Init(BtlMgrChr *chr);
extern void BtlInput_Sample(BtlMgrChr *chr);
extern void BtlInput_Update(BtlMgrChr *chr);
extern void BtlInput_Stub(BtlMgrChr *chr);
extern s32 BtlInput_IsPressed(BtlMgrChr *chr, u32 mask);
extern s32 BtlCtrl_CanAct(s32 side);

extern void Vec4_SetZero(void *vec);
extern void BtlBodyHit_Update(void);
extern void StgGround_UpdateFighter(BtlMgrObj *obj);
extern void StgCol_UpdateFighter(BtlMgrObj *obj, s32 arg1, s32 arg2);
extern void BtlStat_ClearFrameMods(BtlMgrChr *chr);
extern void BtlStat_Reset(BtlMgrChr *chr);
extern void BtlStat_SetFrameMod(BtlMgrChr *chr, s32 idx, s32 val, s32 add);
extern void BtlAnim_Play(BtlMgrChr *chr, s32 arg1, f32 arg2);
extern void BtlAnim_FlushRequest(BtlMgrChr *chr);
extern void BtlAnim_SetObjRate(BtlMgrChr *chr, f32 rate);
extern void BtlAnim_SetRate(BtlMgrChr *chr, f32 rate);
extern s32 BtlAnim_GetId(BtlMgrChr *chr);
extern s32 BtlAnim_GetFlags(s32 idx);
extern void ChrCam_UpdateInput(BtlMgrChr *chr);
extern void ChrCam_Update(BtlMgrChr *chr);
extern void ChrCam_UpdateDemo(BtlMgrChr *chr);
extern void ChrCam_StartCut(BtlMgrChr *chr);
extern void BtlColl_Update(void);
extern void BtlColl_UpdateFrozen(BtlMgrChr *chr);
extern void BtlColl_UpdateGround(BtlMgrChr *chr);
extern void BtlColl_ClearActionBits(BtlMgrChr *chr);
extern void BtlMember_LoadParams(BtlMgrChr *chr, s32 member, s32 init, f32 health, s32 variant);
extern BtlMgrMember *BtlMember_Get(BtlMgrChr *chr, s32 idx);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *chr);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *chr);
extern void BtlMember_SetActiveIndex(BtlMgrChr *chr, s32 member);
extern s32 BtlMember_CountAlive(BtlMgrChr *chr);
extern s32 BtlMember_GetActiveIndex(BtlMgrChr *chr);
extern s32 BtlMember_GetSwitchTarget(BtlMgrChr *chr);
extern void BtlMember_AddHealth(BtlMgrChr *chr, s32 amount);
extern f32 BtlMember_GetHealthRatio(BtlMgrChr *chr);
extern f32 BtlMember_GetTeamHealthRatio(BtlMgrChr *chr);
extern s32 BtlMember_HasAbility(BtlMgrChr *chr, s32 ability);
extern void BtlMembers_UpdateQueuedDamage(void);
extern void BtlFx_UpdateObjEvents(BtlMgrChr *chr);
extern void BtlFx_UpdateAll(BtlMgrChr *chr);
extern void BtlFx_UpdateAfterHits(BtlMgrChr *chr);
extern void BtlFx_UpdatePostScene(BtlMgrChr *chr);
extern void BtlFx_UpdateLate(BtlMgrChr *chr);
extern void BtlFx_UpdateRoster(void);
extern void BtlPartner_Release(BtlMgrChr *chr);
extern void BtlPartner_Update(BtlMgrChr *chr);
extern void BtlPartner_UpdateEvents(BtlMgrChr *chr);
extern void BtlChange_Reset(void);
extern void BtlChange_Update(void);
extern s32 BtlChars_IsTimeStopped(void);
extern void BtlChar_ResetLook(BtlMgrChr *chr);
extern void BtlChar_UpdateLookOffset(BtlMgrChr *chr);
extern void BtlChar_SetLookEnabled(BtlMgrChr *chr, s32 arg1);
extern void BtlChar_UpdateHead(BtlMgrChr *chr);
extern void BtlChar_ObjToPose(BtlMgrChr *chr);
extern void BtlChar_PoseToObj(BtlMgrChr *chr, s32 arg1);
extern void BtlChar_UpdateLean(BtlMgrChr *chr);
extern void BtlChar_PlaceAtStart(BtlMgrChr *chr);
extern void BtlChar_PlaceRestart(BtlMgrChr *chr);
extern void BtlChar_PlaceCenter(BtlMgrChr *chr);
extern void BtlChar_PlaceRelative(BtlMgrChr *chr);
extern void BtlChar_PlaceOnPath(BtlMgrChr *chr);
extern void BtlChar_PlaceCenterHigh(BtlMgrChr *chr);
extern void BtlChar_PlaceSaved(BtlMgrChr *chr);
extern void BtlChar_PlaceWarp(BtlMgrChr *chr);
extern void BtlChars_UpdateHold(void);
extern void BtlChar_ClearSnapshots(BtlMgrChr *chr);
extern void BtlChars_Snapshot(s32 pass);
extern void BtlChar_GetSnapDelta(BtlMgrChr *chr, void *vec, s32 arg2, s32 arg3);
extern void BtlChar_GetMoveSince(BtlMgrChr *chr, void *vec, s32 arg2);
extern void BtlReplay_Rewind(BtlMgrChr *chr);
extern void BtlReplay_ResetViewer(void);
extern void BtlReplay_UpdateViewer(void);
extern void BtlClash_Update(void);
extern void BtlCharSnd_Init(void);
extern void BtlCharSnd_ClearRequests(void);
extern void BtlCharSnd_PlayRequests(void);
extern void BtlCharSnd_PlayTechSounds(BtlMgrChr *chr);
extern void BtlChar_SetStage(BtlMgrChr *chr, s32 stage);
extern void BtlChar_ClearFlagRange(BtlMgrChr *chr, s32 first, s32 last);
extern s32 BtlChar_IsFlagRaised(BtlMgrChr *chr, s32 flag);
extern s32 BtlChar_IsFlagDropped(BtlMgrChr *chr, s32 flag);
extern void BtlChar_UpdateLockOn(BtlMgrChr *chr);
extern void BtlChars_UpdateSightFlag(void);
extern void BtlChar_SetFrameBits(BtlMgrChr *chr, u64 bits);
extern s32 BtlOpp_GetPlayer(BtlMgrChr *chr);
extern void BtlOpp_MirrorFlags(BtlMgrChr *chr);
extern s32 BtlUtil_Clamp(s32 val, s32 lo, s32 hi);
extern s32 BtlUtil_Max(s32 a, s32 b);
extern BtlMgrObj *BtlChar_GetObj(BtlMgrChr *chr);
extern BtlMgrPose *BtlChar_GetPos(BtlMgrChr *chr);
extern s32 BtlChar_IsFrozen(BtlMgrChr *chr);
extern s32 BtlChar_TestMemberUnk70(BtlMgrChr *chr);
extern void BtlChar_UpdateVibration(BtlMgrChr *chr);
extern void BtlChar_StopVoiceOnFlag(BtlMgrChr *chr);
extern void BtlChar_TickVoiceTimers(BtlMgrChr *chr);
extern void BtlMove_UpdateAnimVoice(BtlMgrChr *chr);
extern void BtlMove_UpdateAction(BtlMgrChr *chr);
extern void BtlMove_ClampToStage(BtlMgrChr *chr);
extern void BtlMove_ApplyOrbit(BtlMgrChr *chr);
extern void BtlMove_PushOut(BtlMgrChr *chr);
extern void BtlAct_ClearQueue(BtlMgrChr *chr);
extern s32 BtlAct_GetCurrent(BtlMgrChr *chr);
extern s32 BtlAct_GetCurrentClass(BtlMgrChr *chr);
extern s32 BtlCharApi_EnteredTechnique(s32 objId);
extern s32 BtlCharApi_IsSkillStart(s32 objId);
extern s32 BtlCharApi_IsChanging(s32 objId);
extern s32 BtlAtk_GetId(BtlMgrChr *chr);
extern s32 BtlParam_GetUnk0(BtlMgrChr *chr);
extern s32 BtlParam_GetUnkAD(BtlMgrChr *chr);
extern s32 BtlParam_GetAuraKind(BtlMgrChr *chr);
extern s32 BtlParam_GetUnk70(BtlMgrChr *chr);
extern s32 BtlParam_GetUnk72(BtlMgrChr *chr);
extern void BtlObjAnim_SamplePose(BtlMgrObj *obj);
extern void BtlObjAnim_UpdateEvents(BtlMgrObj *obj);
extern void BtlObjBody_Warp(BtlMgrObj *obj, void *vec);
extern void BtlObjHit_BuildVolumes(BtlMgrObj *obj);
extern void BtlObjPose_CalcMatrices(BtlMgrObj *obj);
extern void BtlObj_SetColorPreset(BtlMgrObj *obj, s32 arg1, s32 arg2);
extern void BtlObj_BindCommonTables(BtlMgrObj *obj);
extern void BtlObj_SaveNodePositions(BtlMgrObj *obj, s32 arg1);
extern void BtlObj_SetMoveVec(BtlMgrObj *obj, void *vec);
extern void BtlObj_InitChains(BtlMgrObj *obj);
extern void BtlObj_UpdateChains(BtlMgrObj *obj);

/* The word of the object's +0x1660 block that mirrors BtlChar_TestMemberUnk70(). */
#define OBJ_WORD_18028(obj) (*(s32 *)((obj)->unk1660 + 0x18028))

/* A table inside a data file: the header word gives its byte offset. */
#define FILE_TABLE(file, word) ((u32 *)(file) + ((u32 *)(file))[word] / 4)

/* Binds the fighter to its side's object and clears the state that belongs to the old object. */
void BtlChar_BindObject(BtlMgrChr *chr) {
    BtlMgrObj *obj;
    s32 i;

    chr->objId = BattleSide_GetObjId(chr->side);
    obj = BtlChar_GetObj(chr);
    chr->unk980 = -1;
    for (i = 0; i < 3; i++) {
        if (obj->file[i] != NULL) {
            chr->objTbl[i] = FILE_TABLE(obj->file[i], 3);
        }
    }
    BtlChar_ClearFlag(chr, 0xBE);
    BtlChar_ClearFlagRange(chr, 0x98, 0x99);
    BtlObj_InitChains(obj);
    memset(chr->unkE00, 0, 0x40);
    BtlStat_Reset(chr);
    OBJ_WORD_18028(obj) = 0;
}

/* Copies one member's seven bonus levels from the battle setup. */
void BtlChar_CopyMemberBonus(BtlMgrChr *chr, s32 idx) {
    BtlMgrMember *dst = &chr->members[idx];
    BattleMember *src = BattleSide_GetMember(chr->side, idx);

    dst->bonus[0] = src->bonus[1];
    dst->bonus[1] = src->bonus[2];
    dst->bonus[2] = src->bonus[3];
    dst->bonus[3] = src->bonus[4];
    dst->bonus[4] = src->bonus[5];
    dst->bonus[5] = src->bonus[6];
    dst->bonus[6] = src->bonus[7];
}

/* Copies every member's bonus levels. */
void BtlChar_CopyAllBonus(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        BtlChar_CopyMemberBonus(chr, i);
    }
}

/* Copies one member's four ability words from the battle setup. */
void BtlChar_CopyMemberAbility(BtlMgrChr *chr, s32 idx) {
    BtlMgrMember *dst = &chr->members[idx];
    BattleMember *src = BattleSide_GetMember(chr->side, idx);
    s32 i;

    for (i = 0; i < 4; i++) {
        dst->ability[i] = src->ability[i];
    }
}

/* Copies every member's ability words. */
void BtlChar_CopyAllAbility(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        BtlChar_CopyMemberAbility(chr, i);
    }
}

/* Resets one fighter from its side of the battle setup. */
void BtlChar_Reset(BtlMgrChr *chr) {
    s32 side = chr->side;
    s32 i;
    BtlMgrObj *obj;
    BattleMember *m;
    BtlMgrMember *mem;
    BattleResult *res;

    BtlChar_SetStage(chr, BTL_CHR_STAGE_RESET);
    chr->optA = BattleSide_GetOptionA(side);
    chr->optBOff = BattleSide_GetOptionB(side) == 0;
    chr->padFlagA = (gSaveData->flags & (2 << chr->pad)) != 0;
    chr->unk1590 = -1;
    chr->unk1594 = -1;
    BtlInput_Init(chr);
    BtlAct_ClearQueue(chr);
    chr->injectOn = BattleSide_IsCpu(side);
    BtlChar_PlaceAtStart(chr);
    BtlReplay_Rewind(chr);
    BtlChar_ResetLook(chr);
    BtlChar_SetHeldFlag(chr, 5);
    obj = BtlChar_GetObj(chr);
    BtlChar_PoseToObj(chr, 1);
    BtlAnim_Play(chr, 0, 0.0f);
    BtlObjAnim_SamplePose(obj);
    BtlObjPose_CalcMatrices(obj);
    chr->memberCount = BattleSide_GetMemberCount(side);
    chr->unk1300 = BattleSide_GetUnk1FC(side);
    chr->unkCF4 = BattleSide_GetUnk200(side);
    BtlChar_CopyAllBonus(chr);
    BtlChar_CopyAllAbility(chr);
    for (i = 0; i < chr->memberCount; i++) {
        mem = &chr->members[i];
        mem->present = 1;
        mem->chara = BattleSide_GetMemberChara(side, i);
        mem->costume = BattleSide_GetMemberCostume(side, i);
        mem->cpuLevel = BattleSide_GetMember(side, i)->cpuLevel;
        mem->aiType = BattleSide_GetMember(side, i)->aiType;
        m = BattleSide_GetMember(side, i);
        BtlMember_LoadParams(chr, i, 1, m->health, BattleSide_GetMemberVariant(side, i));
    }
    chr->unk99C = 0;
    if (BtlMember_HasAbility(chr, 0x1B)) {
        chr->unk99C = 100000;
    }
    res = BattleResult_GetPtr();
    res->unk1C[TagTeam_NativeSide(chr->side)] = 0;
    res->unk24[TagTeam_NativeSide(chr->side)] = 0;
    if (BtlMember_GetActiveGauge(chr)->unk20 != 0) {
        BtlChar_GetObj(chr)->flags |= 0x40000000;
    }
}

/* Applies the form change of the current action once the new model is loaded. */
void BtlChar_OnModelLoaded(BtlMgrChr *chr) {
    BtlMgrObj *obj;
    BtlMgrMember *m;
    BtlMgrGauge *g;
    BtlMgrMember *other;
    BtlMgrGauge *og;
    f32 ratio;
    s32 i;

    BtlChar_BindObject(chr);
    obj = BtlChar_GetObj(chr);
    BtlChar_SetFlag(chr, 0x12B);
    BtlAnim_FlushRequest(chr);
    BtlObjAnim_SamplePose(obj);
    BtlChar_PoseToObj(chr, 1);
    BtlObjPose_CalcMatrices(obj);
    switch (BtlAct_GetCurrent(chr)) {
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
    case 0xF0:
        /* transformation: keep the health ratio */
        m = BtlMember_GetActive(chr);
        m->chara = chr->newChara;
        g = &m->gauge;
        m->costume = chr->newCostume;
        g->unk20 = chr->new20;
        ratio = BtlMember_GetHealthRatio(chr);
        BtlMember_LoadParams(chr, BtlMember_GetActiveIndex(chr), 0, 0.0f, 0);
        g->health = (f32)g->healthMax * ratio + 0.5f;
        if (BtlParam_GetUnkAD(chr) & 1) {
            g->health += 5000;
        }
        if (BtlParam_GetUnkAD(chr) & 2) {
            g->health += 10000;
        }
        if (BtlParam_GetUnkAD(chr) & 4) {
            g->ki = g->kiMax;
        }
        g->health = BtlUtil_Clamp(g->health, 1, g->healthMax);
        g->blast = BtlUtil_Clamp(g->blast, 0, g->blastMax);
        break;
    case 0xF3:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
        /* switch to another team member */
        BtlMember_SetActiveIndex(chr, chr->nextMember);
        BtlMember_GetSwitchTarget(chr);
        chr->unk99C = 0;
        g = &BtlMember_GetActive(chr)->gauge;
        if (BtlMember_HasAbility(chr, 0x43)) {
            BtlChar_SetHeldFlag(chr, 6);
            g->unk1C = 30000;
            g->ki = g->kiMax;
        } else {
            BtlChar_ClearFlag(chr, 6);
            g->unk1C = 0;
        }
        if (BtlMember_HasAbility(chr, 0x61)) {
            g->blast = g->blastMax;
        }
        if (BtlChar_TestMemberUnk70(chr)) {
            BtlObj_BindCommonTables(obj);
        }
        break;
    case 0xF1:
    case 0xF2:
        /* fusion: merge the partner member into the current one */
        m = BtlMember_GetActive(chr);
        g = &m->gauge;
        other = BtlMember_Get(chr, chr->fusionMember);
        if (other != NULL) {
            og = &other->gauge;
            m->bonus[0] = BtlUtil_Clamp(m->bonus[0] + other->bonus[0], -20, 40);
            m->bonus[1] = BtlUtil_Clamp(m->bonus[1] + other->bonus[1], -20, 40);
            m->bonus[2] = BtlUtil_Clamp(m->bonus[2] + other->bonus[2], -20, 40);
            m->bonus[3] = BtlUtil_Clamp(m->bonus[3] + other->bonus[3], -20, 40);
            m->bonus[4] = BtlUtil_Clamp(m->bonus[4] + other->bonus[4], -20, 40);
            m->bonus[5] = BtlUtil_Clamp(m->bonus[5] + other->bonus[5], -20, 40);
            m->bonus[6] = BtlUtil_Clamp(m->bonus[6] + other->bonus[6], -20, 40);
            for (i = 0; i < 4; i++) {
                m->ability[i] |= other->ability[i];
            }
            g->health += og->health;
            g->healthMax += og->healthMax;
            g->blastMax = gBtlChars->tbl[4][m->chara * 4 + 3] * 100000;
            if (BtlParam_GetUnkAD(chr) & 1) {
                g->health += 5000;
            }
            if (BtlParam_GetUnkAD(chr) & 2) {
                g->health += 10000;
            }
            if (BtlParam_GetUnkAD(chr) & 4) {
                g->ki = g->kiMax;
            }
            if (BtlParam_GetUnkAD(chr) & 8) {
                BtlChar_SetHeldFlag(chr, 6);
                g->unk1C = 30000;
                g->ki = g->kiMax;
            }
            g->health = BtlUtil_Clamp(g->health, 1, g->healthMax);
            g->blast = BtlUtil_Clamp(g->blast, 0, g->blastMax);
            other->present = 0;
            og->health = 0;
        }
        g->unk34 = 1;
        m->chara = chr->newChara;
        m->costume = chr->newCostume;
        g->unk20 = chr->new20;
        break;
    case 0x104:
    case 0x106:
    case 0x107:
    case 0x108:
    case 0x12D:
    case 0x12E:
    case 0x12F:
    case 0x139:
    case 0x13A:
    case 0x13B:
        m = BtlMember_GetActive(chr);
        m->chara = chr->newChara;
        g = &m->gauge;
        m->costume = chr->newCostume;
        g->unk20 = chr->new20;
        if (BtlChar_TestFlag(chr, 0xA6)) {
            g->unk30 = 1;
            BtlObj_BindCommonTables(obj);
            BtlMember_LoadParams(chr, BtlMember_GetActiveIndex(chr), 0, 0.0f, 0);
            g->health = g->healthMax;
        }
        break;
    }
    { extern void TagFusion_ModelLoaded(s32); TagFusion_ModelLoaded(chr->side); }
    BtlObj_SetColorPreset(obj, BtlParam_GetAuraKind(chr), -1);
    if (BtlMember_GetActiveGauge(chr)->unk20 != 0) {
        obj->flags |= 0x40000000;
    } else {
        obj->flags &= ~0x40000000;
    }
}

/* Clears the pose block and the per-stage state after a stage change. */
void BtlChar_OnStageLoaded(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    memset(&chr->pose, 0, 0x2D0);
    BtlChar_PlaceAtStart(chr);
    BtlChar_ClearFlag(chr, 0xE);
    BtlChar_SetHeldFlag(chr, 0xF);
    chr->unk15D4[0] = 0;
    chr->unk15D4[1] = 0;
    chr->unk15D4[2] = 0;
    chr->unk15D4[3] = 0;
    chr->unk15D4[4] = 0;
    BtlChar_ResetLook(chr);
    BtlChar_PoseToObj(chr, 1);
    if ((BtlAnim_GetFlags(BtlAnim_GetId(chr)) & 0x10) || chr->unkFB0 >= 3) {
        BtlChar_SetHeldFlag(chr, 0x13A);
    } else {
        BtlChar_SetHeldFlag(chr, 0x139);
    }
    chr->unkFB0 = 1;
    BtlAnim_Play(chr, 0, 0.0f);
    BtlObjAnim_SamplePose(obj);
    BtlObjPose_CalcMatrices(obj);
}

/* Puts the fighter back to the round start action (0xF9) with its timers cleared. */
void BtlChar_ResetRound(BtlMgrChr *chr) {
    s32 prev;

    if (chr->unk1330 != 0) {
        BtlPartner_Release(chr);
    }
    BtlChar_ClearFlag(chr, 0xE);
    BtlChar_SetHeldFlag(chr, 0xF);
    BtlChar_SetHeldFlag(chr, 5);
    BtlChar_ClearFlag(chr, 0x125);
    BtlChar_PlaceAtStart(chr);
    chr->prevAction = chr->action;
    chr->action = 0xF9;
    chr->unk94C = -1;
    BtlAct_ClearQueue(chr);
    chr->unkFB0 = 1;
    chr->unkD58 = 0.5f;
    chr->unkD5C = 500.0f;
    chr->freezeNext = 0;
    chr->freezeDelay = 0;
    chr->freeze = 0;
    BtlChar_ResetLook(chr);
    if (Battle_GetMode() == 3 && chr->side == 0) {
        BtlMember_AddHealth(chr, 10000);
    }
}

/* Resets the work area of the fighter's object. */
void BtlChar_ResetObjWork(BtlMgrChr *chr) {
    BtlObj_InitChains(BtlChar_GetObj(chr));
}

/* Decides which fighters are frozen (hit-stop) and counts the freeze timers down. */
void BtlChars_UpdateFreeze(void) {
    s32 level[PORT_FIGHTER_CAPACITY];
    s32 i;
    s32 max;
    u8 *bytes;
    BtlMgrChr *chr;

    max = 0;
    bytes = (u8 *)level;
    memset(bytes, 0, sizeof(level));
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        if (BtlChar_TestFlag(chr, 0x125)) {
            level[i] = 2;
        } else if (BtlChar_TestFlag(chr, 0x126)) {
            level[i] = 1;
        }
        if (max < level[i]) {
            max = level[i];
        }
    }
    /* Native technique startup assumes exactly one opponent. In Tag Team the
       per-context cinematic policy owns that pause; hit-stop from an actual
       impact still uses each fighter's freeze timers below. */
    if (max > 0 && !(TagTeam_Active() && BtlSeq_IsFighting())) {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            chr->freezeNext = 1;
            chr->freezeDelay = 0;
        }
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            if (max == level[i]) {
                chr->freezeNext = 0;
                chr->freezeDelay = 0;
                if (max >= 2) {
                    break;
                }
            }
            if (BtlChar_TestFlag(chr, 0x127)) {
                if (max == 1) {
                    chr->freezeNext = 0;
                    chr->freezeDelay = 0;
                }
            }
        }
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        if (chr->freeze > 0) {
            s32 v = chr->freeze - 1;

            if (v < 0) {
                v = 0;
            }
            chr->freeze = v;
        }
        if (chr->freezeNext != 0) {
            if (chr->freezeDelay != 0) {
                chr->freezeDelay--;
            } else {
                chr->freeze = chr->freezeNext;
                chr->freezeNext = 0;
            }
        }
    }
}

/* Sets bit 1 of every fighter object's flags: for the fighter with flag 0x12A only, or for all without one. */
void BtlChars_UpdateObjFlag2(void) {
    s32 i;
    s32 found = -1;
    BtlMgrChr *chr;

    /* A startup cut used to hide the entire other side. Visibility is global
       across views, so never use that shortcut in a live Tag Team match.
       Per-actor vanish, transformation and fusion flags still apply below. */
    for (i = 0; i < BtlChar_GetCount() && !(TagTeam_Active() && BtlSeq_IsFighting()); i++) {
        if (BtlChar_TestFlag(BtlChar_Get(i), 0x12A)) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            if (BtlChar_TestFlag(chr, 0x30) || BtlChar_TestFlag(chr, 0xB)) {
                BtlChar_GetObj(chr)->flags &= ~2;
            } else {
                BtlChar_GetObj(chr)->flags |= 2;
            }
        }
    } else {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            if (i == found) {
                if (BtlChar_TestFlag(chr, 0x30) || BtlChar_TestFlag(chr, 0xB)) {
                    BtlChar_GetObj(chr)->flags &= ~2;
                } else {
                    BtlChar_GetObj(chr)->flags |= 2;
                }
            } else {
                BtlChar_GetObj(chr)->flags &= ~2;
            }
        }
    }
}

/* Clears bit 1 of every fighter object's flags. */
void BtlChars_ClearObjFlag2(void) {
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_GetObj(BtlChar_Get(i))->flags &= ~2;
    }
}

/* Runs the fighter-against-fighter pass, or the per-fighter fallback while somebody is frozen. */
void BtlChars_UpdateCollision(void) {
    s32 i;
    s32 nobodyFrozen = 1;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        if (BtlChar_Get(i)->freeze > 0) {
            nobodyFrozen = 0;
            break;
        }
    }
    if (nobodyFrozen) {
        BtlBodyHit_Update();
        BtlColl_Update();
    } else {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            BtlColl_UpdateFrozen(BtlChar_Get(i));
        }
    }
}

/* Raises this frame's battle events for one fighter. */
void BtlChar_RaiseEvents(BtlMgrChr *chr) {
    BtlMgrGauge *g = BtlMember_GetActiveGauge(chr);
    s32 lost = g->healthMax - g->health;
    s32 tech;

    if (lost >= 70000) {
        BtlEvent_Raise(chr->side, 0x1C);
    } else if (lost >= 60000) {
        BtlEvent_Raise(chr->side, 0x1B);
    } else if (lost >= 50000) {
        BtlEvent_Raise(chr->side, 0x1A);
    } else if (lost >= 40000) {
        BtlEvent_Raise(chr->side, 0x19);
    } else if (lost >= 30000) {
        BtlEvent_Raise(chr->side, 0x18);
    } else if (lost >= 20000) {
        BtlEvent_Raise(chr->side, 0x17);
    } else if (lost >= 10000) {
        BtlEvent_Raise(chr->side, 0x16);
    }
    if (g->ki == g->kiMax) {
        BtlEvent_Raise(chr->side, 0x1D);
    }
    if (g->blast == g->blastMax) {
        BtlEvent_Raise(chr->side, 0x1E);
    }
    if (BtlChar_IsFlagRaised(chr, 6)) {
        BtlEvent_Raise(chr->side, 0x1F);
    }
    if (BtlCharApi_EnteredTechnique(chr->objId)) {
        switch (BtlAct_GetCurrentClass(chr)) {
        case 2:
            BtlEvent_Raise(chr->side, 0x26);
            break;
        case 3:
            BtlEvent_Raise(chr->side, 0x27);
            break;
        case 4:
            BtlEvent_Raise(chr->side, 0x28);
            break;
        }
    }
    if (BtlCharApi_IsSkillStart(chr->objId)) {
        switch (BtlAct_GetCurrentClass(chr)) {
        case 0:
            BtlEvent_Raise(chr->side, 0x24);
            break;
        case 1:
            BtlEvent_Raise(chr->side, 0x25);
            break;
        }
    }
    if (BtlChar_TestFlag(chr, 8)) {
        switch (BtlAct_GetCurrent(chr)) {
        case 0x130:
        case 0x131:
        case 0x132:
            BtlEvent_Raise(chr->side, 0x14);
            break;
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
            BtlEvent_Raise(chr->side, 0x42);
            break;
        case 0x41:
            BtlEvent_Raise(chr->side, 0x44);
            break;
        case 0x42:
            BtlEvent_Raise(chr->side, 0x45);
            break;
        case 0x19:
            if (chr->unkD68 > 0) {
                BtlEvent_Raise(chr->side, 0x31);
            }
            break;
        case 0xFA:
        case 0xFC:
            BtlEvent_Raise(chr->side, 0x15);
            break;
        case 0x43:
            BtlEvent_Raise(chr->side, 0x21);
            break;
        }
    }
    if (BtlChar_IsFlagRaised(chr, 0x5B)) {
        tech = BtlAtk_GetId(chr);
        switch (tech) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
            BtlEvent_Raise(chr->side, 0x3D);
            break;
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
            BtlEvent_Raise(chr->side, 0x34);
            break;
        case 0x3E:
        case 0x3F:
        case 0x40:
        case 0x41:
        case 0x42:
            BtlEvent_Raise(chr->side, 0x3E);
            BtlChar_SetFrameBits(chr, 0x20);
            if (chr->unkD70 > 0) {
                BtlChar_SetFrameBits(chr, 0x100000);
            }
            break;
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
            BtlEvent_Raise(chr->side, 0x32);
            BtlChar_SetFrameBits(chr, 0x20);
            if (chr->unkD68 > 0) {
                BtlChar_SetFrameBits(chr, 0x100000);
            }
            break;
        case 0x55:
            BtlEvent_Raise(chr->side, 0x33);
            break;
        case 0x23:
        case 0x25:
        case 0x27:
            BtlEvent_Raise(chr->side, 0x40);
            break;
        case 0x29:
        case 0x2B:
        case 0x2D:
            BtlEvent_Raise(chr->side, 0x56);
            break;
        case 0x8A:
            BtlEvent_Raise(chr->side, 0x3F);
            break;
        case 0x53:
            BtlEvent_Raise(chr->side, 0x4B);
            BtlChar_SetFrameBits(chr, 0x80000);
            break;
        case 0x10:
        case 0x52:
        case 0x58:
        case 0x60:
        case 0x65:
        case 0x6F:
        case 0x72:
        case 0x74:
        case 0x7B:
        case 0x7D:
        case 0x90:
        case 0x92:
            BtlEvent_Raise(chr->side, 0x4B);
            break;
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
            BtlChar_SetFrameBits(chr, 0x2000000);
            break;
        }
        switch (tech) {
        case 0x1D:
        case 0x3E:
        case 0x67:
            BtlEvent_Raise(chr->side, 0x37);
            break;
        case 0x1E:
        case 0x3F:
        case 0x68:
            BtlEvent_Raise(chr->side, 0x38);
            break;
        case 0x1F:
        case 0x40:
        case 0x69:
            BtlEvent_Raise(chr->side, 0x39);
            break;
        case 0x20:
        case 0x41:
        case 0x6A:
            BtlEvent_Raise(chr->side, 0x3A);
            break;
        case 0x21:
        case 0x42:
        case 0x6B:
            BtlEvent_Raise(chr->side, 0x3B);
            break;
        }
    }
    if (BtlChar_TestFlag(chr, 0x70)) {
        BtlEvent_Raise(chr->side, 0x43);
    }
    if (BtlChar_IsFlagRaised(chr, 0xBE)) {
        BtlEvent_Raise(chr->side, 0x23);
    }
    if (BtlChar_TestFlag(chr, 0xBF)) {
        BtlEvent_Raise(chr->side, 0x47);
    }
    if (BtlChar_TestFlag(chr, 0xC1)) {
        BtlEvent_Raise(chr->side, 0x46);
        BtlChar_SetFrameBits(chr, 0x800000);
    }
    if (BtlChar_TestFlag(chr, 0x6C)) {
        BtlEvent_Raise(chr->side, 0x57);
    }
    if (BtlChar_TestFlag(chr, 3)) {
        BattleResult *res = BattleResult_GetPtr();
        s32 opp = TagTeam_NativeSide(BtlOpp_GetPlayer(chr));

        if (res->unk1C[opp] < chr->unkD40) {
            res->unk1C[opp] = chr->unkD40;
        }
        if (res->unk24[opp] < chr->unkD44) {
            res->unk24[opp] = chr->unkD44;
        }
        res->health[TagTeam_NativeSide(chr->side)] = BtlMember_GetTeamHealthRatio(chr) * 100.0f;
        if (BtlMember_GetActiveGauge(chr)->health == 1) {
            if (BtlMember_CountAlive(BtlChar_Get(BtlOpp_GetPlayer(chr))) <= 0) {
                BtlEvent_Raise(chr->side, 0x54);
            }
        }
    }
    if (BtlInput_IsPressed(chr, 0x2000) && BtlCtrl_CanAct(chr->side)) {
        BtlEvent_Raise(chr->side, 0x50);
    }
    if (BtlChar_TestFlag(chr, 7)) {
        BtlEvent_Raise(chr->side, 0x49);
    }
    if (BtlChar_IsFlagRaised(chr, 0x84)) {
        BtlChar_SetFrameBits(chr, 0x4000000);
    }
    if (BtlChar_IsFlagRaised(chr, 0xA4)) {
        BtlChar_SetFrameBits(chr, 0x1000000);
    }
}

/* When any fighter raised flag 0xF9, resets every fighter for the next round and the 0x2000 effect tasks. */
void BtlChars_CheckRoundReset(void) {
    s32 i;
    s32 any = 0;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        if (BtlChar_TestFlag(BtlChar_Get(i), 0xF9)) {
            any = 1;
            break;
        }
    }
    if (any) {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            BtlChar_ResetRound(BtlChar_Get(i));
        }
        BtlScene_Reset(BTL_SCENE_RESET_2000);
    }
}

/* Re-reads the bonus levels of each member whose request flag (0x103 + n) is set. */
void BtlChar_ApplyBonusRequests(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < BTL_CHR_MEMBER_MAX; i++) {
        if (BtlChar_TestFlag(chr, i + 0x103)) {
            BtlChar_CopyMemberBonus(chr, i);
            BtlChar_ClearFlag(chr, i + 0x103);
        }
    }
}

/* Re-reads the ability words of each member whose request flag (0x108 + n) is set. */
void BtlChar_ApplyAbilityRequests(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < BTL_CHR_MEMBER_MAX; i++) {
        if (BtlChar_TestFlag(chr, i + 0x108)) {
            BtlChar_CopyMemberAbility(chr, i);
            BtlChar_ClearFlag(chr, i + 0x108);
        }
    }
}

/* Phase 2 body: builds this frame's input record from the pad. */
void BtlChar_SampleInput(BtlMgrChr *chr) {
    if (!BtlChar_IsFrozen(chr)) {
        BtlChar_SetStage(chr, BTL_CHR_STAGE_SAMPLE);
        BtlInput_Sample(chr);
    }
}

/* Phase 3 body: keeps last frame's values, clears the per-frame state and consumes the input record. */
void BtlChar_BeginFrame(BtlMgrChr *chr) {
    if (BtlChar_IsFrozen(chr)) {
        BtlInput_Stub(chr);
        return;
    }
    BtlChar_SetStage(chr, BTL_CHR_STAGE_INPUT);
    chr->prevPose = chr->pose;
    BtlChar_ObjToPose(chr);
    BtlChar_GetPos(chr)->unkA0 = 0;
    BtlChar_GetPos(chr)->unkA4 = 0;
    BtlChar_ClearSnapshots(chr);
    chr->prevAction = chr->action;
    chr->prev964 = chr->unk964;
    chr->prev974 = chr->unk974;
    BtlAnim_SetObjRate(chr, 1.0f);
    BtlAnim_SetRate(chr, 1.0f);
    chr->unkD4C = 0;
    chr->unkD50 = 0;
    BtlChar_SetLookEnabled(chr, 0);
    BtlChar_TickVoiceTimers(chr);
    BtlColl_ClearActionBits(chr);
    chr->prev1262 = chr->unk1262;
    memset(&chr->unk1262, 0, sizeof(chr->unk1262));
    if (BtlChar_TestFlag(chr, 6)) {
        chr->unkD6C = BtlParam_GetUnk70(chr);
        chr->unkD74 = BtlParam_GetUnk72(chr);
    } else {
        chr->unkD6C = 1;
        chr->unkD74 = 1;
    }
    if (BtlMember_HasAbility(chr, 1)) {
        chr->unkD6C += 2;
    } else if (BtlMember_HasAbility(chr, 0)) {
        chr->unkD6C += 1;
    }
    if (BtlMember_HasAbility(chr, 3)) {
        chr->unkD74 += 2;
    } else if (BtlMember_HasAbility(chr, 2)) {
        chr->unkD74 += 1;
    }
    chr->frameBits = 0;
    chr->unk1590 = -1;
    chr->unk1594 = -1;
    BtlInput_Update(chr);
    BtlChar_ApplyBonusRequests(chr);
    BtlChar_ApplyAbilityRequests(chr);
}

/* Phase 3 body: sets the held flag of the sequence state (1 intro, 2 ready, 3 fight, 4 after) and flag 0x135. */
void BtlChar_UpdateSeqFlags(BtlMgrChr *chr) {
    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    BtlChar_ClearFlag(chr, 1);
    BtlChar_ClearFlag(chr, 2);
    BtlChar_ClearFlag(chr, 3);
    BtlChar_ClearFlag(chr, 4);
    switch (BtlSeq_GetState()) {
    case 0:
    case 1:
        BtlChar_SetHeldFlag(chr, 1);
        break;
    case 2:
        BtlChar_SetHeldFlag(chr, 2);
        break;
    case 3:
        BtlChar_SetHeldFlag(chr, 3);
        break;
    case 4:
    case 5:
    case 6:
        BtlChar_SetHeldFlag(chr, 4);
        break;
    }
    BtlChar_UpdateLockOn(chr);
    if (BtlChars_IsTimeStopped()) {
        BtlChar_SetHeldFlag(chr, 0x135);
    } else {
        BtlChar_ClearFlag(chr, 0x135);
    }
    if (BtlChar_IsFlagDropped(chr, 0x135)) {
        BtlChar_ResetObjWork(chr);
    }
    if (!BtlChar_TestFlag(chr, 0xAF)) {
        BtlChar_ClearFlag(chr, 0x80);
    }
}

/* Phase 4 body 1: seven BtlChar_PlaceRestart.. passes, then pose -> object, object update, object -> pose. */
void BtlChar_UpdateMotion(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    BtlMove_UpdateAction(chr);
    if (BtlChar_TestFlag(chr, 0x2B)) {
        BtlObj_SaveNodePositions(obj, 1);
    } else {
        BtlObj_SaveNodePositions(obj, 0);
    }
    BtlChar_PlaceRestart(chr);
    BtlChar_PlaceCenter(chr);
    BtlChar_PlaceRelative(chr);
    BtlChar_PlaceOnPath(chr);
    BtlChar_PlaceCenterHigh(chr);
    BtlChar_PlaceSaved(chr);
    BtlChar_PlaceWarp(chr);
    BtlChar_PoseToObj(chr, 1);
    BtlObjAnim_SamplePose(obj);
    BtlObjAnim_UpdateEvents(obj);
    BtlChar_UpdateLean(chr);
    BtlFx_UpdateObjEvents(chr);
    BtlObjPose_CalcMatrices(obj);
    BtlChar_ObjToPose(chr);
    BtlChar_UpdateLookOffset(chr);
}

/* Phase 4 body 2 (stage 6). */
void BtlChar_UpdateStage6(BtlMgrChr *chr) {
    if (!BtlChar_IsFrozen(chr)) {
        BtlChar_SetStage(chr, BTL_CHR_STAGE_6);
        BtlOpp_MirrorFlags(chr);
        BtlPartner_Update(chr);
    }
}

/* Phase 4 body 3 (stage 7). */
void BtlChar_UpdateStage7(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (!BtlChar_IsFrozen(chr)) {
        BtlChar_SetStage(chr, BTL_CHR_STAGE_7);
        BtlMove_PushOut(chr);
        BtlMove_ApplyOrbit(chr);
        BtlChar_PoseToObj(chr, 0);
        BtlObjPose_CalcMatrices(obj);
        BtlChar_ObjToPose(chr);
    }
}

/* Phase 4 body 4 (stage 8). */
void BtlChar_UpdateStage8(BtlMgrChr *chr) {
    BtlMgrObj *obj;
    s32 flag = 1;
    s32 started;

    obj = BtlChar_GetObj(chr);
    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    BtlChar_SetStage(chr, BTL_CHR_STAGE_8);
    BtlMove_ClampToStage(chr);
    BtlChar_PoseToObj(chr, 0);
    BtlObjPose_CalcMatrices(obj);
    if (BtlChar_TestFlag(chr, 0x55)) {
        BtlObjBody_Warp(obj, chr->unk1310);
        flag = 0;
    }
    started = 0;
    if (gBtlChars->flags & BTL_CHARS_STARTED) {
        started = 1;
    }
    if (BtlChars_IsTimeStopped()) {
        started = 0;
    }
    StgCol_UpdateFighter(obj, flag, BtlCharApi_IsChanging(chr->objId) ? 0 : started);
    StgGround_UpdateFighter(obj);
    BtlChar_ObjToPose(chr);
}

/* Phase 4 body 5 (stage 9). */
void BtlChar_UpdateStage9(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (!BtlChar_IsFrozen(chr)) {
        BtlChar_SetStage(chr, BTL_CHR_STAGE_9);
        BtlColl_UpdateGround(chr);
        BtlChar_PoseToObj(chr, 0);
        BtlObjPose_CalcMatrices(obj);
    }
}

/* Phase 4 body 6: the fighter's own camera, then the object's final placement. */
void BtlChar_UpdateCamera(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (!BtlChar_IsFrozen(chr)) {
        ChrCam_StartCut(chr);
        ChrCam_UpdateDemo(chr);
        ChrCam_UpdateInput(chr);
        BtlObj_SetMoveVec(obj, BtlChar_GetPos(chr)->unk30);
        BtlObj_UpdateChains(obj);
        BtlChar_UpdateHead(chr);
        BtlObjPose_CalcMatrices(obj);
        BtlFx_UpdateAll(chr);
        BtlObjHit_BuildVolumes(obj);
    }
}

/* Phase 4 body 7 (stage 10). */
void BtlChar_UpdateStage10(BtlMgrChr *chr) {
    if (!BtlChar_IsFrozen(chr)) {
        BtlChar_SetStage(chr, BTL_CHR_STAGE_10);
        BtlPartner_UpdateEvents(chr);
        BtlFx_UpdateAfterHits(chr);
    }
}

/* Phase 5 body: after the effect scene: low-health state and the vector pushed onto the opponent. */
void BtlChar_PostScene(BtlMgrChr *chr) {
    BtlMgrChr *opp;

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    BtlChar_ObjToPose(chr);
    ChrCam_Update(chr);
    BtlFx_UpdatePostScene(chr);
    if (BtlMember_GetActiveGauge(chr)->health < 10000) {
        BtlMember_GetActiveGauge(chr)->lowHealth = 1;
        if (!(BtlParam_GetUnk0(chr) & 0x80) && chr->unkE14 <= 0 && chr->unkE18 <= 0 && !BtlChar_TestFlag(chr, 6)) {
            BtlMember_GetActiveGauge(chr)->lowHealthIdle = 1;
        } else {
            BtlMember_GetActiveGauge(chr)->lowHealthIdle = 0;
        }
    } else {
        BtlMember_GetActiveGauge(chr)->lowHealthIdle = 0;
        BtlMember_GetActiveGauge(chr)->lowHealth = 0;
    }
    opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
    /* In a multi-fighter battle, unrelated actors can target this thrower.
       Only the reciprocal participant owns its paired animation delta. */
    if(TagTeam_Active() && TagTeam_Target(opp->side)!=chr->side) return;
    if (BtlChar_TestFlag(chr, 0x97)) {
        if (!BtlChar_TestFlag(chr, 0x30)) {
            BtlChar_GetSnapDelta(chr, opp->unkF30, 1, 4);
        } else {
            Vec4_SetZero(opp->unkF30);
        }
    } else {
        Vec4_SetZero(opp->unkF30);
    }
}

/* Phase 6 body 1: late per-fighter updates and the status-table penalty while gauge.unk30 is on. */
void BtlChar_UpdateLate(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    BtlFx_UpdateLate(chr);
    BtlChar_StopVoiceOnFlag(chr);
    BtlCharSnd_PlayTechSounds(chr);
    BtlMove_UpdateAnimVoice(chr);
    BtlStat_ClearFrameMods(chr);
    if (BtlChar_TestMemberUnk70(chr)) {
        OBJ_WORD_18028(obj) = 1;
        BtlStat_SetFrameMod(chr, 0, -10, 1);
        BtlStat_SetFrameMod(chr, 1, -10, 1);
        BtlStat_SetFrameMod(chr, 2, -10, 1);
        BtlStat_SetFrameMod(chr, 3, -10, 1);
    } else {
        OBJ_WORD_18028(obj) = 0;
    }
}

/* Phase 6 body 2 (stages 11, 12): final pose vectors and the frame's battle events. */
void BtlChar_EndFrame(BtlMgrChr *chr) {
    BtlMgrPose *pose = BtlChar_GetPos(chr);

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    BtlChar_SetStage(chr, BTL_CHR_STAGE_EVENTS);
    if (!BtlChar_TestFlag(chr, 0x24)) {
        BtlChar_GetSnapDelta(chr, pose->unk30, 0, 1);
        BtlChar_GetMoveSince(chr, pose->unk40, 0);
    } else {
        Vec4_SetZero(pose->unk30);
        Vec4_SetZero(pose->unk40);
    }
    BtlChar_RaiseEvents(chr);
    BtlChar_UpdateVibration(chr);
    BtlChar_SetStage(chr, BTL_CHR_STAGE_END);
}

/* Allocates the roster for `count` fighters and resets it. */
void BtlChar_AllocAll(s32 count) {
    gBtlChars = Heap_Alloc(sizeof(BtlCharMgr), 0x20, 0, HEAP_ANY);
    memset(gBtlChars, 0, sizeof(BtlCharMgr));
    gBtlChars->chars = Heap_Alloc(count * 0x1600, 0x20, 0, HEAP_ANY);
    memset(gBtlChars->chars, 0, count * 0x1600);
    gBtlChars->unk8 = Heap_Alloc(count * 0x34, 0x20, 0, HEAP_ANY);
    memset(gBtlChars->unk8, 0, count * 0x34);
    gBtlChars->unkC = Heap_Alloc(count * 0x34, 0x20, 0, HEAP_ANY);
    memset(gBtlChars->unkC, 0, count * 0x34);
    gBtlChars->count = count;
    gBtlChars->tbl[0] = FILE_TABLE(gCommonRes->data[0], 3);
    gBtlChars->tbl[1] = FILE_TABLE(gCommonRes->data[0], 5);
    gBtlChars->tbl[2] = FILE_TABLE(gCommonRes->data[0], 6);
    gBtlChars->tbl[3] = FILE_TABLE(gCommonRes->data[0], 7);
    gBtlChars->tbl[4] = FILE_TABLE(gCommonRes->data[0], 8);
    gBtlChars->tbl[5] = FILE_TABLE(gCommonRes->data[0], 9);
    BtlChar_ResetAll();
    BtlCharSnd_Init();
}

/* Frees the roster. */
void BtlChar_FreeAll(void) {
    Heap_Free(gBtlChars->unkC);
    Heap_Free(gBtlChars->unk8);
    Heap_Free(gBtlChars->chars);
    Heap_Free(gBtlChars);
    gBtlChars = NULL;
}

/* Zeroes the manager's state and every fighter, then sets each fighter up from the battle setup. */
void BtlChar_ResetAll(void) {
    s32 i;
    BtlMgrChr *chr;
    s32 pad;

    /* Ready/Fight resets happen after the stage and all body sizes are live. */
    { extern void TagSpawn_Reset(void); TagSpawn_Reset(); }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlPartner_Release(BtlChar_Get(i));
    }
    gBtlChars->frame = 0;
    gBtlChars->unk18 = 0;
    gBtlChars->flags = 0;
    gBtlChars->unk1C = 0;
    memset(gBtlChars->unkA0, 0, 0x90);
    memset(gBtlChars->unk138, 0, 0x140);
    memset(gBtlChars->unk40, 0, 0x60);
    gBtlChars->unk130 = 0;
    memset(gBtlChars->chars, 0, BtlChar_GetCount() * 0x1600);
    BtlClash_Reset();
    BtlChange_Reset();
    BtlReplay_ResetViewer();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        chr->side = i;
        pad = BattleSide_GetPad(i);
        chr->index = i;
        chr->pad = pad;
        BtlChar_BindObject(chr);
        BtlChar_Reset(chr);
    }
}

/* Loader callback: a side's new character model is ready. */
void BtlChars_OnModelLoaded(s32 side) {
    BtlMgrChr *chr = BtlChar_Get(side);

    if (chr != NULL) {
        BtlChar_OnModelLoaded(chr);
    }
}

/* Loader callback: a new stage is ready. */
void BtlChars_OnStageLoaded(void) {
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_OnStageLoaded(BtlChar_Get(i));
    }
}

/* Phase 2: samples the pads into every fighter's input record. */
void BtlChars_SampleInput(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_SampleInput(BtlChar_Get(i));
    }
}

/* Phase 1: resets the roster the first frame the sequence reaches Ready or Fight; returns 1 when it did. */
s32 BtlChars_CheckStart(void) {
    s32 ret = 0;

    if (BtlSeq_GetState() == 2 || BtlSeq_GetState() == 3) {
        if (!(gBtlChars->flags & BTL_CHARS_STARTED)) {
            if (Battle_GetMode() != 1) {
                BtlChar_ResetAll();
                ret = 1;
            }
        }
        gBtlChars->flags |= BTL_CHARS_STARTED;
    }
    return ret;
}

/* Phase 3: frame counter, round reset, hit-stop, then input and per-frame clears for every fighter. */
void BtlChars_UpdateInput(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        BtlChars_ClearObjFlag2();
        return;
    }
    if ((gBtlChars->flags & BTL_CHARS_STARTED) && !BtlChars_IsTimeStopped()) {
        gBtlChars->frame++;
        gBtlChars->frame &= 0x3FFFFFFF;
        gBtlChars->unk1C = BtlUtil_Max(gBtlChars->unk1C - 1, 0);
    }
    BtlChars_CheckRoundReset();
    BtlChars_UpdateFreeze();
    BtlCharSnd_ClearRequests();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_BeginFrame(BtlChar_Get(i));
    }
    BtlChars_Snapshot(0);
    BtlChars_UpdateSightFlag();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateSeqFlags(BtlChar_Get(i));
    }
}

/* Phase 4: the simulation proper, as seven passes over the roster. */
void BtlChars_UpdateMain(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateMotion(BtlChar_Get(i));
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage6(BtlChar_Get(i));
    }
    BtlChars_UpdateHold();
    BtlChars_Snapshot(1);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage7(BtlChar_Get(i));
    }
    BtlChars_Snapshot(2);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage8(BtlChar_Get(i));
    }
    BtlChars_Snapshot(3);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage9(BtlChar_Get(i));
    }
    BtlChars_Snapshot(4);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateCamera(BtlChar_Get(i));
    }
    BtlReplay_UpdateViewer();
    BtlFx_UpdateRoster();
    BtlMembers_UpdateQueuedDamage();
    BtlChars_UpdateCollision();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage10(BtlChar_Get(i));
    }
    BtlClash_Update();
}

/* Phase 5: per-fighter pass after the effect scene. */
void BtlChars_PostScene(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_PostScene(BtlChar_Get(i));
    }
}

/* Phase 6: late updates, object flags, queued requests and the frame's battle events. */
void BtlChars_EndFrame(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    BtlChange_Update();
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateLate(BtlChar_Get(i));
    }
    BtlChars_UpdateObjFlag2();
    BtlCharSnd_PlayRequests();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_EndFrame(BtlChar_Get(i));
    }
    BtlChars_Snapshot(5);
}
