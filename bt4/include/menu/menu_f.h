#ifndef MENU_MENU_F_H
#define MENU_MENU_F_H

#include "menu/menu_a.h"
#include "menu/menu_e.h"

/*
 * Menu overlay DBZP.BIN, 0x34D368..0x351C38 (placeholder stem "menu_f"): the last three functions of the
 * TeamSel object, the team select screen of the versus modes (up to five characters per side, then the stage
 * and the music, then the battle setup): TeamSel_Update, TeamSel_Input, TeamSel_Run.
 *
 * The head of the object is the previous chunk (include/menu/menu_e.h); the object ends exactly at 0x351C38,
 * where the ItemPanel code begins. The two chunks are ONE source file, src/menu/menu_e_b.c (merged):
 * TeamSel_Input matches only when TeamSel_ClipGoto, TeamSel_SetChips and TeamSel_RequestFace are defined
 * above it.
 *
 * The layouts (TeamSel, TeamSelSide, TsMember, ...) are those of menu_e.h, included above; this header holds
 * only what the second chunk added.
 */


/* ======== additions of this chunk ======== */

/*
 * TS_RANDOM (0xA1) and TS_CUSTOM (0xA3): include/battle/view_a.h calls 0xA1 "custom" and 0xA3 "random". This
 * chunk shows it is the other way round: on 0xA1 the screen draws a character with Rand_Range when the member
 * is confirmed, on 0xA3 it opens the list of the fourteen saved custom characters.
 */
#define TS_CUSTOM_ROWS 2           /* rows of the custom-character list */
#define TS_CUR_MENU 5              /* TeamSelSide.cur on the "team complete" plate */
#define TS_BGM_LOCKED 0x19         /* music list id of a locked entry (BgmList_ApplyUnlocks) */
#define TS_BGM_FILE 0x816        /* Bgm_Play id of music list id 0 */

/* TeamSelSide.flags, further bits */
#define TEAMSEL_SIDE_CUSTOM 0x80   /* the member is being chosen from the custom-character list */
#define TEAMSEL_SIDE_PANEL 0x800   /* this side's item panel is open: the other side's pad is ignored */

/* TeamSelSide.state: what the side's pad does (the jump table of TeamSel_Input) */
#define TEAMSEL_ST_GRID 0          /* character grid */
#define TEAMSEL_ST_FORM 1          /* form reel */
#define TEAMSEL_ST_PLATE 2         /* item-set plates */
#define TEAMSEL_ST_PANEL 3         /* item panel of the plate (ItemPanel_Input) */
#define TEAMSEL_ST_PANEL_HELP 4    /* the help window over the item panel (ItemHelp_Open / ItemHelp_Close) */
#define TEAMSEL_ST_COLOR 5         /* costume plates */
#define TEAMSEL_ST_TEAM 6          /* member list */
#define TEAMSEL_ST_CUSTOM 7        /* custom-character list */
#define TEAMSEL_ST_DONE 8          /* team complete: waits for the other side */

/* TeamSelStage.state */
#define TEAMSEL_STAGE_GRID 9
#define TEAMSEL_STAGE_BGM 10

/* TeamSel.flags, further bits */
#define TEAMSEL_STARTED 2          /* the cursor chips were lit once */
#define TEAMSEL_STAGE 4            /* both teams are complete: pad 0 chooses the stage and the music */
#define TEAMSEL_DECIDED 8          /* the stage is chosen */
#define TEAMSEL_LEAVING 0x10       /* count `timer` down, then fade out */

/* TeamSel.players (gProgress + 0x620) */
#define TEAMSEL_PLAYERS_VS_CPU 0   /* pad 0 chooses side 0, then side 1; side 1 is the CPU */
#define TEAMSEL_PLAYERS_TWO 1      /* one pad per side; the battle is split screen */
#define TEAMSEL_PLAYERS_CPU_CPU 2  /* pad 0 chooses both teams; both sides are the CPU */

/* one more kind of TeamSel_ClipGoto */
#define TEAMSEL_CLIP_BGM 10        /* "mc_bgm_now" */

/* The two tables of gSaveData this screen reads (SaveCustom / SaveRec of include/sys/save.h; local view). */
typedef struct TsSaveCustom {
    /* 0x00 */ TsItemSet set[3];   /* the character's three item sets */
    /* 0x30 */ s32 unk30[2];
} TsSaveCustom; /* 0x38 */

typedef struct TsSaveRec {
    /* 0x00 */ TsItemSet items;    /* the saved custom character's items */
    /* 0x10 */ s32 unk10[3];
} TsSaveRec; /* 0x1C */

typedef struct TsSave {
    /* 0x0000 */ u8 unk0[0x1808];
    /* 0x1808 */ TsSaveCustom custom[97];        /* by character-grid cell index (row * 7 + col) */
    /* 0x2D40 */ TsSaveRec rec[TS_CUSTOM_MAX];   /* by custom-list cell index */
} TsSave;

#define TS_SAVE ((TsSave *)gSaveData)

void TeamSel_Update(void);
f32 TeamSel_Input(s32 *result);
s32 TeamSel_Run(s32 section);

#endif
