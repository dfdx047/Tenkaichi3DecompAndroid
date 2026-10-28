/* Native fusion uses the game's own paired animation and model-change queue.
   The other participant remains a real actor, suspended until separation. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_fusion.h"
#include "battle/tag_options.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_act_j.h"
#include "battle/btl_seq.h"
#include "battle/tag_clash.h"
#include "battle/stg_a.h"
#include "sys/pad.h"
extern void *memset(void *,s32,u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern s32 BtlParam_GetUnkBA(BtlMgrChr *,s32,s32),BtlParam_GetUnkB1(BtlMgrChr *,s32);
extern s32 BtlParam_GetUnkB4(BtlMgrChr *,s32);
extern s32 BtlChar_TestFlag(BtlMgrChr *,s32),BtlChar_IsFrozen(BtlMgrChr *);
extern void BtlChar_SetHeldFlag(BtlMgrChr *,s32),BtlChar_ClearFlag(BtlMgrChr *,s32);
extern void BtlChar_ClearFxBit(BtlMgrChr *,s32);
extern s32 BtlCharApi_IsChanging(s32),BtlCharApi_IsInTechnique(s32);
extern s32 BtlCharApi_HasCamPriority(s32);
extern s32 BtlAct_GetCurrent(BtlMgrChr *),BtlAct_GetCurrentClass(BtlMgrChr *);
extern s32 BtlAct_GetQueued(BtlMgrChr *);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern void BtlChar_PoseToObj(BtlMgrChr *,s32);
extern f32 BtlCharApi_GetRadius(s32),BtlStage_GetInnerRadius(void);
extern f32 BtlStage_GetTop(void),BtlStage_GetBottom(void);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *),BtlStage_ProbeGround(s32,Vec4 *,StgGroundHit *);
extern f32 sinf(f32),cosf(f32),sqrtf(f32);

typedef struct TagFusion {
    s32 state,partner,result,dance,ticks,request,index,wait,authorized,age;
    s32 duration,controller,controlTicks,controlInterval;
    BtlMgrMember before,other;
} TagFusion;
static TagFusion fusions[TAG_TEAM_CAPACITY];
static s32 hiddenBy[TAG_TEAM_CAPACITY];

void TagFusion_Reset(void) {
    s32 i;
    memset(fusions,0,sizeof(fusions));
    for(i=0;i<TAG_TEAM_CAPACITY;i++) {
        hiddenBy[i]=-1;
        fusions[i].partner=fusions[i].request=-1;
    }
}
s32 TagFusion_Hidden(s32 actor) {
    return TagTeam_Active() && (u32)actor<(u32)TagTeam_Count() && hiddenBy[actor]>=0;
}
void TagFusion_Remove(s32 actor) {
    s32 owner=TagFusion_Owner(actor),i;
    if((u32)owner>=TAG_TEAM_CAPACITY)return;
    for(i=0;i<TAG_TEAM_CAPACITY;i++)if(hiddenBy[i]==owner)hiddenBy[i]=-1;
    memset(&fusions[owner],0,sizeof(fusions[owner]));
    fusions[owner].partner=fusions[owner].request=-1;
}
s32 TagFusion_Owner(s32 actor) { return TagFusion_Hidden(actor)?hiddenBy[actor]:actor; }
s32 TagFusion_SharedBody(s32 actor) {
    s32 owner=TagFusion_Owner(actor);
    return (u32)owner<(u32)TagTeam_Count() && fusions[owner].state>=2?owner:-1;
}
s32 TagFusion_Seconds(s32 actor) {
    if((u32)actor>=(u32)TagTeam_Count()) return -1;
    TagFusion *f=&fusions[TagFusion_Owner(actor)];
    return f->state==2 && f->dance && Port_TagOption(TAG_OPT_TIMED_FUSIONS)?(f->ticks+29)/30:-1;
}
s32 TagFusion_Duration(s32 actor) {
    return TagFusion_Seconds(actor)>=0?fusions[TagFusion_Owner(actor)].duration:0;
}
s32 TagFusion_Controller(s32 actor) {
    if(!TagTeam_Active()) return actor;
    if((u32)actor>=(u32)TagTeam_Count()) return -1;
    s32 owner=TagFusion_Owner(actor);
    return fusions[owner].state>=2?fusions[owner].controller:TagTeam_Player(actor);
}
s32 TagFusion_NextController(s32 actor) {
    s32 owner=TagFusion_SharedBody(actor),a,b;
    TagFusion *f;
    if(owner<0 || !Port_TagOption(TAG_OPT_FUSION_SWAP)) return -1;
    f=&fusions[owner];
    if(f->state!=2) return -1;
    a=TagTeam_Player(owner);b=TagTeam_Player(f->partner);
    if(a<0 || b<0 || a==b) return -1;
    return f->controller==a?b:a;
}
s32 TagFusion_SwitchSeconds(s32 actor) {
    return TagFusion_NextController(actor)<0?-1:(fusions[TagFusion_Owner(actor)].controlTicks+29)/30;
}
s32 TagFusion_Requester(s32 actor) {
    s32 i;
    for(i=0;i<TagTeam_Count();i++) if(fusions[i].wait>0 && fusions[i].request==actor) return i;
    return -1;
}
static s32 available(s32 actor) {
    BtlMgrChr *c=BtlChar_Get(actor);
    return TagTeam_IsAlive(actor) && !fusions[actor].state && !TagFusion_Hidden(actor) &&
        !BtlChar_IsFrozen(c) && BtlClash_Opponent(actor)<0 &&
        !BtlCharApi_IsChanging(c->objId) && !BtlCharApi_IsInTechnique(c->objId) &&
        BtlAct_GetCurrentClass(c)!=1 && !BtlChar_TestFlag(c,0xBB);
}
s32 TagFusion_Find(s32 actor,s32 index) {
    s32 i,k;
    BtlMgrChr *c=BtlChar_Get(actor);
    if(!Port_TagOption(TAG_OPT_FUSIONS) || (u32)index>=3 || fusions[actor].state ||
        TagFusion_Hidden(actor) || !TagTeam_IsAlive(actor)) return -1;
    for(i=0;i<TagTeam_Count();i++) if(i!=actor && !TagTeam_IsEnemy(actor,i) && available(i)) {
        BtlMgrMember *m=BtlMember_GetActive(BtlChar_Get(i));
        if(m->gauge.unk30) continue;
        for(k=0;k<4;k++) if(m->chara==BtlParam_GetUnkBA(c,index,k)) return i;
    }
    return -1;
}
s32 TagFusion_Prepare(s32 actor,s32 index,s32 partner) {
    TagFusion *f=&fusions[actor];
    BtlMgrChr *c=BtlChar_Get(actor),*p;
    s32 found=TagFusion_Find(actor,index),i;
    if(found<0 || (partner>=0 && found!=partner)) return -1;
    partner=found;
    if(TagTeam_Player(partner)>=0 && !f->authorized) {
        /* A held fusion chord must not keep extending an unanswered request. */
        if(f->wait<=0 || f->request!=partner || f->index!=index) {
            f->request=partner;f->index=index;f->wait=150;
        }
        return -1;
    }
    p=BtlChar_Get(partner);
    f->before=*BtlMember_GetActive(c);
    f->other=*BtlMember_GetActive(p);
    f->partner=partner;f->result=BtlParam_GetUnkB4(c,index);
    f->dance=BtlParam_GetUnkB1(c,index)==1;
    f->duration=Port_TagOption(TAG_OPT_FUSION_SECONDS);
    f->ticks=f->duration*30;
    f->controller=TagTeam_Player(actor)>=0?TagTeam_Player(actor):TagTeam_Player(partner);
    f->controlInterval=Port_TagOption(TAG_OPT_FUSION_SWAP_SECONDS);
    f->controlTicks=f->controlInterval*30;
    f->state=1;f->wait=0;f->request=-1;f->authorized=0;f->age=0;
    /* Native fusion expects a reserve-member record for stats/equipment. It
       is not added to memberCount, so switching cannot summon a duplicate. */
    c->members[1]=f->other;
    hiddenBy[partner]=actor;
    BtlAct_ClearQueue(p);BtlAct_Request(p,0xB);
    BtlChar_SetHeldFlag(p,0xB);
    for(i=0;i<0x40;i++) BtlChar_ClearFxBit(p,i);
    return 1;
}

