/* Z Warriors Online, the game's side: a private room or a quick match on port/zw-worker, the two players' warriors
   exchanged as "ZW1:" codes (plat_mods.c) and each one made from the player's own disc. The fight itself is not here
   yet: the room goes as far as both being ready. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/menu/port_zw_net.h"

extern int ZwHttp_Start(const char *what, const char *json);
extern int ZwHttp_Take(char *out, int size);
extern int Port_WarriorCode(int i, char *out, int size); /* plat_mods.c */
extern int Port_WarriorGuest(const char *code);
extern int Port_WarriorDropGuests(void);
extern int Port_WarriorFight(int i, int *chara, int *costume);
extern void Port_ModRescan(void);
extern void ZwFight_Put(const char *p1, const char *p2, int stage, int time); /* gs/zw_net.c */
extern int ZwFight_Take(char *p1, char *p2, int size, int *stage, int *time);

#define ZN_VERSION 1
#define ZN_DISC "SLUS-21678"
#define ZN_POLL 120 /* frames between two polls */
#define ZN_PORT 7777 /* the fight's UDP port, both phones on one network */
extern void Port_SessionRequest(int role, const char *address, int port); /* gs/state.c */
extern int Port_SessionCan(void);
extern void Port_NetZwPunch(int port, const char *punch); /* gs/net.c */
extern const char *ZwHttp_Public(void);
extern const char *ZwHttp_Local(void);

static ZwNetInfo sZn = {.peer = -1};
static char sKey[40], sTicket[40], sCode[17000], sJoinCode[8], sAnswer[40000], sPeerCode[17000];
static int sWarrior, sPeerRev, sWait, sAsked, sMadeOk, sBye;
static char sByeBody[128];
static int sOrphan; /* an entering request whose room or search was left before its answer came */
static char sOrphanCode[8];
enum { ASK_NONE, ASK_HOST, ASK_JOIN, ASK_QUICK, ASK_QUICKPOLL, ASK_POLL, ASK_READY, ASK_WARRIOR, ASK_RULES, ASK_START, ASK_ADDRS };
static int sDue[ASK_ADDRS + 1];
static char sPeerLan[64], sPeerPub[64], sPeerSeen[64]; /* theirs: own network, by STUN, as the server saw it */
static char sMyIp[64];  /* this phone as the server saw it */
static int sTrouble;    /* answers in a row that the server could not serve */
#define ZN_TROUBLE 15   /* so many (about half a minute of polls) and the room is given up */
static int sRound; /* the room's round last seen: a new one is the host's "start" */


/* ---- the little JSON the server answers ---- */
static const char *js_find(const char *s, const char *name) {
    char key[40];
    const char *p;

    snprintf(key, sizeof(key), "\"%s\":", name);
    p = s != NULL ? strstr(s, key) : NULL;
    return p != NULL ? p + strlen(key) : NULL;
}
static int js_str(const char *s, const char *name, char *out, int size) {
    const char *p = js_find(s, name);
    int n = 0;

    out[0] = '\0';
    if (p == NULL || *p++ != '"') {
        return 0;
    }
    for (; *p != '\0' && *p != '"' && n + 1 < size; p++) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
        }
        out[n++] = *p;
    }
    out[n] = '\0';
    return 1;
}
static int js_int(const char *s, const char *name, int def) {
    const char *p = js_find(s, name);

    return p == NULL ? def : *p == 't' ? 1 : *p == 'f' ? 0 : atoi(p);
}

static void reset(void) {
    int gen = sZn.libraryGen;

    memset(&sZn, 0, sizeof(sZn));
    sZn.peer = -1;
    sZn.libraryGen = gen;
    sKey[0] = sTicket[0] = sPeerCode[0] = '\0';
    sPeerRev = sWait = 0;
    memset(sDue, 0, sizeof(sDue));
    sAsked = 0; /* (an answer still on its way is not this room's) */
    sPeerLan[0] = sPeerPub[0] = sPeerSeen[0] = sMyIp[0] = '\0';
    sRound = sTrouble = 0;
}

static void fail(int why) {
    sZn.state = ZN_FAILED;
    sZn.error = why;
    memset(sDue, 0, sizeof(sDue));
}

