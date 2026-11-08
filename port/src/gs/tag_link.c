/* The Tag Team mod's rooms over the internet: a room code from the Tag Team Online service (port/tagnet-worker), and
 * the room's connections over one UDP socket instead of TCP (a phone cannot take a TCP connection from outside: its
 * operator's network does not let one in).
 *
 * The service tells the owner of a room the addresses of the players who joined (their own network, the address the
 * internet sees: STUN) and tells them the owner's. Both sides then send to all of them at once (each one's router
 * lets the other's packets in once it has sent some out): the first to answer is the way. When a player cannot be
 * reached so, the owner takes an address at Cloudflare's TURN relay (net_relay.c, the login from the service) and
 * the player sends there.
 *
 * Over that, each player has one connection to the owner (the room is a star, as tag_net.c's was over TCP): an
 * ordered, reliable stream of bytes, as TCP's. Numbered pieces of up to TL_PIECE bytes, each acknowledged (the next
 * one wanted, and which of the 32 after it came), sent again when no acknowledgement came in time. tag_net.c reads
 * and writes these as it did its sockets (TagLink_Send, TagLink_Recv). */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define closesocket close
#endif
#include "net_relay.h"
#include "tag_link.h"
#ifndef _WIN32
#include <netinet/in.h>
#endif

/* An address of a player: IPv4, or IPv6 (2026-11-02: phones on mobile data are often behind their operator's IPv4
   NAT, where two of them cannot reach each other directly and the match went through the room's relay, 300 ms and
   more a round trip; most Brazilian operators also give them a public IPv6 address, where they can). */
typedef union { struct sockaddr sa; struct sockaddr_in v4; struct sockaddr_in6 v6; } TlAddr;

extern int TagLink_Stun(int sock, char *out, int size);       /* net_match.c */
extern int TagLink_Lan(int port, char *out, int size);
extern int TagLink_Http(const char *url, const char *json, char *out, int size);
extern const char *Port_FileRoot(void);

#define TL_MAGIC 0x54474C31u /* "TGL1" */
#define TL_HEAD 27
#define TL_PIECE 1100        /* bytes of the stream in one packet (with the relay's wrapping, under 1400) */
#define TL_WINDOW 256        /* pieces under way at most (a power of two) */
#define TL_INBOX 131072      /* bytes come in and not yet read */
#define TL_CANDS 10
#define TL_MEMBERS 24

enum { T_SYN = 1, T_SYNACK, T_DATA, T_ACK, T_FIN, T_PUNCH };

typedef struct {
    uint8_t data[TL_PIECE];
    int len, used, tries;
    Uint64 sentAt, firstAt; /* (last sent, first sent) */
} Piece;

typedef struct {
    int state; /* TL_* */
    uint32_t id;
    int member; /* the service's number of the player (the owner: from its SYN) */
    int given;  /* handed to tag_net.c (TagLink_Accept) */
    TlAddr to;
    uint32_t sndNext, sndUna, rcvNext;
    Piece tx[TL_WINDOW], rx[TL_WINDOW];
    uint8_t inbox[TL_INBOX];
    int inN;
    Uint64 lastIn, lastOut, lastSyn, opened;
    int ackDue; /* pieces came in that no packet of ours has acknowledged yet */
    int srtt, rto; /* ms */
    TlAddr direct; /* the last direct way that worked (haveDirect), kept while the relay carries it */
    int haveDirect;
    int quietTold;
    Uint64 lastDirectIn; /* when something last came directly */
    int rttvar;          /* ms: how much the round trip varies (the time before a piece goes again) */
    Uint64 lastSample;   /* when the round trip was last measured */
    uint32_t fastSeq;    /* the piece last sent again early (the other side had later ones), and when */
    Uint64 fastAt;
    int viaRelay;        /* (for the log: the way now used) */
} Conn;

typedef struct {
    int id, watch, cands;
    TlAddr cand[TL_CANDS];
    TlAddr seen;
    int haveSeen;
    Uint64 since;
} Member;

static struct {
    int sock, sock6; /* (sock6: -1 without IPv6) */
    int hosting, active;
    uint32_t tag; /* the room's code as a number: packets of another room are not ours */
    Conn conn[TL_SLOTS];
    /* what the service's thread found out (under `lock`) */
    SDL_Mutex *lock;
    SDL_Thread *thread;
    SDL_AtomicInt gen, busy;  /* gen: the room's number (a thread of an earlier room stops when it changes); busy: the
                                 thread reads the socket itself (STUN, the relay's login): keep off it */
    char url[256], name[32], code[16], key[48], status[160];
    int version, watch, myId, codeReady;
    Member member[TL_MEMBERS];
    int members;
    TlAddr hostCand[TL_CANDS];
    int hostCands;
    SDL_AtomicInt relayOn; /* the room's relay at the service (a WebSocket, the app's RelaySocket) is open */
    Uint64 lastPunch;
} L = {-1, -1};

/* While the service's thread asks Cloudflare's relay for an address (Relay_AllocateVia, up to ten rounds of 0.7 s): the
   game thread goes on reading the socket and taking the room's packets, and the relay's answers (anything that is not
   one of ours: TL_MAGIC) wait here for that thread. Before, the game thread kept off the socket for the whole time and
   the players' packets that came then were read by the allocation and thrown away: a match held still for seconds
   whenever the owner asked (someone in the room's list not connected for 4 s, again every 30 s while it failed). */
static SDL_AtomicInt sAllocating;
static struct { uint8_t data[768]; int len; struct sockaddr_in from; } sBox[8];
static int sBoxN;
static void box_put(const uint8_t *data, int len, const struct sockaddr_in *from) {
    if (len <= 0 || len > (int)sizeof(sBox[0].data)) {
        return;
    }
    SDL_LockMutex(L.lock);
    if (sBoxN == (int)(sizeof(sBox) / sizeof(sBox[0]))) { /* (full: the oldest goes, the allocation sends again) */
        SDL_memmove(&sBox[0], &sBox[1], sizeof(sBox[0]) * (size_t)(sBoxN - 1));
        sBoxN--;
    }
    SDL_memcpy(sBox[sBoxN].data, data, (size_t)len);
    sBox[sBoxN].len = len;
    sBox[sBoxN].from = *from;
    sBoxN++;
    SDL_UnlockMutex(L.lock);
}
static int box_take(uint8_t *out, int max, struct sockaddr_in *from) {
    int n = -1;
    SDL_LockMutex(L.lock);
    if (sBoxN > 0) {
        n = sBox[0].len < max ? sBox[0].len : max;
        SDL_memcpy(out, sBox[0].data, (size_t)n);
        *from = sBox[0].from;
        sBoxN--;
        SDL_memmove(&sBox[0], &sBox[1], sizeof(sBox[0]) * (size_t)sBoxN);
    }
    SDL_UnlockMutex(L.lock);
    return n;
}

/* ---- small things ---- */

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static void set_status(const char *s) {
    SDL_LockMutex(L.lock);
    SDL_strlcpy(L.status, s, sizeof(L.status));
    SDL_UnlockMutex(L.lock);
}

static int parse_addr(const char *text, TlAddr *out) {
    char ip[64];
    int port = 0;
    const char *colon = SDL_strrchr(text, ':');
    if (colon == NULL || colon - text >= (int)sizeof(ip)) {
        return 0;
    }
    SDL_memcpy(ip, text, (size_t)(colon - text));
    ip[colon - text] = 0;
    port = SDL_atoi(colon + 1);
    SDL_memset(out, 0, sizeof(*out));
    if (port <= 0 || port >= 65536) {
        return 0;
    }
    if (SDL_strchr(ip, ':') != NULL) { /* IPv6: "2804:5c::1:41234", the port after the last colon */
        out->v6.sin6_family = AF_INET6;
        out->v6.sin6_port = htons((unsigned short)port);
        return inet_pton(AF_INET6, ip, &out->v6.sin6_addr) == 1;
    }
    out->v4.sin_family = AF_INET;
    out->v4.sin_port = htons((unsigned short)port);
    return inet_pton(AF_INET, ip, &out->v4.sin_addr) == 1;
}
static socklen_t addr_len(const TlAddr *a) {
    return a->sa.sa_family == AF_INET6 ? (socklen_t)sizeof(a->v6) : (socklen_t)sizeof(a->v4);
}

