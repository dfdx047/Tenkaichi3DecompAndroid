#include "battle/tag_clash.h"
/* Native Tag Team experiment. This owns real engine fighter slots; no emulator
 * memory access, context swapping, prepared savestates or injected MIPS code. */
#ifdef PORT
#include "common.h"
#include "battle/tag_challenge.h"
#include "battle/tag_team.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_coll.h"
#include "battle/battle_setup.h"
#include "battle/btl_cam.h"
#include "battle/btl_input.h"
#include "battle/btl_char_move.h"
#include "battle/btl_seq.h"
#include "battle/tag_options.h"
#include "battle/tag_session.h"
#include "battle/tag_lobby.h"
#include "battle/tag_fusion.h"
#include "battle/tag_assist.h"
#include "battle/tag_balance.h"
#include "battle/tag_cpu.h"
#include "battle/tag_position.h"
#include "battle/tag_view_layout.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "../../port/src/gs/tag_net.h"

extern void *memset(void *, s32, u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *);
extern s32 BtlChar_IsFrozen(BtlMgrChr *);
extern s32 BtlChar_TestFlag(BtlMgrChr *, s32);
extern s32 BtlCharApi_IsInRushSequence(s32);
extern s32 BtlCharApi_IsChanging(s32);

static s32 enabled;
static s32 count;
static BattleSide sides[TAG_TEAM_CAPACITY];
static s32 teams[TAG_TEAM_CAPACITY];
static s32 nativeSides[TAG_TEAM_CAPACITY];
static s32 targets[TAG_TEAM_CAPACITY];
static s32 traceFrame;
static s32 wantedPlayers, wantedViews = 1, cooperate;
static s32 viewActors[TAG_TEAM_CAPACITY], players[TAG_TEAM_CAPACITY];
static s32 controlActors[TAG_TEAM_CAPACITY], initialActors[TAG_TEAM_CAPACITY];
static s32 spectatorChosen[TAG_TEAM_CAPACITY];
static s32 unlocked[TAG_TEAM_CAPACITY], targetHeld[TAG_TEAM_CAPACITY];
static s32 stickLatch[TAG_TEAM_CAPACITY],stickNeutral[TAG_TEAM_CAPACITY];
static s32 stickPending[TAG_TEAM_CAPACITY],stickR3[TAG_TEAM_CAPACITY];
static s32 pressAttacker[TAG_TEAM_CAPACITY],tapTarget[TAG_TEAM_CAPACITY],tapLife[TAG_TEAM_CAPACITY];
static s32 viewCount = 1;
static s32 assignment[TAG_TEAM_CAPACITY], assigned, winningTeam=-1;
static s32 grabPartner[TAG_TEAM_CAPACITY],grabOwner[TAG_TEAM_CAPACITY];
static u32 cinematicPaused;
static s32 grab_other(s32 actor),rush_other(s32 actor);
extern s32 BtlAct_GetCurrent(BtlMgrChr *),BtlAct_GetCurrentClass(BtlMgrChr *);
static s32 grabbing(s32 actor) {
    s32 act=BtlAct_GetCurrent(BtlChar_Get(actor));
    return act>=0xB7 && act<=0xBC;
}
void TagTeam_GrabContact(s32 attacker,s32 victim) {
    s32 n;
    if(!TagTeam_Active() || (u32)attacker>=(u32)count || (u32)victim>=(u32)count || attacker==victim) return;
    grabPartner[attacker]=victim; grabPartner[victim]=attacker;
    TagLobby_Contact(attacker,victim);
    grabOwner[attacker]=grabOwner[victim]=attacker;
    targets[attacker]=victim; targets[victim]=attacker;
    /* A successful catch ends locomotion. Backstep velocity must not carry
       the pair away while their root-motion attachment is starting. */
    for(n=0;n<2;n++) {
        BtlMovePose *p=(BtlMovePose *)&BtlChar_Get(n?victim:attacker)->pose;
        p->speed=p->fallSpeed=0;
        memset(&p->unk70,0,sizeof(Vec4));
    }
    {extern void TagCamera_TestContact(s32,s32,s32);TagCamera_TestContact(attacker,victim,0);}
}
s32 TagTeam_CinematicPaused(s32 actor) {
    return TagTeam_Active() && (u32)actor<(u32)count &&
        (TagFusion_Hidden(actor) || TagLobby_Hidden(actor) || TagLobby_Editing(actor) || TagChallenge_ModelHidden(actor) || BtlSeq_WaveIntroPaused(actor) || (cinematicPaused&(1u<<actor))!=0);
}
static void cinematic_pause(void) {
    s32 i,mode=Port_TagOption(TAG_OPT_CINEMATIC_PAUSE),owner;
    u32 participants=0,targetsToPause=0;
    cinematicPaused=0;
    if(!mode || !BtlSeq_IsFighting()) return;
    /* Use the same owner as the view compositor. Authored transform cameras,
       shared grabs and shared clashes must obey the same pause policy. */
    owner=BtlCam_GetPriorityView();
    if(owner<0)return;
    participants=1u<<owner;
    {
        s32 other=TagTeam_Target(owner);
        if((u32)other<(u32)count) {
            targetsToPause|=1u<<other;
            if(!BtlCharApi_IsChanging(BattleSide_GetObjId(owner)))participants|=1u<<other;
        }
    }
    /* Participants in the selected clash include its beam assistants. */
    for(i=0;i<count;i++) if(TagAssist_Active(i)) {
        s32 other=TagTeam_Target(i);
        if((u32)other<(u32)count && (participants&(1u<<other)))participants|=1u<<i;
    }
    cinematicPaused=(mode==2?((1u<<count)-1):targetsToPause)&~participants;
}
/* Reject both collision discovery and already-queued contacts. The latter
   matters when a lower-index fighter establishes a grab earlier this frame. */
