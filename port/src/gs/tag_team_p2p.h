#ifndef BT4_TAG_TEAM_P2P_H
#define BT4_TAG_TEAM_P2P_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int TagPeer_Open(void);
void TagPeer_Close(void);
int TagPeer_Send(const unsigned char packet[512]);
int TagPeer_Read(unsigned char packet[512],uint64_t *at);
int TagPeer_Status(void);
int TagPeer_Ping(void);
int TagPeer_Descriptor(char *json,unsigned capacity);
#ifdef __cplusplus
}
#endif
#endif