static int same_addr(const TlAddr *a, const TlAddr *b) {
    if (a->sa.sa_family != b->sa.sa_family) {
        return 0;
    }
    if (a->sa.sa_family == AF_INET6) {
        return SDL_memcmp(&a->v6.sin6_addr, &b->v6.sin6_addr, 16) == 0 && a->v6.sin6_port == b->v6.sin6_port;
    }
    return a->v4.sin_addr.s_addr == b->v4.sin_addr.s_addr && a->v4.sin_port == b->v4.sin_port;
}

/* The JSON list `key` of the answer (strings) into addresses; "relay=" ones too. Returns how many. */
static int json_addrs(const char *json, const char *key, TlAddr *out, int max) {
    char pat[40];
    const char *p, *end;
    int n = 0;
    SDL_snprintf(pat, sizeof(pat), "\"%s\":[", key);
    p = SDL_strstr(json, pat);
    if (p == NULL) {
        return 0;
    }
    p += SDL_strlen(pat);
    end = SDL_strchr(p, ']');
    while (p != NULL && end != NULL && p < end && n < max) {
        const char *q = SDL_strchr(p, '"'), *e;
        char text[80];
        if (q == NULL || q > end || (e = SDL_strchr(q + 1, '"')) == NULL || e > end || e - q - 1 >= (int)sizeof(text)) {
            break;
        }
        SDL_memcpy(text, q + 1, (size_t)(e - q - 1));
        text[e - q - 1] = 0;
        if (parse_addr(SDL_strncmp(text, "relay=", 6) == 0 ? text + 6 : text, &out[n])) {
            n++;
        }
        p = e + 1;
    }
    return n;
}

static int json_str(const char *json, const char *key, char *out, int size) {
    char pat[40];
    const char *p, *e;
    SDL_snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    p = SDL_strstr(json, pat);
    if (p == NULL) {
        return 0;
    }
    p += SDL_strlen(pat);
    e = SDL_strchr(p, '"');
    if (e == NULL || e - p >= size) {
        return 0;
    }
    SDL_memcpy(out, p, (size_t)(e - p));
    out[e - p] = 0;
    return 1;
}

static int json_int(const char *json, const char *key, int def) {
    char pat[40];
    const char *p;
    SDL_snprintf(pat, sizeof(pat), "\"%s\":", key);
    p = SDL_strstr(json, pat);
    return p != NULL ? SDL_atoi(p + SDL_strlen(pat)) : def;
}

/* ---- the room's relay at the service: a WebSocket of the app's (RelaySocket.kt), through JNI ----
   A player reached through it has an address of its own here: 0.0.0.0 and its number + 1 as the port (the owner: 0). */
static TlAddr relay_addr(int id) {
    TlAddr a;
    SDL_memset(&a, 0, sizeof(a));
    a.v4.sin_family = AF_INET;
    a.v4.sin_port = htons((unsigned short)(id + 1));
    return a;
}
static int is_relay(const TlAddr *a) {
    return a->sa.sa_family == AF_INET && a->v4.sin_addr.s_addr == 0 && a->v4.sin_port != 0;
}
#ifdef __ANDROID__
#include <jni.h>
static jobject sAct;
static jmethodID sROpen, sRSend, sRRecv, sRClose;
static JNIEnv *relay_env(void) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (env != NULL && sAct == NULL) {
        jobject act = (jobject)SDL_GetAndroidActivity();
        jclass cls;
        if (act == NULL) {
            return NULL;
        }
        cls = (*env)->GetObjectClass(env, act);
        sROpen = (*env)->GetMethodID(env, cls, "relayOpen", "(Ljava/lang/String;)Z");
        sRSend = (*env)->GetMethodID(env, cls, "relaySend", "([B)Z");
        sRRecv = (*env)->GetMethodID(env, cls, "relayRecv", "()[B");
        sRClose = (*env)->GetMethodID(env, cls, "relayClose", "()V");
        if ((*env)->ExceptionCheck(env) || !sROpen || !sRSend || !sRRecv || !sRClose) {
            (*env)->ExceptionClear(env);
            (*env)->DeleteLocalRef(env, cls);
            (*env)->DeleteLocalRef(env, act);
            return NULL;
        }
        sAct = (*env)->NewGlobalRef(env, act);
        (*env)->DeleteLocalRef(env, cls);
        (*env)->DeleteLocalRef(env, act);
    }
    return sAct != NULL ? env : NULL;
}
static int relay_open(const char *url) {
    JNIEnv *env = relay_env();
    jstring u;
    jboolean ok;
    if (env == NULL) {
        return 0;
    }
    u = (*env)->NewStringUTF(env, url);
    ok = (*env)->CallBooleanMethod(env, sAct, sROpen, u);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        ok = JNI_FALSE;
    }
    (*env)->DeleteLocalRef(env, u);
    return ok == JNI_TRUE;
}
static void relay_send(const uint8_t *data, int len) {
    JNIEnv *env = relay_env();
    jbyteArray a;
    if (env == NULL) {
        return;
    }
    a = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env, a, 0, len, (const jbyte *)data);
    (*env)->CallBooleanMethod(env, sAct, sRSend, a);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
    (*env)->DeleteLocalRef(env, a);
}
/* The next message in: its length, 0 none, -1 the relay closed. */
static int relay_recv(uint8_t *out, int max) {
    JNIEnv *env = relay_env();
    jbyteArray a;
    int n;
    if (env == NULL) {
        return -1;
    }
    a = (jbyteArray)(*env)->CallObjectMethod(env, sAct, sRRecv);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return -1;
    }
    if (a == NULL) {
        return 0;
    }
    n = (*env)->GetArrayLength(env, a);
    if (n == 0) {
        (*env)->DeleteLocalRef(env, a);
        return -1;
    }
    if (n > max) {
        n = max;
    }
    (*env)->GetByteArrayRegion(env, a, 0, n, (jbyte *)out);
    (*env)->DeleteLocalRef(env, a);
    return n;
}
static void relay_close(void) {
    JNIEnv *env = relay_env();
    if (env != NULL) {
        (*env)->CallVoidMethod(env, sAct, sRClose);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
        }
    }
}
#elif !defined(_WIN32)
/* PC: no room relay, except for port/tools/taglink_test (TAGLINK_TEST_RELAY=<port>: a stand-in for the Durable
   Object over UDP on this computer, its relay.mjs; the first message says who this is, "h" or "m<number>"). */
