/* Teammate beam support uses real actors and their own animation banks. No
   duplicate models or synthetic controller presses enter the fight. */
#include "common.h"
#include "battle/tag_assist.h"
#include "battle/tag_team.h"
#include "battle/tag_clash.h"
#include "battle/tag_fusion.h"
#include "battle/tag_options.h"
#include "battle/tag_position.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/bobj_a.h"
#include "battle/btl_seq.h"
#include "battle/stg_a.h"
#include "battle/eft_h.h"
#include "battle/btl_pool.h"
#include "battle/tag_assist_profiles.h"
#include "sys/pad.h"
extern void *memset(void *,s32,u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern BobjObj *BtlChar_GetObj(BtlMgrChr *);
extern s32 BtlChar_IsFrozen(BtlMgrChr *);
extern s32 BtlSuper_GetType(BtlMgrChr *,s32),BtlSuper_GetHitDirKind(BtlMgrChr *,s32);
extern u32 BtlAnim_GetFlags(s32);
extern void BtlAnim_Play(BtlMgrChr *,s32,f32),BtlAnim_AdvanceLoop(BtlMgrChr *,s32);
extern void BtlAnim_AdvanceThen(BtlMgrChr *,s32,f32,s32);
extern s32 BtlAnim_GetId(BtlMgrChr *),BtlParam_GetDashSound(BtlMgrChr *);
extern void BtlCharSnd_PlayCommon(BtlMgrChr *,s32),BtlChar_SetFxBit(BtlMgrChr *,s32);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern void BtlChar_PoseToObj(BtlMgrChr *,s32),BtlChar_PlayVoice(BtlMgrChr *,s32);
extern void BtlChar_SetFlag(BtlMgrChr *,s32),BtlChar_ClearFlag(BtlMgrChr *,s32);
extern s32 EftStruggle_IsActiveFor(s32);
extern f32 BtlStage_GetInnerRadius(void),BtlStage_GetTop(void),BtlStage_GetBottom(void);
extern f32 BtlCharApi_GetRadius(s32),BtlCharApi_GetHeight(s32),sinf(f32),cosf(f32),atan2f(f32,f32);
extern s32 BtlStage_ProbeGround(s32,Vec4 *,StgGroundHit *);
extern void Port_TagAssistTrace(s32,s32,s32,s32);
extern EftHSlot *EftShot_GetSlot(s32,s32);
extern s32 EftStruggle_GetPosFor(s32,Vec4 *);
extern void EftShot_CreateSlotTask(s32,s32),EftShot_Start(EftHStartArg *);
extern void BtlTask_Kill(EftHTask *);
extern void EftShot_SetCharPool(s32);
extern s32 EftShot_GetCurSlot(s32);
extern void EftShot_SetCurSlot(s32,s32);

typedef struct TagAssist {
    s32 state,ally,enemy,age,hp,pose,slot,paid,beamSlot,beamAge;
    Vec4 start,spot;
} TagAssist;
static TagAssist assists[PORT_FIGHTER_CAPACITY];
static s32 consumed[PAD_COUNT],clock;
static EftShotParam borrowedParams[PORT_FIGHTER_CAPACITY];
static void drop_borrowed(s32 actor) {
    EftHSlot *s;s32 oldPool;
    if(assists[actor].beamSlot!=5)return;
    s=EftShot_GetSlot(actor,5);
    oldPool=BtlPool_GetCurrent();EftShot_SetCharPool(actor);
    if(s->task)BtlTask_Kill(s->task);
    BtlPool_SetCurrent(oldPool);
    memset(s,0,sizeof(*s));assists[actor].beamSlot=-1;
}
static s32 beam_slot(s32 actor) {
    BtlMgrChr *c=BtlChar_Get(actor);
    s32 slot,chara=c->members[c->curMember].chara;
    s32 reviewed=PortGame_IsBT4() ?
        ((u32)chara<250?tagAssistBT4OwnSlot[chara]:-2) :
        ((u32)chara<161?tagAssistOwnSlot[chara]:-2);
    /* Early Piccolo's Explosive Demon Wave is a straight palm-fired beam.
       Prefer it for sustained support over the piercing cannon's ring FX. */
    if(chara==22)reviewed=2;
    if(reviewed==-1)return -1;
    for(slot=2;slot<=4;slot++) {
        EftHSlot *fx=EftShot_GetSlot(actor,slot);
        s32 kind=BtlSuper_GetType(c,slot);
        if((reviewed>=2?slot==reviewed:kind==2) &&
           BtlSuper_GetHitDirKind(c,slot)!=1 && BtlSuper_GetHitDirKind(c,slot)!=9 &&
           fx && fx->pack && fx->param && fx->param->type==0) return slot;
    }
    return -1;
}
/* The sixth effect slot is reserved by the native layout. A helper with no
   straight beam gets its own manager and emitters there; none of its real
   techniques, textures or animation banks are replaced. */
static s32 borrow_beam(s32 actor) {
    EftHSlot *source,*s;EftHStartArg request;s32 donor,slot=-1,oldPool,oldSlot,pass;
    for(pass=0;pass<2 && slot<2;pass++)for(donor=0;donor<TagTeam_Count();donor++) {
        s32 candidate=beam_slot(donor);
        if(candidate>=2 && (pass || BtlSuper_GetType(BtlChar_Get(donor),candidate)==2)) {
            slot=candidate;break;
        }
    }
    if(slot<2)return 0;
    source=EftShot_GetSlot(donor,slot);s=EftShot_GetSlot(actor,5);
    borrowedParams[actor]=*source->param;
    borrowedParams[actor].kind=1;
    /* Generic clash clips have no technique muzzle event. Bind every phase
       to the firing palm instead of interpreting the absent event as root. */
    borrowedParams[actor].flags|=4;
    {s32 node;for(node=0;node<6;node++)borrowedParams[actor].unkE[node]=0x2D;}
    *s=*source;s->param=&borrowedParams[actor];s->chr=actor;s->task=NULL;
    s->arg.chr=actor;s->arg.slot=5;
    oldPool=BtlPool_GetCurrent();
    EftShot_SetCharPool(actor);EftShot_CreateSlotTask(actor,5);
    request=s->arg;request.time=60;request.unk14=0;request.unk18=0;
    oldSlot=EftShot_GetCurSlot(actor);
    EftShot_Start(&request);
    /* This is an extra visual effect, not a change to the fighter's selected
       technique. Keep the private slot out of normal move-table indexing. */
    EftShot_SetCurSlot(actor,oldSlot);BtlPool_SetCurrent(oldPool);
    return 1;
}
/* The helper's native effect is visual support: the existing assist multiplier
   supplies its gameplay contribution, so this beam never adds another hit. */
s32 TagAssist_Beam(s32 actor,Vec4 *end,s32 *age) {
    TagAssist *a;
    if(!TagAssist_Active(actor)) return 0;
    a=&assists[actor];
    if(a->state!=3 || a->beamSlot<2 || !EftStruggle_GetPosFor(a->ally,end)) return 0;
    if(age) *age=a->beamAge;
    return 1;
}
s32 TagAssist_BeamForSlot(s32 actor,s32 slot,Vec4 *end,s32 *age) {
    /* A projectile left over from an earlier attack must keep its own path. */
    return TagAssist_Active(actor) && assists[actor].beamSlot==slot &&
        TagAssist_Beam(actor,end,age);
}
void TagAssist_Reset(void) {
    memset(assists,0,sizeof(assists));memset(consumed,0,sizeof(consumed));clock=0;
}
s32 TagAssist_Active(s32 actor) {
    return TagTeam_Active() && (u32)actor<(u32)TagTeam_Count() && assists[actor].state;
}
s32 TagAssist_Ally(s32 actor) { return TagAssist_Active(actor)?assists[actor].ally:-1; }
s32 TagAssist_ConsumeR3(s32 player) { return (u32)player<PAD_COUNT && consumed[player]; }
s32 TagAssist_PendingAction(s32 actor) {
    TagAssist *a;
    if(!TagAssist_Active(actor)) return -1;
    a=&assists[actor];
    if(a->state==1 && BtlChar_Get(actor)->action!=0xF) return 0xF;
    if(a->state==2) return 0x130;
    if(a->state==4) return 0xB;
    return -1;
}
static s32 pose_for(s32 actor) {
    BtlMgrChr *c=BtlChar_Get(actor);
    BobjObj *obj=BtlChar_GetObj(c);
    s32 slot,anim;
    if(!obj) return -1;
    for(slot=2;slot<=5;slot++) {
        anim=slot==5?0x16C:196+34*slot;
        if(slot!=5 && (slot!=beam_slot(actor))) continue;
        if(obj->mdl.anims[anim] && *(u32 *)obj->mdl.anims[anim]>=32 &&
           *(u32 *)obj->mdl.anims[anim]<=49152 && !(BtlAnim_GetFlags(anim)&0x20000000)) return anim;
    }
    /* Arale's reviewed mouth-beam bank has no generic clash animation. */
    anim=264;
    if(BtlSuper_GetType(c,2)==6 && obj->mdl.anims[anim] &&
       *(u32 *)obj->mdl.anims[anim]>=32 && *(u32 *)obj->mdl.anims[anim]<=49152) return anim;
    return -1;
}
static s32 available(s32 actor) {
    BtlMgrChr *c=BtlChar_Get(actor);
    return TagTeam_IsAlive(actor) && !TagFusion_Hidden(actor) &&
        !BtlChar_IsFrozen(c) && !TagTeam_CinematicPaused(actor) &&
        c->action>=0xB && c->action<0x37 && BtlClash_Opponent(actor)<0 &&
        TagFusion_Requester(actor)<0 && BtlMember_GetActiveGauge(c)->blast>=100000;
}
s32 TagAssist_ReadyAlly(s32 actor) {
    s32 i,best=-1;f32 reach=Port_TagOption(TAG_OPT_BEAM_ASSIST_RANGE),nearest=reach*reach;
    Vec4 p;
    if(!TagTeam_Active() || !Port_TagOption(TAG_OPT_BEAM_ASSIST) ||
       TagAssist_Active(actor) || !available(actor)) return -1;
    p=((BtlMovePose *)&BtlChar_Get(actor)->pose)->pos;
    for(i=0;i<TagTeam_Count();i++) if(i!=actor && !TagTeam_IsEnemy(actor,i) &&
        BtlClash_State(i)==2 && EftStruggle_IsActiveFor(i) && TagTeam_IsAlive(i)) {
        Vec4 q=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos;
        f32 x=p.x-q.x,z=p.z-q.z;
        if(x*x+z*z<=nearest) {nearest=x*x+z*z;best=i;}
    }
    return best>=0 && pose_for(actor)>=0?best:-1;
}
static void finish(s32 actor) {
    TagAssist *a=&assists[actor];BtlMgrChr *c=BtlChar_Get(actor);
    Port_TagAssistTrace(actor,a->state,c->action,a->age);
    if(a->state && (c->action==0x130 || (a->state==1 && c->action==0xF))) {
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
    }
    /* Keep ownership through action exit. The native struggle exit tears down
       a technique; a helper never entered that technique. */
    if(c->action==0x130) a->state=4;
    else {drop_borrowed(actor);memset(a,0,sizeof(*a));}
}
static s32 placement(s32 actor,TagAssist *a) {
    BtlMgrChr *ally=BtlChar_Get(a->ally),*enemy=BtlChar_Get(a->enemy);
    Vec4 center=((BtlMovePose *)&ally->pose)->pos,q=((BtlMovePose *)&enemy->pose)->pos;
    f32 yaw=atan2f(q.x-center.x,q.z-center.z),radius=BtlCharApi_GetRadius(BtlChar_Get(actor)->objId);
    f32 gap=radius+BtlCharApi_GetRadius(ally->objId)+1.5f;
    f32 side=(a->slot&1)?-1.0f:1.0f,height=BtlCharApi_GetHeight(BtlChar_Get(actor)->objId);
    StgGroundHit allyFloor;
    Vec4 probe=center;
    s32 grounded;
    s32 n,j,clear;
    probe.y-=10;
    BtlStage_ProbeGround(-1,&probe,&allyFloor);
    grounded=allyFloor.y>=center.y-2 && allyFloor.y<=center.y+2;
    /* Shoulder to shoulder, just behind the beam source. Outer helpers use a
       second row. Search slightly farther out only if a real body is there. */
    for(n=0;n<5;n++) {
        Vec4 p=center;
        f32 lateral=gap+(a->slot/2)*(radius*2+3)+n*(radius+2);
        f32 back=gap*0.25f+(a->slot/2)*(radius+3);
        p.x+=cosf(yaw)*lateral*side-sinf(yaw)*back;
        p.z-=sinf(yaw)*lateral*side+cosf(yaw)*back;
        if(grounded) {
            StgGroundHit floor;
            probe=p;probe.y-=40;
            BtlStage_ProbeGround(-1,&probe,&floor);
            if(floor.y>probe.y && floor.y>=center.y-24 && floor.y<=center.y+24)
                p.y=floor.y;
        }
        if(!TagPosition_SafeSpot(actor,&p)) continue;
        clear=1;
        for(j=0;j<TagTeam_Count();j++) {
            f32 x,z,r,otherHeight;
            if(j==actor || !TagTeam_IsAlive(j) || TagFusion_Hidden(j)) continue;
            q=assists[j].state?assists[j].spot:((BtlMovePose *)&BtlChar_Get(j)->pose)->pos;
            x=p.x-q.x;z=p.z-q.z;
            r=radius+BtlCharApi_GetRadius(BtlChar_Get(j)->objId)+1;
            otherHeight=BtlCharApi_GetHeight(BtlChar_Get(j)->objId);
            if(x*x+z*z<r*r && p.y>q.y-otherHeight && q.y>p.y-height) {clear=0;break;}
        }
        if(clear) {a->spot=p;return 1;}
    }
    return 0;
}
void TagAssist_Update(void) {
    s32 i,j;
    if(!TagTeam_Active() || !BtlSeq_IsFighting() ||
       (Battle_GetWork()->flags&(BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOADING))) return;
    clock++;
    for(i=0;i<PAD_COUNT;i++) if(!(gPad[i].held&PAD_R3)) consumed[i]=0;
    for(i=0;i<TagTeam_Count();i++) {
        TagAssist *a=&assists[i];BtlMgrChr *c=BtlChar_Get(i);
        BtlMgrGauge *g=BtlMember_GetActiveGauge(c);
        s32 player=TagTeam_InputPlayer(i);
        if(a->state==4) {
            if(c->action!=0x130) {drop_borrowed(i);memset(a,0,sizeof(*a));}
            continue;
        }
        if(a->state) {
            if(!Port_TagOption(TAG_OPT_BEAM_ASSIST) || !TagTeam_IsAlive(i) ||
               !TagTeam_IsAlive(a->ally) || !EftStruggle_IsActiveFor(a->ally) ||
               BtlClash_Opponent(a->ally)!=a->enemy || g->health<a->hp ||
               (a->state==3 && c->action!=0x130) ||
               (a->age>2 && c->action!=0xF && c->action!=0xB && c->action!=0x130) ||
               !placement(i,a)) { finish(i);continue; }
            if(BtlChar_IsFrozen(c)) continue;
            a->age++;
            if(a->state==1) {
                if(a->age>=15) {a->state=2;BtlAct_ClearQueue(c);BtlAct_Request(c,0x130);}
            }
            continue;
        }
        if(player>=0) {
            if(!(gPad[player].pressed&PAD_R3) || (gPad[player].held&
                (PAD_TRIANGLE|PAD_CIRCLE|PAD_CROSS|PAD_SQUARE|PAD_L1|PAD_L2|PAD_R1|PAD_R2))) continue;
        } else if(!Port_TagOption(TAG_OPT_BEAM_ASSIST_CPU) || clock%30!=i*3) continue;
        j=TagAssist_ReadyAlly(i);if(j<0) continue;
        a->ally=j;a->enemy=BtlClash_Opponent(j);a->pose=pose_for(i);
        {
            s32 used=0;
            for(j=0;j<TagTeam_Count();j++) if(assists[j].state && assists[j].ally==a->ally)
                used|=1<<assists[j].slot;
            for(a->slot=0;a->slot<4 && (used&(1<<a->slot));a->slot++) {}
        }
        if(a->slot>=4 || !placement(i,a)) {memset(a,0,sizeof(*a));continue;}
        a->hp=g->health;a->start=((BtlMovePose *)&c->pose)->pos;a->state=1;
        if(player>=0) consumed[player]=1;
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xF);
    }
}
s32 TagAssist_Handle(s32 actor,s32 phase) {
    TagAssist *a;BtlMgrChr *c;
    if(!TagAssist_Active(actor)) return 0;
    a=&assists[actor];c=BtlChar_Get(actor);
    if(phase==0 || phase==3) Port_TagAssistTrace(actor,a->state,c->action,1000+phase);
    if(a->state==4) {
        if(phase==3) {drop_borrowed(actor);memset(a,0,sizeof(*a));}
        return 1;
    }
    if(c->action==0xF) {
        if(phase==0) {
            BtlAnim_Play(c,0xC,0.15f);
            BtlCharSnd_PlayCommon(c,BtlParam_GetDashSound(c));
        } else if(phase==1) {
            BtlMovePose *p=(BtlMovePose *)&c->pose;
            f32 t=a->age/15.0f;if(t>1)t=1;t=t*t*(3-2*t);
            if(BtlAnim_GetId(c)==0xC) BtlAnim_AdvanceThen(c,0xD,0.15f,0);
            else BtlAnim_AdvanceLoop(c,0);
            p->pos.x=a->start.x+(a->spot.x-a->start.x)*t;
            p->pos.y=a->start.y+(a->spot.y-a->start.y)*t;
            p->pos.z=a->start.z+(a->spot.z-a->start.z)*t;
            p->unk20.x=p->unk20.y=p->unk20.z=0;
            p->rot.y=atan2f(a->spot.x-a->start.x,a->spot.z-a->start.z);
            p->speed=p->fallSpeed=0;
            BtlChar_PoseToObj(c,1);
            BtlChar_SetFxBit(c,0xB);
        }
        return 1;
    }
    if(phase==0) {
        BtlMgrGauge *g=BtlMember_GetActiveGauge(c);
        if(g->blast<100000) {finish(actor);return 1;}
        a->beamSlot=beam_slot(actor);a->beamAge=0;
        BtlAnim_Play(c,a->beamSlot>=2?a->pose-1:a->pose,0.15f);
        g->blast-=100000;a->paid=1;a->state=3;
        if(a->beamSlot<2) {
            a->beamSlot=5;
            if(!borrow_beam(actor)){a->beamSlot=-1;finish(actor);return 1;}
        }
        Port_TagAssistTrace(actor,a->beamSlot,a->pose,2000);
        if(a->beamSlot>=2 && a->beamSlot<5) BtlChar_SetFxBit(c,0x3C+a->beamSlot);
        BtlChar_PlayVoice(c,0x14);
    } else if(phase==1) {
        BtlMovePose *p=(BtlMovePose *)&c->pose;
        Vec4 q=((BtlMovePose *)&BtlChar_Get(a->enemy)->pose)->pos;
        if(a->beamSlot>=2 && a->beamSlot<5 && BtlAnim_GetId(c)==a->pose-1) BtlAnim_AdvanceThen(c,a->pose,0.0f,0);
        else BtlAnim_AdvanceLoop(c,0);
        a->beamAge++;
        p->pos=a->spot;p->unk20.x=p->unk20.y=p->unk20.z=0;
        p->rot.y=atan2f(q.x-p->pos.x,q.z-p->pos.z);
        BtlChar_PoseToObj(c,1);
        BtlChar_SetFlag(c,0x128);
    } else if(phase==3) {
        drop_borrowed(actor);
        memset(a,0,sizeof(*a));
    }
    return 1;
}
s32 TagAssist_Multiplier(s32 actor) {
    s32 i,m=100;
    for(i=0;i<TagTeam_Count();i++) if(assists[i].state==3 && assists[i].paid && assists[i].ally==actor)
        m+=Port_TagOption(TAG_OPT_BEAM_ASSIST_POWER)-100;
    return m>300?300:m;
}
s32 TagAssist_DamageMultiplier(s32 a,s32 b) {
    s32 m=TagAssist_Multiplier(a)*TagAssist_Multiplier(b)/100;
    return m>400?400:m;
}
