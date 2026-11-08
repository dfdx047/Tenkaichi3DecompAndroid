/* The line between BT2's engine (libbt2.so, made from the game's code, shared outside GitHub) and the part of Dragon Rage
   that does not need that code (libbt2online.so, built by the public CI and carried by the APK): the Online Mode item of the
   main menu, its screens, the opening's music, and later the online play. The engine loads the library when it starts
   (plugin/plugin_host.cpp: dlopen), gives it this table and takes back the one below; neither side sees the other's types.
   Plain C: it is the same on both sides whatever compiler built them.

   A change here that an older side cannot follow raises BT2_PLUGIN_API; the engine then leaves the library out and the menu
   stays the game's. Adding an entry at the END of a table does not need a new number: `size` says how much of it is there. */
#ifndef BT2_PLUGIN_H
#define BT2_PLUGIN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BT2_PLUGIN_API 1

/* One call of the game that the library has taken over (a replaced function): the engine's registers, mirrored. The engine copies
   them in before the library's function runs and out after it, and around every call the library makes into the game. */
typedef struct Bt2Gpr { uint64_t low, high; } Bt2Gpr;
typedef struct Bt2Regs {
    Bt2Gpr r[32];
    uint32_t pc;
    float f[32];
} Bt2Regs;
typedef struct Bt2Call Bt2Call; /* the engine's own record of the call: opaque */
typedef void (*Bt2Hook)(uint8_t *ram, Bt2Regs *regs, Bt2Call *call);

/* What the engine gives the library. */
typedef struct Bt2HostApi {
    uint32_t api, size;
    uint8_t *ram;                    /* the game's main memory (32 MB) */
    const char *disc_image;          /* the player's disc image, a path */
    const char *pictures_dir;        /* the library's pictures (the app's assets, copied: <root>/bt2/user/menu-item) */
    const char *online_dir;          /* <root>/bt2/user: where the app and the library leave each other files */

    /* Takes over a game function: `fn` runs in its place. `slot` (0 to 15; an engine of an earlier version has 8: the others are ignored) names it for call_original. */
    void (*hook)(uint32_t address, Bt2Hook fn, int slot);
    /* The function a slot replaced, run as it was. */
    void (*call_original)(Bt2Call *call, Bt2Regs *regs, int slot);
    /* A game function run as a JAL from `self` would run it (arguments in the registers; the callee returns to `self`). */
    void (*call)(Bt2Call *call, Bt2Regs *regs, uint32_t target, uint32_t self);
    /* The replaced function's return to its caller, at `ra`. */
    void (*ret)(Bt2Call *call, Bt2Regs *regs, uint32_t ra, uint32_t self);

    /* Sound. The percent of the game's own streams on the output (the menu's music silenced, then given back), the master
       volume, and where the library adds its sound to each field's (48 kHz, stereo, interleaved, `samples` values). */
    unsigned (*stream_percent)(void);
    void (*set_stream_percent)(unsigned percent);
    unsigned (*master_percent)(void);
    void (*set_music_mix)(void (*mix)(int16_t *stereo, size_t samples, unsigned master_percent));

    /* The renderer: the frame buffer register the game last drew with (FRAME_1), and a picture of the library's, by slot (what
       Bt2Plugin.pixels gives), drawn over the frame about to be shown. */
    uint64_t (*last_frame)(void);
    void (*overlay_sprite)(int fbp, int fbw, int psm, int width, int height, uint32_t slot,
                           float x0, float y0, float x1, float y1, int alpha);

    void (*log)(const char *line);

    /* Online play (added at the end: an engine that has no such entry has a smaller `size`). A second controller on port 1 of the pad service
       (the game then sees two DualShock 2: the other player's), on or off; set before the game opens its sockets. */
    void (*set_second_pad)(int on);
    /* A session's start-up runs without waiting for the clock (no picture is held back for it either), and with the whole output silent. */
    void (*set_fast_forward)(int on);
    void (*set_master_percent)(unsigned percent);
} Bt2HostApi;

/* What the library gives the engine (the renderer and the app ask for these through the engine). */
typedef struct Bt2Plugin {
    uint32_t api, size;
    /* at every flip, after the game drew: the library's pictures over it (DISPFB1/2 and DISPLAY1/2 as the game left them) */
    void (*overlay)(uint64_t dispfb, uint64_t display);
    /* a picture by slot, RGBA with the GS's alpha (0x80 opaque), malloc'ed for the renderer to take; NULL: none */
    uint32_t *(*pixels)(uint32_t slot, uint32_t *version, uint32_t *w, uint32_t *h);
    uint32_t (*version)(uint32_t slot);
    /* a 512 x 256 sheet the renderer has decoded: the library may write into it; 1: it did */
    int (*art_swap)(unsigned char *rgba, int w, int h);
    /* which of its screens is open, for the app's pieces over the game (0 none) */
    int (*online_state)(void);
    /* Online play (at the end; `size` says whether it is there). Asked once per field by the pad service while a second pad is on: the local
       controller of this field in (buttons active high, 2 bytes little endian, then the four sticks), the two players' controllers out
       (player 1's, player 2's). The library may wait inside the call for the other side's input (the lockstep). 0: no exchange now, the
       engine uses the local controller as player 1's and a neutral player 2. */
    int (*pad_field)(const uint8_t local[6], uint8_t p1[6], uint8_t p2[6]);
} Bt2Plugin;

/* The library's one entry: the table in, the library's table out (NULL: it cannot work with this engine). */
#define BT2_PLUGIN_INIT "bt2_plugin_init"
typedef const Bt2Plugin *(*Bt2PluginInit)(const Bt2HostApi *host);

#ifdef __cplusplus
}
#endif
#endif