static int sTestRelay = -1;
static int relay_open(const char *url) {
    const char *port = getenv("TAGLINK_TEST_RELAY");
    struct sockaddr_in a;
    char who[16];
    (void)url;
    if (port == NULL) {
        return 0;
    }
    sTestRelay = (int)socket(AF_INET, SOCK_DGRAM, 0);
    SDL_memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)SDL_atoi(port));
    a.sin_addr.s_addr = htonl(0x7f000001);
    if (sTestRelay < 0 || connect(sTestRelay, (struct sockaddr *)&a, sizeof(a)) != 0) {
        return 0;
    }
    fcntl(sTestRelay, F_SETFL, fcntl(sTestRelay, F_GETFL, 0) | O_NONBLOCK);
    if (L.hosting) {
        SDL_strlcpy(who, "Rh", sizeof(who));
    } else {
        SDL_snprintf(who, sizeof(who), "Rm%d", L.myId);
    }
    send(sTestRelay, who, SDL_strlen(who), 0);
    return 1;
}
static void relay_send(const uint8_t *data, int len) {
    if (sTestRelay >= 0) {
        send(sTestRelay, (const char *)data, (size_t)len, 0);
    }
}
static int relay_recv(uint8_t *out, int max) {
    int n;
    static Uint64 said;
    if (sTestRelay < 0) {
        return -1;
    }
    if (SDL_GetTicks() - said > 500) { /* (who this is, again: one UDP message may be lost, a WebSocket's is not) */
        char who[16];
        said = SDL_GetTicks();
        if (L.hosting) {
            SDL_strlcpy(who, "Rh", sizeof(who));
        } else {
            SDL_snprintf(who, sizeof(who), "Rm%d", L.myId);
        }
        send(sTestRelay, who, SDL_strlen(who), 0);
    }
    n = (int)recv(sTestRelay, (char *)out, (size_t)max, 0);
    return n > 0 ? n : 0;
}
static void relay_close(void) {
    if (sTestRelay >= 0) {
        close(sTestRelay);
        sTestRelay = -1;
    }
}
#else
static int relay_open(const char *url) { (void)url; return 0; }
static void relay_send(const uint8_t *data, int len) { (void)data; (void)len; }
static int relay_recv(uint8_t *out, int max) { (void)out; (void)max; return -1; }
static void relay_close(void) {}
#endif

/* ---- the socket ---- */

static void send_raw(const uint8_t *pkt, int len, const TlAddr *to) {
    if (is_relay(to)) {
        uint8_t out[TL_HEAD + TL_PIECE + 1];
        if (!SDL_GetAtomicInt(&L.relayOn) || len > TL_HEAD + TL_PIECE) {
            return;
        }
        if (L.hosting) {
            out[0] = (uint8_t)(ntohs(to->v4.sin_port) - 1);
            SDL_memcpy(out + 1, pkt, (size_t)len);
            relay_send(out, len + 1);
        } else {
            relay_send(pkt, len);
        }
        return;
    }
    if (to->sa.sa_family == AF_INET6) {
        if (L.sock6 >= 0) {
            sendto(L.sock6, (const char *)pkt, len, 0, &to->sa, addr_len(to));
        }
    } else if (!SDL_GetAtomicInt(&sAllocating) && Relay_Active() && Relay_IsPeer(&to->v4)) {
        Relay_Send(L.sock, pkt, len, &to->v4);
    } else {
        sendto(L.sock, (const char *)pkt, len, 0, &to->sa, addr_len(to));
    }
}

static void send_packet(Conn *c, int type, uint32_t seq, const uint8_t *data, int len, const TlAddr *to) {
    uint8_t pkt[TL_HEAD + TL_PIECE];
    uint32_t sack = 0;
    int i;
    for (i = 0; i < 32; i++) {
        const Piece *r = &c->rx[(c->rcvNext + 1 + (uint32_t)i) & (TL_WINDOW - 1)];
        if (r->used) {
            sack |= 1u << i;
        }
    }
    put32(pkt, TL_MAGIC);
    pkt[4] = (uint8_t)type;
    put32(pkt + 5, c->id);
    put32(pkt + 9, seq);
    put32(pkt + 13, c->rcvNext);
    put32(pkt + 17, sack);
    pkt[21] = (uint8_t)(len >> 8);
    pkt[22] = (uint8_t)len;
    put32(pkt + 23, L.tag);
    if (len > 0) {
        SDL_memcpy(pkt + TL_HEAD, data, (size_t)len);
    }
    send_raw(pkt, TL_HEAD + len, to != NULL ? to : &c->to);
    if (to == NULL && c->state == TK_LINK_OPEN) {
        /* Nothing has come from the other side for a while: its way may have closed (a phone's network changing, a
           router forgetting the mapping, the direct way going mute in the middle of a match: "Input from a player
           stopped arriving"). Send by the other way too until something comes back: through the room's relay if the
           direct way went quiet, directly if the relay did. Whatever answers first becomes the way again
           (take_packet); a piece that arrives twice is only taken once. */
        Uint64 now = SDL_GetTicks();
        const Piece *oldest = &c->tx[c->sndUna & (TL_WINDOW - 1)];
        /* (or only our way to it failing: it is heard, but what we send stays unacknowledged) */
        int quiet = now - c->lastIn > 1200;
        if (quiet || (c->sndUna != c->sndNext && oldest->used && oldest->tries >= 3)) {
            TlAddr other;
            int have = 0;
            if (!is_relay(&c->to) && SDL_GetAtomicInt(&L.relayOn)) {
                other = relay_addr(L.hosting ? c->member : 0);
                have = 1;
            } else if (is_relay(&c->to) && c->haveDirect) {
                other = c->direct;
                have = 1;
            }
            if (have) {
                send_raw(pkt, TL_HEAD + len, &other);
                if (!c->quietTold && quiet) {
                    c->quietTold = 1;
                    fprintf(stderr, "taglink: connection with player %d quiet for %u ms: also sending %s\n", c->member,
                            (unsigned)(now - c->lastIn), is_relay(&other) ? "through the relay" : "directly");
                }
            }
        } else if (c->quietTold == 2) {
            c->quietTold = 0;
        }
    }
    c->lastOut = SDL_GetTicks();
    c->ackDue = 0;
}

static Conn *conn_by_id(uint32_t id) {
    int i;
    for (i = 0; i < TL_SLOTS; i++) {
        if (L.conn[i].state != TK_LINK_FREE && L.conn[i].id == id) {
            return &L.conn[i];
        }
    }
    return NULL;
}

static void conn_reset(Conn *c) {
    SDL_memset(c, 0, sizeof(*c));
    c->rto = 500; /* (the room's relay alone is 300 ms and more a round trip: see take_ack) */
    c->srtt = 150;
    c->rttvar = 80;
    c->fastSeq = 0xFFFFFFFFu;
}

/* The way to answer by, from where a packet came. The direct way is kept whenever it works: the room's relay (the
   Worker's Durable Object) adds 100-300 ms each way, and before, any one packet through it (a SYN sent every way at
   the start, a copy sent while a way was quiet) moved the whole connection onto it until a direct one came again:
   the ping went up and down and inputs came late. The relay only while nothing has come directly for 1.2 s. */
static void take_way(Conn *c, const TlAddr *from) {
    Uint64 now = SDL_GetTicks();
    int relay = is_relay(from);
    if (!relay) {
        c->direct = *from;
        c->haveDirect = 1;
        c->lastDirectIn = now;
        c->to = *from;
    } else if (!c->haveDirect || now - c->lastDirectIn > 1200) {
        c->to = *from;
    }
    if (is_relay(&c->to) != c->viaRelay) {
        static int told;
        c->viaRelay = is_relay(&c->to);
        if (told < 40) {
            told++;
            fprintf(stderr, "taglink: connection with player %d now %s\n", c->member, c->viaRelay ? "through the room's relay (slower)" : "direct");
        }
    }
}