s32 TagTeam_CanContact(s32 attacker,s32 victim) {
    s32 a,b;
    if(!TagTeam_Active())return 1;
    if((u32)attacker>=(u32)count || (u32)victim>=(u32)count)return 0;
    if(!TagLobby_CanContact(attacker,victim))return 0;
    if(TagChallenge_Hidden(attacker)||TagChallenge_Hidden(victim))return 0;
    if(BtlChar_TestFlag(BtlChar_Get(attacker),7) || BtlChar_TestFlag(BtlChar_Get(victim),7))return 0;
    if(TagTeam_CinematicPaused(attacker)||TagTeam_CinematicPaused(victim))return 0;
    a=grab_other(attacker);b=grab_other(victim);
    if(a<0)a=rush_other(attacker);
    if(b<0)b=rush_other(victim);
    return (a<0||a==victim) && (b<0||b==attacker);
}
static s32 grab_other(s32 actor) {
    s32 other=grabPartner[actor];
    if((u32)other<(u32)count && grabPartner[other]==actor &&
       (grabbing(actor)||grabbing(other)||
        ((BtlAct_GetCurrent(BtlChar_Get(grabOwner[actor]))>=0xB4 &&
          BtlAct_GetCurrent(BtlChar_Get(grabOwner[actor]))<=0xB6) &&
          BtlChar_TestFlag(BtlChar_Get(grabOwner[actor]),0x5B)))) return other;
    return -1;
}
static s32 rush_other(s32 actor) {
    BtlMgrChr *chr=BtlChar_Get(actor);
    BtlCollThrow *thr=&((BtlCollChr *)chr)->thr;
    s32 other=actor==thr->atkSide?thr->defSide:thr->atkSide;
    if((actor!=thr->atkSide && actor!=thr->defSide) || (u32)other>=(u32)count ||
        !TagTeam_IsEnemy(actor,other)) return -1;
    {
        BtlCollThrow *b=&((BtlCollChr *)BtlChar_Get(other))->thr;
        if(b->atkSide!=thr->atkSide || b->defSide!=thr->defSide) return -1;
    }
    /* The contact pass fills both records before action dispatch. Protect that
       pending catch too, otherwise another effect in this very pass can knock
       the attacker away while the victim still enters the rush animation.
       Require both halves of the handshake so an old record or guard flag
       cannot leave a fighter protected after the move. */
    if(!BtlCharApi_IsInRushSequence(chr->objId) &&
       !BtlCharApi_IsInRushSequence(BattleSide_GetObjId(other))) {
        BtlMgrChr *atk=BtlChar_Get(thr->atkSide);
        s32 reaction=((BtlCollChr *)BtlChar_Get(thr->defSide))->react.id;
        if(!((reaction==BTL_REACT_CAUGHT && BtlChar_TestFlag(atk,0x72)) ||
             ((reaction==0x1D || reaction==0x1E) && BtlChar_TestFlag(atk,0x73)))) return -1;
    }
    return other;
}

void TagTeam_Configure(s32 humans, s32 views, s32 coop) {
    s32 maximum=TagNet_Active()||TagLobby_Practice()?TN_PLAYERS:4;
    wantedPlayers = humans < 0 ? 0 : humans > maximum ? maximum : humans;
    wantedViews = views < 1 ? (wantedPlayers > 0 ? wantedPlayers : 1) : views > maximum ? maximum : views;
    cooperate = coop != 0;
    assigned=0;
}
void TagTeam_SetAssignment(s32 actor,s32 player) {
    if((u32)actor<TAG_TEAM_CAPACITY) { assignment[actor]=player; assigned=1; }
}
s32 TagTeam_ViewCount(void) { return TagTeam_Active() ? (TagNet_Active()?1:viewCount) : 2; }
s32 TagTeam_CameraCount(void) {return TagTeam_Active()?viewCount:2;}
s32 TagTeam_ViewActor(s32 view) { return TagTeam_Active() ? TagFusion_Owner(viewActors[view]) : view; }
s32 TagTeam_Player(s32 actor) { return TagTeam_Active() ? players[actor] : actor; }
s32 TagTeam_InputPlayer(s32 actor) {
    return TagFusion_Hidden(actor)?-1:TagFusion_Controller(actor);
}
s32 TagTeam_ControlActor(s32 player) { return TagTeam_Active() ? controlActors[player] : player; }
static void view_layout(s32 rects[4][4],s32 visible[4]) {
    s32 i,group[4]={-1,-1,-1,-1};
    for(i=0;i<viewCount;i++) {
        s32 actor=controlActors[i];
        /* Spectators choosing the same fighter are not a fused pair. */
        if((u32)actor<(u32)count && players[actor]>=0)
            group[i]=TagFusion_SharedBody(actor);
    }
    TagView_Layout(viewCount,Port_TagOption(TAG_OPT_THREE_WIDE),group,rects,visible);
}
s32 TagTeam_ViewVisible(s32 view) {
    s32 r[4][4],visible[4];
    if(!TagTeam_Active()) return 1;
    if(TagNet_Active())return view==0;
    view_layout(r,visible);
    return (u32)view<(u32)viewCount && visible[view];
}
s32 TagTeam_VisibleViews(void) {
    s32 r[4][4],visible[4],i,n=0;
    if(!TagTeam_Active()) return 2;
    if(TagNet_Active())return 1;
    view_layout(r,visible);
    for(i=0;i<viewCount;i++) n+=visible[i];
    return n;
}
void TagTeam_ViewRect(s32 view, s32 *r) {
    s32 rects[4][4],visible[4],i;
    if(TagNet_Active()){r[0]=r[1]=0;r[2]=512;r[3]=448;return;}
    view_layout(rects,visible);
    for(i=0;i<4;i++) r[i]=rects[view][i];
}
s32 TagTeam_HudMarker(s32 view,s32 shared,s32 bottom) {
    s32 r[4]={0,0,512,448};
    if(!shared) TagTeam_ViewRect(view,r);
    return TagView_HudMarker(r,bottom);
}

