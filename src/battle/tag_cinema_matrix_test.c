/* Drive native technique animations and a concurrent native transformation. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_act_j.h"
#include "battle/btl_cam.h"
#include "battle/stg_a.h"
extern s32 Port_TagCinemaTest(void);
extern void Port_TagTestCheck(s32,s32),Port_TagClashTrace(s32,s32,s32,s32,s32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *);
extern void BtlAct_Request(BtlMgrChr *,s32),BtlAct_ClearQueue(BtlMgrChr *);
extern s32 BtlAct_GetQueued(BtlMgrChr *),BtlAnim_GetId(BtlMgrChr *);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *),BtlCharApi_HasCamPriority(s32),BtlCharApi_IsChanging(s32);
extern s32 BtlCharApi_IsInRushSequence(s32);
extern s32 BtlCharApi_HasAuthoredCamera(s32);
static void transition_test(s32 tick,s32 kind);
void TagCinema_MatrixTick(s32 tick) {
    extern void TagSpecial_TestTick(s32);
    static s32 camera,paired,transformed,settled,hp,queued;
    s32 i,kind=Port_TagCinemaTest();
    BtlMgrChr *a=BtlChar_Get(0),*b=BtlChar_Get(3),*t=BtlChar_Get(2);
    if(kind==10||kind==11||kind==13) {transition_test(tick,kind);return;}
    if(kind==12)kind=2;
    if(kind>=4) {TagSpecial_TestTick(tick);return;}
    if(tick==30) {
        Port_TagOptionSet(TAG_OPT_LOCAL_CINEMATICS,1);
        Port_TagOptionSet(TAG_OPT_SHARED_TRANSFORM,Port_TagCinemaTest()==12);
        Port_TagOptionSet(TAG_OPT_SHARED_ULTIMATE,Port_TagCinemaTest()==12);
        for(i=0;i<4;i++) {
            BtlMgrChr *c=BtlChar_Get(i);
            Vec4 p={i==1?300.0f:i==2?-300.0f:0,-100,i==3?75.0f:0,1};
            Vec4 r={0,i==3?3.14159265f:0,0,0};
            StgGroundHit floor;
            p.y=BtlStage_GetTop()+1;
            BtlStage_ProbeGround(-1,&p,&floor);p.y=floor.y;
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
            BtlMember_GetActiveGauge(c)->health=100000;
            BtlMember_GetActiveGauge(c)->ki=BtlMember_GetActiveGauge(c)->kiMax;
            BtlMember_GetActiveGauge(c)->blast=BtlMember_GetActiveGauge(c)->blastMax;
            c->freeze=c->freezeNext=0;
        }
        while(TagTeam_Target(0)!=3) TagTeam_CycleTarget(0);
        while(TagTeam_Target(3)!=2) TagTeam_CycleTarget(3);
        hp=BtlMember_GetActiveGauge(b)->health;
    }
    if(tick==35) BtlAct_Request(a,BtlAct_GetTechniqueAction((BtlActJChr *)a,kind==1?3:4));
    if(tick>35 && !queued && BtlCharApi_HasCamPriority(a->objId)) {
        Port_TagTestCheck(BtlAct_CanTransform((BtlActJChr *)t,0,1,1),650);
        Port_TagTestCheck(BtlAct_QueueTransform((BtlActJChr *)t,0),651);
        BtlAct_Request(t,BtlAct_GetQueued(t));queued=1;
    }
    if(tick>35) {
        if(BtlCharApi_HasAuthoredCamera(a->objId)) camera++;
        if(BtlCharApi_IsInRushSequence(a->objId)) {
            paired++;
            Port_TagTestCheck(TagTeam_Target(3)==0,652);
            if(BtlCharApi_HasAuthoredCamera(a->objId)) Port_TagTestCheck(TagTeam_CameraActor(3)==0,662);
            Port_TagTestCheck(!TagTeam_CinematicPaused(0)&&!TagTeam_CinematicPaused(3),653);
        }
    }
    if(BtlMember_GetActive(t)->chara!=3 && !BtlCharApi_IsChanging(t->objId)) transformed=1;
    if(queued && transformed && tick>100 && a->action<0x100 && b->action<0x100 &&
       !BtlCharApi_HasCamPriority(a->objId)&&!BtlCharApi_HasCamPriority(t->objId) &&
       !BtlCharApi_IsInRushSequence(a->objId)) settled++;
    else settled=0;
    if(settled==45) {
        Port_TagTestCheck(camera>0,654);
        if(kind!=2) Port_TagTestCheck(paired>0,655);
        Port_TagTestCheck(BtlMember_GetActiveGauge(b)->health<hp,656);
        for(i=0;i<4;i++) Port_TagTestCheck(!TagTeam_CinematicPaused(i)&&TagTeam_CameraActor(i)==i,657+i);
        Port_TagTestCheck(1,99);
    }
    if(tick%60==0) for(i=0;i<4;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        Port_TagClashTrace(tick,i,TagTeam_CameraActor(i),BtlMember_GetActive(c)->chara,c->action);
    }
    if(tick==1200) Port_TagTestCheck(0,661);
}

/* Real transformations and the native planet/stage loader, with every logical
   fighter observed through the whole transition. */
