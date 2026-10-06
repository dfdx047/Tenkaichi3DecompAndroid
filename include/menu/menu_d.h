#ifndef MENU_MENU_D_H
#define MENU_MENU_D_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x342190..0x348D78: ONE object, the character / stage / music select screen of the
 * versus modes ("CharSel"; names are guesses), src/menu/menu_c_e.c. (The header keeps the placeholder stem
 * "menu_d" of the chunk that held the middle of the object, 0x342588..0x348710; the three functions in front
 * came from chunk "menu_c" and CharSel_Run at 0x348710 from chunk "menu_e".)
 */

/* A cell of the character grid (include/battle/view_a.h has the original). */
typedef struct MChrCell {
    /* 0x00 */ s32 id;         /* character id 0..0xA0, or MCHR_ */
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} MChrCell; /* 0x24 */

#define MCHR_COLS 7
/* What this screen does with the marker cells: 0xA1 is the RANDOM cell (a character is drawn with Rand_Range
   when it is confirmed) and 0xA3 opens the list of saved custom characters (gSaveData->rec). 
   include/battle/view_a.h has the same values (CHRGRID_ID_RANDOM 0xA1, CHRGRID_ID_CUSTOM 0xA3). */
#define MCHR_RANDOM 0xA1
#define MCHR_LOCKED 0xA2
#define MCHR_CUSTOM 0xA3
#define MCHR_EMPTY 0xA4
#define MCHR_CELL_MAX 165
#define MCHR_CUSTOM_MAX 14

#define CHARSEL_SIDES 2
#define CHARSEL_FLASH_NUM 5

/* An item set: eight item ids (1-based, 0 = empty). */
typedef struct CharSelItems {
    u16 id[8];
} CharSelItems; /* 0x10 */

/* gSaveData: the two tables this screen copies item sets from (include/sys/save.h has the whole layout). */
typedef struct CharSelSaveCustom {
    /* 0x00 */ CharSelItems set[3];
    /* 0x30 */ s32 unk30;
    /* 0x34 */ u16 level;
    /* 0x36 */ u16 unk36;
} CharSelSaveCustom; /* 0x38: SaveCustom */

typedef struct CharSelSaveRec {
    /* 0x00 */ CharSelItems items;
    /* 0x10 */ u8 unk10[0xC];
} CharSelSaveRec; /* 0x1C: SaveRec */

typedef struct CharSelSave {
    /* 0x0000 */ u8 unk0[0x1808];
    /* 0x1808 */ CharSelSaveCustom custom[97]; /* indexed by the cell of the character grid (col + row * 7) */
    /* 0x2D40 */ CharSelSaveRec rec[14];       /* the saved custom characters */
} CharSelSave;

#define CSSAVE ((CharSelSave *)gSaveData)

/* What one side has chosen. The first 0x30 bytes are kept in gProgress (+0x440 side 0, +0x530 side 1). */
typedef struct CharSelSide {
    /* 0x00 */ s32 col;            /* cursor column in the character grid */
    /* 0x04 */ s32 row;            /* cursor row in the character grid */
    /* 0x08 */ s32 form;           /* which chip of the form reel is chosen */
    /* 0x0C */ s32 customCol;      /* cursor in the list of saved custom characters (7 x 2) */
    /* 0x10 */ s32 customRow;
    /* 0x14 */ s32 customPlate;    /* item set: 0 = none, 1..3 = set of gSaveData->custom[] (0 / 1 for a saved character) */
    /* 0x18 */ s32 colorPlate;     /* costume, 0..colorCount-1 */
    /* 0x1C */ s32 picked;         /* the character finally chosen (a form id; drawn at random for the random cell) */
    /* 0x20 */ CharSelItems items; /* the item set that goes with it, zero for none */
    /* 0x30 */ s32 chip[7];        /* character ids on the seven chips of the reel */
    /* 0x4C */ s32 prevChip[7];    /* the same before the last change */
    /* 0x68 */ s32 flags;          /* CHARSEL_SIDE_ */
    /* 0x6C */ s32 chara;          /* character under the cursor: portrait file 0x2F9 + chara, name line */
    /* 0x70 */ s32 state;          /* step of this side's choice (CharSel_Input) */
    /* 0x74 */ s32 mask;           /* non-zero while the chips are hidden for a reel change */
    /* 0x78 */ s32 unk78[30];
} CharSelSide; /* 0xF0 */

