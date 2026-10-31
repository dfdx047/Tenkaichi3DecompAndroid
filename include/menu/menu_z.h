#ifndef MENU_MENU_Z_H
#define MENU_MENU_Z_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#include "sys/pad.h"
#include "sys/common.h"
#include "sys/save.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x3A7D98..0x3AC440 (placeholder stem "menu_z"): the "Data Center" (main-menu item 7,
 * progress modes 53..56; the development paths in the objects' data are "host:data/ps2/test/main/DC/").
 *
 *   (menu_z.c)  0x3A7D98..0x3A9850  DcList   tail of the saved-custom-character list (mode 55): now merged
 *                                            into src/menu/menu_y_b.c (0x3A65E8..0x3A9850), which uses the
 *                                            DcList layout below
 *   menu_z_b.c  0x3A9850..0x3A9A70  Dc_Main, the handler of modes 53..56
 *   menu_z_c.c  0x3A9A70..0x3AAF30  DcMenu   the mode's top menu (mode 53), a whole object
 *   menu_z_d.c  0x3AAF30..0x3AE648  DcPass   the password screen object (mode 54); menu_za.c, its tail
 *                                            (from 0x3AC440), was merged in
 *
 * Every structure here is this chunk's own view: the neighbours' headers were not written yet.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern s32 atoi(const char *);
extern char *strcpy(char *, const char *);
extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetScale(MFlash *flash, MFlashRef *ref, f32 x, f32 y);
extern u32 strlen(const char *);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Flash_ClipGetPos(MFlash *flash, MFlashRef *ref, s32 *x, s32 *y);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetRect(MTextBox *box, s32 x0, s32 x1, s32 y0, s32 y1);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern s32 Dialog_Input(s32 allowCancel);
extern s32 Dialog_IsClosed(void);
extern void Dialog_Draw(s32 visible);
extern void Dialog_Start(s32 kind);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetCursor(s32 choice);
extern void Dialog_SetMsg(s32 idx);
extern void Dialog_SetLayout(s32 layout);
extern void Voice_StopWithLip(void);
extern void McFlow_SetModeCb(s32 mode, s32 idx, void *cb, s32 arg);
extern void McFlow_SetSlot(s32 slot);
extern s32 McFlow_PollCard(void);
extern void Progress_ClearTeams(void);
extern double pow(double, double);

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* Voice set of the mode's guide (Bulma: clips "mc_guide_blma_eye" / "mc_guide_blma_mouth"). */
#define DC_VOICE_BASE 0x8398

/* Game buttons (Pad.gamePressed / gameRepeat). */
#define ZPAD_DOWN 4
#define ZPAD_UP 8
#define ZPAD_OK 0x200
#define ZPAD_CANCEL 0x400

/* Item entry of common file 4, section 2 (ItemTblEntry in battle/view_b.h; local view). */
typedef struct ZItemEntry {
    /* 0x00 */ u8 type;         /* 0..2: column of the item-kind icon */
    /* 0x01 */ u8 unk1[2];
    /* 0x03 */ u8 slots;        /* how many item slots it takes (0 = no cost icon) */
    /* 0x04 */ u8 unk4[0x24];
} ZItemEntry; /* 0x28 */

extern s32 ItemTbl_GetClass(s32 item, ZItemEntry *table);

/* A saved custom character: gSaveData->rec[n] (sys/save.h SaveRec, with the first 0x14 bytes worked out). */
typedef struct ZSaveRec {
    /* 0x00 */ u16 item[8];     /* item ids, 1-based, 0 = empty (the eighth is never shown as an item) */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u16 level;
    /* 0x16 */ u16 unk16;
    /* 0x18 */ s32 chara;       /* character id, -1 = empty */
} ZSaveRec; /* 0x1C */

/* What the status page of a custom character shows; filled by ItemSet_GetBonus from the record's items. */
typedef struct ZStatus {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 val[5];      /* [0] item slots the character has; [1..4] the four bars, -4..4 */
    /* 0x18 */ s32 attr;        /* frame of "mc_status_attribute" */
    /* 0x1C */ ZSaveRec rec;
} ZStatus; /* 0x38 */

