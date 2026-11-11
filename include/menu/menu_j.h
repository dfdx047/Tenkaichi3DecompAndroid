#ifndef MENU_MENU_J_H
#define MENU_MENU_J_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x35F650..0x364358 (placeholder stem "menu_j"): Dragon World Tour (the tournament mode,
 * progress modes 33..35, main-menu item 2, archive gMenuArc4). Two pieces:
 *
 *   (menu_j.c  0x35F650..0x3623A8  EntrySel  rest of the entrant select (mode 34) and Tour_Main, the handler of
 *                                             modes 33..35: merged into menu_i_d.c, where the object starts)
 *   menu_j_b.c  0x3623A8..0x364DA8  TourMenu  the tournament menu (mode 33); merged with the former menu_k.c
 *
 * ESel below is the one view of the EntrySel work area (menu_i_d.c includes menu_i.h and then this header; the
 * partial view that menu_i.h had is gone).
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode,
                          s32 parentFmt);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern void *memcpy(void *, const void *, u32);

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* Voice bank base of the menu guides (Voice_PlayWithSubtitle). */
#define TOUR_VOICE_BASE 0x85D3

/* A cell of the character grid (ChrGridCell of include/battle/view_a.h; local view). */
typedef struct ESelCell {
    /* 0x00 */ s32 id;          /* character id 0..0xA0, or ESEL_ID_ (0xA2 locked, 0xA4 filler) */
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} ESelCell; /* 0x24 */

#define ESEL_ID_NONE 0xA4         /* filler: no character */
#define ESEL_ID_RANDOM 0xA1       /* a random character is drawn when the entrant is decided */
#define ESEL_ID_REC 0xA3          /* the cell of the saved custom characters (gSaveData->rec) */

/* A grid as stored in a menu pack (section 29 of the entrant select). */
typedef struct ESelGridList {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ ESelCell cell[1];
} ESelGridList;

