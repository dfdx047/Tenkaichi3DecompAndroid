#ifndef MENU_MENU_W_H
#define MENU_MENU_W_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#include "sys/pad.h"
#include "sys/common.h"
#include "sys/save.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x39A978..0x39EFC0 (placeholder stem "menu_w"): "Evolution Z" (main-menu item 5,
 * progress modes 48..50; the development path in the next object's data is "host:data/ps2/test/main/evoZ/").
 *
 *   menu_w.c    0x39A978..0x39E940  Shop     tail of the item shop object (mode 50); head: src/menu/menu_v_d.c
 *                                            -- now merged into src/menu/menu_v_d.c, which includes this header
 *   menu_w_b.c  0x39E940..0x39EB08  EvoMode_Main, the handler of modes 48..50
 *   menu_w_c.c  0x39EB08..0x39FAA8  EvoTop   the mode's top menu (mode 48); declared in include/menu/menu_x.h
 *
 * The Shop layout below is this chunk's own view (the previous chunk's include/menu/menu_v.h declares the same
 * structure from the head of the object only and was still changing; unify them when the two files are merged:
 * this one has the fields from 0x22C to 0x284 and the ShopList fields at +0x10 / +0x1C worked out).
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetScale(MFlash *flash, MFlashRef *ref, f32 x, f32 y);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void Save_AddItem(s32 idx);
extern void GetWin_Init(void *pack, s32 lang);

/* gProgress + 0x7D0: the plate the Evolution Z top menu was left on (-1 when the mode is entered). */
#define EVO_PROGRESS_CURSOR (*(s32 *)((u8 *)gProgress + 0x7D0))

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* Item entry of common file 4, section 2 (ItemTblEntry in battle/view_b.h; local view). */
typedef struct WItemEntry {
    /* 0x00 */ u8 type;         /* 0..2: column of the item-kind icon, and the list tab (type + 1) */
    /* 0x01 */ u8 unk1;
    /* 0x02 */ u8 picture;
    /* 0x03 */ u8 slots;        /* how many item slots it takes (0 = no cost icon) */
    /* 0x04 */ s32 price;       /* what the shop asks for it */
    /* 0x08 */ u32 stockLevel;
    /* 0x0C */ u8 unkC[8];
    /* 0x14 */ s32 flags;
    /* 0x18 */ u8 unk18[0x10];
} WItemEntry; /* 0x28 */

#define WITEM_HIDDEN 5         /* either bit: never listed */
#define WITEM_SOLD 0x20        /* the shop sells it */
#define WITEM_LISTED 0x40      /* appears in the item lists */
#define WITEM_UNCOUNTED 0x100  /* not counted for the collection percentage */

extern s32 ItemTbl_GetClass(s32 item, WItemEntry *table);
extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetRect(MTextBox *box, s32 a, s32 b, s32 c, s32 d);

/* ---- Shop (0x399790..0x39E940; this chunk has it from 0x39A978) ---- */

#define SHOP_FLASH_NUM 6
#define SHOP_TABS 4            /* all items, then one tab per item type 0..2 */
#define SHOP_ITEM_MAX 350
#define SHOP_BOX_NUM 14
#define SHOP_VOICE_BASE 0x86DB
#define SHOP_GUIDE_LINES 15    /* voice lines per guide: line = guide * 15 + SHOP_LINE_ */

/* MFlash indices */
#define SHOP_FL_MAIN 0         /* backdrop, the two menu plates, money and percentage */
#define SHOP_FL_BUY 1          /* the buy list (page 0) */
#define SHOP_FL_ALL 2          /* the collection list (page 1) */
#define SHOP_FL_GUIDE 3        /* the guide */
#define SHOP_FL_CONFIRM 4      /* the three-row confirmation window */
#define SHOP_FL_COMPLETE 5     /* the "complete" caption */

/* One item list with its four tabs. */
typedef struct ShopList {
    /* 0x0000 */ s32 cur[SHOP_TABS];      /* cursor (list index) of each tab */
    /* 0x0010 */ s32 tab;                 /* the tab shown */
    /* 0x0014 */ s32 rows;                /* rows on screen: 4 (buy list) or 7 (collection) */
    /* 0x0018 */ f32 rowsF;               /* 4.0 / 7.28: height of the scroll bar's track in rows */
    /* 0x001C */ s32 extra;               /* list index shown on the extra plate (rows + 1) while the list scrolls */
    /* 0x0020 */ s32 count[SHOP_TABS];
    /* 0x0030 */ s32 ids[SHOP_TABS][SHOP_ITEM_MAX]; /* 0-based item numbers */
    /* 0x1610 */ s32 top[SHOP_TABS];      /* first visible list index */
    /* 0x1620 */ s32 bottom[SHOP_TABS];   /* last visible list index */
} ShopList; /* 0x1630 */

/* Shop.level: what the pad drives */
#define SHOP_LV_MENU 0         /* the two plates: buy / collection */
#define SHOP_LV_BUY 1          /* the buy list */
#define SHOP_LV_ALL 2          /* the collection list */
#define SHOP_LV_HELP 3         /* the item details page (ItemHelp); cursor[3] holds the level to return to */
#define SHOP_LV_CONFIRM 4      /* the confirmation window: 0 buy, 1 cancel, 2 details */
#define SHOP_LV_BOUGHT 5       /* the "you got it" plate, until confirm is pressed */
#define SHOP_LV_NUM 6