void TagTeam_Enable(s32 on) { enabled = on != 0; }
s32 TagTeam_Enabled(void) { return enabled; }
s32 TagTeam_Active(void) { return count >= 2; }
s32 TagTeam_Count(void) { return count >= 2 ? count : 2; }

BattleSide *TagTeam_Side(s32 fighter) {
    if (TagTeam_Active() && (u32)fighter < (u32)count) return &sides[fighter];
    return &Battle_GetSetup()->sides[fighter];
}

s32 TagTeam_Team(s32 fighter) {
    if (TagTeam_Active() && (u32)fighter < (u32)count) return teams[fighter];
    return fighter & 1;
}
s32 TagTeam_NativeSide(s32 fighter) {
    return TagTeam_Active() && (u32)fighter<(u32)count ? nativeSides[fighter] : fighter&1;
}

s32 TagTeam_IsEnemy(s32 a, s32 b) {
    if(TagLobby_Practice())return TagLobby_Enemy(a,b);
    return a != b && TagTeam_Team(a) != TagTeam_Team(b);
}
s32 TagTeam_RingOutEnabled(void) { return !TagLobby_Practice() && TagSession_Mode()!=TAG_MODE_TRAINING && Port_TagOption(TAG_OPT_RING_OUT); }
s32 TagTeam_TransformFrequency(void) { return Port_TagOption(TAG_OPT_CPU_TRANSFORM); }
s32 TagTeam_FusionsEnabled(void) { return Port_TagOption(TAG_OPT_FUSIONS); }
s32 TagTeam_ShareClashCamera(s32 actor) {
    s32 state=BtlClash_State(actor);
    BtlMgrChr *chr=BtlChar_Get(actor);
    if(BtlClash_Opponent(actor)>=0) {
        if(state>=1 && state<=5) return Port_TagOption(TAG_OPT_SHARED_CLASH);
        if(state>=6 && state<=8) return Port_TagOption(TAG_OPT_SHARED_MELEE);
        return Port_TagOption(TAG_OPT_SHARED_VANISH);
    }
    if(grab_other(actor)>=0) return Port_TagOption(TAG_OPT_SHARED_GRAB);
    if(BtlCharApi_IsChanging(chr->objId)) return Port_TagOption(TAG_OPT_SHARED_TRANSFORM);
    if(rush_other(actor)>=0) {
        s32 attacker=((BtlCollChr *)chr)->thr.atkSide;
        return Port_TagOption(BtlAct_GetCurrentClass(BtlChar_Get(attacker))==4 ?
            TAG_OPT_SHARED_ULTIMATE:TAG_OPT_SHARED_RUSH);
    }
    return Port_TagOption(BtlAct_GetCurrentClass(chr)==4 ? TAG_OPT_SHARED_ULTIMATE:TAG_OPT_SHARED_SPECIAL);
}
s32 TagTeam_CameraActor(s32 actor) {
    extern s32 BtlCharApi_HasCamPriority(s32);
    extern s32 BtlCharApi_HasAuthoredCamera(s32);
    s32 other;
    if(TagAssist_Ally(actor)>=0) actor=TagAssist_Ally(actor);
    other=BtlClash_Opponent(actor);
    if(other<0) other=grab_other(actor);
    if(other<0) other=rush_other(actor);
    if(other>=0) {
        /* Prefer the authored attacker's camera, including the victim's
           screen. Object IDs can differ from logical fighter slots. */
        s32 owner=grab_other(actor)>=0?grabOwner[actor]:
            rush_other(actor)>=0?((BtlCollChr *)BtlChar_Get(actor))->thr.atkSide:actor;
        if(BtlCharApi_HasCamPriority(BattleSide_GetObjId(owner)) ||
           BtlCharApi_HasAuthoredCamera(BattleSide_GetObjId(owner))) return owner;
        if(!BtlCharApi_HasCamPriority(BattleSide_GetObjId(actor)) &&
           BtlCharApi_HasCamPriority(BattleSide_GetObjId(other))) return other;
    }
    return actor;
}

s32 TagTeam_Target(s32 fighter) {
    if (TagTeam_Active() && (u32)fighter < (u32)count) {
        s32 clashOpponent = BtlClash_Opponent(fighter);
        if(TagAssist_Ally(fighter)>=0) clashOpponent=BtlClash_Opponent(TagAssist_Ally(fighter));
        if (clashOpponent >= 0) return clashOpponent;
        /* The loader's throw record already owns the actual attacker/victim.
         * A victim can be attacked by someone other than their selected target;
         * its animation and completion flags must follow that participant. */
        if (gBtlChars != NULL) {
            s32 other=grab_other(fighter);
            if(other<0) other=rush_other(fighter);
            if(other>=0) return other;
        }
        return targets[fighter];
    }
    return fighter == 0 ? 1 : 0;
}

/* Called after the native menus finish building the ordinary team roster.
 * Keep that setup intact for menu return/rematch. Every active actor gets its
 * own model/animation buffers and a one-member runtime side. */
