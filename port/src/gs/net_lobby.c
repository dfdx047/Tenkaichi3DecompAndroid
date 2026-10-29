/*
 * Lobbies: up to eight games meet at one, the lobby's owner (docs/port/README.md, "Lobbies").
 *
 * The owner's game is the lobby. It keeps eight places, two for the players and six for spectators, the rules of the
 * match, and who is ready; every member's game talks to it alone, over the socket the match will use (by address,
 * or with a room code: the matchmaking service only tells the members where the owner is, and keeps the code while
 * the owner asks for it). When the owner starts a match, each member is told what to be in it (T_LOBBY_GO): the
 * game in the first place hosts the match, the one in the second joins it, everyone else watches it as a spectator
 * of the owner, whose game passes the match on to them whatever its own part is (the host's input as the host has
 * it, gs/net.c, "Spectators"). The match is the usual session (the rules are the lobby's and cannot change in it).
 * When the owner goes back to the lobby, or its session ends for any reason, every member's session ends too
 * (T_LOBBY_BACK) and all are in the lobby window again; a member's own session that ends (its connection, the
 * other player leaving) brings it back as well, and the owner then ends the match for all if it was a player's.
 *
 * Packets (types 30 to 40; a release without lobbies ignores them; all words little-endian as in net.c):
 *   HELLO   member -> owner   {LOBBY_VERSION, NET_VERSION, fingerprint mark, low, high, nonce, own public ip, port, name}
 *   NO      owner -> member   {why, nonce}: another version, other game data, the lobby is full
 *   STATE   owner -> member   the whole lobby, every second and at each change (also the owner's heartbeat)
 *   ALIVE   member -> owner   {id, state seen, ready, last match it was sent to, the match it is in, connected, the match
 *                             whose result it has, the result}, every second
 *   GO      owner -> member   {match, role, rules, rollback, delay, 4 addresses}: begin that match's session
 *   BACK    owner -> member   {match, note}: the match is over, back to the lobby window
 *   LEAVE   member -> owner   {id}: this member leaves the lobby
 *   CLOSE   owner -> member   {why, note}: the lobby is closed, or this member was removed
 *   CHAT    member -> owner   {id, number, length, text}; the owner passes each line on to all (SAID)
 *   SAID    owner -> member   {number, length, name, text}
 *   INVITE  owner -> a game that knocked with the greeting of a match of two (by address): "this is a lobby"
 *   TOKEN   owner -> one member {tournament, 1 keep / 2 forget, entrant, token, code}: its own rejoin token (below)
 * LOBBY_VERSION 3 (tournaments, all of it one step after 0.1.38.9's 2): STATE carries the lobby's mode and the tournament's bracket after the words it had
 * (620 bytes), ALIVE the result of the last match this member was in, HELLO a rejoin token (a game of version 2 is told the versions differ).
 *
 * Tournaments (docs/port/README.md, "Tournaments"): the owner switches the lobby to Tournament (the first rule) and
 * draws a bracket of those in the player places, of which the Entrants rule makes 2 to 8 (empty ones are left out;
 * the rest of the eight are the spectators'): their order is shuffled from a number the owner draws (the
 * system's random source; sent with the bracket), with byes where the entrants are not 2, 4 or 8. The owner's game
 * holds the bracket and sends it with the lobby (STATE). Each match is a lobby's match: the owner puts its two in 1P
 * and 2P and starts it, everyone else watches. Every copy in the match notes how its first battle ended (the battle
 * sequence past the fight, held for 0.3 s: result_watch) and a player says it with each ALIVE; the owner, when the two
 * players agree on a winner, marks it, says so and ends the match (everyone back to the lobby, the bracket advanced).
 * Two different results, a draw, or a match that ends without one (the owner went back, the players could not connect,
 * one result and not the other within 20 s): the match is played again; the second time the two games name different
 * winners the owner chooses the winner instead (TOUR_CHOOSE, final). An entrant who leaves the lobby loses its
 * match now or next by walkover; one whose game falls silent (a lost connection, a crash) keeps its place for 90 s
 * (HOLD_NS): it gets back in by joining with the code again, its greeting carrying the entrant's rejoin token. The
 * owner draws a token (128 bits, the system's random source) for each entrant and sends it to that member alone
 * (TOKEN, with every STATE), which keeps it in memory and in lobby_rejoin.txt beside its settings file (a crash or a
 * start again keeps it). A greeting with the token of an entrant whose place is held (or whose member is still there
 * but silent: dropped at once) puts the newcomer in that entrant's place and gives it a new token; the match the drop
 * broke is played again from its start. The owner forgets a token when its entrant is out, leaves on purpose, runs
 * out of the 90 s, rejoins (a new one), and when the tournament ends; it then tells the member to forget it
 * (TOKEN 2), as it does when the lobby closes (CLOSE). A member also forgets it when it leaves on purpose, when the
 * lobby it greets does not answer, and at start when the file is older than an hour.
 * A game of 0.1.38.7 or older that knocks by address is answered as a host of another release would answer it (it
 * says so); with the room code the matchmaking service tells it to update (port/matchmaking).
 *
 * Everything here runs on the game's thread: outside a session from Port_VBlank (Lobby_Pump), in one from net.c's
 * look at the socket (Lobby_Packet, Lobby_Tick). The window reads a copy (Port_LobbyView) and leaves its wishes in a
 * queue (Port_LobbyDo), both under a spin lock.
 *
 * Testing: BT3_LOBBY=lobby (a lobby with a room code), lobby:<port> (on a port), lobbyjoin:<address>:<port>; a
 * code of a lobby with BT3_LOBBY=join:<code>. BT3_LOBBY_DO=<what>|<what>|...: one after the other, each
 * "<seconds>:<command>" (that long after the one before was done): start, back, leave, quit (the program ends), kick <place>,
 * move <place> <place>, rule <rule> <step>, ready, say <text>, wait <members> (until that many are there),
 * waitmatch (until a match is on), waitlobby (until none is); tournaments: waitready <n> (until n places are ready),
 * tour (a tournament drawn), next (its next match started, once it can be), choose <side> (once the owner is asked
 * to choose a match's winner: 0 or 1), waittour (until it is over); BT3_TEST_RESULT_LIE=1: a member reports the other
 * side as the winner. The lobby's
 * changes are written to the log.
 */
#ifdef _WIN32
#define _CRT_RAND_S /* (rand_s: the system's random numbers, RtlGenRandom) */
#endif
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include "net_lobby.h"
#include "net_match.h"
#include "net_relay.h"

#define MAGIC 0x4E335442u
enum { T_HELLO = 1, T_HELLO_ACK = 2, T_SPEC_HELLO = 20, T_SPEC_NO = 22, SPEC_NO_VERSION = 1 };
enum { T_L_HELLO = 30, T_L_NO, T_L_STATE, T_L_ALIVE, T_L_GO, T_L_BACK, T_L_LEAVE, T_L_CLOSE, T_L_CHAT, T_L_SAID, T_L_INVITE, T_L_TOKEN };
enum { LOBBY_VERSION = 3, /* 2: STATE carries the member's nonce (its greeting's), which a member connecting asks for; 3: tournaments (the bracket in STATE, results in ALIVE, the rejoin token in HELLO, TOKEN) */ FP_MARK = 0x46505231, LOBBY_CAP = 0x4C4F4231 /* "1BOL": a game's 1v1 greeting says it knows lobbies */ };
enum { LNO_VERSION = 1, LNO_DATA = 2, LNO_FULL = 3 };
enum { CLOSE_CLOSED = 1, CLOSE_KICKED = 2 };
#define NOTE 96
#define NAMEB 20
#define TEXT 160
#define SEC 1000000000ull
#define QUIET_NS (15 * SEC)    /* a member, or the owner, silent this long is gone */
#define CONNECT_NS (25 * SEC)  /* a member that has not reached the owner in this long gives up */
#define MATCH_CONNECT_NS (40 * SEC) /* the two players have not connected to each other in this long: back to the lobby */
#define RESULT_WAIT_NS (20 * SEC)  /* a tournament's match: one player's result and not the other's this long: played again */
#define STATE_BYTES 620            /* STATE: the lobby (372 bytes), then the mode and the bracket (tour_put) */
#define HOLD_NS (90 * SEC)         /* a tournament's entrant whose game fell silent keeps its place this long (a rejoin) */
#define TOKEN 16                   /* a rejoin token's bytes */
#define HELLO_BYTES (40 + NAMEB + 4 + TOKEN) /* (the token after the name, with its tournament's number) */
#define TOUR LOBBY_TOUR

extern int NetIo_Version(void);
extern void Port_UiNotice(const char *text);
extern int Port_SessionCan(void);

typedef struct {
    int on;
    uint32_t id;
    struct sockaddr_in addr;   /* where it is, as this side sees it */
    struct sockaddr_in self;   /* its public address as it found it out itself (STUN; port 0: not known) */
    char name[NAMEB];
    uint32_t nonce;
    int ready;
    uint64_t heardNs;
    uint32_t goGen, inGen;     /* what it says: the last match it was sent to, the match it is in (0 none) */
    int connected;             /* ... and whether its session has its other side */
    int wasIn;                 /* it has said it is in the present match */
    uint64_t goSentNs, joinedNs;
    uint32_t chatSeq;
    int claimed;               /* (room codes: it is known to be one of the members the service told of) */
    int tokFor;                /* the entrant whose token it was sent + 1 (0 none), */
    int forgetLeft;            /* ... and how many more times it is told to forget it */
    uint32_t resGen;           /* what it says: the match whose result it has, */
    int res;                   /* ... and the result (RES_) */
} Member;

/* How the first battle of a match ended, as a copy saw it. */
enum { RES_NONE = 0, RES_SIDE0 = 1, RES_SIDE1 = 2, RES_NO_WINNER = 3 /* a draw, or the battle was left */ };
enum { HOW_BYE = 1, HOW_WALKOVER = 2, HOW_PLAYED = 3, HOW_CHOSEN = 4 /* the owner chose the winner (the games disagreed twice) */ };

/* A tournament's bracket (the owner draws it and keeps it; a member has the owner's word of it). */
typedef struct {
    int state;                 /* 0 none, 1 on, 2 over */
    uint32_t gen, seed;
    int size, count;           /* leaves (2, 4, 8), entrants */
    uint32_t id[TOUR];         /* the entrants: their members' ids, */
    char name[TOUR][NAMEB];    /* ... names, */
    int gone[TOUR];            /* ... and whether they left */
    int leaf[TOUR];            /* the entrant at each leaf, -1 a bye */
    int win[TOUR - 1], how[TOUR - 1]; /* each match's winner (-1 not yet) and how (HOW_) */
    int cur, champ;            /* the next match (-1 none), the champion (-1 none yet) */
    int playing;               /* the match on now (-1 none) */
    int choose;                /* the match whose winner the owner chooses (its two games disagreed twice), -1 none */
    int disagree[TOUR - 1];    /* the owner: how often each match's two games reported different winners */
    int side[2];               /* ... its entrants on sides 0 (1P, the host) and 1 */
    uint64_t oneResNs;         /* ... when one player's result came and not the other's (0 not yet) */
    uint8_t tok[TOUR][TOKEN];  /* the owner: each entrant's rejoin token (memory only), */
    int hasTok[TOUR];
    uint64_t dropNs[TOUR];     /* ... when its game was found silent (0: not; its place held for HOLD_NS) */
    int dropPlace[TOUR];       /* ... and the place it had then */
    int dropSecs[TOUR];        /* both: the seconds of the hold left (0 none), as the bracket shows them */
} Tour;

/* A member: its rejoin token, kept over lobbies and in a file over a start again of the program (tok_file). */
static struct {
    int has;
    uint32_t gen;
    uint8_t tok[TOKEN];
    char code[8];
    int loaded;
} sTok;
static char sJoinCode[8];      /* the code this member joined with ("" by address) */

typedef struct {
    int kind;
    int a, b;
    char text[TEXT + 1];
    char addrs[MATCH_ADDRS][64]; /* (a join found by the 1v1 lobby: the owner's addresses, this game's own) */
    int count;
    char self[64];
    struct sockaddr_in owner;
} Cmd;
enum { CMD_DO = 1, CMD_SAY, CMD_CREATE, CMD_JOIN_ADDR, CMD_JOIN_FOUND };

static struct {
    int on;                    /* 0 none, 1 owner, 2 member */
    int state;                 /* the window's: 1 connecting, 2 in the lobby, 3 closed */
    char error[256];
    char code[8];
    int port;
    /* the lobby (the owner keeps it; a member has the owner's word of it) */
    uint32_t ids[LOBBY_SLOTS]; /* who is in each place (0 nobody) */
    int ready[LOBBY_SLOTS];
    char names[LOBBY_SLOTS][NAMEB];
    uint32_t ownerId, myId;
    int battle, time, speed, roll, delay;
    int mode;                  /* 0 versus, 1 tournament */
    int entrants;              /* a tournament's player places (the Entrants rule, 2..8): places 0..entrants-1 */
    Tour t;
    int phase;
    uint32_t gen;
    char note[NOTE];
    uint32_t noteSeq;
    /* the owner */
    Member m[LOBBY_SLOTS];     /* m[0]: the owner itself */
    int slot[LOBBY_SLOTS];     /* the member in each place, -1 none */
    uint32_t nextId, seq;
    int players[2];
    uint64_t goNs, stateNs;
    int dirty;
    int byCode, watchGen, relayGen, relayAsked;
    struct { uint32_t ip; uint64_t since; } pend[16];
    uint32_t told[64];         /* (room codes: the members the service has told of, by a hash of their first address) */
    int toldCount;
    /* a member */
    struct sockaddr_in owner;
    struct sockaddr_in cand[MATCH_ADDRS + 1];
    int candCount, candRelay, candGen;
    uint64_t candSince, helloNs, heardNs, aliveNs;
    int probe;                 /* a member joining a typed address: it also asks as a spectator would (probe_send) */
    uint64_t probeNs;
    uint64_t otherNs;          /* the owner: when someone of another version last tried to join (said once a minute) */
    uint32_t nonce, seqSeen, goGen, inGen, backGen;
    char self[64];
    /* both */
    int leaving, closing, closedBy; /* closedBy: the owner closed it / removed this member (CLOSE) */
    int wantReady;             /* a member: READY asked for, until the owner's word says it (-1 none) */
    uint32_t chatOut;          /* a member: its lines so far; the owner: the lobby's */
    uint32_t chatIn;           /* a member: the owner's last line taken */
    struct { char name[NAMEB]; char text[TEXT + 1]; } chat[LOBBY_CHAT];
    int chatCount;
    struct { char name[NAMEB]; char text[TEXT + 1]; } log[LOBBY_LOG]; /* the notes and the chat, in order (the screen's) */
    int logCount;
    uint32_t logTotal;
    uint64_t lastView;
    /* both: how this copy saw the first battle of its match end (result_watch) */
    uint32_t resGen;
    int res;
    int resSawFight, resCand;
    uint64_t resCandNs;
} sL;

static SDL_SpinLock sLock;
static PortLobbyView sView;
static Cmd sQueue[16];
static int sQueueN;
extern const char *Port_FileRoot(void);