static void transition_test(s32 tick,s32 kind) {
    static s32 original,seen,settled,loadFrames,viewFrames,wasLoading;
    static f32 cameraTime;
    static Vec4 cameraStart;
    static f32 cameraTravel;
    static Vec4 positions[10];static s32 hp[10];
    extern void BtlScene_RequestStageChange(void);
    extern s32 BtlScene_IsSingleView(void),DemoCam_IsActive(void),EftBurst_IsBusy(void);
    extern void Port_TagSpawnTrace(s32,s32,u32),Port_TagSpecialTrace(s32,s32,s32,s32,s32);
    s32 i,loading=(Battle_GetWork()->flags&BATTLE_FLAG_LOADING)!=0;
    BtlMgrChr *a=BtlChar_Get(0);
    if(tick==30) {
        original=kind==10?BtlMember_GetActive(a)->chara:Battle_GetStage();
        for(i=0;i<TagTeam_Count();i++) {
            BtlMgrChr *c=BtlChar_Get(i);
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            c->freeze=c->freezeNext=0;
            BtlMember_GetActiveGauge(c)->blast=BtlMember_GetActiveGauge(c)->blastMax;
            BtlMember_GetActiveGauge(c)->ki=BtlMember_GetActiveGauge(c)->kiMax;
            hp[i]=BtlMember_GetActiveGauge(c)->health;
        }
    }
    if(tick==40) {
        if(kind==10) {
            BtlMember_GetActiveGauge(a)->ki=BtlMember_GetActiveGauge(a)->kiMax;
            Port_TagTestCheck(BtlAct_CanTransform((BtlActJChr *)a,0,1,1),710);
            BtlAct_QueueTransform((BtlActJChr *)a,0);
            BtlAct_Request(a,BtlAct_GetQueued(a));
        } else if(kind==13) {
            BtlMgrChr *b=BtlChar_Get(3);
            Vec4 p=((BtlMovePose *)&a->pose)->pos,r={0,0,0,0};
            BtlChar_Place(a,&p,&r,BtlStage_FindZoneNear(-1,&p));
            p.z+=75;r.y=3.14159265f;
            BtlChar_Place(b,&p,&r,BtlStage_FindZoneNear(-1,&p));
            while(TagTeam_Target(0)!=3)TagTeam_CycleTarget(0);
            /* Destruction requires a living target below half health. Keep
               enough HP to survive the bomb's earlier multi-hit ticks. */
            BtlMember_GetActiveGauge(b)->healthMax=200000;
            BtlMember_GetActiveGauge(b)->health=90000;
            BtlMember_GetActiveGauge(a)->ki=BtlMember_GetActiveGauge(a)->kiMax;
            BtlAct_Request(a,BtlAct_GetTechniqueAction((BtlActJChr *)a,4));
        } else BtlScene_RequestStageChange();
    }
    if(tick>40) {
        if(kind==10) {
            s32 priority=BtlCam_GetPriorityView();
            if(BtlMember_GetActive(a)->chara==original) {
                extern void *DemoCam_EnterFighter(s32);
                extern void DemoCam_LeaveFighter(void *);
                extern f32 DemoCam_GetTime(void);
                void *saved=DemoCam_EnterFighter(0);f32 time=DemoCam_GetTime();
                if(time>cameraTime)cameraTime=time;DemoCam_LeaveFighter(saved);
            }
            if(priority==0) {
                Vec4 eye,rot;
                extern void BtlCharApi_GetCamPose(s32,Vec4 *,Vec4 *);
                BtlCharApi_GetCamPose(a->objId,&eye,&rot);
                if(!seen)cameraStart=eye;
                else cameraTravel+=__builtin_fabsf(eye.x-cameraStart.x)+
                    __builtin_fabsf(eye.y-cameraStart.y)+__builtin_fabsf(eye.z-cameraStart.z);
                seen++;
                if(seen>2)for(i=1;i<TagTeam_Count();i++)
                    Port_TagTestCheck(TagTeam_CinematicPaused(i),711);
            }
            if(tick%5==0) {
                Vec4 eye,rot;
                extern void BtlCharApi_GetCamPose(s32,Vec4 *,Vec4 *);
                BtlCharApi_GetCamPose(a->objId,&eye,&rot);
                Port_TagSpawnTrace(tick,0,(u32)&eye);
                Port_TagSpecialTrace(tick,priority,TagTeam_CinematicPaused(1),BtlMember_GetActive(a)->chara,a->action);
            }
            if(BtlMember_GetActive(a)->chara!=original&&!BtlCharApi_IsChanging(a->objId)&&priority<0)settled++;
            if(settled==30) {
                /* B14's ordinary power-ups are shorter than the Great Ape
                   movie. Require a progressing camera, not the ape's length. */
                Port_TagSpecialTrace(tick,seen,(s32)cameraTime,original,BtlMember_GetActive(a)->chara);
                Port_TagTestCheck(seen>10&&(cameraTime>0||cameraTravel>1.0f),712);
                for(i=0;i<TagTeam_Count();i++)Port_TagTestCheck(!TagTeam_CinematicPaused(i),713);
                Port_TagTestCheck(1,99);
            }
        } else {
            if(loading) {
                loadFrames++;
                for(i=0;i<TagTeam_Count();i++) {
                    Vec4 p=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos;
                    if(wasLoading)Port_TagTestCheck(p.x==positions[i].x&&p.y==positions[i].y&&p.z==positions[i].z,720);
                    positions[i]=p;
                    if(!wasLoading)hp[i]=BtlMember_GetActiveGauge(BtlChar_Get(i))->health;
                    Port_TagTestCheck(hp[i]==BtlMember_GetActiveGauge(BtlChar_Get(i))->health,721);
                }
                if(DemoCam_IsActive()) {
                    viewFrames++;
                    if(viewFrames>2)Port_TagTestCheck(BtlScene_IsSingleView(),722);
                }
            } else if(wasLoading) {
                /* Check the loader's placements before CPUs resume movement;
                   thirty updates later they may already be dodging or flying. */
                for(i=0;i<TagTeam_Count();i++) {
                    extern s32 TagPosition_SafeSpot(s32,Vec4 *);
                    Vec4 p=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos;
                    Port_TagSpawnTrace(tick,i,(u32)&p);
                    Port_TagTestCheck(TagPosition_SafeSpot(i,&p),725);
                }
            }
            wasLoading=loading;
            if(loadFrames&&!loading&&!DemoCam_IsActive())settled++;
            if(tick%15==0)Port_TagSpecialTrace(tick,loading,viewFrames,Battle_GetStage(),EftBurst_IsBusy());
            if(kind==13 && tick%30==0)
                Port_TagClashTrace(tick,0,TagTeam_Target(0),BtlMember_GetActiveGauge(BtlChar_Get(3))->health,a->action);
            if(settled==30) {
                Port_TagTestCheck(Battle_GetStage()!=original&&loadFrames>150&&viewFrames>100,723);
                Port_TagTestCheck(!BtlScene_IsSingleView(),724);
                Port_TagTestCheck(1,99);
            }
        }
    }
    if(tick==1000)Port_TagTestCheck(0,729);
}
/* Exercise the actual tournament or a reloaded gauntlet wave. The fixture
   compares the final viewport matrix with the native authored camera. */
