#ifndef SYS_LATE_A_H
#define SYS_LATE_A_H

#include "types.h"
#include "battle/view_a.h"

/*
 * The dragon-summoning ("Shenron") screen of the main executable, 0x2BD230..0x2BF6B0 (placeholder stem
 * "late_a"): the player has the seven Dragon Balls, a dragon appears and grants wishes (an item, a stage, a
 * character or money), and the game is saved. Game code linked after the SDK libraries and built with -G0 (no
 * $gp addressing). Three objects:
 *
 *   late_a.c    0x2BD230..0x2BEB20  Shen      the wish screen: mode loop, wish list, reward window
 *   late_a_b.c  0x2BEB20..0x2BF370  ShenCfm   the confirmation window ("reconfir") with the item details
 *   late_a_c.c  0x2BF370..0x2BF6B0  ShenSave  the save screen that follows (memory-card flow 0)
 *
 * Reached from the menu overlay's mode table: gProgress->mode 70 dispatches to Shen_Main. The resources are
 * sections of the menu archive (gMenuArc12, overlay data); the debug names left in the read-only data give
 * the original file names ("host:data/test/shenron/...").
 *
 * Matching notes for this code: movies are arrays of SHEN_FLASH_COUNT (1) and are stepped, drawn and destroyed
 * in loops over that count; the one-iteration loops disappear but decide how the compiler shares addresses
 * (a call in such a loop is never a tail call). Calls on a named clip go through small inline helpers.
 */

/*
 * Cross or triangle pressed on pad 0. The original tests the two bits of gamePressed through the 64-bit word
 * that starts at gameHeld (one `ld`, mask 0x600 << 32); a 32-bit test of gamePressed does not match.
 */
#define SHEN_PRESSED_CROSS_OR_TRIANGLE() (*(u64 *)&gPad[0].gameHeld & ((u64)(PADG_CROSS | PADG_TRIANGLE) << 32))

/* Movies per window: the code loops over them. */
#define SHEN_FLASH_COUNT 1

/* What a wish gives (ShenWish.kind). */
#define SHEN_WISH_ITEM 0    /* id = item number, 1-based */
#define SHEN_WISH_STAGE 1   /* id = stage number, 1-based */
#define SHEN_WISH_CHARA 2   /* id = character number, 1-based */
#define SHEN_WISH_MONEY 3   /* id = amount */

/*
 * Which dragon came (ShenWork.dragon); drawn by Rand_Range(100) in Shen_BuildList. Which is which is not
 * verified: 1 grants three wishes from a list of seven and has its own voice lines (Porunga, presumably).
 */
#define SHEN_DRAGON_0 0     /* 4 wishes listed, 1 granted; voice lines 0..7; wish names at text line 0 */
#define SHEN_DRAGON_1 1     /* 7 wishes listed, 3 granted; voice lines 8..15; wish names at text line 4 */
#define SHEN_DRAGON_2 2     /* 4 wishes listed, 1 granted; voice lines 0..7; wish names at text line 11;
                               only comes when bit 0 of gSaveData->slot[8].flags is set */

/* Voice lines (file 0x85B7 + line, Shen_Say); add 8 for SHEN_DRAGON_1. */
#define SHEN_LINE_GREET 0   /* 0 or 2 (dragon 1: 8 or 9), drawn by Rand_Range(2) */
#define SHEN_LINE_GRANT 3   /* 4 or 5 (dragon 1: 12 or 13), drawn by Rand_Range(2) */
#define SHEN_LINE_BYE 7     /* dragon 1: 15 */

/* ShenWork.state */
#define SHEN_STATE_SELECT 0   /* cursor on the wish list */
#define SHEN_STATE_CONFIRM 1  /* the confirmation window is open */
#define SHEN_STATE_GET 2      /* the reward window is open */
#define SHEN_STATE_LEAVE 3    /* farewell line, "fl_out" */