/* Keep the diagnostic [lobby] lines beside the game, including on Android where stderr is hard to retrieve. */
static void lobby_debug(const char *fmt, ...) {
    static char lines[200][1024];
    static int count, loaded;
    char line[1024];
    va_list ap;
    FILE *f;
    const char *base;
    char path[1024];
    int i;
    va_start(ap, fmt);
    SDL_vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    fputs(line, stderr);
    if (!loaded) {
        loaded = 1;
        base = Port_FileRoot();
        if (base != NULL && SDL_snprintf(path, sizeof(path), "%s/lobby_log.txt", base) < (int)sizeof(path) &&
            (f = fopen(path, "r")) != NULL) {
            char old[sizeof(lines[0])];
            while (fgets(old, sizeof(old), f) != NULL) {
                if (count < 200) {
                    SDL_strlcpy(lines[count++], old, sizeof(lines[0]));
                } else {
                    memmove(lines, lines + 1, sizeof(lines[0]) * 199);
                    SDL_strlcpy(lines[199], old, sizeof(lines[199]));
                }
            }
            fclose(f);
        }
    }
    base = Port_FileRoot();
    if (base == NULL || SDL_snprintf(path, sizeof(path), "%s/lobby_log.txt", base) >= (int)sizeof(path)) {
        return;
    }
    if (count < 200) {
        SDL_strlcpy(lines[count++], line, sizeof(lines[0]));
        if ((f = fopen(path, "a")) != NULL) {
            fputs(line, f);
            fflush(f);
            fclose(f);
        }
    } else {
        memmove(lines, lines + 1, sizeof(lines[0]) * 199);
        SDL_strlcpy(lines[199], line, sizeof(lines[199]));
        if ((f = fopen(path, "w")) != NULL) {
            for (i = 0; i < 200; i++) {
                fputs(lines[i], f);
            }
            fflush(f);
            fclose(f);
        }
    }
}

static int same(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

static uint64_t now_ns(void) {
    return SDL_GetTicksNS();
}

static void put(uint8_t *p, int word, uint32_t v) {
    memcpy(p + word * 4, &v, 4);
}

static uint32_t get(const uint8_t *p, int word) {
    uint32_t v;
    memcpy(&v, p + word * 4, 4);
    return v;
}

static void name_clean(char *s, int size) {
    int i;
    s[size - 1] = '\0';
    for (i = 0; s[i] != '\0'; i++) {
        if ((unsigned char)s[i] < 0x20 || s[i] == 0x7F) {
            s[i] = '?';
        }
    }
}

static void send_to(const void *buf, int n, const struct sockaddr_in *to) {
    if (NetIo_Sock() >= 0 && to->sin_port != 0) {
        NetIo_Send(buf, n, to);
    }
}

static const char *rules_text(void) {
    static char t[96];
    static const char *const kBattle[] = {"single battle", "team battle", "DP battle (10)", "DP battle (15)", "DP battle (20)"};
    static const char *const kTime[] = {"60 s", "90 s", "180 s", "240 s", "no limit"};
    static char m[32];
    SDL_snprintf(m, sizeof(m), "tournament of %d, ", sL.entrants);
    SDL_snprintf(t, sizeof(t), "%s%s, %s, %d%%, rollback %d, delay %s%d", sL.mode == 1 ? m : "", kBattle[sL.mode == 1 ? 0 : sL.battle], kTime[sL.time],
                 sL.speed, sL.roll, sL.delay == 0 ? "auto" : "", sL.delay == 0 ? 0 : sL.delay - 1);
    return t;
}

static void log_push(const char *name, const char *text);

/* A number nobody can guess, from the system (a member's nonce: the owner's first answer is known by it). */
static uint32_t os_random32(void) {
    uint32_t v = 0;
#ifdef _WIN32
    unsigned int r;
    if (rand_s(&r) == 0) {
        return (uint32_t)r;
    }
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f != NULL) {
        size_t got = fread(&v, 1, sizeof(v), f);
        fclose(f);
        if (got == sizeof(v)) {
            return v;
        }
    }
#endif
    v = (uint32_t)SDL_rand_bits() ^ (uint32_t)SDL_GetPerformanceCounter(); /* (only if the system had none to give) */
    return v;
}

static void note_set(const char *text) {
    log_push("", text);
    SDL_strlcpy(sL.note, text, sizeof(sL.note));
    sL.noteSeq++;
    sL.dirty = 1;
    lobby_debug("[lobby] %s\n", text);
}

/* A line of what happened, for the screen: a note (name "") or a line of chat. */
static void log_push(const char *name, const char *text) {
    if (sL.logCount == LOBBY_LOG) {
        memmove(&sL.log[0], &sL.log[1], sizeof(sL.log[0]) * (LOBBY_LOG - 1));
        sL.logCount--;
    }
    SDL_strlcpy(sL.log[sL.logCount].name, name, NAMEB);
    SDL_strlcpy(sL.log[sL.logCount].text, text, TEXT + 1);
    sL.logCount++;
    sL.logTotal++;
}

/* ---- the bracket (both: the owner keeps it, a member reads the owner's) ---- */

static int t_matches(void) {
    return sL.t.size - 1;
}

static int t_round(int m) { /* the round match m is in (0 the first) */
    int r = 0;
    while (r < 2 && m >= sL.t.size - (sL.t.size >> (r + 1))) {
        r++;
    }
    return r;
}

/* Match m's side s (0, 1): an entrant, -1 not known yet, -2 a bye. */
static int t_part(int m, int s) {
    int r = t_round(m), i = m - (sL.t.size - (sL.t.size >> r));
    if (r == 0) {
        return sL.t.leaf[2 * i + s] < 0 ? -2 : sL.t.leaf[2 * i + s];
    }
    return sL.t.win[sL.t.size - (sL.t.size >> (r - 1)) + 2 * i + s];
}

/* The match to play next: the first not decided whose two are known. */
static int t_next(void) {
    int m;
    for (m = 0; m < t_matches(); m++) {
        if (sL.t.win[m] < 0 && t_part(m, 0) >= 0 && t_part(m, 1) >= 0) {
            return m;
        }
    }
    return -1;
}

/* ---- the window's copy ---- */

static void view_update(int force) {
    uint64_t now = now_ns();
    int i;
    if (!force && now - sL.lastView < 50000000ull) {
        return;
    }
    sL.lastView = now;
    SDL_LockSpinlock(&sLock);
    if (sL.on == 0 && sL.state != 3) {
        memset(&sView, 0, sizeof(sView));
        SDL_UnlockSpinlock(&sLock);
        return;
    }
    sView.state = sL.on == 0 ? 3 : sL.state;
    sView.owner = sL.on == 1;
    sView.me = -1;
    sView.phase = sL.phase;
    sView.inMatch = NetIo_SessionMode() != 0;
    SDL_strlcpy(sView.code, sL.code, sizeof(sView.code));
    sView.port = sL.port;
    for (i = 0; i < LOBBY_SLOTS; i++) {
        sView.taken[i] = sL.ids[i] != 0;
        sView.ready[i] = sL.ready[i];
        sView.isOwner[i] = sL.ids[i] != 0 && sL.ids[i] == sL.ownerId;
        SDL_strlcpy(sView.name[i], sL.ids[i] != 0 ? sL.names[i] : "", LOBBY_NAME);
        if (sL.ids[i] != 0 && sL.ids[i] == sL.myId) {
            sView.me = i;
        }
    }
    sView.battle = sL.battle;
    sView.time = sL.time;
    sView.speed = sL.speed;
    sView.rollback = sL.roll;
    sView.delay = sL.delay;
    SDL_strlcpy(sView.note, sL.note, sizeof(sView.note));
    sView.noteSeq = sL.noteSeq;
    SDL_strlcpy(sView.error, sL.error, sizeof(sView.error));
    sView.chatCount = sL.chatCount;
    for (i = 0; i < sL.chatCount; i++) {
        SDL_strlcpy(sView.chatName[i], sL.chat[i].name, LOBBY_NAME);
        SDL_strlcpy(sView.chatText[i], sL.chat[i].text, sizeof(sView.chatText[i]));
    }
    sView.logCount = sL.logCount;
    sView.logTotal = sL.logTotal;
    for (i = 0; i < sL.logCount; i++) {
        SDL_strlcpy(sView.logName[i], sL.log[i].name, LOBBY_NAME);
        SDL_strlcpy(sView.logText[i], sL.log[i].text, sizeof(sView.logText[i]));
    }
    sView.mode = sL.mode;
    sView.tour = sL.t.size >= 2 ? sL.t.state : 0;
    sView.tourGen = sL.t.gen;
    sView.tourSize = sView.tour != 0 ? sL.t.size : 0;
    sView.tourCount = sView.tour != 0 ? sL.t.count : 0;
    sView.tourMe = -1;
    sView.readyCount = 0;
    sView.entrants = sL.mode == 1 && sL.entrants >= 2 && sL.entrants <= TOUR ? sL.entrants : 2;
    sView.entrantCount = 0;
    for (i = 0; i < LOBBY_SLOTS; i++) {
        sView.readyCount += sL.ids[i] != 0 && sL.ready[i];
        sView.entrantCount += sL.ids[i] != 0 && i < sView.entrants;
    }
    for (i = 0; i < TOUR; i++) {
        int on = sView.tour != 0 && i < sL.t.count;
        SDL_strlcpy(sView.tourName[i], on ? sL.t.name[i] : "", LOBBY_NAME);
        sView.tourGone[i] = on && sL.t.gone[i];
        sView.tourDrop[i] = on && !sL.t.gone[i] ? sL.t.dropSecs[i] : 0;
        sView.tourLeaf[i] = sView.tour != 0 && i < sL.t.size ? sL.t.leaf[i] : -1;
        if (on && sL.t.id[i] == sL.myId && sL.myId != 0) {
            sView.tourMe = i;
        }
    }
    for (i = 0; i < TOUR - 1; i++) {
        int on = sView.tour != 0 && i < t_matches();
        sView.tourA[i] = on ? t_part(i, 0) : -1;
        sView.tourB[i] = on ? t_part(i, 1) : -1;
        sView.tourWin[i] = on ? sL.t.win[i] : -1;
        sView.tourHow[i] = on ? sL.t.how[i] : 0;
    }
    sView.tourChoose = sView.tour == 1 ? sL.t.choose : -1;
    sView.tourCur = sView.tour == 1 ? sL.t.cur : -1;
    sView.tourOn = sView.tour == 1 && sL.phase == 1 ? sL.t.playing : -1;
    sView.tourChamp = sView.tour == 2 ? sL.t.champ : -1;
    SDL_UnlockSpinlock(&sLock);
}

int Port_LobbyView(PortLobbyView *out) {
    SDL_LockSpinlock(&sLock);
    *out = sView;
    SDL_UnlockSpinlock(&sLock);
    return out->state != 0;
}

static void queue_put(const Cmd *c) {
    SDL_LockSpinlock(&sLock);
    if (sQueueN < (int)(sizeof(sQueue) / sizeof(sQueue[0]))) {
        sQueue[sQueueN++] = *c;
    }
    SDL_UnlockSpinlock(&sLock);
}

void Port_LobbyDo(int what, int a, int b) {
    Cmd c;
    memset(&c, 0, sizeof(c));
    c.kind = CMD_DO;
    c.a = what;
    c.b = a;
    c.text[0] = (char)b; /* (the second number, -1 / +1 / 0..7: one byte) */
    queue_put(&c);
}

void Port_LobbySay(const char *text) {
    Cmd c;
    memset(&c, 0, sizeof(c));
    c.kind = CMD_SAY;
    SDL_strlcpy(c.text, text, sizeof(c.text));
    queue_put(&c);
}

int Port_LobbyCreate(int byCode, int port, int battle, int time, int speed, int rollback, int delay) {
    Cmd c;
    memset(&c, 0, sizeof(c));
    c.kind = CMD_CREATE;
    c.a = byCode;
    c.b = port;
    c.text[0] = (char)battle;
    c.text[1] = (char)time;
    c.text[2] = (char)speed;
    c.text[3] = (char)rollback;
    c.text[4] = (char)delay;
    SDL_LockSpinlock(&sLock); /* (shown as under way at once) */
    memset(&sView, 0, sizeof(sView));
    sView.state = 1;
    sView.owner = 1;
    sView.port = byCode ? 0 : port; /* (the page says "opening the port" meanwhile) */
    SDL_UnlockSpinlock(&sLock);
    queue_put(&c);
    return 1;
}

int Port_LobbyJoin(const char *address, int port) {
    Cmd c;
    memset(&c, 0, sizeof(c));
    c.kind = CMD_JOIN_ADDR;
    c.a = port;
    SDL_strlcpy(c.text, address, sizeof(c.text));
    SDL_LockSpinlock(&sLock);
    memset(&sView, 0, sizeof(sView));
    sView.state = 1;
    SDL_UnlockSpinlock(&sLock);
    queue_put(&c);
    return 1;
}

int Port_LobbyWantsScreen(void) {
    int state;
    SDL_LockSpinlock(&sLock);
    state = sView.state;
    SDL_UnlockSpinlock(&sLock);
    return state != 0 && NetIo_SessionMode() == 0 && !NetIo_SessionStarting();
}

int Lobby_Active(void) {
    return sL.on != 0;
}

int Lobby_InMatch(void) {
    return sL.on != 0 && NetIo_SessionMode() != 0;
}

int Lobby_ServesSpectators(void) {
    return sL.on == 1 && NetIo_SessionMode() != 0;
}

int Lobby_IsMember(const struct sockaddr_in *from) {
    int i;
    if (sL.on == 2) {
        return same(from, &sL.owner);
    }
    for (i = 1; i < LOBBY_SLOTS; i++) {
        if (sL.m[i].on && same(&sL.m[i].addr, from)) {
            return 1;
        }
    }
    return 0;
}

/* ---- the owner ---- */

static int member_place(int k) {
    int s;
    for (s = 0; s < LOBBY_SLOTS; s++) {
        if (sL.slot[s] == k) {
            return s;
        }
    }
    return -1;
}

static void table_make(void) { /* the owner's places into the table the window and the members see */
    int s;
    for (s = 0; s < LOBBY_SLOTS; s++) {
        int k = sL.slot[s];
        sL.ids[s] = k >= 0 ? sL.m[k].id : 0;
        sL.ready[s] = k >= 0 ? sL.m[k].ready : 0;
        SDL_strlcpy(sL.names[s], k >= 0 ? sL.m[k].name : "", NAMEB);
    }
    sL.ownerId = sL.m[0].id;
    sL.myId = sL.m[0].id;
    sL.dirty = 1;
}

static void log_table(void) {
    char line[512];
    size_t at = 0;
    int s;
    for (s = 0; s < LOBBY_SLOTS; s++) {
        at += (size_t)SDL_snprintf(line + at, sizeof(line) - at, "%s%s=%s%s", s == 0 ? "" : ", ", s == 0 ? "P1" : s == 1 ? "P2" : "S", sL.ids[s] != 0 ? sL.names[s] : "-",
                                   sL.ready[s] ? "(ready)" : "");
        if (at >= sizeof(line)) {
            break;
        }
    }
    lobby_debug("[lobby] places %s; rules %s\n", line, rules_text());
}

/* The mode and the bracket, in STATE after the lobby (bytes 372 on): words 93 mode | state << 8 | size << 16 |
   count << 24, 94 the tournament's number, 95 the shuffle's seed, 96 the next match, the champion and the match on
   (a byte each, 0xFF none) and the Entrants rule, 97..104 the entrants' ids; their names from byte 420 (8 x 20), the leaves from 580 (8
   bytes, 0xFF a bye), the matches' winners from 588 (7) and how from 595 (7), at 602 who left (a bit each), from 608 the seconds left of a held place (8), at 616 the match whose winner the owner
   chooses. */