extern void ItemSet_GetBonus(u16 *items, ZItemEntry *table, s32 *out);

/* gProgress as this mode uses it (menu_a.h's MenuProgress has no names here). */
typedef struct ZProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;         /* ZPROG_ */
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x690 - 0x1C];
    /* 0x690 */ s32 dcVisits;      /* times Dc_Main was left since the session was cleared */
    /* 0x694 */ s32 dcCursor;      /* plate the top menu was left on, -1 = the mode was just entered */
    /* 0x698 */ s32 unk698;        /* cleared by Dc_Main */
} ZProgress;

#define ZPROG ((ZProgress *)gProgress)
#define ZPROG_DIRTY 1              /* the save changed in this mode: the top menu offers to save on leaving */

/* Flat view of the save for the three fields this chunk touches. */
typedef struct ZSave {
    /* 0x0000 */ u8 unk0[0x1208];
    /* 0x1208 */ s32 dcFlags;      /* bit 0: the guide's introduction of the Data Center was heard */
    /* 0x120C */ u8 unk120C[0x2D40 - 0x120C];
    /* 0x2D40 */ ZSaveRec rec[SAVE_REC_COUNT];
} ZSave;

#define ZSAVE ((ZSave *)gSaveData)

/* ---- DcList (src/menu/menu_y_b.c): the list of saved custom characters (mode 55). ---- */

#define DCLIST_FLASH_NUM 2
#define DCLIST_ITEM_ROWS 8
#define DC_ROWS 3                  /* plates the cursor can be on; a fourth shows the row scrolling in or out */
#define DC_TOP_MAX 11              /* SAVE_REC_COUNT - DC_ROWS */

/* The list and its cursor, with a copy of the fourteen records. */
typedef struct DcChars {
    /* 0x000 */ s32 top;           /* record on the first plate, 0..11 */
    /* 0x004 */ s32 row;           /* plate the cursor is on, 0..2 */
    /* 0x008 */ s32 extra;         /* record shown on the fourth plate while the list scrolls, -1 = none */
    /* 0x00C */ ZSaveRec rec[SAVE_REC_COUNT]; /* copy of gSaveData->rec */
} DcChars; /* 0x194 */

/*
 * What the details panel shows for the chosen record. Not ZStatus: here ItemSet_GetBonus writes to offset 0
 * (DcStatus_Calc), in the password screen to offset 4.
 */
typedef struct DcStatus {
    /* 0x00 */ s32 bonus[5];       /* ItemSet_GetBonus: [0] item slots used (0..7), [1..4] stat changes -3..3 */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 attr;           /* picture of "mc_status_attribute" */
    /* 0x1C */ ZSaveRec rec;       /* the character under the cursor */
} DcStatus; /* 0x38 */

/* The part of the work the drawing helpers take (the movies, their textures and the text boxes). */
typedef struct DcView {
    /* 0x000 */ MFlash flash[DCLIST_FLASH_NUM]; /* 0 the list ("chara_list_top", section 5), 1 the item page
                                                   ("chara_list_z_item", section 9) */
    /* 0x058 */ MTexRes *bg;        /* section 1 */
    /* 0x05C */ u8 *tex0[38];
    /* 0x0F4 */ u8 *tex1[17];
    /* 0x138 */ s32 blink;          /* the guide's blink timer; starts at Rand_Range(32) */
    /* 0x13C */ s32 talk;
    /* 0x140 */ MTextBox nameBox[4]; /* character name of each list plate */
    /* 0x370 */ MTextBox formBox[4]; /* second text line of each list plate */
    /* 0x5A0 */ MTextBox nameBoxB;  /* "mc_name_text_l" of the details panel */
    /* 0x62C */ MTextBox formBoxB;  /* "mc_form_text_l" of the details panel */
    /* 0x6B8 */ MTextBox nameBoxL;  /* "mc_name_text_l" of the item page */
    /* 0x744 */ MTextBox formBoxL;  /* "mc_form_text_l" of the item page */
    /* 0x7D0 */ MTextBox itemBox[DCLIST_ITEM_ROWS];
} DcView; /* 0xC30 */

