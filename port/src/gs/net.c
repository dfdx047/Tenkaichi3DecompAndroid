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
#include "tag_net.h"

extern int Port_PadRead(int socket, unsigned char *data); /* gs_input.c: the keyboard and the controllers */
void Port_NetLeave(void);

enum { PAD = 18, RING = 4096, REDUNDANT = 16, MAGIC = 0x4E335442 /* "BT3N" */ };
enum { T_HELLO = 1, T_HELLO_ACK = 2, T_INPUT = 3, T_BYE = 4, T_PING = 5, T_PONG = 6, T_CONFIG = 7, T_CONFIG_ACK = 8,
       T_PUNCH = 9 /* nothing to read: sent so that the sender's router lets the other side's packets in (room codes) */,
       T_CHAT = 10 /* a line of text between the players (net_chat.c): its number, its length, its bytes. Not part of
                      the game: a copy without the chat ignores it, so NET_VERSION stays */,
       T_FPRINT = 12 /* the sender's gameplay fingerprint (fprint_send, below); a copy without it ignores it */ };
/* The version of what goes over the line. The greeting and its answer carry it, and two copies that differ do not
   start a match (the input packets changed with version 2: times for the ping were added; a copy that read them
   the old way took them for input). 1 was releases 0.1.8 and 0.1.9, which said no version. */
enum { NET_VERSION = 6, IN_HEAD = 44 }; /* 3 (0.1.15): the host's battle rules travel with its choices.
   4 (0.1.34): a session's start-up no longer follows the player's "Skip logos" / "Skip intro video" (0.1.32 and
   0.1.33 with either on computed another start-up than the other side and the two never agreed again: such a copy
   must not be let into a match); the input packets carry the sender's check value (sync_*, below), 12 bytes more
   in their head; the goodbye can say why.
   5: a session's start-up is run with nothing pressed and without waiting for the other side, which then is not
   sent the start-up's input; the two meet at the character select (T_ARRIVED). A copy of 4 would wait for input
   that does not come.
   6 (0.1.39): lobbies replace the rooms of two (a friend is played in a lobby; Tenkaichi Budokai Online has no
   Create Room and no Watch), and a peer without a gameplay fingerprint is refused (FP_PEER_REQUIRED). Nothing a
   0.1.38 game sends is taken: the matchmaking service's queues and codes are kept apart by this number, and by
   address each side says "another version" (a lobby's owner refuses the greeting; a 0.1.38 host of two answers
   the lobby member's spectator question with SPEC_NO_VERSION, net_lobby.c). */
static int sVersionBad; /* the other side answered with another version */
static const uint32_t kVersion = NET_VERSION;
#include "net_match.h"
#include "net_relay.h"
#include "net_chat.h"
#include "net_lobby.h"
#include "net_update.h"
static int sLobbyOther;

/* What the meter shows (ui.cpp): the round trip time, and per second how often the game went back, how far, and
   how often and for how long it waited for the other player. */
static uint32_t sEchoTs, sEchoAtMs;     /* the other side's last send time and when it arrived here */
static int sPingMs = -1;
static unsigned sSecRoll, sSecRollTicks, sSecWaits, sSecWaitMs, sSecTicks;
static int sShown[6] = {0, 0, 0, 0, 0, 0}; /* ping, rollbacks, depth x 10, waits, ms waited, why (wait_why): of the last second */

static uint32_t now_ms(void) {
    return (uint32_t)(SDL_GetTicksNS() / 1000000ull) | 1u; /* (never 0: 0 means "no time yet") */
}

/* What the host chose in the Dragon Net Battle window (-1: not said; the environment or the defaults decide).
   The host's answer to the greeting carries its input delay and rollback limit, and the joining side takes them:
   the delay has to be the same on both sides. (The rollback limit is each side's own business, the two ways of
   playing work against each other; the host's is only the other side's starting value.) */
static int sOptRoll = -1, sOptDelay = -1;
static int sCfgGot, sCfgDelay, sCfgRoll;
static int sGone; /* the other player said goodbye (2: because its check found the two games apart); a lobby's match
                     ends (net_lobby.c, NetIo_End): 3 back to the lobby, 4 the lobby was closed, 5 this player left it */
static char sGoneNote[160];
extern void Port_UiNotice(const char *text); /* ui.cpp: a line over the picture for a few seconds */
extern void Port_UiBanner(const char *text, int ms); /* ui.cpp: a line in the middle while the game waits */
extern void Gs_HoldFrame(void);                      /* gs_draw.c: the last picture again, with that line on it */
extern const char *MenuTranslation_PortText(const char *id);
static void session_hold(void);

static int sAutoDelay;
static int sOptAuto;      /* the host asked for the input delay to be chosen from the line's ping */
static int sSetupRtt = -1; /* the smallest round trip measured while setting up, ms */
static int sCfgAcked;

void Port_TestSlow(int what, int amount);

void Port_NetOptions(int rollback, int delay) {
    sOptRoll = rollback;
    sOptAuto = delay == -2; /* -2: automatic; -1: not said; else the delay in blanks */
    sOptDelay = delay >= 0 ? delay : -1;
}

#define NAME_LEN 20
static char sNames[2][NAME_LEN], sMyName[NAME_LEN] = "Player";

void Port_NetNameSet(const char *name) {
    /* (through a copy: the menus pass the buffer of Port_NetNameGet(), this one, and snprintf into its own source is
       undefined: glibc left it empty, the name gone from a quick match on Linux until the next start) */
    char copy[NAME_LEN];
    snprintf(copy, sizeof(copy), "%s", name != NULL && name[0] != '\0' ? name : "Player");
    memcpy(sMyName, copy, sizeof(sMyName));
}

/* A match found with a room code (the lobby, below): the local port the lobby's socket had, which the session's
   socket takes again, and for the host the address the other player's greeting came from. */
static int sCodeLocalPort, sCodePunchSet;
/* The lobby's socket, handed to the session that follows (a match found with a room code, begun without a restart).
   It is not closed and opened again on the same port: while it is closed, packets from the other side meet a
   closed port, the system answers "unreachable", and a relay stops passing that player's packets on (seen with
   Cloudflare's: the match's greeting never arrived). It also keeps the routers' view of the conversation whole. */
static int sHandSock = -1, sFromLobby;
int Port_LobbyRelayed(void);
static struct sockaddr_in sCodePunch;
/* A lobby's match (net_lobby.c, NetIo_Go): a second address to knock at (the host) or to greet (the one who joins):
   the other player's own public address beside the one the lobby's owner sees it under. Port 0: none. */
static struct sockaddr_in sCodePunch2, sJoinAlt;
static int sGoPending; /* a lobby's session has been asked for and has not begun yet */

/* The match's rules, chosen by the host in the lobby window (the game's own versus menu is not shown in a session):
   the battle type as the versus menu leaves it (0 single, 1 team, 2 DP battle), the row of the DP limit list
   (0..2: 10, 15, 20) and the time limit as the battle settings keep it (save rule 0: 0..4 = 60, 90, 180, 240
   seconds, none). One word: type | dp << 4 | time << 8. The host's travels with its other choices (T_HELLO_ACK,
   T_CONFIG); headless.c applies it on both sides when the session enters the versus mode. */
static uint32_t sRules = 3u << 8; /* single battle, 240 seconds: the game's defaults */

/* The match's seed. A session is a fresh start of the program on both sides with the same fixed seed (plat_sys.c),
   so the game's random numbers were the same in every session: "Random" at the character select gave the same
   fighters in the same order every time (reported: Yajirobe, then Cooler). The host draws a number when it starts,
   it travels behind the host's other choices (T_HELLO_ACK, T_CONFIG), and both copies seed the game's generator
   with it at the same point, where the session enters the versus mode (headless.c). 0 = leave the generator as it
   is. BT3_NET_SEED=<n> on the host: that number (tests; 0 for the sequence every session had before). */
static uint32_t sSeed;

unsigned Port_NetSeed(void) {
    return sSeed;
}

static void seed_draw(void) {
    const char *e = getenv("BT3_NET_SEED");
    if (e != NULL) {
        sSeed = (uint32_t)strtoul(e, NULL, 0);
    } else {
        do {
            sSeed = SDL_rand_bits() ^ (uint32_t)SDL_GetTicksNS() ^ (uint32_t)(SDL_GetPerformanceCounter() >> 7);
        } while (sSeed == 0);
    }
}

/* Each player's own two battle options of the game's option menu (save 0x1694: the camera type; 0x1698: the
   screen shake; they become BattleOption.optA / optB of the player's side, battle_load.c). A session starts from
   an empty save on both sides, so both sides had the defaults. Each copy now reads its player's own from his save
   and tells the other; both then set side 0 to the host's and side 1 to the joiner's, the same on both copies
   (the cameras are part of the game's state). Sent behind the name in the greeting and in its answers, with a
   mark: a copy of 0.1.25 or earlier does not send them and does not read them, and then neither side uses them.
   OPT_MARK | camera | shake << 8. */
int Port_NetActive(void);
#define OPT_MARK 0x4F500000u
static uint32_t sOpts[2]; /* by player: 0 = not known */

static uint32_t own_options(void) {
    const char *root = getenv("BT3_SAVES") != NULL ? getenv("BT3_SAVES") : "saves";
    const char *carried = getenv("BT3_NET_OPTS"); /* (across the restart into the session: the save folder is the session's by then) */
    char path[600];
    unsigned char w[8];
    uint32_t a = 0, b = 0;
    FILE *fp;

    if (carried != NULL) {
        return OPT_MARK | ((uint32_t)strtoul(carried, NULL, 0) & 0xFFFFu);
    }
    snprintf(path, sizeof(path), "%s/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3", root);
    fp = fopen(path, "rb");
    if (fp != NULL) {
        if (fseek(fp, 0x1694, SEEK_SET) == 0 && fread(w, 1, 8, fp) == 8) {
            a = w[0] | (uint32_t)w[1] << 8 | (uint32_t)w[2] << 16 | (uint32_t)w[3] << 24;
            b = w[4] | (uint32_t)w[5] << 8 | (uint32_t)w[6] << 16 | (uint32_t)w[7] << 24;
        }
        fclose(fp);
    }
    return OPT_MARK | (a > 15 ? 0 : a) | (b > 15 ? 0 : b) << 8; /* (anything else: the defaults) */
}

static void opts_take(int player, const uint8_t *at) {
    uint32_t v;
    memcpy(&v, at, 4);
    if ((v & 0xFFFF0000u) == OPT_MARK) {
        sOpts[player] = v;
    }
}

/* For the game (BattleSetup_DefaultOptions): the two sides' options of an online match. 0 = not both known. */
int Port_NetSideOptions(int *camera, int *shake) {
    int i;
    if (!Port_NetActive() || sOpts[0] == 0 || sOpts[1] == 0) {
        return 0;
    }
    for (i = 0; i < 2; i++) {
        /* (the other side's word is whatever its program sent: only the values the options menu can set go into
           the game, the camera distance 0..2 and the shake on or off; anything else is the default) */
        camera[i] = (int)(sOpts[i] & 0xFF) > 2 ? 0 : (int)(sOpts[i] & 0xFF);
        shake[i] = (int)(sOpts[i] >> 8 & 0xFF) > 1 ? 0 : (int)(sOpts[i] >> 8 & 0xFF);
    }
    {
        static int said;
        if (!said) {
            said = 1;
            fprintf(stderr, "bt3: net: a battle is set up with each side's own camera type and screen shake\n");
        }
    }
    return 1;
}

/* The match's speed, a percentage of the game's own (100 to 200). It is the vertical blanks' pace (plat_stub.c)
   and nothing of the game's state: every step is computed as always, there are just more of them a second; the
   sound keeps its pitch (snd_adx.c). The host's choice, like the other rules, in the rules word:
     bits 16..23  the percentage (0 = not said)
     bit 12       "120%", which is all 0.1.29 knows of this; set beside the number when the number is 120
   What the one who joins knows is in the capability word of its greeting (CAP_MARK | 1: bit 12; | 2: the number;
   0.1.28 and earlier send none). The host takes back what the other does not know before it answers, so the two
   never run at different paces: a 0.1.29 gets 120% or 100%, anything older 100%.
   NET_SPEED_STANDARD is what a quick match runs at and what the Host tab starts from. Competitive play of this
   game is "120% on the PAL version in Dolphin"; on this (the USA) version's scale the players settled on 114%
   after trying the sliders of 0.1.30 (told by the maintainer, 2026-10-12). */
#define RULE_FAST 0x1000u
#define RULE_SPEED_SHIFT 16
#define CAP_MARK 0x43410000u
#define NET_SPEED_STANDARD 114
static int sWantSpeed = NET_SPEED_STANDARD;

static uint32_t speed_bits(int percent) {
    return (uint32_t)percent << RULE_SPEED_SHIFT | (percent == 120 ? RULE_FAST : 0);
}

void Port_NetRulesSet(int type, int dp, int time) {
    sRules = (uint32_t)(type < 0 || type > 2 ? 0 : type) | (uint32_t)(dp < 0 || dp > 2 ? 0 : dp) << 4 | (uint32_t)(time < 0 || time > 4 ? 3 : time) << 8 |
             speed_bits(sWantSpeed);
}

void Port_NetSpeedSet(int percent) {
    sWantSpeed = percent < 100 ? 100 : percent > 200 ? 200 : percent;
    sRules = (sRules & ~(RULE_FAST | 0xFFu << RULE_SPEED_SHIFT)) | speed_bits(sWantSpeed);
}

int Port_NetSpeedStandard(void) {
    return NET_SPEED_STANDARD;
}

static int rules_speed(void) {
    int percent = (int)(sRules >> RULE_SPEED_SHIFT & 0xFF);
    return percent >= 100 && percent <= 200 ? percent : (sRules & RULE_FAST) != 0 ? 120 : 100;
}

int Port_NetSession(void);
/* 1000 = the game's own pace; only ever the match's, never the player's own setting. */
int Port_NetSpeedMilli(void) {
    if(TagNet_Enabled())return 1000;
    return Port_NetSession() || Port_NetActive() ? rules_speed() * 10 : 1000;
}

void Port_NetRules(int *type, int *dp, int *time) {
    *type = (int)(sRules & 15) > 2 ? 0 : (int)(sRules & 15);
    *dp = (int)(sRules >> 4 & 15) > 2 ? 0 : (int)(sRules >> 4 & 15);
    *time = (int)(sRules >> 8 & 15) > 4 ? 3 : (int)(sRules >> 8 & 15);
}

/* The input delay for a line with this round trip. A blank is 16.7 ms; the other side's input for a blank is
   entered `delay` blanks ahead and takes half the round trip plus up to a blank (it waits for the next look at
   the socket) to arrive, so the game runs about ceil(trip / 2 / 16.7 + 1) - delay blanks on guesses. The delay
   is what keeps that at 4 or under: 1 on a good line (up to about 130 ms), more beyond, 6 at most. */
static int auto_delay(int rttMs) {
    int ticks, delay;
    if (rttMs < 0) {
        return 2;
    }
    {
        int blankUs = 16683 * 1000 / Port_NetSpeedMilli(); /* (a blank is shorter at 120%) */
        ticks = (rttMs * 1000 / 2 + blankUs - 1) / blankUs + 1; /* ceil(one way in blanks) + 1 */
    }
    delay = ticks - 4;
    return delay < 1 ? 1 : delay > 6 ? 6 : delay;
}

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
static uint32_t sResimFrom;      /* and the first blank it runs again */
static unsigned sRollbacks, sRollTicks, sStalls;
/* Keeping the two copies together in time: each says in its packets how many blanks it is beyond the input it
   has from the other (sTheirAhead). On an even line both numbers are the same; the copy whose clock is ahead has
   the larger one, by twice the difference of the clocks. That copy lets its blanks come a little later until the
   two agree (plat_stub.c's pacing grid is moved), so that neither does all the guessing and going back. */
static int32_t sTheirAhead, sMyAhead;
static int sBalance;  /* (mine - theirs), smoothed, in 1/16 blank */
static unsigned sSlowed;
extern unsigned gPortPaceShiftNs;

/* BT3_NET_LATENCY=<ms>: testing, a line that takes that long one way. What is sent waits in a queue. */
static int sLatencyMs;
static struct { uint64_t due; int n; uint8_t data[IN_HEAD + 16 * PAD]; } sQueue[256];
static unsigned sQueueHead, sQueueTail;

/* Every packet of a match goes out and comes in through these two: the host of a match found with a room code may
   hold a relay address (net_relay.c), and then the other player's packets can arrive wrapped by the relay and must
   go back the same way. Without a relay they are plain sendto / recvfrom. */
static int sStatOut, sStatIn, sStatInVia, sStatOutVia, sStatErr, sStatOther, sStatBytes;
static void net_stat(void) { /* BT3_RELAY_LOG: packets out and in, a line a second */
    static uint64_t last;
    uint64_t now = SDL_GetTicks();
    if (now - last >= 1000 && getenv("BT3_RELAY_LOG") != NULL) {
        if (sStatOut + sStatIn > 0) {
            fprintf(stderr, "bt3: net: packets out %d (%d through the relay, %d failed, %d bytes), in %d (%d through the relay, %d of the relay's own)\n", sStatOut,
                    sStatOutVia, sStatErr, sStatBytes, sStatIn, sStatInVia, sStatOther);
        }
        sStatOut = sStatIn = sStatInVia = sStatOutVia = sStatErr = sStatOther = sStatBytes = 0;
        last = now;
    }
}

/* BT3_TEST_FIREWALL=1 (testing): this copy's socket lets in only what comes from an address it has sent to, as a
   stateful firewall does (Windows' with no rule for the game): what a game must cope with on the other side. */
static struct sockaddr_in sFwSent[64];
static int sFwCount = -1, sFwNext;

static int fw_on(void) {
    if (sFwCount == -1) {
        sFwCount = getenv("BT3_TEST_FIREWALL") != NULL && atoi(getenv("BT3_TEST_FIREWALL")) != 0 ? 0 : -2;
    }
    return sFwCount >= 0;
}

static void fw_sent(const struct sockaddr_in *to) {
    int i;
    if (!fw_on()) {
        return;
    }
    for (i = 0; i < sFwCount; i++) {
        if (sFwSent[i].sin_addr.s_addr == to->sin_addr.s_addr && sFwSent[i].sin_port == to->sin_port) {
            return;
        }
    }
    if (sFwCount < 64) {
        sFwSent[sFwCount++] = *to;
    } else {
        sFwSent[sFwNext++ % 64] = *to;
    }
}

static int fw_drops(const struct sockaddr_in *from) {
    int i;
    if (!fw_on()) {
        return 0;
    }
    for (i = 0; i < sFwCount; i++) {
        if (sFwSent[i].sin_addr.s_addr == from->sin_addr.s_addr && sFwSent[i].sin_port == from->sin_port) {
            return 0;
        }
    }
    return 1;
}

static void net_send(const void *buf, int n, const struct sockaddr_in *to) {
    fw_sent(to);
    sStatOut++;
    sStatBytes += n;
    if (Relay_IsPeer(to)) {
        sStatOutVia++;
        Relay_Send(sSock, buf, n, to);
    } else if (sendto(sSock, (const char *)buf, n, 0, (const struct sockaddr *)to, sizeof(*to)) != n) {
        sStatErr++;
    }
}

/* The gameplay fingerprint (plat_fprint.c: one number for the data a fight is computed from) goes to the other side
   with the greetings, in a packet of its own: {MAGIC, T_FPRINT, FP_MARK, low half, high half}. Two games whose data
   differ (a modified disc image on one side, or another one on each) compute two different fights: the match ended
   as a desync at the fight's first moments.
   The rule (docs/port/README.md, "Online: whose data may meet"): a quick match (strangers) needs the original disc's
   data (Port_LobbyStartQuick); with friends (a room code, a lobby, an address, watching) the two need the same data,
   modified or not. A game never begins to connect before its own fingerprint is known (fp_known: the lobby's start
   waits for it, state 6, and so does net_start), so a copy of this release always sends one.
   A copy that sends none: 0.1.37 (NET_VERSION 5 as well, nothing else in its greetings told it from 0.1.37.1 to
   0.1.38.9, which send one), or a 0.1.37.1 to 0.1.38.9 that connected before its own was computed. Those were let
   in while FP_PEER_REQUIRED was 0; from 0.1.39 (NET_VERSION 6: every copy of it sends one) a peer, a spectator or
   a lobby's member without one is refused. BT3_TEST_FPRINT=<hex>: testing, this copy claims that value
   (plat_fprint.c); BT3_TEST_FPRINT=off: it sends none, as 0.1.37 did; BT3_TEST_FP_REQUIRED=0: FP_PEER_REQUIRED off
   for this copy. */
enum { FP_MARK = 0x46505231, FP_PEER_REQUIRED = 1 };
extern int Port_DataFingerprint(unsigned long long *out); /* plat_fprint.c */
extern int Port_DataFingerprintNeed(void);
extern int Port_DataFingerprintOriginal(void);
extern const char *Port_DataFingerprintText(void);
static const char kDataDiffer[] = "Your game data and the other player's are not the same (a modified disc image or mods on one side): "
                                  "an online match needs the same on both.";
static const char kDataUnsaid[] = "The other player's game did not say which game data it has (release 0.1.37, or it was still checking "
                                  "its data): both need 0.1.38 or newer.";
static int sDataBad;  /* the other side's fingerprint differs from this side's */
static int sFpHeard;  /* the other side's fingerprint has come (this session) */

static int fprint_mine(unsigned long long *fp) {
    const char *t = getenv("BT3_TEST_FPRINT");
    if (t != NULL && strcmp(t, "off") == 0) {
        return 0; /* (testing: as 0.1.37, which neither sends nor compares) */
    }
    return Port_DataFingerprint(fp) == 1;
}

/* 1: this game's fingerprint is known. Else it is being computed (and is started if this run had not). */
static int fp_known(void) {
    return Port_DataFingerprintNeed() == 1;
}

/* A peer that sends no fingerprint is refused (see FP_PEER_REQUIRED), when this side has one to compare. */
static int fp_required(void) {
    unsigned long long fp;
    const char *t = getenv("BT3_TEST_FP_REQUIRED");
    return (t != NULL ? atoi(t) != 0 : FP_PEER_REQUIRED) && fprint_mine(&fp);
}

/* Why this game cannot search for a quick match ("" it can: its data are the original disc's), in `out`. */
static const char *fp_quick_refusal(char *out, size_t size) {
    const char *t = Port_DataFingerprintText(), *m = strstr(t, "modified:");
    if (Port_DataFingerprint(NULL) == 1 && Port_DataFingerprintOriginal() == 1) {
        return "";
    }
    snprintf(out, size, "Quick match needs the original disc's data, and your game data are not the disc's%s%s%s. With friends "
             "(a room code, a lobby or an address) you can play when both have the same data.",
             m != NULL ? " (" : "", m != NULL ? m + 10 : "", m != NULL ? ")" : "");
    return out;
}