/* An acknowledgement: everything before `ack`, and of the 32 after it those in `sack`. */
static void take_ack(Conn *c, uint32_t ack, uint32_t sack) {
    Uint64 now = SDL_GetTicks();
    while ((int32_t)(ack - c->sndUna) > 0 && c->sndUna != c->sndNext) {
        Piece *p = &c->tx[c->sndUna & (TL_WINDOW - 1)];
        /* A round trip is measured on a piece sent once (one sent again could be answered for either copy). But
           when the round trip is longer than the time before a piece goes again (the relay: 300 ms and more,
           against 250 at the start), every piece went twice and nothing was ever measured: the connection stayed
           on its first guess, every input sent twice, for the whole match. Then, after 2 s without a measure,
           the time from the first sending counts: longer than the truth, so the next pieces go once and are
           measured exactly. */
        if (p->used && (p->tries == 1 || now - c->lastSample > 2000)) {
            int rtt = (int)(now - (p->tries == 1 ? p->sentAt : p->firstAt)), err = rtt - c->srtt;
            c->lastSample = now;
            c->srtt = (c->srtt * 7 + rtt) / 8;
            c->rttvar = (c->rttvar * 3 + (err < 0 ? -err : err)) / 4;
            /* (round trip + 4 x its variation, as TCP: twice the round trip made every lost input wait far longer
               than the rollback window, the match held still meanwhile) */
            c->rto = SDL_clamp(c->srtt + SDL_max(4 * c->rttvar, c->srtt / 4) + 10, 60, 1000); /* (a quarter of the round trip at least: a steady relay's spread is near 0) */
        }
        p->used = 0;
        c->sndUna++;
    }
    {
        int i;
        for (i = 0; i < 32; i++) {
            uint32_t seq = ack + 1 + (uint32_t)i;
            if ((sack & (1u << i)) && (int32_t)(seq - c->sndUna) >= 0 && (int32_t)(c->sndNext - seq) > 0) {
                c->tx[seq & (TL_WINDOW - 1)].used = 0;
            }
        }
    }
    while (c->sndUna != c->sndNext && !c->tx[c->sndUna & (TL_WINDOW - 1)].used) {
        c->sndUna++;
    }
    /* The other side has later pieces but not the oldest one: it was lost. Sent again now, once per loss, instead
       of after the whole retransmission time; the stream is in order, so every input after it waited too. */
    if (sack != 0 && c->sndUna != c->sndNext) {
        Piece *p = &c->tx[c->sndUna & (TL_WINDOW - 1)];
        Uint64 wait = (Uint64)(c->srtt / 4 + 5);
        if (p->used && now - p->sentAt >= wait && (c->fastSeq != c->sndUna || now - c->fastAt >= (Uint64)c->srtt)) {
            c->fastSeq = c->sndUna;
            c->fastAt = now;
            p->sentAt = now;
            p->tries++;
            send_packet(c, T_DATA, c->sndUna, p->data, p->len, NULL);
        }
    }
}

static void take_data(Conn *c, uint32_t seq, const uint8_t *data, int len) {
    if ((int32_t)(seq - c->rcvNext) < 0 || (int32_t)(seq - c->rcvNext) >= TL_WINDOW) {
        return; /* (old: acknowledged again below; too far ahead: sent again later) */
    }
    {
        Piece *r = &c->rx[seq & (TL_WINDOW - 1)];
        if (!r->used) {
            SDL_memcpy(r->data, data, (size_t)len);
            r->len = len;
            r->used = 1;
        }
    }
    for (;;) {
        Piece *r = &c->rx[c->rcvNext & (TL_WINDOW - 1)];
        if (!r->used || c->inN + r->len > TL_INBOX) {
            break;
        }
        SDL_memcpy(c->inbox + c->inN, r->data, (size_t)r->len);
        c->inN += r->len;
        r->used = 0;
        c->rcvNext++;
    }
}

/* One packet of the room (from the socket, or from the relay: tag_link_relay below), from `from`. */
static void take_packet(uint8_t *pkt, int len, TlAddr from) {
    int type;
    uint32_t id;
    Conn *c;
    if (len < TL_HEAD || get32(pkt) != TL_MAGIC || get32(pkt + 23) != L.tag) {
        return;
    }
    type = pkt[4];
    id = get32(pkt + 5);
    {
        int dlen = pkt[21] << 8 | pkt[22];
        if (dlen > len - TL_HEAD || dlen > TL_PIECE) {
            return;
        }
        c = conn_by_id(id);
        if (type == T_PUNCH) { /* (the owner opening its router to us: answer at once, from where it came) */
            if (!L.hosting && (L.conn[0].state == TK_LINK_CONNECTING || (L.conn[0].state == TK_LINK_OPEN && is_relay(&L.conn[0].to)))) {
                send_packet(&L.conn[0], T_SYN, (uint32_t)L.myId, NULL, 0, &from);
            }
            return;
        }
        if (type == T_SYN && L.hosting) {
            int fresh = c == NULL;
            if (c == NULL) {
                int i;
                for (i = 0; i < TL_SLOTS && L.conn[i].state != TK_LINK_FREE; i++) {
                }
                if (i == TL_SLOTS) {
                    return;
                }
                c = &L.conn[i];
                conn_reset(c);
                c->id = id;
                c->member = (int)get32(pkt + 9);
                c->state = TK_LINK_OPEN;
                c->opened = SDL_GetTicks();
                fprintf(stderr, "taglink: player %d connected (%s)\n", c->member,
                        is_relay(&from) ? "through the room's relay" : from.sa.sa_family == AF_INET6 ? "directly, IPv6" :
                        Relay_IsPeer(&from.v4) ? "through the relay" : "directly");
                c->viaRelay = is_relay(&from);
            }
            if (fresh || is_relay(&c->to) == is_relay(&from)) {
                take_way(c, &from);
            } else if (!is_relay(&from)) {
                /* (a SYN by the direct way on a relayed connection: answered by that way; the connection moves when
                   the player's packets come directly too, which shows the way works both ways) */
                c->direct = from;
                c->haveDirect = 1;
            }
            c->lastIn = SDL_GetTicks();
            send_packet(c, T_SYNACK, 0, NULL, 0, &from);
            return;
        }
        if (c == NULL) {
            return;
        }
        if (c->quietTold == 1 && SDL_GetTicks() - c->lastIn > 1200) {
            c->quietTold = 2; /* (said once per quiet spell: the next "quiet" line comes when it is over) */
            fprintf(stderr, "taglink: connection with player %d answers again (%s, after %u ms)\n", c->member,
                    is_relay(&from) ? "through the relay" : "directly", (unsigned)(SDL_GetTicks() - c->lastIn));
        }
        take_way(c, &from); /* (the way that works now: directly if it does, else the relay) */
        c->lastIn = SDL_GetTicks();
        if (type == T_SYNACK && c->state == TK_LINK_CONNECTING) {
            c->state = TK_LINK_OPEN;
            c->opened = SDL_GetTicks();
            fprintf(stderr, "taglink: connected to the room's owner (%s)\n", is_relay(&c->to) ? "through the room's relay" : "directly");
            return;
        }
        if (type == T_FIN) {
            c->state = TK_LINK_CLOSED;
            return;
        }
        take_ack(c, get32(pkt + 13), get32(pkt + 17));
        if (type == T_DATA && c->state == TK_LINK_OPEN) {
            take_data(c, get32(pkt + 9), pkt + TL_HEAD, dlen);
            send_packet(c, T_ACK, 0, NULL, 0, NULL);
        }
    }
}

/* What came through the room's relay. */
static void relay_poll(void) {
    uint8_t buf[2048];
    int guard;
    if (!SDL_GetAtomicInt(&L.relayOn)) {
        return;
    }
    for (guard = 0; guard < 512; guard++) {
        int n = relay_recv(buf, sizeof(buf));
        if (n < 0) {
            SDL_SetAtomicInt(&L.relayOn, 0);
            fprintf(stderr, "taglink: the room's relay closed\n");
            return;
        }
        if (n == 0) {
            return;
        }
        if (L.hosting) {
            if (n > 1) {
                take_packet(buf + 1, n - 1, relay_addr(buf[0]));
            }
        } else {
            take_packet(buf, n, relay_addr(0));
        }
    }
}

