/* Ginyu's two-body transaction. Native simulation state, included in rollback. */
#ifdef PORT
#include "common.h"
#include "battle/btl_char_mgr.h"
#include "battle/bt4_body_change.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

enum { BODY_IDLE, BODY_WAIT, BODY_REQUEST, BODY_LOADING, BODY_READY, BODY_LOADED, BODY_DONE, BODY_LEFT };
typedef struct Bt4BodyChange {
    s32 state,peer,member;
    s32 chara,costume,variant,health,healthMax;
    s32 ki,blast;
} Bt4BodyChange;
static Bt4BodyChange sBody[10];

extern s32 BtlChar_GetCount(void),BtlAct_GetCurrent(BtlMgrChr *);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *);
extern void BtlAct_Request(BtlMgrChr *,s32),BtlAct_SetFormCurrent(BtlMgrChr *);
extern void BtlChar_SavePlacement(BtlMgrChr *),BtlMember_ClearQueuedDamage(BtlMgrChr *);
extern void BtlChar_SetFlag(BtlMgrChr *,s32),BtlChar_ClearFlag(BtlMgrChr *,s32);
extern s32 BtlChange_TryRequestChara(s32,s32,s32,s32,s32,s32,s32);
extern s32 BtlChange_IsLoadedFor(s32);
extern void BtlChange_SetReady(s32),BtlChange_SetDone(s32);
extern void BtlAnim_Request(BtlMgrChr *,s32,f32);
extern void BtlObj_BindCommonTables(BtlMgrObj *);
extern BtlMgrObj *BtlChar_GetObj(BtlMgrChr *);
extern void BtlMember_LoadParams(BtlMgrChr *,s32,s32,f32,s32);

