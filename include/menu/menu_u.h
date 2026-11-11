#ifndef MENU_MENU_U_H
#define MENU_MENU_U_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#define Rand_Range Rand_Range_menuA
#include "menu/menu_a.h"
#undef Snd_PlaySe
#undef Rand_Range
#include "sys/pad.h"
#include "sys/common.h"
extern s32 Snd_PlaySe(u32 mask, s32 id);
/* menu_a.h has u32 Rand_Range(u32); the shell game (menu_u_d.c) only matches with a signed result. */
extern s32 Rand_Range(s32 n);

/* The day screen's work area (mode 22): a local view of SimDay (include/menu/menu_q.h has the full layout). */
typedef struct USimDay {
    /* 0x000 */ u8 unk0[0x20];
    /* 0x020 */ MFlash flash[7];    /* [4] = the movie of training 2 and of the mini games */
    /* 0x154 */ u8 unk154[0x3FC];
    /* 0x550 */ s32 flags;
    /* 0x554 */ s32 unk554;
    /* 0x558 */ s32 cur[13];
    /* 0x58C */ s32 timer;
    /* 0x590 */ s32 state;
    /* 0x594 */ s32 prevState;
    /* 0x598 */ s32 unk598[2];
    /* 0x5A0 */ s32 msgLine;
    /* 0x5A4 */ s32 menuText;
    /* 0x5A8 */ s32 bgm;
    /* 0x5AC */ s32 unk5AC;
    /* 0x5B0 */ s32 day;
    /* 0x5B4 */ s32 preview[5];
    /* 0x5C8 */ u8 unk5C8[0x59C];
    /* 0xB64 */ s32 event;
    /* 0xB68 */ s32 seq;
    /* 0xB6C */ s32 seqTimer;
    /* 0xB70 */ s32 lastEvent;
    /* 0xB74 */ s32 repeat;
    /* 0xB78 */ s32 faceA;
    /* 0xB7C */ s32 faceB;
    /* 0xB80 */ s32 rank[3];
    /* 0xB8C */ s32 unkB8C;         /* answer of the yes / no window (SimDay_Cmd(0x11)) */
} USimDay;

/* One rank of a training (SimTrain of include/menu/menu_q.h). */
typedef struct USimTrain {
    /* 0x00 */ s32 param;       /* rock game: rocks thrown; = hits needed to win */
    /* 0x04 */ s32 speed;       /* rock game: the rock moves speed * 3 + 3 a frame */
    /* 0x08 */ s32 points;
    /* 0x0C */ s32 gain;
    /* 0x10 */ s32 loss;
    /* 0x14 */ s32 turn;
} USimTrain; /* 0x18 */

extern USimTrain gSimTrain1[5];   /* 0x3B9088 */
extern USimTrain gSimTrain2[5];   /* 0x3B9100 */

extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void StreamSe_Stop(s32 se);
extern void Bgm_Stop(void);
extern void SimDay_PlayBgm(void);

/* Work of the rock game (event 32). Uninitialised globals: they sit in the main executable's .bss. */
extern s32 gSimRockX;       /* 0x31EAFC */
extern s32 gSimRockSpeed;   /* 0x31EB00 */
extern s32 gSimRockLeft;    /* 0x31EB04 */
extern s32 gSimRockHits;    /* 0x31EB08 */
extern u8 gSimRockFlags;    /* 0x31EB0C: 1 = swinging, 2 = won */
extern s32 gSimRockRank;    /* 0x31EB10 */

/* Work of the shell game (event 34), in the main executable's .bss like the rock game's. */
extern s32 gSimPopoTarget;  /* 0x31EB14: the figure that holds the mark */
extern s32 gSimPopoState;   /* 0x31EB18: phase of a swap */
extern s32 gSimPopoDx;      /* 0x31EB1C */
extern s32 gSimPopoDy;      /* 0x31EB20 */
extern s32 gSimPopoSpeed;   /* 0x31EB24: index of the speed table */
extern s32 gSimPopoA;       /* 0x31EB28: the pair being swapped, A < B */
extern s32 gSimPopoB;       /* 0x31EB2C */
extern s32 gSimPopoCount;   /* 0x31EB30: swaps done */
extern s32 gSimPopoWon;     /* 0x31EB34 */
extern s32 gSimPopoRank;    /* 0x31EB38 */

