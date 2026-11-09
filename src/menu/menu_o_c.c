#include "common.h"
#include "menu/menu_o.h"

/*
 * Ub_Main, 0x379908..0x379F58: the handler of gProgress->mode 13..30 (main-menu item 1; archive gMenuArc3 = file
 * baseFile + 3) and the three functions that finish the battle setup once the player's fighters are chosen.
 * Read-only data: the jump table of the mode switch, 0x3B83B0..0x3B83F8.
 */

/* The screens of the group that live in other chunks. */
extern s32 SoloSel_Run(s32 section);    /* menu_m: one fighter */
extern s32 UbTeamSel_Run(s32 section);  /* menu_n: a team */
extern s32 UbzSel_Run(s32 section);     /* menu_n: mode 28 */
extern s32 UbRank_Run(s32 section);  /* menu_n: mode 26 */
extern s32 MisSel_Run(s32 section);     /* menu_p: mode 14 */
extern s32 MisResult_Run(s32 section);  /* menu_p: mode 16 */
extern s32 UbMenu_Run(s32 section);     /* menu_p: mode 13 */
extern s32 SimDay_Run(s32 section);  /* mode 22 */
extern s32 SimResult_Run(s32 section);  /* mode 23 */
extern s32 SimTop_Run(s32 section);  /* mode 20 */
extern s32 SurvSel_Run(s32 section);  /* mode 17 */
extern s32 SurvResult_Run(s32 section);  /* mode 19 */

/*
 * Mode 15 -> battle: side 0 is the pad with the team chosen in SoloSel / UbTeamSel (gProgress->team, teamSize
 * members, CPU level 0, full health). The rule and side 1 were written by MisSel_SetupBattle.
 */
void Ub_SetupTeam(void) {
    s32 i;
    s32 count = UO_PROG->teamSize;

    BattleSetup_SetSide(0, 0, 0, count, 1, 1, 0, NULL);
    for (i = 0; i < count; i++) {
        BattleSetup_SetMember(0, i, UO_PROG->team[i].chara, UO_PROG->team[i].color, 0, 0, 100.0f,
                              UO_PROG->team[i].items);
    }
    BattleSetup_Finish();
}

/* Mode 18 -> battle: member 0 of side 0 is the fighter chosen in SoloSel; everything else was set up by mode 17. */
void Ub_SetupSolo(void) {
    BattleSetup_SetMember(0, 0, UO_PROG->team[0].chara, UO_PROG->team[0].color, 0, 0, 100.0f, UO_PROG->team[0].items);
    BattleSetup_Finish();
}

/* Mode 29 -> battle: the same for the course chosen in mode 28. */
void Ub_SetupSolo2(void) {
    BattleSetup_SetMember(0, 0, UO_PROG->team[0].chara, UO_PROG->team[0].color, 0, 0, 100.0f, UO_PROG->team[0].items);
    BattleSetup_Finish();
}

/*
 * Runs the screens of modes 13..30 until one of them starts a battle (returns 1) or the player goes back to the
 * main menu (mode 4, returns 0).
 */
