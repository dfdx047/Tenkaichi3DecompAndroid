#ifndef BT4_SPATIAL_AUDIO_H
#define BT4_SPATIAL_AUDIO_H
#include <math.h>
/* Distances use the native arena units, not screen pixels or meters. */
static inline float Bt4_AudioFalloff(float distance,float innerRadius,float outerRadius){
    float t;if(!isfinite(distance)||distance<0||outerRadius<=innerRadius)return 0;
    t=(distance-innerRadius)/(outerRadius-innerRadius);if(t<=0)return 1;if(t>=1)return 0;
    return 1-t*t*(3-2*t);
}
#ifdef __cplusplus
extern "C" {
#endif
void PortAudio_World(int active,int listener,int count,const float *positions,const int *alive);
float PortAudio_Gain(int actor,int voice);
int PortAudio_Alive(int actor);
void Port_SeBind(int handle,int actor);
void Port_SeActorFile(int actor,int file);
int Port_SeActorLoop(int actor,int id);
void *Port_ActorAdx(int actor,int voice);
void Port_ActorAdxStopAll(void);
void Port_ActorAdxPauseAll(int paused);
void Port_ActorAdxStop(int actor,int voice);
#ifdef __cplusplus
}
#endif
#endif
