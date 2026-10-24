/* Opt-in full technique regression: real animation events emit the beams,
 * the effect collision creates the struggle, and the native actions finish it. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_clash.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_char_flag.h"
#include "battle/tag_assist.h"
#include "battle/tag_options.h"
#include "battle/btl_cam.h"
#include "battle/eft_h.h"
#include "battle/btl_act_f.h"
#include "sys/pad.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern void BtlAct_Request(BtlMgrChr *,s32), BtlAct_ClearQueue(BtlMgrChr *);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *);
extern s32 BtlAct_GetTechniqueAction(BtlMgrChr *,s32), BtlAnim_GetId(BtlMgrChr *);
extern s32 EftStruggle_IsActive(void);
extern s32 EftStruggle_IsActiveFor(s32),Port_TagMultiBeamTest(void);
extern s32 Port_TagBeamAssistTest(void);
extern void Port_TagTestCheck(s32,s32), Port_TagClashTrace(s32,s32,s32,s32,s32);
extern s32 Port_TagShowcase(void);
extern s32 Port_TagShowcaseDelay(void);
extern s32 TagNet_Active(void);
static s32 showcaseTick,showcaseActive,showcaseHelpers;

/* Local capture fixture: real beams, clash collision and CPU assistance. Only
   staging, inputs, HUD visibility and the filming camera are directed. */
static void showcase(s32 tick) {
    s32 i,n=TagTeam_Count(),help=Port_TagShowcase()==2,helpers=0;
    showcaseTick=tick;
    if(tick==1) {
        Port_TagOptionSet(TAG_OPT_BEAM_ASSIST_CPU,help);
        Port_TagOptionSet(TAG_OPT_BEAM_ASSIST_RANGE,150);
        Port_TagOptionSet(TAG_OPT_CINEMATIC_PAUSE,0);
    }
    if(tick==30)for(i=0;i<n;i++) {
        extern s32 Port_TagShowcaseDistance(void);
        BtlMgrChr *c=BtlChar_Get(i);
        f32 distance=Port_TagShowcaseDistance();
        Vec4 p={0,-15,(i==1)?distance:-distance,1},r={0,i==1?3.14159265f:0,0,0};
        if(i>=2) {p.x=(i&1?-1:1)*(18+((i-2)/2)*18);p.z-=22+((i-2)/2)*15;}
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
        BtlMember_GetActiveGauge(c)->ki=BtlMember_GetActiveGauge(c)->kiMax;
        BtlMember_GetActiveGauge(c)->blast=500000;
        BtlMember_GetActiveGauge(c)->health=200000;
        BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
        while(TagTeam_Target(i)!=(i==1?0:1))TagTeam_CycleTarget(i);
        { extern s32 BtlSuper_GetType(BtlMgrChr *,s32); Port_TagClashTrace(tick,i,BtlSuper_GetType(c,2),BtlSuper_GetType(c,3),BtlSuper_GetType(c,4)); }
        { extern s32 BtlSuper_GetType(BtlMgrChr *,s32),BtlSuper_GetHitDirKind(BtlMgrChr *,s32);
          extern EftHSlot *EftShot_GetSlot(s32,s32);s32 k;
          for(k=2;k<5;k++){EftHSlot *fx=EftShot_GetSlot(i,k);
            Port_TagClashTrace(600+i,k,BtlSuper_GetType(c,k),BtlSuper_GetHitDirKind(c,k),fx->param->type);
          } }
    }
    for(i=0;i<2;i++)if(tick==80+(i?Port_TagShowcaseDelay():0)) {
        extern s32 Port_TagShowcaseSlot(s32);
        BtlMgrChr *c=BtlChar_Get(i);
        BtlAct_Request(c,BtlAct_GetTechniqueAction(c,Port_TagShowcaseSlot(i)));
    }
    if(tick==100)for(i=2;i<n;i++){BtlAct_ClearQueue(BtlChar_Get(i));BtlAct_Request(BtlChar_Get(i),0xB);}
    /* Release a chargeable beam through its normal handler once its charge
       loop starts; a staged actor has no physical release packet. */
    for(i=0;i<2;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        if(c->action>=0x10F && c->action<=0x111 &&
           BtlAnim_GetId(c)==262+34*(c->action-0x10F))
            ((BtlSuperChr *)c)->work[0]|=1;
    }
    if(tick%15==0 && tick>=30 && tick<180)for(i=0;i<2;i++) {
        Vec4 p=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos;
        Port_TagClashTrace(tick,i,(s32)p.x,(s32)p.y,(s32)p.z);
    }
    if(EftStruggle_IsActiveFor(0)) {
        showcaseActive++;
        for(i=2;i<n;i++)if(TagAssist_Ally(i)==0)helpers++;
        if(helpers==4 && TagAssist_Multiplier(0)==300) {
            extern s32 TagAssist_Beam(s32,Vec4 *,s32 *);
            s32 beams=0;Vec4 end;
            for(i=2;i<n;i++)if(TagAssist_Beam(i,&end,NULL))beams++;
            if(beams==4)showcaseHelpers++;
        }
        if(showcaseActive%15==0)Port_TagClashTrace(tick,0,helpers,TagAssist_Multiplier(0),BtlClash_State(0));
        if(showcaseActive>60) ((BtlFlagChr *)BtlChar_Get(0))->clashCountA=40;
    }
    if(tick==400) {
        Port_TagTestCheck(showcaseActive>40,550);
        if(help) {
            Port_TagTestCheck(showcaseHelpers>20,551);
            for(i=2;i<n;i++) {
                EftHSlot *s=EftShot_GetSlot(i,5);
                Port_TagTestCheck(!TagAssist_Active(i) && !s->task,552+i);
            }
        }
        Port_TagTestCheck(1,99);
    }
}
s32 TagBeam_ShowcaseCamera(void) {
    extern f32 atan2f(f32,f32),sqrtf(f32);
    BtlCamView *v;Vec4 eye,look;f32 x,y,z;
    if(!Port_TagShowcase() || showcaseTick<30)return 0;
    /* A gentle side-on drift keeps both beam sources and the supporting line
       in frame instead of using the player's tight shoulder camera. */
    eye=(Vec4){80,-35,-20,1};look=(Vec4){0,-26,-5,1};
    {extern void Port_TagShowcaseView(Vec4 *,Vec4 *);Port_TagShowcaseView(&eye,&look);}
    eye.z+=showcaseTick*.035f;
    BtlCam_SelectView(0);BtlCam_SetLayout(0,0);v=BtlCam_GetNativeView(0);
    x=look.x-eye.x;y=look.y-eye.y;z=look.z-eye.z;
    v->pos=eye;v->rot=(Vec4){-atan2f(y,sqrtf(x*x+z*z)),atan2f(x,z),0,0};
    BtlCam_BuildView(v,1);
    return 1;
}

