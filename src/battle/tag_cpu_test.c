/* Exercises target decisions on the loaded native fighter roster, including
   damage through the normal combat accounting and real action/queue fields. */
#include "common.h"
#include "battle/tag_cpu.h"
#include "battle/cpu_level.h"
#include "battle/tag_team.h"
#include "battle/tag_balance.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_member.h"
#include "battle/btl_char_move.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern void BtlAct_Request(BtlMgrChr *,s32),BtlAct_ClearQueue(BtlMgrChr *);
extern void Port_TagTestCheck(s32,s32);
static void advance(s32 n) {while(n-->0)TagCpu_Update();}
void TagCpu_TestTick(s32 tick) {
    s32 i,humanTarget,oldAction;
    BtlMgrChr *cpu=BtlChar_Get(2);
    BtlMemberChr *member=(BtlMemberChr *)cpu;
    if(tick==20) {
        for(i=0;i<4;i++) {
            BtlMgrChr *c=BtlChar_Get(i);
            BtlAct_ClearQueue(c);BtlAct_Request(c,11);
            c->freeze=c->freezeNext=0;
        }
    }
    if(tick!=25)return;
    {
        extern s32 PortGame_IsBT4(void),CpuLevel_FromSetting(u32),CpuLevel_ToSetting(s32);
        extern const char *CpuLevel_SettingName(s32);
        const s32 bt3[]={0,6,13,21,29},bt4[]={6,13,21,29,99};
        for(i=0;i<5;i++) {
            s32 level=CpuLevel_FromSetting(i);
            Port_TagTestCheck(level==(PortGame_IsBT4()?bt4[i]:bt3[i]),820+i);
            Port_TagTestCheck(CpuLevel_ToSetting(level)==i,825+i);
        }
        Port_TagTestCheck(CpuLevel_ToSetting(-1)==-1 && CpuLevel_TableIndex(-1)==0 &&
            CpuLevel_TableIndex(29)==4 && CpuLevel_TableIndex(99)==4,830);
        Port_TagTestCheck(!strcmp(CpuLevel_SettingName(4),PortGame_IsBT4()?"Z Hard":"Very Strong"),831);
    }
    for(i=0;i<4;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlMovePose *p=(BtlMovePose *)&c->pose;
        BtlMemberQueue *q=&((BtlMemberChr *)c)->queue;
        p->pos.x=i==1?1000:i==3?-1000:0;p->pos.y=-50;p->pos.z=0;
        q->total=q->drainHealth=q->drainKi=0;
        c->freeze=i==2?0:10000;c->freezeNext=0;
        BtlMember_GetActiveGauge((BtlMemberChr *)c)->health=30000;
    }
    humanTarget=TagTeam_Target(0);
    TagCpu_Reset();
    Port_TagTestCheck(TagTeam_InputPlayer(0)==0 && TagTeam_InputPlayer(2)<0,800);
    Port_TagTestCheck(TagTeam_SetCpuTarget(2,1) && !TagTeam_SetCpuTarget(0,3) &&
        !TagTeam_SetCpuTarget(2,0) && !TagTeam_SetCpuTarget(-1,3),801);
    advance(1);
    TagCpu_Damage(2,0,32000); /* Allies must never earn retaliation. */
    Port_TagTestCheck(TagBalance_Damage(cpu,6000,0x1C3,3)==6000,802);
    Battle_GetWork()->flags|=BATTLE_FLAG_PAUSE;advance(400);
    Battle_GetWork()->flags&=~BATTLE_FLAG_PAUSE;
    advance(59);
    Port_TagTestCheck(TagTeam_Target(2)==1,803); /* Minimum commitment; pause adds no age. */
    advance(1);
    Port_TagTestCheck(TagTeam_Target(2)==3 && TagTeam_Target(0)==humanTarget,804);
    /* Fresh threat cannot redirect a pending cinematic's damage to somebody
       else. Once the queue ends, the dead target is replaced immediately. */
    member->queue.total=1000;
    BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(3))->health=0;
    advance(100);
    Port_TagTestCheck(TagTeam_Target(2)==3,805);
    member->queue.total=0;advance(1);
    Port_TagTestCheck(TagTeam_Target(2)==1,806);
    BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(3))->health=30000;
    oldAction=cpu->action;cpu->action=0xFD;
    TagCpu_Damage(2,3,32000);advance(100);
    Port_TagTestCheck(TagTeam_Target(2)==1,807);
    cpu->action=oldAction;advance(1);
    Port_TagTestCheck(TagTeam_Target(2)==3,808);
    /* No candidates is valid while the round result is being resolved. */
    BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(1))->health=0;
    BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(3))->health=0;
    advance(300);
    Port_TagTestCheck(TagTeam_Target(2)==3 && TagTeam_Target(0)==humanTarget,809);
    TagCpu_Reset();
    BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(1))->health=30000;
    BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(3))->health=30000;
    TagTeam_SetCpuTarget(2,1);advance(1);
    Port_TagTestCheck(TagTeam_Target(2)==1,810); /* Rematch clears decisions and grudges. */
    Port_TagTestCheck(1,99);
}
