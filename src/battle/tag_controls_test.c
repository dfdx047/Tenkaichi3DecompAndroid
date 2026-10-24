/* Runs against the real native roster. No mock fighter memory or emulator. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_clash.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_cam.h"
#include "battle/tag_options.h"
#include "battle/tag_balance.h"
#include "battle/tag_cpu.h"
#include "battle/tag_position.h"
#include "battle/btl_char_coll.h"
#include "battle/stg_a.h"
#include "sys/pad.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern void BtlChar_SetHeldFlag(BtlMgrChr *,s32), BtlChar_ClearFlag(BtlMgrChr *,s32);
extern void BtlAct_Request(BtlMgrChr *,s32), BtlAct_ClearQueue(BtlMgrChr *);
extern s32 BtlAct_GetCurrent(BtlMgrChr *);
extern BtlCamView *BtlCam_GetNativeView(s32);
extern void Port_TagTestCheck(s32,s32);
extern s32 BtlHit_CheckClash(void);
extern s32 Port_TagControlsTest(void);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);

static void position_checks(void) {
    BtlMgrChr *a=BtlChar_Get(0),*b=BtlChar_Get(3);
    BtlMovePose *ap=(BtlMovePose *)&a->pose,*bp=(BtlMovePose *)&b->pose;
    BtlCollThrow at=((BtlCollChr *)a)->thr,bt=((BtlCollChr *)b)->thr;
    Vec4 oldA=ap->pos,oldB=bp->pos,ar=ap->rot,br=bp->rot,delta;
    Vec4 p={100,-100,40,1},q={0,-60,0,1},r={0,0,0,0};
    s32 aa=a->action,ba=b->action;
    Port_TagOptionSet(TAG_OPT_LOCAL_CINEMATICS,1);
    BtlChar_Place(a,&p,&r,BtlStage_FindZoneNear(-1,&p));a->action=236;
    TagPosition_Place(0,&q,&r,BtlStage_FindZoneNear(-1,&q),2);
    Port_TagTestCheck(ap->pos.x>99.9f && ap->pos.x<100.1f &&
        ap->pos.y>-100.1f && ap->pos.y<-99.9f,350);
    q.x=10;
    TagPosition_Place(0,&q,&r,BtlStage_FindZoneNear(-1,&q),2);
    TagPosition_Place(0,&q,&r,BtlStage_FindZoneNear(-1,&q),2);
    Port_TagTestCheck(ap->pos.x>109.9f&&ap->pos.x<110.1f,351);
    /* Recovery fixes invalid/out-of-arena poses, then gives placement back. */
    ap->pos.x=1000000;a->action=11;
    TagPosition_Update();TagPosition_Update();
    Port_TagTestCheck(ap->pos.x>99.9f&&ap->pos.x<100.1f&&!TagPosition_Offset(0,&delta),352);
    TagPosition_Reset();
    p.x=-80;p.y=-100;p.z=40;
    BtlChar_Place(b,&p,&r,BtlStage_FindZoneNear(-1,&p));
    a->action=313;b->action=301;
    ((BtlCollChr *)a)->thr.atkSide=((BtlCollChr *)b)->thr.atkSide=3;
    ((BtlCollChr *)a)->thr.defSide=((BtlCollChr *)b)->thr.defSide=0;
    q.x=0;q.y=-60;q.z=0;
    TagPosition_Place(0,&q,&r,BtlStage_FindZoneNear(-1,&q),2);
    q.x=20;
    TagPosition_Place(3,&q,&r,BtlStage_FindZoneNear(-1,&q),2);
    Port_TagTestCheck(ap->pos.x>-80.1f&&ap->pos.x<-79.9f&&
        bp->pos.x-ap->pos.x>19.9f&&bp->pos.x-ap->pos.x<20.1f,353);
    Port_TagTestCheck(TagPosition_Offset(0,&delta)&&delta.x>-80.1f&&delta.x<-79.9f,354);
    /* Switching to the high-centre staging opcode adjusts an already placed
       partner by the same offset, including before its next own update. */
    q.x=40;
    TagPosition_Place(3,&q,&r,BtlStage_FindZoneNear(-1,&q),3);
    Port_TagTestCheck(bp->pos.x>-80.1f&&bp->pos.x<-79.9f&&
        ap->pos.x>-120.1f&&ap->pos.x<-119.9f,356);
    a->action=b->action=11;ap->pos.y=100;
    TagPosition_Update();TagPosition_Update();
    {
        StgGroundHit ground;Vec4 probe=ap->pos;probe.y-=10;
        BtlStage_ProbeGround(-1,&probe,&ground);
        Port_TagTestCheck(ap->pos.y<=ground.y+.01f&&!TagPosition_Offset(3,&delta),355);
    }
    TagPosition_Reset();Port_TagOptionSet(TAG_OPT_LOCAL_CINEMATICS,0);
    a->action=aa;b->action=ba;((BtlCollChr *)a)->thr=at;((BtlCollChr *)b)->thr=bt;
    BtlChar_Place(a,&oldA,&ar,BtlStage_FindZoneNear(-1,&oldA));
    BtlChar_Place(b,&oldB,&br,BtlStage_FindZoneNear(-1,&oldB));
}