extern s32 SimDay_CountItems2(void);
extern void SimDay_TakeItem(void);

s32 SimPopo_Swap(USimDay *day, s32 a, s32 b);
void SimPopo_CursorOn(USimDay *day);
void SimPopo_CursorOff(USimDay *day);

extern void SimDay_AddChange(s32 stat, s32 amount);
extern s32 SimDay_GetStat(s32 stat, s32 preview);
extern void SimDay_GiveItem(void);
extern void SimDay_Cmd(s32 cmd);

/* ---- EvoZ (menu_u_g.c): the character customising screen of mode 49. The object goes on in the next chunk
 * (EvoZ_Load, 0x395E30, is the last function of this source file); include/menu/menu_v.h has that chunk's view
 * of the same work area. Everything below is a local view. ---- */

extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetRect(MTextBox *box, s32 x0, s32 x1, s32 y0, s32 y1);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h,
                          s32 mode, s32 parentFmt);
extern void Flash_ClipSetScale(MFlash *flash, MFlashRef *ref, f32 x, f32 y);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern s32 ChrTbl_GetMaxExp(s32 chara);
extern void ItemHelp_Term(void);
extern void ItemHelp_Draw(s32 item);
extern void PassWin_Term(void);   /* next chunks: term of a module this screen shares */
extern void PassWin_Draw(void);   /* ... and its draw */
extern void PassWin_Init(void *pack);
extern void ItemHelp_Init(u32 *pack);
extern void Dialog_SetLayout(s32 layout);

/* Item entry of common file 4 (ItemTblEntry in battle/view_b.h). */
typedef struct UItemEntry {
    /* 0x00 */ u8 type;         /* 0..2: tab of the item list; column of the kind icon */
    /* 0x01 */ u8 unk1[2];
    /* 0x03 */ u8 slots;        /* item slots it takes; 0 = no cost icon */
    /* 0x04 */ u8 unk4[0x10];
    /* 0x14 */ s32 flags;       /* 5: never listed; 0x40: appears in the lists */
    /* 0x18 */ u8 unk18[0x10];
} UItemEntry; /* 0x28 */

extern s32 ItemTbl_GetClass(s32 item, UItemEntry *table);

/* A cell of the character grid. */
typedef struct UChrCell {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} UChrCell; /* 0x24 */

extern void ChrGrid_Build(s32 *outCount, UChrCell *out, s32 *inCount, UChrCell *in, s32 *customCount,
                          UChrCell *custom);
extern s32 ChrGrid_IsSelectable(UChrCell *cells, s32 index);

/* The grid list of a screen pack: count, then the cells from 0x10 (VChrGridList of menu_v.h). */
typedef struct UChrGridList {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ UChrCell cell[1];
} UChrGridList;

/* The pick record kept in gProgress + 0x440 (the same record the character selects keep there). */
typedef struct UPick {
    /* 0x00 */ s32 col;
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 unk8[10];
} UPick; /* 0x30 */

typedef struct UProgress {
    /* 0x000 */ u8 unk0[0x14];
    /* 0x014 */ s32 flags;
    /* 0x018 */ u8 unk18[0x428];
    /* 0x440 */ UPick pick;
} UProgress;

#define UPROG ((UProgress *)gProgress)

/* The screen's cursor record: the pick, then what the reel shows. */
typedef struct UEvoZSide {
    /* 0x00 */ UPick pick;
    /* 0x30 */ s32 chip[7];        /* character ids on the seven chips of the reel */
    /* 0x4C */ s32 prevChip[7];
    /* 0x68 */ s32 flags;          /* 2 = the portrait is loaded */
    /* 0x6C */ s32 chara;          /* character under the cursor */
    /* 0x70 */ s32 mask;           /* non-zero while the chips are hidden for a reel change */
} UEvoZSide; /* 0x74 */

