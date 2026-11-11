/* Exercises the same camera, freeze and damage paths used in a real battle. */
static s32 TagAudit_CameraPolicy(void){
    extern void BtlSuper_RequestHitStop(BtlMgrChr *,s32),BtlMembers_UpdateQueuedDamage(void);
    s32 i,k,hp[10],grabTarget;
    sTagAuditChecks=sTagAuditFailures=0;
    TagAudit_Clear();TagBattle_ResetPairs();
    for(i=0;i<BtlChar_GetCount();++i)BtlChar_Get(i)->action=0xB;
    grabTarget=TagBattle_Target(0);BtlChar_Get(0)->action=0xB4;
    TagBattle_CycleTarget(0);
    TagAudit_Check("grab_approach_keeps_original_target",TagBattle_Target(0)==grabTarget&&!TagBattle_SetTarget(0,3));
    TagBattle_ReservePair(0,1);
    for(k=0;k<18;++k)TagBattle_UpdatePairs();
    TagAudit_Check("grab_approach_retains_pair_until_contact",TagBattle_Pair(0)==1&&TagBattle_Pair(1)==0);
    BtlChar_Get(0)->action=0xB7;BtlChar_Get(1)->action=0xBF;
    for(k=0;k<18;++k)TagBattle_UpdatePairs();
    TagAudit_Check("grab_contact_retains_actual_victim",TagBattle_Pair(0)==1);
    BtlChar_Get(0)->action=0xB;
    for(k=0;k<7;++k)TagBattle_UpdatePairs();
    TagAudit_Check("orphaned_victim_does_not_keep_pair_locked",TagBattle_Pair(0)<0&&TagBattle_Pair(1)<0);
    TagAudit_Clear();TagBattle_ResetPairs();
    for(i=0;i<BtlChar_GetCount();++i)BtlChar_Get(i)->action=0xB;
    BtlChar_Get(0)->action=0x105;BtlSuper_RequestHitStop(BtlChar_Get(0),0);BtlChars_UpdateFreeze();
    TagAudit_Check("shared_preparation_owner",TagBattle_DefaultCutOwner()==0);
    for(i=0;i<BtlChar_GetCount();++i){
        TagAudit_Check("preparation_camera_remains_private",TagBattle_CameraActor(i)==i);
        TagAudit_Check("preparation_keeps_outsiders_active",!BtlChar_Get(i)->freeze);
    }
    TagAudit_Clear();TagBattle_ReservePair(0,1);
    BtlChar_Get(0)->action=0x12E;BtlChar_Get(1)->action=0x13A;BtlChars_UpdateFreeze();
    TagAudit_Check("impact_has_no_global_owner",TagBattle_DefaultCutOwner()<0);
    TagAudit_Check("attacker_and_victim_share_attacker_camera",TagBattle_CameraActor(0)==0&&TagBattle_CameraActor(1)==0);
    TagAudit_Check("victim_cannot_replace_attacker_camera",!TagBattle_CutVisible(1,0)&&!TagBattle_CutVisible(1,1));
    for(i=2;i<BtlChar_GetCount();++i){
        TagAudit_Check("outsider_keeps_own_camera",TagBattle_CameraActor(i)==i);
        TagAudit_Check("outsider_keeps_control",!BtlChar_Get(i)->freeze);
    }
    BtlChar_Get(2)->action=0x105;BtlSuper_RequestHitStop(BtlChar_Get(2),0);BtlChars_UpdateFreeze();
    TagAudit_Check("second_preparation_preserves_existing_pair",TagBattle_CameraActor(0)==0&&TagBattle_CameraActor(1)==0&&!BtlChar_Get(0)->freeze&&!BtlChar_Get(1)->freeze);
    TagAudit_Check("second_preparation_keeps_free_fighters_independent",TagBattle_CameraActor(3)==3&&!BtlChar_Get(3)->freeze);
    TagAudit_Clear();for(i=0;i<BtlChar_GetCount();++i)BtlChar_Get(i)->action=0xB;
    for(k=0;k<7;++k)TagBattle_UpdatePairs();BtlChars_UpdateFreeze();
    TagAudit_Check("released_pair_and_camera",TagBattle_Pair(0)<0&&TagBattle_CameraActor(1)==1);
    TagAudit_Check("damage_destination_survives_pair_release",TagBattle_DamageTarget(0)==1);
    TagBattle_SetTarget(0,3);
    for(i=0;i<BtlChar_GetCount();++i)hp[i]=BtlMember_GetActiveGauge(BtlChar_Get(i))->health;
    /* BtlMemberQueue is the shared documented fighter + 0xD94 block. */
    {s32 *q=(s32 *)((u8 *)BtlChar_Get(0)+0xD94);q[1]=500;q[4]=0x80;}
    BtlChar_SetHeldFlag(BtlChar_Get(0),0x8C);BtlMembers_UpdateQueuedDamage();
    TagAudit_Check("retarget_cannot_redirect_pending_damage",BtlMember_GetActiveGauge(BtlChar_Get(1))->health<hp[1]);
    for(i=0;i<BtlChar_GetCount();++i)if(i!=1)TagAudit_Check("pending_damage_never_hits_other_actors",BtlMember_GetActiveGauge(BtlChar_Get(i))->health==hp[i]);
    TagAudit_Check("damage_lock_cleared_after_flush",TagBattle_DamageTarget(0)<0);
    BtlChar_Get(2)->action=0x108;
    TagAudit_Check("projectile_impact_keeps_victim_view",TagBattle_CameraActor(3)==3&&TagBattle_CameraActor(0)==0&&TagBattle_Pair(3)<0);
    BtlChar_Get(2)->action=0xB;TagBattle_UpdatePairs();
    TagAudit_Check("projectile_camera_released",TagBattle_CameraActor(3)==3);
    printf("tag camera audit: checks=%d failures=%d\n",sTagAuditChecks,sTagAuditFailures);return sTagAuditFailures;
}
