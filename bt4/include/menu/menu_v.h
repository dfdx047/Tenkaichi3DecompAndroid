#ifndef MENU_MENU_V_H
#define MENU_MENU_V_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#include "sys/pad.h"
#include "sys/common.h"
#include "sys/save.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x395E30..0x39A978 (placeholder stem "menu_v"): part of the Evolution Z mode group
 * (progress modes 48..50, handler EvoZ_Main 0x39E940 in the next chunk: 48 top menu, 49 the customising
 * screen, 50 the shop). Four files, cut at object boundaries:
 *
 *   menu_v.c    0x395E30..0x396838  EvoZ_Load: the LAST function of the customising screen's first source file
 *                                   (0x392F10..0x396838, the rest is in src/menu/menu_u*.c)
 *                                   -- now appended to src/menu/menu_u_h.c (object 0x393C58..0x396838)
 *   menu_v_b.c  0x396838..0x399240  EvoZ, second source file: input, dialog, status, edits of the saved sets
 *   menu_v_c.c  0x399240..0x399790  ItemHelp: the item details page shared by many screens
 *   menu_v_d.c  0x399790..0x39A978  Shop: head of the item shop object (the rest is in src/menu/menu_w*.c)
 *
 * The structures below are local views; include/menu/menu_u.h (UEvoZ) and include/menu/menu_w.h (Shop) describe
 * the same work areas from the neighbouring chunks.
 */

/* Item entry of common file 4, section 2 (ItemTblEntry in battle/view_b.h; local view). */
typedef struct VItemEntry {
    /* 0x00 */ u8 type;         /* column of the item-kind icon */
    /* 0x01 */ u8 group;        /* two items of the same type and group cannot be in one set */
    /* 0x02 */ u8 picture;      /* picture of the details page: texture number, also cell (bit 0, the rest) of the icon sheet */
    /* 0x03 */ u8 slots;        /* how many item slots it takes (0 = no cost icon) */
    /* 0x04 */ u8 unk4[4];
    /* 0x08 */ u32 stockLevel;  /* the shop sells it once the save's shop level reaches this */
    /* 0x0C */ u8 unkC[8];
    /* 0x14 */ s32 flags;       /* VITEM_ */
    /* 0x18 */ u8 unk18[0x10];
} VItemEntry; /* 0x28 */

#define VITEM_HIDDEN 5        /* either bit: never listed */
#define VITEM_SOLD 0x20       /* the shop sells it */
#define VITEM_LISTED 0x40     /* appears in the item lists */
#define VITEM_UNCOUNTED 0x100  /* not counted for the collection percentage */

extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern s32 ItemTbl_GetClass(s32 item, VItemEntry *table);

#define VLIST_TABS 4
#define VLIST_ITEM_MAX 350

/* An item list with its four tabs (used by the customising screen and by the shop). */
typedef struct VItemList {
    /* 0x0000 */ s32 cur[VLIST_TABS];     /* cursor (list index) of each tab */
    /* 0x0010 */ s32 tab;                 /* 0 = all items, 1..3 = one item type each (the customising screen: 0..2 types, 3 = the last row's list) */
    /* 0x0014 */ s32 rows;                /* rows on screen */
    /* 0x0018 */ f32 rowsF;               /* guess: the scroll bar's span */
    /* 0x001C */ s32 edge;                /* top or bottom index before the last scroll */
    /* 0x0020 */ s32 count[VLIST_TABS];
    /* 0x0030 */ s32 ids[VLIST_TABS][VLIST_ITEM_MAX]; /* 0-based item numbers */
    /* 0x1610 */ s32 top[VLIST_TABS];     /* first visible list index */
    /* 0x1620 */ s32 bottom[VLIST_TABS];  /* last visible list index */
} VItemList; /* 0x1630 */


/* ---- EvoZ (menu_v.c, menu_v_b.c): the character customising screen. Its first file is in the previous chunk ---- */

