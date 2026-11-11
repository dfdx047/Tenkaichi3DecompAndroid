/*
 * Headless harness: stands in for the menu overlay. The game's own main loop is
 *     for (;;) { Overlay_Load(0); Progress_Main(0); Battle_Main(0); }
 * and Progress_Main is the menu's entry point: it returns when a battle has been set up. Here it sets one up
 * without any menu:
 *     BT3_REPLAY=<file>   a replay save file (0x1AC00 bytes: 0x38 header + the replay block)
 *     otherwise           the game's own attract-demo battle (CPU against CPU, one of nine fixed pairings)
 * and the second time it is called (the battle is over) it ends the program.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_host.h"
#include "plat_stages.h"
#include "plat_songs.h"
#include "port_addr.h"

extern void Battle_ClearWork(void);
extern int BattleReplay_Load(void *buf, int size);
extern void Demo_SetupBattle(void);

#define REPLAY_FILE_SIZE 0x1AC00
#define REPLAY_HEADER 0x38
#define REPLAY_BLOCK 0x1ABA8

static int sBattles;
static int sFromMenu;

#if (defined(__x86_64__) || defined(__aarch64__)) && defined(__clang__) /* (the 64-bit builds: game pointers are 4 bytes) */
#undef __ptr32 /* the Windows headers of mingw define these two away */
#undef __uptr
#define GAME_PTR *__ptr32 __uptr
#else
#define GAME_PTR *
#endif

/* Linked with --wrap=Progress_Main: the game's call in Game_Main comes here. BT3_REPLAY or BT3_DEMO: the battle is
   set up without menus (below); otherwise the real menus run (the overlay's Progress_Main, src/menu/menu_a_b.c). */
extern int __real_Progress_Main(int arg);
int gPortMenuMode;
/* A menu screen that is a 3D scene (the character reference's model viewer) asks, every frame, to be shown as a
   fight is while it runs: the wide projection, 2D kept in proportion. Only between the two lines of Progress_Main
   below that set and clear the menu mode. */
static int sMenuScene3D;
void Port_MenuScene3D(int on) {
    on = on != 0;
    if (on != sMenuScene3D) {
        sMenuScene3D = on;
        gPortMenuMode = !on;
    }
}

volatile int gPortUnlockAll, gPortUnlockDone;
extern struct SaveData GAME_PTR gSaveData;
extern void Save_UnlockAll(void *opt);
extern int Port_NetSession(void); /* gs/net.c */
extern int Port_NetWarp(void);
extern void Port_NetArrived(void);
extern struct { int unk0[2]; int loadPack, loadRes, loadSprites; int flags; int mode,prevMode,reserved[5],subMenu,subMenuItem; } GAME_PTR gProgress; /* include/menu/menu_a.h */
static int sNetEntered;
/* The vertical blank at which the session's start-up was over (0: not yet). Part of the game's state like the rest
   of this file's variables, so it goes back with a rollback: it is always what the blanks as they were last run
   say, which gs/net.c's own note of the same moment (sArrived, for the picture) is not. */
static unsigned sNetArrivedAt;
/* Extra stage ids (BT3_STAGE_REPLACE="0x1b=0x24,0x23=0x25"): the menus swap the id in a stage-list slot for a
   map added from outside the disc. Parsed here, in a port file where pointers are the host's; the game code reads
   the result from these globals. (A getenv result cannot be used inside the game code: its 64-bit pointer would
   be truncated to the game's 32-bit ones.) The grid is a fixed 6x6, so the list cannot grow: a slot is reused. */
int gPortStageReplaceCount;
int gPortReplaceOld[16];
int gPortReplaceNew[16];

static void port_stage_replace_init(void) {
    static int done;
    const char *p;
    if (done) {
        return;
    }
    done = 1;
    p = getenv("BT3_STAGE_REPLACE");
    while (p != NULL && *p != '\0' && gPortStageReplaceCount < 16) {
        char *end;
        long old = strtol(p, &end, 0);
        long nw;
        if (end == p) {
            break;
        }
        p = end;
        while (*p == ' ') {
            p++;
        }
        if (*p == '=') {
            p++;
        }
        while (*p == ' ') {
            p++;
        }
        nw = strtol(p, &end, 0);
        if (end == p) {
            break;
        }
        p = end;
        gPortReplaceOld[gPortStageReplaceCount] = (int)old;
        gPortReplaceNew[gPortStageReplaceCount] = (int)nw;
        gPortStageReplaceCount++;
        while (*p == ',' || *p == ' ') {
            p++;
        }
    }
}

extern void Port_NetRules(int *type, int *dp, int *time); /* gs/net.c */

