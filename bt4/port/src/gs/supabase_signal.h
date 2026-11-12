/* Discovery and ICE only. All functions return without network I/O on callers. */
#ifndef BT4_SUPABASE_SIGNAL_H
#define BT4_SUPABASE_SIGNAL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int Bt4Signal_Open(void);
int Bt4Signal_Status(void); /* 0 connecting, 1 subscribed, -1 failed */
void Bt4Signal_Listen(int consumer,int enabled);
/* consumer 0 room/duel, 1 Tag, 2 voice, 3 HTTP-only presence, 4 HTTP-only ranked reads, 5 fight liveness/forfeits.
 * Presence does not subscribe to Realtime. Serial correlates an RPC reply. */
int Bt4Signal_Command(int consumer,unsigned serial,const char *operation,const char *args_json);
int Bt4Signal_Read(int consumer,char *json,size_t capacity);
void Bt4Signal_Close(void);
const char *Bt4Signal_Project(void);
void Bt4Signal_ReleaseRoom(const char *generation);
#ifdef __cplusplus
}
#endif
#endif
