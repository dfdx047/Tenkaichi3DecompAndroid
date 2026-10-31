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
 *   ALIVE   member -> owner   {id, state seen, ready, last match it was sent to, the match it is in, connected}, every second
 *   GO      owner -> member   {match, role, rules, rollback, delay, 4 addresses}: begin that match's session
 *   BACK    owner -> member   {match, note}: the match is over, back to the lobby window
 *   LEAVE   member -> owner   {id}: this member leaves the lobby
 *   CLOSE   owner -> member   {why, note}: the lobby is closed, or this member was removed
 *   CHAT    member -> owner   {id, number, length, text}; the owner passes each line on to all (SAID)
 *   SAID    owner -> member   {number, length, name, text}
 *   INVITE  owner -> a game that knocked with the greeting of a match of two (by address): "this is a lobby"
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
 * waitmatch (until a match is on), waitlobby (until none is). The lobby's changes are written to the log.
 */
#ifdef _WIN32
#define _CRT_RAND_S /* (rand_s: the system's random numbers, RtlGenRandom) */
#endif
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
enum { T_HELLO = 1, T_HELLO_ACK = 2, T_SPEC_HELLO = 20 };
enum { T_L_HELLO = 30, T_L_NO, T_L_STATE, T_L_ALIVE, T_L_GO, T_L_BACK, T_L_LEAVE, T_L_CLOSE, T_L_CHAT, T_L_SAID, T_L_INVITE };
enum { LOBBY_VERSION = 2, /* 2: STATE carries the member's nonce (its greeting's), which a member connecting asks for */ FP_MARK = 0x46505231, LOBBY_CAP = 0x4C4F4231 /* "1BOL": a game's 1v1 greeting says it knows lobbies */ };
enum { LNO_VERSION = 1, LNO_DATA = 2, LNO_FULL = 3 };
enum { CLOSE_CLOSED = 1, CLOSE_KICKED = 2 };
#define NOTE 96
#define NAMEB 20
#define TEXT 160
#define SEC 1000000000ull
#define QUIET_NS (15 * SEC)    /* a member, or the owner, silent this long is gone */
#define CONNECT_NS (25 * SEC)  /* a member that has not reached the owner in this long gives up */
#define MATCH_CONNECT_NS (40 * SEC) /* the two players have not connected to each other in this long: back to the lobby */

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
} Member;

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
} sL;

static SDL_SpinLock sLock;
static PortLobbyView sView;
static Cmd sQueue[16];
static int sQueueN;

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
    SDL_snprintf(t, sizeof(t), "%s, %s, %d%%, rollback %d, delay %s%d", kBattle[sL.battle], kTime[sL.time], sL.speed, sL.roll, sL.delay == 0 ? "auto" : "",
                 sL.delay == 0 ? 0 : sL.delay - 1);
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
    fprintf(stderr, "bt3: lobby: %s\n", text);
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
    fprintf(stderr, "bt3: lobby: places %s; rules %s\n", line, rules_text());
}

static void state_send(int k) {
    uint8_t p[372];
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
    send_to(p, (int)sizeof(p), &sL.m[k].addr);
}

