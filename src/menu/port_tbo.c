#include "common.h"
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "sys/save.h"

#ifdef PORT
/*
 * PC only: "Tenkaichi Budokai Online", the main menu's item 11 (menu_a.c). The port's screen mode (port_screen.c)
 * runs this screen. Its own look for online play, after the mockups of online_design: every picture, piece,
 * letter and button is its own (tbo/ui/*.png, drawn by the renderer at full resolution; see "the pictures" below).
 *
 * The hub lists nine modes. What runs today is what the room-code lobby of port/src/gs/net.c can do (the same as
 * the Dragon Net Battle window, without the window):
 *   Create Room   the host's rules, then a room code to give the other player;
 *   Join Room     a pad keypad (the PC keyboard types too) for the other player's code;
 *   the search    a Dragon Radar while the two games find each other; square: "Train While Waiting" leaves for the
 *                 Ultimate Training menu with the room still open (net.c polls it each blank and starts the match
 *                 from wherever the game is when the other player comes);
 *   Profile       the name the other player sees and the defaults of the connection.
 * Quick Match, Ranked Match, Friends, Tournament, Replays and Leaderboard need the matchmaking service to do more
 * than room codes (a public queue, player ids, ratings): they are listed, marked SOON, and say what they will be.
 * Once both players are there the session is the game's own versus mode (character select, the fight, results).
 */

/* The PC keyboard (port/src/gs/gs_input.c): on a keypad it types (a character at a time: 8 backspace, 13 enter,
   27 escape (back), 0 none) and the arrow keys and the controller move on it. On the other screens it is the pad,
   and Esc asks to close the game, as everywhere. */
extern void PortTbo_TextMode(int on);
extern int PortTbo_TextRead(void);
extern void PortTbo_Copy(const char *text); /* onto the clipboard ("Copy Code") */
extern void PortTbo_Paste(void);            /* the clipboard's room code, typed (Join Room's PASTE) */

/* The game's windows of every menu (battle/view_a_b.c, view_a_c.c), made from the Duel menu's data as
   DuelMenu_Init makes them: the heading's window (IconWin: the cloud swirls in the corners and the line under the
   heading) and the message window (MsgWin: the teal box with the meander corners). */
extern void MsgWin_SetText(void *text); /* (the rest: menu/menu_a.h) */
extern void IconWin_PortHideWords(void);
extern void MsgWin_PortDrawString(u16 *str); /* (battle/view_a_b.c) the box with a line of ours, in the game's font */
extern s32 MsgWin_PortLineRoom(s32 right);
extern void MsgWin_PortSameTop(void);
extern s32 Font_GetWidth(u16 *str); /* (battle/col_c.c) a line's width in the game's font */
#define Port_TextMode PortTbo_TextMode
#define Port_TextRead PortTbo_TextRead
static void Port_EscQuit(s32 on) {
    (void)on;
}
static s32 Port_EscTake(void) {
    return 0;
}

/* The fighters' names (Train While Waiting's choice), by character id. */
static const char *const sTbNames[] = {
    "Goku (Early)",
    "Goku (Med)",
    "Goku (Med) Super Saiyan",
    "Goku (End)",
    "Goku (End) Super Saiyan",
    "Goku (End) Super Saiyan 2",
    "Goku (End) Super Saiyan 3",
    "Goku (GT)",
    "Goku (GT) Super Saiyan",
    "Goku (GT) Super Saiyan 3",
    "Goku (GT) Super Saiyan 4",
    "Kid Goku",
    "Great Ape",
    "Kid Gohan",
    "Teen Gohan",
    "Teen Gohan Super Saiyan",
    "Teen Gohan Super Saiyan 2",
    "Gohan",
    "Gohan Super Saiyan",
    "Gohan Super Saiyan 2",
    "Great Saiyaman",
    "Ultimate Gohan",
    "Piccolo (Early)",
    "Piccolo (End)",
    "Nail",
    "Krillin",
    "Yamcha",
    "Tien",
    "Chiaotzu",
    "Vegeta (Scouter)",
    "Vegeta (Scouter) Great Ape",
    "Vegeta",
    "Vegeta Super Saiyan",
    "Super Vegeta",
    "Vegeta (2nd Form)",
    "Vegeta (2nd Form) Super Saiyan",
    "Vegeta (2nd Form) Super Saiyan 2",
    "Majin Vegeta",
    "Vegeta (2nd Form) Super Saiyan 4",
    "Trunks (Sword)",
    "Trunks (Sword) Super Saiyan",
    "Trunks",
    "Trunks Super Saiyan",
    "Super Trunks",
    "Kid Trunks",
    "Kid Trunks Super Saiyan",
    "Goten",
    "Goten Super Saiyan",
    "Gotenks",
    "Gotenks Super Saiyan",
    "Gotenks Super Saiyan 3",
    "Vegito",
    "Super Vegito",
    "Super Gogeta",
    "Gogeta Super Saiyan 4",
    "Mr Satan",
    "Videl",
    "Great Saiyaman 2",
    "Supreme Kai",
    "Kibito Kai",
    "Yajirobe",
    "Paikuhan",
    "Tapion",
    "Pan",
    "Uub",
    "Majuub",
    "Master Roshi",
    "Master Roshi Full Power",
    "Grandpa Gohan",
    "Namu",
    "Hatchan",
    "King Vegeta",
    "King Vegeta Great Ape",
    "Bardock",
    "Bardock Great Ape",
    "Celipa",
    "Celipa Great Ape",
    "Raditz",
    "Raditz Great Ape",
    "Saibamen",
    "Nappa",
    "Nappa Great Ape",
    "Zarbon",
    "Zarbon Transformed",
    "Dodoria",
    "Cui",
    "Ginyu",
    "Recoome",
    "Burter",
    "Jeice",
    "Guldo",
    "Frieza 1st Form",
    "Frieza 2nd Form",
    "Frieza 3rd Form",
    "Frieza Final Form",
    "Frieza 100% Power",
    "Mecha Frieza",
    "King Cold",
    "Appule",
    "Frieza Soldier",
    "Android 16",
    "Android 17",
    "Android 18",
    "Android 19",
    "Android 2",
    "Cell 1st Form",
    "Cell 2nd Form",
    "Perfect Cell",
    "Super Perfect Cell",
    "Cell Jr",
    "Babidi",
    "Dabura",
    "Majin Buu",
    "Evil Buu",
    "Super Buu",
    "Super Buu Gotenks Absorbed",
    "Super Buu Gohan Absorbed",
    "Kid Buu",
    "Garlic Jr",
    "Super Garlic Jr",
    "Wheelo",
    "Turles",
    "Turles Great Ape",
    "Slug",
    "Slug Giant",
    "Sauza",
    "Cooler",
    "Cooler Final Form",
    "Metal Cooler",
    "Android 13",
    "Android 13 Fusion",
    "Broly",
    "Broly Super Saiyan",
    "Broly Legendary Super Saiyan",
    "Zangya",
    "Bojack",
    "Bojack 100% Power",
    "Janemba",
    "Super Janemba",
    "Hildegarn",
    "Baby Vegeta",
    "Super Baby 1",
    "Super Baby 2",
    "Super Baby Great Ape",
    "Super 17",
    "Nuova Shenron",
    "Shin Shenron",
    "Omega Shenron",
    "Taopaipai",
    "Cyborg Tao",
    "General Blue",
    "Akuman",
    "Pilaf Machine",
    "Pilaf Machine Fusion",
    "Tambourine",
    "Demon King Piccolo",
    "Arale",
    "Chichi",
    "Spopovich",
    "Future Gohan",
    "Future Gohan Super Saiyan",
};
#include "menu/tbo_art.h"
#include "menu/tbo_sizes.h"
#include "menu/port_screen.h"

extern void Job_Run(void);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 arg);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void File_Stub264D90(void);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern s32 ColorFade_IsInDone(void);
extern s32 ColorFade_IsOutDone(void);
extern s32 sceGsSyncPath(s32 mode, u16 timeout);

/* port/src/gs/net.c, state.c, plat_settings.c */
extern void Port_NetOptions(int rollback, int delay);
extern void Port_NetRulesSet(int type, int dp, int time);
extern const char *Port_NetNameGet(void);
extern void Port_NetNameSave(const char *name);
extern int Port_LobbyStartCode(int host, const char *code);
extern int Port_LobbyPoll(void);
extern void Port_LobbyCancel(void);
extern void Port_LobbyLaunch(void);
extern int Port_NetSessionsEnded(void); /* gs/net.c */
extern const char *Port_LobbyCode(void);
extern const char *Port_LobbyError(void);
extern int Port_LobbyOther(void);
extern void Port_LobbyBackground(int on);
extern int Port_LobbyInBackground(void);
extern int Port_RollCan(void);
extern int Port_LobbyStartQuick(int queue);   /* a quick match: 0 one on one, 1 teams (the service's queues S, T) */
extern void Port_LobbyQueuesAsk(void);        /* asks how many wait in each queue */
extern void Port_LobbyQuickArea(const char *area); /* what the next quick match looks for: "auto", "any" or a region */
extern int Port_LobbyWiden(void);             /* a quick match that waits (Auto): seconds after which it looks everywhere, 0 not said */
extern const char *Port_LobbyRivalRegion(void); /* the rival's region's name, "" not known */
extern int Port_LobbyQueueWaiting(int queue); /* ... as last heard, -1 if not known */
extern void Port_NetSpeedSet(int percent);
extern int Port_NetSpeedStandard(void);
extern void Port_NetNameSet(const char *name);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead, void *unk);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health, void *items);
extern void BattleSetup_Finish(void);
extern int Port_DataFingerprint(unsigned long long *out); /* plat_fprint.c: 1 known, 0 being worked out, -1 not done */
extern void GsDraw_ScreenScaleCap(int cap4);             /* port/src/gs/gs_draw.c: the online screens' resolution cap */
extern int Port_DataFingerprintOriginal(void);           /* 1 the data the game computes with is the original disc's */
extern const char *Port_DataFingerprintText(void);
extern int Port_Setting(const char *name, int def);
extern void Port_SettingSave(const char *name, int value);
extern void Port_SettingsWrite(void);
#include "../../port/src/gs/net_lobby.h" /* the lobby (Port_LobbyView, Port_LobbyDo ...): plain C, its view has no pointers */
#include "../../port/src/gs/net_dragon.h" /* Dragon Net: against whom (DrNet_Mode), Ranked (DrRanked_*), players online */
extern void Port_LobbyHelloAsk(void);

/* the screens */
#define TB_HUB 0
#define TB_CREATE 1
#define TB_JOIN 2
#define TB_WAIT 3
#define TB_PROFILE 4
#define TB_NAME 5
#define TB_QUICK 6
#define TB_SPAR 7 /* Train While Waiting: the fighters of the sparring fight */
#define TB_BLOCK 8 /* "Cannot Go Online": this game's data is not the disc's */
#define TB_LOBBY 9 /* a lobby of up to eight (port/src/gs/net_lobby.c): its places, its rules, its talk */
#define TB_RANKED 10 /* Ranked (Dragon Net only): the profile, the battle, search, the ranking, update */
#define TB_RKWAIT 11 /* Ranked: searching for a rival, then the room the service pairs the two in */
#define TB_BOARD 12  /* Ranked: the best fighters */
#define TB_MODES 13
#define TB_FIGHTERS 161
#define TB_TOURNAMENT_STAGE 4 /* the World Tournament Stage, at noon */

/* the hub's modes, in the order of the list */
#define HUB_QUICK 0
#define HUB_RANKED 1
#define HUB_CREATE 2
#define HUB_JOIN 3
#define HUB_LOBBY 4
#define HUB_FRIENDS 5
#define HUB_TOURNAMENT 6
#define HUB_REPLAYS 7
#define HUB_LEADERBOARD 8
#define HUB_PROFILE 9
#define HUB_COUNT 10
#define HUB_LANG 10 /* the language of these screens: the row above the list */
#define HUB_VS 11   /* against whom: Android + Switch (Dragon Net's service) or the PC (the PC port's) */
#define HUB_ITEMS 12

/* Ranked's rows */
#define RK_BATTLE 0
#define RK_SEARCH 1
#define RK_BOARD 2
#define RK_UPDATE 3
#define RK_COUNT 4

/* Create Room's rows */
#define CR_BATTLE 0
#define CR_TIME 1
#define CR_DELAY 2
#define CR_ROLL 3
#define CR_OPEN 4
#define CR_COUNT 5

/* Profile's rows */
/* (the input delay and the rollback are the room's: Create Room sets them; a joiner gets the host's, a quick match
   has its own) */
#define PR_NAME 0
#define PR_COUNT 1

#define TB_CHAT_MAX 60 /* a line of the lobby's chat */
#define KEY_COLS 9
#define KEY_ROWS 4
#define CODE_LEN 6

typedef struct TboScreen {
    s32 frames;
    s32 mode;
    s32 modeT;
    s32 leaving;
    s32 train;          /* it is left for the Ultimate Training, the room still open */
    s32 note;
    char noteLine[96];
    s32 hubCur;         /* the item under the cursor (HUB_*) */
    f32 hubY;           /* where the hub's cursor plate is drawn: it glides */
    s32 lang;           /* the screens' language: 0 English, 1 Portuguese, 2 Spanish (setting tbo_lang) */
    /* the host's choices (kept in the settings) */
    s32 battle;         /* 0 one on one, 1 teams of up to five */
    s32 time;           /* 0..4: 60, 90, 180, 240 seconds, no limit */
    s32 delay;          /* -1 automatic, else frames */
    s32 roll;           /* 0 off, else frames (2..8) */
    s32 crCur;
    s32 prCur;
    /* the keypad */
    char code[CODE_LEN + 1];
    s32 codeLen;
    s32 keyCol;
    s32 keyRow;         /* KEY_ROWS: the row of DELETE / JOIN */
    /* the search */
    s32 host;           /* this side opened the room */
    s32 waitFrom;       /* frames when the search began */
    s32 found;          /* the other player is there: the match is starting */
    s32 launchIn;       /* ... in this many frames (the page says so first: the session holds the last picture shown) */
    s32 ended;          /* Port_NetSessionsEnded when it started: a change says the match has been played */
    s32 waitBack;       /* the screen a cancelled search goes back to */
    /* Quick Match */
    s32 queue;          /* 0 one on one, 1 teams */
    s32 region;         /* where to look: 0 Auto (the nearest first), 1..7 only that region, 8 Anywhere (QK_REGIONS) */
    s32 qkCur;
    s32 asked;          /* frames when the queues were last asked about */
    /* Train While Waiting */
    s32 spMe;           /* your fighter */
    s32 spFoe;          /* your sparring partner */
    s32 spCur;
    s32 blockBack;      /* the screen "Cannot Go Online" goes back to */
    s32 typing;         /* the PC keyboard types (circle on a keypad); else its keys are the pad's, as everywhere */
    s32 lower;          /* the name pad shows small letters */
    /* typing the name */
    char typed[20];
    s32 typedLen;
    /* the lobby (TB_LOBBY) */
    s32 lbCol;          /* 0 the places, 1 the right side (the rules, the buttons) */
    s32 lbPlace;        /* the place under the cursor */
    s32 lbRight;        /* the right side's item under the cursor: a rule 0..4, a button 5..8 */
    s32 lbPicked;       /* the owner: the place whose member is being moved (Move), -1 none */
    s32 lbPop;          /* a window over the page: 0 none, LB_POP_ */
    s32 lbPopCur;
    s32 lbPopPlace;     /* (the place the owner's window is about) */
    s32 lbChat;         /* the keypad for a line of chat is up */
    s32 lbPopT;         /* frames since the window over the page opened */
    s32 lbPopClose;     /* frames left of its going (it slides out, then is gone) */
    s32 lbHostPose;     /* the hosts of the page the lobby's came from, who leave as it comes in (-1 none) */
    f32 lbHostX;
    f32 lbHostH;
    s32 lbHostT;        /* frames since they began to go */
    s32 lbSeenState;    /* the lobby's state last frame (reached: the page's windows come in then) */
    char chat[TB_CHAT_MAX + 1];
    s32 chatLen;
    /* Ranked */
    s32 rkCur;
    s32 rkMode;         /* 0 single, 1 team */
    s32 rkLobby;        /* paired: the room of the two is being opened / joined (the search's own, as a room's) */
} TboScreen;

static TboScreen *sTb;
static s32 sTbBgKind;  /* what the search left open for "Train While Waiting" is: 0 joining, 1 hosting, 2 a quick match */
static s32 sTbBgQueue;
static s32 sTbBgFrames; /* how long it had been searching when the sparring fight began; the sign counts on from it */
s32 gPortTboSpar;       /* the battle that runs is Train While Waiting's sparring fight (its sign, its pause menu) */
static char sTbBgCode[CODE_LEN + 1];
/* A format with a number in it ("%d FRAME") in the chosen language, for sprintf (the translation file's line keeps
   its %d slots, or it is not used: port/src/gs/menu_translation.c). */
static char *Tb_Fmt(char *format) {
    static char buf[4][160];
    static s32 k;
    extern int Port_TrLine(const char *original, char *out, int size);

    k = (k + 1) & 3;
    return Port_TrLine(format, buf[k], 160) ? buf[k] : format;
}

static char *sTbHubName[HUB_ITEMS] = {"Quick Match", "Ranked", "Create Room", "Join Room", "Lobby", "Friends", "Tournament", "Replays", "Leaderboard", "Profile",
                                     "Language", "Play Against"};
static const s32 sTbHubReady[HUB_ITEMS] = {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1};
/* What the hub lists, top to bottom: the language, then what can be played. Ranked Match, Friends, Tournament, Replays
   and Leaderboard need more of the matchmaking service than it has: not listed until they do. */
static const s32 sTbHubOrder[] = {HUB_LANG, HUB_VS, HUB_QUICK, HUB_RANKED, HUB_CREATE, HUB_JOIN, HUB_LOBBY, HUB_PROFILE};
#define HUB_SMALL 2 /* the rows above the list (the language, against whom): small, changed with left / right */
#define HUB_ROWS ((s32)(sizeof(sTbHubOrder) / sizeof(sTbHubOrder[0])))
static s32 Tb_HubRow(s32 item) {
    s32 i;

    for (i = 0; i < HUB_ROWS; i++) {
        if (sTbHubOrder[i] == item) {
            return i;
        }
    }
    return 1;
}
/* the languages of the language row, in their own words (not translated) */
static char *sTbLangName[3] = {"ENGLISH", "PORTUGU\xC3\x8AS", "ESPA\xC3\x91OL"}; /* (UTF-8 in escapes: the build reads the file as ASCII) */
static char *sTbLangCode[3] = {"en", "pt", "es"};
static char *sTbHubSay[HUB_ITEMS] = {
    "Fight whoever is searching right now! No time limit, and you can train while you wait.",
    "Fights for Elo points against Android and Switch players. Points change when both results agree.",
    "Open a room with your own rules, then give your rival its code. No port forwarding needed!",
    "Got a room code from your rival? Type it in and jump right into their room!",
    "A lobby for up to eight: two fight, six watch. You pick who plays and the rules; the others join with its code.",
    "Your rivals, who is online, and invites. That one is coming soon!",
    "The Tenkaichi Budokai itself: brackets of 4 to 16 players. That one is coming soon!",
    "Your last matches, saved to watch again. That one is coming soon!",
    "The strongest in the world, among your friends, and with each fighter. Coming soon!",
    "The name your rivals see.",
    "Choose the language of these screens. Left and right change it.",
    "Choose who you play against: Android and Switch, or the PC.",
};
/* against whom, as the row writes it (not translated: names) */
static char *sTbVsName[2] = {"ANDROID + SWITCH", "PC"};
static void Tb_LobbyOpen(void);
static void Tb_LobbyCreate(void);
/* Quick Match's Region: what the player chooses, and what the matchmaking service is told (port/matchmaking/worker.js:
   Auto takes the nearest first and everyone after 25 seconds; a region players placed in it first and everyone
   after 40; Anywhere whoever waits longest). */
#define QK_REGIONS 9
#define QK_REGION_WIDEN 40 /* a region's search takes everyone after this many seconds (the service's REGION_WIDEN_SECONDS) */
static char *sTbRegionName[QK_REGIONS] = {"AUTO", "NORTH AMERICA", "SOUTH AMERICA", "EUROPE", "MIDDLE EAST", "AFRICA", "ASIA", "OCEANIA", "ANYWHERE"};
static char *sTbRegionArea[QK_REGIONS] = {"auto", "NA", "SA", "EU", "ME", "AF", "AS", "OC", "any"};
static char *sTbTime[5] = {"60 SECONDS", "90 SECONDS", "180 SECONDS", "240 SECONDS", "NO LIMIT"};
static char sTbKeys[KEY_ROWS][KEY_COLS + 1] = {"ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789"};

/* ---------------------------------------------------------------------------------------------- small things */
static f32 Tb_In(s32 delay) {
    f32 t = (f32)(sTb->modeT - delay) / 16.0f;

    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    t = 1.0f - t;
    return 1.0f - t * t * t;
}

static void Tb_Note(char *line) {
    Ps_Copy(sTb->noteLine, line, 96);
    sTb->note = 150;
}

static void Tb_SetMode(s32 mode) {
    sTb->typing = mode == TB_JOIN || mode == TB_NAME; /* the keyboard types on a keypad (elsewhere I is triangle ...) */
    Port_TextMode(sTb->typing);
    if (sTb->mode != mode) { /* (the page that is up again, as when the rival comes on the search: it does not come in anew) */
        sTb->modeT = 0;
    }
    sTb->mode = mode;
}