static void tour_put(uint8_t *p) {
    int i;
    put(p, 93, (uint32_t)sL.mode | (uint32_t)sL.t.state << 8 | (uint32_t)sL.t.size << 16 | (uint32_t)sL.t.count << 24);
    put(p, 94, sL.t.gen);
    put(p, 95, sL.t.seed);
    put(p, 96, (uint32_t)(uint8_t)sL.t.cur | (uint32_t)(uint8_t)sL.t.champ << 8 | (uint32_t)(uint8_t)(sL.phase == 1 ? sL.t.playing : -1) << 16 |
                   (uint32_t)(uint8_t)sL.entrants << 24);
    for (i = 0; i < TOUR; i++) {
        put(p, 97 + i, sL.t.id[i]);
        memcpy(p + 420 + i * NAMEB, sL.t.name[i], NAMEB);
        p[580 + i] = (uint8_t)sL.t.leaf[i];
        p[602] |= (uint8_t)(sL.t.gone[i] ? 1 << i : 0);
    }
    for (i = 0; i < TOUR - 1; i++) {
        p[588 + i] = (uint8_t)sL.t.win[i];
        p[595 + i] = (uint8_t)sL.t.how[i];
    }
    for (i = 0; i < TOUR; i++) {
        p[608 + i] = (uint8_t)(sL.t.dropSecs[i] < 255 ? sL.t.dropSecs[i] : 255); /* (a held place's seconds left) */
    }
    p[616] = (uint8_t)sL.t.choose; /* (the match whose winner the owner chooses, 0xFF none) */
}

static void tour_get(const uint8_t *p) {
    uint32_t w = get(p, 93), x = get(p, 96);
    int i, size = (int)(w >> 16 & 0xFF), count = (int)(w >> 24 & 0xFF);
    sL.mode = (int)(w & 0xFF) == 1;
    sL.entrants = (int)(x >> 24 & 0xFF) >= 2 && (int)(x >> 24 & 0xFF) <= TOUR ? (int)(x >> 24 & 0xFF) : 2;
    if (!(size == 2 || size == 4 || size == 8) || count < 2 || count > size) {
        memset(&sL.t, 0, sizeof(sL.t));
        sL.t.cur = sL.t.champ = sL.t.playing = sL.t.choose = -1;
        return;
    }
    sL.t.state = (int)(w >> 8 & 0xFF) % 3;
    sL.t.size = size;
    sL.t.count = count;
    sL.t.gen = get(p, 94);
    sL.t.seed = get(p, 95);
    sL.t.cur = (int)(int8_t)(uint8_t)(x & 0xFF);
    sL.t.champ = (int)(int8_t)(uint8_t)(x >> 8 & 0xFF);
    sL.t.playing = (int)(int8_t)(uint8_t)(x >> 16 & 0xFF);
    for (i = 0; i < TOUR; i++) {
        int leaf = (int)(int8_t)p[580 + i];
        sL.t.id[i] = get(p, 97 + i);
        memcpy(sL.t.name[i], p + 420 + i * NAMEB, NAMEB);
        name_clean(sL.t.name[i], NAMEB);
        sL.t.leaf[i] = leaf >= 0 && leaf < count ? leaf : -1;
        sL.t.gone[i] = p[602] >> i & 1;
    }
    for (i = 0; i < TOUR - 1; i++) {
        int win = (int)(int8_t)p[588 + i];
        sL.t.win[i] = win >= 0 && win < count ? win : -1;
        sL.t.how[i] = p[595 + i] <= HOW_CHOSEN ? p[595 + i] : 0;
    }
    for (i = 0; i < TOUR; i++) {
        sL.t.dropSecs[i] = p[608 + i];
    }
    sL.t.choose = (int)(int8_t)p[616] >= 0 && (int)(int8_t)p[616] < size - 1 ? (int)(int8_t)p[616] : -1;
    if (sL.t.cur >= size - 1) { sL.t.cur = -1; }
    if (sL.t.champ >= count) { sL.t.champ = -1; }
    if (sL.t.playing >= size - 1) { sL.t.playing = -1; }
}

static void state_send(int k) {
    uint8_t p[STATE_BYTES];
    int s;
    memset(p, 0, sizeof(p));
    put(p, 0, MAGIC);
    put(p, 1, T_L_STATE);
    put(p, 2, sL.seq);
    put(p, 3, sL.m[k].id);
    put(p, 4, sL.m[0].id);
    put(p, 5, (uint32_t)sL.phase);
    put(p, 6, sL.gen);
    put(p, 7, (uint32_t)sL.battle | (uint32_t)sL.time << 8 | (uint32_t)sL.speed << 16);
    put(p, 8, (uint32_t)sL.roll | (uint32_t)sL.delay << 8);
    put(p, 9, sL.noteSeq);
    for (s = 0; s < LOBBY_SLOTS; s++) {
        put(p, 10 + s * 2, sL.ids[s]);
        put(p, 11 + s * 2, (uint32_t)sL.ready[s]);
        memcpy(p + 104 + s * NAMEB, sL.names[s], NAMEB);
    }
    memcpy(p + 264, sL.note, NOTE);
    memcpy(p + 360, sL.code, 8);
    put(p, 92, sL.m[k].nonce); /* (the member's greeting's: how a member connecting knows this is its owner's answer) */
    tour_put(p);
    send_to(p, (int)sizeof(p), &sL.m[k].addr);
}

static void token_send(int k);

static void state_all(void) {
    int k;
    sL.seq++;
    for (k = 1; k < LOBBY_SLOTS; k++) {
        if (sL.m[k].on) {
            state_send(k);
            token_send(k);
        }
    }
    sL.stateNs = now_ns();
    sL.dirty = 0;
}

static void back_send(const struct sockaddr_in *to) {
    uint8_t p[12 + NOTE];
    memset(p, 0, sizeof(p));
    put(p, 0, MAGIC);
    put(p, 1, T_L_BACK);
    put(p, 2, sL.gen);
    memcpy(p + 12, sL.note, NOTE);
    send_to(p, (int)sizeof(p), to);
}

static void close_send(const struct sockaddr_in *to, uint32_t why, const char *note) {
    uint8_t p[12 + NOTE];
    memset(p, 0, sizeof(p));
    put(p, 0, MAGIC);
    put(p, 1, T_L_CLOSE);
    put(p, 2, why);
    SDL_strlcpy((char *)p + 12, note, NOTE);
    send_to(p, (int)sizeof(p), to);
}

static void addr_put(uint8_t *p, int word, const struct sockaddr_in *a) {
    put(p, word, a != NULL ? a->sin_addr.s_addr : 0);
    put(p, word + 1, a != NULL ? a->sin_port : 0);
}

static void addr_get(const uint8_t *p, int word, struct sockaddr_in *a) {
    memset(a, 0, sizeof(*a));
    a->sin_family = AF_INET;
    a->sin_addr.s_addr = get(p, word);
    a->sin_port = (uint16_t)get(p, word + 1);
}

static uint32_t match_rules(void) {
    int battle = sL.mode == 1 ? 0 : sL.battle; /* (a tournament's matches are single battles) */
    int me = member_place(0), type = battle == 0 ? 0 : battle == 1 ? 1 : 2, dp = battle >= 2 ? battle - 2 : 0;
    uint32_t side = me == 0 ? 0 : me == 1 ? 1 : 2;
    /* bits 24..27: a lobby's match, and the owner's side + 1 (the same on every copy: the pause menus, src/battle/hud_0.c) */
    return NetIo_Rules(type, dp, sL.time, sL.speed) | (side + 1) << 24;
}

static int opt_roll(void) {
    return sL.roll;
}

static int opt_delay(void) {
    return sL.delay == 0 ? -2 : sL.delay - 1;
}

/* What member k is told to do in the match (and what the owner does itself, k = 0). */
static void go_for(int k, int *role, struct sockaddr_in *t, struct sockaddr_in *alt, struct sockaddr_in *kn, struct sockaddr_in *kn2) {
    int s = member_place(k), p1 = sL.slot[0], p2 = sL.slot[1];
    memset(t, 0, sizeof(*t));
    memset(alt, 0, sizeof(*alt));
    memset(kn, 0, sizeof(*kn));
    memset(kn2, 0, sizeof(*kn2));
    t->sin_family = alt->sin_family = kn->sin_family = kn2->sin_family = AF_INET;
    if (s == 0) {
        *role = 1;
        if (p2 > 0) { /* (the one who joins is not the owner: knocked at, so that its greeting gets in) */
            *kn = sL.m[p2].addr;
            *kn2 = sL.m[p2].self;
        }
    } else if (s == 1) {
        *role = 2;
        if (p1 > 0) { /* (port 0 means: the owner, whose address the member knows) */
            *t = sL.m[p1].addr;
            *alt = sL.m[p1].self;
        }
    } else {
        *role = 3;
        if (k == 0) { /* the owner watches the host; everyone else watches the owner */
            *t = sL.m[p1].addr;
        }
    }
}

static void go_send(int k) {
    uint8_t p[60];
    int role;
    struct sockaddr_in t, alt, kn, kn2;
    go_for(k, &role, &t, &alt, &kn, &kn2);
    put(p, 0, MAGIC);
    put(p, 1, T_L_GO);
    put(p, 2, sL.gen);
    put(p, 3, (uint32_t)role);
    put(p, 4, match_rules());
    put(p, 5, (uint32_t)opt_roll());
    put(p, 6, (uint32_t)opt_delay());
    addr_put(p, 7, &t);
    addr_put(p, 9, &alt);
    addr_put(p, 11, &kn);
    addr_put(p, 13, &kn2);
    send_to(p, (int)sizeof(p), &sL.m[k].addr);
    sL.m[k].goSentNs = now_ns();
}

static int match_begin(void) {
    int k, role;
    struct sockaddr_in t, alt, kn, kn2;
    if (sL.phase != 0 || sL.slot[0] < 0 || sL.slot[1] < 0 || NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
        return 0;
    }
    sL.gen++;
    sL.phase = 1;
    sL.players[0] = sL.slot[0];
    sL.players[1] = sL.slot[1];
    sL.goNs = now_ns();
    {
        char text[NOTE];
        SDL_snprintf(text, sizeof(text), "Match %u: %s vs %s", (unsigned)sL.gen, sL.m[sL.slot[0]].name, sL.m[sL.slot[1]].name);
        note_set(text);
    }
    log_table();
    for (k = 1; k < LOBBY_SLOTS; k++) {
        sL.m[k].wasIn = 0;
        sL.m[k].connected = 0;
        if (sL.m[k].on) {
            go_send(k);
        }
    }
    state_all();
    go_for(0, &role, &t, &alt, &kn, &kn2);
    NetIo_Go(role, &t, &alt, &kn, &kn2, match_rules(), opt_roll(), opt_delay());
    return 1;
}

/* The owner's match is over (its session has ended): everyone back to the lobby window. */
static void match_over(void) {
    int k, r;
    if (sL.phase == 0) {
        return;
    }
    sL.phase = 0;
    if (sL.t.playing >= 0) { /* a tournament's match that ended without a winner: it is the next again */
        char text[NOTE];
        SDL_snprintf(text, sizeof(text), "%s", "The match will be played again.");
        sL.t.playing = -1;
        note_set(text);
    }
    for (r = 0; r < 3; r++) {
        for (k = 1; k < LOBBY_SLOTS; k++) {
            if (sL.m[k].on) {
                back_send(&sL.m[k].addr);
            }
        }
    }
    sL.dirty = 1;
    lobby_debug("[lobby] match %u is over: back to the lobby\n", (unsigned)sL.gen);
}

/* The owner ends its match (it is in its session): the session ends at its next look at the socket, and with it
   (match_over) everyone's. */
static void match_end(const char *note) {
    if (note != NULL) {
        note_set(note);
    }
    if (NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
        NetIo_End(3, sL.note);
    } else {
        match_over();
    }
}

/* ---- the owner's tournament ---- */

/* Entrant e has lost a match (it is out). */
static int t_out(int e) {
    int m;
    for (m = 0; m < t_matches(); m++) {
        if (sL.t.win[m] >= 0 && sL.t.win[m] != e && (t_part(m, 0) == e || t_part(m, 1) == e)) {
            return 1;
        }
    }
    return 0;
}

static void tok_draw(int e) { /* a new rejoin token for entrant e: 128 bits of the system's random source */
    int i;
    for (i = 0; i < TOKEN / 4; i++) {
        uint32_t v = os_random32();
        memcpy(sL.t.tok[e] + i * 4, &v, 4);
    }
    sL.t.hasTok[e] = 1;
}

/* The tokens no longer needed, forgotten: the entrant is out or gone, the tournament is over or off. */
static void t_tokens(void) {
    int e;
    for (e = 0; e < TOUR; e++) {
        if (sL.t.hasTok[e] && (sL.t.state != 1 || e >= sL.t.count || sL.t.gone[e] || t_out(e))) {
            sL.t.hasTok[e] = 0;
            memset(sL.t.tok[e], 0, TOKEN);
            lobby_debug("[lobby] entrant %d's rejoin token forgotten\n", e);
        }
    }
}

/* Member k's own token (TOKEN 1), or the word to forget the one it had (TOKEN 2, ten times). With each STATE. */
static void token_send(int k) {
    uint8_t p[20 + TOKEN + 8];
    int e;
    memset(p, 0, sizeof(p));
    put(p, 0, MAGIC);
    put(p, 1, T_L_TOKEN);
    put(p, 2, sL.t.gen);
    for (e = 0; e < sL.t.count && !(sL.t.state == 1 && sL.t.hasTok[e] && sL.t.id[e] == sL.m[k].id); e++) {
    }
    if (sL.t.state == 1 && e < sL.t.count) {
        put(p, 3, 1);
        put(p, 4, (uint32_t)e);
        memcpy(p + 20, sL.t.tok[e], TOKEN);
        memcpy(p + 20 + TOKEN, sL.code, 8);
        sL.m[k].tokFor = e + 1;
        sL.m[k].forgetLeft = 10;
    } else if (sL.m[k].tokFor != 0 && sL.m[k].forgetLeft > 0) {
        put(p, 3, 2);
        if (--sL.m[k].forgetLeft == 0) {
            sL.m[k].tokFor = 0;
        }
    } else {
        return;
    }
    send_to(p, (int)sizeof(p), &sL.m[k].addr);
}

static void t_log(void) {
    char line[256];
    size_t at = 0;
    int m;
    for (m = 0; m < t_matches() && at < sizeof(line); m++) {
        int a = t_part(m, 0), b = t_part(m, 1), w = sL.t.win[m];
        at += (size_t)SDL_snprintf(line + at, sizeof(line) - at, "%s%d:%s-%s>%s", m == 0 ? "" : " ", m, a >= 0 ? sL.t.name[a] : a == -2 ? "bye" : "?",
                                   b >= 0 ? sL.t.name[b] : b == -2 ? "bye" : "?", w >= 0 ? sL.t.name[w] : "?");
    }
    lobby_debug("[lobby] bracket %s; next %d\n", line, sL.t.cur);
}

/* Byes and walkovers decided, the next match and the champion found (after every change of the bracket). */
static void t_resolve(void) {
    int changed = 1, m;
    while (changed) {
        changed = 0;
        for (m = 0; m < t_matches(); m++) {
            int a = t_part(m, 0), b = t_part(m, 1), w = -1, how = 0;
            if (sL.t.win[m] >= 0) {
                continue;
            }
            if (a == -2 && b >= 0) {
                w = b, how = HOW_BYE;
            } else if (b == -2 && a >= 0) {
                w = a, how = HOW_BYE;
            } else if (a >= 0 && b >= 0 && (sL.t.gone[a] || sL.t.gone[b])) {
                w = sL.t.gone[a] && !sL.t.gone[b] ? b : a, how = HOW_WALKOVER;
                if (!sL.t.gone[w]) {
                    char text[NOTE];
                    SDL_snprintf(text, sizeof(text), "%s won by walkover", sL.t.name[w]);
                    note_set(text);
                }
            }
            if (w >= 0) {
                sL.t.win[m] = w;
                sL.t.how[m] = how;
                changed = 1;
            }
        }
    }
    if (sL.t.choose >= 0 && sL.t.win[sL.t.choose] >= 0) {
        sL.t.choose = -1; /* (decided otherwise: a walkover) */
    }
    sL.t.cur = t_next();
    m = t_matches() - 1;
    if (sL.t.state == 1 && sL.t.win[m] >= 0) {
        char text[NOTE];
        sL.t.state = 2;
        sL.t.champ = sL.t.win[m];
        SDL_snprintf(text, sizeof(text), "%s is the champion!", sL.t.name[sL.t.champ]);
        note_set(text);
    }
    t_tokens();
    sL.dirty = 1;
    t_log();
}