static void multi_beams(s32 tick) {
    static s32 together,firstEnded,ended,seen[2];
    s32 i;
    if(tick==30) for(i=0;i<4;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        Vec4 pos={(i<2?-160.0f:160.0f),-100.0f,(i&1)?150.0f:-150.0f,1};
        Vec4 rot={0,(i&1)?3.14159265f:0,0,0};
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
        BtlMember_GetActiveGauge(c)->ki=BtlMember_GetActiveGauge(c)->kiMax;
        BtlChar_Place(c,&pos,&rot,BtlStage_FindZoneNear(-1,&pos));
        while(TagTeam_Target(i)!=(i^1)) TagTeam_CycleTarget(i);
    }
    if(tick==35 || tick==65) for(i=tick==35?0:2;i<(tick==35?2:4);i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlAct_Request(c,BtlAct_GetTechniqueAction(c,2));
    }
    for(i=0;i<2;i++) if(EftStruggle_IsActiveFor(i*2)) seen[i]=1;
    if(EftStruggle_IsActiveFor(0) && EftStruggle_IsActiveFor(2)) {
        if(!together) {
            Port_TagTestCheck(BtlClash_Opponent(0)==1 && BtlClash_Opponent(2)==3,88);
            together=1;
        }
    }
    if(seen[0] && !EftStruggle_IsActiveFor(0) && EftStruggle_IsActiveFor(2) && !firstEnded) {
        Port_TagTestCheck(together,89);firstEnded=1;
    }
    if(seen[1] && !EftStruggle_IsActive() && BtlClash_Opponent(2)<0) ended++;
    if(ended==60) {
        Port_TagTestCheck(firstEnded && BtlClash_Opponent(0)<0 && BtlClash_Opponent(3)<0,90);
        Port_TagTestCheck(1,99);
    }
    if(tick==700) Port_TagTestCheck(0,91);
}

