/* Opt-in integration fixture: normal grab animations and native camera cuts,
   with a victim who was targeting a different opponent. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_cam.h"
#include "battle/btl_char_coll.h"
#include "battle/bobj_a.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern void BtlAct_Request(BtlMgrChr *,s32),BtlAct_ClearQueue(BtlMgrChr *);
extern s32 BtlAct_GetCurrent(BtlMgrChr *),BtlAnim_GetId(BtlMgrChr *);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *),BtlCharApi_HasCamPriority(s32);
extern s32 TagTeam_CameraActor(s32),TagTeam_ShareClashCamera(s32);
extern void Port_TagTestCheck(s32,s32),Port_TagClashTrace(s32,s32,s32,s32,s32);
extern s32 Port_TagCameraTest(void);
extern BobjObj *BtlChar_GetObj(BtlMgrChr *);
extern f32 BtlCharApi_GetRadius(s32),BtlCharApi_GetCenterHeight(s32);
extern void BtlCharApi_GetNodePos(s32,s32,Vec4 *);
extern s32 BtlChar_TestFlag(BtlMgrChr *,s32);
extern void BtlChar_SetHeldFlag(BtlMgrChr *,s32);
extern void Port_TagTestPositions(u32,u32);

static void grab_contact_fixture(BtlMgrChr *a,BtlMgrChr *b) {
    BobjObj *o=BtlChar_GetObj(a),*v=BtlChar_GetObj(b);
    BobjWork *w=o->work;
    if(BtlAct_GetCurrent(a)!=0xB4)return;
    if(w->atkSphereCount || w->atkObbCount) {
        Vec4 p=((BtlMovePose *)&b->pose)->pos,r={0,3.14159265f,0,0};
        Vec4 contact=w->atkSphereCount?w->atkSphere[0].center:w->atkObb[0].center;
        p.x+=contact.x-v->body.cur->center.x;
        p.y+=contact.y-v->body.cur->center.y;
        p.z+=contact.z-v->body.cur->center.z;
        BtlChar_Place(b,&p,&r,BtlStage_FindZoneNear(-1,&p));
    }
}
static s32 check_hold(s32 holder,s32 victim) {
    BtlMgrChr *a=BtlChar_Get(holder),*b=BtlChar_Get(victim);
    Vec4 p,q;f32 x,y,z;
    if(!BtlChar_TestFlag(a,0x8B)||!BtlChar_TestFlag(b,0x97)||BtlChar_GetObj(b)->anim.blend>0)return 0;
    BtlCharApi_GetNodePos(a->objId,0x34,&p);
    BtlCharApi_GetNodePos(b->objId,BtlChar_GetObj(b)->hit.node,&q);
    x=p.x-q.x;y=p.y-q.y;z=p.z-q.z;
    /* Check the final rendered pose, including subsequent gravity/terrain
       corrections. A detached actor is hundreds of units away. */
    if(x*x+y*y+z*z>=100)Port_TagTestPositions((u32)&p,(u32)&q);
    Port_TagTestCheck(x*x+y*y+z*z<100,470+holder);
    return 1;
}

/* Run on the successful native contact, before action dispatch can turn the
   pending catch into an animation. Exercise the late effect callbacks too. */
void TagCamera_TestContact(s32 attacker,s32 victim,s32 rush) {
    extern s32 Port_TagContactTest(void);
    extern s32 BtlColl_TryDodge(s32,BtlCollHit *),BtlColl_TryGuard(s32,BtlCollHit *),BtlColl_Hit(s32,BtlCollHit *);
    s32 i,j;
    if(!Port_TagContactTest() || !TagTeam_Active())return;
    Port_TagClashTrace(0,attacker,victim,rush,BtlAct_GetCurrent(BtlChar_Get(attacker)));
    Port_TagTestCheck(TagTeam_CanContact(attacker,victim),460);
    for(i=0;i<TagTeam_Count();i++)if(i!=attacker && i!=victim)for(j=0;j<2;j++) {
        s32 actor=j?victim:attacker;
        BtlCollChr *c=(BtlCollChr *)BtlChar_Get(actor);
        BtlCollHit hit;
        s32 hp=BtlMember_GetActiveGauge((BtlMgrChr *)c)->health,reaction=c->react.id;
        memset(&hit,0,sizeof(hit));hit.ownerId=i;hit.type=BTL_HIT_TECH;
        Port_TagTestCheck(!TagTeam_CanContact(i,actor)&&!TagTeam_CanContact(actor,i),461);
        /* An unbound effect is safe if a regression lets it through, but must
           be consumed before any guard/dodge/reaction processing. */
        Port_TagTestCheck(BtlColl_TryDodge(c->objId,&hit),462);
        Port_TagTestCheck(BtlColl_TryGuard(c->objId,&hit),463);
        Port_TagTestCheck(BtlColl_Hit(c->objId,&hit),464);
        Port_TagTestCheck(c->react.id==reaction && BtlMember_GetActiveGauge((BtlMgrChr *)c)->health==hp,465);
    }
}

