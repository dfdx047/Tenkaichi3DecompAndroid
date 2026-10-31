#include "common.h"
#include "menu/menu_a.h"

/* The menu archives, one per group of modes (overlay .data, 0x3B0E84..0x3B0EB8); see menu_a.h. */
void *gMenuArc0 = NULL;
void *gMenuArc1 = NULL;
void *gMenuArc2 = NULL;
void *gMenuArc3 = NULL;
void *gMenuArc4 = NULL;
void *gMenuArc5 = NULL;
void *gMenuArc6 = NULL;
void *gMenuArc7 = NULL;
void *gMenuArc8 = NULL;
void *gMenuArc9 = NULL;
void *gMenuArc10 = NULL;
void *gMenuArc11 = NULL;
void *gMenuArc12 = NULL; /* file baseFile + 0x18: loaded and freed by the wish screen (src/sys/late_a.c) */

/*
 * Progress_Main, 0x336A90..0x336FC0: the entry point of the menu overlay and its mode dispatcher. The main
 * executable's Game_Main calls it (by address, 0x336A90) right after Overlay_Load(0), and calls Battle_Main when
 * it returns. Its only read-only data is the 70-entry jump table at 0x3B1130.
 *
 * gProgress->mode -> handler (anything not listed does nothing, so the loop spins on it):
 *    1          Title_Run(1)        0x337D70  title screen
 *    2          Movie_PlayOpening   main      opening movie, then mode 1
 *    4          MainMenu_Run(2)     0x336838  main menu
 *    6..10      Hist_Main
 *   13..30      Ub_Main
 *   33..35      Tour_Main
 *   38..41      Duel_Main
 *   44..45      TrainMode_Main
 *   48..50      EvoMode_Main       (result ignored)
 *   53..56      Dc_Main
 *   60          CharRefMode_Main       (result ignored)
 *   62          OptMode_Main       (result ignored)
 *   70          Shen_Main           main 0x2BD230 (result ignored)
 * A handler that returns non-zero ends the loop: the overlay returns and the battle starts.
 */

extern void FirstRun_Main(void); /* first run: loads the third archive partition */
extern s32 Hist_Main(void);
extern s32 Ub_Main(void);
extern s32 Tour_Main(void);
extern s32 Duel_Main(void);
extern s32 TrainMode_Main(void);
extern void EvoMode_Main(void);
extern s32 Dc_Main(void);
extern void CharRefMode_Main(void);
extern void OptMode_Main(void);

#define MENU_BGM_TITLE 0x10B16

#define MENU_ARC_FREE(p) \
    if ((p) != NULL) { \
        Heap_Free(p); \
        (p) = NULL; \
    }