typedef struct DcList {
    /* 0x000 */ void *pack;         /* this screen's section of archive 8 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 23 sections */
    /* 0x008 */ void *faceFile;     /* 0x16800 bytes: file 0x2F9 + character, compressed */
    /* 0x00C */ MTexRes *faceRes;   /* 0x20800 bytes: the same unpacked */
    /* 0x010 */ s32 section;
    /* 0x014 */ DcView view;
    /* 0xC44 */ u8 unkC44[0x3C];    /* passed to MsgWin_Init, which does not take it */
    /* 0xC80 */ void *msgText;      /* section 13 */
    /* 0xC84 */ void *dialogMsg;    /* section 22 */
    /* 0xC88 */ void *subtitles;    /* section 14 */
    /* 0xC8C */ void *nameText;     /* section 17: character names */
    /* 0xC90 */ void *formText;     /* section 18: form names */
    /* 0xC94 */ void *itemText;     /* section 21: item names */
    /* 0xC98 */ f32 cloudX;         /* scroll of "mc_compane_3" */
    /* 0xC9C */ s32 result;         /* what DcList_Run returns; cleared when the list is left */
    /* 0xCA0 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0xCA4 */ s32 state;          /* DCLIST_ST_ */
    /* 0xCA8 */ s32 menuCursor;     /* details menu: 0 items, 1 password, 2 delete */
    /* 0xCAC */ s32 itemCursor;     /* row of the item page, 0..7 */
    /* 0xCB0 */ DcChars chars;
    /* 0xE44 */ DcStatus status;    /* the character under the cursor */
    /* 0xE7C */ s32 unkE7C;         /* 30 at start */
    /* 0xE80 */ ZItemEntry *itemTbl; /* section 15 */
    /* 0xE84 */ s32 flags;          /* DCLIST_ */
    /* 0xE88 */ s32 nextState;      /* state the dialog leads to when it has closed */
    /* 0xE8C */ f32 faceAlpha;
} DcList; /* 0xE90 */

/* DcList.state: what the pad drives */
#define DCLIST_ST_LIST 0
#define DCLIST_ST_MENU 1
#define DCLIST_ST_ITEMS 2       /* the item page */
#define DCLIST_ST_ITEM_HELP 3   /* the item details page (ItemHelp) */
#define DCLIST_ST_PASSWORD 4    /* the password window */
#define DCLIST_ST_DIALOG 5      /* "delete this character?" */
#define DCLIST_ST_DELETED 6     /* the guide's line after a deletion */

#define DCLIST_FACE_CHANGE 1    /* the character under the cursor changed: request its picture */
#define DCLIST_FACE_LOADING 2
#define DCLIST_STARTED 4
#define DCLIST_GREETED 8
#define DCLIST_LEAVE 0x10
#define DCLIST_LEAVING 0x20
#define DCLIST_FACE_READY 0x40  /* the picture is loaded and fades in */

#define DCLIST_FACE_FILE DISC_RESOURCE(0x2F9, 0x71C)
#define DCLIST_FACE_SIZE 0x16800

/* ---- DcMenu: the top menu of the Data Center (mode 53); a whole object. ---- */

#define DCMENU_PLATES 3

/* The part of the work the drawing helpers take. */
typedef struct DcMenuView {
    /* 0x00 */ MFlash flash;       /* section 5 */
    /* 0x2C */ MTexRes *bg;        /* section 6 */
    /* 0x30 */ u8 *tex[16];
    /* 0x70 */ s32 blink;
    /* 0x74 */ s32 talk;
} DcMenuView; /* 0x78 */

