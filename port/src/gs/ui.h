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
    int fxOff;       /* bits: 1 outline, 2 see-through tint, 4 depth tint, 8 glare and glow, 16 distance blur, 32 movement blur */
    int glow;        /* percent */
    int music, effects, voices, stage; /* volumes, percent */
    int display;     /* 0 = the desktop chooses, n = the n-th display; used at the next start */
    int texPack;     /* 1 = the texture pack's replacements are used */
    int texPackCount; /* read only: replacement textures found in the textures folder */
    int vsync;       /* 1 = a picture waits for the display's refresh (no tearing where the driver has no other way) */
    int frameSkip;   /* 1 = a tick's picture is left out when the machine is behind, instead of the game slowing down */
    int hudLayout;   /* a fight's HUD in a wide picture: 0 at the edges, 1 at the edges with long health bars, 2 in the middle as in 4:3 */
    int menuFont;    /* 0 Original (default), then entries in the generated font catalog */
    int textFont;    /* separate font for dialogue and other shared Font_Print text */
    int smooth2d;    /* "2D filtering": 0 sharp (a PS2 pixel is a block), 1 smooth (sampled per output pixel) */
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
int *PortInput_OverlayKey(void);       /* the key that opens and closes the settings window (scancode, 0 = none) */
int *PortInput_OverlayPad(void);       /* the controller button that does (-1 = none; START + SELECT always do) */
int PortInput_Hotkey(const SDL_Event *ev); /* 1 = the event opens or closes the settings window */
void PortInput_Swallow(void);           /* the controller buttons held now are not the game's until they are let go */
void PortInput_SwallowKeys(void);       /* the same for the bound keys held now */
int *PortInput_ChatKey(void);           /* the key that opens the online chat's line (scancode, 0 = none) */
void PortInput_ResetDefaults(int player);
#define PORT_DEADZONE_MAX 90   /* percent */
#define PORT_TRIGGER_AT 12000  /* of 32767: from here a trigger counts as pressed */
int *PortInput_Deadzone(int player);  /* [2] left, right stick: -1 = the game's own (per axis), 0..PORT_DEADZONE_MAX = a round one, percent */
int *PortInput_Vibration(int player); /* [2] on (0 / 1), strength (0..100 percent) */
void PortInput_VibrationTest(int player); /* half a second of both motors */
typedef struct PortStickView { /* [0] the left stick / trigger, [1] the right */
    int connected, canVibrate;
    float raw[2][2];            /* x, y as the controller gives them: -1..1, + = right / down */
    unsigned char bytes[2][2];  /* the pad's bytes the game is given for them */
    float game[2][2];           /* what the game makes of those (Pad_Update) */
    int dir[2];                 /* the direction the game takes the stick for: 0 none, 1 up, 2 down, 3 left, 4 right */
    float trigger[2];           /* 0..1 */
    int triggerHeld[2];
} PortStickView;
void PortInput_StickView(int player, PortStickView *out);
void PortInput_Save(void);
extern int gPortOverlayOpen;   /* the settings window is open: neither keyboard nor controller reaches the game */
extern int gPortInputCapture;  /* it is waiting for a key or button to bind: nothing reaches the game */
extern int gPortChatTyping;    /* a line of the online chat is being typed: the keyboard does not reach the game */

/* ui.cpp */
int Ui_Init(SDL_Window *window, SDL_GPUDevice *device, void *gl_context, const char *gpu_name); /* device: the SDL GPU back end; NULL and a GL context: the OpenGL one */
void Ui_DrawGL(void); /* the OpenGL back end: draws this frame's window with the context current */
int Ui_Event(const SDL_Event *ev); /* 1 = the window used the event */
int GsDraw_FrameSkip(void);       /* gs_draw.c */
void GsDraw_FrameSkipSet(int on);
void GsGpu_Vsync(int on); /* gs_gpu.c */
void GsGl_Vsync(int on);  /* gs_gl.c */
int Port_Setting(const char *name, int def); /* plat_settings.c */
void Ui_Toggle(void);
int Ui_QuitAsk(void); /* Esc outside the window: asks before the game closes. 0 = there is no window to ask in */
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