static void receive(void) {
    uint8_t buf[2048];
    int guard;
    if (SDL_GetAtomicInt(&L.busy) && !SDL_GetAtomicInt(&sAllocating)) {
        return; /* (the service's thread reads the socket itself: STUN at the room's start) */
    }
    if (SDL_GetAtomicInt(&sAllocating)) {
        for (guard = 0; guard < 512; guard++) {
            TlAddr from;
            socklen_t fl = sizeof(from);
            int n;
            SDL_memset(&from, 0, sizeof(from));
            n = (int)recvfrom(L.sock, (char *)buf, sizeof(buf), 0, &from.sa, &fl);
            if (n <= 0) {
                break;
            }
            if (n >= TL_HEAD && get32(buf) == TL_MAGIC) {
                take_packet(buf, n, from); /* (no relay is in use yet: what comes is direct) */
            } else if (from.sa.sa_family == AF_INET) {
                box_put(buf, n, &from.v4);
            }
        }
    }
    for (guard = 0; !SDL_GetAtomicInt(&sAllocating) && guard < 512; guard++) {
        TlAddr from;
        socklen_t fl = sizeof(from);
        uint8_t *pkt = buf;
        int n, len, r;
        SDL_memset(&from, 0, sizeof(from));
        n = (int)recvfrom(L.sock, (char *)buf, sizeof(buf), 0, &from.sa, &fl);
        len = n;
        if (n <= 0) {
            break;
        }
        r = Relay_Unwrap(buf, n, &from.v4, &pkt, &len);
        if (r == 0) {
            continue;
        }
        if (r == 2) {
            Relay_SawDirect(&from.v4);
        }
        take_packet(pkt, len, from);
    }
    for (guard = 0; L.sock6 >= 0 && guard < 512; guard++) { /* (IPv6: no TURN wrapping there) */
        TlAddr from;
        socklen_t fl = sizeof(from);
        int n;
        SDL_memset(&from, 0, sizeof(from));
        n = (int)recvfrom(L.sock6, (char *)buf, sizeof(buf), 0, &from.sa, &fl);
        if (n <= 0) {
            break;
        }
        if (from.sa.sa_family == AF_INET6) {
            take_packet(buf, n, from);
        }
    }
    relay_poll();
}

/* Pieces not acknowledged in time go again; a quiet connection says it is still there; a silent one is closed. */
static void timers(void) {
    Uint64 now = SDL_GetTicks();
    int i;
    for (i = 0; i < TL_SLOTS; i++) {
        Conn *c = &L.conn[i];
        uint32_t s;
        int sent = 0;
        if (c->state == TK_LINK_CONNECTING) {
            if (now - c->lastSyn >= 200) {
                int k;
                c->lastSyn = now;
                SDL_LockMutex(L.lock);
                for (k = 0; k < L.hostCands; k++) {
                    send_packet(c, T_SYN, (uint32_t)L.myId, NULL, 0, &L.hostCand[k]);
                }
                SDL_UnlockMutex(L.lock);
                if (SDL_GetAtomicInt(&L.relayOn)) {
                    TlAddr r = relay_addr(0);
                    send_packet(c, T_SYN, (uint32_t)L.myId, NULL, 0, &r);
                }
            }
            continue;
        }
        if (c->state != TK_LINK_OPEN) {
            continue;
        }
        if (!L.hosting && is_relay(&c->to) && now - c->lastSyn >= 1000) {
            /* Connected through the room's relay (300 ms and more a round trip from Brazil): the direct way is
               still tried, once a second, for as long as the connection lasts. Before, the SYNs to the owner's
               addresses stopped at the first answer, which was the relay's when it came first, and the match
               stayed on the relay. The owner's SYNACK by the direct way moves the connection there (take_way). */
            int k;
            c->lastSyn = now;
            SDL_LockMutex(L.lock);
            for (k = 0; k < L.hostCands; k++) {
                if (!is_relay(&L.hostCand[k])) {
                    send_packet(c, T_SYN, (uint32_t)L.myId, NULL, 0, &L.hostCand[k]);
                }
            }
            SDL_UnlockMutex(L.lock);
        }
        /* (tag_net.c decides when a player is gone, by the match's phase: up to two minutes while a battle loads, when a
           game does not answer. Closed here at 20 s, a guest waiting for a slow host lost the room during its load) */
        if (now - c->lastIn > 60000) {
            fprintf(stderr, "taglink: connection %d silent for 60 s: closed\n", i);
            c->state = TK_LINK_CLOSED;
            continue;
        }
        for (s = c->sndUna; s != c->sndNext && sent < 64; s++) {
            Piece *p = &c->tx[s & (TL_WINDOW - 1)];
            int wait = c->rto << (p->tries > 4 ? 4 : p->tries - 1);
            if (p->used && now - p->sentAt >= (Uint64)(wait > 3000 ? 3000 : wait)) {
                p->sentAt = now;
                p->tries++;
                send_packet(c, T_DATA, s, p->data, p->len, NULL);
                sent++;
            }
        }
        if (now - c->lastOut > 500) {
            send_packet(c, T_ACK, 0, NULL, 0, NULL);
        }
    }
}

/* The owner opens its router to the players the service told of, and lets their packets through its relay. */
static void punch(void) {
    Uint64 now = SDL_GetTicks();
    int i, k, j;
    if (!L.hosting || now - L.lastPunch < 300) {
        return;
    }
    L.lastPunch = now;
    SDL_LockMutex(L.lock);
    for (i = 0; i < L.members; i++) {
        Member *m = &L.member[i];
        int connected = 0, relayed = 0;
        for (j = 0; j < TL_SLOTS; j++) {
            if (L.conn[j].state == TK_LINK_OPEN && L.conn[j].member == m->id) {
                connected = 1;
                relayed = is_relay(&L.conn[j].to);
            }
        }
        if (Relay_Active()) {
            if (m->haveSeen) {
                Relay_Permit(L.sock, &m->seen.v4);
            }
            for (k = 0; k < m->cands; k++) {
                if (m->cand[k].sa.sa_family == AF_INET) {
                    Relay_Permit(L.sock, &m->cand[k].v4);
                }
            }
        }
        /* (a player on the room's relay keeps being punched, once a second: the direct way may open later) */
        if (connected ? !relayed || (now / 300) % 3 != 0 : now - m->since > 60000) {
            continue;
        }
        for (k = 0; k < m->cands; k++) {
            uint8_t pkt[TL_HEAD];
            SDL_memset(pkt, 0, sizeof(pkt));
            put32(pkt, TL_MAGIC);
            pkt[4] = T_PUNCH;
            put32(pkt + 23, L.tag);
            if (m->cand[k].sa.sa_family == AF_INET6) {
                if (L.sock6 >= 0) {
                    sendto(L.sock6, (const char *)pkt, TL_HEAD, 0, &m->cand[k].sa, addr_len(&m->cand[k]));
                }
            } else {
                sendto(L.sock, (const char *)pkt, TL_HEAD, 0, &m->cand[k].sa, addr_len(&m->cand[k]));
            }
        }
    }
    SDL_UnlockMutex(L.lock);
}

/* ---- the service (a thread of its own: its requests take a while) ---- */

static int post(const char *what, const char *json, char *out, int size) {
    char url[320];
    SDL_snprintf(url, sizeof(url), "%s/v1/%s", L.url, what);
    out[0] = 0;
    return TagLink_Http(url, json, out, size);
}