#define CHARSEL_SIDE_FACE_CHANGE 1 /* the portrait must be reloaded */
#define CHARSEL_SIDE_FACE_READY 2  /* the portrait is loaded and fades in */
#define CHARSEL_SIDE_FORMS 0x40       /* the reel shows the forms of a cell */
#define CHARSEL_SIDE_CUSTOM 0x80      /* the reel shows the saved custom characters */
#define CHARSEL_SIDE_ITEMS 0x800      /* the item panel of this side is open: the other side's input is ignored */

/* CharSelSide.state */
#define CHARSEL_ST_GRID 0
#define CHARSEL_ST_FORM 1
#define CHARSEL_ST_ITEMSET 2
#define CHARSEL_ST_ITEMS 3
#define CHARSEL_ST_ITEMINFO 4
#define CHARSEL_ST_COLOR 5
#define CHARSEL_ST_CUSTOM 6
#define CHARSEL_ST_DONE 7
/* CharSelStage.state */
#define CHARSEL_ST_STAGE 8
#define CHARSEL_ST_BGM 9

/* The stage and music choice (0x54 bytes used). */
typedef struct CharSelStage {
    /* 0x00 */ s32 col;        /* cursor column in the stage grid (6 columns) */
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 chip[2][6]; /* stage ids on the six chips of the reel: [0] now, [1] before the last change */
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 stage;      /* stage id under the cursor: picture file 0x39D + stage */
    /* 0x40 */ s32 state;      /* step of the stage / music choice (CharSel_Input) */
    /* 0x44 */ s32 mask;       /* non-zero while the stage chips are hidden */
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 bgm;        /* index in the music list */
    /* 0x50 */ s32 bgmPlaying; /* index of the entry that is playing */
} CharSelStage; /* 0x54 */

/* The part of a side's choice that gProgress keeps between visits. */
typedef struct CharSelPick {
    s32 unk0[12];
} CharSelPick; /* 0x30 */

/* gProgress: the fields this screen uses (include/menu/menu_a.h has the head). */
typedef struct CharSelProgress {
    /* 0x000 */ u8 unk0[0x440];
    /* 0x440 */ struct {
        CharSelPick pick;
        u8 unk30[0xC0];
    } side[CHARSEL_SIDES];     /* 0xF0 each */
    /* 0x620 */ s32 players;   /* copied to CharSel.players; mode 45 forces 0 */
    /* 0x624 */ s32 unk624;
    /* 0x628 */ s32 stage;     /* index in the stage grid */
    /* 0x62C */ s32 bgm;       /* id in the music list */
} CharSelProgress;

#define CSPROG ((CharSelProgress *)gProgress)

#define CHARSEL_BGM_RANDOM 0x18          /* the "random" entry of the music list */
#define CHARSEL_STAGE_RANDOM 0x23        /* the "random" stage id; ids from here on are not real stages */
#define CHARSEL_STAGE_COLS 6             /* columns of the stage grid (gProgress keeps col + row * 6) */
#define CHARSEL_BGM_FIRST 0x10B16        /* Bgm_Play id of music list entry 0 */
#define CHARSEL_BGM_RANDOM_FIRST 0x10B1E /* + Rand_Range(9) */

