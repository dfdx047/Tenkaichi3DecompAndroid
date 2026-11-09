#include "battle/tag_lobby_rules.h"
#include <string.h>

static int member_valid(const TlRoom *r,int id) {
    return id>=0 && id<TL_MEMBERS && r->members[id].present;
}
static int loadout_valid(const TlLoadout *f) {
    int i;
    /* Disc-specific character/outfit checks belong at the network boundary. */
    if(f->chara<0 || f->chara>1023 || f->costume<0 || f->costume>31)return 0;
    for(i=0;i<8;i++)if(f->items[i]<0 || f->items[i]>350)return 0;
    return 1;
}
static void invalidate(TlRoom *r) {
    int i;r->revision++;
    for(i=0;i<TL_MEMBERS;i++)r->members[i].ready=r->members[i].loaded=0;
}
void TlRoom_Init(TlRoom *r,const TlLoadout *host) {
    int i;memset(r,0,sizeof(*r));r->revision=1;r->generation=1;
    r->members[0].present=r->members[0].generation=1;
    r->members[0].loadout=*host;
    r->draft.music=24;r->draft.time=3;r->draft.difficulty=2;r->draft.teams=2;
    r->draft.count=2;r->draft.team[1]=1;r->draft.playerLimit=10;
    for(i=0;i<10;i++) {
        r->draft.owner[i]=-1;r->draft.cpu[i]=-1;r->draft.fighters[i]=*host;
    }
    r->draft.owner[0]=0;
}
int TlRoom_Join(TlRoom *r,const TlLoadout *fighter) {
    int i;
    if(r->phase==TL_CLOSED || !loadout_valid(fighter))return -1;
    for(i=1;i<TL_MEMBERS;i++)if(!r->members[i].present) {
        TlMember *m=&r->members[i];memset(m,0,sizeof(*m));m->present=1;
        m->generation=++r->generation;m->loadout=*fighter;m->team=m->slot=-1;
        /* Arrivals follow the current room page. A guest joining during team
           setup must not send everyone else back to the practice map. */
        if(r->phase==TL_WAITING || r->phase==TL_READY_CHECK)invalidate(r);
        return i;
    }
    return -1;
}
int TlRoom_MatchMask(const TlRoom *r) {
    int i,mask=0;
    for(i=0;i<r->draft.count;i++)if(member_valid(r,r->draft.owner[i]))mask|=1<<r->draft.owner[i];
    return mask;
}
int TlRoom_PracticeMask(const TlRoom *r) {
    int i,mask=0;
    for(i=1;i<TL_MEMBERS;i++)if(r->members[i].present)mask|=1<<i;
    /* Team setup brings everybody out together. Unclaimed members watch the
       same setup and the ensuing match; there is no second practice session. */
    if(r->phase!=TL_WAITING)mask=0;
    return mask;
}
int TlRoom_Leave(TlRoom *r,int member) {
    int i;
    if(!member_valid(r,member))return TL_INVALID;
    r->members[member].present=0;
    if(member==TL_HOST){r->phase=TL_CLOSED;return TL_OK;}
    for(i=0;i<r->draft.count;i++)if(r->draft.owner[i]==member)r->draft.owner[i]=-1;
    /* A guest departure releases only that guest; it never reassigns IDs or
       erases everybody's room. The active match replaces their seat with CPU. */
    if(r->phase<=TL_READY_CHECK)invalidate(r);
    return TL_OK;
}
int TlRoom_Kick(TlRoom *r,int sender,int member) {
    if(sender!=TL_HOST || member==TL_HOST || !member_valid(r,sender))return TL_DENIED;
    return TlRoom_Leave(r,member);
}
int TlDraft_Valid(const TlRoom *r,const TlDraft *d) {
    int i,counts[4]={0},mask=0;
    if(d->mode<TL_TEAM || d->mode>TL_TOURNAMENT || d->count<1 || d->count>10 ||
       d->playerLimit<1 || d->playerLimit>(d->mode==TL_GAUNTLET?5:10) ||
       d->stage<0 || d->music<0 || d->time<0 || d->time>5 || d->difficulty<0 || d->difficulty>=TL_DIFFICULTIES ||
       d->preset<0 || d->heal<0 || d->heal>4)return 0;
    if(d->mode==TL_GAUNTLET) {
        if(d->count>5 || d->teams!=1)return 0;
    } else if(d->mode==TL_TOURNAMENT) {
        if(d->teams<1 || d->teams>4)return 0;
    } else if(d->mode==TL_FFA) {
        if(d->count<2 || d->teams!=d->count)return 0;
    } else if(d->count<2 || d->teams!=2)return 0;
    for(i=0;i<d->count;i++) {
        int owner=d->owner[i];
        if(d->cpu[i]<-1 || d->cpu[i]>=TL_DIFFICULTIES || !loadout_valid(&d->fighters[i]))return 0;
        if(owner < -1 || owner>=TL_MEMBERS || (owner>=0 && (!member_valid(r,owner) || (mask&(1<<owner)))))return 0;
        if(owner>=0)mask|=1<<owner;
        if(d->mode==TL_FFA) {if(d->team[i]!=i)return 0;}
        else if(d->team[i]<0 || d->team[i]>=d->teams || ++counts[d->team[i]]>5)return 0;
    }
    if(d->mode!=TL_FFA)for(i=0;i<d->teams;i++)if(!counts[i])return 0;
    {int players=0;for(i=0;i<10;i++)if(mask&(1<<i))players++;if(players>d->playerLimit)return 0;}
    return 1;
}
int TlRoom_Edit(TlRoom *r,int sender,int revision,const TlDraft *draft) {
    if(sender!=TL_HOST || !member_valid(r,sender))return TL_DENIED;
    if(r->phase>TL_READY_CHECK)return TL_BUSY;
    if(revision!=r->revision)return TL_STALE;
    if(!TlDraft_Valid(r,draft))return TL_INVALID;
    r->draft=*draft;invalidate(r);r->phase=TL_READY_CHECK;return TL_OK;
}
int TlRoom_CancelSetup(TlRoom *r,int sender,int revision) {
    if(sender!=TL_HOST || !member_valid(r,sender))return TL_DENIED;
    if(r->phase!=TL_READY_CHECK)return TL_BUSY;
    if(revision!=r->revision)return TL_STALE;
    invalidate(r);r->phase=TL_WAITING;return TL_OK;
}
int TlRoom_Ready(TlRoom *r,int sender,int revision,int ready) {
    if(!member_valid(r,sender) || !(TlRoom_MatchMask(r)&(1<<sender)))return TL_DENIED;
    if(r->phase!=TL_READY_CHECK)return TL_BUSY;
    if(revision!=r->revision)return TL_STALE;
    r->members[sender].ready=ready!=0;return TL_OK;
}
int TlRoom_Claim(TlRoom *r,int sender,int revision,int playing,int team) {
    TlDraft d;int i,old=-1,free=-1,claimed=0;
    if(!member_valid(r,sender))return TL_DENIED;
    if(r->phase!=TL_READY_CHECK)return TL_BUSY;
    if(revision!=r->revision)return TL_STALE;
    d=r->draft;
    for(i=0;i<d.count;i++) {
        if(d.owner[i]==sender)old=i;
        if(d.owner[i]>=0)claimed++;
        if(d.owner[i]<0 && (team<0 || d.team[i]==team) && free<0)free=i;
    }
    if(!playing) {
        if(sender==TL_HOST)return TL_DENIED;
        if(old>=0){r->draft.owner[old]=-1;invalidate(r);}return TL_OK;
    }
    if(team>=d.teams || team < -1)return TL_INVALID;
    if(old>=0 && (team<0 || team==d.team[old]))return TL_OK;
    if(old<0 && claimed>=d.playerLimit)return TL_FULL;
    if(free<0)return TL_FULL;
    if(old>=0)d.owner[old]=-1;
    d.owner[free]=sender;d.fighters[free]=r->members[sender].loadout;
    if(!TlDraft_Valid(r,&d))return TL_INVALID;
    r->draft=d;invalidate(r);return TL_OK;
}
int TlRoom_Start(TlRoom *r,int sender,int revision) {
    int i,mask;
    if(sender!=TL_HOST || !member_valid(r,sender))return TL_DENIED;
    if(r->phase!=TL_READY_CHECK)return TL_BUSY;
    if(revision!=r->revision)return TL_STALE;
    if(!TlDraft_Valid(r,&r->draft))return TL_INVALID;
    mask=TlRoom_MatchMask(r);
    for(i=0;i<TL_MEMBERS;i++)if((mask&(1<<i)) && !r->members[i].ready)return TL_NOT_READY;
    for(i=0;i<TL_MEMBERS;i++)r->members[i].loaded=0;
    r->phase=TL_LOADING;r->round++;return TL_OK;
}
int TlRoom_Loaded(TlRoom *r,int sender,int round) {
    int i,mask=TlRoom_MatchMask(r)|(1<<TL_HOST);
    if(!member_valid(r,sender) || !(mask&(1<<sender)))return TL_DENIED;
    if(round!=r->round)return TL_STALE;
    if(r->phase!=TL_LOADING)return TL_BUSY;
    r->members[sender].loaded=1;
    for(i=0;i<TL_MEMBERS;i++)if((mask&(1<<i)) && !r->members[i].loaded)return TL_OK;
    r->phase=TL_MATCH;return TL_OK;
}
int TlRoom_Return(TlRoom *r,int sender,int round) {
    if(sender!=TL_HOST || !member_valid(r,sender))return TL_DENIED;
    if(round!=r->round)return TL_STALE;
    if(r->phase!=TL_MATCH && r->phase!=TL_LOADING)return TL_BUSY;
    invalidate(r);r->phase=TL_WAITING;return TL_OK;
}
int TlRoom_SetFighter(TlRoom *r,int sender,int generation,const TlLoadout *fighter) {
    int i;
    if(!member_valid(r,sender))return TL_DENIED;
    if(generation!=r->members[sender].generation)return TL_STALE;
    if(!loadout_valid(fighter))return TL_INVALID;
    if(r->phase>=TL_LOADING && (TlRoom_MatchMask(r)&(1<<sender)))return TL_BUSY;
    r->members[sender].loadout=*fighter;
    for(i=0;i<r->draft.count;i++)if(r->draft.owner[i]==sender)r->draft.fighters[i]=*fighter;
    if(r->phase<=TL_READY_CHECK)invalidate(r);
    return TL_OK;
}

