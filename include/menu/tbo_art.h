/* Tenkaichi Budokai Online: its pictures, drawn by the renderer at their own resolution. Each is tbo/ui/<name>.png
   next to the program; the game asks for one by its number (src/menu/port_tbo.c), the renderer reads and keeps it
   (port/src/gs/gs_draw.c, tbo_texture). Plain C, no other headers: both sides include it. The pictures are made by
   port/build/ml/tbo_assets.py from the Flow art in online_design/elements. */
#ifndef TBO_ART_H
#define TBO_ART_H

#define TBO_ART_LIST(X) \
    X(TBO_BG, "bg")                         /* the tournament grounds, 16:9 */ \
    X(TBO_CREST, "crest")                   /* the tournament's crest */ \
    X(TBO_HOSTS_MIC_WAVE, "hosts_mic_wave") /* Goten and Trunks, one picture per pose */ \
    X(TBO_HOSTS_BINOCULARS, "hosts_binoculars") \
    X(TBO_HOSTS_ARMS_CROSSED, "hosts_arms_crossed") \
    X(TBO_HOSTS_PUZZLED, "hosts_puzzled") \
    X(TBO_HOSTS_CHEER, "hosts_cheer") \
    X(TBO_HOSTS_THUMBS_UP, "hosts_thumbs_up") \
    X(TBO_KIT_SEL, "kit_sel")               /* the interface pieces: the chosen bar, */ \
    X(TBO_KIT_BAR, "kit_bar")               /* a bar, */ \
    X(TBO_KIT_DIALOG, "kit_dialog")         /* the hosts' box, */ \
    X(TBO_KIT_WINDOW, "kit_window")         /* a window, */ \
    X(TBO_KIT_GOLD, "kit_gold")             /* the gold capsule, */ \
    X(TBO_KIT_STEEL, "kit_steel")           /* the steel capsule */ \
    X(TBO_BTN_CROSS, "btn_cross")           /* the pad's buttons, in the order of the prompts' kinds */ \
    X(TBO_BTN_SQUARE, "btn_square") \
    X(TBO_BTN_TRIANGLE, "btn_triangle") \
    X(TBO_BTN_LEFTRIGHT, "btn_leftright") \
    X(TBO_BTN_CIRCLE, "btn_circle") \
    X(TBO_BTN_START, "btn_start")

#define TBO_ART_ENUM(id, name) id,
enum { TBO_ART_LIST(TBO_ART_ENUM) TBO_ART_COUNT };
#undef TBO_ART_ENUM

/* The texture format that marks a draw as one of these pictures (a reserved PSM value the GS has no format for and
   the game never uses); the picture's number is in the palette address (CBP), the size given is 1024 x 1024 so
   that UVs 0..1024 cover the whole picture whatever its own size. */
#define TBO_PSM 0x05

/* The lines of letters (port/src/gs/tbo_text.c) are drawn the same way: numbers TBO_TEXT_BASE and on, one per line
   the screens write, the picture made from a TrueType font of the font list. */
#define TBO_TEXT_BASE 0x100
#define TBO_TEXT_SLOTS 128

#endif
