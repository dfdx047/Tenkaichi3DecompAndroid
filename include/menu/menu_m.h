#ifndef MENU_MENU_M_H
#define MENU_MENU_M_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x36DBE8..0x372148 (placeholder stem "menu_m"): the two character selects of the mode
 * group 13..30 (handler 0x379A58, archive gMenuArc3, main-menu item 1). Two pieces, cut at an object boundary:
 *
 *   (menu_m.c)  0x36DBE8..0x36E028  SoloSel     tail of the one-character select (work pointer 0x3B7348): the
 *                                               guide's closing line and the frame loop; merged into menu_l_b.c
 *   menu_m_b.c  0x36E028..0x372148  UbTeamSel   the team select (one side, up to five fighters, optional DP
 *                                               limit; work pointer 0x3B734C), up to 0x372560: its last
 *                                               two functions (0x372148 the guide's closing line, 0x372260 the
 *                                               frame loop) come from the next chunk, menu_n
 *
 * All names are guesses from what the code does ("Ub" = the group of modes 13..30). The structures are this
 * chunk's own views; the member record and the grid cell are those of include/menu/menu_e.h (TsMember, TsCell).
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern s32 ChrTbl_GetCost(s32 chara);
extern s32 ChrTbl_IsRelated(s32 a, s32 b);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);

/* Voice_GetStat result when nothing is playing. */
#define UB_VOICE_IDLE 5
/* Voice bank base of this mode group's guides (Voice_PlayWithSubtitle). */
#define UB_VOICE_BASE 0x8765

/* A cell of the character grid (ChrGridCell of include/battle/view_a.h; local view). */
typedef struct UbCell {
    /* 0x00 */ s32 id;          /* character id 0..0xA0, or UB_ID_ */
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} UbCell; /* 0x24 */

#define UB_COLS 7
#define UB_CELL_MAX 165
#define UB_ID_RANDOM 0xA1       /* the cell that draws a character at random */
#define UB_ID_LOCKED 0xA2
#define UB_ID_REC 0xA3          /* the cell of the saved custom characters */
#define UB_ID_EMPTY 0xA4        /* filler: an empty chip */
#define UB_FACE_FILE DISC_RESOURCE(0x2F9, 0x71C)      /* + character id: compressed portrait */

