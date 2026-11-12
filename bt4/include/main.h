#ifndef MAIN_H
#define MAIN_H

#include "types.h"

/* One loadable code overlay (gOverlayTbl; only entry 0 exists). */
typedef struct OverlayEntry {
    /* 0x00 */ char *label; /* "PROGRESS\n" */
    /* 0x04 */ char *path;  /* "cdrom0:\BIN\DBZP.BIN;1" */
} OverlayEntry;

/* Part of the view/camera object that View_ApplyScissor reads (the rest is not known here). */
typedef struct ViewScissor {
    /* 0x000 */ u8 unk0[0x200]; /* matrices at 0x140 and 0x180 among others */
    /* 0x200 */ s32 scissorX0;
    /* 0x204 */ s32 scissorX1;
    /* 0x208 */ s32 scissorY0;
    /* 0x20C */ s32 scissorY1;
} ViewScissor;

s32 main(void);
s32 Overlay_Load(s32 idx);
void Sys_RebootIop(void);
void Sys_LoadIopModules(void);
void Game_Main(void);
void View_ApplyScissor(ViewScissor *view);

#endif
