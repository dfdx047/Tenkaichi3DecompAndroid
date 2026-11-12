/* Presentation-only battle snapshot; no audio callback reads the game heap. */
#include <SDL3/SDL.h>
#include <string.h>
#include "spatial_audio.h"
static SDL_SpinLock sLock;
static struct {int active,listener,count,alive[10];float pos[40];Uint64 at;} sWorld;
void PortAudio_World(int active,int listener,int count,const float *positions,const int *alive){
    SDL_LockSpinlock(&sLock);
    sWorld.active=active&&count>0&&count<=10&&listener>=0&&listener<count;
    sWorld.listener=listener;sWorld.count=sWorld.active?count:0;sWorld.at=SDL_GetTicks();
    if(sWorld.active){memcpy(sWorld.pos,positions,count*4*sizeof(float));memcpy(sWorld.alive,alive,count*sizeof(int));}
    SDL_UnlockSpinlock(&sLock);
}
float PortAudio_Gain(int actor,int voice){
    float gain=1;SDL_LockSpinlock(&sLock);
    if(sWorld.active&&actor>=0&&actor<sWorld.count){
        const float *a=sWorld.pos+sWorld.listener*4,*b=sWorld.pos+actor*4;
        float x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2];
        gain=SDL_GetTicks()-sWorld.at>1500?0:Bt4_AudioFalloff(sqrtf(x*x+y*y+z*z),voice?8:10,voice?120:160);
    }
    SDL_UnlockSpinlock(&sLock);return gain;
}
int PortAudio_Alive(int actor){
    int alive=1;SDL_LockSpinlock(&sLock);
    if(sWorld.active&&actor>=0&&actor<sWorld.count)alive=sWorld.alive[actor];
    SDL_UnlockSpinlock(&sLock);return alive;
}