/* A tournament of those in the player places (the Entrants rule's), drawn (the owner's TOUR_NEW). */
static void t_new(void) {
    static const int kByePairs8[4] = {0, 3, 1, 2}, kByePairs4[2] = {0, 1}; /* (the byes shared out between the halves) */
    int order[TOUR], n = 0, s, i, byes;
    uint32_t x, gen = sL.t.gen;
    if (sL.phase != 0 || sL.mode != 1 || sL.t.state == 1) {
        return;
    }
    for (s = 0; s < sL.entrants; s++) { /* (the entrants: whoever sits in the player places) */
        n += sL.slot[s] >= 0;
    }
    if (n < 2) {
        lobby_debug("[lobby] a tournament needs two of its %d player places taken (%d are)\n", sL.entrants, n);
        return;
    }
    memset(&sL.t, 0, sizeof(sL.t));
    n = 0;
    for (s = 0; s < sL.entrants; s++) {
        int k = sL.slot[s];
        if (k >= 0) {
            sL.t.id[n] = sL.m[k].id;
            SDL_strlcpy(sL.t.name[n], sL.m[k].name, NAMEB);
            order[n] = n;
            n++;
        }
    }
    sL.t.cur = sL.t.champ = sL.t.playing = sL.t.choose = -1;
    sL.t.gen = gen + 1;
    sL.t.state = 1;
    sL.t.count = n;
    sL.t.size = n <= 2 ? 2 : n <= 4 ? 4 : 8;
    sL.t.seed = os_random32();
    x = sL.t.seed != 0 ? sL.t.seed : 0x9E3779B9u;
    for (i = n - 1; i > 0; i--) { /* (Fisher-Yates, xorshift32 from the seed) */
        int j, t;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        j = (int)(x % (uint32_t)(i + 1));
        t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    byes = sL.t.size - n;
    for (i = 0; i < sL.t.size; i++) {
        sL.t.leaf[i] = 0;
    }
    for (i = 0; i < byes; i++) {
        sL.t.leaf[2 * (sL.t.size == 8 ? kByePairs8[i] : kByePairs4[i]) + 1] = -1;
    }
    for (i = 0, s = 0; i < sL.t.size; i++) {
        if (sL.t.leaf[i] == 0) {
            sL.t.leaf[i] = order[s++];
        }
    }
    for (i = 0; i < TOUR - 1; i++) {
        sL.t.win[i] = -1;
    }
    for (i = 0; i < n; i++) {
        tok_draw(i);
    }
    {
        char text[NOTE];
        SDL_snprintf(text, sizeof(text), "Tournament: %d entrants", n);
        note_set(text);
    }
    lobby_debug("[lobby] tournament %u drawn (seed %08x)\n", (unsigned)sL.t.gen, (unsigned)sL.t.seed);
    t_resolve();
}

static int member_by_id(uint32_t id) {
    int k;
    for (k = 0; k < LOBBY_SLOTS; k++) {
        if (sL.m[k].on && sL.m[k].id == id) {
            return k;
        }
    }
    return -1;
}

/* The tournament's next match: its two put in 1P and 2P (the first of the match on the left of the bracket in
   1P), and begun. */
/* The next match can begin: both its entrants are in the lobby (not one whose place is held). */
static int t_can_start(void) {
    int m = sL.t.cur;
    return sL.t.state == 1 && m >= 0 && sL.phase == 0 && sL.t.choose < 0 && t_part(m, 0) >= 0 && t_part(m, 1) >= 0 && member_by_id(sL.t.id[t_part(m, 0)]) >= 0 &&
           member_by_id(sL.t.id[t_part(m, 1)]) >= 0;
}

static void t_start(void) {
    int m = sL.t.cur, e[2], s, k;
    if (!t_can_start()) {
        return;
    }
    e[0] = t_part(m, 0);
    e[1] = t_part(m, 1);
    for (s = 0; s < 2; s++) {
        int at;
        k = e[s] >= 0 ? member_by_id(sL.t.id[e[s]]) : -1;
        if (k < 0) {
            return;
        }
        at = member_place(k);
        if (at != s) {
            int t = sL.slot[s];
            sL.slot[s] = sL.slot[at];
            sL.slot[at] = t;
        }
    }
    table_make();
    sL.t.side[0] = e[0];
    sL.t.side[1] = e[1];
    sL.t.oneResNs = 0;
    sL.t.playing = m;
    if (!match_begin()) {
        sL.t.playing = -1;
    }
}

/* The owner, while a tournament's match is on: the two players' results (its own if it plays). Agreed on a winner:
   marked, and the match ends; anything else: it ends and is played again. */
static void t_decide(void) {
    int r[2], s, m = sL.t.playing;
    const char *again = NULL;
    if (sL.phase != 1 || m < 0 || sL.t.win[m] >= 0) {
        return;
    }
    for (s = 0; s < 2; s++) {
        int k = sL.players[s];
        r[s] = k == 0 ? (sL.resGen == sL.gen ? sL.res : 0) : k > 0 && sL.m[k].on && sL.m[k].resGen == sL.gen ? sL.m[k].res : 0;
    }
    if (r[0] != 0 && r[1] != 0) {
        if (r[0] == r[1] && r[0] != RES_NO_WINNER) {
            char text[NOTE];
            int w = sL.t.side[r[0] - 1];
            sL.t.win[m] = w;
            sL.t.how[m] = HOW_PLAYED;
            sL.t.playing = -1;
            lobby_debug("[lobby] match %u decided: side %d won (%s)\n", (unsigned)sL.gen, r[0] - 1, sL.t.name[w]);
            t_resolve();
            if (sL.t.state == 1) { /* (the final's: the champion is said instead, the members' log has one line of it) */
                SDL_snprintf(text, sizeof(text), "%s won the match", sL.t.name[w]);
                note_set(text);
            }
            match_end(NULL);
            return;
        }
        if (r[0] != r[1] && r[0] != RES_NO_WINNER && r[1] != RES_NO_WINNER && ++sL.t.disagree[m] >= 2) {
            /* the second time the two games name different winners: no more replays, the owner chooses */
            lobby_debug("[lobby] match %u: the two games disagree again (%d / %d): the owner chooses\n", (unsigned)sL.gen, r[0], r[1]);
            sL.t.playing = -1;
            sL.t.choose = m;
            sL.dirty = 1;
            match_end("The two games disagree on this match: the owner chooses the winner.");
            return;
        }
        again = r[0] == r[1] ? "No winner: the match will be played again." : "The players' results differ: the match will be played again.";
    } else if (r[0] != 0 || r[1] != 0) {
        uint64_t now = now_ns();
        if (sL.t.oneResNs == 0) {
            sL.t.oneResNs = now;
        } else if (now - sL.t.oneResNs > RESULT_WAIT_NS) {
            again = "Only one player's result came: the match will be played again.";
        }
    }
    if (again != NULL) {
        lobby_debug("[lobby] match %u: results %d / %d\n", (unsigned)sL.gen, r[0], r[1]);
        sL.t.playing = -1;
        match_end(again);
    }
}

/* Entrant e is gone: its match now, or its next, is lost by walkover. */
static void t_gone_e(int e) {
    sL.t.gone[e] = 1;
    sL.t.dropNs[e] = 0;
    sL.t.dropSecs[e] = 0;
    if (sL.t.playing >= 0 && (sL.t.side[0] == e || sL.t.side[1] == e)) {
        sL.t.playing = -1; /* (decided by the walkover below, not played again) */
    }
    t_resolve();
}

/* The entrant of member id (-1 none, or not in the tournament any more). */
static int t_entrant(uint32_t id) {
    int e;
    for (e = 0; sL.t.state == 1 && e < sL.t.count; e++) {
        if (sL.t.id[e] == id && !sL.t.gone[e]) {
            return e;
        }
    }
    return -1;
}

/* An entrant left the lobby on purpose (or was removed): walkover. */
static void t_gone(uint32_t id) {
    int e = t_entrant(id);
    if (e >= 0) {
        t_gone_e(e);
    }
}

/* An entrant's game fell silent: its place is held for HOLD_NS (a rejoin with its token), unless it is out already. */
static int t_hold(int k, int place) {
    int e = t_entrant(sL.m[k].id);
    char text[NOTE];
    if (e < 0 || t_out(e) || !sL.t.hasTok[e]) {
        return 0;
    }
    sL.t.dropNs[e] = now_ns();
    sL.t.dropPlace[e] = place;
    sL.t.dropSecs[e] = (int)(HOLD_NS / SEC);
    SDL_snprintf(text, sizeof(text), "%s lost the connection: the place is held for 90 s", sL.t.name[e]);
    note_set(text);
    return 1;
}

/* The holds that ran out: walkover; the seconds left, for the bracket. */
static void t_holds(void) {
    uint64_t now = now_ns();
    int e;
    for (e = 0; sL.t.state == 1 && e < sL.t.count; e++) {
        if (sL.t.dropNs[e] != 0) {
            uint64_t held = now - sL.t.dropNs[e];
            int left = held >= HOLD_NS ? 0 : (int)((HOLD_NS - held + SEC - 1) / SEC);
            if (left != sL.t.dropSecs[e]) {
                sL.t.dropSecs[e] = left;
                sL.dirty = 1;
            }
            if (left == 0) {
                char text[NOTE];
                SDL_snprintf(text, sizeof(text), "%s did not come back in time", sL.t.name[e]);
                note_set(text);
                t_gone_e(e);
            }
        }
    }
}

/* A greeting with a rejoin token: the entrant it belongs to (-1 none). */
static int t_token_owner(const uint8_t *tok, uint32_t gen) {
    static const uint8_t zero[TOKEN];
    int e;
    if (memcmp(tok, zero, TOKEN) == 0 || gen != sL.t.gen) {
        return -1;
    }
    for (e = 0; sL.t.state == 1 && e < sL.t.count; e++) {
        if (sL.t.hasTok[e] && !sL.t.gone[e] && memcmp(sL.t.tok[e], tok, TOKEN) == 0) {
            return e;
        }
    }
    return -1;
}

/* Member k is gone: on purpose (LEAVE, removed), or its game fell silent (`lost`: a tournament's entrant keeps its
   place for a while, t_hold). */
static void member_drop(int k, const char *why, int lost) {
    char text[NOTE];
    int s = member_place(k), player = sL.phase == 1 && (k == sL.players[0] || k == sL.players[1]);
    SDL_snprintf(text, sizeof(text), "%s %s", sL.m[k].name, why);
    sL.m[k].on = 0;
    if (s >= 0) {
        sL.slot[s] = -1;
    }
    table_make();
    if (!(lost && t_hold(k, s))) {
        note_set(text);
        t_gone(sL.m[k].id);
    }
    log_table();
    if (player) {
        match_end(NULL);
    }
}

static void owner_hello(const uint8_t *pkt, int n, const struct sockaddr_in *from) {
    uint32_t why = 0, nonce = n >= 32 ? get(pkt, 7) : 0;
    unsigned long long mine;
    int k, free = -1, s, back = n >= HELLO_BYTES ? t_token_owner(pkt + 40 + NAMEB + 4, get(pkt, (40 + NAMEB) / 4)) : -1;
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &from->sin_addr, ip, sizeof(ip));
    lobby_debug("[lobby] owner_hello from %s:%u bytes=%d\n", ip, (unsigned)ntohs(from->sin_port), n);
    if (n < 40 + NAMEB) {
        lobby_debug("[lobby] owner_hello rejected: malformed packet\n");
        return;
    }
    if (back >= 0 && sL.t.dropNs[back] == 0 && get(pkt, 2) == LOBBY_VERSION) {
        /* the entrant's game greets again from somewhere new (it was started again) while its old one is not yet
           found silent: that one is dropped now, the place held */
        for (k = 1; k < LOBBY_SLOTS; k++) {
            if (sL.m[k].on && sL.m[k].id == sL.t.id[back] && !same(&sL.m[k].addr, from)) {
                member_drop(k, "lost the connection to the lobby", 1);
            }
        }
    }
    if (get(pkt, 2) != LOBBY_VERSION || get(pkt, 3) != (uint32_t)NetIo_Version()) {
        why = LNO_VERSION;
    } else if (get(pkt, 4) == FP_MARK && NetIo_Fingerprint(&mine) && ((unsigned long long)get(pkt, 5) | (unsigned long long)get(pkt, 6) << 32) != mine) {
        why = LNO_DATA; /* (the same data as the owner's, modified or not: a lobby is among friends) */
    } else if (get(pkt, 4) != FP_MARK && NetIo_FingerprintRequired()) {
        why = LNO_DATA; /* (says none: 0.1.37, net.c FP_PEER_REQUIRED) */
    }
    for (k = 1; k < LOBBY_SLOTS; k++) {
        if (sL.m[k].on && same(&sL.m[k].addr, from)) {
            sL.m[k].heardNs = now_ns();
            state_send(k); /* (its greeting came again: the answer was lost) */
            lobby_debug("[lobby] owner_hello existing member: resent state to %s:%u\n", ip, (unsigned)ntohs(from->sin_port));
            return;
        }
        if (!sL.m[k].on && free < 0) {
            free = k;
        }
    }
    for (s = 0; s < LOBBY_SLOTS && sL.slot[s] >= 0; s++) {
    }
    if (why == 0 && (free < 0 || s == LOBBY_SLOTS)) {
        why = LNO_FULL;
    }
    if (why == LNO_VERSION && (sL.otherNs == 0 || now_ns() - sL.otherNs > 60 * SEC)) {
        sL.otherNs = now_ns(); /* (its greetings come five times a second until it gives up) */
        note_set("Someone with another version of the game tried to join.");
    }
    if (why != 0) {
        uint32_t no[4] = {MAGIC, T_L_NO, why, nonce};
        lobby_debug("[lobby] owner_hello rejected: %s from %s:%u\n",
                why == LNO_VERSION ? "version differs" : why == LNO_DATA ? "game data differs" : "lobby full",
                ip, (unsigned)ntohs(from->sin_port));
        send_to(no, sizeof(no), from);
        return;
    }
    memset(&sL.m[free], 0, sizeof(sL.m[free]));
    sL.m[free].on = 1;
    sL.m[free].id = ++sL.nextId;
    sL.m[free].addr = *from;
    sL.m[free].nonce = nonce;
    addr_get(pkt, 8, &sL.m[free].self);
    memcpy(sL.m[free].name, pkt + 40, NAMEB);
    name_clean(sL.m[free].name, NAMEB);
    if (sL.m[free].name[0] == '\0') {
        SDL_strlcpy(sL.m[free].name, "Player", NAMEB);
    }
    sL.m[free].heardNs = now_ns();
    /* the first free place: a player's while one is free and no match is on, else a spectator's (while a tournament is
       on, a spectator's: its entrants are drawn already) */
    for (s = sL.mode == 1 && sL.t.state == 1 ? sL.entrants : sL.phase == 1 ? 2 : 0; s < LOBBY_SLOTS && sL.slot[s] >= 0; s++) {
    }
    if (s == LOBBY_SLOTS) {
        for (s = 0; s < LOBBY_SLOTS && sL.slot[s] >= 0; s++) {
        }
    }
    sL.slot[s] = free;
    lobby_debug("[lobby] owner_hello accepted: member %s at slot %d from %s:%u\n",
            sL.m[free].name, s, ip, (unsigned)ntohs(from->sin_port));
    Relay_Permit(NetIo_Sock(), from);
    {   /* a member who came with the code: the oldest knocked address is taken as its (its address as this side
           sees it may be another than the one the service saw: the same network, another router) */
        int j, oldest = -1;
        for (j = 0; j < 16; j++) {
            if (sL.pend[j].ip != 0 && (oldest < 0 || sL.pend[j].since < sL.pend[oldest].since)) {
                oldest = j;
            }
            if (sL.pend[j].ip == from->sin_addr.s_addr || (sL.m[free].self.sin_port != 0 && sL.pend[j].ip == sL.m[free].self.sin_addr.s_addr)) {
                oldest = j;
                break;
            }
        }
        if (oldest >= 0) {
            sL.pend[oldest].ip = 0;
            sL.m[free].claimed = 1;
        }
        sL.m[free].joinedNs = now_ns();
    }
    table_make();
    {
        char text[NOTE];
        SDL_snprintf(text, sizeof(text), "%s joined the lobby", sL.m[free].name);
        note_set(text);
    }
    if (back >= 0 && sL.t.dropNs[back] != 0) { /* a held entrant back, by its token: its place again, a new token */
        char text[NOTE];
        int p = sL.t.dropPlace[back];
        if (p >= 0 && p < LOBBY_SLOTS && p != s) {
            int t = sL.slot[p];
            sL.slot[p] = free;
            sL.slot[s] = t;
        }
        sL.t.id[back] = sL.m[free].id;
        sL.t.dropNs[back] = 0;
        sL.t.dropSecs[back] = 0;
        tok_draw(back);
        table_make();
        SDL_snprintf(text, sizeof(text), "%s is back in the tournament", sL.m[free].name);
        note_set(text);
        lobby_debug("[lobby] entrant %d back by its token (a new one drawn)\n", back);
    } else if (n >= HELLO_BYTES && get(pkt, (40 + NAMEB) / 4) != 0) {
        lobby_debug("[lobby] a greeting with a rejoin token that is no entrant's now: a member as any other, told to forget it\n");
        sL.m[free].tokFor = 1;
        sL.m[free].forgetLeft = 10;
    }
    log_table();
    state_all();
    if (sL.phase == 1) { /* a match is on: it is watched from now */
        go_send(free);
    }
}

