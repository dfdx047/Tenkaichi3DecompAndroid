#include "common.h"
#include "menu/menu_g.h"
#ifdef PORT
extern int Port_NetSession(void); /* port/src/gs/net.c */
extern int Port_NetLobbyOwnerSide(void); /* port/src/gs/net.c: in a lobby's match, the owner's side; -1 otherwise */
#endif

/*
 * Duel_Main, 0x352CB8..0x352EC0: the handler of progress modes 38..41 (main-menu item 3, the duel mode).
 * Mode 38 is the duel menu, 39 and 40 are the two character selects that follow it (CharSel_Run and
 * TeamSel_Run, in earlier chunks; they write the battle setup), 41 is an empty screen. Returns non-zero
 * when the overlay has to return to the main executable (a battle was set up), zero to go back to the main
 * menu.
 */

extern s32 DuelMenu_Run(s32 section); /* 0x356090 (next chunk): the duel menu's frame loop */
extern s32 CharSel_Run(s32 section);  /* 0x348710: one-on-one character select; writes the battle setup */
extern s32 TeamSel_Run(s32 section);  /* 0x351508: team character select (team and DP battles); writes the battle setup */

s32 Duel_Mode41(s32 arg);

/* Runs the duel mode's screens until one of them leaves the mode. */
s32 Duel_Main(void) {
    s32 bgm = 0;
    s32 done = 0;
    s32 result = 1;

    if (gMenuArc5 == NULL) {
        gMenuArc5 = File_LoadSync(gProgress->baseFile, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case 38:
            if (!bgm) {
                bgm = 1;
                Bgm_Play(DUEL_BGM);
            }
            if (DuelMenu_Run(1)) {
                Adx_StopAll();
                gProgress->mode = DUEL_PROG->battleType ? 40 : 39;
            } else {
                Adx_StopAll();
                bgm = 0;
                gProgress->mode = 4;
                result = 0;
                done = 1;
            }
            break;
        case 39:
            if (CharSel_Run(2)) {
                Adx_StopAll();
                bgm = 0;
                done = 1;
            } else {
                Adx_StopAll();
                bgm = 0;
                gProgress->mode = 38;
#ifdef PORT /* PC build, online session: there is no versus menu in it; going back leaves for the main menu, which
               ends the session (as cancelling the versus menu does). In a lobby's match only its owner can go back
               (menu_c_e.c, CharSel_PortLobbyHold): everyone is back in the lobby (port/src/gs/net_lobby.c). */
                if (Port_NetSession()) {
                    gProgress->mode = 4;
                    result = 0;
                    done = 1;
                }
#endif
            }
            break;
        case 40:
            if (TeamSel_Run(2)) {
                Adx_StopAll();
                bgm = 0;
                done = 1;
            } else {
                Adx_StopAll();
                bgm = 0;
                gProgress->mode = 38;
#ifdef PORT /* PC build, online session: there is no versus menu in it; going back leaves for the main menu, which
               ends the session (as cancelling the versus menu does). In a lobby's match only its owner ends the
               session (port/src/gs/net_lobby.c), as from the character select; the team select goes back on the
               first pad alone, so another's going back opens it again. */
                if (Port_NetSession() && Port_NetLobbyOwnerSide() > 0) {
                    gProgress->mode = 40;
                } else if (Port_NetSession()) {
                    gProgress->mode = 4;
                    result = 0;
                    done = 1;
                }
#endif
            }
            break;
        case 41:
            if (Duel_Mode41(0)) {
                Adx_StopAll();
                bgm = 0;
                done = 1;
                gProgress->mode = 39;
            } else {
                Adx_StopAll();
                bgm = 0;
                gProgress->mode = 39;
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc5 != NULL) {
        Heap_Free(gMenuArc5);
        gMenuArc5 = NULL;
    }
    return result;
}

/* The screen of mode 41: nothing; reports "done". */
s32 Duel_Mode41(s32 arg) {
    return 1;
}
