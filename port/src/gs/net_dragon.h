/* Dragon Net: online play between Dragon Rage (Android) and DBT-NX (Switch) on a service of their own (port/dragonnet),
 * apart from the PC port's: Ranked (Elo), the general chat and the count of players online. Online against the PC
 * goes on through the PC port's own service (mode 1). See net_dragon.c. */
#ifndef NET_DRAGON_H
#define NET_DRAGON_H
#ifdef __cplusplus
extern "C" {
#endif

/* The simulation version: raised only when something that decides a fight changes (not with every release of the PC
   port). Games meet only with the same netcode version AND the same simulation: the service is told
   NET_VERSION * 1000 + DRNET_SIM (5001 with the PC port's netcode 5). DBT-NX uses the same number. */
#define DRNET_SIM 1
/* The service, the official one for Android and Switch (the Tenkaichi3DecompAndroidOnline fork's Worker, running
   port/dragonnet: it takes several versions and keeps them apart). Another one: dragonnet_url.txt in the game folder,
   one line "https://...". */
#define MATCH_LIMIT_TEXT "Server busy (daily limit), try again later." /* the lobby's error then (ui_dragonnet.inc words it) */
/* A room code the service does not know: rooms of Dragon Net and of the PC port's service are apart, so a code made in
   the other mode is never found (net_match.c says it; ui_dragonnet.inc and menu/port_tbo.c translate it). */
#define MATCH_NOROOM_DN "There is no room with that code on Dragon Net. Check the code (it lasts ten minutes). A room made in PC mode does not show here: both players need the same service."
#define MATCH_NOROOM_PC "There is no room with that code on the PC service. Check the code (it lasts ten minutes). A room made on Dragon Net does not show here: both players need the same service."
int Match_ServiceLimited(void); /* a service answered HTTP 429 a moment ago: nothing is asked for now (net_match.c) */
#define DRNET_URL_DEFAULT "https://mitigo-rooms.minetivacraft.workers.dev"

int DrNet_Mode(void);          /* 0 Dragon Net (Android + Switch), 1 against PC (the PC port's service); setting "net_mode" */
void DrNet_SetMode(int mode);
int DrNet_WireVersion(int netVersion); /* what the service is told as `v` in this mode */
#define DRNET_CHAT 0 /* chat geral desligado */
const char *DrNet_UrlB(void);       /* dragonnet_url_b.txt ("" se nao existe) */
const char *DrNet_RankedUrl(void);  /* dragonnet_ranked_url.txt ("" se nao existe) */
const char *DrNet_RankedUrlD(void);      /* dragonnet_ranked_url_d.txt (https://host); padrao: D embutido */
const char *DrNet_RankedActiveUrl(void); /* Ranked: C, ou D ate 00:00 UTC se o C falhar */
int DrNet_CdLoad(char *out, int n);      /* id da autoridade E aprendido de um C verificado */
int DrNet_CdSave(const char *authority);
const char *DrNet_ActiveUrl(void);  /* A, ou B se o A falhou hoje */
void DrNet_NoteResult(const char *base, int ok);
int DrRanked_Active(void);
const char *DrNet_Url(void);   /* Dragon Net's service */

/* Players online (Dragon Net only): on while the online screen is open. -1 not known yet, -2 the service did not answer. */
void DrNet_PresenceActive(int on);
int DrNet_Online(void);

/* Messages for the window (it words them in the player's language). */
enum {
    DRMSG_NONE, DRMSG_SEARCHING, DRMSG_SEARCHING_LEVEL, DRMSG_FOUND, DRMSG_CONFIRMED, DRMSG_ENDED_NO_POINTS, DRMSG_WAIT_RIVAL,
    DRMSG_DISPUTED, DRMSG_VOID, DRMSG_NO_ANSWER, DRMSG_REGISTER_FAILED, DRMSG_PROFILE_FAILED, DRMSG_ROOM_FAILED,
    DRMSG_FINISH_FIRST, DRMSG_VERSION, DRMSG_UPDATED, DRMSG_BUSY, DRMSG_LIMIT, DRMSG_BACKGROUND_TIMEOUT
};

typedef struct DrRanked {
    int state;          /* 0 idle, 1 asking, 2 waiting for a rival, 3 rival found (paired), -1 a problem (msg) */
    int flow;            /* 2: the service negotiated the lifecycle (flow:2) with the rival */
    int searching, role, mode; /* role 1 host, 2 the one who joins; mode 0 single, 1 team */
    int profile;        /* 0 not loaded, 1 loaded */
    int rating, wins, losses, draws, position;
    char rank[20], peer[20], code[8], match[33];
    int msg;            /* DRMSG_* */
    int board;          /* 0 not asked, 1 asking, 2 there, -1 failed */
    int boardCount;
    char boardName[9][20];
    int boardRating[9];
} DrRanked;

/* flow:2 rematch, after the result (the Revanche screen): 1 asks the state, 2 accepts, 3 leaves. */
void DrRanked_FlowAct(int act);
/* 0 none, 1 confirming, 2 choice, 3 waiting, 4 declined or gave up, 5 a new match was saved. */
int DrRanked_FlowState(void);
/* The saved new match (match needs 33 bytes): 1 if there is one. */
int DrRanked_FlowNext(char *match, int *role, int *mode);
void DrRanked_FlowClear(void);
/* flow:2 rematch: the saved new match becomes a paired search again (1 = resumed). */
int DrRanked_Resume(void);
/* flow:2: should the Revanche screen come before the session ends (1 = yes). */
int DrRanked_RematchOffer(void);
void DrRanked_Open(const char *name);              /* starts the client and loads the profile */
void DrRanked_Search(const char *name, int mode);  /* 0 single, 1 team */
void DrRanked_Cancel(void);
void DrRanked_Board(void);
void DrRanked_Poll(DrRanked *out);
void DrRanked_SetRoom(const char *code);           /* the host's room code, once the lobby has it */
void DrRanked_Connection(int ready);   /* the lobby is connected (flow:2): tells the service */
void DrRanked_FightStarted(void);      /* the fight begins (flow:2): tells the service */
int DrRanked_Handover(void);                       /* the match starts: remembered for its result (1 = it was a ranked one) */
int DrRanked_InSession(void);

/* The battle's end (battle_load.c, BattleResult_Finish) and the end of an online session (net.c, Port_NetLeave). */
void DrNet_BattleFinished(int flags, int reason, int clock);
void DrNet_SessionEnding(int mine);

/* The general chat. */
typedef struct DrChat {
    int state;          /* 0 off, 1 connecting, 2 there, -1 no answer */
    int count;
    char name[24][24];
    char text[24][168];
    int mine[24];
    int sending;
} DrChat;
void DrChat_Open(const char *name);
void DrChat_Close(void);
void DrChat_Send(const char *text);
void DrChat_Poll(DrChat *out);

#ifdef __cplusplus
}
#endif
#endif