static void fprint_send(const struct sockaddr_in *to) {
    unsigned long long fp;
    uint32_t pkt[5] = {MAGIC, T_FPRINT, FP_MARK, 0, 0};
    if (fprint_mine(&fp)) {
        pkt[3] = (uint32_t)fp;
        pkt[4] = (uint32_t)(fp >> 32);
        net_send(pkt, (int)sizeof(pkt), to);
    }
}

/* 1: this T_FPRINT says that the other side's data are not this side's. */
static int fprint_differs(const uint8_t *pkt, int n) {
    static unsigned long long said;
    unsigned long long mine, theirs;
    uint32_t v[3];
    if (n < 20) {
        return 0;
    }
    memcpy(v, pkt + 8, 12);
    if (v[0] != FP_MARK) {
        return 0;
    }
    sFpHeard = 1;
    if (!fprint_mine(&mine)) {
        return 0;
    }
    theirs = (unsigned long long)v[1] | (unsigned long long)v[2] << 32;
    if (theirs == mine) {
        return 0;
    }
    if (theirs != said) { /* (once: the packet comes again and again) */
        said = theirs;
        fprintf(stderr, "bt3: net: the other player's game data are not this game's (gameplay fingerprint %016llx, this one %016llx)\n", theirs, mine);
    }
    return 1;
}

static int net_recv(uint8_t *pkt, int size, struct sockaddr_in *from) {
    Relay_Tick(sSock);
    net_stat();
    for (;;) {
        socklen_t fromLen = sizeof(*from);
        uint8_t *data = NULL;
        int len = 0, n = (int)recvfrom(sSock, (char *)pkt, size, 0, (struct sockaddr *)from, &fromLen), kind;
        if (n <= 0) {
            return n;
        }
        kind = Relay_Unwrap(pkt, n, from, &data, &len);
        if (kind == 2) {
            if (fw_drops(from)) {
                continue; /* (BT3_TEST_FIREWALL: unasked for) */
            }
            Relay_SawDirect(from);
            sStatIn++;
            return n;
        }
        if (kind == 0) {
            sStatOther++;
        }
        if (kind == 1) {
            sStatIn++;
            sStatInVia++;
            memmove(pkt, data, (size_t)len);
            return len;
        }
    }
}

static void queue_flush(void) {
    uint64_t now = SDL_GetTicksNS();
    while (sQueueHead != sQueueTail && sQueue[sQueueHead % 256].due <= now) {
        net_send(sQueue[sQueueHead % 256].data, sQueue[sQueueHead % 256].n, &sPeer);
        sQueueHead++;
    }
}

static void send_to_peer(const void *buf, int n) {
    if (sLoss != 0 && (rand() % 100) < sLoss) {
        return;
    }
    {   /* testing: BT3_NET_MUTE_AT=<blank>: from that blank this game's packets stop reaching the other one (the way
           from here to there failing, as the other side sees it: its game stops hearing this one) */
        static int muteAt = -2;
        extern unsigned Port_NetTick(void);
        if (muteAt == -2) {
            muteAt = getenv("BT3_NET_MUTE_AT") != NULL ? atoi(getenv("BT3_NET_MUTE_AT")) : -1;
        }
        if (muteAt >= 0 && sConnected && Port_NetTick() >= (unsigned)muteAt) {
            return;
        }
    }
    if (sLatencyMs > 0 && n <= (int)sizeof(sQueue[0].data) && sQueueTail - sQueueHead < 256) {
        sQueue[sQueueTail % 256].due = SDL_GetTicksNS() + (uint64_t)sLatencyMs * 1000000ull;
        sQueue[sQueueTail % 256].n = n;
        memcpy(sQueue[sQueueTail % 256].data, buf, (size_t)n);
        sQueueTail++;
        return;
    }
    net_send(buf, n, &sPeer);
}

/* Do the two copies still compute the same game? Only button presses are exchanged, so nothing else would say:
   two copies that have come apart go on, each with its own fight, for good. Every SYNC_EVERY blanks each copy
   notes a number made of what must be the same on both (Port_NetDigest, headless.c: where the game is, and in a
   fight both fighters' health and position and the clock). A note counts once the blank is sure: every blank
   before it was run with the other player's real input (with rollback a blank is run on a guess first and noted
   again each time it is run again). The newest sure note travels in every input packet; a copy that has a sure
   note of its own for the same blank compares, and when they differ the match ends on both sides with a message
   saying so. A dozen bytes a packet and a few hundred bytes hashed twice a second. */
enum { SYNC_EVERY = 32, SYNC_SLOTS = 64, BYE_APART = 0x41504152 };
/* Why a game says goodbye (the T_BYE word; a release before 0.1.38.9 sends 0 or BYE_APART, and shows every other
   word as "left the match"): its player left the match (the menus), its program was closed, or it stopped hearing
   the other game (QUIET_NS without the input it waited for). The other side's screen and log say which. */
enum { BYE_LEFT = 0x5446454C, BYE_QUIT = 0x54495551, BYE_LOST = 0x54534F4C };
static uint32_t sByeHeard; /* the word of the goodbye heard (0: none, or an older release's) */
static int sByeSent;       /* this session's goodbye has gone out */
static void bye_at_exit(void);
typedef struct { uint32_t tick; uint64_t value; } SyncNote;
static SyncNote sSyncMine[SYNC_SLOTS], sSyncTheirs[SYNC_SLOTS];
static uint32_t sSyncOutTick, sByeWhy;
static unsigned sSyncCompared; /* (for the log: notes of the two copies compared since the last line) */
static uint64_t sSyncOutValue;
extern unsigned long long Port_NetDigest(void); /* headless.c */
static void ended(const char *why, unsigned tick);
/* Spectators (below): every sure note of the match, by blank / SYNC_EVERY (the host's own; a spectator's copy of
   the host's, and its own) */
typedef struct { uint64_t *v; uint8_t *has; uint32_t cap; } NoteLog;
static NoteLog sNotesSure, sNotesOwn;
static void note_put(NoteLog *l, uint32_t tick, uint64_t v);

/* When the two copies' notes differ, the match goes on (as before 0.1.34, when nothing compared them): the player
   is told once that the two screens may now show different fights, and the log says where it began and, as the
   session ends, how many comparisons differed. The setting net_desync_end=1 (or BT3_NET_DESYNC_END=1) ends the
   match instead, as 0.1.34 to 0.1.37 did. A copy of those releases still ends it, and says so to this one. */
static uint32_t sApartAt;   /* the first blank whose notes differed (0: none in this session) */
static unsigned sApartCount;

static int desync_ends(void) {
    extern int Port_Setting(const char *name, int def);
    if (getenv("BT3_NET_DESYNC_END") != NULL) {
        return atoi(getenv("BT3_NET_DESYNC_END")) != 0;
    }
    return Port_Setting("net_desync_end", 0) != 0;
}

/* The game is at the start of blank `tick` (run for the first time or again). */
static void sync_note(uint32_t tick) {
    if (tick % SYNC_EVERY == 0) {
        SyncNote *m = &sSyncMine[tick / SYNC_EVERY % SYNC_SLOTS];
        m->tick = tick;
        m->value = Port_NetDigest();
    }
}

/* Every blank up to `sure` is as it will stay (and the game is not being run again): what goes out next, and the
   other copy's notes that can be compared now. */
static void sync_check(uint32_t sure, unsigned tick) {
    uint32_t at = sure - sure % SYNC_EVERY;
    int k;

    if (at != 0 && sSyncMine[at / SYNC_EVERY % SYNC_SLOTS].tick == at) {
        sSyncOutTick = at;
        sSyncOutValue = sSyncMine[at / SYNC_EVERY % SYNC_SLOTS].value;
        if (sMode == 1 || (sMode == 2 && Lobby_ServesSpectators())) {
            note_put(&sNotesSure, at, sSyncOutValue); /* (what spectators compare with, for the whole match) */
        }
    }
    if (getenv("BT3_TEST_APART") != NULL && tick == (unsigned)atoi(getenv("BT3_TEST_APART"))) {
        sSyncOutValue ^= 1; /* testing: from that blank on this copy claims another game than it has, once */
    }
    for (k = 0; k < SYNC_SLOTS; k++) {
        SyncNote *o = &sSyncTheirs[k];
        if (o->tick != 0 && o->tick <= sure) {
            const SyncNote *m = &sSyncMine[k];
            uint32_t was = o->tick;
            int differ = m->tick == was && m->value != o->value;
            o->tick = 0;
            sSyncCompared += m->tick == was;
            if (differ) {
                sApartCount++;
                if (sApartAt == 0) {
                    sApartAt = was;
                    fprintf(stderr, "bt3: net: the two games differ at blank %u (this copy %016llx, the other %016llx)\n", (unsigned)was,
                            (unsigned long long)m->value, (unsigned long long)o->value);
                    if (desync_ends()) {
                        sByeWhy = BYE_APART;
                        ended("The two games no longer agree (desync): the match has ended.", tick);
                    }
                    Port_UiNotice("The two games no longer agree (desync): your screen and the other player's may now show different fights.");
                }
            }
        }
    }
}

static void gone(unsigned tick);

/* A wait for the other player's input (roll_tick, Port_NetBeginTick) that lasted 100 ms or more goes to the log with
   what came from the other side meanwhile: input packets that did not yet carry the blank this copy needs (the
   other game is behind this one: its machine is slow or stood still for a moment), or nothing at all for most of
   the wait (the line held the packets back, or the other game stood still and sent nothing). The other copy's log
   says which of the two machines it was: its slow frames are in it (gs_core.c). The meter shows the same per second.
   gPortNetWaitNs: the time waited, for the slow-frame report (a frame that only waited is not a slow machine). */
enum { WAIT_NOTE_MS = 100, WAIT_NOTES = 200, WHY_BEHIND = 1, WHY_SILENT = 2 };
static unsigned sInPkts;        /* input packets taken */
static uint64_t sInLastNs;      /* when the last one came */
static uint64_t sInGapNs;       /* the longest time without one since the wait began */
static unsigned sWaitNotes;     /* lines written in this session */
static uint64_t sSecWaitMaxNs;  /* the longest wait of this second, and why it was */
static int sSecWaitWhy;
uint64_t gPortNetWaitNs;

typedef struct { uint64_t t0; unsigned pkts; } WaitMark;

static WaitMark wait_begin(uint64_t t0) {
    WaitMark w;
    w.t0 = t0;
    w.pkts = sInPkts;
    sInGapNs = sInLastNs != 0 && t0 > sInLastNs ? t0 - sInLastNs : 0;
    return w;
}

static void wait_end(WaitMark w, unsigned tick) {
    uint64_t now = SDL_GetTicksNS(), took = now - w.t0, gap = sInGapNs;
    unsigned got = sInPkts - w.pkts;
    int why;

    gPortNetWaitNs += took;
    if (now > sInLastNs && now - sInLastNs > gap) {
        gap = now - sInLastNs;
    }
    why = gap * 10 >= took * 8 ? WHY_SILENT : WHY_BEHIND;
    if (took > sSecWaitMaxNs) {
        sSecWaitMaxNs = took;
        sSecWaitWhy = why;
    }
    if (took < (uint64_t)WAIT_NOTE_MS * 1000000ull || sWaitNotes >= WAIT_NOTES) {
        return;
    }
    sWaitNotes++;
    if (why == WHY_SILENT) {
        fprintf(stderr, "bt3: net: waited %u ms at blank %u for the other player's input: nothing came from them for %u ms (the line held it back, "
                        "or their game stood still); ping %d ms\n", (unsigned)(took / 1000000ull), tick, (unsigned)(gap / 1000000ull), sPingMs);
    } else {
        fprintf(stderr, "bt3: net: waited %u ms at blank %u for the other player's input: %u packets came meanwhile without it (their game is "
                        "behind this one; they say they are %d blanks ahead); ping %d ms\n", (unsigned)(took / 1000000ull), tick, got, (int)sTheirAhead, sPingMs);
    }
}

/* The meeting at the end of a session's start-up (warp_tick, meet): the blank at which each copy reached the
   character select. A copy that is waiting sends {MAGIC, T_ARRIVED, blank, 0}; one that has arrived answers each such
   greeting with {..., 1}, so a lost packet costs a resend and the two never go on answering each other. */
enum { T_ARRIVED = 11 };
enum { T_LOBBY_INVITE = 40, LOBBY_CAP = 0x4C4F4231 }; /* (net_lobby.c) */
static uint32_t sMyArrival, sTheirArrival; /* (0: not yet) */
static int sMet;
static char sBannerSaid[NAME_LEN + 8]; /* what session_banner last put up (empty: nothing yet in this session) */

static void send_inputs(void) {
    uint8_t pkt[IN_HEAD + REDUNDANT * PAD];
    uint32_t tsend = now_ms(), techo = sEchoTs, thold = sEchoTs != 0 ? tsend - sEchoAtMs : 0;
    uint32_t magic = MAGIC, type = T_INPUT, n = sHave[sMe] < REDUNDANT ? sHave[sMe] : REDUNDANT, base = sHave[sMe] - n, i;
    int32_t ahead = (int32_t)sTick - (int32_t)sHave[!sMe]; /* how far this copy is beyond the input it has from the other */

    memcpy(pkt, &magic, 4);
    memcpy(pkt + 4, &type, 4);
    memcpy(pkt + 8, &base, 4);
    memcpy(pkt + 12, &n, 4);
    sMyAhead = ahead;
    memcpy(pkt + 16, &ahead, 4);
    memcpy(pkt + 20, &tsend, 4); /* for the ping: when this left, */
    memcpy(pkt + 24, &techo, 4); /* the other side's last such time that arrived here, */
    memcpy(pkt + 28, &thold, 4); /* and how long ago that was */
    memcpy(pkt + 32, &sSyncOutTick, 4); /* the check: the newest blank this copy is sure of, */
    memcpy(pkt + 36, &sSyncOutValue, 8); /* and what the game was at it (Port_NetDigest) */
    for (i = 0; i < n; i++) {
        memcpy(pkt + IN_HEAD + i * PAD, sIn[sMe][(base + i) % RING], PAD);
    }
    send_to_peer(pkt, (int)(IN_HEAD + n * PAD));
}

/* The lines of text that are due to go to the other player (net_chat.c: each is sent a few times). */
static void chat_send(void) {
    uint8_t pkt[16 + CHAT_TEXT];
    uint32_t head[4] = {MAGIC, T_CHAT};
    int len;

    while (sConnected && (len = PortChat_Outgoing(now_ms(), &head[2], (char *)pkt + 16)) > 0) {
        head[3] = (uint32_t)len;
        memcpy(pkt, head, 16);
        send_to_peer(pkt, 16 + len);
    }
}

/* ---------------------------------------------------------------------------------------------------------------
 * Spectators (the first stage of the lobbies; docs/port/README.md, "Spectators").
 *
 * A third game can watch a match. It connects to the host and is told the match's set-up (what the one who joins
 * is told: the rules and speed, the seed, both players' options and names), then sent the input of both players
 * for every blank from the session's first, as the host has it CONFIRMED (its own, and the other player's as it
 * arrived; never a guess). A session is computed from its first blank out of nothing but that input, so the
 * spectator's copy computes the same match: the character select, every battle, the rematches. It runs about a
 * second behind the host (SPEC_BUFFER); one that is further behind (it came in mid-match, or the line held it up)
 * runs blanks without picture until it has caught up. The host keeps the whole match's input (36 bytes a blank,
 * fewer in the packets), so a spectator can come at any time. The check values (sync_check) of the host travel
 * with the input, and a spectator compares them with its own: a spectator that computes another fight says so.
 *
 * Nothing here waits for anything or is waited for: the host sends a spectator at most SPEC_BURST packets a blank
 * whatever it is owed, and forgets one that has not answered for SPEC_QUIET_NS. The packets:
 *   T_SPEC_HELLO  spectator -> host   {SPEC_VERSION, NET_VERSION, fingerprint mark, low, high, name, flags}: flag 1,
 *                                      only asking (the spectator's lobby): the host answers and keeps nothing
 *   T_SPEC_WAIT   host -> spectator   no match has begun yet (the host's lobby, or its set-up): ask again
 *   T_SPEC_READY  host -> spectator   (to a lobby's question) the match is on, come in
 *   T_SPEC_NO     host -> spectator   {why: SPEC_NO_*}
 *   T_SPEC_SETUP  host -> spectator   {SPEC_VERSION, NET_VERSION, delay, rules, seed, options of both players,
 *                                      flags, blanks known, the two names}
 *   T_SPEC_DATA   host -> spectator   {first blank, blanks, blanks known, ...} + blanks + check values (spec_data)
 *   T_SPEC_ACK    spectator -> host   {blanks it has in a row, 1 if a packet came that it could not use}
 *   T_SPEC_BYE    either way          the match is over / the spectator leaves
 *   T_SPEC_LIST   host -> the other player   {count, names}: who watches (for the players' screens)
 * A copy without spectators (0.1.38.7 and older) ignores all of them: an old host never answers, and after
 * SPEC_ASK_NS the spectator says why that may be. Nothing the two players send each other has changed.
 *
 * The blanks of T_SPEC_DATA, a byte per step: 0x80 | n: the next n + 1 blanks are the same as the one before; 1, 2
 * or 3: the pads that changed follow (bit 0 player 1, bit 1 player 2, PAD bytes each). A packet's first blank is
 * always whole (3), so that every packet can be read by itself.
 * ------------------------------------------------------------------------------------------------------------- */
enum { T_SPEC_HELLO = 20, T_SPEC_WAIT = 21, T_SPEC_NO = 22, T_SPEC_SETUP = 23, T_SPEC_DATA = 24, T_SPEC_ACK = 25, T_SPEC_BYE = 26, T_SPEC_LIST = 27,
       T_SPEC_READY = 28 };
enum { SPEC_VERSION = 1, SPEC_MAX = 6, SPEC_HEAD = 32, SPEC_PAYLOAD = 1150, SPEC_BURST = 2, SPEC_BUFFER = 60, SPEC_MAX_BLANKS = 1000000 };
enum { SPEC_NO_VERSION = 1, SPEC_NO_DATA = 2, SPEC_NO_FULL = 3, SPEC_NO_OFF = 4, SPEC_NO_PLAIN = 5, SPEC_NO_PLAYER = 6, SPEC_NO_LOBBY = 7 };
#define SPEC_QUIET_NS 8000000000ull  /* silent this long: the spectator is gone / the host is gone */
#define SPEC_ASK_NS 10000000000ull   /* no answer at all this long: nobody hosts there, or a release without spectators */
#define SPEC_LAG_BLANKS (60u * 60u)   /* a spectator that has been there a minute and is still a minute behind is let go */

static uint8_t *sLog; /* both players' input by blank (PAD bytes each), every blank below sLogN (host and spectator) */
static uint32_t sLogN, sLogCap;

static int log_room(uint32_t n) {
    if (n > SPEC_MAX_BLANKS) {
        return 0;
    }
    if (n > sLogCap) {
        uint32_t cap = sLogCap != 0 ? sLogCap : 65536;
        uint8_t *p;
        while (cap < n) {
            cap *= 2;
        }
        p = (uint8_t *)realloc(sLog, (size_t)cap * 2 * PAD);
        if (p == NULL) {
            return 0;
        }
        sLog = p;
        sLogCap = cap;
    }
    return 1;
}

static void note_put(NoteLog *l, uint32_t tick, uint64_t v) {
    uint32_t i = tick / SYNC_EVERY;
    if (i >= SPEC_MAX_BLANKS / SYNC_EVERY) {
        return;
    }
    if (i >= l->cap) {
        uint32_t cap = l->cap != 0 ? l->cap : 1024;
        uint64_t *nv;
        uint8_t *nh;
        while (cap <= i) {
            cap *= 2;
        }
        nv = (uint64_t *)realloc(l->v, (size_t)cap * sizeof(uint64_t));
        if (nv == NULL) {
            return;
        }
        l->v = nv;
        nh = (uint8_t *)realloc(l->has, cap);
        if (nh == NULL) {
            return;
        }
        memset(nh + l->cap, 0, cap - l->cap);
        l->has = nh;
        l->cap = cap;
    }
    l->v[i] = v;
    l->has[i] = 1;
}

static int note_get(const NoteLog *l, uint32_t tick, uint64_t *v) {
    uint32_t i = tick / SYNC_EVERY;
    if (tick % SYNC_EVERY != 0 || i >= l->cap || !l->has[i]) {
        return 0;
    }
    *v = l->v[i];
    return 1;
}

/* The host's spectators. */
typedef struct {
    int on;
    struct sockaddr_in addr;
    char name[NAME_LEN];
    uint32_t acked, next;                              /* it has every blank below acked; the next blank to send */
    uint32_t noteNext;                                 /* the next check value to send (a blank) */
    uint64_t heardNs, movedNs, rewoundNs, sentNs, sinceNs;
} Watcher;
static Watcher sWatch[SPEC_MAX];
static int sSpecOpen;        /* host: the match's set-up is final, spectators are let in */
static int sSpecAllow = -1;  /* host: spectators allowed (Port_NetSpectatorsAllow; -1: the setting says) */
static char sWatchName[SPEC_MAX][NAME_LEN]; /* who watches: the host's own list, or the host's word (T_SPEC_LIST) */
static int sWatchCount, sListDirty;
static uint64_t sListSentNs;

/* The spectator's side. */
static uint32_t sSpecHead;                 /* the blanks the host has (its last word) */
static uint64_t sSpecHeardNs, sSpecAckNs, sSpecGapNs;
static int sSpecSkip;                      /* running blanks without picture to catch up */
/* How far behind the host the spectator runs (its buffer, in blanks): short while the host's blanks come evenly,
   longer after this copy ran out of them (a stall of the line or of the host), shorter again after a quiet while.
   (It began as a fixed second, SPEC_BUFFER: the view was a second late.) */
enum { SPEC_LEAD_MIN = 18, SPEC_LEAD_START = 24, SPEC_LEAD_STEP = 12, SPEC_LEAD_QUIET = 900 };
static int sSpecLead = SPEC_LEAD_START;
static unsigned sSpecLeadSince;            /* the blank it last ran out at, or grew shorter at */
static unsigned sSpecLeadLog;
static int sSpecNudge;                     /* this blank runs without picture and pacing: a little ahead (behind by a lot) */
static int sSpecSetup, sSpecWaits, sSpecNo, sSpecEnded;
static uint32_t sSpecApartAt;
static unsigned sSpecCompared, sSpecDiffered;
/* What the spectator shows of the fight. A session's battle is always the game's two-player battle, with both
   players' cameras computed on every copy; each player's copy shows its own one full screen (Port_NetView,
   btl_cam.c BtlCam_UpdateOverride), and which one it shows is only drawing (the two players' copies show different
   ones and compute the same fight). The spectator chooses: 0 player 1's view, 1 player 2's, 2 both, the screen
   split as in a two-player game on one machine (the same battle: nothing is loaded differently). */
static int sSpecView;
static int sSpecResyncs, sSpecRejoin;      /* following again from the start after a desync */
static char sSpecTarget[160];              /* "address:port" of the host (for following again) */
static uint64_t sSpecDrawNs;
static unsigned sSpecSkipped;
static int sSpecNoticed;                   /* the end of watching has been told (ended) */
static volatile int sSpecLeaveAsked;       /* the window's "Stop watching" (done on the game's thread) */

