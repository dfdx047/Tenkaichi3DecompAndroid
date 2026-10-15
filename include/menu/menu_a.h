#ifndef MENU_MENU_A_H
#define MENU_MENU_A_H

#include "types.h"

/*
 * Menu overlay DBZP.BIN ("PROGRESS"), 0x334C00..0x339610 (placeholder stem "menu_a"). Four modules:
 *
 *   menu_a.c    0x334C00..0x336A90  MainMenu   the main menu (mode 4)
 *   menu_a_b.c  0x336A90..0x336FC0  Progress_Main, the overlay entry and mode dispatcher
 *   menu_a_c.c  0x336FC0..0x338020  Title      the title screen (mode 1)
 *   menu_a_d.c  0x338020..0x33A360  ModeMenu   the per-mode sub menu (layout in menu_b.h)
 *
 * The overlay is compiled with -G0: nothing is reached through $gp, not even the main executable's small data.
 */

/* ---- Local views of main-executable types (the two Flash headers under include/ conflict). ---- */

/* A movie instance (include/sys/gfxm_c.h has the full layout). */
typedef struct MFlash {
    /* 0x00 */ u8 *data;
    /* 0x04 */ void **tex;
    /* 0x08 */ u32 flags;    /* MFLASH_ */
    /* 0x0C */ u32 trig;
    /* 0x10 */ u32 se;       /* bit n: the movie asked for sound n this tick */
    /* 0x14 */ s32 unk14[6];
} MFlash; /* 0x2C */

#define MFLASH_PAD 2         /* the movie's timeline says input is accepted ("pad" "true") */

/* What Flash_FindLabel fills in. */
typedef struct MFlashRef {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 more;
} MFlashRef;

/* Texture rectangle given to Flash_ClipSetUv. */
typedef struct MFlashUv {
    /* 0x00 */ s32 x0;
    /* 0x04 */ s32 y0;
    /* 0x08 */ s32 x1;
    /* 0x0C */ s32 y1;
    /* 0x10 */ s32 unk10;
} MFlashUv; /* 0x14 */

/* Header of a relocated texture list: 0x40-byte entries at tex. */
typedef struct MTexRes {
    /* 0x00 */ s32 unk0[4];
    /* 0x10 */ u8 *tex;
} MTexRes;

#define MTEX(res, n) ((res)->tex + (n) * 0x40)

/* A section of a pack: header word n is a byte offset, rounded down to a multiple of 4. */
#define MPACK_AT(pack, n) ((u8 *)(pack) + ((((u32 *)(pack))[n] >> 2) << 2))

/* gProgress (0x7FC bytes, main executable): the fields this file uses. */
typedef struct MenuProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;      /* 0x1C1: file id of the first menu archive */
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;         /* MPROG_ */
    /* 0x018 */ s32 mode;          /* the screen Progress_Main dispatches to (1..70) */
    /* 0x01C */ s32 prevMode;      /* guess: tested for 8 / 10 by ModeMenu_Init */
    /* 0x020 */ s32 unk20;
    /* 0x024 */ s32 unk24;
    /* 0x028 */ s32 demoCount;     /* attract demos shown since the opening movie (0..2) */
    /* 0x02C */ s32 mainMenuItem;  /* item id the main menu was left on */
    /* 0x030 */ s32 unk30;
    /* 0x034 */ s32 subMenu;       /* which per-mode sub menu ModeMenu shows (0..7) */
    /* 0x038 */ s32 subMenuItem;   /* item the sub menu was left on */
    /* 0x03C */ u8 unk3C[0x5E8];
    /* 0x624 */ s32 unk624;
    /* 0x628 */ u8 unk628[0x64];
    /* 0x68C */ s32 unk68C;
} MenuProgress;

#define MPROG_NO_SAVE 4        /* "continue without saving" */
#define MPROG_SAVE_LOADED 8    /* a save was loaded at boot (or the title was left without a card flow) */
#define MPROG_FLAG10 0x10
#define MPROG_FLAG20 0x20
#define MPROG_FIRST_RUN 0x40   /* set by Progress_Init: the overlay has not run yet */
#define MPROG_FREEZE 0x100     /* the screens stop advancing and reading input */