int __wrap_Progress_Main(int arg) {
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_VOICE_SELFTEST")){extern int PortVoice_TestRun(void);exit(PortVoice_TestRun());}
    const char *path;
    PortStages_Init();
    PortSongs_Init();
    {
        extern void Port_VerifyStart(void); /* plat_verify.c: is the data the original disc's (in the background) */
        Port_VerifyStart();
    }
    port_stage_replace_init();
    path = getenv("BT3_REPLAY");

    if (path == NULL && getenv("BT3_DEMO") == NULL) {
        {
            int r;
            /* Local regression hook: open a real menu through the normal boot path. */
            if (!Port_NetSession() && gProgress != NULL && gSaveData != NULL && getenv("BT4_TEST_MODE") != NULL) {
                static int testEntered;
                if (!testEntered) {
                    gProgress->mode = atoi(getenv("BT4_TEST_MODE"));
                    if(getenv("BT4_RENDER_TEST") && getenv("BT4_TEST_SUBMENU")) {
                        Save_UnlockAll(gSaveData);
                        gProgress->subMenu=atoi(getenv("BT4_TEST_SUBMENU"));
                        gProgress->subMenuItem=0;
                    }
                    if (!getenv("BT4_TAG_VIDEO_TEST") && (getenv("BT4_RENDER_TBO") != NULL || getenv("BT4_TAG_DRAFT_TEST") != NULL || getenv("BT4_TAG_PREVIEW") != NULL)) {
                        extern int gPortScreen;
                        gProgress->mode=80;gPortScreen=1;
                    }
                    testEntered = 1;
                    fprintf(stderr, "bt4: test menu mode %d\n", gProgress->mode);
                }
            }
            /* An online session (gs/net.c) goes straight to the versus mode's character select, with the game's
               default save and everything in it unlocked: the same on both sides. (The overlay's first-run part, the
               logos and the memory card check, still runs first; without picture: Port_NetWarp.) */
            if (Port_NetSession() && !sNetEntered && gProgress != NULL && gSaveData != NULL) {
                sNetEntered = 1;
                /* Mode 39 is the versus mode's character select. The versus menu in front of it (mode 38) only
                   leaves three choices behind, in this same record (DuelProgress, include/menu/menu_g.h): who
                   plays (+0x620: 1 = 1P vs 2P), the battle type (+0x624: 0 = single) and the DP limit (+0x630).
                   They are set here, so the session opens on the character select itself. Going back from it
                   leads to the versus menu, where the host can change the battle type; back from there ends the
                   session. (After a battle the game itself returns to mode 39.) */
                /* Since 0.1.15 the host chooses the battle type, the DP limit and the time limit in the lobby window
                   (gs/net.c: Port_NetRules, the same on both sides). The versus menu is not shown at all: a team
                   or DP battle opens on the team select (mode 40), and going back from either select ends the
                   session (Duel_Main). The time limit is rule 0 of the battle settings in the save (+0xC34). */
                {
                    int type, dp, time;
                    Port_NetRules(&type, &dp, &time);
                    gProgress->mode = type != 0 ? 40 : 39;
                    *(int *)((char *)gProgress + 0x620) = 1;
                    *(int *)((char *)gProgress + 0x624) = type;
                    *(int *)((char *)gProgress + 0x630) = dp;
                    Save_UnlockAll(gSaveData);
                    *(int *)((char *)gSaveData + 0xC34) = time;
                    {
                        /* the match's seed, the host's draw (gs/net.c): the same on both copies, at the same point */
                        /* Both of the game's generators, the way Rand_Init (src/sys/rand.c) seeds them at start-up:
                           the C library's, then the twister from four of its numbers. "Random" at the character
                           select, the random stage and song draw from the twister (Rand_Range). */
                        extern unsigned Port_NetSeed(void);
                        extern void Port_Srand(unsigned seed);
                        extern int Port_Rand(void);
                        extern void Rand_SeedByArray(unsigned *key, int keyLen);
                        if (Port_NetSeed() != 0) {
                            unsigned key[4];
                            int k;
                            Port_Srand(Port_NetSeed());
                            for (k = 0; k < 4; k++) {
                                key[k] = (unsigned)Port_Rand();
                            }
                            Rand_SeedByArray(key, 4);
                        }
                    }
                    fprintf(stderr, "bt3: net: rules: %s%s, time limit %s\n", type == 0 ? "single battle" : type == 1 ? "team battle" : "DP battle",
                            type == 2 ? (dp == 0 ? " (10)" : dp == 1 ? " (15)" : " (20)") : "",
                            time == 0 ? "60" : time == 1 ? "90" : time == 2 ? "180" : time == 3 ? "240" : "none");
                }
            }
            {   /* BT3_MENU_MODE=<n> (testing): the menus start at progress mode n instead of the title (src/menu/menu_a_b.c:
                   6 Dragon History, 13 Ultimate Battle, 33 World Tournament, 38 the versus menu, 44 Training, 48 Evolution Z,
                   53 the disc fusion, 60 Character Reference, 62 Options, 70 Shenron): its screens, seen without the way
                   there being pressed */
                static int modeEntered;

                if (!modeEntered && getenv("BT3_MENU_MODE") != NULL && !Port_NetSession() && gProgress != NULL && gSaveData != NULL) {
                    modeEntered = 1;
                    gProgress->mode = atoi(getenv("BT3_MENU_MODE"));
                }
            }
            {   /* BT3_TBO=1 (testing): the first menu is Tenkaichi Budokai Online's (src/menu/port_tbo.c), not the
                   title: its screens, a lobby's included (BT3_LOBBY), without a button pressed */
                static int tboEntered;
                extern int gPortScreen; /* src/menu/port_screen.c */

                if (!tboEntered && getenv("BT3_TBO") != NULL && !Port_NetSession() && gProgress != NULL && gSaveData != NULL) {
                    tboEntered = 1;
                    gPortScreen = 1; /* PORT_SCREEN_TBO */
                    gProgress->mode = 80; /* MPROG_MODE_PORT_SCREEN */
                }
            }
            gPortMenuMode = 1; /* the renderer treats all 2D as one centred 4:3 page while the menus run */
            r = __real_Progress_Main(arg);
            gPortMenuMode = 0;
            sFromMenu = 1; /* a battle set up by the real menus follows: the traces apply to it too */
            return r;
        }
    }
    if (sBattles++ > 0 && !(getenv("BT4_RENDER_TEST") && getenv("BT4_MEMORY_BATTLES") && sBattles<=atoi(getenv("BT4_MEMORY_BATTLES")))) {
        printf("bt3: battle finished\n");
        exit(0);
    }
    if (path != NULL) {
        static uint8_t buf[REPLAY_FILE_SIZE] __attribute__((aligned(16)));
        FILE *fp = fopen(path, "rb");

        if (fp == NULL || fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
            fprintf(stderr, "bt3: cannot read replay %s (0x%X bytes expected)\n", path, REPLAY_FILE_SIZE);
            exit(2);
        }
        fclose(fp);
        Battle_ClearWork();
        if (!BattleReplay_Load(buf + REPLAY_HEADER, REPLAY_BLOCK)) {
            fprintf(stderr, "bt3: %s is not a replay this version accepts\n", path);
            exit(2);
        }
        printf("bt3: replay %s\n", path);
    } else {
        Demo_SetupBattle();
        if(getenv("BT4_STORY_BATTLE_TEST")) {
            extern void BattleSetup_Clear(void);
            extern void BattleSetup_SetScript(int script);
            BattleSetup_Clear();BattleSetup_SetScript(atoi(getenv("BT4_STORY_BATTLE_TEST")));
        }
        if (getenv("BT4_TEST_TECH") != NULL || getenv("BT4_HUD_TEST") != NULL) {
            extern unsigned char GAME_PTR Battle_GetSetup(void);
            unsigned char GAME_PTR setup = Battle_GetSetup();
            *(int GAME_PTR)(setup + 8) = 0;
            *(int GAME_PTR)(setup + 0xC0 + 0x208) = 0;
            *(int GAME_PTR)(setup + 0xC0 + 0x270 + 0x208) = 0;
        }
        printf("bt3: attract-demo battle\n");
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_NATIVE_DUEL_TEST")) {
            extern unsigned char GAME_PTR Battle_GetSetup(void);
            *(int GAME_PTR)(Battle_GetSetup()+8)=0;
        }
        /* BT3_CHARA=<id> (and BT3_COSTUME=<n>, BT3_VARIANT=1 for the damaged model): side 0's fighter of the demo
           battle, for looking at one character's model without going through the menus */
        extern struct { int chara, costume, variant; } GAME_PTR BattleSide_GetMember(int side, int index);
        if (getenv("BT3_CHARA") != NULL) {
            BattleSide_GetMember(0, 0)->chara = atoi(getenv("BT3_CHARA"));
            BattleSide_GetMember(0, 0)->costume = getenv("BT3_COSTUME") != NULL ? atoi(getenv("BT3_COSTUME")) : 0;
            BattleSide_GetMember(0, 0)->variant = getenv("BT3_VARIANT") != NULL;
            /* Rebuild startForm too: the loader reads that snapshot, not members[]. */
            { extern void BattleSetup_Finish(void); BattleSetup_Finish(); }
            printf("bt3: side 0 is character %d, costume %d\n", BattleSide_GetMember(0, 0)->chara, BattleSide_GetMember(0, 0)->costume);
        }
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_OPP_CHARA")){
            extern void BattleSetup_Finish(void);
            BattleSide_GetMember(1,0)->chara=atoi(getenv("BT4_TEST_OPP_CHARA"));BattleSide_GetMember(1,0)->costume=0;BattleSide_GetMember(1,0)->variant=0;BattleSetup_Finish();
        }
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TRAINING_MAP_TEST")){
            extern unsigned char GAME_PTR Battle_GetSetup(void);
            extern void BattleSetup_SetRule(int,int,int,int,int,int,int),BattleSetup_Finish(void);
            unsigned char GAME_PTR setup=Battle_GetSetup();int stage=atoi(getenv("BT4_TRAINING_MAP_TEST"));
            /* The real training rule and inert opponent, not the demo sequence.
             * This opt-in fixture does not run in ordinary player sessions. */
            BattleSetup_SetRule(0,6,54,0,7,stage,0);
            *(int GAME_PTR)(setup+0xC0+0x208)=0;
            *(int GAME_PTR)(setup+0xC0+0x270+0x208)=2;
            *(int GAME_PTR)(setup+0xC0+0x270+0x10)=-1;
            BattleSetup_Finish();
            {extern int Battle_GetMode(void);
             fprintf(stderr,"training map fixture: mode=%d stage=%d opponent-level=-1\n",Battle_GetMode(),stage);}
        }
    }
    fflush(stdout);
    return 0;
}