/* The window (ui.cpp): stop watching; switch whose view is shown. */
void Port_NetSpecLeave(void) {
    sSpecLeaveAsked = 1;
}
void Port_NetSpecViewSet(int view) {
    sSpecView = view < 0 || view > 2 ? 0 : view;
}
int Port_NetSpecView(void) {
    return sSpecView;
}

static int spec_allowed(void) {
    extern int Port_Setting(const char *name, int def);
    if (Lobby_InMatch()) {
        return 1; /* (a lobby's match: its spectators are the lobby's, whatever this player's setting) */
    }
    if (getenv("BT3_NET_SPECTATORS") != NULL) {
        return atoi(getenv("BT3_NET_SPECTATORS")) != 0;
    }
    return sSpecAllow >= 0 ? sSpecAllow : Port_Setting("net_spectators", 1) != 0;
}

void Port_NetSpectatorsAllow(int on) {
    sSpecAllow = on != 0;
}

/* 1 while this copy watches a match. */
int Port_NetSpectating(void) {
    return sMode == 3;
}

static int same_addr(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

/* For the players' screens (ui.cpp): how many watch this match, and their names. */
int Port_NetWatchers(void) {
    return Port_NetActive() && sMode != 3 ? sWatchCount : 0;
}
const char *Port_NetWatcher(int i) {
    return i >= 0 && i < sWatchCount ? sWatchName[i] : "";
}

/* For the spectator's line (ui.cpp): 0 not watching; else 1 and out = catching up (percent, -1 not), waiting for
   the host's blanks (1), the blank this copy came apart from the host's at (0: it has not). */
int Port_NetSpecStatus(int *out) {
    if (sMode != 3 || !sSpecSetup) {
        return 0;
    }
    out[0] = sSpecSkip && sSpecHead != 0 ? (int)((uint64_t)sTick * 100 / sSpecHead) : -1;
    out[1] = sTick >= sLogN;
    out[2] = (int)sSpecApartAt;
    return 1;
}

static void spec_list_make(void) {
    int i;
    sWatchCount = 0;
    for (i = 0; i < SPEC_MAX; i++) {
        if (sWatch[i].on) {
            memcpy(sWatchName[sWatchCount++], sWatch[i].name, NAME_LEN);
        }
    }
    sListDirty = 1;
}

/* The other player is told who watches (when that changes, and every two seconds while anyone does). */
static void spec_list_send(void) {
    uint32_t pkt[3 + SPEC_MAX * NAME_LEN / 4] = {MAGIC, T_SPEC_LIST, (uint32_t)sWatchCount};
    int i;
    for (i = 0; i < sWatchCount; i++) {
        memcpy((uint8_t *)pkt + 12 + i * NAME_LEN, sWatchName[i], NAME_LEN);
    }
    net_send(pkt, 12 + sWatchCount * NAME_LEN, &sPeer);
    sListSentNs = SDL_GetTicksNS();
    sListDirty = 0;
}

static void spec_drop(int i, const char *why) {
    fprintf(stderr, "bt3: net: spectator %d (%s) %s\n", i + 1, sWatch[i].name, why);
    sWatch[i].on = 0;
    spec_list_make();
}

static void spec_no(const struct sockaddr_in *to, uint32_t why) {
    uint32_t no[3] = {MAGIC, T_SPEC_NO, why};
    net_send(no, sizeof(no), to);
}

/* Is this greeting of a spectator one this game can take? 0 yes, else SPEC_NO_*. (Also the lobby's, below.) */
static uint32_t spec_check(const uint8_t *pkt, int n) {
    uint32_t v[5];
    unsigned long long mine;
    if (n < 28 + NAME_LEN) {
        return SPEC_NO_VERSION;
    }
    memcpy(v, pkt + 8, 20);
    if (v[0] != SPEC_VERSION || v[1] != NET_VERSION) {
        return SPEC_NO_VERSION;
    }
    if (v[2] == FP_MARK && fprint_mine(&mine) && ((unsigned long long)v[3] | (unsigned long long)v[4] << 32) != mine) {
        return SPEC_NO_DATA;
    }
    if (v[2] != FP_MARK && fp_required()) {
        return SPEC_NO_DATA; /* (says no data: a copy of 0.1.37, see FP_PEER_REQUIRED) */
    }
    return spec_allowed() ? 0 : SPEC_NO_OFF;
}

static void spec_setup_send(const struct sockaddr_in *to) {
    uint32_t s[11 + 2 * NAME_LEN / 4] = {MAGIC, T_SPEC_SETUP, SPEC_VERSION, NET_VERSION, (uint32_t)sDelay, sRules, sSeed, sOpts[0], sOpts[1], 1, sLogN};
    memcpy(&s[11], sNames[0], NAME_LEN);
    memcpy(&s[11 + NAME_LEN / 4], sNames[1], NAME_LEN);
    net_send(s, sizeof(s), to);
}

/* The host: a spectator's greeting. */
static void spec_hello(const uint8_t *pkt, int n, const struct sockaddr_in *from) {
    uint32_t why = sMode == 1 || Lobby_ServesSpectators() ? spec_check(pkt, n) : SPEC_NO_PLAYER;
    uint64_t now = SDL_GetTicksNS();
    int i, free = -1;

    if (why == 0 && !Port_NetSession()) {
        why = SPEC_NO_PLAIN;
    }
    if (why == 0 && Lobby_Active() && !Lobby_IsMember(from)) {
        why = SPEC_NO_LOBBY; /* (a lobby's match is watched by its members, through its owner) */
    }
    if (why != 0) {
        spec_no(from, why);
        return;
    }
    if (sSpecOpen && n >= 32 + NAME_LEN && (pkt[28 + NAME_LEN] & 1)) { /* a lobby's question: is there room? */
        for (i = 0; i < SPEC_MAX && sWatch[i].on && !same_addr(&sWatch[i].addr, from); i++) {
        }
        if (i == SPEC_MAX) {
            spec_no(from, SPEC_NO_FULL);
            return;
        }
    }
    if (!sSpecOpen || (n >= 32 + NAME_LEN && (pkt[28 + NAME_LEN] & 1))) {
        uint32_t wait[3] = {MAGIC, sSpecOpen ? T_SPEC_READY : T_SPEC_WAIT, SPEC_VERSION};
        net_send(wait, sizeof(wait), from); /* (a lobby's question: answered, nothing kept) */
        return;
    }
    for (i = 0; i < SPEC_MAX; i++) {
        if (sWatch[i].on && same_addr(&sWatch[i].addr, from)) {
            sWatch[i].heardNs = now;
            spec_setup_send(from); /* (its greeting came again: the answer was lost) */
            return;
        }
        if (!sWatch[i].on && free < 0) {
            free = i;
        }
    }
    if (free < 0) {
        spec_no(from, SPEC_NO_FULL);
        return;
    }
    memset(&sWatch[free], 0, sizeof(sWatch[free]));
    sWatch[free].on = 1;
    sWatch[free].addr = *from;
    memcpy(sWatch[free].name, pkt + 28, NAME_LEN);
    sWatch[free].name[NAME_LEN - 1] = '\0';
    for (i = 0; sWatch[free].name[i] != '\0'; i++) { /* (shown on the players' screens: printable only) */
        if ((unsigned char)sWatch[free].name[i] < 0x20 || sWatch[free].name[i] == 0x7F) {
            sWatch[free].name[i] = '?';
        }
    }
    sWatch[free].heardNs = sWatch[free].movedNs = sWatch[free].sinceNs = now;
    fprintf(stderr, "bt3: net: spectator %d (%s) is watching from blank %u of %u\n", free + 1, sWatch[free].name, 0u, (unsigned)sLogN);
    spec_list_make();
    spec_setup_send(from);
}

/* The host: what the confirmed input has become (every blank both players' input is known for). */
static void spec_log_update(void) {
    uint32_t sure = sHave[0] < sHave[1] ? sHave[0] : sHave[1];
    while (sLogN < sure && sure - sLogN < RING && log_room(sLogN + 1)) {
        memcpy(sLog + (size_t)sLogN * 2 * PAD, sIn[0][sLogN % RING], PAD);
        memcpy(sLog + (size_t)sLogN * 2 * PAD + PAD, sIn[1][sLogN % RING], PAD);
        sLogN++;
    }
}

/* Blanks from `from` on, at most `most` of them, into `out` (at most `room` bytes); how many in *count. */
static int spec_encode(uint32_t from, uint32_t most, uint8_t *out, int room, uint32_t *count) {
    uint32_t t = from, stop = sLogN - from > most ? from + most : sLogN;
    int n = 0;
    while (t < stop) {
        const uint8_t *cur = sLog + (size_t)t * 2 * PAD, *prev = cur - 2 * PAD;
        int mask, need;
        if (t == from) {
            mask = 3;
        } else if (memcmp(cur, prev, 2 * PAD) == 0) {
            uint32_t r = 1;
            while (t + r < stop && r < 128 && memcmp(cur + (size_t)r * 2 * PAD, cur + ((size_t)r - 1) * 2 * PAD, 2 * PAD) == 0) {
                r++;
            }
            if (n + 1 > room) {
                break;
            }
            out[n++] = (uint8_t)(0x80 | (r - 1));
            t += r;
            continue;
        } else {
            mask = (memcmp(cur, prev, PAD) != 0) | (memcmp(cur + PAD, prev + PAD, PAD) != 0) << 1;
        }
        need = 1 + ((mask & 1) + (mask >> 1)) * PAD;
        if (n + need > room) {
            break;
        }
        out[n++] = (uint8_t)mask;
        if (mask & 1) {
            memcpy(out + n, cur, PAD);
            n += PAD;
        }
        if (mask & 2) {
            memcpy(out + n, cur + PAD, PAD);
            n += PAD;
        }
        t++;
    }
    *count = t - from;
    return n;
}

/* A T_SPEC_DATA packet: {first blank, blanks, blanks known, check values, bytes of blanks, 0}, the blanks, then
   the host's sure check values (blank, value: 12 bytes each) from the first not yet sent, as far as the packet's
   blanks go (one becomes sure only when the host has run its blank with both players' input: those of the newest
   blanks come with a later packet). A packet spans at most SPEC_NOTES check values' worth of blanks, so that
   the spectator can compare every one of them. */
enum { SPEC_NOTES = 16 };
static void spec_data(Watcher *w, uint64_t now) {
    uint8_t pkt[SPEC_HEAD + SPEC_PAYLOAD + SPEC_NOTES * 12];
    uint32_t head[8], count = 0, at;
    int len = spec_encode(w->next, SPEC_NOTES * SYNC_EVERY, pkt + SPEC_HEAD, SPEC_PAYLOAD, &count), notes = 0;

    if (w->noteNext < w->next - w->next % SYNC_EVERY) { /* (after a rewind: from the blanks it gets again) */
        w->noteNext = w->next - w->next % SYNC_EVERY;
    }
    for (at = w->noteNext; at < w->next + count && notes < SPEC_NOTES; at += SYNC_EVERY) {
        uint64_t v;
        if (at == 0) {
            continue;
        }
        if (!note_get(&sNotesSure, at, &v)) {
            break; /* (not sure yet on the host: in a later packet) */
        }
        memcpy(pkt + SPEC_HEAD + len + notes * 12, &at, 4);
        memcpy(pkt + SPEC_HEAD + len + notes * 12 + 4, &v, 8);
        notes++;
    }
    w->noteNext = at;
    head[0] = MAGIC;
    head[1] = T_SPEC_DATA;
    head[2] = w->next;
    head[3] = count;
    head[4] = sLogN;
    head[5] = (uint32_t)notes;
    head[6] = (uint32_t)len;
    head[7] = 0;
    memcpy(pkt, head, SPEC_HEAD);
    net_send(pkt, SPEC_HEAD + len + notes * 12, &w->addr);
    w->next += count;
    w->sentNs = now;
}

/* The host of a room code: the spectators the matchmaking service has told of (net_match.c, Match_Watchers). Each
   new one is let through this side's relay (if it holds one) and knocked at for ten seconds, so that this side's
   router lets its greeting in (as the host does for the other player). Called by the lobby and by spec_pump. */
enum { WATCH_KNOCK = 16 };
static struct sockaddr_in sKnock[WATCH_KNOCK];
static uint64_t sKnockUntil[WATCH_KNOCK], sKnockLast;
static int sKnockGen;

static void watch_service(void) {
    char addr[MATCH_WATCH_ADDRS][64];
    uint64_t now = SDL_GetTicksNS();
    int gen, n = Match_Watchers(addr, MATCH_WATCH_ADDRS, &gen), i, k;

    if (sSock < 0 || Match_SocketBusy()) {
        return;
    }
    if (gen != sKnockGen) {
        sKnockGen = gen;
        for (i = 0; i < n; i++) {
            struct sockaddr_in a;
            char text[64], *colon;
            int seen = strncmp(addr[i], "seen=", 5) == 0;
            memset(&a, 0, sizeof(a));
            a.sin_family = AF_INET;
            snprintf(text, sizeof(text), "%s", seen ? addr[i] + 5 : addr[i]);
            colon = seen ? NULL : strrchr(text, ':');
            if (colon != NULL) {
                *colon = '\0';
                a.sin_port = htons((uint16_t)atoi(colon + 1));
            }
            if (strncmp(text, "relay=", 6) == 0 || inet_pton(AF_INET, text, &a.sin_addr) != 1) {
                continue;
            }
            Relay_Permit(sSock, &a);
            if (seen || a.sin_port == 0) {
                continue;
            }
            for (k = 0; k < WATCH_KNOCK && !(sKnockUntil[k] != 0 && same_addr(&sKnock[k], &a)); k++) {
            }
            if (k == WATCH_KNOCK) { /* (a new one: in the first free place, or the oldest) */
                int pick = 0;
                for (k = 0; k < WATCH_KNOCK; k++) {
                    if (sKnockUntil[k] < sKnockUntil[pick]) {
                        pick = k;
                    }
                }
                k = pick;
                sKnock[k] = a;
            }
            sKnockUntil[k] = now + 10000000000ull;
        }
    }
    if (now - sKnockLast >= 200000000ull) {
        uint32_t punch[2] = {MAGIC, T_PUNCH};
        sKnockLast = now;
        for (k = 0; k < WATCH_KNOCK; k++) {
            if (sKnockUntil[k] > now) {
                net_send(punch, sizeof(punch), &sKnock[k]);
            }
        }
    }
}

/* The host: what is due to each spectator. Called with every look at the socket (receive), done at most every
   16 ms: a spectator that is live gets a packet every other blank or so, one with nothing new a packet every half
   second (to be heard), and those that are catching up SPEC_BURST packets every 16 ms between them all (about
   140 KB a second at the most, whatever their number: the host's line is the players' match first). */
static void spec_pump(void) {
    static uint64_t last;
    static int turn;
    uint64_t now = SDL_GetTicksNS();
    int i, k, budget = SPEC_BURST;

    if (!(sMode == 1 || Lobby_ServesSpectators()) || now - last < 16000000ull) {
        return;
    }
    if (now - last > 1000000000ull) { /* (this copy was away a while, loading: the spectators' silence says nothing) */
        for (i = 0; i < SPEC_MAX; i++) {
            sWatch[i].heardNs = now;
        }
    }
    last = now;
    if (sMode != 3) { /* (a lobby's owner that watches passes on what its host sent it) */
        spec_log_update();
    }
    if (sFromLobby) {
        watch_service(); /* (a room code: spectators the service has told of) */
    }
    turn++;
    for (k = 0; k < SPEC_MAX; k++) {
        Watcher *w = &sWatch[(turn + k) % SPEC_MAX];
        uint32_t behind;
        i = (turn + k) % SPEC_MAX;
        if (!w->on) {
            continue;
        }
        if (now > w->heardNs && now - w->heardNs > SPEC_QUIET_NS) {
            spec_drop(i, "stopped answering: no longer sent the match");
            continue;
        }
        if (now - w->sinceNs > 60000000000ull && sLogN - w->acked > SPEC_LAG_BLANKS) {
            spec_drop(i, "does not keep up (a minute behind): no longer sent the match");
            continue;
        }
        if (w->next > w->acked && now - w->movedNs > 500000000ull && now - w->rewoundNs > 500000000ull) {
            w->next = w->acked; /* nothing taken for half a second: from what it has, again */
            w->noteNext = w->acked - w->acked % SYNC_EVERY;
            w->rewoundNs = now;
        }
        behind = sLogN - w->next;
        if (behind == 0) {
            if (now - w->sentNs >= 500000000ull) {
                spec_data(w, now);
            }
        } else if (behind > 240) {
            while (budget > 0 && w->next < sLogN) {
                spec_data(w, now);
                budget--;
            }
        } else if (now - w->sentNs >= 30000000ull) {
            spec_data(w, now);
        }
    }
    if (sConnected && sMode != 3 && (sListDirty || (sWatchCount > 0 && now - sListSentNs > 2000000000ull))) {
        spec_list_send();
    }
}

static void spec_ack_send(int gap) {
    uint32_t ack[4] = {MAGIC, T_SPEC_ACK, sLogN, (uint32_t)gap};
    if (sLoss != 0 && (rand() % 100) < sLoss) {
        return; /* (testing: BT3_NET_LOSS) */
    }
    net_send(ack, sizeof(ack), &sPeer);
    sSpecAckNs = SDL_GetTicksNS();
}

extern int Port_SessionCan(void); /* gs/state.c */
static void spec_compare(uint32_t at) {
    uint64_t mine, theirs;
    if (!note_get(&sNotesOwn, at, &mine) || !note_get(&sNotesSure, at, &theirs)) {
        return;
    }
    sNotesOwn.has[at / SYNC_EVERY] = 2; /* (compared: not again) */
    sSpecCompared++;
    sSyncCompared++;
    if (mine == theirs) {
        return;
    }
    sSpecDiffered++;
    if (sSpecApartAt == 0) {
        sSpecApartAt = at;
        fprintf(stderr, "bt3: net: spectating: this game differs from the host's at blank %u (this copy %016llx, the host %016llx)\n", (unsigned)at,
                (unsigned long long)mine, (unsigned long long)theirs);
        if (sSpecResyncs < 1 && Port_SessionCan() && getenv("BT3_SPEC_NORESYNC") == NULL) {
            sSpecResyncs++;
            sSpecRejoin = 1; /* followed again from the start (Port_LobbyAuto), once */
            Port_UiNotice("Spectating: this screen no longer shows the players' fight (desync). Following the match again from the start...");
        } else {
            Port_UiNotice("Spectating: this screen no longer shows the players' fight (desync). The players' match is not affected.");
        }
    }
}

/* The spectator: blanks from the host. */
static void spec_take(const uint8_t *pkt, int n) {
    uint32_t h[6], t, end, k;
    uint8_t cur[2 * PAD];
    int i = SPEC_HEAD, stop;

    if (n < SPEC_HEAD) {
        return;
    }
    memcpy(h, pkt + 8, 24); /* first, blanks, known, check values, bytes of blanks, 0 */
    if (h[4] > (uint32_t)(n - SPEC_HEAD) || h[3] > SPEC_NOTES || SPEC_HEAD + h[4] + h[3] * 12 > (uint32_t)n) {
        return;
    }
    stop = SPEC_HEAD + (int)h[4];
    sSpecHead = h[2] > sSpecHead ? h[2] : sSpecHead;
    for (k = 0; k < h[3]; k++) { /* the host's check values: compared with this copy's as both are known */
        uint32_t at;
        uint64_t v, had;
        memcpy(&at, pkt + stop + k * 12, 4);
        memcpy(&v, pkt + stop + k * 12 + 4, 8);
        if (at != 0 && at % SYNC_EVERY == 0 && !note_get(&sNotesSure, at, &had)) {
            note_put(&sNotesSure, at, v);
            spec_compare(at);
        }
    }
    if (h[1] == 0) {
        return;
    }
    if (h[0] > sLogN) { /* a packet before it went missing: say so (at most every 50 ms) */
        uint64_t now = SDL_GetTicksNS();
        if (now - sSpecGapNs > 50000000ull) {
            sSpecGapNs = now;
            spec_ack_send(1);
        }
        return;
    }
    end = h[0] + h[1];
    for (t = h[0]; t < end && i < stop;) {
        uint8_t b = pkt[i++];
        uint32_t r = 1;
        if (b & 0x80) {
            if (t == h[0]) {
                return; /* (not a packet this program makes) */
            }
            r = (uint32_t)(b & 0x7F) + 1;
        } else {
            if ((t == h[0] && b != 3) || b == 0 || b > 3 || i + ((b & 1) + (b >> 1)) * PAD > stop) {
                return;
            }
            if (b & 1) {
                memcpy(cur, pkt + i, PAD);
                i += PAD;
            }
            if (b & 2) {
                memcpy(cur + PAD, pkt + i, PAD);
                i += PAD;
            }
        }
        for (k = 0; k < r && t < end; k++, t++) {
            if (t == sLogN) {
                if (!log_room(sLogN + 1)) {
                    return;
                }
                memcpy(sLog + (size_t)sLogN * 2 * PAD, cur, 2 * PAD);
                sLogN++;
            }
        }
    }
}

/* A packet of the spectators' kind. Returns 1 if it was for this copy. */
static int spec_packet(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type) {
    int serving = sMode == 1 || Lobby_ServesSpectators(); /* (a lobby's owner passes the match on, whatever its part) */
    if (sMode == 3 && (same_addr(from, &sPeer) || !serving)) {
        if (!same_addr(from, &sPeer)) {
            return 0; /* (a spectator listens to the host only) */
        }
        if (type == T_SPEC_DATA && sLoss != 0 && (rand() % 100) < sLoss) {
            return 0; /* (testing: BT3_NET_LOSS drops what comes in too) */
        }
        sSpecHeardNs = SDL_GetTicksNS();
        if (type == T_SPEC_SETUP && n >= 44 + 2 * NAME_LEN && !sSpecSetup) {
            uint32_t s[9];
            memcpy(s, pkt + 8, 36);
            if (s[0] != SPEC_VERSION || s[1] != NET_VERSION) {
                sSpecNo = SPEC_NO_VERSION;
                return 1;
            }
            sDelay = (int)s[2] < 0 || (int)s[2] > 30 ? 2 : (int)s[2];
            sRules = s[3];
            sSeed = s[4];
            sOpts[0] = s[5];
            sOpts[1] = s[6];
            sSpecHead = s[8];
            memcpy(sNames[0], pkt + 44, NAME_LEN);
            memcpy(sNames[1], pkt + 44 + NAME_LEN, NAME_LEN);
            sNames[0][NAME_LEN - 1] = sNames[1][NAME_LEN - 1] = '\0';
            sSpecSetup = 1;
            sConnected = 1;
        } else if (type == T_SPEC_DATA && sSpecSetup) {
            spec_take(pkt, n);
        } else if (type == T_SPEC_WAIT) {
            sSpecWaits = 1;
        } else if (type == T_SPEC_NO && n >= 12) {
            memcpy(&sSpecNo, pkt + 8, 4);
            sSpecNo = sSpecNo == 0 ? SPEC_NO_VERSION : sSpecNo;
        } else if (type == T_SPEC_BYE) {
            sSpecEnded = 1;
        }
        return 1;
    }
    if (type == T_SPEC_HELLO && Lobby_Active() && !Lobby_IsMember(from)) {
        Lobby_Stranger(pkt, n, from, type); /* (a lobby's: told it is one, to join it) */
        return 1;
    }
    if (type == T_SPEC_HELLO) {
        spec_hello(pkt, n, from);
        return 1;
    }
    if (type == T_SPEC_LIST && (sMode == 2 || (sMode == 1 && Lobby_Active())) && sConnected && same_addr(from, &sPeer) && n >= 12) {
        uint32_t count;
        int i;
        int was = sWatchCount;
        memcpy(&count, pkt + 8, 4);
        sWatchCount = 0;
        for (i = 0; i < (int)count && i < SPEC_MAX && 12 + (i + 1) * NAME_LEN <= n; i++) {
            int k;
            memcpy(sWatchName[i], pkt + 12 + i * NAME_LEN, NAME_LEN);
            sWatchName[i][NAME_LEN - 1] = '\0';
            for (k = 0; sWatchName[i][k] != '\0'; k++) {
                if ((unsigned char)sWatchName[i][k] < 0x20 || sWatchName[i][k] == 0x7F) {
                    sWatchName[i][k] = '?';
                }
            }
            sWatchCount++;
        }
        if (sWatchCount != was) {
            fprintf(stderr, "bt3: net: %d watching this match%s%s\n", sWatchCount, sWatchCount > 0 ? ", the first " : "", sWatchCount > 0 ? sWatchName[0] : "");
        }
        return 1;
    }
    if (serving && (type == T_SPEC_ACK || type == T_SPEC_BYE)) {
        int i;
        for (i = 0; i < SPEC_MAX; i++) {
            Watcher *w = &sWatch[i];
            if (!w->on || !same_addr(&w->addr, from)) {
                continue;
            }
            w->heardNs = SDL_GetTicksNS();
            if (type == T_SPEC_BYE) {
                spec_drop(i, "left");
            } else if (n >= 16) {
                uint32_t v[2];
                memcpy(v, pkt + 8, 8);
                if (v[0] > w->acked && v[0] <= sLogN) {
                    w->acked = v[0];
                    w->movedNs = w->heardNs;
                }
                if (w->next < w->acked) {
                    w->next = w->acked;
                }
                if (v[1] != 0 && w->next > w->acked && w->heardNs - w->rewoundNs > 150000000ull) {
                    w->next = w->acked; /* it missed a packet: from what it has, again */
                    w->noteNext = w->acked - w->acked % SYNC_EVERY;
                    w->rewoundNs = w->heardNs;
                }
            }
            return 1;
        }
    }
    return 0;
}

/* The spectator's greeting, to `to` (the session's and the lobby's). */
static void spec_hello_send(const struct sockaddr_in *to, int asking) {
    uint32_t hello[8 + NAME_LEN / 4] = {MAGIC, T_SPEC_HELLO, SPEC_VERSION, NET_VERSION, 0, 0, 0};
    unsigned long long fp;
    if (fprint_mine(&fp)) {
        hello[4] = FP_MARK;
        hello[5] = (uint32_t)fp;
        hello[6] = (uint32_t)(fp >> 32);
    }
    memcpy(&hello[7], sMyName, NAME_LEN);
    hello[7 + NAME_LEN / 4] = (uint32_t)asking;
    net_send(hello, sizeof(hello), to);
}

static const char *spec_no_text(int why) {
    switch (why) {
    case SPEC_NO_DATA:
        return "The host's game data are not the same as yours (a modified disc image or mods on one side): you cannot watch this match.";
    case SPEC_NO_FULL:
        return "This match already has as many spectators as it can take (6).";
    case SPEC_NO_OFF:
        return "The host does not let anyone watch its matches.";
    case SPEC_NO_PLAIN:
        return "This match cannot be watched (it was not started from the online menus).";
    case SPEC_NO_PLAYER:
        return "That is the address of the player who joined, not of the host: spectators connect to the host.";
    case SPEC_NO_LOBBY:
        return "This match is a lobby's: join the lobby (its code, or its owner's address) to watch it.";
    default:
        return "The host's game is a different version: watching needs the same release as the host's.";
    }
}

static const char kSpecNoAnswer[] = "No answer from the host. Check the address and port. Watching a match needs a release newer than 0.1.38.7 on "
                                    "the host as well (an older host does not answer spectators).";

static void spec_tick(unsigned tick);

/* Takes in what has arrived. Returns 1 if anything did. */
static int receive(void) {
    uint8_t pkt[2048];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    int got = 0, n;

    queue_flush();
    if (sMode != 3) {
        chat_send();
    }
    while ((n = net_recv(pkt, (int)sizeof(pkt), &from)) >= 8) {
        uint32_t magic, type;
        memcpy(&magic, pkt, 4);
        memcpy(&type, pkt + 4, 4);
        if (magic != MAGIC) {
            continue;
        }
        if (type >= T_LOBBY_FIRST && type <= T_LOBBY_LAST) { /* (a lobby's: from its members or its owner, net_lobby.c) */
            Lobby_Packet(pkt, n, &from, type);
            continue;
        }
        if (type >= T_SPEC_HELLO && type <= T_SPEC_READY) { /* (spectators: from any address, see spec_packet) */
            got |= spec_packet(pkt, n, &from, type) && sMode == 3;
            continue;
        }
        if (type == T_HELLO && Lobby_ServesSpectators() && !same_addr(&from, &sPeer) && !Lobby_IsMember(&from)) {
            Lobby_Stranger(pkt, n, &from, type); /* (a lobby's owner in its match: someone knocking as for a match of two) */
            continue;
        }
        if (sMode == 3) {
            continue; /* (a spectator takes nothing else) */
        }
        if (sConnected && (from.sin_addr.s_addr != sPeer.sin_addr.s_addr || from.sin_port != sPeer.sin_port)) {
            continue; /* a match takes packets from the other player's address only: nobody else can end it or put buttons in */
        }
        got = 1;
        if (type == T_PING && n >= 12) { /* setting up: sent back as it came, for the round trip time */
            uint32_t pong[3] = {MAGIC, T_PONG, 0};
            memcpy(&pong[2], pkt + 8, 4);
            send_to_peer(pong, (int)sizeof(pong)); /* (through the test line's delay, like the input) */
        } else if (type == T_PONG && n >= 12) {
            uint32_t ts;
            int rtt;
            memcpy(&ts, pkt + 8, 4);
            rtt = (int)(now_ms() - ts);
            if (rtt >= 0 && rtt < 5000 && (sSetupRtt < 0 || rtt < sSetupRtt)) {
                sSetupRtt = rtt;
            }
        } else if (type == T_CONFIG && sMode == 2 && n >= 16) { /* the host's choices, made after it measured the line */
            uint32_t v[2], ack[2] = {MAGIC, T_CONFIG_ACK};
            memcpy(v, pkt + 8, 8);
            sCfgDelay = (int)v[0];
            sCfgRoll = (int)v[1];
            if (n >= 24) {
                memcpy(&sRules, pkt + 20, 4);
            }
            if (n >= 24 + NAME_LEN) {
                memcpy(sNames[0], pkt + 24, NAME_LEN);
                sNames[0][NAME_LEN - 1] = '\0';
            }
            if (n >= 28 + NAME_LEN) {
                opts_take(0, pkt + 24 + NAME_LEN);
            }
            if (n >= 32 + NAME_LEN) {
                memcpy(&sSeed, pkt + 28 + NAME_LEN, 4);
            }
            sCfgGot = 2;
            net_send(ack, sizeof(ack), &from);
        } else if (type == T_CONFIG_ACK && sMode == 1) {
            sCfgAcked = 1;
        } else if (type == T_CHAT && sConnected && n >= 16) { /* (from the other player's address only: above) */
            uint32_t seq, len;
            memcpy(&seq, pkt + 8, 4);
            memcpy(&len, pkt + 12, 4);
            if (len <= CHAT_TEXT && len <= (uint32_t)(n - 16)) {
                PortChat_Heard(seq, (const char *)pkt + 16, (int)len);
            }
        } else if (type == T_BYE && sConnected) {
            uint32_t why = 0;
            if (n >= 12) {
                memcpy(&why, pkt + 8, 4);
            }
            if (sGone < 3) { /* (a lobby's own end, said first, stays what the screen says) */
                sGone = why == BYE_APART ? 2 : 1;
                sByeHeard = why;
            }
        } else if (type == T_HELLO && sMode == 1 && !(n >= 12 && memcmp(pkt + 8, &kVersion, 4) == 0)) {
            static int said;
            if (!said++) {
                fprintf(stderr, "bt3: net: a copy of another version tried to join; both players need the same release\n");
            }
        } else if (type == T_HELLO && sMode == 1 && sFromLobby && n < 12 + NAME_LEN) {
            /* a greeting of the other side's lobby, still on its way or sent again because this side's answers
               were lost: answered as the lobby did, so that the other side starts its session too */
            uint32_t ack[3] = {MAGIC, T_HELLO_ACK, NET_VERSION};
            net_send(ack, sizeof(ack), &from);
        } else if (type == T_HELLO && sMode == 1) {
            uint32_t ack[8 + NAME_LEN / 4] = {MAGIC, T_HELLO_ACK, (uint32_t)sDelay, (uint32_t)sRollMax, NET_VERSION, sRules};
            ack[7 + NAME_LEN / 4] = sSeed;
            memcpy(&ack[6], sMyName, NAME_LEN);
            ack[6 + NAME_LEN / 4] = sOpts[0];
            if (n >= 12 + NAME_LEN) { /* the other player's name came with the greeting */
                memcpy(sNames[1], pkt + 12, NAME_LEN);
                sNames[1][NAME_LEN - 1] = '\0';
            }
            if (n >= 16 + NAME_LEN) {
                opts_take(1, pkt + 12 + NAME_LEN);
            }
            {   /* (does the other copy know the 120% pace? if not, the match is at the game's own) */
                uint32_t cap = 0;
                if (n >= 20 + NAME_LEN) {
                    memcpy(&cap, pkt + 16 + NAME_LEN, 4);
                }
                if ((cap & 0xFFFF0000u) != CAP_MARK) {
                    cap = 0;
                }
                if (!(cap & 2)) { /* does not know the number: 120% if it knows that and it is that, else 100% */
                    sRules &= ~(0xFFu << RULE_SPEED_SHIFT);
                }
                if (!(cap & 1)) {
                    sRules &= ~RULE_FAST;
                }
                ack[5] = sRules;
            }
            memcpy(sNames[0], sMyName, NAME_LEN);
            sPeer = from; /* the host learns the other side's address from its greeting */
            sConnected = 1;
            net_send(ack, sizeof(ack), &sPeer);
        } else if (type == T_HELLO_ACK && sMode == 2 && sFromLobby && n < 20) {
            /* an answer to the lobby's greeting, still in the socket the session took over: nothing to do */
        } else if (type == T_HELLO_ACK && sMode == 2) {
            if (!(n >= 20 && memcmp(pkt + 16, &kVersion, 4) == 0)) {
                sVersionBad = 1;
            } else if (!sConnected) {
                uint32_t v[2];
                sPeer = from; /* (a lobby's match: the one of the host's two addresses that answered) */
                memcpy(v, pkt + 8, 8);
                sCfgDelay = (int)v[0];
                sCfgRoll = (int)v[1];
                if (n >= 24) {
                    memcpy(&sRules, pkt + 20, 4);
                }
                if (n >= 24 + NAME_LEN) {
                    memcpy(sNames[0], pkt + 24, NAME_LEN);
                    sNames[0][NAME_LEN - 1] = '\0';
                }
                if (n >= 28 + NAME_LEN) {
                    opts_take(0, pkt + 24 + NAME_LEN);
                }
                if (n >= 32 + NAME_LEN) {
                    memcpy(&sSeed, pkt + 28 + NAME_LEN, 4);
                }
                memcpy(sNames[1], sMyName, NAME_LEN);
                sCfgGot = 1;
                sConnected = 1;
            }
        } else if (type == T_FPRINT) {
            sDataBad |= fprint_differs(pkt, n);
        } else if (type == T_ARRIVED && sConnected && n >= 16) {
            uint32_t v[2];
            memcpy(v, pkt + 8, 8);
            sTheirArrival = v[0];
            if (v[1] == 0 && sMyArrival != 0) {
                uint32_t answer[4] = {MAGIC, T_ARRIVED, sMyArrival, 1};
                send_to_peer(answer, (int)sizeof(answer));
            }
        } else if (type == T_INPUT && n >= IN_HEAD && (sConnected || sMode == 1)) { /* (the joining side: not before the host's answer) */
            uint32_t base, count, i, other = (uint32_t)!sMe;
            {
                uint64_t at = SDL_GetTicksNS();
                if (sInLastNs != 0 && at - sInLastNs > sInGapNs) {
                    sInGapNs = at - sInLastNs;
                }
                sInLastNs = at;
                sInPkts++;
            }
            memcpy(&base, pkt + 8, 4);
            memcpy(&count, pkt + 12, 4);
            memcpy(&sTheirAhead, pkt + 16, 4);
            if (sMode == 1 && !sConnected) {
                sPeer = from;
            }
            {
                uint32_t at;
                memcpy(&at, pkt + 32, 4);
                if (at != 0) { /* the other copy's check value for that blank: compared when this copy is sure of it too */
                    SyncNote *o = &sSyncTheirs[at / SYNC_EVERY % SYNC_SLOTS];
                    o->tick = at;
                    memcpy(&o->value, pkt + 36, 8);
                }
            }
            sConnected = 1; /* (an input packet says the greeting got through) */
            {
                uint32_t ts, echo, hold, now = now_ms();
                memcpy(&ts, pkt + 20, 4);
                memcpy(&echo, pkt + 24, 4);
                memcpy(&hold, pkt + 28, 4);
                sEchoTs = ts;
                sEchoAtMs = now;
                if (echo != 0 && now - echo - hold < 5000u) { /* the round trip: there and back, less their holding time */
                    /* The smallest of the last 64: a packet waits in the socket until the next blank looks, on both
                       sides, which adds up to two blanks to a single measurement and nothing to the best one. */
                    static int ring[64], at, filled;
                    int rtt = (int)(now - echo - hold), k, best;
                    ring[at++ & 63] = rtt;
                    filled = filled < 64 ? filled + 1 : 64;
                    best = ring[0];
                    for (k = 1; k < filled; k++) {
                        best = ring[k] < best ? ring[k] : best;
                    }
                    sPingMs = best;
                }
            }
            for (i = 0; i < count && IN_HEAD + (i + 1) * PAD <= (uint32_t)n; i++) {
                if (base + i >= sHave[other]) {
                    if (base + i > sHave[other]) {
                        break; /* a gap: wait for a packet that covers it */
                    }
                    memcpy(sIn[other][(base + i) % RING], pkt + IN_HEAD + i * PAD, PAD);
                    sHave[other] = base + i + 1;
                }
            }
        }
    }
    if (sMode == 1 || Lobby_ServesSpectators()) {
        spec_pump();
    }
    Lobby_Tick();
    return got;
}

/* Testing: this copy behaves like a weak machine (real time only; nothing of the game's state may follow from it).
     BT3_TEST_SLOW_READ=<ms>    every file read of the game takes that long more (what = 0)
     BT3_TEST_SLOW_START=<ms>   that long between the connection and the session's first blank (what = 1)
     BT3_TEST_SLOW_BLANK=<ms>   every blank of a match takes that long more (what = 2) */
void Port_TestSlow(int what, int amount) {
    static int ms[3] = {-1, -1, -1};
    (void)amount;
    if (ms[0] < 0) {
        ms[0] = getenv("BT3_TEST_SLOW_READ") != NULL ? atoi(getenv("BT3_TEST_SLOW_READ")) : 0;
        ms[1] = getenv("BT3_TEST_SLOW_START") != NULL ? atoi(getenv("BT3_TEST_SLOW_START")) : 0;
        ms[2] = getenv("BT3_TEST_SLOW_BLANK") != NULL ? atoi(getenv("BT3_TEST_SLOW_BLANK")) : 0;
    }
    if (what >= 0 && what < 3 && ms[what] > 0) {
        SDL_Delay((Uint32)ms[what]);
    }
}

static int sSession; /* a session started from Dragon Net Battle (below), not a plain connected run */
static int sArrived; /* its start-up has reached the character select */
static uint32_t sStartedMs; /* when the two were connected (for the log) */

/* Before connecting: this game's gameplay fingerprint must be known (it is sent with the greetings, and a copy
   that connected without one was let in unchecked). With its cache it is a matter of seconds after the start;
   meanwhile the session holds its picture with a line. */
static void fp_wait(void) {
    uint64_t t0 = SDL_GetTicksNS();
    int said = 0;
    while (!fp_known()) {
        if (!said) {
            said = 1;
            fprintf(stderr, "bt3: net: this game's gameplay data are still being checked: waiting for the fingerprint before connecting\n");
        }
        if (sSession) {
            Port_UiBanner("Checking your game data...", 0);
            Gs_HoldFrame();
        }
        SDL_Delay(10);
    }
    if (said) {
        fprintf(stderr, "bt3: net: the gameplay fingerprint is known after %u ms: connecting\n", (unsigned)((SDL_GetTicksNS() - t0) / 1000000ull));
    }
}

/* Connects: role 1 hosts on `port`, role 2 joins `join` ("address:port"), role 3 watches the match hosted at `join`
   (a spectator). Waits until the other side is there. */
static void net_start(int role, const char *host, const char *join) {
    struct sockaddr_in local;
    uint64_t t0, last = 0;
    uint32_t i;

    fp_wait();
    sMode = role;
    sConnected = 0;
    sTick = 0;
    memset(sIn, 0, sizeof(sIn));
    memset(sHave, 0, sizeof(sHave)); /* (a second session in one run starts as clean as the first) */
    PortChat_Reset();
    sWaitNs = sWaits = 0;
    {   /* spectators: nothing of the last session's */
        int i;
        for (i = 0; i < SPEC_MAX; i++) {
            sWatch[i].on = 0;
        }
        sWatchCount = sListDirty = 0;
        sLogN = 0;
        for (i = 0; i < 2; i++) {
            NoteLog *l = i == 0 ? &sNotesSure : &sNotesOwn;
            if (l->has != NULL) {
                memset(l->has, 0, l->cap);
            }
        }
        sSpecOpen = sSpecSetup = sSpecWaits = sSpecNo = sSpecEnded = sSpecSkip = sSpecNoticed = sSpecLeaveAsked = sSpecRejoin = 0;
        sSpecLead = SPEC_LEAD_START;
        sSpecLeadSince = sSpecLeadLog = 0;
        sSpecNudge = 0;
        sSpecHead = 0;
        sSpecApartAt = 0;
        sSpecCompared = sSpecDiffered = sSpecSkipped = 0;
        sSpecHeardNs = sSpecAckNs = sSpecGapNs = sSpecDrawNs = 0;
        if (role == 3) {
            snprintf(sSpecTarget, sizeof(sSpecTarget), "%s", join);
        }
    }
    sFromLobby = sHandSock >= 0;
    if (sFromLobby) {
        sSock = sHandSock;
        sHandSock = -1;
    } else {
#ifdef _WIN32
        WSADATA w;
        u_long on = 1;
        WSAStartup(MAKEWORD(2, 2), &w);
        sSock = (int)socket(AF_INET, SOCK_DGRAM, 0);
        ioctlsocket(sSock, FIONBIO, &on);
#else
        sSock = socket(AF_INET, SOCK_DGRAM, 0);
        fcntl(sSock, F_SETFL, fcntl(sSock, F_GETFL, 0) | O_NONBLOCK);
#endif
    }
    sMe = sMode == 3 ? 0 : sMode - 1; /* (a spectator sees the match as the host does) */
    if (getenv("BT3_NET_LOCALPORT") != NULL && sCodeLocalPort == 0) { /* (across the restart into the session) */
        sCodeLocalPort = atoi(getenv("BT3_NET_LOCALPORT"));
    }
    if (getenv("BT3_NET_PUNCH") != NULL && !sCodePunchSet) {
        char text[64], *colon;
        snprintf(text, sizeof(text), "%s", getenv("BT3_NET_PUNCH"));
        colon = strrchr(text, ':');
        if (colon != NULL) {
            *colon = '\0';
            memset(&sCodePunch, 0, sizeof(sCodePunch));
            sCodePunch.sin_family = AF_INET;
            sCodePunch.sin_port = htons((uint16_t)atoi(colon + 1));
            sCodePunchSet = inet_pton(AF_INET, text, &sCodePunch.sin_addr) == 1;
        }
    }
    if (getenv("BT3_NET_NAME") != NULL) { /* (across the restart into the session) */
        Port_NetNameSet(getenv("BT3_NET_NAME"));
    }
    memset(sNames, 0, sizeof(sNames));
    memcpy(sNames[sMe], sMyName, NAME_LEN);
    memset(sOpts, 0, sizeof(sOpts));
    if (sMode != 3) {
        sOpts[sMe] = own_options();
    } else {
        sNames[0][0] = '\0'; /* (both names are the host's word) */
    }
    if (getenv("BT3_NET_RULES") != NULL) { /* (the host's, across the restart into the session, or for a test) */
        sRules = (uint32_t)strtoul(getenv("BT3_NET_RULES"), NULL, 0);
    }
    if (getenv("BT3_NET_SPEED") != NULL) { /* testing: 120 or 100 */
        Port_NetSpeedSet(atoi(getenv("BT3_NET_SPEED")));
    }
    sAutoDelay = getenv("BT3_NET_DELAY") != NULL ? strcmp(getenv("BT3_NET_DELAY"), "auto") == 0 : sOptAuto;
    sDelay = getenv("BT3_NET_DELAY") != NULL && !sAutoDelay ? atoi(getenv("BT3_NET_DELAY")) : sOptDelay >= 0 ? sOptDelay : 2;
    sSetupRtt = -1;
    sCfgAcked = 0;
    if (sDelay < 0) { sDelay = 0; }
    if (sDelay > 30) { sDelay = 30; }
    sCfgGot = 0;
    sGone = 0;
    sGoneNote[0] = '\0';
    sVersionBad = 0;
    sDataBad = 0;
    sFpHeard = 0;
    sEchoTs = 0;
    sPingMs = -1;
    sSecRoll = sSecRollTicks = sSecWaits = sSecWaitMs = sSecTicks = 0;
    memset(sShown, 0, sizeof(sShown));
    sApartAt = 0;
    sApartCount = 0;
    sInPkts = sWaitNotes = 0;
    sInLastNs = sInGapNs = sSecWaitMaxNs = 0;
    sSecWaitWhy = 0;
    sLoss = getenv("BT3_NET_LOSS") != NULL ? atoi(getenv("BT3_NET_LOSS")) : 0;
    sLagMs = getenv("BT3_NET_LAG") != NULL ? atoi(getenv("BT3_NET_LAG")) : 0;
    sLatencyMs = getenv("BT3_NET_LATENCY") != NULL ? atoi(getenv("BT3_NET_LATENCY")) : 0;
    sQueueHead = sQueueTail = 0; /* (what the test line held back from a session before is not this one's: a lobby's
                                    next match goes on with the same socket, perhaps to another player) */
    /* BT3_NET_ROLLBACK=<n>: the game does not wait for the other player's input; it goes on with a guess (what
       they held last) for up to n blanks and, when the real input differs, goes back and runs those blanks again
       (roll_tick). Needs the roll ring of gs/state.c (the 64-bit programs); 0 or unset: wait, as before. */
    sRollMax = !Port_RollCan() ? 0 : getenv("BT3_NET_ROLLBACK") != NULL ? atoi(getenv("BT3_NET_ROLLBACK")) : sOptRoll >= 0 ? sOptRoll : 0;
    if (sRollMax < 0) { sRollMax = 0; }
    if (sRollMax > 30) { sRollMax = 30; }
    sChecked = sLive = 0;
    sResim = sRollBegun = 0;
    memset(sSyncMine, 0, sizeof(sSyncMine));
    memset(sSyncTheirs, 0, sizeof(sSyncTheirs));
    sSyncOutTick = 0;
    sSyncOutValue = 0;
    sByeWhy = 0;
    sByeHeard = 0;
    sByeSent = 0;
    {
        static int armed;
        if (!armed) {
            armed = 1;
            atexit(bye_at_exit);
        }
    }
    sTheirAhead = 0;
    sBalance = 0;
    sRollbacks = sRollTicks = sStalls = 0;
    if (getenv("BT3_NET_SCRIPT") != NULL) {
        sScript = fopen(getenv("BT3_NET_SCRIPT"), "rb");
    }
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&sPeer, 0, sizeof(sPeer));
    sPeer.sin_family = AF_INET;
    sSeed = 0;
    if (sMode == 1) {
        seed_draw();
        local.sin_port = htons((uint16_t)atoi(host));
        if (!sFromLobby && bind(sSock, (struct sockaddr *)&local, sizeof(local)) != 0) {
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
                if (sSession) {
                    Port_UiNotice("That address is not known.");
                    Port_NetLeave();
                }
                exit(2);
            }
            memcpy(&sPeer.sin_addr, he->h_addr_list[0], sizeof(sPeer.sin_addr));
        }
        if (sCodeLocalPort != 0 && !sFromLobby) {
            /* a match found with a room code: from the port the lobby used, so that the routers on the way keep
               taking this for the same conversation */
            local.sin_port = htons((uint16_t)sCodeLocalPort);
            bind(sSock, (struct sockaddr *)&local, sizeof(local));
        }
        /* The address is only printed when the player typed it. One that came from the matchmaking service (a
           room code, a quick match) is the other player's, and logs get pasted into bug reports. */
        if (sMode == 3 && Lobby_Active()) {
            fprintf(stderr, "bt3: net: watching the lobby's match (a spectator)...\n");
        } else if (sMode == 3) {
            fprintf(stderr, "bt3: net: watching the match of %s (a spectator)...\n", sCodeLocalPort != 0 ? "the host found by the matchmaking service" : join);
        } else if (Lobby_Active()) {
            fprintf(stderr, "bt3: net: joining the lobby's match (player 2)...\n");
        } else if (sCodeLocalPort != 0) {
            fprintf(stderr, "bt3: net: joining the host found by the matchmaking service (player 2)...\n");
        } else {
            fprintf(stderr, "bt3: net: joining %s (player 2)...\n", join);
        }
    }
    t0 = SDL_GetTicksNS();
    while (!sConnected) {
        uint64_t now = SDL_GetTicksNS();
        if (sMode == 1 && sCodePunchSet && now - last > 100000000ull) {
            uint32_t punch[2] = {MAGIC, T_PUNCH}; /* (room codes: keeps this side's router open for the one who joins) */
            net_send(punch, sizeof(punch), &sCodePunch);
            if (sCodePunch2.sin_port != 0) {
                net_send(punch, sizeof(punch), &sCodePunch2);
            }
            last = now;
        }
        if (sMode == 3) {
            if (now - last > 200000000ull) {
                spec_hello_send(&sPeer, 0);
                last = now;
            }
            receive();
            if (sSpecWaits && !sSpecSetup) {
                Port_UiBanner("The host's match has not begun yet. Waiting for it...", 0);
            }
            if (sSession) {
                Gs_HoldFrame();
            }
            if (sGone >= 3 && sSession) {
                gone(0);
            }
            if (sSpecNo != 0 || sSpecEnded || (!sSpecWaits && now - t0 > SPEC_ASK_NS) || now - t0 > 120000000000ull) {
                const char *why = sSpecNo != 0 ? spec_no_text(sSpecNo) : sSpecEnded ? "The match has ended." : sSpecWaits ? "The host's match did not begin in two minutes." : kSpecNoAnswer;
                fprintf(stderr, "bt3: net: spectating: %s\n", why);
                Port_UiBanner(NULL, 0);
                Port_UiNotice(why);
                if (sSession) {
                    Port_NetLeave();
                }
                exit(2);
            }
            SDL_Delay(2);
            continue;
        }
        if (sMode == 2 && now - last > 100000000ull) {
            uint32_t hello[5 + NAME_LEN / 4] = {MAGIC, T_HELLO, NET_VERSION};
            memcpy(&hello[3], sMyName, NAME_LEN);
            hello[3 + NAME_LEN / 4] = sOpts[1];
            hello[4 + NAME_LEN / 4] = CAP_MARK | 3;
            fprint_send(&sPeer);
            net_send(hello, sizeof(hello), &sPeer);
            if (sJoinAlt.sin_port != 0) {
                fprint_send(&sJoinAlt);
                net_send(hello, sizeof(hello), &sJoinAlt);
            }
            last = now;
        }
        receive();
        session_hold();
        if (sGone >= 3 && sSession) {
            gone(0); /* (a lobby's match that ended before it began: back to the lobby) */
        }
        if (sVersionBad) {
            fprintf(stderr, "bt3: net: the other player's game is another version; both need the same release\n");
            Port_UiNotice("The other player's game is a different version.");
            if (sSession) {
                Port_NetLeave();
            }
            exit(2);
        }
        SDL_Delay(2);
        if (now - t0 > 120000000000ull) {
            fprintf(stderr, "bt3: net: nobody answered in two minutes\n");
            if (sSession) {
                Port_UiNotice("Nobody answered in two minutes.");
                Port_NetLeave();
            }
            exit(2);
        }
    }
    /* Setting up. The host measures the round trip (a dozen pings, the smallest counts), chooses the input delay
       from it if that is automatic, and tells the other side its choices until that side has them; the other
       side answers the pings and waits for the choices. */
    if (sMode == 3) {
        Port_UiBanner(NULL, 0);
    } else if (sMode == 1) {
        uint64_t start = SDL_GetTicksNS(), lastPing = 0, lastCfg = 0;
        int pings = 0;
        while (SDL_GetTicksNS() - start < 400000000ull) {
            uint64_t now = SDL_GetTicksNS();
            if (pings < 12 && now - lastPing >= 25000000ull) {
                uint32_t ping[3] = {MAGIC, T_PING, now_ms()};
                send_to_peer(ping, (int)sizeof(ping));
                fprint_send(&sPeer);
                lastPing = now;
                pings++;
            }
            receive();
            session_hold();
            if (sGone >= 3 && sSession) {
                gone(0);
            }
            SDL_Delay(1);
        }
        if (sAutoDelay) {
            sDelay = auto_delay(sSetupRtt);
        }
        fprintf(stderr, "bt3: net: round trip %d ms%s\n", sSetupRtt, sAutoDelay ? " (the input delay is chosen from it)" : "");
        start = SDL_GetTicksNS();
        while (!sCfgAcked && SDL_GetTicksNS() - start < 3000000000ull) {
            uint64_t now = SDL_GetTicksNS();
            if (now - lastCfg >= 50000000ull) {
                uint32_t cfg[8 + NAME_LEN / 4] = {MAGIC, T_CONFIG, (uint32_t)sDelay, (uint32_t)sRollMax, NET_VERSION, sRules};
                cfg[7 + NAME_LEN / 4] = sSeed;
                memcpy(&cfg[6], sMyName, NAME_LEN);
                cfg[6 + NAME_LEN / 4] = sOpts[0];
                net_send(cfg, sizeof(cfg), &sPeer);
                fprint_send(&sPeer); /* (again with each: the other side refuses a match it had none for, FP_PEER_REQUIRED) */
                lastCfg = now;
            }
            receive();
            session_hold();
            if (sGone >= 3 && sSession) {
                gone(0);
            }
            SDL_Delay(1);
        }
    } else {
        uint64_t start = SDL_GetTicksNS(), lastFp = 0;
        while (sCfgGot != 2 && SDL_GetTicksNS() - start < 5000000000ull) {
            if (SDL_GetTicksNS() - lastFp >= 50000000ull) {
                fprint_send(&sPeer);
                lastFp = SDL_GetTicksNS();
            }
            receive();
            session_hold();
            if (sGone >= 3 && sSession) {
                gone(0);
            }
            SDL_Delay(1);
        }
    }
    if (sMode == 2 && sCfgGot) { /* the host's choices, unless this side was started with its own */
        if ((getenv("BT3_NET_DELAY") == NULL || sAutoDelay) && sCfgDelay >= 0 && sCfgDelay <= 30) {
            sDelay = sCfgDelay;
        }
        if (getenv("BT3_NET_ROLLBACK") == NULL && sOptRoll < 0 && sCfgRoll >= 0 && sCfgRoll <= 30) {
            sRollMax = Port_RollCan() ? sCfgRoll : 0;
        }
    }
    if (sMode != 3 && !sDataBad && !sFpHeard && fp_required()) {
        /* none has come: given a second more (it travels with the greetings, the pings and the host's choices, so
           only a copy that sends none at all should get here), then refused */
        uint64_t start = SDL_GetTicksNS(), lastFp = 0;
        while (!sFpHeard && SDL_GetTicksNS() - start < 1000000000ull) {
            if (SDL_GetTicksNS() - lastFp >= 50000000ull) {
                fprint_send(&sPeer);
                lastFp = SDL_GetTicksNS();
            }
            receive();
            session_hold();
            SDL_Delay(1);
        }
        if (!sFpHeard) {
            fprintf(stderr, "bt3: net: the other player's game sent no gameplay fingerprint: no match (an older release)\n");
            Port_UiNotice(kDataUnsaid);
            if (sSession) {
                Port_NetLeave();
            }
            exit(2);
        }
    }
    if (sDataBad) {
        Port_UiNotice(kDataDiffer);
        if (sSession) {
            Port_NetLeave();
        }
        exit(2);
    }
    /* the blanks before any input can have arrived (0 .. delay) are idle for both players */
    for (i = 0; i <= (uint32_t)sDelay; i++) {
        memcpy(sIn[0][i], kIdle, PAD);
        memcpy(sIn[1][i], kIdle, PAD);
    }
    sHave[0] = sHave[1] = (uint32_t)sDelay + 1;
    if (sMode == 3) {
        fprintf(stderr, "bt3: net: spectating \"%s\" vs \"%s\": the host has %u blanks of the match; game speed %d%%\n", sNames[0], sNames[1], (unsigned)sSpecHead,
                rules_speed());
        sRollMax = 0;
        sStartedMs = now_ms();
        spec_ack_send(0);
        sSpecOpen = Lobby_ServesSpectators(); /* (a lobby's owner that watches: it passes the match on from here) */
        return;
    }
    /* (the host: from here its choices are final, and spectators are let in; a lobby's owner that joined passes the
       match on to the lobby's spectators) */
    sSpecOpen = sMode == 1 || Lobby_ServesSpectators();
    fprintf(stderr, "bt3: net: connected%s; input delay %d blanks, %s\n", sFromLobby && Port_LobbyRelayed() ? " through the relay" : sFromLobby ? " directly" : "", sDelay,
            sRollMax > 0 ? "rollback" : "waiting for each other");
    fprintf(stderr, "bt3: net: game speed %d%%\n", rules_speed());
    if (sOpts[0] != 0 && sOpts[1] != 0) {
        fprintf(stderr, "bt3: net: each player's own options: player 1 camera %u shake %u, player 2 camera %u shake %u\n", (unsigned)(sOpts[0] & 0xFF),
                (unsigned)(sOpts[0] >> 8 & 0xFF), (unsigned)(sOpts[1] & 0xFF), (unsigned)(sOpts[1] >> 8 & 0xFF));
    } else {
        fprintf(stderr, "bt3: net: the other copy does not send its player's options (0.1.25 or earlier): both sides use the defaults\n");
    }
    if (sRollMax > 0) {
        fprintf(stderr, "bt3: net: rollback of up to %d blanks\n", sRollMax);
    }
    sStartedMs = now_ms();
    Port_TestSlow(1, 0);
}