static void owner_say(int k, const char *text);

/* The owner: a packet of a member. */
static void owner_packet(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type) {
    int k;
    if (type == T_L_HELLO) {
        owner_hello(pkt, n, from);
        return;
    }
    for (k = 1; k < LOBBY_SLOTS && !(sL.m[k].on && same(&sL.m[k].addr, from)); k++) {
    }
    if (k == LOBBY_SLOTS) {
        return; /* (not a member: nothing it says counts) */
    }
    sL.m[k].heardNs = now_ns();
    if (type == T_L_ALIVE && n >= 32) {
        int ready = get(pkt, 4) != 0, s = member_place(k);
        sL.m[k].goGen = get(pkt, 5);
        sL.m[k].inGen = get(pkt, 6);
        sL.m[k].connected = get(pkt, 7) != 0;
        if (n >= 40 && get(pkt, 9) <= RES_NO_WINNER) {
            sL.m[k].resGen = get(pkt, 8);
            sL.m[k].res = (int)get(pkt, 9);
        }
        if (sL.phase == 1 && sL.m[k].inGen == sL.gen) {
            sL.m[k].wasIn = 1;
        }
        if (ready != sL.m[k].ready) {
            sL.m[k].ready = ready;
            if (s >= 0) {
                sL.ready[s] = ready;
            }
            sL.dirty = 1;
        }
    } else if (type == T_L_LEAVE) {
        member_drop(k, "left the lobby", 0);
    } else if (type == T_L_CHAT && n >= 20) {
        uint32_t seq = get(pkt, 3), len = get(pkt, 4);
        if (seq > sL.m[k].chatSeq && len <= TEXT && 20 + len <= (uint32_t)n) {
            char text[TEXT + 1];
            sL.m[k].chatSeq = seq;
            memcpy(text, pkt + 20, len);
            text[len] = '\0';
            owner_say(k, text); /* (passed on to everyone, with the member's name) */
        }
    }
}

static void owner_say(int k, const char *text) {
    uint8_t p[16 + NAMEB + TEXT];
    int len = (int)strlen(text), i, r;
    if (len > TEXT) {
        len = TEXT;
    }
    if (sL.chatCount == LOBBY_CHAT) {
        memmove(&sL.chat[0], &sL.chat[1], sizeof(sL.chat[0]) * (LOBBY_CHAT - 1));
        sL.chatCount--;
    }
    SDL_strlcpy(sL.chat[sL.chatCount].name, sL.m[k].name, NAMEB);
    memcpy(sL.chat[sL.chatCount].text, text, (size_t)len);
    sL.chat[sL.chatCount].text[len] = '\0';
    sL.chatCount++;
    sL.chatOut++;
    log_push(sL.chat[sL.chatCount - 1].name, sL.chat[sL.chatCount - 1].text);
    lobby_debug("[lobby] chat: %s: %s\n", sL.m[k].name, sL.chat[sL.chatCount - 1].text);
    put(p, 0, MAGIC);
    put(p, 1, T_L_SAID);
    put(p, 2, sL.chatOut);
    put(p, 3, (uint32_t)len);
    memcpy(p + 16, sL.m[k].name, NAMEB);
    memcpy(p + 16 + NAMEB, text, (size_t)len);
    for (r = 0; r < 3; r++) { /* (three times: a lost one is covered) */
        for (i = 1; i < LOBBY_SLOTS; i++) {
            if (sL.m[i].on) {
                send_to(p, 16 + NAMEB + len, &sL.m[i].addr);
            }
        }
    }
}

static int rule_count(int which) {
    return which == LOBBY_RULE_BATTLE ? 5 : which == LOBBY_RULE_TIME ? 5 : which == LOBBY_RULE_ROLLBACK ? 5 : which == LOBBY_RULE_DELAY ? 8 : 0;
}

static void owner_do(int what, int a, int b) {
    if (what == LOBBY_START) {
        if (sL.mode == 1) {
            t_start(); /* (the tournament's next match) */
        } else if (sL.phase == 0 && sL.slot[0] >= 0 && sL.slot[1] >= 0) {
            match_begin();
        }
    } else if (what == LOBBY_TOUR_CHOOSE) {
        int m = sL.t.choose, w = m >= 0 && (a == 0 || a == 1) ? t_part(m, a) : -1;
        if (sL.phase == 0 && sL.t.state == 1 && w >= 0 && sL.t.win[m] < 0) {
            char text[NOTE];
            sL.t.win[m] = w;
            sL.t.how[m] = HOW_CHOSEN;
            sL.t.choose = -1;
            lobby_debug("[lobby] the owner chose the winner of match %d: %s\n", m, sL.t.name[w]);
            SDL_snprintf(text, sizeof(text), "%s won the match (the owner's choice)", sL.t.name[w]);
            note_set(text);
            t_resolve();
        }
    } else if (what == LOBBY_TOUR_NEW) {
        t_new();
    } else if (what == LOBBY_TOUR_CALLOFF) {
        if (sL.phase == 0 && sL.t.state == 1) {
            sL.t.state = 0;
            sL.t.cur = sL.t.playing = -1;
            memset(sL.t.dropNs, 0, sizeof(sL.t.dropNs));
            memset(sL.t.dropSecs, 0, sizeof(sL.t.dropSecs));
            t_tokens();
            note_set("The tournament was called off.");
        }
    } else if (what == LOBBY_BACK) {
        if (sL.phase == 1) {
            match_end("The owner went back to the lobby.");
        }
    } else if (what == LOBBY_KICK) {
        if (a >= 0 && a < LOBBY_SLOTS && sL.slot[a] > 0) {
            int k = sL.slot[a], r;
            for (r = 0; r < 3; r++) {
                close_send(&sL.m[k].addr, CLOSE_KICKED, "The lobby's owner removed you from the lobby.");
            }
            member_drop(k, "was removed from the lobby", 0);
        }
    } else if (what == LOBBY_MOVE) {
        if (sL.phase == 0 && a >= 0 && a < LOBBY_SLOTS && b >= 0 && b < LOBBY_SLOTS && a != b) {
            int t = sL.slot[a];
            sL.slot[a] = sL.slot[b];
            sL.slot[b] = t;
            table_make();
            log_table();
        }
    } else if (what == LOBBY_RULE) {
        if (sL.phase == 0) { /* (the rules cannot change while a match is on: everyone comes back here for that) */
            if (a == LOBBY_RULE_MODE) {
                if (sL.t.state == 1) {
                    return; /* (a tournament that is on is called off first: TOUR_CALLOFF) */
                }
                sL.mode = !sL.mode;
                if (sL.mode == 0) { /* (the last tournament's bracket goes with it) */
                    uint32_t gen = sL.t.gen;
                    memset(&sL.t, 0, sizeof(sL.t));
                    sL.t.gen = gen;
                    sL.t.cur = sL.t.champ = sL.t.playing = sL.t.choose = -1;
                }
            } else if (a == LOBBY_RULE_ENTRANTS) {
                if (sL.mode != 1 || sL.t.state == 1) {
                    return; /* (a tournament's, and not while one is on) */
                }
                /* (fewer: who sat in the places that are no longer the players' is a spectator there now) */
                sL.entrants = (sL.entrants - 2 + b + (TOUR - 1)) % (TOUR - 1) + 2;
            } else if (a == LOBBY_RULE_BATTLE && sL.mode == 1) {
                return; /* (a tournament's matches are single battles) */
            } else if (a == LOBBY_RULE_SPEED) {
                static const int kSpeeds[] = {100, 105, 110, 114, 120, 125, 130, 140, 150};
                int i, n = (int)(sizeof(kSpeeds) / sizeof(kSpeeds[0]));
                for (i = 0; i < n - 1 && kSpeeds[i] < sL.speed; i++) {
                }
                i = (i + b + n) % n;
                sL.speed = kSpeeds[i];
            } else if (rule_count(a) > 0) {
                int *v = a == LOBBY_RULE_BATTLE ? &sL.battle : a == LOBBY_RULE_TIME ? &sL.time : a == LOBBY_RULE_ROLLBACK ? &sL.roll : &sL.delay;
                int n = rule_count(a), cur = a == LOBBY_RULE_ROLLBACK ? *v / 2 : *v;
                cur = (cur + b + n) % n;
                *v = a == LOBBY_RULE_ROLLBACK ? cur * 2 : cur;
            }
            sL.dirty = 1;
            lobby_debug("[lobby] rules %s\n", rules_text());
        }
    } else if (what == LOBBY_READY) {
        sL.m[0].ready = a != 0;
        table_make();
    }
}

/* ---- a member ---- */

/* The member's rejoin token on disk: lobby_rejoin.txt beside the settings file (no settings file: none). */
static const char *tok_path(void) {
    static char path[600];
    const char *set = getenv("BT3_SETTINGS"), *slash;
    size_t n;
    if (set != NULL && set[0] == '\0') {
        return NULL;
    }
    if (set == NULL) {
        return "lobby_rejoin.txt";
    }
    slash = strrchr(set, '/');
#ifdef _WIN32
    if (strrchr(set, '\\') != NULL && (slash == NULL || strrchr(set, '\\') > slash)) {
        slash = strrchr(set, '\\');
    }
#endif
    n = slash != NULL ? (size_t)(slash - set + 1) : 0;
    if (n + 20 > sizeof(path)) {
        return NULL;
    }
    memcpy(path, set, n);
    strcpy(path + n, "lobby_rejoin.txt");
    return path;
}

static void tok_forget(const char *why) {
    const char *path = tok_path();
    if (sTok.has) {
        lobby_debug("[lobby] the rejoin token is forgotten (%s)\n", why);
    }
    memset(&sTok, 0, sizeof(sTok));
    sTok.loaded = 1;
    if (path != NULL) {
        remove(path);
    }
}

static void tok_keep(uint32_t gen, const uint8_t *tok, const char *code) {
    const char *path = tok_path();
    FILE *f;
    int i;
    sTok.has = 1;
    sTok.loaded = 1;
    sTok.gen = gen;
    memcpy(sTok.tok, tok, TOKEN);
    memcpy(sTok.code, code, 8);
    sTok.code[7] = '\0';
    name_clean(sTok.code, 8);
    f = path != NULL ? fopen(path, "w") : NULL;
    if (f != NULL) {
        fprintf(f, "bt3 lobby rejoin 1\n%lld\n%s\n%u\n", (long long)time(NULL), sTok.code[0] != '\0' ? sTok.code : "-", (unsigned)gen);
        for (i = 0; i < TOKEN; i++) {
            fprintf(f, "%02x", sTok.tok[i]);
        }
        fprintf(f, "\n");
        fclose(f);
    }
    lobby_debug("[lobby] a rejoin token kept for tournament %u (lobby %s)\n", (unsigned)gen, sTok.code);
}

/* At the program's start (the first lobby blank): the token of a game that crashed or was closed in a tournament,
   if it is less than an hour old; an older file is removed. */
static void tok_load(void) {
    const char *path;
    FILE *f;
    char head[32], code[16];
    long long when;
    unsigned gen;
    char hex[2 * TOKEN + 2];
    int i, ok = 0;
    if (sTok.loaded) {
        return;
    }
    sTok.loaded = 1;
    path = tok_path();
    f = path != NULL ? fopen(path, "r") : NULL;
    if (f == NULL) {
        return;
    }
    if (fgets(head, sizeof(head), f) != NULL && strncmp(head, "bt3 lobby rejoin 1", 18) == 0 &&
        fscanf(f, "%lld %15s %u %34s", &when, code, &gen, hex) == 4 && strlen(hex) == 2 * TOKEN) {
        long long age = (long long)time(NULL) - when;
        ok = age >= 0 && age < 3600;
        for (i = 0; ok && i < TOKEN; i++) {
            unsigned v;
            ok = sscanf(hex + 2 * i, "%2x", &v) == 1;
            sTok.tok[i] = (uint8_t)v;
        }
        if (!ok) {
            lobby_debug("[lobby] the rejoin token file is %lld s old: removed\n", age);
        }
    }
    fclose(f);
    if (!ok) {
        tok_forget("old or not readable");
        return;
    }
    sTok.has = 1;
    sTok.gen = gen;
    SDL_strlcpy(sTok.code, strcmp(code, "-") == 0 ? "" : code, sizeof(sTok.code));
    lobby_debug("[lobby] a rejoin token from before (lobby %s, tournament %u)\n", sTok.code, gen);
}

static void hello_send(const struct sockaddr_in *to) {
    uint8_t p[HELLO_BYTES];
    unsigned long long fp = 0;
    struct sockaddr_in self;
    char text[64], *colon;
    memset(p, 0, sizeof(p));
    memset(&self, 0, sizeof(self));
    SDL_strlcpy(text, sL.self, sizeof(text));
    colon = strrchr(text, ':');
    if (colon != NULL) {
        *colon = '\0';
        if (inet_pton(AF_INET, text, &self.sin_addr) == 1) {
            self.sin_port = htons((uint16_t)atoi(colon + 1));
        }
    }
    put(p, 0, MAGIC);
    put(p, 1, T_L_HELLO);
    put(p, 2, LOBBY_VERSION);
    put(p, 3, (uint32_t)NetIo_Version());
    if (NetIo_Fingerprint(&fp)) {
        put(p, 4, FP_MARK);
        put(p, 5, (uint32_t)fp);
        put(p, 6, (uint32_t)(fp >> 32));
    }
    put(p, 7, sL.nonce);
    addr_put(p, 8, &self);
    SDL_strlcpy((char *)p + 40, NetIo_MyName(), NAMEB);
    {
        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &to->sin_addr, ip, sizeof(ip));
        lobby_debug("[lobby] member sent T_L_HELLO to %s:%u\n", ip, (unsigned)ntohs(to->sin_port));
    }
    if (sTok.has && strcmp(sTok.code, sJoinCode) == 0) { /* (its own lobby's only) */
        static uint32_t said;
        put(p, (40 + NAMEB) / 4, sTok.gen);
        memcpy(p + 40 + NAMEB + 4, sTok.tok, TOKEN);
        if (said != sL.nonce) {
            said = sL.nonce;
            lobby_debug("[lobby] the greeting carries this game's rejoin token\n");
        }
    }
    send_to(p, (int)sizeof(p), to);
}

