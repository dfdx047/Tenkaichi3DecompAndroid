#ifndef MENU_PORT_SELDIAG_H
#define MENU_PORT_SELDIAG_H

#include <stdio.h>
#include <stdlib.h>
#include "menu/port_selcanon.h"

/* Diagnostics of the character select, for finding where two machines of an online match part. OFF unless BT3_SELECT_DIAG=1 (or the
   setting select_diag): it only prints, it reads game state and writes none, and it is the only thing in the select that looks at
   which screen is up (printed, never used). Compare the lines of the two machines (adb logcat / log files):
     flow   the order decided (PortSelFlow_StageFirst), whether it was online, which screen
     leave  the select is left: the vblank, the canonical hash and fields. The two machines must print the same hash.
     battle the battle's introduction begins: the vblank. */
#ifndef PORT_HOST /* (as src/menu/port_charsel.c has it: the port's own variables are in a section no saved state holds) */
#ifdef __APPLE__
#define PORT_HOST __attribute__((section("__DATA,__porthst")))
#else
#define PORT_HOST __attribute__((section(".porthst")))
#endif
#endif
extern unsigned gPortVBlanks; /* plat_stub.c */
extern int Port_Setting(const char *name, int def);

static inline int PortSelDiag_On(void) {
    PORT_HOST static int on = -1; /* (not the game's state: it is only read) */

    if (on < 0) {
        const char *e = getenv("BT3_SELECT_DIAG");

        on = e != NULL ? atoi(e) != 0 : Port_Setting("select_diag", 0) != 0;
    }
    return on;
}

static inline void PortSelDiag_Flow(const char *which, int online, int stageFirst, int newScreen) {
    if (PortSelDiag_On()) {
        fprintf(stderr, "bt3: select-diag: flow %s online=%d stageFirst=%d screen=%s vblank=%u\n", which, online, stageFirst,
                newScreen ? "new" : "classic", gPortVBlanks);
    }
}

static inline void PortSelDiag_Leave(const char *which, const PortSelCanon *c) {
    if (PortSelDiag_On()) {
        fprintf(stderr, "bt3: select-diag: leave %s vblank=%u canon=%016llx players=%d stage=%d bgm=%d side0=%d,%d,%d,%d side1=%d,%d,%d,%d\n",
                which, gPortVBlanks, (unsigned long long)PortSelCanon_Hash(c), (int)c->players, (int)c->stage, (int)c->bgm,
                (int)c->words[0][0], (int)c->words[0][1], (int)c->words[0][2], (int)c->words[0][3],
                (int)c->words[1][0], (int)c->words[1][1], (int)c->words[1][2], (int)c->words[1][3]);
    }
}

static inline void PortSelDiag_Battle(void) {
    if (PortSelDiag_On()) {
        fprintf(stderr, "bt3: select-diag: battle introduction begins vblank=%u\n", gPortVBlanks);
    }
}

#endif