int Port_NetActive(void);

/* The voice language the player chose in the game's options, read from their own save (bit 0 of the flags word at
   0x1608: set = the first choice of the menu, which is what a new save has; clear = the second voice set). An
   online match starts from an empty save on both sides, so both compute with the default; this choice is kept
   aside for what this copy plays (BT3_NET_VOICE=alt; snd_adx.c, snd_se.c). 1 = the second set. */
static int own_voice_alt(void) {
    const char *root = getenv("BT3_SAVES") != NULL ? getenv("BT3_SAVES") : "saves";
    char path[600];
    unsigned char w[4];
    FILE *fp;
    int alt = 0;

    snprintf(path, sizeof(path), "%s/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3", root);
    fp = fopen(path, "rb");
    if (fp != NULL) {
        if (fseek(fp, 0x1608, SEEK_SET) == 0 && fread(w, 1, 4, fp) == 4) {
            alt = !(w[0] & 1);
        }
        fclose(fp);
    }
    return alt;
}

/* Whether this copy plays the second voice set in the online match it is in (the player's own choice, above). */
static int sVoiceAlt = -1; /* 1 the second set, 0 the default; -1 not decided (then BT3_NET_VOICE says, once) */

int Port_NetVoiceAlt(void) {
    /* (Not asked of the environment every time: the port's getenv remembers its first answer at each place it is
       called from (gs_internal.h), and a match that begins without the program starting again sets the variable
       after that first look. Found on Windows, 2026-10-09: the choice was never seen. The match's start sets
       sVoiceAlt itself; the variable is for the program that starts again as the session, and for tests.) */
    if (sVoiceAlt < 0) {
        const char *v = getenv("BT3_NET_VOICE");
        sVoiceAlt = v != NULL && strcmp(v, "alt") == 0;
    }
    return Port_NetActive() && sVoiceAlt == 1;
}

