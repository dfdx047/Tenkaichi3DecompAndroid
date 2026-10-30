/*
 * A relay address for the host of a match: a small TURN client (RFC 5766 over UDP, long-term credentials).
 *
 * Why: some pairs of players cannot send to each other directly, whatever the two games do (routers that give every
 * destination another port, mobile networks). A TURN server has a public address; the host asks it for a "relayed
 * address" and tells the other player that one. What the other player sends there, the server passes on to the
 * host inside a "data indication"; what the host wants to send, it hands to the server inside a "send indication".
 * Only the host speaks TURN: for the other player the relayed address is an ordinary address.
 *
 * What is used of the protocol:
 *   Allocate           -> the relayed address (first asked without a login: the server answers 401 with its realm
 *                         and a nonce, and the request is made again with USERNAME, REALM, NONCE and a
 *                         MESSAGE-INTEGRITY: HMAC-SHA1 of the message, keyed with MD5(user ":" realm ":" password))
 *   CreatePermission   -> packets from the other player's address are let through (for five minutes: asked again)
 *   Refresh            -> the allocation lives on (ten minutes at a time: asked again)
 *   Send / Data indication -> the match's first packets, 36 bytes longer each
 *   ChannelBind        -> a channel to the other player's address once it is known; from then on the packets
 *                         travel as channel data, 4 bytes longer each (BT3_RELAY_NOCHAN=1: indications only)
 * A 438 answer (stale nonce) brings a new nonce; the request is made again by the next Relay_Tick.
 *
 * BT3_RELAY_LOG=1 writes what is asked and answered to the log.
 *
 * The server and login come from the matchmaking Worker (port/matchmaking): Cloudflare's TURN service, a name and
 * password good for three hours.
 *
 * Several peers: a lobby's owner (net_lobby.c) holds the relay for up to seven members. Every address the relay
 * delivers from gets a place of its own (RELAY_PEERS) with its own channel, and up to RELAY_PERMITS addresses are let
 * through; a ChannelBind's answer is matched to its request by the transaction. A match of two uses one place.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
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
#include "net_relay.h"

/* ---- MD5 and SHA-1 (for the login's key and the messages' integrity; small textbook versions) ---- */

static uint32_t rol(uint32_t x, int n) { return x << n | x >> (32 - n); }

