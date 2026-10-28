/* Team rosters travel with the native bracket entrants. The original bracket,
   match results and champion presentation remain responsible for progression. */
#include "common.h"
#include "menu/menu_k.h"
#include "menu/tag_tournament.h"
#include "battle/battle_setup.h"
#include "battle/tag_session.h"
#include "battle/tag_team.h"
#include "battle/btl_char_mgr.h"
#include "menu/port_screen.h"

extern s32 PortGame_IsBT4(void),PortGame_IsCharacter(s32);
extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
typedef struct TagEntry {
    s32 count,owners[5];
    BattleMember member[5];
} TagEntry;
static TagEntry draft[4],entry[17];
static s32 draftTeam, draftCount;
static s32 type,live,pads[4]={0,1,2,3};
static s32 matchEntry[2]={-1,-1};
static f32 remaining[17][5];
static s32 testAge,testMatches;
extern s32 Port_TagTournamentTest(void);
extern s32 Port_TagModeRushTest(void);
extern void Port_TagTestCheck(s32,s32);
s32 TagTournament_Active(void) {return live && TagSession_Mode()==TAG_MODE_TOURNAMENT;}
s32 TagTournament_DraftTeam(void) {return draftTeam;}
void TagTournament_Choose(s32 index) {type=index>=0 && index<5?index:0;}
void TagTournament_BeginDraft(void) {
    memset(draft,0,sizeof(draft));draftCount=0;draftTeam=0;
    if(TagSession_Players())while(draftTeam<3 && !TagSession_DraftSize(draftTeam))draftTeam++;
}
void TagTournament_Select(s32 direction) {type=(type+direction+5)%5;}
const char *TagTournament_Title(void) {
    static const char *names[]={"World Tournament","World Martial Arts Big Tournament","Cell Games","Otherworld Tournament","Yamcha Game"};
    return type==4 && PortGame_IsBT4()?"Tournament of Power":names[type];
}
void TagTournament_End(void) {s32 i;live=0;matchEntry[0]=matchEntry[1]=-1;for(i=0;i<4;i++)pads[i]=i;}
s32 TagTournament_Pad(s32 slot) {
    return live && TagTeam_Active() && (u32)slot<4?pads[slot]:slot;
}
s32 TagTournament_Start(void) {
    s32 t,m;BattleSetup *b=Battle_GetSetup();
    if(TagSession_Mode()!=TAG_MODE_TOURNAMENT)return 0;
    t=draftTeam;
    draft[t].count=b->sides[0].memberCount;
    for(m=0;m<draft[t].count;m++) {
        draft[t].member[m]=b->sides[0].members[m];
        draft[t].owners[m]=TagSession_DraftOwner(t,m);
    }
    draftCount++;
    while(++draftTeam<4) if(TagSession_DraftSize(draftTeam)) {
        extern void TeamSel_ResetTagRoster(void);
        TeamSel_ResetTagRoster();
        return 2; /* another entered team, on the original character grid */
    }
    /* Progress storage also holds the native team-select draft, so capture it
       before writing the tournament's persistent fields. */
    memset(&TOUR_PROG2->ents,0,sizeof(TOUR_PROG2->ents));
    memset(&TOUR_PROG2->matches,0,sizeof(TOUR_PROG2->matches));
    TOUR_PROG2->entry=1;TOUR_PROG2->tour=type;
    TOUR_PROG2->level=2;
    TOUR_PROG2->entryNum=draftCount;TOUR_PROG2->round=TOUR_PROG2->match=0;
    for(t=0,m=0;t<4;t++) if(draft[t].count) {
        BrkEntrant *e=&TOUR_PROG2->ents.e[m++];
        e->flags=TOUR_ENT_PLAYER;e->player=t;e->chara=draft[t].member[0].chara;
        e->costume=draft[t].member[0].costume;e->health=100;
        memcpy(&e->item,&draft[t].member[0].items,sizeof(e->item));
    }
    gProgress->flags&=~(MPROG_TOUR_RUNNING|MPROG_TOUR_GREETED);
    gProgress->mode=35;live=1;
    Port_TagMenuTrace(87,0,type,draftCount,0,TagSession_Players());
    return 1;
}
static s32 random_fighter(Bracket *b) {
    s32 candidates[256],n=0,i,j;
    for(i=0;i<b->gridCount;i++) {
        BrkCell *c=&b->grid[i];
        if(!PortGame_IsCharacter(c->id))continue;
        for(j=0;j<c->formCount && j<7;j++)
            if(PortGame_IsCharacter(c->form[j]) && n<256)candidates[n++]=c->form[j];
    }
    return n?candidates[Rand_Range(n)]:0;
}
void TagTournament_BuildTeams(void *ptr) {
    Bracket *b=ptr;s32 i,m;
    if(!live || TagSession_Mode()!=TAG_MODE_TOURNAMENT)return;
    for(i=0;i<17;i++) {
        BrkEntrant *e=&b->ents.e[i];
        TagEntry *team=&entry[i];
        for(m=0;m<5;m++)remaining[i][m]=100;
        if((e->flags&TOUR_ENT_PLAYER) && (u32)e->player<4) {*team=draft[e->player];continue;}
        team->count=1;for(m=0;m<4;m++)if(draft[m].count>team->count)team->count=draft[m].count;
        if(!PortGame_IsCharacter(e->chara))e->chara=random_fighter(b);
        for(m=0;m<team->count;m++) {
            memset(&team->member[m],0,sizeof(BattleMember));
            team->member[m].chara=m?random_fighter(b):e->chara;
            /* Stable native rush donor for the opt-in camera replay, even
               when the bracket's first match has two generated CPU teams. */
            if(Port_TagModeRushTest() && m==0)team->member[m].chara=0;
            team->member[m].cpuLevel=TagSession_Level();team->owners[m]=-1;
        }
    }
    Port_TagMenuTrace(87,1,type,entry[0].count,entry[16].count,0);
}
void TagTournament_SetupBattle(void *ptr) {
    Bracket *b=ptr;s32 ids[2],t,m,p,n=0,a=0,assigned[2][5],local[4]={-1,-1,-1,-1};
    BattleSetup *setup=Battle_GetSetup();
    if(!live || TagSession_Mode()!=TAG_MODE_TOURNAMENT)return;
    ids[0]=b->matches.m[b->match].ent[0];ids[1]=b->matches.m[b->match].ent[1];
    if(!(b->ents.e[ids[0]].flags&TOUR_ENT_PLAYER)) {t=ids[0];ids[0]=ids[1];ids[1]=t;}
    for(p=0;p<4;p++)for(t=0;t<2;t++)for(m=0;m<entry[ids[t]].count;m++)
        if(entry[ids[t]].owners[m]==p && local[p]<0) {local[p]=n;pads[n++]=p;}
    /* Keep this a permutation even when only P3/P4 are in the next match;
       a parked controller must not duplicate another player's pause/input. */
    {s32 next=n;for(p=0;p<4;p++)if(local[p]<0)pads[next++]=p;}
    matchEntry[0]=ids[0];matchEntry[1]=ids[1];
    setup->rule.timeLimit=0; /* Tournament rounds end by elimination. */
    testAge=0;
    for(t=0;t<2;t++) {
        TagEntry *team=&entry[ids[t]];BattleMember leader=setup->sides[t].members[0];
        BattleSetup_SetSide(t,BTL_CONTROL_CPU,0,team->count,1,1,0,NULL);
        for(m=0;m<team->count;m++) {
            BattleMember *f=&team->member[m];s32 owner=team->owners[m];
            s32 custom=(b->ents.e[ids[t]].flags&TOUR_ENT_PLAYER)!=0;
            f32 hp=type==2 && b->round?remaining[ids[t]][m]+20:100;
            if(hp>100)hp=100;
            BattleSetup_SetMember(t,m,f->chara,f->costume,0,
                f->cpuLevel,
                hp,custom?&f->items:&leader.items);
            assigned[t][m]=owner>=0?local[owner]:-1;
            if(Port_TagTournamentTest())Port_TagTestCheck(
                setup->sides[t].members[m].cpuLevel==f->cpuLevel,2202);
        }
    }
    TagTeam_Configure(n,n,0);
    for(m=0;m<5;m++)for(t=0;t<2;t++)if(m<entry[ids[t]].count)
        TagTeam_SetAssignment(a++,assigned[t][m]);
    Port_TagMenuTrace(87,2,ids[0],ids[1],n,a);
    if(Port_TagTournamentTest()) {
        Port_TagTestCheck(a>=4,2200);
        Port_TagTestCheck(pads[0]>=0 && pads[0]<4,2201);
        if(testMatches>=2)Port_TagTestCheck(1,99);
    }
}

