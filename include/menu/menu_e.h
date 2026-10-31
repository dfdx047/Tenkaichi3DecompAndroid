#ifndef MENU_MENU_E_H
#define MENU_MENU_E_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x348710..0x34D368 (placeholder stem "menu_e"). Two pieces (the .data words are
 * 0x3B38D4.., the .rodata runs from 0x3B38F0 through this chunk's 0x3B3D40..0x3B4188 and on):
 *
 *   (menu_e.c)  0x348710..0x348D78  CharSel_Run: the frame loop and the battle hand-off of the one-on-one
 *                                   character select; now the end of src/menu/menu_c_e.c, which uses the
 *                                   CharSel view of include/menu/menu_d.h (the partial view that was here
 *                                   is gone)
 *   menu_e_b.c  0x348D78..0x351C38  TeamSel: the team character select (teams of up to five, with or without
 *                                   a DP limit; work pointer 0x3B38D8). Written as two chunks (the cut was
 *                                   at 0x34D368, in front of Update / Input / Run), now one file; what the
 *                                   second chunk added to the layouts is in include/menu/menu_f.h, which
 *                                   includes this header.
 *
 * All names are guesses from what the code does. The structures below are this chunk's own views.
 */

/* A cell of the character grid (include/battle/view_a.h has the original). */
typedef struct TsCell {
    /* 0x00 */ s32 id;         /* character id 0..0xA0, 0xA1 random, 0xA2 locked, 0xA3 custom, 0xA4 filler */
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} TsCell; /* 0x24 */

#define TS_COLS 7
#define TS_RANDOM 0xA1       /* the cell that draws a character at random */
#define TS_LOCKED 0xA2
#define TS_CUSTOM 0xA3       /* the cell that opens the list of saved custom characters */
#define TS_EMPTY 0xA4
#define TS_CELL_MAX 165
#define TS_CUSTOM_MAX 14
#define TS_STAGE_COLS 6
#define TS_STAGE_RANDOM 0x23
#define TS_BGM_RANDOM 0x18

#ifdef PORT
#define TS_FACE_FILE (PortGame_IsBT4() ? 0x71C : 0x2F9)   /* + character id: compressed portrait */
#else
#define TS_FACE_FILE 0x2F9   /* + character id: compressed portrait */
#endif
#ifdef PORT
#define TS_STAGE_FILE (PortGame_IsBT4() ? 0x6B9 : 0x39D)  /* + stage id: compressed stage picture */
#else
#define TS_STAGE_FILE 0x39D  /* + stage id: compressed stage picture */
#endif

/* An item set: eight item ids, one object (it is copied whole with a structure assignment; TeamSel_Input only
   compiles to the original's code that way). */
typedef struct TsItemSet {
    u16 id[8];
} TsItemSet; /* 0x10 */

/* What a side chose for one fighter. Kept in gProgress between screens (five per side). */
typedef struct TsMember {
    /* 0x00 */ s32 col;         /* cursor column in the character grid */
    /* 0x04 */ s32 row;         /* cursor row in the character grid */
    /* 0x08 */ s32 form;        /* chip of the form reel */
    /* 0x0C */ s32 customCol;   /* cursor in the custom-character list */
    /* 0x10 */ s32 customRow;
    /* 0x14 */ s32 plate;       /* item-set plate the cursor is on (0..3) */
    /* 0x18 */ s32 color;       /* costume: plate the colour cursor is on (0..3) */
    /* 0x1C */ s32 chara;       /* the chosen character id, -1 = none yet */
    /* 0x20 */ TsItemSet items; /* equipped items (BattleItemSet) */
} TsMember; /* 0x30 */

#define TS_MEMBER_MAX 5

typedef struct TsTeam {
    TsMember member[TS_MEMBER_MAX];
} TsTeam; /* 0xF0 */

/* gProgress as the versus screens use it (menu_a.h has the head). */
typedef struct TsProgress {
    /* 0x000 */ u8 unk0[0x14];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x424];
    /* 0x440 */ TsTeam team[2];     /* the two sides' choices, kept for the next visit */
    /* 0x620 */ s32 players;        /* 0 pad against CPU, 1 pad against pad, 2 CPU against CPU */
    /* 0x624 */ s32 battleType;     /* 2 = team battle with a DP limit */
    /* 0x628 */ s32 stageCell;      /* cursor in the stage grid: col + row * 6 */
    /* 0x62C */ s32 bgm;            /* music id */
    /* 0x630 */ s32 dpLevel;        /* 0..2: DP limit 10 / 15 / 20 */
} TsProgress;

#define gTsProgress ((TsProgress *)gProgress)

/* ---- TeamSel ---- */

#define TEAMSEL_SIDES 2
#define TEAMSEL_FLASH_NUM 7

