/* Script mods (port/src/gs/mods.c, docs/port/mods.md): Lua scripts in mods/<name>/mod.lua that read the fight,
   draw over the picture and dress the camera as it is shown. What the game's thread and the render thread share.
   C and C++. */
#ifndef PORT_MODS_H
#define PORT_MODS_H
#ifdef __cplusplus
extern "C" {
#endif

/* What port/src/mods_game.c reads of a fight once per blank (the gauges come from fight_panels.c). Floats are the
   game's own values, copied as they are. */
typedef struct ModsFighter {
    int found;
    float pos[3];  /* world position (the pose block, chr + 0x10); +Y is down */
    float yaw;     /* facing (radians) */
} ModsFighter;

typedef struct ModsGame {
    int battle;    /* a battle's fighters are there */
    int split;     /* split-screen layout */
    ModsFighter f[2];
} ModsGame;

/* One side's camera as BtlCam_UpdateView is about to build it (src/battle/btl_cam.c). */
typedef struct ModsCam {
    int side;
    int cut;       /* a technique's or a transformation's cut, a fixed or a scripted camera: the game's framing */
    float pos[3];  /* camera position */
    float rot[3];  /* Euler angles (radians), as src/sys/vu0_b_c.c Vu0View_SetRotTrans takes them */
    float target[3]; /* the point the fighter camera looks at (ChrCam target, fighter + 0x450) */
} ModsCam;

/* port/src/mods_game.c (the game's side) */
void PortMods_GameTick(void);                 /* once per blank from Port_VBlank (plat_stub.c) */
int PortMods_Read(ModsGame *out);             /* the last blank's reading */
int PortMods_CamGet(ModsCam *out);            /* during Mods_Camera: the camera being built */
void PortMods_CamSet(const ModsCam *cam);     /* ... and its new pose */

/* port/src/gs/mods.c (the scripts; all on the game's thread) */
void Mods_Tick(void);                         /* on_frame and on_hud, once per shown blank */
int Mods_WantCamera(void);                    /* some enabled mod has on_camera */
void Mods_Camera(void);                       /* on_camera for the camera PortMods_CamGet gives */

/* The picture drawn by the mods' on_hud, for the render thread (gs/mods_ui.cpp). Coordinates are in HUD units:
   the game's picture is MODS_HUD_H high and as wide as its shape makes it (ModsHud.w). */
#define MODS_HUD_H 448.0f
enum { MODS_CMD_RECT, MODS_CMD_FILL, MODS_CMD_LINE, MODS_CMD_CIRCLE, MODS_CMD_DISC, MODS_CMD_TEXT };
typedef struct ModsCmd {
    unsigned char kind;
    unsigned char align;   /* text: 0 left, 1 centre, 2 right */
    unsigned short text;   /* text: its length; the bytes follow in ModsHud.chars from `at` */
    unsigned color;        /* 0xRRGGBBAA */
    float a, b, c, d;      /* rect / fill: x y w h; line: x1 y1 x2 y2; circle / disc: x y r; text: x y size */
    float e;               /* line / rect / circle: thickness; fill: rounding */
    unsigned at;
} ModsCmd;

#define MODS_HUD_CMDS 4096
#define MODS_HUD_CHARS 32768
typedef struct ModsHud {
    float w;               /* the picture's width in HUD units when the scripts drew */
    int count;
    unsigned chars;
    ModsCmd cmd[MODS_HUD_CMDS];
    char text[MODS_HUD_CHARS];
} ModsHud;
int Mods_HudRead(ModsHud *out);               /* any thread: a consistent copy; 0 = nothing to draw */

/* The settings window's Mods tab (gs/mods_ui.cpp): the list, guarded by Mods_Lock. */
#define MODS_MAX 32
#define MODS_SETTINGS 24
enum { MODS_SET_TOGGLE, MODS_SET_SLIDER, MODS_SET_CHOICE };
typedef struct ModsSetting {
    char id[32];
    char label[64];
    char tip[160];
    int kind;
    double min, max, step, def;
    int choices;
    char choice[8][32];
    volatile double value;   /* written by the window, read by the scripts */
} ModsSetting;

typedef struct ModsInfo {
    char folder[64];         /* its folder's name: the key of its settings */
    char name[64], version[24], author[64];
    char about[256];
    char error[256];         /* why it stopped ("" = running) */
    int enabled;             /* the player's choice (the window writes it under the lock) */
    int hooks;               /* MODS_HOOK_* it has */
    int settings;
    ModsSetting set[MODS_SETTINGS];
} ModsInfo;
enum { MODS_HOOK_FRAME = 1, MODS_HOOK_HUD = 2, MODS_HOOK_CAMERA = 4 };

void Mods_Lock(void);
void Mods_Unlock(void);
int Mods_Count(void);                         /* under the lock */
ModsInfo *Mods_Info(int i);                   /* under the lock */
const char *Mods_Folder(void);                /* where they are looked for */
void Mods_SettingChanged(void);               /* the window changed a value or a choice: saved at the next blank */
void Mods_ReloadAsk(void);                    /* every script loaded again at the next blank */

#ifdef __cplusplus
}
#endif
#endif