/* Character entry of common file 4, section 1 (ChrTblEntry in battle/view_b.h; local view). */
typedef struct VChrEntry {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ u16 flags;
    /* 0x0A */ u16 costumes;
    /* 0x0C */ u16 cost;
    /* 0x0E */ u16 baseLevel;
    /* 0x10 */ s32 exp[7];      /* experience needed for each level */
    /* 0x2C */ u8 unk2C[0x10];
} VChrEntry; /* 0x3C */

/* A cell of the character grid (include/battle/view_a.h has the original). */
typedef struct VChrCell {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} VChrCell; /* 0x24 */

/* The grid list of a screen pack: count, then the cells from 0x10. */
typedef struct VChrGridList {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ VChrCell cell[1];
} VChrGridList;

/* An item set: eight item ids (1-based, 0 = empty). */
typedef struct VItemSet {
    u16 id[8];
} VItemSet;

/* gSaveData as this file's address arithmetic needs it (see MSave in include/menu/menu_c.h). */
typedef struct VSaveCustom {
    /* 0x00 */ VItemSet set[3];
    /* 0x30 */ s32 exp;
    /* 0x34 */ u16 level;
    /* 0x36 */ u16 unk36;
} VSaveCustom; /* 0x38 */

typedef struct VSaveBody {
    /* 0x0008 */ u8 unk8[0x1800];
    /* 0x1808 */ VSaveCustom custom[97];  /* by cell of the character grid (col + row * 7) */
    /* 0x2D40 */ u8 unk2D40[0x188];
    /* 0x2EC8 */ u8 item[350];
    /* 0x3026 */ u8 unk3026[2];
    /* 0x3028 */ s32 money;
} VSaveBody;

typedef struct VSave {
    /* 0x00 */ s32 sum[2];
    /* 0x08 */ VSaveBody body;
} VSave;

#define VSAVE (&((VSave *)gSaveData)->body)

/* The cursor record (same layout as a character select's side record). */
typedef struct EvoZSide {
    /* 0x00 */ s32 col;
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 unk8[10];
    /* 0x30 */ s32 chip[7];        /* character ids on the seven chips of the reel */
    /* 0x4C */ s32 prevChip[7];
    /* 0x68 */ s32 flags;          /* EVOZ_SIDE_ */
    /* 0x6C */ s32 chara;          /* character under the cursor */
    /* 0x70 */ s32 mask;           /* non-zero while the chips are hidden for a reel change */
} EvoZSide; /* 0x74 */

#define EVOZ_SIDE_FACE_CHANGE 1    /* the portrait must be reloaded */
#define EVOZ_SIDE_FACE_READY 2     /* the portrait is loaded */

/* The confirmation dialog's state. */
typedef struct EvoZDialog {
    /* 0x00 */ s32 state;          /* 0 = closed; 1..4 "remove every item?", 5..8 "pay the points?" */
    /* 0x04 */ s32 answer;
    /* 0x08 */ s32 timer;
    /* 0x0C */ s32 msg;
} EvoZDialog;

#define EVOZ_FLASH_NUM 3