typedef struct DcMenu {
    /* 0x00 */ void *pack;         /* this screen's section of archive 8 (compressed) */
    /* 0x04 */ u32 *res;           /* the same unpacked: a pack of 13 sections */
    /* 0x08 */ DcMenuView view;
    /* 0x80 */ void *msgText;      /* section 4 */
    /* 0x84 */ void *subtitles;    /* section 1 */
    /* 0x88 */ u8 unk88[0x3C];     /* passed to MsgWin_Init, which does not take it */
    /* 0xC4 */ f32 scroll;         /* "mc_compane_3" */
    /* 0xC8 */ s32 cursor;         /* plate: 0 password, 1 character list, 2 replay */
    /* 0xCC */ s32 result;         /* the mode to go to (54..56), 0 = back to the main menu */
    /* 0xD0 */ s32 voiceLine;      /* the guide's current line (it also drives DcMenu_UpdateVoice), -1 = none */
    /* 0xD4 */ s32 unkD4;          /* set to 30, never read here */
    /* 0xD8 */ s32 visits;         /* gProgress->dcVisits when the screen started */
    /* 0xDC */ s32 flags;          /* DCMENU_ */
} DcMenu; /* 0xE0 */

#define DCMENU_CHOSEN 1
#define DCMENU_LEAVING 2
#define DCMENU_STARTED 4
#define DCMENU_GREETED 8
#define DCMENU_ITEM_LINE 0x10      /* a plate's own line was started */

/* Lines of the guide's voice set (DC_VOICE_BASE) */
#define DCLINE_INTRO 0             /* 0..4: the first visit's explanation */
#define DCLINE_HELLO 5             /* greeting on later visits, then one of 6..9 */
#define DCLINE_ASK 10              /* "what will you do?", then the plate's line */
#define DCLINE_PASSWORD 11         /* plate 0, then one of 12..14 */
#define DCLINE_REPLAY 15           /* plate 2, then one of 16..18 */
#define DCLINE_LIST 22             /* plate 1, then one of 23..25 */

/* ---- DcPass (src/menu/menu_z_d.c): the password screen (mode 54). ---- */

#define DCPASS_FLASH_NUM 2
#define DCPASS_PAGES 2          /* capitals / small letters */
#define DCPASS_ROWS 5           /* four rows of keys and the row of three buttons */
#define DCPASS_COLS 14
#define DCPASS_TEXT_LEN 0x22    /* 34 cells: two lines of 17 */
#define DCPASS_TEXT_LAST 0x21   /* index of the last of the 34 password characters */
#define DCPASS_LIST_ROWS 3      /* rows of the "which slot" list that the cursor can be on */
#define DCPASS_FACE_FILE DISC_RESOURCE(0x2F9, 0x71C)
#define DCPASS_FACE_SIZE 0x16800

/* Codes in the key table that are not characters: where they are ... */
#define DCKEY_BUTTON_L 0        /* bottom row, columns 0..3 */
#define DCKEY_BUTTON_M 1        /* bottom row, columns 4..9 */
#define DCKEY_BUTTON_R 2        /* bottom row, columns 10..13 */
#define DCKEY_WIDE_E 3          /* row 3, columns 12..13 */
#define DCKEY_ZERO 4            /* row 3, columns 0..1: DcPass_GetKey gives '0' for it */
#define DCKEY_WIDE_D1 5         /* rows 1 and 2, column 12 */
#define DCKEY_WIDE_D2 6         /* rows 1 and 2, column 13 */
/* ... and what they do (DcPass_Input; the same codes) */
#define DCPASS_KEY_QUIT 0
#define DCPASS_KEY_PAGE 1
#define DCPASS_KEY_OK 2
#define DCPASS_KEY_BACK 3
#define DCPASS_KEY_LEFT 5
#define DCPASS_KEY_RIGHT 6

/* A cell of the on-screen keyboard; the keyboard's cursor. */
typedef struct DcKeyPos {
    /* 0x00 */ s32 page;        /* which of the keyboard's character sets is shown */
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 col;
} DcKeyPos; /* 0xC */

/* The text being typed. */
typedef struct DcPassText {
    /* 0x00 */ char text[DCPASS_TEXT_LEN + 1]; /* as shown: 34 cells, ' ' = empty, then a terminator */
    /* 0x23 */ char pass[0x25];     /* the same up to the first empty cell: what the decoders get */
    /* 0x48 */ s32 cursor;          /* cell the next character goes to, 0..33 */
    /* 0x4C */ s32 len;             /* strlen(pass): 32 = the previous game's format, 34 = this game's */
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ s32 unk58;
} DcPassText; /* 0x5C */

