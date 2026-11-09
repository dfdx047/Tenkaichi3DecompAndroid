/* Real scene regression for extra-fighter projectiles and interrupted auras. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/btl_char_mgr.h"
#include "battle/eft_p.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern void Port_TagTestCheck(s32,s32);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern void BtlChar_SetFxBit(BtlMgrChr *,s32),BtlChar_ClearFxBit(BtlMgrChr *,s32);
extern void BtlFx_UpdateAura(BtlMgrChr *);
extern s32 EftAura_IsBurstRising(s32);
extern void EftAura_Command(s32,s32);
void TagEffects_TestTick(s32 tick) {
    static EftPTask *survivor,*removed;
    static s32 owner;
    s32 i;
    extern void TagBodyFx_TestTick(s32);
    extern void TagEffectSlots_TestTick(s32),TagDust_TestTick(s32),TagWater_TestTick(s32),
                TagBubble_TestTick(s32),TagAura_TestTick(s32),TagTransformFx_TestTick(s32),TagBlind_TestTick(s32);
    TagBodyFx_TestTick(tick);
    TagEffectSlots_TestTick(tick);TagDust_TestTick(tick);TagWater_TestTick(tick);
    TagBubble_TestTick(tick);TagAura_TestTick(tick);
    TagTransformFx_TestTick(tick);TagBlind_TestTick(tick);
    if(tick==69) {
        extern s32 BtlScene_IsEffectStopped(s32,s32);
        BtlMgrChr *a=BtlChar_Get(0),*b=BtlChar_Get(TagTeam_Count()-1);
        s32 fa=a->freeze,fb=b->freeze,aa=a->action,ab=b->action;
        a->action=b->action=0xB;a->freeze=2;b->freeze=0;
        Port_TagTestCheck(BtlScene_IsEffectStopped(a->objId,0),1120);
        Port_TagTestCheck(!BtlScene_IsEffectStopped(b->objId,0),1121);
        a->freeze=0;b->freeze=2;
        Port_TagTestCheck(!BtlScene_IsEffectStopped(a->objId,0),1122);
        Port_TagTestCheck(BtlScene_IsEffectStopped(b->objId,0),1123);
        a->freeze=fa;b->freeze=fb;a->action=aa;b->action=ab;
    }
    if(tick==5 || tick==50) {
        for(i=0;i<TagTeam_Count();i++) {
            EftDiscAtk arg={0};
            arg.objId=BattleSide_GetObjId(i);arg.node=0x15;
            arg.pos.z=1;arg.scale=1;arg.speed=3;arg.life=90;arg.level=2;
            Port_TagTestCheck(EftDisc_Throw(&arg),710+i);
            Port_TagTestCheck(((EftDisc *)gEftDisc->mgr->tail[arg.objId]->task->work)->atk.objId==arg.objId,727);
        }
        {
            EftDiscAtk arg={0};
            arg.objId=-1;Port_TagTestCheck(!EftDisc_Throw(&arg),725);
            arg.objId=EFT_DISC_OWNERS;Port_TagTestCheck(!EftDisc_Throw(&arg),726);
        }
    }
    if(tick==60) {
        EftDiscAtk arg={0};
        owner=BattleSide_GetObjId(TagTeam_Count()-1);
        arg.objId=owner;arg.node=0x15;arg.pos.z=1;
        arg.scale=1;arg.speed=1;arg.life=200;arg.level=2;
        Port_TagTestCheck(EftDisc_SpawnThrown(&arg),721);
        survivor=gEftDisc->mgr->tail[owner]->task;
        /* Keep this one in the hand so terrain/other projectiles cannot
           legitimately destroy it while the newer task is being removed. */
        ((EftDisc *)survivor->work)->flags&=~EFT_DISC_AUTO;
        Port_TagTestCheck(EftDisc_SpawnThrown(&arg),722);
        removed=gEftDisc->mgr->tail[owner]->task;
        EftDisc_Kill(removed);
    }
    if(tick==63) {
        EftDiscNode *n;
        s32 found=0;
        for(n=gEftDisc->mgr->head[owner];n;n=n->next) {
            Port_TagTestCheck(n->task!=removed,723);
            if(n->task==survivor)found=1;
        }
        Port_TagTestCheck(found && gEftDisc->mgr->tail[owner]->task==survivor,724);
    }
    /* Exercise both a normal release and a dropped edge after an interrupted
       update. An idle aura should survive; only the charging burst must end. */
    if(tick>=90 && tick<=112) for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        c->freezeNext=2;
        BtlMember_GetActiveGauge(c)->ki=BtlMember_GetActiveGauge(c)->kiMax;
        if(tick==90)EftAura_Command(c->objId,7);
        if(tick>=95)EftAura_Command(c->objId,0);
    }
    if(tick==109) for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
        EftAura_Command(c->objId,0);EftAura_Command(c->objId,2);
    }
    if(tick==110) for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        Port_TagTestCheck(EftAura_IsBurstRising(c->objId),730+i);
    }
    if(tick==111) for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlChar_ClearFxBit(c,7);
        c->prev1262.b[0]&=~0x80;
        BtlFx_UpdateAura(c);
        Port_TagTestCheck(!EftAura_IsBurstRising(c->objId),740+i);
    }
    if(tick==180) Port_TagTestCheck(1,99);
}