/* Place the two bodies on opposite sides of the fused fighter. Search valid
   terrain inside the arena and avoid every other living actor. */
static void separate(s32 actor,TagFusion *f) {
    BtlMgrChr *a=BtlChar_Get(actor),*b=BtlChar_Get(f->partner);
    Vec4 center=((BtlMovePose *)&a->pose)->pos,rot=((BtlMovePose *)&a->pose)->rot;
    Vec4 positions[2];
    f32 radii[2]={BtlCharApi_GetRadius(a->objId),BtlCharApi_GetRadius(b->objId)};
    s32 body,n,j,ok;
    for(body=0;body<2;body++) {
        f32 angle=rot.y+(body?3.14159265f:0.0f),dist=(radii[0]+radii[1])*0.6f+12.0f;
        for(n=0;n<64;n++,angle+=0.78539816f) {
            Vec4 p=center,probe;
            StgGroundHit ground;
            f32 limit=BtlStage_GetInnerRadius()-radii[body]-4.0f;
            p.x+=cosf(angle)*dist;p.z-=sinf(angle)*dist;
            if((n&7)==7) dist+=radii[body]+12.0f;
            if(p.x*p.x+p.z*p.z>limit*limit) continue;
            probe=p;probe.y=BtlStage_GetTop()+1.0f;
            BtlStage_ProbeGround(-1,&probe,&ground);
            if(ground.y<=probe.y || ground.y>BtlStage_GetBottom()) continue;
            if(p.y>ground.y) p.y=ground.y;
            if(p.y<BtlStage_GetTop()+radii[body]+2.0f) p.y=BtlStage_GetTop()+radii[body]+2.0f;
            ok=1;
            for(j=0;j<TagTeam_Count();j++) {
                Vec4 q;f32 x,z,y,r;
                if(j==actor || j==f->partner || !TagTeam_IsAlive(j)) continue;
                q=((BtlMovePose *)&BtlChar_Get(j)->pose)->pos;
                x=p.x-q.x;z=p.z-q.z;y=p.y-q.y;r=radii[body]+BtlCharApi_GetRadius(BtlChar_Get(j)->objId)+8.0f;
                if(x*x+y*y+z*z<r*r) {ok=0;break;}
            }
            if(body) {
                f32 x=p.x-positions[0].x,y=p.y-positions[0].y,z=p.z-positions[0].z,r=radii[0]+radii[1]+8.0f;
                if(x*x+y*y+z*z<r*r) ok=0;
            }
            if(ok) {positions[body]=p;break;}
        }
        if(n==64) {
            /* The native start placement already searches safe separated
               slots. Use it only if local geometry has no free pair. */
            extern s32 BtlStage_GetStartPlace(s32,Vec4 *,Vec4 *,s32);
            Vec4 unused;
            BtlStage_GetStartPlace(body?f->partner:actor,&positions[body],&unused,0);
        }
    }
    BtlChar_Place(a,&positions[0],&rot,BtlStage_FindZoneNear(-1,&positions[0]));
    BtlChar_Place(b,&positions[1],&rot,BtlStage_FindZoneNear(-1,&positions[1]));
    BtlChar_PoseToObj(a,1);BtlChar_PoseToObj(b,1);
}
void TagFusion_ModelLoaded(s32 actor) {
    if(!TagTeam_Active()) return;
    TagFusion *f=&fusions[actor];
    BtlMgrChr *c=BtlChar_Get(actor);
    BtlMgrMember *m=BtlMember_GetActive(c);
    if(f->state==1 && m->chara==f->result) {
        f->state=2;f->age=0;
        TagTeam_RefreshControl(actor);
    }
    else if(f->state==3 && m->chara==f->before.chara) {
        BtlMgrMember *p=BtlMember_GetActive(BtlChar_Get(f->partner));
        f32 ratio=m->gauge.healthMax>0?(f32)m->gauge.health/m->gauge.healthMax:0.0f;
        s32 ki=m->gauge.ki,stocks=m->gauge.blast;
        *m=f->before;*p=f->other;
        /* Restore each fighter's stats, not the resources they had before
           fusion. Spending stocks while fused must survive separation. */
        m->gauge.ki=ki<m->gauge.kiMax?ki:m->gauge.kiMax;
        p->gauge.ki=ki<p->gauge.kiMax?ki:p->gauge.kiMax;
        m->gauge.blast=stocks<m->gauge.blastMax?stocks:m->gauge.blastMax;
        p->gauge.blast=stocks<p->gauge.blastMax?stocks:p->gauge.blastMax;
        m->gauge.health=(s32)(m->gauge.healthMax*ratio);
        p->gauge.health=(s32)(p->gauge.healthMax*ratio);
        if(m->gauge.health<1) m->gauge.health=1;
        if(p->gauge.health<1) p->gauge.health=1;
        m->gauge.lowHealth=m->gauge.lowHealthIdle=0;
        p->gauge.lowHealth=p->gauge.lowHealthIdle=0;
        memset(&c->members[1],0,sizeof(c->members[1]));
        f->state=4;
    }
}
static void begin_defusion(s32 actor,TagFusion *f) {
    BtlActJChr *c=(BtlActJChr *)BtlChar_Get(actor);
    BtlActJForm *form=&c->form;
    form->index=-1;form->chara=f->before.chara;form->costume=f->before.costume;
    form->cost=0;form->kind=3;form->variant=f->before.gauge.unk20;
    form->animChara=form->unk1C=form->voiceChara=form->chara;
    form->unk24=-1;form->unk28=0;form->partner=-1;form->unk30=0;
    f->state=3;
    BtlAct_ClearQueue((BtlMgrChr *)c);
    BtlAct_Request((BtlMgrChr *)c,0xEE);
}
void TagFusion_Update(void) {
    s32 i;
    if(!TagTeam_Active() || !BtlSeq_IsFighting() ||
        (Battle_GetWork()->flags&(BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOADING))) return;
    for(i=0;i<TagTeam_Count();i++) {
        TagFusion *f=&fusions[i];
        BtlMgrChr *c=BtlChar_Get(i);
        if(f->wait>0) {
            s32 player=TagTeam_Player(f->request),partner;
            f->wait--;
            if(!Port_TagOption(TAG_OPT_FUSIONS) || !TagTeam_IsAlive(i) || !available(f->request))
                f->wait=0;
            else if(player>=0 && (gPad[player].pressed&PAD_R3) &&
                    BtlAct_CanFuse((BtlActJChr *)c,f->index,1,1,&partner) &&
                    partner==f->request && available(i)) {
                f->authorized=1;
                if(BtlAct_QueueFusion((BtlActJChr *)c,f->index,partner))
                    BtlAct_Request(c,BtlAct_GetQueued(c));
                f->authorized=0;
            }
        }
        if(f->state==1 && ++f->age>2 && c->action!=0xF1 && c->action!=0xF2) {
            /* A queued move can be interrupted before it ever starts. */
            hiddenBy[f->partner]=-1;BtlChar_ClearFlag(BtlChar_Get(f->partner),0xB);
            memset(&c->members[1],0,sizeof(c->members[1]));f->state=0;
        }
        if(f->state==2) {
            s32 interval=Port_TagOption(TAG_OPT_FUSION_SWAP_SECONDS),next=TagFusion_NextController(i);
            if(interval!=f->controlInterval || next<0) {
                f->controlInterval=interval;f->controlTicks=interval*30;
            }
            if(next<0 && !Port_TagOption(TAG_OPT_FUSION_SWAP)) {
                s32 initial=TagTeam_Player(i)>=0?TagTeam_Player(i):TagTeam_Player(f->partner);
                if(f->controller!=initial) {f->controller=initial;TagTeam_RefreshControl(i);}
            }
            /* Cinematics and clash prompts belong to the player who started
               them. Pause the handoff clock while they cannot freely play. */
            if(next>=0 && TagTeam_IsAlive(i) && !BtlChar_IsFrozen(c) &&
                !TagTeam_CinematicPaused(i) && !BtlCharApi_IsChanging(c->objId) &&
                !BtlCharApi_IsInTechnique(c->objId) && BtlClash_Opponent(i)<0 &&
                !BtlCharApi_HasCamPriority(BattleSide_GetObjId(TagTeam_CameraActor(i)))) {
                if(f->controlTicks>0) f->controlTicks--;
                if(!f->controlTicks) {
                    f->controller=next;f->controlTicks=interval*30;
                    TagTeam_RefreshControl(i);
                }
            }
        }
        if(f->state==2 && f->dance && Port_TagOption(TAG_OPT_TIMED_FUSIONS) &&
            TagTeam_IsAlive(i) && !BtlChar_IsFrozen(c) && !BtlCharApi_IsChanging(c->objId)) {
            if(f->ticks>0) f->ticks--;
            if(f->ticks==0 && BtlAct_GetCurrent(c)==0xB) begin_defusion(i,f);
        }
        if(f->state==4 && !BtlCharApi_IsChanging(c->objId)) {
            BtlMgrChr *p=BtlChar_Get(f->partner);
            separate(i,f);
            hiddenBy[f->partner]=-1;BtlChar_ClearFlag(p,0xB);
            p->freeze=p->freezeNext=p->freezeDelay=0;
            BtlAct_ClearQueue(p);BtlAct_Request(p,0xB);
            f->state=0;
            TagTeam_RefreshControl(i);
            TagTeam_RefreshControl(f->partner);
        }
    }
}
