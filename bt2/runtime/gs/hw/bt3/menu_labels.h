/* The list of menu labels of the "Menu font" setting, made from the player's own disc data (menu_labels.c). */
#ifndef GS_MENU_LABELS_H
#define GS_MENU_LABELS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Header of the current labels.tsv layout. A list without this version is made again. */
#define MENU_LABELS_VERSION "BT3 menu text v12:"

enum { MENU_LABELS_MADE = 0, MENU_LABELS_OTHER_DISC = 1, MENU_LABELS_FAILED = -1 };

/* Reads the HUD and menu archives in `dataDir` (pzs3us1/00005.bin, 00448.bin ...) and writes labels.tsv and name-styles.tsv (and
   labels.json, the form to edit, unless one is there) into `outDir`, each whole or not at all. `anyDisc`: the
   archives are not checked to be the USA release's (testing; a changed archive). Returns one of the values above;
   `why` gets one line: how many labels, or what kept the list from being made. Takes a few tenths of a second, reads
   nothing of the game's memory, and may run in a thread of its own; not twice at a time. */
int MenuLabels_Make(const char *dataDir, const char *outDir, int anyDisc, char *why, size_t whyLen);

/* The label of the PC's own item of the main menu (src/menu/menu_a.c), in the lettering of the game's:
   "Tenkaichi Budokai Online" put together from the letters of the main menu's labels in `dataDir` (and of the
   stage select's names, for the ones they lack), as two straight RGBA pictures of 512 x 32 (8-bit alpha, as
   NameFont_Compose's), the item as it is
   and while chosen, malloc'ed into out[0] and out[1]. 0 (both NULL): not the USA release's archives, or a letter
   none of them has. Files only, a few tenths of a second; may run in a thread of its own. */
int MenuLabels_MainItem(const char *dataDir, unsigned char *out[2]);
/* What the label sheet's pixels hold for such a picture (512 x 32 values into `out`): each pixel the colour of
   `table` (the sheet's 256 colours, in the order they are in memory) nearest to it. The game's write and the list of
   labels (which finds the sheet by its pixels) both use this, so the two agree. */
void MenuLabels_ItemPixels(const unsigned char *pic, const unsigned char *table, unsigned char *out);

/* The European disc's menu art in another language: `bank` 1 Spanish, 2 German, 3 French, 4 Italian. The setup
   (or port/tools/extract_eu.py) has put the European disc's menu and HUD archives into `dataDir`/pzs3eu1; every
   texture file of them that the language paints otherwise is paired with the USA one in `dataDir`/pzs3us1 by the
   hash of the decoded USA pixels, the one the renderer sees. Returns how many textures, or -1 (`why` says why);
   once, in a thread of its own (a second or two). Then MenuLabels_ArtSwap puts the European picture into a decoded
   texture that is one of them (1), and MenuLabels_ArtKey is no longer 0 (the renderer's cache key: textures decoded
   before are decoded again). */
int MenuLabels_Art(const char *dataDir, int bank, char *why, size_t whyLen);
int MenuLabels_ArtSwap(unsigned char *rgba, int w, int h);
unsigned MenuLabels_ArtKey(void);

#ifdef __cplusplus
}
#endif
#endif