extern s32 ChrGrid_IsSelectable(ESelCell *cells, s32 index);
extern void ChrGrid_Build(s32 *outCount, ESelCell *out, s32 *inCount, ESelCell *in, s32 *customCount, ESelCell *custom);
extern void ChrGrid_MoveLeft(ESelCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveRight(ESelCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveUp(ESelCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_MoveDown(ESelCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_PrevForm(s32 *forms, s32 *form);
extern void ChrGrid_NextForm(s32 *forms, s32 *form);

/* ---- menu overlay, other chunks ---- */

extern void ItemPanel_Term(s32 side);                 /* menu_g */
extern void ItemPanel_Draw(s32 side);
extern void ItemPanel_Update(s32 side);
extern s32 ItemPanel_Input(s32 side, s32 pad);
extern void ItemPanel_SetChara(s32 side, s32 chara, s32 slot, s32 set, s32 fromRec);
extern void ItemPanel_Show(s32 side);
extern void ItemPanel_Hide(s32 side);
extern void ItemHelp_Init(void *pack);                /* item help window: init (EntrySel_Init) */
extern void ItemHelp_Term(void);                      /* item help window: term */
extern void ItemHelp_Draw(s32 item);                  /* item help window: draw */
extern void ItemHelp_Open(void);                      /* item help window: open */
extern void ItemHelp_Close(void);                      /* item help window: close */
extern void TourBg_Term(void);                        /* menu_k: the cloud backdrop (work pointer 0x3B591C) */
extern void TourBg_Draw(void);
extern s32 Bracket_Run(s32 section);                  /* menu_k: the bracket screen, which sets up the battles */

/* ---- EntrySel (menu_i_d.c) ---- */

#define ESEL_COLS 7
#define ESEL_CELL_MAX 165
#define ESEL_ENTRY_MAX 8
#define ESEL_FLASH_NUM 4

/* A set of equipped items: 1-based item ids, 0 = empty (SaveCustom.item[n], the head of a SaveRec). */
typedef struct TourItemSet {
    u16 id[8];
} TourItemSet;

/* gSaveData->rec[n] (SaveRec, 0x1C bytes): a saved custom character begins with its item set. */
typedef struct TourSaveRec {
    /* 0x00 */ TourItemSet items;
    /* 0x10 */ s32 unk10[3];
} TourSaveRec; /* 0x1C */

typedef struct TourSave {
    /* 0x0000 */ u8 unk0[0x2D40];
    /* 0x2D40 */ TourSaveRec rec[14];
} TourSave;

#define TOUR_SAVE ((TourSave *)gSaveData)

/* One entrant. */
typedef struct ESelEntry {
    /* 0x00 */ s32 col;         /* grid column */
    /* 0x04 */ s32 row;         /* grid row */
    /* 0x08 */ s32 form;        /* index into the cell's form list */
    /* 0x0C */ s32 recCol;      /* cursor in the list of saved custom characters (7 columns); never written here */
    /* 0x10 */ s32 recRow;
    /* 0x14 */ s32 custom;      /* 0 = no items, 1..3 = one of the character's saved item sets */
    /* 0x18 */ s32 costume;
    /* 0x1C */ s32 chara;       /* the character id finally chosen */
    /* 0x20 */ TourItemSet items; /* the item set taken along (zero when custom == 0) */
} ESelEntry; /* 0x30 */

typedef struct ESelState {
    /* 0x000 */ ESelEntry entry[ESEL_ENTRY_MAX];
    /* 0x180 */ s32 rowChara[ESEL_COLS];     /* characters on the seven chips (a grid row, or the forms of a cell) */
    /* 0x19C */ s32 prevRowChara[ESEL_COLS];
    /* 0x1B8 */ s32 flags;                   /* ESEL_SEL_ */
    /* 0x1BC */ s32 image;                   /* character whose large picture, name and form text are shown */
    /* 0x1C0 */ s32 cur;                     /* entrant being chosen */
    /* 0x1C4 */ s32 step;                    /* ESEL_STEP_ */
    /* 0x1C8 */ s32 rowAnim;                 /* the chip row is scrolling: the old chips stay visible */
} ESelState; /* 0x1CC */

#define ESEL_SEL_IMAGE_CHANGE 1
#define ESEL_SEL_IMAGE_READY 2
#define ESEL_SEL_FORM 0x40      /* the chips show the forms of the chosen cell */
#define ESEL_SEL_REC 0x80       /* chosen from the saved custom characters; never set in this range */

#define ESEL_STEP_CHARA 0       /* cursor on the grid */
#define ESEL_STEP_FORM 1        /* cursor on the forms */
#define ESEL_STEP_CUSTOM 2      /* item set plates */
#define ESEL_STEP_PANEL 3       /* item panel of the chosen set */
#define ESEL_STEP_HELP 4        /* item help window */
#define ESEL_STEP_COLOR 5       /* costume plates */

/* loadState: the same machine as ModeMenu's */
#define ESEL_LOAD_REQUEST 1
#define ESEL_LOAD_READ 2
#define ESEL_LOAD_UNPACK 3
#define ESEL_LOAD_SHOWN 4
#define ESEL_LOAD_ABORT 5
#define ESEL_LOAD_RESTART 6

typedef struct ESel {
    /* 0x0000 */ u32 *pack;          /* this screen's section of archive 4 (compressed) */
    /* 0x0004 */ u32 *res;           /* the same unpacked */
    /* 0x0008 */ void *imageFile;    /* 0x16800 bytes: compressed large picture (file 0x71C + character) */
    /* 0x000C */ MTexRes *imageRes;  /* 0x20800 bytes: the same unpacked */
    /* 0x0010 */ u32 *chips;         /* section 32: pack of the small character pictures */
    /* 0x0014 */ void *nameText;     /* section 30 */
    /* 0x0018 */ void *formText;     /* section 31 */
    /* 0x001C */ void *msgText;      /* section 33 */
    /* 0x0020 */ void *subtitles;    /* section 36 */
    /* 0x0024 */ void *file;         /* file 0x3C9 + tournament */
    /* 0x0028 */ MFlash flash[ESEL_FLASH_NUM]; /* 0 picture and entry list, 1 chips, 2 custom / colour plates, 3 guide */
    /* 0x00D8 */ u8 *texA[19];
    /* 0x0124 */ u8 *texC[11];
    /* 0x0150 */ u8 *texB[25];
    /* 0x01B4 */ u8 *texD[9];
    /* 0x01D8 */ s32 flags;          /* ESEL_ */
    /* 0x01DC */ s32 unk1DC;
    /* 0x01E0 */ s32 timer;          /* frames until the fade out starts once all entrants are chosen */
    /* 0x01E4 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x01E8 */ s32 loadState;      /* ESEL_LOAD_: the large picture's loader */
    /* 0x01EC */ s32 unk1EC;
    /* 0x01F0 */ s32 helpItem;       /* item the help window explains */
    /* 0x01F4 */ s32 endStep;        /* ESEL_END_: the guide's closing line */
    /* 0x01F8 */ ESelState state;
    /* 0x03C4 */ ESelState *sel;     /* &state */
    /* 0x03C8 */ s32 gridCount;
    /* 0x03CC */ ESelCell *grid;
    /* 0x03D0 */ s32 gridOutCount;
    /* 0x03D4 */ ESelCell gridBuf[ESEL_CELL_MAX];
    /* 0x1B08 */ s32 rows;           /* grid rows */
    /* 0x1B0C */ s32 customCount;    /* item-set plates that can be chosen: 4, or 1 with ESEL_NO_CUSTOM */
    /* 0x1B10 */ s32 colorCount;     /* costumes of the character */
    /* 0x1B14 */ f32 imageAlpha;
    /* 0x1B18 */ s32 talker;         /* which guide is talking (the Yamcha Game has two) */
    /* 0x1B1C */ s32 blink[2];
    /* 0x1B24 */ s32 talk[2];
    /* 0x1B2C */ MTextBox box[2];    /* character name, form name */
    /* 0x1C44 */ void *items;        /* item table of common file 4 */
} ESel; /* 0x1C48 */

#define ESEL_DONE 1
#define ESEL_LEAVING 2
#define ESEL_STARTED 4          /* the first chip was lit */
#define ESEL_GREETED 8          /* the guide's greeting was started */
#define ESEL_NO_CUSTOM 0x10     /* only the "no items" plate can be chosen; never set in this range */

#define ESEL_END_NONE 0
#define ESEL_END_SPEAK 1
#define ESEL_END_WAIT 2
#define ESEL_END_LEAVE 3

/* What the tournament keeps of one entrant: gProgress + 0x98, 0x28 bytes each. */
typedef struct TourEntrant {
    /* 0x00 */ u16 flags;        /* TOUR_ENT_ */
    /* 0x02 */ u16 unk2;
    /* 0x04 */ s32 chara;
    /* 0x08 */ s32 costume;
    /* 0x0C */ s32 player;       /* index of the entrant among the player's choices */
    /* 0x10 */ s32 unk10[2];
    /* 0x18 */ u16 item[8];
} TourEntrant; /* 0x28 */

#define TOUR_ENT_PLAYER 1        /* chosen by the player */
#define TOUR_ENT_REC 0x10        /* chosen from the cell of the saved custom characters */
#define TOUR_ENT_ITEMS 0x20      /* brings an item set */

#define TOUR_ENTRANT_MAX 17      /* 0x2A8 bytes cleared by EntrySel_Init */

/* The tournament's block of gProgress (0x7C..0x440, one of the blocks Progress_ClearSession clears). */
typedef struct TourSession {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 entry;       /* gProgress + 0x80: 0 the open tournament (prizes), 1 free play; set by TourMenu_Run */
    /* 0x008 */ s32 tour;        /* gProgress + 0x84: TOUR_, which tournament; set by TourMenu_Run */
    /* 0x00C */ s32 level;       /* gProgress + 0x88: difficulty 0..2; set by TourMenu_Run */
    /* 0x010 */ s32 entryNum;    /* gProgress + 0x8C: entrants the player chooses, 1..8; set by TourMenu_Run */
    /* 0x014 */ s32 round;       /* gProgress + 0x90 (menu_k) */
    /* 0x018 */ s32 match;       /* gProgress + 0x94 (menu_k) */
    /* 0x01C */ TourEntrant entrant[TOUR_ENTRANT_MAX]; /* gProgress + 0x98 */
    /* 0x2C4 */ u8 matches[0x100];                     /* gProgress + 0x340: the match table (menu_k) */
} TourSession; /* 0x3C4 */

/* gProgress as the tournament mode uses it. */
typedef struct TourProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x60];
    /* 0x07C */ TourSession t;
    /* 0x440 */ ESelEntry lastEntry; /* EntrySel: cursor of the previous visit */
} TourProgress;

#define TOUR_PROG ((TourProgress *)gProgress)

/* The five tournaments, in menu order (names from the guide clips; the first two are inferred). */
#define TOUR_WORLD 0             /* "mc_guide_tenkaichi": World Tournament */
#define TOUR_BIG 1               /* World Martial Arts Big Tournament (shares the Cell guide clips) */
#define TOUR_CELL 2              /* "mc_guide_ceru": Cell Games */
#define TOUR_OTHERWORLD 3        /* "mc_guide_anoyo": Otherworld Tournament */
#define TOUR_YAMCHA 4            /* "mc_guide_yamucha" / "puaru": Yamcha Game, entrants drawn at random */
#define TOUR_NUM 5

extern ESel *gEntrySel;          /* 0x3B5910 */

void EntrySel_SwapRowTex(void);
void EntrySel_SetRowTex(void);
void EntrySel_SetMemberTex(void);
void EntrySel_UpdateImage(void);
void EntrySel_ChangeImage(void);
void EntrySel_ClipGoto(s32 movie, s32 kind, char *label);
void EntrySel_Init(s32 section);
void EntrySel_Term(void);
void EntrySel_Draw(void);
void EntrySel_Update(void);
void EntrySel_Input(s32 *result);
void EntrySel_UpdateEnd(void);
s32 EntrySel_Run(s32 section);
s32 Tour_Main(void);

/* ---- TourMenu (menu_j_b.c) ---- */

#define TOURMENU_FLASH_NUM 2

/* Menu levels: `level` indexes cursor[]. */
#define TOURMENU_LV_TOP 0        /* 0 the open tournament (prizes, fixed level, one entrant), 1 free play */
#define TOURMENU_LV_TOUR 1       /* which of the five tournaments */
#define TOURMENU_LV_INFO 2       /* the tournament's description */
#define TOURMENU_LV_LEVEL 3      /* difficulty, 0..2 */
#define TOURMENU_LV_NUM 4        /* number of entrants - 1, 0..7 */
#define TOURMENU_LV_MAX 5

/* Section 27 of the menu's pack: what the menu shows about one tournament. */
typedef struct TourInfo {
    /* 0x00 */ s32 unk0[4];
    /* 0x10 */ s32 prize[2][3];    /* the two prize sums (zenny) for each of the three levels */
    /* 0x28 */ s32 unk28[15];
} TourInfo; /* 0x64 */

typedef struct TourMenu {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 4 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked */
    /* 0x008 */ void *imageFile;    /* 0xF000 bytes: the invitation picture, file 0x3C4 + tournament (compressed) */
    /* 0x00C */ void *imageRes;     /* 0x41000 bytes: the same unpacked */
    /* 0x010 */ void *msgText;      /* section 19 */
    /* 0x014 */ void *subtitles;    /* section 23 */
    /* 0x018 */ MFlash flash[TOURMENU_FLASH_NUM]; /* 0 the menu, 1 the invitation */
    /* 0x070 */ void *bg;           /* section 1: background picture */
    /* 0x074 */ u8 *tex[31];
    /* 0x0F0 */ u8 *texB[6];
    /* 0x108 */ s32 flags;          /* TOURMENU_ */
    /* 0x10C */ s32 cursor[TOURMENU_LV_MAX];
    /* 0x120 */ s32 leaveTimer;     /* frames until the fade out starts (TourMenu_Run) */
    /* 0x124 */ s32 iconTimer;
    /* 0x128 */ s32 iconFrame;
    /* 0x12C */ s32 level;          /* TOURMENU_LV_ */
    /* 0x130 */ s32 invite;         /* tournament the invitation is for */
    /* 0x134 */ s32 seq;            /* step of the guide's scripted speech (TourMenu_UpdateSeq), 0 = idle */
    /* 0x138 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x13C */ s32 blink;
    /* 0x140 */ s32 talk;
    /* 0x144 */ TourInfo *info;     /* section 27: one entry per tournament */
} TourMenu; /* 0x148 */

#define TOURMENU_DONE 1
#define TOURMENU_LEAVING 2
#define TOURMENU_STARTED 4
#define TOURMENU_GREETED 8      /* the opening step was chosen once the movie accepted input */
#define TOURMENU_INVITE 0x10    /* an invitation arrived: its picture is loaded */

/* gSaveData->unkA08: bit n (0..4) = tournament n is the open one; 0x20 = the mode was visited (the clock runs) */
#define TOUR_SAVE_INVITE_MASK 0x1F
#define TOUR_SAVE_STARTED 0x20  /* the first-visit speech was heard */
#define TOUR_SAVE_EXPLAINED 0x40 /* the guide's long explanation was heard */
#define TOUR_HOURS 24           /* gSaveData->unkA0C, the mode's clock, runs 0..23 */

extern TourMenu *gTourMenu;      /* 0x3B5914 */

void TourMenu_CheckInvite(void);
void TourMenu_Init(s32 section);
void TourMenu_Term(void);
void TourMenu_Draw(void);
void TourMenu_ClipGoto(s32 movie, s32 kind, char *label);
void TourMenu_Update(void);
void TourMenu_Input(s32 *result);
void TourMenu_UpdateSeq(void);
s32 TourMenu_Run(s32 section);

#endif
