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

static void send_to_peer(const void *buf, int n) {
    if (sLoss != 0 && (rand() % 100) < sLoss) {
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

static void net_init(void) {
    const char *host = getenv("BT3_NET_HOST"), *join = getenv("BT3_NET_JOIN");
    struct sockaddr_in local;
    uint64_t t0, last = 0;
    uint32_t i;

    sMode = host != NULL ? 1 : join != NULL ? 2 : 0;
    if (sMode == 0) {
        return;
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
    sMe = sMode - 1;
    sDelay = getenv("BT3_NET_DELAY") != NULL ? atoi(getenv("BT3_NET_DELAY")) : 2;
    if (sDelay < 0) { sDelay = 0; }
    if (sDelay > 30) { sDelay = 30; }
    sLoss = getenv("BT3_NET_LOSS") != NULL ? atoi(getenv("BT3_NET_LOSS")) : 0;
    sLagMs = getenv("BT3_NET_LAG") != NULL ? atoi(getenv("BT3_NET_LAG")) : 0;
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
    if (sMode < 0) {
        net_init();
    }
    return sMode > 0;
}

/* A new vertical blank begins (Port_VBlank): this player's input made now takes effect `delay` blanks on; then
   wait until the other player's input for THIS blank is here. */
void Port_NetBeginTick(unsigned tick) {
    uint8_t mine[PAD];
    uint64_t t0, last;
    uint32_t at = (uint32_t)tick + (uint32_t)sDelay;

    if (!Port_NetActive()) {
        return;
    }
    sTick = tick;
    memcpy(mine, kIdle, PAD);
    if (sScript != NULL) {
        uint8_t both[2 * PAD];
        if (fseek(sScript, (long)tick * 2 * PAD, SEEK_SET) == 0 && fread(both, 1, sizeof(both), sScript) == sizeof(both)) {
            memcpy(mine, both + sMe * PAD, PAD);
        }
    } else if (!Port_PadRead(0, mine)) {
        memcpy(mine, kIdle, PAD);
    }
    if (at >= sHave[sMe]) {
        memcpy(sIn[sMe][at % RING], mine, PAD);
        sHave[sMe] = at + 1;
    }
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
            fprintf(stderr, "bt3: net: the other player has not answered for 15 seconds (blank %u)\n", tick);
            exit(2);
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
    memcpy(data, player >= 0 && player < 2 && sTick < sHave[player] ? sIn[player][sTick % RING] : kIdle, PAD);
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