static int begin(int warrior, int what) {
    int got = Port_WarriorCode(warrior, sCode, sizeof(sCode)); /* (first: leaving removes the guest, numbers move) */

    Port_ZwNetLeave();
    reset();
    sWarrior = warrior;
    if (!got) {
        fail(ZNE_WARRIOR);
        return 0;
    }
    sZn.state = what == ASK_QUICK ? ZN_SEARCHING : ZN_CONNECTING;
    sDue[what] = 1;
    return 1;
}

void Port_ZwNetHost(int warrior, const int *rules) {
    int r[6];

    memcpy(r, rules, sizeof(r));
    if (begin(warrior, ASK_HOST)) {
        sZn.host = 1;
        memcpy(sZn.rules, r, sizeof(r));
    }
}
void Port_ZwNetJoin(int warrior, const char *code) {
    snprintf(sJoinCode, sizeof(sJoinCode), "%s", code);
    begin(warrior, ASK_JOIN);
}
void Port_ZwNetQuick(int warrior, int madeOk) {
    sMadeOk = madeOk;
    begin(warrior, ASK_QUICK);
}
void Port_ZwNetReady(int ready) {
    if (sZn.state == ZN_ROOM) {
        sZn.ready = ready;
        sDue[ASK_ADDRS] = ready; /* (the addresses again: a router forgets) */
        sDue[ASK_READY] = 1;
    }
}
void Port_ZwNetWarrior(int warrior) {
    if (sZn.state == ZN_ROOM && Port_WarriorCode(warrior, sCode, sizeof(sCode))) {
        sWarrior = warrior;
        sZn.ready = 0;
        sDue[ASK_WARRIOR] = 1;
    }
}
void Port_ZwNetRules(const int *rules) {
    if (sZn.state == ZN_ROOM && sZn.host) {
        memcpy(sZn.rules, rules, sizeof(sZn.rules));
        sDue[ASK_RULES] = 1;
    }
}
void Port_ZwNetLeave(void) {
    if (sAsked == ASK_HOST || sAsked == ASK_JOIN || sAsked == ASK_QUICK) {
        sOrphan = sAsked;
        snprintf(sOrphanCode, sizeof(sOrphanCode), "%s", sJoinCode);
    }
    if (sKey[0] != '\0' && sZn.code[0] != '\0') {
        snprintf(sByeBody, sizeof(sByeBody), "{\"code\":\"%s\",\"key\":\"%s\"}", sZn.code, sKey);
        sBye = 1;
    } else if (sTicket[0] != '\0') {
        snprintf(sByeBody, sizeof(sByeBody), "{\"ticket\":\"%s\",\"key\":\"%s\"}", sTicket, sKey);
        sBye = 2;
    }
    reset();
    if (Port_WarriorDropGuests() > 0) { /* (the rival's warrior goes with the room) */
        sZn.libraryGen++;
    }
}
/* The host: the fight (`now`: a quick match, nobody presses ready there). */
void Port_ZwNetStart(int now) {
    if (sZn.state == ZN_ROOM && sZn.host && sZn.peerHere && sZn.peer >= 0 && (now || (sZn.ready && sZn.peerReady))) {
        sDue[ASK_START] = 1;
    }
}
void Port_ZwNetCopyCode(void) {
    extern void PortZw_ClipCopy(const char *text);

    PortZw_ClipCopy(sZn.code);
}
void Port_ZwNetGet(ZwNetInfo *out) {
    *out = sZn;
}

/* "a.b.c.d[:port]" of IPv4 only: its first three numbers are the same, its whole address is the same. */
static int ip_len(const char *a) {
    return (int)strcspn(a, ":");
}
static int ip_v4(const char *a) {
    const char *c = strchr(a, ':');

    return a[0] != '\0' && (c == NULL || strchr(c + 1, ':') == NULL) && strncmp(a, "0.0.0.0", 7) != 0 && strchr(a, '.') != NULL;
}
static int same_ip(const char *a, const char *b) {
    return ip_len(a) == ip_len(b) && strncmp(a, b, (size_t)ip_len(a)) == 0;
}
static int same_net(const char *a, const char *b) {
    int n = ip_len(a);

    while (n > 0 && a[n - 1] != '.') {
        n--;
    }
    return n > 0 && strncmp(a, b, (size_t)n) == 0 && memchr(b + n, '.', strcspn(b + n, ":")) == NULL;
}

/* The fight: on one home network (the same public address AND the same local network: a carrier gives one public address
   to many that cannot reach each other locally) straight there; else each by the other's public address (STUN's, or
   the one the server saw, on the fight's port), the host knocking. 1: the session was asked for. */
