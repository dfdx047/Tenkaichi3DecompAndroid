/*
 * Online play, first step: two copies of the game kept in step by exchanging the pads' input (branch netplay).
 *
 * Both copies run the same game from the first vertical blank on. Each has ONE local player (the host is player 1,
 * the one who joins player 2); at every vertical blank a copy sends its player's input for that blank to the other
 * and does not go on before it has the other's input for it ("lockstep"). The game then reads the two pads from
 * here on both machines, so both simulate the same thing. Nothing is predicted and nothing is rewound yet: a slow
 * connection makes both games wait. Rewinding (port/src/gs/state.c) comes on top of this.
 *
 *   BT3_NET_HOST=<port>             wait for the other copy on this UDP port, be player 1
 *   BT3_NET_JOIN=<address>:<port>   connect to a host, be player 2
 *   BT3_NET_DELAY=<n>               a player's input takes effect n blanks after it was made (default 2): the time
 *                                   the other side has to receive it without the game having to wait
 *   BT3_NET_SCRIPT=<file>           testing: the local player's input comes from a table (36 bytes per blank: pad
 *                                   1, pad 2; written by BT3_PAD_TABLE=<file> during an ordinary run), not from
 *                                   the keyboard or a controller
 *   BT3_NET_LOSS=<percent>, BT3_NET_LAG=<ms>   testing: drop that share of the packets sent / hold each one back
 *   BT3_VIEW=0|1                    testing: show that player's view full screen in a two-player battle (what a
 *                                   connected copy does with its own player's)
 *
 * A packet carries the sender's input for the last 16 blanks, so a lost packet is covered by the next ones; while
 * a copy waits it sends its own latest packet again every few milliseconds.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
#endif
#include "gs_internal.h"

extern int Port_PadRead(int socket, unsigned char *data); /* gs_input.c: the keyboard and the controllers */
void Port_NetLeave(void);

enum { PAD = 18, RING = 4096, REDUNDANT = 16, MAGIC = 0x4E335442 /* "BT3N" */ };
enum { T_HELLO = 1, T_HELLO_ACK = 2, T_INPUT = 3 };

static int sMode = -1; /* 0 off, 1 host, 2 join */
static int sSock = -1, sMe, sDelay = 2, sConnected, sLoss, sLagMs;
static struct sockaddr_in sPeer;
static uint8_t sIn[2][RING][PAD]; /* [player][blank % RING] */
static uint32_t sHave[2];         /* inputs known for blanks below this */
static uint32_t sTick;            /* the blank the game is in */
static FILE *sScript;
static uint64_t sWaitNs, sWaits;

static const uint8_t kIdle[PAD] = {0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80};

/* rollback (roll_tick) */
extern int Port_RollCan(void), Port_RollSave(void); /* gs/state.c */
extern void Port_RollBack(int k);
extern int gPortResim;
int Port_NetWarp(void);
static int sRollMax, sResim, sRollBegun;
static uint8_t sUsed[RING][PAD]; /* what the other player's pad read in each blank */
static uint32_t sChecked;        /* blanks below this were given the other player's real input */
static uint32_t sLive;           /* during a re-run: the blank that was the present when it began */
static unsigned sRollbacks, sRollTicks, sStalls;

/* BT3_NET_LATENCY=<ms>: testing, a line that takes that long one way. What is sent waits in a queue. */
static int sLatencyMs;
static struct { uint64_t due; int n; uint8_t data[16 + 16 * PAD]; } sQueue[256];
static unsigned sQueueHead, sQueueTail;

static void queue_flush(void) {
    uint64_t now = SDL_GetTicksNS();
    while (sQueueHead != sQueueTail && sQueue[sQueueHead % 256].due <= now) {
        sendto(sSock, (const char *)sQueue[sQueueHead % 256].data, sQueue[sQueueHead % 256].n, 0, (struct sockaddr *)&sPeer, sizeof(sPeer));
        sQueueHead++;
    }
}

