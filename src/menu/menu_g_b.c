#include "common.h"
#include "menu/menu_g.h"
#ifdef PORT
#include "battle/tag_session.h"
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
#ifdef PORT
extern s32 gPortDuelRole;
extern s32 PortModMgr_Run(void);
extern s32 PortZw_Run(void);
extern s32 Port_ZwAllowed(void); /* port/src/plat_fprint.c: the original disc only */
#endif
s32 Duel_Main(void) {
    s32 bgm = 0;
    s32 done = 0;
    s32 result = 1;

    if (gMenuArc5 == NULL) {
        gMenuArc5 = File_LoadSync(gProgress->baseFile, NULL, 0);
    }
#ifdef PORT
    if (gPortDuelRole == 3 && !Port_ZwAllowed()) { /* a modified disc image: back to the main menu */
        gPortDuelRole = 0;
        gProgress->mode = 4;
        result = 0;
        done = 1;
    }
    if (gPortDuelRole == 3) { /* opened from the main menu's Z Warriors item: its screen, then back; it can leave for the
                                 character select (back to it after) or a test fight (its pause menu comes back) */
        s32 to;

        while (1) {
            Bgm_Play(0x10B16 + 2);
            to = PortZw_Run();
            Adx_StopAll();
            if (to != 2) {
                break;
            }
            CharSel_Run(2); /* (the real character select: the CUSTOM slot shows the warriors) */
            Adx_StopAll();
        }
        gPortDuelRole = 0;
        if (to == 1) { /* the test fight is set up: the Training runs it */
            gProgress->mode = 45;
            result = 1;
        } else {
            gProgress->mode = 4;
            result = 0;
        }
        done = 1;
    }
    if (gPortDuelRole == 2) { /* opened from the main menu's Mod Manager item: its screen, then back */
        Bgm_Play(0x10B16 + 2); /* "Innocent World" (the third piece of the music list) */
        if (PortModMgr_Run()) { /* left for a test fight: a training battle is set up; from it the Training is next */
            Adx_StopAll();
            gPortDuelRole = 0;
            gProgress->mode = 45;
            result = 1;
        } else {
            Adx_StopAll();
            gPortDuelRole = 0;
            gProgress->mode = 4;
            result = 0;
        }
        done = 1;
    }
#endif
#ifdef PORT
    if (!done) { /* Z Warriors online: its session goes straight to the fight, and out of it once fought */
        extern int Port_NetSession(void);
        extern int Port_ZwFightSetup(int *out); /* port/src/plat_zwnet.c */
        static int zw[6];
        int kind = Port_NetSession() ? Port_ZwFightSetup(zw) : 0;

        if (kind == 1) {
            static u8 noItems[0x10];
            extern void Battle_ClearWork(void);
            extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
            extern void BattleSetup_SetSide(s32 sideNo, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, void *a7);
            extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health, void *items);
            extern void BattleSetup_Finish(void);

            Battle_ClearWork();
            BattleSetup_SetRule(1, 0, 8, zw[5] == 0 ? 3 : zw[5] == 1 ? 4 : 0, 0, zw[4], 1);
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 0, 1, 1, 1, 1, 0, NULL);
            BattleSetup_SetMember(0, 0, zw[0], zw[1], 0, 0, 100.0f, noItems);
            BattleSetup_SetMember(1, 0, zw[2], zw[3], 0, 0, 100.0f, noItems);
            BattleSetup_Finish();
            { /* (as the character select leaves for a battle) */
                extern void Adx_StopAll(void);
                extern void Dma_ResetBuffers(void);

                Adx_StopAll();
                sceGsSyncPath(0, 0);
                Dma_ResetBuffers();
            }
            gProgress->mode = 39;
            result = 1;
            done = 1;
        } else if (kind < 0) {
            gProgress->mode = 4;
            result = 0;
            done = 1;
        }
    }
    while (!done) {
#else
    do {
#endif
        switch (gProgress->mode) {
        case 38:
            if (!bgm) {
                bgm = 1;
                Bgm_Play(DISC_RESOURCE(DUEL_BGM, 0x8AF));
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
#ifdef PORT
                { extern s32 TagTournament_Start(void);
                  s32 next=TagTournament_Start();
                  if(next==2)continue;
                  if(next) result=0; else TagSession_Apply(); }
#endif
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
                if (TagSession_Selected()) {
                    TagSession_Open();
                    result=0;
                    done=1;
                }
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
#ifdef PORT
    }
#else
    } while (!done);
#endif
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