void TagTeam_Begin(void) {
    BattleSetup *setup = Battle_GetSetup();
    s32 team, member, slot = 0;
    count = 0;
    if (!enabled || BattleReplay_IsActive() || (setup->rule.mode != BTL_MODE_VERSUS && setup->rule.mode != BTL_MODE_DEMO &&
        !(setup->rule.mode==4 && TagSession_Mode()==TAG_MODE_TOURNAMENT))) return;
    if (setup->sides[0].memberCount + setup->sides[1].memberCount < 2) return;
    if (setup->sides[0].memberCount + setup->sides[1].memberCount > TAG_TEAM_CAPACITY) {
        extern void Port_TagTeamFatal(const char *message);
        Port_TagTeamFatal("this experimental build supports at most five fighters per team");
    }
    memset(sides, 0, sizeof(sides));
    for (member = 0; member < BTL_MEMBER_MAX; member++) {
        for (team = 0; team < 2; team++) {
            BattleSide *src = &setup->sides[team];
            BattleSide *dst;
            s32 index;
            if (member >= src->memberCount) continue;
            dst = &sides[slot];
            *dst = *src;
            index = (src->lead + member) % src->memberCount;
            memset(dst->members, 0, sizeof(dst->members));
            dst->members[0] = src->members[index];
            dst->members[0].data = NULL;
            dst->members[0].buf[0] = dst->members[0].buf[1] = NULL;
            dst->memberCount = 1;
            dst->lead = 0;
            dst->startForm.chara = dst->members[0].chara;
            dst->startForm.costume = dst->members[0].costume;
            dst->startForm.variant = dst->members[0].variant;
            dst->form = dst->startForm;
            dst->objId = dst->modelSlot = -1;
            if (member > 0) dst->control = BTL_CONTROL_CPU;
            /* CPU input is generated per fighter; the pad preference still
             * comes from its team's original controller, not an invalid port. */
            teams[slot] = TagSession_Mode()==TAG_MODE_FREE_FOR_ALL ? slot : team;
            nativeSides[slot]=team;
            slot++;
        }
    }
    if(TagLobby_Practice()) {
        slot=TagLobby_BuildSides(sides,teams,nativeSides,assignment);assigned=1;
        wantedPlayers=TN_PLAYERS;wantedViews=1;
    }
    count = slot;
    viewCount = TagNet_Active()?wantedViews:wantedViews > count ? count : wantedViews;
    for (slot = 0; slot < count; slot++) {
        s32 actor = slot;
        if (cooperate) {
            s32 t,n,k=0;
            for(t=0;t<2;t++) for(n=0;n<count;n++) if(teams[n]==t) { if(k==slot) actor=n; k++; }
        }
        viewActors[slot] = actor;
        initialActors[slot] = actor;
        players[actor] = slot < wantedPlayers ? slot : -1;
        sides[actor].control = slot < wantedPlayers ? BTL_CONTROL_PAD : BTL_CONTROL_CPU;
        sides[actor].pad = slot < wantedPlayers ? slot : 0;
    }
    { extern s32 Port_TagFusionTest(void);
      extern void TagFusion_TestAssign(s32);
      if(Port_TagFusionTest()==1 || Port_TagFusionTest()==3) TagFusion_TestAssign(wantedPlayers);
    }
    if(assigned && !TagNet_Active() && !TagLobby_Practice()) {
        s32 n=wantedPlayers;
        for(slot=0;slot<count;slot++) {
            s32 player=assignment[slot];
            if(player>=0 && player<wantedPlayers) initialActors[player]=slot;
            else initialActors[n++]=slot;
        }
    }
    TagTeam_Reset();
}

void TagTeam_End(void) { extern void Port_TagHudClear(void); count = 0; Port_TagHudClear(); }

/* Reproducible roster launch using the real loader and CPU controllers. */
void TagTeam_SetupDemo(s32 human, s32 team0, s32 team1) {
    extern s32 Port_TagAudioTest(void);
    extern s32 Port_TagFusionTest(void);
    extern s32 Port_TagMultiBeamTest(void);
    BattleSetup *setup = Battle_GetSetup();
    s32 team;
    s32 member;
    s32 sizes[2];
    sizes[0] = team0; sizes[1] = team1;
    for (team = 0; team < 2; team++) {
        BattleSide *side = &setup->sides[team];
        for (member = 1; member < sizes[team]; member++) {
            side->members[member] = side->members[0];
            side->members[member].chara = team ? 7 + member - 1 : 10 + member - 1;
            /* Goku GT's SSJ forms have silent clip-zero samples. Use other
             * distinct banks for the test that checks audible sample output. */
            if (Port_TagAudioTest() && team && member>=2)
                side->members[member].chara=12+member;
            side->members[member].costume = 0;
            side->members[member].variant = 0;
        }
        side->memberCount = sizes[team];
    }
    if(Port_TagMultiBeamTest()) for(team=0;team<2;team++)
        for(member=0;member<sizes[team];member++) {
            setup->sides[team].members[member].chara=10;
            setup->sides[team].members[member].costume=0;
        }
    if(Port_TagFusionTest()) {
        extern s32 Port_TagFusionPair(s32),Port_TagFusionDouble(void);
        for(team=0;team<(Port_TagFusionDouble()?2:1);team++) {
            setup->sides[team].members[0].chara=Port_TagFusionPair(0);
            setup->sides[team].members[1].chara=Port_TagFusionPair(1);
            setup->sides[team].members[0].costume=setup->sides[team].members[1].costume=Port_TagFusionPair(2);
        }
    }
    {
        extern s32 Port_TagCinemaTest(void);
        if(Port_TagCinemaTest()) {
            s32 test=Port_TagCinemaTest();
            extern s32 Port_TagSpecialCharacter(void);
            if(test==13)setup->rule.unk10=1; /* Actual ultimate destroys the stage. */
            setup->sides[0].members[0].chara=test==3?6:(test==4||test==6||test==8||test==9)?10:0;
            setup->sides[0].members[1].chara=test==9?10:3;
            if(Port_TagSpecialCharacter()>=0)setup->sides[0].members[0].chara=Port_TagSpecialCharacter();
            setup->sides[0].members[0].costume=setup->sides[0].members[1].costume=0;
        }
    }
    {
        extern s32 Port_TagSpawnTest(void);
        if(Port_TagSpawnTest()) {
            TagSession_Setup(TAG_MODE_FREE_FOR_ALL,human?wantedPlayers:0);
            TagSession_Commit();
        }
    }
    if (human) {
        setup->rule.mode = BTL_MODE_VERSUS;
        setup->rule.timeLimit = 3; /* 180 seconds for the playable prototype. */
        setup->sides[0].control = BTL_CONTROL_PAD;
        setup->sides[0].pad = 0;
    }
    {
        extern s32 Port_TagShowcase(void);
        if(Port_TagShowcase()) {
            extern s32 Port_TagShowcaseLead(s32,s32);
            static const s32 cast[]={16,32,22,4,43};
            for(team=0;team<2;team++)for(member=0;member<sizes[team];member++) {
                setup->sides[team].members[member].chara=team?108:cast[member];
                if(member==0) setup->sides[team].members[member].chara=Port_TagShowcaseLead(team,setup->sides[team].members[member].chara);
                setup->sides[team].members[member].costume=0;
            }
        }
    }
    { extern s32 Port_TagChallengeTest(void);
      if(Port_TagChallengeTest()) {
          extern s32 Port_TagChallengeSkipTest(void);
          extern s32 Port_TagModeRushTest(void),Port_TagModeRushViews(void);
          TagSession_Setup(TAG_MODE_GAUNTLET,Port_TagModeRushTest()?Port_TagModeRushViews():Port_TagChallengeSkipTest()?1:0);TagSession_Commit();
          setup->rule.mode=BTL_MODE_VERSUS;
          TagChallenge_Select(Port_TagChallengeTest()-1);TagChallenge_SetupBattle();TagSession_Apply();
      }
    }
    TagLobby_TestSetup();
    BattleSetup_Finish();
}