/* Runs the menus until one of them starts a battle. Chooses the first screen from how the last battle ended. */
s32 Progress_Main(s32 arg) {
    s32 done = 0;
    s32 flags;
    s32 reason;
    s32 ret;

    Pad_SetRepeat(0x28, 3);
    if (gProgress->flags & MPROG_FIRST_RUN) {
        FirstRun_Main();
        gProgress->flags &= ~MPROG_FIRST_RUN;
    }
    flags = BattleResult_GetFlags();
    reason = BattleResult_GetReason();
    if (flags & 8) {
        if (reason & 8) {
            gProgress->flags &= ~MPROG_FLAG10;
            gProgress->flags &= ~MPROG_FLAG20;
            gProgress->mode = 4;
        } else if (reason & 0x10) {
            if (gProgress->mode >= 38 && gProgress->mode <= 41) {
                if (gProgress->unk624 != 0) {
                    gProgress->mode = 40;
                } else {
                    gProgress->mode = 39;
                }
            } else if (gProgress->mode >= 44 && gProgress->mode <= 45) {
                gProgress->mode = 45;
            }
        } else if (reason & 0x80) {
            gProgress->mode = 38;
        } else if (reason & 0x800) {
            gProgress->mode = 6;
        } else if (reason & 0x1000) {
            gProgress->mode = 56;
            gProgress->unk68C |= 1;
        }
#ifdef PORT
        {   /* Tenkaichi Budokai Online's sparring fight (Train While Waiting): its "Back to Budokai Online" goes back
               to the search (menu/port_tbo.c) */
            extern s32 gPortTboSpar;

            if (gPortTboSpar && (reason & 0x10)) {
                gPortScreen = PORT_SCREEN_TBO;
                gProgress->mode = MPROG_MODE_PORT_SCREEN;
            }
            gPortTboSpar = 0;
        }
        {   /* A lobby's match (port/src/gs/net_lobby.c): "Return to Vs Menu", the owner's way back to the lobby window,
               leaves for the main menu, which ends the session on every copy (there is no versus menu in it) */
            extern int Port_NetLobbyOwnerSide(void);

            if ((reason & 0x80) && Port_NetLobbyOwnerSide() >= 0) {
                gProgress->flags &= ~MPROG_FLAG10;
                gProgress->flags &= ~MPROG_FLAG20;
                gProgress->mode = 4;
            }
        }
#endif
    }
    if (gProgress->mode == 1 || gProgress->mode == 4) {
        if (gMenuArc1 == NULL) {
            gMenuArc1 = File_LoadSync(gProgress->baseFile + 1, NULL, 0);
        }
    }
    Snd_LoadBankFile(2, 0x14C);

    do {
        switch (gProgress->mode) {
        case 2:
            PadWatch_SetEnabled(0);
            Movie_PlayOpening();
            PadWatch_SetEnabled(1);
            gProgress->mode = 1;
            break;
        case 1:
            Bgm_Play(MENU_BGM_TITLE);
            ret = Title_Run(1);
            Adx_StopAll();
            switch (ret) {
            case 1:
                done = 1;
                break;
            default:
                gProgress->mode = 4;
                break;
            case 2:
                gProgress->mode = ret;
                MENU_ARC_FREE(gMenuArc1);
                break;
            }
            break;
        case 4:
            Bgm_Play(MENU_BGM_TITLE);
            if (MainMenu_Run(2)) {
                Adx_StopAll();
                MENU_ARC_FREE(gMenuArc1);
            } else {
                Adx_StopAll();
                gProgress->mode = 1;
            }
            break;
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
            if (Hist_Main()) {
                done = 1;
            }
            break;
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
            if (Ub_Main()) {
                done = 1;
            }
            break;
        case 33:
        case 34:
        case 35:
            if (Tour_Main()) {
                done = 1;
            }
            break;
        case 38:
        case 39:
        case 40:
        case 41:
            if (Duel_Main()) {
                done = 1;
            }
            break;
        case 44:
        case 45:
            if (TrainMode_Main()) {
                done = 1;
            }
            break;
        case 48:
        case 49:
        case 50:
            EvoMode_Main();
            break;
        case 53:
        case 54:
        case 55:
        case 56:
            if (Dc_Main()) {
                done = 1;
            }
            break;
        case 60:
            CharRefMode_Main();
            break;
        case 62:
            OptMode_Main();
            break;
        case 70:
            Shen_Main();
            break;
#ifdef PORT
        case MPROG_MODE_PORT_SCREEN: /* the port's own screens (menu/port_screen.c) */
            if (PortScreen_Main()) {
                done = 1;
            }
            break;
#endif
        }
    } while (!done);

    Pad_SetRepeat(0x14, 1);
    Snd_UnloadBank(2);
    MENU_ARC_FREE(gMenuArc0);
    MENU_ARC_FREE(gMenuArc1);
    MENU_ARC_FREE(gMenuArc2);
    MENU_ARC_FREE(gMenuArc3);
    MENU_ARC_FREE(gMenuArc4);
    MENU_ARC_FREE(gMenuArc5);
    MENU_ARC_FREE(gMenuArc6);
    MENU_ARC_FREE(gMenuArc7);
    MENU_ARC_FREE(gMenuArc8);
    MENU_ARC_FREE(gMenuArc9);
    MENU_ARC_FREE(gMenuArc10);
    MENU_ARC_FREE(gMenuArc11);
    return 0;
}