static void Tb_SaveChoices(void) {
    Port_SettingSave("tbo_battle", sTb->battle);
    Port_SettingSave("tbo_time", sTb->time);
    Port_SettingSave("net_delay2", sTb->delay);
    Port_SettingSave("net_rollback2", sTb->roll);
    Port_SettingsWrite();
}

/* The screens' language: the one chosen here before, else the launcher's (dr_lang: 0 English, 1 Portuguese,
   2 Spanish), else English. */
extern int Port_TboLanguage(const char *locale); /* port/src/gs/menu_translation.c */
static void Tb_LangSet(s32 lang) {
    sTb->lang = lang >= 0 && lang <= 2 ? lang : 0;
    Port_TboLanguage(sTbLangCode[sTb->lang]);
}

static void Tb_LoadChoices(void) {
    s32 lang = Port_Setting("tbo_lang", -1);

    Tb_LangSet(lang >= 0 ? lang : Port_Setting("dr_lang", 0));
    sTb->battle = Port_Setting("tbo_battle", 0);
    sTb->rkMode = Port_Setting("tbo_rank_mode", 0) != 0;
    sTb->time = Port_Setting("tbo_time", 3);
    sTb->delay = Port_Setting("net_delay2", -1);
    sTb->roll = Port_RollCan() ? Port_Setting("net_rollback2", 8) : 0;
    if (sTb->battle < 0 || sTb->battle > 1) {
        sTb->battle = 0;
    }
    if (sTb->time < 0 || sTb->time > 4) {
        sTb->time = 3;
    }
    if (sTb->delay < -1 || sTb->delay > 6) {
        sTb->delay = -1;
    }
    if (sTb->roll < 0 || sTb->roll > 8) {
        sTb->roll = 8;
    }
    sTb->roll &= ~1;
    sTb->queue = Port_Setting("tbo_queue", 0) == 1;
    sTb->region = Port_Setting("tbo_region", 0);
    if (sTb->region < 0 || sTb->region >= QK_REGIONS) {
        sTb->region = 0;
    }
    sTb->spMe = Port_Setting("tbo_spar_me", 0);
    sTb->spFoe = Port_Setting("tbo_spar_foe", 31);
    if (sTb->spMe < 0 || sTb->spMe >= TB_FIGHTERS) {
        sTb->spMe = 0;
    }
    if (sTb->spFoe < 0 || sTb->spFoe >= TB_FIGHTERS) {
        sTb->spFoe = 31;
    }
}

static void Tb_DelayText(char *out, s32 delay) {
    if (delay < 0) {
        sprintf(out, "AUTOMATIC");
    } else {
        sprintf(out, delay == 1 ? Tb_Fmt("%d FRAME") : Tb_Fmt("%d FRAMES"), (int)delay); /* (two lines to translate: not every language adds an S) */
    }
}

static void Tb_RollText(char *out, s32 roll) {
    if (!Port_RollCan()) {
        sprintf(out, "NOT ON THIS PC");
    } else if (roll <= 0) {
        sprintf(out, "OFF");
    } else {
        sprintf(out, Tb_Fmt("UP TO %d FRAMES"), (int)roll);
    }
}

/* ---------------------------------------------------------------------------------------------- the rule */
/* Online matches are only between two unmodded games. Fighter mods are switched off by themselves while a session
   runs (plat_mods.c); what is checked here is the rest of the data the game computes with, against the original
   disc (the gameplay fingerprint, plat_fprint.c: files added under gamedata/mods count). Returns 1 when a search may
   start; else the note or the "Cannot Go Online" window says why. */
static s32 Tb_Vanilla(void) {
    s32 state = Port_DataFingerprint(NULL);

    if (state == 0) {
        Tb_Note("CHECKING YOUR GAME DATA AGAINST THE DISC... TRY AGAIN IN A MOMENT");
        return 0;
    }
    if (state == 1 && Port_DataFingerprintOriginal() == 1) {
        return 1;
    }
    if (state < 0) { /* not computed in this run (Android: the app checked the disc image when it installed it, and keeps
                        the archives whole, not as the loose files it reads): let in, as net.c does */
        return 1;
    }
    sTb->blockBack = sTb->mode;
    Snd_PlaySe(1, 2);
    Tb_SetMode(TB_BLOCK);
    return 0;
}

static void Tb_BlockUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Ps_Prompt(0, "Back");
    if (live && (pad->gamePressed & (PADG_CROSS | PADG_CIRCLE | PADG_TRIANGLE))) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(sTb->blockBack);
    }
}

/* ---------------------------------------------------------------------------------------------- the search */
static void Tb_BeginSearch(s32 host) {
    s32 state;

    if (!Tb_Vanilla()) {
        return;
    }
    if (host) {
        Port_NetOptions(sTb->roll, sTb->delay < 0 ? -2 : sTb->delay);
        Port_NetRulesSet(sTb->battle, 0, sTb->time);
        Tb_SaveChoices();
    } else {
        Port_NetOptions(-1, -1); /* joining: the host's choices arrive with its answer */
    }
    Port_NetNameGet(); /* (the saved name is the one sent) */
    state = Port_LobbyStartCode(host, host ? "" : sTb->code);
    if (state < 0) {
        Tb_Note("THE NETWORK COULD NOT BE OPENED ON THIS PC");
        return;
    }
    sTb->host = host;
    sTb->waitFrom = sTb->frames;
    sTb->found = 0;
    sTb->waitBack = sTb->mode;
    Tb_SetMode(TB_WAIT);
}

/* What the search says (each frame on the screens that wait): the rival came (the match starts), or it failed.
   Returns 1 when the screen that called has nothing more to do this frame. */
static s32 Tb_SearchNews(void) {
    s32 state;

    /* The session is over: the game is back as it was when it was asked for (the rival had just come), so these
       screens are again where they were then. The match has been played: back to the first page. */
    if (sTb->found && Port_NetSessionsEnded() != sTb->ended) {
        sTb->found = 0;
        if (sTb->rkLobby) { /* a ranked match: back to Ranked, where its result shows */
            sTb->rkLobby = 0;
            DrRanked_Open(Port_NetNameGet());
            Tb_SetMode(TB_RANKED);
        } else {
            Tb_SetMode(TB_HUB);
        }
        return 1;
    }
    if (sTb->found && sTb->launchIn > 0) {
        /* The session begins at the game's next blank, from the game's first moment, and holds the picture that
           was shown last while it starts: a few frames of "YOUR RIVAL IS HERE!" first, so that is the picture
           (launched at once, it was the search's, "SEARCHING..."). */
        if (--sTb->launchIn == 0) {
            Port_LobbyLaunch();
        }
        return 1;
    }
    state = sTb->found ? 2 : Port_LobbyPoll();
    if (state == 2 && !sTb->found) {
        if (sTb->rkLobby) {
            DrRanked_Handover(); /* (the service is told the result of this one) */
        }
        sTb->found = 1;
        sTb->launchIn = 4;
        sTb->ended = Port_NetSessionsEnded();
        Snd_PlaySe(1, 1);
        Tb_SetMode(TB_WAIT);
        return 1;
    }
    if (state == 5 && !sTb->found) { /* the code was a lobby's: this game is its member now (gs/net_lobby.c) */
        Snd_PlaySe(1, 1);
        Tb_LobbyOpen();
        return 1;
    }
    if (state <= 0 && !sTb->found) {
        if (state == -3) {
            Tb_Note((char *)Port_LobbyError());
        } else if (state == -4) {
            Tb_Note("THE TWO GAMES COULD NOT REACH EACH OTHER, EVEN THROUGH THE RELAY");
        } else if (state < 0) {
            Tb_Note("THE ROOM COULD NOT BE OPENED");
        }
        Port_LobbyCancel();
        if (sTb->rkLobby) {
            sTb->rkLobby = 0;
            DrRanked_Cancel();
        }
        Tb_SetMode(sTb->waitBack == TB_WAIT || sTb->waitBack == TB_SPAR ? TB_HUB : sTb->waitBack);
        return 1;
    }
    return sTb->found;
}

static void Tb_WaitUpdate(s32 live) {
    Pad *pad = &gPad[0];

    if (!sTb->found) {
        if (sTb->host == 1 && Port_LobbyCode()[0] != 0) {
            Ps_Prompt(0, "Copy Code");
        }
        Ps_Prompt(1, "Train While Waiting");
        Ps_Prompt(4, "Cancel");
    }
    if (Tb_SearchNews()) {
        return;
    }
    if (!live || sTb->found) {
        return;
    }
    if ((pad->gamePressed & PADG_CROSS) && sTb->host == 1 && Port_LobbyCode()[0] != 0) { /* the code, to paste to the rival */
        PortTbo_Copy(Port_LobbyCode());
        Snd_PlaySe(1, 1);
        Tb_Note("COPIED: PASTE THE CODE TO YOUR RIVAL");
    } else if (pad->gamePressed & PADG_SQUARE) { /* the fighters of the sparring fight first; the search goes on */
        Snd_PlaySe(1, 1);
        sTb->spCur = 2;
        Tb_SetMode(TB_SPAR);
    } else if (pad->gamePressed & (PADG_CIRCLE | PADG_TRIANGLE)) {
        Snd_PlaySe(1, 2);
        Port_LobbyCancel();
        Tb_SetMode(sTb->waitBack == TB_WAIT ? TB_HUB : sTb->waitBack);
    }
}

/* ---------------------------------------------------------------------------------------------- Train While Waiting */
static char *Tb_Fighter(s32 chara) {
    static char name[TB_FIGHTERS][48];
    static u8 got[TB_FIGHTERS];

    if (chara < 0 || chara >= TB_FIGHTERS) {
        return "-";
    }
    if (!got[chara]) {
        /* (not `?:` between the table's entry and a literal: the 32-bit pointer build read the entry as 8 bytes) */
        if (chara < (s32)(sizeof(sTbNames) / sizeof(sTbNames[0]))) {
            Ps_Copy(name[chara], sTbNames[chara], 48);
        } else {
            Ps_Copy(name[chara], "?", 48);
        }
        got[chara] = 1;
    }
    return name[chara];
}

/* Left / right on a row of choices, and cross too: cross changes the choice (one step on), as on the language row; it
   does not walk down to the next row (only the last row's cross starts). */
static s32 Tb_Side(void) {
    s32 side = Ps_Side();

    return side == 0 && (gPad[0].gamePressed & PADG_CROSS) ? 1 : side;
}

static void Tb_SparUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 *who;

    Ps_Prompt(0, sTb->spCur == 2 ? "Start Training" : "Change");
    Ps_Prompt(2, "Back");
    if (Tb_SearchNews()) {
        return;
    }
    if (!live) {
        return;
    }
    Ps_Move(&sTb->spCur, 3);
    who = sTb->spCur == 0 ? &sTb->spMe : sTb->spCur == 1 ? &sTb->spFoe : NULL;
    if (who != NULL) {
        s32 step = Tb_Side();

        if (pad->gameRepeat & PADG_R1) {
            step = 10;
        } else if (pad->gameRepeat & PADG_L1) {
            step = -10;
        }
        if (step != 0) {
            *who = (*who + step + TB_FIGHTERS * 4) % TB_FIGHTERS;
            Snd_PlaySe(1, 0);
        }
    }
    if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
        if (sTb->spCur == 2 || (pad->gamePressed & PADG_START)) { /* the search stays open: net.c polls it from here on */
            Snd_PlaySe(1, 1);
            Port_SettingSave("tbo_spar_me", sTb->spMe);
            Port_SettingSave("tbo_spar_foe", sTb->spFoe);
            Port_SettingsWrite();
            Port_LobbyBackground(1);
            sTbBgKind = sTb->host;
            sTbBgQueue = sTb->queue;
            sTbBgFrames = sTb->frames - sTb->waitFrom;
            Ps_Copy(sTbBgCode, sTb->code, CODE_LEN + 1);
            sTb->train = 1;
            sTb->leaving = 1;
            ColorFade_StartOut(0, 0, 0, 0x14);
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_WAIT);
    }
}

/* ---------------------------------------------------------------------------------------------- Quick Match */
static void Tb_QuickStart(void) {
    s32 state;

    if (!Tb_Vanilla()) {
        return;
    }
    Port_NetSpeedSet(Port_NetSpeedStandard());
    Port_NetRulesSet(sTb->queue, 0, 4); /* (a queue's rules: its battle type, no time limit, the standard speed) */
    Port_NetOptions(Port_RollCan() ? 8 : 0, -2);
    Port_NetNameSet(Port_NetNameGet());
    Port_SettingSave("tbo_queue", sTb->queue);
    Port_SettingSave("tbo_region", sTb->region);
    Port_SettingsWrite();
    Port_LobbyQuickArea(sTbRegionArea[sTb->region]);
    state = Port_LobbyStartQuick(sTb->queue);
    if (state < 0) {
        Tb_Note("THE NETWORK COULD NOT BE OPENED ON THIS PC");
        return;
    }
    sTb->host = 2;
    sTb->waitFrom = sTb->frames;
    sTb->found = 0;
    sTb->waitBack = TB_QUICK;
    Tb_SetMode(TB_WAIT);
}

static void Tb_QuickUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 side;

    Ps_Prompt(0, sTb->qkCur == 2 ? "Search" : "Change");
    Ps_Prompt(2, "Back");
    if (sTb->frames - sTb->asked > 1800) { /* how many wait in each queue, asked again every half minute */
        sTb->asked = sTb->frames;
        Port_LobbyQueuesAsk();
    }
    if (!live) {
        return;
    }
    Ps_Move(&sTb->qkCur, 3);
    side = Tb_Side();
    if (sTb->qkCur == 0 && side != 0) {
        sTb->queue = !sTb->queue;
        Snd_PlaySe(1, 0);
    } else if (sTb->qkCur == 1 && side != 0) {
        sTb->region = (sTb->region + side + QK_REGIONS) % QK_REGIONS;
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
        if (sTb->qkCur == 2 || (pad->gamePressed & PADG_START)) {
            Snd_PlaySe(1, 1);
            Tb_QuickStart();
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_HUB);
    }
}

/* ---------------------------------------------------------------------------------------------- Ranked */
/* Ranked is Dragon Net's (port/src/gs/net_dragon.c): an Elo profile per player, a search that pairs two of a level,
   then a room of the two (the first opens it and tells the service its code, the other joins with it), and the
   result, counted when both sides report the same one. These pages are its face; the work is net_dragon.c's. */
static DrRanked sTbRk; /* (what Ranked says this frame) */

/* A message of Ranked (DRMSG_), in English: the screens' line, translated as the others. "" none. */
static char *Tb_RkMsg(s32 m) {
    switch (m) {
    case DRMSG_SEARCHING: return "Searching for a rival...";
    case DRMSG_SEARCHING_LEVEL: return "Searching for a rival of your level...";
    case DRMSG_FOUND: return "Rival found. Connecting...";
    case DRMSG_CONFIRMED: return "Result confirmed.";
    case DRMSG_ENDED_NO_POINTS: return "Search over. No points.";
    case DRMSG_WAIT_RIVAL: return "Waiting for your rival's result.";
    case DRMSG_DISPUTED: return "The two results differ: no points.";
    case DRMSG_VOID: return "Match void: no points.";
    case DRMSG_NO_ANSWER: return "Ranked did not answer. Trying again...";
    case DRMSG_REGISTER_FAILED: return "The Ranked profile could not be made.";
    case DRMSG_PROFILE_FAILED: return "The ranking could not be updated.";
    case DRMSG_ROOM_FAILED: return "The ranked room could not be published.";
    case DRMSG_FINISH_FIRST: return "Your last ranked match is still being confirmed.";
    case DRMSG_VERSION: return "The Dragon Net service does not take this version yet.";
    case DRMSG_UPDATED: return "Profile updated.";
    case DRMSG_BUSY: return "Too many requests: wait a moment.";
    case DRMSG_LIMIT: return "Server busy (daily limit), try again later.";
    }
    return "";
}

/* The service names the tiers in Spanish (Bronce .. Maestro): in English here, then translated as the others. */
static char *Tb_RkTier(const char *server) {
    static char other[24];

    if (strcmp(server, "Bronce") == 0) {
        return "Bronze";
    }
    if (strcmp(server, "Plata") == 0) {
        return "Silver";
    }
    if (strcmp(server, "Oro") == 0) {
        return "Gold";
    }
    if (strcmp(server, "Platino") == 0) {
        return "Platinum";
    }
    if (strcmp(server, "Diamante") == 0) {
        return "Diamond";
    }
    if (strcmp(server, "Maestro") == 0) {
        return "Master";
    }
    Ps_Copy(other, server, 24);
    return other;
}

static void Tb_RankedUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 side;

    Ps_Prompt(0, sTb->rkCur == RK_BATTLE ? "Change" : "Enter");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    Ps_Move(&sTb->rkCur, RK_COUNT);
    side = sTb->rkCur == RK_BATTLE ? Tb_Side() : 0;
    if (side != 0) {
        Snd_PlaySe(1, 0);
        sTb->rkMode = !sTb->rkMode;
        Port_SettingSave("tbo_rank_mode", sTb->rkMode);
        Port_SettingsWrite();
    } else if (pad->gamePressed & PADG_CROSS) {
        switch (sTb->rkCur) {
        case RK_SEARCH:
            if (!Tb_Vanilla()) {
                break;
            }
            Snd_PlaySe(1, 1);
            DrRanked_Search(Port_NetNameGet(), sTb->rkMode);
            sTb->rkLobby = 0;
            sTb->found = 0;
            sTb->waitFrom = sTb->frames;
            Tb_SetMode(TB_RKWAIT);
            break;
        case RK_BOARD:
            Snd_PlaySe(1, 1);
            DrRanked_Board();
            Tb_SetMode(TB_BOARD);
            break;
        case RK_UPDATE:
            Snd_PlaySe(1, 1);
            DrRanked_Open(Port_NetNameGet());
            break;
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_HUB);
    }
}

/* Searching: the service pairs two players; then the first opens the room of the two (its code goes to the service)
   and the other joins with that code, as a room's search (Tb_SearchNews: the rival comes, the match starts). */
static void Tb_RankedWaitUpdate(s32 live) {
    Pad *pad = &gPad[0];

    if (!sTb->found) {
        Ps_Prompt(4, "Cancel");
    }
    if (sTb->rkLobby) {
        if (sTbRk.role == 1 && Port_LobbyCode()[0] != 0) {
            DrRanked_SetRoom(Port_LobbyCode());
        }
        if (Tb_SearchNews()) {
            return;
        }
    } else if (sTbRk.state == 3) { /* paired */
        if (sTbRk.role == 1) {
            Port_NetNameSet(Port_NetNameGet());
            Port_NetOptions(Port_RollCan() ? 8 : 0, -2); /* rollback when the build has it, the delay from the line */
            Port_NetSpeedSet(Port_NetSpeedStandard());
            Port_NetRulesSet(sTbRk.mode ? 1 : 0, 0, 4); /* no time limit */
            Port_SettingsWrite();
            if (Port_LobbyStartCode(1, "") >= 0) {
                sTb->rkLobby = 1;
                sTb->host = 1;
            }
        } else if (sTbRk.role == 2 && Ps_Len(sTbRk.code) == CODE_LEN) {
            Port_NetNameSet(Port_NetNameGet());
            Port_NetOptions(-1, -1); /* joining: the host's choices arrive with its answer */
            if (Port_LobbyStartCode(0, sTbRk.code) >= 0) {
                sTb->rkLobby = 1;
                sTb->host = 0;
            }
        }
        if (sTb->rkLobby) {
            sTb->waitBack = TB_RANKED;
            sTb->ended = Port_NetSessionsEnded();
        }
    } else if (sTb->modeT > 30 && (sTbRk.state < 0 || (sTbRk.state == 0 && !sTbRk.searching))) { /* the search ended */
        Tb_SetMode(TB_RANKED);
        return;
    }
    if (!live || sTb->found) {
        return;
    }
    if (pad->gamePressed & (PADG_CIRCLE | PADG_TRIANGLE)) {
        Snd_PlaySe(1, 2);
        if (sTb->rkLobby) {
            Port_LobbyCancel();
        }
        sTb->rkLobby = 0;
        DrRanked_Cancel();
        Tb_SetMode(TB_RANKED);
    }
}

static void Tb_BoardUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Ps_Prompt(0, "Update");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    if (pad->gamePressed & PADG_CROSS) {
        Snd_PlaySe(1, 1);
        DrRanked_Board();
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_RANKED);
    }
}