static void send_to_peer(const void *buf, int n) {
    if (sLoss != 0 && (rand() % 100) < sLoss) {
        return;
    }
    if (sLatencyMs > 0 && n <= (int)sizeof(sQueue[0].data) && sQueueTail - sQueueHead < 256) {
        sQueue[sQueueTail % 256].due = SDL_GetTicksNS() + (uint64_t)sLatencyMs * 1000000ull;
        sQueue[sQueueTail % 256].n = n;
        memcpy(sQueue[sQueueTail % 256].data, buf, (size_t)n);
        sQueueTail++;
        return;
    }
    sendto(sSock, buf, n, 0, (struct sockaddr *)&sPeer, sizeof(sPeer));
}

static void send_inputs(void) {
    uint8_t pkt[16 + REDUNDANT * PAD];
    uint32_t magic = MAGIC, type = T_INPUT, n = sHave[sMe] < REDUNDANT ? sHave[sMe] : REDUNDANT, base = sHave[sMe] - n, i;

    memcpy(pkt, &magic, 4);
    memcpy(pkt + 4, &type, 4);
    memcpy(pkt + 8, &base, 4);
    memcpy(pkt + 12, &n, 4);
    for (i = 0; i < n; i++) {
        memcpy(pkt + 16 + i * PAD, sIn[sMe][(base + i) % RING], PAD);
    }
    send_to_peer(pkt, (int)(16 + n * PAD));
}

/* Takes in what has arrived. Returns 1 if anything did. */
static int receive(void) {
    uint8_t pkt[2048];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    int got = 0, n;

    queue_flush();
    while ((n = (int)recvfrom(sSock, (char *)pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &fromLen)) >= 8) {
        uint32_t magic, type;
        memcpy(&magic, pkt, 4);
        memcpy(&type, pkt + 4, 4);
        if (magic != MAGIC) {
            continue;
        }
        got = 1;
        if (type == T_HELLO && sMode == 1) {
            uint32_t ack[2] = {MAGIC, T_HELLO_ACK};
            sPeer = from; /* the host learns the other side's address from its greeting */
            sConnected = 1;
            sendto(sSock, (const char *)ack, sizeof(ack), 0, (struct sockaddr *)&sPeer, sizeof(sPeer));
        } else if (type == T_HELLO_ACK && sMode == 2) {
            sConnected = 1;
        } else if (type == T_INPUT && n >= 16) {
            uint32_t base, count, i, other = (uint32_t)!sMe;
            memcpy(&base, pkt + 8, 4);
            memcpy(&count, pkt + 12, 4);
            if (sMode == 1 && !sConnected) {
                sPeer = from;
            }
            sConnected = 1; /* (an input packet says the greeting got through) */
            for (i = 0; i < count && 16 + (i + 1) * PAD <= (uint32_t)n; i++) {
                if (base + i >= sHave[other]) {
                    if (base + i > sHave[other]) {
                        break; /* a gap: wait for a packet that covers it */
                    }
                    memcpy(sIn[other][(base + i) % RING], pkt + 16 + i * PAD, PAD);
                    sHave[other] = base + i + 1;
                }
            }
        }
    }
    return got;
}

static int sSession; /* a session started from Dragon Net Battle (below), not a plain connected run */
static int sArrived; /* its start-up has reached the character select */

