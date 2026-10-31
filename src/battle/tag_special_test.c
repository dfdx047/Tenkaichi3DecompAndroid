/* Real move startup, visibility and pause checks. Opt-in cinema cases 4..9. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_act_j.h"
#include "battle/stg_a.h"
#include "battle/btl_cam.h"
#include "battle/btl_act_g.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern BtlMgrObj *BtlChar_GetObj(BtlMgrChr *);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *),BtlChar_TestFlag(BtlMgrChr *,s32);
extern s32 BtlChar_IsFrozen(BtlMgrChr *),BtlCharApi_HasCamPriority(s32);
extern s32 Port_TagCinemaTest(void);
extern void Port_TagTestCheck(s32,s32),Port_TagSpecialTrace(s32,s32,s32,s32,s32);
void TagSpecial_TestTick(s32 tick) {
    static s32 startup,paused,damage,settled;
    s32 i,kind=Port_TagCinemaTest(),shared=kind==6||kind==7;
    BtlMgrChr *a=BtlChar_Get(0);
    if(tick==30) {
        Port_TagOptionSet(TAG_OPT_LOCAL_CINEMATICS,1);
        Port_TagOptionSet(TAG_OPT_SHARED_SPECIAL,kind==6);
        Port_TagOptionSet(TAG_OPT_SHARED_RUSH,0);
        Port_TagOptionSet(TAG_OPT_SHARED_ULTIMATE,1);
        Port_TagOptionSet(TAG_OPT_CINEMATIC_PAUSE,kind==8?0:2);
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
            c->freeze=c->freezeNext=c->freezeDelay=0;
        }
        while(TagTeam_Target(0)!=3) TagTeam_CycleTarget(0);
        while(TagTeam_Target(2)!=1) TagTeam_CycleTarget(2);
    }
    if(tick==35) {
        extern s32 Port_TagSpecialSlot(void);
        s32 slot=Port_TagSpecialSlot();
        s32 action=BtlAct_GetTechniqueAction((BtlActJChr *)a,slot>=0?slot:kind==5?3:kind==7?4:2);
        Port_TagSpecialTrace(tick,action,0,1,a->action);
        BtlAct_Request(a,action);
    }
    if(kind==9 && tick==40) {
        BtlMgrChr *c=BtlChar_Get(2);
        BtlAct_Request(c,BtlAct_GetTechniqueAction((BtlActJChr *)c,2));
    }
    if(tick>35) {
        extern s32 DemoCam_IsActive(void);
        if(!shared) {Port_TagTestCheck(BtlCam_GetPriorityView()<0,682);Port_TagTestCheck(!DemoCam_IsActive(),683);}
        s32 flags=BtlChar_TestFlag(a,0x125)|BtlChar_TestFlag(a,0x126)*2|BtlChar_TestFlag(a,0x12A)*4;
        if(tick%15==0) Port_TagSpecialTrace(tick,flags,BtlChar_IsFrozen(a),1,a->action);
        if(tick%60==0) for(i=0;i<4;i+=3) {
            extern s32 BtlAnim_GetId(BtlMgrChr *);
            extern f32 BtlAnim_GetFrame(BtlMgrChr *);
            extern void Port_TagMotionTrace(s32,s32,u32);
            BtlMgrChr *c=BtlChar_Get(i);ActGChr *g=(ActGChr *)c;
            s32 v[18]={BtlAnim_GetId(c),0,c->action,BtlChar_IsFrozen(c),BtlChar_TestFlag(c,0x31),
                g->work[0],g->work[1],g->work[2],g->work[3],g->thr.tech,g->thr.unk10,
                g->thr.unk18,g->thr.unk20,g->thr.unk1C,g->thr.unk48,g->thr.unk4C,g->thr.atkSide,g->thr.defSide};
            *(f32 *)&v[1]=BtlAnim_GetFrame(c);Port_TagMotionTrace(tick,i,(u32)v);
        }
        if(flags) startup++;
        /* Actor 2 is neither the attacker nor its victim. Native per-hit
           reactions on the actual target must remain intact. */
        if(kind!=9 && tick<150) {
            BtlMgrChr *other=BtlChar_Get(2);
            s32 frozen=BtlChar_IsFrozen(other);
            s32 visible=(BtlChar_GetObj(other)->flags&2)!=0;
            if(flags||frozen||!visible) Port_TagSpecialTrace(tick,flags,frozen,visible,other->action);
            Port_TagTestCheck(visible,670);
            if(!shared) Port_TagTestCheck(!frozen,671);
            if(frozen) paused++;
        }
        if(kind==9 && tick<100) {
            /* A second special must not be held behind the first one's
               level-2 global freeze request. */
            Port_TagTestCheck(!BtlChar_IsFrozen(BtlChar_Get(2)),672);
            Port_TagTestCheck((BtlChar_GetObj(BtlChar_Get(1))->flags&2)!=0,673);
        }
        if(BtlMember_GetActiveGauge(BtlChar_Get(3))->health<100000) damage=1;
        if(tick>150 && a->action<0x100 && !BtlCharApi_HasCamPriority(a->objId)) settled++;
        else settled=0;
        if(settled==40) {
            Port_TagTestCheck(startup>0,674);
            Port_TagTestCheck(damage,675);
            if(shared) Port_TagTestCheck(paused>0,676);
            for(i=0;i<4;i++) Port_TagTestCheck(!TagTeam_CinematicPaused(i),677+i);
            Port_TagTestCheck(1,99);
        }
    }
    if(tick==1200) Port_TagTestCheck(0,681);
}
