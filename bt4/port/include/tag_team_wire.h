#ifndef PORT_TAG_TEAM_WIRE_H
#define PORT_TAG_TEAM_WIRE_H
#include "tag_team.h"
#define TT_WIRE_BYTES 512
enum { TTW_OPEN=1, TTW_JOIN, TTW_SELECT, TTW_CHOOSE, TTW_LOAD, TTW_LOADED,
       TTW_ARM, TTW_ACK, TTW_START, TTW_PING, TTW_INPUT, TTW_CANCEL_SELECT=12, TTW_TEAMS=13, TTW_SLOT=14, TTW_ARENA=15, TTW_CHAT=16, TTW_HASH=17, TTW_RESULT=18, TTW_MOVE=19, TTW_VOICE_TICKET=20, TTW_RESULT_ENDED=21, TTW_STATE=100, TTW_FRAME };
uint32_t Ttw_Read(const unsigned char *p);
void Ttw_Write(unsigned char *p, uint32_t v);
void Ttw_Request(unsigned char *p, int command, uint32_t epoch);
void Ttw_Snapshot(unsigned char *p, const TtRoom *r, uint32_t member, int result, const char *code);
int Ttw_Decode(const unsigned char *p, TtRoom *r, uint32_t *member, int *result, char *code);
/* Host facade, normal pointer ABI. Called from the native port menu. */
int Port_TagConnect(const char *server);
int Port_TagCopyCode(void),Port_TagJoinCode(const char *code);
int Port_TagDataFingerprint(unsigned long long *out);
void Port_TagClose(void);
int Port_TagCancelSelect(void);
unsigned Port_TagMenuMillis(void);
void Port_TagMenuTestAdvance(unsigned ms);
void Port_TagPoll(void);
void Port_TagOpen(int size, int time, int delay, const char *name, uint64_t content);
void Port_TagCPUOptions(int fill,int difficulty);
void Port_TagModeOptions(int free_for_all,int quick);
int Port_TagFreeForAll(void);
void Port_TagJoin(const char *code, const char *name, uint64_t content);
void Port_TagCommand(int command, int a, int b);
int Port_TagNetwork(void);
int Port_TagLocalBattle(void);
int Port_TagStage(void),Port_TagBgm(void),Port_TagAttacks(void),Port_TagDelay(void);
unsigned Port_TagSeed(void);
int Port_TagAllLoaded(void),Port_TagAllArmed(void);
void Port_TagArena(int stage,int bgm,int attacks);
int Port_TagPhase(void);
int Port_TagCount(void);
int Port_TagCapacity(void);
int Port_TagSlots(void);
int Port_TagTeam(int seat);
int Port_TagTeamIndex(int seat);
int Port_TagTeamCount(int team);
int Port_TagTeamSlots(int team);
int Port_TagTeams(int left,int right);
int Port_TagSlot(int seat,int cpu,int difficulty);
int Port_TagCanStart(void);
int Port_TagSeat(void);
int Port_TagDraftSeat(void);
int Port_TagHumans(void);
int Port_TagCPU(int seat);
int Port_TagDifficulty(int seat);
int Port_TagTurn(void);
int Port_TagHost(void);
int Port_TagPlayerChara(int seat);
int Port_TagPlayerCostume(int seat);
int Port_TagConfirmed(int seat);
const char *Port_TagPlayerName(int seat);
const char *Port_TagCode(void);
const char *Port_TagError(void);
int Port_TagSendInput(uint32_t frame,const unsigned char pad[TT_PAD_BYTES]);
int Port_TagTakeFrame(unsigned char out[TT_MAX_PLAYERS][TT_PAD_BYTES]);
#endif