/* Connects: role 1 hosts on `port`, role 2 joins `join` ("address:port"). Waits until the other side is there. */
static void net_start(int role, const char *host, const char *join) {
    struct sockaddr_in local;
    uint64_t t0, last = 0;
    uint32_t i;

    sMode = role;
    sConnected = 0;
    sTick = 0;
    memset(sIn, 0, sizeof(sIn));
    memset(sHave, 0, sizeof(sHave)); /* (a second session in one run starts as clean as the first) */
    sWaitNs = sWaits = 0;
#ifdef _WIN32
    {
        WSADATA w;
        u_long on = 1;
        WSAStartup(MAKEWORD(2, 2), &w);
        sSock = (int)socket(AF_INET, SOCK_DGRAM, 0);
        ioctlsocket(sSock, FIONBIO, &on);
    }
#else
    sSock = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(sSock, F_SETFL, fcntl(sSock, F_GETFL, 0) | O_NONBLOCK);
#endif
    sMe = sMode - 1;
    sDelay = getenv("BT3_NET_DELAY") != NULL ? atoi(getenv("BT3_NET_DELAY")) : 2;
    if (sDelay < 0) { sDelay = 0; }
    if (sDelay > 30) { sDelay = 30; }
    sLoss = getenv("BT3_NET_LOSS") != NULL ? atoi(getenv("BT3_NET_LOSS")) : 0;
    sLagMs = getenv("BT3_NET_LAG") != NULL ? atoi(getenv("BT3_NET_LAG")) : 0;
    sLatencyMs = getenv("BT3_NET_LATENCY") != NULL ? atoi(getenv("BT3_NET_LATENCY")) : 0;
    /* BT3_NET_ROLLBACK=<n>: the game does not wait for the other player's input; it goes on with a guess (what
       they held last) for up to n blanks and, when the real input differs, goes back and runs those blanks again
       (roll_tick). Needs the roll ring of gs/state.c (the 64-bit programs); 0 or unset: wait, as before. */
    sRollMax = getenv("BT3_NET_ROLLBACK") != NULL && Port_RollCan() ? atoi(getenv("BT3_NET_ROLLBACK")) : 0;
    if (sRollMax < 0) { sRollMax = 0; }
    if (sRollMax > 30) { sRollMax = 30; }
    sChecked = sLive = 0;
    sResim = sRollBegun = 0;
    sRollbacks = sRollTicks = sStalls = 0;
    if (getenv("BT3_NET_SCRIPT") != NULL) {
        sScript = fopen(getenv("BT3_NET_SCRIPT"), "rb");
    }
    /* the blanks before any input can have arrived (0 .. delay) are idle for both players */
    for (i = 0; i <= (uint32_t)sDelay; i++) {
        memcpy(sIn[0][i], kIdle, PAD);
        memcpy(sIn[1][i], kIdle, PAD);
    }
    sHave[0] = sHave[1] = (uint32_t)sDelay + 1;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&sPeer, 0, sizeof(sPeer));
    sPeer.sin_family = AF_INET;
    if (sMode == 1) {
        local.sin_port = htons((uint16_t)atoi(host));
        if (bind(sSock, (struct sockaddr *)&local, sizeof(local)) != 0) {
            fprintf(stderr, "bt3: net: cannot use port %s\n", host);
            exit(2);
        }
        fprintf(stderr, "bt3: net: hosting on port %s (player 1), waiting for the other player...\n", host);
    } else {
        char addr[128];
        char *colon;
        snprintf(addr, sizeof(addr), "%s", join);
        colon = strrchr(addr, ':');
        if (colon == NULL) {
            fprintf(stderr, "bt3: net: BT3_NET_JOIN wants <address>:<port>\n");
            exit(2);
        }
        *colon = '\0';
        sPeer.sin_port = htons((uint16_t)atoi(colon + 1));
        if (inet_pton(AF_INET, addr, &sPeer.sin_addr) != 1) {
            struct hostent *he = gethostbyname(addr);
            if (he == NULL) {
                fprintf(stderr, "bt3: net: unknown address %s\n", addr);
                exit(2);
            }
            memcpy(&sPeer.sin_addr, he->h_addr_list[0], sizeof(sPeer.sin_addr));
        }
        fprintf(stderr, "bt3: net: joining %s (player 2)...\n", join);
    }
    t0 = SDL_GetTicksNS();
    while (!sConnected) {
        uint64_t now = SDL_GetTicksNS();
        if (sMode == 2 && now - last > 100000000ull) {
            uint32_t hello[2] = {MAGIC, T_HELLO};
            sendto(sSock, (const char *)hello, sizeof(hello), 0, (struct sockaddr *)&sPeer, sizeof(sPeer));
            last = now;
        }
        receive();
        SDL_Delay(2);
        if (now - t0 > 120000000000ull) {
            fprintf(stderr, "bt3: net: nobody answered in two minutes\n");
            exit(2);
        }
    }
    fprintf(stderr, "bt3: net: connected; input delay %d blanks\n", sDelay);
}