typedef struct CharSel {
    /* 0x0000 */ u32 *pack;                 /* this screen's section of archive 5 or 6 (compressed) */
    /* 0x0004 */ u32 *res;                  /* the same unpacked: a pack of 57 sections */
    /* 0x0008 */ void *faceFile[CHARSEL_SIDES]; /* 0x16800 bytes each: compressed portrait */
    /* 0x0010 */ MTexRes *faceRes[CHARSEL_SIDES]; /* 0x20800 bytes each: the same unpacked */
    /* 0x0018 */ void *stageFile;           /* 0x3B800 bytes: compressed stage picture */
    /* 0x001C */ MTexRes *stageRes[2];      /* 0x43000 bytes each: unpacked, two buffers for the cross fade */
    /* 0x0024 */ u32 *chipPack;             /* section 45: 165 texture lists, section id + 1 = chip of a character */
    /* 0x0028 */ void *nameText;            /* section 43: character names */
    /* 0x002C */ void *formText;            /* section 44: form names */
    /* 0x0030 */ u32 *stagePack;            /* section 46: 38 texture lists */
    /* 0x0034 */ MFlash flash[CHARSEL_FLASH_NUM]; /* 0 background, 1 / 2 the sides' reels, 3 / 4 their plates */
    /* 0x0110 */ MTexRes *bg[2];            /* stage picture drawn behind: new, old */
    /* 0x0118 */ u8 *tex[49];               /* movie 0 */
    /* 0x01DC */ u8 *plateTex[CHARSEL_SIDES][11]; /* movies 3 and 4 */
    /* 0x0234 */ u8 *sideTex[CHARSEL_SIDES][25];  /* movies 1 and 2 */
    /* 0x02FC */ s32 flags;                 /* CHARSEL_ */
    /* 0x0300 */ s32 faceState;             /* CHARSEL_LOAD_: the portrait loader */
    /* 0x0304 */ s32 stageState;            /* CHARSEL_LOAD_: the stage picture loader */
    /* 0x0308 */ s32 faceSide;              /* side whose portrait is being loaded */
    /* 0x030C */ s32 stageBuf;              /* which of stageRes the next picture goes to */
    /* 0x0310 */ s32 timer;                 /* 60 when the stage is confirmed (counted by the Run function) */
    /* 0x0314 */ s32 itemInfo;              /* item shown by the description window (ItemHelp_Draw), from ItemPanel_Input */
    /* 0x0318 */ CharSelSide sideData[CHARSEL_SIDES];
    /* 0x04F8 */ CharSelSide *side[CHARSEL_SIDES];
    /* 0x0500 */ CharSelStage stageData;
    /* 0x0554 */ CharSelStage *stage;
    /* 0x0558 */ s32 masterCount[CHARSEL_SIDES]; /* cells in the master grid */
    /* 0x0560 */ MChrCell *cells[CHARSEL_SIDES]; /* each side's grid (first the master list of section 42) */
    /* 0x0568 */ s32 cellCount[CHARSEL_SIDES];
    /* 0x0570 */ MChrCell grid[CHARSEL_SIDES][MCHR_CELL_MAX];
    /* 0x33D8 */ s32 customCount[CHARSEL_SIDES];
    /* 0x33E0 */ MChrCell custom[CHARSEL_SIDES][MCHR_CUSTOM_MAX];
    /* 0x37D0 */ s32 rows[CHARSEL_SIDES];   /* rows of each side's grid */
    /* 0x37D8 */ s32 customRows[CHARSEL_SIDES]; /* item-set plates offered: 4, or 2 for a saved custom character */
    /* 0x37E0 */ s32 colorCount[CHARSEL_SIDES]; /* costumes of the character (ChrTbl_WrapCostume), 2 for random */
    /* 0x37E8 */ s32 stageRows;             /* 6 */
    /* 0x37EC */ s32 players;               /* gProgress + 0x620: 1 = one pad per side; 0 / 2 = pad 0 chooses side 0, then side 1 */
    /* 0x37F0 */ s32 bgAlpha[2];            /* of bg[0] / bg[1], 0..0x80 */
    /* 0x37F8 */ f32 faceAlpha[CHARSEL_SIDES];
    /* 0x3800 */ s32 turn;                  /* with one pad: the side it is choosing for (0, then 1) */
    /* 0x3804 */ s32 *stageIds;             /* section 39 + 0x10 */
    /* 0x3808 */ s32 *bgmIds;               /* section 56 + 0x10 */
    /* 0x380C */ s32 stageCount;
    /* 0x3810 */ s32 bgmCount;
    /* 0x3814 */ s32 faceMask;              /* non-zero while the portraits are hidden */
    /* 0x3818 */ MTextBox nameBox[CHARSEL_SIDES];
    /* 0x3930 */ MTextBox formBox[CHARSEL_SIDES];
    /* 0x3A48 */ void *unk3A48;             /* section 2 of common file 4 (gCommonRes->data[2]); not used in this range */
} CharSel; /* 0x3A4C */

