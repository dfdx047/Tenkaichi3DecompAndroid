#ifndef BATTLE_HUD_0_H
#define BATTLE_HUD_0_H

#include "types.h"

/*
 * Battle pause menu. Source range 0x2129C8-0x215420, three modules:
 *
 *   src/battle/hud_0.c    0x2129C8-0x213238  tail of the battle-level wrappers (BtlGame_Draw, ...) and the
 *                                            pause menu itself: which menu tree a battle mode gets, the item
 *                                            callbacks, and what a picked item does to the battle result.
 *   src/battle/hud_0_b.c  0x213238-0x215010  the menu engine (BtlMenu_*): a tree of BtlMenu / BtlMenuItem,
 *                                            its input, its open / close animation and its drawing.
 *   src/battle/hud_0_c.c  0x215010-0x215420  head of the skill-list text module that continues in
 *                                            src/battle/btl_seq.c (BtlText_*): the controls page, the
 *                                            sheet sprite, the scissor and the line scanner.
 *
 * The menu trees themselves are initialised data at 0x2C4E70-0x2C6070 (not defined in C yet).
 */

typedef struct BtlMenu BtlMenu;

/* What an item callback gets. */
typedef struct BtlMenuEvent {
    /* 0x00 */ BtlMenu *menu; /* the menu the item is in */
    /* 0x04 */ s32 id;        /* BtlMenuItem.id */
    /* 0x08 */ s32 arg;       /* BtlMenuItem.arg */
    /* 0x0C */ s32 first;     /* 1 on the first call ever made for this item */
    /* 0x10 */ s32 selected;  /* 1 on the frame the item was confirmed */
} BtlMenuEvent; /* size 0x14 */

typedef s32 (*BtlMenuFunc)(BtlMenuEvent *ev);

/* BtlMenuItem.type */
enum {
    BTL_MENU_ITEM_VALUE = 0,  /* confirm changes `value` as the menu's mode says */
    BTL_MENU_ITEM_RESULT = 1, /* confirm makes the item (or its top-level ancestor) the menu's result */
    BTL_MENU_ITEM_BACK = 2,   /* confirm closes the menu */
    BTL_MENU_ITEM_SKILLS = 3, /* as the first item: the menu is the skill-list page */
    BTL_MENU_ITEM_CONTROLS = 4 /* as the first item: the menu is the controls page */
};

/* BtlMenu.mode: what confirm does to an item of type 0 */
enum {
    BTL_MENU_MODE_TOGGLE = 0, /* value ^= 1 */
    BTL_MENU_MODE_RADIO = 1,  /* every value = 0, this one = 1 */
    BTL_MENU_MODE_SINGLE = 2  /* value ^= 1; when it became 1, every other value = 0 */
};

/* BtlMenu.state */
enum {
    BTL_MENU_STATE_OPEN = 0,     /* first frame: start growing from the parent's size */
    BTL_MENU_STATE_OPENING = 1,  /* 6 frames */
    BTL_MENU_STATE_CLOSE = 2,    /* start shrinking to the parent's size */
    BTL_MENU_STATE_CLOSING = 3,  /* 6 frames, then BtlMenu_Update returns 1 */
    BTL_MENU_STATE_ACTIVE = 4    /* takes input, draws its items */
};

typedef struct BtlMenuItem {
    /* 0x00 */ s16 text[2];      /* text number per controller port (BtlMenu_GetText(8 + n)); negative: sheet part ~n */
    /* 0x04 */ s32 id;
    /* 0x08 */ BtlMenu *sub;     /* menu opened by confirm, or NULL */
    /* 0x0C */ BtlMenuFunc func; /* called every frame while `active` */
    /* 0x10 */ s32 arg;
    /* 0x14 */ s32 type;         /* BTL_MENU_ITEM_* */
    /* 0x18 */ s32 value;
    /* 0x1C */ s32 disabled;     /* the cursor steps over the item */
    /* 0x20 */ s32 active;       /* confirmed: `func` runs until it returns non-zero or the sub menu closes */
    /* 0x24 */ s32 called;       /* `func` has been called at least once since the reset */
    /* 0x28 */ s32 selected;     /* confirmed this frame (cleared after the next `func` call) */
    /* 0x2C */ s32 picked;       /* confirmed at least once (cleared by BtlMenu_Reset) */
} BtlMenuItem; /* size 0x30 */

