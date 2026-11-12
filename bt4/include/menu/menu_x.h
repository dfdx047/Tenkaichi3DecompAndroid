#ifndef MENU_MENU_X_H
#define MENU_MENU_X_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (the code after a call shows it). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x39EFC0..0x3A3848 (placeholder stem "menu_x"). Three pieces:
 *
 *   (menu_x.c)  0x39EFC0..0x39FAA8  EvoTop      tail of the top menu of Evolution Z, mode 48: now merged into
 *                                               src/menu/menu_w_c.c (0x39EB08..0x39FAA8), which uses the
 *                                               EvoTop layout below
 *
 *   menu_x_b.c  0x39FAA8..0x39FBB8  OptMode_Main, the handler of mode 62 (the options)
 *   menu_x_c.c  0x39FBB8..0x3A65E8  Option      the options screen object (menu_y.c, its tail, was merged in;
 *                                               include/menu/menu_y.h has the prototypes that half added)
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void StreamSe_FadeOutStep(s32 se);
extern void Bgm_Stop(void);
extern void Bgm_SetVolume(s32 volume);
extern void SndOpt_Apply(void);                       /* Bgm_SetVolume(0x40) + Snd_SetStereo(soundMode ^ 1) */
extern void BgmList_ApplyUnlocks(s32 *count, s32 *ids);

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* Pad.gamePressed / gameRepeat / gameHeld bits as the menus use them (sys/pad.h has the PADG_ names). */
#define MPAD_LEFT 1
#define MPAD_RIGHT 2
#define MPAD_DOWN 4
#define MPAD_UP 8
#define MPAD_OK 0x200
#define MPAD_CANCEL 0x400
#define MPAD_START 0x1000

/* ---- The overlay's "save on leaving" helper (0x3B0C08..0x3B0E04, chunk menu_za: DcSave) ---- */

extern void DcSave_Init(void *dialogPack);   /* Dialog_Init + McFlow_Init */
extern void DcSave_Term(void);
extern void DcSave_Start(void);              /* McFlow_Start(0) */
extern void DcSave_Update(void);             /* per frame: McFlow_Update while started */
extern s32 DcSave_GetState(void);            /* McFlow_Update's last result while started, else 0 */
extern s32 DcSave_IsDone(void);              /* 1 when not started, else whether the flow has ended */
extern s32 DcSave_IsStarted(void);

/* ---- EvoTop (menu_w_c.c): the top menu of Evolution Z (mode 48), guide Krillin ---- */

/* gProgress + 0x7D0: the plate the Evolution Z top menu was left on (-1 when the mode is entered). */
#ifndef EVO_PROGRESS_CURSOR
#define EVO_PROGRESS_CURSOR (*(s32 *)((u8 *)gProgress + 0x7D0))
#endif

#define EVOTOP_FLASH_NUM 1

typedef struct EvoTop {
    /* 0x00 */ u32 *pack;           /* this screen's section of archive 7 (compressed) */
    /* 0x04 */ u32 *res;            /* the same unpacked */
    /* 0x08 */ MFlash flash[EVOTOP_FLASH_NUM];
    /* 0x34 */ void *bg;            /* background picture */
    /* 0x38 */ u8 *tex[13];
    /* 0x6C */ s32 blink;
    /* 0x70 */ s32 talk;
    /* 0x74 */ void *subtitles;
    /* 0x78 */ void *msgText;
    /* 0x7C */ u8 unk7C[0x3C];      /* handed to MsgWin_Init, which ignores it */
    /* 0xB8 */ s32 cursor;          /* plate 0..2 */
    /* 0xBC */ s32 voiceLine;       /* guide line being said / shown, -1 = none */
    /* 0xC0 */ s32 result;          /* 0 = back, 1 / 2 = the plate chosen */
    /* 0xC4 */ s32 timer;           /* idle frames; after the choice, frames until the screen is left */
    /* 0xC8 */ s32 flags;           /* EVOTOP_ */
    /* 0xCC */ f32 cloud;
} EvoTop; /* 0xD0 */

#define EVOTOP_CHOSEN 1
#define EVOTOP_LEAVING 2
#define EVOTOP_STARTED 4       /* "fl_in" was started */
#define EVOTOP_GREETED 8       /* the first voice line was started */
#define EVOTOP_EXPLAIN 0x20    /* plate 2: the guide's explanation is running */

