#ifdef PORT
#include "common.h"
#include "battle/tag_challenge.h"
#include "battle/tag_team.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_seq.h"
#include "battle/tag_session.h"
#include "battle/tag_lobby.h"
#include "battle/tag_options.h"
#include "battle/tag_fusion.h"
#include "battle/tag_assist.h"
#include "battle/tag_revive.h"
#include "battle/stg_a.h"

extern void *memset(void *, s32, u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern s32 BtlChar_TestFlag(BtlMgrChr *, s32);
extern void BtlChar_ClearFlag(BtlMgrChr *, s32);
extern void BtlChar_SetHeldFlag(BtlMgrChr *, s32);
extern s32 BtlChar_IsFrozen(BtlMgrChr *);
extern s32 BtlColl_IsGuarding(BtlMgrChr *);
extern s32 BtlCharApi_IsInRushSequence(s32), BtlCharApi_IsChanging(s32);
extern void BtlAct_ClearQueue(BtlMgrChr *), BtlAct_Request(BtlMgrChr *, s32);
extern s32 BtlAct_GetRequested(BtlMgrChr *);
extern s32 Port_TagOption(s32);
extern void Port_TagRevived(s32, s32, s32);

static s32 progress[TAG_TEAM_CAPACITY], helper[TAG_TEAM_CAPACITY], previousHp[TAG_TEAM_CAPACITY], previousGuard[TAG_TEAM_CAPACITY];
static s32 trainingHp[TAG_TEAM_CAPACITY], trainingQuiet[TAG_TEAM_CAPACITY];
static s32 enabled = 1, duration = 90, cost = 2;
static f32 radius = 45.0f;

s32 TagRevive_FallenSupported(s32 actor) {
    BtlMgrChr *c;
    BtlMovePose *p;
    f32 bottom;
    s32 state;
    if(!TagTeam_Active() || !Port_TagOption(TAG_OPT_CORPSE_SAFETY) ||
       actor<0 || actor>=TagTeam_Count() || !BtlStage_IsReady() ||
       (Battle_GetWork()->flags&BATTLE_FLAG_LOADING)) return 0;
    state=BtlSeq_GetState();
    if(state<BTL_SEQ_FIGHT || state>BTL_SEQ_END) return 0;
    c=BtlChar_Get(actor);
    if((c->action!=0xD8 && c->action!=0xD9) || BtlMember_GetActiveGauge(c)->health>0 ||
       BtlChar_TestFlag(c,7) || TagFusion_Hidden(actor) || TagChallenge_Hidden(actor)) return 0;
    p=(BtlMovePose *)&c->pose;
    bottom=BtlStage_GetBottom();
    /* Bounds are read each update, including after stage destruction/expansion.
       Comparisons also reject NaN and infinite positions or stage parameters. */
    return bottom>-100000.0f && bottom<100000.0f && BtlStage_GetTop()<bottom &&
           p->pos.y>=bottom && p->pos.y<1000000.0f;
}

void TagRevive_CatchFallen(s32 actor) {
    BtlMgrChr *c;
    BtlMovePose *p;
    s32 next;
    if(!TagRevive_FallenSupported(actor)) return;
    c=BtlChar_Get(actor);p=(BtlMovePose *)&c->pose;
    p->pos.y=BtlStage_GetBottom();p->fallSpeed=0;
    next=BtlAct_GetRequested(c);
    if(c->action==0xD9 && (next==-1 || next==0xD9)) BtlAct_Request(c,0xD8);
}

s32 TagRevive_Progress(s32 i) { return progress[i]; }
s32 TagRevive_Helper(s32 i) { return helper[i]; }
s32 TagRevive_Enabled(void) { return !TagLobby_Practice() && Port_TagOption(TAG_OPT_REVIVE) && TagSession_Mode()!=TAG_MODE_FREE_FOR_ALL; }
s32 TagRevive_Duration(void) { return duration; }
s32 TagRevive_Cost(void) { return cost; }
f32 TagRevive_Radius(void) { return radius; }
s32 TagRevive_Nearby(s32 actor) {
    s32 i,best=-1;f32 nearest=radius*radius;
    BtlMovePose *own;
    if(!TagRevive_Enabled() || !TagTeam_IsAlive(actor))return -1;
    own=(BtlMovePose *)&BtlChar_Get(actor)->pose;
    for(i=0;i<TagTeam_Count();i++) if(i!=actor && !TagTeam_IsEnemy(actor,i) && !TagFusion_Hidden(i) && !TagChallenge_Hidden(i)) {
        BtlMgrChr *c=BtlChar_Get(i);BtlMovePose *p=(BtlMovePose *)&c->pose;
        f32 x=p->pos.x-own->pos.x,y=p->pos.y-own->pos.y,z=p->pos.z-own->pos.z,d=x*x+y*y+z*z;
        if(BtlMember_GetActiveGauge(c)->health<=0 && !BtlChar_TestFlag(c,7) && d<=nearest) {
            nearest=d;best=i;
        }
    }
    return best;
}
void TagRevive_Reset(void) {
    s32 i;
    memset(progress, 0, sizeof(progress));
    memset(previousHp, 0, sizeof(previousHp));
    memset(previousGuard, 0, sizeof(previousGuard));
    memset(trainingHp,0,sizeof(trainingHp));
    memset(trainingQuiet,0,sizeof(trainingQuiet));
    for (i=0;i<TAG_TEAM_CAPACITY;i++) helper[i]=-1;
}
static s32 busy(BtlMgrChr *c) {
    return BtlCharApi_IsInRushSequence(c->objId) || BtlCharApi_IsChanging(c->objId);
}
static void stand_up(BtlMgrChr *body) {
    BtlMgrGauge *g=BtlMember_GetActiveGauge(body);
    g->lowHealth=g->lowHealthIdle=0;
    BtlChar_ClearFlag(body,0x137);
    BtlChar_ClearFlag(body,0x96);
    BtlChar_ClearFlag(body,0xE7);
    body->freeze=body->freezeNext=body->freezeDelay=0;
    BtlAct_ClearQueue(body);
    BtlAct_Request(body,0xB);
}
void TagTraining_Update(void) {
    s32 i;
    if(TagLobby_Practice() || TagSession_Mode()!=TAG_MODE_TRAINING || !BtlSeq_IsFighting() ||
       (Battle_GetWork()->flags&(BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOADING))) return;
    for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        BtlMgrGauge *g=BtlMember_GetActiveGauge(c);
        if(TagFusion_Hidden(i) || TagAssist_Active(i)) continue;
        if(trainingHp[i]>g->health) trainingQuiet[i]=0;
        trainingHp[i]=g->health;
        if(busy(c) || BtlChar_IsFrozen(c)) continue;
        if(Port_TagOption(TAG_OPT_TRAIN_KI)) g->ki=g->kiMax;
        if(Port_TagOption(TAG_OPT_TRAIN_STOCKS)) g->blast=g->blastMax;
        if(trainingQuiet[i]<60) trainingQuiet[i]++;
        if(trainingQuiet[i]>=60 && (g->health<=0 || Port_TagOption(TAG_OPT_TRAIN_HEALTH))) {
            s32 dead=g->health<=0;
            g->health=g->healthMax;
            trainingHp[i]=g->health;
            if(dead) stand_up(c);
        }
    }
}
void TagRevive_Update(void) {
    s32 i,j, used=0, count=TagTeam_Count(), hit[TAG_TEAM_CAPACITY];
    enabled = TagRevive_Enabled(); duration=Port_TagOption(1)*30; cost=Port_TagOption(2); radius=(f32)Port_TagOption(3);
    if (Battle_GetWork()->flags & (BATTLE_FLAG_PAUSE | BATTLE_FLAG_LOADING)) return;
    for (i=0;i<count;i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        s32 hp=BtlMember_GetActiveGauge(c)->health, guard=BtlColl_IsGuarding(c);
        hit[i]=previousHp[i]>hp && !guard && !previousGuard[i];
        previousHp[i]=hp; previousGuard[i]=guard;
    }
    if (!BtlSeq_IsFighting()) return;
    for (i=0;i<count;i++) {
        BtlMgrChr *body=BtlChar_Get(i);
        BtlMgrGauge *g=BtlMember_GetActiveGauge(body);
        BtlMovePose *pos=(BtlMovePose *)&body->pose;
        s32 best=-1;
        f32 nearest=radius*radius;
        /* Ring-out is permanent. Movement, taunting and guarded chip damage are
         * deliberately not cancellation conditions. Each rescuer helps one body. */
        if (enabled && g->health<=0 && !TagFusion_Hidden(i) && !TagChallenge_Hidden(i) && !BtlChar_TestFlag(body,7)) {
            for (j=0;j<count;j++) {
                BtlMgrChr *c=BtlChar_Get(j);
                BtlMgrGauge *cg=BtlMember_GetActiveGauge(c);
                BtlMovePose *p=(BtlMovePose *)&c->pose;
                f32 x=p->pos.x-pos->pos.x,y=p->pos.y-pos->pos.y,z=p->pos.z-pos->pos.z,d=x*x+y*y+z*z;
                if (i==j || (used&(1<<j)) || TagFusion_Hidden(j) || TagAssist_Active(j) ||
                    TagTeam_IsEnemy(i,j) || cg->health<=0 ||
                    BtlChar_TestFlag(c,7) || cg->blast < cost*100000 || busy(c) || busy(body) || hit[j]) continue;
                if (d<=nearest) { nearest=d; best=j; }
            }
        }
        if (best<0) { progress[i]=0; helper[i]=-1; continue; }
        used |= 1<<best;
        if (helper[i]!=best) progress[i]=0;
        helper[i]=best;
        if (BtlChar_IsFrozen(body) || BtlChar_IsFrozen(BtlChar_Get(best))) continue;
        if (++progress[i]>=duration) {
            BtlMgrGauge *payer=BtlMember_GetActiveGauge(BtlChar_Get(best));
            payer->blast-=cost*100000;
            g->health=g->healthMax*Port_TagOption(TAG_OPT_REVIVE_HEALTH)/100; if (g->health<1) g->health=1;
            g->ki=g->kiMax/2; g->blast=0;
            stand_up(body);
            Port_TagRevived(i,best,g->health);
            progress[i]=0; helper[i]=-1; previousHp[i]=g->health;
        }
    }
}
#endif
