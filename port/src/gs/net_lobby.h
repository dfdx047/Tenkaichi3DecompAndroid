/* The eight-place lobby (net_lobby.c): what it and net.c ask of each other. Internal to the netcode; the window's
   part (C and C++) is at the end. */
#ifndef NET_LOBBY_H
#define NET_LOBBY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { T_LOBBY_FIRST = 30, T_LOBBY_LAST = 40 }; /* the lobby's packet types (net_lobby.c); a release without lobbies ignores them */

#ifndef __cplusplus
struct sockaddr_in;

/* ---- net.c, for the lobby ---- */
int NetIo_Sock(void);                     /* the socket of the lobby (and of the session that follows), -1 none */
int NetIo_Open(int port);                 /* a new socket on `port` (0: the system's choice); its local port, -1 failed */
void NetIo_Close(void);                   /* the lobby's socket, closed (and its relay address given up) */
void NetIo_Send(const void *buf, int n, const struct sockaddr_in *to);
int NetIo_Recv(uint8_t *pkt, int size, struct sockaddr_in *from); /* (through the relay if one is held) */
int NetIo_Fingerprint(unsigned long long *fp);
const char *NetIo_MyName(void);
void NetIo_WatchService(void);            /* knocks at the addresses the matchmaking service has told of (room codes) */
uint32_t NetIo_Rules(int type, int dp, int time, int speed); /* the rules word a host sends (net.c: sRules) */
/* A session for the lobby's match: `role` 1 hosts, 2 joins `target` (or `alt`), 3 watches `target`. The host knocks
   at `knock` and `knock2` while it waits (the one who joins, when it is not the lobby's owner). Addresses with port
   0 are not given. The socket goes into the session (and comes back when it ends, Lobby_SessionEnded). */
void NetIo_Go(int role, const struct sockaddr_in *target, const struct sockaddr_in *alt, const struct sockaddr_in *knock,
              const struct sockaddr_in *knock2, uint32_t rules, int rollback, int delay);
/* Ends the session this copy is in at its next look at the socket: 3 back to the lobby, 4 the lobby was closed,
   5 this player left the lobby. `note` is what the screen says. */
void NetIo_End(int why, const char *note);
int NetIo_SessionMode(void);              /* 0 no session, else this copy's part in it: 1 host, 2 joins, 3 watches */
int NetIo_SessionStarting(void);          /* a session has been asked for and has not begun yet */
int NetIo_Connected(void);                /* the session has its other side (the other player; a spectator: the match) */
int NetIo_Version(void);                  /* NET_VERSION */

/* ---- net_lobby.c, for net.c ---- */
int Lobby_Active(void);                   /* this copy is in a lobby (owner or member) */
int Lobby_InMatch(void);                  /* ... and in the lobby's match (its session) */
void Lobby_Packet(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type);
void Lobby_Tick(void);                    /* in a session, with every look at the socket: its timers (cheap) */
void Lobby_Pump(void);                    /* outside a session, every blank (game thread): the socket and the timers */
void Lobby_SessionLeaving(void);          /* Port_NetLeave, before the goodbyes */
int Lobby_SessionEnded(void);             /* Port_NetSessionEnd: 1 = the lobby goes on with the socket */
int Lobby_ServesSpectators(void);         /* this copy's session sends the match to the lobby's spectators (its owner) */
int Lobby_IsMember(const struct sockaddr_in *from); /* (the owner: one of its members; a member: the owner) */
/* The 1v1 lobby (net.c, Port_LobbyPoll) found that the other side is a lobby: become its member. `addrs` the
   owner's addresses ("ip:port", "relay=ip:port", "seen=ip"), from a room code; or the owner's address that answered. */
void Lobby_JoinFound(const char addrs[][64], int count, const char *code, const char *self);
void Lobby_JoinAddress(const struct sockaddr_in *owner);
/* Outside a session, a packet of the 1v1 lobby's kind came to the owner's socket (T_HELLO, T_SPEC_HELLO): answered. */
void Lobby_Stranger(const uint8_t *pkt, int n, const struct sockaddr_in *from, uint32_t type);
#endif

/* ---- the window (ui.cpp) ---- */
#define LOBBY_SLOTS 8
#define LOBBY_NAME 20
#define LOBBY_CHAT 32
#define LOBBY_LOG 6
typedef struct PortLobbyView {
    int state;                  /* 0 none, 1 connecting, 2 in the lobby, 3 closed (error says why; Port_LobbyDo(LOBBY_OK) clears it) */
    int owner;                  /* this copy owns the lobby */
    int me;                     /* this copy's place (0, 1 the players, 2..7 the spectators), -1 none yet */
    int phase;                  /* 0 in the lobby window, 1 a match is on */
    int inMatch;                /* this copy is in the match */
    char code[8];               /* the room code ("" none, or not yet) */
    int port;                   /* the owner's port (by address) */
    int taken[LOBBY_SLOTS], ready[LOBBY_SLOTS], isOwner[LOBBY_SLOTS];
    char name[LOBBY_SLOTS][LOBBY_NAME];
    int battle, time, speed, rollback, delay; /* the rules: battle 0..4 (single, team, DP 10/15/20), time 0..4, speed %,
                                                 rollback frames 0..8, delay 0 automatic else frames + 1 */
    char note[128];             /* the last thing that happened ("X left the lobby") */
    unsigned noteSeq;
    char error[256];
    int chatCount;              /* lines of chat, oldest first */
    char chatName[LOBBY_CHAT][LOBBY_NAME];
    char chatText[LOBBY_CHAT][164];
    /* what happened, in order: the notes and the lines of chat together, the last LOBBY_LOG of them, oldest first
       (logName "" for a note); logTotal counts every one there has been (a new one: it changed) */
    int logCount;
    unsigned logTotal;
    char logName[LOBBY_LOG][LOBBY_NAME];
    char logText[LOBBY_LOG][164];
} PortLobbyView;

enum { LOBBY_START = 1, LOBBY_BACK, LOBBY_LEAVE, LOBBY_KICK, LOBBY_MOVE, LOBBY_RULE, LOBBY_READY, LOBBY_OK };
enum { LOBBY_RULE_BATTLE = 0, LOBBY_RULE_TIME, LOBBY_RULE_SPEED, LOBBY_RULE_ROLLBACK, LOBBY_RULE_DELAY };

int Port_LobbyView(PortLobbyView *out);   /* 0: no lobby (state 0) */
/* START, BACK (the owner: the match ends for everyone), LEAVE, KICK a (place), MOVE a b (two places swap), RULE a b
   (rule a one step: b -1 or +1), READY a (0 / 1), OK (a closed lobby's message was read). Done at the game's next
   blank. */
void Port_LobbyDo(int what, int a, int b);
void Port_LobbySay(const char *text);     /* a line of chat to everyone in the lobby */
/* A new lobby, owned by this copy: with a room code (byCode) or on `port`. The rules to start from. 1 = under way. */
int Port_LobbyCreate(int byCode, int port, int battle, int time, int speed, int rollback, int delay);
int Port_LobbyJoin(const char *address, int port); /* a lobby by its owner's address */
int Port_NetLobbyOwnerSide(void);         /* in a lobby's match: the owner's side (0, 1; 2 a spectator); -1 not a lobby's match */
/* The lobby's screen (src/menu/port_tbo.c) should be up: this copy is in a lobby, or was just told why it is not
   (state 1, 2, 3), and no match is on or starting for it. The main menu goes to it (src/menu/menu_a.c). */
int Port_LobbyWantsScreen(void);

#ifdef __cplusplus
}
#endif
#endif