/* The players' names (the lobby window's "Player name"), for the line under each side's gauges: this copy's own
   goes out with its greeting (T_HELLO, the one who joins) or its answer and choices (T_HELLO_ACK, T_CONFIG, the
   host), 20 bytes after the words those packets had. */

/* The name of the player of side 0 or 1 in the match this copy is in ("" if it is not known). */
const char *Port_NetName(int player) {
    return Port_NetActive() && (player == 0 || player == 1) ? sNames[player] : "";
}

/* Whether this copy shows the two sides of the fight's HUD in each other's places: the player who joined is side 1,
   and with this their own gauges are on the left, as the host's are for the host. Setting net_hud_own (on by
   default); BT3_HUD_SWAP=1 the same without a connection, for looking at it. Only this copy's picture changes. */
int Port_HudSwap(void) {
    if(TagNet_Enabled()) return 0;
    static int forced = -1, pref = -1;
    extern int Port_Setting(const char *name, int def);
    if (forced < 0) {
        forced = getenv("BT3_HUD_SWAP") != NULL ? atoi(getenv("BT3_HUD_SWAP")) != 0 : 0;
        pref = Port_Setting("net_hud_own", 1) != 0;
    }
    return forced || (pref && Port_NetActive() && sMe == 1);
}

/* Which player this copy plays in an online match (0 the host, 1 the one who joined). */
int Port_NetMe(void) {
    if(TagNet_Enabled()) return TagNet_Role();
    return sMe;
}