void TagTournament_TestUpdate(void) {
    s32 i;
    extern s32 BtlSeq_IsFighting(void);
    extern s32 Port_TagModeRushTest(void);
    if(Port_TagModeRushTest() || !Port_TagTournamentTest() || !live || !BtlSeq_IsFighting())return;
    if(++testAge>=240)for(i=0;i<TagTeam_Count();i++) {
        BtlMgrGauge *g=BtlMember_GetActiveGauge(BtlChar_Get(i));
        g->health=TagTeam_Team(i)?0:g->healthMax;
    }
}

void TagTournament_FinishBattle(void) {
    s32 i,slot[2]={0,0};
    if(!live || !TagTeam_Active() || !(BattleResult_GetFlags()&3))return;
    if(Port_TagTournamentTest()) {testMatches++;Port_TagTestCheck((BattleResult_GetFlags()&3)==1,2202);}
    for(i=0;i<TagTeam_Count();i++) {
        s32 t=TagTeam_Team(i),m=slot[t]++;BtlMgrGauge *g=BtlMember_GetActiveGauge(BtlChar_Get(i));
        if((u32)matchEntry[t]<17 && m<5)
            remaining[matchEntry[t]][m]=g->healthMax>0?100.0f*g->health/g->healthMax:0;
    }
}