static void alive_send(void) {
    uint32_t p[10] = {MAGIC, T_L_ALIVE, sL.myId, sL.seqSeen, 0, sL.goGen, sL.inGen, 0, sL.resGen, (uint32_t)sL.res};
    int s;
    for (s = 0; s < LOBBY_SLOTS; s++) {
        if (sL.ids[s] == sL.myId && sL.myId != 0) {
            p[4] = (uint32_t)sL.ready[s];
        }
    }
    if (sL.wantReady >= 0) {
        p[4] = (uint32_t)sL.wantReady; /* (the wish, until the owner's word says it) */
    }
    p[7] = (uint32_t)NetIo_Connected();
    send_to(p, sizeof(p), &sL.owner);
    sL.aliveNs = now_ns();
}

static const char *no_text(uint32_t why) {
    return why == LNO_DATA ? "The lobby owner's game data are not the same as yours (a modified disc image or mods on one side): you cannot join."
         : why == LNO_FULL ? "This lobby is full (8 places)."
                          : "The lobby owner's game is a different version: joining needs the same release.";
}

static void lobby_off(void);

/* The program ends (its window closed, the game quit: atexit) while it is in a lobby: the owner closes it for
   everyone and a member leaves, three times as when it is done from the screens, on the socket it has (in a match
   the session's, which reaches the same games). Then the others know at once instead of after the silence. */
static void lobby_goodbye(void) {
    static int said;
    int k, r;
    if (said || sL.on == 0 || NetIo_Sock() < 0) {
        return;
    }
    said = 1;
    for (r = 0; r < 3; r++) {
        if (sL.on == 1) {
            for (k = 1; k < LOBBY_SLOTS; k++) {
                if (sL.m[k].on) {
                    close_send(&sL.m[k].addr, CLOSE_CLOSED, "");
                }
            }
        } else {
            uint32_t p[3] = {MAGIC, T_L_LEAVE, sL.myId};
            send_to(p, sizeof(p), &sL.owner);
        }
    }
    lobby_debug("[lobby] the program ends: %s\n", sL.on == 1 ? "the lobby is closed for everyone" : "this game leaves the lobby");
}

static void goodbye_arm(void) {
    static int armed;
    if (!armed) {
        armed = 1;
        atexit(lobby_goodbye);
    }
}

static void member_closed(const char *why) {
    SDL_strlcpy(sL.error, why, sizeof(sL.error));
    lobby_debug("[lobby] %s\n", why);
    lobby_off();
}

static void member_packet(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type) {
    if (type == T_L_STATE || type == T_L_NO) {
        char ip[INET_ADDRSTRLEN] = "?";
        uint32_t nonce = type == T_L_NO && n >= 16 ? get(pkt, 3) : type == T_L_STATE && n >= 372 ? get(pkt, 92) : 0;
        inet_ntop(AF_INET, &from->sin_addr, ip, sizeof(ip));
        lobby_debug("[lobby] member received %s from %s:%u bytes=%d nonce=%u expected=%u%s\n",
                type == T_L_STATE ? "T_L_STATE" : "T_L_NO", ip, (unsigned)ntohs(from->sin_port), n,
                nonce, (unsigned)sL.nonce, nonce == sL.nonce ? " match" : " mismatch/absent");
    }
    if (sL.state == 1) { /* connecting: the owner's first answer is the one that carries this member's nonce (its
                            greeting's, a number nobody else knows), from whatever address it comes (the owner's
                            game may answer from another address of its machine, or through its router with
                            another port, than the service was told of) */
        int ours = 0;
        if (type == T_L_NO && n >= 16) {
            ours = get(pkt, 3) == sL.nonce;
        } else if (type == T_L_STATE && n >= 372) {
            ours = get(pkt, 92) == sL.nonce;
        }
        if (!ours) {
            static int said;
            char text[32];
            if ((type == T_L_STATE || type == T_L_NO) && said++ < 4 && inet_ntop(AF_INET, &from->sin_addr, text, sizeof(text)) != NULL) {
                lobby_debug("[lobby] an answer from %s:%d without this member's nonce: not taken\n", text, ntohs(from->sin_port));
            }
            return;
        }
        if (type == T_L_NO) {
            member_closed(no_text(get(pkt, 2)));
            return;
        }
        sL.owner = *from;
        sL.state = 2;
        Match_Cancel(); /* (a code's search: it has done its work, the owner's relay address is not needed now) */
        lobby_debug("[lobby] in the lobby (%s)\n", Relay_IsPeer(from) ? "through the relay" : "the owner answered");
    }
    if (!same(from, &sL.owner)) {
        return;
    }
    sL.heardNs = now_ns();
    if (type == T_L_STATE && n >= STATE_BYTES) {
        uint32_t seq = get(pkt, 2), r = get(pkt, 7), o = get(pkt, 8), noteSeq = get(pkt, 9);
        int s, changed = 0;
        if (seq < sL.seqSeen && sL.seqSeen - seq < 1000) {
            return; /* (an old one, overtaken) */
        }
        sL.seqSeen = seq;
        sL.myId = get(pkt, 3);
        sL.ownerId = get(pkt, 4);
        sL.phase = (int)get(pkt, 5);
        sL.gen = get(pkt, 6);
        changed |= sL.battle != (int)(r & 0xFF) || sL.time != (int)(r >> 8 & 0xFF) || sL.speed != (int)(r >> 16 & 0xFFFF);
        sL.battle = (int)(r & 0xFF) % 5;
        sL.time = (int)(r >> 8 & 0xFF) % 5;
        sL.speed = (int)(r >> 16 & 0xFFFF);
        sL.roll = (int)(o & 0xFF);
        sL.delay = (int)(o >> 8 & 0xFF);
        for (s = 0; s < LOBBY_SLOTS; s++) {
            uint32_t id = get(pkt, 10 + s * 2);
            changed |= id != sL.ids[s] || sL.ready[s] != (int)get(pkt, 11 + s * 2);
            sL.ids[s] = id;
            sL.ready[s] = (int)get(pkt, 11 + s * 2);
            memcpy(sL.names[s], pkt + 104 + s * NAMEB, NAMEB);
            name_clean(sL.names[s], NAMEB);
        }
        if (noteSeq != sL.noteSeq) {
            sL.noteSeq = noteSeq;
            memcpy(sL.note, pkt + 264, NOTE);
            name_clean(sL.note, NOTE);
            if (sL.note[0] != '\0') {
                log_push("", sL.note);
            }
            lobby_debug("[lobby] note: %s\n", sL.note);
        }
        memcpy(sL.code, pkt + 360, 8);
        sL.code[7] = '\0';
        {
            uint32_t was = sL.t.gen;
            int wasState = sL.t.state, wasChamp = sL.t.champ, wasMode = sL.mode, wasEntrants = sL.entrants;
            tour_get(pkt);
            changed |= sL.mode != wasMode || sL.entrants != wasEntrants;
            if (sL.t.size >= 2 && (sL.t.gen != was || sL.t.state != wasState || sL.t.champ != wasChamp)) {
                lobby_debug("[lobby] tournament %u: %s, %d entrants, bracket of %d%s%s\n", (unsigned)sL.t.gen,
                        sL.t.state == 1 ? "on" : sL.t.state == 2 ? "over" : "called off", sL.t.count, sL.t.size,
                        sL.t.champ >= 0 ? ", champion " : "", sL.t.champ >= 0 ? sL.t.name[sL.t.champ] : "");
            }
        }
        if (changed) {
            log_table();
        }
        if (sL.wantReady >= 0) {
            int me;
            for (me = 0; me < LOBBY_SLOTS && sL.ids[me] != sL.myId; me++) {
            }
            if (me < LOBBY_SLOTS && sL.ready[me] == sL.wantReady) {
                sL.wantReady = -1;
            }
        }
    } else if (type == T_L_GO && n >= 60) {
        uint32_t gen = get(pkt, 2);
        struct sockaddr_in t, alt, kn, kn2;
        int role = (int)get(pkt, 3);
        if (gen <= sL.backGen || sL.inGen != 0 || NetIo_SessionMode() != 0 || NetIo_SessionStarting() || sL.leaving || role < 1 || role > 3) {
            return;
        }
        addr_get(pkt, 7, &t);
        addr_get(pkt, 9, &alt);
        addr_get(pkt, 11, &kn);
        addr_get(pkt, 13, &kn2);
        if (t.sin_port == 0) {
            t = sL.owner; /* (the owner's address, as this member knows it) */
        }
        sL.goGen = gen;
        sL.inGen = gen;
        lobby_debug("[lobby] match %u: %s\n", (unsigned)gen, role == 1 ? "hosting it (player 1)" : role == 2 ? "joining it (player 2)" : "watching it");
        NetIo_Go(role, &t, &alt, &kn, &kn2, get(pkt, 4), (int)get(pkt, 5), (int)get(pkt, 6));
        alive_send();
    } else if (type == T_L_BACK && n >= 12 + NOTE) {
        uint32_t gen = get(pkt, 2);
        if (gen > sL.backGen) {
            sL.backGen = gen;
            memcpy(sL.note, pkt + 12, NOTE);
            name_clean(sL.note, NOTE);
            sL.phase = 0;
            if (NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
                char text[NOTE + 32];
                SDL_snprintf(text, sizeof(text), "Back to the lobby%s%s", sL.note[0] != '\0' ? ": " : ".", sL.note);
                NetIo_End(3, text);
            }
        }
    } else if (type == T_L_CLOSE && n >= 12 + NOTE) {
        char note[NOTE];
        memcpy(note, pkt + 12, NOTE);
        name_clean(note, NOTE);
        sL.closedBy = 1;
        tok_forget("the lobby is closed");
        SDL_strlcpy(sL.error, get(pkt, 2) == CLOSE_KICKED ? "The lobby's owner removed you from the lobby." : "The lobby was closed by its owner.", sizeof(sL.error));
        if (NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
            NetIo_End(4, sL.error);
        } else {
            member_closed(sL.error);
        }
    } else if (type == T_L_TOKEN && n >= 20 + TOKEN + 8) {
        uint32_t what = get(pkt, 3);
        if (what == 1 && (!sTok.has || sTok.gen != get(pkt, 2) || memcmp(sTok.tok, pkt + 20, TOKEN) != 0)) {
            tok_keep(get(pkt, 2), pkt + 20, (const char *)pkt + 20 + TOKEN);
        } else if (what == 2 && sTok.has) {
            tok_forget("the owner says it is not needed");
        }
    } else if (type == T_L_SAID && n >= 16 + NAMEB) {
        uint32_t seq = get(pkt, 2), len = get(pkt, 3);
        if (seq > sL.chatIn && len <= TEXT && 16 + NAMEB + len <= (uint32_t)n) {
            sL.chatIn = seq;
            if (sL.chatCount == LOBBY_CHAT) {
                memmove(&sL.chat[0], &sL.chat[1], sizeof(sL.chat[0]) * (LOBBY_CHAT - 1));
                sL.chatCount--;
            }
            memcpy(sL.chat[sL.chatCount].name, pkt + 16, NAMEB);
            name_clean(sL.chat[sL.chatCount].name, NAMEB);
            memcpy(sL.chat[sL.chatCount].text, pkt + 16 + NAMEB, len);
            sL.chat[sL.chatCount].text[len] = '\0';
            name_clean(sL.chat[sL.chatCount].text, TEXT + 1);
            lobby_debug("[lobby] chat: %s: %s\n", sL.chat[sL.chatCount].name, sL.chat[sL.chatCount].text);
            log_push(sL.chat[sL.chatCount].name, sL.chat[sL.chatCount].text);
            sL.chatCount++;
        }
    }
}

static void member_say(const char *text) {
    uint8_t p[20 + TEXT];
    int len = (int)strlen(text), r;
    if (len > TEXT) {
        len = TEXT;
    }
    sL.chatOut++;
    put(p, 0, MAGIC);
    put(p, 1, T_L_CHAT);
    put(p, 2, sL.myId);
    put(p, 3, sL.chatOut);
    put(p, 4, (uint32_t)len);
    memcpy(p + 20, text, (size_t)len);
    for (r = 0; r < 3; r++) {
        send_to(p, 20 + len, &sL.owner);
    }
}

/* ---- both ---- */

static void lobby_off(void) {
    int was = sL.on;
    sL.on = 0;
    sL.state = sL.error[0] != '\0' ? 3 : 0;
    if (was == 1 && sL.byCode) {
        Match_Cancel(); /* (the room is given up) */
    }
    if (NetIo_SessionMode() == 0 && !NetIo_SessionStarting()) {
        NetIo_Close();
    }
    view_update(1);
}

void Lobby_Packet(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type) {
    if (sL.on == 1) {
        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &from->sin_addr, ip, sizeof(ip));
        lobby_debug("[lobby] host received UDP from %s:%u type=%u bytes=%d\n",
                ip, (unsigned)ntohs(from->sin_port), (unsigned)type, n);
        owner_packet(pkt, n, from, type);
    } else if (sL.on == 2) {
        member_packet(pkt, n, from, type);
    }
}

void Lobby_Stranger(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type) {
    if (sL.on != 1) {
        return;
    }
    if ((type == T_HELLO && n >= 16 && get(pkt, 3) == LOBBY_CAP) || type == T_SPEC_HELLO) {
        uint32_t inv[3] = {MAGIC, T_L_INVITE, LOBBY_VERSION}; /* a game that knows lobbies, knocking as for a match of two */
        send_to(inv, sizeof(inv), from);
    } else if (type == T_HELLO && n >= 12) {
        /* a release without lobbies, joining as the second player: answered as a host of another release (it says
           "The host's game is a different version") */
        uint32_t ack[3] = {MAGIC, T_HELLO_ACK, 0};
        send_to(ack, sizeof(ack), from);
    }
}

static void do_cmd(const Cmd *c);
static void join_found(const char addrs[][64], int count, const char *code, const char *self);

/* In a match (match `gen`): how its first battle ended, as this copy sees it. The fight has to have been seen on (a
   battle left over from before is not this match's), and its end held for 0.3 s (longer than any rollback: an end
   that a late input takes back is not taken). Kept until the next match; a member says it with each ALIVE. */
static void result_watch(uint32_t gen) {
    extern int Port_BattleOutcome(int *winner); /* headless.c */
    int bits, state, res;
    uint64_t now = now_ns();
    if (gen == 0) {
        return;
    }
    if (gen != sL.resGen) {
        sL.resGen = gen;
        sL.res = RES_NONE;
        sL.resSawFight = 0;
        sL.resCand = RES_NONE;
    }
    if (sL.res != RES_NONE) {
        return;
    }
    state = Port_BattleOutcome(&bits);
    if (state == 3) {
        sL.resSawFight = 1;
        sL.resCand = RES_NONE;
        return;
    }
    if (!sL.resSawFight || state < 4 || state > 6 || (bits & 0xF) == 0) {
        sL.resCand = RES_NONE;
        return;
    }
    res = (bits & 3) == 1 ? RES_SIDE0 : (bits & 3) == 2 ? RES_SIDE1 : RES_NO_WINNER;
    if (res != sL.resCand) {
        sL.resCand = res;
        sL.resCandNs = now;
    } else if (now - sL.resCandNs > 300000000ull) {
        if (sL.on == 2 && res != RES_NO_WINNER && getenv("BT3_TEST_RESULT_LIE") != NULL) {
            res = res == RES_SIDE0 ? RES_SIDE1 : RES_SIDE0; /* (tests: this member reports the other winner) */
            lobby_debug("[lobby] test: the result reported is the other one\n");
        }
        sL.res = res;
        lobby_debug("[lobby] match %u: the battle ended: %s\n", (unsigned)gen,
                res == RES_SIDE0 ? "player 1 won" : res == RES_SIDE1 ? "player 2 won" : "no winner");
        if (sL.on == 2) {
            alive_send(); /* (the owner hears it at once) */
        }
    }
}