static void md5(const uint8_t *msg, size_t len, uint8_t out[16]) {
    static const uint8_t s[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                                  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t k[64], h[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    uint8_t buf[256 + 72];
    size_t total, off;
    int i;

    for (i = 0; i < 64; i++) {
        k[i] = (uint32_t)(SDL_fabs(SDL_sin((double)(i + 1))) * 4294967296.0);
    }
    if (len > 256) {
        len = 256; /* (never here: a login is short) */
    }
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    total = (len + 9 + 63) & ~(size_t)63;
    memset(buf + len + 1, 0, total - len - 1);
    for (i = 0; i < 8; i++) {
        buf[total - 8 + i] = (uint8_t)((uint64_t)len * 8 >> (8 * i));
    }
    for (off = 0; off < total; off += 64) {
        uint32_t w[16], a = h[0], b = h[1], c = h[2], d = h[3];
        for (i = 0; i < 16; i++) {
            w[i] = (uint32_t)buf[off + i * 4] | (uint32_t)buf[off + i * 4 + 1] << 8 | (uint32_t)buf[off + i * 4 + 2] << 16 | (uint32_t)buf[off + i * 4 + 3] << 24;
        }
        for (i = 0; i < 64; i++) {
            uint32_t f, t;
            int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
            else { f = c ^ (b | ~d); g = (7 * i) & 15; }
            t = d;
            d = c;
            c = b;
            b = b + rol(a + f + k[i] + w[g], s[i]);
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    for (i = 0; i < 16; i++) {
        out[i] = (uint8_t)(h[i / 4] >> (8 * (i & 3)));
    }
}

typedef struct Sha1 { uint32_t h[5]; uint8_t block[64]; uint64_t len; int fill; } Sha1;

static void sha1_block(Sha1 *c) {
    uint32_t w[80], a = c->h[0], b = c->h[1], d2 = c->h[2], d = c->h[3], e = c->h[4];
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)c->block[i * 4] << 24 | (uint32_t)c->block[i * 4 + 1] << 16 | (uint32_t)c->block[i * 4 + 2] << 8 | c->block[i * 4 + 3];
    }
    for (i = 16; i < 80; i++) {
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    for (i = 0; i < 80; i++) {
        uint32_t f, k, t;
        if (i < 20) { f = (b & d2) | (~b & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ d2 ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & d2) | (b & d) | (d2 & d); k = 0x8F1BBCDCu; }
        else { f = b ^ d2 ^ d; k = 0xCA62C1D6u; }
        t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = d2;
        d2 = rol(b, 30);
        b = a;
        a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += d2; c->h[3] += d; c->h[4] += e;
}

static void sha1_init(Sha1 *c) {
    c->h[0] = 0x67452301u; c->h[1] = 0xEFCDAB89u; c->h[2] = 0x98BADCFEu; c->h[3] = 0x10325476u; c->h[4] = 0xC3D2E1F0u;
    c->len = 0;
    c->fill = 0;
}

static void sha1_add(Sha1 *c, const uint8_t *p, size_t n) {
    c->len += n;
    while (n-- > 0) {
        c->block[c->fill++] = *p++;
        if (c->fill == 64) {
            sha1_block(c);
            c->fill = 0;
        }
    }
}

static void sha1_end(Sha1 *c, uint8_t out[20]) {
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80, zero = 0, lenb[8];
    int i;
    sha1_add(c, &pad, 1);
    while (c->fill != 56) {
        sha1_add(c, &zero, 1);
    }
    for (i = 0; i < 8; i++) {
        lenb[i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    sha1_add(c, lenb, 8);
    for (i = 0; i < 20; i++) {
        out[i] = (uint8_t)(c->h[i / 4] >> (24 - 8 * (i & 3)));
    }
}

static void hmac_sha1(const uint8_t *key, size_t keyLen, const uint8_t *msg, size_t len, uint8_t out[20]) {
    uint8_t k[64], pad[64], inner[20];
    Sha1 c;
    int i;
    memset(k, 0, sizeof(k));
    memcpy(k, key, keyLen > 64 ? 64 : keyLen); /* (the key is an MD5: 16 bytes) */
    for (i = 0; i < 64; i++) { pad[i] = k[i] ^ 0x36; }
    sha1_init(&c);
    sha1_add(&c, pad, 64);
    sha1_add(&c, msg, len);
    sha1_end(&c, inner);
    for (i = 0; i < 64; i++) { pad[i] = k[i] ^ 0x5C; }
    sha1_init(&c);
    sha1_add(&c, pad, 64);
    sha1_add(&c, inner, 20);
    sha1_end(&c, out);
}

/* ---- the client ---- */

#define COOKIE0 0x21
#define COOKIE1 0x12
#define COOKIE2 0xA4
#define COOKIE3 0x42

/* The relay serves the host's other players: one in a match, up to seven in a lobby (its owner holds the relay).
   Each address the relay delivers from gets a place, and a channel of its own once it is known. */
enum { RELAY_PEERS = 12, RELAY_PERMITS = 16 };
typedef struct {
    struct sockaddr_in addr;         /* an address the relay delivered from */
    int via;                         /* ... and packets to it go through the relay */
    int chan, chanBound;             /* its channel's number (0x4000 and up; 0 none), and whether the server has agreed */
    uint64_t lastBind, lastHeard;
    uint8_t txn[12];                 /* the transaction of its last ChannelBind (the answer names no channel) */
} RelayPeer;

static struct {
    int active;
    struct sockaddr_in server;
    char username[160], password[160], realm[128], nonce[256];
    uint8_t key[16];                 /* MD5(username:realm:password) */
    int haveLogin;                   /* realm and nonce are known */
    struct sockaddr_in permit[RELAY_PERMITS]; /* the addresses let through (by IP: the port does not matter) */
    int permits;
    RelayPeer peer[RELAY_PEERS];
    int nextChan;                    /* the next channel number to give (a number stays with its address) */
    uint64_t lastRefresh, lastPermit;
} sR;

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

/* Starts a message of `type`; returns its length so far (20). */
static int msg_begin(uint8_t *m, unsigned type) {
    int i;
    put16(m, type);
    put16(m + 2, 0);
    m[4] = COOKIE0; m[5] = COOKIE1; m[6] = COOKIE2; m[7] = COOKIE3;
    for (i = 8; i < 20; i++) {
        m[i] = (uint8_t)SDL_rand(256);
    }
    return 20;
}

static int msg_attr(uint8_t *m, int at, unsigned type, const void *value, int len) {
    put16(m + at, type);
    put16(m + at + 2, (unsigned)len);
    memcpy(m + at + 4, value, (size_t)len);
    memset(m + at + 4 + len, 0, (size_t)((4 - (len & 3)) & 3));
    return at + 4 + ((len + 3) & ~3);
}

/* XOR-PEER-ADDRESS (or another XOR address attribute) of an IPv4 address. */
static int msg_addr(uint8_t *m, int at, unsigned type, const struct sockaddr_in *a) {
    uint8_t v[8];
    const uint8_t *ip = (const uint8_t *)&a->sin_addr, *port = (const uint8_t *)&a->sin_port;
    v[0] = 0;
    v[1] = 1;
    v[2] = port[0] ^ COOKIE0;
    v[3] = port[1] ^ COOKIE1;
    v[4] = ip[0] ^ COOKIE0; v[5] = ip[1] ^ COOKIE1; v[6] = ip[2] ^ COOKIE2; v[7] = ip[3] ^ COOKIE3;
    return msg_attr(m, at, type, v, 8);
}

/* The login's three attributes and the integrity, and the message's length field. */
static int msg_sign(uint8_t *m, int at) {
    uint8_t mac[20];
    if (sR.haveLogin) {
        at = msg_attr(m, at, 0x0006, sR.username, (int)strlen(sR.username));
        at = msg_attr(m, at, 0x0014, sR.realm, (int)strlen(sR.realm));
        at = msg_attr(m, at, 0x0015, sR.nonce, (int)strlen(sR.nonce));
        put16(m + 2, (unsigned)(at - 20 + 24)); /* (the length as it will be with the integrity attribute) */
        hmac_sha1(sR.key, 16, m, (size_t)at, mac);
        at = msg_attr(m, at, 0x0008, mac, 20);
    }
    put16(m + 2, (unsigned)(at - 20));
    return at;
}

static void to_server(int sock, const uint8_t *m, int len) {
    sendto(sock, (const char *)m, len, 0, (const struct sockaddr *)&sR.server, sizeof(sR.server));
}

/* An attribute of a received message: its value and length, or NULL. */
static const uint8_t *attr_find(const uint8_t *m, int n, unsigned type, int *len) {
    int at;
    for (at = 20; at + 4 <= n;) {
        unsigned t = (unsigned)(m[at] << 8 | m[at + 1]);
        int l = m[at + 2] << 8 | m[at + 3];
        if (at + 4 + l > n) {
            break;
        }
        if (t == type) {
            *len = l;
            return m + at + 4;
        }
        at += 4 + ((l + 3) & ~3);
    }
    return NULL;
}

static int xor_addr(const uint8_t *v, int len, struct sockaddr_in *out) {
    uint8_t *ip, *port;
    if (v == NULL || len < 8 || v[1] != 1) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    ip = (uint8_t *)&out->sin_addr;
    port = (uint8_t *)&out->sin_port;
    port[0] = v[2] ^ COOKIE0;
    port[1] = v[3] ^ COOKIE1;
    ip[0] = v[4] ^ COOKIE0; ip[1] = v[5] ^ COOKIE1; ip[2] = v[6] ^ COOKIE2; ip[3] = v[7] ^ COOKIE3;
    return 1;
}

/* Takes the realm and the nonce out of an error answer and works the key out. */
static void take_login(const uint8_t *m, int n) {
    int rl = 0, nl = 0;
    const uint8_t *realm = attr_find(m, n, 0x0014, &rl), *nonce = attr_find(m, n, 0x0015, &nl);
    char text[512];
    if (nonce != NULL && nl < (int)sizeof(sR.nonce)) {
        memcpy(sR.nonce, nonce, (size_t)nl);
        sR.nonce[nl] = '\0';
    }
    if (realm != NULL && rl < (int)sizeof(sR.realm)) {
        memcpy(sR.realm, realm, (size_t)rl);
        sR.realm[rl] = '\0';
    }
    if (sR.realm[0] != '\0' && sR.nonce[0] != '\0') {
        SDL_snprintf(text, sizeof(text), "%s:%s:%s", sR.username, sR.realm, sR.password);
        md5((const uint8_t *)text, strlen(text), sR.key);
        sR.haveLogin = 1;
    }
}

static int same_addr(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

int Relay_Allocate(int sock, const char *server, const char *username, const char *password, char *out, int size) {
    struct addrinfo hints, *res = NULL;
    char host[128], *colon;
    uint8_t m[768], pkt[768];
    int round, silent = 0, fresh = 1, at = 0;

    memset(&sR, 0, sizeof(sR));
    SDL_strlcpy(host, server, sizeof(host));
    colon = strrchr(host, ':');
    if (colon == NULL) {
        return 0;
    }
    *colon = '\0';
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, colon + 1, &hints, &res) != 0 || res == NULL) {
        return 0;
    }
    memcpy(&sR.server, res->ai_addr, sizeof(sR.server));
    freeaddrinfo(res);
    SDL_strlcpy(sR.username, username, sizeof(sR.username));
    SDL_strlcpy(sR.password, password, sizeof(sR.password));
    for (round = 0; round < 10; round++) { /* without the login, with it, and again after a stale nonce or a lost packet */
        static const uint8_t udp[4] = {17, 0, 0, 0};
        int waited;
        if (silent == 2 && !sR.haveLogin) {
            /* no answer at all: seen now and then with Cloudflare's service, for a whole conversation (every
               request from one port) while another gets through. Its other port is another conversation. */
            sR.server.sin_port = htons(ntohs(sR.server.sin_port) == 53 ? 3478 : 53);
            silent = 0;
        }
        silent++;
        if (fresh) {
            /* A request that got no answer is sent again as it was, with the same transaction number: if only the
               answer was lost, the server gives it again. A new number would be a second request for an address
               the conversation already has, and refused (437). */
            at = msg_begin(m, 0x0003);
            at = msg_attr(m, at, 0x0019, udp, 4);
            at = msg_sign(m, at);
            fresh = 0;
        }
        to_server(sock, m, at);
        if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
            fprintf(stderr, "relay: round %d asked (%d bytes)\n", round, at);
        }
        for (waited = 0; waited < 700; waited += 10) {
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            int n = (int)recvfrom(sock, (char *)pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &fromLen), len = 0;
            unsigned type;
            if (n < 20 || !same_addr(&from, &sR.server) || memcmp(pkt + 8, m + 8, 12) != 0) {
                SDL_Delay(10);
                continue;
            }
            type = (unsigned)(pkt[0] << 8 | pkt[1]);
            silent = 0;
            if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
                int q;
                fprintf(stderr, "relay: round %d answer type %04x, %d bytes, attributes:", round, type, n);
                for (q = 20; q + 4 <= n; q += 4 + (((pkt[q + 2] << 8 | pkt[q + 3]) + 3) & ~3)) {
                    fprintf(stderr, " %04x(%d)", pkt[q] << 8 | pkt[q + 1], pkt[q + 2] << 8 | pkt[q + 3]);
                }
                fprintf(stderr, "; login known %d\n", sR.haveLogin);
            }
            if (type == 0x0103) { /* success */
                struct sockaddr_in relayed;
                char text[32];
                const uint8_t *v = attr_find(pkt, n, 0x0016, &len); /* (on its own line: len must be set before it is passed on) */
                if (!xor_addr(v, len, &relayed) || inet_ntop(AF_INET, &relayed.sin_addr, text, sizeof(text)) == NULL) {
                    return 0;
                }
                SDL_snprintf(out, (size_t)size, "%s:%d", text, ntohs(relayed.sin_port));
                sR.active = 1;
                sR.lastRefresh = sR.lastPermit = SDL_GetTicks();
                return 1;
            }
            if (type == 0x0113) { /* error: 401 the first time (the login is wanted), 438 a stale nonce */
                const uint8_t *e = attr_find(pkt, n, 0x0009, &len);
                int code = e != NULL && len >= 4 ? (e[2] & 7) * 100 + e[3] : 0;
                if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
                    fprintf(stderr, "relay: allocate answered %d %.*s\n", code, e != NULL && len > 4 ? len - 4 : 0, e != NULL ? (const char *)e + 4 : "");
                }
                if (code == 437 && sR.haveLogin) { /* an address is held already (an earlier run of this port): given up, asked anew */
                    static const uint8_t life[4] = {0, 0, 0, 0};
                    uint8_t r[768];
                    int rt = msg_begin(r, 0x0004);
                    rt = msg_attr(r, rt, 0x000D, life, 4);
                    rt = msg_sign(r, rt);
                    to_server(sock, r, rt);
                    SDL_Delay(150);
                } else if (code != 401 && code != 438) {
                    return 0;
                } else {
                    take_login(pkt, n);
                }
                fresh = 1;
                break;
            }
        }
    }
    return 0;
}

static void send_permit(int sock, const struct sockaddr_in *peer) {
    uint8_t m[768];
    int at = msg_begin(m, 0x0008);
    at = msg_addr(m, at, 0x0012, peer);
    at = msg_sign(m, at);
    to_server(sock, m, at);
}

/* Asks for a channel to `peer` (or asks again: a binding lasts ten minutes). With a channel the match's packets
   travel with a 4-byte header in both directions, as the plain data they are, instead of each inside a STUN message
   ("indication"): that is what a relay expects of a steady stream. */
static void send_bind(int sock, RelayPeer *p) {
    uint8_t m[768], num[4];
    int at = msg_begin(m, 0x0009);
    num[0] = (uint8_t)(p->chan >> 8);
    num[1] = (uint8_t)p->chan;
    num[2] = num[3] = 0;
    memcpy(p->txn, m + 8, 12);
    at = msg_attr(m, at, 0x000C, num, 4);
    at = msg_addr(m, at, 0x0012, &p->addr);
    at = msg_sign(m, at);
    to_server(sock, m, at);
    p->lastBind = SDL_GetTicks();
}

static RelayPeer *peer_find(const struct sockaddr_in *a) {
    int i;
    for (i = 0; i < RELAY_PEERS; i++) {
        if (sR.peer[i].chan != 0 && same_addr(&sR.peer[i].addr, a)) {
            return &sR.peer[i];
        }
    }
    return NULL;
}

void Relay_Permit(int sock, const struct sockaddr_in *peer) {
    int i;
    if (!sR.active) {
        return;
    }
    for (i = 0; i < sR.permits; i++) {
        if (sR.permit[i].sin_addr.s_addr == peer->sin_addr.s_addr) {
            return;
        }
    }
    if (sR.permits < RELAY_PERMITS) {
        sR.permit[sR.permits++] = *peer;
    }
    send_permit(sock, peer);
}

int Relay_Active(void) {
    return sR.active;
}

void Relay_Tick(int sock) {
    uint64_t now;
    int i;

    if (!sR.active) {
        return;
    }
    now = SDL_GetTicks();
    for (i = 0; i < RELAY_PEERS; i++) {
        RelayPeer *p = &sR.peer[i];
        if (p->chan != 0 && now - p->lastBind > (p->chanBound ? 240000u : 500u) && SDL_getenv("BT3_RELAY_NOCHAN") == NULL) {
            send_bind(sock, p); /* not agreed yet (an answer lost): again; agreed: renewed well before it runs out */
        }
    }
    if (now - sR.lastPermit > 60000) { /* a permission lasts five minutes: asked again every minute (an answer may be lost) */
        sR.lastPermit = now;
        for (i = 0; i < sR.permits; i++) {
            send_permit(sock, &sR.permit[i]);
        }
    }
    if (now - sR.lastRefresh > 120000) { /* the allocation lasts ten minutes */
        static const uint8_t life[4] = {0, 0, 0x02, 0x58}; /* 600 seconds */
        uint8_t m[768];
        int at = msg_begin(m, 0x0004);
        sR.lastRefresh = now;
        at = msg_attr(m, at, 0x000D, life, 4);
        at = msg_sign(m, at);
        to_server(sock, m, at);
    }
}

int Relay_Unwrap(uint8_t *pkt, int n, struct sockaddr_in *from, uint8_t **data, int *len) {
    unsigned type;
    int l = 0;

    if (!sR.active || !same_addr(from, &sR.server)) {
        return 2;
    }
    if (n >= 4 && (pkt[0] & 0xC0) == 0x40) { /* channel data: the channel's number says whose */
        int i, num = pkt[0] << 8 | pkt[1];
        for (i = 0; i < RELAY_PEERS && !(sR.peer[i].chan == num && num != 0); i++) {
        }
        l = pkt[2] << 8 | pkt[3];
        if (i == RELAY_PEERS || l > n - 4) {
            return 0;
        }
        sR.peer[i].via = 1;
        sR.peer[i].lastHeard = SDL_GetTicks();
        *from = sR.peer[i].addr;
        *data = pkt + 4;
        *len = l;
        return 1;
    }
    if (n < 20) {
        return 0;
    }
    type = (unsigned)(pkt[0] << 8 | pkt[1]);
    if (type == 0x0109) { /* a channel is agreed: which one, its request's transaction says */
        int i;
        for (i = 0; i < RELAY_PEERS; i++) {
            if (sR.peer[i].chan != 0 && memcmp(sR.peer[i].txn, pkt + 8, 12) == 0) {
                sR.peer[i].chanBound = 1;
                if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
                    fprintf(stderr, "relay: channel %04x bound\n", sR.peer[i].chan);
                }
            }
        }
        return 0;
    }
    if (type == 0x0017) { /* a data indication: the other player's packet */
        struct sockaddr_in peer;
        const uint8_t *d = attr_find(pkt, n, 0x0012, &l);
        if (!xor_addr(d, l, &peer)) {
            return 0;
        }
        d = attr_find(pkt, n, 0x0013, &l);
        if (d == NULL) {
            return 0;
        }
        {
            RelayPeer *p = peer_find(&peer);
            if (p == NULL) { /* a new address: the first free place, or the one heard from longest ago */
                int i, pick = 0;
                for (i = 0; i < RELAY_PEERS; i++) {
                    if (sR.peer[i].chan == 0) {
                        pick = i;
                        break;
                    }
                    if (sR.peer[i].lastHeard < sR.peer[pick].lastHeard) {
                        pick = i;
                    }
                }
                p = &sR.peer[pick];
                memset(p, 0, sizeof(*p));
                p->addr = peer;
                p->chan = 0x4000 + (sR.nextChan++ & 0xFFF); /* (a number stays with its address) */
                if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
                    fprintf(stderr, "relay: data from port %d (channel %04x)\n", ntohs(peer.sin_port), p->chan);
                }
            }
            p->via = 1;
            p->lastHeard = SDL_GetTicks();
            /* the address is known now: a channel to it is asked for by the next tick */
        }
        *from = peer;
        *data = (uint8_t *)d;
        *len = l;
        return 1;
    }
    if ((type & 0x0110) == 0x0110) { /* an error answer: a stale nonce brings a new one; the request is repeated by the tick */
        const uint8_t *e = attr_find(pkt, n, 0x0009, &l);
        int code = e != NULL && l >= 4 ? (e[2] & 7) * 100 + e[3] : 0;
        if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
            fprintf(stderr, "relay: answer %04x: error %d %.*s\n", type, code, e != NULL && l > 4 ? l - 4 : 0, e != NULL ? (const char *)e + 4 : "");
        }
        if (code == 438 || code == 401) {
            take_login(pkt, n);
            sR.lastPermit = sR.lastRefresh = 0; /* (again at the next tick) */
        }
    }
    return 0;
}