typedef struct EvoZ {
    /* 0x0000 */ u32 * pack;  /* this screen's section of archive 7 (compressed) */
    /* 0x0004 */ u32 * res;  /* the same unpacked: a pack of 40 sections */
    /* 0x0008 */ void * faceFile;  /* 0x16800 bytes: compressed portrait (file 0x71C + character) */
    /* 0x000C */ MTexRes * faceRes;  /* the same unpacked */
    /* 0x0010 */ void * chipFile;  /* file 0x3C0 (compressed) */
    /* 0x0014 */ MTexRes * chipRes;  /* the same unpacked */
    /* 0x0018 */ u32 * chipPack;  /* section 36: one texture list per character chip */
    /* 0x001C */ void * dialogText;  /* section 39 */
    /* 0x0020 */ void * text[3];  /* sections 34, 35, 37 */
    /* 0x002C */ MFlash flash[EVOZ_FLASH_NUM];  /* sections 13, 23, 31: screen, character reel, item list */
    /* 0x00B0 */ MTexRes * chipTex;
    /* 0x00B4 */ u8 * tex0[43];  /* [42] = the portrait */
    /* 0x0160 */ u8 * tex1[25];  /* [0..6] the reel chips, [18..24] the chips before the last change */
    /* 0x01C4 */ u8 * tex2[26];
    /* 0x022C */ s32 flags;  /* EVOZ_ */
    /* 0x0230 */ s32 cur[11];  /* one cursor per state: [1] menu row, [2] item set plate, [3] slot row (7 = the last row), [5] digit of the points page; [8] action of the slot list */
    /* 0x025C */ s32 leaveTimer;
    /* 0x0260 */ s32 state;  /* EVOZ_ST_ */
    /* 0x0264 */ s32 faceState;  /* the portrait loader's step */
    /* 0x0268 */ s32 listSide;  /* 0 = slot list shown, 1 = item list shown */
    /* 0x026C */ s32 helpItem;  /* item (0-based) the details page shows */
    /* 0x0270 */ s32 zp;  /* Z points chosen on the points page; while paying, what is still to be turned into experience */
    /* 0x0274 */ s32 set;  /* item set being edited, 0..2 */
    /* 0x0278 */ s32 lastTab;  /* tab the item list was left on */
    /* 0x027C */ s32 curItem;  /* item id in the slot being changed (0 = empty) */
    /* 0x0280 */ s32 rowsUsed;  /* slot rows that can be reached: items kept plus free slots (ItemSet_Fit) */
    /* 0x0284 */ EvoZDialog dialog;
    /* 0x0294 */ s32 unk294[3];
    /* 0x02A0 */ EvoZSide sideRec;  /* its first 0x30 bytes are kept in gProgress + 0x440 between visits */
    /* 0x0314 */ EvoZSide * side;  /* = &sideRec */
    /* 0x0318 */ s32 gridCount;
    /* 0x031C */ VChrCell * grid;  /* section 33: the character grid (EvoZ_Load); EvoZ_Init then points it at cells */
    /* 0x0320 */ s32 cellCount;
    /* 0x0324 */ VChrCell cells[165];  /* the grid as ChrGrid_Build filters it (previous chunk) */
    /* 0x1A58 */ s32 rows;  /* rows of the character grid */
    /* 0x1A5C */ f32 faceAlpha;
    /* 0x1A60 */ s32 capacity;  /* item slots of the character (ChrTbl_GetLevel) */
    /* 0x1A64 */ s32 fits;  /* item slots the set takes (result of ItemSet_Fit) */
    /* 0x1A68 */ s32 kind;  /* bit 0 of the character entry's flags, inverted */
    /* 0x1A6C */ s32 exp;
    /* 0x1A70 */ s32 nextExp;  /* experience of the next level (0 at the last level) */
    /* 0x1A74 */ s32 expLeft;
    /* 0x1A78 */ s32 bonus[5];
    /* 0x1A8C */ f32 expBar;
    /* 0x1A90 */ s32 unk1A90[2];
    /* 0x1A98 */ VItemList list;  /* the owned items that can be equipped */
    /* 0x30C8 */ VItemEntry * items;  /* item table of common file 4 */
    /* 0x30CC */ MTextBox box[18];  /* previous chunk */
    /* 0x3AA4 */ s32 seTimer;  /* previous chunk */
} EvoZ; /* 0x3AA8 */

/* EvoZ.flags */
#define EVOZ_STARTED 4         /* the cursor chip was lit once */
#define EVOZ_HELP_SLOT 8       /* the details page was opened from the slot list (else from the item list) */
#define EVOZ_PAYING 0x20       /* the points are being turned into experience (the previous chunk's update pays some every frame) */
#define EVOZ_LIMIT_UP 0x40     /* the "limit up" animation runs */
#define EVOZ_LIST_BUSY 0x80    /* the item list is moving: no input */
#define EVOZ_REEL_READY 0x100  /* set by the previous chunk; cleared when the menu is left for the grid */

/* EvoZ.state */
#define EVOZ_ST_GRID 0
#define EVOZ_ST_MENU 1
#define EVOZ_ST_SLOTS 3
#define EVOZ_ST_PASSWORD 4
#define EVOZ_ST_ZP 5
#define EVOZ_ST_PAYING 6
#define EVOZ_ST_LEVEL_UP 7
#define EVOZ_ST_HELP 9
#define EVOZ_ST_LIST 10

