/* TAG TEAM's room service (admission and WebRTC negotiation only: never gameplay), for the participant-hosted room of
 * tag_team_p2p.cpp. Two services speak the same messages (supabase_signal.h's operations and events, consumer 1):
 *   - BT4decomp's Supabase signaling (supabase_signal.cpp): the PC's, as upstream;
 *   - Dragon Net (dragonnet_tag.cpp, Dragon Rage): POST /v1/tag of Dragon Net's Worker, polled. Android always (Dragon
 *     Rage prototype); a PC build when BT4_TAG_DRAGONNET (1, or the service's URL) or BT3_MATCH_URL is set.
 * Implemented in dragonnet_tag.cpp. */
#ifndef BT4_TAG_SIGNAL_H
#define BT4_TAG_SIGNAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int TagSignal_DragonNet(void);        /* 1: Dragon Net carries TAG TEAM's rooms */
const char *TagSignal_Project(void);  /* the room service's name (Port_TagEndpoint): Dragon Net's URL, or Supabase's */
int TagSignal_Open(void);
void TagSignal_Listen(int consumer,int enabled);
int TagSignal_Command(int consumer,unsigned serial,const char *operation,const char *args_json);
int TagSignal_Read(int consumer,char *json,size_t capacity);
/* How soon the room needs news (Dragon Net polls; Supabase pushes and ignores it): 2 negotiating or searching, 1 the
 * owner's lobby (players may join), 0 nothing expected */
void TagSignal_Pace(int level);
/* Games waiting in TAG TEAM's three quick queues (Free for All, 2 vs 2, 5 vs 5); 0 while not known */
int TagSignal_QueueCounts(uint64_t content,int out[3]);
#ifdef __cplusplus
}
#endif
#endif
