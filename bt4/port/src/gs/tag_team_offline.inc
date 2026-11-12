/* Android prototype (Dragon Rage): TAG TEAM's transports this build does not compile, as inert stand-ins. Included by
 * tag_team_net.c in place of tag_team_ws.h (curl WebSocket to a Tag room server), tag_team_direct.cpp (that server's
 * direct route) and voice.cpp (voice chat). Online rooms are the participant-hosted ones (tag_team_p2p.cpp: WebRTC,
 * libdatachannel.so loaded at run time) through Dragon Net (dragonnet_tag.cpp), which need none of these; the local
 * room ("LOCAL0") needs no transport at all. Port_TagConnect refuses every other server on Android, so ws_mode without
 * peer_mode and the TCP client never start: nothing here opens, sends or receives. */
#include <SDL3/SDL.h>
#include "voice.h"

/* tag_team_ws.h (the fields Port_TagIncomingWake and Port_TagYield read: no lock, so both return at once) */
static struct {SDL_Mutex *lock;SDL_Condition *receiveReady;SDL_AtomicInt status;unsigned inRead,inWrite;} sTagWs;
static void TagWs_Close(void){}
static int TagWs_Open(const char *root,const char *code){(void)root;(void)code;return 0;}
static int TagWs_Send(const unsigned char *packet){(void)packet;return 0;}
static int TagWs_Read(unsigned char *packet,Uint64 *arrived){(void)packet;(void)arrived;return 0;}
static void TagWs_QueueCounts(unsigned *rx,unsigned *tx){*rx=*tx=0;}

/* tag_team_direct.h */
static int TagDirect_Available(void){return 0;}
static void TagDirect_Session(const char *endpoint,const char *code,const char *cap,unsigned self){(void)endpoint;(void)code;(void)cap;(void)self;}
static void TagDirect_State(const unsigned char packet[512]){(void)packet;}
static void TagDirect_Close(void){}
static int TagDirect_Active(void){return 0;}
static int TagDirect_Ping(void){return -1;}
static void TagDirect_RelayPing(int milliseconds){(void)milliseconds;}
static int TagDirect_Send(const unsigned char packet[512]){(void)packet;return 0;}
static int TagDirect_Read(unsigned char packet[512],uint64_t *arrived){(void)packet;(void)arrived;return 0;}
static int TagDirect_Pending(void){return 0;}
void Port_TagIncomingWake(void);

/* voice.h's sessions (voice.cpp is not built) */
#define PortVoice_Session(mode,endpoint,room,cap,seat,name) ((void)0)
#define PortVoice_P2PSession(mode,descriptor,members,count) ((void)0)
#define PortVoice_Leave(mode) ((void)0)