/* Every bracket match is offered in order. Watching never assigns a human
   controller or permits a spectator to take over a CPU entrant. */
s32 TagTournament_CpuMatch(void *ptr) {
    Bracket *b=ptr;s32 t,m;
    if(!TagTournament_Active() || b->match>=16)return 0;
    for(t=0;t<2;t++)for(m=0;m<entry[b->matches.m[b->match].ent[t]].count;m++)
        if(entry[b->matches.m[b->match].ent[t]].owners[m]>=0)return 0;
    return 1;
}
void TagTournament_SkipMatch(void *ptr) {
    Bracket *b=ptr;
    if(!TagTournament_CpuMatch(ptr) || !(b->flags&BRK_INPUT))return;
    b->winSide=Rand_Range(2);
    b->matches.m[b->match].flags|=TOUR_MATCH_DONE;
    b->flags&=~BRK_INPUT;
    Bracket_StartWin(b);
    Port_TagMenuTrace(88,1,b->match,b->winSide,0,0);
}
void TagTournament_DrawBracket(void *ptr) {
    Bracket *b=ptr;
    if(!TagTournament_Active() || !(b->flags&BRK_INPUT) || b->seq)return;
    Ps_PageSet();Ps_SetShift(0);Ps_Align(1);
    Ps_Rect(18,391,494,424,Ps_Rgba(15,15,32,118),Ps_Rgba(15,15,32,118));
    Ps_Text(256,397,TagTournament_CpuMatch(ptr)?"CPU MATCH: Cross - Watch    Square - Skip":"Cross - Play next match",0.82f,460,3);
    Font_FlushAll();
}