int Port_NetActive(void) {
    if(TagNet_Enabled()) return TagNet_Active();
    if (sMode < 0) { /* first asked: connected from the start if the environment says so */
        const char *host = getenv("BT3_NET_HOST"), *join = getenv("BT3_NET_JOIN"), *watch = getenv("BT3_NET_SPECTATE");
        sMode = 0;
        if (host != NULL || join != NULL || watch != NULL) {
            sSession = getenv("BT3_NET_SESSION") != NULL;
            net_start(host != NULL ? 1 : join != NULL ? 2 : 3, host, join != NULL ? join : watch);
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

#define QUIET_NS 6000000000ull /* this long without the input the game is waiting for: the other player is gone */

static int sEndedThere; /* the session ends because of the other side (or a disagreement), not this player's choice */

static void ended(const char *why, unsigned tick) {
    fprintf(stderr, "bt3: net: %s (blank %u)\n", why, tick);
    sEndedThere = 1;
    Port_UiNotice(why);
    sSpecNoticed = 1;
    if (sSession) {
        Port_NetLeave(); /* back to the game as it normally is */
    }
    exit(2);
}

/* The other copy said goodbye. */
static void gone(unsigned tick) {
    if (sGone >= 3) { /* a lobby's: back to the lobby, the lobby closed, or this player left it (net_lobby.c) */
        ended(sGoneNote[0] != '\0' ? sGoneNote : "Back to the lobby.", tick);
    }
    if (sGone == 1 && Lobby_Active()) {
        /* a lobby's match: the other copy left the match on its own, which is the owner's way back (the menus) or
           its leaving the lobby (then the owner's word of it follows) */
        ended("Back to the lobby.", tick);
    }
    if (sByeWhy == 0) {
        sByeWhy = BYE_LEFT; /* (this game leaves because the other one did) */
    }
    ended(sGone == 2 ? "The two games no longer agree (desync): the match has ended." :
          sByeHeard == BYE_QUIT ? "The other player's game was closed: the match has ended." :
          sByeHeard == BYE_LOST ? "The other player's game lost contact with yours (a connection problem on one side or between you): the match has ended." :
          "The other player left the match.", tick);
}

static void gone_quiet(unsigned tick) {
    sByeWhy = BYE_LOST; /* (said to the other side on the way out, in case it still hears this one) */
    ended("The connection to the other player was lost.", tick);
}

/* The program ends (its window closed, the game quit: atexit) in a match: the other game is told so at once, instead
   of finding out from the silence after QUIET_NS. */
static void bye_at_exit(void) {
    uint32_t bye[3] = {MAGIC, T_BYE, sByeWhy != 0 ? sByeWhy : BYE_QUIT}; /* (a match that ended for a reason of its own says that one) */
    int k;
    if (sByeSent || sMode == 3 || sSock < 0 || !sConnected) {
        return;
    }
    sByeSent = 1;
    for (k = 0; k < 3; k++) {
        net_send(bye, sizeof(bye), &sPeer);
    }
    fprintf(stderr, "bt3: net: the program ends: the other player is told\n");
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
    sync_note(tick); /* (the game as this blank finds it, whether the blank is new or run again) */
    if (sResim && tick >= sLive) {
        extern void PortSe_RunAgainEnds(unsigned first, unsigned live); /* snd_se.c */
        sResim = 0; /* caught up: this blank is the present again */
        PortSe_RunAgainEnds(sResimFrom, sLive);
    }
    if (!sResim) {
        extern void PortSe_BlankBegins(unsigned tick); /* snd_se.c: which effects this blank starts */
        PortSe_BlankBegins(tick);
        Port_TestSlow(2, 0);
        sample_local(tick);
        send_inputs();
        receive();
        if (sGone) {
            gone(tick);
        }
        t0 = last = SDL_GetTicksNS();
        if (tick >= sHave[other] + (uint32_t)sRollMax) {
            WaitMark w = wait_begin(t0);
            sStalls++;
            sSecWaits++;
            while (tick >= sHave[other] + (uint32_t)sRollMax) {
                uint64_t now = SDL_GetTicksNS();
                if (now - last > 4000000ull) {
                    send_inputs();
                    last = now;
                }
                if (!receive()) {
                    SDL_DelayNS(200000);
                }
                if (sGone) {
                    gone(tick);
                }
                if (now - t0 > QUIET_NS) {
                    gone_quiet(tick);
                }
            }
            wait_end(w, tick);
        }
        {   /* (see sTheirAhead) */
            int diff = (int)sMyAhead - (int)sTheirAhead; /* (both taken at the same point of a blank: when sending) */
            if (getenv("BT3_NET_SKEW") != NULL && tick == 1500) { /* testing: this copy falls behind by that many ms, once */
                gPortPaceShiftNs = (unsigned)atoi(getenv("BT3_NET_SKEW")) * 1000000u;
            }
            sBalance += (diff * 16 - sBalance) / 16;
            if (sBalance >= 24 && getenv("BT3_NET_NOBALANCE") == NULL) { /* a blank and a half apart or more: a millisecond later */
                gPortPaceShiftNs += 1000000;
                sSlowed++;
            }
        }
        sSecWaitMs += (unsigned)((SDL_GetTicksNS() - t0) / 1000000ull);
        if (++sSecTicks >= 60) { /* a second of blanks: what the meter shows next */
            sShown[1] = (int)sSecRoll;
            sShown[2] = sSecRoll != 0 ? (int)(sSecRollTicks * 10 / sSecRoll) : 0;
            sShown[3] = (int)sSecWaits;
            sShown[4] = (int)sSecWaitMs;
            sShown[5] = sSecWaitMaxNs >= 30000000ull ? sSecWaitWhy : 0;
            sSecRoll = sSecRollTicks = sSecWaits = sSecWaitMs = sSecTicks = 0;
            sSecWaitMaxNs = 0;
        }
        limit = sHave[other] < tick ? sHave[other] : tick;
        for (t = sChecked; t < limit && memcmp(sUsed[t % RING], sIn[other][t % RING], PAD) == 0; t++) {
        }
        sChecked = t;
        if (t < limit) { /* blank t was given something else than what they really pressed */
            sLive = tick;
            sResimFrom = t;
            sResim = 1;
            sRollbacks++;
            sRollTicks += tick - t;
            sSecRoll++;
            sSecRollTicks += tick - t;
            gPortResim = 1;
            Port_RollBack((int)(tick - t)); /* does not return: goes on after blank t's Port_RollSave below */
        }
        sync_check(sChecked, tick); /* (every blank below sChecked was run with their real input) */
        if (tick % 600 == 0 && getenv("BT3_GS_VERBOSE") != NULL) {
            fprintf(stderr, "net: blank %u: %u rollbacks (%.1f blanks each), %u waits in the last 600 blanks; ahead %d, they %d, slowed %u ms; %u checks against the other copy\n", tick, sRollbacks,
                    sRollbacks != 0 ? (double)sRollTicks / sRollbacks : 0.0, sStalls, (int)sMyAhead, (int)sTheirAhead, sSlowed, sSyncCompared);
            sRollbacks = sRollTicks = sStalls = sSlowed = sSyncCompared = 0;
        }
    }
    if (Port_RollSave() != 0) {
        sTick = tick; /* back at this blank, to run from here again */
    }
    gPortResim = sResim || Port_NetWarp();
    memcpy(sUsed[tick % RING], sIn[other][(tick < sHave[other] ? tick : sHave[other] - 1) % RING], PAD);
}

/* A quick match: the other player's region as the matchmaking service placed it ("EU"; "" not known), for the
   lines that say the rival is there; kept from the moment the two are known to the end of their session. */
static char sRivalRegion[4];

/* A region's name in the chosen language (the port catalog's region/XX), "" for none. */
const char *Port_LobbyRegionName(const char *code) {
    static const char *const codes[] = {"NA", "SA", "EU", "ME", "AF", "AS", "OC"};
    static const char *const names[] = {"North America", "South America", "Europe", "the Middle East", "Africa", "Asia", "Oceania"};
    char id[16];
    const char *t;
    int i;

    for (i = 0; i < 7 && (code == NULL || strcmp(code, codes[i]) != 0); i++) {
    }
    if (i == 7) {
        return "";
    }
    snprintf(id, sizeof(id), "region/%s", codes[i]);
    t = MenuTranslation_PortText(id);
    return t != NULL && t[0] != '\0' ? t : names[i];
}

/* The rival's region's name ("" not known: not a quick match, or an older service or game). */
const char *Port_LobbyRivalRegion(void) {
    return Port_LobbyRegionName(sRivalRegion);
}

/* Puts `line` into `text` with {name}, {region} and {ms} replaced. */
static void banner_fill(char *text, size_t size, const char *line, const char *name, const char *region, int ms) {
    size_t i, k, n;
    char num[16];

    snprintf(num, sizeof(num), "%d", ms);
    for (i = n = 0; line[i] != '\0' && n + 1 < size; i++) {
        const char *put = strncmp(line + i, "{name}", 6) == 0 ? name : strncmp(line + i, "{region}", 8) == 0 ? region : strncmp(line + i, "{ms}", 4) == 0 ? num : NULL;
        if (put != NULL) {
            for (k = 0; put[k] != '\0' && n + 1 < size; k++) {
                text[n++] = put[k];
            }
            i += put == name ? 5 : put == region ? 7 : 3;
        } else {
            text[n++] = line[i];
        }
    }
    text[n] = '\0';
}

/* What the screen says while a session starts (the picture stands still meanwhile: Gs_HoldFrame keeps showing it
   with this line on it). Before the other player's name is known: that their game is being reached; then that they
   are there, by name. The lines are the port catalog's (net/connecting, net/rival_here; {name} is replaced). */
static void session_banner(void) {
    const char *other = sNames[!sMe], *line, *region = Port_LobbyRivalRegion();
    char text[256], said[sizeof(sBannerSaid)], name[NAME_LEN];
    size_t i, k;
    int ms = sPingMs >= 0 ? sPingMs : sSetupRtt;

    if (!sSession) {
        return;
    }
    for (i = k = 0; other[i] != '\0' && k + 1 < sizeof(name); i++) { /* (the other side's text: printable only) */
        if ((unsigned char)other[i] >= 0x20 && other[i] != 0x7F) {
            name[k++] = other[i];
        }
    }
    name[k] = '\0';
    /* (said again when the name comes, and once more when the round trip is first known: it then stays as it is) */
    snprintf(said, sizeof(said), "%c%s", k == 0 ? '\x01' : ms < 0 ? '\x02' : '\x03', name);
    if (strcmp(sBannerSaid, said) == 0 || (sBannerSaid[0] == '\x03' && k != 0)) {
        return;
    }
    snprintf(sBannerSaid, sizeof(sBannerSaid), "%s", said);
    if (k == 0) {
        line = MenuTranslation_PortText("net/connecting");
        Port_UiBanner(line != NULL ? line : "Connecting to your rival...", 0);
        return;
    }
    if (region[0] != '\0') { /* a quick match: where the rival is */
        line = MenuTranslation_PortText("net/rival_from");
        if (line == NULL) {
            line = "Your rival \"{name}\" from {region} is here! The match begins.";
        }
    } else {
        line = MenuTranslation_PortText("net/rival_here");
        if (line == NULL) {
            line = "Your rival \"{name}\" is here! The match begins.";
        }
    }
    banner_fill(text, sizeof(text), line, name, region, ms);
    if (ms >= 0) { /* the round trip, as measured once the two are connected */
        char more[96];
        line = MenuTranslation_PortText("net/round_trip");
        banner_fill(more, sizeof(more), line != NULL ? line : "Round trip: ~{ms} ms", name, region, ms);
        k = strlen(text);
        snprintf(text + k, sizeof(text) - k, "\n%s", more);
    }
    Port_UiBanner(text, 0);
}

/* While the game's thread waits for the other side during a session's start: the line, and the picture. */
static void session_hold(void) {
    if (sSession) {
        session_banner();
        Gs_HoldFrame();
    }
}

/* A blank of a session's start-up (the logos and the card check, without picture, up to the character select:
   Port_NetWarp). Nobody plays it, so both copies give it nothing pressed for both players and go on without
   waiting for each other: the two run the same blanks from the same first blank with the same input and arrive at
   the character select in the same state, at their own speed (it was the round trip per blank or two: half a
   minute at 100 ms). The check values still travel (with a packet a tenth of a second, which also keeps the line
   open) and are compared as always. */
static void warp_tick(unsigned tick) {
    static uint64_t last;
    uint64_t now = SDL_GetTicksNS();
    uint32_t at = (uint32_t)tick + (uint32_t)sDelay, k;

    for (k = 0; k < 2; k++) {
        while (sHave[k] <= at) {
            memcpy(sIn[k][sHave[k] % RING], kIdle, PAD);
            sHave[k]++;
        }
    }
    memcpy(sUsed[tick % RING], kIdle, PAD); /* (what the rollback's reading of the other pad gives) */
    sync_note(tick);
    sync_check(tick, tick);
    if (now - last > 100000000ull) {
        send_inputs();
        last = now;
    }
    receive();
    if (sGone) {
        gone(tick);
    }
    session_hold();
    gPortResim = 1;
}

/* The first blank after the start-up: waits for the other copy to have arrived too (a slower machine may take a
   while: a minute is allowed), and both must have arrived at the same blank, or the two games are not the same. */
static void meet(unsigned tick) {
    uint64_t t0 = SDL_GetTicksNS(), last = 0;

    sMet = 1;
    sMyArrival = tick;
    do {
        uint64_t now = SDL_GetTicksNS();
        if (last == 0 || now - last > 50000000ull) {
            uint32_t hello[4] = {MAGIC, T_ARRIVED, tick, 0};
            send_to_peer(hello, (int)sizeof(hello));
            last = now;
        }
        if (sTheirArrival != 0) {
            break;
        }
        if (!receive()) {
            SDL_DelayNS(200000);
        }
        if (sGone) {
            gone(tick);
        }
        if (now - t0 > 60000000000ull) {
            gone_quiet(tick);
        }
        session_hold();
    } while (sTheirArrival == 0);
    session_banner();
    Port_UiBanner(NULL, 2500); /* (the line stays a little over the character select, without the dimming) */
    fprintf(stderr, "bt3: net: both at the character select (blank %u), %u ms after connecting\n", tick, (unsigned)(now_ms() - sStartedMs));
    if (sTheirArrival != tick) {
        fprintf(stderr, "bt3: net: the other copy reached the character select at blank %u\n", (unsigned)sTheirArrival);
        sByeWhy = BYE_APART;
        ended("The two games no longer agree (desync): the match has ended.", tick);
    }
}

/* The spectator: looks at the socket, and tells the host what it has (every 50 ms). */
static void spec_service(void) {
    static uint64_t last;
    uint64_t now = SDL_GetTicksNS();
    if (now - last > 1000000000ull) {
        sSpecHeardNs = now; /* (this copy was away a while, loading: the host's silence meanwhile says nothing) */
    }
    last = now;
    receive();
    if (SDL_GetTicksNS() - sSpecAckNs > 50000000ull) {
        spec_ack_send(0);
    }
}

/* The spectator's line in the middle while it waits or catches up, with the last picture (at most 10 a second). */
static void spec_hold(const char *line) {
    uint64_t now = SDL_GetTicksNS();
    if (now - sSpecDrawNs >= 100000000ull) {
        sSpecDrawNs = now;
        Port_UiBanner(line, 0);
        Gs_HoldFrame();
    }
}

/* 1 while a spectator runs blanks without picture and not held to real time, to catch up (plat_stub.c). */
int Port_NetSpecSkip(void) {
    return sMode == 3 && (sSpecSkip || sSpecNudge);
}

/* A blank of the spectator. Its input is the host's (Port_NetInput); the start-up runs as the players' does
   (Port_NetWarp: nothing pressed, no picture). It keeps about SPEC_BUFFER blanks of the host's in hand: with none
   left it waits until it has that many again; with more than SPEC_BUFFER + 60 it runs without picture until it is
   back at SPEC_BUFFER. */
static void spec_tick(unsigned tick) {
    uint64_t now = SDL_GetTicksNS();

    if (tick % SYNC_EVERY == 0 && tick != 0) { /* its own check value, compared with the host's for the same blank */
        uint64_t had;
        if (!note_get(&sNotesOwn, tick, &had)) {
            uint64_t v = Port_NetDigest();
            static int falsed;
            if (getenv("BT3_TEST_SPEC_APART") != NULL && tick >= (unsigned)atoi(getenv("BT3_TEST_SPEC_APART")) && !falsed) {
                falsed = 1; /* testing: this spectator claims another game than it has, once in the run */
                v ^= 1;
            }
            note_put(&sNotesOwn, tick, v);
            spec_compare(tick);
        }
    }
    spec_service();
    if (getenv("BT3_SPEC_VIEWS") != NULL) { /* testing: BT3_SPEC_VIEWS=<blank>:<view>|...: the view changes at those blanks */
        const char *p = getenv("BT3_SPEC_VIEWS");
        char *after;
        while (*p != '\0') {
            unsigned at = (unsigned)strtoul(p, &after, 10);
            if (after == p || *after != ':') {
                break;
            }
            if (at == tick) {
                sSpecView = atoi(after + 1) % 3;
                fprintf(stderr, "bt3: net: spectating: view %d from blank %u\n", sSpecView, tick);
            }
            p = after + 1;
            while (*p >= '0' && *p <= '9') {
                p++;
            }
            while (*p != '\0' && (*p < '0' || *p > '9')) {
                p++;
            }
        }
    }
    if (getenv("BT3_SPEC_LEAVE") != NULL && tick == (unsigned)atoi(getenv("BT3_SPEC_LEAVE"))) { /* testing: leaves at that blank */
        fprintf(stderr, "bt3: net: spectating: leaving at blank %u (BT3_SPEC_LEAVE)\n", tick);
        Port_NetLeave();
    }
    if (sGone >= 3) {
        gone(tick);
    }
    if (sSpecEnded) {
        ended(Lobby_Active() ? "Back to the lobby." : "The players' match has ended.", tick);
    }
    if (sSpecLeaveAsked) {
        sSpecLeaveAsked = 0;
        sSpecNoticed = 1;
        fprintf(stderr, "bt3: net: spectating: stopped watching at blank %u\n", tick);
        Port_NetLeave();
    }
    now = SDL_GetTicksNS(); /* (after the look at the socket: what came meanwhile is later than `now` was) */
    if (now > sSpecHeardNs && now - sSpecHeardNs > SPEC_QUIET_NS) {
        ended("The connection to the host was lost.", tick);
    }
    if (sSpecRejoin) {
        fprintf(stderr, "bt3: net: spectating: following the match again from the start\n");
        Port_NetLeave(); /* (Port_LobbyAuto asks for the session again) */
    }
    if (Port_NetWarp()) {
        spec_hold("Joining the match as a spectator...");
        gPortResim = 1; /* (as warp_tick: the start-up's blanks are not shown) */
        return;
    }
    if (!sMet) {
        sMet = 1;
        Port_UiBanner(NULL, 0); /* (the start-up's line, "Joining the match as a spectator...", is done) */
        fprintf(stderr, "bt3: net: spectating: at the character select (blank %u), %u ms after connecting; the host has %u blanks\n", tick,
                (unsigned)(now_ms() - sStartedMs), (unsigned)sLogN);
    }
    sSpecNudge = 0;
    if (tick >= sLogN) { /* nothing to run this blank with: wait until it has its lead again, a longer one from now */
        uint64_t t0 = now;
        int said = 0;
        if (tick > 1800) { /* (not the first wait, at the character select: the host's blanks were only beginning) */
            sSpecLead = sSpecLead + SPEC_LEAD_STEP < SPEC_BUFFER ? sSpecLead + SPEC_LEAD_STEP : SPEC_BUFFER;
        }
        sSpecLeadSince = tick;
        sSpecSkip = 0;
        while (sLogN < tick + (unsigned)sSpecLead && !(sLogN > tick && sSpecHead <= sLogN && SDL_GetTicksNS() - t0 > 2000000000ull)) {
            uint64_t at = SDL_GetTicksNS();
            spec_service();
            if (sGone >= 3) {
                gone(tick);
            }
            if (sSpecEnded) {
                ended(Lobby_Active() ? "Back to the lobby." : "The players' match has ended.", tick);
            }
            if (at > sSpecHeardNs && at - sSpecHeardNs > SPEC_QUIET_NS) {
                ended("The connection to the host was lost.", tick);
            }
            if (at - t0 > 250000000ull) {
                said = 1;
                spec_hold("Waiting for the match...");
            }
            SDL_DelayNS(500000);
        }
        if (said) {
            Port_UiBanner(NULL, 0);
            fprintf(stderr, "bt3: net: spectating: waited %u ms for the host's blanks at blank %u (running %d blanks behind from now)\n",
                    (unsigned)((SDL_GetTicksNS() - t0) / 1000000ull), tick, sSpecLead);
        }
    }
    if (tick - sSpecLeadSince > SPEC_LEAD_QUIET && sSpecLead > SPEC_LEAD_MIN) { /* a quiet while: a little closer again */
        sSpecLead -= 2;
        sSpecLeadSince = tick;
    }
    if (!sSpecSkip && sLogN > tick + (unsigned)sSpecLead + 60) {
        sSpecSkip = 1;
        sSpecSkipped = 0;
        fprintf(stderr, "bt3: net: spectating: %u blanks behind the host at blank %u: catching up without picture\n", (unsigned)(sLogN - tick), tick);
    } else if (sSpecSkip && sLogN <= tick + (unsigned)sSpecLead) {
        sSpecSkip = 0;
        sSpecDrawNs = 0;
        Port_UiBanner(NULL, 0);
        fprintf(stderr, "bt3: net: spectating: caught up at blank %u (%u blanks run without picture)\n", tick, sSpecSkipped);
    }
    if (tick >= sSpecLeadLog + 1800) { /* (the log: how far behind the host this copy runs) */
        sSpecLeadLog = tick;
        fprintf(stderr, "bt3: net: spectating: blank %u, %u blanks behind the host's latest (lead %d)\n", tick, (unsigned)(sLogN - tick), sSpecLead);
    }
    if (!sSpecSkip && sLogN > tick + (unsigned)sSpecLead + 12 && tick % 16 == 0) {
        sSpecNudge = 1; /* a little behind: one blank in sixteen runs without picture (gently back to the lead) */
    }
    if (sSpecSkip) {
        char line[96];
        sSpecSkipped++;
        snprintf(line, sizeof(line), "Catching up with the match... %u%%", sSpecHead != 0 ? (unsigned)((uint64_t)tick * 100 / sSpecHead) : 0u);
        spec_hold(line);
    }
}

void Port_NetBeginTick(unsigned tick) {
    if(TagNet_Enabled()) {TagNet_Tick(tick);return;}
    uint64_t t0, last;

    if (!Port_NetActive()) {
        return;
    }
    sTick = tick;
    if (sMode == 3) {
        spec_tick(tick);
        return;
    }
    if (Port_NetWarp()) {
        warp_tick(tick);
        return;
    }
    if (Port_NetSession() && !sMet) {
        meet(tick);
    }
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
    if (sGone >= 3) {
        gone(tick);
    }
    t0 = last = SDL_GetTicksNS();
    if (sHave[!sMe] <= tick) {
        WaitMark w = wait_begin(t0);
        while (sHave[!sMe] <= tick) {
            uint64_t now = SDL_GetTicksNS();
            if (now - last > 4000000ull) {
                send_inputs(); /* the other side may be waiting for a packet that was lost */
                last = now;
            }
            if (!receive()) {
                SDL_DelayNS(200000);
            }
            if (sGone) {
                gone(tick);
            }
            if (now - t0 > QUIET_NS) {
                gone_quiet(tick);
            }
        }
        wait_end(w, tick);
    }
    sWaitNs += SDL_GetTicksNS() - t0;
    sWaits++;
    sync_note(tick); /* (nothing is guessed here: every blank is sure when it begins) */
    sync_check(tick, tick);
    {   /* (the meter, when the two copies wait for each other every blank: time waited, and blanks that waited
           more than a millisecond) */
        unsigned ms = (unsigned)((SDL_GetTicksNS() - t0) / 1000000ull);
        sSecWaitMs += ms;
        sSecWaits += ms >= 1;
        if (++sSecTicks >= 60) {
            sShown[1] = sShown[2] = 0;
            sShown[3] = (int)sSecWaits;
            sShown[4] = (int)sSecWaitMs;
            sShown[5] = sSecWaitMaxNs >= 30000000ull ? sSecWaitWhy : 0;
            sSecWaits = sSecWaitMs = sSecTicks = 0;
            sSecWaitMaxNs = 0;
        }
    }
    if (tick % 600 == 0 && getenv("BT3_GS_VERBOSE") != NULL) {
        fprintf(stderr, "net: blank %u: waited for the other player %.2f ms per blank on average; %u checks against the other copy\n", tick, (double)sWaitNs / (double)sWaits / 1e6,
                sSyncCompared);
        sWaitNs = sWaits = 0;
        sSyncCompared = 0;
    }
}

/* What the game reads for a pad during this blank. */
void Port_NetInput(int player, unsigned char *data) {
    if(TagNet_Enabled()) {TagNet_Input(player,data);return;}
    if (sMode == 3) { /* a spectator: both players' input as the host has it */
        memcpy(data, player >= 0 && player < 2 && sTick < sLogN ? sLog + ((size_t)sTick * 2 + (size_t)player) * PAD : kIdle, PAD);
        return;
    }
    if (sRollMax > 0 && player == !sMe) {
        memcpy(data, sUsed[sTick % RING], PAD); /* their real input, or the guess (roll_tick) */
        return;
    }
    memcpy(data, player >= 0 && player < 2 && sTick < sHave[player] ? sIn[player][sTick % RING] : kIdle, PAD);
}

int Port_NetResim(void) {
    if(TagNet_Enabled()) return TagNet_Resim();
    return sResim;
}

/* The blank the game is in (run for the first time or again). */
unsigned Port_NetTick(void) {
    return sTick;
}

/* Which player's view this copy shows full screen in a two-player battle: the local player's when online; -1
   otherwise (the two views side by side, as always). BT3_VIEW=0|1: the same without a connection, for looking at
   it and for comparing the game's state between the three ways of showing a battle. */
int Port_NetView(void) {
    if(TagNet_Enabled()) return TagNet_Role()<TN_PLAYERS?TagNet_Role():-1;
    static int forced = -2;
    if (forced == -2) {
        const char *e = getenv("BT3_VIEW");
        forced = e != NULL ? atoi(e) : -1;
    }
    if (forced >= 0) {
        return forced & 1;
    }
    return Port_NetActive() ? (sMode == 3 ? (sSpecView == 2 ? -1 : sSpecView) : sMe) : -1;
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

/* The session's own save folder: net_session beside the program's data (BT3_NET_SAVES=<folder>, testing: another,
   for several copies started in one folder). */
static const char *session_saves(void) {
    const char *e = getenv("BT3_NET_SAVES");
    return e != NULL && e[0] != '\0' ? e : "net_session";
}

static void session_saves_empty(void) {
    static const char *const kFiles[] = {"BASLUS-21678DBZT3", "icon.sys", "dbzsm.ico"};
    char path[600];
    int i;
    for (i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/card1/BASLUS-21678DBZT3/%s", session_saves(), kFiles[i]);
        remove(path);
    }
}

static void relaunch(int role, const char *join, int port) {
    char value[160];
#ifdef _WIN32
    char exe[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
#define SETENV(n, v) SetEnvironmentVariableA(n, v)
#else
#ifdef __APPLE__
    char exe[1024];
#endif
#define SETENV(n, v) ((v) != NULL ? setenv(n, v, 1) : unsetenv(n))
#endif
    SETENV("BT3_NET_HOST", NULL);
    SETENV("BT3_NET_JOIN", NULL);
    SETENV("BT3_NET_SPECTATE", NULL);
    SETENV("BT3_NET_SESSION", NULL);
    SETENV("BT3_SOUND_TICKS", NULL);
    if (role != 0) {
        if (role == 1) {
            snprintf(value, sizeof(value), "%d", port);
            SETENV("BT3_NET_HOST", value);
        } else {
            snprintf(value, sizeof(value), "%s:%d", join, port);
            SETENV(role == 3 ? "BT3_NET_SPECTATE" : "BT3_NET_JOIN", value);
        }
        SETENV("BT3_NET_SESSION", "1");
        SETENV("BT3_NET_NAME", sMyName);
        {   /* a match found with a room code: the lobby's local port, and for the host where to go on knocking */
            char text[64] = "";
            snprintf(value, sizeof(value), "%d", sCodeLocalPort);
            SETENV("BT3_NET_LOCALPORT", sCodeLocalPort != 0 ? value : NULL);
            if (sCodePunchSet) {
                inet_ntop(AF_INET, &sCodePunch.sin_addr, text, sizeof(text));
                snprintf(value, sizeof(value), "%s:%d", text, ntohs(sCodePunch.sin_port));
            }
            SETENV("BT3_NET_PUNCH", sCodePunchSet ? value : NULL);
        }
        if (role == 1) {
            snprintf(value, sizeof(value), "%u", (unsigned)sRules);
            SETENV("BT3_NET_RULES", value);
        } else {
            SETENV("BT3_NET_RULES", NULL);
        }
        SETENV("BT3_SOUND_TICKS", "1");
        SETENV("BT3_NOMOVIE", "1");
        if (getenv("BT3_NET_VOICE") == NULL) { /* (before the save folder changes: it is read from the player's own) */
            SETENV("BT3_NET_VOICE", own_voice_alt() ? "alt" : "default");
        }
        if (getenv("BT3_NET_OPTS") == NULL) { /* (the same for the player's camera type and screen shake) */
            snprintf(value, sizeof(value), "%u", (unsigned)(own_options() & 0xFFFFu));
            SETENV("BT3_NET_OPTS", value);
        }
        SETENV("BT3_SAVES", session_saves());
        /* the session's own save folder starts empty: both sides begin from the game's defaults */
        session_saves_empty();
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
#elif defined(__APPLE__)
    {
        extern int _NSGetExecutablePath(char *buf, uint32_t *size); /* <mach-o/dyld.h> */
        uint32_t cap = sizeof(exe) - 1;
        if (_NSGetExecutablePath(exe, &cap) == 0) {
            char *argv[2] = {exe, NULL};
            execv(exe, argv);
        }
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

/* Restart after changing the menu language, keeping the player's launch environment. */
void Port_Restart(const char *language) {
    const char *settings = getenv("BT3_SETTINGS");
    /* With settings disabled, carry this run's choice through the restart. */
    if (settings != NULL && settings[0] == '\0' && language != NULL) {
#ifdef _WIN32
        SetEnvironmentVariableA("BT3_MENU_LANG", language);
        _putenv_s("BT3_MENU_LANG", language);
#else
        setenv("BT3_MENU_LANG", language, 1);
#endif
    }
    fflush(NULL);
#ifdef _WIN32
    char exe[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    GetModuleFileNameA(NULL, exe, sizeof(exe) - 1);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (CreateProcessA(exe, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) ExitProcess(0);
#elif defined(__APPLE__)
    char exe[1024];
    uint32_t cap = sizeof(exe) - 1;
    extern int _NSGetExecutablePath(char *buf, uint32_t *size);
    if (_NSGetExecutablePath(exe, &cap) == 0) { char *argv[2] = {exe, NULL}; execv(exe, argv); }
#else
    char *argv[2] = {(char *)"/proc/self/exe", NULL};
    execv("/proc/self/exe", argv);
#endif
    fprintf(stderr, "bt3: language: could not restart the program\n");
    exit(2);
}

/* 1 in a session started from Dragon Net Battle. */
int Port_NetSession(void) {
    return Port_NetActive() && sSession;
}

/* For the game's start-up (src/menu/menu_i_c.c, menu_a_c.c): 1 when it has to be the game's own whatever the
   player's settings say, which is in every session. BT3_TEST_OWN_START=1 (testing the check above): this copy
   follows its settings in a session too, as 0.1.32 and 0.1.33 did. */
int Port_NetFixedStart(void) {
    return Port_NetSession() && getenv("BT3_TEST_OWN_START") == NULL;
}

/* The session without starting the program again (gs/state.c exchanges the game's state; 64-bit programs).
   Begin: called on the game's own thread just before the game is put back to its first moment. */
extern int Port_SessionCan(void);                                    /* gs/state.c */
extern void Port_SessionRequest(int role, const char *address, int port);
extern void Port_SessionReturn(void);
extern int Port_SessionAsked(void);
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
    session_saves_empty();
    sVoiceAlt = own_voice_alt(); /* (before the save folder changes: it is read from the player's own) */
    put_env("BT3_SAVES", session_saves());
    snprintf(host, sizeof(host), "%d", port);
    snprintf(join, sizeof(join), "%s:%d", address, port);
    if (sSock >= 0) {
        closesocket(sSock);
        sSock = -1;
    }
    sSession = 1;
    sGoPending = 0;
    sArrived = 0;
    sMyArrival = sTheirArrival = 0;
    sMet = 0;
    sBannerSaid[0] = 0;
    net_start(role, role == 1 ? host : NULL, role >= 2 ? join : NULL);
}

/* How many sessions have ended in this run. The port's own memory: the game's is put back to the moment the session
   was asked for, so this is how the screens that asked for it (menu/port_tbo.c) know it has been and gone. */
static int sSessionsEnded;

int Port_NetSessionsEnded(void) {
    return sSessionsEnded;
}

void Port_NetSessionEnd(void) {
    sSessionsEnded++;
    sRivalRegion[0] = '\0';
    if (Lobby_SessionEnded()) { /* a lobby's match: the lobby goes on, with its socket, its relay and its room code */
        sMode = 0;
        sSession = 0;
        sConnected = 0;
        PortChat_Reset();
        put_env("BT3_SAVES", sSavesWasSet ? sSavesBefore : NULL);
        return;
    }
    Match_Cancel(); /* (a host's room kept for spectators is given up) */
    Relay_Close(sSock);
    if (sSock >= 0) {
        closesocket(sSock);
        sSock = -1;
    }
    sMode = 0;
    sSession = 0;
    PortChat_Reset();
    put_env("BT3_SAVES", sSavesWasSet ? sSavesBefore : NULL);
}

/* The session's start-up runs without picture and sound and without waiting for real time until the game has
   reached the versus menu (headless.c says when). */
int Port_NetWarp(void) {
    return Port_NetSession() && !sArrived;
}
void Port_NetArrived(void) {
    sArrived = 1;
    fprintf(stderr, "bt3: net: the start-up is over at blank %u, %u ms after connecting: the character select\n", (unsigned)sTick, (unsigned)(now_ms() - sStartedMs));
}

/* The session is over (the game went back to the main menu, or the other side is gone): back to the game as it
   normally is. */
void Port_NetLeave(void) {
    fprintf(stderr, "bt3: net: the session has ended\n");
    { /* Dragon Net: a ranked match left before its fight was over is a loss for who left (net_dragon.c) */
        extern void DrNet_SessionEnding(int mine);
        DrNet_SessionEnding(!sEndedThere && !sGone);
    }
    Lobby_SessionLeaving(); /* (a lobby's owner: everyone back to the lobby, before the goodbyes) */
    if (sApartAt != 0) {
        fprintf(stderr, "bt3: net: the two games differed from blank %u on (%u comparisons differed)\n", (unsigned)sApartAt, sApartCount);
    }
    PortChat_Reset();
    if (sMode == 3) {
        uint32_t bye[2] = {MAGIC, T_SPEC_BYE};
        int k;
        fprintf(stderr, "bt3: net: spectating: %u check values compared with the host's, %u differed\n", sSpecCompared, sSpecDiffered);
        for (k = 0; k < 3 && sSock >= 0; k++) {
            net_send(bye, sizeof(bye), &sPeer); /* (the host lets this spectator go at once) */
        }
        if (!sSpecNoticed && !sSpecRejoin) {
            Port_UiNotice("The players' match has ended.");
        }
        Port_UiBanner(NULL, 0);
    } else if (sSock >= 0 && sConnected) { /* tell the other side, so that it does not wait for input that will not come */
        uint32_t bye[3] = {MAGIC, T_BYE, sByeWhy != 0 ? sByeWhy : BYE_LEFT}, specBye[2] = {MAGIC, T_SPEC_BYE};
        int k, i;
        sByeSent = 1;
        for (k = 0; k < 3; k++) {
            net_send(bye, sizeof(bye), &sPeer);
        }
        for (i = 0; i < SPEC_MAX; i++) { /* (and the spectators: the match is over) */
            for (k = 0; k < 2 && sWatch[i].on; k++) {
                net_send(specBye, sizeof(specBye), &sWatch[i].addr);
            }
            sWatch[i].on = 0;
        }
        sWatchCount = 0;
    }
    if (Port_SessionCan()) {
        if (Port_SessionAsked()) {
            Port_SessionReturn(); /* the game as it was when the session was asked for (does not return) */
        }
        /* (a program started as the session where sessions are begun without a start again: only tests do that,
           and there was no game before it to go back to) */
        fflush(NULL);
        exit(0);
    }
    relaunch(0, NULL, 0);
}

/* ---- the lobby: called from the settings code's window (ui.cpp), once a frame ---- */
static int sLobby; /* 0 idle, 1 waiting, 2 found, -1 failed */
static int sLobbyRole, sLobbyPort;
static char sLobbyAddr[128];
static uint64_t sLobbyLast;

/* Room codes (net_match.c, port/matchmaking): the lobby's states 3 (asking the matchmaking service; the host's code
   is known at some point, Port_LobbyCode) and, once the other player's addresses are known, 1 as with an address:
   greetings go to every one of those addresses, and the host sends its own packets there too, so that both routers
   let the other side in. -3: the service said no (Port_LobbyError); -4: the two could not reach each other. */
static int sCodeMode, sCodeAddrs;
static int sCodeRelay = -1; /* which of sCodeAddr is the host's relay address (the one who joins) */
static int sCodeVia;        /* the match was reached through the relay (for the window) */
static struct sockaddr_in sCodeSeen; /* the address the service saw the other player under (for the relay's permission) */
static char sCodeAsked[16]; /* the code this game asked the service for (a lobby's: its member keeps it, net_lobby.c) */
static int sCodeSeenSet, sCodeGen, sCodeRelayGen; /* ... and which of the search's later news the lobby has taken in */

int Port_LobbyRelayed(void) { return sCodeVia; }
int Port_NetVersion(void) { return NET_VERSION; } /* (Dragon Net: net_dragon.c) */
static struct sockaddr_in sCodeAddr[MATCH_ADDRS];
static char sCodeText[8], sCodeError[256];
static uint64_t sCodeSince;

const char *Port_LobbyCode(void) { return sCodeText; }
const char *Port_LobbyServer(void) { return Match_ServerLabel(); }
const char *Port_LobbyError(void) { return sCodeError; }

/* A start asked for before this game's fingerprint was known: kept, and made by Port_LobbyPoll once it is (the
   lobby's state 6 meanwhile, which Port_LobbyPoll gives as 3, "asking"; Port_LobbyCheckingData says why). */
enum { PEND_ADDR = 1, PEND_WATCH, PEND_CODE, PEND_WATCH_CODE, PEND_QUICK };
static struct {
    int kind, host, port;
    char text[128];
} sPend;

static int lobby_hold(int kind, int host, const char *text, int port) {
    if (fp_known()) {
        return 0;
    }
    sPend.kind = kind;
    sPend.host = host;
    sPend.port = port;
    snprintf(sPend.text, sizeof(sPend.text), "%s", text != NULL ? text : "");
    sCodeError[0] = '\0';
    sLobby = 6;
    fprintf(stderr, "bt3: net: this game's gameplay data are still being checked: the %s begins when its fingerprint is known\n",
            kind == PEND_QUICK ? "quick match" : kind == PEND_WATCH || kind == PEND_WATCH_CODE ? "watching" : "connection");
    return 1;
}

/* 1 while a start waits for this game's fingerprint (the windows say "Checking your game data..."). */
int Port_LobbyCheckingData(void) {
    return sLobby == 6;
}

/* Why this game cannot search for a quick match now: NULL it can, "" its data are still being checked, else the
   reason (its data are not the original disc's). For the windows, before the search is asked for. */
const char *Port_LobbyQuickRefusal(void) {
    static char text[400];
    if (!fp_known()) {
        return "";
    }
    return fp_quick_refusal(text, sizeof(text))[0] != '\0' ? text : NULL;
}

/* An update is being downloaded or waits to be installed (gs/net_update.c): no new online session meanwhile (the other
   player would soon be on another version). The line that says so, in the chosen language; NULL: online play is on. */
const char *Port_OnlineBlocked(void) {
    const char *t;
    if (!Update_BlocksOnline()) {
        return NULL;
    }
    t = MenuTranslation_PortText("update/online_off"); /* (menu_translation.h: PORT_TEXT_UPDATE_ONLINE_OFF) */
    return t != NULL && t[0] != '\0' ? t : Update_OnlineNote();
}

int Port_LobbyStart(int host, const char *address, int port) {
    struct sockaddr_in local;

    if (Port_OnlineBlocked() != NULL) { /* (said by the screens through Port_LobbyError) */
        snprintf(sCodeError, sizeof(sCodeError), "%s", Port_OnlineBlocked());
        sLobby = -3;
        return sLobby;
    }
    if (lobby_hold(PEND_ADDR, host, address, port)) {
        return sLobby;
    }
    sRivalRegion[0] = '\0';
    if (Lobby_Active()) { /* (the lobby has the socket) */
        sLobby = -1;
        return sLobby;
    }
    if (sHandSock >= 0) { /* (a session that was asked for and never began) */
        closesocket(sHandSock);
        sHandSock = -1;
    }
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
    sCodeMode = 0;
    sCodeVia = 0;
    sCodeLocalPort = 0;
    sCodePunchSet = 0;
    sLobbyRole = host ? 1 : 2;
    sLobbyPort = port;
    snprintf(sLobbyAddr, sizeof(sLobbyAddr), "%s", address != NULL ? address : "");
    memset(&sPeer, 0, sizeof(sPeer));
    sPeer.sin_family = AF_INET;
    sLobby = 1;
    sLobbyOther = 0;
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

/* Watching the match hosted at `address`:`port` (a spectator). The lobby asks the host (T_SPEC_HELLO) until it
   answers with the match's set-up (state 2: the session begins as a spectator's) or says no (-3, Port_LobbyError
   says why); while the host has no match yet it says so (Port_LobbySpectateWaiting) and the lobby goes on asking. */
static uint64_t sSpecAskSince;
static int sLobbySpecWait;

int Port_LobbySpectate(const char *address, int port) {
    if (lobby_hold(PEND_WATCH, 0, address, port)) {
        return sLobby;
    }
    Port_LobbyStart(0, address, port);
    if (sLobby < 0) {
        return sLobby;
    }
    sLobbyRole = 3;
    sLobbySpecWait = 0;
    sCodeError[0] = '\0';
    sSpecAskSince = SDL_GetTicksNS();
    return sLobby;
}

int Port_LobbyStartCode(int host, const char *code);
static int sWatchByCode;

/* Watching the match of a room code: the service gives the host's addresses (Match_Begin role 4), and the lobby
   asks the host from there as by address. */
int Port_LobbySpectateCode(const char *code) {
    if (lobby_hold(PEND_WATCH_CODE, 0, code, 0)) {
        return sLobby;
    }
    sWatchByCode = 1;
    Port_LobbyStartCode(0, code);
    sWatchByCode = 0;
    if (sLobby >= 0) {
        sLobbyRole = 3;
        sLobbySpecWait = 0;
        sSpecAskSince = SDL_GetTicksNS();
    }
    return sLobby;
}

int Port_LobbySpectateWaiting(void) {
    return sLobby == 1 && sLobbyRole == 3 && sLobbySpecWait;
}

/* This PC's address on its own network (what another PC there types to join a match hosted by address): the address a
   socket would send from, found without sending anything (as net_match.c's for the matchmaking service). Asked again
   at most every five seconds; "" when it is not known (no network). */
const char *Port_NetLanAddress(void) {
    static char text[48];
    static uint64_t asked;
    uint64_t now = SDL_GetTicksNS();
    struct sockaddr_in to, me;
    socklen_t len = sizeof(me);
    int s;

    if (asked != 0 && now - asked < 5000000000ull) {
        return text;
    }
    asked = now;
    text[0] = '\0';
#ifdef _WIN32
    {
        WSADATA w;
        WSAStartup(MAKEWORD(2, 2), &w);
    }
#endif
    s = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        return text;
    }
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &to.sin_addr); /* (a documentation address: nothing is sent) */
    if (connect(s, (struct sockaddr *)&to, sizeof(to)) != 0 || getsockname(s, (struct sockaddr *)&me, &len) != 0 ||
        inet_ntop(AF_INET, &me.sin_addr, text, sizeof(text)) == NULL || strcmp(text, "0.0.0.0") == 0) {
        text[0] = '\0';
    }
    closesocket(s);
    return text;
}

static int sQuickQueue = -1; /* Port_LobbyStartQuick: the search that Port_LobbyStartCode starts is a quick match in this queue */

int Port_LobbyStartCode(int host, const char *code) {
    struct sockaddr_in local;
    socklen_t len = sizeof(local);

    if (sQuickQueue < 0 && !sWatchByCode && lobby_hold(PEND_CODE, host, code, 0)) {
        return sLobby;
    }
    Port_LobbyStart(1, "", 0); /* a socket on a port of the system's choosing */
    if (sLobby < 0) {
        return sLobby;
    }
    if (getsockname(sSock, (struct sockaddr *)&local, &len) != 0) {
        sLobby = -1;
        return sLobby;
    }
    sLobbyRole = host ? 1 : 2;
    sCodeMode = 1;
    sCodeAddrs = 0;
    sCodeSeenSet = 0;
    sCodeGen = sCodeRelayGen = 0;
    sCodeText[0] = sCodeError[0] = '\0';
    sCodeLocalPort = ntohs(local.sin_port);
    sLobbyPort = sCodeLocalPort;
    sLobby = 3;
    Relay_Close(-1);
    sCodeVia = 0;
    Match_Watch(sLobbyRole == 1 && sQuickQueue < 0 && spec_allowed()); /* (a room of its own: spectators may come with its code) */
    sKnockGen = -1;
    memset(sKnockUntil, 0, sizeof(sKnockUntil));
    snprintf(sCodeAsked, sizeof(sCodeAsked), "%s", sQuickQueue < 0 && code != NULL ? code : "");
    Match_Begin(sQuickQueue >= 0 ? 3 : sWatchByCode ? 4 : sLobbyRole, sSock, sCodeLocalPort, sQuickQueue >= 0 ? kMatchQueues[sQuickQueue] : code, sMyName, NET_VERSION);
    return sLobby;
}

/* A quick match in one of the queues (net_match.h: kMatchQueues). Whether this player hosts or joins is the
   service's choice, known when it answers (Port_LobbyPoll takes it from there). */
int Port_LobbyStartQuick(int queue) {
    char why[400];
    if (queue < 0 || queue >= MATCH_QUEUES) {
        return -1;
    }
    if (lobby_hold(PEND_QUICK, 0, NULL, queue)) {
        return sLobby;
    }
    /* Quick match is with strangers: the original disc's data only (with friends, the same data on both sides is
       enough). Here, where every quick match begins (Dragon Net Battle, Tenkaichi Budokai Online, BT3_LOBBY=quick). */
    if (fp_quick_refusal(why, sizeof(why))[0] != '\0') {
        unsigned long long fp = 0;
        Port_DataFingerprint(&fp);
        fprintf(stderr, "bt3: net: no quick match: this game's data are not the original disc's (gameplay fingerprint %016llx)\n", fp);
        snprintf(sCodeError, sizeof(sCodeError), "%s", why);
        sLobby = -3;
        return sLobby;
    }
    sQuickQueue = queue;
    Port_LobbyStartCode(1, "");
    sQuickQueue = -1;
    return sLobby;
}

/* What a quick match looks for: "auto" (the nearest first, everywhere after a while), "any", or a region ("EU"). */
void Port_LobbyQuickArea(const char *area) { Match_Area(area); }

/* A quick match that waits: whether the service now looks everywhere (Auto, past its widening): its seconds, 0 not said. */
int Port_LobbyWiden(void) {
    MatchStatus st;
    Match_Poll(&st);
    return st.widen;
}

void Port_LobbyQueuesAsk(void) { Match_QueuesAsk(NET_VERSION); }
void Port_LobbyHelloAsk(void) { Match_HelloAsk(); }
int Port_LobbyRelease(int *min, int *latest) { return Match_Release(min, latest); }
int Port_LobbyQueueWaiting(int queue) { return Match_QueueWaiting(queue); }

void Port_LobbyCancel(void) {
    if (sLobby == 6) { /* (a start that waited for the fingerprint: not made) */
        sPend.kind = 0;
        sLobby = 0;
        return;
    }
    if (sLobby == 5 || Lobby_Active()) { /* (it became a lobby: the lobby has the socket now) */
        sLobby = 0;
        sCodeMode = 0;
        return;
    }
    if (sCodeMode) {
        Match_Cancel();
        sCodeMode = 0;
    }
    Relay_Close(sSock);
    if (sSock >= 0) {
        closesocket(sSock);
        sSock = -1;
    }
    sLobby = 0;
}

/* The other player's addresses as the matchmaking service gave them: where to knock, which of them is the host's
   relay address, and whom this side's relay (if it holds one) lets through. Called again when they change. */
static void lobby_take_addrs(const MatchStatus *st) {
    int i;

    sCodeAddrs = 0;
    sCodeRelay = -1;
    for (i = 0; i < st->peerCount && sCodeAddrs < MATCH_ADDRS; i++) {
        char text[64], *colon;
        const char *addr = st->peerAddr[i];
        struct sockaddr_in *a = &sCodeAddr[sCodeAddrs];
        memset(a, 0, sizeof(*a));
        a->sin_family = AF_INET;
        if (strncmp(addr, "seen=", 5) == 0) { /* the address the service saw the other player under: for the relay only */
            if (inet_pton(AF_INET, addr + 5, &a->sin_addr) == 1) {
                Relay_Permit(sSock, a);
                sCodeSeen = *a;
                sCodeSeenSet = 1;
            }
            continue;
        }
        if (strncmp(addr, "relay=", 6) == 0) { /* the host's relay address: tried when the direct ones stay silent */
            addr += 6;
            sCodeRelay = sCodeAddrs;
        }
        snprintf(text, sizeof(text), "%s", addr);
        colon = strrchr(text, ':');
        if (colon != NULL) {
            *colon = '\0';
            a->sin_port = htons((uint16_t)atoi(colon + 1));
            if (inet_pton(AF_INET, text, &a->sin_addr) == 1 && a->sin_port != 0) {
                Relay_Permit(sSock, a); /* (the host, if it holds a relay address: this player may come in through it) */
                sCodeAddrs++;
            } else if (sCodeRelay == sCodeAddrs) {
                sCodeRelay = -1;
            }
        }
    }
}

/* This side has got a relay address after the other player was known: he is let through it now. */
static void lobby_permit_again(void) {
    int i;
    for (i = 0; i < sCodeAddrs; i++) {
        if (i != sCodeRelay) {
            Relay_Permit(sSock, &sCodeAddr[i]);
        }
    }
    if (sCodeSeenSet) {
        Relay_Permit(sSock, &sCodeSeen);
    }
}

/* The host's lobby: someone wants to watch before the match has begun (wait, or no). */
static void lobby_spec_hello(const uint8_t *pkt, int n, const struct sockaddr_in *from) {
    uint32_t why = spec_check(pkt, n);
    if (why != 0) {
        spec_no(from, why);
    } else {
        uint32_t wait[3] = {MAGIC, T_SPEC_WAIT, SPEC_VERSION};
        net_send(wait, sizeof(wait), from);
    }
}

/* 0 idle, 1 still waiting, 2 the other player is there, -1 it cannot work (port in use, unknown address). */
int Port_LobbyPoll(void) {
    uint8_t pkt[256];
    struct sockaddr_in from;
    uint64_t now = SDL_GetTicksNS();
    int n;

    if (sLobby == 6) { /* a start that waits for this game's fingerprint */
        int kind = sPend.kind;
        if (!fp_known()) {
            return 3;
        }
        fprintf(stderr, "bt3: net: the gameplay fingerprint is known: the start goes on\n");
        sLobby = 0;
        sPend.kind = 0;
        if (kind == PEND_ADDR) {
            Port_LobbyStart(sPend.host, sPend.text, sPend.port);
        } else if (kind == PEND_WATCH) {
            Port_LobbySpectate(sPend.text, sPend.port);
        } else if (kind == PEND_CODE) {
            Port_LobbyStartCode(sPend.host, sPend.text);
        } else if (kind == PEND_WATCH_CODE) {
            Port_LobbySpectateCode(sPend.text);
        } else if (kind == PEND_QUICK) {
            Port_LobbyStartQuick(sPend.port);
        }
        return sLobby == 6 ? 3 : sLobby;
    }
    if (sLobby == 3) { /* a room code: what the matchmaking service has said so far */
        MatchStatus st;
        int i;
        Match_Poll(&st);
        if (st.state == 2) {
            Relay_Tick(sSock); /* (the relay address is kept while the room waits; the thread is done with the socket) */
            if (sLobbyRole == 1) { /* spectators who come before the other player: told to wait (the other player's
                                      greetings that are taken here meanwhile come again) */
                watch_service();
                while ((n = net_recv(pkt, (int)sizeof(pkt), &from)) >= 8) {
                    uint32_t magic, type;
                    memcpy(&magic, pkt, 4);
                    memcpy(&type, pkt + 4, 4);
                    if (magic == MAGIC && type == T_SPEC_HELLO) {
                        lobby_spec_hello(pkt, n, &from);
                    }
                }
            }
        }
        snprintf(sCodeText, sizeof(sCodeText), "%s", st.code);
        if (st.role != 0) {
            sLobbyRole = st.role; /* (a quick match: host or joiner, as the service decided) */
        }
        if (st.state == -1) {
            snprintf(sCodeError, sizeof(sCodeError), "%s", st.error);
            sLobby = -3;
        } else if (st.state == 3 && st.lobby) {
            /* the code is a lobby's (the service said so): this game becomes its member (net_lobby.c), with this
               socket; the search's thread goes on asking for the owner's relay address meanwhile */
            Lobby_JoinFound((const char (*)[64])st.peerAddr, st.peerCount, st.code[0] != '\0' ? st.code : sCodeAsked, st.self);
            sCodeMode = 0;
            sLobby = 5;
        } else if (st.state == 3) {
            snprintf(sRivalRegion, sizeof(sRivalRegion), "%s", st.region);
            lobby_take_addrs(&st);
            sCodeGen = st.addrGen;
            sCodeRelayGen = st.relayGen;
            sCodeSince = now;
            sLobby = sCodeAddrs > 0 ? 1 : -4;
        }
        return sLobby;
    }
    if (sLobby != 1) {
        return sLobby;
    }
    if (sCodeMode) {
        MatchStatus st;
        if (Match_SocketBusy()) {
            return sLobby; /* (the search's thread is getting a relay address on this socket: not touched meanwhile) */
        }
        Match_Poll(&st);
        if (st.state == 3 && st.addrGen != sCodeGen) { /* the host's relay address has come: knocked at from now on */
            sCodeGen = st.addrGen;
            lobby_take_addrs(&st);
        }
        if (st.state == 3 && st.relayGen != sCodeRelayGen) { /* this side has a relay address now */
            sCodeRelayGen = st.relayGen;
            lobby_permit_again();
        }
    }
    if (sCodeMode && now - sLobbyLast > 200000000ull) {
        /* to every address the other player may be reached at: the one who joins greets, the host only knocks */
        uint32_t hello[4] = {MAGIC, T_HELLO, NET_VERSION, LOBBY_CAP}, punch[2] = {MAGIC, T_PUNCH};
        int i;
        for (i = 0; i < sCodeAddrs; i++) {
            if (i == sCodeRelay ? now - sCodeSince < 3000000000ull && getenv("BT3_RELAY_ONLY") == NULL : getenv("BT3_RELAY_ONLY") != NULL) {
                continue; /* three seconds for the direct ways first (BT3_RELAY_ONLY, testing: the relay at once, and
                             nothing else, also while there is no relay address yet) */
            }
            if (sLobbyRole == 3) {
                spec_hello_send(&sCodeAddr[i], 1);
            } else if (sLobbyRole == 2) {
                fprint_send(&sCodeAddr[i]);
                net_send(hello, sizeof(hello), &sCodeAddr[i]);
            } else {
                net_send(punch, sizeof(punch), &sCodeAddr[i]);
            }
        }
        sLobbyLast = now;
        if (sLobbyRole == 1) {
            watch_service();
        }
        if (sLobbyRole == 3 && sLobbySpecWait) {
            sCodeSince = now; /* (the host is there and has no match yet: waited for as long as the player does) */
        }
        if (sLobbyRole == 3 && now - sCodeSince > 24000000000ull) {
            snprintf(sCodeError, sizeof(sCodeError), "%s", kSpecNoAnswer);
            sLobby = -3;
            return sLobby;
        }
        if (now - sCodeSince > 24000000000ull) {
            sLobby = -4; /* 24 seconds of knocking (a relay address is asked for after four of them, see net_match.c):
                            the two cannot reach each other */
            return sLobby;
        }
    } else if (!sCodeMode && sLobbyRole == 3 && now - sLobbyLast > 200000000ull) {
        spec_hello_send(&sPeer, 1);
        sLobbyLast = now;
        if (!sLobbySpecWait && now - sSpecAskSince > SPEC_ASK_NS) {
            snprintf(sCodeError, sizeof(sCodeError), "%s", kSpecNoAnswer);
            sLobby = -3;
            return sLobby;
        }
    } else if (!sCodeMode && sLobbyRole == 2 && now - sLobbyLast > 200000000ull) {
        uint32_t hello[4] = {MAGIC, T_HELLO, NET_VERSION, LOBBY_CAP}; /* (the last word: this game knows lobbies) */
        fprint_send(&sPeer);
        net_send(hello, sizeof(hello), &sPeer);
        sLobbyLast = now;
    }
    while ((n = net_recv(pkt, (int)sizeof(pkt), &from)) >= 8) {
        uint32_t magic, type;
        memcpy(&magic, pkt, 4);
        memcpy(&type, pkt + 4, 4);
        if (magic != MAGIC) {
            continue;
        }
        if (type == T_LOBBY_INVITE && sLobbyRole != 1 && !sCodeMode && same_addr(&from, &sPeer)) {
            /* the address is a lobby's owner: this game becomes its member (net_lobby.c), with this socket */
            Lobby_JoinAddress(&from);
            sLobby = 5;
            return sLobby;
        }
        if (sLobbyRole == 3) { /* watching: only the host's answers */
            int k, ours = !sCodeMode && same_addr(&from, &sPeer);
            for (k = 0; sCodeMode && k < sCodeAddrs; k++) {
                ours |= same_addr(&from, &sCodeAddr[k]);
            }
            if (!ours) {
                continue;
            }
            if (type == T_SPEC_SETUP || type == T_SPEC_READY) {
                sLobby = 2;
                if (sCodeMode) { /* the address that answered is the one the session watches */
                    inet_ntop(AF_INET, &from.sin_addr, sLobbyAddr, sizeof(sLobbyAddr));
                    sLobbyPort = ntohs(from.sin_port);
                    sCodeVia = sCodeRelay >= 0 && same_addr(&from, &sCodeAddr[sCodeRelay]);
                }
            } else if (type == T_SPEC_WAIT) {
                sLobbySpecWait = 1;
            } else if (type == T_SPEC_NO && n >= 12) {
                uint32_t why;
                memcpy(&why, pkt + 8, 4);
                snprintf(sCodeError, sizeof(sCodeError), "%s", spec_no_text((int)why));
                sLobby = -3;
                return sLobby;
            }
            continue;
        }
        if (sLobbyRole == 1 && type == T_SPEC_HELLO) { /* someone wants to watch, before the match has begun */
            lobby_spec_hello(pkt, n, &from);
            continue;
        }
        if (type == T_FPRINT && fprint_differs(pkt, n)) {
            /* not the same game data: no match (the host answers with its own, so that the other side knows too) */
            if (sLobbyRole == 1) {
                fprint_send(&from);
            }
            snprintf(sCodeError, sizeof(sCodeError), "%s", kDataDiffer);
            sLobby = -3;
            return sLobby;
        } else if (sLobbyRole == 1 && type == T_HELLO && !(n >= 12 && memcmp(pkt + 8, &kVersion, 4) == 0)) {
            sLobbyOther = 1; /* someone with another release is trying: the window says so, and goes on waiting */
        } else if (sLobbyRole == 1 && type == T_HELLO) {
            uint32_t ack[3] = {MAGIC, T_HELLO_ACK, NET_VERSION};
            int k;
            fprint_send(&from);
            for (k = 0; k < 3; k++) {
                net_send(ack, sizeof(ack), &from);
            }
            if (sCodeMode) { /* where the session goes on knocking, so that the greeting of the session gets in */
                sCodePunch = from;
                sCodePunchSet = 1;
                sCodeVia = Relay_IsPeer(&from);
            }
            sLobby = 2;
        } else if (sLobbyRole == 2 && type == T_HELLO_ACK) {
            sLobby = n >= 12 && memcmp(pkt + 8, &kVersion, 4) == 0 ? 2 : -2; /* -2: the host is another release */
            if (sCodeMode && sLobby == 2) { /* the address that answered is the one the session joins */
                inet_ntop(AF_INET, &from.sin_addr, sLobbyAddr, sizeof(sLobbyAddr));
                sLobbyPort = ntohs(from.sin_port);
                sCodeVia = sCodeRelay >= 0 && from.sin_addr.s_addr == sCodeAddr[sCodeRelay].sin_addr.s_addr && from.sin_port == sCodeAddr[sCodeRelay].sin_port;
            }
        }
    }
    return sLobby;
}

void Port_LobbyLaunch(void);

/* BT3_LOBBY=host or BT3_LOBBY=join:CODE: the room-code lobby driven without its window, once a blank (tests; the
   code and each change of state are written to the log). BT3_RELAY_ONLY=1: the one who joins tries nothing but the
   host's relay address. */
static void lobby_background(void);
void Port_LobbyAuto(void) {
    static int state = -99, begun, coded;
    const char *what = getenv("BT3_LOBBY");
    int now;

    lobby_background();
    if (!Port_NetSession()) {
        Lobby_Pump(); /* (the eight-place lobby, outside its matches: net_lobby.c) */
    }
    if (sSpecRejoin && !Port_NetSession() && Lobby_Active()) {
        sSpecRejoin = 0; /* (a lobby's spectator is sent to the match again by the owner) */
    }
    if (sSpecRejoin && !Port_NetSession()) { /* a spectator whose game came apart from the host's: from the start again */
        char addr[160], *colon;
        sSpecRejoin = 0;
        snprintf(addr, sizeof(addr), "%s", sSpecTarget);
        colon = strrchr(addr, ':');
        if (colon != NULL) {
            *colon = '\0';
            Port_SessionRequest(3, addr, atoi(colon + 1));
        }
        return;
    }
    if (what == NULL || begun == 2 || Port_NetSession() || strncmp(what, "lobby", 5) == 0) {
        return; /* (BT3_LOBBY=lobby...: net_lobby.c) */
    }
    if (!begun) {
        static unsigned blanks;
        if (getenv("BT3_LOBBY_AT") != NULL && ++blanks < (unsigned)atoi(getenv("BT3_LOBBY_AT"))) {
            return; /* (BT3_LOBBY_AT=<n>: the lobby begins at the program's n-th blank, as a player would from the menus) */
        }
        begun = 1;
        if (getenv("BT3_NET_NAME") != NULL) {
            Port_NetNameSet(getenv("BT3_NET_NAME")); /* (the name for the matchmaking service too) */
        }
        if (strncmp(what, "quick:", 6) == 0) { /* BT3_LOBBY=quick:<queue>: a quick match (S, T, D10, D15, D20) */
            int q, type, dp;
            for (q = 0; q < MATCH_QUEUES && strcmp(what + 6, kMatchQueues[q]) != 0; q++) {
            }
            type = q == 0 ? 0 : q == 1 ? 1 : 2;
            dp = q >= 2 ? q - 2 : 0;
            Port_NetSpeedSet(NET_SPEED_STANDARD);
            Port_NetRulesSet(type, dp, 4); /* (a queue's rules: its battle type, no time limit, 120%) */
            if (getenv("BT3_NET_ROLLBACK") == NULL && getenv("BT3_NET_DELAY") == NULL) {
                Port_NetOptions(Port_RollCan() ? 8 : 0, -2); /* (and a quick match's fixed rollback and delay, as the window sets them) */
            }
            Port_LobbyQuickArea(getenv("BT3_QUICK_AREA")); /* (testing: "auto", "any", or a region such as "EU") */
            Port_LobbyStartQuick(q);
        } else if (strncmp(what, "spectate:", 9) == 0) { /* BT3_LOBBY=spectate:<address>:<port> or spectate:CODE: watches that match */
            char addr[128];
            int port;
            if (sscanf(what + 9, "%127[^:]:%d", addr, &port) == 2) {
                Port_LobbySpectate(addr, port);
            } else {
                Port_LobbySpectateCode(what + 9);
            }
        } else if (strncmp(what, "direct:", 7) == 0) { /* BT3_LOBBY=direct:<port> hosts, direct:<address>:<port> joins */
            char addr[128];
            int port;
            if (sscanf(what + 7, "%127[^:]:%d", addr, &port) == 2) {
                Port_LobbyStart(0, addr, port);
            } else {
                Port_LobbyStart(1, NULL, atoi(what + 7));
            }
        } else {
            Port_LobbyStartCode(strncmp(what, "join:", 5) != 0, strncmp(what, "join:", 5) == 0 ? what + 5 : "");
        }
    }
    now = Port_LobbyPoll();
    if (now != state || (!coded && Port_LobbyCode()[0] != '\0')) {
        state = now;
        coded = Port_LobbyCode()[0] != '\0';
        fprintf(stderr, "bt3: lobby: state %d, code '%s'%s%s\n", now, Port_LobbyCode(), now == -3 ? ", " : "", now == -3 ? Port_LobbyError() : "");
    }
    if (now == 5) {
        begun = 2; /* (a lobby's: net_lobby.c goes on) */
    } else if (now == 2) {
        fprintf(stderr, "bt3: lobby: the other player is there (%s)\n", Port_LobbyRelayed() ? "through the relay" : "directly");
        begun = 2;
        Port_LobbyLaunch();
    } else if (now < 0) {
        begun = 2;
    }
}

/* The other player is there: start the session. */
void Port_LobbyLaunch(void) {
    if (sCodeMode && Port_SessionCan()) {
        sHandSock = sSock; /* the session goes on with this socket (see sHandSock) */
    } else {
        closesocket(sSock);
    }
    sSock = -1;
    sLobby = 0;
    if (sCodeMode) {
        if (!(sLobbyRole == 1 && Port_SessionCan() && spec_allowed())) {
            Match_Cancel(); /* (a host that lets spectators in keeps its room while the match is on: Port_NetSessionEnd) */
        }
        sCodeMode = 0;
        if (sLobbyRole == 1) {
            sLobbyPort = sCodeLocalPort; /* the host's session listens where the lobby did */
        }
    }
    if (Port_SessionCan()) {
        Port_SessionRequest(sLobbyRole, sLobbyAddr, sLobbyPort); /* the game's thread makes the change at its next blank */
        return;
    }
    relaunch(sLobbyRole, sLobbyAddr, sLobbyPort);
}

/* For the meter (ui.cpp). 0 when no match is on; else out = ping in ms (-1 not known yet), going back per second,
   blanks per going back x 10, waits per second, ms waited per second, the rollback limit (0: waiting for each
   other), the input delay, whether it is chosen from the ping, why the waits of the last second were (1: the other
   game behind, 2: nothing came), the blank the two games came apart at (0: they agree). */
int Port_NetStats(int *out) {
    if (sMode <= 0 || sMode == 3 || !sConnected) {
        return 0;
    }
    out[0] = sPingMs;
    out[1] = sShown[1];
    out[2] = sShown[2];
    out[3] = sShown[3];
    out[4] = sShown[4];
    out[5] = sRollMax;
    out[6] = sDelay;
    out[7] = sAutoDelay;
    out[8] = sShown[5];        /* why this copy waited in the last second (WHY_*), 0 for no wait worth naming */
    out[9] = (int)sApartAt;    /* the blank at which the two games came apart, 0 for not */
    return 1;
}

/* 1 while a copy of another release is trying to join the lobby this copy hosts. */
int Port_LobbyOther(void) {
    return sLobbyOther;
}

/* ---- Tenkaichi Budokai Online (menu/port_tbo.c), the game's own screens for the room-code lobby ----
   The player's name is kept in online_name.txt beside the program. "Train While Waiting": the screens are left with
   the room still open; from then on Port_LobbyAuto (each blank) polls it, and when the other player comes the
   session begins wherever the game is (it starts from the game's first moment anyway). */
static int sLobbyBg;

/* The player's name: the setting net_name (the settings file's, which Dragon Net Battle's window and Tenkaichi
   Budokai Online's Profile both write), else online_name.txt beside the program (where Profile alone wrote it
   before: a game started with its own settings file, BT3_SETTINGS, kept the shared file's name). */
extern void Port_SettingText(const char *name, const char *def, char *out, int size);
extern void Port_SettingTextSave(const char *name, const char *value);
extern void Port_SettingsWrite(void);
const char *Port_NetNameGet(void) {
    static int loaded;
    if (!loaded) {
        char set[64];
        FILE *f;
        loaded = 1;
        if (strcmp(sMyName, "Player") != 0) {
            return sMyName; /* (a name given before it was first asked for: BT3_NET_NAME, tests) */
        }
        Port_SettingText("net_name", "", set, sizeof(set));
        if (set[0] != 0) {
            Port_NetNameSet(set);
            return sMyName;
        }
        f = fopen("online_name.txt", "rb");
        if (f != NULL) {
            char line[64] = "";
            size_t n = fread(line, 1, sizeof(line) - 1, f);
            fclose(f);
            line[n] = 0;
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] != 0) {
                Port_NetNameSet(line);
            }
        }
    }
    return sMyName;
}

void Port_NetNameSave(const char *name) {
    FILE *f;
    Port_NetNameSet(name);
    Port_SettingTextSave("net_name", sMyName);
    Port_SettingsWrite();
    f = fopen("online_name.txt", "wb");
    if (f != NULL) {
        fputs(sMyName, f);
        fclose(f);
    }
}

void Port_LobbyBackground(int on) {
    sLobbyBg = on;
}

int Port_LobbyInBackground(void) {
    return sLobbyBg;
}

static void lobby_background(void) {
    int now;
    if (!sLobbyBg || Port_NetSession()) {
        return;
    }
    now = Port_LobbyPoll();
    if (now == 5) { /* (the code was a lobby's: its window takes over) */
        sLobbyBg = 0;
    } else if (now == 2) {
        sLobbyBg = 0;
        Port_UiNotice("Tenkaichi Budokai Online: your opponent is here! The match begins.");
        Port_LobbyLaunch();
    } else if (now <= 0) {
        sLobbyBg = 0;
        Port_UiNotice(now == -3 ? Port_LobbyError() : "Tenkaichi Budokai Online: the room was closed.");
    }
}

/* ---- for the eight-place lobby (net_lobby.c, net_lobby.h) ---- */
int NetIo_Sock(void) {
    return sSock;
}

int NetIo_Version(void) {
    return NET_VERSION;
}

int NetIo_Open(int port) {
    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    sLobby = 0;
    sCodeMode = 0;
    if (sHandSock >= 0) {
        closesocket(sHandSock);
        sHandSock = -1;
    }
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
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons((uint16_t)port);
    if (sSock < 0 || bind(sSock, (struct sockaddr *)&local, sizeof(local)) != 0 || getsockname(sSock, (struct sockaddr *)&local, &len) != 0) {
        if (sSock >= 0) {
            closesocket(sSock);
        }
        sSock = -1;
        return -1;
    }
    Relay_Close(-1);
    sCodeVia = 0;
    sCodeLocalPort = 0;
    sCodePunchSet = 0;
    return ntohs(local.sin_port);
}

void NetIo_Close(void) {
    Relay_Close(sSock);
    if (sSock >= 0) {
        closesocket(sSock);
        sSock = -1;
    }
}

void NetIo_Send(const void *buf, int n, const struct sockaddr_in *to) {
    net_send(buf, n, to);
}

int NetIo_Recv(uint8_t *pkt, int size, struct sockaddr_in *from) {
    return net_recv(pkt, size, from);
}

int NetIo_Fingerprint(unsigned long long *fp) {
    return fprint_mine(fp);
}

int NetIo_FingerprintKnown(void) {
    return fp_known();
}

int NetIo_FingerprintRequired(void) {
    return fp_required();
}

const char *NetIo_MyName(void) {
    return sMyName;
}

void NetIo_WatchService(void) {
    watch_service();
}

uint32_t NetIo_Rules(int type, int dp, int time, int speed) {
    int percent = speed < 100 ? 100 : speed > 200 ? 200 : speed;
    return (uint32_t)(type < 0 || type > 2 ? 0 : type) | (uint32_t)(dp < 0 || dp > 2 ? 0 : dp) << 4 | (uint32_t)(time < 0 || time > 4 ? 3 : time) << 8 |
           speed_bits(percent);
}

void NetIo_Go(int role, const struct sockaddr_in *target, const struct sockaddr_in *alt, const struct sockaddr_in *knock,
              const struct sockaddr_in *knock2, uint32_t rules, int rollback, int delay) {
    char text[64] = "";
    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (sSock < 0) {
        return;
    }
    if (role == 1) {
        Port_NetOptions(rollback, delay); /* (the lobby's choices: the host's are the match's) */
        sRules = rules;
    } else {
        Port_NetOptions(-1, -1); /* (the host's choices come with its answer) */
        sRules = rules;
    }
    memset(&sCodePunch2, 0, sizeof(sCodePunch2));
    memset(&sJoinAlt, 0, sizeof(sJoinAlt));
    sCodePunchSet = knock != NULL && knock->sin_port != 0;
    if (sCodePunchSet) {
        sCodePunch = *knock;
    }
    if (knock2 != NULL && knock2->sin_port != 0) {
        sCodePunch2 = *knock2;
    }
    if (alt != NULL && alt->sin_port != 0 && !(target != NULL && same_addr(alt, target))) {
        sJoinAlt = *alt;
    }
    if (target != NULL && target->sin_port != 0) {
        inet_ntop(AF_INET, &target->sin_addr, text, sizeof(text));
    }
    sCodeLocalPort = getsockname(sSock, (struct sockaddr *)&local, &len) == 0 ? ntohs(local.sin_port) : 0;
    sCodeVia = target != NULL && Relay_IsPeer(target);
    sHandSock = sSock; /* (the session goes on with the lobby's socket, and gives it back when it ends) */
    sSock = -1;
    sGoPending = 1;
    Port_SessionRequest(role, text, role == 1 ? sCodeLocalPort : target != NULL ? ntohs(target->sin_port) : 0);
}

void NetIo_End(int why, const char *note) {
    if (sGone < 3) {
        sGone = why;
        snprintf(sGoneNote, sizeof(sGoneNote), "%s", note != NULL ? note : "");
    }
}

int NetIo_SessionMode(void) {
    return sSession && sMode > 0 ? sMode : 0;
}

int NetIo_SessionStarting(void) {
    return sGoPending;
}

int NetIo_Connected(void) {
    return NetIo_SessionMode() != 0 && (sMode == 3 ? sSpecSetup : sConnected && sMet);
}

int Port_NetLobbyOwnerSide(void) {
    uint32_t v = sRules >> 24 & 15;
    return Port_NetSession() && v >= 1 && v <= 3 ? (int)v - 1 : -1;
}