/* ---- Main executable ---- */

extern s32 sprintf(char *, const char *, ...);
extern void *memset(void *, s32, u32);
extern void *File_LoadSync(s32 id, void *buf, s32 unused);
extern void *Heap_Alloc(s32 size, u32 align, s32 fromTail, s32 heap);
extern void Heap_Free(void *ptr);
extern void *Sprite_Unpack(void *src, void *dst, s32 *rawSize);
extern void Sprite_DrawPicture(void *res, s32 x, s32 y, s32 alpha);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern u32 Rand_Range(u32 n);
extern void Flash_Create(MFlash *flash, void *data, void *tex);
extern void Flash_Destroy(MFlash *flash);
extern void Flash_Advance(MFlash *flash);
extern void Flash_Draw(MFlash *flash);
extern void Flash_Play(MFlash *flash, s32 speed);
extern void Flash_GotoLabel(MFlash *flash, char *label, s32 fromStart);
extern void Flash_FindLabel(MFlash *flash, char *parent, char *name, MFlashRef *out);
extern void Flash_ClipGotoLabel(MFlash *flash, MFlashRef *ref, char *label);
extern void Flash_ClipSetFlags(MFlash *flash, MFlashRef *ref, s32 props, s32 on);
extern void Flash_ClipSetUv(MFlash *flash, MFlashRef *ref, MFlashUv *uv);
extern void Flash_ClipSetTex(MFlash *flash, MFlashRef *ref, s32 tex);
extern void FlashAnim_Blink(MFlash *flash, MFlashRef *ref, s32 *timer, s32 frame);
extern void FlashAnim_Talk(MFlash *flash, MFlashRef *ref, s32 *timer, s32 frame);
extern void FlashAnim_ShowNext2(MFlash *flash, MFlashRef *ref, s32 frame);
extern void FlashAnim_Sheet(MFlash *flash, MFlashRef *ref, s32 *timer, s32 *frame, MFlashUv *uv, s32 cols, s32 rows,
                            s32 period);
extern void FlashAnim_Scroll(MFlash *flash, MFlashRef *ref, MFlashUv *uv, f32 *x, f32 *y, f32 dx, f32 dy);
extern void MsgWin_Init(void *pack, void *text, s32 side, s32 unused);
extern void MsgWin_Term(void);
extern void MsgWin_Draw(s32 unused0, s32 unused1, s32 line);
extern void MsgWin_Open(void);
extern void MsgWin_Close(void);
extern void IconWin_Init(void *pack, void *icons);
extern void IconWin_Open(void);
extern void IconWin_Close(void);
extern void IconWin_Draw(void);
extern void IconWin_Term(void);
extern s32 Voice_GetStat(s32 voice);
extern void Voice_PlayWithSubtitle(void *subtitles, s32 base, s32 line);
extern void Voice_FadeOutStep(s32 voice);
extern void Bgm_FadeOutStep(void);
extern s32 Snd_PlaySe(u32 mask, s32 id);
extern void Snd_Update(void);
extern void Pad_Update(void);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 vsyncs);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void File_Stub264D90(void);
extern s32 sceGsSyncPath(s32 mode, u16 timeout);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern s32 ColorFade_IsInDone(void);
extern s32 ColorFade_IsOutDone(void);
extern s32 ColorFade_IsFadingOut(void);
extern void Flash_ClipSetAlpha(MFlash *flash, MFlashRef *ref, f32 alpha);
extern void Flash_ClipSetColor(MFlash *flash, MFlashRef *ref, f32 color);
extern void Dialog_Init(void *file, void *msgTbl, s32 size);
extern void Dialog_Term(void);
extern void McFlow_Init(s32 unused);   /* takes no argument; the title passes 1 */
extern void McFlow_Term(void);
extern void McFlow_Start(s32 mode);
extern s32 McFlow_Update(void);
extern void McFlow_SetDoneCb(s32 idx, void *cb, void *arg);
extern void Demo_SetupBattle(void);    /* main 0x2617F0: sets up the attract-demo battle */
extern void Pad_SetRepeat(s32 delay, s32 interval);
extern void PadWatch_SetEnabled(s32 enable);
extern s32 BattleResult_GetFlags(void);
extern s32 BattleResult_GetReason(void);
extern void Snd_LoadBankFile(u32 mask, s32 fileId);
extern void Snd_UnloadBank(u32 mask);
extern void Movie_PlayOpening(void);
extern void Bgm_Play(s32 id);
extern void Adx_StopAll(void);
extern void StreamSe_PlayDefault(s32 se, s32 id);
extern void File_CancelRequests(void);
extern void *File_Request(s32 id, void *buf, s32 size);
extern s32 File_UpdateRequests(void);
extern void Font_FlushAll(void);
extern void Shen_Main(void);           /* main 0x2BD230: the dragon-summoning mode (70) */