int Port_NetActive(void) {
    if (sMode < 0) { /* first asked: connected from the start if the environment says so */
        const char *host = getenv("BT3_NET_HOST"), *join = getenv("BT3_NET_JOIN");
        sMode = 0;
        if (host != NULL || join != NULL) {
            sSession = getenv("BT3_NET_SESSION") != NULL;
            net_start(host != NULL ? 1 : 2, host, join);
        }
    }
    return sMode > 0;
}

/* A new vertical blank begins (Port_VBlank): this player's input made now takes effect `delay` blanks on; then
   wait until the other player's input for THIS blank is here. */
/* The local player's input of this blank, entered for the blank `delay` later (once per blank). */
static void sample_local(unsigned tick) {
    uint8_t mine[PAD];
    uint32_t at = (uint32_t)tick + (uint32_t)sDelay;

    if (at < sHave[sMe]) {
        return;
    }
    memcpy(mine, kIdle, PAD);
    if (sScript != NULL) {
        uint8_t both[2 * PAD];
        if (fseek(sScript, (long)tick * 2 * PAD, SEEK_SET) == 0 && fread(both, 1, sizeof(both), sScript) == sizeof(both)) {
            memcpy(mine, both + sMe * PAD, PAD);
        }
    } else if (!Port_PadRead(0, mine)) {
        memcpy(mine, kIdle, PAD);
    }
    memcpy(sIn[sMe][at % RING], mine, PAD);
    sHave[sMe] = at + 1;
}

static void gone_quiet(unsigned tick) {
    fprintf(stderr, "bt3: net: the other player has not answered for 15 seconds (blank %u)\n", tick);
    if (sSession) {
        Port_NetLeave(); /* back to the game as it normally is */
    }
    exit(2);
}

/*
 * Rollback. Each blank's state is saved before the blank reads its pads (Port_RollSave). The other player's pad
 * for a blank whose input has not arrived reads as their last known input, and what each blank was given is
 * remembered (sUsed). When input arrives that differs from what a past blank was given, the game goes back to
 * that blank's save and runs up to the present again, without picture or sound (gPortResim), now with the right
 * input. It never runs more than sRollMax blanks ahead of the other player's last known input: there it waits.
 */
static void roll_tick(unsigned tick) {
    uint32_t other = (uint32_t)!sMe, t, limit;
    uint64_t t0, last;

    if (!sRollBegun) {
        sRollBegun = 1;
        sChecked = tick; /* nothing was given to an earlier blank */
    }
    if (sResim && tick >= sLive) {
        sResim = 0; /* caught up: this blank is the present again */
    }
    if (!sResim) {
        sample_local(tick);
        send_inputs();
        receive();
        t0 = last = SDL_GetTicksNS();
        if (tick >= sHave[other] + (uint32_t)sRollMax) {
            sStalls++;
        }
        while (tick >= sHave[other] + (uint32_t)sRollMax) {
            uint64_t now = SDL_GetTicksNS();
            if (now - last > 4000000ull) {
                send_inputs();
                last = now;
            }
            if (!receive()) {
                SDL_DelayNS(200000);
            }
            if (now - t0 > 15000000000ull) {
                gone_quiet(tick);
            }
        }
        limit = sHave[other] < tick ? sHave[other] : tick;
        for (t = sChecked; t < limit && memcmp(sUsed[t % RING], sIn[other][t % RING], PAD) == 0; t++) {
        }
        sChecked = t;
        if (t < limit) { /* blank t was given something else than what they really pressed */
            sLive = tick;
            sResim = 1;
            sRollbacks++;
            sRollTicks += tick - t;
            gPortResim = 1;
            Port_RollBack((int)(tick - t)); /* does not return: goes on after blank t's Port_RollSave below */
        }
        if (tick % 600 == 0 && getenv("BT3_GS_VERBOSE") != NULL) {
            fprintf(stderr, "net: blank %u: %u rollbacks (%.1f blanks each), %u waits in the last 600 blanks\n", tick, sRollbacks,
                    sRollbacks != 0 ? (double)sRollTicks / sRollbacks : 0.0, sStalls);
            sRollbacks = sRollTicks = sStalls = 0;
        }
    }
    if (Port_RollSave() != 0) {
        sTick = tick; /* back at this blank, to run from here again */
    }
    gPortResim = sResim || Port_NetWarp();
    memcpy(sUsed[tick % RING], sIn[other][(tick < sHave[other] ? tick : sHave[other] - 1) % RING], PAD);
}

