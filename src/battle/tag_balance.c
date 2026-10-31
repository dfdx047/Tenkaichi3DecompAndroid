/* Opt-in assistance for a smaller living team or a fighter being surrounded.
   Damage always carries the actual hit owner, never the victim's selected target. */
#include "common.h"
#include "battle/tag_balance.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
#include "battle/tag_fusion.h"
#include "battle/tag_clash.h"
#include "battle/tag_session.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_seq.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern s32 BtlChar_IsFrozen(BtlMgrChr *),BtlCharApi_IsInRushSequence(s32),BtlCharApi_IsChanging(s32);
extern s32 BtlMember_Damage(void *,s32,s32);
extern void BtlAct_Request(BtlMgrChr *,s32);
extern void *memset(void *,s32,u32);
typedef struct Balance {
    s32 helped,dealt,taken,protect,cooldown,action,fraction,armor;
} Balance;
static Balance rows[PORT_FIGHTER_CAPACITY];
static s32 value(s32 option) {
    s32 preset=Port_TagOption(TAG_OPT_OUTNUMBERED);
    if(preset==1 || preset==2) {
        switch(option) {
        case TAG_OPT_OUT_DAMAGE: return preset==1?15:40;
        case TAG_OPT_OUT_DEFENSE:return preset==1?10:30;
        case TAG_OPT_OUT_RECOVERY:return preset==1?150:200;
        case TAG_OPT_OUT_PROTECTION:return preset==1?5:10;
        case TAG_OPT_OUT_BREAKER:return preset==1?12:8;
        }
    }
    return Port_TagOption(option);
}
static s32 live(s32 i) {return TagTeam_IsAlive(i)&&!TagFusion_Hidden(i);}
static s32 paired(s32 i) {
    BtlMgrChr *c=BtlChar_Get(i);
    return (c->action>=183&&c->action<=188) || BtlClash_Opponent(i)>=0 ||
        BtlCharApi_IsInRushSequence(c->objId) || BtlCharApi_IsChanging(c->objId);
}
void TagBalance_Reset(void) {memset(rows,0,sizeof(rows));}
void TagBalance_Update(void) {
    s32 i,j;
    if(!TagTeam_Active() || !BtlSeq_IsFighting() || !Port_TagOption(TAG_OPT_OUTNUMBERED)) {
        if(TagTeam_Active()) for(i=0;i<TagTeam_Count();i++) {
            BtlMgrChr *c=BtlChar_Get(i);
            /* Leave a longer protection timer from the base game alone. */
            if(rows[i].armor && c->unkE18<=rows[i].armor && c->unkE18>=rows[i].armor-1)
                c->unkE18=0;
        }
        TagBalance_Reset();return;
    }
    for(i=0;i<TagTeam_Count();i++) {
        Balance *b=&rows[i];BtlMgrChr *c=BtlChar_Get(i);
        s32 own=0,enemy=0,attackers=0,small,threshold=value(TAG_OPT_OUT_BREAKER);
        if(b->armor && c->unkE18<=b->armor && c->unkE18>=b->armor-1)c->unkE18=0;
        b->armor=0;
        b->helped=0;b->dealt=b->taken=100;
        if(!live(i)) {b->protect=b->cooldown=0;continue;}
        for(j=0;j<TagTeam_Count();j++) if(live(j)) {
            if(TagTeam_IsEnemy(i,j)) {enemy++;if(TagTeam_Target(j)==i) attackers++;}
            else own++;
        }
        small=TagSession_Mode()!=TAG_MODE_FREE_FOR_ALL && own>0 && own<enemy;
        b->helped=(small || (Port_TagOption(TAG_OPT_OUT_SCOPE)&&attackers>=2)) &&
            (!Port_TagOption(TAG_OPT_OUT_HUMANS)||TagTeam_InputPlayer(i)>=0);
        if(small&&b->helped) {
            b->dealt=100+value(TAG_OPT_OUT_DAMAGE)*(enemy-own)/own;
            b->taken=100-value(TAG_OPT_OUT_DEFENSE)*(enemy-own)/own;
            if(b->dealt>300)b->dealt=300;
            if(b->taken<40)b->taken=40;
        }
        if(BtlChar_IsFrozen(c)||TagTeam_CinematicPaused(i)) continue;
        if(b->protect)b->protect--;
        if(b->cooldown)b->cooldown--;
        if(!b->helped) {b->protect=0;b->fraction=0;}
        else if(!paired(i)) {
            if(c->action!=b->action && c->action>=225&&c->action<=230)
                b->protect=value(TAG_OPT_OUT_PROTECTION)*3;
            if(threshold && !b->cooldown && c->unkD44>=threshold && c->action>=189&&c->action<=224) {
                b->protect=30;b->cooldown=150;
                BtlAct_Request(c,0xE6);
            }
            if(b->protect && c->unkE18<b->protect)c->unkE18=b->armor=b->protect;
        }
        b->action=c->action;
    }
}
s32 TagBalance_Modify(s32 victim,s32 source,s32 damage,s32 flags) {
    Balance *b;
    if(!TagTeam_Active() || !Port_TagOption(TAG_OPT_OUTNUMBERED) ||
       (u32)victim>=(u32)TagTeam_Count() || damage<=0 || (flags&1)) return damage;
    if((u32)source>=(u32)TagTeam_Count() || !TagTeam_IsEnemy(victim,source)) return damage;
    b=&rows[victim];
    if(b->protect && !(flags&0x3800000) && !paired(victim) && !paired(source)) return 0;
    damage=damage*(rows[source].dealt?rows[source].dealt:100)/100;
    damage=damage*(b->taken?b->taken:100)/100;
    return damage<1?1:damage;
}
s32 TagBalance_Damage(void *victim,s32 damage,s32 flags,s32 source) {
    extern void TagCpu_Damage(s32,s32,s32);
    s32 lost=BtlMember_Damage(victim,TagBalance_Modify(*(s32 *)victim,source,damage,flags),flags);
    TagCpu_Damage(*(s32 *)victim,source,lost);
    return lost;
}
f32 TagBalance_Recovery(s32 actor) {
    s32 action;
    if(!TagTeam_Active() || (u32)actor>=(u32)TagTeam_Count() ||
       !Port_TagOption(TAG_OPT_OUTNUMBERED) || !rows[actor].helped || paired(actor)) return 1;
    action=BtlChar_Get(actor)->action;
    if(action==189 || (action>=192&&action<=230)) return (f32)value(TAG_OPT_OUT_RECOVERY)/100.0f;
    return 1;
}
s32 TagBalance_AgeStep(s32 actor) {
    s32 extra=TagBalance_Recovery(actor)>1 ? value(TAG_OPT_OUT_RECOVERY)-100 : 0;
    if(extra<=0)return 1;
    rows[actor].fraction+=extra;
    extra=rows[actor].fraction/100;rows[actor].fraction%=100;
    return 1+extra;
}