/* ---------------------------------------------------------------------------------------------- the hub */
static void Tb_HubUpdate(s32 live) {
    Pad *pad = &gPad[0];

    s32 row = Tb_HubRow(sTb->hubCur), side;
    s32 small = sTb->hubCur == HUB_LANG || sTb->hubCur == HUB_VS;

    Ps_Prompt(0, small ? "Change" : "Enter");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    if (Ps_Move(&row, HUB_ROWS)) {
        sTb->hubCur = sTbHubOrder[row];
        small = sTb->hubCur == HUB_LANG || sTb->hubCur == HUB_VS;
    }
    side = small ? Tb_Side() : 0;
    if (side != 0 && sTb->hubCur == HUB_LANG) {
        Snd_PlaySe(1, 0);
        Tb_LangSet((sTb->lang + side + 3) % 3);
        Port_SettingSave("tbo_lang", sTb->lang);
        Port_SettingsWrite();
    } else if (side != 0) { /* against whom: the other service (its players online counted only on Dragon Net's) */
        s32 mode = 1 - DrNet_Mode();

        Snd_PlaySe(1, 0);
        DrNet_SetMode(mode);
        DrNet_PresenceActive(mode == 0);
        Port_LobbyHelloAsk();
    } else if (small) {
        if (pad->gamePressed & PADG_TRIANGLE) {
            Snd_PlaySe(1, 2);
            sTb->leaving = 1;
            ColorFade_StartOut(0, 0, 0, 0x14);
        }
    } else if (pad->gamePressed & PADG_CROSS) {
        switch (sTb->hubCur) {
        case HUB_RANKED:
            if (DrNet_Mode() != 0) {
                Snd_PlaySe(1, 2);
                Tb_Note("RANKED IS AGAINST ANDROID + SWITCH: CHOOSE IT IN PLAY AGAINST");
                break;
            }
            Snd_PlaySe(1, 1);
            DrRanked_Open(Port_NetNameGet());
            sTb->rkCur = RK_SEARCH;
            Tb_SetMode(TB_RANKED);
            break;
        case HUB_QUICK:
            Snd_PlaySe(1, 1);
            sTb->qkCur = 2;
            sTb->asked = sTb->frames - 100000;
            Tb_SetMode(TB_QUICK);
            break;
        case HUB_CREATE:
            Snd_PlaySe(1, 1);
            sTb->crCur = CR_OPEN;
            Tb_SetMode(TB_CREATE);
            break;
        case HUB_JOIN:
            Snd_PlaySe(1, 1);
            sTb->code[0] = 0;
            sTb->codeLen = 0;
            sTb->keyCol = 0;
            sTb->keyRow = 0;
            Tb_SetMode(TB_JOIN);
            break;
        case HUB_LOBBY:
            Tb_LobbyCreate();
            break;
        case HUB_PROFILE:
            Snd_PlaySe(1, 1);
            sTb->prCur = 0;
            Tb_SetMode(TB_PROFILE);
            break;
        default:
            Snd_PlaySe(1, 2);
            Tb_Note("NOT OPEN YET: IT NEEDS MORE FROM THE MATCHMAKING SERVICE THAN ROOM CODES");
            break;
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        sTb->leaving = 1;
        ColorFade_StartOut(0, 0, 0, 0x14);
    }
}

/* ---------------------------------------------------------------------------------------------- Create Room */
static void Tb_CreateUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 side;

    Ps_Prompt(0, sTb->crCur == CR_OPEN ? "Open Room" : "Change");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    Ps_Move(&sTb->crCur, CR_COUNT);
    side = sTb->crCur == CR_OPEN ? Ps_Side() : Tb_Side();
    if (side != 0) {
        switch (sTb->crCur) {
        case CR_BATTLE:
            sTb->battle = !sTb->battle;
            break;
        case CR_TIME:
            sTb->time = Ps_Wrap(sTb->time + side, 0, 4);
            break;
        case CR_DELAY:
            sTb->delay = Ps_Wrap(sTb->delay + side, -1, 6);
            break;
        case CR_ROLL:
            if (Port_RollCan()) {
                sTb->roll = Ps_Wrap(sTb->roll + side * 2, 0, 8);
            }
            break;
        }
        if (sTb->crCur != CR_OPEN) {
            Snd_PlaySe(1, 0);
        }
    }
    if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
        if (sTb->crCur == CR_OPEN || (pad->gamePressed & PADG_START)) {
            Snd_PlaySe(1, 1);
            Tb_BeginSearch(1);
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SaveChoices();
        Tb_SetMode(TB_HUB);
    }
}

/* ---------------------------------------------------------------------------------------------- the keypads */
/* A pad keypad: rows of nine keys and, under them, a row of wide buttons, each over a run of the columns. Up / down
   / left / right move, cross presses; circle switches the PC keyboard to typing and back. */
typedef struct TbKeyBtn {
    char *label;
    s32 c0;
    s32 c1;
    s32 act; /* TB_KEY_ */
} TbKeyBtn;
#define TB_KEY_DELETE 1
#define TB_KEY_OK 2
#define TB_KEY_CASE 3
#define TB_KEY_SPACE 4
#define TB_KEY_PASTE 5

static const TbKeyBtn sTbJoinBtn[3] = {{"DELETE", 0, 2, TB_KEY_DELETE}, {"PASTE", 3, 5, TB_KEY_PASTE}, {"JOIN", 6, 8, TB_KEY_OK}};
static const TbKeyBtn sTbNameBtn[4] = {{"Aa", 0, 1, TB_KEY_CASE}, {"SPACE", 2, 4, TB_KEY_SPACE}, {"DELETE", 5, 6, TB_KEY_DELETE}, {"DONE", 7, 8, TB_KEY_OK}};
#define NAME_ROWS 5
static char sTbNameKeys[2][NAME_ROWS][KEY_COLS + 1] = {{"ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789", "-_.!?#&*+"},
                                                     {"abcdefghi", "jklmnopqr", "stuvwxyz0", "123456789", "-_.!?#&*+"}};

static s32 Tb_BtnAt(const TbKeyBtn *b, s32 nb, s32 col) {
    s32 k;

    for (k = 0; k < nb; k++) {
        if (col >= b[k].c0 && col <= b[k].c1) {
            return k;
        }
    }
    return 0;
}

/* The cursor's moves. Returns 0, a key's letter, or -(TB_KEY_) for a button, when cross is pressed. */
static s32 Tb_PadUpdate(s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb) {
    Pad *pad = &gPad[0];

    if (sTb->keyRow == rows) { /* the buttons' row: a step goes to the next button */
        s32 k = Tb_BtnAt(b, nb, sTb->keyCol);

        if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
            sTb->keyCol = b[(k + 1) % nb].c0;
            Snd_PlaySe(1, 0);
        } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
            sTb->keyCol = b[(k + nb - 1) % nb].c0;
            Snd_PlaySe(1, 0);
        }
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        sTb->keyCol = (sTb->keyCol + 1) % KEY_COLS;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        sTb->keyCol = (sTb->keyCol + KEY_COLS - 1) % KEY_COLS;
        Snd_PlaySe(1, 0);
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sTb->keyRow = (sTb->keyRow + 1) % (rows + 1);
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sTb->keyRow = (sTb->keyRow + rows) % (rows + 1);
        Snd_PlaySe(1, 0);
    }
    if (sTb->keyRow == rows) {
        sTb->keyCol = b[Tb_BtnAt(b, nb, sTb->keyCol)].c0;
    }
    if (pad->gamePressed & PADG_CROSS) {
        return sTb->keyRow < rows ? (u8)keys[sTb->keyRow][sTb->keyCol] : -b[Tb_BtnAt(b, nb, sTb->keyCol)].act;
    }
    return 0;
}

/* Draws a keypad at (x, y): its keys, the cursor and the buttons' row. */
static void Tb_Keypad(f32 x, f32 y, s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb) {
    char one[2] = {0, 0};
    s32 r;
    s32 c;
    s32 k;
    f32 by = y + 34.0f * (f32)rows + 4.0f;

    for (r = 0; r < rows; r++) {
        for (c = 0; c < KEY_COLS; c++) {
            f32 kx = x + 36.0f * (f32)c;
            f32 ky = y + 34.0f * (f32)r;
            s32 on = r == sTb->keyRow && c == sTb->keyCol;

            if (on) {
                Ps_Pill(TBO_KIT_SEL, kx - 2.0f, ky - 2.0f, kx + 34.0f, ky + 30.0f, 0x80);
            } else {
                Ps_Pill(TBO_KIT_BAR, kx, ky, kx + 32.0f, ky + 28.0f, 0x80);
            }
            one[0] = keys[r][c];
            Ps_Align(1);
            Ps_Text(kx + 16.0f, Ps_TextTop(ky + 14.0f, 0.72f), one, 0.72f, 0.0f, on ? 1 : 0);
            Ps_Align(0);
        }
    }
    for (k = 0; k < nb; k++) {
        f32 x0 = x + 36.0f * (f32)b[k].c0;
        f32 x1 = x + 36.0f * (f32)b[k].c1 + 32.0f;
        s32 on = sTb->keyRow == rows && sTb->keyCol >= b[k].c0 && sTb->keyCol <= b[k].c1;

        /* (JOIN and DONE as the others: gold is the one the cursor is on) */
        Ps_Pill(on ? TBO_KIT_SEL : TBO_KIT_BAR, x0, by, x1, by + 28.0f, 0x80);
        Ps_Align(1);
        Ps_Text((x0 + x1) * 0.5f, Ps_TextTop(by + 14.0f, 0.6f), b[k].label, 0.6f, x1 - x0 - 12.0f, on ? 1 : 0);
        Ps_Align(0);
    }
}

/* ---------------------------------------------------------------------------------------------- Join Room */
static void Tb_CodeAdd(s32 c) {
    if (c >= 'a' && c <= 'z') {
        c -= 'a' - 'A';
    }
    if (((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) && sTb->codeLen < CODE_LEN) {
        sTb->code[sTb->codeLen++] = (char)c;
        sTb->code[sTb->codeLen] = 0;
        Snd_PlaySe(1, 0);
    }
}

static void Tb_CodeBack(void) {
    if (sTb->codeLen > 0) {
        sTb->code[--sTb->codeLen] = 0;
        Snd_PlaySe(1, 2);
    }
}

static void Tb_JoinTry(void) {
    if (sTb->codeLen == CODE_LEN) {
        Snd_PlaySe(1, 1);
        Tb_BeginSearch(0);
    } else {
        Snd_PlaySe(1, 2);
        Tb_Note("A ROOM CODE HAS SIX LETTERS AND NUMBERS");
    }
}

static void Tb_JoinUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 c;

    Ps_Prompt(0, "Enter");
    Ps_Prompt(1, "Delete");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    while (sTb->typing && (c = Port_TextRead()) != 0) { /* the PC keyboard: letters, backspace, enter, escape */
        if (c == 13) {
            Tb_JoinTry();
            return;
        } else if (c == 27) { /* Esc: back, as triangle */
            Snd_PlaySe(1, 2);
            Tb_SetMode(TB_HUB);
            return;
        } else if (c == 8) {
            Tb_CodeBack();
        } else {
            Tb_CodeAdd(c);
            if (sTb->codeLen == CODE_LEN) { /* full (typed or pasted): the cursor goes to JOIN */
                sTb->keyRow = KEY_ROWS;
                sTb->keyCol = sTbJoinBtn[2].c0;
            }
        }
    }
    c = Tb_PadUpdate(KEY_ROWS, sTbKeys, sTbJoinBtn, 3);
    if (c > 0) {
        Tb_CodeAdd(c);
        if (sTb->codeLen == CODE_LEN) { /* full: the cursor goes to JOIN */
            sTb->keyRow = KEY_ROWS;
            sTb->keyCol = sTbJoinBtn[2].c0;
        }
    } else if (c == -TB_KEY_PASTE) { /* the copied code in place of what is typed (it comes with the next frames) */
        sTb->codeLen = 0;
        sTb->code[0] = 0;
        Snd_PlaySe(1, 1);
        PortTbo_Paste();
    } else if (c == -TB_KEY_DELETE) {
        Tb_CodeBack();
    } else if (c == -TB_KEY_OK || (pad->gamePressed & PADG_START)) {
        Tb_JoinTry();
    } else if (pad->gamePressed & PADG_SQUARE) {
        Tb_CodeBack();
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_HUB);
    }
}

/* ---------------------------------------------------------------------------------------------- Profile */
static void Tb_ProfileUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Ps_Prompt(0, "Type Name");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    Ps_Move(&sTb->prCur, PR_COUNT);
    if ((pad->gamePressed & PADG_CROSS) && sTb->prCur == PR_NAME) {
        Snd_PlaySe(1, 1);
        Ps_Copy(sTb->typed, Port_NetNameGet(), 20);
        sTb->typedLen = Ps_Len(sTb->typed);
        Tb_SetMode(TB_NAME);
        sTb->keyRow = 0;
        sTb->keyCol = 0;
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SaveChoices();
        Tb_SetMode(TB_HUB);
    }
}

/* Whether a character is one of the name keypad's (or a space): what the keyboard types into a name too, so a name
   has only letters the game's font has, and the two ways of typing give the same names. */
static s32 Tb_NameKey(s32 c) {
    s32 r;
    s32 k;

    if (c == ' ') {
        return 1;
    }
    for (r = 0; r < NAME_ROWS; r++) {
        for (k = 0; k < KEY_COLS; k++) {
            if (sTbNameKeys[0][r][k] == c || sTbNameKeys[1][r][k] == c) {
                return 1;
            }
        }
    }
    return 0;
}

static void Tb_NameAdd(s32 c) {
    if (Tb_NameKey(c) && sTb->typedLen < 16) {
        sTb->typed[sTb->typedLen++] = (char)c;
        sTb->typed[sTb->typedLen] = 0;
        Snd_PlaySe(1, 0);
    }
}

static void Tb_NameBack(void) {
    if (sTb->typedLen > 0) {
        sTb->typed[--sTb->typedLen] = 0;
        Snd_PlaySe(1, 2);
    }
}

static void Tb_NameUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 c;
    s32 done = 0;

    Ps_Prompt(0, "Enter");
    Ps_Prompt(1, "Delete");
    Ps_Prompt(2, "Cancel");
    if (!live) {
        return;
    }
    while (sTb->typing && (c = Port_TextRead()) != 0) {
        if (c == 13) {
            done = 1;
        } else if (c == 27) { /* Esc: cancel, as triangle */
            done = -1;
            break;
        } else if (c == 8) {
            Tb_NameBack();
        } else {
            Tb_NameAdd(c);
        }
    }
    if (!done) {
        c = Tb_PadUpdate(NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
        if (c > 0) {
            Tb_NameAdd(c);
        } else if (c == -TB_KEY_SPACE) {
            Tb_NameAdd(' ');
        } else if (c == -TB_KEY_CASE) {
            sTb->lower = !sTb->lower;
            Snd_PlaySe(1, 0);
        } else if (c == -TB_KEY_DELETE || (pad->gamePressed & PADG_SQUARE)) {
            Tb_NameBack();
        } else if (c == -TB_KEY_OK || (pad->gamePressed & PADG_START)) {
            done = 1;
        } else if (pad->gamePressed & PADG_TRIANGLE) {
            done = -1;
        }
    }
    if (done == 1) {
        while (sTb->typedLen > 0 && sTb->typed[sTb->typedLen - 1] == ' ') {
            sTb->typed[--sTb->typedLen] = 0;
        }
        if (sTb->typedLen > 0) {
            Port_NetNameSave(sTb->typed);
            Tb_Note("SAVED: YOUR RIVALS WILL SEE THIS NAME");
        }
        Snd_PlaySe(1, 1);
        Tb_SetMode(TB_PROFILE);
    } else if (done == -1) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_PROFILE);
    }
}

/* ---------------------------------------------------------------------------------------------- what is drawn */
/* ---------------------------------------------------------------------------------------------- the pictures */
/* (how a screen draws: menu/port_screen.c) */


/* The left of the crest and the title: in line with the bars, but on the screen. */
static f32 Tb_TitleLeft(void) {
    return Ps_Left() + 14.0f; /* (by the window's left edge: on a wide phone the list keeps to the left, beside Raditz) */
}

/* The tournament grounds over the whole window: the middle of the 16:9 picture, as wide as the window is. */
static void Tb_Backdrop(void) {
    f32 aspect = (f32)Port_PageAspectMilli() / 1000.0f;
    f32 pic = Ps_PicW(TBO_BG) / Ps_PicH(TBO_BG);
    f32 u = aspect < pic ? aspect / pic : 1.0f;
    f32 v = aspect < pic ? 1.0f : pic / aspect;

    Ps_SpriteScreen(TBO_BG, 0.0f, 0.0f, 512.0f, 448.0f, 0.5f - u * 0.5f, 0.5f - v * 0.5f, 0.5f + u * 0.5f, 0.5f + v * 0.5f, PS_PLAIN, 0x80);
}

/* Raditz, from the waist up (a film: gs_draw.c), large, on the right, always in the same place: his waist at the foot of the screen,
   floating a little (a slow rise and fall, as the title's). `dx` moves him right (a page going off). */
#define TB_TALK_TOP 320.0f    /* the talk box (Tb_DrawTalk): its top edge, */
#define TB_TALK_BOTTOM 408.0f /* its bottom (the buttons under it, at 425), */
#define TB_TALK_WIDE 400.0f   /* its width, from the list's left */
#define TB_RADITZ_X 420.0f    /* his middle */
#define TB_RADITZ_TOP 40.0f   /* the top of his picture (his hair starts a little lower) */
static void Tb_RaditzBust(f32 dx) {
    f32 h = 452.0f - TB_RADITZ_TOP; /* (down to the foot of the screen, and a little under it: he floats up and down) */
    f32 w = h * Ps_PicW(TBO_RADITZ_BUST) / Ps_PicH(TBO_RADITZ_BUST) * 0.857f;
    f32 x = TB_RADITZ_X + dx;
    f32 y = TB_RADITZ_TOP + 3.0f * sinf((f32)sTb->frames * 0.04f);

    Ps_Sprite(TBO_RADITZ_BUST, x - w * 0.5f, y, x + w * 0.5f, y + h, 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80); /* (moving: raditz.mpg) */
}

/* The host of a page: Raditz, whatever the page asked for (the announcer's poses: hosts_*.png, kept for the PC's look).
   The pages' hosts stand on the box's line (feet 350): there he is the bust, in his one place; one page has him small
   in its window (Tb_DrawBlock). */
static void Tb_Hosts(s32 pose, f32 cx, f32 feet, f32 height) {
    (void)pose;
    if (feet >= 300.0f) {
        Tb_RaditzBust(0.0f);
    } else {
        Ps_Stand(TBO_RADITZ, cx, feet, height, 0x80);
    }
}

/* The Dragon Radar of the search: its glass, its grid, the sweeping line, a few dragon balls blinking. */
static void Tb_Radar(f32 cx, f32 cy, f32 r) {
    f32 rx = r * 0.857f;
    f32 sweep = (f32)sTb->frames * 0.06f;
    s32 k;

    Ps_Disc(cx, cy, r + 10.0f, Ps_Rgba(0xD8, 0xDC, 0xE4, 0x80), Ps_Rgba(0x70, 0x78, 0x88, 0x80)); /* the casing */
    Ps_Disc(cx, cy, r + 3.0f, Ps_Rgba(0x20, 0x30, 0x28, 0x80), Ps_Rgba(0x10, 0x18, 0x14, 0x80));
    Ps_Disc(cx, cy, r, Ps_Rgba(0x40, 0xC8, 0x60, 0x80), Ps_Rgba(0x10, 0x60, 0x28, 0x80));    /* the glass */
    for (k = -3; k <= 3; k++) {                                                                  /* the grid */
        f32 t = (f32)k / 3.5f;
        f32 half = sqrtf(1.0f - t * t);

        Ps_Rect(cx + t * rx - 0.6f, cy - half * r, cx + t * rx + 0.6f, cy + half * r, Ps_Rgba(0x90, 0xFF, 0xA0, 0x50), Ps_Rgba(0x90, 0xFF, 0xA0, 0x50));
        Ps_Rect(cx - half * rx, cy + t * r - 0.6f, cx + half * rx, cy + t * r + 0.6f, Ps_Rgba(0x90, 0xFF, 0xA0, 0x50), Ps_Rgba(0x90, 0xFF, 0xA0, 0x50));
    }
    for (k = 0; k < 10; k++) { /* the sweep: a wedge that fades behind the line */
        f32 a0 = sweep - (f32)(k + 1) * 0.07f;
        f32 a1 = sweep - (f32)k * 0.07f;
        s32 al = 0x60 - k * 6;

        Ps_Quad(cx, cy, cx, cy, cx + cosf(a0) * rx, cy + sinf(a0) * r, cx + cosf(a1) * rx, cy + sinf(a1) * r, Ps_Rgba(0xD0, 0xFF, 0xC0, al),
                Ps_Rgba(0xD0, 0xFF, 0xC0, al), Ps_Rgba(0xD0, 0xFF, 0xC0, al / 2), Ps_Rgba(0xD0, 0xFF, 0xC0, al / 2));
    }
    {
        static const f32 ball[5][2] = {{0.35f, -0.4f}, {-0.5f, 0.2f}, {0.15f, 0.55f}, {-0.2f, -0.65f}, {0.62f, 0.25f}};

        for (k = 0; k < 5; k++) {
            f32 bx = cx + ball[k][0] * rx;
            f32 by = cy + ball[k][1] * r;
            s32 al = ((sTb->frames + k * 17) / 20) % 3 == 0 ? 0x40 : 0x80;

            Ps_Disc(bx, by, 4.0f, Ps_Rgba(0xFF, 0xF8, 0x80, al), Ps_Rgba(0xFF, 0xA0, 0x20, al));
        }
    }
    Ps_Disc(cx, cy, 3.0f, Ps_Rgba(0xFF, 0x50, 0x40, 0x80), Ps_Rgba(0xA0, 0x10, 0x10, 0x80)); /* you */
}

/* The hub: two small rows under the title (the language, against whom), then the list, a plate each, large enough
   for a phone's screen. The plates lean like the main menu's (kit_bar, kit_sel: the slant is in their ends); the one
   under the cursor is longer, gold, and glides to where the cursor is. */
