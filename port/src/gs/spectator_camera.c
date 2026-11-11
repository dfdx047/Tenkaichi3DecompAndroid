/* Host-only presentation state: not serialized, hashed or replayed by netcode. */
#include "presentation.h"
#include "spectator_camera.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
extern int Port_Setting(const char *,int);
extern void Port_SettingSave(const char *,int),Port_SettingsWrite(void);
extern void PortInput_Spectator(float *axes);
extern int gPortOverlayOpen,gPortResim;
static SpecCamera sCamera;
static int sReset,sSeenReset,sCount=2,sInputActive,sLastZoom=100,sZoomHeld;
static Uint64 sLastTick;
int PortSpectator_Mode(void) { return (int)spec_clamp(Port_Setting("spectator_mode",Port_Setting("spectator_camera",0)?1:0),0,3); }
void PortSpectator_SetMode(int mode) {
    mode=(int)spec_clamp(mode,0,3);Port_SettingSave("spectator_mode",mode);
    Port_SettingSave("spectator_camera",mode!=0);Port_SettingsWrite();
    __atomic_add_fetch(&sReset,1,__ATOMIC_RELAXED);
}
int PortSpectator_Target(void) { return (int)spec_clamp(Port_Setting("spectator_target",0),0,9); }
void PortSpectator_SetTarget(int target) { Port_SettingSave("spectator_target",(int)spec_clamp(target,0,9));Port_SettingsWrite(); }
int PortSpectator_Count(void) { return __atomic_load_n(&sCount,__ATOMIC_RELAXED); }
int PortSpectator_Speed(void) { return (int)spec_clamp(Port_Setting("spectator_speed",100),25,400); }
void PortSpectator_SetSpeed(int speed) { Port_SettingSave("spectator_speed",(int)spec_clamp(speed,25,400));Port_SettingsWrite(); }
int PortSpectator_Zoom(void) { return (int)spec_clamp(Port_Setting("spectator_zoom",100),100,300); }
void PortSpectator_SetZoom(int zoom) { Port_SettingSave("spectator_zoom",(int)spec_clamp(zoom,100,300));Port_SettingsWrite(); }
void PortSpectator_ResetCamera(void) { PortSpectator_SetZoom(100);__atomic_add_fetch(&sReset,1,__ATOMIC_RELAXED); }
void PortSpectator_EndBattle(void) { sCamera.live=0;sLastTick=0;__atomic_store_n(&sInputActive,0,__ATOMIC_RELAXED); }
void PortSpectator_SetInputActive(int active) { __atomic_store_n(&sInputActive,active!=0,__ATOMIC_RELAXED); }
int PortSpectator_ConsumesInput(void) { return __atomic_load_n(&sInputActive,__ATOMIC_RELAXED) && PortSpectator_Mode()!=0; }
unsigned PortSpectator_FilterButtons(unsigned buttons) { return PortSpectator_ConsumesInput()?buttons&0x0009u:buttons; }
void PortSpectator_ResolveEye(const float *eye) { memcpy(sCamera.eye,eye,16); }
int PortSpectator_Camera(const float *native_eye,const float *native_rot,const float *targets,int count,
                         float tan_y,float aspect,float *eye,float *rotation,float *zoom) {
    Uint64 now;float dt,config_zoom;SpecInput input={0};int generation,mode,result;
    if(gPortResim)return 0;
    mode=PortSpectator_Mode();now=SDL_GetTicksNS();
    dt=sLastTick?(float)(now-sLastTick)/1000000000.f:1.f/60;sLastTick=now;
    __atomic_store_n(&sCount,count,__ATOMIC_RELAXED);
    generation=__atomic_load_n(&sReset,__ATOMIC_RELAXED);
    if(generation!=sSeenReset){sSeenReset=generation;sCamera.live=0;}
    if(!gPortOverlayOpen && __atomic_load_n(&sInputActive,__ATOMIC_RELAXED))PortInput_Spectator((float *)&input);
    if(getenv("BT4_RENDER_TEST") && getenv("BT4_FREE_CAMERA_AUDIT")){input.yaw=.5f;dt=1.f/60;}
    config_zoom=(float)PortSpectator_Zoom();
    if(!sCamera.live || (int)config_zoom!=sLastZoom)sCamera.zoom=config_zoom;
    result=spec_step(&sCamera,mode,PortSpectator_Target(),PortSpectator_Speed(),sCamera.zoom,dt,native_eye,native_rot,targets,count,input,tan_y,aspect);
    if(!result)return 0;
    sLastZoom=(int)(sCamera.zoom+.5f);
    if(sLastZoom!=(int)config_zoom)Port_SettingSave("spectator_zoom",sLastZoom);
    if(sZoomHeld && input.zoom==0)Port_SettingsWrite();
    sZoomHeld=input.zoom!=0;
    memcpy(eye,sCamera.eye,16);memcpy(rotation,sCamera.rotation,16);*zoom=sCamera.zoom/100;
    return 1;
}
