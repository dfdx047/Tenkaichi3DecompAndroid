/* Dragon Rage's platform layer (gs_input.c, snd_adx.c, state.c, headless.c) asks BT3's Tag Team mod a few things; this BT4
   build has not got it: the answers of a game without them (no Tag Team, no network room). */
int TagNet_Enabled(void) { return 0; }
int TagNet_Active(void) { return 0; }
int TagNet_Role(void) { return -1; }
int TagTeam_Active(void) { return 0; }
void TagTeam_Enable(int enabled) { (void)enabled; }
int TagTeam_ViewActor(int view) { return view; }
/* (PortCharSel_Abandon: src/menu/port_charsel.c, the character select of the new screen, is in BT4 too) */
/* The Android touch menu (gs/ui_android.inc) names the matchmaking service of a lobby: Dragon Net's (BT3 only). */
const char *Port_LobbyServer(void) { return ""; }
/* A clock in microseconds for the character select's log (src/battle/view_a_d.c: a fighter read in so many ms; BT3's is
   in tagteam_trace.c, which this build has not got). */
extern unsigned long long SDL_GetTicksNS(void);
unsigned Port_LoadTimeUs(void) { return (unsigned)(SDL_GetTicksNS() / 1000); }
/* The stage behind BT4's new character select (src/battle/view_a_d.c): Supreme Kai's World (7), as the user asked, or
   BT4_SELECT_STAGE=<id> (testing). */
#include <stdlib.h>
#include <stdio.h>
static int sSelectChosen = -1; /* the stage chosen before the fighters (Port_SelectStageSet), -1 none */
int Port_SelectStage(void) {
    const char *e = getenv("BT4_SELECT_STAGE");
    if (sSelectChosen >= 0) {
        return sSelectChosen;
    }
    return e != NULL && e[0] != '\0' ? atoi(e) : 7;
}
/* The select's stage-first order (src/menu/menu_e_b.c, menu_c_e.c): the map chosen first is the one behind the
   fighters; -1 back to the usual one. */
void Port_SelectStageSet(int stage) {
    sSelectChosen = stage;
}

/* Whose motions every fighter of BT4's new select plays (its entrance, then standing still): Goku Black's base form,
   as the user asked. BT4_SELECT_ENTRANCE=<character> (testing) another; -1 each its own. */
int Port_SelectEntrance(void) {
    static int v = -2;
    if (v == -2) {
        const char *e = getenv("BT4_SELECT_ENTRANCE");
        v = e != NULL && e[0] != '\0' ? atoi(e) : 99; /* 99: Goku Black (34 is his Super Saiyan Rose) */
    }
    return v;
}

/* How much more BT4's new select turns every fighter (thousandths of a radian), so the shared entrance ends facing the
   camera. BT4_SELECT_TURN (testing) another. */
int Port_SelectTurnMilli(void) {
    static int v = -100000;
    if (v == -100000) {
        const char *e = getenv("BT4_SELECT_TURN");
        v = e != NULL && e[0] != '\0' ? atoi(e) : 420; /* (turned more to the front, 2026-11-28; 750 before) */
    }
    return v;
}

/* BT4_SELECT_DEMO=<frames>: testing the character select's look (src/menu/port_charsel.c): the fighters of the roster
   shown one after another, so many frames each. Only what one machine draws. */
int Port_SelectDemo(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("BT4_SELECT_DEMO");
        v = e != NULL && e[0] != '\0' ? atoi(e) : 0;
    }
    return v;
}

/* BT4's new select, the one fighter's camera: 0 its distance, 1 the height it looks at, in thousandths of the
   character table's (ChrViewInfo dist, targetY). BT4_SELECT_CAMD / BT4_SELECT_CAMY (testing) others. */
int Port_SelectCam(int which) {
    static int v[2] = { -100000, -100000 };
    if (which < 0 || which > 1) {
        return 1000;
    }
    if (v[which] == -100000) {
        const char *e = getenv(which == 0 ? "BT4_SELECT_CAMD" : "BT4_SELECT_CAMY");
        v[which] = e != NULL && e[0] != '\0' ? atoi(e) : which == 0 ? 1000 : 1000; /* (multipliers: the framing itself is in view_a_d.c) */
    }
    return v[which];
}

/* BT4's new select, one on one: the arms brought a little closer to the body (src/battle/bobj_a.c BtlObjAnim_PortSpreadApply). 0 the left
   upper arm's node, 1 the right's, 2 the axis it turns round (0 x, 1 y, 2 z), 3 the order (1: in the parent's axes),
   4 how much (thousandths of a radian; 0 off). BT4_SELECT_ARMS="l r axis order milli" (testing). */
int Port_SelectArms(int which) {
    static int v[5] = { 19, 33, 2, 1, -250 }; /* (the upper arms, round the forward axis: a little closer in, as the user asked) */
    static int read;
    if (!read) {
        const char *e = getenv("BT4_SELECT_ARMS");
        read = 1;
        if (e != NULL && e[0] != '\0') {
            sscanf(e, "%d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4]);
        }
    }
    return which >= 0 && which < 5 ? v[which] : 0;
}

/* Fat Buu's arms on BT4's select (src/battle/view_a_d.c): opened so much (thousandths of a radian, the same turn as
   Port_SelectArms; the others are closed by 250). Testing: BT4_SELECT_ARMS_BUU. */
int Port_SelectArmsBuu(void) {
    static int v = 300;
    static int read;
    if (!read) {
        const char *e = getenv("BT4_SELECT_ARMS_BUU");
        read = 1;
        if (e != NULL && e[0] != '\0') {
            v = atoi(e);
        }
    }
    return v;
}

/* Testing: the select's numbers in the log (BT4_SELECT_DEBUG=1). */
int Port_SelectDebug(void) {
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("BT4_SELECT_DEBUG");
        v = e != NULL && e[0] != '\0' ? atoi(e) : 0;
    }
    return v;
}