/* One side of the team select. */
typedef struct TeamSelSide {
    /* 0x000 */ TsMember member[TS_MEMBER_MAX];
    /* 0x0F0 */ s32 chip[2][TS_COLS]; /* [0] character ids on the seven chips of the reel, [1] before the last change */
    /* 0x128 */ s32 flags;          /* TEAMSEL_SIDE_ */
    /* 0x12C */ s32 cur;            /* member being chosen; 5 = on the menu plate */
    /* 0x130 */ s32 cost;           /* DP total of the team */
    /* 0x134 */ s32 memberCount;
    /* 0x138 */ s32 chara;          /* character under the cursor: portrait file, name line; negative = none */
    /* 0x13C */ s32 state;          /* step of this side's choice */
    /* 0x140 */ s32 mask;           /* non-zero while the chips are hidden for a reel change */
    /* 0x144 */ s32 unk144[81];
} TeamSelSide; /* 0x288 */

#define TEAMSEL_SIDE_FACE_CHANGE 1  /* the portrait must be reloaded */
#define TEAMSEL_SIDE_FACE_READY 2   /* the portrait is loaded and fades in */
#define TEAMSEL_SIDE_FORM 0x40      /* the member is being chosen from the form reel */
#define TEAMSEL_SIDE_NO_FACE 0x400  /* nothing to show (chara < 0) */

/* The stage and music choice. */
typedef struct TeamSelStage {
    /* 0x00 */ s32 col;        /* cursor column in the stage grid (6 columns; 6 = on the music plate) */
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 chip[2][TS_STAGE_COLS]; /* [0] stage ids on the six chips, [1] before the last change */
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 stage;      /* stage id under the cursor */
    /* 0x40 */ s32 state;      /* 9 stage grid, 10 music list (next chunk) */
    /* 0x44 */ s32 mask;       /* non-zero while the stage chips are hidden */
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 bgmCursor;  /* index in the music list while it is open */
    /* 0x50 */ s32 bgm;        /* index of the entry chosen (and playing) */
} TeamSelStage; /* 0x54 */

typedef struct TeamSel {
    /* 0x0000 */ u32 *pack;                 /* this screen's section of archive 5 (compressed) */
    /* 0x0004 */ u32 *res;                  /* the same unpacked: a pack of 57 sections */
    /* 0x0008 */ void *faceFile[TEAMSEL_SIDES]; /* 0x16800 bytes each: compressed portrait */
    /* 0x0010 */ MTexRes *faceRes[TEAMSEL_SIDES]; /* 0x20800 bytes each: the same unpacked */
    /* 0x0018 */ void *stageFile;           /* 0x3B800 bytes: compressed stage picture */
    /* 0x001C */ MTexRes *stageRes[2];      /* 0x43000 bytes each: unpacked, two buffers for the cross fade */
    /* 0x0024 */ u32 *chipPack;             /* section 45: 165 texture lists, section id + 1 = chip of a character */
    /* 0x0028 */ void *nameText;            /* section 43: character names */
    /* 0x002C */ void *formText;            /* section 44: form names */
    /* 0x0030 */ u32 *stagePack;            /* section 46: 38 texture lists */
    /* 0x0034 */ MFlash flash[TEAMSEL_FLASH_NUM]; /* 0 background, 1 / 2 the teams, 3 / 4 the reels, 5 / 6 the plates */
    /* 0x0168 */ MTexRes *bg[2];            /* stage picture drawn behind: new, old */
    /* 0x0170 */ u8 *tex[49];               /* movie 0 */
    /* 0x0234 */ u8 *plateTex[TEAMSEL_SIDES][11]; /* movies 5 and 6 */
    /* 0x028C */ u8 *sideTex[TEAMSEL_SIDES][25];  /* movies 3 and 4 */
    /* 0x0354 */ u8 *teamTex[TEAMSEL_SIDES][13];  /* movies 1 and 2 */
    /* 0x03BC */ s32 flags;                 /* TEAMSEL_ */
    /* 0x03C0 */ s32 faceState;             /* TEAMSEL_LOAD_: the portrait loader */
    /* 0x03C4 */ s32 stageState;            /* TEAMSEL_LOAD_: the stage picture loader */
    /* 0x03C8 */ s32 faceSide;              /* side whose portrait is being loaded */
    /* 0x03CC */ s32 stageBuf;              /* which of stageRes the next picture goes to */
    /* 0x03D0 */ s32 timer;                 /* frames from the stage choice to the fade out (next chunk) */
    /* 0x03D4 */ s32 help;                  /* argument of ItemHelp_Draw: which item the help window explains */
    /* 0x03D8 */ TeamSelSide sideData[TEAMSEL_SIDES];
    /* 0x08E8 */ TeamSelSide *side[TEAMSEL_SIDES];
    /* 0x08F0 */ TsMember backup[TEAMSEL_SIDES]; /* the member being changed, as it was (next chunk) */
    /* 0x0950 */ TeamSelStage stageData;
    /* 0x09A4 */ TeamSelStage *stage;
    /* 0x09A8 */ s32 masterCount[TEAMSEL_SIDES]; /* cells in each side's grid */
    /* 0x09B0 */ TsCell *cells[TEAMSEL_SIDES]; /* each side's grid (first the master list of section 42) */
    /* 0x09B8 */ s32 cellCount[TEAMSEL_SIDES];
    /* 0x09C0 */ TsCell grid[TEAMSEL_SIDES][TS_CELL_MAX];
    /* 0x3828 */ s32 customCount[TEAMSEL_SIDES];
    /* 0x3830 */ TsCell custom[TEAMSEL_SIDES][TS_CUSTOM_MAX];
    /* 0x3C20 */ s32 rows[TEAMSEL_SIDES];   /* rows of each side's grid */
    /* 0x3C28 */ s32 plateCount[TEAMSEL_SIDES];  /* item-set plates offered */
    /* 0x3C30 */ s32 colorCount[TEAMSEL_SIDES];
    /* 0x3C38 */ s32 players;               /* gProgress->players */
    /* 0x3C3C */ s32 battleType;            /* gProgress->battleType: 2 = with a DP limit */
    /* 0x3C40 */ s32 dpLevel;               /* gProgress->dpLevel */
    /* 0x3C44 */ s32 dpMax;                 /* 10 / 15 / 20 */
    /* 0x3C48 */ s32 bgAlpha[2];            /* of bg[0] / bg[1], 0..0x80 */
    /* 0x3C50 */ f32 faceAlpha[TEAMSEL_SIDES];
    /* 0x3C58 */ s32 base;                  /* side that pad 0 is choosing for (next chunk) */
    /* 0x3C5C */ s32 *stageIds;             /* section 39 + 0x10 */
    /* 0x3C60 */ s32 *bgmIds;               /* section 56 + 0x10 */
    /* 0x3C64 */ s32 stageCount;
    /* 0x3C68 */ s32 bgmCount;
    /* 0x3C6C */ s32 faceMask;              /* non-zero while the portraits are hidden */
    /* 0x3C70 */ MTextBox nameBox[TEAMSEL_SIDES];
    /* 0x3D88 */ MTextBox formBox[TEAMSEL_SIDES];
    /* 0x3EA0 */ void *unk3EA0;             /* common file 4, section 2 */
} TeamSel; /* 0x3EA4 */