/* kinds of EvoZ_ClipGoto */
#define EVOZ_CLIP_CHIP 0
#define EVOZ_CLIP_MENU 1
#define EVOZ_CLIP_SET 2
#define EVOZ_CLIP_SLOT 3
#define EVOZ_CLIP_ZP 5
#define EVOZ_CLIP_LIST 10

extern void PassWin_Init(void *pack);
extern void PassWin_OpenEx(u16 *ids, s32 chara, s32 level);  /* next chunks: opens the password page of an item set (guess) */
extern void PassWin_Close(void);                           /* closes it */
extern void ChrGrid_MoveLeft(VChrCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveRight(VChrCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveUp(VChrCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_MoveDown(VChrCell *cells, s32 *col, s32 *row, s32 rows);   /* next chunks: initialises the password entry (guess) */
extern void Dialog_Start(s32 kind);
extern s32 Dialog_Input(s32 allowCancel);
extern void Dialog_SetLayout(s32 layout);
extern void Dialog_SetCursor(s32 cursor);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetMsg(s32 idx);
extern s32 Dialog_IsClosed(void);
extern void Dialog_Draw(s32 visible);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern s32 ChrTbl_GetLevel(s32 chara, s32 slot, s32 fromRec);
extern s32 ChrTbl_GetExp(s32 chara, s32 slot);
extern s32 ChrTbl_GetMaxExp(s32 chara);
extern s32 ItemSet_Fit(u16 *ids, VItemEntry *table, s32 capacity, s32 *used);
extern void ItemSet_GetBonus(u16 *ids, VItemEntry *table, s32 *out);
extern s32 ItemTbl_CanEquip(s32 item, s32 chara, VItemEntry *table);

void EvoZ_Load(EvoZ *ez, s32 section);
void EvoZ_ClipGoto(EvoZ *ez, s32 flash, s32 kind, s32 sub, char *label);
void EvoZ_Input(EvoZ *ez, s32 *result);
void EvoZ_UpdateDialog(EvoZ *ez);
void EvoZ_ToggleList(EvoZ *ez);
s32 EvoZ_Pow(s32 base, s32 exp);
s32 EvoZ_GetLevelExp(EvoZ *ez, s32 chara, s32 cell);
void EvoZ_RefreshStatus(EvoZ *ez);
void EvoZ_SetRowChips(EvoZ *ez);
void EvoZ_UpdateFace(EvoZ *ez);
void EvoZ_RequestFace(EvoZ *ez);
s32 EvoZ_CanEquip(EvoZ *ez, s32 item);
void EvoZ_RemoveItem(EvoZ *ez);
void EvoZ_RemoveAllItems(EvoZ *ez);

/* ---- ItemHelp (menu_v_c.c): the item details page shared by every character select and the wish screen ---- */

#define ITEMHELP_FLASH_NUM 1    /* the code loops over the movies although there is one */

typedef struct ItemHelp {
    /* 0x000 */ void *nameText;     /* pack section 6: item names */
    /* 0x004 */ void *descText;     /* pack section 5: item descriptions */
    /* 0x008 */ MFlash flash[ITEMHELP_FLASH_NUM]; /* pack section 2 */
    /* 0x034 */ u8 *tex[12];        /* textures of the movie's image records */
    /* 0x064 */ VItemEntry *items;  /* item table of common file 4 */
    /* 0x068 */ MTextBox name;
    /* 0x0F4 */ MTextBox desc;
} ItemHelp; /* 0x180 */

extern ItemHelp *gItemHelp; /* 0x3BB144 */

void ItemHelp_Init(u32 *pack);
void ItemHelp_Term(void);
void ItemHelp_Draw(s32 item);
void ItemHelp_Open(void);
void ItemHelp_Close(void);

/* ---- Shop (menu_v_d.c): merged with menu_w.c; the Shop structure, gShop and the prototypes are in
 * include/menu/menu_w.h (this header's partial view was removed). ---- */

extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void TextBox_SetRect(MTextBox *box, s32 a, s32 b, s32 c, s32 d);

#endif
