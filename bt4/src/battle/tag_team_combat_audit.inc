#include <stdio.h>
#include "battle/tag_team_native.h"
static s32 sTagAuditChecks,sTagAuditFailures;
typedef struct TagAuditVec {f32 x,y,z,w;} TagAuditVec;
static void TagAudit_Check(char *name,s32 ok){
    ++sTagAuditChecks;if(!ok)++sTagAuditFailures;
    printf("tag combat audit: %s %s\n",name,ok?"PASS":"FAIL");
}
static void TagAudit_Clear(void){s32 i;
    for(i=0;i<BtlChar_GetCount();++i){BtlMgrChr *c=BtlChar_Get(i);
        BtlChar_ClearFlagRange(c,0,0x13F);c->freeze=c->freezeNext=c->freezeDelay=0;
        BtlChar_SetHeldFlag(c,3);BtlChar_SetHeldFlag(c,5);
    }
}
static s32 TagAudit_DefaultCuts(void){
    extern void BtlSuper_RequestHitStop(BtlMgrChr *,s32);
    extern s32 BtlScene_IsEffectStopped(s32,s32);
    s32 i,other=2;
    sTagAuditChecks=sTagAuditFailures=0;
    TagAudit_Clear();TagBattle_ResetPairs();
    for(i=0;i<BtlChar_GetCount();++i)BtlChar_Get(i)->action=0;
    BtlChar_Get(0)->action=0x105;
    BtlSuper_RequestHitStop(BtlChar_Get(0),0);BtlChars_UpdateFreeze();
    TagAudit_Check("startup_owner_advances",!BtlChar_Get(0)->freeze);
    for(i=1;i<BtlChar_GetCount();++i)TagAudit_Check("startup_does_not_freeze_outsiders",!BtlChar_Get(i)->freeze);
    for(i=1;i<BtlChar_GetCount();++i)TagAudit_Check("startup_does_not_stop_outsider_effects",!BtlScene_IsEffectStopped(BtlChar_Get(i)->objId,0));
    BtlChar_Get(other)->action=0x105;BtlChar_SetHeldFlag(BtlChar_Get(other),0x125);
    BtlChars_UpdateFreeze();
    TagAudit_Check("simultaneous_startups_both_advance",!BtlChar_Get(0)->freeze&&!BtlChar_Get(other)->freeze);
    TagAudit_Clear();for(i=0;i<BtlChar_GetCount();++i)BtlChar_Get(i)->action=0;
    BtlChars_UpdateFreeze();
    TagAudit_Check("cinematic_stop_releases_without_a_timer",TagBattle_DefaultCutOwner()<0);
    for(i=0;i<BtlChar_GetCount();++i)TagAudit_Check("all_actors_can_resume",!BtlChar_Get(i)->freeze);
    BtlChar_Get(0)->action=0xB7;BtlChar_SetHeldFlag(BtlChar_Get(0),0x125);BtlChars_UpdateFreeze();
    TagAudit_Check("throw_does_not_stop_an_unrelated_actor",TagBattle_DefaultCutOwner()<0 && !BtlChar_Get(other)->freeze);
    {extern s32 BtlCharApi_IsCamShown(s32);s32 actions[3]={0x130,0xFA,0xFC};
     for(i=0;i<3;++i){BtlChar_Get(0)->action=actions[i];TagAudit_Check("clash_keeps_external_effect_framing",!BtlCharApi_IsCamShown(BtlChar_Get(0)->objId));}}
    printf("tag combat audit: checks=%d failures=%d\n",sTagAuditChecks,sTagAuditFailures);return sTagAuditFailures;
}
static s32 TagAudit_RushCuts(void){
    extern void BtlSuper_RequestHitStop(BtlMgrChr *,s32),BtlAct_SetQueue(BtlMgrChr *,u32,s32);
    s32 i,k,actor=0,victim=1,phases[3]={0x11C,0x12E,0x12B};
    sTagAuditChecks=sTagAuditFailures=0;
    for(k=0;k<3;++k){
        TagAudit_Clear();TagBattle_ResetPairs();
        for(i=0;i<BtlChar_GetCount();++i)BtlChar_Get(i)->action=0xB;
        BtlChar_Get(actor)->action=phases[k];
        if(k){TagBattle_ReservePair(actor,victim);BtlChar_Get(victim)->action=0x13A;}
        BtlSuper_RequestHitStop(BtlChar_Get(actor),0);BtlChars_UpdateFreeze();
        TagAudit_Check("rush_never_requests_global_owner",TagBattle_DefaultCutOwner()<0);
        TagAudit_Check("attacker_sees_own_camera",TagBattle_CutVisible(actor,actor));
        TagAudit_Check("victim_camera_only_after_contact",TagBattle_CutVisible(actor,victim)==(k!=0));
        for(i=2;i<BtlChar_GetCount();++i){
            TagAudit_Check("bystander_camera_is_private",!TagBattle_CutVisible(actor,i));
            TagAudit_Check("bystander_keeps_simulating",!BtlChar_Get(i)->freeze);
        }
        if(k)TagAudit_Check("victim_cannot_replace_attacker_camera",!TagBattle_CutVisible(victim,actor));
    }
    TagAudit_Clear();TagBattle_ResetPairs();
    for(i=0;i<BtlChar_GetCount();++i)BtlChar_Get(i)->action=0xB;
    BtlAct_SetQueue(BtlChar_Get(actor),0,0x11D);BtlChar_Get(actor)->action=0x105;
    BtlSuper_RequestHitStop(BtlChar_Get(actor),0);BtlChars_UpdateFreeze();
    TagAudit_Check("ultimate_rush_prelude_is_private",TagBattle_RushAction(actor)&&TagBattle_DefaultCutOwner()<0&&!TagBattle_CutVisible(actor,victim));
    for(i=1;i<BtlChar_GetCount();++i)TagAudit_Check("ultimate_rush_prelude_keeps_others_active",!BtlChar_Get(i)->freeze);
    printf("tag rush audit: checks=%d failures=%d\n",sTagAuditChecks,sTagAuditFailures);return sTagAuditFailures;
}
#include "tag_team_camera_audit.inc"
static s32 TagAudit_WarpPrediction(void){
    extern void BtlOpp_GetPoseVec30(void *,void *),BtlOpp_GetVelocity(void *,void *);
    extern void BtlMove_WarpBehindOpponent(void *,f32);
    s32 target=TagBattle_Target(0);BtlMgrChr *c=BtlChar_Get(target);TagAuditVec velocity={80,0,80,0},got,before,after;
    sTagAuditChecks=sTagAuditFailures=0;TagAudit_Clear();
    memcpy(BtlChar_GetPos(c)->unk30,&velocity,sizeof(velocity));memcpy(BtlChar_GetPos(c)->unk40,&velocity,sizeof(velocity));
    c->freeze=2;BtlOpp_GetPoseVec30(BtlChar_Get(0),&got);
    TagAudit_Check("held_target_has_no_predicted_delta",got.x==0&&got.y==0&&got.z==0);
    BtlOpp_GetVelocity(BtlChar_Get(0),&got);TagAudit_Check("held_target_has_no_predicted_velocity",got.x==0&&got.y==0&&got.z==0);
    BtlCharApi_GetBasePos(c->objId,&before);BtlMove_WarpBehindOpponent(BtlChar_Get(0),0.f);
    memcpy(&after,BtlChar_GetPos(BtlChar_Get(0)),sizeof(after));
    TagAudit_Check("back_kick_warp_stays_next_to_held_victim",(after.x-before.x)*(after.x-before.x)+(after.z-before.z)*(after.z-before.z)<900.f);
    c->freeze=0;BtlOpp_GetPoseVec30(BtlChar_Get(0),&got);TagAudit_Check("moving_target_keeps_original_prediction",got.x==80&&got.z==80);
    printf("tag warp audit: checks=%d failures=%d\n",sTagAuditChecks,sTagAuditFailures);return sTagAuditFailures;
}
s32 TagBattle_RunCombatAudit(void){
    extern void BtlSuper_RequestHitStop(BtlMgrChr *,s32),BtlAct_UpdateTimers(BtlMgrChr *),BtlAct_UpdateGauges(BtlMgrChr *);
    extern s32 BtlAnim_GetFlags(s32),BtlSeq_CheckBattleEnd(void);
    extern void BtlCharApi_GetNodePos(s32,s32,void *);
    s32 i,teamA[10],teamB[10],na=0,nb=0,n=BtlChar_GetCount();TagAuditVec a,b;f32 d;
    sTagAuditChecks=sTagAuditFailures=0;
    if(getenv("BT4_TAG_AUDIO_AUDIT")){extern s32 BtlCharSnd_RunAudit(void);return BtlCharSnd_RunAudit();}
    if(getenv("BT4_TAG_WARP_AUDIT"))return TagAudit_WarpPrediction();
    if(getenv("BT4_TAG_CAMERA_AUDIT"))return TagAudit_CameraPolicy();
    if(getenv("BT4_TAG_RUSH_AUDIT"))return TagAudit_RushCuts();
    if(getenv("BT4_TAG_DEFAULT_AUDIT"))return TagAudit_DefaultCuts();
    TagAudit_Check("ten_native_actors",n==10);
    if(n!=10)return 1;
    for(i=0;i<n;++i)if(TagBattle_Team(i)==0)teamA[na++]=i;else teamB[nb++]=i;
    if(na<3||nb<2)return 1;
    TagAudit_Clear();TagBattle_ResetPairs();
    BtlSuper_RequestHitStop(BtlChar_Get(0),0);BtlSuper_RequestHitStop(BtlChar_Get(2),0);
    BtlChars_UpdateFreeze();
    for(i=0;i<n;++i)TagAudit_Check("attack_startup_does_not_freeze_roster",BtlChar_Get(i)->freeze==0);
    TagAudit_Clear();
    BtlChar_SetHeldFlag(BtlChar_Get(0),0x125);BtlChar_SetHeldFlag(BtlChar_Get(2),0x125);
    BtlChars_UpdateFreeze();
    TagAudit_Check("concurrent_cinematic_owners_advance",!BtlChar_Get(0)->freeze && !BtlChar_Get(2)->freeze);
    TagAudit_Check("unrelated_actor_not_frozen",!BtlChar_Get(4)->freeze);
    TagAudit_Clear();BtlChar_SetHeldFlag(BtlChar_Get(0),0x12A);BtlChars_UpdateObjFlag2();
    for(i=0;i<n;++i)TagAudit_Check("cinematic_owner_does_not_hide_roster",(BtlChar_GetObj(BtlChar_Get(i))->flags&2)!=0);
    TagAudit_Clear();TagBattle_ResetPairs();
    TagAudit_Check("reserve_actual_attacker_and_victim",TagBattle_ReservePair(0,teamB[1]));
    TagAudit_Check("victim_tracks_actual_attacker",TagBattle_Target(teamB[1])==0 && TagBattle_Target(0)==teamB[1]);
    TagAudit_Check("third_actor_cannot_steal_pair",!TagBattle_ReservePair(1,0));
    TagBattle_CycleTarget(teamB[1]);TagAudit_Check("target_switch_does_not_break_cinematic_pair",TagBattle_Target(teamB[1])==0);
    TagBattle_ResetPairs();TagBattle_ReservePair(teamA[0],teamB[0]);TagBattle_ReservePair(teamA[1],teamB[1]);
    for(i=0;i<2;++i){BtlChar_SetHeldFlag(BtlChar_Get(teamA[i]),0x8B);BtlChar_SetHeldFlag(BtlChar_Get(teamB[i]),0x97);}
    BtlChars_UpdateHold();
    for(i=0;i<2;++i){BtlMgrObj *obj=BtlChar_GetObj(BtlChar_Get(teamB[i]));
        s32 node=*(s32 *)((u8 *)obj+0xCB4);
        BtlCharApi_GetNodePos(BtlChar_Get(teamA[i])->objId,0x34,&a);
        BtlCharApi_GetNodePos(BtlChar_Get(teamB[i])->objId,node,&b);
        d=(a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);
        TagAudit_Check("simultaneous_grabs_keep_own_victim",d<0.1f);
    }
    TagAudit_Clear();TagBattle_ResetPairs();
    BtlAnim_Play(BtlChar_Get(0),0,0);BtlChar_Get(0)->action=0xD3;
    *(s32 *)((u8 *)BtlChar_Get(0)+0xFE0)=5;
    for(i=0;i<5;++i)BtlAct_UpdateTimers(BtlChar_Get(0));
    TagAudit_Check("exhaustion_has_finite_recovery",*(s32 *)((u8 *)BtlChar_Get(0)+0xFE0)==0);
    TagAudit_Clear();BtlChar_Get(0)->action=0xEA;BtlMember_GetActiveGauge(BtlChar_Get(0))->ki=0;
    BtlChar_SetHeldFlag(BtlChar_Get(0),0xBE);
    for(i=0;i<6000 && BtlChar_TestFlag(BtlChar_Get(0),0xBE);++i)BtlAct_UpdateGauges(BtlChar_Get(0));
    TagAudit_Check("guard_break_recovers_with_original_ki_rate",!BtlChar_TestFlag(BtlChar_Get(0),0xBE) && BtlMember_GetActiveGauge(BtlChar_Get(0))->ki>0);
    TagAudit_Clear();BtlChar_Get(0)->action=0xB;BtlMember_GetActiveGauge(BtlChar_Get(0))->health=0;
    TagBattle_UpdateSpectator(0);TagAudit_Check("ko_enters_teammate_spectator",TagBattle_Spectating() && TagBattle_ViewActor()==teamA[1]);
    TagBattle_UpdateSpectator(1);TagAudit_Check("spectator_next_living_teammate",TagBattle_ViewActor()==teamA[2]);
    TagBattle_UpdateSpectator(-1);TagAudit_Check("spectator_previous_living_teammate",TagBattle_ViewActor()==teamA[1]);
    BattleResult_Clear();TagAudit_Check("individual_ko_does_not_end_team_match",!BtlSeq_CheckBattleEnd());
    for(i=1;i<n;++i){
        BtlMgrChr *c=BtlChar_Get(i);
        if(BtlCharApi_GetHp(TagBattle_Target(i))<=0)TagBattle_CycleTarget(i);
        BtlOpp_MirrorFlags(c);
        TagAudit_Check("survivors_use_live_target_after_leader_ko",!BtlChar_TestFlag(c,0xB1));
    }
    for(i=0;i<n;++i)if(TagBattle_Team(i)==0)BtlMember_GetActiveGauge(BtlChar_Get(i))->health=0;
    TagAudit_Check("whole_team_ko_ends_match",BtlSeq_CheckBattleEnd() && Battle_GetResult()->winner==BTL_RESULT_WIN_P2);
    printf("tag combat audit: checks=%d failures=%d\n",sTagAuditChecks,sTagAuditFailures);
    return sTagAuditFailures;
}