typedef struct Shop {
    /* 0x0000 */ void *pack;          /* this screen's section of archive 7 (compressed) */
    /* 0x0004 */ u32 *res;            /* the same unpacked: a pack of 34 sections */
    /* 0x0008 */ void *msgText;       /* section 32 */
    /* 0x000C */ void *subtitles;     /* section 26 */
    /* 0x0010 */ void *itemText;      /* section 30: item names */
    /* 0x0014 */ MFlash flash[SHOP_FLASH_NUM]; /* sections 6, 17, 19, 9, 29, 33 */
    /* 0x011C */ MTexRes *bg;         /* section 1 */
    /* 0x0120 */ MTexRes *guideRes[2];
    /* 0x0128 */ u8 *tex0[15];
    /* 0x0164 */ u8 *tex3[5];
    /* 0x0178 */ u8 *tex1[21];
    /* 0x01CC */ u8 *tex2[18];
    /* 0x0214 */ u8 *tex4[6];
    /* 0x022C */ u8 *tex5[1];
    /* 0x0230 */ s32 flags;           /* SHOP_ */
    /* 0x0234 */ s32 cursor[SHOP_LV_NUM]; /* by level: [0] menu plate (the "page": 0 buy, 1 collection), [3] the level the
                                         details page returns to, [4] row of the confirmation window */
    /* 0x024C */ s32 timer;           /* frames until the fade out starts once SHOP_LEAVING is set */
    /* 0x0250 */ s32 level;           /* SHOP_LV_ */
    /* 0x0254 */ s32 seq;             /* step of the scripted sequence (Shop_UpdateSeq), 0 = none: the pad is read */
    /* 0x0258 */ s32 voiceLine;       /* subtitle line of the guide's voice, -1 = none */
    /* 0x025C */ s32 guide;           /* which of the two guides talks */
    /* 0x0260 */ s32 blink;
    /* 0x0264 */ s32 talk;
    /* 0x0268 */ s32 idle;            /* frames without input on the menu level; at 3600 the guides swap */
    /* 0x026C */ s32 helpItem;        /* item shown by the details page */
    /* 0x0270 */ s32 item;            /* item just bought (shown on plate 6 of the buy list) */
    /* 0x0274 */ s32 total;           /* collectable items */
    /* 0x0278 */ s32 owned;           /* of those, owned */
    /* 0x027C */ s32 percent;
    /* 0x0280 */ f32 cloud;           /* scroll position of the backdrop clouds */
    /* 0x0284 */ ShopList list[2];    /* [0] what the shop sells now, [1] every item */
    /* 0x2EE4 */ WItemEntry *items;   /* item table of common file 4 */
    /* 0x2EE8 */ MTextBox box[SHOP_BOX_NUM]; /* [0..7] rows of the collection, [8..12] rows of the buy list, [13] plate 6 */
} Shop; /* 0x3690 */

#define SHOP_LEAVING 2         /* never set by this object: Shop_Run would count timer down, then fade out */
#define SHOP_STARTED 4         /* the cursor plate was lit once */
#define SHOP_GREETED 8         /* the greeting was started */
#define SHOP_LIST_BUY 0x10     /* the buy list is drawn (bit 0x10 << list) */
#define SHOP_LIST_ALL 0x20     /* the collection list is drawn */
#define SHOP_MSG_BEHIND 0x80   /* the message window is drawn before the movies instead of after them */

/* Voice lines of one guide (added to guide * SHOP_GUIDE_LINES) */
#define SHOP_LINE_GREET 0
#define SHOP_LINE_BUY 1        /* cursor on "buy" (Shop_PlayVoice) */
#define SHOP_LINE_GREET_NEW 2  /* greeting when the stock has just grown */
#define SHOP_LINE_ALL 3        /* cursor on "collection" */
#define SHOP_LINE_BACK 4       /* a list was left */
#define SHOP_LINE_LIST 5       /* the buy list was opened */
#define SHOP_LINE_CONFIRM 6    /* the confirmation window opened */
#define SHOP_LINE_THANKS 7     /* after a purchase */
#define SHOP_LINE_NO_MONEY 8
#define SHOP_LINE_COMPLETE 9   /* the collection reached 100 % */
#define SHOP_LINE_SOLD_OUT 10  /* everything on sale is owned */
#define SHOP_LINE_OWNED 11     /* the item under the cursor cannot be bought */
#define SHOP_LINE_ALL_OWNED 12 /* "buy" chosen at 100 % */
#define SHOP_LINE_SWAP 13      /* said before the guides swap */
#define SHOP_LINE_BYE 14

extern Shop *gShop; /* 0x3BB148 */

/* ItemHelp (src/menu/menu_v_c.c). */
extern void ItemHelp_Init(u32 *pack);
extern void ItemHelp_Term(void);
extern void ItemHelp_Draw(s32 item);
extern void ItemHelp_Open(void);
extern void ItemHelp_Close(void);

/* The Shop object is one file since the merge: src/menu/menu_v_d.c (0x399790..0x39E940). */
void Shop_CheckStockLevel(void);
s32 Shop_IsSoldOut(void);
s32 Shop_CanBuy(s32 item, WItemEntry *table);
void Shop_PlayVoice(void);
void Shop_SetListScissor(void);
void Shop_SetWindowScissor(void);
void Shop_ResetScissor(void);
void Shop_SwapGuide(void);
void Shop_Init(s32 section);
void Shop_Term(void);
void Shop_Draw(void);
void Shop_SetPlate(s32 flash, s32 level, s32 tab, char *label);
void Shop_Update(void);
void Shop_Input(s32 *result);
void Shop_UpdateSeq(void);
s32 Shop_Run(s32 section);

s32 EvoMode_Main(void);

/* EvoTop (menu_w_c.c, the whole object since the merge with menu_x.c): see include/menu/menu_x.h. */

#endif
