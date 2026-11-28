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

static ZwNetInfo sZn = {.peer = -1};
static char sKey[40], sTicket[40], sCode[17000], sJoinCode[8], sAnswer[40000], sPeerCode[17000];
static int sWarrior, sPeerRev, sWait, sAsked, sMadeOk, sBye;
static char sByeBody[128];
enum { ASK_NONE, ASK_HOST, ASK_JOIN, ASK_QUICK, ASK_QUICKPOLL, ASK_POLL, ASK_READY, ASK_WARRIOR, ASK_RULES, ASK_START, ASK_ADDRS };
static int sDue[ASK_ADDRS + 1];
static char sPeerLan[64], sPeerPub[64];
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
    sPeerLan[0] = sPeerPub[0] = '\0';
    sRound = 0;
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

/* The fight: on one network (the same public address) straight there; else each by what STUN saw, the host knocking. */
static void fight(void) {
    char ip[64], *colon;
    const char *mine = ZwHttp_Public();
    size_t n = strcspn(mine, ":");
    int lan = sPeerPub[0] == '\0' || mine[0] == '\0' || (strncmp(mine, sPeerPub, n) == 0 && sPeerPub[n] == ':');
    int port = ZN_PORT;

    if (!Port_SessionCan() || (sPeerLan[0] == '\0' && sPeerPub[0] == '\0')) {
        return;
    }
    if (sPeerCode[0] == '\0') {
        return;
    }
    ZwFight_Put(sZn.host ? sCode : sPeerCode, sZn.host ? sPeerCode : sCode, sZn.rules[0], sZn.rules[1]);
    Port_NetZwPunch(lan ? 0 : ZN_PORT, sZn.host && !lan ? sPeerPub : NULL);
    if (sZn.host) {
        Port_SessionRequest(1, "", ZN_PORT);
        return;
    }
    snprintf(ip, sizeof(ip), "%s", lan ? sPeerLan : sPeerPub);
    if ((colon = strchr(ip, ':')) != NULL) {
        port = atoi(colon + 1);
        *colon = '\0';
    }
    Port_SessionRequest(2, ip, port);
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
        fight();
        return;
    }

    if (js_str(a, "error", err, sizeof(err))) {
        fail(!strcmp(err, "no room") ? ZNE_NOROOM : !strcmp(err, "full") ? ZNE_FULL : !strcmp(err, "version") ? ZNE_VERSION
             : !strcmp(err, "disc") ? ZNE_DISC : !strcmp(err, "busy") ? ZNE_BUSY : !strcmp(err, "official") ? ZNE_OFFICIAL
             : !strcmp(err, "time") ? ZNE_TIME : ZNE_OTHER);
        return;
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
            sRound = js_int(a, "round", sRound);
            sZn.ready = 0;
            if (!sZn.host) { /* the host started: join its fight */
                fight();
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
