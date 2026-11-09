#ifndef TAG_LOBBY_RULES_H
#define TAG_LOBBY_RULES_H

/* Persistent room rules, independent of sockets, presentation and game heaps.
 * Stable member IDs never change when another member leaves. The practice
 * state belongs in rollback; the host's draft belongs to the room transport. */
#define TL_MEMBERS 10
#define TL_DIFFICULTIES 5
#define TL_DUMMIES (1 + TL_DIFFICULTIES)
#define TL_ACTORS (TL_MEMBERS - 1 + TL_DUMMIES)
#define TL_NO_MEMBER (-1)
#define TL_HOST 0
#define TL_RECOVER_TICKS 90
#define TL_REQUEST_TICKS 150
enum { TL_TEAM, TL_FFA, TL_TRAINING, TL_GAUNTLET, TL_TOURNAMENT };
enum { TL_WAITING, TL_READY_CHECK, TL_LOADING, TL_MATCH, TL_RETURNING, TL_CLOSED };
enum { TL_OK, TL_DENIED, TL_STALE, TL_FULL, TL_INVALID, TL_BUSY, TL_NOT_READY };

typedef struct TlLoadout { int chara, costume, items[8]; } TlLoadout;
typedef struct TlMember {
    int present, generation, ready, loaded, team, slot;
    TlLoadout loadout;
    int wins, losses, draws;
} TlMember;
typedef struct TlDraft {
    int mode, stage, music, time, difficulty, preset, heal;
    int count, teams, playerLimit;
    /* Slot ownership is a member ID or -1 for a CPU, not a controller port. */
    int owner[10], team[10], cpu[10];
    TlLoadout fighters[10];
} TlDraft;
typedef struct TlRoom {
    int revision, phase, round, generation;
    TlMember members[TL_MEMBERS];
    TlDraft draft;
} TlRoom;
typedef struct TlPracticeActor {
    int member, difficulty, enabled;
    int opponent, request, requestTicks, recovering, editing;
    int hp, maxHp;
} TlPracticeActor;
typedef struct TlPractice {
    TlPracticeActor actors[TL_ACTORS];
    /* Per-member scores survive rebuilding a practice map between matches. */
    int wins[TL_MEMBERS], losses[TL_MEMBERS], draws[TL_MEMBERS];
} TlPractice;

typedef struct TlTournament {
    int active,match,entered;
    int left[7],right[7],winner[7],count[8];
    TlLoadout fighters[8][5];
    int owner[8][5],cpu[8][5];
} TlTournament;

static inline int TlTournament_Build(TlTournament *t,const TlDraft *d) {
    int i,k,n=0,map[4]={-1,-1,-1,-1},counts[4]={0},largest=1;
    if(d->count<1 || d->count>10)return TL_INVALID;
    for(i=0;i<d->count;i++)if(d->team[i]<0 || d->team[i]>3 || ++counts[d->team[i]]>5)return TL_INVALID;
    *t=(TlTournament){0};for(i=0;i<8;i++)for(k=0;k<5;k++)t->owner[i][k]=-1;for(i=0;i<7;i++)t->winner[i]=-1;
    for(i=0;i<4;i++)if(counts[i]){map[i]=n++;if(counts[i]>largest)largest=counts[i];}
    for(i=0;i<d->count;i++) {int team=map[d->team[i]],slot=t->count[team]++;
        t->fighters[team][slot]=d->fighters[i];t->owner[team][slot]=d->owner[i];t->cpu[team][slot]=d->cpu[i];}
    t->entered=n;
    for(i=n;i<8;i++){t->count[i]=largest;for(k=0;k<largest;k++){t->fighters[i][k]=d->fighters[(i+k)%d->count];t->cpu[i][k]=d->difficulty;}}
    for(i=0;i<4;i++){t->left[i]=i*2;t->right[i]=i*2+1;}
    for(i=4;i<7;i++)t->left[i]=t->right[i]=-1;
    t->active=1;return TL_OK;
}
static inline int TlTournament_CpuMatch(const TlTournament *t) {
    int i,k;
    if(t->active!=1 || t->match<0 || t->match>=7)return 0;
    for(i=0;i<2;i++){int team=i?t->right[t->match]:t->left[t->match];if(team<0 || team>=8)return 0;
        for(k=0;k<t->count[team];k++)if(t->owner[team][k]>=0)return 0;}
    return 1;
}
static inline int TlTournament_Advance(TlTournament *t,int side) {
    int match=t->match,winner;
    if(t->active!=1 || match<0 || match>=7 || side<0 || side>1)return TL_INVALID;
    winner=side?t->right[match]:t->left[match];if(winner<0 || winner>=8)return TL_INVALID;
    t->winner[match]=winner;
    if(match<4){if(match&1)t->right[4+match/2]=winner;else t->left[4+match/2]=winner;}
    else if(match<6){if(match==4)t->left[6]=winner;else t->right[6]=winner;}
    t->match++;if(t->match==7)t->active=2;return TL_OK;
}


void TlRoom_Init(TlRoom *r, const TlLoadout *host);
int TlRoom_Join(TlRoom *r, const TlLoadout *fighter);
int TlRoom_Leave(TlRoom *r, int member);
int TlRoom_Kick(TlRoom *r, int sender, int member);
int TlRoom_Edit(TlRoom *r, int sender, int revision, const TlDraft *draft);
int TlRoom_CancelSetup(TlRoom *r, int sender, int revision);
int TlRoom_Ready(TlRoom *r, int sender, int revision, int ready);
int TlRoom_Claim(TlRoom *r, int sender, int revision, int playing, int team);
int TlRoom_Start(TlRoom *r, int sender, int revision);
int TlRoom_Loaded(TlRoom *r, int sender, int round);
int TlRoom_Return(TlRoom *r, int sender, int round);
int TlRoom_SetFighter(TlRoom *r, int sender, int generation, const TlLoadout *fighter);
int TlDraft_Valid(const TlRoom *r, const TlDraft *draft);
int TlRoom_MatchMask(const TlRoom *r);
int TlRoom_PracticeMask(const TlRoom *r);

void TlPractice_Init(TlPractice *p, const TlRoom *r, int health);
void TlPractice_Refresh(TlPractice *p, const TlRoom *r, int health);
int TlPractice_MemberActor(const TlPractice *p, int member);
/* A request expires unless the other member accepts while still nearby.
 * Calls must come from synchronized simulation input, never local UI input. */
int TlPractice_Challenge(TlPractice *p, int a, int b, int nearby);
int TlPractice_CanHit(const TlPractice *p, int a, int b);
int TlPractice_Contact(TlPractice *p, int a, int b);
int TlPractice_CpuActive(const TlPractice *p, int actor);
int TlPractice_Edit(TlPractice *p, int actor, int editing);
void TlPractice_Remove(TlPractice *p, int actor);
/* Returns a bitmask of actors whose health/position must be restored. Both
 * simultaneous KOs are one draw; dummy fights never enter the leaderboard. */
unsigned TlPractice_Tick(TlPractice *p);

#endif