static void spectator_test(void) {
    s32 i,target[4],before=TagTeam_ControlActor(0);
    for(i=0;i<4;i++) {
        target[i]=TagTeam_Target(i);
        Port_TagTestCheck(TagTeam_Player(i)<0 && BtlChar_Get(i)->injectOn && TagTeam_Side(i)->control==2,324);
    }
    Port_TagTestCheck(TagTeam_Spectating(0) && TagTeam_ViewCount()==1,325);
    TagTeam_CycleSpectator(0);
    Port_TagTestCheck(TagTeam_ViewActor(0)==1,326);
    BtlMember_GetActiveGauge(BtlChar_Get(2))->health=0;
    Port_TagOptionSet(TAG_OPT_SPECTATE_FALLEN,0);
    TagTeam_CycleSpectator(0);
    Port_TagTestCheck(TagTeam_ViewActor(0)==3,327);
    Port_TagOptionSet(TAG_OPT_SPECTATE_FALLEN,1);
    TagTeam_CycleSpectator(0);TagTeam_CycleSpectator(0);TagTeam_CycleSpectator(0);
    Port_TagTestCheck(TagTeam_ViewActor(0)==2,328);
    Port_TagTestCheck(TagTeam_TakeoverCandidate(0)<0 && !TagTeam_Takeover(0) &&
        TagTeam_ControlActor(0)==before,329);
    for(i=0;i<4;i++) Port_TagTestCheck(TagTeam_Player(i)<0 && TagTeam_Target(i)==target[i],330);
    Port_TagTestCheck(1,99);
}

/* Land a human and a CPU on real out-of-ring ground, then keep the rest
   of the team alive long enough to catch the old get-up/attack regression. */
static void ring_out_test(s32 tick) {
    s32 i;extern s32 BtlChar_TestFlag(BtlMgrChr *,s32),BtlChar_IsDead(BtlMgrChr *),BtlChar_IsFree(BtlMgrChr *);
    extern s32 TagRevive_Nearby(s32),TagRevive_Progress(s32);
    for(i=2;i<4;i++)BtlChar_Get(i)->freezeNext=2;
    if(tick==30)for(i=0;i<2;i++) {
        BtlMgrChr *c=BtlChar_Get(i);Vec4 p={250,-1000,0,1},r={0,0,0,0};StgGroundHit ground;
        p.z=i?60:-60;BtlStage_ProbeGround(-1,&p,&ground);p.y=ground.y;
        BtlMember_GetActiveGauge(c)->health=30000;
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xD9);
        /* Normal knock-downs disable the tournament's automatic low-altitude
           flight. Keep that flag while this fixture drops them onto the grass. */
        BtlChar_SetHeldFlag(c,0x2A);
        BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
    }
    if(tick==60)for(i=0;i<2;i++) {
        extern void Port_TagTestPositions(u32,u32),Port_TagTestState(s32,s32,s32,s32);
        BtlMgrChr *c=BtlChar_Get(i);BtlMovePose *p=(BtlMovePose *)&c->pose;
        Port_TagTestPositions((u32)&p->pos,(u32)&p->unkB0);
        Port_TagTestState(c->action,BtlChar_TestFlag(c,0xF),Battle_GetStage(),BtlChar_TestFlag(c,7));
        Port_TagTestCheck(BtlChar_TestFlag(c,7),370+i);
        BtlChar_ClearFlag(c,0x2A);
    }
    if(tick==180 || tick==270)for(i=0;i<2;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        Port_TagTestCheck(!TagTeam_IsAlive(i)&&BtlChar_IsDead(c)&&!BtlChar_IsFree(c),372+i);
        Port_TagTestCheck(c->action==0xD8 || c->action==0xD9,374+i);
        Port_TagTestCheck(!TagTeam_CanContact(i,3-i)&&!TagTeam_CanContact(3-i,i),376+i);
        Port_TagTestCheck(TagRevive_Progress(i)==0,378+i);
    }
    if(tick==200) {
        /* Even a queued attack or mash recovery must not stand them back up. */
        for(i=0;i<2;i++)BtlAct_Request(BtlChar_Get(i),0x44);
    }
    if(tick==280) {
        BtlMgrChr *ally=BtlChar_Get(2);ally->freeze=ally->freezeNext=ally->freezeDelay=0;
        Port_TagTestCheck(TagTeam_TakeoverCandidate(0)==2&&TagTeam_Takeover(0),380);
        Port_TagTestCheck(TagTeam_ControlActor(0)==2&&TagTeam_Player(0)<0,381);
        Port_TagTestCheck(!TagTeam_CheckEnd(0),382);
        BtlChar_SetHeldFlag(BtlChar_Get(1),7);BtlChar_SetHeldFlag(BtlChar_Get(3),7);
        Port_TagTestCheck(TagTeam_CheckEnd(0),383);
        Port_TagTestCheck(1,99);
    }
}