void TagCamera_TestTick(s32 tick) {
    static s32 caught,priority,finished,backstepped,attacked,overlaps;
    static s32 heldFrames,simultaneous;
    BtlMgrChr *a=BtlChar_Get(0),*b=BtlChar_Get(3);
    s32 i;
    if(tick==30) {
        if(Port_TagCameraTest()==4)Port_TagOptionSet(TAG_OPT_SHARED_GRAB,1);
        for(i=0;i<4;i++) {
            BtlMgrChr *c=BtlChar_Get(i);
            Vec4 p={i==1?300.0f:i==2?-300.0f:0.0f,0, i==3?22.0f:0.0f,1};
            Vec4 r={0,i==3?3.14159265f:0,0,0};
            if(Port_TagCameraTest()>=5) {
                p.y=-350;
                if(i==3) {
                    p.y+=BtlCharApi_GetCenterHeight(b->objId)-BtlCharApi_GetCenterHeight(a->objId);
                    p.z=BtlCharApi_GetRadius(a->objId)+BtlCharApi_GetRadius(b->objId)+12;
                }
                if(Port_TagCameraTest()==6 && i==2) {p.x=300;p.z=22;r.y=3.14159265f;}
            }
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
            if(Port_TagCameraTest()>=5)BtlChar_SetHeldFlag(c,0xE);
            c->freeze=c->freezeNext=0;
            BtlMember_GetActiveGauge(c)->health=30000;
        }
        while(TagTeam_Target(0)!=3) TagTeam_CycleTarget(0);
        while(TagTeam_Target(3)!=2) TagTeam_CycleTarget(3);
        if(Port_TagCameraTest()==6) {
            while(TagTeam_Target(1)!=2)TagTeam_CycleTarget(1);
            while(TagTeam_Target(2)!=1)TagTeam_CycleTarget(2);
        }
    }
    if(tick==35) {
        BtlAct_Request(a,0xB4);
        if(Port_TagCameraTest()==6)BtlAct_Request(BtlChar_Get(1),0xB4);
    }
    if(Port_TagCameraTest()>=5 && !caught && tick>35 && tick<100) {
        /* Put the receiving fighter in the live native grab volume. This
           exercises the real collision/action handshake, without conflating
           attachment with a test fighter falling out of range beforehand. */
        grab_contact_fixture(a,b);
    }
    if(Port_TagCameraTest()==6 && tick>35 && tick<100)grab_contact_fixture(BtlChar_Get(1),BtlChar_Get(2));
    if(Port_TagCameraTest()>=5) {
        s32 held=check_hold(0,3);
        heldFrames+=held;
        if(Port_TagCameraTest()==6)simultaneous+=check_hold(1,2)&&held;
    }
    if(tick==40 && Port_TagCameraTest()==2)BtlAct_Request(b,0x1B);
    if(tick==50 && Port_TagCameraTest()==2) {
        /* Intercept a moving backstep after its initial evade window. Use the
           real catch collision, not a forced throw state. */
        BtlMovePose *q=(BtlMovePose *)&b->pose;
        Vec4 p=q->pos,r={0,0,0,0};
        p.x+=q->dir.x*q->speed*2;p.z+=q->dir.z*q->speed*2-10;
        BtlChar_Place(a,&p,&r,BtlStage_FindZoneNear(-1,&p));
    }
    if(BtlAct_GetCurrent(b)==0x1B)backstepped=1;
    if(tick>35 && tick%15==0) {
        Port_TagClashTrace(tick,0,TagTeam_CameraActor(0),BtlAnim_GetId(a),BtlAct_GetCurrent(a));
        Port_TagClashTrace(tick,3,TagTeam_CameraActor(3),BtlAnim_GetId(b),BtlAct_GetCurrent(b));
        if(Port_TagCameraTest()>=5) {
            BobjObj *o=BtlChar_GetObj(a);Vec4 p,q;
            BtlCharApi_GetNodePos(a->objId,0x34,&p);
            BtlCharApi_GetNodePos(b->objId,BtlChar_GetObj(b)->hit.node,&q);
            Port_TagTestPositions((u32)&p,(u32)&q);
            Port_TagClashTrace(tick,BtlChar_TestFlag(a,0x8B),BtlChar_TestFlag(b,0x97),o->work->atkSphereCount,o->work->atkObbCount);
        }
    }
    if(BtlAct_GetCurrent(a)==0xB7 || BtlAct_GetCurrent(a)==0xBB) {
        if(!caught) {
            caught=1;
            Port_TagTestCheck(TagTeam_Target(0)==3 && TagTeam_Target(3)==0,400);
            if(Port_TagCameraTest()==2)Port_TagTestCheck(backstepped,415);
        }
        if(BtlCharApi_HasCamPriority(a->objId)) {
            priority++;
            Port_TagTestCheck(TagTeam_CameraActor(3)==0,401);
            {
                BtlMovePose *p=(BtlMovePose *)&a->pose,*q=(BtlMovePose *)&b->pose;
                f32 x=p->pos.x-q->pos.x,y=p->pos.y-q->pos.y,z=p->pos.z-q->pos.z;
                if(Port_TagCameraTest()<5)Port_TagTestCheck(x*x+y*y+z*z<150*150,416);
            }
            Port_TagTestCheck(!TagTeam_CinematicPaused(0)&&!TagTeam_CinematicPaused(3),402);
            if(priority==1 && Port_TagCameraTest()!=4) {
                Port_TagOptionSet(TAG_OPT_SHARED_GRAB,1);
                Port_TagTestCheck(TagTeam_ShareClashCamera(0)&&BtlCam_GetPriorityView()==0,403);
                Port_TagOptionSet(TAG_OPT_SHARED_GRAB,0);
                Port_TagTestCheck(!TagTeam_ShareClashCamera(0)&&BtlCam_GetPriorityView()<0,404);
            }
        }
    } else if(caught && BtlAct_GetCurrent(a)==0xB && BtlAct_GetCurrent(b)==0xB) finished++;
    if(caught && Port_TagCameraTest()>=3 && BtlAct_GetCurrent(a)==0xB7) {
        if(Port_TagCameraTest()==4 && priority>2)
            Port_TagTestCheck(TagTeam_CinematicPaused(1)&&TagTeam_CinematicPaused(2),417);
        if(Port_TagCameraTest()==3) {
            /* A teammate punches through the held victim while the other
               opponent punches the grabber. Real native active hit volumes. */
            for(i=1;i<=2;i++) {
                BtlMgrChr *c=BtlChar_Get(i),*victim=i==1?a:b;
                Vec4 p=((BtlMovePose *)&victim->pose)->pos,r={0,0,0,0};
                p.z-=18;
                BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
                while(TagTeam_Target(i)!=victim->side)TagTeam_CycleTarget(i);
                {
                    extern s32 BtlBodyHit_Test(void *,void *);
                    if(BtlBodyHit_Test(BtlChar_GetObj(c),BtlChar_GetObj(victim)))overlaps++;
                }
                Port_TagTestCheck(BtlMember_GetActiveGauge(a)->health==30000,421);
                if(priority%18==1) {BtlAct_ClearQueue(c);BtlAct_Request(c,0x44);attacked++;}
            }
            Port_TagTestCheck(!TagTeam_CanContact(1,0)&&!TagTeam_CanContact(2,3),418);
            Port_TagTestCheck(TagTeam_CanContact(0,3),419);
        }
    }
    if(finished==15) {
        if(Port_TagCameraTest()>=5)Port_TagTestCheck(heldFrames>10,472);
        if(Port_TagCameraTest()==6)Port_TagTestCheck(simultaneous>5,473);
        if(Port_TagCameraTest()==3)Port_TagTestCheck(attacked>0&&overlaps>0,420);
        Port_TagTestCheck(priority>0,405);
        Port_TagTestCheck(TagTeam_CameraActor(0)==0&&TagTeam_CameraActor(3)==3,406);
        for(i=0;i<4;i++) Port_TagTestCheck(!TagTeam_CinematicPaused(i),410+i);
        Port_TagTestCheck(TagTeam_CanContact(1,0)&&TagTeam_CanContact(2,3),466);
        Port_TagTestCheck(1,99);
    }
    if(tick==450) Port_TagTestCheck(0,409);
}