static int fight(void) {
    char ip[64], theirs[64] = "", *colon;
    const char *mine = ZwHttp_Public(), *local = ZwHttp_Local();
    int port = ZN_PORT, lan;

    if (!Port_SessionCan() || sPeerCode[0] == '\0') {
        return 0;
    }
    if (!ip_v4(mine)) {
        mine = ip_v4(sMyIp) ? sMyIp : "";
    }
    if (ip_v4(sPeerPub)) {
        snprintf(theirs, sizeof(theirs), "%s", sPeerPub);
    } else if (ip_v4(sPeerSeen)) {
        snprintf(theirs, sizeof(theirs), "%.40s:%d", sPeerSeen, ZN_PORT);
    }
    lan = ip_v4(sPeerLan) && ip_v4(local) && same_net(local, sPeerLan) && (mine[0] == '\0' || theirs[0] == '\0' || same_ip(mine, theirs));
    if (!lan && theirs[0] == '\0') {
        return 0;
    }
    ZwFight_Put(sZn.host ? sCode : sPeerCode, sZn.host ? sPeerCode : sCode, sZn.rules[0], sZn.rules[1]);
    Port_NetZwPunch(lan ? 0 : ZN_PORT, sZn.host && !lan ? theirs : NULL);
    if (sZn.host) {
        Port_SessionRequest(1, "", ZN_PORT);
        return 1;
    }
    snprintf(ip, sizeof(ip), "%s", lan ? sPeerLan : theirs);
    if ((colon = strchr(ip, ':')) != NULL) {
        port = atoi(colon + 1);
        *colon = '\0';
    }
    Port_SessionRequest(2, ip, port);
    return 1;
}

static void peer_read(const char *a) {
    const char *p = js_find(a, "peer");
    static char code[17000];

    if (p == NULL || *p != '{') {
        sZn.peerHere = sZn.peerReady = 0;
        sZn.peer = -1;
        sPeerRev = 0;
        return;
    }
    sZn.peerHere = 1;
    {
        const char *q = js_find(p, "addrs");

        js_str(p, "seen", sPeerSeen, sizeof(sPeerSeen));
        sPeerLan[0] = sPeerPub[0] = '\0';
        if (q != NULL) {
            sscanf(q, "[\"%63[^\"]\",\"%63[^\"]\"", sPeerLan, sPeerPub);
        }
    }
    sZn.peerReady = js_int(p, "ready", 0);
    if (js_str(p, "warrior", code, sizeof(code))) {
        sPeerRev = js_int(p, "rev", 0);
        if (strcmp(code, sPeerCode) != 0 || sZn.peer < 0) {
            snprintf(sPeerCode, sizeof(sPeerCode), "%s", code);
            if (Port_WarriorDropGuests() > 0) { /* (their last one goes first) */
                sZn.libraryGen++;
            }
            sZn.peer = Port_WarriorGuest(code);
            sZn.libraryGen++;
        }
    }
}
static void rules_read(const char *a) {
    const char *p = js_find(a, "rules");
    int i;

    for (i = 0; p != NULL && *p != '\0' && *p != ']' && i < 6; i++) {
        sZn.rules[i] = atoi(++p);
        while (*p != '\0' && *p != ',' && *p != ']') {
            p++;
        }
    }
}