static void say_error(const char *answer, const char *what) {
    char e[64];
    if (!json_str(answer, "error", e, sizeof(e))) {
        set_status(answer[0] ? "The online service gave an unexpected answer." : "The online service could not be reached.");
    } else if (SDL_strcmp(e, "no room") == 0) {
        set_status(L.hosting ? "The online room was closed." : "No room has that code.");
    } else if (SDL_strcmp(e, "version") == 0) {
        set_status("That room is for another version of the mod.");
    } else if (SDL_strcmp(e, "full") == 0) {
        set_status("That room is full.");
    } else {
        char line[120];
        SDL_snprintf(line, sizeof(line), "The online service refused (%s: %s).", what, e);
        set_status(line);
    }
}

/* This device's public IPv6 address and the IPv6 socket's port, "2804:...:41234" ("" none): the address the system
   would send from to the internet (a UDP socket "connected" to a public address sends nothing). Link-local, loopback
   and unique-local addresses are not reachable from another network. */
static void own_ipv6(char *out, int size) {
    int s;
    struct sockaddr_in6 to, me, bound;
    socklen_t ml = sizeof(me), bl = sizeof(bound);
    char text[64];
    out[0] = 0;
    if (L.sock6 < 0 || getsockname(L.sock6, (struct sockaddr *)&bound, &bl) != 0) {
        return;
    }
    s = (int)socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        return;
    }
    SDL_memset(&to, 0, sizeof(to));
    to.sin6_family = AF_INET6;
    to.sin6_port = htons(53);
    inet_pton(AF_INET6, "2001:4860:4860::8888", &to.sin6_addr);
    if (connect(s, (struct sockaddr *)&to, sizeof(to)) == 0 && getsockname(s, (struct sockaddr *)&me, &ml) == 0 &&
        (me.sin6_addr.s6_addr[0] & 0xE0) == 0x20 && /* (global unicast, 2000::/3) */
        inet_ntop(AF_INET6, &me.sin6_addr, text, sizeof(text)) != NULL) {
        SDL_snprintf(out, size, "%s:%d", text, (int)ntohs(bound.sin6_port));
    }
    closesocket(s);
}

/* This game's addresses as JSON strings ("lan", the internet's view), with the relay's if it holds one. */
static void my_addrs(char *out, int size, const char *pub, const char *lan, const char *relay) {
    char v6[80];
    own_ipv6(v6, sizeof(v6));
    out[0] = 0;
    SDL_strlcat(out, "[", size);
    if (v6[0]) { SDL_strlcat(out, "\"", size); SDL_strlcat(out, v6, size); SDL_strlcat(out, "\"", size); if (pub[0] || lan[0] || (relay != NULL && relay[0])) SDL_strlcat(out, ",", size); }
    if (pub[0]) { SDL_strlcat(out, "\"", size); SDL_strlcat(out, pub, size); SDL_strlcat(out, "\"", size); }
    if (lan[0]) { if (pub[0]) SDL_strlcat(out, ",", size); SDL_strlcat(out, "\"", size); SDL_strlcat(out, lan, size); SDL_strlcat(out, "\"", size); }
    if (relay != NULL && relay[0]) { if (pub[0] || lan[0]) SDL_strlcat(out, ",", size); SDL_strlcat(out, "\"relay=", size); SDL_strlcat(out, relay, size); SDL_strlcat(out, "\"", size); }
    SDL_strlcat(out, "]", size);
}

/* The members of the owner's poll answer. */
static void take_members(const char *json) {
    const char *p = SDL_strstr(json, "\"members\":[");
    Member found[TL_MEMBERS];
    int n = 0, i, k;
    if (p == NULL) {
        return;
    }
    p += 11;
    while (n < TL_MEMBERS) {
        const char *obj = SDL_strchr(p, '{'), *end;
        char one[600], seen[64];
        if (obj == NULL || (end = SDL_strchr(obj, '}')) == NULL || end - obj >= (int)sizeof(one)) {
            break;
        }
        SDL_memcpy(one, obj, (size_t)(end - obj + 1));
        one[end - obj + 1] = 0;
        SDL_memset(&found[n], 0, sizeof(found[n]));
        found[n].id = json_int(one, "id", 0);
        found[n].watch = SDL_strstr(one, "\"watch\":true") != NULL;
        found[n].cands = json_addrs(one, "addrs", found[n].cand, TL_CANDS);
        if (json_str(one, "seen", seen, sizeof(seen))) {
            SDL_memset(&found[n].seen, 0, sizeof(found[n].seen));
            found[n].seen.v4.sin_family = AF_INET;
            found[n].haveSeen = inet_pton(AF_INET, seen, &found[n].seen.v4.sin_addr) == 1; /* (an IPv6 one: not for TURN) */
        }
        n++;
        p = end + 1;
    }
    SDL_LockMutex(L.lock);
    for (i = 0; i < n; i++) { /* (a member already known keeps when it was first heard of) */
        found[i].since = SDL_GetTicks();
        for (k = 0; k < L.members; k++) {
            if (L.member[k].id == found[i].id) {
                found[i].since = L.member[k].since;
            }
        }
    }
    SDL_memcpy(L.member, found, sizeof(found[0]) * (size_t)n);
    L.members = n;
    SDL_UnlockMutex(L.lock);
}

