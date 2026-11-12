#ifndef BT4_MATCH_SUPABASE_H
#define BT4_MATCH_SUPABASE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Bt4DiscoveryStatus {
    int state,role,lobby,peerCount,watchCount,watchGen;
    char code[8],error[256],peerName[20],peerAddr[4][64],watchAddr[32][64];
} Bt4DiscoveryStatus;
int Bt4Discovery_Begin(unsigned generation,int role,int protocol,uint64_t content,const char *name,const char *code,const char *addresses_json,const char *queue);
int Bt4Discovery_Poll(unsigned generation,Bt4DiscoveryStatus *out);
void Bt4Discovery_End(unsigned generation);
int Bt4Discovery_Counts(int protocol,uint64_t content,int counts[5]);
int Port_OnlinePresence(int active); /* -1 until an authenticated aggregate is received */
int Bt4Discovery_TagCounts(uint64_t content,int counts[3]);
typedef struct Bt4RankedEntry {char name[64];int rating,wins,losses;} Bt4RankedEntry;
typedef struct Bt4RankedView {
    int state,rating,wins,losses,count,page,pages; /* 0 loading, 1 prepared, -1 unavailable */
    int topState;
    char season[40],updated[40];
    Bt4RankedEntry top[5];
} Bt4RankedView;
void Port_RankedRefresh(int refreshStatus);
void Port_RankedView(Bt4RankedView *out);
int Port_RankedTopTen(Bt4RankedEntry out[10]);
void Port_RankedPage(int direction);
void Port_RankedSearch(int selected);
int Port_RankedActive(void);
void Port_RankedObserve(int battleState,int outcomeBits,unsigned tick,unsigned confirmed,int resimulating,int localConcession);
int Port_RankedResult(int *rating,int *delta);
void Port_RankedDisconnected(int peerGone);
int Port_RankedTakeSettlement(int *rating,int *delta);
enum {
    BT4_NICKNAME_LOADING, BT4_NICKNAME_READY, BT4_NICKNAME_REQUIRED, BT4_NICKNAME_DISABLED,
    BT4_NICKNAME_SAVING, BT4_NICKNAME_TAKEN, BT4_NICKNAME_INVALID, BT4_NICKNAME_UNAVAILABLE,
    BT4_NICKNAME_OPTIONAL
};
void Port_OnlineNicknameBegin(void);
int Port_OnlineNicknameView(char name[20]);
int Port_OnlineNicknameSubmit(const char *name);
#ifdef __cplusplus
}
#endif
#endif
