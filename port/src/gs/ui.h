/* The settings window (ui.cpp, Dear ImGui) and what it reaches of the renderer and the input code. C and C++. */
#ifndef PORT_UI_H
#define PORT_UI_H
#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct PortVideo {
    int scale;       /* internal resolution multiplier, 1..8 */
    int aspectMilli; /* width / height of the picture * 1000 */
    int fullscreen;
    int fxOff;       /* bits: 1 outline, 2 see-through tint, 4 depth tint, 8 glare and glow, 16 distance blur */
    int glow;        /* percent */
    int music, effects; /* volumes, percent */
    int display;     /* 0 = the desktop chooses, n = the n-th display; used at the next start */
    int texPack;     /* 1 = the texture pack's replacements are used */
    int texPackCount; /* read only: replacement textures found in the textures folder */
} PortVideo;

/* gs_draw.c */
void GsGpu_GetSettings(PortVideo *v);
void GsGpu_SetSettings(const PortVideo *v); /* applies what changed and saves it */
/* gs_draw.c: the one window both back ends share (created before the device / the GL context). */
SDL_Window *GsDraw_WindowCreate(int opengl);
SDL_Window *GsDraw_Window(void);
void GsDraw_WindowShape(void);
void GsDraw_FullscreenSet(int on);
void GsDraw_FullscreenToggle(void); /* F11: switch, save it, write the settings */

/* gs_input.c: bindings of both players. Actions 0..15 are the pad's buttons, 16..23 the two sticks' directions. */
#define PORT_ACTIONS 24
#define PORT_PAD_BUTTONS 16
#define PORT_PAD_LT 100 /* controller sources besides SDL_GamepadButton values: the triggers */
#define PORT_PAD_RT 101
const char *PortInput_ActionLabel(int action);
int *PortInput_Keys(int player);       /* [PORT_ACTIONS] SDL scancodes, 0 = none */
int *PortInput_PadButtons(int player); /* [PORT_PAD_BUTTONS] controller source, -1 = none */
int *PortInput_PadSlot(int player);    /* which connected controller: -1 none, 0 = first, ... */
void PortInput_ResetDefaults(int player);
void PortInput_Save(void);
extern int gPortOverlayOpen;   /* the settings window is open: the keyboard does not reach the game */
extern int gPortInputCapture;  /* it is waiting for a key or button to bind: nothing reaches the game */

/* ui.cpp */
int Ui_Init(SDL_Window *window, SDL_GPUDevice *device, void *gl_context); /* device: the SDL GPU back end; NULL and a GL context: the OpenGL one */
void Ui_DrawGL(void); /* the OpenGL back end: draws this frame's window with the context current */
int Ui_Event(const SDL_Event *ev); /* 1 = the window used the event */
void Ui_Toggle(void);
int Ui_IsOpen(void);  /* a window (settings, online) is open */
int Ui_Pauses(void); /* the settings window is open and the game waits behind it */
void Ui_Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target);      /* builds and draws this frame's window */
void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target); /* the same picture onto another texture */

/* Stage-name overlay: the names of stages added from outside the disc are drawn by the port (its font has no
   stylized glyphs), over the picture, in the game's own coordinates. gs_gpu.c fills the present rectangle (the
   letterboxed 512x448 picture in window pixels); the game's menu fills the name's rectangle (game pixels) and
   which strip image to show, each frame it draws a stage name. ui.cpp combines them. */
extern volatile int gUiPresentX, gUiPresentY, gUiPresentW, gUiPresentH; /* window pixels */
extern volatile int gUiNameX, gUiNameY, gUiNameW, gUiNameH;            /* game pixels */
extern volatile int gUiNameIdx;   /* strip image to show; < 0 = none */
extern volatile int gUiNameReady; /* 1 when the name strip was loaded (else the game prints with its own font) */

/* The same for the music select's added tracks: a second strip (gamedata/songs/names.rgba). */
extern volatile int gUiSongX, gUiSongY, gUiSongW, gUiSongH; /* game pixels */
extern volatile int gUiSongIdx;   /* < 0 = none */
extern volatile int gUiSongReady;
extern volatile int gUiSongLit;   /* 1 while the music list is open: the disc's names are then drawn from their second sheet */

#ifdef __cplusplus
}
#endif
#endif