#define UEVOZ_ITEM_MAX 350
#define UEVOZ_TABS 4               /* all items, then one tab per item type */

/* The item list (same layout as the shop's lists, ShopList in menu_v.h). */
typedef struct UEvoZList {
    /* 0x0000 (0x1A98) */ s32 cur[UEVOZ_TABS];
    /* 0x0010 (0x1AA8) */ s32 tab;
    /* 0x0014 (0x1AAC) */ s32 rows;       /* rows on screen: 6 */
    /* 0x0018 (0x1AB0) */ f32 rowsF;      /* 6.16: the scroll bar's span */
    /* 0x001C (0x1AB4) */ s32 extra;      /* list index shown on the eighth plate (the row scrolling in or out) */
    /* 0x0020 (0x1AB8) */ s32 count[UEVOZ_TABS];
    /* 0x0030 (0x1AC8) */ s32 ids[UEVOZ_TABS][UEVOZ_ITEM_MAX]; /* 0-based item numbers */
    /* 0x1610 (0x30A8) */ s32 top[UEVOZ_TABS];
    /* 0x1620 (0x30B8) */ s32 bottom[UEVOZ_TABS];
} UEvoZList; /* 0x1630 */

#define UEVOZ_FLASH_NUM 3
#define UEVOZ_BOX_NUM 18

typedef struct UEvoZ {
    /* 0x0000 */ u32 *pack;
    /* 0x0004 */ u32 *res;
    /* 0x0008 */ void *faceFile;        /* 0x16800 bytes: compressed portrait (file 0x71C + character) */
    /* 0x000C */ MTexRes *faceRes;      /* 0x20800 bytes: the same unpacked */
    /* 0x0010 */ void *chipFile;
    /* 0x0014 */ MTexRes *chipRes;
    /* 0x0018 */ u32 *chipPack;
    /* 0x001C */ void *dialogText;
    /* 0x0020 */ void *text[3];         /* character names, form names, item names */
    /* 0x002C */ MFlash flash[UEVOZ_FLASH_NUM]; /* screen, character reel, item / slot list */
    /* 0x00B0 */ void *bg;              /* background picture */
    /* 0x00B4 */ u8 *tex0[43];          /* [42] = the portrait */
    /* 0x0160 */ u8 *tex1[25];
    /* 0x01C4 */ u8 *tex2[26];
    /* 0x022C */ s32 flags;             /* UEVOZ_ */
    /* 0x0230 */ s32 unk230[2];
    /* 0x0238 */ s32 setCur;            /* item set shown */
    /* 0x023C */ s32 slot;              /* cursor of the slot list */
    /* 0x0240 */ s32 unk240;
    /* 0x0244 */ s32 zpDigit;           /* digit under the cursor of the points page */
    /* 0x0248 */ s32 unk248[2];
    /* 0x0250 */ s32 action;            /* 0..3: what confirm does in the slot list */
    /* 0x0254 */ s32 unk254[2];
    /* 0x025C */ s32 leaveTimer;
    /* 0x0260 */ s32 state;
    /* 0x0264 */ s32 faceState;         /* the portrait loader's step; 4 = idle */
    /* 0x0268 */ s32 listOpen;          /* the third movie shows the item list (else the slot list) */
    /* 0x026C */ s32 helpItem;          /* item of the details page */
    /* 0x0270 */ s32 pay;               /* Z points still to be turned into experience */
    /* 0x0274 */ s32 set;               /* item set being edited, 0..2 */
    /* 0x0278 */ s32 unk278;
    /* 0x027C */ s32 curItem;           /* item id in the slot being changed, 0 = empty */
    /* 0x0280 */ s32 used;              /* item slots the set takes */
    /* 0x0284 */ s32 dialogState;       /* 0 = no dialog */
    /* 0x0288 */ s32 unk288[6];
    /* 0x02A0 */ UEvoZSide sideRec;
    /* 0x0314 */ UEvoZSide *side;       /* = &sideRec */
    /* 0x0318 */ s32 gridCount;
    /* 0x031C */ UChrCell *grid;        /* pack section 33 until Init, then cells */
    /* 0x0320 */ s32 cellCount;
    /* 0x0324 */ UChrCell cells[165];   /* the grid as ChrGrid_Build filters it */
    /* 0x1A58 */ s32 rows;              /* rows of the grid (7 columns) */
    /* 0x1A5C */ f32 faceAlpha;         /* fades the portrait in once it is loaded */
    /* 0x1A60 */ s32 capacity;          /* item slots of the character */
    /* 0x1A64 */ s32 fits;              /* slots still free */
    /* 0x1A68 */ s32 kind;
    /* 0x1A6C */ s32 exp;
    /* 0x1A70 */ s32 nextExp;           /* experience of the next level, 0 at the last level */
    /* 0x1A74 */ s32 expLeft;
    /* 0x1A78 */ s32 bonus[5];          /* [0] 0..7 marks; [1..4] -3..3 marks */
    /* 0x1A8C */ f32 expBar;
    /* 0x1A90 */ s32 unk1A90[2];
    /* 0x1A98 */ UEvoZList list;
    /* 0x30C8 */ UItemEntry *items;     /* item table of common file 4 */
    /* 0x30CC */ MTextBox box[UEVOZ_BOX_NUM]; /* [0] name, [1] form, [2] changed slot, [3..9] list rows, [10..17] slots */
    /* 0x3AA4 */ s32 seTimer;           /* ticks of the counting sound while points are paid */
} UEvoZ; /* 0x3AA8 */