#define CHARSEL_STARTED 2          /* the cursors were lit once */
#define CHARSEL_STAGE_PHASE 4      /* both sides are done: the stage / music choice has the input */
#define CHARSEL_LEAVING 8          /* the stage is confirmed */
#define CHARSEL_FLAG10 0x10        /* set together with CHARSEL_LEAVING; the one CharSel_Run tests to count the timer */
#define CHARSEL_STAGE_CHANGE 0x20  /* the stage under the cursor changed: load its picture */
#define CHARSEL_STAGE_READY 0x40   /* the picture is loaded and fades in */

/* faceState / stageState: one step per frame; the two loaders share the file request queue and take turns */
#define CHARSEL_LOAD_REQUEST 1
#define CHARSEL_LOAD_READ 2
#define CHARSEL_LOAD_UNPACK 3
#define CHARSEL_LOAD_IDLE 4
#define CHARSEL_LOAD_ABORT 5
#define CHARSEL_LOAD_RESTART 6

#define CHARSEL_FACE_FILE 0x2F9   /* + character id */
#define CHARSEL_STAGE_FILE 0x39D  /* + stage id */
#ifdef PORT
/* The stage thumbnails are 0x39D..0x3C0 (one per stage of the disc); the loading screen files start right after,
   at 0x3C1. A stage added from outside the disc has an id past 0x23, so its thumbnail would fall on a loading
   screen: it reuses an existing thumbnail instead. */
#define CHARSEL_STAGE_FILE_ID(id) (CHARSEL_STAGE_FILE + ((id) < 0x24 ? (id) : (id) - 0x24))
#else
#define CHARSEL_STAGE_FILE_ID(id) (CHARSEL_STAGE_FILE + (id))
#endif

/* kinds of CharSel_ClipGoto */
#define CHARSEL_CLIP_CHIP 0
#define CHARSEL_CLIP_FORM_CHIP 1
#define CHARSEL_CLIP_CUSTOM_PLATE 2
#define CHARSEL_CLIP_COLOR_PLATE 5
#define CHARSEL_CLIP_CUSTOM_CHIP 6
#define CHARSEL_CLIP_STAGE_CHIP 8
#define CHARSEL_CLIP_NONE 9          /* does nothing (used for the music entry) */

#define CHARSEL_BGM_LOCKED 0x19
#define CHARSEL_STAGE_BGM_COL 6       /* column of the stage grid that is the music entry */

extern CharSel *gCharSel; /* 0x3B38D4, first word of the object's .data */

void CharSel_SetStageChips(void);
void CharSel_SetRowChips(s32 side);
void CharSel_SetCellFormChips(s32 side);
void CharSel_SetCustomChips(s32 side);
void CharSel_SetFormChips(s32 side);
void CharSel_UpdateFaceLoad(void);
void CharSel_RequestFace(s32 side);
void CharSel_UpdateStageLoad(void);
void CharSel_RequestStage(void);
void CharSel_ClipGoto(s32 flash, s32 side, s32 kind, char *label);
void CharSel_Init(s32 section);
void CharSel_Term(void);
void CharSel_Draw(void);
void CharSel_Update(void);
void CharSel_Input(s32 *running);
s32 CharSel_Run(s32 section);

#endif