/* Timers of both, with the session or without. */
static void timers(void) {
    uint64_t now = now_ns();
    int k;
    if (NetIo_SessionMode() != 0) {
        result_watch(sL.on == 1 ? (sL.phase == 1 ? sL.gen : 0) : sL.inGen);
    }
    if (sL.on == 1) {
        if (sL.byCode && NetIo_SessionMode() == 0 && !NetIo_SessionStarting()) {
            MatchStatus st;
            Match_Poll(&st);
            if (st.code[0] != '\0' && strcmp(st.code, sL.code) != 0) {
                SDL_strlcpy(sL.code, st.code, sizeof(sL.code));
                lobby_debug("[lobby] code '%s'\n", sL.code);
                sL.state = 2;
                sL.dirty = 1;
            }
            if (st.state == -1 && sL.state == 1) {
                SDL_strlcpy(sL.error, st.error, sizeof(sL.error));
                lobby_debug("[lobby] %s\n", st.error);
                lobby_off();
                return;
            }
            if (st.state >= 2 && !Match_SocketBusy()) {
                char addr[MATCH_WATCH_ADDRS][64];
                int gen, n = Match_Watchers(addr, MATCH_WATCH_ADDRS, &gen), i, j;
                NetIo_WatchService(); /* (knocks at new members' addresses for ten seconds, lets them through the relay) */
                if (gen != sL.watchGen) { /* members who have joined with the code: if one does not reach this side in eight
                                             seconds, a relay address is asked for (once) */
                    sL.watchGen = gen;
                    for (i = 0; i < n; i++) {
                        struct in_addr ip;
                        uint32_t h = 2166136261u;
                        const char *c;
                        int t;
                        if (strncmp(addr[i], "seen=", 5) != 0 || inet_pton(AF_INET, addr[i] + 5, &ip) != 1) {
                            continue;
                        }
                        for (c = i > 0 ? addr[i - 1] : addr[i]; *c != '\0'; c++) { /* (one member: its address before "seen=") */
                            h = (h ^ (uint8_t)*c) * 16777619u;
                        }
                        for (t = 0; t < sL.toldCount && sL.told[t] != h; t++) {
                        }
                        if (t < sL.toldCount) {
                            continue; /* (told of before) */
                        }
                        if (sL.toldCount < 64) {
                            sL.told[sL.toldCount++] = h;
                        }
                        for (j = 0; j < 16 && sL.pend[j].ip != ip.s_addr; j++) {
                        }
                        for (k = 1; j == 16 && k < LOBBY_SLOTS; k++) { /* (it came before the service told of it) */
                            if (sL.m[k].on && !sL.m[k].claimed && now - sL.m[k].joinedNs < 15 * SEC) {
                                sL.m[k].claimed = 1;
                                j = -1;
                            }
                        }
                        if (j == 16) {
                            for (j = 0; j < 16 && sL.pend[j].ip != 0; j++) {
                            }
                            if (j < 16) {
                                sL.pend[j].ip = ip.s_addr;
                                sL.pend[j].since = now;
                            }
                        }
                    }
                }
                for (j = 0; j < 16; j++) {
                    if (sL.pend[j].ip == 0) {
                        continue;
                    }
                    for (k = 1; k < LOBBY_SLOTS && !(sL.m[k].on && sL.m[k].addr.sin_addr.s_addr == sL.pend[j].ip); k++) {
                    }
                    if (k < LOBBY_SLOTS || now - sL.pend[j].since > 60 * SEC) {
                        sL.pend[j].ip = 0; /* (it has come, or it is long gone) */
                    } else if (now - sL.pend[j].since > 8 * SEC && !sL.relayAsked && !Relay_Active()) {
                        sL.relayAsked = 1;
                        lobby_debug("[lobby] a member has not reached this side in eight seconds: a relay address is asked for\n");
                        Match_RelayAsk();
                    }
                }
                if (Match_RelayGen() != sL.relayGen) { /* a relay address has come: the members and the knocked are let through it */
                    sL.relayGen = Match_RelayGen();
                    for (k = 1; k < LOBBY_SLOTS; k++) {
                        if (sL.m[k].on) {
                            Relay_Permit(NetIo_Sock(), &sL.m[k].addr);
                        }
                    }
                    for (i = 0; i < n; i++) {
                        struct sockaddr_in a;
                        char text[64], *colon;
                        memset(&a, 0, sizeof(a));
                        a.sin_family = AF_INET;
                        SDL_strlcpy(text, strncmp(addr[i], "seen=", 5) == 0 ? addr[i] + 5 : addr[i], sizeof(text));
                        colon = strrchr(text, ':');
                        if (colon != NULL && strncmp(addr[i], "seen=", 5) != 0) {
                            *colon = '\0';
                        }
                        if (strncmp(text, "relay=", 6) != 0 && inet_pton(AF_INET, text, &a.sin_addr) == 1) {
                            Relay_Permit(NetIo_Sock(), &a);
                        }
                    }
                    lobby_debug("[lobby] the relay address is there: members who cannot reach this side directly come through it\n");
                }
            }
        }
        for (k = 1; k < LOBBY_SLOTS; k++) {
            Member *m = &sL.m[k];
            if (!m->on) {
                continue;
            }
            if (now > m->heardNs && now - m->heardNs > QUIET_NS) {
                member_drop(k, "lost the connection to the lobby", 1);
                continue;
            }
            if (sL.phase == 1) {
                int player = k == sL.players[0] || k == sL.players[1];
                if (player && m->wasIn && m->inGen != sL.gen) {
                    char text[NOTE];
                    SDL_snprintf(text, sizeof(text), "%s's match ended (their game left it).", m->name);
                    match_end(text);
                    break;
                }
                if (m->goGen != sL.gen && now - m->goSentNs > 500000000ull && (player || member_place(k) >= 2)) {
                    go_send(k); /* (not taken yet: again) */
                } else if (!player && m->goGen == sL.gen && m->inGen != sL.gen && now - m->goSentNs > 2 * SEC) {
                    go_send(k); /* a spectator back in the lobby window while the match is on (its connection, a
                                   desync): watching again */
                }
            }
        }
        t_decide();
        t_holds();
        if (sL.phase == 1 && now - sL.goNs > MATCH_CONNECT_NS) {
            int a = sL.players[0], b = sL.players[1];
            int ok = (a == 0 ? NetIo_Connected() : sL.m[a].connected) && (b == 0 ? NetIo_Connected() : sL.m[b].connected);
            if (!ok) {
                sL.goNs = now + 3600 * SEC; /* (said once) */
                match_end("The two players could not connect to each other.");
            }
        }
        if (sL.dirty ? now - sL.stateNs > 50000000ull : now - sL.stateNs > SEC) {
            state_all();
        }
    } else if (sL.on == 2) {
        if (sL.state == 1) {
            MatchStatus st;
            Match_Poll(&st);
            if (st.state == 3 && st.lobby && st.addrGen != sL.candGen) { /* the owner's relay address has come */
                sL.candGen = st.addrGen;
                join_found((const char (*)[64])st.peerAddr, st.peerCount, NULL, NULL);
            }
            if (now - sL.helloNs > 200000000ull) {
                int i;
                sL.helloNs = now;
                for (i = 0; i < sL.candCount; i++) {
                    if (i == sL.candRelay ? now - sL.candSince < 3 * SEC && getenv("BT3_RELAY_ONLY") == NULL : getenv("BT3_RELAY_ONLY") != NULL) {
                        continue; /* (three seconds for the direct ways first) */
                    }
                    hello_send(&sL.cand[i]);
                }
            }
            if (sL.probe && now - sL.candSince > 2 * SEC && now - sL.probeNs > SEC) {
                /* No answer yet from a typed address: it may be a game of a release before 0.1.39 hosting a match
                   of two, which ignores a lobby's greeting. A spectator's greeting too short to be one is answered
                   by such a game with SPEC_NO_VERSION (taken in Lobby_Pump: "a different version"), and a 1v1
                   greeting of this version makes its page say "another version tried". A lobby's owner answers
                   both with an invitation, ignored here (its answer to the lobby's greeting is the one taken). */
                uint32_t probe[3] = {MAGIC, T_SPEC_HELLO, 0}, hello[4] = {MAGIC, T_HELLO, (uint32_t)NetIo_Version(), LOBBY_CAP};
                sL.probeNs = now;
                send_to(probe, sizeof(probe), &sL.cand[0]);
                send_to(hello, sizeof(hello), &sL.cand[0]);
            }
            if (now - sL.candSince > CONNECT_NS) {
                if (sTok.has && strcmp(sTok.code, sJoinCode) == 0) {
                    tok_forget("its lobby does not answer");
                }
                lobby_debug("[lobby] member connection timed out after 25 seconds: no valid T_L_STATE/T_L_NO with nonce %u\n",
                        (unsigned)sL.nonce);
                Port_UiNotice("O anfitrião não respondeu. Verifique se os dois usam a mesma versão do app.");
                member_closed("The lobby's owner did not answer. A firewall on the owner's PC may be blocking the game: "
                              "allow Tenkaichi3Decomp through it (Windows Firewall: allow an app), then try again.");
                return;
            }
        } else {
            if (now - sL.aliveNs > SEC) {
                alive_send();
            }
            if (now > sL.heardNs && now - sL.heardNs > QUIET_NS) {
                SDL_strlcpy(sL.error, "The connection to the lobby's owner was lost: the lobby is closed.", sizeof(sL.error));
                sL.closedBy = 1;
                if (NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
                    NetIo_End(4, sL.error);
                    sL.heardNs = now; /* (once) */
                } else {
                    member_closed(sL.error);
                    return;
                }
            }
        }
    }
    view_update(0);
}

/* The window's and the test's wishes. */
static void do_cmd(const Cmd *c) {
    if (c->kind == CMD_SAY) {
        if (sL.on == 1) {
            owner_say(c->a, c->text);
        } else if (sL.on == 2 && sL.state == 2) {
            member_say(c->text);
        }
        return;
    }
    if (c->kind != CMD_DO) {
        return;
    }
    if (c->a == LOBBY_OK) {
        if (sL.on == 0) {
            sL.state = 0;
            sL.error[0] = '\0';
            view_update(1);
        }
        return;
    }
    if (sL.on == 0) {
        return;
    }
    if (c->a == LOBBY_LEAVE) {
        int k, r;
        sL.error[0] = '\0';
        if (sL.on == 1) {
            sL.closing = 1;
            lobby_debug("[lobby] the owner closes the lobby\n");
            for (r = 0; r < 3; r++) {
                for (k = 1; k < LOBBY_SLOTS; k++) {
                    if (sL.m[k].on) {
                        close_send(&sL.m[k].addr, CLOSE_CLOSED, "");
                    }
                }
            }
        } else {
            uint32_t p[3] = {MAGIC, T_L_LEAVE, sL.myId};
            sL.leaving = 1;
            tok_forget("left on purpose");
            lobby_debug("[lobby] leaving the lobby\n");
            for (r = 0; r < 3; r++) {
                send_to(p, sizeof(p), &sL.owner);
            }
        }
        if (NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
            NetIo_End(5, sL.on == 1 ? "You closed the lobby." : "You left the lobby.");
        } else {
            lobby_off();
        }
        return;
    }
    if (sL.on == 1) {
        owner_do(c->a, c->b, (signed char)c->text[0]);
    } else if (c->a == LOBBY_READY && sL.state == 2) {
        sL.wantReady = c->b != 0;
        alive_send();
    }
}

static void owner_start(int byCode, int port, const char *r) {
    int local, s;
    goodbye_arm();
    memset(&sL, 0, sizeof(sL));
    local = NetIo_Open(byCode ? 0 : port);
    if (local < 0) {
        SDL_snprintf(sL.error, sizeof(sL.error), "Port %d cannot be used (another program has it?).", port);
        sL.state = 3;
        view_update(1);
        return;
    }
    sL.on = 1;
    sL.state = byCode ? 1 : 2;
    sL.byCode = byCode;
    sL.port = local;
    sL.battle = (int)(signed char)r[0] % 5;
    sL.time = (int)(signed char)r[1] % 5;
    sL.speed = (unsigned char)r[2];
    sL.roll = (int)(signed char)r[3];
    sL.delay = (int)(signed char)r[4];
    if (sL.speed < 100 || sL.speed > 150) { sL.speed = 114; }
    if (sL.roll < 0 || sL.roll > 8) { sL.roll = 8; }
    if (sL.delay < 0 || sL.delay > 7) { sL.delay = 0; }
    if (sL.battle < 0) { sL.battle = 0; }
    if (sL.time < 0) { sL.time = 3; }
    for (s = 0; s < LOBBY_SLOTS; s++) {
        sL.slot[s] = -1;
    }
    sL.t.cur = sL.t.champ = sL.t.playing = sL.t.choose = -1;
    sL.entrants = 2;
    sL.m[0].on = 1;
    sL.m[0].id = sL.nextId = 1;
    SDL_strlcpy(sL.m[0].name, NetIo_MyName(), NAMEB);
    sL.slot[0] = 0;
    sL.players[0] = sL.players[1] = -1;
    table_make();
    if (byCode) {
        Relay_Close(-1);
        Match_Begin(5, NetIo_Sock(), local, "", NetIo_MyName(), NetIo_Version());
    }
    lobby_debug("[lobby] made%s, on port %d\n", byCode ? " (a room code is asked for)" : "", local);
    log_table();
    view_update(1);
}

static void member_start(void) {
    int was = sL.candCount;
    struct sockaddr_in cand[MATCH_ADDRS + 1];
    char self[64];
    int relay = sL.candRelay;
    memcpy(cand, sL.cand, sizeof(cand));
    SDL_strlcpy(self, sL.self, sizeof(self));
    goodbye_arm();
    memset(&sL, 0, sizeof(sL));
    memcpy(sL.cand, cand, sizeof(cand));
    sL.candCount = was;
    sL.candRelay = relay;
    SDL_strlcpy(sL.self, self, sizeof(sL.self));
    sL.on = 2;
    sL.state = 1;
    sL.speed = 114;
    sL.t.cur = sL.t.champ = sL.t.playing = sL.t.choose = -1;
    sL.wantReady = -1;
    sL.candSince = now_ns();
    sL.nonce = os_random32();
    if (sL.nonce == 0) {
        sL.nonce = 1;
    }
    {
        MatchStatus st;
        Match_Poll(&st);
        sL.candGen = st.addrGen;
    }
    view_update(1);
}