static s32 body_rush(s32 action){return (action>=0x12D&&action<=0x12F)||(action>=0x139&&action<=0x13B);}
static s32 body_valid(s32 actor){return actor>=0&&actor<BtlChar_GetCount()&&actor<10;}
static s32 body_clamp(s32 n,s32 max){return n<0?0:n>max?max:n;}
void Bt4BodyChange_Reset(void){memset(sBody,0,sizeof(sBody));}
s32 Bt4BodyChange_Active(s32 actor){return body_valid(actor)&&sBody[actor].state!=BODY_IDLE;}
void Bt4BodyChange_Queue(s32 attacker,s32 victim){
    if(!body_valid(attacker)||!body_valid(victim)||attacker==victim||sBody[attacker].state||sBody[victim].state)return;
    sBody[attacker].state=sBody[victim].state=BODY_WAIT;
    sBody[attacker].peer=victim;sBody[victim].peer=attacker;
    sBody[attacker].member=BtlChar_Get(attacker)->curMember;
    sBody[victim].member=BtlChar_Get(victim)->curMember;
}
static void body_capture(s32 actor,s32 target){
    BtlMgrChr *chr=BtlChar_Get(actor),*other=BtlChar_Get(target);
    BtlMgrMember *m=BtlMember_GetActive(chr),*body=BtlMember_GetActive(other);
    Bt4BodyChange *p=&sBody[actor];
    p->chara=body->chara;p->costume=body->costume;p->variant=body->gauge.unk20;
    p->health=body->gauge.health;p->healthMax=body->gauge.healthMax;
    p->ki=m->gauge.ki;p->blast=m->gauge.blast;
}
void Bt4BodyChange_Update(void){
    s32 a,b;
    for(a=0;a<BtlChar_GetCount()&&a<10;++a){
        Bt4BodyChange *p=&sBody[a];b=p->peer;
        if(!p->state||!body_valid(b)||b<=a)continue;
        if(p->state==BODY_WAIT&&sBody[b].state==BODY_WAIT){
            BtlMgrChr *left=BtlChar_Get(a),*right=BtlChar_Get(b);
            if(body_rush(BtlAct_GetCurrent(left))||body_rush(BtlAct_GetCurrent(right)))continue;
            if(left->curMember!=p->member||right->curMember!=sBody[b].member||
               BtlMember_GetActive(left)->gauge.health<=0||BtlMember_GetActive(right)->gauge.health<=0){
                memset(p,0,sizeof(*p));memset(&sBody[b],0,sizeof(sBody[b]));continue;
            }
            /* Capture both before either record or model is changed. */
            body_capture(a,b);body_capture(b,a);
            if(getenv("BT4_RENDER_TEST")&&getenv("BT4_BODY_CHANGE_TRACE"))
                fprintf(stderr,"body exchange: begin actors=%d,%d targets=%d,%d hp=%d,%d max=%d,%d\n",a,b,p->chara,sBody[b].chara,p->health,sBody[b].health,p->healthMax,sBody[b].healthMax);
            p->state=sBody[b].state=BODY_REQUEST;
            BtlAct_Request(left,0x103);BtlAct_Request(right,0x103);
        }else if(p->state==BODY_LEFT&&sBody[b].state==BODY_LEFT){
            if(getenv("BT4_RENDER_TEST")&&getenv("BT4_BODY_CHANGE_TRACE"))fprintf(stderr,"body exchange: completed actors=%d,%d\n",a,b);
            memset(p,0,sizeof(*p));memset(&sBody[b],0,sizeof(sBody[b]));
        }
    }
}
void Bt4BodyChange_Action(void *fighter,s32 phase){
    BtlMgrChr *chr=fighter;Bt4BodyChange *p=&sBody[chr->side];s32 bit;
    if(phase==0){
        BtlChar_SavePlacement(chr);BtlMember_ClearQueuedDamage(chr);BtlChar_ClearFlag(chr,6);
        BtlChar_ClearFlag(chr,0x125);chr->freeze=chr->freezeNext=chr->freezeDelay=0;
    }
    if(phase==1){
        if(p->state==BODY_REQUEST){
            BtlAct_SetFormCurrent(chr);chr->newChara=p->chara;chr->newCostume=p->costume;chr->new20=p->variant;
            if(BtlChange_TryRequestChara(chr->side,p->chara,p->costume,p->variant,-1,-1,-1))p->state=BODY_LOADING;
        }else if(p->state==BODY_LOADING&&BtlChange_IsLoadedFor(chr->side)){
            BtlChange_SetReady(chr->side);BtlAnim_Request(chr,0,0.0f);p->state=BODY_READY;
        }else if(p->state==BODY_LOADED){
            BtlChange_SetDone(chr->side);p->state=BODY_DONE;
        }else if(p->state==BODY_DONE&&sBody[p->peer].state>=BODY_DONE){
            BtlChar_SetFlag(chr,0xF5);BtlChar_SetFlag(chr,0x55);BtlAct_Request(chr,0xB);
        }
        /* Only these two actors are protected while their models stream. */
        for(bit=0x42;bit<=0x46;++bit)BtlChar_SetFlag(chr,bit);
        /* Two level-2 hit-stop owners would freeze each other while the
           sequential loader waits for the first owner's completion. */
        BtlChar_SetFlag(chr,0x136);
        BtlChar_SetFlag(chr,0xBB);BtlChar_SetFlag(chr,0x128);
    }
    if(phase==3)p->state=BODY_LEFT;
}
s32 Bt4BodyChange_OnModelLoaded(void *fighter){
    BtlMgrChr *chr=fighter;Bt4BodyChange *p;
    BtlMgrMember *m;BtlMgrGauge *g;
    if(!body_valid(chr->side))return 0;
    p=&sBody[chr->side];
    if(p->state!=BODY_READY||BtlAct_GetCurrent(chr)!=0x103)return 0;
    m=BtlMember_GetActive(chr);m->chara=p->chara;m->costume=p->costume;
    m->gauge.unk20=p->variant;
    BtlObj_BindCommonTables(BtlChar_GetObj(chr));
    BtlMember_LoadParams(chr,chr->curMember,0,0.0f,0);
    g=&m->gauge;g->healthMax=p->healthMax;g->health=body_clamp(p->health,p->healthMax);
    g->ki=body_clamp(p->ki,g->kiMax);g->blast=body_clamp(p->blast,g->blastMax);
    g->unk20=p->variant;g->unk1C=0;g->lowHealth=g->health<10000;g->unk30=1;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_BODY_CHANGE_TRACE"))
        fprintf(stderr,"body exchange: loaded actor=%d char=%d costume=%d hp=%d max=%d\n",chr->side,m->chara,m->costume,g->health,g->healthMax);
    p->state=BODY_LOADED;return 1;
}
#endif