/* Overlay_Load reads the menu overlay to its PS2 address: there is nothing to read on PC (size 0). */
int sceOpen(char *name, int flags) { (void)name; (void)flags; return 3; }
int sceLseek(int fd, int offset, int whence) { (void)fd; (void)offset; (void)whence; return 0; }
int sceRead(int fd, void *buf, int size) { (void)fd; (void)buf; return size; }
int sceClose(int fd) { (void)fd; return 0; }

/*
 * BT3_TRACE=<n>: every n vertical blanks, print the state of the fight (sequence state, battle clock words, health
 * and position of the two fighters). Read-only; called from the vertical blank (plat_stub.c).
 */
/* A pointer variable of the game: 4 bytes wide in the 64-bit build too (port/tools/ptr32.py does this for the game's
   own files; a declaration written in a port file has to say it itself). */
extern struct { int state; } GAME_PTR gBtlSeq;
extern int *BtlSeq_GetClock(void);
extern int BtlCharApi_GetHp(int objId);
extern void BtlCharApi_GetPos(int objId, float *out);



/* For the lobby's tests (gs/net_lobby.c, BT3_LOBBY_DO "waitseq"): the battle sequence's state, -1 outside a battle. */
int Port_BattleSeqState(void) {
    return gBtlSeq != NULL ? gBtlSeq->state : -1;
}

/* For the lobby's tournaments (gs/net_lobby.c, result_watch): the battle sequence's state (-1 outside a battle) and,
   once the fight is decided (states 4 to 6), the result's winner bits (include/battle/battle.h, BTL_RESULT_*: 1 side
   0 won, 2 side 1 won, 4 a draw, 8 left without a finish); 0 before. Read only. */
int Port_BattleOutcome(int *winner) {
    extern struct { int winner, reason; } GAME_PTR Battle_GetResult(void);
    *winner = 0;
    if (gBtlSeq == NULL) {
        return -1;
    }
    if (gBtlSeq->state >= 4 && gBtlSeq->state <= 6) {
        *winner = Battle_GetResult()->winner;
    }
    return gBtlSeq->state;
}

/* Main-menu item 4, Dragon Net Battle, was confirmed (src/menu/menu_a.c): the settings code opens the online
   screen when it sees the request (port/src/gs/ui.cpp). Without a window nothing does. */
volatile int gPortNetMenuRequest;
void Port_NetMenuOpen(void) {
    gPortNetMenuRequest = 1;
}
/* BT3_SPEC_LINE=0 (testing): a spectator's line over the match is not drawn (src/menu/port_tbo.c). It is drawn with
   the game's own lists, so a spectator's whole state differs from its host's by what it draws; without it (and with
   its pause menu shut) the two are the same, blank for blank, as the spectator tests compare them. */
int Port_SpecLineShown(void) {
    PORT_HOST static int shown = -1; /* (not the game's state) */
    if (shown < 0) {
        shown = getenv("BT3_SPEC_LINE") == NULL || atoi(getenv("BT3_SPEC_LINE")) != 0;
    }
    return shown;
}

/* (asked here and not in the game's file: there a pointer is 4 bytes wide in the 64-bit build, and getenv's is not)
   Dragon Net Battle is listed only when the player asks for it: F1 > Online > Advanced, "List Dragon Net Battle in the
   main menu" (setting net_dnb_menu, off by default; Tenkaichi Budokai Online's screens do everything it does).
   BT3_MENU_ITEM4=0 / 1 (testing) hides or lists it whatever the setting says. */
int Port_NetMenuListed(void) {
    extern int Port_Setting(const char *name, int def);
    const char *e = getenv("BT3_MENU_ITEM4");
    if (e != NULL) {
        return atoi(e) != 0;
    }
    return Port_Setting("net_dnb_menu", 0) != 0;
}

/* "Unlock everything" on the Cheats tab of the settings window (port/src/gs/ui.cpp): the game's own leftover debug
   function Save_UnlockAll (0x266088, no caller in the game): every character, stage, music track and item, the
   unlock flags, and the largest amount of Zenni. It changes the save in memory (the records list is emptied too);
   the game writes it to the card the next time it saves. The window only sets the request; it is carried out
   here, on the game's side, at a vertical blank. */


/* For gs/net.c's check that the two copies of an online match still compute the same game: 1v1 keeps its original
   two-fighter digest. Tag Team online uses a separate branch that hashes every active actor, since its roster can
   contain ten fighters. */
unsigned long long Port_NetDigest(void) {
    struct { unsigned arrived; int mode, first, battle, seq, clock, hp[2]; unsigned pos[2][3]; } d;
    float q[4] __attribute__((aligned(16)));
    unsigned long long h = 0xCBF29CE484222325ull;
    const unsigned char *b = (const unsigned char *)&d;
    unsigned i;
    int k;

    /* Tag Team's lockstep roster can contain up to ten actors. Keep its
       synchronization identity separate from the two-fighter 1v1 digest. */
    {
        extern int TagBattle_Network(void), TagBattle_Count(void), TagBattle_Team(int), TagBattle_Target(int);
        if (TagBattle_Network() && gBtlSeq != NULL && gBtlSeq->state >= 2) {
            struct TagActorDigest {
                int hp, ki, chara, costume, action, target, team;
                int pos[3];
            };
            struct TagDigest {
                int mode, first, seq, clock, count;
                struct TagActorDigest actors[10];
            } tag;
            extern unsigned char GAME_PTR BtlChar_Get(int);
            extern int BtlSide_GetKi(int), BtlCharApi_GetChara(int), BtlCharApi_GetCostume(int);
            extern int BtlAct_GetCurrent(void *);
            int count = TagBattle_Count();
            int actor, axis;

            memset(&tag, 0, sizeof(tag));
            tag.mode = gProgress != NULL ? gProgress->mode : 0;
            tag.first = gProgress != NULL && (gProgress->flags & 0x40) != 0;
            tag.seq = gBtlSeq->state;
            tag.clock = BtlSeq_GetClock()[0];
            if (count < 0) count = 0;
            if (count > 10) count = 10;
            tag.count = count;
            for (actor = 0; actor < count; ++actor) {
                float pos[4] __attribute__((aligned(16)));
                struct TagActorDigest *d = &tag.actors[actor];
                d->hp = BtlCharApi_GetHp(actor);
                d->ki = BtlSide_GetKi(actor);
                d->chara = BtlCharApi_GetChara(actor);
                d->costume = BtlCharApi_GetCostume(actor);
                d->action = BtlAct_GetCurrent(BtlChar_Get(actor));
                d->target = TagBattle_Target(actor);
                d->team = TagBattle_Team(actor);
                BtlCharApi_GetPos(actor, pos);
                for (axis = 0; axis < 3; ++axis) {
                    float scaled = pos[axis] * 16.0f;
                    d->pos[axis] = (int)(scaled + (scaled >= 0.0f ? 0.5f : -0.5f));
                }
            }
            b = (const unsigned char *)&tag;
            for (i = 0; i < sizeof(tag); ++i) {
                h = (h ^ b[i]) * 0x100000001B3ull;
            }
            return h;
        }
    }

    memset(&d, 0, sizeof(d));
    d.arrived = sNetArrivedAt;
    if (gProgress != NULL) {
        d.mode = gProgress->mode;
        d.first = (gProgress->flags & 0x40) != 0;
    }
    if ((sBattles != 0 || sFromMenu) && gBtlSeq != NULL && gBtlSeq->state >= 2) {
        d.battle = 1;
        d.seq = gBtlSeq->state;
        d.clock = BtlSeq_GetClock()[0];
        for (k = 0; k < 2; k++) {
            d.hp[k] = BtlCharApi_GetHp(k);
            BtlCharApi_GetPos(k, q);
            memcpy(d.pos[k], q, 12);
        }
    }
    for (i = 0; i < sizeof(d); i++) { /* FNV-1a */
        h = (h ^ b[i]) * 0x100000001B3ull;
    }
    return h;
}

