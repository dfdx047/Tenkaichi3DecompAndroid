#ifndef BT4_PRESENTATION_H
#define BT4_PRESENTATION_H
#ifdef __cplusplus
extern "C" {
#endif
int PortHud_Hidden(void);
void PortHud_Toggle(void);
void PortHud_SetHidden(int hidden);
int PortSpectator_Mode(void);
void PortSpectator_SetMode(int mode);
int PortSpectator_Target(void);
void PortSpectator_SetTarget(int target);
int PortSpectator_Count(void);
int PortSpectator_Speed(void);
void PortSpectator_SetSpeed(int speed);
int PortSpectator_Zoom(void);
void PortSpectator_SetZoom(int zoom);
void PortSpectator_ResetCamera(void);
void PortSpectator_EndBattle(void);
void PortSpectator_SetInputActive(int active);
int PortSpectator_ConsumesInput(void);
unsigned PortSpectator_FilterButtons(unsigned buttons);
int PortSpectator_Camera(const float *native_eye,const float *native_rot,const float *targets,int count,
                         float tan_y,float aspect,float *eye,float *rotation,float *zoom);
void PortSpectator_ResolveEye(const float *eye);
#ifdef __cplusplus
}
#endif
#endif