static void state_all(void) {
    int k;
    sL.seq++;
    for (k = 1; k < LOBBY_SLOTS; k++) {
        if (sL.m[k].on) {
            state_send(k);
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
    int me = member_place(0), type = sL.battle == 0 ? 0 : sL.battle == 1 ? 1 : 2, dp = sL.battle >= 2 ? sL.battle - 2 : 0;
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

static void match_begin(void) {
    int k, role;
    struct sockaddr_in t, alt, kn, kn2;
    if (sL.phase != 0 || sL.slot[0] < 0 || sL.slot[1] < 0 || NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
        return;
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
}

/* The owner's match is over (its session has ended): everyone back to the lobby window. */
static void match_over(void) {
    int k, r;
    if (sL.phase == 0) {
        return;
    }
    sL.phase = 0;
    for (r = 0; r < 3; r++) {
        for (k = 1; k < LOBBY_SLOTS; k++) {
            if (sL.m[k].on) {
                back_send(&sL.m[k].addr);
            }
        }
    }
    sL.dirty = 1;
    fprintf(stderr, "bt3: lobby: match %u is over: back to the lobby\n", (unsigned)sL.gen);
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

static void member_drop(int k, const char *why) {
    char text[NOTE];
    int s = member_place(k), player = sL.phase == 1 && (k == sL.players[0] || k == sL.players[1]);
    SDL_snprintf(text, sizeof(text), "%s %s", sL.m[k].name, why);
    sL.m[k].on = 0;
    if (s >= 0) {
        sL.slot[s] = -1;
    }
    table_make();
    note_set(text);
    log_table();
    if (player) {
        match_end(NULL);
    }
}

static void owner_hello(const uint8_t *pkt, int n, const struct sockaddr_in *from) {
    uint32_t why = 0, nonce = n >= 32 ? get(pkt, 7) : 0;
    unsigned long long mine;
    int k, free = -1, s;
    if (n < 40 + NAMEB) {
        return;
    }
    if (get(pkt, 2) != LOBBY_VERSION || get(pkt, 3) != (uint32_t)NetIo_Version()) {
        why = LNO_VERSION;
    } else if (get(pkt, 4) == FP_MARK && NetIo_Fingerprint(&mine) && ((unsigned long long)get(pkt, 5) | (unsigned long long)get(pkt, 6) << 32) != mine) {
        why = LNO_DATA;
    }
    for (k = 1; k < LOBBY_SLOTS; k++) {
        if (sL.m[k].on && same(&sL.m[k].addr, from)) {
            sL.m[k].heardNs = now_ns();
            state_send(k); /* (its greeting came again: the answer was lost) */
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
    if (why != 0) {
        uint32_t no[4] = {MAGIC, T_L_NO, why, nonce};
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
    /* the first free place: a player's while one is free and no match is on, else a spectator's */
    for (s = sL.phase == 1 ? 2 : 0; s < LOBBY_SLOTS && sL.slot[s] >= 0; s++) {
    }
    if (s == LOBBY_SLOTS) {
        for (s = 0; s < LOBBY_SLOTS && sL.slot[s] >= 0; s++) {
        }
    }
    sL.slot[s] = free;
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
        member_drop(k, "left the lobby");
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
    fprintf(stderr, "bt3: lobby: chat: %s: %s\n", sL.m[k].name, sL.chat[sL.chatCount - 1].text);
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
        if (sL.phase == 0 && sL.slot[0] >= 0 && sL.slot[1] >= 0) {
            match_begin();
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
            member_drop(k, "was removed from the lobby");
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
            if (a == LOBBY_RULE_SPEED) {
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
            fprintf(stderr, "bt3: lobby: rules %s\n", rules_text());
        }
    } else if (what == LOBBY_READY) {
        sL.m[0].ready = a != 0;
        table_make();
    }
}

/* ---- a member ---- */

static void hello_send(const struct sockaddr_in *to) {
    uint8_t p[40 + NAMEB];
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
    send_to(p, (int)sizeof(p), to);
}

static void alive_send(void) {
    uint32_t p[8] = {MAGIC, T_L_ALIVE, sL.myId, sL.seqSeen, 0, sL.goGen, sL.inGen, 0};
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
    fprintf(stderr, "bt3: lobby: the program ends: %s\n", sL.on == 1 ? "the lobby is closed for everyone" : "this game leaves the lobby");
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
    fprintf(stderr, "bt3: lobby: %s\n", why);
    lobby_off();
}

static void member_packet(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type) {
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
                fprintf(stderr, "bt3: lobby: an answer from %s:%d without this member's nonce: not taken\n", text, ntohs(from->sin_port));
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
        fprintf(stderr, "bt3: lobby: in the lobby (%s)\n", Relay_IsPeer(from) ? "through the relay" : "the owner answered");
    }
    if (!same(from, &sL.owner)) {
        return;
    }
    sL.heardNs = now_ns();
    if (type == T_L_STATE && n >= 368) {
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
            fprintf(stderr, "bt3: lobby: note: %s\n", sL.note);
        }
        memcpy(sL.code, pkt + 360, 8);
        sL.code[7] = '\0';
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
        fprintf(stderr, "bt3: lobby: match %u: %s\n", (unsigned)gen, role == 1 ? "hosting it (player 1)" : role == 2 ? "joining it (player 2)" : "watching it");
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
        SDL_strlcpy(sL.error, get(pkt, 2) == CLOSE_KICKED ? "The lobby's owner removed you from the lobby." : "The lobby was closed by its owner.", sizeof(sL.error));
        if (NetIo_SessionMode() != 0 || NetIo_SessionStarting()) {
            NetIo_End(4, sL.error);
        } else {
            member_closed(sL.error);
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
            fprintf(stderr, "bt3: lobby: chat: %s: %s\n", sL.chat[sL.chatCount].name, sL.chat[sL.chatCount].text);
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

/* Timers of both, with the session or without. */
static void timers(void) {
    uint64_t now = now_ns();
    int k;
    if (sL.on == 1) {
        if (sL.byCode && NetIo_SessionMode() == 0 && !NetIo_SessionStarting()) {
            MatchStatus st;
            Match_Poll(&st);
            if (st.code[0] != '\0' && strcmp(st.code, sL.code) != 0) {
                SDL_strlcpy(sL.code, st.code, sizeof(sL.code));
                fprintf(stderr, "bt3: lobby: code '%s'\n", sL.code);
                sL.state = 2;
                sL.dirty = 1;
            }
            if (st.state == -1 && sL.state == 1) {
                SDL_strlcpy(sL.error, st.error, sizeof(sL.error));
                fprintf(stderr, "bt3: lobby: %s\n", st.error);
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
                        fprintf(stderr, "bt3: lobby: a member has not reached this side in eight seconds: a relay address is asked for\n");
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
                    fprintf(stderr, "bt3: lobby: the relay address is there: members who cannot reach this side directly come through it\n");
                }
            }
        }
        for (k = 1; k < LOBBY_SLOTS; k++) {
            Member *m = &sL.m[k];
            if (!m->on) {
                continue;
            }
            if (now > m->heardNs && now - m->heardNs > QUIET_NS) {
                member_drop(k, "lost the connection to the lobby");
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
            if (now - sL.candSince > CONNECT_NS) {
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
            fprintf(stderr, "bt3: lobby: the owner closes the lobby\n");
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
            fprintf(stderr, "bt3: lobby: leaving the lobby\n");
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
    fprintf(stderr, "bt3: lobby: made%s, on port %d\n", byCode ? " (a room code is asked for)" : "", local);
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
    member_start();
    fprintf(stderr, "bt3: lobby: the code is a lobby's: joining it (the owner at");
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
    member_start();
    fprintf(stderr, "bt3: lobby: the address is a lobby's: joining it\n");
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
            fprintf(stderr, "bt3: lobby: test: quit\n");
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
        } else if (strncmp(c, "waitmatch", 9) == 0) {
            done = NetIo_SessionMode() != 0;
        } else if (strncmp(c, "waitlobby", 9) == 0) {
            done = NetIo_SessionMode() == 0 && !NetIo_SessionStarting() && sL.phase == 0;
        }
        if (!done) {
            return;
        }
        fprintf(stderr, "bt3: lobby: test: %s\n", c);
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

static void queue_run(void) {
    Cmd c[16];
    int n, i;
    SDL_LockSpinlock(&sLock);
    n = sQueueN;
    memcpy(c, sQueue, sizeof(c[0]) * (size_t)n);
    sQueueN = 0;
    SDL_UnlockSpinlock(&sLock);
    for (i = 0; i < n; i++) {
        if (c[i].kind == CMD_CREATE) {
            if (sL.on == 0 && NetIo_SessionMode() == 0 && session_can()) {
                owner_start(c[i].a, c[i].b, c[i].text);
            }
        } else if (c[i].kind == CMD_JOIN_ADDR) {
            if (sL.on == 0 && NetIo_SessionMode() == 0 && session_can()) {
                if (c[i].a < 0) {
                    join_address_found(&c[i].owner);
                } else {
                    join_address(c[i].text, c[i].a);
                }
            }
        } else if (c[i].kind == CMD_JOIN_FOUND) {
            if (sL.on == 0 && NetIo_SessionMode() == 0 && session_can()) {
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