struct BtlMenu {
    /* 0x00 */ BtlMenuItem *items;
    /* 0x04 */ s32 count;
    /* 0x08 */ f32 scale;   /* text scale; also scales the box */
    /* 0x0C */ s16 padX;    /* margins */
    /* 0x0E */ s16 padY;
    /* 0x10 */ s32 mode;    /* BTL_MENU_MODE_* */
    /* 0x14 */ s32 open;    /* index of the item whose sub menu is open, -1 = none */
    /* 0x18 */ s32 cursor;
    /* 0x1C */ s32 state;   /* BTL_MENU_STATE_* */
    /* 0x20 */ f32 w;       /* box size during the open / close animation */
    /* 0x24 */ f32 h;
    /* 0x28 */ f32 dw;      /* its step per frame */
    /* 0x2C */ f32 dh;
    /* 0x30 */ s32 frame;   /* animation frame, 0..6 */
}; /* size 0x34 (0x38 apart in the data) */

/* One texture of the menu's sprite sheet (local view of TexEntry, sys/gfxm_b_b.h). */
typedef struct BtlMenuTex {
    /* 0x00 */ u8 unk0[0x20];
    /* 0x20 */ s32 tbpOfs;
    /* 0x24 */ s32 cbpOfs;
    /* 0x28 */ u8 unk28[8];
    /* 0x30 */ u64 tex0;
    /* 0x38 */ u8 unk38[8];
} BtlMenuTex; /* size 0x40 */

/* Local view of TexFile. */
typedef struct BtlMenuTexFile {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ BtlMenuTex *ent;
} BtlMenuTexFile;

/* One port's skill list state (the same layout as BtlTextList in battle/btl_seq.h). */
typedef struct BtlMenuList {
    /* 0x00 */ s32 pages;
    /* 0x04 */ s32 count[10];
    /* 0x2C */ s32 page;
    /* 0x30 */ s32 scroll[10];
    /* 0x58 */ s32 cursor[10];
} BtlMenuList; /* size 0x80 */

/* The menu work, 0x120 bytes on heap 2 (the same block as BtlTextWork in battle/btl_seq.h). */
typedef struct BtlMenuWork {
    /* 0x00 */ BtlMenuTexFile *tex; /* sprite sheet: the texture file inside the common file at gCommonRes + 0x28 */
    /* 0x04 */ s32 skillList;  /* 1 when a skill-list page was updated this frame */
    /* 0x08 */ s32 active;     /* the menu is shown; read by BtlMenu_IsActive (replay recorder) */
    /* 0x0C */ s32 pad;        /* controller port that owns the menu (-1: nobody) */
    /* 0x10 */ s32 padType;    /* Pad_GetLastStatus(pad); 0 in battle mode 5 */
    /* 0x14 */ BtlMenu *top;   /* the tree being shown */
    /* 0x18 */ u16 *text;      /* skill-list script of the owner's fighter (battle object + 0xBC) */
    /* 0x1C */ u16 *text2;     /* second script (never set inside this range) */
    /* 0x20 */ BtlMenuList list[2];
} BtlMenuWork; /* size 0x120 */

/* hud_0.c */
void BtlGame_Draw(void);
s32 BtlGame_IsFighting(void);
s32 BtlGame_IsReplay(void);
BtlMenu *PauseMenu_GetMenu(s32 which);
s32 PauseMenu_ConfirmFunc(BtlMenuEvent *ev);
s32 PauseMenu_SkillListFunc(BtlMenuEvent *ev);
s32 PauseMenu_CpuLevelFunc(BtlMenuEvent *ev);
void PauseMenu_Reset(void);
void PauseMenu_Init(void);
void PauseMenu_Term(void);
s32 PauseMenu_Update(s32 pad, s32 which);
void PauseMenu_Draw(void);

