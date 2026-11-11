#ifndef MENU_MENU_L_H
#define MENU_MENU_L_H

#include "menu/menu_a.h"
#include "sys/save.h"

/*
 * Menu overlay DBZP.BIN, 0x368C18..0x36DBE8 (placeholder stem "menu_l"). Two pieces:
 *
 *   (menu_l.c)  0x368C18..0x36B3E0  Bracket  the tournament logic of Dragon World Tour (progress mode 35): the tree,
 *                                            the entrants, the CPU matches, the battle hand-off and the prizes. Tail
 *                                            of the object that starts at 0x368068 (Bracket_Load): now merged into
 *                                            menu_k_f.c, which includes this header.
 *   menu_l_b.c  0x36B3E0..0x36E028  SoloSel  the one-character select of the modes 13..30 group (called for modes
 *                                            15, 18, 21, 25 and 29); merged with the former menu_m.c (the closing
 *                                            step and SoloSel_Run, 0x36DBE8..0x36E028).
 *
 * menu_j.h and menu_k.h were still changing while this was written, so this header depends on menu_a.h only:
 * LBracket is this chunk's own, complete view of the bracket work area (same offsets as Bracket of menu_k.h; the
 * entrant and match records are more detailed here), and the cell / entry / item-set records are repeated under
 * L names (LChrCell = ESelCell, LSelEntry = ESelEntry, LItemSet = TourItemSet of menu_j.h).
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void *memcpy(void *, const void *, u32);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Flash_SetOffset(MFlash *flash, s32 x, s32 y);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern s32 ChrTbl_GetCost(s32 chara);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *charaBits); /* (a pointer: declared s32 it was passed as 32 bits on the stack, the upper half of what the function reads left to chance on a 64-bit build) */
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  u16 *items);
extern void BattleSetup_Finish(void);

/* The battle result (include/battle/battle.h): the two sides' remaining health. */
typedef struct LBattleResult {
    /* 0x00 */ u8 unk0[0x2C];
    /* 0x2C */ f32 health[2];
} LBattleResult;

extern LBattleResult *BattleResult_GetPtr(void);

/* Always-loaded resources (include/sys/common.h): common file 4 is data[2]. */
typedef struct LCommonRes {
    /* 0x00 */ void *boot;
    /* 0x04 */ void *data[3];
} LCommonRes;

extern LCommonRes *gCommonRes;

/* A cell of the character grid (ChrGridCell of include/battle/view_a.h; local view). */
typedef struct LChrCell {
    /* 0x00 */ s32 id;          /* character id 0..0xA0; above that a special cell (0xA4 = filler) */
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} LChrCell; /* 0x24 */

#define LCHR_CELL_MAX 165
#define LCHR_ID_MAX 0xA0         /* highest real character id */
#define LCHR_ID_NONE 0xA4        /* filler: an empty chip */

/* The grid list of a screen pack: count, then the cells from 0x10. */
typedef struct LChrGridList {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ LChrCell cell[1];
} LChrGridList;