s32 Ub_Main(void) {
    s32 done = 0;
    s32 result = 1;
    s32 r;

    do {
        if (gMenuArc3 == NULL) {
            gMenuArc3 = File_LoadSync(UO_PROG->baseFile + 3, NULL, 0);
        }
        switch (UO_PROG->mode) {
        case 13:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x840));
            r = UbMenu_Run(1);
            if (r) {
                switch (r) {
                case 1:
                    UO_PROG->mode = 20;
                    break;
                case 2:
                    UO_PROG->mode = 14;
                    break;
                case 3:
                    UO_PROG->mode = 17;
                    break;
                case 4:
                    UO_PROG->mode = 24;
                    break;
                }
            } else {
                UO_PROG->mode = 4;
                result = 0;
                done = 1;
            }
            Adx_StopAll();
            break;
        case 14:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x84A));
            if (MisSel_Run(2)) {
                UO_PROG->mode = 15;
            } else {
                UO_PROG->mode = 13;
            }
            Adx_StopAll();
            break;
        case 15:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x83F));
            if (UO_PROG->teamSize >= 2) {
                r = UbTeamSel_Run(4);
            } else {
                r = SoloSel_Run(4);
            }
            if (r) {
                Ub_SetupTeam();
                done = 1;
                UO_PROG->mode = 16;
            } else {
                UO_PROG->mode = 14;
            }
            Adx_StopAll();
            break;
        case 16:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x841));
            MisResult_Run(3);
            UO_PROG->mode = 14;
            Adx_StopAll();
            break;
        case 17:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x84A));
            if (SurvSel_Run(2)) {
                UO_PROG->mode = 18;
            } else {
                UO_PROG->mode = 13;
            }
            Adx_StopAll();
            break;
        case 18:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x83F));
            if (SoloSel_Run(4)) {
                Ub_SetupSolo();
                done = 1;
                UO_PROG->mode = 19;
            } else {
                UO_PROG->mode = 17;
            }
            Adx_StopAll();
            break;
        case 19:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x841));
            SurvResult_Run(3);
            UO_PROG->mode = 17;
            Adx_StopAll();
            break;
        case 20:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x842));
            if (SimTop_Run(5)) {
                UO_PROG->mode = 21;
            } else {
                UO_PROG->mode = 13;
            }
            Adx_StopAll();
            break;
        case 21:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x83F));
            if (SoloSel_Run(4)) {
                UO_PROG->mode = 22;
            } else {
                UO_PROG->mode = 20;
            }
            Adx_StopAll();
            break;
        case 22:
            if (SimDay_Run(0)) {
                UO_PROG->mode = 23;
                done = 1;
            } else {
                UO_PROG->mode = 13;
            }
            Adx_StopAll();
            break;
        case 23:
            if (SimResult_Run(3)) {
                UO_PROG->mode = 20;
            } else {
                UO_PROG->mode = 22;
                UO_PROG->unk674++;
            }
            Adx_StopAll();
            break;
        case 24:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x84A));
            if (DiscFusion_Run(6)) {
                if (UO_PROG->cursor == 0) {
                    UO_PROG->teamSize = 1;
                    UO_PROG->dpRule = 0;
                    UO_PROG->mode = 25;
                } else {
                    UO_PROG->mode = 28;
                }
            } else {
                UO_PROG->mode = 13;
            }
            Adx_StopAll();
            break;
        case 25:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x8DC));
            if (SoloSel_Run(4)) {
                UO_PROG->mode = 26;
            } else {
                UO_PROG->mode = 24;
            }
            Adx_StopAll();
            break;
        case 26:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x8EE));
            if (UbRank_Run(0)) {
                UO_PROG->mode = 27;
                done = 1;
            } else {
                UO_PROG->mode = 24;
            }
            Adx_StopAll();
            break;
        case 27:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x83E));
            UbResult_Run(3);
            UO_PROG->mode = 26;
            Adx_StopAll();
            break;
        case 28:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x84E));
            if (UbzSel_Run(0)) {
                UO_PROG->mode = 29;
            } else {
                UO_PROG->mode = 24;
            }
            Adx_StopAll();
            break;
        case 29:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x832));
            if (SoloSel_Run(4)) {
                Ub_SetupSolo2();
                done = 1;
                UO_PROG->mode = 30;
            } else {
                UO_PROG->mode = 28;
            }
            Adx_StopAll();
            break;
        case 30:
            Bgm_Play(DISC_RESOURCE(UB_BGM, 0x84F));
            UbResult_Run(3);
            UO_PROG->mode = 28;
            Adx_StopAll();
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc3 != NULL) {
        Heap_Free(gMenuArc3);
        gMenuArc3 = NULL;
    }
    return result;
}