void Port_NetBeginTick(unsigned tick) {
    uint64_t t0, last;

    if (!Port_NetActive()) {
        return;
    }
    sTick = tick;
    if (sRollMax > 0) {
        roll_tick(tick);
        return;
    }
    sample_local(tick);
    if (sLagMs != 0) {
        SDL_Delay((Uint32)sLagMs); /* (a crude stand-in for a slow line: everything this copy sends is late) */
    }
    send_inputs();
    receive();
    t0 = last = SDL_GetTicksNS();
    while (sHave[!sMe] <= tick) {
        uint64_t now = SDL_GetTicksNS();
        if (now - last > 4000000ull) {
            send_inputs(); /* the other side may be waiting for a packet that was lost */
            last = now;
        }
        if (!receive()) {
            SDL_DelayNS(200000);
        }
        if (now - t0 > 15000000000ull) {
            gone_quiet(tick);
        }
    }
    sWaitNs += SDL_GetTicksNS() - t0;
    sWaits++;
    if (tick % 600 == 0 && getenv("BT3_GS_VERBOSE") != NULL) {
        fprintf(stderr, "net: blank %u: waited for the other player %.2f ms per blank on average\n", tick, (double)sWaitNs / (double)sWaits / 1e6);
        sWaitNs = sWaits = 0;
    }
}

/* What the game reads for a pad during this blank. */
void Port_NetInput(int player, unsigned char *data) {
    if (sRollMax > 0 && player == !sMe) {
        memcpy(data, sUsed[sTick % RING], PAD); /* their real input, or the guess (roll_tick) */
        return;
    }
    memcpy(data, player >= 0 && player < 2 && sTick < sHave[player] ? sIn[player][sTick % RING] : kIdle, PAD);
}

int Port_NetResim(void) {
    return sResim;
}

/* Which player's view this copy shows full screen in a two-player battle: the local player's when online; -1
   otherwise (the two views side by side, as always). BT3_VIEW=0|1: the same without a connection, for looking at
   it and for comparing the game's state between the three ways of showing a battle. */
int Port_NetView(void) {
    static int forced = -2;
    if (forced == -2) {
        const char *e = getenv("BT3_VIEW");
        forced = e != NULL ? atoi(e) : -1;
    }
    if (forced >= 0) {
        return forced & 1;
    }
    return Port_NetActive() ? sMe : -1;
}

/* ---------------------------------------------------------------------------------------------------------------
 * The session started from the game's "Dragon Net Battle" entry.
 *
 * Two copies can only stay in step if they start from the same state, and two players coming from their own menus
 * with their own saves are not in the same state. So a session is a fresh start of the program on both sides:
 *   1. the lobby (the window behind the menu entry): one side hosts, the other joins; the two find each other
 *      (Port_Lobby*, a greeting and its answer);
 *   2. each side then starts the program again with BT3_NET_SESSION=1 and its role, an empty save folder of the
 *      session's own (net_session/), and connects again as above. The game boots without picture and sound and
 *      not held to real time, straight into the versus mode's menu (headless.c), with everything unlocked by the
 *      game's own Save_UnlockAll, so both sides have the same roster;
 *   3. from there it is the game's own two-player versus mode, the host's pad also driving the menus;
 *   4. leaving the versus mode for the main menu ends the session: each side starts the program once more, as
 *      it normally is, with its own save.
 * ------------------------------------------------------------------------------------------------------------- */
#ifndef _WIN32
#include <unistd.h>
#endif

