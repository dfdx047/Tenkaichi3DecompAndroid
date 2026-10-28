/* Native version of the PCSX2 mod's dynamic opponent policy.
 * Timers count released gameplay updates, not render passes or paused time.
 * Private random state leaves the original combat AI's random sequence alone. */
#include "common.h"
#include "battle/tag_challenge.h"
#include "battle/tag_cpu.h"
#include "battle/tag_team.h"
#include "battle/tag_fusion.h"
#include "battle/tag_assist.h"
#include "battle/tag_clash.h"
#include "battle/tag_session.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_member.h"
#include "battle/btl_char_move.h"
#include "battle/btl_seq.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern s32 BtlChar_TestFlag(BtlMgrChr *,s32),BtlChar_IsFrozen(BtlMgrChr *);
extern s32 BtlCharApi_HasCamPriority(s32),BtlCharApi_IsChanging(s32),BtlCharApi_IsInRushSequence(s32);
extern void *memset(void *,s32,u32);
typedef struct Threat { s32 tick,damage; } Threat;
typedef struct Decision { s32 ready,changed,evaluate,reconsider,previous; } Decision;
static Threat threats[TAG_TEAM_CAPACITY][TAG_TEAM_CAPACITY];
static Decision decisions[TAG_TEAM_CAPACITY];
static s32 tick;
static u32 randomState;
static u32 jitter(void) {
    randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;
    return randomState;
}
static s32 living(s32 actor) {
    return (u32)actor<(u32)TagTeam_Count() && !TagFusion_Hidden(actor) && !TagChallenge_Hidden(actor) &&
        !BtlChar_TestFlag(BtlChar_Get(actor),7) &&
        BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(actor))->health>0;
}
static s32 cpu(s32 actor) {return TagTeam_InputPlayer(actor)<0 && TagTeam_Side(actor)->control==BTL_CONTROL_CPU;}
static s32 safe(s32 actor) {
    BtlMgrChr *c=BtlChar_Get(actor);
    BtlMemberQueue *q=&((BtlMemberChr *)c)->queue;
    return c->action>=4 && !(c->action>=180&&c->action<188) &&
        !(c->action>=236&&c->action<316) && !q->total && !q->drainHealth && !q->drainKi &&
        !BtlChar_IsFrozen(c) && !TagTeam_CinematicPaused(actor) && !TagAssist_Active(actor) &&
        BtlClash_Opponent(actor)<0 && !BtlCharApi_HasCamPriority(c->objId) &&
        !BtlCharApi_IsChanging(c->objId) && !BtlCharApi_IsInRushSequence(c->objId);
}
static s32 grudge(s32 actor,s32 candidate) {
    Threat *t=&threats[actor][candidate];
    s32 age=tick-t->tick;
    if(age>=240) {t->damage=0;return 0;}
    return t->damage>>(age/60);
}
void TagCpu_Reset(void) {
    s32 i;
    memset(threats,0,sizeof(threats));memset(decisions,0,sizeof(decisions));
    tick=0;randomState=0x94b37a25;
    for(i=0;i<TAG_TEAM_CAPACITY;i++) decisions[i].previous=-1;
}
void TagCpu_Damage(s32 victim,s32 attacker,s32 damage) {
    s32 value;
    if(!TagTeam_Active() || !BtlSeq_IsFighting() || damage<=0 ||
        !living(victim) || !living(attacker) || !TagTeam_IsEnemy(victim,attacker)) return;
    value=grudge(victim,attacker)+damage;
    threats[victim][attacker].damage=value>32000?32000:value;
    threats[victim][attacker].tick=tick;
}
/* The same short warning window as the PCSX2 mod. Recent hits outrank
   distance; otherwise use the nearest enemy currently attacking this actor. */