#define EVOTOP_VOICE_BASE 0x86C9
#define EVOTOP_IDLE_FRAMES 0xE10 /* 3600 frames until the guide's idle line */
#define EVOTOP_LINE_EXPLAIN_FIRST 4
#define EVOTOP_LINE_EXPLAIN_LAST 0x10
#define EVOTOP_LINE_IDLE 0x11

/* src/menu/menu_w_c.c */
void EvoTop_PlayVoice(EvoTop *m, s32 line);          /* plays a guide line and shows it */
s32 EvoTop_Wrap(s32 value, s32 min, s32 max);        /* wraps value into min..max */
void EvoTop_SetPlate(EvoTop *m, s32 on);             /* lights / dims the cursor plate */
void EvoTop_SetPlateText(EvoTop *m);                 /* sets the plates' text rectangles */
void EvoTop_Advance(EvoTop *m);                      /* advances the movie */
void EvoTop_Init(EvoTop *m, s32 section);

/* ---- Option (menu_x_c.c): the options screen, mode 62 ---- */

#define OPTION_FLASH_NUM 1
#define OPTION_PAD_NUM 2
#define OPTION_KEY_NUM 8

typedef struct Option {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 10 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 39 sections */
    /* 0x008 */ MFlash flash[OPTION_FLASH_NUM];
    /* 0x034 */ void *bg;           /* section 30: background picture */
    /* 0x038 */ u8 *tex[51];
    /* 0x104 */ s32 cursor;         /* OPT_ITEM_: the row the cursor is on */
    /* 0x108 */ s32 state;          /* OPT_ST_ */
    /* 0x10C */ s32 page;           /* 0 top, 1 screen, 2 sound, 3 controller */
    /* 0x110 */ s32 bgmVolume;      /* the save's volumes when the volume picker was opened / last confirmed */
    /* 0x114 */ s32 seVolume;
    /* 0x118 */ s32 bgmCursor;      /* sound test: index into bgmIds */
    /* 0x11C */ s32 bgmTop;         /* first visible row */
    /* 0x120 */ s32 bgmBottom;      /* one past the last visible row (top + 6) */
    /* 0x124 */ s32 bgmExtra;       /* row that scrolls out */
    /* 0x128 */ s32 value;          /* column of a two-way picker, or volume row (0 music, 1 effects) */
    /* 0x12C */ s32 voiceLine;      /* guide line being said, also the subtitle shown */
    /* 0x130 */ s32 picker;         /* which captions the two-way picker shows; 7 and up = the volume window */
    /* 0x134 */ s32 unk134;
    /* 0x138 */ s32 pad;            /* controller page: the player being edited (0 / 1) */
    /* 0x13C */ s32 fromPage;       /* how the top page was reached again (which rows the page lists) */
    /* 0x140 */ s32 ctrlKind;       /* 0 vibration, 1 key assignment */
    /* 0x144 */ s32 screenX;        /* the save's screen position when the adjust page was opened */
    /* 0x148 */ s32 screenY;
    /* 0x14C */ s32 type;           /* controller type shown (0..2) */
    /* 0x150 */ s32 typePrev;       /* the type scrolling out */
    /* 0x154 */ s32 blink;          /* guide's eyes */
    /* 0x158 */ s32 talk;           /* guide's mouth */
    /* 0x15C */ s32 resetStep;      /* step of Option_UpdateReset */
    /* 0x160 */ s32 keyRow;         /* key page: index into the save's key table of the row under the cursor */
    /* 0x164 */ s32 markX;          /* where Option_SetKeyMark puts the cursor mark and the arrows */
    /* 0x168 */ s32 markY;
    /* 0x16C */ s32 arrowX;
    /* 0x170 */ s32 arrowY;
    /* 0x174 */ s32 keyOld;         /* value of that row when the button went down, -1 = none */
    /* 0x178 */ s32 valueOld;       /* the value when a picker was opened (to tell a change) */
    /* 0x17C */ s32 keyCursor;      /* key page: cursor 0..7 (two columns of four) */
    /* 0x180 */ s32 keyEdit;        /* the key page is open */
    /* 0x184 */ s32 keyHeld;        /* confirm is held on the key page this frame */
    /* 0x188 */ s32 mcBusy;         /* a memory card flow is running: Option_Run skips Option_Input */
    /* 0x18C */ s32 started;
    /* 0x190 */ s32 greeted;        /* the second greeting line was started */
    /* 0x194 */ s32 unk194;
    /* 0x198 */ s32 dirty;          /* a setting was changed: save on leaving */
    /* 0x19C */ s32 *bgmIds;        /* section 37 + 0x10: music ids of the sound test */
    /* 0x1A0 */ s32 bgmCount;
    /* 0x1A4 */ void *msgText;      /* section 33 */
    /* 0x1A8 */ void *dialogMsg;    /* section 38: message table of the "reset?" dialog */
    /* 0x1AC */ void *subtitles;    /* section 36 */
    /* 0x1B0 */ u8 unk1B0[0x3C];    /* handed to MsgWin_Init, which ignores it */
    /* 0x1EC */ s32 key[OPTION_PAD_NUM][OPTION_KEY_NUM]; /* the default key assignment */
} Option; /* 0x22C */