extern void ChrGrid_MoveLeft(LChrCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveRight(LChrCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveUp(LChrCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_MoveDown(LChrCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_PrevForm(s32 *forms, s32 *form);
extern void ChrGrid_NextForm(s32 *forms, s32 *form);
extern s32 ChrGrid_IsSelectable(LChrCell *cells, s32 index);
extern void ChrGrid_Build(s32 *outCount, LChrCell *out, s32 *inCount, LChrCell *in, s32 *customCount,
                          LChrCell *custom);

/* An item set as the battle setup takes it: eight item ids, zero = empty. */
typedef struct LItemSet {
    /* 0x00 */ u16 id[8];
} LItemSet; /* 0x10 */

/* gSaveData->custom[n] (SaveCustom of include/sys/save.h, with the three item sets as records). */
typedef struct LSaveCustom {
    /* 0x00 */ LItemSet set[SAVE_CUSTOM_SETS];
    /* 0x30 */ s32 unk30;
    /* 0x34 */ u16 level;
    /* 0x36 */ u16 unk36;
} LSaveCustom; /* 0x38 */

/*
 * The save as the overlay addresses it (MSave of menu_c.h, repeated here because menu_c.h declares
 * BattleResult_GetPtr with another view): two checksum words, then the body as a nested structure.
 */
typedef struct LSaveBody {
    /* 0x0008 */ s32 unlockFlags;  /* bits 0-6: the seven dragon balls */
    /* 0x000C */ s32 level;
    /* 0x0010 */ SaveSlot slot[SAVE_SLOT_COUNT];
    /* 0x00A0 */ u8 unkA0[0xC10 - 0xA0];
    /* 0x0C10 */ u64 charaBits[SAVE_CHARA_WORDS];
    /* 0x0C28 */ u64 stageBits;
    /* 0x0C30 */ u32 bgmBits;
    /* 0x0C34 */ u8 unkC34[0x1808 - 0xC34];
    /* 0x1808 */ LSaveCustom custom[SAVE_CUSTOM_COUNT];
    /* 0x2D40 */ SaveRec rec[SAVE_REC_COUNT];
    /* 0x2EC8 */ u8 item[SAVE_ITEM_COUNT];
    /* 0x3026 */ u8 unk3026[2];
    /* 0x3028 */ s32 money;
} LSaveBody;

typedef struct LSave {
    /* 0x00 */ s32 sum[2];
    /* 0x08 */ LSaveBody body;
} LSave;

#define LSAVE (&((LSave *)gSaveData)->body)

/* ---- menu overlay, other chunks ---- */

extern void ItemPanel_Init(u32 *pack, s32 side);      /* menu_g */
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

/* ---- The tournament (menu_l.c) ---- */

/* The five tournaments (TOUR_ of menu_j.h). */
#define LTOUR_WORLD 0            /* World Tournament */
#define LTOUR_BIG 1              /* World Martial Arts Big Tournament */
#define LTOUR_CELL 2             /* Cell Games */
#define LTOUR_OTHERWORLD 3       /* Otherworld Tournament */
#define LTOUR_YAMCHA 4           /* Yamcha Game */
#define LTOUR_NUM 5

/* One entrant of the running tournament (0x28 bytes; menu_j.h's TourEntrant with the two unknown words named). */
typedef struct LTourEntrant {
    /* 0x00 */ u16 flags;        /* LTOUR_ENT_ */
    /* 0x02 */ u16 unk2;
    /* 0x04 */ s32 chara;
    /* 0x08 */ s32 costume;
    /* 0x0C */ s32 player;
    /* 0x10 */ s32 pos;          /* where the chip sits in the tree: 0..15 the leaves, 16..32 the match nodes */
    /* 0x14 */ f32 health;       /* health left after the last battle (carried over in the Cell Games) */
    /* 0x18 */ LItemSet item;
} LTourEntrant; /* 0x28 */

#define LTOUR_ENT_PLAYER 1       /* chosen by a player (pad controlled) */
#define LTOUR_ENT_LOST 2
#define LTOUR_ENT_BOSS 4         /* the seeded seventeenth entrant */
#define LTOUR_ENT_HIDDEN 8       /* its chip is hidden in the tree while the match animation shows it */

#define LTOUR_ENTRANT_MAX 17

/* One match of the tree. */
typedef struct LTourMatch {
    /* 0x00 */ u16 flags;        /* LTOUR_MATCH_ */
    /* 0x02 */ u16 ent[2];       /* entrant indices, left and right */
    /* 0x06 */ u16 next;         /* match the winner goes to */
    /* 0x08 */ u16 pos;          /* tree position the winner's chip takes (16..32) */
    /* 0x0A */ u16 vsAnim;       /* number - 1 of the "fr_vs_action_%d" label */
    /* 0x0C */ u16 view;         /* which part of the tree is scrolled into view (Bracket_GetViewX) */
    /* 0x0E */ u16 winAnim;      /* number - 1 of the "fr_win_action_%d" label */
} LTourMatch; /* 0x10 */

#define LTOUR_MATCH_DONE 1
#define LTOUR_MATCH_ROUND_END 2  /* last match of its round */
#define LTOUR_MATCH_TO_LEFT 4    /* the winner becomes ent[0] of match `next` */
#define LTOUR_MATCH_TO_RIGHT 8   /* the winner becomes ent[1] of match `next` */
#define LTOUR_MATCH_FINAL 0x10

#define LTOUR_MATCH_MAX 16

/* gProgress as the tournament uses it. */
typedef struct LTourProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;       /* 0x20 cleared when a round ends */
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x68];
    /* 0x084 */ s32 tour;        /* LTOUR_ */
    /* 0x088 */ s32 level;       /* difficulty 0..2 */
    /* 0x08C */ s32 entryNum;    /* entrants the players chose */
    /* 0x090 */ s32 unk90[2];
    /* 0x098 */ LTourEntrant entrant[LTOUR_ENTRANT_MAX];
} LTourProgress;

#define LTOUR_PROG ((LTourProgress *)gProgress)

/* Section 31 of the bracket pack: the CPU opponents' strength, [level][round]. */
typedef struct LTourCpu {
    /* 0x00 */ s32 cpuLevel;
    /* 0x04 */ LItemSet item;    /* item ids - 1 */
} LTourCpu; /* 0x14 */

#define LTOUR_ROUND_NUM 5

/* Section 41 of the bracket pack: the prizes of one tournament, [kind][level]. */
typedef struct LTourPrize {
    /* 0x00 */ s32 unk0[4];
    /* 0x10 */ s32 prize[7][3];  /* LPRIZE_; -1 = none */
} LTourPrize; /* 0x64 */

#define LPRIZE_MONEY_WIN 0
#define LPRIZE_MONEY_SECOND 1
#define LPRIZE_ITEM 2
#define LPRIZE_CHARA_A 3
#define LPRIZE_CHARA_B 4
#define LPRIZE_STAGE 5
#define LPRIZE_NUM 7

typedef struct LBracketReward {
    /* 0x00 */ s32 kind;         /* GetWin kind: 0 character, 1 stage, 2 item, 6 money, 9 dragon ball */
    /* 0x04 */ s32 value;
} LBracketReward;

#define LBRACKET_FLASH_NUM 6

/* flash[] (BRK_FL_ of menu_k.h) */
#define LBRK_FL_TREE 0           /* the tree with the 17 chips; scrolls sideways */
#define LBRK_FL_MOVE_A 1         /* the two chips of the current match */
#define LBRK_FL_MOVE_B 2         /* the winner's chip moving up */
#define LBRK_FL_GUIDE 3          /* the guide character(s) */
#define LBRK_FL_VS 4             /* the "versus" panel: names, forms, numbers, win / lose */
#define LBRK_FL_TITLE 5          /* title plate and the result banners */

typedef struct LBracket {
    /* 0x0000 */ void *pack;
    /* 0x0004 */ u32 *res;
    /* 0x0008 */ void *imageFile[2]; /* 0x16800 bytes each: compressed large pictures of the two fighters */
    /* 0x0010 */ void *imageRes[2];  /* the same unpacked */
    /* 0x0018 */ u32 *chips;         /* pack of the small character pictures */
    /* 0x001C */ void *subtitles;
    /* 0x0020 */ void *msgText;
    /* 0x0024 */ void *nameText;
    /* 0x0028 */ void *formText;
    /* 0x002C */ void *file;
    /* 0x0030 */ MFlash flash[LBRACKET_FLASH_NUM]; /* 0 tree, 1 the two chips meeting, 2 the winner moving up, ... */
    /* 0x0138 */ s32 unk138;
    /* 0x013C */ MTexRes *guideTex[2]; /* sections 43 / 44: the two picture sets of the guide */
    /* 0x0144 */ s32 unk144;
    /* 0x0148 */ u8 *texTree[26];    /* textures of flash[0]; 6 and 9..24 are the seventeen chips */
    /* 0x01B0 */ u8 *texTitle[38];
    /* 0x0248 */ u8 *texGuide[9];
    /* 0x026C */ u8 *texMoveA[6];    /* flash[1]; 1 and 4 are the two chips */
    /* 0x0284 */ u8 *texMoveB[6];    /* flash[2]; 1 and 4 are the two chips */
    /* 0x029C */ u8 *texVs[16];      /* flash[4]; 6 and 5 are the large pictures (left, right entrant) */
    /* 0x02DC */ s32 flags;          /* LBRK_ */
    /* 0x02E0 */ s32 voiceLine;
    /* 0x02E4 */ s32 winSide;        /* side of the current match that won (0 left, 1 right) */
    /* 0x02E8 */ s32 result;
    /* 0x02EC */ s32 pose;
    /* 0x02F0 */ s32 talker;
    /* 0x02F4 */ s32 gridCount;
    /* 0x02F8 */ LChrCell *grid;
    /* 0x02FC */ s32 gridOutCount;
    /* 0x0300 */ LChrCell gridBuf[LCHR_CELL_MAX];
    /* 0x1A34 */ s32 timer;
    /* 0x1A38 */ s32 started;
    /* 0x1A3C */ s32 seq;            /* step of the guide's script, 0 = idle; 0x65 = "round over" */
    /* 0x1A40 */ s32 loadState;      /* LBRK_LOAD_: background loader of the two large pictures */
    /* 0x1A44 */ s32 round;          /* 0..4 */
    /* 0x1A48 */ s32 match;          /* index into match_[] of the match being played */
    /* 0x1A4C */ s32 mcState;
    /* 0x1A50 */ s32 scrollSpeed;
    /* 0x1A54 */ f32 scroll;         /* horizontal offset of the tree */
    /* 0x1A58 */ s32 blink[2];
    /* 0x1A60 */ s32 talk[2];
    /* 0x1A68 */ LTourEntrant entrant[LTOUR_ENTRANT_MAX];
    /* 0x1D10 */ LTourMatch match_[LTOUR_MATCH_MAX];
    /* 0x1E10 */ MTextBox nameBox[2];
    /* 0x1F28 */ MTextBox formBox[2];
    /* 0x2040 */ LTourCpu *cpu;      /* section 31 */
    /* 0x2044 */ LTourPrize *prize;  /* section 41 */
    /* 0x2048 */ LBracketReward reward[8];
    /* 0x2088 */ s32 rewardCount;
    /* 0x208C */ s32 rewardIdx;
} LBracket; /* 0x2090 */

#define LBRK_RESULT 0x10         /* a battle result was taken over */
#define LBRK_MOVE_B 0x20         /* flash[2] plays: the winner's chip moves up */
#define LBRK_MOVE_A 0x40         /* flash[1] plays: the two chips meet */
#define LBRK_IMAGES 0x200        /* the two large pictures are loaded */
#define LBRK_PLAYER_OUT 0x4000   /* no player entrant is left */
#define LBRK_PVP 0x8000          /* the battle set up is player against player */
#define LBRK_LEFT_PAD 0x10000    /* ... the left entrant (pad) against the CPU */
#define LBRK_RIGHT_PAD 0x20000   /* ... the right entrant (pad) against the CPU */

#define LBRK_LOAD_REQUEST 1
#define LBRK_LOAD_READ 2
#define LBRK_LOAD_UNPACK 3

#define LBRK_SEQ_ROUND_END 0x65

extern void TourBg_Init(void *file, u32 kind, u8 **tex);   /* menu_k_d.c */
extern void GetWin_Init(void *pack, s32 lang);

void Bracket_Load(LBracket *b);
void Bracket_PlayCpuMatch(LBracket *b, s32 match);
s32 Bracket_GetViewX(s32 view);
s32 Bracket_GetPosX(s32 pos);
s32 Bracket_GetPosY(s32 pos);
void Bracket_InitMatches(LBracket *b);
s32 Bracket_IsPlayerOut(LBracket *b);
void Bracket_FillEntrants(LBracket *b);
void Bracket_ShuffleEntrants(LBracket *b);
void Bracket_SetChipTex(LBracket *b);
void Bracket_PlayCpuMatches(LBracket *b);
void Bracket_NextMatch(LBracket *b);
void Bracket_ApplyResult(LBracket *b);
void Bracket_StartVs(LBracket *b);
void Bracket_StartWin(LBracket *b);
void Bracket_UpdateImages(LBracket *b);
void Bracket_LoadImages(LBracket *b);
s32 Bracket_PickStage(s32 yamcha);
void Bracket_SetupBattle(LBracket *b);
void Bracket_GivePrizes(LBracket *b, s32 second);
s32 Bracket_DrawDragonBall(s32 tour, s32 level, s32 second);

/* ---- SoloSel (menu_l_b.c): one-character select of the modes 13..30 group ---- */

#define SOLOSEL_FLASH_NUM 3
#define SOLOSEL_COLS 7

/* A chosen character: the 0x30-byte record the select screens keep in gProgress (ESelEntry of menu_j.h). */
typedef struct LSelEntry {
    /* 0x00 */ s32 col;         /* grid column */
    /* 0x04 */ s32 row;         /* grid row */
    /* 0x08 */ s32 form;        /* index into the cell's form list */
    /* 0x0C */ s32 recCol;
    /* 0x10 */ s32 recRow;
    /* 0x14 */ s32 custom;      /* 0 = no items, 1..3 = one of the character's saved item sets */
    /* 0x18 */ s32 costume;
    /* 0x1C */ s32 chara;       /* the character id finally chosen */
    /* 0x20 */ LItemSet items;  /* the item set taken along (zero when custom == 0) */
} LSelEntry; /* 0x30 */

/* The choice being made. */
typedef struct SoloSelState {
    /* 0x00 */ LSelEntry entry;
    /* 0x30 */ s32 rowChara[SOLOSEL_COLS];     /* characters on the seven chips (a grid row, or the forms of a cell) */
    /* 0x4C */ s32 prevRowChara[SOLOSEL_COLS];
    /* 0x68 */ s32 flags;                      /* SOLOSEL_SEL_ */
    /* 0x6C */ s32 image;                      /* character whose large picture, name and form text are shown */
    /* 0x70 */ s32 step;                       /* SOLOSEL_STEP_ */
    /* 0x74 */ s32 rowAnim;                    /* the chip row is scrolling */
} SoloSelState; /* 0x78 */

#define SOLOSEL_SEL_IMAGE_CHANGE 1
#define SOLOSEL_SEL_IMAGE_READY 2
#define SOLOSEL_SEL_FORM 0x40    /* the chips show the forms of the chosen cell */
#define SOLOSEL_SEL_PANEL 0x800  /* set when the item panel is opened; never cleared or tested in this range */

#define SOLOSEL_STEP_CHARA 0     /* cursor on the grid */
#define SOLOSEL_STEP_FORM 1      /* cursor on the forms */
#define SOLOSEL_STEP_CUSTOM 2    /* item set plates */
#define SOLOSEL_STEP_PANEL 3     /* item panel of the chosen set */
#define SOLOSEL_STEP_HELP 4      /* item help window */
#define SOLOSEL_STEP_COLOR 5     /* costume plates */

typedef struct SoloSel {
    /* 0x0000 */ u32 *pack;          /* this screen's section of archive 3 (compressed) */
    /* 0x0004 */ u32 *res;           /* the same unpacked */
    /* 0x0008 */ void *imageFile;    /* 0x16800 bytes: compressed large picture (file 0x71C + character) */
    /* 0x000C */ void *imageRes;     /* 0x20800 bytes: the same unpacked */
    /* 0x0010 */ u32 *chips;         /* section 27: pack of the small character pictures */
    /* 0x0014 */ void *nameText;     /* section 28 */
    /* 0x0018 */ void *formText;     /* section 29 */
    /* 0x001C */ void *msgText;      /* section 25 */
    /* 0x0020 */ void *subtitles;    /* section 26 */
    /* 0x0024 */ MFlash flash[SOLOSEL_FLASH_NUM]; /* 0 picture and guides, 1 chips, 2 custom / colour plates */
    /* 0x00A8 */ void *bg;           /* section 32: background picture */
    /* 0x00AC */ u8 *texA[20];
    /* 0x00FC */ u8 *texC[11];
    /* 0x0128 */ u8 *texB[25];
    /* 0x018C */ s32 flags;          /* SOLOSEL_ */
    /* 0x0190 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x0194 */ s32 loadState;      /* the large picture's loader */
    /* 0x0198 */ s32 unk198;
    /* 0x019C */ s32 timer;          /* frames until the fade out starts */
    /* 0x01A0 */ s32 helpItem;       /* item the help window explains */
    /* 0x01A4 */ SoloSelState state;
    /* 0x021C */ SoloSelState *sel;  /* &state */
    /* 0x0220 */ s32 gridCount;
    /* 0x0224 */ LChrCell *grid;
    /* 0x0228 */ s32 gridOutCount;
    /* 0x022C */ LChrCell gridBuf[LCHR_CELL_MAX];
    /* 0x1960 */ s32 rows;           /* grid rows */
    /* 0x1964 */ s32 customCount;    /* item-set plates that can be chosen: 4, or 1 with SOLOSEL_NO_CUSTOM */
    /* 0x1968 */ s32 colorCount;     /* costumes of the character */
    /* 0x196C */ f32 imageAlpha;
    /* 0x1970 */ s32 endStep;        /* SOLOSEL_END_: set to 1 when the character is decided */
    /* 0x1974 */ s32 talker;         /* which of the two guides is talking */
    /* 0x1978 */ s32 blink[2];
    /* 0x1980 */ s32 talk[2];
    /* 0x1988 */ MTextBox box[2];    /* character name, form name */
    /* 0x1AA0 */ void *items;        /* item table of common file 4 */
} SoloSel; /* 0x1AA4 */

#define SOLOSEL_STARTED 2        /* the first chip was lit */
#define SOLOSEL_DONE 8
#define SOLOSEL_LEAVING 0x10     /* the leave timer runs */
#define SOLOSEL_NO_CUSTOM 0x40   /* progress mode 21: only the "no items" plate can be chosen */
#define SOLOSEL_GREETED 0x80     /* the guide's greeting was started */

/* endStep: the guide's closing line */
#define SOLOSEL_END_NONE 0
#define SOLOSEL_END_SPEAK 1
#define SOLOSEL_END_WAIT 2
#define SOLOSEL_END_LEAVE 3

/* Voice_GetStat result when nothing is playing. */
#define SOLO_VOICE_IDLE 5
/* Voice bank base of this mode group's guides (Voice_PlayWithSubtitle). */
#define SOLO_VOICE_BASE 0x8765

/* loadState */
#define SOLOSEL_LOAD_REQUEST 1
#define SOLOSEL_LOAD_READ 2
#define SOLOSEL_LOAD_UNPACK 3
#define SOLOSEL_LOAD_SHOWN 4
#define SOLOSEL_LOAD_ABORT 5
#define SOLOSEL_LOAD_RESTART 6

/* gProgress as this screen uses it. */
typedef struct SoloProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x424];
    /* 0x440 */ LSelEntry lastEntry;   /* side 0's record of the versus select: the last choice */
} SoloProgress;

#define SOLO_PROG ((SoloProgress *)gProgress)

extern SoloSel *gSoloSel;        /* 0x3B7348 */

void SoloSel_SwapRowTex(void);
void SoloSel_SetRowTex(void);
void SoloSel_UpdateImage(void);
void SoloSel_ChangeImage(void);
void SoloSel_ClipGoto(s32 movie, s32 kind, char *label);
void SoloSel_Init(s32 section);
void SoloSel_Term(void);
void SoloSel_Draw(void);
void SoloSel_Update(void);
void SoloSel_Input(s32 *result);
void SoloSel_UpdateEnd(void);
s32 SoloSel_Run(s32 section);

#endif