static void answer(int what, const char *a) {
    char err[24];

    if (what == ASK_START && js_find(a, "round") != NULL) { /* the host: the room is told, the fight is waited for */
        sRound = js_int(a, "round", sRound);
        sZn.ready = 0;
        sTrouble = 0;
        fight();
        return;
    }

    if (js_str(a, "error", err, sizeof(err))) {
        /* a server that could not serve just now (503, its limits) is asked again: only a lasting one ends the room.
           Entering a room or a search still says so at once. */
        if (what != ASK_HOST && what != ASK_JOIN && what != ASK_QUICK && (!strcmp(err, "unavailable") || !strcmp(err, "limit") || !strcmp(err, "busy")) &&
            ++sTrouble < ZN_TROUBLE) {
            if (what != ASK_POLL && what != ASK_QUICKPOLL) {
                sDue[what] = 1;
                sWait = 0;
            }
            return;
        }
        fail(!strcmp(err, "no room") ? ZNE_NOROOM : !strcmp(err, "full") ? ZNE_FULL : !strcmp(err, "version") ? ZNE_VERSION
             : !strcmp(err, "disc") ? ZNE_DISC : !strcmp(err, "busy") ? ZNE_BUSY : !strcmp(err, "official") ? ZNE_OFFICIAL
             : !strcmp(err, "time") ? ZNE_TIME : ZNE_OTHER);
        return;
    }
    sTrouble = 0;
    if (sMyIp[0] == '\0') {
        js_str(a, "you", sMyIp, sizeof(sMyIp));
    }
    switch (what) {
    case ASK_HOST:
        js_str(a, "code", sZn.code, sizeof(sZn.code));
        js_str(a, "key", sKey, sizeof(sKey));
        sZn.state = sKey[0] != '\0' ? ZN_ROOM : ZN_FAILED;
        break;
    case ASK_JOIN:
        js_str(a, "key", sKey, sizeof(sKey));
        snprintf(sZn.code, sizeof(sZn.code), "%s", sJoinCode);
        sZn.state = sKey[0] != '\0' ? ZN_ROOM : ZN_FAILED;
        rules_read(a);
        peer_read(a);
        break;
    case ASK_QUICK:
    case ASK_QUICKPOLL:
        if (what == ASK_QUICK) {
            js_str(a, "ticket", sTicket, sizeof(sTicket));
            js_str(a, "key", sKey, sizeof(sKey));
        }
        js_str(a, "state", err, sizeof(err));
        if (!strcmp(err, "matched")) {
            sTicket[0] = '\0';
            js_str(a, "code", sZn.code, sizeof(sZn.code));
            js_str(a, "roomKey", sKey, sizeof(sKey));
            sZn.host = js_int(a, "host", 0);
            sZn.state = ZN_ROOM;
            sZn.quick = 1;
            sDue[ASK_ADDRS] = 1;
            rules_read(a);
            peer_read(a);
        } else if (!strcmp(err, "gone")) {
            fail(ZNE_NOROOM);
        } else {
            sZn.waiting = js_int(a, "waiting", 1);
        }
        break;
    case ASK_POLL:
        peer_read(a);
        rules_read(a);
        if (js_int(a, "round", sRound) > sRound) {
            sZn.ready = 0;
            /* the host started: join its fight (a start that could not be made now is tried at the next poll) */
            if (sZn.host || fight()) {
                sRound = js_int(a, "round", sRound);
            }
        }
        if (!sZn.host && !sZn.peerHere) {
            fail(ZNE_NOROOM);
        }
        break;
    }
}