#define STOPPED() (SDL_GetAtomicInt(&L.gen) != mine)
static int SDLCALL service_thread(void *arg) {
    char pub[64] = "", lan[64] = "", relay[64] = "", addrs[256], body[1024], answer[4096], name[48], code[16], key[48];
    int mine = (int)(intptr_t)arg, sock = L.sock, hosting = L.hosting;
    struct sockaddr_in me;
    socklen_t ml = sizeof(me);
    Uint64 lastPoll = 0, relayAsked = 0, relayRetry = 0;
    int relayRefused = 0, relayWait = 5000, pollEvery = 1000, got;
    char ws[400];
    getsockname(L.sock, (struct sockaddr *)&me, &ml);
    SDL_SetAtomicInt(&L.busy, 1);
    set_status("Finding this device's internet address...");
    TagLink_Stun(L.sock, pub, sizeof(pub));
    if (STOPPED()) { /* (the room is gone, another may have begun: nothing below is this thread's any more) */
        return 0;
    }
    SDL_SetAtomicInt(&L.busy, 0);
    TagLink_Lan(ntohs(me.sin_port), lan, sizeof(lan));
    { char v6[80]; own_ipv6(v6, sizeof(v6));
      fprintf(stderr, "taglink: addresses: internet %s, own network %s, IPv6 %s\n", pub[0] ? pub : "(not found)", lan[0] ? lan : "(none)", v6[0] ? v6 : "(none)"); }
    my_addrs(addrs, sizeof(addrs), pub, lan, NULL);
    SDL_strlcpy(name, L.name, sizeof(name));
    if (L.hosting) {
        set_status("Creating the online room...");
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"name\":\"%s\",\"addrs\":%s}", L.version, name, addrs);
        if (!post("host", body, answer, sizeof(answer)) || !json_str(answer, "code", code, sizeof(code)) || !json_str(answer, "key", key, sizeof(key))) {
            if (!STOPPED()) {
                say_error(answer, "host");
            }
            return 0;
        }
        if (STOPPED()) { /* (given up while the service answered: its room is not left behind, nor its code put on the next room) */
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", code, key);
            post("leave", body, answer, sizeof(answer));
            return 0;
        }
        SDL_strlcpy(L.code, code, sizeof(L.code));
        SDL_strlcpy(L.key, key, sizeof(L.key));
        L.tag = (uint32_t)SDL_atoi(L.code);
        L.codeReady = 1;
        {
            char line[96];
            SDL_snprintf(line, sizeof(line), "Online room code: %s", L.code);
            set_status(line);
        }
        fprintf(stderr, "taglink: online room %s open\n", L.code);
    } else {
        set_status("Joining the online room...");
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"code\":\"%s\",\"name\":\"%s\",\"addrs\":%s,\"watch\":%s}", L.version, L.code, name, addrs, L.watch ? "true" : "false");
        SDL_strlcpy(code, L.code, sizeof(code));
        if (!post("join", body, answer, sizeof(answer)) || !json_str(answer, "key", key, sizeof(key))) {
            if (!STOPPED()) { /* (a stopped room's answer must not close the connection of the room after it) */
                say_error(answer, "join");
                SDL_LockMutex(L.lock);
                L.conn[0].state = TK_LINK_CLOSED;
                SDL_UnlockMutex(L.lock);
            }
            return 0;
        }
        if (STOPPED()) {
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", code, key);
            post("leave", body, answer, sizeof(answer));
            return 0;
        }
        SDL_strlcpy(L.key, key, sizeof(L.key));
        L.myId = json_int(answer, "id", 1);
        SDL_LockMutex(L.lock);
        L.hostCands = json_addrs(answer, "addrs", L.hostCand, TL_CANDS);
        SDL_UnlockMutex(L.lock);
        L.codeReady = 1;
        set_status("Reaching the room's owner...");
    }
    SDL_snprintf(ws, sizeof(ws), "%s/v1/ws?code=%s&key=%s", L.url, code, key);
    { /* the room's relay at the service: for players whose networks keep them from reaching each other directly */
        if (relay_open(ws)) {
            if (STOPPED()) {
                if (!L.active) {
                    relay_close();
                }
            } else {
                SDL_SetAtomicInt(&L.relayOn, 1);
                fprintf(stderr, "taglink: the room's relay is open\n");
            }
        } else {
            fprintf(stderr, "taglink: the room's relay could not be opened (direct connections only)\n");
        }
    }
    while (!STOPPED()) {
        Uint64 now = SDL_GetTicks();
        /* The relay is the way left when the direct one fails, so it comes back when it closes (a phone's network drops
           a connection, the service restarts); it stayed closed for the rest of the room. 5 s, then up to a minute. */
        if (!SDL_GetAtomicInt(&L.relayOn) && now - relayRetry >= (Uint64)relayWait) {
            relayRetry = now;
            if (relay_open(ws)) {
                if (STOPPED()) {
                    if (!L.active) {
                        relay_close();
                    }
                    break;
                }
                SDL_SetAtomicInt(&L.relayOn, 1);
                relayWait = 5000;
                fprintf(stderr, "taglink: the room's relay is open again\n");
            } else if (relayWait < 60000) {
                relayWait *= 2;
            }
            now = SDL_GetTicks();
        }
        if (now - lastPoll < (Uint64)pollEvery) {
            SDL_Delay(50);
            continue;
        }
        lastPoll = now;
        SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", code, key);
        if (!post("poll", body, answer, sizeof(answer)) || STOPPED()) {
            continue; /* (the service was not reached this time: the room goes on) */
        }
        if (SDL_strstr(answer, "\"error\"") != NULL) {
            say_error(answer, "poll");
            if (!L.hosting) {
                break;
            }
            continue;
        }
        if (L.hosting) {
            int i, j, waiting = 0, pending = 0;
            take_members(answer);
            SDL_LockMutex(L.lock);
            for (i = 0; i < L.members; i++) { /* someone heard of for 4 s and still not connected: the relay is wanted */
                int connected = 0;
                for (j = 0; j < TL_SLOTS; j++) {
                    if (L.conn[j].state == TK_LINK_OPEN && L.conn[j].member == L.member[i].id) {
                        connected = 1;
                    }
                }
                pending |= !connected;
                if (!connected && now - L.member[i].since > 4000) {
                    waiting = 1;
                }
            }
            SDL_UnlockMutex(L.lock);
            /* The service is only needed to meet: every second from every player of every room, for a whole match, spent
               the free plan's day (a room of three: 3 requests and 5 rows written a second). Asked often while someone is
               arriving, every 2 s to notice who comes. */
            pollEvery = pending ? 1000 : 2000;
            if (waiting && !relay[0] && !relayRefused && now - relayAsked > 30000) {
                char server[128], user[160], pass[160];
                relayAsked = now;
                post("relay", body, answer, sizeof(answer));
                if (json_str(answer, "server", server, sizeof(server)) && json_str(answer, "username", user, sizeof(user)) &&
                    json_str(answer, "credential", pass, sizeof(pass))) {
                    if (STOPPED()) {
                        break;
                    }
                    SDL_LockMutex(L.lock);
                    sBoxN = 0;
                    SDL_UnlockMutex(L.lock);
                    SDL_SetAtomicInt(&sAllocating, 1);
                    SDL_SetAtomicInt(&L.busy, 1);
                    got = Relay_AllocateVia(sock, server, user, pass, relay, sizeof(relay), box_take);
                    SDL_SetAtomicInt(&sAllocating, 0);
                    if (got) {
                        fprintf(stderr, "taglink: relay address %s\n", relay);
                        my_addrs(addrs, sizeof(addrs), pub, lan, relay);
                        SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":%s}", code, key, addrs);
                        SDL_SetAtomicInt(&L.busy, 0);
                        post("addrs", body, answer, sizeof(answer));
                    } else {
                        SDL_SetAtomicInt(&L.busy, 0);
                        relay[0] = 0;
                        fprintf(stderr, "taglink: the relay gave no address\n");
                    }
                } else {
                    relayRefused = 1;
                    fprintf(stderr, "taglink: no relay: %s\n", answer);
                }
            }
        } else {
            TlAddr c[TL_CANDS];
            int n = json_addrs(answer, "addrs", c, TL_CANDS);
            /* (connected directly: only to stay in the room's list, whose limit is 45 s) */
            pollEvery = L.conn[0].state == TK_LINK_OPEN && !is_relay(&L.conn[0].to) ? 15000 : 1000;
            if (n > 0) {
                SDL_LockMutex(L.lock);
                SDL_memcpy(L.hostCand, c, sizeof(c[0]) * (size_t)n);
                L.hostCands = n;
                SDL_UnlockMutex(L.lock);
            }
        }
    }
    SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", code, key);
    post("leave", body, answer, sizeof(answer)); /* (the room's owner leaving closes it) */
    (void)hosting;
    return 0;
}

/* ---- for tag_net.c ---- */

/* The service's address: <game folder>/tag_online_url.txt (one line), else the one built in. */
static int service_url(void) {
    char path[1200];
    FILE *f;
    L.url[0] = 0;
    SDL_snprintf(path, sizeof(path), "%s/tag_online_url.txt", Port_FileRoot());
    f = fopen(path, "r");
    if (f != NULL) {
        if (fgets(L.url, sizeof(L.url), f) == NULL) {
            L.url[0] = 0;
        }
        fclose(f);
    }
    if (!L.url[0]) {
        SDL_strlcpy(L.url, TAG_ONLINE_URL, sizeof(L.url));
    }
    while (L.url[0] && (L.url[SDL_strlen(L.url) - 1] <= ' ' || L.url[SDL_strlen(L.url) - 1] == '/')) {
        L.url[SDL_strlen(L.url) - 1] = 0;
    }
    return SDL_strncmp(L.url, "https://", 8) == 0;
}