void TagTeam_Reset(void) {
    s32 i, j;
    traceFrame = 0;
    winningTeam=-1;
    cinematicPaused=0;
    { extern void TagRevive_Reset(void); TagRevive_Reset(); }
    TagFusion_Reset();
    TagAssist_Reset();
    TagBalance_Reset();
    TagCpu_Reset();
    TagPosition_Reset();
    TagSpawn_Reset();
    TagLobby_Reset();
    for(i=0;i<TAG_TEAM_CAPACITY;i++){spectatorChosen[i]=0;controlActors[i]=-1;viewActors[i]=0;}
    if(TagNet_Active() || TagLobby_Practice()) {
        /* Player seats can have holes after a lobby role change. Never turn
           an unclaimed fighter into a player just to fill that hole. */
        for(i=0;i<count;i++) {
            s32 player=assignment[i];
            players[i]=player;
            sides[i].control=player>=0?BTL_CONTROL_PAD:BTL_CONTROL_CPU;
            sides[i].pad=player>=0?player:0;
            if(player>=0)controlActors[player]=viewActors[player]=i;
        }
    }
    for (i = 0; i < count; i++) {
        s32 actor = initialActors[i];
        grabPartner[i]=grabOwner[i]=-1;
        if(!TagNet_Active() && !TagLobby_Practice()) {
            controlActors[i] = viewActors[i] = actor;
            players[actor] = i < wantedPlayers ? i : -1;
            sides[actor].control = i < wantedPlayers ? BTL_CONTROL_PAD : BTL_CONTROL_CPU;
            sides[actor].pad = i < wantedPlayers ? i : 0;
        }
        unlocked[i] = targetHeld[i] = 0;
        pressAttacker[i]=tapTarget[i]=-1;tapLife[i]=0;
        stickLatch[i]=stickNeutral[i]=stickPending[i]=stickR3[i]=0;
        targets[i] = i == 0 ? 1 : 0;
        for (j = 1; j < count; j++) {
            s32 candidate = (i + j) % count;
            if (TagTeam_IsEnemy(i, candidate)) { targets[i] = candidate; break; }
        }
    }
}

static s32 alive(s32 i) {
    return !TagFusion_Hidden(i) && !TagLobby_Hidden(i) && !TagChallenge_Hidden(i) && BtlMember_GetActiveGauge(BtlChar_Get(i))->health > 0 && !BtlChar_TestFlag(BtlChar_Get(i), 7);
}
s32 TagTeam_IsAlive(s32 actor) { return (u32)actor<(u32)TagTeam_Count() && alive(actor); }

s32 TagTeam_BodiesTouch(s32 a, s32 b) {
    extern f32 BtlCharApi_GetHeight(s32), BtlCharApi_GetRadius(s32);
    BtlMovePose *p=(BtlMovePose *)&BtlChar_Get(a)->pose, *q=(BtlMovePose *)&BtlChar_Get(b)->pose;
    f32 x=q->pos.x-p->pos.x, y=q->pos.y-p->pos.y, z=q->pos.z-p->pos.z;
    f32 radius=(BtlCharApi_GetRadius(a)+BtlCharApi_GetRadius(b))*1.05f;
    return TagTeam_IsEnemy(a,b) && TagTeam_CanContact(a,b) && alive(a) && alive(b) && x*x+z*z<=radius*radius &&
           y > -BtlCharApi_GetHeight(a)*0.5f && y < BtlCharApi_GetHeight(b)*0.5f;
}

static s32 busy(s32 i) {
    return BtlClash_Opponent(i)>=0 || BtlChar_IsFrozen(BtlChar_Get(i)) ||
           BtlCharApi_IsInRushSequence(BattleSide_GetObjId(i)) || BtlCharApi_IsChanging(BattleSide_GetObjId(i));
}

static s32 targetable(s32 i, s32 other) {
    if (i==other || TagFusion_Hidden(other) || TagChallenge_Hidden(other)) return 0;
    if (TagTeam_IsEnemy(i,other)) return alive(other);
    return TagSession_Mode()!=TAG_MODE_FREE_FOR_ALL && TagTeam_InputPlayer(i)>=0 && Port_TagOption(TAG_OPT_REVIVE) &&
        BtlMember_GetActiveGauge(BtlChar_Get(other))->health<=0 && !BtlChar_TestFlag(BtlChar_Get(other),7);
}

s32 TagTeam_LockedOff(s32 actor) {
    return TagTeam_Active() && (u32)actor<(u32)count &&
        (unlocked[actor] || TagChallenge_BetweenWaves() || BtlChar_TestFlag(BtlChar_Get(actor),7));
}

/* The combat engine always has a valid target pointer, including in free flight.
 * Only camera locking and the optional enemy panel are disabled. */