#define TB_HUB_SMALL_Y 50.0f  /* the first small row, under the title */
#define TB_HUB_SMALL_H 22.0f  /* a small row's height; they are 26 apart */
#define TB_HUB_LIST_Y 104.0f  /* the first plate of the list */
#define TB_HUB_PITCH 35.0f    /* from plate to plate: the last one ends above the talk box (TB_TALK_TOP) */
#define TB_HUB_PLATE 31.0f    /* a plate's height */
#define TB_HUB_LONG 300.0f    /* how long a plate is (from the list's left); under the cursor: longer */
#define TB_HUB_SHORT 256.0f   /* ... a small row */
static f32 Tb_HubRowY(s32 row) {
    return row < HUB_SMALL ? TB_HUB_SMALL_Y + 26.0f * (f32)row : TB_HUB_LIST_Y + TB_HUB_PITCH * (f32)(row - HUB_SMALL);
}

static f32 Tb_HubRowH(s32 row) {
    return row < HUB_SMALL ? TB_HUB_SMALL_H : TB_HUB_PLATE;
}

static void Tb_DrawHub(void) {
    s32 i, cur = Tb_HubRow(sTb->hubCur);
    f32 slide = (1.0f - Tb_In(0)) * -160.0f;
    f32 left = Tb_TitleLeft() - 8.0f + slide;

    Tb_RaditzBust(0.0f);
    sTb->hubY += (Tb_HubRowY(cur) - sTb->hubY) * 0.6f; /* (quick: the gold plate is where the cursor is in a few frames) */
    for (i = 0; i < HUB_ROWS; i++) {
        f32 y = Tb_HubRowY(i);
        f32 end = left + (i < HUB_SMALL ? TB_HUB_SHORT : TB_HUB_LONG - 4.0f * (f32)(i - HUB_SMALL)) - slide;

        if (i != cur) {
            Ps_Pill(TBO_KIT_BAR, left + 12.0f, y, end + slide, y + Tb_HubRowH(i), 0x80);
        }
    }
    {   /* the cursor's plate, gliding between the rows (its height follows: the small rows are lower) */
        f32 h = Tb_HubRowH(cur);
        f32 end = left + (cur < HUB_SMALL ? TB_HUB_SHORT + 20.0f : TB_HUB_LONG + 26.0f) - slide;

        Ps_Pill(TBO_KIT_SEL, left, sTb->hubY - 2.0f, end + slide, sTb->hubY + h + 2.0f, 0x80);
        if (cur < HUB_SMALL) {
            Ps_Button(3, left + 266.0f, sTb->hubY + h * 0.5f, 18.0f);
        }
    }
}

static void Tb_DrawHubText(void) {
    s32 i, cur = Tb_HubRow(sTb->hubCur);
    f32 slide = (1.0f - Tb_In(0)) * -160.0f;
    f32 x = Tb_TitleLeft() + 38.0f + slide;

    for (i = 0; i < HUB_ROWS; i++) {
        s32 item = sTbHubOrder[i];
        f32 y = i == cur ? sTb->hubY : Tb_HubRowY(i);
        f32 mid = y + Tb_HubRowH(i) * 0.5f;

        if (i < HUB_SMALL) { /* "Language   PORTUGUES", "Play Against   ANDROID + SWITCH": the label, then the choice */
            char *value = sTbVsName[DrNet_Mode() != 0];

            if (item == HUB_LANG) { /* (not `?:` between two tables' entries: the 32-bit pointers) */
                value = sTbLangName[sTb->lang];
            }
            Ps_Text(x - 10.0f, Ps_TextTop(mid, 0.62f), sTbHubName[item], 0.62f, 96.0f, i == cur ? 1 : 0);
            Ps_Align(1);
            Ps_Text(x + 152.0f, Ps_TextTop(mid, 0.66f), value, 0.66f, 112.0f, i == cur ? 1 : 3);
            Ps_Align(0);
        } else if (i == cur) {
            Ps_Text(x, Ps_TextTop(mid, 1.12f), sTbHubName[item], 1.12f, 230.0f, 1);
        } else {
            Ps_Text(x + 12.0f, Ps_TextTop(mid, 0.96f), sTbHubName[item], 0.96f, 200.0f, item == HUB_RANKED && DrNet_Mode() != 0 ? 4 : 0);
        }
    }
}

/* rows of label / value, as Create Room and Profile have them: TB_ROW apart, TB_ROW_H high (a finger's size) */
#define TB_ROW 42.0f
#define TB_ROW_H 36.0f
#define TB_ROW_LABEL 0.9f /* the label's scale, */
#define TB_ROW_VALUE 0.72f /* the value's */
static void Tb_DrawRows(s32 count, s32 cur, f32 y0) {
    s32 i;

    Ps_Panel(TBO_KIT_WINDOW, 8.0f, y0 - 12.0f, 306.0f, y0 + TB_ROW * (f32)count + 4.0f, 0x80);
    for (i = 0; i < count; i++) {
        f32 y = y0 + TB_ROW * (f32)i;

        if (i == cur) {
            Ps_Pill(TBO_KIT_SEL, 14.0f, y - 2.0f, 300.0f, y + TB_ROW_H + 2.0f, 0x80);
            Ps_Button(3, 282.0f, y + TB_ROW_H * 0.5f, 22.0f);
        } else {
            Ps_Pill(TBO_KIT_BAR, 18.0f, y, 296.0f, y + TB_ROW_H, 0x80);
        }
    }
}

static void Tb_DrawCreate(void) {
    Tb_Hosts(TBO_HOSTS_ARMS_CROSSED, 408.0f, 350.0f, 250.0f);
    Tb_DrawRows(CR_COUNT, sTb->crCur, 100.0f);
}

static void Tb_DrawCreateText(void) {
    static char value[CR_COUNT][32];
    static char *label[CR_COUNT] = {"Battle", "Time Limit", "Input Delay", "Rollback", "Open Room"};
    s32 i;

    sprintf(value[CR_BATTLE], sTb->battle ? "TEAMS (UP TO 5)" : "ONE ON ONE");
    Ps_Copy(value[CR_TIME], sTbTime[sTb->time], 32);
    Tb_DelayText(value[CR_DELAY], sTb->delay);
    Tb_RollText(value[CR_ROLL], sTb->roll);
    value[CR_OPEN][0] = 0;
    Ps_Align(1);
    Ps_Text(157.0f, Ps_TextTop(76.0f, 0.7f), "ROOM SETTINGS", 0.7f, 0.0f, 2);
    Ps_Align(0);
    for (i = 0; i < CR_COUNT; i++) {
        f32 y = 100.0f + TB_ROW * (f32)i + TB_ROW_H * 0.5f;

        Ps_Text(36.0f, Ps_TextTop(y, TB_ROW_LABEL), label[i], TB_ROW_LABEL, 110.0f, i == sTb->crCur ? 1 : 0);
        Ps_Align(2);
        Ps_Text(266.0f, Ps_TextTop(y, TB_ROW_VALUE), value[i], TB_ROW_VALUE, 120.0f, i == sTb->crCur ? 1 : 3);
        Ps_Align(0);
    }
}

/* Ranked: the profile's window (the tier, the points, wins and losses, the place), then the rows. */
#define TB_RK_ROWS_Y 146.0f
static void Tb_DrawRanked(void) {
    Tb_RaditzBust(0.0f);
    Ps_Panel(TBO_KIT_WINDOW, 8.0f, 60.0f, 306.0f, 126.0f, 0x80);
    Tb_DrawRows(RK_COUNT, sTb->rkCur, TB_RK_ROWS_Y);
}

static void Tb_DrawRankedText(void) {
    static char line[64];
    static char *label[RK_COUNT] = {"Battle", "Search for a Rival", "Ranking", "Update Profile"};
    s32 i;

    if (!sTbRk.profile) {
        Ps_Text(26.0f, Ps_TextTop(93.0f, 0.8f), "Loading profile...", 0.8f, 270.0f, 0);
    } else {
        Ps_Text(26.0f, Ps_TextTop(80.0f, 0.86f), Tb_RkTier(sTbRk.rank), 0.86f, 150.0f, 2);
        sprintf(line, "%d ELO", sTbRk.rating);
        Ps_Align(2);
        Ps_Text(290.0f, Ps_TextTop(80.0f, 0.86f), line, 0.86f, 120.0f, 3);
        Ps_Align(0);
        sprintf(line, Tb_Fmt("%d W / %d L"), sTbRk.wins, sTbRk.losses);
        Ps_Text(26.0f, Ps_TextTop(108.0f, 0.66f), line, 0.66f, 150.0f, 0);
        sprintf(line, Tb_Fmt("Position %d"), sTbRk.position);
        Ps_Align(2);
        Ps_Text(290.0f, Ps_TextTop(108.0f, 0.66f), line, 0.66f, 120.0f, 0);
        Ps_Align(0);
    }
    for (i = 0; i < RK_COUNT; i++) {
        f32 y = TB_RK_ROWS_Y + TB_ROW * (f32)i + TB_ROW_H * 0.5f;

        Ps_Text(36.0f, Ps_TextTop(y, TB_ROW_LABEL), label[i], TB_ROW_LABEL, 170.0f, i == sTb->rkCur ? 1 : 0);
        if (i == RK_BATTLE) {
            Ps_Align(2);
            Ps_Text(266.0f, Ps_TextTop(y, TB_ROW_VALUE), sTb->rkMode ? "TEAM" : "SINGLE", TB_ROW_VALUE, 90.0f, i == sTb->rkCur ? 1 : 3);
            Ps_Align(0);
        }
    }
}

/* Ranked's search: the radar, and a window with the battle and what the service says. */
static void Tb_DrawRankedWait(void) {
    Tb_RaditzBust(0.0f);
    Tb_Radar(264.0f, 188.0f, 80.0f);
    Ps_Panel(TBO_KIT_WINDOW, 10.0f, 84.0f, 180.0f, 246.0f, 0x80);
}

static void Tb_DrawRankedWaitText(void) {
    static char line[48];
    s32 secs = (sTb->frames - sTb->waitFrom) / 60;

    Ps_Text(22.0f, 94.0f, "RANKED", 0.56f, 0.0f, 0);
    Ps_Text(22.0f, 116.0f, sTb->rkMode ? "TEAM" : "SINGLE", 0.8f, 150.0f, 2);
    if (sTbRk.state == 3 && sTbRk.peer[0] != 0) {
        Ps_Text(22.0f, 150.0f, "RIVAL", 0.56f, 0.0f, 0);
        Ps_Text(22.0f, 170.0f, sTbRk.peer, 0.8f, 150.0f, 3);
    }
    sprintf(line, "%d:%02d", (int)(secs / 60), (int)(secs % 60));
    Ps_Text(22.0f, 214.0f, line, 0.8f, 150.0f, 0);
}

/* Ranked's table: the nine best, their points. */
#define TB_BOARD_Y 66.0f
#define TB_BOARD_ROW 27.0f
static void Tb_DrawBoard(void) {
    s32 i;

    Tb_RaditzBust(0.0f);
    Ps_Panel(TBO_KIT_WINDOW, 8.0f, TB_BOARD_Y - 8.0f, 306.0f, TB_BOARD_Y + TB_BOARD_ROW * 9.0f + 6.0f, 0x80);
    for (i = 0; i < 9; i++) {
        f32 y = TB_BOARD_Y + TB_BOARD_ROW * (f32)i;

        Ps_Pill(TBO_KIT_BAR, 18.0f, y, 296.0f, y + TB_BOARD_ROW - 3.0f, i % 2 ? 0x60 : 0x80);
    }
}

static void Tb_DrawBoardText(void) {
    static char line[48];
    s32 i;

    if (sTbRk.board != 2) {
        Ps_Text(30.0f, Ps_TextTop(TB_BOARD_Y + TB_BOARD_ROW * 0.5f - 1.5f, 0.7f), sTbRk.board == 1 ? "Updating the ranking..." : "The ranking did not answer: X tries again.",
                0.7f, 260.0f, 0);
        return;
    }
    for (i = 0; i < 9 && i < sTbRk.boardCount; i++) {
        f32 mid = TB_BOARD_Y + TB_BOARD_ROW * (f32)i + TB_BOARD_ROW * 0.5f - 1.5f;

        sprintf(line, "%d.", i + 1);
        Ps_Text(30.0f, Ps_TextTop(mid, 0.7f), line, 0.7f, 30.0f, i == 0 ? 2 : 0);
        Ps_Text(62.0f, Ps_TextTop(mid, 0.7f), sTbRk.boardName[i], 0.7f, 150.0f, i == 0 ? 2 : 0);
        sprintf(line, "%d", sTbRk.boardRating[i]);
        Ps_Align(2);
        Ps_Text(284.0f, Ps_TextTop(mid, 0.7f), line, 0.7f, 70.0f, 3);
        Ps_Align(0);
    }
}

static void Tb_DrawJoin(void) {
    s32 c;

    Tb_Hosts(TBO_HOSTS_PUZZLED, 432.0f, 350.0f, 200.0f);
    for (c = 0; c < CODE_LEN; c++) { /* the code's six boxes */
        f32 x = 34.0f + 50.0f * (f32)c;

        Ps_Pill(c == sTb->codeLen ? TBO_KIT_SEL : TBO_KIT_BAR, x, 76.0f, x + 44.0f, 116.0f, 0x80);
    }
    Ps_Panel(TBO_KIT_WINDOW, 14.0f, 126.0f, 352.0f, 330.0f, 0x80);
}

static void Tb_DrawJoinText(void) {
    char one[2] = {0, 0};
    s32 c;

    Ps_Align(1);
    for (c = 0; c < CODE_LEN; c++) {
        f32 x = 56.0f + 50.0f * (f32)c;

        one[0] = c < sTb->codeLen ? sTb->code[c] : (c == sTb->codeLen && (sTb->frames / 16) & 1 ? '_' : ' ');
        Ps_Text(x, 81.0f, one, 1.0f, 0.0f, c < sTb->codeLen ? 3 : 0); /* (what is typed: a value) */
    }
    Ps_Align(0);
    Tb_Keypad(24.0f, 138.0f, KEY_ROWS, sTbKeys, sTbJoinBtn, 3);
}

static void Tb_DrawWait(void) {
    if (sTb->found) {
        Tb_Hosts(TBO_HOSTS_CHEER, 426.0f, 350.0f, 180.0f);
    } else {
        Tb_Hosts(TBO_HOSTS_BINOCULARS, 434.0f, 350.0f, 210.0f);
    }
    Tb_Radar(264.0f, 188.0f, 80.0f);
    Ps_Panel(TBO_KIT_WINDOW, 10.0f, 84.0f, 180.0f, 246.0f, 0x80);
    Ps_Panel(TBO_KIT_WINDOW, 176.0f, 292.0f, 348.0f, 328.0f, 0x80); /* the search's clock (a window, not a bar to choose) */
}

/* A name in capitals, as these pages write theirs (the letters A to Z only: a translation's others stay). */
static void Tb_Upper(char *out, const char *text, s32 size) {
    s32 i;

    for (i = 0; text[i] != 0 && i < size - 1; i++) {
        out[i] = text[i] >= 'a' && text[i] <= 'z' ? (char)(text[i] - 'a' + 'A') : text[i];
    }
    out[i] = 0;
}

