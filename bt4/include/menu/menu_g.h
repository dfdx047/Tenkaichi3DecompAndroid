#ifndef MENU_MENU_G_H
#define MENU_MENU_G_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x351C38..0x356090 (placeholder stem "menu_g"). Three pieces:
 *
 *   menu_g.c    0x351C38..0x352CB8  ItemPanel  the equipped-item ("custom") panel of one side of a character
 *                                              select; tail of the object that starts before this chunk
 *   menu_g_b.c  0x352CB8..0x352EC0  Duel_Main  handler of progress modes 38..41 (main-menu item 3)
 *   menu_g_c.c  0x352EC0..0x3562B8  DuelMenu   the screen of mode 38 (with DuelMenu_Run, 0x356090, merged in from menu_h.c)
 */

/* ---- main executable ---- */

extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void MsgWin_SetText(void *text);
extern void Voice_StopWithLip(void);
extern void Save_ResetRules(void);
extern void Dialog_Start(s32 kind);
extern s32 Dialog_Input(s32 allowCancel);
extern void Dialog_SetLayout(s32 layout);
extern void Dialog_SetCursor(s32 cursor);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetMsg(s32 idx);
extern s32 Dialog_IsClosed(void);
extern void Dialog_Draw(s32 visible);

/* Item entry of common file 4, section 2 (ItemTblEntry in battle/view_b.h; local view). */
typedef struct MItemEntry {
    /* 0x00 */ u8 type;         /* column of the item-kind icon */
    /* 0x01 */ u8 unk1[2];
    /* 0x03 */ u8 slots;        /* how many item slots it takes (0 = no cost icon) */
    /* 0x04 */ u8 unk4[0x24];
} MItemEntry; /* 0x28 */

extern s32 ChrTbl_GetLevel(s32 chara, s32 slot, s32 fromRec);
extern s32 ItemSet_Fit(u16 *ids, MItemEntry *table, s32 capacity, s32 *used);
extern s32 ItemTbl_GetClass(s32 item, MItemEntry *table);

/* A set of equipped items: 1-based item ids, 0 = empty; seven usable slots. */
typedef struct MItemSet {
    u16 id[8];
} MItemSet;

/* ---- ItemPanel (menu_g.c) ---- */

#define ITEMPANEL_ROWS 8       /* list plates: seven item slots and the "back" row */
#define ITEMPANEL_BACK 7       /* the cursor row that is always reachable */

typedef struct ItemPanel {
    /* 0x000 */ void *text[3];      /* sections 13, 14, 15 of the caller's pack: character names, form names, item names */
    /* 0x00C */ MFlash flash;
    /* 0x038 */ u8 *tex[23];
    /* 0x094 */ s32 flags;          /* ITEMPANEL_ */
    /* 0x098 */ s32 chara;          /* character id: line of the name and form texts */
    /* 0x09C */ s32 set;            /* row of the set-name strip: item set + 1 */
    /* 0x0A0 */ s32 limit;          /* item slots the character has (ChrTbl_GetLevel) */
    /* 0x0A4 */ s32 used;           /* ItemSet_Fit result */
    /* 0x0A8 */ s32 count;          /* rows in use (ItemSet_Fit's second result) */
    /* 0x0AC */ s32 cursor;
    /* 0x0B0 */ MItemSet item;      /* the set shown */
    /* 0x0C0 */ MTextBox name;
    /* 0x14C */ MTextBox form;
    /* 0x1D8 */ MTextBox line[9];   /* eight are used */
    /* 0x6C4 */ MItemEntry *items;  /* item table of common file 4 */
} ItemPanel; /* 0x6C8 */

#define ITEMPANEL_STARTED 4     /* the cursor plates were lit once */
#define ITEMPANEL_SHOWN 8
#define ITEMPANEL_CLOSING 0x10  /* "fl_evo_cansel" is playing */

extern ItemPanel *gItemPanel[2]; /* 0x3B38E0, one per side */

void ItemPanel_ClipGoto(s32 side, s32 kind, char *label);
void ItemPanel_Init(u32 *pack, s32 side);
void ItemPanel_Term(s32 side);
void ItemPanel_Draw(s32 side);
void ItemPanel_Update(s32 side);
s32 ItemPanel_Input(s32 side, s32 pad);
void ItemPanel_SetChara(s32 side, s32 chara, s32 slot, s32 set, s32 fromRec);
void ItemPanel_Show(s32 side);
void ItemPanel_Hide(s32 side);

/* ---- Duel (menu_g_b.c, menu_g_c.c) ---- */

