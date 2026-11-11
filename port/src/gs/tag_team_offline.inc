/* Android prototype (Dragon Rage): TAG TEAM's local room only. Included by tag_team_net.c in place of its transports
 * (tag_team_ws.h: curl WebSocket; tag_team_direct.cpp / tag_team_p2p.cpp: direct UDP and WebRTC; supabase_signal.cpp:
 * the room service; voice.cpp: voice chat), none of which this build compiles. The in-process room that "Create Room"
 * opens without a server (Port_TagConnect(NULL): local_room, "LOCAL0") needs none of them; these stand-ins keep the
 * facade's network paths closed: nothing opens, sends or receives. Port_TagConnect also refuses every server on
 * Android, so ws_mode, peer_mode and the TCP client never start. Online play is a later step (its servers are not
 * chosen yet). */
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

/* tag_team_p2p.h */
static int TagPeer_Open(void){return 0;}
static void TagPeer_Close(void){}
static int TagPeer_Send(const unsigned char packet[512]){(void)packet;return 0;}
static int TagPeer_Read(unsigned char packet[512],uint64_t *at){(void)packet;(void)at;return 0;}
static int TagPeer_Status(void){return -1;}
static int TagPeer_Ping(void){return -1;}
static int TagPeer_Descriptor(char *json,unsigned capacity){(void)json;(void)capacity;return 0;}

/* supabase_signal.h: no room service. Not empty: an empty server would make Port_TagEndpoint's callers (Quick Match,
 * Join) open the local room; this name is refused by Port_TagConnect like any other server. */
static const char *Bt4Signal_Project(void){return "none";}

/* voice.h's sessions (voice.cpp is not built) */
#define PortVoice_Session(mode,endpoint,room,cap,seat,name) ((void)0)
#define PortVoice_P2PSession(mode,descriptor,members,count) ((void)0)
#define PortVoice_Leave(mode) ((void)0)