extern void ChrGrid_MoveLeft(UbCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveRight(UbCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveUp(UbCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_MoveDown(UbCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_PrevForm(s32 *forms, s32 *form);
extern void ChrGrid_NextForm(s32 *forms, s32 *form);
extern s32 ChrGrid_IsSelectable(UbCell *cells, s32 index);
extern void ChrGrid_Build(s32 *outCount, UbCell *out, s32 *inCount, UbCell *in, s32 *customCount, UbCell *custom);

/* An item set: eight item ids. */
typedef struct UbItemSet {
    u16 id[8];
} UbItemSet;

/* gSaveData as these screens use it (include/sys/save.h has the full layout). */
typedef struct UbSaveCustom {
    /* 0x00 */ UbItemSet set[3];   /* the character's three item sets */
    /* 0x30 */ s32 unk30[2];
} UbSaveCustom; /* 0x38 */

typedef struct UbSave {
    /* 0x0000 */ u8 unk0[0x1808];
    /* 0x1808 */ UbSaveCustom custom[97]; /* by character-grid cell index (row * 7 + col) */
} UbSave;

#define UB_SAVE ((UbSave *)gSaveData)

/* What was chosen for one fighter (TsMember of include/menu/menu_e.h). Kept in gProgress between screens. */
typedef struct UbMember {
    /* 0x00 */ s32 col;         /* cursor column in the character grid */
    /* 0x04 */ s32 row;         /* cursor row */
    /* 0x08 */ s32 form;        /* chip of the form reel */
    /* 0x0C */ s32 recCol;      /* cursor in the custom-character list (not used by these screens) */
    /* 0x10 */ s32 recRow;
    /* 0x14 */ s32 plate;       /* item-set plate (0 = no items, 1..3 = a saved set) */
    /* 0x18 */ s32 color;       /* costume */
    /* 0x1C */ s32 chara;       /* the chosen character id, -1 = none yet */
    /* 0x20 */ UbItemSet items; /* equipped items */
} UbMember; /* 0x30 */

#define UB_MEMBER_MAX 5
#define UB_CUR_MENU 5            /* UbTeamState.cur on the "done" plate */

typedef struct UbTeam {
    UbMember member[UB_MEMBER_MAX];
} UbTeam; /* 0xF0 */

/* gProgress as the mode group 13..30 uses it (the fields this chunk touches). */
typedef struct UbProgress {
    /* 0x000 */ u8 unk0[0x14];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x424];
    /* 0x440 */ UbTeam team;    /* the player's choice: what the battle set-up of the mode reads */
    /* 0x530 */ u8 unk530[0x114];
    /* 0x644 */ s32 teamSize;   /* in: fighters to choose (>= 2 runs UbTeamSel, else SoloSel); out: fighters chosen */
    /* 0x648 */ s32 dpRule;     /* 0 = no DP limit, 1 / 2 / 3 = limit 10 / 15 / 20 */
} UbProgress;

#define UB_PROG ((UbProgress *)gProgress)

/* ---- menu overlay, other chunks ---- */

extern void ItemPanel_Init(void *data, s32 side);     /* menu_g */
extern void ItemPanel_Term(s32 side);
extern void ItemPanel_Draw(s32 side);
extern void ItemPanel_Update(s32 side);
extern s32 ItemPanel_Input(s32 side, s32 pad);
extern void ItemPanel_SetChara(s32 side, s32 chara, s32 slot, s32 set, s32 fromRec);
extern void ItemPanel_Show(s32 side);
extern void ItemPanel_Hide(s32 side);
extern void ItemHelp_Init(void *pack);                /* item help window: init */
extern void ItemHelp_Term(void);                      /* item help window: term */
extern void ItemHelp_Draw(s32 item);                  /* item help window: draw */
extern void ItemHelp_Open(void);                      /* item help window: open */
extern void ItemHelp_Close(void);                      /* item help window: close */

/* SoloSel (the former menu_m.c, 0x36DBE8..0x36E028) is now part of menu_l_b.c; its work area is SoloSel of
   include/menu/menu_l.h. */

/* flags of both screens */
#define UBSEL_STARTED 2         /* the first chip was lit */
#define UBSEL_DONE 8
#define UBSEL_LEAVING 0x10
#define UBSEL_DP 0x20           /* team select only: the team has a DP limit */
#define UBSEL_NO_CUSTOM 0x40    /* only the "no items" plate can be chosen; never set in this range */
#define UBSEL_GREETED 0x80      /* the guide's greeting was started */

#define UBSEL_END_NONE 0
#define UBSEL_END_SPEAK 1
#define UBSEL_END_WAIT 2
#define UBSEL_END_LEAVE 3

/* loadState: the portrait loader, one step per frame */
#define UBSEL_LOAD_REQUEST 1
#define UBSEL_LOAD_READ 2
#define UBSEL_LOAD_UNPACK 3
#define UBSEL_LOAD_IDLE 4
#define UBSEL_LOAD_ABORT 5
#define UBSEL_LOAD_RESTART 6

/* ---- UbTeamSel ---- */

#define UBTEAM_FLASH_NUM 4

typedef struct UbTeamState {
    /* 0x000 */ UbTeam team;
    /* 0x0F0 */ s32 chip[2][UB_COLS]; /* [0] characters on the seven chips of the reel, [1] before the last change */
    /* 0x128 */ s32 flags;          /* UBTEAM_SEL_ */
    /* 0x12C */ s32 cur;            /* member being chosen; 5 = on the menu plate */
    /* 0x130 */ s32 cost;           /* DP total of the team */
    /* 0x134 */ s32 memberCount;
    /* 0x138 */ s32 chara;          /* character whose portrait and names are shown; negative = none */
    /* 0x13C */ s32 step;           /* UBTEAM_STEP_ */
    /* 0x140 */ s32 mask;           /* non-zero while the chips are hidden for a reel change */
} UbTeamState; /* 0x144 */

#define UBTEAM_SEL_FACE_CHANGE 1    /* the portrait must be reloaded */
#define UBTEAM_SEL_FACE_READY 2     /* the portrait is loaded and fades in */
#define UBTEAM_SEL_FORM 0x40        /* the chips show the forms of the chosen cell */
#define UBTEAM_SEL_REC 0x80         /* cleared with FORM when a member is decided; never set here */
#define UBTEAM_SEL_NO_FACE 0x400    /* nothing to show (chara < 0) */
#define UBTEAM_SEL_PANEL 0x800      /* set when the item panel is opened; never cleared or tested here */

typedef struct UbTeamSel {
    /* 0x0000 */ u32 *pack;          /* this screen's section of archive 3 (compressed) */
    /* 0x0004 */ u32 *res;           /* the same unpacked */
    /* 0x0008 */ void *faceFile;     /* 0x16800 bytes: compressed portrait (file 0x2F9 + character) */
    /* 0x000C */ MTexRes *faceRes;   /* 0x20800 bytes: the same unpacked */
    /* 0x0010 */ u32 *chips;         /* section 27: 165 texture lists, section id + 1 = chip of a character */
    /* 0x0014 */ void *nameText;     /* section 28 */
    /* 0x0018 */ void *formText;     /* section 29 */
    /* 0x001C */ void *msgText;      /* section 25 */
    /* 0x0020 */ void *subtitles;    /* section 26 */
    /* 0x0024 */ MFlash flash[UBTEAM_FLASH_NUM]; /* 0 portrait and guides, 1 the team, 2 the reel, 3 the plates */
    /* 0x00D4 */ void *bg;           /* section 32: background picture */
    /* 0x00D8 */ u8 *texA[20];       /* movie 0 */
    /* 0x0128 */ u8 *texD[11];       /* movie 3 */
    /* 0x0154 */ u8 *texC[25];       /* movie 2 */
    /* 0x01B8 */ u8 *texB[13];       /* movie 1 */
    /* 0x01EC */ s32 flags;          /* UBSEL_ */
    /* 0x01F0 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x01F4 */ s32 loadState;      /* UBSEL_LOAD_ */
    /* 0x01F8 */ s32 timer;          /* frames until the fade out starts */
    /* 0x01FC */ s32 help;           /* item the help window explains */
    /* 0x0200 */ UbTeamState state;
    /* 0x0344 */ UbTeamState *sel;   /* &state */
    /* 0x0348 */ UbMember backup;    /* the member being changed, as it was */
    /* 0x0378 */ s32 gridCount;
    /* 0x037C */ UbCell *grid;
    /* 0x0380 */ s32 gridOutCount;
    /* 0x0384 */ UbCell gridBuf[UB_CELL_MAX];
    /* 0x1AB8 */ s32 rows;           /* grid rows */
    /* 0x1ABC */ s32 plateCount;     /* item-set plates that can be chosen */
    /* 0x1AC0 */ s32 colorCount;     /* costumes of the character */
    /* 0x1AC4 */ s32 teamMax;        /* gProgress->teamSize: fighters the team may have */
    /* 0x1AC8 */ s32 dpLevel;        /* gProgress->dpRule - 1 */
    /* 0x1ACC */ s32 dpMax;          /* 10 / 15 / 20 */
    /* 0x1AD0 */ f32 faceAlpha;
    /* 0x1AD4 */ s32 endStep;        /* UBSEL_END_ */
    /* 0x1AD8 */ s32 talker;         /* which of the two guides is talking */
    /* 0x1ADC */ s32 blink[2];
    /* 0x1AE4 */ s32 talk[2];
    /* 0x1AEC */ MTextBox box[2];    /* character name, form name */
    /* 0x1C04 */ void *items;        /* item table of common file 4 */
} UbTeamSel; /* 0x1C08 */

/* UbTeamState.step */
#define UBTEAM_STEP_CHARA 0
#define UBTEAM_STEP_FORM 1
#define UBTEAM_STEP_PLATE 2
#define UBTEAM_STEP_PANEL 3
#define UBTEAM_STEP_HELP 4
#define UBTEAM_STEP_COLOR 5
#define UBTEAM_STEP_TEAM 6

/* kinds of UbTeamSel_ClipGoto */
#define UBTEAM_CLIP_CHIP 0
#define UBTEAM_CLIP_FORM_CHIP 1
#define UBTEAM_CLIP_PLATE 2
#define UBTEAM_CLIP_COLOR 5
#define UBTEAM_CLIP_TEAM 6

extern UbTeamSel *gUbTeamSel;    /* 0x3B734C */

void UbTeamSel_SumCost(s32 skipCur);
s32 UbTeamSel_IsCharaFree(s32 chara);
s32 UbTeamSel_FitsDp(s32 chara);
void UbTeamSel_RemoveMember(s32 idx);
void UbTeamSel_SetChips(void);
void UbTeamSel_SetFormChips(void);
void UbTeamSel_SetTeamTex(void);
void UbTeamSel_UpdateFaceLoad(void);
void UbTeamSel_RequestFace(void);
void UbTeamSel_ClipGoto(s32 movie, s32 kind, char *label);
void UbTeamSel_Init(s32 section);
void UbTeamSel_Term(void);
void UbTeamSel_Draw(void);
void UbTeamSel_Update(void);
void UbTeamSel_Input(s32 *result);
void UbTeamSel_UpdateEnd(void);
s32 UbTeamSel_Run(s32 section);

#endif