#define UEVOZ_LEAVING 2
#define UEVOZ_PAYING 0x20         /* points are being turned into experience, 33 a frame */
#define UEVOZ_LIMIT_UP 0x40       /* the "limit up" animation runs */
#define UEVOZ_LIST_BUSY 0x80      /* the list movie is moving: no input */
#define UEVOZ_REEL_READY 0x100    /* the reel's intro is over */

/* gSaveData as this screen uses it (include/sys/save.h has the full layout). */
typedef struct USaveCustom {
    /* 0x00 */ u16 set[3][8];   /* three item sets, ids 1-based, 0 = empty */
    /* 0x30 */ s32 exp;
    /* 0x34 */ u16 level;
    /* 0x36 */ u16 unk36;
} USaveCustom; /* 0x38 */

typedef struct USave {
    /* 0x0000 */ u8 unk0[0x1808];
    /* 0x1808 */ USaveCustom custom[97];  /* by cell of the character grid (col + row * 7) */
    /* 0x2D40 */ u8 unk2D40[0x188];
    /* 0x2EC8 */ u8 item[350];            /* bit 0 owned, bit 1 new */
    /* 0x3026 */ u8 unk3026[2];
    /* 0x3028 */ s32 money;               /* Z points */
} USave;

extern USave *gSaveData;

extern UEvoZ *gEvoZ;   /* 0x3BB140 */

/* menu_u_h.c (its last function; was src/menu/menu_v.c) */
void EvoZ_Load(UEvoZ *ez, s32 section);

/* next chunk (menu_v_b.c) */
extern void EvoZ_Input(UEvoZ *ez, s32 *result);
extern void EvoZ_UpdateDialog(UEvoZ *ez);
extern void EvoZ_ToggleList(UEvoZ *ez);
extern s32 EvoZ_GetLevelExp(UEvoZ *ez, s32 chara, s32 cell);
extern void EvoZ_RefreshStatus(UEvoZ *ez);
extern void EvoZ_SetRowChips(UEvoZ *ez);
extern void EvoZ_UpdateFace(UEvoZ *ez);
extern s32 EvoZ_CanEquip(UEvoZ *ez, s32 item);

void EvoZ_Init(s32 section);
void EvoZ_Term(void);
void EvoZ_Draw(void);
void EvoZ_Update(void);
s32 EvoZ_Run(s32 section);
void EvoZ_SetListScissor(void);
void EvoZ_ResetScissor(void);
void EvoZ_Refresh(UEvoZ *ez);

#endif