void Port_ZwNetTick(void) {
    static char body[19000];
    int got = ZwHttp_Take(sAnswer, sizeof(sAnswer)), what = sAsked, i, n;

    if (got != 0 && sOrphan) { /* what it made on the server is given back */
        char key[40] = "", id[40] = "";

        if (got > 0 && js_str(sAnswer, "key", key, sizeof(key))) {
            if (sOrphan == ASK_QUICK && js_str(sAnswer, "ticket", id, sizeof(id))) {
                snprintf(sByeBody, sizeof(sByeBody), "{\"ticket\":\"%s\",\"key\":\"%s\"}", id, key);
                sBye = 2;
            } else if (sOrphan == ASK_JOIN || (sOrphan == ASK_HOST && js_str(sAnswer, "code", id, sizeof(id)))) {
                snprintf(sByeBody, sizeof(sByeBody), "{\"code\":\"%s\",\"key\":\"%s\"}", sOrphan == ASK_JOIN ? sOrphanCode : id, key);
                sBye = 1;
            }
        }
        sOrphan = 0;
        got = 0;
        what = sAsked = ASK_NONE;
    }
    if (got != 0) {
        sAsked = ASK_NONE;
        if (what != ASK_NONE && sZn.state != ZN_IDLE && sZn.state != ZN_FAILED) {
            if (got > 0) {
                answer(what, sAnswer);
            } else if (what == ASK_HOST || what == ASK_JOIN || what == ASK_QUICK) {
                fail(ZNE_NET);
            } else if (what != ASK_POLL && what != ASK_QUICKPOLL) {
                sDue[what] = 1; /* (a ready, a change or the start lost on the way: asked again) */
            }
        }
    }
    if (sBye) { /* (leaving goes first, once the line is free) */
        if (ZwHttp_Start(sBye == 1 ? "leave" : "quickleave", sByeBody)) {
            sBye = 0;
            sAsked = ASK_NONE;
        }
        return;
    }
    if (sAsked != ASK_NONE || sZn.state == ZN_IDLE || sZn.state == ZN_FAILED) {
        return;
    }
    if (++sWait >= ZN_POLL) {
        sWait = 0;
        if (sZn.state == ZN_ROOM) {
            sDue[ASK_POLL] = 1;
        } else if (sZn.state == ZN_SEARCHING && sTicket[0] != '\0') {
            sDue[ASK_QUICKPOLL] = 1;
        }
    }
    for (i = ASK_HOST; i <= ASK_ADDRS && !sDue[i]; i++) {
    }
    if (i > ASK_ADDRS) {
        return;
    }
    if (i == ASK_HOST || i == ASK_JOIN || i == ASK_QUICK) {
        n = snprintf(body, sizeof(body), "{\"v\":%d,\"disc\":\"%s\",\"name\":\"Player\",\"addrs\":[@],\"warrior\":\"%s\"", ZN_VERSION, ZN_DISC, sCode);
        if (i == ASK_HOST) {
            snprintf(body + n, sizeof(body) - n, ",\"rules\":[%d,%d,%d,%d,%d,%d]}", sZn.rules[0], sZn.rules[1], sZn.rules[2], sZn.rules[3], sZn.rules[4], sZn.rules[5]);
        } else if (i == ASK_JOIN) {
            snprintf(body + n, sizeof(body) - n, ",\"code\":\"%s\"}", sJoinCode);
        } else {
            snprintf(body + n, sizeof(body) - n, ",\"made\":true,\"madeOk\":%s}", sMadeOk ? "true" : "false");
        }
    } else if (i == ASK_QUICKPOLL) {
        snprintf(body, sizeof(body), "{\"ticket\":\"%s\",\"key\":\"%s\"}", sTicket, sKey);
    } else {
        n = snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"", sZn.code, sKey);
        if (i == ASK_POLL) {
            snprintf(body + n, sizeof(body) - n, ",\"have\":%d}", sPeerRev);
        } else if (i == ASK_READY) {
            snprintf(body + n, sizeof(body) - n, ",\"ready\":%s}", sZn.ready ? "true" : "false");
        } else if (i == ASK_WARRIOR) {
            snprintf(body + n, sizeof(body) - n, ",\"warrior\":\"%s\"}", sCode);
        } else if (i == ASK_ADDRS) {
            snprintf(body + n, sizeof(body) - n, ",\"addrs\":[@]}");
        } else if (i == ASK_START) {
            snprintf(body + n, sizeof(body) - n, "}");
        } else {
            snprintf(body + n, sizeof(body) - n, ",\"rules\":[%d,%d,%d,%d,%d,%d]}", sZn.rules[0], sZn.rules[1], sZn.rules[2], sZn.rules[3], sZn.rules[4], sZn.rules[5]);
        }
    }
    {
        static const char *const path[] = {"", "host", "join", "quick", "quickpoll", "poll", "ready", "warrior", "rules", "round", "addrs"};

        if (ZwHttp_Start(path[i], body)) {
            sDue[i] = 0;
            sAsked = i;
        }
    }
}

/* The session's Duel mode asks (menu_g_b.c): 1 the fight to set up (out: side 1's character and costume, side 2's,
   stage, time), -1 it has been fought (leave the session), 0 not a Z Warriors session. */
int Port_ZwFightSetup(int *out) {
    static char p1[17000], p2[17000];
    int stage = 0, time = 0, a, b;
    int state = ZwFight_Take(p1, p2, sizeof(p1), &stage, &time);

    if (state != 1) {
        return state == 2 ? -1 : 0;
    }
    Port_ModRescan(); /* (the session put the library back as the game started) */
    a = Port_WarriorGuest(p1);
    if (a < 0 || !Port_WarriorFight(a, &out[0], &out[1])) {
        return -1;
    }
    b = Port_WarriorGuest(p2);
    if (b < 0 || !Port_WarriorFight(b, &out[2], &out[3])) {
        return -1;
    }
    /* (switching one on, or making the second, moves the others: both read again now that both are on) */
    a = Port_WarriorGuest(p1);
    Port_WarriorFight(a, &out[0], &out[1]);
    b = Port_WarriorGuest(p2);
    Port_WarriorFight(b, &out[2], &out[3]);
    out[4] = stage < 0 ? 0 : stage;
    out[5] = time;
    fprintf(stderr, "bt3: zw online: direct fight %d/%d against %d/%d, stage %d\n", out[0], out[1], out[2], out[3], out[4]);
    return 1;
}