static void relaunch(int role, const char *join, int port) {
    char value[160];
#ifdef _WIN32
    char exe[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
#define SETENV(n, v) SetEnvironmentVariableA(n, v)
#else
#define SETENV(n, v) ((v) != NULL ? setenv(n, v, 1) : unsetenv(n))
#endif
    SETENV("BT3_NET_HOST", NULL);
    SETENV("BT3_NET_JOIN", NULL);
    SETENV("BT3_NET_SESSION", NULL);
    SETENV("BT3_SOUND_TICKS", NULL);
    if (role != 0) {
        if (role == 1) {
            snprintf(value, sizeof(value), "%d", port);
            SETENV("BT3_NET_HOST", value);
        } else {
            snprintf(value, sizeof(value), "%s:%d", join, port);
            SETENV("BT3_NET_JOIN", value);
        }
        SETENV("BT3_NET_SESSION", "1");
        SETENV("BT3_SOUND_TICKS", "1");
        SETENV("BT3_NOMOVIE", "1");
        SETENV("BT3_SAVES", "net_session");
        /* the session's own save folder starts empty: both sides begin from the game's defaults */
        remove("net_session/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3");
        remove("net_session/card1/BASLUS-21678DBZT3/icon.sys");
        remove("net_session/card1/BASLUS-21678DBZT3/dbzsm.ico");
    } else {
        SETENV("BT3_SAVES", NULL); /* (a save folder named on the command line is not carried over a session: known) */
        SETENV("BT3_NOMOVIE", "1");
    }
    fflush(NULL);
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe, sizeof(exe) - 1);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (CreateProcessA(exe, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        ExitProcess(0);
    }
#else
    {
        char *argv[2] = {(char *)"/proc/self/exe", NULL};
        execv("/proc/self/exe", argv);
    }
#endif
    fprintf(stderr, "bt3: net: could not start the program again\n");
    exit(2);
}

/* 1 in a session started from Dragon Net Battle. */
int Port_NetSession(void) {
    return Port_NetActive() && sSession;
}

/* The session without starting the program again (gs/state.c exchanges the game's state; 64-bit programs).
   Begin: called on the game's own thread just before the game is put back to its first moment. */
extern int Port_SessionCan(void);                                    /* gs/state.c */
extern void Port_SessionRequest(int role, const char *address, int port);
extern void Port_SessionReturn(void);
static char sSavesBefore[512];
static int sSavesWasSet;

static void put_env(const char *name, const char *value) {
#ifdef _WIN32
    SetEnvironmentVariableA(name, value);
    _putenv_s(name, value != NULL ? value : "");
#else
    if (value != NULL) { setenv(name, value, 1); } else { unsetenv(name); }
#endif
}

void Port_NetSessionBegin(int role, const char *address, int port) {
    char host[16], join[160];
    static int known;

    if (!known) { /* the player's own save folder, as the program was started */
        const char *saves = getenv("BT3_SAVES");
        known = 1;
        sSavesWasSet = saves != NULL;
        snprintf(sSavesBefore, sizeof(sSavesBefore), "%s", saves != NULL ? saves : "");
    }
    /* the session's own save folder starts empty: both sides begin from the game's defaults */
    remove("net_session/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3");
    remove("net_session/card1/BASLUS-21678DBZT3/icon.sys");
    remove("net_session/card1/BASLUS-21678DBZT3/dbzsm.ico");
    put_env("BT3_SAVES", "net_session");
    snprintf(host, sizeof(host), "%d", port);
    snprintf(join, sizeof(join), "%s:%d", address, port);
    if (sSock >= 0) {
        closesocket(sSock);
        sSock = -1;
    }
    sSession = 1;
    sArrived = 0;
    net_start(role, role == 1 ? host : NULL, role == 2 ? join : NULL);
}

void Port_NetSessionEnd(void) {
    if (sSock >= 0) {
        closesocket(sSock);
        sSock = -1;
    }
    sMode = 0;
    sSession = 0;
    put_env("BT3_SAVES", sSavesWasSet ? sSavesBefore : NULL);
}

/* The session's start-up runs without picture and sound and without waiting for real time until the game has
   reached the versus menu (headless.c says when). */
int Port_NetWarp(void) {
    return Port_NetSession() && !sArrived;
}
void Port_NetArrived(void) {
    sArrived = 1;
}

/* The session is over (the game went back to the main menu, or the other side is gone): back to the game as it
   normally is. */
void Port_NetLeave(void) {
    fprintf(stderr, "bt3: net: the session has ended\n");
    if (Port_SessionCan()) {
        Port_SessionReturn(); /* the game as it was when the session was asked for (does not return) */
    }
    relaunch(0, NULL, 0);
}

/* ---- the lobby: called from the settings code's window (ui.cpp), once a frame ---- */
static int sLobby; /* 0 idle, 1 waiting, 2 found, -1 failed */
static int sLobbyRole, sLobbyPort;
static char sLobbyAddr[128];
static uint64_t sLobbyLast;

int Port_LobbyStart(int host, const char *address, int port) {
    struct sockaddr_in local;

    if (sSock >= 0) {
        closesocket(sSock);
    }
#ifdef _WIN32
    {
        WSADATA w;
        u_long on = 1;
        WSAStartup(MAKEWORD(2, 2), &w);
        sSock = (int)socket(AF_INET, SOCK_DGRAM, 0);
        ioctlsocket(sSock, FIONBIO, &on);
    }
#else
    sSock = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(sSock, F_SETFL, fcntl(sSock, F_GETFL, 0) | O_NONBLOCK);
#endif
    sLobbyRole = host ? 1 : 2;
    sLobbyPort = port;
    snprintf(sLobbyAddr, sizeof(sLobbyAddr), "%s", address != NULL ? address : "");
    memset(&sPeer, 0, sizeof(sPeer));
    sPeer.sin_family = AF_INET;
    sLobby = 1;
    sLobbyLast = 0;
    if (host) {
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons((uint16_t)port);
        if (bind(sSock, (struct sockaddr *)&local, sizeof(local)) != 0) {
            sLobby = -1;
        }
    } else {
        sPeer.sin_port = htons((uint16_t)port);
        if (inet_pton(AF_INET, sLobbyAddr, &sPeer.sin_addr) != 1) {
            struct hostent *he = gethostbyname(sLobbyAddr);
            if (he == NULL) {
                sLobby = -1;
            } else {
                memcpy(&sPeer.sin_addr, he->h_addr_list[0], sizeof(sPeer.sin_addr));
            }
        }
    }
    return sLobby;
}

void Port_LobbyCancel(void) {
    if (sSock >= 0) {
        closesocket(sSock);
        sSock = -1;
    }
    sLobby = 0;
}

/* 0 idle, 1 still waiting, 2 the other player is there, -1 it cannot work (port in use, unknown address). */
int Port_LobbyPoll(void) {
    uint8_t pkt[64];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    uint64_t now = SDL_GetTicksNS();
    int n;

    if (sLobby != 1) {
        return sLobby;
    }
    if (sLobbyRole == 2 && now - sLobbyLast > 200000000ull) {
        uint32_t hello[2] = {MAGIC, T_HELLO};
        sendto(sSock, (const char *)hello, sizeof(hello), 0, (struct sockaddr *)&sPeer, sizeof(sPeer));
        sLobbyLast = now;
    }
    while ((n = (int)recvfrom(sSock, (char *)pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &fromLen)) >= 8) {
        uint32_t magic, type;
        memcpy(&magic, pkt, 4);
        memcpy(&type, pkt + 4, 4);
        if (magic != MAGIC) {
            continue;
        }
        if (sLobbyRole == 1 && type == T_HELLO) {
            uint32_t ack[2] = {MAGIC, T_HELLO_ACK};
            int k;
            for (k = 0; k < 3; k++) {
                sendto(sSock, (const char *)ack, sizeof(ack), 0, (struct sockaddr *)&from, sizeof(from));
            }
            sLobby = 2;
        } else if (sLobbyRole == 2 && type == T_HELLO_ACK) {
            sLobby = 2;
        }
    }
    return sLobby;
}

/* The other player is there: start the session. */
void Port_LobbyLaunch(void) {
    closesocket(sSock);
    sSock = -1;
    sLobby = 0;
    if (Port_SessionCan()) {
        Port_SessionRequest(sLobbyRole, sLobbyAddr, sLobbyPort); /* the game's thread makes the change at its next blank */
        return;
    }
    relaunch(sLobbyRole, sLobbyAddr, sLobbyPort);
}
