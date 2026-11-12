/* Opt-in regression checks run by the headless harness after a real battle has loaded. */
#include "common.h"
#include "battle/btl_input.h"
#include <stdio.h>
extern u8 *BtlChar_Get(s32 side);
extern u8 *BtlChar_GetObj(void *chr);
extern u8 *BtlMember_GetActive(void *chr);
extern s32 *BtlMember_GetActiveGauge(void *chr);
extern s32 BtlParam_GetSlotCost(void *chr, s32 slot);
extern s32 BtlParam_GetSlotA8(void *chr, s32 slot);
extern s32 BtlAct_CanTransform(void *chr, s32 slot, s32 need, s32 allowed);
extern s32 BtlAct_QueueTransform(void *chr, s32 slot);
extern s32 BattleSide_IsCharaUsable(s32 side, s32 chara);
extern void BtlChar_SetFlag(void *chr, s32 bit);
extern void BtlChar_ClearFlag(void *chr, s32 bit);
extern s32 BtlChar_TestFlag(void *chr, s32 bit);
extern void BtlMove_UpdateDefenseTimers(void *chr);
extern void Bt4_SpendTransformKi(void *chr, s32 amount);
extern void BtlAct_Action39(void *chr, s32 phase);
extern void BtlAct_Thrown(void *chr, s32 phase);
extern void BtlAct_CheckForced(void *chr);
extern void BtlAct_SetQueue(void *chr, s32 slot, s32 action);
extern s32 StgGrid_IsSelectable(s32 *ids, s32 index);
extern void StgGrid_MoveRightWrap(s32 *ids, s32 *col, s32 row);
extern void BtlHit_TestHit(void *chr);
extern u32 *BtlAtk_GetRecord(void *chr, s32 *id);
extern s32 BtlCtrl_GetChangePrompt(s32 side,s32 *button,s32 *kind,s32 *padStatus);
extern s32 BtlCharApi_GetButtonIcon(void *chr,s32 button);
extern void BtlAct_AscendHandler(void *chr,s32 phase), BtlAct_DescendHandler(void *chr,s32 phase);
extern s32 BtlAnim_GetId(void *chr), BtlStage_GetChangeTarget(void);
extern u8 *gBtlStage;
static s32 failures, checks;
static void check(const char *name, s32 ok, s32 value) {
    printf("bt4-rule: %s %s value=%d\n",ok?"PASS":"FAIL",name,value);
    ++checks;if (!ok)++failures;
}
s32 Bt4_RunRulesRegression(void) {
    u8 *c=BtlChar_Get(0), *member=BtlMember_GetActive(c);
    s32 *g=BtlMember_GetActiveGauge(c);
    u8 *param=*(u8 **)(BtlChar_GetObj(c)+0x91C);
    s32 slot,eligible=0;
    failures=checks=0;
    for(slot=0;slot<4;++slot) {
        s32 expected=param[0x9C+slot]*20000;
        check("transformation_ki_cost",BtlParam_GetSlotCost(c,slot)==expected,BtlParam_GetSlotCost(c,slot));
        check("signed_extended_form_record",BtlParam_GetSlotA8(c,slot)==*(s16 *)(param+0xCC+slot*2),BtlParam_GetSlotA8(c,slot));
        if(param[0x98+slot]==0xFF) check("undefined_form_rejected",!BtlAct_CanTransform(c,slot,0,0),slot);
        if(param[0x98+slot]!=0xFF && param[0xA4+slot]!=3) {
            s32 cost=BtlParam_GetSlotCost(c,slot);
            g[3]=100000;g[5]=0;*(s32 *)(c+0x1300)=1;
            BtlChar_ClearFlag(c,0xAF);
            check("transform_with_ki_and_zero_blast",BtlAct_CanTransform(c,slot,1,1),param[0x98+slot]);
            if(cost>0) {g[3]=cost-1;check("transform_insufficient_ki",!BtlAct_CanTransform(c,slot,1,1),g[3]);}
            g[3]=100000;
            BtlAct_QueueTransform(c,slot);
            check("mapped_target_costume",*(s32 *)(c+0x12F4)==param[0xD4+slot*4+*(s32 *)(member+4)],*(s32 *)(c+0x12F4));
            ++eligible;
        }
    }
    printf("bt4-rule: INFO eligible_transformations=%d\n",eligible);
    check("extended_roster_target",BattleSide_IsCharaUsable(0,249),249);
    BtlChar_ClearFlag(c,6);g[3]=80000;g[5]=300000;
    Bt4_SpendTransformKi(c,20000);
    check("transform_drains_ki",g[3]==60000,g[3]);
    check("transform_keeps_blast",g[5]==300000,g[5]);
    BtlChar_SetFlag(c,6);g[7]=30000;
    Bt4_SpendTransformKi(c,20000);
    check("powered_transform_fraction",g[7]==24000,g[7]);
    BtlChar_ClearFlag(c,6);
    {
        BtlInputChr *in=(BtlInputChr *)c;
        BtlChar_SetFlag(c,5);BtlChar_ClearFlag(c,0x12C);BtlChar_ClearFlag(c,0x138);
        *(s32 *)(c+0x1068)=-30;*(s32 *)(c+0x106C)=-30;*(s32 *)(c+0x1070)=-30;
        in->input.cmdPressed=BTLC_GUARD_H;in->input.pressed=BTLB_UP|BTLB_RUSH;
        BtlMove_UpdateDefenseTimers(c);
        check("bt4_guard_window",*(s32 *)(c+0x106C)==1,*(s32 *)(c+0x106C));
        check("bt4_z_counter_window",*(s32 *)(c+0x1070)==3,*(s32 *)(c+0x1070));
        *(s32 *)(c+0x106C)=-30;BtlChar_SetFlag(c,0x138);
        BtlMove_UpdateDefenseTimers(c);
        check("bt4_enhanced_guard_window",*(s32 *)(c+0x106C)==2,*(s32 *)(c+0x106C));
        in->input.cmdPressed=0;in->input.pressed=0;
    }
    {
        s32 before=*(s32 *)(member+0x14);
        g[3]=0;g[4]=100000;
        BtlAct_Action39(c,0);
        check("z_counter_one_ki_bar_from_empty",g[3]==20000,g[3]);
        check("z_counter_reward",*(s32 *)(member+0x14)==before+1,*(s32 *)(member+0x14));
    }
    {
        static const s32 startKi[]={10000,80000,90000,100000};
        static const s32 expectedKi[]={30000,100000,100000,100000};
        s32 i;
        for(i=0;i<4;++i){
            g[3]=startKi[i];BtlAct_Action39(c,0);
            check("z_counter_additive_ki_capped",g[3]==expectedKi[i],g[3]);
        }
    }
    BtlChar_SetFlag(c,0x1D);BtlAct_Thrown(c,0);
    check("throw_recovery_unlocked",!BtlChar_TestFlag(c,0x1D),BtlChar_TestFlag(c,0x1D));
    {
        s32 ids[6]={0x24,0x25,0x26,0x27,0x62,0},col=2;
        check("reserved_map_38",!StgGrid_IsSelectable(ids,2),38);
        check("map_39_selectable",StgGrid_IsSelectable(ids,3),39);
        check("map_98_selectable",StgGrid_IsSelectable(ids,4),98);
        StgGrid_MoveRightWrap(ids,&col,0);
        check("cursor_reaches_custom_map",col==3,col);
    }
    {
        u8 *op=BtlChar_Get(1);
        BtlChar_ClearFlag(c,0x62);BtlChar_ClearFlag(op,0x62);
        BtlChar_SetFlag(c,0x7C);*(s32 *)(c+0xD88)=2;
        BtlAct_CheckForced(c);
        check("aerial_clash_requires_third_exchange",!BtlChar_TestFlag(c,0x62),BtlChar_TestFlag(c,0x62));
        BtlChar_SetFlag(c,0x7C);*(s32 *)(c+0xD88)=3;
        BtlAct_CheckForced(c);
        check("aerial_clash_after_third_exchange",BtlChar_TestFlag(c,0x62),BtlChar_TestFlag(c,0x62));
        check("aerial_clash_both_participants",BtlChar_TestFlag(op,0x62),BtlChar_TestFlag(op,0x62));
    }
    {
        u8 *data=*(u8 **)(gBtlStage+4), *info=*(u8 **)(data+0x14);
        s32 saved=*(s32 *)(info+0xC),i;
        static const s32 destinations[]={0,3,15,98,-1};
        for(i=0;i<5;++i){
            info[0xD]=(u8)destinations[i];
            check("bt4_stage_transition_destination",BtlStage_GetChangeTarget()==destinations[i],BtlStage_GetChangeTarget());
        }
        *(s32 *)(info+0xC)=saved;
        *(s32 *)(c+0xFF0)=15;
        BtlAct_AscendHandler(c,0);
        check("ascend_after_hit_has_no_recovery_startup",BtlAnim_GetId(c)==0x2B,BtlAnim_GetId(c));
        *(s32 *)(c+0xFF0)=15;
        BtlAct_DescendHandler(c,0);
        check("descend_after_hit_has_no_recovery_startup",BtlAnim_GetId(c)==0x2C,BtlAnim_GetId(c));
    }
    {
        u8 *op=BtlChar_Get(1);
        u32 *attack;
        s32 button=-1,kind=-1,padStatus=-1,bit;
        *(s32 *)(c+0x948)=0x70;
        attack=BtlAtk_GetRecord(c,NULL);
        attack[0]|=8; /* Test a frontal/omnidirectional live attack through the real hit resolver. */
        BtlChar_SetFlag(c,0x65);BtlChar_SetFlag(c,0xBA);
        BtlChar_SetFlag(op,0x76);
        BtlChar_ClearFlag(c,0x60);BtlChar_ClearFlag(c,0x5C);
        BtlHit_TestHit(c);
        check("vanish_exit_hit_processed",BtlChar_TestFlag(c,0x60),BtlChar_TestFlag(c,0x60));
        check("vanish_exit_attacker_not_stunned",!BtlChar_TestFlag(c,0x5C),BtlChar_TestFlag(c,0x5C));
        BtlChar_ClearFlag(op,0x76);BtlChar_ClearFlag(c,0x60);
        BtlHit_TestHit(c);
        check("ordinary_avoid_keeps_reaction",BtlChar_TestFlag(c,0x5C),BtlChar_TestFlag(c,0x5C));
        *(s32 *)(c+0x1278)=0;
        for(bit=0xE7;bit<=0xEC;++bit)BtlChar_ClearFlag(c,bit);
        BtlChar_SetFlag(c,0x13C);
        check("punch_clash_prompt_visible",BtlCtrl_GetChangePrompt(0,&button,&kind,&padStatus),button);
        check("punch_clash_uses_configured_button",button==BtlCharApi_GetButtonIcon(c,0x1E),button);
        check("punch_clash_prompt_style",kind==1,kind);
        BtlChar_ClearFlag(c,0x13C);BtlChar_SetFlag(c,0xE8);
        check("beam_clash_prompt_preserved",BtlCtrl_GetChangePrompt(0,&button,&kind,&padStatus) && button==BtlCharApi_GetButtonIcon(c,2) && kind==1,button);
    }
    {extern s32 Bt4_TestEffectUploadQueue(void);s32 bad=Bt4_TestEffectUploadQueue();check("effect_queue_200_and_reuse",bad==0,bad);}
    printf("bt4-rule: RESULT checks=%d failures=%d\n",checks,failures);
    return failures;
}