static int start(int hosting, const char *code, const char *name, int version, int watch) {
    struct sockaddr_in any;
    TagLink_Stop();
    if (L.lock == NULL) {
        L.lock = SDL_CreateMutex();
    }
    if (!service_url()) {
        set_status("No online service is set for Tag Team rooms.");
        return 0;
    }
    L.sock = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (L.sock < 0) {
        set_status("Could not open the online connection.");
        return 0;
    }
    SDL_memset(&any, 0, sizeof(any));
    any.sin_family = AF_INET;
    if (bind(L.sock, (struct sockaddr *)&any, sizeof(any)) != 0) {
        closesocket(L.sock);
        L.sock = -1;
        return 0;
    }
#ifdef _WIN32
    { u_long one = 1; ioctlsocket(L.sock, FIONBIO, &one); }
#else
    fcntl(L.sock, F_SETFL, fcntl(L.sock, F_GETFL, 0) | O_NONBLOCK);
#endif
    L.sock6 = -1;
    if (getenv("BT3_TAG_NO_IPV6") == NULL) { /* a second socket for IPv6 (any failure: IPv4 only, as before) */
        int s6 = (int)socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP), one = 1;
        struct sockaddr_in6 any6;
        SDL_memset(&any6, 0, sizeof(any6));
        any6.sin6_family = AF_INET6;
        if (s6 >= 0 && setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&one, sizeof(one)) == 0 &&
            bind(s6, (struct sockaddr *)&any6, sizeof(any6)) == 0) {
#ifdef _WIN32
            { u_long nb = 1; ioctlsocket(s6, FIONBIO, &nb); }
#else
            fcntl(s6, F_SETFL, fcntl(s6, F_GETFL, 0) | O_NONBLOCK);
#endif
            L.sock6 = s6;
        } else if (s6 >= 0) {
            closesocket(s6);
        }
    }
    L.hosting = hosting;
    L.active = 1;
    L.version = version;
    L.watch = watch;
    L.codeReady = 0;
    L.members = L.hostCands = 0;
    L.key[0] = 0;
    SDL_strlcpy(L.code, code != NULL ? code : "", sizeof(L.code));
    SDL_strlcpy(L.name, name != NULL ? name : "", sizeof(L.name));
    {
        char *c;
        for (c = L.name; *c; c++) {
            if (*c == '"' || *c == '\\' || (unsigned char)*c < ' ') {
                *c = ' ';
            }
        }
    }
    L.tag = hosting ? 0 : (uint32_t)SDL_atoi(L.code);
    SDL_SetAtomicInt(&L.busy, 0);
    if (!hosting) {
        Conn *c = &L.conn[0];
        conn_reset(c);
        c->state = TK_LINK_CONNECTING;
        c->id = (uint32_t)SDL_rand_bits() | 1u;
        c->lastIn = SDL_GetTicks();
    }
    L.thread = SDL_CreateThread(service_thread, "tag-online", (void *)(intptr_t)SDL_GetAtomicInt(&L.gen));
    if (L.thread != NULL) {
        SDL_DetachThread(L.thread); /* (it ends by itself when the room's number changes: TagLink_Stop) */
    }
    return L.thread != NULL;
}

int TagLink_Host(const char *name, int version) {
    return start(1, NULL, name, version, 0);
}

int TagLink_Join(const char *code, const char *name, int version, int watch) {
    return start(0, code, name, version, watch) ? 0 : -1; /* (the owner's connection: slot 0) */
}

void TagLink_Stop(void) {
    int i;
    SDL_AddAtomicInt(&L.gen, 1); /* (the room's thread says "leave" to the service and ends, by itself) */
    if (SDL_GetAtomicInt(&L.relayOn)) {
        SDL_SetAtomicInt(&L.relayOn, 0);
        relay_close();
    }
    L.thread = NULL;
    SDL_SetAtomicInt(&L.busy, 0);
    SDL_SetAtomicInt(&sAllocating, 0);
    if (L.sock >= 0) {
        for (i = 0; i < TL_SLOTS; i++) {
            if (L.conn[i].state == TK_LINK_OPEN) {
                send_packet(&L.conn[i], T_FIN, 0, NULL, 0, NULL);
            }
        }
        if (Relay_Active()) {
            Relay_Close(L.sock);
        }
        closesocket(L.sock);
        L.sock = -1;
    }
    if (L.sock6 >= 0) {
        closesocket(L.sock6);
    }
    L.sock6 = -1;
    for (i = 0; i < TL_SLOTS; i++) {
        conn_reset(&L.conn[i]);
    }
    L.active = L.hosting = L.codeReady = 0;
    L.code[0] = 0;
}

int TagLink_Active(void) {
    return L.active;
}

void TagLink_Pump(void) {
    int i;
    if (!L.active || L.sock < 0) {
        return;
    }
    for (i = 0; i < TL_SLOTS; i++) { /* pieces that came last frame and that no piece of ours carried an answer to */
        if (L.conn[i].state == TK_LINK_OPEN && L.conn[i].ackDue) {
            send_packet(&L.conn[i], T_ACK, 0, NULL, 0, NULL);
        }
    }
    receive();
    if (!SDL_GetAtomicInt(&L.busy)) {
        if (Relay_Active()) {
            Relay_Tick(L.sock);
        }
        punch();
        timers();
    } else if (SDL_GetAtomicInt(&sAllocating)) {
        timers(); /* (pieces lost meanwhile go again; the relay's own upkeep waits for the allocation) */
    }
}

int TagLink_Accept(void) {
    int i;
    for (i = 0; i < TL_SLOTS; i++) {
        if (L.conn[i].state == TK_LINK_OPEN && !L.conn[i].given && L.hosting) {
            L.conn[i].given = 1;
            return i;
        }
    }
    return -1;
}

int TagLink_State(int slot) {
    return slot >= 0 && slot < TL_SLOTS ? L.conn[slot].state : TK_LINK_CLOSED;
}

int TagLink_Send(int slot, const void *data, int len) {
    Conn *c;
    int taken = 0;
    if (slot < 0 || slot >= TL_SLOTS || (c = &L.conn[slot])->state == TK_LINK_CLOSED || c->state == TK_LINK_FREE) {
        return -1;
    }
    if (c->state != TK_LINK_OPEN) {
        return 0;
    }
    while (taken < len && c->sndNext - c->sndUna < TL_WINDOW) {
        Piece *p = &c->tx[c->sndNext & (TL_WINDOW - 1)];
        int n = len - taken > TL_PIECE ? TL_PIECE : len - taken;
        SDL_memcpy(p->data, (const uint8_t *)data + taken, (size_t)n);
        p->len = n;
        p->used = 1;
        p->tries = 1;
        p->sentAt = p->firstAt = SDL_GetTicks();
        send_packet(c, T_DATA, c->sndNext, p->data, n, NULL);
        c->sndNext++;
        taken += n;
    }
    return taken;
}

int TagLink_Recv(int slot, void *out, int max) {
    Conn *c;
    int n;
    if (slot < 0 || slot >= TL_SLOTS) {
        return -1;
    }
    c = &L.conn[slot];
    if (c->inN == 0) {
        return c->state == TK_LINK_CLOSED || c->state == TK_LINK_FREE ? -1 : 0;
    }
    n = c->inN < max ? c->inN : max;
    SDL_memcpy(out, c->inbox, (size_t)n);
    SDL_memmove(c->inbox, c->inbox + n, (size_t)(c->inN - n));
    c->inN -= n;
    return n;
}

void TagLink_Close(int slot) {
    if (slot >= 0 && slot < TL_SLOTS && L.conn[slot].state != TK_LINK_FREE) {
        if (L.conn[slot].state == TK_LINK_OPEN && L.sock >= 0) {
            send_packet(&L.conn[slot], T_FIN, 0, NULL, 0, NULL);
        }
        conn_reset(&L.conn[slot]);
    }
}

void TagLink_Status(char *out, int size, char *code, int codeSize) {
    if (L.lock == NULL) {
        out[0] = 0;
        code[0] = 0;
        return;
    }
    SDL_LockMutex(L.lock);
    SDL_strlcpy(out, L.status, (size_t)size);
    SDL_strlcpy(code, L.active && L.codeReady ? L.code : "", (size_t)codeSize);
    SDL_UnlockMutex(L.lock);
}