/* The slot list shown after a password was accepted: fourteen slots, four plates on screen. */
typedef struct DcPassList {
    /* 0x00 */ s32 top;         /* first slot shown, 0..11 */
    /* 0x04 */ s32 cursor;      /* row the cursor is on, 0..2 */
    /* 0x08 */ s32 extra;       /* slot shown on the fourth plate while the list scrolls */
    /* 0x0C */ s32 chara[SAVE_REC_COUNT]; /* character id of each saved custom character, -1 = free */
} DcPassList; /* 0x44 */

/* A cell of the character grid (ChrGrid): only the id is used here. */
typedef struct DcGridCell {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 unk4[8];
} DcGridCell; /* 0x24 */

/* Character entry of common file 4, section 1 (ChrTblEntry in battle/view_b.h; local view). */
typedef struct ZChrEntry {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ u16 flags;
    /* 0x0A */ u8 unkA[0x32];
} ZChrEntry; /* 0x3C */

/* The part of the work that the drawing helpers take. */
typedef struct DcPassView {
    /* 0x000 */ MFlash flash[DCPASS_FLASH_NUM]; /* 0 the keyboard (section 17), 1 the new character / status
                                                   page (section 18) */
    /* 0x058 */ MTexRes *bg;        /* section 10 */
    /* 0x05C */ u8 *tex0[30];
    /* 0x0D4 */ u8 *tex1[37];       /* [7]: the character's large picture */
    /* 0x168 */ s32 blink;
    /* 0x16C */ s32 talk;
    /* 0x170 */ void *msgText;      /* section 8 */
    /* 0x174 */ void *dialogMsg;    /* section 21 */
    /* 0x178 */ void *subtitles;    /* section 9 */
    /* 0x17C */ u8 unk17C[0x3C];    /* passed to MsgWin_Init, which does not take it */
    /* 0x1B8 */ MTextBox nameBox[DCPASS_LIST_ROWS];
    /* 0x35C */ MTextBox formBox[DCPASS_LIST_ROWS];
    /* 0x500 */ MTextBox nameBoxB;  /* the fourth plate */
    /* 0x58C */ MTextBox formBoxB;
    /* 0x618 */ MTextBox nameBoxL;  /* "mc_name_text_l" */
    /* 0x6A4 */ MTextBox formBoxL;
    /* 0x730 */ MTextBox nameBoxR;  /* "mc_name_text_r" */
    /* 0x7BC */ MTextBox formBoxR;
    /* 0x848 */ void *nameText;     /* section 13: character names */
    /* 0x84C */ void *formText;     /* section 14: form names */
    /* 0x850 */ f32 scroll;         /* backdrop pattern offset */
} DcPassView; /* 0x854 */

typedef struct DcPass {
    /* 0x000 */ void *pack;         /* this screen's section of archive 8 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 22 sections */
    /* 0x008 */ void *faceFile;     /* 0x16800 bytes: file 0x2F9 + character, compressed */
    /* 0x00C */ MTexRes *faceRes;   /* 0x20800 bytes: the same unpacked */
    /* 0x010 */ s32 section;
    /* 0x014 */ DcPassView view;
    /* 0x868 */ DcPassText text;
    /* 0x8C4 */ s32 result;
    /* 0x8C8 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x8CC */ s32 movie;          /* which of the two movies is shown */
    /* 0x8D0 */ u32 state;          /* DCPASS_ST_ */
    /* 0x8D4 */ DcKeyPos keyPos;    /* the keyboard's cursor */
    /* 0x8E0 */ DcPassList list;
    /* 0x924 */ ZStatus status;     /* the character the password decodes to */
    /* 0x95C */ s32 timer;
    /* 0x960 */ s32 unk960;
    /* 0x964 */ s32 flags;          /* DCPASS_ */
    /* 0x968 */ ZItemEntry *itemTbl; /* section 12 */
    /* 0x96C */ DcGridCell *grid;   /* section 20 + 0x10: the character select order (the unlocked characters) */
    /* 0x970 */ s32 gridCount;      /* first word of section 20 */
    /* 0x974 */ u32 nextState;      /* state the dialog leads to when it has closed */
    /* 0x978 */ f32 faceAlpha;
} DcPass; /* 0x97C */