#include "battle/tag_challenge.h"
#include "battle/btl_demo_cam.h"
#include "battle/btl_obj.h"
static s32 modeRushTick,modeRushWave=-1,modeRushTarget,modeRushPaired,modeRushCamera,modeRushSettle;
void TagModeRush_TestTick(void) {
    s32 i,w=TagChallenge_Wave();BtlMgrChr *a,*b;
    extern s32 Port_TagModeRushTest(void),Port_TagSpecialSlot(void);
    if(!TagTeam_Active() || !BtlSeq_IsFighting() || TagChallenge_BetweenWaves() ||
       (Battle_GetWork()->flags&BATTLE_FLAG_PAUSE))return;
    if(w!=modeRushWave) {modeRushWave=w;modeRushTick=modeRushPaired=modeRushCamera=modeRushSettle=0;}
    modeRushTick++;
    if(modeRushTick==30) {
        modeRushTarget=-1;
        for(i=0;i<TagTeam_Count();i++)if(TagTeam_Team(i)==1 && TagTeam_IsAlive(i)) {modeRushTarget=i;break;}
        Port_TagTestCheck(modeRushTarget>=0,2250);
        Port_TagOptionSet(TAG_OPT_SHARED_RUSH,Port_TagModeRushTest()==2);
        Port_TagOptionSet(TAG_OPT_LOCAL_CINEMATICS,1);
        for(i=0;i<TagTeam_Count();i++)if(TagTeam_IsAlive(i)) {
            BtlMgrChr *c=BtlChar_Get(i);StgGroundHit ground;
            Vec4 p={i==0||i==modeRushTarget?0:400.0f,0,i==modeRushTarget?70.0f:0,1};
            Vec4 r={0,i==modeRushTarget?3.14159265f:0,0,0};
            p.y=BtlStage_GetTop()+1;BtlStage_ProbeGround(-1,&p,&ground);p.y=ground.y;
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));c->freeze=c->freezeNext=0;
            BtlMember_GetActiveGauge(c)->health=100000;
            BtlMember_GetActiveGauge(c)->ki=BtlMember_GetActiveGauge(c)->kiMax;
        }
        for(i=0;i<TagTeam_Count() && TagTeam_Target(0)!=modeRushTarget;i++)TagTeam_CycleTarget(0);
    }
    if(modeRushTick<35)return;
    a=BtlChar_Get(0);b=BtlChar_Get(modeRushTarget);
    if(modeRushTick==35) {
        s32 slot=Port_TagSpecialSlot();
        BtlAct_Request(a,BtlAct_GetTechniqueAction((BtlActJChr *)a,slot>=0?slot:3));
    }
    if(BtlCharApi_IsInRushSequence(a->objId)) {
        modeRushPaired++;
        for(i=0;i<TagTeam_Count();i++)if(i!=0 && i!=modeRushTarget) {
            Port_TagTestCheck(!TagTeam_CanContact(i,0)&&!TagTeam_CanContact(i,modeRushTarget),2257);
        }
        Port_TagTestCheck(TagTeam_Target(modeRushTarget)==0,2251);
        if(DemoCam_FighterActive(0))Port_TagTestCheck(TagTeam_CameraActor(modeRushTarget)==0,2252);
    }
    if(modeRushTick>150 && a->action<0x100 && b->action<0x100 &&
       !DemoCam_FighterActive(0))modeRushSettle++;else modeRushSettle=0;
    if(modeRushSettle==30) {
        Port_TagTestCheck(modeRushPaired>0 && modeRushCamera>5,2253);
        Port_TagTestCheck(BtlMember_GetActiveGauge(b)->health<100000,2254);
        for(i=0;i<TagTeam_Count();i++)if(i!=0 && i!=modeRushTarget && TagTeam_IsAlive(i))
            Port_TagTestCheck(TagTeam_CanContact(i,0)&&TagTeam_CanContact(i,modeRushTarget),2258);
        if(!TagChallenge_Active() || w>=2)Port_TagTestCheck(1,99);
        for(i=0;i<TagTeam_Count();i++)if(TagTeam_Team(i)==1)BtlMember_GetActiveGauge(BtlChar_Get(i))->health=0;
    }
    if(modeRushTick==1200)Port_TagTestCheck(0,2255);
}
void TagModeRush_CheckCameras(s32 shared) {
    s32 v,owner;
    extern s32 Port_TagModeRushTest(void);
    extern void *DemoCam_EnterFighter(s32);
    extern void DemoCam_LeaveFighter(void *);
    if(!Port_TagModeRushTest() || modeRushTick<36 || !BtlSeq_IsFighting())return;
    for(v=0;v<(shared?1:TagTeam_VisibleViews());v++) {
        s32 i;void *saved;DemoCam *d;BtlCamView *view=BtlCam_GetNativeView(v);
        owner=shared && BtlCam_GetPriorityView()>=0?BtlCam_GetPriorityView():TagTeam_CameraActor(TagTeam_ViewActor(v));
        if(owner!=0 || !DemoCam_FighterActive(owner))continue;
        saved=DemoCam_EnterFighter(owner);d=DemoCam_Get();
        for(i=0;i<16;i++)Port_TagTestCheck(__builtin_fabsf(((f32 *)&view->view.world2view2)[i]-((f32 *)&d->view.world2view2)[i])<0.001f,2256);
        DemoCam_LeaveFighter(saved);modeRushCamera++;
    }
}