extern MenuProgress *gProgress;

/* ---- MainMenu (menu_a.c) ---- */

#ifdef PORT
#define MAINMENU_ITEM_MAX 12 /* PC: "Tenkaichi Budokai Online" (11) is listed too */
#else
#define MAINMENU_ITEM_MAX 11
#endif
#define MAINMENU_FLASH_NUM 1    /* the code loops over the movies although there is one: see menu_a.c */
#define MAINMENU_ROWS 4        /* plates the cursor can be on */

typedef struct MainMenu {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 1 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 29 sections */
    /* 0x008 */ void *msgText;      /* section 28: text of the message window */
    /* 0x00C */ void *subtitles;    /* section 27: subtitles of the guide voices */
    /* 0x010 */ MFlash flash[MAINMENU_FLASH_NUM];
    /* 0x03C */ void *bg;           /* section 1: background picture */
    /* 0x040 */ u8 *tex[50];        /* textures of the movie's image records */
    /* 0x108 */ s32 flags;          /* MAINMENU_ */
    /* 0x10C */ s32 cursor;         /* row the cursor is on, 0..3 */
    /* 0x110 */ s32 timer;          /* frames until the fade out starts after a choice */
    /* 0x114 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x118 */ s32 items[MAINMENU_ITEM_MAX]; /* item ids in list order */
    /* 0x144 */ s32 itemCount;
    /* 0x148 */ s32 top;            /* list index of the plate above the first row (the list is a ring) */
    /* 0x14C */ s32 unk14C;
    /* 0x150 */ s32 extra;          /* list index shown on the seventh plate while the list scrolls */
    /* 0x154 */ s32 iconTimer;
    /* 0x158 */ s32 iconFrame;
    /* 0x15C */ s32 guide;          /* which of the four guide characters is talking */
    /* 0x160 */ s32 blink[4];
    /* 0x170 */ s32 talk[4];
    /* 0x180 */ f32 cloud[2];
} MainMenu; /* 0x188 */

#define MAINMENU_CHOSEN 1
#define MAINMENU_LEAVING 2
#define MAINMENU_STARTED 4     /* the cursor plate was lit once */
#define MAINMENU_GREETED 8     /* the greeting voice was started */

/* ---- Title (menu_a_c.c) ---- */

#define TITLE_FLASH_NUM 1

typedef struct Title {
    /* 0x00 */ u32 *pack;
    /* 0x04 */ u32 *res;            /* a pack of 15 sections */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ MFlash flash[TITLE_FLASH_NUM];
    /* 0x38 */ void *bg;            /* section 1: background picture */
    /* 0x3C */ u8 *tex[26];
    /* 0xA4 */ void *logo;          /* section 14: the picture shown for 300 frames first */
    /* 0xA8 */ s32 flags;           /* TITLE_ */
    /* 0xAC */ s32 cursor;
    /* 0xB0 */ s32 timer;
    /* 0xB4 */ s32 itemCount;       /* 1 = "new game" only, 2 = with "continue" */
    /* 0xB8 */ s32 state;           /* 0 intro, 1 "press start", 2 menu */
    /* 0xBC */ s32 mcState;         /* McFlow_Update result: non-zero while a card flow runs */
    /* 0xC0 */ s32 voice;           /* which of six title calls (Rand_Range(6)) */
    /* 0xC4 */ s32 idle;            /* frames without input */
    /* 0xC8 */ f32 cloud[2];
} Title; /* 0xD0 */

