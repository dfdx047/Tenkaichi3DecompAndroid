#ifndef MENU_PORT_CHARSEL_H
#define MENU_PORT_CHARSEL_H

#include "common.h"

#ifdef PORT
/* What the character selects (menu_c_e.c, menu_e_b.c) tell the screen of src/menu/port_charsel.c each frame. */
#define CS_TEAM_MAX 5

typedef struct CsCell { /* MChrCell, TsCell */
    s32 id;
    s32 formCount;
    s32 form[7];
} CsCell;

typedef struct CsView {
    void *chipPack;
    void *nameText;
    void *formText;
    CsCell *cells;
    CsCell *custom;
    MTexRes *face;
    s32 cellCount, customCount;
    s32 side, players;
    s32 col, row, customCol, customRow;
    s32 chara, color, plate;
    s32 colorCount, plateCount;
    s32 showCustom;              /* the list of saved characters instead of the grid */
    s32 tabWant;                 /* the step is one of the personalization tab's */
    s32 plateActive, colorActive, formActive;
    s32 faceReady;
    f32 faceAlpha;
    s32 isTeam;
    s32 teamChara[CS_TEAM_MAX];  /* -1: none */
    s32 cur, teamActive, cost, dpMax;
    s32 done;                    /* this side has finished choosing */
} CsView;

void PortCharSel_Begin(s32 allow);
void PortCharSel_End(void);
void PortCharSel_Abandon(void);
s32 PortCharSel_Shown(s32 stagePhase);
void PortCharSel_MuteOn(void);
/* view[0] alone, or with split the two sides, view[side]: two people choosing, both seen as they choose */
void PortCharSel_Frame(const CsView *view, s32 split, s32 stagePhase);
void PortCharSel_Draw(void);
void PortCharSel_DrawItems(void);
void PortCharSel_DrawEnd(void);
s32 PortCharSel_NetSide(s32 fallback);
/* How many forms a row of the list holds. It decides what the pad does, so it may depend only on what is the same on
   every machine of an online match: never on the window's shape or a setting. */
s32 PortCharSel_FormRowSize(s32 count, s32 split, s32 team);
/* The form above or below this one in that list; -1 if the list has one row. */
s32 PortCharSel_FormOtherRow(s32 count, s32 form, s32 split, s32 team);
#endif
#endif
