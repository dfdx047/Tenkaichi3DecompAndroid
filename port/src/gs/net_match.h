/* Matchmaking with room codes (net_match.c), as the lobby in net.c uses it. */
#ifndef NET_MATCH_H
#define NET_MATCH_H

#define MATCH_ADDRS 4

typedef struct MatchStatus {
    int state;                      /* 0 idle, 1 working, 2 the room is made (code), 3 the other player is known, -1 failed */
    char code[8];
    char error[96];                 /* for the window, when state is -1 */
    char peerName[20];
    char peerAddr[MATCH_ADDRS][64]; /* "ip:port", best first */
    int peerCount;
    int role;                       /* a quick match: what this player turned out to be, 1 the host, 2 the one who joins */
    int searches;                   /* ... and how many times the search has started (a room lasts ten minutes) */
    int addrGen;                    /* raised when peerAddr has changed after state 3 (the host's relay address came) */
    int relayGen;                   /* raised when this side has got a relay address after state 3 (permissions are due) */
} MatchStatus;

#define MATCH_QUEUES 5
extern const char *const kMatchQueues[MATCH_QUEUES]; /* "S", "T", "D10", "D15", "D20" */

/* role 1 makes a room, 2 joins room `code`, 3 is a quick match in the queue named by `code`. */
void Match_Begin(int role, int sock, int localPort, const char *code, const char *name, int version);
int Match_SocketBusy(void); /* the search's thread is using the match's socket (asking for a relay address): hands off */
void Match_QueuesAsk(int version);
void Match_HelloAsk(void);
void Match_UpdateAsk(void);   /* asks GitHub for the newest release (a thread; once is enough) */
int Match_UpdateLatest(void); /* its number as PORT_RELEASE counts, 0 while unknown */
int Match_Release(int *min, int *latest);
int Match_QueueWaiting(int queue);
void Match_Poll(MatchStatus *out);
void Match_Cancel(void);
int Match_SignTest(void);    /* BT3_SIGN_TEST: see net_match.c */

#endif