#define TITLE_CHOSEN 1
#define TITLE_LEAVING 2
#define TITLE_STARTED 4
#define TITLE_FADED_IN 8

#define TITLE_IDLE_FRAMES 0x708   /* 1800 frames until the attract demo */

/*
 * The overlay's first 0x40 bytes of data (0x3B0E80): the two work pointers of this file's screens and one
 * archive pointer per group of modes. Each archive is loaded on demand by its own screens and freed by
 * Progress_Main when the overlay returns to the main executable.
 */
extern MainMenu *gMainMenu; /* 0x3B0E80 */
extern void *gMenuArc0;     /* 0x3B0E84: FirstRun_Main (first run) */
extern void *gMenuArc1;     /* 0x3B0E88: file baseFile + 1: title and main menu */
extern void *gMenuArc2;     /* 0x3B0E8C: modes 6-10 (Hist_Main) and ModeMenu */
extern void *gMenuArc3;     /* 0x3B0E90: modes 13-30 (Ub_Main) */
extern void *gMenuArc4;     /* 0x3B0E94: modes 33-35 (Tour_Main) */
extern void *gMenuArc5;     /* 0x3B0E98: modes 38-41 (Duel_Main) */
extern void *gMenuArc6;     /* 0x3B0E9C: modes 44-45 (TrainMode_Main) */
extern void *gMenuArc7;     /* 0x3B0EA0: modes 48-50 (EvoMode_Main) */
extern void *gMenuArc8;     /* 0x3B0EA4: modes 53-56 (Dc_Main) */
extern void *gMenuArc9;     /* 0x3B0EA8: mode 60 (CharRefMode_Main) */
extern void *gMenuArc10;    /* 0x3B0EAC: mode 62 (OptMode_Main) */
extern void *gMenuArc11;    /* 0x3B0EB0: only freed here; no loader found in this range */
extern void *gMenuArc12;    /* 0x3B0EB4: file baseFile + 0x18, the wish screen (src/sys/late_a.c, main executable) */
extern Title *gTitle;       /* 0x3B0EB8 */

/* ---- ModeMenu (menu_a_d.c): the sub menu of one game mode, with a guide character and a large picture ---- */

/* A text box of the main executable (include/battle/view_a.h has the layout). */
typedef struct MTextBox {
    u8 unk0[0x8C];
} MTextBox;

/* The work struct (ModeMenu) and its item table are in include/menu/menu_b.h (the object was first decompiled in
   two halves; the head's partial view that stood here is gone). */
#define MODEMENU_FLASH_NUM 1
#define MODEMENU_ITEM_MAX 16
#define MODEMENU_ROWS 3        /* plates visible at once */

#define MODEMENU_NEXT 0x20            /* shows "mc_episode_next" */
#define MODEMENU_IMAGE_CHANGE 0x40    /* the current item changed: load its picture */
#define MODEMENU_IMAGE_READY 0x80     /* the picture is loaded and fades in */

/* loadState: the large picture's loader, one step per frame */
#define MODEMENU_LOAD_REQUEST 1
#define MODEMENU_LOAD_READ 2
#define MODEMENU_LOAD_UNPACK 3
#define MODEMENU_LOAD_SHOWN 4
#define MODEMENU_LOAD_ABORT 5
#define MODEMENU_LOAD_RESTART 6


s32 MainMenu_Run(s32 section);
s32 Title_Run(s32 section);
s32 Progress_Main(s32 arg);

#include "menu/port_screen.h" /* (PC only: the port's own screens) */

#endif
