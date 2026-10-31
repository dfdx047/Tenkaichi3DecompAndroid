/* Matchmaking with room codes (net_match.c), as the lobby in net.c uses it. */
#ifndef NET_MATCH_H
#define NET_MATCH_H

#define MATCH_ADDRS 4
#define MATCH_WATCH_ADDRS 32 /* the spectators' addresses a host is told, all together */

typedef struct MatchStatus {
    int state;                      /* 0 idle, 1 working, 2 the room is made (code), 3 the other player is known, -1 failed */
    char code[8];
    char error[256];                /* for the window, when state is -1 */
    char peerName[20];
    char peerAddr[MATCH_ADDRS][64]; /* "ip:port", best first */
    int peerCount;
    int role;                       /* a quick match: what this player turned out to be, 1 the host, 2 the one who joins */
    int searches;                   /* ... and how many times the search has started (a room lasts ten minutes) */
    int addrGen;                    /* raised when peerAddr has changed after state 3 (the host's relay address came) */
    int relayGen;                   /* raised when this side has got a relay address after state 3 (permissions are due) */
    int lobby;                      /* the code is a lobby's (the service said so when joining): the lobby's member, not a player */
    char self[64];                  /* this game's public address as STUN saw it ("" not known) */
    char region[4];                 /* a quick match: the other player's region as the service placed it ("EU"; "" not said) */
    int widen;                      /* a quick match that waits, Auto: after this many seconds it looks everywhere (0: not said) */
} MatchStatus;

#define MATCH_QUEUES 5
extern const char *const kMatchQueues[MATCH_QUEUES]; /* "S", "T", "D10", "D15", "D20" */

/* role 1 makes a room, 2 joins room `code`, 3 is a quick match in the queue named by `code`, 4 watches the match of
   room `code` (a spectator: state 3 with the host's addresses), 5 makes a lobby's room (state 2 with its code for as
   long as the lobby lives: its members' addresses come as the spectators' do, Match_Watchers). A join or watch of a
   lobby's code ends in state 3 with `lobby` set. */
void Match_Begin(int role, int sock, int localPort, const char *code, const char *name, int version);
int Match_SocketBusy(void); /* the search's thread is using the match's socket (asking for a relay address): hands off */
void Match_RelayAsk(void);  /* a lobby's owner: a member cannot reach it directly, a relay address is wanted (once) */
int Match_RelayGen(void);   /* raised each time this side has got a relay address (a lobby's owner: permissions are due) */
void Match_Watch(int on);   /* a host lets spectators in: its room is kept while the match is on (see net_match.c) */
void Match_Area(const char *area); /* a quick match: where to look ("auto", "any" or a region: "EU"), set before Match_Begin */
int Match_Watchers(char out[][64], int max, int *gen); /* the spectators' addresses ("seen=" too); *gen: changes */
void Match_QueuesAsk(int version);
void Match_HelloAsk(void);
int Match_Release(int *min, int *latest);
int Match_QueueWaiting(int queue);
void Match_Poll(MatchStatus *out);
const char *Match_ServerLabel(void); /* A/B for Dragon Net, PC otherwise */
void Match_Cancel(void);
int Match_SignTest(void);    /* BT3_SIGN_TEST: see net_match.c */
#include <stddef.h>
int Match_PostDragon(const char *what, const char *json, char *out, size_t size); /* Dragon Net's service (net_dragon.c) */
int Match_ServiceLimited(void); /* a service answered HTTP 429 a moment ago: nothing is asked for now (net_match.c) */

#endif
