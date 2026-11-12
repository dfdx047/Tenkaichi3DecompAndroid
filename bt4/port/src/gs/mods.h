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
    float zoom;    /* the picture's zoom: 1 the game's, below 1 wider (src/battle/btl_cam.c BtlCam_PortZoom) */
    int collide;   /* the pose given kept out of the stage (btl_cam.c BtlCam_PortCollide); 1 unless a mod says no */
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
enum {
    MODS_CMD_RECT,     /* f0..3 x y w h, f4 thickness, f5 rounding */
    MODS_CMD_FILL,     /* f0..3, f4 rounding */
    MODS_CMD_LINE,     /* f0..3 x1 y1 x2 y2, f4 thickness */
    MODS_CMD_CIRCLE,   /* f0 x, f1 y, f2 radius, f3 thickness */
    MODS_CMD_DISC,     /* f0 x, f1 y, f2 radius */
    MODS_CMD_TEXT,     /* f0 x, f1 y, f2 size, f3 outline size, f4 f5 shadow offset, f6 width to fit in (0: any),
                          f7 lean; color the fill (with MODS_F_GRAD: its top, color4 its bottom), color2 outline,
                          color3 shadow (alpha 0: none); the bytes at text[at], n of them */
    MODS_CMD_POLY,     /* a filled convex polygon: n points at pts[at] */
    MODS_CMD_PATH,     /* a polyline: n points at pts[at], f0 thickness, flags & MODS_F_CLOSED */
    MODS_CMD_GRADIENT, /* f0..3, color .. color4: top-left, top-right, bottom-right, bottom-left */
    MODS_CMD_IMAGE,    /* i0 image (bt3.image), f0..3 rect, f4..7 u0 v0 u1 v1, color tint, flags & MODS_F_FLIP */
    MODS_CMD_PORTRAIT, /* i0 side, i1 member (-1 the active one), i2 band (0 colour, 1 grey, 2 white), f0..3 rect,
                          f4..7 u0 v0 u1 v1 (flags & MODS_F_HEAD: f4 zoom about the head instead), color tint;
                          MODS_F_FACE: f0 f1 is where the face goes and f2 the portrait's width (its height twice
                          that), drawn as a grid inside the window f4..7 (x0 y0 x1 y1), faded out from f8 f9 between
                          the radii f10 and f11 (f11 0: no fade) */
    MODS_CMD_BAND,     /* a strip between two polylines: n points at pts[at] (the top edge) and n after them (the
                          bottom), a colour for each at cols[i0] (top ones, then bottom ones), its edges smoothed */
    MODS_CMD_GLOW,     /* a soft round shade: f0 x, f1 y, f2 radius, color at the middle, color2 half way, fading out */
    MODS_CMD_CLIP,     /* f0..3: what follows is cut to this rectangle (inside the picture) */
    MODS_CMD_UNCLIP,
};
#define MODS_F_CLOSED 1
#define MODS_F_FLIP 2
#define MODS_F_HEAD 4
#define MODS_F_AFTER 8     /* text: x is a gap after the end of the text drawn before it (toward the right) */
#define MODS_F_BEFORE 16   /* ... before its start (toward the left; the text ends there) */
#define MODS_F_BASELINE 32 /* text: y is its baseline, not its top */
#define MODS_F_GRAD 64     /* text: its fill shaded from color (top) to color4 (bottom) */
#define MODS_F_FACE 128    /* portrait: placed by its face, see MODS_CMD_PORTRAIT */

typedef struct ModsCmd {
    unsigned char kind;
    unsigned char align;   /* text: 0 left, 1 centre, 2 right */
    unsigned char font;    /* text: 0 the game's lettering (Ui_FontBold), 1 the window's */
    unsigned char flags;
    unsigned color, color2, color3, color4; /* 0xRRGGBBAA */
    float f[12];
    int i0, i1, i2;
    unsigned at, n;
} ModsCmd;

#define MODS_HUD_CMDS 4096
#define MODS_HUD_CHARS 32768
#define MODS_HUD_POINTS 16384
typedef struct ModsHud {
    float w;               /* the picture's width in HUD units when the scripts drew */
    int count;
    unsigned chars, points, colours;
    ModsCmd cmd[MODS_HUD_CMDS];
    char text[MODS_HUD_CHARS];
    float pts[MODS_HUD_POINTS * 2];
    unsigned cols[MODS_HUD_POINTS];
} ModsHud;
int Mods_HudRead(ModsHud *out);               /* any thread: a consistent copy; 0 = nothing to draw */
int Mods_HudActive(void);                     /* any thread: the scripts drew something at the last blank */
int Mods_HudHidden(void);                     /* any thread: the game's HUD pieces they stand in for (bits: 1 gauges,
                                                 2 timer, 4 combo) */
/* The look of the fight's picture the scripts asked for at the last blank (bt3.look), 0..100 each, 0 = as the game
   draws it; read by the renderer (gs_draw.c) on the game's thread. Several mods asking: the strongest of each.
   shading:      the fighters' two-tone shading ramp redrawn: cool coloured shadows, a soft short edge
   rim:          light along the fighters' silhouettes, in rim_color (0xRRGGBBAA, alpha unused)
   vivid:        the picture's colours livelier, a little more contrast, part of the stage's colour cast taken out
   ambient:      soft shadows in creases and where things meet the ground (from the depth buffer)
   energy:       what adds light (ki blasts, beams, sparks) drawn stronger and more saturated
   energy_light: that light tinting the ground, rocks and fighters round it
   vivid, ambient and energy_light are one pass over the picture before the HUD, drawn only by the renderer of SDL's
   GPU API outside Windows (gs_gpu.c); the others by every renderer. */
typedef struct ModsLook {
    int shading, rim, vivid, ambient, energy, energy_light;
    unsigned rim_color;
} ModsLook;
const ModsLook *Mods_Look(void);

/* Pictures a script asked for (bt3.image): the file of image `id`, and a number that changes when the list is made
   again (a reload), for the render thread's textures. NULL = no such image. */
const char *Mods_ImagePath(int id, char *out, int size);
unsigned Mods_ImageGen(void);

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