/* ShenWork.flags */
#define SHEN_FLAG_SHOWN 0x001     /* the windows were opened once the backdrop reported ready */
#define SHEN_FLAG_EXIT 0x002      /* the backdrop reported its end: leave the screen */
#define SHEN_FLAG_FADING 0x004    /* the fade-out was started */
#define SHEN_FLAG_OPENING 0x008   /* the icon and message windows still have to be opened */
#define SHEN_FLAG_UNUSED40 0x040  /* set when a wish is granted; never read */
#define SHEN_FLAG_GREETED 0x100   /* the greeting line was started */
#define SHEN_FLAG_LIST_ON 0x200   /* the cursor plate was lit after the greeting */
#define SHEN_FLAG_BYE 0x400       /* the farewell line ended: count `timer` down, then end the backdrop */

/* An entry of the wish file (shenron_list.dat) and of a list node. */
typedef struct ShenWish {
    /* 0x00 */ s32 id;      /* see SHEN_WISH_* */
    /* 0x04 */ u32 kind;    /* SHEN_WISH_* */
} ShenWish; /* size 8 */

/* The wish file: three lists of 8-byte entries. */
typedef struct ShenWishFile {
    /* 0x00 */ ShenWish list0[4];   /* SHEN_DRAGON_0 */
    /* 0x20 */ ShenWish list1[7];   /* SHEN_DRAGON_1 */
    /* 0x58 */ ShenWish list2[4];   /* SHEN_DRAGON_2 */
} ShenWishFile; /* size 0x78 */

/* A node of the circular wish list. The head (number 0) is inside ShenWork, the others on the heap. */
typedef struct ShenNode {
    /* 0x00 */ u32 no;              /* 0 = head, then 1, 2, ... */
    /* 0x04 */ ShenWish wish;
    /* 0x0C */ struct ShenNode *prev;
    /* 0x10 */ struct ShenNode *next;
} ShenNode; /* size 0x14 */

/* Entry of the item table (zitem_parameter.dat): only the type byte is read here (see GetWinItem). */
typedef struct ShenItem {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 unk1[0x27];
} ShenItem; /* size 0x28 */

/* The screen's work area: a local variable of Shen_Run. */
typedef struct ShenWork {
    /* 0x000 */ u8 *pack;           /* the screen's section of the menu archive (packed) */
    /* 0x004 */ u32 *res;           /* unpacked pack file, freed by Shen_Term */
    /* 0x008 */ Flash flash[SHEN_FLASH_COUNT]; /* the wish list movie; work +0x10 bit 1 = no labelled animation running */
    /* 0x034 */ s32 unk34;
    /* 0x038 */ u8 *tex[8];         /* texture list of the movie; [3] stays NULL */
    /* 0x058 */ void *msg;          /* shenron_msg: the dragon's lines and the wish names */
    /* 0x05C */ u8 unk5C[0x3C];     /* its address is the unused fourth argument of MsgWin_Init */
    /* 0x098 */ void *font;         /* text file of the five list rows */
    /* 0x09C */ TextBox box[5];     /* one per list row */
    /* 0x358 */ ShenItem *items;    /* item table */
    /* 0x35C */ s32 result;         /* return value of Shen_Run; never written (0), non-zero would keep the music */
    /* 0x360 */ s32 line;           /* voice / message line being said, -1 = none */
    /* 0x364 */ s32 state;          /* SHEN_STATE_* */
    /* 0x368 */ s32 answer;         /* what ShenCfm_Update reported: 0 = yes, 1 = no */
    /* 0x36C */ s32 dragon;         /* SHEN_DRAGON_* */
    /* 0x370 */ s32 granted;        /* wishes granted so far */
    /* 0x374 */ s32 wishMax;        /* wishes this dragon grants */
    /* 0x378 */ ShenNode list;      /* head of the wish list */
    /* 0x38C */ ShenNode *top;      /* the wish in the first row */
    /* 0x390 */ ShenNode *out;      /* the wish in the fifth row: the one scrolling out */
    /* 0x394 */ s32 cursor;         /* row of the cursor, 0..3 */
    /* 0x398 */ s32 unk398;         /* 30 when the fade-out starts; never read */
    /* 0x39C */ s32 flags;          /* SHEN_FLAG_* */
    /* 0x3A0 */ s32 timer;          /* frames between the farewell line and the end */
} ShenWork; /* size 0x3A4 */

