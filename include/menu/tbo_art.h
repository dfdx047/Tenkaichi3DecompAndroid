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
    X(TBO_RADITZ, "raditz")                 /* the one who stands by the screens now (Tb_Hosts), whatever the pose */ \
    X(TBO_RADITZ_BUST, "raditz_bust")       /* him from the waist up (Tb_RaditzBust); moving: tbo/ui/raditz.mpg in its place */ \
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
    X(TBO_BTN_START, "btn_start") \
    X(TBO_DB_SKY, "db_sky")                 /* the Tag Team mod's menus, in the look of Dragon Ball (port/tools/db_assets.py): */ \
    X(TBO_DB_BALLS, "db_balls")             /* the dragon balls: eight on a sheet, 0 to 7 stars */ \
    X(TBO_DB_GLOW, "db_glow")               /* a soft white glow (tinted) */ \
    X(TBO_DB_RAYS, "db_rays")               /* the picture window's rays */ \
    X(TBO_DB_FRAME, "db_frame")             /* its gold frame (nine pieces) */ \
    X(TBO_DB_DIALOG, "db_dialog")           /* the white talk box (nine pieces) */ \
    X(TBO_DB_NIGHT, "db_night")             /* the night behind the mod's menus (the dragon radar's look) */ \
    X(TBO_DB_RADAR, "db_radar")             /* the dragon radar: case, button, green screen (its middle: 512, 588 of 1024 x 1100) */ \
    X(TBO_DB_RING, "db_ring")               /* a thin white ring (tinted): the radar's pulses */ \
    X(TBO_DB_GLASS, "db_glass")             /* dark blue glass (nine pieces): panels and plates */ \
    X(TBO_DB_ARENA, "db_arena")             /* the tournament arena behind the mod's menus (a clip's still; port/tools/tag_menu_art.py) */ \
    X(TBO_DB_GOKU, "db_goku")               /* Goku cut out of his still (RGBA) */ \
    X(TBO_DB_LOGO, "db_logo")               /* the gold TAG TEAM lettering (RGBA) */ \
    X(TBO_DB_PLATE, "db_plate")             /* a list plate, blue (nine pieces) */ \
    X(TBO_DB_PLATE_ON, "db_plate_on")       /* ... orange, under the cursor (nine pieces) */ \
    X(TBO_STORM_BOLT, "storm_bolt")         /* four lightning bolts side by side, 256 x 1024 each (our own drawing): the full power storm (stg_c.c) */ \
    X(MM_BG, "mm_bg")               /* the Mod Manager's pictures (src/menu/port_mmkit.c), in mm_sizes.h's order */ \
    X(MM_BAR, "mm_bar") \
    X(MM_BAR_SEL, "mm_bar_sel") \
    X(MM_TAB, "mm_tab") \
    X(MM_TAB_SEL, "mm_tab_sel") \
    X(MM_BAR_GREY, "mm_bar_grey") \
    X(MM_NAVBAR, "mm_navbar") \
    X(MM_FIELD, "mm_field") \
    X(MM_KEY, "mm_key") \
    X(MM_WINDOW_DARK, "mm_window_dark") \
    X(MM_CARD, "mm_card") \
    X(MM_WINDOW, "mm_window") \
    X(MM_EMBLEM, "mm_emblem") \
    X(MM_PLATFORM, "mm_platform") \
    X(MM_FOLDER, "mm_folder") \
    X(MM_FOLDER_MOD, "mm_folder_mod") \
    X(MM_FOLDER_BATCH, "mm_folder_batch") \
    X(MM_ZIP, "mm_zip") \
    X(MM_KEYCAP, "mm_keycap") \
    X(MM_KEYCAP_SEL, "mm_keycap_sel") \
    X(MM_CELL, "mm_cell") \
    X(MM_UP, "mm_up") \
    X(MM_DOWN, "mm_down") \
    X(MM_LEFT, "mm_left") \
    X(MM_RIGHT, "mm_right") \
    X(MM_BALL, "mm_ball") \
    X(MM_LOCK, "mm_lock") \
    X(MM_PIP, "mm_pip") \
    X(MM_PIP_OFF, "mm_pip_off") \
    X(MM_BG_DUSK, "mm_bg_dusk")     /* the character select's mod grid */ \
    X(ZW_BG, "zw_bg")  /* Z Warriors (src/menu/port_zw.c), in zw_sizes.h's order */ \
    X(ZW_KK_THUMB, "zw_kk_thumb") \
    X(ZW_KK_THINK, "zw_kk_think") \
    X(ZW_KK_LAUGH, "zw_kk_laugh") \
    X(ZW_KK_POINT, "zw_kk_point") \
    X(ZW_EMBLEM, "zw_emblem") \
    X(ZW_PLATFORM, "zw_platform") \
    X(ZW_PORTRAIT, "zw_portrait") \
    X(ZW_HALO, "zw_halo") \
    X(ZW_BAR, "zw_bar") \
    X(ZW_BAR_SEL, "zw_bar_sel") \
    X(ZW_WINDOW, "zw_window") \
    X(ZW_DIALOG, "zw_dialog") \
    X(ZW_PILL, "zw_pill") \
    X(ZW_PILL_STEEL, "zw_pill_steel") \
    X(ZW_FIELD, "zw_field") \
    X(ZW_RING, "zw_ring") \
    X(ZW_RING_SEL, "zw_ring_sel") \
    X(ZW_KEY, "zw_key") \
    X(ZW_KEY_SEL, "zw_key_sel") \
    X(ZW_SECTION, "zw_section") \
    X(ZW_UP, "zw_up") \
    X(ZW_DOWN, "zw_down") \
    X(ZW_SHIN, "zw_shin") \
    X(MENU_BALL, "menu_ball")     /* the main menu's animated icons (port/tools/menu_icon_films.py): Z Warriors', */ \
    X(MENU_RADAR, "menu_radar")   /* ... and Tag Team's */ \
    X(MENU_BALL_STILL, "menu_ball_still")   /* the same two, held still: the row is not the chosen one */ \
    X(MENU_RADAR_STILL, "menu_radar_still") \
    X(ZW_T_PANEL, "zw_t_panel") /* Z Warriors' touch screens (port/tools/zw_template_art.py), in zw_sizes.h's order */ \
    X(ZW_T_SEL, "zw_t_sel") \
    X(ZW_T_STAGE, "zw_t_stage") \
    X(ZW_T_ASK, "zw_t_ask") \
    X(ZW_T_NAVY, "zw_t_navy") \
    X(ZW_T_ORANGE, "zw_t_orange") \
    X(ZW_T_GREEN, "zw_t_green") \
    X(ZW_T_RED, "zw_t_red") \
    X(ZW_T_GREY, "zw_t_grey") \
    X(ZW_T_BAR, "zw_t_bar") \
    X(ZW_T_RING, "zw_t_ring") \
    X(ZW_T_BACK, "zw_t_back") \
    X(ZW_T_CHEV, "zw_t_chev")

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
#define TBO_TEXT_SLOTS 256
/* The last place is not a line: a picture the game made (PortTbo_SetPicture: Z Warriors' texture painting), drawn as
   one picture instead of a rectangle a pixel */
#define TBO_PICTURE (TBO_TEXT_BASE + TBO_TEXT_SLOTS - 1)
int PortTbo_SetPicture(const unsigned int *rgba, int w, int h); /* RGBA, the GS's alpha (0x80 opaque); 1 kept */

/* Native artwork for the first-run language selector, decoded from the player's EU disc. */
#define TBO_LANG_BG 0x200
#define TBO_LANG_ARROW_LEFT 0x201
#define TBO_LANG_ARROW_RIGHT 0x202

#endif