void TagControls_TestTick(s32 tick) {
    s32 i, before;
    BtlMgrChr *a=BtlChar_Get(0), *ally=BtlChar_Get(2);
    if(Port_TagControlsTest()==4) {ring_out_test(tick);return;}
    if(Port_TagControlsTest()==3) {
        if(tick==30) {
            Vec4 p={0,0,0,1},r={0,0,0,0};
            for(i=0;i<4;i++) {
                BtlMgrChr *c=BtlChar_Get(i);
                BtlAct_ClearQueue(c);BtlAct_Request(c,i==2?0xD8:0xB);
                if(!(i&1)) {
                    StgGroundHit ground;
                    p.x=0;p.z=i==2?30:0;p.y=-1000;
                    BtlStage_ProbeGround(-1,&p,&ground);p.y=ground.y;
                    BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
                }
                BtlMember_GetActiveGauge(c)->health=i==2?0:30000;
                BtlMember_GetActiveGauge(c)->blast=0;
            }
            for(i=0;i<15;i++)TagTeam_TargetInput(0,1);
            TagTeam_TargetInput(0,0);
        }
        if(tick>30) {
            extern s32 TagRevive_Nearby(s32);
            Port_TagTestCheck(TagTeam_LockedOff(0)&&TagTeam_HudTarget(0)==-1,366);
            Port_TagTestCheck(TagRevive_Nearby(0)==2,367);
        }
        if(tick==240)Port_TagTestCheck(1,99);
        return;
    }
    if (tick!=30) return;
    if(Port_TagControlsTest()==2) {spectator_test();return;}
    for (i=0;i<4;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlMember_GetActiveGauge(c)->health=30000;
        c->freeze=c->freezeNext=0;
        BtlAct_ClearQueue(c); BtlAct_Request(c,0xB);
        BtlChar_ClearFlag(c,7);
    }
    Port_TagTestCheck(TagTeam_Player(0)==0 && TagTeam_Player(2)<0,70);
    position_checks();
    /* A stick flick changes only its own fighter's target, once per neutral.
       A nearby R3 click must still reach transformations without retargeting. */
    {
        Vec4 *p=&((BtlMovePose *)&a->pose)->pos;
        Vec4 *x=&((BtlMovePose *)&BtlChar_Get(1)->pose)->pos;
        Vec4 *y=&((BtlMovePose *)&BtlChar_Get(3)->pose)->pos;
        Mtx44 saved=BtlCam_GetNativeView(0)->view.world2screen;
        Mtx44 *m=&BtlCam_GetNativeView(0)->view.world2screen;
        memset(m,0,sizeof(*m));m->m[0][0]=1;m->m[1][1]=1;m->m[3][0]=2048;m->m[3][1]=2048;m->m[3][3]=1;
        p->x=p->z=0;x->x=0;x->z=100;y->x=100;y->z=0;
        x->y=y->y=0;
        Port_TagOptionSet(TAG_OPT_TARGET_STICK,2);
        TagTeam_TargetStick(0,0);TagTeam_TargetStick(0,0);
        TagTeam_TargetStick(0,PAD_RSTICK_RIGHT);TagTeam_TargetStick(0,PAD_RSTICK_RIGHT);
        Port_TagTestCheck(TagTeam_Target(0)==3 && TagTeam_Target(2)==3 && TagTeam_TargetStickConsumed(0),331);
        for(i=0;i<20;i++)TagTeam_TargetStick(0,PAD_RSTICK_RIGHT);
        Port_TagTestCheck(TagTeam_Target(0)==3,332);
        TagTeam_TargetStick(0,0);TagTeam_TargetStick(0,0);
        TagTeam_TargetStick(0,PAD_RSTICK_LEFT|PAD_R3);TagTeam_TargetStick(0,PAD_RSTICK_LEFT);
        Port_TagTestCheck(TagTeam_Target(0)==3 && !TagTeam_TargetStickConsumed(0),333);
        for(i=0;i<4;i++)TagTeam_TargetStick(0,0);
        TagTeam_TargetStick(0,PAD_RSTICK_LEFT);TagTeam_TargetStick(0,PAD_RSTICK_LEFT);
        Port_TagTestCheck(TagTeam_Target(0)==1,334);
        TagTeam_TargetStick(0,0);
        m->m[0][0]=-1;
        for(i=0;i<4;i++)TagTeam_TargetStick(0,0);
        TagTeam_TargetStick(0,PAD_RSTICK_LEFT);TagTeam_TargetStick(0,PAD_RSTICK_LEFT);
        Port_TagTestCheck(TagTeam_Target(0)==3,361);
        TagTeam_TargetStick(0,0);TagTeam_TargetStick(0,0);
        TagTeam_TargetStick(0,PAD_RSTICK_RIGHT);TagTeam_TargetStick(0,PAD_RSTICK_RIGHT);
        Port_TagTestCheck(TagTeam_Target(0)==1,362);
        *m=saved;
        Port_TagOptionSet(TAG_OPT_TARGET_STICK,1);
    }
    BtlMember_GetActiveGauge(ally)->health=0;
    Port_TagOptionSet(TAG_OPT_OUTNUMBERED,1);
    TagBalance_Update();
    Port_TagTestCheck(TagBalance_Modify(1,0,1000,0)==1150 &&
        TagBalance_Modify(0,1,1000,0)==900 && TagBalance_Recovery(0)==1,335);
    Port_TagTestCheck(TagBalance_Modify(0,2,1000,0)==1000 &&
        TagBalance_Modify(0,0,1000,0)==1000 && TagBalance_Modify(0,1,1000,1)==1000,336);
    before=a->action;a->action=225;
    TagBalance_Update();
    Port_TagTestCheck(TagBalance_Recovery(0)>1.499f && TagBalance_Recovery(0)<1.501f,337);
    Port_TagTestCheck(TagBalance_AgeStep(0)+TagBalance_AgeStep(0)==3,341);
    Port_TagTestCheck(TagBalance_Modify(0,1,1000,0)==0,339);
    Port_TagTestCheck(TagBalance_Modify(0,1,1000,0x2000000)==900,340);
    Port_TagOptionSet(TAG_OPT_OUTNUMBERED,0);
    TagBalance_Update();a->action=before;
    Port_TagTestCheck(a->unkE18==0,342);
    TagTeam_CycleTarget(0);
    Port_TagTestCheck(TagTeam_Target(0)==2,71);
    BtlChar_SetHeldFlag(ally,7);
    TagTeam_CycleTarget(0);
    Port_TagTestCheck(TagTeam_Target(0)==3,72);
    BtlChar_ClearFlag(ally,7);
    Port_TagOptionSet(TAG_OPT_REVIVE,0);
    TagTeam_CycleTarget(0);TagTeam_CycleTarget(0);
    Port_TagTestCheck(TagTeam_Target(0)==3,357);
    Port_TagOptionSet(TAG_OPT_REVIVE,1);
    TagTeam_CycleTarget(0);TagTeam_CycleTarget(0);
    Port_TagTestCheck(TagTeam_Target(0)==2,358);
    BtlMember_GetActiveGauge(ally)->health=7500;
    TagTeam_CycleTarget(0);TagTeam_CycleTarget(0);TagTeam_CycleTarget(0);
    Port_TagTestCheck(TagTeam_Target(0)==3,359);
    BtlMember_GetActiveGauge(ally)->health=0;
    BtlChar_SetHeldFlag(ally,7);
    for (i=0;i<14;i++) TagTeam_TargetInput(0,1);
    Port_TagTestCheck(!TagTeam_LockedOff(0),73);
    TagTeam_TargetInput(0,1);
    Port_TagTestCheck(TagTeam_LockedOff(0) && TagTeam_HudTarget(0)==-1,74);
    a->freeze=3;
    Port_TagTestCheck(TagTeam_LockedOff(0) && TagTeam_HudTarget(0)==-1,363);
    a->freeze=0;
    for (i=0;i<40;i++) TagTeam_TargetInput(0,1);
    TagTeam_TargetInput(0,0);
    Port_TagTestCheck(TagTeam_LockedOff(0),75);
    /* Render coordinates put actor 1 at screen centre and actor 3 off to the
     * side, regardless of which is nearer in the world. */
    {
        Mtx44 saved=BtlCam_GetNativeView(0)->view.world2screen;
        Mtx44 *m=&BtlCam_GetNativeView(0)->view.world2screen;
        BtlMovePose *p=(BtlMovePose *)&BtlChar_Get(1)->pose, *q=(BtlMovePose *)&BtlChar_Get(3)->pose;
        m->m[0][0]=1; m->m[1][0]=m->m[2][0]=0; m->m[3][0]=2048;
        m->m[0][1]=m->m[2][1]=0; m->m[1][1]=1; m->m[3][1]=2048;
        m->m[0][3]=m->m[1][3]=m->m[2][3]=0; m->m[3][3]=1;
        p->pos.x=p->pos.y=0; q->pos.x=200; q->pos.y=0;
        TagTeam_TargetInput(0,1); TagTeam_TargetInput(0,0);
        Port_TagTestCheck(!TagTeam_LockedOff(0) && TagTeam_Target(0)==1,76);
        *m=saved;
    }
    /* L3 remains a separate retaliation gesture with right-stick targeting. */
    Port_TagOptionSet(TAG_OPT_TARGET_STICK,2);
    TagCpu_Damage(0,3,100);
    TagTeam_TargetInput(0,1);TagTeam_TargetInput(0,0);
    Port_TagTestCheck(TagTeam_Target(0)==3 && !TagTeam_LockedOff(0),360);
    TagCpu_Damage(0,1,100);
    a->freeze=3;
    TagTeam_TargetInput(0,1);TagTeam_TargetInput(0,0);
    Port_TagTestCheck(TagTeam_Target(0)==3,364);
    a->freeze=0;TagTeam_TargetInput(0,0);
    Port_TagTestCheck(TagTeam_Target(0)==1,365);
    BtlMember_GetActiveGauge(a)->health=0;
    BtlMember_GetActiveGauge(ally)->health=30000;
    BtlChar_ClearFlag(ally,7);
    Port_TagTestCheck(TagTeam_Spectating(0),320);
    TagTeam_CycleSpectator(0); /* Can watch opponents, but never steal them. */
    Port_TagTestCheck(TagTeam_ViewActor(0)==1 && TagTeam_TakeoverCandidate(0)<0 && !TagTeam_Takeover(0),321);
    TagTeam_CycleSpectator(0);
    Port_TagTestCheck(TagTeam_ViewActor(0)==2,322);
    BtlAct_Request(ally,0x37); /* taking over a charging CPU must preserve its action */
    before=BtlAct_GetCurrent(ally);
    Port_TagTestCheck(TagTeam_TakeoverCandidate(0)==2 && TagTeam_Takeover(0),77);
    Port_TagTestCheck(TagTeam_Player(2)==0 && TagTeam_Player(0)==-1 && ally->injectOn==0 && a->injectOn==1,78);
    Port_TagTestCheck(TagTeam_ViewActor(0)==2 && BtlAct_GetCurrent(ally)==before,79);
    Port_TagTestCheck(!TagTeam_Spectating(0),323);
    Port_TagTestCheck(TagTeam_WinningActor(0)==2,80);
    TagTeam_Reset();
    Port_TagTestCheck(TagTeam_ControlActor(0)==0 && TagTeam_ViewActor(0)==0 && TagTeam_Player(2)==-1,81);
    /* Opponent 1 is selected, but enemy 3 makes the physical contact. */
    for (i=0;i<4;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlMovePose *p=(BtlMovePose *)&c->pose;
        p->pos.x=i==1?1000:0; p->pos.y=0; p->pos.z=i==2?1000:0;
        BtlMember_GetActiveGauge(c)->health=30000;
        BtlChar_SetHeldFlag(c,3); BtlChar_SetHeldFlag(c,0x47);
    }
    Port_TagTestCheck(BtlHit_CheckClash() && BtlClash_Opponent(0)==3 && BtlClash_Opponent(1)==-1,82);
    Port_TagTestCheck(1,99);
}