#define TEAMSEL_STAGE_CHANGE 0x20  /* the stage under the cursor changed: load its picture */
#define TEAMSEL_STAGE_READY 0x40   /* the picture is loaded and fades in */

/* faceState / stageState: one step per frame; the two loaders share the file request queue and take turns */
#define TEAMSEL_LOAD_REQUEST 1
#define TEAMSEL_LOAD_READ 2
#define TEAMSEL_LOAD_UNPACK 3
#define TEAMSEL_LOAD_IDLE 4
#define TEAMSEL_LOAD_ABORT 5
#define TEAMSEL_LOAD_RESTART 6

/* kinds of TeamSel_ClipGoto */
#define TEAMSEL_CLIP_CHIP 0
#define TEAMSEL_CLIP_FORM_CHIP 1
#define TEAMSEL_CLIP_CUSTOM_PLATE 2
#define TEAMSEL_CLIP_COLOR_PLATE 5
#define TEAMSEL_CLIP_TEAM 6
#define TEAMSEL_CLIP_CUSTOM_CHIP 7
#define TEAMSEL_CLIP_STAGE_CHIP 9

/* The work pointer: second word of the object's .data. */
extern TeamSel *gTeamSel; /* 0x3B38D8 */

void TeamSel_SumCost(s32 side, s32 skipCur);
s32 TeamSel_IsCharaFree(s32 side, s32 chara);
s32 TeamSel_FitsDp(s32 side, s32 chara);
void TeamSel_RemoveMember(s32 side, s32 idx);
void TeamSel_SetStageChips(void);
void TeamSel_SetChips(s32 side);
void TeamSel_SetGridFormChips(s32 side);
void TeamSel_SetCustomChips(s32 side);
void TeamSel_SetFormChips(s32 side);
void TeamSel_SetTeamTex(s32 side);
void TeamSel_UpdateFaceLoad(void);
void TeamSel_RequestFace(s32 side);
void TeamSel_UpdateStageLoad(void);
void TeamSel_RequestStage(void);
void TeamSel_ClipGoto(s32 flash, s32 side, s32 kind, char *label);
void TeamSel_Init(s32 section);
void TeamSel_Term(void);
void TeamSel_Draw(void);
#ifdef PORT
s32 TeamSel_PickOnline(TsMember *member,s32 cpu,s32 *level,s32 revision);
#endif

#endif