void Port_Trace(unsigned vblanks) {
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_CAMERA_GEOMETRY")&&gBtlSeq&&gBtlSeq->state==3&&BtlSeq_GetClock()[0]>=150){
        extern int ChrCam_TagRegression(void);
        int failed=ChrCam_TagRegression();fflush(stdout);exit(failed?3:0);
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_CAPTURE_ATTACK")&&gBtlSeq&&gBtlSeq->state==3){
        static unsigned captured;extern void Port_TagCapture(void);
        if(BtlSeq_GetClock()[0]>=140 && vblanks%12==0 && captured<60){Port_TagCapture();++captured;}
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_OWNERSHIP_TEST")&&gBtlSeq&&gBtlSeq->state==3){
        extern int TagBattle_Count(void),TagBattle_LocalActor(void),TagBattle_ViewActor(void),TagBattle_CameraActor(int),TagBattle_Pair(int);
        extern unsigned char GAME_PTR BtlChar_Get(int);
        extern unsigned char GAME_PTR BtlObj_Get(int);
        extern int GAME_PTR BtlMember_GetActive(void *);
        extern int BattleSide_GetObjId(int);
        int a,view=TagBattle_ViewActor();
        if(vblanks%30==0){
            extern void BtlCharApi_GetCamPose(int,float *,float *);
            float camera[4],rotation[4];int owner=TagBattle_CameraActor(view);
            float GAME_PTR pos=(float GAME_PTR)(BtlChar_Get(owner)+0x10);
            BtlCharApi_GetCamPose(BattleSide_GetObjId(owner),camera,rotation);
            float dx=camera[0]-pos[0],dy=camera[1]-pos[1],dz=camera[2]-pos[2];
            fprintf(stderr,"tag camera distance: frame=%u view=%d owner=%d distance=%.2f action=%x\n",vblanks,view,owner,sqrtf(dx*dx+dy*dy+dz*dz),*(int GAME_PTR)(BtlChar_Get(owner)+0x948));
        }
        if(vblanks%30==0)for(a=0;a<TagBattle_Count();++a){
            int GAME_PTR member=BtlMember_GetActive(BtlChar_Get(a));
            unsigned char GAME_PTR obj=BtlObj_Get(BattleSide_GetObjId(a));
            fprintf(stderr,"tag ownership: frame=%u local=%d view=%d actor=%d chara=%d hp=%d pair=%d camera=%d model=%d\n",vblanks,TagBattle_LocalActor(),view,a,member[0],BtlCharApi_GetHp(a),TagBattle_Pair(a),TagBattle_CameraActor(view),*(int GAME_PTR)(obj+0xC));
        }
    }
    /* Snapshot positions for audio outside the rollback heap. No network or
     * device operation runs here, and lobby voice keeps its normal level. */
    {
        extern int Port_NetActive(void),Port_NetView(void),TagBattle_Active(void),TagBattle_Count(void),TagBattle_ViewActor(void);
        extern void PortVoice_World(int,int,int,const float *);
        extern int BattleSide_GetObjId(int);
        extern void BtlCharApi_GetBasePos(int,float *);
        extern void PortAudio_World(int,int,int,const float *,const int *);
        float positions[40]={0};int alive[10]={0};int count=0,mode=0,listener=0,a;
        if(gBtlSeq&&gBtlSeq->state==3){
            if(TagBattle_Active()){mode=2;count=TagBattle_Count();listener=TagBattle_ViewActor();}
            else if(Port_NetActive()){mode=1;count=2;listener=Port_NetView();}
            for(a=0;a<count;++a){int object=BattleSide_GetObjId(a);BtlCharApi_GetBasePos(object,positions+a*4);alive[a]=BtlCharApi_GetHp(object)>0;}
        }
        PortVoice_World(mode,listener,count,positions);
        PortAudio_World(mode==2,listener,count,positions,alive);
    }
    if ((getenv("BT4_TAG_ACTOR_TEST") != NULL || getenv("BT4_TAG_LOCAL_TEST") != NULL) && gBtlSeq != NULL && gBtlSeq->state >= 2 && vblanks % 120 == 0) {
        extern int TagBattle_Count(void),TagBattle_Team(int),TagBattle_Target(int), BtlAiMgr_GetLevel(int side);
        extern unsigned char GAME_PTR BtlChar_Get(int side);
        int actor;
        if(getenv("BT4_TAG_LONG_VIDEO")){
            extern unsigned char GAME_PTR Battle_GetWork(void);
            extern int BtlChars_IsTimeStopped(void),BtlChange_GetPlayer(void),BtlStage_IsReady(void);
            fprintf(stderr,"tag live: vb=%u flags=%llx stage-ready=%d stop=%d loader-owner=%d clock=%d\n",vblanks,*(unsigned long long GAME_PTR)(Battle_GetWork()+0x19F0),BtlStage_IsReady(),BtlChars_IsTimeStopped(),BtlChange_GetPlayer(),BtlSeq_GetClock()[0]);
        }
        for (actor=0;actor<TagBattle_Count();++actor) {
            float q[4];unsigned char GAME_PTR chr=BtlChar_Get(actor);
            BtlCharApi_GetPos(actor,q);
            fprintf(stderr,"tag actor vb=%u id=%d pos=%.2f,%.2f,%.2f hp=%d action=%x ai=%d level=%d team=%d target=%d\n",vblanks,actor,q[0],q[1],q[2],BtlCharApi_GetHp(actor),*(int GAME_PTR)(chr+0x948),*(int GAME_PTR)(chr+0x1278),BtlAiMgr_GetLevel(actor),TagBattle_Team(actor),TagBattle_Target(actor));
            if(getenv("BT4_TAG_RUSH_SCENE")){
                extern int TagBattle_IsActorLoading(int);
                extern float BtlAnim_GetFrame(void *);
                fprintf(stderr,"tag rush motion: actor=%d freeze=%d loading=%d frame=%.2f\n",actor,*(int GAME_PTR)(chr+0x1320),TagBattle_IsActorLoading(actor),BtlAnim_GetFrame(chr));
            }
        }
    }
    static int every = -1;
    float p[2][4] __attribute__((aligned(16)));
    unsigned u[2][3];
    int *clock;
    int i;

    if (getenv("BT4_TEST_TRANSFORM") != NULL && !Port_NetSession() && gBtlSeq != NULL && gBtlSeq->state == 3) {
        extern unsigned char GAME_PTR BtlChar_Get(int side);
        extern int GAME_PTR BtlMember_GetActive(void *chr);
        extern int GAME_PTR BtlMember_GetActiveGauge(void *chr);
        extern int BtlParam_GetSlotId(void *chr, int slot);
        extern int BtlAct_CanTransform(void *chr, int slot, int need, int allowed);
        extern int BtlAct_QueueTransform(void *chr, int slot);
        extern int BtlAct_GetQueued(void *chr);
        extern void BtlAct_Request(void *chr, int action);
        static int started, target, origin, completed;
        static unsigned began;
        int actor=getenv("BT4_TEST_TECH_ACTOR")?atoi(getenv("BT4_TEST_TECH_ACTOR")):0;
        unsigned char GAME_PTR chr = BtlChar_Get(actor);
        int GAME_PTR member = BtlMember_GetActive(chr);
        int action=*(int GAME_PTR)(chr+0x948);
        if(getenv("BT4_RENDER_TEST")){
            extern int TagBattle_Count(void);int n;
            for(n=0;n<TagBattle_Count();++n)*(int GAME_PTR)(BtlChar_Get(n)+0x1278)=0;
        }
        extern unsigned char GAME_PTR BtlObj_Get(int);extern int BattleSide_GetObjId(int);
        int model=*(int GAME_PTR)(BtlObj_Get(BattleSide_GetObjId(actor))+12);
        if (!started && BtlSeq_GetClock()[0] >= 150) {
            int slot=atoi(getenv("BT4_TEST_TRANSFORM"));
            int GAME_PTR gauge=BtlMember_GetActiveGauge(chr);
            gauge[3]=100000;gauge[5]=0;*(int GAME_PTR)(chr+0x1300)=1;
            if(completed&&getenv("BT4_TEST_TRANSFORM_STEPS")){
                int n;for(n=0;n<4;++n)if(BtlParam_GetSlotId(chr,n)!=255&&BtlAct_CanTransform(chr,n,1,1)){slot=n;break;}
            }
            target=BtlParam_GetSlotId(chr,slot);origin=member[0];
            if(target==255 || !BtlAct_CanTransform(chr,slot,1,1)) {
                fprintf(stderr,"bt4-transform: unavailable source=%d slot=%d target=%d\n",origin,slot,target);exit(4);
            }
            BtlAct_QueueTransform(chr,slot);
            BtlAct_Request(chr,BtlAct_GetQueued(chr));
            began=vblanks;started=1;
            fprintf(stderr,"bt4-transform: begin source=%d slot=%d target=%d\n",origin,slot,target);
        }
        if(started && vblanks-began>180 && member[0]==target && model==target && (action<0xEC || action>0xF0)) {
            fprintf(stderr,"bt4-transform: PASS target=%d model=%d action=%x elapsed=%u\n",member[0],model,action,vblanks-began);
            ++completed;
            if(getenv("BT4_TEST_TRANSFORM_STEPS")&&completed<atoi(getenv("BT4_TEST_TRANSFORM_STEPS"))){started=0;return;}
            exit(0);
        }
        if(started && vblanks-began>1500) {
            fprintf(stderr,"bt4-transform: FAIL target=%d actual=%d model=%d action=%x\n",target,member[0],model,action);exit(3);
        }
    }

    if (getenv("BT4_HUD_TEST") != NULL && !Port_NetSession() && gBtlSeq != NULL && gBtlSeq->state == 3) {
        extern unsigned char GAME_PTR BtlChar_Get(int side);
        extern int GAME_PTR BtlMember_GetActiveGauge(void *chr);
        int side;
        for(side=0;side<2;++side) {
            int GAME_PTR gauge=BtlMember_GetActiveGauge(BtlChar_Get(side));
            const char *health=getenv("BT4_TEST_HEALTH");
            gauge[0]=getenv("BT4_RENDER_TEST") && health ? atoi(health) : side ? 5000 : 42000;
            if(getenv("BT4_RENDER_TEST")&&getenv("BT4_HEALTH_SWEEP")) {
                static const int hp[]={50000,48000,45000,42000,40000,35000,30000,20000,10000,5000};
                gauge[0]=hp[(BtlSeq_GetClock()[0]/90)%10];
            }
            gauge[3]=side ? 60000 : 0;
            gauge[5]=side ? 400000 : 0;
        }
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_WALL_SCENE_TEST")&&!Port_NetSession()&&gBtlSeq&&gBtlSeq->state==3) {
        extern float BtlStage_GetInnerRadius(void);
        extern unsigned char GAME_PTR BtlChar_Get(int);
        extern void BtlChar_Place(void *,const float *,const float *,int);
        extern int BtlStage_FindZoneNear(int,const float *);
        int tick=BtlSeq_GetClock()[0],side;
        float radius=BtlStage_GetInnerRadius()-8.f;
        for(side=0;side<2;++side){
            float a=tick*.002f+side*.015f;
            float pos[4]={radius*cosf(a),-100.f,radius*sinf(a),1.f};
            float rot[4]={0.f,1.5707963f-a+side*3.14159265f,0.f,0.f};
            BtlChar_Place(BtlChar_Get(side),pos,rot,BtlStage_FindZoneNear(0,pos));
        }
    }
    if (getenv("BT4_TEST_RULES") != NULL && !Port_NetSession() && gBtlSeq != NULL &&
        gBtlSeq->state == 3 && BtlSeq_GetClock()[0] >= 150) {
        extern int Bt4_RunRulesRegression(void);
        int failed = Bt4_RunRulesRegression();
        fflush(stdout);
        exit(failed ? 3 : 0);
    }
    if (getenv("BT4_TAG_CLASH_REGRESSION") && gBtlSeq && gBtlSeq->state==3 && BtlSeq_GetClock()[0]>=150) {
        extern int TagClash_RunRegression(void);
        int failed=TagClash_RunRegression();fflush(stdout);exit(failed?3:0);
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_CLASH_ANSWER_TEST")&&gBtlSeq&&gBtlSeq->state==3&&BtlSeq_GetClock()[0]>=60){
        extern int ClashAnswers_RunRegression(void);int failed=ClashAnswers_RunRegression();fflush(stdout);exit(failed?3:0);
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_MISSION_TEST")&&gBtlSeq&&gBtlSeq->state==3){
        static int checked;
        if(!checked){int a=BtlCharApi_GetHp(0),b=BtlCharApi_GetHp(1);checked=1;
            fprintf(stderr,"mission-start index=%s hp=%d,%d %s\n",getenv("BT4_MISSION_TEST"),a,b,a>0&&b>0?"PASS":"FAIL");
            if(a<=0||b<=0)exit(3);
        }
        if(BtlSeq_GetClock()[0]>=60){fprintf(stderr,"mission-play index=%s tick=%d PASS\n",getenv("BT4_MISSION_TEST"),BtlSeq_GetClock()[0]);exit(0);}
    }

    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_STICK_AUDIT")&&gBtlSeq&&gBtlSeq->state==3&&BtlSeq_GetClock()[0]>=60){
        extern int TagBattle_RunTargetAudit(void);int failed=TagBattle_RunTargetAudit();fflush(stdout);exit(failed?3:0);
    }
    if(getenv("BT4_TAG_COMBAT_AUDIT") && gBtlSeq && gBtlSeq->state==3 && BtlSeq_GetClock()[0]>=60){
        extern int TagBattle_RunCombatAudit(void);int failed=TagBattle_RunCombatAudit();fflush(stdout);exit(failed?3:0);
    }
    if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_TEST_FINISH") && gBtlSeq && gBtlSeq->state==3 && BtlSeq_GetClock()[0]>=180){
        extern int TagBattle_Count(void);
        extern unsigned char GAME_PTR BtlChar_Get(int side);
        extern int GAME_PTR BtlMember_GetActiveGauge(void *chr);
        int a;for(a=1;a<TagBattle_Count();a+=2)BtlMember_GetActiveGauge(BtlChar_Get(a))[0]=0;
    }
    if(getenv("BT4_RENDER_TEST") && getenv("BT4_STORY_LOSS_TEST") && gBtlSeq && gBtlSeq->state==3 && BtlSeq_GetClock()[0]>=180){
        extern unsigned char GAME_PTR BtlChar_Get(int actor);
        extern int BtlMember_Damage(void *chr,int damage,int flags);
        static int injected;
        if(!injected){
            /* Use normal damage bookkeeping so the story receives KO events. */
            BtlMember_Damage(BtlChar_Get(0),1000000,0x3C1);injected=1;
            fprintf(stderr,"story-loss damage injected with KO events\n");
        }
    }
    if(getenv("BT4_STORY_BATTLE_TEST") && !getenv("BT4_STORY_CONTINUE_TEST") && gBtlSeq && gBtlSeq->state==3 && BtlSeq_GetClock()[0]>=60) {
        fprintf(stderr,"story-play script=%s state=3 tick=%d PASS\n",getenv("BT4_STORY_BATTLE_TEST"),BtlSeq_GetClock()[0]);exit(0);
    }
    if (getenv("BT4_TEST_TECH") != NULL && gBtlSeq != NULL && gBtlSeq->state == 3) {
        extern unsigned char GAME_PTR BtlChar_Get(int side);
        extern int GAME_PTR BtlMember_GetActiveGauge(void *chr);
        extern int BtlAct_GetTechniqueAction(void *chr, int slot);
        extern void BtlAct_Request(void *chr, int action);
        static int started;
        static unsigned began;
        static int entered;
        static unsigned recovered;
        int actor=getenv("BT4_TEST_TECH_ACTOR")?atoi(getenv("BT4_TEST_TECH_ACTOR")):0;
        int simultaneous=getenv("BT4_TEST_TECH_SIMULTANEOUS")!=NULL;
        int victim=getenv("BT4_TEST_TARGET")?atoi(getenv("BT4_TEST_TARGET")):(actor^1);
        unsigned char GAME_PTR chr = BtlChar_Get(actor);
        int support=getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_SUPPORT_ACTOR")?atoi(getenv("BT4_TEST_SUPPORT_ACTOR")):(actor==0?2:0);
        unsigned char GAME_PTR other = simultaneous?BtlChar_Get(support):chr;
        int tick = BtlSeq_GetClock()[0];
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_CONTACT")&&!started&&tick>=100){
            extern void BtlChar_Place(void *,const float *,const float *,int);
            extern int BtlStage_FindZoneNear(int,const float *);
            extern int TagBattle_Count(void),TagBattle_SetTarget(int,int);
            static int placed;int n;
            for(n=0;n<TagBattle_Count();++n){
                float pos[4]={350.f,-150.f,350.f+n*40.f,1.f};
                float rot[4]={0.f,0.f,0.f,0.f};
                *(int GAME_PTR)(BtlChar_Get(n)+0x1278)=0;
                if(!placed)BtlAct_Request(BtlChar_Get(n),0xB);
                if(n==actor){pos[0]=0.f;pos[2]=getenv("BT4_TEST_BEAM_SCENE")?-800.f:-40.f;}
                if(n==victim){pos[0]=0.f;pos[2]=0.f;rot[1]=3.14159265f;}
                BtlChar_Place(BtlChar_Get(n),pos,rot,BtlStage_FindZoneNear(-1,pos));
            }
            TagBattle_SetTarget(actor,victim);TagBattle_SetTarget(victim,actor);
            placed=1;
        }
        if(getenv("BT4_RENDER_TEST")){
            extern int TagBattle_Count(void);
            int fixtureActor;
            for(fixtureActor=0;fixtureActor<TagBattle_Count();++fixtureActor)
                *(int GAME_PTR)(BtlChar_Get(fixtureActor)+0x1278)=((!getenv("BT4_TEST_CONTACT")||(started&&getenv("BT4_TEST_OUTSIDERS_AI")))&&getenv("BT4_TAG_RUSH_SCENE")&&fixtureActor!=actor&&fixtureActor!=victim)?1:0;
        }
        if (!started && tick >= 150) {
            int slot = atoi(getenv("BT4_TEST_TECH"));
            int GAME_PTR gauge = BtlMember_GetActiveGauge(chr);
            int act = getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_ACTION")?(int)strtol(getenv("BT4_TEST_ACTION"),NULL,0):BtlAct_GetTechniqueAction(chr, slot);
            gauge[3] = gauge[4];
            if(getenv("BT4_TAG_RUSH_SCENE")){
                extern int TagBattle_SetTarget(int,int);
                TagBattle_SetTarget(2,3);TagBattle_SetTarget(3,2);
            }
            BtlAct_Request(chr, act);
            if(simultaneous){
                extern int TagBattle_ReservePair(int,int);
                int GAME_PTR extraGauge=BtlMember_GetActiveGauge(other);
                extraGauge[3]=extraGauge[4];
                TagBattle_ReservePair(0,1);TagBattle_ReservePair(2,3);
                int otherSlot=getenv("BT4_TEST_SUPPORT_TECH")?atoi(getenv("BT4_TEST_SUPPORT_TECH")):slot;
                BtlAct_Request(other,BtlAct_GetTechniqueAction(other,otherSlot));
                fprintf(stderr,"bt4: concurrent tech start actors=%d,%d slot=%d\n",actor,support,slot);
            }
            fprintf(stderr,"bt4: requested tech actor=%d\n",actor);
            started = 1; began = vblanks;
            fprintf(stderr, "bt4: tech start slot=%d action=%x tick=%d vb=%u\n", slot, act, tick, vblanks);
        }
        if (started && *(int GAME_PTR)(chr+0x948) >= 0x106 && *(int GAME_PTR)(chr+0x948) <= 0x126) entered = 1;
        if(started&&getenv("BT4_TEST_ACTION")&&*(int GAME_PTR)(chr+0x948)>=0xB4&&*(int GAME_PTR)(chr+0x948)<=0xBF)entered=1;
        if (entered && vblanks-began > 180 && *(int GAME_PTR)(chr+0x948) < 0xFD && *(int GAME_PTR)(other+0x948) < 0xFD && (!getenv("BT4_TEST_ACTION")||*(int GAME_PTR)(chr+0x948)<0xB4||*(int GAME_PTR)(chr+0x948)>0xBF)) {
            if(!recovered){recovered=vblanks;fprintf(stderr,"bt4: tech recovered vb=%u\n",vblanks);}
            if(getenv("BT4_TAG_OWNERSHIP_TEST")&&vblanks-recovered<120)return;
            fprintf(stderr,"bt4: tech end action=%x tick=%d\n",*(int GAME_PTR)(chr+0x948),tick);
            if(simultaneous)fprintf(stderr,"bt4: concurrent tech complete actors=0,2\n");
            exit(0);
        }
        if (started && (vblanks-began)%120 == 0) {
            unsigned char GAME_PTR opp=BtlChar_Get(victim);
            extern float BtlAnim_GetFrame(void *),BtlAnim_GetLength(void *),BtlAnim_GetStep(void *);
            extern int TagBattle_IsActorLoading(int);
            fprintf(stderr,"bt4: tech state actor=%d freeze=%d loading=%d anim-frame=%.2f/%.2f step=%.3f\n",actor,*(int GAME_PTR)(chr+0x1320),TagBattle_IsActorLoading(actor),BtlAnim_GetFrame(chr),BtlAnim_GetLength(chr),BtlAnim_GetStep(chr));
            fprintf(stderr, "bt4: tech trace vb=%u tick=%d action=%x anim=%x opponent=%x/%x\n",vblanks,tick,*(int GAME_PTR)(chr+0x948),*(int GAME_PTR)(chr+0x974),*(int GAME_PTR)(opp+0x948),*(int GAME_PTR)(opp+0x974));
        }
        if(started && vblanks-began>1800) {
            int action=*(int GAME_PTR)(chr+0x948);
            fprintf(stderr,"bt4: tech end action=%x tick=%d\n",action,tick);
            exit((action>=0xFD && action<=0x13B) || (simultaneous && *(int GAME_PTR)(other+0x948)>=0xFD && *(int GAME_PTR)(other+0x948)<=0x13B) ? 3 : 0);
        }
    }
    {
        /* gs/state.c: BT3_HASH. With it, during a fight, the fight's own values: both fighters' health and
           position, the battle clock and the C library generator's state. Two players' machines differ in much
           (each has its own view, its own picture shape); these have to be the same on both. */
        extern void Port_StateLog(unsigned vblank, const void *fight, unsigned fightSize);
        extern unsigned long long gPortRandNext;
        struct { int hp[2]; unsigned pos[2][3]; int clock; unsigned long long rnd; } f;
        int battle = (sBattles != 0 || sFromMenu) && gBtlSeq != NULL && gBtlSeq->state >= 2;

        if (battle && getenv("BT3_HASH") != NULL) {
            float q[4] __attribute__((aligned(16)));
            int k;
            memset(&f, 0, sizeof(f));
            for (k = 0; k < 2; k++) {
                f.hp[k] = BtlCharApi_GetHp(k);
                BtlCharApi_GetPos(k, q);
                memcpy(f.pos[k], q, 12);
            }
            f.clock = BtlSeq_GetClock()[0];
            f.rnd = gPortRandNext;
            Port_StateLog(vblanks, &f, sizeof(f));
        } else {
            Port_StateLog(vblanks, NULL, 0);
        }
    }
    if (sNetEntered && sNetArrivedAt == 0 && gProgress != NULL && !(gProgress->flags & 0x40)) {
        sNetArrivedAt = vblanks;
    }
    if (Port_NetWarp() && sNetEntered && gProgress != NULL && !(gProgress->flags & 0x40)) {
        Port_NetArrived(); /* the logos and the card check are over: the character select is next, with picture and sound */
    }
    if (gPortUnlockAll) {
        gPortUnlockAll = 0;
        if (gSaveData != NULL) {
            Save_UnlockAll(gSaveData);
            gPortUnlockDone = 1;
            printf("bt3: everything unlocked (Save_UnlockAll)\n");
            fflush(stdout);
        }
    }
    if (every < 0) {
        every = getenv("BT3_TRACE") != NULL ? atoi(getenv("BT3_TRACE")) : 0;
    }
    /* BT3_AT=<tick>[,<tick>...]: when the battle clock first shows one of these tick counts, print the fight state
       and write the heap to port/build/heap_<tick>.bin (to compare with console save states made at those ticks). */
    if ((sBattles != 0 || sFromMenu) && gBtlSeq != NULL && gBtlSeq->state >= 2 && getenv("BT3_AT") != NULL) {
        static unsigned last = 0xFFFFFFFFu;
        unsigned now = (unsigned)BtlSeq_GetClock()[0];

        if (now != last) {
            const char *t = getenv("BT3_AT");

            last = now;
            while (*t != '\0') {
                if ((unsigned)strtoul(t, (char **)&t, 0) == now) {
                    char name[64];
                    FILE *fp;

                    snprintf(name, sizeof(name), "port/build/heap_%u.bin", now);
                    fp = fopen(name, "wb"); /* a developer's dump: skipped where port/build does not exist */
                    if (fp != NULL) {
                        fwrite(Port_Ptr(0x3BE730), 1, 0x1EFB014 - 0x3BE730, fp);
                        fclose(fp);
                    }
#if !defined(_WIN32) && !defined(__APPLE__) /* the linker symbols of an ELF program */
                    {   /* the game's global variables too: [__data_start, _end), base address first */
                        extern char __data_start[], _end[];
                        uint32_t base = (uint32_t)(uintptr_t)__data_start;

                        snprintf(name, sizeof(name), "port/build/glob_%u.bin", now);
                        fp = fopen(name, "wb");
                        if (fp != NULL) {
                            fwrite(&base, 4, 1, fp);
                            fwrite(__data_start, 1, (size_t)(_end - __data_start), fp);
                            fclose(fp);
                        }
                        snprintf(name, sizeof(name), "port/build/heap_%u.bin", now);
                    }
#endif
                    printf("bt3: tick %u (vblank %u, seq %d): hp %d %d; %s\n", now, vblanks, gBtlSeq->state,
                           BtlCharApi_GetHp(0), BtlCharApi_GetHp(1), name);
                    fflush(stdout);
                }
                if (*t == ',') {
                    t++;
                }
            }
        }
    }
    /* BT3_DUMP=<file>: 600 vertical blanks after the battle sequence reaches its last state, write the game heap
       (PS2 addresses 0x3BE730..0x1EFB014) to the file and stop: to be compared with a console memory dump. */
    if ((sBattles != 0 || sFromMenu) && gBtlSeq != NULL && gBtlSeq->state == 6 && getenv("BT3_DUMP") != NULL) {
        static unsigned since;

        if (since == 0) {
            since = vblanks;
        } else if (vblanks - since == 600) {
            FILE *fp = fopen(getenv("BT3_DUMP"), "wb");

            if (fp != NULL) {
                fwrite(Port_Ptr(0x3BE730), 1, 0x1EFB014 - 0x3BE730, fp);
                fclose(fp);
            }
            clock = BtlSeq_GetClock();
            printf("bt3: end of battle: gBtlSeq %p clock %08x %08x %08x %08x hp %d %d; heap dumped\n", (void *)gBtlSeq,
                   clock[0], clock[1], clock[2], clock[3], BtlCharApi_GetHp(0), BtlCharApi_GetHp(1));
            exit(0);
        }
    }
    if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAUNT_AUDIT") && gBtlSeq && gBtlSeq->state==3){
        extern unsigned char GAME_PTR BtlChar_Get(int);
        static int lastAction[2]={-1,-1};int i;
        for(i=0;i<2;++i){unsigned char GAME_PTR chr=BtlChar_Get(i);int action=*(int GAME_PTR)(chr+0x948);
            if(action!=lastAction[i]){lastAction[i]=action;fprintf(stderr,"BT4 action audit: vb=%u actor=%d action=%X animation=%X\n",vblanks,i,action,*(int GAME_PTR)(chr+0x974));}
        }
    }
    if ((sBattles != 0 || sFromMenu) && gBtlSeq != NULL && getenv("BT3_TRACE_SEQ") != NULL) { /* when the battle sequence changes state */
        static int last = -1;
        if (gBtlSeq->state != last) {
            printf("seq: state %d begins at vertical blank %u\n", gBtlSeq->state, vblanks);
            fflush(stdout);
            last = gBtlSeq->state;
        }
    }
    if (every <= 0 || vblanks % (unsigned)every != 0 || sBattles == 0 || gBtlSeq == NULL || gBtlSeq->state < 2) {
        return;
    }
    clock = BtlSeq_GetClock();
    for (i = 0; i < 2; i++) {
        BtlCharApi_GetPos(i, p[i]);
        memcpy(u[i], p[i], 12);
    }
    /* BT3_CAMCHECK=<vblank>: test the pad fighter's line of sight (camera eye -> camera target) against EVERY
       triangle of the stage collision mesh, and say what the game's own sweep answers for the same segment. */
    if (getenv("BT3_CAMCHECK") != NULL && vblanks == (unsigned)atoi(getenv("BT3_CAMCHECK"))) {
        extern void *BtlChar_FindByObjId(int objId);
        extern struct { int nodeCount, unk4, unk8, nodeOfs, polyOfs, vtxOfs; } *gStgColMesh;
        extern void *ColMesh_GetPoly(void *mesh, int idx);
        extern void ColMesh_GetPolyVerts(void *mesh, void *poly, float *v0, float *v1, float *v2);
        extern int BtlCam_TraceStage(float *out, float *from, float *to, float *frac, int *hitObj);
        char *chr = BtlChar_FindByObjId(0);
        float *eye = (float *)(chr + 0x420), *tgt = (float *)(chr + 0x450);
        float v0[4] __attribute__((aligned(16))), v1[4] __attribute__((aligned(16))), v2[4] __attribute__((aligned(16)));
        float out[4] __attribute__((aligned(16))), frac = -1.0f;
        int n = gStgColMesh->unk4, hits = 0, obj = -1, k, r; /* unk4: taken as the polygon count */
        double d[3] = {tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]};

        printf("camcheck: vblank %u, eye (%.2f %.2f %.2f) target (%.2f %.2f %.2f), %d polygons (counts in the header: %d, %d)\n", vblanks,
               (double)eye[0], (double)eye[1], (double)eye[2], (double)tgt[0], (double)tgt[1], (double)tgt[2], n, gStgColMesh->unk4,
               gStgColMesh->unk8);
        for (k = 0; k < n; k++) {
            double e1[3], e2[3], pv[3], tv[3], qv[3], det, u, v, t;
            int j;
            ColMesh_GetPolyVerts(gStgColMesh, ColMesh_GetPoly(gStgColMesh, k), v0, v1, v2);
            for (j = 0; j < 3; j++) { e1[j] = v1[j] - v0[j]; e2[j] = v2[j] - v0[j]; tv[j] = eye[j] - v0[j]; }
            pv[0] = d[1] * e2[2] - d[2] * e2[1]; pv[1] = d[2] * e2[0] - d[0] * e2[2]; pv[2] = d[0] * e2[1] - d[1] * e2[0];
            det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
            if (det > -1e-9 && det < 1e-9) { continue; }
            u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
            qv[0] = tv[1] * e1[2] - tv[2] * e1[1]; qv[1] = tv[2] * e1[0] - tv[0] * e1[2]; qv[2] = tv[0] * e1[1] - tv[1] * e1[0];
            v = (d[0] * qv[0] + d[1] * qv[1] + d[2] * qv[2]) / det;
            t = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;
            if (u >= 0 && v >= 0 && u + v <= 1 && t >= 0 && t <= 1) {
                unsigned flags;
                memcpy(&flags, ColMesh_GetPoly(gStgColMesh, k), 4);
                if (hits++ < 6) {
                    printf("camcheck:   polygon %d crosses the line of sight at %.3f of the way (first word %08x)\n", k, t, flags);
                }
            }
        }
        r = BtlCam_TraceStage(out, eye, tgt, &frac, &obj);
        printf("camcheck: %d polygons cross the line of sight; the game's sweep (eye -> target) returns %d, fraction %.3f, zone %d\n", hits, r,
               (double)frac, obj);
        r = BtlCam_TraceStage(out, tgt, eye, &frac, &obj);
        printf("camcheck: the game's sweep the other way (target -> eye) returns %d, fraction %.3f, zone %d\n", r, (double)frac, obj);
        fflush(stdout);
    }
    {   /* the pad fighter's camera: eye position (fighter block + 0x420) against the ground height under the fighter */
        extern float BtlCharApi_GetGroundY(int objId);
        extern void *BtlChar_FindByObjId(int objId);
        float *chr = BtlChar_FindByObjId(0);
        if (chr != NULL && getenv("BT3_TRACE_CAM") != NULL) {
            float *eye = (float *)((char *)chr + 0x420), g = BtlCharApi_GetGroundY(0);
            unsigned ue[3], ug;
            memcpy(ue, eye, 12);
            memcpy(&ug, &g, 4);
            printf("cam tick %u eye %08x %08x %08x ground %08x\n", (unsigned)clock[0], ue[0], ue[1], ue[2], ug);
        }
    }
    printf("vb %7u seq %d clock %08x %08x %08x %08x | hp %6d %6d | p0 %08x %08x %08x | p1 %08x %08x %08x\n", vblanks,
           gBtlSeq->state, clock[0], clock[1], clock[2], clock[3], BtlCharApi_GetHp(0), BtlCharApi_GetHp(1),
           u[0][0], u[0][1], u[0][2], u[1][0], u[1][1], u[1][2]);
    fflush(stdout);
}

/* BT3_SONG_DEBUG=1: where the stage select asks for an added song's name to be drawn, whenever that changes
   (the menu's state, the song, the name clip's parent position and its own offset, in the game's 512 x 448). */
void Port_SongDebug(int state, int idx, int px, int py, int cx, int cy) {
    static int last[6] = {-99, -99, -99, -99, -99, -99};
    int now[6];
    now[0] = state; now[1] = idx; now[2] = px; now[3] = py; now[4] = cx; now[5] = cy;
    if (getenv("BT3_SONG_DEBUG") != NULL && memcmp(now, last, sizeof(now)) != 0) {
        memcpy(last, now, sizeof(now));
        fprintf(stderr, "song name: menu state %d, added song %d, parent at %d,%d, clip offset %d,%d -> drawn at %d,%d\n", state, idx, px, py, cx, cy,
                px + cx, py + cy);
    }
}
