#ifndef MENU_MENU_Y_H
#define MENU_MENU_Y_H

#include "types.h"
#include "menu/menu_a.h"
#include "sys/save.h"
#include "sys/pad.h"

/*
 * Menu overlay DBZP.BIN, 0x3A3848..0x3A7D98 (placeholder stem "menu_y"). Two pieces, cut at the object boundary:
 *
 *   (menu_y.c)  0x3A3848..0x3A65E8  Option   tail of the option screen object (mode 62, handler 0x39FAA8; work
 *                                            pointer 0x3BC364): now merged into src/menu/menu_x_c.c
 *   menu_y_b.c  0x3A65E8..0x3A9850  DcList   the Data Center's custom character list (mode 55, handler 0x3A9850;
 *                                            menu_z.c, its tail, was merged in): declared in menu_z.h
 *
 * All names are guesses from what the code does. The structures are this chunk's own views.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Flash_ClipSetScale(MFlash *flash, MFlashRef *ref, f32 x, f32 y);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void IconWin_SetIcon(s32 icon);
extern void Dialog_Draw(s32 visible);
extern void Dialog_Start(s32 kind);
extern s32 Dialog_Input(s32 allowCancel);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetMsgTable(void *table);
extern void Dialog_SetMsg(s32 idx);
extern void Dialog_SetCursor(s32 cursor);
extern s32 Dialog_IsClosed(void);
extern void SndOpt_Apply(void);
extern void Dialog_Init(void *file, void *msgTbl, s32 size);
extern void Dialog_Term(void);

/* Snd_PlaySe returns a value in the original (menu_a.h declares it void). */
#define Snd_PlaySe ((s32 (*)(u32, s32))Snd_PlaySe)

/* Voice_GetStat result when nothing is playing. */
#define Y_VOICE_IDLE 5

/* ---- Option (menu_x_c.c): the option screen of mode 62 ---- */

/* The work area (Option, gOption) is declared in include/menu/menu_x.h; src/menu/menu_x_c.c, the whole object
   since menu_y.c was merged into it, includes that header first. */

/* Option.state values this chunk tests (menu_x.h: OPT_ITEM_ / OPT_ST_). */
#define YOPT_SCREEN 2          /* top page row: screen page */
#define YOPT_SOUND 3
#define YOPT_CTRL 4
#define YOPT_TYPE 7            /* screen page: controller-layout type */
#define YOPT_ADJUST 9
#define YOPT_SCR_RESET 10
#define YOPT_ADJUST_RESET 11
#define YOPT_STEREO 12
#define YOPT_VOLUME 13
#define YOPT_BGM 14
#define YOPT_SND_RESET 16
#define YOPT_KEYS 17
#define YOPT_CTRL_RESET 19
#define YOPT_KEYS_PAD 21
#define YOPT_KEYS_EDIT 22

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

void Option_Draw(void);
void Option_PlateGoto(s32 unused, s32 clip, char *label);
s32 Option_SetKeyMark(s32 row);
void Option_UpdateReset(void);
void Option_SetBottomText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv);
void Option_SetMenuText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv);
void Option_DimPickers(void);
void Option_SetBgmScissor(void);
void Option_ResetScissor(void);
void Option_OnSaved(void);
void Option_Term(void);


/* DcList (menu_y_b.c, the whole object since menu_z.c was merged into it): see include/menu/menu_z.h. */

#endif
