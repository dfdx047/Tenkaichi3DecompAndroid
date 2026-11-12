#ifndef MENU_MENU_B_H
#define MENU_MENU_B_H

/*
 * Menu overlay DBZP.BIN, 0x339610..0x33E108 (placeholder stem "menu_b").
 *
 *   (menu_b.c   0x339610..0x33A360  ModeMenu   tail of the object; now merged into menu_a_d.c, 0x338020..0x33A360)
 *   menu_b_b.c  0x33A360..0x33ABA0  ModeBg     the animated background shared by ModeMenu and HistOutro
 *   menu_b_c.c  0x33ABA0..0x33CFC8  HistOutro  the scene after a completed saga (mode 9), and Hist_Main, the
 *                                              handler of progress modes 6..10 (the story mode)
 *   menu_b_d.c  0x33CFC8..0x33F700  HistSel    the saga select (mode 6), merged with its tail (the former menu_c.c);
 *                                              it includes menu/menu_c.h, not this header
 *
 * ModeMenu is the story mode's episode list (mode 7): gProgress->subMenu is the saga (0..7), an item an episode.
 */
#include "menu/menu_a.h"

/* ---- ModeMenu (menu_a_d.c) ---- */

/* One entry per item in section 6 of the sub menu's own archive (menu_a.h has the head's view, ModeMenuText). */
typedef struct ModeMenuDescr {
    /* 0x00 */ s32 count;      /* description lines (up to 3) */
    /* 0x04 */ s32 line;       /* first line in the text file, and the first narration voice line */
    /* 0x08 */ s32 delay[6];   /* frames to wait before line n is read out */
} ModeMenuDescr; /* 0x20 */

typedef struct ModeMenu {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 2 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked */
    /* 0x008 */ void *file;         /* file baseFile + 0x10 + subMenu (compressed) */
    /* 0x00C */ u32 *fileRes;       /* the same unpacked: this sub menu's own pack */
    /* 0x010 */ void *imageFile;    /* 0x1C000 bytes: the compressed large picture of the current item */
    /* 0x014 */ MTexRes *imageRes;  /* 0x20800 bytes: the same unpacked */
    /* 0x018 */ void *msgText;      /* fileRes section 4 */
    /* 0x01C */ void *subtitles;    /* fileRes section 5 */
    /* 0x020 */ void *text;         /* res section 6: text of the three description lines */
    /* 0x024 */ MFlash flash[MODEMENU_FLASH_NUM];
    /* 0x050 */ u8 *tex[41];
    /* 0x0F4 */ s32 flags;          /* MODEMENU_ */
    /* 0x0F8 */ s32 cursor[2];      /* indexed with `focus`; only [0], the index into items, is ever used */
    /* 0x100 */ s32 timer;          /* frames until the fade out starts after the battle was confirmed */
    /* 0x104 */ s32 focus;          /* 0 = the list has the pad, 1 = the description of the chosen item is shown */
    /* 0x108 */ s32 voiceBase;      /* first voice id of this sub menu's guide */
    /* 0x10C */ s32 lineBase;       /* first subtitle line of this sub menu's guide */
    /* 0x110 */ s32 imageBase;      /* file id of item 0's large picture */
    /* 0x114 */ s32 step;           /* MODEMENU_STEP_: the sequence after an item was chosen, 0 = idle */
    /* 0x118 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x11C */ s32 blink;
    /* 0x120 */ s32 talk;
    /* 0x124 */ s32 unk124;         /* cleared at every pad action, never read in this object */
    /* 0x128 */ s32 items[MODEMENU_ITEM_MAX]; /* unlocked item ids; padded with itemMax up to three */
    /* 0x168 */ s32 itemCount;
    /* 0x16C */ s32 top;            /* first visible list index */
    /* 0x170 */ s32 bottom;         /* last visible list index (top + 2) */
    /* 0x174 */ s32 extra;          /* list index shown on plates 4 and 5 while the list scrolls */
    /* 0x178 */ s32 itemMax;        /* items this sub menu has */
    /* 0x17C */ f32 scroll;         /* vertical offset of the description text */
    /* 0x180 */ f32 scrollMax;      /* where the description stops: lines * 40 + 160 */
    /* 0x184 */ s32 narrWait;       /* frames waited before the current description line is read out */
    /* 0x188 */ s32 narrLine;       /* description line being read out */
    /* 0x18C */ s32 loadState;      /* MODEMENU_LOAD_ */
    /* 0x190 */ f32 imageAlpha;
    /* 0x194 */ MTextBox box[3];
    /* 0x338 */ ModeMenuDescr *descr; /* fileRes section 6 */
} ModeMenu; /* 0x33C */

#define MODEMENU_CHOSEN 1
#define MODEMENU_LEAVING 2
#define MODEMENU_STARTED 4            /* the cursor plate was lit once */
#define MODEMENU_GREETED 8            /* the greeting voice was started */
#define MODEMENU_SCROLLING 0x10       /* the description scrolls in */

#define MODEMENU_STEP_GUIDE 1         /* the guide comments the choice */
#define MODEMENU_STEP_GUIDE_WAIT 2
#define MODEMENU_STEP_OPEN 3          /* the movie opens the description */
#define MODEMENU_STEP_OPENED 4
#define MODEMENU_STEP_NARR_INIT 51
#define MODEMENU_STEP_NARR_START 52   /* waits for the movie's trigger 1, then scrolls the text in */
#define MODEMENU_STEP_NARR_LINE 53    /* waits the line's delay and starts its narration */
#define MODEMENU_STEP_NARR_WAIT 54    /* narration running: next line, start or cancel */
#define MODEMENU_STEP_NARR_END 55
#define MODEMENU_STEP_GO 101          /* start pressed during the narration: leave for the battle */