s32 TagTeam_HudTarget(s32 actor) {
    s32 i, enemies=0, mode=Port_TagOption(TAG_OPT_UNLOCKED_HUD);
    if (TagChallenge_Hidden(TagTeam_Target(actor))) return -1;
    if (!TagTeam_LockedOff(actor) || mode==0) return TagTeam_Target(actor);
    for (i=0;i<count;i++) if (TagTeam_IsEnemy(actor,i)&&alive(i)) enemies++;
    return mode==1 && enemies==1 ? TagTeam_Target(actor) : -1;
}

static void relock(s32 actor) {
    extern BtlCamView *BtlCam_GetNativeView(s32);
    extern void BtlCharApi_GetPos(s32, Vec4 *);
    s32 i, v=0, best=-1, rect[4];
    f32 score=1.0e30f;
    Mtx44 *m;
    for (i=0;i<viewCount;i++) if ((TagNet_Active()||TagTeam_ViewVisible(i))&&TagTeam_ViewActor(i)==actor) { v=i; break; }
    m=&BtlCam_GetNativeView(v)->view.world2screen;
    TagTeam_ViewRect(v,rect);
    for (i=0;i<count;i++) if (targetable(actor,i) && (unlocked[actor] || i!=targets[actor])) {
        Vec4 p, own;
        f32 w,x,y,d;
        BtlCharApi_GetPos(i,&p); BtlCharApi_GetPos(actor,&own);
        w=p.x*m->m[0][3]+p.y*m->m[1][3]+p.z*m->m[2][3]+m->m[3][3];
        if (w>0.3f) {
            x=((p.x*m->m[0][0]+p.y*m->m[1][0]+p.z*m->m[2][0]+m->m[3][0])/w-1792-rect[0]-rect[2]*0.5f)/rect[2];
            y=((p.x*m->m[0][1]+p.y*m->m[1][1]+p.z*m->m[2][1]+m->m[3][1])/w-1824-rect[1]-rect[3]*0.5f)/rect[3];
            d=x*x+y*y;
        } else {
            x=p.x-own.x; y=p.z-own.z;
            d=1.0e15f+x*x+y*y;
        }
        if (d<score) { best=i; score=d; }
    }
    if (best>=0) targets[actor]=best;
    unlocked[actor]=0;
}

void TagTeam_TargetInput(s32 actor, s32 held) {
    s32 paired=grab_other(actor)>=0 || rush_other(actor)>=0 || BtlClash_Opponent(actor)>=0 ||
        BtlCharApi_IsChanging(BattleSide_GetObjId(actor)) || TagAssist_Active(actor);
    if(paired) {targetHeld[actor]=held?-1:0;tapLife[actor]=0;return;}
    if(held) {
        if(targetHeld[actor]==0)pressAttacker[actor]=TagCpu_Attacker(actor);
        if(targetHeld[actor]>=0 && ++targetHeld[actor]>=15) {
            unlocked[actor]=1;
            targetHeld[actor]=-1;tapLife[actor]=0;
        }
    } else {
        if(targetHeld[actor]>0) {
            s32 attacker=pressAttacker[actor];
            if(attacker<0 || !targetable(actor,attacker))attacker=TagCpu_Attacker(actor);
            tapTarget[actor]=attacker<0?-2:attacker;
            tapLife[actor]=15;
        }
        targetHeld[actor]=0;
    }
    /* Short hit-stop may cover the entire click. Retain it briefly and apply
       at the next safe update, without disturbing an active paired move. */
    if(tapLife[actor]>0) {
        tapLife[actor]--;
        if(!busy(actor)) {
            s32 target=tapTarget[actor];
            if(target>=0 && targetable(actor,target)) {targets[actor]=target;unlocked[actor]=0;}
            else relock(actor);
            tapLife[actor]=0;
        }
    }
}

static void aim_target(s32 actor,s32 direction) {
    extern BtlCamView *BtlCam_GetNativeView(s32);
    extern void BtlCharApi_GetPos(s32,Vec4 *);
    s32 i,v=0,best=-1,rect[4];
    f32 score=1.0e30f,bx=0,by=0;
    Vec4 p;Mtx44 *m;
    for(i=0;i<viewCount;i++) if((TagNet_Active()||TagTeam_ViewVisible(i))&&TagTeam_ViewActor(i)==actor) {v=i;break;}
    m=&BtlCam_GetNativeView(v)->view.world2screen;TagTeam_ViewRect(v,rect);
    /* Project into this player's actual viewport. All four directions use
       screen space, including elevated targets and rotated cameras. */
    for(i=-1;i<count;i++) {
        s32 candidate=i<0?targets[actor]:i;
        f32 w,x,y,along,across,d;
        if(i>=0 && (i==targets[actor] || !targetable(actor,i)))continue;
        BtlCharApi_GetPos(candidate,&p);
        w=p.x*m->m[0][3]+p.y*m->m[1][3]+p.z*m->m[2][3]+m->m[3][3];
        if(w<=.3f)continue;
        x=((p.x*m->m[0][0]+p.y*m->m[1][0]+p.z*m->m[2][0]+m->m[3][0])/w-1792-rect[0]-rect[2]*.5f)/rect[2];
        y=((p.x*m->m[0][1]+p.y*m->m[1][1]+p.z*m->m[2][1]+m->m[3][1])/w-1824-rect[1]-rect[3]*.5f)/rect[3];
        if(i<0) {if(!unlocked[actor]){bx=x;by=y;}continue;}
        x-=bx;y-=by;
        along=direction==PAD_RSTICK_LEFT?-x:direction==PAD_RSTICK_RIGHT?x:direction==PAD_RSTICK_UP?-y:y;
        across=(direction==PAD_RSTICK_LEFT||direction==PAD_RSTICK_RIGHT)?y:x;
        if(along<=.001f)continue;
        d=along*along+across*across*4;
        if(d<score) {score=d;best=i;}
    }
    if(best>=0) {targets[actor]=best;unlocked[actor]=0;}
}