/* The confirmation window (heap, 0x58 bytes). */
typedef struct ShenCfm {
    /* 0x00 */ Flash flash[SHEN_FLASH_COUNT];
    /* 0x2C */ u8 *tex[6];
    /* 0x44 */ s32 cursor;          /* 0 = yes, 1 = details, 2 = no */
    /* 0x48 */ s32 detail;          /* 1 while the item details page is shown */
    /* 0x4C */ s32 kind;            /* SHEN_WISH_* of the wish asked about */
    /* 0x50 */ s32 id;              /* its 0-based number */
    /* 0x54 */ s32 hasDetail;       /* 1 for an item: the middle choice is usable */
} ShenCfm; /* size 0x58 */

/* The save screen (heap, 0x18 bytes). */
typedef struct ShenSave {
    /* 0x00 */ u8 *pack;            /* the screen's section of the menu archive (packed) */
    /* 0x04 */ u32 *res;            /* unpacked pack file */
    /* 0x08 */ s32 flags;           /* 1 = the card flow reported its end, 2 = fading out, 4 = flow started */
    /* 0x0C */ s32 timer;           /* frames left after the flow ended (15) */
    /* 0x10 */ s32 result;          /* return value of ShenSave_Run; never written (0) */
    /* 0x14 */ s32 flowState;       /* what McFlow_Update returned this frame */
} ShenSave; /* size 0x18 */

s32 Shen_Main(void);
void ShenList_Free(ShenNode *head);
void ShenList_Build(ShenNode *head, u32 count);
ShenNode *ShenList_Find(ShenNode *head, s32 no);
ShenWish *ShenList_StepWish(ShenNode *node, s32 steps);
ShenNode *ShenList_Step(ShenNode *node, s32 steps);
void Shen_Say(ShenWork *work, s32 line);
s32 Shen_ClampCursor(s32 *cursor);
void Shen_PlayPlateOk(ShenWork *work);
void Shen_SetPlate(Flash *flash, s32 row, s32 on);
void Shen_MoveCursor(ShenWork *work, s32 dir);
void Shen_SetArrowUvs(Flash *flash);
void Shen_ScissorList(void);
void Shen_ScissorFull(void);
void Shen_DrawList(ShenWork *work);
void Shen_SetupGetWin(ShenWork *work);
void Shen_BuildList(ShenWork *work, ShenWishFile *file);
void Shen_Init(ShenWork *work, s32 section);
void Shen_Advance(ShenWork *work);
void Shen_Draw(ShenWork *work);
void Shen_Term(ShenWork *work);
s32 Shen_UpdateTalk(ShenWork *work);
void Shen_Update(ShenWork *work);
s32 Shen_UpdateExit(ShenWork *work);
s32 Shen_Run(s32 section);

void ShenCfm_Init(u32 *pack);
void ShenCfm_Term(void);
s32 ShenCfm_IsOwned(ShenCfm *cfm);
void ShenCfm_Draw(void);
void ShenCfm_Open(s32 kind, s32 id);
void ShenCfm_MoveCursor(ShenCfm *cfm, s32 dir);
void ShenCfm_PlayPlateOk(ShenCfm *cfm);
s32 ShenCfm_Update(s32 *answer);
void ShenCfm_Close(void);

void ShenSave_OnFlowDone(s32 arg);
void ShenSave_Init(s32 section);
void ShenSave_Term(void);
void ShenSave_Draw(void);
void ShenSave_Advance(void);
void ShenSave_Update(void);
s32 ShenSave_UpdateExit(void);
s32 ShenSave_Run(s32 section);

#endif
