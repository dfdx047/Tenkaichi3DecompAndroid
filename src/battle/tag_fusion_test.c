/* Opt-in test through the actual native paired animation/model loader. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_fusion.h"
#include "battle/tag_options.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_act_j.h"
#include "battle/btl_char_move.h"
#include "battle/btl_input.h"
#include "battle/btl_cam.h"
#include "battle/btl_demo_cam.h"
#include "sys/pad.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *);
extern s32 BtlParam_GetUnkB4(BtlMgrChr *,s32),BtlAnim_GetId(BtlMgrChr *);
extern s32 BtlAct_GetCurrent(BtlMgrChr *),BtlChar_TestFlag(BtlMgrChr *,s32);
extern f32 BtlCharApi_GetRadius(s32);
extern void Port_TagTestCheck(s32,s32),Port_TagClashTrace(s32,s32,s32,s32,s32);
extern s32 Port_TagFusionSeat(s32);
extern s32 Port_TagFusionResult(void);
extern s32 Port_TagFusionTest(void);
extern void TagFusion_MatrixTick(s32);
extern s32 BtlCharApi_IsChanging(s32);
extern void *memcpy(void *,const void *,u32);
void TagFusion_TestAssign(s32 humans) {
    s32 a=Port_TagFusionSeat(0),b=Port_TagFusionSeat(1),actor,p=0;
    Port_TagTestCheck(a>=0&&b>=0&&a<humans&&b<humans&&a!=b,520);
    for(actor=0;actor<TagTeam_Count();actor++) TagTeam_SetAssignment(actor,-1);
    TagTeam_SetAssignment(0,a);TagTeam_SetAssignment(2,b);
    for(actor=0;actor<TagTeam_Count();actor++) if(actor!=0&&actor!=2) {
        while(p==a||p==b) p++;
        if(p<humans) TagTeam_SetAssignment(actor,p++);
    }
}
void TagFusion_TestTick(s32 tick) {
    if(Port_TagFusionTest()==2) {TagFusion_MatrixTick(tick);return;}
    static s32 fused,slot=-1,waited,baseMax,partnerMax,lastController=-1,swaps,clockChecked;
    static s32 before[4][4];
    static f32 lastCameraTime;
    static s32 cameraAdvances,sharedFrames;
    BtlMgrChr *a=BtlChar_Get(0),*b=BtlChar_Get(2);
    s32 i,j,partner=-1,first=TagTeam_Player(0),second=TagTeam_Player(2);
    s32 result=Port_TagFusionResult(),potara=result==51;
    if(tick==20) {
        if(Port_TagFusionTest()!=3)Port_TagOptionSet(TAG_OPT_LOCAL_CINEMATICS,1);
        for(i=0;i<TagTeam_ViewCount();i++) TagTeam_ViewRect(i,before[i]);
        baseMax=BtlMember_GetActiveGauge(a)->healthMax;
        partnerMax=BtlMember_GetActiveGauge(b)->healthMax;
        BtlMember_GetActiveGauge(a)->blast=BtlMember_GetActiveGauge(a)->blastMax;
        BtlMember_GetActiveGauge(b)->blast=BtlMember_GetActiveGauge(b)->blastMax;
        Port_TagTestCheck(Port_TagOption(TAG_OPT_FUSION_SWAP)==1 &&
            Port_TagOption(TAG_OPT_FUSION_SWAP_SECONDS)==20 &&
            Port_TagOption(TAG_OPT_FUSION_METER)==1 &&
            Port_TagOption(TAG_OPT_FUSION_OWNER)==1 &&
            Port_TagOption(TAG_OPT_FUSION_COUNTDOWN)==1,531);
        Port_TagOptionSet(TAG_OPT_FUSION_SECONDS,20);
        Port_TagOptionSet(TAG_OPT_FUSION_SWAP_SECONDS,5);
        for(i=0;i<3;i++) {
            Port_TagClashTrace(tick,0,i,BtlParam_GetUnkB4(a,i),a->action);
            if(BtlParam_GetUnkB4(a,i)==result) slot=i;
        }
        Port_TagTestCheck(slot>=0,500);
        Port_TagTestCheck(BtlAct_CanFuse((BtlActJChr *)a,slot,1,1,&partner)&&partner==2,501);
        Port_TagTestCheck(!BtlAct_QueueFusion((BtlActJChr *)a,slot,partner),502);
        Port_TagTestCheck(!TagFusion_Hidden(2)&&TagFusion_Requester(2)==0,503);
    }
    if(tick==25) {
        gPad[second].pressed|=PAD_R3;
        TagFusion_Update();
        gPad[second].pressed&=~PAD_R3;
        Port_TagTestCheck(TagFusion_Hidden(2),504);
    }
    if(tick>25 && tick%30==0) {
        Port_TagClashTrace(tick,0,TagTeam_Side(0)->form.chara,BtlAnim_GetId(a),a->action);
        Port_TagClashTrace(tick,2,TagTeam_ViewActor(second),TagFusion_Seconds(0),b->action);
    }
    if(Port_TagFusionTest()==3 && DemoCam_FighterActive(0)) {
        extern void *DemoCam_EnterFighter(s32);
        extern void DemoCam_LeaveFighter(void *);
        extern s32 BtlScene_IsSingleView(void);
        void *saved=DemoCam_EnterFighter(0);
        f32 time=DemoCam_GetTime();
        if(time>lastCameraTime)cameraAdvances++;
        lastCameraTime=time;
        DemoCam_LeaveFighter(saved);
        if(BtlCam_GetPriorityView()==0 && BtlScene_IsSingleView()) {
            View *v=&BtlCam_GetNativeView(0)->view;
            /* A fused layout must not leak an upper/side viewport's centre
               or crop into the shared native dance/appearance camera. */
            Port_TagTestCheck(v->centerX==2048.0f && v->centerY==2048.0f &&
                v->scissorX0==0 && v->scissorX1==511 &&
                v->scissorY0==0 && v->scissorY1==447,546);
            sharedFrames++;
        }
    }
    if(BtlMember_GetActive(a)->chara==result) {
        if(!fused) {
            s32 views=TagTeam_ViewCount(),diagonal=views==4&&(first^second)==3;
            s32 lead=first<second?first:second,other=first<second?second:first,r[4];
            Port_TagTestCheck(TagTeam_ViewActor(second)==0,505);
            Port_TagTestCheck(TagFusion_Hidden(2)&&!TagTeam_IsAlive(2),506);
            Port_TagTestCheck(BtlMember_GetActiveGauge(a)->healthMax==baseMax+partnerMax,507);
            Port_TagTestCheck(TagTeam_VisibleViews()==views-(diagonal?0:1),521);
            Port_TagTestCheck(TagTeam_ViewVisible(lead)&&TagTeam_ViewVisible(other)==diagonal,522);
            TagTeam_ViewRect(lead,r);
            if(views==2) Port_TagTestCheck(r[0]==0&&r[1]==0&&r[2]==512&&r[3]==448,523);
            if(views==4&&!diagonal) {
                if((first^second)==1) Port_TagTestCheck(r[0]==0&&r[1]==(lead/2)*224&&r[2]==512&&r[3]==224,524);
                else Port_TagTestCheck(r[0]==(lead%2)*256&&r[1]==0&&r[2]==256&&r[3]==448,525);
            }
            if(diagonal) for(i=0;i<4;i++) {
                TagTeam_ViewRect(i,r);
                for(j=0;j<4;j++) Port_TagTestCheck(r[j]==before[i][j],526);
            }
            BtlMember_GetActiveGauge(a)->blast=0;
        }
        fused++;
        if(Port_TagFusionTest()==3 && fused==120) {
            Port_TagTestCheck(cameraAdvances>50 && sharedFrames>50,547);
            Port_TagTestCheck(1,99);
        }
        if(!BtlCharApi_IsChanging(a->objId)) {
            s32 controller=TagFusion_Controller(0);
            if(!clockChecked) {
                s32 seconds=TagFusion_SwitchSeconds(0),life=TagFusion_Seconds(0);
                Port_TagTestCheck((life<0)==potara,541);
                Battle_GetWork()->flags|=BATTLE_FLAG_PAUSE;
                for(i=0;i<35;i++) TagFusion_Update();
                Battle_GetWork()->flags&=~BATTLE_FLAG_PAUSE;
                Port_TagTestCheck(seconds==TagFusion_SwitchSeconds(0)&&life==TagFusion_Seconds(0),532);
                /* A permanent fusion has no life meter, but can still rotate
                   control. Re-enable the dance limit for the defusion test. */
                Port_TagOptionSet(TAG_OPT_TIMED_FUSIONS,0);
                Port_TagTestCheck(TagFusion_Seconds(0)==-1&&TagFusion_NextController(0)==second,533);
                Port_TagOptionSet(TAG_OPT_TIMED_FUSIONS,1);
                clockChecked=1;
            }
            if(controller!=lastController) {
                if(lastController>=0) swaps++;
                lastController=controller;
                Port_TagTestCheck(controller==(swaps%2?second:first),534);
                Port_TagTestCheck(a->pad==controller && !a->injectOn && TagTeam_Side(0)->pad==controller,535);
                Port_TagTestCheck(TagTeam_Player(0)==first&&TagTeam_Player(2)==second&&TagTeam_InputPlayer(2)==-1,536);
                {
                    BtlInputChr *input=(BtlInputChr *)a;
                    BtlCharInput saved=input->input;
                    Pad saveFirst=gPad[first],saveSecond=gPad[second];
                    gPad[first].held=PAD_CIRCLE;gPad[second].held=PAD_SQUARE;
                    gPad[first].left[0]=-0.5f;gPad[second].left[0]=0.5f;
                    BtlInput_Sample(input);
                    Port_TagTestCheck(input->input.rawHeld==(controller==first?PAD_CIRCLE:PAD_SQUARE),537);
                    Port_TagTestCheck((input->input.rawStickX>127)==(controller==second),538);
                    input->input=saved;gPad[first]=saveFirst;gPad[second]=saveSecond;
                }
            }
            if(potara && fused>1000 && swaps>=2) {
                /* The real Potara remains fused beyond the dance limit.
                   Disabling automatic control switches returns the first
                   player's controls without touching the fusion itself. */
                Port_TagOptionSet(TAG_OPT_FUSION_SWAP,0);
                for(i=0;i<160;i++) TagFusion_Update();
                Port_TagTestCheck(TagFusion_Controller(0)==first&&a->pad==first,542);
                Port_TagTestCheck(TagFusion_NextController(0)==-1&&TagFusion_SwitchSeconds(0)==-1,543);
                Port_TagTestCheck(TagFusion_Hidden(2)&&BtlMember_GetActive(a)->chara==result,544);
                Port_TagOptionSet(TAG_OPT_FUSION_SWAP,1);
                TagFusion_Update();
                Port_TagTestCheck(TagFusion_NextController(0)==second&&TagFusion_SwitchSeconds(0)==5,545);
                Port_TagTestCheck(1,99);
            }
        }
    }
    if(fused && !TagFusion_Hidden(2) && TagTeam_Side(0)->form.chara==3) {
        Vec4 p=((BtlMovePose *)&a->pose)->pos,q=((BtlMovePose *)&b->pose)->pos;
        f32 x=p.x-q.x,y=p.y-q.y,z=p.z-q.z,r=BtlCharApi_GetRadius(a->objId)+BtlCharApi_GetRadius(b->objId);
        if(++waited==4) {
            Port_TagTestCheck(x*x+y*y+z*z>r*r,508);
            Port_TagTestCheck(TagTeam_ViewActor(first)==0&&TagTeam_ViewActor(second)==2,509);
            Port_TagTestCheck(!BtlChar_TestFlag(b,0xB)&&TagTeam_IsAlive(0)&&TagTeam_IsAlive(2),510);
            Port_TagTestCheck(BtlMember_GetActiveGauge(a)->healthMax==baseMax,511);
            for(i=0;i<TagTeam_ViewCount();i++) {
                s32 r[4];
                Port_TagTestCheck(TagTeam_ViewVisible(i),527);
                TagTeam_ViewRect(i,r);
                for(j=0;j<4;j++) Port_TagTestCheck(r[j]==before[i][j],528);
            }
            Port_TagTestCheck(BtlMember_GetActiveGauge(a)->blast<BtlMember_GetActiveGauge(a)->blastMax,529);
            Port_TagTestCheck(BtlMember_GetActiveGauge(b)->blast<BtlMember_GetActiveGauge(b)->blastMax,530);
            Port_TagTestCheck(swaps>=2,539);
            Port_TagTestCheck(a->pad==first&&b->pad==second&&TagTeam_InputPlayer(0)==first&&TagTeam_InputPlayer(2)==second,540);
        }
        /* Keep ordinary play running long enough to inspect the restored
           cameras and models after the power-down finishes. */
        if(waited==60) Port_TagTestCheck(1,99);
    }
    if(tick==2100) Port_TagTestCheck(0,512);
}
