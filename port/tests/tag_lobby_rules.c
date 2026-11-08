/* gcc -std=c99 -Wall -Wextra -Werror -Iinclude
 * src/battle/tag_lobby_rules.c port/tests/tag_lobby_rules.c -o lobby-rules */
#include "battle/tag_lobby_rules.h"
#include "battle/tag_options.h"
#include "../src/gs/tag_net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static TlLoadout goku={0,0,{0}},vegeta={24,1,{12,23}};
static TlRoom full_room(void) {
    TlRoom r;int i;TlRoom_Init(&r,&goku);
    for(i=1;i<10;i++)assert(TlRoom_Join(&r,&goku)==i);
    assert(TlRoom_Join(&r,&goku)==-1);return r;
}
static TlDraft team_draft(TlRoom *r) {
    TlDraft d=r->draft;int i;d.count=10;d.teams=2;d.playerLimit=10;
    for(i=0;i<10;i++){d.owner[i]=i;d.team[i]=i/5;d.cpu[i]=-1;d.fighters[i]=goku;}
    return d;
}
static void mode_cycle(void) {
    TlRoom r=full_room();TlDraft d=team_draft(&r);int mode,i,round;
    assert(TlRoom_Edit(&r,1,r.revision,&d)==TL_DENIED);
    assert(TlRoom_Edit(&r,0,r.revision-1,&d)==TL_STALE);
    assert(TlRoom_Kick(&r,1,2)==TL_DENIED);
    assert(TlRoom_Kick(&r,0,0)==TL_DENIED);
    for(mode=TL_TEAM;mode<=TL_TOURNAMENT;mode++) {
        d=team_draft(&r);d.mode=mode;
        if(mode==TL_GAUNTLET){d.count=5;d.teams=1;d.playerLimit=5;}
        if(mode==TL_FFA){d.teams=10;for(i=0;i<10;i++)d.team[i]=i;}
        assert(TlRoom_Edit(&r,0,r.revision,&d)==TL_OK);
        round=r.round;
        assert(TlRoom_Start(&r,1,r.revision)==TL_DENIED);
        assert(TlRoom_Start(&r,0,r.revision)==TL_NOT_READY);
        for(i=0;i<d.count;i++)assert(TlRoom_Ready(&r,i,r.revision,1)==TL_OK);
        assert(TlRoom_Start(&r,0,r.revision)==TL_OK && r.round==round+1);
        assert(TlRoom_Loaded(&r,0,round)==TL_STALE);
        assert(TlRoom_Edit(&r,0,r.revision,&d)==TL_BUSY);
        for(i=0;i<d.count-1;i++) {
            assert(TlRoom_Loaded(&r,i,r.round)==TL_OK);assert(r.phase==TL_LOADING);
        }
        assert(TlRoom_Loaded(&r,d.count-1,r.round)==TL_OK && r.phase==TL_MATCH);
        assert(!(TlRoom_PracticeMask(&r)&1));
        assert(TlRoom_Return(&r,1,r.round)==TL_DENIED);
        assert(TlRoom_Return(&r,0,r.round)==TL_OK && r.phase==TL_WAITING);
        assert(TlRoom_PracticeMask(&r)==0x3fe);
        for(i=0;i<10;i++)assert(r.members[i].present && !r.members[i].ready);
    }
    d=team_draft(&r);d.owner[2]=1;assert(!TlDraft_Valid(&r,&d));
    d=team_draft(&r);d.mode=TL_GAUNTLET;d.teams=1;assert(!TlDraft_Valid(&r,&d));
    d=team_draft(&r);d.mode=TL_TOURNAMENT;d.teams=4;
    for(i=0;i<10;i++)d.team[i]=i%4;
    assert(TlDraft_Valid(&r,&d));
    d.team[0]=4;assert(!TlDraft_Valid(&r,&d));
}
static void membership(void) {
    TlRoom r=full_room();TlDraft d=team_draft(&r);int generation=r.members[3].generation;
    assert(TlRoom_SetFighter(&r,3,generation,&vegeta)==TL_OK);
    assert(!memcmp(&r.members[3].loadout,&vegeta,sizeof(vegeta)));
    assert(TlRoom_Edit(&r,0,r.revision,&d)==TL_OK);
    assert(TlRoom_Leave(&r,3)==TL_OK);assert(r.members[4].present && r.draft.owner[3]==-1);
    assert(r.phase==TL_READY_CHECK && !TlRoom_PracticeMask(&r));
    assert(TlRoom_Join(&r,&goku)==3 && r.members[3].generation>generation);
    assert(r.phase==TL_READY_CHECK && !TlRoom_PracticeMask(&r));
    assert(TlRoom_SetFighter(&r,3,generation,&vegeta)==TL_STALE);
    assert(TlRoom_Kick(&r,0,2)==TL_OK);
    assert(r.phase==TL_READY_CHECK && !TlRoom_PracticeMask(&r));
    assert(TlRoom_CancelSetup(&r,1,r.revision)==TL_DENIED);
    assert(TlRoom_CancelSetup(&r,0,r.revision-1)==TL_STALE);
    assert(TlRoom_CancelSetup(&r,0,r.revision)==TL_OK && r.phase==TL_WAITING);
    assert(TlRoom_PracticeMask(&r)==(0x3fe&~(1<<2)));
    assert(TlRoom_Leave(&r,0)==TL_OK && r.phase==TL_CLOSED);
    assert(TlRoom_Join(&r,&goku)==-1);
}
static void first_come_slots(void) {
    TlRoom r=full_room();TlDraft d=team_draft(&r);int i,revision;
    d.playerLimit=3;
    for(i=1;i<10;i++)d.owner[i]=-1;
    assert(TlRoom_Edit(&r,0,r.revision,&d)==TL_OK);
    assert(TlRoom_PracticeMask(&r)==0);
    assert(TlRoom_Claim(&r,8,r.revision,1,0)==TL_OK);
    revision=r.revision;
    assert(TlRoom_Claim(&r,9,revision,1,1)==TL_OK);
    assert(TlRoom_Claim(&r,2,revision,1,0)==TL_STALE);
    assert(TlRoom_Claim(&r,2,r.revision,1,0)==TL_FULL);
    assert(TlRoom_Claim(&r,8,r.revision,1,1)==TL_OK);
    assert(TlRoom_MatchMask(&r)==((1<<0)|(1<<8)|(1<<9)));
    assert(TlRoom_Claim(&r,8,r.revision,0,0)==TL_OK);
    assert(TlRoom_Claim(&r,2,r.revision,1,0)==TL_OK);
    assert(TlRoom_MatchMask(&r)==((1<<0)|(1<<2)|(1<<9)));
    assert(r.members[8].present);
}
static void practice(void) {
    TlRoom r=full_room();TlPractice p,saved;unsigned mask;int i,n;
    TlPractice_Init(&p,&r,50000);
    assert(TlPractice_MemberActor(&p,0)==-1);
    for(i=1;i<10;i++)assert(TlPractice_MemberActor(&p,i)==i-1);
    for(i=9;i<TL_ACTORS;i++)assert(p.actors[i].enabled && !TlPractice_CpuActive(&p,i));
    assert(!TlPractice_CanHit(&p,0,1));
    assert(TlPractice_Challenge(&p,0,1,1)==TL_OK);
    assert(!TlPractice_CanHit(&p,0,1));
    assert(TlPractice_Challenge(&p,1,0,0)==TL_BUSY);
    for(i=0;i<TL_REQUEST_TICKS;i++)TlPractice_Tick(&p);
    assert(TlPractice_Challenge(&p,1,0,1)==TL_OK && !TlPractice_CanHit(&p,0,1));
    assert(TlPractice_Challenge(&p,0,1,1)==TL_OK && TlPractice_CanHit(&p,0,1));
    assert(!TlPractice_CanHit(&p,2,0) && !TlPractice_Contact(&p,2,1));
    assert(TlPractice_Edit(&p,0,1)==TL_BUSY);
    saved=p;p.actors[1].hp=0;assert(!TlPractice_Tick(&p));
    assert(p.wins[1]==1 && p.losses[2]==1 && !TlPractice_CanHit(&p,0,1));
    for(i=1;i<TL_RECOVER_TICKS;i++)assert(!TlPractice_Tick(&p));
    mask=TlPractice_Tick(&p);assert(mask==3 && p.actors[1].hp==50000);
    assert(p.wins[1]==1 && p.losses[2]==1);
    /* Restoring an earlier simulation snapshot restores the score too. */
    p=saved;p.actors[0].hp=p.actors[1].hp=0;TlPractice_Tick(&p);
    assert(!p.wins[1] && !p.losses[2] && p.draws[1]==1 && p.draws[2]==1);
    TlPractice_Init(&p,&r,50000);
    for(n=9;n<TL_ACTORS;n++) {
        assert(!TlPractice_CanHit(&p,n,0));
        assert(TlPractice_Contact(&p,0,n));
        assert(TlPractice_CpuActive(&p,n)==(n!=9));
        assert(TlPractice_CanHit(&p,n,0)==(n!=9));
        assert(!TlPractice_Contact(&p,1,n));
        p.actors[n].hp=0;TlPractice_Tick(&p);
        for(i=0;i<TL_RECOVER_TICKS;i++)TlPractice_Tick(&p);
        assert(!p.wins[1] && !p.losses[1]);
        assert(!TlPractice_CpuActive(&p,n) && p.actors[n].hp==50000);
    }
    assert(TlPractice_Edit(&p,0,1)==TL_OK);
    assert(!TlPractice_CanHit(&p,0,9));
    assert(TlPractice_Edit(&p,0,0)==TL_OK);
    assert(TlPractice_Challenge(&p,0,1,1)==TL_OK);
    assert(TlPractice_Challenge(&p,1,0,1)==TL_OK);
    TlRoom_Leave(&r,1);TlPractice_Refresh(&p,&r,50000);
    assert(!p.actors[0].enabled && p.actors[1].opponent==-1);
    assert(!p.wins[2] && !p.losses[1]);
}
static void tournament(void) {
    TlRoom r=full_room();TlDraft d=team_draft(&r);TlTournament t;int i;
    d.count=8;
    for(i=0;i<8;i++){d.team[i]=i/2;d.owner[i]=i;d.cpu[i]=i%5;}
    assert(TlTournament_Build(&t,&d)==TL_OK && t.entered==4);
    for(i=0;i<4;i++)assert(t.count[i]==2 && t.owner[i][0]==i*2 && t.owner[i][1]==i*2+1);
    assert(!TlTournament_CpuMatch(&t));
    assert(TlTournament_Advance(&t,1)==TL_OK && t.winner[0]==1 && t.left[4]==1);
    assert(TlTournament_Advance(&t,0)==TL_OK && t.right[4]==2);
    assert(TlTournament_CpuMatch(&t));
    assert(TlTournament_Advance(&t,0)==TL_OK && t.left[5]==4);
    assert(TlTournament_Advance(&t,1)==TL_OK && t.right[5]==7);
    assert(TlTournament_Advance(&t,0)==TL_OK && t.left[6]==1);
    assert(TlTournament_Advance(&t,1)==TL_OK && t.right[6]==7);
    assert(TlTournament_Advance(&t,0)==TL_OK && t.active==2 && t.winner[6]==1);
    assert(TlTournament_Advance(&t,0)==TL_INVALID);
    for(i=0;i<8;i++)d.team[i]=0;
    assert(TlTournament_Build(&t,&d)==TL_INVALID);
}
int main(void) {
    TagNetConfig c={0};
    c.chara[2]=31;c.costume[2]=3;c.owner[2]=7;c.cpuLevel[2]=4;c.items[2][6]=120;c.lobby.draft.team[2]=3;
    TagNet_CopyFighter(&c,1,2);
    assert(c.chara[1]==31 && c.costume[1]==3 && c.owner[1]==7 && c.cpuLevel[1]==4 && c.items[1][6]==120 && c.lobby.draft.team[1]==3);
    mode_cycle();membership();first_come_slots();practice();tournament();
    assert(!strcmp(tagOptionGroups[4],"HUD"));
    assert(tagOptionInfo[TAG_OPT_NATIVE_HUD].initial==1);
    assert(TagOption_IsPresentation(TAG_OPT_NATIVE_HUD));
    assert(TagOption_IsPresentation(TAG_OPT_FUSION_METER));
    assert(!TagOption_IsPresentation(TAG_OPT_FUSION_SWAP));
    assert(!TagOption_IsPresentation(TAG_OPT_CINEMATIC_PAUSE));
    assert(!TagOption_IsPresentation(TAG_OPT_TRAIN_HEALTH));
    puts("PASS: persistent membership, five mode cycles, host-only setup, ready/load barriers, stale requests, PvP consent and scores, six dummy types, KO recovery, cosmetic settings");
    return 0;
}