s32 TagTeam_TargetStickConsumed(s32 actor) {
    return TagTeam_Active() && (u32)actor<(u32)count && stickLatch[actor];
}
void TagTeam_TargetStick(s32 actor,u32 buttons) {
    s32 bits=buttons&0xF00000,mode=Port_TagOption(TAG_OPT_TARGET_STICK);
    s32 valid;
    /* R3 may be clicked with a slightly tilted stick when transforming. Never
       eat that click or switch its target, including on the following tick. */
    valid=mode && !busy(actor) && grab_other(actor)<0 && !TagAssist_Active(actor);
    if(buttons&PAD_R3) stickR3[actor]=3;
    else if(stickR3[actor]) stickR3[actor]--;
    if(!bits) {
        stickLatch[actor]=stickPending[actor]=0;
        if(stickNeutral[actor]<2) stickNeutral[actor]++;
        return;
    }
    if(stickPending[actor]) {
        if(valid && !stickR3[actor] && (mode==2 ? !unlocked[actor] : (buttons&PAD_L3)!=0)) {
            aim_target(actor,stickPending[actor]);
            if(buttons&PAD_L3) targetHeld[actor]=-1;
        }
        stickPending[actor]=0;
    }
    if(stickLatch[actor]) return;
    if(valid && stickNeutral[actor]>=2 && !stickR3[actor] &&
       !(buttons&(PAD_L2|PAD_TRIANGLE)) && (mode==2 ? !unlocked[actor] : (buttons&PAD_L3)!=0)) {
        stickLatch[actor]=1;
        stickPending[actor]=bits&-bits; /* one direction per return to neutral */
        if(buttons&PAD_L3) targetHeld[actor]=-1;
    }
    stickNeutral[actor]=0;
}

s32 TagTeam_Spectating(s32 view) {
    s32 actor;
    if(!TagTeam_Active() || (u32)view>=(u32)viewCount) return 0;
    if(view>=wantedPlayers) return 1;
    actor=controlActors[view];
    if(actor<0)return 1;
    return !TagFusion_Hidden(actor) && !alive(actor);
}
static s32 watchable(s32 actor) {
    return (u32)actor<(u32)count && !TagFusion_Hidden(actor) &&
        (alive(actor) || Port_TagOption(TAG_OPT_SPECTATE_FALLEN));
}
static s32 takeover_allowed(s32 player,s32 actor) {
    extern s32 BtlCharApi_HasCamPriority(s32);
    s32 own=controlActors[player];
    return (u32)actor<(u32)count && actor!=own && !TagTeam_IsEnemy(own,actor) &&
        alive(actor) && players[actor]<0 && TagFusion_SharedBody(actor)<0 &&
        !TagAssist_Active(actor) && !busy(actor) && grab_other(actor)<0 &&
        !BtlCharApi_HasCamPriority(BattleSide_GetObjId(actor));
}
void TagTeam_CycleSpectator(s32 view) {
    s32 n;
    if(!TagTeam_Spectating(view)) return;
    for(n=1;n<=count;n++) {
        s32 next=(viewActors[view]+n)%count;
        if(watchable(next)) {viewActors[view]=next;spectatorChosen[view]=1;return;}
    }
}
s32 TagTeam_TakeoverCandidate(s32 player) {
    s32 actor, i;
    if (!TagTeam_Active() || TagLobby_Practice() || (u32)player>=(u32)wantedPlayers || !Port_TagOption(TAG_OPT_TAKEOVER)) return -1;
    actor=controlActors[player];
    if(actor<0)return -1;
    if(TagFusion_Hidden(actor)) return -1;
    if (alive(actor)) return -1;
    if(spectatorChosen[player]) return takeover_allowed(player,viewActors[player])?viewActors[player]:-1;
    for (i=0;i<count;i++) if (takeover_allowed(player,i)) return i;
    return -1;
}

void TagTeam_RefreshControl(s32 actor) {
    BtlMgrChr *chr=BtlChar_Get(actor);
    BtlInputChr *input=(BtlInputChr *)chr;
    s32 player=TagTeam_InputPlayer(actor);
    sides[actor].control=player>=0 ? BTL_CONTROL_PAD : BTL_CONTROL_CPU;
    sides[actor].pad=player>=0 ? player : 0;
    chr->pad=sides[actor].pad;
    chr->padFlagA=(gSaveData->flags&(2<<chr->pad))!=0;
    chr->injectOn=player<0;
    input->injectButtons=0; input->injectStickX=input->injectStickY=0.0f;
    memset(&input->input,0,sizeof(input->input));
    BtlInput_Init(input);
    targetHeld[actor]=0;
    stickLatch[actor]=stickNeutral[actor]=stickPending[actor]=stickR3[actor]=0;
}
static void bind_control(s32 actor, s32 player) {
    players[actor]=player;
    TagTeam_RefreshControl(actor);
    unlocked[actor]=0;
}

s32 TagTeam_Takeover(s32 player) {
    s32 candidate=TagTeam_TakeoverCandidate(player);
    if (candidate<0 || busy(candidate) || busy(controlActors[player])) return 0;
    bind_control(controlActors[player],-1);
    bind_control(candidate,player);
    viewActors[player]=controlActors[player]=candidate;
    spectatorChosen[player]=0;
    return 1;
}

s32 TagTeam_WinningActor(s32 team) {
    s32 i;
    /* Native results have two presentation sides. A free-for-all winner can
     * occupy any actor slot; map those presentation sides to real fighters. */
    if(TagTeam_Active() && TagSession_Mode()==TAG_MODE_FREE_FOR_ALL && winningTeam>=0) {
        if(team==(winningTeam&1)) return winningTeam;
        for(i=0;i<count;i++) if(i!=winningTeam) return i;
    }
    if (TagTeam_Active()) for (i=0;i<count;i++) if (teams[i]==team && alive(i)) return i;
    return team;
}