static int actor_valid(const TlPractice *p,int id) {
    return id>=0 && id<TL_ACTORS && p->actors[id].enabled;
}
static int available(const TlPractice *p,int id) {
    return actor_valid(p,id) && p->actors[id].hp>0 && !p->actors[id].recovering &&
           !p->actors[id].editing && p->actors[id].opponent<0;
}
static void clear_actor(TlPracticeActor *a,int hp) {
    memset(a,0,sizeof(*a));a->member=a->difficulty=a->opponent=a->request=-1;a->hp=a->maxHp=hp;
}
void TlPractice_Init(TlPractice *p,const TlRoom *r,int health) {
    int i;memset(p,0,sizeof(*p));
    for(i=0;i<TL_ACTORS;i++)clear_actor(&p->actors[i],health);
    for(i=0;i<TL_DUMMIES;i++) {
        TlPracticeActor *a=&p->actors[TL_MEMBERS-1+i];a->enabled=1;a->difficulty=i-1;
    }
    TlPractice_Refresh(p,r,health);
    for(i=0;i<TL_MEMBERS;i++){p->wins[i]=r->members[i].wins;p->losses[i]=r->members[i].losses;p->draws[i]=r->members[i].draws;}
}
void TlPractice_Refresh(TlPractice *p,const TlRoom *r,int health) {
    int id,mask=TlRoom_PracticeMask(r);
    for(id=1;id<TL_MEMBERS;id++) {
        TlPracticeActor *a=&p->actors[id-1];
        if(!(mask&(1<<id)))TlPractice_Remove(p,id-1);
        else if(!a->enabled) {clear_actor(a,health);a->enabled=1;a->member=id;}
    }
}
int TlPractice_MemberActor(const TlPractice *p,int member) {
    if(member<=TL_HOST || member>=TL_MEMBERS || !p->actors[member-1].enabled)return -1;
    return member-1;
}
int TlPractice_Challenge(TlPractice *p,int a,int b,int nearby) {
    TlPracticeActor *x,*y;
    if(a==b || !nearby || !available(p,a) || !available(p,b))return TL_BUSY;
    x=&p->actors[a];y=&p->actors[b];
    if(x->member<1 || y->member<1)return TL_DENIED;
    if(y->request==a && y->requestTicks>0) {
        x->opponent=b;y->opponent=a;
        x->request=y->request=-1;x->requestTicks=y->requestTicks=0;
    } else {x->request=b;x->requestTicks=TL_REQUEST_TICKS;}
    return TL_OK;
}
int TlPractice_CanHit(const TlPractice *p,int a,int b) {
    const TlPracticeActor *x,*y;
    if(a==b || !actor_valid(p,a) || !actor_valid(p,b))return 0;
    x=&p->actors[a];y=&p->actors[b];
    if(x->editing || y->editing || x->recovering || y->recovering || x->hp<=0 || y->hp<=0)return 0;
    if(x->opponent==b && y->opponent==a)return x->member>0 || x->difficulty>=0;
    /* A neutral player may initiate a dummy fight. Dummies may never start one. */
    return x->member>0 && y->member<0 && x->opponent<0 && y->opponent<0;
}
int TlPractice_Contact(TlPractice *p,int a,int b) {
    if(!TlPractice_CanHit(p,a,b))return 0;
    if(p->actors[a].opponent<0) {
        p->actors[a].opponent=b;p->actors[b].opponent=a;
        p->actors[a].request=p->actors[b].request=-1;
    }
    return 1;
}
int TlPractice_CpuActive(const TlPractice *p,int actor) {
    return actor_valid(p,actor) && p->actors[actor].member<0 && p->actors[actor].difficulty>=0 &&
           p->actors[actor].opponent>=0 && !p->actors[actor].recovering && p->actors[actor].hp>0;
}
int TlPractice_Edit(TlPractice *p,int actor,int editing) {
    if(!actor_valid(p,actor) || p->actors[actor].member<1)return TL_DENIED;
    if(editing && !available(p,actor))return TL_BUSY;
    p->actors[actor].editing=editing!=0;p->actors[actor].request=-1;p->actors[actor].requestTicks=0;
    return TL_OK;
}
void TlPractice_Remove(TlPractice *p,int actor) {
    int i;
    if(actor<0 || actor>=TL_ACTORS)return;
    for(i=0;i<TL_ACTORS;i++) {
        TlPracticeActor *a=&p->actors[i];
        if(a->opponent==actor) {a->opponent=-1;a->recovering=TL_RECOVER_TICKS;}
        if(a->request==actor){a->request=-1;a->requestTicks=0;}
    }
    clear_actor(&p->actors[actor],0);
}
unsigned TlPractice_Tick(TlPractice *p) {
    int i;unsigned reset=0;
    /* Resolve pairs before independent recoveries, so simultaneous KOs and
       a lingering projectile cannot award a second win. */
    for(i=0;i<TL_ACTORS;i++) {
        TlPracticeActor *a=&p->actors[i];int j=a->opponent;
        if(!a->enabled || j<=i || !actor_valid(p,j))continue;
        {
            TlPracticeActor *b=&p->actors[j];int am=a->member,bm=b->member;
            if(a->hp>0 && b->hp>0)continue;
            if(am>0 && bm>0) {
                if(a->hp<=0 && b->hp<=0){p->draws[am]++;p->draws[bm]++;}
                else if(a->hp<=0){p->losses[am]++;p->wins[bm]++;}
                else {p->wins[am]++;p->losses[bm]++;}
            }
            a->opponent=b->opponent=-1;
            a->recovering=b->recovering=TL_RECOVER_TICKS+1;
        }
    }
    for(i=0;i<TL_ACTORS;i++) {
        TlPracticeActor *a=&p->actors[i];if(!a->enabled)continue;
        if(a->requestTicks>0 && --a->requestTicks==0)a->request=-1;
        if(a->hp<=0 && !a->recovering)a->recovering=TL_RECOVER_TICKS+1;
        if(a->recovering>0 && --a->recovering==0) {
            a->hp=a->maxHp;a->opponent=a->request=-1;a->requestTicks=0;reset|=1u<<i;
        }
    }
    return reset;
}