static void Tb_DrawWaitText(void) {
    static char line[96], region[64];
    s32 secs = (sTb->frames - sTb->waitFrom) / 60;
    const char *code = Port_LobbyCode();

    if (sTb->host == 2) {
        s32 n = Port_LobbyQueueWaiting(sTb->queue);

        Ps_Text(22.0f, 94.0f, "QUICK MATCH", 0.56f, 0.0f, 0);
        Ps_Align(1);
        Ps_Text(95.0f, 124.0f, sTb->queue ? "TEAMS" : "ONE ON ONE", 0.86f, 156.0f, 1);
        Ps_Align(0);
        s32 widen = Port_LobbyWiden(), y = 214;
        const char *from = Port_LobbyRivalRegion();

        if (sTb->found && from[0] != 0) { /* where the matchmaking service placed the rival */
            Tb_Upper(region, from, sizeof(region));
            Ps_Text(22.0f, 172.0f, "YOUR RIVAL IS FROM", 0.46f, 150.0f, 0);
            Ps_Text(22.0f, 190.0f, region, 0.46f, 150.0f, 5);
        } else if (sTb->region > 0 && sTb->region < QK_REGIONS - 1 && widen > 0 && secs >= widen) {
            Ps_Text(22.0f, 172.0f, "NO ONE THERE:", 0.46f, 150.0f, 0); /* (a region's search takes everyone now) */
            Ps_Text(22.0f, 190.0f, "SEARCHING EVERYWHERE", 0.46f, 150.0f, 0);
        } else if (sTb->region > 0 && sTb->region < QK_REGIONS - 1) {
            Ps_Text(22.0f, 172.0f, "PLAYERS IN", 0.46f, 150.0f, 0);
            Ps_Text(22.0f, 190.0f, sTbRegionName[sTb->region], 0.46f, 150.0f, 5);
            sprintf(line, Tb_Fmt("FIRST, EVERYONE AFTER %d S"), (int)(widen > 0 ? widen : QK_REGION_WIDEN));
            Ps_Text(22.0f, 210.0f, line, 0.4f, 150.0f, 3);
            y = 230;
        } else if (sTb->region == 0 && widen > 0 && secs >= widen) {
            Ps_Text(22.0f, 172.0f, "NO ONE NEARBY:", 0.46f, 150.0f, 0);
            Ps_Text(22.0f, 190.0f, "SEARCHING EVERYWHERE", 0.46f, 150.0f, 0);
        } else if (sTb->region == 0 && widen > 0) {
            Ps_Text(22.0f, 172.0f, "LOOKING FOR A RIVAL", 0.46f, 150.0f, 0);
            Ps_Text(22.0f, 190.0f, "NEAR YOU FIRST", 0.46f, 150.0f, 0);
        } else {
            Ps_Text(22.0f, 172.0f, "THE SERVICE PAIRS YOU", 0.46f, 150.0f, 0);
            Ps_Text(22.0f, 190.0f, "WITH THE NEXT RIVAL", 0.46f, 150.0f, 0);
        }
        if (n > 0) {
            sprintf(line, Tb_Fmt("%d IN THIS QUEUE"), (int)n);
            Ps_Text(22.0f, (f32)y, line, 0.42f, 150.0f, 5);
        }
    } else if (sTb->host) {
        Ps_Text(22.0f, 94.0f, "ROOM CODE", 0.56f, 0.0f, 0);
        Ps_Align(1);
        if (code[0] != 0) {
            Ps_Text(95.0f, 120.0f, (char *)code, 1.2f, 156.0f, 0);
        } else {
            Ps_Text(95.0f, 130.0f, "GETTING A CODE...", 0.5f, 156.0f, 0);
        }
        Ps_Align(0);
        Ps_Text(22.0f, 172.0f, code[0] != 0 ? "GIVE THIS CODE TO" : "ASKING THE", 0.46f, 150.0f, 0);
        Ps_Text(22.0f, 190.0f, code[0] != 0 ? "YOUR RIVAL" : "MATCHMAKING SERVICE", 0.46f, 150.0f, 0);
        if (Port_LobbyOther()) {
            Ps_Text(22.0f, 214.0f, "ANOTHER VERSION TRIED", 0.42f, 150.0f, 3);
        }
    } else {
        Ps_Text(22.0f, 94.0f, "JOINING ROOM", 0.56f, 0.0f, 0);
        Ps_Align(1);
        Ps_Text(95.0f, 120.0f, sTb->code, 1.2f, 156.0f, 0);
        Ps_Align(0);
        Ps_Text(22.0f, 172.0f, "LOOKING FOR", 0.46f, 150.0f, 0);
        Ps_Text(22.0f, 190.0f, "THE HOST...", 0.46f, 150.0f, 0);
    }
    Ps_Align(1);
    if (sTb->found) {
        Ps_Text(262.0f, 302.0f, "YOUR RIVAL IS HERE!", 0.56f, 164.0f, 5);
    } else {
        sprintf(line, Tb_Fmt("SEARCHING...  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
        Ps_Text(262.0f, 302.0f, line, 0.52f, 164.0f, 0);
    }
    Ps_Align(0);
}

static void Tb_DrawBlock(void) {
    Ps_Panel(TBO_KIT_WINDOW, 60.0f, 96.0f, 452.0f, 316.0f, 0x80);
    Tb_Hosts(TBO_HOSTS_PUZZLED, 404.0f, 120.0f, 92.0f);
    Ps_Pill(TBO_KIT_GOLD, 206.0f, 282.0f, 306.0f, 306.0f, 0x80);
}

static void Tb_DrawBlockText(void) {
    static char parts[96];
    const char *t = Port_DataFingerprintText();
    const char *m = t;
    s32 n;

    Ps_Heading(80.0f, 108.0f, "Cannot Go Online", 0.9f, 250.0f, 0x80);
    Ps_Msg(80.0f, 146.0f, "Online matches are only between", 1.0f, 0x80);
    Ps_Msg(80.0f, 166.0f, "two unmodded games.", 1.0f, 0x80);
    while (*m != 0 && strncmp(m, "modified:", 9) != 0) {
        m++;
    }
    if (*m != 0) { /* which parts differ, as the data check says it */
        Ps_Copy(parts, "Changed: ", 96);
        Ps_Cat(parts, m + 10, 9 + 81);
    } else if (Port_DataFingerprint(NULL) == 1) {
        sprintf(parts, "Your game data is not the disc's.");
    } else {
        sprintf(parts, "Your game data could not be checked.");
    }
    for (n = 0; parts[n] != 0; n++) {
        if (parts[n] == ',') {
            parts[n] = ' ';
        }
    }
    Ps_Msg(80.0f, 196.0f, parts, 1.0f, 0x80);
    Ps_Msg(80.0f, 226.0f, "Take out what you added in gamedata/mods.", 1.0f, 0x80);
    Ps_Msg(80.0f, 246.0f, "(Fighter mods switch off by themselves online.)", 1.0f, 0x70);
    Ps_Align(1);
    Ps_Text(256.0f, 285.0f, "BACK", 0.56f, 0.0f, 1);
    Ps_Align(0);
}

/* The name's window: over everything else on the screen (drawn last). */
static void Tb_DrawName(void) {
    static char line[40];

    Ps_Panel(TBO_KIT_WINDOW, 64.0f, 62.0f, 448.0f, 340.0f, 0x80);
    Ps_Text(84.0f, 70.0f, "TYPE YOUR NAME", 0.6f, 0.0f, 2);
    Ps_Pill(TBO_KIT_BAR, 84.0f, 94.0f, 428.0f, 124.0f, 0x80);
    Ps_Copy(line, sTb->typed, 40);
    Ps_Cat(line, (sTb->frames / 16) & 1 ? "_" : " ", 40);
    Ps_Text(100.0f, 97.0f, line, 0.86f, 310.0f, 1);
    Tb_Keypad(94.0f, 134.0f, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
}

static void Tb_DrawSpar(void) {
    Tb_Hosts(TBO_HOSTS_CHEER, 406.0f, 350.0f, 200.0f);
    Tb_DrawRows(3, sTb->spCur, 96.0f);
    Ps_Panel(TBO_KIT_WINDOW, 8.0f, 232.0f, 306.0f, 326.0f, 0x80);
}

static void Tb_DrawSparText(void) {
    static char line[64];
    s32 secs = (sTb->frames - sTb->waitFrom) / 60;
#define SP_MID(i) (96.0f + TB_ROW * (f32)(i) + TB_ROW_H * 0.5f)

    Ps_Text(22.0f, Ps_TextTop(70.0f, 0.7f), "SPARRING AT THE TOURNAMENT", 0.7f, 280.0f, 2);
    Ps_Text(36.0f, Ps_TextTop(SP_MID(0), TB_ROW_LABEL), "You", TB_ROW_LABEL, 70.0f, sTb->spCur == 0 ? 1 : 0);
    Ps_Text(36.0f, Ps_TextTop(SP_MID(1), TB_ROW_LABEL), "Partner", TB_ROW_LABEL, 90.0f, sTb->spCur == 1 ? 1 : 0);
    Ps_Text(36.0f, Ps_TextTop(SP_MID(2), TB_ROW_LABEL), "Start Training", TB_ROW_LABEL, 220.0f, sTb->spCur == 2 ? 1 : 0);
    Ps_Align(2);
    Ps_Text(266.0f, Ps_TextTop(SP_MID(0), TB_ROW_VALUE), Tb_Fighter(sTb->spMe), TB_ROW_VALUE, 160.0f, sTb->spCur == 0 ? 1 : 3);
    Ps_Text(266.0f, Ps_TextTop(SP_MID(1), TB_ROW_VALUE), Tb_Fighter(sTb->spFoe), TB_ROW_VALUE, 160.0f, sTb->spCur == 1 ? 1 : 3);
    Ps_Align(0);
#undef SP_MID
    Ps_Text(22.0f, Ps_TextTop(250.0f, 0.62f), sTb->host == 2 ? "QUICK MATCH SEARCH" : sTb->host ? "YOUR ROOM IS OPEN" : "JOINING A ROOM", 0.62f, 276.0f, 2);
    sprintf(line, Tb_Fmt("SEARCHING  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
    Ps_Text(22.0f, Ps_TextTop(274.0f, 0.66f), line, 0.66f, 276.0f, 5);
    Ps_Text(22.0f, Ps_TextTop(298.0f, 0.5f), "THE MATCH STARTS THE MOMENT", 0.5f, 276.0f, 0);
    Ps_Text(22.0f, Ps_TextTop(314.0f, 0.5f), "YOUR RIVAL ARRIVES", 0.5f, 276.0f, 0);
}

static void Tb_DrawQuick(void) {
    Tb_Hosts(TBO_HOSTS_MIC_WAVE, 404.0f, 350.0f, 240.0f);
    Tb_DrawRows(3, sTb->qkCur, 96.0f);
    Ps_Panel(TBO_KIT_WINDOW, 8.0f, 232.0f, 306.0f, 330.0f, 0x80);
}

static void Tb_DrawQuickText(void) {
    static char line[128];
    s32 q;

#define QK_ROW(i) Ps_TextTop(96.0f + TB_ROW * (f32)(i) + TB_ROW_H * 0.5f, TB_ROW_LABEL)
#define QK_VAL(i) Ps_TextTop(96.0f + TB_ROW * (f32)(i) + TB_ROW_H * 0.5f, TB_ROW_VALUE)
    Ps_Text(22.0f, Ps_TextTop(70.0f, 0.7f), "PICK YOUR BATTLE", 0.7f, 0.0f, 2);
    Ps_Text(36.0f, QK_ROW(0), "Battle", TB_ROW_LABEL, 100.0f, sTb->qkCur == 0 ? 1 : 0);
    Ps_Align(2);
    Ps_Text(266.0f, QK_VAL(0), sTb->queue ? "TEAMS (UP TO 5)" : "ONE ON ONE", TB_ROW_VALUE, 140.0f, sTb->qkCur == 0 ? 1 : 3);
    Ps_Align(0);
    Ps_Text(36.0f, QK_ROW(1), "Region", TB_ROW_LABEL, 100.0f, sTb->qkCur == 1 ? 1 : 0);
    Ps_Align(2);
    Ps_Text(266.0f, QK_VAL(1), sTbRegionName[sTb->region], TB_ROW_VALUE, 150.0f, sTb->qkCur == 1 ? 1 : 3);
    Ps_Align(0);
    Ps_Text(36.0f, QK_ROW(2), "Search", TB_ROW_LABEL, 100.0f, sTb->qkCur == 2 ? 1 : 0);
    Ps_Text(22.0f, Ps_TextTop(250.0f, 0.62f), "WAITING RIGHT NOW", 0.62f, 0.0f, 2);
    for (q = 0; q < 2; q++) {
        s32 n = Port_LobbyQueueWaiting(q);

        Ps_Text(36.0f, Ps_TextTop(272.0f + 20.0f * (f32)q, 0.66f), q ? "Teams" : "One on One", 0.66f, 130.0f, q == sTb->queue ? 1 : 0);
        if (n < 0) {
            sprintf(line, "...");
        } else {
            sprintf(line, Tb_Fmt("%d WAITING"), (int)n);
        }
        Ps_Align(2);
        Ps_Text(290.0f, Ps_TextTop(272.0f + 20.0f * (f32)q, 0.6f), line, 0.6f, 120.0f, n > 0 ? 5 : 3);
        Ps_Align(0);
    }
    /* (the queues count everyone; what the region choice means instead of the rules, which the hosts' line says) */
    sprintf(line, Tb_Fmt("PLAYERS THERE FIRST, EVERYONE AFTER %d S"), (int)QK_REGION_WIDEN);
    Ps_Text(22.0f, Ps_TextTop(312.0f, 0.5f), sTb->region == 0 ? "RIVALS NEAR YOU FIRST, THEN ANYWHERE" : sTb->region == QK_REGIONS - 1 ? "THE NEXT RIVAL, WHEREVER: THE FASTEST" : line,
            0.5f, 276.0f, 3);
#undef QK_ROW
#undef QK_VAL
}

static void Tb_DrawProfile(void) {
    Tb_Hosts(TBO_HOSTS_THUMBS_UP, 408.0f, 350.0f, 250.0f);
    Tb_DrawRows(PR_COUNT, sTb->prCur, 110.0f);
}

static void Tb_DrawProfileText(void) {
    static char value[PR_COUNT][32];
    static char *label[PR_COUNT] = {"Name"};
    s32 i;

    Ps_Copy(value[PR_NAME], Port_NetNameGet(), 32);
    Ps_Text(22.0f, Ps_TextTop(80.0f, 0.7f), "PLAYER CARD", 0.7f, 0.0f, 2);
    for (i = 0; i < PR_COUNT; i++) {
        f32 y = 110.0f + TB_ROW * (f32)i + TB_ROW_H * 0.5f;

        Ps_Text(36.0f, Ps_TextTop(y, TB_ROW_LABEL), label[i], TB_ROW_LABEL, 100.0f, i == sTb->prCur ? 1 : 0);
        Ps_Align(2);
        Ps_Text(266.0f, Ps_TextTop(y, TB_ROW_VALUE), value[i], TB_ROW_VALUE, 140.0f, i == sTb->prCur ? 1 : 3);
        Ps_Align(0);
    }
}

/* ---------------------------------------------------------------------------------------------- the lobby */
#define TB_BOX_LINE 224 /* (a line of the box, in the game's 16-bit letters) */
/* A lobby of up to eight (port/src/gs/net_lobby.c; docs/port/README.md, "Lobbies"): the owner's game keeps it, this
   page shows the copy every member has (Port_LobbyView) and leaves its wishes with Port_LobbyDo. On the left the
   eight places (1P and 2P play, the six others watch), on the right the code, the rules and the buttons; the box at
   the foot of the screen has what happened last (who came and went, the matches, the chat). The owner moves people
   (a name, Move, then the place: the two swap), removes them, and changes the rules with left / right. When the
   owner starts a match every copy goes into it from here, and comes back here when it ends (the game is put back to
   this moment: gs/state.c). */
#define LB_ROWS 8
#define LB_RULES 5
#define LB_BTN 5 /* the right side's items: the rules 0..4, then the buttons 5..8 (two rows of two) */
#define LB_RIGHT_COUNT 9
#define LB_POP_MEMBER 1
#define LB_POP_LEAVE 2
#define LB_ACT_START 1
#define LB_ACT_READY 2
#define LB_ACT_CHAT 3
#define LB_ACT_LEAVE 4

static PortLobbyView sTbLv; /* (what the page shows this frame) */

static char *sTbBattleName[5] = {"SINGLE BATTLE", "TEAM BATTLE", "DP BATTLE: 10 DP", "DP BATTLE: 15 DP", "DP BATTLE: 20 DP"};
static char *sTbRuleName[LB_RULES] = {"Battle", "Time Limit", "Game Speed", "Rollback", "Input Delay"};
/* What the lobby says, written by port/src/gs/net_lobby.c in English: here so that the translation file has them
   (a whole line is looked up where it is drawn; a line with a name in it is matched by its end, Tb_LobbyLine). */
static char *sTbLobbyLines[] = {
    "The owner went back to the lobby.",
    "The two players could not connect to each other.",
    "The lobby was closed by its owner.",
    "The lobby's owner removed you from the lobby.",
    "The connection to the lobby's owner was lost: the lobby is closed.",
    "This lobby is full (8 places).",
    "The lobby owner's game is a different version: joining needs the same release.",
    "The lobby owner's game data are not the same as yours (a modified disc image or mods on one side): you cannot join.",
    "That address is not known.",
    "No socket could be opened.",
    "Lobbies need the game's state list (the .mem file beside the program): copy all the files of the release.",
};
static char *sTbLobbyNamed[4] = {"%s joined the lobby", "%s left the lobby", "%s was removed from the lobby", "%s lost the connection to the lobby"};

/* `fmt` (in the chosen language: Tb_Fmt) with its %s parts the names a, b and its %d the number. */
static void Tb_Subst(char *out, s32 size, char *fmt, const char *a, const char *b, s32 num) {
    char *f = Tb_Fmt(fmt);
    s32 n = 0;
    s32 names = 0;

    while (*f != 0 && n < size - 1) {
        if (f[0] == '%' && f[1] == 's') {
            const char *v = names++ == 0 ? a : b;

            while (v != NULL && *v != 0 && n < size - 1) {
                out[n++] = *v++;
            }
            f += 2;
        } else if (f[0] == '%' && f[1] == 'd') {
            char digits[16];
            s32 k = 0;

            sprintf(digits, "%d", (int)num);
            while (digits[k] != 0 && n < size - 1) {
                out[n++] = digits[k++];
            }
            f += 2;
        } else if (f[0] == '%' && f[1] == '%') {
            out[n++] = '%';
            f += 2;
        } else {
            out[n++] = *f++;
        }
    }
    out[n] = 0;
}

/* A line of the lobby's (a note or an error) in the chosen language: a whole line by the translation file, a line
   with a name in it by its end, "Match N: A vs B" by its parts; anything else as it is. */
static void Tb_LobbyLine(char *out, s32 size, const char *in) {
    s32 len = Ps_Len(in);
    s32 k;

    for (k = 0; k < 4; k++) {
        char *end = sTbLobbyNamed[k] + 2; /* (what follows the name) */
        s32 e = Ps_Len(end);

        if (len > e && strcmp(in + len - e, end) == 0) {
            char name[24];
            s32 i;

            for (i = 0; i < len - e && i < 23; i++) {
                name[i] = in[i];
            }
            name[i] = 0;
            Tb_Subst(out, size, sTbLobbyNamed[k], name, NULL, 0);
            return;
        }
    }
    if (strncmp(in, "Match ", 6) == 0) {
        const char *p = in + 6;
        s32 num = 0;

        while (*p >= '0' && *p <= '9') {
            num = num * 10 + (*p++ - '0');
        }
        if (p[0] == ':' && p[1] == ' ') {
            char a[24];
            char b[24];
            s32 i = 0;

            p += 2;
            while (*p != 0 && strncmp(p, " vs ", 4) != 0 && i < 23) {
                a[i++] = *p++;
            }
            a[i] = 0;
            if (strncmp(p, " vs ", 4) == 0) {
                Ps_Copy(b, p + 4, 24);
                Tb_Subst(out, size, "Match %d: %s vs %s", a, b, num);
                return;
            }
        }
    }
    (void)sTbLobbyLines; /* (they are translated as whole lines, by Ps_Copy) */
    Ps_Copy(out, in, size); /* (a whole line of the file in its language, as it is otherwise) */
}

static void Tb_RuleValue(char *out, s32 which) {
    PortLobbyView *v = &sTbLv;

    switch (which) {
    case LOBBY_RULE_BATTLE:
        Ps_Copy(out, sTbBattleName[v->battle % 5], 32);
        break;
    case LOBBY_RULE_TIME:
        Ps_Copy(out, sTbTime[v->time % 5], 32);
        break;
    case LOBBY_RULE_SPEED:
        sprintf(out, v->speed == Port_NetSpeedStandard() ? Tb_Fmt("%d%% (STANDARD)") : Tb_Fmt("%d%%"), (int)v->speed);
        break;
    case LOBBY_RULE_ROLLBACK:
        if (v->rollback <= 0) {
            Ps_Copy(out, "OFF", 32);
        } else {
            sprintf(out, Tb_Fmt("UP TO %d FRAMES"), (int)v->rollback);
        }
        break;
    default:
        Tb_DelayText(out, v->delay - 1); /* (the lobby's 0 is automatic, else the frames + 1) */
        break;
    }
}

/* The page opens (from the hub's Lobby, Join Room with a lobby's code, or the screens opened with a lobby on). */
static void Tb_LobbyOpen(void) {
    /* the hosts of the page it comes from go off to the right while the lobby's windows come in from the left (the
       lobby's page has no room for them) */
    sTb->lbHostPose = sTb->mode == TB_HUB ? TBO_HOSTS_MIC_WAVE : sTb->mode == TB_JOIN ? TBO_HOSTS_PUZZLED
                    : sTb->mode == TB_WAIT ? (sTb->found ? TBO_HOSTS_CHEER : TBO_HOSTS_BINOCULARS) : -1;
    sTb->lbHostX = sTb->mode == TB_HUB ? 396.0f : sTb->mode == TB_JOIN ? 432.0f : sTb->found ? 426.0f : 434.0f;
    sTb->lbHostH = sTb->mode == TB_HUB ? 262.0f : sTb->mode == TB_JOIN ? 200.0f : sTb->found ? 180.0f : 210.0f;
    sTb->lbPopClose = 0;
    sTb->lbHostT = 0;
    sTb->lbSeenState = sTbLv.state;
    sTb->lbCol = 0;
    sTb->lbPlace = 0;
    sTb->lbRight = LB_BTN;
    sTb->lbPicked = -1;
    sTb->lbPop = 0;
    sTb->lbChat = 0;
    sTb->ended = Port_NetSessionsEnded();
    Port_LobbyCancel(); /* (the search that found it: the lobby has its socket now) */
    Tb_SetMode(TB_LOBBY);
}

/* The hub's Lobby: a new lobby, owned by this game, with a room code; the rules to begin with are Create Room's. */
static void Tb_LobbyCreate(void) {
    if (!Tb_Vanilla()) {
        return;
    }
    Snd_PlaySe(1, 1);
    Port_LobbyCancel();
    Port_NetNameSet(Port_NetNameGet());
    Port_LobbyCreate(1, 0, sTb->battle, sTb->time, Port_Setting("net_speed_host", Port_NetSpeedStandard()), sTb->roll, sTb->delay + 1);
    Tb_LobbyOpen();
}

/* The buttons on the right, by place (0 top left, 1 top right, 2 bottom left, 3 bottom right): LB_ACT_, 0 none. The
   owner has four; a member three, Leave across the bottom row. */
static s32 Tb_LobbyAct(s32 k) {
    static const s32 own[4] = {LB_ACT_START, LB_ACT_READY, LB_ACT_CHAT, 0};
    static const s32 member[4] = {LB_ACT_READY, LB_ACT_CHAT, 0, 0}; /* (leaving: triangle, with its yes / no) */

    return sTbLv.owner ? own[k & 3] : member[k & 3];
}

static char *Tb_LobbyActName(s32 act) {
    PortLobbyView *v = &sTbLv;

    switch (act) {
    case LB_ACT_START:
        return v->phase ? "Match On" : "Start Match";
    case LB_ACT_READY:
        return v->me >= 0 && v->ready[v->me] ? "Not Ready" : "Ready";
    case LB_ACT_CHAT:
        return "Chat";
    case LB_ACT_LEAVE:
        return v->owner ? "Close Lobby" : "Leave Lobby";
    }
    return "";
}

static s32 Tb_LobbyCanStart(void) {
    return sTbLv.owner && sTbLv.phase == 0 && sTbLv.taken[0] && sTbLv.taken[1];
}

static void Tb_LobbyStart(void) {
    if (Tb_LobbyCanStart()) {
        Snd_PlaySe(1, 1);
        Port_LobbyDo(LOBBY_START, 0, 0);
    } else {
        Snd_PlaySe(1, 2);
        Tb_Note(sTbLv.phase ? "A MATCH IS ON ALREADY" : "TWO PLAYERS ARE NEEDED: ONE IN 1P, ONE IN 2P");
    }
}

static void Tb_LobbyAction(s32 act) {
    PortLobbyView *v = &sTbLv;

    switch (act) {
    case LB_ACT_START:
        Tb_LobbyStart();
        break;
    case LB_ACT_READY:
        if (v->me >= 0) {
            Snd_PlaySe(1, 1);
            Port_LobbyDo(LOBBY_READY, v->ready[v->me] ? 0 : 1, 0);
        }
        break;
    case LB_ACT_CHAT:
        Snd_PlaySe(1, 1);
        sTb->lbChat = 1;
        sTb->chatLen = 0;
        sTb->chat[0] = 0;
        sTb->keyRow = 0;
        sTb->keyCol = 0;
        sTb->typing = 1;
        Port_TextMode(1);
        break;
    case LB_ACT_LEAVE:
        Snd_PlaySe(1, 0);
        sTb->lbPop = LB_POP_LEAVE;
        sTb->lbPopT = 0;
        sTb->lbPopClose = 0;
        sTb->lbPopCur = 1; /* (on No) */
        break;
    }
}

/* What the chat's keypad types: the name keypad's letters, and from the PC keyboard any plain character but the
   one that starts a tag of the game's text box. */
static void Tb_ChatAdd(s32 c, s32 any) {
    if (sTb->chatLen < TB_CHAT_MAX && (any ? c >= 32 && c < 127 && c != '<' && c != '>' : Tb_NameKey(c))) {
        sTb->chat[sTb->chatLen++] = (char)c;
        sTb->chat[sTb->chatLen] = 0;
        Snd_PlaySe(1, 0);
    }
}

static void Tb_ChatClose(void) {
    sTb->lbChat = 0;
    sTb->typing = 0;
    Port_TextMode(0);
}

static void Tb_ChatUpdate(void) {
    Pad *pad = &gPad[0];
    s32 c;
    s32 done = 0;

    Ps_Prompt(0, "Enter");
    Ps_Prompt(1, "Delete");
    Ps_Prompt(7, "Send");
    Ps_Prompt(2, "Cancel");
    while ((c = Port_TextRead()) != 0) {
        if (c == 13) {
            done = 1;
        } else if (c == 27) {
            done = -1;
            break;
        } else if (c == 8) {
            if (sTb->chatLen > 0) {
                sTb->chat[--sTb->chatLen] = 0;
                Snd_PlaySe(1, 2);
            }
        } else {
            Tb_ChatAdd(c, 1);
        }
    }
    if (!done) {
        c = Tb_PadUpdate(NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
        if (c > 0) {
            Tb_ChatAdd(c, 0);
        } else if (c == -TB_KEY_SPACE) {
            Tb_ChatAdd(' ', 0);
        } else if (c == -TB_KEY_CASE) {
            sTb->lower = !sTb->lower;
            Snd_PlaySe(1, 0);
        } else if (c == -TB_KEY_DELETE || (pad->gamePressed & PADG_SQUARE)) {
            if (sTb->chatLen > 0) {
                sTb->chat[--sTb->chatLen] = 0;
                Snd_PlaySe(1, 2);
            }
        } else if (c == -TB_KEY_OK || (pad->gamePressed & PADG_START)) {
            done = 1;
        } else if (pad->gamePressed & PADG_TRIANGLE) {
            done = -1;
        }
    }
    if (done == 1) {
        while (sTb->chatLen > 0 && sTb->chat[sTb->chatLen - 1] == ' ') {
            sTb->chat[--sTb->chatLen] = 0;
        }
        if (sTb->chatLen > 0) {
            Port_LobbySay(sTb->chat);
        }
        Snd_PlaySe(1, 1);
        Tb_ChatClose();
    } else if (done == -1) {
        Snd_PlaySe(1, 2);
        Tb_ChatClose();
    }
}

/* The owner's window over a place: Move, Remove from the Lobby (not for the owner's own place), Back. */
static s32 Tb_LobbyPopCount(void) {
    return sTb->lbPop == LB_POP_MEMBER ? (sTbLv.isOwner[sTb->lbPopPlace] ? 2 : 3) : 2;
}

static char *Tb_LobbyPopItem(s32 i) {
    if (sTb->lbPop == LB_POP_LEAVE) {
        return i == 0 ? "Yes" : "No";
    }
    if (i == 0) {
        return "Move";
    }
    return i == 1 && !sTbLv.isOwner[sTb->lbPopPlace] ? "Remove from the Lobby" : "Back";
}

#define TB_POP_IN 10  /* frames a window over the lobby's page takes to come in */
#define TB_POP_OUT 8  /* ... and to go */

/* How far in a window over the page is, 0..1 (eased as the pages' slide, Tb_In). */
static f32 Tb_LobbyPopIn(void) {
    f32 t = sTb->lbPopClose > 0 ? (f32)sTb->lbPopClose / (f32)TB_POP_OUT : (f32)sTb->lbPopT / (f32)TB_POP_IN;

    if (t > 1.0f) {
        t = 1.0f;
    }
    t = 1.0f - t;
    return 1.0f - t * t * t;
}

static void Tb_LobbyPopUpdate(void) {
    Pad *pad = &gPad[0];
    s32 count = Tb_LobbyPopCount();
    char *item;

    Ps_Prompt(0, "Enter");
    Ps_Prompt(2, "Back");
    Ps_Move(&sTb->lbPopCur, count);
    if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
        Snd_PlaySe(1, 2);
        sTb->lbPopClose = TB_POP_OUT; /* (it slides out first) */
        return;
    }
    if (!(pad->gamePressed & PADG_CROSS)) {
        return;
    }
    item = Tb_LobbyPopItem(sTb->lbPopCur);
    if (sTb->lbPop == LB_POP_LEAVE) {
        if (sTb->lbPopCur == 0) {
            Snd_PlaySe(1, 1);
            Port_LobbyDo(LOBBY_LEAVE, 0, 0);
        } else {
            Snd_PlaySe(1, 2);
        }
        sTb->lbPopClose = TB_POP_OUT; /* (it slides out first) */
        return;
    }
    if (strcmp(item, "Move") == 0) {
        Snd_PlaySe(1, 1);
        sTb->lbPicked = sTb->lbPopPlace;
        sTb->lbCol = 0;
    } else if (strcmp(item, "Back") == 0) {
        Snd_PlaySe(1, 2);
    } else {
        Snd_PlaySe(1, 1);
        Port_LobbyDo(LOBBY_KICK, sTb->lbPopPlace, 0);
    }
    sTb->lbPopClose = TB_POP_OUT; /* (it slides out first) */
}

/* Up (-1) and down (1) on the right side: the rules (the owner's), then the two rows of buttons, and round. */
static s32 Tb_LobbyNext(s32 r, s32 dir) {
    s32 owner = sTbLv.owner;
    s32 row1 = Tb_LobbyAct(2) != 0; /* (a second row of buttons) */

    if (dir > 0) {
        if (r < LB_BTN) {
            return r + 1; /* (the last rule: to the first button) */
        }
        if (r < LB_BTN + 2) {
            return row1 ? (Tb_LobbyAct(r - LB_BTN + 2) != 0 ? r + 2 : LB_BTN + 2) : owner ? 0 : r;
        }
        return owner ? 0 : r - 2;
    }
    if (r == 0) {
        return row1 ? LB_BTN + 2 : LB_BTN;
    }
    if (r < LB_BTN) {
        return r - 1;
    }
    if (r < LB_BTN + 2) {
        return owner ? LB_BTN - 1 : row1 ? LB_BTN + 2 : r;
    }
    return LB_BTN;
}

static void Tb_LobbyUpdate(s32 live) {
    Pad *pad = &gPad[0];
    PortLobbyView *v = &sTbLv;
    s32 own;

    if (sTb->ended != Port_NetSessionsEnded()) { /* back from a match (the game was put back to its moment): as it was */
        sTb->ended = Port_NetSessionsEnded();
        sTb->lbPop = 0;
        sTb->lbPicked = -1;
        if (sTb->lbChat) {
            Tb_ChatClose();
        }
    }
    sTb->lbHostT++;
    if (v->state == 2 && sTb->lbSeenState == 1) {
        sTb->modeT = 0; /* reached (joined, or the code came): the page's windows come in now, as a page's do */
    }
    sTb->lbSeenState = v->state;
    if (v->state == 0) { /* left, or closed and read */
        if (sTb->lbChat) {
            Tb_ChatClose();
        }
        Tb_SetMode(TB_HUB);
        return;
    }
    own = v->owner && v->phase == 0;
    if (v->state != 2) { /* under way, or closed: the window says it (Tb_DrawLobbyWin) */
        if (sTb->lbChat) {
            Tb_ChatClose();
        }
        sTb->lbPop = 0;
        Ps_Prompt(v->state == 3 ? 0 : 2, v->state == 3 ? "OK" : "Cancel");
        if (!live) {
            return;
        }
        if (v->state == 3 && (pad->gamePressed & (PADG_CROSS | PADG_CIRCLE | PADG_TRIANGLE))) {
            Snd_PlaySe(1, 1);
            Port_LobbyDo(LOBBY_OK, 0, 0);
        } else if (v->state == 1 && (pad->gamePressed & (PADG_CIRCLE | PADG_TRIANGLE))) {
            Snd_PlaySe(1, 2);
            Port_LobbyDo(LOBBY_LEAVE, 0, 0);
        }
        return;
    }
    if (!own) {
        sTb->lbPicked = -1;
        if (sTb->lbPop == LB_POP_MEMBER) {
            sTb->lbPop = 0;
        }
    }
    if (sTb->lbChat) {
        if (live) {
            Tb_ChatUpdate();
        }
        return;
    }
    if (sTb->lbPop != 0) {
        sTb->lbPopT++;
        if (sTb->lbPopClose > 0) { /* going: no input meanwhile */
            if (--sTb->lbPopClose == 0) {
                sTb->lbPop = 0;
            }
        } else if (live) {
            Tb_LobbyPopUpdate();
        }
        return;
    }
    if (sTb->lbPicked >= 0) {
        Ps_Prompt(0, "Move Here");
        Ps_Prompt(2, "Cancel");
    } else {
        if (sTb->lbCol == 1 && sTb->lbRight < LB_BTN && own) {
            Ps_Prompt(3, "Change");
        }
        Ps_Prompt(0, "Enter");
        if (v->code[0] != 0) {
            Ps_Prompt(1, "Copy Code");
        }
        if (Tb_LobbyCanStart() && !(sTb->lbCol == 1 && sTb->lbRight < LB_BTN)) {
            Ps_Prompt(7, "Start Match");
        }
        Ps_Prompt(2, v->owner ? "Close Lobby" : "Leave Lobby");
    }
    if (!live) {
        return;
    }
    if (!v->owner && sTb->lbRight < LB_BTN) {
        sTb->lbRight = LB_BTN;
    }
    if (pad->gamePressed & (PADG_L1 | PADG_R1)) { /* the shoulder buttons: the other side of the page */
        sTb->lbCol = !sTb->lbCol;
        Snd_PlaySe(1, 0);
    } else if (sTb->lbCol == 0) {
        s32 side = Ps_Side();

        Ps_Move(&sTb->lbPlace, LB_ROWS);
        if (side != 0 && sTb->lbPicked < 0) {
            sTb->lbCol = 1;
            Snd_PlaySe(1, 0);
        }
    } else {
        s32 r = sTb->lbRight;
        s32 side = Ps_Side();

        if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            r = Tb_LobbyNext(r, 1);
            Snd_PlaySe(1, 0);
        } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            r = Tb_LobbyNext(r, -1);
            Snd_PlaySe(1, 0);
        }
        sTb->lbRight = r;
        if (side != 0 && r < LB_BTN) {
            if (own) {
                Port_LobbyDo(LOBBY_RULE, r, side);
                Snd_PlaySe(1, 0);
            } else if (v->owner) {
                Snd_PlaySe(1, 2);
                Tb_Note("THE RULES CANNOT CHANGE DURING A MATCH");
            }
        } else if (side != 0) {
            s32 k = r - LB_BTN;

            if (side < 0 && (k & 1) == 0) {
                sTb->lbCol = 0;
            } else if (side > 0 && (k & 1) == 0 && Tb_LobbyAct(k + 1) != 0) {
                sTb->lbRight = r + 1;
            } else if (side < 0) {
                sTb->lbRight = r - 1;
            }
            Snd_PlaySe(1, 0);
        }
    }
    if (pad->gamePressed & PADG_CROSS) {
        if (sTb->lbCol == 0) {
            s32 i = sTb->lbPlace;

            if (sTb->lbPicked >= 0) {
                if (i != sTb->lbPicked && own) {
                    Snd_PlaySe(1, 1);
                    Port_LobbyDo(LOBBY_MOVE, sTb->lbPicked, i);
                } else {
                    Snd_PlaySe(1, 2);
                }
                sTb->lbPicked = -1;
            } else if (own && v->taken[i]) {
                Snd_PlaySe(1, 1);
                sTb->lbPop = LB_POP_MEMBER;
                sTb->lbPopT = 0;
                sTb->lbPopClose = 0;
                sTb->lbPopPlace = i;
                sTb->lbPopCur = 0;
            } else if (v->owner && v->phase) {
                Snd_PlaySe(1, 2);
                Tb_Note("NOT DURING A MATCH: THE PLACES CHANGE BETWEEN MATCHES");
            } else if (!v->owner) {
                Snd_PlaySe(1, 2);
                Tb_Note("THE LOBBY'S OWNER CHOOSES WHO PLAYS");
            }
        } else if (sTb->lbRight < LB_BTN) {
            Snd_PlaySe(1, 0);
            sTb->lbRight++;
        } else {
            Tb_LobbyAction(Tb_LobbyAct(sTb->lbRight - LB_BTN));
        }
    } else if ((pad->gamePressed & PADG_SQUARE) && v->code[0] != 0 && sTb->lbPicked < 0) {
        PortTbo_Copy(v->code);
        Snd_PlaySe(1, 1);
        Tb_Note("COPIED: PASTE THE CODE TO THE OTHERS");
    } else if ((pad->gamePressed & PADG_START) && v->owner && sTb->lbPicked < 0) {
        Tb_LobbyStart();
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        if (sTb->lbPicked >= 0) {
            Snd_PlaySe(1, 2);
            sTb->lbPicked = -1;
        } else {
            Tb_LobbyAction(LB_ACT_LEAVE);
        }
    }
}

/* ---- the lobby's drawing ---- */
#define LB_L0 12.0f   /* the places' window */
#define LB_L1 250.0f
#define LB_R0 258.0f  /* the code, the rules, the buttons */
#define LB_R1 502.0f

static f32 Tb_LobbyRowY(s32 i) {
    return i < 2 ? 76.0f + 25.0f * (f32)i : 150.0f + 25.0f * (f32)(i - 2);
}

static f32 Tb_LobbyRuleY(s32 i) {
    return 134.0f + 22.0f * (f32)i;
}

static void Tb_LobbyBtnBox(s32 k, f32 *x0, f32 *y0, f32 *x1, f32 *y1) {
    s32 wide = (k & 1) == 0 && Tb_LobbyAct(k + 1) == 0; /* (a button alone in its row: across it) */

    *x0 = (k & 1) ? (LB_R0 + LB_R1) * 0.5f + 3.0f : LB_R0;
    *x1 = (k & 1) || wide ? LB_R1 : (LB_R0 + LB_R1) * 0.5f - 3.0f;
    *y0 = Tb_LobbyAct(2) == 0 ? 274.0f : k < 2 ? 260.0f : 290.0f; /* (one row: in the middle of the place for two) */
    *y1 = *y0 + 24.0f;
}

static void Tb_PageIn(s32 on);
/* The last page's hosts, going off to the right in front of the page's windows and behind the box (16 frames, as a
   page comes in). */
static void Tb_LobbyHostsOut(void) {
    if (sTb->lbHostPose >= 0 && sTb->lbHostT < 16) {
        f32 t = (f32)sTb->lbHostT / 16.0f;

        Ps_SetShift(0.0f);
        Tb_RaditzBust(t * t * 320.0f);
        Tb_PageIn(1);
    }
}

static void Tb_DrawLobby(void) {
    PortLobbyView *v = &sTbLv;
    s32 i;
    s32 k;

    if (v->state != 2) {
        if (v->state == 3) { /* (closed: the window says why, the hosts are puzzled) */
            Tb_Hosts(TBO_HOSTS_PUZZLED, 400.0f, 350.0f, 230.0f);
        }
        Tb_LobbyHostsOut();
        return;
    }
    Ps_Panel(TBO_KIT_WINDOW, LB_L0, 52.0f, LB_L1, 316.0f, 0x80);
    for (i = 0; i < LB_ROWS; i++) {
        f32 y = Tb_LobbyRowY(i);
        s32 on = sTb->lbCol == 0 && i == sTb->lbPlace && !sTb->lbChat;

        if (on) {
            Ps_Pill(TBO_KIT_SEL, LB_L0 + 6.0f, y - 2.0f, LB_L1 - 6.0f, y + 24.0f, 0x80);
        } else if (i == sTb->lbPicked) {
            Ps_Pill(TBO_KIT_GOLD, LB_L0 + 8.0f, y, LB_L1 - 8.0f, y + 22.0f, 0x80);
        } else {
            Ps_Pill(TBO_KIT_BAR, LB_L0 + 8.0f, y, LB_L1 - 8.0f, y + 22.0f, v->taken[i] ? 0x80 : 0x58);
        }
        if (v->taken[i] && (v->isOwner[i] || i == v->me)) { /* the owner's mark, and this game's own place */
            Ps_Pill(TBO_KIT_STEEL, 160.0f, y + 5.0f, 200.0f, y + 17.0f, 0x80);
        }
    }
    Ps_Panel(TBO_KIT_WINDOW, LB_R0, 52.0f, LB_R1, 104.0f, 0x80);
    Ps_Panel(TBO_KIT_WINDOW, LB_R0, 108.0f, LB_R1, 254.0f, 0x80);
    for (i = 0; i < LB_RULES; i++) {
        f32 y = Tb_LobbyRuleY(i);
        s32 on = sTb->lbCol == 1 && sTb->lbRight == i && !sTb->lbChat;

        if (on) {
            Ps_Pill(TBO_KIT_SEL, LB_R0 + 6.0f, y - 2.0f, LB_R1 - 6.0f, y + 22.0f, 0x80);
            if (v->owner && v->phase == 0) {
                Ps_Button(3, LB_R1 - 20.0f, y + 10.0f, 18.0f);
            }
        } else {
            Ps_Pill(TBO_KIT_BAR, LB_R0 + 8.0f, y, LB_R1 - 8.0f, y + 20.0f, 0x80);
        }
    }
    for (k = 0; k < 4; k++) {
        f32 x0;
        f32 y0;
        f32 x1;
        f32 y1;
        s32 act = Tb_LobbyAct(k);
        s32 on = sTb->lbCol == 1 && sTb->lbRight == LB_BTN + k && !sTb->lbChat;

        if (act == 0) {
            continue;
        }
        Tb_LobbyBtnBox(k, &x0, &y0, &x1, &y1);
        if (on) {
            Ps_Pill(TBO_KIT_SEL, x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f, 0x80);
        } else {
            Ps_Pill(TBO_KIT_BAR, x0, y0, x1, y1, act == LB_ACT_START && !Tb_LobbyCanStart() ? 0x58 : 0x80);
        }
    }
    Tb_LobbyHostsOut();
}

static void Tb_DrawLobbyText(void) {
    static char line[96];
    PortLobbyView *v = &sTbLv;
    s32 i;
    s32 k;
    s32 watch = 0;

    if (v->state != 2) {
        return; /* (Tb_DrawLobbyWin) */
    }
    for (i = 2; i < LB_ROWS; i++) {
        watch += v->taken[i];
    }
    Ps_Text(26.0f, Ps_TextTop(64.0f, 0.5f), "PLAYERS", 0.5f, 0.0f, 2);
    sprintf(line, Tb_Fmt("SPECTATORS  %d / 6"), (int)watch);
    Ps_Text(26.0f, Ps_TextTop(138.0f, 0.5f), line, 0.5f, 210.0f, 2);
    for (i = 0; i < LB_ROWS; i++) {
        f32 y = Tb_LobbyRowY(i);
        f32 mid = y + 11.0f;
        s32 on = sTb->lbCol == 0 && i == sTb->lbPlace && !sTb->lbChat;
        char tag[4];

        if (i < 2) {
            sprintf(tag, "%dP", (int)(i + 1));
        } else {
            sprintf(tag, "%d", (int)(i - 1));
        }
        Ps_Align(1);
        Ps_Text(37.0f, Ps_TextTop(mid, 0.5f), tag, 0.5f, 0.0f, on ? 1 : 4);
        Ps_Align(0);
        if (v->taken[i]) {
            Ps_Text(54.0f, Ps_TextTop(mid, 0.6f), v->name[i], 0.6f, v->isOwner[i] || i == v->me ? 102.0f : 140.0f, on ? 1 : i == v->me ? 3 : 0);
            if (v->isOwner[i] || i == v->me) {
                Ps_Align(1);
                Ps_Text(180.0f, Ps_TextTop(mid, 0.34f), v->isOwner[i] ? "OWNER" : "YOU", 0.34f, 36.0f, 0);
                Ps_Align(0);
            }
            if (v->ready[i]) {
                Ps_Align(2);
                Ps_Text(228.0f, Ps_TextTop(mid, 0.42f), "READY", 0.42f, 36.0f, 5); /* (inside the bar's round end) */
                Ps_Align(0);
            }
        } else {
            Ps_Text(54.0f, Ps_TextTop(mid, 0.5f), i == sTb->lbPicked ? "" : "EMPTY", 0.5f, 0.0f, 4);
        }
    }
    /* the code */
    if (v->code[0] != 0) {
        Ps_Text(270.0f, Ps_TextTop(68.0f, 0.46f), "LOBBY CODE", 0.46f, 0.0f, 2);
        Ps_Text(270.0f, Ps_TextTop(88.0f, 0.95f), v->code, 0.95f, 120.0f, 0);
        Ps_Align(2);
        Ps_Text(490.0f, Ps_TextTop(68.0f, 0.4f), v->owner ? "GIVE IT TO THE OTHERS" : "JOIN ROOM WITH IT", 0.4f, 120.0f, 4);
        Ps_Align(0);
    } else if (v->owner && v->port > 0) {
        Ps_Text(270.0f, Ps_TextTop(68.0f, 0.46f), "OTHERS JOIN WITH YOUR ADDRESS AND PORT", 0.46f, 220.0f, 2);
        sprintf(line, "%d", (int)v->port);
        Ps_Text(270.0f, Ps_TextTop(88.0f, 0.8f), line, 0.8f, 120.0f, 0);
    } else {
        Ps_Text(270.0f, Ps_TextTop(68.0f, 0.46f), "LOBBY", 0.46f, 0.0f, 2);
        Ps_Text(270.0f, Ps_TextTop(88.0f, 0.6f), "JOINED BY ADDRESS", 0.6f, 220.0f, 0);
    }
    if (v->phase) {
        Ps_Align(2);
        Ps_Text(490.0f, Ps_TextTop(90.0f, 0.46f), "A MATCH IS ON", 0.46f, 110.0f, 5);
        Ps_Align(0);
    }
    /* the rules */
    Ps_Text(270.0f, Ps_TextTop(122.0f, 0.46f), v->owner ? "RULES" : "RULES (THE OWNER'S)", 0.46f, 220.0f, 2);
    for (i = 0; i < LB_RULES; i++) {
        f32 mid = Tb_LobbyRuleY(i) + 10.0f;
        s32 on = sTb->lbCol == 1 && sTb->lbRight == i && !sTb->lbChat;

        Ps_Text(282.0f, Ps_TextTop(mid, 0.54f), sTbRuleName[i], 0.54f, 92.0f, on ? 1 : 0); /* (inside its bar as the places' names are) */
        Tb_RuleValue(line, i);
        Ps_Align(2);
        Ps_Text(on && v->owner && v->phase == 0 ? 456.0f : 482.0f, Ps_TextTop(mid, 0.46f), line, 0.46f, 124.0f, 3);
        Ps_Align(0);
    }
    /* the buttons */
    for (k = 0; k < 4; k++) {
        f32 x0;
        f32 y0;
        f32 x1;
        f32 y1;
        s32 act = Tb_LobbyAct(k);
        s32 on = sTb->lbCol == 1 && sTb->lbRight == LB_BTN + k && !sTb->lbChat;

        if (act == 0) {
            continue;
        }
        Tb_LobbyBtnBox(k, &x0, &y0, &x1, &y1);
        Ps_Align(1);
        Ps_Text((x0 + x1) * 0.5f, Ps_TextTop((y0 + y1) * 0.5f, 0.56f), Tb_LobbyActName(act), 0.56f, x1 - x0 - 14.0f,
                on ? 1 : act == LB_ACT_START && !Tb_LobbyCanStart() ? 4 : 0);
        Ps_Align(0);
    }
}

/* The windows over the page (drawn last): the lobby under way or closed, the owner's choices over a place, leaving,
   and the chat's keypad. */
#define TB_MSG_NOTE 0.88f /* a window's explanation, smaller than its heading (Ps_Heading 0.9) */
static void Tb_MsgLines(f32 x, f32 y, f32 width, const char *text, s32 maxLines) {
    static char part[160];
    s32 at = 0;
    s32 lines = 0;

    while (text[at] != 0 && lines < maxLines) {
        s32 n = 0;
        s32 lastSpace = -1;

        while (text[at + n] != 0 && n < 158) {
            part[n] = text[at + n];
            part[n + 1] = 0;
            if (text[at + n] == ' ') {
                lastSpace = n;
            }
            if (Ps_MsgWidth(part, TB_MSG_NOTE) > width && lastSpace > 0) {
                n = lastSpace;
                break;
            }
            n++;
        }
        part[n] = 0;
        Ps_Msg(x, y + 18.0f * (f32)lines, part, TB_MSG_NOTE, 0x80);
        at += n;
        while (text[at] == ' ') {
            at++;
        }
        lines++;
    }
}

static void Tb_DrawLobbyWin(void) {
    PortLobbyView *v = &sTbLv;
    static char line[256];
    s32 i;


    if (v->state == 1 || v->state == 3) {
        Ps_Panel(TBO_KIT_WINDOW, 40.0f, 110.0f, 330.0f, 250.0f, 0x80);
        if (v->state == 1) {
            Ps_Heading(60.0f, 118.0f, v->owner ? "Opening the Lobby" : "Joining the Lobby", 0.9f, 250.0f, 0x80);
            Ps_Msg(60.0f, 160.0f, v->owner ? "Asking the matchmaking service for a code..." : "Looking for the lobby's owner...", 0.88f, 0x80);
        } else {
            Ps_Heading(60.0f, 118.0f, "No Longer in the Lobby", 0.9f, 250.0f, 0x80);
            if (v->error[0] != 0) { /* (not `?:` between the view's line and a literal: the 32-bit pointers) */
                Tb_LobbyLine(line, 256, v->error);
            } else {
                Tb_LobbyLine(line, 256, "The lobby is closed.");
            }
            Tb_MsgLines(60.0f, 158.0f, 254.0f, line, 4);
        }
        return;
    }
    if (sTb->lbPop == LB_POP_MEMBER || sTb->lbPop == LB_POP_LEAVE) {
        s32 count = Tb_LobbyPopCount();

        Ps_SetShift((1.0f - Tb_LobbyPopIn()) * -200.0f); /* (it slides in from the left, and out the same way) */
        f32 y0 = sTb->lbPop == LB_POP_LEAVE ? 120.0f : Tb_LobbyRowY(sTb->lbPopPlace) - 8.0f;
        f32 x0 = sTb->lbPop == LB_POP_LEAVE ? 126.0f : 150.0f;
        f32 x1 = sTb->lbPop == LB_POP_LEAVE ? 386.0f : 350.0f;
        f32 top = sTb->lbPop == LB_POP_LEAVE ? 60.0f : 34.0f;

        if (y0 + top + 28.0f * (f32)count + 10.0f > 318.0f) {
            y0 = 318.0f - top - 28.0f * (f32)count - 10.0f;
        }
        Ps_Panel(TBO_KIT_WINDOW, x0, y0, x1, y0 + top + 28.0f * (f32)count + 10.0f, 0x80);
        if (sTb->lbPop == LB_POP_LEAVE) {
            Ps_Heading(x0 + 16.0f, y0 + 6.0f, v->owner ? "Close the Lobby?" : "Leave the Lobby?", 0.9f, x1 - x0 - 32.0f, 0x80);
            Ps_Msg(x0 + 16.0f, y0 + 36.0f, v->owner ? "Everyone in it goes back to the menus." : "You go back to the online menu.", TB_MSG_NOTE, 0x80);
        } else {
            Ps_Text(x0 + 16.0f, y0 + 8.0f, v->name[sTb->lbPopPlace], 0.6f, x1 - x0 - 32.0f, 3);
        }
        for (i = 0; i < count; i++) {
            f32 y = y0 + top + 28.0f * (f32)i;
            s32 on = i == sTb->lbPopCur;

            if (on) {
                Ps_Pill(TBO_KIT_SEL, x0 + 10.0f, y - 2.0f, x1 - 10.0f, y + 24.0f, 0x80);
            } else {
                Ps_Pill(TBO_KIT_BAR, x0 + 12.0f, y, x1 - 12.0f, y + 22.0f, 0x80);
            }
            Ps_Text(x0 + 28.0f, Ps_TextTop(y + 11.0f, 0.6f), Tb_LobbyPopItem(i), 0.6f, x1 - x0 - 56.0f, on ? 1 : 0);
        }
        Ps_SetShift(0.0f);
    }
    if (sTb->lbChat) { /* the name's keypad, for a line to everyone */
        static char typed[TB_CHAT_MAX + 2];
        s32 from = 0;

        Ps_Panel(TBO_KIT_WINDOW, 64.0f, 62.0f, 448.0f, 340.0f, 0x80);
        Ps_Text(84.0f, 70.0f, "SAY SOMETHING TO THE LOBBY", 0.6f, 340.0f, 2);
        Ps_Pill(TBO_KIT_BAR, 84.0f, 94.0f, 428.0f, 124.0f, 0x80);
        Ps_Copy(typed, sTb->chat, TB_CHAT_MAX + 2);
        Ps_Cat(typed, (sTb->frames / 16) & 1 ? "_" : " ", TB_CHAT_MAX + 2);
        while (typed[from] != 0 && Ps_TextWidth(typed + from, 0.66f) > 312.0f) { /* (the end of a long line) */
            from++;
        }
        Ps_Text(100.0f, Ps_TextTop(109.0f, 0.66f), typed + from, 0.66f, 0.0f, 1);
        Tb_Keypad(94.0f, 134.0f, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
    }
}

/* The box at the foot of the screen on this page: the last three things that happened (notes and chat), newest
   at the bottom, each cut to one line. NULL while nothing has (the hosts' hint then: Tb_Help). In the game's text,
   as Tb_BoxLine makes it; another buffer for another content (the screen reader tells lines apart by that). */
static u16 *Tb_LobbyBox(void) {
    static u16 buf[2][TB_BOX_LINE];
    static unsigned saidTotal = 0xFFFFFFFF;
    static s32 cur;
    PortLobbyView *v = &sTbLv;
    s32 n = 1;
    s32 room = MsgWin_PortLineRoom(400);
    s32 e;
    u16 *out;

    if (v->state != 2 || v->logCount == 0) {
        saidTotal = 0xFFFFFFFF;
        return NULL;
    }
    if (v->logTotal == saidTotal) {
        return buf[cur];
    }
    saidTotal = v->logTotal;
    cur = !cur;
    out = buf[cur];
    out[0] = 0xFEFF;
    out[1] = 0;
    for (e = v->logCount > 3 ? v->logCount - 3 : 0; e < v->logCount; e++) {
        static char text[224];
        s32 start;
        s32 k;

        if (v->logName[e][0] != 0) { /* a line of chat, "Name: what was said" (what a player wrote is not translated) */
            s32 t;

            Ps_Copy(text, v->logName[e], 224);
            Ps_Cat(text, ": ", 224);
            t = Ps_Len(text);
            for (k = 0; v->logText[e][k] != 0 && t < 222; k++) {
                text[t++] = v->logText[e][k];
            }
            text[t] = 0;
        } else {
            Tb_LobbyLine(text, 224, v->logText[e]);
        }
        if (n > 1) {
            out[n++] = '\n';
        }
        start = n;
        for (k = 0; text[k] != 0 && n < TB_BOX_LINE - 5; k++) { /* (UTF-8 to the box's 16-bit letters) */
            u32 c = (u8)text[k];

            if (c >= 0xC0 && c < 0xE0 && text[k + 1] != 0) {
                c = ((c & 0x1F) << 6) | ((u8)text[k + 1] & 0x3F);
                k += 1;
            } else if (c >= 0xE0 && c < 0xF0 && text[k + 1] != 0 && text[k + 2] != 0) {
                c = ((c & 0x0F) << 12) | (((u8)text[k + 1] & 0x3F) << 6) | ((u8)text[k + 2] & 0x3F);
                k += 2;
            }
            out[n++] = (u16)(c == '<' ? '(' : c == '>' ? ')' : c); /* (a < would start a tag of the box) */
            out[n] = 0;
            if (Font_GetWidth(out + start) > room) { /* one line each: the end cut off */
                do {
                    out[--n] = 0;
                } while (n > start + 1 && Font_GetWidth(out + start) > room - 20);
                out[n++] = '.';
                out[n++] = '.';
                out[n++] = '.';
                out[n] = 0;
                break;
            }
        }
        out[n] = 0;
    }
    return out;
}

static void Tb_KeyFocus(char *out, s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb);
/* What the screen reader says the lobby's cursor is on (Tb_Speak). */
static void Tb_LobbyFocus(char *focus, char *now) {
    static char part[64];
    PortLobbyView *v = &sTbLv;
    s32 i = sTb->lbPlace;

    if (v->state == 1) {
        Ps_Copy(focus, v->owner ? "Opening the lobby" : "Joining the lobby", 160);
    } else if (v->state == 3) {
        if (v->error[0] != 0) {
            Tb_LobbyLine(focus, 160, v->error);
        } else {
            Tb_LobbyLine(focus, 160, "The lobby is closed.");
        }
    } else if (sTb->lbChat) {
        Tb_KeyFocus(focus, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
        Ps_Copy(now, sTb->chat, 40);
    } else if (sTb->lbPop == LB_POP_LEAVE) {
        Ps_Copy(focus, v->owner ? "Close the lobby? " : "Leave the lobby? ", 160);
        Ps_Cat(focus, Tb_LobbyPopItem(sTb->lbPopCur), 160);
    } else if (sTb->lbPop == LB_POP_MEMBER) {
        Ps_Copy(focus, v->name[sTb->lbPopPlace], 160);
        Ps_Cat(focus, ": ", 160);
        Ps_Cat(focus, Tb_LobbyPopItem(sTb->lbPopCur), 160);
    } else if (sTb->lbCol == 0) {
        if (i < 2) {
            sprintf(part, Tb_Fmt("Player %d"), (int)(i + 1));
        } else {
            sprintf(part, Tb_Fmt("Spectator %d"), (int)(i - 1));
        }
        Ps_Copy(focus, sTb->lbPicked >= 0 ? "Move to " : "", 160);
        Ps_Cat(focus, part, 160);
        Ps_Cat(focus, ": ", 160);
        if (v->taken[i]) {
            Ps_Cat(focus, v->name[i], 160);
        } else {
            Ps_Cat(focus, "empty", 160);
        }
        if (v->taken[i] && v->isOwner[i]) {
            Ps_Cat(focus, ", owner", 160);
        }
        if (v->taken[i] && v->ready[i]) {
            Ps_Cat(focus, ", ready", 160);
        }
    } else if (sTb->lbRight < LB_BTN) {
        Ps_Copy(focus, sTbRuleName[sTb->lbRight], 160);
        Ps_Cat(focus, ": ", 160);
        Tb_RuleValue(part, sTb->lbRight);
        Ps_Cat(focus, part, 160);
    } else {
        Ps_Copy(focus, Tb_LobbyActName(Tb_LobbyAct(sTb->lbRight - LB_BTN)), 160);
    }
}

/* What the hosts say in the box at the foot of the screen (two lines at most). */
static char *Tb_Help(void) {
    switch (sTb->mode) {
    case TB_HUB:
        if (sTb->hubCur == HUB_VS) { /* against whom, and how many are online there */
            static char line[160];
            s32 n = DrNet_Online();

            if (DrNet_Mode() != 0) {
                return "Against PC players: rooms and quick match on the PC port's service.";
            }
            if (n < 0) {
                return "Against Android and Switch players, on Dragon Net.";
            }
            sprintf(line, Tb_Fmt("Against Android and Switch players. Online now: about %d."), (int)n);
            return line;
        }
        return sTbHubSay[sTb->hubCur];
    case TB_CREATE:
        return sTb->crCur == CR_DELAY ? "How late your moves come out. Automatic measures the line when the match starts."
               : sTb->crCur == CR_ROLL ? "Your moves come out at once and the game corrects itself. More frames cope with a slower line."
               : "Set the rules of your tournament! Then your rival joins with the room code.";
    case TB_JOIN:
        return "Enter your rival's room code, or Paste the one they sent you! Then Join.";
    case TB_WAIT:
        return sTb->found ? "Here we go! The match is starting!" : "Looking for a worthy challenger... Warm up in Training meanwhile!";
    case TB_BLOCK:
        return "Sorry! Online fights have to be fair, so only the game straight off the disc can play.";
    case TB_SPAR:
        return "Warm up at the tournament while we search! Pick yourself and a sparring partner.";
    case TB_QUICK:
        return sTb->qkCur == 1 ? "Auto: rivals near you first. A region: players there first, everyone after 40 s. Anywhere: the fastest."
                               : "Pick a battle and we'll find you a rival! No time limit, and the game runs a little faster.";
    case TB_NAME:
        return "Pick your letters, then Done! Square deletes, Start finishes.";
    case TB_PROFILE:
        return "Show everyone who you are! This is the name your rivals see.";
    case TB_RANKED:
        if (sTbRk.state < 0 || sTbRk.msg == DRMSG_CONFIRMED || sTbRk.msg == DRMSG_UPDATED || sTbRk.msg == DRMSG_WAIT_RIVAL ||
            sTbRk.msg == DRMSG_DISPUTED || sTbRk.msg == DRMSG_VOID || sTbRk.msg == DRMSG_ENDED_NO_POINTS) {
            if (Tb_RkMsg(sTbRk.msg)[0] != 0) {
                return Tb_RkMsg(sTbRk.msg);
            }
        }
        return sTb->rkCur == RK_BATTLE ? "Single or team fights. No time limit, at the standard speed."
               : sTb->rkCur == RK_SEARCH ? "Find a rival of your level! Points change when both results agree."
               : sTb->rkCur == RK_BOARD ? "The best fighters of the community, by their Elo points."
                                        : "Load your profile again from the service.";
    case TB_RKWAIT:
        if (Tb_RkMsg(sTbRk.msg)[0] != 0) {
            return Tb_RkMsg(sTbRk.msg);
        }
        return "Searching for a rival of your level...";
    case TB_BOARD:
        return "The table shows the community's Elo points. X updates it.";
    case TB_LOBBY:
        if (sTbLv.state == 1) {
            return sTbLv.owner ? "Opening your lobby! The others join it with its code." : "Knocking at the lobby's door...";
        }
        if (sTbLv.state == 3) {
            return "You are out of that lobby. OK takes you back to the online menu.";
        }
        if (sTb->lbChat) {
            return "Type a line for everyone in the lobby, then Done! Start sends it too.";
        }
        if (!sTbLv.owner) {
            return "Welcome to the lobby! Its owner picks who plays and starts the match. Chat while you wait!";
        }
        return sTbLv.code[0] != 0 ? "Your lobby is open! Give the others its code: they join with Join Room. Then put two of you in 1P and 2P."
                                  : "Your lobby is open! The others join with your address and port. Then put two of you in 1P and 2P.";
    }
    return "";
}

/* Each page's title (by TB_ mode), as its heading shows it and the screen reader says it. */
static char *sTbTitle[TB_MODES] = {"Dragon Net Online", "Create Room", "Join Room", "Searching", "Profile", "Profile", "Quick Match",
                                   "Train While Waiting", "Dragon Net Online", "Lobby", "Ranked", "Searching", "Ranking"};

/* ---- the screen reader (port/src/gs/a11y.c; silent unless the player has it on). The game's own screens are read
   from their state by port/src/a11y_game.c; these pages say what they show themselves: the page's title as it opens,
   then what the cursor is on (and its value), the code or the name typed so far, and the notes. The hosts' line is
   the box's, which the reader reads as it reads the game's. ---- */
extern void PortA11y_Say(const char *text, int flags);
#define TB_SAY_INTERRUPT 1

/* A room code letter by letter ("J D Y 2 T E"). */
static void Tb_Spell(char *out, const char *code, s32 max) {
    s32 n = 0;
    s32 i;

    for (i = 0; code[i] != 0 && n < max - 3; i++) {
        if (n > 0) {
            out[n++] = ' ';
        }
        out[n++] = code[i];
    }
    out[n] = 0;
}

/* What a keypad's cursor is on: a key's character, or a button's label. */
static void Tb_KeyFocus(char *out, s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb) {
    s32 k;

    out[0] = 0;
    if (sTb->keyRow < rows) {
        out[0] = keys[sTb->keyRow][sTb->keyCol];
        out[1] = 0;
        return;
    }
    for (k = 0; k < nb; k++) {
        if (sTb->keyCol >= b[k].c0 && sTb->keyCol <= b[k].c1) {
            Ps_Copy(out, b[k].label, 32);
        }
    }
}

/* Once a frame, after the page's update. */
static void Tb_Speak(void) {
    static s32 mode = -1;
    static char said[160];
    static char typed[40];
    static s32 noted;
    char focus[160];
    char part[64];
    char now[40];

    if (sTb->frames == 0) { /* (the screens opened again) */
        mode = -1;
        typed[0] = 0;
    }
    focus[0] = 0;
    now[0] = 0;
    switch (sTb->mode) {
    case TB_HUB:
        Ps_Copy(focus, sTbHubName[sTb->hubCur], 160);
        if (!sTbHubReady[sTb->hubCur]) {
            Ps_Cat(focus, ", soon", 160);
        }
        break;
    case TB_CREATE: {
        static char *label[CR_COUNT] = {"Battle", "Time Limit", "Input Delay", "Rollback", "Open Room"};

        Ps_Copy(focus, label[sTb->crCur], 160);
        part[0] = 0;
        if (sTb->crCur == CR_BATTLE) {
            Ps_Copy(part, sTb->battle ? "Teams, up to 5" : "One on one", 64);
        } else if (sTb->crCur == CR_TIME) {
            Ps_Copy(part, sTbTime[sTb->time], 64);
        } else if (sTb->crCur == CR_DELAY) {
            Tb_DelayText(part, sTb->delay);
        } else if (sTb->crCur == CR_ROLL) {
            Tb_RollText(part, sTb->roll);
        }
        if (part[0] != 0) {
            Ps_Cat(focus, ": ", 160);
            Ps_Cat(focus, part, 160);
        }
        break;
    }
    case TB_JOIN:
        Tb_KeyFocus(focus, KEY_ROWS, sTbKeys, sTbJoinBtn, 3);
        Ps_Copy(now, sTb->code, 40);
        break;
    case TB_NAME:
        Tb_KeyFocus(focus, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
        Ps_Copy(now, sTb->typed, 40);
        break;
    case TB_WAIT:
        if (sTb->found) {
            Ps_Copy(focus, "Your rival is here! The match begins.", 160);
            if (Port_LobbyRivalRegion()[0] != 0) {
                Ps_Cat(focus, " Your rival is from ", 160);
                Ps_Cat(focus, (char *)Port_LobbyRivalRegion(), 160);
            }
        } else if (sTb->host == 2) {
            Ps_Copy(focus, sTb->queue ? "Quick match, teams: searching" : "Quick match, one on one: searching", 160);
        } else if (sTb->host) {
            if (Port_LobbyCode()[0] != 0) {
                Ps_Copy(focus, "Your room code: ", 160);
                Tb_Spell(part, Port_LobbyCode(), 64);
                Ps_Cat(focus, part, 160);
            } else {
                Ps_Copy(focus, "Getting a room code", 160);
            }
        } else {
            Ps_Copy(focus, "Joining room ", 160);
            Tb_Spell(part, sTb->code, 64);
            Ps_Cat(focus, part, 160);
        }
        break;
    case TB_QUICK:
        if (sTb->qkCur == 0) {
            Ps_Copy(focus, sTb->queue ? "Battle: teams" : "Battle: one on one", 160);
        } else if (sTb->qkCur == 1) {
            Ps_Copy(focus, "Region: ", 160);
            Ps_Cat(focus, sTbRegionName[sTb->region], 160);
        } else {
            Ps_Copy(focus, "Search", 160);
        }
        break;
    case TB_SPAR:
        if (sTb->spCur == 0) {
            Ps_Copy(focus, "You: ", 160);
            Ps_Cat(focus, Tb_Fighter(sTb->spMe), 160);
        } else if (sTb->spCur == 1) {
            Ps_Copy(focus, "Partner: ", 160);
            Ps_Cat(focus, Tb_Fighter(sTb->spFoe), 160);
        } else {
            Ps_Copy(focus, "Start training", 160);
        }
        break;
    case TB_BLOCK:
        Ps_Copy(focus, "Cannot go online. Online matches are only between two unmodded games.", 160);
        break;
    case TB_LOBBY:
        Tb_LobbyFocus(focus, now);
        break;
    case TB_RANKED: {
        static char *label[RK_COUNT] = {"Battle", "Search for a Rival", "Ranking", "Update Profile"};

        Ps_Copy(focus, label[sTb->rkCur], 160);
        if (sTb->rkCur == RK_BATTLE) {
            Ps_Cat(focus, sTb->rkMode ? ": team" : ": single", 160);
        }
        break;
    }
    case TB_RKWAIT:
        Ps_Copy(focus, sTbRk.state == 3 ? "Rival found. Connecting" : "Ranked: searching for a rival", 160);
        break;
    case TB_BOARD:
        Ps_Copy(focus, sTbRk.board == 2 && sTbRk.boardCount > 0 ? "First: " : "Ranking", 160);
        if (sTbRk.board == 2 && sTbRk.boardCount > 0) {
            Ps_Cat(focus, sTbRk.boardName[0], 160);
        }
        break;
    default:
        Ps_Copy(focus, "Name: ", 160);
        Ps_Cat(focus, (char *)Port_NetNameGet(), 160);
        break;
    }
    if (sTb->mode != mode) {
        static char line[224];

        mode = sTb->mode;
        Ps_Copy(line, sTbTitle[mode], 224);
        Ps_Cat(line, ". ", 224);
        Ps_Cat(line, focus, 224);
        PortA11y_Say(line, TB_SAY_INTERRUPT);
        Ps_Copy(said, focus, 160);
        Ps_Copy(typed, now, 40);
    } else { /* what was typed, then where the cursor went (one line: two in one blank, the second cuts the first) */
        static char line[256];
        s32 k;

        line[0] = 0;
        for (k = 0; now[k] == typed[k] && now[k] != 0; k++) {
        }
        if (now[k] != typed[k]) { /* the code or the name typed so far */
            Ps_Copy(typed, now, 40);
            Ps_Copy(line, sTb->mode == TB_JOIN ? "Code: " : sTb->mode == TB_LOBBY ? "Chat: " : "Name: ", 256);
            if (sTb->mode == TB_JOIN) {
                Tb_Spell(part, now, 64);
                Ps_Cat(line, now[0] != 0 ? part : "empty", 256);
            } else {
                Ps_Cat(line, now[0] != 0 ? now : "empty", 256);
            }
        }
        for (k = 0; focus[k] == said[k] && focus[k] != 0; k++) {
        }
        if (focus[k] != said[k]) {
            Ps_Copy(said, focus, 160);
            if (line[0] != 0) {
                Ps_Cat(line, ". ", 256);
            }
            Ps_Cat(line, focus, 256);
        }
        if (line[0] != 0) {
            PortA11y_Say(line, TB_SAY_INTERRUPT);
        }
    }
    if (sTb->note > 0 && !noted) { /* a note (copied, a code that did not work ...) */
        PortA11y_Say(sTb->noteLine, 0);
    }
    {   /* the lobby: what happened (who came, the chat) as it comes */
        static unsigned heard;
        static char line[200];

        if (sTb->mode == TB_LOBBY && sTbLv.state == 2 && sTbLv.logCount > 0 && sTbLv.logTotal != heard) {
            s32 e = sTbLv.logCount - 1;

            if (sTbLv.logName[e][0] != 0) {
                Ps_Copy(line, sTbLv.logName[e], 200);
                Ps_Cat(line, ": ", 200);
                Ps_Cat(line, sTbLv.logText[e], 200);
            } else {
                Tb_LobbyLine(line, 200, sTbLv.logText[e]);
            }
            PortA11y_Say(line, 0);
        }
        heard = sTbLv.logTotal;
    }
    noted = sTb->note > 0;
}

/* The hosts' line (Tb_Help) in a navy box at the foot of the screen, under the list, as wide as TB_TALK_WIDE: three
   lines at most, cut at spaces, in the menu's letters (the game's teal box took a third of a phone's screen). The
   box is a picture (Tb_Draw), its lines are letters (Tb_DrawText): sTbTalk says whether it is up. */
#define TB_TALK_SCALE 0.85f
#define TB_TALK_PITCH 24.0f
#define TB_TALK_LINES 3
static s32 sTbTalk;

static void Tb_DrawTalk(void) {
    Ps_Panel(TBO_KIT_WINDOW, Tb_TitleLeft(), TB_TALK_TOP, Tb_TitleLeft() + TB_TALK_WIDE, TB_TALK_BOTTOM, 0x80);
}

static void Tb_DrawTalkText(void) {
    static char tr[512];
    static char said[512]; /* the line cut last (it is cut again only when it changes: measuring is the fonts' work) */
    static s32 saidLang = -1;
    static char line[TB_TALK_LINES][128];
    static s32 lines;
    extern int Port_TrLine(const char *original, char *out, int size);
    char *say = Tb_Help();
    f32 x = Tb_TitleLeft() + 14.0f;
    f32 room = TB_TALK_WIDE - 28.0f;
    s32 at = 0;
    s32 k;

    if (say == NULL || say[0] == 0) {
        return;
    }
    if (saidLang == sTb->lang && strncmp(say, said, sizeof(said) - 1) == 0) {
        goto draw;
    }
    Ps_Copy(said, say, sizeof(said));
    saidLang = sTb->lang;
    lines = 0;
    if (Port_TrLine(say, tr, sizeof(tr))) {
        say = tr;
    }
    while (lines < TB_TALK_LINES) { /* word by word while the line fits (one word at least; the last line: the rest) */
        s32 end;

        while (say[at] == ' ') {
            at++;
        }
        if (say[at] == 0) {
            break;
        }
        end = at;
        for (;;) {
            s32 next = end;

            while (say[next] == ' ') {
                next++;
            }
            while (say[next] != 0 && say[next] != ' ') {
                next++;
            }
            if (next == end || next - at > 127) {
                break;
            }
            for (k = at; k < next; k++) {
                line[lines][k - at] = say[k];
            }
            line[lines][next - at] = 0;
            if (end > at && lines < TB_TALK_LINES - 1 && Ps_TextWidth(line[lines], TB_TALK_SCALE) > room) {
                break;
            }
            end = next;
        }
        line[lines][end - at] = 0;
        at = end;
        lines++;
    }
draw:
    for (k = 0; k < lines; k++) {
        f32 mid = (TB_TALK_TOP + TB_TALK_BOTTOM) * 0.5f + TB_TALK_PITCH * ((f32)k - (f32)(lines - 1) * 0.5f);

        Ps_Text(x, Ps_TextTop(mid, TB_TALK_SCALE), line[k], TB_TALK_SCALE, room, 0);
    }
}


/* What a page draws comes in from the left as it opens, as the hub's bars do (the hub moves its own: Tb_DrawHub);
   the title, the player card and the box stay where they are. The name's keypad comes in over the profile, which
   stays (on: 2 for the keypad). */
static void Tb_PageIn(s32 on) {
    s32 page = on == 2 || (on == 1 && sTb->mode != TB_HUB && sTb->mode != TB_BLOCK && sTb->mode != TB_NAME);

    Ps_SetShift(page ? (1.0f - Tb_In(0)) * -160.0f : 0.0f);
}

static void Tb_Draw(void) {
    Tb_PageIn(1);
    switch (sTb->mode) {
    case TB_HUB:
        Tb_DrawHub();
        break;
    case TB_CREATE:
        Tb_DrawCreate();
        break;
    case TB_JOIN:
        Tb_DrawJoin();
        break;
    case TB_WAIT:
        Tb_DrawWait();
        break;
    case TB_QUICK:
        Tb_DrawQuick();
        break;
    case TB_SPAR:
        Tb_DrawSpar();
        break;
    case TB_BLOCK:
        break; /* (drawn last, over the rest) */
    case TB_LOBBY:
        Tb_DrawLobby();
        break;
    case TB_RANKED:
        Tb_DrawRanked();
        break;
    case TB_RKWAIT:
        Tb_DrawRankedWait();
        break;
    case TB_BOARD:
        Tb_DrawBoard();
        break;
    default:
        Tb_DrawProfile();
        break;
    }
    Tb_PageIn(0);
    Ps_Panel(TBO_KIT_WINDOW, 300.0f, 10.0f, 504.0f, 44.0f, 0x80); /* the player card: the name alone */
    {   /* the game's box with the hosts' line (the buttons under it: Tb_DrawText); in a lobby what happened last */
        u16 *box = NULL;

        if (sTb->mode == TB_LOBBY && !sTb->lbChat) {
            box = Tb_LobbyBox();
        }
        sTbTalk = box == NULL;
        if (box != NULL) { /* (a lobby's last happenings: three lines, the game's box) */
            MsgWin_PortDrawString(box);
        } else {
            Tb_DrawTalk();
        }
    }
    if (sTb->mode == TB_BLOCK) {
        Tb_DrawBlock();
    }
}

static void Tb_DrawText(void) {
    char **title = sTbTitle;

    /* its capitals centred on the crest beside it (y 32); up to the player card */
    /* (it floats: a slow rise and fall of a few units) */
    Ps_Heading(Tb_TitleLeft() + 44.0f, Ps_TextTop(32.0f + 2.2f * sinf((f32)sTb->frames * 0.045f), 1.3f), title[sTb->mode], 1.3f, 248.0f, 0x80);
    Tb_PageIn(1);
    switch (sTb->mode) {
    case TB_HUB:
        Tb_DrawHubText();
        break;
    case TB_CREATE:
        Tb_DrawCreateText();
        break;
    case TB_JOIN:
        Tb_DrawJoinText();
        break;
    case TB_WAIT:
        Tb_DrawWaitText();
        break;
    case TB_QUICK:
        Tb_DrawQuickText();
        break;
    case TB_SPAR:
        Tb_DrawSparText();
        break;
    case TB_BLOCK:
        break;
    case TB_LOBBY:
        Tb_DrawLobbyText();
        break;
    case TB_RANKED:
        Tb_DrawRankedText();
        break;
    case TB_RKWAIT:
        Tb_DrawRankedWaitText();
        break;
    case TB_BOARD:
        Tb_DrawBoardText();
        break;
    default:
        Tb_DrawProfileText();
        break;
    }
    Tb_PageIn(0);
    if (sTbTalk) {
        Tb_DrawTalkText();
    }
    if (sTb->mode == TB_LOBBY) {
        Tb_DrawLobbyWin();
    }
    /* (no status and no title under it: "ONLINE" was not whether the game is connected, and the title is the
       screen's heading) */
    Ps_Text(322.0f, Ps_TextTop(27.0f, 0.6f), (char *)Port_NetNameGet(), 0.6f, 166.0f, 1);
    if (sTb->mode == TB_BLOCK) {
        Tb_DrawBlockText();
    }
    if (sTb->mode == TB_NAME) {
        Tb_PageIn(2);
        Tb_DrawName();
        Tb_PageIn(0);
    }
    if (sTb->note > 0) {
        s32 a = sTb->note > 16 ? 0x80 : sTb->note * 8;

        Ps_Align(1);
        Ps_TextEx(256.0f, 330.0f, sTb->noteLine, 0.52f, 480.0f, Ps_Rgba(0xFF, 0xF8, 0xB0, a), Ps_Rgba(0xFF, 0xC8, 0x30, a), Ps_Rgba(0x5C, 0x24, 0x08, a));
        Ps_Align(0);
    }
}

/* ----------------------------------------------------------------------------------------------- the frame loop */
/* From the main menu's item until triangle on the hub. Returns 1 when it was left for "Train While Waiting" (the
   room is still open; Duel_Main goes on to the Ultimate Training menu), 0 back to the main menu. */
/* The Duel menu's screen unpacked (the windows' data), while the screens run. */
static void *sTbWinRes;

static void Tb_WinInit(void) {
    void *icons;

    sTbWinRes = Sprite_Unpack(MPACK_AT(gMenuArc5, 1), NULL, NULL); /* (Duel_Main: DuelMenu_Run(1)) */
    icons = MPACK_AT(sTbWinRes, 24);
    Res_RelocateOffsets(&icons, icons, icons);
    IconWin_Init(MPACK_AT(sTbWinRes, 23), icons);
    IconWin_PortHideWords(); /* "Versus Menu" and its emblem: the screens' own title is written there */
    IconWin_Open();
    MsgWin_Init(MPACK_AT(sTbWinRes, 22), MPACK_AT(sTbWinRes, 21), 1, 0);
    MsgWin_SetText(NULL); /* the box only: the hosts' lines are the screens' own (Tb_BoxLine) */
    MsgWin_PortSameTop();
    MsgWin_Open();
}

static void Tb_WinTerm(void) {
    if (sTbWinRes == NULL) {
        return;
    }
    MsgWin_Term();
    IconWin_Term();
    Heap_Free(sTbWinRes);
    sTbWinRes = NULL;
}

s32 PortTbo_Run(void) {
    s32 train;

    sTb = Heap_Alloc(sizeof(TboScreen), 0x20, 0, 2);
    memset(sTb, 0, sizeof(TboScreen));
    sTb->hubCur = HUB_QUICK;
    sTb->hubY = Tb_HubRowY(Tb_HubRow(HUB_QUICK));
    Tb_LoadChoices();
    Port_NetNameGet();
    gPortTboSpar = 0;
    if (Port_LobbyWantsScreen()) { /* in a lobby (joined from Dragon Net Battle, or the screens left meanwhile): its page */
        Port_LobbyView(&sTbLv);
        Tb_LobbyOpen();
    } else if (Port_LobbyInBackground()) { /* back with a room open (from the main menu after training): its search again */
        Port_LobbyBackground(0);
        sTb->host = sTbBgKind;
        sTb->queue = sTbBgQueue;
        Ps_Copy(sTb->code, sTbBgCode, CODE_LEN + 1);
        sTb->waitBack = TB_HUB;
        Tb_SetMode(TB_WAIT);
    }
    DrNet_PresenceActive(DrNet_Mode() == 0); /* the players online: counted while these screens are up (Dragon Net) */
    GsDraw_ScreenScaleCap(12); /* (at most 3x here: gs_draw.c; the player's own after) */
    Port_EscQuit(0);
    Tb_WinInit();
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        s32 live;

        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        if (Port_EscTake() && !sTb->typing) {
            gPad[0].gamePressed |= PADG_TRIANGLE;
        }
        Snd_Update();
        ColorFade_Update();
        live = !sTb->leaving && ColorFade_IsInDone();
        Ps_PromptsClear();
        Port_LobbyView(&sTbLv);
        if (sTb->mode >= TB_RANKED || sTb->rkLobby) {
            DrRanked_Poll(&sTbRk);
        }
        switch (sTb->mode) {
        case TB_HUB:
            Tb_HubUpdate(live);
            break;
        case TB_RANKED:
            Tb_RankedUpdate(live);
            break;
        case TB_RKWAIT:
            Tb_RankedWaitUpdate(live);
            break;
        case TB_BOARD:
            Tb_BoardUpdate(live);
            break;
        case TB_CREATE:
            Tb_CreateUpdate(live);
            break;
        case TB_JOIN:
            Tb_JoinUpdate(live);
            break;
        case TB_WAIT:
            Tb_WaitUpdate(live);
            break;
        case TB_PROFILE:
            Tb_ProfileUpdate(live);
            break;
        case TB_NAME:
            Tb_NameUpdate(live);
            break;
        case TB_QUICK:
            Tb_QuickUpdate(live);
            break;
        case TB_SPAR:
            Tb_SparUpdate(live);
            break;
        case TB_BLOCK:
            Tb_BlockUpdate(live);
            break;
        case TB_LOBBY:
            Tb_LobbyUpdate(live);
            break;
        }
        Tb_Speak();
        Ps_PageSet();
        Tb_Backdrop();
        IconWin_Draw(); /* the clouds in the corners, the emblem, the line under the title */
        Tb_Draw();
        Tb_DrawText();
        Ps_PromptsDraw();
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        sTb->frames++;
        sTb->modeT++;
        if (sTb->note > 0) {
            sTb->note--;
        }
        if (sTb->leaving && ColorFade_IsOutDone()) {
            break;
        }
    }
    sceGsSyncPath(0, 0);
    Port_EscQuit(1);
    Port_TextMode(0);
    Tb_WinTerm();
    if (!sTb->train && (sTb->mode == TB_WAIT || sTb->mode == TB_SPAR || sTb->mode == TB_RKWAIT)) {
        Port_LobbyCancel();
    }
    if (sTb->mode == TB_RKWAIT || (sTb->rkLobby && !sTb->found)) {
        DrRanked_Cancel();
    }
    DrNet_PresenceActive(0);
    GsDraw_ScreenScaleCap(0);
    train = sTb->train;
    if (train) {
        static u8 noItems[0x10]; /* an empty set of Z items */
        s32 me = sTb->spMe;
        s32 foe = sTb->spFoe;

        Heap_Free(sTb);
        sTb = NULL;
        Dma_ResetBuffers();
        Battle_ClearWork();
        BattleSetup_SetRule(0, 6, 8, 0, 1, TB_TOURNAMENT_STAGE, 0); /* training: no time limit, a partner who stands */
        BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
        BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, NULL);
        BattleSetup_SetMember(0, 0, me, 0, 0, -1, 100.0f, noItems);
        BattleSetup_SetMember(1, 0, foe, 0, 0, -1, 100.0f, noItems);
        BattleSetup_Finish();
        gPortTboSpar = 1;
        return 1;
    }
    Heap_Free(sTb);
    sTb = NULL;
    Dma_ResetBuffers();
    return train;
}
/* The sign over the sparring fight (battle/hud_a.c, Hud_Draw, every frame): how long it has searched, what for, and
   how to go back. Nothing when the fight is not Train While Waiting's or the search is over. */
static s32 sTbSignFrames;

void PortTbo_SparSign(void) {
    static char line[64];
    s32 secs;

    if (!gPortTboSpar || !Port_LobbyInBackground()) {
        return;
    }
    sTbSignFrames++;
    secs = (sTbBgFrames + sTbSignFrames) / 60;
    Ps_PageSet();
    Ps_Panel(TBO_KIT_DIALOG, 128.0f, 66.0f, 384.0f, 124.0f, 0x70); /* (as much room under the lines as over them) */
    sprintf(line, Tb_Fmt("Searching for a rival...  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
    Ps_Msg(146.0f, 72.0f, line, 1.0f, 0x80);
    if (sTbBgKind == 2) {
        Ps_Copy(line, "Quick Match: ", 64);
        Ps_Cat(line, sTbBgQueue ? "Teams" : "One on One", 64);
    } else if (sTbBgKind == 1) {
        Ps_Copy(line, "Your room: ", 64);
        Ps_Cat(line, Port_LobbyCode(), 64);
    } else {
        Ps_Copy(line, "Joining room ", 64);
        Ps_Cat(line, sTbBgCode, 64);
    }
    Ps_Msg(146.0f, 93.0f, line, 1.0f, 0x80);
}
/* ----------------------------------------------------------------------------------------------- the spectator */
/* Watching a match (port/src/gs/net.c, "Spectators"): the line over the match, drawn over every screen of the match
   (character select, fights) from Gfx_EndFrame (sys/gfx.c). The spectator's own pause menu (START or Esc) is the
   game's animated window, as the PC's exit question (port/src/spec_menu.c); while it is up the line is not drawn. */
extern int Port_NetSpectating(void);
extern const char *Port_NetName(int player);
extern int Port_NetSpecView(void);
extern int Port_NetSpecStatus(int *out);
extern int Port_SpecLineShown(void);  /* headless.c: BT3_SPEC_LINE=0, the line left out (tests) */
extern int PortSpecMenu_Frame(int vsyncs); /* port/src/spec_menu.c: the pause menu; 1 while its window is up */

static char *sTbViewName[3] = {"PLAYER 1", "PLAYER 2", "SPLIT SCREEN"};

/* The line over the match: who plays, the view, and how to change it and open the menu; catching up, and a
   desync, when they are. At the foot of the screen, between the two gauges. */
static void Tb_SpecLine(void) {
    static char line[128];
    int st[3] = {-1, 0, 0};
    s32 known = Port_NetSpecStatus(st) != 0;
    f32 y = 382.0f;

    Ps_Panel(TBO_KIT_DIALOG, 136.0f, y, 376.0f, y + 52.0f, 0x60);
    Tb_Subst(line, 128, "Spectating %s vs %s", Port_NetName(0), Port_NetName(1), 0);
    Ps_Align(1);
    Ps_Text(256.0f, y + 4.0f, line, 0.56f, 220.0f, 1);
    if (known && st[0] >= 0) {
        sprintf(line, Tb_Fmt("CATCHING UP WITH THE MATCH... %d%%"), (int)st[0]);
        Ps_Text(256.0f, y + 26.0f, line, 0.46f, 220.0f, 5);
    } else if (known && st[2] != 0) {
        sprintf(line, Tb_Fmt("NO LONGER THE PLAYERS' FIGHT (SINCE FRAME %d)"), (int)st[2]);
        Ps_Text(256.0f, y + 26.0f, line, 0.42f, 220.0f, 4);
    } else {
        Ps_Copy(line, "VIEW: ", 128);
        Ps_Cat(line, sTbViewName[Port_NetSpecView() % 3], 128);
        Ps_Cat(line, "   R1: CHANGE   START: MENU", 128);
        Ps_Text(256.0f, y + 27.0f, line, 0.42f, 224.0f, 0);
    }
    Ps_Align(0);
}

/* Every frame of every screen (sys/gfx.c, Gfx_EndFrame): nothing of the game's touched unless this copy watches a
   match (and shows the line). */
void PortTbo_SpecDraw(s32 vsyncs) {
    if (PortSpecMenu_Frame(vsyncs) || !Port_NetSpectating() || !Port_SpecLineShown()) {
        return;
    }
    Ps_PageSet();
    Ps_SetShift(0.0f);
    Ps_Align(0);
    Tb_SpecLine();
}
#endif