void TagTeam_CycleTarget(s32 i) {
    s32 offset;
    if (!TagTeam_Active() || (u32)i >= (u32)count) return;
    if (BtlClash_Opponent(i) >= 0) return;
    if (BtlChar_IsFrozen(BtlChar_Get(i)) || BtlCharApi_IsInRushSequence(i) || BtlCharApi_IsChanging(i)) return;
    for (offset = 1; offset <= count; offset++) {
        s32 next = (targets[i] + offset) % count;
        if (targetable(i,next)) { targets[i] = next; return; }
    }
}

s32 TagTeam_SetCpuTarget(s32 actor,s32 target) {
    if(!TagTeam_Active() || (u32)actor>=(u32)count || (u32)target>=(u32)count ||
       TagTeam_InputPlayer(actor)>=0 || sides[actor].control!=BTL_CONTROL_CPU ||
       !alive(actor) || !alive(target) || !TagTeam_IsEnemy(actor,target) || busy(actor)) return 0;
    targets[actor]=target;
    unlocked[actor]=0;
    return 1;
}

void TagTeam_Update(void) {
    s32 i;
    if (!TagTeam_Active() || (Battle_GetWork()->flags & (BATTLE_FLAG_PAUSE | BATTLE_FLAG_LOADING))) return;
    TagLobby_Update();
    if(TagNet_IsParty() && TagNet_Active() && !TagLobby_Practice())for(i=0;i<count;i++) {
        s32 p=players[i];
        if(p>=0 && !TagNet_MemberPresent(p)) {bind_control(i,-1);controlActors[p]=-1;BtlAiMgr_ResetSide(i);}
    }
    TagFusion_Update();
    TagAssist_Update();
    cinematic_pause();
    TagBalance_Update();
    TagPosition_Update();
    TagCpu_Update();
    {extern void TagTournament_TestUpdate(void);TagTournament_TestUpdate();}
    for (i = 0; i < count; i++) {
        s32 player=TagTeam_InputPlayer(i);
        /* CPU decisions also guard queued damage: changing its recipient
           before the native sequence consumes that queue hits the wrong foe. */
        if(TagChallenge_BetweenWaves()) {targetHeld[i]=0;tapLife[i]=0;stickPending[i]=0;continue;}
        if (player>=0 && !targetable(i,targets[i])) TagTeam_CycleTarget(i);
        if (BtlSeq_IsFighting() && player>=0 && alive(i)) {
            TagTeam_TargetStick(i,gPad[player].held);
            TagTeam_TargetInput(i,(gPad[player].held & PAD_L3)!=0);
        }
    }
    if (BtlSeq_IsFighting()) for (i=0;i<viewCount;i++) {
        s32 actor=controlActors[i],candidate;
        if(TagNet_Active() && actor<0)continue;
        if(!TagTeam_Spectating(i)) {
            viewActors[i]=TagFusion_Owner(actor); spectatorChosen[i]=0; continue;
        }
        if(spectatorChosen[i] && !watchable(viewActors[i])) spectatorChosen[i]=0;
        candidate=TagTeam_TakeoverCandidate(i);
        if(!spectatorChosen[i]) {
            if(candidate>=0) viewActors[i]=candidate;
            else if(!alive(viewActors[i])) {
                s32 n;
                for(n=1;n<=count;n++) {
                    s32 next=(viewActors[i]+n)%count;
                    if(alive(next)) {viewActors[i]=next;break;}
                }
            }
        }
        if(gPad[i].pressed & PAD_L3) TagTeam_CycleSpectator(i);
        if(gPad[i].pressed & PAD_SQUARE) TagTeam_Takeover(i);
    }
    if (++traceFrame % 150 == 0) {
        extern void Port_TagTeamTrace(s32 frame, s32 fighter, s32 chara, s32 team, s32 target, s32 hp, s32 action);
        for (i = 0; i < count; i++) {
            Port_TagTeamTrace(traceFrame, i, sides[i].form.chara, teams[i], targets[i],
                BtlMember_GetActiveGauge(BtlChar_Get(i))->health, BtlChar_Get(i)->action);
            {
                extern void Port_TagTeamControlTrace(s32 fighter, s32 control, s32 inject, s32 freeze, s32 side);
                BtlMgrChr *chr = BtlChar_Get(i);
                Port_TagTeamControlTrace(i, sides[i].control, chr->injectOn, chr->freeze, chr->side);
            }
        }
    }
}

/* Called by the real battle sequence after its clock advances. A lost lead
 * does not end a team battle while another member is still fighting. */
s32 TagTeam_CheckEnd(s32 timeUp) {
    BattleResult *result = Battle_GetResult();
    s32 live[TAG_TEAM_CAPACITY] = {0}, hp[TAG_TEAM_CAPACITY] = {0};
    s32 i, remaining=0, best=-1, tied=0;
    if(TagLobby_Practice())return 0;
    if(TagSession_Mode()==TAG_MODE_TRAINING && !timeUp) return 0;
    for (i = 0; i < count; i++) {
        if (alive(i)) {
            live[teams[i]]++;
            hp[teams[i]] += BtlMember_GetActiveGauge(BtlChar_Get(i))->health;
        }
    }
    for(i=0;i<TAG_TEAM_CAPACITY;i++) if(live[i]) {
        remaining++;
        if(best<0 || hp[i]>hp[best]) { best=i; tied=0; }
        else if(hp[i]==hp[best]) tied=1;
    }
    if (!timeUp && remaining>1) return 0;
    if(!timeUp && live[0] && !live[1] && TagChallenge_Advance())return 0;
    winningTeam=best;
    result->reason = timeUp ? BTL_REASON_TIME_UP : BTL_REASON_KO;
    result->winner = best<0 || (timeUp && tied) ? BTL_RESULT_DRAW : (best&1) ? BTL_RESULT_WIN_P2 : BTL_RESULT_WIN_P1;
    return 1;
}
#endif