#define MODEMENU_NARR_VOICE 0x84B6    /* voice base of the narrated description lines */

extern ModeMenu *gModeMenu; /* 0x3B12F0 */

void ModeMenu_UpdateImage(void);
void ModeMenu_ChangeImage(void);
void ModeMenu_Init(s32 section);
void ModeMenu_Term(void);
void ModeMenu_Draw(void);
s32 ModeMenu_GetLine(s32 menu, s32 item);
s32 ModeMenu_Run(s32 section);

/* ---- ModeBg (menu_b_b.c) ---- */

typedef struct ModeBg {
    /* 0x00 */ MFlash flash[1];     /* section 9 of the sub menu's pack; not created for sub menu 3 */
    /* 0x2C */ void *bg;            /* section 1: the still picture */
    /* 0x30 */ u8 *tex[14];         /* textures of section 10, in the order the movie wants them */
    /* 0x68 */ f32 scrollX[2];      /* horizontal offset of the cloud layers ("mc_kumo") */
    /* 0x70 */ f32 scrollY[2];      /* vertical offset of the smoke layers ("mc_kemuri") */
} ModeBg; /* 0x78 */

extern ModeBg *gModeBg; /* 0x3B12F4 */

void ModeBg_Init(u32 *pack);
void ModeBg_Term(void);
void ModeBg_Draw(void);

/* ---- HistOutro (menu_b_c.c): the scene after a completed saga ---- */

#define HISTOUTRO_FLASH_NUM 1

typedef struct HistOutro {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 2 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked */
    /* 0x008 */ void *file;         /* file baseFile + 0x10 + subMenu (compressed) */
    /* 0x00C */ u32 *fileRes;       /* the same unpacked: the saga's own pack */
    /* 0x010 */ void *msgText;      /* fileRes section 4 */
    /* 0x014 */ void *subtitles;    /* fileRes section 5 */
    /* 0x018 */ void *text;         /* res section 6 */
    /* 0x01C */ MFlash flash[HISTOUTRO_FLASH_NUM];
    /* 0x048 */ u8 *tex[32];        /* 1 + 3 * n: the three face textures of guide n (0..8) */
    /* 0x0C8 */ s32 flags;          /* HISTOUTRO_ */
    /* 0x0CC */ s32 unkCC;
    /* 0x0D0 */ s32 timer;          /* frames until the fade out starts */
    /* 0x0D4 */ s32 focus;          /* never written: the pad is read only while it is 0 (ModeMenu's focus) */
    /* 0x0D8 */ s32 voiceBase;      /* first voice id of the saga's guide */
    /* 0x0DC */ s32 lineBase;       /* first subtitle line of this dialogue */
    /* 0x0E0 */ s32 step;           /* position in the script, 0 = ended */
    /* 0x0E4 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x0E8 */ s32 partner;        /* guide id (0..8) of the second character */
    /* 0x0EC */ s32 speaker;        /* who moves the mouth: 0 the saga's guide, 1 the partner */
    /* 0x0F0 */ s32 blink[2];
    /* 0x0F8 */ s32 talk[2];
    /* 0x100 */ s32 unk100;
    /* 0x104 */ s32 itemMax;        /* episodes of the saga = index of the closing text in descr */
    /* 0x108 */ f32 scroll;
    /* 0x10C */ f32 scrollMax;      /* lines * 40 + 230 */
    /* 0x110 */ s32 narrWait;
    /* 0x114 */ s32 narrLine;
    /* 0x118 */ MTextBox box[3];
    /* 0x2BC */ ModeMenuDescr *descr; /* fileRes section 6 */
} HistOutro; /* 0x2C0 */

#define HISTOUTRO_CHOSEN 1
#define HISTOUTRO_LEAVING 2
#define HISTOUTRO_STARTED 4       /* the movie's intro has ended (the guides were sent in) */
#define HISTOUTRO_BEGUN 8         /* the script was started */
#define HISTOUTRO_SCROLLING 0x10

#define OUTRO_STEP_CLOSE 51       /* the dialogue is over: close the message window */
#define OUTRO_STEP_TEXT 501       /* the closing text */
#define OUTRO_STEP_GO 551         /* start pressed during the narration */

/* SaveSlot.flags bits of a saga (slot = saga) */
#define SAVESLOT_OUTRO_SEEN 4     /* HistOutro_Init ran for this saga */
#define SAVESLOT_OUTRO_STARTED 0x10 /* its script was started */
#define SAVESLOT_GREET_FIRST 0x40 /* ModeMenu greets with line lineBase + 3 once, then clears it */

extern HistOutro *gHistOutro; /* 0x3B12F8 */

void HistOutro_GuideGoto(s32 movie, s32 side, s32 out);
s32 HistOutro_Run(s32 section);
s32 Hist_Main(void);

/* ---- HistSel (menu_b_d.c): the saga select of the story mode. Its work struct, flags and functions are in
   include/menu/menu_c.h (the full layout; the head-only view that was here is gone). ---- */

#endif