void TagBeam_TestTick(s32 tick) {
    static s32 activeFrames, pairSeen, ended;
    static s32 assisted,stockBefore;
    s32 i;
    if(Port_TagShowcase()) {showcase(tick);return;}
    if(Port_TagMultiBeamTest()) {multi_beams(tick);return;}
    if(tick==30) {
        Port_TagTestCheck(Port_TagOption(TAG_OPT_SHARED_CLASH)==0,339);
        if(Port_TagBeamAssistTest()) {
            Port_TagTestCheck(Port_TagOption(TAG_OPT_BEAM_ASSIST)==1,97);
            Port_TagOptionSet(TAG_OPT_BEAM_ASSIST_CPU,0);
        }
        for(i=0;i<4;i++) {
            BtlMgrChr *c=BtlChar_Get(i);
            Vec4 pos={i<2?(Port_TagBeamAssistTest()?35.0f:100.0f):0.0f,-100.0f,(i&1)?150.0f:-150.0f,1};
            Vec4 rot={0,(i&1)?3.14159265f:0,0,0};
            BtlAct_ClearQueue(c); BtlAct_Request(c,0xB);
            BtlMember_GetActiveGauge(c)->ki=BtlMember_GetActiveGauge(c)->kiMax;
            BtlMember_GetActiveGauge(c)->blast=300000;
            BtlChar_Place(c,&pos,&rot,BtlStage_FindZoneNear(-1,&pos));
        }
        while(TagTeam_Target(2)!=3) TagTeam_CycleTarget(2);
        while(TagTeam_Target(3)!=2) TagTeam_CycleTarget(3);
    }
    if(tick==35) for(i=2;i<4;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        s32 action=BtlAct_GetTechniqueAction(c,2);
        Port_TagTestCheck(action==0x106,80+i);
        BtlAct_Request(c,action);
    }
    if(tick%15==0) for(i=2;i<4;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        Port_TagClashTrace(tick,i,c->freeze,BtlAnim_GetId(c),c->action);
    }
    if(EftStruggle_IsActive()) {
        pairSeen=1;
        activeFrames++;
        /* Mid-match room rules belong to the host's synchronized config.
         * Exercise local option changes only in the split-screen fixture. */
        if(activeFrames==20 && !TagNet_Active())Port_TagOptionSet(TAG_OPT_SHARED_CLASH,1);
        if(activeFrames==30 && !TagNet_Active()) {
            extern s32 BtlScene_IsSingleView(void);
            Port_TagTestCheck(BtlScene_IsSingleView(),344);
        }
        if(activeFrames==40 && !TagNet_Active())Port_TagOptionSet(TAG_OPT_SHARED_CLASH,0);
        if(activeFrames==50 && !TagNet_Active()) {
            extern s32 BtlScene_IsSingleView(void);
            Port_TagTestCheck(!BtlScene_IsSingleView(),345);
        }
        if(activeFrames==10) {
            Port_TagTestCheck(BtlClash_Opponent(2)==3 && BtlClash_Opponent(3)==2,84);
            Port_TagTestCheck(BtlClash_State(2)==2 && BtlClash_State(0)==0,85);
            Port_TagTestCheck(!TagTeam_ShareClashCamera(2) &&
                TagTeam_CameraActor(0)==0 && TagTeam_CameraActor(1)==1,340);
            Port_TagTestCheck(TagTeam_CameraActor(2)>=2 && TagTeam_CameraActor(3)>=2,341);
            if(!TagNet_Active()) {
                extern s32 BtlCam_GetPriorityView(void);
                Port_TagOptionSet(TAG_OPT_SHARED_CLASH,1);
                Port_TagTestCheck(TagTeam_ShareClashCamera(2) && BtlCam_GetPriorityView()>=2,342);
                Port_TagOptionSet(TAG_OPT_SHARED_CLASH,0);
                Port_TagTestCheck(BtlCam_GetPriorityView()<0,343);
            }
            if(Port_TagBeamAssistTest()) {
                Port_TagTestCheck(TagAssist_ReadyAlly(0)==2,92);
                stockBefore=BtlMember_GetActiveGauge(BtlChar_Get(0))->blast;
                gPad[0].pressed|=PAD_R3;gPad[0].held|=PAD_R3;
                TagAssist_Update();
                gPad[0].pressed&=~PAD_R3;gPad[0].held&=~PAD_R3;
                Port_TagTestCheck(TagAssist_Ally(0)==2,93);
            }
        }
        if(Port_TagBeamAssistTest() && !assisted && TagAssist_Multiplier(2)>100) {
            s32 cost=stockBefore-BtlMember_GetActiveGauge(BtlChar_Get(0))->blast;
            Port_TagTestCheck(TagAssist_Multiplier(2)==150,94);
            /* Normal stock regeneration continues through the approach. */
            Port_TagTestCheck(cost>=90000 && cost<=100000,95);
            {
                extern f32 BtlCharApi_GetRadius(s32);
                Vec4 p=((BtlMovePose *)&BtlChar_Get(0)->pose)->pos;
                Vec4 q=((BtlMovePose *)&BtlChar_Get(2)->pose)->pos;
                f32 x=p.x-q.x,y=p.y-q.y,z=p.z-q.z;
                f32 r=BtlCharApi_GetRadius(BtlChar_Get(0)->objId)+BtlCharApi_GetRadius(BtlChar_Get(2)->objId);
                /* Close enough to read as support, outside both body radii. */
                Port_TagTestCheck(x*x+y*y+z*z>=r*r && x*x+y*y+z*z<(r+5)*(r+5),338);
            }
            assisted=1;
        }
        if(activeFrames>65) ((BtlFlagChr *)BtlChar_Get(2))->clashCountA=50;
    } else if(pairSeen && BtlClash_Opponent(2)<0) ended++;
    if(ended==60) {
        Port_TagTestCheck(activeFrames>10,86);
        if(Port_TagBeamAssistTest()) Port_TagTestCheck(assisted && !TagAssist_Active(0) &&
            BtlChar_Get(0)->action>=0xB && BtlChar_Get(0)->action<0x37 && TagAssist_Multiplier(2)==100,96);
        Port_TagTestCheck(1,99);
    }
    if(tick==700) Port_TagTestCheck(0,87);
}