#define DCPASS_ST_TYPE 0        /* typing the password */
#define DCPASS_ST_FAILED 1      /* the guide says the password is wrong */
#define DCPASS_ST_SHOWN 2       /* the new character is shown */
#define DCPASS_ST_LIST 3        /* choosing the slot to store it in */
#define DCPASS_ST_ASK_QUIT 4    /* "give the character up?" */
#define DCPASS_ST_ASK_OVER 5    /* "overwrite this slot?" */
#define DCPASS_ST_STORED 6      /* the guide's line after storing */

#define DCPASS_LEAVE 1
#define DCPASS_LEAVING 2
#define DCPASS_STARTED 4
#define DCPASS_FACE_CHANGE 8    /* a password was decoded: request the character's picture */
#define DCPASS_FACE_LOADING 0x10
#define DCPASS_FACE_READY 0x20

/* DcList (src/menu/menu_y_b.c) */
extern void PassWin_OpenEx(ZSaveRec *rec, s32 chara, s32 level);
void DcView_InitBlink(DcView *v);
void DcChars_Load(DcChars *c);
void DcStatus_Calc(DcStatus *s, ZItemEntry *table);
void DcList_Reset(DcList *d);
void DcList_UpdateGuide(DcList *d);
void DcList_ScrollCloud(DcList *d);
void DcList_SetListScissor(void);
void DcList_ResetScissor(void);
void DcView_SetRowCallbacks(DcView *v);
void DcView_HideArrow(DcView *v, s32 up);
void DcView_SetArrows(DcView *v, DcChars *c);
void DcView_SetScrollBar(DcView *v, DcChars *c);
void DcView_LightRow(DcView *v, DcChars *c, s32 on);
s32 DcChars_ClampTop(DcChars *c);
s32 DcChars_ClampRow(DcChars *c);
void DcView_SetRows(DcView *v, DcChars *c);
s32 DcChars_GetCursor(DcChars *c);
ZSaveRec DcChars_GetRec(DcChars *c);
void DcList_PickRec(DcList *d);
void DcList_RowOk(DcList *d);
void DcList_DrawList(DcList *d);
void DcList_InputList(DcList *d);
void DcView_SetMenuText(DcView *v);
void DcView_SetStatus(DcView *v, DcStatus *s);
void DcView_LightMenu(DcView *v, s32 item, s32 on);
void DcList_WrapMenu(s32 *cursor);
void DcView_HideNamePlates(DcView *v);
void DcView_SetNameText(DcView *v, s32 line);
void DcView_MenuOk(DcView *v, s32 item);
s32 DcRec_IsSlotEmpty(u16 *items, s32 slot);
void DcView_LightItem(DcView *v, s32 row, s32 on);
void DcList_InputMenu(DcList *d);

/* Item details page (src/menu/menu_v_c.c). */
extern void ItemHelp_Init(void *pack);
extern void ItemHelp_Term(void);
extern void ItemHelp_Draw(s32 item);
extern void ItemHelp_Open(void);
extern void ItemHelp_Close(void);

/* Next chunk (stem menu_za): the password window of the list, the replay menu, the save prompt. */
extern s32 DcPass_Run(s32 section);
extern void PassWin_Init(void *pack);
extern void PassWin_Term(void);
extern void PassWin_Draw(void);
extern void PassWin_Close(void);
extern void PassChk_Init(u32 *pack);
extern void PassChk_Term(void);
extern void PassChk_ConvertOld(void *out, void *old);   /* converts a decoded old password to the current content */
extern s32 PassChk_IsOldValid(void *old);               /* validates a decoded old password */
extern s32 PassChk_IsValid(void *data);              /* validates a decoded password */
extern s32 ReplayMenu_Run(s32 section);
extern void DcSave_Init(void *pack);
extern void DcSave_Term(void);
extern void DcSave_Start(void);
extern void DcSave_Update(void);
extern s32 DcSave_GetState(void);
extern s32 DcSave_IsDone(void);
extern s32 DcSave_IsStarted(void);

s32 DcList_Run(s32 section);
s32 Dc_Main(void);
s32 DcMenu_Run(s32 section);

#endif
