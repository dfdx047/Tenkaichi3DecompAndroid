#ifndef BT4_TAG_TEAM_DIRECT_H
#define BT4_TAG_TEAM_DIRECT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int TagDirect_Available(void);
void TagDirect_Session(const char *endpoint,const char *code,const char *cap,unsigned self);
void TagDirect_State(const unsigned char packet[512]);
void TagDirect_Close(void);
int TagDirect_Active(void);
int TagDirect_Host(void);
int TagDirect_Ping(void);
void TagDirect_RelayPing(int milliseconds);
int TagDirect_Send(const unsigned char packet[512]);
int TagDirect_Read(unsigned char packet[512],uint64_t *arrived);
int TagDirect_Pending(void);
void Port_TagIncomingWake(void);
#ifdef __cplusplus
}
#endif
#endif