s32 TagCpu_Attacker(s32 actor) {
    s32 i,best=-1,latest=-1;f32 nearest=1.0e30f;
    Vec4 own=((BtlMovePose *)&BtlChar_Get(actor)->pose)->pos;
    for(i=0;i<TagTeam_Count();i++) if(living(i)&&TagTeam_IsEnemy(actor,i) &&
        (TagTeam_LockedOff(actor)||i!=TagTeam_Target(actor))) {
        Threat *h=&threats[actor][i];s32 a=BtlChar_Get(i)->action;
        s32 recent=h->damage>0&&tick-h->tick<=45;
        s32 attacking=!TagTeam_LockedOff(i)&&TagTeam_Target(i)==actor &&
            ((a>=68&&a<=191)||(a>=253&&a<=315));
        Vec4 p=((BtlMovePose *)&BtlChar_Get(i)->pose)->pos;
        f32 x=p.x-own.x,y=p.y-own.y,z=p.z-own.z,d=x*x+y*y+z*z;
        s32 stamp=recent?h->tick:-1;
        if((recent||attacking) && (best<0 || stamp>latest || (stamp==latest&&d<nearest))) {
            best=i;latest=stamp;nearest=d;
        }
    }
    return best;
}
static s32 score(s32 actor,s32 candidate,s32 opening) {
    Decision *d=&decisions[actor];
    s32 i,current=TagTeam_Target(actor),age=tick-d->changed,value=512+(jitter()&(opening?1023:127));
    if(!opening) {
        Vec4 a=((BtlMovePose *)&BtlChar_Get(actor)->pose)->pos;
        Vec4 b=((BtlMovePose *)&BtlChar_Get(candidate)->pose)->pos;
        f32 x=a.x-b.x,y=a.y-b.y,z=a.z-b.z,dist=x*x+y*y+z*z;
        s32 hurt=grudge(actor,candidate)/8,action=BtlChar_Get(candidate)->action;
        value+=hurt>800?800:hurt;
        value+=dist<256*256?350:dist<800*800?220:dist<2000*2000?80:0;
        if(TagTeam_Target(candidate)==actor)value+=100;
        if(action>=192&&action<226)value+=80;
        if(BtlMember_GetActiveGauge((BtlMemberChr *)BtlChar_Get(candidate))->health<8000)value+=60;
        if(candidate==d->previous&&age<240)value-=180;
        if(candidate==current)value+=age<180?160:age<360?80:0;
    }
    for(i=0;i<TagTeam_Count();i++) if(i!=actor&&i!=candidate&&living(i)&&cpu(i)&&TagTeam_Target(i)==candidate)value-=120;
    return value>0?value:0;
}
void TagCpu_Update(void) {
    s32 actor;
    if(!TagTeam_Active() || !BtlSeq_IsFighting() ||
       (Battle_GetWork()->flags&(BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOADING)))return;
    ++tick;
    for(actor=0;actor<TagTeam_Count();actor++) {
        Decision *d=&decisions[actor];
        s32 current=TagTeam_Target(actor),candidate,best=-1,bestScore=-1,opening=!d->ready;
        if(!living(actor)||!cpu(actor)) {
            /* A revival or later CPU takeover starts with a fresh decision,
               rather than resurrecting a grudge from before the KO. */
            memset(d,0,sizeof(*d));d->previous=-1;
            if(!living(actor))memset(threats[actor],0,sizeof(threats[actor]));
            continue;
        }
        if(!safe(actor))continue;
        if(living(current)&&TagTeam_IsEnemy(actor,current)) {
            if(opening && TagSession_Mode()!=TAG_MODE_FREE_FOR_ALL) best=current;
            else if(d->ready) {
                s32 threat=0;
                if(tick-d->changed<60 || tick<d->evaluate)continue;
                if(tick<d->reconsider) {
                    for(candidate=0;candidate<TagTeam_Count();candidate++)
                        if(candidate!=current && living(candidate)&&TagTeam_IsEnemy(actor,candidate)&&grudge(actor,candidate)>=2000)threat=1;
                    if(!threat){d->evaluate=tick+15;continue;}
                }
            }
        }
        if(best<0) for(candidate=0;candidate<TagTeam_Count();candidate++) if(living(candidate)&&TagTeam_IsEnemy(actor,candidate)) {
            s32 value=score(actor,candidate,opening&&TagSession_Mode()==TAG_MODE_FREE_FOR_ALL);
            if(value>bestScore){bestScore=value;best=candidate;}
        }
        if(best<0)continue;
        if(best!=current && TagTeam_SetCpuTarget(actor,best)) {
            extern void Port_TagCpuTrace(s32,s32,s32,s32);
            Port_TagCpuTrace(actor,current,best,grudge(actor,best));
            d->previous=current;d->changed=tick;
        }
        if(opening)d->changed=tick;
        d->ready=1;d->evaluate=tick+15;d->reconsider=tick+120+(jitter()&127);
    }
}