#define OPTION_VOICE_BASE 0x873A

/* Option.cursor / Option.state: one number per row of the four pages; a state of the same number means that
   row is open. */
#define OPT_ST_SAVED (-2)      /* the save flow started by leaving has finished */
#define OPT_ST_START (-1)
#define OPT_ST_TOP 0
#define OPT_ITEM_SAVE 1        /* top page: save / load (two-way picker) */
#define OPT_ITEM_SCREEN 2      /* -> screen page */
#define OPT_ITEM_SOUND 3       /* -> sound page */
#define OPT_ITEM_CTRL 4        /* -> controller page */
#define OPT_ITEM_EXIT 5
#define OPT_ST_FADE 6          /* fading out */
#define OPT_ITEM_TYPE 7        /* screen page: three-way type with a picture (save + 0x1694) */
#define OPT_ITEM_SCR2 8        /* screen page: two-way setting (save + 0x1698) */
#define OPT_ITEM_ADJUST 9      /* screen page: screen position */
#define OPT_ITEM_SCR_RESET 10  /* screen page: reset */
#define OPT_ST_ADJUST_RESET 11
#define OPT_ITEM_STEREO 12     /* sound page: stereo / mono */
#define OPT_ITEM_VOLUME 13     /* sound page: music / effect volume */
#define OPT_ITEM_BGM 14        /* sound page: sound test */
#define OPT_ITEM_VOICE 15      /* sound page: voice language */
#define OPT_ITEM_SND_RESET 16  /* sound page: reset */
#define OPT_ITEM_KEYS 17       /* controller page: key assignment */
#define OPT_ITEM_VIB 18        /* controller page: vibration */
#define OPT_ITEM_CTRL_RESET 19 /* controller page: reset */
#define OPT_ST_VIB_EDIT 20
#define OPT_ST_KEYS_PAD 21
#define OPT_ST_KEYS_EDIT 22

extern Option *gOption;

/* Option_PlateGoto: which clip */
#define OPT_CLIP_ROW 0         /* "mc_menu_plate_%d" of the top page's cursor */
#define OPT_CLIP_ROW_SCREEN 1  /* the same on the screen page (cursor - 6) */
#define OPT_CLIP_ROW_SOUND 2   /* the sound page (cursor - 11) */
#define OPT_CLIP_BOTTOM 3      /* "mc_bottom_plate_1" */
#define OPT_CLIP_PICK 4        /* "mc_select_plate_%d" of the two-way picker's column */
#define OPT_CLIP_VIB_OFF 5     /* the vibration picker's plate for the current setting */
#define OPT_CLIP_KEY_OFF 6
#define OPT_CLIP_BGM 7         /* "mc_bgm_plate_%d" of the sound test's cursor */
#define OPT_CLIP_VOL_BGM 8     /* "mc_vol_plate_bgm_%d" of the saved music volume */
#define OPT_CLIP_VOL_SE 9
#define OPT_CLIP_ROW_CTRL 10   /* the controller page (cursor - 16) */

/* second half of the object (was src/menu/menu_y.c) */
void Option_Draw(void);
void Option_PlateGoto(s32 unused, s32 clip, char *label);  /* sends a plate / cursor clip to a label */
s32 Option_SetKeyMark(s32 row);                            /* key page: moves the cursor mark */
void Option_UpdateReset(void);                             /* the reset states (10, 11, 16, 19) */
void Option_DimPickers(void);
void Option_OnSaved(void);                                 /* McFlow "done" callback */
void Option_Term(void);

void EvoTop_Term(EvoTop *m);
void EvoTop_Draw(EvoTop *m);
void EvoTop_Input(EvoTop *m);
s32 EvoTop_Leave(EvoTop *m);
s32 EvoTop_Run(s32 section);
s32 OptMode_Main(void);
s32 Option_Init(s32 section);
s32 Option_Run(s32 section);
s32 Option_Input(void);

#endif
