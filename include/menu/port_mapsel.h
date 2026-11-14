#ifndef MENU_PORT_MAPSEL_H
#define MENU_PORT_MAPSEL_H

#include "common.h"

#ifdef PORT
typedef struct MsView {
    s32 row, col, stage;
    s32 chip[2][6];
    s32 windowRow, windowIds[18];
    u32 *stageIds;
    s32 bgm, bgmPlaying;
    s32 bgmId;
    s32 stageCount, rowCount;
    s32 bgmCount;
    MTexRes *preview;
    u32 *stagePack;
    MTexRes *stageNames;
    MTexRes *songNames;
} MsView;

void PortMapSel_Begin(s32 allow);
void PortMapSel_Frame(const MsView *view, s32 mapPhase);
void PortMapSel_Draw(void);
void PortMapSel_End(void);
s32 PortMapSel_Shown(s32 mapPhase);
void PortMapSel_MuteOn(void);
#endif

#endif
