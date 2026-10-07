/* The names of added stages and songs, put together from the letters of the disc's own name pictures (namefont.c). */
#ifndef PORT_NAMEFONT_H
#define PORT_NAMEFONT_H
#ifdef __cplusplus
extern "C" {
#endif

enum { NF_STAGE, NF_SONG, NF_SONG_LIT, NF_STYLES }; /* stage names; song names; song names while the music list is open */

/* From the game (the stage select's set-up): the addresses of the first texture entry of the three name-sheet
   files, as the game's 32-bit pointers. The sheets are copied; nothing of the game's is changed. */
void Port_NameFontSheets(int stage, int song, int songLit);
/* 1 once the letters are cut (cuts them on the first call after the sheets came). */
int NameFont_Ready(void);
int NameFont_Has(int style, int ch);
/* A name's picture, straight RGBA of *w x *h (64 or 32 high like the disc's rows; 512 wide, or more for a name longer
   than a row), malloc'ed. NULL: not ready, or a character has no letter (then the caller draws plain text). */
unsigned char *NameFont_Compose(int style, const char *text, int *w, int *h);

#ifdef __cplusplus
}
#endif
#endif