/* gProgress fields of the duel mode (local view; MenuProgress in menu_a.h has only +0x624). */
typedef struct DuelProgress {
    /* 0x000 */ u8 unk0[0x620];
    /* 0x620 */ s32 versus;         /* DuelMenu top item: 0 1P vs COM, 1 1P vs 2P, 2 COM vs COM (3 = settings) */
    /* 0x624 */ s32 battleType;     /* 0 single, 1 team, 2 DP battle */
    /* 0x628 */ s32 unk628[2];
    /* 0x630 */ s32 dpLimit;        /* row of the DP limit list (0..2) */
} DuelProgress;

#define DUEL_PROG ((DuelProgress *)gProgress)

#define DUEL_BGM PORT_V(0x10B1B, 0x83D)
#define DUEL_VOICE_BASE 0x85C7
#define DUEL_IDLE_FRAMES 0xE10  /* 3600 frames without input: the second guide speaks line 8 */

#define DUELMENU_FLASH_NUM 1
#define DUELMENU_LEVELS 5

/* The "restore the default settings?" question. */
typedef struct DuelReset {
    /* 0x00 */ s32 state;           /* 0 idle, 1 open, 2 asking, 3 closing, 4 wait until closed */
    /* 0x04 */ s32 answer;          /* Dialog_Input result: > 0 yes, < 0 no */
    /* 0x08 */ s32 timer;
    /* 0x0C */ s32 msg;
} DuelReset;

typedef struct DuelMenu {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 5 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 28 sections */
    /* 0x008 */ void *msgText;      /* section 21: text of the message window */
    /* 0x00C */ void *subtitles;    /* section 26: subtitles of the guide voices */
    /* 0x010 */ void *setText;      /* section 27: dialog text, also the message window text of the settings list */
    /* 0x014 */ MFlash flash[DUELMENU_FLASH_NUM];
    /* 0x040 */ void *bg;           /* section 1: background picture */
    /* 0x044 */ u8 *tex[49];
    /* 0x108 */ MTexRes *setRes;    /* section 10: tex[47] / tex[48] are swapped between its textures */
    /* 0x10C */ s32 flags;          /* DUELMENU_ */
    /* 0x110 */ s32 sel[DUELMENU_LEVELS]; /* cursor of each level: top, battle type, DP limit, setting, setting again */
    /* 0x124 */ s32 timer;          /* frames until the fade out starts after the choice */
    /* 0x128 */ s32 level;          /* DUEL_LEVEL_ */
    /* 0x12C */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x130 */ s32 idle;
    /* 0x134 */ s32 rule;           /* index into gSaveData->rule of the setting being edited */
    /* 0x138 */ s32 column;         /* which of the two values of setting 3 the cursor is on */
    /* 0x13C */ s32 value[2];       /* the value(s) being edited */
    /* 0x144 */ s32 valueCount;
    /* 0x148 */ s32 startState;     /* 0 idle, 1 say the line, 2 wait for it, 3 leave */
    /* 0x14C */ s32 talker;         /* which guide's mouth moves: 0 Vegeta, 1 Nappa */
    /* 0x150 */ s32 blink[2];
    /* 0x158 */ s32 talk[2];
    /* 0x160 */ s32 iconTimer;
    /* 0x164 */ s32 iconFrame;
    /* 0x168 */ f32 smoke[6];
    /* 0x180 */ f32 cloud;
    /* 0x184 */ DuelReset reset;
} DuelMenu; /* 0x194 */

#define DUELMENU_CHOSEN 1
#define DUELMENU_LEAVING 2
#define DUELMENU_STARTED 4      /* the cursor plate was lit once */
#define DUELMENU_GREETED 8      /* set by DuelMenu_Run: the greeting was started */
#define DUELMENU_NO_PAD2 0x10   /* no controller in port 2: "1P vs 2P" is greyed out */

#define DUEL_LEVEL_TOP 0
#define DUEL_LEVEL_TYPE 1
#define DUEL_LEVEL_DP 2
#define DUEL_LEVEL_SETTING 3
#define DUEL_LEVEL_VALUE 4

extern DuelMenu *gDuelMenu;      /* 0x3B38E8 */

s32 Duel_Main(void);
s32 Duel_Mode41(s32 arg);
void DuelMenu_Idle(void);
void DuelMenu_ShowValues(s32 item, s32 show);
void DuelMenu_SetupValue(void);
void DuelMenu_Init(s32 section);
void DuelMenu_Term(void);
void DuelMenu_Draw(void);
void DuelMenu_ClipGoto(s32 level, char *label);
void DuelMenu_Update(void);
void DuelMenu_Input(s32 *result);
void DuelMenu_UpdateStart(void);
void DuelMenu_UpdateReset(void);
s32 DuelMenu_Run(s32 section);

#endif
