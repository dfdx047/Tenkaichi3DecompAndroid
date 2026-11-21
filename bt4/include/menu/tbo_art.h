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
    X(TBO_BTN_START, "btn_start") \
    X(TBO_TAG_HUD_PREVIEW, "tag_hud_preview") \
    X(TBO_TAG_HUD_FACES, "tag_hud_faces") \
    X(TBO_KIT_BAR_COMPACT, "kit_bar_compact") \
    X(TBO_KIT_SEL_COMPACT, "kit_sel_compact") \
    X(TBO_RADAR, "radar") \
    X(TBO_DISCORD, "discord") \
    X(TBO_GOHAN, "gohan")                   /* Future Gohan, the host (a film with its transparency: gohan.mpg) */ \
    X(TBO_TM_PANEL, "tm_panel")             /* the hub's Time Machine: its gold body, */ \
    X(TBO_TM_SCREEN, "tm_screen")           /* the green screen of the modes, */ \
    X(TBO_TM_DIAL, "tm_dial")               /* the destination dial (its needle is drawn), */ \
    X(TBO_TM_STAMP, "tm_stamp")             /* "DESTINATION LOCKED!" (port/tools/tbo_time_machine.py), */ \
    X(TBO_TM_NAME, "tm_name")               /* the player's name plate at the top, */ \
    X(TBO_TM_KEY, "tm_key")                 /* a key of the keypads, */ \
    X(TBO_TM_KEY_ON, "tm_key_on")           /* the key under the cursor, */ \
    X(TBO_TD_WIN, "td_win") /* the redesigned screens' pieces (port/tools/tbo_redesign.py) */ \
    X(TBO_TD_TAB, "td_tab")  \
    X(TBO_TD_OPT, "td_opt")  \
    X(TBO_TD_OPT_ON, "td_opt_on")  \
    X(TBO_TD_KEY, "td_key")  \
    X(TBO_TD_KEY_ON, "td_key_on")  \
    X(TBO_TD_CHIP, "td_chip")  \
    X(TBO_TD_CHIP_ON, "td_chip_on")  \
    X(TBO_TD_VAL, "td_val")  \
    X(TBO_TD_VAL_ON, "td_val_on")  \
    X(TBO_TD_BTN, "td_btn")  \
    X(TBO_TD_BOX, "td_box")  \
    X(TBO_TD_PLATE, "td_plate")  \
    X(TBO_TD_BAND, "td_band")  \
    X(TBO_TD_CAPS, "td_caps")  \
    X(TBO_TD_MACHINE, "td_machine")  \
    X(TBO_TD_VIGNETTE, "td_vignette")  \
    X(TBO_STORM_BOLT, "storm_bolt") /* four lightning bolts side by side, 256 x 1024 each (our own drawing): the full power storm (stg_c.c) */ \
    X(TBO_FLAGS, "flags")           /* the countries' flags, Profile and Global Top (port/tools/tbo_flags.py, include/menu/tbo_flags.h) */ \
    X(TBO_CS_PANEL, "cs_panel")     /* BT4's new character select, its pieces (port/tools/cs_select_art.py): a panel, */ \
    X(TBO_CS_LIT, "cs_lit")         /* the chosen one, */ \
    X(TBO_CS_PLAIN, "cs_plain")     /* a plain row, */ \
    X(TBO_CS_CARD, "cs_card")       /* a card, */ \
    X(TBO_CS_PILL, "cs_pill")       /* a button's pill, */ \
    X(TBO_CS_PILL_ON, "cs_pill_on") /* and lit */ \
    X(TBO_HUB_BG, "hub_bg")         /* the Online main screen's new look (port/tools/tbo_hub_art.py): the meadow, */ \
    X(TBO_HUB_ROW, "hub_row")       /* a mode's row of navy glass, */ \
    X(TBO_HUB_ROW_ON, "hub_row_on") /* the chosen one's gold plate, */ \
    X(TBO_HUB_ICONS, "hub_icons")   /* the modes' badges (unlit, lit), */ \
    X(TBO_HUB_LOGO, "hub_logo")     /* the BT4 ONLINE seal, */ \
    X(TBO_HUB_CHIP, "hub_chip")     /* a chip of facts, */ \
    X(TBO_TRUNKS, "trunks")         /* and Trunks (his film trunks.mpg, colours above and how solid below: gs_draw.c) */ \
    X(TBO_HUB_PAGE, "hub_page")     /* the inner pages' backdrop: the meadow dimmed (port/tools/tbo_kit_template.py) */ \
    X(TBO_TD_WIN_GOLD, "td_win_gold") /* the inner pages' pieces (port/tools/tbo_page_art.py): a window with a gold edge, */ \
    X(TBO_TD_CARD_ON, "td_card_on") /* the chosen card, */ \
    X(TBO_TD_CELL, "td_cell")       /* a light cell, */ \
    X(TBO_TD_TILE, "td_tile")       /* a room code's letter */

#define TBO_MAIN_ONLINE_OFF (TBO_ART_COUNT + 16)
#define TBO_MAIN_ONLINE_ON (TBO_ART_COUNT + 17)

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

/* Native artwork for the first-run language selector, decoded from the player's EU disc. */
#define TBO_LANG_BG 0x200
#define TBO_LANG_ARROW_LEFT 0x201
#define TBO_LANG_ARROW_RIGHT 0x202

/* The Profile's avatar (Android): the large portrait of the player's most used character, the character select's own
   (256 x 512; gs/gs_draw.c reads it when the character changes, Port_TboFavWant / Port_TboFavFace). */
#define TBO_FAV_PORTRAIT 0x210
void Port_TboFavWant(int chara);               /* the character whose portrait the picture is to be (-1: none) */
int Port_TboFavFace(int chara, float face[3]); /* 1 when the picture is that character's: its face point and head size */

#endif