/* hud_0_b.c */
void BtlMenu_BeginDraw(u64 **pkt);
void BtlMenu_EndDraw(u64 **pkt);
void BtlMenu_PutSprite(u64 **pkt, s32 x0, s32 y0, s32 x1, s32 y1, s32 u0, s32 v0, s32 u1, s32 v1, u32 color,
                       s32 tex, s32 clut);
void BtlMenu_PutMirrored(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1, u32 color, s32 tex, s32 clut, s32 halves);
u64 BtlMenu_GetTex0(s32 tex, s32 clut);
void BtlMenu_UploadTex(void);
s32 BtlMenu_GetDefaultPadType(void);
s32 BtlMenu_IsSkillList(BtlMenu *menu);
s32 BtlMenu_IsControls(BtlMenu *menu);
u16 *BtlMenu_GetText(s32 n);
u16 *BtlMenu_GetItemText(s32 n);
u16 *BtlMenu_GetControlsText(s32 padType, s32 config);
BtlMenuItem *BtlMenu_FindItem(BtlMenu *menu, s32 id);
BtlMenu *BtlMenu_FindOwner(BtlMenu *menu, s32 id);
s32 BtlMenu_GetWidth(BtlMenu *menu);
s32 BtlMenu_GetHeight(BtlMenu *menu);
s32 BtlMenu_HasOpenSub(BtlMenu *menu);
void BtlMenu_DrawPart(u64 **pkt, s32 x, s32 y, s32 w, s32 h, s32 tex, s32 part, s32 centered);
void BtlMenu_DrawFrame(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 kind);
void BtlMenu_DrawPageBar(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 page);
void BtlMenu_DrawPageTab(u64 **pkt, s32 x, s32 y, s32 page);
void BtlMenu_Draw(u64 **pkt, BtlMenu *menu);
void BtlMenu_CallItem(BtlMenu *menu, s32 idx, s32 leaf);
void BtlMenu_FixCursor(BtlMenu *menu);
void BtlMenu_SetActive(void);
void BtlMenu_ClearActive(void);
void BtlMenu_Init(void);
void BtlMenu_Term(void);
void BtlMenu_DrawAll(void);
BtlMenuWork *BtlMenu_GetWork(void);
void BtlMenu_SetTop(BtlMenu *menu);
s32 BtlMenu_Update(s32 which, BtlMenu *parent, BtlMenu *menu, s32 pad);
s32 BtlMenu_GetValue(BtlMenu *menu, s32 id);
void BtlMenu_SetValue(BtlMenu *menu, s32 id, s32 value);
s32 BtlMenu_GetPicked(BtlMenu *menu, s32 id);
void BtlMenu_SetPicked(BtlMenu *menu, s32 id, s32 picked);
void BtlMenu_SetDisabled(BtlMenu *menu, s32 id, s32 disabled);
void BtlMenu_Reset(BtlMenu *menu);
BtlMenuItem *BtlMenu_FindResult(BtlMenu *top, BtlMenu *menu);
s32 BtlMenu_IsSubOpen(BtlMenu *menu, s32 id);
void BtlMenu_SetScript(u16 *text);
void BtlMenu_SetScript2(u16 *text);
u16 *BtlMenu_GetScript(void);
u16 *BtlMenu_GetScript2(void);
s32 BtlMenu_IsActive(void);

/* hud_0_c.c */
void BtlText_DrawControls(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1);
s32 BtlText_IsOnScreen(s32 x0, s32 y0, s32 x1, s32 y1);
void BtlText_PutSprite(u64 **pkt, s32 x0, s32 y0, s32 x1, s32 y1, s32 u0, s32 v0, s32 u1, s32 v1, u32 color,
                       s32 part);
u64 BtlText_GetTex0(s32 part);
void BtlText_PutScissor(u64 **pkt, s32 x0, s32 y0, s32 x1, s32 y1);
s32 BtlText_IsNewline(u16 c);
u16 *BtlText_NextLine(u16 *p);

#endif
