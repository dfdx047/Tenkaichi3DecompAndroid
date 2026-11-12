#ifndef BT4_VOICE_H
#define BT4_VOICE_H
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned id;int seat,team,muted,speaking,direct;float proximity;char name[64];} PortVoicePeer;
typedef struct {int enabled,mode,key,button,input,output,gain,volume,threshold,indicators,muted; } PortVoiceOptions;
typedef struct {int enabled,sessionMode,connected,capture,transmitting,speaking,host,allowRoom,channel,count;float level;unsigned sent,received,direct,relayed,decoded;char status[160];PortVoicePeer peers[10];} PortVoiceState;
/* 0: idle, 1: own voice detected, 2: muted / no recording device. */
static inline int PortVoice_MicStatus(const PortVoiceOptions *o,const PortVoiceState *s){
    if(!o->enabled||o->muted||o->gain<=0||!s->capture)return 2;
    return s->connected&&s->speaking?1:0;
}
void PortVoice_Options(PortVoiceOptions *out);
void PortVoice_OptionsSet(const PortVoiceOptions *options);
void PortVoice_State(PortVoiceState *out);
void PortVoice_Event(const SDL_Event *event);
void PortVoice_Session(int mode,const char *endpoint,const char *room,const char *cap,int seat,const char *name);
typedef struct {unsigned id;int seat,team;char name[20];} PortVoiceRoomMember;
void PortVoice_P2PSession(int mode,const char *descriptor,const PortVoiceRoomMember *members,int count);
void PortVoice_Leave(int mode);
void PortVoice_Mute(unsigned id,int mute);
void PortVoice_World(int mode,int listener,int count,const float *positions);
float PortVoice_ProximityGain(float distance);
void PortVoice_DeviceName(int recording,char *out,int size);
int PortVoice_NewKey(unsigned char out[32]);
void PortVoice_DuelKey(const unsigned char key[32],int seat,const char *name);
int PortVoice_TestRun(void);
void PortVoice_Shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