int Relay_IsPeer(const struct sockaddr_in *to) {
    const RelayPeer *p;
    return sR.active && (p = peer_find(to)) != NULL && p->via;
}

void Relay_Send(int sock, const void *data, int len, const struct sockaddr_in *to) {
    uint8_t m[1600];
    int at;

    if (SDL_getenv("BT3_RELAY_LOG") != NULL) {
        static int count;
        if (count++ < 8) {
            fprintf(stderr, "relay: send %d bytes to port %d\n", len, ntohs(to->sin_port));
        }
    }

    if (len > 1400) {
        return;
    }
    {
        const RelayPeer *p = peer_find(to);
        if (p != NULL && p->chanBound && SDL_getenv("BT3_RELAY_NOCHAN") == NULL) {
            m[0] = (uint8_t)(p->chan >> 8);
            m[1] = (uint8_t)p->chan;
        put16(m + 2, (unsigned)len);
            memcpy(m + 4, data, (size_t)len);
            to_server(sock, m, 4 + len);
            return;
        }
    }
    at = msg_begin(m, 0x0016);
    at = msg_addr(m, at, 0x0012, to);
    at = msg_attr(m, at, 0x0013, data, len);
    put16(m + 2, (unsigned)(at - 20));
    to_server(sock, m, at);
}

void Relay_SawDirect(const struct sockaddr_in *from) {
    RelayPeer *p;
    if (sR.active && (p = peer_find(from)) != NULL) {
        p->via = 0;
    }
}

void Relay_Close(int sock) {
    if (sR.active && sock >= 0) {
        static const uint8_t life[4] = {0, 0, 0, 0}; /* a refresh with a lifetime of 0 gives the address up */
        uint8_t m[768];
        int at = msg_begin(m, 0x0004);
        at = msg_attr(m, at, 0x000D, life, 4);
        at = msg_sign(m, at);
        to_server(sock, m, at);
    }
    sR.active = 0;
    sR.permits = 0;
    memset(sR.peer, 0, sizeof(sR.peer));
    sR.nextChan = 0;
}