static void join_found(const char addrs[][64], int count, const char *code, const char *self) {
    int i;
    struct sockaddr_in cand[MATCH_ADDRS + 1];
    int n = 0, relay = -1;
    for (i = 0; i < count && n < MATCH_ADDRS; i++) {
        char text[64], *colon;
        const char *a = addrs[i];
        struct sockaddr_in *c = &cand[n];
        if (strncmp(a, "seen=", 5) == 0) {
            continue;
        }
        if (strncmp(a, "relay=", 6) == 0) {
            a += 6;
            relay = n;
        }
        SDL_strlcpy(text, a, sizeof(text));
        colon = strrchr(text, ':');
        memset(c, 0, sizeof(*c));
        c->sin_family = AF_INET;
        if (colon != NULL) {
            *colon = '\0';
            c->sin_port = htons((uint16_t)atoi(colon + 1));
            if (inet_pton(AF_INET, text, &c->sin_addr) == 1 && c->sin_port != 0) {
                n++;
                continue;
            }
        }
        if (relay == n) {
            relay = -1;
        }
    }
    if (code == NULL) { /* (more addresses for a member already connecting: the owner's relay address came) */
        memcpy(sL.cand, cand, sizeof(cand));
        sL.candCount = n;
        sL.candRelay = relay;
        return;
    }
    memcpy(sL.cand, cand, sizeof(cand));
    sL.candCount = n;
    sL.candRelay = relay;
    SDL_strlcpy(sL.self, self != NULL ? self : "", sizeof(sL.self));
    SDL_strlcpy(sJoinCode, code, sizeof(sJoinCode));
    member_start();
    lobby_debug("[lobby] the code is a lobby's: joining it (the owner at");
    for (i = 0; i < sL.candCount; i++) {
        char text[32];
        if (inet_ntop(AF_INET, &sL.cand[i].sin_addr, text, sizeof(text)) != NULL) {
            fprintf(stderr, " %s%s:%d", i == sL.candRelay ? "relay " : "", text, ntohs(sL.cand[i].sin_port));
        }
    }
    fprintf(stderr, ")\n");
}

void Lobby_JoinFound(const char addrs[][64], int count, const char *code, const char *self) {
    Cmd c;
    if (code == NULL) { /* (the game's thread, a member connecting: more of the owner's addresses) */
        join_found(addrs, count, NULL, NULL);
        return;
    }
    memset(&c, 0, sizeof(c)); /* (from the window's thread: done on the game's) */
    c.kind = CMD_JOIN_FOUND;
    c.count = count < MATCH_ADDRS ? count : MATCH_ADDRS;
    memcpy(c.addrs, addrs, sizeof(c.addrs[0]) * (size_t)c.count);
    SDL_strlcpy(c.text, code, sizeof(c.text));
    SDL_strlcpy(c.self, self != NULL ? self : "", sizeof(c.self));
    SDL_LockSpinlock(&sLock);
    memset(&sView, 0, sizeof(sView));
    sView.state = 1;
    SDL_UnlockSpinlock(&sLock);
    queue_put(&c);
}

static void join_address_found(const struct sockaddr_in *owner) {
    sL.cand[0] = *owner;
    sL.candCount = 1;
    sL.candRelay = -1;
    sL.self[0] = '\0';
    sJoinCode[0] = '\0';
    member_start();
    lobby_debug("[lobby] the address is a lobby's: joining it\n");
}

void Lobby_JoinAddress(const struct sockaddr_in *owner) {
    Cmd c;
    memset(&c, 0, sizeof(c));
    c.kind = CMD_JOIN_ADDR;
    c.a = -1; /* (the socket is there already: the 1v1 lobby's) */
    c.owner = *owner;
    SDL_LockSpinlock(&sLock);
    memset(&sView, 0, sizeof(sView));
    sView.state = 1;
    SDL_UnlockSpinlock(&sLock);
    queue_put(&c);
}

static void join_address(const char *address, int port) {
    struct sockaddr_in a;
    memset(&sL, 0, sizeof(sL));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, address, &a.sin_addr) != 1) {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(address, NULL, &hints, &res) != 0 || res == NULL) {
            SDL_strlcpy(sL.error, "That address is not known.", sizeof(sL.error));
            sL.state = 3;
            view_update(1);
            return;
        }
        a.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    if (NetIo_Open(0) < 0) {
        SDL_strlcpy(sL.error, "No socket could be opened.", sizeof(sL.error));
        sL.state = 3;
        view_update(1);
        return;
    }
    join_address_found(&a);
    sL.probe = 1;
}

/* BT3_LOBBY_DO (tests): the next command, when its time has come. */
static void script(void) {
    static const char *at;
    static uint64_t since;
    static int begun;
    const char *e = getenv("BT3_LOBBY_DO");
    char cmd[200], *colon;
    const char *end;
    double secs;
    int i;
    uint64_t now = now_ns();
    if (e == NULL || sL.on == 0) {
        return;
    }
    if (!begun) {
        begun = 1;
        at = e;
        since = now;
    }
    if (at == NULL || *at == '\0') {
        return;
    }
    end = strchr(at, '|');
    i = end != NULL ? (int)(end - at) : (int)strlen(at);
    if (i >= (int)sizeof(cmd)) {
        at = NULL;
        return;
    }
    memcpy(cmd, at, (size_t)i);
    cmd[i] = '\0';
    colon = strchr(cmd, ':');
    if (colon == NULL) {
        at = NULL;
        return;
    }
    *colon = '\0';
    secs = atof(cmd);
    if ((double)(now - since) / 1e9 < secs) {
        return;
    }
    {
        const char *c = colon + 1;
        int a = 0, b = 0, done = 1, members = 0, s;
        for (s = 0; s < LOBBY_SLOTS; s++) {
            members += sL.ids[s] != 0;
        }
        if (strncmp(c, "start", 5) == 0) {
            done = sL.on == 1 && sL.ids[0] != 0 && sL.ids[1] != 0 && sL.phase == 0 && NetIo_SessionMode() == 0;
            if (done) {
                owner_do(LOBBY_START, 0, 0);
            }
        } else if (strncmp(c, "back", 4) == 0) {
            Port_LobbyDo(LOBBY_BACK, 0, 0);
        } else if (strncmp(c, "leave", 5) == 0) {
            Port_LobbyDo(LOBBY_LEAVE, 0, 0);
        } else if (strncmp(c, "quit", 4) == 0) { /* the program ends as when its window is closed (atexit) */
            lobby_debug("[lobby] test: quit\n");
            exit(0);
        } else if (sscanf(c, "kick %d", &a) == 1) {
            Port_LobbyDo(LOBBY_KICK, a, 0);
        } else if (sscanf(c, "move %d %d", &a, &b) == 2) {
            Port_LobbyDo(LOBBY_MOVE, a, b);
        } else if (sscanf(c, "rule %d %d", &a, &b) == 2) {
            Port_LobbyDo(LOBBY_RULE, a, b);
        } else if (strncmp(c, "ready", 5) == 0) {
            Port_LobbyDo(LOBBY_READY, 1, 0);
        } else if (strncmp(c, "say ", 4) == 0) {
            Port_LobbySay(c + 4);
        } else if (sscanf(c, "wait %d", &a) == 1) {
            done = members >= a && sL.state == 2;
        } else if (sscanf(c, "waitseq %d", &a) == 1) {
            extern int Port_BattleSeqState(void); /* headless.c: the battle sequence's state (6: the result menu), -1 none */
            done = Port_BattleSeqState() == a;
        } else if (sscanf(c, "waitready %d", &a) == 1) {
            int ready = 0;
            for (s = 0; s < LOBBY_SLOTS; s++) {
                ready += sL.ids[s] != 0 && sL.ready[s];
            }
            done = ready >= a;
        } else if (strncmp(c, "tour", 4) == 0) {
            Port_LobbyDo(LOBBY_TOUR_NEW, 0, 0);
        } else if (strncmp(c, "next", 4) == 0) {
            done = sL.on == 1 && t_can_start() && NetIo_SessionMode() == 0 && !NetIo_SessionStarting();
            if (done) {
                owner_do(LOBBY_START, 0, 0);
            }
        } else if (sscanf(c, "choose %d", &a) == 1) { /* (once the owner is asked to choose) */
            done = sL.on == 1 && sL.t.choose >= 0 && sL.phase == 0 && NetIo_SessionMode() == 0;
            if (done) {
                owner_do(LOBBY_TOUR_CHOOSE, a, 0);
            }
        } else if (strncmp(c, "waittour", 8) == 0) {
            done = sL.t.state == 2 && sL.phase == 0 && NetIo_SessionMode() == 0 && !NetIo_SessionStarting();
        } else if (strncmp(c, "waitmatch", 9) == 0) {
            done = NetIo_SessionMode() != 0;
        } else if (strncmp(c, "waitlobby", 9) == 0) {
            done = NetIo_SessionMode() == 0 && !NetIo_SessionStarting() && sL.phase == 0;
        }
        if (!done) {
            return;
        }
        lobby_debug("[lobby] test: %s\n", c);
    }
    at = end != NULL ? end + 1 : NULL;
    since = now;
}

static int session_can(void) { /* a lobby's matches begin without a start again of the program (the 64-bit programs) */
    if (Port_SessionCan()) {
        return 1;
    }
    memset(&sL, 0, sizeof(sL));
    SDL_strlcpy(sL.error, "Lobbies need the game's state list (the .mem file beside the program): copy all the files of the release.", sizeof(sL.error));
    sL.state = 3;
    view_update(1);
    return 0;
}

/* No lobby is made or joined while an update is being downloaded or waits to be installed (gs/net_update.c, net.c's
   Port_OnlineBlocked): the others would soon be on another version. A lobby that is on goes on. */
static int online_ok(void) {
    extern const char *Port_OnlineBlocked(void);
    const char *off = Port_OnlineBlocked();
    if (off == NULL) {
        return 1;
    }
    memset(&sL, 0, sizeof(sL));
    SDL_strlcpy(sL.error, off, sizeof(sL.error));
    sL.state = 3;
    lobby_debug("[lobby] %s\n", off);
    view_update(1);
    return 0;
}

static void queue_run(void) {
    Cmd c[16];
    int n, i;
    SDL_LockSpinlock(&sLock);
    for (i = 0; i < sQueueN; i++) {
        if (sQueue[i].kind == CMD_CREATE || sQueue[i].kind == CMD_JOIN_ADDR || sQueue[i].kind == CMD_JOIN_FOUND) {
            break;
        }
    }
    if (i < sQueueN && !NetIo_FingerprintKnown()) {
        /* a lobby is made or joined once this game's gameplay fingerprint is known (it goes with the greeting, and
           the owner compares the members' with its own): the commands wait in the queue meanwhile */
        static int said;
        SDL_UnlockSpinlock(&sLock);
        if (!said) {
            said = 1;
            lobby_debug("[lobby] this game's gameplay data are still being checked: the lobby begins when its fingerprint is known\n");
        }
        return;
    }
    n = sQueueN;
    memcpy(c, sQueue, sizeof(c[0]) * (size_t)n);
    sQueueN = 0;
    SDL_UnlockSpinlock(&sLock);
    for (i = 0; i < n; i++) {
        if (c[i].kind == CMD_CREATE) {
            if (sL.on == 0 && NetIo_SessionMode() == 0 && session_can() && online_ok()) {
                owner_start(c[i].a, c[i].b, c[i].text);
            }
        } else if (c[i].kind == CMD_JOIN_ADDR) {
            if (sL.on == 0 && NetIo_SessionMode() == 0 && session_can() && online_ok()) {
                if (c[i].a < 0) {
                    join_address_found(&c[i].owner);
                } else {
                    join_address(c[i].text, c[i].a);
                }
            }
        } else if (c[i].kind == CMD_JOIN_FOUND) {
            if (sL.on == 0 && NetIo_SessionMode() == 0 && session_can() && online_ok()) {
                join_found((const char (*)[64])c[i].addrs, c[i].count, c[i].text, c[i].self);
            }
        } else {
            do_cmd(&c[i]);
        }
    }
}

void Lobby_Tick(void) {
    static uint64_t last;
    uint64_t now = now_ns();
    if (sL.on == 0 || now - last < 20000000ull) {
        return;
    }
    last = now;
    queue_run();
    script();
    if (sL.on != 0) {
        timers();
    }
}

void Lobby_Pump(void) {
    uint8_t pkt[2048];
    struct sockaddr_in from;
    int n;
    tok_load();
    {
        static int begun;
        const char *e = getenv("BT3_LOBBY");
        if (!begun && e != NULL && (strncmp(e, "lobby", 5) == 0)) {
            static unsigned blanks;
            if (getenv("BT3_LOBBY_AT") == NULL || ++blanks >= (unsigned)atoi(getenv("BT3_LOBBY_AT"))) {
                char addr[128];
                int port;
                begun = 1;
                if (getenv("BT3_NET_NAME") != NULL) {
                    extern void Port_NetNameSet(const char *name);
                    Port_NetNameSet(getenv("BT3_NET_NAME"));
                }
                if (strncmp(e, "lobbyjoin:", 10) == 0 && sscanf(e + 10, "%127[^:]:%d", addr, &port) == 2) {
                    Port_LobbyJoin(addr, port);
                } else {
                    Port_LobbyCreate(e[5] != ':', e[5] == ':' ? atoi(e + 6) : 0, 0, 3, 100, 8, 2);
                }
            }
        }
    }
    if (NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
        return; /* (the session's look at the socket does it) */
    }
    queue_run();
    if (sL.on == 0) {
        view_update(0);
        return;
    }
    script();
    if (sL.on == 0) {
        return;
    }
    if (NetIo_Sock() >= 0 && !Match_SocketBusy()) {
        MatchStatus st;
        int ready = 1;
        if (sL.byCode || (sL.on == 2 && sL.state == 1)) {
            Match_Poll(&st);
            ready = !(st.state == 1); /* (the matchmaking's thread asks STUN on this socket at first: kept off) */
        }
        while (ready && NetIo_Sock() >= 0 && (n = NetIo_Recv(pkt, (int)sizeof(pkt), &from)) >= 8) {
            uint32_t magic = get(pkt, 0), type = get(pkt, 1);
            if (magic != MAGIC) {
                continue;
            }
            if (type >= T_LOBBY_FIRST && type <= T_LOBBY_LAST) {
                Lobby_Packet(pkt, n, &from, type);
            } else if (type == T_HELLO || type == T_SPEC_HELLO) {
                Lobby_Stranger(pkt, n, &from, type);
            } else if (type == T_SPEC_NO && n >= 12 && sL.on == 2 && sL.state == 1 && sL.probe && same(&from, &sL.cand[0])
                       && get(pkt, 2) == SPEC_NO_VERSION) {
                /* (the answer to the probe: a game at that address that is not a lobby of this release) */
                member_closed("The game at that address is a different version: joining needs the same release.");
            }
            if (sL.on == 0) {
                return;
            }
        }
    }
    timers();
}

void Lobby_SessionLeaving(void) {
    int k, r;
    if (sL.on == 1) {
        if (sL.closing) {
            for (r = 0; r < 3; r++) {
                for (k = 1; k < LOBBY_SLOTS; k++) {
                    if (sL.m[k].on) {
                        close_send(&sL.m[k].addr, CLOSE_CLOSED, "");
                    }
                }
            }
        } else {
            match_over();
        }
    } else if (sL.on == 2 && sL.leaving) {
        uint32_t p[3] = {MAGIC, T_L_LEAVE, sL.myId};
        for (r = 0; r < 3; r++) {
            send_to(p, sizeof(p), &sL.owner);
        }
    }
}

int Lobby_SessionEnded(void) {
    if (sL.on == 0) {
        return 0;
    }
    if (sL.on == 1) {
        if (!sL.closing) {
            match_over();
        }
        if (sL.closing) {
            sL.on = 0;
            sL.state = 0;
            if (sL.byCode) {
                Match_Cancel();
            }
            view_update(1);
            return 0;
        }
        log_table();
    } else {
        sL.inGen = 0;
        sL.phase = sL.backGen >= sL.goGen ? 0 : sL.phase;
        if (sL.leaving || sL.closedBy) {
            sL.on = 0;
            sL.state = sL.closedBy ? 3 : 0;
            view_update(1);
            return 0;
        }
        alive_send();
    }
    sL.heardNs = now_ns(); /* (the time in the session does not count as silence) */
    {
        int k;
        for (k = 1; k < LOBBY_SLOTS; k++) {
            sL.m[k].heardNs = sL.heardNs;
        }
    }
    view_update(1);
    return 1;
}

