#ifndef TAG_NET_H
#define TAG_NET_H
#include "../../../include/battle/tag_options.h"
#include "../../../include/battle/tag_lobby_rules.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { TN_FIGHTERS=10, TN_PLAYERS=10, TN_SPECTATORS=10, TN_OBSERVER=10, TN_OPTIONS=TAG_OPT_COUNT };
typedef struct TagNetConfig {
    int revision,disc,seed,stage,music,time,cpu,size[2];
    int chara[10],costume[10],owner[10],options[TN_OPTIONS];
    int cpuLevel[10],items[10][8];
    /* Persistent-room identity is separate from a claimed battle slot. */
    int party,practice,mode,preset,heal;
    TlRoom lobby;
} TagNetConfig;
static inline void TagNet_CopyFighter(TagNetConfig *c,int to,int from) {
    int j;c->chara[to]=c->chara[from];c->costume[to]=c->costume[from];c->owner[to]=c->owner[from];c->cpuLevel[to]=c->cpuLevel[from];
    for(j=0;j<8;j++)c->items[to][j]=c->items[from][j];
    c->lobby.draft.team[to]=c->lobby.draft.team[from];
}
typedef struct TagNetStatus {
    int phase,role,host,connected,playerMask,ready[TN_PLAYERS],spectators,view,freeCam;
    unsigned frame,rollbacks;
    char message[192];
    TagNetConfig config;
} TagNetStatus;
/* The network and local camera are deliberately outside the rollback state.
 * Only agreed inputs and the immutable match configuration enter the game. */
int TagNet_Enabled(void);
void TagNet_Enable(int enabled);
void TagNet_Party(int enabled);
int TagNet_IsParty(void);
int TagNet_IsPractice(void);
int TagNet_Observer(void);
void TagNet_ChooseMode(int mode);
void TagNet_ChoosePreset(int mode,int preset);
int TagNet_HostMenu(void);
int TagNet_HostPoll(void);
int TagNet_HostChoosing(void);
typedef struct TagLobbyRoster {int count,role,host,waiting;int member[10],ping[10];char code[8];} TagLobbyRoster;
void TagNet_Roster(unsigned address);
void TagLobbyRoster_Draw(int hostMenu);
void TagNet_ReturnPractice(void);
int TagNet_PracticeEnding(void);
typedef struct TagPracticeUi {int visible,host,choice,role;TlRoom lobby;} TagPracticeUi;
typedef struct TagPracticeEvent {unsigned at;int member,present,generation,kind;TlLoadout fighter;} TagPracticeEvent;
void TagNet_PracticeUi(unsigned address);
void TagNet_PracticeState(unsigned address);
void TagNet_PracticeScores(unsigned address);
int TagNet_PracticeEvent(unsigned index,unsigned address);
int TagNet_MemberPresent(int member);
void TagNet_Kick(int member);
void TagNet_Tournament(unsigned address);
void TagNet_SkipTournament(void);
void TagNet_BattleResult(int winningSide);
void TagNet_PracticeLoadout(int member,unsigned address);
int TagNet_NativeMenu(void);
int TagNet_Active(void);
int TagNet_Role(void);
int TagNet_MenuInput(void);
int TagNet_Resim(void);
int TagNet_Stopped(void);
int TagNet_Room(void);
void TagNet_MenuPoll(void);
void TagNet_CopyConfig(unsigned address);
void TagNet_Loaded(void);
void TagNet_Finished(void);
void TagNet_Tick(unsigned blank);
void TagNet_Input(int player,unsigned char *data);
void TagNet_Status(TagNetStatus *out);
void TagNet_Start(int role,const char *address,int port);
void TagNet_Edit(const TagNetConfig *config);
void TagNet_Ready(int ready);
/* Lobby only: release your fighter or claim an available one (-1 = automatic). */
void TagNet_SwitchRole(int playing,int fighter);
void TagNet_Leave(void);
void TagNet_Watch(int actor,int freeCam);
int TagNet_View(void);
int TagNet_FreeCamera(float *position,float *rotation);
int TagNet_Option(int id,int fallback);
void TagNet_Draw(void);
#ifdef __cplusplus
}
#endif
#endif
