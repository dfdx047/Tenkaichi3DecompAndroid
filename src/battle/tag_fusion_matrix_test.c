/* Additional real model-load cases: different pairs, costumes, CPU partners,
 * simultaneous fusions, permanent Potara and a KO while fused. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_fusion.h"
#include "battle/tag_options.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_act_j.h"
#include "sys/pad.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *);
extern s32 BtlParam_GetUnkB4(BtlMgrChr *,s32),BtlParam_GetUnkB1(BtlMgrChr *,s32);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern s32 BtlAct_GetQueued(BtlMgrChr *),BtlCharApi_IsChanging(s32);
extern s32 Port_TagFusionPair(s32),Port_TagFusionDouble(void),Port_TagFusionResult(void),Port_TagFusionKo(void);
extern void Port_TagTestCheck(s32,s32),Port_TagClashTrace(s32,s32,s32,s32,s32);
extern s32 BtlMember_Damage(BtlMgrChr *,s32,s32);
extern f32 BtlCharApi_GetRadius(s32);
extern s32 Port_TagFusionRematch(void),Battle_Restart(void);

void TagFusion_MatrixTick(s32 tick) {
    static s32 seen[2],returned[2],max[2][2],dance[2],koAt,koSeconds,restarted;
    s32 pairs=Port_TagFusionDouble()?2:1,team,i;
    if(restarted) {
        if(tick==restarted+30) {
            Port_TagTestCheck(BtlMember_GetActive(BtlChar_Get(0))->chara==Port_TagFusionPair(0),565);
            Port_TagTestCheck(BtlMember_GetActive(BtlChar_Get(2))->chara==Port_TagFusionPair(1),566);
            Port_TagTestCheck(!TagFusion_Hidden(2)&&TagFusion_SharedBody(0)<0,567);
            Port_TagTestCheck(TagTeam_IsAlive(0)&&TagTeam_IsAlive(2),568);
            Port_TagTestCheck(1,99);
        }
        return;
    }
    if(Port_TagFusionRematch()&&seen[0]&&tick>=200) {
        restarted=tick;Battle_Restart();return;
    }
    if(tick<20) for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
        c->freeze=c->freezeNext=c->freezeDelay=0;
    }
    if(tick==20) {
        Port_TagOptionSet(TAG_OPT_FUSION_SECONDS,10);
        Port_TagOptionSet(TAG_OPT_SHARED_TRANSFORM,0);
        Port_TagOptionSet(TAG_OPT_CINEMATIC_PAUSE,2);
        for(team=0;team<pairs;team++) {
            BtlMgrChr *c=BtlChar_Get(team),*p=BtlChar_Get(team+2);
            s32 slot=-1,partner=-1,queued;
            max[team][0]=BtlMember_GetActiveGauge(c)->healthMax;
            max[team][1]=BtlMember_GetActiveGauge(p)->healthMax;
            BtlMember_GetActiveGauge(c)->blast=BtlMember_GetActiveGauge(c)->blastMax;
            BtlMember_GetActiveGauge(p)->blast=BtlMember_GetActiveGauge(p)->blastMax;
            for(i=0;i<3;i++) if(BtlParam_GetUnkB4(c,i)==Port_TagFusionResult()) slot=i;
            Port_TagTestCheck(slot>=0,550);
            dance[team]=BtlParam_GetUnkB1(c,slot)==1;
            Port_TagTestCheck(BtlAct_CanFuse((BtlActJChr *)c,slot,1,1,&partner)&&partner==team+2,551);
            queued=BtlAct_QueueFusion((BtlActJChr *)c,slot,partner);
            if(TagTeam_Player(partner)<0) {
                Port_TagTestCheck(queued&&TagFusion_Hidden(partner),552);
                BtlAct_Request(c,BtlAct_GetQueued(c));
            } else {
                Port_TagTestCheck(!queued&&TagFusion_Requester(partner)==team,553);
                gPad[TagTeam_Player(partner)].pressed|=PAD_R3;
            }
        }
        TagFusion_Update();
        for(i=0;i<4;i++) gPad[i].pressed&=~PAD_R3;
    }
    for(team=0;team<pairs;team++) {
        BtlMgrChr *c=BtlChar_Get(team),*p=BtlChar_Get(team+2);
        BtlMgrMember *m=BtlMember_GetActive(c);
        if(m->chara==Port_TagFusionResult() && !BtlCharApi_IsChanging(c->objId) &&
           c->action!=0xF1 && c->action!=0xF2) {
            if(!seen[team]) {
                Port_TagTestCheck(TagFusion_Hidden(team+2)&&TagFusion_SharedBody(team)==team,554);
                Port_TagTestCheck(m->gauge.healthMax==max[team][0]+max[team][1],555);
                seen[team]=1;
            }
            if(Port_TagFusionKo() && !koAt) {
                BtlMember_Damage(c,m->gauge.health+1000,0x1C3);
                BtlAct_ClearQueue(c);BtlAct_Request(c,0xD8);
                koAt=tick;koSeconds=TagFusion_Seconds(team);
            } else if(!Port_TagFusionKo()) {
                /* Keep the CPU's fused fighter idle so the timer can hand off
                   through the real power-down instead of starting more moves. */
                BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            }
            if(!dance[team] && tick>500) {
                Port_TagTestCheck(TagFusion_Seconds(team)==-1,556);
                returned[team]=1;
            }
        } else if(seen[team] && !TagFusion_Hidden(team+2)) {
            Vec4 a=((BtlMovePose *)&c->pose)->pos,b=((BtlMovePose *)&p->pose)->pos;
            f32 x=a.x-b.x,y=a.y-b.y,z=a.z-b.z,r=BtlCharApi_GetRadius(c->objId)+BtlCharApi_GetRadius(p->objId);
            Port_TagTestCheck(m->chara==Port_TagFusionPair(0)&&BtlMember_GetActive(p)->chara==Port_TagFusionPair(1),557);
            Port_TagTestCheck(m->costume==Port_TagFusionPair(2)&&BtlMember_GetActive(p)->costume==Port_TagFusionPair(2),558);
            Port_TagTestCheck(m->gauge.healthMax==max[team][0]&&BtlMember_GetActiveGauge(p)->healthMax==max[team][1],559);
            if(!returned[team]) Port_TagTestCheck(x*x+y*y+z*z>r*r,560);
            returned[team]=1;
        }
    }
    if(koAt && tick==koAt+20) {
        Port_TagTestCheck(!TagTeam_IsAlive(0)&&!TagTeam_IsAlive(2)&&TagFusion_Hidden(2),561);
        Port_TagTestCheck(TagFusion_Seconds(0)==koSeconds,562);
        Port_TagTestCheck(TagTeam_IsAlive(4)&&!TagTeam_CheckEnd(0),563);
        Port_TagTestCheck(1,99);
    }
    if(returned[0] && (pairs==1 || returned[1])) Port_TagTestCheck(1,99);
    if(tick%120==0) for(team=0;team<pairs;team++)
        Port_TagClashTrace(tick,team,TagFusion_Seconds(team),TagTeam_Side(team)->form.chara,BtlChar_Get(team)->action);
    if(tick==1400) Port_TagTestCheck(0,564);
}
